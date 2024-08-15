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

    $Id: common.h,v 1.76 2007-10-13 15:36:56 cokeman1982 Exp $
*/
// common.h  -- general definitions

#ifndef __COMMON_H__
#define __COMMON_H__

//============================================================================

// include frequently used headers

// fixes mingw warning about winsock2.h
#ifdef _WIN32
#include <winsock2.h>
#endif

#include "q_shared.h"
#include "zone.h"
#include "cvar.h"
#include "cmd.h"
#include "net.h"
#include "protocol.h"
#include "cmodel.h"
#include "cmdline_params.h"
#include "fs.h"

#ifdef _WIN64
int Q_strlen(const char* s);
#define strlen Q_strlen
#endif

typedef struct texture_ref_s { unsigned int index; } texture_ref;
extern const texture_ref null_texture_reference;
qbool R_TextureValid(texture_ref ref);
#define R_TextureReferenceIsValid(ref) ((ref).index && R_TextureValid(ref))
#define R_TextureReferenceInvalidate(ref) { (ref).index = 0; }
#define R_TextureReferenceEqual(ref1, ref2) ((ref1).index == (ref2).index)
#define R_TextureReferenceCompare(ref1, ref2) (ref1.index < ref2.index ? -1 : ref1.index > ref2.index ? 1 : 0)

typedef enum
{
	mod_brush,
	mod_sprite,
	mod_alias,
	mod_alias3,

	mod_unknown
} modtype_t;

//============================================================================

// per-level limits
#define CL_MAX_EDICTS           2048	// FIXME: ouch! ouch! ouch!
#define MAX_EDICTS              2048    // can't encode more than this, see SV_WriteDelta
#define MAX_EDICTS_SAFE         512     // lower limit, to make sure no client limits exceeded
#define MAX_LIGHTSTYLES         64
#define MAX_MODELS              512	    // can't encode more than this, see SV_WriteDelta
#define MAX_VWEP_MODELS         32	    // could be increased to 256
#define MAX_SOUNDS              256	    // so they cannot be blindly increased

#define	SAVEGAME_COMMENT_LENGTH 39

//============================================================================

// stats are integers communicated to the client by the server
#define	MAX_CL_STATS		32

#define STAT_HEALTH             0
//define STAT_FRAGS             1
#define STAT_WEAPON             2
#define STAT_AMMO               3
#define STAT_ARMOR              4
//define STAT_WEAPONFRAME       5
#define STAT_SHELLS             6
#define STAT_NAILS              7
#define STAT_ROCKETS            8
#define STAT_CELLS              9
#define STAT_ACTIVEWEAPON       10
#define STAT_TOTALSECRETS       11
#define STAT_TOTALMONSTERS      12
#define STAT_SECRETS            13		// bumped on client side by svc_foundsecret
#define STAT_MONSTERS           14		// bumped by svc_killedmonster
#define STAT_ITEMS              15
#define STAT_VIEWHEIGHT         16		// Z_EXT_VIEWHEIGHT protocol extension
#define STAT_TIME               17		// Z_EXT_TIME extension
#define STAT_MATCHSTARTTIME     18		// Server should send this as msec (int)

// item flags
#define	IT_SHOTGUN              1
#define	IT_SUPER_SHOTGUN        2
#define	IT_NAILGUN              4
#define	IT_SUPER_NAILGUN        8
#define	IT_GRENADE_LAUNCHER     16
#define	IT_ROCKET_LAUNCHER      32
#define	IT_LIGHTNING            64
#define	IT_SUPER_LIGHTNING      128
#define	IT_SHELLS               256
#define	IT_NAILS                512
#define	IT_ROCKETS              1024
#define	IT_CELLS                2048
#define	IT_AXE                  4096
#define	IT_ARMOR1               8192
#define	IT_ARMOR2               16384
#define	IT_ARMOR3               32768
#define	IT_SUPERHEALTH          65536
#define	IT_KEY1                 131072
#define	IT_KEY2                 262144
#define	IT_INVISIBILITY         524288
#define	IT_INVULNERABILITY      1048576
#define	IT_SUIT                 2097152
#define	IT_QUAD                 4194304

#define IT_ALL_WEAPONS          (IT_AXE | IT_SHOTGUN | IT_SUPER_SHOTGUN | IT_NAILGUN | IT_SUPER_NAILGUN | IT_GRENADE_LAUNCHER | IT_ROCKET_LAUNCHER | IT_LIGHTNING | IT_SUPER_LIGHTNING)

