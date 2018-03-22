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

#ifndef EZQUAKE_R_LOCAL_HEADER
#define EZQUAKE_R_LOCAL_HEADER

extern int glx, gly, glwidth, glheight;

void GL_BeginRendering(int *x, int *y, int *width, int *height);
void GL_EndRendering(void);
void GL_Set2D(void);
void R_PolyBlend(void);
void R_EnsureFinished(void);
void R_PreRenderView(void);
void R_OnDisconnect(void);

// 2d rendering
void GL_FlushImageDraw(void);
void GL_EmptyImageQueue(void);

// culling
qbool R_CullBox(vec3_t mins, vec3_t maxs);
qbool R_CullSphere(vec3_t centre, float radius);

// buffers
void R_BufferStartFrame(void);
void R_BufferEndFrame(void);
qbool R_BuffersReady(void);

// fog
void R_AddWaterfog(int contents);

#endif // EZQUAKE_R_LOCAL_HEADER
