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

// 3D sprites
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "gl_sprite3d.h"
#include "tr_types.h"
#include "glm_vao.h"
#include "glc_vao.h"
#include "r_state.h"

void GLM_Draw3DSprites(void);
void GLM_Prepare3DSprites(void);
void GLC_Draw3DSprites(void);

#define MAX_3DSPRITES_PER_BATCH     1024  // Batches limited to this so they can't break other functionality
#define INDEXES_MAX_QUADS            512
#define INDEXES_MAX_FLASHBLEND         8
#define INDEXES_MAX_SPARKS            16
static int indexes_start_quads;
static int indexes_start_flashblend;
static int indexes_start_sparks;
static GLuint indexData[INDEXES_MAX_QUADS * 4 + INDEXES_MAX_SPARKS * 9 + INDEXES_MAX_FLASHBLEND * 18 + (INDEXES_MAX_QUADS + INDEXES_MAX_SPARKS + INDEXES_MAX_FLASHBLEND) * 3];

static GLenum glPrimitiveTypes[r_primitive_count] = {
	GL_TRIANGLE_STRIP,
	GL_TRIANGLE_FAN,
	GL_TRIANGLES
};

static const char* batch_type_names[] = {
	"ENTITIES",
	"PARTICLES_CLASSIC",
	"PARTICLES_NEW_p_spark",
	"PARTICLES_NEW_p_smoke",
	"PARTICLES_NEW_p_fire",
	"PARTICLES_NEW_p_bubble",
	"PARTICLES_NEW_p_lavasplash",
	"PARTICLES_NEW_p_gunblast",
	"PARTICLES_NEW_p_chunk",
	"PARTICLES_NEW_p_shockwave",
	"PARTICLES_NEW_p_inferno_flame",
	"PARTICLES_NEW_p_inferno_trail",
	"PARTICLES_NEW_p_sparkray",
	"PARTICLES_NEW_p_staticbubble",
	"PARTICLES_NEW_p_trailpart",
	"PARTICLES_NEW_p_dpsmoke",
	"PARTICLES_NEW_p_dpfire",
	"PARTICLES_NEW_p_teleflare",
	"PARTICLES_NEW_p_blood1",
	"PARTICLES_NEW_p_blood2",
	"PARTICLES_NEW_p_blood3",

	//VULT PARTICLES
	"PARTICLES_NEW_p_rain",
	"PARTICLES_NEW_p_alphatrail",
	"PARTICLES_NEW_p_railtrail",
	"PARTICLES_NEW_p_streak",
	"PARTICLES_NEW_p_streaktrail",
	"PARTICLES_NEW_p_streakwave",
	"PARTICLES_NEW_p_lightningbeam",
	"PARTICLES_NEW_p_vxblood",
	"PARTICLES_NEW_p_lavatrail",
	"PARTICLES_NEW_p_vxsmoke",
	"PARTICLES_NEW_p_vxsmoke_red",
	"PARTICLES_NEW_p_muzzleflash",
	"PARTICLES_NEW_p_inferno",
	"PARTICLES_NEW_p_2dshockwave",
	"PARTICLES_NEW_p_vxrocketsmoke",
	"PARTICLES_NEW_p_trailbleed",
	"PARTICLES_NEW_p_bleedspike",
	"PARTICLES_NEW_p_flame",
	"PARTICLES_NEW_p_bubble2",
	"PARTICLES_NEW_p_bloodcloud",
	"PARTICLES_NEW_p_chunkdir",
	"PARTICLES_NEW_p_smallspark",
	"PARTICLES_NEW_p_slimeglow",
	"PARTICLES_NEW_p_slimebubble",
	"PARTICLES_NEW_p_blacklavasmoke",
	"FLASHBLEND_LIGHTS",
	"CORONATEX_STANDARD",
	"CORONATEX_GUNFLASH",
	"CORONATEX_EXPLOSIONFLASH1",
	"CORONATEX_EXPLOSIONFLASH2",
	"CORONATEX_EXPLOSIONFLASH3",
	"CORONATEX_EXPLOSIONFLASH4",
	"CORONATEX_EXPLOSIONFLASH5",
	"CORONATEX_EXPLOSIONFLASH6",
	"CORONATEX_EXPLOSIONFLASH7",
	"CHATICON_AFK_CHAT",
	"CHATICON_CHAT",
	"CHATICON_AFK",

	"MAX_BATCHES"
};

