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

	$Id: mp3_mpd.c,v 1.1 2007-10-26 07:55:44 dkure Exp $
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

#ifdef WITH_MPD
#include <libmpd/libmpd.h>

/* Function pointers are used to allow dynamic linking at run time */

// Connections
static mpd_Connection *(*qmpd_newConnection) (const char * host, int port, float timeout);
static void (*qmpd_closeConnection)  (mpd_Connection * connection);

// Status
static void (*qmpd_sendStatusCommand) (mpd_Connection * connection);
static mpd_Status *(*qmpd_getStatus)  (mpd_Connection * connection);
static void (*qmpd_freeStatus)        (mpd_Status * status);

// Commands
static void (*qmpd_sendPlayCommand)  (mpd_Connection * connection, int songNum);
static void (*qmpd_sendPauseCommand) (mpd_Connection * connection, int pauseMode);
static void (*qmpd_sendStopCommand)  (mpd_Connection * connection);
static void (*qmpd_sendNextCommand)  (mpd_Connection * connection);
static void (*qmpd_sendPrevCommand)  (mpd_Connection * connection);
static void (*qmpd_sendSeekCommand)  (mpd_Connection * connection, int song, int time);
static void (*qmpd_sendRepeatCommand)(mpd_Connection * connection, int repeatMode);
static void (*qmpd_sendRandomCommand)(mpd_Connection * connection, int randomMode);
static void (*qmpd_sendSetvolCommand)(mpd_Connection * connection, int volumeChange);
static void (*qmpd_finishCommand)     (mpd_Connection * connection);

// Playlist
static void (*qmpd_sendPlaylistInfoCommand)      (mpd_Connection * connection, int songNum);
static mpd_InfoEntity *(*qmpd_getNextInfoEntity) (mpd_Connection * connection);

#define NUM_MPDPROCS   (sizeof(mpdProcs)/sizeof(mpdProcs[0]))
static qlib_dllfunction_t mpdProcs[] = {
	{"mpd_newConnection", 		(void **) &qmpd_newConnection},
	{"mpd_closeConnection", 	(void **) &qmpd_closeConnection},
	{"mpd_sendStatusCommand", 	(void **) &qmpd_sendStatusCommand},
	{"mpd_getStatus", 			(void **) &qmpd_getStatus},
	{"mpd_freeStatus", 			(void **) &qmpd_freeStatus},
	{"mpd_sendPlayCommand", 	(void **) &qmpd_sendPlayCommand},
	{"mpd_sendPauseCommand", 	(void **) &qmpd_sendPauseCommand},
	{"mpd_sendStopCommand", 	(void **) &qmpd_sendStopCommand},
	{"mpd_sendNextCommand", 	(void **) &qmpd_sendNextCommand},
	{"mpd_sendPrevCommand", 	(void **) &qmpd_sendPrevCommand},
	{"mpd_sendSeekCommand", 	(void **) &qmpd_sendSeekCommand},
	{"mpd_sendRepeatCommand", 	(void **) &qmpd_sendRepeatCommand},
	{"mpd_sendRandomCommand", 	(void **) &qmpd_sendRandomCommand},
	{"mpd_sendSetvolCommand", 	(void **) &qmpd_sendSetvolCommand},
	{"mpd_finishCommand", 		(void **) &qmpd_finishCommand},
	{"mpd_sendPlaylistInfoCommand", (void **) &qmpd_sendPlaylistInfoCommand},
	{"mpd_getNextInfoEntity", 	(void **) &qmpd_getNextInfoEntity},
};

static mpd_Connection *connection;
static QLIB_HANDLETYPE_T libmpd_handle = NULL;

static void MPD_FreeLibrary(void) {
	if (libmpd_handle) {
		QLIB_FREELIBRARY(libmpd_handle);
	}
	// Maybe need to clear all the function pointers too
}

static void MPD_LoadLibrary(void) {
#ifdef _WIN32
	libmpd_handle = LoadLibrary("libmpd.dll");
#else
	libmpd_handle = dlopen("libmpd.so", RTLD_NOW);
#endif // _WIN32
	if (!libmpd_handle)
		return;

	if (!QLib_ProcessProcdef(libmpd_handle, mpdProcs, NUM_MPDPROCS)) {
		MPD_FreeLibrary();
		return;
	}
}

qbool MP3_MPD_IsActive(void) {
	return !!libmpd_handle;;
}

