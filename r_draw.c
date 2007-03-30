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

*/

// r_draw.c

#include "quakedef.h"
#ifdef GLQUAKE
#include "gl_model.h"
#include "gl_local.h"
#else
#include "r_model.h"
#include "r_local.h"
#endif
#include "wad.h"
#include "qsound.h"


typedef struct {
	vrect_t	rect;
	int		width;
	int		height;
	byte	*ptexbytes;
	int		rowbytes;
} rectdesc_t;

static rectdesc_t	r_rectdesc;

#define		MAX_CHARSETS 16
int			char_range[MAX_CHARSETS];	// 0x0400, etc; slot 0 is always 0x00
byte		*draw_chars[MAX_CHARSETS];				// 8*8 graphic characters
								// slot 0 is static, the rest are Q_malloc'd
mpic_t		*draw_disc;
mpic_t		*draw_backtile;

cvar_t	scr_conalpha	= {"scr_conalpha", "1"};
cvar_t	scr_menualpha	= {"scr_menualpha", "0.7"};

void customCrosshair_Init(void);

extern cvar_t con_shift;

//=============================================================================
/* Support Routines */

typedef struct cachepic_s {
	char			name[MAX_QPATH];
	cache_user_t	cache;
} cachepic_t;

#define	MAX_CACHED_PICS		128
cachepic_t	cachepics[MAX_CACHED_PICS];
int			numcachepics;

mpic_t *Draw_CacheWadPic (char *name) {
	qpic_t *p;
	mpic_t *pic;

	p = W_GetLumpName (name);
	pic = (mpic_t *)p;
	pic->width = p->width;
	pic->alpha = memchr (&pic->data, 255, pic->width * pic->height) != NULL;

	return pic;
}

static mpic_t *Draw_CachePicBase(char *path, qbool syserror)
{
	cachepic_t *pic;
	int i;
	qpic_t *dat;

	for (pic = cachepics, i = 0; i < numcachepics; pic++, i++)
		if (!strcmp (path, pic->name))
			break;

	if (i == numcachepics) {
		if (numcachepics == MAX_CACHED_PICS)
			Sys_Error ("numcachepics == MAX_CACHED_PICS");
		numcachepics++;
		strcpy (pic->name, path);
	}

	dat = (qpic_t *) Cache_Check (&pic->cache);

	if (dat)
		return (mpic_t *)dat;

	// load the pic from disk
	FS_LoadCacheFile (path, &pic->cache);
	
	dat = (qpic_t *)pic->cache.data;
	if (!dat) {
		if (syserror)
			Sys_Error ("Draw_CachePic: failed to load %s", path);
		else
			return NULL;
	}

	SwapPic (dat);

	((mpic_t *) dat)->width = dat->width;
	((mpic_t *) dat)->alpha = memchr (&dat->data, 255, dat->width * ((mpic_t *)dat)->height) != NULL;

	return (mpic_t *)dat;
}

// load a required picture (if not present, exit with an error)
mpic_t *Draw_CachePic (char *path) {
	return Draw_CachePicBase(path, true);
}

// load an optional picture (if not present, returns null)
mpic_t *Draw_CachePicSafe (char *path, qbool syserror, qbool obsolete)
{
	// 3rd argument is unused because we don't have 24bit pictures support in non-gl rendering
	// and yes, 2nd argument is illogical because calling a function that has 'Safe' in the name
	// we would expect that it automatically presumes not lead to app exit
	return Draw_CachePicBase (path, syserror);
}

// returns Q_malloc'd data, or NULL or error
static byte *LoadAlternateCharset (char *name)
{
	qpic_t *p;
	byte *data;
	
	p = (qpic_t *)FS_LoadTempFile (va("gfx/%s.lmp", name));
	if (!p || fs_filesize != 128*128+8)
		return NULL;
	SwapPic (p);
	if (p->width != 128 || p->height != 128)
		return 0;
	data = Q_malloc (128*128);
	memcpy (data, p->data, 128*128);
	return data;
}


void Draw_Init (void) {
	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register(&scr_conalpha);

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register(&scr_menualpha);

	Cvar_ResetCurrentGroup();

	W_LoadWadFile("gfx.wad"); // safe re-init

	draw_chars[0] = W_GetLumpName ("conchars");
	draw_chars[1] = LoadAlternateCharset ("conchars-cyr");
	if (draw_chars[1])
		char_range[1] = 0x0400;

	draw_disc = Draw_CacheWadPic ("disc");
	draw_backtile = Draw_CacheWadPic ("backtile");

	r_rectdesc.width = draw_backtile->width;
	r_rectdesc.height = draw_backtile->height;
	r_rectdesc.ptexbytes = draw_backtile->data;
	r_rectdesc.rowbytes = draw_backtile->width;

	customCrosshair_Init();
}

