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

// Alias model (.mdl) rendering, classic (immediate mode) GL only
// Most code taken from gl_rmain.c
#ifdef RENDERER_OPTION_CLASSIC_OPENGL

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
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
#include "r_matrix.h"
#include "r_state.h"
#include "r_texture.h"
#include "r_vao.h"
#include "glc_vao.h"
#include "r_brushmodel.h" // R_PointIsUnderwater only
#include "r_buffers.h"
#include "glc_local.h"
#include "glc_matrix.h"
#include "glc_state.h"
#include "r_program.h"
#include "tr_types.h"
#include "r_renderer.h"

static void GLC_DrawAliasModelShadowDrawCall(entity_t* ent, vec3_t shadevector);
static void GLC_DrawCachedAliasOutlineFrame(model_t* model, GLenum primitive, int firstVert, int verts, qbool weaponmodel);

extern float r_avertexnormals[NUMVERTEXNORMALS][3];

extern cvar_t    r_lerpframes;
extern cvar_t    gl_outline;
extern cvar_t    gl_program_aliasmodels;
extern cvar_t    gl_smoothmodels;

extern float     r_framelerp;

// Temporary r_lerpframes fix, unless we go for shaders-in-classic...
//   After all models loaded, this needs to be allocated enough space to hold a complete pose of verts
//   We fill it in with the lerped positions, then render model from there
// Ugly, but don't think there's another way to do this in fixed pipeline
typedef struct glc_aliasmodel_vert_s {
	float position[3];
	float texture_coords[2];
	byte color[4];
} glc_aliasmodel_vert_t;

static glc_aliasmodel_vert_t* temp_aliasmodel_buffer;
static int temp_aliasmodel_buffer_size;

static void GLC_ConfigureAliasModelState(void)
{
	extern cvar_t gl_vbo_clientmemory;

	if (gl_program_aliasmodels.integer) {
		R_GenVertexArray(vao_aliasmodel);
		GLC_VAOSetVertexBuffer(vao_aliasmodel, r_buffer_aliasmodel_vertex_data);
		GLC_VAOEnableVertexPointer(vao_aliasmodel, 3, GL_FLOAT, sizeof(vbo_model_vert_t), VBO_FIELDOFFSET(vbo_model_vert_t, position));
		GLC_VAOEnableNormalPointer(vao_aliasmodel, 3, GL_FLOAT, sizeof(vbo_model_vert_t), VBO_FIELDOFFSET(vbo_model_vert_t, normal));
		GLC_VAOEnableTextureCoordPointer(vao_aliasmodel, 0, 2, GL_FLOAT, sizeof(vbo_model_vert_t), VBO_FIELDOFFSET(vbo_model_vert_t, texture_coords));
		GLC_VAOEnableTextureCoordPointer(vao_aliasmodel, 1, 3, GL_FLOAT, sizeof(vbo_model_vert_t), VBO_FIELDOFFSET(vbo_model_vert_t, direction));
		GLC_VAOEnableCustomAttribute(vao_aliasmodel, 0, r_program_attribute_aliasmodel_std_glc_flags, 1, GL_INT, false, sizeof(vbo_model_vert_t), VBO_FIELDOFFSET(vbo_model_vert_t, flags));
		GLC_VAOEnableCustomAttribute(vao_aliasmodel, 1, r_program_attribute_aliasmodel_shell_glc_flags, 1, GL_INT, false, sizeof(vbo_model_vert_t), VBO_FIELDOFFSET(vbo_model_vert_t, flags));
		GLC_VAOEnableCustomAttribute(vao_aliasmodel, 2, r_program_attribute_aliasmodel_shadow_glc_flags, 1, GL_INT, false, sizeof(vbo_model_vert_t), VBO_FIELDOFFSET(vbo_model_vert_t, flags));
		GLC_VAOEnableCustomAttribute(vao_aliasmodel, 3, r_program_attribute_aliasmodel_outline_glc_flags, 1, GL_INT, false, sizeof(vbo_model_vert_t), VBO_FIELDOFFSET(vbo_model_vert_t, flags));
	}
	else {
		if (gl_vbo_clientmemory.integer) {
			R_GenVertexArray(vao_aliasmodel);
			GLC_VAOEnableVertexPointer(vao_aliasmodel, 3, GL_FLOAT, sizeof(temp_aliasmodel_buffer[0]), &temp_aliasmodel_buffer[0].position[0]);
			GLC_VAOEnableTextureCoordPointer(vao_aliasmodel, 0, 2, GL_FLOAT, sizeof(temp_aliasmodel_buffer[0]), &temp_aliasmodel_buffer[0].texture_coords[0]);
			GLC_VAOEnableTextureCoordPointer(vao_aliasmodel, 1, 2, GL_FLOAT, sizeof(temp_aliasmodel_buffer[0]), &temp_aliasmodel_buffer[0].texture_coords[0]);
			GLC_VAOEnableColorPointer(vao_aliasmodel, 4, GL_UNSIGNED_BYTE, sizeof(temp_aliasmodel_buffer[0]), &temp_aliasmodel_buffer[0].color[0]);
		}
		else {
			R_GenVertexArray(vao_aliasmodel);
			GLC_VAOSetVertexBuffer(vao_aliasmodel, r_buffer_aliasmodel_glc_pose_data);
			GLC_VAOEnableVertexPointer(vao_aliasmodel, 3, GL_FLOAT, sizeof(temp_aliasmodel_buffer[0]), VBO_FIELDOFFSET(glc_aliasmodel_vert_t, position));
			GLC_VAOEnableTextureCoordPointer(vao_aliasmodel, 0, 2, GL_FLOAT, sizeof(temp_aliasmodel_buffer[0]), VBO_FIELDOFFSET(glc_aliasmodel_vert_t, texture_coords));
			GLC_VAOEnableTextureCoordPointer(vao_aliasmodel, 1, 2, GL_FLOAT, sizeof(temp_aliasmodel_buffer[0]), VBO_FIELDOFFSET(glc_aliasmodel_vert_t, texture_coords));
			GLC_VAOEnableColorPointer(vao_aliasmodel, 4, GL_UNSIGNED_BYTE, sizeof(temp_aliasmodel_buffer[0]), VBO_FIELDOFFSET(glc_aliasmodel_vert_t, color));
		}

		R_GenVertexArray(vao_aliasmodel_powerupshell);
		GLC_VAOEnableVertexPointer(vao_aliasmodel_powerupshell, 3, GL_FLOAT, sizeof(temp_aliasmodel_buffer[0]), &temp_aliasmodel_buffer[0].position);
		GLC_VAOEnableTextureCoordPointer(vao_aliasmodel_powerupshell, 0, 2, GL_FLOAT, sizeof(temp_aliasmodel_buffer[0]), &temp_aliasmodel_buffer[0].texture_coords[0]);
	}
}

