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
// gl_lightmaps.c: lightmap-related code

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"
#include "utils.h"

#define	BLOCK_WIDTH  128
#define	BLOCK_HEIGHT 128

#define MAX_LIGHTMAP_SIZE	(32 * 32) // it was 4096 for quite long time

texture_ref lightmap_texture_array;
texture_ref lightmap_textures[MAX_LIGHTMAPS];
static unsigned blocklights[MAX_LIGHTMAP_SIZE * 3];

typedef struct glRect_s {
	unsigned char l, t, w, h;
} glRect_t;

static glpoly_t	*lightmap_polys[MAX_LIGHTMAPS];
static glRect_t	lightmap_rectchange[MAX_LIGHTMAPS];
qbool lightmap_modified[MAX_LIGHTMAPS];

static int allocated[MAX_LIGHTMAPS][LIGHTMAP_WIDTH];
static GLuint last_lightmap_updated;

// the lightmap texture data needs to be kept in
// main memory so texsubimage can update properly
byte	lightmaps[4 * MAX_LIGHTMAPS * LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT];

qbool gl_invlightmaps = true;

typedef struct dlightinfo_s {
	int local[2];
	int rad;
	int minlight;	// rad - minlight
	int lnum; // reference to cl_dlights[]
} dlightinfo_t;

static dlightinfo_t dlightlist[MAX_DLIGHTS];
static int numdlights;

void R_BuildDlightList (msurface_t *surf)
{
	float dist;
	vec3_t impact;
	mtexinfo_t *tex;
	int lnum, i, smax, tmax, irad, iminlight, local[2], tdmin, sdmin, distmin;
	unsigned int dlightbits;
	dlightinfo_t *light;

	numdlights = 0;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;
	dlightbits = surf->dlightbits;

	for (lnum = 0; lnum < MAX_DLIGHTS && dlightbits; lnum++) {
		if ( !(surf->dlightbits & (1 << lnum) ) )
			continue;		// not lit by this light

		dlightbits &= ~(1<<lnum);

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
			light->lnum = lnum;
			numdlights++;
		}
	}
}

// funny, but this colors differ from bubblecolor[NUM_DLIGHTTYPES][4]
int dlightcolor[NUM_DLIGHTTYPES][3] = {
	{ 100,  90,  80 },	// dimlight or brightlight
	{ 100,  50,  10 },	// muzzleflash
	{ 100,  50,  10 },	// explosion
	{  90,  60,   7 },	// rocket
	{ 128,   0,   0 },	// red
	{   0,   0, 128 },	// blue
	{ 128,   0, 128 },	// red + blue
	{   0, 128,   0 },	// green
	{ 128, 128,   0 }, 	// red + green
	{   0, 128, 128 }, 	// blue + green
	{ 128, 128, 128 },	// white
	{ 128, 128, 128 },	// custom
};