qbool R_CharAvailable (wchar num)
{
	int i;

	if (num == (num & 0xff))
		return true;

	for (i = 1; i < MAX_CHARSETS; i++)
		if (char_range[i] == (num & 0xFF00))
			return true;

	return false;
}

//Draws one 8*8 graphics character with 0 being transparent.
//It can be clipped to the top of the screen to allow the console to be smoothly scrolled off.
void Draw_Character (int x, int y, int num) {
	Draw_CharacterW (x, y, char2wc(num));
}

void Draw_CharacterW (int x, int y, wchar num) {
	byte *dest, *source;
	int drawline, row, col, slot;

	if (y <= -8)
		return;			// totally off screen

	if (y > (int) vid.height - 8 || x < 0 || x > vid.width - 8)
		return;

	slot = 0;
	if ((num & 0xFF00) != 0)
	{
		int i;
		for (i = 1; i < MAX_CHARSETS; i++)
			if (char_range[i] == (num & 0xFF00)) {
				slot = i;
				break;
			}
		if (i == MAX_CHARSETS)
			num = '?';
	}

	row = (num >> 4) & 15;
	col = num & 15;
	source = draw_chars[slot] + (row << 10) + (col << 3);

	if (y < 0) {
		// clipped
		drawline = 8 + y;
		source -= 128 * y;
		y = 0;
	} else {
		drawline = 8;
	}

	dest = vid.buffer + y*vid.rowbytes + x;

	while (drawline--) {
		if (source[0])
			dest[0] = source[0];
		if (source[1])
			dest[1] = source[1];
		if (source[2])
			dest[2] = source[2];
		if (source[3])
			dest[3] = source[3];
		if (source[4])
			dest[4] = source[4];
		if (source[5])
			dest[5] = source[5];
		if (source[6])
			dest[6] = source[6];
		if (source[7])
			dest[7] = source[7];
		source += 128;
		dest += vid.rowbytes;
	}
}

void Draw_String (int x, int y, const char *str) {
	while (*str) {
		Draw_Character (x, y, *str);
		str++;
		x += 8;
	}
}

void Draw_StringW (int x, int y, const wchar *str) {
	while (*str) {
		Draw_CharacterW (x, y, *str);
		str++;
		x += 8;
	}
}

void Draw_Alt_String (int x, int y, const char *str) {
	while (*str) {
		Draw_Character (x, y, (*str) | 0x80);
		str++;
		x += 8;
	}
}


int HexToInt(char c) {
	if (isdigit(c))
		return c - '0';
	else if (c >= 'a' && c <= 'f')
		return 10 + c - 'a';
	else if (c >= 'A' && c <= 'F')
		return 10 + c - 'A';
	else
		return -1;
}

void Draw_ColoredString (int x, int y, const char *text, int red) {
	int r, g, b;

	for ( ; *text; text++) {
		if (*text == '&') {
			if (text[1] == 'c' && text[2] && text[3] && text[4])	{
				r = HexToInt(text[2]);
				g = HexToInt(text[3]);
				b = HexToInt(text[4]);
				if (r >= 0 && g >= 0 && b >= 0)
					text += 5;
            } else if (text[1] == 'r')	{
				text += 2;
			}
		}
		Draw_Character (x, y, (*text) | (red ? 0x80 : 0));
		x += 8;
	}
}

const int int_white = 0xFFFFFFFF;

int RGBA_2_Int(byte r, byte g, byte b, byte a) {
	return 0xFFFFFFFF;
}

byte* Int_2_RGBA(int i, byte rgba[4]) {
	memset(rgba, 255, 4);
	return rgba;
}

void Draw_ColoredString3 (int x, int y, const char *text, clrinfo_t *clr, int clr_cnt, int red) {
	Draw_ColoredString(x, y, text, red);
}

void Draw_ColoredString3W (int x, int y, const wchar *text, clrinfo_t *clr, int clr_cnt, int red) {
	Draw_ColoredString(x, y, wcs2str(text), red);
}

void Draw_Pixel(int x, int y, byte color) {
	byte *dest;

	dest = vid.buffer + y * vid.rowbytes + x;
	*dest = color;
}


