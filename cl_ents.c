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

	$Id: cl_ents.c,v 1.58 2007-09-17 19:37:55 qqshka Exp $

*/

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "vx_stuff.h"
#include "pmove.h"
#include "utils.h"


static int MVD_TranslateFlags(int src);
void TP_ParsePlayerInfo(player_state_t *, player_state_t *, player_info_t *info);	

#define ISDEAD(i) ( (i) >= 41 && (i) <= 102 )

extern cvar_t cl_predict_players, cl_solid_players, cl_rocket2grenade;
extern cvar_t cl_predict_half;
extern cvar_t cl_model_bobbing;		
extern cvar_t cl_nolerp, cl_lerp_monsters, cl_newlerp;
extern cvar_t r_drawvweps;		

static struct predicted_player {
	int flags;
	qbool active;
	vec3_t origin;	// predicted origin

	qbool predict;
	vec3_t	oldo;
	vec3_t	olda;
	vec3_t	oldv;
	player_state_t *oldstate;

} predicted_players[MAX_CLIENTS];

char *cl_modelnames[cl_num_modelindices];
int cl_modelindices[cl_num_modelindices];
model_t *cl_flame0_model;

void CL_InitEnts(void) {
	int i;
	byte *memalloc;

	memset(cl_modelnames, 0, sizeof(cl_modelnames));

	cl_modelnames[mi_spike] = "progs/spike.mdl";
	cl_modelnames[mi_player] = "progs/player.mdl";
	cl_modelnames[mi_eyes] = "progs/eyes.mdl";
	cl_modelnames[mi_flag] = "progs/flag.mdl";
	cl_modelnames[mi_tf_flag] = "progs/tf_flag.mdl";
	cl_modelnames[mi_tf_stan] = "progs/tf_stan.mdl";
	cl_modelnames[mi_explod1] = "progs/s_explod.spr";
	cl_modelnames[mi_explod2] = "progs/s_expl.spr";
	cl_modelnames[mi_h_player] = "progs/h_player.mdl";
	cl_modelnames[mi_gib1] = "progs/gib1.mdl";
	cl_modelnames[mi_gib2] = "progs/gib2.mdl";
	cl_modelnames[mi_gib3] = "progs/gib3.mdl";
	cl_modelnames[mi_rocket] = "progs/missile.mdl";
	cl_modelnames[mi_grenade] = "progs/grenade.mdl";
	cl_modelnames[mi_bubble] = "progs/s_bubble.spr";
	cl_modelnames[mi_flame] = "progs/flame.mdl";	//joe

	// carried weapon models (drawviewmodel)
	cl_modelnames[mi_weapon1] = "progs/v_axe.mdl";
	cl_modelnames[mi_weapon2] = "progs/v_shot.mdl";
	cl_modelnames[mi_weapon3] = "progs/v_shot2.mdl";
	cl_modelnames[mi_weapon4] = "progs/v_nail.mdl";
	cl_modelnames[mi_weapon5] = "progs/v_nail2.mdl";
	cl_modelnames[mi_weapon6] = "progs/v_rock.mdl";
	cl_modelnames[mi_weapon7] = "progs/v_rock2.mdl";
	cl_modelnames[mi_weapon8] = "progs/v_light.mdl";

	cl_modelnames[mi_vaxe] = "progs/v_axe.mdl";
	cl_modelnames[mi_vbio] = "progs/v_bio.mdl";
	cl_modelnames[mi_vgrap] = "progs/v_grap.mdl";
	cl_modelnames[mi_vknife] = "progs/v_knife.mdl";
	cl_modelnames[mi_vknife2] = "progs/v_knife2.mdl";
	cl_modelnames[mi_vmedi] = "progs/v_medi.mdl";
	cl_modelnames[mi_vspan] = "progs/v_span.mdl";
	// monsters
	cl_modelnames[mi_monster1] = "progs/soldier.mdl";
	cl_modelnames[mi_m2] = "progs/dog.mdl";
	cl_modelnames[mi_m3] = "progs/demon.mdl";
	cl_modelnames[mi_m4] = "progs/ogre.mdl";
	cl_modelnames[mi_m5] = "progs/shambler.mdl";
	cl_modelnames[mi_m6] = "progs/knight.mdl";
	cl_modelnames[mi_m7] = "progs/zombie.mdl";
	cl_modelnames[mi_m8] = "progs/wizard.mdl";
	cl_modelnames[mi_m9] = "progs/enforcer.mdl";
	cl_modelnames[mi_m10] = "progs/fish.mdl";
	cl_modelnames[mi_m11] = "progs/hknight.mdl";
	cl_modelnames[mi_m12] = "progs/shalrath.mdl";
	cl_modelnames[mi_m13] = "progs/tarbaby";
	// hipnotic monsters
	cl_modelnames[mi_m14] = "progs/armabody.mdl";
	cl_modelnames[mi_m15] = "progs/armalegs.mdl";
	cl_modelnames[mi_m16] = "progs/grem.mdl";
	cl_modelnames[mi_m17] = "progs/scor.mdl";

	// Item sprites.
	// FIXME : 32-bit sprites not working properly.
	#if 0
	cl_modelnames[mi_2dshells]		= "sprites/s_shells.spr";
	cl_modelnames[mi_2dcells]		= "sprites/s_cells.spr";
	cl_modelnames[mi_2drockets]		= "sprites/s_rockets.spr";
	cl_modelnames[mi_2dnails]		= "sprites/s_nails.spr";
	cl_modelnames[mi_2dmega]		= "sprites/s_mega.spr";
	cl_modelnames[mi_2dpent]		= "sprites/s_invuln.spr";
	cl_modelnames[mi_2dquad]		= "sprites/s_quad.spr";
	cl_modelnames[mi_2dring]		= "sprites/s_invis.spr";
	cl_modelnames[mi_2dsuit]		= "sprites/s_suit.spr";
	cl_modelnames[mi_2darmor1]		= "sprites/s_armor1.spr";
	cl_modelnames[mi_2darmor2]		= "sprites/s_armor2.spr";
	cl_modelnames[mi_2darmor3]		= "sprites/s_armor3.spr";
	cl_modelnames[mi_2dbackpack]	= "sprites/s_backpack.spr";
	cl_modelnames[mi_2dhealth10]	= "sprites/s_health10.spr";
	cl_modelnames[mi_2dhealth25]	= "sprites/s_health25.spr";
	#endif

	// FIXME, delay until map load time?
	cl_flame0_model = Mod_ForName ("progs/flame0.mdl", false);

	for (i = 0; i < cl_num_modelindices; i++) 
	{
		if (!cl_modelnames[i])
			Sys_Error("cl_modelnames[%d] not initialized", i);
	}

	cl_firstpassents.max = 64;
	cl_firstpassents.alpha = 0;

	cl_visents.max = 256;
	cl_visents.alpha = 0;

	cl_alphaents.max = 64;
	cl_alphaents.alpha = 1;

	memalloc = (byte *) Hunk_AllocName((cl_firstpassents.max + cl_visents.max + cl_alphaents.max) * sizeof(entity_t), "visents");
	cl_firstpassents.list = (entity_t *) memalloc;
	cl_visents.list = (entity_t *) memalloc + cl_firstpassents.max;
	cl_alphaents.list = (entity_t *) memalloc + cl_firstpassents.max + cl_visents.max;

	CL_ClearScene();
}

static qbool is_monster (int modelindex)
{
	int i;
	if (!cl_lerp_monsters.value)
		return false;
	for (i = 1; i < 17; i++)
		if (modelindex == cl_modelindices[mi_monster1 + i - 1])
			return true;
	return false;
}

void CL_ClearScene (void) {
	cl_firstpassents.count = cl_visents.count = cl_alphaents.count = 0;
}

void CL_AddEntity (entity_t *ent) {
	visentlist_t *vislist;

	if ((ent->effects & (EF_BLUE | EF_RED | EF_GREEN)) && bound(0, gl_powerupshells.value, 1))
	{
		vislist = &cl_visents;
	}
	else if (ent->model->type == mod_sprite)
	{
		vislist = &cl_alphaents;
	}
	else if (ent->model->modhint == MOD_PLAYER || ent->model->modhint == MOD_EYES || ent->renderfx & RF_PLAYERMODEL)
	{
		vislist = &cl_firstpassents;
		ent->renderfx |= RF_NOSHADOW;
	}
	else
	{
		vislist = &cl_visents;
	}

	if (vislist->count < vislist->max)
		vislist->list[vislist->count++] = *ent;
}

// NUM_DLIGHTTYPES - this constant not used here, but help u find dynamic light related code if u change something
static dlighttype_t dl_colors[] = {lt_red, lt_blue, lt_redblue, lt_green, lt_redgreen, lt_bluegreen, lt_white};
static int dl_colors_cnt = sizeof(dl_colors) / sizeof(dl_colors[0]);

dlighttype_t dlightColor(float f, dlighttype_t def, qbool random) {

	if ((int) f >= 1 && (int) f <= dl_colors_cnt)
		return dl_colors[(int) f - 1];
	else if (((int) f == dl_colors_cnt + 1) && random)
		return dl_colors[rand() % dl_colors_cnt];
	else
		return def;
}

// if we have color in "r g b" format use it overwise use pre defined colors
customlight_t *dlightColorEx(float f, char *str, dlighttype_t def, qbool random, customlight_t *l) 
{
	// TODO : Ok to use this in software also?
	byte color[4];
	int i;

	i = StringToRGB_W(str, color);

	if (i > 1)
		l->type = lt_custom; // we got at least two params so treat this as custom color
	else
		l->type = dlightColor(f, def, random); // this may set lt_custom too

	if (l->type == lt_custom)
		for (i = 0; i < 3; i++)
			l->color[i] = min(128, color[i]); // i've seen color set in float form to 0.5 maximum and to 128 in byte form, so keep this tradition even i'm do not understand why they do so
	l->alpha = color[3];

	return l;
}

dlight_t *CL_AllocDlight (int key) {
	int i;
	dlight_t *dl;

	// first look for an exact key match
	if (key) {
		dl = cl_dlights;
		for (i = 0; i < MAX_DLIGHTS; i++, dl++) {
			if (dl->key == key) {
				memset (dl, 0, sizeof(*dl));
				dl->key = key;
				return dl;
			}
		}
	}

	// then look for anything else
	dl = cl_dlights;
	for (i = 0; i < MAX_DLIGHTS; i++, dl++) {
		if (dl->die < cl.time) {
			memset (dl, 0, sizeof(*dl));
			dl->key = key;
			return dl;
		}
	}

	dl = &cl_dlights[0];
	memset (dl, 0, sizeof(*dl));
	dl->key = key;
	return dl;
}


