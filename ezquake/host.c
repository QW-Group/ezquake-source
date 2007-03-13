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
 
	$Id: host.c,v 1.36 2007-03-13 09:07:16 qqshka Exp $
*/
// this should be the only file that includes both server.h and client.h

#ifdef _WIN32
#include <windows.h>
#endif
#include <setjmp.h>

#ifdef __FreeBSD__
#include <sys/types.h>
#include <sys/sysctl.h>
#ifdef id386
#include <sys/time.h>
#include <machine/cpufunc.h>
#endif
#endif
#include "quakedef.h"
#include "EX_browser.h"
#ifdef WITH_TCL
#include "embed_tcl.h"
#endif
#include "modules.h"
#ifdef GLQUAKE
#include "gl_model.h"
#include "gl_local.h"
#else
#include "r_model.h"
#include "r_local.h"
#endif
#include "rulesets.h"
#include "teamplay.h"
#include "pmove.h"
#include "version.h"
#include "qsound.h"
#include "keys.h"


#if !defined(CLIENTONLY) && !defined(SERVERONLY)
qbool	dedicated = false;
#endif

double		curtime;

static int	host_hunklevel;
static void	*host_membase;

qbool	host_initialized;	// true if into command execution
int			host_memsize;

static jmp_buf 	host_abort;

extern void COM_StoreOriginalCmdline(int argc, char **argv);

char f_system_string[1024] = "";

char * SYSINFO_GetString(void)
{
	return f_system_string;
}
int     SYSINFO_memory = 0;
int     SYSINFO_MHz = 0;
char *  SYSINFO_processor_description = NULL;
char *  SYSINFO_3D_description        = NULL;

