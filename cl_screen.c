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

/// declarations may be found in screen.h

#include "quakedef.h"
#include <time.h>
#include "vx_tracker.h"
#include "gl_model.h"
#include "mvd_utils.h"
#include "keys.h"
#include "hud.h"
#include "hud_common.h"
#include "hud_editor.h"
#include "utils.h"
#include "vx_stuff.h"
#include "gl_model.h"
#include "teamplay.h"
#include "input.h"
#include "utils.h"
#include "sbar.h"
#include "menu.h"
#include "Ctrl.h"
#include "qtv.h"
#include "demo_controls.h"
#include "r_trace.h"
#include "r_lightmaps.h"
#include "r_local.h"
#include "r_chaticons.h"
#include "r_renderer.h"

#ifndef CLIENTONLY
#include "server.h"
#endif

void WeaponStats_CommandInit(void);
void SCR_DrawHud(void);
void SCR_DrawClocks(void);
void R_SetupFrame(void);
void SCR_Draw_TeamInfo(void);
void SCR_Draw_ShowNick(void);
void SCR_DrawQTVBuffer(void);
void SCR_DrawFPS(void);
void SCR_DrawSpeed(void);
qbool V_PreRenderView(void);

int	glx, gly, glwidth, glheight;

#define ALPHA_COLOR(x, y) RGBA_TO_COLOR((x)[0],(x)[1],(x)[2],(y))

extern byte	current_pal[768];	// Tonik

int	host_screenupdatecount; // kazik - HUD -> hexum

// only the refresh window will be updated unless these variables are flagged
int		scr_copytop;
int		scr_copyeverything;

float	scr_con_current;
float	scr_conlines;           // lines of console to display

qbool	scr_skipupdate;
qbool	block_drawing;

float	oldscreensize, oldfov, oldsbar;

#define ZOOMEDFOV 35
qbool	zoomedin;
float	unzoomedfov;
float	unzoomedsensitivity;

void OnFovChange (cvar_t *var, char *value, qbool *cancel);
void OnDefaultFovChange (cvar_t *var, char *value, qbool *cancel);
cvar_t	scr_fov					= {"fov", "90", CVAR_NONE, OnFovChange};	// 10 - 140
cvar_t	default_fov				= {"default_fov", "90", CVAR_NONE, OnDefaultFovChange};
cvar_t	scr_viewsize			= {"viewsize", "100", CVAR_NONE};
cvar_t	scr_consize				= {"scr_consize", "0.5"};
cvar_t	scr_conspeed			= {"scr_conspeed", "9999"};
cvar_t	scr_showturtle			= {"showturtle", "0"};
cvar_t	scr_showpause			= {"showpause", "1"};

cvar_t	scr_newHud              = {"scr_newhud", "0"};

cvar_t	show_velocity_3d		= {"show_velocity_3d", "0"};
cvar_t	show_velocity_3d_offset_forward	= {"show_velocity_3d_offset_forward", "2.5"};
cvar_t	show_velocity_3d_offset_down	= {"show_velocity_3d_offset_down", "5"};

cvar_t	cl_hud					= {"cl_hud", "1"};	// QW262 HUD.

cvar_t	gl_triplebuffer			= {"gl_triplebuffer", "1"};
cvar_t  r_chaticons_alpha		= {"r_chaticons_alpha", "0.8"};
cvar_t	scr_coloredfrags		= {"scr_coloredfrags", "0"};

cvar_t	scr_cursor_scale		= {"scr_cursor_scale", "0.2"};			// The mouse cursor scale.
cvar_t	scr_cursor_iconoffset_x	= {"scr_cursor_iconoffset_x", "10"};	// How much the cursor icon should be offseted from the cursor.
cvar_t	scr_cursor_iconoffset_y	= {"scr_cursor_iconoffset_y", "0"};
cvar_t	scr_cursor_alpha		= {"scr_cursor_alpha", "1"};

cvar_t  scr_showcrosshair       = {"scr_showcrosshair", "1"}; // so crosshair does't affected by +showscores, or vice versa
cvar_t  scr_notifyalways        = {"scr_notifyalways", "0"}; // don't hide notification messages in intermission

cvar_t  scr_fovmode             = {"scr_fovmode", "0"}; // When using reduced viewsize, reduce vertical fov or letterbox the screen

qbool	scr_initialized;	// Ready to draw.

mpic_t	*scr_ram;
mpic_t	*scr_net;
mpic_t	*scr_turtle;
mpic_t  *scr_cursor;		// Cursor image.
mpic_t	*scr_cursor_icon;	// Cursor icon image (load icon or similar).