void CL_NewDlight (int key, vec3_t origin, float radius, float time, dlighttype_t type, int bubble) {
	dlight_t *dl;

	dl = CL_AllocDlight (key);
	VectorCopy (origin, dl->origin);
	dl->radius = radius;
	dl->die = cl.time + time;
	dl->type = type;
	dl->bubble = bubble;		
}

void CL_NewDlightEx (int key, vec3_t origin, float radius, float time, customlight_t *l, int bubble) {
// same as CL_NewDlight {
	dlight_t *dl;

	dl = CL_AllocDlight (key);
	VectorCopy (origin, dl->origin);
	dl->radius = radius;
	dl->die = cl.time + time;
	dl->type = l->type; // <= only here difference
	dl->bubble = bubble;
// }

	// and GL addon for custom color
	if (dl->type == lt_custom)
		VectorCopy(l->color, dl->color);
}

void CL_DecayLights (void) {
	int i;
	dlight_t *dl;

	if (cls.state < ca_active)
		return;

	dl = cl_dlights;
	for (i = 0; i < MAX_DLIGHTS; i++, dl++) {
		if (dl->die < cl.time || !dl->radius)
			continue;

		dl->radius -= cls.frametime * dl->decay;
		if (dl->radius < 0)
			dl->radius = 0;
	}
}

qbool NewLerp_AbleModel(int idx)
{
	return !cls.demoplayback && cl_newlerp.value && 
			(idx == cl_modelindices[mi_rocket] || idx == cl_modelindices[mi_grenade] || idx == cl_modelindices[mi_spike]);
}

void CL_SetupPacketEntity (int number, entity_state_t *state, qbool changed) {
	centity_t *cent;

	cent = &cl_entities[number];

	if (!cl.oldvalidsequence || cl.oldvalidsequence != cent->sequence ||
		state->modelindex != cent->current.modelindex ||
		!VectorL2Compare(state->origin, cent->current.origin, 200)
	) {
		cent->startlerp = cl.time;
		cent->deltalerp = -1;
		cent->frametime = -1;
		cent->oldsequence = 0;
		cent->flags = 0;
 	} else {
		cent->oldsequence = cent->sequence;
		
		if (state->frame != cent->current.frame) {
			cent->frametime = cl.time;
			cent->oldframe = cent->current.frame;
		}
		
		if (changed) {
			if (!VectorCompare(state->origin, cent->current.origin) || !VectorCompare(state->angles, cent->current.angles)) {
			    if (NewLerp_AbleModel(state->modelindex))
			    {
			    	float s  = (cls.mvdplayback ? nextdemotime - olddemotime : cl.time - cent->startlerp); // time delta
			    	float s2 = 1.0 / (s ? s : 1); // here no function which divide vector by X value, so we multiplay vector by 1/X
			    	vec3_t tmp, tmp2;
					VectorSubtract(state->origin, cent->current.origin, tmp); // origin delta, also move direction
					VectorScale(tmp, s2, tmp2); // we divide origin delta by time delta, that velocity in other words

					if (cent->deltalerp == -1) {
						VectorCopy(tmp2, cent->velocity); // we got first velocity for our enitity, just save it
					}
					else {
						VectorAdd(tmp2, cent->velocity, cent->velocity);
						VectorScale(cent->velocity, 0.5, cent->velocity); // (previous velocity + current) / 2, we got average velocity
					}

//					Com_Printf("v: %6.1f %6.1f %6.1f %f %f\n", VectorLength(tmp), VectorLength(tmp2), VectorLength(cent->velocity), s2, s);

					if (cent->deltalerp == -1) { // seems we get second update for enitity from server, just save some vectors
						VectorCopy(cent->current.origin, cent->old_origin);
						VectorCopy(cent->current.origin, cent->lerp_origin);
					}
					else {
						// its correction due to round/interploation, we have our own vision where object, and server vision,
						// use 90% of our vision and 10% of server, that make things smooth and somehow accurate
						VectorInterpolate(cent->lerp_origin, cl_newlerp.value, cent->current.origin, cent->old_origin);
					}
			    }
				else 
				{
					VectorCopy(cent->current.origin, cent->old_origin);
				}

				VectorCopy(cent->current.angles, cent->old_angles);
				if (cls.mvdplayback) {
					cent->deltalerp = nextdemotime - olddemotime;
					cent->startlerp = cls.demotime;
				} else {
					cent->deltalerp = cl.time - cent->startlerp;
					cent->startlerp = cl.time;
				}
			}
		}
	}
 
	cent->current = *state;
	cent->sequence = cl.validsequence;
}

//Can go from either a baseline or a previous packet_entity
void CL_ParseDelta (entity_state_t *from, entity_state_t *to, int bits) {
	int i;
#ifdef PROTOCOL_VERSION_FTE
	int morebits;
#endif

	// set everything to the state we are delta'ing from
	*to = *from;

	to->number = bits & 511;
	bits &= ~511;

	if (bits & U_MOREBITS) {	// read in the low order bits
		i = MSG_ReadByte ();
		bits |= i;
	}

#ifdef PROTOCOL_VERSION_FTE
	if (bits & U_FTE_EVENMORE && cls.fteprotocolextensions) {
		morebits = MSG_ReadByte ();
		if (morebits & U_FTE_YETMORE)
			morebits |= MSG_ReadByte()<<8;
	} else {
		morebits = 0;
	}
#endif

	to->flags = bits;

	if (bits & U_MODEL)
		to->modelindex = MSG_ReadByte ();

	if (bits & U_FRAME)
		to->frame = MSG_ReadByte ();

	if (bits & U_COLORMAP)
		to->colormap = MSG_ReadByte();

	if (bits & U_SKIN)
		to->skinnum = MSG_ReadByte();

	if (bits & U_EFFECTS)
		to->effects = MSG_ReadByte();

	if (bits & U_ORIGIN1)
		to->origin[0] = MSG_ReadCoord ();

	if (bits & U_ANGLE1)
		to->angles[0] = MSG_ReadAngle();

	if (bits & U_ORIGIN2)
		to->origin[1] = MSG_ReadCoord ();

	if (bits & U_ANGLE2)
		to->angles[1] = MSG_ReadAngle();

	if (bits & U_ORIGIN3)
		to->origin[2] = MSG_ReadCoord ();

	if (bits & U_ANGLE3)
		to->angles[2] = MSG_ReadAngle();

	if (bits & U_SOLID) {
		// FIXME
	}

#ifdef PROTOCOL_VERSION_FTE
#ifdef FTE_PEXT_TRANS
	if (morebits & U_FTE_TRANS && cls.fteprotocolextensions & FTE_PEXT_TRANS)
		to->trans = MSG_ReadByte();
#endif

#ifdef FTE_PEXT_ENTITYDBL
	if (morebits & U_FTE_ENTITYDBL)
		to->number += 512;
#endif
#ifdef FTE_PEXT_ENTITYDBL2
	if (morebits & U_FTE_ENTITYDBL2)
		to->number += 1024;
#endif
#ifdef FTE_PEXT_MODELDBL
	if (morebits & U_FTE_MODELDBL)
		to->modelindex += 256;
#endif
#endif
}

void FlushEntityPacket (void) {
	int word;
	entity_state_t olde, newe;

	Com_DPrintf ("FlushEntityPacket\n");

	memset (&olde, 0, sizeof(olde));

	cl.delta_sequence = 0;
	cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK].invalid = true;

	// read it all, but ignore it
	while (1) {
		word = (unsigned short) MSG_ReadShort ();
		if (msg_badread) {	// something didn't parse right...
			Host_Error ("msg_badread in packetentities");
			return;
		}

		if (!word)
			break;	// done

		CL_ParseDelta (&olde, &newe, word);
	}
}

