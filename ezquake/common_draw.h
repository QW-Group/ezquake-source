#ifndef __COMMON_DRAW__H__
#define __COMMON_DRAW__H__


void CommonDraw_Init(void);

//int SCR_DrawDemoStatus(void);
//void PrepareCrosshair(int num, byte tab[10][10]);
//void SCR_DrawClients(void);

void SCR_DrawBigClock(int x, int y, int style, int blink);
void SCR_DrawSmallClock(int x, int y, int style, int blink);
void SCR_NetStats(int x, int y, float period);
void SCR_DrawSpeed2 (int x, int y, int type);


#endif  // __COMMON_GRAPH__H__
