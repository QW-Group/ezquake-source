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
 
	$Id: host.c,v 1.60 2007-10-17 17:07:08 dkure Exp $
*/
// this should be the only file that includes both server.h and client.h

#include <setjmp.h>

#ifdef __FreeBSD__
#include <sys/types.h>
#include <sys/sysctl.h>
#include <osreldate.h>
#include <sys/time.h>
#include <machine/cpufunc.h>
#endif
#ifdef __APPLE__
#include <sys/types.h>
#include <sys/sysctl.h>
#endif
#include <SDL.h>
#include "quakedef.h"
#include "EX_browser.h"
#include "fs.h"
#ifdef WITH_TCL
#include "embed_tcl.h"
#endif
#include "modules.h"
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"
#include "teamplay.h"
#include "pmove.h"
#include "version.h"
#include "qsound.h"
#include "keys.h"
#include "config_manager.h"
#include "EX_qtvlist.h"

double		curtime;

static int	host_hunklevel;
static void	*host_membase;

qbool	host_initialized;	// true if into command execution
qbool	host_everything_loaded;	// true if OnChange() applied to every var, end of Host_Init()
int		host_memsize;

static jmp_buf 	host_abort;

extern void COM_StoreOriginalCmdline(int argc, char **argv);

char f_system_string[1024] = "";

char * SYSINFO_GetString(void)
{
	return f_system_string;
}

unsigned long long	SYSINFO_memory = 0;
int					SYSINFO_MHz = 0;
char				*SYSINFO_processor_description = NULL;
char				*SYSINFO_3D_description        = NULL;

#ifdef _WIN32
typedef BOOL (WINAPI *PGMSE)(LPMEMORYSTATUSEX);

void SYSINFO_Init(void)
{
	LONG  ret;
	HKEY  hKey;
	PGMSE pGMSE;

	extern const char *gl_renderer;

	// Get memory size.
	if ((pGMSE = (PGMSE)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "GlobalMemoryStatusEx")) != NULL) {
		MEMORYSTATUSEX	memstat;
		memstat.dwLength = sizeof(memstat);
		pGMSE(&memstat);
		SYSINFO_memory = memstat.ullTotalPhys;
	} else {
		// Win9x doesn't have GlobalMemoryStatusEx.
		MEMORYSTATUS memstat;
		GlobalMemoryStatus(&memstat);
		SYSINFO_memory = memstat.dwTotalPhys;
	}

	// Get processor info.
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

	if (gl_renderer && gl_renderer[0]) {
		if (SYSINFO_3D_description != NULL) {
			free(SYSINFO_3D_description);
		}
		SYSINFO_3D_description = Q_strdup(gl_renderer);
	}

	//
	// Create the f_system string.
	//
	
	snprintf(f_system_string, sizeof(f_system_string), "%uMiB", (unsigned)((SYSINFO_memory / (double) 1048576u)+0.5));

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
	char buffer[1024] = {0};
	char cpu_model[255] = {0};
	char *match;
	FILE *f;

	extern const char *gl_renderer;

	// MEM
	f = fopen("/proc/meminfo", "r");
	if (f) {
		if (fscanf (f, "%*s %llu %*s\n", &SYSINFO_memory) != 1) {
			Com_Printf ("could not read /proc/meminfo!\n");
		} else {
			SYSINFO_memory /= 1024;
		}
		fclose (f);
	} else {
		Com_Printf ("could not open /proc/meminfo!\n");
		SYSINFO_memory = 0;
	}

	//CPU-MHZ
	f = fopen("/proc/cpuinfo", "r");
	if (f) {
		buffer[fread (buffer, 1, sizeof(buffer) - 1, f)] = '\0';
		fclose (f);
		match = strstr (buffer, "cpu MHz");
		sscanf (match, "cpu MHz : %i", &SYSINFO_MHz);
	} else {
		Com_Printf ("could not open /proc/cpuinfo!\n");
	}

	//CPU-MODEL
	f = fopen("/proc/cpuinfo", "r");
	if (f) {
		while (!feof(f)) {
			if (fgets (buffer, sizeof(buffer), f) == NULL && !feof(f)) {	// disconnect: sizeof(buffer) - 1 ?
				Com_Printf("Error reading /proc/cpuinfo\n");
				break;
			}
			if (!strncmp( buffer, "model name", 10)) {
				match = strchr( buffer, ':' );
				match++;
				while (isspace(*match)) match++;
				memcpy(cpu_model, match, sizeof(cpu_model));
				cpu_model[strlen(cpu_model) - 1] = '\0';
				SYSINFO_processor_description= Q_strdup (cpu_model);
				break;
			}
		}
		fclose(f);	
	} else {
		Com_Printf("could not open /proc/cpuinfo!\n");
	}


	if (gl_renderer && gl_renderer[0]) {
		if (SYSINFO_3D_description != NULL) {
			free(SYSINFO_3D_description);
		}
		SYSINFO_3D_description = Q_strdup(gl_renderer);
	}

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
	int mib[2];
	mib[0] = CTL_HW;
	mib[1] = HW_MEMSIZE;
	int64_t memsize_value;
	int cpu_frequency_value;
	size_t length = sizeof(memsize_value);
	char cpu_brand_string[100] = {0};
	size_t cpu_brand_string_len = sizeof(cpu_brand_string) - 1; /* Don't trust Apple, make sure its NULL terminated */

	extern const char *gl_renderer;

	if (sysctl(mib, 2, &memsize_value, &length, NULL, 0) != -1) {
		SYSINFO_memory = memsize_value;
	}

	if (sysctlbyname("machdep.cpu.brand_string", &cpu_brand_string, &cpu_brand_string_len, NULL, 0) != -1) {
		SYSINFO_processor_description = cpu_brand_string;
	}

	mib[0] = CTL_HW;
	mib[1] = HW_CPU_FREQ;
	length = sizeof(cpu_frequency_value);
	if (sysctl(mib, 2, &cpu_frequency_value, &length, NULL, 0) != -1) {
		SYSINFO_MHz = cpu_frequency_value / 1000. / 1000. + .5;
	}


	if (gl_renderer && gl_renderer[0]) {
		if (SYSINFO_3D_description != NULL) {
			free(SYSINFO_3D_description);
		}
		SYSINFO_3D_description = Q_strdup(gl_renderer);
	}

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
	unsigned long val_ul;
	size_t len;
	unsigned long long old_tsc, tsc_freq;
	struct timeval tp, old_tp;
	extern const char *gl_renderer;

	mib[0] = CTL_HW;
	mib[1] =
