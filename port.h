/******************************************************************************
 * serial port
 * ****************************************************************************
 * Copyright (C) 2016
 * Written by Kart (kartdream@163.com)
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 * ****************************************************************************
 */

#ifndef _SERIAL_H
#define _SERIAL_H

#include <stdint.h>
#include <stdio.h>
#include <termios.h>

typedef struct serial 
{
	int				fd;
	struct termios	oldtio;
	struct termios	newtio;
	char			setup_str[11];
} serial_t;

typedef enum 
{
	SERIAL_PARITY_NONE,
	SERIAL_PARITY_EVEN,
	SERIAL_PARITY_ODD,
	
	SERIAL_PARITY_INVALID
} serial_parity_t;

typedef enum 
{
	SERIAL_BITS_5,
	SERIAL_BITS_6,
	SERIAL_BITS_7,
	SERIAL_BITS_8,

	SERIAL_BITS_INVALID
} serial_bits_t;

typedef enum 
{
	SERIAL_BAUD_1200,
	SERIAL_BAUD_1800,
	SERIAL_BAUD_2400,
	SERIAL_BAUD_4800,
	SERIAL_BAUD_9600,
	SERIAL_BAUD_19200,
	SERIAL_BAUD_38400,
	SERIAL_BAUD_57600,
	SERIAL_BAUD_115200,
	SERIAL_BAUD_128000,
	SERIAL_BAUD_230400,
	SERIAL_BAUD_256000,
	SERIAL_BAUD_460800,
	SERIAL_BAUD_500000,
	SERIAL_BAUD_576000,
	SERIAL_BAUD_921600,
	SERIAL_BAUD_1000000,
	SERIAL_BAUD_1500000,
	SERIAL_BAUD_2000000,

	SERIAL_BAUD_INVALID
} serial_baud_t;

typedef enum 
{
	SERIAL_STOPBIT_1,
	SERIAL_STOPBIT_2,

	SERIAL_STOPBIT_INVALID
} serial_stopbit_t;

typedef enum 
{
	GPIO_RTS = 1,
	GPIO_DTR,
	GPIO_BRK,
} serial_gpio_t;

/* common functions */
serial_baud_t		serial_get_baud(const unsigned int baud);
unsigned int		serial_get_baud_int(const serial_baud_t baud);
serial_bits_t		serial_get_bits(const char *mode);
unsigned int		serial_get_bits_int(const serial_bits_t bits);
serial_parity_t		serial_get_parity(const char *mode);
char				serial_get_parity_str(const serial_parity_t parity);
serial_stopbit_t	serial_get_stopbit(const char *mode);
unsigned int		serial_get_stopbit_int(const serial_stopbit_t stopbit);


typedef enum port 
{
	PORT_OK = 0,
	PORT_ERR_NODEV,		/* No such device */
	PORT_ERR_TIMEDOUT,	/* Operation timed out */
	PORT_ERR_UNKNOWN,
} port_t;

/* flags */
#define PORT_BYTE		(1 << 0)	/* byte (not frame) oriented */
#define PORT_GVR_ETX	(1 << 1)	/* cmd GVR returns protection status */
#define PORT_CMD_INIT	(1 << 2)	/* use INIT cmd to autodetect speed */
#define PORT_RETRY		(1 << 3)	/* allowed read() retry after timeout */
#define PORT_STRETCH_W	(1 << 4)	/* warning for no-stretching commands */

/* all options and flags used to open and configure an interface */
typedef struct port_options 
{
	const char		*device;
	serial_baud_t	baudrate;
	const char		*serial_mode;
	int				bus_addr;
	int				rx_frame_max;
	int				tx_frame_max;
}port_opt_t;

/*
 * Specify the length of reply for command GET
 * This is helpful for frame-oriented protocols, e.g. i2c, to avoid time
 * consuming try-fail-timeout-retry operation.
 * On byte-oriented protocols, i.e. UART, this information would be skipped
 * after read the first byte, so not needed.
 */
typedef struct varlen_cmd 
{
	uint8_t version;
	uint8_t length;
} varlen_cmd_t;

typedef struct port_interface 
{
	const char	*name;
	unsigned	flags;
	port_t		(*open)(struct port_interface *port, struct port_options *ops);
	void		(*close)(struct port_interface *port);
	port_t		(*read)(struct port_interface *port, void *buf, size_t nbyte);
	port_t		(*write)(struct port_interface *port, void *buf, size_t nbyte);
	port_t		(*gpio)(struct port_interface *port, serial_gpio_t n, int level);
	const char*	(*get_cfg_str)(struct port_interface *port);
	varlen_cmd_t* cmd_get_reply;
	void		*private;
}port_interface_t;

port_t port_open(port_opt_t *ops, port_interface_t **outport);

#endif
