/*
Copyright (C) 2001-2002       A Nourai

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

	$Id: security.h,v 1.8 2007-09-14 13:29:29 disconn3ct Exp $
*/

#ifndef __SECURITY_H__
#define __SECURITY_H__

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

#define SECURITY_INIT_BAD_CHECKSUM	1
#define SECURITY_INIT_BAD_VERSION	2
#define SECURITY_INIT_ERROR			3
#define SECURITY_INIT_NOPROC		4

typedef struct signed_buffer_s {
	byte *buf;
	unsigned long size;
} signed_buffer_t;

typedef signed_buffer_t *(*Security_Verify_Response_t) (const int, const char*, const char*, const char*);
typedef int (*Security_Init_t) (char *);
typedef signed_buffer_t *(*Security_Generate_Crc_t) (const int, const char*, const char*);
typedef void (*Security_Supported_Binaries_t) (void*);
typedef void (*Security_Shutdown_t) (void);

#endif /* !__SECURITY_H__ */
