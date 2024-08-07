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

    $Id: common.c,v 1.108 2007-10-13 15:36:55 cokeman1982 Exp $

*/

#include "common.h"

#ifdef _WIN32
#include <direct.h>
#include <shlobj.h>
#include <errno.h>
#else
#include <stdio.h>
#include <unistd.h>
#endif

#include "quakedef.h"

#include "utils.h"
#include "gl_model.h"
#include "teamplay.h"
#include "crc.h"
#include "stats_grid.h"
#include "tp_triggers.h"
#include "fs.h"

typedef struct commandline_option_s {
	const char* name;
	int position;
} commandline_option_t;

#define CMDLINE_DEF(x, str) { str, 0 }

static commandline_option_t commandline_parameters[num_cmdline_params] = {
#include "cmdline_params_ids.h"
};

#undef CMDLINE_DEF

const char* Cmd_CommandLineParamName(cmdline_param_id id)
{
	return commandline_parameters[id].name;
}

void Draw_BeginDisc (void);
void Draw_EndDisc (void);

#define MAX_NUM_ARGVS	50

usercmd_t nullcmd; // guaranteed to be zero

static char	*largv[MAX_NUM_ARGVS + 1];

cvar_t	developer = {"developer", "0"};
cvar_t	host_mapname = {"mapname", "", CVAR_ROM};

qbool com_serveractive = false;


/*
All of Quake's data access is through a hierarchic file system, but the contents of the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and all game directories.  The sys_* files pass this to host_init in quakeparms_t->basedir.  This can be overridden with the "-basedir" command line parm to allow code debugging in a different directory.  The base directory is
only used during filesystem initialization.

The "game directory" is the first tree on the search path and directory that all generated files (savegames, screenshots, demos, config files) will be saved to.  This can be overridden with the "-game" command line parameter.  The game directory can never be changed while quake is executing.  This is a precacution against having a malicious server instruct clients to write files over areas they shouldn't.
*/

//============================================================================

void COM_StoreOriginalCmdline (int argc, char **argv)
{
	char buf[4096]; // enough?
	int i;

	buf[0] = 0;
	strlcat (buf, " ", sizeof (buf));

	for (i=0; i < argc; i++)
		strlcat (buf, argv[i], sizeof (buf));

	com_args_original = Q_strdup (buf);
}

//============================================================================

const char *COM_SkipPath (const char *pathname)
{
	const char *last;
	const char *p;

	last = p = pathname;

	while (*p)
	{
		if (*p == '/' || *p == '\\')
			last = p + 1;

		p++;
	}

	return last;
}

//============================================================================

char *COM_SkipPathWritable (char *pathname)
{
	char *last;
	char *p;

	last = p = pathname;

	while (*p)
	{
		if (*p == '/' || *p == '\\')
			last = p + 1;

		p++;
	}

	return last;
}

//
// Makes a path fit in a specified sized string "c:\quake\bla\bla\bla" => "c:\quake...la\bla"
//
char *COM_FitPath(char *dest, int destination_size, char *src, int size_to_fit)
{
	if (!src)
	{
		return dest;
	}

	if (size_to_fit > destination_size)
	{
		size_to_fit = destination_size;
	}

	if (strlen(src) <= size_to_fit)
	{
		// Entire path fits in the destination.
		strlcpy(dest, src, destination_size);
	}
	else
	{
		#define DOT_SIZE		3
		#define MIN_LEFT_SIZE	3
		int right_size = 0;
		int left_size = 0;
		int temp = 0;
		char *right_dir = COM_SkipPathWritable(src);

		// Get the size of the right-most part of the path (filename/directory).
		right_size = strlen (right_dir);

		// Try to fit the dir/filename if possible.
		temp = right_size + DOT_SIZE + MIN_LEFT_SIZE;
		right_size = (temp > size_to_fit) ? abs(right_size - (temp - size_to_fit)) : right_size;

		// Let the left have the rest.
		left_size = size_to_fit - right_size - DOT_SIZE;

		// Only let the left have 35% of the total size.
		left_size = min (left_size, Q_rint(0.35 * size_to_fit));

		// Get the final right size.
		right_size = size_to_fit - left_size - DOT_SIZE - 1;

		// Make sure we don't have negative values,
		// because then our safety checks won't work.
		left_size = max (0, left_size);
		right_size = max (0, right_size);

		if (left_size < destination_size)
		{
			strlcpy (dest, src, destination_size);
			strlcpy (dest + left_size, "...", destination_size);

			if (left_size + DOT_SIZE + right_size < destination_size)
			{
				strlcpy (dest + left_size + DOT_SIZE, src + strlen(src) - right_size, right_size + 1);
			}
		}
	}

	return dest;
}

void COM_StripExtension (const char *in, char *out, int out_size)
{
	char *dot;

	if (in != out)
		strlcpy (out, in, out_size);

	dot = strrchr (out, '.');
	if (dot) {
		*dot = 0;
	}
}


char *COM_FileExtension (const char *in)
{
	static char exten[8];
	int i;

	if (!(in = strrchr(in, '.')))
		return "";
	in++;
	for (i = 0 ; i < 7 && *in ; i++,in++)
		exten[i] = *in;
	exten[i] = 0;
	return exten;
}

//
// Extract file name without extension, to be used for hunk tags (up to 32 characters, including trailing zero)
//
void COM_FileBase (const char *in, char *out)
{
	const char *start, *end;
	int	length;

	if (!(end = strrchr(in, '.')))
		end = in + strlen(in);

	if (!(start = strrchr(in, '/')))
		start = in;
	else
		start += 1;

	length = end - start;
	if (length < 0)
		strcpy (out, "?empty filename?");
	else if (length == 0)
		out[0] = 0;
	else {
		if (length > 31)
			length = 31;
		memcpy (out, start, length);
		out[length] = 0;
	}
}

