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
// glc_surf.c: classic surface-related refresh code

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"
#include "utils.h"
#include "tr_types.h"
#include "r_texture.h"
#include "r_vao.h"

// This is a chain of polys, only used in classic when multi-texturing not available
glpoly_t *fullbright_polys[MAX_GLTEXTURES];
glpoly_t *luma_polys[MAX_GLTEXTURES];

extern glpoly_t *caustics_polys;
extern glpoly_t *detail_polys;

void GLC_ClearLightmapPolys(void);

void GLC_ClearTextureChains(void)
{
	GLC_ClearLightmapPolys();
	memset(fullbright_polys, 0, sizeof(fullbright_polys));
	memset(luma_polys, 0, sizeof(luma_polys));
}

void GLC_DrawMapOutline(model_t *model)
{
	msurface_t *s;
	int waterline, i, k;
	float *v;
	vec3_t n;

	GLC_StateBeginDrawMapOutline();

	for (i = 0; i < model->numtextures; i++) {
		if (!model->textures[i] || (!model->textures[i]->texturechain[0] && !model->textures[i]->texturechain[1]))
			continue;

		for (waterline = 0; waterline < 2; waterline++) {
			if (!(s = model->textures[i]->texturechain[waterline]))
				continue;

			for (; s; s = s->texturechain) {
				v = s->polys->verts[0];
				VectorCopy(s->plane->normal, n);
				VectorNormalize(n);

				glBegin(GL_LINE_LOOP);
				for (k = 0; k < s->polys->numverts; k++, v += VERTEXSIZE) {
					glVertex3fv(v);
				}
				glEnd();
			}
		}
	}
}

static void DrawGLPoly(glpoly_t *p)
{
	int i;
	float *v;

	glBegin(GL_POLYGON);
	v = p->verts[0];
	for (i = 0; i < p->numverts; i++, v += VERTEXSIZE) {
		glTexCoord2f(v[3], v[4]);
		glVertex3fv(v);
	}
	glEnd();
}

GLuint GLC_DrawIndexedPoly(glpoly_t* p, GLuint* modelIndexes, GLuint modelIndexMaximum, GLuint index_count)
{
	int k;

	if (glConfig.primitiveRestartSupported) {
		if (index_count + 1 + p->numverts > modelIndexMaximum) {
			GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
			index_count = 0;
		}

		if (index_count) {
			modelIndexes[index_count++] = ~(GLuint)0;
		}
	}
	else {
		if (index_count + 3 + p->numverts > modelIndexMaximum) {
			GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
			index_count = 0;
		}

		if (index_count) {
			int prev = index_count - 1;
			if (index_count % 2 == 1) {
				modelIndexes[index_count++] = modelIndexes[prev];
			}
			modelIndexes[index_count++] = modelIndexes[prev];
			modelIndexes[index_count++] = p->vbo_start;
		}
	}

	for (k = 0; k < p->numverts; ++k) {
		modelIndexes[index_count++] = p->vbo_start + k;
	}

	return index_count;
}

void GLC_RenderFullbrights(void)
{
	extern GLuint* modelIndexes;
	extern GLuint modelIndexMaximum;

	int i;
	glpoly_t *p;
	texture_ref texture;

	GLC_StateBeginRenderFullbrights();

	for (i = 1; i < MAX_GLTEXTURES; i++) {
		if (!fullbright_polys[i]) {
			continue;
		}

		texture.index = i;
		GL_EnsureTextureUnitBound(GL_TEXTURE0, texture);
		if (R_VAOBound()) {
			int index_count = 0;

			for (p = fullbright_polys[i]; p; p = p->fb_chain) {
				index_count = GLC_DrawIndexedPoly(p, modelIndexes, modelIndexMaximum, index_count);
			}

			if (index_count) {
				GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
			}
		}
		else {
			for (p = fullbright_polys[i]; p; p = p->fb_chain) {
				DrawGLPoly(p);
			}
		}
		fullbright_polys[i] = NULL;
	}

	GLC_StateEndRenderFullbrights();
}

void GLC_RenderLumas(void)
{
	extern GLuint* modelIndexes;
	extern GLuint modelIndexMaximum;

	int i;
	glpoly_t *p;
	texture_ref texture;
	qbool use_vbo = GL_BuffersSupported() && modelIndexes;

	GLC_StateBeginRenderLumas();

	for (i = 1; i < MAX_GLTEXTURES; i++) {
		if (!luma_polys[i]) {
			continue;
		}

		texture.index = i;
		GL_EnsureTextureUnitBound(GL_TEXTURE0, texture);
		if (use_vbo) {
			int index_count = 0;

			for (p = luma_polys[i]; p; p = p->luma_chain) {
				index_count = GLC_DrawIndexedPoly(p, modelIndexes, modelIndexMaximum, index_count);
			}

			if (index_count) {
				GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
			}
		}
		else {
			for (p = luma_polys[i]; p; p = p->luma_chain) {
				DrawGLPoly(p);
			}
		}
		luma_polys[i] = NULL;
	}

	GLC_StateEndRenderLumas();
}

void GLC_EmitDetailPolys(qbool use_vbo)
{
	extern GLuint* modelIndexes;
	extern GLuint modelIndexMaximum;

	glpoly_t *p;
	int i;
	float *v;
	GLuint index_count = 0;

	if (!detail_polys) {
		return;
	}

	GLC_StateBeginEmitDetailPolys();

	for (p = detail_polys; p; p = p->detail_chain) {
		if (use_vbo) {
			index_count = GLC_DrawIndexedPoly(p, modelIndexes, modelIndexMaximum, index_count);
		}
		else {
			glBegin(GL_POLYGON);
			v = p->verts[0];
			for (i = 0; i < p->numverts; i++, v += VERTEXSIZE) {
				glTexCoord2f(v[7], v[8]);
				glVertex3fv(v);
			}
			glEnd();
		}
	}

	if (index_count) {
		GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
	}

	GLC_StateEndEmitDetailPolys();

	detail_polys = NULL;
}
