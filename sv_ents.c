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

#include "qwsvdef.h"
#include "pmove.h"

int SV_PMTypeForClient (client_t *cl);

//The PVS must include a small area around the client to allow head bobbing
//or other small motion on the client side.  Otherwise, a bob might cause an
//entity that should be visible to not show up, especially when the bob crosses a waterline.

int		fatbytes;
byte	fatpvs[MAX_MAP_LEAFS/8];

void SV_AddToFatPVS (vec3_t org, mnode_t *node) {
	int i;
	byte *pvs;
	mplane_t *plane;
	float d;

	while (1) {
		// if this is a leaf, accumulate the pvs bits
		if (node->contents < 0) {
			if (node->contents != CONTENTS_SOLID) {
				pvs = Mod_LeafPVS ( (mleaf_t *)node, sv.worldmodel);
				for (i = 0; i < fatbytes; i++)
					fatpvs[i] |= pvs[i];
			}
			return;
		}

		plane = node->plane;
		d = PlaneDiff (org, plane);
		if (d > 8) {
			node = node->children[0];
		} else if (d < -8) {
			node = node->children[1];
		} else {	// go down both
			SV_AddToFatPVS (org, node->children[0]);
			node = node->children[1];
		}
	}
}

//Calculates a PVS that is the inclusive or of all leafs within 8 pixels of the given point.
byte *SV_FatPVS (vec3_t org) {
	fatbytes = (sv.worldmodel->numleafs + 31) >> 3;
	memset (fatpvs, 0, fatbytes);
	SV_AddToFatPVS (org, sv.worldmodel->nodes);
	return fatpvs;
}


// because there can be a lot of nails, there is a special
// network protocol for them
#define	MAX_NAILS	32
edict_t	*nails[MAX_NAILS];
int		numnails;

extern	int	sv_nailmodel, sv_supernailmodel, sv_playermodel;

#ifdef SERVERONLY
cvar_t	sv_nailhack	= {"sv_nailhack", "0"};
#else
cvar_t	sv_nailhack	= {"sv_nailhack", "1"};
#endif

qboolean SV_AddNailUpdate (edict_t *ent) {
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

void SV_EmitNailUpdate (sizebuf_t *msg) {
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

void SV_WritePlayersToClient (client_t *client, byte *pvs, sizebuf_t *msg) {
	int i, j, msec, pflags;
	client_t *cl;
	edict_t *ent;
	usercmd_t cmd;
	int pm_type, pm_code;

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
				continue;		// not visible
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

		pm_type = SV_PMTypeForClient (cl);
		switch (pm_type) {
		case PM_NORMAL:		// Z_EXT_PM_TYPE protocol extension
			if (cl->jump_held)
				pm_code = PMC_NORMAL_JUMP_HELD;	// encode pm_type and jump_held into pm_code
			else
				pm_code = PMC_NORMAL;
			break;
		case PM_OLD_SPECTATOR:
			pm_code = PMC_OLD_SPECTATOR;
			break;
		case PM_SPECTATOR:	// Z_EXT_PM_TYPE_NEW protocol extension
			pm_code = PMC_SPECTATOR;
			break;
		case PM_FLY:
			pm_code = PMC_FLY;
			break;
		case PM_DEAD:
			pm_code = PMC_NORMAL;
			break;
		default:
			assert(!"SV_WritePlayersToClient: unexpected pm_type");
		}

		pflags |= pm_code << PF_PMC_SHIFT;

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

			if (ent->v.health <= 0) {	// don't show the corpse looking around...
				cmd.angles[0] = 0;
				cmd.angles[1] = ent->v.angles[1];
				cmd.angles[0] = 0;
			}

			cmd.buttons = 0;	// never send buttons
			cmd.impulse = 0;	// never send impulses

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

// we pass it to MSG_EmitPacketEntities
static entity_state_t *SV_GetBaseline (int number) { 
	return &EDICT_NUM(number)->baseline; 
} 

//Encodes the current state of the world as a svc_packetentities messages and possibly
//a svc_nails message and svc_playerinfo messages
void SV_WriteEntitiesToClient (client_t *client, sizebuf_t *msg) {
	int e, i, max_edicts;
	byte *pvs;
	vec3_t org;
	edict_t	*ent;
	packet_entities_t *pack;
	edict_t	*clent;
	client_frame_t *frame;
	entity_state_t *state;

	// this is the frame we are creating
	frame = &client->frames[client->netchan.incoming_sequence & UPDATE_MASK];

	// find the client's PVS
	clent = client->edict;
	VectorAdd (clent->v.origin, clent->v.view_ofs, org);
	pvs = SV_FatPVS (org);

	// send over the players in the PVS
	SV_WritePlayersToClient (client, pvs, msg);
	
	// put other visible entities into either a packet_entities or a nails message
	pack = &frame->entities;
	pack->num_entities = 0;

	numnails = 0;

	// QW protocol can only handle 512 entities. Any entity with number >= 512 will be invisible
	max_edicts = min (sv.num_edicts, 512);

	for (e = MAX_CLIENTS + 1, ent = EDICT_NUM(e); e < max_edicts; e++, ent = NEXT_EDICT(ent)) {
		// ignore ents without visible models
		if (!ent->v.modelindex || !*PR_GetString(ent->v.model))
			continue;

		// ignore if not touching a PV leaf
		for (i = 0; i < ent->num_leafs; i++)
			if (pvs[ent->leafnums[i] >> 3] & (1 << (ent->leafnums[i]&7) ))
				break;
			
		if (i == ent->num_leafs)
			continue;		// not visible

		if (SV_AddNailUpdate (ent))
			continue;	// added to the special update list

		// add to the packetentities
		if (pack->num_entities == MAX_PACKET_ENTITIES)
			continue;	// all full

		state = &pack->entities[pack->num_entities];
		pack->num_entities++;

		state->number = e;
		state->flags = 0;
		VectorCopy (ent->v.origin, state->origin);
		VectorCopy (ent->v.angles, state->angles);
		state->modelindex = ent->v.modelindex;
		state->frame = ent->v.frame;
		state->colormap = ent->v.colormap;
		state->skinnum = ent->v.skin;
		state->effects = ent->v.effects;
	}

	if (client->delta_sequence != -1) {
		// encode the packet entities as a delta from the
		// last packetentities acknowledged by the client
		MSG_EmitPacketEntities (&client->frames[client->delta_sequence & UPDATE_MASK].entities,
			client->delta_sequence, pack, msg, SV_GetBaseline);
	} else {
		// no delta
		MSG_EmitPacketEntities (NULL, 0, pack, msg, SV_GetBaseline);
	}

	// now add the specialized nail update
	SV_EmitNailUpdate (msg);
}
