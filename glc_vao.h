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

#ifndef EZQUAKE_GLC_VAO_HEADER
#define EZQUAKE_GLC_VAO_HEADER

#include "gl_local.h"
#include "r_vao.h"

void GLC_VAOEnableVertexPointer(r_vao_id vao, int size, GLenum type, GLsizei stride, GLvoid* pointer);
void GLC_VAODisableVertexPointer(r_vao_id vao);
void GLC_VAOEnableColorPointer(r_vao_id vao, int size, GLenum type, GLsizei stride, GLvoid* pointer);
void GLC_VAODisableColorPointer(r_vao_id vao);
void GLC_VAOEnableTextureCoordPointer(r_vao_id vao, int index, int size, GLenum type, GLsizei stride, GLvoid* pointer);
void GLC_VAODisableTextureCoordPointer(r_vao_id vao, int index);
void GLC_VAOSetIndexBuffer(r_vao_id vao, buffer_ref ref);
void GLC_VAOSetVertexBuffer(r_vao_id vao, buffer_ref ref);

void GLC_DeleteVAOs(void);
void GLC_GenVertexArray(r_vao_id vao, const char* name);
qbool GLC_VertexArrayCreated(r_vao_id vao);
void GLC_BindVertexArray(r_vao_id vao);
qbool GLC_InitialiseVAOHandling(void);
void GLC_EnsureVAOCreated(r_vao_id vao);

#ifdef WITH_OPENGL_TRACE
void GLC_PrintVAOState(FILE* output, int indent, r_vao_id vao);
#endif

#endif // EZQUAKE_GLC_VAO_HEADER
