/*
Copyright (C) 2002-2003       A Nourai

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
*/

#include "quakedef.h"
#include "teamplay.h"
#include "utils.h"
#include <curl/curl.h>
#include "sha1.h"
#include <time.h>
#include "logging.h"

char* MT_PlayerName(void);
char* MT_PlayerTeam(void);
char* MT_EnemyName(void);
int MT_CountPlayers(void);
char* MT_TempDirectory(void);

static qbool temp_log_upload_pending = false;

void MT_ChallengeMode_Change(cvar_t *cvar, char *value, qbool *cancel)
{
	if (cls.state != ca_disconnected && !cl.standby && !cl.spectator) {
		*cancel = true;
		Com_Printf("Challenge mode cannot be toggled during the match\n");
		return;
	}
}

static cvar_t match_auto_logupload       = { "match_auto_logupload", "0" };
static cvar_t match_auto_logupload_token = { "match_auto_logupload_token", "", 0, MT_ChallengeMode_Change };
static cvar_t match_auto_logurl          = { "match_auto_logurl", "http://stats.quakeworld.nu/logupload" };
static cvar_t match_challenge            = { "match_challenge", "0", 0, MT_ChallengeMode_Change };
static cvar_t match_challenge_url        = { "match_challenge_url", "http://stats.quakeworld.nu/post-challenge", 0, MT_ChallengeMode_Change };
static cvar_t match_ladder_id            = { "match_ladder_id", "1" };

typedef enum challenge_status_e {
	challenge_start,
	challenge_end
} challenge_status_e;

typedef struct challenge_data_s {
	challenge_status_e status;
	char *ladderid;
	int players_count;
	char *token;
	char *player1;
	char *player2;
	char *server;
	char *map;
	char *url;
	char *hash;
} challenge_data_t;

static challenge_data_t *last_challenge = NULL;

qbool MT_Challenge_IsOn(void)
{
	return last_challenge != NULL;
}

const char *MT_Challenge_GetLadderId(void)
{
	if (last_challenge == NULL) {
		return "";
	}

	return last_challenge->ladderid;
}

const char *MT_Challenge_GetHash(void)
{
	if (last_challenge == NULL) {
		return "";
	}

	return last_challenge->hash;
}

const char *MT_Challenge_GetToken(void)
{
	if (last_challenge == NULL) {
		return "";
	}

	return last_challenge->token;
}

const char* MT_Challenge_StatusName(challenge_status_e status)
{
	switch (status) {
		case challenge_start: return "start";
		case challenge_end: return "end";
		default:
			Com_Printf("ERROR: MT_Challenge_StatusName: Unknown challenge status");
			return "";
	}
}

size_t MT_Curl_Write_Void(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	return size * nmemb;
}

static const char *MT_Challenge_GenerateHash(void)
{
	static unsigned char hash[DIGEST_SIZE];
	SHA1_CTX context;
	char* hostname = Info_ValueForKey(cl.serverinfo, "hostname");
	double curtime = Sys_DoubleTime();

	SHA1Init(&context);

	SHA1Update(&context, (unsigned char *)match_ladder_id.string, strlen(match_ladder_id.string));
	SHA1Update(&context, (unsigned char *)match_auto_logupload_token.string, strlen(match_auto_logupload_token.string));
	SHA1Update(&context, (unsigned char *)MT_PlayerName(), strlen(MT_PlayerName()));
	SHA1Update(&context, (unsigned char *)MT_EnemyName(), strlen(MT_EnemyName()));
	SHA1Update(&context, (unsigned char *)hostname, strlen(hostname));
	SHA1Update(&context, (unsigned char *)host_mapname.string, strlen(host_mapname.string));
	SHA1Update(&context, (unsigned char *)match_challenge_url.string, strlen(match_challenge_url.string));
	SHA1Update(&context, (unsigned char *)&curtime, sizeof(double));

	SHA1Final(hash, &context);

	return bin2hex(hash);
}

challenge_data_t *MT_Challenge_Create(challenge_status_e status, int players_count,
	const char *ladderid, const char *token, const char *player1, const char *player2,
	const char *server, const char *map, const char *url, const char *hash)
{
	challenge_data_t *retval = (challenge_data_t *)Q_malloc(sizeof(challenge_data_t));
	retval->status = status;
	retval->players_count = players_count;
	retval->ladderid = Q_strdup(ladderid);
	retval->token = Q_strdup(token);
	retval->player1 = Q_strdup(player1);
	retval->player2 = Q_strdup(player2);
	retval->server = Q_strdup(server);
	retval->map = Q_strdup(map);
	retval->url = Q_strdup(url);
	retval->hash = Q_strdup(hash);

	return retval;
}