#define	IT_SIGIL1               (1<<28)
#define	IT_SIGIL2               (1<<29)
#define	IT_SIGIL3               (1<<30)
#define	IT_SIGIL4               (1<<31)

//
// entity effects
//
#define EF_BRIGHTFIELD        1
#define EF_MUZZLEFLASH        2
#define EF_GREEN              2     // D-Kure: EF_GREEN will replace
#define EF_BRIGHTLIGHT        4     // EF_MUZZLEFLASH and provide RGB colours
#define EF_DIMLIGHT           8     // Both are needed as NQ uses EF_MUZZLE..
#define EF_FLAG1             16
#define EF_FLAG2             32
#define EF_BLUE              64
#define EF_RED              128

// game types sent by serverinfo
// these determine which intermission screen plays
#define	GAME_COOP			0
#define	GAME_DEATHMATCH		1

#define	MAX_INFO_KEY 64
//#define	MAX_INFO_STRING	1024
#define	MAX_SERVERINFO_STRING 512
#define	MAX_LOCALINFO_STRING 32768

#define	MAX_KEY_STRING      (MAX_INFO_KEY)     // mvdsv compatibility
#define	MAX_EXT_INFO_STRING (MAX_INFO_STRING)  // mvdsv compatibility

//============================================================================

#define Cvar_SetROM	Cvar_ForceSet	// mvdsv compatibility

#define Con_Printf  Com_Printf  	// mvdsv compatibility
#define Con_DPrintf Com_DPrintf		// mvdsv compatibility
#define fs_gamedir  com_gamedir		// mvdsv compatibility

//============================================================================


#define MAX_COM_TOKEN 1024

extern	char	com_token[MAX_COM_TOKEN];
extern	qbool	com_eof;
typedef enum {TTP_UNKNOWN, TTP_STRING} com_tokentype_t;

const char *COM_Parse(const char *data);
const char* COM_ParseEx(const char* data, qbool curlybraces);
char *COM_ParseToken (const char *data, const char *punctuation);

char *COM_Argv (int arg); // range and null checked
void COM_ClearArgv (int arg);
void COM_AddParm (char *parm);
void COM_InitArgv (int argc, char **argv);
int COM_Argc (void);
int COM_CheckParm(cmdline_param_id parm_id);
int COM_CheckParmOffset(cmdline_param_id parm_id, int offset);
int COM_FindParm(const char* parm);
const char* Cmd_CommandLineParamName(cmdline_param_id id);

#define IsDeveloperMode() (COM_CheckParm(cmdline_param_developer_mode))

// equals to consecutive calls of strtok(s, " ") that assign values to array
// "1 3.5 6 7" will lead to fl_array[0] = 1.0; fl_array[1] = 3.5; ...
int COM_GetFloatTokens(const char *s, float *fl_array, int fl_array_size);

void COM_Init (void);

const char *COM_SkipPath (const char *pathname);
char *COM_SkipPathWritable (char *pathname);
char *COM_FitPath(char *dest, int destination_size, char *src, int size_to_fit);
char *COM_FileExtension (const char *in);
void COM_StripExtension (const char *in, char *out, int out_size);
void COM_FileBase (const char *in, char *out);
void COM_DefaultExtension (char *path, char *extension, size_t path_len);
// If path doesn't have an extension or has a different extension, append(!) specified extension.
// path buffer supposed to be MAX_OSPATH size
void COM_ForceExtension (char *path, char *extension);
// If path doesn't have an extension or has a different extension, append(!) specified extension.
// a bit extended version of COM_ForceExtension(), we suply size of path, so append safe, sure if u provide right path size
void COM_ForceExtensionEx (char *path, char *extension, size_t path_size);
int COM_GetTempDir(char *buf, int bufsize);
qbool COM_WriteToUniqueTempFile(char* path, int path_buffer_size, const char* ext, const byte* buffer, size_t bytes);
qbool COM_WriteToUniqueTempFileVFS(char* path, int path_buffer_size, const char* ext, vfsfile_t* input);
qbool COM_FileExists(char *path);
void COM_StoreOriginalCmdline(int argc, char **argv);

extern char *SYSINFO_GetString(void);

char *va(const char *format, ...); // does a varargs printf into a temp buffer

//============================================================================
// Quake File System
// for more File System control, include fs.h in your sources

extern int fs_filepos;
extern char *com_filesearchpath;
extern char	fs_netpath[MAX_OSPATH];
struct cache_user_s;

extern char	com_gamedir[MAX_OSPATH];
extern char	com_basedir[MAX_OSPATH];
extern char	com_gamedirfile[MAX_QPATH];
extern char com_homedir[MAX_PATH];