//R_BuildDlightList must be called first!
void R_AddDynamicLights (msurface_t *surf) {
	int i, smax, tmax, s, t, sd, td, _sd, _td, irad, idist, iminlight, color[3], tmp;
	dlightinfo_t *light;
	unsigned *dest;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;

	for (i = 0, light = dlightlist; i < numdlights; i++, light++) {
		extern cvar_t gl_colorlights;
		if (gl_colorlights.value) {
			if (cl_dlights[light->lnum].type == lt_custom)
				VectorCopy(cl_dlights[light->lnum].color, color);
			else
				VectorCopy(dlightcolor[cl_dlights[light->lnum].type], color);
		} else {
			VectorSet(color, 128, 128, 128);
		}

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
	int smax, tmax, i, j, size, blocksize, maps;
	byte *lightmap;
	unsigned scale, *bl;
	qbool fullbright = false;

	surf->cached_dlight = !!numdlights;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	size = smax * tmax;
	blocksize = size * 3;
	lightmap = surf->samples;

	// check for full bright or no light data
	fullbright = (R_FullBrightAllowed() || !cl.worldmodel || !cl.worldmodel->lightdata);

	if (fullbright)
	{	// set to full bright
		for (i = 0; i < blocksize; i++)
			blocklights[i] = 255 << 8;
	}
	else
	{
		// clear to no light
		memset (blocklights, 0, blocksize * sizeof(int));
	}

	// add all the lightmaps
	for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
	{
		scale = d_lightstylevalue[surf->styles[maps]];
		surf->cached_light[maps] = scale;	// 8.8 fraction

		if (!fullbright && lightmap)
		{
			bl = blocklights;
			for (i = 0; i < blocksize; i++)
				*bl++ += lightmap[i] * scale;
			lightmap += blocksize;		// skip to next lightmap
		}
	}

	// add all the dynamic lights
	if (!fullbright)
	{
		if (numdlights)
			R_AddDynamicLights (surf);
	}

	// bound, invert, and shift
	bl = blocklights;
	stride -= smax * 4;
	for (i = 0; i < tmax; i++, dest += stride) {
		scale = (lightmode == 2) ? (int)(256 * 1.5) : 256 * 2;
		scale *= bound(0.5, gl_modulate.value, 3);
		for (j = smax; j; j--) {
			unsigned r, g, b, m;
			r = bl[0] * scale;
			g = bl[1] * scale;
			b = bl[2] * scale;
			m = max(r, g);
			m = max(m, b);
			if (m > ((255<<16) + (1<<15))) {
				unsigned s = (((255<<16) + (1<<15)) << 8) / m;
				r = (r >> 8) * s;
				g = (g >> 8) * s;
				b = (b >> 8) * s;
			}
			if (gl_invlightmaps) {
				dest[2] = 255 - (r >> 16);
				dest[1] = 255 - (g >> 16);
				dest[0] = 255 - (b >> 16);
			} else {
				dest[2] = r >> 16;
				dest[1] = g >> 16;
				dest[0] = b >> 16;
			}
			dest[3] = 255;
			bl += 3;
			dest += 4;
		}
	}
}

void R_UploadLightMap(GLenum textureUnit, int lightmapnum)
{
	glRect_t	*theRect;

	lightmap_modified[lightmapnum] = false;
	theRect = &lightmap_rectchange[lightmapnum];
	if (GL_TextureReferenceIsValid(lightmap_texture_array)) {
		GL_TexSubImage3D(textureUnit, lightmap_texture_array, 0, 0, theRect->t, lightmapnum, LIGHTMAP_WIDTH, theRect->h, 1, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, lightmaps + (lightmapnum * LIGHTMAP_HEIGHT + theRect->t) * LIGHTMAP_WIDTH * 4);
	}
	else {
		GL_TexSubImage2D(textureUnit, lightmap_textures[lightmapnum], 0, 0, theRect->t, LIGHTMAP_WIDTH, theRect->h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
			lightmaps + (lightmapnum * LIGHTMAP_HEIGHT + theRect->t) * LIGHTMAP_WIDTH * 4);
	}
	theRect->l = LIGHTMAP_WIDTH;
	theRect->t = LIGHTMAP_HEIGHT;
	theRect->h = 0;
	theRect->w = 0;
	++frameStats.lightmap_updates;
}

void R_RenderDynamicLightmaps(msurface_t *fa)
{
	byte *base;
	int maps, smax, tmax;
	glRect_t *theRect;
	qbool lightstyle_modified = false;

	++frameStats.classic.brush_polys;

	if (!r_dynamic.value && !fa->cached_dlight) {
		return;
	}

	if (fa->lightmaptexturenum < 0) {
		return;
	}

	// check for lightmap modification
	for (maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++) {
		if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps]) {
			lightstyle_modified = true;
			break;
		}
	}

	if (r_dynamic.value) {
		if (fa->dlightframe == r_framecount) {
			R_BuildDlightList (fa);
		} else {
			numdlights = 0;
		}

		if (numdlights == 0 && !fa->cached_dlight && !lightstyle_modified) {
			return;
		}
	}
	else {
		numdlights = 0;
	}

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
	base = lightmaps + fa->lightmaptexturenum * LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 4;
	base += (fa->light_t * LIGHTMAP_WIDTH + fa->light_s) * 4;
	R_BuildLightMap (fa, base, LIGHTMAP_WIDTH * 4);
}

