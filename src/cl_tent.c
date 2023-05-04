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

	$Id: cl_tent.c,v 1.31 2007-10-29 00:13:26 d3urk Exp $
*/
// cl_tent.c -- client side temporary entities

#include "quakedef.h"
#include "vx_stuff.h"
#include "rulesets.h"
#include "qsound.h"
#include "pmove.h"
#include "utils.h"
#include "qmb_particles.h"
#include "cl_tent.h"

extern cvar_t gl_no24bit;
#ifdef MVD_PEXT1_SIMPLEPROJECTILE
extern cvar_t cl_sproj_xerp;
#endif

temp_entity_list_t temp_entities;

beam_t cl_beams[MAX_BEAMS];

static vec3_t playerbeam_end;
static qbool playerbeam_update;

// cl_free_explosions = linear linked list of free explosions
explosion_t	cl_explosions[MAX_EXPLOSIONS];
explosion_t cl_explosions_headnode, *cl_free_explosions;

fproj_t cl_fakeprojectiles[MAX_FAKEPROJ];
predexplosion_t cl_predictedexplosions[MAX_PREDEXPLOSIONS];

static model_t	*cl_explo_mod, *cl_bolt1_mod, *cl_bolt2_mod, *cl_bolt3_mod, *cl_beam_mod;

sfx_t	*cl_sfx_wizhit, *cl_sfx_knighthit, *cl_sfx_tink1, *cl_sfx_ric1, *cl_sfx_ric2, *cl_sfx_ric3, *cl_sfx_r_exp3;

cvar_t r_lgblood = {"r_lgbloodColor", "225"};
cvar_t r_shiftbeam = {"r_shiftbeam", "0"};

void CL_InitTEnts(void)
{
	cl_sfx_wizhit = S_PrecacheSound ("wizard/hit.wav");
	cl_sfx_knighthit = S_PrecacheSound ("hknight/hit.wav");
	cl_sfx_tink1 = S_PrecacheSound ("weapons/tink1.wav");
	cl_sfx_ric1 = S_PrecacheSound ("weapons/ric1.wav");
	cl_sfx_ric2 = S_PrecacheSound ("weapons/ric2.wav");
	cl_sfx_ric3 = S_PrecacheSound ("weapons/ric3.wav");
	cl_sfx_r_exp3 = S_PrecacheSound ("weapons/r_exp3.wav");
}

void CL_InitTEntsCvar(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_EYECANDY);
	Cvar_Register(&r_lgblood);
	Cvar_Register(&r_shiftbeam);
	Cvar_ResetCurrentGroup();
}

void CL_ClearTEnts(void) 
{
	int i;

	cl_explo_mod = cl_bolt1_mod = cl_bolt2_mod = cl_bolt3_mod = cl_beam_mod = NULL;

	memset (&cl_beams, 0, sizeof(cl_beams));
	memset (&cl_explosions, 0, sizeof(cl_explosions));
	memset (&cl_fakeprojectiles, 0, sizeof(cl_fakeprojectiles));

	// link explosions 
	cl_free_explosions = cl_explosions; 
	cl_explosions_headnode.next = cl_explosions_headnode.prev = &cl_explosions_headnode; 

	for (i = 0; i < MAX_EXPLOSIONS - 1; i++) 
		cl_explosions[i].next = &cl_explosions[i + 1]; 
}

explosion_t *CL_AllocExplosion(void) 
{
	explosion_t *ex;

	if (cl_free_explosions) 
	{	
		// Take a free explosion if possible.
		ex = cl_free_explosions;
		cl_free_explosions = ex->next;
	} 
	else
	{
		// Grab the oldest one otherwise.
		ex = cl_explosions_headnode.prev;
		ex->prev->next = ex->next;
		ex->next->prev = ex->prev;
	}

	// Put the explosion at the start of the list.
	ex->prev = &cl_explosions_headnode;
	ex->next = cl_explosions_headnode.next;
	ex->next->prev = ex;
	ex->prev->next = ex;

	return ex;
}

void CL_FreeExplosion(explosion_t *ex)
{
	// Remove from linked active list.
	ex->prev->next = ex->next;
	ex->next->prev = ex->prev;

	// Insert into front linked free list.
	ex->next = cl_free_explosions;
	cl_free_explosions = ex;
}

void CL_MatchFakeProjectile(centity_t *cent)
{
	fproj_t *prj;
	int i;

	for (i = 0, prj = cl_fakeprojectiles; i < MAX_FAKEPROJ; i++, prj++)
	{
		// only do this for fake projectiles that ended recently (or will end soon)
		if (prj->endtime >= cl.time && prj->endtime <= cl.time + 0.08)
		{
			vec3_t diff;
			VectorSubtract(cent->lerp_origin, prj->org, diff);

			if (VectorLength(diff) < 24)
			{
				if (prj->partcount)
				{
					VectorCopy(prj->partorg, cent->trails[0].stop);
					VectorCopy(prj->partorg, cent->trails[1].stop);
					VectorCopy(prj->partorg, cent->trails[2].stop);
					VectorCopy(prj->partorg, cent->trails[3].stop);
				}

				if (prj->dl_key)
				{
					dlight_t *dl;
					dl = cl_dlights;
					for (i = 0; i < MAX_DLIGHTS; i++, dl++) {
						if (dl->key == prj->dl_key) {
							memset(dl, 0, sizeof(*dl));
							cl_dlight_active[i / 32] -= (cl_dlight_active[i / 32] & (1 << (i % 32)));
						}
					}
				}

				
				//cent->startlerp = cl.time;
				//cent->deltalerp = 0.1;
				//VectorCopy(prj->org, cent->old_origin);
				//VectorCopy(prj->angs, cent->old_angles);
				//VectorCopy(prj->org, cent->lerp_origin);

				memset(prj, 0, sizeof(fproj_t));
				return;
			}
		}
	}
}

