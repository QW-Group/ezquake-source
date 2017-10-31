// Alias model rendering (modern)

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "gl_aliasmodel.h"
#include "tr_types.h"
#include "glsl/constants.glsl"

#define MAXIMUM_MATERIAL_SAMPLERS 32
#define ALIAS_UBO_FOFS(x) (void*)((intptr_t)&(((block_aliasmodels_t*)0)->x))
#define VBO_ALIASVERT_FOFS(x) (void*)((intptr_t)&(((vbo_model_vert_t*)0)->x))

static qbool first_alias_model = true;
static glm_vao_t aliasModel_vao;
extern float r_framelerp;

#define DRAW_DETAIL_TEXTURES 1
#define DRAW_CAUSTIC_TEXTURES 2
#define DRAW_LUMA_TEXTURES 4
static glm_program_t drawAliasModelProgram;
static GLuint drawAliasModel_RefdefCvars_block;
static GLuint drawAliasModel_AliasData_block;
static GLint drawAliasModel_outlines;
static uniform_block_aliasmodels_t aliasdata;
static buffer_ref ubo_aliasdata;
static buffer_ref vbo_aliasIndirectDraw;

typedef struct glm_aliasmodelbatch_s {
	int start;
	int end;

	qbool shells;
	qbool any_outlines;

	texture_ref skin_texture;
	texture_ref skin_texture_fb;
	texture_ref array_texture;
} glm_aliasmodelbatch_t;

static void GLM_QueueAliasModelDraw(
	model_t* model, byte* color, int start, int count,
	texture_ref texture, texture_ref fb_texture,
	int effects, float yaw_angle_radians, float shadelight, float ambientlight,
	float lerpFraction, int lerpFrameVertOffset, qbool outline
);

typedef struct glm_aliasmodel_req_s {
	GLuint vbo_count;
	GLuint instanceCount;
	GLuint vbo_start;
	GLuint baseInstance;
	float mvMatrix[16];
	//float texScale[2];
	int texture_skin_sampler;
	int texture_fb_sampler;
	byte color[4];
	//int texture_model;
	int amFlags;
	float yaw_angle_radians;
	float shadelight;
	float ambientlight;
	qbool outline;

	float lerpFraction;
	int lerpFrameVertOffset;
} glm_aliasmodel_req_t;

typedef struct DrawArraysIndirectCommand_s {
	GLuint count;
	GLuint instanceCount;
	GLuint first;
	GLuint baseInstance;
} DrawArraysIndirectCommand_t;

static int material_samplers_max;
static int material_samplers;
static texture_ref allocated_samplers[MAXIMUM_MATERIAL_SAMPLERS];
static int TEXTURE_UNIT_MATERIAL;
static int TEXTURE_UNIT_CAUSTICS;

static glm_aliasmodel_req_t aliasmodel_requests[MAX_ALIASMODEL_BATCH];
static int batch_count = 0;
static qbool in_batch_mode = false;

static qbool GLM_CompileAliasModelProgram(void)
{
	qbool caustic_textures = gl_caustics.integer && GL_TextureReferenceIsValid(underwatertexture);
	unsigned int drawAlias_desiredOptions = (caustic_textures ? DRAW_CAUSTIC_TEXTURES : 0);

	if (GLM_ProgramRecompileNeeded(&drawAliasModelProgram, drawAlias_desiredOptions)) {
		static char included_definitions[1024];
		GL_VFDeclare(model_alias);

		strlcpy(included_definitions, va("#define GL_BINDINGPOINT_ALIASMODEL_SSBO %d\n", GL_BINDINGPOINT_ALIASMODEL_SSBO), sizeof(included_definitions));
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
		strlcat(included_definitions, va("#define MAX_INSTANCEID %d\n", MAX_ALIASMODEL_BATCH), sizeof(included_definitions));

		// Initialise program for drawing image
		GLM_CreateVFProgramWithInclude("AliasModel", GL_VFParams(model_alias), &drawAliasModelProgram, included_definitions);

		drawAliasModelProgram.custom_options = drawAlias_desiredOptions;
	}

	if (drawAliasModelProgram.program && !drawAliasModelProgram.uniforms_found) {
		GLint size;

		drawAliasModel_RefdefCvars_block = glGetUniformBlockIndex(drawAliasModelProgram.program, "RefdefCvars");
		drawAliasModel_AliasData_block = glGetUniformBlockIndex(drawAliasModelProgram.program, "AliasModelData");

		glGetActiveUniformBlockiv(drawAliasModelProgram.program, drawAliasModel_AliasData_block, GL_UNIFORM_BLOCK_DATA_SIZE, &size);

		glUniformBlockBinding(drawAliasModelProgram.program, drawAliasModel_AliasData_block, GL_BINDINGPOINT_ALIASMODEL_CVARS);

		ubo_aliasdata = GL_GenUniformBuffer("alias-data", &aliasdata, sizeof(aliasdata));
		GL_BindBufferBase(ubo_aliasdata, GL_BINDINGPOINT_ALIASMODEL_CVARS);

		drawAliasModel_outlines = glGetUniformLocation(drawAliasModelProgram.program, "outlines");

		drawAliasModelProgram.uniforms_found = true;
	}

	if (!GL_BufferReferenceIsValid(vbo_aliasIndirectDraw)) {
		vbo_aliasIndirectDraw = GL_GenFixedBuffer(GL_DRAW_INDIRECT_BUFFER, "aliasmodel-indirect-draw", sizeof(aliasmodel_requests), aliasmodel_requests, GL_STREAM_DRAW);
	}

	return drawAliasModelProgram.program;
}