#define		NUMCROSSHAIRS 6
static qbool crosshairdata[NUMCROSSHAIRS][64] = {
	{
	false, false, false, true,  false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, true,  false, false, false, false,
	true,  false, true,  false, true,  false, true,  false,
	false, false, false, true,  false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, true,  false, false, false, false,
	false, false, false, false, false, false, false, false,
	},
	{
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, true,  false, false, false, false,
	false, false, true,  true,  true,  false, false, false,
	false, false, false, true,  false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	},
	{
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, true,  false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	},
	{
	true,  false, false, false, false, false, false, true,
	false, true,  false, false, false, false, true, false,
	false, false, true,  false, false, true,  false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, true,  false, false, true,  false, false,
	false, true,  false, false, false, false, true,  false,
	true,  false, false, false, false, false, false, true,
	},
	{
	false, false, true,  true,  true,  false, false, false, 
	false, true,  false, true,  false, true,  false, false, 
	true,  true,  false, true,  false, true,  true,  false, 
	true,  true,  true,  true,  true,  true,  true,  false, 
	true,  false, true,  true,  true,  false, true,  false, 
	false, true,  false, false, false, true,  false, false, 
	false, false, true,  true,  true,  false, false, false, 
	false, false, false, false, false, false, false, false,
	},
	{
	false, false, true,  true,  true,  false, false, false, 
	false, false, false, false, false, false, false, false, 
	true,  false, false, false, false, false, true,  false, 
	true,  false, false, true,  false, false, true,  false, 
	true,  false, false, false, false, false, true,  false, 
	false, false, false, false, false, false, false, false, 
	false, false, true,  true,  true,  false, false, false, 
	false, false, false, false, false, false, false, false
	}
};

static qbool customcrosshairdata[64];
static qbool customcrosshair_loaded;
void customCrosshair_Init(void) {
	FILE *f;
	int i = 0, c;

	customcrosshair_loaded = false;
	if (FS_FOpenFile("crosshairs/crosshair.txt", &f) == -1)
		return;

	while (i < 64) {
		c = fgetc(f);
		if (c == EOF) {
			Com_Printf("Invalid format in crosshair.txt (Need 64 X's and O's)\n");
			fclose(f);
			return;
		}
		if (isspace(c))
			continue;
		if (c  != 'x' && c  != 'X' && c  != 'O' && c  != 'o') {
			Com_Printf("Invalid format in crosshair.txt (Only X's and O's and whitespace permitted)\n");
			fclose(f);
			return;
		} else if (c == 'x' || c  == 'X' )
			customcrosshairdata[i++] = true;
		else
			customcrosshairdata[i++] = false;		
	}
	fclose(f);
	customcrosshair_loaded = true;
}