void GLC_AllocateAliasPoseBuffer(void)
{
	if (!gl_program_aliasmodels.integer) {
		int max_verts = 0;
		int i;

		for (i = 1; i < MAX_MODELS; ++i) {
			model_t* mod = cl.model_precache[i];

			if (mod && mod->type == mod_alias) {
				aliashdr_t* hdr = (aliashdr_t *)Mod_Extradata(mod);

				if (hdr) {
					max_verts = max(max_verts, hdr->vertsPerPose);
				}
			}
		}

		for (i = 0; i < MAX_VWEP_MODELS; i++) {
			model_t* mod = cl.vw_model_precache[i];

			if (mod && mod->type == mod_alias) {
				aliashdr_t* hdr = (aliashdr_t *)Mod_Extradata(mod);

				if (hdr) {
					max_verts = max(max_verts, hdr->vertsPerPose);
				}
			}
		}

		for (i = 0; i < custom_model_count; ++i) {
			model_t* mod = Mod_CustomModel(i, false);

			if (mod && mod->type == mod_alias) {
				aliashdr_t* hdr = (aliashdr_t *)Mod_Extradata(mod);

				if (hdr) {
					max_verts = max(max_verts, hdr->vertsPerPose);
				}
			}
		}

		if (temp_aliasmodel_buffer == NULL || temp_aliasmodel_buffer_size < max_verts) {
			Q_free(temp_aliasmodel_buffer);
			temp_aliasmodel_buffer = Q_malloc(sizeof(temp_aliasmodel_buffer[0]) * max_verts);
			temp_aliasmodel_buffer_size = max_verts;
		}

		if (R_BufferReferenceIsValid(r_buffer_aliasmodel_glc_pose_data)) {
			buffers.Resize(r_buffer_aliasmodel_glc_pose_data, sizeof(temp_aliasmodel_buffer[0]) * max_verts, NULL);
		}
		else {
			buffers.Create(r_buffer_aliasmodel_glc_pose_data, buffertype_vertex, "glc-alias-pose", sizeof(temp_aliasmodel_buffer[0]) * max_verts, NULL, bufferusage_reuse_per_frame);
		}
	}

	GLC_ConfigureAliasModelState();
}

void GLC_FreeAliasPoseBuffer(void)
{
	Q_free(temp_aliasmodel_buffer);
	temp_aliasmodel_buffer_size = 0;
}

