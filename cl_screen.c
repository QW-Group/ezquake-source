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
#ifdef GLQUAKE
#include "gl_model.h"
#include "gl_local.h"
#else
#include "r_model.h"
#include "r_local.h"
#endif
#include "mvd_utils.h"
#include "keys.h"
#include "hud.h"
#include "hud_common.h"
#include "hud_editor.h"
#include "utils.h"
#include "vx_stuff.h"
#ifdef GLQUAKE
#include "gl_model.h"
#include "gl_local.h"
#else
#include "r_model.h"
#include "r_local.h"
#endif
#include "teamplay.h"
#include "input.h"
#include "utils.h"
#include "sbar.h"
#include "menu.h"
#include "image.h"
#ifdef _WIN32
#include "movie.h"	//joe: capturing to avi
#include "movie_avi.h"	//
#endif
#include "Ctrl.h"
#include "qtv.h"
#include "demo_controls.h"

#ifndef CLIENTONLY
#include "server.h"
#endif

#ifdef GLQUAKE
int				glx, gly, glwidth, glheight;
#endif

/*
#ifdef GLQUAKE
#define			DEFAULT_SSHOT_FORMAT		"tga"
#else
#define			DEFAULT_SSHOT_FORMAT		"pcx"
#endif
*/
#define			DEFAULT_SSHOT_FORMAT		"png"

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
void OnChange_scr_clock_format (cvar_t *var, char *value, qbool *cancel);
cvar_t	scr_fov					= {"fov", "90", CVAR_ARCHIVE, OnFovChange};	// 10 - 140
cvar_t	default_fov				= {"default_fov", "90", CVAR_ARCHIVE, OnDefaultFovChange};
cvar_t	scr_viewsize			= {"viewsize", "100", CVAR_ARCHIVE};
cvar_t	scr_consize				= {"scr_consize", "0.5"};
cvar_t	scr_conspeed			= {"scr_conspeed", "1000"};
cvar_t	scr_centertime			= {"scr_centertime", "2"};
cvar_t	scr_centershift			= {"scr_centershift", "0"};
cvar_t	scr_showram				= {"showram", "1"};
cvar_t	scr_showturtle			= {"showturtle", "0"};
cvar_t	scr_showpause			= {"showpause", "1"};
cvar_t	scr_printspeed			= {"scr_printspeed", "8"};
void	OnChange_scr_allowsnap(cvar_t *, char *, qbool *);
cvar_t	scr_allowsnap			= {"scr_allowsnap", "1", 0, OnChange_scr_allowsnap};

cvar_t	scr_newHud = {"scr_newhud", "0"};
cvar_t	scr_newHudClear = {"scr_newhud_clear", "0"};	// force clearing screen on every frame (necessary with viewsize)

cvar_t	scr_clock				= {"cl_clock", "0"};
cvar_t	scr_clock_format		= {"cl_clock_format", "%H:%M:%S", 0, OnChange_scr_clock_format};
cvar_t	scr_clock_x				= {"cl_clock_x", "0"};
cvar_t	scr_clock_y				= {"cl_clock_y", "-1"};

cvar_t	scr_gameclock			= {"cl_gameclock", "0"};
cvar_t	scr_gameclock_x			= {"cl_gameclock_x", "0"};
cvar_t	scr_gameclock_y			= {"cl_gameclock_y", "-3"};
cvar_t	scr_gameclock_offset	= {"cl_gameclock_offset", "0"};

cvar_t	scr_democlock			= {"cl_democlock", "0"};
cvar_t	scr_democlock_x			= {"cl_democlock_x", "0"};
cvar_t	scr_democlock_y			= {"cl_democlock_y", "-2"};

cvar_t	scr_qtvbuffer			= {"scr_qtvbuffer", "0"};
cvar_t	scr_qtvbuffer_x			= {"scr_qtvbuffer_x", "0"};
cvar_t	scr_qtvbuffer_y			= {"scr_qtvbuffer_y", "-10"};

cvar_t	show_speed				= {"show_speed", "0"};
cvar_t	show_speed_x			= {"show_speed_x", "-1"};
cvar_t	show_speed_y			= {"show_speed_y", "1"};

cvar_t	show_fps				= {"show_fps", "0"};
cvar_t	show_fps_x				= {"show_fps_x", "-5"};
cvar_t	show_fps_y				= {"show_fps_y", "-1"};

cvar_t	scr_sshot_autoname		= {"sshot_autoname", "0"};
cvar_t	scr_sshot_format		= {"sshot_format", DEFAULT_SSHOT_FORMAT};
cvar_t	scr_sshot_dir			= {"sshot_dir", ""};

cvar_t	cl_hud					= {"cl_hud", "1"};	// QW262 HUD.

#ifdef GLQUAKE
cvar_t	gl_triplebuffer			= {"gl_triplebuffer", "1", CVAR_ARCHIVE};
cvar_t  r_chaticons_alpha		= {"r_chaticons_alpha", "0.8"};
cvar_t	scr_autoid				= {"scr_autoid", "5"};
cvar_t	scr_autoid_drawname		= {"scr_autoid_drawname", "1"};
cvar_t	scr_coloredfrags		= {"scr_coloredfrags", "0"};
#endif


cvar_t  scr_teaminfo_order       = {"scr_teaminfo_order", "%p%n $x10%l$x11 %a/%H %w", CVAR_ARCHIVE, OnChange_scr_clock_format};
cvar_t	scr_teaminfo_align_right = {"scr_teaminfo_align_right", "1", CVAR_ARCHIVE};
cvar_t	scr_teaminfo_frame_color = {"scr_teaminfo_frame_color", "10 0 0 120", CVAR_COLOR};
cvar_t	scr_teaminfo_scale		 = {"scr_teaminfo_scale",       "1",  CVAR_ARCHIVE};
cvar_t	scr_teaminfo_y			 = {"scr_teaminfo_y",           "0",  CVAR_ARCHIVE};
cvar_t  scr_teaminfo_x			 = {"scr_teaminfo_x",           "0",  CVAR_ARCHIVE};
cvar_t  scr_teaminfo_loc_width	 = {"scr_teaminfo_loc_width",   "5",  CVAR_ARCHIVE};
cvar_t  scr_teaminfo_name_width	 = {"scr_teaminfo_name_width",  "6",  CVAR_ARCHIVE};
cvar_t	scr_teaminfo_low_health	 = {"scr_teaminfo_low_health",  "25", CVAR_ARCHIVE};
cvar_t	scr_teaminfo_armor_style = {"scr_teaminfo_armor_style", "3",  CVAR_ARCHIVE};
cvar_t	scr_teaminfo_weapon_style= {"scr_teaminfo_weapon_style","0",  CVAR_ARCHIVE};
cvar_t  scr_teaminfo_show_enemies= {"scr_teaminfo_show_enemies","0",  CVAR_ARCHIVE};
cvar_t  scr_teaminfo_show_self   = {"scr_teaminfo_show_self",   "2",  CVAR_ARCHIVE};
cvar_t  scr_teaminfo			 = {"scr_teaminfo",             "1",  CVAR_ARCHIVE};

cvar_t  scr_shownick_order		 = {"scr_shownick_order", "%p%n %a/%H %w", CVAR_ARCHIVE, OnChange_scr_clock_format};
#ifdef GLQUAKE
cvar_t	scr_shownick_frame_color = {"scr_shownick_frame_color", "10 0 0 120", CVAR_ARCHIVE | CVAR_COLOR};
cvar_t	scr_shownick_scale		 = {"scr_shownick_scale",		"1",   CVAR_ARCHIVE};
cvar_t	scr_shownick_y			 = {"scr_shownick_y",			"0",   CVAR_ARCHIVE};
cvar_t	scr_shownick_x			 = {"scr_shownick_x",			"0",   CVAR_ARCHIVE};
cvar_t  scr_shownick_name_width	 = {"scr_shownick_name_width",	"6",   CVAR_ARCHIVE};
cvar_t  scr_shownick_time		 = {"scr_shownick_time",		"0.8", CVAR_ARCHIVE};
#endif

void OnChange_scr_weaponstats (cvar_t *var, char *value, qbool *cancel);
cvar_t  scr_weaponstats_order        = {"scr_weaponstats_order",       "&c990sg&r:%2 &c099ssg&r:%3 &c900rl&r:#7 &c009lg&r:%8", CVAR_ARCHIVE, OnChange_scr_clock_format};
cvar_t	scr_weaponstats_align_right  = {"scr_weaponstats_align_right", "1", CVAR_ARCHIVE};
cvar_t	scr_weaponstats_frame_color  = {"scr_weaponstats_frame_color", "10 0 0 120", CVAR_COLOR};
cvar_t	scr_weaponstats_scale		 = {"scr_weaponstats_scale",       "1",  CVAR_ARCHIVE};
cvar_t	scr_weaponstats_y			 = {"scr_weaponstats_y",           "0",  CVAR_ARCHIVE};
cvar_t  scr_weaponstats_x			 = {"scr_weaponstats_x",           "0",  CVAR_ARCHIVE};
cvar_t  scr_weaponstats				 = {"scr_weaponstats",             "",   CVAR_ARCHIVE, OnChange_scr_weaponstats};

cvar_t	scr_coloredText			= {"scr_coloredText", "1"};

// Tracking text.
cvar_t	scr_tracking			= {"scr_tracking", "Tracking %t %n, [JUMP] for next"};
cvar_t	scr_spectatorMessage	= {"scr_spectatorMessage", "1"};

cvar_t	scr_cursor_scale		= {"scr_cursor_scale", "0.2"};			// The mouse cursor scale.
cvar_t	scr_cursor_iconoffset_x	= {"scr_cursor_iconoffset_x", "10"};	// How much the cursor icon should be offseted from the cursor.
cvar_t	scr_cursor_iconoffset_y	= {"scr_cursor_iconoffset_y", "0"};
#ifdef GLQUAKE
cvar_t	scr_cursor_alpha		= {"scr_cursor_alpha", "1"};
#endif

cvar_t  scr_showcrosshair       = {"scr_showcrosshair", "1"}; // so crosshair does't affected by +showscores, or vice versa

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

static int scr_autosshot_countdown = 0;
static qbool scr_mvsshot_in_progress = false;
char auto_matchname[2 * MAX_OSPATH];

static void SCR_CheckAutoScreenshot(void);
static void SCR_CheckMVScreenshot(void);
void SCR_DrawMultiviewBorders(void);
void SCR_DrawMVStatus(void);
void SCR_DrawMVStatusStrings(void);
void Draw_AlphaFill (int x, int y, int w, int h, byte c, float alpha);
void Draw_AlphaString (int x, int y, char *str, float alpha);
void Draw_AlphaPic (int x, int y, mpic_t *pic, float alpha);

void OnChange_scr_allowsnap(cvar_t *var, char *s, qbool *cancel) {
	*cancel = (cls.state >= ca_connected && cbuf_current == &cbuf_svc);
}


/**************************** CENTER PRINTING ********************************/

char		scr_centerstring[1024];
float		scr_centertime_start;   // for slow victory printing
float		scr_centertime_off;
int			scr_center_lines;
int			scr_erase_lines;
int			scr_erase_center;

// Called for important messages that should stay in the center of the screen for a few moments
void SCR_CenterPrint (char *str)
{
	strlcpy (scr_centerstring, str, sizeof(scr_centerstring));
	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = cl.time;

	// count the number of lines for centering
	scr_center_lines = 1;
	while (*str) 
	{
		if (*str == '\n')
			scr_center_lines++;
		str++;
	}
}

void SCR_DrawCenterString (void)
{
	char *start;
	int l, j, x, y, remaining;

	// the finale prints the characters one at a time
	remaining = cl.intermission ? scr_printspeed.value * (cl.time - scr_centertime_start) : 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	y = ((scr_center_lines <= 4) ? vid.height * 0.35 : 48);

	// shift all centerprint but not proxy menu - more user-friendly way
	if (m_state != m_proxy)
		y += scr_centershift.value*8;

	while (1) {
		// scan the width of the line
		for (l = 0; l < 40; l++) {
			if (start[l] == '\n' || !start[l])
				break;
		}
		x = (vid.width - l * 8) / 2;
		for (j = 0; j < l; j++, x += 8) {
			Draw_Character (x, y, start[j]);
			if (!remaining--)
				return;
		}

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;                // skip the \n
	}

}

