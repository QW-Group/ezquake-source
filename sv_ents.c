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

	$Id: sv_ents.c,v 1.6 2007-03-10 14:11:08 disconn3ct Exp $
*/

#include "qwsvdef.h"
#include "pmove.h"

int SV_PMTypeForClient (client_t *cl);

//=============================================================================

// because there can be a lot of nails, there is a special
// network protocol for them
#define	MAX_NAILS 32
edict_t	*nails[MAX_NAILS];
int numnails;

extern	int	sv_nailmodel, sv_supernailmodel, sv_playermodel;

cvar_t	sv_nailhack	= {"sv_nailhack", "1"};

static qbool SV_AddNailUpdate (edict_t *ent)
{
	if (sv_nailhack.value)
		return false;

	if (ent->v.modelindex != sv_nailmodel
		&& ent->v.modelindex != sv_supernailmodel)
		return false;
	if (numnails == MAX_NAILS)
		return true;
	nails[numnails] = ent;
	numnails++;
	return true;
}

static void SV_EmitNailUpdate (sizebuf_t *msg)
{
	byte bits[6];
	int n, i, x, y, z, pitch, yaw;
	edict_t	*ent;

	if (!numnails)
		return;

	MSG_WriteByte (msg, svc_nails);
	MSG_WriteByte (msg, numnails);

	for (n = 0; n < numnails; n++) {
		ent = nails[n];
		x = ((int) (ent->v.origin[0] + 4096 + 1) >> 1) & 4095;
		y = ((int) (ent->v.origin[1] + 4096 + 1) >> 1) & 4095;
		z = ((int) (ent->v.origin[2] + 4096 + 1) >> 1) & 4095;
		pitch = Q_rint(ent->v.angles[0]*(16.0/360.0)) & 15;
		yaw = Q_rint(ent->v.angles[1]*(256.0/360.0)) & 255;

		bits[0] = x;
		bits[1] = (x >> 8) | (y << 4);
		bits[2] = (y >> 4);
		bits[3] = z;
		bits[4] = (z >> 8) | (pitch << 4);
		bits[5] = yaw;

		for (i = 0; i < 6; i++)
			MSG_WriteByte (msg, bits[i]);
	}
}

//=============================================================================

//
// returns translated entity number for sending to client
//
int SV_TranslateEntnum (int num)
{
	int	i;
	double	besttime, trivial_accept;
	entity_translation_t *trans, *best;

	assert (num >= 0 && num < SV_MAX_EDICTS);

	if (num <= MAX_CLIENTS) // client entitites are never translated
		return num;

	if (sv.entmap[num]) {
		// see if the previous translation is still valid
		trans = &sv.translations[sv.entmap[num]];
		if (trans->original == num) {
			// translation is still valid
			trans->lastused = svs.realtime;
			return sv.entmap[num];
		}
	}

	trivial_accept = svs.realtime - 10;	// anything older than that will do

	if (num < 512) {
		// whenever possible, try to use the original number as translation
		trans = &sv.translations[num];
		if (!trans->lastused || trans->lastused < trivial_accept) {
			// good, we can use it
			trans->original = num;
			trans->lastused = svs.realtime;
			sv.entmap[num] = num;
			return num;
		}
	}

	// find a new translation slot
	besttime = svs.realtime;
	best = NULL;

	for (i = MAX_CLIENTS + 1; i < 512; i++) {
		if (sv.edicts[i].baseline.modelindex)
			continue; // never use slots with baselines

		trans = &sv.translations[i];
		if (!trans->lastused || trans->lastused < trivial_accept) {
			best = trans;
			break;
		}

		if (trans->lastused < besttime) {
			besttime = trans->lastused;
			best = trans;
		}
	}

	if (!best)
		Host_Error ("SV_TranslateEntnum: no free translation slots");

	best->lastused = svs.realtime;
	best->original = num;
	sv.entmap[num] = best - sv.translations;

	return sv.entmap[num];
}

//=============================================================================

