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
// r_surf.c: surface-related refresh code

#include "quakedef.h"
#include "gl_local.h"

#define	BLOCK_WIDTH		128
#define	BLOCK_HEIGHT	128

#define MAX_LIGHTMAP_SIZE	4096
#define	MAX_LIGHTMAPS		64

static int lightmap_textures;
static unsigned blocklights[MAX_LIGHTMAP_SIZE * 3];

typedef struct glRect_s {
	unsigned char l, t, w, h;
} glRect_t;

static glpoly_t	*lightmap_polys[MAX_LIGHTMAPS];
static qboolean	lightmap_modified[MAX_LIGHTMAPS];
static glRect_t	lightmap_rectchange[MAX_LIGHTMAPS];

static int allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];

// the lightmap texture data needs to be kept in
// main memory so texsubimage can update properly
byte	lightmaps[3 * MAX_LIGHTMAPS * BLOCK_WIDTH * BLOCK_HEIGHT];

msurface_t	*skychain = NULL;
msurface_t	**skychain_tail = &skychain;

msurface_t	*waterchain = NULL;
msurface_t	**waterchain_tail = &waterchain;

msurface_t	*alphachain = NULL;
msurface_t	**alphachain_tail = &alphachain;

#define CHAIN_SURF_F2B(surf, chain_tail)		\
	{											\
		*(chain_tail) = (surf);					\
		(chain_tail) = &(surf)->texturechain;	\
		(surf)->texturechain = NULL;			\
	}

#define CHAIN_SURF_B2F(surf, chain) 			\
	{											\
		(surf)->texturechain = (chain);			\
		(chain) = (surf);						\
	}

glpoly_t *fullbright_polys[MAX_GLTEXTURES];
glpoly_t *luma_polys[MAX_GLTEXTURES];
qboolean drawfullbrights = false, drawlumas = false;
glpoly_t *caustics_polys = NULL;
glpoly_t *detail_polys = NULL;

void DrawGLPoly (glpoly_t *p);

void R_RenderFullbrights (void) {
	int i;
	glpoly_t *p;

	if (!drawfullbrights)
		return;

	glDepthMask (GL_FALSE);	// don't bother writing Z
	glEnable(GL_ALPHA_TEST);


	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	for (i = 1; i < MAX_GLTEXTURES; i++) {
		if (!fullbright_polys[i])
			continue;
		GL_Bind (i);
		for (p = fullbright_polys[i]; p; p = p->fb_chain)
			DrawGLPoly (p);
		fullbright_polys[i] = NULL;		
	}

	glDisable(GL_ALPHA_TEST);
	glDepthMask (GL_TRUE);

	drawfullbrights = false;
}

void R_RenderLumas (void) {
	int i;
	glpoly_t *p;

	if (!drawlumas)
		return;

	glDepthMask (GL_FALSE);	// don't bother writing Z
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	for (i = 1; i < MAX_GLTEXTURES; i++) {
		if (!luma_polys[i])
			continue;
		GL_Bind (i);
		for (p = luma_polys[i]; p; p = p->luma_chain)
			DrawGLPoly (p);
		luma_polys[i] = NULL;		
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask (GL_TRUE);

	drawlumas = false;
}


void EmitDetailPolys (void) {
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
			glTexCoord2f (v[7] * 18, v[8] * 18);
			glVertex3fv (v);
		}
		glEnd();
	}

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_BLEND);

	detail_polys = NULL;
}


typedef struct dlightinfo_s {
	int local[2];
	int rad;
	int minlight;	// rad - minlight
	int type;
} dlightinfo_t;

static dlightinfo_t dlightlist[MAX_DLIGHTS];
static int numdlights;

