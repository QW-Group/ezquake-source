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

#define CLIP_TOP(y)		((y) < scissor_top)
#define CLIP_BOTTOM(y)	((y) > scissor_bottom)
#define CLIP_LEFT(x)	((x) < scissor_left)
#define CLIP_RIGHT(x)	((x) > scissor_right)

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
	if ((y < (scissor_top - 8))  || (y > scissor_bottom) || (x < (scissor_left - 8)) || (x > scissor_right))
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

	if (CLIP_LEFT(x) || CLIP_RIGHT(x) || CLIP_TOP(y) || CLIP_BOTTOM(y))
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

// ============================================================
// From GraphicGems fastBitmap.c 
// (http://www.acm.org/tog/GraphicsGems/)
// Stretches a horizontal source line onto a horizontal
// destination line. Used by RectStretch.
// Entry:
//	x_dest_start, x_dest_end - x-coordinates of the destination 
//							   line (coordinates inside texture)
//	x_src_start, x_src_end	 - x-coordinates of the source line 
//							   (screen coordinates)
//	y_src					 - y-coordinate of source line
//	y_dest					 - y-coordinate of destination line
// ============================================================
static void Draw_StretchLine(mpic_t *pic, int x_dest_start, int x_dest_end, int x_src_start, int x_src_end, int y_src, int y_dest)
{
	byte *srcdata = pic->data;
	int dx_dest, dx_src, e, d;
	short x_dest_sign, x_src_sign;
	byte color;

	// Calculate the source and destination widths.
	dx_dest = abs((int)(x_dest_end - x_dest_start));
	dx_src  = abs((int)(x_src_end  - x_src_start));

	// Get the sign to increment/decrement by.
	x_dest_sign = sgn(x_dest_end - x_dest_start);
	x_src_sign  = sgn(x_src_end  - x_src_start);

	// Some cleverness so we don't have to use floats
	// to calculate the ratio of pixels to draw.
	e = (dx_src << 1) - dx_dest;
	dx_src <<= 1;
	
	// Loop through each pixel in the destination line.
	for(d = 0; d <= dx_dest; d++)
	{
		// Get the pixel for the specified source row.
		color = *(srcdata + (y_src * pic->width) + x_src_start);

		// Draw the pixel if it isn't transparent.
		if (color != TRANSPARENT_COLOR)
			Draw_Pixel(x_dest_start, y_dest, color);

		while(e >= 0)
		{
			x_src_start += x_src_sign;
			e -= (dx_dest << 1); 
		}

		x_dest_start += x_dest_sign;
		e += dx_src;
	}
}

//
// x,y						- Position of the pic
// width, height			- The size of the rectangle, the pic will fit within this.
// srcx, srcy				- The position in the pic to start drawing from.
// src_width, src_height	- The width and height of the sub-area of the pic to draw.
//
void Draw_RectStretchSubPic(mpic_t *pic, int x, int y, int srcx, int srcy, int src_width, int src_height, int width, int height)
{
	int dy_dest, dy_src, e, d;
	short y_sign_dest, y_sign_src;

	// Were to start in the source.
	int x_src_start = abs(srcx);
	int y_src_start = abs(srcy);
	int x_src_end	= abs(src_width);
	int y_src_end	= abs(src_height);
	
	// Where to write the image on screen.
	int x_dest_start = x;
	int y_dest_start = y;
	int x_dest_end	= x + width; 
	int y_dest_end	= y + height;

	// Make sure we don't try to draw something outside the source.
	clamp(x_src_start, 0, pic->width);
	clamp(y_src_start, 0, pic->height);
	clamp(x_src_end, 0, (pic->width - x_src_start)); 
	clamp(y_src_end, 0, (pic->height - y_src_start)); 

	dy_dest = abs(y_dest_end - y_dest_start);
	dy_src  = abs(y_src_end  - y_src_start);

	// Get the direction we should move when drawing (up or down).
	y_sign_dest = sgn(y_dest_end - y_dest_start);
	y_sign_src  = sgn(y_src_end  - y_src_start);
	
	e = (dy_src << 1) - dy_dest;
	dy_src <<= 1;

	for(d = 0; d < dy_dest; d++)
	{
		Draw_StretchLine(pic, x_dest_start, x_dest_end, x_src_start, x_src_end, y_src_start, y_dest_start);
		
		while(e >= 0)
		{
			y_src_start += y_sign_src;
			e -= dy_dest << 1;
		}

		y_dest_start += y_sign_dest;
		e += dy_src;
	}
}

