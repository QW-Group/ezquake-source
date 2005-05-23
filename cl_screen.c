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

*/

#include "quakedef.h"
#include "keys.h"
#include "image.h"
#include "menu.h"
#include "sbar.h"
#include "cl_screen.h"
#include <time.h>

#include "teamplay.h"
#include "utils.h"
#include "rulesets.h"
#include "modules.h"

#ifdef GLQUAKE
#include "gl_local.h"
#include "vx_stuff.h"
#else
#include "r_local.h"
#endif

#include "hud.h" // HUD -> hexum
#include "hud_common.h"

#include "mvd_utils.h"

#ifdef _WIN32
#include "movie.h"	//joe: capturing to avi
#include "movie_avi.h"	//
#endif

#ifdef GLQUAKE
int				glx, gly, glwidth, glheight;
#endif

#ifdef GLQUAKE
#define			DEFAULT_SSHOT_FORMAT		"tga"
#else
#define			DEFAULT_SSHOT_FORMAT		"pcx"
#endif



extern qboolean TP_LoadLocFile (char *path, qboolean quiet);
extern char *TP_LocationName(vec3_t location);
extern char *Weapon_NumToString(int num);

char *COM_FileExtension (char *in);


extern byte	current_pal[768];	// Tonik

extern cvar_t	scr_newHud;		// HUD -> hexum


// only the refresh window will be updated unless these variables are flagged 
int				scr_copytop;
int				scr_copyeverything;

float			scr_con_current;
float			scr_conlines;           // lines of console to display

float			oldscreensize, oldfov, oldsbar;
cvar_t			scr_viewsize = {"viewsize", "100", CVAR_ARCHIVE};
cvar_t			scr_fov = {"fov", "90", CVAR_ARCHIVE};	// 10 - 140
cvar_t			scr_consize = {"scr_consize", "0.75"};
cvar_t			scr_conspeed = {"scr_conspeed", "1000"};
cvar_t			scr_centertime = {"scr_centertime", "2"};
cvar_t		    scr_centershift = {"scr_centershift", "0"}; // BorisU
cvar_t			scr_showram = {"showram", "1"};
cvar_t			scr_showturtle = {"showturtle", "0"};
cvar_t			scr_showpause = {"showpause", "1"};
cvar_t			scr_printspeed = {"scr_printspeed", "8"};
qboolean OnChange_scr_allowsnap(cvar_t *, char *);
cvar_t			scr_allowsnap = {"scr_allowsnap", "1", 0, OnChange_scr_allowsnap};

cvar_t			scr_clock = {"cl_clock", "0"};
cvar_t			scr_clock_x = {"cl_clock_x", "0"};
cvar_t			scr_clock_y = {"cl_clock_y", "-1"};

cvar_t			scr_gameclock = {"cl_gameclock", "0"};
cvar_t			scr_gameclock_x = {"cl_gameclock_x", "0"};
cvar_t			scr_gameclock_y = {"cl_gameclock_y", "-3"};

cvar_t			scr_democlock = {"cl_democlock", "0"};
cvar_t			scr_democlock_x = {"cl_democlock_x", "0"};
cvar_t			scr_democlock_y = {"cl_democlock_y", "-2"};

cvar_t			show_speed = {"show_speed", "0"};
cvar_t			show_speed_x = {"show_speed_x", "-1"};
cvar_t			show_speed_y = {"show_speed_y", "1"};

cvar_t			show_fps = {"show_fps", "0"};
cvar_t			show_fps_x = {"show_fps_x", "-5"};
cvar_t			show_fps_y = {"show_fps_y", "-1"};

cvar_t			scr_sshot_format		= {"sshot_format", DEFAULT_SSHOT_FORMAT};
cvar_t			scr_sshot_dir			= {"sshot_dir", ""};

#ifdef GLQUAKE
cvar_t			gl_triplebuffer = {"gl_triplebuffer", "1", CVAR_ARCHIVE};
#endif

cvar_t			scr_autoid		= {"scr_autoid", "0"};
cvar_t			scr_coloredText = {"scr_coloredText", "1"};

// START shaman RFE 1022309
cvar_t			scr_tracking			= {"scr_tracking", ""};
cvar_t			scr_spectatorMessage	= {"scr_spectatorMessage", "1"};
// END shaman RFE 1022309




qboolean		scr_initialized;                // ready to draw

mpic_t			*scr_ram;
mpic_t			*scr_net;
mpic_t			*scr_turtle;

int				scr_fullupdate;

int				clearconsole;
int				clearnotify;

viddef_t		vid;                            // global video state

vrect_t			scr_vrect;

qboolean		scr_skipupdate;

qboolean		scr_drawloading;
qboolean		scr_disabled_for_loading;
float			scr_disabled_time;

qboolean		block_drawing;


static int scr_autosshot_countdown = 0;
static int scr_mvsshot_countdown = 0;
char auto_matchname[2 * MAX_OSPATH];

static void SCR_CheckAutoScreenshot(void);
static void SCR_CheckMVScreenshot(void);
void SCR_DrawStatusMultiview(void);


qboolean OnChange_scr_allowsnap(cvar_t *var, char *s) {
	return (cls.state >= ca_connected && cbuf_current == &cbuf_svc);
}




/**************************** CENTER PRINTING ********************************/

char		scr_centerstring[1024];
float		scr_centertime_start;   // for slow victory printing
float		scr_centertime_off;
int			scr_center_lines;
int			scr_erase_lines;
int			scr_erase_center;

//Called for important messages that should stay in the center of the screen for a few moments
void SCR_CenterPrint (char *str) {
	Q_strncpyz (scr_centerstring, str, sizeof(scr_centerstring));
	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = cl.time;

	// count the number of lines for centering
	scr_center_lines = 1;
	while (*str) {
		if (*str == '\n')
			scr_center_lines++;
		str++;
	}
}

