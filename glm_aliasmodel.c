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

int GL_GenerateShellTexture(void);

void GLM_DrawAliasModel(GLuint vao, byte* color, int start, int count, qbool texture)
{
	if (!drawAliasModelProgram.program) {
		GL_VFDeclare(model_alias);

		// Initialise program for drawing image
		GLM_CreateVFProgram("AliasModel", GL_VFParams(model_alias), &drawAliasModelProgram);

		drawAliasModel_modelViewMatrix = glGetUniformLocation(drawAliasModelProgram.program, "modelViewMatrix");
		drawAliasModel_projectionMatrix = glGetUniformLocation(drawAliasModelProgram.program, "projectionMatrix");
		drawAliasModel_color = glGetUniformLocation(drawAliasModelProgram.program, "color");
		drawAliasModel_materialTex = glGetUniformLocation(drawAliasModelProgram.program, "materialTex");
		drawAliasModel_applyTexture = glGetUniformLocation(drawAliasModelProgram.program, "apply_texture");
	}

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
		glUniform1i(drawAliasModel_applyTexture, texture);

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
void GLM_DrawSimpleAliasFrame(aliashdr_t* paliashdr, int pose1, qbool scrolldir)
{
	int vertIndex = paliashdr->vertsOffset + pose1 * paliashdr->vertsPerPose;
	byte color[4];
	float l;
	qbool texture = (custom_model == NULL);

	if (r_shellcolor[0] || r_shellcolor[1] || r_shellcolor[2]) {
		GLM_DrawPowerupShell(paliashdr, pose1, NULL, NULL, 0.0f, false);
		return;
	}

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

			GLM_DrawAliasModel(paliashdr->vao, color, vertIndex, count, texture);

			vertIndex += count;
		}
	}
}
