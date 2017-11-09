
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"

// For drawing sprites in 3D space
// Always facing camera, send as point

static glm_program_t spriteProgram;
static GLint sprite_modelViewMatrixUniform;
static GLint sprite_projectionMatrixUniform;
static GLint sprite_materialTexUniform;
static GLint sprite_textureIndex;
static GLint sprite_texS;
static GLint sprite_texT;
static GLint sprite_scale;
static GLint sprite_origin;

typedef struct glm_sprite_s {
	vec3_t origin;
	float scale;
	float texScale[2];
	GLuint vao;
	GLuint texture_array;
	int texture_index;
} glm_sprite_t;

#define MAX_SPRITE_BATCH 32
static glm_sprite_t sprite_batch[MAX_SPRITE_BATCH];
static int batch_count = 0;

// Temporary
static glm_vbo_t simpleItemVBO;
static glm_vao_t simpleItemVAO;
static void GL_CompileSpriteProgram(void);

static void GL_PrepareSprites(void)
{
	GL_CompileSpriteProgram();

	if (!simpleItemVBO.vbo) {
		float verts[4][VERTEXSIZE] = { { 0 } };

		VectorSet(verts[0], 0, -1, -1);
		verts[0][3] = 1;
		verts[0][4] = 1;

		VectorSet(verts[1], 0, -1, 1);
		verts[1][3] = 1;
		verts[1][4] = 0;

		VectorSet(verts[2], 0, 1, 1);
		verts[2][3] = 0;
		verts[2][4] = 0;

		VectorSet(verts[3], 0, 1, -1);
		verts[3][3] = 0;
		verts[3][4] = 1;

		GL_GenBuffer(&simpleItemVBO, __FUNCTION__);
		glBindBufferExt(GL_ARRAY_BUFFER, simpleItemVBO.vbo);
		glBufferDataExt(GL_ARRAY_BUFFER, 4 * VERTEXSIZE * sizeof(float), verts, GL_STATIC_DRAW);
	}

	if (!simpleItemVAO.vao) {
		GL_GenVertexArray(&simpleItemVAO);
		GL_BindVertexArray(simpleItemVAO.vao);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glBindBufferExt(GL_ARRAY_BUFFER, simpleItemVBO.vbo);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * VERTEXSIZE, (void*) 0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * VERTEXSIZE, (void*) (sizeof(float) * 3));
	}
}

static GLuint prev_texture_array = -1;

void GL_BeginDrawSprites(void)
{
	batch_count = 0;

	if (GL_ShadersSupported()) {
		GL_PrepareSprites();

		GL_SelectTexture(GL_TEXTURE0);
		prev_texture_array = -1;
	}
}

void GL_FlushSpriteBatch(void)
{
	int i;
	float oldMatrix[16];
	float projectionMatrix[16];
	GLuint vao = 0;

	// hideous but hopefully temporary
	float mvMatrix[MAX_SPRITE_BATCH][16];
	float texScaleS[MAX_SPRITE_BATCH];
	float texScaleT[MAX_SPRITE_BATCH];
	float texture_indexes[MAX_SPRITE_BATCH];

	glDisable(GL_CULL_FACE);
	GL_PushMatrix(GL_MODELVIEW, oldMatrix);

	GLM_GetMatrix(GL_PROJECTION, projectionMatrix);

	for (i = 0; i < batch_count; ++i) {
		glm_sprite_t* sprite = &sprite_batch[i];
		if (sprite->texture_array != prev_texture_array) {
			GL_BindTexture(GL_TEXTURE_2D_ARRAY, sprite->texture_array, true);
			prev_texture_array = sprite->texture_array;
		}

		vao = sprite->vao;

		GL_PopMatrix(GL_MODELVIEW, r_world_matrix);
		GL_Translate(GL_MODELVIEW, sprite->origin[0], sprite->origin[1], sprite->origin[2]);
		if (true)
		{
			float* tempMatrix = mvMatrix[i];

			GL_PushMatrix(GL_MODELVIEW, tempMatrix);
			// x = -y
			tempMatrix[0] = 0;
			tempMatrix[4] = -sprite->scale;
			tempMatrix[8] = 0;

			// y = z
			tempMatrix[1] = 0;
			tempMatrix[5] = 0;
			tempMatrix[9] = sprite->scale;

			// z = -x
			tempMatrix[2] = -sprite->scale;
			tempMatrix[6] = 0;
			tempMatrix[10] = 0;

			GL_PopMatrix(GL_MODELVIEW, tempMatrix);
		}

		texScaleS[i] = sprite->texScale[0];
		texScaleT[i] = sprite->texScale[1];

		texture_indexes[i] = sprite->texture_index;
	}

	GL_UseProgram(spriteProgram.program);
	glUniformMatrix4fv(sprite_projectionMatrixUniform, 1, GL_FALSE, projectionMatrix);
	glUniform1i(sprite_materialTexUniform, 0);
	glUniformMatrix4fv(sprite_modelViewMatrixUniform, batch_count, GL_FALSE, (const GLfloat*) mvMatrix);
	glUniform1fv(sprite_textureIndex, batch_count, texture_indexes);
	glUniform1fv(sprite_texS, batch_count, texScaleS);
	glUniform1fv(sprite_texT, batch_count, texScaleT);

	GL_BindVertexArray(vao);
	glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, batch_count);

	GL_PopMatrix(GL_MODELVIEW, oldMatrix);
	glEnable(GL_CULL_FACE);

	batch_count = 0;
}

