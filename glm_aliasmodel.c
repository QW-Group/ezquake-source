// Alias model rendering (modern)

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "gl_aliasmodel.h"
#include "tr_types.h"
#include "glsl/constants.glsl"

#define MAXIMUM_ALIASMODEL_DRAWCALLS MAX_STANDARD_ENTITIES   // ridiculous
#define MAXIMUM_MATERIAL_SAMPLERS 32

typedef struct DrawArraysIndirectCommand_s {
	GLuint count;
	GLuint instanceCount;
	GLuint first;
	GLuint baseInstance;
} DrawArraysIndirectCommand_t;

// This is passed to OpenGL
typedef struct glm_aliasmodel_uniformdata_s {
	float mvMatrix[16];
	float color[4];
	int amFlags;
	float yaw_angle_radians;
	float shadelight;
	float ambientlight;

	// Get away with these being set per-object as extra passes (outlines & shells) don't use texture
	int materialTextureMapping;
	int lumaTextureMapping;
	int lerpFrameVertOffset;
	float lerpFraction;
} glm_aliasmodel_uniformdata_t;

typedef enum aliasmodel_draw_type_s {
	aliasmodel_draw_std,
	aliasmodel_draw_alpha,
	aliasmodel_draw_outlines,
	aliasmodel_draw_shells,

	aliasmodel_draw_max
} aliasmodel_draw_type_t;

typedef struct aliasmodel_draw_data_s {
	// Need to split standard & alpha model draws to cope with # of texture units available
	//   Outlines & shells are easier as they have 0 & 1 textures respectively
	DrawArraysIndirectCommand_t indirect_buffer[MAXIMUM_ALIASMODEL_DRAWCALLS];
	texture_ref bound_textures[MAXIMUM_ALIASMODEL_DRAWCALLS][MAXIMUM_MATERIAL_SAMPLERS];
	int num_textures[MAXIMUM_ALIASMODEL_DRAWCALLS];
	int num_cmds[MAXIMUM_ALIASMODEL_DRAWCALLS];
	int num_calls;

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

static void GLM_QueueAliasModelDraw(
	model_t* model, byte* color, int start, int count,
	texture_ref texture, texture_ref fb_texture,
	int effects, float yaw_angle_radians, float shadelight, float ambientlight,
	float lerpFraction, int lerpFrameVertOffset, qbool outline
);

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
		vbo_aliasDataBuffer = GL_GenUniformBuffer("alias-data", &aliasdata, sizeof(aliasdata));

		drawAliasModel_mode = glGetUniformLocation(drawAliasModelProgram.program, "mode");

		drawAliasModelProgram.uniforms_found = true;
	}

	if (!GL_BufferReferenceIsValid(vbo_aliasIndirectDraw)) {
		vbo_aliasIndirectDraw = GL_GenFixedBuffer(GL_DRAW_INDIRECT_BUFFER, "aliasmodel-indirect-draw", sizeof(alias_draw_instructions[0].indirect_buffer) * aliasmodel_draw_max, NULL, GL_STREAM_DRAW);
	}

	return drawAliasModelProgram.program;
}

static void GLM_SetPowerupShellColor(float* shell_color, float base_level, float effect_level, int effects)
{
	base_level = bound(0, base_level, 1);
	effect_level = bound(0, effect_level, 1);

	shell_color[0] = shell_color[1] = shell_color[2] = base_level;
	shell_color[3] = bound(0, gl_powerupshells.value, 1);
	if (effects & EF_RED) {
		shell_color[0] += effect_level;
	}
	if (effects & EF_GREEN) {
		shell_color[1] += effect_level;
	}
	if (effects & EF_BLUE) {
		shell_color[2] += effect_level;
	}
}



static int AssignSampler(aliasmodel_draw_instructions_t* instr, texture_ref texture)
{
	int i;
	texture_ref* allocated_samplers;

	if (!GL_TextureReferenceIsValid(texture)) {
		return 0;
	}
	if (instr->num_calls >= sizeof(instr->bound_textures) / sizeof(instr->bound_textures[0])) {
		return -1;
	}

	allocated_samplers = instr->bound_textures[instr->num_calls];
	for (i = 0; i < instr->num_textures[instr->num_calls]; ++i) {
		if (GL_TextureReferenceEqual(texture, allocated_samplers[i])) {
			return i;
		}
	}

	if (instr->num_textures[instr->num_calls] < material_samplers_max) {
		allocated_samplers[instr->num_textures[instr->num_calls]] = texture;
		return instr->num_textures[instr->num_calls]++;
	}

	return -1;
}

