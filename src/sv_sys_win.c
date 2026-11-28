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
#include <conio.h>
#ifdef SV_WINDOWS_MVDSV
#include <mmsystem.h>
#endif
#include <process.h>

static HANDLE console_input;
static DWORD console_input_type;
static qbool console_keyboard;
extern cvar_t sys_select_timeout;
#ifdef SV_WINDOWS_MVDSV
extern cvar_t sys_sleep, sys_simulation;
#endif
#ifdef SV_WINDOWS_GUI
static qbool isdaemon;
#else
static volatile LONG server_done;

static BOOL WINAPI Sys_StopServer(DWORD event)
{
	if (event != CTRL_C_EVENT && event != CTRL_BREAK_EVENT && event != CTRL_CLOSE_EVENT) {
		return FALSE;
	}
	InterlockedExchange(&server_done, 1);
	if (event == CTRL_CLOSE_EVENT) {
		// Returning lets Windows terminate the process. Let main finish SV_Shutdown and exit.
		Sleep(INFINITE);
	}
	return TRUE;
}

#endif // !SV_WINDOWS_GUI

void Sys_Init(void)
{
	DWORD mode;

#ifdef SV_WINDOWS_MVDSV
	timeBeginPeriod(1);
	Cvar_Register(&sys_nostdout);
	Cvar_Register(&sys_sleep);
	if (!COM_CheckParm("-nopriority")) {
		SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	}
	Cvar_Set(&sys_sleep, "0");
#endif
#ifdef SV_WINDOWS_GUI
	return; // the Windows dialog supplies input via the command buffer
#endif

#ifdef SV_WINDOWS_MVDSV
	if (COM_CheckParm("-noconinput")) {
#else
	if (COM_CheckParm(cmdline_param_client_nostdinput)) {
#endif
		return;
	}

	console_input = GetStdHandle(STD_INPUT_HANDLE);
	console_keyboard = GetConsoleMode(console_input, &mode) != FALSE;
	console_input_type = GetFileType(console_input);
}

static int Sys_ReadConsoleChar(void)
{
	unsigned char ch;
	DWORD length;

	if (console_keyboard) {
		int key;

		if (!_kbhit()) {
			return EOF;
		}
		key = _getch();
		if (key == 0 || key == 224) {
			_getch();
			return EOF;
		}
		if (key >= 32 || key == '\r' || key == '\b' || key == '\t') {
			_putch(key);
			if (key == '\r') {
				_putch('\n');
				return '\n';
			}
			return key;
		}
		return EOF;
	}

	if (console_input_type == FILE_TYPE_PIPE) {
		if (!PeekNamedPipe(console_input, NULL, 0, NULL, &length, NULL)) {
			console_input_type = FILE_TYPE_UNKNOWN;
			return EOF;
		}
		if (!length) {
			return EOF;
		}
	}
	else if (console_input_type != FILE_TYPE_DISK) {
		return EOF;
	}

	if (!ReadFile(console_input, &ch, sizeof(ch), &length, NULL) || !length) {
		console_input_type = FILE_TYPE_UNKNOWN;
		return EOF;
	}
	return ch;
}

char *Sys_ConsoleInput(void)
{
	static char line[1024];
	static size_t length;
	static qbool too_long;
	int ch;

	while ((ch = Sys_ReadConsoleChar()) != EOF) {
		if (ch == '\n') {
			line[length] = '\0';
			length = 0;
			if (too_long) {
				too_long = false;
				Con_Printf("Console command is too long.\n");
				continue;
			}
			return line;
		}
		if (console_keyboard && ch == '\b') {
			if (length && !too_long) {
				--length;
				_putch(' ');
				_putch('\b');
			}
		}
		else if (ch != '\r') {
			if (length < sizeof(line) - 1) {
				line[length++] = ch;
			}
			else {
				too_long = true;
			}
		}
	}
	if (!console_keyboard && console_input_type == FILE_TYPE_UNKNOWN && length && !too_long) {
		line[length] = '\0';
		length = 0;
		return line;
	}
	return NULL;
}

void Sys_Printf(char *fmt, ...)
{
	va_list args;
	char text[4096];

#ifdef SV_WINDOWS_GUI
	if (isdaemon) {
		return;
	}
#else
	if (sys_nostdout.value) {
		return;
	}
#endif
	va_start(args, fmt);
	vsnprintf(text, sizeof(text), fmt, args);
	va_end(args);
	Q_normalizetext(text);
#ifdef SV_WINDOWS_GUI
	ConsoleAddText(text);
#else
	fputs(text, stdout);
	fflush(stdout);
#endif
}

void Sys_Exit(int code)
{
#ifdef SV_WINDOWS_GUI
	RemoveNotifyIcon();
#endif
	exit(code);
}

void Sys_Quit(void)
{
	NET_Shutdown();
	Sys_Exit(0);
}

void Sys_Error(const char *error, ...)
{
	static qbool shutting_down;
	va_list args;
	char text[1024];

	va_start(args, error);
	vsnprintf(text, sizeof(text), error, args);
	va_end(args);
	sv_error = true;
#ifdef SV_WINDOWS_GUI
	if (!isdaemon && !COM_CheckParm("-noerrormsgbox")) {
		MessageBoxA(NULL, text, "Error", MB_OK);
	}
	else {
		Sys_Printf("ERROR: %s\n", text);
	}
#else
	fprintf(stderr, "Error: %s\n", text);
#endif
	if (!shutting_down) {
		shutting_down = true;
		SV_Write_Log(ERROR_LOG, 1, va("ERROR: %s\n", text));
		SV_Shutdown(text);
		if ((int)sys_restart_on_error.value) {
			Sys_Restart();
		}
	}
	Sys_Exit(1);
}

void Sys_Restart(void)
{
	STARTUPINFO startup = {0};
	PROCESS_INFORMATION process;
	char executable[MAX_OSPATH];
	char *command_line;
	DWORD length;

	length = GetModuleFileNameA(NULL, executable, sizeof(executable));
	if (!length || length >= sizeof(executable)) {
		fprintf(stderr, "Could not restart: could not find the server executable\n");
		Sys_Exit(1);
	}
	command_line = _strdup(GetCommandLineA());
	if (!command_line) {
		fprintf(stderr, "Could not restart: out of memory\n");
		Sys_Exit(1);
	}
	NET_Shutdown();
	startup.cb = sizeof(startup);
	if (!CreateProcessA(executable, command_line, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process)) {
		fprintf(stderr, "Could not restart (Windows error %lu)\n", GetLastError());
		Sys_Exit(1);
	}
	free(command_line);
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	Sys_Exit(0);
}

double Sys_DoubleTime(void)
{
	static LARGE_INTEGER start, frequency;
	LARGE_INTEGER now;

	if (!frequency.QuadPart) {
		QueryPerformanceFrequency(&frequency);
		QueryPerformanceCounter(&start);
	}
	QueryPerformanceCounter(&now);
	return (now.QuadPart - start.QuadPart) / (double)frequency.QuadPart;
}

typedef struct {
	int (*func)(void *);
	void *data;
} sys_thread_args_t;

static unsigned __stdcall Sys_DetachedThreadMain(void *data)
{
	sys_thread_args_t args = *(sys_thread_args_t *)data;

	free(data);
	args.func(args.data);
	return 0;
}

int Sys_CreateDetachedThread(int (*func)(void *), void *data)
{
	HANDLE thread;
	sys_thread_args_t *args = malloc(sizeof(*args));

	if (!args) {
		return -1;
	}
	args->func = func;
	args->data = data;
	thread = (HANDLE)_beginthreadex(NULL, 0, Sys_DetachedThreadMain, args, 0, NULL);
	if (!thread) {
		free(args);
		return -1;
	}
	CloseHandle(thread);
	return 0;
}

qbool Sys_MutexInit(sys_mutex_t *mutex)
{
	return InitializeCriticalSectionAndSpinCount(mutex, 0) != FALSE;
}

void Sys_MutexLock(sys_mutex_t *mutex)
{
	EnterCriticalSection(mutex);
}

void Sys_MutexUnlock(sys_mutex_t *mutex)
{
	LeaveCriticalSection(mutex);
}

qbool Sys_MutexTryLock(sys_mutex_t *mutex)
{
	return TryEnterCriticalSection(mutex) != FALSE;
}

int Sys_Script(const char *path, const char *args)
{
	STARTUPINFO startup = {0};
	PROCESS_INFORMATION process;
	char *command_line;
	BOOL started;
	char shell[MAX_OSPATH];

	DWORD length = GetFullPathNameA("sh.exe", sizeof(shell), shell, NULL);

	if (!length || length >= sizeof(shell)) {
		return 0;
	}
	command_line = Q_strdup_printf("\"%s\" %s.qws %s", shell, path, args);
	startup.cb = sizeof(startup);
	started = CreateProcessA(shell, command_line, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, fs_gamedir, &startup, &process);
	Q_free(command_line);
	if (!started) {
		return 0;
	}
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	return 1;
}

#ifndef SV_WINDOWS_GUI
int main(int argc, char **argv)
{
	double oldtime, now;

	SetConsoleCtrlHandler(Sys_StopServer, TRUE);
	Host_Init(argc, argv, 64 * 1024 * 1024);

	oldtime = Sys_DoubleTime();
	while (!InterlockedCompareExchange(&server_done, 0, 0)) {
#ifdef SV_WINDOWS_MVDSV
		int sleep_msec = (int)sys_sleep.value;

		if (sleep_msec > 0) {
			Sleep(min(sleep_msec, 13));
		}
		if (!sys_simulation.value)
#endif
			NET_Sleep((int)sys_select_timeout.value / 1000, false);
		now = Sys_DoubleTime();
		curtime += now - oldtime;
		SV_Frame(now - oldtime);
		oldtime = now;
	}

	SV_Shutdown("Server quit\n");
	Sys_Quit();
	return 0;
}

#else // SV_WINDOWS_GUI

static int argc;
static char *argv[MAX_NUM_ARGVS];

static void ParseCommandLine (char *lpCmdLine)
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
	SetWindowText_(va(SERVER_NAME ":%d - QuakeWorld server", NET_UDPSVPort()));

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


	SV_Shutdown("Server quit\n");
	NET_Shutdown();
	Sys_Exit((int)msg.wParam);

	return msg.wParam;
}

#endif // SV_WINDOWS_GUI