void R_BuildDlightList (msurface_t *surf) {
	float dist;
	vec3_t impact;
	mtexinfo_t *tex;
	int lnum, i, smax, tmax, irad, iminlight, local[2], tdmin, sdmin, distmin;
	dlightinfo_t *light;

	numdlights = 0;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;

	for (lnum = 0; lnum < MAX_DLIGHTS; lnum++) {
		if ( !(surf->dlightbits & (1 << lnum) ) )
			continue;		// not lit by this light

		dist = PlaneDiff(cl_dlights[lnum].origin, surf->plane);
		irad = (cl_dlights[lnum].radius - fabs(dist)) * 256;
		iminlight = cl_dlights[lnum].minlight * 256;
		if (irad < iminlight)
			continue;

		iminlight = irad - iminlight;
		
		for (i = 0; i < 3; i++)
			impact[i] = cl_dlights[lnum].origin[i] - surf->plane->normal[i] * dist;
		
		local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3] - surf->texturemins[0];
		local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3] - surf->texturemins[1];
		
		// check if this dlight will touch the surface
		if (local[1] > 0) {
			tdmin = local[1] - (tmax << 4);
			if (tdmin < 0)
				tdmin = 0;
		} else {
			tdmin = -local[1];
		}

		if (local[0] > 0) {
			sdmin = local[0] - (smax << 4);
			if (sdmin < 0)
				sdmin = 0;
		} else {
			sdmin = -local[0];
		}

		if (sdmin > tdmin)
			distmin = (sdmin << 8) + (tdmin << 7);
		else
			distmin = (tdmin << 8) + (sdmin << 7);

		if (distmin < iminlight) {
			// save dlight info
			light = &dlightlist[numdlights];
			light->minlight = iminlight;
			light->rad = irad;
			light->local[0] = local[0];
			light->local[1] = local[1];
			light->type = cl_dlights[lnum].type;
			numdlights++;
		}
	}
}

int dlightcolor[NUM_DLIGHTTYPES][3] = {
	{ 100, 90, 80 },	// dimlight or brightlight
	{ 100, 50, 10 },	// muzzleflash
	{ 100, 50, 10 },	// explosion
	{ 90, 60, 7 },		// rocket
	{ 128, 0, 0 },		// red
	{ 0, 0, 128 },		// blue
	{ 128, 0, 128 },	// red + blue
	{ 0, 128, 0 },		// green
	{ 128, 128, 128},	// white
};


//R_BuildDlightList must be called first!
void R_AddDynamicLights (msurface_t *surf) {
	int i, smax, tmax, s, t, sd, td, _sd, _td, irad, idist, iminlight, color[3], tmp;
	dlightinfo_t *light;
	unsigned *dest;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;

	for (i = 0, light = dlightlist; i < numdlights; i++, light++) {
		color[0] = dlightcolor[light->type][0];
		color[1] = dlightcolor[light->type][1];
		color[2] = dlightcolor[light->type][2];

		irad = light->rad;
		iminlight = light->minlight;

		_td = light->local[1];
		dest = blocklights;
		for (t = 0; t < tmax; t++) {
			td = _td;
			if (td < 0)	td = -td;
			_td -= 16;
			_sd = light->local[0];

			for (s = 0; s < smax; s++) {
				sd = _sd < 0 ? -_sd : _sd;
				_sd -= 16;
				if (sd > td)
					idist = (sd << 8) + (td << 7);
				else
					idist = (td << 8) + (sd << 7);

				if (idist < iminlight) {
					tmp = (irad - idist) >> 7;
					dest[0] += tmp * color[0];
					dest[1] += tmp * color[1];
					dest[2] += tmp * color[2];
				}
				dest += 3;
			}
		}
	}
}

//Combine and scale multiple lightmaps into the 8.8 format in blocklights
void R_BuildLightMap (msurface_t *surf, byte *dest, int stride) {
	int smax, tmax, t, i, j, size, blocksize, maps;
	byte *lightmap;
	unsigned scale, *bl;

	surf->cached_dlight = !!numdlights;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	size = smax * tmax;
	blocksize = size * 3;
	lightmap = surf->samples;

	// set to full bright if no light data
	if (/* r_fullbright.value || */ !cl.worldmodel->lightdata)  {
		for (i = 0; i < blocksize; i++)
			blocklights[i] = 255 * 256;
		goto store;
	}

	// clear to no light
	memset (blocklights, 0, blocksize * sizeof(int));

	// add all the lightmaps
	if (lightmap) {
		for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++) {
			scale = d_lightstylevalue[surf->styles[maps]];
			surf->cached_light[maps] = scale;	// 8.8 fraction
			bl = blocklights;
			for (i = 0; i < blocksize; i++)
				*bl++ += lightmap[i] * scale;
			lightmap += blocksize;		// skip to next lightmap
		}
	}

	// add all the dynamic lights
	if (numdlights)
		R_AddDynamicLights (surf);

// bound, invert, and shift
store:
	bl = blocklights;
	stride -= smax * 3;
	for (i = 0; i < tmax; i++, dest += stride) {
		if (lightmode == 2) {
			for (j = smax; j; j--) {
				t = bl[0]; t = (t >> 8) + (t >> 9); if (t > 255) t = 255;
				dest[0] = 255 - t;
				t = bl[1]; t = (t >> 8) + (t >> 9); if (t > 255) t = 255;
				dest[1] = 255 - t;
				t = bl[2]; t = (t >> 8) + (t >> 9); if (t > 255) t = 255;
				dest[2] = 255 - t;
				bl += 3;
				dest += 3;
			}
		} else {
			for (j = smax; j; j--) {
				t = bl[0]; t = t >> 7; if (t > 255) t = 255;
				dest[0] = 255 - t;
				t = bl[1]; t = t >> 7; if (t > 255) t = 255;
				dest[1] = 255 - t;
				t = bl[2]; t = t >> 7; if (t > 255) t = 255;
				dest[2] = 255 - t;
				bl += 3;
				dest += 3;
			}
		}
	}
}

