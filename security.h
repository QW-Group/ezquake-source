/*
Copyright (C) 2001-2002       A Nourai
*/

#ifndef _SECURITY_H

#define _SECURITY_H

#include "quakedef.h"
#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#define strdup	_strdup
#else
#define EXPORT
#endif

#define SECURITY_AUTH_INITIALIZED	1
#define SECURITY_FMOD_INITIALIZED	2

#define SECURITY_INIT_BAD_CHECKSUM	-1
#define SECURITY_INIT_BAD_VERSION	-2
#define SECURITY_INIT_ERROR			-3
#define SECURITY_INIT_NOPROC		-4

typedef struct signed_buffer_s {
	byte *buf;
	unsigned long size;
} signed_buffer_t;

typedef signed_buffer_t *(*Security_Verify_Response_t) (int, unsigned char *);
typedef int (*Security_Init_t) (char *, void **);
typedef signed_buffer_t *(*Security_Generate_Crc_t) (void);
typedef signed_buffer_t *(*Security_IsModelModified_t) (char *, int, byte *, int);
typedef void (*Security_Supported_Binaries_t) (void *);
typedef void (*Security_Shutdown_t) (void);

#endif //_SECURITY_H
