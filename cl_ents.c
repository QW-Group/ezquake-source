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

#include "quakedef.h"
#include "pmove.h"
#include "teamplay.h"

#ifdef GLQUAKE
#include "gl_local.h"
#endif

static int MVD_TranslateFlags(int src);		
void TP_ParsePlayerInfo(player_state_t *, player_state_t *, player_info_t *info);	

#define ISDEAD(i) ( (i) >= 41 && (i) <= 102 )

extern cvar_t cl_predictPlayers, cl_solidPlayers, cl_rocket2grenade;
extern cvar_t cl_model_bobbing;		
extern cvar_t cl_nolerp;

static struct predicted_player {
	int flags;
	qboolean active;
	vec3_t origin;	// predicted origin

	qboolean predict;
	vec3_t	oldo;
	vec3_t	olda;
	vec3_t	oldv;
	player_state_t *oldstate;

} predicted_players[MAX_CLIENTS];

char *cl_modelnames[cl_num_modelindices];
cl_modelindex_t cl_modelindices[cl_num_modelindices];


void CL_InitEnts(void) {
	int i;
	byte *memalloc;

	memset(cl_modelnames, 0, sizeof(cl_modelnames));

	cl_modelnames[mi_spike] = "progs/spike.mdl";
	cl_modelnames[mi_player] = "progs/player.mdl";
	cl_modelnames[mi_flag] = "progs/flag.mdl";
	cl_modelnames[mi_tf_flag] = "progs/tf_flag.mdl";
	cl_modelnames[mi_tf_stan] = "progs/tf_stan.mdl";
	cl_modelnames[mi_explod1] = "progs/s_explod.spr";
	cl_modelnames[mi_explod2] = "progs/s_expl.spr";
	cl_modelnames[mi_h_player] = "progs/h_player.mdl";
	cl_modelnames[mi_gib1] = "progs/gib1.mdl";
	cl_modelnames[mi_gib2] = "progs/gib2.mdl";
	cl_modelnames[mi_gib3] = "progs/gib3.mdl";
	cl_modelnames[mi_rocket] = "progs/missile.mdl";
	cl_modelnames[mi_grenade] = "progs/grenade.mdl";
	cl_modelnames[mi_bubble] = "progs/s_bubble.spr";
	cl_modelnames[mi_vaxe] = "progs/v_axe.mdl";
	cl_modelnames[mi_vbio] = "progs/v_bio.mdl";
	cl_modelnames[mi_vgrap] = "progs/v_grap.mdl";
	cl_modelnames[mi_vknife] = "progs/v_knife.mdl";
	cl_modelnames[mi_vknife2] = "progs/v_knife2.mdl";
	cl_modelnames[mi_vmedi] = "progs/v_medi.mdl";
	cl_modelnames[mi_vspan] = "progs/v_span.mdl";
	
	for (i = 0; i < cl_num_modelindices; i++) {
		if (!cl_modelnames[i])
			Sys_Error("cl_modelnames[%d] not initialized", i);
	}

#ifdef GLQUAKE
	cl_firstpassents.max = 64;
	cl_firstpassents.alpha = 0;

	cl_visents.max = 256;
	cl_visents.alpha = 0;

	cl_alphaents.max = 64;
	cl_alphaents.alpha = 1;

	memalloc = Hunk_AllocName((cl_firstpassents.max + cl_visents.max + cl_alphaents.max) * sizeof(entity_t), "visents");
	cl_firstpassents.list = (entity_t *) memalloc;
	cl_visents.list = (entity_t *) memalloc + cl_firstpassents.max;
	cl_alphaents.list = (entity_t *) memalloc + cl_firstpassents.max + cl_visents.max;
#else
	cl_visents.max = 256;
	cl_visents.alpha = 0;

	cl_visbents.max = 256;
	cl_visbents.alpha = 0;

	memalloc = Hunk_AllocName((cl_visents.max + cl_visbents.max) * sizeof(entity_t), "visents");
	cl_visents.list = (entity_t *) memalloc;
	cl_visbents.list = (entity_t *) memalloc + cl_visents.max;
#endif

	CL_ClearScene();
}

void CL_ClearScene (void) {
#ifdef GLQUAKE
	cl_firstpassents.count = cl_visents.count = cl_alphaents.count = 0;
#else
	cl_visents.count = cl_visbents.count = 0;
#endif
}

void CL_AddEntity (entity_t *ent) {
	visentlist_t *vislist;

#ifdef GLQUAKE
	if (ent->model->type == mod_sprite) {
		vislist = &cl_alphaents;
	} else if (ent->model->modhint == MOD_PLAYER || ent->model->modhint == MOD_EYES) {
		vislist = &cl_firstpassents;
		ent->flags |= RF_NOSHADOW;
	} else {
		vislist = &cl_visents;
	}
#else
	if (ent->model->type == mod_brush)
		vislist = &cl_visbents;
	else
		vislist = &cl_visents;
#endif

	if (vislist->count < vislist->max)
		vislist->list[vislist->count++] = *ent;
}


dlighttype_t dlightColor(float f, dlighttype_t def, qboolean random) {
	dlighttype_t colors[NUM_DLIGHTTYPES - 4] = {lt_red, lt_blue, lt_redblue, lt_green, lt_white};

	if ((int) f == 1)
		return lt_red;
	else if ((int) f == 2)
		return lt_blue;
	else if ((int) f == 3) 
		return lt_redblue;
	else if ((int) f == 4)
		return lt_green;
	else if ((int) f == 5)
		return lt_white;
	else if (((int) f == NUM_DLIGHTTYPES - 3) && random)
		return colors[rand() % (NUM_DLIGHTTYPES - 4)];
	else
		return def;
}

dlight_t *CL_AllocDlight (int key) {
	int i;
	dlight_t *dl;

	// first look for an exact key match
	if (key) {
		dl = cl_dlights;
		for (i = 0; i < MAX_DLIGHTS; i++, dl++) {
			if (dl->key == key) {
				memset (dl, 0, sizeof(*dl));
				dl->key = key;
				return dl;
			}
		}
	}

	// then look for anything else
	dl = cl_dlights;
	for (i = 0; i < MAX_DLIGHTS; i++, dl++) {
		if (dl->die < cl.time) {
			memset (dl, 0, sizeof(*dl));
			dl->key = key;
			return dl;
		}
	}

	dl = &cl_dlights[0];
	memset (dl, 0, sizeof(*dl));
	dl->key = key;
	return dl;
}


