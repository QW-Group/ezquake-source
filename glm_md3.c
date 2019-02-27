/*
Copyright (C) 2011 fuh and ezQuake team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Code to display .md3 files

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "r_aliasmodel_md3.h"
#include "r_aliasmodel.h"
#include "vx_vertexlights.h" 
#include "r_matrix.h"

void GLM_DrawAlias3Model(entity_t* ent)
{
	extern cvar_t cl_drawgun, r_viewmodelsize, r_lerpframes;
	extern byte	r_avertexnormal_dots[SHADEDOT_QUANT][NUMVERTEXNORMALS];
	extern void R_AliasSetupLighting(entity_t* ent);

	float lerpfrac = 1;
	float oldMatrix[16];
	int v1, v2;

	model_t* mod = ent->model;
	md3model_t* md3Model = (md3model_t *)Mod_Extradata(mod);
	surfinf_t* surfaceInfo = MD3_ExtraSurfaceInfoForModel(md3Model);
	md3Header_t* pHeader = MD3_HeaderForModel(md3Model);
	md3Surface_t* surf;
	int frame1 = bound(0, ent->oldframe, pHeader->numFrames - 1);
	int frame2 = bound(0, ent->frame, pHeader->numFrames - 1);
	int surfnum;
	int vertsPerFrame = 0;

	MD3_ForEachSurface(pHeader, surf, surfnum) {
		vertsPerFrame += 3 * surf->numTriangles;
	}

	if (ent->skinnum >= 0 && ent->skinnum < pHeader->numSkins) {
		surfaceInfo += ent->skinnum * pHeader->numSurfaces;
	}

	R_PushModelviewMatrix(oldMatrix);
	R_AliasModelPrepare(ent, &frame1, &frame2, &lerpfrac);

	v1 = mod->vbo_start + vertsPerFrame * frame1;
	v2 = mod->vbo_start + vertsPerFrame * frame2;
	MD3_ForEachSurface(pHeader, surf, surfnum) {
		GLM_DrawAliasModelFrame(
			ent, mod, v1, v2, 3 * surf->numTriangles,
			surfaceInfo[surfnum].texnum, false, ent->effects, ent->renderfx, lerpfrac
		);
		v1 += 3 * surf->numTriangles;
		v2 += 3 * surf->numTriangles;
	}

	R_PopModelviewMatrix(oldMatrix);
}
