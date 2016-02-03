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

#ifndef LED_H
#define LED_H

#include <stdint.h>

enum led_index
{
	DS3_LED_ONE=0,
	DS3_LED_TWO=1,
	DS3_LED_THREE=2,
	DS3_LED_FOUR=3,
	
	DS4_LED_RED=0,
	DS4_LED_GREEN=1,
	DS4_LED_BLUE=2,
	DS4_LED_GLOBAL=3
};

int  led_ds3_init(int (*led_devices)[4]);
void led_ds3_get(int (*led_devices)[4], uint8_t (*led_values)[4]);
void led_ds3_set(int (*led_devices)[4], uint8_t (*led_values)[4],
                 uint8_t led_one, uint8_t led_two, uint8_t led_three, uint8_t led_four);

int  led_ds4_init(int (*led_devices)[4]);
void led_ds4_get(int (*led_devices)[4], uint8_t (*led_values)[4]);
void led_ds4_set(int (*led_devices)[4], uint8_t (*led_values)[4],
                 uint8_t red, uint8_t green, uint8_t blue, uint8_t global);

#endif
