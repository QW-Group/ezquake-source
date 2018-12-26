/*
Copyright (C) 1996-2003 Id Software, Inc., A Nourai

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

$Id: cl_screen.c,v 1.156 2007-10-29 00:56:47 qqshka Exp $
*/

#include "quakedef.h"
#include "keys.h"
#include "menu.h"

static cvar_t scr_centertime  = { "scr_centertime",  "2" };
static cvar_t scr_centershift = { "scr_centershift", "0" };
static cvar_t scr_printspeed  = { "scr_printspeed",  "8" };

/**************************** CENTER PRINTING ********************************/

static char	 scr_centerstring[1024];
static float scr_centertime_start;   // for slow victory printing
static float scr_centertime_off;
static int   scr_center_lines;
static int   scr_erase_lines;
static int   scr_erase_center;

void SCR_CenterPrint_Clear(void)
{
	// Make sure no centerprint messages are left from previous level.
	scr_centertime_off = 0;
	memset(scr_centerstring, 0, sizeof(scr_centerstring));
}

void SCR_CenterPrint_Init(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register(&scr_centertime);
	Cvar_Register(&scr_centershift);
	Cvar_Register(&scr_printspeed);
	Cvar_ResetCurrentGroup();
}

// Called for important messages that should stay in the center of the screen for a few moments
void SCR_CenterPrint(const char *str)
{
	strlcpy(scr_centerstring, str, sizeof(scr_centerstring));
	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = cl.time;

	// count the number of lines for centering
	scr_center_lines = 1;
	while (*str) {
		if (*str == '\n') {
			scr_center_lines++;
		}
		str++;
	}
}

static void SCR_DrawCenterString(void)
{
	char *start;
	int l, j, x, y, remaining;

	// the finale prints the characters one at a time
	remaining = cl.intermission ? scr_printspeed.value * (cl.time - scr_centertime_start) : 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	y = ((scr_center_lines <= 4) ? vid.height * 0.35 : 48);

	// shift all centerprint but not proxy menu - more user-friendly way
	if (m_state != m_proxy) {
		y += scr_centershift.value * 8;
	}

	while (1) {
		// scan the width of the line
		for (l = 0; l < 40; l++) {
			if (start[l] == '\n' || !start[l]) {
				break;
			}
		}
		x = (vid.width - l * 8) / 2;
		for (j = 0; j < l; j++, x += 8) {
			Draw_Character(x, y, start[j]);
			if (!remaining--) {
				return;
			}
		}

		y += 8;

		while (*start && *start != '\n') {
			start++;
		}

		if (!*start) {
			break;
		}
		start++;                // skip the \n
	}

}

void SCR_CheckDrawCenterString(void)
{
	scr_copytop = 1;
	if (scr_center_lines > scr_erase_lines) {
		scr_erase_lines = scr_center_lines;
	}

	scr_centertime_off -= cls.frametime;
	if (scr_centertime_off <= 0 && !cl.intermission) {
		return;
	}

	// condition says: "Draw center string only when in game or in proxy menu, otherwise leave."
	if (key_dest != key_game && ((key_dest != key_menu) || (m_state != m_proxy))) {
		return;
	}

	SCR_DrawCenterString();
}

void SCR_EraseCenterString(void)
{
	int y;

	if (scr_erase_center++ > vid.numpages) {
		scr_erase_lines = 0;
		return;
	}

	y = (scr_center_lines <= 4) ? vid.height * 0.35 : 48;

	scr_copytop = 1;
	Draw_TileClear(0, y, vid.width, min(8 * scr_erase_lines, vid.height - y - 1));
}
