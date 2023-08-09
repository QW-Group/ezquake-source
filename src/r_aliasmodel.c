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

// Alias model (.mdl) processing
// Most code taken from gl_rmain.c

#include "quakedef.h"
#include "gl_model.h"
#include "vx_stuff.h"
#include "vx_vertexlights.h"
#include "utils.h"
#include "qsound.h"
#include "hud.h"
#include "hud_common.h"
#include "rulesets.h"
#include "teamplay.h"
#include "r_aliasmodel.h"
#include "crc.h"
#include "qmb_particles.h"
#include "r_matrix.h"
#include "r_local.h"
#include "r_framestats.h"
#include "r_trace.h"
#include "r_lighting.h"
#include "r_renderer.h"

texture_ref shelltexture;
model_t* cl_custommodels[custom_model_count];

// precalculated dot products for quantized angles
byte      r_avertexnormal_dots[SHADEDOT_QUANT][NUMVERTEXNORMALS] =
#include "anorm_dots.h"
;
float     r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};
byte      *shadedots = r_avertexnormal_dots[0];

/*
==============================================================================
ALIAS MODELS
==============================================================================
*/

aliashdr_t	*pheader;

stvert_t	stverts[MAXALIASVERTS];
mtriangle_t	triangles[MAXALIASTRIS];

// a pose is a single set of vertexes.  a frame may be
// an animating sequence of poses
trivertx_t	*poseverts[MAXALIASFRAMES];
int			posenum;

static void* Mod_LoadAliasFrame(void* pin, maliasframedesc_t *frame, int* posenum);
static void* Mod_LoadAliasGroup(void* pin, maliasframedesc_t *frame, int* posenum);
void* Mod_LoadAllSkins(model_t* loadmodel, int numskins, daliasskintype_t *pskintype);

static cvar_t    gl_shaftlight = { "gl_shaftlight", "1" };
cvar_t    r_lerpmuzzlehack = { "r_lerpmuzzlehack", "1" };
cvar_t    gl_powerupshells_effect1level = { "gl_powerupshells_effect1level", "0.75" };
cvar_t    gl_powerupshells_base1level = { "gl_powerupshells_base1level", "0.05" };
cvar_t    gl_powerupshells_effect2level = { "gl_powerupshells_effect2level", "0.4" };
cvar_t    gl_powerupshells_base2level = { "gl_powerupshells_base2level", "0.1" };
cvar_t    gl_custom_grenade_tf = { "gl_custom_grenade_tf", "1" };

float     r_framelerp;

extern float     bubblecolor[NUM_DLIGHTTYPES][4];

extern cvar_t    r_lerpframes;
extern cvar_t    gl_outline;

static custom_model_color_t custom_model_colors[] = {
	// LG beam
	{
		{ "gl_custom_lg_color", "", CVAR_COLOR },
		{ "gl_custom_lg_fullbright", "1" },
		&amf_lightning,
		MOD_THUNDERBOLT,
		0,
		true
	},
	// Rockets
	{
		{ "gl_custom_rocket_color", "", CVAR_COLOR },
		{ "gl_custom_rocket_fullbright", "1" },
		NULL,
		MOD_ROCKET,
		0,
		true
	},
	// Grenades
	{
		{ "gl_custom_grenade_color", "", CVAR_COLOR },
		{ "gl_custom_grenade_fullbright", "1" },
		NULL,
		MOD_GRENADE,
		0,
		true
	},
	// Spikes
	{
		{ "gl_custom_spike_color", "", CVAR_COLOR },
		{ "gl_custom_spike_fullbright", "1" },
		&amf_part_spikes,
		MOD_SPIKE,
		0,
		true
	},
	// Backpacks with RL inside
	{
		{ "gl_custom_rlpack_color", "255 64 64", CVAR_COLOR },
		{ "", "0" },
		NULL,
		MOD_BACKPACK,
		RF_ROCKETPACK,
		false
	},
	// Backpacks with LG inside
	{
		{ "gl_custom_lgpack_color", "64 64 255", CVAR_COLOR },
		{ "", "0" },
		NULL,
		MOD_BACKPACK,
		RF_LGPACK,
		false
	}
};

static qbool IsFlameModel(model_t* model)
{
	return model->modhint == MOD_FLAME || model->modhint == MOD_FLAME0 || model->modhint == MOD_FLAME3;
}