void SCR_CheckDrawCenterString (void) {
	scr_copytop = 1;
	if (scr_center_lines > scr_erase_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= cls.frametime;

	if (scr_centertime_off <= 0 && !cl.intermission)
		return;

	// condition says: "Draw center string only when in game or in proxy menu, otherwise leave."
	if (key_dest != key_game && ((key_dest != key_menu) || (m_state != m_proxy)))
		return;

	SCR_DrawCenterString ();
}

void SCR_EraseCenterString (void) {
	int y;

	if (scr_erase_center++ > vid.numpages) {
		scr_erase_lines = 0;
		return;
	}

	y = (scr_center_lines <= 4) ? vid.height * 0.35 : 48;

	scr_copytop = 1;
	Draw_TileClear (0, y, vid.width, min(8 * scr_erase_lines, vid.height - y - 1));
}

/************************************ FOV ************************************/

extern	cvar_t		v_idlescale;
qbool	concussioned = false;
float	nonwidefov = 0; // Store original fov if vid_wideaspect is used

void OnFovChange (cvar_t *var, char *value, qbool *cancel)
{
	float newfov = Q_atof(value);

	if (nonwidefov == 0)
		nonwidefov = newfov;		// save first fov value to initialize nonwidefov

#ifndef __APPLE__
#ifdef GLQUAKE
	if (host_everything_loaded && vid_wideaspect.integer)
	{
		newfov = tan((newfov/2)*M_PI/180);
		newfov = newfov*48/40;				// 3/4 * 16/10
		newfov = 2 * atan(newfov)*180/M_PI;
		Com_Printf("vid_wideaspect enabled - fov recalculated to %f\n", newfov);
	}
	else if (host_initialized && vid_wideaspect.integer)
	{
			nonwidefov = tan((newfov/2)*M_PI/180);
			nonwidefov = nonwidefov * 40 / 48;
			nonwidefov = 2 * atan(nonwidefov) * 180 / M_PI;
	}
#endif // GLQUAKE
#endif // __APPLE__

	if (newfov > 140)
		nonwidefov = newfov = 140;
	else if (newfov < 10)
		nonwidefov = newfov = 10;

	if (newfov == scr_fov.value) {
		*cancel = true;
		return;
	}

	if ( cbuf_current != &cbuf_svc) {
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

static float CalcFov (float fov_x, float width, float height) {
	float x;

	if (fov_x < 1 || fov_x > 179) {
		Com_Printf ("CalcFov: Bad fov (%f)", fov_x);
		Cvar_Set (&scr_fov, "90");
	}

	x = width / tan(fov_x / 360 * M_PI);
	return atan (height / x) * 360 / M_PI;
}

//Must be called whenever vid changes
static void SCR_CalcRefdef (void) {
	float  size;
#ifdef GLQUAKE
	int h;
	qbool full = false;
#else
	vrect_t vrect;
#endif

	scr_fullupdate = 0;             // force a background redraw
	vid.recalc_refdef = 0;

	// force the status bar to redraw
	Sbar_Changed ();

	// bound viewsize
	if (scr_viewsize.value < 30)
		Cvar_Set (&scr_viewsize, "30");
	if (scr_viewsize.value > 120)
		Cvar_Set (&scr_viewsize, "120");

	// intermission is always full screen
	size = cl.intermission ? 120 : scr_viewsize.value;

	if (size >= 120)
		sb_lines = 0;           // no status bar at all
	else if (size >= 110)
		sb_lines = 24;          // no inventory
	else
		sb_lines = 24 + 16 + 8;

#ifdef GLQUAKE

	if (scr_viewsize.value >= 100.0) {
		full = true;
		size = 100.0;
	} else {
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
  		if (r_refdef.vrect.height > vid.height - sb_lines)
  			r_refdef.vrect.height = vid.height - sb_lines;
	} else if (r_refdef.vrect.height > vid.height) {
			r_refdef.vrect.height = vid.height;
	}
	r_refdef.vrect.x = (vid.width - r_refdef.vrect.width) / 2;
	if (full)
		r_refdef.vrect.y = 0;
	else
		r_refdef.vrect.y = (h - r_refdef.vrect.height) / 2;

	r_refdef.fov_x = scr_fov.value;
	r_refdef.fov_y = CalcFov (r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);

	scr_vrect = r_refdef.vrect;

#else

	r_refdef.fov_x = scr_fov.value;
	r_refdef.fov_y = CalcFov (r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);

	// these calculations mirror those in R_Init() for r_refdef, but take noaccount of water warping
	vrect.x = 0;
	vrect.y = 0;
	vrect.width = vid.width;
	vrect.height = vid.height;

	R_SetVrect (&vrect, &scr_vrect, sb_lines);

	// guard against going from one mode to another that's less than half the vertical resolution
	scr_con_current = min(scr_con_current, vid.height);

	// notify the refresh of the change
	R_ViewChanged (&vrect, sb_lines, vid.aspect);

#endif
}

//Keybinding command
void SCR_SizeUp_f (void) {
	Cvar_SetValue (&scr_viewsize, scr_viewsize.value + 10);
	vid.recalc_refdef = 1;
}

//Keybinding command
void SCR_SizeDown_f (void) {
	Cvar_SetValue (&scr_viewsize,scr_viewsize.value - 10);
	vid.recalc_refdef = 1;
}

void SCR_ZoomIn_f (void) {
	if (zoomedin) return;
	zoomedin = true;
	unzoomedfov = scr_fov.value;
	unzoomedsensitivity = sensitivity.value;
	Cvar_SetValue(&scr_fov, ZOOMEDFOV);
	Cvar_SetValue(&sensitivity, unzoomedsensitivity * ((double) ZOOMEDFOV / unzoomedfov));
}

void SCR_ZoomOut_f (void) {
	if (!zoomedin) return;
	zoomedin = false;
	Cvar_SetValue(&scr_fov, unzoomedfov);
	Cvar_SetValue(&sensitivity, unzoomedsensitivity);
}

/********************************** ELEMENTS **********************************/

void SCR_DrawRam (void) {
	if (!scr_showram.value)
		return;

	if (!r_cache_thrash)
		return;

	Draw_Pic (scr_vrect.x + 32, scr_vrect.y, scr_ram);
}

void SCR_DrawTurtle (void) {
	static int  count;

	if (!scr_showturtle.value)
		return;

	if (cls.frametime < 0.1) {
		count = 0;
		return;
	}

	count++;
	if (count < 3)
		return;

	Draw_Pic (scr_vrect.x, scr_vrect.y, scr_turtle);
}

void SCR_DrawNet (void) {
	if (cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged < UPDATE_BACKUP-1)
		return;
	if (cls.demoplayback || scr_newHud.value == 1)
		return;

	Draw_Pic (scr_vrect.x + 64, scr_vrect.y, scr_net);
}



void SCR_DrawFPS (void) {
	int x, y;
	char str[80];
	extern double lastfps;

	if (!show_fps.value || scr_newHud.value == 1) // HUD -> hexum - newHud has its own fps
		return;

	// Multiview
	if (cl_multiview.value && cls.mvdplayback)
	{
		snprintf(str, sizeof(str), "%3.1f", (lastfps + 0.05)/nNumViews);
	}
	else
	{
		snprintf(str, sizeof(str), "%3.1f",  lastfps + 0.05);
	}

	x = ELEMENT_X_COORD(show_fps);
	y = ELEMENT_Y_COORD(show_fps);
	Draw_String (x, y, str);
}



void SCR_DrawSpeed (void) {
	double t;
	int x, y, mynum;
	char str[80];
	vec3_t vel;
	float speed;
	static float maxspeed = 0, display_speed = -1;
	static double lastframetime = 0;
	static int lastmynum = -1;

	if (!show_speed.value || scr_newHud.value == 1) // newHud has its own speed
		return;

	t = Sys_DoubleTime();

	if (!cl.spectator || (mynum = Cam_TrackNum()) == -1)
		mynum = cl.playernum;

	if (mynum != lastmynum) {
		lastmynum = mynum;
		lastframetime = t;
		display_speed = -1;
		maxspeed = 0;
	}


	if (!cl.spectator || cls.demoplayback || mynum == cl.playernum)
		VectorCopy (cl.simvel, vel);
	else
		VectorCopy (cl.frames[cl.validsequence & UPDATE_MASK].playerstate[mynum].velocity, vel);

	vel[2] = 0;
	speed = VectorLength(vel);

	maxspeed = max(maxspeed, speed);

	if (display_speed >= 0) {
		snprintf(str, sizeof(str), "%3d%s", (int) display_speed, show_speed.value == 2 ? " SPD" : "");
		x = ELEMENT_X_COORD(show_speed);
		y = ELEMENT_Y_COORD(show_speed);
		Draw_String (x, y, str);
	}

	if (t - lastframetime >= 0.1) {
		lastframetime = t;
		display_speed = maxspeed;
		maxspeed = 0;
	}
}

void OnChange_scr_clock_format (cvar_t *var, char *value, qbool *cancel) {
	if (!host_initialized)
		return; // we in progress of initialization, allow

	if (cls.state == ca_active) {
		Com_Printf("Can't change %s while connected\n", var->name);
		*cancel = true; // prevent stick notes
		return;
	}
}

void SCR_DrawClock (void) {
	int x, y;
	time_t t;
	struct tm *ptm;
	char str[64];

	if (!scr_clock.value || scr_newHud.value == 1) // newHud  has its own clock
		return;

	if (scr_clock.value == 2) {
		time (&t);
		if ((ptm = localtime (&t))) {
			strftime (str, sizeof (str) - 1, scr_clock_format.string[0] ? scr_clock_format.string : "%H:%M:%S", ptm);
		} else {
			strlcpy (str, "#bad date#", sizeof (str));
		}
	} else {
		float time = (cl.servertime_works) ? cl.servertime : cls.realtime;
		strlcpy (str, SecondsToHourString((int) time), sizeof (str));
	}

	x = ELEMENT_X_COORD(scr_clock);
	y = ELEMENT_Y_COORD(scr_clock);
	Draw_String (x, y, str);
}

void SCR_DrawGameClock (void) {
	int x, y;
	char str[80], *s;
	float timelimit;

	if (!scr_gameclock.value || scr_newHud.value == 1) // newHud has its own gameclock
		return;

	if (scr_gameclock.value == 2 || scr_gameclock.value == 4)
		timelimit = 60 * Q_atof(Info_ValueForKey(cl.serverinfo, "timelimit")) + 1;
	else
		timelimit = 0;

	if (cl.countdown || cl.standby)
		strlcpy (str, SecondsToHourString(timelimit), sizeof(str));
	else
		strlcpy (str, SecondsToHourString((int) abs(timelimit - cl.gametime + scr_gameclock_offset.value)), sizeof(str));

	if ((scr_gameclock.value == 3 || scr_gameclock.value == 4) && (s = strstr(str, ":")))
		s++;		// or just use SecondsToMinutesString() ...
	else
		s = str;

	x = ELEMENT_X_COORD(scr_gameclock);
	y = ELEMENT_Y_COORD(scr_gameclock);
	Draw_String (x, y, s);
}

void SCR_DrawDemoClock (void) {
	int x, y;
	char str[80];

	if (!cls.demoplayback || !scr_democlock.value || scr_newHud.value == 1) // newHud has its own democlock
		return;

	if (scr_democlock.value == 2)
		strlcpy (str, SecondsToHourString((int) (cls.demotime)), sizeof(str));
	else
		strlcpy (str, SecondsToHourString((int) (cls.demotime - demostarttime)), sizeof(str));

	x = ELEMENT_X_COORD(scr_democlock);
	y = ELEMENT_Y_COORD(scr_democlock);
	Draw_String (x, y, str);
}


void SCR_DrawQTVBuffer (void)
{
	extern double Demo_GetSpeed(void);
	extern unsigned char pb_buf[];
	extern int	pb_cnt;

	int x, y;
	int ms, len;
	char str[64];

	switch(scr_qtvbuffer.integer)
	{
		case 0:
			return;

		case 1:
			if (cls.mvdplayback != QTV_PLAYBACK)
				return; // not qtv, ignore

			break;

		default:
			if (!cls.mvdplayback)
				return; // not mvd(that include qtv), ignore

			break;
	}

	len = ConsistantMVDDataEx(pb_buf, pb_cnt, &ms);

	snprintf(str, sizeof(str), "%6dms %5db %2.3f", ms, len, Demo_GetSpeed());

	x = ELEMENT_X_COORD(scr_qtvbuffer);
	y = ELEMENT_Y_COORD(scr_qtvbuffer);
	Draw_String (x, y, str);
}

void SCR_DrawPause (void) {
	mpic_t *pic;

	if (!scr_showpause.value)               // turn off for screenshots
		return;

	if (!cl.paused)
		return;

#ifndef CLIENTONLY
	if (sv.paused == 2)
		return; // auto-paused in single player
#endif

	pic = Draw_CachePic ("gfx/pause.lmp");
	Draw_Pic ((vid.width - pic->width) / 2, (vid.height - 48 - pic->height) / 2, pic);
}

void SCR_DrawLoading (void) {
	mpic_t *pic;

	if (!scr_drawloading)
		return;

	pic = Draw_CachePic ("gfx/loading.lmp");
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
#ifndef GLQUAKE
		scr_copytop = 1;

		// Multiview (software)
		if (!(cl_multiview.value && cls.mvdplayback))
		{
			Draw_TileClear (0, (int) scr_con_current, vid.width, vid.height - (int) scr_con_current);
		}
#endif
		Sbar_Changed ();
	}
	else if (clearnotify++ < vid.numpages)
	{
#ifndef GLQUAKE
		scr_copytop = 1;

		// Multiview (software)
		if (!(cl_multiview.value && cls.mvdplayback))
		{
			Draw_TileClear (0, 0, vid.width, con_notifylines);
		}
#endif
	}
	else
	{
		con_notifylines = 0;
	}

#ifndef GLQUAKE
	{
		extern cvar_t scr_conalpha;
		if (!scr_conalpha.value && scr_con_current) {
			Draw_TileClear(0, 0, vid.width, scr_con_current);
		}
	}
#endif
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

/*********************************** AUTOID ***********************************/

#define AUTOID_HEALTHBAR_BG_COLOR			180, 115, 115
#define AUTOID_HEALTHBAR_NORMAL_COLOR		80, 0, 0
#define AUTOID_HEALTHBAR_MEGA_COLOR			255, 0, 0
#define AUTOID_HEALTHBAR_TWO_MEGA_COLOR		255, 100, 0
#define AUTOID_HEALTHBAR_UNNATURAL_COLOR	255, 255, 255

#define AUTOID_HEALTHBAR_OFFSET_Y			16

#define AUTOID_ARMORBAR_GREEN_ARMOR			25, 170, 0
#define AUTOID_ARMORBAR_YELLOW_ARMOR		255, 220, 0
#define AUTOID_ARMORBAR_RED_ARMOR			255, 0, 0

#define AUTOID_ARMORBAR_OFFSET_Y			AUTOID_HEALTHBAR_OFFSET_Y - 5
#define AUTOID_ARMORNAME_OFFSET_Y			AUTOID_ARMORBAR_OFFSET_Y - 8 - 2
#define AUTOID_ARMORNAME_OFFSET_X			8/2

#define AUTOID_WEAPON_OFFSET_Y				AUTOID_HEALTHBAR_OFFSET_Y
#define AUTOID_WEAPON_OFFSET_X				2

#ifdef GLQUAKE


int qglProject (float objx, float objy, float objz, float *model, float *proj, int *view, float* winx, float* winy, float* winz) {
	float in[4], out[4];
	int i;

	in[0] = objx; in[1] = objy; in[2] = objz; in[3] = 1.0;


	for (i = 0; i < 4; i++)
		out[i] = in[0] * model[0 * 4 + i] + in[1] * model[1 * 4 + i] + in[2] * model[2 * 4 + i] + in[3] * model[3 * 4 + i];


	for (i = 0; i < 4; i++)
		in[i] =	out[0] * proj[0 * 4 + i] + out[1] * proj[1 * 4 + i] + out[2] * proj[2 * 4 + i] + out[3] * proj[3 * 4 + i];

	if (!in[3])
		return 0;

	VectorScale(in, 1 / in[3], in);


	*winx = view[0] + (1 + in[0]) * view[2] / 2;
	*winy = view[1] + (1 + in[1]) * view[3] / 2;
	*winz = (1 + in[2]) / 2;

	return 1;
}

typedef struct player_autoid_s {
	float x, y;
	player_info_t *player;
} autoid_player_t;

static autoid_player_t autoids[MAX_CLIENTS];
static int autoid_count;

#define ISDEAD(i) ((i) >= 41 && (i) <= 102)

void SCR_SetupAutoID (void) {
	int j, view[4], tracknum = -1;
	float model[16], project[16], winz, *origin;
	player_state_t *state;
	player_info_t *info;
	centity_t *cent;
	item_vis_t visitem;
	autoid_player_t *id;

	autoid_count = 0;

	if (!scr_autoid.value)
		return;

	if (cls.state != ca_active || !cl.validsequence)
		return;

	if (!cls.demoplayback && !cl.spectator)
		return;

	glGetFloatv(GL_MODELVIEW_MATRIX, model);
	glGetFloatv(GL_PROJECTION_MATRIX, project);
	glGetIntegerv(GL_VIEWPORT, (GLint *)view);

	if (cl.spectator)
	{
		tracknum = Cam_TrackNum();
	}

	VectorCopy(vpn, visitem.forward);
	VectorCopy(vright, visitem.right);
	VectorCopy(vup, visitem.up);
	VectorCopy(r_origin, visitem.vieworg);

	state = cl.frames[cl.parsecount & UPDATE_MASK].playerstate;
	info = cl.players;
	cent = &cl_entities[1];

	for (j = 0; j < MAX_CLIENTS; j++, info++, state++, cent++)
	{
		if (state->messagenum != cl.parsecount || j == cl.playernum || j == tracknum || info->spectator)
			continue;

		if ((state->modelindex == cl_modelindices[mi_player] && ISDEAD(state->frame)) ||
			state->modelindex == cl_modelindices[mi_h_player])
			continue;

		origin = cent->lerp_origin;

		// FIXME: In multiview, this will detect some players being outside of the view even though
		// he's visible on screen, this only happens in some cases.
		if (R_CullSphere(origin, 0))
			continue;

		VectorCopy (origin, visitem.entorg);
		visitem.entorg[2] += 27;
		VectorSubtract (visitem.entorg, visitem.vieworg, visitem.dir);
		visitem.dist = DotProduct (visitem.dir, visitem.forward);
		visitem.radius = 25;

		if (!TP_IsItemVisible(&visitem))
			continue;

		id = &autoids[autoid_count];
		id->player = info;
		if (qglProject(origin[0], origin[1], origin[2] + 28, model, project, view, &id->x, &id->y, &winz))
			autoid_count++;
	}
}

void SCR_DrawAutoIDStatus (autoid_player_t *autoid_p, int x, int y)
{
	char armor_name[20];
	int health;
	int name_length;
	int health_length;
	int armor;
	int armor_length;

	// Draw health above the name.
	name_length = strlen(autoid_p->player->name) * 4;
	health = autoid_p->player->stats[STAT_HEALTH];
	health = min(100, health);
	health_length = Q_rint((name_length/100.0) * health);

	// Normal health.
	Draw_AlphaFillRGB(x - name_length, y - AUTOID_HEALTHBAR_OFFSET_Y, name_length * 2, 4, RGBA_TO_COLOR(AUTOID_HEALTHBAR_BG_COLOR, 100));
	Draw_AlphaFillRGB(x - name_length, y - AUTOID_HEALTHBAR_OFFSET_Y, health_length * 2, 4, RGBA_TO_COLOR(AUTOID_HEALTHBAR_NORMAL_COLOR, 255));

	health = autoid_p->player->stats[STAT_HEALTH];

	// Mega health
	if(health > 100 && health <= 200)
	{
		health_length = Q_rint((name_length/100.0) * (health - 100));
		Draw_AlphaFillRGB(x - name_length, y - AUTOID_HEALTHBAR_OFFSET_Y, health_length * 2, 4, RGBA_TO_COLOR(AUTOID_HEALTHBAR_MEGA_COLOR, 255));
	}
	else if(health > 200 && health <= 250)
	{
		// Super health.
		health_length = Q_rint((name_length/100.0) * (health - 200));
		Draw_AlphaFillRGB(x - name_length, y - AUTOID_HEALTHBAR_OFFSET_Y, name_length * 2, 4, RGBA_TO_COLOR(AUTOID_HEALTHBAR_MEGA_COLOR, 255));
		Draw_AlphaFillRGB(x - name_length, y - AUTOID_HEALTHBAR_OFFSET_Y, health_length * 2, 4, RGBA_TO_COLOR(AUTOID_HEALTHBAR_TWO_MEGA_COLOR, 255));
	}
	else if(health > 250)
	{
		// Crazy health.
		// This will never happen during a normal game.
		Draw_AlphaFillRGB(x - name_length, y - AUTOID_HEALTHBAR_OFFSET_Y, name_length * 2, 4, RGBA_TO_COLOR(AUTOID_HEALTHBAR_UNNATURAL_COLOR, 255));
	}

	// Draw armor.
	armor = autoid_p->player->stats[STAT_ARMOR];
	armor = min(100, armor);
	armor_length = Q_rint((name_length/100.0) * armor);

	if(autoid_p->player->stats[STAT_ITEMS] & IT_ARMOR1)
	{
		// Green armor.
		strlcpy(armor_name, "&c0f0GA", sizeof(armor_name));
		//Draw_AlphaFillRGB(x - name_length, y - AUTOID_ARMORBAR_OFFSET_Y, name_length * 2, 4, AUTOID_ARMORBAR_GREEN_ARMOR, 0.2);
		//Draw_AlphaFillRGB(x - name_length, y - AUTOID_ARMORBAR_OFFSET_Y, armor_length * 2, 4, AUTOID_ARMORBAR_GREEN_ARMOR, 1.0);
		Draw_AlphaFillRGB(x - name_length, y - AUTOID_ARMORBAR_OFFSET_Y, name_length * 2, 4, RGBA_TO_COLOR(AUTOID_ARMORBAR_GREEN_ARMOR, 50));
		Draw_AlphaFillRGB(x - name_length, y - AUTOID_ARMORBAR_OFFSET_Y, armor_length * 2, 4, RGBA_TO_COLOR(AUTOID_ARMORBAR_GREEN_ARMOR, 255));
	}
	else if(autoid_p->player->stats[STAT_ITEMS] & IT_ARMOR2)
	{
		// Yellow armor.
		strlcpy(armor_name, "&cff0YA", sizeof(armor_name));
		Draw_AlphaFillRGB(x - name_length, y - AUTOID_ARMORBAR_OFFSET_Y, name_length * 2, 4, RGBA_TO_COLOR(AUTOID_ARMORBAR_YELLOW_ARMOR, 50));
		Draw_AlphaFillRGB(x - name_length, y - AUTOID_ARMORBAR_OFFSET_Y, armor_length * 2, 4, RGBA_TO_COLOR(AUTOID_ARMORBAR_YELLOW_ARMOR, 255));
	}
	else if(autoid_p->player->stats[STAT_ITEMS] & IT_ARMOR3)
	{
		// Red armor.
		strlcpy(armor_name, "&cf00RA", sizeof(armor_name));
		Draw_AlphaFillRGB(x - name_length, y - AUTOID_ARMORBAR_OFFSET_Y, name_length * 2, 4, RGBA_TO_COLOR(AUTOID_ARMORBAR_RED_ARMOR, 50));
		Draw_AlphaFillRGB(x - name_length, y - AUTOID_ARMORBAR_OFFSET_Y, armor_length * 2, 4, RGBA_TO_COLOR(AUTOID_ARMORBAR_RED_ARMOR, 255));
	}

	if(scr_autoid.value >= 3 && autoid_p->player->stats[STAT_ITEMS] & (IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3))
	{
		//
		// Draw the name of the armor type.
		//
		Draw_ColoredString(
			x - AUTOID_ARMORNAME_OFFSET_X,
			y - AUTOID_ARMORNAME_OFFSET_Y,
			armor_name, 0);
	}

	if(scr_autoid.value >= 4)
	{
		// Draw the players weapon.
		int best_weapon = -1;
		extern mpic_t *sb_weapons[7][8];
		mpic_t *weapon_pic = NULL;

		best_weapon = BestWeaponFromStatItems(autoid_p->player->stats[STAT_ITEMS]);

		switch(best_weapon)
		{
			case IT_SHOTGUN:
				weapon_pic = sb_weapons[0][0];
				break;
			case IT_SUPER_SHOTGUN:
				weapon_pic = sb_weapons[0][1];
				break;
			case IT_NAILGUN:
				weapon_pic = sb_weapons[0][2];
				break;
			case IT_SUPER_NAILGUN:
				weapon_pic = sb_weapons[0][3];
				break;
			case IT_GRENADE_LAUNCHER:
				weapon_pic = sb_weapons[0][4];
				break;
			case IT_ROCKET_LAUNCHER:
				weapon_pic = sb_weapons[0][5];
				break;
			case IT_LIGHTNING:
				weapon_pic = sb_weapons[0][6];
				break;
			default :
				// No weapon.
				break;
		}

		// Only draw the best weapon if it's the RL when autoid = 4
		// Otherwise draw the current best weapon when autoid = 5
		if(weapon_pic != NULL &&
			((scr_autoid.value == 4 && best_weapon == IT_ROCKET_LAUNCHER)
			|| (scr_autoid.value >= 5 && best_weapon > 0)))
		{
			Draw_SSubPic (
				x - name_length - weapon_pic->width - AUTOID_WEAPON_OFFSET_X,
				y - AUTOID_HEALTHBAR_OFFSET_Y - Q_rint((weapon_pic->height/2.0)),
				weapon_pic,
				0,
				0,
				weapon_pic->width,
				weapon_pic->height,
				1);
		}
	}
}

void SCR_DrawAutoID (void)
{
	int i, x, y;

	if (!scr_autoid.value || (!cls.demoplayback && !cl.spectator))
		return;

	for (i = 0; i < autoid_count; i++)
	{
		x =  autoids[i].x * vid.width / glwidth;
		y =  (glheight - autoids[i].y) * vid.height / glheight;
		if(scr_autoid_drawname.value)
			Draw_String(x - strlen(autoids[i].player->name) * 4, y - 8, autoids[i].player->name);

		// We only have health/armor info for all players when in demo playback.
		if(cls.demoplayback && scr_autoid.value >= 2)
		{
			SCR_DrawAutoIDStatus(&autoids[i], x, y);
		}
	}
}


#endif

/**************************************** chat icon *****************************/

#ifdef GLQUAKE

// qqshka: code is a mixture of autoid and particle engine

typedef byte col_t[4]; // FIXME: why 4?

typedef struct ci_player_s {
	vec3_t		org;
	col_t		color;
	float		rotangle;
	float		size;
	byte		texindex;
	int			flags;

	player_info_t *player;

} ci_player_t;

static ci_player_t ci_clients[MAX_CLIENTS];
static int ci_count;

typedef enum {
	citex_chat,
	citex_afk,
	citex_chat_afk,
	num_citextures,
} ci_tex_t;

#define	MAX_CITEX_COMPONENTS		8
typedef struct ci_texture_s {
	int			texnum;
	int			components;
	float		coords[MAX_CITEX_COMPONENTS][4];
} ci_texture_t;

static ci_texture_t ci_textures[num_citextures];

qbool ci_initialized = false;

#define FONT_SIZE (256.0)

#define ADD_CICON_TEXTURE(_ptex, _texnum, _texindex, _components, _s1, _t1, _s2, _t2)	\
do {																					\
	ci_textures[_ptex].texnum = _texnum;												\
	ci_textures[_ptex].components = _components;										\
	ci_textures[_ptex].coords[_texindex][0] = (_s1 + 1) / FONT_SIZE;					\
	ci_textures[_ptex].coords[_texindex][1] = (_t1 + 1) / FONT_SIZE;					\
	ci_textures[_ptex].coords[_texindex][2] = (_s2 - 1) / FONT_SIZE;					\
	ci_textures[_ptex].coords[_texindex][3] = (_t2 - 1) / FONT_SIZE;					\
} while(0);

void CI_Init (void) {
	int ci_font;

	ci_initialized = false;

	if (!(ci_font = GL_LoadTextureImage ("textures/chaticons", "ci:chaticons", FONT_SIZE, FONT_SIZE, TEX_ALPHA | TEX_COMPLAIN)))
		return;

	ADD_CICON_TEXTURE(citex_chat,     ci_font, 0, 1,  0, 0,  64, 64); // get chat part from font
	ADD_CICON_TEXTURE(citex_afk,      ci_font, 0, 1, 64, 0, 128, 64); // get afk part
	ADD_CICON_TEXTURE(citex_chat_afk, ci_font, 0, 1,  0, 0, 128, 64); // get chat+afk part

	ci_initialized = true;
}

int CmpCI_Order(const void *p1, const void *p2)
{
	const ci_player_t	*a1 = (ci_player_t *) p1;
	const ci_player_t	*a2 = (ci_player_t *) p2;
	int l1, l2;
	vec3_t v;

	VectorSubtract (r_refdef.vieworg, a1->org, v);
	l1 = VectorLength (v);
	VectorSubtract (r_refdef.vieworg, a2->org, v);
	l2 = VectorLength (v);

	if (l1 > l2)
		return -1;
	if (l1 < l2)
		return  1;

	return 0;
}

void SCR_SetupCI (void) {
	int j, tracknum = -1;
	player_state_t *state;
	player_info_t *info;
	ci_player_t *id;
	centity_t *cent;
	char *s;

	ci_count = 0;

	if (!bound(0, r_chaticons_alpha.value, 1))
		return;

	if (cls.state != ca_active || !cl.validsequence)
		return;

	if (cl.spectator)
		tracknum = Cam_TrackNum();

	state = cl.frames[cl.parsecount & UPDATE_MASK].playerstate;
	info = cl.players;
	cent = &cl_entities[1];

	for (j = 0; j < MAX_CLIENTS; j++, info++, state++, cent++) {
		if (state->messagenum != cl.parsecount || j == cl.playernum || j == tracknum || info->spectator)
			continue;

		if (!*(s = Info_ValueForKey (info->userinfo, "chat")))
			continue; // user not chatting, so ignore

		id = &ci_clients[ci_count];
		id->texindex = 0;
		id->player = info;

		id->org[0] = cent->lerp_origin[0];
		id->org[1] = cent->lerp_origin[1];
		id->org[2] = cent->lerp_origin[2] + 33; // move baloon up a bit

		id->size = 8; // scale baloon
		id->rotangle = 5 * sin(2*r_refdef2.time); // may be set to 0, if u dislike rolling
		id->color[0] = 255; // r
		id->color[1] = 255; // g
		id->color[2] = 255; // b
		id->color[3] = 255 * bound(0, r_chaticons_alpha.value, 1); // alpha
		id->flags = Q_atoi(s) & (CIF_CHAT | CIF_AFK); // get known flags
		id->flags = (id->flags ? id->flags : CIF_CHAT); // use chat as default if we got some unknown "chat" value

		ci_count++;
	}

	if (ci_count) // sort icons so we draw most far to you first
		qsort((void *)ci_clients, ci_count, sizeof(ci_clients[0]), CmpCI_Order);
}

#define DRAW_CI_BILLBOARD(_ptex, _p, _coord)			\
	glPushMatrix();											\
	glTranslatef(_p->org[0], _p->org[1], _p->org[2]);		\
	glScalef(_p->size, _p->size, _p->size);					\
	if (_p->rotangle)										\
		glRotatef(_p->rotangle, vpn[0], vpn[1], vpn[2]);	\
															\
	glColor4ubv(_p->color);									\
															\
	glBegin(GL_QUADS);										\
	glTexCoord2f(_ptex->coords[_p->texindex][0], _ptex->coords[_p->texindex][3]); glVertex3fv(_coord[0]);	\
	glTexCoord2f(_ptex->coords[_p->texindex][0], _ptex->coords[_p->texindex][1]); glVertex3fv(_coord[1]);	\
	glTexCoord2f(_ptex->coords[_p->texindex][2], _ptex->coords[_p->texindex][1]); glVertex3fv(_coord[2]);	\
	glTexCoord2f(_ptex->coords[_p->texindex][2], _ptex->coords[_p->texindex][3]); glVertex3fv(_coord[3]);	\
	glEnd();			\
						\
	glPopMatrix();

// probably may be made as macros, but i hate macros cos macroses is unsafe
static void CI_Bind(ci_texture_t *citex, int *texture)
{
	//VULT PARTICLES - I gather this speeds it up, but I haven't really checked
	if (*texture != citex->texnum)
	{
		GL_Bind(citex->texnum);
		*texture = citex->texnum;
	}
}

void DrawCI (void) {
	int	i, texture = 0, flags;
	vec3_t billboard[4], billboard2[4], vright_tmp;
	ci_player_t *p;
	ci_texture_t *citex;

	if (!ci_initialized)
		return;

	if (!bound(0, r_chaticons_alpha.value, 1) || ci_count < 1)
		return;

	if (gl_fogenable.value)
	{
		glDisable(GL_FOG);
	}

	VectorAdd(vup, vright, billboard[2]);
	VectorSubtract(vright, vup, billboard[3]);
	VectorNegate(billboard[2], billboard[0]);
	VectorNegate(billboard[3], billboard[1]);

	VectorScale(vright, 2, vright_tmp);
	VectorAdd(vup, vright_tmp, billboard2[2]);
	VectorSubtract(vright_tmp, vup, billboard2[3]);
	VectorNegate(billboard2[2], billboard2[0]);
	VectorNegate(billboard2[3], billboard2[1]);

	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glShadeModel(GL_SMOOTH);

// FIXME: i'm not sure which blend mode here better
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_TEXTURE_2D);

	for (i = 0; i < ci_count; i++)
	{
		p = &ci_clients[i];
		flags = p->flags;

		if (flags & CIF_CHAT && flags & CIF_AFK)
		{
			flags = flags & ~(CIF_CHAT|CIF_AFK); // so they will be not showed below again
			CI_Bind(citex = &ci_textures[citex_chat_afk], &texture);
			DRAW_CI_BILLBOARD(citex, p, billboard2);
		}
		if (flags & CIF_CHAT)
		{
			CI_Bind(citex = &ci_textures[citex_chat], &texture);
			DRAW_CI_BILLBOARD(citex, p, billboard);
		}
		if (flags & CIF_AFK)
		{
			CI_Bind(citex = &ci_textures[citex_afk], &texture);
			DRAW_CI_BILLBOARD(citex, p, billboard);
		}
	}

	glEnable(GL_TEXTURE_2D);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glShadeModel(GL_FLAT);

	if (gl_fogenable.value)
	{
		glEnable(GL_FOG);
	}
}

#endif

// scr_teaminfo 
// Variable ti_clients and related functions also used by hud_teaminfo in hud_common.c
ti_player_t ti_clients[MAX_CLIENTS];

mpic_t * SCR_GetWeaponIconByFlag (int flag);

void SCR_ClearTeamInfo(void)
{
	memset(ti_clients, 0, sizeof(ti_clients));
}

char *SCR_GetWeaponShortNameByFlag (int flag)
{
	if (flag == IT_LIGHTNING || flag == IT_SUPER_LIGHTNING)
	{
		return "lg";
	}
	else if (flag == IT_ROCKET_LAUNCHER)
	{
		return "rl";
	}
	else if (flag == IT_GRENADE_LAUNCHER)
	{
		return "gl";
	}
	else if (flag == IT_SUPER_NAILGUN)
	{
		return "sng";
	}
	else if (flag == IT_NAILGUN)
	{
		return "ng";
	}
	else if (flag == IT_SUPER_SHOTGUN)
	{
		return "ssg";
	}
	else if (flag == IT_SHOTGUN)
	{
		return "sg";
	}

	return "";
}

static int SCR_Draw_TeamInfoPlayer(ti_player_t *ti_cl, int x, int y, int maxname, int maxloc, qbool width_only, qbool its_shownick)
{
	char *s, *loc, tmp[1024], tmp2[MAX_MACRO_STRING], *aclr;
	int x_in = x; // save x
	int i;
	mpic_t *pic;

	extern mpic_t  *sb_face_invis, *sb_face_quad, *sb_face_invuln;
	extern mpic_t  *sb_armor[3];

	if (!ti_cl)
		return 0;

	i = ti_cl->client;

	if (i < 0 || i >= MAX_CLIENTS)
	{
		Com_DPrintf("SCR_Draw_TeamInfoPlayer: wrong client %d\n", i);
		return 0;
	}

	// this limit len of string because TP_ParseFunChars() do not check overflow
	strlcpy(tmp2, its_shownick ? scr_shownick_order.string : scr_teaminfo_order.string , sizeof(tmp2));
	strlcpy(tmp2, TP_ParseFunChars(tmp2, false), sizeof(tmp2));
	s = tmp2;

	//
	// parse/draw string like this "%n %h:%a %l %p %w"
	//

	for ( ; *s; s++) {
		switch( (int) s[0] ) {
		case '%':

			s++; // advance

			switch( (int) s[0] ) {
			case 'n': // draw name

				if(!width_only) {
					char *nick = TP_ParseFunChars(ti_cl->nick[0] ? ti_cl->nick : cl.players[i].name, false);
					snprintf(tmp, sizeof(tmp), "%*.*s", maxname, maxname, nick);
					Draw_ColoredString (x, y, tmp, false);
				}
				x += maxname * FONTWIDTH;

				break;
			case 'w': // draw "best" weapon icon/name

				switch (scr_teaminfo_weapon_style.integer) {
				case 1:
					if(!width_only)
						Draw_ColoredString (x, y, SCR_GetWeaponShortNameByFlag(BestWeaponFromStatItems( ti_cl->items )), false);
					x += 3 * FONTWIDTH;

					break;
				default: // draw image by default
					if(!width_only)
						if ( (pic = SCR_GetWeaponIconByFlag(BestWeaponFromStatItems( ti_cl->items ))) )
							Draw_SPic (x, y, pic, 0.5);
					x += 2 * FONTWIDTH;

					break;
				}

				break;
			case 'h': // draw health, padding with space on left side
			case 'H': // draw health, padding with space on right side

				if(!width_only) {
					snprintf(tmp, sizeof(tmp), (s[0] == 'h' ? "%s%3d" : "%s%-3d"), (ti_cl->health < scr_teaminfo_low_health.integer ? "&cf00" : ""), ti_cl->health);
					Draw_ColoredString (x, y, tmp, false);
				}
				x += 3 * FONTWIDTH;

				break;
			case 'a': // draw armor, padded with space on left side
			case 'A': // draw armor, padded with space on right side

				aclr = "";

				//
				// different styles of armor
				//
				switch (scr_teaminfo_armor_style.integer) {
				case 1: // image prefixed armor value
					if(!width_only) {
						if (ti_cl->items & IT_ARMOR3)
							Draw_SPic (x, y, sb_armor[2], 1.0/3);
						else if (ti_cl->items & IT_ARMOR2)
							Draw_SPic (x, y, sb_armor[1], 1.0/3);
						else if (ti_cl->items & IT_ARMOR1)
							Draw_SPic (x, y, sb_armor[0], 1.0/3);
					}
					x += FONTWIDTH;

					break;
				case 2: // colored background of armor value
					if(!width_only) {
						byte col[4] = {255, 255, 255, 0};

						if (ti_cl->items & IT_ARMOR3) {
							col[0] = 255; col[1] =   0; col[2] =   0; col[3] = 255;
						}
						else if (ti_cl->items & IT_ARMOR2) {
							col[0] = 255; col[1] = 255; col[2] =   0; col[3] = 255;
						}
						else if (ti_cl->items & IT_ARMOR1) {
							col[0] =   0; col[1] = 255; col[2] =   0; col[3] = 255;
						}

#ifdef GLQUAKE
						glDisable (GL_TEXTURE_2D);
						glColor4ub(col[0], col[1], col[2], col[3]);
						glRectf(x, y, x + 3 * FONTWIDTH, y + 1 * FONTWIDTH);
						glEnable (GL_TEXTURE_2D);
						glColor4f(1, 1, 1, 1);
#endif
					}

					break;
				case 3: // colored armor value
					if(!width_only) {
						if (ti_cl->items & IT_ARMOR3)
							aclr = "&cf00";
						else if (ti_cl->items & IT_ARMOR2)
							aclr = "&cff0";
						else if (ti_cl->items & IT_ARMOR1)
							aclr = "&c0f0";
					}

					break;
				case 4: // armor value prefixed with letter
					if(!width_only) {
						if (ti_cl->items & IT_ARMOR3)
							Draw_ColoredString (x, y, "r", false);
						else if (ti_cl->items & IT_ARMOR2)
							Draw_ColoredString (x, y, "y", false);
						else if (ti_cl->items & IT_ARMOR1)
							Draw_ColoredString (x, y, "g", false);
					}
					x += FONTWIDTH;

					break;
				}

				if(!width_only) { // value drawed no matter which style
					snprintf(tmp, sizeof(tmp), (s[0] == 'a' ? "%s%3d" : "%s%-3d"), aclr, ti_cl->armor);
					Draw_ColoredString (x, y, tmp, false);
				}
				x += 3 * FONTWIDTH;

				break;
			case 'l': // draw location

				if(!width_only) {
					loc = TP_LocationName(ti_cl->org);
					if (!loc[0])
						loc = "unknown";

					snprintf(tmp, sizeof(tmp), "%*.*s", maxloc, maxloc, TP_ParseFunChars(loc, false));
					Draw_ColoredString (x, y, tmp, false);
				}
				x += maxloc * FONTWIDTH;

				break;
			case 'p': // draw powerups

				if(!width_only)
					if ( sb_face_quad && (ti_cl->items & IT_QUAD))
						Draw_SPic (x, y, sb_face_quad, 1.0/3);
				x += FONTWIDTH;

				if(!width_only)
					if ( sb_face_invuln && (ti_cl->items & IT_INVULNERABILITY))
						Draw_SPic (x, y, sb_face_invuln, 1.0/3);
				x += FONTWIDTH;

				if(!width_only)
					if ( sb_face_invis && (ti_cl->items & IT_INVISIBILITY))
						Draw_SPic (x, y, sb_face_invis, 1.0/3);
				x += FONTWIDTH;

				break;
			case '%': // wow, %% result in one %, how smart

				if(!width_only)
					Draw_ColoredString (x, y, "%", false);
				x += FONTWIDTH;

				break;

			default: // print %x - that mean sequence unknown

				if(!width_only) {
					snprintf(tmp, sizeof(tmp), "%%%c", s[0]);
					Draw_ColoredString (x, y, tmp, false);
				}
				x += (s[0] ? 2 : 1) * FONTWIDTH;

				break;
			}

			break;

		default: // print x
			if(!width_only) {
				snprintf(tmp, sizeof(tmp), "%c", s[0]);
				if (s[0] != ' ') // inhuman smart optimization, do not print space!
					Draw_ColoredString (x, y, tmp, false);
			}
			x += FONTWIDTH;

			break;
		}
	}

	return (x - x_in) / FONTWIDTH; // return width
}

static void SCR_Draw_TeamInfo(void)
{
	int x, y, w, h;
	int i, j, slots[MAX_CLIENTS], slots_num, maxname, maxloc;
	char tmp[1024], *nick;

	float	scale = bound(0.1, scr_teaminfo_scale.value, 10);

	if ( CURRVIEW != 1 && CURRVIEW != 0)
		return;

	if ( !cl.teamplay || !scr_teaminfo.integer )  // non teamplay mode
		return;

	if (cls.mvdplayback)
		Update_TeamInfo();

	// fill data we require to draw teaminfo
	for ( maxloc = maxname = slots_num = i = 0; i < MAX_CLIENTS; i++ ) {
		if ( !cl.players[i].name[0] || cl.players[i].spectator
				|| !ti_clients[i].time || ti_clients[i].time + TI_TIMEOUT < r_refdef2.time
		 	)
			continue;

		// do not show enemy players unless it's MVD and user wishes to show them
		if (VX_TrackerIsEnemy( i ) && (!cls.mvdplayback || !scr_teaminfo_show_enemies.integer))
				continue;

		// do not show tracked player to spectator
		if ((cl.spectator && Cam_TrackNum() == i) && !TEAMINFO_SHOWSELF())
			continue;

		// dynamically guess max length of name/location
		nick = (ti_clients[i].nick[0] ? ti_clients[i].nick : cl.players[i].name); // we use nick or name
		maxname = max(maxname, strlen(TP_ParseFunChars(nick, false)));

		strlcpy(tmp, TP_LocationName(ti_clients[i].org), sizeof(tmp));
		maxloc  = max(maxloc,  strlen(TP_ParseFunChars(tmp,  false)));

		slots[slots_num++] = i;
	}

	// well, better use fixed loc length
	maxloc  = bound(0, scr_teaminfo_loc_width.integer, 100);
	// limit name length
	maxname = bound(0, maxname, scr_teaminfo_name_width.integer);

	if ( !slots_num )
		return;

#ifdef GLQUAKE
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glColor4f(1, 1, 1, 1);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_BLEND);

	if (scale != 1) {
		glPushMatrix ();
		glScalef(scale, scale, 1);
	}
#endif

	y = vid.height*0.6/scale + scr_teaminfo_y.value;

	// this does't draw anything, just calculate width
	w = SCR_Draw_TeamInfoPlayer(&ti_clients[0], 0, 0, maxname, maxloc, true, false);
	h = slots_num;

	for ( j = 0; j < slots_num; j++ ) {
		i = slots[j];

		x = (scr_teaminfo_align_right.value ? (vid.width/scale - w * FONTWIDTH) - FONTWIDTH : FONTWIDTH);
		x += scr_teaminfo_x.value;

		if ( !j ) { // draw frame
#ifdef GLQUAKE
			byte	*col = scr_teaminfo_frame_color.color;
			glDisable (GL_TEXTURE_2D);
			glColor4ub(col[0], col[1], col[2], col[3]);
			glRectf(x, y, x + w * FONTWIDTH, y + h * FONTWIDTH);
			glEnable (GL_TEXTURE_2D);
			glColor4f(1, 1, 1, 1);
#endif
		}

		SCR_Draw_TeamInfoPlayer(&ti_clients[i], x, y, maxname, maxloc, false, false);

		y += FONTWIDTH;
	}

#ifdef GLQUAKE
	if (scale != 1)
		glPopMatrix();

	glEnable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glColor4f(1, 1, 1, 1);
#endif
}

void Parse_TeamInfo(char *s)
{
	int		client;

	Cmd_TokenizeString( s );

	client = atoi( Cmd_Argv( 1 ) );

	if (client < 0 || client >= MAX_CLIENTS)
	{
		Com_DPrintf("Parse_TeamInfo: wrong client %d\n", client);
		return;
	}

	ti_clients[ client ].client = client; // no, its not stupid

	ti_clients[ client ].time   = r_refdef2.time;

	ti_clients[ client ].org[0] = atoi( Cmd_Argv( 2 ) );
	ti_clients[ client ].org[1] = atoi( Cmd_Argv( 3 ) );
	ti_clients[ client ].org[2] = atoi( Cmd_Argv( 4 ) );
	ti_clients[ client ].health = atoi( Cmd_Argv( 5 ) );
	ti_clients[ client ].armor  = atoi( Cmd_Argv( 6 ) );
	ti_clients[ client ].items  = atoi( Cmd_Argv( 7 ) );
	strlcpy(ti_clients[ client ].nick, Cmd_Argv( 8 ), TEAMINFO_NICKLEN); // nick is optional
}

void Update_TeamInfo()
{
	int		i;
	int		*st;

	static double lastupdate = 0;

	if (!cls.mvdplayback)
		return;

	// don't update each frame - it's less disturbing
	if (cls.realtime - lastupdate < 1)
		return;

	lastupdate = cls.realtime;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (cl.players[i].spectator || !cl.players[i].name[0])
			continue;

		st = cl.players[i].stats;	

		ti_clients[i].client  = i; // no, its not stupid

		ti_clients[i].time    = r_refdef2.time;

		VectorCopy(cl.frames[cl.parsecount & UPDATE_MASK].playerstate[i].origin, ti_clients[i].org);

		ti_clients[i].health  = bound(0, st[STAT_HEALTH], 999);
		ti_clients[i].armor   = bound(0, st[STAT_ARMOR],  999);
		ti_clients[i].items   = st[STAT_ITEMS];
		ti_clients[i].nick[0] = 0; // sad, we does't have nick, will use name
	}
}

