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


#include "quakedef.h"
#include "mp3_player.h"
#include "utils.h"
#include "keys.h"
#include "modules.h"

#if defined(_WIN32) || defined(__XMMS__)

int COM_FileOpenRead (char *path, FILE **hndl);
char *COM_FileExtension(char *);

cvar_t mp3_scrolltitle = {"mp3_scrolltitle", "1"};
cvar_t mp3_showtime = {"mp3_showtime", "1"};
qboolean OnChange_mp3_volume(cvar_t *var, char *s);
cvar_t mp3_volume = {"mp3_volume", "1", 0, OnChange_mp3_volume};
cvar_t mp3_grabvolume = {"mp3_grabvolume", "2"};	

static char *mp3_notrunning_msg = MP3_PLAYERNAME_LEADINGCAP " is not running";

qboolean mp3_volumectrl_active = false;

#endif


#ifdef _WIN32

#include "windows.h"

static HWND mp3_hwnd = 0;

cvar_t mp3_dir = {"mp3_winamp_dir", "c:/program files/winamp"};

#endif


#ifdef __XMMS__

#include <sys/wait.h>

cvar_t mp3_dir = {"mp3_xmms_dir", "/usr/local/bin"};
cvar_t mp3_xmms_session = {"mp3_xmms_session", "0"};

#endif


#if defined(__XMMS__)

static QLIB_HANDLETYPE_T libxmms_handle = NULL;
static QLIB_HANDLETYPE_T libglib_handle = NULL;

void (*qxmms_remote_play)(gint session);
void (*qxmms_remote_pause)(gint session);
void (*qxmms_remote_stop)(gint session);
void (*qxmms_remote_jump_to_time)(gint session, gint pos);
gboolean (*qxmms_remote_is_running)(gint session);
gboolean (*qxmms_remote_is_playing)(gint session);
gboolean (*qxmms_remote_is_paused)(gint session);
gboolean (*qxmms_remote_is_repeat)(gint session);
gboolean (*qxmms_remote_is_shuffle)(gint session);
gint (*qxmms_remote_get_playlist_pos)(gint session);
void (*qxmms_remote_set_playlist_pos)(gint session, gint pos);
gint (*qxmms_remote_get_playlist_length)(gint session);
gchar *(*qxmms_remote_get_playlist_title)(gint session, gint pos);
void (*qxmms_remote_playlist_prev)(gint session);
void (*qxmms_remote_playlist_next)(gint session);
gint (*qxmms_remote_get_output_time)(gint session);
gint (*qxmms_remote_get_playlist_time)(gint session, gint pos);
gint (*qxmms_remote_get_main_volume)(gint session);
void (*qxmms_remote_set_main_volume)(gint session, gint v);
void (*qxmms_remote_toggle_repeat)(gint session);
void (*qxmms_remote_toggle_shuffle)(gint session);
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
};

static void XMMS_LoadLibrary(void) {
	if (!(libxmms_handle = QLIB_LOADLIBRARY("libxmms")))
		return;

	if (!QLib_ProcessProcdef(libxmms_handle, xmmsProcs, NUM_XMMSPROCS)) {
		QLIB_FREELIBRARY(libxmms_handle);
		return;
	}

	if (!(libglib_handle = QLIB_LOADLIBRARY("libxmms"))) {
		QLIB_FREELIBRARY(libxmms_handle);
		return;
	}

	if (!(qg_free = QLIB_GETPROCADDRESS(libglib_handle, "g_free"))) {
		QLIB_FREELIBRARY(libxmms_handle);
		QLIB_FREELIBRARY(libglib_handle);
		return;
	}
}

static void XMMS_FreeLibrary(void) {
	if (libxmms_handle) {
		QLIB_FREELIBRARY(libxmms_handle);
		QLIB_FREELIBRARY(libglib_handle);
	}
}

#endif

qboolean MP3_IsActive(void) {
#if defined(_WIN32)
	return true;
#elif defined(__XMMS__)
	return !!libxmms_handle;
#endif
}

#ifdef _WIN32

static qboolean MP3_IsPlayerRunning(void) {
	return ((mp3_hwnd = FindWindow("FuhQuake Winamp", NULL)) || (mp3_hwnd = FindWindow("Winamp v1.x", NULL)));
}

