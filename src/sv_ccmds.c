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

#ifndef CLIENTONLY
#include "qwsvdef.h"

cvar_t	sv_cheats = {"sv_cheats", "0"};
qbool	sv_allow_cheats = false;

int fp_messages=4, fp_persecond=4, fp_secondsdead=10;
char fp_msg[255] = { 0 };
extern	cvar_t		sv_logdir; //bliP: 24/7 logdir
extern	redirect_t	sv_redirected;

void SV_Localinfo_Set (const char *name, const char *value);

/*
===============================================================================

OPERATOR CONSOLE ONLY COMMANDS

These commands can only be entered from stdin or by a remote operator datagram
===============================================================================
*/

/*
==================
SV_Quit
==================
*/
void SV_Quit (qbool restart)
{
	SV_Shutdown ("Server shutdown.\n");
#ifdef SERVERONLY
	Sys_Quit (restart);
#else
	Host_Quit(); // will also call SV_Shutdown(), but it is not an issue.
#endif
}

/*
==================
SV_Quit_f
==================
*/
void SV_Quit_f (void)
{
	SV_Quit(false);
}

/*
==================
SV_Restart_f
==================
*/
void SV_Restart_f (void)
{
	SV_Quit(true);
}

/*
============
SV_Logfile
============
*/
void SV_Logfile (int sv_log, qbool newlog)
{
	int		sv_port = NET_UDPSVPort();
	char	name[MAX_OSPATH];
	int		i;

	// newlog - stands for: we want open new log file

	if (logs[sv_log].sv_logfile)
	{
		// turn off logging

		fclose (logs[sv_log].sv_logfile);
		logs[sv_log].sv_logfile = NULL;

		// in case of NON "newlog" we do some additional work and exit function
		if (!newlog)
		{
			Con_Printf ("%s", logs[sv_log].message_off);
			logs[sv_log].log_level = 0;
			return;
		}
	}

	// always use new log file for frag log
	if (sv_log == FRAG_LOG || sv_log == MOD_FRAG_LOG)
		newlog = true;

	for (i = 0; i < 1000; i++)
	{
		snprintf (name, sizeof(name), "%s/%s%d_%04d.log", sv_logdir.string, logs[sv_log].file_name, sv_port, i);

		if (!COM_FileExists(name))
			break; // file doesn't exist
	}

	if (!newlog) //use last log if possible
		snprintf (name, sizeof(name), "%s/%s%d_%04d.log",  sv_logdir.string, logs[sv_log].file_name, sv_port, (int)max(0, i - 1));

	Con_Printf ("Logging %s to %s\n", logs[sv_log].message_on, name);

	if (!(logs[sv_log].sv_logfile = fopen (name, "a")))
	{
		Con_Printf ("Failed.\n");
		logs[sv_log].sv_logfile = NULL;
		return;
	}

	switch (sv_log)
	{
	case TELNET_LOG:
		logs[TELNET_LOG].log_level = Cvar_Value("telnet_log_level");
		break;
	case CONSOLE_LOG:
		logs[CONSOLE_LOG].log_level = Cvar_Value("qconsole_log_say");
		break;
	default:
		logs[sv_log].log_level = 1;
	}
}

/*
============
SV_Logfile_f
============
*/
void SV_Logfile_f (void)
{
	SV_Logfile(CONSOLE_LOG, false);
}

/*
============
SV_ErrorLogfile_f
============
*/
void SV_ErrorLogfile_f (void)
{
	SV_Logfile(ERROR_LOG, false);
}

/*
============
SV_RconLogfile_f
============
*/
void SV_RconLogfile_f (void)
{
	SV_Logfile(RCON_LOG, false);
}

/*
============
SV_RconLogfile_f
============
*/
void SV_TelnetLogfile_f (void)
{
	SV_Logfile(TELNET_LOG, false);
}

/*
============
SV_FragLogfile_f
============
*/
void SV_FragLogfile_f (void)
{
	SV_Logfile(FRAG_LOG, false);
}

//bliP: player log
/*
============
SV_PlayerLogfile_f
============
*/
void SV_PlayerLogfile_f (void)
{
	SV_Logfile(PLAYER_LOG, false);
}
//<-

/*
============
SV_ModFragLogfile_f
============
*/
void SV_ModFragLogfile_f (void)
{
	SV_Logfile(MOD_FRAG_LOG, false);
}

log_t	logs[MAX_LOG] =
    {
        {NULL, "logfile",        "qconsole_", "File logging off.\n",          "console",  SV_Logfile_f,        0},
        {NULL, "logerrors",      "qerror_",   "Error logging off.\n",         "errors",   SV_ErrorLogfile_f,   0},
        {NULL, "logrcon",        "rcon_",     "Rcon logging off.\n",          "rcon",     SV_RconLogfile_f,    0},
        {NULL, "logtelnet",      "qtelnet_",  "Telnet logging off.\n",        "telnet",   SV_TelnetLogfile_f,  0},
        {NULL, "fraglogfile",    "frag_",     "Frag file logging off.\n",     "frags",    SV_FragLogfile_f,    0},
        {NULL, "logplayers",     "player_",   "Player logging off.\n",        "players",  SV_PlayerLogfile_f,  0},//bliP: player logging
        {NULL, "modfraglogfile", "modfrag_",  "Mod frag file logging off.\n", "modfrags", SV_ModFragLogfile_f, 0}
    };

/*
==================
SV_SetPlayer
 
Sets sv_client and sv_player to the player with idnum Cmd_Argv(1)
==================
*/
qbool SV_SetPlayer (void)
{
	client_t	*cl;
	int			i;
	int			idnum;

	idnum = Q_atoi(Cmd_Argv(1));

	// HACK: for cheat commands which comes from client rather than from server console
	if (sv_client && sv_redirected == RD_CLIENT)
	{
		idnum = sv_client->userid;
	}

	for (i=0,cl=svs.clients ; i<MAX_CLIENTS ; i++,cl++)
	{
		if (!cl->state)
			continue;
		if (cl->userid == idnum)
		{
			sv_client = cl;
			sv_player = sv_client->edict;
			return true;
		}
	}
	Con_Printf ("Userid %i is not on the server\n", idnum);
	return false;
}


