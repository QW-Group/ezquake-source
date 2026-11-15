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
#include "gl_model.h"
#include "vx_stuff.h"
#include "qmb_particles.h"

static int fix_trail_num_for_grens(int trail_num)
{
	//trails for rockets and grens are the same
	//only nums 1 and 2 must be swapped
	switch (trail_num) {
		case 1: trail_num = 2; break;
		case 2: trail_num = 1; break;
	}

	return trail_num;
}

static void R_MissileTrail(centity_t *cent, int trail_num)
{
	if ((trail_num == 8 || trail_num == 10 || trail_num == 11) && !qmb_initialized) {
		trail_num = 1;
	}

	if (trail_num == 0) {
		VectorCopy(cent->current.origin, cent->trails[0].stop);
	}
	else if (trail_num == 1) {
		R_EntityParticleTrail(cent, ROCKET_TRAIL);
	}
	else if (trail_num == 2) {
		R_EntityParticleTrail(cent, GRENADE_TRAIL);
	}
	else if (trail_num == 3) {
		R_EntityParticleTrail(cent, ALT_ROCKET_TRAIL);
	}
	else if (trail_num == 4) {
		R_EntityParticleTrail(cent, BLOOD_TRAIL);
	}
	else if (trail_num == 5) {
		R_EntityParticleTrail(cent, BIG_BLOOD_TRAIL);
	}
	else if (trail_num == 6) {
		R_EntityParticleTrail(cent, TRACER1_TRAIL);
	}
	else if (trail_num == 7) {
		R_EntityParticleTrail(cent, TRACER2_TRAIL);
	}
	else if (trail_num == 8) {
		// VULT PARTICLES
		byte color[3];
		color[0] = 0; color[1] = 70; color[2] = 255;
		FireballTrail(cent, color, 2, 0.5);
	}
	else if (trail_num == 9) {
		R_EntityParticleTrail(cent, LAVA_TRAIL);
	}
	else if (trail_num == 10) {
		// VULT PARTICLES
		FuelRodGunTrail(cent);
	}
	else if (trail_num == 11) {
		byte color[3];
		color[0] = 255; color[1] = 70; color[2] = 5;
		FireballTrailWave(cent, color, 2, 0.5, cent->current.angles);
	}
	else if (trail_num == 12) {
		R_EntityParticleTrail(cent, AMF_ROCKET_TRAIL);
	}
	else if (trail_num == 14) {
		R_EntityParticleTrail(cent, RAIL_TRAIL);
	}
	else {
		R_EntityParticleTrail(cent, GRENADE_TRAIL);
	}
}

// The model's r_trail effect, else the effect for its trail flag, or P_INVALID.
// Trails that r_rockettrail, r_grenadetrail and r_railtrail 0 turn off stay
// off; lava balls take the rocket trail whatever r_rockettrail is, as their
// classic trail does. Rails and building gibs get only an r_trail effect.
static int CL_ScriptedTrailType(const model_t *model)
{
	int flagtrail = P_INVALID;

	if (model->modhint == MOD_LAVABALL) {
		flagtrail = PScript_EffectType(PSCRIPT_TR_ROCKET);
	}
	else if (model->flags & EF_ROCKET) {
		if (!r_rockettrail.integer) {
			return P_INVALID;
		}
		flagtrail = PScript_EffectType(PSCRIPT_TR_ROCKET);
	}
	else if (model->flags & EF_GRENADE) {
		if (model->modhint == MOD_RAIL) {
			if (!r_railtrail.integer) {
				return P_INVALID;
			}
		}
		else if (model->modhint != MOD_BUILDINGGIBS) {
			if (!r_grenadetrail.integer) {
				return P_INVALID;
			}
			flagtrail = PScript_EffectType(PSCRIPT_TR_GRENADE);
		}
	}
	else if (model->flags & EF_GIB) {
		flagtrail = PScript_EffectType(PSCRIPT_TR_BLOOD);
	}
	else if (model->flags & EF_ZOMGIB) {
		if (model->modhint != MOD_RAIL2) {
			flagtrail = PScript_EffectType(PSCRIPT_TR_SLIGHTBLOOD);
		}
	}
	else if (model->flags & EF_TRACER) {
		flagtrail = PScript_EffectType(PSCRIPT_TR_WIZSPIKE);
	}
	else if (model->flags & EF_TRACER2) {
		flagtrail = PScript_EffectType(PSCRIPT_TR_KNIGHTSPIKE);
	}
	else if (model->flags & EF_TRACER3) {
		flagtrail = PScript_EffectType(PSCRIPT_TR_VORESPIKE);
	}

	return model->traileffect != P_INVALID ? model->traileffect : flagtrail;
}