//
// If path doesn't have a .EXT, append extension (extension should include the .)
//
void COM_DefaultExtension (char *path, char *extension, size_t path_size)
{
	char *src;

	src = path + strlen(path) - 1;

	while (*src != '/' && src != path) {
		if (*src == '.')
			return;                 // it has an extension
		src--;
	}

	strlcat (path, extension, path_size);
}

// If path doesn't have an extension or has a different extension, append(!) specified extension
// Extension should include the .
// path buffer supposed to be MAX_OSPATH size
void COM_ForceExtension (char *path, char *extension)
{
	char *src;

	src = path + strlen(path) - strlen(extension);
	if (src >= path && !strcmp(src, extension))
		return;

	strlcat (path, extension, MAX_OSPATH);
}

// If path doesn't have an extension or has a different extension, append(!) specified extension
// a bit extended version of COM_ForceExtension(), we suply size of path, so append safe, sure if u provide right path size
void COM_ForceExtensionEx (char *path, char *extension, size_t path_size)
{
	char *src;

	if (path_size < 1 || path_size <= strlen (extension)) // too small buffer, can't even theoreticaly append extension
		Sys_Error("COM_ForceExtensionEx: internall error"); // this is looks like a bug, be fatal then

	src = path + strlen (path) - strlen (extension);
	if (src >= path && !strcmp (src, extension))
		return; // seems we alredy have this extension

	strlcat (path, extension, path_size);

	src = path + strlen (path) - strlen (extension);
	if (src >= path && !strcmp (src, extension))
		return; // seems we succeed

	Com_Printf ("Failed to force extension %s for %s\n", extension, path); // its good to know about failure
}

//
// Get the path of a temporary directory.
// Returns length of the path or negative value if error.
//
int COM_GetTempDir(char *buf, int bufsize)
{
	int returnval = 0;
#ifdef WIN32
	returnval = GetTempPath (bufsize, buf);

	if (returnval > bufsize || returnval == 0) {
		return -1;
	}
#else // UNIX
	char *tmp;
	if (!(tmp = getenv ("TMPDIR")))
		tmp = P_tmpdir; // defined at <stdio.h>

	returnval = strlen (tmp);

	if (returnval > bufsize || returnval == 0) {
		return -1;
	}

	strlcpy (buf, tmp, bufsize);
#endif // WIN32

	return returnval;
}

qbool COM_WriteToUniqueTempFileVFS(char* path, int path_buffer_size, const char* ext, vfsfile_t* input)
{
	size_t bytes = VFS_GETLEN(input);
	byte* buffer = Q_malloc(bytes);
	vfserrno_t err;
	qbool result;

	if (!buffer) {
		return false;
	}
	VFS_READ(input, buffer, (int)bytes, &err);
	VFS_SEEK(input, 0, SEEK_SET);

	result = COM_WriteToUniqueTempFile(path, path_buffer_size, ext, buffer, bytes);
	Q_free(buffer);
	return result;
}

//
// Get a unique temp filename.
// Returns negative value on failure.
//
qbool COM_WriteToUniqueTempFile(char *path, int path_buffer_size, const char* ext, const byte* buffer, size_t bytes)
{
	char temp_path[MAX_OSPATH];

	if (ext == NULL) {
		ext = "";
	}

	// Get the path to temporary directory
	if (COM_GetTempDir(temp_path, sizeof(temp_path)) < 0) {
		return false;
	}

#ifdef _WIN32
	{
		GUID guid;
		char* dot = "";

		dot = (ext[0] && ext[0] != '.' ? "." : "");

		if (path_buffer_size < MAX_PATH) {
			return false;
		}

		if (CoCreateGuid(&guid) == S_OK) {
			if (path_buffer_size > snprintf(path, path_buffer_size, "%s\\%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X%s%s", temp_path, guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7], dot, ext)) {
				FS_WriteFile_2(path, buffer, (int)bytes);
				return true;
			}
		}

		return false;
	}
#else
	{
		int fd = 0;
		strlcat(path, "/ezquakeXXXXXX", path_buffer_size);
		if (ext[0]) {
			int ext_len = 0;

			if (ext[0] != '.') {
				strlcat(path, ".", path_buffer_size);
				strlcat(path, ext, path_buffer_size);
				ext_len = strlen(ext) + 1;
			}
			else {
				strlcat(path, ext, path_buffer_size);
				ext_len = strlen(ext);
			}
			fd = mkstemps(path, ext_len);
		}
		else {
			fd = mkstemp(path);
		}

		if (fd < 0) {
			return false;
		}

		write(fd, buffer, bytes);

		close(fd);
		return true;
	}
#endif
}

qbool COM_FileExists (char *path)
{
	FILE *fexists = NULL;

	// Try opening the file to see if it exists.
	fexists = fopen(path, "rb");

	// The file exists.
	if (fexists)
	{
		// Make sure the file is closed.
		fclose (fexists);
		return true;
	}

	return false;
}


char		com_token[MAX_COM_TOKEN];
static int	com_argc;
static char	**com_argv;
char 		*com_args_original;

com_tokentype_t com_tokentype;

//Parse a token out of a string
const char* COM_ParseEx(const char *data, qbool curlybraces)
{
	unsigned char c;
	int len;
	int quotes;

	len = 0;
	com_token[0] = 0;

	if (!data) {
		return NULL;
	}

	// skip whitespace
	while (true) {
		while ((c = *data) == ' ' || c == '\t' || c == '\r' || c == '\n') {
			data++;
		}

		if (c == 0) {
			return NULL;			// end of file;
		}

		// skip // comments
		if (c == '/' && data[1] == '/') {
			while (*data && *data != '\n') {
				data++;
			}
		}
		else {
			break;
		}
	}

	// handle quoted strings specially
	if (c == '\"' || (c == '{' && curlybraces) ) {
		if (c == '{') {
			quotes = 1;
		}
		else {
			quotes = -1;
		}
		data++;
		while (1) {
			c = *data;
			data++;
			if (quotes < 0) {
				if (c == '\"') {
					quotes++;
				}
			}
			else {
				if (c == '}' && curlybraces) {
					quotes--;
				}
				else if (c == '{' && curlybraces) {
					quotes++;
				}
			}

			if (!quotes || !c) {
				com_token[len] = 0;
				return c ? data : data - 1;
			}
			com_token[len] = c;
			len++;

			if (len >= sizeof(com_token) - 1) {
				return NULL; // quoted section too long
			}
		}
	}

	// parse a regular word
	do {
		com_token[len] = c;
		data++;
		len++;
		if (len >= sizeof(com_token) - 1) {
			break;
		}
		c = *data;
	} while (c && c != ' ' && c != '\t' && c != '\n' && c != '\r');

	com_token[len] = 0;
	return data;
}