void CL_NewDlight (int key, vec3_t origin, float radius, float time, int type, int bubble) {
	dlight_t *dl;

	dl = CL_AllocDlight (key);
	VectorCopy (origin, dl->origin);
	dl->radius = radius;
	dl->die = cl.time + time;
	dl->type = type;
	dl->bubble = bubble;		
}

void CL_DecayLights (void) {
	int i;
	dlight_t *dl;

	if (cls.state < ca_active)
		return;

	dl = cl_dlights;
	for (i = 0; i < MAX_DLIGHTS; i++, dl++) {
		if (dl->die < cl.time || !dl->radius)
			continue;

		dl->radius -= cls.frametime * dl->decay;
		if (dl->radius < 0)
			dl->radius = 0;
	}
}

void CL_SetupPacketEntity (int number, entity_state_t *state, qboolean changed) {
	qboolean nolerp = false;
	centity_t *cent;

	cent = &cl_entities[number];

	if (!cl.oldvalidsequence || cl.oldvalidsequence != cent->sequence ||
		state->modelindex != cent->current.modelindex ||
		!VectorL2Compare(state->origin, cent->current.origin, 200)
	) {
		cent->startlerp = cl.time;
		cent->deltalerp = -1;
		cent->frametime = -1;
		cent->oldsequence = 0;
		cent->flags = 0;
 	} else {
		cent->oldsequence = cent->sequence;
		
		if (state->frame != cent->current.frame) {
			cent->frametime = cl.time;
			cent->oldframe = cent->current.frame;
		}
		
		if (changed) {
			if (!VectorCompare(state->origin, cent->current.origin) || !VectorCompare(state->angles, cent->current.angles)) {
				VectorCopy(cent->current.origin, cent->old_origin);
				VectorCopy(cent->current.angles, cent->old_angles);
				if (cls.mvdplayback) {
					cent->deltalerp = nextdemotime - olddemotime;
					cent->startlerp = cls.demotime;
				} else {
					cent->deltalerp = cl.time - cent->startlerp;
					cent->startlerp = cl.time;
				}
			}
		}
	}
 
	cent->current = *state;
	cent->sequence = cl.validsequence;
}

//Can go from either a baseline or a previous packet_entity
void CL_ParseDelta (entity_state_t *from, entity_state_t *to, int bits) {
	int i;

	// set everything to the state we are delta'ing from
	*to = *from;

	to->number = bits & 511;
	bits &= ~511;

	if (bits & U_MOREBITS) {	// read in the low order bits
		i = MSG_ReadByte ();
		bits |= i;
	}

	to->flags = bits;

	if (bits & U_MODEL)
		to->modelindex = MSG_ReadByte ();

	if (bits & U_FRAME)
		to->frame = MSG_ReadByte ();

	if (bits & U_COLORMAP)
		to->colormap = MSG_ReadByte();

	if (bits & U_SKIN)
		to->skinnum = MSG_ReadByte();

	if (bits & U_EFFECTS)
		to->effects = MSG_ReadByte();

	if (bits & U_ORIGIN1)
		to->origin[0] = MSG_ReadCoord ();

	if (bits & U_ANGLE1)
		to->angles[0] = MSG_ReadAngle();

	if (bits & U_ORIGIN2)
		to->origin[1] = MSG_ReadCoord ();

	if (bits & U_ANGLE2)
		to->angles[1] = MSG_ReadAngle();

	if (bits & U_ORIGIN3)
		to->origin[2] = MSG_ReadCoord ();

	if (bits & U_ANGLE3)
		to->angles[2] = MSG_ReadAngle();

	if (bits & U_SOLID) {
		// FIXME
	}
}

void FlushEntityPacket (void) {
	int word;
	entity_state_t olde, newe;

	Com_DPrintf ("FlushEntityPacket\n");

	memset (&olde, 0, sizeof(olde));

	cl.delta_sequence = 0;
	cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK].invalid = true;

	// read it all, but ignore it
	while (1) {
		word = (unsigned short) MSG_ReadShort ();
		if (msg_badread) {	// something didn't parse right...
			Host_Error ("msg_badread in packetentities");
			return;
		}

		if (!word)
			break;	// done

		CL_ParseDelta (&olde, &newe, word);
	}
}

