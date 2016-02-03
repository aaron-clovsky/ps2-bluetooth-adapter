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

#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <stdint.h>
#include <pthread.h>
#include <linux/joystick.h>

typedef struct
{
	uint32_t buttons;
	
	union {
		struct {
			int16_t lx;
			int16_t ly;
			int16_t rx;
			int16_t ry;
			int16_t up;
			int16_t right;
			int16_t down;
			int16_t left;
			int16_t l2;
			int16_t r2;
			int16_t l1;
			int16_t r1;
			int16_t triangle;
			int16_t circle;
			int16_t cross;
			int16_t square;
		} named;
		
		int16_t buffer[16];
	} axes;
} joystick_inputs;

typedef struct
{
	unsigned int    type; /* Dualshock type (3 or 4)*/
	int             device;
	joystick_inputs inputs;
	pthread_mutex_t inputs_mutex;
	pthread_t       thread;
	unsigned int    thread_terminated;
	
	unsigned int led_support;
	int          led_devices[4];
	uint8_t      led_values[4];
	
	unsigned int     rumble_support;
	int              rumble_device;
	struct ff_effect rumble_motors;
} joystick;

int joystick_init(const char *joystick_path, const char *event_path, joystick *js);

#endif
