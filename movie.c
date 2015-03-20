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

	$Id: movie.c,v 1.26 2007-10-11 06:46:12 borisu Exp $
*/

#include "quakedef.h"
#include "utils.h"
#include "qsound.h"
#ifdef _WIN32
#include "movie_avi.h"	//joe: capturing to avi
#include <windows.h>
#else
	#include <time.h>
#endif

static void OnChange_movie_dir(cvar_t *var, char *string, qbool *cancel);
//int SCR_Screenshot(char *);
void SCR_Movieshot (char *);	//joe: capturing to avi

//joe: capturing audio
#ifdef _WIN32
extern short *snd_out;
extern int snd_linear_count;

// Variables for buffering audio
short capture_audio_samples[44100];	// big enough buffer for 1fps at 44100Hz
int captured_audio_samples;
#endif

extern cvar_t scr_sshot_type;

cvar_t   movie_fps			=  {"demo_capture_fps", "30.0"};
cvar_t   movie_dir			=  {"demo_capture_dir",  "capture", 0, OnChange_movie_dir};
cvar_t   movie_steadycam	=  {"demo_capture_steadycam", "0"};

#ifdef _WIN32
cvar_t   movie_codec		= {"demo_capture_codec", "0"};	// Capturing to avi
cvar_t   movie_mp3			= {"demo_capture_mp3", "0"};
cvar_t   movie_mp3_kbps		= {"demo_capture_mp3_kbps", "128"};
cvar_t   movie_vid_maxlen   = {"demo_capture_vid_maxlen", "0"};
cvar_t   movie_quietcapture = {"demo_capture_quiet", "0"};
char movie_avi_filename[MAX_OSPATH];	// Stores the user's requested filename
static void Movie_Start_AVI_Capture(qbool split);
int avi_number = 0;
static qbool avi_restarting = false;
#endif

static qbool movie_is_capturing = false;
static double movie_start_time, movie_len;
static double movie_fragment_start_time;
static int movie_frame_count;
static char	image_ext[4];

//joe: capturing to avi
#ifdef _WIN32
qbool movie_is_avi = false, movie_avi_loaded, movie_acm_loaded;
static char avipath[256];
static FILE *avifile = NULL;
#endif

#ifdef _WIN32
	static SYSTEMTIME	movie_start_date;
#else
	struct tm			movie_start_date;
#endif

qbool Movie_IsCapturing(void) {
	return cls.demoplayback && !cls.timedemo && movie_is_capturing;
}

static void Movie_Start(double _time) 
{
	extern cvar_t scr_sshot_format;

	#ifndef _WIN32
	time_t t;
	t = time(NULL);
	localtime_r(&t, &movie_start_date);
	#else
	GetLocalTime(&movie_start_date);
	#endif
	movie_is_capturing = true;
	#ifdef _WIN32
	movie_is_avi = !!avifile; //joe: capturing to avi
	#endif
	movie_len = _time;
	movie_start_time = cls.realtime;
	movie_fragment_start_time = cls.realtime;

	movie_frame_count = 0;

	#ifdef _WIN32
	if (movie_is_avi)	//joe: capturing to avi
	{
		movie_is_capturing = Capture_Open (avipath);
	}
	else
	#endif
	{
		// DEFAULT_SSHOT_FORMAT
		if (!strcmp(scr_sshot_format.string, "tga")
		 || !strcmp(scr_sshot_format.string, "jpeg")
		 || !strcmp(scr_sshot_format.string, "jpg")
		 || !strcmp(scr_sshot_format.string, "png"))
		{
			strlcpy(image_ext, scr_sshot_format.string, sizeof(image_ext));		
		}
		else
		{
			strlcpy (image_ext, "tga", sizeof (image_ext));
		}
	}
}

void Movie_Stop (void) {
	movie_is_capturing = false;
#ifdef _WIN32
	if (movie_is_avi) { //joe: capturing to avi
		Capture_Close ();
		fclose (avifile);
		avifile = NULL;
	}
	if (!avi_restarting) {
		S_StopAllSounds(true);
		Com_Printf("Captured %d frames (%.2fs).\n", movie_frame_count, (float) (cls.realtime - movie_start_time));
	}
#endif
}

