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

typedef struct {
	vec3_t	normal;
	float	dist;
} pmplane_t;

typedef struct {
	qboolean	allsolid;	// if true, plane is not valid
	qboolean	startsolid;	// if true, the initial point was in a solid area
	qboolean	inopen, inwater;
	float		fraction;		// time completed, 1.0 = didn't hit anything
	vec3_t		endpos;			// final position
	pmplane_t		plane;			// surface normal at impact
	int			ent;			// entity the surface is on
} pmtrace_t;

#define	MAX_PHYSENTS	64 

typedef struct {
	vec3_t	origin;
	model_t	*model;		// only for bsp models
	vec3_t	mins, maxs;	// only for non-bsp models
	int		info;		// for client or server to identify
} physent_t;

typedef enum { 
	PM_NORMAL,			// normal ground movement 
	PM_OLD_SPECTATOR,	// fly, no clip to world (QW bug) 
	PM_SPECTATOR,		// fly, no clip to world 
	PM_DEAD,			// no acceleration
	PM_FLY				// fly, bump into walls
} pmtype_t;

typedef struct {
	int			sequence;	// just for debugging prints

	// player state
	vec3_t		origin;
	vec3_t		angles;
	vec3_t		velocity;
	qboolean	jump_held;
#ifndef SERVERONLY
	int			jump_msec;	// msec since last jump
#endif
	float		waterjumptime;
	pmtype_t	pm_type;

	// world state
	int			numphysent;
	physent_t	physents[MAX_PHYSENTS];	// 0 should be the world

	// input
	usercmd_t	cmd;

	// results
	int			numtouch;
	int			touchindex[MAX_PHYSENTS];
	qboolean	onground;
	int			groundent;		// index in physents array, only valid
								// when onground is true
	int			waterlevel;
	int			watertype;
} playermove_t;

typedef struct {
	float	gravity;
	float	stopspeed;
	float	maxspeed;
	float	spectatormaxspeed;
	float	accelerate;
	float	airaccelerate;
	float	wateraccelerate;
	float	friction;
	float	waterfriction;
	float	entgravity;
	float	bunnyspeedcap;
	float	ktjump;
	int		slidefix;
} movevars_t;

extern	movevars_t		movevars;
extern	playermove_t	pmove;

void PM_PlayerMove (void);
void PM_Init (void);

qboolean PM_RecursiveHullCheck (hull_t *hull, int num, float p1f, float p2f, vec3_t p1, vec3_t p2, pmtrace_t *trace);
int PM_HullPointContents (hull_t *hull, int num, vec3_t p);
int PM_PointContents (vec3_t point);
void PM_CategorizePosition (void);
qboolean PM_TestPlayerPosition (vec3_t point);
pmtrace_t PM_PlayerTrace (vec3_t start, vec3_t end);
pmtrace_t PM_TraceLine (vec3_t start, vec3_t end);
