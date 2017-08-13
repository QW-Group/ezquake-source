// Alias model rendering (modern)

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "gl_aliasmodel.h"

static glm_program_t drawShellPolyProgram;
static GLint drawShell_modelViewMatrix;
static GLint drawShell_projectionMatrix;
static GLint drawShell_shellSize;
static GLint drawShell_color;
static GLint drawShell_materialTex;
static GLint drawShell_time;

static glm_program_t drawAliasModelProgram;
static GLint drawAliasModel_modelViewMatrix;
static GLint drawAliasModel_projectionMatrix;
static GLint drawAliasModel_color;
static GLint drawAliasModel_materialTex;
static GLint drawAliasModel_applyTexture;
static GLint drawAliasModel_textureIndex;
static GLint drawAliasModel_scaleS;
static GLint drawAliasModel_scaleT;

int GL_GenerateShellTexture(void);

static void GLM_QueueAliasModelDraw(model_t* model, GLuint vao, byte* color, int start, int count, qbool texture, GLuint texture_index, float scaleS, float scaleT);

void GLM_DrawAliasModel(model_t* model, GLuint vao, byte* color, int start, int count, qbool texture, GLuint texture_index, float scaleS, float scaleT)
{
	if (drawAliasModelProgram.program && vao) {
		float modelViewMatrix[16];
		float projectionMatrix[16];

		GLM_GetMatrix(GL_MODELVIEW, modelViewMatrix);
		GLM_GetMatrix(GL_PROJECTION, projectionMatrix);

		GL_UseProgram(drawAliasModelProgram.program);
		glUniformMatrix4fv(drawAliasModel_modelViewMatrix, 1, GL_FALSE, modelViewMatrix);
		glUniformMatrix4fv(drawAliasModel_projectionMatrix, 1, GL_FALSE, projectionMatrix);
		glUniform4f(drawAliasModel_color, color[0] * 1.0f / 255, color[1] * 1.0f / 255, color[2] * 1.0f / 255, color[3] * 1.0f / 255);
		glUniform1i(drawAliasModel_materialTex, 0);
		glUniform1f(drawAliasModel_textureIndex, texture_index);
		glUniform1i(drawAliasModel_applyTexture, texture);
		glUniform1f(drawAliasModel_scaleS, scaleS);
		glUniform1f(drawAliasModel_scaleT, scaleT);

		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLE_STRIP, start, count);
	}
}

// Shell adds normals
void GLM_DrawShellPoly(GLenum type, byte* color, float shellSize, unsigned int vao, int start, int vertices)
{
	if (!drawShellPolyProgram.program) {
		GL_VFDeclare(model_shell);

		// Initialise program for drawing image
		GLM_CreateVFProgram("DrawShell", GL_VFParams(model_shell), &drawShellPolyProgram);

		drawShell_modelViewMatrix = glGetUniformLocation(drawShellPolyProgram.program, "modelViewMatrix");
		drawShell_projectionMatrix = glGetUniformLocation(drawShellPolyProgram.program, "projectionMatrix");
		drawShell_shellSize = glGetUniformLocation(drawShellPolyProgram.program, "shellSize");
		drawShell_color = glGetUniformLocation(drawShellPolyProgram.program, "color");
		drawShell_materialTex = glGetUniformLocation(drawShellPolyProgram.program, "materialTex");
		drawShell_time = glGetUniformLocation(drawShellPolyProgram.program, "time");
	}

	if (drawShellPolyProgram.program && vao) {
		float modelViewMatrix[16];
		float projectionMatrix[16];

		GLM_GetMatrix(GL_MODELVIEW, modelViewMatrix);
		GLM_GetMatrix(GL_PROJECTION, projectionMatrix);

		GL_UseProgram(drawShellPolyProgram.program);
		glUniformMatrix4fv(drawShell_modelViewMatrix, 1, GL_FALSE, modelViewMatrix);
		glUniformMatrix4fv(drawShell_projectionMatrix, 1, GL_FALSE, projectionMatrix);
		glUniform1f(drawShell_shellSize, shellSize);
		glUniform4f(drawShell_color, color[0] * 1.0f / 255, color[1] * 1.0f / 255, color[2] * 1.0f / 255, color[3] * 1.0f / 255);
		glUniform1i(drawShell_materialTex, 0);
		glUniform1f(drawShell_time, cl.time);

		glBindVertexArray(vao);
		glDrawArrays(type, start, vertices);
	}
}

