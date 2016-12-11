/******************************************************************************
 * stm32.c 
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include "stm32.h"
#include "parser.h"

#define STM32_ACK	0x79
#define STM32_NACK	0x1F
#define STM32_BUSY	0x76

#define STM32_CMD_INIT	0x7F
#define STM32_CMD_GET	0x00	/* get the version and command supported */
#define STM32_CMD_GVR	0x01	/* get version and read protection status */
#define STM32_CMD_GID	0x02	/* get ID */
#define STM32_CMD_RM	0x11	/* read memory */
#define STM32_CMD_GO	0x21	/* go */
#define STM32_CMD_WM	0x31	/* write memory */
#define STM32_CMD_WM_NS	0x32	/* no-stretch write memory */
#define STM32_CMD_ER	0x43	/* erase */
#define STM32_CMD_EE	0x44	/* extended erase */
#define STM32_CMD_EE_NS	0x45	/* extended erase no-stretch */
#define STM32_CMD_WP	0x63	/* write protect */
#define STM32_CMD_WP_NS	0x64	/* write protect no-stretch */
#define STM32_CMD_UW	0x73	/* write unprotect */
#define STM32_CMD_UW_NS	0x74	/* write unprotect no-stretch */
#define STM32_CMD_RP	0x82	/* readout protect */
#define STM32_CMD_RP_NS	0x83	/* readout protect no-stretch */
#define STM32_CMD_UR	0x92	/* readout unprotect */
#define STM32_CMD_UR_NS	0x93	/* readout unprotect no-stretch */
#define STM32_CMD_CRC	0xA1	/* compute CRC */
#define STM32_CMD_ERR	0xFF	/* not a valid command */

#define STM32_RESYNC_TIMEOUT	35	/* seconds */
#define STM32_MASSERASE_TIMEOUT	35	/* seconds */
#define STM32_SECTERASE_TIMEOUT	5	/* seconds */
#define STM32_BLKWRITE_TIMEOUT	1	/* seconds */
#define STM32_WUNPROT_TIMEOUT	1	/* seconds */
#define STM32_WPROT_TIMEOUT		1	/* seconds */
#define STM32_RPROT_TIMEOUT		1	/* seconds */

#define STM32_CMD_GET_LENGTH	17	/* bytes in the reply */


/* Reset code for ARMv7-M (Cortex-M3) and ARMv6-M (Cortex-M0)
 * see ARMv7-M or ARMv6-M Architecture Reference Manual (table B3-8)
 * or "The definitive guide to the ARM Cortex-M3", section 14.4.
 */
static const uint8_t stm_reset_code[] = {
	0x01, 0x49,		            // ldr     r1, [pc, #4] ; (<AIRCR_OFFSET>)
	0x02, 0x4A,		            // ldr     r2, [pc, #8] ; (<AIRCR_RESET_VALUE>)
	0x0A, 0x60,		            // str     r2, [r1, #0]
	0xfe, 0xe7,		            // endless: b endless
	0x0c, 0xed, 0x00, 0xe0,	    // .word 0xe000ed0c <AIRCR_OFFSET> = NVIC AIRCR register address
	0x04, 0x00, 0xfa, 0x05	    // .word 0x05fa0004 <AIRCR_RESET_VALUE> = VECTKEY | SYSRESETREQ
};

static const uint32_t stm_reset_code_length = sizeof(stm_reset_code);

extern const stm32_dev_t devices[];

void stm32_warn_stretching(const char *f)
{
	fprintf(stderr, "Attention !!!\n");
	fprintf(stderr, "\tThis %s error could be caused by your I2C\n", f);
	fprintf(stderr, "\tcontroller not accepting \"clock stretching\"\n");
	fprintf(stderr, "\tas required by bootloader.\n");
	fprintf(stderr, "\tCheck \"I2C.txt\" in stm32flash source code.\n");
}

