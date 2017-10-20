
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"
#include "utils.h"

/*

Todo list:

[ ] turbripple effect
[ ] transparent water with r_fastturb

*/

void GLM_DrawIndexedTurbPoly(GLuint vao, GLushort* indices, int count, texture_t* texture);
byte* SurfaceFlatTurbColor(texture_t* texture);

void GLM_DrawWaterSurfaces(void)
{
	float wateralpha = bound((1 - r_refdef2.max_watervis), r_wateralpha.value, 1);
	extern msurface_t* waterchain;

	msurface_t *s;
	GLsizei count;
	GLushort indices[4096];
	texture_t* current_texture = NULL;
	int v;

	count = 0;
	GL_SelectTexture(GL_TEXTURE0);
	if (wateralpha < 1.0 && wateralpha >= 0) {
		GL_AlphaBlendFlags(GL_BLEND_ENABLED);
		GL_TextureEnvMode(GL_MODULATE);
		if (wateralpha < 0.9) {
			glDepthMask(GL_FALSE);
		}
	}

	for (s = waterchain; s; s = s->texturechain) {
		glpoly_t* poly;
		if (current_texture != s->texinfo->texture) {
			if (count && current_texture) {
				GL_Bind(current_texture->gl_texturenum);
				GLM_DrawIndexedTurbPoly(cl.worldmodel->vao, indices, count, current_texture);
				count = 0;
			}
			current_texture = s->texinfo->texture;
		}

		for (poly = s->polys; poly; poly = poly->next) {
			int newVerts = poly->numverts;

			if (count + 2 + newVerts > sizeof(indices) / sizeof(indices[0])) {
				GL_Bind(current_texture->gl_texturenum);
				GLM_DrawIndexedTurbPoly(cl.worldmodel->vao, indices, count, current_texture);
				count = 0;
			}

			if (count) {
				int prev = count - 1;

				indices[count++] = indices[prev];
				indices[count++] = poly->vbo_start;
			}
			for (v = 0; v < newVerts; ++v) {
				indices[count++] = poly->vbo_start + v;
			}
		}
	}
	if (count) {
		GL_Bind(current_texture->gl_texturenum);
		GLM_DrawIndexedTurbPoly(cl.worldmodel->vao, indices, count, current_texture);
		count = 0;
	}

	if (wateralpha < 1.0 && wateralpha >= 0) {
		GL_TextureEnvMode(GL_REPLACE);
		GL_AlphaBlendFlags(GL_BLEND_DISABLED);
		if (wateralpha < 0.9) {
			glDepthMask(GL_TRUE);
		}
	}
}

void GLM_DrawIndexedTurbPoly(GLuint vao, GLushort* indices, int count, texture_t* texture)
{
	float wateralpha = bound((1 - r_refdef2.max_watervis), r_wateralpha.value, 1);
	byte* col = SurfaceFlatTurbColor(texture);
	extern cvar_t r_fastturb;

	if (r_fastturb.value) {
		byte old_alpha = col[3];

		col[3] = 255;
		GLM_DrawIndexedPolygonByType(GL_TRIANGLE_STRIP, col, vao, indices, count, false, false, false);
		col[3] = old_alpha;
	}
	else {
		GLM_DrawIndexedTurbPolys(vao, indices, count, wateralpha);
	}
}

