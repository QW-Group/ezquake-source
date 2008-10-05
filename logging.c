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

	$Id: logging.c,v 1.13 2007-10-11 05:55:47 dkure Exp $
*/

#include "quakedef.h"
#include "logging.h"
#include "utils.h"
#include "hash.h"
#include "vfs.h"


static void OnChange_log_dir(cvar_t *var, char *string, qbool *cancel);

cvar_t		log_dir			= {"log_dir", "", 0, OnChange_log_dir};
cvar_t		log_readable	= {"log_readable", "1"};

#define			LOG_FILENAME_MAXSIZE	(MAX_PATH)
#define			LOGFILEBUFFER			(128*1024)

static FILE       *logfile;     // opened file with log, writing is not performed until logging is stopped to avoid fps spikes
static vfsfile_t  *memlogfile;  // allocated memory where the log is being written
static char       logfilename[LOG_FILENAME_MAXSIZE];

static qbool autologging = false;

qbool Log_IsLogging(void) {
	return logfile ? true : false;
}

static char *Log_LogDirectory(void) {
	static char dir[LOG_FILENAME_MAXSIZE];

	strlcpy(dir, FS_LegacyDir(log_dir.string), sizeof(dir));
	return dir;
}

static void Log_Stop(void) {
	int read;
	vfserrno_t err;
	char buf[1024];
	int size;

	if (!Log_IsLogging())
		return;

	size = (int) VFS_TELL(memlogfile);

	VFS_SEEK(memlogfile, 0, SEEK_SET);
	while (size > 0 && (read = VFS_READ(memlogfile, buf, 1024, &err))) {
		fwrite(buf, 1, min(read, size), logfile);
		size -= read;
	}

	VFS_CLOSE(memlogfile);
	fclose(logfile);

	logfile = NULL;
	memlogfile = NULL;
}

static void OnChange_log_dir(cvar_t *var, char *string, qbool *cancel) {
	if (!string[0])
		return;

	Util_Process_FilenameEx(string, cl_mediaroot.integer == 2);

	if (!Util_Is_Valid_FilenameEx(string, cl_mediaroot.integer == 2)) {
		Com_Printf(Util_Invalid_Filename_Msg(var->name));
		*cancel = true;
		return;
	}
}

static void Log_log_f(void) {
	char *fulllogname;
	FILE *templog;
	void *buf;

	switch (Cmd_Argc()) {
	case 1:
		if (autologging)
			Com_Printf("Auto console logging is in progress\n");
		else if (Log_IsLogging())
			Com_Printf("Logging to %s\n", logfilename);
		else
			Com_Printf("Not logging\n");
		return;
	case 2:
		if (!strcasecmp(Cmd_Argv(1), "stop")) {
			if (autologging) {
				Log_AutoLogging_StopMatch();
			} else {
				if (Log_IsLogging()) {
					Log_Stop();
					Com_Printf("Stopped logging to %s\n", logfilename);
				} else {
					Com_Printf("Not logging\n");
				}
			}
			return;
		}

		if (autologging) {
			Com_Printf("Auto console logging must be stopped first!\n");
			return;
		}

		if (Log_IsLogging()) {
			Log_Stop();
			Com_Printf("Stopped logging to %s\n", logfilename);
		}

		strlcpy(logfilename, Cmd_Argv(1), sizeof(logfilename) - 4);
		Util_Process_Filename(logfilename);
		if (!Util_Is_Valid_Filename(logfilename)) {
			Com_Printf(Util_Invalid_Filename_Msg("filename"));
			return;
		}
		COM_ForceExtensionEx (logfilename, ".log", sizeof (logfilename));
		fulllogname = va("%s/%s", Log_LogDirectory(), logfilename);
		if (!(templog = fopen (fulllogname, log_readable.value ? "w" : "wb"))) {
			FS_CreatePath(fulllogname);
			if (!(templog = fopen (fulllogname, log_readable.value ? "w" : "wb"))) {
				Com_Printf("Error: Couldn't open %s\n", logfilename);
				return;
			}
		}
		buf = Q_calloc(1, LOGFILEBUFFER);
		if (!buf) {
			Com_Printf("Not enough memory to allocate log buffer\n");
			return;
		}
		memlogfile = FSMMAP_OpenVFS(buf, LOGFILEBUFFER);
		Com_Printf("Logging to %s\n", logfilename);
		logfile = templog;
		break;
	default:
		Com_Printf("Usage: %s [filename | stop]\n", Cmd_Argv(0));
		return;
	}
}