void R_UploadLightMap (int lightmapnum) {
	glRect_t	*theRect;

	lightmap_modified[lightmapnum] = false;
	theRect = &lightmap_rectchange[lightmapnum];
	glTexSubImage2D (GL_TEXTURE_2D, 0, 0, theRect->t, BLOCK_WIDTH, theRect->h, GL_RGB, GL_UNSIGNED_BYTE,
		lightmaps + (lightmapnum * BLOCK_HEIGHT + theRect->t) * BLOCK_WIDTH * 3);
	theRect->l = BLOCK_WIDTH;
	theRect->t = BLOCK_HEIGHT;
	theRect->h = 0;
	theRect->w = 0;
}

//Returns the proper texture for a given time and base texture
texture_t *R_TextureAnimation (texture_t *base) {
	int relative, count;

	if (currententity->frame) {
		if (base->alternate_anims)
			base = base->alternate_anims;
	}

	if (!base->anim_total)
		return base;

	relative = (int) (cl.time * 10) % base->anim_total;

	count = 0;	
	while (base->anim_min > relative || base->anim_max <= relative) {
		base = base->anim_next;
		if (!base)
			Host_Error ("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Host_Error ("R_TextureAnimation: infinite cycle");
	}

	return base;
}


void DrawGLPoly (glpoly_t *p) {
	int i;
	float *v;

	glBegin (GL_POLYGON);
	v = p->verts[0];
	for (i = 0; i < p->numverts; i++, v+= VERTEXSIZE) {
		glTexCoord2f (v[3], v[4]);
		glVertex3fv (v);
	}
	glEnd ();
}

void R_BlendLightmaps (void) {
	int i, j;
	glpoly_t *p;
	float *v;

	glDepthMask (GL_FALSE);		// don't bother writing Z
	glBlendFunc (GL_ZERO, GL_ONE_MINUS_SRC_COLOR);	

	if (!r_lightmap.value)
		glEnable (GL_BLEND);

	for (i = 0; i < MAX_LIGHTMAPS; i++) {
		if (!(p = lightmap_polys[i]))
			continue;
		GL_Bind(lightmap_textures + i);
		if (lightmap_modified[i])
			R_UploadLightMap (i);
		for ( ; p; p = p->chain) {
			glBegin (GL_POLYGON);
			v = p->verts[0];
			for (j = 0; j < p->numverts; j++, v+= VERTEXSIZE) {
				glTexCoord2f (v[5], v[6]);
				glVertex3fv (v);
			}
			glEnd ();
		}
		lightmap_polys[i] = NULL;	
	}
	glDisable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask (GL_TRUE);		// back to normal Z buffering
}

void R_RenderDynamicLightmaps (msurface_t *fa) {
	byte *base;
	int maps, smax, tmax;
	glRect_t *theRect;
	qboolean lightstyle_modified = false;

	c_brush_polys++;

	if (!r_dynamic.value)
		return;

	// check for lightmap modification
	for (maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++) {
		if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps]) {
			lightstyle_modified = true;
			break;
		}
	}

	if (fa->dlightframe == r_framecount)
		R_BuildDlightList (fa);
	else
		numdlights = 0;

	if (numdlights == 0 && !fa->cached_dlight && !lightstyle_modified)
		return;

	lightmap_modified[fa->lightmaptexturenum] = true;
	theRect = &lightmap_rectchange[fa->lightmaptexturenum];
	if (fa->light_t < theRect->t) {
		if (theRect->h)
			theRect->h += theRect->t - fa->light_t;
		theRect->t = fa->light_t;
	}
	if (fa->light_s < theRect->l) {
		if (theRect->w)
			theRect->w += theRect->l - fa->light_s;
		theRect->l = fa->light_s;
	}
	smax = (fa->extents[0] >> 4) + 1;
	tmax = (fa->extents[1] >> 4) + 1;
	if (theRect->w + theRect->l < fa->light_s + smax)
		theRect->w = fa->light_s - theRect->l + smax;
	if (theRect->h + theRect->t < fa->light_t + tmax)
		theRect->h = fa->light_t - theRect->t + tmax;
	base = lightmaps + fa->lightmaptexturenum * BLOCK_WIDTH * BLOCK_HEIGHT * 3;
	base += (fa->light_t * BLOCK_WIDTH + fa->light_s) * 3;
	R_BuildLightMap (fa, base, BLOCK_WIDTH * 3);
}

