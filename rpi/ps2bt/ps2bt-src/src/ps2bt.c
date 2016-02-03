/*
 * Dualshock 3/4 Joystick Interface for Raspberry Pi
 *
 * Copyright (C) 2015 Aaron Clovsky <pelvicthrustman@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define _POSIX_C_SOURCE 199309L /* nanosleep() */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include "cfg.h"
#include "controller.h"
#include "joystick.h"
#include "led.h"
#include "rumble.h"
#include "serial.h"

/* This macro function is usually defined by _BSD_SOURCE */
#define timersub(a, b, result)                     \
do {                                               \
  (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;    \
  (result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
  if ((result)->tv_usec < 0) {                     \
    --(result)->tv_sec;                            \
    (result)->tv_usec += 1000000;                  \
  }                                                \
} while (0)

#define MAX_RATE 1000

#define DS3_HOLD_INTERVAL  2.0
#define DS3_BLINK_INTERVAL 0.5
#define DS4_BLINK_INTERVAL 0.3

#define RO_SETTINGS_FILE "/tmp/settings.cfg"
#define RW_SETTINGS_FILE "/var/lib/bluetooth/ds4.cfg"

void nsleep(unsigned long nsec);

void apply_controller_map(unsigned int mode, controller_inputs *in, controller_inputs *out,
                          controller_map *custom_map, uint8_t default_pressure, uint8_t deadzone);

void ds3_handle_interaction_and_settings(joystick_inputs *in, controller_inputs *out,
                                         cfg_settings *settings, uint8_t *led, unsigned int *mode_switch);

void         ds4_range_adjust(int16_t *x_inout, int16_t *y_inout, double divisor);
unsigned int ds4_handle_interaction_and_settings(joystick_inputs *in, controller_inputs *out,
                                                 cfg_settings *settings, uint8_t (*leds)[4],
                                                 unsigned int *led_explicit_mode, unsigned int *mode_switch);

int main(int argc, char **argv)
{
	int          serial_device;
	joystick     js;
	cfg_settings settings;
	uint8_t      leds[4];
	
	#ifdef BENCHMARK
		const unsigned int benchmark_sample_size = 30;
		unsigned int       benchmark_frame_counter = 0;
		double             benchmark_sample_time = 0;
		struct timeval     benchmark_frame_start;
		struct timeval     benchmark_frame_end;
	#endif
	
	if (argc != 4)
	{
		printf("\nUsage: %s <joystick device> <event device> <serial device>\n\n", argv[0]);
		exit(0);
	}
	
	if ((serial_device = serial_init(argv[3])) == -1)
	{
		fprintf(stderr, "Error opening %s: %s\n", argv[3], strerror(errno));
		exit(1);
	}
	
	if (joystick_init(argv[1], argv[2], &js) == -1)
	{
		fprintf(stderr, "Failed to initialize controller\n");
		exit(1);
	}
	
	cfg_file_read(RO_SETTINGS_FILE, RW_SETTINGS_FILE, &settings);
	
	if (js.type == 3)
	{
		leds[0] = settings.ds3_leds[0];
		leds[1] = settings.ds3_leds[1];
		leds[2] = 0;
		leds[3] = 0;
	}
	else
	{
		leds[0] = settings.ds4_leds[DS4_LED_RED];
		leds[1] = settings.ds4_leds[DS4_LED_GREEN];
		leds[2] = settings.ds4_leds[DS4_LED_BLUE];
		leds[3] = 1;
	}
	
	#ifdef BENCHMARK
		printf("\n\nBegin polling loop\n");
		fflush(stdout);
	#endif
	
	while (1)
	{
		uint8_t           tx_packet[SEND_PACKET_SIZE];
		uint8_t           rx_packet[RECV_PACKET_SIZE];
		joystick_inputs   input;
		controller_inputs output;
		unsigned int      led_explicit_mode;
		unsigned int      mode_switch;
		
		#ifdef BENCHMARK
			gettimeofday(&benchmark_frame_start, NULL);
		#endif
		
		pthread_mutex_lock(&js.inputs_mutex);
		input = js.inputs;
		pthread_mutex_unlock(&js.inputs_mutex);
		
		if (js.thread_terminated)
		{
			pthread_join(js.thread, NULL);
			serial_send_disconnect_packet(serial_device);
			exit(0);
		}
		
		if (js.type == 3)
		{
			ds3_handle_interaction_and_settings(&input, &output, &settings, &leds[2], &mode_switch);
		}
		else
		{
			if (ds4_handle_interaction_and_settings(&input, &output,
			                                        &settings, &leds, &led_explicit_mode, &mode_switch))
			{
				cfg_file_write(RW_SETTINGS_FILE, &settings);
			}
		}
		
		serial_construct_packet(&output, &tx_packet, mode_switch);
		
		if (serial_send_packet(serial_device, &tx_packet) == -1)
		{
			exit(1);
		}
		
		if (!serial_recv_packet(serial_device, &rx_packet) == -1)
		{
			exit(1);
		}
		
		if (rx_packet[0] == 0x5A)
		{
			if (js.led_support)
			{
				if (js.type == 3)
				{
					led_ds3_set(&js.led_devices, &js.led_values, leds[0], leds[1], leds[2], (rx_packet[3] == 0xAA));
				}
				else
				{
					int divisor;
					
					divisor = led_explicit_mode ? 1 : ((rx_packet[3] == 0xAA) ? 1 : 4);
					
					led_ds4_set(&js.led_devices, &js.led_values,
					            leds[0] / divisor, leds[1] / divisor, leds[2] / divisor, leds[3]);
				}
			}
			
			if (js.rumble_support)
			{
				rumble_set(js.rumble_device, &js.rumble_motors, rx_packet[2], rx_packet[1]);
			}
		}
		
		nsleep((unsigned long)(((double)1.0 / (double)MAX_RATE) * (double)1000000000.0));
		
		#ifdef BENCHMARK
		{
			double benchmark_elapsed;
			
			gettimeofday(&benchmark_frame_end, NULL);
			
			benchmark_elapsed  = (double)(benchmark_frame_end.tv_sec - benchmark_frame_start.tv_sec);
			benchmark_elapsed += (double)(benchmark_frame_end.tv_usec - benchmark_frame_start.tv_usec) / 1000000.0;
			
			benchmark_sample_time += benchmark_elapsed;
			
			if ((++benchmark_frame_counter % benchmark_sample_size) == 0)
			{
				printf("Average %05.1fFPS | Sample %05.2fms\r",
				       1.0 / (benchmark_sample_time / benchmark_sample_size), benchmark_elapsed * 1000.0);
				fflush(stdout);
				benchmark_sample_time = 0;
				benchmark_frame_counter = 0;
			}
		}
		#endif
	}
}

void nsleep(unsigned long nsec)
{
	struct timespec t;
	t.tv_sec = nsec / 1000000000L;
	t.tv_nsec = nsec % 1000000000L;
	while (nanosleep(&t, &t) != 0) continue;
}

void apply_controller_map(unsigned int mode, controller_inputs *in, controller_inputs *out,
                          controller_map *custom_map, uint8_t default_pressure, uint8_t deadzone)
{
	if (mode == 1)
	{
		unsigned int   i;
		controller_map emulate_dpad;
		
		for (i = 0; i < 24; i++)
		{
			emulate_dpad.inputs.by_offset[i] = i;
		}
		
		emulate_dpad.inputs.by_name.ls_up = offsetof(controller_map, inputs.by_name.up) / sizeof(int);
		emulate_dpad.inputs.by_name.ls_down = offsetof(controller_map, inputs.by_name.down) / sizeof(int);
		emulate_dpad.inputs.by_name.ls_left = offsetof(controller_map, inputs.by_name.left) / sizeof(int);
		emulate_dpad.inputs.by_name.ls_right = offsetof(controller_map, inputs.by_name.right) / sizeof(int);
		
		controller_remap(in, out, &emulate_dpad, default_pressure, deadzone);
	}
	else if (mode == 2)
	{
		controller_remap(in, out, custom_map, default_pressure, deadzone);
	}
	else
	{
		*out = *in;
	}
}

void ds3_handle_interaction_and_settings(joystick_inputs *in, controller_inputs *out,
                                         cfg_settings *settings, uint8_t *led, unsigned int *mode_switch)
{
	static unsigned int   controller_map_mode = 0; /* 0 == Normal, 1 == Analog->DPAD, 2 == Custom */
	static int            hold_state = 0;
	static struct timeval hold_timer = { 0, 0 };
	static struct timeval blink_timer = { 0, 0 };
	
	*mode_switch = 0;
	
	if (in->buttons & 0x10000)
	{
		if (hold_state == 0)
		{
			*mode_switch = 1;
			gettimeofday(&hold_timer, NULL);
			hold_state = 1;
		}
		else if (hold_state < 3)
		{
			struct timeval now;
			struct timeval diff;
			double         elapsed;
			
			gettimeofday(&now, NULL);
			timersub(&now, &hold_timer, &diff);
			elapsed = diff.tv_sec + diff.tv_usec / 1000000.0;
			
			if (hold_state == 1 && elapsed >= (double)(DS3_HOLD_INTERVAL))
			{
				controller_map_mode = !controller_map_mode;
				hold_state = (controller_map_mode != 0) ? 2 : 3;
			}
			else if (hold_state == 2 && elapsed >= (double)(DS3_HOLD_INTERVAL * 2))
			{
				controller_map_mode = 2;
				gettimeofday(&blink_timer, NULL);
				hold_state = 3;
			}
		}
	}
	else
	{
		hold_state = 0;
	}
	
	if (blink_timer.tv_sec != 0)
	{
		struct timeval now;
		struct timeval diff;
		double         elapsed;
		
		gettimeofday(&now, NULL);
		timersub(&now, &blink_timer, &diff);
		elapsed = diff.tv_sec + diff.tv_usec / 1000000.0;
		
		if (elapsed >= (double)(DS3_BLINK_INTERVAL))
		{
			blink_timer.tv_sec = 0;
		}
		else
		{
			*led = 0;
		}
	}
	
	if (blink_timer.tv_sec == 0)
	{
		*led = !!controller_map_mode;
	}
	
	{
		unsigned int      i;
		controller_inputs tmp;
		
		tmp.buttons = (uint16_t)in->buttons;
		
		for (i = 0; i < 16; i++)
		{
			tmp.axes.buffer[i] = (uint8_t)((((int32_t)in->axes.buffer[i]) + 32768) / 256);
			
			if (i > 3 && !!tmp.axes.buffer[i] != ((tmp.buttons >> i) & 1))
			{
				tmp.axes.buffer[i] = ((tmp.buttons >> i) & 1);
			}
		}
		
		apply_controller_map(controller_map_mode, &tmp, out, &settings->map,
		                     settings->default_pressure, settings->analog_to_button_deadzone);
	}
}

void ds4_range_adjust(int16_t *x_inout, int16_t *y_inout, double divisor)
{
	double x, y, r, theta;
	
	if (*x_inout == 0 && *y_inout == 0) /* Center remains unchanged */
	{
		return;
	}
	
	x = (double)*x_inout;
	y = (double)*y_inout;
	
	r = sqrt(x*x + y*y); /* Convert cartesian to polar */
	theta = atan2(y, x);
	
	r /= divisor; /* Apply linear function */
	
	x = r * cos(theta); /* Convert polar to cartesian */
	y = r * sin(theta);
	
	x = (x > 32767) ? 32767 : x; /* Clamp x and y */
	x = (x < -32767) ? -32767 : x;
	y = (y > 32767) ? 32767 : y;
	y = (y < -32767) ? -32767 : y;
	
	*x_inout = (int16_t)(x);
	*y_inout = (int16_t)(y);
}

unsigned int ds4_handle_interaction_and_settings(joystick_inputs *in, controller_inputs *out,
                                                 cfg_settings *settings, uint8_t (*leds)[4],
                                                 unsigned int *led_explicit_mode, unsigned int *mode_switch)
{
	static unsigned int    initialized = 0;
	static unsigned int    controller_map_mode = 0; /* 0 == Normal, 1 == Analog->DPAD, 2 == Custom */
	static unsigned int    blink_state = 0;
	static struct timeval  blink_timer = { 0, 0 };
	static joystick_inputs previous;
	static unsigned int    color_set_mode = 0;
	static uint8_t         saved_leds[3];
	static uint8_t         emulated_pressure;
	static unsigned int    analog_emulated_range;
	unsigned int           save_settings;
	
	if (!initialized)
	{
		emulated_pressure = settings->default_pressure;
		analog_emulated_range = settings->ds4_range_emulation_default;
		previous = *in;
		initialized = 1;
	}
	
	save_settings = 0;
	
	if (in->buttons & 0x20000)
	{
		if ((in->axes.named.l1 != 0 && previous.axes.named.l1 == 0)
		||  (in->axes.named.r1 != 0 && previous.axes.named.r1 == 0))
		{
			if (!color_set_mode)
			{
				saved_leds[0] = (*leds)[0];
				saved_leds[1] = (*leds)[1];
				saved_leds[2] = (*leds)[2];
				color_set_mode = 1;
				blink_state = 1;
				gettimeofday(&blink_timer, NULL);
			}
			else
			{
				settings->ds4_leds[DS4_LED_RED] = (*leds)[0];
				settings->ds4_leds[DS4_LED_GREEN] = (*leds)[1];
				settings->ds4_leds[DS4_LED_BLUE] = (*leds)[2];
				color_set_mode = 0;
				save_settings = 1;
				blink_state = 3;
				gettimeofday(&blink_timer, NULL);
			}
		}
		else if ((in->buttons & 0x1) && !(previous.buttons & 0x1)) /* Select */
		{
			analog_emulated_range = 0;
			blink_state = 1;
			gettimeofday(&blink_timer, NULL);
		}
		else if ((in->buttons & 0x8) && !(previous.buttons & 0x8)) /* Start */
		{
			analog_emulated_range = 1;
			blink_state = 1;
			gettimeofday(&blink_timer, NULL);
		}
		else if ((in->axes.named.up != 0 && previous.axes.named.up == 0)
			  || (in->axes.named.down != 0 && previous.axes.named.down == 0))
		{
			controller_map_mode = 0;
			blink_state = 1;
			gettimeofday(&blink_timer, NULL);
		}
		else if (in->axes.named.left != 0 && previous.axes.named.left == 0)
		{
			controller_map_mode = 1;
			blink_state = 1;
			gettimeofday(&blink_timer, NULL);
		}
		else if (in->axes.named.right != 0 && previous.axes.named.right == 0)
		{
			controller_map_mode = 2;
			blink_state = 1;
			gettimeofday(&blink_timer, NULL);
		}
		else if (in->axes.named.triangle != 0 && previous.axes.named.triangle == 0)
		{
			emulated_pressure = settings->ds4_triangle_pressure;
			blink_state = 1;
			gettimeofday(&blink_timer, NULL);
		}
		else if (in->axes.named.circle != 0 && previous.axes.named.circle == 0)
		{
			emulated_pressure = settings->ds4_circle_pressure;
			blink_state = 1;
			gettimeofday(&blink_timer, NULL);
		}
		else if (in->axes.named.cross != 0 && previous.axes.named.cross == 0)
		{
			emulated_pressure = settings->ds4_cross_pressure;
			blink_state = 1;
			gettimeofday(&blink_timer, NULL);
		}
		else if (in->axes.named.square != 0 && previous.axes.named.square == 0)
		{
			emulated_pressure = settings->ds4_square_pressure;
			blink_state = 1;
			gettimeofday(&blink_timer, NULL);
		}
	}
	else
	{
		unsigned int      i;
		int16_t           sticks[4];
		controller_inputs tmp;
		
		if (color_set_mode)
		{
			(*leds)[0] = saved_leds[0];
			(*leds)[1] = saved_leds[1];
			(*leds)[2] = saved_leds[2];
		}
		
		color_set_mode = 0;
		
		sticks[0] = in->axes.buffer[0];
		sticks[1] = in->axes.buffer[1];
		sticks[2] = in->axes.buffer[2];
		sticks[3] = in->axes.buffer[3];
		
		if (analog_emulated_range)
		{
			ds4_range_adjust(&sticks[0], &sticks[1], settings->ds4_range_emulation_divisor);
			ds4_range_adjust(&sticks[2], &sticks[3], settings->ds4_range_emulation_divisor);
		}
		
		tmp.buttons = (uint16_t)in->buttons;
		
		for (i = 0; i < 16; i++)
		{
			if (i < 4)
			{
				tmp.axes.buffer[i] = (uint8_t)((((int32_t)sticks[i]) + 32768) / 256);
			}
			else if (i == 8 || i == 9)
			{
				tmp.axes.buffer[i] = (uint8_t)((((int32_t)in->axes.buffer[i]) + 32768) / 256);
				
				if (!!tmp.axes.buffer[i] != ((tmp.buttons >> i) & 1))
				{
					tmp.axes.buffer[i] = ((tmp.buttons >> i) & 1);
				}
			}
			else
			{
				tmp.axes.buffer[i] = (uint8_t)in->axes.buffer[i] * emulated_pressure;
			}
		}
		
		apply_controller_map(controller_map_mode, &tmp, out, &settings->map,
		                     emulated_pressure, settings->analog_to_button_deadzone);
	}
	
	if (blink_state > 0)
	{
		struct timeval now;
		struct timeval diff;
		double         elapsed;
		unsigned int   periods;
		
		gettimeofday(&now, NULL);
		timersub(&now, &blink_timer, &diff);
		elapsed = diff.tv_sec + diff.tv_usec / 1000000.0;
		periods = (unsigned int)(elapsed / (double)(DS4_BLINK_INTERVAL));
		
		(*leds)[3] = (uint8_t)(periods & 1);
		
		if (periods >= blink_state)
		{
			(*leds)[0] = settings->ds4_leds[DS4_LED_RED];
			(*leds)[1] = settings->ds4_leds[DS4_LED_GREEN];
			(*leds)[2] = settings->ds4_leds[DS4_LED_BLUE];
			(*leds)[3] = 1;
			
			blink_state = 0;
		}
	}
	
	if (blink_state == 0 && color_set_mode)
	{
		(*leds)[0] = (uint8_t)((((int32_t)in->axes.named.l2) + 32768) / 1024);
		(*leds)[1] = (uint8_t)((((int32_t)in->axes.named.r2) + 32768) / 1024);
		(*leds)[2] = (uint8_t)((((int32_t)-in->axes.named.ly) + 32768) / 1024);
	}
	
	*led_explicit_mode = blink_state != 0 || color_set_mode;
	*mode_switch = ((in->buttons & 0x10000) && !(previous.buttons & 0x10000));
	
	previous = *in;
	
	return save_settings;
}
