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

    $Id: q_shared.c,v 1.3 2006-05-20 16:50:40 disconn3ct Exp $
*/
// q_shared.c -- functions shared by all subsystems

#include "q_shared.h"

/*
============================================================================
                        LIBRARY REPLACEMENT FUNCTIONS
============================================================================
*/

int Q_atoi (const char *str)
{
	int val, sign, c;

	if (*str == '-') {
		sign = -1;
		str++;
	} else {
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

	if (*str == '-') {
		sign = -1;
		str++;
	} else {
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

// Added by VVD {
#ifdef _WIN32
int snprintf(char *buffer, size_t count, char const *format, ...)
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
int vsnprintf(char *buffer, size_t count, const char *format, va_list argptr)
{
	int ret;
	if (!count) return 0;
	ret = _vsnprintf(buffer, count, format, argptr);
	buffer[count - 1] = 0;
	return ret;
}
#endif

#if defined(__linux__) || defined(_WIN32)
size_t strlcpy(char *dst, char *src, size_t siz)
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
							*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}

size_t strlcat(char *dst, char *src, size_t siz)
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

char *strnstr(char *s, char *find, size_t slen)
{
	char c, sc;
	size_t len;

	if ((c = *find++) != '\0') {
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == '\0' || slen-- < 1)
					return (NULL);
			} while (sc != c);
			if (len > slen)
				return (NULL);
		} while (strncmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}
#endif
// Added by VVD }

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

int Com_HashKey (const char *name) {
	unsigned int v;
	unsigned char c;

	v = 0;
	while ( (c = *name++) != 0 )
		v += c &~ 32;   // make it case insensitive

	return v % 32;
}

/*
============================================================================
                         BYTE ORDER FUNCTIONS
============================================================================
*/

short ShortSwap (short l) {
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
}

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

void SZ_Init (sizebuf_t *buf, byte *data, int length) {
	memset (buf, 0, sizeof(*buf));
	buf->data = data;
	buf->maxsize = length;
}

void SZ_Clear (sizebuf_t *buf) {
	buf->cursize = 0;
	buf->overflowed = false;
}

void *SZ_GetSpace (sizebuf_t *buf, int length) {
	void *data;

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

void SZ_Write (sizebuf_t *buf, void *data, int length) {
	memcpy (SZ_GetSpace(buf,length),data,length);
}

void SZ_Print (sizebuf_t *buf, char *data) {
	int len;

	len = strlen(data) + 1;

	if (!buf->cursize || buf->data[buf->cursize-1])
		memcpy ((byte *)SZ_GetSpace(buf, len),data,len); // no trailing 0
	else
		memcpy ((byte *)SZ_GetSpace(buf, len-1)-1,data,len); // write over trailing 0
}

//============================================================================

/*
** Q_malloc
**
** Use it instead of malloc so that if memory allocation fails,
** the program exits with a message saying there's not enough memory
** instead of crashing after trying to use a NULL pointer
*/
void *Q_malloc (size_t size)
{
	void *p = malloc(size);
	if (!p)
		Sys_Error ("Q_malloc: Not enough memory free; check disk space\n");

	memset(p, 0, size);

	return p;
}

void *Q_calloc (size_t n, size_t size) {
	void *p;

	if (!(p = calloc(n, size)))
		Sys_Error ("Q_calloc: Not enough memory free; check disk space\n");
	return p;
}

char *Q_strdup (const char *src)
{
/* disconnect -->
What the FUCK IS _strdup?!
M$ made their ownr (r) implementation (tm) of strdup (c), but the behavior a bit different to original strdup?
yes. it's M$ way - break standarts and back-compitability :E
	<-- disconnect */
#ifdef _WIN32
	char *p = _strdup(src);
#else
	char *p = strdup(src);
#endif
	if (!p)
		Sys_Error ("Q_strdup: Not enough memory free; check disk space\n");
	return p;
}