/***************************** customizeable shownick *************************/

#ifdef GLQUAKE

static ti_player_t shownick;

void SCR_ClearShownick(void)
{
	memset(&shownick, 0, sizeof(shownick));
}

void Parse_Shownick(char *s)
{
	int		client, version, arg;

	Cmd_TokenizeString( s );

	arg = 1;

	version  = atoi( Cmd_Argv( arg++ ) );

	switch ( version )
	{
		case 1:
		{
			client = atoi( Cmd_Argv( arg++ ) );
        
			if (client < 0 || client >= MAX_CLIENTS)
			{
				Com_DPrintf("Parse_Shownick: wrong client %d\n", client);
				return;
			}

			shownick.client = client;
	        
			shownick.time   = r_refdef2.time;
        
			shownick.org[0] = atoi( Cmd_Argv( arg++ ) );
			shownick.org[1] = atoi( Cmd_Argv( arg++ ) );
			shownick.org[2] = atoi( Cmd_Argv( arg++ ) );
			shownick.health = atoi( Cmd_Argv( arg++ ) );
			shownick.armor  = atoi( Cmd_Argv( arg++ ) );
			shownick.items  = atoi( Cmd_Argv( arg++ ) );
			strlcpy(shownick.nick, Cmd_Argv( arg++ ), TEAMINFO_NICKLEN); // nick is optional

			return;
		}

		default:

		Com_DPrintf("Parse_Shownick: unsupported version %d\n", version);
		return;
	}
}

static void SCR_Draw_ShowNick(void)
{
	qbool	scr_shownick_align_right = false;
	int		x, y, w, h;
	int		maxname, maxloc;
	byte	*col;
	float	scale = bound(0.1, scr_shownick_scale.value, 10);

	// check do we have something do draw
	if (!shownick.time || shownick.time + bound(0.1, scr_shownick_time.value, 3) < r_refdef2.time)
		return;

	// loc is unused
	maxloc = 0;

	// limit name length
	maxname = 999;
	maxname = bound(0, maxname, scr_shownick_name_width.integer);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glColor4f(1, 1, 1, 1);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_BLEND);

	if (scale != 1)
	{
		glPushMatrix ();
		glScalef(scale, scale, 1);
	}

	y = vid.height*0.6/scale + scr_shownick_y.value;

	// this does't draw anything, just calculate width
	w = SCR_Draw_TeamInfoPlayer(&shownick, 0, 0, maxname, maxloc, true, true);
	h = 1;

	x = (scr_shownick_align_right ? (vid.width/scale - w * FONTWIDTH) - FONTWIDTH : FONTWIDTH);
	x += scr_shownick_x.value;

	// draw frame
	col = scr_shownick_frame_color.color;
	glDisable (GL_TEXTURE_2D);
	glColor4ub(col[0], col[1], col[2], col[3]);
	glRectf(x, y, x + w * FONTWIDTH, y + h * FONTWIDTH);
	glEnable (GL_TEXTURE_2D);
	glColor4f(1, 1, 1, 1);

	// draw shownick
	SCR_Draw_TeamInfoPlayer(&shownick, x, y, maxname, maxloc, false, true);

	if (scale != 1)
		glPopMatrix();

	glEnable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glColor4f(1, 1, 1, 1);
}

#endif

/***************************** weapon stats *************************/

typedef enum
{
	wpNONE = 0,
	wpAXE,
	wpSG,
	wpSSG,
	wpNG,
	wpSNG,
	wpGL,
	wpRL,
	wpLG,
	wpMAX

} weaponName_t;

typedef struct wpType_s {

	int hits;			// hits with this weapon, for SG and SSG this is count of bullets
	int attacks;		// all attacks with this weapon, for SG and SSG this is count of bullets

} wpType_t;


typedef struct ws_player_s {

	int 		client;

	wpType_t	wpn[wpMAX];

} ws_player_t;

static ws_player_t ws_clients[MAX_CLIENTS];

void SCR_ClearWeaponStats(void)
{
	memset(ws_clients, 0, sizeof(ws_clients));
}

static weaponName_t WS_NameToNum(const char *name)
{
	if ( !strcmp(name, "axe") )
		return wpAXE;
	if ( !strcmp(name, "sg") )
		return wpSG;
	if ( !strcmp(name, "ssg") )
		return wpSSG;
	if ( !strcmp(name, "ng") )
		return wpNG;
	if ( !strcmp(name, "sng") )
		return wpSNG;
	if ( !strcmp(name, "gl") )
		return wpGL;
	if ( !strcmp(name, "rl") )
		return wpRL;
	if ( !strcmp(name, "lg") )
		return wpLG;

	return wpNONE;
}

/*
static char *WS_NumToName( weaponName_t wp )
{
	switch ( wp ) {
		case wpAXE: return "axe";
		case wpSG:  return "sg";
		case wpSSG: return "ssg";
		case wpNG:  return "ng";
		case wpSNG: return "sng";
		case wpGL:  return "gl";
		case wpRL:  return "rl";
		case wpLG:  return "lg";

		// shut up gcc
		case wpNONE:
		case wpMAX: return "unknown";
	}

	return "unknown";
}
*/

