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

typedef struct
{
	int			width, height;
	int			texnum;
	float		sl, tl, sh, th;
} mpic_t;

void Draw_AdjustConback (void);

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
void Draw_FadeScreen (float alpha);

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
void Draw_SColoredStringBasic (int x, int y, const char *text, int red, float scale);
void Draw_ColoredString3 (int x, int y, const char *text, clrinfo_t *clr, int clr_cnt, int red);
void Draw_ColoredString3W (int x, int y, const wchar *text, clrinfo_t *clr, int clr_cnt, int red);
void Draw_SColoredString (int x, int y, const wchar *text, clrinfo_t *clr, int clr_cnt, int red, float scale);

mpic_t *Draw_CachePicSafe (const char *path, qbool crash, qbool only24bit);
mpic_t *Draw_CachePic (char *path);
mpic_t *Draw_CacheWadPic (char *name, int code);
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
void Draw_2dAlphaTexture(float x, float y, float width, float height, int texture_num, float alpha);

qbool R_CharAvailable (wchar num);

void Draw_EnableScissorRectangle(int x, int y, int width, int height);
void Draw_EnableScissor(int left, int right, int top, int bottom);
void Draw_DisableScissor(void);

void InitTracker(void);
void Draw_TextCacheFlush(void);

enum {
	WADPIC_RAM = 0,
	WADPIC_NET,
	WADPIC_TURTLE,
	WADPIC_DISC,
	WADPIC_BACKTILE,
	WADPIC_NUM_0,
	WADPIC_NUM_1,
	WADPIC_NUM_2,
	WADPIC_NUM_3,
	WADPIC_NUM_4,
	WADPIC_NUM_5,
	WADPIC_NUM_6,
	WADPIC_NUM_7,
	WADPIC_NUM_8,
	WADPIC_NUM_9,
	WADPIC_ANUM_0,
	WADPIC_ANUM_1,
	WADPIC_ANUM_2,
	WADPIC_ANUM_3,
	WADPIC_ANUM_4,
	WADPIC_ANUM_5,
	WADPIC_ANUM_6,
	WADPIC_ANUM_7,
	WADPIC_ANUM_8,
	WADPIC_ANUM_9,
	WADPIC_NUM_MINUS,
	WADPIC_ANUM_MINUS,
	WADPIC_NUM_COLON,
	WADPIC_NUM_SLASH,
	WADPIC_INV_SHOTGUN,
	WADPIC_INV_SSHOTGUN,
	WADPIC_INV_NAILGUN,
	WADPIC_INV_SNAILGUN,
	WADPIC_INV_RLAUNCH,
	WADPIC_INV_SRLAUNCH,
	WADPIC_INV_LIGHTNG,
	WADPIC_INV2_SHOTGUN,
	WADPIC_INV2_SSHOTGUN,
	WADPIC_INV2_NAILGUN,
	WADPIC_INV2_SNAILGUN,
	WADPIC_INV2_RLAUNCH,
	WADPIC_INV2_SRLAUNCH,
	WADPIC_INV2_LIGHTNG,
	WADPIC_INVA1_SHOTGUN,
	WADPIC_INVA2_SHOTGUN,
	WADPIC_INVA3_SHOTGUN,
	WADPIC_INVA4_SHOTGUN,
	WADPIC_INVA5_SHOTGUN,
	WADPIC_INVA1_SSHOTGUN,
	WADPIC_INVA2_SSHOTGUN,
	WADPIC_INVA3_SSHOTGUN,
	WADPIC_INVA4_SSHOTGUN,
	WADPIC_INVA5_SSHOTGUN,
	WADPIC_INVA1_NAILGUN,
	WADPIC_INVA2_NAILGUN,
	WADPIC_INVA3_NAILGUN,
	WADPIC_INVA4_NAILGUN,
	WADPIC_INVA5_NAILGUN,
	WADPIC_INVA1_SNAILGUN,
	WADPIC_INVA2_SNAILGUN,
	WADPIC_INVA3_SNAILGUN,
	WADPIC_INVA4_SNAILGUN,
	WADPIC_INVA5_SNAILGUN,
	WADPIC_INVA1_RLAUNCH,
	WADPIC_INVA2_RLAUNCH,
	WADPIC_INVA3_RLAUNCH,
	WADPIC_INVA4_RLAUNCH,
	WADPIC_INVA5_RLAUNCH,
	WADPIC_INVA1_SRLAUNCH,
	WADPIC_INVA2_SRLAUNCH,
	WADPIC_INVA3_SRLAUNCH,
	WADPIC_INVA4_SRLAUNCH,
	WADPIC_INVA5_SRLAUNCH,
	WADPIC_INVA1_LIGHTNG,
	WADPIC_INVA2_LIGHTNG,
	WADPIC_INVA3_LIGHTNG,
	WADPIC_INVA4_LIGHTNG,
	WADPIC_INVA5_LIGHTNG,
	WADPIC_SB_SHELLS,
	WADPIC_SB_NAILS,
	WADPIC_SB_ROCKET,
	WADPIC_SB_CELLS,
	WADPIC_SB_ARMOR1,
	WADPIC_SB_ARMOR2,
	WADPIC_SB_ARMOR3,
	WADPIC_SB_KEY1,
	WADPIC_SB_KEY2,
	WADPIC_SB_INVIS,
	WADPIC_SB_INVULN,
	WADPIC_SB_SUIT,
	WADPIC_SB_QUAD,
	WADPIC_SB_SIGIL1,
	WADPIC_SB_SIGIL2,
	WADPIC_SB_SIGIL3,
	WADPIC_SB_SIGIL4,
	WADPIC_SB_FACE1,
	WADPIC_SB_FACE_P1,
	WADPIC_SB_FACE2,
	WADPIC_SB_FACE_P2,
	WADPIC_SB_FACE3,
	WADPIC_SB_FACE_P3,
	WADPIC_SB_FACE4,
	WADPIC_SB_FACE_P4,
	WADPIC_SB_FACE5,
	WADPIC_SB_FACE_P5,
	WADPIC_FACE_INVIS,
	WADPIC_FACE_INVUL2,
	WADPIC_FACE_INV2,
	WADPIC_FACE_QUAD,
	WADPIC_SB_SBAR,
	WADPIC_SB_IBAR,
	WADPIC_SB_SCOREBAR,

	WADPIC_SB_IBAR_AMMO1,
	WADPIC_SB_IBAR_AMMO2,
	WADPIC_SB_IBAR_AMMO3,
	WADPIC_SB_IBAR_AMMO4,

	WADPIC_PIC_COUNT
};

typedef struct wadpic_s {
	char name[32];
	mpic_t* pic;
} wadpic_t;

extern wadpic_t wad_pictures[WADPIC_PIC_COUNT];

void CachePics_CreateAtlas(void);

#endif // __DRAW_H__




