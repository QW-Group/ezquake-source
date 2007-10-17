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

	$Id: mp3_player.h,v 1.6 2007-10-17 17:08:26 dkure Exp $

*/


#ifdef WITH_MP3_PLAYER
#ifdef WITH_XMMS
#include <xmms/xmmsctrl.h>
#else
#ifdef WITH_AUDACIOUS
#include <audacious/beepctrl.h>
#endif
#endif

#define XMMS_SESSION	((int) mp3_xmms_session.value)
#endif

#ifdef _WIN32
#include "winamp.h"
#endif

#define MP3_NOTRUNNING		-1
#define MP3_PLAYING			1
#define MP3_PAUSED			2
#define MP3_STOPPED			3

#define MP3_MAXSONGTITLE	128

#if defined(WITH_XMMS) || defined(WITH_AUDACIOUS)
/*extern void (*qxmms_remote_play)(gint session);
extern void (*qxmms_remote_pause)(gint session);
extern void (*qxmms_remote_stop)(gint session);
extern void (*qxmms_remote_jump_to_time)(gint session, gint pos);
extern gboolean (*qxmms_remote_is_running)(gint session);
extern gboolean (*qxmms_remote_is_playing)(gint session);
extern gboolean (*qxmms_remote_is_paused)(gint session);
extern gboolean (*qxmms_remote_is_repeat)(gint session);
extern gboolean (*qxmms_remote_is_shuffle)(gint session);
extern gint (*qxmms_remote_get_playlist_pos)(gint session);
extern void (*qxmms_remote_set_playlist_pos)(gint session, gint pos);
*/
extern gint (*qxmms_remote_get_playlist_length)(gint session);
extern gchar *(*qxmms_remote_get_playlist_title)(gint session, gint pos);
/*
extern void (*qxmms_remote_playlist_prev)(gint session);
extern void (*qxmms_remote_playlist_next)(gint session);
extern gint (*qxmms_remote_get_output_time)(gint session);
extern gint (*qxmms_remote_get_playlist_time)(gint session, gint pos);
extern gint (*qxmms_remote_get_main_volume)(gint session);
extern void (*qxmms_remote_set_main_volume)(gint session, gint v);
extern void (*qxmms_remote_toggle_repeat)(gint session);
extern void (*qxmms_remote_toggle_shuffle)(gint session);
*/
extern void (*qg_free)(gpointer);
#endif

void  MP3_Init(void);
void  MP3_Shutdown(void);

qbool MP3_IsActive(void);
qbool MP3_IsPlayerRunning(void);
int   MP3_GetStatus(void);
void  MP3_GetPlaylistInfo(int *current, int *length);
long  MP3_GetPlaylist(char **buf);
qbool MP3_GetOutputtime(int *elapsed, int *total);
qbool MP3_GetToggleState(int *shuffle, int *repeat);

void  MP3_PrintPlaylist_f(void);
void  MP3_PlayTrackNum_f(void);
void  MP3_LoadPlaylist_f(void);
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

extern cvar_t mp3_dir, mp3_scrolltitle, mp3_showtime, mp3_xmms_session;

double Media_GetVolume(void);
void Media_SetVolume(double vol);

typedef enum {
	MP3_NONE,
	MP3_XMMS,
	MP3_AUDACIOUS,
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
	long  (*GetPlaylist)     (char **buf);
	qbool (*GetOutputtime)   (int *elapsed, int *total);
	qbool (*GetToggleState)  (int *shuffle, int *repeat);

	void  (*PrintPlaylist_f) (void);
	void  (*PlayTrackNum_f)  (void);
	void  (*LoadPlaylist_f)  (void);
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
extern const mp3_player_t mp3_player_audacious;
extern const mp3_player_t mp3_player_winamp;