/*
==================
SV_God_f
 
Sets client to godmode
==================
*/
void SV_God_f (void)
{
	if (!sv_allow_cheats)
	{
		Con_Printf ("Cheats are not allowed on this server\n");
		return;
	}

	if (!SV_SetPlayer ())
		return;

	sv_player->v->flags = (int)sv_player->v->flags ^ FL_GODMODE;
	if (!((int)sv_player->v->flags & FL_GODMODE) )
		SV_ClientPrintf (sv_client, PRINT_HIGH, "godmode OFF\n");
	else
		SV_ClientPrintf (sv_client, PRINT_HIGH, "godmode ON\n");
}


void SV_Noclip_f (void)
{
	if (!sv_allow_cheats)
	{
		Con_Printf ("Cheats are not allowed on this server\n");
		return;
	}

	if (!SV_SetPlayer ())
		return;

	if (sv_player->v->movetype != MOVETYPE_NOCLIP)
	{
		sv_player->v->movetype = MOVETYPE_NOCLIP;
		SV_ClientPrintf (sv_client, PRINT_HIGH, "noclip ON\n");
	}
	else
	{
		sv_player->v->movetype = MOVETYPE_WALK;
		SV_ClientPrintf (sv_client, PRINT_HIGH, "noclip OFF\n");
	}
}


/*
==================
SV_Give_f
==================
*/
void SV_Give_f (void)
{
	char	*t;
	int		v, cnt;

	if (!sv_allow_cheats)
	{
		Con_Printf ("Cheats are not allowed on this server\n");
		return;
	}

	if (!SV_SetPlayer ())
		return;

	// HACK: for cheat commands which comes from client rather than from server console
	cnt = (sv_redirected == RD_CLIENT ? 1 : 2);

	t = Cmd_Argv(cnt++);
	v = Q_atoi (Cmd_Argv(cnt++));

	switch (t[0])
	{
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		sv_player->v->items = (int)sv_player->v->items | IT_SHOTGUN<< (t[0] - '2');
		break;

	case 's':
		sv_player->v->ammo_shells = v;
		break;
	case 'n':
		sv_player->v->ammo_nails = v;
		break;
	case 'r':
		sv_player->v->ammo_rockets = v;
		break;
	case 'h':
		sv_player->v->health = v;
		break;
	case 'c':
		sv_player->v->ammo_cells = v;
		break;
	}
}

void SV_Fly_f (void)
{
	if (!sv_allow_cheats)
	{
		Con_Printf ("Cheats are not allowed on this server\n");
		return;
	}

	if (!SV_SetPlayer ())
		return;

	if (sv_player->v->solid != SOLID_SLIDEBOX)
		return;		// dead don't fly

	if (sv_player->v->movetype != MOVETYPE_FLY)
	{
		sv_player->v->movetype = MOVETYPE_FLY;
		SV_ClientPrintf (sv_client, PRINT_HIGH, "flymode ON\n");
	}
	else
	{
		sv_player->v->movetype = MOVETYPE_WALK;
		SV_ClientPrintf (sv_client, PRINT_HIGH, "flymode OFF\n");
	}
}

/*
======================
SV_Map_f

handle a 
map <mapname>
command from the console or progs.
======================
*/
void SV_Map (qbool now)
{
	static char     level[MAX_QPATH];
	static char     expanded[MAX_QPATH];
	static char     entityfile[MAX_QPATH];
	static qbool    changed = false;

	char	*s;

	// if now, change it
	if (now)
	{
		if (!changed)
			return;

		changed = false;

		if (!FS_FLocateFile(expanded, FSLFRT_IFFOUND, NULL))
		{
			Sys_Printf ("Can't find %s\n", expanded);
			return;
		}

		if (sv.mvdrecording)
			SV_MVDStop_f();

#ifndef SERVERONLY
		CL_BeginLocalConnection ();
#endif

		// -> scream
		if ((int)frag_log_type.value)
		{
			//bliP: date check ->
			date_t date;
			SV_TimeOfDay(&date, "%a %b %d, %H:%M:%S %Y");
			s = va("\\newmap\\%s\\\\\\\\%d-%d-%d %d:%d:%d\\\n",
			       level,
			       date.year,
			       date.mon+1,
			       //bliP: check me - date.mon or date.mon+1? existing code was date.mon+1
			       date.day,
			       date.hour,
			       date.min,
			       date.sec);
			//<-
			if (logs[FRAG_LOG].sv_logfile)
				SZ_Print (&svs.log[svs.logsequence&1], s);
			SV_Write_Log(FRAG_LOG, 0, s);
		}
		// <-

		SV_SpawnServer (level, !strcasecmp(Cmd_Argv(0), "devmap"), entityfile, false);

#ifdef SERVERONLY
		SV_BroadcastCommand ("changing\n"
		                     "reconnect\n");
		SV_SendMessagesToAll ();
#else
		SV_BroadcastCommand ("reconnect\n");
#endif

		return;
	}

	// get the map name, but don't change now, could be executed from progs.dat

	if (Cmd_Argc() < 2 || Cmd_Argc() > 3)
	{
		Con_Printf ("map <levelname> [<entityfile>] : continue game on a new level\n");
		return;
	}

	strlcpy (level, Cmd_Argv(1), MAX_QPATH);

	memset(entityfile, 0, sizeof(entityfile));
	if (Cmd_Argc() >= 3)
		strlcpy (entityfile, Cmd_Argv(2), MAX_QPATH);

	// check to make sure the level exists
	snprintf (expanded, MAX_QPATH, "maps/%s.bsp", level);

	// Flush FS cache on each map change.
	FS_FlushFSHash();

	if (!FS_FLocateFile(expanded, FSLFRT_IFFOUND, NULL))
	{
		Con_Printf ("Can't find %s\n", expanded);
		return;
	}

	changed = true;
#ifndef SERVERONLY
	if (!com_serveractive) {
		Host_EndGame();
	}
	com_serveractive = true;
#endif
}

void SV_Map_f (void)
{
	SV_Map(false);
}

