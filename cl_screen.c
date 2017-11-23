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
#include "gl_local.h"
#include "mvd_utils.h"
#include "keys.h"
#include "hud.h"
#include "hud_common.h"
#include "hud_editor.h"
#include "utils.h"
#include "vx_stuff.h"
#include "gl_model.h"
#include "gl_local.h"
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

void WeaponStats_CommandInit(void);
void SCR_DrawHud(void);

int				glx, gly, glwidth, glheight;

#define			DEFAULT_SSHOT_FORMAT		"png"
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
void OnChange_scr_clock_format (cvar_t *var, char *value, qbool *cancel);
cvar_t	scr_fov					= {"fov", "90", CVAR_NONE, OnFovChange};	// 10 - 140
cvar_t	default_fov				= {"default_fov", "90", CVAR_NONE, OnDefaultFovChange};
cvar_t	scr_viewsize			= {"viewsize", "100", CVAR_NONE};
cvar_t	scr_consize				= {"scr_consize", "0.5"};
cvar_t	scr_conspeed			= {"scr_conspeed", "9999"};
cvar_t	scr_centertime			= {"scr_centertime", "2"};
cvar_t	scr_centershift			= {"scr_centershift", "0"};
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

cvar_t	show_speed			= {"show_speed", "0"};
cvar_t	show_speed_x			= {"show_speed_x", "-1"};
cvar_t	show_speed_y			= {"show_speed_y", "1"};

cvar_t	show_velocity_3d		= {"show_velocity_3d", "0"};
cvar_t	show_velocity_3d_offset_forward	= {"show_velocity_3d_offset_forward", "2.5"};
cvar_t	show_velocity_3d_offset_down	= {"show_velocity_3d_offset_down", "5"};

cvar_t	show_fps				= {"show_fps", "0"};
cvar_t	show_fps_x				= {"show_fps_x", "-5"};
cvar_t	show_fps_y				= {"show_fps_y", "-1"};

cvar_t	scr_sshot_autoname		= {"sshot_autoname", "0"};
cvar_t	scr_sshot_format		= {"sshot_format", DEFAULT_SSHOT_FORMAT};
cvar_t	scr_sshot_dir			= {"sshot_dir", ""};

cvar_t	cl_hud					= {"cl_hud", "1"};	// QW262 HUD.

cvar_t	gl_triplebuffer			= {"gl_triplebuffer", "1"};
cvar_t  r_chaticons_alpha		= {"r_chaticons_alpha", "0.8"};
cvar_t	scr_autoid				= {"scr_autoid", "5"};
cvar_t	scr_autoid_weapons		= {"scr_autoid_weapons", "2"};
cvar_t	scr_autoid_namelength	= {"scr_autoid_namelength", "0"};
cvar_t	scr_autoid_barlength	= {"scr_autoid_barlength", "16"};
cvar_t	scr_autoid_weaponicon	= {"scr_autoid_weaponicon", "1"};
cvar_t	scr_autoid_scale		= {"scr_autoid_scale", "1"};
cvar_t	scr_coloredfrags		= {"scr_coloredfrags", "0"};

cvar_t  scr_teaminfo_order       = {"scr_teaminfo_order", "%p%n $x10%l$x11 %a/%H %w", CVAR_NONE, OnChange_scr_clock_format};
cvar_t	scr_teaminfo_align_right = {"scr_teaminfo_align_right", "1"};
cvar_t	scr_teaminfo_frame_color = {"scr_teaminfo_frame_color", "10 0 0 120", CVAR_COLOR};
cvar_t	scr_teaminfo_scale		 = {"scr_teaminfo_scale",       "1"};
cvar_t	scr_teaminfo_y			 = {"scr_teaminfo_y",           "0"};
cvar_t  scr_teaminfo_x			 = {"scr_teaminfo_x",           "0"};
cvar_t  scr_teaminfo_loc_width	 = {"scr_teaminfo_loc_width",   "5"};
cvar_t  scr_teaminfo_name_width	 = {"scr_teaminfo_name_width",  "6"};
cvar_t	scr_teaminfo_low_health	 = {"scr_teaminfo_low_health",  "25"};
cvar_t	scr_teaminfo_armor_style = {"scr_teaminfo_armor_style", "3"};
cvar_t	scr_teaminfo_powerup_style	= {"scr_teaminfo_powerup_style", "1"};
cvar_t	scr_teaminfo_weapon_style= {"scr_teaminfo_weapon_style","1"};
cvar_t  scr_teaminfo_show_enemies= {"scr_teaminfo_show_enemies","0"};
cvar_t  scr_teaminfo_show_self   = {"scr_teaminfo_show_self",   "2"};
cvar_t  scr_teaminfo			 = {"scr_teaminfo",             "1"};

