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

#include "quakedef.h"
#ifdef MVD_PEXT1_EZCSQC
#include "ezcsqc.h"
#include "pmove.h"
#include "qsound.h"
#include "gl_local.h"
#include "gl_model.h"
#include "qmb_particles.h"
#include "vx_stuff.h"

ezcsqc_weapon_state_t	ws_server[UPDATE_BACKUP];
ezcsqc_weapon_state_t	ws_predicted;
ezcsqc_weapon_state_t	ws_saved[UPDATE_BACKUP];
ezcsqc_entity_t			ezcsqc_entities[MAX_EDICTS];
ezcsqc_entity_t*		ezcsqc_networkedents[MAX_EDICTS];
ezcsqc_world_t			ezcsqc;
weppredsound_t*			predictionsoundlist;
int projectile_ringbufferseek;
static int is_effectframe;
static int last_effectframe;
static float lg_twidth;

cvar_t cl_predict_weaponsounds = { "cl_predict_weaponsounds", "1" };

#define MAX_PREDWEPS 16
weppreddef_t wpredict_definitions[MAX_PREDWEPS];
#define WEPANIM(wep, frame) (&wep->anim_states[frame])
#define LENGTH2S(length) ((float)length / 1000)


ezcsqc_entity_t* CL_EZCSQC_Ent_Spawn(void)
{
	int i;
	double time_safety = Sys_DoubleTime() - 0.5;

	for (i = 0; i < MAX_EDICTS; i++)
	{
		ezcsqc_entity_t *ent = &ezcsqc_entities[i];
		if (!ent->isfree) // entity is not currently used
		{
			continue;
		}
		if (ent->freetime > time_safety) // entity was not *very* recently freed
		{
			continue;
		}
		memset(ent, 0, sizeof(ezcsqc_entity_t));
		return ent;
	}

	// uhh... not good, we're out of entities!
	return &ezcsqc_entities[0];
}

void CL_EZCSQC_Ent_Remove(ezcsqc_entity_t *ent)
{
	if (ent->isfree)
	{
		return;
	}

	ent->isfree = true;
	ent->freetime = Sys_DoubleTime();
}

void CL_EZCSQC_InitializeEntities(void)
{
	int i;

	memset(&ezcsqc, 0, sizeof(ezcsqc_world_t));

	last_effectframe = 0;
	lg_twidth = 0;
	projectile_ringbufferseek = 0;

	// clear weapon parameters
	for (i = 0; i < MAX_PREDWEPS; i++)
	{
		weppreddef_t *wep = &wpredict_definitions[i];
		memset(wep, 0, sizeof(weppreddef_t));
	}

	// clear entity array
	for (i = 0; i < MAX_EDICTS; i++)
	{
		ezcsqc_entity_t *ent = &ezcsqc_entities[i];
		memset(ent, 0, sizeof(ezcsqc_entity_t));
		ent->isfree = true;
		ezcsqc_networkedents[i] = NULL;
	}

	// clear prediction sound list
	weppredsound_t *snd = predictionsoundlist;
	while(snd)
	{
		predictionsoundlist = snd->next;
		free(snd);
		snd = predictionsoundlist;
	}
	predictionsoundlist = NULL;
}

/*
==============================================================================
								 Predraws
==============================================================================
*/

