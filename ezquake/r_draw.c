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
#include "d_iface.h"
#include "sound.h"
#include "version.h"

typedef struct {
	vrect_t	rect;
	int		width;
	int		height;
	byte	*ptexbytes;
	int		rowbytes;
} rectdesc_t;

static rectdesc_t	r_rectdesc;

byte		*draw_chars;				// 8*8 graphic characters
mpic_t		*draw_disc;
mpic_t		*draw_backtile;

cvar_t	scr_conalpha	= {"scr_conalpha", "1"};
cvar_t	scr_menualpha	= {"scr_menualpha", "0.7"};

void customCrosshair_Init(void);

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

mpic_t *Draw_CachePic (char *path) {
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

	dat = Cache_Check (&pic->cache);

	if (dat)
		return (mpic_t *)dat;

	// load the pic from disk
	FS_LoadCacheFile (path, &pic->cache);
	
	dat = (qpic_t *)pic->cache.data;
	if (!dat)
		Sys_Error ("Draw_CachePic: failed to load %s", path);

	SwapPic (dat);

	((mpic_t *) dat)->width = dat->width;
	((mpic_t *) dat)->alpha = memchr (&dat->data, 255, dat->width * ((mpic_t *)dat)->height) != NULL;

	return (mpic_t *)dat;
}

void Draw_Init (void) {
	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register(&scr_conalpha);

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register(&scr_menualpha);

	Cvar_ResetCurrentGroup();

	draw_chars = (byte *) Draw_CacheWadPic ("conchars");
	draw_disc = Draw_CacheWadPic ("disc");
	draw_backtile = Draw_CacheWadPic ("backtile");

	r_rectdesc.width = draw_backtile->width;
	r_rectdesc.height = draw_backtile->height;
	r_rectdesc.ptexbytes = draw_backtile->data;
	r_rectdesc.rowbytes = draw_backtile->width;

	customCrosshair_Init();
}

//Draws one 8*8 graphics character with 0 being transparent.
//It can be clipped to the top of the screen to allow the console to be smoothly scrolled off.
void Draw_Character (int x, int y, int num) {
	byte *dest, *source;
	int drawline, row, col;

	num &= 255;

	if (y <= -8)
		return;			// totally off screen

	if (y > (int) vid.height - 8 || x < 0 || x > vid.width - 8)
		return;

	if (num < 0 || num > 255)
		return;

	row = num >> 4;
	col = num & 15;
	source = draw_chars + (row << 10) + (col << 3);

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

void Draw_String (int x, int y, char *str) {
	while (*str) {
		Draw_Character (x, y, *str);
		str++;
		x += 8;
	}
}

void Draw_Alt_String (int x, int y, char *str) {
	while (*str) {
		Draw_Character (x, y, (*str) | 0x80);
		str++;
		x += 8;
	}
}


static int HexToInt(char c) {
	if (isdigit(c))
		return c - '0';
	else if (c >= 'a' && c <= 'f')
		return 10 + c - 'a';
	else if (c >= 'A' && c <= 'F')
		return 10 + c - 'A';
	else
		return -1;
}

void Draw_ColoredString (int x, int y, char *text, int red) {
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

void Draw_Pixel(int x, int y, byte color) {
	byte *dest;

	dest = vid.buffer + y * vid.rowbytes + x;
	*dest = color;
}


#define		NUMCROSSHAIRS 6
static qboolean crosshairdata[NUMCROSSHAIRS][64] = {
	false, false, false, true,  false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, true,  false, false, false, false,
	true,  false, true,  false, true,  false, true,  false,
	false, false, false, true,  false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, true,  false, false, false, false,
	false, false, false, false, false, false, false, false,

	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, true,  false, false, false, false,
	false, false, true,  true,  true,  false, false, false,
	false, false, false, true,  false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,

	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, true,  false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,

	true,  false, false, false, false, false, false, true,
	false, true,  false, false, false, false, true, false,
	false, false, true,  false, false, true,  false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, true,  false, false, true,  false, false,
	false, true,  false, false, false, false, true,  false,
	true,  false, false, false, false, false, false, true,

	false, false, true,  true,  true,  false, false, false, 
	false, true,  false, true,  false, true,  false, false, 
	true,  true,  false, true,  false, true,  true,  false, 
	true,  true,  true,  true,  true,  true,  true,  false, 
	true,  false, true,  true,  true,  false, true,  false, 
	false, true,  false, false, false, true,  false, false, 
	false, false, true,  true,  true,  false, false, false, 
	false, false, false, false, false, false, false, false,
	
	false, false, true,  true,  true,  false, false, false, 
	false, false, false, false, false, false, false, false, 
	true,  false, false, false, false, false, true,  false, 
	true,  false, false, true,  false, false, true,  false, 
	true,  false, false, false, false, false, true,  false, 
	false, false, false, false, false, false, false, false, 
	false, false, true,  true,  true,  false, false, false, 
	false, false, false, false, false, false, false, false
};

static qboolean customcrosshairdata[64];
static qboolean customcrosshair_loaded;
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
	int x, y, crosshair_val, row, col;
	extern cvar_t crosshair, cl_crossx, cl_crossy, crosshaircolor, crosshairsize;
	extern vrect_t scr_vrect;
	byte c = (byte) crosshaircolor.value;
	qboolean *data;

	if (!crosshair.value)
		return;

	x = scr_vrect.x + scr_vrect.width / 2 + cl_crossx.value; 
	y = scr_vrect.y + scr_vrect.height / 2 + cl_crossy.value;

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
		Sys_Error ("Draw_TransPic: bad coordinates");

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
	source = draw_chars + (row<<10) + (col<<3);

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
	char ver[100];
	static char saveback[320 * 8];

	if (!scr_conalpha.value && !SCR_NEED_CONSOLE_BACKGROUND)
		return;

	conback = Draw_CachePic ("gfx/conback.lmp");

	// hack the version number directly into the pic

	if (cls.download) {
		strcpy (ver, FUH_VERSION);
		dest = conback->data + 320 + 320 * 186 - 11 - 8 * strlen(ver);
	} else {
		sprintf (ver, "FuhQuake %s", FUH_VERSION);
		dest = conback->data + 320 - (strlen(ver) * 8 + 11) + 320 * 186;
	}

	memcpy (saveback, conback->data + 320 * 186, 320 * 8);
	for (x = 0; x < strlen(ver); x++)
		Draw_CharToConback (ver[x] | 128, dest + (x << 3));
	
	// draw the pic
	dest = vid.buffer;

	for (y = 0; y < lines; y++, dest += vid.rowbytes) {
		v = (vid.conheight - lines + y) * 200 / vid.conheight;
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
void Draw_Fill (int x, int y, int w, int h, int c) {
	byte *dest;
	int u, v;

	if (x < 0 || x + w > vid.width || y < 0 || y + h > vid.height) {
		Com_Printf ("Bad Draw_Fill(%d, %d, %d, %d, %c)\n", x, y, w, h, c);
		return;
	}

	dest = vid.buffer + y*vid.rowbytes + x;
	for (v = 0; v < h; v++, dest += vid.rowbytes)
		for (u = 0; u < w; u++)
			dest[u] = c;
}

//=============================================================================

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

//=============================================================================

//Draws the little blue disc in the corner of the screen.
//Call before beginning any disc IO.
void Draw_BeginDisc (void) {
	D_BeginDirectRect (vid.width - 24, 0, draw_disc->data, 24, 24);
}

//Erases the disc icon.
//Call after completing any disc IO
void Draw_EndDisc (void) {
	D_EndDirectRect (vid.width - 24, 0, 24, 24);
}