//An svc_packetentities has just been parsed, deal with the rest of the data stream.
void CL_ParsePacketEntities (qboolean delta) {
	int oldpacket, newpacket, oldindex, newindex, word, newnum, oldnum;
	packet_entities_t *oldp, *newp, dummy;
	qboolean full;
	byte from;

	newpacket = cls.netchan.incoming_sequence & UPDATE_MASK;
	newp = &cl.frames[newpacket].packet_entities;
	cl.frames[newpacket].invalid = false;

	if (delta) {
		from = MSG_ReadByte ();

		oldpacket = cl.frames[newpacket].delta_sequence;
		if (cls.mvdplayback)	
			from = oldpacket = cls.netchan.incoming_sequence - 1;

		if (cls.netchan.outgoing_sequence - cls.netchan.incoming_sequence >= UPDATE_BACKUP - 1) {
			// there are no valid frames left, so drop it
			FlushEntityPacket ();
			cl.validsequence = 0;
			return;
		}

		if ((from & UPDATE_MASK) != (oldpacket & UPDATE_MASK)) {
			Com_DPrintf ("WARNING: from mismatch\n");
			FlushEntityPacket ();
			cl.validsequence = 0;
			return;
		}

		if (cls.netchan.outgoing_sequence - oldpacket >= UPDATE_BACKUP - 1) {
			// we can't use this, it is too old
			FlushEntityPacket ();
			// don't clear cl.validsequence, so that frames can still be rendered;
			// it is possible that a fresh packet will be received before
			// (outgoing_sequence - incoming_sequence) exceeds UPDATE_BACKUP - 1
			return;
		}

		oldp = &cl.frames[oldpacket & UPDATE_MASK].packet_entities;
		full = false;
	} else {
		// this is a full update that we can start delta compressing from now
		oldp = &dummy;
		dummy.num_entities = 0;
		full = true;
	}

	cl.oldvalidsequence = cl.validsequence;
	cl.validsequence = cls.netchan.incoming_sequence;
	cl.delta_sequence = cl.validsequence;

	oldindex = 0;
	newindex = 0;
	newp->num_entities = 0;

	while (1) {
		word = (unsigned short) MSG_ReadShort ();
		if (msg_badread) {
			// something didn't parse right...
			Host_Error ("msg_badread in packetentities");
			return;
		}

		if (!word) {
			while (oldindex < oldp->num_entities) {
				// copy all the rest of the entities from the old packet
				if (newindex >= MAX_MVD_PACKET_ENTITIES || (newindex >= MAX_PACKET_ENTITIES && !cls.mvdplayback))
					Host_Error ("CL_ParsePacketEntities: newindex == MAX_PACKET_ENTITIES");
				newp->entities[newindex] = oldp->entities[oldindex];
				CL_SetupPacketEntity(newp->entities[newindex].number, &newp->entities[newindex], false);
				newindex++;
				oldindex++;
			}
			break;
		}
		newnum = word & 511;
		oldnum = oldindex >= oldp->num_entities ? 9999 : oldp->entities[oldindex].number;

		while (newnum > oldnum) 	{
			if (full) {
				Com_Printf ("WARNING: oldcopy on full update");
				FlushEntityPacket ();
				cl.validsequence = 0;	// can't render a frame
				return;
			}

			// copy one of the old entities over to the new packet unchanged
			if (newindex >= MAX_MVD_PACKET_ENTITIES || (newindex >= MAX_PACKET_ENTITIES && !cls.mvdplayback))
				Host_Error ("CL_ParsePacketEntities: newindex == MAX_PACKET_ENTITIES");
			newp->entities[newindex] = oldp->entities[oldindex];
			CL_SetupPacketEntity(oldnum, &newp->entities[newindex], word > 511);
			newindex++;
			oldindex++;
			oldnum = oldindex >= oldp->num_entities ? 9999 : oldp->entities[oldindex].number;
		}

		if (newnum < oldnum) {
			// new from baseline
			if (word & U_REMOVE) {
				if (full) {
					Com_Printf ("WARNING: U_REMOVE on full update\n");
					FlushEntityPacket ();
					cl.validsequence = 0;	// can't render a frame
					return;
				}
				continue;
			}
			if (newindex >= MAX_MVD_PACKET_ENTITIES || (newindex >= MAX_PACKET_ENTITIES && !cls.mvdplayback))
				Host_Error ("CL_ParsePacketEntities: newindex == MAX_PACKET_ENTITIES");
			CL_ParseDelta (&cl_entities[newnum].baseline, &newp->entities[newindex], word);
			CL_SetupPacketEntity (newnum, &newp->entities[newindex], word > 511); 
			newindex++;
			continue;
		}

		if (newnum == oldnum) {
			// delta from previous
			if (full) {
				cl.validsequence = 0;
				cl.delta_sequence = 0;
				Com_Printf ("WARNING: delta on full update");
			}
			if (word & U_REMOVE) {
				oldindex++;
				continue;
			}
			CL_ParseDelta (&oldp->entities[oldindex], &newp->entities[newindex], word);
			CL_SetupPacketEntity (newnum, &newp->entities[newindex], word > 511);
			newindex++;
			oldindex++;
		}
	}

	newp->num_entities = newindex;

	if (cls.state == ca_onserver)	// we can now render a frame
		CL_MakeActive();
}

