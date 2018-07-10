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

#ifndef EZQUAKE_GLC_MATRIX_HEADER
#define EZQUAKE_GLC_MATRIX_HEADER

void GLC_IdentityModelview(void);
void GLC_RotateModelview(float angle, float x, float y, float z);
void GLC_TranslateModelview(float x, float y, float z);
void GLC_PopModelviewMatrix(const float* matrix);
void GLC_LoadModelviewMatrix(void);

void GLC_IdentityProjectionView(void);
void GLC_OrthographicProjection(float left, float right, float top, float bottom, float zNear, float zFar);

void GLC_PopProjectionMatrix(const float* matrix);
void GLC_ScaleModelview(float xScale, float yScale, float zScale);
void GLC_Frustum(double left, double right, double bottom, double top, double zNear, double zFar);
void GLC_PauseMatrixUpdate(void);
void GLC_ResumeMatrixUpdate(void);
void GLC_BeginCausticsTextureMatrix(void);
void GLC_EndCausticsTextureMatrix(void);

#endif // EZQUAKE_GLC_MATRIX_HEADER
