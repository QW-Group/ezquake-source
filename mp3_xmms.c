/*

Copyright (C) 2001-2002       A Nourai
Audacious Control (2007)      P Archer

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

	$Id: mp3_xmms.c,v 1.5 2007-10-25 15:02:07 dkure Exp $
*/

#ifndef _WIN32
#include <sys/wait.h>
#include <sys/types.h> // fork, execv, usleep
#include <signal.h>
#include <unistd.h> // fork, execv, usleep
#endif // !WIN32

#include "quakedef.h"
#include "mp3_player.h"
#include "modules.h"
#include "utils.h"

#ifdef WITH_XMMS
#include <xmms/xmmsctrl.h>

#define XMMS_SESSION	((int) mp3_xmms_session.value)

// TODO: This is currently ignored and we just use the path to find the program
cvar_t mp3_dir = {"mp3_xmms_dir", "%%X11BASE%%/bin"}; // disconnect: todo: check if %%X11BASE%% is OK for Linux
cvar_t mp3_xmms_session = {"mp3_xmms_session", "0"};

static QLIB_HANDLETYPE_T libxmms_handle = NULL;

static void (*qxmms_remote_play)(gint session);
static void (*qxmms_remote_pause)(gint session);
static void (*qxmms_remote_stop)(gint session);
static void (*qxmms_remote_jump_to_time)(gint session, gint pos);
static gboolean (*qxmms_remote_is_running)(gint session);
static gboolean (*qxmms_remote_is_playing)(gint session);
static gboolean (*qxmms_remote_is_paused)(gint session);
static gboolean (*qxmms_remote_is_repeat)(gint session);
static gboolean (*qxmms_remote_is_shuffle)(gint session);
static gint (*qxmms_remote_get_playlist_pos)(gint session);
static void (*qxmms_remote_set_playlist_pos)(gint session, gint pos);
gint (*qxmms_remote_get_playlist_length)(gint session);
gchar *(*qxmms_remote_get_playlist_title)(gint session, gint pos);
static void (*qxmms_remote_playlist_prev)(gint session);
static void (*qxmms_remote_playlist_next)(gint session);
static gint (*qxmms_remote_get_output_time)(gint session);
static gint (*qxmms_remote_get_playlist_time)(gint session, gint pos);
static gint (*qxmms_remote_get_main_volume)(gint session);
static void (*qxmms_remote_set_main_volume)(gint session, gint v);
static void (*qxmms_remote_toggle_repeat)(gint session);
static void (*qxmms_remote_toggle_shuffle)(gint session);
void (*qg_free)(gpointer);

#define NUM_XMMSPROCS	(sizeof(xmmsProcs)/sizeof(xmmsProcs[0]))

static qlib_dllfunction_t xmmsProcs[] = {
	{"xmms_remote_play", (void **) &qxmms_remote_play},
	{"xmms_remote_pause", (void **) &qxmms_remote_pause},
	{"xmms_remote_stop", (void **) &qxmms_remote_stop},

	{"xmms_remote_jump_to_time", (void **) &qxmms_remote_jump_to_time},

	{"xmms_remote_is_running", (void **) &qxmms_remote_is_running},
	{"xmms_remote_is_playing", (void **) &qxmms_remote_is_playing},
	{"xmms_remote_is_paused", (void **) &qxmms_remote_is_paused},
	{"xmms_remote_is_repeat", (void **) &qxmms_remote_is_repeat},
	{"xmms_remote_is_shuffle", (void **) &qxmms_remote_is_shuffle},

	{"xmms_remote_get_playlist_pos", (void **) &qxmms_remote_get_playlist_pos},
	{"xmms_remote_set_playlist_pos", (void **) &qxmms_remote_set_playlist_pos},
	{"xmms_remote_get_playlist_length", (void **) &qxmms_remote_get_playlist_length},
	{"xmms_remote_get_playlist_title", (void **) &qxmms_remote_get_playlist_title},

	{"xmms_remote_playlist_prev", (void **) &qxmms_remote_playlist_prev},
	{"xmms_remote_playlist_next", (void **) &qxmms_remote_playlist_next},

	{"xmms_remote_get_output_time", (void **) &qxmms_remote_get_output_time},
	{"xmms_remote_get_playlist_time", (void **) &qxmms_remote_get_playlist_time},
	{"xmms_remote_get_main_volume", (void **) &qxmms_remote_get_main_volume},
	{"xmms_remote_set_main_volume", (void **) &qxmms_remote_set_main_volume},

	{"xmms_remote_toggle_repeat", (void **) &qxmms_remote_toggle_repeat},
	{"xmms_remote_toggle_shuffle", (void **) &qxmms_remote_toggle_shuffle},

	{"g_free", (void **) &qg_free},
};

