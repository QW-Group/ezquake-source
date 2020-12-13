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

// Alias model rendering (modern)
#ifdef RENDERER_OPTION_MODERN_OPENGL

#include "quakedef.h"
#include "gl_model.h"
#include "r_aliasmodel.h"
#include "tr_types.h"
#include "glsl/constants.glsl"
#include "rulesets.h"
#include "r_matrix.h"
#include "glm_vao.h"
#include "r_texture.h"
#include "r_brushmodel.h" // R_PointIsUnderwater only
#include "r_buffers.h"
#include "r_program.h"
#include "r_renderer.h"

void GLM_StateBeginAliasModelBatch(qbool translucent, qbool additive);
void GLM_StateBeginAliasOutlineBatch(void);

#define MAXIMUM_ALIASMODEL_DRAWCALLS MAX_STANDARD_ENTITIES   // ridiculous
#define MAXIMUM_MATERIAL_SAMPLERS 32

typedef enum aliasmodel_draw_type_s {
	aliasmodel_draw_std,
	aliasmodel_draw_alpha,
	aliasmodel_draw_outlines,
	aliasmodel_draw_shells,
	aliasmodel_draw_postscene,
	aliasmodel_draw_postscene_additive,
	aliasmodel_draw_postscene_shells,

	aliasmodel_draw_max
} aliasmodel_draw_type_t;

typedef struct DrawArraysIndirectCommand_s {
	GLuint count;
	GLuint instanceCount;
	GLuint first;
	GLuint baseInstance;
} DrawArraysIndirectCommand_t;

// Internal structure, filled in during initial pass over entities
typedef struct aliasmodel_draw_data_s {
	// Need to split standard & alpha model draws to cope with # of texture units available
	//   Outlines & shells are easier as they have 0 & 1 textures respectively
	DrawArraysIndirectCommand_t indirect_buffer[MAXIMUM_ALIASMODEL_DRAWCALLS];
	texture_ref bound_textures[MAXIMUM_ALIASMODEL_DRAWCALLS][MAXIMUM_MATERIAL_SAMPLERS];
	int num_textures[MAXIMUM_ALIASMODEL_DRAWCALLS];
	int num_cmds[MAXIMUM_ALIASMODEL_DRAWCALLS];
	int num_calls;
	int current_call;

	unsigned int indirect_buffer_offset;
} aliasmodel_draw_instructions_t;

static aliasmodel_draw_instructions_t alias_draw_instructions[aliasmodel_draw_max];
static int alias_draw_count;

typedef struct uniform_block_aliasmodel_s {
	float modelViewMatrix[16];
	// offset: 16 * float
	float color[4];
	int amFlags;
	float yaw_angle_rad;
	float shadelight;
	float ambientlight;
	int materialSamplerMapping;
	float lerpFraction;
	float minLumaMix;
	float outline_normal_scale;
} uniform_block_aliasmodel_t;

typedef struct block_aliasmodels_s {
	uniform_block_aliasmodel_t models[MAX_STANDARD_ENTITIES];
} uniform_block_aliasmodels_t;

extern float r_framelerp;

#define DRAW_DETAIL_TEXTURES   1
#define DRAW_CAUSTIC_TEXTURES  2
#define DRAW_REVERSED_DEPTH    4
#define DRAW_LERP_MUZZLEHACK   8
static uniform_block_aliasmodels_t aliasdata;

static int cached_mode;

static void R_SetAliasModelUniform(int mode)
{
	if (cached_mode != mode) {
		R_ProgramUniform1i(r_program_uniform_aliasmodel_drawmode, mode);
		cached_mode = mode;
	}
}

static int material_samplers_max;
static int TEXTURE_UNIT_MATERIAL;
static int TEXTURE_UNIT_CAUSTICS;

