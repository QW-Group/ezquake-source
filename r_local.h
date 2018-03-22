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

typedef struct gl_buffer_s {
	int index;
} buffer_ref;

qbool GL_BufferValid(buffer_ref buffer);
#define GL_BufferReferenceIsValid(x) (x.index && GL_BufferValid(x))
extern const buffer_ref null_buffer_reference;

// fog
void R_AddWaterfog(int contents);

// vis
extern refdef_t	r_refdef;
extern struct mleaf_s* r_viewleaf;
extern struct mleaf_s* r_oldviewleaf;
extern struct mleaf_s* r_viewleaf2;
extern struct mleaf_s* r_oldviewleaf2;	// 2 is for watervis hack

// Which renderer to use
extern cvar_t vid_renderer;
#define R_UseImmediateOpenGL()    (vid_renderer.integer == 0)
#define R_UseModernOpenGL()       (vid_renderer.integer == 1)
#define R_UseVulkan()             (vid_renderer.integer == 2)

#endif // EZQUAKE_R_LOCAL_HEADER
