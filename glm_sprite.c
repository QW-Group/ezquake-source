
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "tr_types.h"
#include "gl_billboards.h"

// For drawing sprites in 3D space

// FIXME: Now used by classic too.
void GLM_SpriteToBillboard(vec3_t origin, vec3_t up, vec3_t right, float scale, float s, float t)
{
	vec3_t points[4];

	VectorMA(origin, +scale, up, points[0]);
	VectorMA(points[0], -scale, right, points[0]);
	VectorMA(origin, -scale, up, points[1]);
	VectorMA(points[1], -scale, right, points[1]);
	VectorMA(origin, +scale, up, points[2]);
	VectorMA(points[2], +scale, right, points[2]);
	VectorMA(origin, -scale, up, points[3]);
	VectorMA(points[3], +scale, right, points[3]);

	GL_BillboardAddVert(BILLBOARD_ENTITIES, points[0][0], points[0][1], points[0][2], 0, 0, color_white);
	GL_BillboardAddVert(BILLBOARD_ENTITIES, points[1][0], points[1][1], points[1][2], 0, t, color_white);
	GL_BillboardAddVert(BILLBOARD_ENTITIES, points[2][0], points[2][1], points[2][2], s, 0, color_white);
	GL_BillboardAddVert(BILLBOARD_ENTITIES, points[3][0], points[3][1], points[3][2], s, t, color_white);
}

void GLM_DrawSimpleItem(texture_ref texture_array, int texture_index, float scale_s, float scale_t, vec3_t origin, float scale_, vec3_t up, vec3_t right)
{
	if (GL_BillboardAddEntrySpecific(BILLBOARD_ENTITIES, 4, texture_array, texture_index)) {
		GLM_SpriteToBillboard(origin, up, right, scale_, scale_s, scale_t);
	}
}

void GLM_DrawSpriteModel(entity_t* e)
{
	vec3_t right, up;
	mspriteframe_t *frame;
	msprite2_t *psprite;

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

	if (GL_BillboardAddEntrySpecific(BILLBOARD_ENTITIES, 4, frame->gl_arraynum, frame->gl_arrayindex)) {
		GLM_SpriteToBillboard(e->origin, up, right, 5, frame->gl_scalingS, frame->gl_scalingT);
	}
}

void GL_BeginDrawSprites(void)
{
}

void GL_EndDrawSprites(void)
{
}
