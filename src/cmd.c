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

    $Id: cmd.c,v 1.92 2007-10-28 02:45:19 qqshka Exp $
*/

#ifndef _WIN32
#include <strings.h>
#endif

#include "quakedef.h"
#include "gl_model.h"
#include "teamplay.h"
#include "rulesets.h"
#include "tp_triggers.h"
#include "parser.h"
#include "utils.h"
#include "keys.h"
#include <pcre2.h>

typedef struct {
	char name[MAX_MACRO_NAME];
	char *(*func) (void);
	int teamplay;
} macro_command_t;

#define MACRO_DEF(x) { #x, NULL, 0 }

static macro_command_t macro_commands[num_macros] = {
#include "macro_ids.h"
};

#undef MACRO_DEF

qbool CL_CheckServerCommand (void);

static void Cmd_ExecuteStringEx (cbuf_t *context, char *text);
static int gtf = 0; // global trigger flag

cvar_t cl_warncmd = {"cl_warncmd", "1"};

cvar_t cl_warnexec = {"cl_warnexec", "1"};
cvar_t cl_curlybraces = {"cl_curlybraces", "0"};

cbuf_t cbuf_main;
cbuf_t cbuf_svc;
cbuf_t cbuf_safe, cbuf_formatted_comms;
cbuf_t cbuf_server;

cbuf_t *cbuf_current = NULL;

//=============================================================================

//Causes execution of the remainder of the command buffer to be delayed until next frame.
//This allows commands like: bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
void Cmd_Wait_f (void)
{
	if (cbuf_current)
		cbuf_current->wait = true;

	return;
}

// copies the first argument to clipboard
void Cmd_Clipboard_f(void) {
	CopyToClipboard(Cmd_Args());
}

/*
=============================================================================
						COMMAND BUFFER
=============================================================================
*/

void Cbuf_AddText (const char *text)
{
	Cbuf_AddTextEx (&cbuf_main, text);
}

void Cbuf_InsertText (const char *text)
{
	Cbuf_InsertTextEx (&cbuf_main, text);
}

void Cbuf_Execute (void)
{
	Cbuf_ExecuteEx (&cbuf_main);
	Cbuf_ExecuteEx (&cbuf_safe);
	Cbuf_ExecuteEx (&cbuf_formatted_comms);
	Cbuf_ExecuteEx (&cbuf_server);
}

void Cbuf_Flush(cbuf_t* cbuf)
{
	while (cbuf->text_end > cbuf->text_start)
		Cbuf_ExecuteEx(cbuf);
}

//fuh : ideally we should have 'cbuf_t *Cbuf_Register(int maxsize, int flags, qbool (*blockcmd)(void))
//fuh : so that cbuf_svc and cbuf_safe can be registered outside cmd.c in cl_* .c
//fuh : flags can be used to deal with newline termination etc for cbuf_svc, and *blockcmd can be used for blocking cmd's for cbuf_svc
//fuh : this way cmd.c would be independant of '#ifdef CLIENTONLY's'.
//fuh : I'll take care of that one day.
static void Cbuf_Register (cbuf_t *cbuf, int maxsize)
{
	assert (!host_initialized);
	cbuf->maxsize = maxsize;
	cbuf->text_buf = (char *) Hunk_Alloc(maxsize);
	cbuf->text_start = cbuf->text_end = (cbuf->maxsize >> 1);
	cbuf->wait = false;
	cbuf->waitCount = 0;
}

void Cbuf_Init (void)
{
	Cbuf_Register (&cbuf_main, 1 << 18); // 256kb
	Cbuf_Register (&cbuf_svc, 1 << 13); // 8kb
	Cbuf_Register (&cbuf_safe, 1 << 11); // 2kb
	Cbuf_Register (&cbuf_formatted_comms, 1 << 11); // 2kb
	Cbuf_Register (&cbuf_server, 1 << 18); // 256kb
}

//Adds command text at the end of the buffer
void Cbuf_AddTextEx (cbuf_t *cbuf, const char *text)
{
	size_t new_start, new_bufsize;
	size_t len;

	len = strlen (text);

	if (cbuf->text_end + len <= cbuf->maxsize) {
		memcpy (cbuf->text_buf + cbuf->text_end, text, len);
		cbuf->text_end += len;
		return;
	}

	new_bufsize = cbuf->text_end-cbuf->text_start+len;
	if (new_bufsize > cbuf->maxsize) {
		Com_Printf ("Cbuf_AddText: overflow\n");
		return;
	}

	// Calculate optimal position of text in buffer
	new_start = ((cbuf->maxsize - new_bufsize) >> 1);

	memcpy (cbuf->text_buf + new_start, cbuf->text_buf + cbuf->text_start, cbuf->text_end-cbuf->text_start);
	memcpy (cbuf->text_buf + new_start + cbuf->text_end-cbuf->text_start, text, len);
	cbuf->text_start = new_start;
	cbuf->text_end = cbuf->text_start + new_bufsize;
}

//Adds command text at the beginning of the buffer
void Cbuf_InsertTextEx (cbuf_t *cbuf, const char *text)
{
	size_t new_start, new_bufsize;
	size_t len;

	len = strlen (text);

	if (len <= cbuf->text_start) {
		memcpy (cbuf->text_buf + (cbuf->text_start - len), text, len);
		cbuf->text_start -= len;
		return;
	}

	new_bufsize = cbuf->text_end - cbuf->text_start + len;
	if (new_bufsize > cbuf->maxsize) {
		Com_Printf ("Cbuf_InsertText: overflow\n");
		return;
	}

	// Calculate optimal position of text in buffer
	new_start = ((cbuf->maxsize - new_bufsize) >> 1);

	memmove (cbuf->text_buf + (new_start + len), cbuf->text_buf + cbuf->text_start, cbuf->text_end - cbuf->text_start);
	memcpy (cbuf->text_buf + new_start, text, len);
	cbuf->text_start = new_start;
	cbuf->text_end = cbuf->text_start + new_bufsize;
}

#define MAX_RUNAWAYLOOP 1000

void Cbuf_ExecuteEx (cbuf_t *cbuf)
{
	size_t i, j;
	size_t cursize, nextsize;
	char *text, line[1024], *src, *dest;
	qbool comment;
	int quotes;

	if (cbuf == &cbuf_safe)
		gtf++;

	nextsize = cbuf->text_end - cbuf->text_start;

	while (cbuf->text_end > cbuf->text_start)
	{
		// find a \n or ; line break
		text = (char *) cbuf->text_buf + cbuf->text_start;

		cursize = cbuf->text_end - cbuf->text_start;
		comment = false;
		quotes = 0;

		for (i = 0; i < cursize; i++)
		{
			if (cl_curlybraces.integer)
			{
				if (text[i] == '\\')
				{
					if (i + 1 < cursize && text[i+1] == '\n')
					{ // escaped endline
						text[i] = text[i+1] = '\r'; // '\r' removed later during copying
						i++;
						continue;
					}
					else if (i + 2 < cursize && text[i+1] == '\r' && text[i+2] == '\n')
					{ // escaped dos endline
						text[i] = text[i+2] = '\r';
						i+=2;
						continue;
					}
				}
			}

			if (text[i] == '\n')
				break;

			if (text[i] == '"' && quotes <= 0)
			{
				if (!quotes)
					quotes = -1;
				else
					quotes = 0;
			}
			else if (quotes >= 0)
			{
				if (cl_curlybraces.integer)
				{
					if (text[i] == '{')
						quotes++;
					else if (text[i] == '}')
						quotes--;
				}
			}

			if (comment || quotes)
				continue;

			if (text[i] == '/' && i + 1 < cursize && text[i + 1] == '/')
				comment = true;
			else if (text[i] == ';' && !quotes)
				break;
		}

		if ((cursize - i) < nextsize) // have we reached the next command?
			nextsize = cursize - i;

		// don't execute lines without ending \n; this fixes problems with
		// partially stuffed aliases not being executed properly

		if (cbuf_current == &cbuf_svc && i == cursize)
			break;

		// Copy text to line, skipping carriage return chars
		src = text;
		dest = line;
		j = min (i, sizeof (line) - 1);
		for ( ; j; j--, src++)
		{
			if (*src != '\r')
				*dest++ = *src;
		}
		*dest = 0;

		// delete the text from the command buffer and move remaining commands down  This is necessary
		// because commands (exec, alias) can insert data at the beginning of the text buffer
		if (i == cursize)
		{
			cbuf->text_start = cbuf->text_end = (cbuf->maxsize >> 1);
		}
		else
		{
			i++;
			cbuf->text_start += i;
		}

		cursize = cbuf->text_end - cbuf->text_start;

		// TODO: make it in a more right way
		// since, hud262_add can not correctly create hud elements during normal start
		// (some cvars are not created/initialized at the time when we want to use them in hud262)
		// we should save these commands to buffer and execute it when all
		// cvars will be created
		if (!host_initialized) {
			Hud_262CatchStringsOnLoad(line);
		}

		Cmd_ExecuteStringEx (cbuf, line);	// execute the command line

		if (cbuf->text_end - cbuf->text_start > cursize)
			cbuf->runAwayLoop++;

		if (cbuf->runAwayLoop > MAX_RUNAWAYLOOP)
		{
			Com_Printf("\x02" "A recursive alias has caused an infinite loop.");
			Com_Printf("\x02" " Clearing execution buffer to prevent lockup.\n");
			cbuf->text_start = cbuf->text_end = (cbuf->maxsize >> 1);
			cbuf->runAwayLoop = 0;
		}

		if (cbuf->wait && cbuf->waitCount >= Rulesets_MaxSequentialWaitCommands())
		{
			Com_Printf("\x02" "Max number of wait commands detected.\n");
			cbuf->text_start = cbuf->text_end = (cbuf->maxsize >> 1);
			cbuf->wait = false;
			cbuf->waitCount = 0;
		}

		if (cbuf->wait)
		{
			// skip out while text still remains in buffer, leaving it for next frame
			cbuf->wait = false;
			++cbuf->waitCount;

			cbuf->runAwayLoop += Q_rint (0.5 * cls.frametime * MAX_RUNAWAYLOOP);

			if (cbuf == &cbuf_safe)
				gtf--;
			return;
		}
	}

	if (cbuf == &cbuf_safe)
		gtf--;

	cbuf->runAwayLoop = 0;
	cbuf->waitCount = 0;

	return;
}

