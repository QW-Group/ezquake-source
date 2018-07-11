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
#include "r_sprite3d_internal.h"
#include "glm_vao.h"
#include "tr_types.h"
#include "r_state.h"
#include "glm_local.h"

static glm_program_t sprite3dProgram;
static GLint sprite3dUniform_alpha_test;

static void GLM_Create3DSpriteVAO(void)
{
	GL_Create3DSpriteVBO();

	if (!R_VertexArrayCreated(vao_3dsprites)) {
		R_GenVertexArray(vao_3dsprites);

		GL_Create3DSpriteIndexBuffer();
		buffers.Bind(sprite3dIndexes);

		// position
		GLM_ConfigureVertexAttribPointer(vao_3dsprites, sprite3dVBO, 0, 3, GL_FLOAT, GL_FALSE, sizeof(r_sprite3d_vert_t), VBO_FIELDOFFSET(r_sprite3d_vert_t, position), 0);
		// texture coordinates
		GLM_ConfigureVertexAttribPointer(vao_3dsprites, sprite3dVBO, 1, 3, GL_FLOAT, GL_FALSE, sizeof(r_sprite3d_vert_t), VBO_FIELDOFFSET(r_sprite3d_vert_t, tex), 0);
		// color
		GLM_ConfigureVertexAttribPointer(vao_3dsprites, sprite3dVBO, 2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(r_sprite3d_vert_t), VBO_FIELDOFFSET(r_sprite3d_vert_t, color), 0);

		R_BindVertexArray(vao_none);
	}
}

static void GLM_Compile3DSpriteProgram(void)
{
	if (GLM_ProgramRecompileNeeded(&sprite3dProgram, 0)) {
		GL_VFDeclare(draw_sprites);

		GLM_CreateVFProgram("3d-sprites", GL_VFParams(draw_sprites), &sprite3dProgram);
	}

	if (sprite3dProgram.program && !sprite3dProgram.uniforms_found) {
		sprite3dUniform_alpha_test = GLM_UniformGetLocation(sprite3dProgram.program, "alpha_test");

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

void GLM_Prepare3DSprites(r_sprite3d_vert_t* verts, int batchCount, int vertexCount)
{
	if (!batchCount || !vertexCount) {
		return;
	}

	GL_EnterRegion(__FUNCTION__);

	GLM_Create3DSpriteVAO();

	if (GL_BufferReferenceIsValid(sprite3dVBO)) {
		buffers.Update(sprite3dVBO, vertexCount * sizeof(verts[0]), verts);
	}

	GL_LeaveRegion();
}

void GLM_Draw3DSprites(gl_sprite3d_batch_t* batches, r_sprite3d_vert_t* verts, int batchCount, int vertexCount)
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

	GLM_UseProgram(sprite3dProgram.program);

	for (i = 0; i < batchCount; ++i) {
		gl_sprite3d_batch_t* batch = &batches[i];
		qbool alpha_test = (batch->id == SPRITE3D_ENTITIES);

		if (!batch->count) {
			continue;
		}

		GL_EnterRegion(batch->name);
		if (first_batch || current_alpha_test != alpha_test) {
			GLM_Uniform1i(sprite3dUniform_alpha_test, current_alpha_test = alpha_test);
		}
		first_batch = false;

		R_ApplyRenderingState(batch->textured_rendering_state);
		if (GL_TextureReferenceIsValid(batch->texture)) {
			R_TextureUnitBind(0, batch->texture);
		}
		else if (!GL_TextureReferenceIsValid(batch->textures[0])) {
			extern texture_ref particletexture_array;

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
					if (!GL_TextureReferenceEqual(batch->textures[first], batch->textures[last])) {
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
}
