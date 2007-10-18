/*

Copyright (C) 2001-2002       A Nourai
Plugable MP3 support (2007)   P Archer

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

	$Id: mp3_player.c,v 1.27 2007-10-18 12:50:00 dkure Exp $
*/

#ifdef __FreeBSD__
#include <dlfcn.h>
#endif
#include "quakedef.h"
#include "mp3_player.h"
#include "utils.h"

cvar_t mp3_scrolltitle = {"mp3_scrolltitle", "1"};
cvar_t mp3_showtime = {"mp3_showtime", "1"};

const mp3_player_t mp3_player_none = {
	"NONE",   // PlayerName_AllCaps  
	"None",   // PlayerName_LeadingCaps
	"none",   // PlayerName_NoCaps 
	MP3_NONE, // Type
};

/* TODO: These could be set via a cvar_t */
#ifdef WITH_WINAMP
const mp3_player_t *mp3_player = &mp3_player_winamp;
#else
#ifdef WITH_AUDACIOUS
// AUDACIOUS is backwards compatible, but libraries are still needed
const mp3_player_t *mp3_player = &mp3_player_audacious;
#else
#ifdef WITH_XMMS
const mp3_player_t *mp3_player = &mp3_player_xmms;
#else
const mp3_player_t *mp3_player = &mp3_player_none;
#endif // WITH_XMMS
#endif // WITH_AUDACIOUS
#endif // WITH_WINAMP


#ifdef WITH_MP3_PLAYER

