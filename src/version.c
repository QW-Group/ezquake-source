/*
	version.c

	Build number and version strings

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
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id: version.c,v 1.14 2007-05-28 10:47:36 johnnycz Exp $
*/

#include "common.h"
#include "version.h"

#include <jansson.h>
#include <SDL_mutex.h>
#include <SDL_thread.h>
#include <curl/curl.h>


#define VERSION_UNKNOWN "Unknown"
#define VERSION_GITHUB_MAXLEN 32768 // ~2k @ ~2023
#define VERSION_GITHUB_URL "https://api.github.com/repos/qw-group/ezquake-source/releases/latest"

static char version_latest[VERSION_MAX_LEN] = VERSION_UNKNOWN;
static qbool version_refreshing = false;
static SDL_mutex *version_mutex = NULL;

static void VersionCheck_OnConfigChange(cvar_t *var, char *string, qbool *cancel);
static cvar_t allow_update_check  = {"sys_update_check",  "1", CVAR_NONE, VersionCheck_OnConfigChange};

/*
=======================
CL_Version_f
======================
*/
void CL_Version_f (void)
{
	Com_Printf ("ezQuake %s\n", VersionString());
	Com_Printf ("Exe: "__DATE__" "__TIME__"\n");

#ifdef _DEBUG
	Con_Printf("debug build\n");
#endif

#ifdef __MINGW32__
	Con_Printf("Compiled with MinGW version: %i.%i\n",__MINGW32_MAJOR_VERSION, __MINGW32_MINOR_VERSION);
#endif

#ifdef __CYGWIN__
	Con_Printf("Compiled with Cygwin\n");
#endif

#ifdef __GNUC__
	Con_Printf("Compiled with GCC version: %i.%i.%i (%i)\n",__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__, __VERSION__);

	#ifdef __OPTIMIZE__
		#ifdef __OPTIMIZE_SIZE__
			Con_Printf("GCC Optimization: Optimized for size\n");
		#else
			Con_Printf("GCC Optimization: Optimized for speed\n");
		#endif
	#endif

	#ifdef __NO_INLINE__
		Con_Printf("GCC Optimization: Functions currently not inlined into their callers\n");
	#else
		Con_Printf("GCC Optimization: Functions currently inlined into their callers\n");
	#endif
#endif

#ifdef _M_IX86
	Con_Printf("x86 code optimized for: ");

	if (_M_IX86 == 600) { Con_Printf("Pentium Pro, Pentium II and Pentium III"); }
	else if (_M_IX86 == 500) { Con_Printf("Pentium"); }
	else if (_M_IX86 == 400) { Con_Printf("486"); }
	else if (_M_IX86 == 300) { Con_Printf("386"); }
	else
	{
		Con_Printf("Unknown (%i)\n",_M_IX86);
	}

	Con_Printf("\n");
#endif

#ifdef _M_IX86_FP
	if (_M_IX86_FP == 0) { Con_Printf("SSE & SSE2 instructions disabled\n"); }
	else if (_M_IX86_FP == 1) { Con_Printf("SSE instructions enabled\n"); }
	else if (_M_IX86_FP == 2) { Con_Printf("SSE2 instructions enabled\n"); }
	else
	{
		Con_Printf("Unknown Arch specified: %i\n",_M_IX86_FP);
	}
#endif

#ifdef EZ_FREETYPE_SUPPORT
	Con_Printf("Portions of this software are copyright (c) 2018 The FreeType Project(www.freetype.org). All rights reserved.\n");
#endif
}

/*
=======================
VersionString
======================
*/
char *VersionString (void)
{
	static char str[64];

	snprintf (str, sizeof(str), "%s %s", VERSION_NUMBER, VERSION);

	return str;
}

char *VersionStringColour(void)
{
	static char str[64];

	snprintf (str, sizeof(str), "&c1e1%s&r %s", VERSION_NUMBER, VERSION);

	return str;
}

char *VersionStringFull (void)
{
	static char str[256];

	snprintf (str, sizeof(str), SERVER_NAME " %s " "(" QW_PLATFORM ")" "\n", VersionString());

	return str;
}

typedef struct version_context_St
{
	char *buffer;
	char *ptr;
} version_context_t;

static size_t VersionReadGitHubResponse(void* chunk, size_t size, size_t nmemb, void* user_data)
{
	version_context_t* ctx = (version_context_t *) user_data;

	size_t chunk_len = size * nmemb;
	size_t remaining = VERSION_GITHUB_MAXLEN - (ctx->ptr - ctx->buffer) - 1;

	if (chunk_len > remaining)
	{
		return 0;
	}

	memcpy(ctx->ptr, chunk, chunk_len);
	ctx->ptr += chunk_len;
	*ctx->ptr = '\0';

	return chunk_len;
}

