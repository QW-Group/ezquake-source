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
// common.h  -- general definitions

#ifndef __COMMON_H_

#define __COMMON_H_

#include <math.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

#ifdef _WIN32
#pragma warning( disable : 4244 4127 4201 4214 4514 4305 4115 4018)
#endif

#define	QUAKE_GAME			// as opposed to utilities

typedef unsigned char 		byte;

typedef enum {false, true} qbool;

#ifndef NULL
#define NULL ((void *) 0)
#endif


#ifdef _WIN32
#define IS_SLASH(c) ((c) == '/' || (c) == '\\')
#else
#define IS_SLASH(c) ((c) == '/')
#endif


#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

//#define bound(a,b,c) (max((a), min((b), (c))))
#define bound(a,b,c) ((a) >= (c) ? (a) : \
					(b) < (a) ? (a) : (b) > (c) ? (c) : (b))

//============================================================================

#if id386
#define UNALIGNED_OK	1	// set to 0 if unaligned accesses are not supported
#else
#define UNALIGNED_OK	0
#endif

//============================================================================

#define	MINIMUM_MEMORY	0x550000

#define	MAX_QPATH		64			// max length of a quake game pathname
#define	MAX_OSPATH		128			// max length of a filesystem pathname

#define	ON_EPSILON		0.1			// point on plane side epsilon

#define	MAX_MSGLEN		1450		// max length of a reliable message
#define	MAX_DATAGRAM	1450		// max length of unreliable message
#define	MAX_UDP_PACKET	(MAX_MSGLEN*2)	// one more than msg + header
#define	FILE_TRANSFER_BUF_SIZE	MAX_MSGLEN - 100

// per-level limits
#define	MAX_LIGHTSTYLES	64
#define	MAX_MODELS		256			// these are sent over the net as bytes
#define	MAX_SOUNDS		256			// so they cannot be blindly increased

#define	SAVEGAME_COMMENT_LENGTH	39

#define	MAX_STYLESTRING	64

// stats are integers communicated to the client by the server
#define	MAX_CL_STATS		32
#define	STAT_HEALTH			0
//define STAT_FRAGS			1
#define	STAT_WEAPON			2
#define	STAT_AMMO			3
#define	STAT_ARMOR			4
//define STAT_WEAPONFRAME	5
#define	STAT_SHELLS			6
#define	STAT_NAILS			7
#define	STAT_ROCKETS		8
#define	STAT_CELLS			9
#define	STAT_ACTIVEWEAPON	10
#define	STAT_TOTALSECRETS	11
#define	STAT_TOTALMONSTERS	12
#define	STAT_SECRETS		13		// bumped on client side by svc_foundsecret
#define	STAT_MONSTERS		14		// bumped by svc_killedmonster
#define	STAT_ITEMS			15
#define STAT_VIEWHEIGHT		16		// Z_EXT_VIEWHEIGHT protocol extension
#define STAT_TIME			17		// Z_EXT_TIME extension

// item flags
#define	IT_SHOTGUN				1
#define	IT_SUPER_SHOTGUN		2
#define	IT_NAILGUN				4
#define	IT_SUPER_NAILGUN		8

#define	IT_GRENADE_LAUNCHER		16
#define	IT_ROCKET_LAUNCHER		32
#define	IT_LIGHTNING			64
#define	IT_SUPER_LIGHTNING		128

#define	IT_SHELLS				256
#define	IT_NAILS				512
#define	IT_ROCKETS				1024
#define	IT_CELLS				2048

#define	IT_AXE					4096

#define	IT_ARMOR1				8192
#define	IT_ARMOR2				16384
#define	IT_ARMOR3				32768

#define	IT_SUPERHEALTH			65536

#define	IT_KEY1					131072
#define	IT_KEY2					262144

#define	IT_INVISIBILITY			524288

#define	IT_INVULNERABILITY		1048576
#define	IT_SUIT					2097152
#define	IT_QUAD					4194304

#define	IT_SIGIL1				(1<<28)

#define	IT_SIGIL2				(1<<29)
#define	IT_SIGIL3				(1<<30)
#define	IT_SIGIL4				(1<<31)

// print flags
#define	PRINT_LOW			0		// pickup messages
#define	PRINT_MEDIUM		1		// death messages
#define	PRINT_HIGH			2		// critical messages
#define	PRINT_CHAT			3		// chat messages

// game types sent by serverinfo
// these determine which intermission screen plays
#define	GAME_COOP			0
#define	GAME_DEATHMATCH		1

#define	MAX_INFO_STRING	196
#define	MAX_SERVERINFO_STRING	512
#define	MAX_LOCALINFO_STRING	32768

//============================================================================

typedef struct sizebuf_s {
	qbool	allowoverflow;	// if false, do a Sys_Error
	qbool	overflowed;		// set to true if the buffer size failed
	byte	*data;
	int		maxsize;
	int		cursize;
} sizebuf_t;

