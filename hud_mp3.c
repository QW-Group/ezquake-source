/*
Copyright (C) 2011 azazello and ezQuake team

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

#include "quakedef.h"
#include "common_draw.h"
#include "hud.h"
#include "hud_common.h"
#include "mp3_player.h"
#include "rulesets.h"
#include "utils.h"

// ================================================================
// Draws a word wrapped scrolling string inside the given bounds.
// ================================================================
void SCR_DrawWordWrapString(int x, int y, int y_spacing, int width, int height, int wordwrap, int scroll, double scroll_delay, char *txt, float scale, qbool proportional)
{
	int wordlen = 0;                         // Length of words.
	int cur_x = -1;                          // the current x position.
	int cur_y = 0;                           // current y position.
	char c;                                  // Current char.
	int width_as_text = Draw_CharacterFit(txt, width, scale, false); // How many chars that fits the given width.
	int height_as_rows = 0;                  // How many rows that fits the given height.

	// Scroll variables.
	double t;                           // Current time.
	static double t_last_scroll = 0;	// Time at the last scroll.
	static int scroll_position = 0; 	// At what index to start printing the string.
	static int scroll_direction = 1;	// The direction the string is scrolling.
	static int last_text_length = 0;	// The last text length.
	int text_length = 0;                // The current text length.

	// Don't print all the char's ontop of each other :)
	if (!y_spacing) {
		y_spacing = 8 * scale;
	}

	height_as_rows = height / y_spacing;

	// Scroll the text.
	if (scroll) {
		text_length = strlen(txt);

		// If the text has changed since last time we scrolled
		// the scroll data will be invalid so reset it.
		if (last_text_length != text_length) {
			scroll_position = 0;
			scroll_direction = 1;
			last_text_length = text_length;
		}

		// Check if the text is longer that can fit inside the given bounds.
		if ((wordwrap && text_length > (width_as_text * height_as_rows)) // For more than one row.
			|| (!wordwrap && text_length > width_as_text)) // One row.
		{
			// Set what position to start printing the string at.
			if ((wordwrap && text_length - scroll_position >= width_as_text * height_as_rows) // More than one row.
				|| (!wordwrap && (text_length - scroll_position) >= width_as_text)) // One row.
			{
				txt += scroll_position;
			}

			// Get the time.
			t = Sys_DoubleTime();

			// Only advance the scroll position every n:th second.
			if ((t - t_last_scroll) > scroll_delay) {
				t_last_scroll = t;

				if (scroll_direction) {
					scroll_position++;
				}
				else {
					scroll_position--;
				}
			}

			// Set the scroll direction.
			if ((wordwrap && (text_length - scroll_position) == width_as_text * height_as_rows)
				|| (!wordwrap && (text_length - scroll_position) == width_as_text)) {
				scroll_direction = 0; // Left
			}
			else if (scroll_position == 0) {
				scroll_direction = 1; // Right
			}
		}
	}

	// Print the string.
	while ((c = *txt)) {
		txt++;

		if (wordwrap == 1) {
			// count the word length.
			for (wordlen = 0; wordlen < width_as_text; wordlen++) {
				// Make sure the char isn't above 127.
				char ch = txt[wordlen] & 127;
				if ((wordwrap && (!txt[wordlen] || ch == 0x09 || ch == 0x0D || ch == 0x0A || ch == 0x20)) || (!wordwrap && txt[wordlen] <= ' ')) {
					break;
				}
			}
		}

		// Wordwrap if the word doesn't fit inside the textbox.
		if (wordwrap == 1 && (wordlen != width_as_text) && ((cur_x + wordlen) >= width_as_text)) {
			cur_x = 0;
			cur_y += y_spacing;
		}

		switch (c) {
			case '\n':
			case '\r':
				// Next line for newline and carriage return.
				cur_x = 0;
				cur_y += y_spacing;
				break;
			default:
				// Move the position to draw at forward.
				cur_x++;
				break;
		}

		// Check so that the draw position isn't outside the textbox.
		if (cur_x >= width_as_text) {
			// Don't change row if wordwarp is enabled.
			if (!wordwrap) {
				return;
			}

			cur_x = 0;
			cur_y += y_spacing;
		}

		// Make sure the new line is within the bounds.
		if (cur_y < height) {
			Draw_SCharacter(x + (cur_x) * 8 * scale, y + cur_y, c, scale);
		}
	}
}

static char *Get_MP3_HUD_style(int style, char *st)
{
	static char HUD_style[32];

	memset(HUD_style, 0, sizeof(HUD_style));
	if (style == 1) {
		strlcpy(HUD_style, st, sizeof(HUD_style));
		strlcat(HUD_style, ":", sizeof(HUD_style));
	}
	else if (style == 2) {
		strlcpy(HUD_style, "\x10", sizeof(HUD_style));
		strlcpy(HUD_style, st, sizeof(HUD_style));
		strlcpy(HUD_style, "\x11", sizeof(HUD_style));
	}
	return HUD_style;
}

// Draws MP3 Title.
static void SCR_HUD_DrawMP3_Title(hud_t *hud)
{
	int x = 0, y = 0/*, n=1*/;
	int width = 64;
	int height = 8;

