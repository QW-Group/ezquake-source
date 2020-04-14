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

#ifdef WITH_AUDACIOUS
#include <audacious/dbus.h>
#include <audacious/audctrl.h>

DBusGProxy *audacious_proxy;

static QLIB_HANDLETYPE_T libaud_handle = NULL;

static void (*qaud_remote_play)(DBusGProxy *proxy);
static void (*qaud_remote_pause)(DBusGProxy *proxy);
static void (*qaud_remote_stop)(DBusGProxy *proxy);
static void (*qaud_remote_jump_to_time)(DBusGProxy *proxy, gint pos);
static gboolean (*qaud_remote_is_running)(DBusGProxy *proxy);
static gboolean (*qaud_remote_is_playing)(DBusGProxy *proxy);
static gboolean (*qaud_remote_is_paused)(DBusGProxy *proxy);
static gboolean (*qaud_remote_is_repeat)(DBusGProxy *proxy);
static gboolean (*qaud_remote_is_shuffle)(DBusGProxy *proxy);
static gint (*qaud_remote_get_playlist_pos)(DBusGProxy *proxy);
static void (*qaud_remote_set_playlist_pos)(DBusGProxy *proxy, gint pos);
gint (*qaud_remote_get_playlist_length)(DBusGProxy *proxy);
gchar *(*qaud_remote_get_playlist_title)(DBusGProxy *proxy, gint pos);
static void (*qaud_remote_playlist_prev)(DBusGProxy *proxy);
static void (*qaud_remote_playlist_next)(DBusGProxy *proxy);
static gint (*qaud_remote_get_output_time)(DBusGProxy *proxy);
static gint (*qaud_remote_get_playlist_time)(DBusGProxy *proxy, gint pos);
static gint (*qaud_remote_get_main_volume)(DBusGProxy *proxy);
static void (*qaud_remote_set_main_volume)(DBusGProxy *proxy, gint v);
static void (*qaud_remote_toggle_repeat)(DBusGProxy *proxy);
static void (*qaud_remote_toggle_shuffle)(DBusGProxy *proxy);
static void (*qg_free)(gpointer);
static void (*qg_type_init)(void);
static DBusGConnection* (*qdbus_g_bus_get)(DBusBusType type, GError **error);
static DBusGProxy*      (*qdbus_g_proxy_new_for_name) (
		DBusGConnection *connection,
		const char        *name,
		const char        *path,
		const char        *interface);


#define NUM_AUDPROCS	(sizeof(xmmsProcs)/sizeof(xmmsProcs[0]))

static qlib_dllfunction_t xmmsProcs[] = {
	{"audacious_remote_play", (void **) &qaud_remote_play},
	{"audacious_remote_pause", (void **) &qaud_remote_pause},
	{"audacious_remote_stop", (void **) &qaud_remote_stop},

	{"audacious_remote_jump_to_time", (void **) &qaud_remote_jump_to_time},

	{"audacious_remote_is_running", (void **) &qaud_remote_is_running},
	{"audacious_remote_is_playing", (void **) &qaud_remote_is_playing},
	{"audacious_remote_is_paused", (void **) &qaud_remote_is_paused},
	{"audacious_remote_is_repeat", (void **) &qaud_remote_is_repeat},
	{"audacious_remote_is_shuffle", (void **) &qaud_remote_is_shuffle},

	{"audacious_remote_get_playlist_pos", (void **) &qaud_remote_get_playlist_pos},
	{"audacious_remote_set_playlist_pos", (void **) &qaud_remote_set_playlist_pos},
	{"audacious_remote_get_playlist_length", (void **) &qaud_remote_get_playlist_length},
	{"audacious_remote_get_playlist_title", (void **) &qaud_remote_get_playlist_title},

	{"audacious_remote_playlist_prev", (void **) &qaud_remote_playlist_prev},
	{"audacious_remote_playlist_next", (void **) &qaud_remote_playlist_next},

	{"audacious_remote_get_output_time", (void **) &qaud_remote_get_output_time},
	{"audacious_remote_get_playlist_time", (void **) &qaud_remote_get_playlist_time},
	{"audacious_remote_get_main_volume", (void **) &qaud_remote_get_main_volume},
	{"audacious_remote_set_main_volume", (void **) &qaud_remote_set_main_volume},

	{"audacious_remote_toggle_repeat",  (void **) &qaud_remote_toggle_repeat},
	{"audacious_remote_toggle_shuffle", (void **) &qaud_remote_toggle_shuffle},
	{"g_free",                          (void **) &qg_free},
	{"g_type_init",						(void **) &qg_type_init},
	{"dbus_g_bus_get",					(void **) &qdbus_g_bus_get},
	{"dbus_g_proxy_new_for_name",		(void **) &qdbus_g_proxy_new_for_name},
};