void Movie_Demo_Capture_f(void) {
	int argc;
	double time;
	char *error;
	
#ifdef _WIN32
	error = va("Usage: %s <start time [avifile] | stop>\n", Cmd_Argv(0));
	if ((argc = Cmd_Argc()) != 2 && argc != 3 && argc != 4) {
#else
	error = va("Usage: %s <start time | stop>\n", Cmd_Argv(0));
	if ((argc = Cmd_Argc()) != 2 && argc != 3) {
#endif
		Com_Printf(error);
		return;
	}
	if (argc == 2) {
		if (strncasecmp("stop", Cmd_Argv(1), 4))
			Com_Printf(error);
		else if (Movie_IsCapturing()) 
			Movie_Stop();
		else
			Com_Printf("%s : Not capturing\n", Cmd_Argv(0));
		return;
	}
	if (strncasecmp("start", Cmd_Argv(1), 5)) {
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
#ifdef _WIN32
	//joe: capturing to avi
	if (argc == 4) {
		avi_number = 0;

		strlcpy(movie_avi_filename, Cmd_Argv(3), sizeof(movie_avi_filename)-10);		// Store user's requested filename
		if (!movie_avi_loaded) {
			Com_Printf_State (PRINT_FAIL, "Avi capturing not initialized\n");
			return;
		}

		Movie_Start_AVI_Capture(movie_vid_maxlen.value > 0 && movie_vid_maxlen.value < time);
	}
#endif
	Movie_Start(time);
}

#ifdef _WIN32
static void Movie_Start_AVI_Capture(qbool split) 
{
	++avi_number;

	// If we're going to break up the movie, append number
	char aviname[MAX_OSPATH];
	if (split)
		snprintf (aviname, sizeof(aviname), "%s-%03d", movie_avi_filename, avi_number);
	else 
		strlcpy (aviname, movie_avi_filename, sizeof(aviname));

	if (!(Util_Is_Valid_Filename(aviname))) {
		Com_Printf(Util_Invalid_Filename_Msg(aviname));
		return;
	}
	COM_ForceExtensionEx (aviname, ".avi", sizeof (aviname));
	snprintf (avipath, sizeof(avipath), "%s/%s/%s", com_basedir, movie_dir.string, aviname);
	if (!(avifile = fopen(avipath, "wb"))) {
		FS_CreatePath (avipath);
		if (!(avifile = fopen(avipath, "wb"))) {
			Com_Printf("Error: Couldn't open %s\n", aviname);
			return;
		}
	}
}
#endif

void Movie_Init(void) {
	Cvar_SetCurrentGroup(CVAR_GROUP_DEMO);
	Cvar_Register(&movie_fps);
	Cvar_Register(&movie_dir);
	Cvar_Register(&movie_steadycam);

	Cvar_ResetCurrentGroup();

	Cmd_AddCommand("demo_capture", Movie_Demo_Capture_f);

#ifdef _WIN32
	Capture_InitAVI ();		//joe: capturing to avi
	if (!movie_avi_loaded)
		return;

	captured_audio_samples = 0;
	Cvar_SetCurrentGroup(CVAR_GROUP_DEMO);
	Cvar_Register(&movie_codec);
	Cvar_Register(&movie_vid_maxlen);
	Cvar_Register(&movie_quietcapture);

	Cvar_ResetCurrentGroup();

	Capture_InitACM ();
	if (!movie_acm_loaded)
		return;

	Cvar_SetCurrentGroup(CVAR_GROUP_DEMO);
	Cvar_Register(&movie_mp3);
	Cvar_Register(&movie_mp3_kbps);

	Cvar_ResetCurrentGroup();
#endif
}

double Movie_StartFrame(void) 
{
	double time;
	int views = 1;

	if (cl_multiview.value)
	{
		views = cl_multiview.value;
	}

	if (Cmd_FindAlias("f_captureframe"))
	{
		Cbuf_AddTextEx (&cbuf_main, "f_captureframe\n");
	}

	// Default to 30 fps.
	time = (movie_fps.value > 0) ? (1.0 / movie_fps.value) : (1 / 30.0);
	return bound(1.0 / 1000, time / views, 1.0);
}

void Movie_FinishFrame(void) 
{
	char fname[128];
	if (!Movie_IsCapturing())
		return;

	#ifdef _WIN32
	if (!movie_is_avi) 
	{
		snprintf(fname, sizeof(fname), "%s/capture_%02d-%02d-%04d_%02d-%02d-%02d/shot-%06d.%s",
			movie_dir.string, movie_start_date.wDay, movie_start_date.wMonth, movie_start_date.wYear,
			movie_start_date.wHour,	movie_start_date.wMinute, movie_start_date.wSecond, movie_frame_count, image_ext);

		con_suppress = true;
	}
	else 
	{
		// Split up if we're over the time limit for each segment
		if (movie_vid_maxlen.value && cls.realtime >= movie_fragment_start_time + movie_vid_maxlen.value) 
		{
			double original_start_time = movie_start_time;

			// Close existing, and start again
			avi_restarting = true;
			Movie_Stop();
			Movie_Start_AVI_Capture(true);
			Movie_Start(movie_len);
			avi_restarting = false;

			// keep track of original start time so we know when to stop for good
			movie_start_time = original_start_time;
		}
	}
	#else
	snprintf(fname, sizeof(fname), "%s/capture_%02d-%02d-%04d_%02d-%02d-%02d/shot-%06d.%s",
		movie_dir.string, movie_start_date.tm_mday, movie_start_date.tm_mon, movie_start_date.tm_year,
		movie_start_date.tm_hour, movie_start_date.tm_min, movie_start_date.tm_sec, movie_frame_count, image_ext);

	con_suppress = true;
	#endif // _WIN32

	//SCR_Screenshot(fname);
	//movie_frame_count++;

	// Only capture a frame after all views have been drawn
	// in multiview mode. Otherwise always.
	if (cl_multiview.value && cls.mvdplayback) 
	{
		if (CURRVIEW == 1)
		{
			SCR_Movieshot(fname);
		}
	} 
	else
	{
		SCR_Movieshot(fname);
	}

#ifdef _WIN32
	if (!movie_is_avi)
#endif
	{
		con_suppress = false;
	}

	// Only count the frame when all the views have been drawn
	// in multiview mode. (Instead of counting one for each view that is drawn).
	if (cl_multiview.value && cls.mvdplayback) 
	{
		if (CURRVIEW == 1)
		{
			movie_frame_count++;
		}
	} 
	else
	{
		movie_frame_count++;
	}

	if (cls.realtime >= movie_start_time + movie_len)
		Movie_Stop();
}

//joe: capturing audio
#ifdef _WIN32
short* Movie_SoundBuffer(void) 
{
	if (!movie_is_avi || !Movie_IsCapturing())
		return NULL;

	return capture_audio_samples + (captured_audio_samples << 1);
}

void Movie_TransferStereo16(void) {
	int val, i;

	if (!movie_is_avi || !Movie_IsCapturing())
		return;

	// Copy last audio chunk written into our temporary buffer
	// memcpy (capture_audio_samples + (captured_audio_samples << 1), snd_out, snd_linear_count * shm->format.channels);
	captured_audio_samples += (snd_linear_count >> 1);

	if (captured_audio_samples >= (int)(0.5 + cls.frametime * shm->format.speed)) {
		// We have enough audio samples to match one frame of video
		Capture_WriteAudio (captured_audio_samples, (byte *)capture_audio_samples);
		captured_audio_samples = 0;
	}
}

qbool Movie_GetSoundtime(void) {
	int views = 1;
	extern cvar_t cl_demospeed;

	if (!movie_is_avi || !Movie_IsCapturing() || !cl_demospeed.value)
		return false;

	if (cl_multiview.value)
	{
		views = cl_multiview.value;
	}

	soundtime += (int)(0.5 + cls.frametime * shm->format.speed * views * (1.0 / cl_demospeed.value)); //joe: fix for slowmo/fast forward
	return true;
}
#endif

static void OnChange_movie_dir(cvar_t *var, char *string, qbool *cancel) {
	if (Movie_IsCapturing()) {
		Com_Printf("Cannot change demo_capture_dir whilst capturing.  Use 'demo_capture stop' to cease capturing first.\n");
		*cancel = true;
		return;
	} else if (strlen(string) > 31) {
		Com_Printf("demo_capture_dir can only contain a maximum of 31 characters\n");
		*cancel = true;
		return;
	}
	Util_Process_Filename(string);
	if (!(Util_Is_Valid_Filename(string))) {
		Com_Printf(Util_Invalid_Filename_Msg("demo_capture_dir"));
		*cancel = true;
		return;
	}
}
