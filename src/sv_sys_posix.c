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

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include "qwsvdef.h"

static qbool stdin_ready;
static qbool do_stdin = true;
static char **server_argv;

static int server_stdin_flags = -1;

static void Sys_RestoreStdin(void)
{
	if (server_stdin_flags != -1) {
		fcntl(STDIN_FILENO, F_SETFL, server_stdin_flags);
	}
}

void Sys_Restart(void)
{
	Sys_RestoreStdin();
	Sys_ExecServer(server_argv);
}

void Sys_Quit(void)
{
	NET_Shutdown();
	Sys_RestoreStdin();
	exit(0);
}

void Sys_Error(const char *error, ...)
{
	static qbool shutting_down;
	va_list args;
	char text[1024];

	Sys_RestoreStdin();
	va_start(args, error);
	vsnprintf(text, sizeof(text), error, args);
	va_end(args);
	sv_error = true;
#ifdef SV_POSIX_LEGACY_OPTIONS
	Sys_Printf("ERROR: %s\n", text);
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
	exit(1);
}

static volatile sig_atomic_t server_done;
static void Sys_StopServer(int signal_number)
{
	(void)signal_number;
	server_done = 1;
}

char *Sys_ConsoleInput(void)
{
	static char line[1024];
	static size_t length;
	static qbool too_long;
	char ch;
	ssize_t count;

	while (stdin_ready) {
		count = read(STDIN_FILENO, &ch, 1);
		if (count != 1) {
			stdin_ready = false;
			if (count == 0) {
				do_stdin = 0;
				if (length && !too_long) {
					line[length] = '\0';
					length = 0;
					return line;
				}
			}
			return NULL;
		}
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
		if (ch != '\r') {
			if (length < sizeof(line) - 1) {
				line[length++] = ch;
			}
			else {
				too_long = true;
			}
		}
	}
	return NULL;
}

int main(int argc, char **argv)
{
	extern cvar_t sys_select_timeout;
#ifdef SV_POSIX_LEGACY_OPTIONS
	extern cvar_t sys_simulation, sys_extrasleep;
#endif
	double oldtime, newtime, elapsed;
	int flags;

	server_argv = argv;
	COM_InitArgv(argc, argv);
#ifdef SV_POSIX_LEGACY_OPTIONS
	do_stdin = Sys_PreparePosixServer();
#else
	do_stdin = !COM_CheckParm(cmdline_param_client_nostdinput);
#endif
	if (do_stdin) {
		flags = fcntl(STDIN_FILENO, F_GETFL, 0);
		if (flags == -1 || fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == -1) {
			do_stdin = 0;
		}
		else {
			server_stdin_flags = flags;
		}
	}

	signal(SIGINT, Sys_StopServer);
	signal(SIGTERM, Sys_StopServer);
	signal(SIGPIPE, SIG_IGN);

#ifdef SV_POSIX_LEGACY_OPTIONS
	Host_Init(argc, argv, DEFAULT_MEM_SIZE);
	SV_Frame(0.1); // Send the initial MVDSV heartbeat before polling.
#else
	Host_Init(argc, argv, 64 * 1024 * 1024);
#endif
	oldtime = Sys_DoubleTime();
	while (!server_done) {
#ifdef SV_POSIX_LEGACY_OPTIONS
		if (!sys_simulation.value)
#endif
			stdin_ready = NET_Sleep((int)sys_select_timeout.value / 1000, do_stdin);
		if (server_done) {
			break;
		}
		newtime = Sys_DoubleTime();
		elapsed = newtime - oldtime;
		oldtime = newtime;
		curtime += elapsed;
		SV_Frame(elapsed);
#ifdef SV_POSIX_LEGACY_OPTIONS
		if (sys_extrasleep.value > 0) {
			usleep((unsigned long)sys_extrasleep.value);
		}
#endif
	}

	SV_Shutdown("Server quit\n");
	Sys_Quit();
	return 0;
}