void MT_Challenge_Destroy(challenge_data_t *challenge)
{
	Q_free(challenge->ladderid);
	Q_free(challenge->token);
	Q_free(challenge->player1);
	Q_free(challenge->player2);
	Q_free(challenge->server);
	Q_free(challenge->map);
	Q_free(challenge->url);
	Q_free(challenge->hash);
	Q_free(challenge);
}

challenge_data_t *MT_Challenge_Init(challenge_status_e status)
{
	return MT_Challenge_Create(status, MT_CountPlayers(), match_ladder_id.string, match_auto_logupload_token.string,
		MT_PlayerName(), MT_EnemyName(), Info_ValueForKey(cl.serverinfo, "hostname"), host_mapname.string,
		match_challenge_url.string, MT_Challenge_GenerateHash());
}

challenge_data_t *MT_Challenge_Copy(const challenge_data_t *orig)
{
	return MT_Challenge_Create(orig->status, orig->players_count, orig->ladderid, orig->token, orig->player1, orig->player2,
		orig->server, orig->map, orig->url, orig->hash);
}

int MT_Challenge_StartSend_Thread(void *arg)
{
	challenge_data_t *challenge_data = (challenge_data_t *)arg;

	CURL *curl;
	CURLcode res;
	struct curl_httppost *post = NULL;
	struct curl_httppost *last = NULL;
	struct curl_slist *headers = NULL;
	char errorbuffer[CURL_ERROR_SIZE] = "";

	curl = curl_easy_init();

	headers = curl_slist_append(headers, "Content-Type: text/plain");

	curl_formadd(&post, &last,
		CURLFORM_COPYNAME, "status",
		CURLFORM_COPYCONTENTS, MT_Challenge_StatusName(challenge_data->status),
		CURLFORM_END);
	curl_formadd(&post, &last,
		CURLFORM_COPYNAME, "ladderid",
		CURLFORM_COPYCONTENTS, challenge_data->ladderid,
		CURLFORM_END);
	curl_formadd(&post, &last,
		CURLFORM_COPYNAME, "hash",
		CURLFORM_COPYCONTENTS, challenge_data->hash,
		CURLFORM_END);
	curl_formadd(&post, &last,
		CURLFORM_COPYNAME, "token",
		CURLFORM_COPYCONTENTS, challenge_data->token,
		CURLFORM_END);
	curl_formadd(&post, &last,
		CURLFORM_COPYNAME, "players_count",
		CURLFORM_COPYCONTENTS, va("%d", challenge_data->players_count),
		CURLFORM_END);
	curl_formadd(&post, &last,
		CURLFORM_COPYNAME, "player1",
		CURLFORM_COPYCONTENTS, challenge_data->player1,
		CURLFORM_END);
	curl_formadd(&post, &last,
		CURLFORM_COPYNAME, "player2",
		CURLFORM_COPYCONTENTS, challenge_data->player2,
		CURLFORM_END);
	curl_formadd(&post, &last,
		CURLFORM_COPYNAME, "server",
		CURLFORM_COPYCONTENTS, challenge_data->server,
		CURLFORM_END);
	curl_formadd(&post, &last,
		CURLFORM_COPYNAME, "map",
		CURLFORM_COPYCONTENTS, challenge_data->map,
		CURLFORM_END);

	curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
	curl_easy_setopt(curl, CURLOPT_URL, challenge_data->url);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorbuffer);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, MT_Curl_Write_Void);

	res = curl_easy_perform(curl); /* post away! */

	curl_formfree(post);
	curl_easy_cleanup(curl);

	if (res != CURLE_OK) {
		Com_Printf("Challenge announcement upload failed:\n%s\n", curl_easy_strerror(res));
		Com_Printf("%s\n", errorbuffer);
	}
	else {
		Com_Printf("Challenge %s announced\n", MT_Challenge_StatusName(challenge_data->status));
	}

	MT_Challenge_Destroy(challenge_data);

	return 0;
}

static void MT_Challenge_BreakSend(void)
{
	challenge_data_t *thread_data;

	if (last_challenge != NULL) {
		// normal situation - reuse the challenge we announced on start
		thread_data = MT_Challenge_Copy(last_challenge);
		thread_data->status = challenge_end;
		thread_data->players_count = MT_CountPlayers();

		MT_Challenge_Destroy(last_challenge);
		last_challenge = NULL;
	}
	else {
		// weird situation, we didn't send the start of the challenge (perhaps late-join of the match)
		// but we now want to send the break of the challenge.. we will do our best-effort here
		// - simply generate new challenge and say we break it; it is up to the challenge server
		// what it will do with this message
		thread_data = MT_Challenge_Init(challenge_end);
	}

	if (Sys_CreateDetachedThread(MT_Challenge_StartSend_Thread, thread_data) < 0) {
		Com_Printf("Failed to create MT Challenge BreakSend thread\n");
	}
}