fproj_t *CL_AllocFakeProjectile(void)
{
	fproj_t *prj;
	int		i;
	float	cull_distance;
	int		cull_index;


	for (i = 0, prj = cl_fakeprojectiles; i < MAX_FAKEPROJ; i++, prj++)
	{
		if (prj->endtime < cl.time)
		{
			memset(prj, 0, sizeof(fproj_t));
			prj->dl_key = MAX_EDICTS + i; // this is hacky and gross
			prj->index = i;
			#ifdef MVD_PEXT1_SIMPLEPROJECTILE
			prj->owner = (cl.viewplayernum + 1);
			#endif
			prj->entnum = 0;
			//prj->cent = &cl_entities[CL_MAX_EDICTS - (i + 1)];
			return prj;
		}
	}

	// oh crap, we're out of fakeproj slots! now we need to do some expensive operations to find the furthest projectile and use that
	cull_distance = 1;
	cull_index = 0;
	for (i = 0, prj = cl_fakeprojectiles; i < MAX_FAKEPROJ; i++, prj++)
	{
		float	temp_dist;
		vec3_t	temp_vec;

		VectorSubtract(prj->org, cl.simorg, temp_vec);
		temp_dist = VectorLength(temp_vec);
		if (temp_dist > cull_distance)
		{
			if (temp_dist > 2048) // it's so far away, just use this and hope for the best
				break;

			cull_distance = temp_dist;
			cull_index = i;
		}
	}


	prj = &cl_fakeprojectiles[cull_index];
	memset(prj, 0, sizeof(fproj_t));
	prj->entnum = 0;
	prj->index = cull_index;
	prj->owner = (cl.viewplayernum + 1);
	prj->dl_key = MAX_EDICTS + cull_index;
	//cl_fakeprojectiles[cull_index].cent = &cl_entities[CL_MAX_EDICTS - (cull_index + 1)];
	return prj;
}

#define WEAPONPRED_MAXLATENCY 0.1
fproj_t *CL_CreateFakeNail(void)
{
	if (pmove.client_ping == 0)
		return NULL;//&cl_fakeprojectiles[0];

	fproj_t *newmis = CL_AllocFakeProjectile();
	newmis->modelindex = cl_modelindices[mi_spike];

	float latency = cls.latency;
	newmis->starttime = cl.time + max((latency - 0.013) - ((float)pmove.client_ping / 1000), 0);
	newmis->endtime = cl.time + latency + 0.013;
	newmis->effects = 256;
	
	return newmis;
}

fproj_t *CL_CreateFakeSuperNail(void)
{
	if (pmove.client_ping == 0)
		return NULL;//&cl_fakeprojectiles[0];

	fproj_t *newmis = CL_AllocFakeProjectile();
	newmis->modelindex = cl_modelindices[mi_s_spike];

	float latency = cls.latency;
	newmis->starttime = cl.time + max((latency - 0.013) - ((float)pmove.client_ping / 1000), 0);
	newmis->endtime = cl.time + latency + 0.013;
	newmis->effects = 256;

	return newmis;
}

#define MAX_CLIP_PLANES 5
vec3_t bounce_retvec;
void Fproj_Physics_ClipVelocity(vec3_t vel, vec3_t norm, float f)
{
	vec3_t vel2;
	VectorScale(norm, DotProduct(vel, norm), vel2);
	VectorScale(vel2, f, vel2);
	VectorSubtract(vel, vel2, vel);

	if (vel[0] > -0.1 && vel[0] < 0.1) vel[0] = 0;
	if (vel[1] > -0.1 && vel[1] < 0.1) vel[1] = 0;
	if (vel[2] > -0.1 && vel[2] < 0.1) vel[2] = 0;

	VectorCopy(vel, bounce_retvec);
}

void Fproj_Physics_Bounce(fproj_t *proj, float dt)
{
	float gravity_value = movevars.gravity;

	if (proj->flags & 512)
	{
		if (proj->vel[2] >= 1 / 32)
			proj->flags = proj->flags &~512;
	}

	proj->vel[2] -= 0.5 * dt * gravity_value;
	
	VectorMA(proj->angs, dt, proj->avel, proj->angs);

	float movetime, bump;
	movetime = dt;
	for (bump = 0; bump < MAX_CLIP_PLANES && movetime > 0; ++bump)
	{
		vec3_t move;
		VectorScale(proj->vel, movetime, move);

		vec3_t to;
		VectorAdd(proj->org, move, to);
		//VectorMA(proj->vel, dt, to, to);
		trace_t phytrace = PM_TraceLine(proj->org, to);
		VectorCopy(phytrace.endpos, proj->org);

		if (phytrace.fraction == 1)
			break;

		movetime *= 1 - min(1, phytrace.fraction);

		float d, bouncefac, bouncestp;

		bouncefac = 0.5;
		bouncestp = 60 / 800;
		bouncestp *= gravity_value;

		Fproj_Physics_ClipVelocity(proj->vel, phytrace.plane.normal, 1 + bouncefac);
		VectorCopy(bounce_retvec, proj->vel);

		d = DotProduct(phytrace.plane.normal, proj->vel);
		if (phytrace.plane.normal[2] > 0.7 && d < bouncestp && d > -bouncestp)
		{
			proj->flags |= 512;
			VectorClear(proj->vel);
			VectorClear(proj->avel);
		}
		else
			proj->flags -= proj->flags & 512;
	}

	if (!(proj->flags & 512))
	{
		proj->vel[2] -= 0.5 * dt * gravity_value;
	}
}