typedef struct gl_sprite3d_batch_s {
	rendering_state_t* textured_rendering_state;
	rendering_state_t* untextured_rendering_state;
	r_primitive_id primitive_id;
	texture_ref texture;
	int texture_index;
	qbool allSameNumber;

	int firstVertices[MAX_3DSPRITES_PER_BATCH];
	int glFirstVertices[MAX_3DSPRITES_PER_BATCH];
	int numVertices[MAX_3DSPRITES_PER_BATCH];
	texture_ref textures[MAX_3DSPRITES_PER_BATCH];
	int textureIndexes[MAX_3DSPRITES_PER_BATCH];

	const char* name;
	unsigned int count;
} gl_sprite3d_batch_t;

#define MAX_VERTS_PER_SCENE (MAX_3DSPRITES_PER_BATCH * MAX_SPRITE3D_BATCHES * 18)

static r_sprite3d_vert_t verts[MAX_VERTS_PER_SCENE];
static gl_sprite3d_batch_t batches[MAX_SPRITE3D_BATCHES];
static unsigned int batchMapping[MAX_SPRITE3D_BATCHES];
static unsigned int batchCount;
static unsigned int vertexCount;

static buffer_ref sprite3dVBO;
static buffer_ref sprite3dIndexes;
static glm_program_t sprite3dProgram;
static GLint sprite3dUniform_alpha_test;

static gl_sprite3d_batch_t* BatchForType(sprite3d_batch_id type, qbool allocate)
{
	unsigned int index = batchMapping[type];

	if (index == 0) {
		if (!allocate || batchCount >= sizeof(batches) / sizeof(batches[0])) {
			return NULL;
		}
		batchMapping[type] = index = ++batchCount;
	}

	return &batches[index - 1];
}

void GL_Sprite3DInitialiseBatch(sprite3d_batch_id type, struct rendering_state_s* textured_state, struct rendering_state_s* untextured_state, texture_ref texture, int index, r_primitive_id primitive_type)
{
	gl_sprite3d_batch_t* batch = BatchForType(type, true);

	assert(!textured_state || textured_state->initialized);
	assert(!untextured_state || untextured_state->initialized);

	batch->textured_rendering_state = textured_state;
	batch->untextured_rendering_state = untextured_state;
	batch->texture = texture;
	batch->count = 0;
	batch->primitive_id = primitive_type;
	batch->allSameNumber = true;
	batch->texture_index = index;
	batch->name = batch_type_names[type];
}

r_sprite3d_vert_t* GL_Sprite3DAddEntrySpecific(sprite3d_batch_id type, int verts_required, texture_ref texture, int texture_index)
{
	gl_sprite3d_batch_t* batch = BatchForType(type, false);
	int start = vertexCount;

	if (!batch || batch->count >= MAX_3DSPRITES_PER_BATCH) {
		return NULL;
	}
	if (!GL_TextureReferenceIsValid(texture) && !GL_TextureReferenceIsValid(batch->texture) && !batch->untextured_rendering_state) {
		assert(false);
		return NULL;
	}

	if (start + verts_required >= MAX_VERTS_PER_SCENE) {
		return NULL;
	}
	batch->firstVertices[batch->count] = start;
	batch->glFirstVertices[batch->count] = start + GL_BufferOffset(sprite3dVBO) / sizeof(verts[0]);
	batch->numVertices[batch->count] = verts_required;
	batch->textures[batch->count] = texture;
	batch->textureIndexes[batch->count] = texture_index;

	if (batch->count) {
		batch->allSameNumber &= verts_required == batch->numVertices[0];
	}
	++batch->count;
	vertexCount += verts_required;
	return &verts[start];
}