extern qbool file_from_gamedir;	// set if file came from a gamedir (and gamedir wasn't id1/qw)

void FS_InitFilesystem (void);
void FS_Shutdown(void);
void FS_SetGamedir (char *dir, qbool force);
int FS_FOpenFile (const char *filename, FILE **file);
int FS_FOpenPathFile (const char *filename, FILE **file);
byte *FS_LoadTempFile (char *path, int *len);
byte *FS_LoadHunkFile (char *path, int *len);
byte *FS_LoadHeapFile (const char *path, int *len);
qbool FS_WriteFile(const char *filename, const void *data, int len); //The filename will be prefixed by com_basedir
qbool FS_WriteFile_2(const char *filename, const void *data, int len); //The filename used as is
void FS_CreatePath (char *path);
int FS_FCreateFile (char *filename, FILE **file, char *path, char *mode);
char *FS_LegacyDir (char *media_dir);

int FS_FileLength (FILE *f);
int FS_FileOpenRead (char *path, FILE **hndl);

char * FS_Locate_GetPath (const char *file);

//============================================================
// Alternative variant manipulation with info strings
//============================================================

#define INFO_HASHPOOL_SIZE 256

#define MAX_CLIENT_INFOS 128

typedef struct info_s {
	struct info_s	*hash_next;
	struct info_s	*next;

	char				*name;
	char				*value;

} info_t;

typedef struct ctxinfo_s {

	info_t	*info_hash[INFO_HASHPOOL_SIZE];
	info_t	*info_list;

	int		cur; // current infos
	int		max; // max    infos

} ctxinfo_t;

// return value for given key
char			*Info_Get(ctxinfo_t *ctx, const char *name);
// set value for given key
qbool			Info_Set (ctxinfo_t *ctx, const char *name, const char *value);
// set value for given star key
qbool			Info_SetStar (ctxinfo_t *ctx, const char *name, const char *value);
// remove given key
qbool			Info_Remove (ctxinfo_t *ctx, const char *name);
// remove all infos
void			Info_RemoveAll (ctxinfo_t *ctx);
// convert old way infostring to new way: \name\qqshka\noaim\1 to hashed variant
qbool			Info_Convert(ctxinfo_t *ctx, char *str);
// convert new way to old way
qbool			Info_ReverseConvert(ctxinfo_t *ctx, char *str, int size);
// copy star keys from ont ctx to other
qbool			Info_CopyStar(ctxinfo_t *ctx_from, ctxinfo_t *ctx_to);
// just print all key value pairs
void			Info_PrintList(ctxinfo_t *ctx);

//============================================================================

char *Info_ValueForKey (char *s, char *key);
void Info_RemoveKey (char *s, char *key);
void Info_RemovePrefixedKeys (char *start, char prefix);
void Info_SetValueForKey (char *s, char *key, char *value, int maxsize);
void Info_SetValueForStarKey (char *s, char *key, char *value, int maxsize);
void Info_Print (char *s);

// Same as Info_SetValueForStarKey() but allow do not check for key and value length is less than MAX_INFO_KEY.
// Please do NOT use it unless you sure.
void Info_SetValueForStarKeyEx (char *s, char *key, char *value, int maxsize, qbool max_info_key_check);
// Same as Info_SetValueForKey() but allow do not check for key and value length is less than MAX_INFO_KEY.
// Please do NOT use it unless you sure.
void Info_SetValueForKeyEx (char *s, char *key, char *value, int maxsize, qbool max_info_key_check);

unsigned Com_BlockChecksum (void *buffer, int length);
void Com_BlockFullChecksum (void *buffer, int len, unsigned char *outbuf);
byte COM_BlockSequenceCRCByte (byte *base, int length, int sequence);

//============================================================================

void Com_BeginRedirect (void (*RedirectedPrint) (char *));
void Com_EndRedirect (void);
void Com_Printf (char *fmt, ...);
void Com_DPrintf (char *fmt, ...);
void Com_PrintVerticalBar(int width);

// why not just 1 2 3 4 ... ?
#define PRINT_OK		(1<<0) // have prefix, printed only if developer != 0
#define PRINT_INFO		(1<<1) // have prefix
#define PRINT_FAIL		(1<<2) // have prefix
#define PRINT_WARNING	(1<<3) // do not have prefix
#define PRINT_ALL		(1<<4) // do not have prefix
#define PRINT_ERR_FATAL	(1<<5) // do Sys_Error()
#define PRINT_DBG		(1<<6) // do not have prefix, printed only if developer != 0

void Com_Printf_State(int state, const char *fmt, ...);

extern unsigned	Print_flags[16];
extern int	Print_current;

