/*
Copyright (C) 2026 ezQuake team

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

#ifdef RENDERER_OPTION_CLASSIC_OPENGL

#include "quakedef.h"
#include "gl_local.h"
#include "glc_vao.h"
#include "r_buffers.h"
#include "r_program.h"
#include "tr_types.h"
#include "r_scenetris.h"

static void GLC_DrawSceneTrisArrays(qbool lines, int first, int count)
{
	GL_DrawArrays(lines ? GL_LINES : GL_TRIANGLES, first, count);
}

// The client arrays GLC_DrawSceneTrisInline draws from need the buffer API to unbind
// GL_ARRAY_BUFFER and a bound VAO for GL_DrawArrays, and a context that isn't GLSL only.
qbool GLC_SceneTrisSupported(void)
{
	return buffers.supported && GL_Supported(R_SUPPORT_IMMEDIATEMODE);
}

// Fixed function, with client arrays straight from the vertex list
void GLC_DrawSceneTrisInline(void)
{
	if (GLC_SceneTrisSupported() && R_SceneTrisPrepare()) {
		// The state's VAO must exist before the state is applied, or GLC_EnsureVAOCreated
		// would make it a brush model VAO
		if (!R_VertexArrayCreated(vao_scenetris)) {
			scenetris_vert_t *verts = R_SceneTrisVertices();

			R_GenVertexArray(vao_scenetris);
			GLC_VAOEnableVertexPointer(vao_scenetris, 3, GL_FLOAT, sizeof(verts[0]), verts[0].position);
			GLC_VAOEnableTextureCoordPointer(vao_scenetris, 0, 2, GL_FLOAT, sizeof(verts[0]), verts[0].texcoord);
			GLC_VAOEnableColorPointer(vao_scenetris, 4, GL_FLOAT, sizeof(verts[0]), verts[0].color);
		}

		R_ProgramUse(r_program_none);
		R_SceneTrisDraw(GLC_DrawSceneTrisArrays);
	}

	R_SceneTrisClear();
}

#endif // #ifdef RENDERER_OPTION_CLASSIC_OPENGL
