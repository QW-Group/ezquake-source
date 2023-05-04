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
#include "pmove.h"
#include "cl_tent.h"
#include "utils.h"
#include "qmb_particles.h"
#include "rulesets.h"

static int MVD_TranslateFlags(int src);
void TP_ParsePlayerInfo(player_state_t *, player_state_t *, player_info_t *info);	

extern cvar_t cl_predict_players, cl_solid_players, cl_rocket2grenade;
extern cvar_t cl_predict_half;
extern cvar_t cl_model_bobbing;		
extern cvar_t cl_nolerp, cl_lerp_monsters, cl_newlerp;
extern cvar_t r_drawvweps;		
#ifdef MVD_PEXT1_SIMPLEPROJECTILE
extern cvar_t cl_sproj_xerp;
#endif
extern  unsigned int     cl_dlight_active[MAX_DLIGHTS/32];       

static struct predicted_player {
	int flags;
	qbool active;
	vec3_t origin;	// predicted origin

	qbool predict;
	vec3_t	oldo;
	vec3_t	olda;
	vec3_t	oldv;
	player_state_t *oldstate;

	qbool drawn;
	vec3_t drawn_origin;
	int msec;
	float paused_sec;
} predicted_players[MAX_CLIENTS];

char *cl_modelnames[cl_num_modelindices];
int cl_modelindices[cl_num_modelindices];

