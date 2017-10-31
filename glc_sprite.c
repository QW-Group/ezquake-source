
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"

void GLC_DrawSimpleItem(texture_ref simpletexture, vec3_t org, float sprsize, vec3_t up, vec3_t right)
{
	vec3_t point;
	int oldFlags = GL_AlphaBlendFlags(GL_ALPHATEST_NOCHANGE | GL_BLEND_NOCHANGE);

	GLC_InitTextureUnits1(simpletexture, GL_REPLACE);

	glBegin(GL_QUADS);
	glTexCoord2f(0, 1);
	VectorMA(org, -sprsize, up, point);
	VectorMA(point, -sprsize, right, point);
	glVertex3fv(point);

	glTexCoord2f(0, 0);
	VectorMA(org, sprsize, up, point);
	VectorMA(point, -sprsize, right, point);
	glVertex3fv(point);

	glTexCoord2f(1, 0);
	VectorMA(org, sprsize, up, point);
	VectorMA(point, sprsize, right, point);
	glVertex3fv(point);

	glTexCoord2f(1, 1);
	VectorMA(org, -sprsize, up, point);
	VectorMA(point, sprsize, right, point);
	glVertex3fv(point);
	glEnd();
}

void GLC_DrawSpriteModel(entity_t* e)
{
	vec3_t point, right, up;
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

	GL_EnsureTextureUnitBound(GL_TEXTURE0, frame->gl_texturenum);

	glBegin(GL_QUADS);

	glTexCoord2f(0, 1);
	VectorMA(e->origin, frame->down, up, point);
	VectorMA(point, frame->left, right, point);
	glVertex3fv(point);

	glTexCoord2f(0, 0);
	VectorMA(e->origin, frame->up, up, point);
	VectorMA(point, frame->left, right, point);
	glVertex3fv(point);

	glTexCoord2f(1, 0);
	VectorMA(e->origin, frame->up, up, point);
	VectorMA(point, frame->right, right, point);
	glVertex3fv(point);

	glTexCoord2f(1, 1);
	VectorMA(e->origin, frame->down, up, point);
	VectorMA(point, frame->right, right, point);
	glVertex3fv(point);

	glEnd();
}