/*
==============================================================================
						SCRIPT COMMANDS
==============================================================================
*/

/*
Set commands are added early, so they are guaranteed to be set before
the client and server initialize for the first time.

Other commands are added late, after all initialization is complete.
*/
void Cbuf_AddEarlyCommands (void)
{
	int i;

	for (i = 0; i < COM_Argc () - 2; i++) {
		if (strcasecmp (COM_Argv(i), "+set"))
			continue;

		Cbuf_AddText (va ("set %s %s\n", COM_Argv (i + 1), COM_Argv (i + 2)));
		i += 2;
	}
}

qbool Cmd_IsAllowedStuffCmdsCommand(const char *str)
{
	char*	banned_list[] = {"set ", "cfg_load ", NULL};
	char**	banned_cmd = banned_list;

	while(*banned_cmd)
	{
		if(strncasecmp(str, *banned_cmd, strlen(*banned_cmd)) == 0)
		{
			//+set is processed in Cbuf_AddEarlyCommands(), +cfg_load allowed only once in Host_Init()
			if((strncasecmp(str, "set ", 4) != 0) && (strncasecmp(str, "cfg_load ", 9) != 0))
			{
				Com_Printf("+%s is not allowed in cmdline or obsolete.\n", *banned_cmd);
			}
			return false;
		}
		banned_cmd++;
	}
	return true;
}

/*
Adds command line parameters as script statements
Commands lead with a +, and continue until a - or another +
quake +prog jctest.qp +cmd amlev1
quake -nosound +cmd amlev1
*/
void Cmd_StuffCmds_f (void)
{
	int k, len = 0;
	char *s, *text, *token;

	// build the combined string to parse from
	for (k = 1; k < COM_Argc(); k++)
		len += strlen (COM_Argv(k)) + 1;

	if (!len)
		return;

	text = (char *) Q_malloc(len + 1);
	text[0] = '\0';
	for (k = 1; k < COM_Argc(); k++) {
		strlcat (text, COM_Argv(k), len + 1);
		if (k != COM_Argc() - 1)
			strlcat (text, " ", len + 1);
	}

	// pull out the commands
	token = (char *) Q_malloc(len + 1);

	s = text;
	while (*s) {
		if (*s == '+')	{
			k = 0;
			for (s = s + 1; s[0] && (s[0] != ' ' || (s[1] != '-' && s[1] != '+')); s++)
				token[k++] = s[0];
			token[k++] = '\n';
			token[k] = 0;
			if(Cmd_IsAllowedStuffCmdsCommand(token))
				Cbuf_AddText (token);
		} else if (*s == '-') {
			for (s = s + 1; s[0] && s[0] != ' '; s++)
				;
		} else {
			s++;
		}
	}

	Q_free(text);
	Q_free (token);
}

void Cmd_Exec_f (void)
{
	char *f, name[MAX_OSPATH];
	char reset_bindphysical[128];
	qbool server_command = false;

	if (Cmd_Argc () != 2) {
		Com_Printf ("%s <filename> : execute a script file\n", Cmd_Argv(0));
		return;
	}

#if !defined(SERVERONLY) && !defined(CLIENTONLY)
	server_command = cbuf_current == &cbuf_server || !strcmp(Cmd_Argv(0), "serverexec");
#endif

	strlcpy (name, Cmd_Argv(1), sizeof(name) - 4);
	if (!(f = (char *) FS_LoadHeapFile(name, NULL)))	{
		const char *p;
		p = COM_SkipPath (name);
		if (!strchr (p, '.')) {
			// no extension, so try the default (.cfg)
			strlcat (name, ".cfg", sizeof (name));
			f = (char *)FS_LoadHeapFile(name, NULL);
		}
		if (!f) {
			Com_Printf ("couldn't exec %s\n", Cmd_Argv(1));
			return;
		}
	}
	if (cl_warnexec.integer || developer.integer) {
		Com_Printf("execing %s/%s\n", FS_Locate_GetPath(name), name);
	}

	// All config files default to con_bindphysical 1, and would have to over-ride if they
	//   want different behaviour.
	sprintf(reset_bindphysical, "\ncon_bindphysical %d\n", con_bindphysical.integer);
	if (cbuf_current == &cbuf_svc) {
		Cbuf_AddTextEx(&cbuf_main, "con_bindphysical 1\n");
		Cbuf_AddTextEx(&cbuf_main, f);
		Cbuf_AddTextEx(&cbuf_main, reset_bindphysical);
	}
	else if (server_command) {
		Cbuf_AddTextEx(&cbuf_server, f);
	}
	else {
		Cbuf_InsertTextEx(&cbuf_main, reset_bindphysical);
		Cbuf_InsertTextEx(&cbuf_main, f);
		Cbuf_InsertTextEx(&cbuf_main, "con_bindphysical 1\n");
	}
	
	Q_free(f);
}

//Just prints the rest of the line to the console
/*void Cmd_Echo_f (void) {
	int i;

	for (i = 1; i < Cmd_Argc(); i++)
		Com_Printf ("%s ", Cmd_Argv(i));
	Com_Printf ("\n");
}*/
void Cmd_Echo_f (void)
{
	int	i;
	char *str;
	char args[MAX_MACRO_STRING];
	char buf[MAX_MACRO_STRING];

	memset (args, 0, MAX_MACRO_STRING);

	snprintf (args, MAX_MACRO_STRING, "%s", Cmd_Argv(1));

	for (i = 2; i < Cmd_Argc(); i++) {
		strlcat (args, " ", MAX_MACRO_STRING);
		strlcat (args, Cmd_Argv(i), MAX_MACRO_STRING);
	}

	//	str = TP_ParseMacroString(args);

	str = TP_ParseMacroString(args);
	str = TP_ParseFunChars(str, false);

	strlcpy (buf, str, MAX_MACRO_STRING);

	CL_SearchForReTriggers (buf, RE_PRINT_ECHO); 	// BorisU
	Print_flags[Print_current] |= PR_TR_SKIP;
	Com_Printf ("%s\n", buf);
}

/*
=============================================================================
								ALIASES
=============================================================================
*/
#define ALIAS_HASHPOOL_SIZE 256
cmd_alias_t *cmd_alias_hash[ALIAS_HASHPOOL_SIZE];
cmd_alias_t	*cmd_alias;

cmd_alias_t *Cmd_FindAlias (const char *name)
{
	int key;
	cmd_alias_t *alias;

	key = Com_HashKey (name) % ALIAS_HASHPOOL_SIZE;
	for (alias = cmd_alias_hash[key]; alias; alias = alias->hash_next) {
		if (!strcasecmp(name, alias->name))
			return alias;
	}
	return NULL;
}

char *Cmd_AliasString (char *name)
{
	int key;
	cmd_alias_t *alias;

	key = Com_HashKey (name) % ALIAS_HASHPOOL_SIZE;
	for (alias = cmd_alias_hash[key]; alias; alias = alias->hash_next) {
		if (!strcasecmp(name, alias->name)) {
			return alias->value;
		}
	}
	return NULL;
}

void Cmd_Viewalias_f (void)
{
	cmd_alias_t	*alias;
	char		*name;
	int		i,m;

	if (Cmd_Argc() < 2) {
		Com_Printf ("viewalias <cvar> [<cvar2>..] : view body of alias\n");
		return;
	}

	for (i=1; i<Cmd_Argc(); i++) {
		name = Cmd_Argv(i);

		if (IsRegexp(name)) {
			if (!ReSearchInitEx(name, false)) {
				return;
			}
			Com_Printf ("Current alias commands:\n");

			for (alias = cmd_alias, i = m = 0; alias; alias = alias->next, i++) {
				if (ReSearchMatch(alias->name)) {
					Com_Printf("%s : %s\n", alias->name, alias->value);
					m++;
				}
			}

			Com_Printf ("------------\n%i/%i aliases\n", m, i);
			ReSearchDone();
		}
		else if ((alias = Cmd_FindAlias(name))) {
			Com_Printf("%s : \"%s\"\n", Cmd_Argv(i), alias->value);
		}
		else {
			Com_Printf("No such alias: %s\n", Cmd_Argv(i));
		}
	}
}


int Cmd_AliasCompare (const void *p1, const void *p2)
{
	cmd_alias_t *a1, *a2;

	a1 = *((cmd_alias_t **) p1);
	a2 = *((cmd_alias_t **) p2);

	if (a1->name[0] == '+') {
		if (a2->name[0] == '+')
			return strcasecmp(a1->name + 1, a2->name + 1);
		else
			return -1;
	} else if (a1->name[0] == '-') {
		if (a2->name[0] == '+')
			return 1;
		else if (a2->name[0] == '-')
			return strcasecmp(a1->name + 1, a2->name + 1);
		else
			return -1;
	} else if (a2->name[0] == '+' || a2->name[0] == '-') {
		return 1;
	} else {
		return strcasecmp(a1->name, a2->name);
	}
}

