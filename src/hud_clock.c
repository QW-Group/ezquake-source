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

// HUD: clock, democlock, gameclock etc

#include "quakedef.h"
#include "common_draw.h"
#include "hud.h"
#include "hud_common.h"
#include "utils.h"
#include "fonts.h"

void SCR_DrawBigClock(int x, int y, int style, int blink, float scale, const char *t);
void SCR_DrawSmallClock(int x, int y, int style, int blink, float scale, const char *t, qbool proportional);

extern cvar_t scr_newHud;

static const char *SCR_HUD_ClockFormat(int format)
{
	switch (format) {
		case 1: return "%I:%M %p";
		case 2: return "%I:%M:%S %p";
		case 3: return "%H:%M";
		default: case 0: return "%H:%M:%S";
	}
}

//---------------------
//
// draw HUD clock
//
static void SCR_HUD_DrawClock(hud_t *hud)
{
	int width, height;
	int x, y;
	const char *t;

	static cvar_t
		*hud_clock_big = NULL,
		*hud_clock_style,
		*hud_clock_blink,
		*hud_clock_scale,
		*hud_clock_format,
		*hud_clock_proportional,
		*hud_clock_content;

	if (hud_clock_big == NULL) {
		// first time
		hud_clock_big = HUD_FindVar(hud, "big");
		hud_clock_style = HUD_FindVar(hud, "style");
		hud_clock_blink = HUD_FindVar(hud, "blink");
		hud_clock_scale = HUD_FindVar(hud, "scale");
		hud_clock_format = HUD_FindVar(hud, "format");
		hud_clock_proportional = HUD_FindVar(hud, "proportional");
		hud_clock_content = HUD_FindVar(hud, "content");
	}

	if (hud_clock_content->integer == 1) {
		t = SCR_GetTimeString(TIMETYPE_HOSTCLOCK, SCR_HUD_ClockFormat(hud_clock_format->integer));
	}
	else if (hud_clock_content->integer == 2) {
		t = SCR_GetTimeString(TIMETYPE_CONNECTEDCLOCK, SCR_HUD_ClockFormat(hud_clock_format->integer));
	}
	else {
		t = SCR_GetTimeString(TIMETYPE_CLOCK, SCR_HUD_ClockFormat(hud_clock_format->integer));
	}
	width = SCR_GetClockStringWidth(t, hud_clock_big->integer, hud_clock_scale->value, hud_clock_proportional->integer);
	height = SCR_GetClockStringHeight(hud_clock_big->integer, hud_clock_scale->value);

	if (HUD_PrepareDraw(hud, width, height, &x, &y)) {
		if (hud_clock_big->value) {
			SCR_DrawBigClock(x, y, hud_clock_style->value, hud_clock_blink->value, hud_clock_scale->value, t);
		}
		else {
			SCR_DrawSmallClock(x, y, hud_clock_style->value, hud_clock_blink->value, hud_clock_scale->value, t, hud_clock_proportional->integer);
		}
	}
}

//---------------------
//
// draw HUD gameclock
//
static void SCR_HUD_DrawGameClock(hud_t *hud)
{
	int width, height;
	int x, y;
	int timetype;
	const char *t;

	static cvar_t
		*hud_gameclock_big = NULL,
		*hud_gameclock_style,
		*hud_gameclock_blink,
		*hud_gameclock_countdown,
		*hud_gameclock_scale,
		*hud_gameclock_offset,
		*hud_gameclock_proportional;

	if (hud_gameclock_big == NULL)    // first time
	{
		hud_gameclock_big = HUD_FindVar(hud, "big");
		hud_gameclock_style = HUD_FindVar(hud, "style");
		hud_gameclock_blink = HUD_FindVar(hud, "blink");
		hud_gameclock_countdown = HUD_FindVar(hud, "countdown");
		hud_gameclock_scale = HUD_FindVar(hud, "scale");
		hud_gameclock_offset = HUD_FindVar(hud, "offset");
		hud_gameclock_proportional = HUD_FindVar(hud, "proportional");
		gameclockoffset = &hud_gameclock_offset->integer;
	}

	timetype = (hud_gameclock_countdown->value) ? TIMETYPE_GAMECLOCKINV : TIMETYPE_GAMECLOCK;
	t = SCR_GetTimeString(timetype, NULL);
	width = SCR_GetClockStringWidth(t, hud_gameclock_big->integer, hud_gameclock_scale->value, hud_gameclock_proportional->integer);
	height = SCR_GetClockStringHeight(hud_gameclock_big->integer, hud_gameclock_scale->value);

	if (HUD_PrepareDraw(hud, width, height, &x, &y)) {
		if (hud_gameclock_big->value) {
			SCR_DrawBigClock(x, y, hud_gameclock_style->value, hud_gameclock_blink->value, hud_gameclock_scale->value, t);
		}
		else {
			SCR_DrawSmallClock(x, y, hud_gameclock_style->value, hud_gameclock_blink->value, hud_gameclock_scale->value, t, hud_gameclock_proportional->integer);
		}
	}
}