fproj_t *CL_CreateFakeGrenade(void)
{
	if (pmove.client_ping == 0)
		return NULL;//&cl_fakeprojectiles[0];

	fproj_t *newmis = CL_AllocFakeProjectile();
	newmis->modelindex = cl_modelindices[mi_grenade];

	float latency = cls.latency;
	newmis->starttime = cl.time + max((latency - 0.013) - ((float)pmove.client_ping / 1000), 0);
	newmis->endtime = cl.time + latency;
	newmis->parttime = newmis->starttime + 0.013;

	newmis->type = 1;
	newmis->effects = EF_GRENADE;

	return newmis;
}

extern cvar_t cl_rocket2grenade;
fproj_t *CL_CreateFakeRocket(void)
{
	if (pmove.client_ping == 0)
		return NULL;//&cl_fakeprojectiles[0];

	fproj_t *newmis = CL_AllocFakeProjectile();
	newmis->modelindex = cl_modelindices[mi_rocket];

	float latency = cls.latency;
	newmis->starttime = cl.time + max((latency - 0.013) - ((float)pmove.client_ping / 1000), 0);
	newmis->endtime = cl.time + latency + 0.013;
	newmis->parttime = newmis->starttime + 0.013; //delay trail a tiny bit, otherwise it looks funny

	newmis->effects = EF_ROCKET;

	return newmis;
}



static predexplosion_t* CL_AllocatePredictedExplosion(void)
{
	float reuse_time = cl.time - cls.latency * 3;

	predexplosion_t *expl;
	int i;
	for (i = 0, expl = cl_predictedexplosions; i < MAX_PREDEXPLOSIONS; i++, expl++)
	{
		if (expl->time > reuse_time)
			continue;

		memset(expl, 0, sizeof(predexplosion_t));
		return expl;
	}

	memset(&cl_predictedexplosions[0], 0, sizeof(predexplosion_t));
	return &cl_predictedexplosions[0];
}


void CL_CheckPredictedExplosions(player_state_t *from, player_state_t *to)
{
	predexplosion_t *expl;
	int i;
	for (i = 0, expl = cl_predictedexplosions; i < MAX_PREDEXPLOSIONS; i++, expl++)
	{
		if (expl->time == 0)
			continue;

		if (from->state_time < expl->time && to->state_time >= expl->time)
		{
			int dmg;
			vec3_t diff;
			VectorSubtract(pmove.origin, expl->origin, diff);

			float distance = VectorLength(diff);
			dmg = expl->damage * (distance / expl->radius);

			if (distance < expl->radius)
			{
				VectorNormalize(diff);
				int j;
				for (j = 0; j < 3; j++)
				{
					pmove.velocity[j] += diff[j] * dmg * 1;
				}
			}
		}
	}
}


static void CL_ParseBeam(int type, vec3_t end)
{
	int ent;
	vec3_t start;

	ent = MSG_ReadShort();

	start[0] = MSG_ReadCoord();
	start[1] = MSG_ReadCoord();
	start[2] = MSG_ReadCoord();

	end[0] = MSG_ReadCoord();
	end[1] = MSG_ReadCoord();
	end[2] = MSG_ReadCoord();

	if (ent == cl.playernum + 1)
	{
		if (!pmove_nopred_weapon && cls.mvdprotocolextensions1 & MVD_PEXT1_WEAPONPREDICTION && cl_predict_beam.integer)
			return;
	}

	CL_CreateBeam(type, ent, start, end);
}

