/*
Copyright (C) 2011 ezQuake team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
//#include "q_shared.h"
#include "common.h"		// Com_DPrintf
#include "textencoding.h"
#include "utils.h"

#define ENCODED_BUFFER_SIZE 1024

// Each encoding function should return the number of bytes written to the output string
//   maxCharacters gives the characters before end of string, if function can't generate in given space, output ? instead
typedef int(*EncodingFunction)(char* output, wchar input, int maxCharacters);

static int koiEncoder(char* output, wchar input, int maxCharacters);
static int utf8Encoder(char* output, wchar input, int maxCharacters);
static int fteEncoder(char* output, wchar input, int maxCharacters);

static const char* encodingStrings[] = { "=`k8:", "=`utf8:", "" };
static const char* encodingSuffixes[] = { "`=", "`=", "" };
static const EncodingFunction encodingFunctions[] = { koiEncoder, utf8Encoder, fteEncoder };

extern cvar_t cl_textEncoding;
extern qbool R_CharAvailable (wchar c);

/* KOI8-R encodes Russian capital hard sign as 0xFF, but we can't use it
because it breaks older clients (qwcl).  We use 0xaf ('/'+ 0x80) instead. */
static char *wc2koi_table =
	"?3??4?67??" "??" "??" ">?"
	"abwgdevzijklmnop"
	"rstufhc~{}/yx|`q"
	"ABWGDEVZIJKLMNOP"
	"RSTUFHC^[]_YX\\@Q"
	"?#??$?&'??" "??" "??.?";

static int koiEncoder(char* out, wchar in, int maxCharacters) {
	if (in <= 255)
		out[0] = in;
	else if (in >= 0x400 && in <= 0x45f)
		out[0] = wc2koi_table[in - 0x400] + 128;
	else
		out[0] = '?';
	return 1;
}

static int utf8Encoder(char* out, wchar in, int maxCharacters) {
	if (in <= 0x7F) {
		// <= 127 is encoded as single byte, no translation
		out[0] = in;
		return 1;
	}
	else if (in <= 0x7FF && maxCharacters >= 2) {
		// Two byte characters... 5 bits then 6
		out[0] = 0xC0 | ((in >> 6) & 0x1F);
		out[1] = 0x80 | (in & 0x3F);
		return 2;
	}
	else if (in <= 0xFFFF && maxCharacters >= 3) {
		// Three byte characters... 4 bits then 6 then 6
		out[0] = 0xE0 | ((in >> 12) & 0x0F);
		out[1] = 0x80 | ((in >> 6) & 0x3F);
		out[2] = 0x80 | (in & 0x3F);
		return 3;
	}
	else {
		// Can't support four characters at the moment - values would be higher than wchar's two bytes
		// If we're here for a character we could support, we don't have room for it...
		out[0] = '?';
		return 1;
	}
}

// FIXME: Should we encode ^ as ^^?  FTE interprets ^^ as ^, but doesn't encode that way.
static int fteEncoder(char* out, wchar in, int maxCharacters) {
	if (in <= 0x7F) {
		// <= 127 is encoded as single byte, no translation
		out[0] = in;
		return 1;
	}

	// Otherwise it is ^UXXXX where XXXX is the codepage in hex - so need an extra 5 characters
	if (maxCharacters < sizeof(wchar) * 2 + 1)
	{
		out[0] = '?';
		return 1;
	}

	snprintf(out, maxCharacters + 1, "^U%04x", in);
	return 6;
}

static int TextEncodingMethod(void)
{
	return bound(0, cl_textEncoding.value, sizeof(encodingFunctions) / sizeof(encodingFunctions[0]) - 1);
}

int TextEncodingEncode(char* out, wchar input, int maxBytes)
{
	return (*encodingFunctions[TextEncodingMethod()])(out, input, maxBytes);
}

const char* TextEncodingPrefix(void)
{
	return encodingStrings[TextEncodingMethod()];
}

const char* TextEncodingSuffix(void)
{
	return encodingSuffixes[TextEncodingMethod()];
}

