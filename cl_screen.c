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

	$Id: cl_screen.c,v 1.46 2006-04-06 23:23:18 disconn3ct Exp $
*/

#include "quakedef.h"
#include "cl_screen.h"
#include <time.h>
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


char *COM_FileExtension (char *in);


extern byte	current_pal[768];	// Tonik

extern cvar_t	scr_newHud;		// HUD -> hexum


// only the refresh window will be updated unless these variables are flagged 
int				scr_copytop;
int				scr_copyeverything;

float			scr_con_current;
float			scr_conlines;           // lines of console to display

float			oldscreensize, oldfov, oldsbar;


qbool OnFovChange (cvar_t *var, char *value);
qbool OnDefaultFovChange (cvar_t *var, char *value);
cvar_t			scr_fov = {"fov", "90", CVAR_ARCHIVE, OnFovChange};	// 10 - 140
cvar_t			default_fov = {"default_fov", "90", CVAR_ARCHIVE, OnDefaultFovChange};
cvar_t			scr_viewsize = {"viewsize", "100", CVAR_ARCHIVE};
cvar_t			scr_consize = {"scr_consize", "0.75"};
cvar_t			scr_conspeed = {"scr_conspeed", "1000"};
cvar_t			scr_centertime = {"scr_centertime", "2"};
cvar_t		    	scr_centershift = {"scr_centershift", "0"}; // BorisU
cvar_t			scr_showram = {"showram", "1"};
cvar_t			scr_showturtle = {"showturtle", "0"};
cvar_t			scr_showpause = {"showpause", "1"};
cvar_t			scr_printspeed = {"scr_printspeed", "8"};
qbool OnChange_scr_allowsnap(cvar_t *, char *);
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

// QW262 -->
cvar_t			cl_hud = {"cl_hud", "1"};
// <-- QW262

#ifdef GLQUAKE
cvar_t			gl_triplebuffer = {"gl_triplebuffer", "1", CVAR_ARCHIVE};
#endif

cvar_t			scr_autoid		= {"scr_autoid", "0"};
cvar_t			scr_coloredText = {"scr_coloredText", "1"};

// START shaman RFE 1022309
cvar_t			scr_tracking			= {"scr_tracking", "Tracking %t %n, [JUMP] for next"};
cvar_t			scr_spectatorMessage	= {"scr_spectatorMessage", "1"};
// END shaman RFE 1022309




qbool		scr_initialized;                // ready to draw

mpic_t			*scr_ram;
mpic_t			*scr_net;
mpic_t			*scr_turtle;

int				scr_fullupdate;

int				clearconsole;
int				clearnotify;

viddef_t		vid;                            // global video state

vrect_t			scr_vrect;

qbool		scr_skipupdate;

qbool		scr_drawloading;
qbool		scr_disabled_for_loading;
float			scr_disabled_time;

qbool		block_drawing;


static int scr_autosshot_countdown = 0;
static int scr_mvsshot_countdown = 0;
char auto_matchname[2 * MAX_OSPATH];

static void SCR_CheckAutoScreenshot(void);
static void SCR_CheckMVScreenshot(void);
void SCR_DrawStatusMultiview(void);
void Draw_AlphaFill (int x, int y, int w, int h, int c, float alpha);
void Draw_AlphaString (int x, int y, char *str, float alpha);
void Draw_AlphaPic (int x, int y, mpic_t *pic, float alpha);


