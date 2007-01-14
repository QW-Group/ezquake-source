/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

	$Id: sys_win.c,v 1.34 2007-01-14 22:02:48 qqshka Exp $

*/
// sys_win.c

#include "quakedef.h"
#include "winquake.h"
#include "resource.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <io.h>			// _open, etc
#include <direct.h>		// _mkdir
#include <conio.h>		// _putch

#define MINIMUM_WIN_MEMORY	0x0c00000
#define MAXIMUM_WIN_MEMORY	0x1000000

#define PAUSE_SLEEP		50				// sleep time on pause or minimization
#define NOT_FOCUS_SLEEP	20				// sleep time when not focus

qbool		ActiveApp, Minimized;
qbool        WinNT, Win2K;


qbool OnChange_sys_highpriority (cvar_t *, char *);
cvar_t	sys_highpriority = {"sys_highpriority", "0", 0, OnChange_sys_highpriority};


cvar_t	sys_yieldcpu = {"sys_yieldcpu", "0"};
cvar_t	sys_inactivesleep = {"sys_inactiveSleep", "1"};

static HANDLE	qwclsemaphore;
static HANDLE	tevent;
static HANDLE	hinput, houtput;

void MaskExceptions (void);
void Sys_PopFPCW (void);
void Sys_PushFPCW_SetHigh (void);

#ifndef WITHOUT_WINKEYHOOK

static HHOOK WinKeyHook;
static qbool WinKeyHook_isActive;
static qbool ScreenSaver_isDisabled;

LRESULT CALLBACK LLWinKeyHook(int Code, WPARAM wParam, LPARAM lParam);
qbool OnChange_sys_disableWinKeys(cvar_t *var, char *string);
cvar_t	sys_disableWinKeys = {"sys_disableWinKeys", "0", 0, OnChange_sys_disableWinKeys};

#ifndef id386
void Sys_HighFPPrecision(void) {}
void Sys_LowFPPrecision(void) {}
void Sys_SetFPCW(void) {}
void MaskExceptions(void) {}
#endif

qbool OnChange_sys_disableWinKeys(cvar_t *var, char *string) {
	if (Q_atof(string)) {
		if (!WinKeyHook_isActive) {
			if ((WinKeyHook = SetWindowsHookEx(13, LLWinKeyHook, global_hInstance, 0))) {
				WinKeyHook_isActive = true;
			} else {
				Com_Printf("Failed to install winkey hook.\n");
				Com_Printf("Microsoft Windows NT 4.0, 2000 or XP is required.\n");
				return true;
			}
		}
	} else {
		if (WinKeyHook_isActive) {
			UnhookWindowsHookEx(WinKeyHook);
			WinKeyHook_isActive = false;
		}
	}
	return false;
}

LRESULT CALLBACK LLWinKeyHook(int Code, WPARAM wParam, LPARAM lParam) {
	PKBDLLHOOKSTRUCT p;

	p = (PKBDLLHOOKSTRUCT) lParam;

	if (ActiveApp) {
		switch(p->vkCode) {
			case VK_LWIN: Key_Event (K_LWIN, !(p->flags & LLKHF_UP)); return 1;
			case VK_RWIN: Key_Event (K_RWIN, !(p->flags & LLKHF_UP)); return 1;
			case VK_APPS: Key_Event (K_MENU, !(p->flags & LLKHF_UP)); return 1;
			case VK_SNAPSHOT: Key_Event (K_PRINTSCR, !(p->flags & LLKHF_UP)); return 1;
		}
	}

	return CallNextHookEx(NULL, Code, wParam, lParam);
}

#endif


int Sys_SetPriority(int priority) {
    DWORD p;

	switch (priority) {
		case 0:	p = IDLE_PRIORITY_CLASS; break;
		case 1:	p = NORMAL_PRIORITY_CLASS; break;
		case 2:	p = HIGH_PRIORITY_CLASS; break;
		case 3:	p = REALTIME_PRIORITY_CLASS; break;
		default: return 0;
	}

	return SetPriorityClass(GetCurrentProcess(), p);
}

qbool OnChange_sys_highpriority (cvar_t *var, char *s) {
	int ok, q_priority;
	char *desc;
	float priority;

	priority = Q_atof(s);
	if (priority == 1) {
		q_priority = 2;
		desc = "high";
	} else if (priority == -1) {
		q_priority = 0;
		desc = "low";
	} else {
		q_priority = 1;
		desc = "normal";
	}

	if (!(ok = Sys_SetPriority(q_priority))) {
		Com_Printf("Changing process priority failed\n");
		return true;
	}

	Com_Printf("Process priority set to %s\n", desc);
	return false;
}