#if __FreeBSD_version >= 500000
		HW_REALMEM;
#else
		HW_PHYSMEM;
#endif
// VVD: We can use HW_REALMEM (hw.realmem) for RELENG_5/6/7 for getting exact result,
// but RELENG_4 have only HW_PHYSMEM (hw.physmem).
	len = sizeof(val);
	sysctl(mib, sizeof(mib) / sizeof(mib[0]), &val_ul, &len, NULL, 0);

	SYSINFO_memory = val_ul;

	mib[0] = CTL_HW;
	mib[1] = HW_MODEL;
	len = sizeof(cpu_model);
	sysctl(mib, sizeof(mib) / sizeof(mib[0]), cpu_model, &len, NULL, 0);
	cpu_model[sizeof(cpu_model) - 1] = '\0';

	SYSINFO_processor_description = cpu_model;

	gettimeofday(&old_tp, NULL);
	old_tsc = rdtsc();
	do {
		gettimeofday(&tp, NULL);
	} while ((tp.tv_sec - old_tp.tv_sec) * 1000000. + tp.tv_usec - old_tp.tv_usec < 1000000.);
	tsc_freq = rdtsc();
	SYSINFO_MHz = (int)((tsc_freq - old_tsc) /
						(tp.tv_sec - old_tp.tv_sec + (tp.tv_usec - old_tp.tv_usec) / 1000000.) /
						1000000. + .5);
// VVD: We can use sysctl hw.clockrate, but it don't work on i486 - always 0.
// Must work on Pentium 1/2/3; tested on Pentium 4. And RELENG_4 have no this sysctl.

	if (gl_renderer  &&  gl_renderer[0]) {
		if (SYSINFO_3D_description != NULL) {
			free(SYSINFO_3D_description);
		}
		SYSINFO_3D_description = Q_strdup(gl_renderer);
	}

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

	if ((t = COM_CheckParm ("-heapsize")) != 0 && t + 1 < COM_Argc())
		memsize = Q_atoi (COM_Argv(t + 1)) * 1024;

	if ((t = COM_CheckParm ("-mem")) != 0 && t + 1 < COM_Argc())
		memsize = Q_atoi (COM_Argv(t + 1)) * 1024 * 1024;

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
	D_FlushCaches ();

	// FIXME, move to CL_ClearState
	Mod_ClearAll ();

	CM_InvalidateMap ();

	// any data previously allocated on hunk is no longer valid
	Hunk_FreeToLowMark (host_hunklevel);
}

void Host_Frame (double time)
{
	if (setjmp (host_abort))
		return;			// something bad happened, or the server disconnected

	curtime += time;

	CL_Frame (time);	// will also call SV_Frame
}

