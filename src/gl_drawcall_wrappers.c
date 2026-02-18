/*
Copyright (C) 2018 ezQuake team

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

#include <SDL.h>

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "tr_types.h"
#include "r_vao.h"

// <draw-functions (various)>
GL_StaticProcedureDeclaration(glMultiDrawArrays, "mode=%u, first=%p, count=%p, drawcount=%d", GLenum mode, const GLint* first, const GLsizei* count, GLsizei drawcount)
GL_StaticProcedureDeclaration(glMultiDrawElements, "mode=%u, count=%p, type=%u, indices=%p, drawcount=%d", GLenum mode, const GLsizei* count, GLenum type, const GLvoid* const* indices, GLsizei drawcount)
GL_StaticProcedureDeclaration(glDrawElementsBaseVertex, "mode=%u, count=%d, type=%u, indices=%p, basevertex=%d", GLenum mode, GLsizei count, GLenum type, GLvoid* indices, GLint basevertex)
GL_StaticProcedureDeclaration(glPrimitiveRestartIndex, "index=%u", GLuint index)

// (modern/4.3+)
// typedef void (APIENTRY *glDrawArraysInstanced_t)(GLenum mode, GLint first, GLsizei count, GLsizei primcount);
GL_StaticProcedureDeclaration(glMultiDrawArraysIndirect, "mode=%u, indirect=%p, drawcount=%d, stride=%d", GLenum mode, const void* indirect, GLsizei drawcount, GLsizei stride)
GL_StaticProcedureDeclaration(glMultiDrawElementsIndirect, "mode=%u, type=%u, indirect=%p, drawcount=%d, stride=%d", GLenum mode, GLenum type, const void* indirect, GLsizei drawcount, GLsizei stride)
GL_StaticProcedureDeclaration(glDrawArraysInstancedBaseInstance, "mode=%u, first=%d, count=%d, primcount=%d, baseinstance=%u", GLenum mode, GLint first, GLsizei count, GLsizei primcount, GLuint baseinstance)
GL_StaticProcedureDeclaration(glDrawElementsInstancedBaseInstance, "mode=%u, count=%d, type=%u, indices=%p, primcount=%d, baseinstance=%d", GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei primcount, GLuint baseinstance)
GL_StaticProcedureDeclaration(glDrawElementsInstancedBaseVertexBaseInstance, "mode=%u, count=%d, type=%u, indices=%p, primcount=%d, basevertex=%d, baseinstance=%u", GLenum mode, GLsizei count, GLenum type, GLvoid* indices, GLsizei primcount, GLint basevertex, GLuint baseinstance)
GL_StaticProcedureDeclaration(glDrawElementsIndirect, "mode=%u, type=%u, indirect=%p", GLenum mode, GLenum type, GLvoid* indirect)
GL_StaticProcedureDeclaration(glDrawArraysIndirect, "mode=%u, indirect=%p", GLenum mode, const void* indirect)
GL_StaticProcedureDeclaration(glUniformBlockBinding, "program=%u, index=%u, binding=%u", GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding)
GL_StaticFunctionDeclaration(glGetUniformBlockIndex, "program=%u, name=%s", "returns %u", GLuint, GLuint program, const GLchar *uniformBlockName)
GL_StaticFunctionWrapperBody(glGetUniformBlockIndex, GLuint, program, uniformBlockName)
// </draw-functions>

void GL_LoadDrawFunctions(void)
{
	glConfig.supported_features &= ~(R_SUPPORT_INDIRECT_RENDERING | R_SUPPORT_INSTANCED_RENDERING | R_SUPPORT_PRIMITIVERESTART);

	if (GL_VersionAtLeast(4, 3) || SDL_GL_ExtensionSupported("GL_ARB_multi_draw_indirect")) {
		qbool all_available = true;

		GL_LoadMandatoryFunctionExtension(glMultiDrawArraysIndirect, all_available);
		GL_LoadMandatoryFunctionExtension(glMultiDrawElementsIndirect, all_available);

		glConfig.supported_features |= (all_available ? R_SUPPORT_INDIRECT_RENDERING : 0);
	}

	if (GL_VersionAtLeast(4, 2) || SDL_GL_ExtensionSupported("GL_ARB_base_instance")) {
		qbool all_available = true;

		GL_LoadMandatoryFunctionExtension(glDrawArraysInstancedBaseInstance, all_available);
		GL_LoadMandatoryFunctionExtension(glDrawElementsInstancedBaseInstance, all_available);
		GL_LoadMandatoryFunctionExtension(glDrawElementsInstancedBaseVertexBaseInstance, all_available);

		glConfig.supported_features |= (all_available ? R_SUPPORT_INSTANCED_RENDERING : 0);
	}

	// GL 4.0 core: indirect draw and UBO binding functions
	if (GL_VersionAtLeast(4, 0)) {
		GL_LoadOptionalFunction(glDrawElementsIndirect);
		GL_LoadOptionalFunction(glDrawArraysIndirect);
	}
	if (GL_VersionAtLeast(3, 1)) {
		GL_LoadOptionalFunction(glUniformBlockBinding);
		GL_LoadOptionalFunction(glGetUniformBlockIndex);
	}

	// Draw functions used for modern & classic
	GL_LoadOptionalFunction(glMultiDrawArrays);
	GL_LoadOptionalFunction(glMultiDrawElements);

	// Use this instead of glDrawArrays() if on particular drivers (see github bug #416)
	if (GL_Available(glMultiDrawArrays) && glConfig.amd_issues) {
		glConfig.broken_features |= R_BROKEN_PREFERMULTIDRAW;
	}

	if (GL_VersionAtLeast(3, 2) || SDL_GL_ExtensionSupported("GL_ARB_draw_elements_base_vertex")) {
		GL_LoadOptionalFunction(glDrawElementsBaseVertex);
	}

	glConfig.supported_features &= ~R_SUPPORT_PRIMITIVERESTART;
	if (R_UseModernOpenGL() || GL_VersionAtLeast(3, 1)) {
		GL_LoadOptionalFunction(glPrimitiveRestartIndex);
		if (GL_Available(glPrimitiveRestartIndex)) {
			glEnable(GL_PRIMITIVE_RESTART);
			if (GL_VersionAtLeast(4, 3)) {
				glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
			}
			else {
				GL_Procedure(glPrimitiveRestartIndex, (~(GLuint)0));
			}
			glConfig.supported_features |= R_SUPPORT_PRIMITIVERESTART;
		}
	}
}

// Wrappers around drawing functions
void GL_MultiDrawArrays(GLenum mode, GLint* first, GLsizei* count, GLsizei primcount)
{
	if (GL_Available(glMultiDrawArrays)) {
		GL_Procedure(glMultiDrawArrays, mode, first, count, primcount);
		++frameStats.draw_calls;
		frameStats.subdraw_calls += primcount;
	}
	else {
		int i;
		for (i = 0; i < primcount; ++i) {
			GL_DrawArrays(mode, first[i], count[i]);
		}
	}
}

void GL_DrawArrays(GLenum mode, GLint first, GLsizei count)
{
	R_TraceLogAPICall("glDrawArrays(%d verts)", count);
	assert(R_VAOBound());
	if (!R_VAOBound()) {
		Con_Printf("GL_DrawArrays() with no VAO bound\n");
		return;
	}
	if (GL_WorkaroundNeeded(R_BROKEN_PREFERMULTIDRAW) && GL_Available(glMultiDrawArrays)) {
		GL_Procedure(glMultiDrawArrays, mode, &first, &count, 1);
	}
	else {
		GL_BuiltinProcedure(glDrawArrays, "(mode=%u, first=%d, count=%d)", mode, first, count);
	}
	++frameStats.draw_calls;
}

void GL_DrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, GLvoid* indices, GLint basevertex)
{
	if (basevertex && !GL_Available(glDrawElementsBaseVertex)) {
		Sys_Error("glDrawElementsBaseVertex called, not supported");
	}
	else if (GL_Available(glDrawElementsBaseVertex)) {
		GL_Procedure(glDrawElementsBaseVertex, mode, count, type, indices, basevertex);
	}
	else {
		GL_BuiltinProcedure(glDrawElements, "mode=%u, count=%d, type=%u, indices=%p", mode, count, type, indices);
	}
	++frameStats.draw_calls;
}

qbool GL_DrawElementsBaseVertexAvailable(void)
{
	return GL_Available(glDrawElementsBaseVertex);
}

void GL_DrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices)
{
	R_TraceLogAPICall("glDrawElements(%d verts)", count);
	glDrawElements(mode, count, type, indices);
	++frameStats.draw_calls;
}

void GL_MultiDrawArraysIndirect(GLenum mode, const void* indirect, GLsizei drawcount, GLsizei stride)
{
	if (GL_Available(glMultiDrawArraysIndirect)) {
		GL_Procedure(glMultiDrawArraysIndirect, mode, indirect, drawcount, stride);
	}
	else if (GL_Available(glDrawArraysIndirect)) {
		int actual_stride = stride ? stride : (4 * sizeof(GLuint));
		int i;
		for (i = 0; i < drawcount; i++) {
			GL_Procedure(glDrawArraysIndirect, mode, (const char*)indirect + i * actual_stride);
		}
	}
	++frameStats.draw_calls;
	frameStats.subdraw_calls += drawcount;
}

void GL_MultiDrawElementsIndirect(GLenum mode, GLenum type, const void* indirect, GLsizei drawcount, GLsizei stride)
{
	if (GL_Available(glMultiDrawElementsIndirect)) {
		GL_Procedure(glMultiDrawElementsIndirect, mode, type, indirect, drawcount, stride);
	}
	else if (GL_Available(glDrawElementsIndirect)) {
		int actual_stride = stride ? stride : (5 * sizeof(GLuint));
		int i;
		for (i = 0; i < drawcount; i++) {
			GL_Procedure(glDrawElementsIndirect, mode, type, (const char*)indirect + i * actual_stride);
		}
	}
	++frameStats.draw_calls;
	frameStats.subdraw_calls += drawcount;
}

void GL_DrawElementsIndirect(GLenum mode, GLenum type, GLvoid* indirect)
{
	GL_Procedure(glDrawElementsIndirect, mode, type, indirect);
	++frameStats.draw_calls;
}

void GL_DrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count, GLenum type, GLvoid* indices, GLsizei primcount, GLint basevertex, GLuint baseinstance)
{
	if (GL_Available(glDrawElementsInstancedBaseVertexBaseInstance)) {
		GL_Procedure(glDrawElementsInstancedBaseVertexBaseInstance, mode, count, type, indices, primcount, basevertex, baseinstance);
	}
	else {
		GL_DrawElementsBaseVertex(mode, count, type, indices, basevertex);
	}
	++frameStats.draw_calls;
}

void GL_UniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding)
{
	GL_Procedure(glUniformBlockBinding, program, uniformBlockIndex, uniformBlockBinding);
}

GLuint GL_GetUniformBlockIndex(GLuint program, const GLchar *uniformBlockName)
{
	if (!GL_Available(glGetUniformBlockIndex)) {
		return GL_INVALID_INDEX;
	}
	return GL_Function(glGetUniformBlockIndex, program, uniformBlockName);
}
