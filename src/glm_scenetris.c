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

#ifdef RENDERER_OPTION_MODERN_OPENGL

#include "quakedef.h"
#include "gl_local.h"
#include "glm_local.h"
#include "glm_vao.h"
#include "r_buffers.h"
#include "r_program.h"
#include "r_scenetris.h"

qbool GLM_CompileSceneTrisProgram(void)
{
	if (R_ProgramRecompileNeeded(r_program_scenetris, 0)) {
		R_ProgramCompile(r_program_scenetris);

		if (!GL_ShaderBindingsSupported() && R_ProgramReady(r_program_scenetris)) {
			R_ProgramUse(r_program_scenetris);
			R_ProgramUniform1i(r_program_uniform_scenetris_materialtex, 0);
		}
	}

	return R_ProgramReady(r_program_scenetris);
}

// Drawn with a program and a vertex buffer, which this renderer always has
qbool GLM_SceneTrisSupported(void)
{
	return true;
}

// Created on first use and again after vid_restart, which deletes both buffer and VAO
static qbool GLM_CreateSceneTrisVAO(void)
{
	if (!R_BufferReferenceIsValid(r_buffer_scenetris_vertex_data) || !R_VertexArrayCreated(vao_scenetris)) {
		if (!R_BufferReferenceIsValid(r_buffer_scenetris_vertex_data)) {
			buffers.Create(r_buffer_scenetris_vertex_data, buffertype_vertex, "scenetris-vbo", SCENETRIS_MAX_VERTS * sizeof(scenetris_vert_t), NULL, bufferusage_once_per_frame);
		}

		R_GenVertexArray(vao_scenetris);
		GLM_ConfigureVertexAttribPointer(vao_scenetris, r_buffer_scenetris_vertex_data, 0, 3, GL_FLOAT, GL_FALSE, sizeof(scenetris_vert_t), VBO_FIELDOFFSET(scenetris_vert_t, position), 0);
		GLM_ConfigureVertexAttribPointer(vao_scenetris, r_buffer_scenetris_vertex_data, 1, 2, GL_FLOAT, GL_FALSE, sizeof(scenetris_vert_t), VBO_FIELDOFFSET(scenetris_vert_t, texcoord), 0);
		GLM_ConfigureVertexAttribPointer(vao_scenetris, r_buffer_scenetris_vertex_data, 2, 4, GL_FLOAT, GL_FALSE, sizeof(scenetris_vert_t), VBO_FIELDOFFSET(scenetris_vert_t, color), 0);
		R_BindVertexArray(vao_none);
	}

	return R_BufferReferenceIsValid(r_buffer_scenetris_vertex_data) && R_VertexArrayCreated(vao_scenetris);
}

// The VAO points at the start of the buffer; this frame's copy sits in the triple-buffer slot
static void GLM_DrawSceneTrisArrays(qbool lines, int first, int count)
{
	GL_DrawArrays(lines ? GL_LINES : GL_TRIANGLES, buffers.BufferOffset(r_buffer_scenetris_vertex_data) / sizeof(scenetris_vert_t) + first, count);
}

void GLM_DrawSceneTris(void)
{
	int numverts = R_SceneTrisPrepare();

	if (numverts && GLM_CompileSceneTrisProgram() && GLM_CreateSceneTrisVAO()) {
		buffers.Update(r_buffer_scenetris_vertex_data, numverts * sizeof(scenetris_vert_t), R_SceneTrisVertices());
		R_ProgramUse(r_program_scenetris);
		R_SceneTrisDraw(GLM_DrawSceneTrisArrays);
	}

	R_SceneTrisClear();
}

#endif // #ifdef RENDERER_OPTION_MODERN_OPENGL