stm32_t stm32_get_ack_timeout(const stm32_struct_t *stm, time_t timeout)
{
	port_interface_t *port = stm->port;
	uint8_t byte;
	port_t port_err;
	time_t t0, t1;

	if (!(port->flags & PORT_RETRY))
		timeout = 0;

	if (timeout)
		time(&t0);

	do 
	{
		port_err = port->read(port, &byte, 1);
		if (port_err == PORT_ERR_TIMEDOUT && timeout) 
		{
			time(&t1);
			if (t1 < t0 + timeout)
				continue;
		}

		if (port_err != PORT_OK) 
		{
			fprintf(stderr, "Failed to read ACK byte\n");
			return STM32_ERR_UNKNOWN;
		}

		if (byte == STM32_ACK)
			return STM32_OK;
		if (byte == STM32_NACK)
			return STM32_ERR_NACK;
		if (byte != STM32_BUSY) 
		{
			fprintf(stderr, "Got byte 0x%02x instead of ACK\n", byte);
			return STM32_ERR_UNKNOWN;
		}
	} while (1);
}

stm32_t stm32_get_ack(const stm32_struct_t *stm)
{
	return stm32_get_ack_timeout(stm, 0);
}

stm32_t stm32_send_command_timeout(const stm32_struct_t *stm, const uint8_t cmd, time_t timeout)
{
	port_interface_t *port = stm->port;
	stm32_t stm_err;
	port_t port_err;
	uint8_t buf[2];

	buf[0] = cmd;
	buf[1] = cmd ^ 0xFF;
	port_err = port->write(port, buf, 2);
	if (port_err != PORT_OK) 
	{
		fprintf(stderr, "Failed to send command\n");
		return STM32_ERR_UNKNOWN;
	}
	stm_err = stm32_get_ack_timeout(stm, timeout);
	if (stm_err == STM32_OK)
		return STM32_OK;
	if (stm_err == STM32_ERR_NACK)
		fprintf(stderr, "Got NACK from device on command 0x%02x\n", cmd);
	else
		fprintf(stderr, "Unexpected reply from device on command 0x%02x\n", cmd);
	return STM32_ERR_UNKNOWN;
}

stm32_t stm32_send_command(const stm32_struct_t *stm, const uint8_t cmd)
{
	return stm32_send_command_timeout(stm, cmd, 0);
}

void stm32_close (stm32_struct_t *stm)
{
	assert (stm != NULL);
	free (stm -> cmd);
	free (stm);
}

stm32_t stm32_resync(const stm32_struct_t *stm)
{
	port_interface_t *port = stm->port;
	port_t port_err;
	uint8_t buf[2], ack;
	time_t t0, t1;

	time(&t0);
	t1 = t0;

	buf[0] = STM32_CMD_ERR;
	buf[1] = STM32_CMD_ERR ^ 0xFF;
	while (t1 < t0 + STM32_RESYNC_TIMEOUT) 
	{
		if ((port_err = port->write(port, buf, 2)) != PORT_OK)
		{
			usleep(500000);
			time(&t1);
			continue;
		}
		
		if ((port_err = port->read(port, &ack, 1)) != PORT_OK)
		{
			time(&t1);
			continue;
		}
		if (ack == STM32_NACK)
			return STM32_OK;
		time(&t1);
	}
	return STM32_ERR_UNKNOWN;
}