char *com_args_original;

void SZ_Init (sizebuf_t *buf, byte *data, int length);
void SZ_Clear (sizebuf_t *buf);
void *SZ_GetSpace (sizebuf_t *buf, int length);
void SZ_Write (sizebuf_t *buf, void *data, int length);
void SZ_Print (sizebuf_t *buf, char *data);	// strcats onto the sizebuf

//============================================================================

typedef struct link_s {
	struct link_s	*prev, *next;
} link_t;

void ClearLink (link_t *l);
void RemoveLink (link_t *l);
void InsertLinkBefore (link_t *l, link_t *before);
void InsertLinkAfter (link_t *l, link_t *after);

// (type *)STRUCT_FROM_LINK(link_t *link, type, member)
// ent = STRUCT_FROM_LINK(link,entity_t,order)
// FIXME: remove this mess!
#define	STRUCT_FROM_LINK(l,t,m) ((t *)((byte *)l - (int)&(((t *)0)->m)))

//============================================================================

short	ShortSwap (short l);
int		LongSwap (int l);
float	FloatSwap (float f);

#ifdef __BIG_ENDIAN__
#define BigShort(x) (x)
#define BigLong(x) (x)
#define BigFloat(x) (x)
#define LittleShort(x) ShortSwap (x)
#define LittleLong(x) LongSwap(x)
#define LittleFloat(x) FloatSwap(x)
#else
#define BigShort(x) ShortSwap (x)
#define BigLong(x) LongSwap(x)
#define BigFloat(x) FloatSwap(x)
#define LittleShort(x) (x)
#define LittleLong(x) (x)
#define LittleFloat(x) (x)
#endif

//============================================================================

#ifdef _WIN32

//#define vsnprintf _vsnprintf

#define Q_strcasecmp(s1, s2) _stricmp((s1), (s2))
#define Q_strncasecmp(s1, s2, n) _strnicmp((s1), (s2), (n))

#else

#define Q_strcasecmp(s1, s2) strcasecmp((s1), (s2))
#define Q_strncasecmp(s1, s2, n) strncasecmp((s1), (s2), (n))

#endif

// Added by VVD {
#ifdef _WIN32
#define strcasecmp(s1, s2)	_stricmp  ((s1),   (s2))
#define strncasecmp(s1, s2, n)	_strnicmp ((s1),   (s2),   (n))
int snprintf(char *str, size_t n, char const *fmt, ...);
int vsnprintf(char *buffer, size_t count, const char *format, va_list argptr);
#endif

#if defined(__linux__) || defined(_WIN32)
size_t strlcpy (char *dst, char *src, size_t siz);
size_t strlcat (char *dst, char *src, size_t siz);
char  *strnstr (char *s, char *find, size_t slen);
#endif
// Added by VVD }

// QW262 -->
// same as standard functions but
// return pointer to the end of dest string
char* Q_strcpy(char* dest, const char* src);
char* Q_strcat(char* dest, const char* src);
// <-- QW262

int Q_atoi (char *str);
float Q_atof (char *str);
char *Q_ftos (float value);		// removes trailing zero chars

void Q_strncpyz (char *dest, char *src, size_t size);
void Q_snprintfz (char *dest, size_t size, char *fmt, ...);

// memory management
void *Q_malloc (size_t size);
void *Q_calloc (size_t n, size_t size);
char *Q_strdup (const char *src);
// might be turned into a function that makes sure all Q_*alloc calls are matched with Q_free
#define Q_free(ptr) free(ptr)

int Com_HashKey (char *name);

//============================================================================

extern	char		com_token[1024];
extern	qbool	com_eof;

char *COM_Parse (char *data);

extern	int		com_argc;
extern	char	**com_argv;

void COM_InitArgv (int argc, char **argv);
int	COM_Argc (void);
char *COM_Argv (int arg);	// range and null checked
void COM_ClearArgv (int arg);
int COM_CheckParm (char *parm);
void COM_AddParm (char *parm);

void COM_Init (void);
void COM_Shutdown (void);

char *COM_SkipPath (char *pathname);
void COM_StripExtension (char *in, char *out);
void COM_FileBase (char *in, char *out);
void COM_DefaultExtension (char *path, char *extension);
void COM_ForceExtension (char *path, char *extension);
int COM_FileLength (FILE *f);
int COM_FileOpenRead (char *path, FILE **hndl);

void COM_StoreOriginalCmdline(int argc, char **argv);

extern char * SYSINFO_GetString(void);

char	*va(char *format, ...);
// does a varargs printf into a temp buffer

char *CopyString(char *s);

//============================================================================

extern int com_filesize;
extern qbool com_filefrompak;
extern char *com_filesearchpath;
extern char	com_netpath[MAX_OSPATH];
struct cache_user_s;

extern char	com_gamedir[MAX_OSPATH];
extern char	com_basedir[MAX_OSPATH];
extern char	com_gamedirfile[MAX_QPATH];

