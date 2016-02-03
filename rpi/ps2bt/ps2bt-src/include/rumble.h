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
 
#ifndef RUMBLE_H
#define RUMBLE_H

#include <stdint.h> 
#include <linux/input.h>
  
int rumble_init(const char *device_path, struct ff_effect *effect, int *fd);
int rumble_set(int fd, struct ff_effect *effect, uint8_t strong, uint8_t weak);

#endif