cvar_t  scr_shownick_order		 = {"scr_shownick_order", "%p%n %a/%H %w", CVAR_NONE, OnChange_scr_clock_format};
cvar_t	scr_shownick_frame_color = {"scr_shownick_frame_color", "10 0 0 120", CVAR_COLOR};
cvar_t	scr_shownick_scale		 = {"scr_shownick_scale",		"1"};
cvar_t	scr_shownick_y			 = {"scr_shownick_y",			"0"};
cvar_t	scr_shownick_x			 = {"scr_shownick_x",			"0"};
cvar_t  scr_shownick_name_width	 = {"scr_shownick_name_width",	"6"};
cvar_t  scr_shownick_time		 = {"scr_shownick_time",		"0.8"};

// Tracking text.
cvar_t	scr_tracking			= {"scr_tracking", "\xD4\xF2\xE1\xE3\xEB\xE9\xEE\xE7\xBA %t %n, \xCA\xD5\xCD\xD0 for next"}; //"Tracking: [team] name, JUMP for next", "Tracking:" and "JUMP" are brown. default: "Tracking %t %n, [JUMP] for next"
cvar_t	scr_spectatorMessage	= {"scr_spectatorMessage", "1"};

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

static int scr_autosshot_countdown = 0;
char auto_matchname[2 * MAX_OSPATH];

static void SCR_CheckAutoScreenshot(void);
void SCR_DrawMultiviewBorders(void);
void SCR_DrawMVStatus(void);
void SCR_DrawMVStatusStrings(void);
void Draw_AlphaFill (int x, int y, int w, int h, byte c, float alpha);
void Draw_AlphaPic (int x, int y, mpic_t *pic, float alpha);

qbool SCR_TakingAutoScreenshot(void)
{
	return scr_autosshot_countdown > 0;
}