void CL_LinkPacketEntities (void) {
	entity_t ent;
	centity_t *cent;
	packet_entities_t *pack;
	entity_state_t *state;
	model_t *model;
	vec3_t *old_origin;
	double time;
	float autorotate, lerp;
	int i, pnum, rocketlightsize, flicker;
	dlighttype_t rocketlightcolor, dimlightcolor;

	pack = &cl.frames[cl.validsequence & UPDATE_MASK].packet_entities;

	autorotate = anglemod(100 * cl.time);

	memset (&ent, 0, sizeof(ent));

	for (pnum = 0; pnum < pack->num_entities; pnum++) {
		state = &pack->entities[pnum];
		cent = &cl_entities[state->number];

		// control powerup glow for bots
		if (state->modelindex != cl_modelindices[mi_player] || r_powerupglow.value) {
			flicker = r_lightflicker.value ? (rand() & 31) : 0;
			// spawn light flashes, even ones coming from invisible objects
			if ((state->effects & (EF_BLUE | EF_RED)) == (EF_BLUE | EF_RED)) {
				CL_NewDlight (state->number, state->origin, 200 + flicker, 0.1, lt_redblue, 0);
			} else if (state->effects & EF_BLUE) {
				CL_NewDlight (state->number, state->origin, 200 + flicker, 0.1, lt_blue, 0);
			} else if (state->effects & EF_RED) {
				CL_NewDlight (state->number, state->origin, 200 + flicker, 0.1, lt_red, 0);
			} else if (state->effects & EF_BRIGHTLIGHT) {
				vec3_t	tmp;
				VectorCopy (state->origin, tmp);
				tmp[2] += 16;
				CL_NewDlight (state->number, tmp, 400 + flicker, 0.1, lt_default, 0);
			} else if (state->effects & EF_DIMLIGHT) {
				if (cl.teamfortress && (state->modelindex == cl_modelindices[mi_tf_flag] || state->modelindex == cl_modelindices[mi_tf_stan]))
					dimlightcolor = dlightColor(r_flagcolor.value, lt_default, false);
				else if (state->modelindex == cl_modelindices[mi_flag])
					dimlightcolor = dlightColor(r_flagcolor.value, lt_default, false);
				else
					dimlightcolor = lt_default;
				CL_NewDlight (state->number, state->origin, 200 + flicker, 0.1, dimlightcolor, 0);
			}
		}

		if (!state->modelindex)		// if set to invisible, skip
			continue;

		if (state->modelindex == cl_modelindices[mi_player]) {
			i = state->frame;

			if (cl_deadbodyfilter.value == 2) {
				if (ISDEAD(i))
					continue;
			} else if (cl_deadbodyfilter.value) {
				if (i == 49 || i == 60 || i == 69 || i == 84 || i == 93 || i == 102)
					continue;
			}
		}

		if (cl_gibfilter.value && (state->modelindex == cl_modelindices[mi_h_player] || 
		 state->modelindex == cl_modelindices[mi_gib1] || state->modelindex == cl_modelindices[mi_gib2] || state->modelindex == cl_modelindices[mi_gib3]))
			continue;

		if (!(model = cl.model_precache[state->modelindex]))
			Host_Error ("CL_LinkPacketEntities: bad modelindex");

		ent.model = model;
		if (state->modelindex == cl_modelindices[mi_rocket]) {
			if (cl_rocket2grenade.value && cl_modelindices[mi_grenade] != -1)
				ent.model = cl.model_precache[cl_modelindices[mi_grenade]];
		}
		ent.skinnum = state->skinnum;
	
		ent.frame = state->frame;
		if (cent->frametime >= 0 && cent->frametime <= cl.time) {
			ent.oldframe = cent->oldframe;
			ent.framelerp = (cl.time - cent->frametime) * 10;
		} else {
			ent.oldframe = ent.frame;
			ent.framelerp = -1;
		}
	

		if (state->colormap && state->colormap < MAX_CLIENTS && ent.model->modhint == MOD_PLAYER) {
			ent.colormap = cl.players[state->colormap - 1].translations;
			ent.scoreboard = &cl.players[state->colormap - 1];
		} else {
			ent.colormap = vid.colormap;
			ent.scoreboard = NULL;
		}

	
		if ((cl_nolerp.value && !cls.mvdplayback) || cent->deltalerp <= 0) {
			lerp = -1;
			VectorCopy(cent->current.origin, ent.origin);
		} else {
			time = cls.mvdplayback ? cls.demotime : cl.time;
			lerp = (time - cent->startlerp) / (cent->deltalerp);
			lerp = min(lerp, 1);	
			VectorInterpolate(cent->old_origin, lerp, cent->current.origin, ent.origin);
		}
	

		if (ent.model->flags & EF_ROTATE) {
			ent.angles[0] = ent.angles[2] = 0;
			ent.angles[1] = autorotate;
			if (cl_model_bobbing.value)
				ent.origin[2] += sin(autorotate / 90 * M_PI) * 5 + 5;
		} else {
			if (lerp != -1) {
				AngleInterpolate(cent->old_angles, lerp, cent->current.angles, ent.angles);
			} else {
				VectorCopy(cent->current.angles, ent.angles);
			}
		}
	

	
	#ifdef GLQUAKE
		if (qmb_initialized) {
			if (state->modelindex == cl_modelindices[mi_explod1] || state->modelindex == cl_modelindices[mi_explod2]) {
				if (gl_part_inferno.value) {
					QMB_InfernoFlame (ent.origin);
					continue;
				}
			}

			if (state->modelindex == cl_modelindices[mi_bubble]) {
				QMB_StaticBubble (&ent);
				continue;
			}
		}
	#endif

		//add trails
		if (model->flags & ~EF_ROTATE) {
			if (!(cent->flags & CENT_TRAILDRAWN) || !VectorL2Compare(cent->trail_origin, ent.origin, 140)) {
				old_origin = &ent.origin;	//not present last frame or too far away
				cent->flags |= CENT_TRAILDRAWN;
			} else {
				old_origin = &cent->trail_origin;
			}

			if (model->flags & EF_ROCKET) {
				if (r_rockettrail.value) {
					if (r_rockettrail.value == 2)
						R_ParticleTrail (*old_origin, ent.origin, &cent->trail_origin, GRENADE_TRAIL);
					else if (r_rockettrail.value == 4)
						R_ParticleTrail (*old_origin, ent.origin, &cent->trail_origin, BLOOD_TRAIL);
					else if (r_rockettrail.value == 5)
						R_ParticleTrail (*old_origin, ent.origin, &cent->trail_origin, BIG_BLOOD_TRAIL);
					else if (r_rockettrail.value == 6)
						R_ParticleTrail (*old_origin, ent.origin, &cent->trail_origin, TRACER1_TRAIL);
					else if (r_rockettrail.value == 7)
						R_ParticleTrail (*old_origin, ent.origin, &cent->trail_origin, TRACER2_TRAIL);
					else if (r_rockettrail.value == 8)
						R_ParticleTrail (*old_origin, ent.origin, &cent->trail_origin, VOOR_TRAIL);
					else if (r_rockettrail.value == 3)
						R_ParticleTrail (*old_origin, ent.origin, &cent->trail_origin, ALT_ROCKET_TRAIL);
					else
						R_ParticleTrail (*old_origin, ent.origin, &cent->trail_origin, ROCKET_TRAIL);
				} else {
					VectorCopy(ent.origin, cent->trail_origin);		
				}

				if (r_rocketlight.value) {
					rocketlightcolor = dlightColor(r_rocketlightcolor.value, lt_rocket, false);
					rocketlightsize = 100 * (1 + bound(0, r_rocketlight.value, 1));	
					CL_NewDlight (state->number, ent.origin, rocketlightsize, 0.1, rocketlightcolor, 1);
				}
			} else if (model->flags & EF_GRENADE) {
				if (r_grenadetrail.value)
					R_ParticleTrail (*old_origin, ent.origin, &cent->trail_origin, GRENADE_TRAIL);
				else
					VectorCopy(ent.origin, cent->trail_origin);		
			} else if (model->flags & EF_GIB) {
				R_ParticleTrail (*old_origin, ent.origin, &cent->trail_origin, BLOOD_TRAIL);
			} else if (model->flags & EF_ZOMGIB) {
				R_ParticleTrail (*old_origin, ent.origin, &cent->trail_origin, BIG_BLOOD_TRAIL);
			} else if (model->flags & EF_TRACER) {
				R_ParticleTrail (*old_origin, ent.origin, &cent->trail_origin, TRACER1_TRAIL);
			} else if (model->flags & EF_TRACER2) {
				R_ParticleTrail (*old_origin, ent.origin, &cent->trail_origin, TRACER2_TRAIL);
			} else if (model->flags & EF_TRACER3) {
				R_ParticleTrail (*old_origin, ent.origin, &cent->trail_origin, VOOR_TRAIL);
			}
		}
		VectorCopy (ent.origin, cent->lerp_origin);
		CL_AddEntity (&ent);
	}
}


typedef struct {
	int		modelindex;
	vec3_t	origin;
	vec3_t	angles;
	int		num;	
} projectile_t;

projectile_t	cl_projectiles[MAX_PROJECTILES];
int				cl_num_projectiles;

projectile_t	cl_oldprojectiles[MAX_PROJECTILES];		
int				cl_num_oldprojectiles;					

void CL_ClearProjectiles (void) {

	if (cls.mvdplayback) {
		static int parsecount = 0;

		if (parsecount == cl.parsecount)
			return;

		parsecount = cl.parsecount;
		memset(cl.int_projectiles, 0, sizeof(cl.int_projectiles));
		cl_num_oldprojectiles = cl_num_projectiles;
		memcpy(cl_oldprojectiles, cl_projectiles, sizeof(cl_projectiles));
	}

	cl_num_projectiles = 0;
}

