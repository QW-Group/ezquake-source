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

// Moved out of CL_LinkPacketEntities() so NQD playback can use same code.
void CL_AddParticleTrail(entity_t* ent, centity_t* cent, customlight_t* cst_lt, entity_state_t *state)
{
	extern cvar_t gl_no24bit;
	model_t* model = cl.model_precache[state->modelindex];
	float rocketlightsize = 0.0f;

	if (model->modhint == MOD_LAVABALL) {
		R_EntityParticleTrail(cent, LAVA_TRAIL);
	}
	else {
		if (model->flags & EF_ROCKET) {
			R_MissileTrail(cent, r_rockettrail.integer);

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
		else if (model->flags & EF_GIB) {
			if (amf_part_gibtrails.integer) {
				R_EntityParticleTrail(cent, BLEEDING_TRAIL);
			}
			else {
				R_EntityParticleTrail(cent, BLOOD_TRAIL);
			}
		}
		else if (model->flags & EF_ZOMGIB) {
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
		else if (model->flags & EF_TRACER) {
			// VULT TRACER GLOW
			rocketlightsize = 35 * (1 + bound(0, r_rocketlight.value, 1));
			CL_NewDlight(state->number, ent->origin, rocketlightsize, 0.01, lt_green, true);
			if (!ISPAUSED && amf_coronas.integer) {
				R_CoronasEntityNew(C_WIZLIGHT, cent);
			}

			R_EntityParticleTrail(cent, TRACER1_TRAIL);
		}
		else if (model->flags & EF_TRACER2) {
			// VULT TRACER GLOW
			rocketlightsize = 35 * (1 + bound(0, r_rocketlight.value, 1));
			CL_NewDlight(state->number, ent->origin, rocketlightsize, 0.01, lt_default, true);
			if (!ISPAUSED && amf_coronas.integer) {
				R_CoronasEntityNew(C_KNIGHTLIGHT, cent);
			}

			R_EntityParticleTrail(cent, TRACER2_TRAIL);
		}
		else if (model->flags & EF_TRACER3) {
			// VULT TRACER GLOW
			rocketlightsize = 35 * (1 + bound(0, r_rocketlight.value, 1));
			CL_NewDlight(state->number, ent->origin, rocketlightsize, 0.01, lt_blue, true);
			if (!ISPAUSED && amf_coronas.integer) {
				R_CoronasEntityNew(C_VORELIGHT, cent);
			}

			R_EntityParticleTrail(cent, VOOR_TRAIL);
		}
		else if (model->modhint == MOD_SPIKE && amf_nailtrail.integer && !gl_no24bit.integer) {
			// VULT NAILTRAIL
			if (amf_nailtrail_plasma.integer) {
				byte color[3];
				color[0] = 0; color[1] = 70; color[2] = 255;
				FireballTrail(cent, color, 0.6, 0.3);
			}
			else {
				ParticleNailTrail(cent, 2, 0.4f);
			}
		}
		else if (model->modhint_trail == MOD_TF_TRAIL && amf_extratrails.integer) {
			// VULT TRAILS
			ParticleAlphaTrail(cent, 4, 0.4);
		}
		else if (model->modhint == MOD_LASER && amf_extratrails.integer) {
			rocketlightsize = 35 * (1 + bound(0, r_rocketlight.value, 1));
			CL_NewDlight(state->number, ent->origin, rocketlightsize, 0.01, lt_default, true);
			if (!ISPAUSED && amf_coronas.integer) {
				R_CoronasEntityNew(C_KNIGHTLIGHT, cent);
			}
			if (cent->trails[1].lasttime + 0.01 < particle_time) {
				VX_LightningTrail(cent->trails[1].stop, ent->origin);
				VectorCopy(ent->origin, cent->trails[1].stop);
				cent->trails[1].lasttime = particle_time;
			}
			R_EntityParticleTrail(cent, TRACER2_TRAIL);
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
				ParticleAlphaTrail(cent, 10, 0.8);
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