void Parse_WeaponStats(char *s)
{
	int		client, arg;
	weaponName_t wp;

	Cmd_TokenizeString( s );

	arg = 1;

	client = atoi( Cmd_Argv( arg++ ) );
        
	if (client < 0 || client >= MAX_CLIENTS)
	{
		Com_DPrintf("Parse_WeaponStats: wrong client %d\n", client);
		return;
	}

	ws_clients[ client ].client = client; // no, its not stupid

	wp = WS_NameToNum( Cmd_Argv( arg++ ) );

	if ( wp == wpNONE )
	{
		Com_DPrintf("Parse_WeaponStats: wrong weapon\n");
		return;
	}

	ws_clients[ client ].wpn[wp].attacks = atoi( Cmd_Argv( arg++ ) );
	ws_clients[ client ].wpn[wp].hits    = atoi( Cmd_Argv( arg++ ) );
}

static int SCR_Draw_WeaponStatsPlayer(ws_player_t *ws_cl, int x, int y, qbool width_only)
{
	char *s, tmp[1024], tmp2[MAX_MACRO_STRING], *start, *end;
	qbool percentage;
	int x_in = x; // save x
	int i;
	weaponName_t wp;

	if (!ws_cl)
		return 0;

	i = ws_cl->client;

	if (i < 0 || i >= MAX_CLIENTS)
	{
		Com_DPrintf("SCR_Draw_WeaponStatsPlayer: wrong client %d\n", i);
		return 0;
	}

	// this limit len of string because TP_ParseFunChars() do not check overflow
	strlcpy(tmp2, scr_weaponstats_order.string , sizeof(tmp2));
	strlcpy(tmp2, TP_ParseFunChars(tmp2, false), sizeof(tmp2));
	s = tmp2;

	//
	// parse/draw string like this "#1 %2 %3 #7 %8"
	//

	start = s; // set start

	for ( ; *s; s++)
	{
		if ( s[0] != '%' && s[0] != '#' )
			continue;

		end = s;

		percentage = (s[0] == '%');

		s++; // advance

		wp = (int)s[0] - '0'; 

		if ( wp <= wpNONE || wp >= wpMAX )
			continue; // unsupported weapon

		if ( (int)(end - start) > 0 )
		{
			strlcpy(tmp, start, bound(1, (int)(end - start + 1), (int)sizeof(tmp)));

			if( !width_only )
				Draw_ColoredString (x, y, tmp, false);

			x += strlen_color(tmp) * FONTWIDTH;
		}

		start = s + 1; // set new start

		if ( percentage )
		{
			float	accuracy = (ws_cl->wpn[wp].attacks ? 100.0f * ws_cl->wpn[wp].hits / ws_cl->wpn[wp].attacks : 0 );
			snprintf(tmp, sizeof(tmp), "%.1f", accuracy);
		}
		else
		{
			snprintf(tmp, sizeof(tmp), "%d", ws_cl->wpn[wp].hits);
		}

		if(!width_only)
			Draw_ColoredString (x, y, tmp, false);

		x += strlen_color(tmp) * FONTWIDTH;
	}

	return (x - x_in) / FONTWIDTH; // return width
}

static void SCR_Draw_WeaponStats(void)
{
	int x, y, w, h;
	int i;

#ifdef GLQUAKE
	byte	*col = scr_weaponstats_frame_color.color;
#endif
	float	scale = bound(0.1, scr_weaponstats_scale.value, 10);

	if ( !scr_weaponstats.integer || CURRVIEW > 1 )
		return;

	i = ( cl.spectator ? Cam_TrackNum() : cl.playernum );

	if ( i < 0 || i >= MAX_CLIENTS )
		return;

#ifdef GLQUAKE
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glColor4f(1, 1, 1, 1);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_BLEND);

	if (scale != 1)
	{
		glPushMatrix ();
		glScalef(scale, scale, 1);
	}
#endif

	y = vid.height*0.6/scale + scr_weaponstats_y.value;

	// this does't draw anything, just calculate width
	w = SCR_Draw_WeaponStatsPlayer(&ws_clients[i], 0, 0, true);
	h = 1;

	x = (scr_weaponstats_align_right.value ? (vid.width/scale - w * FONTWIDTH) - FONTWIDTH : FONTWIDTH);
	x += scr_weaponstats_x.value;

	// draw frame
#ifdef GLQUAKE
	glDisable (GL_TEXTURE_2D);
	glColor4ub(col[0], col[1], col[2], col[3]);
	glRectf(x, y, x + w * FONTWIDTH, y + h * FONTWIDTH);
	glEnable (GL_TEXTURE_2D);
	glColor4f(1, 1, 1, 1);
#endif

	SCR_Draw_WeaponStatsPlayer(&ws_clients[i], x, y, false);

#ifdef GLQUAKE
	if (scale != 1)
		glPopMatrix();

	glEnable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glColor4f(1, 1, 1, 1);
#endif
}

void OnChange_scr_weaponstats (cvar_t *var, char *value, qbool *cancel)
{
	extern void CL_UserinfoChanged (char *key, char *value);

	// do not allow set "wpsx" to "0" instead set it to ""
	CL_UserinfoChanged("wpsx", strcmp(value, "0") ? value : "");
}

static void SCR_MvdWeaponStatsOn_f(void)
{
	Cvar_Set (&scr_weaponstats, "1");
}

static void SCR_MvdWeaponStatsOff_f(void)
{
	Cvar_Set (&scr_weaponstats, "0");
}

/**************************************** 262 HUD *****************************/
// QW262 -->
hud_element_t *hud_list=NULL; 
hud_element_t *prev;
 
hud_element_t *Hud_FindElement(const char *name)
{
	hud_element_t *elem;

	prev=NULL;
	for(elem=hud_list; elem; elem = elem->next) {
		if (!strcasecmp(name, elem->name))
			return elem;
		prev = elem;
	}

	return NULL;
}

qbool Hud_ElementExists(const char* name)
{
	return Hud_FindElement(name) != NULL;
}

static hud_element_t* Hud_NewElement(void)
{
	hud_element_t*	elem;
	elem = (hud_element_t *) Q_malloc (sizeof(hud_element_t));
	elem->next = hud_list;
	hud_list = elem;
	elem->name = Q_strdup( Cmd_Argv(1) );
	return elem;
}

static void Hud_DeleteElement(hud_element_t *elem)
{
	if (elem->flags & (HUD_STRING|HUD_IMAGE))
		Q_free(elem->contents);
	if (elem->f_hover)
		Q_free(elem->f_hover);
	if (elem->f_button)
		Q_free(elem->f_button);
	Q_free(elem->name);
	Q_free(elem);
}

typedef void Hud_Elem_func(hud_element_t*);

void Hud_ReSearch_do(Hud_Elem_func f)
{
	hud_element_t *elem;

	for(elem=hud_list; elem; elem = elem->next) {
		if (ReSearchMatch(elem->name))
			f(elem);
	}
}

void Hud_Add_f(void)
{
	hud_element_t*	elem;
	struct hud_element_s*	next = NULL;	// Sergio
	qbool	hud_restore = false;	// Sergio
	cvar_t		*var;
	char		*a2, *a3;
	//Hud_Func	func;
	unsigned	old_coords = 0;
	unsigned	old_width = 0;
	float		old_alpha = 1;
	qbool	old_enabled = true;

	if (Cmd_Argc() != 4)
		Com_Printf("Usage: hud_add <name> <type> <param>\n");
	else {
		if ((elem=Hud_FindElement(Cmd_Argv(1)))) {
			if (elem) {
				old_coords = *((unsigned*)&(elem->coords));
				old_width = elem->width;
				old_alpha = elem->alpha;
				old_enabled = elem->flags & HUD_ENABLED;
				next = elem->next; // Sergio
				if (prev) {
					prev->next = elem->next;
					hud_restore = true; // Sergio
				} else
					hud_list = elem->next;
				Hud_DeleteElement(elem);
			}
		}

		a2 = Cmd_Argv(2);
		a3 = Cmd_Argv(3);

		if (!strcasecmp(a2, "cvar")) {
			if( (var = Cvar_Find(a3)) ) {
				elem = Hud_NewElement();
				elem->contents = var;
				elem->flags = HUD_CVAR | HUD_ENABLED;
			} else {
				Com_Printf("cvar \"%s\" not found\n", a3);
				return;
			}
		} else if (!strcasecmp(a2, "str")) {
			elem = Hud_NewElement();
			elem->contents = Q_strdup( a3 );
			elem->flags = HUD_STRING | HUD_ENABLED;
		/*} else if (!strcasecmp(a2, "std")) { // to add armor, health, ammo, speed
			if (!strcasecmp(a3, "lag"))
				func = &Hud_LagmeterStr;
			else if (!strcasecmp(a3, "fps"))
				func = &Hud_FpsStr;
			else if (!strcasecmp(a3, "clock"))
				func = &Hud_ClockStr;
			else if (!strcasecmp(a3, "speed"))
				func = &Hud_SpeedStr;
			else {
				Com_Printf("\"%s\" is not a standard hud function\n", a3);
				return;
			}
			elem = Hud_NewElement();
			elem->contents = func;
			elem->flags = HUD_FUNC | HUD_ENABLED;
		} else if (!strcasecmp(a2, "img")) {
#ifdef GLQUAKE
			mpic_t *hud_image;
			int texnum = loadtexture_24bit(a3, LOADTEX_GFX);
			if (!texnum) {
				Com_Printf("Unable to load hud image \"%s\"\n", a3);
				return;
			}
			hud_image = (mpic_t *) Q_malloc (sizeof(mpic_t));
			hud_image->texnum = texnum;
			if (current_texture) {
				hud_image->width = current_texture->width;
				hud_image->height = current_texture->height;
			}
			else {
				hud_image->width = image.width;
				hud_image->height = image.height;
			}
			hud_image->sl = 0;
			hud_image->sh = 1;
			hud_image->tl = 0;
			hud_image->th = 1;
			elem = Hud_NewElement();
			elem->contents = hud_image;
			elem->flags = HUD_IMAGE | HUD_ENABLED;
#else
			Com_Printf("Hud images not available in software version\n");
			return;
#endif
		*/} else {
			Com_Printf("\"%s\" is not a valid hud type\n", a2);
			return;
		}

		*((unsigned*)&(elem->coords)) = old_coords;
		elem->width = old_width;
		elem->alpha = old_alpha;
		if (!old_enabled)
			elem->flags &= ~HUD_ENABLED;

// Sergio -->
// Restoring old hud place in hud_list
		if (hud_restore) {
			hud_list = elem->next;
			elem->next = next;
			prev->next = elem;
			}
// <-- Sergio
	}
}

void Hud_Elem_Remove(hud_element_t *elem)
{
	if (prev)
		prev->next = elem->next;
	else
		hud_list = elem->next;
	Hud_DeleteElement(elem);
}

void Hud_Remove_f(void)
{
	hud_element_t	*elem, *next_elem;
	char			*name;
	int				i;

	for (i=1; i<Cmd_Argc(); i++) {
		name = Cmd_Argv(i);
		if (IsRegexp(name)) {
			if(!ReSearchInit(name))
				return;
			prev = NULL;
			for(elem=hud_list; elem; ) {
				if (ReSearchMatch(elem->name)) {
					next_elem = elem->next;
					Hud_Elem_Remove(elem);
					elem = next_elem;
				} else {
					prev = elem;
					elem = elem->next;
				}
			}
			ReSearchDone();
		} else {
			if ((elem = Hud_FindElement(name)))
				Hud_Elem_Remove(elem);
			else
				Com_Printf("HudElement \"%s\" not found\n", name);
		}
	}
}

void Hud_Position_f(void)
{
	hud_element_t *elem;

	if (Cmd_Argc() != 5) {
		Com_Printf("Usage: hud_position <name> <pos_type> <x> <y>\n");
		return;
	}
	if (!(elem = Hud_FindElement(Cmd_Argv(1)))) {
		Com_Printf("HudElement \"%s\" not found\n", Cmd_Argv(1));
		return;
	}

	elem->coords[0] = atoi(Cmd_Argv(2));
	elem->coords[1] = atoi(Cmd_Argv(3));
	elem->coords[2] = atoi(Cmd_Argv(4));
}

void Hud_Elem_Bg(hud_element_t *elem)
{
	elem->coords[3] = atoi(Cmd_Argv(2));
}

void Hud_Bg_f(void)
{
	hud_element_t *elem;
	char	*name = Cmd_Argv(1);

	if (Cmd_Argc() != 3)
		Com_Printf("Usage: hud_bg <name> <bgcolor>\n");
	else if (IsRegexp(name)) {
		if(!ReSearchInit(name))
			return;
		Hud_ReSearch_do(Hud_Elem_Bg);
		ReSearchDone();
	} else {
		if ((elem = Hud_FindElement(name)))
			Hud_Elem_Bg(elem);
		else
			Com_Printf("HudElement \"%s\" not found\n", name);
	}
}

void Hud_Elem_Move(hud_element_t *elem)
{
	elem->coords[1] += atoi(Cmd_Argv(2));
	elem->coords[2] += atoi(Cmd_Argv(3));
}

void Hud262_Move_f(void)
{
	hud_element_t *elem;
	char	*name = Cmd_Argv(1);

	if (Cmd_Argc() != 4)
		Com_Printf("Usage: hud_move <name> <dx> <dy>\n");
	else if (IsRegexp(name)) {
		if(!ReSearchInit(name))
			return;
		Hud_ReSearch_do(Hud_Elem_Move);
		ReSearchDone();
	} else {
		if ((elem = Hud_FindElement(name)))
			Hud_Elem_Move(elem);
		else
			Com_Printf("HudElement \"%s\" not found\n", name);
	}
}

void Hud_Elem_Width(hud_element_t *elem)
{
	if (elem->flags & HUD_IMAGE) {
		mpic_t *pic = elem->contents;
		int width = atoi(Cmd_Argv(2))*8;
		int height = width * pic->height / pic->width;
		pic->height = height;
		pic->width = width;
	}
	elem->width = max(min(atoi(Cmd_Argv(2)), 128), 0);
}

void Hud_Width_f(void)
{
	hud_element_t *elem;
	char	*name = Cmd_Argv(1);

	if (Cmd_Argc() != 3)
		Com_Printf("Usage: hud_width <name> <width>\n");
	else if (IsRegexp(name)) {
		if(!ReSearchInit(name))
			return;
		Hud_ReSearch_do(Hud_Elem_Width);
		ReSearchDone();
	} else {
		if ((elem = Hud_FindElement(name)))
			Hud_Elem_Width(elem);
		else
			Com_Printf("HudElement \"%s\" not found\n", name);
	}
}

#ifdef GLQUAKE

/*
void Hud_Elem_Font(hud_element_t *elem)
{
	if (elem->flags & HUD_IMAGE)
		return;

	elem->charset = loadtexture_24bit (Cmd_Argv(2), LOADTEX_CHARS);

}

void Hud_Font_f(void)
{
	hud_element_t *elem;
	char	*name = Cmd_Argv(1);

	if (Cmd_Argc() != 3)
		Com_Printf("Usage: hud_font <name> <font>\n");
	else if (IsRegexp(name)) {
		if(!ReSearchInit(name))
			return;
		Hud_ReSearch_do(Hud_Elem_Font);
		ReSearchDone();
	} else {
		if ((elem = Hud_FindElement(name)))
			Hud_Elem_Font(elem);
		else
			Com_Printf("HudElement \"%s\" not found\n", name);
	}
}*/

void Hud_Elem_Alpha(hud_element_t *elem)
{
	float alpha = atof (Cmd_Argv(2));
	elem->alpha = bound (0, alpha, 1);
}

void Hud_Alpha_f(void)
{
	hud_element_t *elem;
	char	*name = Cmd_Argv(1);

	if (Cmd_Argc() != 3)
	{
		Com_Printf("hud_alpha <name> <value> : set HUD transparency (0..1)\n");
		return;
	}
	if (IsRegexp(name)) {
		if(!ReSearchInit(name))
			return;
		Hud_ReSearch_do(Hud_Elem_Alpha);
		ReSearchDone();
	} else {
		if ((elem = Hud_FindElement(name)))
			Hud_Elem_Alpha(elem);
		else
			Com_Printf("HudElement \"%s\" not found\n", name);
	}
}
#endif

void Hud_Elem_Blink(hud_element_t *elem)
{
	double		blinktime;
	unsigned	mask;

	blinktime = atof(Cmd_Argv(2))/1000.0;
	mask = atoi(Cmd_Argv(3));

	if (mask > 3) return; // bad mask
	if (blinktime < 0.0 || blinktime > 5.0) return;

	elem->blink = blinktime;
	elem->flags = (elem->flags & (~(HUD_BLINK_F | HUD_BLINK_B))) | (mask << 3);
}

void Hud_Blink_f(void)
{
	hud_element_t *elem;
	char	*name = Cmd_Argv(1);

	if (Cmd_Argc() != 4)
		Com_Printf("Usage: hud_blink <name> <ms> <mask>\n");
	else if (IsRegexp(name)) {
		if(!ReSearchInit(name))
			return;
		Hud_ReSearch_do(Hud_Elem_Blink);
		ReSearchDone();
	} else {
		if ((elem = Hud_FindElement(name)))
			Hud_Elem_Blink(elem);
		else
			Com_Printf("HudElement \"%s\" not found\n", name);
	}
}

void Hud_Elem_Disable(hud_element_t *elem)
{
	elem->flags &= ~HUD_ENABLED;
}

void Hud_Disable_f(void)
{
	hud_element_t	*elem;
	char			*name;
	int				i;

	for (i=1; i<Cmd_Argc(); i++) {
		name = Cmd_Argv(i);
		if (IsRegexp(name)) {
			if(!ReSearchInit(name))
				return;
			Hud_ReSearch_do(Hud_Elem_Disable);
			ReSearchDone();
		} else {
			if ((elem = Hud_FindElement(name)))
				Hud_Elem_Disable(elem);
			else
				Com_Printf("HudElement \"%s\" not found\n", name);
		}
	}
}

void Hud_Elem_Enable(hud_element_t *elem)
{
	elem->flags |= HUD_ENABLED;
}

void Hud_Enable_f(void)
{
	hud_element_t	*elem;
	char			*name;
	int				i;

	for (i=1; i<Cmd_Argc(); i++) {
		name = Cmd_Argv(i);
		if (IsRegexp(name)) {
			if(!ReSearchInit(name))
				return;
			Hud_ReSearch_do(Hud_Elem_Enable);
			ReSearchDone();
		} else {
			if ((elem = Hud_FindElement(name)))
				Hud_Elem_Enable(elem);
			else
				Com_Printf("HudElement \"%s\" not found\n", name);
		}
	}
}

void Hud_List_f(void)
{
	hud_element_t	*elem;
	char			*type;
	char			*param;
	int				c, i, m;

	c = Cmd_Argc();
	if (c>1)
		if (!ReSearchInit(Cmd_Argv(1)))
			return;

	Com_Printf ("List of hud elements:\n");
	for(elem=hud_list, i=m=0; elem; elem = elem->next, i++) {
		if (c==1 || ReSearchMatch(elem->name)) {
			if (elem->flags & HUD_CVAR) {
				type = "cvar";
				param = ((cvar_t*)elem->contents)->name;
			} else if (elem->flags & HUD_STRING) {
				type = "str";
				param = elem->contents;
			} else if (elem->flags & HUD_FUNC) {
				type = "std";
				param = "***";
			} else if (elem->flags & HUD_IMAGE) {
				type = "img";
				param = "***";
			} else {
				type = "invalid type";
				param = "***";
			}
			m++;
			Com_Printf("%s : %s : %s\n", elem->name, type, param);
		}
	}

	Com_Printf ("------------\n%i/%i hud elements\n", m, i);
	if (c>1)
		ReSearchDone();
}

void Hud_BringToFront_f(void)
{
	hud_element_t *elem, *start, *end;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: hud_bringtofront <name>\n");
		return;
	}

	end = Hud_FindElement(Cmd_Argv(1));
	if (end) {
		if (end->next) {
			start = hud_list;
			hud_list = end->next;
			end->next = NULL;
			for(elem=hud_list; elem->next; elem = elem->next) {}
			elem->next = start;
		}
	} else {
		Com_Printf("HudElement \"%s\" not found\n", Cmd_Argv(1));
	}
}

/*void Hud_Hover_f (void)
{
	hud_element_t *elem;

	if (Cmd_Argc() != 3) {
		Com_Printf("hud_hover <name> <alias> : call alias when mouse is over hud\n");
		return;
	}

	elem = Hud_FindElement(Cmd_Argv(1));
	if (elem) {
		if (elem->f_hover)
			Q_free (elem->f_hover);
		elem->f_hover = Q_strdup (Cmd_Argv(2));
	} else {
		Com_Printf("HudElement \"%s\" not found\n", Cmd_Argv(1));
	}
}

void Hud_Button_f (void)
{
	hud_element_t *elem;

	if (Cmd_Argc() != 3) {
		Com_Printf("hud_button <name> <alias> : call alias when mouse button pressed on hud\n");
		return;
	}

	elem = Hud_FindElement(Cmd_Argv(1));
	if (elem) {
		if (elem->f_button)
			Q_free (elem->f_button);
		elem->f_button = Q_strdup (Cmd_Argv(2));
	} else {
		Com_Printf("HudElement \"%s\" not found\n", Cmd_Argv(1));
	}
}*/