#ifdef _WIN32
void SYSINFO_Init(void)
{
	MEMORYSTATUS    memstat;
	LONG            ret;
	HKEY            hKey;

	GlobalMemoryStatus(&memstat);
	SYSINFO_memory = memstat.dwTotalPhys;

	ret = RegOpenKey(
	          HKEY_LOCAL_MACHINE,
	          "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
	          &hKey);

	if (ret == ERROR_SUCCESS) {
		DWORD type;
		byte  data[1024];
		DWORD datasize;

		datasize = 1024;
		ret = RegQueryValueEx(
		          hKey,
		          "~MHz",
		          NULL,
		          &type,
		          data,
		          &datasize);

		if (ret == ERROR_SUCCESS  &&  datasize > 0  &&  type == REG_DWORD)
			SYSINFO_MHz = *((DWORD *)data);

		datasize = 1024;
		ret = RegQueryValueEx(
		          hKey,
		          "ProcessorNameString",
		          NULL,
		          &type,
		          data,
		          &datasize);

		if (ret == ERROR_SUCCESS  &&  datasize > 0  &&  type == REG_SZ)
			SYSINFO_processor_description = Q_strdup((char *) data);

		RegCloseKey(hKey);
	}

#ifdef GLQUAKE
	{
		extern const char *gl_renderer;

		if (gl_renderer  &&  gl_renderer[0])
			SYSINFO_3D_description = Q_strdup(gl_renderer);
	}
#endif

	snprintf(f_system_string, sizeof(f_system_string), "%dMB", (int)(SYSINFO_memory / 1024. / 1024. + .5));

	if (SYSINFO_processor_description) {
		strlcat(f_system_string, ", ", sizeof(f_system_string));
		strlcat(f_system_string, SYSINFO_processor_description, sizeof(f_system_string));
	}
	if (SYSINFO_MHz) {
		strlcat(f_system_string, va(" %dMHz", SYSINFO_MHz), sizeof(f_system_string));
	}
	if (SYSINFO_3D_description) {
		strlcat(f_system_string, ", ", sizeof(f_system_string));
		strlcat(f_system_string, SYSINFO_3D_description, sizeof(f_system_string));
	}

}
#elif defined(__linux__)
void SYSINFO_Init(void)
{
	// disconnect: which way is best(MEM/CPU-MHZ/CPU-MODEL)?
	f_system_string[0] = 0;
	char buffer[1024];
	char cpu_model[255];
	char *match;
	FILE *f;

	// MEM
	f = fopen("/proc/meminfo", "r");
	if (f) {
		fscanf (f, "%*s %u %*s\n", &SYSINFO_memory);
		fclose (f);
		SYSINFO_memory /= 1024;
	} else {
		Com_Printf ("could not open /proc/meminfo!\n");
	}

	//CPU-MHZ
	f = fopen("/proc/cpuinfo", "r");
	if (f) {
		fread (buffer, 1, sizeof(buffer) - 1, f);
		fclose (f);
		buffer[sizeof(buffer)] = '\0';
		match = strstr (buffer, "cpu MHz");
		sscanf (match, "cpu MHz : %i", &SYSINFO_MHz);
	} else {
		Com_Printf ("could not open /proc/cpuinfo!\n");
	}

	//CPU-MODEL
	f = fopen("/proc/cpuinfo", "r");
	if (f) {
		while (!feof(f)) {
			fgets (buffer, sizeof(buffer), f); // disconnect: sizeof(buffer) - 1 ?
			if (!strncmp( buffer, "model name", 10)) {
				match = strchr( buffer, ':' );
				match++;
				while (isspace(*match)) match++;
				memcpy(cpu_model, match, sizeof(cpu_model));
				cpu_model[strlen(cpu_model) - 1] = '\0';
				SYSINFO_processor_description= Q_strdup (cpu_model);
			}
		}
		fclose(f);	
	} else {
		Com_Printf("could not open /proc/cpuinfo!\n");
	}

#ifdef GLQUAKE
	{
		extern const char *gl_renderer;

		if (gl_renderer  &&  gl_renderer[0])
			SYSINFO_3D_description = Q_strdup(gl_renderer);
	}
#endif

	snprintf(f_system_string, sizeof(f_system_string), "%dMB", (int)(SYSINFO_memory));

	if (SYSINFO_processor_description) {
		strlcat(f_system_string, ", ", sizeof(f_system_string));
		strlcat(f_system_string, SYSINFO_processor_description, sizeof(f_system_string));
	}
	if (SYSINFO_MHz) {
		strlcat(f_system_string, va(" %dMHz", SYSINFO_MHz), sizeof(f_system_string));
	}
	if (SYSINFO_3D_description) {
		strlcat(f_system_string, ", ", sizeof(f_system_string));
		strlcat(f_system_string, SYSINFO_3D_description, sizeof(f_system_string));
	}
}
#elif defined(__APPLE__)
void SYSINFO_Init(void)
{
	// TODO: disconnect --> f_system for MacOSX (man sysctl)
	// VVD: Look at code for FreeBSD: 30 lines down. :-)
#ifdef GLQUAKE
	{
		extern const char *gl_renderer;

		if (gl_renderer  &&  gl_renderer[0])
			SYSINFO_3D_description = Q_strdup(gl_renderer);
	}
#endif

	snprintf(f_system_string, sizeof(f_system_string), "%dMB", (int)(SYSINFO_memory / 1024. / 1024. + .5));

	if (SYSINFO_processor_description) {
		strlcat(f_system_string, ", ", sizeof(f_system_string));
		strlcat(f_system_string, SYSINFO_processor_description, sizeof(f_system_string));
	}
	if (SYSINFO_MHz) {
		strlcat(f_system_string, va(" %dMHz", SYSINFO_MHz), sizeof(f_system_string));
	}
	if (SYSINFO_3D_description) {
		strlcat(f_system_string, ", ", sizeof(f_system_string));
		strlcat(f_system_string, SYSINFO_3D_description, sizeof(f_system_string));
	}
}
#elif defined(__FreeBSD__)
void SYSINFO_Init(void)
{
	char cpu_model[256];

	int mib[2], val;
	size_t len;

#ifdef id386
	unsigned long long old_tsc, tsc_freq;
	struct timeval tp, old_tp;
#endif

	mib[0] = CTL_HW;
	mib[1] = HW_PHYSMEM;
// VVD: We can use HW_REALMEM (hw.realmem) for RELENG_5/6/7 for getting exact result,
// but RELENG_4 have only HW_PHYSMEM (hw.physmem).
	len = sizeof(val);
	sysctl(mib, sizeof(mib) / sizeof(mib[0]), &val, &len, NULL, 0);

	SYSINFO_memory = val;

	mib[0] = CTL_HW;
	mib[1] = HW_MODEL;
	len = sizeof(cpu_model);
	sysctl(mib, sizeof(mib) / sizeof(mib[0]), cpu_model, &len, NULL, 0);
	cpu_model[sizeof(cpu_model) - 1] = '\0';

	SYSINFO_processor_description = cpu_model;

#ifdef id386
	gettimeofday(&old_tp, NULL);
	old_tsc = rdtsc();
	do {
		gettimeofday(&tp, NULL);
	} while ((tp.tv_sec - old_tp.tv_sec) * 1000000. + tp.tv_usec - old_tp.tv_usec < 1000000.);
	tsc_freq = rdtsc();
	SYSINFO_MHz = (int)((tsc_freq - old_tsc) /
						(tp.tv_sec - old_tp.tv_sec + (tp.tv_usec - old_tp.tv_usec) / 1000000.) /
						1000000. + .5);
// VVD: We can use sysctl hw.clockrate, but it don't work on i486 - always 0
// (don't know about Pentium 1/2/3 - work on Pentium 4) and RELENG_4 have no this sysctl.
#endif

#ifdef GLQUAKE
	{
		extern const char *gl_renderer;

		if (gl_renderer  &&  gl_renderer[0])
			SYSINFO_3D_description = Q_strdup(gl_renderer);
	}
#endif

	snprintf(f_system_string, sizeof(f_system_string), "%dMB", (int)(SYSINFO_memory / 1024. / 1024. + .5));

	if (SYSINFO_processor_description) {
		strlcat(f_system_string, ", ", sizeof(f_system_string));
		strlcat(f_system_string, SYSINFO_processor_description, sizeof(f_system_string));
	}
	if (SYSINFO_MHz) {
		strlcat(f_system_string, va(" (%dMHz)", SYSINFO_MHz), sizeof(f_system_string));
	}
	if (SYSINFO_3D_description) {
		strlcat(f_system_string, ", ", sizeof(f_system_string));
		strlcat(f_system_string, SYSINFO_3D_description, sizeof(f_system_string));
	}
}
#else
void SYSINFO_Init(void)
{
	strlcpy(f_system_string, "Unknown system.", sizeof(f_system_string));
}
#endif