r_sprite3d_vert_t* GL_Sprite3DAddEntryFixed(sprite3d_batch_id type, int verts_required)
{
	return GL_Sprite3DAddEntrySpecific(type, verts_required, null_texture_reference, 0);
}

r_sprite3d_vert_t* GL_Sprite3DAddEntry(sprite3d_batch_id type, int verts_required)
{
	return GL_Sprite3DAddEntrySpecific(type, verts_required, null_texture_reference, 0);
}

void GL_Sprite3DSetVert(r_sprite3d_vert_t* vert, float x, float y, float z, float s, float t, byte color[4], int texture_index)
{
	extern int particletexture_array_index;

	vert->color[0] = color[0];
	vert->color[1] = color[1];
	vert->color[2] = color[2];
	vert->color[3] = color[3];
	VectorSet(vert->position, x, y, z);
	if (texture_index < 0) {
		vert->tex[0] = 0.99;
		vert->tex[1] = 0.99;
		vert->tex[2] = particletexture_array_index;
	}
	else {
		vert->tex[0] = s;
		vert->tex[1] = t;
		vert->tex[2] = texture_index;
	}
}

void GL_Draw3DSprites(void)
{
	if (!batchCount || !vertexCount) {
		return;
	}

	if (GL_UseImmediateMode()) {
		GLC_Draw3DSprites();

		batchCount = vertexCount = 0;
		memset(batchMapping, 0, sizeof(batchMapping));
	}
}

static void GL_Create3DSpriteVBO(void)
{
	if (!GL_BufferReferenceIsValid(sprite3dVBO)) {
		sprite3dVBO = GL_CreateFixedBuffer(buffertype_vertex, "sprite3d-vbo", sizeof(verts), verts, bufferusage_once_per_frame);
	}
}

static void GL_Create3DSpriteIndexBuffer(void)
{
	if (!GL_BufferReferenceIsValid(sprite3dIndexes)) {
		// Meag: *3 is for case of primitive restart not being supported
		int i, j;
		int pos = 0;
		int vbo_pos;

		indexes_start_quads = pos;
		for (i = 0, vbo_pos = 0; i < INDEXES_MAX_QUADS; ++i) {
			if (i) {
				if (glConfig.primitiveRestartSupported) {
					indexData[pos++] = ~(GLuint)0;
				}
				else {
					indexData[pos++] = vbo_pos - 1;
					indexData[pos++] = vbo_pos;
				}
			}
			for (j = 0; j < 4; ++j) {
				indexData[pos++] = vbo_pos++;
			}
		}

		// These are currently only supported/used if primitive restart supported, as they're rendered as triangle fans
		indexes_start_flashblend = pos;
		for (i = 0, vbo_pos = 0; i < INDEXES_MAX_FLASHBLEND; ++i) {
			if (i) {
				if (glConfig.primitiveRestartSupported) {
					indexData[pos++] = ~(GLuint)0;
				}
				else {
					indexData[pos++] = vbo_pos - 1;
					indexData[pos++] = vbo_pos;
				}
			}
			for (j = 0; j < 18; ++j) {
				indexData[pos++] = vbo_pos++;
			}
		}

		// These are currently only supported/used if primitive restart supported, as they're rendered as triangle fans
		indexes_start_sparks = pos;
		for (i = 0, vbo_pos = 0; i < INDEXES_MAX_SPARKS; ++i) {
			if (i) {
				if (glConfig.primitiveRestartSupported) {
					indexData[pos++] = ~(GLuint)0;
				}
				else {
					indexData[pos++] = vbo_pos - 1;
					indexData[pos++] = vbo_pos - 1;
					indexData[pos++] = vbo_pos;
				}
			}
			for (j = 0; j < 9; ++j) {
				indexData[pos++] = vbo_pos++;
			}
		}

		sprite3dIndexes = GL_CreateFixedBuffer(buffertype_index, "3dsprite-indexes", sizeof(indexData), indexData, bufferusage_constant_data);
	}
}

