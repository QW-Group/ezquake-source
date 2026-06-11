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
#if defined(FTE_PEXT_TRANS)
	qbool		is_transparent;
#endif
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

#ifdef MVD_PEXT1_WEAPONPREDICTION
	// weapon prediction state, mirrored from/into player_state_t for the
	// local player only (see CL_PredictUsercmd)
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
	byte		client_predflags;
	int			frame_current;	// outgoing sequence number of the frame being predicted
	int			effect_frame;	// last frame whose prediction events have been played

	short		ammo_shells;
	short		ammo_nails;
	short		ammo_rockets;
	short		ammo_cells;

	float		t_width;		// re-trigger time for the predicted lightning hit sound
#endif

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
	int     safestrafe; // sv_safestrafe value from server
} movevars_t;

extern movevars_t movevars;
extern playermove_t pmove;

#ifdef MVD_PEXT1_WEAPONPREDICTION
#include "qsound.h"

// client_predflags values sent by the server
#define PRDFL_MIDAIR	1
#define PRDFL_COILGUN	2
#define PRDFL_FORCEOFF	255

extern int pmove_nopred_weapon;

// true when the negotiated extension is in effect and weapon prediction has
// not been disabled by the user, the server (PRDFL_FORCEOFF) or spectating
#define PM_WeaponPredictionActive() \
	(!pmove_nopred_weapon && (cls.mvdprotocolextensions1 & MVD_PEXT1_WEAPONPREDICTION))

// predicted weapon sounds, precached by CL_InitWepSounds (cl_pred.c)
extern sfx_t *cl_sfx_jump, *cl_sfx_ax1, *cl_sfx_sg, *cl_sfx_ssg, *cl_sfx_ng,
	*cl_sfx_sng, *cl_sfx_gl, *cl_sfx_rl, *cl_sfx_lg, *cl_sfx_lghit, *cl_sfx_coil;

extern cvar_t cl_nopred_weapon;
extern cvar_t cl_predict_weaponsound;
extern cvar_t cl_predict_smoothview;
extern cvar_t cl_predict_beam;
extern cvar_t cl_predict_projectiles;
extern cvar_t cl_predict_jump;
extern cvar_t cl_predict_buffer;

void PM_PlayerWeapon (void);
int PM_FilterWeaponSound (byte sound_num);
void PM_SoundEffect (sfx_t *sample, int chan);
#endif

int PM_PlayerMove (void);

int PM_PointContents (vec3_t point);
int PM_PointContents_AllBSPs (vec3_t p);
void PM_CategorizePosition (void);
qbool PM_TestPlayerPosition (vec3_t point);
trace_t PM_PlayerTrace (vec3_t start, vec3_t end);
trace_t PM_TraceLine (vec3_t start, vec3_t end);

#define MIN_STEP_NORMAL 0.7 // roughly 45 degrees

#endif /* !__PMOVE_H__ */
