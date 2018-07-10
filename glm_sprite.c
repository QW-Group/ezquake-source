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
#include "gl_model.h"
#include "gl_local.h"
#include "tr_types.h"
#include "gl_sprite3d.h"

// For drawing sprites in 3D space

// FIXME: Now used by classic too.
void GLM_RenderSprite(r_sprite3d_vert_t* vert, vec3_t origin, vec3_t up, vec3_t right, float scale_up, float scale_down, float scale_left, float scale_right, float s, float t, int index)
{
	vec3_t points[4];

	VectorMA(origin, scale_up, up, points[0]);
	VectorMA(points[0], scale_left, right, points[0]);
	VectorMA(origin, scale_down, up, points[1]);
	VectorMA(points[1], scale_left, right, points[1]);
	VectorMA(origin, scale_up, up, points[2]);
	VectorMA(points[2], scale_right, right, points[2]);
	VectorMA(origin, scale_down, up, points[3]);
	VectorMA(points[3], scale_right, right, points[3]);

	GL_Sprite3DSetVert(vert++, points[0][0], points[0][1], points[0][2], 0, 0, color_white, index);
	GL_Sprite3DSetVert(vert++, points[1][0], points[1][1], points[1][2], 0, t, color_white, index);
	GL_Sprite3DSetVert(vert++, points[2][0], points[2][1], points[2][2], s, 0, color_white, index);
	GL_Sprite3DSetVert(vert++, points[3][0], points[3][1], points[3][2], s, t, color_white, index);
}

void GLM_DrawSimpleItem(texture_ref texture_array, int texture_index, float scale_s, float scale_t, vec3_t origin, float scale_, vec3_t up, vec3_t right)
{
	r_sprite3d_vert_t* vert = GL_Sprite3DAddEntrySpecific(SPRITE3D_ENTITIES, 4, texture_array, texture_index);
	if (vert) {
		GLM_RenderSprite(vert, origin, up, right, scale_, -scale_, -scale_, scale_, scale_s, scale_t, texture_index);
	}
}

void GLM_DrawSpriteModel(entity_t* e)
{
	vec3_t right, up;
	mspriteframe_t *frame;
	msprite2_t *psprite;
	r_sprite3d_vert_t* vert;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	psprite = (msprite2_t*)Mod_Extradata(e->model);	//locate the proper data
	frame = R_GetSpriteFrame(e, psprite);

	if (!frame) {
		return;
	}

	if (psprite->type == SPR_ORIENTED) {
		// bullet marks on walls
		AngleVectors(e->angles, NULL, right, up);
	}
	else if (psprite->type == SPR_FACING_UPRIGHT) {
		VectorSet(up, 0, 0, 1);
		right[0] = e->origin[1] - r_origin[1];
		right[1] = -(e->origin[0] - r_origin[0]);
		right[2] = 0;
		VectorNormalizeFast(right);
	}
	else if (psprite->type == SPR_VP_PARALLEL_UPRIGHT) {
		VectorSet(up, 0, 0, 1);
		VectorCopy(vright, right);
	}
	else {	// normal sprite
		VectorCopy(vup, up);
		VectorCopy(vright, right);
	}

	vert = GL_Sprite3DAddEntrySpecific(SPRITE3D_ENTITIES, 4, frame->gl_arraynum, frame->gl_arrayindex);
	if (vert) {
		GLM_RenderSprite(vert, e->origin, up, right, frame->up, frame->down, frame->left, frame->right, frame->gl_scalingS, frame->gl_scalingT, frame->gl_arrayindex);
	}
}

void GL_BeginDrawSprites(void)
{
}

void GL_EndDrawSprites(void)
{
}