static void XMMS_FreeLibrary(void) {
	if (libxmms_handle) {
		QLIB_FREELIBRARY(libxmms_handle);
	}
	// Maybe need to clear all the function pointers too
}

static void XMMS_LoadLibrary(void) {
	if (!(libxmms_handle = dlopen("libxmms.so", RTLD_NOW)))
		return;

	if (!QLib_ProcessProcdef(libxmms_handle, xmmsProcs, NUM_XMMSPROCS)) {
		XMMS_FreeLibrary();
		return;
	}
}

qbool MP3_XMMS_IsActive(void) {
	return !!libxmms_handle;
}

qbool MP3_XMMS_IsPlayerRunning(void) {
	return (qxmms_remote_is_running((gint) XMMS_SESSION));
}

static int XMMS_pid = 0;
void MP3_XMMS_Execute_f(void) {
	char exec_name[MAX_OSPATH], *argv[2] = {"xmms", NULL}/*, **s*/;
	int i; 

	if (MP3_XMMS_IsPlayerRunning()) {
		Com_Printf("XMMS is already running\n");
		return;
	}
	strlcpy(exec_name, argv[0], sizeof(exec_name));

	if (!(XMMS_pid = fork())) { // Child
		execvp(exec_name, argv);
		exit(-1);
	}
	if (XMMS_pid == -1) {
		Com_Printf ("Couldn't execute XMMS\n");
		return;
	}
	for (i = 0; i < 6; i++) {
		Sys_MSleep(50);
		if (MP3_XMMS_IsPlayerRunning()) {
			Com_Printf("XMMS is now running\n");
			return;
		}
	}
	Com_Printf("XMMS (probably) failed to run\n");
}

#define XMMS_COMMAND(Name, Param)										\
    void MP3_XMMS_##Name##_f(void) {									\
	   if (MP3_XMMS_IsPlayerRunning()) {								\
		   qxmms_remote_##Param(XMMS_SESSION);							\
	   } else {															\
		   Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps); 	\
	   }																\
	   return;															\
    }

XMMS_COMMAND(Prev, playlist_prev);
XMMS_COMMAND(Play, play);
XMMS_COMMAND(Pause, pause);
XMMS_COMMAND(Stop, stop);
XMMS_COMMAND(Next, playlist_next);
XMMS_COMMAND(FadeOut, stop);