static void GLM_Create3DSpriteVAO(void)
{
	GL_Create3DSpriteVBO();

	if (!R_VertexArrayCreated(vao_3dsprites)) {
		R_GenVertexArray(vao_3dsprites);

		GL_Create3DSpriteIndexBuffer();
		GL_BindBuffer(sprite3dIndexes);

		// position
		GLM_ConfigureVertexAttribPointer(vao_3dsprites, sprite3dVBO, 0, 3, GL_FLOAT, GL_FALSE, sizeof(r_sprite3d_vert_t), VBO_FIELDOFFSET(r_sprite3d_vert_t, position), 0);
		// texture coordinates
		GLM_ConfigureVertexAttribPointer(vao_3dsprites, sprite3dVBO, 1, 3, GL_FLOAT, GL_FALSE, sizeof(r_sprite3d_vert_t), VBO_FIELDOFFSET(r_sprite3d_vert_t, tex), 0);
		// color
		GLM_ConfigureVertexAttribPointer(vao_3dsprites, sprite3dVBO, 2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(r_sprite3d_vert_t), VBO_FIELDOFFSET(r_sprite3d_vert_t, color), 0);

		R_BindVertexArray(vao_none);
	}
}

static void GLC_Create3DSpriteVAO(void)
{
	if (GL_BuffersSupported()) {
		R_GenVertexArray(vao_3dsprites);
		GL_Create3DSpriteVBO();
		GL_Create3DSpriteIndexBuffer();

		GLC_VAOSetIndexBuffer(vao_3dsprites, sprite3dIndexes);
		GLC_VAOSetVertexBuffer(vao_3dsprites, sprite3dVBO);
		GLC_VAOEnableVertexPointer(vao_3dsprites, 3, GL_FLOAT, sizeof(r_sprite3d_vert_t), VBO_FIELDOFFSET(r_sprite3d_vert_t, position));
		GLC_VAOEnableColorPointer(vao_3dsprites, 4, GL_UNSIGNED_BYTE, sizeof(r_sprite3d_vert_t), VBO_FIELDOFFSET(r_sprite3d_vert_t, color));
		GLC_VAOEnableTextureCoordPointer(vao_3dsprites, 0, 2, GL_FLOAT, sizeof(r_sprite3d_vert_t), VBO_FIELDOFFSET(r_sprite3d_vert_t, tex));
	}
}

static void GLM_Compile3DSpriteProgram(void)
{
	if (GLM_ProgramRecompileNeeded(&sprite3dProgram, 0)) {
		GL_VFDeclare(draw_sprites);

		GLM_CreateVFProgram("3d-sprites", GL_VFParams(draw_sprites), &sprite3dProgram);
	}

	if (sprite3dProgram.program && !sprite3dProgram.uniforms_found) {
		sprite3dUniform_alpha_test = GL_UniformGetLocation(sprite3dProgram.program, "alpha_test");

		sprite3dProgram.uniforms_found = true;
	}
}

static qbool GLM_3DSpritesInit(void)
{
	// Create program
	GLM_Compile3DSpriteProgram();
	GLM_Create3DSpriteVAO();

	return (sprite3dProgram.program && R_VertexArrayCreated(vao_3dsprites));
}

static void GL_DrawSequentialBatchImpl(gl_sprite3d_batch_t* batch, int first_batch, int last_batch, int index_offset, GLuint maximum_batch_size)
{
	int vertOffset = batch->glFirstVertices[first_batch];
	int numVertices = batch->numVertices[first_batch];
	int batch_count = last_batch - first_batch;
	void* indexes = (void*)(index_offset * sizeof(GLuint));
	int terminators = glConfig.primitiveRestartSupported ? 1 : (numVertices % 2 == 0 ? 2 : 3);

	while (batch_count > maximum_batch_size) {
		GL_DrawElementsBaseVertex(glPrimitiveTypes[batch->primitive_id], maximum_batch_size * numVertices + (maximum_batch_size - 1) * terminators, GL_UNSIGNED_INT, indexes, vertOffset);
		batch_count -= maximum_batch_size;
		vertOffset += maximum_batch_size * numVertices;
	}
	if (batch_count) {
		GL_DrawElementsBaseVertex(glPrimitiveTypes[batch->primitive_id], batch_count * numVertices + (batch_count - 1) * terminators, GL_UNSIGNED_INT, indexes, vertOffset);
	}
}

