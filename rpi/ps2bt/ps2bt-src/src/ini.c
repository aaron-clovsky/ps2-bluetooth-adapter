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

#include <ctype.h>
#include "ini.h"

/* 0 - success
  -1 - failed to allocate memory */
static int ini_key_list_insert_internal(ini_key_list *list, const char *section, unsigned int length_section,
                                        const char *key, unsigned int length_key, 
                                        const char *value, unsigned int length_value)
{
	unsigned int offset;
	unsigned int size_section;
	unsigned int size_key;
	unsigned int size_value;
	ini_key *next;
	
	size_section = section ? length_section + 1 : 0;
	size_key = length_key + 1;
	size_value = length_value + 1;
	offset = 0;
	
	next = (ini_key *)calloc(1, sizeof(ini_key) - 1 + size_section + size_key + size_value);
	if (!next)
	{
		return INI_ERROR_MALLOC_FAILED;
	}
	
	if (section)
	{
		next->section = &next->storage[offset];
		memcpy(next->section, section, (size_t)length_section);
		offset += size_section;
	}
	else
	{
		next->section = NULL;
	}
	
	next->key = &next->storage[offset];
	memcpy(next->key, key, length_key);
	offset += size_key;
	
	next->value = &next->storage[offset];
	memcpy(next->value, value, length_value);
	
	next->next = NULL;
	
	if (!list->head)
	{
		list->head = next;
		list->tail = next;
	}
	else
	{
		list->tail->next = next;
		list->tail = next;
	}
	
	return INI_ERROR_SUCCESS;
}

/* 0 - success
  -1 - failed to allocate memory
  -2 - failed to open file
  -3 - failed to read file
  -4 - invalid text file (contains character '\0') */
static ini_error ini_read_file(const char *path, char **buffer, unsigned int *size)
{
	FILE *input;
	char *buf;
	long  length;
	
	input = fopen(path, "rb");
	if (!input)
	{
		return INI_ERROR_OPEN_FAILED;
	}
	
	if (fseek(input, 0, SEEK_END) == -1)
	{
		fclose(input);
		return INI_ERROR_READ_FAILED;
	}
	
	length = ftell(input);
	
	if (length == -1)
	{
		fclose(input);
		return INI_ERROR_READ_FAILED;
	}
	
	buf = (char *)malloc(length + 2);
	if (!buf)
	{
		fclose(input);
		return INI_ERROR_MALLOC_FAILED;
	}
	
	if (fseek(input, 0, SEEK_SET) == -1)
	{
		fclose(input); free(buf);
		return INI_ERROR_READ_FAILED;
	}
	
	if (fread(buf, length, 1, input) != 1)
	{
		fclose(input);
		free(buf);
		return INI_ERROR_READ_FAILED;
	}
	
	fclose(input);
	
	{
		unsigned int i;
		
		for (i = 0; i < length; i++)
		{
			if (!buf[i])
			{
				free(buf);
				return INI_ERROR_INVALID_FILE;
			}
		}
	}
	
	buf[length] = '\n';
	buf[length+1] = '\0';
	
	*buffer = buf;
	
	if (size)
	{
		*size = length;
	}
	
	return INI_ERROR_SUCCESS;
}

static ini_error ini_fprint(FILE *file, ini_key_list *list)
{
	const char *section;
	ini_key    *current;
	int         first_section;
	int         no_section;
	
	section = "";
	current = list->head;
	first_section = 0;
	no_section = 0;
	
	while (current)
	{
		if (current->section && strcmp(current->section, section))
		{
			if (!first_section && !no_section)
			{
				if (fprintf(file, "[%s]\r\n", current->section) < 0)
				{
					return INI_ERROR_WRITE_FAILED;
				}
			}
			else
			{
				if (fprintf(file, "\r\n[%s]\r\n", current->section) < 0)
				{
					return INI_ERROR_WRITE_FAILED;
				}
			}
			
			section = current->section;
			first_section = 1;
		}
		
		if (fprintf(file, "%s=%s\r\n", current->key, current->value) < 0)
		{
			return INI_ERROR_WRITE_FAILED;
		}
		
		if (!first_section)
		{
			no_section = 1;
		}
		
		current = current->next;
	}
	
	return INI_ERROR_SUCCESS;
}

ini_error ini_key_list_insert(ini_key_list *list, const char *section, const char *key, const char *value)
{
	return ini_key_list_insert_internal(list, section, strlen(section), key, strlen(key), value, strlen(value));
}

void ini_key_list_empty(ini_key_list *list)
{
	ini_key *next;
	
	while (list->head)
	{
		next = list->head->next;
		free(list->head);
		list->head = next;
	}
}

/* !NULL - found
    NULL - not found */
