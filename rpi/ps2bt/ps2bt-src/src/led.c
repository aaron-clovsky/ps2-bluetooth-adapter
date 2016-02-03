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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/limits.h>
#include "led.h"

static int led_ds3_paths(char (*paths)[4][PATH_MAX+1])
{
	DIR           *d;
	struct dirent *dir;
	int            success = 0;
	
	(*paths)[0][PATH_MAX] = '\0';
	(*paths)[1][PATH_MAX] = '\0';
	(*paths)[2][PATH_MAX] = '\0';
	(*paths)[3][PATH_MAX] = '\0';
	
	if (!(d = opendir("/sys/class/leds")))
	{
		return 0;
	}
	
	while ((dir = readdir(d)) != NULL)
	{
		if (strstr(dir->d_name, "sony1"))
		{
			strncpy((*paths)[DS3_LED_ONE], "/sys/class/leds/", PATH_MAX);
			strncat((*paths)[DS3_LED_ONE], dir->d_name, PATH_MAX);
			strncat((*paths)[DS3_LED_ONE], "/brightness", PATH_MAX);
			success += 1;
		}
		else if (strstr(dir->d_name, "sony2"))
		{
			strncpy((*paths)[DS3_LED_TWO], "/sys/class/leds/", PATH_MAX);
			strncat((*paths)[DS3_LED_TWO], dir->d_name, PATH_MAX);
			strncat((*paths)[DS3_LED_TWO], "/brightness", PATH_MAX);
			success += 1;
		}
		else if (strstr(dir->d_name, "sony3"))
		{
			strncpy((*paths)[DS3_LED_THREE], "/sys/class/leds/", PATH_MAX);
			strncat((*paths)[DS3_LED_THREE], dir->d_name, PATH_MAX);
			strncat((*paths)[DS3_LED_THREE], "/brightness", PATH_MAX);
			success += 1;
		}
		else if (strstr(dir->d_name, "sony4"))
		{
			strncpy((*paths)[DS3_LED_FOUR], "/sys/class/leds/", PATH_MAX);
			strncat((*paths)[DS3_LED_FOUR], dir->d_name, PATH_MAX);
			strncat((*paths)[DS3_LED_FOUR], "/brightness", PATH_MAX);
			success += 1;
		}
	}
	
	closedir(d);
	
	return (success == 4);
}

int led_ds3_init(int (*led_devices)[4])
{
	char buffer[4][PATH_MAX+1];
	
	if (!led_ds3_paths(&buffer))
	{
		return 0;
	}
	
	if (((*led_devices)[DS3_LED_ONE] = open(buffer[DS3_LED_ONE], O_RDWR)) == -1)
	{
		return 0;
	}
	if (((*led_devices)[DS3_LED_TWO] = open(buffer[DS3_LED_TWO], O_RDWR)) == -1)
	{
		close((*led_devices)[DS3_LED_ONE]);
		return 0;
	}
	if (((*led_devices)[DS3_LED_THREE] = open(buffer[DS3_LED_THREE], O_RDWR)) == -1)
	{
		close((*led_devices)[DS3_LED_ONE]);
		close((*led_devices)[DS3_LED_TWO]);
		return 0;
	}
	if (((*led_devices)[DS3_LED_FOUR] = open(buffer[DS3_LED_FOUR], O_RDWR)) == -1)
	{
		close((*led_devices)[DS3_LED_ONE]);
		close((*led_devices)[DS3_LED_TWO]);
		close((*led_devices)[DS3_LED_THREE]);
		return 0;
	}
	
	return 1;
}

