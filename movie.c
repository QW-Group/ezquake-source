/*

Copyright (C) 2001-2002       A Nourai

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "quakedef.h"
#include "utils.h"

#ifdef _WIN32
	#include <windows.h>
#else
	#include <time.h>
#endif

static qboolean OnChange_movie_dir(cvar_t *var, char *string);
int SCR_Screenshot(char *);

#ifdef GLQUAKE
	extern cvar_t scr_sshot_type;
#endif

cvar_t   movie_fps			=  {"demo_capture_fps", "30.0"};
cvar_t   movie_dir			=  {"demo_capture_dir",  "capture", 0, OnChange_movie_dir};
cvar_t   movie_steadycam	=  {"demo_capture_steadycam", "0"};

static qboolean	movie_is_capturing = false, movie_first_frame;
static double movie_start_time, movie_len;
static int movie_frame_count;
static char	image_ext[4];

#ifdef _WIN32
	static SYSTEMTIME	movie_start_date;
#else
	struct tm			movie_start_date;
#endif

qboolean Movie_IsCapturing(void) {
	return cls.demoplayback && !cls.timedemo && movie_is_capturing;
}

static void Movie_Start(double _time) {

#ifndef _WIN32
	time_t t;
	t = time(NULL);
	localtime_r(&t, &movie_start_date);
#else
	GetLocalTime(&movie_start_date);
#endif
	movie_first_frame = movie_is_capturing = true;
	movie_len = _time;
	movie_start_time = cls.realtime;

	movie_frame_count = 0;
#ifdef GLQUAKE
	strcpy(image_ext, "tga");
#else
	strcpy(image_ext, "pcx");
#endif
}

void Movie_Stop (void) {
	movie_is_capturing = false;
	Com_Printf("Captured %d frames (%.2fs).\n", movie_frame_count, (float) (cls.realtime - movie_start_time));
}

void Movie_Demo_Capture_f(void) {
	int argc;
	double time;
	char *error;
	
	error = va("Usage: %s <start time | stop>\n", Cmd_Argv(0));
	if ((argc = Cmd_Argc()) != 2 && argc != 3) {
		Com_Printf(error);
		return;
	}
	if (argc == 2) {
		if (Q_strncasecmp("stop", Cmd_Argv(1), 4))
			Com_Printf(error);
		else if (Movie_IsCapturing())
			Movie_Stop();
		else
			Com_Printf("%s : Not capturing\n", Cmd_Argv(0));
		return;
	}
	if (Q_strncasecmp("start", Cmd_Argv(1), 5)) {
		Com_Printf(error);
		return;
	} else if (Movie_IsCapturing()) {
		Com_Printf("%s : Already capturing\n", Cmd_Argv(0));
		return;
	}
	if (!cls.demoplayback || cls.timedemo) {
		Com_Printf("%s : Must be playing a demo to capture\n", Cmd_Argv(0));
		return;
	}
	if ((time = Q_atof(Cmd_Argv(2))) <= 0) {
		Com_Printf("%s : Time argument must be positive\n", Cmd_Argv(0));
		return;
	}
	Movie_Start(time);
}

void Movie_Init(void) {
	Cvar_SetCurrentGroup(CVAR_GROUP_DEMO);
	Cvar_Register(&movie_fps);
	Cvar_Register(&movie_dir);
	Cvar_Register(&movie_steadycam);

	Cvar_ResetCurrentGroup();

	Cmd_AddCommand("demo_capture", Movie_Demo_Capture_f);
}

double Movie_StartFrame(void) {
	double time;

	if (movie_first_frame)
		movie_start_time = cls.realtime;
	movie_first_frame = false;

	time = movie_fps.value > 0 ? 1.0 / movie_fps.value : 1 / 30.0;
	return bound(1.0 / 1000, time, 1.0);
}

void Movie_FinishFrame(void) {
	char fname[128];

	if (!Movie_IsCapturing())
		return;

#ifdef _WIN32
	Q_snprintfz(fname, sizeof(fname), "%s/capture_%02d-%02d-%04d_%02d-%02d-%02d/shot-%06d.%s",
	movie_dir.string, movie_start_date.wDay, movie_start_date.wMonth, movie_start_date.wYear,
	movie_start_date.wHour,	movie_start_date.wMinute, movie_start_date.wSecond,	movie_frame_count, image_ext);
#else
	Q_snprintfz(fname, sizeof(fname), "%s/capture_%02d-%02d-%04d_%02d-%02d-%02d/shot-%06d.%s",
	movie_dir.string, movie_start_date.tm_mday, movie_start_date.tm_mon, movie_start_date.tm_year,
	movie_start_date.tm_hour, movie_start_date.tm_min, movie_start_date.tm_sec,	movie_frame_count, image_ext);
#endif
	SCR_Screenshot(fname);
	movie_frame_count++;
	if (cls.realtime >= movie_start_time + movie_len)
		Movie_Stop();
}

static qboolean OnChange_movie_dir(cvar_t *var, char *string) {
	if (Movie_IsCapturing()) {
		Com_Printf("Cannot change demo_capture_dir whilst capturing.  Use 'demo_capture stop' to cease capturing first.\n");
		return true;
	} else if (strlen(string) > 31) {
		Com_Printf("demo_capture_dir can only contain a maximum of 31 characters\n");
		return true;
	}
	Util_Process_Filename(string);
	if (!(Util_Is_Valid_Filename(string))) {
		Com_Printf(Util_Invalid_Filename_Msg("demo_capture_dir"));
		return true;
	}
	return false;
}