void Cmd_AliasList_f (void)
{
	cmd_alias_t *a;
	int i, c, m = 0;
	static int count;
	static cmd_alias_t *sorted_aliases[4096];

#define MAX_SORTED_ALIASES (sizeof(sorted_aliases) / sizeof(sorted_aliases[0]))

	for (a = cmd_alias, count = 0; a && count < MAX_SORTED_ALIASES; a = a->next, count++)
		sorted_aliases[count] = a;
	qsort(sorted_aliases, count, sizeof (cmd_alias_t *), Cmd_AliasCompare);

	if (count == MAX_SORTED_ALIASES)
		assert(!"count == MAX_SORTED_ALIASES");

	c = Cmd_Argc();
	if (c > 1 && !ReSearchInitEx(Cmd_Argv(1), false)) {
		return;
	}

	Com_Printf ("List of aliases:\n");
	for (i = 0; i < count; i++) {
		a = sorted_aliases[i];
		if (c==1 || ReSearchMatch(a->name)) {
			Com_Printf ("\x02%s :", sorted_aliases[i]->name);
			Com_Printf (" %s\n", sorted_aliases[i]->value);
			m++;
		}
	}

	if (c>1)
		ReSearchDone();
	Com_Printf ("------------\n%i/%i aliases\n", m, count);
}

void Cmd_AliasEdit_f (void)
{
	cmd_alias_t	*a;
	char *s, *v, final_string[MAXCMDLINE - 1];
	int c;

	c = Cmd_Argc();
	if (c == 1)	{
		Com_Printf ("%s <name> : modify an alias\n", Cmd_Argv(0));
		Com_Printf ("aliaslist : list all aliases\n");
		return;
	}

	s = Cmd_Argv(1);
	if (s[0] == '\0') {
		Com_Printf("Alias name must be specified\n");
		return;
	} else if(strlen(s) >= MAX_ALIAS_NAME) {
		Com_Printf("Alias name is too long\n");
		return;
	}

	a = Cmd_FindAlias(s);
	v = (a ? a->value : "");

	strlcpy(final_string, "/alias \"", sizeof(final_string));
	strlcat(final_string, s, sizeof(final_string));
	strlcat(final_string, "\" \"", sizeof(final_string));
	strlcat(final_string, v, sizeof(final_string));
	strlcat(final_string, "\"", sizeof(final_string));
	Key_ClearTyping();
	key_linepos = 9 + (int)strlen(s) + 3; // move to where the commands are in the alias
	memcpy(key_lines[edit_line]+1, str2wcs(final_string), (strlen(final_string) + 1) * sizeof(wchar));
}

static cmd_alias_t* Cmd_AliasCreate (char* name)
{
	cmd_alias_t	*a;
	int key;

	key = Com_HashKey(name) % ALIAS_HASHPOOL_SIZE;

	a = (cmd_alias_t *) Q_malloc(sizeof(cmd_alias_t));
	a->next = cmd_alias;
	cmd_alias = a;
	a->hash_next = cmd_alias_hash[key];
	cmd_alias_hash[key] = a;

	strlcpy (a->name, name, sizeof (a->name));
	return a;
}

//Creates a new command that executes a command string (possibly ; separated)
void Cmd_Alias_f (void)
{
	cmd_alias_t	*a;
	char *s;
	int c, key;

	c = Cmd_Argc();
	if (c == 1)	{
		Com_Printf("%s <name> : show alias content\n", Cmd_Argv(0));
		Com_Printf("%s <name> <command> : create or modify an alias\n", Cmd_Argv(0));
		Com_Printf("use aliaslist to search for aliases\n");
		return;
	}

	s = Cmd_Argv(1);
	if (strlen(s) >= MAX_ALIAS_NAME) {
		Com_Printf ("Alias name is too long\n");
		return;
	}
	else if (s[0] == '\0') {
		Com_Printf("Alias name must be specified\n");
		return;
	}

	key = Com_HashKey(s) % ALIAS_HASHPOOL_SIZE;

	// if the alias already exists, reuse it
	for (a = cmd_alias_hash[key]; a; a = a->hash_next) {
		if (!strcasecmp(a->name, s)) {
			if (Cmd_Argc() == 2) {
				Com_Printf("\x02%s :", a->name);
				Com_Printf(" %s\n", a->value);
				return;
			}
			Q_free(a->value);
			break;
		}
	}

	if (!a)	{
		a = (cmd_alias_t *) Q_malloc(sizeof(cmd_alias_t));
		a->next = cmd_alias;
		cmd_alias = a;
		a->hash_next = cmd_alias_hash[key];
		cmd_alias_hash[key] = a;
	}

	strlcpy (a->name, s, sizeof (a->name));

	a->flags = 0;
	// QW262 -->
	s=Cmd_MakeArgs(2);
	while (*s) {
		if (*s == '%' && ( s[1]>='0' || s[1]<='9')) {
			a->flags |= ALIAS_HAS_PARAMETERS;
			break;
		}
		++s;
	}
	// <-- QW262

	if (cbuf_current == &cbuf_svc)
		a->flags |= ALIAS_SERVER;
	if (!strcasecmp(Cmd_Argv(0), "tempalias"))
		a->flags |= ALIAS_TEMP;

	// copy the rest of the command line
	a->value = Q_strdup(Cmd_MakeArgs(2));
}

qbool Cmd_DeleteAlias (char *name)
{
	cmd_alias_t *a, *prev;
	int key;

	key = Com_HashKey (name) % ALIAS_HASHPOOL_SIZE;

	prev = NULL;
	for (a = cmd_alias_hash[key]; a; a = a->hash_next) {
		if (!strcasecmp(a->name, name)) {
			// unlink from hash
			if (prev)
				prev->hash_next = a->hash_next;
			else
				cmd_alias_hash[key] = a->hash_next;
			break;
		}
		prev = a;
	}

	if (!a)
		return false;	// not found

	prev = NULL;
	for (a = cmd_alias; a; a = a->next) {
		if (!strcasecmp(a->name, name)) {
			// unlink from alias list
			if (prev)
				prev->next = a->next;
			else
				cmd_alias = a->next;

			// free
			Q_free(a->value);
			Q_free(a);
			return true;
		}
		prev = a;
	}

	assert(!"Cmd_DeleteAlias: alias list broken");
	return false; // shut up compiler
}

void Cmd_UnAlias (qbool use_regex)
{
	int 		i;
	char		*name;
	cmd_alias_t	*a, *next;
	qbool		re_search = false;

	if (Cmd_Argc() < 2) {
		Com_Printf ("unalias <cvar> [<cvar2>..]: erase an existing alias\n");
		return;
	}

	for (i=1; i<Cmd_Argc(); i++) {
		name = Cmd_Argv(i);

		if (use_regex && (re_search = IsRegexp(name))) {
			if (!ReSearchInitEx(name, false)) {
				continue;
			}
		}

		if (strlen(name) >= MAX_ALIAS_NAME) {
			Com_Printf ("Alias name is too long: \"%s\"\n", Cmd_Argv(i));
			continue;
		}

		if (use_regex && re_search) {
			for (a = cmd_alias; a; a = next) {
				next = a->next;

				if (ReSearchMatch(a->name))
					Cmd_DeleteAlias(a->name);
			}
		} else {
			if (!Cmd_DeleteAlias(Cmd_Argv(i)))
				Com_Printf ("unalias: unknown alias \"%s\"\n", Cmd_Argv(i));
		}

		if (use_regex && re_search)
			ReSearchDone();

	}
}

void Cmd_UnAlias_f (void)
{
	Cmd_UnAlias(false);
}

void Cmd_UnAlias_re_f (void)
{
	Cmd_UnAlias(true);
}

/* 
 * Remove all aliases unless connected, then remove
 * all aliases except the server created aliases
 */
void Cmd_UnAliasAll_f (void)
{
	cmd_alias_t	*a, *next;

/* FIXME: Optimize this, its n^2 slow atm since Cmd_DeleteAlias will loop through
 * the list again for each entry
 */
	if (cls.state >= ca_connected) {
		Com_Printf("Connected to a server, will not remove server aliases\n");
		for (a = cmd_alias; a; a = next) {
			next = a->next;
			if ((a->flags & ALIAS_SERVER) == 0) {
				Cmd_DeleteAlias(a->name);
			}
		}
	} else {
		for (a = cmd_alias; a ; a = next) {
			next = a->next;
			Q_free(a->value);
			Q_free(a);
		}
		cmd_alias = NULL;

		// clear hash
		memset (cmd_alias_hash, 0, sizeof(cmd_alias_t*) * ALIAS_HASHPOOL_SIZE);
	}
}

void DeleteServerAliases(void)
{
	cmd_alias_t *a, *next;

	for (a = cmd_alias; a; a = next) {
		next = a->next;

		if (a->flags & ALIAS_SERVER)
			Cmd_DeleteAlias (a->name);
	}
}

/*
=============================================================================
					LEGACY COMMANDS
=============================================================================
*/

legacycmd_t *legacycmds = NULL;

void Cmd_AddLegacyCommand(char *oldname, char *newname)
{
	legacycmd_t *cmd;
	cmd = (legacycmd_t *) Q_malloc(sizeof(legacycmd_t));
	cmd->next = legacycmds;
	legacycmds = cmd;

	cmd->oldname = oldname;
	cmd->newname = newname;
	cmd->dummy_cmd.name = oldname;
}