/*==================
SV_ReplaceChar
Replace char in string
==================*/
void SV_ReplaceChar(char *s, char from, char to)
{
	if (s)
		for ( ;*s ; ++s)
			if (*s == from)
				*s = to;
}
//bliP: ls, rm, rmdir, chmod ->
/*==================
SV_ListFiles_f
Lists files
==================*/
void SV_ListFiles_f (void)
{
	dir_t	dir;
	file_t	*list;
	char	*key;
	char	*dirname;
	int	i;

	if (Cmd_Argc() < 2)
	{
		Con_Printf ("ls <directory> <match>\n");
		return;
	}

	dirname = Cmd_Argv(1);
	SV_ReplaceChar(dirname, '\\', '/');

	// Double-check then move to FS_UnsafeFilename() ?
	if (	!strncmp(dirname, "../", 3) || strstr(dirname, "/../") || *dirname == '/'
	        ||	( (i = strlen(dirname)) < 3 ? 0 : !strncmp(dirname + i - 3, "/..", 4) )
	        ||	!strncmp(dirname, "..", 3)
#ifdef _WIN32
	        ||	( dirname[1] == ':' && ((*dirname >= 'a' && *dirname <= 'z') ||
	                                   (*dirname >= 'A' && *dirname <= 'Z'))
	           )
#endif //_WIN32
	   )
	{
		Con_Printf("Unable to list %s\n", dirname);
		return;
	}

	Con_Printf("Content of %s/*.*\n", dirname);
	dir = Sys_listdir(va("%s", dirname), ".*", SORT_BY_NAME);
	list = dir.files;
	if (!list->name[0])
	{
		Con_Printf("No files\n");
		return;
	}

	key = (Cmd_Argc() == 3) ? Cmd_Argv(2) : (char *) "";

	//directories...
	for (; list->name[0]; list++)
	{
		if (!strstr(list->name, key) || !list->isdir)
			continue;
		Con_Printf("- %s\n", list->name);
	}

	list = dir.files;

	//files...
	for (; list->name[0]; list++)
	{
		if (!strstr(list->name, key) || list->isdir)
			continue;
		if ((int)list->size / 1024 > 0)
			Con_Printf("%s %.0fKB (%.2fMB)\n", list->name,
			           (float)list->size / 1024, (float)list->size / 1024 / 1024);
		else
			Con_Printf("%s %dB\n", list->name, list->size);
	}
	Con_Printf("Total: %d files, %.0fKB (%.2fMB)\n", dir.numfiles,
	           (float)dir.size / 1024, (float)dir.size / 1024 / 1024);
}

/*==================
SV_RemoveDirectory_f
Removes an empty directory
==================*/
void SV_RemoveDirectory_f (void)
{
	char	*dirname;

	if (Cmd_Argc() != 2)
	{
		Con_Printf("rmdir <directory>\n");
		return;
	}

	dirname = Cmd_Argv(1);
	SV_ReplaceChar(dirname, '\\', '/');

	if (	!strncmp(dirname, "../", 3) || strstr(dirname, "/../") || *dirname == '/'
#ifdef _WIN32
	        ||	( dirname[1] == ':' && ((*dirname >= 'a' && *dirname <= 'z') ||
	                                   (*dirname >= 'A' && *dirname <= 'Z'))
	           )
#endif //_WIN32
	   )
	{
		Con_Printf("Unable to remove\n");
		return;
	}

	if (!Sys_rmdir(dirname))
		Con_Printf("Directory %s successfully removed\n", dirname);
	else
		Con_Printf("Unable to remove directory %s\n", dirname);
}

/*==================
SV_RemoveFile_f
Remove a file
==================*/
void SV_RemoveFile_f (void)
{
	char *dirname;
	char *filename;
	int i;

	if (Cmd_Argc() < 3)
	{
		Con_Printf("rm <directory> {<filename> | *<token> | *} - removes a file | with token | all\n");
		return;
	}

	dirname = Cmd_Argv(1);
	filename = Cmd_Argv(2);
	SV_ReplaceChar(dirname, '\\', '/');
	SV_ReplaceChar(filename, '\\', '/');

	if (	!strncmp(dirname, "../", 3) || strstr(dirname, "/../")
	        ||	*dirname == '/'             || strchr(filename, '/')
	        ||	( (i = strlen(filename)) < 3 ? 0 : !strncmp(filename + i - 3, "/..", 4) )
#ifdef _WIN32
	        ||	( dirname[1] == ':' && ((*dirname >= 'a' && *dirname <= 'z') ||
	                                   (*dirname >= 'A' && *dirname <= 'Z'))
	           )
#endif //_WIN32
	   )
	{
		Con_Printf("Unable to remove\n");
		return;
	}

	if (*filename == '*') //token, many files
	{
		dir_t dir;
		file_t *list;

		// remove all files with specified token
		filename++;

		dir = Sys_listdir(va("%s", dirname), ".*", SORT_BY_NAME);
		list = dir.files;
		for (i = 0; list->name[0]; list++)
		{
			if (!list->isdir && strstr(list->name, filename))
			{
				if (!Sys_remove(va("%s/%s", dirname, list->name)))
				{
					Con_Printf("Removing %s...\n", list->name);
					i++;
				}
			}
		}
		if (i)
			Con_Printf("%d files removed\n", i);
		else
			Con_Printf("No matching found\n");
	}
	else // 1 file
	{
		if (!Sys_remove(va("%s/%s", dirname, filename)))
			Con_Printf("File %s successfully removed\n", filename);
		else
			Con_Printf("Unable to remove file %s\n", filename);
	}

	// force cache rebuild.
	FS_FlushFSHash();
}

/*==================
SV_ChmodFile_f
Chmod a script
==================*/
#ifndef _WIN32
void SV_ChmodFile_f (void)
{
	char	*_mode, *filename;
	unsigned int	mode, m;

	if (Cmd_Argc() != 3)
	{
		Con_Printf("chmod <mode> <file>\n");
		return;
	}

	_mode = Cmd_Argv(1);
	filename = Cmd_Argv(2);

	if (!strncmp(filename, "../",  3) || strstr(filename, "/../") ||
	        *filename == '/'              || strlen(_mode) != 3 ||
	        ( (m = strlen(filename)) < 3 ? 0 : !strncmp(filename + m - 3, "/..", 4) ))
	{
		Con_Printf("Unable to chmod\n");
		return;
	}
	for (mode = 0; *_mode; _mode++)
	{
		m = *_mode - '0';
		if (m > 7)
		{
			Con_Printf("Unable to chmod\n");
			return;
		}
		mode = (mode << 3) + m;
	}

	if (chmod(filename, mode))
		Con_Printf("Unable to chmod %s\n", filename);
	else
		Con_Printf("Chmod %s successful\n", filename);
}
#endif //_WIN32