void led_ds3_get(int (*led_devices)[4], uint8_t (*led_values)[4])
{
	char state;
	
	if (read((*led_devices)[DS3_LED_ONE], (void *)&state, 1) != 1) return;
	(*led_values)[DS3_LED_ONE] = (state == '1');
	
	if (read((*led_devices)[DS3_LED_TWO], (void *)&state, 1) != 1) return;
	(*led_values)[DS3_LED_TWO] = (state == '1');
	
	if (read((*led_devices)[DS3_LED_THREE], (void *)&state, 1) != 1) return;
	(*led_values)[DS3_LED_THREE] = (state == '1');
	
	if (read((*led_devices)[DS3_LED_FOUR], (void *)&state, 1) != 1) return;
	(*led_values)[DS3_LED_FOUR] = (state == '1');
}

void led_ds3_set(int (*led_devices)[4], uint8_t (*led_values)[4],
                 uint8_t led_one, uint8_t led_two, uint8_t led_three, uint8_t led_four)
{
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wunused-result"
	if ((*led_values)[DS3_LED_ONE] != led_one)
	{
		char value = led_one ? '1' : '0';
		(*led_values)[DS3_LED_ONE] = (uint8_t)led_one;
		write((*led_devices)[DS3_LED_ONE], (void *)&value, 1);
	}
	if ((*led_values)[DS3_LED_TWO] != led_two)
	{
		char value = led_two ? '1' : '0';
		(*led_values)[DS3_LED_TWO] = (uint8_t)led_two;
		write((*led_devices)[DS3_LED_TWO], (void *)&value, 1);
	}
	if ((*led_values)[DS3_LED_THREE] != led_three)
	{
		char value = led_three ? '1' : '0';
		(*led_values)[DS3_LED_THREE] = (uint8_t)led_three;
		write((*led_devices)[DS3_LED_THREE], (void *)&value, 1);
	}
	if ((*led_values)[DS3_LED_FOUR] != led_four)
	{
		char value = led_four ? '1' : '0';
		(*led_values)[DS3_LED_FOUR] = (uint8_t)led_four;
		write((*led_devices)[DS3_LED_FOUR], (void *)&value, 1);
	}
	#pragma GCC diagnostic pop
}

static int led_ds4_paths(char (*paths)[4][PATH_MAX+1])
{
	DIR           *d;
	struct dirent *dir;
	int            success = 0;
	
	(*paths)[0][PATH_MAX] = '\0';
	(*paths)[1][PATH_MAX] = '\0';
	(*paths)[2][PATH_MAX] = '\0';
	(*paths)[3][PATH_MAX] = '\0';
	
	if (!(d = opendir("/sys/class/leds")))
	{
		return 0;
	}
	
	while ((dir = readdir(d)) != NULL)
	{
		if (strstr(dir->d_name, "red"))
		{
			strncpy((*paths)[DS4_LED_RED], "/sys/class/leds/", PATH_MAX);
			strncat((*paths)[DS4_LED_RED], dir->d_name, PATH_MAX);
			strncat((*paths)[DS4_LED_RED], "/brightness", PATH_MAX);
			success += 1;
		}
		else if (strstr(dir->d_name, "green"))
		{
			strncpy((*paths)[DS4_LED_GREEN], "/sys/class/leds/", PATH_MAX);
			strncat((*paths)[DS4_LED_GREEN], dir->d_name, PATH_MAX);
			strncat((*paths)[DS4_LED_GREEN], "/brightness", PATH_MAX);
			success += 1;
		}
		else if (strstr(dir->d_name, "blue"))
		{
			strncpy((*paths)[DS4_LED_BLUE], "/sys/class/leds/", PATH_MAX);
			strncat((*paths)[DS4_LED_BLUE], dir->d_name, PATH_MAX);
			strncat((*paths)[DS4_LED_BLUE], "/brightness", PATH_MAX);
			success += 1;
		}
		else if (strstr(dir->d_name, "global"))
		{
			strncpy((*paths)[DS4_LED_GLOBAL], "/sys/class/leds/", PATH_MAX);
			strncat((*paths)[DS4_LED_GLOBAL], dir->d_name, PATH_MAX);
			strncat((*paths)[DS4_LED_GLOBAL], "/brightness", PATH_MAX);
			success += 1;
		}
	}
	
	closedir(d);
	
	return (success == 4);
}

