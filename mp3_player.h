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


#if defined(__linux__) && defined(WITH_XMMS)
#define __XMMS__
#endif

#ifdef __XMMS__
#include <xmms/xmmsctrl.h>
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

#ifdef _WIN32
#define MP3_PLAYERNAME_ALLCAPS		"WINAMP"
#define MP3_PLAYERNAME_LEADINGCAP	"Winamp"
#define MP3_PLAYERNAME_NOCAPS		"winamp"
#else
#define MP3_PLAYERNAME_ALLCAPS		"XMMS"
#define MP3_PLAYERNAME_LEADINGCAP	"XMMS"
#define MP3_PLAYERNAME_NOCAPS		"xmms"
#endif

#ifdef __XMMS__
extern void (*qxmms_remote_play)(gint session);
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
extern gint (*qxmms_remote_get_playlist_length)(gint session);
extern gchar *(*qxmms_remote_get_playlist_title)(gint session, gint pos);
extern void (*qxmms_remote_playlist_prev)(gint session);
extern void (*qxmms_remote_playlist_next)(gint session);
extern gint (*qxmms_remote_get_output_time)(gint session);
extern gint (*qxmms_remote_get_playlist_time)(gint session, gint pos);
extern gint (*qxmms_remote_get_main_volume)(gint session);
extern void (*qxmms_remote_set_main_volume)(gint session, gint v);
extern void (*qxmms_remote_toggle_repeat)(gint session);
extern void (*qxmms_remote_toggle_shuffle)(gint session);
extern void (*qg_free)(gpointer);
#endif

qboolean MP3_IsActive(void);
void MP3_Init(void);
void MP3_Frame(void);
void MP3_Shutdown(void);

int MP3_GetStatus(void);
void MP3_GetPlaylistInfo(int *current, int *length);
long MP3_GetPlaylist(char **buf);
qboolean MP3_GetOutputtime(int *elapsed, int *total);
qboolean MP3_GetToggleState(int *shuffle, int *repeat);

void MP3_Next_f(void);
void MP3_FastForward_f(void);
void MP3_Rewind_f(void);
void MP3_Prev_f(void);
void MP3_Play_f(void);
void MP3_Pause_f(void);
void MP3_Stop_f(void);
void MP3_Execute_f(void);
void MP3_ToggleRepeat_f(void);
void MP3_ToggleShuffle_f(void);
void MP3_FadeOut_f(void);

int MP3_ParsePlaylist_EXTM3U(char *, unsigned int, char *entries[], unsigned int, unsigned int);

char *MP3_Menu_SongtTitle(void);


extern cvar_t mp3_dir, mp3_scrolltitle, mp3_showtime, mp3_volume, mp3_xmms_session;

extern qboolean mp3_volumectrl_active;