void Log_Init(void) {
	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register (&log_dir);
	Cvar_Register (&log_readable);

	Cvar_ResetCurrentGroup();

	Cmd_AddCommand("log", Log_log_f);
}

void Log_Shutdown(void) {
	if (Log_IsLogging())	
		Log_Stop();
}

void Log_Write(char *s) {
	if (!Log_IsLogging())
		return;
	
	VFS_WRITE(memlogfile, s, strlen(s));
	// fprintf(logfile, "%s", s);
}

//=============================================================================
//							AUTO CONSOLE LOGGING
//=============================================================================



static char	auto_matchname[2 * MAX_OSPATH];
static qbool temp_log_ready = false;
static float auto_starttime;

char *MT_TempDirectory(void);

extern cvar_t match_auto_logconsole, match_auto_minlength;

#define TEMP_LOG_NAME "_!_temp_!_.log"


void Log_AutoLogging_StopMatch(void) {
	if (!autologging)
		return;

	autologging = false;
	Log_Stop();
	temp_log_ready = true;

	if (match_auto_logconsole.value == 2)
		Log_AutoLogging_SaveMatch();
	else
		Com_Printf ("Auto console logging completed\n");
}


void Log_AutoLogging_CancelMatch(void) {
	if (!autologging)
		return;

	autologging = false;
	Log_Stop();
	temp_log_ready = true;

	if (match_auto_logconsole.value == 2) {
		if (cls.realtime - auto_starttime > match_auto_minlength.value)
			Log_AutoLogging_SaveMatch();
		else
			Com_Printf("Auto console logging cancelled\n");
	} else {
		Com_Printf ("Auto console logging completed\n");
	}
}


void Log_AutoLogging_StartMatch(char *logname) {
	char extendedname[MAX_OSPATH * 2], *fullname;
	FILE *templog;
	void *buf;

	temp_log_ready = false;

	if (!match_auto_logconsole.value)
		return;

	if (Log_IsLogging()) {
		if (autologging) {		
			
			autologging = false;
			Log_Stop();
		} else {
			Com_Printf("Auto console logging skipped (already logging)\n");
			return;
		}
	}


	strlcpy(auto_matchname, logname, sizeof(auto_matchname));

	strlcpy (extendedname, TEMP_LOG_NAME, sizeof(extendedname));
	COM_ForceExtensionEx (extendedname, ".log", sizeof (extendedname));
	fullname = va("%s/%s", MT_TempDirectory(), extendedname);


	if (!(templog = fopen (fullname, log_readable.value ? "w" : "wb"))) {
		FS_CreatePath(fullname);
		if (!(templog = fopen (fullname, log_readable.value ? "w" : "wb"))) {
			Com_Printf("Error: Couldn't open %s\n", fullname);
			return;
		}
	}

	buf = Q_calloc(1, LOGFILEBUFFER);
	if (!buf) {
		Com_Printf("Not enough memory to allocate log buffer\n");
		return;
	}
	memlogfile = FSMMAP_OpenVFS(buf, LOGFILEBUFFER);

	Com_Printf ("Auto console logging commenced\n");

	logfile = templog;
	autologging = true;
	auto_starttime = cls.realtime;
}

qbool Log_AutoLogging_Status(void) {
	return temp_log_ready ? 2 : autologging ? 1 : 0;
}

void Log_AutoLogging_SaveMatch(void) {
	int error, num;
	FILE *f;
	char *dir, *tempname, savedname[2 * MAX_OSPATH], *fullsavedname, *exts[] = {"log", NULL};

	if (!temp_log_ready)
		return;

	temp_log_ready = false;

	dir = Log_LogDirectory();
	tempname = va("%s/%s", MT_TempDirectory(), TEMP_LOG_NAME);

	fullsavedname = va("%s/%s", dir, auto_matchname);
	if ((num = Util_Extend_Filename(fullsavedname, exts)) == -1) {
		Com_Printf("Error: no available filenames\n");
		return;
	}
	snprintf (savedname, sizeof(savedname), "%s_%03i.log", auto_matchname, num);

	fullsavedname = va("%s/%s", dir, savedname);

	
	if (!(f = fopen(tempname, "rb")))
		return;
	fclose(f);

	if ((error = rename(tempname, fullsavedname))) {
		FS_CreatePath(fullsavedname);
		error = rename(tempname, fullsavedname);
	}

	if (!error)
		Com_Printf("Match console log saved to %s\n", savedname);
}
