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
#ifndef __COMMON_DRAW__H__
#define __COMMON_DRAW__H__

#define TIMETYPE_CLOCK 0
#define TIMETYPE_GAMECLOCK 1
#define TIMETYPE_GAMECLOCKINV 2
#define TIMETYPE_DEMOCLOCK 3

void CommonDraw_Init(void);

typedef struct cachepic_s 
{
	char		name[MAX_QPATH];
	mpic_t		*pic;
} cachepic_t;

typedef struct cachepic_node_s 
{
	cachepic_t data;
	unsigned int refcount;
	struct cachepic_node_s *next;
} cachepic_node_t;

#define	CACHED_PICS_HDSIZE		64

mpic_t *CachePic_Find(const char *path, qbool inc_refcount);
mpic_t* CachePic_Add(const char *path, mpic_t *pic);
qbool CachePic_Remove(const char *path);
void CachePics_DeInit(void);

int SCR_GetClockStringWidth(const char *s, qbool big, float scale);
int SCR_GetClockStringHeight(qbool big, float scale);
const char* SCR_GetTimeString(int timetype, const char *format);
void SCR_DrawBigClock(int x, int y, int style, int blink, float scale, const char *t);
void SCR_DrawSmallClock(int x, int y, int style, int blink, float scale, const char *t);
void SCR_NetStats(int x, int y, float period);
void SCR_DrawHUDSpeed (int x, int y, int width, int height, 
	int type, 
	float tick_spacing, 
	float opacity,
	int vertical,
	int vertical_text,
	int text_align,
	byte color_stopped,
	byte color_normal,
	byte color_fast,
	byte color_fastest,
	byte color_insane,
	int style,
	float scale
);

void Draw_FitAlphaSubPic (int x, int y, int target_width, int target_height, 
						  mpic_t *gl, int srcx, int srcy, int src_width, int src_height, float alpha);

void Draw_SubPicTiled(int x, int y, 
					int target_width, int target_height, 
					mpic_t *pic, int src_x, int src_y, 
					int src_width, int src_height,
					float alpha);

void SCR_DrawWordWrapString(
	int x, int y, 
	int y_spacing, 
	int width, int height, 
	int wordwrap, 
	int scroll, 
	double scroll_delay, 
	char *txt,
	float scale
);

void HUD_BeforeDraw(void);
void HUD_AfterDraw(void);

qbool Draw_BigFontAvailable(void);

int *gameclockoffset;  // hud_gameclock time offset in seconds

void SCR_DrawWadString(int x, int y, float scale, const char *t);
void SCR_HUD_DrawBar(int direction, int value, float max_value, byte *color, int x, int y, int width, int height);

#endif  // __COMMON_DRAW__H__
