
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "gl_billboards.h"

extern void GLM_SpriteToBillboard(vec3_t origin, vec3_t up, vec3_t right, float scale, float s, float t);

void GLC_DrawSimpleItem(texture_ref simpletexture, vec3_t org, float sprsize, vec3_t up, vec3_t right)
{
	if (GL_BillboardAddEntrySpecific(BILLBOARD_ENTITIES, 4, simpletexture, 0)) {
		GLM_SpriteToBillboard(org, up, right, sprsize, 1, 1);
	}
}

void GLC_DrawSpriteModel(entity_t* e)
{
	vec3_t right, up;
	mspriteframe_t *frame;
	msprite2_t *psprite;

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

	if (GL_BillboardAddEntrySpecific(BILLBOARD_ENTITIES, 4, frame->gl_texturenum, 0)) {
		vec3_t points[4];

		VectorMA(e->origin, frame->up, up, points[0]);
		VectorMA(points[0], frame->left, right, points[0]);
		VectorMA(e->origin, frame->down, up, points[1]);
		VectorMA(points[1], frame->left, right, points[1]);
		VectorMA(e->origin, frame->up, up, points[2]);
		VectorMA(points[2], frame->right, right, points[2]);
		VectorMA(e->origin, frame->down, up, points[3]);
		VectorMA(points[3], frame->right, right, points[3]);

		GL_BillboardAddVert(BILLBOARD_ENTITIES, points[0][0], points[0][1], points[0][2], 0, 0, color_white);
		GL_BillboardAddVert(BILLBOARD_ENTITIES, points[1][0], points[1][1], points[1][2], 0, 1, color_white);
		GL_BillboardAddVert(BILLBOARD_ENTITIES, points[2][0], points[2][1], points[2][2], 1, 0, color_white);
		GL_BillboardAddVert(BILLBOARD_ENTITIES, points[3][0], points[3][1], points[3][2], 1, 1, color_white);
	}
}
