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

#ifndef R_BRUSHMODEL_HEADER
#define R_BRUSHMODEL_HEADER

#include "r_buffers.h"
#include "r_framestats.h"

qbool R_PointIsUnderwater(vec3_t point);

void GLC_StateBeginWaterSurfaces(void);
void GLC_StateBeginAlphaChain(void);
void GLC_StateBeginAlphaChainSurface(msurface_t* s);

void R_StateBrushModelBeginDraw(entity_t* e, qbool polygonOffset);

// gl_rsurf.c
void GLC_EmitDetailPolys(qbool use_vbo);
void R_BrushModelDrawEntity(entity_t *e);
void R_DrawWorld(void);
void GLC_DrawAlphaChain(msurface_t* alphachain, frameStatsPolyType polyType);

// gl_warp.c
void R_TurbSurfacesSubdivide(msurface_t *fa);
void R_SkySurfacesBuildPolys(msurface_t *fa);
void R_InitSky(texture_t *mt);	// called at level load
void R_DrawSky(void);
void R_AddSkyBoxSurface(msurface_t *fa);
qbool R_DrawWorldOutlines(void);

// internal
#define CHAIN_SURF_B2F(surf, chain) 			\
	{											\
		(surf)->texturechain = (chain);			\
		(chain) = (surf);						\
	}
void chain_surfaces_drawflat(msurface_t** chain_head, msurface_t* surf);
void chain_surfaces_by_lightmap(msurface_t** chain_head, msurface_t* surf);
extern unsigned int* modelIndexes;
extern unsigned int modelIndexMaximum;
void R_BrushModelCreateVBO(buffer_ref instance_vbo);

#endif // R_BRUSHMODEL_HEADER