#ifdef WITH_MP3_PLAYER
	//int width_as_text = 0;
	static int title_length = 0;
	//int row_break = 0;
	//int i=0;
	int status = 0;
	static char title[MP3_MAXSONGTITLE];
	double t;		// current time
	static double lastframetime;	// last refresh

	static cvar_t *style = NULL,
		*width_var, *height_var, *scroll, *scroll_delay,
		*on_scoreboard, *wordwrap, *scale, *proportional;

	if (style == NULL)  // first time called
	{
		style = HUD_FindVar(hud, "style");
		width_var = HUD_FindVar(hud, "width");
		height_var = HUD_FindVar(hud, "height");
		scroll = HUD_FindVar(hud, "scroll");
		scroll_delay = HUD_FindVar(hud, "scroll_delay");
		on_scoreboard = HUD_FindVar(hud, "on_scoreboard");
		wordwrap = HUD_FindVar(hud, "wordwrap");
		scale = HUD_FindVar(hud, "scale");
		proportional = HUD_FindVar(hud, "proportional");
	}

	if (on_scoreboard->value) {
		hud->flags |= HUD_ON_SCORES;
	}
	else if ((int)on_scoreboard->value & HUD_ON_SCORES) {
		hud->flags -= HUD_ON_SCORES;
	}

	width = (int)width_var->value * scale->value;
	height = (int)height_var->value * scale->value;

	if (width < 0) width = 0;
	if (width > vid.width) width = vid.width;
	if (height < 0) height = 0;
	if (height > vid.width) height = vid.height;

	t = Sys_DoubleTime();

	if ((t - lastframetime) >= 2) { // 2 sec refresh rate
		lastframetime = t;
		status = MP3_GetStatus();

		switch (status) {
			case MP3_PLAYING:
				title_length = snprintf(title, sizeof(title) - 1, "%s %s", Get_MP3_HUD_style(style->integer, "Playing"), MP3_Macro_MP3Info());
				break;
			case MP3_PAUSED:
				title_length = snprintf(title, sizeof(title) - 1, "%s %s", Get_MP3_HUD_style(style->integer, "Paused"), MP3_Macro_MP3Info());
				break;
			case MP3_STOPPED:
				title_length = snprintf(title, sizeof(title) - 1, "%s %s", Get_MP3_HUD_style(style->integer, "Stopped"), MP3_Macro_MP3Info());
				break;
			case MP3_NOTRUNNING:
			default:
				status = MP3_NOTRUNNING;
				title_length = snprintf(title, sizeof(title), "%s is not running.", mp3_player->PlayerName_AllCaps);
				break;
		}

		if (title_length < 0) {
			snprintf(title, sizeof(title), "Error retrieving current song.");
		}
	}

	if (HUD_PrepareDraw(hud, width, height, &x, &y)) {
		SCR_DrawWordWrapString(x, y, 8 * scale->value, width, height, (int)wordwrap->value, (int)scroll->value, (float)scroll_delay->value, title, scale->value, proportional->integer);
	}
#else
	HUD_PrepareDraw(hud, width, height, &x, &y);
#endif
}