qbool Cmd_IsLegacyCommand (char *oldname)
{
	legacycmd_t *cmd;

	for (cmd = legacycmds; cmd; cmd = cmd->next) {
		if (!strcasecmp(cmd->oldname, oldname))
			return true;
	}
	return false;
}

static qbool Cmd_LegacyCommand (void)
{
	static qbool recursive = false;
	legacycmd_t *cmd;
	char text[1024];

	for (cmd = legacycmds; cmd; cmd = cmd->next) {
		if (!strcasecmp(cmd->oldname, Cmd_Argv(0)))
			break;
	}
	if (!cmd)
		return false;

	if (!cmd->newname[0])
		return true;		// just ignore this command

	// build new command string
	strlcpy(text, cmd->newname, sizeof(text));
	strlcat(text, " ", sizeof(text));
	strlcat(text, Cmd_Args(), sizeof(text));

	if (recursive) {
		Com_Printf("error: recursive legacy command, aborting");
		recursive = false;
		return false;
	}

	recursive = true;
	Cmd_ExecuteString(text);
	recursive = false;

	return true;
}

/*
=============================================================================
					COMMAND EXECUTION
=============================================================================
*/

#define CMD_HASHPOOL_SIZE 512
cmd_function_t	*cmd_hash_array[CMD_HASHPOOL_SIZE];
/*static*/ cmd_function_t	*cmd_functions;		// possible commands to execute

static  tokenizecontext_t cmd_tokenizecontext;

static	char	*cmd_null_string = "";

int Cmd_ArgcEx (tokenizecontext_t *ctx)
{
	return ctx->cmd_argc;
}

char *Cmd_ArgvEx (tokenizecontext_t *ctx, int arg)
{
	if (arg >= ctx->cmd_argc || arg < 0)
		return cmd_null_string;

	return ctx->cmd_argv[arg];
}

// Returns a single string containing argv(1) to argv(argc() - 1)
char *Cmd_ArgsEx (tokenizecontext_t *ctx)
{
	return ctx->cmd_args;
}

// Returns a single string containing argv(start) to argv(argc() - 1)
// Unlike Cmd_Args, shrinks spaces between argvs
char *Cmd_MakeArgsEx (tokenizecontext_t *ctx, int start)
{
	int i, c;

	ctx->text[0] = 0;
	c = Cmd_ArgcEx(ctx);

	for (i = start; i < c; i++)
	{
		if (i > start)
			strlcat (ctx->text, " ", sizeof (ctx->text) - strlen (ctx->text));

		strlcat (ctx->text, Cmd_ArgvEx(ctx, i), sizeof (ctx->text) - strlen (ctx->text));
	}

	return ctx->text;
}

// Parses the given string into command line tokens.
void Cmd_TokenizeStringEx2(tokenizecontext_t *ctx, const char *text, qbool curlybraces)
{
	int idx = 0, token_len;

	memset(ctx, 0, sizeof(*ctx));

	while (1)
	{
		// skip whitespace
		while (*text == ' ' || *text == '\t' || *text == '\r')
			text++;

		// a newline separates commands in the buffer
		if (*text == '\n')
			return;

		if (!*text)
			return;

		if (ctx->cmd_argc == 1)
			strlcpy(ctx->cmd_args, text, sizeof(ctx->cmd_args));

		text = COM_ParseEx(text, curlybraces);
		if (!text)
			return;

		if (ctx->cmd_argc >= MAX_ARGS)
			return;

		token_len = strlen(com_token);

		// ouch ouch, no more space
		if (idx + token_len + 1 > sizeof(ctx->argv_buf))
			return;

		ctx->cmd_argv[ctx->cmd_argc] = ctx->argv_buf + idx;
		strcpy (ctx->cmd_argv[ctx->cmd_argc], com_token);
		ctx->cmd_argc++;

		idx += token_len + 1;
	}
}

void Cmd_TokenizeStringEx(tokenizecontext_t* ctx, const char* text)
{
	Cmd_TokenizeStringEx2(ctx, text, false);
}

// and wrappers for backward compatibility

int Cmd_Argc (void)
{
	return Cmd_ArgcEx(&cmd_tokenizecontext);
}

char *Cmd_Argv (int arg)
{
	return Cmd_ArgvEx(&cmd_tokenizecontext, arg);
}

//Returns a single string containing argv(1) to argv(argc() - 1)
char *Cmd_Args (void)
{
	return Cmd_ArgsEx(&cmd_tokenizecontext);
}

//Returns a single string containing argv(start) to argv(argc() - 1)
//Unlike Cmd_Args, shrinks spaces between argvs
char *Cmd_MakeArgs (int start)
{
	return Cmd_MakeArgsEx(&cmd_tokenizecontext, start);
}

//Parses the given string into command line tokens.
void Cmd_TokenizeString (const char *text)
{
	Cmd_TokenizeStringEx(&cmd_tokenizecontext, text);
}

// save cmd_tokenizecontext struct to ctx
void Cmd_SaveContext(tokenizecontext_t *ctx)
{
	ctx[0] = cmd_tokenizecontext;
}

// restore cmd_tokenizecontext struct from ctx
void Cmd_RestoreContext(tokenizecontext_t *ctx)
{
	cmd_tokenizecontext = ctx[0];
}

void Cmd_AddCommand (char *cmd_name, xcommand_t function)
{
	cmd_function_t *cmd;
	int	key;

	/* commented out when vid_restart was added
	if (host_initialized)	// because hunk allocation would get stomped
		assert (!"Cmd_AddCommand after host_initialized");
	*/

/*	// fail if the command is a variable name
	if (Cvar_Find(cmd_name)) {
		Com_Printf ("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
		return;
	} */

	key = Com_HashKey (cmd_name) % CMD_HASHPOOL_SIZE;

	// fail if the command already exists
	for (cmd = cmd_hash_array[key]; cmd; cmd=cmd->hash_next) {
		if (!strcasecmp (cmd_name, cmd->name)) {
			Com_Printf ("Cmd_AddCommand: %s already defined\n", cmd_name);
			return;
		}
	}

	cmd = (cmd_function_t *) Hunk_Alloc (sizeof(cmd_function_t));
	cmd->name = cmd_name;
	cmd->function = function;
	cmd->zmalloced = false;
	cmd->next = cmd_functions;
	cmd_functions = cmd;
	cmd->hash_next = cmd_hash_array[key];
	cmd_hash_array[key] = cmd;
}

qbool Cmd_AddRemCommand (char *cmd_name, xcommand_t function)
{
	cmd_function_t *cmd;
	int	key;

	key = Com_HashKey (cmd_name) % CMD_HASHPOOL_SIZE;

	// fail if the command already exists
	for (cmd = cmd_hash_array[key]; cmd; cmd=cmd->hash_next) {
		if (!strcasecmp (cmd_name, cmd->name)) {
			Com_Printf ("Cmd_AddCommand: %s already defined\n", cmd_name);
			return false;
		}
	}

	cmd = (cmd_function_t*)Q_malloc(sizeof(cmd_function_t)+strlen(cmd_name)+1);
	cmd->name = (char*)(cmd+1); // points to extra space created after the structure
	strcpy(cmd->name, cmd_name);
	cmd->function = function;
	cmd->zmalloced = true;
	cmd->next = cmd_functions;
	cmd_functions = cmd;
	cmd->hash_next = cmd_hash_array[key];
	cmd_hash_array[key] = cmd;

	return true;
}

// removes command from the hash map of the commands
cmd_function_t *Cmd_RemoveCommand_Hash(char *cmd_name)
{
	int key = Com_HashKey (cmd_name) % CMD_HASHPOOL_SIZE;
	cmd_function_t *cmd = cmd_hash_array[key];
	cmd_function_t *prev = NULL;
	cmd_function_t *retval = NULL;

	if (strcasecmp(cmd_name, cmd->name) == 0) {
		retval = cmd;
		cmd_hash_array[key] = cmd_hash_array[key]->hash_next;
	}
	else
	{
		prev = cmd;
		for (cmd = cmd->hash_next; cmd; cmd = cmd->hash_next) {
			if (strcasecmp(cmd_name, cmd->name) == 0) {
				retval = cmd;
				prev->hash_next = cmd->hash_next;
			}
		}
	}

	return retval;
}

// removes command from the linked list of all commands
cmd_function_t *Cmd_RemoveCommand_List (char *cmd_name)
{
	cmd_function_t	*cmd, **back;

	back = &cmd_functions;
	while (1)
	{
		cmd = *back;
		if (!cmd)
		{
			return NULL;
		}
		if (!strcmp (cmd_name, cmd->name))
		{
			*back = cmd->next;
			return cmd;
		}
		back = &cmd->next;
	}
}

// removes command from all structures and deallocates it
void Cmd_RemoveCommand (char *cmd_name)
{
	cmd_function_t *cmd;

	cmd = Cmd_RemoveCommand_List(cmd_name);
	cmd = Cmd_RemoveCommand_Hash(cmd_name);

	if (cmd) {
		if (cmd->zmalloced)
		{
			Q_free(cmd);
		}
		else
		{
			Con_Printf("Cmd_RemoveCommand: %s was not added dynamically\n", cmd_name);
		}
	}
}