//---------------------
//
// draw HUD democlock
//
static void SCR_HUD_DrawDemoClock(hud_t *hud)
{
	int width = 0;
	int height = 0;
	int x = 0;
	int y = 0;
	const char *t;
	static cvar_t
		*hud_democlock_big = NULL,
		*hud_democlock_style,
		*hud_democlock_blink,
		*hud_democlock_scale,
		*hud_democlock_proportional;

	if (!cls.demoplayback || cls.mvdplayback == QTV_PLAYBACK) {
		HUD_PrepareDraw(hud, width, height, &x, &y);
		return;
	}

	if (hud_democlock_big == NULL) {
		// first time
		hud_democlock_big = HUD_FindVar(hud, "big");
		hud_democlock_style = HUD_FindVar(hud, "style");
		hud_democlock_blink = HUD_FindVar(hud, "blink");
		hud_democlock_scale = HUD_FindVar(hud, "scale");
		hud_democlock_proportional = HUD_FindVar(hud, "proportional");
	}

	t = SCR_GetTimeString(TIMETYPE_DEMOCLOCK, NULL);
	width = SCR_GetClockStringWidth(t, hud_democlock_big->integer, hud_democlock_scale->value, hud_democlock_proportional->integer);
	height = SCR_GetClockStringHeight(hud_democlock_big->integer, hud_democlock_scale->value);

	if (HUD_PrepareDraw(hud, width, height, &x, &y)) {
		if (hud_democlock_big->value) {
			SCR_DrawBigClock(x, y, hud_democlock_style->value, hud_democlock_blink->value, hud_democlock_scale->value, t);
		}
		else {
			SCR_DrawSmallClock(x, y, hud_democlock_style->value, hud_democlock_blink->value, hud_democlock_scale->value, t, hud_democlock_proportional->integer);
		}
	}
}

static void SCR_HUD_DrawScoreClock(hud_t *hud) {
	int width, height;
	int x, y;
	static cvar_t
			*hud_scoreclock_format = NULL,
			*hud_scoreclock_scale = NULL,
			*hud_scoreclock_proportional = NULL;

	if (hud_scoreclock_format == NULL) {
		hud_scoreclock_format = HUD_FindVar(hud, "format");
		hud_scoreclock_scale = HUD_FindVar(hud, "scale");
		hud_scoreclock_proportional = HUD_FindVar(hud, "proportional");
	}

	if(!hud_scoreclock_format->string)
		return;

	time_t t;
	struct tm *ptm;
	char datestr[256] = "bad date";
	time(&t);
	if ((ptm = localtime(&t)))
		strftime(datestr, sizeof(datestr) - 1, hud_scoreclock_format->string, ptm);

	width = Draw_StringLengthColors(datestr, -1, hud_scoreclock_scale->value, hud_scoreclock_proportional->integer);
	height = SCR_GetClockStringHeight(false, hud_scoreclock_scale->value);

	if(!width)
		return;

	if (!HUD_PrepareDraw(hud, width, height, &x, &y))
		return;

	Draw_SString(x, y, datestr, hud_scoreclock_scale->value, hud_scoreclock_proportional->integer);
}

