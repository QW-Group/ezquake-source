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

#ifdef RENDERER_OPTION_MODERN_OPENGL

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "tr_types.h"
#include "gl_sprite3d.h"

// For drawing sprites in 3D space
void GLM_DrawSimpleItem(model_t* m, int skin, vec3_t origin, float scale_, vec3_t up, vec3_t right)
{
	texture_ref texture_array = m->simpletexture_array[skin];
	int texture_index = m->simpletexture_indexes[skin];
	float scale_s = m->simpletexture_scalingS[skin];
	float scale_t = m->simpletexture_scalingT[skin];
	r_sprite3d_vert_t* vert;

	vert = R_Sprite3DAddEntrySpecific(SPRITE3D_ENTITIES, 4, texture_array, texture_index);
	if (vert) {
		R_Sprite3DRender(vert, origin, up, right, scale_, -scale_, -scale_, scale_, scale_s, scale_t, texture_index);
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

	// Check for null texture after s_null sprite found
	if (!frame || !R_TextureReferenceIsValid(frame->gl_arraynum)) {
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

	vert = R_Sprite3DAddEntrySpecific(SPRITE3D_ENTITIES, 4, frame->gl_arraynum, frame->gl_arrayindex);
	if (vert) {
		R_Sprite3DRender(vert, e->origin, up, right, frame->up, frame->down, frame->left, frame->right, frame->gl_scalingS, frame->gl_scalingT, frame->gl_arrayindex);
	}
}

void GL_BeginDrawSprites(void)
{
}

void GL_EndDrawSprites(void)
{
}

#endif // #ifdef RENDERER_OPTION_MODERN_OPENGL