void GL_EndDrawSprites(void)
{
	if (GL_ShadersSupported()) {
		if (batch_count) {
			GL_FlushSpriteBatch();
		}
	}
}

static void GL_CompileSpriteProgram(void)
{
	if (!spriteProgram.program) {
		GL_VFDeclare(sprite);

		// Initialise program for drawing image
		GLM_CreateVFProgram("Sprites", GL_VFParams(sprite), &spriteProgram);
	}

	if (spriteProgram.program && !spriteProgram.uniforms_found) {
		sprite_modelViewMatrixUniform = glGetUniformLocation(spriteProgram.program, "modelView");
		sprite_projectionMatrixUniform = glGetUniformLocation(spriteProgram.program, "projection");
		sprite_materialTexUniform = glGetUniformLocation(spriteProgram.program, "materialTex");
		sprite_scale = glGetUniformLocation(spriteProgram.program, "scale");
		sprite_origin = glGetUniformLocation(spriteProgram.program, "origin");
		sprite_textureIndex = glGetUniformLocation(spriteProgram.program, "skinNumber");
		sprite_texS = glGetUniformLocation(spriteProgram.program, "texS");
		sprite_texT = glGetUniformLocation(spriteProgram.program, "texT");
		spriteProgram.uniforms_found = true;
	}
}

void GL_EndDrawEntities(void)
{
}

void GLM_DrawSimpleItem(model_t* model, int texture, vec3_t origin, vec3_t angles, float scale, float scale_s, float scale_t)
{
	glm_sprite_t* sprite;

	if (batch_count >= MAX_SPRITE_BATCH) {
		GL_FlushSpriteBatch();
	}

	sprite = &sprite_batch[batch_count];
	VectorCopy(origin, sprite->origin);
	// TODO: angles
	sprite->scale = scale;
	sprite->texScale[0] = scale_s;
	sprite->texScale[1] = scale_t;
	sprite->texture_array = model->simpletexture_array;
	sprite->texture_index = texture;
	sprite->vao = model->vao_simple.vao;
	++batch_count;
}

void GLM_DrawSpriteModel(entity_t* e)
{
	vec3_t right, up;
	mspriteframe_t *frame;
	msprite2_t *psprite;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	psprite = (msprite2_t*)Mod_Extradata(e->model);	//locate the proper data
	frame = R_GetSpriteFrame(e, psprite);

	if (!frame) {
		return;
	}

	if (psprite->type == SPR_ORIENTED) {
		// bullet marks on walls
		AngleVectors(e->angles, NULL, right, up);
	}
	else if (psprite->type == SPR_FACING_UPRIGHT) {
		VectorSet(up, 0, 0, 1);
		right[0] = e->origin[1] - r_origin[1];
		right[1] = -(e->origin[0] - r_origin[0]);
		right[2] = 0;
		VectorNormalizeFast(right);
	}
	else if (psprite->type == SPR_VP_PARALLEL_UPRIGHT) {
		VectorSet(up, 0, 0, 1);
		VectorCopy(vright, right);
	}
	else {	// normal sprite
		VectorCopy(vup, up);
		VectorCopy(vright, right);
	}

	// MEAG: TODO: Angles
	{
		glm_sprite_t* sprite;

		if (batch_count >= MAX_SPRITE_BATCH) {
			GL_FlushSpriteBatch();
		}

		sprite = &sprite_batch[batch_count];
		VectorCopy(e->origin, sprite->origin);
		// TODO: angles
		sprite->scale = 5;
		sprite->texScale[0] = frame->gl_scalingS;
		sprite->texScale[1] = frame->gl_scalingT;
		sprite->texture_array = frame->gl_arraynum;
		sprite->texture_index = frame->gl_arrayindex;
		sprite->vao = e->model->vao_simple.vao;
		++batch_count;
	}
}

void GLM_DrawBillboard(ci_texture_t* _ptex, ci_player_t* _p, vec3_t _coord[4])
{
	// MEAG: TODO
}