// The time a trail run covers since the owner's previous run in host frame
// last, at most 0.1 s so that a hitch does not emit a burst. Predicted
// projectiles run every 13 ms, so at high frame rates one run of theirs
// covers several frames.
static float CL_TrailInterval(int last)
{
	int frames = pscript_hostframe - last;

	if (!last || frames <= 1) {
		frames = 1;
	}
	return min(frames * pscript_frametime, 0.1);
}

// Runs the entity's scripted trail up to end once per host frame. Returns
// false when it has none, so the caller draws its own. Only moving entities
// leave trails, so rulesets that restrict particles turn them all off.
static qbool CL_ScriptedEntityTrail(centity_t *cent, const model_t *model, vec3_t end, int dlkey)
{
	// Stamped on every call, so that after the system or the trail type was
	// off a run still covers only the time since the previous call.
	int last = cent->pscript_trailframe;
	qbool due = PScript_EmitterDue(&cent->pscript_trailframe);
	int i, type;

	if (!PScript_DynamicActive()) {
		return false;
	}
	type = CL_ScriptedTrailType(model);
	if (type == P_INVALID) {
		return false;
	}

	// No trail spans a jump, which also follows a demo seek or a slot reuse
	// that left the old stop. The state starts over too, so a beam trail
	// does not span it either. Checked on every call, as frames that emit
	// nothing still move the stop.
	for (i = 0; i < 3; i++) {
		if (fabs(cent->trails[0].stop[i] - end[i]) > 128) {
			PScript_DelinkTrailstate(&cent->pscript_trail);
			VectorCopy(end, cent->trails[0].stop);
			break;
		}
	}
	if (due) {
		PScript_MatchTrailstate(&cent->pscript_trail, &cent->pscript_trailtype, type);
		PScript_ParticleTrail(cent->trails[0].stop, end, type, CL_TrailInterval(last), dlkey, &cent->pscript_trail);
	}
	VectorCopy(end, cent->trails[0].stop);
	return true;
}

