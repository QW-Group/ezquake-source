/*

Copyright (C) 2007      P Archer

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

	$Id: mp3_xmms2.c,v 1.1 2007-10-25 15:04:56 dkure Exp $
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

#ifdef WITH_XMMS2
#include <xmmsclient/xmmsclient.h>

/* Function pointers are used to allow dynamic linking at run time */

static xmmsc_connection_t *(*qxmmsc_init)       (const char *clientname);
static int                 (*qxmmsc_connect)    (xmmsc_connection_t *, const char *);
static void                (*qxmmsc_unref)      (xmmsc_connection_t *c);

// Results
static xmmsc_result_t *(*qxmmsc_medialib_get_info) (xmmsc_connection_t *, uint32_t);

static void (*qxmmsc_result_wait)                   (xmmsc_result_t *res);
static void (*qxmmsc_result_unref)                  (xmmsc_result_t *res);
static const char *(*qxmmsc_result_get_error)       (xmmsc_result_t *res);
static int  (*qxmmsc_result_get_uint)               (xmmsc_result_t *res, uint32_t *r);
static int  (*qxmmsc_result_get_dict_entry_string)  (xmmsc_result_t *res, const char *key, char **r);
static int  (*qxmmsc_result_get_dict_entry_int)     (xmmsc_result_t *res, const char *key, int32_t *r);
static int  (*qxmmsc_result_get_dict_entry_uint)    (xmmsc_result_t *res, const char *key, uint32_t *r);
static int  (*qxmmsc_result_list_next)              (xmmsc_result_t *res);
static int  (*qxmmsc_result_list_valid)             (xmmsc_result_t *res);

// Playback
static xmmsc_result_t *(*qxmmsc_playback_stop)       (xmmsc_connection_t *c);
static xmmsc_result_t *(*qxmmsc_playback_tickle)     (xmmsc_connection_t *c);
static xmmsc_result_t *(*qxmmsc_playback_start)      (xmmsc_connection_t *c);
static xmmsc_result_t *(*qxmmsc_playback_pause)      (xmmsc_connection_t *c);
static xmmsc_result_t *(*qxmmsc_playback_playtime)   (xmmsc_connection_t *c);
static xmmsc_result_t *(*qxmmsc_playback_current_id) (xmmsc_connection_t *c);
static xmmsc_result_t *(*qxmmsc_playback_seek_ms_rel) (xmmsc_connection_t *c, int milliseconds);
static xmmsc_result_t *(*qxmmsc_playback_status)     (xmmsc_connection_t *c);
static xmmsc_result_t *(*qxmmsc_playback_volume_set) (xmmsc_connection_t *c, const char *channel, uint32_t volume);
static xmmsc_result_t *(*qxmmsc_playback_volume_get) (xmmsc_connection_t *c);

// Playlist
static xmmsc_result_t *(*qxmmsc_playlist_list_entries) (xmmsc_connection_t *c, const char *playlist);
static xmmsc_result_t *(*qxmmsc_playlist_set_next)     (xmmsc_connection_t *c, uint32_t);
static xmmsc_result_t *(*qxmmsc_playlist_set_next_rel) (xmmsc_connection_t *c, int32_t);
 
// Errors
static char *(*qxmmsc_get_last_error) (xmmsc_connection_t *c);
static int   (*qxmmsc_result_iserror) (xmmsc_result_t *res);