int		scr_fullupdate;

int		clearconsole;
int		clearnotify;

viddef_t vid; // global video state

vrect_t	scr_vrect;

qbool	scr_skipupdate;

qbool	scr_drawloading;
qbool	scr_disabled_for_loading;
float	scr_disabled_time;

qbool	block_drawing;

double cursor_x = 0, cursor_y = 0;

void SCR_DrawMultiviewBorders(void);
void SCR_DrawMVStatus(void);
void SCR_DrawMVStatusStrings(void);

/************************************ FOV ************************************/

extern	cvar_t		v_idlescale;
qbool	concussioned = false;

void OnFovChange (cvar_t *var, char *value, qbool *cancel)
{
	float newfov = Q_atof(value);

	if (newfov > 140)
		newfov = 140;
	else if (newfov < 10)
		newfov = 10;

	if (newfov == scr_fov.value && newfov == default_fov.value) {
		*cancel = true;
		return;
	}

	if (cbuf_current != &cbuf_svc) {
		Cvar_SetValue (&default_fov, newfov);
		if (concussioned && !cls.demoplayback) {
			*cancel = true;
			return;
		}
	} else {
		if (newfov != 90 && cl.teamfortress && v_idlescale.value >= 20) {
			concussioned = true;
			if (v_idlescale.value == 100)
				TP_ExecTrigger ("f_conc");
		} else if (newfov == 90 && cl.teamfortress) {
			concussioned = false;
		}
		if (cls.demoplayback) { // && !cl_fovfromdemo.value)
			*cancel = true;
			return;
		}
	}

	vid.recalc_refdef = true;
	if (newfov == 90) {
		Cvar_Set (&scr_fov,default_fov.string);
		*cancel = true;
		return;
	}

	Cvar_SetValue (&scr_fov, newfov);
	*cancel = true;
}

void OnDefaultFovChange (cvar_t *var, char *value, qbool *cancel)
{
	float newfov = Q_atof(value);

	if (newfov < 10.0 || newfov > 140.0){
		Com_Printf("Invalid default_fov\n");
		*cancel = true;
	}
}

static void CalcFov(float fov, float *fov_x, float *fov_y, float width, float height, float view_width, float view_height, qbool reduce_vertfov)
{
	float t;
	float fovx;
	float fovy;

	if (fov < 10)
		fov = 10;
	else if (fov > 140)
		fov = 140;

	if (width / 4 < height /3)
	{
		fovx = fov;
		t = width / tan(fovx / 360 * M_PI);
		fovy = atan (height / t) * 360 / M_PI;
	}
	else
	{
		fovx = fov;
		t = 4.0 / tan(fovx / 360 * M_PI);
		fovy = atan (3.0 / t) * 360 / M_PI;
		t = height / tan(fovy / 360 * M_PI);
		fovx = atan (width / t) * 360 / M_PI;

		if (reduce_vertfov) {
			// Crop vertically when viewsize decreased
			t = view_width / tan (fovx / 360 * M_PI);
			fovy = atan (view_height / t) * 360 / M_PI;
		}
	}

	if (fovx < 10 || fovx > 140)
	{
		if (fovx < 10)
			fovx = 10;
		else if (fovx > 140)
			fovx = 140;

		t = width / tan(fovx / 360 * M_PI);
		fovy = atan (height / t) * 360 / M_PI;
	}

	if (fovy < 10 || fovy > 140)
	{
		if (fovy < 10)
			fovy = 10;
		else if (fovy > 140)
			fovy = 140;

		t = height / tan(fovy / 360 * M_PI);
		fovx = atan (width / t) * 360 / M_PI;
	}

	if (fovx < 1 || fovx > 179 || fovy < 1 || fovy > 179)
		Sys_Error ("CalcFov: Bad fov (%f, %f)", fovx, fovy);

	*fov_x = fovx;
	*fov_y = fovy;
}

