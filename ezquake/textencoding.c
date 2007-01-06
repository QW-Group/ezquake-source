//#include "q_shared.h"
#include "common.h"		// Com_DPrintf
#include "textencoding.h"

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
	strcpy (buf, "=`k8:");
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
		return 0x0404;	// ukranian capital round E
	else if (uc == '$' + 128)
		return 0x0454;	// ukranian small round E
	else if (uc == '6' + 128)
		return 0x0406;	// ukranian capital I
	else if (uc == '&' + 128)
		return 0x0456;	// ukranian small i
	else if (uc == '7' + 128)
		return 0x0407;	// ukranian capital I with two dots
	else if (uc == '\'' + 128)
		return 0x0457;	// ukranian small i with two dots
	else if (uc == '>' + 128)
		return 0x040e;	// belarusian Y
	else if (uc == '.' + 128)
		return 0x045e;	// belarusian y
	else if (uc == '/' + 128)
		return 0x042c;	// russian capital hard sign
	else
		return (wchar)(unsigned char)c;
}


wchar *decode_string (const char *s)
{
	static wchar buf[2048];	// should be enough for everyone!!!

	// this code sucks
	if ((strstr(s, "=`koi8q:") || strstr(s, "=`k8:")) && strstr(s, "`="))
	{
		int i;
		char *p, *p1;
		wchar *out = buf;

//Com_DPrintf ("%s\n", s);
		p = strstr(s, "=`koi8q:");
		p1 = p + strlen("=`koi8q:");
		if (!p) {
			p = strstr(s, "=`k8:");
			p1 = p + strlen("=`k8:");
		}
		for (i = 0; i < p - s; i++)
			*out++ = char2wc(s[i]);
		p = p1;
		while (*p && !(*p == '`' && *(p+1) == '='))
		{
			*out++ = koi2wc(*p);
			p++;
		}
		if (*p) {
			p += 2;
			while (*p)
				*out++ = *p++;
		}
		*out++ = 0;
		return buf;
	}
	return str2wcs(s);
}

