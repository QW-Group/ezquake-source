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

#if defined(_WIN32)
// Warning: this is different from mvdsv (no 64-bit check)
#if defined(_WIN64)
	#define QW_PLATFORM			"Win64"
#else
	#define QW_PLATFORM			"Win32"
#endif
#define QW_PLATFORM_SHORT	"w"

#elif defined(__FreeBSD__)
#define QW_PLATFORM			"FreeBSD"
#define QW_PLATFORM_SHORT	"f"

#elif defined(__OpenBSD__)
#define QW_PLATFORM			"OpenBSD"
#define QW_PLATFORM_SHORT	"o"

#elif defined(__NetBSD__)
#define QW_PLATFORM			"NetBSD"
#define QW_PLATFORM_SHORT	"n"

#elif defined(__DragonFly__)
#define QW_PLATFORM			"DragonFly"
#define QW_PLATFORM_SHORT	"d"

#elif defined(__linux__)
// Warning: this is different from mvdsv (no 64-bit check)
	#ifdef __x86_64__
		#define QW_PLATFORM	"Linux64"
	#else
		#define QW_PLATFORM	"Linux32"
	#endif
#define QW_PLATFORM_SHORT	"l"

#elif defined(__sun__)
#define QW_PLATFORM			"SunOS"
#define QW_PLATFORM_SHORT	"s"

#elif defined(__APPLE__)
#define QW_PLATFORM			"MacOSX"
#define QW_PLATFORM_SHORT	"m"

#else
#define QW_PLATFORM			"Unknown"
#define QW_PLATFORM_SHORT	"u"
#endif

#define QW_RENDERER			"GL"

#ifdef _DEBUG
#define QW_CONFIGURATION	"Debug"
#else
#define QW_CONFIGURATION	"Release"
#endif

// Note: for server mods to detect version, change VERSION_NUM below
#define VERSION_NUMBER "3.6.6-dev"
#define VERSION_MAX_LEN 32

void CL_Version_f(void);
char *VersionString(void);
char *VersionStringColour(void);
char *VersionStringFull(void);

void VersionCheck_Init(void);
void VersionCheck_Shutdown(void);
qbool VersionCheck_GetLatest(char dest[VERSION_MAX_LEN]);

#ifndef SERVERONLY
#define SERVER_NAME         "EZQUAKE"
#else
#define SERVER_NAME         "MVDSV"
#endif

// MVDSV compatibility
#define	QW_VERSION			"2.40"
#define SERVER_VERSION      "0.34-beta"
#define SERVER_VERSION_NUM  0.33
#define SERVER_FULLNAME     "MVDSV: MultiView Demo SerVer"
#define SERVER_HOME_URL     "https://mvdsv.deurk.net"
#define VERSION_NUM         3.6
#define BUILD_DATE          __DATE__ ", " __TIME__
#define GIT_COMMIT          ""

// ezQuake URLs etc
#define EZ_VERSION_WEBSITE "http://www.ezquake.com/"
#define EZ_MVD_SIGNOFF \
	"\x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f\n" \
	"Recorded by ezQuake (http://www.ezquake.com/)\n" \
	"Discord: http://discord.quake.world/\n" \
	"Forums: http://quakeworld.nu/\n" \
	"\x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f\n"
#define EZ_QWD_SIGNOFF EZ_MVD_SIGNOFF
	// "\x1d\x1e\x1e\x1e\x1e\x1e\x1e Recorded by ezQuake \x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f"

#endif /* !__VERSION_H__ */