#define EF_NAILTRAIL	0x40000000
extern void R_ParticleTrail(vec3_t start, vec3_t end, trail_type_t type);
static void R_CSQC_MissileTrail(vec3_t start, vec3_t end, vec3_t angles, int trail_num)
{
	if ((trail_num == 8 || trail_num == 10 || trail_num == 11) && !qmb_initialized)
	{
		trail_num = 1;
	}

	if (trail_num == 1)
	{
		R_ParticleTrail(start, end, ROCKET_TRAIL);
	}
	else if (trail_num == 2)
	{
		R_ParticleTrail(start, end, GRENADE_TRAIL);
	}
	else if (trail_num == 3)
	{
		R_ParticleTrail(start, end, ALT_ROCKET_TRAIL);
	}
	else if (trail_num == 4)
	{
		R_ParticleTrail(start, end, BLOOD_TRAIL);
	}
	else if (trail_num == 5)
	{
		R_ParticleTrail(start, end, BIG_BLOOD_TRAIL);
	}
	else if (trail_num == 6)
	{
		R_ParticleTrail(start, end, TRACER1_TRAIL);
	}
	else if (trail_num == 7)
	{
		R_ParticleTrail(start, end, TRACER2_TRAIL);
	}
	else if (trail_num == 8)
	{
		// VULT PARTICLES
		byte color[3];
		color[0] = 0; color[1] = 70; color[2] = 255;
		FireballTrail(start, end, color, 2, 0.5f);
	}
	else if (trail_num == 9)
	{
		R_ParticleTrail(start, end, LAVA_TRAIL);
	}
	else if (trail_num == 10)
	{
		// VULT PARTICLES
		FuelRodGunTrail(start, end, angles);
	}
	else if (trail_num == 11)
	{
		byte color[3];
		color[0] = 255; color[1] = 70; color[2] = 5;
		FireballTrail(start, end, color, 2, 0.5f);
	}
	else if (trail_num == 12)
	{
		R_ParticleTrail(start, end, AMF_ROCKET_TRAIL);
	}
	else if (trail_num == 14)
	{
		R_ParticleTrail(start, end, RAIL_TRAIL);
	}
	else
	{
		R_ParticleTrail(start, end, GRENADE_TRAIL);
	}
}


qbool Predraw_Projectile(ezcsqc_entity_t *self)
{
	customlight_t cst_lt = { 0 };
	vec3_t dist_vec;
	float dist, freq;

	float dt = cl.servertime - self->s_time;
	VectorMA(self->s_origin, dt, self->s_velocity, self->origin);

	if (self->modelflags & EF_ROCKET)
	{
		// Add rocket lights
		float rocketlightsize = 200.0f * bound(0, r_rocketlight.value, 1);
		if (rocketlightsize >= 1)
		{
			extern cvar_t gl_rl_globe;
			int bubble = gl_rl_globe.integer ? 2 : 1;

			if ((r_rockettrail.value < 8 || r_rockettrail.value == 12) || !gl_part_trails.integer)
			{
				dlightColorEx(r_rocketlightcolor.value, r_rocketlightcolor.string, lt_rocket, false, &cst_lt);
				CL_NewDlightEx(self->entnum, self->origin, rocketlightsize, 0.1f, &cst_lt, bubble);
				if (!ISPAUSED && amf_coronas.integer)
				{
					//VULT CORONAS
					R_CoronasNew(C_ROCKETLIGHT, self->origin);
				}
			}

			if (r_rockettrail.value == 9 || r_rockettrail.value == 11)
			{
				CL_NewDlight(self->entnum, self->origin, rocketlightsize, 0.1f, lt_default, bubble);
			}
			else if (r_rockettrail.value == 8)
			{
				// PLASMA ROCKETS
				CL_NewDlight(self->entnum, self->origin, rocketlightsize, 0.1f, lt_blue, bubble);
			}
			else if (r_rockettrail.value == 10)
			{
				// FUEL ROD GUN
				CL_NewDlight(self->entnum, self->origin, rocketlightsize, 0.1f, lt_green, bubble);
			}
		}

		if (r_rockettrail.integer)
		{
			if (!gl_part_trails.integer || r_rockettrail.integer <= 5 || r_rockettrail.integer >= 12)
			{
				freq = 14;
			}
			else
			{
				freq = 2;
			}

			VectorSubtract(self->origin, self->trail_origin, dist_vec);
			dist = VectorLength(dist_vec);
			VectorNormalize(dist_vec);
			if (dist > freq)
			{
				R_CSQC_MissileTrail(self->trail_origin, self->origin, self->angles, r_rockettrail.integer);

				//R_ParticleTrail(self->trail_origin, self->origin, ROCKET_TRAIL);

				/*
				vec3_t part_origin;
				while (dist > 4)
				{
					VectorMA(self->trail_origin, dist, dist_vec, part_origin);



					dist -= 4;
				}
				*/
				VectorCopy(self->origin, self->trail_origin);
			}
		}
	}
	else if (self->modelflags & EF_NAILTRAIL)
	{
		if (amf_nailtrail.integer)
		{
			VectorSubtract(self->origin, self->trail_origin, dist_vec);
			dist = VectorLength(dist_vec);
			VectorNormalize(dist_vec);

			if (amf_nailtrail_plasma.integer)
			{
				if (dist > 3)
				{
					byte color[3];
					color[0] = 0; color[1] = 70; color[2] = 255;
					FireballTrail(self->trail_origin, self->origin, color, 0.6f, 0.3f);

					VectorCopy(self->origin, self->trail_origin);
				}
			}
			else
			{
				if (dist > 14)
				{
					ParticleNailTrail_NoEntity(self->trail_origin, self->origin, 2.0f, 0.014f);

					VectorCopy(self->origin, self->trail_origin);
				}
			}
		}
	}

	return true;
}