void MP3_Execute_f(void) {
	STARTUPINFO si;
	PROCESS_INFORMATION	pi;
	char path[256];
	int length;


	if (MP3_IsPlayerRunning()) {
		Com_Printf("Winamp is already running\n");
		return;
	}
	memset (&si, 0, sizeof(si));
	si.cb = sizeof(si);
	si.wShowWindow = SW_SHOWMINNOACTIVE;
	si.dwFlags = STARTF_USESHOWWINDOW;

	Q_strncpyz(path, mp3_dir.string, sizeof(path) - strlen("/winamp.exe"));
	length = strlen(path);
	if (length && (path[length - 1] == '\\' || path[length - 1] == '/'))
		path[length - 1] = 0;
	strcat(path, "/winamp.exe");
	if (!CreateProcess (NULL, va("%s /CLASS=\"FuhQuake Winamp\"", path), 
		NULL, NULL, FALSE, GetPriorityClass(GetCurrentProcess()), NULL, NULL, &si, &pi))
	{
		Com_Printf ("Couldn't execute winamp\n");
		return;
	}
	Com_Printf("Winamp is now running\n");
	return;
}

#define WINAMP_COMMAND(Name, Param)								\
	void MP3_##Name##_f(void) {									\
		int ret;												\
																\
		if (MP3_IsPlayerRunning())								\
			ret = SendMessage(mp3_hwnd, WM_COMMAND, Param, 0);	\
		else													\
			Com_Printf("%s\n", mp3_notrunning_msg);				\
	}

WINAMP_COMMAND(Prev, WINAMP_BUTTON1);
WINAMP_COMMAND(Play, WINAMP_BUTTON2);
WINAMP_COMMAND(Pause, WINAMP_BUTTON3);
WINAMP_COMMAND(Stop, WINAMP_BUTTON4);
WINAMP_COMMAND(Next, WINAMP_BUTTON5);
WINAMP_COMMAND(Rewind, WINAMP_BUTTON1_SHIFT);
WINAMP_COMMAND(FastForward, WINAMP_BUTTON5_SHIFT);
WINAMP_COMMAND(FadeOut, WINAMP_BUTTON4_SHIFT);

int MP3_GetStatus(void) {
	int ret;

	if (!MP3_IsPlayerRunning())
		return MP3_NOTRUNNING;
	ret = SendMessage(mp3_hwnd, WM_WA_IPC, 0, IPC_ISPLAYING);
	switch (ret) {
		case 3 : return MP3_PAUSED; break;
		case 1 : return MP3_PLAYING; break;
		case 0 : 
		default : return MP3_STOPPED; break;
	}
}

qboolean OnChange_mp3_volume(cvar_t *var, char *s) {
	int vol;

	if (!MP3_IsPlayerRunning())
		return false;
	if (!mp3_volumectrl_active)
		return true;
	vol = (int) (Q_atof(s) * 255.0 + 0.5);
	vol = bound (0, vol, 255);
	SendMessage(mp3_hwnd, WM_WA_IPC, vol, IPC_SETVOLUME);
	return false;
}