stm32_t stm32_guess_len_cmd(const stm32_struct_t *stm, uint8_t cmd, uint8_t *data, unsigned int len)
{
	port_interface_t *port = stm->port;
	port_t port_err;

	if (stm32_send_command(stm, cmd) != STM32_OK)
		return STM32_ERR_UNKNOWN;
	if (port->flags & PORT_BYTE) 
	{
		/* interface is UART-like */
		if ((port_err = port->read(port, data, 1)) != PORT_OK)
			return STM32_ERR_UNKNOWN;

		len = data[0];
		if ((port_err = port->read(port, data + 1, len + 1)) != PORT_OK)
			return STM32_ERR_UNKNOWN;
		return STM32_OK;
	}

	port_err = port->read(port, data, len + 2);
	if (port_err == PORT_OK && len == data[0])
		return STM32_OK;
	if (port_err != PORT_OK) 
	{
		/* restart with only one byte */
		if (stm32_resync(stm) != STM32_OK)
			return STM32_ERR_UNKNOWN;
		if (stm32_send_command(stm, cmd) != STM32_OK)
			return STM32_ERR_UNKNOWN;
		port_err = port->read(port, data, 1);
		if (port_err != PORT_OK)
			return STM32_ERR_UNKNOWN;
	}

	fprintf(stderr, "Re sync (len = %d)\n", data[0]);
	if (stm32_resync(stm) != STM32_OK)
		return STM32_ERR_UNKNOWN;

	len = data[0];
	if (stm32_send_command(stm, cmd) != STM32_OK)
		return STM32_ERR_UNKNOWN;
	
	port_err = port->read(port, data, len + 2);
	if (port_err != PORT_OK)
		return STM32_ERR_UNKNOWN;
	return STM32_OK;
}

stm32_t stm32_send_init_seq(const stm32_struct_t *stm)
{
	port_interface_t *port = stm->port;
	port_t port_err;
	uint8_t byte, cmd = STM32_CMD_INIT;

	if ((port_err = port->write(port, &cmd, 1)) != PORT_OK)
	{
		fprintf(stderr, "Failed to send init to device\n");
		return STM32_ERR_UNKNOWN;
	}

	port_err = port->read(port, &byte, 1);
	if (port_err == PORT_OK && byte == STM32_ACK)
		return STM32_OK;
	if (port_err == PORT_OK && byte == STM32_NACK) 
	{
		/* We could get error later, but let's continue, for now. */
		fprintf(stderr,	"Warning: the interface was not closed properly.\n");
		return STM32_OK;
	}
	if (port_err != PORT_ERR_TIMEDOUT) 
	{
		fprintf(stderr, "Failed to init device.\n");
		return STM32_ERR_UNKNOWN;
	}

	/*
	 * Check if previous STM32_CMD_INIT was taken as first byte
	 * of a command. Send a new byte, we should get back a NACK.
	 */
	if ((port_err = port->write(port, &cmd, 1)) != PORT_OK)
	{
		fprintf(stderr, "Failed to send init to device\n");
		return STM32_ERR_UNKNOWN;
	}

	port_err = port->read(port, &byte, 1);
	if (port_err == PORT_OK && byte == STM32_NACK)
		return STM32_OK;
	
	fprintf(stderr, "Failed to init device.\n");
	return STM32_ERR_UNKNOWN;
}

/* find newer command by higher code */
#define newer(prev, a) (((prev) == STM32_CMD_ERR) \
			? (a) \
			: (((prev) > (a)) ? (prev) : (a)))

