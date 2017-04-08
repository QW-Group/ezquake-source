/*
 *  QW262
 *  Copyright (C) 2004  [sd] angel
 *
 *  This code is based on Q3 VM code by Id Software, Inc.
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 */

#ifndef __G_PUBLIC_H__
#define __G_PUBLIC_H__


// Copyright (C) 1999-2000 Id Software, Inc.
//
// g_public.h -- game module information visible to server

#define	GAME_API_VERSION	15


//===============================================================

// !!! new traps comes to end of list !!!

//
// system traps provided by the main engine
//
typedef enum
{
	//============== general Quake services ==================

	G_GETAPIVERSION,		// ( void);

	G_DPRINT,		// ( const char *string );
	// print message on the local console

	G_ERROR,		// ( const char *string );
	// abort the game
	G_GetEntityToken,

	G_SPAWN_ENT,
	G_REMOVE_ENT,
	G_PRECACHE_SOUND,
	G_PRECACHE_MODEL,
	G_LIGHTSTYLE,
	G_SETORIGIN,
	G_SETSIZE,
	G_SETMODEL,
	G_BPRINT,
	G_SPRINT,
	G_CENTERPRINT,
	G_AMBIENTSOUND,
	G_SOUND,
	G_TRACELINE,
	G_CHECKCLIENT,
	G_STUFFCMD,
	G_LOCALCMD,
	G_CVAR,
	G_CVAR_SET,
	G_FINDRADIUS,
	G_WALKMOVE,
	G_DROPTOFLOOR,
	G_CHECKBOTTOM,
	G_POINTCONTENTS,
	G_NEXTENT,
	G_AIM,
	G_MAKESTATIC,
	G_SETSPAWNPARAMS,
	G_CHANGELEVEL,
	G_LOGFRAG,
	G_GETINFOKEY,
	G_MULTICAST,
	G_DISABLEUPDATES,
	G_WRITEBYTE,     
	G_WRITECHAR,     
	G_WRITESHORT,    
	G_WRITELONG,     
	G_WRITEANGLE,    
	G_WRITECOORD,    
	G_WRITESTRING,   
	G_WRITEENTITY,
	G_FLUSHSIGNON,
	g_memset,
	g_memcpy,		
	g_strncpy,		
	g_sin,	
	g_cos,	
	g_atan2,	
	g_sqrt,	
	g_floor,	
	g_ceil,	
	g_acos,
	G_CMD_ARGC,
	G_CMD_ARGV,
	G_TraceCapsule,
	G_FSOpenFile,
	G_FSCloseFile,
	G_FSReadFile,
	G_FSWriteFile,
	G_FSSeekFile,
	G_FSTellFile,
	G_FSGetFileList,
	G_CVAR_SET_FLOAT,
	G_CVAR_STRING,
	G_Map_Extension,
	G_strcmp,
	G_strncmp,
	G_stricmp,
	G_strnicmp,
	G_Find,
	G_executecmd,
	G_conprint,
	G_readcmd,
	G_redirectcmd,
	G_Add_Bot,
	G_Remove_Bot,
	G_SetBotUserInfo,
	G_SetBotCMD,
	G_QVMstrftime,
	G_CMD_ARGS,
	G_CMD_TOKENIZE,
	g_strlcpy,
	g_strlcat,
	G_MAKEVECTORS,
	G_NEXTCLIENT,
	G_PRECACHE_VWEP_MODEL,
	G_SETPAUSE,
	G_SETUSERINFO,
	G_MOVETOGOAL,
} gameImport_t;

// !!! new things comes to end of list !!!

//
// functions exported by the game subsystem
//
typedef enum
{
	GAME_INIT,	// ( int levelTime, int randomSeed, int restart );
	// init and shutdown will be called every single level
	// The game should call G_GET_ENTITY_TOKEN to parse through all the
	// entity configuration text and spawn gentities.
	GAME_LOADENTS,
	GAME_SHUTDOWN,	// (void);

	GAME_CLIENT_CONNECT,	 	// ( int clientNum ,int isSpectator);
	// ( int clientNum, qbool firstTime, qbool isBot );
	// return NULL if the client is allowed to connect, otherwise return
	// a text string with the reason for denial
	GAME_PUT_CLIENT_IN_SERVER,
	//GAME_CLIENT_BEGIN,				// ( int clientNum ,int isSpectator);

	GAME_CLIENT_USERINFO_CHANGED,	// ( int clientNum,int isSpectator );

	GAME_CLIENT_DISCONNECT,			// ( int clientNum,int isSpectator );

	GAME_CLIENT_COMMAND,			// ( int clientNum,int isSpectator );

	GAME_CLIENT_PRETHINK,
	GAME_CLIENT_THINK,				// ( int clientNum,int isSpectator );
	GAME_CLIENT_POSTTHINK,

	GAME_START_FRAME,					// ( int levelTime );
	GAME_SETCHANGEPARMS, //self
	GAME_SETNEWPARMS,
	GAME_CONSOLE_COMMAND,			// ( void );
	GAME_EDICT_TOUCH,                      //(self,other)
	GAME_EDICT_THINK,                      //(self,other=world,time)
	GAME_EDICT_BLOCKED,                     //(self,other)
	// ConsoleCommand will be called when a command has been issued
	// that is not recognized as a builtin function.
	// The game can issue trap_argc() / trap_argv() commands to get the command
	// and parameters.  Return qfalse if the game doesn't recognize it as a command.
	GAME_CLIENT_SAY,			// ( int isTeamSay );
	GAME_PAUSED_TIC,			// ( int duration_msec );	// duration is in msecs
	GAME_CLEAR_EDICT,           // (self)
} gameExport_t;


typedef enum
{
	F_INT, 
	F_FLOAT,
	F_LSTRING,			// string on disk, pointer in memory, TAG_LEVEL
//	F_GSTRING,			// string on disk, pointer in memory, TAG_GAME
	F_VECTOR,
	F_ANGLEHACK,
//	F_ENTITY,			// index on disk, pointer in memory
//	F_ITEM,				// index on disk, pointer in memory
//	F_CLIENT,			// index on disk, pointer in memory
	F_IGNORE
} fieldtype_t;

typedef struct
{
	stringptr_t   name;
	int           ofs;
	fieldtype_t   type;
} field_t;

typedef struct
{
	edict_t         *ents;
	int             sizeofent;
	globalvars_t    *global;
	field_t         *fields;
	int             APIversion;
	int             maxentities;
} gameData_t;

typedef int		fileHandle_t;

typedef enum {
	FS_READ_BIN,
	FS_READ_TXT,
	FS_WRITE_BIN,
	FS_WRITE_TXT,
	FS_APPEND_BIN,
	FS_APPEND_TXT
} fsMode_t;

typedef enum {
	FS_SEEK_CUR,
	FS_SEEK_END,
	FS_SEEK_SET
} fsOrigin_t;

#endif /* !__G_PUBLIC_H__ */