void Draw_Crosshair(void) {
	int x = 0, y = 0, crosshair_val, row, col;
	extern cvar_t crosshair, cl_crossx, cl_crossy, crosshaircolor, crosshairsize;
	extern vrect_t scr_vrect;
	byte c = (byte) crosshaircolor.value;
	qbool *data;

	if (!crosshair.value)
	{
		return;
	}

	// Multiview - No crosshair needs to be drawn.
	if (cls.mvdplayback && cl_multiview.value == 2 && CURRVIEW == 1 && !cl_mvinsetcrosshair.value)
	{
		return;
	}

	// Multiview
	if (cl_multiview.value && cls.mvdplayback) 
	{
		if (cl_multiview.value == 1) 
		{
			x = scr_vrect.x + scr_vrect.width / 2 + cl_crossx.value; 
			y = scr_vrect.y + scr_vrect.height / 2 + cl_crossy.value;
		}

		if (cl_multiview.value == 2) 
		{
			if (!cl_mvinset.value) 
			{
				if (CURRVIEW == 1) 
				{
					x = vid.width / 2; 
					y = vid.height * 3/4;
				} 
				else if (CURRVIEW == 2) 
				{
					// top cv2
					x = vid.width / 2; 
					y = vid.height / 4;
				}
			}
			else
			{
				// inset
				if (CURRVIEW == 2) 
				{
					// normal
					x = vid.width / 2 + cl_crossx.value; 
					if (cl_sbar.value)
					{
						y = vid.height / 2 + cl_crossy.value - sb_lines / 2;
					}
					else
					{
						y = vid.height / 2 + cl_crossy.value / 2;
					}
				}
				else if (CURRVIEW == 1) 
				{
					x = vid.width - (vid.width/3)/2 - 1;
					if (cl_sbar.value)
					{
						y = ((vid.height/3)-sb_lines/3)/2;
					}
					else // no sbar
					{
						y = ((vid.height/3))/2;
					}
				}
			}
		}
		else if (cl_multiview.value == 3) 
		{
			if (CURRVIEW == 2) 
			{
				// top
				x = vid.width / 2;
				y = vid.height / 4;
			}
			else if (CURRVIEW == 3) 
			{
				// bl
				x = vid.width / 4;
				y = vid.height/2 + vid.height/4;
			}
			else 
			{
				// br
				x = vid.width/2 + vid.width/4;
				y = vid.height/2 + vid.height/4;
			}
		}
		else if (cl_multiview.value >= 4) 
		{
			if (CURRVIEW == 2) 
			{
				// tl
				x = vid.width/4;
				y = vid.height/4;
			}
			else if (CURRVIEW == 3) 
			{
				// tr
				x = vid.width/2 + vid.width/4;
				y = vid.height/4;
			}
			else if (CURRVIEW == 4) 
			{
				// bl
				x = vid.width/4;
				y = vid.height/2 + vid.height/4;
			}
			else if (CURRVIEW == 1) 
			{
				// br
				x = vid.width/2 + vid.width/4;
				y = vid.height/2 + vid.height/4;
			}
		}
	}
	else 
	{
		if (cls.mvdplayback) 
		{
			x = vid.width / 2 + cl_crossx.value;
			if (cl_sbar.value)
			{
				y = vid.height / 2 + cl_crossy.value - sb_lines / 2;
			}
			else
			{
				y = scr_vrect.y + scr_vrect.height / 2 + cl_crossy.value / 2;
			}
		}
		else 
		{
			x = scr_vrect.x + scr_vrect.width / 2 + cl_crossx.value; 
			y = scr_vrect.y + scr_vrect.height / 2 + cl_crossy.value;
		}
	}

	crosshair_val = (int) crosshair.value;
	if ((crosshair_val >= 2 && crosshair_val < 2 + NUMCROSSHAIRS) || (crosshair_val == 1 && customcrosshair_loaded)) {
		data = (crosshair_val == 1) ? customcrosshairdata : crosshairdata[crosshair_val - 2];
		for (row = 0; row < 8; row++) {
			for (col = 0; col < 8; col++) {
				if (data[row * 8 + col]) {
					if (crosshairsize.value >= 3) {
						Draw_Pixel(x + 3 * (col - 3) - 1,	y + 3 * (row - 3) - 1,	c);
						Draw_Pixel(x + 3 * (col - 3) - 1,	y + 3 * (row - 3),		c);
						Draw_Pixel(x + 3 * (col - 3) - 1,	y + 3 * (row - 3) + 1,	c);

						Draw_Pixel(x + 3 * (col - 3),		y + 3 * (row - 3) - 1,	c);
						Draw_Pixel(x + 3 * (col - 3),		y + 3 * (row - 3),		c);
						Draw_Pixel(x + 3 * (col - 3),		y + 3 * (row - 3) + 1,	c);

						Draw_Pixel(x + 3 * (col - 3) + 1,	y + 3 * (row - 3) - 1,	c);
						Draw_Pixel(x + 3 * (col - 3) + 1,	y + 3 * (row - 3),		c);
						Draw_Pixel(x + 3 * (col - 3) + 1,	y + 3 * (row - 3) + 1,	c);
					} else if (crosshairsize.value >= 2) {
						Draw_Pixel(x + 2 * (col - 3),		y + 2 * (row - 3),		c);
						Draw_Pixel(x + 2 * (col - 3),		y + 2 * (row - 3) - 1,	c);

						Draw_Pixel(x + 2 * (col - 3) - 1,	y + 2 * (row - 3),		c);
						Draw_Pixel(x + 2 * (col - 3) - 1,	y + 2 * (row - 3) - 1,	c);
					} else {
						Draw_Pixel(x + col - 3, y + row - 3, c);
					}
				}
			}
		}
	} else {
		Draw_Character (x - 4, y - 4, '+');
	}
}



