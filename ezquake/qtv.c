/*
	Support for FTE QuakeTV automated startup and connecting

	$Id: qtv.c,v 1.4 2006-03-29 20:38:29 oldmanuk Exp $
*/

#include "quakedef.h"
#include "winquake.h"
#define QTV_INITIALIZATION_TIME 2*1000

#ifdef _WIN32
static HANDLE hQTVProcess = NULL;
#else
// todo - linux / mac - global variable for process identification
#endif

int QTV_GetFreePort(void)
{
	return PORT_QUAKETV;
}

qbool QTV_Running(void)
{
#ifdef _WIN32
	return hQTVProcess > 0;
#else
	// todo: linux / mac
	Com_Printf("Not implemented.\n");
	return false;
#endif
}

qbool QTV_ShutDown(void)
/*  Terminates the FTE QuakeTV process
	Sets global process handle (id) to zero
*/
{
#ifdef _WIN32
	DWORD ExitCode;
	BOOL buf;
	
	if (!QTV_Running())
		return 1;

	GetExitCodeProcess(hQTVProcess, &ExitCode);
	buf = TerminateProcess(hQTVProcess, ExitCode);
	if (buf)
		hQTVProcess = 0;
	
	return buf;
#else
	// todo: linux / mac
	Com_Printf("Not implemented.\n");
	return 0;
#endif
}

int QTV_WaitForInitialization(void)
{
	return 0;
	// return WaitForInputIdle(hQTVProcess, QTV_INITIALIZATION_TIME);
	// <- didn't work - johnnycz
}

qbool QTV_RunProcess(int port, char* ip)
/*  Runs external FTE QuakeTV executable with required parameters:
    +port port
	+connect ip
	Stores process handle into a global variable
*/
{
#ifdef _WIN32
	STARTUPINFO si;
	PROCESS_INFORMATION	pi;
	char cmdline[255];
	int retbuf;

	strlcpy(cmdline, va("%s/qtvprox.exe +port %d +connect %s", com_basedir, port, ip), sizeof(cmdline));

	memset (&si, 0, sizeof(si));
	si.cb = sizeof(si);
	si.wShowWindow = SW_HIDE;
	si.dwFlags = STARTF_USESHOWWINDOW;

	if (retbuf = CreateProcess(NULL, cmdline, NULL, NULL,
		FALSE, GetPriorityClass(GetCurrentProcess()),
		NULL, com_basedir, &si, &pi))
	{
		hQTVProcess = pi.hProcess;
	}

	return retbuf;
#else
	// todo: linux / mac
	Com_Printf("Not implemented.\n");
	return 0;
#endif
}

void StartQTV(void)
/*	Starts a FTE QuakeTV Proxy (shuts down previous instance if any)
    tells it where to connect, where to listen
	and connects the client to it
	This function is platform independent.
*/
{
	char * ip = Cmd_Argv(1);
	char cmd[255];
	int qtvport = QTV_GetFreePort();

	if (!*ip) {
		Com_Printf("Usage: qtv <ip_address>; see /describe qtv for more info\n");
		return;
	}

	if (QTV_Running()) {
		QTV_ShutDown();
		if (QTV_Running()) {
			Com_Printf("Unable to terminate previous QTV instance.\n");
			return;
		}
	}

	if (!QTV_RunProcess(qtvport, ip))
	{
		Com_Printf ("Couldn't execute QuakeTV.\n");
		return;
	}

	if (QTV_WaitForInitialization()) {
		Com_Printf ("QTV wasn't initialized successfully. Attempting to shut down.\n");
		QTV_ShutDown();
		if (QTV_Running())
			Com_Printf ("Unable to terminate QTV instance.\n");

		return;
	}

	strlcpy(cmd, va("connect localhost:%i", qtvport), sizeof(cmd));

	Cbuf_AddText(cmd);
}

void QTV_Init(void)
{
	Cmd_AddCommand("qtv", StartQTV);	
}