/*==================
SV_LocalCommand_f
Execute system command
==================*/
//bliP: REMOVE ME REMOVE ME REMOVE ME REMOVE ME REMOVE ME ->
void SV_LocalCommand_f (void)
{
	int i, c;
	char str[1024], *temp_file = "__output_temp_file__";

	if ((c = Cmd_Argc()) < 2)
	{
		Con_Printf("localcommand [command]\n");
		return;
	}

	str[0] = 0;
	for (i = 1; i < c; i++)
	{
		strlcat (str, Cmd_Argv(i), sizeof(str));
		strlcat (str, " ", sizeof(str));
	}
	strlcat (str, va("> %s 2>&1\n", temp_file), sizeof(str));

	if (system(str) == -1)
		Con_Printf("command failed\n");
	else
	{
		char	buf[512];
		FILE	*f;
		if ((f = fopen(temp_file, "rt")) == NULL)
			Con_Printf("(empty)\n");
		else
		{
			while (!feof(f))
			{
				buf[fread (buf, 1, sizeof(buf) - 1, f)] = 0;
				Con_Printf("%s", buf);
			}
			fclose(f);
			if (Sys_remove(temp_file))
				Con_Printf("Unable to remove file %s\n", temp_file);
		}
	}

}
//REMOVE ME REMOVE ME REMOVE ME REMOVE ME REMOVE ME

/*
==================
SV_Kick_f
 
Kick a user off of the server
==================
*/
void SV_Kick_f (void)
{
	int			i, j;
	client_t	*cl;
	int			uid;
	int			c;
	char		reason[80] = "";

	c = Cmd_Argc ();
	if (c < 2)
	{
#ifndef SERVERONLY
		// some mods use a "kick" alias for their own needs, sigh
		if (CL_ClientState() && Cmd_FindAlias("kick"))
		{
			Cmd_ExecuteString (Cmd_AliasString("kick"));
			return;
		}
#endif
		Con_Printf ("kick <userid> [reason]\n");
		return;
	}

	uid = Q_atoi(Cmd_Argv(1));

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
	{
		if (!cl->state)
			continue;
		if (cl->userid == uid)
		{
			if (c > 2)
			{
				strlcpy (reason, " (", sizeof(reason));
				for (j=2 ; j<c; j++)
				{
					strlcat (reason, Cmd_Argv(j), sizeof(reason)-4);
					if (j < c-1)
						strlcat (reason, " ", sizeof(reason)-4);
				}
				if (strlen(reason) < 3)
					reason[0] = '\0';
				else
					strlcat (reason, ")", sizeof(reason));
			}

			SV_KickClient(cl, reason);
			return;
		}
	}

	Con_Printf ("Couldn't find user number %i\n", uid);
}

void SV_KickClient(client_t* cl, const char* reason)
{
	sv_client_state_t saved_state = cl->state;
	cl->state = cs_free; // HACK: don't broadcast to this client
	SV_BroadcastPrintf(PRINT_HIGH, "%s was kicked%s\n", cl->name, reason);
	cl->state = (sv_client_state_t)saved_state;
	SV_ClientPrintf(cl, PRINT_HIGH, "You were kicked from the game%s\n", reason);
	SV_LogPlayer(cl, va("kick%s\n", reason), 1); //bliP: logging
	SV_DropClient(cl);
}

//bliP: mute, cuff ->
int SV_MatchUser (char *s)
{
	int         i;
	client_t   *cl;

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
	{
		if (!cl->state)
			continue;
		if (!strcmp (cl->name, s))
			return cl->userid;
	}
	i = Q_atoi(s);
	return i;
}

/*
================
SV_Cuff_f
================
*/
#define MAXPENALTY 1200.0
void SV_Cuff_f (void)
{
	int         c, i, uid;
	double      mins = 0.5;
	qbool    done = false;
	qbool    print = true;
	client_t    *cl;
	char        reason[80];
	char        text[100];

	if ((c = Cmd_Argc()) < 2)
	{
		Con_Printf ("usage: cuff <userid/name> <minutes> [reason]\n(default = 0.5, 0 = cancel cuff).\n");
		return;
	}

	uid = SV_MatchUser(Cmd_Argv(1));
	if (!uid)
	{
		Con_Printf ("Couldn't find user %s\n", Cmd_Argv(1));
		return;
	}

	if (c >= 3)
	{
		mins = Q_atof(Cmd_Argv(2));
		if (mins < 0.0 || mins > MAXPENALTY)
		{
			mins = MAXPENALTY;
		}
	}

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
	{
		if (!cl->state)
			continue;
		if (cl->userid == uid)
		{
			cl->cuff_time = curtime + (mins * 60.0);
			done = true;
			break;
		}
	}
	if (done)
	{
		reason[0] = 0;
		if (c > 2)
		{
			for (i = 3; i < c; i++)
			{
				strlcat (reason, Cmd_Argv(i), sizeof(reason) - 1 - strlen(reason));
				if (i < c - 1)
					strlcat (reason, " ", sizeof(reason) - strlen(reason));
			}
		}

		if (mins)
		{
			SV_BroadcastPrintf (PRINT_CHAT, "%s cuffed for %.1f minutes%s%s\n", cl->name, mins, reason[0] ? ": " : "", reason[0] ? reason : "");

			snprintf(text, sizeof(text), "You are cuffed for %.1f minutes%s%s\n", mins, reason[0] ? "\n\n" : "", reason[0] ? reason : "");
			ClientReliableWrite_Begin(cl,svc_centerprint, 2+strlen(text));
			ClientReliableWrite_String (cl, text);
		}
		else
		{
			if (print)
			{
				SV_BroadcastPrintf (PRINT_CHAT, "%s un-cuffed.\n", cl->name);
			}
		}
	}
	else
	{
		Con_Printf ("Couldn't find user %s\n", Cmd_Argv(1));
	}
}