void CL_CreateBeam(int type, int ent, vec3_t start, vec3_t end)
{
	int i;
	beam_t *b;
	struct model_s *m;

	if (CL_Demo_SkipMessage(true))
		return;

	if (cl.intermission && ent >= 1 && ent <= MAX_CLIENTS) {
		return;
	}

    // an experimental protocol extension:
    // TE_LIGHTNING1 with entity num in -512..-1 range is a rail trail
    if (type == 1 && (ent >= -512 && ent <= -1)) 
	{
        int colors[8] = { 6 /* white (change to something else?) */,
                208 /* blue */,
                180 /* green */, 35 /* light blue */, 224 /* red */,
                133 /* magenta... kinda */, 192 /* yellow */, 6 /* white */};
        int color;
        int cnum;

        // -512..-257 are colored trails assigned to a specific
        // player, so overrides can be applied; encoded as follows:
        // 7654321076543210
        // 1111111nnnnnnccc  (n = player num, c = color code)
        cnum = ent & 7;
        //pnum = (ent >> 3) & 63;
        color = colors[cnum];

		//color is ignored by most of trails
		switch(r_instagibtrail.integer)
		{
			case 1:
			R_ParticleTrail(start, end, GRENADE_TRAIL);
			break;

			case 2:
			R_ParticleTrail(start, end, ROCKET_TRAIL);
			break;

			case 3:
			R_ParticleTrail(start, end, ALT_ROCKET_TRAIL);
			break;

			case 4:
			R_ParticleTrail(start, end, BLOOD_TRAIL);
			break;

			case 5:
			R_ParticleTrail(start, end, BIG_BLOOD_TRAIL);
			break;

			case 6:
			R_ParticleTrail(start, end, TRACER1_TRAIL);
			break;

			case 7:
			R_ParticleTrail(start, end, TRACER2_TRAIL);
			break;

			case 8:
			R_ParticleTrail(start, end, VOOR_TRAIL);
			break;

			case 9:
			Classic_ParticleRailTrail(start, end, color);
			break;

			case 10:
			R_ParticleTrail(start, end, RAIL_TRAIL);
			break;

			case 11:
			QMB_ParticleRailTrail(start, end, color);
			break;

			case 12:
			R_ParticleTrail(start, end, LAVA_TRAIL);
			break;

			case 13:
			R_ParticleTrail(start, end, AMF_ROCKET_TRAIL);
			break;

			default: break;
		}

        return;
    }

	switch (type) 
	{
		case 1:
			if (!cl_bolt1_mod)
				cl_bolt1_mod = Mod_CustomModel(custom_model_bolt, true);
			m = cl_bolt1_mod;
			break;
		case 2:
			if (!cl_bolt2_mod)
				cl_bolt2_mod = Mod_CustomModel(custom_model_bolt2, true);
			m = cl_bolt2_mod;
			break;
		case 3:
			if (!cl_bolt3_mod)
				cl_bolt3_mod = Mod_CustomModel(custom_model_bolt3, true);
			m = cl_bolt3_mod;
			break;
		case 4: default:
			if (!cl_beam_mod)
				cl_beam_mod = Mod_CustomModel(custom_model_beam, true);
			m = cl_beam_mod;
			break;
	}
	
	if (ent == cl.viewplayernum + 1) {
		VectorCopy(end, playerbeam_end); // for cl_fakeshaft
		playerbeam_update = true;
	}

	// Override any beam with the same entity.
	for (i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++) 
	{
		if (b->entity == ent) 
		{
			b->model = m;
			b->endtime = cl.time + 0.2;
			VectorCopy(start, b->start);
			VectorCopy(end, b->end);
			return;
		}
	}

	// Find a free beam.
	for (i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++)
	{
		if (!b->model || b->endtime < cl.time) 
		{
			b->entity = ent;
			b->model = m;
			b->endtime = cl.time + 0.2;
			VectorCopy(start, b->start);
			VectorCopy(end, b->end);
			return;
		}
	}
	Com_DPrintf ("beam list overflow!\n");
}

void CL_ExplosionSprite(vec3_t pos) 
{					
	explosion_t	*ex;

	ex = CL_AllocExplosion();
	VectorCopy(pos, ex->origin);
	ex->start = cl.time;
	if (!cl_explo_mod)
		cl_explo_mod = Mod_CustomModel(custom_model_explosion, true);
	ex->model = cl_explo_mod;
}

static void CL_Parse_TE_WIZSPIKE(vec3_t pos)
{
	if (amf_part_sparks.value && !Rulesets_RestrictParticles())
	{
		byte col[3] = {0, 124, 0};
		SparkGen(pos, col, 20, 90, 1);
	}
	else
	{
		R_RunParticleEffect(pos, vec3_origin, 20, 30);
	}
	S_StartSound(-1, 0, cl_sfx_wizhit, pos, 1, 1);
}

// FIXME: D-Kure: This seems to be unused
#if 0
static void CL_Parse_TE_KNIGHTSPIKE(vec3_t pos)
{
	if (amf_part_sparks.value && !Rulesets_RestrictParticles())
	{
		byte col[3] = {255, 77, 0};
		SparkGen(pos, col, 20, 150, 1);
	}
	else
	{
		R_RunParticleEffect(pos, vec3_origin, 226, 20);
	}

	S_StartSound(-1, 0, cl_sfx_knighthit, pos, 1, 1);
}
#endif

static void CL_Parse_TE_SPIKE(vec3_t pos)
{
	int rnd;

	if (amf_part_spikes.value && !Rulesets_RestrictParticles() && !gl_no24bit.integer)
		VXNailhit(pos, 10 * amf_part_spikes.value);
	else
	{
		R_RunParticleEffect(pos, vec3_origin, 0, 10);
	}

	if ( rand() % 5 ) 
	{
		S_StartSound(-1, 0, cl_sfx_tink1, pos, 1, 1);
	}
	else
	{
		rnd = rand() & 3;
		if (rnd == 1)
			S_StartSound(-1, 0, cl_sfx_ric1, pos, 1, 1);
		else if (rnd == 2)
			S_StartSound(-1, 0, cl_sfx_ric2, pos, 1, 1);
		else
			S_StartSound(-1, 0, cl_sfx_ric3, pos, 1, 1);
	}
}

static void CL_Parse_TE_SUPERSPIKE(vec3_t pos)
{
	int rnd;

	if (amf_part_spikes.value && !Rulesets_RestrictParticles() && !gl_no24bit.integer)
		VXNailhit(pos, 20 * amf_part_spikes.value);
	else
	{
		R_RunParticleEffect(pos, vec3_origin, 0, 20);
	}

	if (rand() % 5) 
	{
		S_StartSound(-1, 0, cl_sfx_tink1, pos, 1, 1);
	}
	else
	{
		rnd = rand() & 3;
		if (rnd == 1)
			S_StartSound(-1, 0, cl_sfx_ric1, pos, 1, 1);
		else if (rnd == 2)
			S_StartSound(-1, 0, cl_sfx_ric2, pos, 1, 1);
		else
			S_StartSound(-1, 0, cl_sfx_ric3, pos, 1, 1);
	}
}