// Draws MP3 Time as a HUD-element.
static void SCR_HUD_DrawMP3_Time(hud_t *hud)
{
	int x = 0, y = 0, width = 0, height = 0;
#ifdef WITH_MP3_PLAYER
	int elapsed = 0;
	int remain = 0;
	int total = 0;
	static char time_string[MP3_MAXSONGTITLE];
	static char elapsed_string[MP3_MAXSONGTITLE];
	double t; // current time
	static double lastframetime; // last refresh

	static cvar_t *style = NULL, *on_scoreboard, *scale;

	if (style == NULL) {
		style = HUD_FindVar(hud, "style");
		on_scoreboard = HUD_FindVar(hud, "on_scoreboard");
		scale = HUD_FindVar(hud, "scale");
	}

	if (on_scoreboard->value) {
		hud->flags |= HUD_ON_SCORES;
	}
	else if ((int)on_scoreboard->value & HUD_ON_SCORES) {
		hud->flags -= HUD_ON_SCORES;
	}

	t = Sys_DoubleTime();
	if ((t - lastframetime) >= 2) { // 2 sec refresh rate
		lastframetime = t;

		if (!MP3_GetOutputtime(&elapsed, &total) || elapsed < 0 || total < 0) {
			snprintf(time_string, sizeof(time_string), "\x10-:-\x11");
		}
		else {
			switch ((int)style->value) {
				case 1:
					remain = total - elapsed;
					strlcpy(elapsed_string, SecondsToMinutesString(remain), sizeof(elapsed_string));
					snprintf(time_string, sizeof(time_string), "\x10-%s/%s\x11", elapsed_string, SecondsToMinutesString(total));
					break;
				case 2:
					remain = total - elapsed;
					snprintf(time_string, sizeof(time_string), "\x10-%s\x11", SecondsToMinutesString(remain));
					break;
				case 3:
					snprintf(time_string, sizeof(time_string), "\x10%s\x11", SecondsToMinutesString(elapsed));
					break;
				case 4:
					remain = total - elapsed;
					strlcpy(elapsed_string, SecondsToMinutesString(remain), sizeof(elapsed_string));
					snprintf(time_string, sizeof(time_string), "%s/%s", elapsed_string, SecondsToMinutesString(total));
					break;
				case 5:
					strlcpy(elapsed_string, SecondsToMinutesString(elapsed), sizeof(elapsed_string));
					snprintf(time_string, sizeof(time_string), "-%s/%s", elapsed_string, SecondsToMinutesString(total));
					break;
				case 6:
					remain = total - elapsed;
					snprintf(time_string, sizeof(time_string), "-%s", SecondsToMinutesString(remain));
					break;
				case 7:
					snprintf(time_string, sizeof(time_string), "%s", SecondsToMinutesString(elapsed));
					break;
				case 0:
				default:
					strlcpy(elapsed_string, SecondsToMinutesString(elapsed), sizeof(elapsed_string));
					snprintf(time_string, sizeof(time_string), "\x10%s/%s\x11", elapsed_string, SecondsToMinutesString(total));
					break;
			}
		}

	}

	// Don't allow showing the timer if ruleset disallows it
	// It could be used for timing powerups
	// Use same check that is used for any external communication
	if (Rulesets_RestrictPacket()) {
		snprintf(time_string, sizeof(time_string), "\x10%s\x11", "Not allowed");
	}

	width = strlen(time_string) * 8 * scale->value;
	height = 8 * scale->value;

	if (HUD_PrepareDraw(hud, width, height, &x, &y)) {
		Draw_SString(x, y, time_string, scale->value, false);
	}
#else
	HUD_PrepareDraw(hud, width, height, &x, &y);
#endif
}

void MP3_HudInit(void)
{
	HUD_Register(
		"mp3_title", NULL, "Shows current mp3 playing.",
		HUD_PLUSMINUS, ca_disconnected, 0, SCR_HUD_DrawMP3_Title,
		"0", "top", "right", "bottom", "0", "0", "0", "0 0 0", NULL,
		"style", "2",
		"width", "512",
		"height", "8",
		"scroll", "1",
		"scroll_delay", "0.5",
		"on_scoreboard", "0",
		"wordwrap", "0",
		"scale", "1",
		"proportional", "0",
		NULL
	);

	HUD_Register(
		"mp3_time", NULL, "Shows the time of the current mp3 playing.",
		HUD_PLUSMINUS, ca_disconnected, 0, SCR_HUD_DrawMP3_Time,
		"0", "top", "left", "bottom", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"on_scoreboard", "0",
		"scale", "1",
		"proportional", "0",
		NULL
	);
}