/*
================
SV_Mute_f
================
*/
void SV_Mute_f (void)
{
	int         c, i, uid;
	double      mins = 0.5;
	qbool    done = false;
	qbool    print = true;
	client_t    *cl;
	char        reason[1024];
	char        text[1024];
	char        *ptr;

	if ((c = Cmd_Argc()) < 2)
	{
		Con_Printf ("usage: mute <userid/name> <minutes> [reason]\n(default = 0.5, 0 = cancel mute).\n");
		return;
	}

	uid = SV_MatchUser(Cmd_Argv(1));
	if (!uid)
	{
		Con_Printf ("Couldn't find user %s\n", Cmd_Argv(1));
		return;
	}

	if (c >= 3)
	{
		ptr = Cmd_Argv(2);
		if (*ptr == '*')
		{
			ptr++;
			print = false;
		}
		mins = Q_atof(ptr);
		if (mins < 0.0 || mins > MAXPENALTY)
			mins = MAXPENALTY;
	}
	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
	{
		if (!cl->state)
			continue;
		if (cl->userid == uid)
		{
			cl->lockedtill = curtime + (mins * 60.0);
			done = true;
			break;
		}
	}
	if (done)
	{
		reason[0] = 0;
		if (c > 2)
		{
			for (i = 3; i < c; i++)
			{
				strlcat (reason, Cmd_Argv(i), sizeof(reason) - 1 - strlen(reason));
				if (i < c - 1)
					strlcat (reason, " ", sizeof(reason) - strlen(reason));
			}
		}

		if (mins)
		{
			if (print)
				SV_BroadcastPrintf (PRINT_CHAT, "%s muted for %.1f minutes%s%s\n", cl->name, mins, reason[0] ? ": " : "", reason[0] ? reason : "");
			snprintf(text, sizeof(text), "You are muted for %.1f minutes%s%s\n", mins, reason[0] ? "\n\n" : "", reason[0] ? reason : "");
			ClientReliableWrite_Begin(cl, svc_centerprint, 2+strlen(text));
			ClientReliableWrite_String (cl, text);
		}
		else
		{
			if (print)
			{
				SV_BroadcastPrintf (PRINT_CHAT, "%s un-muted.\n", cl->name);
			}
		}
	}
	else
	{
		Con_Printf ("Couldn't find user %s\n", Cmd_Argv(1));
	}
}

void SV_RemovePenalty_f (void)
{
	int     i;
	int     num;
	extern int numpenfilters;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("penaltyremove [num]\n");
		return;
	}

	num = Q_atoi(Cmd_Argv(1));

	for (i = 0; i < numpenfilters; i++)
	{
		if (i == num)
		{
			SV_RemoveIPFilter (i);
			Con_Printf ("Removed.\n");
			return;
		}
	}
	Con_Printf ("Didn't find penalty filter %i.\n", num);
}

void SV_ListPenalty_f (void)
{
	client_t *cl;
	int     i;
	char		s[8];
	extern int numpenfilters;
	extern penfilter_t penfilters[MAX_PENFILTERS];

	Con_Printf ("Active Penalty List:\n");
	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
	{
		if (!cl->state)
			continue;

		if (cl->lockedtill >= curtime)
		{
			Con_Printf ("%i %s mute (remaining: %d)\n", cl->userid, cl->name, (cl->lockedtill) ? (int)(cl->lockedtill - curtime) : 0);
		}
		if (cl->cuff_time >= curtime)
		{
			Con_Printf ("%i %s cuff (remaining: %d)\n", cl->userid, cl->name, (cl->cuff_time) ? (int)(cl->cuff_time - curtime) : 0);
		}
	}

	Con_Printf ("Saved Penalty List:\n");
	for (i = 0; i < numpenfilters; i++)
	{
		switch (penfilters[i].type)
		{
		case ft_mute: strlcpy(s, "Mute", sizeof(s)); break;
		case ft_cuff: strlcpy(s, "Cuff", sizeof(s)); break;
		default: strlcpy(s, "Unknown", sizeof(s)); break;
		}
		Con_Printf ("%i: %s for %i.%i.%i.%i (remaining: %d)\n", i, s,
		            penfilters[i].ip[0],
		            penfilters[i].ip[1],
		            penfilters[i].ip[2],
		            penfilters[i].ip[3],
		            (penfilters[i].time) ? (int)(penfilters[i].time - realtime) : 0);
	}
}
//<-

/*
================
SV_Resolve
 
resolve IP via DNS lookup
================
*/
char *SV_Resolve(char *addr)
{
#if defined (__linux__) || defined (_WIN32)
	unsigned long ip;
#else
	in_addr_t ip;
#endif

	struct hostent *hp;

	ip = inet_addr(addr);
	if ((hp = gethostbyaddr((const char *)&ip, sizeof(ip), AF_INET)) != NULL)
		addr = hp->h_name;
	return addr;
}

/*
==================
SV_Nslookup_f
==================
*/
void SV_Nslookup_f (void)
{
	char		*ip, *name;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("Usage: nslookup <IP address>\n");
		return;
	}

	ip = Cmd_Argv(1);
	name = SV_Resolve(ip);
	if (ip != name)
		Con_Printf ("Name:    %s\nAddress:  %s\n", name, ip);
	else
		Con_Printf ("Couldn't resolve %s\n", ip);

}

