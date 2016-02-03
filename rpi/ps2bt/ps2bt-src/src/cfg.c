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
 
#include "ini.h"
#include "cfg.h"

void cfg_file_read(const char *ro_path, const char *rw_path, cfg_settings *settings)
{
	unsigned int  i, k;
	ini_key      *key;
	ini_key_list  input;
	
	memset(&input, 0, sizeof(input));
	
	settings->default_pressure = 32;
	settings->analog_to_button_deadzone = 64;
	
	settings->ds3_leds[0] = 1;
	settings->ds3_leds[1] = 0;
	
	settings->ds4_leds[DS4_LED_RED] = 0;
	settings->ds4_leds[DS4_LED_GREEN] = 0;
	settings->ds4_leds[DS4_LED_BLUE] = 32;
	settings->ds4_triangle_pressure = 1;
	settings->ds4_circle_pressure = 255;
	settings->ds4_cross_pressure = 32;
	settings->ds4_square_pressure = 128;
	settings->ds4_range_emulation_default = 1;
	settings->ds4_range_emulation_divisor = 0.82;
	
	for (i = 0; i < 24; i++)
	{
		settings->map.inputs.by_offset[i] = -1;
	}
	
    if (ini_read(rw_path, &input) == INI_ERROR_SUCCESS)
	{
		int color;
		
		if ((key = ini_key_list_search(&input, "ds4", "led_red")))
		{
			color = atoi(key->value);
			color = (color > 255) ? 255 : color;
			color = (color < 0) ? 0 : color;
			
			settings->ds4_leds[DS4_LED_RED] = (uint8_t)color;
		}
		
		if ((key = ini_key_list_search(&input, "ds4", "led_green")))
		{
			color = atoi(key->value);
			color = (color > 255) ? 255 : color;
			color = (color < 0) ? 0 : color;
			
			settings->ds4_leds[DS4_LED_GREEN] = (uint8_t)color;
		}
		
		if ((key = ini_key_list_search(&input, "ds4", "led_blue")))
		{
			color = atoi(key->value);
			color = (color > 255) ? 255 : color;
			color = (color < 0) ? 0 : color;
			
			settings->ds4_leds[DS4_LED_BLUE] = (uint8_t)color;
		}
		
		ini_key_list_empty(&input);
		memset(&input, 0, sizeof(input));
	}
	
	if (ini_read(ro_path, &input) == INI_ERROR_SUCCESS)
	{
		int pressure;
		
		if ((key = ini_key_list_search(&input, "common", "default_pressure")))
		{
			pressure = atoi(key->value);
			pressure = (pressure > 255) ? 255 : pressure;
			pressure = (pressure < 1) ? 1 : pressure;
			
			settings->default_pressure = pressure;
		}
		
		if ((key = ini_key_list_search(&input, "common", "analog_to_button_deadzone")))
		{
			int deadzone;
			
			deadzone = atoi(key->value);
			deadzone = (deadzone > 126) ? 126 : deadzone;
			deadzone = (deadzone < 0) ? 0 : deadzone;
			
			settings->analog_to_button_deadzone = deadzone;
		}
		
		if ((key = ini_key_list_search(&input, "ds3", "led_one")))
		{
			settings->ds3_leds[0] = (!strcmp(key->value, "on")) ? 1 : 0;
		}
		
		if ((key = ini_key_list_search(&input, "ds3", "led_two")))
		{
			settings->ds3_leds[1] = (!strcmp(key->value, "on")) ? 1 : 0;
		}
		
		if ((key = ini_key_list_search(&input, "ds4", "triangle_pressure")))
		{
			pressure = atoi(key->value);
			pressure = (pressure > 255) ? 255 : pressure;
			pressure = (pressure < 1) ? 1 : pressure;
			
			settings->ds4_triangle_pressure = pressure;
		}
		
		if ((key = ini_key_list_search(&input, "ds4", "circle_pressure")))
		{
			pressure = atoi(key->value);
			pressure = (pressure > 255) ? 255 : pressure;
			pressure = (pressure < 1) ? 1 : pressure;
			
			settings->ds4_circle_pressure = pressure;
		}
		
		if ((key = ini_key_list_search(&input, "ds4", "cross_pressure")))
		{
			pressure = atoi(key->value);
			pressure = (pressure > 255) ? 255 : pressure;
			pressure = (pressure < 1) ? 1 : pressure;
			
			settings->ds4_cross_pressure = pressure;
		}
		
		if ((key = ini_key_list_search(&input, "ds4", "square_pressure")))
		{
			pressure = atoi(key->value);
			pressure = (pressure > 255) ? 255 : pressure;
			pressure = (pressure < 1) ? 1 : pressure;
			
			settings->ds4_square_pressure = pressure;
		}
		
		if ((key = ini_key_list_search(&input, "ds4", "analog_range_emulation_default")))
		{
			settings->ds4_range_emulation_default = (!strcmp(key->value, "true")) ? 1 : 0;
		}
		
		if ((key = ini_key_list_search(&input, "ds4", "analog_range_emulation_divisor")))
		{
			double divisor;
			
			divisor = atof(key->value);
			divisor = (divisor > 1.0) ? 1.0 : divisor;
			divisor = (divisor < 0.1) ? 0.1 : divisor;
			
			settings->ds4_range_emulation_divisor = divisor;
		}
		
		for (i = 0; i < 24; i++)
		{
			if ((key = ini_key_list_search(&input, "custom_mapping", controller_map_offset_to_string[i])))
			{
				for (k = 0; k < 24; k++)
				{
					if (!strcmp(key->value, controller_map_offset_to_string[k]))
					{
						settings->map.inputs.by_offset[i] = k;
					}
				}
			}
		}
		
		ini_key_list_empty(&input);
	}
}

void cfg_file_write(const char *rw_path, cfg_settings *settings)
{
	ini_key_list output;
	char         buffer_value[256];
	
	memset(&output, 0, sizeof(output));
	
	sprintf(buffer_value, "%d", (int)settings->ds4_leds[DS4_LED_RED]);
	
	if (ini_key_list_insert(&output, "ds4", "led_red", buffer_value) != INI_ERROR_SUCCESS)
	{
		return;
	}
	
	sprintf(buffer_value, "%d", (int)settings->ds4_leds[DS4_LED_GREEN]);
	
	if (ini_key_list_insert(&output, "ds4", "led_green", buffer_value) != INI_ERROR_SUCCESS)
	{
		ini_key_list_empty(&output);
		return;
	}
	
	sprintf(buffer_value, "%d", (int)settings->ds4_leds[DS4_LED_BLUE]);
	
	if (ini_key_list_insert(&output, "ds4", "led_blue", buffer_value) != INI_ERROR_SUCCESS)
	{
		ini_key_list_empty(&output);
		return;
	}
	
	ini_write(rw_path, &output);
	
	ini_key_list_empty(&output);
}