static qbool GLM_NextAliasModelDrawCall(aliasmodel_draw_instructions_t* instr)
{
	if (instr->num_calls >= sizeof(instr->num_textures) / sizeof(instr->num_textures[0])) {
		return false;
	}

	instr->num_calls++;
	instr->num_textures[instr->num_calls] = 0;
	if (drawAliasModelProgram.custom_options & DRAW_CAUSTIC_TEXTURES) {
		instr->bound_textures[instr->num_calls][0] = underwatertexture;
		instr->num_textures[instr->num_calls]++;
	}
	return true;
}

static void GLM_QueueDrawCall(aliasmodel_draw_type_t type, int uniform_index, texture_ref texture, texture_ref fb_texture, int vbo_start, int vbo_count)
{
	int pos;
	aliasmodel_draw_instructions_t* instr = &alias_draw_instructions[type];
	DrawArraysIndirectCommand_t* indirect;

	if (instr->num_cmds[instr->num_calls] >= sizeof(instr->indirect_buffer) / sizeof(instr->indirect_buffer[0])) {
		return;
	}

	pos = instr->num_cmds[instr->num_calls]++;
	indirect = &instr->indirect_buffer[pos];
	indirect->instanceCount = 1;
	indirect->baseInstance = pos;
	indirect->first = vbo_start;
	indirect->count = vbo_count;
}

static void GLM_QueueAliasModelDrawImpl(
	model_t* model, byte* color, int vbo_start, int vbo_count, texture_ref texture, texture_ref fb_texture,
	int effects, float yaw_angle_radians, float shadelight, float ambientlight,
	float lerpFraction, int lerpFrameVertOffset, qbool outline, qbool shell
)
{
	glm_aliasmodel_uniformdata_t* uniform;
	aliasmodel_draw_type_t type = color[4] == 255 ? aliasmodel_draw_std : aliasmodel_draw_alpha;
	aliasmodel_draw_instructions_t* instr = &alias_draw_instructions[type];

	int textureSampler = -1, lumaSampler = -1;
	int uniform_index;

	// Compile here so we can work out how many samplers we have free to allocate per draw-call
	if (!GLM_CompileAliasModelProgram()) {
		return;
	}

	// Should never happen...
	if (alias_draw_count >= sizeof(aliasdata.models) / sizeof(aliasdata.models[0])) {
		return;
	}

	// Assign samplers - if we're over limit, need to flush and try again
	textureSampler = AssignSampler(instr, texture);
	lumaSampler = AssignSampler(instr, fb_texture);
	if (textureSampler < 0 || lumaSampler < 0) {
		if (!GLM_NextAliasModelDrawCall(instr)) {
			return;
		}

		textureSampler = AssignSampler(instr, texture);
		lumaSampler = AssignSampler(instr, fb_texture);
	}

	// Store static data ready for upload
	uniform_index = alias_draw_count;
	memset(&aliasdata.models[uniform_index], 0, sizeof(aliasdata.models[uniform_index]));
	uniform = &aliasdata.models[uniform_index];
	GL_GetMatrix(GL_MODELVIEW, uniform->mvMatrix);
	uniform->amFlags =
		(effects & EF_RED ? AMF_SHELLMODEL_RED : 0) |
		(effects & EF_GREEN ? AMF_SHELLMODEL_GREEN : 0) |
		(effects & EF_BLUE ? AMF_SHELLMODEL_BLUE : 0) |
		(GL_TextureReferenceIsValid(texture) ? AMF_TEXTURE_MATERIAL : 0) |
		(GL_TextureReferenceIsValid(fb_texture) ? AMF_TEXTURE_LUMA : 0);
	uniform->yaw_angle_radians = yaw_angle_radians;
	uniform->shadelight = shadelight;
	uniform->ambientlight = ambientlight;
	uniform->lerpFraction = lerpFraction;
	uniform->lerpFrameVertOffset = lerpFrameVertOffset;
	uniform->color[0] = color[0] * 1.0f / 255;
	uniform->color[1] = color[1] * 1.0f / 255;
	uniform->color[2] = color[2] * 1.0f / 255;
	uniform->color[3] = color[3] * 1.0f / 255;
	uniform->materialTextureMapping = textureSampler;
	uniform->lumaTextureMapping = lumaSampler;

	// Add to queues
	if (color[4] == 255) {
		GLM_QueueDrawCall(aliasmodel_draw_std, uniform_index, texture, fb_texture, vbo_start, vbo_count);
		if (outline) {
			GLM_QueueDrawCall(aliasmodel_draw_outlines, uniform_index, null_texture_reference, null_texture_reference, vbo_start, vbo_count);
		}
	}
	else {
		GLM_QueueDrawCall(aliasmodel_draw_alpha, uniform_index, texture, fb_texture, vbo_start, vbo_count);
	}
	if (shell) {
		GLM_QueueDrawCall(aliasmodel_draw_shells, uniform_index, shelltexture, null_texture_reference, vbo_start, vbo_count);
	}
}

