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
	struct cachepic_node_s *next;
} cachepic_node_t;

#define	CACHED_PICS_HDSIZE		64

mpic_t *CachePic_Find(const char *path);
mpic_t* CachePic_Add(const char *path, mpic_t *pic);
void CachePics_DeInit(void);

//int SCR_DrawDemoStatus(void);
//void PrepareCrosshair(int num, byte tab[10][10]);
//void SCR_DrawClients(void);

void SCR_DrawBigClock(int x, int y, int style, int blink, float scale, int gametime);
void SCR_DrawSmallClock(int x, int y, int style, int blink, float scale, int gametime);
void SCR_NetStats(int x, int y, float period);
void SCR_DrawHUDSpeed (int x, int y, int width, int height, 
					 int type, 
					 float tick_spacing, 
					 float opacity,
					 int vertical,
					 int vertical_text,
					 int text_align,
					 int color_stopped,
					 int color_normal,
					 int color_fast,
					 int color_fastest,
					 int color_insane);
void SCR_DrawWordWrapString(int x, int y, 
							int y_spacing, 
							int width, int height, 
							int wordwrap, 
							int scroll, 
							double scroll_delay, 
							char *txt);

void HUD_BeforeDraw();
void HUD_AfterDraw();

#endif  // __COMMON_DRAW__H__