/*
==============================================================================
								 Weapon Definitons
==============================================================================
*/

void EntUpdate_WeaponDef(ezcsqc_entity_t *self, qbool is_new)
{
	int i, k;

	int sendflags = MSG_ReadByte();

	int wep_index = MSG_ReadByte();
	wep_index = bound(0, wep_index, MAX_PREDWEPS - 1);
	weppreddef_t *wep = &wpredict_definitions[wep_index];

	if (sendflags & 1)
	{
		wep->attack_time = MSG_ReadShort();
		wep->modelindex = MSG_ReadShort();
		wep->modelindex = bound(0, wep->modelindex, MAX_MODELS - 1);
	}

	if (sendflags & 2)
	{
		wep->impulse = MSG_ReadByte();
		wep->itemflag = MSG_ReadByte();
		if (wep->itemflag != 255)
		{
			wep->itemflag = 1 << wep->itemflag;
		}
	}

	if (sendflags & 4)
	{
		wep->anim_number = MSG_ReadByte();
		for (i = 0; i < wep->anim_number; i++)
		{
			weppredanim_t *anim = &wep->anim_states[i];

			anim->mdlframe = MSG_ReadByte() - 127;
			anim->flags = MSG_ReadByte();
			if (anim->flags & WEPPREDANIM_MOREBYTES)
			{
				anim->flags |= MSG_ReadByte() << 8;
			}
			if (anim->flags & WEPPREDANIM_SOUND)
			{
				anim->sound = MSG_ReadShort();
				anim->sound = bound(0, anim->sound, MAX_SOUNDS - 1);
				anim->soundmask = MSG_ReadShort();

				qbool add_sound_to_list = true;
				weppredsound_t *hold = NULL;
				weppredsound_t *snd = predictionsoundlist;
				while (snd)
				{
					if (snd->index == anim->sound && snd->mask == anim->soundmask)
					{
						add_sound_to_list = false;
						break;
					}

					hold = snd;
					snd = snd->next;
				}

				if (add_sound_to_list)
				{
					snd = malloc(sizeof(weppredsound_t));
					snd->index = anim->sound;
					snd->mask = anim->soundmask;
					snd->chan = (anim->flags & WEPPREDANIM_SOUNDAUTO) ? 0 : 1;
					snd->next = NULL;

					if (hold)
					{
						hold->next = snd;
					}
					else
					{
						predictionsoundlist = snd;
					}
				}
			}
			if (anim->flags & WEPPREDANIM_PROJECTILE)
			{
				anim->projectile_model = MSG_ReadShort();
				anim->projectile_model = bound(0, anim->projectile_model, MAX_MODELS - 1);
				for (k = 0; k < 3; k++)
				{
					anim->projectile_velocity[k] = MSG_ReadShort();
				}
				for (k = 0; k < 3; k++)
				{
					anim->projectile_offset[k] = MSG_ReadByte();
				}
			}
			anim->nextanim = MSG_ReadByte();
			if (anim->flags & WEPPREDANIM_BRANCH)
			{
				anim->altanim = MSG_ReadByte();
			}
			anim->length = MSG_ReadByte() * 10;
		}
	}
}


/*
==============================================================================
								 Weapon Prediction
==============================================================================
*/

#define IT_HOOK 32768
#define PRDFL_MIDAIR	1
#define PRDFL_COILGUN	2
#define PRDFL_FORCEOFF	255
ezcsqc_entity_t *viewweapon;

