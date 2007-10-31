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
#ifdef GLQUAKE
#include "gl_model.h"
#include "gl_local.h"
#else
#include "r_model.h"
#include "r_local.h"
#endif
#include "vx_stuff.h"
#include "rulesets.h"
#include "pmove.h"
#include "utils.h"
#include "qsound.h"


#define	MAX_BEAMS 32
typedef struct {
	int entity;
	model_t *model;
	float endtime;
	vec3_t start, end;
} beam_t;

beam_t cl_beams[MAX_BEAMS];

static vec3_t playerbeam_end;

#define MAX_EXPLOSIONS 32
typedef struct explosion_s {
	struct explosion_s *prev, *next;
	vec3_t origin;
	float start;
	model_t *model;
} explosion_t;


//		cl_free_explosions = linear linked list of free explosions
explosion_t	cl_explosions[MAX_EXPLOSIONS];
explosion_t cl_explosions_headnode, *cl_free_explosions;

static model_t	*cl_explo_mod, *cl_bolt1_mod, *cl_bolt2_mod, *cl_bolt3_mod, *cl_beam_mod;

sfx_t	*cl_sfx_wizhit, *cl_sfx_knighthit, *cl_sfx_tink1, *cl_sfx_ric1, *cl_sfx_ric2, *cl_sfx_ric3, *cl_sfx_r_exp3;

cvar_t r_lgblood = {"r_lgbloodColor", "225"};
cvar_t r_shiftbeam = {"r_shiftbeam", "0"};

void CL_InitTEnts (void) {
	cl_sfx_wizhit = S_PrecacheSound ("wizard/hit.wav");
	cl_sfx_knighthit = S_PrecacheSound ("hknight/hit.wav");
	cl_sfx_tink1 = S_PrecacheSound ("weapons/tink1.wav");
	cl_sfx_ric1 = S_PrecacheSound ("weapons/ric1.wav");
	cl_sfx_ric2 = S_PrecacheSound ("weapons/ric2.wav");
	cl_sfx_ric3 = S_PrecacheSound ("weapons/ric3.wav");
	cl_sfx_r_exp3 = S_PrecacheSound ("weapons/r_exp3.wav");
	
	Cvar_SetCurrentGroup(CVAR_GROUP_EYECANDY);
	Cvar_Register(&r_lgblood);
	Cvar_Register(&r_shiftbeam);
	Cvar_ResetCurrentGroup();
}

void CL_ClearTEnts (void) {
	int i;

	cl_explo_mod = cl_bolt1_mod = cl_bolt2_mod = cl_bolt3_mod = cl_beam_mod = NULL;

	memset (&cl_beams, 0, sizeof(cl_beams));
	memset (&cl_explosions, 0, sizeof(cl_explosions));

	// link explosions 
	cl_free_explosions = cl_explosions; 
	cl_explosions_headnode.next = cl_explosions_headnode.prev = &cl_explosions_headnode; 

	for (i = 0; i < MAX_EXPLOSIONS - 1; i++) 
		cl_explosions[i].next = &cl_explosions[i + 1]; 
}

explosion_t *CL_AllocExplosion (void) {
	explosion_t *ex;

	if (cl_free_explosions) {	
		// take a free explosion if possible
		ex = cl_free_explosions;
		cl_free_explosions = ex->next;
	} else {
		// grab the oldest one otherwise
		ex = cl_explosions_headnode.prev;
		ex->prev->next = ex->next;
		ex->next->prev = ex->prev;
	}

	// put the explosion at the start of the list
	ex->prev = &cl_explosions_headnode;
	ex->next = cl_explosions_headnode.next;
	ex->next->prev = ex;
	ex->prev->next = ex;

	return ex;
}

void CL_FreeExplosion (explosion_t *ex) {
	// remove from linked active list
	ex->prev->next = ex->next;
	ex->next->prev = ex->prev;

	// insert into front linked free list
	ex->next = cl_free_explosions;
	cl_free_explosions = ex;
}