/*
===============================================================================
FILE IO
===============================================================================
*/

void Sys_mkdir (const char *path) {
	_mkdir (path);
}

/*
================
Sys_remove
================
*/
int Sys_remove (char *path)
{
	return remove(path);
}

/*
===============================================================================
SYSTEM IO
===============================================================================
*/

void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length) {
	DWORD  flOldProtect;

	//@@@ copy on write or just read-write?
	if (!VirtualProtect((LPVOID)startaddr, length, PAGE_READWRITE, &flOldProtect))
   		Sys_Error("Protection change failed");
}

void Sys_Error (char *error, ...) {
	va_list argptr;
	char text[1024];

	Host_Shutdown ();

	va_start (argptr, error);
//TODO: this sizeof is correct?
	vsnprintf (text, sizeof(text), error, argptr);
	va_end (argptr);

	if (dedicated)
		Sys_Printf("ERROR: %s\n", text);
	else
		MessageBox(NULL, text, "Error", 0 /* MB_OK */ );

	if (qwclsemaphore)
		CloseHandle (qwclsemaphore);

    if (ScreenSaver_isDisabled)
        SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, TRUE, 0, SPIF_SENDWININICHANGE);
 
	exit (1);
}

void Sys_Printf (char *fmt, ...) {
	va_list argptr;
	char text[1024];
	DWORD dummy;

#ifdef NDEBUG
	if (!dedicated)
		return;
#endif

	va_start (argptr,fmt);
	vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);

	WriteFile (houtput, text, strlen(text), &dummy, NULL);
}

void Sys_Quit (void) {
	if (tevent)
		CloseHandle (tevent);

	if (qwclsemaphore)
		CloseHandle (qwclsemaphore);

	if (dedicated)
		FreeConsole ();
#ifndef WITHOUT_WINKEYHOOK
	if (WinKeyHook_isActive)
		UnhookWindowsHookEx(WinKeyHook);
#endif

    if (ScreenSaver_isDisabled)
        SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, TRUE, 0, SPIF_SENDWININICHANGE);
 
	exit (0);
}

static double pfreq;
static qbool hwtimer = false;

void Sys_InitDoubleTime (void) {
	__int64 freq;

	if (!COM_CheckParm("-nohwtimer") && QueryPerformanceFrequency((LARGE_INTEGER *)&freq) && freq > 0) {
		// hardware timer available
		pfreq = (double)freq;
		hwtimer = true;
	} else {
		// make sure the timer is high precision, otherwise NT gets 18ms resolution
		timeBeginPeriod (1);
	}
}

double Sys_DoubleTime (void) {
	__int64 pcount;
	static __int64 startcount;
	static DWORD starttime;
	static qbool first = true;
	DWORD now;

	if (hwtimer) {
		QueryPerformanceCounter ((LARGE_INTEGER *)&pcount);
		if (first) {
			first = false;
			startcount = pcount;
			return 0.0;
		}
		// TODO: check for wrapping
		return (pcount - startcount) / pfreq;
	}

	now = timeGetTime();

	if (first) {
		first = false;
		starttime = now;
		return 0.0;
	}

	if (now < starttime) // wrapped?
		return (now / 1000.0) + (LONG_MAX - starttime / 1000.0);

	if (now - starttime == 0)
		return 0.0;

	return (now - starttime) / 1000.0;
}

char *Sys_ConsoleInput (void) {
	static char	text[256];
	static int len;
	INPUT_RECORD rec;
	int i, dummy, ch, numread, numevents;
	char *textCopied;

	while (1) {
		if (!GetNumberOfConsoleInputEvents (hinput, &numevents))
			Sys_Error ("Error getting # of console events");

		if (numevents <= 0)
			break;

		if (!ReadConsoleInput(hinput, &rec, 1, &numread))
			Sys_Error ("Error reading console input");

		if (numread != 1)
			Sys_Error ("Couldn't read console input");

		if (rec.EventType == KEY_EVENT) {
			if (rec.Event.KeyEvent.bKeyDown) {
				ch = rec.Event.KeyEvent.uChar.AsciiChar;
				switch (ch) {
					case '\r':
						WriteFile(houtput, "\r\n", 2, &dummy, NULL);
						if (len) {
							text[len] = 0;
							len = 0;
							return text;
						}
						break;

					case '\b':
						WriteFile(houtput, "\b \b", 3, &dummy, NULL);
						if (len)
							len--;
						break;

					default:
						if ((ch == ('V' & 31)) /* ctrl-v */ ||
							((rec.Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED) && (rec.Event.KeyEvent.wVirtualKeyCode == VK_INSERT))) {

								if ((textCopied = wcs2str(Sys_GetClipboardTextW()))) {
									i = strlen(textCopied);
									if (i + len >= sizeof(text))
										i = sizeof(text) - len - 1;
									if (i > 0) {
										textCopied[i] = 0;
										text[len] = 0;
										strcat(text, textCopied);
										WriteFile(houtput, textCopied, i, &dummy, NULL);
										len += dummy;
									}
								}
							} else if (ch >= ' ') {
								WriteFile(houtput, &ch, 1, &dummy, NULL);	
								text[len] = ch;
								len = (len + 1) & 0xff;
							}
							break;
				}
			}
		}
	}

	return NULL;
}