///
/// cl_screen.c clock module: to be merged
///
// non-static for menu
cvar_t scr_clock = { "cl_clock", "0" };
static cvar_t scr_clock_format = { "cl_clock_format", "0" };
static cvar_t scr_clock_x = { "cl_clock_x", "0" };
static cvar_t scr_clock_y = { "cl_clock_y", "-1" };

// non-static for menu
cvar_t scr_gameclock = { "cl_gameclock", "0" };
static cvar_t scr_gameclock_x = { "cl_gameclock_x", "0" };
static cvar_t scr_gameclock_y = { "cl_gameclock_y", "-3" };
static cvar_t scr_gameclock_offset = { "cl_gameclock_offset", "0" };

static cvar_t scr_democlock = { "cl_democlock", "0" };
static cvar_t scr_democlock_x = { "cl_democlock_x", "0" };
static cvar_t scr_democlock_y = { "cl_democlock_y", "-2" };

static void SCR_DrawClock(void)
{
	int x, y;
	char str[64];

	if (!scr_clock.integer || scr_newHud.integer == 1) {
		// newHud has its own clock
		return;
	}

	if (scr_clock.integer == 2) {
		strlcpy(str, SCR_GetTimeString(TIMETYPE_CLOCK, SCR_HUD_ClockFormat(scr_clock_format.integer)), sizeof(str));
	}
	else if (scr_clock.integer == 3) {
		strlcpy(str, SCR_GetTimeString(TIMETYPE_HOSTCLOCK, SCR_HUD_ClockFormat(scr_clock_format.integer)), sizeof(str));
	}
	else {
		strlcpy(str, SCR_GetTimeString(TIMETYPE_CONNECTEDCLOCK, SCR_HUD_ClockFormat(scr_clock_format.integer)), sizeof(str));
	}

	x = ELEMENT_X_COORD(scr_clock);
	y = ELEMENT_Y_COORD(scr_clock);
	Draw_String(x, y, str);
}

static void SCR_DrawGameClock(void)
{
	int x, y;
	char str[80], *s;
	float timelimit;

	if (!scr_gameclock.value || scr_newHud.value == 1) {
		// newHud has its own gameclock
		return;
	}

	if (scr_gameclock.value == 2 || scr_gameclock.value == 4) {
		timelimit = 60 * cl.timelimit + 1;
	}
	else {
		timelimit = 0;
	}

	if (cl.countdown || cl.standby) {
		strlcpy(str, SecondsToHourString(timelimit), sizeof(str));
	}
	else {
		strlcpy(str, SecondsToHourString((int)fabs(timelimit - cl.gametime + scr_gameclock_offset.value)), sizeof(str));
	}

	if ((scr_gameclock.value == 3 || scr_gameclock.value == 4) && (s = strchr(str, ':'))) {
		// or just use SecondsToMinutesString() ...
		s++;
	}
	else {
		s = str;
	}

	x = ELEMENT_X_COORD(scr_gameclock);
	y = ELEMENT_Y_COORD(scr_gameclock);
	Draw_String(x, y, s);
}

static void SCR_DrawDemoClock(void)
{
	int x, y;
	char str[80];

	if (!cls.demoplayback || cls.mvdplayback == QTV_PLAYBACK || !scr_democlock.value || scr_newHud.value == 1) {
		// newHud has its own democlock
		return;
	}

	if (scr_democlock.value == 2) {
		strlcpy(str, SecondsToHourString((int)(cls.demotime)), sizeof(str));
	}
	else {
		strlcpy(str, SecondsToHourString((int)(cls.demotime - demostarttime)), sizeof(str));
	}

	x = ELEMENT_X_COORD(scr_democlock);
	y = ELEMENT_Y_COORD(scr_democlock);
	Draw_String(x, y, str);
}

void SCR_DrawClocks(void)
{
	SCR_DrawClock();
	SCR_DrawGameClock();
	SCR_DrawDemoClock();
}