//Must be called whenever vid changes
void SCR_CalcRefdef(void)
{
	float size = 0;
	qbool full = false;
	int h;
	float aspectratio = vid.height ? (float)vid.width / vid.height : 1;
	qbool letterbox = (scr_fovmode.integer == 1);
	qbool height_reduced = false;

	scr_fullupdate = 0;             // force a background redraw
	vid.recalc_refdef = 0;

	// force the status bar to redraw
	Sbar_Changed ();

	// bound viewsize
	if (scr_viewsize.value < 30) {
		Cvar_Set(&scr_viewsize, "30");
	}
	if (scr_viewsize.value > 120) {
		Cvar_Set(&scr_viewsize, "120");
	}

	// intermission is always full screen
	size = cl.intermission ? 120 : scr_viewsize.value;

	if (size >= 120) {
		sb_lines = 0;           // no status bar at all
	}
	else if (size >= 110) {
		sb_lines = 24;          // no inventory
	}
	else {
		sb_lines = 24 + 16 + 8;
	}

	if (scr_viewsize.value >= 100.0) {
		full = true;
		size = 100.0;
	}
	else {
		size = scr_viewsize.value;
	}
	if (cl.intermission) {
		full = true;
		size = 100.0;
		sb_lines = 0;
	}
	size /= 100.0;

	if (!cl_sbar.value && full)
		h = vid.height;
	else
		h = vid.height - sb_lines;

	r_refdef.vrect.width = vid.width * size;
	if (r_refdef.vrect.width < 96) {
		size = 96.0 / r_refdef.vrect.width;
		r_refdef.vrect.width = 96;      // min for icons
	}

	r_refdef.vrect.height = vid.height * size;
	if (cl_sbar.value || !full) {
		if (r_refdef.vrect.height > vid.height - sb_lines) {
			r_refdef.vrect.height = vid.height - sb_lines;
			height_reduced = true;
		}
	} else if (r_refdef.vrect.height > vid.height) {
		r_refdef.vrect.height = vid.height;
	}

	// Reduce width to keep aspect ratio constant with monitor
	if (letterbox) {
		r_refdef.vrect.width = min (r_refdef.vrect.width, r_refdef.vrect.height * aspectratio);
		height_reduced = false;
	}

	r_refdef.vrect.x = (vid.width - r_refdef.vrect.width) / 2;
	if (full)
		r_refdef.vrect.y = 0;
	else
		r_refdef.vrect.y = (h - r_refdef.vrect.height) / 2;

	CalcFov (scr_fov.value, &r_refdef.fov_x, &r_refdef.fov_y, vid.width, vid.height, r_refdef.vrect.width, r_refdef.vrect.height, height_reduced);

	scr_vrect = r_refdef.vrect;
}

//Keybinding command
void SCR_SizeUp_f(void)
{
	Cvar_SetValue(&scr_viewsize, scr_viewsize.value + 10);
	vid.recalc_refdef = 1;
}

//Keybinding command
void SCR_SizeDown_f(void)
{
	Cvar_SetValue(&scr_viewsize, scr_viewsize.value - 10);
	vid.recalc_refdef = 1;
}

void SCR_ZoomIn_f(void)
{
	if (zoomedin) {
		return;
	}
	zoomedin = true;
	unzoomedfov = scr_fov.value;
	unzoomedsensitivity = sensitivity.value;
	Cvar_SetValue(&scr_fov, ZOOMEDFOV);
	Cvar_SetValue(&sensitivity, unzoomedsensitivity * ((double)ZOOMEDFOV / unzoomedfov));
}

void SCR_ZoomOut_f(void)
{
	if (!zoomedin) {
		return;
	}
	zoomedin = false;
	Cvar_SetValue(&scr_fov, unzoomedfov);
	Cvar_SetValue(&sensitivity, unzoomedsensitivity);
}

/********************************** ELEMENTS **********************************/

#ifdef EXPERIMENTAL_SHOW_ACCELERATION
void SCR_DrawAccel (void) {
	extern qbool player_in_air;
	extern float cosinus_val;

	int x, y, length, charsize, pos;
	const float scale_factor = 10.f;
	char cosinus_str[10];
	if(!player_in_air) return;

	charsize = (int) (8.f * vid.height / vid.conheight);
	length = vid.width / 3;
	x = (vid.width - length) / 2;
	y = vid.height - sb_lines - charsize - 1;

	pos = (int) ((cosinus_val + 1) * length / 2);

	hud.draw_accel_bar(x, y, length, charsize, pos);

	//scale: show most interesting
	pos = (int) ((cosinus_val * scale_factor + 1) * length / 2);

	hud.draw_accel_bar(x, y - 2 * charsize, length, charsize, pos);

	cosinus_str[0] = '\0';
	sprintf(cosinus_str,"%.3f", cosinus_val);
	Draw_String(x, y - charsize, cosinus_str);
}
#endif

void SCR_DrawTurtle(void)
{
	static int  count;

	if (!scr_showturtle.value) {
		return;
	}

	if (cls.frametime < 0.1) {
		count = 0;
		return;
	}

	count++;
	if (count < 3) {
		return;
	}

	Draw_Pic(scr_vrect.x, scr_vrect.y, scr_turtle);
}