static void CL_Parse_TE_EXPLOSION(vec3_t pos)
{
	dlight_t *dl;
	extern cvar_t gl_part_explosions;

	if (cls.state != ca_active) {
		return;
	}

	if (r_explosiontype.value == 2) {
		if (amf_part_teleport.value) {
			VXTeleport(pos);
		}
		else {
			// Teleport splash
			R_TeleportSplash(pos);
		}

		if (amf_coronas_tele.integer) {
			R_CoronasNew(C_BLUEFLASH, pos);
		}
	}
	else if (r_explosiontype.value == 3) {
		// lightning blood
		R_RunParticleEffect(pos, vec3_origin, 225, 50);
	}
	else if (r_explosiontype.value == 4) {
		// Big blood
		R_RunParticleEffect(pos, vec3_origin, 73, 20 * 32);
	}
	else if (r_explosiontype.value == 5) {
		// Double gunshot
		R_RunParticleEffect(pos, vec3_origin, 0, 20 * 14);
	}
	else if (r_explosiontype.value == 6) {
		if (amf_part_blobexplosion.value) {
			VXBlobExplosion(pos);
		}
		else {
			R_BlobExplosion(pos); // Blob explosion
		}
		if (amf_coronas.integer) {
			R_CoronasNew(C_BLUEFLASH, pos);
		}
	}
	else if (r_explosiontype.value == 7 && qmb_initialized && gl_part_explosions.value) {
		QMB_DetpackExplosion(pos);	// Detpack explosion
	}
	else if (r_explosiontype.value == 8 && qmb_initialized) {
		FuelRodExplosion(pos);
	}
	else if (r_explosiontype.value == 10) {
		/* Explosions turned off */
	}
	else {
		// sprite and particles
		if (amf_part_explosion.value)
			VXExplosion(pos);
		else {
			R_ParticleExplosion(pos); // Normal explosion
		}
	}

	if (r_explosionlight.value) {
		dl = CL_AllocDlight(0);
		VectorCopy(pos, dl->origin);
		dl->radius = 150 + 200 * bound(0, r_explosionlight.value, 1);
		dl->die = cl.time + 0.5;
		dl->decay = 300;
		if (r_explosiontype.value == 8) {
			dl->type = lt_green;
		}
		else {
			customlight_t cst_lt = { 0 };
			dlightColorEx(r_explosionlightcolor.value, r_explosionlightcolor.string, lt_explosion, true, &cst_lt);
			dl->type = cst_lt.type;

			if (dl->type == lt_custom) {
				VectorCopy(cst_lt.color, dl->color);
			}
		}
		if (amf_coronas.integer && r_explosiontype.integer != 7 && r_explosiontype.integer != 2 && r_explosiontype.integer != 8) {
			R_CoronasNew(C_FLASH, pos);
		}
	}

	S_StartSound(-1, 0, cl_sfx_r_exp3, pos, 1, 1);
}

static void CL_Parse_TE_TAREXPLOSION(vec3_t pos)
{
	if (amf_part_blobexplosion.value) {
		VXBlobExplosion(pos);
	}
	else {
		R_BlobExplosion(pos); // Blob explosion
	}
	
	if (amf_coronas.integer) {
		R_CoronasNew(C_BLUEFLASH, pos);
	}

	S_StartSound(-1, 0, cl_sfx_r_exp3, pos, 1, 1);
}

static void CL_Parse_TE_TELEPORT(vec3_t pos)
{
	if (r_telesplash.value)
	{
		if (amf_part_teleport.value) {
			VXTeleport(pos);
		}
		else {
			R_TeleportSplash(pos); // Teleport splash
		}
		if (amf_coronas_tele.integer) {
			R_CoronasNew(C_BLUEFLASH, pos);
		}
	}
}

static void CL_Parse_TE_GUNSHOT(vec3_t pos)
{
	int count;

	if (cls.nqdemoplayback) {
		count = 1;
	}
	else {
		count = MSG_ReadByte();
	}

	pos[0] = MSG_ReadCoord();
	pos[1] = MSG_ReadCoord();
	pos[2] = MSG_ReadCoord();

	if (CL_Demo_SkipMessage(true)) {
		return;
	}
	
	if (amf_part_gunshot.value && !Rulesets_RestrictParticles()) {
		VXGunshot(pos, 5 * count * amf_part_gunshot.value);
	}
	else {
		R_RunParticleEffect(pos, vec3_origin, 256 /* magic! */, 20 * count);
	}
}

