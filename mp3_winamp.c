/*

Copyright (C) 2001-2002       A Nourai
Plugable Support (2007)       P Archer

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

	$Id: mp3_winamp.c,v 1.3 2007-10-18 20:29:56 cokeman1982 Exp $
*/

#include "quakedef.h"
#include "mp3_player.h"
#include "utils.h"

#ifdef WITH_WINAMP

#include "windows.h"

static HWND mp3_hwnd = 0;

cvar_t mp3_dir = {"mp3_winamp_dir", "c:/program files/winamp"};

qbool MP3_WINAMP_IsActive(void) {
	return true;
}

static qbool MP3_WINAMP_IsPlayerRunning(void) {
	return ((mp3_hwnd = FindWindow("ezQuake Winamp", NULL)) || (mp3_hwnd = FindWindow("Winamp v1.x", NULL)));
}

void MP3_WINAMP_Execute_f(void) {
	STARTUPINFO si;
	PROCESS_INFORMATION	pi;
	char path[256];
	int length;


	if (MP3_WINAMP_IsPlayerRunning()) {
		Com_Printf("Winamp is already running\n");
		return;
	}
	memset (&si, 0, sizeof(si));
	si.cb = sizeof(si);
	si.wShowWindow = SW_SHOWMINNOACTIVE;
	si.dwFlags = STARTF_USESHOWWINDOW;

	strlcpy(path, mp3_dir.string, sizeof(path) - strlen("/winamp.exe"));
	length = strlen(path);
	if (length && (path[length - 1] == '\\' || path[length - 1] == '/'))
		path[length - 1] = 0;
	strlcat (path, "/winamp.exe", sizeof (path));
	if (!CreateProcess (NULL, va("%s /CLASS=\"ezQuake Winamp\"", path), 
		NULL, NULL, FALSE, GetPriorityClass(GetCurrentProcess()), NULL, NULL, &si, &pi))
	{
		Com_Printf ("Couldn't execute winamp\n");
		return;
	}
	Com_Printf("Winamp is now running\n");
	return;
}

#define WINAMP_COMMAND(Name, Param)									\
	void MP3_WINAMP_##Name##_f(void) {								\
		int ret;													\
																	\
		if (MP3_WINAMP_IsPlayerRunning())							\
			ret = SendMessage(mp3_hwnd, WM_COMMAND, Param, 0);		\
		else														\
			Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps); \
	}

WINAMP_COMMAND(Prev, WINAMP_BUTTON1);
WINAMP_COMMAND(Play, WINAMP_BUTTON2);
WINAMP_COMMAND(Pause, WINAMP_BUTTON3);
WINAMP_COMMAND(Stop, WINAMP_BUTTON4);
WINAMP_COMMAND(Next, WINAMP_BUTTON5);
WINAMP_COMMAND(Rewind, WINAMP_BUTTON1_SHIFT);
WINAMP_COMMAND(FastForward, WINAMP_BUTTON5_SHIFT);
WINAMP_COMMAND(FadeOut, WINAMP_BUTTON4_SHIFT);

int MP3_WINAMP_GetStatus(void) {
	int ret;

	if (!MP3_WINAMP_IsPlayerRunning())
		return MP3_NOTRUNNING;
	ret = SendMessage(mp3_hwnd, WM_WA_IPC, 0, IPC_ISPLAYING);
	switch (ret) {
		case 3 : return MP3_PAUSED; break;
		case 1 : return MP3_PLAYING; break;
		case 0 : 
		default : return MP3_STOPPED; break;
	}
}