void R_DrawWaterSurfaces (void) {
	msurface_t *s;
	float wateralpha;

	if (!waterchain)
		return;

	wateralpha = cl.watervis ? bound(0, r_wateralpha.value, 1) : 1;

	if (wateralpha < 1.0) {
		glEnable (GL_BLEND);
		glColor4f (1, 1, 1, wateralpha);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		if (wateralpha < 0.9)
			glDepthMask (GL_FALSE);
	}

	GL_DisableMultitexture();
	for (s = waterchain; s; s = s->texturechain) {
		GL_Bind (s->texinfo->texture->gl_texturenum);
		EmitWaterPolys (s);
	}
	waterchain = NULL;
	waterchain_tail = &waterchain;

	if (wateralpha < 1.0) {
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

		glColor3ubv (color_white);
		glDisable (GL_BLEND);
		if (wateralpha < 0.9)
			glDepthMask (GL_TRUE);
	}
}

//draws transparent textures for HL world and nonworld models
void R_DrawAlphaChain (void) {
	int k;
	msurface_t *s;
	texture_t *t;
	float *v;

	if (!alphachain)
		return;

	glEnable(GL_ALPHA_TEST);

	for (s = alphachain; s; s = s->texturechain) {
		
		
		t = s->texinfo->texture;
		R_RenderDynamicLightmaps (s);

		//bind the world texture
		GL_DisableMultitexture();
		GL_Bind (t->gl_texturenum);

		if (gl_mtexable) {
			//bind the lightmap texture
			GL_EnableMultitexture();
			GL_Bind (lightmap_textures + s->lightmaptexturenum);
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
			//update lightmap if its modified by dynamic lights
			k = s->lightmaptexturenum;
			if (lightmap_modified[k])
				R_UploadLightMap(k);
		}
		
		glBegin(GL_POLYGON);
		v = s->polys->verts[0];
		for (k = 0; k < s->polys->numverts; k++, v += VERTEXSIZE) {
			if (gl_mtexable) {
				qglMultiTexCoord2f (GL_TEXTURE0_ARB, v[3], v[4]);
				qglMultiTexCoord2f (GL_TEXTURE1_ARB, v[5], v[6]);
			} else {
				glTexCoord2f (v[3], v[4]);
			}
			glVertex3fv (v);
		}
		glEnd ();
	}

	alphachain = NULL;

	glDisable(GL_ALPHA_TEST);
	GL_DisableMultitexture();
}

#define CHAIN_RESET(chain)			\
{								\
	chain = NULL;				\
	chain##_tail = &chain;		\
}

static void R_ClearTextureChains(model_t *clmodel) {
	int i, waterline;
	texture_t *texture;

	memset (lightmap_polys, 0, sizeof(lightmap_polys));
	memset (fullbright_polys, 0, sizeof(fullbright_polys));
	memset (luma_polys, 0, sizeof(luma_polys));

	for (i = 0; i < clmodel->numtextures; i++) {
		for (waterline = 0; waterline < 2; waterline++) {
			if ((texture = clmodel->textures[i])) {
				texture->texturechain[waterline] = NULL;
				texture->texturechain_tail[waterline] = &texture->texturechain[waterline];
			}
		}
	}

	r_notexture_mip->texturechain[0] = NULL;
	r_notexture_mip->texturechain_tail[0] = &r_notexture_mip->texturechain[0];
	r_notexture_mip->texturechain[1] = NULL;
	r_notexture_mip->texturechain_tail[1] = &r_notexture_mip->texturechain[1];


	CHAIN_RESET(skychain);
	if (clmodel == cl.worldmodel)
		CHAIN_RESET(waterchain);
	CHAIN_RESET(alphachain);
}