static void SCR_DrawNet(void)
{
	if (cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged < UPDATE_BACKUP - 1) {
		return;
	}
	if (cls.demoplayback || scr_newHud.value == 1) {
		return;
	}

	Draw_Pic(scr_vrect.x + 64, scr_vrect.y, scr_net);
}

static void SCR_DrawPause (void) {
	mpic_t *pic;

	if (!scr_showpause.value)               // turn off for screenshots
		return;

	if (!cl.paused)
		return;

#ifndef CLIENTONLY
	if (sv.paused == 2)
		return; // auto-paused in single player
#endif

	pic = Draw_CachePic (CACHEPIC_PAUSE);
	Draw_Pic ((vid.width - pic->width) / 2, (vid.height - 48 - pic->height) / 2, pic);
}

void SCR_DrawLoading (void) {
	mpic_t *pic;

	if (!scr_drawloading)
		return;

	pic = Draw_CachePic (CACHEPIC_LOADING);
	Draw_Pic ( (vid.width - pic->width )/ 2, (vid.height - 48 - pic->height) / 2, pic);
}



void SCR_BeginLoadingPlaque (void) {
	if (cls.state != ca_active)
		return;

	if (key_dest == key_console)
		return;

	// redraw with no console and the loading plaque
	scr_fullupdate = 0;
	Sbar_Changed ();
	scr_drawloading = true;
	SCR_UpdateScreen ();
	scr_drawloading = false;

	scr_disabled_for_loading = true;
	scr_disabled_time = cls.realtime;
	scr_fullupdate = 0;
}

void SCR_EndLoadingPlaque (void) {
	if (!scr_disabled_for_loading)
		return;
	scr_disabled_for_loading = false;
	scr_fullupdate = 0;
}

/********************************** CONSOLE **********************************/

void SCR_SetUpToDrawConsole (void) {
	Con_CheckResize ();
	
	// decide on the height of the console
	if (SCR_NEED_CONSOLE_BACKGROUND) {
		scr_conlines = vid.height;		// full screen
		scr_con_current = scr_conlines;
	} else if (key_dest == key_console) {
		scr_conlines = vid.height * scr_consize.value;
		scr_conlines = bound(30, scr_conlines, vid.height);
	} else {
		scr_conlines = 0;				// none visible
	}

	if (scr_conlines < scr_con_current)	{
		scr_con_current -= scr_conspeed.value * cls.trueframetime * vid.height / 320;
		scr_con_current = max(scr_con_current, scr_conlines);
	} else if (scr_conlines > scr_con_current) {
		scr_con_current += scr_conspeed.value * cls.trueframetime * vid.height / 320;
		scr_con_current = min(scr_con_current, scr_conlines);
	}

	if (clearconsole++ < vid.numpages)
	{
		Sbar_Changed ();
	}
	else
	{
		con_notifylines = 0;
	}

	clearnotify++;
}

void SCR_DrawConsole (void) {
	if (scr_con_current) {
		scr_copyeverything = 1;
		Con_DrawConsole (scr_con_current);
		clearconsole = 0;
	} else {
		if (key_dest == key_game || key_dest == key_message || (key_dest == key_menu && m_state == m_proxy))
			Con_DrawNotify ();      // only draw notify in game
	}
}

/********************************* TILE CLEAR *********************************/

void SCR_TileClear(void)
{
	int sb_lines_cleared = scr_newHud.integer ? 0 : sb_lines; // newhud does not (typically) have solid status bar, so clear the bottom of the screen

	if (cls.state != ca_active && cl.intermission) {
		Draw_TileClear(0, 0, vid.width, vid.height);
		return;
	}

	if (r_refdef.vrect.x > 0) {
		// left
		Draw_TileClear(0, 0, r_refdef.vrect.x, vid.height - sb_lines_cleared);
		// right
		Draw_TileClear(r_refdef.vrect.x + r_refdef.vrect.width, 0,
			vid.width - (r_refdef.vrect.x + r_refdef.vrect.width), vid.height - sb_lines_cleared);
	}
	if (r_refdef.vrect.y > 0) {
		// top
		Draw_TileClear(r_refdef.vrect.x, 0, r_refdef.vrect.width, r_refdef.vrect.y);
	}
	if (r_refdef.vrect.y + r_refdef.vrect.height < vid.height - sb_lines_cleared) {
		// bottom
		Draw_TileClear(r_refdef.vrect.x,
			r_refdef.vrect.y + r_refdef.vrect.height,
			r_refdef.vrect.width,
			vid.height - sb_lines_cleared - (r_refdef.vrect.height + r_refdef.vrect.y));
	}
}

//
// Calculates the cursor scale based on the current screen/text size
//
static double SCR_GetCursorScale(void) 
{
	return (double) scr_cursor_scale.value * ((double) vid.width / (double)vid.conwidth);
}