void MP3_SongInfo_f(void) {
	char *status_string, *title, *s;
	char elapsed_string[MP3_MAXSONGTITLE];
	int status, elapsed, total;

	if (!MP3_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
	status = MP3_GetStatus();
	if (Cmd_Argc() != 1) {
		Com_Printf("%s : no arguments expected\n", Cmd_Argv(0));
		return;
	}
	if (status == MP3_STOPPED)
		status_string = "\x10Stopped\x11";
	else if (status == MP3_PAUSED)
		status_string = "\x10Paused\x11";
	else
		status_string = "\x10Playing\x11";

	for (s = title = MP3_Macro_MP3Info(); *s; s++)
		*s |= 128;

	if (!MP3_GetOutputtime(&elapsed, &total) || elapsed < 0 || total < 0) {
		Com_Printf(va("%s %s\n", status_string, title));
		return;
	}
	strlcpy(elapsed_string, SecondsToMinutesString(elapsed), sizeof(elapsed_string));
	Com_Printf(va("%s %s \x10%s/%s\x11\n", status_string, title, elapsed_string, SecondsToMinutesString(total)));
}

char *MP3_Menu_SongtTitle(void) {
	static char title[MP3_MAXSONGTITLE], *macrotitle;
	int current = 0;

	if (!MP3_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		snprintf(title, sizeof(title), "%s is not running\n", mp3_player->PlayerName_LeadingCaps);
	    return title;
	}
	macrotitle = MP3_Macro_MP3Info();
	MP3_GetPlaylistInfo(&current, NULL);
	if (*macrotitle)
		strlcpy(title, va("%d. %s", current + 1, macrotitle), sizeof(title));
	else
		strlcpy(title, mp3_player->PlayerName_AllCaps, sizeof(title));
	return title;
}

int MP3_ParsePlaylist_EXTM3U(char *playlist_buf, unsigned int length, char *entries[], unsigned int maxsize, unsigned int maxsonglen) {
	int skip = 0, playlist_size = 0;
	char *s, *t, *buf, *line;

	buf = playlist_buf;
	while (playlist_size < maxsize) {
		for (s = line = buf; s - playlist_buf < length && *s && *s != '\n' && *s != '\r'; s++)
			;
		if (s - playlist_buf >= length)
			break;
		*s = 0;
		buf = s + 2;
		if (skip || !strncmp(line, "#EXTM3U", 7)) {
			skip = 0;
			continue;
		}
		if (!strncmp(line, "#EXTINF:", 8)) {
			if (!(s = strstr(line, ",")) || ++s - playlist_buf >= length) 
				break;
			if (strlen(s) > maxsonglen)
				s[maxsonglen] = 0;
			entries[playlist_size++] = Q_strdup(s);
			skip = 1;
			continue;
		}
	
		for (s = line + strlen(line); s > line && *s != '\\' && *s != '/'; s--)
			;
		if (s != line)
			s++;
	
		if ((t = strrchr(s, '.')) && t - playlist_buf < length)
			*t = 0;		
	
		for (t = s + strlen(s) - 1; t > s && *t == ' '; t--)
			*t = 0;
		if (strlen(s) > maxsonglen)
			s[maxsonglen] = 0;
		entries[playlist_size++] = Q_strdup(s);
	}
	return playlist_size;
}

void Media_SetVolume_f(void);
char* Media_GetVolume_f(void);

void MP3_Init(void) {
	mp3_player->Init();

	if (!MP3_IsActive())
		return;

	Cmd_AddMacro("mp3info", MP3_Macro_MP3Info);
	Cmd_AddMacro("mp3_volume", Media_GetVolume_f);

	Cmd_AddCommand("mp3_prev", MP3_Prev_f);
	Cmd_AddCommand("mp3_play", MP3_Play_f);
	Cmd_AddCommand("mp3_pause", MP3_Pause_f);
	Cmd_AddCommand("mp3_stop", MP3_Stop_f);
	Cmd_AddCommand("mp3_next", MP3_Next_f);
	Cmd_AddCommand("mp3_fforward", MP3_FastForward_f);
	Cmd_AddCommand("mp3_rewind", MP3_Rewind_f);
	Cmd_AddCommand("mp3_fadeout", MP3_FadeOut_f);
	Cmd_AddCommand("mp3_shuffle", MP3_Shuffle_f);
	Cmd_AddCommand("mp3_repeat", MP3_Repeat_f);
	Cmd_AddCommand("mp3_playlist", MP3_PrintPlaylist_f);
	Cmd_AddCommand("mp3_playtrack", MP3_PlayTrackNum_f);
	Cmd_AddCommand("mp3_songinfo", MP3_SongInfo_f);
	Cmd_AddCommand("mp3_loadplaylist", MP3_LoadPlaylist_f);	
	Cmd_AddCommand("mp3_volume", Media_SetVolume_f);
	//Cmd_AddCommand("mp3_start" MP3_PLAYERNAME_NOCAPS, MP3_Execute_f);

	Cvar_SetCurrentGroup(CVAR_GROUP_MP3);
	Cvar_Register(&mp3_dir);
	Cvar_Register(&mp3_scrolltitle);
	Cvar_Register(&mp3_showtime);
	Cvar_ResetCurrentGroup();
}

char* Media_GetVolume_f(void) {
	static char macrobuf[16];
	snprintf(macrobuf, sizeof(macrobuf), "%f", Media_GetVolume());
	return macrobuf;
}

// command to set (and read) mediaplayer volume
void Media_SetVolume_f(void) {
	if (Cmd_Argc() < 2) { // users want to read the volume
		Com_Printf("Use $mp3_volume\nCurrent mediaplayer volume:\n");
		Cbuf_AddText("echo $mp3_volume\n");
	} else {
		char *v = Cmd_Argv(1);
		if (v[0] == '+') {
			Media_SetVolume(Media_GetVolume() + Q_atof(v + 1));
		} else if (v[0] == '-') {
			Media_SetVolume(Media_GetVolume() - Q_atof(v + 1));
		} else {
			Media_SetVolume(Q_atof(v));
		}
	}
}

#else

void MP3_Init(void) {}

#endif


#ifdef __APPLE__
qbool MP3_GetOutputtime (int *elapsed, int *total)  {
	return false;
}

int MP3_GetStatus(void) {
	return -1;
}

char *MP3_Macro_MP3Info(void) {
	return "";
}
#endif

int MP3_CheckFunction(qbool PrintWarning) {
	if (!mp3_player) {
		Sys_Error("MP3 Player has been corrupted\n");
	} else if (mp3_player->Type == MP3_NONE) {
		if (PrintWarning) {
			Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		}
		return -1;
	}
	return 0;
}

/* TODO: Need to some how check that the operations are not null */
void MP3_Shutdown(void) {
	if (MP3_CheckFunction(true))
		return;
	mp3_player->Shutdown();
}

qbool MP3_IsPlayerRunning(void) {
	if (MP3_CheckFunction(true))
		return false; 
	return mp3_player->IsPlayerRunning();
}

qbool MP3_IsActive(void) {
	if (MP3_CheckFunction(false))
		return false;
	return mp3_player->IsActive();
}
int MP3_GetStatus(void) {
	if (MP3_CheckFunction(true))
		return MP3_NOTRUNNING;
	return mp3_player->GetStatus();
}
void MP3_GetPlaylistInfo(int *current, int *length) {
	if (MP3_CheckFunction(true))
		return;
	mp3_player->GetPlaylistInfo(current, length);
}

long MP3_GetPlaylist(char **buf) {
	if (MP3_CheckFunction(true))
		return -1;
	return mp3_player->GetPlaylist(buf);
}

qbool MP3_GetOutputtime(int *elapsed, int *total) {
	if (MP3_CheckFunction(true))
		return false;
	return mp3_player->GetOutputtime(elapsed, total);
}

qbool MP3_GetToggleState(int *shuffle, int *repeat) {
	if (MP3_CheckFunction(true))
		return false;
	return mp3_player->GetToggleState(shuffle, repeat);
}

void MP3_PrintPlaylist_f(void) {
	if (MP3_CheckFunction(true))
		return;
	mp3_player->PrintPlaylist_f();
}

void MP3_PlayTrackNum_f(void) {
	if (MP3_CheckFunction(true))
		return;
	mp3_player->PlayTrackNum_f();
}

void MP3_LoadPlaylist_f(void) {
	if (MP3_CheckFunction(true))
		return;
	mp3_player->LoadPlaylist_f();
}

void MP3_Next_f(void) {
	if (MP3_CheckFunction(true))
		return;
	mp3_player->Next_f();
}

void MP3_FastForward_f(void) {
	if (MP3_CheckFunction(true))
		return;
	mp3_player->FastForward_f();
}

void MP3_Rewind_f(void) {
	if (MP3_CheckFunction(true))
		return;
	mp3_player->Rewind_f();
}

void MP3_Prev_f(void) {
	if (MP3_CheckFunction(true))
		return;
	mp3_player->Prev_f();
}

void MP3_Play_f(void) {
	if (MP3_CheckFunction(true))
		return;
	mp3_player->Play_f();
}

void MP3_Pause_f(void) {
	if (MP3_CheckFunction(true))
		return;
	mp3_player->Pause_f();
}

void MP3_Stop_f(void) {
	if (MP3_CheckFunction(true))
		return;
	mp3_player->Stop_f();
}

void MP3_Execute_f(void) {
	if (MP3_CheckFunction(true))
		return;
	mp3_player->Execute_f();
}

void MP3_ToggleRepeat_f(void) {
	if (MP3_CheckFunction(true))
		return;
	mp3_player->ToggleRepeat_f();
}

void MP3_Repeat_f(void) {
	if (MP3_CheckFunction(true))
		return;
	mp3_player->Repeat_f();
}

void MP3_ToggleShuffle_f(void) {
	if (MP3_CheckFunction(true))
		return;
	mp3_player->ToggleShuffle_f();
}

void MP3_Shuffle_f(void) {
	if (MP3_CheckFunction(true))
		return;
	mp3_player->Shuffle_f();
}

void MP3_FadeOut_f(void) {
	if (MP3_CheckFunction(true))
		return;
	mp3_player->FadeOut_f();
}

char *MP3_Macro_MP3Info(void) {
	if (MP3_CheckFunction(true))
		return NULL;
	return mp3_player->Macro_MP3Info();
}

double Media_GetVolume(void) {
	if (MP3_CheckFunction(true))
		return 0.0;
	return mp3_player->GetVolume();
}

void Media_SetVolume(double vol) {
	if (MP3_CheckFunction(true))
		return;
	mp3_player->SetVolume(vol);
}
