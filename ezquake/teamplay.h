/*
	teamplay.c

	Teamplay enhancements ("proxy features")

	Copyright (C) 2000-2001       Anton Gavrilov

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
*/

#include "pcre.h"

extern cvar_t cl_parsesay;
extern cvar_t cl_nofake;
extern cvar_t cl_teamskin, cl_enemyskin, cl_teamquadskin, cl_enemyquadskin;
extern cvar_t cl_teampentskin, cl_enemypentskin, cl_teambothskin, cl_enemybothskin;
extern cvar_t snd_trigger;

typedef struct pcre_trigger_s {
	char					*name;
	char					*regexpstr;
	struct pcre_trigger_s*	next;
	pcre*					regexp;
	pcre_extra*				regexp_extra;
	unsigned				flags;
	float					min_interval;
	double					lasttime;
	int						counter;
} pcre_trigger_t;

typedef void internal_trigger_func(char *s);

typedef struct pcre_internal_trigger_s {
	struct pcre_internal_trigger_s		*next;
	pcre					*regexp;
	pcre_extra				*regexp_extra;
	internal_trigger_func			*func;
	unsigned				flags;
} pcre_internal_trigger_t;

typedef struct item_vis_s {
	vec3_t	vieworg;
	vec3_t	forward;
	vec3_t	right;
	vec3_t	up;
	vec3_t	entorg;
	float	radius;
	vec3_t	dir;		
	float	dist;		
} item_vis_t;


#define		RE_PRINT_LOW		1
#define		RE_PRINT_NEDIUM		2
#define		RE_PRINT_HIGH		4
#define		RE_PRINT_CHAT		8
#define		RE_PRINT_CENTER		16
#define		RE_PRINT_ECHO		32
#define		RE_PRINT_INTERNAL	64

// all of the above except internal
#define		RE_PRINT_ALL		31

// do not look for other triggers if matching is succesful
#define		RE_FINAL		256
// do not display string if matching is uccesful
#define		RE_REMOVESTR		512
// do not log string if matching is succesful
#define		RE_NOLOG		1024
// trigger is enabled
#define		RE_ENABLED		2048
// do not call alias
#define		RE_NOACTION		4096

qbool TP_IsItemVisible(item_vis_t *visitem);


// re-triggers
qbool CL_SearchForReTriggers (char *s, unsigned trigger_type);
// if true, string should not be displayed
pcre_trigger_t *CL_FindReTrigger (char *name);
void CL_RE_Trigger_ResetLasttime (void);


// triggers
void TP_ExecTrigger (char *s);
void TP_StatChanged (int stat, int value);
void TP_CheckPickupSound (char *s, vec3_t org);
qbool TP_CheckSoundTrigger (char *str);

// message triggers
void TP_SearchForMsgTriggers (char *s, int level);


char *TP_PlayerName(void);
char *TP_PlayerTeam(void);
char *TP_MapName(void);
int	TP_CountPlayers(void);

// teamcolor & enemycolor
// extern int cl_teamtopcolor, cl_teambottomcolor, cl_enemytopcolor, cl_enemybottomcolor;
extern cvar_t cl_teamtopcolor, cl_teambottomcolor, cl_enemytopcolor, cl_enemybottomcolor;

// START shaman RFE 1020608
#ifdef GLQUAKE
char *TP_GetSkyGroupName(char *mapname, qbool *system);
#endif
// END shaman RFE 1020608

char *TP_GetMapGroupName(char *mapname, qbool *system);
char *TP_ParseMacroString (char *s);
char *TP_ParseFunChars (char *s, qbool chat);
void TP_NewMap (void);
int TP_CategorizeMessage (char *s, int *offset);
qbool TP_FilterMessage (char *s);
void TP_ParseWeaponModel(model_t *model);


wchar *TP_ParseWhiteText(wchar *s, qbool team, int offset);



void TP_UpdateSkins(void);
void TP_RefreshSkin(int);
void TP_RefreshSkins(void);
qbool TP_NeedRefreshSkins(void);

extern char *skinforcing_team;


void TP_Init (void);

//#define FPD_NO_TEAM_MACROS	1
#define FPD_NO_TIMERS		2
#define FPD_NO_SOUNDTRIGGERS	4 // disables triggers
#define FPD_NO_FORCE_SKIN	256
#define FPD_NO_FORCE_COLOR	512
#define FPD_LIMIT_PITCH		(1 << 14)
#define FPD_LIMIT_YAW		(1 << 15)

/*fpd values from qizmo.txt
  1 = Disable %-reporting
  2 = Disable use of powerup timer (obsolete in v2.55)
  4 = Disable use of soundtrigger
  8 = Disable use of lag features
 16 = Make Qizmo report any changes in lag settins
 32 = Silent %e enemy vicinity reporting (reporter doesn't see the message)
      (always on in v2.55)
 64 = Spectators can't talk to players and vice versa (voice)
128 = Silent %x and %y (reporter doesn't see the message) (always on in v2.8)
256 = Disable skin forcing
512 = Disable color forcing
*/

#define MAX_LOC_NAME		64
#define MAX_MACRO_STRING 	2048

typedef struct locdata_s {
	vec3_t coord;
	char *name;
	struct locdata_s *next;
} locdata_t;

int BestWeaponFromStatItems (int stat);
