//    $Id: Ctrl.h,v 1.14 2007-07-10 21:20:11 cokeman1982 Exp $

#ifndef __CTRL_H__
#define __CTRL_H__

#include "keys.h"

// this can be used to determine text block proportions
#define LETTERWIDTH 8
#define LETTERHEIGHT 8

//
// common GUI drawing methods
//
void UI_DrawCharacter (int cx, int line, int num);
void UI_Print (int cx, int cy, const char *str, int red);
int UI_DrawSlider (int x, int y, float range);
int UI_SliderWidth(void);
void UI_Print3 (int cx, int cy, const char *str, clrinfo_t *clr, int clr_cnt, int red);
void UI_Print_Center (int cx, int cy, int w, const char *str, int red);
void UI_Print_Center3 (int cx, int cy, int w, char *str, clrinfo_t *clr, int clr_cnt, int red);
void UI_DrawTextBox (int x, int y, int width, int lines);
void UI_MakeLine(char *buf, int w);
void UI_MakeLine2(char *buf, int w);
void UI_DrawColoredAlphaBox(int x, int y, int w, int h, color_t color);
void UI_DrawGrayBox(int x, int y, int w, int h);
void UI_DrawBox(int x, int y, int w, int h);

// prints the text into a box given by the area definitions (x,y, width, height)
// returned value determines, if the text did fit in the box
qbool UI_PrintTextBlock(int x, int y, int w, int h, const char* text, qbool red);

//
// <scrollbar>
// if this part gets too big, it might be moved to own header (Ctrl_*.h)
//

void ScrollBars_Init();

typedef void (*ScrollPos_setter) (double);

typedef struct ScrollBar_s {
    // current scrolling position in percentage 0-100%
    double curpos;                      

    // a function to which the scrollbar should report new scroll position
    ScrollPos_setter    scroll_fnc;     

    // should the scrollbar grab the mouse?
    // used to be able to scroll even if the mouse is a bit off
    qbool mouselocked;

    // scrollbar width and height
    int width, height;
} ScrollBar, *PScrollBar;

// scrollbar constructor
// 1st parameter: a function it should call, can be NULL
PScrollBar ScrollBar_Create(ScrollPos_setter);

// scrollbar destructor
void ScrollBar_Delete(PScrollBar);

// mouse event handler
// returns true if scrollbar changed it's position
qbool ScrollBar_MouseEvent(PScrollBar, const mouse_state_t*);

// scrollbar draw - starting point (x,y) and scrollbar's height (width is given internally)
void ScrollBar_Draw(PScrollBar, int x, int y, int h);

//
// </scrollbar>
//


#endif // __CTRL_H__