// An svc_packetentities has just been parsed, deal with the rest of the data stream.
void CL_ParsePacketEntities (qbool delta) 
{
	int oldpacket, newpacket, oldindex, newindex, word, newnum, oldnum;
	packet_entities_t *oldp, *newp, dummy;
	qbool full;
	byte from;
	int maxentities = MAX_MVD_PACKET_ENTITIES; // allow as many as we can handle

	newpacket = cls.netchan.incoming_sequence & UPDATE_MASK;
	newp = &cl.frames[newpacket].packet_entities;
	cl.frames[newpacket].invalid = false;

	if (delta) 
	{
		from = MSG_ReadByte ();

		oldpacket = cl.frames[newpacket].delta_sequence;
		if (cls.mvdplayback)	
			from = oldpacket = cls.netchan.incoming_sequence - 1;

		if (cls.netchan.outgoing_sequence - cls.netchan.incoming_sequence >= UPDATE_BACKUP - 1) 
		{
			// There are no valid frames left, so drop it.
			FlushEntityPacket();
			cl.validsequence = 0;
			return;
		}

		if ((from & UPDATE_MASK) != (oldpacket & UPDATE_MASK)) 
		{
			Com_DPrintf ("WARNING: from mismatch\n");
			FlushEntityPacket();
			cl.validsequence = 0;
			return;
		}

		if (cls.netchan.outgoing_sequence - oldpacket >= UPDATE_BACKUP - 1) 
		{
			// We can't use this, it is too old.
			FlushEntityPacket ();
			// Don't clear cl.validsequence, so that frames can still be rendered;
			// it is possible that a fresh packet will be received before
			// (outgoing_sequence - incoming_sequence) exceeds UPDATE_BACKUP - 1
			return;
		}

		oldp = &cl.frames[oldpacket & UPDATE_MASK].packet_entities;
		full = false;
	} 
	else 
	{
		// This is a full update that we can start delta compressing from now.
		oldp = &dummy;
		dummy.num_entities = 0;
		full = true;
	}

	cl.oldvalidsequence = cl.validsequence;
	cl.validsequence = cls.netchan.incoming_sequence;
	cl.delta_sequence = cl.validsequence;

	oldindex = 0;
	newindex = 0;
	newp->num_entities = 0;

	while (1) 
	{
		word = (unsigned short) MSG_ReadShort ();
		if (msg_badread)
		{
			// something didn't parse right...
			Host_Error ("msg_badread in packetentities");
			return;
		}

		if (!word) 
		{
			while (oldindex < oldp->num_entities) 
			{
				// Copy all the rest of the entities from the old packet
				
				if (newindex >= maxentities)
					Host_Error ("CL_ParsePacketEntities: newindex == MAX_PACKET_ENTITIES");

				newp->entities[newindex] = oldp->entities[oldindex];
				CL_SetupPacketEntity(newp->entities[newindex].number, &newp->entities[newindex], false);
				newindex++;
				oldindex++;
			}
			break;
		}

		newnum = word & 511;
	
		#ifdef PROTOCOL_VERSION_FTE
		if (word & U_MOREBITS && (cls.fteprotocolextensions & FTE_PEXT_ENTITYDBL))
		{
			// Fte extensions for huge entity counts
			int oldpos = msg_readcount;
			int excessive;
			excessive = MSG_ReadByte();
			
			if (excessive & U_FTE_EVENMORE) 
			{
				excessive = MSG_ReadByte();
				#ifdef FTE_PEXT_ENTITYDBL
				if (excessive & U_FTE_ENTITYDBL)
					newnum += 512;
				#endif // FTE_PEXT_ENTITYDBL
				
				#ifdef FTE_PEXT_ENTITYDBL2
				if (excessive & U_FTE_ENTITYDBL2)
					newnum += 1024;
				#endif // FTE_PEXT_ENTITYDBL2
			}

			msg_readcount = oldpos; // undo the read...
		}
		#endif // PROTOCOL_VERSION_FTE

		oldnum = oldindex >= oldp->num_entities ? 9999 : oldp->entities[oldindex].number;

		while (newnum > oldnum)
		{
			if (full) 
			{
				Com_Printf ("WARNING: oldcopy on full update");
				FlushEntityPacket ();
				cl.validsequence = 0;	// can't render a frame
				return;
			}

			// Copy one of the old entities over to the new packet unchanged
			if (newindex >= maxentities)
				Host_Error ("CL_ParsePacketEntities: newindex == MAX_PACKET_ENTITIES");

			newp->entities[newindex] = oldp->entities[oldindex];
			CL_SetupPacketEntity(oldnum, &newp->entities[newindex], word > 511);
			newindex++;
			oldindex++;
			oldnum = oldindex >= oldp->num_entities ? 9999 : oldp->entities[oldindex].number;
		}

		if (newnum < oldnum) 
		{
			// New from baseline

			if (word & U_REMOVE) 
			{
				#ifdef PROTOCOL_VERSION_FTE
				if (word & U_MOREBITS && (cls.fteprotocolextensions & FTE_PEXT_ENTITYDBL))
				{
					if (MSG_ReadByte() & U_FTE_EVENMORE)
						MSG_ReadByte();
				}
				#endif // PROTOCOL_VERSION_FTE

				if (full) 
				{
					Com_Printf ("WARNING: U_REMOVE on full update\n");
					FlushEntityPacket ();
					cl.validsequence = 0;	// can't render a frame
					return;
				}
				continue;
			}

			if (newindex >= maxentities)
				Host_Error ("CL_ParsePacketEntities: newindex == MAX_PACKET_ENTITIES");

			CL_ParseDelta (&cl_entities[newnum].baseline, &newp->entities[newindex], word);
			CL_SetupPacketEntity (newnum, &newp->entities[newindex], word > 511); 
			newindex++;
			continue;
		}

		if (newnum == oldnum) 
		{
			// Delta from previous
			if (full) 
			{
				cl.validsequence = 0;
				cl.delta_sequence = 0;
				Com_Printf ("WARNING: delta on full update");
			}
			
			if (word & U_REMOVE) 
			{
				#ifdef PROTOCOL_VERSION_FTE
				if (word & U_MOREBITS && (cls.fteprotocolextensions & FTE_PEXT_ENTITYDBL))
				{
					if (MSG_ReadByte() & U_FTE_EVENMORE)
						MSG_ReadByte();
				}
				#endif // PROTOCOL_VERSION_FTE
				oldindex++;
				continue;
			}

			CL_ParseDelta (&oldp->entities[oldindex], &newp->entities[newindex], word);
			CL_SetupPacketEntity (newnum, &newp->entities[newindex], word > 511);
			newindex++;
			oldindex++;
		}
	}

	newp->num_entities = newindex;

	if (cls.state == ca_onserver)	// we can now render a frame
		CL_MakeActive();
}

static int fix_trail_num_for_grens(int trail_num)
{	
	//trails for rockets and grens are the same
	//only nums 1 and 2 must be swapped
	switch(trail_num)
	{
		case 1: trail_num = 2; break;
		case 2: trail_num = 1; break;
	}

	return trail_num;
}

static void missile_trail(int trail_num, model_t *model, vec3_t *old_origin, entity_t* ent, centity_t *cent)
{
	if (trail_num == 1)
		R_ParticleTrail (*old_origin, ent->origin, &cent->trail_origin, ROCKET_TRAIL);
	else if (trail_num == 2)
		R_ParticleTrail (*old_origin, ent->origin, &cent->trail_origin, GRENADE_TRAIL);
	else if (trail_num == 3)
		R_ParticleTrail (*old_origin, ent->origin, &cent->trail_origin, ALT_ROCKET_TRAIL);
	else if (trail_num == 4)
		R_ParticleTrail (*old_origin, ent->origin, &cent->trail_origin, BLOOD_TRAIL);
	else if (trail_num == 5)
		R_ParticleTrail (*old_origin, ent->origin, &cent->trail_origin, BIG_BLOOD_TRAIL);
	else if (trail_num == 6)
		R_ParticleTrail (*old_origin, ent->origin, &cent->trail_origin, TRACER1_TRAIL);
	else if (trail_num == 7)
		R_ParticleTrail (*old_origin, ent->origin, &cent->trail_origin, TRACER2_TRAIL);
	else if (trail_num == 8)
	{
		// VULT PARTICLES
		byte color[3];
		color[0] = 0; color[1] = 70; color[2] = 255;
		FireballTrail (*old_origin, ent->origin, &cent->trail_origin, color, 2, 0.5);
	}
	else if (trail_num == 9)
	{
		R_ParticleTrail (*old_origin, ent->origin, &cent->trail_origin, LAVA_TRAIL);
	}
	else if (trail_num == 10)
	{
		// VULT PARTICLES
		FuelRodGunTrail (*old_origin, ent->origin, ent->angles, &cent->trail_origin);
	}
	else if (trail_num == 11)
	{
		byte color[3];
		color[0] = 255; color[1] = 70; color[2] = 5;
		FireballTrailWave (*old_origin, ent->origin, &cent->trail_origin, color, 2, 0.5, ent->angles);
	}
	else if (trail_num == 12)
	{
		R_ParticleTrail (*old_origin, ent->origin, &cent->trail_origin, AMF_ROCKET_TRAIL);
	}
	else if (trail_num == 13)
	{
		CL_CreateBlurs (*old_origin, ent->origin, ent);
		VectorCopy(ent->origin, cent->trail_origin);		
	}
	else if (trail_num == 14)
	{
		R_ParticleTrail (*old_origin, ent->origin, &cent->trail_origin, RAIL_TRAIL);
	}
	else
	{
		R_ParticleTrail (*old_origin, ent->origin, &cent->trail_origin, GRENADE_TRAIL);
	}

}

