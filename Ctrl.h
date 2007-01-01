//    $Id: Ctrl.h,v 1.4 2007-01-01 07:16:23 qqshka Exp $

#ifndef __CTRL_H__
#define __CTRL_H__


void UI_DrawCharacter (int cx, int line, int num);
void UI_Print (int cx, int cy, char *str, int red);
void UI_Print3 (int cx, int cy, char *str, clrinfo_t *clr, int clr_cnt, int red);
void UI_Print_Center (int cx, int cy, int w, char *str, int red);
void UI_Print_Center3 (int cx, int cy, int w, char *str, clrinfo_t *clr, int clr_cnt, int red);
void UI_DrawTextBox (int x, int y, int width, int lines);
void UI_MakeLine(char *buf, int w);
void UI_MakeLine2(char *buf, int w);

#endif // __CTRL_H__