stm32_struct_t* stm32_init (port_interface_t *port)
{
	uint8_t len, val, buf[256];
	stm32_struct_t *stm;
	int i, new_cmds;

	stm = (stm32_struct_t*)calloc (1, sizeof (stm32_struct_t));
	assert (stm != NULL);

	stm -> cmd = (stm32_cmd_t*) malloc (sizeof (stm32_cmd_t));
	assert (stm -> cmd != NULL);
	memset (stm -> cmd, STM32_CMD_ERR, sizeof (stm32_cmd_t));
	stm -> port = port;

	if(port -> flags & PORT_CMD_INIT)
		if ( stm32_send_init_seq (stm) != STM32_OK)
			return NULL;

	if (stm32_send_command (stm, STM32_CMD_GVR) != STM32_OK)
	{
		stm32_close (stm);
		return NULL;
	}

	/* From AN, only UART bootloader returns 3 bytes */
	len = (port->flags & PORT_GVR_ETX) ? 3 : 1;
	if (port->read(port, buf, len) != PORT_OK)
		return NULL;
	stm->version = buf[0];
	stm->option1 = (port->flags & PORT_GVR_ETX) ? buf[1] : 0;
	stm->option2 = (port->flags & PORT_GVR_ETX) ? buf[2] : 0;
	if (stm32_get_ack(stm) != STM32_OK) 
	{
		stm32_close(stm);
		return NULL;
	}

	/* get the bootloader information */
	len = STM32_CMD_GET_LENGTH;
	if (port->cmd_get_reply)
		for (i = 0; port->cmd_get_reply[i].length; i++)
			if (stm->version == port->cmd_get_reply[i].version) 
			{
				len = port->cmd_get_reply[i].length;
				break;
			}

	if (stm32_guess_len_cmd(stm, STM32_CMD_GET, buf, len) != STM32_OK)
		return NULL;
	len = buf[0] + 1;
	stm->bl_version = buf[1];
	new_cmds = 0;

	for (i = 1; i < len; i++) 
	{
		val = buf[i + 1];
		switch (val) 
		{
			case STM32_CMD_GET:
				(stm -> cmd) -> get = val; 
				break;
			case STM32_CMD_GVR:
				(stm -> cmd) -> gvr = val; 
				break;
			case STM32_CMD_GID:
				(stm -> cmd) -> gid = val; 
				break;
			case STM32_CMD_RM:
				(stm -> cmd) -> rm = val; 
				break;
			case STM32_CMD_GO:
				(stm -> cmd) -> go = val; 
				break;
			case STM32_CMD_WM:
			case STM32_CMD_WM_NS:
				(stm -> cmd) -> wm = newer(stm->cmd->wm, val);
				break;
			case STM32_CMD_ER:
			case STM32_CMD_EE:
			case STM32_CMD_EE_NS:
				(stm -> cmd) -> er = newer(stm->cmd->er, val);
				break;
			case STM32_CMD_WP:
			case STM32_CMD_WP_NS:
				(stm -> cmd) -> wp = newer(stm->cmd->wp, val);
				break;
			case STM32_CMD_UW:
			case STM32_CMD_UW_NS:
				(stm -> cmd) -> uw = newer(stm->cmd->uw, val);
				break;
			case STM32_CMD_RP:
			case STM32_CMD_RP_NS:
				(stm -> cmd) -> rp = newer(stm->cmd->rp, val);
				break;
			case STM32_CMD_UR:
			case STM32_CMD_UR_NS:
				(stm -> cmd) -> ur = newer(stm->cmd->ur, val);
				break;
			case STM32_CMD_CRC:
				(stm -> cmd) -> crc = newer(stm->cmd->crc, val);
				break;
			default:
				if (new_cmds++ == 0)
					fprintf(stderr,	"GET returns unknown commands (0x%2x", val);
				else
					fprintf(stderr, ", 0x%2x", val);
		}
	}
	if (new_cmds)	fprintf(stderr, ")\n");

	if (stm32_get_ack(stm) != STM32_OK) 
	{
		stm32_close(stm);
		return NULL;
	}

	if (stm->cmd->get == STM32_CMD_ERR
	    || stm->cmd->gvr == STM32_CMD_ERR
	    || stm->cmd->gid == STM32_CMD_ERR)
	{
		fprintf(stderr, "Error: bootloader did not returned correct information from GET command\n");
		return NULL;
	}

	/* get the device ID */
	if (stm32_guess_len_cmd(stm, stm->cmd->gid, buf, 1) != STM32_OK)
	{
		stm32_close(stm);
		return NULL;
	}

	len = buf[0] + 1;
	if (len < 2)
	{
		stm32_close(stm);
		fprintf(stderr, "Only %d bytes sent in the PID, unknown/unsupported device\n", len);
		return NULL;
	}
	stm->pid = (buf[1] << 8) | buf[2];
	if (len > 2) 
	{
		fprintf(stderr, "This bootloader returns %d extra bytes in PID:", len);
		for (i = 2; i <= len ; i++)
			fprintf(stderr, " %02x", buf[i]);
		fprintf(stderr, "\n");
	}
	if (stm32_get_ack(stm) != STM32_OK) 
	{
		stm32_close(stm);
		return NULL;
	}

	stm->dev = devices;
	while (stm->dev->id != 0x00 && stm->dev->id != stm->pid)
		++stm->dev;

	if (!stm->dev->id) 
	{
		fprintf(stderr, "Unknown/unsupported device (Device ID: 0x%03x)\n", stm->pid);
		stm32_close(stm);
		return NULL;
	}

	return stm;
}

