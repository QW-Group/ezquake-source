/*
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
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

    $Id: q_shared.h,v 1.36 2007-10-03 14:00:04 dkure Exp $

*/
// q_shared.h -- functions shared by all subsystems

#ifndef __Q_SHARED_H__
#define __Q_SHARED_H__

#include <math.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

#define wchar unsigned short	// 16-bit Unicode char

#undef true
#undef false
typedef enum {false, true} qbool;

#include "mathlib.h"
#include "sys.h"

#if defined(_MSC_VER) | defined (__INTEL_COMPILER)
#define unlink _unlink
#define strdup _strdup
#endif

#ifdef _MSC_VER
#pragma warning( disable : 4244 4127 4201 4214 4514 4305 4115 4018 4996)
#endif

#ifdef __INTEL_COMPILER
#pragma warning( disable : 188)
#endif

typedef unsigned char byte;

#ifndef NULL
#define NULL ((void *) 0)
#endif


#ifdef _WIN32
#define IS_SLASH(c) ((c) == '/' || (c) == '\\')
#else
#define IS_SLASH(c) ((c) == '/')
#endif

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

//#define bound(a,b,c) (max((a), min((b), (c))))
#define bound(a,b,c) ((a) >= (c) ? (a) : \
					(b) < (a) ? (a) : (b) > (c) ? (c) : (b))

#define isspace2(c) ((c) == 0x09 || (c) == 0x0D || (c) == 0x0A || (c) == 0x20)

//============================================================================

#define	MINIMUM_MEMORY		0x550000

#define	MAX_QPATH			64		// max length of a quake game pathname
#define	MAX_OSPATH			260		// max length of a filesystem pathname

#define	ON_EPSILON			0.1		// point on plane side epsilon

//============================================================================

typedef struct sizebuf_s {
	qbool	allowoverflow;	// if false, do a Sys_Error
	qbool	overflowed;		// set to true if the buffer size failed
	byte	*data;
	int		maxsize;
	int		cursize;
} sizebuf_t;

extern char *com_args_original;

void SZ_Init (sizebuf_t *buf, byte *data, int length);
void SZ_InitEx (sizebuf_t *buf, byte *data, int length, qbool allowoverflow);
void SZ_Clear (sizebuf_t *buf);
void *SZ_GetSpace (sizebuf_t *buf, int length);
void SZ_Write (sizebuf_t *buf, const void *data, int length);
void SZ_Print (sizebuf_t *buf, char *data);	// strcats onto the sizebuf

//============================================================================

short	ShortSwap (short l);
int		LongSwap (int l);
float	FloatSwap (float f);
int		LongSwapPDP2Big (int l);
int		LongSwapPDP2Lit (int l);
float	FloatSwapPDP2Big (float f);
float	FloatSwapPDP2Lit (float f);

#if defined(_MSC_VER) && _MSC_VER >= 1400
// vc++ 2005 and up has intrinsics for the BSWAP instruction.
#define ShortSwap _byteswap_ushort
#define LongSwap _byteswap_ulong
#endif

//======================= ENDIAN DECTECTION ==================================
//======================= WIN32 DEFINES ======================================
#ifdef _WIN32
#define __LITTLE_ENDIAN__
#endif

//======================= LINUX DEFINES ======================================
#ifdef __linux__

#if __FLOAT_WORD_ORDER == __BIG_ENDIAN
#define __BIG_ENDIAN__
#elif __FLOAT_WORD_ORDER == __LITTLE_ENDIAN
#define __LITTLE_ENDIAN__
#elif __FLOAT_WORD_ORDER == __PDP_ENDIAN
#define __PDP_ENDIAN__
#endif

#endif

//======================= FreeBSD DEFINES ====================================
#ifdef __FreeBSD__

#include <machine/endian.h>
#if BYTE_ORDER == BIG_ENDIAN
#define __BIG_ENDIAN__
#elif BYTE_ORDER == LITTLE_ENDIAN
#define __LITTLE_ENDIAN__
#elif BYTE_ORDER == PDP_ENDIAN
#define __PDP_ENDIAN__
#endif

#endif

//======================= BYTE SWAPS =========================================
#if defined __BIG_ENDIAN__
#define BigShort(x)		(x)
#define BigLong(x)		(x)
#define BigFloat(x)		(x)
#define LittleShort(x)	ShortSwap(x)
#define LittleLong(x)	LongSwap(x)
#define LittleFloat(x)	FloatSwap(x)

#elif defined __LITTLE_ENDIAN__
#define BigShort(x)		ShortSwap(x)
#define BigLong(x)		LongSwap(x)
#define BigFloat(x)		FloatSwap(x)
#define LittleShort(x)	(x)
#define LittleLong(x)	(x)
#define LittleFloat(x)	(x)