qbool Hud_TranslateCoords (hud_element_t *elem, int *x, int *y)
{
	int l;

	if (!elem->scr_width || !elem->scr_height)
		return false;

	l = elem->scr_width / 8;

	switch (elem->coords[0]) {
		case 1:	*x = elem->coords[1]*8 + 1;					// top left
				*y = elem->coords[2]*8;
				break;
		case 2:	*x = vid.conwidth - (elem->coords[1] + l)*8 -1;	// top right
				*y = elem->coords[2]*8;
				break;
		case 3:	*x = vid.conwidth - (elem->coords[1] + l)*8 -1;	// bottom right
				*y = vid.conheight - sb_lines - (elem->coords[2]+1)*8;
				break;
		case 4:	*x = elem->coords[1]*8 + 1;					// bottom left
				*y = vid.conheight - sb_lines - (elem->coords[2]+1)*8;
				break;
		case 5:	*x = vid.conwidth / 2 - l*4 + elem->coords[1]*8;// top center
				*y = elem->coords[2]*8;
				break;
		case 6:	*x = vid.conwidth / 2 - l*4 + elem->coords[1]*8;// bottom center
				*y = vid.conheight - sb_lines - (elem->coords[2]+1)*8;
				break;
		default:
				return false;
	}
	return true;
}

void SCR_DrawHud (void)
{
	hud_element_t* elem;
	int x,y;
	unsigned int l;
	char buf[1024];
	char *st = NULL;
	Hud_Func func;
	double tblink = 0.0;
	mpic_t *img = NULL;

	if (hud_list && cl_hud.value && !((cls.demoplayback || cl.spectator) && cl_restrictions.value)) {
		for (elem = hud_list; elem; elem=elem->next) {
			if (!(elem->flags & HUD_ENABLED))
				continue; // do not draw disabled elements

			elem->scr_height = 8;

			if (elem->flags & HUD_CVAR) {
				st = ((cvar_t*)elem->contents)->string;
				strlcpy (buf, st, sizeof(buf));
				st = buf;
				l = strlen_color(st);
			} else if (elem->flags & HUD_STRING) {
				Cmd_ExpandString(elem->contents, buf); //, sizeof(buf));
				st = TP_ParseMacroString(buf);
				st = TP_ParseFunChars(st, false);
				l = strlen_color(st);
			} else if (elem->flags & HUD_FUNC) {
				func = (Hud_Func)elem->contents;
				st =(*func)();
				l = strlen(st);
			/*} else if (elem->flags & HUD_IMAGE) {
				img = (mpic_t*)elem->contents;
				l = img->width/8;
				elem->scr_height = img->height;*/
		} else
				continue;

			if (elem->width && !(elem->flags & (HUD_FUNC|HUD_IMAGE))){
				if (elem->width < l) {
					l = elem->width;
					st[l] = '\0';
				} else {
					while (elem->width > l) {
						st[l++] = ' ';
					}
					st[l] = '\0';
				}
			}
			elem->scr_width = l*8;

			if (!Hud_TranslateCoords (elem, &x, &y))
				continue;

			if (elem->flags & (HUD_BLINK_B|HUD_BLINK_F))
				tblink = fmod(cls.realtime, elem->blink)/elem->blink;

			if (!(elem->flags & HUD_BLINK_B) || tblink < 0.5)
				if (elem->coords[3])
				{
					if (elem->alpha < 1)
						Draw_AlphaFill(x, y, elem->scr_width, elem->scr_height, (unsigned char)elem->coords[3], elem->alpha);
					else
						Draw_Fill(x, y, elem->scr_width, elem->scr_height, (unsigned char)elem->coords[3]);
				}
			if (!(elem->flags & HUD_BLINK_F) || tblink < 0.5)
			{
				if (!(elem->flags & HUD_IMAGE))
				{
#ifdef GLQUAKE
					int std_charset = char_textures[0];
					if (elem->charset)
						char_textures[0] = elem->charset;
					if (elem->alpha < 1)
						Draw_AlphaString (x, y, st, elem->alpha);
					else
#endif
						Draw_String (x, y, st);
#ifdef GLQUAKE
					char_textures[0] = std_charset;
#endif
				}
				else
#ifdef GLQUAKE
					if (elem->alpha < 1)
						Draw_AlphaPic (x, y, img, elem->alpha);
					else
#endif
						Draw_Pic (x, y, img);
			}
		}
	}
// Draw Input
/*	if (key_dest == key_message && chat_team == 100) {
		extern float	con_cursorspeed;
		extern int	chat_bufferpos;

		int	i,j;
		char	*s;
		char	t;

		x = input.x*8 + 1; // top left
		y = input.y*8;

		if (input.bg)
			Draw_Fill(x, y, input.len*8, 8, input.bg);

		s = chat_buffer[chat_edit];
		t = chat_buffer[chat_edit][chat_bufferpos];
		i = chat_bufferpos;

		if (chat_bufferpos > (input.len - 1)) {
			s += chat_bufferpos - (input.len -1);
			i = input.len - 1;
		}

		j = 0;
		while(s[j] && j<input.len)	{
			Draw_Character ( x+(j<<3), y, s[j]);
			j++;
		}
		Draw_Character ( x+(i<<3), y, 10+((int)(cls.realtime*con_cursorspeed)&1));
	}*/

}

qbool Hud_CheckBounds (hud_element_t *elem, int x, int y)
{
	unsigned int con_x, con_y;
	int hud_x, hud_y;

	con_x = VID_ConsoleX (x);
	con_y = VID_ConsoleY (y);

	if (!Hud_TranslateCoords (elem, &hud_x, &hud_y))
		return false;

	if (con_x < hud_x || con_x >= (hud_x + elem->scr_width))
		return false;
	if (con_y < hud_y || con_y >= (hud_y + elem->scr_height))
		return false;

	return true;
}

/*void Hud_MouseEvent (int x, int y, int buttons)
{
	int mouse_buttons = 5;
	static int old_x, old_y, old_buttons;
	hud_element_t	*elem;
	int i;

	for (i=0 ; i < mouse_buttons ; i++)
	{
		if ((buttons & (1<<i)) && !(old_buttons & (1<<i)))
			break;
	}
	if (i < mouse_buttons)
		++i;
	else
		i = 0;

	for (elem = hud_list; elem; elem = elem->next)
	{
		if (!(elem->flags & HUD_ENABLED) || !elem->f_hover || !elem->f_button
			|| !elem->scr_width || !elem->scr_height)
			continue;

		if (Hud_CheckBounds (elem, x, y))
		{
			if (elem->f_hover && !Hud_CheckBounds (elem, old_x, old_y))
				Cbuf_AddText (va("%s 1 %s\n", elem->f_hover, elem->name));
			if (i && elem->f_button)
				Cbuf_AddText (va("%s %d %s\n", elem->f_button, i, elem->name));
		}
		else if (elem->f_hover && Hud_CheckBounds (elem, old_x, old_y))
			Cbuf_AddText (va("%s 0 %s\n", elem->f_hover, elem->name));
	}
	old_x = x;
	old_y = y;
	old_buttons = buttons;
}*/
/********************************* TILE CLEAR *********************************/

#ifdef GLQUAKE

void SCR_TileClear (void) {
	int sb_lines_cleared = (scr_newHud.integer && scr_newHudClear.integer)
		? 0 : sb_lines; // newhud does not (typically) have solid status bar, so clear the bottom of the screen

	if (cls.state != ca_active && cl.intermission) {
		Draw_TileClear (0, 0, vid.width, vid.height);
		return;
	}

	if (r_refdef.vrect.x > 0) {
		// left
		Draw_TileClear (0, 0, r_refdef.vrect.x, vid.height - sb_lines_cleared);
		// right
		Draw_TileClear (r_refdef.vrect.x + r_refdef.vrect.width, 0,
			vid.width - (r_refdef.vrect.x + r_refdef.vrect.width), vid.height - sb_lines_cleared);
	}
	if (r_refdef.vrect.y > 0) {
		// top
		Draw_TileClear (r_refdef.vrect.x, 0, r_refdef.vrect.width, r_refdef.vrect.y);
	}
	if (r_refdef.vrect.y + r_refdef.vrect.height < vid.height - sb_lines_cleared) {
		// bottom
		Draw_TileClear (r_refdef.vrect.x,
			r_refdef.vrect.y + r_refdef.vrect.height,
			r_refdef.vrect.width,
			vid.height - sb_lines_cleared - (r_refdef.vrect.height + r_refdef.vrect.y));
	}
}

#else // GLQUAKE

void SCR_TileClear (void) {
	if (scr_fullupdate++ < vid.numpages) {	// clear the entire screen
		scr_copyeverything = 1;
		Draw_TileClear (0, 0, vid.width, vid.height);
		Sbar_Changed ();
	} else {
		char str[11] = "xxxxxxxxxx";
		if (scr_viewsize.value < 100) {
			if (scr_newHud.integer && scr_newHudClear.integer) {
				Draw_TileClear (0, 0, vid.width, vid.height);
			}
			else {
				// clear background for counters
				if (show_speed.value)
					Draw_TileClear(ELEMENT_X_COORD(show_speed), ELEMENT_Y_COORD(show_speed), 10 * 8, 8);
				if (show_fps.value)
					Draw_TileClear(ELEMENT_X_COORD(show_fps), ELEMENT_Y_COORD(show_fps), 10 * 8, 8);
				if (scr_clock.value)
					Draw_TileClear(ELEMENT_X_COORD(scr_clock), ELEMENT_Y_COORD(scr_clock), 10 * 8, 8);
				if (scr_gameclock.value)
					Draw_TileClear(ELEMENT_X_COORD(scr_gameclock), ELEMENT_Y_COORD(scr_gameclock), 10 * 8, 8);
				if (scr_democlock.value)
					Draw_TileClear(ELEMENT_X_COORD(scr_clock), ELEMENT_Y_COORD(scr_clock), 10 * 8, 8);
			}
		}
	}
}

#endif // GLQUAKE else

#ifdef GLQUAKE
//
// Calculates the cursor scale based on the current screen/text size
//
static double SCR_GetCursorScale(void) 
{
	return (double) scr_cursor_scale.value * ((double) vid.width / (double)vid.conwidth);
}
#endif // GLQUAKE

static void SCR_DrawCursor(void) 
{
	// from in_*.c
	extern float mouse_x, mouse_y;
	#ifdef GLQUAKE
    double scale = SCR_GetCursorScale();
	#endif 

	// Updating cursor location
	scr_pointer_state.x += mouse_x;
	scr_pointer_state.y += mouse_y;

	// Bound the cursor to displayed area
	clamp(scr_pointer_state.x, 0, vid.width);
	clamp(scr_pointer_state.y, 0, vid.height);

	// write the global variables which are used only by HUD Editor at the moment
	cursor_x = scr_pointer_state.x;
	cursor_y = scr_pointer_state.y;

	// Disable the cursor in all but following client parts.
	if (key_dest != key_hudeditor && key_dest != key_menu && key_dest != key_demo_controls)
	{
		return;
	}

	// Always draw the cursor.
	#ifdef GLQUAKE
	if (scr_cursor && scr_cursor->texnum)
	{
		Draw_SAlphaPic(cursor_x, cursor_y, scr_cursor, scr_cursor_alpha.value, scale);

		if (scr_cursor_icon && scr_cursor_icon->texnum)
		{
			Draw_SAlphaPic(cursor_x + scr_cursor_iconoffset_x.value, cursor_y + scr_cursor_iconoffset_y.value, scr_cursor_icon, scr_cursor_alpha.value, scale);
		}
	}
	else
	{
		color_t c = RGBA_TO_COLOR(0, 255, 0, 255);
		Draw_AlphaLineRGB(cursor_x + (10 * scale), cursor_y + (10 * scale), cursor_x + (40 * scale), cursor_y + (40 * scale), 10 * scale, c);
        Draw_AlphaLineRGB(cursor_x, cursor_y, cursor_x + (20 * scale), cursor_y, 10 * scale, c);
		Draw_AlphaLineRGB(cursor_x, cursor_y, cursor_x, cursor_y + 20*scale, 10 * scale, c);
        Draw_AlphaLineRGB(cursor_x + (20 * scale), cursor_y, cursor_x, cursor_y + (20 * scale), 10 * scale, c);
	}
	#else // GLQUAKE
	/*
	// FIXME: When the cursor is loaded in software, width and height are fine, but when reaching this point it's fucked up!!!
	if (scr_cursor && scr_cursor->width)
	{
		Draw_SPic(cursor_x, cursor_y, scr_cursor, scale);
	}
	else
	*/
	{
		Draw_Character(cursor_x - LETTERWIDTH/2, cursor_y - LETTERHEIGHT/2, '+');
	}
	#endif // GLQUAKE else

	if (scr_pointer_state.x != scr_pointer_state.x_old || scr_pointer_state.y != scr_pointer_state.y_old)
	{
        Mouse_MoveEvent();
    }

	// remember the position for future
	scr_pointer_state.x_old = scr_pointer_state.x;
	scr_pointer_state.y_old = scr_pointer_state.y;
}

void SCR_DrawElements(void) 
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
		if( !(!scr_menudrawhud.integer && (m_state != m_none)) || (!scr_menudrawhud.integer && (m_state == m_proxy)) )
		{
			if (cl.intermission == 1)
			{
				Sbar_IntermissionOverlay ();
				Con_ClearNotify ();
			} 
			else if (cl.intermission == 2)
			{
				Sbar_FinaleOverlay ();
				SCR_CheckDrawCenterString ();
				Con_ClearNotify ();
			}

			if (cls.state == ca_active)
			{
				SCR_DrawRam ();
				SCR_DrawNet ();
				SCR_DrawTurtle ();

				if (!sb_showscores && !sb_showteamscores) 
				{ 
					// Do not show if +showscores
					SCR_DrawPause ();
					
					#ifdef GLQUAKE
					SCR_DrawAutoID ();
					#endif
				}

				if (!cl.intermission) 
				{
					if ((key_dest != key_menu) && (scr_showcrosshair.integer || (!sb_showscores && !sb_showteamscores)))
					{
						Draw_Crosshair ();
					}

     				if (!sb_showscores && !sb_showteamscores)
					{ 
						// Do not show if +showscores
						SCR_Draw_TeamInfo();
						SCR_Draw_WeaponStats();

						#ifdef GLQUAKE
						SCR_Draw_ShowNick();
						#endif

						SCR_CheckDrawCenterString ();
						SCR_DrawSpeed ();
						SCR_DrawClock ();
						SCR_DrawGameClock ();
						SCR_DrawDemoClock ();
						SCR_DrawQTVBuffer ();
						SCR_DrawFPS ();
					}

					// QW262
					SCR_DrawHud ();

					MVD_Screen ();

					#ifdef GLQUAKE
					// VULT STATS
					SCR_DrawAMFstats();
					#endif
					
					// VULT DISPLAY KILLS
					if (amf_tracker_frags.value || amf_tracker_flags.value || amf_tracker_streaks.value )
						VX_TrackerThink();

					// Multiview
					if (cl_multiview.integer && cls.mvdplayback)
					{
						// Draw multiview mini-HUD's.
						if (cl_mvdisplayhud.integer)
						{
							if(cl_mvdisplayhud.integer >= 2)
							{
								// Graphical with icons.
								SCR_DrawMVStatus();
							}
							else
							{
								// Only strings.
								SCR_DrawMVStatusStrings();
							}
						}

						// Draw a black border between the views.
						SCR_DrawMultiviewBorders();
					}

					Sbar_Draw();
					HUD_Draw();
					HUD_Editor_Draw();
					DemoControls_Draw();
				}
			}
		}

		if (!scr_autosshot_countdown)
		{
			SCR_DrawConsole ();
			M_Draw ();
		}

        SCR_DrawCursor();
    }
}

/******************************* UPDATE SCREEN *******************************/

#ifdef GLQUAKE

// This is called every frame, and can also be called explicitly to flush text to the screen.
// WARNING: be very careful calling this from elsewhere, because the refresh needs almost the entire 256k of stack space!
void SCR_UpdateScreen (void) 
{
	static hud_t *hud_netstats = NULL;

	if (hud_netstats == NULL) // first time
		hud_netstats = HUD_Find("net");

	if (!scr_initialized)
		return;                         // not initialized yet

	if (scr_skipupdate || block_drawing)
		return;

	if (scr_disabled_for_loading) {
		if (cls.realtime - scr_disabled_time > 20)
			scr_disabled_for_loading = false;
		else
			return;
	}

	#ifdef _WIN32
	{	
		// Don't suck up any cpu if minimized.
		extern int Minimized;

		if (Minimized)
			return;
	}
	#endif // _WIN32

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

	if (vid.recalc_refdef)
		SCR_CalcRefdef ();

	if ((v_contrast.value > 1 && !vid_hwgamma_enabled) || gl_clear.value)
		Sbar_Changed ();

	// Multiview
	if (cl_multiview.value && cls.mvdplayback)
	{
		SCR_CalcRefdef();
	}

	// Do 3D refresh drawing, and then update the screen.
	GL_BeginRendering(&glx, &gly, &glwidth, &glheight);

	SCR_SetUpToDrawConsole ();

	SCR_SetupCI ();

	V_RenderView ();

	GL_Set2D ();

	R_PolyBlend ();

	// draw any areas not covered by the refresh
	SCR_TileClear ();

	if (r_netgraph.value && scr_newHud.value != 1)
	{
		// FIXME: ugly hack :(
		float temp = hud_netgraph->show->value;

		Cvar_SetValue(hud_netgraph->show, 1);
		SCR_HUD_Netgraph(hud_netgraph);
		Cvar_SetValue(hud_netgraph->show, temp);
	}

	if (r_netstats.value && scr_newHud.value != 1)
	{
		// FIXME: ugly hack :(
		float temp = hud_netstats->show->value;

		Cvar_SetValue(hud_netstats->show, 1);
		SCR_HUD_DrawNetStats(hud_netstats);
		Cvar_SetValue(hud_netstats->show, temp);
	}

	SCR_DrawElements();

	// For multiview:
	// If we apply the brightness on each update
	// then the first view that is drawn will be
	// 4x as bright as the last view for instance.
	// (using a multiview value of 4 in this example).
	if(cls.mvdplayback && cl_multiview.value)
	{
		if(CURRVIEW == 1)
		{
			R_BrightenScreen ();
		}
	}
	else
	{
		// Default, apply brightness at every update.
		R_BrightenScreen ();
	}

	V_UpdatePalette ();

	if (scr_autosshot_countdown)
		SCR_CheckAutoScreenshot();

	if (cls.mvdplayback && cl_multiview.value)
	{
		SCR_CheckMVScreenshot();
	}

	{extern double render_frame_end;
	render_frame_end = Sys_DoubleTime();}

	GL_EndRendering ();
}

#else

void SCR_UpdateScreen (void) 
{
	vrect_t vrect;

	if (!scr_initialized)
		return;                         // not initialized yet

	if (scr_skipupdate || block_drawing)
		return;

	if (scr_disabled_for_loading) {
		if (cls.realtime - scr_disabled_time > 20)
			scr_disabled_for_loading = false;
		else
			return;
	}

#ifdef _WIN32
	{	// don't suck up any cpu if minimized
		extern int Minimized;

		if (Minimized)
			return;
	}
#endif

	scr_copytop = 0;
	scr_copyeverything = 0;

	host_screenupdatecount ++;  // kazik - HUD -> hexum

	// check for vid changes
	if (oldfov != scr_fov.value) {
		oldfov = scr_fov.value;
		vid.recalc_refdef = true;
	}

	if (oldscreensize != scr_viewsize.value) {
		oldscreensize = scr_viewsize.value;
		vid.recalc_refdef = true;
	}

	if (oldsbar != cl_sbar.value) {
		oldsbar = cl_sbar.value;
		vid.recalc_refdef = true;
	}

	if (vid.recalc_refdef) {
		// something changed, so reorder the screen
		SCR_CalcRefdef ();
	}


	// do 3D refresh drawing, and then update the screen
	D_EnableBackBufferAccess ();	// of all overlay stuff if drawing directly

	 // Multiview - dont tile over the first view
	if (!(cl_multiview.value && cls.mvdplayback) ||
		(cl_multiview.value && cls.mvdplayback && CURRVIEW == 2))
	{
		SCR_TileClear ();
	}

	SCR_SetUpToDrawConsole ();

	// Multiview
	if (!(cl_multiview.value && cls.mvdplayback))
	{
		SCR_EraseCenterString ();
	}

	// Multiview - Only calculate Refdef once for all the views.
	if (cl_multiview.value && cls.mvdplayback && CURRVIEW == 1)
	{
		SCR_CalcRefdef();
	}

	D_DisableBackBufferAccess ();	// for adapters that can't stay mapped in for linear writes all the time

	VID_LockBuffer ();

	V_RenderView ();
	VID_UnlockBuffer ();

	D_EnableBackBufferAccess ();	// of all overlay stuff if drawing directly

	SCR_DrawElements();

	D_DisableBackBufferAccess ();	// for adapters that can't stay mapped in for linear writes all the time
	V_UpdatePalette ();

	// update one of three areas
	if (scr_copyeverything) {
		vrect.x = 0;
		vrect.y = 0;
		vrect.width = vid.width;
		vrect.height = vid.height;
		vrect.pnext = 0;

		VID_Update (&vrect);
	} else if (scr_copytop) {
		vrect.x = 0;
		vrect.y = 0;
		vrect.width = vid.width;
		vrect.height = vid.height - sb_lines;
		vrect.pnext = 0;

		VID_Update (&vrect);
	} else {
		vrect.x = scr_vrect.x;
		vrect.y = scr_vrect.y;
		vrect.width = scr_vrect.width;
		vrect.height = scr_vrect.height;
		vrect.pnext = 0;

		VID_Update (&vrect);
	}

	if (scr_autosshot_countdown)
		SCR_CheckAutoScreenshot();

	// Multiview
	if (cls.mvdplayback && cl_multiview.value)
		SCR_CheckMVScreenshot();
}

#endif

void SCR_UpdateWholeScreen (void) {
	scr_fullupdate = 0;
	SCR_UpdateScreen ();
}

/******************************** SCREENSHOTS ********************************/

#define SSHOT_FAILED		-1
#define SSHOT_FAILED_QUIET	-2		//failed but don't print an error message
#define SSHOT_SUCCESS		0