//Nails are passed as efficient temporary entities
void CL_ParseProjectiles (qboolean indexed) {					
	int i, c, j, num;
	byte bits[6];
	projectile_t *pr;
	interpolate_t *int_projectile;	

	c = MSG_ReadByte ();
	for (i = 0; i < c; i++) {
		num = indexed ? MSG_ReadByte() : 0;	

		for (j = 0 ; j < 6 ; j++)
			bits[j] = MSG_ReadByte ();

		if (cl_num_projectiles == MAX_PROJECTILES)
			continue;

		pr = &cl_projectiles[cl_num_projectiles];
		int_projectile = &cl.int_projectiles[cl_num_projectiles];	
		cl_num_projectiles++;

		pr->modelindex = cl_modelindices[mi_spike];
		pr->origin[0] = (( bits[0] + ((bits[1] & 15) << 8)) << 1) - 4096;
		pr->origin[1] = (((bits[1] >> 4) + (bits[2] << 4)) << 1) - 4096;
		pr->origin[2] = ((bits[3] + ((bits[4] & 15) << 8)) << 1) - 4096;
		pr->angles[0] = 360 * (bits[4] >> 4) / 16;
		pr->angles[1] = 360 * bits[5] / 256;

		if (!(pr->num = num))
			continue;

		
		for (j = 0; j < cl_num_oldprojectiles; j++) {
			if (cl_oldprojectiles[j].num == num) {
				int_projectile->interpolate = true;
				int_projectile->oldindex = j;
				VectorCopy(pr->origin, int_projectile->origin);
				break;
			}
		}		
	}	
}

void CL_LinkProjectiles (void) {
	int i;
	projectile_t *pr;
	entity_t ent;

	memset (&ent, 0, sizeof(entity_t));
	ent.colormap = vid.colormap;

	for (i = 0, pr = cl_projectiles; i < cl_num_projectiles; i++, pr++)	{
		if (pr->modelindex < 1)
			continue;
		ent.model = cl.model_precache[pr->modelindex];
		VectorCopy (pr->origin, ent.origin);
		VectorCopy (pr->angles, ent.angles);
		CL_AddEntity (&ent);
	}
}

void SetupPlayerEntity(int num, player_state_t *state) {
	centity_t *cent;

	cent = &cl_entities[num];

	if (!cl.oldparsecount || cl.oldparsecount != cent->sequence ||
		state->modelindex != cent->current.modelindex ||
		!VectorL2Compare(state->origin, cent->current.origin, 200)
	) {
		cent->startlerp = cl.time;
		cent->deltalerp = -1;
		cent->frametime = -1;
	} else {
		
		if (state->frame != cent->current.frame) {
			cent->frametime = cl.time;
			cent->oldframe = cent->current.frame;
		}
		
		if (!VectorCompare(state->origin, cent->current.origin) || !VectorCompare(state->viewangles, cent->current.angles)) {
			VectorCopy(cent->current.origin, cent->old_origin);
			VectorCopy(cent->current.angles, cent->old_angles);
			if (cls.mvdplayback) {
				cent->deltalerp = nextdemotime - olddemotime;
				cent->startlerp = cls.demotime;
			} else {
				cent->deltalerp = cl.time - cent->startlerp;
				cent->startlerp = cl.time;
			}
		}
	}
	
	
	VectorCopy(state->origin, cent->current.origin);
	VectorCopy(state->viewangles, cent->current.angles);
	cent->current.frame = state->frame;
	cent->sequence = state->messagenum;	
	cent->current.modelindex = state->modelindex;	
}

extern int parsecountmod;
extern double parsecounttime;

player_state_t oldplayerstates[MAX_CLIENTS];	

void CL_ParsePlayerinfo (void) {
	int	msec, flags, pm_code;
	centity_t *cent;
	player_info_t *info;
	player_state_t *state;

	player_state_t *prevstate, dummy;
	int num, i;


	extern void TP_ParsePlayerInfo(player_state_t *, player_state_t *, player_info_t *info);


	num = MSG_ReadByte ();
	if (num >= MAX_CLIENTS)
		Host_Error ("CL_ParsePlayerinfo: num >= MAX_CLIENTS");

	info = &cl.players[num];
	state = &cl.frames[parsecountmod].playerstate[num];
	cent = &cl_entities[num + 1];


	if (cls.mvdplayback) {
		if (!cl.parsecount || cent->sequence > cl.parsecount || cl.parsecount - cent->sequence >= UPDATE_BACKUP - 1) {
			memset(&dummy, 0, sizeof(dummy));
			prevstate = &dummy;
		} else {
			prevstate = &cl.frames[cent->sequence & UPDATE_MASK].playerstate[num];
		}
		memcpy(state, prevstate, sizeof(player_state_t));

		if (cls.findtrack && info->stats[STAT_HEALTH] > 0)	{
			extern int ideal_track;
			autocam = CAM_TRACK;
			Cam_Lock(num);
			ideal_track = num;
			cls.findtrack = false;
		}

		flags = MSG_ReadShort ();
		state->flags = MVD_TranslateFlags(flags);

		state->messagenum = cl.parsecount;
		state->command.msec = 0;

		state->frame = MSG_ReadByte ();

		state->state_time = parsecounttime;
		state->command.msec = 0;

		for (i = 0; i < 3; i++) {
			if (flags & (DF_ORIGIN << i))
				state->origin[i] = MSG_ReadCoord ();
		}

		for (i = 0; i < 3; i++) {
			if (flags & (DF_ANGLES << i))
				state->command.angles[i] = MSG_ReadAngle16 ();
		}

		if (flags & DF_MODEL)
			state->modelindex = MSG_ReadByte ();

		if (flags & DF_SKINNUM)
			state->skinnum = MSG_ReadByte ();

		if (flags & DF_EFFECTS)
			state->effects = MSG_ReadByte ();

		if (flags & DF_WEAPONFRAME)
			state->weaponframe = MSG_ReadByte ();
	} else {

		flags = state->flags = MSG_ReadShort ();

		state->messagenum = cl.parsecount;
		state->origin[0] = MSG_ReadCoord ();
		state->origin[1] = MSG_ReadCoord ();
		state->origin[2] = MSG_ReadCoord ();

		state->frame = MSG_ReadByte ();

		// the other player's last move was likely some time before the packet was sent out,
		// so accurately track the exact time it was valid at
		if (flags & PF_MSEC) {
			msec = MSG_ReadByte ();
			state->state_time = parsecounttime - msec * 0.001;
		} else {
			state->state_time = parsecounttime;
		}

		if (flags & PF_COMMAND)
			MSG_ReadDeltaUsercmd (&nullcmd, &state->command, cl.protoversion);

		for (i = 0; i < 3; i++) {
			if (flags & (PF_VELOCITY1 << i) )
				state->velocity[i] = MSG_ReadShort();
			else
				state->velocity[i] = 0;
		}
		if (flags & PF_MODEL)
			state->modelindex = MSG_ReadByte ();
		else
			state->modelindex = cl_modelindices[mi_player];

		if (flags & PF_SKINNUM)
			state->skinnum = MSG_ReadByte ();
		else
			state->skinnum = 0;

		if (flags & PF_EFFECTS)
			state->effects = MSG_ReadByte ();
		else
			state->effects = 0;

		if (flags & PF_WEAPONFRAME)
			state->weaponframe = MSG_ReadByte ();
		else
			state->weaponframe = 0;

		if (cl.z_ext & Z_EXT_PM_TYPE) {
			pm_code = (flags >> PF_PMC_SHIFT) & PF_PMC_MASK;

			if (pm_code == PMC_NORMAL || pm_code == PMC_NORMAL_JUMP_HELD) {
				if (flags & PF_DEAD) {
					state->pm_type = PM_DEAD;
				} else {
					state->pm_type = PM_NORMAL;
					state->jump_held = (pm_code == PMC_NORMAL_JUMP_HELD);
				}
			} else if (pm_code == PMC_OLD_SPECTATOR) {
				state->pm_type = PM_OLD_SPECTATOR;
			} else if ((cl.z_ext & Z_EXT_PM_TYPE_NEW) && pm_code == PMC_SPECTATOR) {
				state->pm_type = PM_SPECTATOR;
			} else if ((cl.z_ext & Z_EXT_PM_TYPE_NEW) && pm_code == PMC_FLY) {
				state->pm_type = PM_FLY;
			} else {
				// future extension?
				goto guess_pm_type;
			}
		} else {
guess_pm_type:
			if (cl.players[num].spectator)
				state->pm_type = PM_OLD_SPECTATOR;
			else if (flags & PF_DEAD)
				state->pm_type = PM_DEAD;
			else
				state->pm_type = PM_NORMAL;
		}
	}

	VectorCopy (state->command.angles, state->viewangles);
	
	TP_ParsePlayerInfo(&oldplayerstates[num], state, info);
	oldplayerstates[num] = *state;
	
	SetupPlayerEntity(num + 1, state);
}