#elif defined __PDP_ENDIAN__
#define BigShort(x)		ShortSwap(x)
#define BigLong(x)		LongSwapPDP2Big(x)
#define BigFloat(x)		FloatSwapPDP2Big(x)
#define LittleShort(x)	(x)
#define LittleLong(x)	LongSwapPDP2Lit(x)
#define LittleFloat(x)	FloatSwapPDP2Lit(x)

#else
#error Unknown byte order type!
#endif

unsigned int BuffBigLong (const unsigned char *buffer);
unsigned short BuffBigShort (const unsigned char *buffer);
unsigned int BuffLittleLong (const unsigned char *buffer);
unsigned short BuffLittleShort (const unsigned char *buffer);
/* johnnycz: VVD's change broke e.g. TGA loading (crosshairimage, tom)
VVD: fixed by changing from "int" to "short" in 2nd string - stupid copy&paste bug :-(
#define	BuffLittleLong(buffer)	LittleLong(*(int*)buffer)
#define	BuffLittleShort(buffer)	LittleShort(*(short*)buffer)
*/

//============================================================================

int Q_atoi (const char *str);
float Q_atof (const char *str);
char *Q_ftos (float value); // removes trailing zero chars

char *Q_strcpy( char *to, char *from );
char *Q_strlwr( char *s1 );

// Added by VVD {
#ifdef _WIN32
#define strcasecmp(s1, s2)	_stricmp  ((s1),   (s2))
#define strncasecmp(s1, s2, n)	_strnicmp ((s1),   (s2),   (n))
// vc++ snprintf and vsnprintf are non-standard and not compatible with C99.
int qsnprintf(char *str, size_t n, char const *fmt, ...);
int qvsnprintf(char *buffer, size_t count, const char *format, va_list argptr);
#define snprintf qsnprintf
#define vsnprintf qvsnprintf
#endif

char *strstri(const char *text, const char *find); // Case insensitive strstr.

#if defined(__linux__) || defined(_WIN32)
size_t strlcpy (char *dst, const char *src, size_t siz);
size_t strlcat (char *dst, const char *src, size_t siz);
char  *strnstr (const char *s, const char *find, size_t slen);
#endif
// Added by VVD }

char *strchrrev(char *str, char chr);
int wildcmp(char *wild, char *string);

wchar char2wc (char c);
char wc2char (wchar wc);
wchar *str2wcs (const char *str);
char *wcs2str (const wchar *ws);

char *wcs2str_malloc (const wchar *ws); // you must freed returned string after it no longer need!!!

#ifdef _WIN32
#define qwcscpy wcscpy
#define qwcschr wcschr
#define qwcsrchr wcsrchr
#define qwcslen wcslen
#define qwcsstr wcsstr
#else
wchar *qwcscpy (wchar *dest, const wchar *src);
wchar *qwcschr (const wchar *ws, wchar wc);
wchar *qwcsrchr (const wchar *ws, wchar wc);
size_t qwcslen (const wchar *s);
wchar *qwcsstr (const wchar *str, const wchar *strSearch);
#endif

// NOTE: size is not the number of bytes to copy, but the number of characters. sizeof(dest) / sizeof(wchar) should be used.
size_t qwcslcpy (wchar *dst, const wchar *src, size_t size);
size_t qwcslcat (wchar *dst, const wchar *src, size_t size);
wchar *Q_wcsdup(const wchar *src);

qbool Q_glob_match (const char *pattern, const char *text);

unsigned int Com_HashKey (const char *name);

//============================================================================

// memory management
void *Q_malloc (size_t size);
void *Q_calloc (size_t n, size_t size);
void *Q_realloc (void *p, size_t newsize);
char *Q_strdup (const char *src);
// might be turned into a function that makes sure all Q_*alloc calls are matched with Q_free
#define Q_free(ptr) if(ptr) { free(ptr); ptr = NULL; }
//============================================================================
// chat icons flags 
// used now by client code only, but may be used in future by server code too, so put here
#define CIF_CHAT  (1<<0) /* set this flag if user in console, mm1, mm2 etc but not in game */
#define CIF_AFK   (1<<1) /* set this flag if app lose focus, ie alt+tab */

//============================================================================

#define	MAX_MSGLEN			1450		// max length of a reliable message
#define	MAX_DATAGRAM		1450		// max length of unreliable message
#define	MSG_BUF_SIZE		8192		// max length of msg buf; MVD demo need it
#define	FILE_TRANSFER_BUF_SIZE	(MAX_MSGLEN - 100)

// qqshka: Its all messy.
// For example ezquake (and FTE?) expect maximum message is MSG_BUF_SIZE == 8192 with mvd header which have not fixed size,
// however fuhquake uses less msg size as I recall.
// mvd header max size is 10 bytes.
// 
// MAX_MVD_SIZE - max size of single mvd message _WITHOUT_ header
#define	MAX_MVD_SIZE			(MSG_BUF_SIZE - 100)

#endif /* __Q_SHARED_H__ */
