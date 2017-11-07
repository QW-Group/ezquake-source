
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

void GLM_DrawIndexedTurbPoly(GLuint vao, GLuint* indices, int count, byte* colour, GLuint texture_array);
byte* SurfaceFlatTurbColor(texture_t* texture);

void GLM_DrawWaterSurfaces(void)
{
	float wateralpha = bound((1 - r_refdef2.max_watervis), r_wateralpha.value, 1);
	extern msurface_t* waterchain;

	msurface_t *s;
	GLsizei count;
	GLuint indices[4096];
	texture_t* current_texture = NULL;
	int v;
	GLuint current_texture_array = 0;
	byte current_colour[4];

	extern cvar_t r_fastturb;

	count = 0;
	GL_SelectTexture(GL_TEXTURE0);
	if (wateralpha < 1.0 && wateralpha >= 0) {
		GL_AlphaBlendFlags(GL_BLEND_ENABLED);
		GL_TextureEnvMode(GL_MODULATE);
		if (wateralpha < 0.9) {
			glDepthMask(GL_FALSE);
		}
	}
	else {
		GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	}

	for (s = waterchain; s; s = s->texturechain) {
		glpoly_t* poly;
		qbool tex_array_change;
		qbool tex_color_change;
		byte* col = SurfaceFlatTurbColor(s->texinfo->texture);

		current_texture = s->texinfo->texture;
		tex_array_change = (!r_fastturb.integer && current_texture->gl_texture_array != current_texture_array);
		tex_color_change = (r_fastturb.integer && memcmp(col, current_colour, 4));

		if (tex_array_change || tex_color_change) {
			if (count) {
				GLM_DrawIndexedTurbPoly(cl.worldmodel->vao.vao, indices, count, current_colour, current_texture_array);
				count = 0;
			}
			current_texture_array = s->texinfo->texture->gl_texture_array;
			memcpy(current_colour, col, 4);
		}

		for (poly = s->polys; poly; poly = poly->next) {
			int newVerts = poly->numverts;

			if (count + 3 + newVerts > sizeof(indices) / sizeof(indices[0])) {
				GLM_DrawIndexedTurbPoly(cl.worldmodel->vao.vao, indices, count, current_colour, current_texture_array);
				count = 0;
			}

			if (count) {
				int prev = count - 1;

				if (count % 2 == 1) {
					indices[count++] = indices[prev];
				}
				indices[count++] = indices[prev];
				indices[count++] = poly->vbo_start;
			}
			for (v = 0; v < newVerts; ++v) {
				indices[count++] = poly->vbo_start + v;
			}
		}
	}
	if (count) {
		GLM_DrawIndexedTurbPoly(cl.worldmodel->vao.vao, indices, count, current_colour, current_texture_array);
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

void GLM_DrawIndexedTurbPoly(GLuint vao, GLuint* indices, int count, byte* colour, GLuint texture_array)
{
	float wateralpha = bound((1 - r_refdef2.max_watervis), r_wateralpha.value, 1);
	extern cvar_t r_fastturb;

	if (r_fastturb.value) {
		byte old_alpha = colour[3];

		// TODO: wateralpha
		colour[3] = 255;
		GLM_DrawIndexedPolygonByType(GL_TRIANGLE_STRIP, colour, vao, indices, count, false, false, false);
		colour[3] = old_alpha;
	}
	else {
		GL_BindTexture(GL_TEXTURE_2D_ARRAY, texture_array, true);
		GLM_DrawIndexedTurbPolys(vao, indices, count, wateralpha);
	}
}

