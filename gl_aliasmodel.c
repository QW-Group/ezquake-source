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

/*
==============================================================================
ALIAS MODELS
==============================================================================
*/

static void Mod_FloodFillSkin(byte *skin, int skinwidth, int skinheight);

aliashdr_t	*pheader;

stvert_t	stverts[MAXALIASVERTS];
mtriangle_t	triangles[MAXALIASTRIS];

// a pose is a single set of vertexes.  a frame may be
// an animating sequence of poses
trivertx_t	*poseverts[MAXALIASFRAMES];
int			posenum;

//byte player_8bit_texels[320 * 200];
byte player_8bit_texels[256*256]; // Workaround for new player model, isn't proper for "real" quake skins

#ifndef CLIENTONLY
extern cvar_t     maxclients;
#define IsLocalSinglePlayerGame() (com_serveractive && cls.state == ca_active && !cl.deathmatch && maxclients.value == 1)
#else
#define IsLocalSinglePlayerGame() (0)
#endif

static void* Mod_LoadAliasFrame(void* pin, maliasframedesc_t *frame, int* posenum);
static void* Mod_LoadAliasGroup(void* pin, maliasframedesc_t *frame, int* posenum);
static void* Mod_LoadAllSkins(model_t* loadmodel, int numskins, daliasskintype_t *pskintype);
static void GL_AliasModelShadow(entity_t* ent, aliashdr_t* paliashdr);

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
int       lastposenum;
qbool     full_light;

float     r_shellcolor[3];
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

void GLM_DrawSimpleAliasFrame(model_t* model, aliashdr_t* paliashdr, int pose1, qbool scrolldir, texture_ref texture, texture_ref fb_texture, GLuint textureEnvMode, float scaleS, float scaleT, int effects, qbool shell_only);
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

// precalculated dot products for quantized angles
byte      r_avertexnormal_dots[SHADEDOT_QUANT][NUMVERTEXNORMALS] =
#include "anorm_dots.h"
;
float     r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};
byte      *shadedots = r_avertexnormal_dots[0];

extern vec3_t lightspot;
extern cvar_t gl_meshdraw;

void R_DrawPowerupShell(
	model_t* model, int effects, int layer_no,
	maliasframedesc_t *oldframe, maliasframedesc_t *frame, aliashdr_t *paliashdr
)
{
	float base_level = (layer_no == 0 ? gl_powerupshells_base1level.value : gl_powerupshells_base2level.value);
	float effect_level = (layer_no == 0 ? gl_powerupshells_effect1level.value : gl_powerupshells_effect2level.value);

	base_level = bound(0, base_level, 1);
	effect_level = bound(0, effect_level, 1);

	r_shellcolor[0] = r_shellcolor[1] = r_shellcolor[2] = base_level;

	if (effects & EF_RED) {
		r_shellcolor[0] += effect_level;
	}
	if (effects & EF_GREEN) {
		r_shellcolor[1] += effect_level;
	}
	if (effects & EF_BLUE) {
		r_shellcolor[2] += effect_level;
	}

	R_SetupAliasFrame(model, oldframe, frame, paliashdr, false, layer_no == 1, false, null_texture_reference, null_texture_reference, GL_MODULATE, 1.0f, 1.0f, 0, false, false);
}

static qbool IsFlameModel(model_t* model)
{
	return model->modhint == MOD_FLAME || model->modhint == MOD_FLAME0 || model->modhint == MOD_FLAME3;
}

