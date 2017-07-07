
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"

void GLC_DrawSimpleItem(int simpletexture, vec3_t org, float sprsize, vec3_t up, vec3_t right)
{
	vec3_t point;

	glPushAttrib(GL_ENABLE_BIT);

	glDisable(GL_CULL_FACE);
	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);

	GL_Bind(simpletexture);

	glBegin (GL_QUADS);

	glTexCoord2f (0, 1);
	VectorMA (org, -sprsize, up, point);
	VectorMA (point, -sprsize, right, point);
	glVertex3fv (point);

	glTexCoord2f (0, 0);
	VectorMA (org, sprsize, up, point);
	VectorMA (point, -sprsize, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 0);
	VectorMA (org, sprsize, up, point);
	VectorMA (point, sprsize, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 1);
	VectorMA (org, -sprsize, up, point);
	VectorMA (point, sprsize, right, point);
	glVertex3fv (point);

	glEnd ();

	glPopAttrib();
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

	GL_Bind(frame->gl_texturenum);

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

void GLC_DrawBillboard(ci_texture_t* _ptex, ci_player_t* _p, vec3_t _coord[4])
{
	float matrix[16];

	GL_PushMatrix(GL_MODELVIEW, matrix);
	GL_Translate(GL_MODELVIEW, _p->org[0], _p->org[1], _p->org[2]);
	GL_Scale(GL_MODELVIEW, _p->size, _p->size, _p->size);
	if (_p->rotangle) {
		GL_Rotate(GL_MODELVIEW, _p->rotangle, vpn[0], vpn[1], vpn[2]);
	}

	glColor4ubv(_p->color);

	glBegin(GL_QUADS);
	glTexCoord2f(_ptex->coords[_p->texindex][0], _ptex->coords[_p->texindex][3]);
	glVertex3fv(_coord[0]);
	glTexCoord2f(_ptex->coords[_p->texindex][0], _ptex->coords[_p->texindex][1]);
	glVertex3fv(_coord[1]);
	glTexCoord2f(_ptex->coords[_p->texindex][2], _ptex->coords[_p->texindex][1]);
	glVertex3fv(_coord[2]);
	glTexCoord2f(_ptex->coords[_p->texindex][2], _ptex->coords[_p->texindex][3]);
	glVertex3fv(_coord[3]);
	glEnd();

	GL_PopMatrix(GL_MODELVIEW, matrix);
}
