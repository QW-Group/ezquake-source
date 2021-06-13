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

#ifndef __PMOVE_H__
#define __PMOVE_H__

#define	MAX_PHYSENTS 64 

typedef struct {
	vec3_t		origin;
	cmodel_t	*model;		// only for bsp models
	vec3_t		mins, maxs;	// only for non-bsp models
	int			info;		// for client or server to identify
} physent_t;

typedef enum {
	PM_NORMAL,              // normal ground movement
	PM_OLD_SPECTATOR,       // fly, no clip to world (QW bug)
	PM_SPECTATOR,           // fly, no clip to world
	PM_DEAD,                // no acceleration
	PM_FLY,                 // fly, bump into walls
	PM_NONE,                // can't move
	PM_LOCK                 // server controls origin and view angles
} pmtype_t;

typedef struct {
	// player state
	vec3_t		origin;
	vec3_t		angles;
	vec3_t		velocity;
	qbool		jump_held;
	int			jump_msec; // msec since last jump
	float		waterjumptime;
	int			pm_type;

	int			weapon;
	int			weapon_index;
	int			weaponframe;

	int			current_ammo;
	int			items;
	int			impulse;
	float		client_time;
	float		attack_finished;
	float		client_nextthink;
	byte		client_thinkindex;
	byte		client_ping;

	int			last_frame;
	int			effect_frame;

	short		ammo_shells;
	short		ammo_nails;
	short		ammo_rockets;
	short		ammo_cells;

	float		t_width;

	// world state
	int			numphysent;
	physent_t	physents[MAX_PHYSENTS]; // 0 should be the world

	// input
	usercmd_t	cmd;

	// results
	int			numtouch;
	int			touchindex[MAX_PHYSENTS];
	qbool		onground;
	int			groundent; // index in physents array, only valid when onground is true
	int			waterlevel;
	int			watertype;

	int         maxgroundspeed;
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
	qbool	slidefix; // NQ-style movement down ramps
	qbool	airstep;
	qbool	pground; // NQ-style "onground" flag handling.
	int     rampjump; // if set, all vertical velocity clipped by groundplane during jump frame.  If 0, only when falling (standard jumpfix)
} movevars_t;

int pmove_playeffects;
cvar_t cl_nopred_weapon;

extern movevars_t movevars;
extern playermove_t pmove;

int PM_PlayerMove (void);
void PM_PlayerWeapon (void);

int PM_PointContents (vec3_t point);
int PM_PointContents_AllBSPs (vec3_t p);
void PM_CategorizePosition (void);
qbool PM_TestPlayerPosition (vec3_t point);
trace_t PM_PlayerTrace (vec3_t start, vec3_t end);
trace_t PM_TraceLine (vec3_t start, vec3_t end);

#define MIN_STEP_NORMAL 0.7 // roughly 45 degrees

#endif /* !__PMOVE_H__ */