#if 0
qbool WeaponPred_SetModel(ezcsqc_entity_t *self)
{
	int oldframe = self->frame;
	int oldmodel = self->modelindex;

	switch (ws_predicted.weapon)
	{
	case IT_AXE: 					self->modelindex = cl_modelindices[mi_vaxe]; break;
	case IT_SHOTGUN:
		if (ws_server.client_predflags & PRDFL_COILGUN)
		{
			self->modelindex = cl_modelindices[mi_vgrap]; //FIXME: replace with coilgun
		}
		else
		{
			self->modelindex = cl_modelindices[mi_weapon2];
		}
		break;
	case IT_SUPER_SHOTGUN: 			self->modelindex = cl_modelindices[mi_weapon3]; break;
	case IT_NAILGUN: 				self->modelindex = cl_modelindices[mi_weapon4]; break;
	case IT_SUPER_NAILGUN: 			self->modelindex = cl_modelindices[mi_weapon5]; break;
	case IT_GRENADE_LAUNCHER: 		self->modelindex = cl_modelindices[mi_weapon6]; break;
	case IT_ROCKET_LAUNCHER: 		self->modelindex = cl_modelindices[mi_weapon7]; break;
	case IT_LIGHTNING: 				self->modelindex = cl_modelindices[mi_weapon8]; break;
	case IT_HOOK:					self->modelindex = cl_modelindices[mi_vgrap]; break;
	}

	self->frame = ws_predicted.frame;
}

void W_SetCurrentAmmo(ezcsqc_weapon_state_t *ws)
{
	switch (ws->weapon)
	{
	case IT_AXE:
		ws->current_ammo = 0;
		ws->weapon_index = 1;
		break;
	case IT_SHOTGUN:
		ws->current_ammo = ws->ammo_shells;
		if (ws->client_predflags & PRDFL_COILGUN)
		{
			ws->weapon_index = 9;
		}
		else
		{
			ws->weapon_index = 2;
		}
		break;
	case IT_SUPER_SHOTGUN:
		ws->current_ammo = ws->ammo_shells;
		ws->weapon_index = 3;
		break;
	case IT_NAILGUN:
		ws->current_ammo = ws->ammo_nails;
		ws->weapon_index = 4;
		break;
	case IT_SUPER_NAILGUN:
		ws->current_ammo = ws->ammo_nails;
		ws->weapon_index = 5;
		break;
	case IT_GRENADE_LAUNCHER:
		ws->current_ammo = ws->ammo_rockets;
		ws->weapon_index = 6;
		break;
	case IT_ROCKET_LAUNCHER:
		ws->current_ammo = ws->ammo_rockets;
		ws->weapon_index = 7;
		break;
	case IT_LIGHTNING:
		ws->current_ammo = ws->ammo_cells;
		ws->weapon_index = 8;
		break;
	case IT_HOOK:
		ws->current_ammo = 0;
		ws->weapon_index = 10;
		break;
	}
}
#endif

void WeaponPred_SetModel(ezcsqc_entity_t *self)
{
	weppreddef_t *wep = &wpredict_definitions[bound(0, ws_predicted.weapon_index, MAX_PREDWEPS-1)];
	// weppredanim_t *anim = WEPANIM(wep, ws_predicted.client_thinkindex);

	//self->modelindex = wep->modelindex;
	if (wep->modelindex)
	{
		self->modelindex = wep->modelindex;
		self->frame = ws_predicted.frame;
	}

	//Con_Printf("frame %i\n", ws_predicted.frame);
	//self->frame = ws_predicted.frame;
}

void WeaponPred_PlayEffects(usercmd_t *u, player_state_t *ps, ezcsqc_weapon_state_t *ws, weppredanim_t *anim)
{
	if (is_effectframe) // if is effect frame
	{
		if (anim->flags & WEPPREDANIM_SOUND)
		{
			int chan = 1;
			if (anim->flags & WEPPREDANIM_SOUNDAUTO)
			{
				chan = 0;
			}

			if (!(anim->flags & WEPPREDANIM_LTIME) || ws->client_time >= lg_twidth)
			{
				// play sound
				if (cl_predict_weaponsounds.integer == 1 || (cl_predict_weaponsounds.integer && !(cl_predict_weaponsounds.integer & anim->soundmask)))
				{
					S_StartSound(cl.playernum + 1, chan, cl.sound_precache[anim->sound], pmove.origin, 1, 0);
				}

				if (anim->flags & WEPPREDANIM_LTIME)
				{
					lg_twidth = ws->client_time + 0.6f;
				}
			}
		}

		if (anim->flags & WEPPREDANIM_MUZZLEFLASH)
		{
			// spawn muzzleflash
		}

		if (anim->flags & WEPPREDANIM_LGBEAM)
		{
			// spawn beam tent
		}

		if (anim->flags & WEPPREDANIM_PROJECTILE)
		{
			// spawn projectile
		}
	}
}