qbool MP3_MPD_IsPlayerRunning(void) {
	return !!connection;
}

static void MPD_Connect(void) {
	char *hostname = getenv("MPD_HOST");
	char *port     = getenv("MPD_PORT");

	if (hostname == NULL)
		hostname = "localhost";
	if (port == NULL)
		port     = "6600";

	/* Timeout of 10 seconds */
	connection = qmpd_newConnection(hostname, atoi(port), 1);
	if (connection->error) {
		fprintf(stderr,"%s\n",connection->errorStr);
		qmpd_closeConnection(connection);
		connection = NULL;
		return;
	}
}

/* TODO: This works succesfully for starting mpd but we don't shutdown 
 *       afterwards. we could by using ("mpd --kill") */
//static qbool MPD_started = false;
void MP3_MPD_Execute_f(void) {
	char exec_name[MAX_OSPATH], *argv[2] = {"mpd", NULL}/*, **s*/;
	int pid, i;

	if (MP3_MPD_IsPlayerRunning()) {
		Com_Printf("MPD is already running\n");
		return;
	}
	strlcpy(exec_name, argv[0], sizeof(exec_name));

	if (!(pid = fork())) { // Child
		execvp(exec_name, argv);
		exit(-1);
	}
	if (pid == -1) {
		Com_Printf ("Couldn't execute mpd\n");
		return;
	}
	for (i = 0; i < 6; i++) {
		Sys_MSleep(50);
		MPD_Connect();
		if (MP3_MPD_IsPlayerRunning()) {
			Com_Printf("mpd is now running\n");
			return;
		}
	}
	Com_Printf("mpd (probably) failed to run\n");
}

int MP3_MPD_GetStatus(void) {
	int status;
	mpd_Status *qmpd_status;

	qmpd_sendStatusCommand(connection);
	if (!(qmpd_status = qmpd_getStatus(connection)))
		return MP3_NOTRUNNING;

	switch(qmpd_status->state) {
		case MPD_STATUS_STATE_PLAY:  status = MP3_PLAYING; break;
		case MPD_STATUS_STATE_PAUSE: status = MP3_PAUSED;  break;
		case MPD_STATUS_STATE_STOP:  status = MP3_STOPPED; break;
		case MPD_STATUS_STATE_UNKNOWN: // fall through
		default:
			status = MP3_NOTRUNNING;
	}
	qmpd_freeStatus(qmpd_status);

	return status;
}

void MP3_MPD_Play_f(void)
{
	if (!MP3_MPD_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}

	/* TODO: Need to make it play the current song, not from the start.. */
	if (MP3_MPD_GetStatus() == MP3_PAUSED) {
		qmpd_sendPauseCommand(connection, 0); // 1 == Pause, 0 == Play
	} else {
		qmpd_sendPlayCommand(connection, MPD_PLAY_AT_BEGINNING);
	}
	qmpd_finishCommand(connection);
}

void MP3_MPD_Pause_f(void)
{
	if (!MP3_MPD_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}

	if (MP3_MPD_GetStatus() == MP3_PAUSED) {
		qmpd_sendPauseCommand(connection, 0); // 1 == Pause, 0 == Play
	} else {
		qmpd_sendPauseCommand(connection, 1); // 1 == Pause, 0 == Play
	}
	qmpd_finishCommand(connection);
}


#define MPD_COMMAND(Name, Param)										\
	void MP3_MPD_##Name##_f(void) 										\
	{																	\
		if (MP3_MPD_IsPlayerRunning()) {								\
			qmpd_send##Param##Command(connection);						\
			qmpd_finishCommand(connection);								\
		} else {														\
			Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps); 	\
		}																\
		return;															\
    }

MPD_COMMAND(Stop, Stop);
MPD_COMMAND(Next, Next);
MPD_COMMAND(Prev, Prev);
MPD_COMMAND(FadeOut, Stop); /* TODO */

void MP3_MPD_GetPlaylistInfo(int *current, int *length);
qbool MP3_MPD_GetOutputtime(int *elapsed, int *total);

static void MPD_SeekRel(int offset) {
	int current;
	int elapsed;
	if (!MP3_MPD_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}

	MP3_MPD_GetPlaylistInfo(&current, NULL);
	if (!MP3_MPD_GetOutputtime(&elapsed, NULL))
		return;

	qmpd_sendSeekCommand(connection, current, elapsed + offset);
	qmpd_finishCommand(connection);
}