// ============================================================
// Based on GraphicGems fastBitmap.c 
// (http://www.acm.org/tog/GraphicsGems/)
// RectStretch enlarges or diminishes a source rectangle of
// a bitmap to a destination rectangle. The source
// rectangle is selected by the two points (xs1,ys1) and
// (xs2,ys2), and the destination rectangle by (xd1,yd1) and
// (xd2,yd2). Since readability of source-code is wanted,
// some optimizations have been left out for the reader:
// It's possible to read one line at a time, by first
// stretching in x-direction and then stretching that bitmap
// in y-direction.
// Entry:
//	x,y			  - Position of the pic
//	width, height - The size of the rectangle, the pic will
//					fit within this.
// ============================================================
void Draw_RectStretch(mpic_t *pic, int x, int y, int width, int height)
{
	Draw_RectStretchSubPic(pic, x, y, 0, 0, pic->width, pic->height, width, height);
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
	byte *source, tbyte;
	int v, u;

	// Completely outside of scissor bounds.
	if (CLIP_LEFT(x + width) || CLIP_RIGHT(x - width) || CLIP_TOP(x + height) || CLIP_BOTTOM(y - height))
		return;

	// Move the position in the source so that we only draw the part that is
	// within the scissor bounds.
	{
		srcx += CLIP_LEFT(x) ? (scissor_left - x) : 0;
		width -= CLIP_RIGHT(x + width) ? ((x + width) - scissor_right) : 0;

		srcy += CLIP_TOP(y) ? (scissor_top - y) : 0;
		height -= CLIP_BOTTOM(y + height) ? ((y + height) - scissor_bottom) : 0; 
	}

	if ((width < 0) || (height < 0) || (srcx > pic->width) || (srcy > pic->height))
	{
		return;
	}

	source = pic->data + (srcy * pic->width) + srcx;

	for (v = 0; v < height; v++) 
	{
		if (CLIP_TOP(y + v) || CLIP_BOTTOM(y + v))
			continue;

		for (u = 0; u < width; u++)
		{
			if (CLIP_LEFT(x + u) || CLIP_RIGHT(x + u))
				continue;

			if ((tbyte = source[u]) == TRANSPARENT_COLOR && pic->alpha)
				continue;

			Draw_Pixel(x + u, y + v, tbyte);
		}

		source += pic->width;
	}
}