void WeaponPred_StartFrame(usercmd_t *u, player_state_t *ps, ezcsqc_weapon_state_t *ws, weppreddef_t *wep, int framenum)
{
	weppredanim_t *anim = WEPANIM(wep, framenum);

	if (!(anim->flags & WEPPREDANIM_ATTACK && !(anim->flags & WEPPREDANIM_BRANCH))) // if this anim wants to hold until +attack, don't play effects
	{
		WeaponPred_PlayEffects(u, ps, ws, anim);
	}

	ws->client_thinkindex = framenum;
	ws->client_nextthink = ws->client_time + LENGTH2S(anim->length);
	if (!anim->length)
	{
		ws->client_nextthink = 0;
	}

	if (anim->mdlframe >= 0)
	{
		ws->frame = anim->mdlframe;
	}
	else
	{
		ws->frame++;
		if (ws->frame > -anim->mdlframe)
		{
			ws->frame = 1;
		}
	}
}


void WeaponPred_WAttack(usercmd_t *u, player_state_t *ps, ezcsqc_weapon_state_t *ws)
{
	weppreddef_t *wep = &wpredict_definitions[bound(0, ws->weapon_index, MAX_PREDWEPS - 1)];
	weppredanim_t *anim = WEPANIM(wep, ws->client_thinkindex);

	if (anim->flags & WEPPREDANIM_ATTACK && !(anim->flags & WEPPREDANIM_BRANCH))
	{
		//if (anim->flags & WEPPREDANIM_DEFAULT && ws->client_time < ws->attack_finished)
		if (ws->client_time < ws->attack_finished)
		{
			return;
		}

		if (!(u->buttons & BUTTON_ATTACK))
		{
			return;
		}

		WeaponPred_PlayEffects(u, ps, ws, anim);

		ws->attack_finished = ws->client_time + LENGTH2S(wep->attack_time);
		WeaponPred_StartFrame(u, ps, ws, wep, anim->nextanim);
	}
	else
	{
		int i;

		if (ws->client_time < ws->attack_finished)
		{
			return;
		}

		if (!(u->buttons & BUTTON_ATTACK))
		{
			return;
		}

		for (i = 0; i < WEPPRED_MAXSTATES; i++)
		{
			anim = WEPANIM(wep, i);

			if (!(anim->flags & WEPPREDANIM_DEFAULT))
			{
				continue;
			}

			if (!(anim->flags & WEPPREDANIM_ATTACK))
			{
				break;
			}

			WeaponPred_PlayEffects(u, ps, ws, anim);

			ws->attack_finished = ws->client_time + LENGTH2S(wep->attack_time);
			WeaponPred_StartFrame(u, ps, ws, wep, anim->nextanim);
		}
	}
}


void WeaponPred_Logic(usercmd_t *u, player_state_t *ps, ezcsqc_weapon_state_t *ws)
{
	weppreddef_t *wep = &wpredict_definitions[bound(0, ws->weapon_index, MAX_PREDWEPS - 1)];
	weppredanim_t *anim = &wep->anim_states[ws->client_thinkindex];

	if (anim->flags & WEPPREDANIM_DEFAULT)
	{
		return;
	}

	if (!(anim->flags & WEPPREDANIM_ATTACK) || (anim->flags & WEPPREDANIM_BRANCH))
	{
		float time_held = ws->client_time;
		int frame_to_go = anim->nextanim;

		if (ws->client_time < ws->client_nextthink)
		{
			return;
		}

		//if (hold_time) // set time to the nextthink
		ws->client_time = ws->client_nextthink;

		if (anim->flags & WEPPREDANIM_ATTACK)
		{
			if (!(u->buttons & BUTTON_ATTACK) || ws->impulse)
			{
				frame_to_go = anim->altanim;
			}
			else
			{
				ws->attack_finished = ws->client_time + LENGTH2S(wep->attack_time);
			}
		}

		WeaponPred_StartFrame(u, ps, ws, wep, frame_to_go);

		//if (hold_time) // restore time
		ws->client_time = time_held;
	}
}


