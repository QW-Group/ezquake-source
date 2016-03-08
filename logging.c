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
#include <curl/curl.h>


static void OnChange_log_dir(cvar_t *var, char *string, qbool *cancel);

cvar_t		log_dir			= {"log_dir", "", 0, OnChange_log_dir};
cvar_t		log_readable	= {"log_readable", "1"};

#define			LOG_FILENAME_MAXSIZE	(MAX_PATH)
#define			LOGFILEBUFFER			(128*1024)

static FILE       *logfile;     // opened file with log, writing is not performed until logging is stopped to avoid fps spikes
static vfsfile_t  *memlogfile;  // allocated memory where the log is being written
static char       logfilename[LOG_FILENAME_MAXSIZE];

static qbool autologging = false;

extern cvar_t match_challenge;
const char *MT_Challenge_GetToken(void);
const char *MT_Challenge_GetLadderId(void);
const char *MT_Challenge_GetHash(void);
qbool MT_Challenge_IsOn(void);

typedef struct log_upload_job_s {
	qbool challenge_mode;
	char *challenge_hash;
	char *player_name;
	char *token;
	char *hostname;
	char *filename;
	char *url;
	char *mapname;
	char *ladderid;
} log_upload_job_t;

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
			fclose(templog);
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
static qbool temp_log_upload_pending = false;
static float auto_starttime;

char *MT_TempDirectory(void);

extern cvar_t match_auto_logconsole, match_auto_minlength,
	match_auto_logupload, match_auto_logurl, match_auto_logupload_token;

#define TEMP_LOG_NAME "_!_temp_!_.log"

void Log_AutoLogging_Upload(const char *filename);

qbool Log_TempLogUploadPending(void) {
	return temp_log_upload_pending;
}

static qbool Log_IsUploadAllowed(void) {
	return (match_challenge.integer || (match_auto_logupload.integer && match_auto_logconsole.integer))
		&& !cls.demoplayback
		&& cls.server_adr.type != NA_LOOPBACK;
}

static void Log_UploadTemp(void) {
	char *tempname = va("%s/%s", MT_TempDirectory(), TEMP_LOG_NAME);
	temp_log_upload_pending = true;
	Log_AutoLogging_Upload(tempname);
}

void Log_AutoLogging_StopMatch(void) {
	if (!autologging)
		return;

	autologging = false;
	Log_Stop();
	temp_log_ready = true;

	if (match_auto_logconsole.value == 2)
		Log_AutoLogging_SaveMatch(true);
	else {
		Com_Printf ("Auto console logging completed\n");
		if (Log_IsUploadAllowed()) {
			Log_UploadTemp();
		}
	}
}


void Log_AutoLogging_CancelMatch(void) {
	if (!autologging)
		return;

	autologging = false;
	Log_Stop();
	temp_log_ready = true;

	if (match_auto_logconsole.value == 2) {
		if (cls.realtime - auto_starttime > match_auto_minlength.value)
			Log_AutoLogging_SaveMatch(true);
		else
			Com_Printf("Auto console logging cancelled\n");
	} else if (match_auto_logconsole.integer == 1 || match_challenge.integer) {
		Com_Printf ("Auto console logging completed\n");
		if (Log_IsUploadAllowed()) {
			Log_UploadTemp();
		}
	} else {
		Com_Printf ("Auto console logging completed\n");
	}
}