char *encode_say (wchar *in)
{
	static char buf[ENCODED_BUFFER_SIZE];
	wchar *p;
	char *out;

	memset(buf, 0, sizeof(buf));
	for (p = in; *p; p++)
		if (*p > 255)
			goto encode;
	strlcpy (buf, wcs2str(in), sizeof(buf));
	return buf;
encode:
	strlcpy (buf, wcs2str(in), min(p - in + 1, sizeof(buf)));
	in = p;
	strlcat (buf, TextEncodingPrefix(), sizeof(buf));
	out = buf + strlen(buf);
	
	char* lastValidCharacter = &buf[ENCODED_BUFFER_SIZE - strlen(TextEncodingSuffix()) - 1];	// Leave space for terminator
	while (*in && out <= lastValidCharacter)
	{
		out += TextEncodingEncode(out, *in, lastValidCharacter - out + 1);
		++in;
	}
	strlcat(buf, TextEncodingSuffix(), sizeof(buf));
	return buf;
}

//
// Decoding functions
//

static wchar koi2wc (char c)
{
	static char koi2wc_table[64] = {
		0x4e,0x30,0x31,0x46,0x34,0x35,0x44,0x33,0x45,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,
		0x3f,0x4f,0x40,0x41,0x42,0x43,0x36,0x32,0x4c,0x4b,0x37,0x48,0x4d,0x49,0x47,0x4a,
		0x2e,0x10,0x11,0x26,0x14,0x15,0x24,0x13,0x25,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,
		0x1f,0x2f,0x20,0x21,0x22,0x23,0x16,0x12,0x2c,0x2b,0x17,0x28,0x2d,0x29,0x27,0x2a
	};

	unsigned char uc = c;

	if (uc >= 192 /* && (unsigned char)c <= 255 */)
		return koi2wc_table[(unsigned char)c - 192] + 0x400;
	else if (uc == '#' + 128)
		return 0x0451;	// russian small yo
	else if (uc == '3' + 128)
		return 0x0401;	// russian capital yo
	else if (uc == '4' + 128)
		return 0x0404;	// ukrainian capital round E
	else if (uc == '$' + 128)
		return 0x0454;	// ukrainian small round E
	else if (uc == '6' + 128)
		return 0x0406;	// ukrainian capital I
	else if (uc == '&' + 128)
		return 0x0456;	// ukrainian small i
	else if (uc == '7' + 128)
		return 0x0407;	// ukrainian capital I with two dots
	else if (uc == '\'' + 128)
		return 0x0457;	// ukrainian small i with two dots
	else if (uc == '>' + 128)
		return 0x040e;	// belarusian Y
	else if (uc == '.' + 128)
		return 0x045e;	// belarusian y
	else if (uc == '/' + 128)
		return 0x042a;	// russian capital hard sign
	else
		return (wchar)(unsigned char)c;
}

static wchar cp1251towc (char c)
{
	unsigned char uc = c;
	if (uc >= 192)
		return 0x0410 + (uc - 192);
	else if (uc == 168)
		return 0x0401;	// russian capital yo
	else if (uc == 184)
		return 0x0451;	// russian small yo
	else if (uc == 170)
		return 0x0404;	// ukrainian capital round E
	else if (uc == 186)
		return 0x0454;	// ukrainian small round E
	else if (uc == 178)
		return 0x0406;	// ukrainian capital I
	else if (uc == 179)
		return 0x0456;	// ukrainian small i
	else if (uc == 175)
		return 0x0407;	// ukrainian capital I with two dots
	else if (uc == 191)
		return 0x0457;	// ukrainian small i with two dots
	else if (uc == 161)
		return 0x040e;	// belarusian Y
	else if (uc == 162)
		return 0x045e;	// belarusian y
	return (wchar)uc;
}

// returns Q_malloc'ed data
wchar *decode_koi8q (char *str) {
	wchar *buf, *out;
	buf = out = Q_malloc ((strlen(str) + 1)*sizeof(wchar));
	while (*str)
		*out++ = koi2wc(*str++);
	*out = 0;
	return buf;
};