qbool WeaponPred_SwitchWeapon(int impulse, ezcsqc_weapon_state_t *ws)
{
	int i, found, items;

	if (ws->client_time < ws->attack_finished)
	{
		return false;
	}

	found = false;
	items = cl.stats[STAT_ITEMS];
	for (i = 0; i < MAX_PREDWEPS; i++)
	{
		weppreddef_t *wep = &wpredict_definitions[i];
		if (wep->impulse != impulse)
		{
			continue;
		}

		if (ws->weapon_index == i)
		{
			found = true;
			continue;
		}

		if (items & wep->itemflag)
		{
			ws->weapon_index = i;
			ws->weapon = wep->itemflag;
			ws->client_thinkindex = 0;
			ws->client_nextthink = 0;
			ws->frame = WEPANIM(wep, 0)->mdlframe;
			return true;
		}
	}

	return found;
}


void WeaponPred_Simulate(usercmd_t u, player_state_t ps, ezcsqc_weapon_state_t *ws)
{
	if (ps.pm_type == PM_DEAD || ps.pm_type == PM_NONE || ps.pm_type == PM_LOCK)
	{
		ws->impulse = 0;
		ws->attack_finished = ws->client_time + 0.05f;
		return;
	}

	ws->client_time += u.msec * 0.001f;

	if (u.impulse)
	{
		ws->impulse = u.impulse;
	}

	WeaponPred_Logic(&u, &ps, ws);

	if (ws->impulse)
	{
		if (WeaponPred_SwitchWeapon(ws->impulse, ws))
		{
			ws->impulse = 0;
		}
	}

	WeaponPred_WAttack(&u, &ps, ws);
}


qbool WeaponPred_Predraw(ezcsqc_entity_t *self)
{
	int i = 0;

	if (cls.netchan.outgoing_sequence - cl.validsequence <= 2)
	{
		i = -3;
		//Con_Printf("use oldserver\n");
	}

	ws_predicted = ws_server[(cl.validsequence + i) & UPDATE_MASK];

	for (; i < UPDATE_BACKUP - 1 && cl.validsequence + i < cls.netchan.outgoing_sequence; i++)
	{
		frame_t *to;
		to = &cl.frames[(cl.validsequence + i) & UPDATE_MASK];

		is_effectframe = false;
		if ((cl.validsequence + i > last_effectframe) &&
			((cl.validsequence + i) < (cls.netchan.outgoing_sequence - 1)))
		{
			is_effectframe = true;
			last_effectframe = cl.validsequence + i;
		}

		WeaponPred_Simulate(to->cmd, to->playerstate[cl.playernum], &ws_predicted);
		ws_saved[(cl.validsequence + i + 1) & UPDATE_MASK] = ws_predicted;
	}

	WeaponPred_SetModel(self);

	return false;
}