void Host_Abort (void)
{
	longjmp (host_abort, 1);
}

void Host_EndGame (void)
{
	SV_Shutdown ("Server was killed");
	CL_Disconnect ();
	// clear disconnect messages from loopback
	NET_ClearLoopback ();
}

//This shuts down both the client and server
void Host_Error (char *error, ...)
{
	va_list argptr;
	char string[1024];
	static qbool inerror = false;

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
		Sys_Error ("%s", string);
	}

	if (!host_initialized)
		Sys_Error ("Host_Error: %s", string);

	inerror = false;

	Host_Abort ();
}

//memsize is the recommended amount of memory to use for hunk
void Host_InitMemory (int memsize)
{
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
	host_membase = Q_malloc (host_memsize);
	Memory_Init (host_membase, host_memsize);
}

//Free hunk memory up to host_hunklevel
//Can only be called when changing levels!
void Host_ClearMemory (void)
{
	// FIXME, move to CL_ClearState
	if (!dedicated)
		D_FlushCaches ();

	// FIXME, move to CL_ClearState
#ifndef SERVERONLY
	if (!dedicated)
		Mod_ClearAll ();
#endif

	CM_InvalidateMap ();

	// any data previously allocated on hunk is no longer valid
	Hunk_FreeToLowMark (host_hunklevel);
}

void Host_Frame (double time)
{
	if (setjmp (host_abort))
		return;			// something bad happened, or the server disconnected

	curtime += time;

	if (dedicated)
		SV_Frame (time);
	else
		CL_Frame (time);	// will also call SV_Frame
}

char *Host_PrintBars(char *s, int len)
{
	static char temp[512];
	unsigned int i, count;

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
	strcat(temp, "\n\n");

	return temp;
}

