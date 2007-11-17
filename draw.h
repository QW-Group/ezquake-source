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

// draw.h -- these are the only functions outside the refresh allowed
// to touch the vid buffer

#ifndef __DRAW_H__
#define __DRAW_H__

#ifdef GLQUAKE

#define	MAX_SCRAPS		2

#define MAX_CHARSETS 16
extern int		char_textures[MAX_CHARSETS];

typedef struct
{
	int			width, height;
	int			texnum;
	float		sl, tl, sh, th;
} mpic_t;

void Draw_AdjustConback (void);

#else
typedef struct
{
	int			width;
	short		height;
	byte		alpha;
	byte		pad;
	byte		data[4];	// variable sized
} mpic_t;
#endif // GLQUAKE

extern	mpic_t		*draw_disc;	// also used on sbar

#define MCHARSET_PATH "gfx/mcharset.png"

typedef int color_t;

extern const color_t COLOR_WHITE;

color_t RGBA_TO_COLOR(byte r, byte g, byte b, byte a);
color_t RGBAVECT_TO_COLOR(byte rgba[4]);
byte* COLOR_TO_RGBA(int i, byte rgba[4]);
void Draw_SetOverallAlpha(float opacity);

void Draw_Init (void);
void Draw_Character (int x, int y, int num);
void Draw_CharacterW (int x, int y, wchar num);
void Draw_SubPic(int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height);
void Draw_Pic (int x, int y, mpic_t *pic);
void Draw_TransPic (int x, int y, mpic_t *pic);
void Draw_TransSubPic (int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height);
void Draw_ConsoleBackground (int lines);
void Draw_BeginDisc (void);
void Draw_EndDisc (void);
void Draw_TileClear (int x, int y, int w, int h);
void Draw_Fill (int x, int y, int w, int h, byte c);
void Draw_FadeScreen (void);

typedef struct clrinfo_s
{
	color_t c;	// Color.
	int i;		// Index when this colors starts.
} clrinfo_t;

void Draw_GetBigfontSourceCoords(char c, int char_width, int char_height, int *sx, int *sy);
void Draw_BigString (int x, int y, const char *text, clrinfo_t *color, int color_count, float scale, float alpha, int char_gap);
void Draw_String (int x, int y, const char *str);
void Draw_StringW (int x, int y, const wchar *ws);
void Draw_Alt_String (int x, int y, const char *str);
void Draw_ColoredString (int x, int y, const char *str, int red);
void Draw_ColoredString2 (int x, int y, const char *text, int *clr, int red);
void Draw_ColoredString3 (int x, int y, const char *text, clrinfo_t *clr, int clr_cnt, int red);
void Draw_ColoredString3W (int x, int y, const wchar *text, clrinfo_t *clr, int clr_cnt, int red);
void Draw_SColoredString (int x, int y, const wchar *text, clrinfo_t *clr, int clr_cnt, int red, float scale);

mpic_t *Draw_CachePicSafe (const char *path, qbool crash, qbool only24bit);
mpic_t *Draw_CachePic (char *path);
mpic_t *Draw_CacheWadPic (char *name);
void Draw_Crosshair(void);
void Draw_TextBox (int x, int y, int width, int lines);

void Draw_BigCharacter(int x, int y, char c, color_t color, float scale, float alpha);
void Draw_SColoredCharacterW (int x, int y, wchar num, color_t color, float scale);
void Draw_SCharacter (int x, int y, int num, float scale);
void Draw_SString (int x, int y, const char *str, float scale);
void Draw_SAlt_String (int x, int y, const char *text, float scale);
void Draw_SPic (int x, int y, mpic_t *, float scale);
void Draw_FitPic (int x, int y, int fit_width, int fit_height, mpic_t *gl); // Will fit image into given area; will keep it's proportions.
void Draw_SAlphaPic (int x, int y, mpic_t *, float alpha, float scale);
void Draw_SSubPic(int x, int y, mpic_t *, int srcx, int srcy, int width, int height, float scale);
void Draw_STransPic (int x, int y, mpic_t *, float scale);
void Draw_SFill (int x, int y, int w, int h, byte c, float scale);

void Draw_AlphaPieSliceRGB (int x, int y, float radius, float startangle, float endangle, float thickness, qbool fill, color_t color);
void Draw_AlphaPieSlice (int x, int y, float radius, float startangle, float endangle, float thickness, qbool fill, byte c, float alpha);

void Draw_AlphaCircleRGB (int x, int y, float radius, float thickness, qbool fill, color_t color);
void Draw_AlphaCircle (int x, int y, float radius, float thickness, qbool fill, byte c, float alpha);
void Draw_AlphaCircleOutlineRGB (int x, int y, float radius, float thickness, color_t color);
void Draw_AlphaCircleOutline (int x, int y, float radius, float thickness, byte color, float alpha);
void Draw_AlphaCircleFillRGB (int x, int y, float radius, color_t color);
void Draw_AlphaCircleFill (int x, int y, float radius, byte color, float alpha);

void Draw_AlphaLineRGB (int x_start, int y_start, int x_end, int y_end, float thickness, color_t color);
void Draw_AlphaLine (int x_start, int y_start, int x_end, int y_end, float thickness, byte c, float alpha);

void Draw_AlphaFill (int x, int y, int w, int h, byte c, float alpha);
void Draw_AlphaRectangleRGB (int x, int y, int w, int h, float thickness, qbool fill, color_t color);
void Draw_AlphaFillRGB (int x, int y, int w, int h, color_t color);

void Draw_AlphaSubPic (int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height, float alpha);
void Draw_SAlphaSubPic (int x, int y, mpic_t *pic, int src_x, int src_y, int src_width, int src_height, float scale, float alpha);
void Draw_SAlphaSubPic2 (int x, int y, mpic_t *pic, int src_x, int src_y, int src_width, int src_height, float scale_x, float scale_y, float alpha);
void Draw_AlphaPic (int x, int y, mpic_t *pic, float alpha);

qbool R_CharAvailable (wchar num);

void Draw_EnableScissorRectangle(int x, int y, int width, int height);
void Draw_EnableScissor(int left, int right, int top, int bottom);
void Draw_DisableScissor();

void InitTracker(void);

#endif // __DRAW_H__