static void AUDACIOUS_FreeLibrary(void) {
	if (libaud_handle) {
		QLIB_FREELIBRARY(libaud_handle);
	}
	// Maybe need to clear all the function pointers too
}

static void Audacious_LoadLibrary(void) {
	DBusGConnection *connection;
	GError *error;

	if (!(libaud_handle = dlopen("libaudclient.so", RTLD_NOW)))
		return;

	if (!QLib_ProcessProcdef(libaud_handle, xmmsProcs, NUM_AUDPROCS)) {
		goto fail;
	}

	qg_type_init();
	error = NULL;
	connection = qdbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (connection == NULL) {
		Com_Printf("Failed to open connection to bus: %s\n", error->message);
		goto fail;
	}
	audacious_proxy = qdbus_g_proxy_new_for_name (connection,
			AUDACIOUS_DBUS_SERVICE,
			AUDACIOUS_DBUS_PATH,
			AUDACIOUS_DBUS_INTERFACE);
	return;

fail:
	AUDACIOUS_FreeLibrary();
	return;
}

qbool MP3_AUDACIOUS_IsActive(void) {
//	return !!audacious_proxy;
	return !!libaud_handle;
}

qbool MP3_AUDACIOUS_IsPlayerRunning(void) {
	return (qaud_remote_is_running(audacious_proxy));
}

static int AUDACIOUS_pid = 0;
void MP3_AUDACIOUS_Execute_f(void) {
	char exec_name[MAX_OSPATH], *argv[2] = {"audacious", NULL}/*, **s*/;
	int i; 

	if (MP3_AUDACIOUS_IsPlayerRunning()) {
		Com_Printf("Audacious is already running\n");
		return;
	}
	strlcpy(exec_name, argv[0], sizeof(exec_name));

	if (!(AUDACIOUS_pid = fork())) { // Child
		execvp(exec_name, argv);
		exit(-1);
	}
	if (AUDACIOUS_pid == -1) {
		Com_Printf ("Couldn't execute Audacious\n");
		return;
	}
	for (i = 0; i < 6; i++) {
		Sys_MSleep(50);
		if (MP3_AUDACIOUS_IsPlayerRunning()) {
			Com_Printf("Audacious is now running\n");
			return;
		}
	}
	Com_Printf("Audacious (probably) failed to run\n");
}

#define AUDACIOUS_COMMAND(Name, Param)										\
    void MP3_AUDACIOUS_##Name##_f(void) {									\
	   if (MP3_AUDACIOUS_IsPlayerRunning()) {								\
		   qaud_remote_##Param(audacious_proxy);							\
	   } else {															\
		   Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps); 	\
	   }																\
	   return;															\
    }

AUDACIOUS_COMMAND(Prev, playlist_prev);
AUDACIOUS_COMMAND(Play, play);
AUDACIOUS_COMMAND(Pause, pause);
AUDACIOUS_COMMAND(Stop, stop);
AUDACIOUS_COMMAND(Next, playlist_next);
AUDACIOUS_COMMAND(FadeOut, stop);

