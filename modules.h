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

	$Id: modules.h,v 1.9 2007-10-17 17:05:07 dkure Exp $
*/

#ifndef __MODULES_H__
#define __MODULES_H__

#include "security.h"

#ifndef _WIN32
#include <dlfcn.h>
#endif

#if (!defined WITH_PNG_STATIC || !defined WITH_JPEG_STATIC || defined WITH_MP3_PLAYER)
extern char _temp_modulename[MAX_OSPATH];


#ifdef _WIN32
#define QLIB_HANDLETYPE_T HINSTANCE
#define QLIB_LIBRARY_EXTENSION ".dll"
#define QLIB_LOADLIBRARY(lib)									\
	(																\
	snprintf(_temp_modulename, MAX_OSPATH, "%s.dll", lib),	\
	LoadLibrary(_temp_modulename)								\
	)
#define QLIB_GETPROCADDRESS(lib, func) GetProcAddress(lib, func)
#define QLIB_FREELIBRARY(lib) (FreeLibrary(lib), lib = NULL)
#else
#define QLIB_HANDLETYPE_T void *
#ifdef __APPLE__
#define QLIB_LIBRARY_EXTENSION ".dylib"
#else
#define QLIB_LIBRARY_EXTENSION ".so"
#endif
#define QLIB_LOADLIBRARY(lib)									\
	(																\
	snprintf(_temp_modulename, MAX_OSPATH, "%s.so", lib),	\
	dlopen(_temp_modulename, RTLD_NOW)							\
	)
#define QLIB_GETPROCADDRESS(lib, func) dlsym(lib, func)
#define QLIB_FREELIBRARY(lib) (dlclose(lib), lib = NULL)
#endif

#define QLIB_ERROR_MODULE_NOT_FOUND			-1
#define QLIB_ERROR_MODULE_MISSING_PROC		-2

typedef struct qlib_dllfunction_s {
	char *name;
	void **function;
} qlib_dllfunction_t;

#ifdef SERVERONLY
typedef enum qlib_id_s {qlib_nummodules} qlib_id_t;
#else
typedef enum qlib_id_s {qlib_libpng, qlib_libjpeg, qlib_nummodules} qlib_id_t;
#endif

typedef void (*qlib_shutdown_fn) (void);

void QLib_Init (void);
void QLib_Shutdown (void);
void QLib_RegisterModule (qlib_id_t module, qlib_shutdown_fn shutdown);
qbool QLib_isModuleLoaded (qlib_id_t module);
qbool QLib_ProcessProcdef (QLIB_HANDLETYPE_T handle, qlib_dllfunction_t *procdefs, int size);
void QLib_MissingModuleError (int, char *libname, char *cmdline, char *features);
#endif

qbool Modules_SecurityLoaded (void);
void Modules_Init (void);
qbool VerifyData (signed_buffer_t *p);
void Modules_Shutdown (void);

extern Security_Verify_Response_t Security_Verify_Response;
extern Security_Generate_Crc_t Security_Generate_Crc;

#endif /* !__MODULES_H__ */
