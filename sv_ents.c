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

	$Id: sv_ents.c 766 2008-02-28 13:57:25Z qqshka $
*/

#include "qwsvdef.h"


//=============================================================================

// because there can be a lot of nails, there is a special
// network protocol for them
#define MAX_NAILS 32
static edict_t *nails[MAX_NAILS];
static int numnails;
static int nailcount = 0;

extern	int sv_nailmodel, sv_supernailmodel, sv_playermodel;

cvar_t	sv_nailhack	= {"sv_nailhack", "1"};


static qbool SV_AddNailUpdate (edict_t *ent)
{
	if ((int)sv_nailhack.value)
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

static void SV_EmitNailUpdate (sizebuf_t *msg, qbool recorder)
{
	int x, y, z, p, yaw, n, i;
	byte bits[6]; // [48 bits] xyzpy 12 12 12 4 8
	edict_t *ent;


	if (!numnails)
		return;

	if (recorder)
		MSG_WriteByte (msg, svc_nails2);
	else
		MSG_WriteByte (msg, svc_nails);

	MSG_WriteByte (msg, numnails);

	for (n=0 ; n<numnails ; n++)
	{
		ent = nails[n];
		if (recorder)
		{
			if (!ent->v.colormap)
			{
				if (!((++nailcount)&255)) nailcount++;
				ent->v.colormap = nailcount&255;
			}

			MSG_WriteByte (msg, (byte)ent->v.colormap);
		}

		x = ((int)(ent->v.origin[0] + 4096 + 1) >> 1) & 4095;
		y = ((int)(ent->v.origin[1] + 4096 + 1) >> 1) & 4095;
		z = ((int)(ent->v.origin[2] + 4096 + 1) >> 1) & 4095;
		p = Q_rint(ent->v.angles[0]*(16.0/360.0)) & 15;
		yaw = Q_rint(ent->v.angles[1]*(256.0/360.0)) & 255;

		bits[0] = x;
		bits[1] = (x>>8) | (y<<4);
		bits[2] = (y>>4);
		bits[3] = z;
		bits[4] = (z>>8) | (p<<4);
		bits[5] = yaw;

		for (i=0 ; i<6 ; i++)
			MSG_WriteByte (msg, bits[i]);
	}
}

//=============================================================================


/*
==================
SV_WriteDelta

Writes part of a packetentities message.
Can delta from either a baseline or a previous packet_entity
==================
*/
static void SV_WriteDelta (entity_state_t *from, entity_state_t *to, sizebuf_t *msg, qbool force)
{
	int bits, i;


	// send an update
	bits = 0;

	for (i=0 ; i<3 ; i++)
		if ( to->origin[i] != from->origin[i] )
			bits |= U_ORIGIN1<<i;

	if ( to->angles[0] != from->angles[0] )
		bits |= U_ANGLE1;

	if ( to->angles[1] != from->angles[1] )
		bits |= U_ANGLE2;

	if ( to->angles[2] != from->angles[2] )
		bits |= U_ANGLE3;

	if ( to->colormap != from->colormap )
		bits |= U_COLORMAP;

	if ( to->skinnum != from->skinnum )
		bits |= U_SKIN;

	if ( to->frame != from->frame )
		bits |= U_FRAME;

	if ( to->effects != from->effects )
		bits |= U_EFFECTS;

	if ( to->modelindex != from->modelindex )
		bits |= U_MODEL;

	if (bits & U_CHECKMOREBITS)
		bits |= U_MOREBITS;

	if (to->flags & U_SOLID)
		bits |= U_SOLID;

	//
	// write the message
	//
	if (!to->number)
		SV_Error ("Unset entity number");
	if (to->number >= MAX_EDICTS)
	{
		/*SV_Error*/
		Con_Printf ("Entity number >= MAX_EDICTS (%d), set to MAX_EDICTS - 1\n", MAX_EDICTS);
		to->number = MAX_EDICTS - 1;
	}

	if (!bits && !force)
		return;		// nothing to send!
	i = to->number | (bits&~U_CHECKMOREBITS);
	if (i & U_REMOVE)
		Sys_Error ("U_REMOVE");
	MSG_WriteShort (msg, i);

	if (bits & U_MOREBITS)
		MSG_WriteByte (msg, bits&255);
	if (bits & U_MODEL)
		MSG_WriteByte (msg, to->modelindex);
	if (bits & U_FRAME)
		MSG_WriteByte (msg, to->frame);
	if (bits & U_COLORMAP)
		MSG_WriteByte (msg, to->colormap);
	if (bits & U_SKIN)
		MSG_WriteByte (msg, to->skinnum);
	if (bits & U_EFFECTS)
		MSG_WriteByte (msg, to->effects);
	if (bits & U_ORIGIN1)
		MSG_WriteCoord (msg, to->origin[0]);
	if (bits & U_ANGLE1)
		MSG_WriteAngle(msg, to->angles[0]);
	if (bits & U_ORIGIN2)
		MSG_WriteCoord (msg, to->origin[1]);
	if (bits & U_ANGLE2)
		MSG_WriteAngle(msg, to->angles[1]);
	if (bits & U_ORIGIN3)
		MSG_WriteCoord (msg, to->origin[2]);
	if (bits & U_ANGLE3)
		MSG_WriteAngle(msg, to->angles[2]);
}

/*
=============
SV_EmitPacketEntities

Writes a delta update of a packet_entities_t to the message.

=============
*/
static void SV_EmitPacketEntities (client_t *client, packet_entities_t *to, sizebuf_t *msg)
{
	int oldindex, newindex, oldnum, newnum, oldmax;
	client_frame_t	*fromframe;
	packet_entities_t *from1;
	edict_t	*ent;


	// this is the frame that we are going to delta update from
	if (client->delta_sequence != -1)
	{
		fromframe = &client->frames[client->delta_sequence & UPDATE_MASK];
		from1 = &fromframe->entities;
		oldmax = from1->num_entities;

		MSG_WriteByte (msg, svc_deltapacketentities);
		MSG_WriteByte (msg, client->delta_sequence);
	}
	else
	{
		oldmax = 0;	// no delta update
		from1 = NULL;

		MSG_WriteByte (msg, svc_packetentities);
	}

	newindex = 0;
	oldindex = 0;
	//Con_Printf ("---%i to %i ----\n", client->delta_sequence & UPDATE_MASK
	//			, client->netchan.outgoing_sequence & UPDATE_MASK);
	while (newindex < to->num_entities || oldindex < oldmax)
	{
		newnum = newindex >= to->num_entities ? 9999 : to->entities[newindex].number;
		oldnum = oldindex >= oldmax ? 9999 : from1->entities[oldindex].number;

		if (newnum == oldnum)
		{	// delta update from old position
			//Con_Printf ("delta %i\n", newnum);
			SV_WriteDelta (&from1->entities[oldindex], &to->entities[newindex], msg, false);
			oldindex++;
			newindex++;
			continue;
		}

		if (newnum < oldnum)
		{	// this is a new entity, send it from the baseline
			if (newnum == 9999)
			{
				Sys_Printf("LOL, %d, %d, %d, %d %d %d\n", // nice message
				           newnum, oldnum, to->num_entities, oldmax,
				           client->netchan.incoming_sequence & UPDATE_MASK,
				           client->delta_sequence & UPDATE_MASK);
				if (client->edict == NULL)
					Sys_Printf("demo\n");
			}
			ent = EDICT_NUM(newnum);
			//Con_Printf ("baseline %i\n", newnum);
			SV_WriteDelta (&ent->e->baseline, &to->entities[newindex], msg, true);
			newindex++;
			continue;
		}

		if (newnum > oldnum)
		{	// the old entity isn't present in the new message
			//Con_Printf ("remove %i\n", oldnum);
			MSG_WriteShort (msg, oldnum | U_REMOVE);
			oldindex++;
			continue;
		}
	}

	MSG_WriteShort (msg, 0);	// end of packetentities
}

static int TranslateEffects (edict_t *ent)
{
	int fx = (int)ent->v.effects;
	if (pr_nqprogs)
		fx &= ~EF_MUZZLEFLASH;
	if (pr_nqprogs && (fx & EF_DIMLIGHT)) {
		if ((int)ent->v.items & IT_QUAD)
			fx |= EF_BLUE;
		if ((int)ent->v.items & IT_INVULNERABILITY)
			fx |= EF_RED;
	}
	return fx;
}

/*
=============
SV_WritePlayersToClient
=============
*/

#define TruePointContents(p) CM_HullPointContents(&sv.worldmodel->hulls[0], 0, p)

#define ISUNDERWATER(x) ((x) == CONTENTS_WATER || (x) == CONTENTS_SLIME || (x) == CONTENTS_LAVA)

static qbool disable_updates; // disables sending entities to the client


int SV_PMTypeForClient (client_t *cl);
static void SV_WritePlayersToClient (client_t *client, edict_t *clent, byte *pvs, sizebuf_t *msg)
{
	int msec, pflags, pm_type = 0, pm_code = 0, i, j;
	demo_frame_t *demo_frame;
	demo_client_t *dcl;
	usercmd_t cmd;
	client_t *cl;
	edict_t *ent;
	int hideent;

	if (clent && fofs_hideentity)
		hideent = ((eval_t *)((byte *)&(clent)->v + fofs_hideentity))->_int / pr_edict_size;
	else
		hideent = 0;

	demo_frame = &demo.frames[demo.parsecount&UPDATE_MASK];

	for (j=0,cl=svs.clients, dcl = demo_frame->clients; j<MAX_CLIENTS ; j++,cl++, dcl++)
	{
		if (cl->state != cs_spawned)
			continue;

		ent = cl->edict;

		if (clent == NULL)
		{
			if (cl->spectator)
				continue;

			dcl->parsecount = demo.parsecount;

			VectorCopy(ent->v.origin, dcl->origin);
			VectorCopy(ent->v.angles, dcl->angles);
			dcl->angles[0] *= -3;
#ifdef USE_PR2
			if( cl->isBot )
				VectorCopy(ent->v.v_angle, dcl->angles);
#endif
			dcl->angles[2] = 0; // no roll angle

			if (ent->v.health <= 0)
			{	// don't show the corpse looking around...
				dcl->angles[0] = 0;
				dcl->angles[1] = ent->v.angles[1];
				dcl->angles[2] = 0;
			}

			dcl->weaponframe = ent->v.weaponframe;
			dcl->frame       = ent->v.frame;
			dcl->skinnum     = ent->v.skin;
			dcl->model       = ent->v.modelindex;
			dcl->effects     = TranslateEffects(ent);
			dcl->flags       = 0;

			dcl->fixangle    = demo.fixangle[j];
			demo.fixangle[j] = 0;

			dcl->cmdtime     = cl->localtime;
			dcl->sec         = sv.time - cl->localtime;
			
			if (ent->v.health <= 0)
				dcl->flags |= DF_DEAD;
			if (ent->v.mins[2] != -24)
				dcl->flags |= DF_GIB;

			continue;
		}

		// ZOID visibility tracking
		if (ent != clent &&
		        !(client->spec_track && client->spec_track - 1 == j))
		{
			if (cl->spectator)
				continue;

			if (cl - svs.clients == hideent - 1)
				continue;

			// ignore if not touching a PV leaf
			for (i=0 ; i < ent->e->num_leafs ; i++)
				if (pvs[ent->e->leafnums[i] >> 3] & (1 << (ent->e->leafnums[i]&7) ))
					break;
			if (i == ent->e->num_leafs)
				continue; // not visable
		}

		if (disable_updates && client != cl)
		{ // Vladis
			continue;
		}

		pflags = PF_MSEC | PF_COMMAND;

		if (ent->v.modelindex != sv_playermodel)
			pflags |= PF_MODEL;
		for (i=0 ; i<3 ; i++)
			if (ent->v.velocity[i])
				pflags |= PF_VELOCITY1<<i;
		if (ent->v.effects)
			pflags |= PF_EFFECTS;
		if (ent->v.skin)
			pflags |= PF_SKINNUM;
		if (ent->v.health <= 0)
			pflags |= PF_DEAD;
		if (ent->v.mins[2] != -24)
			pflags |= PF_GIB;

		if (cl->spectator)
		{	// only sent origin and velocity to spectators
			pflags &= PF_VELOCITY1 | PF_VELOCITY2 | PF_VELOCITY3;
		}
		else if (ent == clent)
		{	// don't send a lot of data on personal entity
			pflags &= ~(PF_MSEC|PF_COMMAND);
			if (ent->v.weaponframe)
				pflags |= PF_WEAPONFRAME;
		}

		// Z_EXT_PM_TYPE protocol extension
		// encode pm_type and jump_held into pm_code
		pm_type = SV_PMTypeForClient (cl);
		switch (pm_type)
		{
			case PM_DEAD:
				pm_code = PMC_NORMAL; // plus PF_DEAD
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
			case PM_LOCK:
				pm_code = PMC_LOCK;
				break;
			default:
				assert (false);
		}

		pflags |= pm_code << PF_PMC_SHIFT;

		// Z_EXT_PF_ONGROUND protocol extension
		if ((int)ent->v.flags & FL_ONGROUND)
			pflags |= PF_ONGROUND;

		// Z_EXT_PF_SOLID protocol extension
		if (ent->v.solid == SOLID_BBOX || ent->v.solid == SOLID_SLIDEBOX)
			pflags |= PF_SOLID;

		if (pm_type == PM_LOCK && ent == clent)
			pflags |= PF_COMMAND;	// send forced view angles

		if (client->spec_track && client->spec_track - 1 == j &&
		        ent->v.weaponframe)
			pflags |= PF_WEAPONFRAME;

		MSG_WriteByte (msg, svc_playerinfo);
		MSG_WriteByte (msg, j);
		MSG_WriteShort (msg, pflags);

		for (i=0 ; i<3 ; i++)
			MSG_WriteCoord (msg, ent->v.origin[i]);

		MSG_WriteByte (msg, ent->v.frame);

		if (pflags & PF_MSEC)
		{
			msec = 1000*(sv.time - cl->localtime);
			if (msec > 255)
				msec = 255;
			MSG_WriteByte (msg, msec);
		}

		if (pflags & PF_COMMAND)
		{
			cmd = cl->lastcmd;

			if (ent->v.health <= 0)
			{	// don't show the corpse looking around...
				cmd.angles[0] = 0;
				cmd.angles[1] = ent->v.angles[1];
				cmd.angles[0] = 0;
			}

			cmd.buttons = 0;	// never send buttons
			cmd.impulse = 0;	// never send impulses

			if (ent == clent) {
				// this is PM_LOCK, we only want to send view angles
				VectorCopy(ent->v.v_angle, cmd.angles);
				cmd.forwardmove = 0;
				cmd.sidemove = 0;
				cmd.upmove = 0;
			}

			if ((client->extensions & Z_EXT_VWEP) && sv.vw_model_name[0]
					&& fofs_vw_index && ent != clent) {
				cmd.impulse = EdictFieldFloat (ent, fofs_vw_index);
			}

			MSG_WriteDeltaUsercmd (msg, &nullcmd, &cmd);
		}

		for (i=0 ; i<3 ; i++)
			if (pflags & (PF_VELOCITY1<<i) )
				MSG_WriteShort (msg, ent->v.velocity[i]);

		if (pflags & PF_MODEL)
			MSG_WriteByte (msg, ent->v.modelindex);

		if (pflags & PF_SKINNUM)
			MSG_WriteByte (msg, ent->v.skin);

		if (pflags & PF_EFFECTS)
			MSG_WriteByte (msg, TranslateEffects(ent));

		if (pflags & PF_WEAPONFRAME)
			MSG_WriteByte (msg, ent->v.weaponframe);
	}
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

void SV_WriteEntitiesToClient (client_t *client, sizebuf_t *msg, qbool recorder)
{
	int e, i, max_packet_entities;
	packet_entities_t *pack;
	client_frame_t *frame;
	entity_state_t *state;
	edict_t	*clent;
	client_t *cl;
	edict_t *ent;
	vec3_t org;
	byte *pvs;
	int hideent;

	// this is the frame we are creating
	frame = &client->frames[client->netchan.incoming_sequence & UPDATE_MASK];

	// find the client's PVS
	clent = client->edict;
	pvs = NULL;
	if (!recorder)
	{
		VectorAdd (clent->v.origin, clent->v.view_ofs, org);
		pvs = CM_FatPVS (org);
		if (client->fteprotocolextensions & FTE_PEXT_256PACKETENTITIES)
			max_packet_entities = 256;
		else
			max_packet_entities = MAX_PACKET_ENTITIES;
	}
	else
	{
		max_packet_entities = MAX_MVD_PACKET_ENTITIES;

		for (i=0, cl=svs.clients ; i<MAX_CLIENTS ; i++,cl++)
		{
			if (cl->state != cs_spawned)
				continue;

			if (cl->spectator)
				continue;

			VectorAdd (cl->edict->v.origin, cl->edict->v.view_ofs, org);

			// disconnect --> "is it correct?"
			//if (pvs == NULL)
				pvs = CM_FatPVS (org);
			//else
				//	SV_AddToFatPVS (org, sv.worldmodel->nodes, false);
			// <-- disconnect
		}
	}
	if (clent && client->disable_updates_stop > realtime)
	{ // Vladis
		int where = TruePointContents(clent->v.origin); // server flash should not work underwater
		disable_updates = !ISUNDERWATER(where);
	}
	else
	{
		disable_updates = false;
	}

	// send over the players in the PVS
	SV_WritePlayersToClient (client, clent, pvs, msg);

	// put other visible entities into either a packet_entities or a nails message
	pack = &frame->entities;
	pack->num_entities = 0;

	numnails = 0;

	if (fofs_hideentity)
		hideent = ((eval_t *)((byte *)&(clent)->v + fofs_hideentity))->_int / pr_edict_size;
	else
		hideent = 0;

	if (!disable_updates)
	{// Vladis, server flash

		// QW protocol can only handle 512 entities. Any entity with number >= 512 will be invisible
		// From ZQuake.
		// max_edicts = min(sv.num_edicts, MAX_EDICTS);

		for (e = pr_nqprogs ? 1 : MAX_CLIENTS+1, ent=EDICT_NUM(e);
			e < sv.num_edicts/*max_edicts*/;
			e++, ent = NEXT_EDICT(ent))
		{
			if (pr_nqprogs) {
				// don't send the player's model to himself
				if (e < MAX_CLIENTS + 1 && svs.clients[e-1].state != cs_free)
					continue;
			}

			// ignore ents without visible models
			if (!ent->v.modelindex || !*
#ifdef USE_PR2
			        PR2_GetString(ent->v.model)
#else
					PR_GetString(ent->v.model)
#endif
			   )
				continue;

			if (e == hideent)
				continue;

			if (!(int)sv_demoNoVis.value || !recorder)
			{
				// ignore if not touching a PV leaf
				for (i=0 ; i < ent->e->num_leafs ; i++)
					if (pvs[ent->e->leafnums[i] >> 3] & (1 << (ent->e->leafnums[i]&7) ))
						break;

				if (i == ent->e->num_leafs)
					continue;		// not visible
			}

			if (SV_AddNailUpdate (ent))
				continue; // added to the special update list

			// add to the packetentities
			if (pack->num_entities == max_packet_entities)
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
			state->effects = TranslateEffects(ent);
		}
	} // server flash
	// encode the packet entities as a delta from the
	// last packetentities acknowledged by the client

	SV_EmitPacketEntities (client, pack, msg);

	// now add the specialized nail update
	SV_EmitNailUpdate (msg, recorder);

	// Translate NQ progs' EF_MUZZLEFLASH to svc_muzzleflash
	if (pr_nqprogs)
	for (e=1, ent=EDICT_NUM(e) ; e < sv.num_edicts ; e++, ent = NEXT_EDICT(ent))
	{
		// ignore ents without visible models
		if (!ent->v.modelindex || !*PR_GetString(ent->v.model))
			continue;

		// ignore if not touching a PV leaf
		for (i=0 ; i < ent->e->num_leafs ; i++)
			if (pvs[ent->e->leafnums[i] >> 3] & (1 << (ent->e->leafnums[i]&7) ))
				break;

		if ((int)ent->v.effects & EF_MUZZLEFLASH) {
			ent->v.effects = (int)ent->v.effects & ~EF_MUZZLEFLASH;
			MSG_WriteByte (msg, svc_muzzleflash);
			MSG_WriteShort (msg, e);
		}
	}			
}
