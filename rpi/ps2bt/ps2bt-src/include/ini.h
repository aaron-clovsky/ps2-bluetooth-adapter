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

#ifndef INI_H
#define INI_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
	INI_ERROR_SUCCESS        =  0,
	INI_ERROR_MALLOC_FAILED  = -1,
	INI_ERROR_OPEN_FAILED    = -2,
	INI_ERROR_READ_FAILED    = -3,
	INI_ERROR_INVALID_FILE   = -4,
	INI_ERROR_WRITE_FAILED   = -5,
	INI_ERROR_NO_DEFINITIONS = -6
} ini_error;

struct ini_key
{
	char           *section;
	char           *key;
	char           *value;
	struct ini_key *next;
	char            storage[1];
};

typedef struct ini_key ini_key;

typedef struct 
{
	ini_key *head;
	ini_key *tail;
} ini_key_list;

ini_error ini_key_list_insert(ini_key_list *list, const char *section, const char *key, const char *value);
void      ini_key_list_empty(ini_key_list *list);
ini_key  *ini_key_list_search(ini_key_list *list, const char *section, const char *key);
ini_error ini_read(const char *path, ini_key_list *list);
ini_error ini_write(const char *path, ini_key_list *list);
void      ini_print(ini_key_list *list);

#endif