/*
================
SV_Status_f
================
*/
extern cvar_t sv_use_dns;
void SV_Status_f (void)
{
	int i;
	client_t *cl;
	float cpu, avg, pak, demo1 = 0.0;
	char *s;

	cpu = (svs.stats.latched_active + svs.stats.latched_idle);

	if (cpu)
	{
		demo1 = 100.0 * svs.stats.latched_demo  / cpu;
		cpu  = 100.0 * svs.stats.latched_active / cpu;
	}

	avg = 1000 * svs.stats.latched_active  / STATFRAMES;
	pak = (float)svs.stats.latched_packets / STATFRAMES;

	Con_Printf ("net address                 : %s\n"
				"cpu utilization (overall)   : %3i%%\n"
				"cpu utilization (recording) : %3i%%\n"
				"avg response time           : %i ms\n"
				"packets/frame               : %5.2f (%d)\n",
				NET_AdrToString (net_local_sv_ipadr),
				(int)cpu,
				(int)demo1,
				(int)avg,
				pak, num_prstr);

	switch (sv_redirected)
	{
		case RD_NONE:
			Con_Printf ("name             ping frags   id   address                real ip\n"
						"---------------- ---- ----- ------ ---------------------- ---------------\n");
			for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
			{
				if (!cl->state)
					continue;
				s = NET_BaseAdrToString(cl->netchan.remote_address);
				Con_Printf ("%-16s %4i %5i %6i %-22s ", cl->name, (int)SV_CalcPing(cl),
						(int)cl->edict->v->frags, cl->userid, (int)sv_use_dns.value ? SV_Resolve(s) : s);
				if (cl->realip.ip[0])
					Con_Printf ("%-15s", NET_BaseAdrToString (cl->realip));
				Con_Printf (cl->spectator ? (char *) "(s)" : (char *) "");

				switch (cl->state)
				{
					case cs_connected:
					case cs_preconnected:
						Con_Printf (" CONNECTING\n");
						continue;
					case cs_zombie:
						Con_Printf (" ZOMBIE\n");
						continue;
					default:
						Con_Printf ("\n");
				}
			}
			break;
		//case RD_MOD:
		//case RD_CLIENT:
		//case RD_PACKET:
		default:
			// most remote clients are 40 columns
			//           01234567890123456789012345678901234567890123456789
			Con_Printf ("name               ping frags   id\n");
			Con_Printf ("  address\n");
			Con_Printf ("  real ip\n");
			Con_Printf ("------------------ ---- ----- ------\n");
			for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
			{
				if (!cl->state)
					continue;

				s = NET_BaseAdrToString(cl->netchan.remote_address);
				Con_Printf ("%-18s %4i %5i %6s %s\n%-36s\n", cl->name, (int)SV_CalcPing(cl),
							(int)cl->edict->v->frags, Q_yelltext((unsigned char*)va("%d", cl->userid)),
							cl->spectator ? " (s)" : "", (int)sv_use_dns.value ? SV_Resolve(s) : s);

				if (cl->realip.ip[0])
					Con_Printf ("%-36s\n", NET_BaseAdrToString (cl->realip));

				switch (cl->state)
				{
					case cs_connected:
					case cs_preconnected:
						Con_Printf ("CONNECTING\n");
						continue;
					case cs_zombie:
						Con_Printf ("ZOMBIE\n");
						continue;
					default:;
				}
			}
	} // switch
	Con_Printf ("\n");
}

/*
==================
SV_Check_maps_f
==================
*/
void SV_Check_maps_f(void)
{
	dir_t d;
	file_t *list;
	int i, j, maps_id1;

	d = Sys_listdir("id1/maps", ".bsp$", SORT_BY_NAME);
	list = d.files;
	for (i = LOCALINFO_MAPS_LIST_START; list->name[0] && i <= LOCALINFO_MAPS_LIST_END; list++, i++)
	{
		list->name[strlen(list->name) - 4] = 0;
		if (!list->name[0])
			continue;

		SV_Localinfo_Set(va("%d", i), list->name);
	}
	maps_id1 = i - 1;

	d = Sys_listdir("qw/maps", ".bsp$", SORT_BY_NAME);
	list = d.files;
	for (; list->name[0] && i <= LOCALINFO_MAPS_LIST_END; list++, i++)
	{
		list->name[strlen(list->name) - 4] = 0;
		if (!list->name[0])
			continue;

		for (j = LOCALINFO_MAPS_LIST_START; j <= maps_id1; j++)
			if (!strncmp(Info_Get(&_localinfo_, va("%d", j)), list->name, MAX_KEY_STRING))
				break;
		if (j <= maps_id1)
			continue;

		SV_Localinfo_Set(va("%d", i), list->name);
	}

	for (; i <= LOCALINFO_MAPS_LIST_END; i++)
	{
		SV_Localinfo_Set(va("%d", i), "");
	}
}

/*
==================
SV_ConSay_f
==================
*/
void SV_ConSay_f(void)
{
	client_t *client;
	int		j;
	char	*p;
	char	text[1024] = "console: ";

	if (Cmd_Argc () < 2)
		return;

	p = Cmd_Args();

	if (*p == '"')
	{
		p++;
		p[strlen(p)-1] = 0;
	}

	strlcat(text,    p, sizeof(text));
	strlcat(text, "\n", sizeof(text));

	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++)
	{
		if (client->state != cs_spawned)
			continue;
		SV_ClientPrintf2(client, PRINT_CHAT, "%s", text);
	}

	if (sv.mvdrecording)
	{
		if (MVDWrite_Begin (dem_all, 0, strlen(text)+3))
		{
			MVD_MSG_WriteByte (svc_print);
			MVD_MSG_WriteByte (PRINT_CHAT);
			MVD_MSG_WriteString (text);
		}
	}

	Sys_Printf("%s", text);
	SV_Write_Log(CONSOLE_LOG, 1, text);
}

void SV_SendServerInfoChange(char *key, char *value)
{
	if (!sv.state)
		return;

	MSG_WriteByte (&sv.reliable_datagram, svc_serverinfo);
	MSG_WriteString (&sv.reliable_datagram, key);
	MSG_WriteString (&sv.reliable_datagram, value);
}

//Cvar system calls this when a CVAR_SERVERINFO cvar changes
void SV_ServerinfoChanged (char *key, char *string)
{
	string = Cvar_ServerInfoValue(key, string);

	if (strcmp(string, Info_ValueForKey (svs.info, key))) {
		Info_SetValueForKey (svs.info, key, string, MAX_SERVERINFO_STRING);
		SV_SendServerInfoChange (key, string);
	}
}

/*
===========
SV_Serverinfo_f

Examine or change the serverinfo string
===========
*/
void SV_Serverinfo_f (void)
{
	cvar_t	*var;
	char *s;
	char	*key, *value;

	if (Cmd_Argc() == 1)
	{
		Con_Printf ("Server info settings:\n");
		Info_Print (svs.info);
		Con_Printf ("[%d/%d]\n", strlen(svs.info), MAX_SERVERINFO_STRING);
		return;
	}

	//bliP: sane serverinfo usage (mercury) ->
	if (Cmd_Argc() == 2)
	{
		s = Info_ValueForKey(svs.info, Cmd_Argv(1));
		if (*s)
			Con_Printf ("Serverinfo %s: \"%s\"\n", Cmd_Argv(1), s);
		else
			Con_Printf ("No such key %s\n", Cmd_Argv(1));
		return;
	}
	//<-

	if (Cmd_Argc() != 3)
	{
		Con_Printf ("usage: serverinfo [ <key> [ <value> ] ]\n");
		return;
	}

	key = Cmd_Argv(1);
	value = Cmd_Argv(2);

	if (key[0] == '*')
	{
		Con_Printf ("Star variables cannot be changed.\n");
		return;
	}

	// force serverinfo "0" vars to be ""
	if (!strcmp(value, "0"))
		value = "";

	// if the key is also a serverinfo cvar, change it too
	var = Cvar_Find(key);
	if (var && (var->flags & CVAR_SERVERINFO))
	{
		Cvar_Set (var, value); // this call SV_ServerinfoChanged() as well.
	}
	else
	{
		SV_ServerinfoChanged(key, value);
	}
}