void DrawTextureChains (model_t *model) {
	int waterline, i, k, GL_LIGHTMAP_TEXTURE, GL_FB_TEXTURE;
	msurface_t *s;
	texture_t *t;
	float *v;

	qboolean render_lightmaps = false, render_caustics = false, render_details = false;
	qboolean drawLumasGlowing, doMtex1, doMtex2;

	qboolean draw_fbs, draw_caustics, draw_details;

	qboolean can_mtex_lightmaps, can_mtex_fbs;

	qboolean draw_mtex_fbs;

	qboolean mtex_lightmaps, mtex_fbs;


	drawLumasGlowing = (com_serveractive || cl.allow_lumas) && gl_fb_bmodels.value;

	draw_caustics = underwatertexture && gl_caustics.value;
	draw_details = detailtexture && gl_detail.value;

	if (drawLumasGlowing) {
		can_mtex_lightmaps = gl_mtexable;
		can_mtex_fbs = gl_textureunits >= 3;
	} else {
		can_mtex_lightmaps = gl_textureunits >= 3;
		can_mtex_fbs = gl_textureunits >= 3 && gl_add_ext;
	}

	GL_DisableMultitexture();
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	for (i = 0; i < model->numtextures; i++) {
		if (!model->textures[i] || (!model->textures[i]->texturechain[0] && !model->textures[i]->texturechain[1]))
			continue;	

		t = R_TextureAnimation (model->textures[i]);
		//bind the world texture
		GL_SelectTexture(GL_TEXTURE0_ARB);
		GL_Bind (t->gl_texturenum);

		draw_fbs = t->isLumaTexture || gl_fb_bmodels.value;
		draw_mtex_fbs = draw_fbs && can_mtex_fbs;

		if (gl_mtexable) {
			if (t->isLumaTexture && !drawLumasGlowing) {
				if (gl_add_ext) {
					doMtex1 = true;
					GL_EnableTMU(GL_TEXTURE1_ARB);
					GL_FB_TEXTURE = GL_TEXTURE1_ARB;
					glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
					GL_Bind (t->fb_texturenum);

					mtex_lightmaps = can_mtex_lightmaps;
					mtex_fbs = true;

					if (mtex_lightmaps) {
						doMtex2 = true;
						GL_LIGHTMAP_TEXTURE = GL_TEXTURE2_ARB;
						GL_EnableTMU(GL_LIGHTMAP_TEXTURE);
						glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
					} else {
						doMtex2 = false;
						render_lightmaps = true;
					}
				} else {
					GL_DisableTMU(GL_TEXTURE1_ARB);					
					render_lightmaps = true;
					doMtex1 = doMtex2 = mtex_lightmaps = mtex_fbs = false;
				}
			} else {
				doMtex1 = true;
				GL_EnableTMU(GL_TEXTURE1_ARB);
				GL_LIGHTMAP_TEXTURE = GL_TEXTURE1_ARB;
				glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);

				mtex_lightmaps = true;
				mtex_fbs = t->fb_texturenum && draw_mtex_fbs;

				if (mtex_fbs) {
					doMtex2 = true;
					GL_FB_TEXTURE = GL_TEXTURE2_ARB;
					GL_EnableTMU(GL_FB_TEXTURE);
					GL_Bind (t->fb_texturenum);
					glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, t->isLumaTexture ? GL_ADD : GL_DECAL);
				} else {
					doMtex2 = false;
				}
			}
		} else {
			render_lightmaps = true;
			doMtex1 = doMtex2 = mtex_lightmaps = mtex_fbs = false;
		}


		for (waterline = 0; waterline < 2; waterline++) {
			if (!(s = model->textures[i]->texturechain[waterline]))
				continue;	

			for ( ; s; s = s->texturechain) {
				if (mtex_lightmaps) {
					//bind the lightmap texture
					GL_SelectTexture(GL_LIGHTMAP_TEXTURE);
					GL_Bind (lightmap_textures + s->lightmaptexturenum);
					//update lightmap if its modified by dynamic lights
					R_RenderDynamicLightmaps (s);
					k = s->lightmaptexturenum;
					if (lightmap_modified[k])
						R_UploadLightMap(k);
				} else {

					s->polys->chain = lightmap_polys[s->lightmaptexturenum];
					lightmap_polys[s->lightmaptexturenum] = s->polys;

					R_RenderDynamicLightmaps (s);
				}

				glBegin(GL_POLYGON);
				v = s->polys->verts[0];
				for (k = 0; k < s->polys->numverts; k++, v += VERTEXSIZE) {
					if (doMtex1) {
						qglMultiTexCoord2f (GL_TEXTURE0_ARB, v[3], v[4]);

						if (mtex_lightmaps)
							qglMultiTexCoord2f (GL_LIGHTMAP_TEXTURE, v[5], v[6]);

						if (mtex_fbs)
							qglMultiTexCoord2f (GL_FB_TEXTURE, v[3], v[4]);
					} else {
						glTexCoord2f (v[3], v[4]);
					}
					glVertex3fv (v);
				}
				glEnd ();

				if (waterline && draw_caustics) {
					s->polys->caustics_chain = caustics_polys;
					caustics_polys = s->polys;
				}
				if (!waterline && draw_details) {
					s->polys->detail_chain = detail_polys;
					detail_polys = s->polys;
				}

				if (t->fb_texturenum && draw_fbs && !mtex_fbs) {
					if (t->isLumaTexture) {
						s->polys->luma_chain = luma_polys[t->fb_texturenum];
						luma_polys[t->fb_texturenum] = s->polys;
						drawlumas = true;
					} else {
						s->polys->fb_chain = fullbright_polys[t->fb_texturenum];
						fullbright_polys[t->fb_texturenum] = s->polys;
						drawfullbrights = true;
					}
				}
			}
		}

		if (doMtex1)
			GL_DisableTMU(GL_TEXTURE1_ARB);
		if (doMtex2)
			GL_DisableTMU(GL_TEXTURE2_ARB);
	}

	if (gl_mtexable)
		GL_SelectTexture(GL_TEXTURE0_ARB);

	if (drawLumasGlowing) {
		if (render_lightmaps)
			R_BlendLightmaps();
		if (drawfullbrights)
			R_RenderFullbrights();
		if (drawlumas)
			R_RenderLumas();
	} else {
		if (drawlumas)
			R_RenderLumas();
		if (render_lightmaps)
			R_BlendLightmaps();
		if (drawfullbrights)
			R_RenderFullbrights();
	}

	EmitCausticsPolys();
	EmitDetailPolys();
}