//Called when the CTF flags are set
void CL_AddFlagModels (entity_t *ent, int team) {
	int i;
	float f;
	vec3_t v_forward, v_right;
	entity_t newent;

	if (cl_modelindices[mi_flag] == -1)
		return;

	f = 14;
	if (ent->frame >= 29 && ent->frame <= 40) {
		if (ent->frame >= 29 && ent->frame <= 34) { //axpain
			if      (ent->frame == 29) f = f + 2; 
			else if (ent->frame == 30) f = f + 8;
			else if (ent->frame == 31) f = f + 12;
			else if (ent->frame == 32) f = f + 11;
			else if (ent->frame == 33) f = f + 10;
			else if (ent->frame == 34) f = f + 4;
		} else if (ent->frame >= 35 && ent->frame <= 40) { // pain
			if      (ent->frame == 35) f = f + 2; 
			else if (ent->frame == 36) f = f + 10;
			else if (ent->frame == 37) f = f + 10;
			else if (ent->frame == 38) f = f + 8;
			else if (ent->frame == 39) f = f + 4;
			else if (ent->frame == 40) f = f + 2;
		}
	} else if (ent->frame >= 103 && ent->frame <= 118) {
		if      (ent->frame >= 103 && ent->frame <= 104) f = f + 6;  //nailattack
		else if (ent->frame >= 105 && ent->frame <= 106) f = f + 6;  //light 
		else if (ent->frame >= 107 && ent->frame <= 112) f = f + 7;  //rocketattack
		else if (ent->frame >= 112 && ent->frame <= 118) f = f + 7;  //shotattack
	}

	memset (&newent, 0, sizeof(entity_t));
	newent.model = cl.model_precache[cl_modelindices[mi_flag]];
	newent.skinnum = team;
	newent.colormap = vid.colormap;

	AngleVectors (ent->angles, v_forward, v_right, NULL);
	v_forward[2] = -v_forward[2]; // reverse z component
	for (i = 0; i < 3; i++)
		newent.origin[i] = ent->origin[i] - f * v_forward[i] + 22 * v_right[i];
	newent.origin[2] -= 16;

	VectorCopy (ent->angles, newent.angles);
	newent.angles[2] -= 45;

	CL_AddEntity (&newent);
}

