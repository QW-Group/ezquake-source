/*
	version.c

	Build number and version strings

	Copyright (C) 1996-1997 Id Software, Inc.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id: version.c,v 1.14 2007-05-28 10:47:36 johnnycz Exp $
*/

#include "common.h"
#include "version.h"

/*
=======================
CL_Version_f
======================
*/
void CL_Version_f (void)
{
	Com_Printf ("FortressOne %s\n", VersionString());
	Com_Printf ("Exe: "__DATE__" "__TIME__"\n");

#ifdef _DEBUG
	Con_Printf("debug build\n");
#endif

#ifdef __MINGW32__
	Con_Printf("Compiled with MinGW version: %i.%i\n",__MINGW32_MAJOR_VERSION, __MINGW32_MINOR_VERSION);
#endif

#ifdef __CYGWIN__
	Con_Printf("Compiled with Cygwin\n");
#endif

#ifdef __GNUC__
	Con_Printf("Compiled with GCC version: %i.%i.%i (%i)\n",__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__, __VERSION__);

	#ifdef __OPTIMIZE__
		#ifdef __OPTIMIZE_SIZE__
			Con_Printf("GCC Optimization: Optimized for size\n");
		#else
			Con_Printf("GCC Optimization: Optimized for speed\n");
		#endif
	#endif

	#ifdef __NO_INLINE__
		Con_Printf("GCC Optimization: Functions currently not inlined into their callers\n");
	#else
		Con_Printf("GCC Optimization: Functions currently inlined into their callers\n");
	#endif
#endif

#ifdef _M_IX86
	Con_Printf("x86 code optimized for: ");

	if (_M_IX86 == 600) { Con_Printf("Pentium Pro, Pentium II and Pentium III"); }
	else if (_M_IX86 == 500) { Con_Printf("Pentium"); }
	else if (_M_IX86 == 400) { Con_Printf("486"); }
	else if (_M_IX86 == 300) { Con_Printf("386"); }
	else
	{
		Con_Printf("Unknown (%i)\n",_M_IX86);
	}

	Con_Printf("\n");
#endif

#ifdef _M_IX86_FP
	if (_M_IX86_FP == 0) { Con_Printf("SSE & SSE2 instructions disabled\n"); }
	else if (_M_IX86_FP == 1) { Con_Printf("SSE instructions enabled\n"); }
	else if (_M_IX86_FP == 2) { Con_Printf("SSE2 instructions enabled\n"); }
	else
	{
		Con_Printf("Unknown Arch specified: %i\n",_M_IX86_FP);
	}
#endif

}

/*
=======================
VersionString
======================
*/
char *VersionString (void)
{
	static char str[64];

	snprintf (str, sizeof(str), "%s %s", VERSION_NUMBER, VERSION);

	return str;
}

char *VersionStringColour(void)
{
	static char str[64];

	snprintf (str, sizeof(str), "&c1e1%s&r %s", VERSION_NUMBER, VERSION);

	return str;
}

char *VersionStringFull (void)
{
	static char str[256];

	snprintf (str, sizeof(str), SERVER_NAME " %s " "(" QW_PLATFORM ")" "\n", VersionString());

	return str;
}
