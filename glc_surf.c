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
#ifdef RENDERER_OPTION_CLASSIC_OPENGL

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"
#include "utils.h"
#include "tr_types.h"
#include "r_texture.h"
#include "r_vao.h"
#include "glc_local.h"
#include "r_brushmodel.h"
#include "r_renderer.h"
#include "r_program.h"
#include "r_lightmaps.h"

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

void GLC_DrawChainOutline(msurface_t* surf, qbool texture_chains)
{
	msurface_t* prev;
	msurface_t* s = surf;

	while (s) {
		int front = 0, back = s->polys->numverts - 1;

		GLC_Begin(GL_LINE_LOOP);
		while (front < back) {
			GLC_Vertex3fv(s->polys->verts[front]);
			front += 2;
		}
		while (back > 0) {
			GLC_Vertex3fv(s->polys->verts[back]);
			back -= 2;
		}
		GLC_End();

		prev = s;
		if (texture_chains) {
			s = s->texturechain;
			prev->texturechain = NULL;
		}
		else {
			s = s->drawflatchain;
			prev->drawflatchain = NULL;
		}
	}
}

void GLC_DrawMapOutline(model_t *model)
{
	int i;
	unsigned int lightmap_count;

	if (!(model->isworldmodel && r_refdef2.drawWorldOutlines)) {
		return;
	}

	lightmap_count = R_LightmapCount();
	GLC_StateBeginDrawMapOutline();
	for (i = 0; i < model->numtextures; i++) {
		texture_t* t;

		if (!model->textures[i] || !model->textures[i]->texturechain) {
			continue;
		}

		t = R_TextureAnimation(NULL, model->textures[i]);
		GLC_DrawChainOutline(t->texturechain, true);
	}
	GLC_DrawChainOutline(model->drawflat_chain, false);
	for (i = 0; i < lightmap_count; ++i) {
		GLC_DrawChainOutline(R_DrawflatLightmapChain(i), false);
		R_ClearDrawflatLightmapChain(i);
	}
}

static void DrawGLPoly(glpoly_t *p)
{
	int i;
	float *v;

	GLC_Begin(GL_TRIANGLE_STRIP);
	v = p->verts[0];
	for (i = 0; i < p->numverts; i++, v += VERTEXSIZE) {
		glTexCoord2f(v[3], v[4]);
		GLC_Vertex3fv(v);
	}
	GLC_End();
}

unsigned int GLC_DrawIndexedPoly(glpoly_t* p, unsigned int* modelIndexes, unsigned int modelIndexMaximum, unsigned int index_count)
{
	int k;

	if (GL_Supported(R_SUPPORT_PRIMITIVERESTART)) {
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

void GLC_RenderFullbrights_GLSL(void)
{
	int i;
	glpoly_t *p;
	texture_ref texture;

	GLC_StateBeginRenderFullbrights();
	R_ProgramUse(r_program_world_secondpass_glc);

	for (i = 1; i < MAX_GLTEXTURES; i++) {
		int index_count = 0;

		if (!fullbright_polys[i]) {
			continue;
		}

		texture.index = i;
		renderer.TextureUnitBind(0, texture);

		for (p = fullbright_polys[i]; p; p = p->fb_chain) {
			index_count = GLC_DrawIndexedPoly(p, modelIndexes, modelIndexMaximum, index_count);
		}

		if (index_count) {
			GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
			index_count = 0;
		}
		fullbright_polys[i] = NULL;
	}
}

void GLC_RenderFullbrights(void)
{
	int i;
	glpoly_t *p;
	texture_ref texture;

	GLC_StateBeginRenderFullbrights();

	for (i = 1; i < MAX_GLTEXTURES; i++) {
		if (!fullbright_polys[i]) {
			continue;
		}

		texture.index = i;
		renderer.TextureUnitBind(0, texture);
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
}

void GLC_RenderLumas_GLSL(void)
{
	int i;
	glpoly_t *p;
	texture_ref texture;
	int index_count = 0;

	GLC_StateBeginRenderLumas();
	R_ProgramUse(r_program_world_secondpass_glc);

	for (i = 1; i < MAX_GLTEXTURES; i++) {
		if (!luma_polys[i]) {
			continue;
		}

		texture.index = i;
		renderer.TextureUnitBind(0, texture);

		for (p = luma_polys[i]; p; p = p->luma_chain) {
			index_count = GLC_DrawIndexedPoly(p, modelIndexes, modelIndexMaximum, index_count);
		}

		if (index_count) {
			GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
		}
		luma_polys[i] = NULL;
	}
}

void GLC_RenderLumas(void)
{
	int i;
	glpoly_t *p;
	texture_ref texture;
	qbool use_vbo = buffers.supported && modelIndexes;

	GLC_StateBeginRenderLumas();

	for (i = 1; i < MAX_GLTEXTURES; i++) {
		if (!luma_polys[i]) {
			continue;
		}

		texture.index = i;
		renderer.TextureUnitBind(0, texture);
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
}

void GLC_EmitDetailPolys_GLSL(void)
{
	glpoly_t *p;
	GLuint index_count = 0;

	if (!detail_polys) {
		return;
	}

	GLC_StateBeginEmitDetailPolys();
	R_ProgramUse(r_program_world_secondpass_glc);

	for (p = detail_polys; p; p = p->detail_chain) {
		index_count = GLC_DrawIndexedPoly(p, modelIndexes, modelIndexMaximum, index_count);
	}

	if (index_count) {
		GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
	}

	R_ProgramUse(r_program_none);
	detail_polys = NULL;
}

void GLC_EmitDetailPolys(qbool use_vbo)
{
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
			GLC_Begin(GL_TRIANGLE_STRIP);
			v = p->verts[0];
			for (i = 0; i < p->numverts; i++, v += VERTEXSIZE) {
				glTexCoord2f(v[7], v[8]);
				GLC_Vertex3fv(v);
			}
			GLC_End();
		}
	}

	if (index_count) {
		GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
	}

	detail_polys = NULL;
}

#endif // #ifdef RENDERER_OPTION_CLASSIC_OPENGL
