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

extern cvar_t cl_parsesay;
extern cvar_t cl_nofake;
extern cvar_t cl_teamskin, cl_enemyskin, cl_teamquadskin, cl_enemyquadskin;
extern cvar_t cl_teampentskin, cl_enemypentskin, cl_teambothskin, cl_enemybothskin;
extern cvar_t snd_trigger;


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

qboolean TP_IsItemVisible(item_vis_t *visitem);

// triggers
void TP_ExecTrigger (char *s);
void TP_StatChanged (int stat, int value);
void TP_CheckPickupSound (char *s, vec3_t org);
qboolean TP_CheckSoundTrigger (char *str);

// message triggers
void TP_SearchForMsgTriggers (char *s, int level);


char *TP_PlayerName(void);
char *TP_PlayerTeam(void);
char *TP_MapName(void);
int	TP_CountPlayers(void);

// teamcolor & enemycolor
extern int cl_teamtopcolor, cl_teambottomcolor, cl_enemytopcolor, cl_enemybottomcolor;

char *TP_GetMapGroupName(char *mapname, qboolean *system);
char *TP_ParseMacroString (char *s);
char *TP_ParseFunChars (char *s, qboolean chat);
void TP_NewMap (void);
int TP_CategorizeMessage (char *s, int *offset);
qboolean TP_FilterMessage (char *s);
void TP_ParseWeaponModel(model_t *model);


char *TP_ParseWhiteText(char *s, qboolean team, int offset);



void TP_UpdateSkins(void);
void TP_RefreshSkin(int);
void TP_RefreshSkins(void);
qboolean TP_NeedRefreshSkins(void);

extern char *skinforcing_team;


void TP_Init (void);

//#define FPD_NO_TEAM_MACROS	1
#define FPD_NO_FORCE_SKIN	256
#define FPD_NO_FORCE_COLOR	512
#define FPD_LIMIT_PITCH		(1 << 14)
#define FPD_LIMIT_YAW		(1 << 15)