void R_DrawBrushModel (entity_t *e) {
	int i, k, underwater;
	vec3_t mins, maxs;
	msurface_t *psurf;
	float dot;
	mplane_t *pplane;
	model_t *clmodel;
	qboolean rotated;

	currententity = e;
	currenttexture = -1;

	clmodel = e->model;

	if (e->angles[0] || e->angles[1] || e->angles[2]) {
		rotated = true;
		if (R_CullSphere (e->origin, clmodel->radius))
			return;
	} else {
		rotated = false;
		VectorAdd (e->origin, clmodel->mins, mins);
		VectorAdd (e->origin, clmodel->maxs, maxs);

		if (R_CullBox (mins, maxs))
			return;
	}

	VectorSubtract (r_refdef.vieworg, e->origin, modelorg);
	if (rotated) {
		vec3_t	temp;
		vec3_t	forward, right, up;

		VectorCopy (modelorg, temp);
		AngleVectors (e->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);
	}

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

	// calculate dynamic lighting for bmodel if it's not an instanced model
	if (clmodel->firstmodelsurface != 0 && !gl_flashblend.value) {
		for (k = 0; k < MAX_DLIGHTS; k++) {
			if ((cl_dlights[k].die < cl.time) || !cl_dlights[k].radius)
				continue;

			R_MarkLights (&cl_dlights[k], 1 << k, clmodel->nodes + clmodel->hulls[0].firstclipnode);
		}
	}

    glPushMatrix ();

	glTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);
	glRotatef (e->angles[1], 0, 0, 1);
	glRotatef (e->angles[0], 0, 1, 0);
	glRotatef (e->angles[2], 1, 0, 0);

	R_ClearTextureChains(clmodel);

	for (i = 0; i < clmodel->nummodelsurfaces; i++, psurf++) {
		// find which side of the node we are on
		pplane = psurf->plane;
		dot = PlaneDiff(modelorg, pplane);

		//draw the water surfaces now, and setup sky/normal chains
		if (	((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) || 
				(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			if (psurf->flags & SURF_DRAWSKY) {	
				CHAIN_SURF_B2F(psurf, skychain);
			} else if (psurf->flags & SURF_DRAWTURB) {	
				EmitWaterPolys (psurf);
			} else if (psurf->flags & SURF_DRAWALPHA) {
				
				CHAIN_SURF_B2F(psurf, alphachain);
			} else {
				
				underwater = (psurf->flags & SURF_UNDERWATER) ? 1 : 0;
				CHAIN_SURF_B2F(psurf, psurf->texinfo->texture->texturechain[underwater]);
			}
		}
	}

	//draw the textures chains for the model
	DrawTextureChains(clmodel);
	R_DrawSkyChain();
	R_DrawAlphaChain ();

	glPopMatrix ();
}


void R_RecursiveWorldNode (mnode_t *node, int clipflags) {
	int c, side, clipped, underwater;
	mplane_t *plane, *clipplane;
	msurface_t *surf, **mark;
	mleaf_t *pleaf;
	float dot;

	if (node->contents == CONTENTS_SOLID)
		return;		// solid
	if (node->visframe != r_visframecount)
		return;
	for (c = 0, clipplane = frustum; c < 4; c++, clipplane++) {
		if (!(clipflags & (1 << c)))
			continue;	// don't need to clip against it

		clipped = BOX_ON_PLANE_SIDE (node->minmaxs, node->minmaxs + 3, clipplane);
		if (clipped == 2)
			return;
		else if (clipped == 1)
			clipflags &= ~(1<<c);	// node is entirely on screen
	}

	// if a leaf node, draw stuff
	if (node->contents < 0)	{
		pleaf = (mleaf_t *) node;

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c) {
			do {
				(*mark)->visframe = r_framecount;
				mark++;
			} while (--c);
		}

	// deal with model fragments in this leaf
		if (pleaf->efrags)
			R_StoreEfrags (&pleaf->efrags);

		return;
	}

	// node is just a decision point, so go down the apropriate sides

	// find which side of the node we are on
	plane = node->plane;

	dot = PlaneDiff(modelorg, plane);
	side = (dot >= 0) ? 0 : 1;

	// recurse down the children, front side first
	R_RecursiveWorldNode (node->children[side], clipflags);

	// draw stuff
	c = node->numsurfaces;

	if (c)	{
		surf = cl.worldmodel->surfaces + node->firstsurface;

		if (dot < -BACKFACE_EPSILON)
			side = SURF_PLANEBACK;
		else if (dot > BACKFACE_EPSILON)
			side = 0;

		for ( ; c; c--, surf++) {
			if (surf->visframe != r_framecount)
				continue;

			if ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK))
				continue;		// wrong side

			// add surf to the right chain
			if (surf->flags & SURF_DRAWSKY) {
				CHAIN_SURF_F2B(surf, skychain_tail);
			} else if (surf->flags & SURF_DRAWTURB) {
				CHAIN_SURF_F2B(surf, waterchain_tail);
			} else if (surf->flags & SURF_DRAWALPHA) {
				
				CHAIN_SURF_B2F(surf, alphachain);
			} else {
				underwater = (surf->flags & SURF_UNDERWATER) ? 1 : 0;
				CHAIN_SURF_F2B(surf, surf->texinfo->texture->texturechain_tail[underwater]);
			}
		}
	}
	// recurse down the back side
	R_RecursiveWorldNode (node->children[!side], clipflags);
}

