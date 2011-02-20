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
typedef struct 
{
	int entity;
	model_t *model;
	float endtime;
	vec3_t start, end;
} beam_t;

beam_t cl_beams[MAX_BEAMS];

static vec3_t playerbeam_end;
static qbool playerbeam_update;

#define MAX_EXPLOSIONS 32
typedef struct explosion_s 
{
	struct explosion_s *prev, *next;
	vec3_t origin;
	float start;
	model_t *model;
} explosion_t;


// cl_free_explosions = linear linked list of free explosions
explosion_t	cl_explosions[MAX_EXPLOSIONS];
explosion_t cl_explosions_headnode, *cl_free_explosions;

static model_t	*cl_explo_mod, *cl_bolt1_mod, *cl_bolt2_mod, *cl_bolt3_mod, *cl_beam_mod;

sfx_t	*cl_sfx_wizhit, *cl_sfx_knighthit, *cl_sfx_tink1, *cl_sfx_ric1, *cl_sfx_ric2, *cl_sfx_ric3, *cl_sfx_r_exp3;

cvar_t r_lgblood = {"r_lgbloodColor", "225"};
cvar_t r_shiftbeam = {"r_shiftbeam", "0"};

void CL_InitTEnts (void)
{
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

void CL_ClearTEnts(void) 
{
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

static void CL_ParseBeam(int type) 
{
	int ent, i;
	vec3_t start, end;
	beam_t *b;
	struct model_s *m;

	ent = MSG_ReadShort();

	start[0] = MSG_ReadCoord();
	start[1] = MSG_ReadCoord();
	start[2] = MSG_ReadCoord();

	end[0] = MSG_ReadCoord();
	end[1] = MSG_ReadCoord();
	end[2] = MSG_ReadCoord();

	if (cls.demoseeking)
		return;

    // an experimental protocol extension:
    // TE_LIGHTNING1 with entity num in -512..-1 range is a rail trail
    if (type == 1 && (ent >= -512 && ent <= -1)) 
	{
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

		//color is ignored by most of trails
		switch(r_instagibtrail.integer)
		{
			case 1:
			R_ParticleTrail(start, end, &start, GRENADE_TRAIL);
			break;

			case 2:
			R_ParticleTrail(start, end, &start, ROCKET_TRAIL);
			break;

			case 3:
			R_ParticleTrail(start, end, &start, ALT_ROCKET_TRAIL);
			break;

			case 4:
			R_ParticleTrail(start, end, &start, BLOOD_TRAIL);
			break;

			case 5:
			R_ParticleTrail(start, end, &start, BIG_BLOOD_TRAIL);
			break;

			case 6:
			R_ParticleTrail(start, end, &start, TRACER1_TRAIL);
			break;

			case 7:
			R_ParticleTrail(start, end, &start, TRACER2_TRAIL);
			break;

			case 8:
			R_ParticleTrail(start, end, &start, VOOR_TRAIL);
			break;

			case 9:
			Classic_ParticleRailTrail(start, end, color);
			break;

#ifdef GLQUAKE
			case 10:
			R_ParticleTrail(start, end, &start, RAIL_TRAIL);
			break;

			case 11:
			QMB_ParticleRailTrail(start, end, color);
			break;

			case 12:
			R_ParticleTrail(start, end, &start, LAVA_TRAIL);
			break;

			case 13:
			R_ParticleTrail(start, end, &start, AMF_ROCKET_TRAIL);
			break;

#endif

			default: break;
		}

        return;
    }

	switch (type) 
	{
		case 1:
			if (!cl_bolt1_mod)
				cl_bolt1_mod = Mod_ForName("progs/bolt.mdl", true);
			m = cl_bolt1_mod;
			break;
		case 2:
			if (!cl_bolt2_mod)
				cl_bolt2_mod = Mod_ForName("progs/bolt2.mdl", true);
			m = cl_bolt2_mod;
			break;
		case 3:
			if (!cl_bolt3_mod)
				cl_bolt3_mod = Mod_ForName("progs/bolt3.mdl", true);
			m = cl_bolt3_mod;
			break;
		case 4: default:
			if (!cl_beam_mod)
				cl_beam_mod = Mod_ForName("progs/beam.mdl", true);
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
		cl_explo_mod = Mod_ForName("progs/s_explod.spr", true);
	ex->model = cl_explo_mod;
}

static void CL_Parse_TE_WIZSPIKE(vec3_t pos)
{
	#ifdef GLQUAKE
	if (amf_part_sparks.value && !Rulesets_RestrictParticles())
	{
		byte col[3] = {0, 124, 0};
		SparkGen(pos, col, 20, 90, 1);
	}
	else
	#endif // GLQUAKE
	{
		R_RunParticleEffect(pos, vec3_origin, 20, 30);
	}
	S_StartSound(-1, 0, cl_sfx_wizhit, pos, 1, 1);
}

// FIXME: D-Kure: This seems to be unused
#if 0
static void CL_Parse_TE_KNIGHTSPIKE(vec3_t pos)
{
	#ifdef GLQUAKE
	if (amf_part_sparks.value && !Rulesets_RestrictParticles())
	{
		byte col[3] = {255, 77, 0};
		SparkGen(pos, col, 20, 150, 1);
	}
	else
	#endif // GLQUAKE
	{
		R_RunParticleEffect(pos, vec3_origin, 226, 20);
	}

	S_StartSound(-1, 0, cl_sfx_knighthit, pos, 1, 1);
}
#endif

static void CL_Parse_TE_SPIKE(vec3_t pos)
{
	int rnd;

	#ifdef GLQUAKE
	if (amf_part_spikes.value && !Rulesets_RestrictParticles() && !gl_no24bit.integer)
		VXNailhit(pos, 10 * amf_part_spikes.value);
	else
	#endif // GLQUAKE
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

	#ifdef GLQUAKE
	if (amf_part_spikes.value && !Rulesets_RestrictParticles() && !gl_no24bit.integer)
		VXNailhit(pos, 20 * amf_part_spikes.value);
	else
	#endif // GLQUAKE
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

	if (cls.state != ca_active)
		return;

	if (r_explosiontype.value == 2) 
	{
		#ifdef GLQUAKE
		if (amf_part_teleport.value)
			VXTeleport(pos);
		else
		#endif // GLQUAKE
		{
			R_TeleportSplash(pos); // Teleport splash
		}
		
		#ifdef GLQUAKE
		if (amf_coronas_tele.value)
			NewCorona(C_BLUEFLASH, pos);
		#endif // GLQUAKE
	}
	else if (r_explosiontype.value == 3) 
	{	
		R_RunParticleEffect(pos, vec3_origin, 225, 50);		//lightning blood
	}
	else if (r_explosiontype.value == 4) 
	{	
		R_RunParticleEffect(pos, vec3_origin, 73, 20 * 32); // Big blood
	}
	else if (r_explosiontype.value == 5) 
	{
		R_RunParticleEffect(pos, vec3_origin, 0, 20 * 14);	// Double gunshot
	}
	else if (r_explosiontype.value == 6) 
	{
		#ifdef GLQUAKE
		if (amf_part_blobexplosion.value)
		{
			VXBlobExplosion(pos);
		}
		else
		#endif // GLQUAKE
		{
			R_BlobExplosion(pos); // Blob explosion
		}
		#ifdef GLQUAKE
		if (amf_coronas.value)
			NewCorona(C_BLUEFLASH, pos);
		#endif // GLQUAKE
	}
	#ifdef GLQUAKE
	else if (r_explosiontype.value == 7 && qmb_initialized && gl_part_explosions.value) 
	{
		QMB_DetpackExplosion(pos);	// Detpack explosion
	}
	else if (r_explosiontype.value == 8 && qmb_initialized)
	{
		FuelRodExplosion(pos);
	}
	#endif // GLQUAKE
	else if (r_explosiontype.value == 10)
	{ /* Explosions turned off */ }
	else
	{	
		// sprite and particles
		#ifdef GLQUAKE
		if (amf_part_explosion.value)
			VXExplosion(pos);
		else
		#endif // GLQUAKE
		{
			R_ParticleExplosion(pos); // Normal explosion
		}
	}

	if (r_explosionlight.value)	
	{
		dl = CL_AllocDlight(0);
		VectorCopy(pos, dl->origin);
		dl->radius = 150 + 200 * bound(0, r_explosionlight.value, 1);
		dl->die = cl.time + 0.5;
		dl->decay = 300;
		#ifdef GLQUAKE
		if (r_explosiontype.value == 8)
		{
			dl->type = lt_green;
		}
		else
		#endif // GLQUAKE
		{
			customlight_t cst_lt = {0};
			dlightColorEx(r_explosionlightcolor.value, r_explosionlightcolor.string, lt_explosion, true, &cst_lt);
			dl->type = cst_lt.type;
			
			#ifdef GLQUAKE
			if (dl->type == lt_custom)
				VectorCopy(cst_lt.color, dl->color);
			#endif // GLQUAKE
		}
		#ifdef GLQUAKE
		if (amf_coronas.value && r_explosiontype.value != 7 && r_explosiontype.value != 2 && r_explosiontype.value != 8)
			NewCorona(C_FLASH, pos);
		#endif // GLQUAKE
	}

	S_StartSound(-1, 0, cl_sfx_r_exp3, pos, 1, 1);
}

static void CL_Parse_TE_TAREXPLOSION(vec3_t pos)
{
	#ifdef GLQUAKE
	if (amf_part_blobexplosion.value)
	{
		VXBlobExplosion(pos);
	}
	else
	#endif // GLQUAKE
	{
		R_BlobExplosion(pos); // Blob explosion
	}
	
	#ifdef GLQUAKE
	if (amf_coronas.value)
		NewCorona (C_BLUEFLASH, pos);
	#endif // GLQUAKE

	S_StartSound(-1, 0, cl_sfx_r_exp3, pos, 1, 1);
}

static void CL_Parse_TE_TELEPORT(vec3_t pos)
{
	if (r_telesplash.value)
	{
		#ifdef GLQUAKE
		if (amf_part_teleport.value)
			VXTeleport(pos);
		else
		#endif // GLQUAKE
		{
			R_TeleportSplash(pos); // Teleport splash
		}
		#ifdef GLQUAKE
		if (amf_coronas_tele.value)
			NewCorona(C_BLUEFLASH, pos);
		#endif // GLQUAKE
	}
}

static void CL_Parse_TE_GUNSHOT(void)
{
	int count;
	vec3_t pos;

	if (cls.nqdemoplayback)
		count = 1;
	else
		count = MSG_ReadByte();

	pos[0] = MSG_ReadCoord();
	pos[1] = MSG_ReadCoord();
	pos[2] = MSG_ReadCoord();

	if (cls.demoseeking)
		return;
	
	#ifdef GLQUAKE
	if (amf_part_gunshot.value && !Rulesets_RestrictParticles())
		VXGunshot(pos, 5 * count * amf_part_gunshot.value);
	else
	#endif // GLQUAKE
	{
		R_RunParticleEffect(pos, vec3_origin, 256 /* magic! */, 20 * count);
	}
}

static void CL_Parse_TE_BLOOD(void)
{
	int count;
	vec3_t pos;
	dlight_t *dl;
	
	if (cls.nqdemoplayback)
	{
		// NQ_TE_EXPLOSION2
		int colorStart, colorLength;
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();
		colorStart = MSG_ReadByte();
		colorLength = MSG_ReadByte();

		if (cls.demoseeking)
			return;

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

	if (cls.demoseeking)
		return;

	#ifdef GLQUAKE
	if (amf_part_blood.value)
		VXBlood(pos, 5 * count * amf_part_blood.value);
	else
	#endif // GLQUAKE
	{
		R_RunParticleEffect(pos, vec3_origin, 73, 20 * count);
	}
}

static void CL_Parse_TE_LIGHTNINGBLOOD(void)
{
	vec3_t pos;

	if (cls.nqdemoplayback) 
	{
		// NQ_TE_BEAM - grappling hook beam
		CL_ParseBeam(4);
	}
	else
	{
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();

		if (cls.demoseeking)
			return;
#ifdef GLQUAKE
		R_RunParticleEffect(pos, vec3_origin, gl_part_blood.value?225:r_lgblood.value, 50); // 225 default
#else
		R_RunParticleEffect(pos, vec3_origin, r_lgblood.value, 50); // 225 default
#endif //GLQUAKE
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
			CL_ParseBeam(1);
			parsed = true;
			break;

		case TE_LIGHTNING2:
			CL_ParseBeam(2);
			parsed = true;
			break;

		case TE_LIGHTNING3:
			CL_ParseBeam(3);
			parsed = true;
			break;

		// Bullet hitting wall.
		case TE_GUNSHOT:
			CL_Parse_TE_GUNSHOT();
			parsed = true;
			break;

		// Bullets hitting body.
		case TE_BLOOD: 
			CL_Parse_TE_BLOOD();
			parsed = true;
			break;

		// Lightning hitting body
		case TE_LIGHTNINGBLOOD: 
			CL_Parse_TE_LIGHTNINGBLOOD();
			parsed = true;
			break;
	}

	if (!parsed)
	{
		// The rest only needs position.
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();

		if (cls.demoseeking)
			return;
		
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
				Host_Error("CL_ParseTEnt: bad type");
		}
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
	char *p;
	if (cls.demoplayback || cl.spectator)
		return 1;
	else
		return *(p = Info_ValueForKey(cl.serverinfo, "fakeshaft")) ?
			bound(0, Q_atof(p), 1) :
			*(p = Info_ValueForKey(cl.serverinfo, "truelightning")) ?
			bound(0, Q_atof(p), 1) : 1;
}

void CL_UpdateBeams(void) 
{
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
	beamstodraw = bound(1, amf_lightning.value, MAX_LIGHTNINGBEAMS);	
	#endif // GLQUAKE

	memset (&ent, 0, sizeof(entity_t));
	ent.colormap = vid.colormap;

	fakeshaft = bound(0, cl_fakeshaft.value, fakeshaft_policy());

	// Update lightning.
	for (i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++)	
	{
		if (!b->model || b->endtime < cl.time)
			continue;

		// if coming from the player, update the start position
		if (b->entity == cl.viewplayernum + 1) 
		{
			VectorCopy (cl.simorg, b->start);
			b->start[2] += cl.crouch + bound(-7, v_viewheight.value, 4);
			VectorMA(b->start, r_shiftbeam.value, vright, b->start);

			if (fakeshaft && (cl_fakeshaft_extra_updates.value || playerbeam_update))
			{
				vec3_t	forward, v, org, ang, target_angles, target_origin;
				float	delta;
				trace_t	trace;

				playerbeam_update = false;

				if (cl_fakeshaft.value == 2 && !cl.spectator) { // try to simulate 13 ms ping
					frame_t *frame = &cl.frames[(cls.netchan.outgoing_sequence - 2) & UPDATE_MASK];
					VectorCopy(frame->cmd.angles, target_angles);
					VectorCopy(frame->playerstate[cl.viewplayernum].origin, target_origin);
				} else {
					VectorCopy(cl.simangles, target_angles);
					VectorCopy(cl.simorg, target_origin);
				}

				VectorSubtract (playerbeam_end, target_origin, v);
				v[2] -= 22;	// Adjust for view height.
				vectoangles (v, ang);

				// Lerp pitch.
				ang[0] = -ang[0];
				if (ang[0] < -180)
					ang[0] += 360;
				ang[0] += (target_angles[0] - ang[0]) * fakeshaft;

				// Lerp yaw.
				delta = target_angles[1] - ang[1];
				if (delta > 180)
					delta -= 360;
				if (delta < -180)
					delta += 360;
				ang[1] += delta * fakeshaft;
				ang[2] = 0;

				AngleVectors (ang, forward, NULL, NULL);
				VectorScale (forward, 600, forward);
				VectorCopy (target_origin, org);
				org[2] += 16;
				VectorAdd (org, forward, b->end);

				trace = PM_TraceLine (org, b->end);
				if (trace.fraction < 1)
					VectorCopy (trace.endpos, b->end);
			}
		}

		// Calculate pitch and yaw.
		VectorSubtract (b->end, b->start, dist);

		if (dist[1] == 0 && dist[0] == 0) 
		{
			yaw = 0;
			pitch = (dist[2] > 0) ? 90 : 270;
		}
		else 
		{
			yaw = atan2(dist[1], dist[0]) * 180 / M_PI;
			if (yaw < 0)
				yaw += 360;
	
			forward = sqrt (dist[0]*dist[0] + dist[1]*dist[1]);
			pitch = atan2(dist[2], forward) * 180 / M_PI;
			if (pitch < 0)
				pitch += 360;
		}

		// Add new entities for the lightning.
		VectorCopy (b->start, org);
		
		#ifdef GLQUAKE
		for (k = 0; k < beamstodraw; k++)
			VectorCopy (b->start, beamstart[k]);
		#endif // GLQUAKE
		
		d = VectorNormalize(dist);
		VectorScale (dist, 30, dist);

		for ( ; d > 0; d -= 30) 
		{
#ifdef GLQUAKE
			if (!amf_lightning.value || Rulesets_RestrictParticles())
			{
#endif // GLQUAKE
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
						for (j = 0; j < 3; j++)
							beamend[k][j] += f_rnd(-lg_size, lg_size);
						VX_LightningBeam (beamstart[k], beamend[k]);
						VectorCopy (beamend[k], beamstart[k]);
					}
				}
			}

			// The infamous d-light glow has been replaced with a simple corona so it doesn't light up the room anymore.
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
			// LIGHTNING SPARKS
			if (amf_lightning_sparks.value && !Rulesets_RestrictParticles() && !sparks && !ISPAUSED && !gl_no24bit.integer)
			{
				trace_t	trace;
				trace = PM_TraceLine (org, b->end);
				if (trace.fraction < 1)
				{
					//byte col[3] = {60,100,240};
					SparkGen (trace.endpos, amf_lightning_color.color, (int)(3*amf_lightning_sparks.value), amf_lightning_sparks_size.value, 0.25);
					sparks = true;
				}
			}
#endif // GLQUAKE
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

