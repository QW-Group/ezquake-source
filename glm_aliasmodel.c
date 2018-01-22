// Alias model rendering (modern)

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "gl_aliasmodel.h"
#include "tr_types.h"

#define MAX_ALIASMODEL_BATCH 32
#define MAXIMUM_MATERIAL_SAMPLERS 32
#define ALIAS_UBO_FOFS(x) (void*)((intptr_t)&(((block_aliasmodels_t*)0)->x))

static qbool first_alias_model = true;
extern glm_vao_t aliasModel_vao;

typedef struct block_aliasmodels_s {
	float modelViewMatrix[MAX_ALIASMODEL_BATCH][16];
	float color[MAX_ALIASMODEL_BATCH][4];
	float scale[MAX_ALIASMODEL_BATCH][4];              // indexes 2&3 are padding
	int   apply_texture[MAX_ALIASMODEL_BATCH][4];
	int   shellMode[MAX_ALIASMODEL_BATCH][4];
	float yaw_angle_rad[MAX_ALIASMODEL_BATCH][4];
	float shadelight[MAX_ALIASMODEL_BATCH][4];
	float ambientlight[MAX_ALIASMODEL_BATCH][4];
	int   materialSamplerMapping[MAX_ALIASMODEL_BATCH][4];
	int   lumaSamplerMapping[MAX_ALIASMODEL_BATCH][4];

	float shellSize;
	// console var data
	float shell_base_level1;
	float shell_base_level2;
	float shell_effect_level1;
	float shell_effect_level2;
	float shell_alpha;
	// Total size must be multiple of vec4
	int padding[2];
} block_aliasmodels_t;

static glm_program_t drawAliasModelProgram;
static GLuint drawAliasModel_RefdefCvars_block;
static GLuint drawAliasModel_AliasData_block;
static block_aliasmodels_t aliasdata;
static glm_ubo_t ubo_aliasdata;

typedef struct glm_aliasmodelbatch_s {
	int start;
	int end;

	qbool shells;
	texture_ref skin_texture;
	texture_ref skin_texture_fb;
	texture_ref array_texture;
} glm_aliasmodelbatch_t;

static void GLM_QueueAliasModelDraw(
	model_t* model, byte* color, int start, int count,
	texture_ref texture, texture_ref fb_texture, float scaleS, float scaleT,
	int effects, qbool shell_only, float yaw_angle_radians, float shadelight, float ambientlight
);