void SCR_DrawCenterString (void) {
	char *start;
	int l, j, x, y, remaining;

	// the finale prints the characters one at a time
	remaining = cl.intermission ? scr_printspeed.value * (cl.time - scr_centertime_start) : 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	y = (scr_center_lines <= 4) ? vid.height * 0.35 : 48 + scr_centershift.value*8;

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
	if (key_dest != key_game)
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

static float CalcFov (float fov_x, float width, float height) {
	float x;

	if (fov_x < 1 || fov_x > 179)
		Sys_Error ("CalcFov: Bad fov (%f)", fov_x);

	x = width / tan(fov_x / 360 * M_PI);
	return atan (height / x) * 360 / M_PI;
}

//Must be called whenever vid changes
static void SCR_CalcRefdef (void) {
	float  size;
#ifdef GLQUAKE
	int h;
	qboolean full = false;
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

	// bound field of view
	if (scr_fov.value < 10)
		Cvar_Set (&scr_fov, "10");
	if (scr_fov.value > 140)
		Cvar_Set (&scr_fov, "140");

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
	if (cls.demoplayback)
		return;

	Draw_Pic (scr_vrect.x + 64, scr_vrect.y, scr_net);
}



void SCR_DrawFPS (void) {
	int x, y;
	char str[80];
	extern double	lastfps;
	extern int		fps_count;

	if (!show_fps.value || scr_newHud.value == 1) // HUD -> hexum - newHud has its own fps
		return;

	if (cl_multiview.value && cls.mvdplayback)
		snprintf(str, sizeof(str), "%3.1f", (lastfps + 0.05)/nNumViews);
	else
		snprintf(str, sizeof(str), "%3.1f",  lastfps + 0.05);

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

	if (!show_speed.value)
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
		Q_snprintfz(str, sizeof(str), "%3d%s", (int) display_speed, show_speed.value == 2 ? " SPD" : "");
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

void SCR_DrawClock (void) {
	int x, y;
	time_t t;
	struct tm *ptm;
	char str[80];

	if (!scr_clock.value)
		return;

	if (scr_clock.value == 2) {
		time (&t);
		if ((ptm = localtime (&t))) {
			strftime (str, sizeof(str) - 1, "%H:%M:%S", ptm);
		} else {
			strcpy (str, "#bad date#");
		}
	} else {
		float time = (cl.servertime_works) ? cl.servertime : cls.realtime;
		Q_strncpyz (str, SecondsToHourString((int) time), sizeof(str));
	}

	x = ELEMENT_X_COORD(scr_clock);
	y = ELEMENT_Y_COORD(scr_clock);
	Draw_String (x, y, str);
}

void SCR_DrawGameClock (void) {
	int x, y;
	char str[80], *s;
	float timelimit;

	if (!scr_gameclock.value)
		return;

	if (scr_gameclock.value == 2 || scr_gameclock.value == 4) 
		timelimit = 60 * Q_atof(Info_ValueForKey(cl.serverinfo, "timelimit"));
	else
		timelimit = 0;

	if (cl.countdown || cl.standby)
		Q_strncpyz (str, SecondsToHourString(timelimit), sizeof(str));
	else
		Q_strncpyz (str, SecondsToHourString((int) abs(timelimit - cl.gametime)), sizeof(str));

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

	if (!cls.demoplayback || !scr_democlock.value)
		return;

	if (scr_democlock.value == 2)
		Q_strncpyz (str, SecondsToHourString((int) (cls.demotime)), sizeof(str));
	else
		Q_strncpyz (str, SecondsToHourString((int) (cls.demotime - demostarttime)), sizeof(str));

	x = ELEMENT_X_COORD(scr_democlock);
	y = ELEMENT_Y_COORD(scr_democlock);
	Draw_String (x, y, str);
}

void SCR_DrawPause (void) {
	mpic_t *pic;

	if (!scr_showpause.value)               // turn off for screenshots
		return;

	if (!cl.paused)
		return;

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
		if (!(cl_multiview.value && cls.mvdplayback)) // oppmv 040904
			Draw_TileClear (0, (int) scr_con_current, vid.width, vid.height - (int) scr_con_current);
#endif
		Sbar_Changed ();
	}
	else if (clearnotify++ < vid.numpages)
	{
#ifndef GLQUAKE
		scr_copytop = 1;

		if (!(cl_multiview.value && cls.mvdplayback))
			Draw_TileClear (0, 0, vid.width, con_notifylines);
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
		if (key_dest == key_game || key_dest == key_message)
			Con_DrawNotify ();      // only draw notify in game
	}
}

/*********************************** AUTOID ***********************************/

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
	glGetIntegerv(GL_VIEWPORT, view);

	if (cl.spectator)
		tracknum = Cam_TrackNum();

	
	VectorCopy(vpn, visitem.forward);
	VectorCopy(vright, visitem.right);
	VectorCopy(vup, visitem.up);
	VectorCopy(r_origin, visitem.vieworg);

	state = cl.frames[cl.parsecount & UPDATE_MASK].playerstate;
	info = cl.players;

	for (j = 0; j < MAX_CLIENTS; j++, info++, state++) {
		if (state->messagenum != cl.parsecount || j == cl.playernum || j == tracknum || info->spectator)
			continue;

		if (
			(state->modelindex == cl_modelindices[mi_player] && ISDEAD(state->frame)) ||
			state->modelindex == cl_modelindices[mi_h_player]
		)
			continue;

		if (R_CullSphere(state->origin, 0))
			continue;

		
		VectorCopy (state->origin, visitem.entorg);
		visitem.entorg[2] += 27;
		VectorSubtract (visitem.entorg, visitem.vieworg, visitem.dir);
		visitem.dist = DotProduct (visitem.dir, visitem.forward);
		visitem.radius = 25;

		if (!TP_IsItemVisible(&visitem))
			continue;

		id = &autoids[autoid_count];
		id->player = info;
		origin = state->origin;
		if (qglProject(origin[0], origin[1], origin[2] + 28, model, project, view, &id->x, &id->y, &winz))
			autoid_count++;
	}
}

void SCR_DrawAutoID (void) {
	int i, x, y;

	if (!scr_autoid.value || (!cls.demoplayback && !cl.spectator))
		return;

	for (i = 0; i < autoid_count; i++) {
		x =  autoids[i].x * vid.width / glwidth;
		y =  (glheight - autoids[i].y) * vid.height / glheight;
		Draw_String(x - strlen(autoids[i].player->name) * 4, y - 8, autoids[i].player->name);
	}
}

#endif

/********************************* TILE CLEAR *********************************/