void OnChange_scr_allowsnap(cvar_t *var, char *s, qbool *cancel)
{
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
static void SCR_CalcRefdef (void) {
	float size        = 0;
	qbool full        = false;
	int h;
	float aspectratio = vid.height ? (float)vid.width / vid.height : 1;
	qbool letterbox = scr_fovmode.integer == 1;
	qbool height_reduced = false;

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

#ifdef EXPERIMENTAL_SHOW_ACCELERATION
static void draw_accel_bar(int x, int y, int length, int charsize, int pos)
{
	if (GL_ShadersSupported()) {
		GLC_DrawAccelBar(x, y, length, charsize, pos);
	}
	else {
		GLM_DrawAccelBar(x, y, length, charsize, pos);
	}
}

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

	draw_accel_bar(x, y, length, charsize, pos);

	//scale: show most interesting
	pos = (int) ((cosinus_val * scale_factor + 1) * length / 2);

	draw_accel_bar(x, y - 2 * charsize, length, charsize, pos);

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



void SCR_DrawFPS(void)
{
	int x, y;
	char str[80];
	extern double lastfps;

	if (!show_fps.value || scr_newHud.value == 1) // HUD -> hexum - newHud has its own fps
		return;

	// Multiview
	if (CL_MultiviewEnabled()) {
		snprintf(str, sizeof(str), "%3.1f", (lastfps + 0.05) / CL_MultiviewNumberViews());
	}
	else {
		snprintf(str, sizeof(str), "%3.1f", lastfps + 0.05);
	}

	x = ELEMENT_X_COORD(show_fps);
	y = ELEMENT_Y_COORD(show_fps);
	Draw_String(x, y, str);
}

void SCR_DrawSpeed (void)
{
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

	if (!cls.demoplayback || cls.mvdplayback == QTV_PLAYBACK || !scr_democlock.value || scr_newHud.value == 1) // newHud has its own democlock
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

/*********************************** AUTOID ***********************************/

cvar_t scr_autoid_healthbar_bg_color        = { "scr_autoid_healthbar_bg_color", "180 115 115 100", CVAR_COLOR };
cvar_t scr_autoid_healthbar_normal_color    = { "scr_autoid_healthbar_normal_color", "80 0 0 255", CVAR_COLOR };
cvar_t scr_autoid_healthbar_mega_color      = { "scr_autoid_healthbar_mega_color", "255 0 0 255", CVAR_COLOR };
cvar_t scr_autoid_healthbar_two_mega_color  = { "scr_autoid_healthbar_two_mega_color", "255 100 0 255", CVAR_COLOR };
cvar_t scr_autoid_healthbar_unnatural_color = { "scr_autoid_healthbar_unnatural_color", "255 255 255 255", CVAR_COLOR };

cvar_t scr_autoid_armorbar_green_armor      = { "scr_autoid_armorbar_green_armor", "25 170 0 255", CVAR_COLOR };
cvar_t scr_autoid_armorbar_yellow_armor     = { "scr_autoid_armorbar_yellow_armor", "255 220 0 255", CVAR_COLOR };
cvar_t scr_autoid_armorbar_red_armor        = { "scr_autoid_armorbar_red_armor", "255 0 0 255", CVAR_COLOR };

#define AUTOID_HEALTHBAR_OFFSET_Y			16

#define AUTOID_ARMORBAR_OFFSET_Y			(AUTOID_HEALTHBAR_OFFSET_Y + 5)
#define AUTOID_ARMORNAME_OFFSET_Y			(AUTOID_ARMORBAR_OFFSET_Y + 8 + 2)
#define AUTOID_ARMORNAME_OFFSET_X			8

#define AUTOID_WEAPON_OFFSET_Y				AUTOID_HEALTHBAR_OFFSET_Y
#define AUTOID_WEAPON_OFFSET_X				2

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

	if (cls.state != ca_active || !cl.validsequence || cl.intermission)
		return;

	if (!cls.demoplayback && !cl.spectator)
		return;

	GL_GetMatrix(GL_MODELVIEW_MATRIX, model);
	GL_GetMatrix(GL_PROJECTION_MATRIX, project);
	GL_GetViewport((GLint *)view);

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

void SCR_DrawAutoIDStatus (autoid_player_t *autoid_p, int x, int y, float scale)
{
	char armor_name[20];
	char weapon_name[20];
	int bar_length;

	if (scr_autoid_barlength.integer > 0) {
		bar_length = scr_autoid_barlength.integer;
	} else {
		if (scr_autoid_namelength.integer >= 1) {
			bar_length = min(scr_autoid_namelength.integer, strlen(autoid_p->player->name)) * 4;
		} else {
			bar_length = strlen(autoid_p->player->name) * 4;
		}
	}

	// Draw health above the name.
	if (scr_autoid.integer >= 2)
	{
		int health;
		int health_length;

		health = autoid_p->player->stats[STAT_HEALTH];
		health = min(100, health);
		health_length = Q_rint((bar_length/100.0) * health);

		// Normal health.
		Draw_AlphaFillRGB(x - bar_length * scale, y - AUTOID_HEALTHBAR_OFFSET_Y * scale, bar_length * 2 * scale, 4 * scale, RGBAVECT_TO_COLOR(scr_autoid_healthbar_bg_color.color));
		Draw_AlphaFillRGB(x - bar_length * scale, y - AUTOID_HEALTHBAR_OFFSET_Y * scale, health_length * 2 * scale, 4 * scale, RGBAVECT_TO_COLOR(scr_autoid_healthbar_normal_color.color));

		health = autoid_p->player->stats[STAT_HEALTH];

		// Mega health
		if(health > 100 && health <= 200)
		{
			health_length = Q_rint((bar_length/100.0) * (health - 100));
			Draw_AlphaFillRGB(x - bar_length * scale, y - AUTOID_HEALTHBAR_OFFSET_Y * scale, health_length * 2 * scale, 4 * scale, RGBAVECT_TO_COLOR(scr_autoid_healthbar_mega_color.color));
		}
		else if(health > 200 && health <= 250)
		{
			// Super health.
			health_length = Q_rint((bar_length/100.0) * (health - 200));
			Draw_AlphaFillRGB(x - bar_length * scale, y - AUTOID_HEALTHBAR_OFFSET_Y * scale, bar_length * 2 * scale, 4 * scale, RGBAVECT_TO_COLOR(scr_autoid_healthbar_mega_color.color));
			Draw_AlphaFillRGB(x - bar_length * scale, y - AUTOID_HEALTHBAR_OFFSET_Y * scale, health_length * 2 * scale, 4 * scale, RGBAVECT_TO_COLOR(scr_autoid_healthbar_two_mega_color.color));
		}
		else if(health > 250)
		{
			// Crazy health.
			// This will never happen during a normal game.
			Draw_AlphaFillRGB(x - bar_length * scale, y - AUTOID_HEALTHBAR_OFFSET_Y * scale, bar_length * 2 * scale, 4 * scale, RGBAVECT_TO_COLOR(scr_autoid_healthbar_unnatural_color.color));
		}
	}

	// Draw armor.
	if (scr_autoid.integer >= 2)
	{
		int armor;
		int armor_length;
		cvar_t* armor_color = NULL;

		armor = autoid_p->player->stats[STAT_ARMOR];
		armor = min(100, armor);
		armor_length = Q_rint((bar_length/100.0) * armor);

		if (autoid_p->player->stats[STAT_ITEMS] & IT_ARMOR1) {
			armor_color = &scr_autoid_armorbar_green_armor;
		}
		else if (autoid_p->player->stats[STAT_ITEMS] & IT_ARMOR2) {
			armor_color = &scr_autoid_armorbar_yellow_armor;
		}
		else if (autoid_p->player->stats[STAT_ITEMS] & IT_ARMOR3) {
			armor_color = &scr_autoid_armorbar_red_armor;
		}

		if (armor_color != NULL) {
			color_t background = RGBA_TO_COLOR (armor_color->color[0], armor_color->color[1], armor_color->color[2], 50);
			color_t foreground = RGBAVECT_TO_COLOR (armor_color->color);

			Draw_AlphaFillRGB(x - bar_length * scale, y - AUTOID_ARMORBAR_OFFSET_Y * scale, bar_length * 2 * scale, 4 * scale, background);
			Draw_AlphaFillRGB(x - bar_length * scale, y - AUTOID_ARMORBAR_OFFSET_Y * scale, armor_length * 2 * scale, 4 * scale, foreground);
		}
	}

	// Draw the name of the armor type.
	if(scr_autoid.integer >= 3 && scr_autoid.integer < 6 && autoid_p->player->stats[STAT_ITEMS] & (IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3))
	{
		if(autoid_p->player->stats[STAT_ITEMS] & IT_ARMOR1) {
			strlcpy(armor_name, "&c0f0GA", sizeof(armor_name));
		} else if(autoid_p->player->stats[STAT_ITEMS] & IT_ARMOR2) {
			strlcpy(armor_name, "&cff0YA", sizeof(armor_name));
		} else {
			strlcpy(armor_name, "&cf00RA", sizeof(armor_name));
		}

		Draw_SColoredString(
				x - AUTOID_ARMORNAME_OFFSET_X * scale,
				y - AUTOID_ARMORNAME_OFFSET_Y * scale,
				str2wcs(armor_name), NULL, 0, 0, scale);
	}

	if(scr_autoid_weapons.integer > 0 && (scr_autoid.integer >= 4))
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
				strlcpy(weapon_name, "SG", sizeof(weapon_name));
				break;
			case IT_SUPER_SHOTGUN:
				weapon_pic = sb_weapons[0][1];
				strlcpy(weapon_name, "BS", sizeof(weapon_name));
				break;
			case IT_NAILGUN:
				weapon_pic = sb_weapons[0][2];
				strlcpy(weapon_name, "NG", sizeof(weapon_name));
				break;
			case IT_SUPER_NAILGUN:
				weapon_pic = sb_weapons[0][3];
				strlcpy(weapon_name, "SN", sizeof(weapon_name));
				break;
			case IT_GRENADE_LAUNCHER:
				weapon_pic = sb_weapons[0][4];
				strlcpy(weapon_name, "GL", sizeof(weapon_name));
				break;
			case IT_ROCKET_LAUNCHER:
				weapon_pic = sb_weapons[0][5];
				strlcpy(weapon_name, "RL", sizeof(weapon_name));
				break;
			case IT_LIGHTNING:
				weapon_pic = sb_weapons[0][6];
				strlcpy(weapon_name, "LG", sizeof(weapon_name));
				break;
			default :
				// No weapon.
				break;
		}

		if(weapon_pic != NULL && best_weapon > 0 && ((scr_autoid.integer == 4 && best_weapon == 7) || (scr_autoid.integer > 4 && best_weapon >= scr_autoid_weapons.integer)))
		{
			if (scr_autoid_weaponicon.value) {
				Draw_SSubPic (
						x - (bar_length + weapon_pic->width + AUTOID_WEAPON_OFFSET_X) * scale,
						y - (AUTOID_HEALTHBAR_OFFSET_Y + Q_rint((weapon_pic->height/2.0))) * scale,
						weapon_pic,
						0,
						0,
						weapon_pic->width,
						weapon_pic->height,
						scale);
			} else {
				Draw_SColoredString(
						x - (bar_length + 16 + AUTOID_WEAPON_OFFSET_X) * scale,
						y - (AUTOID_HEALTHBAR_OFFSET_Y + 4) * scale,
						str2wcs(weapon_name), NULL, 0, 1, scale);
			}
		}
	}
}

void SCR_DrawAutoID(void)
{
	int i, x, y;
	float scale;

	if (!scr_autoid.value || (!cls.demoplayback && !cl.spectator) || cl.intermission)
		return;

	for (i = 0; i < autoid_count; i++) {
		x = autoids[i].x * vid.width / glwidth;
		y = (glheight - autoids[i].y) * vid.height / glheight;
		scale = (scr_autoid_scale.value > 0 ? scr_autoid_scale.value : 1.0);

		if (scr_autoid.integer) {
			if (scr_autoid_namelength.integer >= 1 && scr_autoid_namelength.integer < MAX_SCOREBOARDNAME) {
				char name[MAX_SCOREBOARDNAME];

				strlcpy(name, autoids[i].player->name, sizeof(name));
				name[scr_autoid_namelength.integer] = 0;
				Draw_SString(x - strlen(name) * 4 * scale, y - 8 * scale, name, scale);
			}
			else {
				Draw_SString(x - strlen(autoids[i].player->name) * 4 * scale, y - 8 * scale, autoids[i].player->name, scale);
			}
		}

		// We only have health/armor info for all players when in demo playback.
		if (cls.demoplayback && scr_autoid.value >= 2) {
			SCR_DrawAutoIDStatus(&autoids[i], x, y, scale);
		}
	}
}

// scr_teaminfo 
// Variable ti_clients and related functions also used by hud_teaminfo in hud_common.c
ti_player_t ti_clients[MAX_CLIENTS];

mpic_t * SCR_GetWeaponIconByFlag (int flag);

void SCR_ClearTeamInfo(void)
{
	memset(ti_clients, 0, sizeof(ti_clients));
}

qbool Has_Both_RL_and_LG (int flags);
static int SCR_Draw_TeamInfoPlayer(ti_player_t *ti_cl, int x, int y, int maxname, int maxloc, qbool width_only, qbool its_shownick)
{
	char *s, *loc, tmp[1024], tmp2[MAX_MACRO_STRING], *aclr;
	int x_in = x; // save x
	int i;
	mpic_t *pic;

	extern mpic_t  *sb_face_invis, *sb_face_quad, *sb_face_invuln;
	extern mpic_t  *sb_armor[3];
	extern mpic_t  *sb_items[32];
	extern cvar_t tp_name_rlg;

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
							str_align_right(tmp, sizeof(tmp), nick, maxname);
							Draw_ColoredString (x, y, tmp, false);
						}
						x += maxname * FONTWIDTH;

						break;
					case 'w': // draw "best" weapon icon/name

						switch (scr_teaminfo_weapon_style.integer) {
							case 1:
								if(!width_only) {
									if (Has_Both_RL_and_LG(ti_cl->items)) {
										char *weap_str = tp_name_rlg.string;
										char weap_white_stripped[32];
										Util_SkipChars(weap_str, "{}", weap_white_stripped, 32);
										Draw_ColoredString (x, y, weap_white_stripped, false);
									}
									else {
										char *weap_str = TP_ItemName(BestWeaponFromStatItems( ti_cl->items ));
										char weap_white_stripped[32];
										Util_SkipChars(weap_str, "{}", weap_white_stripped, 32);
										Draw_ColoredString (x, y, weap_white_stripped, false);
									}
								}
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

									Draw_AlphaRectangleRGB(x, y, 3 * FONTWIDTH, FONTWIDTH, 0, true, RGBAVECT_TO_COLOR(col));
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

						if(!width_only) { // value drawn no matter which style
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

							str_align_right(tmp, sizeof(tmp), TP_ParseFunChars(loc, false), maxloc);
							Draw_ColoredString (x, y, tmp, false);
						}
						x += maxloc * FONTWIDTH;

						break;


					case 'p': // draw powerups	
						switch (scr_teaminfo_powerup_style.integer) {
							case 1: // quad/pent/ring image
								if(!width_only) {
									if (ti_cl->items & IT_QUAD)
										Draw_SPic (x, y, sb_items[5], 1.0/2);
									x += FONTWIDTH;
									if (ti_cl->items & IT_INVULNERABILITY)
										Draw_SPic (x, y, sb_items[3], 1.0/2);
									x += FONTWIDTH;
									if (ti_cl->items & IT_INVISIBILITY)
										Draw_SPic (x, y, sb_items[2], 1.0/2);
									x += FONTWIDTH;
								}
								else { x += 3* FONTWIDTH; }
								break;

							case 2: // player powerup face
								if(!width_only) {
									if ( sb_face_quad && (ti_cl->items & IT_QUAD))
										Draw_SPic (x, y, sb_face_quad, 1.0/3);
									x += FONTWIDTH;
									if ( sb_face_invuln && (ti_cl->items & IT_INVULNERABILITY))
										Draw_SPic (x, y, sb_face_invuln, 1.0/3);
									x += FONTWIDTH;
									if ( sb_face_invis && (ti_cl->items & IT_INVISIBILITY))
										Draw_SPic (x, y, sb_face_invis, 1.0/3);
									x += FONTWIDTH;
								}
								else { x += 3* FONTWIDTH; }
								break;

							case 3: // colored font (QPR)
								if(!width_only) {
									if (ti_cl->items & IT_QUAD)
										Draw_ColoredString (x, y, "&c03fQ", false);
									x += FONTWIDTH;
									if (ti_cl->items & IT_INVULNERABILITY)
										Draw_ColoredString (x, y, "&cf00P", false);
									x += FONTWIDTH;
									if (ti_cl->items & IT_INVISIBILITY)
										Draw_ColoredString (x, y, "&cff0R", false);
									x += FONTWIDTH;
								}
								else { x += 3* FONTWIDTH; }
								break;
						}
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
	float oldMatrix[16];

	float	scale = bound(0.1, scr_teaminfo_scale.value, 10);

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

	GL_TextureEnvMode(GL_MODULATE);
	GL_Color4f(1, 1, 1, 1);
	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);

	if (scale != 1) {
		GL_PushMatrix(GL_PROJECTION, oldMatrix);
		GL_Scale(GL_PROJECTION, scale, scale, 1);
	}

	y = vid.height*0.6/scale + scr_teaminfo_y.value;

	// this does't draw anything, just calculate width
	w = SCR_Draw_TeamInfoPlayer(&ti_clients[0], 0, 0, maxname, maxloc, true, false);
	h = slots_num;

	for ( j = 0; j < slots_num; j++ ) {
		i = slots[j];

		x = (scr_teaminfo_align_right.value ? (vid.width/scale - w * FONTWIDTH) - FONTWIDTH : FONTWIDTH);
		x += scr_teaminfo_x.value;

		if ( !j ) { // draw frame
			byte	*col = scr_teaminfo_frame_color.color;

			Draw_AlphaRectangleRGB(x, y, w * FONTWIDTH, h * FONTWIDTH, 0, true, RGBAVECT_TO_COLOR(col));
		}

		SCR_Draw_TeamInfoPlayer(&ti_clients[i], x, y, maxname, maxloc, false, false);

		y += FONTWIDTH;
	}

	if (scale != 1) {
		GL_PopMatrix(GL_PROJECTION, oldMatrix);
	}

	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
	GL_TextureEnvMode(GL_REPLACE);
	GL_Color4f(1, 1, 1, 1);
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

void Update_TeamInfo(void)
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
	float   oldMatrix[16];

	// check do we have something do draw
	if (!shownick.time || shownick.time + bound(0.1, scr_shownick_time.value, 3) < r_refdef2.time)
		return;

	// loc is unused
	maxloc = 0;

	// limit name length
	maxname = 999;
	maxname = bound(0, maxname, scr_shownick_name_width.integer);

	GL_TextureEnvMode(GL_MODULATE);
	GL_Color4f(1, 1, 1, 1);
	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);

	if (scale != 1)
	{
		GL_PushMatrix(GL_PROJECTION, oldMatrix);
		GL_Scale(GL_PROJECTION, scale, scale, 1);
	}

	y = vid.height*0.6/scale + scr_shownick_y.value;

	// this does't draw anything, just calculate width
	w = SCR_Draw_TeamInfoPlayer(&shownick, 0, 0, maxname, maxloc, true, true);
	h = 1;

	x = (scr_shownick_align_right ? (vid.width/scale - w * FONTWIDTH) - FONTWIDTH : FONTWIDTH);
	x += scr_shownick_x.value;

	// draw frame
	col = scr_shownick_frame_color.color;

	Draw_AlphaRectangleRGB(x, y, w * FONTWIDTH, h * FONTWIDTH, 0, true, RGBAVECT_TO_COLOR(col));

	// draw shownick
	SCR_Draw_TeamInfoPlayer(&shownick, x, y, maxname, maxloc, false, true);

	if (scale != 1) {
		GL_PopMatrix(GL_PROJECTION, oldMatrix);
	}

	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
	GL_TextureEnvMode(GL_REPLACE);
	GL_Color4f(1, 1, 1, 1);
}