// TODO: OMG SPLIT THIS UP!
void CL_LinkPacketEntities(void) 
{
	entity_t ent;
	centity_t *cent;
	packet_entities_t *pack;
	entity_state_t *state;
	model_t *model;
	vec3_t *old_origin;
	double time;
	float autorotate, lerp;
	int i, pnum, flicker;
	customlight_t cst_lt = {0};
	extern qbool cl_nolerp_on_entity_flag;

	pack = &cl.frames[cl.validsequence & UPDATE_MASK].packet_entities;

	autorotate = anglemod(100 * cl.time);

	memset(&ent, 0, sizeof(ent));

	for (pnum = 0; pnum < pack->num_entities; pnum++) 
	{
		state = &pack->entities[pnum];
		cent = &cl_entities[state->number];

		// Control powerup glow for bots.
		if (state->modelindex != cl_modelindices[mi_player] || r_powerupglow.value) 
		{
			flicker = r_lightflicker.value ? (rand() & 31) : 0;
			
			// Spawn light flashes, even ones coming from invisible objects.
			if ((state->effects & (EF_BLUE | EF_RED | EF_GREEN)) == (EF_BLUE | EF_RED | EF_GREEN)) 
			{
				CL_NewDlight (state->number, state->origin, 200 + flicker, 0.1, lt_white, 0);
			} 
			else if ((state->effects & (EF_BLUE | EF_RED)) == (EF_BLUE | EF_RED)) 
			{
				CL_NewDlight (state->number, state->origin, 200 + flicker, 0.1, lt_redblue, 0);
			} 
			else if ((state->effects & (EF_BLUE | EF_GREEN)) == (EF_BLUE | EF_GREEN)) 
			{
				CL_NewDlight (state->number, state->origin, 200 + flicker, 0.1, lt_bluegreen, 0);
			}
			else if ((state->effects & (EF_RED | EF_GREEN)) == (EF_RED | EF_GREEN)) 
			{
				CL_NewDlight (state->number, state->origin, 200 + flicker, 0.1, lt_redgreen, 0);
			} 
			else if (state->effects & EF_BLUE)
			{
				CL_NewDlight (state->number, state->origin, 200 + flicker, 0.1, lt_blue, 0);
			} 
			else if (state->effects & EF_RED) 
			{
				CL_NewDlight (state->number, state->origin, 200 + flicker, 0.1, lt_red, 0);
			}
			else if (state->effects & EF_GREEN) 
			{
				CL_NewDlight (state->number, state->origin, 200 + flicker, 0.1, lt_green, 0);
			}
			else if (state->effects & EF_BRIGHTLIGHT) 
			{
				vec3_t	tmp;
				VectorCopy (state->origin, tmp);
				tmp[2] += 16;
				CL_NewDlight (state->number, tmp, 400 + flicker, 0.1, lt_default, 0);
			}
			else if (state->effects & EF_DIMLIGHT)
			{
				qbool flagcolor = false;

				if (cl.teamfortress && (state->modelindex == cl_modelindices[mi_tf_flag] || state->modelindex == cl_modelindices[mi_tf_stan]))
					flagcolor = true;
				else if (state->modelindex == cl_modelindices[mi_flag])
					flagcolor = true;

				if (flagcolor)
				{
					dlightColorEx(r_flagcolor.value, r_flagcolor.string, lt_default, false, &cst_lt);
					CL_NewDlightEx(state->number, state->origin, 200 + flicker, 0.1, &cst_lt, 0);
				}
				else
				{
					CL_NewDlight(state->number, state->origin, 200 + flicker, 0.1, lt_default, 0);
				}
			}
		}

		if (!state->modelindex)		// If set to invisible, skip
			continue;

		ent.effects = state->effects; // Electro - added for shells

		if (state->modelindex == cl_modelindices[mi_player]) 
		{
			i = state->frame;

			if ((cl_deadbodyfilter.value == 3) & !cl.teamfortress)
			{ // will instantly filter dead bodies unless gamedir is fortress (because in TF you probably want to see dead bodies as they may be feigned spies)
					if (ISDEAD(i))
						continue;
			}
			if (cl_deadbodyfilter.value == 2)
			{
				if (ISDEAD(i))
					continue;
			}
			else if (cl_deadbodyfilter.value == 1) 
			{
				// These indices are the last animation of a death sequence in the player.mdl
				//49=axdeth9, 60=deatha11, 69=deathb9, 84=deathc15, 93=deathd9, 102=deathe9
				if (i == 49 || i == 60 || i == 69 || i == 84 || i == 93 || i == 102)
					continue;
			}
		}

		if (cl_gibfilter.value &&
			(state->modelindex == cl_modelindices[mi_h_player]
			|| state->modelindex == cl_modelindices[mi_gib1]
			|| state->modelindex == cl_modelindices[mi_gib2]
			|| state->modelindex == cl_modelindices[mi_gib3]))
			continue;

		if (!(model = cl.model_precache[state->modelindex]))
		{
			Host_Error ("CL_LinkPacketEntities: bad modelindex");
		}

		ent.model = model;

		if (state->modelindex == cl_modelindices[mi_rocket]) 
		{
			if (cl_rocket2grenade.value && cl_modelindices[mi_grenade] != -1)
				ent.model = cl.model_precache[cl_modelindices[mi_grenade]];
		}

		ent.skinnum = state->skinnum;

		if (ent.model->modhint == MOD_BACKPACK && cl_backpackfilter.value) 
		{
			continue;
		}

		ent.frame = state->frame;
		if (cent->frametime >= 0 && cent->frametime <= cl.time)
		{
			ent.oldframe = cent->oldframe;
			ent.framelerp = (cl.time - cent->frametime) * 10;
		} 
		else 
		{
			ent.oldframe = ent.frame;
			ent.framelerp = -1;
		}

		if (state->colormap >=1 && state->colormap <= MAX_CLIENTS
		&& (ent.model->modhint == MOD_PLAYER || (ent.renderfx & RF_PLAYERMODEL)))
		{
			ent.colormap = cl.players[state->colormap - 1].translations;
			ent.scoreboard = &cl.players[state->colormap - 1];
		}
		else
		{
			ent.colormap = vid.colormap;
			ent.scoreboard = NULL;
		}
	
		if (((cl_nolerp.value || cl_nolerp_on_entity_flag) && !cls.mvdplayback && !is_monster(state->modelindex))
			|| cent->deltalerp <= 0)
		{
			lerp = -1;
			VectorCopy(cent->current.origin, ent.origin);
		} 
		else 
		{
			time = cls.mvdplayback ? cls.demotime : cl.time;
			lerp = (time - cent->startlerp) / (cent->deltalerp);
			lerp = min(lerp, 1);

			if (NewLerp_AbleModel(cent->current.modelindex))
			{
				float d = time - cent->startlerp;

				if (d >= 2 * cent->deltalerp) // Seems enitity stopped move.
					VectorCopy(cent->lerp_origin, ent.origin);
				else // interpolate
					VectorMA(cent->old_origin, d, cent->velocity, ent.origin);
			}
			else
			{
				VectorInterpolate(cent->old_origin, lerp, cent->current.origin, ent.origin);
			}
		}
#if defined(FTE_PEXT_TRANS)
		//set trans
		ent.alpha = state->trans/255.0;
#endif

		if (ent.model->flags & EF_ROTATE)
		{
			ent.angles[0] = ent.angles[2] = 0;
			ent.angles[1] = autorotate;
			if (cl_model_bobbing.value)
				ent.origin[2] += sin(autorotate / 90 * M_PI) * 5 + 5;
		} 
		else 
		{
			if (lerp != -1) 
			{
				AngleInterpolate(cent->old_angles, lerp, cent->current.angles, ent.angles);
			}
			else 
			{
				VectorCopy(cent->current.angles, ent.angles);
			}
		}

		if (qmb_initialized) 
		{
			if (state->modelindex == cl_modelindices[mi_explod1] || state->modelindex == cl_modelindices[mi_explod2]) 
			{
				if (gl_part_inferno.value) 
				{
					QMB_InfernoFlame (ent.origin);
					continue;
				}
			}

			if (state->modelindex == cl_modelindices[mi_bubble]) 
			{
				QMB_StaticBubble (&ent);
				continue;
			}
		}

		// Add trails
		if (model->flags & ~EF_ROTATE || model->modhint) 
		{
			if (!(cent->flags & CENT_TRAILDRAWN) || !VectorL2Compare(cent->trail_origin, ent.origin, 140)) 
			{
				old_origin = &ent.origin;	// Not present last frame or too far away
				cent->flags |= CENT_TRAILDRAWN;
			} 
			else
			{
				old_origin = &cent->trail_origin;
			}

			CL_AddParticleTrail(&ent, cent, old_origin, &cst_lt, state);
		}
		VectorCopy (ent.origin, cent->lerp_origin);

		CL_AddEntity (&ent);
	}
}



typedef struct 
{
	int		modelindex;
	vec3_t	origin;
	vec3_t	angles;
	int		num;	
} projectile_t;

projectile_t	cl_projectiles[MAX_PROJECTILES];
int				cl_num_projectiles;

projectile_t	cl_oldprojectiles[MAX_PROJECTILES];		
int				cl_num_oldprojectiles;					

void CL_ClearProjectiles (void) 
{
	if (cls.mvdplayback) 
	{
		static int parsecount = 0;

		if (parsecount == cl.parsecount)
			return;

		parsecount = cl.parsecount;
		memset(cl.int_projectiles, 0, sizeof(cl.int_projectiles));
		cl_num_oldprojectiles = cl_num_projectiles;
		memcpy(cl_oldprojectiles, cl_projectiles, sizeof(cl_projectiles));
	}

	cl_num_projectiles = 0;
}

// Nails are passed as efficient temporary entities
void CL_ParseProjectiles (qbool indexed) 
{
	int i, c, j, num;
	byte bits[6];
	projectile_t *pr;
	interpolate_t *int_projectile;	

	c = MSG_ReadByte ();
	for (i = 0; i < c; i++) 
	{
		num = indexed ? MSG_ReadByte() : 0;	

		for (j = 0 ; j < 6 ; j++)
			bits[j] = MSG_ReadByte ();

		if (cl_num_projectiles == MAX_PROJECTILES)
			continue;

		pr = &cl_projectiles[cl_num_projectiles];
		int_projectile = &cl.int_projectiles[cl_num_projectiles];	
		cl_num_projectiles++;

		pr->modelindex = cl_modelindices[mi_spike];
		pr->origin[0] = (( bits[0] + ((bits[1] & 15) << 8)) << 1) - 4096;
		pr->origin[1] = (((bits[1] >> 4) + (bits[2] << 4)) << 1) - 4096;
		pr->origin[2] = ((bits[3] + ((bits[4] & 15) << 8)) << 1) - 4096;
		pr->angles[0] = 360 * (bits[4] >> 4) / 16;
		pr->angles[1] = 360 * bits[5] / 256;

		if (!(pr->num = num))
			continue;

		for (j = 0; j < cl_num_oldprojectiles; j++) 
		{
			if (cl_oldprojectiles[j].num == num)
			{
				int_projectile->interpolate = true;
				int_projectile->oldindex = j;
				VectorCopy(pr->origin, int_projectile->origin);
				break;
			}
		}		
	}	
}

void CL_LinkProjectiles (void) 
{
	int i;
	projectile_t *pr;
	entity_t ent;

	memset(&ent, 0, sizeof(entity_t));
	ent.colormap = vid.colormap;

	for (i = 0, pr = cl_projectiles; i < cl_num_projectiles; i++, pr++)	
	{
		if (pr->modelindex < 1)
			continue;
		ent.model = cl.model_precache[pr->modelindex];
		VectorCopy(pr->origin, ent.origin);
		VectorCopy(pr->angles, ent.angles);
		CL_AddEntity(&ent);
	}
}

void SetupPlayerEntity(int num, player_state_t *state) 
{
	centity_t *cent;

	cent = &cl_entities[num];

	if (!cl.oldparsecount || cl.oldparsecount != cent->sequence ||
		state->modelindex != cent->current.modelindex ||
		!VectorL2Compare(state->origin, cent->current.origin, 200)) 
	{
		cent->startlerp = cl.time;
		cent->deltalerp = -1;
		cent->frametime = -1;
	} 
	else 
	{
		
		if (state->frame != cent->current.frame) 
		{
			cent->frametime = cl.time;
			cent->oldframe = cent->current.frame;
			if (state->vw_index == cent->old_vw_index)
				cent->old_vw_frame = cent->oldframe;
			else
				cent->old_vw_frame = state->frame;	// no lerping if vwep model changed
		}

		if (!VectorCompare(state->origin, cent->current.origin) || !VectorCompare(state->viewangles, cent->current.angles)) {
			VectorCopy(cent->current.origin, cent->old_origin);
			VectorCopy(cent->current.angles, cent->old_angles);
			if (cls.mvdplayback) 
			{
				cent->deltalerp = nextdemotime - olddemotime;
				cent->startlerp = cls.demotime;
			} 
			else
			{
				cent->deltalerp = cl.time - cent->startlerp;
				cent->startlerp = cl.time;
			}
		}
	}

	VectorCopy(state->origin, cent->current.origin);
	VectorCopy(state->viewangles, cent->current.angles);
	cent->current.frame = state->frame;
	cent->sequence = state->messagenum;	
	cent->current.modelindex = state->modelindex;
	cent->old_vw_index = state->vw_index;
}

extern int parsecountmod;
extern double parsecounttime;

player_state_t oldplayerstates[MAX_CLIENTS];	

static int MVD_WeaponModelNumber (int cweapon)
{
	int i;

	for (i = 0; i < 8; i++)
	{
		if (cweapon == cl_modelindices[mi_weapon1 + i])
			return i + 1;
	}

	return 0;
}

