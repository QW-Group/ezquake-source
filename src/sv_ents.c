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

#ifndef CLIENTONLY
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

// Maximum packet entity count for live clients.
#define MAX_PACKETENTITIES_POSSIBLE 256

/* Conservative byte budget for a single entity delta record. */
#define ENTITY_DELTA_RESERVE_BYTES 40

static qbool SV_AddNailUpdate (edict_t *ent)
{
	if ((int)sv_nailhack.value)
		return false;

	if (ent->v->modelindex != sv_nailmodel && ent->v->modelindex != sv_supernailmodel)
		return false;

	if (msg_coordsize != 2)
		return false; // Do not allow nailhack in case of sv_bigcoords.

	if (numnails == MAX_NAILS)
		return true;

	nails[numnails] = ent;
	numnails++;
	return true;
}

static void SV_EmitNailUpdate (sizebuf_t *msg, qbool recorder)
{
	int x, y, z, p, yaw, n, i, count;
	byte bits[6]; // [48 bits] xyzpy 12 12 12 4 8
	edict_t *ent;


	count = min(numnails, (msg->maxsize - msg->cursize - 2) / (recorder ? 7 : 6));
	if (count <= 0)
		return;

	if (recorder)
		MSG_WriteByte (msg, svc_nails2);
	else
		MSG_WriteByte (msg, svc_nails);

	MSG_WriteByte (msg, count);

	for (n=0 ; n<count ; n++)
	{
		ent = nails[n];
		if (recorder)
		{
			if (!ent->v->colormap)
			{
				if (!((++nailcount)&255)) nailcount++;
				ent->v->colormap = nailcount&255;
			}

			MSG_WriteByte (msg, (byte)ent->v->colormap);
		}

		x = ((int)(ent->v->origin[0] + 4096 + 1) >> 1) & 4095;
		y = ((int)(ent->v->origin[1] + 4096 + 1) >> 1) & 4095;
		z = ((int)(ent->v->origin[2] + 4096 + 1) >> 1) & 4095;
		p = Q_rint(ent->v->angles[0]*(16.0/360.0)) & 15;
		yaw = Q_rint(ent->v->angles[1]*(256.0/360.0)) & 255;

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
void SV_WriteDelta(client_t* client, entity_state_t *from, entity_state_t *to, sizebuf_t *msg, qbool force)
{
	int bits, i;
#ifdef PROTOCOL_VERSION_FTE
	int evenmorebits = 0;
	unsigned int required_extensions = 0;
	unsigned int fte_extensions = client->fteprotocolextensions;
#endif

	// send an update
	bits = 0;

	for (i = 0; i < 3; i++)
		if (to->origin[i] != from->origin[i])
			bits |= U_ORIGIN1 << i;

	if (to->angles[0] != from->angles[0])
		bits |= U_ANGLE1;

	if (to->angles[1] != from->angles[1])
		bits |= U_ANGLE2;

	if (to->angles[2] != from->angles[2])
		bits |= U_ANGLE3;

	if (to->colormap != from->colormap)
		bits |= U_COLORMAP;

	if (to->skinnum != from->skinnum)
		bits |= U_SKIN;

	if (to->frame != from->frame)
		bits |= U_FRAME;

	if (to->effects != from->effects)
		bits |= U_EFFECTS;

	if (to->modelindex != from->modelindex) {
		bits |= U_MODEL;
#ifdef FTE_PEXT_ENTITYDBL
		if (to->modelindex > 255) {
			if (to->modelindex >= 512) {
				bits &= ~U_MODEL;
			}
			evenmorebits |= U_FTE_MODELDBL;
			required_extensions |= FTE_PEXT_MODELDBL;
		}
#endif
	}

	if (bits & U_CHECKMOREBITS)
		bits |= U_MOREBITS;

	if (to->flags & U_SOLID)
		bits |= U_SOLID;

	// Taken from FTE
	if (msg->cursize + ENTITY_DELTA_RESERVE_BYTES > msg->maxsize && !MSG_HasOverflowHandler(msg))
	{
		// not enough space in the buffer, don't send the entity this frame. (not sending means nothing changes, and it takes no bytes!!)
		int oldnum = to->number;
		*to = *from;
		if (oldnum && !from->number) {
			to->number = oldnum;
		}
		return;
	}

	//
	// write the message
	//
	if (!to->number)
		SV_Error("Unset entity number");

#ifdef PROTOCOL_VERSION_FTE
	if (to->number >= 512)
	{
		if (to->number >= 1024)
		{
			if (to->number >= 1024 + 512) {
				evenmorebits |= U_FTE_ENTITYDBL;
				required_extensions |= FTE_PEXT_ENTITYDBL;
			}

			evenmorebits |= U_FTE_ENTITYDBL2;
			required_extensions |= FTE_PEXT_ENTITYDBL2;
			if (to->number >= 2048)
				SV_Error ("Entity number >= 2048");
		}
		else {
			evenmorebits |= U_FTE_ENTITYDBL;
			required_extensions |= FTE_PEXT_ENTITYDBL;
		}
	}

#ifdef U_FTE_TRANS
	if (to->trans != from->trans && (fte_extensions & FTE_PEXT_TRANS)) {
		evenmorebits |= U_FTE_TRANS;
		required_extensions |= FTE_PEXT_TRANS;
	}
#endif

#ifdef U_FTE_COLOURMOD
	if ((to->colourmod[0] != from->colourmod[0] ||
	     to->colourmod[1] != from->colourmod[1] ||
	     to->colourmod[2] != from->colourmod[2]) && (fte_extensions & FTE_PEXT_COLOURMOD)) {
		evenmorebits |= U_FTE_COLOURMOD;
		required_extensions |= FTE_PEXT_COLOURMOD;
	}
#endif

	if (evenmorebits&0xff00)
		evenmorebits |= U_FTE_YETMORE;
	if (evenmorebits&0x00ff)
		bits |= U_FTE_EVENMORE;
	if (bits & 511)
		bits |= U_MOREBITS;
#endif

	if (to->number >= sv.max_edicts) {
		/*SV_Error*/
		Con_Printf("Entity number >= MAX_EDICTS (%d), set to MAX_EDICTS - 1\n", sv.max_edicts);
		to->number = sv.max_edicts - 1;
	}

#ifdef PROTOCOL_VERSION_FTE
	if (evenmorebits && (fte_extensions & required_extensions) != required_extensions) {
		return;
	}
#endif
	if (!bits && !force) {
		return;		// nothing to send!
	}
	i = (to->number & U_CHECKMOREBITS) | (bits&~U_CHECKMOREBITS);
	if (i & U_REMOVE)
		Sys_Error("U_REMOVE");
	MSG_WriteShort(msg, i);

	if (bits & U_MOREBITS)
		MSG_WriteByte(msg, bits & 255);
#ifdef PROTOCOL_VERSION_FTE
	if (bits & U_FTE_EVENMORE)
		MSG_WriteByte (msg, evenmorebits&255);
	if (evenmorebits & U_FTE_YETMORE)
		MSG_WriteByte (msg, (evenmorebits>>8)&255);
#endif

	if (bits & U_MODEL)
		MSG_WriteByte(msg, to->modelindex & 255);
	else if (evenmorebits & U_FTE_MODELDBL)
		MSG_WriteShort(msg, to->modelindex);
	if (bits & U_FRAME)
		MSG_WriteByte(msg, to->frame);
	if (bits & U_COLORMAP)
		MSG_WriteByte(msg, to->colormap);
	if (bits & U_SKIN)
		MSG_WriteByte(msg, to->skinnum);
	if (bits & U_EFFECTS)
		MSG_WriteByte(msg, to->effects);
	if (bits & U_ORIGIN1) {
		if (client->mvdprotocolextensions1 & MVD_PEXT1_FLOATCOORDS) {
			MSG_WriteLongCoord(msg, to->origin[0]);
		}
		else {
			MSG_WriteCoord(msg, to->origin[0]);
		}
	}
	if (bits & U_ANGLE1) {
		MSG_WriteAngle(msg, to->angles[0]);
	}
	if (bits & U_ORIGIN2) {
		if (client->mvdprotocolextensions1 & MVD_PEXT1_FLOATCOORDS) {
			MSG_WriteLongCoord(msg, to->origin[1]);
		}
		else {
			MSG_WriteCoord(msg, to->origin[1]);
		}
	}
	if (bits & U_ANGLE2) {
		MSG_WriteAngle(msg, to->angles[1]);
	}
	if (bits & U_ORIGIN3) {
		if (client->mvdprotocolextensions1 & MVD_PEXT1_FLOATCOORDS) {
			MSG_WriteLongCoord(msg, to->origin[2]);
		}
		else {
			MSG_WriteCoord(msg, to->origin[2]);
		}
	}
	if (bits & U_ANGLE3) {
		MSG_WriteAngle(msg, to->angles[2]);
	}

#ifdef U_FTE_TRANS
	if (evenmorebits & U_FTE_TRANS)
		MSG_WriteByte (msg, to->trans);
#endif

#ifdef U_FTE_COLOURMOD
	if (evenmorebits & U_FTE_COLOURMOD)
	{
		MSG_WriteByte (msg, to->colourmod[0]);
		MSG_WriteByte (msg, to->colourmod[1]);
		MSG_WriteByte (msg, to->colourmod[2]);
	}
#endif
}

/* Removal records use four bytes for extended entity IDs, two otherwise. */
static void SV_WriteEntityRemoval(sizebuf_t *msg, int number)
{
	if (number >= 512)
	{
		MSG_WriteShort (msg, (number & 511) | U_REMOVE | U_MOREBITS);
		MSG_WriteByte (msg, U_FTE_EVENMORE);
		if (number >= 1024)
		{
			if (number >= 1024 + 512)
			{
				MSG_WriteByte(msg, U_FTE_ENTITYDBL | U_FTE_ENTITYDBL2);
			}
			else
			{
				MSG_WriteByte(msg, U_FTE_ENTITYDBL2);
			}
		}
		else
		{
			MSG_WriteByte(msg, U_FTE_ENTITYDBL);
		}
	}
	else
	{
		MSG_WriteShort(msg, (number & 511) | U_REMOVE);
	}
}

/*
=============
SV_EmitPacketEntities

Writes a delta update of a packet_entities_t to the message.
Saves the state the receiver can reconstruct, including deferred removals.

=============
*/
static void SV_EmitPacketEntities(client_t *client, packet_entities_t *to, sizebuf_t *msg,
                                 int max_entities, int tail_bytes)
{
	packet_entities_t sent;
	packet_entities_t *from = NULL;
	byte remove_sizes[MAX_MVD_PACKET_ENTITIES];
	int oldmax = 0, oldindex, newindex, last_remove = -1;
	int remove_bytes = 0, reserved = 0, known_count, header;
	const int full_limit = msg->maxsize;

	/* A ring slot being reused cannot also supply the delta base. */
	if (client->delta_sequence != -1 && to == &client->frames[client->delta_sequence & UPDATE_MASK].entities)
		client->delta_sequence = -1;
	if (client->delta_sequence != -1)
	{
		from = &client->frames[client->delta_sequence & UPDATE_MASK].entities;
		oldmax = from->num_entities;
	}

	/* Skip this update if its header and terminator cannot fit. */
	header = from ? 2 : 1;
	if (msg->maxsize - msg->cursize < header + 2)
	{
		to->num_entities = oldmax;
		if (oldmax)
			memcpy(to->entities, from->entities, oldmax * sizeof(to->entities[0]));
		return;
	}

	/* Find removed entities and count the bytes their removal records need. */
	for (oldindex = 0, newindex = 0; oldindex < oldmax; ++oldindex)
	{
		int number = from->entities[oldindex].number;

		while (newindex < to->num_entities && to->entities[newindex].number < number)
			++newindex;
		remove_sizes[oldindex] = newindex == to->num_entities || to->entities[newindex].number != number
			? (number >= 512 ? 4 : 2) : 0;
		remove_bytes += remove_sizes[oldindex];
	}

	/* Normally retain room for effects/nails too; a crowded frame favours world progress. */
	if (tail_bytes > 0
		&& tail_bytes <= msg->maxsize - msg->cursize - header - 2 - remove_bytes - ENTITY_DELTA_RESERVE_BYTES)
		msg->maxsize -= tail_bytes;

	/* Reserve fitting removals and count the entity slots they free. */
	known_count = oldmax;
	for (oldindex = 0; oldindex < oldmax; ++oldindex)
	{
		if (!remove_sizes[oldindex])
			continue;
		if (reserved + remove_sizes[oldindex] > msg->maxsize - msg->cursize - header - 2)
			break;
		reserved += remove_sizes[oldindex];
		last_remove = oldindex;
		--known_count;
	}

	MSG_WriteByte(msg, from ? svc_deltapacketentities : svc_packetentities);
	if (from)
		MSG_WriteByte(msg, client->delta_sequence);

	/* Merge the sorted states and build the receiver's resulting state. */
	sent.num_entities = 0;
	oldindex = newindex = 0;
	while (newindex < to->num_entities || oldindex < oldmax)
	{
		int newnum = newindex < to->num_entities ? to->entities[newindex].number : 9999;
		int oldnum = oldindex < oldmax ? from->entities[oldindex].number : 9999;

		if (oldnum < newnum)
		{
			if (oldindex <= last_remove)
			{
				SV_WriteEntityRemoval(msg, oldnum);
				reserved -= remove_sizes[oldindex];
			}
			else
			{
				sent.entities[sent.num_entities++] = from->entities[oldindex];
			}
			++oldindex;
		}
		else
		{
			const int entity_limit = msg->maxsize;
			int before = msg->cursize;
			qbool existing = newnum == oldnum;

			/* Leave room for pending removals and the terminator. */
			msg->maxsize -= reserved + 2;
			if ((existing || known_count < max_entities)
				&& msg->cursize + ENTITY_DELTA_RESERVE_BYTES <= msg->maxsize)
			{
				entity_state_t *base = existing ? &from->entities[oldindex] : &EDICT_NUM(newnum)->e.baseline;
				SV_WriteDelta(client, base, &to->entities[newindex], msg, !existing);
			}
			msg->maxsize = entity_limit;

			/* Record emitted state; retain deferred entries and omit unsent additions. */
			if (msg->cursize != before)
			{
				sent.entities[sent.num_entities++] = to->entities[newindex];
				if (!existing)
					++known_count;
			}
			else if (existing)
			{
				sent.entities[sent.num_entities++] = from->entities[oldindex];
			}
			if (existing)
				++oldindex;
			++newindex;
		}
	}

	MSG_WriteShort(msg, 0);
	msg->maxsize = full_limit;

	/* Save this packet's state as a future delta base. */
	to->num_entities = sent.num_entities;
	memcpy(to->entities, sent.entities, sent.num_entities * sizeof(to->entities[0]));
}

static int TranslateEffects (edict_t *ent)
{
	int fx = (int)ent->v->effects;
	if (pr_nqprogs)
		fx &= ~EF_MUZZLEFLASH;
	if (pr_nqprogs && (fx & EF_DIMLIGHT)) {
		if ((int)ent->v->items & IT_QUAD)
			fx |= EF_BLUE;
		if ((int)ent->v->items & IT_INVULNERABILITY)
			fx |= EF_RED;
	}
	return fx;
}

/*
=============
SV_MVD_WritePlayersToClient
=============
*/
static void SV_MVD_WritePlayersToClient ( void )
{
	int j;
	demo_frame_t *demo_frame;
	demo_client_t *dcl;
	client_t *cl;
	edict_t *ent;

	if ( !sv.mvdrecording )
		return;

	demo_frame = &demo.frames[demo.parsecount&UPDATE_MASK];

	for (j = 0, cl = svs.clients, dcl = demo_frame->clients; j < MAX_CLIENTS; j++, cl++, dcl++)
	{
		if ( cl->state != cs_spawned || cl->spectator )
			continue;

		ent = cl->edict;

		dcl->parsecount = demo.parsecount;

		VectorCopy(ent->v->origin, dcl->origin);
		VectorCopy(ent->v->angles, dcl->angles);
		dcl->angles[0] *= -3;
#ifdef USE_PR2
		if( cl->isBot )
			VectorCopy(ent->v->v_angle, dcl->angles);
#endif
		dcl->angles[2] = 0; // no roll angle

		if (ent->v->health <= 0)
		{	// don't show the corpse looking around...
			dcl->angles[0] = 0;
			dcl->angles[1] = ent->v->angles[1];
			dcl->angles[2] = 0;
		}

		dcl->weaponframe = ent->v->weaponframe;
		dcl->frame       = ent->v->frame;
		dcl->skinnum     = ent->v->skin;
		dcl->model       = ent->v->modelindex;
		dcl->effects     = TranslateEffects(ent);
		dcl->flags       = 0;

		dcl->fixangle    = demo.fixangle[j];
		demo.fixangle[j] = 0;

		dcl->sec         = sv.time - cl->localtime;

		if (ent->v->health <= 0)
			dcl->flags |= DF_DEAD;
		if (ent->v->mins[2] != -24)
			dcl->flags |= DF_GIB;

		continue;
	}
}

/*
=============
SV_PlayerVisibleToClient
=============
*/
qbool SV_PlayerVisibleToClient (client_t* client, int j, byte* pvs, edict_t* self_ent, edict_t* ent)
{
	client_t* cl = &svs.clients[j];

	// ZOID visibility tracking
	if (ent != self_ent && !(client->spec_track && client->spec_track - 1 == j))
	{
		int i;

		if (cl->spectator)
			return false;

		if (pvs && ent->e.num_leafs >= 0) {
			// ignore if not touching a PV leaf
			for (i = 0; i < ent->e.num_leafs; i++) {
				if (pvs[ent->e.leafnums[i] >> 3] & (1 << (ent->e.leafnums[i] & 7))) {
					break;
				}
			}
			if (i == ent->e.num_leafs) {
				return false; // not visible
			}
		}
	}

	return true;
}

/*
=============
SV_WritePlayersToClient
=============
*/

int SV_PMTypeForClient (client_t *cl);
static void SV_WritePlayersToClient (client_t *client, client_frame_t *frame, byte *pvs, qbool disable_updates, sizebuf_t *msg)
{
	int msec, pflags, pm_type = 0, pm_code = 0, i, j;
	usercmd_t cmd;
	int hideent = 0;
	int trackent = 0;
	qbool hide_players = fofs_hide_players && ((eval_t *)((byte *)(client->edict)->v + fofs_hide_players))->_int;

	if (fofs_hideentity)
		hideent = ((eval_t *)((byte *)(client->edict)->v + fofs_hideentity))->_int / pr_edict_size;

	if (fofs_trackent)
	{
		trackent = ((eval_t *)((byte *)(client->edict)->v + fofs_trackent))->_int;
		if (trackent < 1 || trackent > MAX_CLIENTS || svs.clients[trackent - 1].state != cs_spawned)
			trackent = 0;
	}

	frame->sv_time = sv.time;

	for (j = 0; j < MAX_CLIENTS; j++)
	{
		client_t*   cl = &svs.clients[j];
		edict_t*    ent = NULL;
		edict_t*    self_ent = NULL;
		edict_t*    track_ent = NULL;

		if (fofs_visibility) {
			// Presume not visible
			((eval_t *)((byte *)(cl->edict)->v + fofs_visibility))->_int &= ~(1 << (client - svs.clients));
		}

		if (cl->state != cs_spawned)
			continue;

		if (client != cl && hide_players)
			continue;

		if (trackent && cl == client)
		{
			cl = &svs.clients[trackent - 1]; // fakenicking.

			track_ent = svs.clients[trackent - 1].edict;

			self_ent = track_ent;
			ent = track_ent;
		}
		else
		{
			self_ent = client->edict;
			ent = cl->edict;
		}

		// set up edicts.
		if (!SV_PlayerVisibleToClient (client, j, pvs, self_ent, ent))
			continue;

		if (fofs_visibility) {
			// Update flags so mods can tell what was visible
			((eval_t *)((byte *)(ent)->v + fofs_visibility))->_int |= (1 << (client - svs.clients));
		}

		if (j == hideent - 1)
			continue;

		if (j == trackent - 1)
			continue;

		if (disable_updates && ent != self_ent)
		{ // Vladis
			continue;
		}

		//====================================================
		// OK, seems we must send info about this player below

		pflags = PF_MSEC | PF_COMMAND;

		if (ent->v->modelindex != sv_playermodel)
			pflags |= PF_MODEL;
		for (i=0 ; i<3 ; i++)
			if (ent->v->velocity[i])
				pflags |= PF_VELOCITY1<<i;
		if (ent->v->effects)
			pflags |= PF_EFFECTS;
		if (ent->v->skin || ent->v->modelindex >= 256)
			pflags |= PF_SKINNUM;
		if (ent->v->health <= 0)
			pflags |= PF_DEAD;
		if (ent->v->mins[2] != -24)
			pflags |= PF_GIB;

		if (cl->spectator)
		{	// only sent origin and velocity to spectators
			pflags &= PF_VELOCITY1 | PF_VELOCITY2 | PF_VELOCITY3;
		}
		else if (ent == self_ent)
		{	// don't send a lot of data on personal entity
			pflags &= ~(PF_MSEC|PF_COMMAND);
			if (ent->v->weaponframe)
				pflags |= PF_WEAPONFRAME;
		}

#ifdef FTE_PEXT_TRANS
		if (client->fteprotocolextensions & FTE_PEXT_TRANS && ent->xv.alpha > 0.0f && ent->xv.alpha < 1.0f)
		{
			pflags |= PF_TRANS_Z;
		}
#endif

		// Z_EXT_PM_TYPE protocol extension
		// encode pm_type and jump_held into pm_code
		pm_type = track_ent ? PM_LOCK : SV_PMTypeForClient (cl);
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
		if ((int)ent->v->flags & FL_ONGROUND)
			pflags |= PF_ONGROUND;

		// Z_EXT_PF_SOLID protocol extension
		if (ent->v->solid == SOLID_BBOX || ent->v->solid == SOLID_SLIDEBOX)
			pflags |= PF_SOLID;

		if (pm_type == PM_LOCK && ent == self_ent)
			pflags |= PF_COMMAND;	// send forced view angles

		if (client->spec_track && client->spec_track - 1 == j && ent->v->weaponframe)
			pflags |= PF_WEAPONFRAME;

		MSG_WriteByte (msg, svc_playerinfo);
		MSG_WriteByte (msg, j);

#if defined(FTE_PEXT_TRANS)
		if (client->fteprotocolextensions & FTE_PEXT_TRANS)
		{
			if (pflags & 0xff0000)
			{
				pflags |= PF_EXTRA_PFS;
			}
			MSG_WriteShort (msg, pflags & 0xffff);
			if (pflags & PF_EXTRA_PFS)
			{
				MSG_WriteByte(msg, (pflags & 0xff0000) >> 16);
			}
		}
		else
		{
			// Without PEXT_TRANS there's no PF_EXTRA_PFS, move
			// PF_ONGROUND and PF_SOLID to their expected offsets.
			MSG_WriteShort (msg, (pflags & 0x3fff) | ((pflags & 0xc00000) >> 8));
		}
#else
		MSG_WriteShort (msg, pflags);
#endif

		if (client->mvdprotocolextensions1 & MVD_PEXT1_FLOATCOORDS) {
			MSG_WriteLongCoord(msg, ent->v->origin[0]);
			MSG_WriteLongCoord(msg, ent->v->origin[1]);
			MSG_WriteLongCoord(msg, ent->v->origin[2]);
		}
		else {
			MSG_WriteCoord(msg, ent->v->origin[0]);
			MSG_WriteCoord(msg, ent->v->origin[1]);
			MSG_WriteCoord(msg, ent->v->origin[2]);
		}

		MSG_WriteByte (msg, ent->v->frame);

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

			if (ent->v->health <= 0)
			{	// don't show the corpse looking around...
				cmd.angles[0] = 0;
				cmd.angles[1] = ent->v->angles[1];
				cmd.angles[0] = 0;
			}

			cmd.buttons = 0;	// never send buttons
			cmd.impulse = 0;	// never send impulses

			if (ent == self_ent)
			{
				// this is PM_LOCK, we only want to send view angles
				VectorCopy(ent->v->v_angle, cmd.angles);
				cmd.forwardmove = 0;
				cmd.sidemove = 0;
				cmd.upmove = 0;
			}

			if ((client->extensions & Z_EXT_VWEP) && sv.vw_model_name[0] && fofs_vw_index) {
				cmd.impulse = EdictFieldFloat (ent, fofs_vw_index);
			}

			MSG_WriteDeltaUsercmd (msg, &nullcmd, &cmd, 0);
		}

		for (i=0 ; i<3 ; i++)
			if (pflags & (PF_VELOCITY1<<i) )
				MSG_WriteShort (msg, ent->v->velocity[i]);

		if (pflags & PF_MODEL)
			MSG_WriteByte (msg, ent->v->modelindex);

		if (pflags & PF_SKINNUM)
			MSG_WriteByte (msg, (int)ent->v->skin | (((pflags & PF_MODEL)&&(ent->v->modelindex >= 256))<<7));

		if (pflags & PF_EFFECTS)
			MSG_WriteByte (msg, TranslateEffects(ent));

		if (pflags & PF_WEAPONFRAME)
			MSG_WriteByte (msg, ent->v->weaponframe);

#ifdef FTE_PEXT_TRANS
		if (pflags & PF_TRANS_Z)
		{
			MSG_WriteByte (msg, bound(1, (byte)(ent->xv.alpha * 254.0f), 254));
		}
#endif
	}
}

/*
=============
SV_EntityVisibleToClient
=============
*/
qbool SV_EntityVisibleToClient (client_t* client, int e, byte* pvs)
{
	edict_t* ent = EDICT_NUM (e);

	if (pr_nqprogs)
	{
		// don't send the player's model to himself
		if (e < MAX_CLIENTS + 1 && svs.clients[e-1].state != cs_free)
			return false;
	}

	// ignore ents without visible models
	if (!ent->v->modelindex || !*PR_GetEntityString(ent->v->model))
		return false;

	if ( pvs && ent->e.num_leafs >= 0 )
	{
		int i;

		// ignore if not touching a PV leaf
		for (i=0 ; i < ent->e.num_leafs ; i++)
			if (pvs[ent->e.leafnums[i] >> 3] & (1 << (ent->e.leafnums[i]&7) ))
				break;

		if (i == ent->e.num_leafs)
			return false;		// not visible
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

void SV_WriteEntitiesToClient (client_t *client, sizebuf_t *msg, qbool recorder)
{
	qbool disable_updates; // disables sending entities to the client
	int e, i, max_packet_entities;
	int full_limit, datagram_bytes, tail_bytes;
	packet_entities_t *pack;
	client_frame_t *frame;
	entity_state_t *state;
	edict_t *ent;
	byte *pvs;
	int hideent;
	// The MVD recorder isn't one of svs.clients, so it has no visibility bit
	unsigned int client_flag = recorder ? 0 : 1u << (client - svs.clients);
	edict_t	*clent = client->edict;

	float distances[MAX_PACKETENTITIES_POSSIBLE] = { 0 };
	float distance;
	int position;
	vec3_t org;

	if ( recorder )
	{
		if ( !sv.mvdrecording )
			return;
	}

	// this is the frame we are creating
	frame = &client->frames[client->netchan.incoming_sequence & UPDATE_MASK];

	// find the client's PVS
	if ( recorder )
	{// demo
		hideent = 0;
		pvs = NULL; // ignore PVS for demos
		max_packet_entities = MAX_MVD_PACKET_ENTITIES;
		disable_updates = false; // updates always allowed in demos
	}
	else
	{// normal client
		vec3_t org;
		int trackent = 0;

		if (fofs_hideentity)
			hideent = ((eval_t *)((byte *)(client->edict)->v + fofs_hideentity))->_int / pr_edict_size;
		else
			hideent = 0;

		if (fofs_trackent)
		{
			trackent = ((eval_t *)((byte *)(client->edict)->v + fofs_trackent))->_int;
			if (trackent < 1 || trackent > MAX_CLIENTS || svs.clients[trackent - 1].state != cs_spawned)
				trackent = 0;
		}

		// we should use org of tracked player in case or trackent.
		if (trackent)
		{
			VectorAdd (svs.clients[trackent - 1].edict->v->origin, svs.clients[trackent - 1].edict->v->view_ofs, org);		
		}
		else
		{
			VectorAdd (client->edict->v->origin, client->edict->v->view_ofs, org);
		}

		pvs = CM_FatPVS (org); // search some PVS
		max_packet_entities = (client->fteprotocolextensions & FTE_PEXT_256PACKETENTITIES) ? MAX_PEXT256_PACKET_ENTITIES : MAX_PACKET_ENTITIES;

		if (client->disable_updates_stop > realtime)
		{
			#define ISUNDERWATER(x) ((x) == CONTENTS_WATER || (x) == CONTENTS_SLIME || (x) == CONTENTS_LAVA)

			// server flash should not work underwater
			int content = CM_HullPointContents(&sv.worldmodel->hulls[0], 0, client->edict->v->origin);
			disable_updates = !ISUNDERWATER(content);

			#undef ISUNDERWATER
		}
		else
		{
			disable_updates = false;
		}
	}

	// send over the players in the PVS
	if ( recorder )
		SV_MVD_WritePlayersToClient (); // nice, no params at all!
	else
		SV_WritePlayersToClient (client, frame, pvs, disable_updates, msg);

	// put other visible entities into either a packet_entities or a nails message
	pack = &frame->entities;
	pack->num_entities = 0;

	numnails = 0;

	if (!disable_updates)
	{// Vladis, server flash

		// QW protocol can only handle 512 entities. Any entity with number >= 512 will be invisible
		// from ZQuake unless using protocol extensions.
		// max_edicts = min(sv.num_edicts, MAX_EDICTS);

		for (e = pr_nqprogs ? 1 : MAX_CLIENTS + 1, ent = EDICT_NUM(e); e < sv.num_edicts; e++, ent = NEXT_EDICT(ent))
		{
			if (!SV_EntityVisibleToClient(client, e, pvs)) {
				if (fofs_visibility) {
					((eval_t *)((byte *)(ent)->v + fofs_visibility))->_int &= ~client_flag;
				}
				continue;
			}

			if (fofs_visibility) {
				// Don't include other filters in logic for setting this field
				((eval_t *)((byte *)(ent)->v + fofs_visibility))->_int |= client_flag;
			}

			if (e == hideent) {
				continue;
			}

			if (SV_AddNailUpdate (ent))
				continue; // added to the special update list

			if (clent) {
				VectorAdd(ent->v->absmin, ent->v->absmax, org);
				VectorMA(clent->v->origin, -0.5, org, org);
				distance = DotProduct(org, org);	//Length

				// add to the packetentities
				if (pack->num_entities == max_packet_entities) {
					// replace the furthest entity
					float furthestdist = -1;
					int best = -1;
					for (i = 0; i < max_packet_entities; i++) {
						if (furthestdist < distances[i]) {
							furthestdist = distances[i];
							best = i;
						}
					}

					if (furthestdist <= distance || best == -1) {
						continue;
					}

					// shuffle other entities down, add to end
					if (best < pack->num_entities - 1) {
						memmove(&pack->entities[best], &pack->entities[best + 1], sizeof(pack->entities[0]) * (pack->num_entities - 1 - best));
						memmove(&distances[best], &distances[best + 1], sizeof(distances[0]) * (pack->num_entities - 1 - best));
					}
					position = pack->num_entities - 1;
				}
				else {
					position = pack->num_entities++;
				}

				distances[position] = distance;
			}
			else {
				if (pack->num_entities == max_packet_entities) {
					continue;
				}

				position = pack->num_entities++;
			}

			state = &pack->entities[position];
			memset(state, 0, sizeof(*state));

			state->number = e;
			state->flags = 0;
			VectorCopy (ent->v->origin, state->origin);
			VectorCopy (ent->v->angles, state->angles);
			state->modelindex = ent->v->modelindex;
			state->frame = ent->v->frame;
			state->colormap = ent->v->colormap;
			state->skinnum = ent->v->skin;
			state->effects = TranslateEffects(ent);
#ifdef FTE_PEXT_TRANS
			state->trans = ent->xv.alpha >= 1.0f ? 0 : bound(0, (byte)(ent->xv.alpha * 254.0f), 254);
#endif
#ifdef FTE_PEXT_COLOURMOD
			if (ent->xv.colourmod[0] != 1.0f && ent->xv.colourmod[1] != 1.0f && ent->xv.colourmod[2] != 1.0f)
			{
				state->colourmod[0] = bound(0, ent->xv.colourmod[0] * (256.0f / 8.0f), 255);
				state->colourmod[1] = bound(0, ent->xv.colourmod[1] * (256.0f / 8.0f), 255);
				state->colourmod[2] = bound(0, ent->xv.colourmod[2] * (256.0f / 8.0f), 255);
			}
#endif
		}
	} // server flash

	full_limit = msg->maxsize;
	datagram_bytes = !recorder && !client->datagram.overflowed ? client->datagram.cursize : 0;
	tail_bytes = datagram_bytes + (numnails ? 2 + numnails * (recorder ? 7 : 6) : 0);

	// Encode packet entities, using the client's acknowledged frame when available.
	SV_EmitPacketEntities(client, pack, msg, max_packet_entities, tail_bytes);

	/* Keep optional tails from using space reserved for queued multicast. */
	msg->maxsize = max(msg->cursize, full_limit - datagram_bytes);

	// now add the specialized nail update
	SV_EmitNailUpdate (msg, recorder);

	// Translate NQ progs' EF_MUZZLEFLASH to svc_muzzleflash
	if (pr_nqprogs)
	{
		for (e=1, ent=EDICT_NUM(e) ; e < sv.num_edicts ; e++, ent = NEXT_EDICT(ent))
		{
			// ignore ents without visible models
			if (!ent->v->modelindex || !*PR_GetEntityString(ent->v->model))
				continue;

			// ignore if not touching a PV leaf (meag: this does nothing... complete or remove?)
			if (pvs && ent->e.num_leafs >= 0) {
				for (i = 0; i < ent->e.num_leafs; i++)
					if (pvs[ent->e.leafnums[i] >> 3] & (1 << (ent->e.leafnums[i] & 7)))
						break;
			}

			if ((int)ent->v->effects & EF_MUZZLEFLASH) {
				ent->v->effects = (int)ent->v->effects & ~EF_MUZZLEFLASH;
				if (msg->maxsize - msg->cursize < 3)
					continue;
				MSG_WriteByte (msg, svc_muzzleflash);
				MSG_WriteShort (msg, e);
			}
		}
	}
	msg->maxsize = full_limit;
}

/*
============
SV_SetVisibleEntitiesForBot
============
*/
void SV_SetVisibleEntitiesForBot (client_t* client)
{
	int j = 0;
	int e = 0;
	unsigned int client_flag = 1 << (client - svs.clients);
	vec3_t org;
	byte* pvs = NULL;

	if (!fofs_visibility)
		return;

	VectorAdd (client->edict->v->origin, client->edict->v->view_ofs, org);
	pvs = CM_FatPVS (org); // search some PVS

	// players first
	for (j = 0; j < MAX_CLIENTS; j++)
	{
		if (SV_PlayerVisibleToClient(client, j, pvs, client->edict, svs.clients[j].edict)) {
			((eval_t *)((byte *)(svs.clients[j].edict)->v + fofs_visibility))->_int |= client_flag;
		}
		else {
			((eval_t *)((byte *)(svs.clients[j].edict)->v + fofs_visibility))->_int &= ~client_flag;
		}
	}

	// Other entities
	for (e = pr_nqprogs ? 1 : MAX_CLIENTS + 1; e < sv.num_edicts; e++)
	{
		edict_t* ent = EDICT_NUM (e);

		if (SV_EntityVisibleToClient(client, e, pvs)) {
			((eval_t *)((byte *)(ent)->v + fofs_visibility))->_int |= client_flag;
		}
		else {
			((eval_t *)((byte *)(ent)->v + fofs_visibility))->_int &= ~client_flag;
		}
	}
}

qbool SV_SkipCommsBotMessage(client_t* client)
{
	extern cvar_t sv_serveme_fix;

	return sv_serveme_fix.value && client->spectator && !strcmp(client->name, "[ServeMe]");
}

#endif // !CLIENTONLY