static void GLC_AliasModelLightPoint(float color[4], entity_t* ent, vbo_model_vert_t* verts1, vbo_model_vert_t* verts2, float lerpfrac)
{
	float l;

	// VULT VERTEX LIGHTING
	if (amf_lighting_vertex.integer && !ent->full_light) {
		vec3_t lc;

		l = VLight_LerpLight(verts1->lightnormalindex, verts2->lightnormalindex, lerpfrac, ent->angles[0], ent->angles[1]);
		l *= (ent->shadelight + ent->ambientlight) / 256.0;
		l = min(l, 1);

		if (amf_lighting_colour.integer) {
			int i;
			for (i = 0; i < 3; i++) {
				lc[i] = l * ent->lightcolor[i] / 255;
				lc[i] = min(lc[i], 1);
			}
		}
		else {
			VectorSet(lc, l, l, l);
		}

		if (ent->r_modelcolor[0] < 0) {
			// normal color
			VectorCopy(lc, color);
		}
		else {
			color[0] = ent->r_modelcolor[0] * lc[0];
			color[1] = ent->r_modelcolor[1] * lc[1];
			color[2] = ent->r_modelcolor[2] * lc[2];
		}
	}
	else {
		l = FloatInterpolate(shadedots[verts1->lightnormalindex], lerpfrac, shadedots[verts2->lightnormalindex]) / 127.0;
		l = (l * ent->shadelight + ent->ambientlight) / 256.0;
		l = min(l, 1);

		if (ent->custom_model == NULL) {
			if (ent->r_modelcolor[0] < 0) {
				color[0] = color[1] = color[2] = l;
			}
			else {
				color[0] = ent->r_modelcolor[0] * l;
				color[1] = ent->r_modelcolor[1] * l;
				color[2] = ent->r_modelcolor[2] * l;
			}
		}
		else {
			color[0] = ent->custom_model->color_cvar.color[0] / 255.0f;
			color[1] = ent->custom_model->color_cvar.color[1] / 255.0f;
			color[2] = ent->custom_model->color_cvar.color[2] / 255.0f;
		}
	}

	color[0] *= ent->r_modelalpha;
	color[1] *= ent->r_modelalpha;
	color[2] *= ent->r_modelalpha;
	color[3] = ent->r_modelalpha;
}

#define DRAWFLAGS_CAUSTICS     1
#define DRAWFLAGS_TEXTURED     2
#define DRAWFLAGS_FULLBRIGHT   4
#define DRAWFLAGS_MUZZLEHACK   8
//#define DRAWFLAGS_FLATSHADING  16     // Disabled until we can specify GLSL 1.3 dynamically, MESA drivers very strict
#define DRAWFLAGS_MAXIMUM      (DRAWFLAGS_CAUSTICS | DRAWFLAGS_TEXTURED | DRAWFLAGS_FULLBRIGHT | DRAWFLAGS_MUZZLEHACK /* | DRAWFLAGS_FLATSHADING*/)

int GLC_AliasModelSubProgramIndex(qbool textured, qbool fullbright, qbool caustics, qbool muzzlehack)
{
	// gl_smoothmodels disabled for now
	return
		(textured ? DRAWFLAGS_TEXTURED : 0) | 
		(fullbright ? DRAWFLAGS_FULLBRIGHT : 0) | /*(gl_smoothmodels.integer ? 0 : DRAWFLAGS_FLATSHADING)) |*/
		(caustics ? DRAWFLAGS_CAUSTICS : 0) | 
		(muzzlehack ? DRAWFLAGS_MUZZLEHACK : 0);
}

qbool GLC_AliasModelStandardCompileSpecific(int subprogram_index)
{
	int flags = subprogram_index;

	R_ProgramSetSubProgram(r_program_aliasmodel_std_glc, subprogram_index);
	if (R_ProgramRecompileNeeded(r_program_aliasmodel_std_glc, flags)) {
		char included_definitions[512];

		included_definitions[0] = '\0';
		if (subprogram_index & DRAWFLAGS_TEXTURED) {
			strlcat(included_definitions, "#define TEXTURING_ENABLED\n", sizeof(included_definitions));
		}
		if (subprogram_index & DRAWFLAGS_FULLBRIGHT) {
			strlcat(included_definitions, "#define FULLBRIGHT_MODELS\n", sizeof(included_definitions));
		}
		if (subprogram_index & DRAWFLAGS_CAUSTICS) {
			strlcat(included_definitions, "#define DRAW_CAUSTIC_TEXTURES\n", sizeof(included_definitions));
		}
		if (subprogram_index & DRAWFLAGS_MUZZLEHACK) {
			strlcat(included_definitions, "#define EZQ_ALIASMODEL_MUZZLEHACK\n", sizeof(included_definitions));
		}
#ifdef DRAWFLAGS_FLATSHADING
		if (subprogram_index & DRAWFLAGS_FLATSHADING) {
			strlcat(included_definitions, "#define EZQ_ALIASMODEL_FLATSHADING\n", sizeof(included_definitions));
		}
#endif
		R_ProgramCompileWithInclude(r_program_aliasmodel_std_glc, included_definitions);
		R_ProgramUniform1i(r_program_uniform_aliasmodel_std_glc_texSampler, 0);
		R_ProgramUniform1i(r_program_uniform_aliasmodel_std_glc_causticsSampler, 1);
		R_ProgramSetCustomOptions(r_program_aliasmodel_std_glc, flags);
	}

	R_ProgramSetStandardUniforms(r_program_aliasmodel_std_glc);

	return R_ProgramReady(r_program_aliasmodel_std_glc);
}

// Only called from vid system startup, not from rendering loop
qbool GLC_AliasModelStandardCompile(void)
{
	int i;

	for (i = 0; i < DRAWFLAGS_MAXIMUM; ++i) {
		GLC_AliasModelStandardCompileSpecific(i);
	}

	return true;
}

qbool GLC_AliasModelShadowCompile(void)
{
	if (R_ProgramRecompileNeeded(r_program_aliasmodel_shadow_glc, 0)) {
		R_ProgramCompile(r_program_aliasmodel_shadow_glc);
	}

	R_ProgramSetStandardUniforms(r_program_aliasmodel_shadow_glc);

	return R_ProgramReady(r_program_aliasmodel_shadow_glc);
}