qbool OnChange_scr_allowsnap(cvar_t *var, char *s) {
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
	strlcpy (scr_centerstring, str, sizeof(scr_centerstring));
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

extern	cvar_t		v_idlescale;
qbool	concussioned = false;

qbool OnFovChange (cvar_t *var, char *value)
{
	
	float newfov = Q_atof(value);

	if (newfov > 140)
		newfov = 140;
	else if (newfov < 10)
		newfov = 10;

	if (newfov == scr_fov.value)
		return true;

	if ( cbuf_current != &cbuf_svc) {
		if (concussioned && !cls.demoplayback)
			return true;
	} else {
		if (newfov != 90 && cl.teamfortress && v_idlescale.value >= 20) {
			concussioned = true;
			if (v_idlescale.value == 100)
				TP_ExecTrigger ("f_conc");
		} else if (newfov == 90 && cl.teamfortress) {
			concussioned = false;
		}
		if (cls.demoplayback) // && !cl_fovfromdemo.value)
			return true;
	}

	vid.recalc_refdef = true;
	if (newfov == 90) {
		Cvar_Set (&scr_fov,default_fov.string);
		return true;
	}

	Cvar_SetValue (&scr_fov, newfov);
	return true;
}

qbool OnDefaultFovChange (cvar_t *var, char *value)
{
	float newfov = Q_atof(value);
	
	if (newfov < 10.0 || newfov > 140.0){
		Com_Printf("Invalid default_fov\n");
		return true;
	}
	return false;
}

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
		strlcpy (str, SecondsToHourString((int) time), sizeof(str));
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
		strlcpy (str, SecondsToHourString(timelimit), sizeof(str));
	else
		strlcpy (str, SecondsToHourString((int) abs(timelimit - cl.gametime)), sizeof(str));

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
		strlcpy (str, SecondsToHourString((int) (cls.demotime)), sizeof(str));
	else
		strlcpy (str, SecondsToHourString((int) (cls.demotime - demostarttime)), sizeof(str));

	x = ELEMENT_X_COORD(scr_democlock);
	y = ELEMENT_Y_COORD(scr_democlock);
	Draw_String (x, y, str);
}

