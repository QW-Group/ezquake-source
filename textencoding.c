//#include "q_shared.h"
#include "common.h"		// Com_DPrintf
#include "textencoding.h"

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

static char wc2koi (wchar wc) {
	if (wc <= 127)
		return (char)wc;
	if (wc >= 0x400 && wc <= 0x45f)
		return wc2koi_table[wc - 0x400] + 128;
	else
		return '?';
		
}


char *encode_say (wchar *in)
{
	static char buf[1024];
	wchar *p;
	char *out;

	for (p = in; *p; p++)
		if (*p > 256)
			goto encode;
	strlcpy (buf, wcs2str(in), sizeof(buf));
	return buf;
encode:
	strlcpy (buf, "=`k8:", sizeof (buf));
	out = buf + strlen(buf);
	while (*in && (out - buf < sizeof(buf)/sizeof(buf[0])))
	{
		if (*in <= 255)
			*out++ = *in;
		else {
			*out++ = wc2koi(*in);
		}
		in++;
	}
	*out++ = '`';
	*out++ = '=';
	*out++ = 0;
	return buf;
}

/* koi2wc_table is also used in vid_glx.c */
char koi2wc_table[64] = {
0x4e,0x30,0x31,0x46,0x34,0x35,0x44,0x33,0x45,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,
0x3f,0x4f,0x40,0x41,0x42,0x43,0x36,0x32,0x4c,0x4b,0x37,0x48,0x4d,0x49,0x47,0x4a,
0x2e,0x10,0x11,0x26,0x14,0x15,0x24,0x13,0x25,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,
0x1f,0x2f,0x20,0x21,0x22,0x23,0x16,0x12,0x2c,0x2b,0x17,0x28,0x2d,0x29,0x27,0x2a };

static wchar koi2wc (char c)
{
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
	{NULL, NULL}
};

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
		if (R_CharAvailable(*src))
			*dst++ = *src;
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