void Sys_SendKeyEvents (void) {
    MSG msg;

	while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE)) {
		// we always update if there are any event, even if we're paused
		scr_skipupdate = 0;

		if (!GetMessage (&msg, NULL, 0, 0))
			Host_Quit ();
      	TranslateMessage (&msg);
      	DispatchMessage (&msg);
	}
}

BOOL WINAPI HandlerRoutine (DWORD dwCtrlType) {
	switch (dwCtrlType) {
		case CTRL_C_EVENT:		
		case CTRL_BREAK_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			Cbuf_AddText ("quit\n");
			return true;
	}
	return false;
}

//Quake calls this so the system can register variables before host_hunklevel is marked
void Sys_Init (void) {

	Cvar_SetCurrentGroup(CVAR_GROUP_SYSTEM_SETTINGS);
	Cvar_Register(&sys_highpriority);
	Cvar_Register(&sys_yieldcpu);
	Cvar_Register(&sys_inactivesleep);
#ifndef WITHOUT_WINKEYHOOK
	if (WinNT)
		Cvar_Register(&sys_disableWinKeys);	
#endif
	Cvar_ResetCurrentGroup();

}

void Sys_Init_ (void) {
	OSVERSIONINFO vinfo;

	// allocate a named semaphore on the client so the front end can tell if it is alive
	if (!dedicated
#ifdef _DEBUG
		&& !COM_CheckParm("-allowmultiple")
#endif// enabled for development purposes, but disabled for official builds
	)
	{
		// mutex will fail if semaphore already exists
		qwclsemaphore = CreateMutex(
			NULL,		//Security attributes
			0,			//owner
			"qwcl");	//Semaphore name/
		if (!qwclsemaphore)
			Sys_Error ("QWCL is already running on this system");
		CloseHandle (qwclsemaphore);

		qwclsemaphore = CreateSemaphore(
			NULL,		//Security attributes
			0,			//Initial count
			1,			//Maximum count
			"qwcl");	//Semaphore name
	}

	MaskExceptions ();
	Sys_SetFPCW ();

	Sys_InitDoubleTime ();

	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	if (!GetVersionEx(&vinfo))
		Sys_Error ("Couldn't get OS info");

	if ((vinfo.dwMajorVersion < 4) || (vinfo.dwPlatformId == VER_PLATFORM_WIN32s))
		Sys_Error ("ezQuake requires at least Win95 or NT 4.0");

	WinNT = (vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT) ? true : false;
	Win2K = WinNT && (vinfo.dwMajorVersion == 5);
}

/********************************* CLIPBOARD *********************************/

#define SYS_CLIPBOARD_SIZE		256

wchar *Sys_GetClipboardTextW(void) {
	HANDLE th;
	wchar *clipText, *s, *t;
	static wchar clipboard[SYS_CLIPBOARD_SIZE];

	if (!OpenClipboard(NULL))
		return NULL;

	if (WinNT) {
		if (!(th = GetClipboardData(CF_UNICODETEXT))) {
			CloseClipboard();
			return NULL;
		}

		if (!(clipText = GlobalLock(th))) {
			CloseClipboard();
			return NULL;
		}
	} else {
		char *txt;

		if (!(th = GetClipboardData(CF_TEXT))) {
			CloseClipboard();
			return NULL;
		}

		if (!(txt = GlobalLock(th))) {
			CloseClipboard();
			return NULL;
		}
		clipText = str2wcs(txt);
	}

	s = clipText;
	t = clipboard;
	while (*s && t - clipboard < SYS_CLIPBOARD_SIZE - 1 && *s != '\n' && *s != '\r' && *s != '\b')
		*t++ = *s++;
	*t = 0;

	GlobalUnlock(th);
	CloseClipboard();

	return clipboard;
}