static void R_RenderAllDynamicLightmapsForChain(msurface_t *s, unsigned int* min_changed, unsigned int* max_changed)
{
	int k;
	extern cvar_t r_turbalpha;

	while (s) {
		k = s->lightmaptexturenum;

		if (k >= 0 && !(s->flags & (SURF_DRAWTURB | SURF_DRAWSKY))) {
			R_RenderDynamicLightmaps(s);
			if (lightmap_modified[k]) {
				*min_changed = min(k, *min_changed);
				*max_changed = max(k, *max_changed);
			}
		}

		s = s->texturechain;
	}
}

void R_RenderAllDynamicLightmaps(model_t *model)
{
	msurface_t *s;
	unsigned int waterline;
	unsigned int i;
	unsigned int min_changed = MAX_LIGHTMAPS;
	unsigned int max_changed = 0;

	for (i = 0; i < model->numtextures; i++) {
		if (!model->textures[i] || (!model->textures[i]->texturechain[0] && !model->textures[i]->texturechain[1])) {
			continue;
		}

		for (waterline = 0; waterline < 2; waterline++) {
			if (!(s = model->textures[i]->texturechain[waterline])) {
				continue;
			}

			R_RenderAllDynamicLightmapsForChain(s, &min_changed, &max_changed);
		}
	}

	R_RenderAllDynamicLightmapsForChain(model->drawflat_chain[0], &min_changed, &max_changed);
	R_RenderAllDynamicLightmapsForChain(model->drawflat_chain[1], &min_changed, &max_changed);

	if (min_changed < MAX_LIGHTMAPS) {
		for (i = min_changed; i <= max_changed; ++i) {
			if (lightmap_modified[i]) {
				R_UploadLightMap(GL_TEXTURE0, i);
			}
		}
	}
}