#define NUM_XMMS2PROCS   (sizeof(xmms2Procs)/sizeof(xmms2Procs[0]))
static qlib_dllfunction_t xmms2Procs[] = {
	{"xmmsc_init", 				(void **) &qxmmsc_init},
	{"xmmsc_connect", 			(void **) &qxmmsc_connect},
	{"xmmsc_unref", 			(void **) &qxmmsc_unref},

	{"xmmsc_medialib_get_info", (void **) &qxmmsc_medialib_get_info},

	{"xmmsc_result_iserror", 	(void **) &qxmmsc_result_iserror},
	{"xmmsc_result_unref",     (void **) &qxmmsc_result_unref},
	{"xmmsc_result_get_error", 	(void **) &qxmmsc_result_get_error},
	{"xmmsc_result_get_uint",	(void **) &qxmmsc_result_get_uint},
	{"xmmsc_result_get_dict_entry_string",(void **) &qxmmsc_result_get_dict_entry_string},
	{"xmmsc_result_get_dict_entry_int",(void **) &qxmmsc_result_get_dict_entry_int},
	{"xmmsc_result_get_dict_entry_uint",(void **) &qxmmsc_result_get_dict_entry_uint},
	{"xmmsc_result_list_next",	(void **) &qxmmsc_result_list_next},
	{"xmmsc_result_list_valid",	(void **) &qxmmsc_result_list_valid},

	{"xmmsc_playback_stop",		(void **) &qxmmsc_playback_stop},
	{"xmmsc_playback_tickle",	(void **) &qxmmsc_playback_tickle},
	{"xmmsc_playback_start",	(void **) &qxmmsc_playback_start},
	{"xmmsc_playback_pause",	(void **) &qxmmsc_playback_pause},
	{"xmmsc_playback_playtime",	(void **) &qxmmsc_playback_playtime},
	{"xmmsc_playback_current_id",(void **) &qxmmsc_playback_current_id},
	{"xmmsc_playback_seek_ms_rel",	(void **) &qxmmsc_playback_seek_ms_rel},
	{"xmmsc_playback_status",	(void **) &qxmmsc_playback_status},
	{"xmmsc_playback_volume_set",	(void **) &qxmmsc_playback_volume_set},
	{"xmmsc_playback_volume_get",	(void **) &qxmmsc_playback_volume_get},

	{"xmmsc_playlist_list_entries",	(void **) &qxmmsc_playlist_list_entries},
	{"xmmsc_playlist_set_next",	(void **) &qxmmsc_playlist_set_next},
	{"xmmsc_playlist_set_next_rel",	(void **) &qxmmsc_playlist_set_next_rel},

	{"xmmsc_result_wait", 		(void **) &qxmmsc_result_wait},
	{"xmmsc_get_last_error", 	(void **) &qxmmsc_get_last_error},
};

static xmmsc_connection_t *connection;
static QLIB_HANDLETYPE_T libxmms2_handle = NULL;

static void XMMS2_FreeLibrary(void) {
	if (libxmms2_handle) {
		QLIB_FREELIBRARY(libxmms2_handle);
	}
	// Maybe need to clear all the function pointers too
}

static void XMMS2_LoadLibrary(void) {
#ifdef _WIN32
	libxmms2_handle = LoadLibrary("libxmmsclient.dll");
#else
	libxmms2_handle = dlopen("libxmmsclient.so", RTLD_NOW);
#endif // _WIN32
	if (!libxmms2_handle)
		return;

	if (!QLib_ProcessProcdef(libxmms2_handle, xmms2Procs, NUM_XMMS2PROCS)) {
		XMMS2_FreeLibrary();
		return;
	}

}

qbool MP3_XMMS2_IsActive(void) {
	return !!libxmms2_handle;;
}

qbool MP3_XMMS2_IsPlayerRunning(void) {
	return !!connection;
}

static void XMMS2_Connect(void) {
	connection = qxmmsc_init("ezquake");
	if (!connection) {
		fprintf(stderr, "OOM!\n");
		return;
	}

	if (!qxmmsc_connect(connection, getenv("XMMS_PATH"))) {
		Com_Printf("Connection failed: %s\n", 
				qxmmsc_get_last_error (connection));
		qxmmsc_unref (connection);
		connection = NULL;
		return;
	}
}

/* TODO: This works succesfully for starting XMMS2d but gives us no 
 * 		 way to shutdown the daemon afterwards */