// copies given text to clipboard
void Sys_CopyToClipboard(char *text) {
	char *clipText;
	HGLOBAL hglbCopy;

	if (!OpenClipboard(NULL))
		return;

	if (!EmptyClipboard()) {
		CloseClipboard();
		return;
	}

	if (!(hglbCopy = GlobalAlloc(GMEM_DDESHARE, strlen(text) + 1))) {
		CloseClipboard();
		return;
	}

	if (!(clipText = GlobalLock(hglbCopy))) {
		CloseClipboard();
		return;
	}

	strcpy((char *) clipText, text);
	GlobalUnlock(hglbCopy);
	SetClipboardData(CF_TEXT, hglbCopy);

	CloseClipboard();
}

/*
==============================================================================
 WINDOWS CRAP
==============================================================================
*/

#define MAX_NUM_ARGVS	50

int		argc;
char	*argv[MAX_NUM_ARGVS];
static char exename[1024] = {0};

void ParseCommandLine (char *lpCmdLine) {
	argc = 1;
	argv[0] = exename;

	if(!GetModuleFileName(NULL, exename, sizeof(exename)-1)) // here we get loong string, with full path
		exename[0] = 0; // oh, something bad
	else
		strlcpy(exename, COM_SkipPath(exename), sizeof(exename));

	while (*lpCmdLine && (argc < MAX_NUM_ARGVS))
	{
		while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
			lpCmdLine++;

		if (*lpCmdLine)
		{
			if (*lpCmdLine == '\"')
			{
				lpCmdLine++;

				argv[argc] = lpCmdLine;
				argc++;

				while (*lpCmdLine && *lpCmdLine != '\"') // this include chars less that 32 and greate than 126... is that evil?
					lpCmdLine++;
			}
			else
			{
				argv[argc] = lpCmdLine;
				argc++;

				while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
					lpCmdLine++;
			}

			if (*lpCmdLine)
			{
				*lpCmdLine = 0;
				lpCmdLine++;
			}
		}
	}
}

void SleepUntilInput (int time) {
	MsgWaitForMultipleObjects (1, &tevent, FALSE, time, QS_ALLINPUT);
}

HINSTANCE	global_hInstance;
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	int memsize, i;
	double time, oldtime, newtime;
	MEMORYSTATUS lpBuffer;
	int bIsEnabled = 0;

	global_hInstance = hInstance;

    ScreenSaver_isDisabled = false;
 
	ParseCommandLine (lpCmdLine);

	// we need to check some parms before Host_Init is called
	COM_InitArgv (argc, argv);

	// let me use -condebug C:\condebug.log before Quake FS init, so I get ALL messages before quake fully init
	if ((i = COM_CheckParm("-condebug")) && i < COM_Argc() - 1) {
		extern FILE *qconsole_log;
		char *s = COM_Argv(i + 1);
		if (*s != '-' && *s != '+')
			qconsole_log = fopen(s, "a");
	}

#if !defined(CLIENTONLY)
	dedicated = COM_CheckParm ("-dedicated");
#endif

	if (dedicated) {
		if (!AllocConsole())
			Sys_Error ("Couldn't allocate dedicated server console");
		SetConsoleCtrlHandler (HandlerRoutine, TRUE);
		SetConsoleTitle ("fqds");
		hinput = GetStdHandle (STD_INPUT_HANDLE);
		houtput = GetStdHandle (STD_OUTPUT_HANDLE);
	}
	else {
		if ( SystemParametersInfo(SPI_GETSCREENSAVEACTIVE, 0, (PVOID)(&bIsEnabled), 0) && bIsEnabled ) {
            if ( SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, FALSE, 0, SPIF_SENDWININICHANGE) ) {
				ScreenSaver_isDisabled = true;
			}
		}
	}

	// take the greater of all the available memory or half the total memory,
	// but at least 8 Mb and no more than 16 Mb, unless they explicitly request otherwise
	lpBuffer.dwLength = sizeof(MEMORYSTATUS);
	GlobalMemoryStatus (&lpBuffer);

	memsize = lpBuffer.dwAvailPhys;

	if (memsize < MINIMUM_WIN_MEMORY)
		memsize = MINIMUM_WIN_MEMORY;

	if (memsize < (lpBuffer.dwTotalPhys >> 1))
		memsize = lpBuffer.dwTotalPhys >> 1;

	if (memsize > MAXIMUM_WIN_MEMORY)
		memsize = MAXIMUM_WIN_MEMORY;

	tevent = CreateEvent (NULL, FALSE, FALSE, NULL);
	if (!tevent)
		Sys_Error ("Couldn't create event");

	Sys_Init_();

	Sys_Printf ("Host_Init\n");
	Host_Init (argc, argv, memsize);

	oldtime = Sys_DoubleTime ();

