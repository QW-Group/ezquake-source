// Alias model rendering (modern)

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "gl_aliasmodel.h"

static qbool first_alias_model = true;
extern glm_vao_t aliasModel_vao;

typedef struct block_aliasmodels_s {
	float modelViewMatrix[32][16];
	float color[32][4];
	float scale[32][4];            // indexes 2&3 are padding
	int textureIndex[32][4];       // indexes 1,2,3 are padding
	int apply_texture[32][4];
	int shellMode[32][4];
	float yaw_angle_rad[32][4];
	float shadelight[32][4];
	float ambientlight[32][4];

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

#define ALIAS_UBO_FOFS(x) (void*)((intptr_t)&(((block_aliasmodels_t*)0)->x))

static glm_program_t drawAliasModelProgram;
static GLuint drawAliasModel_RefdefCvars_block;
static GLuint drawAliasModel_AliasData_block;
static block_aliasmodels_t aliasdata;
static glm_ubo_t ubo_aliasdata;

typedef struct glm_aliasmodelbatch_s {
	int start;
	int end;

	qbool texture2d;
	qbool shells;
	int skin_texture;
	GLuint array_texture;
} glm_aliasmodelbatch_t;

static void GLM_QueueAliasModelDraw(model_t* model, byte* color, int start, int count, qbool texture, GLuint texture_index, float scaleS, float scaleT, int effects, qbool is_texture_array, qbool shell_only, float yaw_angle_radians, float shadelight, float ambientlight);

typedef struct glm_aliasmodel_req_s {
	GLuint vbo_count;
	GLuint instanceCount;
	GLuint vbo_start;
	GLuint baseInstance;
	float mvMatrix[16];
	float texScale[2];
	GLuint texture_array;
	int texture_index;
	byte color[4];
	int texture_model;
	int effects;
	qbool is_texture_array;
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

#define MAX_ALIASMODEL_BATCH 32
static glm_aliasmodel_req_t aliasmodel_requests[MAX_ALIASMODEL_BATCH];
static int batch_count = 0;
static GLuint prev_texture_array = 0;
static qbool in_batch_mode = false;

static void GLM_CompileAliasModelProgram(void)
{
	if (!drawAliasModelProgram.program) {
		GL_VFDeclare(model_alias);

		// Initialise program for drawing image
		GLM_CreateVFProgram("AliasModel", GL_VFParams(model_alias), &drawAliasModelProgram);
	}

	if (drawAliasModelProgram.program && !drawAliasModelProgram.uniforms_found) {
		GLint size;

		drawAliasModel_RefdefCvars_block = glGetUniformBlockIndex(drawAliasModelProgram.program, "RefdefCvars");
		drawAliasModel_AliasData_block = glGetUniformBlockIndex(drawAliasModelProgram.program, "AliasModelData");

		glGetActiveUniformBlockiv(drawAliasModelProgram.program, drawAliasModel_AliasData_block, GL_UNIFORM_BLOCK_DATA_SIZE, &size);

		glUniformBlockBinding(drawAliasModelProgram.program, drawAliasModel_RefdefCvars_block, GL_BINDINGPOINT_REFDEF_CVARS);
		glUniformBlockBinding(drawAliasModelProgram.program, drawAliasModel_AliasData_block, GL_BINDINGPOINT_ALIASMODEL_CVARS);

		GL_GenUniformBuffer(&ubo_aliasdata, "alias-data", &aliasdata, sizeof(aliasdata));
		glBindBufferBase(GL_UNIFORM_BUFFER, GL_BINDINGPOINT_ALIASMODEL_CVARS, ubo_aliasdata.ubo);

		drawAliasModelProgram.uniforms_found = true;
	}
}

// Drawing single frame from an alias model (no lerping)
void GLM_DrawSimpleAliasFrame(model_t* model, aliashdr_t* paliashdr, int pose1, qbool scrolldir, GLuint texture, GLuint fb_texture, GLuint textureEnvMode, float scaleS, float scaleT, int effects, qbool is_texture_array, qbool shell_only)
{
	int vertIndex = paliashdr->vertsOffset + pose1 * paliashdr->vertsPerPose;
	byte color[4];
	qbool texture_model = (custom_model == NULL);

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

		texture_model = (custom_model->fullbright_cvar.integer == 0);
	}

	GLM_QueueAliasModelDraw(model, color, vertIndex, paliashdr->vertsPerPose, texture_model, texture, scaleS, scaleT, effects, is_texture_array, shell_only, currententity->angles[YAW] * M_PI / 180.0, shadelight, ambientlight);
}

static void GLM_AliasModelBatchDraw(int start, int end)
{
	int i;

	// FIXME: Should be glDrawArraysIndirect
	for (i = start; i <= end; ++i) {
		const DrawArraysIndirectCommand_t *cmd = (const DrawArraysIndirectCommand_t *)((byte*)aliasmodel_requests + i * sizeof(aliasmodel_requests[0]));

		glDrawArraysInstancedBaseInstance(GL_TRIANGLES, cmd->first, cmd->count, cmd->instanceCount, cmd->baseInstance);
	}
}

static void GLM_FlushAliasModelBatch(void)
{
	int i;
	glm_aliasmodelbatch_t batches[MAX_ALIASMODEL_BATCH];
	qbool was_texture_array = false;
	qbool was_shells = false;
	int non_texture_array = -1;
	int batch = 0;

	memset(&aliasdata, 0, sizeof(aliasdata));

	if (in_batch_mode && first_alias_model) {
		if (GL_ShadersSupported()) {
			GL_EnterRegion("AliasModels");

			GLM_CompileAliasModelProgram();

			GL_SelectTexture(GL_TEXTURE0);
			prev_texture_array = 0;
			first_alias_model = false;
		}
	}

	GLM_CompileAliasModelProgram();
	GL_UseProgram(drawAliasModelProgram.program);
	GL_BindVertexArray(aliasModel_vao.vao);

	prev_texture_array = 0;
	batches[batch].start = 0;
	for (i = 0; i < batch_count; ++i) {
		glm_aliasmodel_req_t* req = &aliasmodel_requests[i];
		qbool array_switch = (req->is_texture_array && prev_texture_array != req->texture_array);
		qbool is_shells = req->effects != 0;
		qbool non_array_switch = (!req->is_texture_array && non_texture_array != req->texture_index);
		qbool texture_mode_switch = (prev_texture_array && array_switch) || (non_texture_array != -1 && non_array_switch);
		qbool shell_mode_switch = (was_shells != is_shells);

		if (texture_mode_switch || non_array_switch || array_switch || shell_mode_switch) {
			if (i != 0) {
				++batch;
			}

			batches[batch].start = i;
			batches[batch].texture2d = !req->is_texture_array;
			was_shells = batches[batch].shells = is_shells;
			if (batches[batch].texture2d) {
				non_texture_array = batches[batch].skin_texture = req->texture_index;
			}
			else {
				prev_texture_array = batches[batch].array_texture = req->texture_array;
			}
		}

		memcpy(&aliasdata.modelViewMatrix[i][0], req->mvMatrix, sizeof(aliasdata.modelViewMatrix[i]));
		aliasdata.color[i][0] = req->color[0] * 1.0f / 255;
		aliasdata.color[i][1] = req->color[1] * 1.0f / 255;
		aliasdata.color[i][2] = req->color[2] * 1.0f / 255;
		aliasdata.color[i][3] = req->color[3] * 1.0f / 255;
		aliasdata.scale[i][0] = req->texScale[0];
		aliasdata.scale[i][1] = req->texScale[1];
		aliasdata.textureIndex[i][0] = req->texture_index;
		aliasdata.apply_texture[i][0] = req->is_texture_array ? req->texture_model : (req->texture_model ? 2 : 0);
		aliasdata.shellMode[i][0] = req->effects;
		aliasdata.ambientlight[i][0] = req->ambientlight;
		aliasdata.shadelight[i][0] = req->shadelight;
		aliasdata.yaw_angle_rad[i][0] = req->yaw_angle_radians;
		batches[batch].end = i;

		aliasmodel_requests[i].baseInstance = i;
		aliasmodel_requests[i].instanceCount = 1;
	}

	aliasdata.shellSize = bound(0, gl_powerupshells_size.value, 20);
	aliasdata.shell_base_level1 = gl_powerupshells_base1level.value;
	aliasdata.shell_base_level2 = gl_powerupshells_base2level.value;
	aliasdata.shell_effect_level1 = gl_powerupshells_effect1level.value;
	aliasdata.shell_effect_level2 = gl_powerupshells_effect2level.value;
	aliasdata.shell_alpha = bound(0, gl_powerupshells.value, 1);

	// Update data
	GL_UpdateUBO(&ubo_aliasdata, sizeof(aliasdata), &aliasdata);

	for (i = 0; i <= batch; ++i) {
		if (batches[i].texture2d) {
			GL_SelectTexture(GL_TEXTURE0 + 1);
			GL_Bind(batches[i].skin_texture);
		}
		else {
			GL_SelectTexture(GL_TEXTURE0);
			GL_BindTexture(GL_TEXTURE_2D_ARRAY, batches[i].array_texture, true);
		}

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

		GLM_AliasModelBatchDraw(batches[i].start, batches[i].end);
	}
	batch_count = 0;
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

static void GLM_QueueAliasModelDrawImpl(model_t* model, byte* color, int start, int count, qbool texture, GLuint texture_index, float scaleS, float scaleT, int effects, qbool is_texture_array, float yaw_angle_radians, float shadelight, float ambientlight)
{
	glm_aliasmodel_req_t* req;

	if (batch_count >= MAX_ALIASMODEL_BATCH) {
		GLM_FlushAliasModelBatch();
	}

	if (is_texture_array && !model->texture_arrays[0]) {
		// Texture array set but no value
		Con_Printf("Model %s has no texture array\n", model->name);
		return;
	}

	req = &aliasmodel_requests[batch_count];
	GL_GetMatrix(GL_MODELVIEW, req->mvMatrix);
	// TODO: angles
	req->vbo_start = start;
	req->vbo_count = count;
	req->texScale[0] = is_texture_array ? scaleS : 1.0f;
	req->texScale[1] = is_texture_array ? scaleT : 1.0f;
	req->texture_model = texture;
	req->texture_array = model->texture_arrays[0];
	req->texture_index = texture_index;
	req->instanceCount = 1;
	req->effects = effects;
	req->is_texture_array = is_texture_array;
	req->yaw_angle_radians = yaw_angle_radians;
	req->shadelight = shadelight;
	req->ambientlight = ambientlight;
	memcpy(req->color, color, 4);
	++batch_count;
}

static void GLM_QueueAliasModelDraw(model_t* model, byte* color, int start, int count, qbool texture, GLuint texture_index, float scaleS, float scaleT, int effects, qbool is_texture_array, qbool shell_only, float yaw_angle_radians, float shadelight, float ambientlight)
{
	if (shell_only) {
		if (!shelltexture) {
			return;
		}
		if (effects) {
			// always allow powerupshells for specs or demos.
			// do not allow powerupshells for eyes in other cases
			if (bound(0, gl_powerupshells.value, 1) && ((cls.demoplayback || cl.spectator) || model->modhint != MOD_EYES)) {
				effects &= (EF_RED | EF_GREEN | EF_BLUE);

				if (effects) {
					GLM_QueueAliasModelDrawImpl(model, color_white, start, count, true, shelltexture, 1, 1, effects, false, yaw_angle_radians, shadelight, ambientlight);
					if (!in_batch_mode) {
						GLM_FlushAliasModelBatch();
					}
				}
			}
		}
	}
	else {
		GLM_QueueAliasModelDrawImpl(model, color, start, count, texture, texture_index, scaleS, scaleT, 0, is_texture_array, yaw_angle_radians, shadelight, ambientlight);
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