static void SCR_DrawCursor(void) 
{
	double scale = SCR_GetCursorScale();

	// Always draw the cursor if fullscreen
	if (IN_QuakeMouseCursorRequired()) {
		float x_coord = scr_pointer_state.x;
		float y_coord = scr_pointer_state.y;

		if (scr_cursor && R_TextureReferenceIsValid(scr_cursor->texnum)) {
			Draw_SAlphaPic(x_coord, y_coord, scr_cursor, scr_cursor_alpha.value, scale);

			if (scr_cursor_icon && R_TextureReferenceIsValid(scr_cursor_icon->texnum)) {
				Draw_SAlphaPic(x_coord + scr_cursor_iconoffset_x.value, y_coord + scr_cursor_iconoffset_y.value, scr_cursor_icon, scr_cursor_alpha.value, scale);
			}
		}
		else {
			color_t c = RGBA_TO_COLOR(0, 255, 0, 255);

			Draw_AlphaLineRGB(x_coord + (10 * scale), y_coord + (10 * scale), x_coord + (40 * scale), y_coord + (40 * scale), 10 * scale, c);
			Draw_AlphaLineRGB(x_coord, y_coord, x_coord + (20 * scale), y_coord, 10 * scale, c);
			Draw_AlphaLineRGB(x_coord, y_coord, x_coord, y_coord + 20 * scale, 10 * scale, c);
			Draw_AlphaLineRGB(x_coord + (20 * scale), y_coord, x_coord, y_coord + (20 * scale), 10 * scale, c);
		}
	}

	// remember the position for future
	scr_pointer_state.x_old = scr_pointer_state.x;
	scr_pointer_state.y_old = scr_pointer_state.y;
}

static void SCR_UpdateCursor(void)
{
	// vid_sdl2 updates absolute cursor position when not locked
	scr_pointer_state.x = max(0, min(scr_pointer_state.x_old + (VID_ConsoleX(cursor_x) - scr_pointer_state.x_old), glwidth));
	scr_pointer_state.y = max(0, min(scr_pointer_state.y_old + (VID_ConsoleY(cursor_y) - scr_pointer_state.y_old), glheight));

	if (IN_MouseTrackingRequired() && (scr_pointer_state.x != scr_pointer_state.x_old || scr_pointer_state.y != scr_pointer_state.y_old)) {
		Mouse_MoveEvent();
	}
}

static void SCR_VoiceMeter(void)
{
#ifdef FTE_PEXT2_VOICECHAT

	float	range;
	int		loudness;
	int		w1, w2;
	int		w = 100;												// random.
	int     h = 8;
	int     x, y;

	if (!S_Voip_ShowMeter (&x, &y)) {
		return;
	}

	loudness = S_Voip_Loudness();

	if (loudness < 0)
		return;

	range = loudness / 100.0f;
	range = bound(0.0f, range, 1.0f);
	w1 = range * w;
	w2 = w - w1;
	Draw_String (x, y, "mic ");
	x += 8 * (sizeof("mic ")-1);
	Draw_AlphaRectangleRGB(x, y, w1, h, 1, true, RGBA_TO_COLOR(255, 0, 0, 255));
	x += w1;
	Draw_AlphaRectangleRGB(x, y, w2, h, 1, true, RGBA_TO_COLOR(0, 255, 0, 255));

#endif // FTE_PEXT2_VOICECHAT
}

void SCR_DrawMultiviewOverviewElements (void)
{
	SCR_DrawMultiviewBorders ();
}

// Elements to be drawn on every view during multiview
void SCR_DrawMultiviewIndividualElements(void)
{
	extern qbool  sb_showscores,  sb_showteamscores;

	// Show autoid in all views
	if (!sb_showscores && !sb_showteamscores) {
		SCR_DrawAutoID();
	}

	// Crosshair
	if ((key_dest != key_menu) && (scr_showcrosshair.integer || (!sb_showscores && !sb_showteamscores))) {
		if (!CL_MultiviewInsetView() || cl_mvinsetcrosshair.integer) {
			Draw_Crosshair();
		}
	}

	// Draw multiview mini-HUD's.
	if (cl_mvdisplayhud.integer) {
		if (cl_mvdisplayhud.integer >= 2) {
			// Graphical with icons.
			SCR_DrawMVStatus ();
		}
		else {
			// Only strings.
			SCR_DrawMVStatusStrings ();
		}
	}
}