// { probably that code may be located in Host_Init() at end of function, so this will work for both, linux and windows
	if (COM_Argc() >= 2) { // check .qtv files
		char *infile = COM_Argv(1);

		if (infile[0] && infile[0] != '-' && infile[0] != '+') {
			char tmp[1024] = {0}, *ext = COM_FileExtension(infile);

			if (!strncasecmp(ext, "qtv", sizeof("qtv")))
				snprintf(tmp, sizeof(tmp), "qtvplay \"#%s\"\n", infile);
			else if (   !strncasecmp(ext, "mvd", sizeof("mvd"))
					 || !strncasecmp(ext, "qwd", sizeof("qwd"))
					 || !strncasecmp(ext, "dem", sizeof("dem"))
					 || !strncasecmp(ext, "qwz", sizeof("qwz"))
					)
				snprintf(tmp, sizeof(tmp), "playdemo \"%s\"\n", infile);

			if (tmp[0])
				Cbuf_AddText(tmp);
		}
	}
// }

    /* main window message loop */
	while (1) {
		if (dedicated) {
			NET_Sleep(1);
		} else if (sys_inactivesleep.value) {
			// yield the CPU for a little while when paused, minimized, or not the focus
			if ((ISPAUSED && (!ActiveApp && !DDActive)) || Minimized || block_drawing) {
				SleepUntilInput (PAUSE_SLEEP);
				scr_skipupdate = 1;		// no point in bothering to draw
			} else if (!ActiveApp && !DDActive) {
				SleepUntilInput (NOT_FOCUS_SLEEP);
			}
		}

		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;
		Host_Frame (time);
		oldtime = newtime;

	}
    return TRUE;	/* return success of application */
}

int  Sys_CreateThread(DWORD (WINAPI *func)(void *), void *param)
{
    DWORD threadid;
    HANDLE thread;

    thread = CreateThread (
        NULL,               // pointer to security attributes
        0,                  // initial thread stack size
        func,               // pointer to thread function
        param,              // argument for new thread
        CREATE_SUSPENDED,   // creation flags
        &threadid);         // pointer to receive thread ID

    SetThreadPriority(thread, THREAD_PRIORITY_HIGHEST);
    ResumeThread(thread);

    return 1;
}

void MakeDirent(sys_dirent *ent, WIN32_FIND_DATA *data)
{
    FILETIME ft1;

    strncpy(ent->fname, data->cFileName, min(strlen(data->cFileName)+1, MAX_PATH_LENGTH));
    ent->fname[MAX_PATH_LENGTH-1] = 0;

    if (data->nFileSizeHigh > 0)
        ent->size = 0xffffffff;
    else
        ent->size = data->nFileSizeLow;

    FileTimeToLocalFileTime(&data->ftLastWriteTime, &ft1);
    FileTimeToSystemTime(&ft1, &ent->time);

    if (data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        ent->directory = 1;
    else
        ent->directory = 0;

    if (data->dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))
        ent->hidden = 1;
    else
        ent->hidden = 0;
}

unsigned long Sys_ReadDirFirst(sys_dirent *ent)
{
    HANDLE search;
    WIN32_FIND_DATA data;

    search = FindFirstFile("*", &data);

    if (search == (HANDLE)ERROR_INVALID_HANDLE)
        return 0;

    MakeDirent(ent, &data);

    return (unsigned long) search;
}

int Sys_ReadDirNext(unsigned long search, sys_dirent *ent)
{
    WIN32_FIND_DATA data;
    if (!FindNextFile((HANDLE)search, &data))
        return 0;

    MakeDirent(ent, &data);
    return 1;
}

void Sys_ReadDirClose(unsigned long search)
{
    FindClose((HANDLE)search);
}

int Sys_chdir (const char *path)
{
    if (SetCurrentDirectory(path))
        return 1;
    else
        return 0;
}

char * Sys_getcwd (char *buf, int bufsize)
{
    if (GetCurrentDirectory(bufsize, buf))
        return buf;
    else
        return NULL;
}

char *Sys_fullpath(char *absPath, const char *relPath, int maxLength)
{
    return _fullpath(absPath, relPath, maxLength);
} 