typedef enum image_format_s {IMAGE_PCX, IMAGE_TGA, IMAGE_JPEG, IMAGE_PNG} image_format_t;

static char *SShot_ExtForFormat(int format) {
	switch (format) {
		case IMAGE_PCX:		return ".pcx";
		case IMAGE_TGA:		return ".tga";
		case IMAGE_JPEG:	return ".jpg";
		case IMAGE_PNG:		return ".png";
	}
	assert(!"SShot_ExtForFormat: unknown format");
	return "err";
}

static image_format_t SShot_FormatForName(char *name) {
	char *ext;

	ext = COM_FileExtension(name);

#ifdef GLQUAKE
	if (!strcasecmp(ext, "tga"))
		return IMAGE_TGA;
#else
	if (!strcasecmp(ext, "pcx"))
		return IMAGE_PCX;
#endif

#ifdef WITH_PNG
	else if (!strcasecmp(ext, "png"))
		return IMAGE_PNG;
#endif

#ifdef WITH_JPEG
	else if (!strcasecmp(ext, "jpg"))
		return IMAGE_JPEG;
#endif

#ifdef WITH_PNG
	else if (!strcasecmp(scr_sshot_format.string, "png"))
		return IMAGE_PNG;
#endif

#ifdef WITH_JPEG
	else if (!strcasecmp(scr_sshot_format.string, "jpg") || !strcasecmp(scr_sshot_format.string, "jpeg"))
		return IMAGE_JPEG;
#endif

	else
#ifdef GLQUAKE
		return IMAGE_TGA;
#else
		return IMAGE_PCX;
#endif
}

static char *Sshot_SshotDirectory(void) {
	static char dir[MAX_PATH];

	strlcpy(dir, FS_LegacyDir(scr_sshot_dir.string), sizeof(dir));
	return dir;
}


#ifdef GLQUAKE

extern unsigned short ramps[3][256];
//applies hwgamma to RGB data
static void applyHWGamma(byte *buffer, int size) {
	int i;

	if (vid_hwgamma_enabled) {
		for (i = 0; i < size; i += 3) {
			buffer[i + 0] = ramps[0][buffer[i + 0]] >> 8;
			buffer[i + 1] = ramps[1][buffer[i + 1]] >> 8;
			buffer[i + 2] = ramps[2][buffer[i + 2]] >> 8;
		}
	}
}

int SCR_Screenshot(char *name) {
	int i, temp, buffersize;
	int success = SSHOT_FAILED;
	byte *buffer;
	image_format_t format;

// name is fullpath now
//	name = (*name == '/') ? name + 1 : name;

	format = SShot_FormatForName(name);
	COM_ForceExtension (name, SShot_ExtForFormat(format));
	buffersize = glwidth * glheight * 3;

	buffer = (byte *) Q_malloc (buffersize);
	glReadPixels (glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer);

#ifdef WITH_PNG
	if (format == IMAGE_PNG) {
#ifndef WITH_PNG_STATIC
		if (QLib_isModuleLoaded(qlib_libpng)) {
#endif
			applyHWGamma(buffer, buffersize);
			success = Image_WritePNG(name, image_png_compression_level.value,
					buffer + buffersize - 3 * glwidth, -glwidth, glheight)
				? SSHOT_SUCCESS : SSHOT_FAILED;
#ifndef WITH_PNG_STATIC
		} else {
			Com_Printf("Can't take a PNG screenshot without libpng.");
			if (SShot_FormatForName("noext") == IMAGE_PNG)
				Com_Printf(" Try changing \"%s\" to another image format.", scr_sshot_format.name);
			Com_Printf("\n");
			success = SSHOT_FAILED_QUIET;
		}
#endif
	}
#endif

#ifdef WITH_JPEG
	if (format == IMAGE_JPEG) {
#ifndef WITH_JPEG_STATIC
		if (QLib_isModuleLoaded(qlib_libjpeg)) {
#endif
			applyHWGamma(buffer, buffersize);
			success = Image_WriteJPEG(name, image_jpeg_quality_level.value,
					buffer + buffersize - 3 * glwidth, -glwidth, glheight)
				? SSHOT_SUCCESS : SSHOT_FAILED;;
#ifndef WITH_JPEG_STATIC
		} else {
			Com_Printf("Can't take a JPEG screenshot without libjpeg.");
			if (SShot_FormatForName("noext") == IMAGE_JPEG)
				Com_Printf(" Try changing \"%s\" to another image format.", scr_sshot_format.name);
			Com_Printf("\n");
			success = SSHOT_FAILED_QUIET;
		}
#endif
	}
#endif

	if (format == IMAGE_TGA) {
		// swap rgb to bgr
		for (i = 0; i < buffersize; i += 3)	{
			temp = buffer[i];
			buffer[i] = buffer[i + 2];
			buffer[i + 2] = temp;
		}
		applyHWGamma(buffer, buffersize);
		success = Image_WriteTGA(name, buffer, glwidth, glheight)
					? SSHOT_SUCCESS : SSHOT_FAILED;
	}

	Q_free(buffer);
	return success;
}

#else

int SCR_Screenshot(char *name) {
	int success = -1;
	image_format_t format;

// name is fullpath now
//	name = (*name == '/') ? name + 1 : name;

	format = SShot_FormatForName(name);
	COM_ForceExtension (name, SShot_ExtForFormat(format));

	D_EnableBackBufferAccess ();	// enable direct drawing of console to back buffer

#ifdef WITH_PNG
	if (format == IMAGE_PNG) {
#ifndef WITH_PNG_STATIC
		if (QLib_isModuleLoaded(qlib_libpng)) {
#endif
			success = Image_WritePNGPLTE(name, image_png_compression_level.value,
					vid.buffer, vid.width, vid.height, vid.rowbytes, current_pal)
					? SSHOT_SUCCESS : SSHOT_FAILED;
#ifndef WITH_PNG_STATIC
		} else {
			Com_Printf("Can't take a PNG screenshot without libpng.");
			if (SShot_FormatForName("noext") == IMAGE_PNG)
				Com_Printf(" Try changing \"%s\" to another image format.", scr_sshot_format.name);
			Com_Printf("\n");
			success = SSHOT_FAILED_QUIET;
		}
#endif
	}
#endif

	if (format == IMAGE_PCX) {
		success = Image_WritePCX (name, vid.buffer, vid.width, vid.height, vid.rowbytes, current_pal)
					? SSHOT_SUCCESS : SSHOT_FAILED;
	}

	D_DisableBackBufferAccess ();	// for adapters that can't stay mapped in for linear writes all the time

	return success;
}

#endif

#define MAX_SCREENSHOT_COUNT	1000

int SCR_GetScreenShotName (char *name, int name_size, char *sshot_dir)
{
	int i = 0;
	char ext[4], basename[64];
	FILE *f;

	ext[0] = 0;

	// Find a file name to save it to
	#ifdef WITH_PNG
	if (!strcasecmp(scr_sshot_format.string, "png"))
	{
		strlcpy(ext, "png", 4);
	}
	#endif

	#ifdef WITH_JPEG
	if (!strcasecmp(scr_sshot_format.string, "jpeg") || !strcasecmp(scr_sshot_format.string, "jpg"))
	{
		strlcpy(ext, "jpg", 4);
	}
	#endif

	if (!strcasecmp(scr_sshot_format.string, "tga"))
	{
		strlcpy(ext, "tga", 4);
	}

	if (!strcasecmp(scr_sshot_format.string, "pcx"))
	{
		strlcpy(ext, "pcx", 4);
	}

	if (!ext[0])
	{
		strlcpy(ext, DEFAULT_SSHOT_FORMAT, sizeof(ext));
	}

	if(fabsf(scr_sshot_autoname.value - 1.0f) < 0.0001f) {
		// if sshot_autoname is 1, prefix with map name.
		snprintf(basename, sizeof(basename), "%s_", host_mapname.string);
	}
	else {
		// otherwise prefix with ezquake.
		strcpy(basename, "ezquake");
	}

	for (i = 0; i < MAX_SCREENSHOT_COUNT; i++)
	{
		snprintf(name, name_size, "%s%03i.%s", basename, i, ext);
		if (!(f = fopen (va("%s/%s", sshot_dir, name), "rb")))
		{
			break;  // file doesn't exist
		}
		fclose(f);
	}

	if (i == MAX_SCREENSHOT_COUNT)
	{
		Com_Printf ("Error: Cannot create more than %d screenshots\n", MAX_SCREENSHOT_COUNT);
		return -1;
	}

	return 1;
}

void SCR_ScreenShot_f (void)
{
	char name[MAX_OSPATH], *filename, *sshot_dir;
	int success;

	// Multiview
	if (cls.mvdplayback && cl_multiview.value)
	{
		if (CURRVIEW == 1)
		{
			scr_mvsshot_in_progress = false;

			#ifdef GLQUAKE
			// Make sure all gl calls have been drawn.
			glFinish();
			#endif
		}
		else
		{
			scr_mvsshot_in_progress = true;
			Com_DPrintf ("Waiting with screenshot because all views haven't been drawn... view = %d\n", CURRVIEW);
			return;
		}
	}

	sshot_dir = Sshot_SshotDirectory();

	if (Cmd_Argc() == 2)
	{
		strlcpy (name, Cmd_Argv(1), sizeof(name));
	}
	else if (Cmd_Argc() == 1)
	{
		if (SCR_GetScreenShotName (name, sizeof(name), sshot_dir) < 0)
		{
			return;
		}
	}
	else
	{
		Com_Printf("Usage: %s [filename]\n", Cmd_Argv(0));
		return;
	}

	for (filename = name; *filename == '/' || *filename == '\\'; filename++)
		;

	success = SCR_Screenshot(va("%s/%s", sshot_dir, filename));

	if (success != SSHOT_FAILED_QUIET)
	{
		Com_Printf ("%s %s\n", success == SSHOT_SUCCESS ? "Wrote" : "Couldn't write", name);
	}
}

void SCR_RSShot_f (void) {
	int success = SSHOT_FAILED;
	char filename[MAX_PATH];
#ifdef GLQUAKE
	int width, height;
	byte *base, *pixels;
#endif

	if (CL_IsUploading())
		return;		// already one pending

	if (cls.state < ca_onserver)
		return;		// gotta be connected

	if (!scr_allowsnap.value) {
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, "snap\n");
		Com_Printf ("Refusing remote screen shot request.\n");
		return;
	}

	Com_Printf ("Remote screenshot requested.\n");

	snprintf(filename, sizeof(filename), "%s/temp/__rsshot__", Sshot_SshotDirectory());

#ifdef GLQUAKE

	width = 400; height = 300;
	base = (byte *) Q_malloc ((width * height + glwidth * glheight) * 3);
	pixels = base + glwidth * glheight * 3;

	glReadPixels (glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, base);
	Image_Resample (base, glwidth, glheight, pixels, width, height, 3, 0);
#ifdef WITH_JPEG
#ifndef WITH_JPEG_STATIC
	if (QLib_isModuleLoaded(qlib_libjpeg)) {
#endif
		success = Image_WriteJPEG (filename, 70, pixels + 3 * width * (height - 1), -width, height)
			? SSHOT_SUCCESS : SSHOT_FAILED;
#ifndef WITH_JPEG_STATIC
	} else
#endif
#endif
	success = Image_WriteTGA (filename, pixels, width, height)
		? SSHOT_SUCCESS : SSHOT_FAILED;

	Q_free(base);

#else		//GLQUAKE

	D_EnableBackBufferAccess ();

#ifdef WITH_PNG
#ifndef WITH_PNG_STATIC
	if (QLib_isModuleLoaded(qlib_libpng)) {
#endif
		success = Image_WritePNGPLTE(filename, 9, vid.buffer, vid.width, vid.height, vid.rowbytes, current_pal)
			? SSHOT_SUCCESS : SSHOT_FAILED;
#ifndef WITH_PNG_STATIC
	} else
#endif
#endif
	success = Image_WritePCX (filename, vid.buffer, vid.width, vid.height, vid.rowbytes, current_pal)
		? SSHOT_SUCCESS : SSHOT_FAILED;

	D_DisableBackBufferAccess();

#endif		//GLQUAKE

	if (success == SSHOT_SUCCESS)
	{
		FILE	*f;
		byte	*screen_shot;
		int	size;
		if ((size = FS_FileOpenRead (filename, &f)) == -1)
		{
			Com_Printf ("Can't send screenshot to server: can't open file %s\n", filename);
		}
		else
		{
			screen_shot = (byte *) Q_malloc (size);
			if (fread(screen_shot, 1, (size_t) size, f) == (size_t) size)
			{
				Com_Printf ("Sending screenshot to server...\n");
				CL_StartUpload(screen_shot, size);
			}
			else
				Com_Printf ("Can't send screenshot to server: can't read file %s\n", filename);
			fclose(f);
			Q_free(screen_shot);
		}
	}

	remove(filename);
}

static void SCR_CheckMVScreenshot(void)
{
	if(scr_mvsshot_in_progress && cls.mvdplayback && cl_multiview.value)
	{
		Cbuf_AddText ("screenshot\n");
	}
}

static void SCR_CheckAutoScreenshot(void) {
	char *filename, savedname[MAX_PATH], *sshot_dir, *fullsavedname, *ext;
	char *exts[5] = {"pcx", "tga", "png", "jpg", NULL};
	int num;

	if (!scr_autosshot_countdown || --scr_autosshot_countdown)
		return;

	if (!cl.intermission)
		return;

	sshot_dir = Sshot_SshotDirectory();

// no, sorry
//	while (*sshot_dir && (*sshot_dir == '/'))
//		sshot_dir++; // will skip all '/' chars at the beginning

	if (!sshot_dir[0])
		sshot_dir = cls.gamedir;

	for (filename = auto_matchname; *filename == '/' || *filename == '\\'; filename++)
		;

	ext = SShot_ExtForFormat(SShot_FormatForName(filename));

	fullsavedname = va("%s/%s", sshot_dir, filename);
	if ((num = Util_Extend_Filename(fullsavedname, exts)) == -1) {
		Com_Printf("Error: no available filenames\n");
		return;
	}

	snprintf (savedname, sizeof(savedname), "%s_%03i%s", filename, num, ext);
	fullsavedname = va("%s/%s", sshot_dir, savedname);

#ifdef GLQUAKE
	glFinish();
#endif

	if ((SCR_Screenshot(fullsavedname)) == SSHOT_SUCCESS)
		Com_Printf("Match scoreboard saved to %s\n", savedname);
}

void SCR_AutoScreenshot(char *matchname)
{
	if (cl.intermission == 1)
	{
		scr_autosshot_countdown = vid.numpages;
		strlcpy(auto_matchname, matchname, sizeof(auto_matchname));
	}
}