qbool GLC_AliasModelShellCompile(void)
{
	extern cvar_t r_lerpmuzzlehack;
	int flags = (r_lerpmuzzlehack.integer ? DRAWFLAGS_MUZZLEHACK : 0);

	if (R_ProgramRecompileNeeded(r_program_aliasmodel_shell_glc, flags)) {
		char included_definitions[512];

		included_definitions[0] = '\0';
		if (flags & DRAWFLAGS_MUZZLEHACK) {
			strlcat(included_definitions, "#define EZQ_ALIASMODEL_MUZZLEHACK\n", sizeof(included_definitions));
		}

		R_ProgramCompileWithInclude(r_program_aliasmodel_shell_glc, included_definitions);
		R_ProgramUniform1i(r_program_uniform_aliasmodel_shell_glc_texSampler, 0);
		R_ProgramSetCustomOptions(r_program_aliasmodel_shell_glc, flags);
	}

	R_ProgramSetStandardUniforms(r_program_aliasmodel_shell_glc);

	return R_ProgramReady(r_program_aliasmodel_shell_glc);
}

qbool GLC_AliasModelOutlineCompile(void)
{
	extern cvar_t r_lerpmuzzlehack;
	int flags = (r_lerpmuzzlehack.integer ? DRAWFLAGS_MUZZLEHACK : 0);

	if (R_ProgramRecompileNeeded(r_program_aliasmodel_outline_glc, flags)) {
		char included_definitions[512];

		strlcpy(included_definitions, "#define BACKFACE_PASS\n", sizeof(included_definitions));
		if (flags & DRAWFLAGS_MUZZLEHACK) {
			strlcat(included_definitions, "#define EZQ_ALIASMODEL_MUZZLEHACK\n", sizeof(included_definitions));
		}

		R_ProgramCompileWithInclude(r_program_aliasmodel_outline_glc, included_definitions);
		R_ProgramSetCustomOptions(r_program_aliasmodel_outline_glc, flags);
	}

	return R_ProgramReady(r_program_aliasmodel_outline_glc);
}

static void GLC_DrawAliasFrameImpl_Program(entity_t* ent, model_t* model, int pose1, int pose2, texture_ref texture, texture_ref fb_texture, qbool outline, int effects, int render_effects, float lerpfrac)
{
	extern cvar_t r_lerpmuzzlehack, gl_program_aliasmodels;
	aliashdr_t* paliashdr = (aliashdr_t*)Mod_Extradata(model);
	float color[4];
	qbool invalidate_texture;
	float angle_radians = -ent->angles[YAW] * M_PI / 180.0;
	vec3_t angle_vector = { cos(angle_radians), sin(angle_radians), 1 };
	int firstVert = model->vbo_start + pose1 * paliashdr->vertsPerPose;
	int subprogram;

	if (outline) {
		if (buffers.supported && GL_Supported(R_SUPPORT_RENDERING_SHADERS) && GLC_AliasModelOutlineCompile()) {
			R_ProgramUse(r_program_aliasmodel_outline_glc);
			R_ProgramUniform1f(r_program_uniform_aliasmodel_outline_glc_lerpFraction, lerpfrac);
			R_ProgramUniform1f(r_program_uniform_aliasmodel_outline_glc_outlineScale, ent->outlineScale);
			GLC_DrawCachedAliasOutlineFrame(model, GL_TRIANGLES, firstVert, paliashdr->vertsPerPose, ent->renderfx & RF_WEAPONMODEL);
		}
	}
	else {
		VectorNormalize(angle_vector);
		R_AliasModelColor(ent, color, &invalidate_texture);

		subprogram = GLC_AliasModelSubProgramIndex(
			!invalidate_texture,
			gl_fb_models.integer && (ent->ambientlight == 4096 && ent->shadelight == 4096),
			r_refdef2.drawCaustics && (render_effects & RF_CAUSTICS),
			r_lerpmuzzlehack.integer && (render_effects & RF_WEAPONMODEL)
		);

		if (buffers.supported && GL_Supported(R_SUPPORT_RENDERING_SHADERS) && GLC_AliasModelStandardCompileSpecific(subprogram)) {
			R_ProgramUse(r_program_aliasmodel_std_glc);
			R_ProgramUniform3fv(r_program_uniform_aliasmodel_std_glc_angleVector, angle_vector);
			R_ProgramUniform1f(r_program_uniform_aliasmodel_std_glc_shadelight, ent->shadelight / 256.0f);
			R_ProgramUniform1f(r_program_uniform_aliasmodel_std_glc_ambientlight, ent->ambientlight / 256.0f);
			R_ProgramUniform1i(r_program_uniform_aliasmodel_std_glc_fsTextureEnabled, invalidate_texture ? 0 : 1);
			R_ProgramUniform1f(r_program_uniform_aliasmodel_std_glc_fsMinLumaMix, 1.0f - (ent->full_light ? bound(0, gl_fb_models.integer, 1) : 0));
			R_ProgramUniform1f(r_program_uniform_aliasmodel_std_glc_fsCausticEffects, render_effects & RF_CAUSTICS ? 1 : 0);
			R_ProgramUniform1f(r_program_uniform_aliasmodel_std_glc_lerpFraction, lerpfrac);
			R_ProgramUniform1f(r_program_uniform_aliasmodel_std_glc_time, cl.time);

			if (ent->r_modelalpha < 1) {
				GLC_StateBeginDrawAliasZPass(render_effects & RF_WEAPONMODEL);
				GL_DrawArrays(GL_TRIANGLES, firstVert, paliashdr->vertsPerPose);
			}
			GLC_StateBeginDrawAliasFrameProgram(texture, underwatertexture, render_effects, ent->custom_model, ent->r_modelalpha, false);
			R_CustomColor(color[0], color[1], color[2], color[3]);
			GL_DrawArrays(GL_TRIANGLES, firstVert, paliashdr->vertsPerPose);
			if (render_effects & RF_CAUSTICS) {
				GLC_StateEndUnderwaterAliasModelCaustics();
			}
			R_ProgramUse(r_program_none);
		}
	}
}