qbool GLM_CompileAliasModelProgram(void)
{
	extern cvar_t r_lerpmuzzlehack;

	unsigned int drawAlias_desiredOptions =
		(r_refdef2.drawCaustics ? DRAW_CAUSTIC_TEXTURES : 0) |
		(glConfig.reversed_depth ? DRAW_REVERSED_DEPTH : 0) |
		(r_lerpmuzzlehack.integer ? DRAW_LERP_MUZZLEHACK : 0);

	if (R_ProgramRecompileNeeded(r_program_aliasmodel, drawAlias_desiredOptions)) {
		static char included_definitions[1024];

		included_definitions[0] = '\0';
		if (drawAlias_desiredOptions & DRAW_CAUSTIC_TEXTURES) {
			material_samplers_max = glConfig.texture_units - 1;
			TEXTURE_UNIT_CAUSTICS = 0;
			TEXTURE_UNIT_MATERIAL = 1;

			strlcat(included_definitions, "#define DRAW_CAUSTIC_TEXTURES\n", sizeof(included_definitions));
			strlcat(included_definitions, "#define SAMPLER_CAUSTIC_TEXTURE 0\n", sizeof(included_definitions));
			strlcat(included_definitions, "#define SAMPLER_MATERIAL_TEXTURE_START 1\n", sizeof(included_definitions));
		}
		else {
			material_samplers_max = glConfig.texture_units;
			TEXTURE_UNIT_MATERIAL = 0;

			strlcat(included_definitions, "#define SAMPLER_MATERIAL_TEXTURE_START 0\n", sizeof(included_definitions));
		}

		strlcat(included_definitions, va("#define SAMPLER_COUNT %d\n", material_samplers_max), sizeof(included_definitions));
		if (drawAlias_desiredOptions & DRAW_REVERSED_DEPTH) {
			strlcat(included_definitions, "#define EZQ_REVERSED_DEPTH\n", sizeof(included_definitions));
		}
		if (drawAlias_desiredOptions & DRAW_LERP_MUZZLEHACK) {
			strlcat(included_definitions, "#define EZQ_ALIASMODEL_MUZZLEHACK\n", sizeof(included_definitions));
		}

		// Initialise program for drawing image
		R_ProgramCompileWithInclude(r_program_aliasmodel, included_definitions);
		R_ProgramSetCustomOptions(r_program_aliasmodel, drawAlias_desiredOptions);
	}
	cached_mode = R_ProgramUniformGet1i(r_program_uniform_aliasmodel_drawmode, 0);

	if (!R_BufferReferenceIsValid(r_buffer_aliasmodel_drawcall_indirect)) {
		buffers.Create(r_buffer_aliasmodel_drawcall_indirect, buffertype_indirect, "aliasmodel-indirect-draw", sizeof(alias_draw_instructions[0].indirect_buffer) * aliasmodel_draw_max, NULL, bufferusage_once_per_frame);
	}

	if (!R_BufferReferenceIsValid(r_buffer_aliasmodel_model_data)) {
		buffers.Create(r_buffer_aliasmodel_model_data, buffertype_storage, "alias-data", sizeof(aliasdata), NULL, bufferusage_once_per_frame);
	}

	return R_ProgramReady(r_program_aliasmodel);
}

void GLM_CreateAliasModelVAO(void)
{
	R_GenVertexArray(vao_aliasmodel);

	GLM_ConfigureVertexAttribPointer(vao_aliasmodel, r_buffer_aliasmodel_vertex_data, 0, 3, GL_FLOAT, GL_FALSE, sizeof(vbo_model_vert_t), VBO_FIELDOFFSET(vbo_model_vert_t, position), 0);
	GLM_ConfigureVertexAttribPointer(vao_aliasmodel, r_buffer_aliasmodel_vertex_data, 1, 2, GL_FLOAT, GL_FALSE, sizeof(vbo_model_vert_t), VBO_FIELDOFFSET(vbo_model_vert_t, texture_coords), 0);
	GLM_ConfigureVertexAttribPointer(vao_aliasmodel, r_buffer_aliasmodel_vertex_data, 2, 3, GL_FLOAT, GL_FALSE, sizeof(vbo_model_vert_t), VBO_FIELDOFFSET(vbo_model_vert_t, normal), 0);
	GLM_ConfigureVertexAttribIPointer(vao_aliasmodel, r_buffer_instance_number, 3, 1, GL_UNSIGNED_INT, sizeof(GLuint), 0, 1);
	GLM_ConfigureVertexAttribPointer(vao_aliasmodel, r_buffer_aliasmodel_vertex_data, 4, 3, GL_FLOAT, GL_FALSE, sizeof(vbo_model_vert_t), VBO_FIELDOFFSET(vbo_model_vert_t, direction), 0);
	GLM_ConfigureVertexAttribIPointer(vao_aliasmodel, r_buffer_aliasmodel_vertex_data, 5, 1, GL_UNSIGNED_INT, sizeof(vbo_model_vert_t), VBO_FIELDOFFSET(vbo_model_vert_t, flags), 0);

	R_BindVertexArray(vao_none);
}