static void WINAMP_Set_ToggleFn(char *name, int setparam, int getparam) {
	int ret, set;

	if (!MP3_WINAMP_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
	if (Cmd_Argc() >= 3) {
		Com_Printf("Usage: %s [on|off|toggle]\n", Cmd_Argv(0));
		return;
	}
	ret = SendMessage(mp3_hwnd, WM_WA_IPC, 0, getparam);
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
	SendMessage(mp3_hwnd, WM_WA_IPC, set, setparam);
	Com_Printf("%s set to %s\n", name, set ? "on" : "off");
}

void MP3_WINAMP_Repeat_f(void) {
	WINAMP_Set_ToggleFn("Repeat", IPC_SET_REPEAT, IPC_GET_REPEAT);
}

void MP3_WINAMP_Shuffle_f(void) {
	WINAMP_Set_ToggleFn("Shuffle", IPC_SET_SHUFFLE, IPC_GET_SHUFFLE);
}

static void WINAMP_Toggle_ToggleFn(char *name, int setparam, int getparam) {
	int ret;

	if (!MP3_WINAMP_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
	ret = SendMessage(mp3_hwnd, WM_WA_IPC, 0, getparam);
	SendMessage(mp3_hwnd, WM_WA_IPC, ret ? 0 : 1, setparam);
}

void MP3_WINAMP_ToggleRepeat_f(void) {
	WINAMP_Toggle_ToggleFn("Repeat", IPC_SET_REPEAT, IPC_GET_REPEAT);
}

void MP3_WINAMP_ToggleShuffle_f(void) {
	WINAMP_Toggle_ToggleFn("Shuffle", IPC_SET_SHUFFLE, IPC_GET_SHUFFLE);
}

char *MP3_WINAMP_Macro_MP3Info(void) {
	char *s;
	static char title[MP3_MAXSONGTITLE];

	if (!MP3_WINAMP_IsPlayerRunning()) {
	
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		snprintf(title, sizeof(title), "%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return title;
	}
	GetWindowText(mp3_hwnd, title, sizeof(title));
	if ((s = strrchr(title, '-')) && s > title)
		*(s - 1) = 0;
	for (s = title; *s && isdigit(*s); s++)
		;
	if (*s == '.' && s[1] == ' ' && s[2])
		memmove(title, s + 2, strlen(s + 2) + 1);
	return title;
}

qbool MP3_WINAMP_GetOutputtime(int *elapsed, int *total) {
	int ret1, ret2;

	if (!MP3_WINAMP_IsPlayerRunning())
		return false;
	if (!(ret1 = SendMessage(mp3_hwnd, WM_WA_IPC, 0, IPC_GETOUTPUTTIME)) == -1)
		return false;
	if (!(ret2 = SendMessage(mp3_hwnd, WM_WA_IPC, 1, IPC_GETOUTPUTTIME)) == -1)
		return false;
	*elapsed = ret1 / 1000;
	*total = ret2;

	return true;
}

qbool MP3_WINAMP_GetToggleState(int *shuffle, int *repeat) {
	int ret1, ret2;

	if (!MP3_WINAMP_IsPlayerRunning())
		return false;
	if (!(ret1 = SendMessage(mp3_hwnd, WM_WA_IPC, 0, IPC_GET_SHUFFLE)) == -1)
		return false;
	if (!(ret2 = SendMessage(mp3_hwnd, WM_WA_IPC, 0, IPC_GET_REPEAT)) == -1)
		return false;
	*shuffle = ret1;
	*repeat = ret2;

	return true;
}

void MP3_WINAMP_LoadPlaylist_f(void) {
	COPYDATASTRUCT cds;
	char playlist[64];

	if (Cmd_Argc() == 1) {
		Com_Printf("Usage: %s <filename>\n", Cmd_Argv(0));
		return;
	}
	if (!MP3_WINAMP_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}

	strlcpy(playlist, Cmd_Args(), sizeof(playlist));

	if (!strcmp(COM_FileExtension(playlist), "pls")) {
		Com_Printf("Error: .pls playlists are not supported.  Try loading a .m3u playlist\n");
		return;
	}

	COM_ForceExtensionEx (playlist, ".m3u", sizeof (playlist));
	
	SendMessage(mp3_hwnd, WM_WA_IPC, 0, IPC_DELETE);
	cds.dwData = IPC_PLAYFILE;
	cds.lpData = playlist;
	cds.cbData = strlen(playlist) + 1;
	SendMessage(mp3_hwnd, WM_COPYDATA, (WPARAM) NULL, (LPARAM) &cds);
}

long MP3_WINAMP_GetPlaylist(char **buf) {
	FILE *f;
	char path[512];
	int pathlength;
	long filelength;
	COPYDATASTRUCT cds;

	if (!MP3_WINAMP_IsPlayerRunning())
		return -1;

	cds.dwData = IPC_CHDIR;
	cds.lpData = mp3_dir.string;
	cds.cbData = strlen(mp3_dir.string) + 1;
	SendMessage(mp3_hwnd, WM_COPYDATA, (WPARAM) NULL, (LPARAM) &cds);

	SendMessage(mp3_hwnd, WM_WA_IPC, 0, IPC_WRITEPLAYLIST);
	strlcpy(path, mp3_dir.string, sizeof(path));
	pathlength = strlen(path);
	if (pathlength && (path[pathlength - 1] == '\\' || path[pathlength - 1] == '/'))
		path[pathlength - 1] = 0;
	strlcat (path, "/winamp.m3u", sizeof (path));
	filelength = FS_FileOpenRead(path, &f);
	if (!f)
		return -1;
	*buf = Q_malloc(filelength);
	if (filelength != fread(*buf, 1,  filelength, f)) {
		Q_free(*buf);
		fclose(f);
		return -1;
	}
	fclose(f);
	return filelength;
}

void MP3_WINAMP_GetPlaylistInfo(int *current, int *length) {
	if (!MP3_WINAMP_IsPlayerRunning())
		return;
	if (length)
		*length = SendMessage(mp3_hwnd, WM_WA_IPC, 0, IPC_GETLISTLENGTH);
	if (current)
		*current = SendMessage(mp3_hwnd, WM_WA_IPC, 0, IPC_GETLISTPOS);
}

void MP3_WINAMP_PrintPlaylist_f(void) {
	char *playlist_buf, *entries[1024];
	unsigned int length;
	int i, playlist_size, current;

	if ((length = MP3_WINAMP_GetPlaylist(&playlist_buf)) == -1) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
	MP3_WINAMP_GetPlaylistInfo(&current, NULL);
	playlist_size = MP3_ParsePlaylist_EXTM3U(playlist_buf, length, entries, sizeof(entries) / sizeof(entries[0]), 128);

	for (i = 0; i < playlist_size; i++) {
		Com_Printf("%s%3d %s\n", i == current ? "\x02" : "", i + 1, entries[i]);
		Q_free(entries[i]);
	}

	Q_free(playlist_buf);
}

void MP3_WINAMP_PlayTrackNum_f(void) {
	int pos, length;

	if (!MP3_WINAMP_IsPlayerRunning()) {
		Com_Printf("%s is not running\n", mp3_player->PlayerName_LeadingCaps);
		return;
	}
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <track #>\n", Cmd_Argv(0));
		return;
	}
	MP3_WINAMP_GetPlaylistInfo(NULL, &length);
	pos = Q_atoi(Cmd_Argv(1)) - 1;
	if (pos < 0 || pos >= length)
		return;
	SendMessage(mp3_hwnd, WM_WA_IPC, pos, IPC_SETPLAYLISTPOS);
	MP3_WINAMP_Play_f();
}

double Media_WINAMP_GetVolume(void) {
	static double vol = 0;

	vol = SendMessage(mp3_hwnd, WM_WA_IPC, -666, IPC_SETVOLUME) / 255.0;
	vol = ((int) (vol * 100)) / 100.0;

	return vol;
}

void Media_WINAMP_SetVolume(double vol) {
	if (!MP3_WINAMP_IsPlayerRunning())
		return;

	vol = vol * 255.0 + 0.5;
	vol = bound (0, vol, 255);
	SendMessage(mp3_hwnd, WM_WA_IPC, (int) vol, IPC_SETVOLUME);
	return;
}

void MP3_WINAMP_Init(void)
{
}

void MP3_WINAMP_Shutdown(void) 
{
}

const mp3_player_t mp3_player_winamp = {
	/* Messages */
	"WINAMP", // PlayerName_AllCaps
	"Winamp", // PlayerName_LeadingCaps
	"winamp", // PlayerName_NoCaps
	MP3_WINAMP, // Type

	/* Functions */
	MP3_WINAMP_Init, 
	MP3_WINAMP_Shutdown, 

	MP3_WINAMP_IsActive, 
	MP3_WINAMP_IsPlayerRunning, 
	MP3_WINAMP_GetStatus, 
	MP3_WINAMP_GetPlaylistInfo, 
	MP3_WINAMP_GetPlaylist, 
	MP3_WINAMP_GetOutputtime, 
	MP3_WINAMP_GetToggleState, 

	MP3_WINAMP_PrintPlaylist_f, 
	MP3_WINAMP_PlayTrackNum_f, 
	MP3_WINAMP_LoadPlaylist_f, 
	MP3_WINAMP_Next_f, 
	MP3_WINAMP_FastForward_f, 
	MP3_WINAMP_Rewind_f, 
	MP3_WINAMP_Prev_f, 
	MP3_WINAMP_Play_f, 
	MP3_WINAMP_Pause_f, 
	MP3_WINAMP_Stop_f, 
	MP3_WINAMP_Execute_f, 
	MP3_WINAMP_ToggleRepeat_f, 
	MP3_WINAMP_Repeat_f, 
	MP3_WINAMP_ToggleShuffle_f, 
	MP3_WINAMP_Shuffle_f, 
	MP3_WINAMP_FadeOut_f, 

	Media_WINAMP_GetVolume,
	Media_WINAMP_SetVolume,

	/* Macro's */
	MP3_WINAMP_Macro_MP3Info, 
};

#else

const mp3_player_t mp3_player_winamp = {
	"NONE",   // PlayerName_AllCaps  
	"None",   // PlayerName_LeadingCaps
	"none",   // PlayerName_NoCaps 
	MP3_NONE, // Type
};

#endif // WITH_WINAMP