qbool Cmd_Exists (char *cmd_name)
{
	int	key;
	cmd_function_t	*cmd;

	key = Com_HashKey (cmd_name) % CMD_HASHPOOL_SIZE;
	for (cmd=cmd_hash_array[key]; cmd; cmd = cmd->hash_next) {
		if (!strcasecmp (cmd_name, cmd->name))
			return true;
	}
	return false;
}

cmd_function_t *Cmd_FindCommand (const char *cmd_name)
{
	int	key;
	cmd_function_t *cmd;

	key = Com_HashKey (cmd_name) % CMD_HASHPOOL_SIZE;
	for (cmd = cmd_hash_array[key]; cmd; cmd = cmd->hash_next) {
		if (!strcasecmp (cmd_name, cmd->name))
			return cmd;
	}
	return NULL;
}

char *Cmd_CompleteCommand (char *partial)
{
	cmd_function_t *cmd;
	int len;
	cmd_alias_t *alias;
	legacycmd_t* legacy_cmd;

	len = strlen(partial);

	if (!len)
		return NULL;

	// check for exact match
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strcasecmp (partial, cmd->name))
			return cmd->name;
	for (alias = cmd_alias; alias; alias = alias->next)
		if (!strcasecmp (partial, alias->name))
			return alias->name;
	for (legacy_cmd = legacycmds; legacy_cmd; legacy_cmd = legacy_cmd->next) {
		if (!strcasecmp(partial, legacy_cmd->oldname)) {
			return legacy_cmd->oldname;
		}
	}

	// check for partial match
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strncasecmp (partial, cmd->name, len))
			return cmd->name;
	for (alias = cmd_alias; alias; alias = alias->next)
		if (!strncasecmp (partial, alias->name, len))
			return alias->name;
	for (legacy_cmd = legacycmds; legacy_cmd; legacy_cmd = legacy_cmd->next) {
		if (!strncasecmp(partial, legacy_cmd->oldname, len)) {
			return legacy_cmd->oldname;
		}
	}

	return NULL;
}

int Cmd_CompleteCountPossible (char *partial)
{
	cmd_function_t *cmd;
	legacycmd_t* legacy_cmd;
	int len, c = 0;

	len = strlen(partial);
	if (!len)
		return 0;

	for (cmd = cmd_functions; cmd; cmd = cmd->next) {
		if (!strncasecmp(partial, cmd->name, len)) {
			c++;
		}
	}

	// Also check legacy commands
	for (legacy_cmd = legacycmds; legacy_cmd; legacy_cmd = legacy_cmd->next) {
		if (!strncasecmp(partial, legacy_cmd->oldname, len)) {
			++c;
		}
	}

	return c;
}

int Cmd_AliasCompleteCountPossible (char *partial)
{
	cmd_alias_t *alias;
	int len, c = 0;

	len = strlen(partial);
	if (!len)
		return 0;

	for (alias = cmd_alias; alias; alias = alias->next)
		if (!strncasecmp (partial, alias->name, len))
			c++;

	return c;
}

int Cmd_CommandCompare (const void *p1, const void *p2)
{
	return strcmp((*((cmd_function_t **) p1))->name, (*((cmd_function_t **) p2))->name);
}

void Cmd_CmdList (qbool use_regex)
{
	static cmd_function_t *sorted_cmds[1024];
	int i, c, m = 0, count;
	cmd_function_t *cmd;
	char *pattern;

#define MAX_SORTED_CMDS (sizeof (sorted_cmds) / sizeof (sorted_cmds[0]))

	for (cmd = cmd_functions, count = 0; cmd && count < MAX_SORTED_CMDS; cmd = cmd->next, count++)
		sorted_cmds[count] = cmd;
	qsort (sorted_cmds, count, sizeof (cmd_function_t *), Cmd_CommandCompare);

	if (count == MAX_SORTED_CMDS)
		assert(!"count == MAX_SORTED_CMDS");

	pattern = (Cmd_Argc() > 1) ? Cmd_Argv(1) : NULL;

	if (((c = Cmd_Argc()) > 1) && use_regex) {
		if (!ReSearchInitEx(Cmd_Argv(1), false)) {
			return;
		}
	}

	Com_Printf ("List of commands:\n");
	for (i = 0; i < count; i++) {
		cmd = sorted_cmds[i];
		if (use_regex) {
			if (!(c == 1 || ReSearchMatch (cmd->name)))
				continue;
		} else {
			if (pattern && !Q_glob_match (pattern, cmd->name))
				continue;
		}

		Com_Printf ("%s\n", cmd->name);
		m++;
	}

	if (use_regex && (c > 1))
		ReSearchDone ();

	Com_Printf ("------------\n%i/%i commands\n", m,count);
}

void Cmd_CmdList_f (void)
{
	Cmd_CmdList (false);
}

void Cmd_CmdList_re_f (void)
{
	Cmd_CmdList (true);
}

void Cmd_ReInitAllMacro(void)
{
	int i;
	int teamplay = (int)Rulesets_RestrictTriggers();

	for (i = 0; i < num_macros; i++) {
		if (macro_commands[i].teamplay != MACRO_NORULES) {
			macro_commands[i].teamplay = teamplay;
		}
	}
}

void Cmd_AddMacroEx(macro_id id, char *(*f) (void), int teamplay)
{
	if (id < 0 || id >= num_macros) {
		return;
	}

	macro_commands[id].func = f;
	macro_commands[id].teamplay = teamplay;
}

void Cmd_AddMacro (macro_id id, char *(*f) (void))
{
	Cmd_AddMacroEx(id, f, MACRO_NORULES);
}

char *Cmd_MacroString (const char* s, int *macro_length)
{
	int i;
	macro_command_t	*macro;
	int best = -1;
	int best_length = -1;

	*macro_length = 0;
	for (i = 0; i < num_macros; i++) {
		macro = &macro_commands[i];
		if (macro->func) {
			int name_length = strlen(macro->name);
			if (!strncasecmp(s, macro->name, name_length)) {
				if (best_length == -1 || best_length < name_length) {
					best = i;
					best_length = name_length;
				}
			}
		}
	}

	if (best >= 0) {
		if (cbuf_current == &cbuf_main && (macro_commands[best].teamplay == MACRO_DISALLOWED)) {
			cbuf_current = &cbuf_formatted_comms;
		}
		*macro_length = best_length;
		return macro_commands[best].func();
	}

	return NULL;
}

static int Cmd_MacroCompare (const void *p1, const void *p2)
{
	return strcmp ((*((macro_command_t **) p1))->name, (*((macro_command_t **) p2))->name);
}

const char* Cmd_MacroName(macro_id id)
{
	return macro_commands[id].name;
}

qbool Cmd_MacroTeamplayRestricted(macro_id id)
{
	return macro_commands[id].teamplay;
}

void Cmd_MacroList_f (void)
{
	int i, c, m = 0;
	static macro_command_t* sorted_macros[num_macros];

	for (i = 0; i < num_macros; i++) {
		sorted_macros[i] = &macro_commands[i];
	}
	qsort(sorted_macros, num_macros, sizeof (macro_command_t *), Cmd_MacroCompare);

	c = Cmd_Argc();
	if (c > 1 && !ReSearchInitEx(Cmd_Argv(1), false)) {
		return;
	}

	Com_Printf ("List of macros:\n");
	for (i = 0; i < num_macros; i++) {
		if (c==1 || ReSearchMatch (sorted_macros[i]->name)) {
			Com_Printf ("$%s\n", sorted_macros[i]->name);
			m++;
		}
	}

	if (c > 1) {
		ReSearchDone();
	}

	Com_Printf ("------------\n%i/%i macros\n", m, num_macros);
}

void TP_SetDefaultMacroFormat(char* cvar_lookup, int* fixed_width, int* alignment);
char* TP_AlignMacroText(char* text, int fixed_width, int alignment);

//Expands all $cvar expressions to cvar values
//Also expands $macro expressions
//Note: dest must point to a 1024 byte buffer
void Cmd_ExpandString (const char *data, char *dest)
{
	unsigned int c;
	char buf[255], *str;
	int i, len = 0, quotes = 0, name_length = 0;
	cvar_t *var, *bestvar;
	int macro_length;

	while ((c = *data)) {
		if (c == '"')
			quotes++;

		if (c == '$' && !(quotes & 1)) {
			data++;

			// Copy the text after '$' to a temp buffer
			i = 0;
			buf[0] = 0;
			bestvar = NULL;
			while ((c = *data) > 32) {
				if (c == '$')
					break;

				data++;
				buf[i++] = c;
				buf[i] = 0;

				if ((var = Cvar_Find(buf)))
					bestvar = var;

				if (i >= (int) sizeof (buf) - 1)
					break; // there no more space in buf
			}

			str = Cmd_MacroString (buf, &macro_length);
			name_length = macro_length;

			if (bestvar && (!str || (strlen (bestvar->name) > macro_length))) {
				str = bestvar->string;
				name_length = strlen(bestvar->name);
                if (bestvar->teamplay)
                    cbuf_current = &cbuf_formatted_comms;
			}

			if (str) {
				int fixed_width = 0;
				int alignment = 0;

				TP_SetDefaultMacroFormat(buf, &fixed_width, &alignment);
				if (fixed_width != 0)
					str = TP_AlignMacroText(str, fixed_width, alignment);

				// check buffer size
				if (len + strlen (str) >= 1024 - 1)
					break;

				strcpy (&dest[len], str);
				len += strlen (str);
				i = name_length;
				while (buf[i])
					dest[len++] = buf[i++];
			} else {
				// no matching cvar or macro
				dest[len++] = '$';
				if (len + strlen (buf) >= 1024 - 1)
					break;

				strcpy (&dest[len], buf);
				len += strlen (buf);
			}
		} else {
			dest[len] = c;
			data++;
			len++;
			if (len >= 1024 - 1)
				break;
		}
	}

	dest[len] = 0;
}