static void GLC_DrawAliasFrameImpl_Immediate_Cache(aliashdr_t* paliashdr, entity_t* ent, vbo_model_vert_t* verts1, vbo_model_vert_t* verts2, float lerpfracDefault, qbool limit_lerp, qbool outline, qbool mtex)
{
	int i;
	vec3_t interpolated_verts;

	for (i = 0; i < paliashdr->vertsPerPose; ++i, ++verts1, ++verts2) {
		float color[4];
		float s = verts1->texture_coords[0];
		float t = verts1->texture_coords[1];
		float lerpfrac = lerpfracDefault;

		if (limit_lerp && !VectorL2Compare(verts1->position, verts2->position, ALIASMODEL_MAX_LERP_DISTANCE)) {
			lerpfrac = 1;
		}

		GLC_AliasModelLightPoint(color, ent, verts1, verts2, lerpfrac);
		if (outline) {
			vec3_t v1, v2;
			VectorMA(verts1->position, ent->outlineScale, verts1->normal, v1);
			VectorMA(verts2->position, ent->outlineScale, verts2->normal, v2);
			VectorInterpolate(v1, lerpfrac, v2, interpolated_verts);
		}
		else {
			VectorInterpolate(verts1->position, lerpfrac, verts2->position, interpolated_verts);
		}
		temp_aliasmodel_buffer[i].texture_coords[0] = s;
		temp_aliasmodel_buffer[i].texture_coords[1] = t;
		temp_aliasmodel_buffer[i].color[0] = color[0] * 255;
		temp_aliasmodel_buffer[i].color[1] = color[1] * 255;
		temp_aliasmodel_buffer[i].color[2] = color[2] * 255;
		temp_aliasmodel_buffer[i].color[3] = color[3] * 255;
		VectorCopy(interpolated_verts, temp_aliasmodel_buffer[i].position);
	}
}

static void GLC_DrawAliasFrameImpl_Immediate_TrueImmediate(aliashdr_t* paliashdr, qbool mtex)
{
	int i;

	GLC_Begin(GL_TRIANGLES);
	for (i = 0; i < paliashdr->vertsPerPose; ++i) {
		// texture coordinates come from the draw list
		float s = temp_aliasmodel_buffer[i].texture_coords[0];
		float t = temp_aliasmodel_buffer[i].texture_coords[1];
		byte* color = temp_aliasmodel_buffer[i].color;

		if (mtex) {
			GLC_MultiTexCoord2f(GL_TEXTURE0, s, t);
			GLC_MultiTexCoord2f(GL_TEXTURE1, s, t);
		}
		else {
			glTexCoord2f(s, t);
		}
		R_CustomColor4ubv(color);
		GLC_Vertex3fv(temp_aliasmodel_buffer[i].position);
	}
	GLC_End();
}

