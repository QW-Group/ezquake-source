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

	$Id: modules.c,v 1.14 2007-10-17 17:05:07 dkure Exp $
*/

#ifdef __FreeBSD__
#include <dlfcn.h>
#endif

#include "quakedef.h"
#include "modules.h"
#include "version.h"

#if (!defined WITH_PNG_STATIC || !defined WITH_JPEG_STATIC || defined WITH_MP3_PLAYER)
typedef struct registeredModule_s {
	qlib_id_t id;
	qbool loaded;
	qlib_shutdown_fn shutdown;
} registeredModule_t;

char _temp_modulename[MAX_OSPATH];
static registeredModule_t registeredModules[qlib_nummodules];
#endif

#ifdef _WIN32
#define SECURITY_GETFUNC(f) (Security_##f = (Security_##f##_t) GetProcAddress(hSecurity, "Security_" #f))
#else
#define SECURITY_GETFUNC(f) (Security_##f = (Security_##f##_t) dlsym(hSecurity, "Security_" #f))
#endif

Security_Init_t	Security_Init;
Security_Shutdown_t Security_Shutdown;
Security_Supported_Binaries_t Security_Supported_Binaries;

Security_Verify_Response_t Security_Verify_Response;
Security_Generate_Crc_t Security_Generate_Crc;

static qbool security_loaded;

qbool Modules_SecurityLoaded (void)
{
	return security_loaded;
}

qbool VerifyData (signed_buffer_t *p)
{
	return p ? true : false;
}

#ifdef _WIN32
static HINSTANCE hSecurity = NULL;
#else
static void *hSecurity = NULL;
#endif

void Modules_Init (void)
{
	qbool have_security;
	char version_string[128], binary_type[32], *renderer;
	int retval;

	have_security = security_loaded = false;

#ifdef _WIN32
	if (!(hSecurity = LoadLibrary ("ezquake-security.dll"))) {
		Com_Printf_State (PRINT_FAIL, "Security module not found\n");
		goto fail;
	}
#else
	if (!(hSecurity = dlopen ("./ezquake-security.so", RTLD_NOW))) {
		Com_Printf_State (PRINT_FAIL, "Security module not found\n");
		goto fail;
	}
#endif
	have_security = true;

	SECURITY_GETFUNC (Init);
	SECURITY_GETFUNC (Verify_Response);
	SECURITY_GETFUNC (Generate_Crc);
	SECURITY_GETFUNC (Supported_Binaries);
	SECURITY_GETFUNC (Shutdown);

	security_loaded	=	Security_Verify_Response && 
		Security_Generate_Crc && 
		Security_Supported_Binaries &&
		Security_Init &&
		Security_Shutdown;

	if (!security_loaded) {
		Com_Printf_State (PRINT_FAIL, "Security module not initialized\n");
		goto fail;
	}

#ifdef CLIENTONLY
	strlcpy (binary_type, "ezqwcl", sizeof (binary_type));
#else
	strlcpy (binary_type, "ezquake", sizeof (binary_type));
#endif

#if defined (_Soft_X11)
	renderer = "Soft(x11)";
#elif defined (_Soft_SVGA)
	renderer = "Soft(svga)";
#else
	renderer = QW_RENDERER;
#endif
	snprintf (version_string, sizeof (version_string), "%s-p:%s-r:%s-b:%d", binary_type, QW_PLATFORM, renderer, build_number());

	retval = Security_Init (version_string);
	security_loaded = (retval == 0) ? true : false;

	if (!security_loaded) {
		switch (retval) {
			case SECURITY_INIT_ERROR:
				Com_Printf_State (PRINT_FAIL, "Error initializing security module\n");
				break;
			case SECURITY_INIT_BAD_VERSION:
				Com_Printf_State (PRINT_FAIL, "Incompatible client version\n");
				break;
			case SECURITY_INIT_BAD_CHECKSUM:
				Com_Printf_State (PRINT_FAIL, "Modified client binary detected\n");
				break;
			case SECURITY_INIT_NOPROC:
				Com_Printf_State (PRINT_FAIL, "Proc filesystem not found\n");
				break;
		}

		Com_Printf_State (PRINT_FAIL, "Security module not initialized\n");
		goto fail;
	}

	Com_Printf_State (PRINT_OK, "Security module initialized\n");
	return;

fail:
	security_loaded = false;
	if (hSecurity) {
#ifdef _WIN32
		FreeLibrary (hSecurity);
#else
		dlclose (hSecurity);
#endif

		hSecurity = NULL;
	}
}

void Modules_Shutdown(void)
{
	if (Security_Shutdown && Modules_SecurityLoaded())
		Security_Shutdown();

	if (hSecurity) {
#ifdef _WIN32
		FreeLibrary (hSecurity);
#else
		dlclose (hSecurity);
#endif

		hSecurity = NULL;
	}
}

#if (!defined WITH_PNG_STATIC || !defined WITH_JPEG_STATIC || defined WITH_MP3_PLAYER)
void QLib_Init(void)
{
	int i;

	for (i = 0; i < qlib_nummodules; i++) {
		registeredModules[i].id = i;
		registeredModules[i].loaded = false;
		registeredModules[i].shutdown = NULL;
	}
}

void QLib_Shutdown(void)
{
	int i;

	for (i = 0; i < qlib_nummodules; i++) {
		if (registeredModules[i].loaded) {
			registeredModules[i].shutdown();
			registeredModules[i].loaded = false;
		}
	}
}

void QLib_RegisterModule (qlib_id_t module, qlib_shutdown_fn shutdown)
{
	if (module < 0 || module >= qlib_nummodules)
		Sys_Error ("QLib_isModuleLoaded: bad module %d", module);

	registeredModules[module].loaded = true;
	registeredModules[module].shutdown = shutdown;
}

qbool QLib_isModuleLoaded(qlib_id_t module)
{
	if (module < 0 || module >= qlib_nummodules)
		Sys_Error("QLib_isModuleLoaded: bad module %d", module);

	return registeredModules[module].loaded;
}

qbool QLib_ProcessProcdef(QLIB_HANDLETYPE_T handle, qlib_dllfunction_t *procdefs, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		if (!(*procdefs[i].function = QLIB_GETPROCADDRESS(handle, procdefs[i].name))) {
			for (i = 0; i < size; i++)
				procdefs[i].function = NULL;

			return false;
		}
	}

	return true;
}

void QLib_MissingModuleError(int errortype, char *libname, char *cmdline, char *features)
{
	if (!COM_CheckParm("-showliberrors"))
		return;
	switch (errortype) {
	case QLIB_ERROR_MODULE_NOT_FOUND:
		Sys_Error(
			"ezQuake couldn't load the required \"%s" QLIB_LIBRARY_EXTENSION "\" library.  You must\n"
			"specify \"%s\" on the cmdline to disable %s.",
			libname, cmdline, features
			);
		break;
	case QLIB_ERROR_MODULE_MISSING_PROC:
		Sys_Error("Broken \"%s" QLIB_LIBRARY_EXTENSION "\" library - required function missing.", libname);
		break;
	default:
		Sys_Error("QLib_MissingModuleError: unknown error type (%d)", errortype);
	}
}
#endif