int Commands_Compare_Func (const void * arg1, const void * arg2)
{
	return strcasecmp (*(char**) arg1, *(char**) arg2);
}

char *msgtrigger_commands[] = {
	"play", "playvol", "stopsound", "set", "echo", "say", "say_team",
	"alias", "unalias", "msg_trigger", "inc", "bind", "unbind", "record",
	"easyrecord", "stop", "if", "if_exists", "wait", "log", "match_forcestart",
	"dns", "addserver", "connect", "join", "observe",
	"tcl_proc", "tcl_exec", "tcl_eval", "exec",
	"set_ex", "set_alias_str", "set_bind_str","unset", "unset_re" ,
	"toggle", "toggle_re", "set_calc", "rcon", "user", "users",
	"unalias", "unalias_re",
	"re_trigger", "re_trigger_options", "re_trigger_delete",
	"re_trigger_enable","re_trigger_disable", "re_trigger_match",
	"hud262_add","hud262_remove","hud262_position","hud262_bg",
	"hud262_move","hud262_width","hud262_alpha","hud262_blink",
	"hud262_disable","hud262_enable","hud262_list","hud262_bringtofront",
	"hud_262font","hud262_hover","hud262_button",
	"alias_in", "alias_out", "cvar_in", "cvar_out"
	// ,NULL
};

char *formatted_comms_commands[] = {
	"if", "wait", "echo", "say", "say_team", "set_tp",
	"tp_point", "tp_pickup", "tp_took",
	"tp_msgreport", "tp_msgcoming", "tp_msglost", "tp_msgenemypwr",
	"tp_msgquaddead", "tp_msgsafe", "tp_msgkillme", "tp_msghelp",
	"tp_msggetquad", "tp_msggetpent", "tp_msgpoint", "tp_msgtook",
	"tp_msgtrick", "tp_msgreplace", "tp_msgneed", "tp_msgyesok",
	"tp_msgnocancel", "tp_msgutake", "tp_msgitemsoon", "tp_msgwaiting",
	"tp_msgslipped",
    NULL
};

float	impulse_time = -9999;
int		impulse_counter;

qbool AllowedImpulse(int imp)
{

	static int Allowed_TF_Impulses[] = {
	                                   135, 99, 101, 102, 103, 104, 105, 106, 107, 108, 109, 23, 144, 145,
	                                   159, 160, 161, 162, 163, 164, 165, 166, 167
	                               };

	int i;

	if (!cl.teamfortress) return false;
	for (i=0; i<sizeof(Allowed_TF_Impulses)/sizeof(Allowed_TF_Impulses[0]); i++) {
		if (Allowed_TF_Impulses[i] == imp) {
			if(++impulse_counter >= 30) {
				if (cls.realtime < impulse_time + 5 && !cls.demoplayback) {
					return false;
				}
				impulse_time = cls.realtime;
				impulse_counter = 0;
			}
			return true;
		}
	}
	return false;
}

static qbool Cmd_IsCommandAllowedInMessageTrigger( const char *command )
{
	if( !strcasecmp( command, "impulse") )
		return AllowedImpulse(Q_atoi(Cmd_Argv(1)));

	return 	  bsearch( &(command), msgtrigger_commands,
	                   sizeof(msgtrigger_commands)/sizeof(msgtrigger_commands[0]),
	                   sizeof(msgtrigger_commands[0]),Commands_Compare_Func) != NULL;
}

static qbool Cmd_IsCommandAllowedInTeamPlayMacros( const char *command )
{
	char **s;
	for (s = formatted_comms_commands; *s; s++) {
		if (!strcasecmp(command, *s))
			break;
	}
	return *s != NULL;
}

//A complete command line has been parsed, so try to execute it
static void Cmd_ExecuteStringEx (cbuf_t *context, char *text)
{
	cvar_t *v;
	cmd_function_t *cmd;
	cmd_alias_t *a;
	static char buf[1024];
	cbuf_t *inserttarget, *oldcontext;
	char *p, *n, *s;
	char text_exp[1024];

	oldcontext = cbuf_current;
	cbuf_current = context;

	Cmd_ExpandString(text, text_exp);
	Cmd_TokenizeStringEx2(&cmd_tokenizecontext, text_exp, cl_curlybraces.integer);

	if (!Cmd_Argc())
		goto done; // no tokens

	if (cbuf_current == &cbuf_svc) {
		if (CL_CheckServerCommand())
			goto done;
	}

#ifndef CLIENTONLY
	// 'status' on remote ktx servers..
	if (!strcmp(Cmd_Argv(0), "status") && Cmd_Argc() == 1 && Cmd_FindAlias("status")) {
		goto checkaliases;
	}
#endif

	// check functions
	if ((cmd = Cmd_FindCommand(Cmd_Argv(0)))) {
		if (gtf || cbuf_current == &cbuf_safe) {
			if (!Cmd_IsCommandAllowedInMessageTrigger(Cmd_Argv(0))) {
				Com_Printf ("\"%s\" cannot be used in message triggers\n", Cmd_Argv(0));
				goto done;
			}
		} else if (cbuf_current == &cbuf_formatted_comms) {
			if (!Cmd_IsCommandAllowedInTeamPlayMacros(Cmd_Argv(0))) {
				Com_Printf ("\"%s\" cannot be used in combination with teamplay $macros\n", Cmd_Argv(0));
				goto done;
			}
		}

		if (cmd->function)
			cmd->function();
		else
			Cmd_ForwardToServer ();
		goto done;
	}

	// some bright guy decided to use "skill" as a mod command in Custom TF, sigh
	if (!strcmp(Cmd_Argv(0), "skill") && Cmd_Argc() == 1 && Cmd_FindAlias("skill"))
		goto checkaliases;

	// check cvars
	if ((v = Cvar_Find(Cmd_Argv(0)))) {
		if (cbuf_current == &cbuf_formatted_comms) {
			Com_Printf ("\"%s\" cannot be used in combination with teamplay $macros\n", Cmd_Argv(0));
			goto done;
		}
		if (Cvar_Command())
			goto done;
	}

	// check aliases
checkaliases:
	if ((a = Cmd_FindAlias(Cmd_Argv(0)))) {

		// QW262 -->
		if (a->value[0] == '\0') {
			goto done; // alias is empty.
		}

		if(a->flags & ALIAS_HAS_PARAMETERS) { // %parameters are given in alias definition
			s=a->value;
			buf[0] = '\0';
			do {
				n = strchr(s, '%');
				if(n) {
					if(*++n >= '1' && *n <= '9') {
						n[-1] = 0;
						strlcat(buf, s, sizeof(buf));
						n[-1] = '%';
						// insert numbered parameter
						strlcat(buf,Cmd_Argv(*n-'0'), sizeof(buf));
					} else if (*n == '0') {
						n[-1] = 0;
						strlcat(buf, s, sizeof(buf));
						n[-1] = '%';
						// insert all parameters
						strlcat(buf, Cmd_Args(), sizeof(buf));
					} else if (*n == '%') {
						n[0] = 0;
						strlcat(buf, s, sizeof(buf));
						n[0] = '%';
					} else {
						if (*n) {
							char tmp = n[1];
							n[1] = 0;
							strlcat(buf, s, sizeof(buf));
							n[1] = tmp;
						} else
							strlcat(buf, s, sizeof(buf));
					}
					s=n+1;
				}
			} while(n);
			strlcat(buf, s, sizeof(buf));
			p = buf;

		} else  // alias has no parameters
			p = a->value;
		// <-- QW262

		if (cbuf_current == &cbuf_svc)
		{
			Cbuf_AddText (p);
			Cbuf_AddText ("\n");
		} else
		{
			inserttarget = cbuf_current ? cbuf_current : &cbuf_main;
			Cbuf_InsertTextEx (inserttarget, "\n");

			// if the alias value is a command or cvar and
			// the alias is called with parameters, add them
			if (Cmd_Argc() > 1 && !strchr(p, ' ') && !strchr(p, '\t') &&
			        (Cvar_Find(p) || (Cmd_FindCommand(p) && p[0] != '+' && p[0] != '-'))
			   ) {
				Cbuf_InsertTextEx (inserttarget, Cmd_Args());
				Cbuf_InsertTextEx (inserttarget, " ");
			}
			Cbuf_InsertTextEx (inserttarget, p);
		}
		goto done;
	}

	if (Cmd_LegacyCommand())
		goto done;

	if (!host_initialized && Cmd_Argc() > 1) {
		if (Cvar_CreateTempVar())
			goto done;
	}

	if (cbuf_current != &cbuf_svc)
	{
		if (cl_warncmd.integer || developer.integer)
			Com_Printf ("Unknown command \"%s\"\n", Cmd_Argv(0));
	}

done:
	cbuf_current = oldcontext;
}

void Cmd_ExecuteString (char *text)
{
	Cmd_ExecuteStringEx (NULL, text);
}

static qbool is_numeric (char *c)
{
	return ( isdigit((int)(unsigned char)*c) ||
	         ((*c == '-' || *c == '+') && (c[1] == '.' || isdigit((int)(unsigned char)c[1]))) ||
	         (*c == '.' && isdigit((int)(unsigned char)c[1])) );
}

void Re_Trigger_Copy_Subpatterns (const char *s, size_t* offsets, int num, cvar_t *re_sub); // QW262
extern cvar_t re_sub[10]; // QW262

