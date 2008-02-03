//    $Id: Ctrl.c,v 1.20 2007-07-11 23:07:34 cokeman1982 Exp $

#include "quakedef.h"
#include "utils.h"
#include "Ctrl.h"


#ifdef GLQUAKE
cvar_t     menu_marked_bgcolor = {"menu_marked_bgcolor", "20 20 20 128", CVAR_COLOR};
cvar_t     menu_marked_fade = {"menu_marked_fade", "4"};
#else
cvar_t     menu_marked_bgcolor = {"menu_marked_bgcolor", "0"};
#endif


#define SLIDER_RANGE 12

void UI_DrawCharacter (int cx, int line, int num)
{
	Draw_Character ( cx, line, num);
}

void UI_Print (int cx, int cy, const char *str, int red)
{
	// We want to draw the charset red, if colored text is turned on
	// Draw_ColoredString will only draw white chars and color them.
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

int UI_DrawSlider (int x, int y, float range) {
	int    i;

	range = bound(0, range, 1);
	UI_DrawCharacter (x-8, y, 128);
	for (i=0 ; i<SLIDER_RANGE ; i++)
		UI_DrawCharacter (x + i*8, y, 129);
	UI_DrawCharacter (x+i*8, y, 130);
	UI_DrawCharacter (x + (SLIDER_RANGE-1)*8 * range, y, 131);
	return x+(i+1)*8;
}

int UI_SliderWidth(void) { return (SLIDER_RANGE+1)*LETTERWIDTH; }

void UI_Print3 (int cx, int cy, const char *str, clrinfo_t *clr, int clr_cnt, int red)
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
		Draw_ColoredString3 (cx, cy, str, clr, clr_cnt, red);
	}
}

void UI_Print_Center (int cx, int cy, int w, const char *str, int red)
{
	// UH!!! wtf.... if I do:
	// UI_Print(cx + (w - 8 * strlen(str)) / 2, cy, str, red);
	// The call to UI_Print will get the first argument all fucked up (-217309285) instead of 192 in one instance.
	// making the line that should be drawn to dissapear?!?!? ...... If I moved the strlen(str) outside of the
	// function call like below it works fine.
	int text_length = strlen(str);
	UI_Print(cx + (w - 8 * text_length) / 2, cy, str, red);
}

void UI_Print_Center3 (int cx, int cy, int w, char *str, clrinfo_t *clr, int clr_cnt, int red)
{
	int text_length = strlen(str);
	UI_Print3(cx + (w - 8 * text_length) / 2, cy, str, clr, clr_cnt, red);
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

void UI_DrawColoredAlphaBox(int x, int y, int w, int h, color_t color)
{
	Draw_AlphaFillRGB(x, y, w, h, color);
}

void UI_DrawGrayBox(int x, int y, int w, int h)
{	// ridiculous function, eh?
	byte c[4];
	float fade = 1;

	memcpy(c, menu_marked_bgcolor.color, sizeof(byte) * 4);
#ifdef GLQUAKE
	if (menu_marked_fade.value) 
	{
		fade = 0.5 * (1.0 + sin(menu_marked_fade.value * Sys_DoubleTime())); // this give us sinusoid from 0 to 1
		fade = 0.5 + (1.0 - 0.5) * fade ; // this give us sinusoid from 0.5 to 1, so no zero alpha
		fade = bound(0, fade, 1); // guarantee sanity if we mess somewhere
	}
#endif

	UI_DrawColoredAlphaBox(x, y, w, h, RGBA_TO_COLOR((fade * c[0]), (fade * c[1]), (fade * c[2]), c[3]));
}

void UI_DrawBox(int x, int y, int w, int h)
{
	Draw_TextBox(x, y, w / 8 - 1, h / 8 - 1);
}

qbool UI_PrintTextBlock(int x, int y, int w, int h, const char* text, qbool red)
{
	int letters_per_line = w / 8;	// Letters per line.
	int lines = h / 8;				// Lines in the text block.
	int linelen = 0;				// Current line length.
	int linecount = 0;				// Current line.
	char buf[1024];

	const char *start = text, *end, *wordend;

	while (*start && linecount < lines) 
	{
		wordend = start;
		end = wordend;
		linelen = 0;

		// Read the next line.
		while (linelen < letters_per_line && *wordend) 
		{
			end = wordend;

			// Read the next word.
			do 
			{
				wordend++;
				linelen++;
			}
			while (*wordend && *wordend != ' ' && *wordend != '\n');
			
			// Make sure we didn't find a word that was too long and
			// caused us to go outside of the line boundary.
			if(linelen >= letters_per_line)
			{
				// Try to find the last word boundary (whis
				do
				{
					wordend--;
					linelen--;
				}
				while(*wordend && *wordend != ' ' && linelen > 0);

				// Check if failed to find a word boundary. 
				// If that's the case just break the line at the max letters allowed for a line.
				// Otherwise break at the last word boundary.
				end = (linelen == 0) ? (end + letters_per_line - 1) : wordend;
				break;
			}

			// Check if it's time for a new line.
			if (*wordend == '\n') 
			{
				end = wordend; 
				break; 
			}
		}
		
		// End of string found.
		if (!*wordend && linelen < letters_per_line)
		{
			end = wordend;
		}

		// Break the line after the word, not after the next whitespace
		// so the next line won't start with that whitespace.
		while (*end && *end == ' ')
		{
			end++;
		}

		// Make sure we consume any newline character before printing.
		if(*start && *start == '\n')
		{
			start++;
		}

		strlcpy(buf, start, end - start + 1);
		UI_Print(x, y + (linecount * 8), buf, red);
		linecount++;
		start = end;
	}

	// If the text didn't fit in there, *start won't be zero.
	return !(*start);
}
