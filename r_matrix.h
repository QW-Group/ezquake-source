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

void R_GetModelviewMatrix(float* matrix);
void R_GetProjectionMatrix(float* matrix);
void R_GetViewport(int* view);

void R_IdentityProjectionView(void);
void R_IdentityModelView(void);
void R_RotateModelview(float angle, float x, float y, float z);
void R_ScaleModelview(float xScale, float yScale, float zScale);
void R_TranslateModelview(float x, float y, float z);
void R_Frustum(double left, double right, double bottom, double top, double zNear, double zFar);
void R_OrthographicProjection(float left, float right, float top, float bottom, float zNear, float zFar);

void R_PushModelviewMatrix(float* matrix);
void R_PopModelviewMatrix(const float* matrix);
void R_PushProjectionMatrix(float* matrix);
void R_PopProjectionMatrix(const float* matrix);

void R_ScaleMatrix(float* matrix, float x_scale, float y_scale, float z_scale);
void R_TransformMatrix(float* matrix, float x, float y, float z);
void R_RotateMatrix(float* matrix, float angle, float x, float y, float z);
void R_RotateVector(vec3_t vector, float angle, float x, float y, float z);

void R_SetIdentityMatrix(float* matrix);
float* R_ModelviewMatrix(void);
float* R_ProjectionMatrix(void);

void R_MultiplyMatrix(const float* lhs, const float* rhs, float* target);
void R_MultiplyVector(const float* matrix, const float* vector, float* result);
void R_MultiplyVector3f(const float* matrix, float x, float y, float z, float* result);
void R_MultiplyVector3fv(const float* matrix, const vec3_t vector, float* result);

void R_RotateForEntity(const struct entity_s* e);

#endif // EZQUAKE_R_MATRIX_HEADER
