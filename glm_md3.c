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
	extern byte	*shadedots;
	extern byte	r_avertexnormal_dots[SHADEDOT_QUANT][NUMVERTEXNORMALS];
	extern void R_AliasSetupLighting(entity_t* ent);

	float lerpfrac = 1;
	float oldMatrix[16];
	int v1, v2;

	int frame1 = ent->oldframe, frame2 = ent->frame;
	model_t* mod = ent->model;
	md3model_t* md3Model = (md3model_t *)Mod_Extradata(mod);
	surfinf_t* surfaceInfo = MD3_ExtraSurfaceInfoForModel(md3Model);
	md3Header_t* pHeader = MD3_HeaderForModel(md3Model);
	int surfnum;
	md3Surface_t* surf;

	int vertsPerFrame = 0;
	MD3_ForEachSurface(pHeader, surf, surfnum) {
		vertsPerFrame += 3 * surf->numTriangles;
	}

	if (ent->skinnum >= 0 && ent->skinnum < pHeader->numSkins) {
		surfaceInfo += ent->skinnum * pHeader->numSurfaces;
	}

	R_PushModelviewMatrix(oldMatrix);
	R_RotateForEntity(ent);
	if ((ent->renderfx & RF_WEAPONMODEL) && r_viewmodelsize.value < 1) {
		// perform scalling for r_viewmodelsize
		R_ScaleModelview(0.5 + bound(0, r_viewmodelsize.value, 1) / 2, 1, 1);
	}

	//
	ent->r_modelcolor[0] = -1;
	ent->r_modelalpha = ((ent->renderfx & RF_WEAPONMODEL) && gl_mtexable) ? bound(0, cl_drawgun.value, 1) : 1;
	if (ent->alpha) {
		ent->r_modelalpha = ent->alpha;
	}

	R_AliasSetupLighting(ent);
	shadedots = r_avertexnormal_dots[((int) (ent->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];

	frame1 = min(frame1, pHeader->numFrames - 1);
	frame2 = min(frame2, pHeader->numFrames - 1);

	if (!r_lerpframes.value || ent->framelerp < 0 || frame1 == frame2 || (frame2 != frame1 + 1 && frame2 != frame1 - 1)) {
		lerpfrac = 1.0f;
	}
	else {
		lerpfrac = min(ent->framelerp, 1.0f);
	}

	if (frame2 == frame1 - 1) {
		--frame1;
		++frame2;
		lerpfrac = 1.0f - lerpfrac;
	}

	if (lerpfrac == 1) {
		lerpfrac = 0;
		frame1 = frame2;
	}

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