void SV_Localinfo_Set (const char *name, const char *value)
{
	char *old_value;

	if (!name || !*name)
		return;

	if (!value)
		value = "";

	old_value = Info_Get(&_localinfo_, name); // remember old value.
	Info_Set (&_localinfo_, name, value); // set new value.

	if (mod_localinfoChanged)
	{
		pr_global_struct->time = sv.time;
		pr_global_struct->self = 0;
		PR_SetTmpString(&G_INT(OFS_PARM0), name);
		PR_SetTmpString(&G_INT(OFS_PARM1), old_value);
		PR_SetTmpString(&G_INT(OFS_PARM2), Info_Get(&_localinfo_, name));
		PR_ExecuteProgram (mod_localinfoChanged);
	}
}

/*
===========
SV_Localinfo_f
 
  Examine or change the localinfo string
===========
*/
void SV_Localinfo_f (void)
{
	if (Cmd_Argc() == 1)
	{
		char info[MAX_LOCALINFO_STRING];

		Con_Printf ("Local info settings:\n");
		Info_ReverseConvert(&_localinfo_, info, sizeof(info));
		Info_Print (info);
		Con_Printf ("[%d/%d]\n", strlen(info), sizeof(info));
		return;
	}

	//bliP: sane localinfo usage (mercury) ->
	if (Cmd_Argc() == 2)
	{
		char *s = Info_Get(&_localinfo_, Cmd_Argv(1));

		if (*s)
			Con_Printf ("Localinfo %s: \"%s\"\n", Cmd_Argv(1), s);
		else
			Con_Printf ("No such key %s\n", Cmd_Argv(1));
		return;
	}
	//<-

	if (Cmd_Argc() != 3)
	{
		Con_Printf ("usage: localinfo [ <key> <value> ]\n");
		return;
	}

	if (Cmd_Argv(1)[0] == '*')
	{
		Con_Printf ("Star variables cannot be changed.\n");
		return;
	}

	SV_Localinfo_Set(Cmd_Argv(1), Cmd_Argv(2));
}


/*
===========
SV_User_f
 
Examine a users info strings
===========
*/
void SV_User_f (void)
{
	char info[MAX_EXT_INFO_STRING];

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("Usage: user <userid>\n");
		return;
	}

	if (!SV_SetPlayer ())
		return;

	Info_ReverseConvert(&sv_client->_userinfo_ctx_, info, sizeof(info));
	Info_Print(info);
	Con_DPrintf ("[%d/%d]\n", strlen(info), sizeof(info));
}

/*
================
SV_Gamedir
 
Sets the fake *gamedir to a different directory.
================
*/
void SV_Gamedir (void)
{
	char			*dir;

	if (Cmd_Argc() == 1)
	{
		Con_Printf ("Current *gamedir: %s\n", Info_ValueForKey (svs.info, "*gamedir"));
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("Usage: sv_gamedir <newgamedir>\n");
		return;
	}

	dir = Cmd_Argv(1);

	if (strstr(dir, "..") || strchr(dir, '/')
	        || strchr(dir, '\\') || strchr(dir, ':') )
	{
		Con_Printf ("*Gamedir should be a single filename, not a path\n");
		return;
	}

	Info_SetValueForStarKey (svs.info, "*gamedir", dir, MAX_SERVERINFO_STRING);
}

/*
================
SV_Floodport_f
 
Sets the gamedir and path to a different directory.
================
*/

void SV_Floodprot_f (void)
{
	int arg1, arg2, arg3;

	if (Cmd_Argc() == 1)
	{
		if (fp_messages)
		{
			Con_Printf ("Current floodprot settings: \nAfter %d msgs per %d seconds, silence for %d seconds\n",
			            fp_messages, fp_persecond, fp_secondsdead);
			return;
		}
		else
			Con_Printf ("No floodprots enabled.\n");
	}

	if (Cmd_Argc() != 4)
	{
		Con_Printf ("Usage: floodprot <# of messages> <per # of seconds> <seconds to silence>\n");
		Con_Printf ("Use floodprotmsg to set a custom message to say to the flooder.\n");
		return;
	}

	arg1 = Q_atoi(Cmd_Argv(1));
	arg2 = Q_atoi(Cmd_Argv(2));
	arg3 = Q_atoi(Cmd_Argv(3));

	if (arg1<=0 || arg2 <= 0 || arg3<=0)
	{
		Con_Printf ("All values must be positive numbers\n");
		return;
	}

	if (arg1 > 10)
	{
		Con_Printf ("Can only track up to 10 messages.\n");
		return;
	}

	fp_messages	= arg1;
	fp_persecond = arg2;
	fp_secondsdead = arg3;
}

void SV_Floodprotmsg_f (void)
{
	if (Cmd_Argc() == 1)
	{
		Con_Printf("Current msg: %s\n", fp_msg);
		return;
	}
	else if (Cmd_Argc() != 2)
	{
		Con_Printf("Usage: floodprotmsg \"<message>\"\n");
		return;
	}
	snprintf(fp_msg, sizeof(fp_msg), "%s", Cmd_Argv(1));
}

/*
================
SV_Gamedir_f
 
Sets the gamedir and path to a different directory.
================
*/
void SV_Gamedir_f (void)
{
	char			*dir;

	if (Cmd_Argc() == 1)
	{
		Con_Printf ("Current gamedir: %s\n", fs_gamedir);
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("Usage: gamedir <newdir>\n");
		return;
	}

	dir = Cmd_Argv(1);

	if (strstr(dir, "..") || strchr(dir, '/') || strchr(dir, '\\') || strchr(dir, ':') )
	{
		Con_Printf ("Gamedir should be a single filename, not a path\n");
		return;
	}

#ifndef SERVERONLY
	if (CL_ClientState())
	{
		Con_Printf ("you must disconnect before changing gamedir\n");
		return;
	}
#endif

	FS_SetGamedir (dir, false);
	Info_SetValueForStarKey (svs.info, "*gamedir", dir, MAX_SERVERINFO_STRING);
}