stm32_t stm32_write_memory(const stm32_struct_t *stm, uint32_t address, const uint8_t data[], unsigned int len)
{
	port_interface_t *port = stm->port;
	uint8_t cs, buf[256 + 2];
	unsigned int i, aligned_len;
	stm32_t stm_err;

	if (!len)
		return STM32_OK;

	if (len > 256) 
	{
		fprintf(stderr, "Error: READ length limit at 256 bytes\n");
		return STM32_ERR_UNKNOWN;
	}

	/* must be 32bit aligned */
	if (address & 0x3 || len & 0x3) 
	{
		fprintf(stderr, "Error: WRITE address and length must be 4 byte aligned\n");
		return STM32_ERR_UNKNOWN;
	}

	if (stm->cmd->wm == STM32_CMD_ERR)
	{
		fprintf(stderr, "Error: WRITE command not implemented in bootloader.\n");
		return STM32_ERR_NO_CMD;
	}

	/* send the address and checksum */
	if (stm32_send_command(stm, stm->cmd->wm) != STM32_OK)
		return STM32_ERR_UNKNOWN;

	buf[0] = address >> 24;
	buf[1] = (address >> 16) & 0xFF;
	buf[2] = (address >> 8) & 0xFF;
	buf[3] = address & 0xFF;
	buf[4] = buf[0] ^ buf[1] ^ buf[2] ^ buf[3];
	if (port->write(port, buf, 5) != PORT_OK)
		return STM32_ERR_UNKNOWN;
	if (stm32_get_ack(stm) != STM32_OK)
		return STM32_ERR_UNKNOWN;

	aligned_len = (len + 3) & ~3;
	cs = aligned_len - 1;
	buf[0] = aligned_len - 1;
	for (i = 0; i < len; i++) 
	{
		cs ^= data[i];
		buf[i + 1] = data[i];
	}
	/* padding data */
	for (i = len; i < aligned_len; i++)
	{
		cs ^= 0xFF;
		buf[i + 1] = 0xFF;
	}
	buf[aligned_len + 1] = cs;
	if (port->write(port, buf, aligned_len + 2) != PORT_OK)
		return STM32_ERR_UNKNOWN;

	stm_err = stm32_get_ack_timeout(stm, STM32_BLKWRITE_TIMEOUT);
	if (stm_err != STM32_OK) 
	{
		if (port->flags & PORT_STRETCH_W
		    && stm->cmd->wm != STM32_CMD_WM_NS)
			stm32_warn_stretching("write");
		return STM32_ERR_UNKNOWN;
	}
	return STM32_OK;
}