extern qbool file_from_pak;		// set if file came from a pak file
extern qbool file_from_gamedir;	// set if file came from a gamedir (and gamedir wasn't id1/qw)

void FS_InitFilesystem (void);
void FS_SetGamedir (char *dir);
int FS_FOpenFile (char *filename, FILE **file);
byte *FS_LoadStackFile (char *path, void *buffer, int bufsize);
byte *FS_LoadTempFile (char *path);
byte *FS_LoadHunkFile (char *path);
void FS_LoadCacheFile (char *path, struct cache_user_s *cu);

qbool COM_WriteFile (char *filename, void *data, int len);
void COM_CreatePath (char *path);
int COM_FCreateFile (char *filename, FILE **file, char *path, char *mode);
int Q_strlen (char *str);

void COM_CheckRegistered (void);

char *Info_ValueForKey (char *s, char *key);
void Info_RemoveKey (char *s, char *key);
void Info_RemovePrefixedKeys (char *start, char prefix);
void Info_SetValueForKey (char *s, char *key, char *value, int maxsize);
void Info_SetValueForStarKey (char *s, char *key, char *value, int maxsize);
void Info_Print (char *s);

unsigned Com_BlockChecksum (void *buffer, int length);
void Com_BlockFullChecksum (void *buffer, int len, unsigned char *outbuf);
byte COM_BlockSequenceCRCByte (byte *base, int length, int sequence);

//============================================================================

void Com_BeginRedirect (void (*RedirectedPrint) (char *));
void Com_EndRedirect (void);
void Com_Printf (char *fmt, ...);
void Com_DPrintf (char *fmt, ...);

// QW262 -->
extern unsigned	Print_flags[16];
extern int	Print_current;

#define		PR_SKIP		1
#define		PR_LOG_SKIP	2
#define		PR_TR_SKIP	4
#define		PR_IS_CHAT	8
// <-- QW262

//============================================================================

// include frequently used headers
#include "mathlib.h"
#include "zone.h"
#include "cvar.h"
#include "cmd.h"
#include "sys.h"
#include "net.h"
#include "protocol.h"

//============================================================================

struct usercmd_s;

extern struct usercmd_s nullcmd;

void MSG_WriteChar (sizebuf_t *sb, int c);
void MSG_WriteByte (sizebuf_t *sb, int c);
void MSG_WriteShort (sizebuf_t *sb, int c);
void MSG_WriteLong (sizebuf_t *sb, int c);
void MSG_WriteFloat (sizebuf_t *sb, float f);
void MSG_WriteString (sizebuf_t *sb, char *s);
void MSG_WriteCoord (sizebuf_t *sb, float f);
void MSG_WriteAngle (sizebuf_t *sb, float f);
void MSG_WriteAngle16 (sizebuf_t *sb, float f);
void MSG_WriteDeltaUsercmd (sizebuf_t *sb, struct usercmd_s *from, struct usercmd_s *cmd);
void MSG_WriteDeltaEntity  (entity_state_t *from, entity_state_t *to, sizebuf_t *msg, qbool force);
void MSG_EmitPacketEntities (packet_entities_t *from, int delta_sequence, packet_entities_t *to, 
	sizebuf_t *msg, entity_state_t *(*GetBaseline)(int number)); 

extern	int			msg_readcount;
extern	qbool	msg_badread;		// set if a read goes beyond end of message

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
float MSG_ReadAngle (void);
float MSG_ReadAngle16 (void);
void MSG_ReadDeltaUsercmd (struct usercmd_s *from, struct usercmd_s *cmd, int protoversion);

//============================================================================

#ifdef SERVERONLY
#define	dedicated	1
#elif CLIENTONLY
#define	dedicated	0
#else
extern qbool	dedicated;
#endif

extern cvar_t	developer;
extern cvar_t	registered;
extern cvar_t	mapname;

extern qbool		com_serveractive;	// true if sv.state != ss_dead
extern int			CL_ClientState ();	// returns cls.state

extern double		curtime;	// not bounded or scaled, shared by local client and server

// host
extern qbool		host_initialized;
extern int			host_memsize;

void Host_Init (int argc, char **argv, int default_memsize);
void Host_ClearMemory ();
void Host_Shutdown (void);
void Host_Frame (double time);
void Host_Abort (void);			// longjmp() to Host_Frame
void Host_EndGame (void);		// kill local client and server
void Host_Error (char *error, ...);
void Host_Quit (void);

void CL_Init (void);
void CL_Shutdown (void);
void CL_Frame (double time);
void CL_Disconnect ();
void CL_BeginLocalConnection (void);
void Con_Init (void);
void Con_Print (char *txt);

void SV_Init (void);
void SV_Shutdown (char *finalmsg);
void SV_Frame (double time);

int isspace2(int c);

#endif //__COMMON_H_