void Cmd_CatchTriggerSubpatterns(const char *s, size_t* offsets, int num)
{
	Re_Trigger_Copy_Subpatterns(s, offsets, min(num, 10), re_sub);
}

// this is a test replacement of the "if" command
void Cmd_If_New(void)
{
	// syntax of this command has two possibilities:
	// if (<expr>) then <cmd1> [else <cmd2>]
	// if o1 o2 o3 [then] <cmd1> [else <cmd2>]
	// the second one is for backward compatibility
	parser_extra pars_ex;
	int c;
	int then_pos = 0;
	qbool then_found = false, else_found = false;
	int i, clen;
	size_t expr_len = 0;
	char* expr, * curarg;
	int result, error;
	char buf[1024];

	pars_ex.subpatt_fnc = Cmd_CatchTriggerSubpatterns;
	pars_ex.var2val_fnc = NULL;

	c = Cmd_Argc();

	// 0  1 2    3
	// if e then c
	if (c < 4) {
		Com_Printf("Usage: if <expr> then <cmds> [else <cmds>]\n");
		return;
	}

	for (i = 2; i < c; i++) {
		if (!strcmp(Cmd_Argv(i), "then")) {
			then_pos = i; then_found = true; break;
		}
	}

	if (!then_found) {
		Com_Printf("if command: \"then\" not found\n");
		return;
	}

	for (i = 1; i < then_pos; i++) {
		clen = strlen(Cmd_Argv(i));
		expr_len += clen ? clen + 1 : 3; // we will take '' as a representation of an empty string
	}

	expr = (char *) Q_malloc(expr_len + 1);
	expr[0] = '\0';

	for (i = 1; i < then_pos; i++) {
		if (i > 1)
			strlcat(expr, " ", expr_len + 1);

		curarg = Cmd_Argv(i);

		if (*curarg)
			strlcat(expr, curarg, expr_len + 1);
		else
			strlcat(expr, "''", expr_len + 1);
	}

	error = Expr_Eval_Bool(expr, &pars_ex, &result);
	if (error != EXPR_EVAL_SUCCESS) {
		Com_Printf("Error in condition: %s (\"%s\")\n", Parser_Error_Description(error), expr);
		Q_free(expr);
		return;
	}

	Q_free(expr);

	then_pos++;	// skip "then"

	buf[0] = '\0';
	if (result)	// true case
	{
		for (i = then_pos; i < c; i++) {
			if (!else_found && !strcmp(Cmd_Argv(i), "else"))
				break;

			if (buf[0])
				strlcat (buf, " ", sizeof(buf));

			strlcat (buf, Cmd_Argv(i), sizeof(buf));
		}
	}
	else // result = false
	{
		for (i = then_pos; i < c; i++) {
			if (else_found) {
				if (buf[0])
					strlcat (buf, " ", sizeof(buf));
				strlcat (buf, Cmd_Argv(i), sizeof(buf));
			}
			if (!else_found && !strcmp(Cmd_Argv(i), "else")) else_found = true;
		}
	}

	strlcat (buf, "\n", sizeof(buf));
	Cbuf_InsertTextEx (cbuf_current ? cbuf_current : &cbuf_main, buf);
}

void Cmd_If_Old (void)
{
	int	i, c;
	char *op, buf[1024] = {0};
	qbool result;

	if ((c = Cmd_Argc()) < 5) {
		Com_Printf ("Usage: if <expr1> <op> <expr2> <command> [else <command>]\n");
		return;
	}

	op = Cmd_Argv(2);
	if (!strcmp(op, "==") || !strcmp(op, "=") || !strcmp(op, "!=") || !strcmp(op, "<>")) {
		if (is_numeric(Cmd_Argv(1)) && is_numeric(Cmd_Argv(3)))
			result = Q_atof(Cmd_Argv(1)) == Q_atof(Cmd_Argv(3));
		else
			result = !strcmp(Cmd_Argv(1), Cmd_Argv(3));

		if (op[0] != '=')
			result = !result;
	} else if (!strcmp(op, ">")) {
		result = Q_atof(Cmd_Argv(1)) > Q_atof(Cmd_Argv(3));
	} else if (!strcmp(op, "<")) {
		result = Q_atof(Cmd_Argv(1)) < Q_atof(Cmd_Argv(3));
	} else if (!strcmp(op, ">=")) {
		result = Q_atof(Cmd_Argv(1)) >= Q_atof(Cmd_Argv(3));
	} else if (!strcmp(op, "<=")) {
		result = Q_atof(Cmd_Argv(1)) <= Q_atof(Cmd_Argv(3));

	} else if (!strcmp(op, "isin")) {
		result = (strstr(Cmd_Argv(3), Cmd_Argv(1)) ? 1 : 0);
	} else if (!strcmp(op, "!isin")) {
		result = (strstr(Cmd_Argv(3), Cmd_Argv(1)) ? 0 : 1);

	} else if (!strcmp(op, "=~") || !strcmp(op, "!~")) {
		pcre2_code       *regexp;
		int              error;
		PCRE2_SIZE       error_offset;
		pcre2_match_data *match_data = NULL;
		int              rc;

		regexp = pcre2_compile ((PCRE2_SPTR)Cmd_Argv(3), PCRE2_ZERO_TERMINATED, 0, &error, &error_offset, NULL);
		if (!regexp) {
			PCRE2_UCHAR error_str[256];
			pcre2_get_error_message(error, error_str, sizeof(error_str));
			Com_Printf ("Error in regexp: %s\n", error_str);
			return;
		}
		match_data = pcre2_match_data_create_from_pattern(regexp, NULL);
		rc = pcre2_match (regexp, (PCRE2_SPTR)Cmd_Argv(1), strlen(Cmd_Argv(1)),
		                0, 0, match_data, NULL);
		if (rc >= 0) {
			PCRE2_SIZE *offsets = pcre2_get_ovector_pointer(match_data);
			Re_Trigger_Copy_Subpatterns (Cmd_Argv(1), offsets, min(rc, 10), re_sub);
			result = true;
		} else
			result = false;

		if (op[0] != '=')
			result = !result;

		pcre2_match_data_free (match_data);
		pcre2_code_free (regexp);
	} else {
		Com_Printf ("unknown operator: %s\n", op);
		Com_Printf ("valid operators are ==, =, !=, <>, >, <, >=, <=, isin, !isin, =~, !~\n");
		return;
	}

	if (result)	{
		for (i = 4; i < c; i++) {
			if ((i == 4) && !strcasecmp(Cmd_Argv(i), "then"))
				continue;
			if (!strcasecmp(Cmd_Argv(i), "else"))
				break;
			if (buf[0])
				strlcat (buf, " ", sizeof (buf) - strlen (buf) - 1);

			strlcat (buf, Cmd_Argv(i), sizeof (buf) - strlen (buf) - 1);
		}
	} else {
		for (i = 4; i < c ; i++) {
			if (!strcasecmp(Cmd_Argv(i), "else"))
				break;
		}
		if (i == c)
			return;
		for (i++; i < c; i++) {
			if (buf[0])
				strlcat (buf, " ", sizeof (buf) - strlen (buf) - 1);

			strlcat (buf, Cmd_Argv(i), sizeof (buf) - strlen (buf) - 1);
		}
	}

	strlcat (buf, "\n", sizeof (buf) - strlen (buf));
	Cbuf_InsertTextEx (cbuf_current ? cbuf_current : &cbuf_main, buf);
}

void Cmd_If_f(void) {
	if (Cmd_Argc() > 2 && Cmd_Argv(1)[0] == '(')
		 // new "if" requires parentheses around the condition
		 // while the original "if" wouldn't work with these so it's safe
		 // to presume noone used it there
		 Cmd_If_New();
	else Cmd_If_Old();
}

void Cmd_If_Exists_f(void)
{
	int	argc;
	char	*type;
	char	*name;
	qbool	exists;
	qbool	iscvar, isalias, istrigger, ishud;

	argc = Cmd_Argc();
	if ( argc < 4 || argc > 5) {
		Com_Printf ("if_exists <type> <name> <cmd1> [<cmd2>] - conditional execution\n");
		return;
	}

	type = Cmd_Argv(1);
	name = Cmd_Argv(2);
	if ( ( (iscvar = !strcmp(type, "cvar")) && Cvar_Find(name) )			||
	        ( (isalias = !strcmp(type, "alias")) && Cmd_FindAlias (name) )			||
	        ( (istrigger = !strcmp(type, "trigger")) && CL_FindReTrigger (name) )	||
	        ( (ishud = !strcmp(type, "hud")) && Hud_ElementExists (name) ) )
		exists = true;
	else {
		exists = false;
		if (!(iscvar || isalias || istrigger || ishud)) {
			Com_Printf("if_exists: <type> can be cvar, alias, trigger, hud\n");
			return;
		}
	}

	if (exists) {
		Cbuf_InsertTextEx (cbuf_current ? cbuf_current : &cbuf_main,"\n");
		Cbuf_InsertTextEx (cbuf_current ? cbuf_current : &cbuf_main,Cmd_Argv(3));
	} else if (argc == 5) {
		Cbuf_InsertTextEx (cbuf_current ? cbuf_current : &cbuf_main,"\n");
		Cbuf_InsertTextEx (cbuf_current ? cbuf_current : &cbuf_main,Cmd_Argv(4));
	} else
		return;
}