// returns Q_malloc'ed data
wchar *decode_cp1251 (char *str) {
	wchar *buf, *out;
	buf = out = Q_malloc ((strlen(str) + 1)*sizeof(wchar));
	while (*str)
		*out++ = cp1251towc(*str++);
	*out = 0;
	return buf;
};

wchar TextEncodingDecodeUTF8(char* str, int* index)
{
	int stringLength = strlen(str);
	int extraBytes = 0;
	wchar result = 0;

	if (!(str[*index] & 0x80))
	{
		// Standard ASCII
		result = str[*index];
	}
	else
	{
		char valueMask = 0x7F;
		char lengthBits = (str[*index] << 1);

		// First character is 1<length in set bits><value>
		while (lengthBits & 0x80)
		{
			++extraBytes;
			lengthBits = (lengthBits << 1);

			valueMask >>= 1;
		}

		// Extract value from first character
		result = str[*index] & valueMask;

		while (extraBytes--)
		{
			*index = *index + 1;

			// Extra characters must be 10xx xxxx
			if (*index >= stringLength || (str[*index] & 0xC0) != 0x80)
				return 0;

			result <<= 6;
			result += str[*index] & 0x3F;
		}
	}

	return result;
}

wchar* decode_utf8(char* str) {
	wchar *buf, *out;
	int length = strlen(str);
	int i = 0;

	buf = out = Q_malloc((length + 1)*sizeof(wchar));	// This size would be worst case scenario
	for (i = 0; i < length; ++i)
	{
		*out = TextEncodingDecodeUTF8(str, &i);
		out++;
	}
	*out = 0;
	return buf;
};

typedef wchar *(*decodeFUNC) (char *);

static struct {
	char *name;
	decodeFUNC func;
} decode_table[] = {
	{"koi8q", decode_koi8q},
	{"koi8r", decode_koi8q},
	{"k8", decode_koi8q},
	{"cp1251", decode_cp1251},
	{"wr", decode_cp1251},	// wc = Windows Cyrillic wr = Windows Russian
	{"utf8", decode_utf8},
	{NULL, NULL}
};

qbool ishex(char ch)
{
	return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f');
}

qbool wc_ishex(wchar ch)
{
	return ch < 127 && ishex(ch);
}

int wc_hextodec(wchar ch)
{
	return ch < 127 ? HexToInt(ch) : 0;
}

void decode_fte_string(wchar* s)
{
	while (s[0] && s[1])
	{
		if (s[0] == '^' && s[1] == '^')
		{
			// ^^ => ^
			qwcscpy(s + 1, s + 2);
		}
		else if (s[0] == '^' && s[1] == 'U' && s[2] && s[3] && s[4] && s[5] && wc_ishex(s[2]) && wc_ishex(s[3]) && wc_ishex(s[4]) && wc_ishex(s[5]))
		{
			// ^UXXXX, XXXX is hex for wchar
			s[0] = (wc_hextodec(s[2]) << 12) + (wc_hextodec(s[3]) << 8) + (wc_hextodec(s[4]) << 4) + wc_hextodec(s[5]);

			qwcscpy(s + 1, s + 6);
		}
		else if (s[0] == '^' && s[1] == '{')
		{
			// ^U{XXX...}, XXX.. is hex for wchar
			wchar* num = s + 2;
			wchar result = 0;

			while (*num && wc_ishex(*num))
			{
				result = result * 16 + wc_hextodec(*num);

				++num;
			}

			if (*num == '}')
			{
				if (result)
					s[0] = result;
				else
					s[0] = '?';

				qwcscpy(s + 1, num + 1);
			}
			else
			{
				s[0] = '?';
			}
		}
		++s;
	}
}