char *Host_PrintBars(char *s, int len)
{
	static char temp[512];
	unsigned int i, count;

	temp[0] = 0;

	count = (len - 2 - 2 - strlen (s)) / 2;
	if (count > sizeof (temp) / 2 - 8)
		return temp;

	strlcat (temp, "\x1d", sizeof (temp));

	for (i = 0; i < count; i++)
		strlcat(temp, "\x1e", sizeof (temp));

	strlcat (temp, " ", sizeof (temp));
	strlcat (temp, s, sizeof (temp));
	strlcat (temp, " ", sizeof (temp));

	for (i = 0; i < count; i++)
		strlcat(temp, "\x1e", sizeof (temp));

	strlcat(temp, "\x1f", sizeof (temp));
	strlcat(temp, "\n\n", sizeof (temp));

	return temp;
}

static void Commands_For_Configs_Init (void)
{
extern void SV_Floodprot_f (void);
extern void SV_Floodprotmsg_f (void);
extern void TP_MsgTrigger_f (void);
extern void TP_MsgFilter_f (void);
extern void TP_Took_f (void);
extern void TP_Pickup_f (void);
extern void TP_Point_f (void);
extern void MT_AddMapGroups (void);
extern void MT_MapGroup_f (void);
extern void MT_AddSkyGroups (void);
extern void MT_SkyGroup_f (void);
extern void CL_Fog_f (void);
extern void SB_SourceUnmarkAll(void);
extern void SB_SourceMark(void);
extern void LoadConfig_f(void);


	//disconnect: fix it if i forgot something
	Cmd_AddCommand ("floodprot", SV_Floodprot_f);
	Cmd_AddCommand ("floodprotmsg", SV_Floodprotmsg_f);
	Cmd_AddCommand ("msg_trigger", TP_MsgTrigger_f);
	Cmd_AddCommand ("filter", TP_MsgFilter_f);
	Cmd_AddCommand ("tp_took", TP_Took_f);
	Cmd_AddCommand ("tp_pickup", TP_Pickup_f);
	Cmd_AddCommand ("tp_point", TP_Point_f);

	MT_AddMapGroups ();
	Cmd_AddCommand ("mapgroup", MT_MapGroup_f);

	MT_AddSkyGroups ();
	Cmd_AddCommand ("skygroup", MT_SkyGroup_f);
	Cmd_AddCommand ("fog", CL_Fog_f);
	Cmd_AddCommand ("allskins", Skin_AllSkins_f);

	Cmd_AddCommand ("sb_sourceunmarkall", SB_SourceUnmarkAll);
	Cmd_AddCommand ("sb_sourcemark", SB_SourceMark);
}

void Startup_Place(void)
{
    extern cvar_t cl_onload;
	extern cvar_t cl_startupdemo;

	if (cl_startupdemo.string[0]) {
		Cbuf_AddText(va("playdemo %s\n", cl_startupdemo.string));
		key_dest = key_startupdemo;
	}
	else if (!strcmp (cl_onload.string, "menu")) {
		Cbuf_AddText ("togglemenu\n");
	}
	else if (!strcmp (cl_onload.string, "browser")) {
		Cbuf_AddText ("menu_slist\n");
	}
	else if (!strcmp (cl_onload.string, "console")) {
		key_dest = key_console;
	}
	else {
		key_dest = key_console;
		Cbuf_AddText (va ("%s\n", cl_onload.string));
	}
}

