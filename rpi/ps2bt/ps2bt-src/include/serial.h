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

#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>
#include "controller.h"

#define SEND_PACKET_SIZE 20
#define RECV_PACKET_SIZE 4

int  serial_init(const char *serial_path);
void serial_construct_packet(controller_inputs *inputs, uint8_t (*packet)[SEND_PACKET_SIZE], int mode_switch);
int  serial_send_packet(int fd, uint8_t (*packet)[SEND_PACKET_SIZE]);
int  serial_send_disconnect_packet(int fd);
int  serial_recv_packet(int fd, uint8_t (*packet)[RECV_PACKET_SIZE]);

#endif