const char* COM_Parse(const char* data)
{
	return COM_ParseEx(data, false);
}

#define DEFAULT_PUNCTUATION "(,{})(\':;=!><&|+"
char *COM_ParseToken (const char *data, const char *punctuation)
{
	int c;
	int	len;

	if (!punctuation)
		punctuation = DEFAULT_PUNCTUATION;

	len = 0;
	com_token[0] = 0;

	if (!data)
	{
		com_tokentype = TTP_UNKNOWN;
		return NULL;
	}

	// skip whitespace
skipwhite:
	while ((c = *(unsigned char *) data) <= ' ')
	{
		if (c == 0)
		{
			com_tokentype = TTP_UNKNOWN;
			return NULL; // end of file;
		}

		data++;
	}

	// skip // comments
	if (c == '/')
	{
		if (data[1] == '/')
		{
			while (*data && *data != '\n')
				data++;

			goto skipwhite;
		}
		else if (data[1] == '*')
		{
			data += 2;

			while (*data && (*data != '*' || data[1] != '/'))
				data++;

			data += 2;
			goto skipwhite;
		}
	}


	// handle quoted strings specially
	if (c == '\"')
	{
		com_tokentype = TTP_STRING;
		data++;
		while (1)
		{
			if (len >= MAX_COM_TOKEN - 1)
			{
				com_token[len] = '\0';
				return (char*) data;
			}

			c = *data++;

			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				return (char*) data;
			}

			com_token[len] = c;
			len++;
		}
	}

	com_tokentype = TTP_UNKNOWN;

	// parse single characters
	if (strchr (punctuation, c))
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return (char*) (data + 1);
	}

	// parse a regular word
	do
	{
		if (len >= MAX_COM_TOKEN - 1)
			break;

		com_token[len] = c;
		data++;
		len++;
		c = *data;
		if (strchr (punctuation, c))
			break;

	} while (c > 32);

	com_token[len] = 0;
	return (char*) data;
}

//Adds the given string at the end of the current argument list
void COM_AddParm (char *parm)
{
	if (com_argc >= MAX_NUM_ARGVS)
		return;
	largv[com_argc++] = parm;
}

//Returns the position (1 to argc-1) in the program's argument list
//where the given parameter appears, or 0 if not present
int COM_FindParm(const char* parm)
{
	int		i;

	for (i = 1; i < com_argc; i++) {
		if (!strcmp(parm, com_argv[i])) {
			return i;
		}
	}

	return 0;
}

int COM_CheckParm(cmdline_param_id id)
{
	if (id < 0 || id >= num_cmdline_params) {
		return 0;
	}

	return commandline_parameters[id].position;
}

//Tei: added cos -data feature use it.
int COM_CheckParmOffset(cmdline_param_id id, int offset)
{
	int             i;
	const char* parm;

	if (id < 0 || id >= num_cmdline_params) {
		return 0;
	}

	parm = commandline_parameters[id].name;
	for (i = offset; i < com_argc; i++) {
		// NEXTSTEP sometimes clears appkit vars.
		if (!com_argv[i]) {
			continue;
		}

		if (!strcmp(parm, com_argv[i])) {
			return i;
		}
	}

	return 0;
}
//qbism// 1999-12-23 Multiple "-data" parameters by Maddes  end


int COM_Argc (void)
{
	return com_argc;
}

char *COM_Argv(int arg)
{
	if (arg < 0 || arg >= com_argc) {
		return "";
	}

	return com_argv[arg];
}

void COM_ClearArgv(int arg)
{
	if (arg < 0 || arg >= com_argc) {
		return;
	}
	com_argv[arg] = "";
}

void COM_InitArgv(int argc, char **argv)
{
	int id;
	int i;

	for (i = 0, com_argc = 0; com_argc < MAX_NUM_ARGVS - 1 && i < argc; ++i) {
		if (argv[i]) {
			// follow qw urls if they are our argument without a +qwurl command
			if (!strncmp(argv[i], "qw://", 5) && (i == 0 || strncmp(argv[i - 1], "+qwurl", 6)) && com_argc < MAX_NUM_ARGVS - 1) {
				largv[com_argc++] = "+qwurl";
				largv[com_argc++] = argv[i];
			}
			else {
				largv[com_argc++] = argv[i];
			}
		}
		else {
			largv[com_argc++] = "";
		}
	}

	largv[com_argc] = "";
	com_argv = largv;

	for (id = 0; id < num_cmdline_params; ++id) {
		commandline_parameters[id].position = COM_FindParm(commandline_parameters[id].name);
	}
}

void COM_Init(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_NO_GROUP);
	Cvar_Register(&developer);
	Cvar_Register(&host_mapname);

	Cvar_ResetCurrentGroup();
}

//does a varargs printf into a temp buffer, so I don't need to have varargs versions of all text functions.
char *va(const char *format, ...)
{
	va_list argptr;
	static char string[32][2048];
	static int idx = 0;

	idx++;
	if (idx == 32) {
		idx = 0;
	}

	va_start (argptr, format);
	vsnprintf (string[idx], sizeof(string[idx]), format, argptr);
	va_end (argptr);

	return string[idx];
}

