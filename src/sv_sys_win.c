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
 
*/

#include "qwsvdef.h"
#include <mmsystem.h>
#include <io.h>

extern cvar_t sys_restart_on_error;
extern cvar_t sys_select_timeout, sys_simulation;

cvar_t	sys_nostdout	= {"sys_nostdout", "0"};
cvar_t	sys_sleep		= {"sys_sleep", "8"};

static char title[16];

static qbool	isdaemon = false;

//==============================================================================
// WINDOWS CMD LINE CRAP
//==============================================================================

static int			argc;
static char			*argv[MAX_NUM_ARGVS];

char *Sys_GetModuleName(void)
{
	static char	exename[1024] = {0}; // static
	int i;

	if (exename[0])
		return exename;

	if(!(i = GetModuleFileName(NULL, exename, sizeof(exename)-1))) // here we get loong string, with full path
	{
		exename[0] = 0; // oh, something bad
	}
	else 
	{
		exename[i] = 0; // ensure null terminator
	}

	return exename;
}

#ifdef _CONSOLE
void ParseCommandLine (int ac, char **av)
{
	argc = 1;
	argv[0] = Sys_GetModuleName();

	for( ; argc < MAX_NUM_ARGVS && ac > 0; argc++, ac--)
	{
		argv[argc] = av[argc-1];
	}
}
#else
void ParseCommandLine (char *lpCmdLine)
{
	argc = 1;
	argv[0] = Sys_GetModuleName();

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
#endif

/*
================
Sys_FileTime
================
*/
int	Sys_FileTime (const char *path)
{
	struct _stat buf;
	return _stat (path, &buf) == -1 ? -1 : buf.st_mtime;
}

/*
================
Sys_mkdir
================
*/
void Sys_mkdir (const char *path)
{
	_mkdir(path);
}

/*
================
Sys_remove
================
*/
int Sys_remove (const char *path)
{
	return remove(path);
}

//bliP: rmdir ->
/*
================
Sys_rmdir
================
*/
int Sys_rmdir (const char *path)
{
	return _rmdir(path);
}
//<-

/*
================
Sys_listdir
================
*/

dir_t Sys_listdir (const char *path, const char *ext, int sort_type)
{
	static file_t	list[MAX_DIRFILES];
	dir_t	dir;
	HANDLE	h;
	WIN32_FIND_DATA fd;
	char	pathname[MAX_DEMO_NAME];
	qbool all;

	PCRE2_SIZE	error_offset;
	pcre2_code	*preg;
	pcre2_match_data *match_data = NULL;
	int error;

	memset(list, 0, sizeof(list));
	memset(&dir, 0, sizeof(dir));

	dir.files = list;
	all = !strncmp(ext, ".*", 3);
	if (!all)
		if (!(preg = pcre2_compile((PCRE2_SPTR)ext, PCRE2_ZERO_TERMINATED, PCRE2_CASELESS, &error, &error_offset, NULL)))
		{
			PCRE2_UCHAR error_str[256];
			pcre2_get_error_message(error, error_str, sizeof(error_str));
			Con_Printf("Sys_listdir: pcre2_compile(%s) error: %s at offset %d\n",
			           ext, error_str, error_offset);
			pcre2_code_free(preg);
			return dir;
		}

	snprintf(pathname, sizeof(pathname), "%s/*.*", path);
	if ((h = FindFirstFile (pathname , &fd)) == INVALID_HANDLE_VALUE)
	{
		if (!all)
			pcre2_code_free(preg);
		return dir;
	}

	do
	{
		if (!strncmp(fd.cFileName, ".", 2) || !strncmp(fd.cFileName, "..", 3))
			continue;
		if (!all)
		{
			match_data = pcre2_match_data_create_from_pattern(preg, NULL);
			error = pcre2_match(preg, (PCRE2_SPTR)fd.cFileName,
				strlen(fd.cFileName), 0, 0, match_data, NULL);
			if (error < 0) {
				if (error != PCRE2_ERROR_NOMATCH) {
					Con_Printf("Sys_listdir: pcre2_match(%s, %s) error code: %d\n",
						ext, fd.cFileName, error);
				}
				continue;
			}
		}

		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) //bliP: list dir
		{
			dir.numdirs++;
			list[dir.numfiles].isdir = true;
			list[dir.numfiles].size = list[dir.numfiles].time = 0;
		}
		else
		{
			list[dir.numfiles].isdir = false;
			snprintf(pathname, sizeof(pathname), "%s/%s", path, fd.cFileName);
			list[dir.numfiles].time = Sys_FileTime(pathname);
			dir.size += (list[dir.numfiles].size = fd.nFileSizeLow);
		}
		strlcpy (list[dir.numfiles].name, fd.cFileName, sizeof(list[0].name));

		if (++dir.numfiles == MAX_DIRFILES - 1)
			break;

	}
	while (FindNextFile(h, &fd));

	FindClose (h);
	if (!all)
		pcre2_code_free(preg);

	switch (sort_type)
	{
	case SORT_NO: break;
	case SORT_BY_DATE:
		qsort((void *)list, dir.numfiles, sizeof(file_t), Sys_compare_by_date);
		break;
	case SORT_BY_NAME:
		qsort((void *)list, dir.numfiles, sizeof(file_t), Sys_compare_by_name);
		break;
	}
	return dir;
}