/*
================
SV_Snap
================
*/
void SV_Snap (int uid)
{
	client_t *cl;
	char pcxname[80];
	char checkname[MAX_OSPATH];
	int i;
	FILE *f;

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
	{
		if (cl->state < cs_preconnected)
			continue;
		if (cl->userid == uid)
			break;
	}
	if (i >= MAX_CLIENTS)
	{
		Con_Printf ("userid not found\n");
		return;
	}

	FS_CreatePath (va ("%s/snap/", fs_gamedir));
	snprintf (pcxname, sizeof (pcxname), "%d-00.pcx", uid);

	for (i=0 ; i<=99 ; i++)
	{
		pcxname[strlen(pcxname) - 6] = i/10 + '0';
		pcxname[strlen(pcxname) - 5] = i%10 + '0';
		snprintf (checkname, MAX_OSPATH, "%s/snap/%s", fs_gamedir, pcxname);
		f = fopen (checkname, "rb");
		if (!f)
			break; // file doesn't exist
		fclose (f);
	}
	if (i==100)
	{
		Con_Printf ("Snap: Couldn't create a file, clean some out.\n");
		return;
	}
	strlcpy(cl->uploadfn, checkname, MAX_QPATH);

	memcpy(&cl->snap_from, &net_from, sizeof(net_from));
	if (sv_redirected != RD_NONE)
		cl->remote_snap = true;
	else
		cl->remote_snap = false;

	ClientReliableWrite_Begin (cl, svc_stufftext, 24);
	ClientReliableWrite_String (cl, "cmd snap\n");
	Con_Printf ("Requesting snap from user %d...\n", uid);
}

/*
================
SV_Snap_f
================
*/
void SV_Snap_f (void)
{
	int			uid;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("Usage:  snap <userid>\n");
		return;
	}

	uid = Q_atoi(Cmd_Argv(1));

	SV_Snap(uid);
}

/*
================
SV_Snap
================
*/
void SV_SnapAll_f (void)
{
	client_t *cl;
	int			i;

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
	{
		if (cl->state < cs_preconnected || cl->spectator)
			continue;
		SV_Snap(cl->userid);
	}
}

// QW262 -->
/*
================
SV_MasterPassword
================
*/
void SV_MasterPassword_f (void)
{
#ifdef SERVERONLY
	if (!host_everything_loaded)
#else
	if (!server_cfg_done)
#endif
		strlcpy(master_rcon_password, Cmd_Argv(1), sizeof(master_rcon_password));
	else
		Con_DPrintf("master_rcon_password can be set only in server.cfg\n");
}
// <-- QW262

/*
==================
SV_InitOperatorCommands
==================
*/
void SV_InitOperatorCommands (void)
{
	int i;

	Cvar_Register (&sv_cheats);

	if (SV_CommandLineEnableCheats())
	{
		sv_allow_cheats = true;
		Cvar_SetValue (&sv_cheats, 1);
		Info_SetValueForStarKey (svs.info, "*cheats", "ON", MAX_SERVERINFO_STRING);
	}

	for (i = MIN_LOG; i < MAX_LOG; ++i)
		Cmd_AddCommand (logs[i].command, logs[i].function);

#ifdef SERVERONLY
	Cmd_AddCommand ("nslookup", SV_Nslookup_f);
#endif
	Cmd_AddCommand ("check_maps", SV_Check_maps_f);
	Cmd_AddCommand ("snap", SV_Snap_f);
	Cmd_AddCommand ("snapall", SV_SnapAll_f);
	Cmd_AddCommand ("kick", SV_Kick_f);

	// Add sv_status as client allows 'status' alias to over-ride (ezQuake #532)
	Cmd_AddCommand ("status", SV_Status_f);
	Cmd_AddCommand ("sv_status", SV_Status_f);

#ifdef SERVERONLY
	//bliP: init ->
	Cmd_AddCommand ("rmdir", SV_RemoveDirectory_f);
	Cmd_AddCommand ("rm", SV_RemoveFile_f);
	Cmd_AddCommand ("ls", SV_ListFiles_f);
#endif

	Cmd_AddCommand ("mute", SV_Mute_f);
	Cmd_AddCommand ("cuff", SV_Cuff_f);

	Cmd_AddCommand ("penaltylist", SV_ListPenalty_f);
	Cmd_AddCommand ("penaltyremove", SV_RemovePenalty_f);

#ifdef SERVERONLY
#ifndef _WIN32
	Cmd_AddCommand ("chmod", SV_ChmodFile_f);
#endif //_WIN32
	//<-
	if (SV_CommandLineEnableLocalCommand())
		Cmd_AddCommand ("localcommand", SV_LocalCommand_f);
#endif

	Cmd_AddCommand ("map", SV_Map_f);
#ifdef SERVERONLY
	Cmd_AddCommand ("devmap", SV_Map_f);
#else
	if (IsDeveloperMode()) {
		Cmd_AddCommand("devmap", SV_Map_f);
	}
#endif
	Cmd_AddCommand ("setmaster", SV_SetMaster_f);

	Cmd_AddCommand ("heartbeat", SV_Heartbeat_f);
	Cmd_AddCommand ("save", SV_SaveGame_f); 
	Cmd_AddCommand ("load", SV_LoadGame_f); 

#ifdef SERVERONLY
	Cmd_AddCommand ("say", SV_ConSay_f);
	Cmd_AddCommand ("quit", SV_Quit_f);
	Cmd_AddCommand ("restart", SV_Restart_f);
#endif

#ifdef SERVERONLY
	Cmd_AddCommand ("god", SV_God_f);
	Cmd_AddCommand ("give", SV_Give_f);
	Cmd_AddCommand ("noclip", SV_Noclip_f);
#endif

	Cmd_AddCommand ("localinfo", SV_Localinfo_f);

#ifdef SERVERONLY
	Cmd_AddCommand ("serverinfo", SV_Serverinfo_f);
	Cmd_AddCommand ("user", SV_User_f); // FIXME: probably should be done like CL_Serverinfo_f().
#endif

	Cmd_AddCommand ("gamedir", SV_Gamedir_f);
	Cmd_AddCommand ("sv_gamedir", SV_Gamedir);

// I wonder why it registered in host.c in ezquake...
#ifdef SERVERONLY
	Cmd_AddCommand ("floodprot", SV_Floodprot_f);
	Cmd_AddCommand ("floodprotmsg", SV_Floodprotmsg_f);
#endif

	Cmd_AddCommand ("master_rcon_password", SV_MasterPassword_f);
}

#endif // !CLIENTONLY