/*
=============
SV_WritePlayersToClient

=============
*/
static void SV_WritePlayersToClient (client_t *client, byte *pvs, sizebuf_t *msg)
{
	int i, j, msec, pflags, pm_type = 0, pm_code = 0;
	client_t *cl;
	edict_t *ent;
	usercmd_t cmd;

	for (j = 0, cl = svs.clients; j < MAX_CLIENTS; j++, cl++) {
		if (cl->state != cs_spawned)
			continue;

		ent = cl->edict;

		// ZOID visibility tracking
		if (cl != client && !(client->spec_track && client->spec_track - 1 == j)) {
			if (cl->spectator)
				continue;

			// ignore if not touching a PV leaf
			for (i = 0; i < ent->num_leafs; i++)
				if (pvs[ent->leafnums[i] >> 3] & (1 << (ent->leafnums[i] & 7)))
					break;
			if (i == ent->num_leafs)
				continue; // not visible
		}

		pflags = PF_MSEC | PF_COMMAND;

		if (ent->v.modelindex != sv_playermodel)
			pflags |= PF_MODEL;
		for (i = 0; i < 3; i++)
			if (ent->v.velocity[i])
				pflags |= (PF_VELOCITY1 << i);
		if (ent->v.effects)
			pflags |= PF_EFFECTS;
		if (ent->v.skin)
			pflags |= PF_SKINNUM;
		if (ent->v.health <= 0)
			pflags |= PF_DEAD;
		if (ent->v.mins[2] != -24)
			pflags |= PF_GIB;

		if (cl->spectator) {
			// only send origin and velocity to spectators
			pflags &= PF_VELOCITY1 | PF_VELOCITY2 | PF_VELOCITY3;
		} else if (cl == client) {
			// don't send a lot of data on personal entity
			pflags &= ~(PF_MSEC|PF_COMMAND);
			if (ent->v.weaponframe)
				pflags |= PF_WEAPONFRAME;
		}

		// Z_EXT_PM_TYPE protocol extension
		// encode pm_type and jump_held into pm_code
		pm_type = SV_PMTypeForClient (cl);
		switch (pm_type) {
		case PM_DEAD:
			pm_code = PMC_NORMAL;	// plus PF_DEAD
			break;
		case PM_NORMAL:
			pm_code = PMC_NORMAL;
			if (cl->jump_held)
				pm_code = PMC_NORMAL_JUMP_HELD;
			break;
		case PM_OLD_SPECTATOR:
			pm_code = PMC_OLD_SPECTATOR;
			break;
		case PM_SPECTATOR:
			pm_code = PMC_SPECTATOR;
			break;
		case PM_FLY:
			pm_code = PMC_FLY;
			break;
		case PM_NONE:
			pm_code = PMC_NONE;
			break;
		case PM_FREEZE:
			pm_code = PMC_FREEZE;
			break;
		default:
			assert (false);
		}
		pflags |= pm_code << PF_PMC_SHIFT;

		// Z_EXT_PF_ONGROUND protocol extension
		if ((int)ent->v.flags & FL_ONGROUND)
			pflags |= PF_ONGROUND;

		if (client->spec_track && client->spec_track - 1 == j && ent->v.weaponframe) 
			pflags |= PF_WEAPONFRAME;

		MSG_WriteByte (msg, svc_playerinfo);
		MSG_WriteByte (msg, j);
		MSG_WriteShort (msg, pflags);

		for (i = 0; i < 3; i++)
			MSG_WriteCoord (msg, ent->v.origin[i]);

		MSG_WriteByte (msg, ent->v.frame);

		if (pflags & PF_MSEC) {
			msec = 1000*(svs.realtime - cl->cmdtime);
			if (msec > 255)
				msec = 255;
			MSG_WriteByte (msg, msec);
		}

		if (pflags & PF_COMMAND) {
			cmd = cl->lastcmd;

			// don't show the corpse looking around...
			if (ent->v.health <= 0) {
				cmd.angles[0] = 0;
				cmd.angles[1] = ent->v.angles[1];
				cmd.angles[0] = 0;
			}

			cmd.buttons = 0; // never send buttons
			cmd.impulse = 0; // never send impulses

			MSG_WriteDeltaUsercmd (msg, &nullcmd, &cmd);
		}

		for (i = 0; i < 3; i++)
			if (pflags & (PF_VELOCITY1 << i))
				MSG_WriteShort (msg, ent->v.velocity[i]);

		if (pflags & PF_MODEL)
			MSG_WriteByte (msg, ent->v.modelindex);

		if (pflags & PF_SKINNUM)
			MSG_WriteByte (msg, ent->v.skin);

		if (pflags & PF_EFFECTS)
			MSG_WriteByte (msg, ent->v.effects);

		if (pflags & PF_WEAPONFRAME)
			MSG_WriteByte (msg, ent->v.weaponframe);
	}
}

// passed to qsort
static int EntityState_Compare(const void *p1, const void *p2) {
	return ((entity_state_t *) p1)->number - ((entity_state_t *) p2)->number;
}

// we pass it to MSG_EmitPacketEntities
static entity_state_t *SV_GetBaseline (int number) { 
	return &EDICT_NUM(number)->baseline; 
} 

#define offsetrandom(MIN,MAX) ((rand() & 32767) * (((MAX)-(MIN)) * (1.0f / 32767.0f)) + (MIN))

extern cvar_t sv_cullentities;
extern trace_t SV_ClipMoveToEntity (edict_t *ent, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end);