static void GLC_DrawSequentialBatch(gl_sprite3d_batch_t* batch, int index_offset, GLuint maximum_batch_size)
{
	if (GL_TextureReferenceIsValid(batch->texture)) {
		if (!batch->textured_rendering_state) {
			assert(false);
			return;
		}
		R_ApplyRenderingState(batch->textured_rendering_state);
		R_TextureUnitBind(0, batch->texture);

		// All batches are the same texture, so no issues
		GL_DrawSequentialBatchImpl(batch, 0, batch->count, index_offset, maximum_batch_size);
	}
	else {
		// Group by texture usage
		int start = 0, end = 1;

		if (GL_TextureReferenceIsValid(batch->textures[start])) {
			if (!batch->textured_rendering_state) {
				assert(false);
				return;
			}

			R_ApplyRenderingState(batch->textured_rendering_state);
			R_TextureUnitBind(0, batch->textures[start]);
		}
		else {
			if (!batch->untextured_rendering_state) {
				assert(false);
				return;
			}
			R_ApplyRenderingState(batch->untextured_rendering_state);
		}
		for (end = 1; end < batch->count; ++end) {
			if (!GL_TextureReferenceEqual(batch->textures[start], batch->textures[end])) {
				GL_DrawSequentialBatchImpl(batch, start, end, index_offset, maximum_batch_size);

				if (GL_TextureReferenceIsValid(batch->textures[end])) {
					if (!batch->textured_rendering_state) {
						assert(false);
						return;
					}
					R_TextureUnitBind(0, batch->textures[end]);
					R_ApplyRenderingState(batch->textured_rendering_state);
				}
				else {
					if (!batch->untextured_rendering_state) {
						assert(false);
						return;
					}
					R_ApplyRenderingState(batch->untextured_rendering_state);
				}
				start = end;
			}
		}

		if (end > start) {
			GL_DrawSequentialBatchImpl(batch, start, end, index_offset, maximum_batch_size);
		}
	}
}

static void GLM_DrawSequentialBatch(gl_sprite3d_batch_t* batch, int index_offset, GLuint maximum_batch_size)
{
	if (GL_TextureReferenceIsValid(batch->texture)) {
		// All batches are the same texture, so no issues
		GL_DrawSequentialBatchImpl(batch, 0, batch->count, index_offset, maximum_batch_size);
	}
	else {
		// Group by texture usage
		int start = 0, end = 1;

		R_TextureUnitBind(0, batch->textures[start]);
		for (end = 1; end < batch->count; ++end) {
			if (!GL_TextureReferenceEqual(batch->textures[start], batch->textures[end])) {
				GL_DrawSequentialBatchImpl(batch, start, end, index_offset, maximum_batch_size);

				R_TextureUnitBind(0, batch->textures[end]);
				start = end;
			}
		}

		if (end > start) {
			GL_DrawSequentialBatchImpl(batch, start, end, index_offset, maximum_batch_size);
		}
	}
}

void GLM_Prepare3DSprites(void)
{
	if (!batchCount || !vertexCount) {
		return;
	}

	GL_EnterRegion(__FUNCTION__);

	GLM_Create3DSpriteVAO();

	if (GL_BufferReferenceIsValid(sprite3dVBO)) {
		GL_UpdateBuffer(sprite3dVBO, vertexCount * sizeof(verts[0]), verts);
	}

	GL_LeaveRegion();
}

