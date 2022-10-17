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
#ifdef RENDERER_OPTION_MODERN_OPENGL

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "r_aliasmodel_md3.h"
#include "r_aliasmodel.h"
#include "vx_vertexlights.h" 
#include "r_matrix.h"
#include "rulesets.h"

void GLM_DrawAlias3Model(entity_t* ent, qbool outline, qbool additive_pass)
{
	extern void R_AliasSetupLighting(entity_t* ent);

	float lerpfrac = 1;
	float oldMatrix[16];
	int v1, v2;

	model_t* mod = ent->model;
	md3model_t* md3Model = (md3model_t *)Mod_Extradata(mod);
	surfinf_t* surfaceInfo = MD3_ExtraSurfaceInfoForModel(md3Model);
	md3Header_t* pHeader = MD3_HeaderForModel(md3Model);
	md3Surface_t* surf;
	int frame1, frame2, surfnum, vertsPerFrame = 0;

	MD3_ForEachSurface(pHeader, surf, surfnum) {
		vertsPerFrame += 3 * surf->numTriangles;
	}

	if (ent->skinnum >= 0 && ent->skinnum < pHeader->numSkins) {
		surfaceInfo += ent->skinnum * pHeader->numSurfaces;
	}

	R_PushModelviewMatrix(oldMatrix);
	R_AliasModelPrepare(ent, pHeader->numFrames, &frame1, &frame2, &lerpfrac, &outline);

	v1 = mod->vbo_start + vertsPerFrame * frame1;
	v2 = mod->vbo_start + vertsPerFrame * frame2;
	MD3_ForEachSurface(pHeader, surf, surfnum) {
		// FIXME: hack for not reading different shader types
		int extra_fx = ((mod->modhint & MOD_VMODEL) && surfnum >= 1 ? RF_ADDITIVEBLEND : 0);

		GLM_DrawAliasModelFrame(
			ent, mod, v1, v2, 3 * surf->numTriangles,
			surfaceInfo[surfnum].texnum, outline, ent->effects, ent->renderfx | extra_fx, lerpfrac
		);
		v1 += 3 * surf->numTriangles;
		v2 += 3 * surf->numTriangles;
	}

	R_PopModelviewMatrix(oldMatrix);
}

#endif // #ifdef RENDERER_OPTION_MODERN_OPENGL
