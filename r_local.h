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

#include "r_buffers.h"

extern int glx, gly, glwidth, glheight;

void GL_BeginRendering(int *x, int *y, int *width, int *height);
void GL_EndRendering(void);
void GL_Set2D(void);
void R_PolyBlend(void);
void R_EnsureFinished(void);
void R_PreRenderView(void);
void R_OnDisconnect(void);

// 2d rendering
void R_FlushImageDraw(void);
void R_EmptyImageQueue(void);

// culling
qbool R_CullBox(vec3_t mins, vec3_t maxs);
qbool R_CullSphere(vec3_t centre, float radius);

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

// Debug profile may or may not do anything, but if it does anything it's slower, so only enable in dev mode
#define R_DebugProfileContext()  (IsDeveloperMode() && COM_CheckParm(cmdline_param_client_video_gl_debug))

// textures
void R_TextureUnitBind(int unit, texture_ref texture);

// bloom.c
void R_InitBloomTextures(void);
void R_BloomBlend(void);

// r_draw.c (here for atlas)
#define NUMCROSSHAIRS 6

#endif // EZQUAKE_R_LOCAL_HEADER