static void SCR_DrawElements(void) 
{
	extern qbool  sb_showscores,  sb_showteamscores;
	extern cvar_t	scr_menudrawhud;

	if (scr_drawloading) 
	{
		SCR_DrawLoading ();
		Sbar_Draw ();
		HUD_Draw ();		// HUD -> hexum
	}
	else 
	{
		SCR_UpdateCursor();

		if( !(!scr_menudrawhud.integer && (m_state != m_none)) || (!scr_menudrawhud.integer && (m_state == m_proxy)) )
		{
			if (cl.intermission == 1) {
				Sbar_IntermissionOverlay();
				if (!scr_notifyalways.integer) {
					Con_ClearNotify();
				}
			}
			else if (cl.intermission == 2) {
				Sbar_FinaleOverlay();
				SCR_CenterString_Draw();
				if (!scr_notifyalways.integer) {
					Con_ClearNotify();
				}
			}

			if (cls.state == ca_active)
			{
				SCR_DrawNet ();
				SCR_DrawTurtle ();
#ifdef EXPERIMENTAL_SHOW_ACCELERATION
				SCR_DrawAccel();
#endif

				if (!sb_showscores && !sb_showteamscores) 
				{ 
					// Do not show if +showscores
					SCR_DrawPause ();

					SCR_DrawAutoID ();

					SCR_VoiceMeter ();
				}

				if (!cl.intermission) 
				{
					if ((key_dest != key_menu) && (scr_showcrosshair.integer || (!sb_showscores && !sb_showteamscores)))
					{
						Draw_Crosshair ();
					}

					// Do not show if +showscores
					if (!sb_showscores && !sb_showteamscores)
					{ 
						SCR_Draw_TeamInfo();

						SCR_Draw_ShowNick();

						SCR_CenterString_Draw();
						SCR_DrawSpeed();
						SCR_DrawClocks();
						SCR_DrawQTVBuffer();
						SCR_DrawFPS();
					}

					// QW262
					SCR_DrawHud ();

					MVD_Screen ();

					// VULT STATS
					SCR_DrawAMFstats();

					// VULT DISPLAY KILLS
					VX_TrackerThink();

					if (CL_MultiviewEnabled())
						SCR_DrawMultiviewOverviewElements ();

					Sbar_Draw();
					HUD_Draw();
					HUD_Editor_Draw();

					DemoControls_Draw();
				}
			}
		}

		if (!SCR_TakingAutoScreenshot()) {
			SCR_DrawConsole();

			M_Draw();
		}

		SCR_DrawCursor();
	}
}

/******************************* UPDATE SCREEN *******************************/

qbool SCR_UpdateScreenPrePlayerView (void)
{
	extern cvar_t gl_clear;
	extern qbool Minimized;
	static int oldfovmode = 0;

	if (!scr_initialized) {
		return false;
	}

	if (scr_skipupdate || block_drawing) {
		return false;
	}

	if (scr_disabled_for_loading) {
		if (cls.realtime - scr_disabled_time > 20) {
			scr_disabled_for_loading = false;
		}
		else {
			return false;
		}
	}
	// Don't suck up any cpu if minimized.

	if (Minimized) {
		return false;
	}

	vid.numpages = 2 + gl_triplebuffer.value;

	scr_copytop = 0;
	scr_copyeverything = 0;

	host_screenupdatecount++;  // For HUD.

	// Check for vid changes.
	if (oldfov != scr_fov.value) 
	{
		oldfov = scr_fov.value;
		vid.recalc_refdef = true;
	}

	if (oldscreensize != scr_viewsize.value) 
	{
		oldscreensize = scr_viewsize.value;
		vid.recalc_refdef = true;
	}

	if (oldsbar != cl_sbar.value) 
	{
		oldsbar = cl_sbar.value;
		vid.recalc_refdef = true;
	}

	if (oldfovmode != scr_fovmode.integer) {
		oldfovmode = scr_fovmode.integer;
		vid.recalc_refdef = true;
	}

	if (vid.recalc_refdef) {
		SCR_CalcRefdef();
	}

	if ((v_contrast.value > 1 && !vid_hwgamma_enabled) || gl_clear.value) {
		Sbar_Changed();
	}
	else if (scr_newHud.integer == 2 && scr_viewsize.value < 120) {
		Sbar_Changed();
	}

	return true;
}

void SCR_UpdateScreenPlayerView(int flags)
{
	R_TraceResetRegion(true);

	R_CheckReloadLightmaps();

	// preache skins if needed
	Skins_PreCache();

	SCR_SetUpToDrawConsole();

	R_SetupChatIcons();

	R_BeginRendering(&glx, &gly, &glwidth, &glheight);

	if (V_PreRenderView()) {
		R_SetupFrame();

		R_RenderView();

		if (flags & UPDATESCREEN_POSTPROCESS) {
			R_PostProcessScene();
		}
	}

	R_Set2D();

	R_PolyBlend();

	// draw any areas not covered by the refresh
	SCR_TileClear();
}