void GLM_Draw3DSprites(void)
{
	unsigned int i;
	qbool current_alpha_test = false;
	qbool first_batch = true;

	if (!batchCount || (batchCount == 1 && !batches[0].count)) {
		return;
	}

	GL_EnterRegion(__FUNCTION__);

	if (!GLM_3DSpritesInit()) {
		GL_LeaveRegion();
		return;
	}

	GLM_StateBeginDraw3DSprites();
	GL_UseProgram(sprite3dProgram.program);
	R_BindVertexArray(vao_3dsprites);

	for (i = 0; i < batchCount; ++i) {
		gl_sprite3d_batch_t* batch = &batches[i];
		qbool alpha_test = (i == batchMapping[SPRITE3D_ENTITIES] - 1);

		if (!batch->count) {
			continue;
		}

		GL_EnterRegion(batch->name);
		if (first_batch || current_alpha_test != alpha_test) {
			GL_Uniform1i(sprite3dUniform_alpha_test, current_alpha_test = alpha_test);
		}
		first_batch = false;

		if (GL_TextureReferenceIsValid(batch->texture)) {
			if (!batch->textured_rendering_state) {
				assert(false);
				continue;
			}
			R_ApplyRenderingState(batch->textured_rendering_state);
			R_TextureUnitBind(0, batch->texture);
		}
		else if (!GL_TextureReferenceIsValid(batch->textures[0])) {
			extern texture_ref particletexture_array;

			if (!batch->textured_rendering_state) {
				assert(false);
				continue;
			}
			R_ApplyRenderingState(batch->textured_rendering_state);
			R_TextureUnitBind(0, batch->texture = particletexture_array);
		}

		if (batch->count == 1) {
			if (GL_TextureReferenceIsValid(batch->textures[0])) {
				R_TextureUnitBind(0, batch->textures[0]);
			}
			GL_DrawArrays(glPrimitiveTypes[batch->primitive_id], batch->glFirstVertices[0], batch->numVertices[0]);
		}
		else if (batch->allSameNumber && batch->numVertices[0] == 4) {
			GLM_DrawSequentialBatch(batch, indexes_start_quads, INDEXES_MAX_QUADS);
		}
		else if (batch->allSameNumber && batch->numVertices[0] == 9 && glConfig.primitiveRestartSupported) {
			GLM_DrawSequentialBatch(batch, indexes_start_sparks, INDEXES_MAX_SPARKS);
		}
		else if (batch->allSameNumber && batch->numVertices[0] == 18 && glConfig.primitiveRestartSupported) {
			GLM_DrawSequentialBatch(batch, indexes_start_flashblend, INDEXES_MAX_FLASHBLEND);
		}
		else {
			if (GL_TextureReferenceIsValid(batch->texture)) {
				GL_MultiDrawArrays(glPrimitiveTypes[batch->primitive_id], batch->glFirstVertices, batch->numVertices, batch->count);
			}
			else {
				int first = 0, last = 1;
				R_TextureUnitBind(0, batch->textures[0]);
				while (last < batch->count) {
					if (! GL_TextureReferenceEqual(batch->textures[first], batch->textures[last])) {
						GL_MultiDrawArrays(glPrimitiveTypes[batch->primitive_id], batch->glFirstVertices, batch->numVertices, last - first);

						R_TextureUnitBind(0, batch->textures[last]);
						first = last;
					}
					++last;
				}

				GL_MultiDrawArrays(glPrimitiveTypes[batch->primitive_id], batch->glFirstVertices, batch->numVertices, last - first);
			}
		}

		GL_LeaveRegion();

		batch->count = 0;
	}

	GL_LeaveRegion();

	batchCount = vertexCount = 0;
	memset(batchMapping, 0, sizeof(batchMapping));
}

