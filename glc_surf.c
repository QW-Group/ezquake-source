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
static glpoly_t	*lightmap_polys[MAX_LIGHTMAPS];
static glpoly_t *fullbright_polys[MAX_GLTEXTURES];
static glpoly_t *luma_polys[MAX_GLTEXTURES];
qbool drawfullbrights = false, drawlumas = false;

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

void DrawTextureChains(model_t *model, int contents)
{
	extern cvar_t  gl_lumaTextures;

	int waterline, i, k, GL_LIGHTMAP_TEXTURE = 0, GL_FB_TEXTURE = 0, fb_texturenum;
	msurface_t *s;
	texture_t *t;
	float *v;

	qbool render_lightmaps = false;
	qbool doMtex1, doMtex2;

	qbool isLumaTexture;

	qbool draw_fbs, draw_caustics, draw_details;

	qbool can_mtex_lightmaps, can_mtex_fbs;

	qbool draw_mtex_fbs;

	qbool mtex_lightmaps, mtex_fbs;

	draw_caustics = underwatertexture && gl_caustics.value;
	draw_details = detailtexture && gl_detail.value;

	if (gl_fb_bmodels.value) {
		can_mtex_lightmaps = gl_mtexable;
		can_mtex_fbs = gl_textureunits >= 3;
	}
	else {
		can_mtex_lightmaps = gl_textureunits >= 3;
		can_mtex_fbs = gl_textureunits >= 3 && gl_add_ext;
	}

	GL_DisableMultitexture();
	if (gl_fogenable.value)
		glEnable(GL_FOG);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	for (i = 0; i < model->numtextures; i++) {
		if (!model->textures[i] || (!model->textures[i]->texturechain[0] && !model->textures[i]->texturechain[1]))
			continue;

		t = R_TextureAnimation(model->textures[i]);

		if (t->isLumaTexture) {
			isLumaTexture = gl_lumaTextures.value && r_refdef2.allow_lumas;
			fb_texturenum = isLumaTexture ? t->fb_texturenum : 0;
		}
		else {
			isLumaTexture = false;
			fb_texturenum = t->fb_texturenum;
		}

		//bind the world texture
		GL_SelectTexture(GL_TEXTURE0);
		GL_Bind(t->gl_texturenum);

		draw_fbs = gl_fb_bmodels.value /* || isLumaTexture */;
		draw_mtex_fbs = draw_fbs && can_mtex_fbs;

		if (gl_mtexable) {
			if (isLumaTexture && !gl_fb_bmodels.value) {
				if (gl_add_ext) {
					doMtex1 = true;
					GL_EnableTMU(GL_TEXTURE1);
					GL_FB_TEXTURE = GL_TEXTURE1;
					glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
					GL_Bind(fb_texturenum);

					mtex_lightmaps = can_mtex_lightmaps;
					mtex_fbs = true;

					if (mtex_lightmaps) {
						doMtex2 = true;
						GL_LIGHTMAP_TEXTURE = GL_TEXTURE2;
						GL_EnableTMU(GL_LIGHTMAP_TEXTURE);
						GLC_SetLightmapTextureEnvironment();
					}
					else {
						doMtex2 = false;
						render_lightmaps = true;
					}
				}
				else {
					GL_DisableTMU(GL_TEXTURE1);
					render_lightmaps = true;
					doMtex1 = doMtex2 = mtex_lightmaps = mtex_fbs = false;
				}
			}
			else {
				doMtex1 = true;
				GL_EnableTMU(GL_TEXTURE1);
				GL_LIGHTMAP_TEXTURE = GL_TEXTURE1;
				GLC_SetLightmapTextureEnvironment();

				mtex_lightmaps = true;
				mtex_fbs = fb_texturenum && draw_mtex_fbs;

				if (mtex_fbs) {
					doMtex2 = true;
					GL_FB_TEXTURE = GL_TEXTURE2;
					GL_EnableTMU(GL_FB_TEXTURE);
					GL_Bind(fb_texturenum);
					glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, isLumaTexture ? GL_ADD : GL_DECAL);
				}
				else {
					doMtex2 = false;
				}
			}
		}
		else {
			render_lightmaps = true;
			doMtex1 = doMtex2 = mtex_lightmaps = mtex_fbs = false;
		}


		for (waterline = 0; waterline < 2; waterline++) {
			if (!(s = model->textures[i]->texturechain[waterline]))
				continue;

			for (; s; s = s->texturechain) {
				if (mtex_lightmaps) {
					GLC_MultitextureLightmap(s->lightmaptexturenum);
				}
				else {
					s->polys->chain = lightmap_polys[s->lightmaptexturenum];
					lightmap_polys[s->lightmaptexturenum] = s->polys;
				}

				glBegin(GL_POLYGON);
				v = s->polys->verts[0];

				if (!s->texinfo->flags & TEX_SPECIAL) {
					for (k = 0; k < s->polys->numverts; k++, v += VERTEXSIZE) {
						if (doMtex1) {
							//Tei: textureless for the world brush models
							if (gl_textureless.value && model->isworldmodel) { //Qrack
								qglMultiTexCoord2f(GL_TEXTURE0, 0, 0);

								if (mtex_lightmaps)
									qglMultiTexCoord2f(GL_LIGHTMAP_TEXTURE, v[5], v[6]);

								if (mtex_fbs)
									qglMultiTexCoord2f(GL_TEXTURE2, 0, 0);
							}
							else {
								qglMultiTexCoord2f(GL_TEXTURE0, v[3], v[4]);

								if (mtex_lightmaps)
									qglMultiTexCoord2f(GL_LIGHTMAP_TEXTURE, v[5], v[6]);

								if (mtex_fbs)
									qglMultiTexCoord2f(GL_FB_TEXTURE, v[3], v[4]);
							}
						}
						else {
							if (gl_textureless.value && model->isworldmodel) //Qrack
								glTexCoord2f(0, 0);
							else
								glTexCoord2f(v[3], v[4]);
						}
						glVertex3fv(v);
					}
				}
				glEnd();

				if (draw_caustics && (waterline || ISUNDERWATER(contents))) {
					s->polys->caustics_chain = caustics_polys;
					caustics_polys = s->polys;
				}

				if (!waterline && draw_details) {
					s->polys->detail_chain = detail_polys;
					detail_polys = s->polys;
				}

				if (fb_texturenum && draw_fbs && !mtex_fbs) {
					if (isLumaTexture) {
						s->polys->luma_chain = luma_polys[fb_texturenum];
						luma_polys[fb_texturenum] = s->polys;
						drawlumas = true;
					}
					else {
						s->polys->fb_chain = fullbright_polys[fb_texturenum];
						fullbright_polys[fb_texturenum] = s->polys;
						drawfullbrights = true;
					}
				}
			}
		}

		if (doMtex1)
			GL_DisableTMU(GL_TEXTURE1);
		if (doMtex2)
			GL_DisableTMU(GL_TEXTURE2);
	}

	if (gl_mtexable)
		GL_SelectTexture(GL_TEXTURE0);

	if (gl_fb_bmodels.value) {
		if (render_lightmaps)
			R_BlendLightmaps();
		if (drawfullbrights)
			R_RenderFullbrights();
		if (drawlumas)
			R_RenderLumas();
	}
	else {
		if (drawlumas)
			R_RenderLumas();
		if (render_lightmaps)
			R_BlendLightmaps();
		if (drawfullbrights)
			R_RenderFullbrights();
	}

	if (gl_fogenable.value)
		glDisable(GL_FOG);

	EmitCausticsPolys();
	EmitDetailPolys();
}