static void GLM_DrawPowerupShell(aliashdr_t* paliashdr, int pose, trivertx_t* verts1, trivertx_t* verts2, float lerpfrac, qbool scrolldir)
{
	int *order, count;
	float scroll[2];
	float shell_size = bound(0, gl_powerupshells_size.value, 20);
	byte color[4];
	int vertIndex = paliashdr->vertsOffset + pose * paliashdr->vertsPerPose;

	// LordHavoc: set the state to what we need for rendering a shell
	if (!shelltexture) {
		shelltexture = GL_GenerateShellTexture();
	}
	GL_Bind(shelltexture);
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);

	if (gl_powerupshells_style.integer) {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	}
	else {
		glBlendFunc(GL_ONE, GL_ONE);
	}

	if (scrolldir) {
		scroll[0] = cos(cl.time * -0.5); // FIXME: cl.time ????
		scroll[1] = sin(cl.time * -0.5);
	}
	else {
		scroll[0] = cos(cl.time * 1.5);
		scroll[1] = sin(cl.time * 1.1);
	}

	color[0] = r_shellcolor[0] * 255;
	color[1] = r_shellcolor[1] * 255;
	color[2] = r_shellcolor[2] * 255;
	color[3] = bound(0, gl_powerupshells.value, 1) * 255;

	// get the vertex count and primitive type
	order = (int *)((byte *)paliashdr + paliashdr->commands);
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

		order += 2 * count;

		GLM_DrawShellPoly(drawMode, color, shell_size, paliashdr->vao, vertIndex, count);

		vertIndex += count;
	}

	// LordHavoc: reset the state to what the rest of the renderer expects
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// Drawing single frame from an alias model (no lerping)
void GLM_DrawSimpleAliasFrame(model_t* model, aliashdr_t* paliashdr, int pose1, qbool scrolldir, GLuint texture, GLuint fb_texture, GLuint textureEnvMode, float scaleS, float scaleT)
{
	int vertIndex = paliashdr->vertsOffset + pose1 * paliashdr->vertsPerPose;
	byte color[4];
	float l;
	qbool texture_model = (custom_model == NULL);

	if (r_shellcolor[0] || r_shellcolor[1] || r_shellcolor[2]) {
		GLM_DrawPowerupShell(paliashdr, pose1, NULL, NULL, 0.0f, false);
		return;
	}

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

	if (paliashdr->vao) {
		int* order = (int *) ((byte *) paliashdr + paliashdr->commands);
		int count;

		while ((count = *order++)) {
			GLenum drawMode = GL_TRIANGLE_STRIP;

			if (count < 0) {
				count = -count;
				drawMode = GL_TRIANGLE_FAN;
			}

			// texture coordinates now stored in the VBO
			order += 2 * count;

			// TODO: model lerping between frames
			// TODO: Vertex lighting etc
			// TODO: shadedots[vert.l] ... need to copy shadedots to program somehow
			// TODO: Coloured lighting per-vertex?
			l = shadedots[0] / 127.0;
			l = (l * shadelight + ambientlight) / 256.0;
			l = min(l, 1);

			if (custom_model == NULL) {
				if (r_modelcolor[0] < 0) {
					// normal color
					color[0] = color[1] = color[2] = l * 255;
				}
				else {
					// forced
					color[0] *= l;
					color[1] *= l;
					color[2] *= l;
				}
			}
			else {
				color[0] = custom_model->color_cvar.color[0];
				color[1] = custom_model->color_cvar.color[1];
				color[2] = custom_model->color_cvar.color[2];
			}

			GLM_QueueAliasModelDraw(model, paliashdr->vao, color, vertIndex, count, texture_model, texture, scaleS, scaleT);

			vertIndex += count;
		}
	}
}