#ifdef GLQUAKE

void SCR_TileClear (void) {
	if (cls.state != ca_active && cl.intermission) {
		Draw_TileClear (0, 0, vid.width, vid.height);
		return;
	}

	if (r_refdef.vrect.x > 0) {
		// left
		Draw_TileClear (0, 0, r_refdef.vrect.x, vid.height - sb_lines);
		// right
		Draw_TileClear (r_refdef.vrect.x + r_refdef.vrect.width, 0, 
			vid.width - (r_refdef.vrect.x + r_refdef.vrect.width), vid.height - sb_lines);
	}
	if (r_refdef.vrect.y > 0) {
		// top
		Draw_TileClear (r_refdef.vrect.x, 0, r_refdef.vrect.width, r_refdef.vrect.y);
	}
	if (r_refdef.vrect.y + r_refdef.vrect.height < vid.height - sb_lines) {
		// bottom
		Draw_TileClear (r_refdef.vrect.x,
			r_refdef.vrect.y + r_refdef.vrect.height, 
			r_refdef.vrect.width, 
			vid.height - sb_lines - (r_refdef.vrect.height + r_refdef.vrect.y));
	}
}

#else

void SCR_TileClear (void) {
	if (scr_fullupdate++ < vid.numpages) {	// clear the entire screen
		scr_copyeverything = 1;
		Draw_TileClear (0, 0, vid.width, vid.height);
		Sbar_Changed ();
	} else {
		char str[11] = "xxxxxxxxxx";		
		if (scr_viewsize.value < 100) {
			// clear background for counters
			if (show_speed.value)
				Draw_TileClear(ELEMENT_X_COORD(show_speed), ELEMENT_Y_COORD(show_speed), 10 * 8, 8);
			if (show_fps.value)
				Draw_TileClear(ELEMENT_X_COORD(show_fps), ELEMENT_Y_COORD(show_fps), 10 * 8, 8);
			if (scr_clock.value)
				Draw_TileClear(ELEMENT_X_COORD(scr_clock), ELEMENT_Y_COORD(scr_clock), 10 * 8, 8);
			if (scr_democlock.value)
				Draw_TileClear(ELEMENT_X_COORD(scr_clock), ELEMENT_Y_COORD(scr_clock), 10 * 8, 8);
				
		}
	}
}

#endif

void SCR_DrawElements(void) {
	if (scr_drawloading) {
		SCR_DrawLoading ();
		Sbar_Draw ();
		HUD_Draw ();		// HUD -> hexum
	} else {
		if (cl.intermission == 1) {
			Sbar_IntermissionOverlay ();
			Con_ClearNotify ();
		} else if (cl.intermission == 2) {
			Sbar_FinaleOverlay ();
			SCR_CheckDrawCenterString ();
			Con_ClearNotify ();
		}

		if (cls.state == ca_active) {
			SCR_DrawRam ();
			SCR_DrawNet ();
			SCR_DrawTurtle ();
			SCR_DrawPause ();
#ifdef GLQUAKE
			SCR_DrawAutoID ();
#endif
			if (!cl.intermission) {
				if (key_dest != key_menu)
					Draw_Crosshair ();
				SCR_CheckDrawCenterString ();
				SCR_DrawSpeed ();
				SCR_DrawClock ();
				SCR_DrawGameClock ();
				SCR_DrawDemoClock ();
				SCR_DrawFPS ();
				MVD_Screen ();

#ifdef GLQUAKE
				//VULT STATS
				SCR_DrawAMFstats();
				//VULT DISPLAY KILLS
				if (amf_tracker_frags.value || amf_tracker_flags.value || amf_tracker_streaks.value )
					VX_TrackerThink();
#endif

				if (cl_multiview.value && cls.mvdplayback)
					SCR_DrawStatusMultiview();

				Sbar_Draw();
				HUD_Draw();		// HUD -> hexum
			}
		}

		if (!scr_autosshot_countdown) {
			SCR_DrawConsole ();	
			M_Draw ();
		}
	}
}

/******************************* UPDATE SCREEN *******************************/

#ifdef GLQUAKE

//This is called every frame, and can also be called explicitly to flush text to the screen.
//WARNING: be very careful calling this from elsewhere, because the refresh needs almost the entire 256k of stack space!
void SCR_UpdateScreen (void) {
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
	{	// don't suck up any cpu if minimized
		extern int Minimized;

		if (Minimized)
			return;
	}
#endif

	vid.numpages = 2 + gl_triplebuffer.value;

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

	if (vid.recalc_refdef)
		SCR_CalcRefdef ();

	if ((v_contrast.value > 1 && !vid_hwgamma_enabled) || gl_clear.value)
		Sbar_Changed ();

	if (cl_multiview.value && cls.mvdplayback)
		SCR_CalcRefdef();
 
	// do 3D refresh drawing, and then update the screen
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);

	SCR_SetUpToDrawConsole ();

	V_RenderView ();

	SCR_SetupAutoID ();	

	GL_Set2D ();

	R_PolyBlend ();

	// draw any areas not covered by the refresh
	SCR_TileClear ();

	if (r_netgraph.value && scr_newHud.value != 1) { // HUD -> hexum
		// FIXME: ugly hack :(
		float temp = hud_netgraph->show->value;

		Cvar_SetValue(hud_netgraph->show, 1);
		SCR_HUD_Netgraph(hud_netgraph);
		Cvar_SetValue(hud_netgraph->show, temp);
	}

	if (r_netstats.value && scr_newHud.value != 1) {
		// FIXME: ugly hack :(
		float temp = hud_netstats->show->value;

		Cvar_SetValue(hud_netstats->show, 1);
		SCR_HUD_DrawNetStats(hud_netstats);
		Cvar_SetValue(hud_netstats->show, temp);
	}

	SCR_DrawElements();

	R_BrightenScreen ();

	V_UpdatePalette ();

	SCR_CheckAutoScreenshot();

	if (cls.mvdplayback && cl_multiview.value && cl_mvinset.value)
		SCR_CheckMVScreenshot();

	GL_EndRendering ();
}

#else