static void GLC_DrawAliasFrameImpl_Immediate(entity_t* ent, model_t* model, int pose1, int pose2, texture_ref texture, texture_ref fb_texture, qbool outline, int effects, int render_effects, float lerpfracDefault)
{
	extern cvar_t r_lerpmuzzlehack;

	aliashdr_t* paliashdr = (aliashdr_t*)Mod_Extradata(model);
	qbool cache = buffers.supported && temp_aliasmodel_buffer_size >= paliashdr->poseverts;
	qbool mtex = R_TextureReferenceIsValid(fb_texture) && gl_mtexable;
	vbo_model_vert_t* vbo_buffer = (vbo_model_vert_t*)model->temp_vbo_buffer;
	vbo_model_vert_t* verts1, * verts2;
	qbool limit_lerp = r_lerpmuzzlehack.integer && (ent->model->renderfx & RF_LIMITLERP);
	qbool alpha_blend = (render_effects & RF_ALPHABLEND) || ent->r_modelalpha < 1;
	qbool is_weapon_model = (ent->renderfx & RF_WEAPONMODEL);

	verts1 = &vbo_buffer[pose1 * paliashdr->poseverts];
	verts2 = &vbo_buffer[pose2 * paliashdr->poseverts];

	GLC_DrawAliasFrameImpl_Immediate_Cache(paliashdr, ent, verts1, verts2, lerpfracDefault, limit_lerp, outline, mtex);
	if (cache) {
		buffers.Update(r_buffer_aliasmodel_glc_pose_data, sizeof(temp_aliasmodel_buffer[0]) * paliashdr->vertsPerPose, temp_aliasmodel_buffer);
	}

	R_ProgramUse(r_program_none);
	if (alpha_blend && !outline) {
		GLC_StateBeginDrawAliasZPass(is_weapon_model);
		if (cache) {
			GL_DrawArrays(GL_TRIANGLES, 0, paliashdr->vertsPerPose);
		}
		else {
			GLC_DrawAliasFrameImpl_Immediate_TrueImmediate(paliashdr, mtex);
		}
	}

	if (outline) {
		GLC_StateBeginAliasOutlineFrame(is_weapon_model);
	}
	else if (render_effects & RF_CAUSTICS) {
		GLC_StateBeginUnderwaterAliasModelCaustics(texture, fb_texture);
	}
	else {
		GLC_StateBeginDrawAliasFrame(texture, fb_texture, mtex, alpha_blend, ent->custom_model, is_weapon_model);
	}

	if (cache) {
		GL_DrawArrays(GL_TRIANGLES, 0, paliashdr->vertsPerPose);
	}
	else {
		GLC_DrawAliasFrameImpl_Immediate_TrueImmediate(paliashdr, mtex);
	}

	if (render_effects & RF_CAUSTICS) {
		GLC_StateEndUnderwaterAliasModelCaustics();
	}
}

void GLC_DrawAliasFrame(entity_t* ent, model_t* model, int pose1, int pose2, texture_ref texture, texture_ref fb_texture, qbool outline, int effects, int render_effects, float lerpfrac)
{
	qbool draw_caustics = r_refdef2.drawCaustics && gl_mtexable && R_PointIsUnderwater(ent->origin);

	if (gl_program_aliasmodels.integer) {
		GLC_DrawAliasFrameImpl_Program(ent, model, pose1, pose2, texture, fb_texture, outline, effects, render_effects | (draw_caustics ? RF_CAUSTICS : 0), lerpfrac);
	}
	else if (outline) {
		GLC_DrawAliasFrameImpl_Immediate(ent, model, pose1, pose2, texture, fb_texture, outline, effects, render_effects, lerpfrac);
	}
	else {
		if (R_TextureReferenceIsValid(fb_texture) && gl_mtexable) {
			GLC_DrawAliasFrameImpl_Immediate(ent, model, pose1, pose2, texture, fb_texture, outline, effects, render_effects, lerpfrac);
		}
		else {
			GLC_DrawAliasFrameImpl_Immediate(ent, model, pose1, pose2, texture, null_texture_reference, outline, effects, render_effects, lerpfrac);

			if (R_TextureReferenceIsValid(fb_texture)) {
				GLC_DrawAliasFrameImpl_Immediate(ent, model, pose1, pose2, fb_texture, null_texture_reference, false, effects, render_effects | RF_ALPHABLEND, lerpfrac);
			}
		}

		if (draw_caustics) {
			GLC_DrawAliasFrameImpl_Immediate(ent, model, pose1, pose2, texture, underwatertexture, false, 0, RF_ALPHABLEND | RF_CAUSTICS, lerpfrac);
		}
	}
}

// This can be used with program or immediate mode
static void GLC_DrawCachedAliasOutlineFrame(model_t* model, GLenum primitive, int firstVert, int verts, qbool weaponmodel)
{
	GLC_StateBeginAliasOutlineFrame(weaponmodel);

	GL_DrawArrays(primitive, firstVert, verts);
}

void GLC_PowerupShellColor(int layer_no, int effects, float* color)
{
	// set color: alpha so we can see colour underneath still
	float base_level = bound(0, (layer_no == 0 ? gl_powerupshells_base1level.value : gl_powerupshells_base2level.value), 1);
	float effect_level = bound(0, (layer_no == 0 ? gl_powerupshells_effect1level.value : gl_powerupshells_effect2level.value), 1);
	float shell_alpha = bound(0, gl_powerupshells.value, 1);

	color[0] = (base_level + ((effects & EF_RED) ? effect_level : 0)) * shell_alpha;
	color[1] = (base_level + ((effects & EF_GREEN) ? effect_level : 0)) * shell_alpha;
	color[2] = (base_level + ((effects & EF_BLUE) ? effect_level : 0)) * shell_alpha;
	color[3] = shell_alpha;
}

void GLC_SetPowerupShellColor(int layer_no, int effects)
{
	float color[4];

	GLC_PowerupShellColor(layer_no, effects, color);

	R_CustomColor(color[0], color[1], color[2], color[3]);
}

const float* GLC_PowerupShell_ScrollParams(void)
{
	return r_refdef2.powerup_scroll_params;
}

