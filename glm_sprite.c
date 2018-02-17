
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "tr_types.h"

// For drawing sprites in 3D space
// Always facing camera, send as point

#define VBO_OFFSET(type, field) (void*)((intptr_t)&(((type*)0)->field))
#define MAX_SAMPLERS 32

static glm_program_t spriteProgram;
static GLuint spriteProgram_RefdefCvars_block;
static GLuint spriteProgram_SpriteData_block;

typedef struct glm_sprite_s {
	vec3_t origin;
	vec3_t up;
	vec3_t right;
	float texScale[2];
	int sampler;
	int texture_index;
} glm_sprite_t;

typedef struct glm_sprite_vert_s {
	vec3_t position;
	float tex[2];
	int sampler;
	float array_index;
	int padding;
} glm_sprite_vert_t;

static glm_sprite_t sprite_batch[MAX_SPRITE_BATCH];
static glm_sprite_vert_t sprite_vbo_data[MAX_SPRITE_BATCH * 4];
static texture_ref sampler_textures[MAX_SAMPLERS];
static int allocated_samplers = 0;
static int max_allocated_samplers;
static int batch_count = 0;

// Temporary
static glm_vao_t spriteVAO;
static buffer_ref spriteVBO;
static buffer_ref spriteIndexBuffer;
static void GL_CompileSpriteProgram(void);

static qbool first_sprite_draw = true;

static void GL_PrepareSprites(void)
{
	static GLushort sprite_index_data[MAX_SPRITE_BATCH * 5];

	GL_CompileSpriteProgram();

	if (!GL_BufferReferenceIsValid(spriteVBO)) {
		spriteVBO = GL_GenFixedBuffer(GL_ARRAY_BUFFER, "3d-sprites", sizeof(sprite_vbo_data), NULL, GL_STREAM_DRAW);
	}

	if (!GL_BufferReferenceIsValid(spriteIndexBuffer)) {
		int i;

		for (i = 0; i < MAX_SPRITE_BATCH; ++i) {
			sprite_index_data[i * 5 + 0] = i * 4;
			sprite_index_data[i * 5 + 1] = i * 4 + 1;
			sprite_index_data[i * 5 + 2] = i * 4 + 2;
			sprite_index_data[i * 5 + 3] = i * 4 + 3;
			sprite_index_data[i * 5 + 4] = ~(GLushort)0;
		}

		spriteIndexBuffer = GL_GenFixedBuffer(GL_ELEMENT_ARRAY_BUFFER, "3d-indexes", sizeof(sprite_index_data), sprite_index_data, GL_STATIC_DRAW);
	}

	if (!spriteVAO.vao) {
		GL_GenVertexArray(&spriteVAO, "sprite-vao");
		GL_SetVertexArrayElementBuffer(&spriteVAO, spriteIndexBuffer);

		GL_ConfigureVertexAttribPointer(&spriteVAO, spriteVBO, 0, 3, GL_FLOAT, GL_FALSE, sizeof(glm_sprite_vert_t), VBO_OFFSET(glm_sprite_vert_t, position));
		GL_ConfigureVertexAttribPointer(&spriteVAO, spriteVBO, 1, 2, GL_FLOAT, GL_FALSE, sizeof(glm_sprite_vert_t), VBO_OFFSET(glm_sprite_vert_t, tex));
		GL_ConfigureVertexAttribIPointer(&spriteVAO, spriteVBO, 2, 1, GL_INT, sizeof(glm_sprite_vert_t), VBO_OFFSET(glm_sprite_vert_t, sampler));
		GL_ConfigureVertexAttribPointer(&spriteVAO, spriteVBO, 3, 1, GL_FLOAT, GL_FALSE, sizeof(glm_sprite_vert_t), VBO_OFFSET(glm_sprite_vert_t, array_index));
	}
}

void GL_BeginDrawSprites(void)
{
	batch_count = 0;
	first_sprite_draw = true;

	max_allocated_samplers = min(MAX_SAMPLERS, glConfig.texture_units);
}

static void GLM_SpriteVertSet(glm_sprite_vert_t* vert, vec3_t position, float s, float t, int sampler, float index)
{
	vert->sampler = sampler;
	vert->tex[0] = s;
	vert->tex[1] = t;
	VectorCopy(position, vert->position);
	vert->array_index = index;
}

