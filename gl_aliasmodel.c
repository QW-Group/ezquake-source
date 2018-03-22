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
#include "gl_local.h"
#include "vx_stuff.h"
#include "vx_vertexlights.h"
#include "utils.h"
#include "qsound.h"
#include "hud.h"
#include "hud_common.h"
#include "gl_bloom.h"
#include "rulesets.h"
#include "teamplay.h"
#include "gl_aliasmodel.h"
#include "crc.h"

void R_SetSkinForPlayerEntity(entity_t* ent, texture_ref* texture, texture_ref* fb_texture, byte** color32bit);

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

static void GL_AliasModelPowerupShell(entity_t* ent, maliasframedesc_t* oldframe, maliasframedesc_t* frame);

aliashdr_t	*pheader;

stvert_t	stverts[MAXALIASVERTS];
mtriangle_t	triangles[MAXALIASTRIS];

// a pose is a single set of vertexes.  a frame may be
// an animating sequence of poses
trivertx_t	*poseverts[MAXALIASFRAMES];
int			posenum;

#ifndef CLIENTONLY
extern cvar_t     maxclients;
#define IsLocalSinglePlayerGame() (com_serveractive && cls.state == ca_active && !cl.deathmatch && maxclients.value == 1)
#else
#define IsLocalSinglePlayerGame() (0)
#endif

static void* Mod_LoadAliasFrame(void* pin, maliasframedesc_t *frame, int* posenum);
static void* Mod_LoadAliasGroup(void* pin, maliasframedesc_t *frame, int* posenum);
static void GL_AliasModelShadow(entity_t* ent, aliashdr_t* paliashdr);
void* Mod_LoadAllSkins(model_t* loadmodel, int numskins, daliasskintype_t *pskintype);

static vec3_t    shadevector;
static vec3_t    vertexlight;
static vec3_t    dlight_color;

static cvar_t    r_lerpmuzzlehack = { "r_lerpmuzzlehack", "1" };
static cvar_t    gl_shaftlight = { "gl_shaftlight", "1" };
cvar_t    gl_powerupshells_effect1level = { "gl_powerupshells_effect1level", "0.75" };
cvar_t    gl_powerupshells_base1level = { "gl_powerupshells_base1level", "0.05" };
cvar_t    gl_powerupshells_effect2level = { "gl_powerupshells_effect2level", "0.4" };
cvar_t    gl_powerupshells_base2level = { "gl_powerupshells_base2level", "0.1" };

float     apitch;
float     ayaw;
float     r_framelerp;
float     r_lerpdistance;
qbool     full_light;

float     r_modelcolor[3];
float     r_modelalpha;
float     shadelight;
float     ambientlight;
custom_model_color_t* custom_model = NULL;

extern vec3_t    lightcolor;
extern float     bubblecolor[NUM_DLIGHTTYPES][4];

extern cvar_t    r_lerpframes;
extern cvar_t    gl_outline;
extern cvar_t    gl_outline_width;

//static void GL_DrawAliasOutlineFrame(aliashdr_t *paliashdr, int pose1, int pose2);
void R_AliasSetupLighting(entity_t *ent);

static custom_model_color_t custom_model_colors[] = {
	// LG beam
	{
		{ "gl_custom_lg_color", "", CVAR_COLOR },
		{ "gl_custom_lg_fullbright", "1" },
		&amf_lightning,
		MOD_THUNDERBOLT
	},
	// Rockets
	{
		{ "gl_custom_rocket_color", "", CVAR_COLOR },
		{ "gl_custom_rocket_fullbright", "1" },
		NULL,
		MOD_ROCKET
	},
	// Grenades
	{
		{ "gl_custom_grenade_color", "", CVAR_COLOR },
		{ "gl_custom_grenade_fullbright", "1" },
		NULL,
		MOD_GRENADE
	},
	// Spikes
	{
		{ "gl_custom_spike_color", "", CVAR_COLOR },
		{ "gl_custom_spike_fullbright", "1" },
		&amf_part_spikes,
		MOD_SPIKE
	}
};