//Create visible entities in the correct position for all current players
void CL_LinkPlayers (void) {
	int j, msec, i, flicker, oldphysent;
	float *org;
	vec3_t	tmp;
	double playertime;
	player_info_t *info;
	player_state_t *state, exact;
	entity_t ent;
	centity_t *cent;
	frame_t *frame;
	dlighttype_t dimlightcolor;

	playertime = cls.realtime - cls.latency + 0.02;
	if (playertime > cls.realtime)
		playertime = cls.realtime;

	frame = &cl.frames[cl.parsecount & UPDATE_MASK];
	memset (&ent, 0, sizeof(entity_t));

	for (j = 0; j < MAX_CLIENTS; j++, info++, state++) {
		info = &cl.players[j];
		state = &frame->playerstate[j];
		cent = &cl_entities[j + 1];

		if (state->messagenum != cl.parsecount)
			continue;	// not present this frame

		// spawn light flashes, even ones coming from invisible objects
		if (r_powerupglow.value && !(r_powerupglow.value == 2 && j == cl.viewplayernum)) {
			org = (j == cl.playernum) ? cl.simorg : state->origin;
			flicker = r_lightflicker.value ? (rand() & 31) : 0;

			if ((state->effects & (EF_BLUE | EF_RED)) == (EF_BLUE | EF_RED)) {
				CL_NewDlight (j + 1, org, 200 + flicker, 0.1, lt_redblue, 0);
			} else if (state->effects & EF_BLUE) {
				CL_NewDlight (j + 1, org, 200 + flicker, 0.1, lt_blue, 0);
			} else if (state->effects & EF_RED) {
				CL_NewDlight (j + 1, org, 200 + flicker, 0.1, lt_red, 0);
			} else if (state->effects & EF_BRIGHTLIGHT) {
				VectorCopy (org, tmp);
				tmp[2] += 16;
				CL_NewDlight (j + 1, tmp, 400 + flicker, 0.1, lt_default, 0);
			} else if (state->effects & EF_DIMLIGHT) {
				if (cl.teamfortress)	
					dimlightcolor = dlightColor(r_flagcolor.value, lt_default, false);
				else if (state->effects & (EF_FLAG1|EF_FLAG2))
					dimlightcolor = dlightColor(r_flagcolor.value, lt_default, false);
				else
					dimlightcolor = lt_default;
				CL_NewDlight (j + 1, org, 200 + flicker, 0.1, dimlightcolor, 0);
			}
		}

		// the player object never gets added
		if (j == cl.playernum || !Cam_DrawPlayer(j))
			continue;

		if (!state->modelindex)
			continue;

		if (state->modelindex == cl_modelindices[mi_player]) {
			i = state->frame;

			if (cl_deadbodyfilter.value == 2) {
				if (ISDEAD(i))
					continue;
			} else if (cl_deadbodyfilter.value) {
				if (i == 49 || i == 60 || i == 69 || i == 84 || i == 93 || i == 102)
					continue;
			}
		}

		if (!(ent.model = cl.model_precache[state->modelindex]))
			Host_Error ("CL_LinkPlayers: bad modelindex");
		ent.skinnum = state->skinnum;
		ent.colormap = info->translations;
		ent.scoreboard = (state->modelindex == cl_modelindices[mi_player]) ? info : NULL;
		ent.frame = state->frame;
		if (cent->frametime >= 0 && cent->frametime <= cl.time) {
			ent.oldframe = cent->oldframe;
			ent.framelerp = (cl.time - cent->frametime) * 10;
		} else {
			ent.oldframe = ent.frame;
			ent.framelerp = -1;
		}

		// angles
		ent.angles[PITCH] = -state->viewangles[PITCH] / 3;
		ent.angles[YAW] = state->viewangles[YAW];
		ent.angles[ROLL] = 0;
		ent.angles[ROLL] = 4 * V_CalcRoll (ent.angles, state->velocity);

		// only predict half the move to minimize overruns
		msec = 500 * (playertime - state->state_time);
		if (msec <= 0 || !cl_predictPlayers.value || cls.mvdplayback) {		
			VectorCopy (state->origin, ent.origin);
		} else {
			// predict players movement
			if (msec > 255)
				msec = 255;
			state->command.msec = msec;

			oldphysent = pmove.numphysent;
			CL_SetSolidPlayers (j);
			CL_PredictUsercmd (state, &exact, &state->command);
			pmove.numphysent = oldphysent;
			VectorCopy (exact.origin, ent.origin);
		}

		if (state->effects & (EF_FLAG1|EF_FLAG2))
			CL_AddFlagModels (&ent, !!(state->effects & EF_FLAG2));

		VectorCopy (ent.origin, cent->lerp_origin);
		CL_AddEntity (&ent);
	}
}

//Builds all the pmove physents for the current frame
void CL_SetSolidEntities (void) {
	int i;
	frame_t	*frame;
	packet_entities_t *pak;
	entity_state_t *state;

	pmove.physents[0].model = cl.worldmodel;
	VectorClear (pmove.physents[0].origin);
	pmove.physents[0].info = 0;
	pmove.numphysent = 1;

	frame = &cl.frames[parsecountmod];
	pak = &frame->packet_entities;

	for (i = 0; i < pak->num_entities; i++) {
		state = &pak->entities[i];

	
		if (pmove.numphysent == MAX_PHYSENTS)
			break;
	

		if (!state->modelindex)
			continue;
		if (!cl.model_precache[state->modelindex])
			continue;
		if (cl.model_precache[state->modelindex]->hulls[1].firstclipnode) {
			pmove.physents[pmove.numphysent].model = cl.model_precache[state->modelindex];
			VectorCopy (state->origin, pmove.physents[pmove.numphysent].origin);
			pmove.numphysent++;
		}
	}
}

/*
Calculate the new position of players, without other player clipping

We do this to set up real player prediction.
Players are predicted twice, first without clipping other players,
then with clipping against them.
This sets up the first phase.
*/
void CL_SetUpPlayerPrediction(qboolean dopred) {
	int j, msec;
	player_state_t *state, exact;
	double playertime;
	frame_t *frame;
	struct predicted_player *pplayer;

	playertime = cls.realtime - cls.latency + 0.02;
	if (playertime > cls.realtime)
		playertime = cls.realtime;

	frame = &cl.frames[cl.parsecount & UPDATE_MASK];

	for (j = 0; j < MAX_CLIENTS; j++) {
		pplayer = &predicted_players[j];
		state = &frame->playerstate[j];

		pplayer->active = false;

		if (state->messagenum != cl.parsecount)
			continue;	// not present this frame

		if (!state->modelindex)
			continue;

		pplayer->active = true;
		pplayer->flags = state->flags;

		// note that the local player is special, since he moves locally we use his last predicted postition
		if (j == cl.playernum) {
			VectorCopy(cl.frames[cls.netchan.outgoing_sequence & UPDATE_MASK].playerstate[cl.playernum].origin, pplayer->origin);
		} else {
			// only predict half the move to minimize overruns
			msec = 500 * (playertime - state->state_time);
			if (msec <= 0 || !cl_predictPlayers.value || !dopred || cls.mvdplayback) { 
				VectorCopy (state->origin, pplayer->origin);
			} else {
				// predict players movement
				if (msec > 255)
					msec = 255;
				state->command.msec = msec;

				CL_PredictUsercmd (state, &exact, &state->command);
				VectorCopy (exact.origin, pplayer->origin);
			}
		}
	}
}

//Builds all the pmove physents for the current frame.
//Note that CL_SetUpPlayerPrediction() must be called first!
//pmove must be setup with world and solid entity hulls before calling (via CL_PredictMove)
void CL_SetSolidPlayers (int playernum) {
	int j;
	struct predicted_player *pplayer;
	physent_t *pent;
	extern vec3_t player_mins, player_maxs;

	if (!cl_solidPlayers.value)
		return;

	pent = pmove.physents + pmove.numphysent;

	for (j = 0; j < MAX_CLIENTS; j++) {
		pplayer = &predicted_players[j];

	
		if (pmove.numphysent == MAX_PHYSENTS)
			break;
	

		if (!pplayer->active)
			continue;	// not present this frame

		// the player object never gets added
		if (j == playernum)
			continue;

		if (pplayer->flags & PF_DEAD)
			continue; // dead players aren't solid

		pent->model = 0;
		VectorCopy(pplayer->origin, pent->origin);
		VectorCopy(player_mins, pent->mins);
		VectorCopy(player_maxs, pent->maxs);
		pmove.numphysent++;
		pent++;
	}
}