void Log_AutoLogging_StartMatch(char *logname) {
	char extendedname[MAX_OSPATH * 2], *fullname;
	FILE *templog;
	void *buf;

	temp_log_ready = false;

	if (!match_auto_logconsole.value && !match_challenge.integer)
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
		fclose(templog);
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

size_t Log_Curl_Write_Void( void *ptr, size_t size, size_t nmemb, void *userdata)
{
	return size*nmemb;
}

static log_upload_job_t* Log_Upload_Job_Prepare(qbool challenge_mode, const char *challenge_hash,
                                                const char *filename, const char *hostname,
                                                const char* player_name, const char *token, const char *url,
                                                const char *mapname, const char *ladderid)
{
	log_upload_job_t *job = (log_upload_job_t *) Q_malloc(sizeof(log_upload_job_t));

	job->challenge_mode = challenge_mode;
	job->challenge_hash = Q_strdup(challenge_hash);
	job->filename = Q_strdup(filename);
	job->hostname = Q_strdup(hostname);
	job->player_name = Q_strdup(player_name);
	job->token = Q_strdup(token);
	job->url = Q_strdup(url);
	job->mapname = Q_strdup(mapname);
	job->ladderid = Q_strdup(ladderid);

	return job;
}

static void Log_Upload_Job_Free(log_upload_job_t *job)
{
	Q_free(job->filename);
	Q_free(job->hostname);
	Q_free(job->challenge_hash);
	Q_free(job->player_name);
	Q_free(job->token);
	Q_free(job->url);
	Q_free(job->mapname);
	Q_free(job);
}

DWORD WINAPI Log_AutoLogging_Upload_Thread(void *vjob)
{
	log_upload_job_t *job = (log_upload_job_t *) vjob;
	CURL *curl;
	CURLcode res;
	struct curl_httppost *post=NULL;
	struct curl_httppost *last=NULL;
	struct curl_slist *headers=NULL;
	char errorbuffer[CURL_ERROR_SIZE] = "";

	curl = curl_easy_init();

	headers = curl_slist_append(headers, "Content-Type: text/plain");

	curl_formadd(&post, &last,
		CURLFORM_COPYNAME, "name",
		CURLFORM_COPYCONTENTS, job->player_name,
		CURLFORM_END);
	curl_formadd(&post, &last,
		CURLFORM_COPYNAME, "challenge",
		CURLFORM_COPYCONTENTS, job->challenge_mode ? "1" : "0",
		CURLFORM_END);
	curl_formadd(&post, &last,
		CURLFORM_COPYNAME, "challenge_hash",
		CURLFORM_COPYCONTENTS, job->challenge_hash,
		CURLFORM_END);
	curl_formadd(&post, &last,
		CURLFORM_COPYNAME, "token",
		CURLFORM_COPYCONTENTS, job->token,
		CURLFORM_END);
	curl_formadd(&post, &last,
		CURLFORM_COPYNAME, "host",
		CURLFORM_COPYCONTENTS, job->hostname,
		CURLFORM_END);
	curl_formadd(&post, &last,
		CURLFORM_COPYNAME, "map",
		CURLFORM_COPYCONTENTS, job->mapname,
		CURLFORM_END);
	curl_formadd(&post, &last,
		CURLFORM_COPYNAME, "log",
		CURLFORM_FILE, job->filename,
		CURLFORM_CONTENTHEADER, headers,
		CURLFORM_END);

	curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
	curl_easy_setopt(curl, CURLOPT_URL, job->url);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorbuffer);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Log_Curl_Write_Void);

	res = curl_easy_perform(curl); /* post away! */

	curl_formfree(post);
	curl_easy_cleanup(curl);

	if (res != CURLE_OK) {
		Com_Printf("Uploading of log failed:\n%s\n", curl_easy_strerror(res));
		Com_Printf("%s\n", errorbuffer);
	}
	else {
		Com_Printf("Match log uploaded\n");
	}

	Log_Upload_Job_Free(job);
	temp_log_upload_pending = false;
	return 0;
}

void Log_AutoLogging_Upload(const char *filename)
{
	log_upload_job_t *job = Log_Upload_Job_Prepare(
		MT_Challenge_IsOn(),
		MT_Challenge_GetHash(),
		filename,
		Info_ValueForKey(cl.serverinfo, "hostname"),
		cl.players[cl.playernum].name,
		MT_Challenge_GetToken(),
		match_auto_logurl.string,
		host_mapname.string,
		MT_Challenge_GetLadderId());

	Com_Printf("Uploading match log...\n");
	Sys_CreateThread(Log_AutoLogging_Upload_Thread, (void *) job);
}

void Log_AutoLogging_SaveMatch(qbool allow_upload) {
	int error, num;
	FILE *f;
	char *dir, *tempname, savedname[2 * MAX_OSPATH], *fullsavedname, *exts[] = {"log", NULL};

	if (!temp_log_ready)
		return;

	if (temp_log_upload_pending) {
		Com_Printf("Error: Can't save the log. Log upload is still pending.\n");
		return;
	}

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

	if (!error) {
		Com_Printf("Match console log saved to %s\n", savedname);
		if (allow_upload && Log_IsUploadAllowed()) {
			// note: we allow the client to be a spectator, so that spectators
			// can submit logs for matches they spec in case players don't do it
			Log_AutoLogging_Upload(fullsavedname);
		}
	}
}
