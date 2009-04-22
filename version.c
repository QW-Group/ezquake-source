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

static char *date = __DATE__ ;
static char *mon[12] =
{ "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
static char mond[12] =
{ 31,    28,    31,    30,    31,    30,    31,    31,    30,    31,    30,    31 };

//returns days since Dec 21 1999 (the day before q1source release)

int build_number (void) {
	int m = 0;
	int d = 0;
	int y = 0;
	static int b = 0;

	if (b != 0)
		return b;

	for (m = 0; m < 11; m++) {
		if (strncasecmp( &date[0], mon[m], 3 ) == 0)
			break;
		d += mond[m];
	}

	d += atoi( &date[4] ) - 1;
	y = atoi( &date[7] ) - 1900;
	b = d + (int)((y - 1) * 365.25);

	if (((y % 4) == 0) && m > 1)
		b += 1;

	b -= 36148 + 797; // Dec 21 1999 (Feb 25 2002)

	return b;
}


/*
=======================
CL_Version_f
======================
*/
void CL_Version_f (void)
{
	Com_Printf ("ezQuake %s\n", VersionString());
	Com_Printf ("Exe: "__TIME__" "__DATE__"\n");

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

#ifdef _MSC_VER
	if (_MSC_VER == 600) { Con_Printf("C Compiler version 6.0\n"); }
	else if (_MSC_VER == 700) { Con_Printf("C/C++ compiler version 7.0\n"); }
	else if (_MSC_VER == 800) { Con_Printf("Visual C++, Windows, version 1.0 or Visual C++, 32-bit, version 1.0\n"); }
	else if (_MSC_VER == 900) { Con_Printf("Visual C++, Windows, version 2.0 or Visual C++, 32-bit, version 2.x\n"); }
	else if (_MSC_VER == 1000) { Con_Printf("Visual C++, 32-bit, version 4.0\n"); }
	else if (_MSC_VER == 1020) { Con_Printf("Visual C++, 32-bit, version 4.2\n"); }
	else if (_MSC_VER == 1100) { Con_Printf("Visual C++, 32-bit, version 5.0\n"); }
	else if (_MSC_VER == 1200) { Con_Printf("Visual C++, 32-bit, version 6.0\n"); }
	else if (_MSC_VER == 1300) { Con_Printf("Visual C++, version 7.0\n"); }
	else if (_MSC_VER == 1310) { Con_Printf("Visual C++ 2003, version 7.1\n"); }
	else if (_MSC_VER == 1400) { Con_Printf("Visual C++ 2005, version 8.0\n"); }
	else if (_MSC_VER == 1500) { Con_Printf("Visual C++ 2008, version 9.0\n"); }
	else
	{
		Con_Printf("Unknown Microsoft C++ compiler: %i %i \n",_MSC_VER, _MSC_FULL_VER);
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

	snprintf (str, sizeof(str), "%s (build %i)", VERSION_NUMBER, build_number());

	return str;
}