void SCR_HUD_WeaponStats(hud_t* hud);

// Drawing new HUD items in old HUD mode - eventually move everything across
static void SCR_DrawNewHudElements(void)
{
	extern cvar_t r_netgraph, r_netstats;
	static hud_t *hud_netstats = NULL;
	static hud_t *hud_weaponstats = NULL;
	if (hud_netstats == NULL) // first time
		hud_netstats = HUD_Find("net");
	if (hud_weaponstats == NULL)
		hud_weaponstats = HUD_Find("weaponstats");

	if (r_netgraph.value)
	{
		float temp = hud_netgraph->show->value;

		Cvar_SetValue(hud_netgraph->show, 1);
		SCR_HUD_Netgraph(hud_netgraph);
		Cvar_SetValue(hud_netgraph->show, temp);
	}

	if (r_netstats.value)
	{
		float temp = hud_netstats->show->value;

		Cvar_SetValue(hud_netstats->show, 1);
		SCR_HUD_DrawNetStats(hud_netstats);
		Cvar_SetValue(hud_netstats->show, temp);
	}

	if (hud_weaponstats && hud_weaponstats->show && hud_weaponstats->show->value) {
		SCR_HUD_WeaponStats(hud_weaponstats);
	}
}

void SCR_UpdateScreenPostPlayerView(void)
{
	extern qbool  sb_showscores, sb_showteamscores;
	extern cvar_t scr_menudrawhud;

	R_TraceEnterNamedRegion("HUD");
	if (scr_newHud.value != 1) {
		SCR_DrawNewHudElements();
	}

	SCR_DrawElements();

	// Actual rendering...
	R_FlushImageDraw();
	R_TraceLeaveNamedRegion();

	renderer.PostProcessScreen();

	SCR_CheckAutoScreenshot();

	VID_RenderFrameEnd();

	R_EndRendering();
}

// This is called every frame, and can also be called explicitly to flush text to the screen.
// WARNING: be very careful calling this from elsewhere, because the refresh needs almost the entire 256k of stack space!
qbool SCR_UpdateScreen(void)
{
	if (!SCR_UpdateScreenPrePlayerView()) {
		VID_RenderFrameEnd();
		return false;
	}

	renderer.ScreenDrawStart();

	SCR_UpdateScreenPlayerView(UPDATESCREEN_POSTPROCESS);

	SCR_UpdateScreenPostPlayerView();

	return true;
}

void SCR_UpdateWholeScreen(void)
{
	scr_fullupdate = 0;
	SCR_UpdateScreen();
}

mpic_t *SCR_LoadCursorImage(char *cursorimage)
{
	mpic_t *image = NULL;

	image = Draw_CachePicSafe(va("%s.lmp", cursorimage), false, false);

	// Failed to load anything, maybe missing .lmp-file, so just try
	// loading any 24-bit version that's available instead.
	if(!image)
	{
		image = Draw_CachePicSafe(cursorimage, false, true);
	}

	return image;
}

/************************************ TEMPORARY *******************************/

/* FIXME: Remove this when most people have upgraded to 3.0 */
static void tmp_calc_fov(void)
{
	float fov;
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: calc_fov <old_fov>\n");
		return;
	}

	fov = Q_atof(Cmd_Argv(1));
	fov = atan(((tan((fov/2)*M_PI/180))/1.2))*360/M_PI;

	Com_Printf("Use fov %d (%f)\n", (int)(fov+0.5), fov);
}


/************************************ INIT ************************************/