/********************************* TILE CLEAR *********************************/

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
	if (IN_QuakeMouseCursorRequired())
	{
		int x_coord = (int)scr_pointer_state.x;
		int y_coord = (int)scr_pointer_state.y;

		if (scr_cursor && scr_cursor->texnum)
		{
			Draw_SAlphaPic(x_coord, y_coord, scr_cursor, scr_cursor_alpha.value, scale);

			if (scr_cursor_icon && scr_cursor_icon->texnum)
			{
				Draw_SAlphaPic(x_coord + scr_cursor_iconoffset_x.value, y_coord + scr_cursor_iconoffset_y.value, scr_cursor_icon, scr_cursor_alpha.value, scale);
			}
		}
		else
		{
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
void SCR_DrawMultiviewIndividualElements (void)
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

	GL_EnterRegion(__FUNCTION__);
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
			if (cl.intermission == 1)
			{
				Sbar_IntermissionOverlay ();
				if (!scr_notifyalways.integer)
				{
					Con_ClearNotify ();
				}
			} 
			else if (cl.intermission == 2)
			{
				Sbar_FinaleOverlay ();
				SCR_CheckDrawCenterString ();
				if (!scr_notifyalways.integer)
				{
					Con_ClearNotify ();
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
						//SCR_Draw_WeaponStats();

						SCR_Draw_ShowNick();

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

					// VULT STATS
					SCR_DrawAMFstats();

					// VULT DISPLAY KILLS
					if (amf_tracker_frags.value || amf_tracker_flags.value || amf_tracker_streaks.value )
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
	GL_LeaveRegion();
}

/******************************* UPDATE SCREEN *******************************/

static void SCR_RenderFrameEnd(void)
{
	extern double render_frame_end;
	render_frame_end = Sys_DoubleTime();
}

qbool SCR_UpdateScreenPrePlayerView (void)
{
	extern qbool Minimized;
	static int oldfovmode = 0;

	if (!scr_initialized) {
		SCR_RenderFrameEnd();
		return false;
	}

	if (scr_skipupdate || block_drawing) {
		SCR_RenderFrameEnd();
		return false;
	}

	if (scr_disabled_for_loading) {
		if (cls.realtime - scr_disabled_time > 20) {
			scr_disabled_for_loading = false;
		}
		else {
			SCR_RenderFrameEnd();
			return false;
		}
	}
	// Don't suck up any cpu if minimized.

	if (Minimized) {
		SCR_RenderFrameEnd();
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

	if (vid.recalc_refdef)
		SCR_CalcRefdef ();

	if ((v_contrast.value > 1 && !vid_hwgamma_enabled) || gl_clear.value)
		Sbar_Changed ();

	return true;
}

void R_SetupFrame(void);
void R_CreateWorldTextureChains(void);
qbool V_PreRenderView(void);

void SCR_UpdateScreenPlayerView (int flags)
{
	qbool renderView = false;

	GL_EnterRegion(__FUNCTION__);
	if (flags & UPDATESCREEN_MULTIVIEW) {
		SCR_CalcRefdef();
	}

	SCR_SetUpToDrawConsole ();

	SCR_SetupCI ();

	renderView = V_PreRenderView();

	if (renderView) {
		GL_EnterRegion("WorldTextureChains");
		R_SetupFrame ();
		R_CreateWorldTextureChains();
		GL_LeaveRegion();
	}

	// Do 3D refresh drawing, and then update the screen.
	GL_BeginRendering(&glx, &gly, &glwidth, &glheight);

	if (renderView) {
		V_RenderView();
	}

	if (!GL_ShadersSupported() && (flags & UPDATESCREEN_POSTPROCESS)) {
		R_RenderPostProcess();
	}

	GL_Set2D ();

	R_PolyBlend ();

	// draw any areas not covered by the refresh
	SCR_TileClear ();

	if (flags & UPDATESCREEN_MULTIVIEW) {
		SCR_DrawMultiviewIndividualElements ();
	}
	GL_LeaveRegion();
}

void SCR_HUD_WeaponStats(hud_t* hud);

// Drawing new HUD items in old HUD mode - eventually move everything across
static void SCR_DrawNewHudElements(void)
{
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

void SCR_UpdateScreenPostPlayerView (void)
{
	extern qbool  sb_showscores, sb_showteamscores;
	extern cvar_t scr_menudrawhud;

	if (scr_newHud.value != 1) {
		SCR_DrawNewHudElements();
	}

	SCR_DrawElements();

	//Draw_TextCacheFlush();
	if (GL_ShadersSupported()) {
		GLM_FlushImageDraw();
	}
	
	R_BrightenScreen();

	if (!GL_ShadersSupported()) {
		V_UpdatePalette();
	}

	if (SCR_TakingAutoScreenshot ()) {
		SCR_CheckAutoScreenshot ();
	}

	SCR_RenderFrameEnd();

	GL_EndRendering ();
}

// This is called every frame, and can also be called explicitly to flush text to the screen.
// WARNING: be very careful calling this from elsewhere, because the refresh needs almost the entire 256k of stack space!
qbool SCR_UpdateScreen(void)
{
	if (!SCR_UpdateScreenPrePlayerView()) {
		return false;
	}

	SCR_UpdateScreenPlayerView(UPDATESCREEN_POSTPROCESS);

	SCR_UpdateScreenPostPlayerView();

	return true;
}

void SCR_UpdateWholeScreen (void) {
	scr_fullupdate = 0;
	SCR_UpdateScreen ();
}

/******************************** SCREENSHOTS ********************************/

#define SSHOT_FAILED		-1
#define SSHOT_FAILED_QUIET	-2		//failed but don't print an error message
#define SSHOT_SUCCESS		0

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

	if (!strcasecmp(ext, "tga"))
		return IMAGE_TGA;

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
		return IMAGE_TGA;
}

static char *Sshot_SshotDirectory(void) {
	static char dir[MAX_PATH];

	strlcpy(dir, FS_LegacyDir(scr_sshot_dir.string), sizeof(dir));
	return dir;
}

#ifdef X11_GAMMA_WORKAROUND
unsigned short ramps[3][4096];
#else
unsigned short  ramps[3][256];
#endif

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

int SCR_Screenshot(char *name)
{
	scr_sshot_target_t* target_params = Q_malloc(sizeof(scr_sshot_target_t));

	// name is fullpath now
	//	name = (*name == '/') ? name + 1 : name;
	target_params->format = SShot_FormatForName(name);
	strlcpy(target_params->fileName, name, sizeof(target_params->fileName));
	COM_ForceExtension(target_params->fileName, SShot_ExtForFormat(target_params->format));
	target_params->width = glwidth;
	target_params->height = glheight;

	target_params->buffer = Movie_TempBuffer(glwidth, glheight);
	if (!target_params->buffer) {
		target_params->buffer = Q_malloc(glwidth * glheight * 3);
		target_params->freeMemory = true;
	}
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, target_params->buffer);

	if (Movie_BackgroundCapture(target_params)) {
		return SSHOT_SUCCESS;
	}

	return SCR_ScreenshotWrite(target_params);
}

int SCR_ScreenshotWrite(scr_sshot_target_t* target_params)
{
	int i, temp;
	int success = SSHOT_FAILED;
	int format = target_params->format;
	byte* buffer = target_params->buffer;
	char* name = target_params->fileName;
	int buffersize = target_params->width * target_params->height * 3;

#ifdef WITH_PNG
	if (format == IMAGE_PNG) {
#ifndef WITH_PNG_STATIC
		if (QLib_isModuleLoaded(qlib_libpng)) {
#endif
			applyHWGamma(buffer, buffersize);
			success = Image_WritePNG(
				name, image_png_compression_level.value,
				buffer + buffersize - 3 * glwidth, -glwidth, glheight
			) ? SSHOT_SUCCESS : SSHOT_FAILED;
#ifndef WITH_PNG_STATIC
		}
		else {
			if (!Movie_IsCapturing()) {
				Com_Printf("Can't take a PNG screenshot without libpng.");
				if (SShot_FormatForName("noext") == IMAGE_PNG) {
					Com_Printf(" Try changing \"%s\" to another image format.", scr_sshot_format.name);
				}
				Com_Printf("\n");
			}
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
			success = Image_WriteJPEG(
				name, image_jpeg_quality_level.value,
				buffer + buffersize - 3 * glwidth, -glwidth, glheight
			) ? SSHOT_SUCCESS : SSHOT_FAILED;
#ifndef WITH_JPEG_STATIC
		}
		else {
			if (!Movie_IsCapturing()) {
				Com_Printf("Can't take a JPEG screenshot without libjpeg.");
				if (SShot_FormatForName("noext") == IMAGE_JPEG) {
					Com_Printf(" Try changing \"%s\" to another image format.", scr_sshot_format.name);
				}
				Com_Printf("\n");
			}
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
		success = Image_WriteTGA(name, buffer, glwidth, glheight) ? SSHOT_SUCCESS : SSHOT_FAILED;
	}

	if (target_params->freeMemory) {
		Q_free(target_params->buffer);
	}
	Q_free(target_params);
	return success;
}

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
	int width, height;
	byte *base, *pixels;

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

static void SCR_CheckAutoScreenshot(void) {
	char *filename, savedname[MAX_PATH], *sshot_dir, *fullsavedname, *ext;
	char *exts[5] = {"pcx", "tga", "png", "jpg", NULL};
	int num;

	if (!SCR_TakingAutoScreenshot() || --scr_autosshot_countdown)
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

	glFinish();

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
	if (Movie_IsCapturingAVI())
	{
		int size = 0;
		// Capturing a movie.
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
	Cvar_Register (&scr_newHud);
	Cvar_Register (&scr_newHudClear);
	Cvar_ResetCurrentGroup();

	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register (&scr_consize);
	Cvar_Register (&scr_conspeed);
	Cvar_Register (&scr_printspeed);

	Cvar_SetCurrentGroup(CVAR_GROUP_OPENGL);
	Cvar_Register (&gl_triplebuffer);

	Cvar_SetCurrentGroup(CVAR_GROUP_EYECANDY);
	Cvar_Register (&r_chaticons_alpha);

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
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

	Cvar_Register (&show_velocity_3d);
	Cvar_Register (&show_velocity_3d_offset_forward);
	Cvar_Register (&show_velocity_3d_offset_down);

	Cvar_Register (&show_fps);
	Cvar_Register (&show_fps_x);
	Cvar_Register (&show_fps_y);

	Cvar_Register (&scr_autoid);
	Cvar_Register (&scr_autoid_weapons);
	Cvar_Register (&scr_autoid_namelength);
	Cvar_Register (&scr_autoid_barlength);
	Cvar_Register (&scr_autoid_weaponicon);
	Cvar_Register (&scr_autoid_scale);
	Cvar_Register (&scr_autoid_healthbar_bg_color);
	Cvar_Register (&scr_autoid_healthbar_normal_color);
	Cvar_Register (&scr_autoid_healthbar_mega_color);
	Cvar_Register (&scr_autoid_healthbar_two_mega_color);
	Cvar_Register (&scr_autoid_healthbar_unnatural_color);

	Cvar_Register (&scr_autoid_armorbar_green_armor);
	Cvar_Register (&scr_autoid_armorbar_yellow_armor);
	Cvar_Register (&scr_autoid_armorbar_red_armor);

	Cvar_Register (&scr_coloredfrags);
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
	Cvar_Register (&scr_teaminfo_powerup_style);
	Cvar_Register (&scr_teaminfo_weapon_style);
	Cvar_Register (&scr_teaminfo_show_enemies);
	Cvar_Register (&scr_teaminfo_show_self);
	Cvar_Register (&scr_teaminfo);
	Cvar_Register (&scr_shownick_order);
	Cvar_Register (&scr_shownick_frame_color);
	Cvar_Register (&scr_shownick_scale);
	Cvar_Register (&scr_shownick_y);
	Cvar_Register (&scr_shownick_x);
	Cvar_Register (&scr_shownick_name_width);
	Cvar_Register (&scr_shownick_time);

	Cmd_AddCommand("calc_fov", tmp_calc_fov);

	// QW 262 HUD
	Cvar_Register (&cl_hud);

	Cvar_Register (&scr_tracking);
	Cvar_Register (&scr_spectatorMessage);
	Cvar_Register (&scr_showcrosshair);
	Cvar_Register (&scr_notifyalways);
	Cvar_ResetCurrentGroup();

	Cvar_SetCurrentGroup(CVAR_GROUP_MENU);
	Cvar_Register (&scr_cursor_scale);
	Cvar_Register (&scr_cursor_iconoffset_x);
	Cvar_Register (&scr_cursor_iconoffset_y);
	Cvar_Register (&scr_cursor_alpha);
	Cvar_ResetCurrentGroup();

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

	WeaponStats_CommandInit();

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