static void GLC_DrawPowerupShell_Program(entity_t* ent, int pose1, float fraclerp)
{
	if (buffers.supported && GL_Supported(R_SUPPORT_RENDERING_SHADERS) && GLC_AliasModelShellCompile()) {
		aliashdr_t* paliashdr = (aliashdr_t*)Mod_Extradata(ent->model);
		int firstVert = ent->model->vbo_start + pose1 * paliashdr->vertsPerPose;
		float color1[4], color2[4];

		GLC_PowerupShellColor(0, ent->effects, color1);
		GLC_PowerupShellColor(1, ent->effects, color2);

		R_ProgramUse(r_program_aliasmodel_shell_glc);
		GLC_StateBeginAliasPowerupShell(ent->renderfx & RF_WEAPONMODEL);
		GLC_BindVertexArrayAttributes(vao_aliasmodel);
		R_ProgramUniform4fv(r_program_uniform_aliasmodel_shell_glc_fsBaseColor1, color1);
		R_ProgramUniform4fv(r_program_uniform_aliasmodel_shell_glc_fsBaseColor2, color2);
		R_ProgramUniform4fv(r_program_uniform_aliasmodel_shell_glc_scroll, GLC_PowerupShell_ScrollParams());
		R_ProgramUniform1f(r_program_uniform_aliasmodel_shell_glc_lerpFraction, fraclerp);
		GL_DrawArrays(GL_TRIANGLES, firstVert, paliashdr->vertsPerPose);
		R_ProgramUse(r_program_none);
	}
}