void MP3_MPD_FastForward_f(void) {
	MPD_SeekRel(5);
}

void MP3_MPD_Rewind_f(void) {
	MPD_SeekRel(-5);
}

void MP3_MPD_Repeat_f(void) {
	int repeat;

	if (!MP3_MPD_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}

	MP3_GetToggleState(NULL, &repeat);
	qmpd_sendRepeatCommand(connection, !repeat); // 0 == Off, 1 == On
	qmpd_finishCommand(connection);
}

void MP3_MPD_Shuffle_f(void) {
	int shuffle;
	if (!MP3_MPD_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}

	MP3_GetToggleState(&shuffle, NULL);
	qmpd_sendRandomCommand(connection, !shuffle); // 0 == Off, 1 == On
	qmpd_finishCommand(connection);
}

void MP3_MPD_ToggleRepeat_f(void) {
	MP3_MPD_Repeat_f();
}

void MP3_MPD_ToggleShuffle_f(void) {
	MP3_MPD_Shuffle_f();
}

void MP3_MPD_GetSongTitle(int track_num, char *song, size_t song_len);
char *MP3_MPD_Macro_MP3Info(void) {
	int pl_current;
	static char song[MP3_MAXSONGTITLE];
	if (!MP3_MPD_IsPlayerRunning()) {
		Com_Printf("MPD is not running\n");
		return "";
	}

	MP3_GetPlaylistInfo(&pl_current, NULL);
	MP3_MPD_GetSongTitle(pl_current+1, song, sizeof(song));

	return song;
}

qbool MP3_MPD_GetOutputtime(int *elapsed, int *total) {
	mpd_Status *qmpd_status;
	if (!MP3_MPD_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return false;
	}

	qmpd_sendStatusCommand(connection);
	if (!(qmpd_status = qmpd_getStatus(connection)))
		return false;

	if (elapsed)
		*elapsed = qmpd_status->elapsedTime;
	if (total)
		*total = qmpd_status->totalTime;

	qmpd_freeStatus(qmpd_status);

	return true;
}

qbool MP3_MPD_GetToggleState(int *shuffle, int *repeat) {
	mpd_Status *qmpd_status;

	if (!MP3_MPD_IsPlayerRunning()) 
		return false;

	qmpd_sendStatusCommand(connection);
	if (!(qmpd_status = qmpd_getStatus(connection)))
		return false;

	if (shuffle)
		*shuffle = qmpd_status->random;
	if (repeat)
		*repeat = qmpd_status->repeat;

	qmpd_freeStatus(qmpd_status);

	return true;
}

/* TODO */
void MP3_MPD_LoadPlaylist_f(void) {
	Com_Printf("The %s command is not (yet) supported.\n", Cmd_Argv(0));
}

/**
 * Return the current songs index and also the number of entries in the playlist
 */
void MP3_MPD_GetPlaylistInfo(int *current, int *length) {
	mpd_Status *qmpd_status;

	if (!MP3_MPD_IsPlayerRunning()) 
		return;

	qmpd_sendStatusCommand(connection);
	if (!(qmpd_status = qmpd_getStatus(connection)))
		return;

	if (current) {
		*current = qmpd_status->song;
	}

	if (length) {
		*length = qmpd_status->playlistLength;
	}
	qmpd_freeStatus(qmpd_status);
}

/* TODO: */
// Caching may not be needed
int MP3_MPD_CachePlaylist(void) {
	return 0;
}

void MP3_MPD_GetSongTitle(int track_num, char *song, size_t song_len) {
	mpd_InfoEntity *entity;
	strlcpy(song, "", song_len);

	if (!MP3_MPD_IsPlayerRunning()) 
		return;

	qmpd_sendPlaylistInfoCommand(connection, --track_num);
	
	while ((entity = qmpd_getNextInfoEntity(connection))) {
		if (entity->type==MPD_INFO_ENTITY_TYPE_SONG) {
			mpd_Song *mpd_song = entity->info.song;

			snprintf(song, song_len,"%s - %s",mpd_song->artist,mpd_song->title);

			return;
		}
	}

}