static void CL_Parse_TE_BLOOD(vec3_t pos)
{
	int count;
	dlight_t *dl;
	
	if (cls.nqdemoplayback) {
		// NQ_TE_EXPLOSION2
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();
		MSG_ReadByte(); // colorStart
		MSG_ReadByte(); // colorLength

		if (CL_Demo_SkipMessage(true)) {
			return;
		}

		dl = CL_AllocDlight(0);
		VectorCopy(pos, dl->origin);
		dl->radius = 350;
		dl->die = cl.time + 0.5;
		dl->decay = 300;
		S_StartSound(-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		return;
	}

	count = MSG_ReadByte();
	pos[0] = MSG_ReadCoord();
	pos[1] = MSG_ReadCoord();
	pos[2] = MSG_ReadCoord();

	if (CL_Demo_SkipMessage(true)) {
		return;
	}

	if (amf_part_blood.value) {
		VXBlood(pos, 5 * count * amf_part_blood.value);
	}
	else {
		R_RunParticleEffect(pos, vec3_origin, 73, 20 * count);
	}
}

static void CL_Parse_TE_LIGHTNINGBLOOD(vec3_t pos)
{
	if (cls.nqdemoplayback) {
		// NQ_TE_BEAM - grappling hook beam
		CL_ParseBeam(4, pos);
	}
	else {
		extern cvar_t gl_part_blood;

		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();

		if (CL_Demo_SkipMessage(true)) {
			return;
		}

		R_RunParticleEffect(pos, vec3_origin, gl_part_blood.value?225:r_lgblood.value, 50); // 225 default
	}
}

void CL_ParseTEnt (void) 
{
	int	type;
	vec3_t pos;
	qbool parsed = false;
	
	memset(pos, 0, sizeof(pos));
	
	type = MSG_ReadByte();

	// Handle special cases first (need to parse more than just position).
	switch (type)
	{
		// Lightning bolts.
		case TE_LIGHTNING1:			
			CL_ParseBeam(1, pos);
			parsed = true;
			break;

		case TE_LIGHTNING2:
			CL_ParseBeam(2, pos);
			parsed = true;
			break;

		case TE_LIGHTNING3:
			CL_ParseBeam(3, pos);
			parsed = true;
			break;

		// Bullet hitting wall.
		case TE_GUNSHOT:
			CL_Parse_TE_GUNSHOT(pos);
			parsed = true;
			break;

		// Bullets hitting body.
		case TE_BLOOD: 
			CL_Parse_TE_BLOOD(pos);
			parsed = true;
			break;

		// Lightning hitting body
		case TE_LIGHTNINGBLOOD: 
			CL_Parse_TE_LIGHTNINGBLOOD(pos);
			parsed = true;
			break;
	}

	if (!parsed)
	{
		// The rest only needs position.
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();

		if (CL_Demo_SkipMessage(true)) {
			return;
		}
		
		switch (type) 
		{
			// Spike hitting wall.
			case TE_WIZSPIKE: 
				CL_Parse_TE_WIZSPIKE(pos);
				break;

			// Spike hitting wall.
			case TE_KNIGHTSPIKE: 
				CL_Parse_TE_WIZSPIKE(pos);
				break;
			
			// Spike hitting wall.
			case TE_SPIKE: 
				CL_Parse_TE_SPIKE(pos);
				break;

			// Super spike hitting wall.
			case TE_SUPERSPIKE:		
				CL_Parse_TE_SUPERSPIKE(pos);
				break;

			// Rocket explosion.
			case TE_EXPLOSION:	
				CL_Parse_TE_EXPLOSION(pos);
				break;

			// Tarbaby explosion
			case TE_TAREXPLOSION:
				CL_Parse_TE_TAREXPLOSION(pos);
				break;

			case TE_LAVASPLASH:	
				R_LavaSplash(pos);
				break;

			case TE_TELEPORT:
				CL_Parse_TE_TELEPORT(pos);
				break;

			default:
				Host_Error("CL_ParseTEnt: unknown type %d", type);
		}
	}
	else if (CL_Demo_SkipMessage(true)) {
		return;
	}

	// Save the temp entities.
	VectorCopy(pos, temp_entities.list[temp_entities.count].pos);
	temp_entities.list[temp_entities.count].time = cls.demoplayback ? cls.demotime : cls.realtime; // FIXME: Use realtime here?
	temp_entities.list[temp_entities.count].type = type;
	temp_entities.count = (temp_entities.count + 1 >= MAX_TEMP_ENTITIES) ? 0 : temp_entities.count + 1;
}

void vectoangles(vec3_t vec, vec3_t ang);

#define MAX_LIGHTNINGBEAMS 10

static float fakeshaft_policy (void)
{
	if (cls.demoplayback || cl.spectator) {
		return 1;
	}
	else {
		return cl.fakeshaft_policy;
	}
}

static void CL_UpdateBeams(void)
{
	int i;
	beam_t *b;	
	vec3_t dist, org;
	entity_t ent;
	float d, yaw, pitch, forward, fakeshaft;
	extern cvar_t v_viewheight;

	float lg_size = bound(3, amf_lightning_size.value, 30);
	int beamstodraw, j, k;
	qbool sparks = false;
	vec3_t beamstart[MAX_LIGHTNINGBEAMS], beamend[MAX_LIGHTNINGBEAMS];
	vec3_t furthest_sparks;
	beamstodraw = bound(1, amf_lightning.value, MAX_LIGHTNINGBEAMS);	

	memset (&ent, 0, sizeof(entity_t));
	ent.colormap = vid.colormap;

	fakeshaft = cl.intermission ? 0 : bound(0, cl_fakeshaft.value, fakeshaft_policy());

	// Update lightning.
	for (i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++) {
		sparks = false;

		if (!b->model || b->endtime < cl.time) {
			continue;
		}
		if (b->entity >= 1 && b->entity <= MAX_CLIENTS && cl.intermission) {
			continue;
		}

		// if coming from the player, update the start position
		if (b->entity == cl.viewplayernum + 1) {
			VectorCopy (cl.simorg, b->start);
			b->start[2] += cl.crouch + bound(-7, v_viewheight.value, 4);
			VectorMA(b->start, r_shiftbeam.value, vright, b->start);

			if (fakeshaft && (cl_fakeshaft_extra_updates.value || playerbeam_update)) {
				vec3_t	forward, v, org, ang, target_angles, target_origin;
				float	delta;
				trace_t	trace;

				playerbeam_update = false;

				if (cl_fakeshaft.value == 2 && !cl.spectator && !cls.nqdemoplayback) {
					// try to simulate 13 ms ping
					frame_t *frame = &cl.frames[(cls.netchan.outgoing_sequence - 2) & UPDATE_MASK];
					VectorCopy(frame->cmd.angles, target_angles);
					VectorCopy(frame->playerstate[cl.viewplayernum].origin, target_origin);
				}
				else {
					VectorCopy(cl.simangles, target_angles);
					VectorCopy(cl.simorg, target_origin);
				}

				VectorSubtract (playerbeam_end, target_origin, v);
				v[2] -= 22;	// Adjust for view height.
				vectoangles (v, ang);

				// Lerp pitch.
				ang[0] = -ang[0];
				if (ang[0] < -180) {
					ang[0] += 360;
				}
				ang[0] += (target_angles[0] - ang[0]) * fakeshaft;

				// Lerp yaw.
				delta = target_angles[1] - ang[1];
				if (delta > 180) {
					delta -= 360;
				}
				if (delta < -180) {
					delta += 360;
				}
				ang[1] += delta * fakeshaft;
				ang[2] = 0;

				AngleVectors (ang, forward, NULL, NULL);
				VectorScale (forward, 600, forward);
				VectorCopy (target_origin, org);
				org[2] += 16;
				VectorAdd (org, forward, b->end);

				trace = PM_TraceLine (org, b->end);
				if (trace.fraction < 1) {
					VectorCopy(trace.endpos, b->end);
				}
			}
		}

		// Calculate pitch and yaw.
		VectorSubtract (b->end, b->start, dist);

		if (dist[1] == 0 && dist[0] == 0) {
			yaw = 0;
			pitch = (dist[2] > 0) ? 90 : 270;
		}
		else {
			yaw = atan2(dist[1], dist[0]) * 180 / M_PI;
			if (yaw < 0) {
				yaw += 360;
			}
	
			forward = sqrt (dist[0]*dist[0] + dist[1]*dist[1]);
			pitch = atan2(dist[2], forward) * 180 / M_PI;
			if (pitch < 0) {
				pitch += 360;
			}
		}

		// Add new entities for the lightning.
		VectorCopy (b->start, org);
		
		for (k = 0; k < beamstodraw; k++) {
			VectorCopy(b->start, beamstart[k]);
		}
		
		d = VectorNormalize(dist);
		VectorScale (dist, 30, dist);

		for ( ; d > 0; d -= 30) {
			if (!amf_lightning.value || Rulesets_RestrictParticles()) {
				VectorCopy (org, ent.origin);
				ent.model = b->model;
				ent.angles[0] = pitch;
				ent.angles[1] = yaw;
				ent.angles[2] = (cl.racing ? 0 : rand() % 360);
				if ((cl.racing && b->entity >= MAX_CLIENTS) || !Rulesets_RestrictParticles()) {
					ent.alpha = r_shaftalpha.value;
				}

				CL_AddEntity (&ent);
			}
			else if (!ISPAUSED) {
				//VULT - Some people might like their lightning beams thicker
				for (k = 0; k < beamstodraw; k++) {
					VectorAdd(org, dist, beamend[k]);
					if (!cl.racing) {
						for (j = 0; j < 3; j++) {
							beamend[k][j] += f_rnd(-lg_size, lg_size);
						}
					}
					VX_LightningBeam(beamstart[k], beamend[k]);
					VectorCopy(beamend[k], beamstart[k]);
				}
			}

			// The infamous d-light glow has been replaced with a simple corona so it doesn't light up the room anymore.
			if (amf_coronas.value && amf_lightning.value && !ISPAUSED) {
				if (b->entity == cl.viewplayernum + 1 && !((cls.demoplayback || cl.spectator) && cl_camera_tpp.integer)) {
					R_CoronasNew(C_SMALLLIGHTNING, org);
				}
				else {
					R_CoronasNew(C_LIGHTNING, org);
				}
			}

			VectorAdd (org, dist, org);

			// LIGHTNING SPARKS
			if (amf_lightning_sparks.value && !Rulesets_RestrictParticles() && !ISPAUSED && !gl_no24bit.integer) {
				trace_t	trace;
				trace = PM_TraceLine (org, b->end);
				if (trace.fraction < 1) {
					VectorCopy(trace.endpos, furthest_sparks);
					sparks = true;
				}
			}
		}

		if (sparks) {
			SparkGen(furthest_sparks, amf_lightning_color.color, (int)(3 * amf_lightning_sparks.value), amf_lightning_sparks_size.value, 0.25);
		}
	}
}

static void CL_UpdateExplosions(void)
{
	int f;
	explosion_t	*ex, *next, *hnode;
	entity_t ent;

	memset (&ent, 0, sizeof(entity_t));
	ent.colormap = vid.colormap;

	hnode = &cl_explosions_headnode;
	for (ex = hnode->next; ex != hnode; ex = next) {
		next = ex->next;
		f = 10 * (cl.time - ex->start);

		if (f >= ex->model->numframes) {
			CL_FreeExplosion (ex);
			continue;
		}

		VectorCopy (ex->origin, ent.origin);
		ent.model = ex->model;
		ent.frame = f;

		CL_AddEntity (&ent);
	}
}

static void CL_UpdateFakeProjectiles(void)
{
	int i;
	fproj_t	*prj;
	
	centity_t *cent;
	entity_t ent;
	entity_state_t centstate;
	customlight_t cst_lt = { 0 };
	memset(&centstate, 0, sizeof(entity_state_t));

	memset(&ent, 0, sizeof(entity_t));
	ent.colormap = vid.colormap;

	for (i = 0; i < MAX_FAKEPROJ; i++) {
		prj = &cl_fakeprojectiles[i];
		cent = &prj->cent;//&cl_entities[prj->entnum];
		//if (prj->entnum > 0 && prj->entnum < (CL_MAX_EDICTS - 1))
		//	cent = &cl_entities[prj->entnum];

		if (cent == NULL)
			continue;

		//cent->sequence = cl.validsequence;

		if (cl.time <= prj->starttime)
		{
			continue;
		}
		else if (cl.time >= prj->endtime)
		{
			if (prj->dl_key)
			{
				dlight_t *dl;
				dl = cl_dlights;
				int ik;
				for (ik = 0; ik < MAX_DLIGHTS; ik++, dl++) {
					if (dl->key == prj->dl_key) {
						memset(dl, 0, sizeof(*dl));
						cl_dlight_active[ik / 32] -= (cl_dlight_active[ik / 32] & (1 << (ik % 32)));
					}
				}

				prj->dl_key = 0;
			}

			continue;
		}


		//memset(&cent, 0, sizeof(centity_t));
		cent->current = centstate;
		
		if (!prj->modelindex || !cl.model_precache[prj->modelindex] || prj->modelindex >= MAX_MODELS)
			continue;

		//continue;

		ent.model = cl.model_precache[prj->modelindex];
		centstate.modelindex = prj->modelindex;
		centstate.number = prj->dl_key;

		vec3_t sframe_org;
		VectorCopy(prj->org, sframe_org);

		if (prj->type == 1)
		{
			Fproj_Physics_Bounce(prj, cls.frametime);

			VectorCopy(prj->org, ent.origin);
			VectorCopy(prj->angs, ent.angles);
		}
		else
		{
			float modified_time = cl.time;
			#ifdef MVD_PEXT1_SIMPLEPROJECTILE
			if (cl_sproj_xerp.value)
			{
				if (prj->owner != (cl.viewplayernum + 1))
				{
					modified_time += min(150, cls.latency);
				}
			}
			#endif

			VectorCopy(prj->start, ent.origin);
			vec3_t traveled;
			VectorScale(prj->vel, (modified_time - prj->starttime) + 0.02, traveled);
			VectorAdd(ent.origin, traveled, ent.origin);
			VectorCopy(prj->angs, ent.angles);
			VectorCopy(ent.origin, prj->org);

			trace_t trace = PM_TraceLine(prj->start, ent.origin);
			if (trace.fraction < 1)
			{
				#ifdef MVD_PEXT1_SIMPLEPROJECTILE
				if (prj->modelindex == cl_modelindices[mi_rocket])
				{
					predexplosion_t *expl = CL_AllocatePredictedExplosion();
					expl->time = cls.realtime;
					expl->damage = 100;
					expl->radius = expl->damage + 40;
					VectorCopy(trace.endpos, expl->origin);
				}

				if (prj->parttime)
					continue;
				else
					VectorCopy(trace.endpos, prj->org);
				#else
				continue;//prj->endtime = cl.time;
				#endif
			}
		}

		if (prj->effects)
		{
			if (cl.time >= prj->parttime)
			{
				prj->parttime = cl.time + 0.013;
				if (!VectorLength(prj->partorg))
				{
					VectorCopy(prj->org, prj->partorg);

					VectorCopy(prj->partorg, cent->trails[3].stop);
					VectorCopy(prj->partorg, cent->trails[2].stop);
					VectorCopy(prj->partorg, cent->trails[1].stop);
					VectorCopy(prj->partorg, cent->trails[0].stop);
				}

				prj->partcount++;
				VectorCopy(prj->partorg, cent->old_origin);
				///*
				VectorCopy(prj->partorg, cent->trails[3].stop);
				VectorCopy(prj->partorg, cent->trails[2].stop);
				VectorCopy(prj->partorg, cent->trails[1].stop);
				VectorCopy(prj->partorg, cent->trails[0].stop);
				//*/
				VectorCopy(ent.origin, cent->current.origin);
				VectorCopy(ent.origin, cent->lerp_origin);
				VectorCopy(ent.origin, prj->partorg);


				if (prj->effects & 256)
				{
					if (amf_nailtrail.integer && !gl_no24bit.integer) {
						// VULT NAILTRAIL
						if (amf_nailtrail_plasma.integer) {
							byte color[3];
							color[0] = 0; color[1] = 70; color[2] = 255;
							FireballTrail(cent, color, 0.6, 0.3);
						}
						else {
							//TODO: fix this
							//ParticleNailTrail(cent, 2, 0.4);
						}
					}
				}
				else
				{
					CL_AddParticleTrail(&ent, cent, &cst_lt, &centstate);
				}
			}
		}

		// hacky fix for cl_r2g, we have to change the model after the particles have been handled
		if (prj->effects & EF_ROCKET)
		{
			if (cl_rocket2grenade.integer)
			{
				ent.model = cl.model_precache[cl_modelindices[mi_grenade]];
			}
		}

		// finally add fake projectile to the scene
		CL_AddEntity(&ent);
	}
}

void CL_UpdateTEnts (void)
{
	CL_UpdateBeams();
	CL_UpdateExplosions();
	CL_UpdateFakeProjectiles();
}