void SCR_UpdateScreen (void) {
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

	 // oppymv 040904 - dont tile over the first view
	if (!(cl_multiview.value && cls.mvdplayback) ||
		cl_multiview.value && cls.mvdplayback && CURRVIEW == 2)
		SCR_TileClear ();

	SCR_SetUpToDrawConsole ();


	if (!(cl_multiview.value && cls.mvdplayback)) // oppymv 010904
		SCR_EraseCenterString ();


	// oppymv 010904
	if (cl_multiview.value && cls.mvdplayback && CURRVIEW==1)
		SCR_CalcRefdef();

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

	SCR_CheckAutoScreenshot();

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
	case IMAGE_PCX: return ".pcx";
	case IMAGE_TGA: return ".tga";
	case IMAGE_JPEG: return ".jpg";
	case IMAGE_PNG: return ".png";
	}
	assert(!"SShot_ExtForFormat: unknown format");
	return "err";
}

static image_format_t SShot_FormatForName(char *name) {
	char *ext;
	
	ext = COM_FileExtension(name);

#ifdef GLQUAKE
	if (!Q_strcasecmp(ext, "tga"))
		return IMAGE_TGA;
#else
	if (!Q_strcasecmp(ext, "pcx"))
		return IMAGE_PCX;
#endif

#ifdef WITH_PNG
	else if (!Q_strcasecmp(ext, "png"))
		return IMAGE_PNG;
#endif

#ifdef WITH_JPEG
	else if (!Q_strcasecmp(ext, "jpg"))
		return IMAGE_JPEG;
#endif

#ifdef WITH_PNG
	else if (!Q_strcasecmp(scr_sshot_format.string, "png"))
		return IMAGE_PNG;
#endif

#ifdef WITH_JPEG
	else if (!Q_strcasecmp(scr_sshot_format.string, "jpg") || !Q_strcasecmp(scr_sshot_format.string, "jpeg"))
		return IMAGE_JPEG;
#endif

	else
#ifdef GLQUAKE
		return IMAGE_TGA;
#else
		return IMAGE_PCX;
#endif
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

	name = (*name == '/') ? name + 1 : name;
	format = SShot_FormatForName(name);
	COM_ForceExtension (name, SShot_ExtForFormat(format));
	buffersize = glwidth * glheight * 3;

	buffer = Q_Malloc (buffersize);
	glReadPixels (glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer); 

#ifdef WITH_PNG
	if (format == IMAGE_PNG) {
		if (QLib_isModuleLoaded(qlib_libpng)) {
			applyHWGamma(buffer, buffersize);
			success = Image_WritePNG(name, image_png_compression_level.value,
					buffer + buffersize - 3 * glwidth, -glwidth, glheight)
				? SSHOT_SUCCESS : SSHOT_FAILED;
		} else {
			Com_Printf("Can't take a PNG screenshot without libpng.");
			if (SShot_FormatForName("noext") == IMAGE_PNG)
				Com_Printf(" Try changing \"%s\" to another image format.", scr_sshot_format.name);
			Com_Printf("\n");
			success = SSHOT_FAILED_QUIET;
		}
	}
#endif

#ifdef WITH_JPEG
	if (format == IMAGE_JPEG) {
		if (QLib_isModuleLoaded(qlib_libjpeg)) {
			applyHWGamma(buffer, buffersize);
			success = Image_WriteJPEG(name, image_jpeg_quality_level.value,
					buffer + buffersize - 3 * glwidth, -glwidth, glheight)
				? SSHOT_SUCCESS : SSHOT_FAILED;;
		} else {
			Com_Printf("Can't take a JPEG screenshot without libjpeg.");
			if (SShot_FormatForName("noext") == IMAGE_JPEG)
				Com_Printf(" Try changing \"%s\" to another image format.", scr_sshot_format.name);
			Com_Printf("\n");
			success = SSHOT_FAILED_QUIET;
		}
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

	free(buffer);
	return success;
}

#else

int SCR_Screenshot(char *name) {
	int success;
	image_format_t format;

	name = (*name == '/') ? name + 1 : name;
	format = SShot_FormatForName(name);
	COM_ForceExtension (name, SShot_ExtForFormat(format));

	D_EnableBackBufferAccess ();	// enable direct drawing of console to back buffer

#ifdef WITH_PNG
	if (format == IMAGE_PNG) {
		if (QLib_isModuleLoaded(qlib_libpng)) {
			success = Image_WritePNGPLTE(name, image_png_compression_level.value,
					vid.buffer, vid.width, vid.height, vid.rowbytes, current_pal)
					? SSHOT_SUCCESS : SSHOT_FAILED;
		} else {
			Com_Printf("Can't take a PNG screenshot without libpng.");
			if (SShot_FormatForName("noext") == IMAGE_PNG)
				Com_Printf(" Try changing \"%s\" to another image format.", scr_sshot_format.name);
			Com_Printf("\n");
			success = SSHOT_FAILED_QUIET;
		}
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

void SCR_ScreenShot_f (void) {
	char name[MAX_OSPATH], ext[4], *filename, *sshot_dir;
	int i, success;
	FILE *f;

#ifdef GLQUAKE
	if (cls.mvdplayback && cl_multiview.value == 2 && cl_mvinset.value && CURRVIEW != 1) {
		scr_mvsshot_countdown = 1;
		return;
	}
#else
	// sorry braindead atm, this can be cleaned up someday
	if (cls.mvdplayback && cl_multiview.value && CURRVIEW != 1) {
		if (cl_multiview.value == 2)
			scr_mvsshot_countdown = 1;
		else if (cl_multiview.value == 3) {
			if (CURRVIEW == 2)
				scr_mvsshot_countdown = 2;
			else if (CURRVIEW == 3)
				scr_mvsshot_countdown = 1;
		} 
		else if (cl_multiview.value == 4) {
			if (CURRVIEW == 2)
				scr_mvsshot_countdown = 3;
			else if (CURRVIEW == 3)
				scr_mvsshot_countdown = 2;
			else if (CURRVIEW == 4)
				scr_mvsshot_countdown = 1;
		}
		return;
	}
#endif

	ext[0] = 0;
	sshot_dir = scr_sshot_dir.string[0] ? scr_sshot_dir.string : cls.gamedirfile;

	if (Cmd_Argc() == 2) {
		Q_strncpyz (name, Cmd_Argv(1), sizeof(name));
	} else if (Cmd_Argc() == 1) {
		// find a file name to save it to
#ifdef WITH_PNG
		if (!Q_strcasecmp(scr_sshot_format.string, "png"))
			Q_strncpyz(ext, "png", 4);
#endif
#ifdef WITH_JPEG
		if (!Q_strcasecmp(scr_sshot_format.string, "jpeg") || !Q_strcasecmp(scr_sshot_format.string, "jpg"))
			Q_strncpyz(ext, "jpg", 4);
#endif
		if (!ext[0])
			Q_strncpyz(ext, DEFAULT_SSHOT_FORMAT, 4);

		for (i = 0; i < 999; i++) {
			Q_snprintfz(name, sizeof(name), "ezquake%03i.%s", i, ext);
			if (!(f = fopen (va("%s/%s/%s", com_basedir, sshot_dir, name), "rb")))
				break;  // file doesn't exist
			fclose(f);
		}
		if (i == 1000) {
			Com_Printf ("Error: Cannot create more than 1000 screenshots\n");
			return;
		}
	} else {
		Com_Printf("Usage: %s [filename]\n", Cmd_Argv(0));
		return;
	}
	
	for (filename = name; *filename == '/' || *filename == '\\'; filename++)
		;

	success = SCR_Screenshot(va("%s/%s", sshot_dir, filename));
	if (success != SSHOT_FAILED_QUIET)
		Com_Printf ("%s %s\n", success == SSHOT_SUCCESS ? "Wrote" : "Couldn't write", name);
}

void SCR_RSShot_f (void) { 
	int success = SSHOT_FAILED;
	char *filename;
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

	filename = "ezquake/temp/__rsshot__";

#ifdef GLQUAKE

	width = 400; height = 300;
	base = Q_Malloc ((width * height + glwidth * glheight) * 3);
	pixels = base + glwidth * glheight * 3;

	glReadPixels (glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, base);
	Image_Resample (base, glwidth, glheight, pixels, width, height, 3, 0);
#ifdef WITH_JPEG
	if (QLib_isModuleLoaded(qlib_libjpeg)) {
		success = Image_WriteJPEG (filename, 70, pixels + 3 * width * (height - 1), -width, height)
			? SSHOT_SUCCESS : SSHOT_FAILED;
		goto sshot_taken;
	}
#endif
	success = Image_WriteTGA (filename, pixels, width, height)
		? SSHOT_SUCCESS : SSHOT_FAILED;
	goto sshot_taken;

sshot_taken:
	free(base);

#else		//GLQUAKE

	D_EnableBackBufferAccess ();

#ifdef WITH_PNG
	if (QLib_isModuleLoaded(qlib_libpng)) {
		success = Image_WritePNGPLTE(filename, 9, vid.buffer, vid.width, vid.height, vid.rowbytes, current_pal)
			? SSHOT_SUCCESS : SSHOT_FAILED;
		goto sshot_taken;
	}
#endif

	success = Image_WritePCX (filename, vid.buffer, vid.width, vid.height, vid.rowbytes, current_pal)
		? SSHOT_SUCCESS : SSHOT_FAILED;
	goto sshot_taken;

sshot_taken:

	D_DisableBackBufferAccess();

#endif		//GLQUAKE

	if (success == SSHOT_SUCCESS)
	{
		FILE	*f;
		byte	*screen_shot;
		int	size;
		if ((size = COM_FileOpenRead (va("%s/%s", com_basedir, filename), &f)) == -1)
		{
			Com_Printf ("Can't send screenshot to server: can't open file %s\n", filename);
		}
		else
		{
			screen_shot = Q_Malloc (size);
			if (fread(screen_shot, 1, size, f) == size)
			{
				Com_Printf ("Sending screenshot to server...\n");
				CL_StartUpload(screen_shot, size);
			}
			else
				Com_Printf ("Can't send screenshot to server: can't read file %s\n", filename);
			fclose(f);
			Q_Free(screen_shot);
		}
	}

	remove(va("%s/%s", com_basedir, filename));
}

static void SCR_CheckMVScreenshot(void) {

	if (!scr_mvsshot_countdown || --scr_mvsshot_countdown)
		return;

#ifdef GLQUAKE
	glFinish();
#endif

#ifdef GLQUAKE
	// only concerned with inset for gl
	if (cls.mvdplayback && cl_multiview.value == 2 && cl_mvinset.value && CURRVIEW == 1)
		SCR_ScreenShot_f();
#else
	if (cls.mvdplayback && cl_multiview.value && CURRVIEW == 1)
		SCR_ScreenShot_f();
#endif
}

static void SCR_CheckAutoScreenshot(void) {
	char *filename, savedname[2 * MAX_OSPATH], *sshot_dir, *fullsavedname, *ext;
	char *exts[5] = {"pcx", "tga", "png", "jpg", NULL};
	int num;

	if (!scr_autosshot_countdown || --scr_autosshot_countdown)
		return;


	if (!cl.intermission)
		return;

	for (filename = auto_matchname; *filename == '/' || *filename == '\\'; filename++)
		;

	sshot_dir = scr_sshot_dir.string[0] ? scr_sshot_dir.string : cls.gamedirfile;
	ext = SShot_ExtForFormat(SShot_FormatForName(filename));

	fullsavedname = va("%s/%s", sshot_dir, auto_matchname);
	if ((num = Util_Extend_Filename(fullsavedname, exts)) == -1) {
		Com_Printf("Error: no available filenames\n");
		return;
	}

	Q_snprintfz (savedname, sizeof(savedname), "%s_%03i%s", auto_matchname, num, ext);
	fullsavedname = va("%s/%s", sshot_dir, savedname);

#ifdef GLQUAKE
	glFinish();
#endif

	if ((SCR_Screenshot(fullsavedname)) == SSHOT_SUCCESS)
		Com_Printf("Match scoreboard saved to %s\n", savedname);
}

void SCR_AutoScreenshot(char *matchname) {
	if (cl.intermission == 1) {
		scr_autosshot_countdown = vid.numpages;
		Q_strncpyz(auto_matchname, matchname, sizeof(auto_matchname));
	}
}

//joe: capturing to avi
void SCR_Movieshot(char *name) {
#ifdef _WIN32
	if (movie_is_avi) {
#ifdef GLQUAKE
		int i, size = glwidth * glheight * 3;
		byte *buffer, temp;

		buffer = Q_Malloc (size);
		glReadPixels (glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer);
		applyHWGamma (buffer, size);

		for (i = 0; i < size; i += 3)
		{
			temp = buffer[i];
			buffer[i] = buffer[i+2];
			buffer[i+2] = temp;
		}
#else
		int i, j, rowp;
		byte *buffer, *p;

		buffer = Q_Malloc (vid.width * vid.height * 3);

		D_EnableBackBufferAccess ();

		p = buffer;
		for (i = vid.height - 1; i >= 0; i--) {
			rowp = i * vid.rowbytes;
			for (j = 0; j < vid.width; j++) {
				*p++ = current_pal[vid.buffer[rowp]*3+2];
				*p++ = current_pal[vid.buffer[rowp]*3+1];
				*p++ = current_pal[vid.buffer[rowp]*3+0];
				rowp++;
			}
		}

		D_DisableBackBufferAccess ();
#endif
		Capture_WriteVideo (buffer);

		free (buffer);
	} else {
		SCR_Screenshot (name);
	}
#else
	SCR_Screenshot (name);
#endif
}

/************************************ INIT ************************************/

void SCR_Init (void) {
	scr_ram = Draw_CacheWadPic ("ram");
	scr_net = Draw_CacheWadPic ("net");
	scr_turtle = Draw_CacheWadPic ("turtle");

	Cvar_SetCurrentGroup(CVAR_GROUP_VIEW);
	Cvar_Register (&scr_fov);
	Cvar_Register (&scr_viewsize);

	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register (&scr_consize);
	Cvar_Register (&scr_conspeed);
	Cvar_Register (&scr_printspeed);

#ifdef GLQUAKE
	Cvar_SetCurrentGroup(CVAR_GROUP_OPENGL);
	Cvar_Register (&gl_triplebuffer);
#endif

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register (&scr_showram);
	Cvar_Register (&scr_showturtle);
	Cvar_Register (&scr_showpause);
	Cvar_Register (&scr_centertime);
	Cvar_Register (&scr_centershift); // BorisU

	Cvar_Register (&scr_clock_x);
	Cvar_Register (&scr_clock_y);
	Cvar_Register (&scr_clock);

	Cvar_Register (&scr_gameclock_x);
	Cvar_Register (&scr_gameclock_y);
	Cvar_Register (&scr_gameclock);

	Cvar_Register (&scr_democlock_x);
	Cvar_Register (&scr_democlock_y);
	Cvar_Register (&scr_democlock);
	
	Cvar_Register (&show_speed);
	Cvar_Register (&show_speed_x);
	Cvar_Register (&show_speed_y);

	Cvar_Register (&show_fps);
	Cvar_Register (&show_fps_x);
	Cvar_Register (&show_fps_y);

	Cvar_Register (&scr_autoid);
	Cvar_Register (&scr_coloredText);

	// START shaman RFE 1022309
	Cvar_Register (&scr_tracking);
	Cvar_Register (&scr_spectatorMessage);
	// END shaman RFE 1022309

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREENSHOTS);
	Cvar_Register (&scr_allowsnap);
	Cvar_Register (&scr_sshot_format);
	Cvar_Register (&scr_sshot_dir);

	Cvar_ResetCurrentGroup();
	
	
	Cmd_AddCommand ("screenshot", SCR_ScreenShot_f);
	Cmd_AddCommand ("sizeup", SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown", SCR_SizeDown_f);
	
	scr_initialized = true;
}

void SCR_DrawStatusMultiview(void) {
	int xb, yb, xc, yc, xd, yd;
	char strng[80];
	char weapons[40];
	char w1,w2;
	char sAmmo[3];
	char pups[4];
	char armor;
	char name[16];
	byte c, c2;

	int i;

	if (!cl_multiview.value || !cls.mvdplayback)
		return;

	if (cl.stats[STAT_ACTIVEWEAPON] & IT_LIGHTNING || cl.stats[STAT_ACTIVEWEAPON] & IT_SUPER_LIGHTNING) {
		w1='l'; w2='g';
	} else if (cl.stats[STAT_ACTIVEWEAPON] & IT_ROCKET_LAUNCHER) {
		w1='r'; w2='l';
	} else if (cl.stats[STAT_ACTIVEWEAPON] & IT_GRENADE_LAUNCHER) {
		w1='g'; w2='l';
	} else if (cl.stats[STAT_ACTIVEWEAPON] & IT_SUPER_NAILGUN) {
		w1='s'; w2='n';
	} else if (cl.stats[STAT_ACTIVEWEAPON] & IT_NAILGUN) {
		w1='n'; w2='g';
	} else if (cl.stats[STAT_ACTIVEWEAPON] & IT_SUPER_SHOTGUN) {
		w1='s'; w2='s';
	} else if (cl.stats[STAT_ACTIVEWEAPON] & IT_SHOTGUN) {
		w1='s'; w2='g';
	} else if (cl.stats[STAT_ACTIVEWEAPON] & IT_AXE) {
		w1='a'; w2='x';
	}
	w1 |= 128; w2 |= 128;

	pups[0] = pups[1] = pups[2] = ' ';
	pups[3] = '\0';

	if (cl.stats[STAT_ITEMS] & IT_QUAD) {
		pups[0] = 'Q';
		pups[0] |= 128;
	}
	if (cl.stats[STAT_ITEMS] & IT_INVISIBILITY) {
		pups[1] = 'R';
		pups[1] |= 128;
	}
	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) {
		pups[2] = 'P';
		pups[2] |= 128;
	}

	strng[0]='\0';

	c = (byte) 128;
	c2 = (byte) 0;

	for (i=0;i<8;i++) {
		weapons[i]=' ';
		weapons[8] = '\0';
	}

	if (cl.stats[STAT_ITEMS] & IT_AXE)
		weapons[0] = '1' - '0' + 0x12;
	if (cl.stats[STAT_ITEMS] & IT_SHOTGUN)
		weapons[1] = '2' - '0' + 0x12;
	if (cl.stats[STAT_ITEMS] & IT_SUPER_SHOTGUN)
		weapons[2] = '3' - '0' + 0x12;
	if (cl.stats[STAT_ITEMS] & IT_NAILGUN)
		weapons[3] = '4' - '0' + 0x12;
	if (cl.stats[STAT_ITEMS] & IT_SUPER_NAILGUN)
		weapons[4] = '5' - '0' + 0x12;
	if (cl.stats[STAT_ITEMS] & IT_GRENADE_LAUNCHER)
		weapons[5] = '6' - '0' + 0x12;
	if (cl.stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER)
		weapons[6] = '7' - '0' + 0x12;
	if (cl.stats[STAT_ITEMS] & IT_SUPER_LIGHTNING || cl.stats[STAT_ITEMS] & IT_LIGHTNING)
		weapons[7] = '8' - '0' + 0x12;

	armor = ' ';
	if (cl.stats[STAT_ITEMS] & IT_ARMOR1) {
		armor='g';
		armor |= 128;
	}
	else if (cl.stats[STAT_ITEMS] & IT_ARMOR2) {
		armor='y';
		armor |= 128;
	}
	else if (cl.stats[STAT_ITEMS] & IT_ARMOR3) {
		armor='r';
		armor |= 128;
	}
	
	strcpy(name,cl.players[nPlayernum].name);

	if (strcmp(cl.players[nPlayernum].name,"") && !cl.players[nPlayernum].spectator) {
		if (cl.players[nPlayernum].stats[STAT_HEALTH] <= 0 && cl_multiview.value == 2 && cl_mvinset.value) { // mvinset and dead
			sprintf(sAmmo, "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			sprintf(strng, "%.5s   %s %c%c:%-3s",name,
										"dead   ",
										w1,w2,
										sAmmo);
		}
		else if (cl.players[nPlayernum].stats[STAT_HEALTH] <= 0 && vid.width <= 512) { // <= 512 and dead
			sprintf(sAmmo, "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			sprintf(strng, "%.2s  %s %c%c:%-3s",name,
										"dead   ",
										w1,w2,
										sAmmo);
		}
		else if (cl.players[nPlayernum].stats[STAT_HEALTH] <= 0) { // > 512 and dead
			sprintf(sAmmo, "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			sprintf(strng, "%s   %s %c%c:%-3s",name,
										"dead   ",
										w1,w2,
										sAmmo);
		}

		else if (cl_multiview.value == 2 && cl_mvinset.value && CURRVIEW == 1) { // mvinset
			sprintf(sAmmo, "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			sprintf(strng, "%s %.5s  %c%03d %03d %c%c:%-3s",pups,
												name,
												armor,
												cl.players[nPlayernum].stats[STAT_ARMOR],
												cl.players[nPlayernum].stats[STAT_HEALTH],
												w1,w2,
												sAmmo);
		} 
		else if (cl_multiview.value && vid.width <= 512) { // <= 512 and alive
			sprintf(sAmmo, "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			sprintf(strng, "%s %.2s %c%03d %03d %c%c:%-3s",pups,
												name,
												armor,
												cl.players[nPlayernum].stats[STAT_ARMOR],
												cl.players[nPlayernum].stats[STAT_HEALTH],
												w1,w2,
												sAmmo);
		}
		
		else {
			sprintf(sAmmo, "%02d", cl.players[nPlayernum].stats[STAT_AMMO]); // > 512 and alive
			sprintf(strng, "%s %s  %c%03d %03d %c%c:%-3s",pups,
												name,
												armor,
												cl.players[nPlayernum].stats[STAT_ARMOR],
												cl.players[nPlayernum].stats[STAT_HEALTH],
												w1,w2,
												sAmmo);
		}
	}

	// placement
	if (cl_multiview.value == 1) {
			xb = vid.width - strlen(strng) * 8 - 12;
			yb = vid.height - sb_lines - 16;
			xc = vid.width/2;
			yc = vid.height/2;
			xd = vid.width - strlen(weapons) * 8 - 84;
			yd = vid.height - sb_lines - 8;
	}

	else if (cl_multiview.value == 2) {
		if (!cl_mvinset.value) {
		if (CURRVIEW == 2) { // top
			xb = vid.width - strlen(strng) * 8 - 12;
			yb = vid.height/2 - sb_lines - 16;
			xc = vid.width/2;
			yc = vid.height/4;
			xd = vid.width - strlen(weapons) * 8 - 84;
			yd = vid.height/2 - sb_lines - 8;
		}
		else if (CURRVIEW == 1) { // bottom
			xb = vid.width - strlen(strng) * 8 - 12;
			yb = vid.height - sb_lines - 16;
			xc = vid.width/2;
			yc = vid.height/2 + vid.height/4;
			xd = vid.width - strlen(weapons) * 8 - 84;
			yd = vid.height - sb_lines - 8;
		}
		} else if (cl_mvinset.value) {
		if (CURRVIEW == 2) { // main
			xb = vid.width - strlen(strng) * 8 - 12;
			yb = vid.height/2 - sb_lines - 16;
			xc = -2;
			yc = -2;
			xd = vid.width - strlen(weapons) * 8 - 84;
			yd = vid.height/2 - sb_lines - 8;
		}
		else if (CURRVIEW == 1) { // top right
			xb = vid.width - strlen(strng) * 8; // hud
			yb = vid.height/3 - 16;
			if (cl_mvinsetcrosshair.value) {
				xc = vid.width - (double)(vid.width/3)/2-2;
				yc = (vid.height/3)/2;
			} else { 
				xc=yc=-2; 
			}
			xd = vid.width - strlen(weapons) * 8 - 70; // weapons
			yd = vid.height/3 - 8;
		}
		}
		
	}
	else if (cl_multiview.value == 3) {
		if (CURRVIEW == 2) { // top
			xb = vid.width - strlen(strng) * 8 - 12;
			yb = vid.height/2 - sb_lines - 16;
			xc = vid.width/2;
			yc = vid.height/4;
			xd = vid.width - strlen(weapons) * 8 - 84;
			yd = vid.height/2 - sb_lines - 8;
		}
		else if (CURRVIEW == 3) { // bl
			xb = vid.width - (vid.width/2)- strlen(strng) * 8 - 12;
			yb = vid.height - sb_lines - 16;
			xc = vid.width/4;
			yc = vid.height/2 + vid.height/4;
			xd = vid.width - (vid.width/2)- strlen(weapons) * 8 - 84;
			yd = vid.height - sb_lines - 8;
		}
		else if (CURRVIEW == 1) { // br
			xb = vid.width - strlen(strng) * 8 - 12;
			yb = vid.height - sb_lines - 16;
			xc = vid.width/2 + vid.width/4;
			yc = vid.height/2 + vid.height/4;
			xd = vid.width - strlen(weapons) * 8 - 84;
			yd = vid.height - sb_lines - 8;
		}
	}
	else if (cl_multiview.value == 4) {
		if (CURRVIEW == 2) { // tl
			xb = vid.width - (vid.width/2)- strlen(strng) * 8 - 12;
			yb = vid.height/2 - sb_lines - 16;
			xc = vid.width/4;
			yc = vid.height/4;
			xd = vid.width - (vid.width/2)- strlen(weapons) * 8 - 84;
			yd = vid.height/2 - sb_lines - 8;
		}
		else if (CURRVIEW == 3) { // tr
			xb = vid.width - strlen(strng) * 8 - 12;
			yb = vid.height/2 - sb_lines - 16;
			xc = vid.width/2 + vid.width/4;
			yc = vid.height/4;
			xd = vid.width - strlen(weapons)-25 * 8 - 8;
			yd = vid.height - sb_lines - (vid.height/1.9);
			xd = vid.width - strlen(weapons) * 8 - 84;
			yd = vid.height/2 - sb_lines - 8;
		}
		else if (CURRVIEW == 4) { // bl
			xb = vid.width - (vid.width/2)- strlen(strng) * 8 - 12;
			yb = vid.height - sb_lines - 16;
			xc = vid.width/4;
			yc = vid.height/2 + vid.height/4;
			xd = vid.width - (vid.width/2)- strlen(weapons) * 8 - 84;
			yd = vid.height - sb_lines - 8;
		}
		else if (CURRVIEW == 1) { // br
			xb = vid.width - strlen(strng) * 8 - 12;
			yb = vid.height - sb_lines - 16;
			xc = vid.width/2 + vid.width/4;
			yc = vid.height/2 + vid.height/4;
			xd = vid.width - strlen(weapons) * 8 - 84;
			yd = vid.height - sb_lines - 8;
		}
	}
	if (cl_multiview.value == 2 && cl_mvinset.value)
		memcpy(cl.stats, cl.players[nTrack1duel].stats, sizeof(cl.stats));

	// fill the void
	if (cl_sbar.value && cl_multiview.value==2 && cl_mvinset.value && cl_mvinsethud.value && cl_mvdisplayhud.value) {
		if (vid.width > 512)
			Draw_Fill(vid.width/3*2+1,vid.height/3-sb_lines/3+1,vid.width/3+2, sb_lines/3-1,c2);
		else
			Draw_Fill(vid.width/3*2+1,vid.height/3-sb_lines/3+1,vid.width/3+2, sb_lines/6 + 1,c2);
	}

	// hud info
	if (cl_mvdisplayhud.value && !cl_mvinset.value && cl_multiview.value == 2 
		|| cl_mvdisplayhud.value && cl_multiview.value != 2)
		Draw_String(xb,yb,strng);
	else if (cl_multiview.value == 2 && cl_mvdisplayhud.value && CURRVIEW == 1 && cl_mvinsethud.value) {
		if (vid.width > 512)
			Draw_String(xb,yb,strng);
		else { // <= 512 mvinset, just draw the name
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

			limit = ceil(7.0/192 * vid.width + 4/3); // linearly limit length of name for 320->512 conwidth to fit in inset
			Q_strncpyz(namestr,name,limit);

			if (cl_sbar.value)
				Draw_String(vid.width - strlen(namestr) * 8 - var - 2,yb + 1,namestr);
			else
				Draw_String(vid.width - strlen(namestr) * 8 - var - 2,yb + 4,namestr);
		}
	}


	// weapons
	if (cl_mvdisplayhud.value && !cl_mvinset.value && cl_multiview.value == 2 
		|| cl_mvdisplayhud.value && cl_multiview.value != 2)
		Draw_String(xd,yd,weapons);
	else if (cl_multiview.value == 2 && cl_mvdisplayhud.value && CURRVIEW == 1 && vid.width > 512  && cl_mvinsethud.value)
		Draw_String(xd,yd,weapons);

	// borders
	if (cl_multiview.value == 2 && !cl_mvinset.value) {
		Draw_Fill(0,vid.height/2,vid.width-1,1,c2); // oppymv 300804
	}
	else if (cl_multiview.value == 2 && cl_mvinset.value) { // oppymv 040904 - do ifdefs if over border in gl really annoys
		if (vid.width <= 512 && cl_sbar.value) {
			Draw_Fill(vid.width/3*2+1,vid.height/3-sb_lines/3,vid.width/3+2,1,c2); // oppymv 300804
			Draw_Fill(vid.width/3*2+1,0,1,vid.height/3-sb_lines/3,c2);
		} else if (vid.width>512 && cl_sbar.value && !cl_mvinsethud.value || vid.width>512 && cl_sbar.value && !cl_mvdisplayhud.value) {
			Draw_Fill(vid.width/3*2,vid.height/3-sb_lines/3,vid.width/3,1,c2); // oppymv 300804
			Draw_Fill(vid.width/3*2,0,1,vid.height/3-sb_lines/3,c2);
		}
		else { // sbar 0 and <= 512 conwidth
			Draw_Fill(vid.width/3*2+1,vid.height/3,vid.width/3+2,1,c2); // oppymv 300804
			Draw_Fill(vid.width/3*2+1,0,1,vid.height/3,c2);
		}
	}
	else if (cl_multiview.value == 3) {
		Draw_Fill(vid.width/2,vid.height/2,1,vid.height/2,c2);
		Draw_Fill(0,vid.height/2,vid.width-0,1,c2); // oppymv 300804
	}
	else if (cl_multiview.value == 4) {
		Draw_Fill(vid.width/2,0,1,vid.height,c2);
		Draw_Fill(0,vid.height/2,vid.width-0,1,c2); // oppymv 300804
	}
}
