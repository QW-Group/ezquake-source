/*

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

	$Id: version.h,v 1.7 2007-10-04 15:18:20 johnnycz Exp $
*/
// version.h

#ifndef __VERSION_H__
#define __VERSION_H__

#define	QW_VERSION			"2.40"

#if defined(_WIN32)

#if defined(_WIN64)
	#define QW_PLATFORM			"Win64"
#else
	#define QW_PLATFORM			"Win32"
#endif

#elif defined(__FreeBSD__)
#define QW_PLATFORM			"FreeBSD"

#elif defined(__OpenBSD__)
#define QW_PLATFORM			"OpenBSD"

#elif defined(__NetBSD__)
#define QW_PLATFORM			"NetBSD"

#elif defined(__DragonFly__)
#define QW_PLATFORM			"DragonFly"

#elif defined(__linux__)
	#ifdef __x86_64__
		#define QW_PLATFORM	"Linux64"
	#else
		#define QW_PLATFORM	"Linux32"
	#endif

#elif defined(__sun__)
#define QW_PLATFORM			"SunOS"

#elif defined(__APPLE__)
#define QW_PLATFORM			"MacOSX"

#else
#define QW_PLATFORM			"Unknown"
#endif

#define QW_RENDERER			"GL"

#ifdef _DEBUG
#define QW_CONFIGURATION	"Debug"
#else
#define QW_CONFIGURATION	"Release"
#endif

// Note: for server mods to detect version, change VERSION_NUM below
#define VERSION_NUMBER "3.5-dev-alpha22"

void CL_Version_f(void);
char *VersionString(void);
char *VersionStringColour(void);
char *VersionStringFull(void);

#define SERVER_NAME         "EZQUAKE"
#define VERSION_NUM         3.5

// MVDSV compatibility
#define QWE_VERSION			"0.28"
#define QWE_VERNUM			0.28
#define QWE_SERVER_NAME		"MVDSV"

#endif /* !__VERSION_H__ */