void MP3_AUDACIOUS_FastForward_f(void) {
	int current;

	if (!MP3_AUDACIOUS_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
	current = qaud_remote_get_output_time(audacious_proxy) + 5 * 1000;
	qaud_remote_jump_to_time(audacious_proxy, current);
}

void MP3_AUDACIOUS_Rewind_f(void) {
	int current;

	if (!MP3_AUDACIOUS_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
	current = qaud_remote_get_output_time(audacious_proxy) - 5 * 1000;
	current = max(0, current);
	qaud_remote_jump_to_time(audacious_proxy, current);
}

int MP3_AUDACIOUS_GetStatus(void) {
	if (!MP3_AUDACIOUS_IsPlayerRunning())
		return MP3_NOTRUNNING;
	if (qaud_remote_is_paused(audacious_proxy))
		return MP3_PAUSED;
	if (qaud_remote_is_playing(audacious_proxy))
		return MP3_PLAYING;	
	return MP3_STOPPED;
}

static void AUDACIOUS_Set_ToggleFn(char *name, void *togglefunc, void *getfunc) {
	int ret, set;
	gboolean (*xmms_togglefunc)(DBusGProxy *proxy) = togglefunc;
	gboolean (*xmms_getfunc)(DBusGProxy *proxy) = getfunc;

	if (!MP3_AUDACIOUS_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
	if (Cmd_Argc() >= 3) {
		Com_Printf("Usage: %s [on|off|toggle]\n", Cmd_Argv(0));
		return;
	}
	ret = xmms_getfunc(audacious_proxy);
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
		xmms_togglefunc(audacious_proxy);
	else if (!set && ret)
		xmms_togglefunc(audacious_proxy);
	Com_Printf("%s set to %s\n", name, set ? "on" : "off");
}

void MP3_AUDACIOUS_Repeat_f(void) {
	AUDACIOUS_Set_ToggleFn("Repeat", qaud_remote_toggle_repeat, qaud_remote_is_repeat);
}

void MP3_AUDACIOUS_Shuffle_f(void) {
	AUDACIOUS_Set_ToggleFn("Shuffle", qaud_remote_toggle_shuffle, qaud_remote_is_shuffle);
}

void MP3_AUDACIOUS_ToggleRepeat_f(void) {
	if (!MP3_AUDACIOUS_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
	qaud_remote_toggle_repeat(audacious_proxy);
}

void MP3_AUDACIOUS_ToggleShuffle_f(void) {
	if (!MP3_AUDACIOUS_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
	qaud_remote_toggle_shuffle(audacious_proxy);
}

char *MP3_AUDACIOUS_Macro_MP3Info(void) {
	int playlist_pos;
	char *s;
	static char title[MP3_MAXSONGTITLE];

	if (!MP3_AUDACIOUS_IsPlayerRunning()) {
		Com_Printf("AUDACIOUS not running\n");
		return NULL;
	}
	playlist_pos = qaud_remote_get_playlist_pos(audacious_proxy);
	s = qaud_remote_get_playlist_title(audacious_proxy, playlist_pos);
	strlcpy(title, s ? s : "", sizeof(title));
	//g_free(s);
	qg_free(s);
	return title;
}

qbool MP3_AUDACIOUS_GetOutputtime(int *elapsed, int *total) {
	int pos;

	if (!MP3_AUDACIOUS_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return false;
	}
	pos = qaud_remote_get_playlist_pos(audacious_proxy);
	*elapsed = qaud_remote_get_output_time(audacious_proxy) / 1000.0;
	*total = qaud_remote_get_playlist_time(audacious_proxy, pos) / 1000.0;
	return true;
}

qbool MP3_AUDACIOUS_GetToggleState(int *shuffle, int *repeat) {
	if (!MP3_AUDACIOUS_IsPlayerRunning()) 
		return false;
	*shuffle = qaud_remote_is_shuffle(audacious_proxy);
	*repeat = qaud_remote_is_repeat(audacious_proxy);
	return true;
}


// Playlist functions
void MP3_AUDACIOUS_LoadPlaylist_f(void) {
	Com_Printf("The %s command is not (yet) supported.\n", Cmd_Argv(0));
}

void MP3_AUDACIOUS_GetPlaylistInfo(int *current, int *length) {
	if (!MP3_AUDACIOUS_IsPlayerRunning()) 
		return;
	if (length)
		*length = qaud_remote_get_playlist_length(audacious_proxy);
	if (current)
		*current = qaud_remote_get_playlist_pos(audacious_proxy);
}

void MP3_AUDACIOUS_CachePlaylistFlush(void) {
	return; // No need for cache with AUDACIOUS, we can request song titles directly
}

int MP3_AUDACIOUS_CachePlaylist(void) {
	return 0; //No need for cache with AUDACIOUS, we can request song titles directly
}


void MP3_AUDACIOUS_GetSongTitle(int track_num, char *song, size_t song_len) {
	int playlist_len;
	char *playlist_title;

	strlcpy(song, "", song_len);

	if (!MP3_AUDACIOUS_IsPlayerRunning()) 
		return;

	MP3_AUDACIOUS_GetPlaylistInfo(NULL, &playlist_len);
	track_num--; // Count from 0 to N
	if (track_num < 0 || track_num > playlist_len)
		return;

	playlist_title = qaud_remote_get_playlist_title(audacious_proxy, track_num);
	strlcpy(song, playlist_title, song_len);
}

void MP3_AUDACIOUS_PrintPlaylist_f(void) {
	int current = 0, length = 0, i;
	char *title, *s;

	if (!MP3_AUDACIOUS_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
	MP3_AUDACIOUS_GetPlaylistInfo(&current, &length);
	for (i = 0 ; i < length; i ++) {
		title = qaud_remote_get_playlist_title(audacious_proxy, i);

		if (i == current)
			for (s = title; *s; s++)
				*s |= 128;

		Com_Printf("%3d %s\n", i + 1, title);
	}
	return;
}

void MP3_AUDACIOUS_PlayTrackNum_f(void) {
	int pos, length = 0;

	if (!MP3_AUDACIOUS_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <track #>\n", Cmd_Argv(0));
		return;
	}
	MP3_AUDACIOUS_GetPlaylistInfo(NULL, &length);
	pos = Q_atoi(Cmd_Argv(1)) - 1;
	if (pos < 0 || pos >= length)
		return;
	qaud_remote_set_playlist_pos(audacious_proxy, pos);
	MP3_AUDACIOUS_Play_f();
}

void Media_SetVolume_f(void);
char* Media_GetVolume_f(void);

void MP3_AUDACIOUS_Shutdown(void) {
	if (!MP3_AUDACIOUS_IsActive())
		return;

	AUDACIOUS_FreeLibrary();

	if (AUDACIOUS_pid) {
		if (!kill(AUDACIOUS_pid, SIGTERM))
			waitpid(AUDACIOUS_pid, NULL, 0);
	}

	// Cvar_Delete(mp3_xmms_session.name); <-- impossible to delete static cvars
}

void MP3_AUDACIOUS_Init(void) {
	Audacious_LoadLibrary();

	if (!MP3_AUDACIOUS_IsActive())
		return;

	if (!Cmd_Exists("mp3_startaudacious"))
		Cmd_AddCommand("mp3_startaudacious", MP3_Execute_f);
}

double Media_AUDACIOUS_GetVolume(void) {
	static double vol = 0.5;

	vol = qaud_remote_get_main_volume(audacious_proxy) / 100.0;

	return vol;
}

void Media_AUDACIOUS_SetVolume(double vol) {
	if (!MP3_AUDACIOUS_IsPlayerRunning())
		return;

	vol = vol * 100 + 0.5;
 	vol = bound(0, vol, 100);
	qaud_remote_set_main_volume(audacious_proxy, vol);
	return;
}

const mp3_player_t mp3_player_audacious = {
	/* Messages */
	"AUDACIOUS", // PlayerName_AllCaps
	"Audacious", // PlayerName_LeadingCaps  
	"audacious", // PlayerName_NoCaps       
	MP3_AUDACIOUS, // Type                    

	/* Functions */
	MP3_AUDACIOUS_Init, 
	MP3_AUDACIOUS_Shutdown, 

	MP3_AUDACIOUS_IsActive, 
	MP3_AUDACIOUS_IsPlayerRunning, 
	MP3_AUDACIOUS_GetStatus, 
	MP3_AUDACIOUS_GetPlaylistInfo, 
	MP3_AUDACIOUS_GetSongTitle,
	MP3_AUDACIOUS_GetOutputtime, 
	MP3_AUDACIOUS_GetToggleState, 

	MP3_AUDACIOUS_PrintPlaylist_f, 
	MP3_AUDACIOUS_PlayTrackNum_f, 
	MP3_AUDACIOUS_LoadPlaylist_f,
	MP3_AUDACIOUS_CachePlaylist,
	MP3_AUDACIOUS_Next_f, 
	MP3_AUDACIOUS_FastForward_f, 
	MP3_AUDACIOUS_Rewind_f, 
	MP3_AUDACIOUS_Prev_f, 
	MP3_AUDACIOUS_Play_f, 
	MP3_AUDACIOUS_Pause_f, 
	MP3_AUDACIOUS_Stop_f, 
	MP3_AUDACIOUS_Execute_f, 
	MP3_AUDACIOUS_ToggleRepeat_f, 
	MP3_AUDACIOUS_Repeat_f, 
	MP3_AUDACIOUS_ToggleShuffle_f, 
	MP3_AUDACIOUS_Shuffle_f, 
	MP3_AUDACIOUS_FadeOut_f, 

	Media_AUDACIOUS_GetVolume,
	Media_AUDACIOUS_SetVolume,

	/* Macro's */
	MP3_AUDACIOUS_Macro_MP3Info, 
};

#else
const mp3_player_t mp3_player_audacious = {
	"NONE",   // PlayerName_AllCaps  
	"None",   // PlayerName_LeadingCaps
	"none",   // PlayerName_NoCaps 
	MP3_NONE, // Type
};
#endif // WITH_AUDACIOUS