static void GLC_DrawPowerupShell_Immediate(entity_t* ent, int pose1, int pose2, float fraclerp)
{
	ez_trivertx_t* verts1;
	ez_trivertx_t* verts2;
	int layer_no;
	int *order, count;
	vec3_t v1, v2, v;
	aliashdr_t* paliashdr = (aliashdr_t*)Mod_Extradata(ent->model);

	qbool cache = buffers.supported && temp_aliasmodel_buffer_size >= paliashdr->poseverts;
	int position;
	const float* scroll = GLC_PowerupShell_ScrollParams();

	R_ProgramUse(r_program_none);
	GLC_StateBeginAliasPowerupShell(ent->renderfx & RF_WEAPONMODEL);
	for (position = 0, layer_no = 0; layer_no <= 1; ++layer_no) {
		// get the vertex count and primitive type
		order = (int *)((byte *)paliashdr + paliashdr->commands);
		verts1 = verts2 = (ez_trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
		verts1 += pose1 * paliashdr->poseverts;
		verts2 += pose2 * paliashdr->poseverts;

		for (;;) {
			GLenum drawMode = GL_TRIANGLE_STRIP;

			count = *order++;
			if (!count) {
				break;
			}

			if (count < 0) {
				count = -count;
				drawMode = GL_TRIANGLE_FAN;
			}

			if (!cache) {
				GLC_SetPowerupShellColor(layer_no, ent->effects);
				GLC_Begin(drawMode);
			}

			do {
				float s = ((float *)order)[0] * 2.0f + scroll[layer_no * 2];
				float t = ((float *)order)[1] * 2.0f + scroll[layer_no * 2 + 1];

				order += 2;

				VectorMA(verts1->v, 0.5f, r_avertexnormals[verts1->lightnormalindex], v1);
				VectorMA(verts2->v, 0.5f, r_avertexnormals[verts2->lightnormalindex], v2);
				VectorInterpolate(v1, fraclerp, v2, v);

				if (cache) {
					temp_aliasmodel_buffer[position].texture_coords[0] = s;
					temp_aliasmodel_buffer[position].texture_coords[1] = t;
					VectorCopy(v, temp_aliasmodel_buffer[position].position);
					position++;
				}
				else {
					glTexCoord2f(s, t);
					GLC_Vertex3f(v[0], v[1], v[2]);
				}

				verts1++;
				verts2++;
			} while (--count);

			if (cache) {
				int i;

				R_BindVertexArray(vao_aliasmodel_powerupshell);
				GLC_SetPowerupShellColor(0, ent->effects);
				GL_DrawArrays(drawMode, 0, position);

				// And quickly update texture-coordinates and run again...
				GLC_SetPowerupShellColor(1, ent->effects);
				for (i = 0; i < position; ++i) {
					temp_aliasmodel_buffer[i].texture_coords[0] += scroll[2] - scroll[0];
					temp_aliasmodel_buffer[i].texture_coords[1] += scroll[3] - scroll[1];
				}
				GL_DrawArrays(drawMode, 0, position);

				return;
			}
			else {
				GLC_End();
			}
		}
	}
}

static void GLC_DrawPowerupShell(entity_t* ent, maliasframedesc_t *oldframe, maliasframedesc_t *frame)
{
	int pose1, pose2;
	float lerp;

	R_AliasModelDeterminePoses(oldframe, frame, &pose1, &pose2, &lerp);

	if (gl_program_aliasmodels.integer) {
		GLC_DrawPowerupShell_Program(ent, pose1, lerp);
	}
	else {
		GLC_DrawPowerupShell_Immediate(ent, pose1, pose2, lerp);
	}
}

void GLC_DrawAliasModelShadow(entity_t* ent)
{
	float theta;
	float oldMatrix[16];
	static float shadescale = 0;
	vec3_t shadevector;

	if (!shadescale) {
		shadescale = 1 / sqrt(2);
	}
	theta = -ent->angles[1] / 180 * M_PI;

	VectorSet(shadevector, cos(theta) * shadescale, sin(theta) * shadescale, shadescale);

	R_PushModelviewMatrix(oldMatrix);
	R_TranslateModelview(ent->origin[0], ent->origin[1], ent->origin[2]);
	R_RotateModelview(ent->angles[1], 0, 0, 1);

	GLC_StateBeginAliasModelShadow();
	GLC_DrawAliasModelShadowDrawCall(ent, shadevector);
	R_PopModelviewMatrix(oldMatrix);
}

static void GLC_DrawAliasModelShadowDrawCall_Program(entity_t* ent, int pose1, float lerpfrac, float lheight, vec3_t shadevector)
{
	if (buffers.supported && GL_Supported(R_SUPPORT_RENDERING_SHADERS) && GLC_AliasModelShadowCompile()) {
		const aliashdr_t* paliashdr = (aliashdr_t *)Mod_Extradata(ent->model); // locate the proper data
		int firstVert = ent->model->vbo_start + pose1 * paliashdr->vertsPerPose;

		R_ProgramUse(r_program_aliasmodel_shadow_glc);
		GLC_BindVertexArrayAttributes(vao_aliasmodel);
		R_ProgramUniform1f(r_program_uniform_aliasmodel_shadow_glc_lerpFraction, lerpfrac);
		R_ProgramUniform1f(r_program_uniform_aliasmodel_shadow_glc_lheight, lheight);
		R_ProgramUniform2fv(r_program_uniform_aliasmodel_shadow_glc_shadevector, shadevector);
		GL_DrawArrays(GL_TRIANGLES, firstVert, paliashdr->vertsPerPose);
		R_ProgramUse(r_program_none);
	}
}

static void GLC_DrawAliasModelShadowDrawCall_Immediate(entity_t* ent, int pose1, float lerpfrac, float lheight, vec3_t shadevector)
{
	const aliashdr_t* paliashdr = (aliashdr_t *)Mod_Extradata(ent->model); // locate the proper data
	int *order, count;
	vec3_t point;
	ez_trivertx_t *verts;
	float height = (1 - lheight);

	verts = (ez_trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += pose1 * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);

	R_ProgramUse(r_program_none);
	while ((count = *order++)) {
		// get the vertex count and primitive type
		if (count < 0) {
			count = -count;
			GLC_Begin(GL_TRIANGLE_FAN);
		}
		else {
			GLC_Begin(GL_TRIANGLE_STRIP);
		}

		do {
			//no texture for shadows
			order += 2;

			// normals and vertexes come from the frame list
			point[0] = verts->v[0] + paliashdr->scale_origin[0];
			point[1] = verts->v[1] + paliashdr->scale_origin[1];
			point[2] = verts->v[2] + paliashdr->scale_origin[2];

			point[0] -= shadevector[0] * (point[2] + lheight);
			point[1] -= shadevector[1] * (point[2] + lheight);
			point[2] = height;

			GLC_Vertex3fv(point);

			verts++;
		} while (--count);

		GLC_End();
	}
}

static void GLC_DrawAliasModelShadowDrawCall(entity_t* ent, vec3_t shadevector)
{
	const aliashdr_t* paliashdr = (aliashdr_t *)Mod_Extradata(ent->model); // locate the proper data
	const maliasframedesc_t* frame = &paliashdr->frames[ent->frame];
	const maliasframedesc_t* oldframe = &paliashdr->frames[ent->oldframe];
	float lheight = ent->origin[2] - ent->lightspot[2];
	int pose1, pose2;
	float lerpfrac;

	R_AliasModelDeterminePoses(oldframe, frame, &pose1, &pose2, &lerpfrac);

	if (gl_program_aliasmodels.integer) {
		GLC_DrawAliasModelShadowDrawCall_Program(ent, pose1, lerpfrac, lheight, shadevector);
	}
	else {
		GLC_DrawAliasModelShadowDrawCall_Immediate(ent, pose1, lerpfrac, lheight, shadevector);
	}
}

void GLC_DrawAliasModelPowerupShell(entity_t *ent)
{
	aliashdr_t* paliashdr = (aliashdr_t *)Mod_Extradata(ent->model); // locate the proper data
	maliasframedesc_t *oldframe, *frame;
	float oldMatrix[16];

	if (!(ent->effects & (EF_RED | EF_GREEN | EF_BLUE)) || !R_TextureReferenceIsValid(shelltexture)) {
		return;
	}

	// FIXME: This is all common with R_DrawAliasModel(), and if passed there, don't need to be run here...
	if (!Ruleset_AllowPowerupShell(ent->model) || R_FilterEntity(ent)) {
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

	R_TraceEnterRegion(va("%s(%s)", __func__, ent->model->name), true);

	// FIXME: think need put it after caustics
	R_PushModelviewMatrix(oldMatrix);
	R_StateBeginDrawAliasModel(ent, paliashdr);
	GLC_DrawPowerupShell(ent, oldframe, frame);
	R_PopModelviewMatrix(oldMatrix);

	R_TraceLeaveRegion(true);
}

#endif // #ifdef RENDERER_OPTION_CLASSIC_OPENGL
