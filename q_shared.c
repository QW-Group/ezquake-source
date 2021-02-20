/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>

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

    $Id: q_shared.c,v 1.27 2007-09-15 21:00:32 disconn3ct Exp $

*/
// q_shared.c -- functions shared by all subsystems

#include "quakedef.h"
#include "q_shared.h"
#include "r_framestats.h"

/*
============================================================================
                        LIBRARY REPLACEMENT FUNCTIONS
============================================================================
*/

int Q_atoi (const char *str)
{
	int val, sign, c;

	if (!str) return 0;

	for (; *str && *str <= ' '; str++);

	if (*str == '-') {
		sign = -1;
		str++;
	} else {
		if (*str == '+')
			str++;

		sign = 1;
	}

	val = 0;

	// check for hex
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X') ) {
		str += 2;
		while (1) {
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val << 4) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val << 4) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = (val << 4) + c - 'A' + 10;
			else
				return val*sign;
		}
	}

	// check for character
	if (str[0] == '\'')
		return sign * str[1];

	// assume decimal
	while (1) {
		c = *str++;
		if (c <'0' || c > '9')
			return val*sign;
		val = val * 10 + c - '0';
	}

	return 0;
}

float Q_atof (const char *str)
{
	double val;
	int sign, c, decimal, total;

	if (!str) return 0;

	for (; *str && *str <= ' '; str++); // VVD: from MVDSV :-)
/*
	//R00k - qbism// 1999-12-27 ATOF problems with leading spaces fix by Maddes
	while ((*str) && (*str<=' '))
	{
		str++;
	}
	//R00k - qbism// 1999-12-27 ATOF problems with leading spaces fix by Maddes
*/
	if (*str == '-') {
		sign = -1;
		str++;
	} else {
		if (*str == '+')
			str++;

		sign = 1;
	}

	val = 0;

	// check for hex
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X') ) {
		str += 2;
		while (1) {
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val * 16) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val * 16) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = (val * 16) + c - 'A' + 10;
			else
				return val*sign;
		}
	}

	// check for character
	if (str[0] == '\'')
		return sign * str[1];

	// assume decimal
	decimal = -1;
	total = 0;
	while (1) {
		c = *str++;
		if (c == '.') {
			decimal = total;
			continue;
		}
		if (c <'0' || c > '9')
			break;
		val = val*10 + c - '0';
		total++;
	}

	if (decimal == -1)
		return val * sign;
	while (total > decimal) {
		val /= 10;
		total--;
	}

	return val * sign;
}

// removes trailing zeros
char *Q_ftos (float value)
{
	static char str[128];
	int	i;

	snprintf (str, sizeof(str), "%f", value);

	for (i=strlen(str)-1 ; i>0 && str[i]=='0' ; i--)
		str[i] = 0;
	if (str[i] == '.')
		str[i] = 0;

	return str;
}

// like strcpy, but allow overlapping strings
char *Q_strcpy( char *to, char *from )
{
	int i;

	if (to < from) {
		for (i = 0; from[i] != 0; i++) {
			to[i] = from[i];
		}
		to[i] = from[i];
	} else {
		for (i = strlen(from); i >= 0; i--) {
			to[i] = from[i];
		}
	}

	return to;
}

char *Q_strlwr( char *s1 ) {
    char	*s;

    s = s1;
	while ( *s ) {
		*s = tolower(*s);
		s++;
	}
    return s1;
}

// Added by VVD {
#ifdef _WIN32
int qsnprintf(char *buffer, size_t count, char const *format, ...)
{
	int ret;
	va_list argptr;
	if (!count) return 0;
	va_start(argptr, format);
	ret = _vsnprintf(buffer, count, format, argptr);
	buffer[count - 1] = 0;
	va_end(argptr);
	return ret;
}
int qvsnprintf(char *buffer, size_t count, const char *format, va_list argptr)
{
	int ret;
	if (!count) return 0;
	ret = _vsnprintf(buffer, count, format, argptr);
	buffer[count - 1] = 0;
	return ret;
}
#endif

#if defined(__linux__) || defined(_WIN32)