static int AssignSampler(aliasmodel_draw_instructions_t* instr, texture_ref texture)
{
	int i;
	texture_ref* allocated_samplers;

	if (!R_TextureReferenceIsValid(texture)) {
		return 0;
	}
	if (instr->current_call >= sizeof(instr->bound_textures) / sizeof(instr->bound_textures[0])) {
		return -1;
	}

	allocated_samplers = instr->bound_textures[instr->current_call];
	for (i = 0; i < instr->num_textures[instr->current_call]; ++i) {
		if (R_TextureReferenceEqual(texture, allocated_samplers[i])) {
			return i;
		}
	}

	if (instr->num_textures[instr->current_call] < material_samplers_max) {
		allocated_samplers[instr->num_textures[instr->current_call]] = texture;
		return instr->num_textures[instr->current_call]++;
	}

	return -1;
}

static qbool GLM_NextAliasModelDrawCall(aliasmodel_draw_instructions_t* instr, qbool bind_default_textures)
{
	if (instr->num_calls >= sizeof(instr->num_textures) / sizeof(instr->num_textures[0])) {
		return false;
	}

	instr->current_call = instr->num_calls;
	instr->num_calls++;
	instr->num_textures[instr->current_call] = 0;
	if (bind_default_textures) {
		if (R_ProgramCustomOptions(r_program_aliasmodel) & DRAW_CAUSTIC_TEXTURES) {
			instr->bound_textures[instr->current_call][0] = underwatertexture;
			instr->num_textures[instr->current_call]++;
		}
	}
	return true;
}

static void GLM_QueueDrawCall(aliasmodel_draw_type_t type, int vbo_start, int vbo_count, int instance)
{
	int pos;
	aliasmodel_draw_instructions_t* instr = &alias_draw_instructions[type];
	DrawArraysIndirectCommand_t* indirect;

	if (!instr->num_calls) {
		GLM_NextAliasModelDrawCall(instr, false);
	}

	if (instr->num_cmds[instr->current_call] >= sizeof(instr->indirect_buffer) / sizeof(instr->indirect_buffer[0])) {
		return;
	}

	pos = instr->num_cmds[instr->current_call]++;
	indirect = &instr->indirect_buffer[pos];
	indirect->instanceCount = 1;
	indirect->baseInstance = instance;
	indirect->first = vbo_start;
	indirect->count = vbo_count;
}

static void GLM_QueueAliasModelDrawImpl(
	entity_t* ent, model_t* model, float* color, int vbo_start, int vbo_count, texture_ref texture,
	int effects, float lerpFraction, int lerpFrameVertOffset, qbool outline, int render_effects
)
{
	qbool shell = (effects & (EF_RED | EF_BLUE | EF_GREEN)) && R_TextureReferenceIsValid(shelltexture) && Ruleset_AllowPowerupShell(model);
	uniform_block_aliasmodel_t* uniform;
	aliasmodel_draw_type_t type = aliasmodel_draw_std;
	aliasmodel_draw_type_t shelltype = aliasmodel_draw_shells;
	aliasmodel_draw_instructions_t* instr;
	int textureSampler = -1;

	// Compile here so we can work out how many samplers we have free to allocate per draw-call
	if (!GLM_CompileAliasModelProgram()) {
		return;
	}

	if ((render_effects & RF_WEAPONMODEL) && color[3] < 1) {
		type = aliasmodel_draw_postscene;
		shelltype = aliasmodel_draw_postscene_shells;
		outline = false;
	}
	else if (render_effects & RF_ADDITIVEBLEND) {
		type = aliasmodel_draw_postscene_additive;
		shell = false;
		outline = false;
	}
	else if (color[3] < 1) {
		type = aliasmodel_draw_alpha;
		outline = false;
	}

	instr = &alias_draw_instructions[type];

	// Should never happen...
	if (alias_draw_count >= sizeof(aliasdata.models) / sizeof(aliasdata.models[0])) {
		return;
	}

	if (!instr->num_calls) {
		GLM_NextAliasModelDrawCall(instr, true);
	}

	// Assign samplers - if we're over limit, need to flush and try again
	textureSampler = AssignSampler(instr, texture);
	if (textureSampler < 0) {
		if (!GLM_NextAliasModelDrawCall(instr, true)) {
			return;
		}

		textureSampler = AssignSampler(instr, texture);
	}

	// Store static data ready for upload
	memset(&aliasdata.models[alias_draw_count], 0, sizeof(aliasdata.models[alias_draw_count]));
	uniform = &aliasdata.models[alias_draw_count];
	R_GetModelviewMatrix(uniform->modelViewMatrix);
	uniform->amFlags =
		(effects & EF_RED ? AMF_SHELLMODEL_RED : 0) |
		(effects & EF_GREEN ? AMF_SHELLMODEL_GREEN : 0) |
		(effects & EF_BLUE ? AMF_SHELLMODEL_BLUE : 0) |
		(R_TextureReferenceIsValid(texture) ? AMF_TEXTURE_MATERIAL : 0) |
		(render_effects & RF_CAUSTICS ? AMF_CAUSTICS : 0) |
		(render_effects & RF_WEAPONMODEL ? AMF_WEAPONMODEL : 0);
	uniform->yaw_angle_rad = ent->angles[YAW] * M_PI / 180.0;
	uniform->shadelight = ent->shadelight;
	uniform->ambientlight = ent->ambientlight;
	uniform->lerpFraction = lerpFraction;
	uniform->color[0] = color[0];
	uniform->color[1] = color[1];
	uniform->color[2] = color[2];
	uniform->color[3] = color[3];
	uniform->materialSamplerMapping = textureSampler;
	uniform->minLumaMix = 1.0f - (ent->full_light ? bound(0, gl_fb_models.integer, 1) : 0);
	uniform->outline_normal_scale = ent->outlineScale;

	// Add to queues
	GLM_QueueDrawCall(type, vbo_start, vbo_count, alias_draw_count);
	if (outline) {
		GLM_QueueDrawCall(aliasmodel_draw_outlines, vbo_start, vbo_count, alias_draw_count);
	}
	if (shell) {
		GLM_QueueDrawCall(shelltype, vbo_start, vbo_count, alias_draw_count);
	}

	alias_draw_count++;
}