void CL_InitEnts(void) {
	int i;

	memset(cl_modelnames, 0, sizeof(cl_modelnames));

	cl_modelnames[mi_spike] = "progs/spike.mdl";
	cl_modelnames[mi_s_spike] = "progs/s_spike.mdl";
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
	cl_modelnames[mi_coilgun] = "progs/v_coil.mdl";
	cl_modelnames[mi_hook] = "progs/v_star.mdl";

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

	for (i = 0; i < cl_num_modelindices; i++) {
		if (!cl_modelnames[i]) {
			Sys_Error("cl_modelnames[%d] not initialized", i);
		}
	}

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

void CL_ClearScene(void)
{
	memset(cl_visents.list, 0, sizeof(cl_visents.list));
	memset(cl_visents.typecount, 0, sizeof(cl_visents.typecount));
	cl_visents.count = 0;
}

void CL_AddEntityToList(visentlist_t* list, visentlist_entrytype_t vistype, entity_t* ent, modtype_t type, qbool shell)
{
	extern cvar_t gl_outline;

	if (list->count < sizeof(list->list) / sizeof(list->list[0])) {
		list->list[cl_visents.count].ent = *ent;

		ent = &list->list[cl_visents.count].ent;
		list->list[cl_visents.count].type = type;
		list->list[cl_visents.count].distance = VectorDistanceQuick(cl.simorg, ent->origin);
		list->list[cl_visents.count].draw[vistype] = true;

		ent->outlineScale = 0.5f * (r_refdef2.outlineBase + DotProduct(ent->origin, r_refdef2.outline_vpn));
		ent->outlineScale = bound(ent->outlineScale, 0, 2);

		++list->typecount[vistype];
		if (shell) {
			list->list[cl_visents.count].draw[visent_shells] = true;
			++list->typecount[visent_shells];
		}
		// Check for outline on models.
		// We don't support outline for transparent models,
		// and we also check for ruleset, since we don't want outline on eyes.
		if (((gl_outline.integer & 1) && !RuleSets_DisallowModelOutline(ent->model))) {
			list->list[cl_visents.count].draw[visent_outlines] = true;
			++list->typecount[visent_outlines];
		}

		++list->count;
	}
}

void CL_AddEntity(entity_t *ent)
{
	extern qbool R_CanDrawSimpleItem(entity_t* ent);
	visentlist_entrytype_t vistype;
	modtype_t type = ent->model->type;
	qbool shell = false;
	extern cvar_t gl_powerupshells;

	if ((ent->effects & (EF_BLUE | EF_RED | EF_GREEN)) && bound(0, gl_powerupshells.value, 1)) {
		if (R_CanDrawSimpleItem(ent)) {
			vistype = visent_alpha;
			type = mod_sprite;
		}
		else {
			vistype = visent_normal;
			shell = true;
		}
	}
	else if (ent->renderfx & RF_NORMALENT) {
		vistype = visent_normal;
	}
	else if (ent->model->type == mod_sprite || R_CanDrawSimpleItem(ent)) {
		vistype = visent_alpha;
		type = mod_sprite;
	}
	else if (ent->model->modhint == MOD_PLAYER || ent->model->modhint == MOD_EYES || ent->renderfx & RF_PLAYERMODEL) {
		vistype = visent_firstpass;
		ent->renderfx |= RF_NOSHADOW;
	}
	else {
		vistype = visent_normal;
	}

	CL_AddEntityToList(&cl_visents, vistype, ent, type, shell);
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

dlight_t *CL_AllocDlight(int key)
{
	unsigned int i;
	unsigned int j;
	dlight_t *dl;

	// first look for an exact key match
	if (key) {
		dl = cl_dlights;
		for (i = 0; i < MAX_DLIGHTS; i++, dl++) {
			if (dl->key == key) {
				memset (dl, 0, sizeof(*dl));
				dl->key = key;
				cl_dlight_active[i/32] |= (1<<(i%32));
				return dl;
			}
		}
	}

	// then look for anything else
	for (i = 0; i < MAX_DLIGHTS/32; i++) {
		if (cl_dlight_active[i] != 0xffffffff) {
			for (j = 0; j < 32; j++) {
				if (!(cl_dlight_active[i] & (1<<j)) && i*32+j < MAX_DLIGHTS) {
					dl = cl_dlights + i * 32 + j;
					memset(dl, 0, sizeof(*dl));
					dl->key = key;
					cl_dlight_active[i] |= 1<<j;
					return dl;
				}
			}
		}
	}

	dl = &cl_dlights[0];
	memset (dl, 0, sizeof(*dl));
	dl->key = key;
	cl_dlight_active[0] |= 1;

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

void CL_DecayLights(void)
{
	unsigned int i;
	unsigned int j;
	dlight_t *dl;
	extern cvar_t r_lightdecayrate;

	if (cls.state < ca_active) {
		return;
	}

	for (i = 0; i < MAX_DLIGHTS/32; i++) {
		if (cl_dlight_active[i]) {
			for (j = 0; j < 32; j++) {
				if ((cl_dlight_active[i]&(1<<j)) && i*32+j < MAX_DLIGHTS) {
					dl = cl_dlights + i*32 + j;

					if (dl->die < cl.time) {
						cl_dlight_active[i] &= ~(1 << j);
						dl->radius = 0;
						continue;
					}

					dl->radius -= max(r_lightdecayrate.value, 1) * cls.frametime * dl->decay;

					if (dl->radius <= 0) {
						cl_dlight_active[i] &= ~(1 << j);
						dl->radius = 0;
						continue;
					}
				}
			}
		}
	}
}

qbool NewLerp_AbleModel(int idx)
{
	return !cls.demoplayback && cl_newlerp.value && 
			(idx == cl_modelindices[mi_rocket] || idx == cl_modelindices[mi_grenade] || idx == cl_modelindices[mi_spike]);
}

void CL_SetupPacketEntity (int number, entity_state_t *state, qbool changed) {
	centity_t *cent;
	int t;

	cent = &cl_entities[number];

	if (!cl.oldvalidsequence || cl.oldvalidsequence != cent->sequence ||
		state->modelindex != cent->current.modelindex ||
		!VectorL2Compare(state->origin, cent->current.origin, 200)
	) {
		cent->startlerp = cl.time;
		cent->deltalerp = -1;
		cent->frametime = -1;
		cent->oldsequence = 0;
		cent->trail_number = (cent->trail_number + 1) % 1000;
		for (t = 0; t < sizeof(cent->trails) / sizeof(cent->trails[0]); ++t) {
			cent->trails[t].lasttime = 0;
			VectorCopy(state->origin, cent->trails[t].stop);
		}
	}
	else {
		cent->oldsequence = cent->sequence;
		
		if (state->frame != cent->current.frame) {
			cent->frametime = cl.time;
			cent->oldframe = cent->current.frame;
		}
		
		if (changed) {
			if (!VectorCompare(state->origin, cent->current.origin) || !VectorCompare(state->angles, cent->current.angles)) {
			    if (NewLerp_AbleModel(state->modelindex)) {
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
		to->modelindex = MSG_ReadByte();

	if (bits & U_FRAME)
		to->frame = MSG_ReadByte ();

	if (bits & U_COLORMAP)
		to->colormap = MSG_ReadByte();

	if (bits & U_SKIN)
		to->skinnum = MSG_ReadByte();

	if (bits & U_EFFECTS)
		to->effects = MSG_ReadByte();

	if (bits & U_ORIGIN1) {
		if (cls.mvdprotocolextensions1 & MVD_PEXT1_FLOATCOORDS) {
			to->origin[0] = MSG_ReadFloatCoord();
		}
		else {
			to->origin[0] = MSG_ReadCoord();
		}
	}

	if (bits & U_ANGLE1)
		to->angles[0] = MSG_ReadAngle();

	if (bits & U_ORIGIN2) {
		if (cls.mvdprotocolextensions1 & MVD_PEXT1_FLOATCOORDS) {
			to->origin[1] = MSG_ReadFloatCoord();
		}
		else {
			to->origin[1] = MSG_ReadCoord();
		}
	}

	if (bits & U_ANGLE2)
		to->angles[1] = MSG_ReadAngle();

	if (bits & U_ORIGIN3) {
		if (cls.mvdprotocolextensions1 & MVD_PEXT1_FLOATCOORDS) {
			to->origin[2] = MSG_ReadFloatCoord();
		}
		else {
			to->origin[2] = MSG_ReadCoord();
		}
	}

	if (bits & U_ANGLE3)
		to->angles[2] = MSG_ReadAngle();

	if (bits & U_SOLID) {
		// FIXME
	}

#ifdef PROTOCOL_VERSION_FTE
#ifdef FTE_PEXT_TRANS
	if (morebits & U_FTE_TRANS && cls.fteprotocolextensions & FTE_PEXT_TRANS) {
		to->trans = MSG_ReadByte();
	}
#endif

#ifdef FTE_PEXT_ENTITYDBL
	if (morebits & U_FTE_ENTITYDBL) {
		to->number += 512;
	}
#endif
#ifdef FTE_PEXT_ENTITYDBL2
	if (morebits & U_FTE_ENTITYDBL2) {
		to->number += 1024;
	}
#endif
#ifdef FTE_PEXT_MODELDBL
	if (morebits & U_FTE_MODELDBL) {
		to->modelindex += 256;
	}
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
	qbool copy = (cls.netchan.incoming_sequence == 0 && cls.mvdplayback);

	newpacket = cls.netchan.incoming_sequence & UPDATE_MASK;
	newp = &cl.frames[newpacket].packet_entities;
	cl.frames[newpacket].invalid = false;
	cl.frames[newpacket].in_qwd = false;

	if (delta) 
	{
		from = MSG_ReadByte ();

		oldpacket = cl.frames[newpacket].delta_sequence;
		if (cls.mvdplayback) {
			from = oldpacket = cls.netchan.incoming_sequence - 1;
		}

		if (cls.netchan.outgoing_sequence - cls.netchan.incoming_sequence >= UPDATE_BACKUP - 1) 
		{
			// There are no valid frames left, so drop it.
			FlushEntityPacket();
			cl.validsequence = 0;
			return;
		}

		if ((from & UPDATE_MASK) != (oldpacket & UPDATE_MASK)) 
		{
			Com_DPrintf ("WARNING: from mismatch (%d vs %d, %d vs %d)\n", (from & UPDATE_MASK), (oldpacket & UPDATE_MASK), from, oldpacket);
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
	if (copy) {
		// do this incase it's FTE demo...
		memcpy(&cl.frames[1], &cl.frames[0], sizeof(cl.frames[1]));
	}

	if (cls.state == ca_onserver) {
		// we can now render a frame
		CL_MakeActive();
	}
}





#ifdef MVD_PEXT1_SIMPLEPROJECTILE
extern fproj_t cl_fakeprojectiles[MAX_FAKEPROJ];
cs_sprojectile_t cs_sprojectiles[CL_MAX_EDICTS];

int CL_SimpleProjectile_MatchFakeProj(cs_sprojectile_t *sproj, int entnum)
{
	fproj_t *prj;
	int i;
	for (i = 0, prj = cl_fakeprojectiles; i < MAX_FAKEPROJ; i++, prj++)
	{
		if (prj->endtime == 0)
			continue;

		if (prj->owner != sproj->owner)
			continue;

		//if (prj->modelindex != sproj->modelindex)
		//	continue;

		if (cl.time > prj->endtime + 0.1)
			continue;

		//vec3_t diff;
		//VectorSubtract(sproj->origin, prj->start, diff);
		//if (VectorLength(diff) < 24)
		if (prj->entnum == sproj->fproj_number)
		{
			return i;
		}
#ifdef MVD_PEXT1_WEAPONPREDICTION
		else if (cls.mvdprotocolextensions1 & MVD_PEXT1_WEAPONPREDICTION && prj->entnum == 0)
		{
			//if (cl.time > prj->endtime - 0.05)
			//	continue;

			vec3_t diff;
			VectorSubtract(sproj->origin, prj->org, diff);

			if (VectorLength(diff) > 24)
				continue;

			//VectorCopy(prj->org, prj->partorg);
			prj->entnum = entnum;
			prj->endtime = prj->starttime + 5;
			return i;
		}
#endif
	}

	return -1;
}


/*
void CL_ParseDeltaSimpleProjectile(sprojectile_state_t *from, sprojectile_state_t *to, int bits, int entnum) {
	int i;

	*to = *from;
	to->number = entnum;
	to->flags = bits;
	
	if (bits & U_ORIGIN2)
		to->owner = (unsigned short)MSG_ReadShort();

	if (bits & U_FRAME)
		to->modelindex = MSG_ReadByte();

	if (bits & U_ORIGIN3)
		to->time_offset = -(float)MSG_ReadByte() / 255;

	if (bits & U_ORIGIN1)
	{
		to->time = MSG_ReadFloat();

		to->origin[0] = MSG_ReadCoord();
		to->origin[1] = MSG_ReadCoord();
		to->origin[2] = MSG_ReadCoord();

		to->velocity[0] = MSG_ReadCoord();
		to->velocity[1] = MSG_ReadCoord();
		to->velocity[2] = MSG_ReadCoord();
	}

	if (bits & U_ANGLE2)
	{
		to->angles[0] = MSG_ReadAngle();
		to->angles[1] = MSG_ReadAngle();
		to->angles[2] = MSG_ReadAngle();
	}
}


void FlushSimpleProjectilePacket(void) {
	int word, entnum;
	sprojectile_state_t olde, newe;

	Com_DPrintf("FlushEntityPacket\n");

	memset(&olde, 0, sizeof(olde));

	//cl.delta_sequence = 0;
	//cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK].invalid = true;

	// read it all, but ignore it
	while (1) {
		entnum = (unsigned short)MSG_ReadShort();
		word = (unsigned short)MSG_ReadShort();
		if (msg_badread) {	// something didn't parse right...
			Host_Error("msg_badread in packetentities");
			return;
		}

		if (word == 0 && entnum == 0)
			break;	// done

		CL_ParseDeltaSimpleProjectile(&olde, &newe, word, entnum);
	}
}


void CL_SetupPacketSimpleProjectile(int bits, sprojectile_state_t *from, sprojectile_state_t *to)
{
	if ((from->time != to->time) && !(bits & U_REMOVE))
	{
		fproj_t *mis;
		to->fproj_num = CL_SimpleProjectile_MatchFakeProj(to);
		if (to->fproj_num < 0)
		{
			//Con_Printf("NO MATCH!\n");
			mis = CL_CreateFakeSuperNail();
			to->fproj_num = mis->index;

			mis->entnum = to->number;
			mis->starttime = cl.time + (to->time - cl.servertime);
			mis->endtime = mis->starttime + 5;
			VectorCopy(to->origin, mis->partorg);
			VectorMA(mis->partorg, to->time_offset, to->velocity, mis->partorg);
		}
		else
		{
			//Con_Printf("MATCHED! %i\n", to->number);
			mis = &cl_fakeprojectiles[to->fproj_num];
			mis->entnum = to->number;
			mis->starttime = cl.time + (to->time - cl.servertime);
		}

		mis->owner = to->owner;
		VectorCopy(to->origin, mis->start);
		VectorCopy(to->origin, mis->org);
		VectorCopy(to->velocity, mis->vel);
		VectorCopy(to->angles, mis->angs);

		if (to->owner == (cl.viewplayernum + 1))
		{
			//mis->starttime -= 0.5;
		}

		if (to->modelindex != 0)
		{
			mis->modelindex = to->modelindex;
			// apply the model's effect flags
			mis->effects = cl.model_precache[to->modelindex]->flags;

			// a yucky hack to add nail trails
			if (to->modelindex == cl_modelindices[mi_spike] || to->modelindex == cl_modelindices[mi_s_spike])
			{
				mis->effects |= EF_TRACER2;
			}
		}

		//CL_MatchFakeProjectile(cent);

		//Con_Printf("fp created\n   stime: %f, fptime %f\n", cl.time, mis->starttime);
	}
}
*/


// An svc_packetsprojectiles has just been parsed, deal with the rest of the data stream.
void CL_ParsePacketSimpleProjectiles(void)
{
	int word, sendflags;
	int framenum;

	framenum = MSG_ReadLong();
	/*
	cl.csqc_latestframenums[cl.csqc_latestframeposition] = framenum;
	cl.csqc_latestframeposition++;
	if (cl.csqc_latestframeposition >= LATESTFRAMENUMS)
		cl.csqc_latestframeposition = LATESTFRAMENUMS - 1;
	*/

	cl.csqc_latestframenums[cl.csqc_latestframeposition] = framenum;
	cl.csqc_latestsendnums[cl.csqc_latestframeposition] = cls.netchan.outgoing_sequence;
	cl.csqc_latestframeposition = (cl.csqc_latestframeposition + 1) % LATESTFRAMENUMS;


	while (true)
	{
		word = (unsigned short)MSG_ReadShort();

		if (word == 0)
			break;

		if (word & 0x8000)
		{
			word -= word & 0x8000;

			fproj_t *mis = &cl_fakeprojectiles[cs_sprojectiles[word].fproj_number];
			if (mis->entnum == word)
			{
				memset(mis, 0, sizeof(fproj_t));
				mis->endtime = -1;
				mis->index = 0;
			}
			memset(&cs_sprojectiles[word], 0, sizeof(cs_sprojectile_t));

			continue;
		}

		word = bound(0, word, CL_MAX_EDICTS - 1);

		fproj_t *mis;
		int mis_new = 0;
		if (cs_sprojectiles[word].active == false)
			mis_new = 1;

		cs_sprojectiles[word].active = true;
		cs_sprojectile_t *cs_sproj = &cs_sprojectiles[word];
		cs_sproj->fproj_number = bound(0, cs_sproj->fproj_number, MAX_FAKEPROJ - 1);
		mis = &cl_fakeprojectiles[cs_sproj->fproj_number];

		sendflags = (unsigned short)MSG_ReadShort();


		if (sendflags & U_ORIGIN1)
		{
			cs_sproj->origin[0] = MSG_ReadFloat();
			cs_sproj->origin[1] = MSG_ReadFloat();
			cs_sproj->origin[2] = MSG_ReadFloat();

			cs_sproj->velocity[0] = MSG_ReadFloat();
			cs_sproj->velocity[1] = MSG_ReadFloat();
			cs_sproj->velocity[2] = MSG_ReadFloat();
		}

		if (sendflags & U_ANGLE1)
		{
			cs_sproj->angles[0] = MSG_ReadAngle();
			cs_sproj->angles[1] = MSG_ReadAngle();
			cs_sproj->angles[2] = MSG_ReadAngle();
		}

		if (sendflags & U_MODEL)
		{
			cs_sproj->modelindex = MSG_ReadShort();
			cs_sproj->owner = MSG_ReadShort();
		}

		if (sendflags & U_ORIGIN3)
		{
			cs_sproj->time_offset = -(float)MSG_ReadByte() / 255;
		}


		if (mis_new)
		{
			cs_sproj->fproj_number = -1;
			cs_sproj->fproj_number = CL_SimpleProjectile_MatchFakeProj(cs_sproj, word);
			if (cs_sproj->fproj_number < 0)
			{
				mis_new = 2;
				mis = CL_AllocFakeProjectile();
				cs_sproj->fproj_number = mis->index;
			}


			mis = &cl_fakeprojectiles[cs_sproj->fproj_number];

			mis->starttime = cl.time;
			VectorCopy(cs_sproj->origin, mis->start);
		}


		if (sendflags & U_ORIGIN1)
		{
			VectorCopy(cs_sproj->origin, mis->org);
			VectorCopy(cs_sproj->velocity, mis->vel);

			mis->starttime = cl.time;
			VectorCopy(mis->org, mis->start);
		}

		if (sendflags & U_ANGLE1)
		{
			VectorCopy(cs_sproj->angles, mis->angs);
		}

		if (sendflags & U_MODEL)
		{
			mis->modelindex = cs_sproj->modelindex;
			mis->owner = cs_sproj->owner;

			///*
			if (mis->modelindex != 0 && cl.model_precache[mis->modelindex])
			{
				// apply the model's effect flags
				mis->effects = cl.model_precache[mis->modelindex]->flags;

				// a yucky hack to add nail trails
				if (mis->modelindex == cl_modelindices[mi_spike] || mis->modelindex == cl_modelindices[mi_s_spike])
				{
					mis->effects |= 256;
				}
			}
			//*/
		}

		

		mis->endtime = cl.time + 20;
		if (mis_new)
		{
			mis->entnum = word;
			if (mis_new == 2)
			{
				memset(&cl_entities[mis->entnum], 0, sizeof(centity_t));

				VectorCopy(mis->org, mis->partorg);
				if (cs_sproj->time_offset)
				{
					VectorMA(mis->partorg, cs_sproj->time_offset, cs_sproj->velocity, mis->partorg);
				}
			}
			else
			{
				cl_entities[mis->entnum] = mis->cent;
			}
		}

	}





	/*
	return;

	int oldpacket, newpacket, oldindex, newindex, word, entnum, newnum, oldnum;
	packet_entities_t *oldp, *newp, dummy;
	qbool full;
	qbool delta;
	byte from;
	newpacket = cls.netchan.incoming_sequence & UPDATE_MASK;
	newp = &cl.frames[newpacket].packet_entities;


	if (delta)
	{
		from = MSG_ReadByte();
		oldpacket = cl.frames[newpacket].delta_sequence;
		
		if (cls.netchan.outgoing_sequence - cls.netchan.incoming_sequence >= UPDATE_BACKUP - 1)
		{
			Con_Printf("No valid frames\n");
			// There are no valid frames left, so drop it.
			FlushSimpleProjectilePacket();
			//cl.validsequence = 0;
			return;
		}

		if ((from & UPDATE_MASK) != (oldpacket & UPDATE_MASK))
		{
			Con_Printf("Mismatch\n");
			Com_DPrintf("WARNING: from mismatch (%d vs %d, %d vs %d)\n", (from & UPDATE_MASK), (oldpacket & UPDATE_MASK), from, oldpacket);
			FlushSimpleProjectilePacket();
			//cl.validsequence = 0;
			return;
		}

		if (cls.netchan.outgoing_sequence - oldpacket >= UPDATE_BACKUP - 1)
		{
			Con_Printf("Too old\n");
			// We can't use this, it is too old.
			FlushSimpleProjectilePacket();
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
		dummy.num_sprojectiles = 0;
		full = true;
	}

	oldindex = 0;
	newindex = 0;
	newp->num_sprojectiles = 0;

	int f_enm, t_enm;
	f_enm = MAX_EDICTS;
	t_enm = (unsigned short)MSG_ReadShort();
	word = (unsigned short)MSG_ReadShort();



	//Con_Printf("ent num %i\n word %i\n", t_enm, word);
	if (t_enm != 0 || word != 0)
	{
		//Con_Printf("\n\n\nFRAME:%i\n", oldp->num_sprojectiles);

		if (oldindex < oldp->num_sprojectiles)
			f_enm = oldp->sprojectiles[oldindex].number;

		while (true)
		{
			if (t_enm == 0 && word == 0)// || msg_badread)
			{
				//Con_Printf("msg end\n");
				break;
			}

			if (oldindex >= MAX_SIMPLEPROJECTILES || newindex >= MAX_SIMPLEPROJECTILES)
				break;


			if (oldindex < oldp->num_sprojectiles)
				f_enm = oldp->sprojectiles[oldindex].number;
			else
				f_enm = MAX_EDICTS;

			//if (oldindex >= oldp->num_sprojectiles && newindex >= newp->num_sprojectiles)
			//	break;


			//if (newindex < newp->num_sprojectiles)
			//{
			//	t_enm = newp->sprojectiles[to_ind].number;
			//}

			//Con_Printf("SORT %i vs %i\n", f_enm, t_enm);
			if (t_enm == f_enm)
			{
				if (word & U_REMOVE)
				{
					//Con_Printf("Remove: %i\n", t_enm);
					fproj_t *mis = &cl_fakeprojectiles[oldp->sprojectiles[oldindex].fproj_num];
					if (mis->entnum == oldp->sprojectiles[oldindex].number)
					{
						memset(mis, 0, sizeof(fproj_t));
						//Con_Printf("fprj delete: %i\n", oldp->sprojectiles[oldindex].fproj_num);
					}

					t_enm = (unsigned short)MSG_ReadShort();
					word = (unsigned short)MSG_ReadShort();

					oldindex++;

					if (oldindex < oldp->num_sprojectiles)
						f_enm = oldp->sprojectiles[oldindex].number;
					continue;
				}

				//update
				//Con_Printf("Update: %i\n", t_enm);

				newp->sprojectiles[newindex] = oldp->sprojectiles[newindex];
				//newp->sprojectiles[newindex] = (newp->sprojectiles[newindex]);
				//newp->sprojectiles[newindex].sflag = oldindex;
				//newp->sprojectiles[newindex].number = t_enm;
				CL_ParseDeltaSimpleProjectile(&oldp->sprojectiles[oldindex], &newp->sprojectiles[newindex], word, t_enm);
				CL_SetupPacketSimpleProjectile(word, &oldp->sprojectiles[oldindex], &newp->sprojectiles[newindex]);

				t_enm = (unsigned short)MSG_ReadShort();
				word = (unsigned short)MSG_ReadShort();

				oldindex++;
				newindex++;
			}
			else if (f_enm < t_enm)
			{
				//Con_Printf("Insert: %i\n", f_enm);

				//newp->sprojectiles[newindex] = (oldp->sprojectiles[oldindex]);
				//newp->sprojectiles[newindex].sflag = -1;
				newp->sprojectiles[newindex] = oldp->sprojectiles[oldindex];
				CL_SetupPacketSimpleProjectile(word, &oldp->sprojectiles[oldindex], &newp->sprojectiles[newindex]);

				newindex++;
				oldindex++;
			}
			else if (t_enm < f_enm)
			{
				//newp->sprojectiles[newindex] = (newp->sprojectiles[newindex]);
				//newp->sprojectiles[newindex].sflag = 0;

				//new
				//Con_Printf("New: %i\n", t_enm);

				//newp->sprojectiles[newindex].number = t_enm;
				CL_ParseDeltaSimpleProjectile(&cl_entities[newnum].baseline, &newp->sprojectiles[newindex], word, t_enm);
				CL_SetupPacketSimpleProjectile(word, &cl_entities[newnum].baseline, &newp->sprojectiles[newindex]);

				t_enm = (unsigned short)MSG_ReadShort();
				word = (unsigned short)MSG_ReadShort();

				newindex++;
			}
		}
	}


	for (; oldindex < oldp->num_sprojectiles && newindex < MAX_SIMPLEPROJECTILES; oldindex++)
	{
		if (oldp->sprojectiles[oldindex].number == 0)
			continue;

		newp->sprojectiles[newindex] = oldp->sprojectiles[oldindex];
		newindex++;
	}


	// cleanup any lost info due to overflow
	while (word != 0 || t_enm != 0)
	{
		//Con_Printf("Still need to flush.\n");
		sprojectile_state_t olde, newe;
		
		CL_ParseDeltaSimpleProjectile(&olde, &newe, word, t_enm);

		t_enm = (unsigned short)MSG_ReadShort();
		word = (unsigned short)MSG_ReadShort();
	}

	newp->num_sprojectiles = newindex;
	*/
}

#endif



static qbool CL_SetAlphaByDistance(entity_t* ent)
{
	vec3_t diff;
	float distance;

	VectorSubtract(ent->origin, cl.simorg, diff);
	distance = VectorLength(diff);

	// If too close to player, just hide entirely
	if (distance < KTX_RACING_PLAYER_MIN_DISTANCE)
		return false;

	ent->alpha = min(distance, KTX_RACING_PLAYER_MAX_DISTANCE) / KTX_RACING_PLAYER_ALPHA_SCALE;
	ent->renderfx |= RF_NORMALENT;
	return true;
}

// TODO: OMG SPLIT THIS UP!
void CL_LinkPacketEntities(void) 
{
	entity_t ent;
	centity_t *cent;
	packet_entities_t *pack;
	entity_state_t *state;
	model_t *model;
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
			CL_MatchFakeProjectile(cent);
		} 
		else if (state->modelindex == cl_modelindices[mi_grenade])
		{
			CL_MatchFakeProjectile(cent);
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
				extern cvar_t gl_part_inferno;

				if (gl_part_inferno.value) 
				{
					QMB_InfernoFlame (ent.origin);
					continue;
				}
			}

			if (state->modelindex == cl_modelindices[mi_bubble]) 
			{
				extern cvar_t gl_part_bubble;

				if (gl_part_bubble.value) {
					QMB_StaticBubble(&ent);
					continue;
				}
			}
		}

		// Add trails
		VectorCopy(ent.origin, cent->lerp_origin);
		if ((model->flags & ~EF_ROTATE) || model->modhint) {
			CL_AddParticleTrail(&ent, cent, &cst_lt, state);
		}

		if ((!cls.mvdplayback || Cam_TrackNum() >= 0) && cl.racing && cl.race_pacemaker_ent == state->number && !CL_SetAlphaByDistance(&ent)) {
			continue;
		}

		ent.renderfx &= ~(RF_BACKPACK_FLAGS);
		if (cls.mvdplayback && model->modhint == MOD_BACKPACK) {
			if (cent->contents == IT_ROCKET_LAUNCHER) {
				ent.renderfx |= RF_ROCKETPACK;
			}
			else if (cent->contents == IT_LIGHTNING) {
				ent.renderfx |= RF_LGPACK;
			}
		}

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
	int wep_predict = 0;

	extern void TP_ParsePlayerInfo(player_state_t *, player_state_t *, player_info_t *info);

	num = MSG_ReadByte ();
	if (num >= MAX_CLIENTS)
		Host_Error ("CL_ParsePlayerinfo: num >= MAX_CLIENTS");

	info = &cl.players[num];
	state = &cl.frames[cl.parsecountmod].playerstate[num];
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
			cl.autocam = CAM_TRACK;
			Cam_Lock(num);
			cl.ideal_track = num;
			cls.findtrack = false;
		}

		flags = MSG_ReadShort ();

		state->flags = MVD_TranslateFlags(flags);

		state->messagenum = cl.parsecount;

		state->frame = MSG_ReadByte ();

		state->state_time = cl.parsecounttime;
		state->command.msec = 0;

		for (i = 0; i < 3; i++) 
		{
			if (flags & (DF_ORIGIN << i)) {
				state->origin[i] = MSG_ReadCoord();
			}
		}

		for (i = 0; i < 3; i++) 
		{
			if (flags & (DF_ANGLES << i))
				state->command.angles[i] = MSG_ReadAngle16 ();
		}

		if (flags & DF_MODEL) {
			state->modelindex = MSG_ReadByte();
		}
		else if (cl_fix_mvd.integer && !state->modelindex && !cl.players[num].spectator && cl_modelindices[mi_player] != -1) {
			// old bug in mvd/qtv
			state->modelindex = cl_modelindices[mi_player];
		}

		if (flags & DF_SKINNUM) {
			state->skinnum = MSG_ReadByte();

			if (cls.fteprotocolextensions & FTE_PEXT_MODELDBL) {
				if ((state->skinnum & (1 << 7)) && (flags & DF_MODEL)) {
					state->skinnum &= ~(1 << 7);
					state->modelindex += 256;
				}
				else if (cl.model_precache[state->modelindex] != NULL && cl.model_precache[state->modelindex]->type == mod_brush && state->modelindex + 256 == cl_modelindices[mi_player]) {
					// Temporary hack to detect demos where the modelindex needs to be +256, but not encoded in the skin field
					state->modelindex = cl_modelindices[mi_player];
				}
			}

		}

		if (flags & DF_EFFECTS)
			state->effects = MSG_ReadByte ();

		if (flags & DF_WEAPONFRAME)
			state->weaponframe = MSG_ReadByte ();

		if (cl.vwep_enabled && !(state->flags & PF_GIB) && state->modelindex == cl_modelindices[mi_player] /* no vweps for ring! */) {
			state->vw_index = MVD_WeaponModelNumber(info->stats[STAT_WEAPON]);
		}
		else {
			state->vw_index = 0;
		}
	} 
	else 
	{
		flags = state->flags = (unsigned short)MSG_ReadShort ();

#if defined(FTE_PEXT_TRANS) // and any other fte extension that uses this... (we don't support those yet)
		if (cls.fteprotocolextensions & (FTE_PEXT_TRANS))
		{
			if (flags & PF_FTE_EXTRA)
				flags |= MSG_ReadByte() << 16;
		}
#endif

		state->messagenum = cl.parsecount;
		if (cls.mvdprotocolextensions1 & MVD_PEXT1_FLOATCOORDS) {
			state->origin[0] = MSG_ReadFloatCoord();
			state->origin[1] = MSG_ReadFloatCoord();
			state->origin[2] = MSG_ReadFloatCoord();
		}
		else {
			state->origin[0] = MSG_ReadCoord();
			state->origin[1] = MSG_ReadCoord();
			state->origin[2] = MSG_ReadCoord();
		}
		state->frame = MSG_ReadByte ();

		// the other player's last move was likely some time before the packet was sent out,
		// so accurately track the exact time it was valid at
		if (flags & PF_MSEC) 
		{
			msec = MSG_ReadByte ();
			state->state_time = cl.parsecounttime - msec * 0.001;
		} 
		else
		{
			state->state_time = cl.parsecounttime;
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
		if (flags & PF_MODEL) {
			state->modelindex = MSG_ReadByte();
		}
		else {
			state->modelindex = cl_modelindices[mi_player];
		}

		if (flags & PF_SKINNUM) {
			state->skinnum = MSG_ReadByte();
			if (state->skinnum & (1<<7) && (flags & PF_MODEL)) {
				state->modelindex += 256;
				state->skinnum -= (1<<7);
			}
		}
		else {
			state->skinnum = 0;
		}

		if (flags & PF_EFFECTS)
			state->effects = MSG_ReadByte ();
		else
			state->effects = 0;

		if (flags & PF_WEAPONFRAME)
		{
			state->weaponframe = MSG_ReadByte();

			if (cls.mvdprotocolextensions1 & MVD_PEXT1_WEAPONPREDICTION)
			{
				wep_predict = MSG_ReadByte();
				if (wep_predict)
				{
					int data_impulse = MSG_ReadByte();
					state->impulse = data_impulse;

					state->weapon = MSG_ReadShort();
					state->items = cl.stats[STAT_ITEMS];

					state->client_time = MSG_ReadFloat();
					state->attack_finished = MSG_ReadFloat();
					state->client_nextthink = MSG_ReadFloat();
					state->client_thinkindex = MSG_ReadByte();
					state->client_ping = MSG_ReadByte();
					state->client_predflags = MSG_ReadByte();

					state->ammo_shells = MSG_ReadByte();
					state->ammo_nails = MSG_ReadByte();
					state->ammo_rockets = MSG_ReadByte();
					state->ammo_cells = MSG_ReadByte();
				}
			}
			else
			{
				state->weapon = cl.stats[STAT_ACTIVEWEAPON];
				state->items = cl.stats[STAT_ITEMS];
			}
		}
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
			if (cls.mvdprotocolextensions1 & MVD_PEXT1_FLOATCOORDS) {
				MSG_WriteLongCoord(&cls.demomessage, state->origin[0]);
				MSG_WriteLongCoord(&cls.demomessage, state->origin[1]);
				MSG_WriteLongCoord(&cls.demomessage, state->origin[2]);
			}
			else {
				MSG_WriteCoord(&cls.demomessage, state->origin[0]);
				MSG_WriteCoord(&cls.demomessage, state->origin[1]);
				MSG_WriteCoord(&cls.demomessage, state->origin[2]);
			}
			MSG_WriteByte(&cls.demomessage, state->frame);
			if (flags & PF_MSEC)
				MSG_WriteByte(&cls.demomessage, msec);
			if (flags & PF_COMMAND)
				MSG_WriteDeltaUsercmd(&cls.demomessage, &nullcmd, &state->command, 0);
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
			{
				MSG_WriteByte(&cls.demomessage, state->weaponframe);

				if (cls.mvdprotocolextensions1 & MVD_PEXT1_WEAPONPREDICTION)
				{
					MSG_WriteByte(&cls.demomessage, wep_predict);
					if (wep_predict)
					{
						MSG_WriteByte(&cls.demomessage, state->impulse);
						MSG_WriteShort(&cls.demomessage, state->weapon);

						MSG_WriteFloat(&cls.demomessage, state->client_time);
						MSG_WriteFloat(&cls.demomessage, state->attack_finished);
						MSG_WriteFloat(&cls.demomessage, state->client_nextthink);
						MSG_WriteByte(&cls.demomessage, state->client_thinkindex);
						MSG_WriteByte(&cls.demomessage, state->client_ping);
						MSG_WriteByte(&cls.demomessage, state->client_predflags);

						MSG_WriteByte(&cls.demomessage, state->ammo_shells);
						MSG_WriteByte(&cls.demomessage, state->ammo_nails);
						MSG_WriteByte(&cls.demomessage, state->ammo_rockets);
						MSG_WriteByte(&cls.demomessage, state->ammo_cells);
					}
				}
			}
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

	if ((!cls.mvdplayback || Cam_TrackNum() >= 0) && cl.racing && !CL_SetAlphaByDistance(&newent)) {
		return false;
	}

	CL_AddEntity (&newent);
	return true;
}

static double CL_PlayerTime (void)
{
	double current_time = (cls.demoplayback && !cls.mvdplayback) ? cls.demotime : cls.realtime;
	double playertime = current_time - cls.latency + 0.02;

	return min(playertime, current_time);
}

void CL_StorePausePredictionLocations(void)
{
	int i;
	frame_t* frame = &cl.frames[cl.parsecount & UPDATE_MASK];
	double playertime = CL_PlayerTime();

	for (i = 0; i < MAX_CLIENTS; ++i) {
		predicted_players[i].paused_sec = playertime - frame->playerstate[i].state_time;
	}
}

// Create visible entities in the correct position for all current players
static void CL_LinkPlayers(void)
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
	extern cvar_t cl_debug_antilag_ghost, cl_debug_antilag_view;

	frame = &cl.frames[cl.parsecount & UPDATE_MASK];
	memset (&ent, 0, sizeof(entity_t));

	for (j = 0; j < MAX_CLIENTS; j++, info++, state++) 
	{
		info = &cl.players[j];
		state = &frame->playerstate[j];
		cent = &cl_entities[j + 1];

		predicted_players[j].drawn = false;

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

		if (cl_gibfilter.value && state->modelindex == cl_modelindices[mi_h_player]) {
			continue;
		}

		if (state->modelindex == cl_modelindices[mi_player])
		{
			i = state->frame;

			if (cl_deadbodyfilter.value == 3 && !cl.teamfortress) {
				// will instantly filter dead bodies unless gamedir is fortress (in TF they may be feigned spies)
				if (ISDEAD(i))
					continue;
			}
			else if (cl_deadbodyfilter.value == 2)  {
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
			ent.renderfx |= RF_PLAYERMODEL;
		}

		ent.skinnum = state->skinnum;
		ent.colormap = info->translations;
		ent.scoreboard = (state->modelindex == cl_modelindices[mi_player]) ? info : NULL;
		ent.frame = state->frame;
		ent.effects = state->effects; // Electro - added for shells
		ent.alpha = (float)state->alpha / 255;

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
		msec = (cl_predict_half.value ? 500 : 1000) * (cl.paused ? predicted_players[j].paused_sec : (playertime - state->state_time));
		if (msec <= 0 || !cl_predict_players.value || cls.mvdplayback) {
			VectorCopy (state->origin, ent.origin);
			VectorCopy(ent.origin, predicted_players[j].drawn_origin);
			predicted_players[j].msec = 0;
			predicted_players[j].drawn = true;
		}
		else {
			// predict players movement
			msec = min(msec, 255);
			state->command.msec = msec;

			oldphysent = pmove.numphysent;
			CL_SetSolidPlayers(j);
			CL_PredictUsercmd(state, &exact, &state->command, false);
			pmove.numphysent = oldphysent;
			VectorCopy(exact.origin, ent.origin);
			VectorCopy(exact.origin, predicted_players[j].drawn_origin);
			predicted_players[j].msec = msec;
			predicted_players[j].drawn = true;
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

		// Set alpha after origin determined
		if ((!cls.mvdplayback || Cam_TrackNum() >= 0) && cl.racing && !CL_SetAlphaByDistance(&ent)) {
			continue;
		}

		// Add mvd ghost
		if (cls.mvdplayback && cl_debug_antilag_ghost.integer != cl_debug_antilag_view.integer) {
			vec3_t temp;
			float old_alpha;

			VectorCopy(ent.origin, temp);
			old_alpha = ent.alpha;

			ent.alpha = 0.2f;
			switch (cl_debug_antilag_ghost.integer) {
			case 0:
				if (state->antilag_flags & dbg_antilag_position_set) {
					VectorCopy(state->current_origin, ent.origin);
					CL_AddEntity(&ent);
				}
				break;
			case 1:
				if (state->antilag_flags & dbg_antilag_rewind_present) {
					VectorCopy(state->rewind_origin, ent.origin);
					CL_AddEntity(&ent);
				}
				break;
			case 2:
				if (state->antilag_flags & dbg_antilag_client_present) {
					VectorCopy(state->client_origin, ent.origin);
					CL_AddEntity(&ent);
				}
				break;
			}

			VectorCopy(temp, ent.origin);
			ent.alpha = old_alpha;
		}

		VectorCopy (ent.origin, cent->lerp_origin);

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
					if (Cam_TrackNum() >= 0 && cl.racing) {
						CL_SetAlphaByDistance(&ent);
					}
					CL_AddEntity (&ent);
				}
				else 
				{
					// server said don't add vwep player model
				}
			}
			else {
				CL_AddEntity(&ent);
			}
		}
		else {
			CL_AddEntity(&ent);
		}
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

	frame = &cl.frames[cl.parsecountmod];
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

				CL_PredictUsercmd (state, &exact, &state->command, false);
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

	if (cls.nqdemoplayback) {
		NQD_LinkEntities();
	}
	else {
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
	qbool dead_body, was_dead_body;
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

		// Identify dead bodies
		dead_body = (state->modelindex == cl_modelindices[mi_player] && ISDEAD(state->frame)) || state->modelindex == cl_modelindices[mi_h_player];
		was_dead_body = (oldstate->modelindex == cl_modelindices[mi_player] && ISDEAD(oldstate->frame)) || oldstate->modelindex == cl_modelindices[mi_h_player];

		// Don't lerp if respawning
		if (!dead_body && was_dead_body) {
			continue;
		}

		// Don't lerp if first frame being gibbed
		if (state->modelindex == cl_modelindices[mi_h_player] && !was_dead_body) {
			continue;
		}

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

void MVD_Interpolate(void)
{
	int i, j;
	float f;
	frame_t	*frame, *oldframe;
	player_state_t *state, *oldstate, *self, *oldself;
	struct predicted_player *pplayer;
	static double old;

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
		extern cvar_t cl_debug_antilag_view;
		extern cvar_t cl_debug_antilag_ghost;
		extern cvar_t cl_debug_antilag_self;

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

		// copy the antilag values for currently viewed client
		VectorCopy(state->origin, state->current_origin);
		VectorCopy(cl.antilag_positions[i].pos, state->rewind_origin);
		VectorCopy(cl.antilag_positions[i].clientpos, state->client_origin);
		state->antilag_flags = (cl.antilag_positions[i].present ? dbg_antilag_rewind_present : 0) | (cl.antilag_positions[i].clientpresent ? dbg_antilag_client_present : 0);

		if (i == cl.viewplayernum && !cl_debug_antilag_self.integer) {
			// only move the other players, not the current view
			continue;
		}

		// show the antilagged positions from the current player's point of view
		if (cl_debug_antilag_view.integer && cl.spectator == cl.autocam) {
			if (cl_debug_antilag_view.integer == 1) {
				if (cl.antilag_positions[i].present) {
					VectorCopy(cl.antilag_positions[i].pos, state->origin);
					state->antilag_flags |= dbg_antilag_position_set;
				}
			}
			else if (cl_debug_antilag_view.integer == 2) {
				if (cl.antilag_positions[i].clientpresent) {
					VectorCopy(cl.antilag_positions[i].clientpos, state->origin);
					state->antilag_flags |= dbg_antilag_position_set;
				}
			}
		}
	}
}

void CL_ClearPredict(void) {
	memset(predicted_players, 0, sizeof(predicted_players));
	mvd_fixangle = 0;
	pmove.effect_frame = 0;
	pmove.t_width = 0;
	pmove.impulse = 0;
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

qbool CL_DrawnPlayerPosition(int player_num, float* pos, int* msec)
{
	if (pos) {
		VectorCopy(predicted_players[player_num].drawn_origin, pos);
	}
	if (msec) {
		*msec = predicted_players[player_num].msec;
	}
	return predicted_players[player_num].drawn;
}
