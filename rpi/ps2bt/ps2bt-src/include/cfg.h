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

#ifndef CFG_H
#define CFG_H

#include <stdint.h>
#include "led.h"
#include "controller.h"

typedef struct {
	uint8_t        default_pressure;
	uint8_t        analog_to_button_deadzone;
	unsigned int   ds3_leds[2];
	uint8_t        ds4_leds[3];
	uint8_t        ds4_triangle_pressure;
	uint8_t        ds4_circle_pressure;
	uint8_t        ds4_cross_pressure;
	uint8_t        ds4_square_pressure;
	unsigned int   ds4_range_emulation_default;
	double         ds4_range_emulation_divisor;
	controller_map map;
} cfg_settings;

void cfg_file_read(const char *ro_path, const char *rw_path, cfg_settings *settings);
void cfg_file_write(const char *rw_path, cfg_settings *settings);

#endif
