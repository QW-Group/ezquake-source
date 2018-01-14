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

// This is a chain of polys, only used in classic when multi-texturing not available
glpoly_t *lightmap_polys[MAX_LIGHTMAPS];
glpoly_t *fullbright_polys[MAX_GLTEXTURES];
glpoly_t *luma_polys[MAX_GLTEXTURES];

extern glpoly_t *caustics_polys;
extern glpoly_t *detail_polys;

extern cvar_t gl_textureless;

void R_RenderFullbrights(void);
void R_RenderLumas(void);

void GLC_ClearTextureChains(void)
{
	memset(lightmap_polys, 0, sizeof(lightmap_polys));
	memset(fullbright_polys, 0, sizeof(fullbright_polys));
	memset(luma_polys, 0, sizeof(luma_polys));
}

void R_DrawMapOutline(model_t *model)
{
	extern cvar_t gl_outline_width;
	msurface_t *s;
	int waterline, i, k;
	float *v;
	vec3_t n;

	GL_PolygonOffset(POLYGONOFFSET_OUTLINES);
	glColor3f(1.0f, 1.0f, 1.0f);
	glLineWidth(bound(0.1, gl_outline_width.value, 3.0));

	glPushAttrib(GL_ENABLE_BIT);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_TEXTURE_2D);

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

	glPopAttrib();

	glColor3f(1.0f, 1.0f, 1.0f);
	GL_PolygonOffset(POLYGONOFFSET_DISABLED);
}

void DrawGLPoly(glpoly_t *p)
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

void R_RenderFullbrights(void)
{
	int i;
	glpoly_t *p;
	texture_ref texture;

	GL_DepthMask(GL_FALSE);	// don't bother writing Z
	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED);
	GL_TextureEnvMode(GL_REPLACE);

	for (i = 1; i < MAX_GLTEXTURES; i++) {
		if (!fullbright_polys[i]) {
			continue;
		}

		texture.index = i;
		GL_BindTextureUnit(GL_TEXTURE0, GL_TEXTURE_2D, texture);
		for (p = fullbright_polys[i]; p; p = p->fb_chain) {
			DrawGLPoly(p);
		}
		fullbright_polys[i] = NULL;
	}

	// FIXME: GL_ResetState()
	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED);
	GL_DepthMask(GL_TRUE);
}

void R_RenderLumas(void)
{
	int i;
	glpoly_t *p;
	texture_ref texture;

	GL_DepthMask(GL_FALSE);	// don't bother writing Z
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	GL_BlendFunc(GL_ONE, GL_ONE);

	GL_TextureEnvMode(GL_REPLACE);

	for (i = 1; i < MAX_GLTEXTURES; i++) {
		if (!luma_polys[i]) {
			continue;
		}

		texture.index = i;
		GL_BindTextureUnit(GL_TEXTURE0, GL_TEXTURE_2D, texture);
		for (p = luma_polys[i]; p; p = p->luma_chain) {
			DrawGLPoly(p);
		}
		luma_polys[i] = NULL;
	}

	// FIXME: GL_ResetState()
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthMask(GL_TRUE);
}

void EmitDetailPolys(void)
{
	glpoly_t *p;
	int i;
	float *v;

	if (!detail_polys) {
		return;
	}

	GL_BindTextureUnit(GL_TEXTURE0, GL_TEXTURE_2D, detailtexture);
	GL_TextureEnvMode(GL_DECAL);
	GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);

	for (p = detail_polys; p; p = p->detail_chain) {
		glBegin(GL_POLYGON);
		v = p->verts[0];
		for (i = 0; i < p->numverts; i++, v += VERTEXSIZE) {
			glTexCoord2f(v[7] * 18, v[8] * 18);
			glVertex3fv(v);
		}
		glEnd();
	}

	// FIXME: GL_ResetState()
	GL_TextureEnvMode(GL_REPLACE);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);

	detail_polys = NULL;
}