static void WINAMP_Set_ToggleFn(char *name, int setparam, int getparam) {
	int ret, set;

	if (!MP3_IsPlayerRunning()) {
		Com_Printf("%s\n", mp3_notrunning_msg);	
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
	if (!Q_strcasecmp(Cmd_Argv(1), "on")) {
		set = 1;
	} else if (!Q_strcasecmp(Cmd_Argv(1), "off")) {
		set = 0;
	} else if (!Q_strcasecmp(Cmd_Argv(1), "toggle")) {
		set = ret ? 0 : 1;
	} else {
		Com_Printf("Usage: %s [on|off|toggle]\n", Cmd_Argv(0));
		return;
	}
	SendMessage(mp3_hwnd, WM_WA_IPC, set, setparam);
	Com_Printf("%s set to %s\n", name, set ? "on" : "off");
}

void MP3_Repeat_f(void) {
	WINAMP_Set_ToggleFn("Repeat", IPC_SET_REPEAT, IPC_GET_REPEAT);
}

void MP3_Shuffle_f(void) {
	WINAMP_Set_ToggleFn("Shuffle", IPC_SET_SHUFFLE, IPC_GET_SHUFFLE);
}

static void WINAMP_Toggle_ToggleFn(char *name, int setparam, int getparam) {
	int ret;

	if (!MP3_IsPlayerRunning()) {
		Com_Printf("%s\n", mp3_notrunning_msg);	
		return;
	}
	ret = SendMessage(mp3_hwnd, WM_WA_IPC, 0, getparam);
	SendMessage(mp3_hwnd, WM_WA_IPC, ret ? 0 : 1, setparam);
}

void MP3_ToggleRepeat_f(void) {
	WINAMP_Toggle_ToggleFn("Repeat", IPC_SET_REPEAT, IPC_GET_REPEAT);
}

void MP3_ToggleShuffle_f(void) {
	WINAMP_Toggle_ToggleFn("Shuffle", IPC_SET_SHUFFLE, IPC_GET_SHUFFLE);
}

char *MP3_Macro_MP3Info(void) {
	char *s;
	static char title[MP3_MAXSONGTITLE];

	if (!MP3_IsPlayerRunning()) {
	
		Q_strncpyz(title, mp3_notrunning_msg, sizeof(title));
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

qboolean MP3_GetOutputtime(int *elapsed, int *total) {
	int ret1, ret2;

	if (!MP3_IsPlayerRunning())
		return false;
	if (!(ret1 = SendMessage(mp3_hwnd, WM_WA_IPC, 0, IPC_GETOUTPUTTIME)) == -1)
		return false;
	if (!(ret2 = SendMessage(mp3_hwnd, WM_WA_IPC, 1, IPC_GETOUTPUTTIME)) == -1)
		return false;
	*elapsed = ret1 / 1000;
	*total = ret2;

	return true;
}

qboolean MP3_GetToggleState(int *shuffle, int *repeat) {
	int ret1, ret2;

	if (!MP3_IsPlayerRunning())
		return false;
	if (!(ret1 = SendMessage(mp3_hwnd, WM_WA_IPC, 0, IPC_GET_SHUFFLE)) == -1)
		return false;
	if (!(ret2 = SendMessage(mp3_hwnd, WM_WA_IPC, 0, IPC_GET_REPEAT)) == -1)
		return false;
	*shuffle = ret1;
	*repeat = ret2;

	return true;
}

#endif


#ifdef __XMMS__

static qboolean MP3_IsPlayerRunning(void) {
	return (qxmms_remote_is_running((gint) XMMS_SESSION));
}

static int XMMS_pid = 0;
void MP3_Execute_f(void) {
	char path[MAX_OSPATH], *argv[2] = {"xmms", NULL}, **s;
	int i, length;

	if (MP3_IsPlayerRunning()) {
		Com_Printf("XMMS is already running\n");
		return;
	}
	Q_strncpyz(path, mp3_dir.string, sizeof(path) - strlen("/xmms"));
	length = strlen(path);
	for (i = 0; i < length; i++) {
		if (path[i] == '\\')
			path[i] = '/';
	}
	if (length && path[length - 1] == '/')
		path[length - 1] = 0;
	strcat(path, "/xmms");

	if (!(XMMS_pid = fork())) {
		execv(path, argv);
		exit(-1);
	}
	if (XMMS_pid == -1) {
		Com_Printf ("Couldn't execute XMMS\n");
		return;
	}
	for (i = 0; i < 6; i++) {
		Sys_MSleep(50);
		if (MP3_IsPlayerRunning()) {
			Com_Printf("XMMS is now running\n");
			return;
		}
	}
	Com_Printf("XMMS (probably) failed to run\n");
}

#define XMMS_COMMAND(Name, Param)						\
    void MP3_##Name##_f(void) {							\
	   if (MP3_IsPlayerRunning()) {						\
		   qxmms_remote_##Param(XMMS_SESSION);			\
	   } else {											\
		   Com_Printf("%s\n", mp3_notrunning_msg);		\
	   }												\
	   return;											\
    }

XMMS_COMMAND(Prev, playlist_prev);
XMMS_COMMAND(Play, play);
XMMS_COMMAND(Pause, pause);
XMMS_COMMAND(Stop, stop);
XMMS_COMMAND(Next, playlist_next);
XMMS_COMMAND(FadeOut, stop);

void MP3_FastForward_f(void) {
	int current;

	if (!MP3_IsPlayerRunning()) {
		Com_Printf("%s\n", mp3_notrunning_msg);
		return;
	}
	current = qxmms_remote_get_output_time(XMMS_SESSION) + 5 * 1000;
	qxmms_remote_jump_to_time(XMMS_SESSION, current);
}

void MP3_Rewind_f(void) {
	int current;

	if (!MP3_IsPlayerRunning()) {
		Com_Printf("%s\n", mp3_notrunning_msg);
		return;
	}
	current = qxmms_remote_get_output_time(XMMS_SESSION) - 5 * 1000;
	current = max(0, current);
	qxmms_remote_jump_to_time(XMMS_SESSION, current);
}

int MP3_GetStatus(void) {
	if (!MP3_IsPlayerRunning())
		return MP3_NOTRUNNING;
	if (qxmms_remote_is_paused(XMMS_SESSION))
		return MP3_PAUSED;
	if (qxmms_remote_is_playing(XMMS_SESSION))
		return MP3_PLAYING;	
	return MP3_STOPPED;
}

qboolean OnChange_mp3_volume(cvar_t *var, char *s) {
	int vol;

	if (!MP3_IsPlayerRunning())
		return false;
	if (!mp3_volumectrl_active)
		return true;
	vol = (int) (Q_atof(s) * 100 + 0.5);
	vol = bound(0, vol, 100);
	qxmms_remote_set_main_volume(XMMS_SESSION, vol);
	return false;
}

static void XMMS_Set_ToggleFn(char *name, void *togglefunc, void *getfunc) {
	int ret, set;
	gboolean (*xmms_togglefunc)(gint) = togglefunc;
	gboolean (*xmms_getfunc)(gint) = getfunc;

	if (!MP3_IsPlayerRunning()) {
		Com_Printf("%s\n", mp3_notrunning_msg);	
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
	if (!Q_strcasecmp(Cmd_Argv(1), "on")) {
		set = 1;
	} else if (!Q_strcasecmp(Cmd_Argv(1), "off")) {
		set = 0;
	} else if (!Q_strcasecmp(Cmd_Argv(1), "toggle")) {
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

void MP3_Repeat_f(void) {
	XMMS_Set_ToggleFn("Repeat", qxmms_remote_toggle_repeat, qxmms_remote_is_repeat);
}

void MP3_Shuffle_f(void) {
	XMMS_Set_ToggleFn("Shuffle", qxmms_remote_toggle_shuffle, qxmms_remote_is_shuffle);
}

void MP3_ToggleRepeat_f(void) {
	if (!MP3_IsPlayerRunning()) {
		Com_Printf("%s\n", mp3_notrunning_msg);	
		return;
	}
	qxmms_remote_toggle_repeat(XMMS_SESSION);
}

void MP3_ToggleShuffle_f(void) {
	if (!MP3_IsPlayerRunning()) {
		Com_Printf("%s\n", mp3_notrunning_msg);	
		return;
	}
	qxmms_remote_toggle_shuffle(XMMS_SESSION);
}

char *MP3_Macro_MP3Info(void) {
	int playlist_pos;
	char *s;
	static char title[MP3_MAXSONGTITLE];

	if (!MP3_IsPlayerRunning()) {
		Com_Printf("XMMS not running\n");
		return NULL;
	}
	playlist_pos = qxmms_remote_get_playlist_pos(XMMS_SESSION);
	s = qxmms_remote_get_playlist_title(XMMS_SESSION, playlist_pos);
	Q_strncpyz(title, s ? s : "", sizeof(title));
	g_free(s);
	return title;
}

qboolean MP3_GetOutputtime(int *elapsed, int *total) {
	int status, pos;

	if (!MP3_IsPlayerRunning()) {
		Com_Printf("%s\n", mp3_notrunning_msg);
		return false;
	}
	pos = qxmms_remote_get_playlist_pos(XMMS_SESSION);
	*elapsed = qxmms_remote_get_output_time(XMMS_SESSION) / 1000.0;
	*total = qxmms_remote_get_playlist_time(XMMS_SESSION, pos) / 1000.0;
	return true;
}

qboolean MP3_GetToggleState(int *shuffle, int *repeat) {
	if (!MP3_IsPlayerRunning()) 
		return false;
	*shuffle = qxmms_remote_is_shuffle(XMMS_SESSION);
	*repeat = qxmms_remote_is_repeat(XMMS_SESSION);
	return true;
}

#endif


#if defined(_WIN32) || defined(__XMMS__)

void MP3_SongInfo_f(void) {
	char *status_string, *title, *s;
	char elapsed_string[MP3_MAXSONGTITLE];
	int status, elapsed, total;

	if (!MP3_IsPlayerRunning()) {
		Com_Printf("%s\n", mp3_notrunning_msg);
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
	Q_strncpyz(elapsed_string, SecondsToMinutesString(elapsed), sizeof(elapsed_string));
	Com_Printf(va("%s %s \x10%s/%s\x11\n", status_string, title, elapsed_string, SecondsToMinutesString(total)));
}

char *MP3_Menu_SongtTitle(void) {
	static char title[MP3_MAXSONGTITLE], *macrotitle;
	int current;

	if (!MP3_IsPlayerRunning()) {
		Com_Printf("%s\n", mp3_notrunning_msg);
	    Q_strncpyz(title, mp3_notrunning_msg, sizeof(title));
	    return title;
	}
	macrotitle = MP3_Macro_MP3Info();
	MP3_GetPlaylistInfo(&current, NULL);
	if (*macrotitle)
		Q_strncpyz(title, va("%d. %s", current + 1, macrotitle), sizeof(title));
	else
		Q_strncpyz(title, MP3_PLAYERNAME_ALLCAPS, sizeof(title));
	return title;
}

#endif


#ifdef _WIN32

void MP3_LoadPlaylist_f(void) {
	COPYDATASTRUCT cds;
	char playlist[64];

	if (Cmd_Argc() == 1) {
		Com_Printf("Usage: %s <filename>\n", Cmd_Argv(0));
		return;
	}
	if (!MP3_IsPlayerRunning()) {
		Com_Printf("%s\n", mp3_notrunning_msg);
		return;
	}

	Q_strncpyz(playlist, Cmd_Args(), sizeof(playlist));

	if (!strcmp(COM_FileExtension(playlist), "pls")) {
		Com_Printf("Error: .pls playlists are not supported.  Try loading a .m3u playlist\n");
		return;
	}

	COM_ForceExtension(playlist, ".m3u");
	
	SendMessage(mp3_hwnd, WM_WA_IPC, 0, IPC_DELETE);
	cds.dwData = IPC_PLAYFILE;
	cds.lpData = playlist;
	cds.cbData = strlen(playlist) + 1;
	SendMessage(mp3_hwnd, WM_COPYDATA, (WPARAM) NULL, (LPARAM) &cds);
}

long MP3_GetPlaylist(char **buf) {
	FILE *f;
	char path[512];
	int pathlength;
	long filelength;
	COPYDATASTRUCT cds;

	if (!MP3_IsPlayerRunning())
		return -1;

	cds.dwData = IPC_CHDIR;
	cds.lpData = mp3_dir.string;
	cds.cbData = strlen(mp3_dir.string) + 1;
	SendMessage(mp3_hwnd, WM_COPYDATA, (WPARAM) NULL, (LPARAM) &cds);

	SendMessage(mp3_hwnd, WM_WA_IPC, 0, IPC_WRITEPLAYLIST);
	Q_strncpyz(path, mp3_dir.string, sizeof(path));
	pathlength = strlen(path);
	if (pathlength && (path[pathlength - 1] == '\\' || path[pathlength - 1] == '/'))
		path[pathlength - 1] = 0;
	strcat(path, "/winamp.m3u");
	filelength = COM_FileOpenRead(path, &f);
	if (!f)
		return -1;
	*buf = Q_Malloc(filelength);
	if (filelength != fread(*buf, 1,  filelength, f)) {
		free(*buf);
		fclose(f);
		return -1;
	}
	fclose(f);
	return filelength;
}

void MP3_GetPlaylistInfo(int *current, int *length) {
	if (!MP3_IsPlayerRunning())
		return;
	if (length)
		*length = SendMessage(mp3_hwnd, WM_WA_IPC, 0, IPC_GETLISTLENGTH);
	if (current)
		*current = SendMessage(mp3_hwnd, WM_WA_IPC, 0, IPC_GETLISTPOS);
}

void MP3_PrintPlaylist_f(void) {
	char *playlist_buf, *entries[256];
	unsigned int length;
	int i, playlist_size, current;

	if ((length = MP3_GetPlaylist(&playlist_buf)) == -1) {
		Com_Printf("%s\n", mp3_notrunning_msg);
		return;
	}
	MP3_GetPlaylistInfo(&current, NULL);
	playlist_size = MP3_ParsePlaylist_EXTM3U(playlist_buf, length, entries, sizeof(entries) / sizeof(entries[0]), 128);

	for (i = 0; i < playlist_size; i++) {
		Com_Printf("%s%3d %s\n", i == current ? "\x02" : "", i + 1, entries[i]);
		free(entries[i]);
	}

	free(playlist_buf);
}

void MP3_PlayTrackNum_f(void) {
	int pos, length;

	if (!MP3_IsPlayerRunning()) {
		Com_Printf("%s\n", mp3_notrunning_msg);
		return;
	}
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <track #>\n", Cmd_Argv(0));
		return;
	}
	MP3_GetPlaylistInfo(NULL, &length);
	pos = Q_atoi(Cmd_Argv(1)) - 1;
	if (pos < 0 || pos >= length)
		return;
	SendMessage(mp3_hwnd, WM_WA_IPC, pos, IPC_SETPLAYLISTPOS);
	MP3_Play_f();
}

#endif


#ifdef __XMMS__

void MP3_LoadPlaylist_f(void) {
	Com_Printf("The %s command is not (yet) supported.\n", Cmd_Argv(0));
}

void MP3_GetPlaylistInfo(int *current, int *length) {
	if (!MP3_IsPlayerRunning()) 
		return;
	if (length)
		*length = qxmms_remote_get_playlist_length(XMMS_SESSION);
	if (current)
		*current = qxmms_remote_get_playlist_pos(XMMS_SESSION);
}

void MP3_PrintPlaylist_f(void) {
	int current, length, i;
	char *title, *s;

	if (!MP3_IsPlayerRunning()) {
		Com_Printf("%s\n", mp3_notrunning_msg);
		return;
	}
	MP3_GetPlaylistInfo(&current, &length);
	for (i = 0 ; i < length; i ++) {
		title = qxmms_remote_get_playlist_title(XMMS_SESSION, i);

		if (i == current)
			for (s = title; *s; s++)
				*s |= 128;

		Com_Printf("%3d %s\n", i + 1, title);
	}
	return;
}

void MP3_PlayTrackNum_f(void) {
	int pos, length;

	if (!MP3_IsPlayerRunning()) {
		Com_Printf("%s\n", mp3_notrunning_msg);
		return;
	}
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <track #>\n", Cmd_Argv(0));
		return;
	}
	MP3_GetPlaylistInfo(NULL, &length);
	pos = Q_atoi(Cmd_Argv(1)) - 1;
	if (pos < 0 || pos >= length)
		return;
	qxmms_remote_set_playlist_pos(XMMS_SESSION, pos);
	MP3_Play_f();
}

#endif


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
			entries[playlist_size++] = strdup(s);
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
		entries[playlist_size++] = strdup(s);
	}
	return playlist_size;
}


#if defined(_WIN32) || defined(__XMMS__)

void MP3_Init(void) {
#ifdef __XMMS__
	XMMS_LoadLibrary();
#endif

	if (!MP3_IsActive())
		return;

	mp3_volumectrl_active = !COM_CheckParm("-nomp3volumectrl");

	Cmd_AddMacro("mp3info", MP3_Macro_MP3Info);

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
	Cmd_AddCommand("mp3_start" MP3_PLAYERNAME_NOCAPS, MP3_Execute_f);

	Cvar_SetCurrentGroup(CVAR_GROUP_MP3);
	Cvar_Register(&mp3_dir);
	Cvar_Register(&mp3_scrolltitle);
	Cvar_Register(&mp3_showtime);
	Cvar_Register(&mp3_volume);
	Cvar_Register(&mp3_grabvolume);
#ifdef __XMMS__
	Cvar_Register(&mp3_xmms_session);
#endif
	Cvar_ResetCurrentGroup();
}

#else

void MP3_Init(void) {}

#endif


#if defined(_WIN32) || defined(__XMMS__)


void MP3_Frame(void) {
	float vol;

	if (!mp3_grabvolume.value)
		return;

	if (!MP3_IsPlayerRunning())
		return;

#ifdef _WIN32
	vol = SendMessage(mp3_hwnd, WM_WA_IPC, -666, IPC_SETVOLUME) / 255.0;
	vol = ((int) (vol * 100)) / 100.0;
#else
	vol = qxmms_remote_get_main_volume(XMMS_SESSION) / 100.0;
#endif
	vol = bound(0, vol, 1);

	if (vol != mp3_volume.value) {
		mp3_volume.OnChange = NULL;
		Cvar_SetValue(&mp3_volume, vol);
		mp3_volume.OnChange = OnChange_mp3_volume;
	}
}

void MP3_Shutdown(void) {
#ifdef __XMMS__
	if (XMMS_pid) {
		if (!kill(XMMS_pid, SIGTERM))
			waitpid(XMMS_pid, NULL, 0);
	}
#endif
}

#else	

void MP3_Frame(void) {}

void MP3_Shutdown(void) {}

#endif
