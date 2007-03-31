/**
    
    GUI Control: ScrollBar

    Include and read Ctrl.h to use scrollbars

    made by:
        johnnycz, Mar 2007
    last edit:
        $Id: Ctrl_ScrollBar.c,v 1.3 2007-03-31 15:24:10 johnnycz Exp $

*/

#include "quakedef.h"
#include "keys.h"
#include "Ctrl.h"

#define SCRBARSCALE 0.25

// In GL shouw .png scrollbar images
mpic_t *scrbar_up, *scrbar_down, *scrbar_bg, *scrbar_slider;
int scrollbar_width;
int slider_height;

void ScrollBars_Init()
{
#ifdef GLQUAKE
	scrbar_bg = Draw_CachePicSafe("textures/scrollbars/slidebg", false, true);
	scrbar_up = Draw_CachePicSafe("textures/scrollbars/arrow_up", false, true);
	scrbar_down = Draw_CachePicSafe("textures/scrollbars/arrow_down", false, true);
    scrbar_slider = Draw_CachePicSafe("textures/scrollbars/slider", false, true);
    if (scrbar_slider) {
        scrollbar_width = scrbar_slider->width * SCRBARSCALE;
        slider_height = scrbar_slider->height * SCRBARSCALE;
    } else
#endif
    {
        scrollbar_width = 8;
        slider_height = 8;
    }
}

PScrollBar ScrollBar_Create(ScrollPos_setter pos_setter)
{
    PScrollBar scb_new = Q_malloc(sizeof(ScrollBar));

    if (!scb_new) return NULL;

    scb_new->curpos = 0;
    scb_new->mouselocked = false;
    scb_new->scroll_fnc = pos_setter;
    scb_new->width = scrollbar_width;
    return scb_new;
}

// scrollbar destructor
void ScrollBar_Delete(PScrollBar scrbar)
{
    Q_free (scrbar);
}

// mouse event handler
qbool ScrollBar_MouseEvent(PScrollBar scrbar, const mouse_state_t *ms)
{
    // check if there is some reason to react on this event
    if (!ms->button_down && !ms->button_up && !ms->buttons[1])
        return false;   

    if (ms->button_up)
    {
        scrbar->mouselocked = false;
    }
    else // button_down or mousemove
    {
        double y = ms->y - scrollbar_width - slider_height/2;
        double ah = scrbar->height - scrollbar_width*2 - slider_height; 

        scrbar->mouselocked = true;
        y = bound(0, y, ah);
        scrbar->curpos = y / ah;
        scrbar->curpos = bound(0, scrbar->curpos, 1);
    }

    if (scrbar->scroll_fnc)
        scrbar->scroll_fnc(scrbar->curpos);

    return true;
}

#ifdef GLQUAKE

static void SCRB_DrawGL(PScrollBar scrbar, int x, int y, int h)
{
    int w = scrollbar_width;
    Draw_Fill(x,y,w,h,4);
    Draw_SPic(x, y, scrbar_up, SCRBARSCALE);
    Draw_SPic(x, y + h - w, scrbar_down, SCRBARSCALE);
    // Draw_SPic(x, y + w, scrbar_bg, SCRBARSCALE);
    Draw_SPic(x, y + w + (h-2*w-slider_height)*scrbar->curpos, scrbar_slider, SCRBARSCALE);
}

#endif

static void SCRB_DrawSW(PScrollBar scrbar, int x, int y, int h)
{
    int w = scrollbar_width;

    Draw_Fill(x,y,w,h,4);
    Draw_Fill(x,y,w,w,72);
    Draw_Fill(x,y+h-w,w,w,72);
    Draw_Fill(x,y+w+(h-3*w)*scrbar->curpos,w,w,40);
}

void ScrollBar_Draw(PScrollBar scrbar, int x, int y, int h)
{
    scrbar->height = h;

// noone coded PNG loading for software yet so we hate to use completely different rendering .. damnit
#ifdef GLQUAKE
    if (scrbar_up)
        SCRB_DrawGL(scrbar, x,y,h);
    else
        SCRB_DrawSW(scrbar, x,y,h);
#else
    SCRB_DrawSW(scrbar, x,y,h);
#endif
}
