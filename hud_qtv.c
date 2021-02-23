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
#include "hud.h"
#include "qtv.h"

static cvar_t scr_qtvbuffer   = { "scr_qtvbuffer",        "0" };
static cvar_t scr_qtvbuffer_x = { "scr_qtvbuffer_x",      "0" };
static cvar_t scr_qtvbuffer_y = { "scr_qtvbuffer_y",    "-10" };

void SCR_DrawQTVBuffer(void)
{
	extern double Demo_GetSpeed(void);

	int x, y;
	int ms, len;
	char str[64];

	if (!SCR_QTVBufferToBeDrawn(scr_qtvbuffer.integer)) {
		return;
	}

	len = Demo_BufferSize(&ms);
	snprintf(str, sizeof(str), "%6dms %5db %2.3f", ms, len, Demo_GetSpeed());

	x = ELEMENT_X_COORD(scr_qtvbuffer);
	y = ELEMENT_Y_COORD(scr_qtvbuffer);
	Draw_String(x, y, str);
}

static void SCR_HUD_DrawQTVBuffer(hud_t* hud)
{
	extern double Demo_GetSpeed(void);

	int x, y;
	int ms, len;
	char str[64];
	float draw_len;

	static cvar_t *hud_scale, *hud_proportional;

	if (hud_scale == NULL) {
		hud_scale = HUD_FindVar(hud, "scale");
		hud_proportional = HUD_FindVar(hud, "proportional");
	}

	if (!SCR_QTVBufferToBeDrawn(hud->show->integer)) {
		return;
	}

	len = Demo_BufferSize(&ms);
	snprintf(str, sizeof(str), "%6dms %5db %2.3f", ms, len, Demo_GetSpeed());
	
	draw_len = Draw_StringLength(str, -1, hud_scale->value, hud_proportional->integer);
	if (HUD_PrepareDraw(hud, draw_len, 8 * hud_scale->value, &x, &y)) {
		Draw_SString(x, y, str, hud_scale->value, hud_proportional->integer);
	}
}

void Qtv_HudInit(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register(&scr_qtvbuffer_x);
	Cvar_Register(&scr_qtvbuffer_y);
	Cvar_Register(&scr_qtvbuffer);
	Cvar_ResetCurrentGroup();

	HUD_Register(
		"qtv_buffer", NULL, "QTV buffering status.",
		0, ca_active, 0, SCR_HUD_DrawQTVBuffer,
		"0", "screen", "left", "top", "0", "0", "0", "0 0 0", NULL,
		"scale", "1",
		"proportional", "0",
		NULL
	);
}
