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

	$Id: mp3_player.h,v 1.8 2007-10-26 07:55:45 dkure Exp $

*/

#ifdef _WIN32
#include "winamp.h"
#endif

typedef enum {
	MP3_NOTRUNNING	= -1,
	MP3_PLAYING		= 1,
	MP3_PAUSED		= 2,
	MP3_STOPPED		= 3,
} MP3_status_t;
#define MP3_MAXSONGTITLE	128

void  MP3_Init(void);
void  MP3_Shutdown(void);

qbool MP3_IsActive(void);
qbool MP3_IsPlayerRunning(void);
int   MP3_GetStatus(void);
void  MP3_GetPlaylistInfo(int *current, int *length);
void  MP3_GetSongTitle(int track_num, char *song, size_t song_len);
qbool MP3_GetOutputtime(int *elapsed, int *total);
qbool MP3_GetToggleState(int *shuffle, int *repeat);

void  MP3_PrintPlaylist_f(void);
void  MP3_PlayTrackNum_f(void);
void  MP3_LoadPlaylist_f(void);
int   MP3_CachePlaylist(void);
void  MP3_Next_f(void);
void  MP3_FastForward_f(void);
void  MP3_Rewind_f(void);
void  MP3_Prev_f(void);
void  MP3_Play_f(void);
void  MP3_Pause_f(void);
void  MP3_Stop_f(void);
void  MP3_Execute_f(void);
void  MP3_ToggleRepeat_f(void);
void  MP3_Repeat_f(void);
void  MP3_ToggleShuffle_f(void);
void  MP3_Shuffle_f(void);
void  MP3_FadeOut_f(void);

int   MP3_ParsePlaylist_EXTM3U(char *, unsigned int, char *entries[], unsigned int, unsigned int);

char *MP3_Menu_SongtTitle(void);
char *MP3_Macro_MP3Info(void);

extern cvar_t mp3_scrolltitle, mp3_showtime, mp3_xmms_session;

double Media_GetVolume(void);
void Media_SetVolume(double vol);

typedef enum {
	MP3_NONE,
	MP3_XMMS,
	MP3_XMMS2,
	MP3_AUDACIOUS,
	MP3_MPD,
	MP3_WINAMP,
} mp3_players_t;

typedef struct {
	/* Status */
	const char *PlayerName_AllCaps;
	const char *PlayerName_LeadingCaps;
	const char *PlayerName_NoCaps;
	const mp3_players_t Type;
	
	/* Functions */
	void  (*Init)            (void);
	void  (*Shutdown)        (void);

	qbool (*IsActive)        (void);
	qbool (*IsPlayerRunning) (void);
	int   (*GetStatus)       (void);
	void  (*GetPlaylistInfo) (int *current, int *length);
	void  (*GetSongTitle)    (int track_num, char *song, size_t song_len);
	qbool (*GetOutputtime)   (int *elapsed, int *total);
	qbool (*GetToggleState)  (int *shuffle, int *repeat);

	void  (*PrintPlaylist_f) (void);
	void  (*PlayTrackNum_f)  (void);
	void  (*LoadPlaylist_f)  (void);
	int   (*CachePlaylist)      (void);

	void  (*Next_f)          (void);
	void  (*FastForward_f)   (void);
	void  (*Rewind_f)        (void);
	void  (*Prev_f)          (void);
	void  (*Play_f)          (void);
	void  (*Pause_f)         (void);
	void  (*Stop_f)          (void);
	void  (*Execute_f)       (void);
	void  (*ToggleRepeat_f)  (void);
	void  (*Repeat_f)        (void);
	void  (*ToggleShuffle_f) (void);
	void  (*Shuffle_f)       (void);
	void  (*FadeOut_f)       (void);

	double (*GetVolume)      (void);
	void   (*SetVolume)      (double vol);

	/* Macro's */
	char *(*Macro_MP3Info)   (void);
} mp3_player_t;

extern const mp3_player_t *mp3_player;
extern const mp3_player_t mp3_player_none;
extern const mp3_player_t mp3_player_xmms;
extern const mp3_player_t mp3_player_xmms2;
extern const mp3_player_t mp3_player_mpd;
extern const mp3_player_t mp3_player_audacious;
extern const mp3_player_t mp3_player_winamp;