static void MT_Challenge_StartSend(void)
{
	challenge_data_t *thread_data;

	last_challenge = MT_Challenge_Init(challenge_start);

	thread_data = MT_Challenge_Copy(last_challenge);

	if (Sys_CreateDetachedThread(MT_Challenge_StartSend_Thread, thread_data) < 0) {
		Com_Printf("Failed to create MT Challenge StartSend thread\n");
	}
}

void MT_Challenge_Cancel(void)
{
	if (match_challenge.integer) {
		MT_Challenge_BreakSend();
	}
}

void MT_Challenge_StartMatch(void)
{
	if (last_challenge != NULL) {
		MT_Challenge_Destroy(last_challenge);
		last_challenge = NULL;
	}

	if (match_challenge.integer) {
		Cbuf_AddText("play items/protect.wav\n");
		Cbuf_AddText("say Challenge mode: on\n");
		MT_Challenge_StartSend();
	}
}

void MT_Challenge_InitCvars(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_MATCH_TOOLS);
	Cvar_Register(&match_auto_logupload);
	Cvar_Register(&match_auto_logurl);
	Cvar_Register(&match_auto_logupload_token);
	Cvar_Register(&match_challenge);
	Cvar_Register(&match_challenge_url);
	Cvar_Register(&match_ladder_id);
	Cvar_ResetCurrentGroup();
}

qbool MT_Challenge_Log_IsUploadAllowed(void)
{
	extern cvar_t match_auto_logconsole;

	return (match_challenge.integer || (match_auto_logupload.integer && match_auto_logconsole.integer))
		&& !cls.demoplayback
		&& cls.server_adr.type != NA_LOOPBACK;
}

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

static log_upload_job_t* Log_Upload_Job_Prepare(qbool challenge_mode, const char *challenge_hash,
	const char *filename, const char *hostname,
	const char* player_name, const char *token, const char *url,
	const char *mapname, const char *ladderid)
{
	log_upload_job_t *job = (log_upload_job_t *)Q_malloc(sizeof(log_upload_job_t));

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

static size_t Log_Curl_Write_Void(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	return size * nmemb;
}

int Log_AutoLogging_Upload_Thread(void *vjob)
{
	log_upload_job_t *job = (log_upload_job_t *)vjob;
	CURL *curl;
	CURLcode res;
	struct curl_httppost *post = NULL;
	struct curl_httppost *last = NULL;
	struct curl_slist *headers = NULL;
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
	if (Sys_CreateDetachedThread(Log_AutoLogging_Upload_Thread, (void *)job) < 0) {
		Com_Printf("Failed to create AutoLogging_Upload thread\n");
	}
}

qbool Log_TempLogUploadPending(void)
{
	return temp_log_upload_pending;
}

void Log_UploadTemp(void)
{
	char *tempname = va("%s/%s", MT_TempDirectory(), TEMP_LOG_NAME);
	temp_log_upload_pending = true;
	Log_AutoLogging_Upload(tempname);
}

void CL_QWURL_ProcessChallenge(const char *parameters)
{
	extern cvar_t match_auto_logupload_token;
	extern cvar_t match_challenge;

	// parameters is expected to be of the format "?token=<string>&otherparam=<string>&..."
	char info_buf[1024];
	char *wp = info_buf;
	const char *rp = parameters;
	size_t write_len = 0;
	ctxinfo_t ctx;
	char *token;
	memset(&ctx, 0, sizeof(ctxinfo_t));
	ctx.max = 20;

	while (*rp && write_len < 1022) {
		char c = *rp++;
		if (c == '?') {
			c = '\\';
		}
		else if (c == '&') {
			c = '\\';
		}
		else if (c == '=') {
			c = '\\';
		}

		*wp++ = c;
		write_len++;
	}

	*wp++ = '\0';

	Info_Convert(&ctx, info_buf);

	token = Info_Get(&ctx, "token");

	Info_RemoveAll(&ctx);

	if (*token) {
		Cvar_Set(&match_auto_logupload_token, token);
		Cvar_Set(&match_challenge, "1");
		Com_Printf("Joining challenge ...\n");
	}
	else {
		Com_Printf("Challenge token not found in the URL\n");
	}
}

qbool MT_ChallengeSpecified(void)
{
	extern cvar_t match_challenge;

	return match_challenge.integer != 0;
}
