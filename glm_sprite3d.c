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
#include "r_program.h"
#include "r_renderer.h"

static void GLM_Create3DSpriteVAO(void)
{
	R_Sprite3DCreateVBO();

	if (!R_VertexArrayCreated(vao_3dsprites)) {
		R_GenVertexArray(vao_3dsprites);

		R_Sprite3DCreateIndexBuffer();
		buffers.Bind(r_buffer_sprite_index_data);

		// position
		GLM_ConfigureVertexAttribPointer(vao_3dsprites, r_buffer_sprite_vertex_data, 0, 3, GL_FLOAT, GL_FALSE, sizeof(r_sprite3d_vert_t), VBO_FIELDOFFSET(r_sprite3d_vert_t, position), 0);
		// texture coordinates
		GLM_ConfigureVertexAttribPointer(vao_3dsprites, r_buffer_sprite_vertex_data, 1, 3, GL_FLOAT, GL_FALSE, sizeof(r_sprite3d_vert_t), VBO_FIELDOFFSET(r_sprite3d_vert_t, tex), 0);
		// color
		GLM_ConfigureVertexAttribPointer(vao_3dsprites, r_buffer_sprite_vertex_data, 2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(r_sprite3d_vert_t), VBO_FIELDOFFSET(r_sprite3d_vert_t, color), 0);

		R_BindVertexArray(vao_none);
	}
}

static void GLM_Compile3DSpriteProgram(void)
{
	if (R_ProgramRecompileNeeded(r_program_sprite3d, 0)) {
		R_ProgramCompile(r_program_sprite3d);
	}
}

static qbool GLM_3DSpritesInit(void)
{
	// Create program
	GLM_Compile3DSpriteProgram();
	GLM_Create3DSpriteVAO();

	return (R_ProgramReady(r_program_sprite3d) && R_VertexArrayCreated(vao_3dsprites));
}

static void GLM_DrawSequentialBatch(gl_sprite3d_batch_t* batch, int index_offset, GLuint maximum_batch_size)
{
	if (R_TextureReferenceIsValid(batch->texture)) {
		// All batches are the same texture, so no issues
		GL_DrawSequentialBatchImpl(batch, 0, batch->count, index_offset, maximum_batch_size);
	}
	else {
		// Group by texture usage
		int start = 0, end = 1;

		renderer.TextureUnitBind(0, batch->textures[start]);
		for (end = 1; end < batch->count; ++end) {
			if (!R_TextureReferenceEqual(batch->textures[start], batch->textures[end])) {
				GL_DrawSequentialBatchImpl(batch, start, end, index_offset, maximum_batch_size);

				renderer.TextureUnitBind(0, batch->textures[end]);
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

	R_TraceEnterNamedRegion(__FUNCTION__);

	GLM_Create3DSpriteVAO();

	if (R_BufferReferenceIsValid(r_buffer_sprite_vertex_data)) {
		buffers.Update(r_buffer_sprite_vertex_data, vertexCount * sizeof(verts[0]), verts);
	}

	R_TraceLeaveNamedRegion();
}

void GLM_Draw3DSprites(void)
{
	unsigned int i;
	qbool current_alpha_test = false;
	qbool first_batch = true;

	if (!batchCount || !vertexCount || (batchCount == 1 && !batches[0].count)) {
		return;
	}

	R_TraceEnterNamedRegion(__FUNCTION__);

	if (!GLM_3DSpritesInit()) {
		R_TraceLeaveNamedRegion();
		return;
	}

	R_ProgramUse(r_program_sprite3d);

	for (i = 0; i < batchCount; ++i) {
		gl_sprite3d_batch_t* batch = &batches[i];
		qbool alpha_test = (batch->id == SPRITE3D_ENTITIES);

		if (!batch->count) {
			continue;
		}

		R_TraceEnterNamedRegion(batch->name);
		if (first_batch || current_alpha_test != alpha_test) {
			R_ProgramUniform1i(r_program_uniform_sprite3d_alpha_test, current_alpha_test = alpha_test);
		}
		first_batch = false;

		R_ApplyRenderingState(batch->rendering_state);
		if (R_TextureReferenceIsValid(batch->texture)) {
			renderer.TextureUnitBind(0, batch->texture);
		}
		else if (!R_TextureReferenceIsValid(batch->textures[0])) {
			extern texture_ref particletexture_array;

			renderer.TextureUnitBind(0, batch->texture = particletexture_array);
		}

		if (batch->count == 1) {
			if (R_TextureReferenceIsValid(batch->textures[0])) {
				renderer.TextureUnitBind(0, batch->textures[0]);
			}
			GL_DrawArrays(glPrimitiveTypes[batch->primitive_id], batch->glFirstVertices[0], batch->numVertices[0]);
		}
		else if (batch->allSameNumber && batch->numVertices[0] == 4 && batch->primitive_id == r_primitive_triangle_strip) {
			GLM_DrawSequentialBatch(batch, indexes_start_quads, INDEXES_MAX_QUADS);
		}
		else if (batch->allSameNumber && batch->numVertices[0] == 9 && GL_Supported(R_SUPPORT_PRIMITIVERESTART)) {
			GLM_DrawSequentialBatch(batch, indexes_start_sparks, INDEXES_MAX_SPARKS);
		}
		else if (batch->allSameNumber && batch->numVertices[0] == 18 && GL_Supported(R_SUPPORT_PRIMITIVERESTART)) {
			GLM_DrawSequentialBatch(batch, indexes_start_flashblend, INDEXES_MAX_FLASHBLEND);
		}
		else if (R_TextureReferenceIsValid(batch->texture)) {
			GL_MultiDrawArrays(glPrimitiveTypes[batch->primitive_id], batch->glFirstVertices, batch->numVertices, batch->count);
		}
		else {
			int first = 0, last = 1;
			renderer.TextureUnitBind(0, batch->textures[0]);
			while (last < batch->count) {
				if (!R_TextureReferenceEqual(batch->textures[first], batch->textures[last])) {
					GL_MultiDrawArrays(glPrimitiveTypes[batch->primitive_id], batch->glFirstVertices, batch->numVertices, last - first);

					renderer.TextureUnitBind(0, batch->textures[last]);
					first = last;
				}
				++last;
			}

			GL_MultiDrawArrays(glPrimitiveTypes[batch->primitive_id], batch->glFirstVertices, batch->numVertices, last - first);
		}

		R_TraceLeaveNamedRegion();

		batch->count = 0;
	}

	R_TraceLeaveNamedRegion();

	R_Sprite3DClearBatches();
}