void CL_ParsePlayerinfo (void) 
{
	extern cvar_t cl_fix_mvd;
	int	msec=0, flags, pm_code;
	centity_t *cent;
	player_info_t *info;
	player_state_t *state;

	player_state_t *prevstate, dummy;
	int num, i;

	extern void TP_ParsePlayerInfo(player_state_t *, player_state_t *, player_info_t *info);

	num = MSG_ReadByte ();
	if (num >= MAX_CLIENTS)
		Host_Error ("CL_ParsePlayerinfo: num >= MAX_CLIENTS");

	info = &cl.players[num];
	state = &cl.frames[parsecountmod].playerstate[num];
	cent = &cl_entities[num + 1];

	if (cls.mvdplayback)
	{
		if (!cl.parsecount || cent->sequence > cl.parsecount || cl.parsecount - cent->sequence >= UPDATE_BACKUP - 1)
	 	{
			memset(&dummy, 0, sizeof(dummy));
			dummy.pm_type = PM_SPECTATOR; // so camera able to fly
			prevstate = &dummy;
		}
		else
		{
			prevstate = &cl.frames[cent->sequence & UPDATE_MASK].playerstate[num];
		}

		memcpy(state, prevstate, sizeof(player_state_t));

		if (cls.findtrack && info->name[0] && !info->spectator)
		{
			extern int ideal_track;
			autocam = CAM_TRACK;
			Cam_Lock(num);
			ideal_track = num;
			cls.findtrack = false;
		}

		flags = MSG_ReadShort ();
		state->flags = MVD_TranslateFlags(flags);

		state->messagenum = cl.parsecount;

		state->frame = MSG_ReadByte ();

		state->state_time = parsecounttime;
		state->command.msec = 0;

		for (i = 0; i < 3; i++) 
		{
			if (flags & (DF_ORIGIN << i))
				state->origin[i] = MSG_ReadCoord ();
		}

		for (i = 0; i < 3; i++) 
		{
			if (flags & (DF_ANGLES << i))
				state->command.angles[i] = MSG_ReadAngle16 ();
		}

		if (flags & DF_MODEL)
			state->modelindex = MSG_ReadByte ();
		else // check for possible bug in mvd/qtv
		{
			if (cl_fix_mvd.integer && !state->modelindex && !cl.players[num].spectator && cl_modelindices[mi_player] != -1)
				state->modelindex = cl_modelindices[mi_player];
		}

		if (flags & DF_SKINNUM)
			state->skinnum = MSG_ReadByte ();

		if (flags & DF_EFFECTS)
			state->effects = MSG_ReadByte ();

		if (flags & DF_WEAPONFRAME)
			state->weaponframe = MSG_ReadByte ();

		if (cl.vwep_enabled && !(state->flags & PF_GIB)
		&& state->modelindex == cl_modelindices[mi_player] /* no vweps for ring! */)
			state->vw_index = MVD_WeaponModelNumber(info->stats[STAT_WEAPON]);
		else
			state->vw_index = 0;
	} 
	else 
	{
		flags = state->flags = MSG_ReadShort ();

		state->messagenum = cl.parsecount;
		state->origin[0] = MSG_ReadCoord ();
		state->origin[1] = MSG_ReadCoord ();
		state->origin[2] = MSG_ReadCoord ();

		state->frame = MSG_ReadByte ();

		// the other player's last move was likely some time before the packet was sent out,
		// so accurately track the exact time it was valid at
		if (flags & PF_MSEC) 
		{
			msec = MSG_ReadByte ();
			state->state_time = parsecounttime - msec * 0.001;
		} 
		else
		{
			state->state_time = parsecounttime;
		}

		if (flags & PF_COMMAND) 
		{
			MSG_ReadDeltaUsercmd (&nullcmd, &state->command, cl.protoversion);
			CL_CalcPlayerFPS(info, state->command.msec);
		}
		else
		{
			if (num != cl.playernum)	
				info->fps = -1;
		}

		if (cl.z_ext & Z_EXT_VWEP && !(state->flags & PF_GIB))
			state->vw_index = state->command.impulse;
		else
			state->vw_index = 0;

		for (i = 0; i < 3; i++)
		{
			if (flags & (PF_VELOCITY1 << i) )
				state->velocity[i] = MSG_ReadShort();
			else
				state->velocity[i] = 0;
		}
		if (flags & PF_MODEL)
			state->modelindex = MSG_ReadByte ();
		else
			state->modelindex = cl_modelindices[mi_player];

		if (flags & PF_SKINNUM)
			state->skinnum = MSG_ReadByte ();
		else
			state->skinnum = 0;

		if (flags & PF_EFFECTS)
			state->effects = MSG_ReadByte ();
		else
			state->effects = 0;

		if (flags & PF_WEAPONFRAME)
			state->weaponframe = MSG_ReadByte ();
		else
			state->weaponframe = 0;


		state->alpha = 255;
#ifdef FTE_PEXT_TRANS
		if (flags & PF_TRANS_Z && cls.fteprotocolextensions & FTE_PEXT_TRANS)
			state->alpha = MSG_ReadByte();
#endif

		if (cl.z_ext & Z_EXT_PM_TYPE)
		{
			pm_code = (flags >> PF_PMC_SHIFT) & PF_PMC_MASK;
			if (pm_code == PMC_NORMAL || pm_code == PMC_NORMAL_JUMP_HELD)
			{
				if (flags & PF_DEAD)
				{
					state->pm_type = PM_DEAD;
				}
				else
				{
					state->pm_type = PM_NORMAL;
					state->jump_held = (pm_code == PMC_NORMAL_JUMP_HELD);
				}
			}
			else
				if (pm_code == PMC_OLD_SPECTATOR)
					state->pm_type = PM_OLD_SPECTATOR;
				else
				{
					if (cl.z_ext & Z_EXT_PM_TYPE_NEW)
					{
						switch (pm_code)
						{
							case PMC_SPECTATOR:
								state->pm_type = PM_SPECTATOR;
								break;
							case PMC_FLY:
								state->pm_type = PM_FLY;
								break;
							case PMC_NONE:
								state->pm_type = PM_NONE;
								break;
							case PMC_LOCK:
								state->pm_type = PM_LOCK;
								break;
							default:
								// future extension?
								goto guess_pm_type;
						}
					}
					else
					{
						// future extension?
						goto guess_pm_type;
					}
				}
		}
		else
		{
guess_pm_type:
			if (cl.players[num].spectator)
				state->pm_type = PM_OLD_SPECTATOR;
			else if (flags & PF_DEAD)
				state->pm_type = PM_DEAD;
			else
				state->pm_type = PM_NORMAL;
		}

		if (cls.demorecording)
		{
			// Write out here - generally packets are copied, but flags for userdelta were changed
			//   in protocol 27 - we always write out in new format
			MSG_WriteByte(&cls.demomessage, num);
			MSG_WriteShort(&cls.demomessage, flags);
			MSG_WriteCoord(&cls.demomessage, state->origin[0]);
			MSG_WriteCoord(&cls.demomessage, state->origin[1]);
			MSG_WriteCoord(&cls.demomessage, state->origin[2]);
			MSG_WriteByte(&cls.demomessage, state->frame);
			if (flags & PF_MSEC)
				MSG_WriteByte(&cls.demomessage, msec);
			if (flags & PF_COMMAND)
				MSG_WriteDeltaUsercmd(&cls.demomessage, &nullcmd, &state->command);
			for (i = 0; i < 3; i++)
				if (flags & (PF_VELOCITY1 << i) )
					MSG_WriteShort(&cls.demomessage, state->velocity[i]);
			if (flags & PF_MODEL)
				MSG_WriteByte(&cls.demomessage, state->modelindex);
			if (flags & PF_SKINNUM)
				MSG_WriteByte(&cls.demomessage, state->skinnum);
			if (flags & PF_EFFECTS)
				MSG_WriteByte(&cls.demomessage, state->effects);
			if (flags & PF_WEAPONFRAME)
				MSG_WriteByte(&cls.demomessage, state->weaponframe);
#ifdef FTE_PEXT_TRANS
			if (flags & PF_TRANS_Z && cls.fteprotocolextensions & FTE_PEXT_TRANS)
				MSG_WriteByte(&cls.demomessage, state->alpha);		
#endif
		}
	}

	if (cl.z_ext & Z_EXT_PF_ONGROUND)
		state->onground = (flags & PF_ONGROUND) != 0;
	else
		state->onground = false;

	if (!(cl.z_ext & Z_EXT_PF_SOLID))
	{	// the PF_SOLID bit is unsupported, use our best guess
		if (cl.players[num].spectator || (state->flags & PF_DEAD))
			state->flags &= ~PF_SOLID;
		else
			state->flags |= PF_SOLID;
	}

	VectorCopy (state->command.angles, state->viewangles);
	
	TP_ParsePlayerInfo(&oldplayerstates[num], state, info);
	oldplayerstates[num] = *state;
	
	SetupPlayerEntity(num + 1, state);
}

// Called when the CTF flags are set
void CL_AddFlagModels (entity_t *ent, int team) 
{
	int i;
	float f;
	vec3_t v_forward, v_right;
	entity_t newent;

	if (cl_modelindices[mi_flag] == -1)
		return;

	f = 14;
	if (ent->frame >= 29 && ent->frame <= 40) {
		if (ent->frame >= 29 && ent->frame <= 34) { //axpain
			if      (ent->frame == 29) f = f + 2; 
			else if (ent->frame == 30) f = f + 8;
			else if (ent->frame == 31) f = f + 12;
			else if (ent->frame == 32) f = f + 11;
			else if (ent->frame == 33) f = f + 10;
			else if (ent->frame == 34) f = f + 4;
		} else if (ent->frame >= 35 && ent->frame <= 40) { // pain
			if      (ent->frame == 35) f = f + 2; 
			else if (ent->frame == 36) f = f + 10;
			else if (ent->frame == 37) f = f + 10;
			else if (ent->frame == 38) f = f + 8;
			else if (ent->frame == 39) f = f + 4;
			else if (ent->frame == 40) f = f + 2;
		}
	} else if (ent->frame >= 103 && ent->frame <= 118) {
		if      (ent->frame >= 103 && ent->frame <= 104) f = f + 6;  //nailattack
		else if (ent->frame >= 105 && ent->frame <= 106) f = f + 6;  //light 
		else if (ent->frame >= 107 && ent->frame <= 112) f = f + 7;  //rocketattack
		else if (ent->frame >= 112 && ent->frame <= 118) f = f + 7;  //shotattack
	}

	memset (&newent, 0, sizeof(entity_t));
	newent.model = cl.model_precache[cl_modelindices[mi_flag]];
	newent.skinnum = team;
	newent.colormap = vid.colormap;

	AngleVectors (ent->angles, v_forward, v_right, NULL);
	v_forward[2] = -v_forward[2]; // reverse z component
	for (i = 0; i < 3; i++)
		newent.origin[i] = ent->origin[i] - f * v_forward[i] + 22 * v_right[i];
	newent.origin[2] -= 16;

	VectorCopy (ent->angles, newent.angles);
	newent.angles[2] -= 45;

	CL_AddEntity (&newent);
}