extern vec3_t lightspot;

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

	r_modelcolor[0] = -1;  // by default no solid fill color for model, using texture
	if (color32bit) {
		// force some color for such model
		for (i = 0; i < 3; i++) {
			r_modelcolor[i] = (float)color32bit[i] / 255.0;
			r_modelcolor[i] = bound(0, r_modelcolor[i], 1);
		}

		R_SetupAliasFrame(model, oldframe, frame, false, false, outline, texture, null_texture_reference, effects, ent->renderfx);

		r_modelcolor[0] = -1;  // by default no solid fill color for model, using texture
	}
	else if (GL_TextureReferenceIsValid(fb_texture) && gl_mtexable) {
		R_SetupAliasFrame(model, oldframe, frame, true, false, outline, texture, fb_texture, effects, ent->renderfx);
	}
	else {
		R_SetupAliasFrame(model, oldframe, frame, false, false, outline, texture, null_texture_reference, effects, ent->renderfx);

		if (GL_TextureReferenceIsValid(fb_texture)) {
			GL_AlphaBlendFlags(GL_BLEND_ENABLED);
			R_SetupAliasFrame(model, oldframe, frame, false, false, false, fb_texture, null_texture_reference, 0, ent->renderfx);
			GL_AlphaBlendFlags(GL_BLEND_DISABLED);
		}
	}

	if (GL_UseImmediateMode()) {
		GLC_UnderwaterCaustics(ent, model, oldframe, frame, paliashdr);
	}
}