#define     PR_SKIP     1
#define     PR_LOG_SKIP 2
#define     PR_TR_SKIP  4
#define     PR_IS_CHAT  8
#define     PR_NONOTIFY 16
#define     PR_NORESET  32        // Don't reset at the end of each Print() call

//============================================================================

#ifdef FTE_PEXT_FLOATCOORDS

typedef union {	//note: reading from packets can be misaligned
	int b4;
	float f;
	short b2;
	char b[4];
} coorddata;

extern int msg_coordsize; // 2 or 4.
extern int msg_anglesize; // 1 or 2.

float MSG_FromCoord(coorddata c, int bytes);
coorddata MSG_ToCoord(float f, int bytes);	//return value should be treated as (char*)&ret;
coorddata MSG_ToAngle(float f, int bytes);	//return value is NOT byteswapped.

#endif

struct usercmd_s;

extern struct usercmd_s nullcmd;

void MSG_WriteChar (sizebuf_t *sb, int c);
void MSG_WriteByte (sizebuf_t *sb, int c);
void MSG_WriteShort (sizebuf_t *sb, int c);
void MSG_WriteLong (sizebuf_t *sb, int c);
void MSG_WriteFloat (sizebuf_t *sb, float f);
void MSG_WriteString (sizebuf_t *sb, const char *s);
void MSG_WriteUnterminatedString (sizebuf_t *sb, char *s);
void MSG_WriteCoord (sizebuf_t *sb, float f);
void MSG_WriteLongCoord (sizebuf_t *sb, float f);
void MSG_WriteAngle (sizebuf_t *sb, float f);
void MSG_WriteAngle16 (sizebuf_t *sb, float f);
int MSG_WriteDeltaUsercmd (sizebuf_t *sb, struct usercmd_s *from, struct usercmd_s *cmd, unsigned int mvdsv_extensions);
void MSG_WriteDeltaEntity  (entity_state_t *from, entity_state_t *to, sizebuf_t *msg, qbool force, unsigned int fte_extensions, unsigned int mvdsv_extensions);

extern	int	msg_readcount;
extern	qbool	msg_badread; // set if a read goes beyond end of message

void MSG_BeginReading (void);
int MSG_GetReadCount(void);
int MSG_ReadChar (void);
int MSG_ReadByte (void);
int MSG_ReadShort (void);
int MSG_ReadLong (void);
float MSG_ReadFloat (void);
char *MSG_ReadString (void);
char *MSG_ReadStringLine (void);

float MSG_ReadCoord (void);
float MSG_ReadFloatCoord (void);
float MSG_ReadAngle (void);
float MSG_ReadAngle16 (void);
void MSG_ReadDeltaUsercmd (struct usercmd_s *from, struct usercmd_s *cmd, int protoversion);

void MSG_ReadData (void *data, int len);
void MSG_ReadSkip(int bytes);

//============================================================================

extern cvar_t	developer;
extern cvar_t	host_mapname;

extern qbool	com_serveractive; // true if sv.state != ss_dead
extern int	CL_ClientState (void); // returns cls.state

extern double	curtime; // not bounded or scaled, shared by local client and server

// host
extern qbool host_initialized;
extern qbool host_everything_loaded;
extern int host_memsize;

void Host_Init (int argc, char **argv, int default_memsize);
void Host_ClearMemory (void);
void Host_Shutdown (void);
void Host_Frame (double time);
void Host_Abort (void);	 // longjmp() to Host_Frame
void Host_EndGame (void); // kill local client and server
void Host_Error (char *error, ...);
void Host_Quit (void);

void CL_Init (void);
void CL_Shutdown (void);
void CL_Frame (double time);
void CL_Disconnect(void);
void CL_BeginLocalConnection (void);
void CL_UpdateCaption(qbool force);
void Con_Init (void);

void SV_Init (void);
void SV_Shutdown (char *finalmsg);
void SV_Frame (double time);

void SV_Error (char *error, ...);

void COM_ParseIPCData(const char *buf, unsigned int bufsize);

qbool COM_CheckArgsForPlayableFiles(char *commandbuf_out, unsigned int commandbuf_size);

int Com_TranslateMapChecksum (const char *mapname, int checksum);
float AdjustAngle(float current, float ideal, float fraction);

char *Q_normalizetext(char *str);
unsigned char *Q_redtext(unsigned char *str);
unsigned char *Q_yelltext(unsigned char *str);

// Name comparison: case insensitive, red/white text insensitive
int Q_namecmp(const char* s1, const char* s2);

#endif /* !__COMMON_H__ */