static int VersionCheck_Thread(void *args)
{
	char buffer[VERSION_GITHUB_MAXLEN] = { 0 };
	json_error_t error;
	json_t *root, *tag_name;
	struct curl_slist *headers = NULL;
	version_context_t ctx = {
		.buffer = buffer,
		.ptr = buffer,
	};

	root = tag_name = NULL;

	CURL* curl = curl_easy_init();
	if (!curl)
	{
		Com_DPrintf("Unable to create cURL client\n");
		goto cleanup;
	}

	curl_easy_setopt(curl, CURLOPT_URL, VERSION_GITHUB_URL);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, VersionReadGitHubResponse);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);

	headers = curl_slist_append(headers, "User-Agent: Shub-Niggurath Inc");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	
	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK)
	{
		Com_DPrintf("Could not fetch latest version: %s", curl_easy_strerror(res));
		goto cleanup;
	}

	root = json_loads(buffer, 0, &error);
	if (!root)
	{
		Com_DPrintf("JSON decode failed: %s", error.text);
		goto cleanup;
	}
	
	tag_name = json_object_get(root, "tag_name");
	if (!json_is_string(tag_name))
	{
		Com_DPrintf("Version not found in GitHub response");
		goto cleanup;
	}

	const char* version_str = json_string_value(tag_name);

	SDL_LockMutex(version_mutex);
	strlcpy(version_latest, version_str, VERSION_MAX_LEN);
	SDL_UnlockMutex(version_mutex);

cleanup:
	if (root != NULL)
	{
		json_decref(root);
	}
	if (curl != NULL)
	{
		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);
	}

	SDL_LockMutex(version_mutex);
	version_refreshing = false;
	SDL_UnlockMutex(version_mutex);

	return 0;
}

static void VersionCheck_Refresh(void)
{
	SDL_LockMutex(version_mutex);

	if (!version_refreshing)
	{
		SDL_Thread *thread = SDL_CreateThread(VersionCheck_Thread, "Version fetcher", NULL);
		if (!thread)
		{
			Com_DPrintf("Could not start version fetcher: %s\n", SDL_GetError());
			version_refreshing = false;
		}
		else
		{
			SDL_DetachThread(thread);
			version_refreshing = true;
		}
	}

	SDL_UnlockMutex(version_mutex);
}

static void VersionCheck_OnConfigChange(cvar_t *cfg, char *string, qbool *cancel)
{
	if (strcmp(string, "0") == 0)
	{
		return;
	}

	VersionCheck_Refresh();
}

void VersionCheck_Init(void)
{
	Cvar_Register(&allow_update_check);

	version_mutex = SDL_CreateMutex();
}

void VersionCheck_Shutdown(void)
{
	if (version_mutex != NULL)
	{
		SDL_DestroyMutex(version_mutex);
		version_mutex = NULL;
	}
}

typedef struct semantic_version_St
{
	int major;
	int minor;
	int patch;
} semantic_version_t;

static qbool SemanticVersionFromString(const char* value, semantic_version_t* version)
{
	if (!value)
		return false;
	if (sscanf(value, "%d.%d.%d", &version->major, &version->minor, &version->patch) != 3)
		return false;
	return true;
}

static qbool VersionCheck_CompareVersions(const semantic_version_t *this_version, const semantic_version_t *latest_version)
{
	if (this_version->major < latest_version->major)
		return true;
	if (this_version->major > latest_version->major)
		return false;
	if (this_version->minor < latest_version->minor)
		return true;
	if (this_version->minor > latest_version->minor)
		return false;
	return this_version->patch < latest_version->patch;
}

// Assumes any failures = already on latest version to avoid erroneous showing of update message
static qbool VersionCheck_IsOutdated(const char recent_version[VERSION_MAX_LEN])
{
	semantic_version_t v1, v2;

	if (strcmp(recent_version, VERSION_UNKNOWN) == 0)
		return false;
	
	if (!SemanticVersionFromString(VERSION_NUMBER, &v1))
	{
		Com_DPrintf("Could not parse own version: %s", VERSION_NUMBER);
		return false;
	}

	if (!SemanticVersionFromString(recent_version, &v2))
	{
		Com_DPrintf("Could not parse recent version: %s", recent_version);
		return false;
	}

	return VersionCheck_CompareVersions(&v1, &v2);
}

qbool VersionCheck_GetLatest(char dest[VERSION_MAX_LEN])
{
	SDL_LockMutex(version_mutex);
	strlcpy(dest, version_latest, VERSION_MAX_LEN);
	SDL_UnlockMutex(version_mutex);

	return VersionCheck_IsOutdated(dest);
}