// equals to consecutive calls of strtok(s, " ") that assign values to array
int COM_GetFloatTokens(const char *s, float *fl_array, int fl_array_size)
{
    int i;
    if (!s || !*s) return 0;

    for(i = 0; *s && i < fl_array_size; i++)
    {
        fl_array[i] = atof(s);
        while(*s && *s != ' ') s++; // skips the number
        while(*s && *s == ' ') s++; // skips the spaces
    }

    return i;
}


/*
=====================================================================
  INFO STRINGS
=====================================================================
*/

//Searches the string for the given key and returns the associated value, or an empty string.
char *Info_ValueForKey (char *s, char *key) {
	char pkey[512];
	static char value[4][512];	// use two buffers so compares work without stomping on each other
	static int	valueindex;
	char *o;

	valueindex = (valueindex + 1) % 4;
	if (*s == '\\')
		s++;
	while (1) {
		o = pkey;
		while (*s != '\\') {
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s) {
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
			return value[valueindex];

		if (!*s)
			return "";
		s++;
	}
}

void Info_RemoveKey (char *s, char *key) {
	char *start, pkey[512], value[512], *o;

	if (strchr (key, '\\')) {
		Com_Printf ("Can't use a key with a \\\n");
		return;
	}

	while (1) {
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\') {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s) {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) ) {
			Q_strcpy (start, s);	// remove this part
			return;
		}

		if (!*s)
			return;
	}

}

void Info_RemovePrefixedKeys (char *start, char prefix) {
	char *s, pkey[512], value[512], *o;

	s = start;

	while (1) {
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\') {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s) {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (pkey[0] == prefix) {
			Info_RemoveKey (start, pkey);
			s = start;
		}

		if (!*s)
			return;
	}

}

void Info_SetValueForStarKeyEx (char *s, char *key, char *value, int maxsize, qbool max_info_key_check)
{
	char new[1024], *v;
	int c;
#ifndef CLIENTONLY
//	extern cvar_t sv_highchars;
#endif

	if (strchr (key, '\\') || strchr (value, '\\') ) {
		Com_Printf ("Can't use keys or values with a \\\n");
		return;
	}

	if (strchr (key, '\"') || strchr (value, '\"') ) {
		Com_Printf ("Can't use keys or values with a \"\n");
		return;
	}

	if (max_info_key_check)
	{
		if (strlen(key) >= MAX_INFO_KEY || strlen(value) >= MAX_INFO_KEY)
		{
			Com_Printf ("Keys and values must be < %i characters.\n", MAX_INFO_KEY);
			return;
		}
	}

	// this next line is kinda trippy
	if (*(v = Info_ValueForKey(s, key))) {
		// key exists, make sure we have enough room for new value, if we don't,
		// don't change it!
		if ((int) (strlen(value) - strlen(v) + strlen(s)) >= maxsize) {
			Com_Printf ("Info string length exceeded\n");
			return;
		}
	}
	Info_RemoveKey (s, key);
	if (!value || !strlen(value))
		return;

	snprintf (new, sizeof (new), "\\%s\\%s", key, value);

	if ((int) (strlen(new) + strlen(s)) >= maxsize) {
		Com_Printf ("Info string length exceeded\n");
		return;
	}

	// only copy ascii values
	s += strlen(s);
	v = new;
#ifndef CLIENTONLY
//	if (!sv_highchars.value) {
//		while (*v) {
//			c = (unsigned char)*v++;
//			if (c == ('\\'|128))
//				continue;
//			c &= 127;
//			if (c >= 32)
//				*s++ = c;
//		}
//	} else
#endif
	{
		while (*v) {
			c = (unsigned char)*v++;
			if (c > 13)
				*s++ = c;
		}
		*s = 0;
	}
}

void Info_SetValueForStarKey (char *s, char *key, char *value, int maxsize)
{
	Info_SetValueForStarKeyEx(s, key, value, maxsize, true);
}

void Info_SetValueForKeyEx (char *s, char *key, char *value, int maxsize, qbool max_info_key_check)
{
	if (key[0] == '*')
	{
		Com_Printf ("Can't set * keys\n");
		return;
	}

	Info_SetValueForStarKeyEx (s, key, value, maxsize, max_info_key_check);
}

void Info_SetValueForKey (char *s, char *key, char *value, int maxsize)
{
	Info_SetValueForKeyEx (s, key, value, maxsize, true);
}