void R_DrawWorld (void) {
	entity_t ent;

	memset (&ent, 0, sizeof(ent));
	ent.model = cl.worldmodel;

	R_ClearTextureChains(cl.worldmodel);

	VectorCopy (r_refdef.vieworg, modelorg);

	currententity = &ent;
	currenttexture = -1;

	//set up texture chains for the world
	R_RecursiveWorldNode (cl.worldmodel->nodes, 15);

	//draw the world sky
	if (r_skyboxloaded)
		R_DrawSkyBox ();
	else
		R_DrawSkyChain ();

	R_DrawEntitiesOnList (&cl_firstpassents);

	//draw the world
	DrawTextureChains (cl.worldmodel);

	//draw the world alpha textures
	R_DrawAlphaChain ();
}

void R_MarkLeaves (void) {
	byte *vis;
	mnode_t *node;
	int i;
	byte solid[MAX_MAP_LEAFS/8];

	if (!r_novis.value && r_oldviewleaf == r_viewleaf
		&& r_oldviewleaf2 == r_viewleaf2)	// watervis hack
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	if (r_novis.value) {
		vis = solid;
		memset (solid, 0xff, (cl.worldmodel->numleafs + 7) >> 3);
	} else {
		vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);

		if (r_viewleaf2) {
			int			i, count;
			unsigned	*src, *dest;

			// merge visibility data for two leafs
			count = (cl.worldmodel->numleafs + 7) >> 3;
			memcpy (solid, vis, count);
			src = (unsigned *) Mod_LeafPVS (r_viewleaf2, cl.worldmodel);
			dest = (unsigned *) solid;
			count = (count + 3) >> 2;
			for (i = 0; i < count; i++)
				*dest++ |= *src++;
			vis = solid;
		}
	}
		
	for (i = 0; i < cl.worldmodel->numleafs; i++)	{
		if (vis[i >> 3] & (1 << (i & 7))) {
			node = (mnode_t *)&cl.worldmodel->leafs[i + 1];
			do {
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}


// returns a texture number and the position inside it
int AllocBlock (int w, int h, int *x, int *y) {
	int i, j, best, best2, texnum;

	for (texnum = 0; texnum < MAX_LIGHTMAPS; texnum++) {
		best = BLOCK_HEIGHT;

		for (i = 0; i < BLOCK_WIDTH - w; i++) {
			best2 = 0;

			for (j = 0; j < w; j++) {
				if (allocated[texnum][i+j] >= best)
					break;
				if (allocated[texnum][i+j] > best2)
					best2 = allocated[texnum][i+j];
			}
			if (j == w) {
				// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i = 0; i < w; i++)
			allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error ("AllocBlock: full");
	return 0;
}

mvertex_t	*r_pcurrentvertbase;
model_t		*currentmodel;

void BuildSurfaceDisplayList (msurface_t *fa) {
	int i, lindex, lnumverts, vertpage;
	medge_t *pedges, *r_pedge;
	float *vec, s, t;
	glpoly_t *poly;

	// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	// draw texture
	poly = Hunk_Alloc (sizeof(glpoly_t) + (lnumverts - 4) * VERTEXSIZE*sizeof(float));
	poly->next = fa->polys;
	fa->polys = poly;
	poly->numverts = lnumverts;

	for (i = 0; i < lnumverts; i++) {
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0) {
			r_pedge = &pedges[lindex];
			vec = r_pcurrentvertbase[r_pedge->v[0]].position;
		} else {
			r_pedge = &pedges[-lindex];
			vec = r_pcurrentvertbase[r_pedge->v[1]].position;
		}
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->texture->height;

		VectorCopy (vec, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		// lightmap texture coordinates
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s * 16;
		s += 8;
		s /= BLOCK_WIDTH*16; //fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t * 16;
		t += 8;
		t /= BLOCK_HEIGHT * 16; //fa->texinfo->texture->height;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;

		
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= 128;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= 128;

		VectorCopy (vec, poly->verts[i]);
		poly->verts[i][7] = s;
		poly->verts[i][8] = t;
		
	}

	poly->numverts = lnumverts;
}

void GL_CreateSurfaceLightmap (msurface_t *surf) {
	int smax, tmax;
	byte *base;

	if (surf->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
		return;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	if (smax > BLOCK_WIDTH)
		Host_Error("GL_CreateSurfaceLightmap: smax = %d > BLOCK_WIDTH", smax);
	if (tmax > BLOCK_HEIGHT)
		Host_Error("GL_CreateSurfaceLightmap: tmax = %d > BLOCK_HEIGHT", tmax);
	if (smax * tmax > MAX_LIGHTMAP_SIZE)
		Host_Error("GL_CreateSurfaceLightmap: smax * tmax = %d > MAX_LIGHTMAP_SIZE", smax * tmax);

	surf->lightmaptexturenum = AllocBlock (smax, tmax, &surf->light_s, &surf->light_t);
	base = lightmaps + surf->lightmaptexturenum * BLOCK_WIDTH * BLOCK_HEIGHT * 3;
	base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * 3;
	numdlights = 0;
	R_BuildLightMap (surf, base, BLOCK_WIDTH * 3);
}

//Builds the lightmap texture with all the surfaces from all brush models
void GL_BuildLightmaps (void) {
	int i, j;
	model_t	*m;

	memset (allocated, 0, sizeof(allocated));

	r_framecount = 1;		// no dlightcache

	if (!lightmap_textures) {
		lightmap_textures = texture_extension_number;
		texture_extension_number += MAX_LIGHTMAPS;
	}

	for (j = 1; j < MAX_MODELS; j++) {
		if (!(m = cl.model_precache[j]))
			break;
		if (m->name[0] == '*')
			continue;
		r_pcurrentvertbase = m->vertexes;
		currentmodel = m;
		for (i = 0; i < m->numsurfaces; i++) {
			GL_CreateSurfaceLightmap (m->surfaces + i);
			if ( m->surfaces[i].flags & (SURF_DRAWTURB | SURF_DRAWSKY) )
				continue;
			BuildSurfaceDisplayList (m->surfaces + i);
		}
	}

 	if (gl_mtexable)
 		GL_EnableMultitexture();

	// upload all lightmaps that were filled
	for (i = 0; i < MAX_LIGHTMAPS; i++) {
		if (!allocated[i][0])
			break;		// no more used
		lightmap_modified[i] = false;
		lightmap_rectchange[i].l = BLOCK_WIDTH;
		lightmap_rectchange[i].t = BLOCK_HEIGHT;
		lightmap_rectchange[i].w = 0;
		lightmap_rectchange[i].h = 0;
		GL_Bind(lightmap_textures + i);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D (GL_TEXTURE_2D, 0, gl_lightmap_format, BLOCK_WIDTH, BLOCK_HEIGHT, 0,
			GL_RGB, GL_UNSIGNED_BYTE, lightmaps + i * BLOCK_WIDTH * BLOCK_HEIGHT * 3);
	}

	if (gl_mtexable)
 		GL_DisableMultitexture();
}