int Sys_EnumerateFiles (char *gpath, char *match, int (*func)(char *, int, void *), void *parm)
{
	HANDLE r;
	WIN32_FIND_DATA fd; 
	char apath[MAX_OSPATH];
	char apath2[MAX_OSPATH];
	char file[MAX_OSPATH];
	char *s;
	int go;
	if (!gpath)
		return 0;

	snprintf(apath, sizeof(apath), "%s/%s", gpath, match);
	for (s = apath+strlen(apath)-1; s> apath; s--)
	{
		if (*s == '/') 
			break;
	}
	*s = '\0';

	// This is what we ask windows for.
	snprintf(file, sizeof(file), "%s/*.*", apath);

	// We need to make apath contain the path in match but not gpath
	strlcpy(apath2, match, sizeof(apath));
	match = s+1;
	for (s = apath2+strlen(apath2)-1; s> apath2; s--)
	{
		if (*s == '/')
			break;
	}
	*s = '\0';
	if (s != apath2)
		strlcat (apath2, "/", sizeof (apath2));

	r = FindFirstFile(file, &fd);
	if (r==(HANDLE)-1)
		return 1;
	go = true;
	do
	{
		if (*fd.cFileName == '.');  // Don't ever find files with a name starting with '.'
		else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)    //is a directory
		{
			if (wildcmp(match, fd.cFileName))
			{
				snprintf(file, sizeof(file), "%s%s/", apath2, fd.cFileName);
				go = func(file, fd.nFileSizeLow, parm);
			}
		}
		else
		{
			if (wildcmp(match, fd.cFileName))
			{
				snprintf(file, sizeof(file), "%s%s", apath2, fd.cFileName);
				go = func(file, fd.nFileSizeLow, parm);
			}
		}
	}
	while(FindNextFile(r, &fd) && go);
	FindClose(r);

	return go;
}

/*
================
Sys_Exit
================
*/
void Sys_Exit(int code)
{
#ifndef _CONSOLE
	RemoveNotifyIcon();
#endif
	exit(code);
}

/*
================
Sys_Quit
================
*/

void myInvalidParameterHandler(const wchar_t* expression,
   const wchar_t* function, 
   const wchar_t* file, 
   unsigned int line, 
   uintptr_t pReserved)
{
	// nothing
}

