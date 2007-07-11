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

typedef struct 
{
	vrect_t	rect;
	int		width;
	int		height;
	byte	*ptexbytes;
	int		rowbytes;
} rectdesc_t;

static rectdesc_t	r_rectdesc;

#define		MAX_CHARSETS 16
int			char_range[MAX_CHARSETS];		// 0x0400, etc; slot 0 is always 0x00
byte		*draw_chars[MAX_CHARSETS];		// 8*8 graphic characters
											// slot 0 is static, the rest are Q_malloc'd
mpic_t		*draw_disc;
mpic_t		*draw_backtile;

cvar_t	scr_conalpha	= {"scr_conalpha", "1"};
cvar_t	scr_menualpha	= {"scr_menualpha", "0.7"};

void customCrosshair_Init(void);

extern cvar_t con_shift;

int scissor_left	= 0;
int	scissor_right	= 0;
int scissor_top		= 0;
int scissor_bottom	= 0;

//=============================================================================
// Support Routines 

typedef struct cachepic_s 
{
	char			name[MAX_QPATH];
	cache_user_t	cache;
} cachepic_t;

#define	MAX_CACHED_PICS		128
cachepic_t	cachepics[MAX_CACHED_PICS];
int			numcachepics;

mpic_t *Draw_CacheWadPic (char *name) 
{
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
	{
		if (!strcmp (path, pic->name))
			break;
	}

	if (i == numcachepics) 
	{
		if (numcachepics == MAX_CACHED_PICS)
			Sys_Error ("numcachepics == MAX_CACHED_PICS");
		numcachepics++;
		strcpy (pic->name, path);
	}

	dat = (qpic_t *) Cache_Check (&pic->cache);

	if (dat)
		return (mpic_t *)dat;

	// Load the pic from disk.
	FS_LoadCacheFile (path, &pic->cache);
	
	dat = (qpic_t *)pic->cache.data;
	if (!dat) 
	{
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

//
// Loads a required picture (if not present, exit with an error)
//
mpic_t *Draw_CachePic (char *path) 
{
	return Draw_CachePicBase(path, true);
}

//
// Load an optional picture (if not present, returns null)
//
mpic_t *Draw_CachePicSafe (char *path, qbool syserror, qbool obsolete)
{
	// 3rd argument is unused because we don't have 24bit pictures support in non-gl rendering
	// and yes, 2nd argument is illogical because calling a function that has 'Safe' in the name
	// we would expect that it automatically presumes not lead to app exit
	return Draw_CachePicBase (path, syserror);
}

//
// Returns Q_malloc'd data, or NULL or error
//
static byte *LoadAlternateCharset (const char *name)
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

void Draw_EnableScissor(int left, int right, int top, int bottom)
{
	clamp(left, 0, vid.width);
	clamp(right, 0, vid.width);
	clamp(top, 0, vid.height);
	clamp(bottom, 0, vid.height);

	left = (left > right) ? right  : left;
	top	 = (top > bottom) ? bottom : top;

	scissor_left	= left;
	scissor_right	= right;
	scissor_top		= top;
	scissor_bottom	= bottom;
}

void Draw_EnableScissorRectangle(int x, int y, int width, int height)
{
	Draw_EnableScissor(x, (x + width), y, (y + height));
}

void Draw_DisableScissor()
{
	scissor_left	= 0;
	scissor_right	= vid.width;
	scissor_top		= 0;
	scissor_bottom	= vid.height;
}

void Draw_Init (void) 
{
	Draw_DisableScissor();

	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register(&scr_conalpha);

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register(&scr_menualpha);

	Cvar_ResetCurrentGroup();

	W_LoadWadFile("gfx.wad"); // Safe re-init.

	draw_chars[0] = W_GetLumpName ("conchars");
	draw_chars[1] = LoadAlternateCharset ("conchars-cyr");
	if (draw_chars[1])
		char_range[1] = (1 << 10);

	draw_disc = Draw_CacheWadPic ("disc");
	draw_backtile = Draw_CacheWadPic ("backtile");

	r_rectdesc.width = draw_backtile->width;
	r_rectdesc.height = draw_backtile->height;
	r_rectdesc.ptexbytes = draw_backtile->data;
	r_rectdesc.rowbytes = draw_backtile->width;

	customCrosshair_Init();
}

// 
// Checks if a unicode character is available.
// 
qbool R_CharAvailable (wchar num)
{
	int i;

	if (num == (num & 0xFF))
		return true;

	for (i = 1; i < MAX_CHARSETS; i++)
	{
		if (char_range[i] == (num & 0xFF00))
			return true;
	}

	return false;
}

//
// Draws one 8*8 graphics character with 0 being transparent.
// It can be clipped to the top of the screen to allow the console to be smoothly scrolled off.
//
void Draw_Character (int x, int y, int num) 
{
	Draw_CharacterW (x, y, char2wc(num));
}

void Draw_CharacterW (int x, int y, wchar num) 
{
	// The length of a row in the charset image, 16 characters, 8 pixels each. 16*8 = 128
	#define CHARSET_ROW_LENGTH	(16 * 8)

	byte *dest, *source;	// Draw destination and character source.
	int row_outside = 0;	// How many lines of the character are outside the specified scissor bounds.
	int row_drawcount = 0;	// How many lines of the character should be drawn.
	int col_outside = 0;	// The number of columns in the character that aren't visible.
	int col_drawcount = 0;	// How many columns of the character that should be drawn.
	int row, col;			// Row and column for the character in the charset texture.
	int slot = 0;			// The slot for the charset the character was located in.
	int i = 0;

	// Don't draw if we're outside the scissor bounds.
	if ((y <= (scissor_top - 8))  || (y > scissor_bottom) 
	 || (x <= (scissor_left - 8)) || (x > scissor_right))
		return;

	// Find a characterset with the character available in it.
	if ((num & 0xFF00) != 0)
	{
		for (i = 1; i < MAX_CHARSETS; i++)
		{
			if (char_range[i] == (num & 0xFF00)) 
			{
				slot = i;
				break;
			}
		}

		if (i == MAX_CHARSETS)
			num = '?';
	}

	row = (num >> 4) & 0x0F; // 'a' = 97 ASCII		(97 >> 4) = 6, row 6 in the charset (Row of characters, not pixels).
	col = num & 0x0F;		 // 'a' = 112 ASCII		(112 & 0x0F) = 1, column 1 (0-index).
	
	// Get the buffer position of the character,
	// 16 chars per row
	// 8*8 pixels per char
	// Example: 'a' = 97 gives row = 6, col = 1
	// (16 chars per row) * (8*8 pixels per char) * (6 rows) = 6144
	source = draw_chars[slot] + (16 * 8*8 * row) + (8 * col);

	//
	// Clip the character according to scissor bounds.
	//
	{
		if (x < scissor_left)
		{
			// Outside to the left.
			col_outside = (scissor_left - x);		// How many pixel columns of the character are outside?
			source += col_outside;					// Move forward in the charset buffer so that only the visible part is drawn.
			x = scissor_left;						// Set the new drawing point to start at.		
		}
		else if (x > (scissor_right - 8))
		{
			// Outside to the right.
			col_outside = (scissor_right - x);	
		}

		// The number of columns to draw.
		col_drawcount = (8 - col_outside);

		if (y < scissor_top) 
		{
			// Outside at the top.
			row_outside = (scissor_top - y);				// How many lines we should draw of the character.
			source += CHARSET_ROW_LENGTH * row_outside;		// Draw the bottom part of the image by moving our pos in the source.
			y = scissor_top;								// Move the point we draw at to match the bounds.
		}
		else if (y > (scissor_bottom - 8))
		{
			row_outside = (scissor_bottom - y);
		}

		// The number of lines to draw.
		row_drawcount = (8 - row_outside);
	}

	// Get the destination in the video buffer.
	dest = vid.buffer + (y * vid.rowbytes) + x;

	// Draw the character to the video buffer.
	while (row_drawcount--)
	{
		for (i = 0; i < col_drawcount; i++)
		{
			if (source[i])
				dest[i] = source[i];
		}

		source += CHARSET_ROW_LENGTH;		// Advance to the next row of the character source.
		dest += vid.rowbytes;				// Next row of the video buffer.
	}
}

void Draw_String (int x, int y, const char *str) 
{
	while (*str) 
	{
		Draw_Character (x, y, *str);
		str++;
		x += 8;
	}
}

void Draw_StringW (int x, int y, const wchar *str) 
{
	while (*str) 
	{
		Draw_CharacterW (x, y, *str);
		str++;
		x += 8;
	}
}

void Draw_Alt_String (int x, int y, const char *str) 
{
	while (*str) 
	{
		Draw_Character (x, y, (*str) | 0x80);
		str++;
		x += 8;
	}
}


int HexToInt(char c) 
{
	if (isdigit(c))
		return c - '0';
	else if (c >= 'a' && c <= 'f')
		return 10 + c - 'a';
	else if (c >= 'A' && c <= 'F')
		return 10 + c - 'A';
	else
		return -1;
}

void Draw_ColoredString (int x, int y, const char *text, int red) 
{
	int r, g, b;

	for ( ; *text; text++) 
	{
		if (*text == '&') 
		{
			if (text[1] == 'c' && text[2] && text[3] && text[4])	
			{
				r = HexToInt(text[2]);
				g = HexToInt(text[3]);
				b = HexToInt(text[4]);
				if (r >= 0 && g >= 0 && b >= 0)
					text += 5;
            } 
			else if (text[1] == 'r')	
			{
				text += 2;
			}
		}
		Draw_Character (x, y, (*text) | (red ? 0x80 : 0));
		x += 8;
	}
}

/*
const color_t COLOR_WHITE = 0xFFFFFFFF;

color_t RGBA_TO_COLOR(byte r, byte g, byte b, byte a) 
{
	return 0xFFFFFFFF;
}

color_t RGBAVECT_TO_COLOR(byte rgba[4])
{
	return 0xFFFFFFFF;
}

byte* COLOR_TO_RGBA(color_t i, byte rgba[4]) 
{
	memset(rgba, 255, 4);
	return rgba;
}
*/

const int COLOR_WHITE = 0xFFFFFFFF;

color_t RGBA_TO_COLOR(byte r, byte g, byte b, byte a) 
{
	return ((r << 0) | (g << 8) | (b << 16) | (a << 24)) & 0xFFFFFFFF;
}

color_t RGBAVECT_TO_COLOR(byte rgba[4])
{
	return ((rgba[0] << 0) | (rgba[1] << 8) | (rgba[2] << 16) | (rgba[3] << 24)) & 0xFFFFFFFF;
}

byte* COLOR_TO_RGBA(color_t i, byte rgba[4]) 
{
	rgba[0] = (i >> 0  & 0xFF);
	rgba[1] = (i >> 8  & 0xFF);
	rgba[2] = (i >> 16 & 0xFF);
	rgba[3] = (i >> 24 & 0xFF);

	return rgba;
}

void Draw_ColoredString3 (int x, int y, const char *text, clrinfo_t *clr, int clr_cnt, int red) 
{
	Draw_ColoredString(x, y, text, red);
}

void Draw_ColoredString3W (int x, int y, const wchar *text, clrinfo_t *clr, int clr_cnt, int red) 
{
	Draw_ColoredString(x, y, wcs2str(text), red);
}

// No scale in software. Sorry :(
void Draw_ScalableColoredString (int x, int y, const wchar *text, clrinfo_t *clr, int clr_cnt, int red, float scale) 
{
	Draw_ColoredString(x, y, wcs2str(text), red);
}

static __inline void Draw_Pixel(int x, int y, byte color) 
{
	byte *dest;

	if ((x < scissor_left) || (x > scissor_right) || (y < scissor_top) || (y > scissor_bottom))
		return;

	dest = vid.buffer + (y * vid.rowbytes) + x;
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
void customCrosshair_Init(void) 
{
	FILE *f;
	int i = 0, c;

	customcrosshair_loaded = false;
	if (FS_FOpenFile("crosshairs/crosshair.txt", &f) == -1)
		return;

	while (i < 64) 
	{
		c = fgetc(f);
		if (c == EOF) 
		{
			Com_Printf("Invalid format in crosshair.txt (Need 64 X's and O's)\n");
			fclose(f);
			return;
		}
		if (isspace(c))
			continue;
		if (c  != 'x' && c  != 'X' && c  != 'O' && c  != 'o') 
		{
			Com_Printf("Invalid format in crosshair.txt (Only X's and O's and whitespace permitted)\n");
			fclose(f);
			return;
		} 
		else if (c == 'x' || c  == 'X' )
			customcrosshairdata[i++] = true;
		else
			customcrosshairdata[i++] = false;		
	}
	fclose(f);
	customcrosshair_loaded = true;
}

void Draw_Crosshair(void) 
{
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
		for (row = 0; row < 8; row++) 
		{
			for (col = 0; col < 8; col++) 
			{
				if (data[row * 8 + col]) 
				{
					if (crosshairsize.value >= 3) 
					{
						Draw_Pixel(x + 3 * (col - 3) - 1,	y + 3 * (row - 3) - 1,	c);
						Draw_Pixel(x + 3 * (col - 3) - 1,	y + 3 * (row - 3),		c);
						Draw_Pixel(x + 3 * (col - 3) - 1,	y + 3 * (row - 3) + 1,	c);

						Draw_Pixel(x + 3 * (col - 3),		y + 3 * (row - 3) - 1,	c);
						Draw_Pixel(x + 3 * (col - 3),		y + 3 * (row - 3),		c);
						Draw_Pixel(x + 3 * (col - 3),		y + 3 * (row - 3) + 1,	c);

						Draw_Pixel(x + 3 * (col - 3) + 1,	y + 3 * (row - 3) - 1,	c);
						Draw_Pixel(x + 3 * (col - 3) + 1,	y + 3 * (row - 3),		c);
						Draw_Pixel(x + 3 * (col - 3) + 1,	y + 3 * (row - 3) + 1,	c);
					} 
					else if (crosshairsize.value >= 2) {
						Draw_Pixel(x + 2 * (col - 3),		y + 2 * (row - 3),		c);
						Draw_Pixel(x + 2 * (col - 3),		y + 2 * (row - 3) - 1,	c);

						Draw_Pixel(x + 2 * (col - 3) - 1,	y + 2 * (row - 3),		c);
						Draw_Pixel(x + 2 * (col - 3) - 1,	y + 2 * (row - 3) - 1,	c);
					} 
					else 
					{
						Draw_Pixel(x + col - 3, y + row - 3, c);
					}
				}
			}
		}
	} 
	else 
	{
		Draw_Character (x - 4, y - 4, '+');
	}
}



void Draw_TextBox (int x, int y, int width, int lines) 
{
	mpic_t *p;
	int cx, cy, n;

	// draw left side
	cx = x;
	cy = y;
	p = Draw_CachePic ("gfx/box_tl.lmp");
	Draw_TransPic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_ml.lmp");
	for (n = 0; n < lines; n++) 
	{
		cy += 8;
		Draw_TransPic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_bl.lmp");
	Draw_TransPic (cx, cy+8, p);

	// draw middle
	cx += 8;
	while (width > 0) 
	{
		cy = y;
		p = Draw_CachePic ("gfx/box_tm.lmp");
		Draw_TransPic (cx, cy, p);
		p = Draw_CachePic ("gfx/box_mm.lmp");
		for (n = 0; n < lines; n++) 
		{
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
	for (n = 0; n < lines; n++) 
	{
		cy += 8;
		Draw_TransPic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_br.lmp");
	Draw_TransPic (cx, cy + 8, p);
}

void Draw_Pic (int x, int y, mpic_t *pic) 
{
	Draw_TransSubPic(x, y, pic, 0, 0, pic->width, pic->height);
}

void Draw_SubPic (int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height) 
{
	Draw_TransSubPic (x, y, pic, srcx, srcy, width, height);
}

void Draw_TransPic (int x, int y, mpic_t *pic) 
{
	Draw_TransSubPic(x, y, pic, 0, 0, pic->width, pic->height);
}

void Draw_TransSubPic (int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height) 
{
	byte *dest, *source, tbyte;
	int v, u, i;

	// Completely outside of scissor bounds.
	if (((x + width) < scissor_left) || ((x - width) > scissor_right) 
	 || ((y + height) < scissor_top) || ((y - height) > scissor_bottom))
		return;

	// Move the position in the source so that we only draw the part that is
	// within the scissor bounds.
	{
		srcx += (x < scissor_left) ? (scissor_left - x) : 0;
		width -= ((x + width) > scissor_right) ? ((x + width) - scissor_right) : 0;

		srcy += (y < scissor_top) ? (scissor_top - y) : 0;
		height -= ((y + height) > scissor_bottom) ? ((y + height) - scissor_bottom) : 0; 
	}

	if ((width < 0) || (height < 0) || (srcx > pic->width) || (srcy > pic->height))
	{
		return;
	}

	source = pic->data + srcy * pic->width + srcx;

	dest = vid.buffer + y * vid.rowbytes + x;

	if (pic->alpha)
	{
		if (width & 7) 
		{	
			// General
			for (v = 0; v < height; v++) 
			{
				for (u = 0; u < width; u++)
				{
					if ((tbyte = source[u]) != TRANSPARENT_COLOR)
						dest[u] = tbyte;
				}

				dest += vid.rowbytes;
				source += pic->width;
			}
		} 
		else 
		{	
			// Unwound
			for (v = 0; v < height; v++) 
			{
				for (u = 0; u < width; u += 8) 
				{
					for (i = 0; i < 8; i++)
					{
						if ((tbyte = source[u + i]) != TRANSPARENT_COLOR)
							dest[u + i] = tbyte;
					}
				}
				dest += vid.rowbytes;
				source += pic->width;
			}
		}
	}
	else
	{
		for (v = 0; v < height; v++) 
		{
			memcpy (dest, source, width);
			dest += vid.rowbytes;
			source += pic->width;
		}
	}
}

void Draw_CharToConback (int num, byte *dest) 
{
	int	 row, col, drawline, x;
	byte *source;

	row = num >> 4;
	col = num & 15;
	source = draw_chars[0] + (row<<10) + (col<<3);

	drawline = 8;

	while (drawline--) 
	{
		for (x = 0; x < 8; x++)
		{
			if (source[x])
				dest[x] = source[x];
		}
		source += 128;
		dest += 320;
	}
}

void Draw_ConsoleBackground (int lines) 
{
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

	for (y = 0; y < lines; y++, dest += vid.rowbytes) 
	{
		v = (vid.conheight - lines + y + con_shift.value) * 200 / vid.conheight;
		src = conback->data + v * 320;
		if (vid.conwidth == 320) 
		{
			memcpy (dest, src, vid.conwidth);
		}
		else 
		{
			f = 0;
			fstep = 320 * 0x10000 / vid.conwidth;
			for (x = 0; x < vid.conwidth; x += 4) 
			{
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

	// Put it back
	memcpy(conback->data + 320 * 186, saveback, 320 * 8);
}

void R_DrawRect8 (vrect_t *prect, int rowbytes, byte *psrc, int transparent) 
{
	byte t;
	int i, j, srcdelta, destdelta;
	byte *pdest;

	pdest = vid.buffer + (prect->y * vid.rowbytes) + prect->x;

	srcdelta = rowbytes - prect->width;
	destdelta = vid.rowbytes - prect->width;

	if (transparent) 
	{
		for (i = 0; i < prect->height; i++) 
		{
			for (j = 0; j < prect->width; j++) 
			{
				t = *psrc;
				if (t != TRANSPARENT_COLOR)
					*pdest = t;

				psrc++;
				pdest++;
			}

			psrc += srcdelta;
			pdest += destdelta;
		}
	} 
	else 
	{
		for (i = 0; i < prect->height; i++) 
		{
			memcpy (pdest, psrc, prect->width);
			psrc += rowbytes;
			pdest += vid.rowbytes;
		}
	}
}

// This repeats a 64*64 tile graphic to fill the screen around a sized down refresh window.
void Draw_TileClear (int x, int y, int w, int h) 
{
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

	while (height > 0) 
	{
		vr.x = r_rectdesc.rect.x;
		width = r_rectdesc.rect.width;

		vr.height = r_rectdesc.height - tileoffsety;

		if (vr.height > height)
			vr.height = height;

		tileoffsetx = vr.x % r_rectdesc.width;

		while (width > 0) 
		{
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

// 
// Fills a box of pixels with a single color
// 
void Draw_Fill (int x, int y, int w, int h, byte c) 
{
	int cur_x, cur_y;

	for (cur_y = 0; cur_y < h; cur_y++)
	{
		for (cur_x = 0; cur_x < w; cur_x++)
		{
			Draw_Pixel(x + cur_x, y + cur_y, c);
		}
	}
}

static byte Draw_FindNearestColor(color_t color)
{
	#define PALETTE_SIZE 255
    int i; 
	int distanceSquared;
	int bestIndex = 0;
    int minDistanceSquared = (255 * 255) + (255 * 255) + (255 * 255) + 1;
	byte bytecolor[4];

	COLOR_TO_RGBA(color, bytecolor);
    
	for (i = 0; i < PALETTE_SIZE; i++)
	{
		int Rdiff = ((int)bytecolor[0]) - host_basepal[(i * 3)];
        int Gdiff = ((int)bytecolor[1]) - host_basepal[(i * 3) + 1];
        int Bdiff = ((int)bytecolor[2]) - host_basepal[(i * 3) + 2];
        
		distanceSquared = (Rdiff * Rdiff) + (Gdiff * Gdiff) + (Bdiff * Bdiff);
        
		if (distanceSquared < minDistanceSquared) 
		{
            minDistanceSquared = distanceSquared;
            bestIndex = i;
        }
    }

    return bestIndex;
}

void Draw_AlphaFill (int x, int y, int w, int h, byte c, float alpha)
{
	Draw_FadeBox(x, y, w, h, c, alpha);
}

void Draw_AlphaFillRGB (int x, int y, int width, int height, color_t color)
{
	Draw_FadeBox(x, y, width, height, Draw_FindNearestColor(color), (float)((color >> 24) & 0xFF) / 255.0);
}

void Draw_AlphaRectangleRGB (int x, int y, int w, int h, float thickness, qbool fill, color_t color)
{
	int t = Q_rint(thickness);

	if (fill)
	{
		Draw_AlphaFillRGB(x, y, w, h, color);
	}
	else
	{
		Draw_AlphaFillRGB(x,		 y,				w,	t,				color);
		Draw_AlphaFillRGB(x,		 (y + h - t),	w,	t,				color);
		Draw_AlphaFillRGB(x,		 (y + t),		t,	(h - (2 * t)),	color);
		Draw_AlphaFillRGB(x + w - t, (y + t),		t,	(h - (2 * t)),	color);
	}
}

//=============================================================================

// ================
// Draw_FadeBox
// ================

void Draw_FadeBox (int x, int y, int width, int height, byte color, float alpha)
{
	int t;
	int cur_x, cur_y;

	if (alpha <= 0)
		return;

	for (cur_y = y; cur_y < y + height; cur_y++)
	{
		if (alpha < 0.333)
		{
			t = (cur_y & 1) << 1;

			for (cur_x = x; cur_x < x + width; cur_x++)
			{
				if ((cur_x & 3) == t)
					Draw_Pixel(cur_x, cur_y, color); 
			}
		}
		else if (alpha < 0.666)
		{
			t = (cur_y & 1);

			for (cur_x = x; cur_x < x + width; cur_x++)
			{
				if ((cur_x & 1) == t)
					Draw_Pixel(cur_x, cur_y, color);
			}
		}
		else if (alpha < 1)
		{
			t = (cur_y & 1) << 1;

			for (cur_x = x; cur_x < x + width; cur_x++)
			{
				if ((cur_x & 3) != t)
					Draw_Pixel(cur_x, cur_y, color);
			}
		}
		else
		{
			for (cur_x = x; cur_x < x + width; cur_x++)
				Draw_Pixel(cur_x, cur_y, color);
		}
	}
}

void Draw_FadeScreen (void) 
{
	Draw_FadeBox(0, 0, vid.width, vid.height, 0, scr_menualpha.value);
}

//
// SCALE versions of some functions
//

void Draw_SCharacter (int x, int y, int num, float scale)
{
    // no scale in SOFT yet..
    Draw_Character(x, y, num);
}

void Draw_SString (int x, int y, const char *str, float scale)
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

void Draw_SFill (int x, int y, int w, int h, byte c, float scale)
{
    // no scale in SOFT yet..
    Draw_Fill(x, y, w, h, c);
}

//=============================================================================

// 
// Draws the little blue disc in the corner of the screen.
// Call before beginning any disc IO.
// 
void Draw_BeginDisc (void) 
{
	if (!draw_disc)
		return;		// not initialized yet
	D_BeginDirectRect (vid.width - 24, 0, draw_disc->data, 24, 24);
}

// 
// Erases the disc icon.
// Call after completing any disc IO
// 
void Draw_EndDisc (void) 
{
	D_EndDirectRect (vid.width - 24, 0, 24, 24);
}