typedef struct glm_aliasmodel_req_s {
	GLuint vbo_count;
	GLuint instanceCount;
	GLuint vbo_start;
	GLuint baseInstance;
	float mvMatrix[16];
	float texScale[2];
	int texture_skin_sampler;
	int texture_fb_sampler;
	byte color[4];
	int texture_model;
	int effects;
	float yaw_angle_radians;
	float shadelight;
	float ambientlight;
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

static glm_aliasmodel_req_t aliasmodel_requests[MAX_ALIASMODEL_BATCH];
static int batch_count = 0;
static qbool in_batch_mode = false;

static qbool GLM_CompileAliasModelProgram(void)
{
	if (!drawAliasModelProgram.program) {
		static char included_definitions[512];

		strlcpy(included_definitions, va("#define SAMPLER_COUNT %d\n", glConfig.texture_units), sizeof(included_definitions));
		strlcat(included_definitions, va("#define MAX_INSTANCEID %d\n", MAX_ALIASMODEL_BATCH), sizeof(included_definitions));

		GL_VFDeclare(model_alias);

		// Initialise program for drawing image
		GLM_CreateVFProgramWithInclude("AliasModel", GL_VFParams(model_alias), &drawAliasModelProgram, included_definitions);

		material_samplers_max = glConfig.texture_units;
	}

	if (drawAliasModelProgram.program && !drawAliasModelProgram.uniforms_found) {
		GLint size;

		drawAliasModel_RefdefCvars_block = glGetUniformBlockIndex(drawAliasModelProgram.program, "RefdefCvars");
		drawAliasModel_AliasData_block = glGetUniformBlockIndex(drawAliasModelProgram.program, "AliasModelData");

		glGetActiveUniformBlockiv(drawAliasModelProgram.program, drawAliasModel_AliasData_block, GL_UNIFORM_BLOCK_DATA_SIZE, &size);

		glUniformBlockBinding(drawAliasModelProgram.program, drawAliasModel_RefdefCvars_block, GL_BINDINGPOINT_REFDEF_CVARS);
		glUniformBlockBinding(drawAliasModelProgram.program, drawAliasModel_AliasData_block, GL_BINDINGPOINT_ALIASMODEL_CVARS);

		GL_GenUniformBuffer(&ubo_aliasdata, "alias-data", &aliasdata, sizeof(aliasdata));
		GL_BindUniformBufferBase(&ubo_aliasdata, GL_BINDINGPOINT_ALIASMODEL_CVARS, ubo_aliasdata.ubo);

		drawAliasModelProgram.uniforms_found = true;
	}

	return drawAliasModelProgram.program;
}

static void GLM_FlushAliasModelBatch(void)
{
	glm_aliasmodelbatch_t batches[MAX_ALIASMODEL_BATCH];
	qbool was_texture_array = false;
	qbool was_shells = false;
	int batch = 0;
	int i, j;

	memset(&aliasdata, 0, sizeof(aliasdata));

	if (in_batch_mode && first_alias_model) {
		if (GL_ShadersSupported()) {
			GL_EnterRegion("AliasModels");

			first_alias_model = false;
		}
	}

	aliasdata.shellSize = bound(0, gl_powerupshells_size.value, 20);
	aliasdata.shell_base_level1 = gl_powerupshells_base1level.value;
	aliasdata.shell_base_level2 = gl_powerupshells_base2level.value;
	aliasdata.shell_effect_level1 = gl_powerupshells_effect1level.value;
	aliasdata.shell_effect_level2 = gl_powerupshells_effect2level.value;
	aliasdata.shell_alpha = bound(0, gl_powerupshells.value, 1);

	// Bind textures
	for (i = 0; i < material_samplers; ++i) {
		GL_BindTextureUnit(GL_TEXTURE0 + i, GL_TEXTURE_2D, allocated_samplers[i]);
	}

	if (GLM_CompileAliasModelProgram()) {
		GL_UseProgram(drawAliasModelProgram.program);
		GL_BindVertexArray(&aliasModel_vao);

		batches[batch].start = 0;
		batches[batch].shells = false;
		for (i = 0; i < batch_count; ++i) {
			glm_aliasmodel_req_t* req = &aliasmodel_requests[i];
			qbool is_shells = req->effects != 0;
			qbool shell_mode_switch = (was_shells != is_shells);

			if (shell_mode_switch) {
				if (i != 0) {
					++batch;
				}

				batches[batch].start = i;
				was_shells = batches[batch].shells = is_shells;
			}

			memcpy(&aliasdata.modelViewMatrix[i][0], req->mvMatrix, sizeof(aliasdata.modelViewMatrix[i]));
			aliasdata.color[i][0] = req->color[0] * 1.0f / 255;
			aliasdata.color[i][1] = req->color[1] * 1.0f / 255;
			aliasdata.color[i][2] = req->color[2] * 1.0f / 255;
			aliasdata.color[i][3] = req->color[3] * 1.0f / 255;
			aliasdata.scale[i][0] = req->texScale[0];
			aliasdata.scale[i][1] = req->texScale[1];
			aliasdata.apply_texture[i][0] = req->texture_model;
			aliasdata.shellMode[i][0] = req->effects;
			aliasdata.ambientlight[i][0] = req->ambientlight;
			aliasdata.shadelight[i][0] = req->shadelight;
			aliasdata.yaw_angle_rad[i][0] = req->yaw_angle_radians;
			aliasdata.materialSamplerMapping[i][0] = req->texture_skin_sampler;
			aliasdata.lumaSamplerMapping[i][0] = req->texture_fb_sampler;
			batches[batch].end = i;

			aliasmodel_requests[i].baseInstance = i;
			aliasmodel_requests[i].instanceCount = 1;
		}

		// Update data
		GL_UpdateUBO(&ubo_aliasdata, sizeof(aliasdata), &aliasdata);

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

			// FIXME: Should be glDrawArraysIndirect
			for (j = batches[i].start; j <= batches[i].end; ++j) {
				const DrawArraysIndirectCommand_t *cmd = (const DrawArraysIndirectCommand_t *)((byte*)aliasmodel_requests + j * sizeof(aliasmodel_requests[0]));

				glDrawArraysInstancedBaseInstance(GL_TRIANGLES, cmd->first, cmd->count, cmd->instanceCount, cmd->baseInstance);
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

static void GLM_QueueAliasModelDrawImpl(model_t* model, byte* color, int start, int count, texture_ref texture, texture_ref fb_texture, float scaleS, float scaleT, int effects, float yaw_angle_radians, float shadelight, float ambientlight)
{
	glm_aliasmodel_req_t* req;
	int textureSampler = -1, lumaSampler = -1;

	if (batch_count >= MAX_ALIASMODEL_BATCH) {
		GLM_FlushAliasModelBatch();
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
	req->texScale[0] = 1.0f;
	req->texScale[1] = 1.0f;
	req->texture_model = (GL_TextureReferenceIsValid(texture) ? 1 : 0) + (GL_TextureReferenceIsValid(fb_texture) ? 2 : 0);
	req->texture_skin_sampler = textureSampler;
	req->texture_fb_sampler = lumaSampler;
	req->instanceCount = 1;
	req->effects = effects;
	req->yaw_angle_radians = yaw_angle_radians;
	req->shadelight = shadelight;
	req->ambientlight = ambientlight;
	memcpy(req->color, color, 4);
	++batch_count;
}

static void GLM_QueueAliasModelDraw(
	model_t* model, byte* color, int start, int count,
	texture_ref texture, texture_ref fb_texture, float scaleS, float scaleT,
	int effects, qbool shell_only, float yaw_angle_radians, float shadelight, float ambientlight
)
{
	qbool texture_model = GL_TextureReferenceIsValid(texture);

	if (shell_only) {
		if (!GL_TextureReferenceIsValid(shelltexture)) {
			return;
		}
		if (effects) {
			// always allow powerupshells for specs or demos.
			// do not allow powerupshells for eyes in other cases
			if (bound(0, gl_powerupshells.value, 1) && ((cls.demoplayback || cl.spectator) || model->modhint != MOD_EYES)) {
				effects &= (EF_RED | EF_GREEN | EF_BLUE);

				if (effects) {
					GLM_QueueAliasModelDrawImpl(model, color_white, start, count, shelltexture, null_texture_reference, 1, 1, effects, yaw_angle_radians, shadelight, ambientlight);
					if (!in_batch_mode) {
						GLM_FlushAliasModelBatch();
					}
				}
			}
		}
	}
	else {
		GLM_QueueAliasModelDrawImpl(model, color, start, count, texture, fb_texture, scaleS, scaleT, 0, yaw_angle_radians, shadelight, ambientlight);
		if (!in_batch_mode) {
			GLM_FlushAliasModelBatch();
		}
	}
}

void GL_BeginDrawAliasModels(void)
{
	first_alias_model = true;
	batch_count = 0;
	in_batch_mode = true;
	material_samplers = 0;

	if (!GL_ShadersSupported()) {
		// FIXME: Why is classic code in here?
		GL_EnableTMU(GL_TEXTURE0);
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

// Drawing single frame from an alias model (no lerping)
void GLM_DrawSimpleAliasFrame(
	model_t* model, aliashdr_t* paliashdr, int pose1, qbool scrolldir,
	texture_ref texture, texture_ref fb_texture, GLuint textureEnvMode,
	float scaleS, float scaleT, int effects, qbool shell_only
)
{
	int vertIndex = paliashdr->vertsOffset + pose1 * paliashdr->vertsPerPose;
	byte color[4];

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

	// TODO: model lerping between frames
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

	GLM_QueueAliasModelDraw(
		model, color, vertIndex, paliashdr->vertsPerPose,
		texture, fb_texture, scaleS, scaleT, effects, shell_only,
		currententity->angles[YAW] * M_PI / 180.0, shadelight, ambientlight
	);
}
