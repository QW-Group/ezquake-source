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

void R_BeginRendering(int *x, int *y, int *width, int *height);
void R_EndRendering(void);
void R_Set2D(void);
void R_PolyBlend(void);
void R_OnDisconnect(void);
void R_StateDefault3D(void);

// 2d rendering
void R_FlushImageDraw(void);
void R_EmptyImageQueue(void);

// culling
qbool R_CullBox(vec3_t mins, vec3_t maxs);
qbool R_CullSphere(vec3_t centre, float radius);

// vis
extern refdef_t	r_refdef;
extern struct mleaf_s* r_viewleaf;
extern struct mleaf_s* r_oldviewleaf;
extern struct mleaf_s* r_viewleaf2;
extern struct mleaf_s* r_oldviewleaf2;	// 2 is for watervis hack

// Using multiple renderers?
#ifdef EZ_MULTIPLE_RENDERERS
#undef EZ_MULTIPLE_RENDERERS
#endif

#if defined(RENDERER_OPTION_CLASSIC_OPENGL) && defined(RENDERER_OPTION_MODERN_OPENGL)
#define EZ_MULTIPLE_RENDERERS

extern cvar_t vid_renderer;
#define R_UseImmediateOpenGL()    (vid_renderer.integer == 0)
#define R_UseModernOpenGL()       (vid_renderer.integer == 1)
#define R_UseVulkan()             (vid_renderer.integer == 2)

void R_SelectRenderer(void);
#endif

#ifndef EZ_MULTIPLE_RENDERERS
#define R_SelectRenderer()
#if defined(RENDERER_OPTION_CLASSIC_OPENGL)
#define R_UseImmediateOpenGL()    (1)
#define R_UseModernOpenGL()       (0)
#define R_UseVulkan()             (0)
#elif defined(RENDERER_OPTION_MODERN_OPENGL)
#define R_UseImmediateOpenGL()    (0)
#define R_UseModernOpenGL()       (1)
#define R_UseVulkan()             (0)
#else
#error No renderer options defined
#endif
#endif

// Debug profile may or may not do anything, but if it does anything it's slower, so only enable in dev mode
#define R_DebugProfileContext()  ((IsDeveloperMode() && COM_CheckParm(cmdline_param_client_video_r_debug)) || COM_CheckParm(cmdline_param_client_video_r_trace))
#define R_CompressFullbrightTextures() (!R_UseImmediateOpenGL())
#define R_LumaTexturesMustMatchDimensions() (!R_UseImmediateOpenGL())
#define R_UseCubeMapForSkyBox() (!R_UseImmediateOpenGL() || GL_Supported(R_SUPPORT_SEAMLESS_CUBEMAPS))

// bloom.c
void R_InitBloomTextures(void);
void R_BloomBlend(void);

// r_draw.c (here for atlas)
#define NUMCROSSHAIRS 6

// r_main.c
void R_NewMapPrepare(qbool vid_restart);
void R_Shutdown(qbool restart);
void VID_GfxInfo_f(void);
int VID_DisplayNumber(qbool fullscreen);

// Shorthand
#define ISUNDERWATER(contents) (contents == CONTENTS_WATER || contents == CONTENTS_SLIME || contents == CONTENTS_LAVA)
//#define TruePointContents(p) CM_HullPointContents(&cl.worldmodel->hulls[0], 0, p)
#define TruePointContents(p) CM_HullPointContents(&cl.clipmodels[1]->hulls[0], 0, p) // ?TONIK?

extern int r_framecount;

// palette
void Check_Gamma(unsigned char *pal);
void VID_SetPalette(unsigned char *palette);
qbool R_OldGammaBehaviour(void);

void R_Initialise(void);
float R_WaterAlpha(void);

// 3d rendering limits
#define R_MINIMUM_NEARCLIP (2.0f)
#define R_MAXIMUM_NEARCLIP (4.0f)
#define R_MINIMUM_FARCLIP (4096.0f)
#define R_MAXIMUM_FARCLIP (16384.0f)
float R_FarPlaneZ(void);
float R_NearPlaneZ(void);

#endif // EZQUAKE_R_LOCAL_HEADER