void Host_Init (int argc, char **argv, int default_memsize)
{
	vfsfile_t *vf;
	cvar_t *v;
	char cfg[MAX_PATH] = {0};
	int i;
	char *cfg_name;

	COM_InitArgv (argc, argv);
	COM_StoreOriginalCmdline(argc, argv);

	if (SDL_Init(0) != 0)
	{
		fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}
	atexit(SDL_Quit);

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

	Cache_Init_Commands ();

	FS_InitFilesystem ();
	NET_Init ();

	Commands_For_Configs_Init ();
	Browser_Init2();
	ConfigManager_Init();
	ResetBinds();

	i = COM_CheckParm("+cfg_load");

	if (i && (i + 1 < COM_Argc())) {
		cfg_name = COM_Argv(i + 1);
	}
	else {
		cfg_name = MAIN_CONFIG_FILENAME;
	}
	snprintf(cfg, sizeof(cfg), "%s", cfg_name);
	COM_ForceExtensionEx (cfg, ".cfg", sizeof (cfg));
	Cbuf_AddText(va("cfg_load %s\n", cfg));
	Cbuf_Execute();

	Cbuf_AddEarlyCommands ();
	Cbuf_Execute ();

	Con_Init ();
	NET_InitClient ();
	Netchan_Init ();

#if (!defined WITH_PNG_STATIC || !defined WITH_JPEG_STATIC || defined WITH_MP3_PLAYER)
	QLib_Init();
#endif

	Sys_Init ();
	Sys_CvarInit();
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

	// walk through all vars and forse OnChange event if cvar was modified,
	// also apply that to variables which mirrored in userinfo because of cl_parsefunchars was't applyed as this moment,
	// same for serverinfo and may be this fix something also.
	for ( v = NULL; (v = Cvar_Next ( v )); ) {
		char val[2048];

//		if ( !v->modified )
//			continue; // not modified even that strange at this moment

		if ( Cvar_GetFlags( v ) & (CVAR_ROM | CVAR_INIT) )
			continue;

		snprintf(val, sizeof(val), "%s", v->string);
		Cvar_Set(v, val);
	}
	
	Hud_262LoadOnFirstStart();

	Com_Printf_State (PRINT_INFO, "Exe: "__DATE__" "__TIME__"\n");
	Com_Printf_State (PRINT_INFO, "Hunk allocation: %4.1f MB\n", (float) host_memsize / (1024 * 1024));
	Com_Printf("\n");
	Com_Printf("http://ezquake.github.io/\n");
	Com_Printf("\n");
//	Com_Printf(Host_PrintBars("ezQuake\x9c" "SourceForge\x9c" "net", 38));
	Com_Printf("ezQuake %s\n", VersionStringColour());
	Com_Printf("\n");
	Com_Printf(Host_PrintBars("&c1e1ezQuake Initialized&r", 38));
	Com_Printf("\n");
	Com_Printf("Type /help to access the manual.\nUse /describe for help on commands.\n\n", VersionString());

	if ((vf = FS_OpenVFS("autoexec.cfg", "rb", FS_ANY))) {
		Cbuf_AddText ("exec autoexec.cfg\n\n");
		VFS_CLOSE(vf);
	}

	Cmd_StuffCmds_f ();		// process command line arguments
	Cbuf_AddText ("cl_warncmd 1\n");

	#ifdef WIN32
	//
	// Verify that ezQuake is associated with the QW:// protocl handler.
	//
	{
		#define INITIAL_CON_WIDTH 35
		extern qbool CL_CheckIfQWProtocolHandler();
		extern cvar_t cl_verify_qwprotocol;

		if (cl_verify_qwprotocol.integer >= 2)
		{
			// Always register the qw:// protocol.
			Cbuf_AddText("register_qwurl_protocol\n");
		}
		else if (cl_verify_qwprotocol.integer == 1 && !CL_CheckIfQWProtocolHandler())
		{
			// Check if the running exe is the one associated with the qw:// protocol.
			Com_PrintVerticalBar(INITIAL_CON_WIDTH);
			Com_Printf("\n");
			Com_Printf("ezQuake is not associated with the ");
			Com_Printf("\x02QW:// protocol. ");
			Com_Printf("Register it using"); 
			Com_Printf("\x02/register_qwurl_protocol\n");
			Com_Printf("(set");
			Com_Printf("\x02 cl_verify_qwprotocol 0 ");
			Com_Printf("to hide this warning)\n");
			Com_PrintVerticalBar(INITIAL_CON_WIDTH);
			Com_Printf("\n\n");
		}
	}
	#endif // WIN32

	// Check if a qtv/demo file is specified as the first argument, in that case play that
	// otherwise, do some more checks of what to show at startup.
	{
		char cmd[1024] = {0};

		if (COM_CheckArgsForPlayableFiles(cmd, sizeof(cmd)))
		{
			Cbuf_AddText(cmd);
		}
		else
		{
			Startup_Place();
		}
	}

	host_everything_loaded = true;
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

	// on low-end systems quit process may last long time (was about 1 minute for me on old compo),
	// at the same time may repeats repeats repeats some sounds, trying preventing this
	S_StopAllSounds();
	S_Update (vec3_origin, vec3_origin, vec3_origin, vec3_origin);

	SV_Shutdown ("Server quit\n");

#if (!defined WITH_PNG_STATIC && !defined WITH_JPEG_STATIC)
	QLib_Shutdown();
#endif

	CL_Shutdown ();
	NET_Shutdown ();
	Con_Shutdown();
#ifdef WITH_TCL
	TCL_Shutdown ();
#endif
	qtvlist_deinit();
}

void Host_Quit (void)
{
	// execute user's trigger
	TP_ExecTrigger ("f_exit");
	Cbuf_Execute();
	
	// save config (conditional)
	Config_QuitSave();

	// turn off
	Host_Shutdown ();
	Sys_Quit ();
}