ini_key *ini_key_list_search(ini_key_list *list, const char *section, const char *key)
{
	ini_key *current = list->head;
	
	while (current)
	{
		if (!section || !current->section || !strcmp(current->section, section))
		{
			if (!strcmp(current->key, key))
			{
				break;
			}
		}
		
		current = current->next;
	}
	
	return current;
}

/* 0 - success
  -1 - failed to allocate memory
  -2 - failed to open file
  -3 - failed to read file
  -4 - invalid text file (contains character '\0')
  -6 - no valid key definitions */
ini_error ini_read(const char *path, ini_key_list *list)
{
	ini_error  error;
	char      *buffer;
	
	ini_key_list_empty(list);
	
	if ((error = ini_read_file(path, &buffer, NULL)) != 0)
	{
		return error;
	}
	
	{
		const char   *s;
		const char   *section, *tmp_section;
		const char   *key;
		const char   *value;
		unsigned int  length_section;
		unsigned int  length_key;
		unsigned int  length_value;
		int           state;
		
		section = NULL;
		tmp_section = NULL;
		key = NULL;
		value = NULL;
		length_section = 0;
		length_key = 0;
		length_value = 0;
		
		s = buffer;
		state = 0;
		
		while (*s)
		{
			char c;
			
			c = (isspace(*s) && *s != '\n') ? ' ' : *s;
			c = (ispunct(c) && c != ';' && c != '#') ? '.' : c;
			c = (isalnum(c)) ? 'a' : c;
			
			switch (state)
			{
				case 0:
				{
					if (c == ' ' || c == '\n')
					{
						state = 0;
					}
					else if (*s == '[')
					{
						state = 10;
					}
					else if (c == 'a' || c == '.')
					{
						key = s;
						state = 20;
					}
					else if (c == ';' || c == '#')
					{
						state = 99;
					}
					
					break;
				}
				/* Parse section */
				case 10:
				{
					if (*s != ']' && (c == 'a' || c == '.'))
					{
						tmp_section = s;
						state = 11;
					}
					else if (c != ' ')
					{
						state = 99;
					}
					
					break;
				}
				case 11:
				{
					if (c == ' ')
					{
						state = 12;
					}
					else if (c != 'a' && c != '.')
					{
						state = 99;
					}
					
					if (*s != ']')
					{
						break;
					}
					/* Fall through */
				}
				case 12:
				{
					if (*s == ']')
					{
						const char *ss;
						
						ss = s - 1;
						
						while (isspace(*ss)) ss--;
						
						length_section = ss - tmp_section + 1;
						section = tmp_section;
						
						state = 99;
					}
					else if (c != ' ')
					{
						state = 99;
					}
					
					break;
				}
				/* Parse key and value */
				case 20:
				{
					if (c == ' ' || *s == '=')
					{
						length_key = s - key;
						
						state = (c == ' ') ? 21 : 22;
					}
					else if (c != 'a' && c != '.')
					{
						state = 99;
					}
					
					break;
				}
				case 21:
				{
					if (*s == '=')
					{
						state = 22;
					}
					else if (c != ' ')
					{
						state = 99;
					}
					
					break;
				}
				case 22:
				{
					if (c == 'a' || c == '.')
					{
						value = s;
						state = 23;
					}
					else if (c != ' ')
					{
						state = 99;
					}
					
					break;
				}
				case 23:
				{
					if (c == ' ' || c == '\n' || c == ';' || c == '#')
					{
						length_value = s - value;
						
						if ((error = ini_key_list_insert_internal(list, section, length_section, key, 
						                                          length_key, value, length_value)) != 0)
						{
							ini_key_list_empty(list);
							free(buffer);
							return error;
						}
						
						state = 99;
					}
					else if (c != 'a' && c != '.')
					{
						state = 99;
					}
					
					break;
				}
				case 100:
				{
					if (c == '\n')
					{
						state = 0;
					}
					
					break;
				}
			}
			
			if (state == 99) 
			{
				state = 100;
			}
			else
			{
				s++;
			}
		}
	}
	
	free(buffer);
	
	if (!list->head)
	{
		return INI_ERROR_NO_DEFINITIONS;
	}
	
	return INI_ERROR_SUCCESS;
}

/* 0 - success
  -1 - failed to open file
  -5 - failed to write file*/
ini_error ini_write(const char *path, ini_key_list *list)
{
	ini_error  error;
	FILE      *output;
	
	output = fopen(path, "wb");
	if (!output)
	{
		return INI_ERROR_OPEN_FAILED;
	}
	
	if ((error = ini_fprint(output, list)) != 0)
	{
		fclose(output);
		return error;
	}
	
	fclose(output);
	
	return INI_ERROR_SUCCESS;
}

void ini_print(ini_key_list *list)
{
	ini_fprint(stdout, list);
}