/// Initialisation
void Clock_HudInit(void)
{
	Cvar_Register(&scr_clock_x);
	Cvar_Register(&scr_clock_y);
	Cvar_Register(&scr_clock_format);
	Cvar_Register(&scr_clock);

	Cvar_Register(&scr_gameclock_offset);
	Cvar_Register(&scr_gameclock_x);
	Cvar_Register(&scr_gameclock_y);
	Cvar_Register(&scr_gameclock);

	Cvar_Register(&scr_democlock_x);
	Cvar_Register(&scr_democlock_y);
	Cvar_Register(&scr_democlock);

	// init clock
	HUD_Register(
		"clock", NULL, "Shows current local time (hh:mm:ss).",
		HUD_PLUSMINUS, ca_disconnected, 8, SCR_HUD_DrawClock,
		"0", "top", "right", "console", "0", "0", "0", "0 0 0", NULL,
		"big", "1",
		"style", "0",
		"scale", "1",
		"blink", "1",
		"format", "0",
		"proportional", "0",
		"content", "0",
		NULL
	);

	// init democlock
	HUD_Register(
		"democlock", NULL, "Shows current demo time (hh:mm:ss).",
		HUD_PLUSMINUS, ca_disconnected, 7, SCR_HUD_DrawDemoClock,
		"1", "top", "right", "console", "0", "8", "0", "0 0 0", NULL,
		"big", "0",
		"style", "0",
		"scale", "1",
		"blink", "0",
		"proportional", "0",
		NULL
	);

	// init gameclock
	HUD_Register(
		"gameclock", NULL, "Shows current game time (hh:mm:ss).",
		HUD_PLUSMINUS, ca_disconnected, 8, SCR_HUD_DrawGameClock,
		"1", "top", "right", "console", "0", "0", "0", "0 0 0", NULL,
		"big", "1",
		"style", "0",
		"scale", "1",
		"blink", "1",
		"countdown", "0",
		"offset", "0",
		"proportional", "0",
		NULL
	);

	// init scoreclock
	HUD_Register(
			"scoreclock", NULL, "Shows current date and time on the scoreboard",
			HUD_NO_DRAW | HUD_ON_INTERMISSION | HUD_ON_SCORES | HUD_ON_FINALE, ca_disconnected, 8, SCR_HUD_DrawScoreClock,
			"1", "screen", "center", "bottom", "0", "-10", "0", "0 0 0", NULL,
			"scale", "1",
			"format", "%d-%m-%Y %H:%M:%S",
			"proportional", "0",
			NULL
	);
}

//
// ------------------
// draw BIG clock
// style:
//  0 - normal
//  1 - red
void SCR_DrawBigClock(int x, int y, int style, int blink, float scale, const char *t)
{
	extern  mpic_t  *sb_nums[2][11];
	extern  mpic_t  *sb_colon/*, *sb_slash*/;
	qbool lblink = blink && ((int)(curtime * 10)) % 10 < 5;

	style = bound(0, style, 1);

	while (*t) {
		if (*t >= '0'  &&  *t <= '9') {
			Draw_STransPic(x, y, sb_nums[style][*t - '0'], scale);
			x += 24 * scale;
		}
		else if (*t == ':') {
			if (lblink || !blink) {
				Draw_STransPic(x, y, sb_colon, scale);
			}

			x += 16 * scale;
		}
		else {
			Draw_SCharacter(x, y, *t + (style ? 128 : 0), 3 * scale);
			x += 24 * scale;
		}
		t++;
	}
}

// ------------------
// draw SMALL clock
// style:
//  0 - small white
//  1 - small red
//  2 - small yellow/white
//  3 - small yellow/red
void SCR_DrawSmallClock(int x, int y, int style, int blink, float scale, const char *t, qbool proportional)
{
	qbool lblink = blink && ((int)(curtime * 10)) % 10 < 5;
	int c;

	style = bound(0, style, 3);

	while (*t) {
		c = (int)*t;
		if (c >= '0'  &&  c <= '9') {
			if (style == 1) {
				c += 128;
			}
			else if (style == 2 || style == 3) {
				c -= 30;
			}
		}
		else if (c == ':') {
			if (style == 1 || style == 3) {
				c += 128;
			}
			if (lblink || !blink) {
				;
			}
			else {
				c = ' ';
			}
		}
		x += Draw_SCharacterP(x, y, c, scale, proportional);
		t++;
	}
}