// returns a lightmap number and the position inside it
int AllocBlock (int w, int h, int *x, int *y) {
	int i, j, best, best2;
	int texnum;

	if (w < 1 || w > LIGHTMAP_WIDTH || h < 1 || h > LIGHTMAP_HEIGHT) {
		Sys_Error("AllocBlock: Bad dimensions");
	}

	for (texnum = last_lightmap_updated; texnum < MAX_LIGHTMAPS; texnum++, last_lightmap_updated++) {
		best = LIGHTMAP_HEIGHT + 1;

		for (i = 0; i < LIGHTMAP_WIDTH - w; i++) {
			best2 = 0;

			for (j = i; j < i + w; j++) {
				if (allocated[texnum][j] >= best) {
					i = j;
					break;
				}
				if (allocated[texnum][j] > best2)
					best2 = allocated[texnum][j];
			}
			if (j == i + w) {
				// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > LIGHTMAP_HEIGHT) {
			continue;
		}

		for (i = 0; i < w; i++) {
			allocated[texnum][*x + i] = best + h;
		}

		return texnum;
	}

	Sys_Error ("AllocBlock: full");
	return 0;
}

mvertex_t	*r_pcurrentvertbase;
model_t		*currentmodel;

void BuildSurfaceDisplayList(msurface_t *fa)
{
	int i, lindex, lnumverts;
	medge_t *pedges, *r_pedge;
	float *vec, s, t;
	glpoly_t *poly;

	// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;

	// draw texture
	if (!fa->polys) { // seems map loaded first time, so light maps loaded first time too
		poly = (glpoly_t *)Hunk_Alloc(sizeof(glpoly_t) + (lnumverts - 4) * VERTEXSIZE * sizeof(float));
		poly->next = fa->polys;
		fa->polys = poly;
		poly->numverts = lnumverts;
	}
	else { // seems vid_restart issued, so do not allocate memory, we alredy done it, I hope
		poly = fa->polys;
	}

	for (i = 0; i < lnumverts; i++) {
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0) {
			r_pedge = &pedges[lindex];
			vec = r_pcurrentvertbase[r_pedge->v[0]].position;
		}
		else {
			r_pedge = &pedges[-lindex];
			vec = r_pcurrentvertbase[r_pedge->v[1]].position;
		}
		s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->texture->width;

		t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->texture->height;

		VectorCopy(vec, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		// lightmap texture coordinates
		s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s * 16;
		s += 8;
		s /= LIGHTMAP_WIDTH * 16; //fa->texinfo->texture->width;

		t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t * 16;
		t += 8;
		t /= LIGHTMAP_HEIGHT * 16; //fa->texinfo->texture->height;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;


		s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= 128;

		t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= 128;

		VectorCopy(vec, poly->verts[i]);
		poly->verts[i][7] = s;
		poly->verts[i][8] = t;
	}

	poly->numverts = lnumverts;
	return;
}

void GL_CreateSurfaceLightmap(msurface_t *surf)
{
	int smax, tmax;
	byte *base;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	if (smax > BLOCK_WIDTH)
		Host_Error("GL_CreateSurfaceLightmap: smax = %d > BLOCK_WIDTH", smax);
	if (tmax > BLOCK_HEIGHT)
		Host_Error("GL_CreateSurfaceLightmap: tmax = %d > BLOCK_HEIGHT", tmax);
	if (smax * tmax > MAX_LIGHTMAP_SIZE)
		Host_Error("GL_CreateSurfaceLightmap: smax * tmax = %d > MAX_LIGHTMAP_SIZE", smax * tmax);

	surf->lightmaptexturenum = AllocBlock(smax, tmax, &surf->light_s, &surf->light_t);
	base = lightmaps + surf->lightmaptexturenum * BLOCK_WIDTH * BLOCK_HEIGHT * 4;
	base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * 4;
	numdlights = 0;
	R_BuildLightMap(surf, base, BLOCK_WIDTH * 4);
}

//Builds the lightmap texture with all the surfaces from all brush models
//Only called when map is initially loaded
void GL_BuildLightmaps(void)
{
	int i, j;
	model_t	*m;

	memset(allocated, 0, sizeof(allocated));
	last_lightmap_updated = 0;

	gl_invlightmaps = !COM_CheckParm("-noinvlmaps");

	r_framecount = 1;		// no dlightcache

	for (j = 1; j < MAX_MODELS; j++) {
		if (!(m = cl.model_precache[j])) {
			break;
		}
		if (m->name[0] == '*') {
			continue;
		}
		r_pcurrentvertbase = m->vertexes;
		currentmodel = m;
		for (i = 0; i < m->numsurfaces; i++) {
			if (m->surfaces[i].flags & (SURF_DRAWTURB | SURF_DRAWSKY)) {
				continue;
			}
			if (m->surfaces[i].texinfo->flags & TEX_SPECIAL) {
				continue;
			}

			GL_CreateSurfaceLightmap(m->surfaces + i);
			BuildSurfaceDisplayList(m->surfaces + i);
		}
	}

	// upload all lightmaps that were filled
	for (i = 0; i < MAX_LIGHTMAPS; i++) {
		if (!allocated[i][0]) {
			break;		// no more used
		}
		lightmap_modified[i] = false;
		lightmap_rectchange[i].l = LIGHTMAP_WIDTH;
		lightmap_rectchange[i].t = LIGHTMAP_HEIGHT;
		lightmap_rectchange[i].w = 0;
		lightmap_rectchange[i].h = 0;
		if (GL_ShadersSupported() && GL_TextureReferenceIsValid(lightmap_texture_array)) {
			GL_TexSubImage3D(GL_TEXTURE0, lightmap_texture_array, 0, 0, 0, i, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, 1, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, lightmaps + i * LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 4);
		}
		else {
			GL_TexSubImage2D(
				GL_TEXTURE0, lightmap_textures[i], 0, 0, 0,
				LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
				lightmaps + i * LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 4
			);
		}
	}
}

void GLC_SetTextureLightmap(GLenum textureUnit, int lightmap_num)
{
	//update lightmap if its modified by dynamic lights
	if (lightmap_modified[lightmap_num]) {
		R_UploadLightMap(textureUnit, lightmap_num);
	}
	else {
		GL_BindTextureUnit(textureUnit, lightmap_textures[lightmap_num]);
	}
}

void GLC_SetLightmapTextureEnvironment(void)
{
	GL_TextureEnvMode(gl_invlightmaps ? GL_BLEND : GL_MODULATE);
}

void GLC_SetLightmapBlendFunc(void)
{
	if (gl_invlightmaps) {
		GL_BlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
	}
	else {
		GL_BlendFunc(GL_ZERO, GL_SRC_COLOR);
	}
}
