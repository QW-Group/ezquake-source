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

// cmd.h -- Command buffer and command execution

//===========================================================================

/*
Any number of commands can be added in a frame, from several different sources.
Most commands come from either keybindings or console line input, but remote
servers can also send across commands and entire text files can be execed.

The + command line options are also added to the command buffer.

The game starts with a Cbuf_AddText ("exec quake.rc\n"); Cbuf_Execute ();
*/

typedef struct cbuf_s {
	char*   text_buf;
	size_t  maxsize;
	size_t  text_start;
	size_t  text_end;
	qbool   wait;
	int     waitCount;
	int     runAwayLoop;
} cbuf_t;

extern cbuf_t cbuf_main;
extern cbuf_t cbuf_safe; // msg_trigger commands
extern cbuf_t cbuf_formatted_comms;
extern cbuf_t cbuf_svc; // svc_stufftext commands
extern cbuf_t cbuf_server;  // mod commands
extern cbuf_t *cbuf_current;

void Cbuf_AddTextEx (cbuf_t *cbuf, const char *text);
void Cbuf_InsertTextEx (cbuf_t *cbuf, const char *text);
void Cbuf_ExecuteEx (cbuf_t *cbuf);

void Cbuf_Init (void);
// allocates an initial text buffer that will grow as needed

void Cbuf_AddText (const char *text);
// as new commands are generated from the console or keybindings,
// the text is added to the end of the command buffer.

void Cbuf_InsertText (const char *text);
// when a command wants to issue other commands immediately, the text is
// inserted at the beginning of the buffer, before any remaining unexecuted
// commands.

void Cbuf_Execute (void);
// Pulls off \n terminated lines of text from the command buffer and sends
// them through Cmd_ExecuteString.  Stops when the buffer is empty.
// Normally called once per frame, but may be explicitly invoked.
// Do not call inside a command function!

void Cbuf_Flush(cbuf_t* cbuf);
// Intended for startup... keeps executing until empty

//===========================================================================

/*
Command execution takes a null terminated string, breaks it into tokens,
then searches for a command or variable that matches the first token.
*/

typedef void (*xcommand_t) (void);

typedef struct cmd_function_s {
	struct cmd_function_s	*hash_next;
	struct cmd_function_s	*next;
	char					*name;
	xcommand_t				function;
	qbool                   zmalloced;
} cmd_function_t;

void Cmd_Init (void);
void Cmd_Shutdown (void);

void Cmd_AddCommand (char *cmd_name, xcommand_t function);
// called by the init functions of other parts of the program to
// register commands and functions to call for them.
// The cmd_name is referenced later, so it should not be in temp memory
// if function is NULL, the command will be forwarded to the server
// as a clc_stringcmd instead of executed locally

// plugins use it; command is Q_malloced, name is stored within the same memory piece
qbool Cmd_AddRemCommand (char *cmd_name, xcommand_t function);

// only used by plugins, other commands stay in the client forever
void Cmd_RemoveCommand (char *cmd_name);

qbool Cmd_Exists (char *cmd_name);
// used by the cvar code to check for cvar / command name overlap

cmd_function_t *Cmd_FindCommand (const char *cmd_name);  // for message triggers

char *Cmd_CompleteCommand (char *partial);
// attempts to match a partial command for automatic command line completion
// returns NULL if nothing fits

#define	MAX_ARGS		80

typedef struct tokenizecontext_s
{
	int		cmd_argc; // arguments count
	char	*cmd_argv[MAX_ARGS]; // links to argv_buf[]

	// FIXME: MAX_COM_TOKEN not defined here, need redesign headers or something

	char	argv_buf[/*MAX_COM_TOKEN*/ 1024]; // here we store data for *cmd_argv[]

	char	cmd_args[/*MAX_COM_TOKEN*/ 1024 * 2]; // here we store original of what we parse, from argv(1) to argv(argc() - 1)

	char	text[/*MAX_COM_TOKEN*/ 1024]; // this is used/overwrite each time we using Cmd_MakeArgs()

} tokenizecontext_t;

int Cmd_ArgcEx (tokenizecontext_t *ctx);
char *Cmd_ArgvEx (tokenizecontext_t *ctx, int arg);

//Returns a single string containing argv(1) to argv(argc() - 1)
char *Cmd_ArgsEx (tokenizecontext_t *ctx);

//Returns a single string containing argv(start) to argv(argc() - 1)
//Unlike Cmd_Args, shrinks spaces between argvs
char *Cmd_MakeArgsEx (tokenizecontext_t *ctx, int start);

//Parses the given string into command line tokens.
void Cmd_TokenizeStringEx (tokenizecontext_t *ctx, const char *text);

int	Cmd_Argc (void);
char *Cmd_Argv (int arg);
char *Cmd_Args (void);
char *Cmd_MakeArgs (int start);

// save cmd_tokenizecontext struct to ctx
void Cmd_SaveContext(tokenizecontext_t *ctx);

// restore cmd_tokenizecontext struct from ctx
void Cmd_RestoreContext(tokenizecontext_t *ctx);

// The functions that execute commands get their parameters with these
// functions. Cmd_Argv () will return an empty string, not a NULL
// if arg > argc, so string operations are always safe.

void Cmd_ExpandString (const char *data, char *dest);
// Expands all $cvar or $macro expressions.
// dest should point to a 1024-byte buffer

void Cmd_TokenizeString (const char *text);
// Takes a null terminated string.  Does not need to be /n terminated.
// breaks the string up into arg tokens.

void Cmd_ExecuteString (char *text);
// Parses a single line of text into arguments and tries to execute it
// as if it was typed at the console

void Cmd_ForwardToServer (void);
// adds the current command line as a clc_stringcmd to the client message.
// things like godmode, noclip, etc, are commands directed to the server,
// so when they are typed in at the console, they will need to be forwarded.

void Cbuf_AddEarlyCommands (void);
void Cmd_StuffCmds_f (void);
qbool Cmd_IsLegacyCommand (char *oldname);
void Cmd_AddLegacyCommand (char *oldname, char *newname);
qbool CL_IsDownloadableFileExtension(const char *filename);

//===========================================================================

#define	MAX_ALIAS_NAME 32

#define ALIAS_ARCHIVE			1
#define ALIAS_SERVER			2
#define ALIAS_TEMP				4
#define	ALIAS_HAS_PARAMETERS	8

typedef struct cmd_alias_s {
	struct cmd_alias_s	*hash_next;
	struct cmd_alias_s	*next;
	char				name[MAX_ALIAS_NAME];
	char				*value;
	int					flags;
} cmd_alias_t;

qbool Cmd_DeleteAlias (char *name);	// return true if successful
cmd_alias_t *Cmd_FindAlias (const char *name); // returns NULL on failure
char *Cmd_AliasString (char *name); // returns NULL on failure

void DeleteServerAliases (void);

#define	MAX_MACRO_NAME 32
#define MACRO_NORULES -1
#define MACRO_ALLOWED 0
#define MACRO_DISALLOWED 1

#include "macro_definitions.h"

void Cmd_AddMacro(macro_id id, char *(*f)(void));
void Cmd_AddMacroEx(macro_id id, char *(*f)(void), int teamplay);
char* Cmd_MacroString(const char *s, int *macro_length);
const char* Cmd_MacroName(macro_id id);
qbool Cmd_MacroTeamplayRestricted(macro_id id);

// only here for enumerating from keys.c (tab-complete) - move to cmd.c
typedef struct legacycmd_s
{
	char* oldname, * newname;
	cmd_function_t dummy_cmd;
	struct legacycmd_s* next;
} legacycmd_t;

extern legacycmd_t* legacycmds;