static qbool R_CullAliasModel(entity_t* ent, maliasframedesc_t* oldframe, maliasframedesc_t* frame)
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
		if (!amf_part_fire.value && ent->model->modhint == MOD_FLAME0) {
			ent->model = cl.model_precache[cl_modelindices[mi_flame]];
		}
		else if (amf_part_fire.value) {
			if (ent->model->modhint == MOD_FLAME0) {
				if (!ISPAUSED) {
					ParticleFire(ent->origin);
				}
			}
			else if (ent->model->modhint == MOD_FLAME && cl_flame0_model) {
				// do we have progs/flame0.mdl?
				if (!ISPAUSED) {
					ParticleFire(ent->origin);
				}
				ent->model = cl_flame0_model;
			}
			else if (ent->model->modhint == MOD_FLAME2 || ent->model->modhint == MOD_FLAME3) {
				if (!ISPAUSED) {
					ParticleFire(ent->origin);
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

	if (ent->scoreboard) {
		playernum = ent->scoreboard - cl.players;
	}

	if (playernum >= 0 && playernum < MAX_CLIENTS) {
		R_SetSkinForPlayerEntity(ent, texture, fb_texture, color32bit);
	}
	// TODO: Can we move the custom_model logic to here?  If fullbright, nullify textures and set color?

	if (full_light || !gl_fb_models.integer) {
		*fb_texture = null_texture_reference;
	}
}

static qbool R_CanDrawModelShadow(entity_t* ent)
{
	return (r_shadows.value && !full_light && !(ent->renderfx & RF_NOSHADOW)) && !ent->alpha;
}

void R_DrawAliasModel(entity_t *ent)
{
	int anim, skinnum;
	texture_ref texture, fb_texture;
	aliashdr_t* paliashdr = (aliashdr_t *)Mod_Extradata(ent->model); // locate the proper data
	maliasframedesc_t *oldframe, *frame;
	byte *color32bit = NULL;
	qbool outline = false;
	float oldMatrix[16];

	if (R_FilterEntity(ent)) {
		return;
	}

	//VULT CORONAS
	if (amf_coronas.value) {
		if (IsFlameModel(ent->model)) {
			//FIXME: This is slow and pathetic as hell, really we should just check the entity
			//alternativley add some kind of permanent client side TE for the torch
			NewStaticLightCorona(C_FIRE, ent->origin, ent);
		}
		if (ent->model->modhint == MOD_TELEPORTDESTINATION) {
			NewStaticLightCorona(C_LIGHTNING, ent->origin, ent);
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

	GL_EnterTracedRegion(va("%s(%s)", __FUNCTION__, ent->model->name), true);
	GL_PushMatrix(GL_MODELVIEW, oldMatrix);
	GL_StateBeginDrawAliasModel(ent, paliashdr);

	//get lighting information
	R_AliasSetupLighting(ent);
	shadedots = r_avertexnormal_dots[((int)(ent->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];
	r_modelalpha = (ent->alpha ? ent->alpha : 1);

	anim = (int)(r_refdef2.time * 10) & 3;
	skinnum = ent->skinnum;
	if (skinnum >= paliashdr->numskins || skinnum < 0) {
		Com_DPrintf("R_DrawAliasModel: no such skin # %d\n", skinnum);
		skinnum = 0;
	}

	texture = paliashdr->gl_texturenum[skinnum][anim];
	fb_texture = paliashdr->fb_texturenum[skinnum][anim];

	R_OverrideModelTextures(ent, &texture, &fb_texture, &color32bit);

	// Check for outline on models.
	// We don't support outline for transparent models,
	// and we also check for ruleset, since we don't want outline on eyes.
	outline = ((gl_outline.integer & 1) && r_modelalpha == 1 && !RuleSets_DisallowModelOutline(ent->model));

	R_RenderAliasModelEntity(ent, paliashdr, color32bit, texture, fb_texture, oldframe, frame, outline, ent->effects);

	GL_PopMatrix(GL_MODELVIEW, oldMatrix);

	// VULT MOTION TRAILS - No shadows on motion trails
	if (R_CanDrawModelShadow(ent)) {
		GL_AliasModelShadow(ent, paliashdr);
	}

	GL_StateEndDrawAliasModel();
	GL_LeaveTracedRegion(true);

	return;
}

int R_AliasFramePose(maliasframedesc_t* frame)
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
	model_t* model,
	maliasframedesc_t *oldframe, maliasframedesc_t *frame,
	qbool mtex, qbool scrolldir, qbool outline,
	texture_ref texture, texture_ref fb_texture,
	int effects, int render_effects
)
{
	extern cvar_t gl_lumaTextures;
	int oldpose, pose;

	if (!gl_lumaTextures.integer) {
		GL_TextureReferenceInvalidate(fb_texture);
	}

	oldpose = R_AliasFramePose(oldframe);
	pose = R_AliasFramePose(frame);

	if (GL_UseGLSL()) {
		GLM_DrawAliasFrame(model, oldpose, pose, texture, fb_texture, outline, effects, render_effects);
	}
	else {
		GLC_DrawAliasFrame(model, oldpose, pose, mtex, scrolldir, texture, fb_texture, outline, effects);
	}
}

void R_AliasSetupLighting(entity_t *ent)
{
	int lnum;
	float add, fbskins;
	unsigned int i;
	unsigned int j;
	unsigned int k;
	vec3_t dist;
	model_t *clmodel;

	//VULT COLOURED MODEL LIGHTING
	float radiusmax = 0;

	clmodel = ent->model;

	custom_model = NULL;
	for (i = 0; i < sizeof(custom_model_colors) / sizeof(custom_model_colors[0]); ++i) {
		custom_model_color_t* test = &custom_model_colors[i];
		if (test->model_hint == clmodel->modhint) {
			if (test->color_cvar.string[0] && (test->amf_cvar == NULL || test->amf_cvar->integer == 0)) {
				custom_model = &custom_model_colors[i];
			}
			break;
		}
	}

	if (custom_model && custom_model->fullbright_cvar.integer) {
		ambientlight = 4096;
		shadelight = 0;
		full_light = true;
		return;
	}

	// make thunderbolt and torches full light
	if (clmodel->modhint == MOD_THUNDERBOLT) {
		ambientlight = 60 + 150 * bound(0, gl_shaftlight.value, 1);
		shadelight = 0;
		full_light = true;
		return;
	}
	else if (clmodel->modhint == MOD_FLAME || clmodel->modhint == MOD_FLAME2) {
		ambientlight = 255;
		shadelight = 0;
		full_light = true;
		return;
	}

	//normal lighting
	full_light = false;

	if (clmodel->modhint == MOD_PLAYER || ent->renderfx & RF_PLAYERMODEL) {
		fbskins = bound(0, r_fullbrightSkins.value, r_refdef2.max_fbskins);
		if (fbskins == 1 && gl_fb_models.value == 1) {
			ambientlight = shadelight = 4096;
			full_light = true;
		}
		else if (fbskins == 0) {
			ambientlight = max(ambientlight, 8);
			shadelight = max(shadelight, 8);
			full_light = false;
		}
		else if (fbskins) {
			ambientlight = max(ambientlight, 8 + fbskins * 120);
			shadelight = max(shadelight, 8 + fbskins * 120);
			full_light = true;
		}
	}
	else if (Rulesets_FullbrightModel(clmodel, IsLocalSinglePlayerGame())) {
		ambientlight = shadelight = 4096;
	}
	else {
		ambientlight = shadelight = R_LightPoint(ent->origin);

		/* FIXME: dimman... cache opt from fod */
		//VULT COLOURED MODEL LIGHTS
		for (i = 0; i < MAX_DLIGHTS / 32; i++) {
			if (cl_dlight_active[i]) {
				for (j = 0; j < 32; j++) {
					if ((cl_dlight_active[i] & (1 << j)) && i * 32 + j < MAX_DLIGHTS) {
						lnum = i * 32 + j;

						if (amf_lighting_colour.integer) {
							VectorSubtract(ent->origin, cl_dlights[lnum].origin, dist);
							add = cl_dlights[lnum].radius - VectorLength(dist);

							if (add > 0) {
								//VULT VERTEX LIGHTING
								if (amf_lighting_vertex.value) {
									if (!radiusmax) {
										radiusmax = cl_dlights[lnum].radius;
										VectorCopy(cl_dlights[lnum].origin, vertexlight);
									}
									else if (cl_dlights[lnum].radius > radiusmax) {
										radiusmax = cl_dlights[lnum].radius;
										VectorCopy(cl_dlights[lnum].origin, vertexlight);
									}
								}

								if (cl_dlights[lnum].type == lt_custom) {
									VectorCopy(cl_dlights[lnum].color, dlight_color);
									VectorScale(dlight_color, (1.0 / 255), dlight_color); // convert color from byte to float
								}
								else {
									VectorCopy(bubblecolor[cl_dlights[lnum].type], dlight_color);
								}

								for (k = 0; k < 3; k++) {
									lightcolor[k] = lightcolor[k] + (dlight_color[k] * add) * 2;
									if (lightcolor[k] > 256) {
										switch (k) {
											case 0:
												lightcolor[1] = lightcolor[1] - (1 * lightcolor[1] / 3);
												lightcolor[2] = lightcolor[2] - (1 * lightcolor[2] / 3);
												break;
											case 1:
												lightcolor[0] = lightcolor[0] - (1 * lightcolor[0] / 3);
												lightcolor[2] = lightcolor[2] - (1 * lightcolor[2] / 3);
												break;
											case 2:
												lightcolor[1] = lightcolor[1] - (1 * lightcolor[1] / 3);
												lightcolor[0] = lightcolor[0] - (1 * lightcolor[0] / 3);
												break;
										}
									}
								}
							}
						}
						else {
							VectorSubtract(ent->origin, cl_dlights[lnum].origin, dist);
							add = cl_dlights[lnum].radius - VectorLength(dist);

							if (add > 0) {
								//VULT VERTEX LIGHTING
								if (amf_lighting_vertex.value) {
									if (!radiusmax) {
										radiusmax = cl_dlights[lnum].radius;
										VectorCopy(cl_dlights[lnum].origin, vertexlight);
									}
									else if (cl_dlights[lnum].radius > radiusmax) {
										radiusmax = cl_dlights[lnum].radius;
										VectorCopy(cl_dlights[lnum].origin, vertexlight);
									}
								}
								ambientlight += add;
							}
						}
					}
				}
			}
		}

		//calculate pitch and yaw for vertex lighting
		if (amf_lighting_vertex.integer) {
			vec3_t dist, ang;
			apitch = currententity->angles[0];
			ayaw = currententity->angles[1];

			if (!radiusmax) {
				vlight_pitch = 45;
				vlight_yaw = 45;
			}
			else {
				VectorSubtract(vertexlight, currententity->origin, dist);
				vectoangles(dist, ang);
				vlight_pitch = ang[0];
				vlight_yaw = ang[1];
			}
		}

		// clamp lighting so it doesn't overbright as much
		if (ambientlight > 128) {
			ambientlight = 128;
		}
		if (ambientlight + shadelight > 192) {
			shadelight = 192 - ambientlight;
		}
	}

	// always give the gun some light
	if ((ent->renderfx & RF_WEAPONMODEL) && ambientlight < 24) {
		ambientlight = shadelight = 24;
	}

	// never allow players to go totally black
	if (clmodel->modhint == MOD_PLAYER || ent->renderfx & RF_PLAYERMODEL) {
		if (ambientlight < 8) {
			ambientlight = shadelight = 8;
		}
	}

	if (ambientlight < cl.minlight) {
		ambientlight = shadelight = cl.minlight;
	}
}

void R_DrawViewModel(void)
{
	extern cvar_t cl_drawgun;
	centity_t *cent;
	static entity_t gun;

	//VULT CAMERA - Don't draw gun in external camera
	if (cameratype != C_NORMAL) {
		return;
	}

	if (!r_drawentities.value || !cl.viewent.current.modelindex || cl_drawgun.value <= 0) {
		return;
	}

	memset(&gun, 0, sizeof(gun));
	cent = &cl.viewent;
	currententity = &gun;

	if (!(gun.model = cl.model_precache[cent->current.modelindex])) {
		Host_Error("R_DrawViewModel: bad modelindex");
	}

	VectorCopy(cent->current.origin, gun.origin);
	VectorCopy(cent->current.angles, gun.angles);
	gun.colormap = vid.colormap;
	gun.renderfx = RF_WEAPONMODEL | RF_NOSHADOW;
	if (r_lerpmuzzlehack.value) {
		if (cent->current.modelindex != cl_modelindices[mi_vaxe] &&
			cent->current.modelindex != cl_modelindices[mi_vbio] &&
			cent->current.modelindex != cl_modelindices[mi_vgrap] &&
			cent->current.modelindex != cl_modelindices[mi_vknife] &&
			cent->current.modelindex != cl_modelindices[mi_vknife2] &&
			cent->current.modelindex != cl_modelindices[mi_vmedi] &&
			cent->current.modelindex != cl_modelindices[mi_vspan]) {
			gun.renderfx |= RF_LIMITLERP;
			r_lerpdistance = 135;
		}
	}

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

	if (gl_mtexable) {
		gun.alpha = bound(0, cl_drawgun.value, 1);
	}

	if (GL_UseImmediateMode()) {
		GL_EnterRegion("R_DrawViewModel");
		GLC_StateBeginDrawViewModel(gun.alpha);
	}

	switch (currententity->model->type) {
	case mod_alias:
		R_DrawAliasModel(currententity);
		if (gun.effects) {
			R_DrawAliasPowerupShell(currententity);
		}
		break;
	case mod_alias3:
		R_DrawAlias3Model(currententity);
		break;
	default:
		Com_Printf("Not drawing view model of type %i\n", currententity->model->type);
		break;
	}

	if (GL_UseImmediateMode()) {
		GLC_StateEndDrawViewModel();

		GL_LeaveRegion();
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

	if (pheader->skinheight > MAX_LBM_HEIGHT)
		Host_Error("Mod_LoadAliasModel: model %s has a skin taller than %d", mod->name, MAX_LBM_HEIGHT);

	pheader->numverts = LittleLong(pinmodel->numverts);

	if (pheader->numverts <= 0)
		Host_Error("Mod_LoadAliasModel: model %s has no vertices", mod->name);

	if (pheader->numverts > MAXALIASVERTS)
		Host_Error("Mod_LoadAliasModel: model %s has too many vertices", mod->name);

	pheader->numtris = LittleLong(pinmodel->numtris);

	if (pheader->numtris <= 0)
		Host_Error("Mod_LoadAliasModel: model %s has no triangles", mod->name);

	pheader->numframes = LittleLong(pinmodel->numframes);
	numframes = pheader->numframes;
	if (numframes < 1)
		Host_Error("Mod_LoadAliasModel: Invalid # of frames: %d\n", numframes);

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
			triangles[i].vertindex[j] = LittleLong(pintriangles[i].vertindex[j]);
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
	GL_MakeAliasModelDisplayLists(mod, pheader);

	// move the complete, relocatable alias model to the cache
	end = Hunk_LowMark();
	total = end - start;

	Cache_Alloc(&mod->cache, total, loadname);
	if (!mod->cache.data)
		return;
	memcpy(mod->cache.data, pheader, total);

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
	int i;
	daliasframe_t *pdaliasframe;

	pdaliasframe = (daliasframe_t *)pin;

	strlcpy(frame->name, pdaliasframe->name, sizeof(frame->name));
	frame->firstpose = *posenum;
	frame->numposes = 1;

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

static void GL_AliasModelShadow(entity_t* ent, aliashdr_t* paliashdr)
{
	if (GL_UseGLSL()) {
		GLM_AliasModelShadow(ent, paliashdr, shadevector, lightspot);
	}
	else {
		GLC_AliasModelShadow(ent, paliashdr, shadevector, lightspot);
	}
}

static void GL_AliasModelPowerupShell(entity_t* ent, maliasframedesc_t* oldframe, maliasframedesc_t* frame)
{
	if (!Ruleset_AllowPowerupShell(ent->model)) {
		return;
	}

	// FIXME: think need put it after caustics
	if ((ent->effects & (EF_RED | EF_GREEN | EF_BLUE)) && GL_TextureReferenceIsValid(shelltexture)) {
		model_t* clmodel = ent->model;

		if (GL_UseGLSL()) {
			GLM_DrawPowerupShell(clmodel, ent->effects, oldframe, frame);
		}
		else {
			GLC_StateBeginAliasPowerupShell();
			GLC_DrawPowerupShell(clmodel, ent->effects, oldframe, frame);
			GLC_StateEndAliasPowerupShell();
		}
	}
}

void R_DrawAliasPowerupShell(entity_t *ent)
{
	aliashdr_t* paliashdr = (aliashdr_t *)Mod_Extradata(ent->model); // locate the proper data
	maliasframedesc_t *oldframe, *frame;
	float oldMatrix[16];

	// FIXME: This is all common with R_DrawAliasModel(), and if passed there, don't need to be run here... 
	if (R_FilterEntity(ent)) {
		return;
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

	GL_EnterTracedRegion(va("%s(%s)", __FUNCTION__, ent->model->name), true);
	GL_PushMatrix(GL_MODELVIEW, oldMatrix);
	GL_StateBeginDrawAliasModel(ent, paliashdr);
	GL_AliasModelPowerupShell(ent, oldframe, frame);
	GL_StateEndDrawAliasModel();
	GL_PopMatrix(GL_MODELVIEW, oldMatrix);
	GL_LeaveTracedRegion(true);
}