int led_ds4_init(int (*led_devices)[4])
{
	char buffer[4][PATH_MAX+1];
	
	if (!led_ds4_paths(&buffer))
	{
		return 0;
	}
	
	if (((*led_devices)[DS4_LED_RED] = open(buffer[DS4_LED_RED], O_RDWR)) == -1)
	{
		return 0;
	}
	if (((*led_devices)[DS4_LED_GREEN] = open(buffer[DS4_LED_GREEN], O_RDWR)) == -1)
	{
		close((*led_devices)[DS4_LED_RED]);
		return 0;
	}
	if (((*led_devices)[DS4_LED_BLUE] = open(buffer[DS4_LED_BLUE], O_RDWR)) == -1)
	{
		close((*led_devices)[DS4_LED_RED]);
		close((*led_devices)[DS4_LED_GREEN]);
		return 0;
	}
	if (((*led_devices)[DS4_LED_GLOBAL] = open(buffer[DS4_LED_GLOBAL], O_RDWR)) == -1)
	{
		close((*led_devices)[DS4_LED_RED]);
		close((*led_devices)[DS4_LED_GREEN]);
		close((*led_devices)[DS4_LED_BLUE]);
		return 0;
	}
	
	return 1;
}

void led_ds4_get(int (*led_devices)[4], uint8_t (*led_values)[4])
{
	char state[4];
	
	memset(&state[0], 0, sizeof(state));
	if (read((*led_devices)[DS4_LED_RED], (void *)&state[0], 3) < 1) return;
	(*led_values)[DS4_LED_RED] = (uint8_t)atoi(state);
	
	memset(&state[0], 0, sizeof(state));
	if (read((*led_devices)[DS4_LED_GREEN], (void *)&state[0], 3) < 1) return;
	(*led_values)[DS4_LED_GREEN] = (uint8_t)atoi(state);
	
	memset(&state[0], 0, sizeof(state));
	if (read((*led_devices)[DS4_LED_BLUE], (void *)&state[0], 3) < 1) return;
	(*led_values)[DS4_LED_BLUE] = (uint8_t)atoi(state);
	
	memset(&state[0], 0, sizeof(state));
	if (read((*led_devices)[DS4_LED_GLOBAL], (void *)&state[0], 3) < 1) return;
	(*led_values)[DS4_LED_GLOBAL] = (uint8_t)atoi(state);
}

void led_ds4_set(int (*led_devices)[4], uint8_t (*led_values)[4],
                 uint8_t red, uint8_t green, uint8_t blue, uint8_t global)
{
	char value[4];
	
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wunused-result"
	if ((*led_values)[DS4_LED_RED] != red)
	{
		(*led_values)[DS4_LED_RED] = (uint8_t)red;
		sprintf(value, "%u", (unsigned int)red);
		write((*led_devices)[DS4_LED_RED], (void *)&value[0], strlen(value));
	}
	if ((*led_values)[DS4_LED_GREEN] != green)
	{
		(*led_values)[DS4_LED_GREEN] = (uint8_t)green;
		sprintf(value, "%u", (unsigned int)green);
		write((*led_devices)[DS4_LED_GREEN], (void *)&value[0], strlen(value));
	}
	if ((*led_values)[DS4_LED_BLUE] != blue)
	{
		(*led_values)[DS4_LED_BLUE] = (uint8_t)blue;
		sprintf(value, "%u", (unsigned int)blue);
		write((*led_devices)[DS4_LED_BLUE], (void *)&value[0], strlen(value));
	}
	if ((*led_values)[DS4_LED_GLOBAL] != global)
	{
		value[0] = global ? '1' : '0';
		(*led_values)[DS4_LED_GLOBAL] = (uint8_t)global;
		write((*led_devices)[DS4_LED_GLOBAL], (void *)&value[0], 1);
	}
	#pragma GCC diagnostic pop
}