void SCR_DrawPause (void) {
	extern cvar_t sv_paused;
	mpic_t *pic;

	if (!scr_showpause.value)               // turn off for screenshots
		return;

	if (!cl.paused)
		return;

#ifndef CLIENTONLY
	if (sv_paused.value == 2)
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
	glGetIntegerv(GL_VIEWPORT, (GLint *)view);

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

/**************************************** 262 HUD *****************************/
// QW262 -->
static hud_element_t *hud_list=NULL;
static hud_element_t *prev;

hud_element_t *Hud_FindElement(char *name)
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

static hud_element_t* Hud_NewElement(void)
{
	hud_element_t* elem;
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
			if( (var = Cvar_FindVar(a3)) ) {
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
extern int char_texture;

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

	if (mask < 0 || mask > 3) return; // bad mask
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
	hud_element_t*	elem;
	int			x,y,l;
	char			buf[256];
	char			*st = NULL;
	Hud_Func		func;
	double			tblink = 0;
	mpic_t			*img = NULL;

	if (hud_list && cl_hud.value && !cls.demoplayback && !cl.spectator) {

		for (elem = hud_list; elem; elem=elem->next) {
			if (!(elem->flags & HUD_ENABLED)) continue; // do not draw disabled elements

			elem->scr_height = 8;
		
			if (elem->flags & HUD_CVAR) {
				st = ((cvar_t*)elem->contents)->string;
				strlcpy (buf, st, sizeof(buf));
				st = buf;
				l = strlen (st);
			} else if (elem->flags & HUD_STRING) {
				Cmd_ExpandString(elem->contents, buf); //, sizeof(buf));
				st = TP_ParseMacroString(buf);
				st = TP_ParseFunChars(st, false);
				l = strlen(st);
			} else if (elem->flags & HUD_FUNC) {
				func = elem->contents;
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
#ifdef GLQUAKE
					if (elem->alpha < 1)
						Draw_AlphaFill(x, y, elem->scr_width, elem->scr_height, (unsigned char)elem->coords[3], elem->alpha);
					else
#endif
						Draw_Fill(x, y, elem->scr_width, elem->scr_height, (unsigned char)elem->coords[3]);
				}
			if (!(elem->flags & HUD_BLINK_F) || tblink < 0.5)
			{
				if (!(elem->flags & HUD_IMAGE))
				{
#ifdef GLQUAKE
					extern int char_texture;
					int std_charset = char_texture;
					if (elem->charset)
						char_texture = elem->charset;
					if (elem->alpha < 1)
						Draw_AlphaString (x, y, st, elem->alpha);
					else
#endif
						Draw_String (x, y, st);
#ifdef GLQUAKE
					char_texture = std_charset;
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
	int hud_x, hud_y, con_x, con_y;

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
			if (scr_gameclock.value)
				Draw_TileClear(ELEMENT_X_COORD(scr_gameclock), ELEMENT_Y_COORD(scr_gameclock), 10 * 8, 8);
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
				// QW262 -->
				SCR_DrawHud ();
				// <-- QW262
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
		(cl_multiview.value && cls.mvdplayback && CURRVIEW == 2))
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

	buffer = (byte *) Q_malloc (buffersize);
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
	int success = -1;
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
		strlcpy (name, Cmd_Argv(1), sizeof(name));
	} else if (Cmd_Argc() == 1) {
		// find a file name to save it to
#ifdef WITH_PNG
		if (!strcasecmp(scr_sshot_format.string, "png"))
			strlcpy(ext, "png", 4);
#endif
#ifdef WITH_JPEG
		if (!strcasecmp(scr_sshot_format.string, "jpeg") || !strcasecmp(scr_sshot_format.string, "jpg"))
			strlcpy(ext, "jpg", 4);
#endif
		if (!ext[0])
			strlcpy(ext, DEFAULT_SSHOT_FORMAT, 4);

		for (i = 0; i < 999; i++) {
			snprintf(name, sizeof(name), "ezquake%03i.%s", i, ext);
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
	base = (byte *) Q_malloc ((width * height + glwidth * glheight) * 3);
	pixels = base + glwidth * glheight * 3;

	glReadPixels (glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, base);
	Image_Resample (base, glwidth, glheight, pixels, width, height, 3, 0);
#ifdef WITH_JPEG
	if (QLib_isModuleLoaded(qlib_libjpeg)) {
		success = Image_WriteJPEG (filename, 70, pixels + 3 * width * (height - 1), -width, height)
			? SSHOT_SUCCESS : SSHOT_FAILED;
	} else
#endif
	success = Image_WriteTGA (filename, pixels, width, height)
		? SSHOT_SUCCESS : SSHOT_FAILED;

	free(base);

#else		//GLQUAKE

	D_EnableBackBufferAccess ();

#ifdef WITH_PNG
	if (QLib_isModuleLoaded(qlib_libpng)) {
		success = Image_WritePNGPLTE(filename, 9, vid.buffer, vid.width, vid.height, vid.rowbytes, current_pal)
			? SSHOT_SUCCESS : SSHOT_FAILED;
	} else
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
		if ((size = COM_FileOpenRead (va("%s/%s", com_basedir, filename), &f)) == -1)
		{
			Com_Printf ("Can't send screenshot to server: can't open file %s\n", filename);
		}
		else
		{
			screen_shot = (byte *) Q_malloc (size);
			if (fread(screen_shot, 1, size, f) == size)
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

	snprintf (savedname, sizeof(savedname), "%s_%03i%s", auto_matchname, num, ext);
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
		strlcpy(auto_matchname, matchname, sizeof(auto_matchname));
	}
}

//joe: capturing to avi
void SCR_Movieshot(char *name) {
#ifdef _WIN32
	if (movie_is_avi) {
#ifdef GLQUAKE
		int i, size = glwidth * glheight * 3;
		byte *buffer, temp;

		buffer = (byte *) Q_malloc (size);
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

		buffer = (byte *) Q_malloc (vid.width * vid.height * 3);

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
	Cvar_Register (&default_fov);
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

// QW262 -->
	Cvar_Register (&cl_hud);
// <--QW262

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
	
	int xb = 0, yb = 0, xc = 0, yc = 0, xd = 0, yd = 0;
	char strng[80];
	char weapons[40];
	char w1,w2;
	char sAmmo[3];
	char pups[4];
	char armor;
	char name[16];
	byte c, c2;

	int i;
	
	extern int powerup_cam_active,cam_1,cam_2,cam_3,cam_4;
	extern cvar_t mvd_pc_view_1,mvd_pc_view_2,mvd_pc_view_3,mvd_pc_view_4;
	

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
	} else
		w1='?'; w2='?';
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
	
	strlcpy(name,cl.players[nPlayernum].name, sizeof(name));

	if (strcmp(cl.players[nPlayernum].name,"") && !cl.players[nPlayernum].spectator) {
		if (cl.players[nPlayernum].stats[STAT_HEALTH] <= 0 && cl_multiview.value == 2 && cl_mvinset.value) { // mvinset and dead
			snprintf(sAmmo, sizeof(sAmmo), "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			sprintf(strng, "%.5s   %s %c%c:%-3s",name,
										"dead   ",
										w1,w2,
										sAmmo);
		}
		else if (cl.players[nPlayernum].stats[STAT_HEALTH] <= 0 && vid.width <= 512) { // <= 512 and dead
			snprintf(sAmmo, sizeof(sAmmo), "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			sprintf(strng, "%.2s  %s %c%c:%-3s",name,
										"dead   ",
										w1,w2,
										sAmmo);
		}
		else if (cl.players[nPlayernum].stats[STAT_HEALTH] <= 0) { // > 512 and dead
			snprintf(sAmmo, sizeof(sAmmo), "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			sprintf(strng, "%s   %s %c%c:%-3s",name,
										"dead   ",
										w1,w2,
										sAmmo);
		}

		else if (cl_multiview.value == 2 && cl_mvinset.value && CURRVIEW == 1) { // mvinset
			snprintf(sAmmo, sizeof(sAmmo), "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			sprintf(strng, "%s %.5s  %c%03d %03d %c%c:%-3s",pups,
												name,
												armor,
												cl.players[nPlayernum].stats[STAT_ARMOR],
												cl.players[nPlayernum].stats[STAT_HEALTH],
												w1,w2,
												sAmmo);
		} 
		else if (cl_multiview.value && vid.width <= 512) { // <= 512 and alive
			snprintf(sAmmo, sizeof(sAmmo), "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			sprintf(strng, "%s %.2s %c%03d %03d %c%c:%-3s",pups,
												name,
												armor,
												cl.players[nPlayernum].stats[STAT_ARMOR],
												cl.players[nPlayernum].stats[STAT_HEALTH],
												w1,w2,
												sAmmo);
		}
		
		else {
			snprintf(sAmmo, sizeof(sAmmo), "%02d", cl.players[nPlayernum].stats[STAT_AMMO]); // > 512 and alive
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

	if (CURRVIEW == 1 && mvd_pc_view_1.string && strlen(mvd_pc_view_1.string) && powerup_cam_active && cam_1){
		sAmmo[0]='\0';
		strng[0]='\0';
		weapons[0]='\0';
		};
	if (CURRVIEW == 2 && mvd_pc_view_2.string && strlen(mvd_pc_view_2.string) && powerup_cam_active && cam_2){
		sAmmo[0]='\0';
		strng[0]='\0';
		weapons[0]='\0';
		};
	if (CURRVIEW == 3 && mvd_pc_view_3.string && strlen(mvd_pc_view_3.string) && powerup_cam_active && cam_3){
		sAmmo[0]='\0';
		strng[0]='\0';
		weapons[0]='\0';
		};
	if (CURRVIEW == 4 && mvd_pc_view_4.string && strlen(mvd_pc_view_4.string) && powerup_cam_active && cam_4){
		sAmmo[0]='\0';
		strng[0]='\0';
		weapons[0]='\0';
		};

		
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
	if ((cl_mvdisplayhud.value && !cl_mvinset.value && cl_multiview.value == 2)
		|| (cl_mvdisplayhud.value && cl_multiview.value != 2))
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
			strlcpy(namestr,name,limit);

			if (cl_sbar.value)
				Draw_String(vid.width - strlen(namestr) * 8 - var - 2,yb + 1,namestr);
			else
				Draw_String(vid.width - strlen(namestr) * 8 - var - 2,yb + 4,namestr);
		}
	}


	// weapons
	if ((cl_mvdisplayhud.value && !cl_mvinset.value && cl_multiview.value == 2)
		|| (cl_mvdisplayhud.value && cl_multiview.value != 2))
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
		} else if ((vid.width>512 && cl_sbar.value && !cl_mvinsethud.value) || (vid.width>512 && cl_sbar.value && !cl_mvdisplayhud.value)) {
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
// <-- QW262
