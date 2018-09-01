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
#include "tr_types.h"

GLenum glPrimitiveTypes[r_primitive_count] = {
	GL_TRIANGLE_STRIP,
	GL_TRIANGLE_FAN,
	GL_TRIANGLES
};

void GL_DrawSequentialBatchImpl(gl_sprite3d_batch_t* batch, int first_batch, int last_batch, int index_offset, GLuint maximum_batch_size)
{
	int vertOffset = batch->glFirstVertices[first_batch];
	int numVertices = batch->numVertices[first_batch];
	int batch_count = last_batch - first_batch;
	void* indexes = (void*)(index_offset * sizeof(GLuint));
	int terminators = (glConfig.supported_features & R_SUPPORT_PRIMITIVERESTART) ? 1 : (numVertices % 2 == 0 ? 2 : 3);

	while (batch_count > maximum_batch_size) {
		GL_DrawElementsBaseVertex(glPrimitiveTypes[batch->primitive_id], maximum_batch_size * numVertices + (maximum_batch_size - 1) * terminators, GL_UNSIGNED_INT, indexes, vertOffset);
		batch_count -= maximum_batch_size;
		vertOffset += maximum_batch_size * numVertices;
	}
	if (batch_count) {
		GL_DrawElementsBaseVertex(glPrimitiveTypes[batch->primitive_id], batch_count * numVertices + (batch_count - 1) * terminators, GL_UNSIGNED_INT, indexes, vertOffset);
	}
}