wchar *decode_string (const char *s)
{
	static wchar buf[2048];	// should be enough for everyone!!!
	char encoding[13];
	char enc_str[1024];
	const char *p, *q, *r;
	wchar *decoded;
	int i;

	buf[0] = 0;
	p = s;

	while (1)
	{
		p = strstr(p, "=`");
		if (!p)
			break;

		// copy source string up to p as is
		qwcslcat (buf, str2wcs(s), min(p-s+qwcslen(buf)+1, sizeof(buf)/sizeof(buf[0])));
		s = p;

		p += 2;		// skip the =`
		for (q = p; isalnum(*q) && q - p < 12; q++)	{
			;
		}
		if (!*q)
			break;
		if (*q != ':') {
			p += 2;
			continue;
		}

		q++;	// skip the :
		assert (q - p <= sizeof(encoding));
		strlcpy (encoding, p, q - p);
		
		r = strstr(q, "`=");
		if (!r) {
			p = q;
			continue;
		}

		strlcpy (enc_str, q, min(r - q + 1, sizeof(enc_str)));

		for (i = 0; decode_table[i].name; i++) {
			if (!strcasecmp(encoding, decode_table[i].name))
				break;	// found it
		}

		if (!decode_table[i].name) {
			// unknown encoding
			Con_DPrintf("Unknown encoding %s\n", encoding);
			p = r + 2;
			continue;	
		}

		decoded = decode_table[i].func(enc_str);
		qwcslcat (buf, decoded, sizeof(buf)/sizeof(buf[0]));
		Q_free (decoded);
		s = p = r + 2;
	}

	// copy remainder as is
	qwcslcat (buf, str2wcs(s), sizeof(buf)/sizeof(buf[0]));

	// FTE encodes strings differently. Decoded is always smaller, can execute in-place
	decode_fte_string(buf);

	return maybe_transliterate(buf);
}


char cyr_trans_table[] = {"ABVGDEZZIJKLMNOPRSTUFHCCSS'Y'EYY"};
wchar *transliterate_char (wchar c)
{
	static wchar s[2] = {0};

	if (c == 0x401)
		return str2wcs("YO");
	if (c == 0x404)
		return str2wcs("E");
	if (c == 0x406)
		return str2wcs("I");
	if (c == 0x407)
		return str2wcs("J");
	if (c == 0x40e)
		return str2wcs("U");
	if (c == 0x416)
		return str2wcs("ZH");
	if (c == 0x427)
		return str2wcs("CH");
	if (c == 0x428)
		return str2wcs("SH");
	if (c == 0x429)
		return str2wcs("SH");
	if (c == 0x42e)
		return str2wcs("YU");
	if (c == 0x42f)
		return str2wcs("YA");
	if (c == 0x451)
		return str2wcs("yo");
	if (c == 0x454)
		return str2wcs("e");
	if (c == 0x456)
		return str2wcs("i");
	if (c == 0x457)
		return str2wcs("j");
	if (c == 0x45e)
		return str2wcs("u");
	if (c == 0x436)
		return str2wcs("zh");
	if (c == 0x447)
		return str2wcs("ch");
	if (c == 0x448)
		return str2wcs("sh");
	if (c == 0x449)
		return str2wcs("sh");
	if (c == 0x44e)
		return str2wcs("yu");
	if (c == 0x44f)
		return str2wcs("ya");
	if (c >= 0x0410 && c < 0x0430)
		s[0] = cyr_trans_table[c - 0x410];
	else if (c >= 0x0430 && c < 0x0450)
		s[0] = tolower(cyr_trans_table[c - 0x430]);
	else
		s[0] = '?';
	return s;
}

// Make sure the renderer can display all Unicode chars in the string,
// otherwise try to replace them with Latin equivalents
wchar *maybe_transliterate (wchar *src)
{
	wchar *dst, *trans;
	static wchar buf[2048];
#define buflen (sizeof(buf)/sizeof(buf[0]))
	int len;

	dst = buf;
	while (*src && dst < buf+buflen-1) {
		if (R_CharAvailable(*src)) {
			*dst++ = *src;
		}
		else {
			trans = transliterate_char (*src);
			len = min(qwcslen(trans), buf+buflen-1 - dst);
			memcpy (dst, trans, len*sizeof(wchar));
			dst += len;
		}
		src++;
	}
	*dst = 0;

	return buf;
}