void Cmd_Eval_f(void)
{
	int errn;
	expr_val value;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: eval <expression>\n"
			"Prints the value of given expression after evaluation in the internal parser\n");
		return;
	}

	value = Expr_Eval(Cmd_Argv(1), NULL, &errn);

	if (errn != EXPR_EVAL_SUCCESS)
	{
		Com_Printf("Error occured: %s\n", Parser_Error_Description(errn));
		return;
	}
	else
	{
		switch (value.type) {
		case ET_INT:  Com_Printf("Result: %i (integer)\n", value.i_val); break;
		case ET_DBL:  Com_Printf("Result: %f (double)\n",  value.d_val); break;
		case ET_BOOL: Com_Printf("Result: %s (bool)\n", value.b_val ? "true" : "false"); break;
		case ET_STR:  Com_Printf("Result: (string)\n\"%s\"\n", value.s_val); Q_free(value.s_val); break;
		default:      Com_Printf("Error: Unknown value type\n"); break;
		}
	}
}

// QW262 -->
static qbool do_in(char *buf, char *orig, char *str, int options)
{
	if ((options & 2) && strstr(orig, str))
		return false;

	if (options & 1) { // buf size is 1024 both in Cmd_Alias_In_f and Cmd_Cvar_In_f
		strlcpy(buf, orig, 1024);
		strlcat(buf, str, 1024);
	} else {
		strlcpy(buf, str, 1024);
		strlcat(buf, orig, 1024);
	}
	return true;
}

static qbool do_out(char *orig, char *str, int options)
{
	char	*p;
	int		len = strlen(str);

	if (!(p=strstr(orig, str)))
		return false;

	if (!(options & 1))
		memmove(p, p+len, strlen(p)+1);
	return true;
}

void Cmd_Alias_In_f (void)
{
	cmd_alias_t	*alias;
	cvar_t		*var;
	char		buf[1024];
	char		*alias_name;
	int			options;

	if (Cmd_Argc() < 3 || Cmd_Argc() > 4) {
		Com_Printf ("alias_in <alias> <cvar> [<options>]\n");
		return;
	}

	alias_name = Cmd_Argv(1);
	alias = Cmd_FindAlias(alias_name);
	options = atoi(Cmd_Argv(3));

	if (!alias) {
		if ((options & 8)) {
			alias = Cmd_AliasCreate(alias_name);
			if (!alias)
				return;
			alias->value = Q_strdup("");
		} else {
			Com_Printf ("alias_in: unknown alias \"%s\"\n", alias_name);
			return;
		}
	}

	var = Cvar_Find(Cmd_Argv(2));
	if (!var) {
		Com_Printf ("alias_in: unknown cvar \"%s\"\n", Cmd_Argv(2));
		return;
	}

	if (!do_in (buf, alias->value, var->string, options)) {
		if (options & 4)
			Com_Printf ("alias_in: already inserted\n");
		return;
	}

	Q_free(alias->value);
	alias->value = Q_strdup(buf);
	if (strchr(buf, '%'))
		alias->flags |= ALIAS_HAS_PARAMETERS;
}

void Cmd_Alias_Out_f (void)
{
	cmd_alias_t	*alias;
	cvar_t		*var;
	int			options;

	if (Cmd_Argc() < 3 || Cmd_Argc() > 4) {
		Com_Printf ("alias_out <alias> <cvar> [options]\n");
		return;
	}

	options = atoi(Cmd_Argv(3));

	alias = Cmd_FindAlias(Cmd_Argv(1));
	if (!alias) {
		Com_Printf ("alias_out: unknown alias \"%s\"\n", Cmd_Argv(1));
		return;
	}

	var = Cvar_Find(Cmd_Argv(2));
	if (!var) {
		Com_Printf ("alias_out: unknown cvar \"%s\"\n", Cmd_Argv(2));
		return;
	}

	if (!do_out (alias->value, var->string, options)) {
		if (!(options & 2))
			Com_Printf ("alias_out: not found\n");
		return;
	}
}

void Cmd_Cvar_In_f (void)
{
	cvar_t		*var1;
	cvar_t		*var2;
	char		buf[1024];
	char		*var_name;
	int			options;

	if (Cmd_Argc() < 3 || Cmd_Argc() > 4) {
		Com_Printf ("cvar_in <cvar1> <cvar2> [<options>]\n");
		return;
	}

	var_name = Cmd_Argv(1);
	options = atoi(Cmd_Argv(3));

	var1 = Cvar_Find(var_name);
	if (!var1) {
		if ((options & 8)) {
			var1 = Cvar_Create (var_name, "", 0);
		} else {
			Com_Printf ("cvar_in: unknown cvar \"%s\"\n", var_name);
			return;
		}
	}

	var2 = Cvar_Find(Cmd_Argv(2));
	if (!var2) {
		Com_Printf ("cvar_in: unknown cvar \"%s\"\n", Cmd_Argv(2));
		return;
	}

	if (!do_in (buf, var1->string, var2->string, options)) {
		if (options & 4)
			Com_Printf ("cvar_in: already inserted\n");
		return;
	}

	Cvar_Set (var1, buf);
}

void Cmd_Cvar_Out_f (void)
{
	cvar_t		*var1;
	cvar_t		*var2;
	char		buf[1024];
	int			options;

	if (Cmd_Argc()<3 || Cmd_Argc()>4){
		Com_Printf ("cvar_out <cvar1> <cvar2> [<options>]\n");
		return;
	}

	options = atoi(Cmd_Argv(3));
	var1 = Cvar_Find(Cmd_Argv(1));
	if (!var1) {
		Com_Printf ("cvar_out: unknown cvar \"%s\"\n", Cmd_Argv(1));
		return;
	}

	var2 = Cvar_Find(Cmd_Argv(2));
	if (!var2) {
		Com_Printf ("cvar_out: unknown cvar \"%s\"\n", Cmd_Argv(2));
		return;
	}

	strcpy (buf, var1->string);
	if (!do_out (buf, var2->string, options)) {
		if (!(options & 2))
			Com_Printf ("cvar_out: not found\n");
		return;
	}

	Cvar_Set (var1, buf);
}
// <-- QW262


void Cmd_Init (void)
{
	// register our commands
	Cmd_AddCommand ("exec", Cmd_Exec_f);
#ifndef SERVERONLY
	Cmd_AddCommand ("serverexec", Cmd_Exec_f);
#endif
	Cmd_AddCommand ("echo", Cmd_Echo_f);
	Cmd_AddCommand ("aliaslist", Cmd_AliasList_f);
	Cmd_AddCommand ("aliasedit", Cmd_AliasEdit_f);
	Cmd_AddCommand ("alias", Cmd_Alias_f);
	Cmd_AddCommand ("tempalias", Cmd_Alias_f);
	Cmd_AddCommand ("viewalias", Cmd_Viewalias_f);
	Cmd_AddCommand ("unaliasall", Cmd_UnAliasAll_f);
	Cmd_AddCommand ("unalias", Cmd_UnAlias_f);
	Cmd_AddCommand ("unalias_re", Cmd_UnAlias_re_f);
	Cmd_AddCommand ("wait", Cmd_Wait_f);
	Cmd_AddCommand ("cmdlist", Cmd_CmdList_f);
	Cmd_AddCommand ("cmdlist_re", Cmd_CmdList_re_f);
	Cmd_AddCommand ("if", Cmd_If_f);
	Cmd_AddCommand ("if_exists", Cmd_If_Exists_f);
	Cmd_AddCommand ("eval", Cmd_Eval_f);
	Cmd_AddCommand ("clipboard", Cmd_Clipboard_f);
// QW262 -->
	Cmd_AddCommand ("alias_in", Cmd_Alias_In_f);
	Cmd_AddCommand ("alias_out", Cmd_Alias_Out_f);
	Cmd_AddCommand ("cvar_in", Cmd_Cvar_In_f);
	Cmd_AddCommand ("cvar_out", Cmd_Cvar_Out_f);
// <-- QW262

	Cvar_Register(&cl_curlybraces);
    Cvar_Register(&cl_warnexec);

	Cmd_AddCommand ("macrolist", Cmd_MacroList_f);
	qsort(msgtrigger_commands,
	      sizeof(msgtrigger_commands)/sizeof(msgtrigger_commands[0]),
	      sizeof(msgtrigger_commands[0]),Commands_Compare_Func);
}

void Cmd_Shutdown(void)
{
	int i;
	cmd_alias_t* alias;
	cmd_alias_t* next_alias;
	cmd_function_t* cmd;
	cmd_function_t* next_cmd;
	legacycmd_t* legacycmd;
	legacycmd_t* next_legacycmd;

	Sys_Printf("Cmd_Shutdown(aliases)\n");
	for (i = 0; i < sizeof(cmd_alias_hash) / sizeof(cmd_alias_hash[0]); ++i) {
		cmd_alias_hash[i] = NULL;
	}

	for (alias = cmd_alias; alias; alias = next_alias) {
		next_alias = alias->next;
		Q_free(alias->value);
		Q_free(alias);
	}
	cmd_alias = NULL;

	Sys_Printf("Cmd_Shutdown(functions)\n");
	for (cmd = cmd_functions; cmd; cmd = next_cmd) {
		next_cmd = cmd->next;

		if (cmd->zmalloced) {
			Q_free(cmd);
		}
	}
	cmd_functions = NULL;

	Sys_Printf("Cmd_Shutdown(legacy)\n");
	for (legacycmd = legacycmds; legacycmd; legacycmd = next_legacycmd) {
		next_legacycmd = legacycmd->next;
		Q_free(legacycmd);
	}
}
