/*
Copyright (C) 2021-2023 Sam "Reki" Piper

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



#define EZCSQC_WEAPONINFO		1
#define EZCSQC_PROJECTILE		2
#define EZCSQC_PLAYER			3
#define EZCSQC_WEAPONDEF		4
#define EZCSQC_HUDELEMENT		32

#define DRAWMASK_ENGINE			0x01
#define DRAWMASK_VIEWMODEL		0x02
#define DRAWMASK_PROJECTILE		0x04

typedef struct
{
	qbool	active;
	double	time;

	int		weapon_prediction;
	int		weaponframe;
	int		weaponmodel;
} ezcsqc_world_t;

extern ezcsqc_world_t ezcsqc;

typedef struct ezcsqc_entity_s
{
	qbool	isfree;
	double	freetime;

	int		entnum;
	int		drawmask;
	qbool	(*predraw)(struct ezcsqc_entity_s *self);

	int		frame;
	int		modelindex;
	float	alpha;

	int		flags;
	int		modelflags;
	int		effects;

	vec3_t	origin;
	vec3_t	angles;


// additional entity fields
	int		ownernum;

	vec3_t	trail_origin;

	vec3_t	oldorigin;
	vec3_t	s_origin;
	vec3_t	s_velocity;
	double	s_time;

} ezcsqc_entity_t;

typedef struct
{
	byte	state;

	byte	impulse;
	int		weapon;
	int		items;

	int		frame;

	int		ammo_shells;
	int		ammo_nails;
	int		ammo_rockets;
	int		ammo_cells;

	float	attack_finished;
	float	client_time;

	float	client_nextthink;
	int		client_thinkindex;

	int		client_predflags;
	float	client_ping;

	// additional fields
	int		current_ammo;
	int		weapon_index;
} ezcsqc_weapon_state_t;

//
#define WEPPRED_MAXSTATES	32
#define WEPPREDANIM_SOUND		0x0001
#define WEPPREDANIM_PROJECTILE	0x0002
#define WEPPREDANIM_LGBEAM		0x0004
#define WEPPREDANIM_MUZZLEFLASH 0x0008
#define WEPPREDANIM_DEFAULT		0x0010  // +attack will always be checked on this (unless current frame is +attack waiting
#define WEPPREDANIM_ATTACK		0x0020
#define WEPPREDANIM_BRANCH		0x0040
#define WEPPREDANIM_MOREBYTES	0x0080	// mark if we need "full" 16 bits of flags
#define WEPPREDANIM_SOUNDAUTO	0x0100
#define WEPPREDANIM_LTIME		0x0200	// use hacky ltime sound cooldown
typedef struct weppredanim_s
{
	signed char		mdlframe;			// frame number in model
	unsigned short	flags;				// flags from WEPPREDANIM
	unsigned short	sound;				// WEPPREDANIM_SOUND: sound index to play
	unsigned short	soundmask;			// WEPPREDANIM_SOUND: bitmask for sound (cl_predict_weaponsound)
	unsigned short	projectile_model;	// WEPPREDANIM_PROJECTILE: model index of projectile
	short			projectile_velocity[3]; // WEPPREDANIM_PROJECTILE: projectile velocity (v_right, v_forward, v_up)
	byte			projectile_offset[3]; // WEPPREDANIM_PROJECTILE: projectile spawn position (v_right, v_forward, z)
	byte			nextanim;			// next anim state index
	byte			altanim;			// WEPPREDANIM_BRANCH: next anim state if condition is fullfilled
	short			length;				// msec length of anim state (networked in 10ms increments)
} weppredanim_t;

typedef struct weppreddef_s
{
	unsigned short	modelindex;		// view model index
	unsigned short	attack_time;	// attack time in msec

	byte			impulse;		// impulse for equipping this weapon
	int				itemflag;		// .items bit for this item

	byte			anim_number;	// number of anim frames
	weppredanim_t	anim_states[WEPPRED_MAXSTATES];
} weppreddef_t;

typedef struct weppredsound_s
{
	unsigned short index;
	byte chan;
	unsigned short mask;

	struct weppredsound_s *next;
} weppredsound_t;

extern weppredsound_t *predictionsoundlist;
//

extern cvar_t			cl_predict_weaponsounds;
extern ezcsqc_entity_t  ezcsqc_entities[MAX_EDICTS];
extern ezcsqc_entity_t* ezcsqc_networkedents[MAX_EDICTS];

void CL_EZCSQC_ParseEntities(void);
void CL_EZCSQC_Ent_Update(ezcsqc_entity_t *self, qbool is_new);
void CL_EZCSQC_InitializeEntities(void);
ezcsqc_entity_t* CL_EZCSQC_Ent_Spawn(void);
void CL_EZCSQC_Ent_Remove(ezcsqc_entity_t *ent);
qbool CL_EZCSQC_Event_Sound(int entnum, int channel, int soundnumber, float vol, float attenuation, vec3_t pos, float pitchmod, float flags);

void CL_EZCSQC_ViewmodelUpdate(player_state_t *view_message);
void CL_EZCSQC_UpdateView(void);