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
 
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include "joystick.h"
#include "led.h"
#include "rumble.h"

/*  - Non-blocking I/O is used here with select() to provide both blocking reads and non-blocking read-ahead
	- After each event is processed the next event is read ahead of time
	- If the timestamp of the next event matches the current event's timestamp then the next event is also
	  processed before updating the controller data
	- Joystick data is mutexed to ensure that simultaneous reads/writes do not occur */
static void *joystick_polling_thread(void *data)
{
	fd_set           set;
	struct timeval   timeout;
	struct js_event  event;
	unsigned int     read_ahead;
	joystick        *js;
	joystick_inputs  inputs;
	
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	
	read_ahead = 0;
	
	js = (joystick *)data;
	inputs = js->inputs;
	
	while (1)
	{
		FD_ZERO(&set);
		FD_SET(js->device, &set);
		
		if (read_ahead || select(js->device + 1, &set, NULL, NULL, &timeout))
		{
			if (!read_ahead && read(js->device, &event, sizeof(struct js_event)) != sizeof(struct js_event))
			{
				js->thread_terminated = 1;
				
				return NULL;
			}
			
			read_ahead = 0;
			
			while (1) /* while(1) used for continue */
			{
				switch (event.type)
				{
					case JS_EVENT_BUTTON:
					{
						if (js->type == 3)
						{
							if (event.number < 17)
							{
								inputs.buttons &= ~(1 << event.number);
								inputs.buttons |= (event.value << event.number);
							}
						}
						else
						{
							if (event.number < 14)
							{
								const unsigned int ds4_button_map[14] = { 15, 14, 13, 12, 10, 11, 8,
								                                           9,  0,  3,  1,  2, 16, 17 };
								
								inputs.buttons &= ~(1 << ds4_button_map[event.number]);
								inputs.buttons |= (event.value << ds4_button_map[event.number]);
								
								if (event.number < 12 && event.number != 6 && event.number != 7)
								{
									inputs.axes.buffer[ds4_button_map[event.number]] = event.value;
								}
							}
						}
						
						break;
					}
					case JS_EVENT_AXIS:
					{
						if (js->type == 3)
						{
							if (event.number < 20 && (event.number <= 3 || event.number >= 8))
							{
								inputs.axes.buffer[(event.number < 8) ? event.number : event.number - 4] = event.value;
							}
						}
						else
						{
							if (event.number < 8)
							{
								const unsigned int ds4_axis_map[6] = { 0, 1, 2, 8, 9, 3 };
								
								if (event.number == 6) /* DPAD Left/Right */
								{
									inputs.buttons &= ~(1 << 7); /* Left */
									inputs.buttons |= (event.value < 0) << 7;
									inputs.axes.buffer[7] = (event.value < 0);
									inputs.buttons &= ~(1 << 5); /* Right */
									inputs.buttons |= (event.value > 0) << 5;
									inputs.axes.buffer[5] = (event.value > 0);
								}
								else if (event.number == 7) /* DPAD Up/Down */
								{
									inputs.buttons &= ~(1 << 4); /* Up */
									inputs.buttons |= (event.value < 0) << 4;
									inputs.axes.buffer[4] = (event.value < 0);
									inputs.buttons &= ~(1 << 6); /* Down */
									inputs.buttons |= (event.value > 0) << 6;
									inputs.axes.buffer[6] = (event.value > 0);
								}
								else if (event.number == 3 || event.number == 4) /* L2/R2 */
								{
									inputs.axes.buffer[ds4_axis_map[event.number]] = event.value;
								}
								else /* Sticks */
								{
									inputs.axes.buffer[ds4_axis_map[event.number]] = event.value;
								}
							}
						}
						
						break;
					}
				}
				
				{
					struct js_event tmp;
					
					if (read(js->device, &tmp, sizeof(struct js_event)) == sizeof(struct js_event))
					{
						if (tmp.time == event.time)
						{
							event = tmp;
							continue;
						}
						else
						{
							event = tmp;
							read_ahead = 1;
						}
					}
				}
				
				break;
			}
			
			pthread_mutex_lock(&js->inputs_mutex);
			js->inputs = inputs;
			pthread_mutex_unlock(&js->inputs_mutex);
		}
	}
	
	return 0;
}

int joystick_init(const char *joystick_path, const char *event_path, joystick *js)
{
	memset(js, 0, sizeof(joystick));
	
	if ((js->device = open(joystick_path, O_RDONLY | O_NONBLOCK)) == -1)
	{
		return -1;
	}
	
	pthread_mutex_init(&js->inputs_mutex, NULL);
	
	{
		uint8_t axes, buttons;
		
		ioctl(js->device, JSIOCGAXES, &axes); 
		ioctl(js->device, JSIOCGBUTTONS, &buttons);  
		
		/*	DS3 - 19 buttons and 27 AXES
			DS4 - 14 buttons and 18 AXES */
		js->type = (buttons == 14 && axes == 18) ? 4 : 3;
	}
	
	/* Initialize button pressures -- Joystick thread reports 
	   DS3 button pressure as -32767 to +32767 while
	   DS4 pressures are reported as 0 or 1 (except L2/R2) */
	if (js->type == 3)
	{
		unsigned int i;
		
		for (i = 4; i < 16; i++)
		{
			js->inputs.axes.buffer[i] = -32767;
		}
	}
	else
	{
		js->inputs.axes.buffer[8] = -32767;
		js->inputs.axes.buffer[9] = -32767;
	}
	
	if (pthread_create(&js->thread, NULL, &joystick_polling_thread, (void *)js) != 0)
	{
		close(js->device);
		
		return -1;
	}
	
	if (js->type == 3)
	{
		if ((js->led_support = led_ds3_init(&js->led_devices)))
		{
			led_ds3_get(&js->led_devices, &js->led_values);
		}
	}
	else
	{
		if ((js->led_support = led_ds4_init(&js->led_devices)))
		{
			led_ds4_get(&js->led_devices, &js->led_values);
		}
	}
	
	js->rumble_support = rumble_init(event_path, &js->rumble_motors, &js->rumble_device);
	
	return 0;
}