void Sys_Quit (qbool restart)
{
	if (restart)
	{
#ifndef __MINGW32__
		int maxfd = 131072; // well, should be enough for everyone...

		_set_invalid_parameter_handler(myInvalidParameterHandler); // so close() does not crash our program on invalid handle...

		// close all file descriptors even stdin stdout and stderr, seems that not hurt...
		for (; maxfd > -1; maxfd--)
		{
			close(maxfd);
			closesocket(maxfd); // yeah, windows separate sockets and files, so you can't close socket with close() like on *nix.
		}

		if (execv(argv[0], com_argv) == -1)
#endif
		{
#ifdef _CONSOLE
			if (!((int)sys_nostdout.value || isdaemon))
				printf("Restart failed: (%i): %s\n", qerrno, strerror(qerrno));
#else
			if (!(COM_CheckParm("-noerrormsgbox") || isdaemon))
				MessageBox(NULL, strerror(qerrno), "Restart failed", 0 /* MB_OK */ );
#endif
			Sys_Exit(1);
		}
	}
	Sys_Exit(0);
}

/*
================
Sys_Error
================
*/
void Sys_Error (const char *error, ...)
{
	static qbool inerror = false;
	va_list argptr;
	char text[1024];

	sv_error = true;

	if (inerror)
		Sys_Exit (1);

	inerror = true;

	va_start (argptr, error);
	vsnprintf (text, sizeof (text), error, argptr);
	va_end (argptr);

#ifdef _CONSOLE
	if (!((int)sys_nostdout.value || isdaemon))
		printf ("ERROR: %s\n", text);
#else
	if (!(COM_CheckParm ("-noerrormsgbox") || isdaemon))
		MessageBox (NULL, text, "Error", 0 /* MB_OK */ );
	else
		Sys_Printf ("ERROR: %s\n", text);

#endif

	if (logs[ERROR_LOG].sv_logfile)
	{
		SV_Write_Log (ERROR_LOG, 1, va ("ERROR: %s\n", text));
//		fclose (logs[ERROR_LOG].sv_logfile);
	}

// FIXME: hack - checking SV_Shutdown with svs.socketip set in -1 NET_Shutdown
	if (svs.socketip != -1)
		SV_Shutdown (va("ERROR: %s\n", text));

	if ((int)sys_restart_on_error.value)
		Sys_Quit (true);

	Sys_Exit (1);
}

static double pfreq;
static qbool hwtimer = false;
static __int64 startcount;
void Sys_InitDoubleTime (void)
{
	__int64 freq;

	if (!COM_CheckParm("-nohwtimer") &&
		QueryPerformanceFrequency ((LARGE_INTEGER *)&freq) && freq > 0)
	{
		// hardware timer available
		pfreq = (double)freq;
		hwtimer = true;
		QueryPerformanceCounter ((LARGE_INTEGER *)&startcount);
	}
	else
	{
		// make sure the timer is high precision, otherwise
		// NT gets 18ms resolution
		timeBeginPeriod (1);
	}
}