/*
================
CL_AddVWepModel
================
*/
static qbool CL_AddVWepModel (entity_t *ent, int vw_index, int old_vw_frame)
{
	entity_t	newent;

	if ((unsigned)vw_index >= MAX_VWEP_MODELS)
		return false;

	if (cl.vw_model_name[vw_index][0] == '-')
		return true;	// empty vwep model

	if (!cl.vw_model_precache[vw_index])
		return false;	// vwep model not present - draw default player.mdl

	// build the weapon entity
	memset (&newent, 0, sizeof(entity_t));
	VectorCopy (ent->origin, newent.origin);
	VectorCopy (ent->angles, newent.angles);
	newent.model = cl.vw_model_precache[vw_index];
	newent.frame = ent->frame;
	newent.oldframe = old_vw_frame;
	newent.framelerp = ent->framelerp;
	newent.skinnum = 0;
	newent.colormap = vid.colormap;
	newent.renderfx |= RF_PLAYERMODEL;	// not really, but use same lighting rules
	newent.effects = ent->effects; // Electro - added for shells

	CL_AddEntity (&newent);
	return true;
}

static double CL_PlayerTime (void)
{
	double current_time = (cls.demoplayback && !cls.mvdplayback) ? cls.demotime : cls.realtime;
	double playertime = current_time - cls.latency + 0.02;

	return min (playertime, current_time);
}

// Create visible entities in the correct position for all current players
void CL_LinkPlayers (void) 
{
	int j, msec, i, flicker, oldphysent;
	float *org;
	vec3_t	tmp;
	double playertime = CL_PlayerTime();
	player_info_t *info;
	player_state_t *state, exact;
	entity_t ent;
	centity_t *cent;
	frame_t *frame;
	customlight_t cst_lt = {0};

	frame = &cl.frames[cl.parsecount & UPDATE_MASK];
	memset (&ent, 0, sizeof(entity_t));

	for (j = 0; j < MAX_CLIENTS; j++, info++, state++) 
	{
		info = &cl.players[j];
		state = &frame->playerstate[j];
		cent = &cl_entities[j + 1];

		if (state->messagenum != cl.parsecount)
			continue;	// not present this frame

		// spawn light flashes, even ones coming from invisible objects
		if (r_powerupglow.value && !(r_powerupglow.value == 2 && j == cl.viewplayernum)) 
		{
			org = (j == cl.playernum) ? cl.simorg : state->origin;
			flicker = r_lightflicker.value ? (rand() & 31) : 0;

			if ((state->effects & (EF_BLUE | EF_RED | EF_GREEN)) == (EF_BLUE | EF_RED | EF_GREEN)) 
			{
				CL_NewDlight (j + 1, org, 200 + flicker, 0.1, lt_white, 0);
			} 
			else if ((state->effects & (EF_BLUE | EF_RED)) == (EF_BLUE | EF_RED)) 
			{
				CL_NewDlight (j + 1, org, 200 + flicker, 0.1, lt_redblue, 0);
			}
			else if ((state->effects & (EF_BLUE | EF_GREEN)) == (EF_BLUE | EF_GREEN)) 
			{
				CL_NewDlight (j + 1, org, 200 + flicker, 0.1, lt_bluegreen, 0);
			} 
			else if ((state->effects & (EF_RED | EF_GREEN)) == (EF_RED | EF_GREEN)) 
			{
				CL_NewDlight (j + 1, org, 200 + flicker, 0.1, lt_redgreen, 0);
			} 
			else if (state->effects & EF_BLUE) 
			{
				CL_NewDlight (j + 1, org, 200 + flicker, 0.1, lt_blue, 0);
			}
			else if (state->effects & EF_RED) 
			{
				CL_NewDlight (j + 1, org, 200 + flicker, 0.1, lt_red, 0);
			}
			else if (state->effects & EF_GREEN) 
			{
				CL_NewDlight (j + 1, org, 200 + flicker, 0.1, lt_green, 0);
			} 
			else if (state->effects & EF_BRIGHTLIGHT) 
			{
				VectorCopy (org, tmp);
				tmp[2] += 16;
				CL_NewDlight (j + 1, tmp, 400 + flicker, 0.1, lt_default, 0);
			}
			else if (state->effects & EF_DIMLIGHT)
			{
				qbool flagcolor = false;
				if (cl.teamfortress)	
					flagcolor = true;
				else if (state->effects & (EF_FLAG1|EF_FLAG2))
					flagcolor = true;

				if (flagcolor) {
					dlightColorEx(r_flagcolor.value, r_flagcolor.string, lt_default, false, &cst_lt);
					CL_NewDlightEx(j + 1, org, 200 + flicker, 0.1, &cst_lt, 0);
				}
				else
					CL_NewDlight(j + 1, org, 200 + flicker, 0.1, lt_default, 0);
			}
		}

		// VULT MOTION TRAILS
		ent.alpha = 0;
		// The player object never gets added.
		if (j == cl.playernum) {
			// VULT CAMERAS
			if (cameratype != C_NORMAL && !cl.spectator) {
				ent.alpha = -1;
			}
			else {
				continue;
			}
		}

		if (!Cam_DrawPlayer(j)) {
			continue;
		}

		if (!state->modelindex)
			continue;

		if (state->modelindex == cl_modelindices[mi_player]) 
		{
			i = state->frame;


			if ((cl_deadbodyfilter.value == 3) & !cl.teamfortress)
			{ // will instantly filter dead bodies unless gamedir is fortress (because in TF you probably want to see dead bodies as they may be feigned spies)
					if (ISDEAD(i))
						continue;
			}
			if (cl_deadbodyfilter.value == 2) 
			{
				if (ISDEAD(i))
					continue;
			} 
			else if (cl_deadbodyfilter.value == 1) 
			{
				// These indices are the last animation of a death sequence in the player.mdl
				//49=axdeth9, 60=deatha11, 69=deathb9, 84=deathc15, 93=deathd9, 102=deathe9
				if (i == 49 || i == 60 || i == 69 || i == 84 || i == 93 || i == 102)
					continue;
			}
		}

		if (!(ent.model = cl.model_precache[state->modelindex]))
		{
			Host_Error ("CL_LinkPlayers: bad modelindex");
		}
		else if (state->modelindex == cl_modelindices[mi_player] && ISDEAD(state->frame)
				&& cl.vw_model_precache[0] && r_drawvweps.value
				&& cls.mvdplayback) // For non-mvd, let the server decide 
		{
			ent.model = cl.vw_model_precache[0];
		}

		ent.skinnum = state->skinnum;
		ent.colormap = info->translations;
		ent.scoreboard = (state->modelindex == cl_modelindices[mi_player]) ? info : NULL;
		ent.frame = state->frame;
		ent.effects = state->effects; // Electro - added for shells

		if (cent->frametime >= 0 && cent->frametime <= cl.time) 
		{
			ent.oldframe = cent->oldframe;
			ent.framelerp = (cl.time - cent->frametime) * 10;
		} 
		else 
		{
			ent.oldframe = ent.frame;
			ent.framelerp = -1;
		}

		// angles
		ent.angles[PITCH] = -state->viewangles[PITCH] / 3;
		ent.angles[YAW] = state->viewangles[YAW];
		ent.angles[ROLL] = 0;
		ent.angles[ROLL] = 4 * V_CalcRoll (ent.angles, state->velocity);

		// only predict half the move to minimize overruns
		msec = (cl_predict_half.value ? 500 : 1000) * (playertime - state->state_time);
		if (msec <= 0 || !cl_predict_players.value || cls.mvdplayback)
		{		
			VectorCopy (state->origin, ent.origin);
		} 
		else
		{
			// predict players movement
			if (msec > 255)
				msec = 255;
			state->command.msec = msec;

			oldphysent = pmove.numphysent;
			CL_SetSolidPlayers (j);
			CL_PredictUsercmd (state, &exact, &state->command);
			pmove.numphysent = oldphysent;
			VectorCopy (exact.origin, ent.origin);
		}

		if (state->effects & (EF_FLAG1|EF_FLAG2))
			CL_AddFlagModels (&ent, !!(state->effects & EF_FLAG2));

		// VULT CAMERAS
		if (j == cl.playernum)
		{
			if (cl.stats[STAT_HEALTH] <= 0)
			{
				VectorCopy(state->viewangles, ent.angles);
				//This doesn't work... But no one has noticed yet :D
			}
			else
				VectorCopy(cl.simangles, ent.angles);
			ent.angles[0] = state->viewangles[0];
			VectorCopy (cl.simorg, ent.origin);
		}

		VectorCopy (ent.origin, cent->lerp_origin);

		// VULT MOTION TRAILS
		if (amf_motiontrails_wtf.value)
			CL_CreateBlurs (state->origin, ent.origin, &ent);

		// VULT DEATH EFFECT
		if (amf_part_deatheffect.value)
		{
			if (ISDEAD(state->frame) && amf_part_deatheffect.value == 2)
			{
				if (info->dead == false) //dead effect not played yet
				{
					//deatheffect
					VX_DeathEffect (state->origin);
					info->dead = true;
				}
			}
			else if (state->modelindex == cl_modelindices[mi_h_player])
			{
				if (info->dead == false)
				{
					//gibeffect
					VX_GibEffect (state->origin);
					info->dead = true;
				}
			}
			else
			{
				info->dead = false;
			}
		}

		if ((cl.vwep_enabled && r_drawvweps.value && state->vw_index) && (state->modelindex != cl_modelindices[mi_eyes])) 
		{
			qbool vwep;
			vwep = CL_AddVWepModel (&ent, state->vw_index, cent->old_vw_frame);
			if (vwep) 
			{
				if (cl.vw_model_name[0][0] != '-') 
				{
					ent.model = cl.vw_model_precache[0];
					ent.renderfx |= RF_PLAYERMODEL;
					CL_AddEntity (&ent);
				}
				else 
				{
					// server said don't add vwep player model
				}
			}
			else
				CL_AddEntity (&ent);
		}
		else
			CL_AddEntity (&ent);
	}
}

