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
#include <math.h>
#include "controller.h"

#define ROUND(x) ((x < 0) ? ceil((x)-0.5) : floor((x)+0.5))

const char *controller_map_offset_to_string[24] = 
{
	"start",
	"select",
	"l3",
	"r3",
	"triangle",
	"circle",
	"cross",
	"square",
	"up",
	"right",
	"down",
	"left",
	"l2",
	"r2",
	"l1",
	"r1",
	"ls_up",
	"ls_left",
	"rs_up",
	"rs_left",
	"ls_down",
	"ls_right",
	"rs_down",
	"rs_right"
};

uint16_t controller_map_offset_to_buffer[24] = 
{
	0,  0,  0,  0,
	12, 13, 14, 15, 
	4,  5,  6,  7,
	8,  9,  10, 11,
	1,  0,  3,  2,
	1,  0,  3,  2
};

uint16_t controller_map_offset_to_bitmask[16] = 
{
	0x0008, 0x0001, 0x0002, 0x0004,
	0x1000, 0x2000, 0x4000, 0x8000,
	0x0010, 0x0020, 0x0040, 0x0080,
	0x0100, 0x0200, 0x0400, 0x0800
};

static void controller_remap_digital(controller_inputs *in, controller_inputs *out, controller_map *map,
                                     int offset, uint8_t default_pressure)
{
	if (in->buttons & controller_map_offset_to_bitmask[offset])
	{
		if (map->inputs.by_offset[offset] <= 3)
		{
			out->buttons |= controller_map_offset_to_bitmask[map->inputs.by_offset[offset]];
		}
		else if (map->inputs.by_offset[offset] <= 15)
		{
			out->buttons |= controller_map_offset_to_bitmask[map->inputs.by_offset[offset]];
			out->axes.buffer[controller_map_offset_to_buffer[map->inputs.by_offset[offset]]] = default_pressure;
		}
		else
		{
			if (map->inputs.by_offset[offset] <= 19)
			{
				out->axes.buffer[controller_map_offset_to_buffer[map->inputs.by_offset[offset]]] = 0x00;
			}
			else
			{
				out->axes.buffer[controller_map_offset_to_buffer[map->inputs.by_offset[offset]]] = 0xff;
			}
		}
	}
}

static void controller_remap_button(controller_inputs *in, controller_inputs *out, controller_map *map, int offset)
{
	if (in->buttons & controller_map_offset_to_bitmask[offset])
	{		
		if (map->inputs.by_offset[offset] <= 3)
		{
			out->buttons |= controller_map_offset_to_bitmask[map->inputs.by_offset[offset]];
		}
		else if (map->inputs.by_offset[offset] <= 15)
		{
			out->buttons |= controller_map_offset_to_bitmask[map->inputs.by_offset[offset]];
			out->axes.buffer[controller_map_offset_to_buffer[map->inputs.by_offset[offset]]] =
			                                                  in->axes.buffer[controller_map_offset_to_buffer[offset]];
		}
		else
		{
			if (map->inputs.by_offset[offset] <= 19)
			{
				out->axes.buffer[controller_map_offset_to_buffer[map->inputs.by_offset[offset]]] =
				                                0x80 - (in->axes.buffer[controller_map_offset_to_buffer[offset]] >> 1);
			}
			else
			{
				out->axes.buffer[controller_map_offset_to_buffer[map->inputs.by_offset[offset]]] =
				                                0x80 + (in->axes.buffer[controller_map_offset_to_buffer[offset]] >> 1);
			}
		}
	}
}

static void controller_remap_axis(controller_inputs *in, controller_inputs *out, controller_map *map,
                                  int offset, uint8_t deadzone)
{
	uint8_t value;
	int     active;
	
	value = in->axes.buffer[controller_map_offset_to_buffer[offset]];
	active = (offset <= 19) ? (value < 0x80 - deadzone) : (value > 0x80 + deadzone);
	
	if (map->inputs.by_offset[offset] <= 3)
	{
		if (active)
		{
			out->buttons |= controller_map_offset_to_bitmask[map->inputs.by_offset[offset]];
		}
	}
	else if (map->inputs.by_offset[offset] <= 15)
	{
		if (active)
		{
			out->buttons |= controller_map_offset_to_bitmask[map->inputs.by_offset[offset]];
		
			if (offset <= 19)
			{
				out->axes.buffer[controller_map_offset_to_buffer[map->inputs.by_offset[offset]]] =
				                       (uint8_t)ROUND(((128 - deadzone) - (double)value) * (255.0 / (128 - deadzone)));
			}
			else
			{
				out->axes.buffer[controller_map_offset_to_buffer[map->inputs.by_offset[offset]]] =
				                       (uint8_t)ROUND(((double)value - (128 + deadzone)) * (255.0 / (127 - deadzone)));
			}
		}
	}
	else if (map->inputs.by_offset[offset] <= 19)
	{
		if (offset <= 19)
		{
			if (value < 0x80) out->axes.buffer[controller_map_offset_to_buffer[map->inputs.by_offset[offset]]] = value;
		}
		else
		{
			if (value > 0x80) out->axes.buffer[controller_map_offset_to_buffer[map->inputs.by_offset[offset]]] = 
			                                                                                                    ~value;
		}
	}
	else
	{
		if (offset <= 19)
		{
			if (value < 0x80) out->axes.buffer[controller_map_offset_to_buffer[map->inputs.by_offset[offset]]] =
			                                                                                                    ~value;
		}
		else
		{
			if (value > 0x80) out->axes.buffer[controller_map_offset_to_buffer[map->inputs.by_offset[offset]]] = value;
		}
	}
}

void controller_remap(controller_inputs *in, controller_inputs *out, controller_map *map,
                      uint8_t default_pressure, uint8_t deadzone)
{
	memset(out, 0, sizeof(controller_inputs));
	
	out->axes.named.lx = 0x80;
	out->axes.named.ly = 0x80;
	out->axes.named.rx = 0x80;
	out->axes.named.ry = 0x80;
	
	{
		unsigned int i = 0;
		
		for (; i < 4; i++)
		{
			if (map->inputs.by_offset[i] >= 0 && map->inputs.by_offset[i] < 24)
			{
				controller_remap_digital(in, out, map, i, default_pressure);
			}
		}
		for (; i < 16; i++)
		{
			if (map->inputs.by_offset[i] >= 0 && map->inputs.by_offset[i] < 24)
			{
				controller_remap_button(in, out, map, i);
			}
		}
		for (; i < 24; i++)
		{
			if (map->inputs.by_offset[i] >= 0 && map->inputs.by_offset[i] < 24)
			{
				controller_remap_axis(in, out, map, i, deadzone);
			}
		}
	}
}
