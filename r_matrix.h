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

#ifndef EZQUAKE_R_MATRIX_HEADER
#define EZQUAKE_R_MATRIX_HEADER

qbool R_Project3DCoordinates(float objx, float objy, float objz, float* winx, float* winy, float* winz);

void GL_GetModelviewMatrix(float* matrix);
void GL_GetProjectionMatrix(float* matrix);
void GL_GetViewport(int* view);

void GL_IdentityProjectionView(void);
void GL_IdentityModelView(void);
void GL_RotateModelview(float angle, float x, float y, float z);
void GL_ScaleModelview(float xScale, float yScale, float zScale);
void GL_TranslateModelview(float x, float y, float z);
void GL_Frustum(double left, double right, double bottom, double top, double zNear, double zFar);
void GL_OrthographicProjection(float left, float right, float top, float bottom, float zNear, float zFar);

void GL_PushModelviewMatrix(float* matrix);
void GL_PopModelviewMatrix(const float* matrix);
void GL_PushProjectionMatrix(float* matrix);
void GL_PopProjectionMatrix(const float* matrix);

void GLM_ScaleMatrix(float* matrix, float x_scale, float y_scale, float z_scale);
void GLM_TransformMatrix(float* matrix, float x, float y, float z);
void GLM_RotateMatrix(float* matrix, float angle, float x, float y, float z);
void GLM_RotateVector(vec3_t vector, float angle, float x, float y, float z);

#endif // EZQUAKE_R_MATRIX_HEADER