// Called for .mdl & .md3
void GLM_DrawAliasModelFrame(
	entity_t* ent, model_t* model, int poseVertIndex, int poseVertIndex2, int vertsPerPose,
	texture_ref texture, qbool outline, int effects, int render_effects, float lerp_fraction
)
{
	float color[4];
	qbool invalidate_texture;

	if (lerp_fraction == 1) {
		poseVertIndex = poseVertIndex2;
		lerp_fraction = 0;
	}

	// TODO: Vertex lighting etc
	// TODO: Coloured lighting per-vertex?
	R_AliasModelColor(ent, color, &invalidate_texture);

	if (invalidate_texture) {
		R_TextureReferenceInvalidate(texture);
	}

	if (r_refdef2.drawCaustics && R_PointIsUnderwater(ent->origin)) {
		render_effects |= RF_CAUSTICS;
	}

	GLM_QueueAliasModelDrawImpl(
		ent, model, color, poseVertIndex, vertsPerPose,
		texture, effects, lerp_fraction, poseVertIndex2, outline, render_effects
	);
}

// .mdl rendering
void GLM_DrawAliasFrame(
	entity_t* ent, model_t* model, int pose1, int pose2,
	texture_ref texture, texture_ref fb_texture,
	qbool outline, int effects, int render_effects, float lerpfrac
)
{
	aliashdr_t* paliashdr = (aliashdr_t*) Mod_Extradata(model);
	int vertIndex = model->vbo_start + pose1 * paliashdr->vertsPerPose;
	int nextVertIndex = model->vbo_start + pose2 * paliashdr->vertsPerPose;

	GLM_DrawAliasModelFrame(
		ent, model, vertIndex, nextVertIndex, paliashdr->vertsPerPose,
		texture, outline, effects, render_effects, lerpfrac
	);
}

void GLM_PrepareAliasModelBatches(void)
{
	if (!GLM_CompileAliasModelProgram() || !alias_draw_count) {
		return;
	}

	R_TraceEnterNamedRegion(__FUNCTION__);

	// Update VBO with data about each entity
	buffers.Update(r_buffer_aliasmodel_model_data, sizeof(aliasdata.models[0]) * alias_draw_count, aliasdata.models);
	buffers.BindRange(r_buffer_aliasmodel_model_data, EZQ_GL_BINDINGPOINT_ALIASMODEL_DRAWDATA, buffers.BufferOffset(r_buffer_aliasmodel_model_data), sizeof(aliasdata.models[0]) * alias_draw_count);

	// Build & update list of indirect calls
	{
		int i, j;
		int offset = 0;

		for (i = 0; i < aliasmodel_draw_max; ++i) {
			aliasmodel_draw_instructions_t* instr = &alias_draw_instructions[i];
			int total_cmds = 0;
			int size;

			if (!instr->num_calls) {
				continue;
			}

			instr->indirect_buffer_offset = offset;
			for (j = 0; j < instr->num_calls; ++j) {
				total_cmds += instr->num_cmds[j];
			}

			size = sizeof(instr->indirect_buffer[0]) * total_cmds;
			buffers.UpdateSection(r_buffer_aliasmodel_drawcall_indirect, offset, size, instr->indirect_buffer);
			offset += size;
		}
	}

	R_TraceLeaveNamedRegion();
}

