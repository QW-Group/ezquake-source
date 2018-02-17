// Alias model rendering (modern)

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "gl_aliasmodel.h"
#include "tr_types.h"
#include "glsl/constants.glsl"

#define MAXIMUM_ALIASMODEL_DRAWCALLS MAX_STANDARD_ENTITIES   // ridiculous
#define MAXIMUM_MATERIAL_SAMPLERS 32

typedef enum aliasmodel_draw_type_s {
	aliasmodel_draw_std,
	aliasmodel_draw_alpha,
	aliasmodel_draw_outlines,
	aliasmodel_draw_shells,

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

extern float r_framelerp;

#define DRAW_DETAIL_TEXTURES 1
#define DRAW_CAUSTIC_TEXTURES 2
#define DRAW_LUMA_TEXTURES 4
static glm_program_t drawAliasModelProgram;
static glm_vao_t aliasModel_vao;
static GLint drawAliasModel_mode;
static uniform_block_aliasmodels_t aliasdata;
static buffer_ref vbo_aliasDataBuffer;
static buffer_ref vbo_aliasIndirectDraw;

static int cached_mode;

static void SetAliasModelMode(int mode)
{
	if (cached_mode != mode) {
		glUniform1i(drawAliasModel_mode, mode);
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
		GL_VFDeclare(model_alias);

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
		GLM_CreateVFProgramWithInclude("AliasModel", GL_VFParams(model_alias), &drawAliasModelProgram, included_definitions);

		drawAliasModelProgram.custom_options = drawAlias_desiredOptions;
	}

	if (drawAliasModelProgram.program && !drawAliasModelProgram.uniforms_found) {
		vbo_aliasDataBuffer = GL_GenFixedBuffer(GL_SHADER_STORAGE_BUFFER, "alias-data", sizeof(aliasdata), NULL, GL_STREAM_DRAW);
		GL_BindBufferBase(vbo_aliasDataBuffer, GL_BINDINGPOINT_ALIASMODEL_DRAWDATA);

		drawAliasModel_mode = glGetUniformLocation(drawAliasModelProgram.program, "mode");
		cached_mode = 0;

		drawAliasModelProgram.uniforms_found = true;
	}

	if (!GL_BufferReferenceIsValid(vbo_aliasIndirectDraw)) {
		vbo_aliasIndirectDraw = GL_GenFixedBuffer(GL_DRAW_INDIRECT_BUFFER, "aliasmodel-indirect-draw", sizeof(alias_draw_instructions[0].indirect_buffer) * aliasmodel_draw_max, NULL, GL_STREAM_DRAW);
	}

	return drawAliasModelProgram.program;
}

void GL_CreateAliasModelVAO(buffer_ref aliasModelVBO, buffer_ref instanceVBO)
{
	GL_GenVertexArray(&aliasModel_vao, "aliasmodel-vao");

	GL_ConfigureVertexAttribPointer(&aliasModel_vao, aliasModelVBO, 0, 3, GL_FLOAT, GL_FALSE, sizeof(vbo_model_vert_t), VBO_FIELDOFFSET(vbo_model_vert_t, position));
	GL_ConfigureVertexAttribPointer(&aliasModel_vao, aliasModelVBO, 1, 2, GL_FLOAT, GL_FALSE, sizeof(vbo_model_vert_t), VBO_FIELDOFFSET(vbo_model_vert_t, texture_coords));
	GL_ConfigureVertexAttribPointer(&aliasModel_vao, aliasModelVBO, 2, 3, GL_FLOAT, GL_FALSE, sizeof(vbo_model_vert_t), VBO_FIELDOFFSET(vbo_model_vert_t, normal));
	GL_ConfigureVertexAttribIPointer(&aliasModel_vao, instanceVBO, 3, 1, GL_UNSIGNED_INT, sizeof(GLuint), 0);
	GL_ConfigureVertexAttribIPointer(&aliasModel_vao, aliasModelVBO, 4, 1, GL_INT, sizeof(vbo_model_vert_t), VBO_FIELDOFFSET(vbo_model_vert_t, vert_index));

	glVertexAttribDivisor(3, 1);
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
	aliasmodel_draw_type_t type = (color[3] == 1 ? aliasmodel_draw_std : aliasmodel_draw_alpha);
	aliasmodel_draw_instructions_t* instr = &alias_draw_instructions[type];
	int textureSampler = -1, lumaSampler = -1;

	// Compile here so we can work out how many samplers we have free to allocate per draw-call
	if (!GLM_CompileAliasModelProgram()) {
		return;
	}

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
	GL_GetMatrix(GL_MODELVIEW, uniform->modelViewMatrix);
	uniform->amFlags =
		(effects & EF_RED ? AMF_SHELLMODEL_RED : 0) |
		(effects & EF_GREEN ? AMF_SHELLMODEL_GREEN : 0) |
		(effects & EF_BLUE ? AMF_SHELLMODEL_BLUE : 0) |
		(GL_TextureReferenceIsValid(texture) ? AMF_TEXTURE_MATERIAL : 0) |
		(GL_TextureReferenceIsValid(fb_texture) ? AMF_TEXTURE_LUMA : 0) |
		(render_effects & RF_CAUSTICS ? AMF_CAUSTICS : 0) |
		(render_effects & RF_WEAPONMODEL ? AMF_WEAPONMODEL : 0);
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
	if (color[3] == 1.0) {
		GLM_QueueDrawCall(aliasmodel_draw_std, vbo_start, vbo_count, alias_draw_count);
		if (outline) {
			GLM_QueueDrawCall(aliasmodel_draw_outlines, vbo_start, vbo_count, alias_draw_count);
		}
	}
	else {
		GLM_QueueDrawCall(aliasmodel_draw_alpha, vbo_start, vbo_count, alias_draw_count);
	}
	if (shell) {
		GLM_QueueDrawCall(aliasmodel_draw_shells, vbo_start, vbo_count, alias_draw_count);
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
	qbool texture_model = GL_TextureReferenceIsValid(texture);
	int shell_effects = effects & (EF_RED | EF_BLUE | EF_GREEN);
	qbool shell = false;

	if (shell_effects && GL_TextureReferenceIsValid(shelltexture)) {
		// always allow powerupshells for specs or demos.
		// do not allow powerupshells for eyes in other cases
		shell = (bound(0, gl_powerupshells.value, 1) && ((cls.demoplayback || cl.spectator) || model->modhint != MOD_EYES));
	}

	GLM_QueueAliasModelDrawImpl(model, color, start, count, texture, fb_texture, effects, yaw_angle_radians, shadelight, ambientlight, lerpFraction, lerpFrameVertOffset, outline, shell, render_effects);
}

void GL_BeginDrawAliasModels(void)
{
	if (gl_affinemodels.value) {
		GL_Hint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	}
	if (gl_smoothmodels.value) {
		GL_ShadeModel(GL_SMOOTH);
	}
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

	// FIXME: Need to take into account the RF_LIMITLERP flag which is used on Team Fortress viewmodels

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

void GLM_DrawPowerupShell(
	model_t* model, int effects, int layer_no,
	maliasframedesc_t *oldframe, maliasframedesc_t *frame
)
{
	return;
}

void GLM_PrepareAliasModelBatches(void)
{
	if (!GLM_CompileAliasModelProgram()) {
		return;
	}

	// Update VBO with data about each entity
	GL_BindAndUpdateBuffer(vbo_aliasDataBuffer, sizeof(aliasdata.models[0]) * alias_draw_count, aliasdata.models);

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
}

static void GLM_RenderPreparedEntities(aliasmodel_draw_type_t type)
{
	aliasmodel_draw_instructions_t* instr = &alias_draw_instructions[type];
	GLint mode = EZQ_ALIAS_MODE_NORMAL;
	unsigned int extra_offset = 0;
	int batch = 0;
	int i;

	if (!instr->num_calls || !GLM_CompileAliasModelProgram()) {
		return;
	}

	GL_AlphaBlendFlags(type == aliasmodel_draw_std ? GL_BLEND_DISABLED : GL_BLEND_ENABLED);

	if (type == aliasmodel_draw_shells) {
		if (gl_powerupshells_style.integer) {
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		}
		else {
			GL_BlendFunc(GL_ONE, GL_ONE);
		}
		mode = EZQ_ALIAS_MODE_SHELLS;
	}
	else {
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	GL_BindVertexArray(&aliasModel_vao);
	GL_UseProgram(drawAliasModelProgram.program);
	SetAliasModelMode(mode);
	GL_BindBuffer(vbo_aliasIndirectDraw);

	// We have prepared the draw calls earlier in the frame so very trival logic here
	for (i = 0; i < instr->num_calls; ++i) {
		if (type == aliasmodel_draw_shells) {
			GL_EnsureTextureUnitBound(GL_TEXTURE0 + TEXTURE_UNIT_MATERIAL, shelltexture);
		}
		else if (instr->num_textures[i]) {
			GL_BindTextures(TEXTURE_UNIT_MATERIAL, instr->num_textures[i], instr->bound_textures[i]);
		}

		glMultiDrawArraysIndirect(
			GL_TRIANGLES,
			(const void*)(instr->indirect_buffer_offset + extra_offset),
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
			glMultiDrawArraysIndirect(
				GL_TRIANGLES,
				(const void*)instr->indirect_buffer_offset,
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

void GLM_InitialiseAliasModelBatches(void)
{
	alias_draw_count = 0;
	memset(alias_draw_instructions, 0, sizeof(alias_draw_instructions));
}

void GLM_AliasModelShadow(entity_t* ent, aliashdr_t* paliashdr, vec3_t shadevector, vec3_t lightspot)
{
	// MEAG: TODO
}