void Draw_TextBox (int x, int y, int width, int lines) {
	mpic_t *p;
	int cx, cy, n;

	// draw left side
	cx = x;
	cy = y;
	p = Draw_CachePic ("gfx/box_tl.lmp");
	Draw_TransPic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_ml.lmp");
	for (n = 0; n < lines; n++) {
		cy += 8;
		Draw_TransPic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_bl.lmp");
	Draw_TransPic (cx, cy+8, p);

	// draw middle
	cx += 8;
	while (width > 0) {
		cy = y;
		p = Draw_CachePic ("gfx/box_tm.lmp");
		Draw_TransPic (cx, cy, p);
		p = Draw_CachePic ("gfx/box_mm.lmp");
		for (n = 0; n < lines; n++) {
			cy += 8;
			if (n == 1)
				p = Draw_CachePic ("gfx/box_mm2.lmp");
			Draw_TransPic (cx, cy, p);
		}
		p = Draw_CachePic ("gfx/box_bm.lmp");
		Draw_TransPic (cx, cy + 8, p);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	p = Draw_CachePic ("gfx/box_tr.lmp");
	Draw_TransPic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_mr.lmp");
	for (n = 0; n < lines; n++) {
		cy += 8;
		Draw_TransPic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_br.lmp");
	Draw_TransPic (cx, cy + 8, p);
}

void Draw_Pic (int x, int y, mpic_t *pic) {
	byte *dest, *source;
	int v;

	if (pic->alpha) {
		Draw_TransPic (x, y, pic);
		return;
	}

	if (x < 0 || x + pic->width > vid.width || y < 0 || y + pic->height > vid.height)
		Sys_Error ("Draw_Pic: bad coordinates");

	source = pic->data;

	dest = vid.buffer + y * vid.rowbytes + x;

	for (v = 0; v < pic->height; v++) {
		memcpy (dest, source, pic->width);
		dest += vid.rowbytes;
		source += pic->width;
	}
}

void Draw_TransSubPic (int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height);

void Draw_SubPic (int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height) {
	byte *dest, *source;
	int v;

	if (pic->alpha) {
		Draw_TransSubPic (x, y, pic, srcx, srcy, width, height);
		return;
	}

	if (x < 0 || x + width > vid.width || y < 0 || y + height > vid.height)
		Sys_Error ("Draw_Pic: bad coordinates");

	source = pic->data + srcy * pic->width + srcx;

	dest = vid.buffer + y * vid.rowbytes + x;

	for (v = 0; v < height; v++) {
		memcpy (dest, source, width);
		dest += vid.rowbytes;
		source += pic->width;
	}
}

void Draw_TransPic (int x, int y, mpic_t *pic) {
	byte *dest, *source, tbyte;
	int v, u;

	if (x < 0 || (unsigned) (x + pic->width) > vid.width || y < 0 || (unsigned)(y + pic->height) > vid.height)
		return; //Sys_Error ("Draw_TransPic: bad coordinates");

	source = pic->data;

	dest = vid.buffer + y * vid.rowbytes + x;

	if (pic->width & 7) {	
		// general
		for (v = 0; v < pic->height; v++) {
			for (u = 0; u < pic->width; u++)
				if ((tbyte = source[u]) != TRANSPARENT_COLOR)
					dest[u] = tbyte;

			dest += vid.rowbytes;
			source += pic->width;
		}
	} else {	// unwound
		for (v = 0; v < pic->height; v++) {
			for (u = 0; u < pic->width; u += 8) {
				if ((tbyte = source[u]) != TRANSPARENT_COLOR)
					dest[u] = tbyte;
				if ((tbyte = source[u + 1]) != TRANSPARENT_COLOR)
					dest[u + 1] = tbyte;
				if ((tbyte = source[u + 2]) != TRANSPARENT_COLOR)
					dest[u + 2] = tbyte;
				if ((tbyte = source[u + 3]) != TRANSPARENT_COLOR)
					dest[u + 3] = tbyte;
				if ((tbyte = source[u + 4]) != TRANSPARENT_COLOR)
					dest[u + 4] = tbyte;
				if ((tbyte = source[u + 5]) != TRANSPARENT_COLOR)
					dest[u + 5] = tbyte;
				if ((tbyte = source[u + 6]) != TRANSPARENT_COLOR)
					dest[u + 6] = tbyte;
				if ((tbyte = source[u + 7]) != TRANSPARENT_COLOR)
					dest[u + 7] = tbyte;
			}
			dest += vid.rowbytes;
			source += pic->width;
		}
	}
}

void Draw_TransSubPic (int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height) {
	byte *dest, *source, tbyte;
	int v, u;

	if (x < 0 || x + width > vid.width || y < 0 || y + height > vid.height)
		Sys_Error ("Draw_Pic: bad coordinates");

	source = pic->data + srcy * pic->width + srcx;

	dest = vid.buffer + y * vid.rowbytes + x;

	if (width & 7) {	
		// general
		for (v = 0; v < height; v++) {
			for (u = 0; u < width; u++)
				if ((tbyte = source[u]) != TRANSPARENT_COLOR)
					dest[u] = tbyte;

			dest += vid.rowbytes;
			source += pic->width;
		}
	} else {	
		// unwound
		for (v = 0; v < height; v++) {
			for (u = 0; u < width; u += 8) {
				if ((tbyte = source[u]) != TRANSPARENT_COLOR)
					dest[u] = tbyte;
				if ((tbyte = source[u + 1]) != TRANSPARENT_COLOR)
					dest[u + 1] = tbyte;
				if ((tbyte = source[u + 2]) != TRANSPARENT_COLOR)
					dest[u + 2] = tbyte;
				if ((tbyte = source[u + 3]) != TRANSPARENT_COLOR)
					dest[u + 3] = tbyte;
				if ((tbyte = source[u + 4]) != TRANSPARENT_COLOR)
					dest[u + 4] = tbyte;
				if ((tbyte = source[u + 5]) != TRANSPARENT_COLOR)
					dest[u + 5] = tbyte;
				if ((tbyte = source[u + 6]) != TRANSPARENT_COLOR)
					dest[u + 6] = tbyte;
				if ((tbyte = source[u + 7]) != TRANSPARENT_COLOR)
					dest[u + 7] = tbyte;
			}
			dest += vid.rowbytes;
			source += pic->width;
		}
	}
}

void Draw_TransPicTranslate (int x, int y, mpic_t *pic, byte *translation) {
	byte *dest, *source, tbyte;
	int v, u;

	if (x < 0 || (unsigned)(x + pic->width) > vid.width || y < 0 || (unsigned) (y + pic->height) > vid.height)
		Sys_Error ("Draw_TransPic: bad coordinates");

	source = pic->data;

	dest = vid.buffer + y * vid.rowbytes + x;

	if (pic->width & 7) {	
		// general
		for (v = 0; v < pic->height; v++) {
			for (u = 0; u < pic->width; u++)
				if ((tbyte = source[u]) != TRANSPARENT_COLOR)
					dest[u] = translation[tbyte];

			dest += vid.rowbytes;
			source += pic->width;
		}
	} else {	// unwound
		for (v = 0; v < pic->height; v++) {
			for (u = 0; u < pic->width; u += 8) {
				if ((tbyte = source[u]) != TRANSPARENT_COLOR)
					dest[u] = translation[tbyte];
				if ((tbyte = source[u + 1]) != TRANSPARENT_COLOR)
					dest[u + 1] = translation[tbyte];
				if ((tbyte = source[u + 2]) != TRANSPARENT_COLOR)
					dest[u + 2] = translation[tbyte];
				if ((tbyte = source[u + 3]) != TRANSPARENT_COLOR)
					dest[u + 3] = translation[tbyte];
				if ((tbyte = source[u + 4]) != TRANSPARENT_COLOR)
					dest[u + 4] = translation[tbyte];
				if ((tbyte = source[u + 5]) != TRANSPARENT_COLOR)
					dest[u + 5] = translation[tbyte];
				if ((tbyte = source[u + 6]) != TRANSPARENT_COLOR)
					dest[u + 6] = translation[tbyte];
				if ((tbyte = source[u + 7]) != TRANSPARENT_COLOR)
					dest[u + 7] = translation[tbyte];
			}
			dest += vid.rowbytes;
			source += pic->width;
		}
	}
}

void Draw_CharToConback (int num, byte *dest) {
	int	 row, col, drawline, x;
	byte *source;

	row = num >> 4;
	col = num & 15;
	source = draw_chars[0] + (row<<10) + (col<<3);

	drawline = 8;

	while (drawline--) {
		for (x = 0; x < 8; x++)
			if (source[x])
				dest[x] = source[x];
		source += 128;
		dest += 320;
	}
}

void Draw_ConsoleBackground (int lines) {
	int x, y, v, f, fstep;
	byte *src, *dest;
	mpic_t *conback;
	static char saveback[320 * 8];

	if (!scr_conalpha.value && !SCR_NEED_CONSOLE_BACKGROUND)
		return;

	conback = Draw_CachePic ("gfx/conback.lmp");

	// hack the version number directly into the pic

	memcpy (saveback, conback->data + 320 * 186, 320 * 8);

	// draw the pic
	dest = vid.buffer;

	for (y = 0; y < lines; y++, dest += vid.rowbytes) {
		v = (vid.conheight - lines + y + con_shift.value) * 200 / vid.conheight;
		src = conback->data + v * 320;
		if (vid.conwidth == 320) {
			memcpy (dest, src, vid.conwidth);
		} else {
			f = 0;
			fstep = 320 * 0x10000 / vid.conwidth;
			for (x = 0; x < vid.conwidth; x += 4) {
				dest[x] = src[f >> 16];
				f += fstep;
				dest[x + 1] = src[f >> 16];
				f += fstep;
				dest[x + 2] = src[f >> 16];
				f += fstep;
				dest[x + 3] = src[f >> 16];
				f += fstep;
			}
		}
	}
	// put it back
	memcpy(conback->data + 320 * 186, saveback, 320 * 8);
}

void R_DrawRect8 (vrect_t *prect, int rowbytes, byte *psrc, int transparent) {
	byte t;
	int i, j, srcdelta, destdelta;
	byte *pdest;

	pdest = vid.buffer + (prect->y * vid.rowbytes) + prect->x;

	srcdelta = rowbytes - prect->width;
	destdelta = vid.rowbytes - prect->width;

	if (transparent) {
		for (i = 0; i < prect->height; i++) {
			for (j = 0; j < prect->width; j++) {
				t = *psrc;
				if (t != TRANSPARENT_COLOR)
					*pdest = t;

				psrc++;
				pdest++;
			}

			psrc += srcdelta;
			pdest += destdelta;
		}
	} else {
		for (i = 0; i < prect->height; i++) {
			memcpy (pdest, psrc, prect->width);
			psrc += rowbytes;
			pdest += vid.rowbytes;
		}
	}
}

//This repeats a 64*64 tile graphic to fill the screen around a sized down refresh window.
void Draw_TileClear (int x, int y, int w, int h) {
	int width, height, tileoffsetx, tileoffsety;
	byte *psrc;
	vrect_t vr;

	r_rectdesc.rect.x = x;
	r_rectdesc.rect.y = y;
	r_rectdesc.rect.width = w;
	r_rectdesc.rect.height = h;

	vr.y = r_rectdesc.rect.y;
	height = r_rectdesc.rect.height;

	tileoffsety = vr.y % r_rectdesc.height;

	while (height > 0) {
		vr.x = r_rectdesc.rect.x;
		width = r_rectdesc.rect.width;

		vr.height = r_rectdesc.height - tileoffsety;

		if (vr.height > height)
			vr.height = height;

		tileoffsetx = vr.x % r_rectdesc.width;

		while (width > 0) {
			vr.width = r_rectdesc.width - tileoffsetx;

			if (vr.width > width)
				vr.width = width;

			psrc = r_rectdesc.ptexbytes + (tileoffsety * r_rectdesc.rowbytes) + tileoffsetx;

			R_DrawRect8 (&vr, r_rectdesc.rowbytes, psrc, 0);

			vr.x += vr.width;
			width -= vr.width;
			tileoffsetx = 0;	// only the left tile can be left-clipped
		}

		vr.y += vr.height;
		height -= vr.height;
		tileoffsety = 0;		// only the top tile can be top-clipped
	}
}

//Fills a box of pixels with a single color
void Draw_Fill (int x, int y, int w, int h, int c) 
{
	byte *dest;
	int u, v;

	if (!(cls.mvdplayback && cl_multiview.value == 2)) 
	{ 
		if (x < 0 || x + w > vid.width || y < 0 || y + h > vid.height) 
		{
			//Com_Printf ("Bad Draw_Fill(%d, %d, %d, %d, %c)\n", x, y, w, h, c);
			return;
		}
	}

	dest = vid.buffer + y*vid.rowbytes + x;
	for (v = 0; v < h; v++, dest += vid.rowbytes)
		for (u = 0; u < w; u++)
			dest[u] = c;
}

//=============================================================================

// HUD -> hexum
#define clamp(a,b,c) (a = min(max(a, b), c))
/*
================
Draw_FadeBox

================
*/
void Draw_FadeBox (int x, int y, int width, int height, byte color, float a)
//                   float r, float g, float b, float a)
{
    int         _x, _y;
    byte        *pbuf;
    //int         col_index;
    //byte        color;

    if (a <= 0)
        return;

    VID_UnlockBuffer ();
    S_ExtraUpdate ();
    VID_LockBuffer ();

	/*
    // find color
    clamp(r, 0, 1);
    clamp(g, 0, 1);
    clamp(b, 0, 1);
    col_index = ((int)(r*63) << 12) +
                ((int)(g*63) <<  6) +
                ((int)(b*63) <<  0);
    color = 0; // color = d_15to8table[col_index];
	*/

    for (_y=y; _y < y + height; _y++)
    {
        int t;

        pbuf = (byte *)(vid.buffer + vid.rowbytes*_y);
        if (a < 0.333)
        {
            t = (_y & 1) << 1;

            for (_x = x; _x < x + width; _x++)
            {
                if ((_x & 3) == t)
                    pbuf[_x] = color;
            }
        }
        else if (a < 0.666)
        {
            t = (_y & 1);

            for (_x = x; _x < x + width; _x++)
            {
                if ((_x & 1) == t)
                    pbuf[_x] = color;
            }
        }
        else if (a < 1)
        {
            t = (_y & 1) << 1;

            for (_x = x; _x < x + width; _x++)
            {
                if ((_x & 3) != t)
                    pbuf[_x] = color;
            }
        }
        else
        {
            for (_x = x; _x < x + width; _x++)
                pbuf[_x] = color;
        }
    }

    VID_UnlockBuffer ();
    S_ExtraUpdate ();
    VID_LockBuffer ();
}

void Draw_FadeScreen (void) {
	int x,y;
	byte *pbuf;
	float alpha;

	alpha = bound(0, scr_menualpha.value, 1);

	if (!alpha)
		return;

	VID_UnlockBuffer ();
	S_ExtraUpdate ();
	VID_LockBuffer ();

	for (y = 0; y < vid.height; y++) {
		int	t;

		pbuf = (byte *) (vid.buffer + vid.rowbytes * y);

        if (alpha < 1 / 3.0) {
            t = (y & 1) << 1;

            for (x = 0; x < vid.width; x++) {
                if ((x & 3) == t)
                    pbuf[x] = 0;
            }
		} else if (alpha < 2 / 3.0) {
            t = (y & 1) << 1;

            for (x = 0; x < vid.width; x++) {
                if ((x & 1) == t)
                    pbuf[x] = 0;
            }
		} else if (alpha < 1) {
			t = (y & 1) << 1;

			for (x = 0; x < vid.width; x++) {
				if ((x & 3) != t)
					pbuf[x] = 0;
			}
		} else {            
			for (x = 0; x < vid.width; x++)
                pbuf[x] = 0;
		}
	}

	VID_UnlockBuffer ();
	S_ExtraUpdate ();
	VID_LockBuffer ();
}

// HUD -> hexum
// kazik -->
//
// SCALE versions of some functions
//

void Draw_SCharacter (int x, int y, int num, float scale)
{
    // no scale in SOFT yet..
    Draw_Character(x, y, num);
}

void Draw_SString (int x, int y, char *str, float scale)
{
    // no scale in SOFT yet..
    Draw_String(x, y, str);
}

void Draw_SAlt_String (int x, int y, char *str, float scale)
{
    // no scale in SOFT yet..
    Draw_Alt_String(x, y, str);
}

void Draw_SPic (int x, int y, mpic_t *pic, float scale)
{
    // no scale in SOFT yet..
    // but mae it transparent
    Draw_TransPic(x, y, pic);
}

void Draw_FitPic (int x, int y, int fit_width, int fit_height, mpic_t *gl)
{
    // no scale in SOFT yet...
    Draw_TransPic(x, y, gl);
}

void Draw_SSubPic(int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height, float scale)
{
    // no scale in SOFT yet..
    // but make it transparent
    Draw_TransSubPic(x, y, pic, srcx, srcy, width, height);
}

void Draw_STransPic (int x, int y, mpic_t *pic, float scale)
{
    // no scale in SOFT yet..
    Draw_TransPic(x, y, pic);
}

void Draw_SFill (int x, int y, int w, int h, int c, float scale)
{
    // no scale in SOFT yet..
    Draw_Fill(x, y, w, h, c);
}

// kazik <--

//=============================================================================

//Draws the little blue disc in the corner of the screen.
//Call before beginning any disc IO.
void Draw_BeginDisc (void) {
	if (!draw_disc)
		return;		// not initialized yet
	D_BeginDirectRect (vid.width - 24, 0, draw_disc->data, 24, 24);
}

//Erases the disc icon.
//Call after completing any disc IO
void Draw_EndDisc (void) {
	D_EndDirectRect (vid.width - 24, 0, 24, 24);
}