stm32_t stm32_erase_memory(const stm32_struct_t *stm, uint8_t spage, uint8_t pages)
{
	port_interface_t *port = stm->port;
	stm32_t stm_err;
	port_t port_err;

	if (!pages)
		return STM32_OK;

	if (stm->cmd->er == STM32_CMD_ERR) 
	{
		fprintf(stderr, "Error: ERASE command not implemented in bootloader.\n");
		return STM32_ERR_NO_CMD;
	}

	if (stm32_send_command(stm, stm->cmd->er) != STM32_OK) 
	{
		fprintf(stderr, "Can't initiate chip erase!\n");
		return STM32_ERR_UNKNOWN;
	}

	/* The erase command reported by the bootloader is either 0x43, 0x44 or 0x45 */
	/* 0x44 is Extended Erase, a 2 byte based protocol and needs to be handled differently. */
	/* 0x45 is clock no-stretching version of Extended Erase for I2C port. */
	if (stm->cmd->er != STM32_CMD_ER) 
	{
		/* Not all chips using Extended Erase support mass erase */
		/* Currently known as not supporting mass erase is the Ultra Low Power STM32L15xx range */
		/* So if someone has not overridden the default, but uses one of these chips, take it out of */
		/* mass erase mode, so it will be done page by page. This maximum might not be correct either! */
		if (stm->pid == 0x416 && pages == 0xFF)
			pages = 0xF8; /* works for the STM32L152RB with 128Kb flash */

		if (pages == 0xFF) 
		{
			uint8_t buf[3];

			/* 0xFFFF the magic number for mass erase */
			buf[0] = 0xFF;
			buf[1] = 0xFF;
			buf[2] = 0x00;	/* checksum */
			if (port->write(port, buf, 3) != PORT_OK) 
			{
				fprintf(stderr, "Mass erase error.\n");
				return STM32_ERR_UNKNOWN;
			}
			stm_err = stm32_get_ack_timeout(stm, STM32_MASSERASE_TIMEOUT);
			if (stm_err != STM32_OK) 
			{
				fprintf(stderr, "Mass erase failed. Try specifying the number of pages to be erased.\n");
				if (port->flags & PORT_STRETCH_W
				    && stm->cmd->er != STM32_CMD_EE_NS)
					stm32_warn_stretching("erase");
				return STM32_ERR_UNKNOWN;
			}
			return STM32_OK;
		}

		uint16_t pg_num;
		uint8_t pg_byte;
		uint8_t cs = 0;
		uint8_t *buf;
		int i = 0;

		buf = malloc(2 + 2 * pages + 1);
		if (!buf)
			return STM32_ERR_UNKNOWN;

		/* Number of pages to be erased - 1, two bytes, MSB first */
		pg_byte = (pages - 1) >> 8;
		buf[i++] = pg_byte;
		cs ^= pg_byte;
		pg_byte = (pages - 1) & 0xFF;
		buf[i++] = pg_byte;
		cs ^= pg_byte;

		for (pg_num = spage; pg_num < spage + pages; pg_num++)
		{
			pg_byte = pg_num >> 8;
			cs ^= pg_byte;
			buf[i++] = pg_byte;
			pg_byte = pg_num & 0xFF;
			cs ^= pg_byte;
			buf[i++] = pg_byte;
		}
		buf[i++] = cs;
		port_err = port->write(port, buf, i);
		free(buf);
		if (port_err != PORT_OK)
		{
			fprintf(stderr, "Page-by-page erase error.\n");
			return STM32_ERR_UNKNOWN;
		}

		stm_err = stm32_get_ack_timeout(stm, STM32_SECTERASE_TIMEOUT);
		if (stm_err != STM32_OK) 
		{
			fprintf(stderr, "Page-by-page erase failed. Check the maximum pages your device supports.\n");
			if (port->flags & PORT_STRETCH_W
			    && stm->cmd->er != STM32_CMD_EE_NS)
				stm32_warn_stretching("erase");
			return STM32_ERR_UNKNOWN;
		}

		return STM32_OK;
	}

	/* And now the regular erase (0x43) for all other chips */
	if (pages == 0xFF) 
	{
		stm_err = stm32_send_command_timeout(stm, 0xFF, STM32_MASSERASE_TIMEOUT);
		if (stm_err != STM32_OK)
		{
			if (port->flags & PORT_STRETCH_W)
				stm32_warn_stretching("erase");
			return STM32_ERR_UNKNOWN;
		}
		return STM32_OK;
	} 
	else 
	{
		uint8_t cs = 0;
		uint8_t pg_num;
		uint8_t *buf;
		int i = 0;

		buf = malloc(1 + pages + 1);
		if (!buf)
			return STM32_ERR_UNKNOWN;

		buf[i++] = pages - 1;
		cs ^= (pages-1);
		for (pg_num = spage; pg_num < (pages + spage); pg_num++)
		{
			buf[i++] = pg_num;
			cs ^= pg_num;
		}
		buf[i++] = cs;
		port_err = port->write(port, buf, i);
		free(buf);
		if (port_err != PORT_OK) 
		{
			fprintf(stderr, "Erase failed.\n");
			return STM32_ERR_UNKNOWN;
		}
		stm_err = stm32_get_ack_timeout(stm, STM32_MASSERASE_TIMEOUT);
		if (stm_err != STM32_OK)
		{
			if (port->flags & PORT_STRETCH_W)
				stm32_warn_stretching("erase");
			return STM32_ERR_UNKNOWN;
		}
		return STM32_OK;
	}
}