void GL_FlushSpriteBatch(void)
{
	glm_sprite_vert_t* vert = sprite_vbo_data;
	int i;

	if (batch_count && first_sprite_draw) {
		GL_EnterRegion("Sprites");

		GL_PrepareSprites();

		first_sprite_draw = false;
	}

	GL_Disable(GL_CULL_FACE);

	for (i = 0; i < batch_count; ++i) {
		glm_sprite_t* sprite = &sprite_batch[i];
		vec3_t points[4];

		VectorMA(sprite->origin, +1, sprite->up, points[0]);
		VectorMA(points[0], -1, sprite->right, points[0]);
		VectorMA(sprite->origin, -1, sprite->up, points[1]);
		VectorMA(points[1], -1, sprite->right, points[1]);
		VectorMA(sprite->origin, +1, sprite->up, points[2]);
		VectorMA(points[2], +1, sprite->right, points[2]);
		VectorMA(sprite->origin, -1, sprite->up, points[3]);
		VectorMA(points[3], +1, sprite->right, points[3]);

		GLM_SpriteVertSet(vert++, points[0], 0, 0, sprite->sampler, sprite->texture_index);
		GLM_SpriteVertSet(vert++, points[1], 0, sprite->texScale[1], sprite->sampler, sprite->texture_index);
		GLM_SpriteVertSet(vert++, points[2], sprite->texScale[0], 0, sprite->sampler, sprite->texture_index);
		GLM_SpriteVertSet(vert++, points[3], sprite->texScale[0], sprite->texScale[1], sprite->sampler, sprite->texture_index);
	}

	GL_UseProgram(spriteProgram.program);
	GL_UpdateVBO(spriteVBO, sizeof(sprite_vbo_data[0]) * batch_count * 4, sprite_vbo_data);
	GL_BindTextures(0, allocated_samplers, sampler_textures);

	GL_BindVertexArray(&spriteVAO);
	glDrawElements(GL_TRIANGLE_STRIP, batch_count * 5, GL_UNSIGNED_SHORT, NULL);

	GL_Enable(GL_CULL_FACE);

	++frameStats.draw_calls;
	batch_count = 0;
	allocated_samplers = 0;
}

void GL_EndDrawSprites(void)
{
	if (GL_ShadersSupported()) {
		if (batch_count) {
			GL_FlushSpriteBatch();
		}

		if (!first_sprite_draw) {
			GL_LeaveRegion();
		}
	}
}

static void GL_CompileSpriteProgram(void)
{
	if (GLM_ProgramRecompileNeeded(&spriteProgram, 0)) {
		char include_text[512];
		GL_VFDeclare(sprite);

		include_text[0] = '\0';
		strlcpy(include_text, va("#define MAX_INSTANCEID %d\n", MAX_SPRITE_BATCH), sizeof(include_text));
		strlcpy(include_text, va("#define SAMPLER_COUNT %d\n", max_allocated_samplers), sizeof(include_text));

		// Initialise program for drawing image
		GLM_CreateVFProgramWithInclude("Sprites", GL_VFParams(sprite), &spriteProgram, include_text);
	}

	if (spriteProgram.program && !spriteProgram.uniforms_found) {
		spriteProgram.uniforms_found = true;
	}
}

glm_sprite_t* NextSprite(texture_ref texture)
{
	int samplerIndex = -1;
	int i;

	if (batch_count >= sizeof(sprite_batch) / sizeof(sprite_batch[0])) {
		GL_FlushSpriteBatch();
	}

	for (i = 0; i < allocated_samplers; ++i) {
		if (GL_TextureReferenceEqual(sampler_textures[i], texture)) {
			samplerIndex = i;
			break;
		}
	}

	if (samplerIndex < 0) {
		if (allocated_samplers >= max_allocated_samplers) {
			GL_FlushSpriteBatch();
		}
		sampler_textures[allocated_samplers++] = texture;
	}

	return &sprite_batch[batch_count++];
}

void GLM_DrawSimpleItem(texture_ref texture_array, int texture_index, float scale_s, float scale_t, vec3_t origin, float scale_, vec3_t up, vec3_t right)
{
	glm_sprite_t* sprite;

	sprite = NextSprite(texture_array);
	sprite->texture_index = texture_index;
	sprite->texScale[0] = scale_s;
	sprite->texScale[1] = scale_t;
	VectorCopy(origin, sprite->origin);
	VectorScale(up, scale_, sprite->up);
	VectorScale(right, scale_, sprite->right);
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

	{
		glm_sprite_t* sprite = NextSprite(frame->gl_arraynum);

		sprite = &sprite_batch[batch_count];
		sprite->texture_index = frame->gl_arrayindex;
		sprite->texScale[0] = frame->gl_scalingS;
		sprite->texScale[1] = frame->gl_scalingT;
		VectorCopy(e->origin, sprite->origin);
		VectorScale(up, 5, sprite->up);
		VectorScale(right, 5, sprite->right);
	}
}