//Builds the visedicts array for cl.time
//Made up of: clients, packet_entities, nails, and tents
void CL_EmitEntities (void) {
	if (cls.state != ca_active)
		return;

	if (!cl.validsequence)
		return;

	CL_ClearScene ();
	CL_LinkPlayers ();
	CL_LinkPacketEntities ();
	CL_LinkProjectiles ();
	CL_UpdateTEnts ();
}

int	mvd_fixangle;

static int MVD_TranslateFlags(int src) {
	int dst = 0;

	if (src & DF_EFFECTS)
		dst |= PF_EFFECTS;
	if (src & DF_SKINNUM)
		dst |= PF_SKINNUM;
	if (src & DF_DEAD)
		dst |= PF_DEAD;
	if (src & DF_GIB)
		dst |= PF_GIB;
	if (src & DF_WEAPONFRAME)
		dst |= PF_WEAPONFRAME;
	if (src & DF_MODEL)
		dst |= PF_MODEL;

	return dst;
}

static float MVD_AdjustAngle(float current, float ideal, float fraction) {
	float move;

	move = ideal - current;
	if (move >= 180)
		move -= 360;
	else if (move <= -180)
		move += 360;

	return current + fraction * move;
}

static void MVD_InitInterpolation(void) {
	player_state_t *state, *oldstate;
	int i, tracknum;
	frame_t	*frame, *oldframe;
	vec3_t dist;
	struct predicted_player *pplayer;

	if (!cl.validsequence)
		 return;

	if (nextdemotime <= olddemotime)
		return;

	frame = &cl.frames[cl.parsecount & UPDATE_MASK];
	oldframe = &cl.frames[cl.oldparsecount & UPDATE_MASK];

	// clients
	for (i = 0; i < MAX_CLIENTS; i++) {
		pplayer = &predicted_players[i];
		state = &frame->playerstate[i];
		oldstate = &oldframe->playerstate[i];

		if (pplayer->predict) {
			VectorCopy(pplayer->oldo, oldstate->origin);
			VectorCopy(pplayer->olda, oldstate->command.angles);
			VectorCopy(pplayer->oldv, oldstate->velocity);
		}

		pplayer->predict = false;

		tracknum = Cam_TrackNum();
		if ((mvd_fixangle & 1) << i) {
			if (i == tracknum) {
				VectorCopy(cl.viewangles, state->command.angles);
				VectorCopy(cl.viewangles, state->viewangles);
			}

			// no angle interpolation
			VectorCopy(state->command.angles, oldstate->command.angles);

			mvd_fixangle &= ~(1 << i);
		}

		// we dont interpolate ourself if we are spectating
		if (i == cl.playernum && cl.spectator)
			continue;

		memset(state->velocity, 0, sizeof(state->velocity));

		if (state->messagenum != cl.parsecount)
			continue;	// not present this frame

		if (oldstate->messagenum != cl.oldparsecount || !oldstate->messagenum)
			continue;	// not present last frame

		if (!ISDEAD(state->frame) && ISDEAD(oldstate->frame))
			continue;

		VectorSubtract(state->origin, oldstate->origin, dist);
		if (DotProduct(dist, dist) > 22500)
			continue;

		VectorScale(dist, 1 / (nextdemotime - olddemotime), pplayer->oldv);

		VectorCopy(state->origin, pplayer->oldo);
		VectorCopy(state->command.angles, pplayer->olda);

		pplayer->oldstate = oldstate;
		pplayer->predict = true;
	}

	// nails
	for (i = 0; i < cl_num_projectiles; i++) {
		if (!cl.int_projectiles[i].interpolate)
			continue;

		VectorCopy(cl.int_projectiles[i].origin, cl_projectiles[i].origin);
	}
}

void MVD_Interpolate(void) {
	int i, j;
	float f;
	frame_t	*frame, *oldframe;
	player_state_t *state, *oldstate, *self, *oldself;
	entity_state_t *oldents;
	struct predicted_player *pplayer;
	static float old;

	self = &cl.frames[cl.parsecount & UPDATE_MASK].playerstate[cl.playernum];
	oldself = &cl.frames[(cls.netchan.outgoing_sequence - 1) & UPDATE_MASK].playerstate[cl.playernum];

	self->messagenum = cl.parsecount;

	VectorCopy(oldself->origin, self->origin);
	VectorCopy(oldself->velocity, self->velocity);
	VectorCopy(oldself->viewangles, self->viewangles);

	if (old != nextdemotime) {
		old = nextdemotime;
		MVD_InitInterpolation();
	}

	CL_ParseClientdata();

	cls.netchan.outgoing_sequence = cl.parsecount + 1;

	if (!cl.validsequence)
		return;

	if (nextdemotime <= olddemotime)
		return;

	frame = &cl.frames[cl.parsecount & UPDATE_MASK];
	oldframe = &cl.frames[cl.oldparsecount & UPDATE_MASK];
	oldents = oldframe->packet_entities.entities;

	f = bound(0, (cls.demotime - olddemotime) / (nextdemotime - olddemotime), 1);

	// interpolate nails
	for (i = 0; i < cl_num_projectiles; i++)	{
		if (!cl.int_projectiles[i].interpolate)
			continue;

		for (j = 0; j < 3; j++) {
			cl_projectiles[i].origin[j] = cl_oldprojectiles[cl.int_projectiles[i].oldindex].origin[j] +
				f * (cl.int_projectiles[i].origin[j] - cl_oldprojectiles[cl.int_projectiles[i].oldindex].origin[j]);
		}
	}

	// interpolate clients
	for (i = 0; i < MAX_CLIENTS; i++) {
		pplayer = &predicted_players[i];
		state = &frame->playerstate[i];
		oldstate = &oldframe->playerstate[i];

		if (pplayer->predict) {
			for (j = 0; j < 3; j++) {
				state->viewangles[j] = MVD_AdjustAngle(oldstate->command.angles[j], pplayer->olda[j], f);
				state->origin[j] = oldstate->origin[j] + f * (pplayer->oldo[j] - oldstate->origin[j]);
				state->velocity[j] = oldstate->velocity[j] + f * (pplayer->oldv[j] - oldstate->velocity[j]);
			}
		}
	}
}

void CL_ClearPredict(void) {
	memset(predicted_players, 0, sizeof(predicted_players));
	mvd_fixangle = 0;
}
