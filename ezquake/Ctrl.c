//    $Id: Ctrl.c,v 1.5 2006-12-31 05:15:40 cokeman1982 Exp $

#include "quakedef.h"

void UI_DrawCharacter (int cx, int line, int num)
{
	Draw_Character ( cx, line, num);
}

void UI_Print (int cx, int cy, char *str, int red)
{
	if (red)
	{
		while (*str) 
		{
			UI_DrawCharacter (cx, cy, (*str) | (red ? 128 : 0));
			str++;
			cx += 8;
		}
	}
	else
	{
		Draw_ColoredString (cx, cy, str, red);
	}
}

void UI_Print_Center (int cx, int cy, int w, char *str, int red)
{
	// UH!!! wtf.... if I do:
	// UI_Print(cx + (w - 8 * strlen(str)) / 2, cy, str, red);
	// The call to UI_Print will get the first argument all fucked up (-217309285) instead of 192 in one instance.
	// making the line that should be drawn to dissapear?!?!? ...... If I moved the strlen(str) outside of the
	// function call like below it works fine.
	int text_length = strlen(str);
	UI_Print(cx + (w - 8 * text_length) / 2, cy, str, red);
}

void UI_MakeLine(char *buf, int w)
{
	buf[0] = '\x1d';
	memset(buf+1, '\x1e', w-2);
	buf[w-1] = '\x1f';
	buf[w] = 0;
}

void UI_MakeLine2(char *buf, int w)
{
	buf[0] = '\x80';
	memset(buf+1, '\x81', w-2);
	buf[w-1] = '\x82';
	buf[w] = 0;
}
