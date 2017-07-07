
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