// Capturing to avi.
void SCR_Movieshot(char *name)
{
	#ifdef _WIN32
	if (movie_is_avi)
	{
		int size = 0;
		// Capturing a movie.
		#ifdef GLQUAKE
		int i;
		byte *buffer, temp;

		// Set buffer size to fit RGB data for the image.
		size = glwidth * glheight * 3;

		// Allocate the RGB buffer, get the pixels from GL and apply the gamma.
		buffer = (byte *) Q_malloc (size);
		glReadPixels (glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer);
		applyHWGamma (buffer, size);

		// We now have a byte buffer with RGB values, but
		// before we write it to the file, we need to swap
		// them to GBR instead, which windows DIBs uses.
		// (There's a GL Extension that allows you to use
		// BGR_EXT instead of GL_RGB in the glReadPixels call
		// instead, but there is no real speed gain using it).
		for (i = 0; i < size; i += 3)
		{
			// Swap RGB => GBR
			temp = buffer[i];
			buffer[i] = buffer[i+2];
			buffer[i+2] = temp;
		}

		#else // GLQUAKE

		int i, j, rowp;
		byte *buffer, *p;

		size = vid.width * vid.height * 3;
		buffer = (byte *) Q_malloc (size);

		D_EnableBackBufferAccess ();

		p = buffer;
		for (i = vid.height - 1; i >= 0; i--)
		{
			rowp = i * vid.rowbytes;
			for (j = 0; j < vid.width; j++)
			{
				*p++ = current_pal[vid.buffer[rowp]*3+2];
				*p++ = current_pal[vid.buffer[rowp]*3+1];
				*p++ = current_pal[vid.buffer[rowp]*3+0];
				rowp++;
			}
		}

		D_DisableBackBufferAccess ();
		#endif // GLQUAKE

		// Write the buffer to video.
		Capture_WriteVideo (buffer, size);

		Q_free (buffer);
	}
	else
	{
		// We're just capturing images.
		SCR_Screenshot (name);
	}

	#else // _WIN32

	// Capturing to avi only supported in windows yet.
	SCR_Screenshot (name);

	#endif // _WIN32
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

/************************************ INIT ************************************/

void SCR_Init (void)
{
	scr_ram = Draw_CacheWadPic ("ram");
	scr_net = Draw_CacheWadPic ("net");
	scr_turtle = Draw_CacheWadPic ("turtle");
	scr_cursor = SCR_LoadCursorImage("gfx/cursor");

	Cvar_SetCurrentGroup(CVAR_GROUP_VIEW);
	Cvar_Register (&scr_fov);
	Cvar_Register (&default_fov);
	Cvar_Register (&scr_viewsize);

	Cvar_SetCurrentGroup(CVAR_GROUP_SBAR);
	Cvar_Register (&scr_newHud);
	Cvar_Register (&scr_newHudClear);
	Cvar_ResetCurrentGroup();

	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register (&scr_consize);
	Cvar_Register (&scr_conspeed);
	Cvar_Register (&scr_printspeed);

#ifdef GLQUAKE
	Cvar_SetCurrentGroup(CVAR_GROUP_OPENGL);
	Cvar_Register (&gl_triplebuffer);

	Cvar_SetCurrentGroup(CVAR_GROUP_EYECANDY);
	Cvar_Register (&r_chaticons_alpha);
#endif

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register (&scr_showram);
	Cvar_Register (&scr_showturtle);
	Cvar_Register (&scr_showpause);
	Cvar_Register (&scr_centertime);
	Cvar_Register (&scr_centershift);

	Cvar_Register (&scr_clock_x);
	Cvar_Register (&scr_clock_y);
	Cvar_Register (&scr_clock_format);
	Cvar_Register (&scr_clock);

	Cvar_Register (&scr_gameclock_offset);
	Cvar_Register (&scr_gameclock_x);
	Cvar_Register (&scr_gameclock_y);
	Cvar_Register (&scr_gameclock);

	Cvar_Register (&scr_democlock_x);
	Cvar_Register (&scr_democlock_y);
	Cvar_Register (&scr_democlock);

	Cvar_Register (&scr_qtvbuffer_x);
	Cvar_Register (&scr_qtvbuffer_y);
	Cvar_Register (&scr_qtvbuffer);

	Cvar_Register (&show_speed);
	Cvar_Register (&show_speed_x);
	Cvar_Register (&show_speed_y);

	Cvar_Register (&show_fps);
	Cvar_Register (&show_fps_x);
	Cvar_Register (&show_fps_y);

#ifdef GLQUAKE
	Cvar_Register (&scr_autoid);
	Cvar_Register (&scr_autoid_drawname);
	Cvar_Register (&scr_coloredfrags);
#endif
	Cvar_Register (&scr_teaminfo_order);
	Cvar_Register (&scr_teaminfo_align_right);
	Cvar_Register (&scr_teaminfo_frame_color);
	Cvar_Register (&scr_teaminfo_scale);
	Cvar_Register (&scr_teaminfo_y);
	Cvar_Register (&scr_teaminfo_x);
	Cvar_Register (&scr_teaminfo_loc_width);
	Cvar_Register (&scr_teaminfo_name_width);
	Cvar_Register (&scr_teaminfo_low_health);
	Cvar_Register (&scr_teaminfo_armor_style);
	Cvar_Register (&scr_teaminfo_weapon_style);
	Cvar_Register (&scr_teaminfo_show_enemies);
	Cvar_Register (&scr_teaminfo_show_self);
	Cvar_Register (&scr_teaminfo);
	Cvar_Register (&scr_shownick_order);
#ifdef GLQUAKE
	Cvar_Register (&scr_shownick_frame_color);
	Cvar_Register (&scr_shownick_scale);
	Cvar_Register (&scr_shownick_y);
	Cvar_Register (&scr_shownick_x);
	Cvar_Register (&scr_shownick_name_width);
	Cvar_Register (&scr_shownick_time);
#endif
	Cvar_Register (&scr_weaponstats_order);
	Cvar_Register (&scr_weaponstats_align_right);
	Cvar_Register (&scr_weaponstats_frame_color);
	Cvar_Register (&scr_weaponstats_scale);
	Cvar_Register (&scr_weaponstats_y);
	Cvar_Register (&scr_weaponstats_x);
	Cvar_Register (&scr_weaponstats);

	Cmd_AddCommand ("+cl_wp_stats", SCR_MvdWeaponStatsOn_f);
	Cmd_AddCommand ("-cl_wp_stats", SCR_MvdWeaponStatsOff_f);
	
	Cvar_Register (&scr_coloredText);

	// QW 262 HUD
	Cvar_Register (&cl_hud);

	Cvar_Register (&scr_tracking);
	Cvar_Register (&scr_spectatorMessage);

	Cvar_Register (&scr_cursor_scale);
	Cvar_Register (&scr_cursor_iconoffset_x);
	Cvar_Register (&scr_cursor_iconoffset_y);
#ifdef GLQUAKE
	Cvar_Register (&scr_cursor_alpha);
#endif

	Cvar_Register (&scr_showcrosshair);

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREENSHOTS);
	Cvar_Register (&scr_allowsnap);
	Cvar_Register (&scr_sshot_autoname);
	Cvar_Register (&scr_sshot_format);
	Cvar_Register (&scr_sshot_dir);

	Cvar_ResetCurrentGroup();


	Cmd_AddCommand ("screenshot", SCR_ScreenShot_f);
	Cmd_AddCommand ("sizeup", SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown", SCR_SizeDown_f);
	Cmd_AddCommand ("+zoom", SCR_ZoomIn_f);
	Cmd_AddCommand ("-zoom", SCR_ZoomOut_f);

	scr_initialized = true;

    ScrollBars_Init();
}

void SCR_DrawMultiviewBorders()
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

mpic_t *SCR_GetWeaponIconByWeaponNumber (int num)
{
	extern mpic_t *sb_weapons[7][8];  // sbar.c Weapon pictures.

	num -= 2;

	if (num >= 0 && num < 8)
	{
		return sb_weapons[0][num];
	}

	return NULL;
}

mpic_t *SCR_GetActiveWeaponIcon()
{
	return SCR_GetWeaponIconByFlag (cl.stats[STAT_ACTIVEWEAPON]);
}

typedef struct mv_viewrect_s
{
	int x;
	int y;
	int width;
	int height;
} mv_viewrect_t;

char *Macro_BestAmmo (void); // Teamplay.c
void Draw_AlphaRectangle (int x, int y, int w, int h, int c, float thickness, qbool fill, float alpha); // gl_draw.c

#define MV_HUD_POS_BOTTOM_CENTER	1
#define MV_HUD_POS_BOTTOM_LEFT		2
#define MV_HUD_POS_BOTTOM_RIGHT		3
#define MV_HUD_POS_TOP_CENTER		4
#define MV_HUD_POS_TOP_LEFT			5
#define MV_HUD_POS_TOP_RIGHT		6
#define MV_HUD_POS_GATHERED_CENTER	7

#define MV_HUDPOS(regexp, input, position, cvar) if(Utils_RegExpMatch(regexp, input)) { mv_hudpos = position; found = true; }

int mv_hudpos = MV_HUD_POS_BOTTOM_CENTER;

void SCR_OnChangeMVHudPos(cvar_t *var, char *newval, qbool *cancel)
{
	qbool found = false;

	MV_HUDPOS ("(top\\s*left|tl)",		newval, MV_HUD_POS_TOP_LEFT, var);
	MV_HUDPOS ("(top\\s*right|tr)",		newval, MV_HUD_POS_TOP_RIGHT, var);
	MV_HUDPOS ("(top\\s*center|tc)",	newval, MV_HUD_POS_TOP_CENTER, var);
	MV_HUDPOS ("(bottom\\s*left|bl)",	newval, MV_HUD_POS_BOTTOM_LEFT, var);
	MV_HUDPOS ("(bottom\\s*right|br)",	newval, MV_HUD_POS_BOTTOM_RIGHT, var);
	MV_HUDPOS ("(bottom\\s*center|bc)",	newval, MV_HUD_POS_BOTTOM_CENTER, var);
	MV_HUDPOS ("(gather|g)",			newval, MV_HUD_POS_GATHERED_CENTER, var);

	if (found)
	{
		Cvar_Set (var, newval);
	}

	*cancel = found;
}

// SCR_SetMVStatusPosition calls SCR_SetMVStatusGatheredPosition and vice versa.
void SCR_SetMVStatusGatheredPosition (mv_viewrect_t *view, int hud_width, int hud_height, int *x, int *y);

void SCR_SetMVStatusPosition (int position, mv_viewrect_t *view, int hud_width, int hud_height, int *x, int *y)
{
	switch (position)
	{
		case MV_HUD_POS_BOTTOM_LEFT :
		{
			(*x) = 0;
			(*y) = max(0, (view->height - hud_height));
			break;
		}
		case MV_HUD_POS_BOTTOM_RIGHT :
		{
			(*x) = max(0, (view->width - hud_width));
			(*y) = max(0, (view->height - hud_height));
			break;
		}
		case MV_HUD_POS_TOP_CENTER :
		{
			(*x) = max(0, (view->width - hud_width) / 2);
			(*y) = 0;
			break;
		}
		case MV_HUD_POS_TOP_LEFT :
		{
			(*x) = 0;
			(*y) = 0;
			break;
		}
		case MV_HUD_POS_TOP_RIGHT :
		{
			(*x) = max(0, view->width - hud_width);
			(*y) = 0;
			break;
		}
		case MV_HUD_POS_GATHERED_CENTER :
		{
			SCR_SetMVStatusGatheredPosition (view, hud_width, hud_height, &(*x), &(*y));
			break;
		}
		case MV_HUD_POS_BOTTOM_CENTER :
		default:
		{
			(*x) = max(0, (view->width - hud_width) / 2);
			(*y) = max(0, (view->height - hud_height));
			break;
		}
	}
}

void SCR_SetMVStatusGatheredPosition (mv_viewrect_t *view, int hud_width, int hud_height, int *x, int *y)
{
	//
	// Position the huds towards the center of the screen for the different views
	// so that all the HUD's are close to each other on the screen.
	//

	if (cl_multiview.value == 2)
	{
		if (!cl_mvinset.value)
		{
			if (CURRVIEW == 2)
			{
				// Top view. (Put the hud at the bottom)
				SCR_SetMVStatusPosition (MV_HUD_POS_BOTTOM_CENTER, view, hud_width, hud_height, x, y);
			}
			else if (CURRVIEW == 1)
			{
				// Bottom view. (Put the hud at the top)
				SCR_SetMVStatusPosition (MV_HUD_POS_TOP_CENTER, view, hud_width, hud_height, x, y);
			}
		}
	}
	else if (cl_multiview.value == 3)
	{
		if (CURRVIEW == 2)
		{
			// Top view. (Put the hud at the bottom)
			SCR_SetMVStatusPosition (MV_HUD_POS_BOTTOM_CENTER, view, hud_width, hud_height, x, y);
		}
		else if (CURRVIEW == 3)
		{
			// Bottom left view. (Put the hud at the top right)
			SCR_SetMVStatusPosition (MV_HUD_POS_TOP_RIGHT, view, hud_width, hud_height, x, y);
		}
		else if (CURRVIEW == 1)
		{
			// Bottom right view. (Put the hud at the top left)
			SCR_SetMVStatusPosition (MV_HUD_POS_TOP_LEFT, view, hud_width, hud_height, x, y);
		}
	}
	else if (cl_multiview.value == 4)
	{
		if (CURRVIEW == 2)
		{
			// Top left view. (Put the hud at the bottom right)
			SCR_SetMVStatusPosition (MV_HUD_POS_BOTTOM_RIGHT, view, hud_width, hud_height, x, y);
		}
		else if (CURRVIEW == 3)
		{
			// Top right view. (Put the hud at the bottom left)
			SCR_SetMVStatusPosition (MV_HUD_POS_BOTTOM_LEFT, view, hud_width, hud_height, x, y);
		}
		else if (CURRVIEW == 4)
		{
			// Bottom left view. (Put the hud at the top right)
			SCR_SetMVStatusPosition (MV_HUD_POS_TOP_RIGHT, view, hud_width, hud_height, x, y);
		}
		else if (CURRVIEW == 1)
		{
			// Bottom right view. (Put the hud at the top left)
			SCR_SetMVStatusPosition (MV_HUD_POS_TOP_LEFT, view, hud_width, hud_height, x, y);
		}
	}
}

#define MV_HUD_ARMOR_WIDTH		(5*8)
#define MV_HUD_HEALTH_WIDTH		(5*8)
#define MV_HUD_CURRAMMO_WIDTH	(5*8)
#define MV_HUD_CURRWEAP_WIDTH	(3*8)
#define MV_HUD_POWERUPS_WIDTH	(2*8)
#define MV_HUD_POWERUPS_HEIGHT	(2*8)

#define MV_HUD_STYLE_ONLY_NAME	2
#define MV_HUD_STYLE_ALL		3
#define MV_HUD_STYLE_ALL_TEXT	4
#define MV_HUD_STYLE_ALL_FILL	5

void SCR_MV_SetBoundValue (int *var, int val)
{
	if (var != NULL)
	{
		(*var) = val;
	}
}

void SCR_MV_DrawName (int x, int y, int *width, int *height)
{
	char *name = cl.players[nPlayernum].name;
	SCR_MV_SetBoundValue (width, 8*(strlen(name) + 1));
	SCR_MV_SetBoundValue (height, 8);

	Draw_String(x, y, name);
}

void SCR_MV_DrawArmor (int x, int y, int *width, int *height, int style)
{
	char armor_color_code[6] = "";
	int armor_amount_width = 0;

	byte armor_color[4] = {0, 0, 0, 75};	// RGBA.

	//
	// Set the armor text color based on what armor the player has.
	//
	if (cl.stats[STAT_ITEMS] & IT_ARMOR1)
	{
		// Green armor.
		strlcpy(armor_color_code, "&c0f0", sizeof(armor_color_code));
		armor_amount_width = Q_rint(MV_HUD_ARMOR_WIDTH * cl.stats[STAT_ARMOR] / 100.0);

		armor_color[0] = 80;
		armor_color[1] = 190;
		armor_color[2] = 80;
	}
	else if (cl.stats[STAT_ITEMS] & IT_ARMOR2)
	{
		// Yellow armor.
		strlcpy(armor_color_code, "&cff0", sizeof(armor_color_code));
		armor_amount_width = Q_rint(MV_HUD_ARMOR_WIDTH * cl.stats[STAT_ARMOR] / 150.0);

		armor_color[0] = 250;
		armor_color[1] = 230;
		armor_color[2] = 0;
	}
	else if (cl.stats[STAT_ITEMS] & IT_ARMOR3)
	{
		// Red armor.
		strlcpy(armor_color_code, "&cf00", sizeof(armor_color_code));
		armor_amount_width = Q_rint(MV_HUD_ARMOR_WIDTH * cl.stats[STAT_ARMOR] / 200.0);

		armor_color[0] = 190;
		armor_color[1] = 50;
		armor_color[2] = 0;
	}

	if (cl.stats[STAT_ARMOR] > 0)
	{
		//
		// Background fill for armor.
		//
		Draw_AlphaFillRGB(x, y, armor_amount_width, 8, RGBA_TO_COLOR(armor_color[0], armor_color[1], armor_color[2], armor_color[3]));

		// Armor value.
		if (style >= MV_HUD_STYLE_ALL_TEXT)
		{
			Draw_ColoredString(x, y, va("%s%4d", armor_color_code, cl.stats[STAT_ARMOR]), 0);
		}
	}

	SCR_MV_SetBoundValue (width, MV_HUD_ARMOR_WIDTH);
	SCR_MV_SetBoundValue (height, 8);
}

void SCR_MV_DrawHealth (int x, int y, int *width, int *height, int style)
{
	#define MV_HEALTH_OPACITY 75
	int health = cl.stats[STAT_HEALTH];

	int health_amount_width = 0;

	health = min(100, health);
	health_amount_width = Q_rint(abs((MV_HUD_HEALTH_WIDTH * health) / 100.0));

	if (health > 0)
	{
		Draw_AlphaFillRGB(x, y, health_amount_width, 8, RGBA_TO_COLOR(AUTOID_HEALTHBAR_NORMAL_COLOR, 2 *  MV_HEALTH_OPACITY));
	}

	health = cl.stats[STAT_HEALTH];

	if (health > 100 && health <= 200)
	{
		// Mega health.
		health_amount_width = Q_rint((MV_HUD_HEALTH_WIDTH / 100.0) * (health - 100));

		Draw_AlphaFillRGB(x, y, health_amount_width, 8, RGBA_TO_COLOR(AUTOID_HEALTHBAR_MEGA_COLOR, MV_HEALTH_OPACITY));
	}
	else if (health > 200 && health <= 250)
	{
		// Super health.
		health_amount_width = Q_rint((MV_HUD_HEALTH_WIDTH / 100.0) * (health - 200));

		Draw_AlphaFillRGB(x, y, MV_HUD_HEALTH_WIDTH, 8, RGBA_TO_COLOR(AUTOID_HEALTHBAR_MEGA_COLOR, MV_HEALTH_OPACITY));
		Draw_AlphaFillRGB(x, y, health_amount_width, 8, RGBA_TO_COLOR(AUTOID_HEALTHBAR_TWO_MEGA_COLOR, MV_HEALTH_OPACITY));
	}
	else if (health > 250)
	{
		// Crazy health.
		Draw_AlphaFillRGB(x, y, MV_HUD_HEALTH_WIDTH, 8, RGBA_TO_COLOR(AUTOID_HEALTHBAR_UNNATURAL_COLOR, MV_HEALTH_OPACITY));

	}

	// No powerup.
	if (style >= MV_HUD_STYLE_ALL_TEXT && !(cl.stats[STAT_ITEMS] & IT_INVULNERABILITY))
	{
		Draw_String(x, y, va("%4d", health));
	}

	SCR_MV_SetBoundValue(width, MV_HUD_HEALTH_WIDTH);
	SCR_MV_SetBoundValue(height, 8);
}

void SCR_MV_DrawPowerups (int x, int y)
{
	extern mpic_t  *sb_face_invis;
	extern mpic_t  *sb_face_quad;
	extern mpic_t  *sb_face_invuln;
	extern mpic_t  *sb_face_invis_invuln;

	if (   cl.stats[STAT_ITEMS] & IT_INVULNERABILITY
		&& cl.stats[STAT_ITEMS] & IT_INVISIBILITY)
	{
		// Pentagram + Ring.
		#ifdef GLQUAKE
		Draw_AlphaPic (x + (MV_HUD_HEALTH_WIDTH - sb_face_invis_invuln->width) / 2,
			y - sb_face_invis_invuln->height / 2,
			sb_face_invis_invuln, 0.4);
		#else
		Draw_Pic (x + (MV_HUD_HEALTH_WIDTH - sb_face_invuln->width) / 2,
			y - sb_face_invuln->height / 2,
			sb_face_invis_invuln);
		#endif
	}
	else if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY)
	{
		// Pentagram.
		#ifdef GLQUAKE
		Draw_AlphaPic (x + (MV_HUD_HEALTH_WIDTH - sb_face_invuln->width) / 2,
			y - sb_face_invuln->height / 2,
			sb_face_invuln, 0.4);
		#else
		Draw_Pic (x + (MV_HUD_HEALTH_WIDTH - sb_face_invuln->width) / 2,
			y - sb_face_invuln->height / 2,
			sb_face_invuln);
		#endif
	}
	else if (cl.stats[STAT_ITEMS] & IT_INVISIBILITY)
	{
		// Ring.
		#ifdef GLQUAKE
		Draw_AlphaPic (x + (MV_HUD_HEALTH_WIDTH - sb_face_invis->width) / 2,
			y - sb_face_invis->height / 2,
			sb_face_invis, 0.4);
		#else
		Draw_Pic (x + (MV_HUD_HEALTH_WIDTH - sb_face_invuln->width) / 2,
			y - sb_face_invuln->height / 2,
			sb_face_invis);
		#endif
	}

	if (cl.stats[STAT_ITEMS] & IT_QUAD)
	{
		// Ring.
		#ifdef GLQUAKE
		Draw_AlphaPic (x + (MV_HUD_HEALTH_WIDTH - sb_face_quad->width) / 2,
			y - sb_face_quad->height / 2,
			sb_face_quad, 0.4);
		#else
		Draw_Pic (x + (MV_HUD_HEALTH_WIDTH - sb_face_quad->width) / 2,
			y - sb_face_quad->height / 2,
			sb_face_quad);
		#endif
	}
}

void SCR_MV_DrawCurrentWeapon (int x, int y, int *width, int *height)
{
	mpic_t *current_weapon = NULL;
	current_weapon = SCR_GetActiveWeaponIcon();

	if (current_weapon)
	{
		Draw_Pic (x,
			y - (current_weapon->height / 4),
			current_weapon);
	}

	SCR_MV_SetBoundValue (width, MV_HUD_CURRWEAP_WIDTH);
	SCR_MV_SetBoundValue (height, 8);
}

void SCR_MV_DrawCurrentAmmo (int x, int y, int *width, int *height)
{
	// Draw the ammo count in blue/greyish color.
	Draw_ColoredString (x, y, va("&c5CE%4d", cl.stats[STAT_AMMO]), 0);
	SCR_MV_SetBoundValue (width, MV_HUD_CURRAMMO_WIDTH);
	SCR_MV_SetBoundValue (height, 8);
}

void SCR_MV_DrawWeapons (int x, int y, int *width, int *height, int hud_width, int hud_height, qbool vertical)
{
	#define WEAPON_COUNT 8
	mpic_t *weapon_pic = NULL;
	int weapon = 0;
	int weapon_flag = 0;
	int weapon_x = 0;
	int weapon_y = 0;

	// Draw the weapons the user has.
	for (weapon = 0; weapon < WEAPON_COUNT; weapon++)
	{
		weapon_flag = IT_SHOTGUN << weapon;

		if (cl.stats[STAT_ITEMS] & weapon_flag)
		{
			// Get the weapon picture and draw it.
			weapon_pic = SCR_GetWeaponIconByWeaponNumber (weapon + 2);
			Draw_Pic (x + weapon_x, y + weapon_y, weapon_pic);
		}

		// Evenly distribute the weapons.
		if (!vertical)
		{
			weapon_x += Q_rint((float)hud_width / WEAPON_COUNT);
		}
		else
		{
			weapon_y += Q_rint((float)hud_height / WEAPON_COUNT);
		}
	}
}

void SCR_DrawMVStatusView(mv_viewrect_t *view, int style, int position, qbool flip, qbool vertical)
{
	int hud_x = 0;
	int hud_y = 0;
	int hud_width = 0;
	int hud_height = 0;

	char *name = cl.players[nPlayernum].name;

	if (style == MV_HUD_STYLE_ONLY_NAME)
	{
		// Only draw the players name.

		hud_height = 2*8;
		hud_width = 8 * (strlen(name) + 1);

		//
		// Get the position we should draw the hud at.
		//
		SCR_SetMVStatusPosition (position, view, hud_width, hud_height, &hud_x, &hud_y);

		Draw_String(view->x + hud_x, view->y + hud_y, name);
	}
	else if (style >= MV_HUD_STYLE_ALL)
	{
		#define MV_HUD_VERTICAL_GAP	4

		int name_width = 0;
		int name_height = 0;
		int armor_width = 0;
		int armor_height = 0;
		int health_width = 0;
		int health_height = 0;
		int currweap_width = 0;
		int currweap_height = 0;
		int currammo_width = 0;
		int currammo_height = 0;

		//
		// Calculate the total width and height of the hud.
		//
		if(!vertical)
		{
			hud_height = 3*8;
			hud_width =
				8 * (strlen(name)) +				// Name.
				MV_HUD_ARMOR_WIDTH + 				// Armor + space.
				MV_HUD_HEALTH_WIDTH + 				// Health.
				MV_HUD_CURRWEAP_WIDTH +				// Current weapon.
				MV_HUD_CURRAMMO_WIDTH;				// Current weapon ammo count.

		}
		else
		{
			// Vertical.
			hud_height = 5 * (8 + MV_HUD_VERTICAL_GAP);
			hud_width = max (8 * strlen(name), MV_HUD_ARMOR_WIDTH) + MV_HUD_ARMOR_WIDTH;
		}

		//
		// Get the position we should draw the hud at.
		//
		SCR_SetMVStatusPosition(position, view, hud_width, hud_height, &hud_x, &hud_y);

		//
		// Draw a fill background.
		//
		if(style >= MV_HUD_STYLE_ALL_FILL)
		{
			Draw_AlphaFill(view->x + hud_x, view->y + hud_y, hud_width, hud_height, 0, 0.5);
		}

		// Draw powerups in the middle background of the hud.
		SCR_MV_DrawPowerups(view->x + hud_x + (hud_width / 2), view->y + hud_y + (hud_height / 2));

		// Draw the elements vertically? (Add a small gap between the items when
		// drawing them vertically, otherwise they're too close together).
		#define MV_FLIP(W,H) if(vertical) { hud_y += (H) + MV_HUD_VERTICAL_GAP; } else { hud_x += (W); }

		if (!flip)
		{
			// Name.
			SCR_MV_DrawName (view->x + hud_x, view->y + hud_y, &name_width, &name_height);
			MV_FLIP(name_width, name_height);

			// Armor.
			SCR_MV_DrawArmor (view->x + hud_x, view->y + hud_y, &armor_width, &armor_height, style);
			MV_FLIP(armor_width, armor_height);

			// Health.
			SCR_MV_DrawHealth (view->x + hud_x, view->y + hud_y, &health_width, &health_height, style);
			MV_FLIP(health_width, health_height);

			// Current weapon.
			SCR_MV_DrawCurrentWeapon (view->x + hud_x, view->y + hud_y, &currweap_width, &currweap_height);
			MV_FLIP(currweap_width, currweap_height);

			// Ammo for current weapon.
			SCR_MV_DrawCurrentAmmo (view->x + hud_x, view->y + hud_y, &currammo_width, &currammo_height);
			MV_FLIP(currammo_width, currammo_height);
		}
		else
		{
			//
			// Flipped horizontally.
			//

			// Ammo for current weapon.
			SCR_MV_DrawCurrentAmmo (view->x + hud_x, view->y + hud_y, &currammo_width, &currammo_height);
			MV_FLIP(currammo_width, currammo_height);

			// Current weapon.
			SCR_MV_DrawCurrentWeapon (view ->x + hud_x, view->y + hud_y, &currweap_width, &currweap_height);
			MV_FLIP(currweap_width, currweap_height);

			// Health.
			SCR_MV_DrawHealth (view->x + hud_x, view->y + hud_y, &health_width, &health_height, style);
			MV_FLIP(health_width, health_height);

			// Armor.
			SCR_MV_DrawArmor (view->x + hud_x, view->y + hud_y, &armor_width, &armor_height, style);
			MV_FLIP(armor_width, armor_height);

			// Name.
			SCR_MV_DrawName (view->x + hud_x, view->y + hud_y, &name_width, &name_height);
			MV_FLIP(name_width, name_height);
		}

		if (vertical)
		{
			// Start in the next column.
			hud_x += max (8 * strlen(name), MV_HUD_ARMOR_WIDTH);
			hud_y -= hud_height;
		}
		else
		{
			// Start on the next row.
			hud_x -= hud_width;
			hud_y += hud_height / 2;
		}

		// Weapons.
		SCR_MV_DrawWeapons (view->x + hud_x, view->y + hud_y, NULL, NULL, hud_width, hud_height, vertical);
	}
}