// Moved out of CL_LinkPacketEntities() so NQD playback can use same code.
// A scripted trail replaces the classic and QMB trails; lights stay. It ends
// at ent's origin rather than cent->lerp_origin, which NetQuake demos set
// only after the call.
void CL_AddParticleTrail(entity_t* ent, centity_t* cent, customlight_t* cst_lt, entity_state_t *state)
{
	extern cvar_t gl_no24bit;
	model_t* model = cl.model_precache[state->modelindex];
	float rocketlightsize = 0.0f;
	qbool scripted = CL_ScriptedEntityTrail(cent, model, ent->origin, state->number);

	if (model->modhint == MOD_LAVABALL) {
		if (!scripted) {
			R_EntityParticleTrail(cent, LAVA_TRAIL);
		}
	}
	else {
		if (model->flags & EF_ROCKET) {
			if (!scripted) {
				R_MissileTrail(cent, r_rockettrail.integer);
			}

			// Add rocket lights
			rocketlightsize = 200.0 * bound(0, r_rocketlight.value, 1);
			if (rocketlightsize >= 1) {
				extern cvar_t gl_rl_globe;
				int bubble = gl_rl_globe.integer ? 2 : 1;

				if ((r_rockettrail.value < 8 || r_rockettrail.value == 12) && model->modhint != MOD_LAVABALL) {
					dlightColorEx(r_rocketlightcolor.value, r_rocketlightcolor.string, lt_rocket, false, cst_lt);
					CL_NewDlightEx(state->number, ent->origin, rocketlightsize, 0.1, cst_lt, bubble);
					if (!ISPAUSED && amf_coronas.integer) {
						//VULT CORONAS
						R_CoronasEntityNew(C_ROCKETLIGHT, cent);
					}
				}
				else if (r_rockettrail.value == 9 || r_rockettrail.value == 11) {
					CL_NewDlight(state->number, ent->origin, rocketlightsize, 0.1, lt_default, bubble);
				}
				else if (r_rockettrail.value == 8) {
					// PLASMA ROCKETS
					CL_NewDlight(state->number, ent->origin, rocketlightsize, 0.1, lt_blue, bubble);
				}
				else if (r_rockettrail.value == 10) {
					// FUEL ROD GUN
					CL_NewDlight(state->number, ent->origin, rocketlightsize, 0.1, lt_green, bubble);
				}
			}
		}
		else if (model->flags & EF_GRENADE) {
			if (!scripted) {
				if (model->modhint == MOD_BUILDINGGIBS) {
					R_EntityParticleTrail(cent, GRENADE_TRAIL);
				}
				else if (r_grenadetrail.integer && model->modhint != MOD_RAIL) {
					R_MissileTrail(cent, fix_trail_num_for_grens(r_grenadetrail.integer));
				}
				else if (r_railtrail.integer && model->modhint == MOD_RAIL) {
					R_MissileTrail(cent, fix_trail_num_for_grens(r_railtrail.integer));
				}
				else {
					R_MissileTrail(cent, 0);
				}
			}
		}
		else if (model->flags & EF_GIB) {
			if (!scripted) {
				if (amf_part_gibtrails.integer) {
					R_EntityParticleTrail(cent, BLEEDING_TRAIL);
				}
				else {
					R_EntityParticleTrail(cent, BLOOD_TRAIL);
				}
			}
		}
		else if (model->flags & EF_ZOMGIB) {
			if (!scripted) {
				if (model->modhint == MOD_RAIL2) {
					R_EntityParticleTrail(cent, RAIL_TRAIL2);
				}
				else if (amf_part_gibtrails.value) {
					R_EntityParticleTrail(cent, BLEEDING_TRAIL);
				}
				else {
					R_EntityParticleTrail(cent, BIG_BLOOD_TRAIL);
				}
			}
		}
		else if (model->flags & EF_TRACER) {
			// VULT TRACER GLOW
			rocketlightsize = 35 * (1 + bound(0, r_rocketlight.value, 1));
			CL_NewDlight(state->number, ent->origin, rocketlightsize, 0.01, lt_green, true);
			if (!ISPAUSED && amf_coronas.integer) {
				R_CoronasEntityNew(C_WIZLIGHT, cent);
			}

			if (!scripted) {
				R_EntityParticleTrail(cent, TRACER1_TRAIL);
			}
		}
		else if (model->flags & EF_TRACER2) {
			// VULT TRACER GLOW
			rocketlightsize = 35 * (1 + bound(0, r_rocketlight.value, 1));
			CL_NewDlight(state->number, ent->origin, rocketlightsize, 0.01, lt_default, true);
			if (!ISPAUSED && amf_coronas.integer) {
				R_CoronasEntityNew(C_KNIGHTLIGHT, cent);
			}

			if (!scripted) {
				R_EntityParticleTrail(cent, TRACER2_TRAIL);
			}
		}
		else if (model->flags & EF_TRACER3) {
			// VULT TRACER GLOW
			rocketlightsize = 35 * (1 + bound(0, r_rocketlight.value, 1));
			CL_NewDlight(state->number, ent->origin, rocketlightsize, 0.01, lt_blue, true);
			if (!ISPAUSED && amf_coronas.integer) {
				R_CoronasEntityNew(C_VORELIGHT, cent);
			}

			if (!scripted) {
				R_EntityParticleTrail(cent, VOOR_TRAIL);
			}
		}
		else if (model->modhint == MOD_SPIKE && amf_nailtrail.integer && !gl_no24bit.integer) {
			// VULT NAILTRAIL
			if (!scripted) {
				if (amf_nailtrail_plasma.integer) {
					byte color[3];
					color[0] = 0; color[1] = 70; color[2] = 255;
					FireballTrail(cent, color, 0.6, 0.3);
				}
				else {
					ParticleNailTrail(cent, 2, 0.4f);
				}
			}
		}
		else if (model->modhint_trail == MOD_TF_TRAIL && amf_extratrails.integer) {
			// VULT TRAILS
			if (!scripted) {
				ParticleAlphaTrail(cent, 4, 0.4);
			}
		}
		else if (model->modhint == MOD_LASER && amf_extratrails.integer) {
			rocketlightsize = 35 * (1 + bound(0, r_rocketlight.value, 1));
			CL_NewDlight(state->number, ent->origin, rocketlightsize, 0.01, lt_default, true);
			if (!ISPAUSED && amf_coronas.integer) {
				R_CoronasEntityNew(C_KNIGHTLIGHT, cent);
			}
			if (!scripted) {
				if (cent->trails[1].lasttime + 0.01 < particle_time) {
					VX_LightningTrail(cent->trails[1].stop, ent->origin);
					VectorCopy(ent->origin, cent->trails[1].stop);
					cent->trails[1].lasttime = particle_time;
				}
				R_EntityParticleTrail(cent, TRACER2_TRAIL);
			}
		}
		else if (model->modhint == MOD_DETPACK) {
			// VULT CORONAS
			if (qmb_initialized && amf_detpacklights.integer) {
				vec3_t liteorg, forward, right, up;
				VectorCopy(ent->origin, liteorg);
				AngleVectors(ent->angles, forward, right, up);
				VectorMA(liteorg, -15, up, liteorg);
				VectorMA(liteorg, -10, forward, liteorg);

				// MEAG: Fixme, was *old_origin, liteorg, cent->trail_origin - need to add check for detpack in ParticleAlphaTrail (?)
				if (!scripted) {
					ParticleAlphaTrail(cent, 10, 0.8);
				}
				if (!ISPAUSED && amf_coronas.integer) {
					if (ent->skinnum > 0) {
						R_CoronasNew(C_REDLIGHT, liteorg);
					}
					else {
						R_CoronasNew(C_GREENLIGHT, liteorg);
					}
				}
			}
		}

		// VULT BUILDING SPARKS
		if (!ISPAUSED && amf_buildingsparks.value && (model->modhint == MOD_BUILDINGGIBS && (rand() % 40 < 2))) {
			vec3_t liteorg, forward, right, up;
			byte col[3] = { 60, 100, 240 };
			VectorCopy(ent->origin, liteorg);
			AngleVectors(ent->angles, forward, right, up);
			VectorMA(liteorg, rand() % 10 - 5, up, liteorg);
			VectorMA(liteorg, rand() % 10 - 5, right, liteorg);
			VectorMA(liteorg, rand() % 10 - 5, forward, liteorg);
			SparkGen(liteorg, col, (int)(6 * amf_buildingsparks.value), 100, 1);
			if (amf_coronas.integer) {
				R_CoronasNew(C_BLUESPARK, liteorg);
			}
		}

		// VULT TESLA CHARGE - Tesla or shambler in charging animation
		if (((model->modhint == MOD_TESLA && ent->frame >= 7 && ent->frame <= 12) ||
			(model->modhint == MOD_SHAMBLER && ent->frame >= 65 && ent->frame <= 68))
			&& !ISPAUSED && amf_cutf_tesla_effect.value) {
			vec3_t liteorg;
			VectorCopy(ent->origin, liteorg);
			liteorg[2] += 32;
			VX_TeslaCharge(liteorg);
		}
	}
}

