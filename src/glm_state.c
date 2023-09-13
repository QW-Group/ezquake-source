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

#include "quakedef.h"
#include "r_local.h"
#include "r_trace.h"
#include "r_state.h"

void GLM_BeginDrawWorld(qbool alpha_surfaces, qbool polygon_offset)
{
	if (alpha_surfaces && polygon_offset) {
		R_ApplyRenderingState(r_state_alpha_surfaces_offset_glm);
	}
	else if (alpha_surfaces) {
		R_ApplyRenderingState(r_state_alpha_surfaces_glm);
	}
	else if (polygon_offset) {
		R_ApplyRenderingState(r_state_opaque_surfaces_offset_glm);
	}
	else {
		R_ApplyRenderingState(r_state_opaque_surfaces_glm);
	}
}

void GLM_StateBeginDraw3DSprites(void)
{
}

void GLM_StateBeginPolyBlend(void)
{
	R_ApplyRenderingState(r_state_poly_blend);
}

void GLM_StateBeginImageDraw(void)
{
	R_ApplyRenderingState(r_state_hud_images_glm);
}

void GLM_StateBeginAliasOutlineBatch(void)
{
	R_ApplyRenderingState(r_state_aliasmodel_outline);
}

void GLM_StateBeginAliasModelBatch(qbool translucent, qbool additive)
{
	if (additive) {
		R_ApplyRenderingState(r_state_aliasmodel_additive_batch);
	}
	else if (translucent) {
		R_ApplyRenderingState(r_state_aliasmodel_translucent_batch);
	}
	else {
		R_ApplyRenderingState(r_state_aliasmodel_opaque_batch);
	}
}

void GLM_StateBeginAliasModelZPassBatch(void)
{
	R_ApplyRenderingState(r_state_aliasmodel_translucent_batch_zpass);
}