//static qbool XMMS2_started = false;
void MP3_XMMS2_Execute_f(void) {
	char exec_name[MAX_OSPATH], *argv[2] = {"xmms2-launcher", NULL}/*, **s*/;
	int pid, i;

	if (MP3_XMMS2_IsPlayerRunning()) {
		Com_Printf("XMMS2 is already running\n");
		return;
	}
	strlcpy(exec_name, argv[0], sizeof(exec_name));

	if (!(pid = fork())) { // Child
		execvp(exec_name, argv);
		exit(-1);
	}
	if (pid == -1) {
		Com_Printf ("Couldn't execute xmms2-launcher\n");
		return;
	}
	for (i = 0; i < 6; i++) {
		Sys_MSleep(50);
		XMMS2_Connect();
		if (MP3_XMMS2_IsPlayerRunning()) {
			Com_Printf("XMMS2 is now running\n");
			return;
		}
	}
	Com_Printf("XMMS2 (probably) failed to run\n");
}

static int XMMS2_GetMediaInfo(unsigned int id, char *name, size_t name_len)
{
	xmmsc_result_t *result;
	char *artist, *title;

	if (id == 0) {
		strlcpy(name, "", name_len);
		return -1;
	}

	result = qxmmsc_medialib_get_info (connection, id);
	qxmmsc_result_wait(result);

	if (qxmmsc_result_iserror (result)) {
		Com_DPrintf("medialib get info returns error, %s\n",
				qxmmsc_result_get_error (result));
		qxmmsc_result_unref (result);
		strlcpy(name, "", name_len);
		return -1;
	}

	if (!qxmmsc_result_get_dict_entry_string (result, "artist", &artist)) {
		artist = "No artist";
	}

	if (!qxmmsc_result_get_dict_entry_string (result, "title", &title)) {
		title = "No Title";
	}

	snprintf(name, name_len, "%s - %s", artist, title);

	qxmmsc_result_unref (result);
	return 0;
}

static unsigned int XMMS2_GetCurrentId(void)
{
	xmmsc_result_t *result;
	unsigned int id;

	result = qxmmsc_playback_current_id (connection);
	qxmmsc_result_wait (result);
	if (qxmmsc_result_iserror (result)) {
		Com_DPrintf("playback current id returns error, %s\n",
				qxmmsc_result_get_error (result));
		qxmmsc_result_unref (result);
		return 0;
	}

	if (!qxmmsc_result_get_uint (result, &id)) {
		Com_DPrintf("xmmsc_playback_current_id didn't"
				"return uint as expected\n");
		qxmmsc_result_unref (result);
		return 0;
	}
	qxmmsc_result_unref (result);

	return id;
}

static unsigned int XMMS2_GetCurrentTrack(void) {
	xmmsc_result_t *result;
	unsigned int track_num, current_id, id;

	current_id = XMMS2_GetCurrentId();

	result = qxmmsc_playlist_list_entries(connection, NULL);
	qxmmsc_result_wait(result);

	if (qxmmsc_result_iserror(result)) {
		Com_DPrintf("error when asking for the playlist, %s\n",
				qxmmsc_result_get_error(result));
		qxmmsc_result_unref (result);
		return 1;
	} else {
		for (track_num = 1;qxmmsc_result_list_valid(result); qxmmsc_result_list_next(result), track_num++) {
			if (!qxmmsc_result_get_uint(result, &id)) {
				Com_DPrintf("Couldn't get uint from list\n");
				continue; 
			}
			if (current_id == id)
				return track_num;

		}
	}

	return 1;
}

static unsigned int XMMS2_GetPlaylistLength(void) {
	int length;
	xmmsc_result_t *result;
	
	/* Grab the playlist */
	result = qxmmsc_playlist_list_entries(connection, NULL);
	qxmmsc_result_wait(result);

	if (qxmmsc_result_iserror(result)) {
		Com_DPrintf("error when asking for the playlist, %s\n",
				qxmmsc_result_get_error(result));
		qxmmsc_result_unref (result);
		return 0;
	} 

	/* Count the number of entries */
	for (length = 0;qxmmsc_result_list_valid(result); qxmmsc_result_list_next(result), length++)
		; // We are just counting
	return length;
}

