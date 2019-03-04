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
*/

#ifdef __FreeBSD__
#include <dlfcn.h>
#endif

#include "quakedef.h"
#include "modules.h"
#include "version.h"

#if (defined WITH_MP3_PLAYER)
typedef struct registeredModule_s {
	qlib_id_t id;
	qbool loaded;
	qlib_shutdown_fn shutdown;
} registeredModule_t;

char _temp_modulename[MAX_OSPATH];
static registeredModule_t registeredModules[qlib_nummodules];

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