// Recieve struct update from SSQC
void EntUpdate_WeaponInfo(ezcsqc_entity_t *self, qbool is_new)
{
	int sendflags;
	ezcsqc_weapon_state_t *ws_current;
	ezcsqc_weapon_state_t *ws_error;
	sendflags = MSG_ReadByte();
	ws_current = &ws_server[(cl.validsequence + 1) & UPDATE_MASK];
	*ws_current = ws_server[(cl.validsequence) & UPDATE_MASK];
	ws_error = &ws_saved[(cl.validsequence + 1) & UPDATE_MASK];

	// parse the delta update to the struct
	if (sendflags & 0x01)
	{
		ws_current->impulse = MSG_ReadByte();
		ws_current->weapon_index = MSG_ReadByte();
	}
	if (sendflags & 0x02)
	{
		ws_current->ammo_shells = MSG_ReadByte();
	}
	if (sendflags & 0x04)
	{
		ws_current->ammo_nails = MSG_ReadByte();
	}
	if (sendflags & 0x08)
	{
		ws_current->ammo_rockets = MSG_ReadByte();
	}
	if (sendflags & 0x10)
	{
		ws_current->ammo_cells = MSG_ReadByte();
	}
	if (sendflags & 0x20)
	{
		ws_current->attack_finished = MSG_ReadFloat();
		ws_current->client_nextthink = MSG_ReadFloat();
		ws_current->client_thinkindex = MSG_ReadByte();
	}
	if (sendflags & 0x40)
	{
		ws_current->client_time = MSG_ReadFloat();
		ws_current->frame = MSG_ReadByte();
	}
	if (sendflags & 0x80)
	{
		ws_current->client_predflags = MSG_ReadByte();
		ws_current->client_ping = MSG_ReadByte() / 1000;
	}

#if 0
	if (ws_current->impulse != ws_error->impulse)
	{
		Con_Printf("%s: impulse desync, %i -> %i\n", __func__, (int)ws_error->impulse, (int)ws_current->impulse);
	}
#endif
#if 0
	if (ws_current->weapon_index != ws_error->weapon_index)
	{
		Con_Printf("%s: weapon_index desync, %i -> %i\n", __func__, ws_error->weapon_index, ws_current->weapon_index);
	}
#endif
#if 0
	if (ws_current->attack_finished != ws_error->attack_finished)
	{
		Con_Printf("%s: attack_finished desync, %g -> %g\n", __func__, ws_error->attack_finished, ws_current->attack_finished);
	}
#endif
#if 0
	if (ws_current->client_nextthink != ws_error->client_nextthink)
	{
		Con_Printf("%s: client_nextthink desync, %g -> %g\n", __func__, ws_error->client_nextthink, ws_current->client_nextthink);
	}
#endif
#if 1
	if (ws_current->client_thinkindex != ws_error->client_thinkindex)
	{
		Con_Printf("%s: client_thinkindex desync, %i -> %i\n", __func__, ws_error->client_thinkindex, ws_current->client_thinkindex);
	}
#endif
#if 0
	if (ws_current->client_time != ws_error->client_time)
	{
		Con_Printf("%s: client_time desync, %g -> %g\n", __func__, ws_error->client_time, ws_current->client_time);
	}
#endif
#if 0
	if (ws_current->frame != ws_error->frame)
	{
		Con_Printf("%s: frame desync, %i -> %i\n", __func__, ws_error->frame, ws_current->frame);
	}
#endif

	// if this is our first update, we need to do some entity setup
	if (is_new)
	{
		self->drawmask = DRAWMASK_VIEWMODEL;
		self->predraw = WeaponPred_Predraw;
		ezcsqc.weapon_prediction = true;
		viewweapon = self;
	}
}

void EntUpdate_Projectile(ezcsqc_entity_t *self, qbool is_new)
{
	int sendflags;
	sendflags = MSG_ReadByte();

	if (sendflags & 1)
	{
		self->s_origin[0] = MSG_ReadCoord();
		self->s_origin[1] = MSG_ReadCoord();
		self->s_origin[2] = MSG_ReadCoord();

		self->s_velocity[0] = MSG_ReadCoord();
		self->s_velocity[1] = MSG_ReadCoord();
		self->s_velocity[2] = MSG_ReadCoord();

		self->s_time = MSG_ReadFloat();
	}


	if (sendflags & 2)
	{
		self->modelindex = MSG_ReadShort();
		self->effects = MSG_ReadShort();

		self->modelflags = (cl.model_precache[self->modelindex])->flags;
		if ((cl.model_precache[self->modelindex])->modhint == MOD_SPIKE)
		{
			self->modelflags |= EF_NAILTRAIL;
		}
	}


	if (sendflags & 4)
	{
		self->angles[0] = MSG_ReadAngle();
		self->angles[1] = MSG_ReadAngle();
		self->angles[2] = MSG_ReadAngle();
	}


	if (sendflags & 8)
	{
		self->ownernum = MSG_ReadShort();
	}


	if (sendflags & 16)
	{
		vec3_t spawn_origin, diff;
		spawn_origin[0] = MSG_ReadCoord();
		spawn_origin[1] = MSG_ReadCoord();
		spawn_origin[2] = MSG_ReadCoord();

		VectorSubtract(spawn_origin, self->s_origin, diff);
		if (VectorLength(diff) < 150) // reconcile trail if it's less than 150u from the current origin
		{
			VectorCopy(spawn_origin, self->trail_origin);
		}
	}


	if (is_new)
	{
		self->drawmask = DRAWMASK_PROJECTILE;
		self->predraw = Predraw_Projectile;

		// if we didn't get a trail
		if (self->trail_origin[0] == 0 && self->trail_origin[1] == 0 && self->trail_origin[2] == 0)
		{
			VectorCopy(self->s_origin, self->trail_origin);
		}

		VectorCopy(self->s_origin, self->oldorigin);
	}
}