static void R_RenderAliasModelEntity(
	entity_t* ent, aliashdr_t *paliashdr, byte *color32bit,
	texture_ref texture, texture_ref fb_texture, maliasframedesc_t* oldframe, maliasframedesc_t* frame,
	qbool outline, int effects
)
{
	int i;
	model_t* model = ent->model;

	ent->r_modelcolor[0] = -1;  // by default no solid fill color for model, using texture
	if (color32bit) {
		// force some color for such model
		for (i = 0; i < 3; i++) {
			ent->r_modelcolor[i] = color32bit[i] / 255.0;
			ent->r_modelcolor[i] = bound(0, ent->r_modelcolor[i], 1);
		}

		fb_texture = null_texture_reference;
	}

	R_SetupAliasFrame(ent, model, oldframe, frame, outline, texture, fb_texture, effects, ent->renderfx);
}

qbool R_CullAliasModel(entity_t* ent, maliasframedesc_t* oldframe, maliasframedesc_t* frame)
{
	vec3_t mins, maxs;

	//culling
	if (!(ent->renderfx & RF_WEAPONMODEL)) {
		if (ent->angles[0] || ent->angles[1] || ent->angles[2]) {
			if (R_CullSphere(ent->origin, max(oldframe->radius, frame->radius))) {
				return true;
			}
		}
		else {
			if (r_framelerp == 1) {
				VectorAdd(ent->origin, frame->bboxmin, mins);
				VectorAdd(ent->origin, frame->bboxmax, maxs);
			}
			else {
				int i;
				for (i = 0; i < 3; i++) {
					mins[i] = ent->origin[i] + min(oldframe->bboxmin[i], frame->bboxmin[i]);
					maxs[i] = ent->origin[i] + max(oldframe->bboxmax[i], frame->bboxmax[i]);
				}
			}
			if (R_CullBox(mins, maxs)) {
				return true;
			}
		}
	}

	return false;
}

// FIXME: Move filtering options to cl_ents.c
qbool R_FilterEntity(entity_t* ent)
{
	// VULT NAILTRAIL - Hidenails
	if (amf_hidenails.value && ent->model->modhint == MOD_SPIKE) {
		return true;
	}

	// VULT ROCKETTRAILS - Hide rockets
	if (amf_hiderockets.value && (ent->model->flags & EF_ROCKET)) {
		return true;;
	}

	// VULT CAMERAS - Show/Hide playermodel
	if (ent->alpha == -1) {
		if (cameratype == C_NORMAL) {
			return true;
		}
		ent->alpha = 1;
		return false;
	}
	else if (ent->alpha < 0) {
		// VULT MOTION TRAILS
		return true;
	}

	// Handle flame/flame0 model changes
	if (qmb_initialized) {
		if (!amf_part_fire.integer && ent->model->modhint == MOD_FLAME0) {
			ent->model = cl.model_precache[cl_modelindices[mi_flame]];
		}
		else if (amf_part_fire.integer) {
			if (ent->model->modhint == MOD_FLAME0) {
				if (!ISPAUSED) {
					ParticleTorchFire(ent);
				}
			}
			else if (ent->model->modhint == MOD_FLAME && cl_custommodels[custom_model_flame0]) {
				// do we have progs/flame0.mdl?
				if (!ISPAUSED) {
					ParticleTorchFire(ent);
				}
				ent->model = cl_custommodels[custom_model_flame0];
			}
			else if (ent->model->modhint == MOD_FLAME2 || ent->model->modhint == MOD_FLAME3) {
				if (!ISPAUSED) {
					ParticleTorchFire(ent);
				}
				return true;
			}
		}
	}

	return false;
}

void R_OverrideModelTextures(entity_t* ent, texture_ref* texture, texture_ref* fb_texture, byte** color32bit)
{
	int playernum = -1;

	if (ent->scoreboard && (ent->renderfx & RF_VWEPMODEL) == 0) {
		playernum = ent->scoreboard - cl.players;
	}

	if (playernum >= 0 && playernum < MAX_CLIENTS) {
		R_SetSkinForPlayerEntity(ent, texture, fb_texture, color32bit);
	}
	// TODO: Can we move the custom_model logic to here?  If fullbright, nullify textures and set color?

	if (ent->full_light || !gl_fb_models.integer) {
		*fb_texture = null_texture_reference;
	}
}

static qbool R_CanDrawModelShadow(entity_t* ent)
{
	return (r_shadows.integer && !ent->full_light && !(ent->renderfx & RF_NOSHADOW)) && !ent->alpha;
}

