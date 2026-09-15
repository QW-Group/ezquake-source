
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

#ifdef RENDERER_OPTION_VULKAN

#include "quakedef.h"
#include "gl_model.h"
#include "r_aliasmodel.h"
#include "r_aliasmodel_md3.h"
#include "r_matrix.h"
#include "vk_local.h"

void VK_DrawAlias3Model(entity_t* ent, qbool outline, qbool additive_pass)
{
	float oldMatrix[16];
	float lerpfrac = 1.0f;
	int frame1;
	int frame2;
	int vertsPerFrame = 0;
	int v1;
	model_t* mod;
	md3model_t* md3Model;
	surfinf_t* surfaceInfo;
	md3Header_t* pHeader;
	md3Surface_t* surf;
	int surfnum;

	if (!ent || !ent->model) {
		return;
	}

	mod = ent->model;
	md3Model = (md3model_t*)Mod_Extradata(mod);
	pHeader = MD3_HeaderForModel(md3Model);
	surfaceInfo = MD3_ExtraSurfaceInfoForModel(md3Model);

	MD3_ForEachSurface(pHeader, surf, surfnum) {
		vertsPerFrame += 3 * surf->numTriangles;
	}

	if (ent->skinnum >= 0 && ent->skinnum < pHeader->numSkins) {
		surfaceInfo += ent->skinnum * pHeader->numSurfaces;
	}

	R_PushModelviewMatrix(oldMatrix);
	R_AliasModelPrepare(ent, pHeader->numFrames, &frame1, &frame2, &lerpfrac, &outline);

	v1 = mod->vbo_start + vertsPerFrame * frame1;
	MD3_ForEachSurface(pHeader, surf, surfnum) {
		int render_effects = ent->renderfx;

		if (additive_pass || ((mod->modhint & MOD_VMODEL) && surfnum >= 1)) {
			render_effects |= RF_ADDITIVEBLEND;
		}

		VK_AliasQueueDraw(ent, mod, v1, 3 * surf->numTriangles, surfaceInfo[surfnum].texnum, outline, ent->effects, render_effects, lerpfrac);
		v1 += 3 * surf->numTriangles;
	}

	(void)frame2;
	R_PopModelviewMatrix(oldMatrix);
}

#endif // #ifdef RENDERER_OPTION_VULKAN