void GLC_Draw3DSprites(void)
{
	unsigned int i, j, k;

	if (!batchCount) {
		return;
	}

	if (GL_BuffersSupported()) {
		if (!R_VertexArrayCreated(vao_3dsprites)) {
			GLC_Create3DSpriteVAO();
		}
		GL_UpdateBuffer(sprite3dVBO, vertexCount * sizeof(verts[0]), verts);
	}

	for (i = 0; i < batchCount; ++i) {
		gl_sprite3d_batch_t* batch = &batches[i];

		if (!batch->count) {
			continue;
		}

		if (GL_BuffersSupported()) {
			if (batch->count == 1) {
				if (GL_TextureReferenceIsValid(batch->textures[0])) {
					R_TextureUnitBind(0, batch->textures[0]);
					R_ApplyRenderingState(batch->textured_rendering_state);
				}
				else if (GL_TextureReferenceIsValid(batch->texture)) {
					R_TextureUnitBind(0, batch->texture);
					R_ApplyRenderingState(batch->textured_rendering_state);
				}
				else {
					R_ApplyRenderingState(batch->untextured_rendering_state);
				}
				GL_DrawArrays(glPrimitiveTypes[batch->primitive_id], batch->glFirstVertices[0], batch->numVertices[0]);
			}
			else if (GL_DrawElementsBaseVertexAvailable() && batch->allSameNumber && batch->numVertices[0] == 4 && glPrimitiveTypes[batch->primitive_id] == GL_TRIANGLE_STRIP) {
				GLC_DrawSequentialBatch(batch, indexes_start_quads, INDEXES_MAX_QUADS);
			}
			else if (GL_DrawElementsBaseVertexAvailable() && batch->allSameNumber && batch->numVertices[0] == 9 && glConfig.primitiveRestartSupported) {
				GLC_DrawSequentialBatch(batch, indexes_start_sparks, INDEXES_MAX_SPARKS);
			}
			else if (GL_DrawElementsBaseVertexAvailable() && batch->allSameNumber && batch->numVertices[0] == 18 && glConfig.primitiveRestartSupported) {
				GLC_DrawSequentialBatch(batch, indexes_start_flashblend, INDEXES_MAX_FLASHBLEND);
			}
			else {
				if (GL_TextureReferenceIsValid(batch->texture)) {
					R_TextureUnitBind(0, batch->texture);
					R_ApplyRenderingState(batch->textured_rendering_state);

					GL_MultiDrawArrays(glPrimitiveTypes[batch->primitive_id], batch->glFirstVertices, batch->numVertices, batch->count);
				}
				else {
					int first = 0, last = 1;

					if (GL_TextureReferenceIsValid(batch->textures[0])) {
						R_TextureUnitBind(0, batch->textures[0]);
						R_ApplyRenderingState(batch->textured_rendering_state);
					}
					else {
						R_ApplyRenderingState(batch->untextured_rendering_state);
					}

					while (last < batch->count) {
						if (!GL_TextureReferenceEqual(batch->textures[first], batch->textures[last])) {
							GL_MultiDrawArrays(glPrimitiveTypes[batch->primitive_id], batch->glFirstVertices, batch->numVertices, last - first);

							if (GL_TextureReferenceIsValid(batch->textures[last])) {
								R_TextureUnitBind(0, batch->textures[last]);
								R_ApplyRenderingState(batch->textured_rendering_state);
							}
							else {
								R_ApplyRenderingState(batch->untextured_rendering_state);
							}
							first = last;
						}
						++last;
					}

					GL_MultiDrawArrays(glPrimitiveTypes[batch->primitive_id], batch->glFirstVertices, batch->numVertices, last - first);
				}
			}
		}
		else {
			for (j = 0; j < batch->count; ++j) {
				r_sprite3d_vert_t* v;

				if (GL_TextureReferenceIsValid(batch->textures[j])) {
					R_TextureUnitBind(0, batch->textures[j]);
					R_ApplyRenderingState(batch->textured_rendering_state);
				}
				else if (GL_TextureReferenceIsValid(batch->texture)) {
					R_TextureUnitBind(0, batch->texture);
					R_ApplyRenderingState(batch->textured_rendering_state);
				}
				else {
					R_ApplyRenderingState(batch->untextured_rendering_state);
				}

				// Immediate mode
				glBegin(glPrimitiveTypes[batch->primitive_id]);
				v = &verts[batch->firstVertices[j]];
				for (k = 0; k < batch->numVertices[j]; ++k, ++v) {
					glTexCoord2fv(v->tex);
					R_CustomColor4ubv(v->color);
					glVertex3fv(v->position);
				}
				glEnd();
			}
		}

		batch->count = 0;
	}
}