void Draw_CharToConback (int num, byte *dest) 
{
	int	 row, col, drawline, x;
	byte *source;

	row = num >> 4;
	col = num & 0x0F;

	// row * (16 chars per row) * (8*8 pixels per char)
	// col * (8 pixels for char height)
	source = draw_chars[0] + (row << 10) + (col << 3);

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

	// Hack the version number directly into the pic.

	memcpy (saveback, conback->data + 320 * 186, 320 * 8);

	// Draw the pic.
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
			if (CLIP_TOP(prect->y + i) || CLIP_BOTTOM(prect->y + i))
				continue;

			for (j = 0; j < prect->width; j++) 
			{
				if (CLIP_LEFT(prect->x + j) || CLIP_RIGHT(prect->x + j))
					continue;

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
		if (CLIP_TOP(y + cur_y) || CLIP_BOTTOM(y + cur_y))
			continue;

		for (cur_x = 0; cur_x < w; cur_x++)
		{
			if (CLIP_LEFT(x + cur_x) || CLIP_RIGHT(x + cur_x))
				continue;

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

//
// Bresenham's line algorithm - Draws a line effectivly between two points using only integer addition.
// http://en.wikipedia.org/wiki/Bresenham's_line_algorithm
//
static void bresenham_line(int x1, int y1, int x2, int y2, byte color)
{
	int i;
	int dx = x2 - x1;		// The horizontal distance of the line.
	int dy = y2 - y1;		// The vertical distance of the line.
	int dxabs = abs(dx);
	int dyabs = abs(dy);
	int sdx = sgn(dx);
	int sdy = sgn(dy);
	int x = dyabs >> 1;
	int y = dxabs >> 1;
	int px = x1;
	int py = y1;

	Draw_Pixel(px, py, color);

	if (dxabs >= dyabs) 
	{
		// The line is more horizontal than vertical.

		for(i = 0; i < dxabs; i++)
		{
			y += dyabs;

			if (y >= dxabs)
			{
				y -= dxabs;
				py += sdy;
			}

			px += sdx;
			Draw_Pixel(px, py, color);
		}
	}
	else 
	{
		// The line is more vertical than horizontal.

		for(i = 0; i < dyabs; i++)
		{
			x += dxabs;

			if (x >= dyabs)
			{
				x -= dyabs;
				px += sdx;
			}

			py += sdy;
			Draw_Pixel(px, py, color);
		}
	}
}

//
// FIXME: Not a proper thickline algorithm, doesn't draw the ends properly like => http://homepages.enterprise.net/murphy/thickline/index.html
//
static void bresenham_thickline (int x1, int y1, int x2, int y2, int thick, byte color)
{
	int dx, dy, incr1, incr2, d, x, y, xend, yend, xdirflag, ydirflag;
	int wid;
	int w, wstart;

	dx = abs (x2 - x1);
	dy = abs (y2 - y1);

	if (dy <= dx)
	{
		// More-or-less horizontal. use wid for vertical stroke
		if ((dx == 0) && (dy == 0))
		{
			wid = 1;
		}
		else
		{
			double ac = cos (atan2 (dy, dx));		
			wid = (ac != 0) ? (thick / ac) : 1;
	
			if (wid == 0)
			{
				wid = 1;
			}
		}

		d = 2 * dy - dx;
		incr1 = 2 * dy;
		incr2 = 2 * (dy - dx);

		if (x1 > x2)
		{
			x = x2;
			y = y2;
			ydirflag = (-1);
			xend = x1;
		}
		else
		{
			x = x1;
			y = y1;
			ydirflag = 1;
			xend = x2;
		}

		// Set up line thickness.
		wstart = y - wid / 2;
		for (w = wstart; w < wstart + wid; w++)
			Draw_Pixel(x, w, color);

		if (((y2 - y1) * ydirflag) > 0)
		{
			while (x < xend)
			{
				x++;
				if (d < 0)
				{
					d += incr1;
				}
				else
				{
					y++;
					d += incr2;
				}
				
				wstart = y - wid / 2;
				
				for (w = wstart; w < wstart + wid; w++)
					Draw_Pixel(x, w, color);
			}
		}
		else
		{
			while (x < xend)
			{
				x++;
				if (d < 0)
				{
					d += incr1;
				}
				else
				{
					y--;
					d += incr2;
				}
				
				wstart = y - wid / 2;
				
				for (w = wstart; w < wstart + wid; w++)
					Draw_Pixel(x, w, color);
			}
		}
    }
	else
	{
		// More-or-less vertical. use wid for horizontal stroke.
		double as = sin (atan2 (dy, dx));
		wid = (as != 0) ? (thick / as) : 1;
		
		if (wid == 0)
			wid = 1;

		d = 2 * dx - dy;
		incr1 = 2 * dx;
		incr2 = 2 * (dx - dy);
		
		if (y1 > y2)
		{
			y = y2;
			x = x2;
			yend = y1;
			xdirflag = (-1);
		}
		else
		{
			y = y1;
			x = x1;
			yend = y2;
			xdirflag = 1;
		}

		// Set up line thickness.
		wstart = x - wid / 2;
		for (w = wstart; w < wstart + wid; w++)
			Draw_Pixel(w, y, color);

		if (((x2 - x1) * xdirflag) > 0)
		{
			while (y < yend)
			{
				y++;
				if (d < 0)
				{
					d += incr1;
				}
				else
				{
					x++;
					d += incr2;
				}

				wstart = x - wid / 2;
				
				for (w = wstart; w < wstart + wid; w++)
					Draw_Pixel(w, y, color);
			}
		}
		else
		{
			while (y < yend)
			{
				y++;
				if (d < 0)
				{
					d += incr1;
				}
				else
				{
					x--;
					d += incr2;
				}
				
				wstart = x - wid / 2;
				
				for (w = wstart; w < wstart + wid; w++)
					Draw_Pixel(w, y, color);
			}
		}
	}
}

void Draw_AlphaLine (int x_start, int y_start, int x_end, int y_end, float thickness, byte c, float alpha)
{
	if ((int)thickness == 1)
	{
		bresenham_line(x_start, y_start, x_end, y_end, c);
	}
	else
	{
		bresenham_thickline(x_start, y_start, x_end, y_end, Q_rint(thickness), c);
	}
}

#define COLOR_ALPHA(color) ((float)((color >> 24) & 0xFF) / 255.0)

void Draw_AlphaLineRGB (int x_start, int y_start, int x_end, int y_end, float thickness, color_t color)
{
	Draw_AlphaLine(x_start, y_start, x_end, y_end, thickness, Draw_FindNearestColor(color), COLOR_ALPHA(color));
}

void Draw_Polygon(int x, int y, vec3_t *vertices, int num_vertices, qbool fill, color_t color)
{
	// TODO : Implement Draw_Polygon for software.
}

void Draw_AlphaFill (int x, int y, int w, int h, byte c, float alpha)
{
	Draw_FadeBox(x, y, w, h, c, alpha);
}

void Draw_AlphaFillRGB (int x, int y, int width, int height, color_t color)
{
	Draw_FadeBox(x, y, width, height, Draw_FindNearestColor(color), COLOR_ALPHA(color));
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
	Draw_RectStretch(pic, x, y, Q_rint(scale * pic->width), Q_rint(scale * pic->height));
}

void Draw_FitPic (int x, int y, int fit_width, int fit_height, mpic_t *pic)
{
	Draw_RectStretch(pic, x, y, fit_width, fit_height);
}

void Draw_SSubPic(int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height, float scale)
{
	Draw_RectStretchSubPic(pic, x, y, srcx, srcy, width, height, Q_rint(scale * width), Q_rint(scale * height));
}

void Draw_STransPic (int x, int y, mpic_t *pic, float scale)
{
	Draw_RectStretchSubPic(pic, x, y, 0, 0, pic->width, pic->height, Q_rint(scale * pic->width), Q_rint(scale * pic->height));
}

void Draw_SFill (int x, int y, int w, int h, byte c, float scale)
{
    Draw_Fill(x, y, Q_rint(scale * w), Q_rint(scale * h), c);
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