typedef struct glm_aliasmodel_req_s {
	GLuint vbo_count;
	GLuint instanceCount;
	GLuint vbo_start;
	GLuint baseInstance;
	float mvMatrix[16];
	float texScale[2];
	GLuint vao;
	GLuint texture_array;
	int texture_index;
	byte color[4];
	int texture_model;
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

static void GLM_FlushAliasModelBatch(void)
{
	int i;
	float projectionMatrix[16];

	float mvMatrix[MAX_ALIASMODEL_BATCH][16];
	float colors[MAX_ALIASMODEL_BATCH][4];
	float texScaleS[MAX_ALIASMODEL_BATCH];
	float texScaleT[MAX_ALIASMODEL_BATCH];
	float texture_indexes[MAX_ALIASMODEL_BATCH];
	int texture_models[MAX_ALIASMODEL_BATCH];

	GLM_GetMatrix(GL_PROJECTION, projectionMatrix);

	GL_UseProgram(drawAliasModelProgram.program);
	glUniformMatrix4fv(drawAliasModel_projectionMatrix, 1, GL_FALSE, projectionMatrix);
	glUniform1i(drawAliasModel_materialTex, 0);

	for (i = 0; i < batch_count; ++i) {
		glm_aliasmodel_req_t* req = &aliasmodel_requests[i];

		if (req->texture_array != prev_texture_array) {
			glBindTexture(GL_TEXTURE_2D_ARRAY, req->texture_array);
			prev_texture_array = req->texture_array;
		}

		if (i == 0) {
			glBindVertexArray(req->vao);
		}

		memcpy(&mvMatrix[i], req->mvMatrix, sizeof(mvMatrix[i]));
		colors[i][0] = req->color[0] * 1.0f / 255;
		colors[i][1] = req->color[1] * 1.0f / 255;
		colors[i][2] = req->color[2] * 1.0f / 255;
		colors[i][3] = req->color[3] * 1.0f / 255;
		texture_indexes[i] = req->texture_index;
		texScaleS[i] = req->texScale[0];
		texScaleT[i] = req->texScale[1];
		texture_models[i] = req->texture_model;

		aliasmodel_requests[i].baseInstance = i;
		aliasmodel_requests[i].instanceCount = 1;

		if (req->texture_array != prev_texture_array) {
			glBindTexture(GL_TEXTURE_2D_ARRAY, req->texture_array);
			prev_texture_array = req->texture_array;
		}
	}

	glUniformMatrix4fv(drawAliasModel_modelViewMatrix, batch_count, GL_FALSE, (const GLfloat*) mvMatrix);
	glUniform4fv(drawAliasModel_color, batch_count, (const GLfloat*) colors);
	glUniform1fv(drawAliasModel_scaleS, batch_count, texScaleS);
	glUniform1fv(drawAliasModel_scaleT, batch_count, texScaleT);
	glUniform1fv(drawAliasModel_textureIndex, batch_count, texture_indexes);
	glUniform1iv(drawAliasModel_applyTexture, batch_count, texture_models);

	for (i = 0; i < batch_count; ++i) {
		const DrawArraysIndirectCommand_t *cmd = (const DrawArraysIndirectCommand_t *)((byte*)aliasmodel_requests + i * sizeof(aliasmodel_requests[0]));

		glDrawArraysInstancedBaseInstance(GL_TRIANGLE_STRIP, cmd->first, cmd->count, cmd->instanceCount, cmd->baseInstance);
	}

	batch_count = 0;
}

static void GLM_QueueAliasModelDraw(model_t* model, GLuint vao, byte* color, int start, int count, qbool texture, GLuint texture_index, float scaleS, float scaleT)
{
	glm_aliasmodel_req_t* req;

	if (batch_count >= MAX_ALIASMODEL_BATCH) {
		GLM_FlushAliasModelBatch();
	}

	if (!vao) {
		return;
	}

	req = &aliasmodel_requests[batch_count];
	GL_GetMatrix(GL_MODELVIEW, req->mvMatrix);
	// TODO: angles
	req->vbo_start = start;
	req->vbo_count = count;
	req->texScale[0] = scaleS;
	req->texScale[1] = scaleT;
	req->texture_model = texture;
	req->vao = vao;
	req->texture_array = model->texture_arrays[0];
	req->texture_index = texture_index;
	memcpy(req->color, color, 4);
	++batch_count;

	//GLM_FlushAliasModelBatch();
}

void GL_BeginDrawAliasModels(void)
{
	batch_count = 0;

	if (!drawAliasModelProgram.program) {
		GL_VFDeclare(model_alias);

		// Initialise program for drawing image
		GLM_CreateVFProgram("AliasModel", GL_VFParams(model_alias), &drawAliasModelProgram);

		drawAliasModel_modelViewMatrix = glGetUniformLocation(drawAliasModelProgram.program, "modelViewMatrix");
		drawAliasModel_projectionMatrix = glGetUniformLocation(drawAliasModelProgram.program, "projectionMatrix");
		drawAliasModel_color = glGetUniformLocation(drawAliasModelProgram.program, "color");
		drawAliasModel_materialTex = glGetUniformLocation(drawAliasModelProgram.program, "materialTex");
		drawAliasModel_applyTexture = glGetUniformLocation(drawAliasModelProgram.program, "apply_texture");
		drawAliasModel_textureIndex = glGetUniformLocation(drawAliasModelProgram.program, "textureIndex");
		drawAliasModel_scaleS = glGetUniformLocation(drawAliasModelProgram.program, "scaleS");
		drawAliasModel_scaleT = glGetUniformLocation(drawAliasModelProgram.program, "scaleT");
	}

	glActiveTexture(GL_TEXTURE0);
	prev_texture_array = 0;
	glDisable(GL_CULL_FACE);
}

void GL_EndDrawAliasModels(void)
{
	if (batch_count) {
		GLM_FlushAliasModelBatch();
	}
	glEnable(GL_CULL_FACE);
}
