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

// this should be the only file that includes both server.h and client.h

#include "quakedef.h"
#include "pmove.h"
#include "version.h"
#include "modules.h"
#include <setjmp.h>


#if !defined(CLIENTONLY) && !defined(SERVERONLY)
qboolean	dedicated = false;
#endif

double		curtime;

static int	host_hunklevel;
static void	*host_membase;

qboolean	host_initialized;	// true if into command execution
int			host_memsize;

static jmp_buf 	host_abort;

void Host_Abort (void) {
	longjmp (host_abort, 1);
}

void Host_EndGame (void) {
	SV_Shutdown ("Server was killed");
	CL_Disconnect ();
	// clear disconnect messages from loopback
	NET_ClearLoopback ();
}

//This shuts down both the client and server
void Host_Error (char *error, ...) {
	va_list argptr;
	char string[1024];
	static qboolean inerror = false;

	if (inerror)
		Sys_Error ("Host_Error: recursively entered");
	inerror = true;

	va_start (argptr,error);
	vsnprintf (string, sizeof(string), error, argptr);
	va_end (argptr);

	Com_Printf ("\n===========================\n");
	Com_Printf ("Host_Error: %s\n",string);
	Com_Printf ("===========================\n\n");

	SV_Shutdown (va("server crashed: %s\n", string));
	CL_Disconnect ();

	if (dedicated) {
		NET_Shutdown ();
		COM_Shutdown ();
		Sys_Error ("%s", string);
	}

	if (!host_initialized)
		Sys_Error ("Host_Error: %s", string);

	inerror = false;

	Host_Abort ();
}

//memsize is the recommended amount of memory to use for hunk
void Host_InitMemory (int memsize) {
	int t;

	if (COM_CheckParm ("-minmemory"))
		memsize = MINIMUM_MEMORY;

	if ((t = COM_CheckParm ("-heapsize")) != 0 && t + 1 < com_argc)
		memsize = Q_atoi (com_argv[t + 1]) * 1024;

	if ((t = COM_CheckParm ("-mem")) != 0 && t + 1 < com_argc)
		memsize = Q_atoi (com_argv[t + 1]) * 1024 * 1024;

	if (memsize < MINIMUM_MEMORY)
		Sys_Error ("Only %4.1f megs of memory reported, can't execute game", memsize / (float)0x100000);

	host_memsize = memsize;
	host_membase = Q_Malloc (host_memsize);
	Memory_Init (host_membase, host_memsize);
}

//Free hunk memory up to host_hunklevel
//Can only be called when changing levels!
void Host_ClearMemory (void) {
	D_FlushCaches ();
	Mod_ClearAll ();

	// any data previously allocated on hunk is no longer valid
	Hunk_FreeToLowMark (host_hunklevel);
}

void Host_Frame (double time) {
	if (setjmp (host_abort))
		return;			// something bad happened, or the server disconnected

	curtime += time;

	if (dedicated)
		SV_Frame (time);
	else
		CL_Frame (time);	// will also call SV_Frame
}

char *Host_PrintBars(char *s, int len) {
	static char temp[512];									
	int i, count;

	temp[0] = 0;

	count = (len - 2 - 2 - strlen(s)) / 2;
	if (count < 0 || count > sizeof(temp) / 2 - 8)
		return temp;

	strcat(temp, "\x1d");
	for (i = 0; i < count; i++)
		strcat(temp, "\x1e");
	strcat(temp, " ");
	strcat(temp, s);
	strcat(temp, " ");
	for (i = 0; i < count; i++)
		strcat(temp, "\x1e");
	strcat(temp, "\x1f");
	strcat(temp, "\n");

	return temp;
}

void CL_SaveArgv(int, char **);

void Host_Init (int argc, char **argv, int default_memsize) {
	FILE *f;

	COM_InitArgv (argc, argv);

	Host_InitMemory (default_memsize);

	Cbuf_Init ();
	Cmd_Init ();
	Cvar_Init ();
	COM_Init ();

	FS_InitFilesystem ();
	COM_CheckRegistered ();

	Cbuf_AddEarlyCommands ();
	Cbuf_Execute ();

	Con_Init ();
	NET_Init ();
	Netchan_Init ();
	QLib_Init();
	Sys_Init ();
	PM_Init ();
	Mod_Init ();

	SV_Init ();
	CL_Init ();

#ifndef SERVERONLY
	
	if (!dedicated)
		CL_SaveArgv(argc, argv);
#endif

	Hunk_AllocName (0, "-HOST_HUNKLEVEL-");
	host_hunklevel = Hunk_LowMark ();

	host_initialized = true;

	Com_Printf ("Exe: "__TIME__" "__DATE__"\n");
	Com_Printf ("Hunk allocation: %4.1f MB.\n", (float) host_memsize / (1024 * 1024));

	Com_Printf ("\nFuhQuake version %s\n\n", VersionString());
	if (dedicated) {
		Com_Printf ("========== www.fuhquake.net ==========\n\n");
		Com_Printf ("======== FuhQuake Initialized ========\n\n");
	} else {
		Com_Printf (Host_PrintBars("www\x9c" "fuhquake\x9c" "net", 38));
		Com_Printf(Host_PrintBars("FuhQuake Initialized", 38));
	}

	if (dedicated) {
		Cbuf_AddText ("exec server.cfg\n");
		Cmd_StuffCmds_f ();		// process command line arguments
		Cbuf_Execute ();

		// if a map wasn't specified on the command line, spawn start map
		if (!com_serveractive)
			Cmd_ExecuteString ("map start");
		if (!com_serveractive)
			Host_Error ("Couldn't spawn a server");
	} else {
		Cbuf_AddText ("exec default.cfg\n");
		if (FS_FOpenFile("config.cfg", &f) != -1) {
			Cbuf_AddText ("exec config.cfg\n");
			fclose(f);
		}
		if (FS_FOpenFile("autoexec.cfg", &f) != -1) {
			Cbuf_AddText ("exec autoexec.cfg\n");
			fclose(f);
		}
		Cmd_StuffCmds_f ();		// process command line arguments
		Cbuf_AddText ("cl_warncmd 1\n");
	}
}

//FIXME: this is a callback from Sys_Quit and Sys_Error.  It would be better
//to run quit through here before the final handoff to the sys code.
void Host_Shutdown (void) {
	static qboolean isdown = false;

	if (isdown) {
		printf ("recursive shutdown\n");
		return;
	}
	isdown = true;

	SV_Shutdown ("Server quit\n");
	QLib_Shutdown();
	CL_Shutdown ();
	NET_Shutdown ();
#ifndef SERVERONLY
	Con_Shutdown();
#endif
	COM_Shutdown ();
}

void Host_Quit (void) {
	Host_Shutdown ();
	Sys_Quit ();
}
