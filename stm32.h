/******************************************************************************
 *
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

#ifndef _STM32_H
#define _STM32_H

#include <stdint.h>
#include "port.h"

#define STM32_MAX_RX_FRAME	256				/* cmd read memory */
#define STM32_MAX_TX_FRAME	(1 + 256 + 1)	/* cmd write memory */

typedef enum {
	STM32_OK = 0,
	STM32_ERR_UNKNOWN,	/* Generic error */
	STM32_ERR_NACK,
	STM32_ERR_NO_CMD,	/* Command not available in bootloader */
} stm32_t;

typedef struct stm32_struct		stm32_struct_t;
typedef struct stm32_cmd		stm32_cmd_t;
typedef struct stm32_dev		stm32_dev_t;

struct stm32_struct 
{
	const serial_t			*serial;
	struct port_interface	*port;
	uint8_t					bl_version;
	uint8_t					version;
	uint8_t					option1, option2;
	uint16_t				pid;
	stm32_cmd_t				*cmd;
	const stm32_dev_t		*dev;
};

struct stm32_dev 
{
	uint16_t	id;
	const char	*name;
	uint32_t	ram_start, ram_end;
	uint32_t	fl_start, fl_end;
	uint16_t	fl_pps; // pages per sector
	uint16_t	fl_ps;  // page size
	uint32_t	opt_start, opt_end;
	uint32_t	mem_start, mem_end;
};

struct stm32_cmd 
{
	uint8_t get;
	uint8_t gvr;
	uint8_t gid;
	uint8_t rm;
	uint8_t go;
	uint8_t wm;
	uint8_t er; /* this may be extended erase */
	uint8_t wp;
	uint8_t uw;
	uint8_t rp;
	uint8_t ur;
	uint8_t	crc;
};

stm32_struct_t	*stm32_init(struct port_interface *);
void			stm32_close(stm32_struct_t*);

stm32_t stm32_read_memory(const stm32_struct_t *, uint32_t, uint8_t *, unsigned int);
stm32_t stm32_write_memory(const stm32_struct_t *, uint32_t, const uint8_t *, unsigned int);
stm32_t stm32_wunprot_memory(const stm32_struct_t *);
stm32_t stm32_wprot_memory(const stm32_struct_t *);
stm32_t stm32_erase_memory(const stm32_struct_t *, uint8_t, uint8_t);
stm32_t stm32_go(const stm32_struct_t *, uint32_t);
stm32_t stm32_reset_device(const stm32_struct_t *);
stm32_t stm32_readprot_memory(const stm32_struct_t *);
stm32_t stm32_runprot_memory(const stm32_struct_t *);
stm32_t stm32_crc_memory(const stm32_struct_t *, uint32_t, uint32_t, uint32_t *);
stm32_t stm32_crc_wrapper(const stm32_struct_t *, uint32_t, uint32_t, uint32_t *);
uint32_t stm32_sw_crc(uint32_t, uint8_t *, unsigned int);

#endif
