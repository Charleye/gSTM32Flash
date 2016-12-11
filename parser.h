/******************************************************************************
 * parser common
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

#ifndef _H_PARSER
#define _H_PARSER

#include <stdint.h>

typedef enum
{
	PARSER_OK,
	PARSER_ERR_SYSTEM,
	PARSER_ERR_INVALID_FILE,
	PARSER_ERR_WRONLY,
	PARSER_ERR_RDONLY
}parser_t;

typedef struct parser_operation 
{
	const char*		name;
	void*			(*init )();			/* initialise the parser */
	parser_t		(*open )(void *, const char *);	/* open the file for read|write */
	parser_t		(*close)(void *);						/* close and free the parser */
	unsigned int	(*size )(void *);						/* get the total data size */
	parser_t		(*read )(void *, void *, unsigned int *);		/* read a block of data */
	parser_t		(*write)(void *, void *, unsigned int);		/* write a block of data */
}parser_ops_t;

static inline const char* parser_error_to_str(parser_t err) 
{
	switch(err) 
	{
		case PARSER_OK				: return "OK";
		case PARSER_ERR_SYSTEM      : return "System Error";
		case PARSER_ERR_INVALID_FILE: return "Invalid File";
		case PARSER_ERR_WRONLY      : return "Parser can only write";
		case PARSER_ERR_RDONLY      : return "Parser can only read";
		default:
			return "Unknown Error";
	}
}

static inline const char detect_cpu_end ()
{
	const uint32_t cpu_le = 0x12345678;
	return (((const unsigned char *)&cpu_le)[0] == 0x78);
}

static inline const uint32_t be_u32 (const uint32_t v)
{
	if(detect_cpu_end ())
		return ((v & 0xFF000000) >> 24) |
			   ((v & 0x00FF0000) >> 8 ) |
			   ((v & 0x0000FF00) << 8 ) |
			   ((v & 0x000000FF) << 24) ;
	return v;
}

static inline const uint32_t le_u32 (const uint32_t v)
{
	if (!detect_cpu_end ())
		return ((v & 0xFF000000) >> 24) |
			   ((v & 0x00FF0000) >> 8 ) |
			   ((v & 0x0000FF00) << 8 ) |
			   ((v & 0x000000FF) << 24) ;
	return v;
}

#endif