void R_DrawFlat(model_t *model)
{
	msurface_t *s;
	int waterline, i, k;
	float *v;
	vec3_t n;
	byte w[3], f[3];
	qbool draw_caustics = underwatertexture && gl_caustics.value;

	memcpy(w, r_wallcolor.color, 3);
	memcpy(f, r_floorcolor.color, 3);

	GL_DisableMultitexture();

	// START shaman BUG /fog not working with /r_drawflat {
	if (gl_fogenable.value)
		glEnable(GL_FOG);
	// } END shaman BUG /fog not working with /r_drawflat

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);

	GL_SelectTexture(GL_TEXTURE0);

	for (i = 0; i < model->numtextures; i++) {
		if (!model->textures[i] || (!model->textures[i]->texturechain[0] && !model->textures[i]->texturechain[1]))
			continue;

		for (waterline = 0; waterline < 2; waterline++) {
			if (!(s = model->textures[i]->texturechain[waterline]))
				continue;

			for (; s; s = s->texturechain) {
				GLC_SetTextureLightmap(s->lightmaptexturenum);

				v = s->polys->verts[0];
				VectorCopy(s->plane->normal, n);
				VectorNormalize(n);

				// r_drawflat 1 == All solid colors
				// r_drawflat 2 == Solid floor/ceiling only
				// r_drawflat 3 == Solid walls only

				if (n[2] < -0.5 || n[2] > 0.5) // floor or ceiling
				{
					if (r_drawflat.integer == 2 || r_drawflat.integer == 1) {
						glColor3ubv(f);
					}
					else {
						continue;
					}
				}
				else										// walls
				{
					if (r_drawflat.integer == 3 || r_drawflat.integer == 1) {
						glColor3ubv(w);
					}
					else {
						continue;
					}
				}

				glBegin(GL_POLYGON);
				for (k = 0; k < s->polys->numverts; k++, v += VERTEXSIZE) {
					glTexCoord2f(v[5], v[6]);
					glVertex3fv(v);
				}
				glEnd();

				// START shaman FIX /r_drawflat + /gl_caustics {
				if (waterline && draw_caustics) {
					s->polys->caustics_chain = caustics_polys;
					caustics_polys = s->polys;
				}
				// } END shaman FIX /r_drawflat + /gl_caustics
			}
		}
	}

	if (gl_fogenable.value)
		glDisable(GL_FOG);

	glColor3f(1.0f, 1.0f, 1.0f);
	// START shaman FIX /r_drawflat + /gl_caustics {
	EmitCausticsPolys();
	// } END shaman FIX /r_drawflat + /gl_caustics
}