static void CL_ParseBeam (int type) {
	int ent, i;
	vec3_t start, end;
	beam_t *b;
	struct model_s *m;

	ent = MSG_ReadShort ();

	start[0] = MSG_ReadCoord ();
	start[1] = MSG_ReadCoord ();
	start[2] = MSG_ReadCoord ();

	end[0] = MSG_ReadCoord ();
	end[1] = MSG_ReadCoord ();
	end[2] = MSG_ReadCoord ();

        // an experimental protocol extension:
        // TE_LIGHTNING1 with entity num in -512..-1 range is a rail trail
	        if (type == 1 && (ent >= -512 && ent <= -1)) {
                int colors[8] = { 6 /* white (change to something else?) */,
                        208 /* blue */,
                        180 /* green */, 35 /* light blue */, 224 /* red */,
                        133 /* magenta... kinda */, 192 /* yellow */, 6 /* white */};
                int color;
                int pnum, cnum;

                // -512..-257 are colored trails assigned to a specific
                // player, so overrides can be applied; encoded as follows:
                // 7654321076543210
                // 1111111nnnnnnccc  (n = player num, c = color code)
                cnum = ent & 7;
                pnum = (ent >> 3) & 63;
                if (pnum < MAX_CLIENTS)
                {
                        // TODO: apply team/enemy overrides
                }
                color = colors[cnum];

#ifdef GLQUAKE
				QMB_ParticleRailTrail (start, end, color);
#else
				Classic_ParticleRailTrail (start, end, color);
#endif

                return;
        }

	switch (type) {
		case 1:
			if (!cl_bolt1_mod)
				cl_bolt1_mod = Mod_ForName ("progs/bolt.mdl", true);
			m = cl_bolt1_mod;
			break;
		case 2:
			if (!cl_bolt2_mod)
				cl_bolt2_mod = Mod_ForName ("progs/bolt2.mdl", true);
			m = cl_bolt2_mod;
			break;
		case 3:
			if (!cl_bolt3_mod)
				cl_bolt3_mod = Mod_ForName ("progs/bolt3.mdl", true);
			m = cl_bolt3_mod;
			break;
		case 4: default:
			if (!cl_beam_mod)
				cl_beam_mod = Mod_ForName ("progs/beam.mdl", true);
			m = cl_beam_mod;
			break;
	}
	
	if (ent == cl.viewplayernum + 1)
		VectorCopy (end, playerbeam_end);	// for cl_fakeshaft

	// override any beam with the same entity
	for (i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++) {
		if (b->entity == ent) {
			b->model = m;
			b->endtime = cl.time + 0.2;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			return;
		}
	}

	// find a free beam
	for (i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++)	{
		if (!b->model || b->endtime < cl.time) {
			b->entity = ent;
			b->model = m;
			b->endtime = cl.time + 0.2;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			return;
		}
	}
	Com_DPrintf ("beam list overflow!\n");
}

void CL_ExplosionSprite (vec3_t pos) {					
	explosion_t	*ex;

	ex = CL_AllocExplosion ();
	VectorCopy (pos, ex->origin);
	ex->start = cl.time;
	if (!cl_explo_mod)
		cl_explo_mod = Mod_ForName ("progs/s_explod.spr", true);
	ex->model = cl_explo_mod;
}

