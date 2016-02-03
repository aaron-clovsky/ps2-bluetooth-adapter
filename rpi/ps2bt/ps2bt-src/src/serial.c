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

#define _DEFAULT_SOURCE
#define _BSD_SOURCE /* CRTSCTS */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <termios.h>
#include "serial.h"

int serial_init(const char *serial_path)
{
	int            fd;
	struct termios tty;
	
	if ((fd = open(serial_path, O_RDWR | O_NOCTTY | O_SYNC)) == -1)
	{
		fprintf(stderr, "error opening %s: %s", serial_path, strerror (errno));
		return -1;
	}
	
	memset(&tty, 0, sizeof(tty));
	
	if (tcgetattr (fd, &tty) != 0)
	{
		fprintf(stderr, "error %d from tcgetattr", errno);
		return -1;
	}
	
	cfsetospeed(&tty, B38400);
	cfsetispeed(&tty, B38400);
	
	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; /* 8-bit chars */
	
	/* Disable IGNBRK for mismatched speed tests; otherwise receive break as \000 chars */
	tty.c_iflag &= ~IGNBRK;                 /* Disable break processing */
	tty.c_lflag = 0;                        /* No signaling chars, no echo, */
	                                        /* No canonical processing */
	tty.c_oflag = 0;                        /* No controller_remapping, no delays */
	tty.c_cc[VMIN]  = 1; /* should_block */ /* Read blocking */
	tty.c_cc[VTIME] = 5;                    /* 0.5 seconds read timeout */
	
	tty.c_iflag &= ~(IXON | IXOFF | IXANY); /* Shut off xon/xoff ctrl */
	
	tty.c_cflag |= (CLOCAL | CREAD);        /* Ignore modem controls, */
	                                        /* Enable reading */
	tty.c_cflag &= ~(PARENB | PARODD);      /* Shut off parity */
	/*tty.c_cflag |= parity;*/
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CRTSCTS;
	
	if (tcsetattr(fd, TCSANOW, &tty) != 0)
	{
		fprintf(stderr, "error %d from tcsetattr", errno);
		return -1;
	}
	
	return fd;
}

void serial_construct_packet(controller_inputs *inputs, uint8_t (*packet)[SEND_PACKET_SIZE], int mode_switch)
{
	(*packet)[0] = 0x5A;
	
	(*packet)[1] = ~((uint8_t *)&inputs->buttons)[0];
	(*packet)[2] = ~((uint8_t *)&inputs->buttons)[1];
	(*packet)[3] = inputs->axes.buffer[2];   /* RX */
	(*packet)[4] = inputs->axes.buffer[3];   /* RY */
	(*packet)[5] = inputs->axes.buffer[0];   /* LX */
	(*packet)[6] = inputs->axes.buffer[1];   /* LY */
	
	(*packet)[7] = inputs->axes.buffer[5];   /* Right */
	(*packet)[8] = inputs->axes.buffer[7];   /* Left */
	(*packet)[9] = inputs->axes.buffer[4];   /* Up */
	(*packet)[10] = inputs->axes.buffer[6];  /* Down */
	
	(*packet)[11] = inputs->axes.buffer[12]; /* Triangle */
	(*packet)[12] = inputs->axes.buffer[13]; /* Circle */
	(*packet)[13] = inputs->axes.buffer[14]; /* Cross */
	(*packet)[14] = inputs->axes.buffer[15]; /* Square */
	
	(*packet)[15] = inputs->axes.buffer[10]; /* L1 */
	(*packet)[16] = inputs->axes.buffer[11]; /* R1 */
	(*packet)[17] = inputs->axes.buffer[8];  /* L2 */
	(*packet)[18] = inputs->axes.buffer[9];  /* R2 */
	
	/* Footer - 0x55 normally - 0xAA if mode button was pressed */
	if (mode_switch)
	{
		(*packet)[19] = 0xAA;
	}
	else
	{
		(*packet)[19] = 0x55;
	}
}

int serial_send_packet(int fd, uint8_t (*packet)[SEND_PACKET_SIZE])
{
	if (write(fd, (void *)packet, SEND_PACKET_SIZE) == -1)
	{
		return -1;
	}
	
	return 0;
}

int serial_send_disconnect_packet(int fd)
{
	uint8_t packet[SEND_PACKET_SIZE];
	
	memset(&packet[0], 0, sizeof(packet));
	
	packet[0] = 0x5A;
	packet[19] = 0x5A;
	
	return serial_send_packet(fd, &packet);
}

int serial_recv_packet(int fd, uint8_t (*packet)[RECV_PACKET_SIZE])
{
	if (read(fd, (void *)packet, RECV_PACKET_SIZE) != RECV_PACKET_SIZE)
	{
		return -1;
	}
	
	return 0;
}