// Builds all the pmove physents for the current frame
void CL_SetSolidEntities (void)
{
	int i;
	frame_t	*frame;
	packet_entities_t *pak;
	entity_state_t *state;

	pmove.physents[0].model = cl.clipmodels[1];
	VectorClear (pmove.physents[0].origin);
	pmove.physents[0].info = 0;
	pmove.numphysent = 1;

	frame = &cl.frames[parsecountmod];
	pak = &frame->packet_entities;

	for (i = 0; i < pak->num_entities; i++) 
	{
		state = &pak->entities[i];

		if (!state->modelindex)
			continue;

		if (cl.clipmodels[state->modelindex])
		{
			if (pmove.numphysent == MAX_PHYSENTS)
				break;

			pmove.physents[pmove.numphysent].model = cl.clipmodels[state->modelindex];
			VectorCopy (state->origin, pmove.physents[pmove.numphysent].origin);
			pmove.numphysent++;
		}
	}
}

// Calculate the new position of players, without other player clipping
//
// We do this to set up real player prediction.
// Players are predicted twice, first without clipping other players,
// then with clipping against them.
// This sets up the first phase.
void CL_SetUpPlayerPrediction(qbool dopred)
{
	int j, msec;
	player_state_t *state, exact;
	double playertime = CL_PlayerTime();
	frame_t *frame;
	struct predicted_player *pplayer;

#ifdef EXPERIMENTAL_SHOW_ACCELERATION
	extern qbool flag_player_pmove;
#endif

	frame = &cl.frames[cl.parsecount & UPDATE_MASK];

	for (j = 0; j < MAX_CLIENTS; j++) 
	{
		pplayer = &predicted_players[j];
		state = &frame->playerstate[j];

		pplayer->active = false;

		if (state->messagenum != cl.parsecount)
			continue;	// not present this frame

		if (!state->modelindex)
			continue;

		pplayer->active = true;
		pplayer->flags = state->flags;

#ifdef EXPERIMENTAL_SHOW_ACCELERATION
		flag_player_pmove = (j == cl.playernum);
#endif
		// note that the local player is special, since he moves locally we use his last predicted postition
		if (j == cl.playernum) 
		{
			VectorCopy(cl.frames[cls.netchan.outgoing_sequence & UPDATE_MASK].playerstate[cl.playernum].origin, pplayer->origin);
		} 
		else 
		{
			msec = (cl_predict_half.value ? 500 : 1000) * (playertime - state->state_time);
			if (msec <= 0 || !cl_predict_players.value || !dopred || cls.mvdplayback) 
			{ 
				VectorCopy (state->origin, pplayer->origin);
			} 
			else 
			{
				// Predict players movement
				if (msec > 255)
					msec = 255;
				state->command.msec = msec;

				CL_PredictUsercmd (state, &exact, &state->command);
				VectorCopy (exact.origin, pplayer->origin);
			}
		}
	}
}

// Builds all the pmove physents for the current frame.
// Note that CL_SetUpPlayerPrediction() must be called first!
// pmove must be setup with world and solid entity hulls before calling (via CL_PredictMove)
void CL_SetSolidPlayers (int playernum)
{
	int j;
	struct predicted_player *pplayer;
	physent_t *pent;
	extern vec3_t player_mins, player_maxs;

	if (!cl_solid_players.value)
		return;

	pent = pmove.physents + pmove.numphysent;

	for (j = 0; j < MAX_CLIENTS; j++) 
	{
		pplayer = &predicted_players[j];
	
		if (pmove.numphysent == MAX_PHYSENTS)
			break;

		if (!pplayer->active)
			continue;	// not present this frame

		// the player object never gets added
		if (j == playernum)
			continue;

		if (!(pplayer->flags & PF_SOLID))
			continue;

		pent->model = 0;
		VectorCopy(pplayer->origin, pent->origin);
		VectorCopy(player_mins, pent->mins);
		VectorCopy(player_maxs, pent->maxs);
		pmove.numphysent++;
		pent++;
	}
}

// Builds the visedicts array for cl.time
// Made up of: clients, packet_entities, nails, and tents
void CL_EmitEntities (void) 
{
	if (cls.state != ca_active)
		return;

	if (cls.demoseeking)
		return;

	if (!cl.validsequence && !cls.nqdemoplayback)
		return;

	CL_ClearScene ();
	
	if (cls.nqdemoplayback)
		NQD_LinkEntities ();
	else 
	{
		CL_LinkPlayers();
		CL_LinkPacketEntities();
		CL_LinkProjectiles();
	}

	CL_UpdateTEnts();
}

int	mvd_fixangle;

static int MVD_TranslateFlags(int src) 
{
	int dst = 0;

	if (src & DF_EFFECTS)
		dst |= PF_EFFECTS;
	if (src & DF_SKINNUM)
		dst |= PF_SKINNUM;
	if (src & DF_DEAD)
		dst |= PF_DEAD;
	if (src & DF_GIB)
		dst |= PF_GIB;
	if (src & DF_WEAPONFRAME)
		dst |= PF_WEAPONFRAME;
	if (src & DF_MODEL)
		dst |= PF_MODEL;

	return dst;
}

static float MVD_AdjustAngle(float current, float ideal, float fraction) {
	float move;

	move = ideal - current;
	if (move >= 180)
		move -= 360;
	else if (move <= -180)
		move += 360;

	return current + fraction * move;
}

static void MVD_InitInterpolation(void) {
	player_state_t *state, *oldstate;
	int i, tracknum;
	frame_t	*frame, *oldframe;
	vec3_t dist;
	struct predicted_player *pplayer;

	if (!cl.validsequence)
		 return;

	if (nextdemotime <= olddemotime)
		return;

	frame = &cl.frames[cl.parsecount & UPDATE_MASK];
	oldframe = &cl.frames[cl.oldparsecount & UPDATE_MASK];

	// clients
	for (i = 0; i < MAX_CLIENTS; i++) {
		pplayer = &predicted_players[i];
		state = &frame->playerstate[i];
		oldstate = &oldframe->playerstate[i];

		if (pplayer->predict) {
			VectorCopy(pplayer->oldo, oldstate->origin);
			VectorCopy(pplayer->olda, oldstate->command.angles);
			VectorCopy(pplayer->oldv, oldstate->velocity);
		}

		pplayer->predict = false;

		tracknum = Cam_TrackNum();
		if (mvd_fixangle & (1 << i)) {
			if (i == tracknum) {
				VectorCopy(cl.viewangles, state->command.angles);
				VectorCopy(cl.viewangles, state->viewangles);
			}

			// no angle interpolation
			VectorCopy(state->command.angles, oldstate->command.angles);

			mvd_fixangle &= ~(1 << i);
		}

		// we dont interpolate ourself if we are spectating
		if (i == cl.playernum && cl.spectator)
			continue;

		memset(state->velocity, 0, sizeof(state->velocity));

		if (state->messagenum != cl.parsecount)
			continue;	// not present this frame

		if (oldstate->messagenum != cl.oldparsecount || !oldstate->messagenum)
			continue;	// not present last frame

		if (!ISDEAD(state->frame) && ISDEAD(oldstate->frame))
			continue;

		VectorSubtract(state->origin, oldstate->origin, dist);
		if (DotProduct(dist, dist) > 22500)
			continue;

		VectorScale(dist, 1 / (nextdemotime - olddemotime), pplayer->oldv);

		VectorCopy(state->origin, pplayer->oldo);
		VectorCopy(state->command.angles, pplayer->olda);

		pplayer->oldstate = oldstate;
		pplayer->predict = true;
	}

	// nails
	for (i = 0; i < cl_num_projectiles; i++) {
		if (!cl.int_projectiles[i].interpolate)
			continue;

		VectorCopy(cl.int_projectiles[i].origin, cl_projectiles[i].origin);
	}
}

void MVD_Interpolate(void) {
	int i, j;
	float f;
	frame_t	*frame, *oldframe;
	player_state_t *state, *oldstate, *self, *oldself;
	struct predicted_player *pplayer;
	static float old;

	self = &cl.frames[cl.parsecount & UPDATE_MASK].playerstate[cl.playernum];
	oldself = &cl.frames[(cls.netchan.outgoing_sequence - 1) & UPDATE_MASK].playerstate[cl.playernum];

	self->messagenum = cl.parsecount;

	VectorCopy(oldself->origin, self->origin);
	VectorCopy(oldself->velocity, self->velocity);
	VectorCopy(oldself->viewangles, self->viewangles);

	if (old != nextdemotime) {
		old = nextdemotime;
		MVD_InitInterpolation();
	}

	CL_ParseClientdata();

	cls.netchan.outgoing_sequence = cl.parsecount + 1;

	if (!cl.validsequence)
		return;

	if (nextdemotime <= olddemotime)
		return;

	frame = &cl.frames[cl.parsecount & UPDATE_MASK];
	oldframe = &cl.frames[cl.oldparsecount & UPDATE_MASK];

	f = bound(0, (cls.demotime - olddemotime) / (nextdemotime - olddemotime), 1);

	// interpolate nails
	for (i = 0; i < cl_num_projectiles; i++)	{
		if (!cl.int_projectiles[i].interpolate)
			continue;

		for (j = 0; j < 3; j++) {
			cl_projectiles[i].origin[j] = cl_oldprojectiles[cl.int_projectiles[i].oldindex].origin[j] +
				f * (cl.int_projectiles[i].origin[j] - cl_oldprojectiles[cl.int_projectiles[i].oldindex].origin[j]);
		}
	}

	// interpolate clients
	for (i = 0; i < MAX_CLIENTS; i++) {
		pplayer = &predicted_players[i];
		state = &frame->playerstate[i];
		oldstate = &oldframe->playerstate[i];

		if (pplayer->predict) {
			for (j = 0; j < 3; j++) {
				state->viewangles[j] = MVD_AdjustAngle(oldstate->command.angles[j], pplayer->olda[j], f);
				state->origin[j] = oldstate->origin[j] + f * (pplayer->oldo[j] - oldstate->origin[j]);
				state->velocity[j] = oldstate->velocity[j] + f * (pplayer->oldv[j] - oldstate->velocity[j]);
			}
		}
	}
}

void CL_ClearPredict(void) {
	memset(predicted_players, 0, sizeof(predicted_players));
	mvd_fixangle = 0;
}

