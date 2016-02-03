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

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <stdint.h>

typedef struct
{
	uint16_t buttons;
	
	union {
		struct {
			uint8_t lx;
			uint8_t ly;
			uint8_t rx;
			uint8_t ry;
			uint8_t up;
			uint8_t right;
			uint8_t down;
			uint8_t left;
			uint8_t l2;
			uint8_t r2;
			uint8_t l1;
			uint8_t r1;
			uint8_t triangle;
			uint8_t circle;
			uint8_t cross;
			uint8_t square;
		} named;
		
		uint8_t buffer[16];
	} axes;
} controller_inputs;

typedef struct {
	union
	{
		struct {
			int start, select, l3, r3;
			int triangle, circle, cross, square;
			int up, right, down, left;
			int l2, r2;
			int l1, r1;
			int ls_up, ls_left, rs_up, rs_left;
			int ls_down, ls_right, rs_down, rs_right;
		} by_name;
		
		int by_offset[24];
	} inputs;
} controller_map;

extern const char *controller_map_offset_to_string[24];
extern uint16_t    controller_map_offset_to_buffer[24];
extern uint16_t    controller_map_offset_to_bitmask[16];

void controller_remap(controller_inputs *in, controller_inputs *out, controller_map *map,
                      uint8_t default_pressure, uint8_t deadzone);

#endif