static unsigned int XMMS2_GetCurrentPlayTime(void)
{
	xmmsc_result_t *result;
	unsigned int playtime;

	result = qxmmsc_playback_playtime(connection);
	qxmmsc_result_wait(result);
	if (qxmmsc_result_iserror (result)) {
		Com_DPrintf("playback playtime returned error, %s",
				qxmmsc_result_get_error (result));
		qxmmsc_result_unref (result);
		return 0;
	}

	if (!qxmmsc_result_get_uint(result, &playtime)) {
		Com_DPrintf("Couldn't get uint from playback\n");
		qxmmsc_result_unref (result);
		return 0;
	}

	qxmmsc_result_unref (result);
	return playtime;
}

static int XMMS2_GetCurrentDuration(void) 
{
	xmmsc_result_t *result;
	unsigned int id;
	int duration;

	id = XMMS2_GetCurrentId();
	if (id == 0)
		return 0;
	result = qxmmsc_medialib_get_info (connection, id);
	qxmmsc_result_wait (result);

	if (qxmmsc_result_iserror (result)) {
		Com_DPrintf("medialib get info returns error, %s\n",
				qxmmsc_result_get_error (result));
		qxmmsc_result_unref (result);
		return 0;
	}

	if (!qxmmsc_result_get_dict_entry_int(result, "duration", &duration)) {
		duration = 0; 
	}
	qxmmsc_result_unref (result);

	return duration;
}

int MP3_XMMS2_GetStatus(void) {
	int status;
	unsigned int xmms2_status;
	xmmsc_result_t *result;

	if (!MP3_XMMS2_IsPlayerRunning())
		return MP3_NOTRUNNING;

	result = qxmmsc_playback_status(connection);
	qxmmsc_result_wait(result);
	if (qxmmsc_result_iserror (result)) {
		Com_DPrintf("playback status returned error, %s",
				qxmmsc_result_get_error (result));
		qxmmsc_result_unref (result);
		return MP3_NOTRUNNING;
	}

	qxmmsc_result_get_uint(result, &xmms2_status);
	qxmmsc_result_unref (result);

	switch(xmms2_status) {
		case XMMS_PLAYBACK_STATUS_PLAY:  status = MP3_PLAYING; break;
		case XMMS_PLAYBACK_STATUS_PAUSE: status = MP3_PAUSED;  break;
		case XMMS_PLAYBACK_STATUS_STOP:  status = MP3_STOPPED; break;
		default:
			status = MP3_NOTRUNNING;
	}

	return status;
}

#define XMMS2_COMMAND(Name, Param)										\
	void MP3_XMMS2_##Name##_f(void) 									\
	{																	\
		if (MP3_XMMS2_IsPlayerRunning()) {								\
			xmmsc_result_t *result;										\
			result = qxmmsc_playback_##Param(connection);				\
			qxmmsc_result_wait(result);									\
			if (qxmmsc_result_iserror (result)) {						\
				Com_DPrintf("playback ##Param## returned error, %s",	\
						qxmmsc_result_get_error (result));				\
			}															\
			qxmmsc_result_unref (result);								\
		} else {														\
			Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps); 	\
		}																\
		return;															\
    }

XMMS2_COMMAND(Play, start);
XMMS2_COMMAND(Pause, pause);
XMMS2_COMMAND(Stop, stop);
XMMS2_COMMAND(FadeOut, stop); /* TODO */