void R_DrawMapOutline(model_t *model)
{
	extern cvar_t gl_outline_width;
	msurface_t *s;
	int waterline, i, k;
	float *v;
	vec3_t n;

	GL_PolygonOffset(1, 1);
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
				GLC_SetTextureLightmap(s->lightmaptexturenum);

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
	GL_PolygonOffset(0, 0);
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

	if (!drawfullbrights) {
		return;
	}

	glDepthMask(GL_FALSE);	// don't bother writing Z
	glEnable(GL_ALPHA_TEST);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	for (i = 1; i < MAX_GLTEXTURES; i++) {
		if (!fullbright_polys[i]) {
			continue;
		}
		GL_Bind(i);
		for (p = fullbright_polys[i]; p; p = p->fb_chain) {
			DrawGLPoly(p);
		}
		fullbright_polys[i] = NULL;
	}

	glDisable(GL_ALPHA_TEST);
	glDepthMask(GL_TRUE);

	drawfullbrights = false;
}

void R_RenderLumas(void)
{
	int i;
	glpoly_t *p;

	if (!drawlumas)
		return;

	glDepthMask(GL_FALSE);	// don't bother writing Z
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	for (i = 1; i < MAX_GLTEXTURES; i++) {
		if (!luma_polys[i]) {
			continue;
		}
		GL_Bind(i);
		for (p = luma_polys[i]; p; p = p->luma_chain) {
			DrawGLPoly(p);
		}
		luma_polys[i] = NULL;
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_TRUE);

	drawlumas = false;
}

void EmitDetailPolys(void)
{
	glpoly_t *p;
	int i;
	float *v;

	if (!detail_polys)
		return;

	GL_Bind(detailtexture);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	glEnable(GL_BLEND);

	for (p = detail_polys; p; p = p->detail_chain) {
		glBegin(GL_POLYGON);
		v = p->verts[0];
		for (i = 0; i < p->numverts; i++, v += VERTEXSIZE) {
			glTexCoord2f(v[7] * 18, v[8] * 18);
			glVertex3fv(v);
		}
		glEnd();
	}

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_BLEND);

	detail_polys = NULL;
}