void R_DrawAliasModel(entity_t *ent, qbool outline)
{
	int anim, skinnum;
	texture_ref texture, fb_texture;
	aliashdr_t* paliashdr;
	maliasframedesc_t *oldframe, *frame;
	byte *color32bit = NULL;
	float oldMatrix[16];

	if (R_FilterEntity(ent)) {
		return;
	}

	// Meag: Do not move this above R_FilterEntity(), it might change the model... :(
	paliashdr = (aliashdr_t *)Mod_Extradata(ent->model); // locate the proper data

	//VULT CORONAS
	if (amf_coronas.integer) {
		if (IsFlameModel(ent->model)) {
			//FIXME: This is slow and pathetic as hell, really we should just check the entity
			//alternatively add some kind of permanent client side TE for the torch
			NewStaticLightCorona(C_FIRE, ent->origin, ent->entity_id);
		}
		else if (ent->model->modhint == MOD_TELEPORTDESTINATION) {
			NewStaticLightCorona(C_LIGHTNING, ent->origin, ent->entity_id);
		}
	}

	ent->frame = bound(0, ent->frame, paliashdr->numframes - 1);
	ent->oldframe = bound(0, ent->oldframe, paliashdr->numframes - 1);

	frame = &paliashdr->frames[ent->frame];
	oldframe = &paliashdr->frames[ent->oldframe];

	r_framelerp = 1.0;
	if (r_lerpframes.integer && ent->framelerp >= 0 && ent->oldframe != ent->frame) {
		r_framelerp = min(ent->framelerp, 1);
	}

	if (R_CullAliasModel(ent, oldframe, frame)) {
		return;
	}

	frameStats.classic.polycount[polyTypeAliasModel] += paliashdr->numtris;

	R_TraceEnterRegion(va("%s(%s)", __func__, ent->model->name), true);
	R_PushModelviewMatrix(oldMatrix);
	R_StateBeginDrawAliasModel(ent, paliashdr);

	//get lighting information
	R_AliasSetupLighting(ent);
	shadedots = r_avertexnormal_dots[((int)(ent->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];
	ent->r_modelalpha = (ent->alpha ? ent->alpha : 1);

	if (ent->r_modelalpha != 1 && outline) {
		// todo
		R_PopModelviewMatrix(oldMatrix);
		R_TraceLeaveRegion(true);
		return;
	}

	anim = (int)(r_refdef2.time * 10) & 3;
	skinnum = ent->skinnum;
	if (skinnum >= paliashdr->numskins || skinnum < 0) {
		Com_DPrintf("R_DrawAliasModel: no such skin # %d\n", skinnum);
		skinnum = 0;
	}

	texture = paliashdr->gl_texturenum[skinnum][anim];
	fb_texture = paliashdr->glc_fb_texturenum[skinnum][anim];

	R_OverrideModelTextures(ent, &texture, &fb_texture, &color32bit);
	R_RenderAliasModelEntity(ent, paliashdr, color32bit, texture, fb_texture, oldframe, frame, outline, ent->effects);
	R_PopModelviewMatrix(oldMatrix);

	if (R_CanDrawModelShadow(ent)) {
		renderer.DrawAliasModelShadow(ent);
	}

	R_TraceLeaveRegion(true);

	return;
}

int R_AliasFramePose(const maliasframedesc_t* frame)
{
	int pose, numposes;
	float interval;

	pose = frame->firstpose;
	numposes = frame->numposes;
	if (numposes > 1) {
		interval = frame->interval;
		pose += (int)(r_refdef2.time / interval) % numposes;
	}

	return pose;
}

void R_SetupAliasFrame(
	entity_t* ent,
	model_t* model,
	maliasframedesc_t *oldframe, maliasframedesc_t *frame,
	qbool outline,
	texture_ref texture, texture_ref fb_texture,
	int effects, int render_effects
)
{
	extern cvar_t gl_lumatextures;
	int oldpose, pose;
	float lerp;

	R_AliasModelDeterminePoses(oldframe, frame, &oldpose, &pose, &lerp);

	if (!gl_lumatextures.integer) {
		R_TextureReferenceInvalidate(fb_texture);
	}

	renderer.DrawAliasFrame(ent, model, oldpose, pose, texture, fb_texture, outline, effects, render_effects, lerp);
}

void R_AliasModelDeterminePoses(const maliasframedesc_t* oldframe, const maliasframedesc_t* frame, int* prevpose, int* nextpose, float* lerpfrac)
{
	int oldpose = R_AliasFramePose(oldframe);
	int pose = R_AliasFramePose(frame);
	float lerp = 0;

	if (oldframe->nextpose == pose) {
		lerp = r_framelerp;
	}
	else if (frame->nextpose == oldpose) {
		int temp = pose;

		pose = oldpose;
		oldpose = temp;
		lerp = (1 - r_framelerp);
	}
	else {
		lerp = 1;
	}

	if (lerp == 1) {
		oldpose = pose;
		lerp = 0;
	}
	else if (lerp == 0) {
		pose = oldpose;
	}

	*prevpose = oldpose;
	*nextpose = pose;
	*lerpfrac = lerp;
}

static void R_AliasModelScaleLight(entity_t* ent)
{
	float max_component;

	max_component = max(ent->lightcolor[0], ent->lightcolor[1]);
	max_component = max(max_component, ent->lightcolor[2]);

	if (max_component >= 256) {
		VectorScale(ent->lightcolor, 255 / max_component, ent->lightcolor);
	}
	else if (max_component < cl.minlight) {
		ent->lightcolor[0] += cl.minlight;
		ent->lightcolor[1] += cl.minlight;
		ent->lightcolor[2] += cl.minlight;
	}
}

static void R_AliasModelColoredLighting(entity_t* ent)
{
	int i, j, k, lnum;
	vec3_t dist;
	float add, added = 0;

	/* FIXME: dimman... cache opt from fod */
	//VULT COLOURED MODEL LIGHTS
	ent->bestlight = -1;
	for (i = 0; i < MAX_DLIGHTS / 32; i++) {
		if (!cl_dlight_active[i]) {
			continue;
		}

		for (j = 0; j < 32; j++) {
			if ((cl_dlight_active[i] & (1 << j)) && i * 32 + j < MAX_DLIGHTS) {
				lnum = i * 32 + j;

				VectorSubtract(ent->origin, cl_dlights[lnum].origin, dist);
				add = cl_dlights[lnum].radius - VectorLength(dist);

				if (add > 0) {
					vec3_t dlight_color;

					if (amf_lighting_vertex.integer && (ent->bestlight < 0 || cl_dlights[lnum].radius > cl_dlights[ent->bestlight].radius)) {
						ent->bestlight = lnum;
					}
					added += add;

					if (cl_dlights[lnum].type == lt_custom) {
						VectorCopy(cl_dlights[lnum].color, dlight_color);
						VectorScale(dlight_color, (1.0 / 255), dlight_color); // convert color from byte to float
					}
					else {
						VectorCopy(bubblecolor[cl_dlights[lnum].type], dlight_color);
					}

					for (k = 0; k < 3; k++) {
						ent->lightcolor[k] += (dlight_color[k] * add) * 2;
					}
				}
			}
		}
	}

	ent->ambientlight += added;
	R_AliasModelScaleLight(ent);
}

static void R_AliasModelStandardLighting(entity_t* ent)
{
	int i, j, lnum;
	vec3_t dist;
	float add, added = 0;

	/* FIXME: dimman... cache opt from fod */
	ent->bestlight = -1;
	for (i = 0; i < MAX_DLIGHTS / 32; i++) {
		if (cl_dlight_active[i]) {
			for (j = 0; j < 32; j++) {
				if ((cl_dlight_active[i] & (1 << j)) && i * 32 + j < MAX_DLIGHTS) {
					lnum = i * 32 + j;

					VectorSubtract(ent->origin, cl_dlights[lnum].origin, dist);
					add = cl_dlights[lnum].radius - VectorLength(dist);

					if (add > 0) {
						if (amf_lighting_vertex.integer && (ent->bestlight < 0 || cl_dlights[lnum].radius > cl_dlights[ent->bestlight].radius)) {
							ent->bestlight = lnum;
						}
						added += add;
					}
				}
			}
		}
	}

	ent->ambientlight += added;
	ent->lightcolor[0] += added;
	ent->lightcolor[1] += added;
	ent->lightcolor[2] += added;
}

void R_AliasSetupLighting(entity_t *ent)
{
	float fbskins = 0;
	unsigned int i;
	model_t* clmodel = ent->model;
	qbool player_model = (clmodel->modhint == MOD_PLAYER || ent->renderfx & RF_PLAYERMODEL);
	qbool calculate_lighting = true;

	//VULT COLOURED MODEL LIGHTING
	ent->custom_model = NULL;
	ent->ambientlight = ent->shadelight = 0;
	for (i = 0; i < sizeof(custom_model_colors) / sizeof(custom_model_colors[0]); ++i) {
		custom_model_color_t* test = &custom_model_colors[i];
		if (test->model_hint && test->model_hint == clmodel->modhint && (test->renderfx == 0 || test->renderfx == ent->renderfx)) {
			if (test->color_cvar.string[0] && (test->amf_cvar == NULL || test->amf_cvar->integer == 0)) {
				ent->custom_model = &custom_model_colors[i];
			}
			break;
		}
	}

	if (clmodel->modhint == MOD_GRENADE && !gl_custom_grenade_tf.integer && cl.teamfortress) {
		ent->custom_model = NULL;
	}

	if (ent->custom_model && ent->custom_model->fullbright_cvar.integer) {
		ent->ambientlight = 4096;
		ent->shadelight = 0;
		ent->full_light = true;
		return;
	}

	// make thunderbolt and torches full light
	if (clmodel->modhint == MOD_THUNDERBOLT) {
		ent->ambientlight = 60 + 150 * bound(0, gl_shaftlight.value, 1);
		ent->shadelight = 0;
		ent->full_light = true;
		return;
	}
	else if (clmodel->modhint == MOD_FLAME || clmodel->modhint == MOD_FLAME2) {
		ent->ambientlight = 255;
		ent->shadelight = 0;
		ent->full_light = true;
		return;
	}

	//normal lighting
	ent->full_light = false;
	if (player_model) {
		fbskins = bound(0, r_fullbrightSkins.value, r_refdef2.max_fbskins);
		if (fbskins >= 1 && gl_fb_models.integer == 1) {
			ent->ambientlight = ent->shadelight = 4096;
			ent->full_light = true;
			calculate_lighting = false;
		}
		else {
			ent->full_light = fbskins > 0;
			calculate_lighting = true;
		}
	}
	else if (Rulesets_FullbrightModel(clmodel)) {
		ent->ambientlight = ent->shadelight = 4096;
		calculate_lighting = (r_shadows.integer);
	}

	if (calculate_lighting) {
		R_LightEntity(ent);

		if (amf_lighting_colour.integer) {
			R_AliasModelColoredLighting(ent);
		}
		else {
			R_AliasModelStandardLighting(ent);
		}

		if (player_model) {
			ent->ambientlight = max(ent->ambientlight, 8 + fbskins * 120);
			ent->shadelight = max(ent->shadelight, 8 + fbskins * 120);
		}
		else {
			// clamp lighting so it doesn't overbright as much
			ent->ambientlight = min(ent->ambientlight, 128);
			if (ent->ambientlight + ent->shadelight > 192) {
				ent->shadelight = 192 - ent->ambientlight;
			}
		}
	}

	// always give the gun some light
	if ((ent->renderfx & RF_WEAPONMODEL) && ent->ambientlight < 24) {
		ent->ambientlight = ent->shadelight = 24;
	}

	// never allow players to go totally black
	if (player_model && ent->ambientlight < 8) {
		ent->ambientlight = ent->shadelight = 8;
	}

	if (ent->ambientlight < cl.minlight) {
		ent->ambientlight = ent->shadelight = cl.minlight;
	}
}

void R_DrawViewModel(void)
{
	extern cvar_t cl_drawgun;
	centity_t *cent = CL_WeaponModelForView();
	static entity_t gun;

	//VULT CAMERA - Don't draw gun in external camera
	if (cameratype != C_NORMAL) {
		return;
	}

	if (!r_drawentities.value || !cent->current.modelindex || cl_drawgun.value <= 0) {
		return;
	}

	memset(&gun, 0, sizeof(gun));

	if (!(gun.model = cl.model_precache[cent->current.modelindex])) {
		Host_Error("R_DrawViewModel: bad modelindex");
	}

	VectorCopy(cent->current.origin, gun.origin);
	VectorCopy(cent->current.angles, gun.angles);
	gun.colormap = vid.colormap;
	gun.renderfx = RF_WEAPONMODEL | RF_NOSHADOW | gun.model->renderfx;

	gun.effects |= (cl.stats[STAT_ITEMS] & IT_QUAD) ? EF_BLUE : 0;
	gun.effects |= (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) ? EF_RED : 0;
	gun.effects |= (cl.stats[STAT_ITEMS] & IT_SUIT) ? EF_GREEN : 0;

	gun.frame = cent->current.frame;
	if (cent->frametime >= 0 && cent->frametime <= r_refdef2.time) {
		gun.oldframe = cent->oldframe;
		gun.framelerp = (r_refdef2.time - cent->frametime) * 10;
	}
	else {
		gun.oldframe = gun.frame;
		gun.framelerp = -1;
	}
	//Con_Printf("Lerping %d=>%d\n", gun.oldframe, gun.frame);

	gun.alpha = bound(0, cl_drawgun.value, 1);

	if (R_UseImmediateOpenGL() && !gl_mtexable) {
		gun.alpha = 0;
	}

	switch (gun.model->type) {
		case mod_alias:
			// don't support outlining weaponmodel just yet
			R_DrawAliasModel(&gun, false);
			if (gun.effects) {
				renderer.DrawAliasModelPowerupShell(&gun);
			}
			break;
		case mod_alias3:
			renderer.DrawAlias3Model(&gun, false, false);
			if (gun.effects) {
				renderer.DrawAlias3ModelPowerupShell(&gun);
			}
			renderer.DrawAlias3Model(&gun, false, true);
			break;
		default:
			Com_Printf("Not drawing view model of type %i\n", gun.model->type);
			break;
	}
}

void R_InitAliasModelCvars(void)
{
	int i;

	for (i = 0; i < sizeof(custom_model_colors) / sizeof(custom_model_colors[0]); ++i) {
		Cvar_Register(&custom_model_colors[i].color_cvar);
		Cvar_Register(&custom_model_colors[i].fullbright_cvar);
	}

	Cvar_Register(&r_lerpmuzzlehack);
	Cvar_Register(&gl_shaftlight);

	Cvar_Register(&gl_powerupshells_base1level);
	Cvar_Register(&gl_powerupshells_effect1level);
	Cvar_Register(&gl_powerupshells_base2level);
	Cvar_Register(&gl_powerupshells_effect2level);
	Cvar_Register(&gl_custom_grenade_tf);
}

void Mod_LoadAliasModel(model_t *mod, void *buffer, int filesize, const char* loadname)
{
	int i, j, version, numframes, size, start, end, total;
	mdl_t *pinmodel;
	stvert_t *pinstverts;
	dtriangle_t *pintriangles;
	daliasframetype_t *pframetype;
	daliasskintype_t *pskintype;
	aliasframetype_t frametype;
	int posenum;

	//VULT MODELS
	Mod_AddModelFlags(mod);

	if (mod->modhint == MOD_PLAYER || mod->modhint == MOD_EYES) {
		mod->crc = CRC_Block(buffer, filesize);
	}

	start = Hunk_LowMark();

	pinmodel = (mdl_t *)buffer;

	version = LittleLong(pinmodel->version);

	if (version != ALIAS_VERSION) {
		Hunk_FreeToLowMark(start);
		Host_Error("Mod_LoadAliasModel: %s has wrong version number (%i should be %i)\n", mod->name, version, ALIAS_VERSION);
		return;
	}

	// allocate space for a working header, plus all the data except the frames, skin and group info
	size = sizeof(aliashdr_t) + (LittleLong(pinmodel->numframes) - 1) * sizeof(pheader->frames[0]);
	pheader = (aliashdr_t *)Hunk_AllocName(size, loadname);

	mod->flags = LittleLong(pinmodel->flags);

	// endian-adjust and copy the data, starting with the alias model header
	pheader->boundingradius = LittleFloat(pinmodel->boundingradius);
	pheader->numskins = LittleLong(pinmodel->numskins);
	pheader->skinwidth = LittleLong(pinmodel->skinwidth);
	pheader->skinheight = LittleLong(pinmodel->skinheight);

	if (pheader->skinheight > MAX_LBM_HEIGHT) {
		Host_Error("Mod_LoadAliasModel: model %s has a skin taller than %d", mod->name, MAX_LBM_HEIGHT);
	}

	pheader->numverts = LittleLong(pinmodel->numverts);

	if (pheader->numverts <= 0) {
		Host_Error("Mod_LoadAliasModel: model %s has no vertices", mod->name);
	}

	if (pheader->numverts > MAXALIASVERTS) {
		Host_Error("Mod_LoadAliasModel: model %s has too many vertices", mod->name);
	}

	pheader->numtris = LittleLong(pinmodel->numtris);

	if (pheader->numtris <= 0) {
		Host_Error("Mod_LoadAliasModel: model %s has no triangles", mod->name);
	}

	pheader->numframes = LittleLong(pinmodel->numframes);
	numframes = pheader->numframes;
	if (numframes < 1) {
		Host_Error("Mod_LoadAliasModel: model %s has invalid # of frames: %d\n", mod->name, numframes);
	}

	pheader->size = LittleFloat(pinmodel->size) * ALIAS_BASE_SIZE_RATIO;
	mod->synctype = LittleLong(pinmodel->synctype);
	mod->numframes = pheader->numframes;

	for (i = 0; i < 3; i++) {
		pheader->scale[i] = LittleFloat(pinmodel->scale[i]);
		pheader->scale_origin[i] = LittleFloat(pinmodel->scale_origin[i]);
		pheader->eyeposition[i] = LittleFloat(pinmodel->eyeposition[i]);
	}

	// load the skins
	pskintype = (daliasskintype_t *)&pinmodel[1];
	pskintype = Mod_LoadAllSkins(mod, pheader->numskins, pskintype);

	// load base s and t vertices
	pinstverts = (stvert_t *)pskintype;

	for (i = 0; i < pheader->numverts; i++) {
		stverts[i].onseam = LittleLong(pinstverts[i].onseam);
		stverts[i].s = LittleLong(pinstverts[i].s);
		stverts[i].t = LittleLong(pinstverts[i].t);
	}

	// load triangle lists
	pintriangles = (dtriangle_t *)&pinstverts[pheader->numverts];

	for (i = 0; i < pheader->numtris; i++) {
		triangles[i].facesfront = LittleLong(pintriangles[i].facesfront);

		for (j = 0; j < 3; j++) {
			int index = triangles[i].vertindex[j] = LittleLong(pintriangles[i].vertindex[j]);
			if (index < 0 || index >= pheader->numverts) {
				Host_Error("Mod_LoadAliasModel: model %s has invalid vertex ref (%d/%d)\n", mod->name, index, pheader->numverts);
			}
		}
	}

	// load the frames
	posenum = 0;
	pframetype = (daliasframetype_t *)&pintriangles[pheader->numtris];

	mod->mins[0] = mod->mins[1] = mod->mins[2] = 255;
	mod->maxs[0] = mod->maxs[1] = mod->maxs[2] = 0;

	for (i = 0; i < numframes; i++) {
		frametype = LittleLong(pframetype->type);

		if (frametype == ALIAS_SINGLE) {
			pframetype = (daliasframetype_t *)Mod_LoadAliasFrame(pframetype + 1, &pheader->frames[i], &posenum);
		}
		else {
			pframetype = (daliasframetype_t *)Mod_LoadAliasGroup(pframetype + 1, &pheader->frames[i], &posenum);
		}

		for (j = 0; j < 3; j++) {
			mod->mins[j] = min(mod->mins[j], pheader->frames[i].bboxmin[j]);
			mod->maxs[j] = max(mod->maxs[j], pheader->frames[i].bboxmax[j]);
		}
	}

	mod->radius = RadiusFromBounds(mod->mins, mod->maxs);

	pheader->numposes = posenum;

	mod->type = mod_alias;

	// build the draw lists
	renderer.PrepareAliasModel(mod, pheader);

	// move the complete, relocatable alias model to the cache
	end = Hunk_LowMark();
	total = end - start;

	mod->cached_data = Q_malloc(total);
	memcpy(mod->cached_data, pheader, total);

	// try load simple textures
	memset(mod->simpletexture, 0, sizeof(mod->simpletexture));
	for (i = 0; i < MAX_SIMPLE_TEXTURES && i < pheader->numskins; i++) {
		mod->simpletexture[i] = Mod_LoadSimpleTexture(mod, i);
	}

	Hunk_FreeToLowMark(start);
}

static void* Mod_LoadAliasFrame(void * pin, maliasframedesc_t *frame, int* posenum)
{
	trivertx_t *pinframe;
	int i, len;
	daliasframe_t *pdaliasframe;

	pdaliasframe = (daliasframe_t *)pin;

	strlcpy(frame->name, pdaliasframe->name, sizeof(frame->name));
	strlcpy(frame->groupname, frame->name, sizeof(frame->groupname));
	frame->firstpose = *posenum;
	frame->numposes = 1;
	frame->groupnumber = 0;

	for (len = strlen(frame->groupname); len > 0 && isdigit(frame->groupname[len - 1]); --len) {
		frame->groupnumber *= 10;
		frame->groupnumber += (frame->groupname[len - 1] - '0');

		frame->groupname[len - 1] = '\0';
	}

	for (i = 0; i < 3; i++) {
		// these are byte values, so we don't have to worry about endianness
		frame->bboxmin[i] = pdaliasframe->bboxmin.v[i] * pheader->scale[i] + pheader->scale_origin[i];
		frame->bboxmax[i] = pdaliasframe->bboxmax.v[i] * pheader->scale[i] + pheader->scale_origin[i];
	}
	frame->radius = RadiusFromBounds(frame->bboxmin, frame->bboxmax);

	pinframe = (trivertx_t *)(pdaliasframe + 1);

	poseverts[*posenum] = pinframe;
	(*posenum)++;

	pinframe += pheader->numverts;

	return (void *)pinframe;
}

static void* Mod_LoadAliasGroup(void * pin, maliasframedesc_t *frame, int* posenum)
{
	daliasgroup_t *pingroup;
	int i, numframes;
	daliasinterval_t *pin_intervals;
	void *ptemp;

	pingroup = (daliasgroup_t *)pin;

	numframes = LittleLong(pingroup->numframes);

	frame->firstpose = *posenum;
	frame->numposes = numframes;

	for (i = 0; i < 3; i++) {
		// these are byte values, so we don't have to worry about endianness
		frame->bboxmin[i] = pingroup->bboxmin.v[i] * pheader->scale[i] + pheader->scale_origin[i];
		frame->bboxmax[i] = pingroup->bboxmax.v[i] * pheader->scale[i] + pheader->scale_origin[i];
	}
	frame->radius = RadiusFromBounds(frame->bboxmin, frame->bboxmax);

	pin_intervals = (daliasinterval_t *)(pingroup + 1);
	frame->interval = LittleFloat(pin_intervals->interval);
	pin_intervals += numframes;

	ptemp = (void *)pin_intervals;
	for (i = 0; i < numframes; i++) {
		poseverts[*posenum] = (trivertx_t *)((daliasframe_t *)ptemp + 1);
		(*posenum)++;

		ptemp = (trivertx_t *)((daliasframe_t *)ptemp + 1) + pheader->numverts;
	}

	return ptemp;
}

maliasframedesc_t* R_AliasModelFindFrame(aliashdr_t* hdr, const char* framename, int framenumber)
{
	int f;

	for (f = 0; f < hdr->numframes; ++f) {
		maliasframedesc_t* frame = &hdr->frames[f];

		if (frame->groupnumber == framenumber && !strcmp(frame->groupname, framename)) {
			return frame;
		}
	}

	return NULL;
}

void R_AliasModelColor(const entity_t* ent, float* color, qbool* invalidate_texture)
{
	*invalidate_texture = false;

	if (ent->custom_model == NULL) {
		if (ent->r_modelcolor[0] < 0) {
			// normal color
			VectorSet(color, 1, 1, 1);
		}
		else {
			VectorCopy(ent->r_modelcolor, color);
		}
	}
	else {
		VectorScale(ent->custom_model->color_cvar.color, 1.0f / 255, color);

		*invalidate_texture = (ent->custom_model->fullbright_cvar.integer || ent->custom_model->disable_texturing);
	}
	VectorScale(color, ent->r_modelalpha, color);
	color[3] = ent->r_modelalpha;
}

void R_AliasModelPrepare(entity_t* ent, int framecount, int* frame1_, int* frame2_, float* lerpfrac, qbool* outline)
{
	extern cvar_t r_viewmodelsize, cl_drawgun;
	int frame1 = bound(0, ent->oldframe, framecount - 1);
	int frame2 = bound(0, ent->frame, framecount - 1);
	int expected1 = Mod_ExpectedNextFrame(ent->model, frame1, framecount);
	int expected2 = Mod_ExpectedNextFrame(ent->model, frame2, framecount);

	R_RotateForEntity(ent);
	if ((ent->renderfx & RF_WEAPONMODEL) && r_viewmodelsize.value < 1) {
		// perform scalling for r_viewmodelsize
		R_ScaleModelview(0.5 + bound(0, r_viewmodelsize.value, 1) / 2, 1, 1);
	}

	//
	ent->r_modelalpha = ((ent->renderfx & RF_WEAPONMODEL) && gl_mtexable) ? bound(0, cl_drawgun.value, 1) : 1;
	ent->r_modelalpha = (ent->alpha ? ent->alpha : ent->r_modelalpha);
	ent->r_modelcolor[0] = -1;  // by default no solid fill color for model, using texture

	R_AliasSetupLighting(ent);

	if (!r_lerpframes.value || ent->framelerp < 0 || frame1 == frame2 || (frame2 != expected1 && frame1 != expected2)) {
		*lerpfrac = 1.0f;
	}
	else {
		*lerpfrac = min(ent->framelerp, 1.0f);
	}

	if (frame2 != expected1 && frame1 == expected2) {
		int temp = frame1;
		frame1 = frame2;
		frame2 = temp;
		*lerpfrac = 1.0f - *lerpfrac;
	}

	if (*lerpfrac == 1) {
		*lerpfrac = 0;
		frame1 = frame2;
	}

	*frame1_ = frame1;
	*frame2_ = frame2;

	// We don't support outline for transparent models,
	*outline &= ent->r_modelalpha == 1;
}