size_t strlcpy(char *dst, const char *src, size_t siz)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0) {
		do {
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0'; /* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}

size_t strlcat(char *dst, const char *src, size_t siz)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;
	size_t dlen;

	/* Find the end of dst and adjust bytes left but don't go past end */
	while (n-- != 0 && *d != '\0')
		d++;
	dlen = d - dst;
	n = siz - dlen;

	if (n == 0)
		return(dlen + strlen(s));
	while (*s != '\0') {
		if (n != 1) {
			*d++ = *s;
			n--;
		}
		s++;
	}
	*d = '\0';

	return(dlen + (s - src));       /* count does not include NUL */
}

char *strnstr(const char *s, const char *find, size_t slen)
{
	char c, sc;
	size_t len;

	if ((c = *find++) != '\0') 
	{
		len = strlen(find);

		do 
		{
			do 
			{
				if ((sc = *s++) == '\0' || slen-- < 1)
					return (NULL);
			} 
			while (sc != c);
			
			if (len > slen)
				return (NULL);

		} while (strncmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}
#endif
// A Case-insensitive strstr.
char *strstri(const char *text, const char *find)
{
	char *s = (char *)text;
	int findlen = strlen(find);

	// Empty substring, return input (like strstr).
	if (findlen == 0)
	{
		return s;
	}

	while (*s)
	{
		// Check if we can find the substring.
		if (!strncasecmp(s, find, findlen))
		{
			return s;
		}

		s++;
	}

	return NULL;
}

// Added by VVD }

//
// Finds the first occurance of a char in a string starting from the end.
// 
char *strchrrev(char *str, char chr)
{
	char *firstchar = str;
	for (str = str + strlen(str)-1; str >= firstchar; str--)
		if (*str == chr)
			return str;

	return NULL;
}

// D-Kure: added for fte vfs
int wildcmp(char *wild, char *string)
{
	char *cp=NULL, *mp=NULL;

	while ((*string) && (*wild != '*'))
	{
		if ((*wild != *string) && (*wild != '?'))
		{
			return 0;
		}
		wild++;
		string++;
	}

	while (*string)
	{
		if (*wild == '*')
		{
			if (!*++wild)   //a * at the end of the wild string matches anything the checked string has
			{
				return 1;
			}
			mp = wild;
			cp = string+1;
		}
		else if ((*wild == *string) || (*wild == '?'))
		{
			wild++;
			string++;
		}
		else
		{
			wild = mp;
			string = cp++;
		}
	}

	while (*wild == '*')
	{
		wild++;
	}
	return !*wild;
}

wchar char2wc (char c)
{
	return (wchar)(unsigned char)c;
}

char wc2char (wchar wc)
{
	return (wc <= 255) ? (char)wc : '?';
}

wchar *str2wcs (const char *s)
{
	static wchar buf[65536]; //ouch! ouch!
	size_t i;

	for (i = 0; i < (sizeof(buf)/sizeof(buf[0])) - 1; i++) {
		if (s[i] == 0)
			break;
		buf[i] = (short)(unsigned char)s[i];
	}
	buf[i] = 0;
	return buf;
}

char *wcs2str (const wchar *ws)
{
	static char buf[65536];	//ouch! ouch!
	size_t i;

	for (i = 0; i < sizeof(buf) - 1; i++) {
		if (ws[i] == 0)
			break;
		buf[i] = ws[i] <= 255 ? (char)ws[i] : '?';
	}
	buf[i] = 0;
	return buf;
}

#ifndef _WIN32
// disconnect: We assume both str and strSearch are NULL-terminated
wchar *qwcsstr (const wchar *str, const wchar *strSearch)
{
	size_t i, j, search;
	size_t str_len;
	size_t strSearch_len;

	str_len = qwcslen (str);
	strSearch_len = qwcslen (strSearch);
	search = 0;

	if (str_len && strSearch_len) { // paranoid check
		for (i = 0; i < str_len - 1; i++) { // -1 because last wchar is NULL
			for (j = 0; j <  strSearch_len - 1; j++) { // -1 because last wchar is NULL
				if (str [j + i] != strSearch[j]) {
					search = 0;
					break;
				} else {
					search = i;
				}
			}

			if (search)
				break;
		}
	}

	return (wchar *)(str + search);
}

size_t qwcslen (const wchar *ws)
{
	size_t i = 0;
	while (*ws++)
		i++;
	return i;
}

wchar *qwcscpy (wchar *dest, const wchar *src)
{
	while (*src)
		*dest++ = *src++;
	*dest = 0;
	return dest;
}
#endif

// NOTE: size is not the number of bytes to copy, but the number of characters. sizeof(dest) / sizeof(wchar) should be used.
size_t qwcslcpy (wchar *dst, const wchar *src, size_t size)
{
	size_t len = qwcslen (src);

	if (len < size) {
		// it'll fit
		memmove(dst, src, (len + 1) * sizeof(wchar));
		return len;
	}

	if (size == 0)
		return len;

	assert (size >= 0);	// if a negative size was passed, then we're fucked

	memcpy (dst, src, (size - 1) * sizeof(wchar));
	dst[size - 1] = 0;

	return len;
}

size_t qwcslcat (wchar *dst, const wchar *src, size_t size)
{
	size_t dstlen = qwcslen (dst);
	size_t srclen = qwcslen (src);
	size_t len = dstlen + srclen;

	if (len < size) {
		// it'll fit
		memcpy (dst + dstlen, src, (srclen + 1) * sizeof(wchar));
		return len;
	}

	if (dstlen >= size - 1)
		return srclen + size;

	if (size == 0)
		return srclen;

	assert (size >= 0);	// if a negative size was passed, then we're fucked

	memcpy (dst + dstlen, src, (size - 1 - dstlen)*sizeof(wchar));
	dst[size - 1] = 0;

	return len;
}

#ifndef qwcschr
wchar *qwcschr (const wchar *ws, wchar wc)
{
	while (*ws) {
		if (*ws == wc)
			return (wchar *)ws;
		ws++;
	}
	return NULL;
}
#endif

#ifndef qwcschr
wchar *qwcsrchr (const wchar *ws, wchar wc)
{
	wchar *p = NULL;
	while (*ws) {
		if (*ws == wc)
			p = (wchar *)ws;
		ws++;
	}
	return p;
}
#endif

wchar *Q_wcsdup (const wchar *src)
{
	wchar *out;
	size_t size = (qwcslen(src) + 1) * sizeof(wchar);
	out = Q_malloc (size);
	memcpy (out, src, size);
	return out;
}


static qbool Q_glob_match_after_star (const char *pattern, const char *text)
{
	char c, c1;
	const char *p = pattern, *t = text;

	while ((c = *p++) == '?' || c == '*') {
		if (c == '?' && *t++ == '\0')
			return false;
	}

	if (c == '\0')
		return true;

	for (c1 = ((c == '\\') ? *p : c); ; ) {
		if (tolower(*t) == c1 && Q_glob_match (p - 1, t))
			return true;
		if (*t++ == '\0')
			return false;
	}
}

/*
Match a pattern against a string.
Based on Vic's Q_WildCmp, which is based on Linux glob_match.
Works like glob_match, except that sets ([]) are not supported.

A match means the entire string TEXT is used up in matching.

In the pattern string, `*' matches any sequence of characters,
`?' matches any character. Any other character in the pattern
must be matched exactly.

To suppress the special syntactic significance of any of `*?\'
and match the character exactly, precede it with a `\'.
*/
qbool Q_glob_match (const char *pattern, const char *text)
{
	char c;

	while ((c = *pattern++) != '\0') {
		switch (c) {
			case '?':
				if (*text++ == '\0')
					return false;
				break;
			case '\\':
				if (tolower(*pattern++) != tolower(*text++))
					return false;
				break;
			case '*':
				return Q_glob_match_after_star(pattern, text);
			default:
				if (tolower(c) != tolower(*text++))
					return false;
		}
	}

	return (*text == '\0');
}

extern void Com_Printf (char *fmt, ...);
unsigned int Com_HashKey (const char *str) {
	unsigned int hash = 0;
	int c;

	if (!str) {
		Com_Printf("warning: Com_HashKey called with NULL argument\n");
		return 0;
	}

	// the (c&~32) makes it case-insensitive
	// hash function known as sdbm, used in gawk
	while ((c = *str++))
        hash = (c &~ 32) + (hash << 6) + (hash << 16) - hash;

    return hash;
}


/*
============================================================================
                         BYTE ORDER FUNCTIONS
============================================================================
*/

/*short ShortSwap (short l) {
	byte b1, b2;

	b1 = l & 255;
	b2 = (l >> 8) & 255;

	return (b1 << 8) + b2;
}

int LongSwap (int l) {
	byte b1, b2, b3, b4;

	b1 = l & 255;
	b2 = (l >> 8) & 255;
	b3 = (l >> 16) & 255;
	b4 = (l >> 24) & 255;

	return ((int) b1 << 24) + ((int) b2 << 16) + ((int) b3 << 8) + b4;
}

float FloatSwap (float f) {
	union {
		float	f;
		byte	b[4];
	} dat1, dat2;

	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}*/

#ifndef ShortSwap
short ShortSwap (short s)
{
	union
	{
		short	s;
		byte	b[2];
	} dat1, dat2;
	dat1.s = s;
	dat2.b[0] = dat1.b[1];
	dat2.b[1] = dat1.b[0];
	return dat2.s;
}
#endif

#ifndef LongSwap
int LongSwap (int l)
{
	union
	{
		int		l;
		byte	b[4];
	} dat1, dat2;
	dat1.l = l;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.l;
}
#endif

float FloatSwap (float f)
{
	union
	{
		float	f;
		byte	b[4];
	} dat1, dat2;
	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

#ifdef __PDP_ENDIAN
int LongSwapPDP2Big (int l)
{
	union
	{
		int		l;
		byte	b[4];
	} dat1, dat2;
	dat1.l = l;
	dat2.b[0] = dat1.b[1];
	dat2.b[1] = dat1.b[0];
	dat2.b[2] = dat1.b[3];
	dat2.b[3] = dat1.b[2];
	return dat2.l;
}

int LongSwapPDP2Lit (int l)
{
	union
	{
		int		l;
		short	s[2];
	} dat1, dat2;
	dat1.l = l;
	dat2.s[0] = dat1.s[1];
	dat2.s[1] = dat1.s[0];
	return dat2.l;
}

float FloatSwapPDP2Big (float f)
{
	union
	{
		float	f;
		byte	b[4];
	} dat1, dat2;
	dat1.f = f;
	dat2.b[0] = dat1.b[1];
	dat2.b[1] = dat1.b[0];
	dat2.b[2] = dat1.b[3];
	dat2.b[3] = dat1.b[2];
	return dat2.f;
}

float FloatSwapPDP2Lit (float f)
{
	union
	{
		float	f;
		short	s[2];
	} dat1, dat2;
	dat1.f = f;
	dat2.s[0] = dat1.s[1];
	dat2.s[1] = dat1.s[0];
	return dat2.f;
}
#endif

// Extract integers from buffers
unsigned int BuffBigLong (const unsigned char *buffer)
{
	return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
}

unsigned short BuffBigShort (const unsigned char *buffer)
{
	return (buffer[0] << 8) | buffer[1];
}

unsigned int BuffLittleLong (const unsigned char *buffer)
{
	return (buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | buffer[0];
}

unsigned short BuffLittleShort (const unsigned char *buffer)
{
	return (buffer[1] << 8) | buffer[0];
}

//===========================================================================

void SZ_InitEx2(sizebuf_t* buf, byte* data, int length, qbool allowoverflow, sizebuf_overflow_handler_func_t overflow_handler)
{
	memset(buf, 0, sizeof(*buf));
	buf->data = data;
	buf->maxsize = length;
	buf->allowoverflow = allowoverflow;
	buf->overflow_handler = overflow_handler;
}

void SZ_InitEx (sizebuf_t *buf, byte *data, int length, qbool allowoverflow)
{
	SZ_InitEx2(buf, data, length, allowoverflow, NULL);
}

void SZ_Init (sizebuf_t *buf, byte *data, int length)
{
	SZ_InitEx (buf, data, length, false);
}

void SZ_Clear (sizebuf_t *buf) {
	buf->cursize = 0;
	buf->overflowed = false;
}

void *SZ_GetSpace(sizebuf_t *buf, int length)
{
	void *data;

	if (buf->cursize + length > buf->maxsize && buf->overflow_handler) {
		buf->overflow_handler(buf, length);
	}

	if (buf->cursize + length > buf->maxsize) {
		if (!buf->allowoverflow)
			Sys_Error ("SZ_GetSpace: overflow without allowoverflow set (%i/%i/%i)", buf->cursize, length, buf->maxsize);

		if (length > buf->maxsize)
			Sys_Error ("SZ_GetSpace: %i/%i is > full buffer size", length, buf->maxsize);

		Sys_Printf ("SZ_GetSpace: overflow\n");	// because Com_Printf may be redirected
		SZ_Clear (buf);
		buf->overflowed = true;
	}

	data = buf->data + buf->cursize;
	buf->cursize += length;

	return data;
}

void SZ_Write(sizebuf_t *buf, const void *data, int length)
{
	byte* dest = SZ_GetSpace(buf, length);

	memcpy(dest, data, length);
}

void SZ_Print(sizebuf_t *buf, char *data)
{
	int len = strlen(data) + 1;

	// Remove trailing '\0'
	if (buf->cursize && !buf->data[buf->cursize - 1]) {
		--buf->cursize;
	}

	SZ_Write(buf, data, len);
}

//============================================================================

#ifdef DEBUG_MEMORY_ALLOCATIONS
static unsigned int allocation_number = 0;

typedef struct ezquake_memory_block_s {
	char filename[MAX_OSPATH];
	char label[64];
	int line_number;
	size_t size;
	unsigned int allocation_number;
} ezquake_memory_block_t;

#define MEMORY_BLOCK_FOR_PTR(ptr) ((ezquake_memory_block_t*) (((intptr_t)ptr) - sizeof(ezquake_memory_block_t)));
#define PTR_FOR_MEMORY_BLOCK(block) (void*)(((intptr_t)block) + sizeof(ezquake_memory_block_t))

static void Q_malloc_register(const char* file, int line)
{
	if (frameStats.hotloop) {
		++frameStats.hotloop_mallocs;
		if (developer.integer) {
			frameStats.hotloop = false;
			Com_Printf("HotMalloc: %s[%d]\n", file, line);
			frameStats.hotloop = true;
		}
	}
	++frameStats.mallocs;
}

#endif

/*
** Q_malloc
**
** Use it instead of malloc so that if memory allocation fails,
** the program exits with a message saying there's not enough memory
** instead of crashing after trying to use a NULL pointer
*/
#ifdef DEBUG_MEMORY_ALLOCATIONS
void* Q_malloc_debug(size_t size, const char* file, int line, const char* label)
#else
void* Q_malloc(size_t size)
#endif
{
#ifdef DEBUG_MEMORY_ALLOCATIONS
	void *p;
	ezquake_memory_block_t* block = malloc(sizeof(ezquake_memory_block_t) + size);

	Sys_Printf("\nmemory,alloc,%u,%s,%d,%u,%s\n", allocation_number, file, line, size, label ? label : "");

	if (!block) {
		Sys_Error("Q_malloc: Not enough memory free; check disk space\n");
		return NULL;
	}

	strlcpy(block->filename, file, sizeof(block->filename));
	if (label) {
		strlcpy(block->label, label, sizeof(block->label));
	}
	block->line_number = line;
	block->size = size;
	block->allocation_number = allocation_number++;

	Q_malloc_register(file, line);

	p = PTR_FOR_MEMORY_BLOCK(block);
#else
	void *p = malloc(size);

	if (!p) {
		Sys_Error("Q_malloc: Not enough memory free; check disk space\n");
	}
#endif

//#ifndef _DEBUG
	memset(p, 0, size);
//#endif

	return p;
}

#ifdef DEBUG_MEMORY_ALLOCATIONS
void *Q_calloc_debug(size_t n, size_t size, const char* file, int line, const char* label)
#else
void *Q_calloc(size_t n, size_t size)
#endif
{
#ifdef DEBUG_MEMORY_ALLOCATIONS
	void *p = Q_malloc_debug(n * size, file, line, label);
#else
	void *p = calloc(n, size);

	if (!p) {
		Sys_Error("Q_calloc: Not enough memory free; check disk space\n");
	}
#endif

	return p;
}

#ifdef DEBUG_MEMORY_ALLOCATIONS
void *Q_realloc_debug(void *p, size_t newsize, const char* file, int line, const char* label)
#else
void *Q_realloc(void *p, size_t newsize)
#endif
{
#ifdef DEBUG_MEMORY_ALLOCATIONS
	ezquake_memory_block_t* block = NULL;

	if (p) {
		block = MEMORY_BLOCK_FOR_PTR(p);
		Sys_Printf("\nmemory,free(realloc),%u,%s,%d,%u,%s,%d\n", block->allocation_number, block->filename, block->line_number, block->size, file, line);
	}
	Sys_Printf("\nmemory,alloc(realloc),%u,%s,%d,%u,%s\n", allocation_number, file, line, newsize, label ? label : "");

	block = p = realloc(block, newsize + sizeof(ezquake_memory_block_t));

	strlcpy(block->filename, file, sizeof(block->filename));
	block->line_number = line;
	block->size = newsize;
	block->allocation_number = allocation_number++;
	if (label) {
		strlcpy(block->label, label, sizeof(block->label));
	}
	Q_malloc_register(file, line);

	return (void*)(((intptr_t)p) + sizeof(ezquake_memory_block_t));
#else
	if (!(p = realloc(p, newsize))) {
		Sys_Error("Q_realloc: Not enough memory free; check disk space\n");
	}

	return p;
#endif
}

#ifdef DEBUG_MEMORY_ALLOCATIONS
void Q_free_debug(void* ptr, const char* file, int line)
{
	if (ptr) {
		ezquake_memory_block_t* block = MEMORY_BLOCK_FOR_PTR(ptr);

		Sys_Printf("\nmemory,free,%u,%s,%d,%u,%s,%d\n", block->allocation_number, block->filename, block->line_number, block->size, file, line);

		free(block);
	}
}

char *Q_strdup_debug(const char *src, const char* file, int line, const char* label)
{
	if (src) {
		size_t size = strlen(src) + 1;
		char *p = Q_malloc_debug(size, file, line, label);

		if (!p) {
			Sys_Error("Q_strdup: Not enough memory free; check disk space\n");
		}

		strlcpy(p, src, size);

		return p;
	}
	return NULL;
}

#else
char *Q_strdup(const char *src)
{
	if (src) {
		char *p = strdup(src);

		if (!p) {
			Sys_Error("Q_strdup: Not enough memory free; check disk space\n");
		}
		return p;
	}
	return NULL;
}
#endif

// PLZ free returned string after it no longer need!!!
#ifdef DEBUG_MEMORY_ALLOCATIONS
char *Q_wcs2str_malloc_debug(const wchar *ws, const char* file, int line)
#else
char *Q_wcs2str_malloc(const wchar *ws)
#endif
{
	size_t i;
	size_t len = qwcslen(ws);
#ifdef DEBUG_MEMORY_ALLOCATIONS
	char *buf = (char *)Q_malloc_debug(len + 1, file, line, NULL);
	Q_malloc_register(file, line);
#else
	char *buf = (char *)Q_malloc(len + 1);
#endif

	for (i = 0; i < len; i++) {
		if (ws[i] == 0)
			break;
		buf[i] = ws[i] <= 255 ? (char)ws[i] : '?';
	}
	buf[i] = 0;
	return buf;
}

#ifdef _WIN64
#undef strlen
int Q_strlen(const char* s)
{
	return (int)strlen(s);
}
#endif

// case insensitive and red-insensitive compare
int Q_strcmp2(const char * s1, const char * s2)
{
	if (s1 == NULL && s2 == NULL)
		return 0;

	if (s1 == NULL)
		return -1;

	if (s2 == NULL)
		return 1;

	while (*s1 || *s2) {
		if (tolower(*s1 & 0x7f) != tolower(*s2 & 0x7f)) {
			return tolower(*s1 & 0x7f) - tolower(*s2 & 0x7f);
		}
		s1++;
		s2++;
	}

	return 0;
}