static void GLM_QueueAliasModelShellDraw(
	model_t* model, int effects, int start, int count,
	float yaw_angle_radians, float lerpFraction, int lerpFrameVertOffset
)
{
	return;
}

static void GLM_QueueAliasModelDraw(
	model_t* model, byte* color, int start, int count,
	texture_ref texture, texture_ref fb_texture,
	int effects, float yaw_angle_radians, float shadelight, float ambientlight,
	float lerpFraction, int lerpFrameVertOffset, qbool outline
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

	GLM_QueueAliasModelDrawImpl(model, color, start, count, texture, fb_texture, effects, yaw_angle_radians, shadelight, ambientlight, lerpFraction, lerpFrameVertOffset, outline, shell);
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

void GLM_AliasModelShadow(entity_t* ent, aliashdr_t* paliashdr, vec3_t shadevector, vec3_t lightspot)
{
	// MEAG: TODO
}

void GLM_DrawAliasModelFrame(
	model_t* model, int poseVertIndex, int poseVertIndex2, int vertsPerPose,
	texture_ref texture, texture_ref fb_texture,
	qbool outline
)
{
	float lerp_fraction = r_framelerp;
	byte color[4];
	int effects = 0;

	if (lerp_fraction == 1) {
		poseVertIndex = poseVertIndex2;
		lerp_fraction = 0;
	}

	// FIXME: Need to take into account the RF_LIMITLERP flag which is used on Team Fortress viewmodels

	// FIXME: This has been converted from 4 bytes already?
	if (r_modelcolor[0] < 0) {
		color[0] = color[1] = color[2] = 255;
	}
	else {
		color[0] = r_modelcolor[0] * 255;
		color[1] = r_modelcolor[1] * 255;
		color[2] = r_modelcolor[2] * 255;
	}
	color[3] = r_modelalpha * 255;

	// TODO: Vertex lighting etc
	// TODO: Coloured lighting per-vertex?
	if (custom_model == NULL) {
		if (r_modelcolor[0] < 0) {
			// normal color
			color[0] = color[1] = color[2] = 255;
		}
	}
	else {
		color[0] = custom_model->color_cvar.color[0];
		color[1] = custom_model->color_cvar.color[1];
		color[2] = custom_model->color_cvar.color[2];

		if (custom_model->fullbright_cvar.integer) {
			GL_TextureReferenceInvalidate(texture);
		}
	}

	if (gl_caustics.integer && GL_TextureReferenceIsValid(underwatertexture)) {
		if (R_PointIsUnderwater(currententity->origin)) {
			effects |= EF_CAUSTICS;
		}
	}

	GLM_QueueAliasModelDraw(
		model, color, poseVertIndex, vertsPerPose,
		texture, fb_texture, effects,
		currententity->angles[YAW] * M_PI / 180.0, shadelight, ambientlight, lerp_fraction, poseVertIndex2, outline
	);
}

void GLM_DrawAliasFrame(
	model_t* model, int pose1, int pose2,
	qbool scrolldir, texture_ref texture, texture_ref fb_texture,
	qbool outline
)
{
	aliashdr_t* paliashdr = (aliashdr_t*) Mod_Extradata(model);
	int vertIndex = paliashdr->vertsOffset + pose1 * paliashdr->vertsPerPose;
	int nextVertIndex = paliashdr->vertsOffset + pose2 * paliashdr->vertsPerPose;

	GLM_DrawAliasModelFrame(
		model, vertIndex, nextVertIndex, paliashdr->vertsPerPose,
		texture, fb_texture, outline
	);
}

void GLM_DrawPowerupShell(
	model_t* model, int effects, int layer_no /* not used but keeps signature in sync with classic */,
	maliasframedesc_t *oldframe, maliasframedesc_t *frame
)
{
	aliashdr_t* paliashdr = (aliashdr_t*)Mod_Extradata(model);
	int pose1 = R_AliasFramePose(oldframe);
	int pose2 = R_AliasFramePose(frame);
	int poseVertIndex = paliashdr->vertsOffset + pose1 * paliashdr->vertsPerPose;
	int poseVertIndex2 = paliashdr->vertsOffset + pose2 * paliashdr->vertsPerPose;
	float lerp_fraction = r_framelerp;

	if (!GL_TextureReferenceIsValid(shelltexture)) {
		return;
	}

	if (lerp_fraction == 1) {
		poseVertIndex = poseVertIndex2;
		lerp_fraction = 0;
	}

	GLM_QueueAliasModelShellDraw(model, effects, poseVertIndex, paliashdr->vertsPerPose, currententity->angles[YAW] * M_PI / 180.0, lerp_fraction, poseVertIndex2);
}

void GL_CreateAliasModelVAO(buffer_ref aliasModelVBO, buffer_ref instanceVBO)
{
	GL_GenVertexArray(&aliasModel_vao);

	GL_ConfigureVertexAttribPointer(&aliasModel_vao, aliasModelVBO, 0, 3, GL_FLOAT, GL_FALSE, sizeof(vbo_model_vert_t), VBO_FIELDOFFSET(vbo_model_vert_t, position));
	GL_ConfigureVertexAttribPointer(&aliasModel_vao, aliasModelVBO, 1, 2, GL_FLOAT, GL_FALSE, sizeof(vbo_model_vert_t), VBO_FIELDOFFSET(vbo_model_vert_t, texture_coords));
	GL_ConfigureVertexAttribPointer(&aliasModel_vao, aliasModelVBO, 2, 3, GL_FLOAT, GL_FALSE, sizeof(vbo_model_vert_t), VBO_FIELDOFFSET(vbo_model_vert_t, normal));
	GL_ConfigureVertexAttribIPointer(&aliasModel_vao, instanceVBO, 3, 1, GL_UNSIGNED_INT, sizeof(GLuint), 0);
	GL_ConfigureVertexAttribIPointer(&aliasModel_vao, aliasModelVBO, 4, 1, GL_INT, sizeof(vbo_model_vert_t), VBO_FIELDOFFSET(vbo_model_vert_t, vert_index));

	glVertexAttribDivisor(3, 1);
}

void GLM_PrepareAliasModelBatches(void)
{
	if (!GLM_CompileAliasModelProgram()) {
		return;
	}

	// Update VBO with data about each entity
	GL_UpdateVBO(vbo_aliasDataBuffer, sizeof(aliasdata.models[0]) * alias_draw_count, aliasdata.models);

	// Build & update list of indirect calls
	{
		int i, j;
		int offset = 0;

		for (i = 0; i < aliasmodel_draw_max; ++i) {
			aliasmodel_draw_instructions_t* instr = &alias_draw_instructions[i];
			int total_cmds = 0;
			int size;

			instr->indirect_buffer_offset = offset;
			for (j = 0; j < instr->num_calls; ++j) {
				total_cmds += instr->num_cmds[i];
			}

			size = sizeof(instr->indirect_buffer[0]) * total_cmds;
			GL_UpdateVBOSection(vbo_aliasIndirectDraw, offset, size, instr->indirect_buffer);
			offset += size;
		}
	}
}

void GLM_DrawAliasModelBatch(aliasmodel_draw_type_t type)
{
	aliasmodel_draw_instructions_t* instr = &alias_draw_instructions[type];
	int batch = 0;
	int i;

	if (!instr->num_calls || !GLM_CompileAliasModelProgram()) {
		return;
	}

	if (type == aliasmodel_draw_shells) {
		GL_AlphaBlendFlags(GL_BLEND_ENABLED);
		if (gl_powerupshells_style.integer) {
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		}
		else {
			GL_BlendFunc(GL_ONE, GL_ONE);
		}
	}
	else {
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	GL_BindVertexArray(&aliasModel_vao);
	GL_UseProgram(drawAliasModelProgram.program);
	GL_BindBuffer(vbo_aliasIndirectDraw);
	glUniform1i(drawAliasModel_mode, EZQ_ALIAS_MODE_NORMAL);

	// We have prepared the draw calls earlier in the frame so very trival logic here
	for (i = 0; i < instr->num_calls; ++i) {
		GL_BindTextures(GL_TEXTURE0, instr->num_textures[i], instr->bound_textures[i]);

		glMultiDrawArraysIndirect(
			GL_TRIANGLES,
			(const void*)instr->indirect_buffer_offset,
			instr->num_cmds[i],
			0
		);
	}

	if (type == aliasmodel_draw_std && alias_draw_instructions[aliasmodel_draw_std].num_calls) {
		instr = &alias_draw_instructions[aliasmodel_draw_outlines];

		GL_EnterRegion("GLM_DrawOutlineBatch");
		glUniform1i(drawAliasModel_mode, EZQ_ALIAS_MODE_OUTLINES);
		GL_StateBeginAliasOutlineFrame();

		for (i = 0; i < instr->num_calls; ++i) {
			glMultiDrawArraysIndirect(
				GL_TRIANGLES,
				(const void*)instr->indirect_buffer_offset,
				instr->num_cmds[i],
				0
			);
		}

		GL_StateEndAliasOutlineFrame();
		GL_LeaveRegion();
	}
}