/*
==============================================================================
								 Networking
==============================================================================
*/

qbool CL_EZCSQC_Event_Sound(int entnum, int channel, int soundnumber, float vol, float attenuation, vec3_t pos, float pitchmod, float flags)
{
	weppredsound_t *snd;

	if (entnum == cl.playernum + 1)
	{
		if (!cl_predict_weaponsounds.integer)
		{
			return false;
		}

		for (snd = predictionsoundlist; snd; snd = snd->next)
		{
			if (snd->chan != channel)
			{
				continue;
			}

			if (snd->index != soundnumber)
			{
				continue;
			}

			if (cl_predict_weaponsounds.integer & snd->mask)
			{
				continue;
			}

			return true;
		}
	}

	return false;
}

void CL_EZCSQC_Ent_Update(ezcsqc_entity_t *self, qbool is_new)
{
	int type;
	type = MSG_ReadByte();

	switch (type)
	{
	case EZCSQC_PROJECTILE:
		EntUpdate_Projectile(self, is_new);
		break;
	case EZCSQC_WEAPONINFO:
		EntUpdate_WeaponInfo(self, is_new);
		break;
	case EZCSQC_WEAPONDEF:
		EntUpdate_WeaponDef(self, is_new);
		break;
	}
}

/*
==============================================================================
								 Rendering
==============================================================================
*/

void CL_EZCSQC_ViewmodelUpdate(player_state_t *view_message)
{
	if (ezcsqc.weapon_prediction && viewweapon)
	{
		cl.stats[STAT_WEAPON] = viewweapon->modelindex;
		view_message->weaponframe = viewweapon->frame;
	}
}

void CL_EZCSQC_RenderEntity(ezcsqc_entity_t *self)
{
	entity_t ent;

	memset(&ent, 0, sizeof(entity_t));
	ent.colormap = vid.colormap;
	VectorCopy(self->origin, ent.origin);
	VectorCopy(self->angles, ent.angles);
	ent.model = cl.model_precache[self->modelindex];
	ent.frame = self->frame;

	CL_AddEntity(&ent);
}

void CL_EZCSQC_RenderEntities(int drawmask)
{
	int i;

	for (i = 0; i < MAX_EDICTS; i++)
	{
		ezcsqc_entity_t *ent = &ezcsqc_entities[i];
		if (ent->isfree) // entity is not currently used
		{
			continue;
		}

		if (!(ent->drawmask & drawmask))
		{
			continue;
		}

		if (ent->predraw)
		{
			if (ent->predraw(ent))
			{
				CL_EZCSQC_RenderEntity(ent);
			}
			continue;
		}

		CL_EZCSQC_RenderEntity(ent);
	}
}

void CL_EZCSQC_RenderEntities_Ring(int drawmask, int index)
{
	int i = index;
	do
	{
		ezcsqc_entity_t *ent = &ezcsqc_entities[i];
		if (ent->isfree) // entity is not currently used
		{
			goto cont;
		}

		if (!(ent->drawmask & drawmask))
		{
			goto cont;
		}

		if (ent->predraw)
		{
			if (ent->predraw(ent))
			{
				CL_EZCSQC_RenderEntity(ent);
			}
			goto cont;
		}

		CL_EZCSQC_RenderEntity(ent);

		cont:
		i = (i + 1) & (MAX_EDICTS - 1);
	} while (i != index);
}

void CL_EZCSQC_UpdateView(void)
{
	CL_EZCSQC_RenderEntities(DRAWMASK_ENGINE | DRAWMASK_VIEWMODEL);

	// add projectiles and increment the index, so trails are kinda evenly spotty.
	CL_EZCSQC_RenderEntities_Ring(DRAWMASK_PROJECTILE, projectile_ringbufferseek = (projectile_ringbufferseek + 7) & (MAX_EDICTS - 1));
}
#endif