#define INFO_PRINT_FIRST_COLUMN_WIDTH 20
void Info_Print (char *s) {
	char key[512], value[512], *o;
	int l;

	if (*s == '\\')
		s++;
	while (*s) {
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;

		l = o - key;
		if (l < INFO_PRINT_FIRST_COLUMN_WIDTH) {
			memset (o, ' ', INFO_PRINT_FIRST_COLUMN_WIDTH - l);
			key[INFO_PRINT_FIRST_COLUMN_WIDTH] = 0;
		} else {
			*o = 0;
		}
		Com_Printf ("%s", key);

		if (!*s) {
			Com_Printf ("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;
		Com_Printf ("%s\n", value);
	}
}

//============================================================
//
// Alternative variant manipulation with info strings
//
//============================================================

// this is seems to be "better" than Com_HashKey()
static unsigned int Info_HashKey (const char *str)
{
	unsigned int hash = 0;
	int c;

	// the (c&~32) makes it case-insensitive
	// hash function known as sdbm, used in gawk
	while ((c = *str++))
        hash = (c &~ 32) + (hash << 6) + (hash << 16) - hash;

    return hash;
}

// used internally
static info_t *_Info_Get (ctxinfo_t *ctx, const char *name)
{
	info_t *a;
	int key;

	if (!ctx || !name || !name[0])
		return NULL;

	key = Info_HashKey (name) % INFO_HASHPOOL_SIZE;

	for (a = ctx->info_hash[key]; a; a = a->hash_next)
		if (!strcasecmp(name, a->name))
			return a;

	return NULL;
}

char *Info_Get(ctxinfo_t *ctx, const char *name)
{
	static	char value[4][512];
	static	int valueindex = 0;

	info_t *a = _Info_Get(ctx, name);

	if ( a )
	{
		valueindex = (valueindex + 1) % 4;

		strlcpy(value[valueindex], a->value, sizeof(value[0]));

		return value[valueindex];
	}
	else
	{
		return "";
	}
}

qbool Info_SetStar (ctxinfo_t *ctx, const char *name, const char *value)
{
	info_t	*a;
	int key;

	if (!value)
		value = "";

	if (!ctx || !name || !name[0])
		return false;

	// empty value, instead of set just remove it
	if (!value[0])
	{
		return Info_Remove(ctx, name);
	}

	if (strchr(name, '\\') || strchr(value, '\\'))
		return false;
	if (strchr(name, 128 + '\\') || strchr(value, 128 + '\\'))
		return false;
	if (strchr(name, '"') || strchr(value, '"'))
		return false;
	if (strchr(name, '\r') || strchr(value, '\r')) // bad for print functions
		return false;
	if (strchr(name, '\n') || strchr(value, '\n')) // bad for print functions
		return false;
	if (strchr(name, '$') || strchr(value, '$')) // variable expansion may be exploited, escaping this
		return false;
	if (strchr(name, ';') || strchr(value, ';')) // interpreter may be haxed, escaping this
		return false;

	if (strlen(name) >= MAX_KEY_STRING || strlen(value) >= MAX_KEY_STRING)
		return false; // too long name/value, its wrong

	key = Info_HashKey(name) % INFO_HASHPOOL_SIZE;

	// if already exists, reuse it
	for (a = ctx->info_hash[key]; a; a = a->hash_next)
		if (!strcasecmp(name, a->name))
		{
			Q_free (a->value);
			break;
		}

	// not found, create new one
	if (!a)
	{
		if (ctx->cur >= ctx->max)
			return false; // too much infos

		a = (info_t *) Q_malloc (sizeof(info_t));
		a->next = ctx->info_list;
		ctx->info_list = a;
		a->hash_next = ctx->info_hash[key];
		ctx->info_hash[key] = a;

		ctx->cur++; // increase counter

		// copy name
		a->name = Q_strdup (name);
	}

	// copy value
#if 0
	{
		// unfortunatelly evil users use non printable/control chars, so that does not work well
		a->value = Q_strdup (value);
	}
#else
	{
		// skip some control chars, doh
		char v_buf[MAX_KEY_STRING] = {0}, *v = v_buf;
		int i;

		for (i = 0; value[i]; i++) // len of 'value' should be less than MAX_KEY_STRING according to above checks
		{
			if ((unsigned char)value[i] > 13)
				*v++ = value[i];
		}
		*v = 0;

		a->value = Q_strdup (v_buf);
	}
#endif

	// hrm, empty value, remove it then
	if (!a->value[0])
	{
		return Info_Remove(ctx, name);
	}

	return true;
}

qbool Info_Set (ctxinfo_t *ctx, const char *name, const char *value)
{
	if (!value)
		value = "";

	if (!ctx || !name || !name[0])
		return false;

	if (name[0] == '*')
	{
		Con_Printf ("Can't set * keys\n");
		return false;
	}

	return Info_SetStar (ctx, name, value);
}

// used internally
static void _Info_Free(info_t *a)
{
	if (!a)
		return;

	Q_free (a->name);
	Q_free (a->value);
	Q_free (a);
}

qbool Info_Remove (ctxinfo_t *ctx, const char *name)
{
	info_t *a, *prev;
	int key;

	if (!ctx || !name || !name[0])
		return false;

	key = Info_HashKey (name) % INFO_HASHPOOL_SIZE;

	prev = NULL;
	for (a = ctx->info_hash[key]; a; a = a->hash_next)
	{
		if (!strcasecmp(name, a->name))
		{
			// unlink from hash
			if (prev)
				prev->hash_next = a->hash_next;
			else
				ctx->info_hash[key] = a->hash_next;
			break;
		}
		prev = a;
	}

	if (!a)
		return false;	// not found

	prev = NULL;
	for (a = ctx->info_list; a; a = a->next)
	{
		if (!strcasecmp(name, a->name))
		{
			// unlink from info list
			if (prev)
				prev->next = a->next;
			else
				ctx->info_list = a->next;

			// free
			_Info_Free(a);

			ctx->cur--; // decrease counter

			return true;
		}
		prev = a;
	}

	Sys_Error("Info_Remove: info list broken");
	return false; // shut up compiler
}

// remove all infos
void Info_RemoveAll (ctxinfo_t *ctx)
{
	info_t	*a, *next;

	if (!ctx)
		return;

	for (a = ctx->info_list; a; a = next) {
		next = a->next;

		// free
		_Info_Free(a);
	}
	ctx->info_list = NULL;
	ctx->cur = 0; // set counter to 0

	// clear hash
	memset (ctx->info_hash, 0, sizeof(ctx->info_hash));
}

qbool Info_Convert(ctxinfo_t *ctx, char *str)
{
	char name[MAX_KEY_STRING], value[MAX_KEY_STRING], *start;

	if (!ctx)
		return false;

	for ( ; str && str[0]; )
	{
		if (!(str = strchr(str, '\\')))
			break;

		start = str; // start of name

		if (!(str = strchr(start + 1, '\\')))  // end of name
			break;

		strlcpy(name, start + 1, min(str - start, (int)sizeof(name)));

		start = str; // start of value

		str = strchr(start + 1, '\\'); // end of value

		strlcpy(value, start + 1, str ? min(str - start, (int)sizeof(value)) : (int)sizeof(value));

		Info_SetStar(ctx, name, value);
	}

	return true;
}

qbool Info_ReverseConvert(ctxinfo_t *ctx, char *str, int size)
{
	info_t *a;
	int next_size;
	
	if (!ctx)
		return false;

	if (!str || size < 1)
		return false;

	str[0] = 0;

	for (a = ctx->info_list; a; a = a->next)
	{
		if (!a->value[0])
			continue; // empty

		next_size = size - 2 - strlen(a->name) - strlen(a->value);

		if (next_size < 1)
		{
			// sigh, next snprintf will not fit
			return false;
		}

		snprintf(str, size, "\\%s\\%s", a->name, a->value);
		str += (size - next_size);
		size = next_size;
	}

	return true;
}

qbool Info_CopyStar(ctxinfo_t *ctx_from, ctxinfo_t *ctx_to)
{
	info_t *a;

	if (!ctx_from || !ctx_to)
		return false;

	if (ctx_from == ctx_to)
		return true; // hrm

	for (a = ctx_from->info_list; a; a = a->next)
	{
		if (a->name[0] != '*')
			continue; // not a star key

		// do we need check status of this function?
		Info_SetStar (ctx_to, a->name, a->value);
	}

	return true;
}

void Info_PrintList(ctxinfo_t *ctx)
{
	info_t *a;
	int cnt = 0;

	if (!ctx)
		return;

	for (a = ctx->info_list; a; a = a->next)
	{
		Con_Printf("%-20s %s\n", a->name, a->value);
		cnt++;
	}

	
	Con_DPrintf("%d infos\n", cnt);
}

//============================================================================

static byte chktbl[1024] = {
                               0x78,0xd2,0x94,0xe3,0x41,0xec,0xd6,0xd5,0xcb,0xfc,0xdb,0x8a,0x4b,0xcc,0x85,0x01,
                               0x23,0xd2,0xe5,0xf2,0x29,0xa7,0x45,0x94,0x4a,0x62,0xe3,0xa5,0x6f,0x3f,0xe1,0x7a,
                               0x64,0xed,0x5c,0x99,0x29,0x87,0xa8,0x78,0x59,0x0d,0xaa,0x0f,0x25,0x0a,0x5c,0x58,
                               0xfb,0x00,0xa7,0xa8,0x8a,0x1d,0x86,0x80,0xc5,0x1f,0xd2,0x28,0x69,0x71,0x58,0xc3,
                               0x51,0x90,0xe1,0xf8,0x6a,0xf3,0x8f,0xb0,0x68,0xdf,0x95,0x40,0x5c,0xe4,0x24,0x6b,
                               0x29,0x19,0x71,0x3f,0x42,0x63,0x6c,0x48,0xe7,0xad,0xa8,0x4b,0x91,0x8f,0x42,0x36,
                               0x34,0xe7,0x32,0x55,0x59,0x2d,0x36,0x38,0x38,0x59,0x9b,0x08,0x16,0x4d,0x8d,0xf8,
                               0x0a,0xa4,0x52,0x01,0xbb,0x52,0xa9,0xfd,0x40,0x18,0x97,0x37,0xff,0xc9,0x82,0x27,
                               0xb2,0x64,0x60,0xce,0x00,0xd9,0x04,0xf0,0x9e,0x99,0xbd,0xce,0x8f,0x90,0x4a,0xdd,
                               0xe1,0xec,0x19,0x14,0xb1,0xfb,0xca,0x1e,0x98,0x0f,0xd4,0xcb,0x80,0xd6,0x05,0x63,
                               0xfd,0xa0,0x74,0xa6,0x86,0xf6,0x19,0x98,0x76,0x27,0x68,0xf7,0xe9,0x09,0x9a,0xf2,
                               0x2e,0x42,0xe1,0xbe,0x64,0x48,0x2a,0x74,0x30,0xbb,0x07,0xcc,0x1f,0xd4,0x91,0x9d,
                               0xac,0x55,0x53,0x25,0xb9,0x64,0xf7,0x58,0x4c,0x34,0x16,0xbc,0xf6,0x12,0x2b,0x65,
                               0x68,0x25,0x2e,0x29,0x1f,0xbb,0xb9,0xee,0x6d,0x0c,0x8e,0xbb,0xd2,0x5f,0x1d,0x8f,
                               0xc1,0x39,0xf9,0x8d,0xc0,0x39,0x75,0xcf,0x25,0x17,0xbe,0x96,0xaf,0x98,0x9f,0x5f,
                               0x65,0x15,0xc4,0x62,0xf8,0x55,0xfc,0xab,0x54,0xcf,0xdc,0x14,0x06,0xc8,0xfc,0x42,
                               0xd3,0xf0,0xad,0x10,0x08,0xcd,0xd4,0x11,0xbb,0xca,0x67,0xc6,0x48,0x5f,0x9d,0x59,
                               0xe3,0xe8,0x53,0x67,0x27,0x2d,0x34,0x9e,0x9e,0x24,0x29,0xdb,0x69,0x99,0x86,0xf9,
                               0x20,0xb5,0xbb,0x5b,0xb0,0xf9,0xc3,0x67,0xad,0x1c,0x9c,0xf7,0xcc,0xef,0xce,0x69,
                               0xe0,0x26,0x8f,0x79,0xbd,0xca,0x10,0x17,0xda,0xa9,0x88,0x57,0x9b,0x15,0x24,0xba,
                               0x84,0xd0,0xeb,0x4d,0x14,0xf5,0xfc,0xe6,0x51,0x6c,0x6f,0x64,0x6b,0x73,0xec,0x85,
                               0xf1,0x6f,0xe1,0x67,0x25,0x10,0x77,0x32,0x9e,0x85,0x6e,0x69,0xb1,0x83,0x00,0xe4,
                               0x13,0xa4,0x45,0x34,0x3b,0x40,0xff,0x41,0x82,0x89,0x79,0x57,0xfd,0xd2,0x8e,0xe8,
                               0xfc,0x1d,0x19,0x21,0x12,0x00,0xd7,0x66,0xe5,0xc7,0x10,0x1d,0xcb,0x75,0xe8,0xfa,
                               0xb6,0xee,0x7b,0x2f,0x1a,0x25,0x24,0xb9,0x9f,0x1d,0x78,0xfb,0x84,0xd0,0x17,0x05,
                               0x71,0xb3,0xc8,0x18,0xff,0x62,0xee,0xed,0x53,0xab,0x78,0xd3,0x65,0x2d,0xbb,0xc7,
                               0xc1,0xe7,0x70,0xa2,0x43,0x2c,0x7c,0xc7,0x16,0x04,0xd2,0x45,0xd5,0x6b,0x6c,0x7a,
                               0x5e,0xa1,0x50,0x2e,0x31,0x5b,0xcc,0xe8,0x65,0x8b,0x16,0x85,0xbf,0x82,0x83,0xfb,
                               0xde,0x9f,0x36,0x48,0x32,0x79,0xd6,0x9b,0xfb,0x52,0x45,0xbf,0x43,0xf7,0x0b,0x0b,
                               0x19,0x19,0x31,0xc3,0x85,0xec,0x1d,0x8c,0x20,0xf0,0x3a,0xfa,0x80,0x4d,0x2c,0x7d,
                               0xac,0x60,0x09,0xc0,0x40,0xee,0xb9,0xeb,0x13,0x5b,0xe8,0x2b,0xb1,0x20,0xf0,0xce,
                               0x4c,0xbd,0xc6,0x04,0x86,0x70,0xc6,0x33,0xc3,0x15,0x0f,0x65,0x19,0xfd,0xc2,0xd3,

                               // Only the first 512 bytes of the table are initialized, the rest
                               // is just zeros.
                               // This is an idiocy in QW but we can't change this, or checksums
                               // will not match.
                           };

//For proxy protecting
byte COM_BlockSequenceCRCByte (byte *base, int length, int sequence) {
	unsigned short crc;
	byte *p;
	byte chkb[60 + 4];

	p = chktbl + ((unsigned int)sequence % (sizeof(chktbl) - 4));

	if (length > 60)
		length = 60;
	memcpy (chkb, base, length);

	chkb[length] = (sequence & 0xff) ^ p[0];
	chkb[length+1] = p[1];
	chkb[length+2] = ((sequence>>8) & 0xff) ^ p[2];
	chkb[length+3] = p[3];

	length += 4;

	crc = CRC_Block (chkb, length);

	crc &= 0xff;

	return crc;
}

//=====================================================================

#define	MAXPRINTMSG	4096

void (*rd_print) (char *) = NULL;

void Com_BeginRedirect (void (*RedirectedPrint) (char *)) {
	rd_print = RedirectedPrint;
}

void Com_EndRedirect (void) {
	rd_print = NULL;
}

// All console printing must go through this in order to be logged to disk
unsigned Print_flags[16];
int Print_current = 0;

/* FIXME: Please make us thread safe! */
void Com_Printf (char *fmt, ...) 
{
	va_list argptr;
	char msg[MAXPRINTMSG];

	va_start (argptr, fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	if (rd_print) {
		// add to redirected message
		rd_print (msg);
		return;
	}

	// also echo to debugging console
	Sys_Printf ("%s", msg);

	/* This is beyond retarded because it may cause RECURSION */
	// Triggers with mask 64
	if (!(Print_flags[Print_current] & PR_TR_SKIP))
		CL_SearchForReTriggers (msg, RE_PRINT_INTERNAL);

	// write it to the scrollable buffer
	//	Con_Print (va("ezQuake: %s", msg));
	Con_PrintW (str2wcs(msg));
}

void Com_DPrintf (char *fmt, ...) 
{
	va_list argptr;
	char msg[MAXPRINTMSG];

	if (!developer.value)
		return;			// don't confuse non-developers with techie stuff...

	Print_flags[Print_current] |= PR_TR_SKIP;
	va_start (argptr,fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	Com_Printf ("%s", msg);
}

void Com_Printf_State(int state, const char *fmt, ...)
{
/* Com_Printf equivalent with importance level as a first parm for each msg,
   it should be one of PRINT_OK, PRINT_INFO, PRINT_FAIL defines
   PRINT_OK won't be displayed unless we're in developer mode
*/
	va_list argptr;
	char *prefix = "";
	char msg[MAXPRINTMSG];

	msg[0] = 0;

	switch (state)
	{
		case PRINT_FAIL:
			prefix = "\x02" "\x10" "fail" "\x11 ";
			break;
		case PRINT_OK:

			if (!developer.integer)
				return;  // NOTE: print msgs only in developer mode

			prefix = " " " ok " "  ";
			break;
		case PRINT_DBG:
			if (!developer.integer)
				return;  // NOTE: print msgs only in developer mode

			prefix = "";
			break;

		case PRINT_WARNING:
		case PRINT_ALL:
			prefix = ""; // FIXME: put here some color
			break;
		case PRINT_ERR_FATAL:
			prefix = "";
			break;
		default:
		case PRINT_INFO:
			prefix = "\x9c\x9c\x9c ";
			break;
	}

	va_start (argptr, fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end(argptr);
	Com_Printf ("%s%s", prefix, msg);

	if (state == PRINT_ERR_FATAL)
		Sys_Error(msg);
}

void Com_PrintVerticalBar(int width)
{
	int i;
	Com_Printf("\x80");

	for (i = 0; i < width; i++)
	{
		Com_Printf("\x81");
	}

	Com_Printf("\x82");
}

void COM_ParseIPCData(const char *buf, unsigned int bufsize)
{
	if (bufsize > 0)
	{
		// TODO : Expect some more fancy commands and stuff here instead.. if we want to use it for more than qw:// urls...
		if (!strncasecmp(buf, "qw://", 5))
		{
			Cbuf_AddText(va("qwurl %s\n", buf));
		}
		else
		{
			Cbuf_AddText(buf);
		}
	}
}

//
// Check if the first command line argument is a .qtv or a demo to be played.
//
qbool COM_CheckArgsForPlayableFiles(char *commandbuf_out, unsigned int commandbuf_size)
{
	// Check for .qtv or demo files to play.
	if (COM_Argc() >= 2) 
	{
		char *infile = COM_Argv(1);

		if (infile[0] && infile[0] != '-' && infile[0] != '+') 
		{
			char *ext = COM_FileExtension(infile);

			if (!strncasecmp(ext, "qtv", sizeof("qtv")))
			{
				snprintf(commandbuf_out, commandbuf_size, "qtvplay \"#%s\"\n", infile);
			}
			else if (!strncasecmp(ext, "qw", sizeof("qw")))
			{
				snprintf(commandbuf_out, commandbuf_size, "qtvplay \"#%s\"\n", infile);
			}
			else if (CL_IsDemoExtension(infile))
			{
				snprintf(commandbuf_out, commandbuf_size, "playdemo \"%s\"\n", infile);
			}
			else if (!strncasecmp(infile, "qw://", sizeof("qw://") - 1))
			{
				snprintf(commandbuf_out, commandbuf_size, "qwurl \"%s\"\n", infile);
			}
			else 
			{
				return false;
			}

			return true;
		}
	}

    return false;
}

//============================================================================

static char q_normalize_chartbl[256];
static qbool q_normalize_chartbl_init;

static void Q_normalizetext_Init (void)
{
	int i;

	for (i = 0; i < 32; i++)
		q_normalize_chartbl[i] = q_normalize_chartbl[i + 128] = '#';
	for (i = 32; i < 128; i++)
		q_normalize_chartbl[i] = q_normalize_chartbl[i + 128] = i;

	// special cases
	q_normalize_chartbl[10] = 10;
	q_normalize_chartbl[13] = 13;

	// dot
	q_normalize_chartbl[5      ] = q_normalize_chartbl[14      ] = q_normalize_chartbl[15      ] = q_normalize_chartbl[28      ] = q_normalize_chartbl[46      ] = '.';
	q_normalize_chartbl[5 + 128] = q_normalize_chartbl[14 + 128] = q_normalize_chartbl[15 + 128] = q_normalize_chartbl[28 + 128] = q_normalize_chartbl[46 + 128] = '.';

	// numbers
	for (i = 18; i < 28; i++)
		q_normalize_chartbl[i] = q_normalize_chartbl[i + 128] = i + 30;

	// brackets
	q_normalize_chartbl[16] = q_normalize_chartbl[16 + 128]= '[';
	q_normalize_chartbl[17] = q_normalize_chartbl[17 + 128] = ']';
	q_normalize_chartbl[29] = q_normalize_chartbl[29 + 128] = q_normalize_chartbl[128] = '(';
	q_normalize_chartbl[31] = q_normalize_chartbl[31 + 128] = q_normalize_chartbl[130] = ')';

	// left arrow
	q_normalize_chartbl[127] = '>';
	// right arrow
	q_normalize_chartbl[141] = '<';

	// '='
	q_normalize_chartbl[30] = q_normalize_chartbl[129] = q_normalize_chartbl[30 + 128] = '=';

	q_normalize_chartbl_init = true;
}

/*
==================
Q_normalizetext
returns readable extended quake names
==================
*/
char *Q_normalizetext (char *str)
{
	unsigned char	*i;

	if (!q_normalize_chartbl_init)
		Q_normalizetext_Init();

	for (i = (unsigned char*)str; *i; i++)
		*i = q_normalize_chartbl[*i];
	return str;
}

/*
==================
Q_redtext
returns extended quake names
==================
*/
unsigned char *Q_redtext (unsigned char *str)
{
	unsigned char *i;
	for (i = str; *i; i++)
		if (*i > 32 && *i < 128)
			*i |= 128;
	return str;
}
//<-

/*
==================
Q_yelltext
returns extended quake names (yellow numbers)
==================
*/
unsigned char *Q_yelltext (unsigned char *str)
{
	unsigned char *i;
	for (i = str; *i; i++)
	{
		if (*i >= '0' && *i <= '9')
			*i += 18 - '0';
		else if (*i > 32 && *i < 128)
			*i |= 128;
		else if (*i == 13)
			*i = ' ';
	}
	return str;
}

//=====================================================================

// "GPL map" support.  If we encounter a map with a known "GPL" CRC,
// we fake the CRC so that, on the client side, the CRC of the original
// map is transferred to the server, and on the server side, comparison
// of clients' CRC is done against the orignal one
typedef struct {
	const char *mapname;
	int original;
	int gpl;
} csentry_t;

static csentry_t table[] = {
	// CRCs for AquaShark's "simpletextures" maps
	{ "dm1",  0xc5c7dab3, 0x7d37618e },
	{ "dm2",  0x65f63634, 0x7b337440 },
	{ "dm3",  0x15e20df8, 0x912781ae },
	{ "dm4",  0x9c6fe4bf, 0xc374df89 },
	{ "dm5",  0xb02d48fd, 0x77ca7ce5 },
	{ "dm6",  0x5208da2b, 0x200c8b5d },
	{ "end",  0xbbd4b4a5, 0xf89b12ae }, // this is the version with the extra room
	{ "end",  0xbbd4b4a5, 0x924f4d33 }, // GPL end
	{ "e2m2", 0xaf961d4d, 0xa23126c5 }, // GPL e2m2
	{ NULL, 0, 0 },
};

int Com_TranslateMapChecksum (const char *mapname, int checksum)
{
	csentry_t *p;

//	Com_Printf ("Map checksum (%s): 0x%x\n", mapname, checksum);

	for (p = table; p->mapname; p++)
		if (!strcmp(p->mapname, mapname) && checksum == p->gpl)
			return p->original;

	return checksum;
}

float AdjustAngle(float current, float ideal, float fraction)
{
	float move = ideal - current;

	if (move >= 180)
		move -= 360;
	else if (move <= -180)
		move += 360;

	return current + fraction * move;
}

int Q_namecmp(const char* s1, const char* s2)
{
	if (s1 == NULL && s2 == NULL)
		return 0;

	if (s1 == NULL)
		return -1;

	if (s2 == NULL)
		return 1;

	while (*s1 || *s2) {
		if (tolower(*s1 & 0x7f) != tolower(*s2 & 0x7f)) {
			return tolower(*s1 & 0x7f) - tolower(*s2 & 0x7f);
		}
		s1++;
		s2++;
	}

	return 0;
}