static void XMMS2_PlayList_SetNext(int absolute) {
	xmmsc_result_t *result;

	if (!MP3_XMMS2_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
	result = qxmmsc_playlist_set_next(connection, absolute);
	qxmmsc_result_wait(result);
	if (qxmmsc_result_iserror (result)) {
		Com_DPrintf("playlist set next returned error, %s",
				qxmmsc_result_get_error (result));
	}
	qxmmsc_result_unref (result);

	/* Start playing the playists song */
	result = qxmmsc_playback_tickle(connection);
	qxmmsc_result_wait(result);
	if (qxmmsc_result_iserror (result)) {
		Com_DPrintf("playback tickle returned error, %s",
				qxmmsc_result_get_error (result));
	}
	qxmmsc_result_unref (result);

	/* If playback is stopped we need to force play */
	if (MP3_XMMS2_GetStatus() == MP3_STOPPED) {
		MP3_Play_f();
	}
}

static void XMMS2_PlayList_SetNextRel(int offset) {
	xmmsc_result_t *result;

	if (!MP3_XMMS2_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
	result = qxmmsc_playlist_set_next_rel(connection, offset);
	qxmmsc_result_wait(result);
	if (qxmmsc_result_iserror (result)) {
		Com_DPrintf("playlist set next returned error, %s",
				qxmmsc_result_get_error (result));
	}
	qxmmsc_result_unref (result);

	/* Start playing the playists song */
	result = qxmmsc_playback_tickle(connection);
	qxmmsc_result_wait(result);
	if (qxmmsc_result_iserror (result)) {
		Com_DPrintf("playback tickle returned error, %s",
				qxmmsc_result_get_error (result));
	}
	qxmmsc_result_unref (result);
}

void MP3_XMMS2_Prev_f(void) {
	XMMS2_PlayList_SetNextRel(-1);
}

void MP3_XMMS2_Next_f(void) {
	XMMS2_PlayList_SetNextRel(1);
}

static void XMMS2_SeekRel(int offset) {
	xmmsc_result_t *result;

	if (!MP3_XMMS2_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
	result = qxmmsc_playback_seek_ms_rel(connection, offset);
	qxmmsc_result_wait(result);
	if (qxmmsc_result_iserror (result)) {
		Com_DPrintf("playback seek returned error, %s",
				qxmmsc_result_get_error (result));
	}
	qxmmsc_result_unref (result);
}

void MP3_XMMS2_FastForward_f(void) {
	XMMS2_SeekRel(5 * 1000);
}

void MP3_XMMS2_Rewind_f(void) {
	XMMS2_SeekRel(-5 * 1000);
}

/* TODO: D-Kure: I have no idea how to turn repeat on or off */
void MP3_XMMS2_Repeat_f(void) {
	if (!MP3_XMMS2_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
}

/* TODO: Shuffling is done by shuffling the whole playlist... */
void MP3_XMMS2_Shuffle_f(void) {
	if (!MP3_XMMS2_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
}

/* TODO: see MP3_XMMS2_Repeat_f() */
void MP3_XMMS2_ToggleRepeat_f(void) {
	if (!MP3_XMMS2_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
}

/* TODO: see MP3_XMMS2_Shuffle_f() */
void MP3_XMMS2_ToggleShuffle_f(void) {
	if (!MP3_XMMS2_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
}

char *MP3_XMMS2_Macro_MP3Info(void) {
	static char song[MP3_MAXSONGTITLE];
	unsigned int id;

	if (!MP3_XMMS2_IsPlayerRunning()) {
		Com_Printf("XMMS not running\n");
		return "";
	}

	id = XMMS2_GetCurrentId();

	XMMS2_GetMediaInfo(id, song, sizeof(song));

	return song;
}

qbool MP3_XMMS2_GetOutputtime(int *elapsed, int *total) {
	if (!MP3_XMMS2_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return false;
	}

	if (elapsed) 
		*elapsed = XMMS2_GetCurrentPlayTime() / 1000.0;
	if (total)
		*total = XMMS2_GetCurrentDuration() / 1000.0;

	return true;
}

/* TODO */
qbool MP3_XMMS2_GetToggleState(int *shuffle, int *repeat) {
	if (!MP3_XMMS2_IsPlayerRunning()) 
		return false;
	if (shuffle)
		*shuffle = false;
	if (repeat)
		*repeat = false;

	return true;
}

/* TODO */
void MP3_XMMS2_LoadPlaylist_f(void) {
	Com_Printf("The %s command is not (yet) supported.\n", Cmd_Argv(0));
}

/**
 * Return the current songs index and also the number of entries in the playlist
 */
void MP3_XMMS2_GetPlaylistInfo(int *current, int *length) {
	if (!MP3_XMMS2_IsPlayerRunning()) 
		return;

	if (current) 
		*current = XMMS2_GetCurrentTrack() - 1; // Expects an index

	if (length) {
		*length = XMMS2_GetPlaylistLength();
	}
}

// Caching may not be needed
int MP3_XMMS2_CachePlaylist(void) {
	return 0;
}

void MP3_XMMS2_GetSongTitle(int track_num, char *song, size_t song_len) {
	xmmsc_result_t *result;
	int count;

	strlcpy(song, "", song_len);

	if (!MP3_XMMS2_IsPlayerRunning()) 
		return;

	/* Need to get the whole playlist */
	result = qxmmsc_playlist_list_entries(connection, NULL);
	qxmmsc_result_wait(result);

	if (qxmmsc_result_iserror(result)) {
		Com_DPrintf("error when asking for the playlist, %s\n",
				qxmmsc_result_get_error(result));
		qxmmsc_result_unref (result);
		return;
	} 

	/* Search threw the playlist for the track number */
	for (count = 0;qxmmsc_result_list_valid(result); qxmmsc_result_list_next(result), count++) {
		if (count == track_num - 1) {
			unsigned int id;
			if (!qxmmsc_result_get_uint(result, &id)) {
				Com_DPrintf("Couldn't get uint from list\n");
				continue; 
			}

			XMMS2_GetMediaInfo(id, song, song_len);

			return;
		}
	}
}

void MP3_XMMS2_PrintPlaylist_f(void) {
	xmmsc_result_t *result;
	unsigned int id;
	char song[MP3_MAXSONGTITLE];
	int current_song;
	int count;

	if (!MP3_XMMS2_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}

	MP3_XMMS2_GetPlaylistInfo(&current_song, NULL);

	result = qxmmsc_playlist_list_entries(connection, NULL);
	qxmmsc_result_wait(result);

	if (qxmmsc_result_iserror(result)) {
		Com_DPrintf("error when asking for the playlist, %s\n",
				qxmmsc_result_get_error(result));
		qxmmsc_result_unref (result);
		return;
	}

	for (count = 1;qxmmsc_result_list_valid(result); qxmmsc_result_list_next(result), count++) {
		if (!qxmmsc_result_get_uint(result, &id)) {
			Com_DPrintf("Couldn't get uint from list\n");
			continue; 
		}

		XMMS2_GetMediaInfo(id, song, sizeof(song));

		/* Make the current song brown */
		if (count - 1 == current_song) { // current_song is an index
			char *s;
			for (s = song; *s; s++)
				*s |= 128;
		}

		Com_Printf("%3d %s\n", count, song);
	}
	qxmmsc_result_unref(result);
}

/**
 * Set the current track to the given argument
 */
void MP3_XMMS2_PlayTrackNum_f(void) {
	int track_num;

	if (!MP3_XMMS2_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <track #>\n", Cmd_Argv(0));
		return;
	}

	track_num = Q_atoi(Cmd_Argv(1)) - 1;	// Count from zero
	XMMS2_PlayList_SetNext(track_num);
}

void Media_SetVolume_f(void);
char* Media_GetVolume_f(void);

void MP3_XMMS2_Shutdown(void) {
//	if (XMMS2_started) {
//		/* TODO: Send quit command */
//		if (!kill(XMMS2_pid, SIGTERM))
//			waitpid(XMMS2_pid, NULL, 0);
//	}

	if (!MP3_XMMS2_IsPlayerRunning())
		return;
	qxmmsc_unref (connection);
	connection = NULL;
}

void MP3_XMMS2_Init(void) {
	XMMS2_LoadLibrary();

	if (!MP3_XMMS2_IsActive())
		return;

	XMMS2_Connect();

	if (!Cmd_Exists("mp3_startxmms2"))
			Cmd_AddCommand("mp3_startxmms2", MP3_Execute_f);
}

/* XXX: The channels may be called other names on different computers */
double Media_XMMS2_GetVolume(void) {
	xmmsc_result_t *result;
	static char *channels[] = {"left", "right", NULL};
	char **channel;
	int channel_count;
	double vol;

	if (!MP3_XMMS2_IsPlayerRunning())
		return 0.0;

	result = qxmmsc_playback_volume_get (connection);
	qxmmsc_result_wait(result);
	if (qxmmsc_result_iserror (result)) {
		Com_DPrintf("playback volume returns error, %s\n",
				qxmmsc_result_get_error (result));
		qxmmsc_result_unref (result);
		return 0.0;
	}

	for (channel = channels, channel_count = 0, vol = 0.0; *channel; channel++) {
		unsigned int temp;
		if (!qxmmsc_result_get_dict_entry_uint(result, *channel, &temp)) {
			continue;
		} else {
			vol += temp;
			channel_count++;
		}
	}
	qxmmsc_result_unref (result);

	vol = vol / channel_count; // Take the average of the channels

	return vol / 100.0;
}

/* XXX: The channels may be called other names on different computers */
void Media_XMMS2_SetVolume(double vol) {
	xmmsc_result_t *result;
	static char *channels[] = {"left", "right", NULL};
	char **channel;

	if (!MP3_XMMS2_IsPlayerRunning())
		return;

	vol = vol * 100 + 0.5;
 	vol = bound(0, vol, 100);

	/* Set each channel to the given volume */
	for (channel = channels; *channel; channel++) {
		result = qxmmsc_playback_volume_set(connection, *channel, vol);
		qxmmsc_result_wait(result);
		if (qxmmsc_result_iserror (result)) {
			Com_DPrintf("medialib get info returns error, %s\n",
					qxmmsc_result_get_error (result));
			qxmmsc_result_unref (result);
			return;
		}
		qxmmsc_result_unref (result);
	}
}

const mp3_player_t mp3_player_xmms2 = {
	/* Messages */
	"XMMS2",   // PlayerName_AllCaps
	"Xmms2",   // PlayerName_LeadingCaps  
	"xmms2",   // PlayerName_NoCaps 
	MP3_XMMS2,

	/* Functions */
	MP3_XMMS2_Init,
	MP3_XMMS2_Shutdown,

	MP3_XMMS2_IsActive,
	MP3_XMMS2_IsPlayerRunning,
	MP3_XMMS2_GetStatus,
	MP3_XMMS2_GetPlaylistInfo,
	MP3_XMMS2_GetSongTitle,
	MP3_XMMS2_GetOutputtime,
	MP3_XMMS2_GetToggleState,

	MP3_XMMS2_PrintPlaylist_f,
	MP3_XMMS2_PlayTrackNum_f,
	MP3_XMMS2_LoadPlaylist_f,
	MP3_XMMS2_CachePlaylist,
	MP3_XMMS2_Next_f,
	MP3_XMMS2_FastForward_f,
	MP3_XMMS2_Rewind_f,
	MP3_XMMS2_Prev_f,
	MP3_XMMS2_Play_f,
	MP3_XMMS2_Pause_f,
	MP3_XMMS2_Stop_f,
	MP3_XMMS2_Execute_f,
	MP3_XMMS2_ToggleRepeat_f, 	// Not supported
	MP3_XMMS2_Repeat_f, 		// Not supported
	MP3_XMMS2_ToggleShuffle_f, 	// Not supported
	MP3_XMMS2_Shuffle_f, 		// Not supported
	MP3_XMMS2_FadeOut_f, 		// Just does the same as stop

	Media_XMMS2_GetVolume,
	Media_XMMS2_SetVolume,

	/* Macro's */
	MP3_XMMS2_Macro_MP3Info, 
};

#else
const mp3_player_t mp3_player_xmms2 = {
	"NONE",   // PlayerName_AllCaps  
	"None",   // PlayerName_LeadingCaps
	"none",   // PlayerName_NoCaps 
	MP3_NONE, // Type
};
#endif // WITH_XMMS2

