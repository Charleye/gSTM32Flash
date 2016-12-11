/******************************************************************************
 * hexadecimal data operation
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

#ifndef _HEX_H
#define _HEX_H
#include <stdint.h>
#include "parser.h"

extern window_t *data;
extern parser_ops_t PARSER_HEX;

typedef struct 
{
	size_t		data_len, offset;
	uint8_t		*data;
	uint8_t		base;
}hex_t;

void*			hex_init();
parser_t		hex_open(void *storage, const char *filename);
parser_t		hex_close(void *storage);
unsigned int	hex_size(void *storage);
parser_t		hex_read(void *storage, void *data, unsigned int *len);
parser_t		hex_write(void *storage, void *data, unsigned int len);

#endif