void SCR_SetMVStatusTwoViewRect (mv_viewrect_t *view)
{
	if (CURRVIEW == 2)
	{
		// Top.
		view->x			= 0;
		view->y			= 0;
		view->width		= vid.width;
		view->height	= vid.height / 2;
	}
	else if (CURRVIEW == 1)
	{
		// Bottom.
		view->x			= 0;
		view->y			= vid.height / 2;
		view->width		= vid.width;
		view->height	= vid.height / 2;
	}
}

void SCR_SetMVStatusTwoInsetViewRect (mv_viewrect_t *view)
{
	if (CURRVIEW == 2)
	{
		// Main.
		view->x			= 0;
		view->y			= 0;
		view->width		= vid.width;
		view->height	= vid.height;
	}
	else if (CURRVIEW == 1)
	{
		// Top right.
		view->width		= (vid.width / 3);
		view->height	= (vid.height / 3);
		view->x			= (vid.width / 3) * 2;
		view->y			= 0;
	}
}

void SCR_SetMVStatusThreeViewRect (mv_viewrect_t *view)
{
	if (CURRVIEW == 2)
	{
		// Top.
		view->x			= 0;
		view->y			= 0;
		view->width		= vid.width;
		view->height	= vid.height / 2;
	}
	else if (CURRVIEW == 3)
	{
		// Bottom left.
		view->x			= 0;
		view->y			= vid.height / 2;
		view->width		= vid.width / 2;
		view->height	= vid.height / 2;
	}
	else if (CURRVIEW == 1)
	{
		// Bottom right.
		view->x			= vid.width / 2;
		view->y			= vid.height / 2;
		view->width		= vid.width / 2;
		view->height	= vid.height / 2;
	}
}

void SCR_SetMVStatusFourViewRect (mv_viewrect_t *view)
{
	if (CURRVIEW == 2)
	{
		// Top left.
		view->x			= 0;
		view->y			= 0;
		view->width		= vid.width / 2;
		view->height	= vid.height / 2;
	}
	else if (CURRVIEW == 3)
	{
		// Top right.
		view->x			= vid.width / 2;
		view->y			= 0;
		view->width		= vid.width / 2;
		view->height	= vid.height / 2;
	}
	else if (CURRVIEW == 4)
	{
		// Bottom left.
		view->x			= 0;
		view->y			= vid.height / 2;
		view->width		= vid.width / 2;
		view->height	= vid.height / 2;
	}
	else if (CURRVIEW == 1)
	{
		// Bottom right.
		view->x			= vid.width / 2;
		view->y			= vid.height / 2;
		view->width		= vid.width / 2;
		view->height	= vid.height / 2;
	}
}

void SCR_DrawMVStatus(void)
{
	mv_viewrect_t view;

	// Only draw mini hud when there are more than 1 views.
	if (cl_multiview.value <= 1 || !cls.mvdplayback)
	{
		return;
	}

	// Reset the view.
	memset (&view, -1, sizeof(view));

	//
	// Get the view rect to draw the hud within based on the current
	// multiview mode, and what view that is being drawn.
	//
	if (cl_multiview.value == 2)
	{
		if (cl_mvinset.value)
		{
			// Only draw the mini hud for the inset, 
			// since we probably want the full size hud 
			// for the main view.
			if (CURRVIEW == 2)
			{
				return;
			}

			SCR_SetMVStatusTwoInsetViewRect(&view);
		}
		else
		{
			SCR_SetMVStatusTwoViewRect(&view);
		}
	}
	else if (cl_multiview.value == 3)
	{
		SCR_SetMVStatusThreeViewRect (&view);
	}
	else if (cl_multiview.value == 4)
	{
		SCR_SetMVStatusFourViewRect (&view);
	}

	// Only draw if we the view rect was set properly.
	if (view.x != -1)
	{
		SCR_DrawMVStatusView (&view,
			cl_mvdisplayhud.integer,
			mv_hudpos,
			(qbool)cl_mvhudflip.value,
			(qbool)cl_mvhudvertical.value);
	}
}

void SCR_DrawMVStatusStrings(void)
{
	int xb = 0, yb = 0, xc = 0, yc = 0, xd = 0, yd = 0;
	char strng[80];
	char weapons[40];
	char weapon[3];
	char sAmmo[3];
	char pups[4];
	char armor;
	char name[16];

	int i;

	extern int powerup_cam_active, cam_1, cam_2, cam_3, cam_4;
	extern cvar_t mvd_pc_view_1, mvd_pc_view_2, mvd_pc_view_3, mvd_pc_view_4;

	// Only in MVD.
	if (!cl_multiview.value || !cls.mvdplayback)
	{
		return;
	}

	//
	// Get the current weapon.
	//
	if (cl.stats[STAT_ACTIVEWEAPON] & IT_LIGHTNING || cl.stats[STAT_ACTIVEWEAPON] & IT_SUPER_LIGHTNING)
	{
		strlcpy(weapon, "lg", sizeof(weapon));
	}
	else if (cl.stats[STAT_ACTIVEWEAPON] & IT_ROCKET_LAUNCHER)
	{
		strlcpy(weapon, "rl", sizeof(weapon));
	}
	else if (cl.stats[STAT_ACTIVEWEAPON] & IT_GRENADE_LAUNCHER)
	{
		strlcpy(weapon, "gl", sizeof(weapon));
	}
	else if (cl.stats[STAT_ACTIVEWEAPON] & IT_SUPER_NAILGUN)
	{
		strlcpy(weapon, "sn", sizeof(weapon));
	}
	else if (cl.stats[STAT_ACTIVEWEAPON] & IT_NAILGUN)
	{
		strlcpy(weapon, "ng", sizeof(weapon));
	}
	else if (cl.stats[STAT_ACTIVEWEAPON] & IT_SUPER_SHOTGUN)
	{
		strlcpy(weapon, "ss", sizeof(weapon));
	}
	else if (cl.stats[STAT_ACTIVEWEAPON] & IT_SHOTGUN)
	{
		strlcpy(weapon, "sg", sizeof(weapon));
	}
	else if (cl.stats[STAT_ACTIVEWEAPON] & IT_AXE)
	{
		strlcpy(weapon, "ax", sizeof(weapon));
	}
	else
	{
		strlcpy(weapon, "??", sizeof(weapon));
	}
	weapon[0] |= 128;
	weapon[1] |= 128;

	//
	// Get current powerups.
	//
	pups[0] = pups[1] = pups[2] = ' ';
	pups[3] = '\0';

	if (cl.stats[STAT_ITEMS] & IT_QUAD)
	{
		pups[0] = 'Q';
		pups[0] |= 128;
	}

	if (cl.stats[STAT_ITEMS] & IT_INVISIBILITY)
	{
		pups[1] = 'R';
		pups[1] |= 128;
	}

	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY)
	{
		pups[2] = 'P';
		pups[2] |= 128;
	}

	strng[0] = '\0';

	for (i = 0; i < 8; i++)
	{
		weapons[i]=' ';
		weapons[8] = '\0';
	}

	if (cl.stats[STAT_ITEMS] & IT_AXE)
	{
		weapons[0] = '1' - '0' + 0x12;
	}
	if (cl.stats[STAT_ITEMS] & IT_SHOTGUN)
	{
		weapons[1] = '2' - '0' + 0x12;
	}
	if (cl.stats[STAT_ITEMS] & IT_SUPER_SHOTGUN)
	{
		weapons[2] = '3' - '0' + 0x12;
	}
	if (cl.stats[STAT_ITEMS] & IT_NAILGUN)
	{
		weapons[3] = '4' - '0' + 0x12;
	}
	if (cl.stats[STAT_ITEMS] & IT_SUPER_NAILGUN)
	{
		weapons[4] = '5' - '0' + 0x12;
	}
	if (cl.stats[STAT_ITEMS] & IT_GRENADE_LAUNCHER)
	{
		weapons[5] = '6' - '0' + 0x12;
	}
	if (cl.stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER)
	{
		weapons[6] = '7' - '0' + 0x12;
	}
	if (cl.stats[STAT_ITEMS] & IT_SUPER_LIGHTNING || cl.stats[STAT_ITEMS] & IT_LIGHTNING)
	{
		weapons[7] = '8' - '0' + 0x12;
	}

	armor = ' ';
	if (cl.stats[STAT_ITEMS] & IT_ARMOR1)
	{
		armor='g';
		armor |= 128;
	}
	else if (cl.stats[STAT_ITEMS] & IT_ARMOR2)
	{
		armor='y';
		armor |= 128;
	}
	else if (cl.stats[STAT_ITEMS] & IT_ARMOR3)
	{
		armor='r';
		armor |= 128;
	}

	//
	// Get the player's name.
	//
	strlcpy(name, cl.players[nPlayernum].name, sizeof(name));

	if (strcmp(cl.players[nPlayernum].name, "") && !cl.players[nPlayernum].spectator)
	{
		if ((cl.players[nPlayernum].stats[STAT_HEALTH] <= 0) && (cl_multiview.value == 2) && cl_mvinset.integer)
		{
			// mvinset and dead
			snprintf(sAmmo, sizeof(sAmmo), "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			snprintf(strng, sizeof (strng), "%.5s   %s %s:%-3s",name,
										"dead   ",
										weapon,
										sAmmo);
		}
		else if ((cl.players[nPlayernum].stats[STAT_HEALTH] <= 0) && (vid.width <= 400))
		{
			// Resolution width <= 400 and dead
			snprintf(sAmmo, sizeof(sAmmo), "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			snprintf(strng, sizeof (strng), "%.4s  %s %s:%-3s",name,
										"dead   ",
										weapon,
										sAmmo);
		}
		else if (cl.players[nPlayernum].stats[STAT_HEALTH] <= 0)
		{
			// > 512 and dead
			snprintf(sAmmo, sizeof(sAmmo), "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			snprintf(strng, sizeof (strng),"%s   %s %s:%-3s", name,
										"dead   ",
										weapon,
										sAmmo);
		}

		else if ((cl_multiview.integer == 2) && cl_mvinset.integer && (CURRVIEW == 1))
		{
			// mvinset
			snprintf(sAmmo, sizeof(sAmmo), "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			snprintf(strng, sizeof (strng),"%s %.5s  %c%03d %03d %s:%-3s", pups,
												name,
												armor,
												cl.players[nPlayernum].stats[STAT_ARMOR],
												cl.players[nPlayernum].stats[STAT_HEALTH],
												weapon,
												sAmmo);
		}
		else if (cl_multiview.value && vid.width <= 400)
		{
			// <= 400 and alive
			snprintf(sAmmo, sizeof(sAmmo), "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			snprintf(strng, sizeof (strng),"%s %.4s %c%03d %03d %s:%-3s", pups,
												name,
												armor,
												cl.players[nPlayernum].stats[STAT_ARMOR],
												cl.players[nPlayernum].stats[STAT_HEALTH],
												weapon,
												sAmmo);
		}
		else
		{
			snprintf(sAmmo, sizeof(sAmmo), "%02d", cl.players[nPlayernum].stats[STAT_AMMO]); // > 512 and alive
			snprintf(strng, sizeof (strng),"%s %s  %c%03d %03d %s:%-3s", pups,
												name,
												armor,
												cl.players[nPlayernum].stats[STAT_ARMOR],
												cl.players[nPlayernum].stats[STAT_HEALTH],
												weapon,
												sAmmo);
		}
	}

	//
	// Powerup cam stuff.
	//
	if (CURRVIEW == 1 && mvd_pc_view_1.string && strlen(mvd_pc_view_1.string) && powerup_cam_active && cam_1)
	{
		sAmmo[0] = '\0';
		strng[0] = '\0';
		weapons[0] = '\0';
	}
	else if (CURRVIEW == 2 && mvd_pc_view_2.string && strlen(mvd_pc_view_2.string) && powerup_cam_active && cam_2)
	{
		sAmmo[0] = '\0';
		strng[0] = '\0';
		weapons[0] = '\0';
	}
	else if (CURRVIEW == 3 && mvd_pc_view_3.string && strlen(mvd_pc_view_3.string) && powerup_cam_active && cam_3)
	{
		sAmmo[0] = '\0';
		strng[0] = '\0';
		weapons[0] = '\0';
	}
	else if (CURRVIEW == 4 && mvd_pc_view_4.string && strlen(mvd_pc_view_4.string) && powerup_cam_active && cam_4)
	{
		sAmmo[0] = '\0';
		strng[0] = '\0';
		weapons[0] = '\0';
	}

	//
	// Placement.
	//
	if (cl_multiview.value == 1)
	{
			xb = vid.width - strlen(strng) * 8 - 12;
			yb = vid.height - sb_lines - 16;
			xc = vid.width / 2;
			yc = vid.height / 2;
			xd = vid.width - strlen(weapons) * 8 - 84;
			yd = vid.height - sb_lines - 8;
	}
	else if (cl_multiview.value == 2)
	{
		if (!cl_mvinset.value)
		{
			if (CURRVIEW == 2)
			{
				// Top
				xb = vid.width - strlen(strng) * 8 - 12;
				yb = vid.height / 2 - sb_lines - 16;
				xc = vid.width / 2;
				yc = vid.height / 4;
				xd = vid.width - strlen(weapons) * 8 - 84;
				yd = vid.height / 2 - sb_lines - 8;
			}
			else if (CURRVIEW == 1)
			{
				// Bottom
				xb = vid.width - strlen(strng) * 8 - 12;
				yb = vid.height - sb_lines - 16;
				xc = vid.width / 2;
				yc = vid.height / 2 + vid.height / 4;
				xd = vid.width - strlen(weapons) * 8 - 84;
				yd = vid.height - sb_lines - 8;
			}
		}
		else if (cl_mvinset.value)
		{
			if (CURRVIEW == 2)
			{
				// Main
				xb = vid.width - strlen(strng) * 8 - 12;
				yb = vid.height / 2 - sb_lines - 16;
				xc = -2;
				yc = -2;
				xd = vid.width - strlen(weapons) * 8 - 84;
				yd = vid.height / 2 - sb_lines - 8;
			}
			else if (CURRVIEW == 1)
			{
				// Top right
				xb = vid.width - strlen(strng) * 8; // hud
				yb = vid.height / 3 - 16;
				if (cl_mvinsetcrosshair.value)
				{
					xc = vid.width - (double)(vid.width / 3) / 2 - 2;
					yc = (vid.height / 3) / 2;
				}
				else
				{
					xc = yc = -2;
				}
				xd = vid.width - strlen(weapons) * 8 - 70; // weapons
				yd = vid.height / 3 - 8;
			}
		}
	}
	else if (cl_multiview.value == 3)
	{
		if (CURRVIEW == 2)
		{
			// top
			xb = vid.width - strlen(strng) * 8 - 12;
			yb = vid.height / 2 - sb_lines - 16;
			xc = vid.width / 2;
			yc = vid.height / 4;
			xd = vid.width - strlen(weapons) * 8 - 84;
			yd = vid.height / 2 - sb_lines - 8;
		}
		else if (CURRVIEW == 3)
		{
			// Bottom left
			xb = vid.width - (vid.width / 2)- strlen(strng) * 8 - 12;
			yb = vid.height - sb_lines - 16;
			xc = vid.width / 4;
			yc = vid.height / 2 + vid.height/4;
			xd = vid.width - (vid.width/2)- strlen(weapons) * 8 - 84;
			yd = vid.height - sb_lines - 8;
		}
		else if (CURRVIEW == 1)
		{
			// Bottom right
			xb = vid.width - strlen(strng) * 8 - 12;
			yb = vid.height - sb_lines - 16;
			xc = vid.width / 2 + vid.width / 4;
			yc = vid.height / 2 + vid.height / 4;
			xd = vid.width - strlen(weapons) * 8 - 84;
			yd = vid.height - sb_lines - 8;
		}
	}
	else if (cl_multiview.value == 4)
	{
		if (CURRVIEW == 2)
		{
			// Top left
			xb = vid.width - (vid.width / 2)- strlen(strng) * 8 - 12;
			yb = vid.height / 2 - sb_lines - 16;
			xc = vid.width / 4;
			yc = vid.height / 4;
			xd = vid.width - (vid.width / 2)- strlen(weapons) * 8 - 84;
			yd = vid.height/2 - sb_lines - 8;
		}
		else if (CURRVIEW == 3)
		{
			// Top right
			xb = vid.width - strlen(strng) * 8 - 12;
			yb = vid.height / 2 - sb_lines - 16;
			xc = vid.width / 2 + vid.width / 4;
			yc = vid.height / 4;
			xd = vid.width - strlen(weapons) - 25 * 8 - 8;
			yd = vid.height - sb_lines - (vid.height / 1.9);
			xd = vid.width - strlen(weapons) * 8 - 84;
			yd = vid.height / 2 - sb_lines - 8;
		}
		else if (CURRVIEW == 4)
		{
			// Bottom left
			xb = vid.width - (vid.width / 2)- strlen(strng) * 8 - 12;
			yb = vid.height - sb_lines - 16;
			xc = vid.width / 4;
			yc = vid.height / 2 + vid.height / 4;
			xd = vid.width - (vid.width / 2)- strlen(weapons) * 8 - 84;
			yd = vid.height - sb_lines - 8;
		}
		else if (CURRVIEW == 1)
		{
			// Bottom right
			xb = vid.width - strlen(strng) * 8 - 12;
			yb = vid.height - sb_lines - 16;
			xc = vid.width / 2 + vid.width / 4;
			yc = vid.height / 2 + vid.height / 4;
			xd = vid.width - strlen(weapons) * 8 - 84;
			yd = vid.height - sb_lines - 8;
		}
	}

	if (cl_multiview.value == 2 && cl_mvinset.value)
	{
		memcpy(cl.stats, cl.players[nTrack1duel].stats, sizeof(cl.stats));
	}

	// Fill the void
	if (cl_sbar.value && cl_multiview.value==2 && cl_mvinset.value && cl_mvinsethud.value && cl_mvdisplayhud.value)
	{
		if (vid.width > 512)
		{
			Draw_Fill(vid.width / 3 * 2 + 1, vid.height / 3 - sb_lines / 3 + 1, vid.width / 3 + 2, sb_lines / 3 - 1, 0);
		}
		else
		{
			Draw_Fill(vid.width / 3 * 2 + 1, vid.height / 3 - sb_lines / 3 + 1, vid.width / 3 + 2, sb_lines / 6 + 1, 0);
		}
	}

	// Hud info
	if ((cl_mvdisplayhud.value && !cl_mvinset.value && cl_multiview.value == 2)
		|| (cl_mvdisplayhud.value && cl_multiview.value != 2))
	{
		Draw_String(xb, yb, strng);
	}
	else if (cl_multiview.value == 2 && cl_mvdisplayhud.value && CURRVIEW == 1 && cl_mvinsethud.value)
	{
		if (vid.width > 512)
		{
			Draw_String(xb, yb, strng);
		}
		else
		{
			// <= 512 mvinset, just draw the name
			int var, limit;
			char namestr[16];

			var = (vid.width - 320) * 0.05;
			var--;
			var |= (var >> 1);
			var |= (var >> 2);
			var |= (var >> 4);
			var |= (var >> 8);
			var |= (var >> 16);
			var++;

			limit = ceil(7.0 / 192 * vid.width + 4 / 3); // linearly limit length of name for 320->512 conwidth to fit in inset
			strlcpy(namestr, name, limit);

			if (cl_sbar.value)
			{
				Draw_String(vid.width - strlen(namestr) * 8 - var - 2, yb + 1, namestr);
			}
			else
			{
				Draw_String(vid.width - strlen(namestr) * 8 - var - 2, yb + 4, namestr);
			}
		}
	}

	// Weapons
	if ((cl_mvdisplayhud.value && !cl_mvinset.value && cl_multiview.value == 2)
		|| (cl_mvdisplayhud.value && cl_multiview.value != 2))
	{
		Draw_String(xd, yd, weapons);
	}
	else if (cl_multiview.value == 2 && cl_mvdisplayhud.value && CURRVIEW == 1 && vid.width > 512  && cl_mvinsethud.value)
	{
		Draw_String(xd, yd, weapons);
	}
}

// QW262 -->
void Hud_262Init (void)
{
//
// register hud commands
//
	Cmd_AddCommand ("hud262_add",Hud_Add_f);
	Cmd_AddCommand ("hud262_remove",Hud_Remove_f);
	Cmd_AddCommand ("hud262_position",Hud_Position_f);
	Cmd_AddCommand ("hud262_bg",Hud_Bg_f);
	Cmd_AddCommand ("hud262_move",Hud262_Move_f);
	Cmd_AddCommand ("hud262_width",Hud_Width_f);
#ifdef GLQUAKE
	//Cmd_AddCommandTrig ("hud_262font",Hud_Font_f);
	Cmd_AddCommand ("hud262_alpha",Hud_Alpha_f);
#endif
	Cmd_AddCommand ("hud262_blink",Hud_Blink_f);
	Cmd_AddCommand ("hud262_disable",Hud_Disable_f);
	Cmd_AddCommand ("hud262_enable",Hud_Enable_f);
	Cmd_AddCommand ("hud262_list",Hud_List_f);
	Cmd_AddCommand ("hud262_bringtofront",Hud_BringToFront_f);
//	Cmd_AddCommand ("hud262_hover",);
//	Cmd_AddCommand ("hud262_button",Hud_Button_f);
}

void Hud_262LoadOnFirstStart(void)
{
	extern char *hud262_load_buff;

	if (hud262_load_buff != NULL){
		Cbuf_AddText(hud262_load_buff);
		Cbuf_Execute();
	}
	Q_free(hud262_load_buff);
}
// <-- QW262