void MP3_XMMS_FastForward_f(void) {
	int current;

	if (!MP3_XMMS_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
	current = qxmms_remote_get_output_time(XMMS_SESSION) + 5 * 1000;
	qxmms_remote_jump_to_time(XMMS_SESSION, current);
}

void MP3_XMMS_Rewind_f(void) {
	int current;

	if (!MP3_XMMS_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
	current = qxmms_remote_get_output_time(XMMS_SESSION) - 5 * 1000;
	current = max(0, current);
	qxmms_remote_jump_to_time(XMMS_SESSION, current);
}

int MP3_XMMS_GetStatus(void) {
	if (!MP3_XMMS_IsPlayerRunning())
		return MP3_NOTRUNNING;
	if (qxmms_remote_is_paused(XMMS_SESSION))
		return MP3_PAUSED;
	if (qxmms_remote_is_playing(XMMS_SESSION))
		return MP3_PLAYING;	
	return MP3_STOPPED;
}

static void XMMS_Set_ToggleFn(char *name, void *togglefunc, void *getfunc) {
	int ret, set;
	gboolean (*xmms_togglefunc)(gint) = togglefunc;
	gboolean (*xmms_getfunc)(gint) = getfunc;

	if (!MP3_XMMS_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
	if (Cmd_Argc() >= 3) {
		Com_Printf("Usage: %s [on|off|toggle]\n", Cmd_Argv(0));
		return;
	}
	ret = xmms_getfunc(XMMS_SESSION);
	if (Cmd_Argc() == 1) {
		Com_Printf("%s is %s\n", name, (ret == 1) ? "on" : "off");
		return;
	}
	if (!strcasecmp(Cmd_Argv(1), "on")) {
		set = 1;
	} else if (!strcasecmp(Cmd_Argv(1), "off")) {
		set = 0;
	} else if (!strcasecmp(Cmd_Argv(1), "toggle")) {
		set = ret ? 0 : 1;
	} else {
		Com_Printf("Usage: %s [on|off|toggle]\n", Cmd_Argv(0));
		return;
	}
	if (set && !ret)
		xmms_togglefunc(XMMS_SESSION);
	else if (!set && ret)
		xmms_togglefunc(XMMS_SESSION);
	Com_Printf("%s set to %s\n", name, set ? "on" : "off");
}

void MP3_XMMS_Repeat_f(void) {
	XMMS_Set_ToggleFn("Repeat", qxmms_remote_toggle_repeat, qxmms_remote_is_repeat);
}

void MP3_XMMS_Shuffle_f(void) {
	XMMS_Set_ToggleFn("Shuffle", qxmms_remote_toggle_shuffle, qxmms_remote_is_shuffle);
}

void MP3_XMMS_ToggleRepeat_f(void) {
	if (!MP3_XMMS_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
	qxmms_remote_toggle_repeat(XMMS_SESSION);
}

void MP3_XMMS_ToggleShuffle_f(void) {
	if (!MP3_XMMS_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
	qxmms_remote_toggle_shuffle(XMMS_SESSION);
}

char *MP3_XMMS_Macro_MP3Info(void) {
	int playlist_pos;
	char *s;
	static char title[MP3_MAXSONGTITLE];

	if (!MP3_XMMS_IsPlayerRunning()) {
		Com_Printf("XMMS not running\n");
		return NULL;
	}
	playlist_pos = qxmms_remote_get_playlist_pos(XMMS_SESSION);
	s = qxmms_remote_get_playlist_title(XMMS_SESSION, playlist_pos);
	strlcpy(title, s ? s : "", sizeof(title));
	//g_free(s);
	qg_free(s);
	return title;
}

qbool MP3_XMMS_GetOutputtime(int *elapsed, int *total) {
	int pos;

	if (!MP3_XMMS_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return false;
	}
	pos = qxmms_remote_get_playlist_pos(XMMS_SESSION);
	*elapsed = qxmms_remote_get_output_time(XMMS_SESSION) / 1000.0;
	*total = qxmms_remote_get_playlist_time(XMMS_SESSION, pos) / 1000.0;
	return true;
}

qbool MP3_XMMS_GetToggleState(int *shuffle, int *repeat) {
	if (!MP3_XMMS_IsPlayerRunning()) 
		return false;
	*shuffle = qxmms_remote_is_shuffle(XMMS_SESSION);
	*repeat = qxmms_remote_is_repeat(XMMS_SESSION);
	return true;
}


// Playlist functions
void MP3_XMMS_LoadPlaylist_f(void) {
	Com_Printf("The %s command is not (yet) supported.\n", Cmd_Argv(0));
}

void MP3_XMMS_GetPlaylistInfo(int *current, int *length) {
	if (!MP3_XMMS_IsPlayerRunning()) 
		return;
	if (length)
		*length = qxmms_remote_get_playlist_length(XMMS_SESSION);
	if (current)
		*current = qxmms_remote_get_playlist_pos(XMMS_SESSION);
}

void MP3_XMMS_CachePlaylistFlush(void) {
	return; // No need for cache with XMMS, we can request song titles directly
}

int MP3_XMMS_CachePlaylist(void) {
	return 0; //No need for cache with XMMS, we can request song titles directly
}


void MP3_XMMS_GetSongTitle(int track_num, char *song, size_t song_len) {
	int playlist_len;
	char *playlist_title;

	strlcpy(song, "", song_len);

	if (!MP3_XMMS_IsPlayerRunning()) 
		return;

	MP3_XMMS_GetPlaylistInfo(NULL, &playlist_len);
	track_num--; // Count from 0 to N
	if (track_num < 0 || track_num > playlist_len)
		return;

	playlist_title = qxmms_remote_get_playlist_title(XMMS_SESSION, track_num);
	strlcpy(song, playlist_title, song_len);
}

void MP3_XMMS_PrintPlaylist_f(void) {
	int current = 0, length = 0, i;
	char *title, *s;

	if (!MP3_XMMS_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
	MP3_XMMS_GetPlaylistInfo(&current, &length);
	for (i = 0 ; i < length; i ++) {
		title = qxmms_remote_get_playlist_title(XMMS_SESSION, i);

		if (i == current)
			for (s = title; *s; s++)
				*s |= 128;

		Com_Printf("%3d %s\n", i + 1, title);
	}
	return;
}

void MP3_XMMS_PlayTrackNum_f(void) {
	int pos, length = 0;

	if (!MP3_XMMS_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <track #>\n", Cmd_Argv(0));
		return;
	}
	MP3_XMMS_GetPlaylistInfo(NULL, &length);
	pos = Q_atoi(Cmd_Argv(1)) - 1;
	if (pos < 0 || pos >= length)
		return;
	qxmms_remote_set_playlist_pos(XMMS_SESSION, pos);
	MP3_XMMS_Play_f();
}

void Media_SetVolume_f(void);
char* Media_GetVolume_f(void);

void MP3_XMMS_Shutdown(void) {
	if (!MP3_XMMS_IsActive())
		return;

	XMMS_FreeLibrary();

	if (XMMS_pid) {
		if (!kill(XMMS_pid, SIGTERM))
			waitpid(XMMS_pid, NULL, 0);
	}

	// Cvar_Delete(mp3_xmms_session.name); <-- impossible to delete static cvars
}

void MP3_XMMS_Init(void) {
	XMMS_LoadLibrary();

	if (!MP3_XMMS_IsActive())
		return;

	if (!Cmd_Exists("mp3_startxmms"))
			Cmd_AddCommand("mp3_startxmms", MP3_Execute_f);

	/* FIXME: This doesnt play nice with multiple initialisations */
	Cvar_SetCurrentGroup(CVAR_GROUP_MP3);
	Cvar_Register(&mp3_xmms_session);
	Cvar_ResetCurrentGroup(); 
}

double Media_XMMS_GetVolume(void) {
	static double vol = 0;

	vol = qxmms_remote_get_main_volume(XMMS_SESSION) / 100.0;

	return vol;
}

void Media_XMMS_SetVolume(double vol) {
	if (!MP3_XMMS_IsPlayerRunning())
		return;

	vol = vol * 100 + 0.5;
 	vol = bound(0, vol, 100);
	qxmms_remote_set_main_volume(XMMS_SESSION, vol);
	return;
}

const mp3_player_t mp3_player_xmms = {
	/* Messages */
	"XMMS",   // PlayerName_AllCaps
	"Xmms",   // PlayerName_LeadingCaps  
	"xmms",   // PlayerName_NoCaps 
	MP3_XMMS,

	/* Functions */
	MP3_XMMS_Init,
	MP3_XMMS_Shutdown,

	MP3_XMMS_IsActive,
	MP3_XMMS_IsPlayerRunning,
	MP3_XMMS_GetStatus,
	MP3_XMMS_GetPlaylistInfo,
	MP3_XMMS_GetSongTitle,
	MP3_XMMS_GetOutputtime,
	MP3_XMMS_GetToggleState,

	MP3_XMMS_PrintPlaylist_f,
	MP3_XMMS_PlayTrackNum_f,
	MP3_XMMS_LoadPlaylist_f,
	MP3_XMMS_CachePlaylist,

	MP3_XMMS_Next_f,
	MP3_XMMS_FastForward_f,
	MP3_XMMS_Rewind_f,
	MP3_XMMS_Prev_f,
	MP3_XMMS_Play_f,
	MP3_XMMS_Pause_f,
	MP3_XMMS_Stop_f,
	MP3_XMMS_Execute_f,
	MP3_XMMS_ToggleRepeat_f, 
	MP3_XMMS_Repeat_f, 
	MP3_XMMS_ToggleShuffle_f, 
	MP3_XMMS_Shuffle_f, 
	MP3_XMMS_FadeOut_f, 

	Media_XMMS_GetVolume,
	Media_XMMS_SetVolume,

	/* Macro's */
	MP3_XMMS_Macro_MP3Info, 
};

#else
const mp3_player_t mp3_player_xmms = {
	"NONE",   // PlayerName_AllCaps  
	"None",   // PlayerName_LeadingCaps
	"none",   // PlayerName_NoCaps 
	MP3_NONE, // Type
};
#endif // WITH_XMMS