static void R_RenderAliasModel(
	model_t* model, aliashdr_t *paliashdr, byte *color32bit, int local_skincolormode,
	texture_ref texture, texture_ref fb_texture, maliasframedesc_t* oldframe, maliasframedesc_t* frame,
	qbool outline, float scaleS, float scaleT, int effects, qbool shell_only
)
{
	int i;

	r_modelcolor[0] = -1;  // by default no solid fill color for model, using texture
	if (color32bit) {
		// we may use different methods for filling model surfaces, mixing(modulate), replace, add etc..
		static GLenum modes[] = { GL_MODULATE, GL_REPLACE, GL_BLEND, GL_DECAL, GL_ADD, GL_MODULATE };
		GLenum textureEnvMode = modes[bound(0, local_skincolormode, sizeof(modes) / sizeof(modes[0]) - 1)];

		// particletexture is just solid white texture
		texture = local_skincolormode ? texture : particletexture;

		// force some color for such model
		for (i = 0; i < 3; i++) {
			r_modelcolor[i] = (float)color32bit[i] / 255.0;
			r_modelcolor[i] = bound(0, r_modelcolor[i], 1);
		}

		R_SetupAliasFrame(model, oldframe, frame, paliashdr, false, false, outline, texture, null_texture_reference, textureEnvMode, scaleS, scaleT, effects, false, shell_only);

		r_modelcolor[0] = -1;  // by default no solid fill color for model, using texture
	}
	else if (GL_TextureReferenceIsValid(fb_texture) && gl_mtexable) {
		R_SetupAliasFrame(model, oldframe, frame, paliashdr, true, false, outline, texture, fb_texture, GL_MODULATE, scaleS, scaleT, effects, false, shell_only);

		GL_DisableMultitexture();
	}
	else {
		R_SetupAliasFrame(model, oldframe, frame, paliashdr, false, false, outline, texture, null_texture_reference, GL_MODULATE, scaleS, scaleT, effects, false, shell_only);

		if (GL_TextureReferenceIsValid(fb_texture) && !shell_only) {
			GL_AlphaBlendFlags(GL_BLEND_ENABLED);
			R_SetupAliasFrame(model, oldframe, frame, paliashdr, false, false, false, fb_texture, null_texture_reference, GL_REPLACE, scaleS, scaleT, 0, false, shell_only);
			GL_AlphaBlendFlags(GL_BLEND_DISABLED);
		}
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
void R_DrawAliasModel(entity_t *ent, qbool shell_only)
{
	int anim, skinnum, playernum = -1, local_skincolormode;
	texture_ref texture, fb_texture;
	float scale;
	aliashdr_t *paliashdr;
	model_t *clmodel;
	maliasframedesc_t *oldframe, *frame;
	cvar_t *cv = NULL;
	byte *color32bit = NULL;
	qbool outline = false;
	float oldMatrix[16];
	extern	cvar_t r_viewmodelsize, cl_drawgun;
	qbool is_player_model = (ent->model->modhint == MOD_PLAYER || ent->renderfx & RF_PLAYERMODEL);
	float scaleS = 1.0f;
	float scaleT = 1.0f;

	// VULT NAILTRAIL - Hidenails
	if (amf_hidenails.value && currententity->model->modhint == MOD_SPIKE) {
		return;
	}

	// VULT ROCKETTRAILS - Hide rockets
	if (amf_hiderockets.value && currententity->model->flags & EF_ROCKET) {
		return;
	}

	// VULT CAMERAS - Show/Hide playermodel
	if (currententity->alpha == -1) {
		if (cameratype == C_NORMAL) {
			return;
		}
		currententity->alpha = 1;
	}
	else if (currententity->alpha < 0) {
		// VULT MOTION TRAILS
		return;
	}

	// Handle flame/flame0 model changes
	if (qmb_initialized) {
		if (!amf_part_fire.value && currententity->model->modhint == MOD_FLAME0) {
			currententity->model = cl.model_precache[cl_modelindices[mi_flame]];
		}
		else if (amf_part_fire.value) {
			if (currententity->model->modhint == MOD_FLAME0) {
				if (!ISPAUSED) {
					ParticleFire(currententity->origin);
				}
			}
			else if (currententity->model->modhint == MOD_FLAME && cl_flame0_model) {
				// do we have progs/flame0.mdl?
				if (!ISPAUSED) {
					ParticleFire(currententity->origin);
				}
				currententity->model = cl_flame0_model;
			}
			else if (currententity->model->modhint == MOD_FLAME2 || currententity->model->modhint == MOD_FLAME3) {
				if (!ISPAUSED) {
					ParticleFire(currententity->origin);
				}
				return;
			}
		}
	}

	local_skincolormode = r_skincolormode.integer;

	VectorCopy(ent->origin, r_entorigin);
	VectorSubtract(r_origin, r_entorigin, modelorg);

	//TODO: use modhints here? 
	//VULT CORONAS	
	if (amf_coronas.value && IsFlameModel(ent->model)) {
		//FIXME: This is slow and pathetic as hell, really we should just check the entity
		//alternativley add some kind of permanent client side TE for the torch
		NewStaticLightCorona(C_FIRE, ent->origin, ent);
	}

	if (ent->model->modhint == MOD_TELEPORTDESTINATION && amf_coronas.value) {
		NewStaticLightCorona(C_LIGHTNING, ent->origin, ent);
	}

	clmodel = ent->model;
	paliashdr = (aliashdr_t *)Mod_Extradata(ent->model); // locate the proper data

	if (ent->frame >= paliashdr->numframes || ent->frame < 0) {
		if (ent->model->modhint != MOD_EYES) {
			Com_DPrintf("R_DrawAliasModel: no such frame %d\n", ent->frame);
		}

		ent->frame = 0;
	}
	if (ent->oldframe >= paliashdr->numframes || ent->oldframe < 0) {
		if (ent->model->modhint != MOD_EYES) {
			Com_DPrintf("R_DrawAliasModel: no such oldframe %d\n", ent->oldframe);
		}

		ent->oldframe = 0;
	}

	frame = &paliashdr->frames[ent->frame];
	oldframe = &paliashdr->frames[ent->oldframe];

	r_framelerp = min(ent->framelerp, 1);
	if (!r_lerpframes.value || ent->framelerp < 0 || ent->oldframe == ent->frame) {
		r_framelerp = 1.0;
	}

	if (R_CullAliasModel(ent, oldframe, frame)) {
		return;
	}

	GL_EnableFog();

	//get lighting information
	R_AliasSetupLighting(ent);
	shadedots = r_avertexnormal_dots[((int)(ent->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];

	//draw all the triangles
	c_alias_polys += paliashdr->numtris;

	GL_PushMatrix(GL_MODELVIEW, oldMatrix);
	R_RotateForEntity(ent);

	if (clmodel->modhint == MOD_EYES) {
		GL_Translate(GL_MODELVIEW, paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2] - (22 + 8));
		// double size of eyes, since they are really hard to see in gl
		GL_Scale(GL_MODELVIEW, paliashdr->scale[0] * 2, paliashdr->scale[1] * 2, paliashdr->scale[2] * 2);
	}
	else if (ent->renderfx & RF_WEAPONMODEL) {
		scale = 0.5 + bound(0, r_viewmodelsize.value, 1) / 2;
		GL_Translate(GL_MODELVIEW, paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		GL_Scale(GL_MODELVIEW, paliashdr->scale[0] * scale, paliashdr->scale[1], paliashdr->scale[2]);
	}
	else {
		GL_Translate(GL_MODELVIEW, paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		GL_Scale(GL_MODELVIEW, paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);
	}

	anim = (int)(r_refdef2.time * 10) & 3;
	skinnum = ent->skinnum;
	if (skinnum >= paliashdr->numskins || skinnum < 0) {
		Com_DPrintf("R_DrawAliasModel: no such skin # %d\n", skinnum);
		skinnum = 0;
	}

	texture = paliashdr->gl_texturenum[skinnum][anim];
	fb_texture = paliashdr->fb_texturenum[skinnum][anim];

	r_modelalpha = ((ent->renderfx & RF_WEAPONMODEL) && gl_mtexable) ? bound(0, cl_drawgun.value, 1) : 1;
	//VULT MOTION TRAILS
	if (ent->alpha) {
		r_modelalpha = ent->alpha;
	}

	if (ent->scoreboard) {
		playernum = ent->scoreboard - cl.players;
	}

	// we can't dynamically colormap textures, so they are cached separately for the players.  Heads are just uncolored.
	if (!gl_nocolors.value) {
		if (playernum >= 0 && playernum < MAX_CLIENTS) {
			if (!ent->scoreboard->skin) {
				CL_NewTranslation(playernum);
			}
			texture = playernmtextures[playernum];
			fb_texture = playerfbtextures[playernum];
		}
	}
	if (full_light || !gl_fb_models.value) {
		GL_TextureReferenceInvalidate(fb_texture);
	}

	if (gl_smoothmodels.value) {
		GL_ShadeModel(GL_SMOOTH);
	}

	if (gl_affinemodels.value) {
		GL_Hint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	}

	if (is_player_model && playernum >= 0 && playernum < MAX_CLIENTS) {
		if (cl.teamplay && strcmp(cl.players[playernum].team, TP_SkinForcingTeam()) == 0) {
			cv = &r_teamskincolor;
		}
		else {
			cv = &r_enemyskincolor;
		}

		if (ISDEAD(ent->frame) && r_skincolormodedead.integer != -1) {
			local_skincolormode = r_skincolormodedead.integer;
		}
	}

	if (cv && cv->string[0]) {
		color32bit = cv->color;
	}

	// Check for outline on models.
	// We don't support outline for transparent models,
	// and we also check for ruleset, since we don't want outline on eyes.
	outline = ((gl_outline.integer & 1) && r_modelalpha == 1 && !RuleSets_DisallowModelOutline(clmodel));

	if (shell_only && !GL_ShadersSupported()) {
		GLC_AliasModelPowerupShell(ent, clmodel, oldframe, frame, paliashdr);
	}
	else {
		R_RenderAliasModel(clmodel, paliashdr, color32bit, local_skincolormode, texture, fb_texture, oldframe, frame, outline, scaleS, scaleT, ent->effects, shell_only);

		if (!GL_ShadersSupported()) {
			GLC_UnderwaterCaustics(ent, clmodel, oldframe, frame, paliashdr, scaleS, scaleT);

			if (gl_smoothmodels.value) {
				GL_ShadeModel(GL_FLAT);
			}
		}
	}

	if (gl_affinemodels.value) {
		GL_Hint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	}

	GL_PopMatrix(GL_MODELVIEW, oldMatrix);

	// VULT MOTION TRAILS - No shadows on motion trails
	if ((r_shadows.value && !full_light && !(ent->renderfx & RF_NOSHADOW)) && !ent->alpha) {
		GL_AliasModelShadow(ent, paliashdr);
	}

	// FIXME: Reset state
	glColor3ubv(color_white);
	GL_DisableFog();
	return;
}

void R_SetupAliasFrame(
	model_t* model,
	maliasframedesc_t *oldframe, maliasframedesc_t *frame, aliashdr_t *paliashdr,
	qbool mtex, qbool scrolldir, qbool outline,
	texture_ref texture, texture_ref fb_texture, GLuint textureEnvMode,
	float scaleS, float scaleT,
	int effects, qbool is_texture_array, qbool shell_only
)
{
	extern cvar_t gl_lumaTextures;
	int oldpose, pose, numposes;
	float interval;

	if (!gl_lumaTextures.integer) {
		GL_TextureReferenceInvalidate(fb_texture);
	}

	oldpose = oldframe->firstpose;
	numposes = oldframe->numposes;
	if (numposes > 1) {
		interval = oldframe->interval;
		oldpose += (int)(r_refdef2.time / interval) % numposes;
	}

	pose = frame->firstpose;
	numposes = frame->numposes;
	if (numposes > 1) {
		interval = frame->interval;
		pose += (int)(r_refdef2.time / interval) % numposes;
	}

	if (GL_ShadersSupported()) {
		GLM_DrawSimpleAliasFrame(model, paliashdr, (r_framelerp >= 0.5) ? pose : oldpose, scrolldir, texture, fb_texture, textureEnvMode, scaleS, scaleT, effects, shell_only);
	}
	else {
		GLC_DrawAliasFrame(paliashdr, oldpose, pose, mtex, scrolldir, texture, fb_texture, textureEnvMode, outline);
	}
}

void R_AliasSetupLighting(entity_t *ent)
{
	int minlight, lnum;
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
	ambientlight = shadelight = R_LightPoint(ent->origin);

	/* FIXME: dimman... cache opt from fod */
	//VULT COLOURED MODEL LIGHTS
	if (amf_lighting_colour.value) {
		for (i = 0; i < MAX_DLIGHTS / 32; i++) {
			if (cl_dlight_active[i]) {
				for (j = 0; j < 32; j++) {
					if ((cl_dlight_active[i] & (1 << j)) && i * 32 + j < MAX_DLIGHTS) {
						lnum = i * 32 + j;

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

							for (k = 0;k < 3;k++) {
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
				}
			}
		}
	}
	else {
		for (i = 0; i < MAX_DLIGHTS / 32; i++) {
			if (cl_dlight_active[i]) {
				for (j = 0; j < 32; j++) {
					if ((cl_dlight_active[i] & (1 << j)) && i * 32 + j < MAX_DLIGHTS) {
						lnum = i * 32 + j;

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
	if (amf_lighting_vertex.value) {
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
	else if (
		!((clmodel->modhint == MOD_EYES || clmodel->modhint == MOD_BACKPACK) && strncasecmp(Rulesets_Ruleset(), "default", 7)) &&
		(gl_fb_models.integer == 1 && clmodel->modhint != MOD_GIB && clmodel->modhint != MOD_VMODEL && !IsLocalSinglePlayerGame())
		) {
		ambientlight = shadelight = 4096;
	}

	minlight = cl.minlight;

	if (ambientlight < minlight) {
		ambientlight = shadelight = minlight;
	}
}

void R_DrawViewModel(void)
{
	centity_t *cent;
	static entity_t gun;

	//VULT CAMERA - Don't draw gun in external camera
	if (cameratype != C_NORMAL) {
		return;
	}

	if (!r_drawentities.value || !cl.viewent.current.modelindex) {
		return;
	}

	GL_EnterRegion("R_DrawViewModel");
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

	// hack the depth range to prevent view model from poking into walls
	GL_DepthRange(gldepthmin, gldepthmin + 0.3 * (gldepthmax - gldepthmin));

	switch (currententity->model->type) {
	case mod_alias:
		R_DrawAliasModel(currententity, false);
		break;
	case mod_alias3:
		R_DrawAlias3Model(currententity);
		break;
	default:
		Com_Printf("Not drawing view model of type %i\n", currententity->model->type);
		break;
	}

	GL_DepthRange(gldepthmin, gldepthmax);
	GL_LeaveRegion();
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
	if (GL_ShadersSupported()) {
		GLM_AliasModelShadow(ent, paliashdr, shadevector, lightspot);
	}
	else {
		GLC_AliasModelShadow(ent, paliashdr, shadevector, lightspot);
	}
}

static texture_ref Mod_LoadExternalSkin(model_t* loadmodel, char *identifier, texture_ref *fb_texnum)
{
	char loadpath[64] = {0};
	GLuint texmode;
	texture_ref texnum;
	qbool luma_allowed = Ruleset_IsLumaAllowed(loadmodel);

	GL_TextureReferenceInvalidate(texnum);
	GL_TextureReferenceInvalidate(*fb_texnum);

	if (RuleSets_DisallowExternalTexture(loadmodel)) {
		return texnum;
	}

	texmode = TEX_MIPMAP;
	if (!gl_scaleModelTextures.value) {
		texmode |= TEX_NOSCALE;
	}

	// try "textures/models/..." path
	snprintf (loadpath, sizeof(loadpath), "textures/models/%s", identifier);
	texnum = GL_LoadTextureImage (loadpath, identifier, 0, 0, texmode);
	if (GL_TextureReferenceIsValid(texnum)) {
		if (luma_allowed) {
			// not a luma actually, but which suffix use then? _fb or what?
			snprintf (loadpath, sizeof(loadpath), "textures/models/%s_luma", identifier);
			*fb_texnum = GL_LoadTextureImage(loadpath, va("@fb_%s", identifier), 0, 0, texmode | TEX_FULLBRIGHT | TEX_ALPHA | TEX_LUMA);
		}

		return texnum;
	}

	// try "textures/..." path
	snprintf (loadpath, sizeof(loadpath), "textures/%s", identifier);
	texnum = GL_LoadTextureImage (loadpath, identifier, 0, 0, texmode);
	if (GL_TextureReferenceIsValid(texnum)) {
		// not a luma actually, but which suffix use then? _fb or what?
		if (luma_allowed) {
			snprintf (loadpath, sizeof(loadpath), "textures/%s_luma", identifier);
			*fb_texnum = GL_LoadTextureImage(loadpath, va("@fb_%s", identifier), 0, 0, texmode | TEX_FULLBRIGHT | TEX_ALPHA | TEX_LUMA);
		}

		return texnum;
	}

	return texnum; // we failed miserable
}

static void* Mod_LoadAllSkins(model_t* loadmodel, int numskins, daliasskintype_t* pskintype)
{
	int i, j, k, s, groupskins, texmode;
	texture_ref gl_texnum, fb_texnum;
	char basename[64], identifier[64];
	byte *skin;
	daliasskingroup_t *pinskingroup;
	daliasskininterval_t *pinskinintervals;

	skin = (byte *)(pskintype + 1);

	if (numskins < 1 || numskins > MAX_SKINS) {
		Host_Error("Mod_LoadAllSkins: Invalid # of skins: %d\n", numskins);
	}

	s = pheader->skinwidth * pheader->skinheight;

	COM_StripExtension(COM_SkipPath(loadmodel->name), basename, sizeof(basename));

	texmode = TEX_MIPMAP;
	if (!gl_scaleModelTextures.value && !loadmodel->isworldmodel) {
		texmode |= TEX_NOSCALE;
	}

	for (i = 0; i < numskins; i++) {
		if (pskintype->type == ALIAS_SKIN_SINGLE) {
			Mod_FloodFillSkin(skin, pheader->skinwidth, pheader->skinheight);

			// save 8 bit texels for the player model to remap
			if (loadmodel->modhint == MOD_PLAYER) {
				if (s > sizeof(player_8bit_texels)) {
					Host_Error("Mod_LoadAllSkins: Player skin too large");
				}
				memcpy(player_8bit_texels, (byte *)(pskintype + 1), s);
			}

			snprintf(identifier, sizeof(identifier), "%s_%i", basename, i);

			GL_TextureReferenceInvalidate(gl_texnum);
			GL_TextureReferenceInvalidate(fb_texnum);
			
			gl_texnum = Mod_LoadExternalSkin(loadmodel, identifier, &fb_texnum);
			if (!GL_TextureReferenceIsValid(gl_texnum)) {
				gl_texnum = GL_LoadTexture(identifier, pheader->skinwidth, pheader->skinheight, (byte *)(pskintype + 1), texmode, 1);

				if (Img_HasFullbrights((byte *)(pskintype + 1), pheader->skinwidth * pheader->skinheight)) {
					fb_texnum = GL_LoadTexture(va("@fb_%s", identifier), pheader->skinwidth, pheader->skinheight, (byte *)(pskintype + 1), texmode | TEX_FULLBRIGHT, 1);
				}
			}

			pheader->gl_texturenum[i][0] = pheader->gl_texturenum[i][1] =
				pheader->gl_texturenum[i][2] = pheader->gl_texturenum[i][3] = gl_texnum;

			pheader->fb_texturenum[i][0] = pheader->fb_texturenum[i][1] =
				pheader->fb_texturenum[i][2] = pheader->fb_texturenum[i][3] = fb_texnum;

			pskintype = (daliasskintype_t *)((byte *)(pskintype + 1) + s);
		}
		else {
			// animating skin group.  yuck.
			pskintype++;
			pinskingroup = (daliasskingroup_t *)pskintype;
			groupskins = LittleLong(pinskingroup->numskins);
			pinskinintervals = (daliasskininterval_t *)(pinskingroup + 1);

			pskintype = (void *)(pinskinintervals + groupskins);

			for (j = 0; j < groupskins; j++) {
				Mod_FloodFillSkin(skin, pheader->skinwidth, pheader->skinheight);

				snprintf(identifier, sizeof(identifier), "%s_%i_%i", basename, i, j);

				GL_TextureReferenceInvalidate(gl_texnum);
				GL_TextureReferenceInvalidate(fb_texnum);

				gl_texnum = Mod_LoadExternalSkin(loadmodel, identifier, &fb_texnum);
				if (!GL_TextureReferenceIsValid(gl_texnum)) {
					gl_texnum = GL_LoadTexture(identifier, pheader->skinwidth, pheader->skinheight, (byte *)(pskintype), texmode, 1);

					if (Img_HasFullbrights((byte *)(pskintype), pheader->skinwidth*pheader->skinheight)) {
						fb_texnum = GL_LoadTexture(va("@fb_%s", identifier), pheader->skinwidth, pheader->skinheight, (byte *)(pskintype), texmode | TEX_FULLBRIGHT, 1);
					}
				}

				pheader->gl_texturenum[i][j & 3] = gl_texnum;
				pheader->fb_texturenum[i][j & 3] = fb_texnum;

				pskintype = (daliasskintype_t *)((byte *)(pskintype)+s);
			}

			for (k = j; j < 4; j++) {
				pheader->gl_texturenum[i][j & 3] = pheader->gl_texturenum[i][j - k];
				pheader->fb_texturenum[i][j & 3] = pheader->fb_texturenum[i][j - k];
			}
		}
	}
	return pskintype;
}

typedef struct {
	short x, y;
} floodfill_t;

extern unsigned d_8to24table[];

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
}

//Fill background pixels so mipmapping doesn't have haloes - Ed
static void Mod_FloodFillSkin(byte *skin, int skinwidth, int skinheight)
{
	byte fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t fifo[FLOODFILL_FIFO_SIZE];
	int inpt = 0, outpt = 0, filledcolor = -1, i;

	if (filledcolor == -1) {
		filledcolor = 0;
		// attempt to find opaque black
		for (i = 0; i < 256; ++i) {
			if (d_8to24table[i] == (255 << 0)) { // alpha 1.0
				filledcolor = i;
				break;
			}
		}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255)) {
		//printf( "not filling skin from %d to %d\n", fillcolor, filledcolor );
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt) {
		int x = fifo[outpt].x, y = fifo[outpt].y, fdc = filledcolor;
		byte *pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)				FLOODFILL_STEP(-1, -1, 0);
		if (x < skinwidth - 1)	FLOODFILL_STEP(1, 1, 0);
		if (y > 0)				FLOODFILL_STEP(-skinwidth, 0, -1);
		if (y < skinheight - 1)	FLOODFILL_STEP(skinwidth, 0, 1);
		skin[x + skinwidth * y] = fdc;
	}
}