static void GLM_DrawOutlineBatch(int start, int end)
{
	int i;
	int begin = -1;

	GL_EnterRegion("GLM_DrawOutlineBatch");
	glUniform1i(drawAliasModel_outlines, 1);
	GL_StateBeginAliasOutlineFrame();

	for (i = start; i <= end; ++i) {
		if (!aliasmodel_requests[i].outline) {
			if (begin >= 0) {
				// Draw outline models so far
				glMultiDrawArraysIndirect(GL_TRIANGLES, (void*)(begin * sizeof(aliasmodel_requests[0])), i - begin, sizeof(aliasmodel_requests[0]));
			}
			begin = -1;
			continue;
		}
		else if (begin < 0) {
			begin = i;
		}
	}

	if (begin >= 0) {
		// Draw outline models to end
		glMultiDrawArraysIndirect(GL_TRIANGLES, (void*)(begin * sizeof(aliasmodel_requests[0])), end - begin + 1, sizeof(aliasmodel_requests[0]));
	}

	glUniform1i(drawAliasModel_outlines, 0);
	GL_StateEndAliasOutlineFrame();
	GL_LeaveRegion();
}

static void GLM_FlushAliasModelBatch(void)
{
	glm_aliasmodelbatch_t batches[MAX_ALIASMODEL_BATCH] = { 0 };
	qbool was_texture_array = false;
	qbool was_shells = false;
	qbool was_outline = false;
	int batch = 0;
	int i;

	memset(&aliasdata, 0, sizeof(aliasdata));
	memset(batches, 0, sizeof(batches));

	if (in_batch_mode && first_alias_model) {
		if (GL_ShadersSupported()) {
			GL_EnterRegion("AliasModels");

			first_alias_model = false;
		}
	}

	if (GLM_CompileAliasModelProgram()) {
		// Bind textures
		if (drawAliasModelProgram.custom_options & DRAW_CAUSTIC_TEXTURES) {
			GL_EnsureTextureUnitBound(GL_TEXTURE0 + TEXTURE_UNIT_CAUSTICS, underwatertexture);
		}
		GL_BindTextures(TEXTURE_UNIT_MATERIAL, material_samplers, allocated_samplers);

		// Update indirect buffer
		GL_UpdateVBO(vbo_aliasIndirectDraw, sizeof(aliasmodel_requests), &aliasmodel_requests);

		GL_UseProgram(drawAliasModelProgram.program);
		GL_BindVertexArray(&aliasModel_vao);

		for (i = 0; i < batch_count; ++i) {
			glm_aliasmodel_req_t* req = &aliasmodel_requests[i];
			qbool is_shells = (req->amFlags & AMF_SHELLFLAGS);
			qbool shell_mode_switch = (was_shells != is_shells);

			if (shell_mode_switch) {
				if (i != 0) {
					++batch;
				}

				batches[batch].start = i;
				was_shells = batches[batch].shells = is_shells;
			}

			batches[batch].any_outlines |= req->outline;

			memcpy(&aliasdata.models[i].modelViewMatrix[0], req->mvMatrix, sizeof(aliasdata.models[i].modelViewMatrix));
			aliasdata.models[i].color[0] = req->color[0] * 1.0f / 255;
			aliasdata.models[i].color[1] = req->color[1] * 1.0f / 255;
			aliasdata.models[i].color[2] = req->color[2] * 1.0f / 255;
			aliasdata.models[i].color[3] = req->color[3] * 1.0f / 255;
			aliasdata.models[i].amFlags = req->amFlags;
			aliasdata.models[i].ambientlight = req->ambientlight;
			aliasdata.models[i].shadelight = req->shadelight;
			aliasdata.models[i].yaw_angle_rad = req->yaw_angle_radians;
			aliasdata.models[i].materialSamplerMapping = req->texture_skin_sampler;
			aliasdata.models[i].lumaSamplerMapping = req->texture_fb_sampler;
			aliasdata.models[i].lerpFraction = req->lerpFraction;
			aliasdata.models[i].lerpBaseIndex = req->lerpFrameVertOffset;
			batches[batch].end = i;

			aliasmodel_requests[i].baseInstance = i;
			aliasmodel_requests[i].instanceCount = 1;
		}

		// Update data
		GL_UpdateVBO(ubo_aliasdata, sizeof(aliasdata), &aliasdata);

		for (i = 0; i <= batch; ++i) {
			if (batches[i].shells) {
				GL_AlphaBlendFlags(GL_BLEND_ENABLED);
				// FIXME: Should be able to move this to Begin() ... but, could be R_DrawViewModel() and not batched...
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

			glMultiDrawArraysIndirect(GL_TRIANGLES, (void*)(batches[i].start * sizeof(aliasmodel_requests[0])), batches[i].end - batches[i].start + 1, sizeof(aliasmodel_requests[0]));

			if (batches[i].any_outlines) {
				GLM_DrawOutlineBatch(batches[i].start, batches[i].end);
			}
		}
	}

	batch_count = 0;
	material_samplers = 0;
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

static int AssignSampler(texture_ref texture)
{
	int i;

	if (!GL_TextureReferenceIsValid(texture)) {
		return 0;
	}

	for (i = 0; i < material_samplers; ++i) {
		if (GL_TextureReferenceEqual(texture, allocated_samplers[i])) {
			return i;
		}
	}

	if (material_samplers < material_samplers_max) {
		allocated_samplers[material_samplers] = texture;
		return material_samplers++;
	}

	return -1;
}

void GLM_QueueAliasModelDrawImpl(
	model_t* model, byte* color, int start, int count, texture_ref texture, texture_ref fb_texture,
	int effects, float yaw_angle_radians, float shadelight, float ambientlight,
	float lerpFraction, int lerpFrameVertOffset, qbool outline
)
{
	glm_aliasmodel_req_t* req;
	int textureSampler = -1, lumaSampler = -1;

	if (batch_count >= MAX_ALIASMODEL_BATCH) {
		GLM_FlushAliasModelBatch();
	}

	// Compile here so we can work out how many samplers we have free to allocate per draw-call
	if (!GLM_CompileAliasModelProgram()) {
		return;
	}

	// Assign samplers - if we're over limit, need to flush and try again
	textureSampler = AssignSampler(texture);
	lumaSampler = AssignSampler(fb_texture);
	if (textureSampler < 0 || lumaSampler < 0) {
		GLM_FlushAliasModelBatch();

		textureSampler = AssignSampler(texture);
		lumaSampler = AssignSampler(fb_texture);
	}

	req = &aliasmodel_requests[batch_count];
	GL_GetMatrix(GL_MODELVIEW, req->mvMatrix);
	req->vbo_start = start;
	req->vbo_count = count;
	req->texture_skin_sampler = textureSampler;
	req->texture_fb_sampler = lumaSampler;
	req->instanceCount = 1;
	req->amFlags =
		(effects & EF_RED ? AMF_SHELLMODEL_RED : 0) |
		(effects & EF_GREEN ? AMF_SHELLMODEL_GREEN : 0) |
		(effects & EF_BLUE ? AMF_SHELLMODEL_BLUE : 0) |
		(GL_TextureReferenceIsValid(texture) ? AMF_TEXTURE_MATERIAL : 0) |
		(GL_TextureReferenceIsValid(fb_texture) ? AMF_TEXTURE_LUMA : 0);
	req->yaw_angle_radians = yaw_angle_radians;
	req->shadelight = shadelight;
	req->ambientlight = ambientlight;
	req->lerpFraction = lerpFraction;
	req->lerpFrameVertOffset = lerpFrameVertOffset;
	req->outline = outline;
	memcpy(req->color, color, 4);
	++batch_count;
}

static void GLM_QueueAliasModelShellDraw(
	model_t* model, int effects, int start, int count,
	float yaw_angle_radians, float lerpFraction, int lerpFrameVertOffset
)
{
	int shell_effects = effects & (EF_RED | EF_BLUE | EF_GREEN);
	effects &= ~shell_effects;

	if (!GL_TextureReferenceIsValid(shelltexture)) {
		return;
	}
	if (shell_effects) {
		// always allow powerupshells for specs or demos.
		// do not allow powerupshells for eyes in other cases
		if (bound(0, gl_powerupshells.value, 1) && ((cls.demoplayback || cl.spectator) || model->modhint != MOD_EYES)) {
			GLM_QueueAliasModelDrawImpl(model, color_white, start, count, shelltexture, null_texture_reference, shell_effects, yaw_angle_radians, shadelight, ambientlight, lerpFraction, lerpFrameVertOffset, false);
			if (!in_batch_mode) {
				GLM_FlushAliasModelBatch();
			}
		}
	}
}

static void GLM_QueueAliasModelDraw(
	model_t* model, byte* color, int start, int count,
	texture_ref texture, texture_ref fb_texture,
	int effects, float yaw_angle_radians, float shadelight, float ambientlight,
	float lerpFraction, int lerpFrameVertOffset, qbool outline
)
{
	qbool texture_model = GL_TextureReferenceIsValid(texture);

	GLM_QueueAliasModelDrawImpl(model, color, start, count, texture, fb_texture, effects, yaw_angle_radians, shadelight, ambientlight, lerpFraction, lerpFrameVertOffset, outline);
	if (!in_batch_mode) {
		GLM_FlushAliasModelBatch();
	}
}

void GL_BeginDrawAliasModels(void)
{
	first_alias_model = true;
	batch_count = 0;
	in_batch_mode = true;
	material_samplers = 0;

	if (gl_affinemodels.value) {
		GL_Hint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	}
	if (gl_smoothmodels.value) {
		GL_ShadeModel(GL_SMOOTH);
	}
}

void GL_EndDrawAliasModels(void)
{
	if (GL_ShadersSupported()) {
		if (batch_count) {
			GLM_FlushAliasModelBatch();
		}

		if (in_batch_mode && !first_alias_model) {
			GL_LeaveRegion();
		}
		in_batch_mode = false;
	}
}

void GLM_AliasModelShadow(entity_t* ent, aliashdr_t* paliashdr, vec3_t shadevector, vec3_t lightspot)
{
	// MEAG: TODO
}

void GLM_DrawAliasModelFrame(
	model_t* model, int poseVertIndex, int poseVertIndex2, int vertsPerPose,
	qbool scrolldir, texture_ref texture, texture_ref fb_texture,
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
		scrolldir, texture, fb_texture, outline
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

	GL_ConfigureVertexAttribPointer(&aliasModel_vao, aliasModelVBO, 0, 3, GL_FLOAT, GL_FALSE, sizeof(vbo_model_vert_t), VBO_ALIASVERT_FOFS(position));
	GL_ConfigureVertexAttribPointer(&aliasModel_vao, aliasModelVBO, 1, 2, GL_FLOAT, GL_FALSE, sizeof(vbo_model_vert_t), VBO_ALIASVERT_FOFS(texture_coords));
	GL_ConfigureVertexAttribPointer(&aliasModel_vao, aliasModelVBO, 2, 3, GL_FLOAT, GL_FALSE, sizeof(vbo_model_vert_t), VBO_ALIASVERT_FOFS(normal));
	GL_ConfigureVertexAttribIPointer(&aliasModel_vao, instanceVBO, 3, 1, GL_UNSIGNED_INT, sizeof(GLuint), 0);
	GL_ConfigureVertexAttribIPointer(&aliasModel_vao, aliasModelVBO, 4, 1, GL_INT, sizeof(vbo_model_vert_t), VBO_ALIASVERT_FOFS(vert_index));

	glVertexAttribDivisor(3, 1);
}