qbool SV_InvisibleToClient(edict_t *viewer, edict_t *seen)
{
    int		i;
    trace_t	tr;
    vec3_t	start;
    vec3_t	end;

    if (seen->v.movetype == MOVETYPE_PUSH )//dont cull doors and plats :(
    {
        return false;
    }

    if (sv_cullentities.value == 1)    //1 only check player models, 2 = check all ents
    if (strcmp(pr_strings + seen->v.classname, "player"))    
        return false;

    memset (&tr, 0, sizeof(tr));                
    tr.fraction = 1;

    start[0] = viewer->v.origin[0];
    start[1] = viewer->v.origin[1];
    start[2] = viewer->v.origin[2] + viewer->v.view_ofs[2];

    //aim straight at the center of "seen" from our eyes
    end[0] = 0.5 * (seen->v.mins[0] + seen->v.maxs[0]);
    end[1] = 0.5 * (seen->v.mins[1] + seen->v.maxs[1]);
    end[2] = 0.5 * (seen->v.mins[2] + seen->v.maxs[2]);            

    tr = SV_ClipMoveToEntity (sv.edicts, start, vec3_origin, vec3_origin, end);
    if (tr.fraction == 1)// line hit the ent
            return false;

    //last attempt to eliminate any flaws...
    if ((!strcmp(pr_strings + seen->v.classname, "player")) || (sv_cullentities.value > 1))
    {
        for (i = 0; i < 64; i++)
        {
            end[0] = seen->v.origin[0] + offsetrandom(seen->v.mins[0], seen->v.maxs[0]);
            end[1] = seen->v.origin[1] + offsetrandom(seen->v.mins[1], seen->v.maxs[1]);
            end[2] = seen->v.origin[2] + offsetrandom(seen->v.mins[2], seen->v.maxs[2]);

            tr = SV_ClipMoveToEntity (sv.edicts, start, vec3_origin, vec3_origin, end);
            if (tr.fraction == 1)// line hit the ent
			{
				    Com_DPrintf (va("found ent in %i hits\n", i));
                    return false;
			}
        }
    }

    return true;
}

/*
=============
SV_WriteEntitiesToClient

Encodes the current state of the world as
a svc_packetentities messages and possibly
a svc_nails message and
svc_playerinfo messages
=============
*/
void SV_WriteEntitiesToClient (client_t *client, sizebuf_t *msg)
{
	int e, i;
	byte *pvs;
	vec3_t org;
	edict_t	*ent;
	edict_t	*clent;
	client_frame_t *frame;
	entity_state_t *state;
	packet_entities_t *pack;

	// this is the frame we are creating
	frame = &client->frames[client->netchan.incoming_sequence & UPDATE_MASK];

	clent = client->edict;

	if (sv.intermission_running && sv.intermission_origin_valid) {
		pvs = CM_FatPVS (sv.intermission_origin);
	} else {
		// find the client's PVS
		VectorAdd (clent->v.origin, clent->v.view_ofs, org);
		pvs = CM_FatPVS (org);
	}

	// send over the players in the PVS
	SV_WritePlayersToClient(client, pvs, msg);
	
	// put other visible entities into either a packet_entities or a nails message
	pack = &frame->entities;
	pack->num_entities = 0;

	numnails = 0;

	for (e = MAX_CLIENTS + 1, ent = EDICT_NUM(e); e < sv.num_edicts; e++, ent = NEXT_EDICT(ent)) {
		// ignore ents without visible models
		if (!ent->v.modelindex || !*PR_GetString(ent->v.model))
			continue;

		// ignore if not touching a PV leaf
		for (i=0 ; i < ent->num_leafs ; i++)
			if (pvs[ent->leafnums[i] >> 3] & (1 << (ent->leafnums[i]&7) ))
				break;
			
		if (i == ent->num_leafs)
			continue; // not visible
			
		if (sv_cullentities.value && SV_InvisibleToClient(clent, ent))
			continue;

		if (SV_AddNailUpdate(ent))
			continue; // added to the special update list

		// add to the packetentities
		if (pack->num_entities == MAX_PACKET_ENTITIES)
			continue; // all full

		state = &pack->entities[pack->num_entities];
		pack->num_entities++;

		state->number = SV_TranslateEntnum(e);
		state->flags = 0;
		VectorCopy(ent->v.origin, state->origin);
		VectorCopy(ent->v.angles, state->angles);
		state->modelindex = ent->v.modelindex;
		state->frame = ent->v.frame;
		state->colormap = ent->v.colormap;
		state->skinnum = ent->v.skin;
		state->effects = ent->v.effects;
	}

	// entity translation might have broken original entnum order so make sure entity states are sorted
	qsort (pack->entities, pack->num_entities, sizeof(pack->entities[0]), EntityState_Compare);

	if (client->delta_sequence != -1) {
		// encode the packet entities as a delta from the
		// last packetentities acknowledged by the client
		MSG_EmitPacketEntities(&client->frames[client->delta_sequence & UPDATE_MASK].entities,
			client->delta_sequence, pack, msg, SV_GetBaseline);
	} else {
		// no delta
		MSG_EmitPacketEntities(NULL, 0, pack, msg, SV_GetBaseline);
	}

	// now add the specialized nail update
	SV_EmitNailUpdate(msg);
}