void CL_ParseTEnt (void) {
	int	rnd, cnt, type;
	vec3_t pos;
	dlight_t *dl;
#ifdef GLQUAKE
	byte col[3];
#endif
	pos[0] = pos[1] = pos[2] = 0;
	type = MSG_ReadByte ();
	switch (type) {
	case TE_WIZSPIKE:		// spike hitting wall
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
#ifdef GLQUAKE
		if (amf_part_sparks.value && !Rulesets_RestrictParticles())
		{
			col[0] = 0; col[1] = 124; col[2] = 0;
			SparkGen (pos, col, 20, 90, 1);
		}
		else
#endif
			R_RunParticleEffect (pos, vec3_origin, 20, 30);
		S_StartSound (-1, 0, cl_sfx_wizhit, pos, 1, 1);
		break;

	case TE_KNIGHTSPIKE:	// spike hitting wall
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
#ifdef GLQUAKE
		if (amf_part_sparks.value && !Rulesets_RestrictParticles())
		{
			col[0] = 255; col[1] = 77; col[2] = 0;
			SparkGen (pos, col, 20, 150, 1);
		}
		else
#endif
			R_RunParticleEffect (pos, vec3_origin, 226, 20);
		S_StartSound (-1, 0, cl_sfx_knighthit, pos, 1, 1);
		break;

	case TE_SPIKE:			// spike hitting wall
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
#ifdef GLQUAKE
		if (amf_part_spikes.value && !Rulesets_RestrictParticles())
			VXNailhit (pos, 10*amf_part_spikes.value);
		else
#endif
			R_RunParticleEffect (pos, vec3_origin, 0, 10);

		if ( rand() % 5 ) {
			S_StartSound (-1, 0, cl_sfx_tink1, pos, 1, 1);
		} else {
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound (-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				S_StartSound (-1, 0, cl_sfx_ric3, pos, 1, 1);
		}
		break;
	case TE_SUPERSPIKE:		// super spike hitting wall
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
#ifdef GLQUAKE
		if (amf_part_spikes.value && !Rulesets_RestrictParticles())
			VXNailhit (pos, 20*amf_part_spikes.value);
		else
#endif
			R_RunParticleEffect (pos, vec3_origin, 0, 20);

		if ( rand() % 5 ) {
			S_StartSound (-1, 0, cl_sfx_tink1, pos, 1, 1);
		} else {
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound (-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				S_StartSound (-1, 0, cl_sfx_ric3, pos, 1, 1);
		}
		break;

	case TE_EXPLOSION:		// rocket explosion
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();

		if (cls.state != ca_active)
			break;

		if (r_explosiontype.value == 2) 
		{
#ifdef GLQUAKE
			if (amf_part_teleport.value)
				VXTeleport(pos);
			else
#endif
				R_TeleportSplash (pos);									//teleport splash
#ifdef GLQUAKE
			if (amf_coronas_tele.value)
				NewCorona (C_BLUEFLASH, pos);
#endif
		}
		else if (r_explosiontype.value == 3) 
		{	
			R_RunParticleEffect (pos, vec3_origin, 225, 50);		//lightning blood
		}
		else if (r_explosiontype.value == 4) 
		{	
			R_RunParticleEffect (pos, vec3_origin, 73, 20 * 32);	//big blood
		}
		else if (r_explosiontype.value == 5) 
		{
			R_RunParticleEffect (pos, vec3_origin, 0, 20 * 14);		//dbl gunshot
		}
		else if (r_explosiontype.value == 6) 
		{
#ifdef GLQUAKE
			if (amf_part_blobexplosion.value)
				VXBlobExplosion(pos);
			else
#endif
				R_BlobExplosion (pos);									//blob explosion
#ifdef GLQUAKE
			//VULT CORONAS
			if (amf_coronas.value)
				NewCorona (C_BLUEFLASH, pos);
		}
		else if (r_explosiontype.value == 7 && qmb_initialized && gl_part_explosions.value) 
		{
			QMB_DetpackExplosion (pos);								//detpack explosion
#endif
		}
#ifdef GLQUAKE
		//VULT PARTICLES
		else if (r_explosiontype.value == 8 && qmb_initialized)
		{
			FuelRodExplosion (pos);
		}
//		else if (r_explosiontype.value == 9)
//		{
//			BurningExplosion (pos); 
//		}
#endif
		else
		{	//sprite and particles
#ifdef GLQUAKE
			if (amf_part_explosion.value)
				VXExplosion(pos);
			else
#endif
				R_ParticleExplosion (pos);								//normal explosion
		}

		if (r_explosionlight.value)	
		{
			dl = CL_AllocDlight (0);
			VectorCopy (pos, dl->origin);
			dl->radius = 150 + 200 * bound(0, r_explosionlight.value, 1);
			dl->die = cl.time + 0.5;
			dl->decay = 300;
#ifdef GLQUAKE
			if (r_explosiontype.value == 8)
				dl->type = lt_green;
			else
#endif
			{
				customlight_t cst_lt = {0};
				dlightColorEx(r_explosionlightcolor.value, r_explosionlightcolor.string, lt_explosion, true, &cst_lt);
				dl->type = cst_lt.type;
#ifdef GLQUAKE
				if (dl->type == lt_custom)
					VectorCopy(cst_lt.color, dl->color);
#endif
			}
#ifdef GLQUAKE
			//VULT CORONAS
			if (amf_coronas.value && r_explosiontype.value != 7 && r_explosiontype.value != 2 && r_explosiontype.value != 8)
				NewCorona (C_FLASH, pos);
#endif
		}

		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	case TE_TAREXPLOSION:			// tarbaby explosion
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
#ifdef GLQUAKE
		if (amf_part_blobexplosion.value)
			VXBlobExplosion(pos);
		else
#endif
			R_BlobExplosion (pos);									//blob explosion
		//VULT CORONAS
#ifdef GLQUAKE
		if (amf_coronas.value)
			NewCorona (C_BLUEFLASH, pos);
#endif
		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	case TE_LIGHTNING1:			// lightning bolts
		CL_ParseBeam (1);
		break;

	case TE_LIGHTNING2:			// lightning bolts
		CL_ParseBeam (2);
		break;

	case TE_LIGHTNING3:			// lightning bolts
		CL_ParseBeam (3);
		break;

	case TE_LAVASPLASH:	
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
		R_LavaSplash (pos);
		break;

	case TE_TELEPORT:
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
		if (r_telesplash.value){
#ifdef GLQUAKE
		if (amf_part_teleport.value)
			VXTeleport(pos);
		else
#endif
			R_TeleportSplash (pos);									//teleport splash
#ifdef GLQUAKE
		if (amf_coronas_tele.value)
			NewCorona (C_BLUEFLASH, pos);
#endif
			}
		break;

	case TE_GUNSHOT:			// bullet hitting wall
		if (cls.nqdemoplayback)
			cnt = 1;
		else
			cnt = MSG_ReadByte ();
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
#ifdef GLQUAKE
		if (amf_part_gunshot.value && !Rulesets_RestrictParticles())
			VXGunshot (pos, 5*cnt*amf_part_gunshot.value);
		else
#endif
			R_RunParticleEffect (pos, vec3_origin, 0, 20*cnt);
		break;

	case TE_BLOOD:				// bullets hitting body
		if (cls.nqdemoplayback) {
			// NQ_TE_EXPLOSION2
			int colorStart, colorLength;
			pos[0] = MSG_ReadCoord ();
			pos[1] = MSG_ReadCoord ();
			pos[2] = MSG_ReadCoord ();
			colorStart = MSG_ReadByte ();
			colorLength = MSG_ReadByte ();
//##			CL_ParticleExplosion2 (pos, colorStart, colorLength);
			dl = CL_AllocDlight (0);
			VectorCopy (pos, dl->origin);
			dl->radius = 350;
			dl->die = cl.time + 0.5;
			dl->decay = 300;
			S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
			break;
		}
		cnt = MSG_ReadByte ();
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
#ifdef GLQUAKE
		if (amf_part_blood.value)
			VXBlood (pos, 5*cnt*amf_part_blood.value);
		else
#endif
			R_RunParticleEffect (pos, vec3_origin, 73, 20*cnt);
		break;

	case TE_LIGHTNINGBLOOD:		// lightning hitting body
		if (cls.nqdemoplayback) {
			// NQ_TE_BEAM - grappling hook beam
			CL_ParseBeam (4);
			break;
		}
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
		R_RunParticleEffect (pos, vec3_origin, r_lgblood.value, 50); // 225 default
		break;

	default:
		Host_Error("CL_ParseTEnt: bad type");
	}

	// Save the temp entities.
	VectorCopy(pos, temp_entities.list[temp_entities.count].pos);
	temp_entities.list[temp_entities.count].time = cls.demoplayback ? cls.demotime : cls.realtime; // FIXME: Use realtime here?
	temp_entities.list[temp_entities.count].type = type;
	temp_entities.count = (temp_entities.count + 1 >= MAX_TEMP_ENTITIES) ? 0 : temp_entities.count + 1;
}

void vectoangles(vec3_t vec, vec3_t ang);


#define MAX_LIGHTNINGBEAMS 10
//VULT LIGHTNING

void CL_UpdateBeams (void) {
	int i;
	beam_t *b;	
	vec3_t dist, org;
	entity_t ent;
	float d, yaw, pitch, forward, fakeshaft;
	extern cvar_t v_viewheight;

#ifdef GLQUAKE
	float lg_size = bound(3, amf_lightning_size.value, 30);
	int beamstodraw, j, k;
	qbool sparks = false;
	vec3_t beamstart[MAX_LIGHTNINGBEAMS], beamend[MAX_LIGHTNINGBEAMS];
#endif

	memset (&ent, 0, sizeof(entity_t));
	ent.colormap = vid.colormap;

#ifdef GLQUAKE
	beamstodraw = bound(1, amf_lightning.value, MAX_LIGHTNINGBEAMS);
#endif
	
	fakeshaft = bound(0, cl_fakeshaft.value, cl.fakeshaft);

	// update lightning
	for (i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++)	{
		if (!b->model || b->endtime < cl.time)
			continue;

		// if coming from the player, update the start position
		if (b->entity == cl.viewplayernum + 1) {
			VectorCopy (cl.simorg, b->start);
			b->start[2] += cl.crouch + bound(-7, v_viewheight.value, 4);
			VectorMA(b->start, r_shiftbeam.value, vright, b->start);
			if (cl_fakeshaft.value)	{
				vec3_t	forward, v, org, ang;
				float	delta;
				trace_t	trace;

				VectorSubtract (playerbeam_end, cl.simorg, v);
				v[2] -= 22;		// adjust for view height
				vectoangles (v, ang);

				// lerp pitch
				ang[0] = -ang[0];
				if (ang[0] < -180)
					ang[0] += 360;
				ang[0] += (cl.simangles[0] - ang[0]) * fakeshaft;

				// lerp yaw
				delta = cl.simangles[1] - ang[1];
				if (delta > 180)
					delta -= 360;
				if (delta < -180)
					delta += 360;
				ang[1] += delta * fakeshaft;
				ang[2] = 0;

				AngleVectors (ang, forward, NULL, NULL);
				VectorScale (forward, 600, forward);
				VectorCopy (cl.simorg, org);
				org[2] += 16;
				VectorAdd (org, forward, b->end);

				trace = PM_TraceLine (org, b->end);
				if (trace.fraction < 1)
					VectorCopy (trace.endpos, b->end);
			}
		}

		// calculate pitch and yaw
		VectorSubtract (b->end, b->start, dist);

		if (dist[1] == 0 && dist[0] == 0) {
			yaw = 0;
			pitch = (dist[2] > 0) ? 90 : 270;
		} else {
			yaw = atan2(dist[1], dist[0]) * 180 / M_PI;
			if (yaw < 0)
				yaw += 360;
	
			forward = sqrt (dist[0]*dist[0] + dist[1]*dist[1]);
			pitch = atan2(dist[2], forward) * 180 / M_PI;
			if (pitch < 0)
				pitch += 360;
		}

		// add new entities for the lightning
		VectorCopy (b->start, org);
#ifdef GLQUAKE
		for (k=0;k<beamstodraw;k++)
			VectorCopy (b->start, beamstart[k]);
#endif
		d = VectorNormalize(dist);
		VectorScale (dist, 30, dist);

		for ( ; d > 0; d -= 30) 
		{
#ifdef GLQUAKE
			if (!amf_lightning.value || Rulesets_RestrictParticles())
			{
#endif
				VectorCopy (org, ent.origin);
				ent.model = b->model;
				ent.angles[0] = pitch;
				ent.angles[1] = yaw;
				ent.angles[2] = rand() % 360;
				if (!Rulesets_RestrictParticles())
					ent.alpha = r_shaftalpha.value;

				CL_AddEntity (&ent);
#ifdef GLQUAKE
			}
			else
			{
				if (!ISPAUSED)
				{
					//VULT - Some people might like their lightning beams thicker
					for (k=0;k<beamstodraw;k++)
					{

						VectorAdd (org, dist, beamend[k]);
						for (j=0;j<3;j++)
//						beamend[k][j]+=(rand()%40)-20;
//						beamend[k][j]+=(rand()%((int)lg_size*2))-(int)lg_size; // can be rendered this way too, seems more or less equal
						beamend[k][j]+=f_rnd(-lg_size, lg_size);
						VX_LightningBeam (beamstart[k], beamend[k]);
						VectorCopy (beamend[k], beamstart[k]);
					}
				}
			}
			//VULT LIGHTNING
			//The infamous d-light glow has been replaced with a simple corona so it doesn't light up the room anymore
			if (amf_coronas.value && amf_lightning.value && !ISPAUSED)
			{
				if (b->entity == cl.viewplayernum + 1 && !((cls.demoplayback || cl.spectator) && amf_camera_chase.value))
					NewCorona (C_SMALLLIGHTNING, org);
				else
					NewCorona (C_LIGHTNING, org);
			}
#endif
			VectorAdd (org, dist, org);
#ifdef GLQUAKE
			//VULT LIGHTNING SPARKS
			if (amf_lightning_sparks.value && (cls.demoplayback || cl.spectator) && !sparks && !ISPAUSED)
			{
				trace_t	trace;
				trace = PM_TraceLine (org, b->end);
				if (trace.fraction < 1)
				{
					byte col[3] = {60,100,240};
					SparkGen (trace.endpos, col, (int)(3*amf_lightning_sparks.value), 300, 0.25);
					sparks = true;
				}
			}
#endif
		}
	}	
}

void CL_UpdateExplosions (void) {
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

void CL_UpdateTEnts (void) {
	CL_UpdateBeams ();
	CL_UpdateExplosions ();
#ifdef GLQUAKE
	//VULT MOTION TRAILS
	if (amf_motiontrails.value || MotionBlurCount) //dont stop removing trails if we have 'em
		CL_UpdateBlurs();
#endif

}

