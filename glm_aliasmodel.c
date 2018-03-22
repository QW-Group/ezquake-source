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

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "gl_aliasmodel.h"
#include "tr_types.h"
#include "glsl/constants.glsl"
#include "rulesets.h"
#include "r_matrix.h"
#include "glm_vao.h"
#include "r_texture.h"

#define MAXIMUM_ALIASMODEL_DRAWCALLS MAX_STANDARD_ENTITIES   // ridiculous
#define MAXIMUM_MATERIAL_SAMPLERS 32

typedef enum aliasmodel_draw_type_s {
	aliasmodel_draw_std,
	aliasmodel_draw_alpha,
	aliasmodel_draw_outlines,
	aliasmodel_draw_shells,
	aliasmodel_draw_postscene,
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

static buffer_ref aliasModel_ssbo;

extern float r_framelerp;

#define DRAW_DETAIL_TEXTURES 1
#define DRAW_CAUSTIC_TEXTURES 2
#define DRAW_LUMA_TEXTURES 4
static glm_program_t drawAliasModelProgram;
static GLint drawAliasModel_mode;
static uniform_block_aliasmodels_t aliasdata;
static buffer_ref vbo_aliasDataBuffer;
static buffer_ref vbo_aliasIndirectDraw;

static int cached_mode;

static void SetAliasModelMode(int mode)
{
	if (cached_mode != mode) {
		GL_Uniform1i(drawAliasModel_mode, mode);
		cached_mode = mode;
	}
}

static int material_samplers_max;
static int TEXTURE_UNIT_MATERIAL;
static int TEXTURE_UNIT_CAUSTICS;

static qbool GLM_CompileAliasModelProgram(void)
{
	qbool caustic_textures = gl_caustics.integer && GL_TextureReferenceIsValid(underwatertexture);
	unsigned int drawAlias_desiredOptions = (caustic_textures ? DRAW_CAUSTIC_TEXTURES : 0);

	if (GLM_ProgramRecompileNeeded(&drawAliasModelProgram, drawAlias_desiredOptions)) {
		static char included_definitions[1024];
		GL_VFDeclare(draw_aliasmodel);

		included_definitions[0] = '\0';
		if (caustic_textures) {
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

		// Initialise program for drawing image
		GLM_CreateVFProgramWithInclude("AliasModel", GL_VFParams(draw_aliasmodel), &drawAliasModelProgram, included_definitions);

		drawAliasModelProgram.custom_options = drawAlias_desiredOptions;
	}

	if (drawAliasModelProgram.program && !drawAliasModelProgram.uniforms_found) {
		drawAliasModel_mode = GL_UniformGetLocation(drawAliasModelProgram.program, "mode");
		cached_mode = 0;

		drawAliasModelProgram.uniforms_found = true;
	}

	if (!GL_BufferReferenceIsValid(vbo_aliasIndirectDraw)) {
		vbo_aliasIndirectDraw = GL_CreateFixedBuffer(buffertype_indirect, "aliasmodel-indirect-draw", sizeof(alias_draw_instructions[0].indirect_buffer) * aliasmodel_draw_max, NULL, bufferusage_once_per_frame);
	}

	if (!GL_BufferReferenceIsValid(vbo_aliasDataBuffer)) {
		vbo_aliasDataBuffer = GL_CreateFixedBuffer(buffertype_storage, "alias-data", sizeof(aliasdata), NULL, bufferusage_once_per_frame);
	}

	return drawAliasModelProgram.program;
}

static void GL_CreateAliasModelVAO(buffer_ref aliasModelVBO, buffer_ref instanceVBO)
{
	R_GenVertexArray(vao_aliasmodel);

	GLM_ConfigureVertexAttribPointer(vao_aliasmodel, aliasModelVBO, 0, 3, GL_FLOAT, GL_FALSE, sizeof(vbo_model_vert_t), VBO_FIELDOFFSET(vbo_model_vert_t, position), 0);
	GLM_ConfigureVertexAttribPointer(vao_aliasmodel, aliasModelVBO, 1, 2, GL_FLOAT, GL_FALSE, sizeof(vbo_model_vert_t), VBO_FIELDOFFSET(vbo_model_vert_t, texture_coords), 0);
	GLM_ConfigureVertexAttribPointer(vao_aliasmodel, aliasModelVBO, 2, 3, GL_FLOAT, GL_FALSE, sizeof(vbo_model_vert_t), VBO_FIELDOFFSET(vbo_model_vert_t, normal), 0);
	GLM_ConfigureVertexAttribIPointer(vao_aliasmodel, instanceVBO, 3, 1, GL_UNSIGNED_INT, sizeof(GLuint), 0, 1);
	GLM_ConfigureVertexAttribIPointer(vao_aliasmodel, aliasModelVBO, 4, 1, GL_INT, sizeof(vbo_model_vert_t), VBO_FIELDOFFSET(vbo_model_vert_t, vert_index), 0);

	R_BindVertexArray(vao_none);
}

static void GL_ImportModelToVBO(model_t* mod, vbo_model_vert_t* aliasmodel_data, int* new_vbo_position)
{
	if (mod->type == mod_alias) {
		aliashdr_t* paliashdr = (aliashdr_t *)Mod_Extradata(mod);

		GL_AliasModelAddToVBO(mod, paliashdr, aliasmodel_data, *new_vbo_position);
		*new_vbo_position += mod->vertsInVBO;
	}
	else if (mod->type == mod_alias3) {
		GL_MD3ModelAddToVBO(mod, aliasmodel_data, *new_vbo_position);

		*new_vbo_position += mod->vertsInVBO;
	}
	else if (mod->type == mod_sprite) {
		mod->vbo_start = 0;
	}
	else if (mod->type == mod_brush) {
		mod->vbo_start = 0;
	}
}

static void GL_ImportSpriteCoordsToVBO(vbo_model_vert_t* verts, int* position)
{
	VectorSet(verts[0].position, 0, -1, -1);
	verts[0].texture_coords[0] = 1;
	verts[0].texture_coords[1] = 1;
	verts[0].vert_index = 0;

	VectorSet(verts[1].position, 0, -1, 1);
	verts[1].texture_coords[0] = 1;
	verts[1].texture_coords[1] = 0;
	verts[1].vert_index = 1;

	VectorSet(verts[2].position, 0, 1, 1);
	verts[2].texture_coords[0] = 0;
	verts[2].texture_coords[1] = 0;
	verts[2].vert_index = 2;

	VectorSet(verts[3].position, 0, 1, -1);
	verts[3].texture_coords[0] = 0;
	verts[3].texture_coords[1] = 1;
	verts[3].vert_index = 3;

	*position += sizeof(verts) / sizeof(verts[0]);
}

void GL_CreateAliasModelVBO(buffer_ref instanceVBO)
{
	vbo_model_vert_t* aliasModelData;
	int new_vbo_position = 0;
	int required_vbo_length = 4;
	buffer_ref vbo;
	int i;

	for (i = 1; i < MAX_MODELS; ++i) {
		model_t* mod = cl.model_precache[i];

		if (mod && (mod->type == mod_alias || mod->type == mod_alias3)) {
			required_vbo_length += mod->vertsInVBO;
		}
	}

	for (i = 0; i < MAX_VWEP_MODELS; i++) {
		model_t* mod = cl.vw_model_precache[i];

		if (mod && (mod->type == mod_alias || mod->type == mod_alias3)) {
			required_vbo_length += mod->vertsInVBO;
		}
	}

	// custom models are explicitly loaded by client, not notified by server
	for (i = 0; i < custom_model_count; ++i) {
		model_t* mod = Mod_CustomModel(i, false);

		if (mod && (mod->type == mod_alias || mod->type == mod_alias3)) {
			required_vbo_length += mod->vertsInVBO;
		}
	}

	// Go back through all models, importing textures into arrays and creating new VBO
	aliasModelData = Q_malloc(required_vbo_length * sizeof(vbo_model_vert_t));

	// VBO starts with simple-model/sprite vertices
	GL_ImportSpriteCoordsToVBO(aliasModelData, &new_vbo_position);

	for (i = 1; i < MAX_MODELS; ++i) {
		model_t* mod = cl.model_precache[i];

		if (mod) {
			GL_ImportModelToVBO(mod, aliasModelData, &new_vbo_position);
		}
	}

	for (i = 0; i < MAX_VWEP_MODELS; i++) {
		model_t* mod = cl.vw_model_precache[i];

		if (mod) {
			GL_ImportModelToVBO(mod, aliasModelData, &new_vbo_position);
		}
	}

	for (i = 0; i < custom_model_count; ++i) {
		model_t* mod = Mod_CustomModel(i, false);

		if (mod) {
			GL_ImportModelToVBO(mod, aliasModelData, &new_vbo_position);
		}
	}

	vbo = GL_CreateFixedBuffer(buffertype_vertex, "aliasmodel-vertex-data", required_vbo_length * sizeof(vbo_model_vert_t), aliasModelData, bufferusage_constant_data);

	if (GL_UseGLSL()) {
		GL_CreateAliasModelVAO(vbo, instanceVBO);
		aliasModel_ssbo = GL_CreateFixedBuffer(buffertype_storage, "aliasmodel-vertex-ssbo", required_vbo_length * sizeof(vbo_model_vert_t), aliasModelData, bufferusage_constant_data);
		GL_BindBufferBase(aliasModel_ssbo, EZQ_GL_BINDINGPOINT_ALIASMODEL_SSBO);
	}
	else {
		GLC_AllocateAliasPoseBuffer();
	}
	Q_free(aliasModelData);
}

static int AssignSampler(aliasmodel_draw_instructions_t* instr, texture_ref texture)
{
	int i;
	texture_ref* allocated_samplers;

	if (!GL_TextureReferenceIsValid(texture)) {
		return 0;
	}
	if (instr->current_call >= sizeof(instr->bound_textures) / sizeof(instr->bound_textures[0])) {
		return -1;
	}

	allocated_samplers = instr->bound_textures[instr->current_call];
	for (i = 0; i < instr->num_textures[instr->current_call]; ++i) {
		if (GL_TextureReferenceEqual(texture, allocated_samplers[i])) {
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
		if (drawAliasModelProgram.custom_options & DRAW_CAUSTIC_TEXTURES) {
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
	model_t* model, float* color, int vbo_start, int vbo_count, texture_ref texture, texture_ref fb_texture,
	int effects, float yaw_angle_radians, float shadelight, float ambientlight,
	float lerpFraction, int lerpFrameVertOffset, qbool outline, qbool shell, int render_effects
)
{
	uniform_block_aliasmodel_t* uniform;
	aliasmodel_draw_type_t type = aliasmodel_draw_std;
	aliasmodel_draw_type_t shelltype = aliasmodel_draw_shells;
	aliasmodel_draw_instructions_t* instr;
	int textureSampler = -1, lumaSampler = -1;

	// Compile here so we can work out how many samplers we have free to allocate per draw-call
	if (!GLM_CompileAliasModelProgram()) {
		return;
	}

	if ((render_effects & RF_WEAPONMODEL) && color[3] < 1) {
		type = aliasmodel_draw_postscene;
		shelltype = aliasmodel_draw_postscene_shells;
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
	lumaSampler = AssignSampler(instr, fb_texture);
	if (textureSampler < 0 || lumaSampler < 0) {
		if (!GLM_NextAliasModelDrawCall(instr, true)) {
			return;
		}

		textureSampler = AssignSampler(instr, texture);
		lumaSampler = AssignSampler(instr, fb_texture);
	}

	// Store static data ready for upload
	memset(&aliasdata.models[alias_draw_count], 0, sizeof(aliasdata.models[alias_draw_count]));
	uniform = &aliasdata.models[alias_draw_count];
	GL_GetModelviewMatrix(uniform->modelViewMatrix);
	uniform->amFlags =
		(effects & EF_RED ? AMF_SHELLMODEL_RED : 0) |
		(effects & EF_GREEN ? AMF_SHELLMODEL_GREEN : 0) |
		(effects & EF_BLUE ? AMF_SHELLMODEL_BLUE : 0) |
		(GL_TextureReferenceIsValid(texture) ? AMF_TEXTURE_MATERIAL : 0) |
		(GL_TextureReferenceIsValid(fb_texture) ? AMF_TEXTURE_LUMA : 0) |
		(render_effects & RF_CAUSTICS ? AMF_CAUSTICS : 0) |
		(render_effects & RF_WEAPONMODEL ? AMF_WEAPONMODEL : 0) |
		(render_effects & RF_LIMITLERP ? AMF_LIMITLERP : 0);
	uniform->yaw_angle_rad = yaw_angle_radians;
	uniform->shadelight = shadelight;
	uniform->ambientlight = ambientlight;
	uniform->lerpFraction = lerpFraction;
	uniform->lerpBaseIndex = lerpFrameVertOffset;
	uniform->color[0] = color[0];
	uniform->color[1] = color[1];
	uniform->color[2] = color[2];
	uniform->color[3] = color[3];
	uniform->materialSamplerMapping = textureSampler;
	uniform->lumaSamplerMapping = lumaSampler;

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

static void GLM_QueueAliasModelDraw(
	model_t* model, float* color, int start, int count,
	texture_ref texture, texture_ref fb_texture,
	int effects, float yaw_angle_radians, float shadelight, float ambientlight,
	float lerpFraction, int lerpFrameVertOffset, qbool outline, int render_effects
)
{
	int shell_effects = effects & (EF_RED | EF_BLUE | EF_GREEN);
	qbool shell = shell_effects && GL_TextureReferenceIsValid(shelltexture) && Ruleset_AllowPowerupShell(model);

	GLM_QueueAliasModelDrawImpl(model, color, start, count, texture, fb_texture, effects, yaw_angle_radians, shadelight, ambientlight, lerpFraction, lerpFrameVertOffset, outline, shell, render_effects);
}

void GL_BeginDrawAliasModels(void)
{
}

void GL_EndDrawAliasModels(void)
{
}

// Called for .mdl & .md3
void GLM_DrawAliasModelFrame(
	model_t* model, int poseVertIndex, int poseVertIndex2, int vertsPerPose,
	texture_ref texture, texture_ref fb_texture, qbool outline, int effects, int render_effects
)
{
	float lerp_fraction = r_framelerp;
	float color[4];

	if (lerp_fraction == 1) {
		poseVertIndex = poseVertIndex2;
		lerp_fraction = 0;
	}

	// TODO: Vertex lighting etc
	// TODO: Coloured lighting per-vertex?
	if (custom_model == NULL) {
		if (r_modelcolor[0] < 0) {
			// normal color
			color[0] = color[1] = color[2] = 1.0f;
		}
		else {
			color[0] = r_modelcolor[0];
			color[1] = r_modelcolor[1];
			color[2] = r_modelcolor[2];
		}
	}
	else {
		color[0] = custom_model->color_cvar.color[0] / 255.0f;
		color[1] = custom_model->color_cvar.color[1] / 255.0f;
		color[2] = custom_model->color_cvar.color[2] / 255.0f;

		if (custom_model->fullbright_cvar.integer) {
			GL_TextureReferenceInvalidate(texture);
		}
	}
	color[0] *= r_modelalpha;
	color[1] *= r_modelalpha;
	color[2] *= r_modelalpha;
	color[3] = r_modelalpha;

	if (gl_caustics.integer && GL_TextureReferenceIsValid(underwatertexture)) {
		if (R_PointIsUnderwater(currententity->origin)) {
			render_effects |= RF_CAUSTICS;
		}
	}

	GLM_QueueAliasModelDraw(
		model, color, poseVertIndex, vertsPerPose,
		texture, fb_texture, effects,
		currententity->angles[YAW] * M_PI / 180.0, shadelight, ambientlight, lerp_fraction, poseVertIndex2, outline, render_effects
	);
}

void GLM_DrawAliasFrame(
	model_t* model, int pose1, int pose2,
	texture_ref texture, texture_ref fb_texture,
	qbool outline, int effects, int render_effects
)
{
	aliashdr_t* paliashdr = (aliashdr_t*) Mod_Extradata(model);
	int vertIndex = paliashdr->vertsOffset + pose1 * paliashdr->vertsPerPose;
	int nextVertIndex = paliashdr->vertsOffset + pose2 * paliashdr->vertsPerPose;

	GLM_DrawAliasModelFrame(
		model, vertIndex, nextVertIndex, paliashdr->vertsPerPose,
		texture, fb_texture, outline, effects, render_effects
	);
}

void GLM_PrepareAliasModelBatches(void)
{
	if (!GLM_CompileAliasModelProgram() || !alias_draw_count) {
		return;
	}

	GL_EnterRegion(__FUNCTION__);

	// Update VBO with data about each entity
	GL_UpdateBuffer(vbo_aliasDataBuffer, sizeof(aliasdata.models[0]) * alias_draw_count, aliasdata.models);
	GL_BindBufferRange(vbo_aliasDataBuffer, EZQ_GL_BINDINGPOINT_ALIASMODEL_DRAWDATA, GL_BufferOffset(vbo_aliasDataBuffer), sizeof(aliasdata.models[0]) * alias_draw_count);

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
			GL_UpdateBufferSection(vbo_aliasIndirectDraw, offset, size, instr->indirect_buffer);
			offset += size;
		}
	}

	GL_LeaveRegion();
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

	GLM_StateBeginAliasModelBatch(type != aliasmodel_draw_std);
	GL_BindBuffer(vbo_aliasIndirectDraw);
	extra_offset = GL_BufferOffset(vbo_aliasIndirectDraw);

	if (type == aliasmodel_draw_shells || type == aliasmodel_draw_postscene_shells) {
		mode = EZQ_ALIAS_MODE_SHELLS;
	}

	GL_UseProgram(drawAliasModelProgram.program);
	SetAliasModelMode(mode);

	// We have prepared the draw calls earlier in the frame so very trival logic here
	for (i = 0; i < instr->num_calls; ++i) {
		if (type == aliasmodel_draw_shells || type == aliasmodel_draw_postscene_shells) {
			R_TextureUnitBind(TEXTURE_UNIT_MATERIAL, shelltexture);
		}
		else if (instr->num_textures[i]) {
			GL_BindTextures(TEXTURE_UNIT_MATERIAL, instr->num_textures[i], instr->bound_textures[i]);
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

		GL_EnterRegion("GLM_DrawOutlineBatch");
		SetAliasModelMode(EZQ_ALIAS_MODE_OUTLINES);
		GLM_StateBeginAliasOutlineBatch();

		for (i = 0; i < instr->num_calls; ++i) {
			GL_MultiDrawArraysIndirect(
				GL_TRIANGLES,
				(const void*)(uintptr_t)(instr->indirect_buffer_offset + extra_offset),
				instr->num_cmds[i],
				0
			);
		}

		GLM_StateEndAliasOutlineBatch();
		GL_LeaveRegion();
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
	GLM_RenderPreparedEntities(aliasmodel_draw_postscene_shells);
}

void GLM_InitialiseAliasModelBatches(void)
{
	alias_draw_count = 0;
	memset(alias_draw_instructions, 0, sizeof(alias_draw_instructions));
}

void GLM_AliasModelShadow(entity_t* ent, aliashdr_t* paliashdr, vec3_t shadevector, vec3_t lightspot)
{
	// MEAG: TODO
}