void CL_CalcPlayerFPS(player_info_t *info, int msec)
{
    info->fps_msec += msec;
    info->fps_frames++;
    if (info->fps < 0  ||  cls.realtime - info->fps_measure_time > 2  ||  cls.realtime < info->fps_measure_time)
    {
        info->fps_measure_time = cls.realtime;
        info->fps_frames = 0;
        info->fps_msec = 0;
        info->fps = 0;
    }

    if (cls.realtime - info->fps_measure_time >= 1)
    {
        if (info->fps_msec  &&  info->fps_frames)
        {
            info->fps = (int)(1000.0/(info->fps_msec/(double)info->fps_frames));
            info->last_fps = info->fps;
        }
        else
			info->fps = 0;
		info->fps_msec = 0;
		info->fps_frames = 0;
		info->fps_measure_time += 1.0;
    }
}

// Moved out of CL_LinkPacketEntities() so NQD playback can use same code.
void CL_AddParticleTrail(entity_t* ent, centity_t* cent, vec3_t* old_origin, customlight_t* cst_lt, entity_state_t *state)
{
	model_t* model = cl.model_precache[state->modelindex];
	float rocketlightsize = 0.0f;

	if (model->modhint == MOD_LAVABALL)
	{
		R_ParticleTrail(*old_origin, ent->origin, &cent->trail_origin, LAVA_TRAIL);
	}
	else
	{
		if (model->flags & EF_ROCKET)
		{
			if (r_rockettrail.value)
			{
				missile_trail(r_rockettrail.integer, model, old_origin, ent, cent);
			}
			else
			{
				VectorCopy(ent->origin, cent->trail_origin);
			}

			// Add rocket lights
			rocketlightsize = 200.0 * bound(0, r_rocketlight.value, 1);
			if (rocketlightsize >= 1)
			{
				int bubble = gl_rl_globe.integer ? 2 : 1;

				if ((r_rockettrail.value < 8 || r_rockettrail.value == 12) && model->modhint != MOD_LAVABALL)
				{
					dlightColorEx(r_rocketlightcolor.value, r_rocketlightcolor.string, lt_rocket, false, cst_lt);
					CL_NewDlightEx(state->number, ent->origin, rocketlightsize, 0.1, cst_lt, bubble);
					if (!ISPAUSED && amf_coronas.value) //VULT CORONAS
						NewCorona(C_ROCKETLIGHT, ent->origin);
				}
				else if (r_rockettrail.value == 9 || r_rockettrail.value == 11)
				{
					CL_NewDlight(state->number, ent->origin, rocketlightsize, 0.1, lt_default, bubble);
				}
				else if (r_rockettrail.value == 8)
				{
					// PLASMA ROCKETS
					CL_NewDlight(state->number, ent->origin, rocketlightsize, 0.1, lt_blue, bubble);
				}
				else if (r_rockettrail.value == 10)
				{
					// FUEL ROD GUN
					CL_NewDlight(state->number, ent->origin, rocketlightsize, 0.1, lt_green, bubble);
				}
			}
		}
		else if (model->flags & EF_GRENADE)
		{
			if (model->modhint == MOD_BUILDINGGIBS)
			{
				R_ParticleTrail(*old_origin, ent->origin, &cent->trail_origin, GRENADE_TRAIL);
			}
			else if (r_grenadetrail.value && model->modhint != MOD_RAIL)
			{
				missile_trail(fix_trail_num_for_grens(r_grenadetrail.integer),
					model, old_origin, ent, cent);
			}
			else if (r_railtrail.value && model->modhint == MOD_RAIL)
			{
				missile_trail(fix_trail_num_for_grens(r_railtrail.integer),
					model, old_origin, ent, cent);
			}
			else
			{
				VectorCopy(ent->origin, cent->trail_origin);
			}
		}
		else if (model->flags & EF_GIB)
		{
			if (amf_part_gibtrails.value)
				R_ParticleTrail(*old_origin, ent->origin, &cent->trail_origin, BLEEDING_TRAIL);
			else
				R_ParticleTrail(*old_origin, ent->origin, &cent->trail_origin, BLOOD_TRAIL);
		}
		else if (model->flags & EF_ZOMGIB)
		{
			if (model->modhint == MOD_RAIL2)
				R_ParticleTrail(*old_origin, ent->origin, &cent->trail_origin, RAIL_TRAIL2);
			else if (amf_part_gibtrails.value)
				R_ParticleTrail(*old_origin, ent->origin, &cent->trail_origin, BLEEDING_TRAIL);
			else
				R_ParticleTrail(*old_origin, ent->origin, &cent->trail_origin, BIG_BLOOD_TRAIL);
		}
		else if (model->flags & EF_TRACER)
		{
			// VULT TRACER GLOW
			rocketlightsize = 35 * (1 + bound(0, r_rocketlight.value, 1));
			CL_NewDlight(state->number, ent->origin, rocketlightsize, 0.01, lt_green, true);
			if (!ISPAUSED && amf_coronas.value)
				NewCorona(C_WIZLIGHT, ent->origin);

			R_ParticleTrail(*old_origin, ent->origin, &cent->trail_origin, TRACER1_TRAIL);
		}
		else if (model->flags & EF_TRACER2)
		{
			// VULT TRACER GLOW
			rocketlightsize = 35 * (1 + bound(0, r_rocketlight.value, 1));
			CL_NewDlight(state->number, ent->origin, rocketlightsize, 0.01, lt_default, true);
			if (!ISPAUSED && amf_coronas.value)
				NewCorona(C_KNIGHTLIGHT, ent->origin);

			R_ParticleTrail(*old_origin, ent->origin, &cent->trail_origin, TRACER2_TRAIL);
		}
		else if (model->flags & EF_TRACER3)
		{
			// VULT TRACER GLOW
			rocketlightsize = 35 * (1 + bound(0, r_rocketlight.value, 1));
			CL_NewDlight(state->number, ent->origin, rocketlightsize, 0.01, lt_blue, true);
			if (!ISPAUSED && amf_coronas.value)
				NewCorona(C_VORELIGHT, ent->origin);

			R_ParticleTrail(*old_origin, ent->origin, &cent->trail_origin, VOOR_TRAIL);
		}
		else if (model->modhint == MOD_SPIKE && amf_nailtrail.value && !gl_no24bit.integer)
		{
			// VULT NAILTRAIL
			if (amf_nailtrail_plasma.value)
			{
				byte color[3];
				color[0] = 0; color[1] = 70; color[2] = 255;
				FireballTrail(*old_origin, ent->origin, &cent->trail_origin, color, 0.6, 0.3);
			}
			else
			{
				ParticleAlphaTrail(*old_origin, ent->origin, &cent->trail_origin, 2, 0.4);
			}
		}
		else if (model->modhint == MOD_TF_TRAIL && amf_extratrails.value)
		{
			// VULT TRAILS
			ParticleAlphaTrail(*old_origin, ent->origin, &cent->trail_origin, 4, 0.4);
		}
		else if (model->modhint == MOD_LASER && amf_extratrails.value)
		{
			rocketlightsize = 35 * (1 + bound(0, r_rocketlight.value, 1));
			CL_NewDlight(state->number, ent->origin, rocketlightsize, 0.01, lt_default, true);
			if (!ISPAUSED && amf_coronas.value)
				NewCorona(C_KNIGHTLIGHT, ent->origin);
			VX_LightningTrail(*old_origin, ent->origin);
			R_ParticleTrail(*old_origin, ent->origin, &cent->trail_origin, TRACER2_TRAIL);
		}
		else if (model->modhint == MOD_DETPACK)
		{
			// VULT CORONAS
			if (qmb_initialized)
			{
				vec3_t liteorg, litestop, forward, right, up;
				VectorCopy(ent->origin, liteorg);
				AngleVectors(ent->angles, forward, right, up);
				VectorMA(liteorg, -15, up, liteorg);
				VectorMA(liteorg, -10, forward, liteorg);
				VectorCopy(*old_origin, litestop);
				VectorMA(litestop, -15, up, litestop);
				VectorMA(litestop, -10, forward, litestop);

				// R_ParticleTrail (*old_origin, ent->origin, &cent->trail_origin, TF_TRAIL); //for cutf tossable dets
				ParticleAlphaTrail(*old_origin, liteorg, &cent->trail_origin, 10, 0.8);
				if (!ISPAUSED && amf_coronas.value)
				{
					if (ent->skinnum > 0)
						NewCorona(C_REDLIGHT, liteorg);
					else
						NewCorona(C_GREENLIGHT, liteorg);
				}
			}
		}

		// VULT BUILDING SPARKS
		if (!ISPAUSED && amf_buildingsparks.value && (model->modhint == MOD_BUILDINGGIBS && (rand() % 40 < 2)))
		{
			vec3_t liteorg, forward, right, up;
			byte col[3] = { 60, 100, 240 };
			VectorCopy(ent->origin, liteorg);
			AngleVectors(ent->angles, forward, right, up);
			VectorMA(liteorg, rand() % 10 - 5, up, liteorg);
			VectorMA(liteorg, rand() % 10 - 5, right, liteorg);
			VectorMA(liteorg, rand() % 10 - 5, forward, liteorg);
			SparkGen(liteorg, col, (int)(6 * amf_buildingsparks.value), 100, 1);
			if (amf_coronas.value)
				NewCorona(C_BLUESPARK, liteorg);
		}
		// VULT MOTION TRAILS
		if (model->modhint == MOD_FLAG && amf_motiontrails.value)
		{
			CL_CreateBlurs(*old_origin, ent->origin, ent);
		}
		// VULT MOTION TRAILS - Demon in pouncing animation
		if (model->modhint == MOD_DEMON && amf_motiontrails.value)
		{
			if (ent->frame >= 27 && ent->frame <= 38)
				CL_CreateBlurs(*old_origin, ent->origin, ent);
		}

		// VULT TESLA CHARGE - Tesla or shambler in charging animation
		if (((model->modhint == MOD_TESLA && ent->frame >= 7 && ent->frame <= 12) ||
			(model->modhint == MOD_SHAMBLER && ent->frame >= 65 && ent->frame <= 68))
			&& !ISPAUSED && amf_cutf_tesla_effect.value)
		{
			vec3_t liteorg;
			VectorCopy(ent->origin, liteorg);
			liteorg[2] += 32;
			VX_TeslaCharge(liteorg);
		}
	}

	if (amf_motiontrails_wtf.value)
		CL_CreateBlurs(ent->origin, ent->origin, ent);
}
