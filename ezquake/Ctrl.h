//    $Id: Ctrl.h,v 1.8 2007-03-06 15:52:06 johnnycz Exp $

#ifndef __CTRL_H__
#define __CTRL_H__

#define LETTERWIDTH 8
#define LETTERHEIGHT 8

void UI_DrawCharacter (int cx, int line, int num);
void UI_Print (int cx, int cy, const char *str, int red);
int UI_DrawSlider (int x, int y, float range);
void UI_Print3 (int cx, int cy, char *str, clrinfo_t *clr, int clr_cnt, int red);
void UI_Print_Center (int cx, int cy, int w, const char *str, int red);
void UI_Print_Center3 (int cx, int cy, int w, char *str, clrinfo_t *clr, int clr_cnt, int red);
void UI_DrawTextBox (int x, int y, int width, int lines);
void UI_MakeLine(char *buf, int w);
void UI_MakeLine2(char *buf, int w);
void UI_DrawGrayBox(int x, int y, int w, int h);
void UI_DrawBox(int x, int y, int w, int h);

// prints the text into a box given by the area definitions (x,y, width, height)
// returned value determines, if the text did fit in the box
qbool UI_PrintTextBlock(int x, int y, int w, int h, const char* text, qbool red);

#endif // __CTRL_H__