void MP3_MPD_PrintPlaylist_f(void) {
	int i;
	int pl_current, pl_length;
	char song[MP3_MAXSONGTITLE];

	if (!MP3_MPD_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}

	MP3_GetPlaylistInfo(&pl_current, &pl_length);
	for (i = 0; i < pl_length; i++) {
		MP3_MPD_GetSongTitle(i+1, song, sizeof(song));
		if (pl_current == i) {
			char *s;
			for (s = song; *s; s++)
				*s |= 128;
		}

		Com_Printf("%3d %s\n", i+1, song);
	}
}

/**
 * Set the current track to the given argument
 */
void MP3_MPD_PlayTrackNum_f(void) {
	int track_num;

	if (!MP3_MPD_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <track #>\n", Cmd_Argv(0));
		return;
	}

	track_num = Q_atoi(Cmd_Argv(1)) - 1;	// Count from zero
	qmpd_sendPlayCommand(connection, track_num);
	qmpd_finishCommand(connection);
}

void Media_SetVolume_f(void);
char* Media_GetVolume_f(void);

/* TODO: */
void MP3_MPD_Shutdown(void) {
	/*	
	if (MPD_started) {
		/\* TODO: Send quit command *\/
		if (!kill(MPD_pid, SIGTERM))
			waitpid(MPD_pid, NULL, 0);
	}*/

	if (!MP3_MPD_IsPlayerRunning())
		return;
	qmpd_closeConnection(connection);
	connection = NULL;
}

void MP3_MPD_Init(void) {
	MPD_LoadLibrary();

	if (!MP3_MPD_IsActive())
		return;

	MPD_Connect();

	if (!Cmd_Exists("mp3_startmpd"))
			Cmd_AddCommand("mp3_startmpd", MP3_Execute_f);
}

double Media_MPD_GetVolume(void) {
	int vol;
	mpd_Status *qmpd_status;

	if (!MP3_MPD_IsPlayerRunning())
		return 0.0;

	qmpd_sendStatusCommand(connection);
	if (!(qmpd_status = qmpd_getStatus(connection)))
		return 0.0;

	vol = qmpd_status->volume;
	if (vol == MPD_STATUS_NO_VOLUME)
		vol = 0;

	qmpd_freeStatus(qmpd_status);
	return (double) vol / 100.0;
}

void Media_MPD_SetVolume(double vol) {
	if (!MP3_MPD_IsPlayerRunning())
		return;

	qmpd_sendSetvolCommand(connection, (int) (vol * 100.0));
	qmpd_finishCommand(connection);
}

const mp3_player_t mp3_player_mpd = {
	/* Messages */
	"MPD",   // PlayerName_AllCaps
	"Mpd",   // PlayerName_LeadingCaps  
	"mpd",   // PlayerName_NoCaps 
	MP3_MPD,

	/* Functions */
	MP3_MPD_Init,
	MP3_MPD_Shutdown,

	MP3_MPD_IsActive,
	MP3_MPD_IsPlayerRunning,
	MP3_MPD_GetStatus,
	MP3_MPD_GetPlaylistInfo,
	MP3_MPD_GetSongTitle,
	MP3_MPD_GetOutputtime,
	MP3_MPD_GetToggleState,

	MP3_MPD_PrintPlaylist_f,
	MP3_MPD_PlayTrackNum_f,
	MP3_MPD_LoadPlaylist_f,
	MP3_MPD_CachePlaylist,
	MP3_MPD_Next_f,
	MP3_MPD_FastForward_f,
	MP3_MPD_Rewind_f,
	MP3_MPD_Prev_f,
	MP3_MPD_Play_f,
	MP3_MPD_Pause_f,
	MP3_MPD_Stop_f,
	MP3_MPD_Execute_f,
	MP3_MPD_ToggleRepeat_f, 	// Not supported
	MP3_MPD_Repeat_f, 			// Not supported
	MP3_MPD_ToggleShuffle_f, 	// Not supported
	MP3_MPD_Shuffle_f, 			// Not supported
	MP3_MPD_FadeOut_f, 			// Just does the same as stop

	Media_MPD_GetVolume,
	Media_MPD_SetVolume,

	/* Macro's */
	MP3_MPD_Macro_MP3Info, 
};

#else
const mp3_player_t mp3_player_mpd = {
	"NONE",   // PlayerName_AllCaps  
	"None",   // PlayerName_LeadingCaps
	"none",   // PlayerName_NoCaps 
	MP3_NONE, // Type
};
#endif // WITH_MPD

