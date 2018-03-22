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
#include "gl_sprite3d.h"

void GLC_DrawSimpleItem(texture_ref simpletexture, vec3_t org, float sprsize, vec3_t up, vec3_t right)
{
	gl_sprite3d_vert_t* vert = GL_Sprite3DAddEntrySpecific(SPRITE3D_ENTITIES, 4, simpletexture, 0);
	if (vert) {
		GLM_RenderSprite(vert, org, up, right, sprsize, -sprsize, -sprsize, sprsize, 1, 1, 0);
	}
}

void GLC_DrawSpriteModel(entity_t* e)
{
	vec3_t right, up;
	mspriteframe_t *frame;
	msprite2_t *psprite;
	gl_sprite3d_vert_t* vert;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	psprite = (msprite2_t*)Mod_Extradata(e->model);	//locate the proper data
	frame = R_GetSpriteFrame(e, psprite);

	if (!frame)
		return;

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

	vert = GL_Sprite3DAddEntrySpecific(SPRITE3D_ENTITIES, 4, frame->gl_texturenum, 0);
	if (vert) {
		vec3_t points[4];

		VectorMA(e->origin, frame->up, up, points[0]);
		VectorMA(points[0], frame->left, right, points[0]);
		VectorMA(e->origin, frame->down, up, points[1]);
		VectorMA(points[1], frame->left, right, points[1]);
		VectorMA(e->origin, frame->up, up, points[2]);
		VectorMA(points[2], frame->right, right, points[2]);
		VectorMA(e->origin, frame->down, up, points[3]);
		VectorMA(points[3], frame->right, right, points[3]);

		GL_Sprite3DSetVert(vert++, points[0][0], points[0][1], points[0][2], 0, 0, color_white, 0);
		GL_Sprite3DSetVert(vert++, points[1][0], points[1][1], points[1][2], 0, 1, color_white, 0);
		GL_Sprite3DSetVert(vert++, points[2][0], points[2][1], points[2][2], 1, 0, color_white, 0);
		GL_Sprite3DSetVert(vert++, points[3][0], points[3][1], points[3][2], 1, 1, color_white, 0);
	}
}