static void GLM_RenderPreparedEntities(aliasmodel_draw_type_t type)
{
	aliasmodel_draw_instructions_t* instr = &alias_draw_instructions[type];
	GLint mode = EZQ_ALIAS_MODE_NORMAL;
	unsigned int extra_offset = 0;
	int i;

	if (!instr->num_calls || !GLM_CompileAliasModelProgram()) {
		return;
	}

	GLM_StateBeginAliasModelBatch(type != aliasmodel_draw_std && type != aliasmodel_draw_postscene_additive, type == aliasmodel_draw_postscene_additive);
	buffers.Bind(r_buffer_aliasmodel_drawcall_indirect);
	extra_offset = buffers.BufferOffset(r_buffer_aliasmodel_drawcall_indirect);

	if (type == aliasmodel_draw_shells || type == aliasmodel_draw_postscene_shells) {
		mode = EZQ_ALIAS_MODE_SHELLS;
	}

	R_ProgramUse(r_program_aliasmodel);
	R_SetAliasModelUniform(mode);

	// We have prepared the draw calls earlier in the frame so very trival logic here
	if (r_refdef2.drawCaustics) {
		renderer.TextureUnitBind(TEXTURE_UNIT_CAUSTICS, underwatertexture);
	}
	for (i = 0; i < instr->num_calls; ++i) {
		if (type == aliasmodel_draw_shells || type == aliasmodel_draw_postscene_shells) {
			renderer.TextureUnitBind(TEXTURE_UNIT_MATERIAL, shelltexture);
		}
		else if (instr->num_textures[i]) {
			renderer.TextureUnitMultiBind(TEXTURE_UNIT_MATERIAL, instr->num_textures[i], instr->bound_textures[i]);
		}

		GL_MultiDrawArraysIndirect(
			GL_TRIANGLES,
			(const void*)(uintptr_t)(instr->indirect_buffer_offset + extra_offset),
			instr->num_cmds[i],
			0
		);
	}

	if (type == aliasmodel_draw_std && alias_draw_instructions[aliasmodel_draw_outlines].num_calls) {
		instr = &alias_draw_instructions[aliasmodel_draw_outlines];

		R_TraceEnterNamedRegion("GLM_DrawOutlineBatch");
		R_SetAliasModelUniform(EZQ_ALIAS_MODE_OUTLINES);
		GLM_StateBeginAliasOutlineBatch();

		for (i = 0; i < instr->num_calls; ++i) {
			GL_MultiDrawArraysIndirect(
				GL_TRIANGLES,
				(const void*)(uintptr_t)(instr->indirect_buffer_offset + extra_offset),
				instr->num_cmds[i],
				0
			);
		}

		R_TraceLeaveNamedRegion();
	}
}

void GLM_DrawAliasModelBatches(void)
{
	GLM_RenderPreparedEntities(aliasmodel_draw_std);
	GLM_RenderPreparedEntities(aliasmodel_draw_alpha);
	GLM_RenderPreparedEntities(aliasmodel_draw_shells);
}

void GLM_DrawAliasModelPostSceneBatches(void)
{
	GLM_RenderPreparedEntities(aliasmodel_draw_postscene);
	GLM_RenderPreparedEntities(aliasmodel_draw_postscene_additive);
	GLM_RenderPreparedEntities(aliasmodel_draw_postscene_shells);
}

void GLM_InitialiseAliasModelBatches(void)
{
	alias_draw_count = 0;
	memset(alias_draw_instructions, 0, sizeof(alias_draw_instructions));
}

void GLM_AliasModelShadow(entity_t* ent)
{
	// MEAG: TODO
	// aliashdr_t* paliashdr = (aliashdr_t *)Mod_Extradata(ent->model); // locate the proper data
}

#endif // RENDERER_OPTION_MODERN_OPENGL