// Under rulesets that restrict particles, emitters run on static entities,
// the only ones with an entity_id, and on the map's inline brush models;
// those of other entities need PScript_DynamicActive.
qbool CL_ScriptedEmitter(const entity_t *ent)
{
	if (ent->model->emiteffect == P_INVALID) {
		return false;
	}
	return (ent->entity_id || PScript_InlineModel(ent->model)) ? PScript_Active() : PScript_DynamicActive();
}

// Runs the r_effect of ent's model once per host frame, down or, for a
// "forwards" effect, along the model's forward vector. Returns true while it
// runs. An effect that replaces the model hides it in CL_AddEntity.
qbool CL_RunModelEmitter(entity_t *ent, trailkey_t *key, int *frame, int *type)
{
	model_t *model = ent->model;
	vec3_t angles, forward, right, up;

	if (!CL_ScriptedEmitter(ent)) {
		return false;
	}

	if (PScript_EmitterDue(frame)) {
		// Alias models pitch the other way.
		VectorCopy(ent->angles, angles);
		if (model->type == mod_alias || model->type == mod_alias3) {
			angles[PITCH] = -angles[PITCH];
		}
		AngleVectors(angles, forward, right, up);
		if (!(model->emitflags & PSCRIPT_EMIT_FORWARDS)) {
			VectorNegate(up, forward);
		}
		PScript_MatchTrailstate(key, type, model->emiteffect);
		// Capped so that a hitch does not emit a burst.
		PScript_RunWorldEffect(ent->origin, forward, min(pscript_frametime, 0.1), model->emiteffect, key);
	}
	return true;
}
