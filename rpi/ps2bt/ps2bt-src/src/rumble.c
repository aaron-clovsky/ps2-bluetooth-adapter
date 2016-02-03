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
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include "rumble.h"

#define BITS_PER_LONG        (sizeof(long) * 8)
#define LONG(x)              ((x)/BITS_PER_LONG)
#define OFF(x)               ((x)%BITS_PER_LONG)
#define test_bit(bit, array) ((array[LONG(bit)] >> OFF(bit)) & 1)

int rumble_init(const char *device_path, struct ff_effect *effect, int *fd)
{
	int           device;
	unsigned long features[4];
	
	memset(effect, 0, sizeof(struct ff_effect));
	
	effect->type = FF_RUMBLE;
	effect->id = -1;
	effect->replay.length = 1000;
	
	if ((device = open(device_path, O_RDWR)) == -1)
	{
		return 0;
	}

	if (ioctl(device, EVIOCGBIT(EV_FF, sizeof(unsigned long) * 4), features) == -1)
	{
		close(device);
		return 0;
	}

	if (test_bit(FF_RUMBLE, features))
	{
		if (ioctl(device, EVIOCSFF, effect) == -1)
		{
			close(device);
			return 0;
		}
		
		*fd = device;
		return 1;
	}

	close(device);

	return 0;
}

int rumble_set(int fd, struct ff_effect *effect, uint8_t strong, uint8_t weak)
{
	struct input_event play;

	effect->u.rumble.strong_magnitude = (unsigned short)strong * 256;
	effect->u.rumble.weak_magnitude   = ((unsigned short)(weak > 0)) * 65535;

	if (ioctl(fd, EVIOCSFF, effect) == -1)
	{
		return -1;
	}

	play.type = EV_FF;
	play.code = effect->id;
	play.value = 1;

	if (write(fd, (const void*)&play, sizeof(play)) == -1)
	{
		return -1;
	}

	return 0;
}