void SCR_Init (void)
{
	scr_ram = Draw_CacheWadPic ("ram", WADPIC_RAM);
	scr_net = Draw_CacheWadPic ("net", WADPIC_NET);
	scr_turtle = Draw_CacheWadPic ("turtle", WADPIC_TURTLE);
	scr_cursor = SCR_LoadCursorImage("gfx/cursor");

	Cvar_SetCurrentGroup(CVAR_GROUP_VIEW);
	Cvar_Register (&scr_fov);
	Cvar_Register (&default_fov);
	Cvar_Register (&scr_viewsize);
	Cvar_Register (&scr_fovmode);

	Cvar_SetCurrentGroup(CVAR_GROUP_SBAR);
	Cvar_Register(&scr_newHud);
	Cvar_ResetCurrentGroup();

	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register (&scr_consize);
	Cvar_Register (&scr_conspeed);

	Cvar_SetCurrentGroup(CVAR_GROUP_OPENGL);
	Cvar_Register (&gl_triplebuffer);

	Cvar_SetCurrentGroup(CVAR_GROUP_EYECANDY);
	Cvar_Register (&r_chaticons_alpha);

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register (&scr_showturtle);
	Cvar_Register (&scr_showpause);

	Cvar_Register (&show_velocity_3d);
	Cvar_Register (&show_velocity_3d_offset_forward);
	Cvar_Register (&show_velocity_3d_offset_down);

	SCR_RegisterAutoIDCvars();

	Cvar_Register (&scr_coloredfrags);

	Cmd_AddCommand("calc_fov", tmp_calc_fov);

	// QW 262 HUD
	Cvar_Register (&cl_hud);

	Cvar_Register (&scr_showcrosshair);
	Cvar_Register (&scr_notifyalways);
	Cvar_ResetCurrentGroup();

	Cvar_SetCurrentGroup(CVAR_GROUP_MENU);
	Cvar_Register (&scr_cursor_scale);
	Cvar_Register (&scr_cursor_iconoffset_x);
	Cvar_Register (&scr_cursor_iconoffset_y);
	Cvar_Register (&scr_cursor_alpha);
	Cvar_ResetCurrentGroup();

	SShot_RegisterCvars();

	Cmd_AddCommand ("sizeup", SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown", SCR_SizeDown_f);
	Cmd_AddCommand ("+zoom", SCR_ZoomIn_f);
	Cmd_AddCommand ("-zoom", SCR_ZoomOut_f);

	WeaponStats_CommandInit();
	SCR_CenterPrint_Init();

	scr_initialized = true;

	ScrollBars_Init();
}

void SCR_DrawMultiviewBorders(void)
{
	//
	// Draw black borders around the views.
	//
	if (cl_multiview.value == 2 && !cl_mvinset.value)
	{
		Draw_Fill(0, vid.height / 2, vid.width - 1, 1, 0);
	}
	else if (cl_multiview.value == 2 && cl_mvinset.value)
	{
		if (vid.width <= 512 && cl_sbar.value)
		{
			Draw_Fill(vid.width / 3 * 2 + 1, vid.height / 3 - sb_lines / 3, vid.width / 3 + 2, 1, 0);
			Draw_Fill(vid.width / 3 * 2 + 1, 0, 1, vid.height / 3 - sb_lines / 3, 0);
		}
		else if ((vid.width > 512 && cl_sbar.value && !cl_mvinsethud.value) || (vid.width > 512 && cl_sbar.value && !cl_mvdisplayhud.value))
		{
			Draw_Fill(vid.width / 3 * 2, vid.height / 3 - sb_lines / 3, vid.width / 3, 1, 0);
			Draw_Fill(vid.width / 3 * 2, 0, 1, vid.height / 3 - sb_lines/3, 0);
		}
		else
		{
			// sbar 0 and <= 512 conwidth
			Draw_Fill(vid.width / 3 * 2 + 1, vid.height / 3, vid.width / 3 + 2, 1, 0);
			Draw_Fill(vid.width / 3 * 2 + 1, 0, 1, vid.height / 3, 0);
		}
	}
	else if (cl_multiview.value == 3)
	{
		Draw_Fill(vid.width / 2, vid.height / 2, 1, vid.height / 2, 0);
		Draw_Fill(0, vid.height / 2, vid.width, 1, 0);
	}
	else if (cl_multiview.value == 4)
	{
		Draw_Fill(vid.width / 2, 0, 1, vid.height, 0);
		Draw_Fill(0, vid.height / 2, vid.width, 1, 0);
	}
}

mpic_t * SCR_GetWeaponIconByFlag (int flag)
{
	extern mpic_t *sb_weapons[7][8];  // sbar.c Weapon pictures.

	if (flag == IT_LIGHTNING || flag == IT_SUPER_LIGHTNING)
	{
		return sb_weapons[0][6];
	}
	else if (flag == IT_ROCKET_LAUNCHER)
	{
		return sb_weapons[0][5];
	}
	else if (flag == IT_GRENADE_LAUNCHER)
	{
		return sb_weapons[0][4];
	}
	else if (flag == IT_SUPER_NAILGUN)
	{
		return sb_weapons[0][3];
	}
	else if (flag == IT_NAILGUN)
	{
		return sb_weapons[0][2];
	}
	else if (flag == IT_SUPER_SHOTGUN)
	{
		return sb_weapons[0][1];
	}
	else if (flag == IT_SHOTGUN)
	{
		return sb_weapons[0][0];
	}

	return NULL;
}