double Sys_DoubleTime (void)
{
	__int64 pcount;

	static DWORD starttime;
	static qbool first = true;
	DWORD now;

	if (hwtimer)
	{
		QueryPerformanceCounter ((LARGE_INTEGER *)&pcount);
		if (first) {
			first = false;
			startcount = pcount;
			return 0.0;
		}
		// TODO: check for wrapping; is it necessary?
		return (pcount - startcount) / pfreq;
	}

	now = timeGetTime();

	if (first)
	{
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

/*
================
Sys_ConsoleInput
================
*/
char *Sys_ConsoleInput (void)
{
#ifdef _CONSOLE

	static char	text[256], *t;
	static int	len = 0;

	int		c;

	// read a line out
	if (!isdaemon)
		while (_kbhit())
		{
			c = _getch();

			if (c == 224)
			{
				if (_kbhit())
				{
					// assume escape sequence (arrows etc), skip
					_getch();
					continue;
				}
				// assume character
			}

			if (c < 32 && c != '\r' && c != 8)
				continue;

			putch (c);
			if (c == '\r')
			{
				text[len] = 0;
				putch ('\n');
				len = 0;
				return text;
			}
			if (c == 8)
			{
				if (len)
				{
					putch (' ');
					putch (c);
					len--;
					text[len] = 0;
				}
				continue;
			}

			text[len] = c;
			len++;
			text[len] = 0;
			if (len == sizeof(text))
				len = 0;
		}
#endif

	// If you searching where input added under non console application, then you should check DialogFunc WM_COMMAND.

	return NULL;
}

/*
================
Sys_Printf
================
*/
void Sys_Printf(char* fmt, ...)
{
	va_list argptr;
	char text[MAXCMDBUF];
	char* startpos;
	char* endpos;
	date_t      date;

#ifdef _CONSOLE
	if ((int)sys_nostdout.value) {
		return;
	}
#endif

	if (isdaemon) {
		return;
	}

	va_start(argptr, fmt);
	vsnprintf(text, MAXCMDBUF, fmt, argptr);
	va_end(argptr);

	// normalize text before add to console.
	Q_normalizetext(text);

#ifndef _CONSOLE
	ConsoleAddText(text);
#else
	SV_TimeOfDay(&date, "%Y-%m-%d %H:%M:%S");
	startpos = text;
	while (startpos && startpos[0]) {
		endpos = strchr(startpos, '\n');
		if (endpos) {
			*endpos = '\0';
		}
		fprintf(stdout, "[%s] %s\n", date.str, startpos);
		fflush(stdout);
		if (endpos) {
			startpos = endpos + 1;
			if (startpos[0] == (char)10) {
				++startpos;
			}
		}
		else {
			break;
		}
	}
#endif //_CONSOLE
}

/*
=============
Sys_Init
 
Quake calls this so the system can register variables before host_hunklevel
is marked
=============
*/
void Sys_Init (void)
{
	qbool	WinNT;
	OSVERSIONINFO	vinfo;

	// make sure the timer is high precision, otherwise
	// NT gets 18ms resolution
	timeBeginPeriod( 1 );

	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	if (!GetVersionEx (&vinfo))
		Sys_Error ("Couldn't get OS info");

	if (vinfo.dwMajorVersion < 4 || vinfo.dwPlatformId == VER_PLATFORM_WIN32s)
		Sys_Error (SERVER_NAME " requires at least Win95 or NT 4.0");

	WinNT = (vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT ? true : false);

	Cvar_Register (&sys_nostdout);
	Cvar_Register (&sys_sleep);

	if (COM_CheckParm ("-nopriority"))
	{
		Cvar_Set (&sys_sleep, "0");
	}
	else
	{
		if ( ! SetPriorityClass (GetCurrentProcess(), HIGH_PRIORITY_CLASS))
			Con_Printf ("SetPriorityClass() failed\n");
		else
			Con_Printf ("Process priority class set to HIGH\n");

		// sys_sleep > 0 seems to cause packet loss on WinNT (why?)
		if (WinNT)
			Cvar_Set (&sys_sleep, "0");
	}

	Sys_InitDoubleTime ();
}

void Sys_Sleep (unsigned long ms)
{
	Sleep (ms);
}

int Sys_Script (const char *path, const char *args)
{
	STARTUPINFO			si;
	PROCESS_INFORMATION	pi;
	char cmdline[1024], curdir[MAX_OSPATH];

	memset (&si, 0, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_SHOWMINNOACTIVE;

	GetCurrentDirectory(sizeof(curdir), curdir);


	snprintf(cmdline, sizeof(cmdline), "%s\\sh.exe %s.qws %s", curdir, path, args);
	strlcat(curdir, va("\\%s", fs_gamedir+2), MAX_OSPATH);

	return CreateProcess (NULL, cmdline, NULL, NULL,
	                      FALSE, 0/*DETACHED_PROCESS /*CREATE_NEW_CONSOLE*/ , NULL, curdir, &si, &pi);
}

DL_t Sys_DLOpen (const char *path)
{
	return LoadLibrary (path);
}

qbool Sys_DLClose (DL_t dl)
{
	return FreeLibrary (dl);
}

void *Sys_DLProc (DL_t dl, const char *name)
{
	return (void *) GetProcAddress (dl, name);
}

int Sys_CreateThread(DWORD (WINAPI *func)(void *), void *param)
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

    ResumeThread(thread);

    return 1;
}

#ifdef _CONSOLE
/*
==================
main
 
==================
*/

int main(int ac, char *av[])
{
	double			newtime, time, oldtime;
	int				sleep_msec;

	ParseCommandLine (ac, av);

	COM_InitArgv (argc, argv);

	GetConsoleTitle(title, sizeof(title));

	Host_Init(argc, argv, DEFAULT_MEM_SIZE);

	// run one frame immediately for first heartbeat
	SV_Frame (0.1);

	//
	// main loop
	//
	oldtime = Sys_DoubleTime () - 0.1;

	while (1)
	{
		sleep_msec = (int)sys_sleep.value;
		if (sleep_msec > 0)
		{
			if (sleep_msec > 13)
				sleep_msec = 13;
			Sleep (sleep_msec);
		}

		// select on the net socket and stdin
		// the only reason we have a timeout at all is so that if the last
		// connected client times out, the message would not otherwise
		// be printed until the next event.
		if (!sys_simulation.value) {
			NET_Sleep((int)sys_select_timeout.value / 1000, false);
		}

		// find time passed since last cycle
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;
		oldtime = newtime;

		curtime = newtime;
		SV_Frame (time);
	}

	return true;
}

#else  // _CONSOLE

int APIENTRY WinMain(   HINSTANCE   hInstance,
						HINSTANCE   hPrevInstance,
						LPSTR       lpCmdLine,
						int         nCmdShow)
{
	static MSG			msg;
	static double		newtime, time, oldtime;
	register int		sleep_msec;

	static qbool		disable_gpf = false;

	ParseCommandLine(lpCmdLine);

	COM_InitArgv (argc, argv);

	// create main window
	if (!CreateMainWindow(hInstance, nCmdShow))
		return 1;

	if (COM_CheckParm("-noerrormsgbox"))
		disable_gpf = true;

	if (COM_CheckParm ("-d"))
	{
		isdaemon = disable_gpf = true;
		//close(0); close(1); close(2);
	}

	if (disable_gpf)
	{
		DWORD dwMode = SetErrorMode(SEM_NOGPFAULTERRORBOX);
		SetErrorMode(dwMode | SEM_NOGPFAULTERRORBOX);
	}
	
	Host_Init(argc, argv, DEFAULT_MEM_SIZE);

	// if stared miminize update notify icon message (with correct port)
	if (minimized)
		UpdateNotifyIconMessage(va(SERVER_NAME ":%d", NET_UDPSVPort()));

	// run one frame immediately for first heartbeat
	SV_Frame (0.1);

	//
	// main loop
	//
	oldtime = Sys_DoubleTime () - 0.1;

	while(1)
	{
		// get messeges sent to windows
		if( PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE ) )
		{
			if( !GetMessage( &msg, NULL, 0, 0 ) )
				break;
			if(!IsDialogMessage(DlgHwnd, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		CheckIdle();

		// server frame

		sleep_msec = (int)sys_sleep.value;
		if (sleep_msec > 0)
		{
			if (sleep_msec > 13)
				sleep_msec = 13;
			Sleep (sleep_msec);
		}

		// select on the net socket and stdin
		// the only reason we have a timeout at all is so that if the last
		// connected client times out, the message would not otherwise
		// be printed until the next event.
		if (!sys_simulation.value) {
			NET_Sleep((int)sys_select_timeout.value / 1000, false);
		}

		// find time passed since last cycle
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;
		oldtime = newtime;

		curtime = newtime;
		SV_Frame (time);
	}


	Sys_Exit(msg.wParam);

	return msg.wParam;
}

#endif // _CONSOLE