void Host_Init (int argc, char **argv, int default_memsize)
{
	FILE *f;
	cvar_t *v;

	COM_InitArgv (argc, argv);
	COM_StoreOriginalCmdline(argc, argv);

	Host_InitMemory (default_memsize);

#ifdef WITH_TCL
	// interpreter should be initialized
	// before any cvar definitions
	TCL_InterpInit ();
#endif
	Cbuf_Init ();
	Cmd_Init ();
	Cvar_Init ();
	COM_Init ();
	Key_Init ();

	FS_InitFilesystem ();

	if (!dedicated) {
		char cfg[MAX_PATH] = {0};

		Cbuf_AddText("exec default.cfg\n");

		snprintf(cfg, sizeof(cfg), "%s/config.cfg", com_homedir);

		if ((f = fopen(cfg, "r"))) { // found cfg in home dir, use it
		    extern void LoadHomeCfg(const char *filename);

			fclose(f);

			Com_Printf("Using home config %s\n", cfg);
			LoadHomeCfg("config.cfg"); // well, we can't use exec here, because exec does't support full path by design
		}
		else {
			snprintf(cfg, sizeof(cfg), "%s/ezquake/configs/config.cfg", com_basedir);

			if ((f = fopen(cfg, "r"))) { // found cfg in ezquake dir, use it, if not found in home dir
				fclose(f);

				Cbuf_AddText("exec configs/config.cfg\n");
			}
			else // well, search it on whole file system if not found in above cases
				Cbuf_AddText("exec config.cfg\n");
		}

		Cbuf_Execute();
	}

	Cbuf_AddEarlyCommands ();
	Cbuf_Execute ();

	Con_Init ();
	NET_Init ();
	NET_InitClient ();
	Netchan_Init ();
	QLib_Init();
	Sys_Init ();
	CM_Init ();
	PM_Init ();
	Mod_Init ();

	SV_Init ();
	CL_Init ();

	Cvar_CleanUpTempVars ();

	SYSINFO_Init();

#ifdef WITH_TCL
	if (!TCL_InterpLoaded())
		Com_Printf_State (PRINT_FAIL, "Could not load "TCL_LIB_NAME", embedded Tcl disabled\n");
#endif

	Hunk_AllocName (0, "-HOST_HUNKLEVEL-");
	host_hunklevel = Hunk_LowMark ();

	host_initialized = true;

	// walk through all vars and forse OnChange event if cvar was modified
	for ( v = NULL; (v = Cvar_Next ( v )); ) {
		char val[2048];

		if ( !v->modified || !v->OnChange )
			continue; // not modified even that strange, or just have't OnChange

		snprintf(val, sizeof(val), "%s", v->string);
		Cvar_Set(v, val);
	}

	Com_Printf_State (PRINT_INFO, "Exe: "__TIME__" "__DATE__"\n");
	Com_Printf_State (PRINT_INFO, "Hunk allocation: %4.1f MB.\n", (float) host_memsize / (1024 * 1024));

	Com_Printf ("\nezQuake %s\n\n", VersionString());
	if (dedicated) {
		Com_Printf ("====== ezQuake.SourceForge.net ======\n\n");
		Com_Printf ("======== ezQuake Initialized ========\n\n");
		Com_Printf("\n");
	} else {
		Com_Printf(Host_PrintBars("ezQuake\x9c" "SourceForge\x9c" "net", 38));
		Com_Printf(Host_PrintBars("ezQuake Initialized", 38));
		Com_Printf("\n");
	}

	Com_Printf ("\nType /help to access the manual.\nUse /describe to learn about commands.\n", VersionString());

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
/*		Cbuf_AddText ("exec default.cfg\n");
		if (FS_FOpenFile("config.cfg", &f) != -1) {
			Cbuf_AddText ("exec config.cfg\n");
			fclose(f);
		} */
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
void Host_Shutdown (void)
{
	static qbool isdown = false;

	if (isdown) {
		printf ("recursive shutdown\n");
		return;
	}
	isdown = true;

#ifndef SERVERONLY
	// on low-end systems quit process may last long time (was about 1 minute for me on old compo),
	// at the same time may repeats repeats repeats some sounds, trying preventing this
	S_StopAllSounds (true);
	S_Update (vec3_origin, vec3_origin, vec3_origin, vec3_origin);
#endif

	SV_Shutdown ("Server quit\n");
	QLib_Shutdown();
	CL_Shutdown ();
	NET_Shutdown ();
#ifndef SERVERONLY
	Con_Shutdown();
#endif
#ifdef WITH_TCL
	TCL_Shutdown ();
#endif
}

void Host_Quit (void)
{
	TP_ExecTrigger ("f_exit");
	Cbuf_Execute();
	Host_Shutdown ();
	Sys_Quit ();
}
