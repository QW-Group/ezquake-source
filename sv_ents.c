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


//=============================================================================

// because there can be a lot of nails, there is a special
// network protocol for them
#define MAX_NAILS 32
static edict_t *nails[MAX_NAILS];
static int numnails;
static int nailcount = 0;

extern	int sv_nailmodel, sv_supernailmodel, sv_playermodel;

cvar_t	sv_nailhack	= {"sv_nailhack", "1"};

// Maximum packet we will send - currently 256 if extension supported
#define MAX_PACKETENTITIES_POSSIBLE 256

static qbool SV_AddNailUpdate (edict_t *ent)
{
	if ((int)sv_nailhack.value)
		return false;

	if (ent->v.modelindex != sv_nailmodel && ent->v.modelindex != sv_supernailmodel)
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
	if (msg->cursize + 40 > msg->maxsize)
	{	//not enough space in the buffer, don't send the entity this frame. (not sending means nothing changes, and it takes no bytes!!)
		*to = *from;
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
			SV_WriteDelta (client, &from1->entities[oldindex], &to->entities[newindex], msg, false);
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
			SV_WriteDelta (client, &ent->e->baseline, &to->entities[newindex], msg, true);
			newindex++;
			continue;
		}

		if (newnum > oldnum)
		{
			// the old entity isn't present in the new message
			if (oldnum >= 512) {
				//yup, this is expensive.
				MSG_WriteShort (msg, oldnum | U_REMOVE | U_MOREBITS);
				MSG_WriteByte (msg, U_FTE_EVENMORE);
				if (oldnum >= 1024) {
					if (oldnum >= 1024 + 512) {
						MSG_WriteByte(msg, U_FTE_ENTITYDBL | U_FTE_ENTITYDBL2);
					}
					else {
						MSG_WriteByte(msg, U_FTE_ENTITYDBL2);
					}
				}
				else {
					MSG_WriteByte(msg, U_FTE_ENTITYDBL);
				}
			}
			else {
				MSG_WriteShort(msg, oldnum | U_REMOVE);
			}

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

		dcl->sec         = sv.time - cl->localtime;

		if (ent->v.health <= 0)
			dcl->flags |= DF_DEAD;
		if (ent->v.mins[2] != -24)
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

		if (pvs && ent->e->num_leafs >= 0) {
			// ignore if not touching a PV leaf
			for (i = 0; i < ent->e->num_leafs; i++) {
				if (pvs[ent->e->leafnums[i] >> 3] & (1 << (ent->e->leafnums[i] & 7))) {
					break;
				}
			}
			if (i == ent->e->num_leafs) {
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
	qbool hide_players = fofs_hide_players && ((eval_t *)((byte *)&(client->edict)->v + fofs_hide_players))->_int;

	if (fofs_hideentity)
		hideent = ((eval_t *)((byte *)&(client->edict)->v + fofs_hideentity))->_int / pr_edict_size;

	if (fofs_trackent)
	{
		trackent = ((eval_t *)((byte *)&(client->edict)->v + fofs_trackent))->_int;
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
			((eval_t *)((byte *)&(cl->edict)->v + fofs_visibility))->_int &= ~(1 << (client - svs.clients));
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
			((eval_t *)((byte *)&(ent)->v + fofs_visibility))->_int |= (1 << (client - svs.clients));
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

		if (ent->v.modelindex != sv_playermodel)
			pflags |= PF_MODEL;
		for (i=0 ; i<3 ; i++)
			if (ent->v.velocity[i])
				pflags |= PF_VELOCITY1<<i;
		if (ent->v.effects)
			pflags |= PF_EFFECTS;
		if (ent->v.skin || ent->v.modelindex >= 256)
			pflags |= PF_SKINNUM;
		if (ent->v.health <= 0)
			pflags |= PF_DEAD;
		if (ent->v.mins[2] != -24)
			pflags |= PF_GIB;

		if (cl->spectator)
		{	// only sent origin and velocity to spectators
			pflags &= PF_VELOCITY1 | PF_VELOCITY2 | PF_VELOCITY3;
		}
		else if (ent == self_ent)
		{	// don't send a lot of data on personal entity
			pflags &= ~(PF_MSEC|PF_COMMAND);
			if (ent->v.weaponframe)
				pflags |= PF_WEAPONFRAME;
		}

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
		if ((int)ent->v.flags & FL_ONGROUND)
			pflags |= PF_ONGROUND;

		// Z_EXT_PF_SOLID protocol extension
		if (ent->v.solid == SOLID_BBOX || ent->v.solid == SOLID_SLIDEBOX)
			pflags |= PF_SOLID;

		if (pm_type == PM_LOCK && ent == self_ent)
			pflags |= PF_COMMAND;	// send forced view angles

		if (client->spec_track && client->spec_track - 1 == j && ent->v.weaponframe)
			pflags |= PF_WEAPONFRAME;

		MSG_WriteByte (msg, svc_playerinfo);
		MSG_WriteByte (msg, j);
		MSG_WriteShort (msg, pflags);

		if (client->mvdprotocolextensions1 & MVD_PEXT1_FLOATCOORDS) {
			MSG_WriteLongCoord(msg, ent->v.origin[0]);
			MSG_WriteLongCoord(msg, ent->v.origin[1]);
			MSG_WriteLongCoord(msg, ent->v.origin[2]);
		}
		else {
			MSG_WriteCoord(msg, ent->v.origin[0]);
			MSG_WriteCoord(msg, ent->v.origin[1]);
			MSG_WriteCoord(msg, ent->v.origin[2]);
		}

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

			if (ent == self_ent)
			{
				// this is PM_LOCK, we only want to send view angles
				VectorCopy(ent->v.v_angle, cmd.angles);
				cmd.forwardmove = 0;
				cmd.sidemove = 0;
				cmd.upmove = 0;
			}

			if ((client->extensions & Z_EXT_VWEP) && sv.vw_model_name[0] && fofs_vw_index) {
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
			MSG_WriteByte (msg, (int)ent->v.skin | (((pflags & PF_MODEL)&&(ent->v.modelindex >= 256))<<7));

		if (pflags & PF_EFFECTS)
			MSG_WriteByte (msg, TranslateEffects(ent));

		if (pflags & PF_WEAPONFRAME)
			MSG_WriteByte (msg, ent->v.weaponframe);
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
	if (!ent->v.modelindex || !*PR_GetEntityString(ent->v.model))
		return false;

	if ( pvs && ent->e->num_leafs >= 0 )
	{
		int i;

		// ignore if not touching a PV leaf
		for (i=0 ; i < ent->e->num_leafs ; i++)
			if (pvs[ent->e->leafnums[i] >> 3] & (1 << (ent->e->leafnums[i]&7) ))
				break;

		if (i == ent->e->num_leafs)
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
	packet_entities_t *pack;
	client_frame_t *frame;
	entity_state_t *state;
	edict_t *ent;
	byte *pvs;
	int hideent;
	unsigned int client_flag = (1 << (client - svs.clients));
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
			hideent = ((eval_t *)((byte *)&(client->edict)->v + fofs_hideentity))->_int / pr_edict_size;
		else
			hideent = 0;

		if (fofs_trackent)
		{
			trackent = ((eval_t *)((byte *)&(client->edict)->v + fofs_trackent))->_int;
			if (trackent < 1 || trackent > MAX_CLIENTS || svs.clients[trackent - 1].state != cs_spawned)
				trackent = 0;
		}

		// we should use org of tracked player in case or trackent.
		if (trackent)
		{
			VectorAdd (svs.clients[trackent - 1].edict->v.origin, svs.clients[trackent - 1].edict->v.view_ofs, org);		
		}
		else
		{
			VectorAdd (client->edict->v.origin, client->edict->v.view_ofs, org);
		}

		pvs = CM_FatPVS (org); // search some PVS
		max_packet_entities = (client->fteprotocolextensions & FTE_PEXT_256PACKETENTITIES) ? MAX_PEXT256_PACKET_ENTITIES : MAX_PACKET_ENTITIES;

		if (client->disable_updates_stop > realtime)
		{
			#define ISUNDERWATER(x) ((x) == CONTENTS_WATER || (x) == CONTENTS_SLIME || (x) == CONTENTS_LAVA)

			// server flash should not work underwater
			int content = CM_HullPointContents(&sv.worldmodel->hulls[0], 0, client->edict->v.origin);
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
					((eval_t *)((byte *)&(ent)->v + fofs_visibility))->_int &= ~client_flag;
				}
				continue;
			}

			if (fofs_visibility) {
				// Don't include other filters in logic for setting this field
				((eval_t *)((byte *)&(ent)->v + fofs_visibility))->_int |= (1 << (client - svs.clients));
			}

			if (e == hideent) {
				continue;
			}

			if (SV_AddNailUpdate (ent))
				continue; // added to the special update list

			if (clent) {
				VectorAdd(ent->v.absmin, ent->v.absmax, org);
				VectorMA(clent->v.origin, -0.5, org, org);
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
	{
		for (e=1, ent=EDICT_NUM(e) ; e < sv.num_edicts ; e++, ent = NEXT_EDICT(ent))
		{
			// ignore ents without visible models
			if (!ent->v.modelindex || !*PR_GetEntityString(ent->v.model))
				continue;

			// ignore if not touching a PV leaf (meag: this does nothing... complete or remove?)
			if (pvs && ent->e->num_leafs >= 0) {
				for (i = 0; i < ent->e->num_leafs; i++)
					if (pvs[ent->e->leafnums[i] >> 3] & (1 << (ent->e->leafnums[i] & 7)))
						break;
			}

			if ((int)ent->v.effects & EF_MUZZLEFLASH) {
				ent->v.effects = (int)ent->v.effects & ~EF_MUZZLEFLASH;
				MSG_WriteByte (msg, svc_muzzleflash);
				MSG_WriteShort (msg, e);
			}
		}
	}
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

	VectorAdd (client->edict->v.origin, client->edict->v.view_ofs, org);
	pvs = CM_FatPVS (org); // search some PVS

	// players first
	for (j = 0; j < MAX_CLIENTS; j++)
	{
		if (SV_PlayerVisibleToClient(client, j, pvs, client->edict, svs.clients[j].edict)) {
			((eval_t *)((byte *)&(svs.clients[j].edict)->v + fofs_visibility))->_int |= client_flag;
		}
		else {
			((eval_t *)((byte *)&(svs.clients[j].edict)->v + fofs_visibility))->_int &= ~client_flag;
		}
	}

	// Other entities
	for (e = pr_nqprogs ? 1 : MAX_CLIENTS + 1; e < sv.num_edicts; e++)
	{
		edict_t* ent = EDICT_NUM (e);

		if (SV_EntityVisibleToClient(client, e, pvs)) {
			((eval_t *)((byte *)&(ent)->v + fofs_visibility))->_int |= client_flag;
		}
		else {
			((eval_t *)((byte *)&(ent)->v + fofs_visibility))->_int &= ~client_flag;
		}
	}
}

qbool SV_SkipCommsBotMessage(client_t* client)
{
	extern cvar_t sv_serveme_fix;

	return sv_serveme_fix.value && client->spectator && !strcmp(client->name, "[ServeMe]");
}
