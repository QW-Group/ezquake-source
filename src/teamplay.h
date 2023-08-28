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

#include <pcre2.h>

extern cvar_t cl_parsesay;
extern cvar_t cl_nofake;
extern cvar_t cl_teamskin, cl_enemyskin, cl_teamquadskin, cl_enemyquadskin;
extern cvar_t cl_teampentskin, cl_enemypentskin, cl_teambothskin, cl_enemybothskin;

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

qbool TP_IsItemVisible(item_vis_t *visitem);


// triggers
void TP_ExecTrigger (const char *s);
void TP_StatChanged (int stat, int value);
void TP_CheckPickupSound (char *s, vec3_t org);
qbool TP_CheckSoundTrigger (wchar *str);

char *TP_PlayerName(void);
char *TP_PlayerTeam(void);
char *TP_MapName(void);
int	TP_CountPlayers(void);

// returns true if the player in the current POV is from given team
qbool TP_ThisPOV_IsHisTeam(const char* team);

// assigns numbers 1..n to teammates and enemies, returns given players number
int TP_PlayersNumber(int userid, const char* team);

// teamcolor & enemycolor
extern cvar_t cl_teamtopcolor, cl_teambottomcolor, cl_enemytopcolor, cl_enemybottomcolor;

char *TP_GetSkyGroupName(char *mapname, qbool *system);
char *TP_GetMapGroupName(char *mapname, qbool *system);
char *TP_ParseMacroString (char *s);
char *TP_ParseFunChars (const char *s, qbool chat);
void TP_NewMap (void);


#define msgtype_unknown   0
#define msgtype_normal    1
#define msgtype_team      2
#define msgtype_spec      4
#define msgtype_specteam  8
#define msgtype_qtv       16
/*
returns a logical combination of msgtype_* values
Note that sometimes we can't be sure who really sent the message,  e.g. when there's a
player "unnamed" in your team and "(unnamed)" in the enemy team. The result will be 3 (1+2)
*/
int TP_CategorizeMessage (const char *s, int *offset);
qbool TP_FilterMessage (wchar *s);


wchar *TP_ParseWhiteText (const wchar *s, qbool team, int offset);



void TP_UpdateSkins(void);
void TP_RefreshSkin(int);
void TP_RefreshSkins(void);
qbool TP_NeedRefreshSkins(void);

extern char *skinforcing_team;
char *TP_SkinForcingTeam(void);
qbool TP_TeamLockSpecified(void);

void TP_Init(void);
void TP_Shutdown(void);

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

#define it_quad		(1 << 0)
#define it_pent		(1 << 1)
#define it_ring		(1 << 2)
#define it_suit		(1 << 3)
#define it_ra		(1 << 4)
#define it_ya		(1 << 5)
#define it_ga		(1 << 6)
#define it_mh		(1 << 7)
#define it_health	(1 << 8)
#define it_lg		(1 << 9)
#define it_rl		(1 << 10)
#define it_gl		(1 << 11)
#define it_sng		(1 << 12)
#define it_ng		(1 << 13)
#define it_ssg		(1 << 14)
#define it_pack		(1 << 15)
#define it_cells	(1 << 16)
#define it_rockets	(1 << 17)
#define it_nails	(1 << 18)
#define it_shells	(1 << 19)
#define it_flag		(1 << 20)
#define it_teammate	(1 << 21)
#define it_enemy	(1 << 22)
#define it_eyes		(1 << 23)
#define it_sentry   (1 << 24)
#define it_disp		(1 << 25)
#define it_quaded   (1 << 26)
#define it_pented   (1 << 27)
#define it_rune1	(1 << 28)
#define it_rune2	(1 << 29)
#define it_rune3	(1 << 30)
#define it_rune4	((unsigned int) (1 << 31))
#define NUM_ITEMFLAGS 32
 
#define it_runes	(it_rune1|it_rune2|it_rune3|it_rune4)
#define it_powerups	(it_quad|it_pent|it_ring|it_suit)
#define it_weapons	(it_lg|it_rl|it_gl|it_sng|it_ssg)
#define it_armor	(it_ra|it_ya|it_ga)
#define it_ammo		(it_cells|it_rockets|it_nails|it_shells)
#define it_players	(it_teammate|it_enemy|it_eyes)

// this structure is cleared after entering a new map
typedef struct tvars_s
{
	int		health;
	int		items;
	int		olditems;
	int		activeweapon;
	int		stat_framecounts[MAX_CL_STATS];
	double	deathtrigger_time;
	float	f_skins_reply_time;
	float	f_version_reply_time;
	char	lastdeathloc[MAX_LOC_NAME];
	char	tookname[32];
    int     tookflag;
    char	tookloc[MAX_LOC_NAME];
	double	tooktime;
	double	pointtime; // cls.realtime for which pointitem & pointloc are valid
	char	pointname[64];
    int     pointflag;
	char	pointloc[MAX_LOC_NAME];
	int		pointtype;
	char	nearestitemloc[MAX_LOC_NAME];
	char	lastreportedloc[MAX_LOC_NAME];
	double	lastdrop_time;
	char	lastdroploc[MAX_LOC_NAME];
	char	lasttrigger_match[256];
	int		needflags;	// sum of items player needs, updated on TP_GetNeed()
 
	int	numenemies;
	int	numfriendlies;
	int	last_numenemies;
	int	last_numfriendlies;
 
	int enemy_powerups;
	double enemy_powerups_time;
} tvars_t;
 
extern tvars_t vars;

extern char *TP_PlayerName (void);

extern cvar_t tp_tooktimeout;

extern void TP_FindPoint (void);
#define TOOK_EMPTY() (!vars.tooktime || cls.realtime > vars.tooktime + tp_tooktimeout.value)

// updates the state of vars.needflags
void TP_GetNeed(void);

char *Macro_PointName (void);

// Locations
char *TP_LocationName (vec3_t location);
void TP_LocFiles_Init(void);
void TP_LocFiles_NewMap(void);
void TP_LocFiles_Shutdown(void);

char *TP_ItemName(int item_flag);