const stm32_dev_t devices[] = {
	/* F0 */
	{0x440, "STM32F051xx"       , 0x20001000, 0x20002000, 0x08000000, 0x08010000,  4, 1024, 0x1FFFF800, 0x1FFFF80B, 0x1FFFEC00, 0x1FFFF800},
	{0x444, "STM32F030/F031"    , 0x20001000, 0x20002000, 0x08000000, 0x08010000,  4, 1024, 0x1FFFF800, 0x1FFFF80B, 0x1FFFEC00, 0x1FFFF800},
	{0x445, "STM32F042xx"       , 0x20001800, 0x20001800, 0x08000000, 0x08008000,  4, 1024, 0x1FFFF800, 0x1FFFF80F, 0x1FFFC400, 0x1FFFF800},
	{0x448, "STM32F072xx"       , 0x20001800, 0x20004000, 0x08000000, 0x08020000,  2, 2048, 0x1FFFF800, 0x1FFFF80F, 0x1FFFC800, 0x1FFFF800},
	/* F1 */
	{0x412, "Low-density"       , 0x20000200, 0x20002800, 0x08000000, 0x08008000,  4, 1024, 0x1FFFF800, 0x1FFFF80F, 0x1FFFF000, 0x1FFFF800},
	{0x410, "Medium-density"    , 0x20000200, 0x20005000, 0x08000000, 0x08020000,  4, 1024, 0x1FFFF800, 0x1FFFF80F, 0x1FFFF000, 0x1FFFF800},
	{0x414, "High-density"      , 0x20000200, 0x20010000, 0x08000000, 0x08080000,  2, 2048, 0x1FFFF800, 0x1FFFF80F, 0x1FFFF000, 0x1FFFF800},
	{0x420, "Medium-density VL" , 0x20000200, 0x20002000, 0x08000000, 0x08020000,  4, 1024, 0x1FFFF800, 0x1FFFF80F, 0x1FFFF000, 0x1FFFF800},
	{0x428, "High-density VL"   , 0x20000200, 0x20008000, 0x08000000, 0x08080000,  2, 2048, 0x1FFFF800, 0x1FFFF80F, 0x1FFFF000, 0x1FFFF800},
	{0x418, "Connectivity line" , 0x20001000, 0x20010000, 0x08000000, 0x08040000,  2, 2048, 0x1FFFF800, 0x1FFFF80F, 0x1FFFB000, 0x1FFFF800},
	{0x430, "XL-density"        , 0x20000800, 0x20018000, 0x08000000, 0x08100000,  2, 2048, 0x1FFFF800, 0x1FFFF80F, 0x1FFFE000, 0x1FFFF800},
	/* Note that F2 and F4 devices have sectors of different page sizes
           and only the first sectors (of one page size) are included here */
	/* F2 */
	{0x411, "STM32F2xx"         , 0x20002000, 0x20020000, 0x08000000, 0x08100000,  4, 16384, 0x1FFFC000, 0x1FFFC00F, 0x1FFF0000, 0x1FFF77DF},
	/* F3 */
	{0x432, "STM32F373/8"       , 0x20001400, 0x20008000, 0x08000000, 0x08040000,  2, 2048, 0x1FFFF800, 0x1FFFF80F, 0x1FFFD800, 0x1FFFF800},
	{0x422, "F302xB/303xB/358"  , 0x20001400, 0x20010000, 0x08000000, 0x08040000,  2, 2048, 0x1FFFF800, 0x1FFFF80F, 0x1FFFD800, 0x1FFFF800},
	{0x439, "STM32F302x4(6/8)"  , 0x20001800, 0x20004000, 0x08000000, 0x08040000,  2, 2048, 0x1FFFF800, 0x1FFFF80F, 0x1FFFD800, 0x1FFFF800},
	{0x438, "F303x4/334/328"    , 0x20001800, 0x20003000, 0x08000000, 0x08040000,  2, 2048, 0x1FFFF800, 0x1FFFF80F, 0x1FFFD800, 0x1FFFF800},
	/* F4 */
	{0x413, "STM32F40/1"        , 0x20002000, 0x20020000, 0x08000000, 0x08100000,  4, 16384, 0x1FFFC000, 0x1FFFC00F, 0x1FFF0000, 0x1FFF77DF},
	/* 0x419 is also used for STM32F429/39 but with other bootloader ID... */
	{0x419, "STM32F427/37"      , 0x20002000, 0x20030000, 0x08000000, 0x08100000,  4, 16384, 0x1FFFC000, 0x1FFFC00F, 0x1FFF0000, 0x1FFF77FF},
	{0x423, "STM32F401xB(C)"    , 0x20003000, 0x20010000, 0x08000000, 0x08100000,  4, 16384, 0x1FFFC000, 0x1FFFC00F, 0x1FFF0000, 0x1FFF77FF},
	{0x433, "STM32F401xD(E)"    , 0x20003000, 0x20018000, 0x08000000, 0x08100000,  4, 16384, 0x1FFFC000, 0x1FFFC00F, 0x1FFF0000, 0x1FFF77FF},
	/* L0 */
	{0x417, "L05xxx/06xxx"      , 0x20001000, 0x20002000, 0x08000000, 0x08010000, 32,  128, 0x1FF80000, 0x1FF8000F, 0x1FF00000, 0x1FF01000},
	/* L1 */
	{0x416, "L1xxx6(8/B)"       , 0x20000800, 0x20004000, 0x08000000, 0x08020000, 16,  256, 0x1FF80000, 0x1FF8000F, 0x1FF00000, 0x1FF01000},
	{0x429, "L1xxx6(8/B)A"      , 0x20001000, 0x20008000, 0x08000000, 0x08020000, 16,  256, 0x1FF80000, 0x1FF8000F, 0x1FF00000, 0x1FF01000},
	{0x427, "L1xxxC"            , 0x20001000, 0x20008000, 0x08000000, 0x08020000, 16,  256, 0x1FF80000, 0x1FF8000F, 0x1FF00000, 0x1FF02000},
	{0x436, "L1xxxD"            , 0x20001000, 0x2000C000, 0x08000000, 0x08060000, 16,  256, 0x1ff80000, 0x1ff8000F, 0x1FF00000, 0x1FF02000},
	{0x437, "L1xxxE"            , 0x20001000, 0x20014000, 0x08000000, 0x08060000, 16,  256, 0x1ff80000, 0x1ff8000F, 0x1FF00000, 0x1FF02000},
	/* These are not (yet) in AN2606: */
	{0x641, "Medium_Density PL" , 0x20000200, 0x00005000, 0x08000000, 0x08020000,  4, 1024, 0x1FFFF800, 0x1FFFF80F, 0x1FFFF000, 0x1FFFF800},
	{0x9a8, "STM32W-128K"       , 0x20000200, 0x20002000, 0x08000000, 0x08020000,  1, 1024, 0, 0, 0, 0},
	{0x9b0, "STM32W-256K"       , 0x20000200, 0x20004000, 0x08000000, 0x08040000,  1, 2048, 0, 0, 0, 0},
	{0x0}
};
