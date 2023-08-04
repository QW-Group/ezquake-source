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
#include "rulesets.h"
#include "utils.h"
#include "r_local.h"
#include "r_texture.h"
#include "r_lightmaps.h"
#include "r_lighting.h"
#include "r_buffers.h"
#include "r_framestats.h"
#include "r_lightmaps_internal.h"
#include "r_trace.h"
#include "r_renderer.h"
#include "tr_types.h"

typedef struct dlightinfo_s {
	int local[2];
	int rad;
	int minlight;	// rad - minlight
	int lnum; // reference to cl_dlights[]
	int color[3];
} dlightinfo_t;

static unsigned int blocklights[MAX_LIGHTMAP_SIZE * 3];

lightmap_data_t* lightmaps;
static unsigned int last_lightmap_updated;
unsigned int lightmap_array_size;

static qbool gl_invlightmaps = true;

static dlightinfo_t dlightlist[MAX_DLIGHTS];
static int numdlights;

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

static void R_BuildDlightList (msurface_t *surf)
{
	float dist;
	vec3_t impact;
	mtexinfo_t *tex;
	int lnum, i, smax, tmax, irad, iminlight, local[2], tdmin, sdmin, distmin;
	unsigned int dlightbits;
	dlightinfo_t *light;

	numdlights = 0;

	smax = (surf->extents[0] >> surf->lmshift) + 1;
	tmax = (surf->extents[1] >> surf->lmshift) + 1;
	tex = surf->texinfo;
	dlightbits = surf->dlightbits;

	for (lnum = 0; lnum < MAX_DLIGHTS && dlightbits; lnum++) {
		if (!(surf->dlightbits & (1 << lnum))) {
			continue;		// not lit by this light
		}

		dlightbits &= ~(1<<lnum);

		dist = PlaneDiff(cl_dlights[lnum].origin, surf->plane);
		irad = (cl_dlights[lnum].radius - fabs(dist)) * 256;
		iminlight = cl_dlights[lnum].minlight * 256;
		if (irad < iminlight) {
			continue;
		}

		iminlight = irad - iminlight;

		for (i = 0; i < 3; i++) {
			impact[i] = cl_dlights[lnum].origin[i] - surf->plane->normal[i] * dist;
		}

		local[0] = DotProduct (impact, surf->lmvecs[0]) + surf->lmvecs[0][3] - surf->texturemins[0];
		local[1] = DotProduct (impact, surf->lmvecs[1]) + surf->lmvecs[1][3] - surf->texturemins[1];

		// check if this dlight will touch the surface
		if (local[1] > 0) {
			tdmin = local[1] - (tmax << surf->lmshift);
			if (tdmin < 0) {
				tdmin = 0;
			}
		}
		else {
			tdmin = -local[1];
		}

		if (local[0] > 0) {
			sdmin = local[0] - (smax << surf->lmshift);
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
			extern cvar_t gl_colorlights;

			// save dlight info
			light = &dlightlist[numdlights];
			light->minlight = iminlight >> 7;
			light->rad = irad >> 7;
			light->local[0] = local[0];
			light->local[1] = local[1];
			light->lnum = lnum;

			if (gl_colorlights.integer) {
				if (cl_dlights[light->lnum].type == lt_custom) {
					VectorCopy(cl_dlights[light->lnum].color, light->color);
				}
				else {
					VectorCopy(dlightcolor[cl_dlights[light->lnum].type], light->color);
				}
			}
			else {
				VectorSet(light->color, 128, 128, 128);
			}
			numdlights++;
		}
	}
}

//R_BuildDlightList must be called first!
static void R_AddDynamicLights(msurface_t *surf)
{
	int i, smax, tmax, s, t, sd, td, _sd, _td, irad, idist, iminlight, tmp;
	dlightinfo_t *light;
	unsigned *dest;

	smax = (surf->extents[0] >> surf->lmshift) + 1;
	tmax = (surf->extents[1] >> surf->lmshift) + 1;

	for (i = 0, light = dlightlist; i < numdlights; i++, light++) {
		irad = light->rad;
		iminlight = light->minlight;

		for (t = 0, _td = light->local[1]; t < tmax; t++, _td -= (1 << surf->lmshift)) {
			td = _td < 0 ? -_td : _td;
			td *= surf->lmvlen[1];

			if (td < irad) {
				dest = blocklights + t * smax * 3;
				for (s = 0, _sd = light->local[0]; s < smax; s++, _sd -= (1 << surf->lmshift), dest += 3) {
					sd = _sd < 0 ? -_sd : _sd;
					sd *= surf->lmvlen[0];

					if (sd + td < iminlight) {
						idist = sd + td;
						if (sd > td) {
							idist += sd;
						}
						else {
							idist += td;
						}

						if (idist < iminlight) {
							tmp = irad - idist;
							dest[0] += tmp * light->color[0];
							dest[1] += tmp * light->color[1];
							dest[2] += tmp * light->color[2];
						}
					}
					else if (_sd < 0) {
						s = smax;
					}
				}
			}
			else if (_td < 0) {
				t = tmax;
			}
		}
	}
}

//Combine and scale multiple lightmaps into the 8.8 format in blocklights
static void R_BuildLightMap(msurface_t *surf, byte *dest, int stride)
{
	int smax, tmax, i, j, size, blocksize, maps;
	byte *lightmap;
	unsigned scale, *bl;
	qbool fullbright = false;

	R_TraceEnterRegion(va("R_BuildLightMap(%d)", surf->surfacenum), true);

	surf->cached_dlight = !!numdlights;

	smax = (surf->extents[0] >> surf->lmshift) + 1;
	tmax = (surf->extents[1] >> surf->lmshift) + 1;
	size = smax * tmax;
	blocksize = size * 3;
	lightmap = surf->samples;

	// check for full bright or no light data
	fullbright = (R_FullBrightAllowed() || !cl.worldmodel || !cl.worldmodel->lightdata);

	if (fullbright) {	// set to full bright
		for (i = 0; i < blocksize; i++) {
			blocklights[i] = 255 << 8;
		}
	}
	else {
		// clear to no light
		memset(blocklights, 0, blocksize * sizeof(int));
	}

	// add all the lightmaps
	for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++) {
		scale = d_lightstylevalue[surf->styles[maps]];
		surf->cached_light[maps] = scale;	// 8.8 fraction

		if (!fullbright && lightmap) {
			bl = blocklights;
			for (i = 0; i < blocksize; i++) {
				*bl++ += lightmap[i] * scale;
			}
			lightmap += blocksize;		// skip to next lightmap
		}
	}

	// add all the dynamic lights
	if (!fullbright && numdlights) {
		R_AddDynamicLights(surf);
	}

	// bound, invert, and shift
	bl = blocklights;
	stride -= smax * 4;

	scale = (lightmode == 2) ? (int)(256 * 1.5) : 256 * 2;
	scale *= bound(0.5, gl_modulate.value, 3);
	for (i = 0; i < tmax; i++, dest += stride) {
		for (j = smax; j; j--) {
			unsigned r, g, b, m;
			r = bl[0] * scale;
			g = bl[1] * scale;
			b = bl[2] * scale;
			m = max(r, g);
			m = max(m, b);
			if (m > ((255 << 16) + (1 << 15))) {
				unsigned s = (((255 << 16) + (1 << 15)) << 8) / m;
				r = (r >> 8) * s;
				g = (g >> 8) * s;
				b = (b >> 8) * s;
			}
			if (gl_invlightmaps) {
				if (GL_Supported(R_SUPPORT_BGRA_LIGHTMAPS)) {
					dest[2] = 255 - (r >> 16);
					dest[1] = 255 - (g >> 16);
					dest[0] = 255 - (b >> 16);
				}
				else {
					dest[0] = 255 - (r >> 16);
					dest[1] = 255 - (g >> 16);
					dest[2] = 255 - (b >> 16);
				}
			}
			else {
				if (GL_Supported(R_SUPPORT_BGRA_LIGHTMAPS)) {
					dest[2] = r >> 16;
					dest[1] = g >> 16;
					dest[0] = b >> 16;
				}
				else {
					dest[0] = r >> 16;
					dest[1] = g >> 16;
					dest[2] = b >> 16;
				}
			}
			dest[3] = 255;
			bl += 3;
			dest += 4;
		}
	}

	R_TraceLeaveFunctionRegion;
}

void R_UploadLightMap(int textureUnit, int lightmapnum)
{
	lightmap_data_t* lm = &lightmaps[lightmapnum];

	if (lm->modified) {
		R_TraceEnterFunctionRegion;
		lm->modified = false;
		renderer.UploadLightmap(textureUnit, lightmapnum);
		lm->change_area.l = LIGHTMAP_WIDTH;
		lm->change_area.t = LIGHTMAP_HEIGHT;
		lm->change_area.h = 0;
		lm->change_area.w = 0;
		++frameStats.lightmap_updates;
		R_TraceLeaveFunctionRegion;
	}
}

void R_RenderDynamicLightmaps(msurface_t *fa, qbool world)
{
	byte *base;
	int maps, smax, tmax;
	glRect_t *theRect;
	qbool lightstyle_modified = false;
	lightmap_data_t* lm;

	if (!R_SoftwareLighting() && !fa->cached_dlight) {
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

	if (R_SoftwareLighting()) {
		if (fa->dlightframe == r_framecount) {
			R_BuildDlightList(fa);
		}
		else {
			numdlights = 0;
		}

		if (numdlights == 0 && !fa->cached_dlight && !lightstyle_modified) {
			return;
		}
	}
	else {
		numdlights = 0;
	}

	lm = &lightmaps[fa->lightmaptexturenum];
	lm->modified = true;
	theRect = &lm->change_area;
	if (fa->light_t < theRect->t) {
		if (theRect->h) {
			theRect->h += theRect->t - fa->light_t;
		}
		theRect->t = fa->light_t;
	}
	if (fa->light_s < theRect->l) {
		if (theRect->w) {
			theRect->w += theRect->l - fa->light_s;
		}
		theRect->l = fa->light_s;
	}
	smax = (fa->extents[0] >> fa->lmshift) + 1;
	tmax = (fa->extents[1] >> fa->lmshift) + 1;
	if (theRect->w + theRect->l < fa->light_s + smax) {
		theRect->w = fa->light_s - theRect->l + smax;
	}
	if (theRect->h + theRect->t < fa->light_t + tmax) {
		theRect->h = fa->light_t - theRect->t + tmax;
	}
	base = lm->rawdata + (fa->light_t * LIGHTMAP_WIDTH + fa->light_s) * 4;
	R_BuildLightMap (fa, base, LIGHTMAP_WIDTH * 4);
}

void R_LightmapFrameInit(void)
{
	frameStats.lightmap_min_changed = lightmap_array_size;
	frameStats.lightmap_max_changed = 0;

	renderer.LightmapFrameInit();
}

static void R_RenderAllDynamicLightmapsForChain(msurface_t* surface, qbool world)
{
	int k;
	msurface_t* s;
	frameStatsPolyType polyType;

	if (!surface) {
		return;
	}

	R_TraceEnterFunctionRegion;

	polyType = world ? polyTypeWorldModel : polyTypeBrushModel;
	for (s = surface; s; s = s->texturechain) {
		k = s->lightmaptexturenum;

		++frameStats.classic.polycount[polyType];

		if (k >= 0 && !(s->flags & (SURF_DRAWTURB | SURF_DRAWSKY))) {
			renderer.RenderDynamicLightmaps(s, world);

			if (lightmaps[k].modified) {
				frameStats.lightmap_min_changed = min(k, frameStats.lightmap_min_changed);
				frameStats.lightmap_max_changed = max(k, frameStats.lightmap_max_changed);
			}
		}
	}

	for (s = surface->drawflatchain; s; s = s->drawflatchain) {
		k = s->lightmaptexturenum;

		++frameStats.classic.polycount[polyType];

		if (k >= 0 && !(s->flags & (SURF_DRAWTURB | SURF_DRAWSKY))) {
			renderer.RenderDynamicLightmaps(s, world);

			if (lightmaps[k].modified) {
				frameStats.lightmap_min_changed = min(k, frameStats.lightmap_min_changed);
				frameStats.lightmap_max_changed = max(k, frameStats.lightmap_max_changed);
			}
		}
	}

	R_TraceLeaveFunctionRegion;
}

void R_UploadChangedLightmaps(void)
{
	if (r_lightmap_lateupload.integer == 1) {
		return;
	}

	R_TraceEnterNamedRegion(__func__);
	if (R_HardwareLighting()) {
		if (R_FullBrightAllowed() || !cl.worldmodel || !cl.worldmodel->lightdata) {
			int i;

			for (i = frameStats.lightmap_min_changed; i <= frameStats.lightmap_max_changed; ++i) {
				if (lightmaps[i].modified) {
					memset(lightmaps[i].rawdata, 255, sizeof(lightmaps[i].rawdata));
					R_UploadLightMap(0, i);
				}
			}

			frameStats.lightmap_min_changed = lightmap_array_size;
			frameStats.lightmap_max_changed = 0;
		}
#ifdef RENDERER_OPTION_MODERN_OPENGL
		else if (R_UseModernOpenGL()) {
			GLM_ComputeLightmaps();
		}
#endif
	}
	else if (frameStats.lightmap_min_changed < lightmap_array_size) {
		unsigned int i;

		for (i = frameStats.lightmap_min_changed; i <= frameStats.lightmap_max_changed; ++i) {
			if (lightmaps[i].modified) {
				R_UploadLightMap(0, i);
			}
		}

		frameStats.lightmap_min_changed = lightmap_array_size;
		frameStats.lightmap_max_changed = 0;
	}
	R_TraceLeaveNamedRegion();
}

void R_RenderAllDynamicLightmaps(model_t *model)
{
	unsigned int i;

	R_TraceEnterFunctionRegion;

	for (i = 0; i < model->numtextures; i++) {
		if (!model->textures[i]) {
			continue;
		}

		R_TraceEnterRegion(model->textures[i]->name, true);
		R_RenderAllDynamicLightmapsForChain(model->textures[i]->texturechain, model->isworldmodel);
		R_TraceLeaveRegion(true);
	}

	R_TraceEnterRegion("drawflat_chain", true);
	R_RenderAllDynamicLightmapsForChain(model->drawflat_chain, model->isworldmodel);
	R_TraceLeaveRegion(true);

	for (i = 0; i < R_LightmapCount(); ++i) {
		msurface_t* surf = R_DrawflatLightmapChain(i);

		if (surf) {
			R_TraceEnterRegion(va("lightmapChain[%d]", i), true);
			R_RenderAllDynamicLightmapsForChain(surf, model->isworldmodel);
			R_TraceLeaveRegion(true);
		}
	}

	if (R_UseImmediateOpenGL()) {
		R_UploadChangedLightmaps();
	}

	R_TraceLeaveFunctionRegion;
}

// returns a lightmap number and the position inside it
static int LightmapAllocBlock(int w, int h, int *x, int *y)
{
	int i, j, best, best2;
	int texnum;

	if (w < 1 || w > LIGHTMAP_WIDTH || h < 1 || h > LIGHTMAP_HEIGHT) {
		Sys_Error("AllocBlock: Bad dimensions (w: %d, h: %d)", w, h);
	}

	for (texnum = last_lightmap_updated; texnum < lightmap_array_size; texnum++, last_lightmap_updated++) {
		best = LIGHTMAP_HEIGHT + 1;

		for (i = 0; i < LIGHTMAP_WIDTH - w; i++) {
			best2 = 0;

			for (j = i; j < i + w; j++) {
				if (lightmaps[texnum].allocated[j] >= best) {
					i = j;
					break;
				}
				if (lightmaps[texnum].allocated[j] > best2) {
					best2 = lightmaps[texnum].allocated[j];
				}
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
			lightmaps[texnum].allocated[*x + i] = best + h;
		}

		return texnum;
	}

	// Dynamically increase array
	{
		unsigned int new_size = lightmap_array_size + LIGHTMAP_ARRAY_GROWTH;
		lightmaps = Q_realloc(lightmaps, sizeof(lightmaps[0]) * new_size);
		if (!lightmaps) {
			Sys_Error("AllocBlock: full");
			return 0;
		}
		memset(lightmaps + lightmap_array_size, 0, sizeof(lightmaps[0]) * LIGHTMAP_ARRAY_GROWTH);
		lightmap_array_size = new_size;

		// Memory pointers might now be invalid afer realloc()
		for (i = 0; i < new_size; ++i) {
			lightmaps[i].drawflat_chain_tail = &lightmaps[i].drawflat_chain;
		}
	}
	return LightmapAllocBlock(w, h, x, y);
}

// 
static void R_BuildSurfaceDisplayList(model_t* currentmodel, msurface_t *fa)
{
	int i, lindex, lnumverts;
	medge_t *pedges, *r_pedge;
	float *vec, s, t;
	glpoly_t *poly;
	qbool isTurb = (fa->flags & SURF_DRAWTURB);

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
			vec = currentmodel->vertexes[r_pedge->v[0]].position;
		}
		else {
			r_pedge = &pedges[-lindex];
			vec = currentmodel->vertexes[r_pedge->v[1]].position;
		}

		s = DotProduct(vec, fa->texinfo->vecs[0]);
		if (!isTurb) {
			s += fa->texinfo->vecs[0][3];
			s /= fa->texinfo->texture->width;
		}
		else {
			s /= 64;
		}

		t = DotProduct(vec, fa->texinfo->vecs[1]);
		if (!isTurb) {
			t += fa->texinfo->vecs[1][3];
			t /= fa->texinfo->texture->height;
		}
		else {
			t /= 64;
		}

		VectorCopy(vec, poly->verts[i]);

		// material
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		// lightmap texture coordinates
		if (isTurb && !fa->texinfo->texture->isLitTurb) {
			poly->verts[i][5] = poly->verts[i][6] = 0;
			poly->verts[i][7] = poly->verts[i][8] = 0;
		}
		else {
			s = DotProduct(vec, fa->lmvecs[0]) + fa->lmvecs[0][3];
			s -= fa->texturemins[0];
			s += fa->light_s * (1 << fa->lmshift);
			s += (1 << fa->lmshift) * 0.5;
			s /= LIGHTMAP_WIDTH * (1 << fa->lmshift);

			t = DotProduct(vec, fa->lmvecs[1]) + fa->lmvecs[1][3];
			t -= fa->texturemins[1];
			t += fa->light_t * (1 << fa->lmshift);
			t += (1 << fa->lmshift) * 0.5;
			t /= LIGHTMAP_HEIGHT * (1 << fa->lmshift);

			poly->verts[i][5] = s;
			poly->verts[i][6] = t;

			s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
			s /= 128;

			t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
			t /= 128;

			// Detail textures
			poly->verts[i][7] = 18 * s;
			poly->verts[i][8] = 18 * t;
		}
	}
	poly->numverts = lnumverts;

	R_BrushModelPolygonToTriangleStrip(poly);
	return;
}

static void R_BuildLightmapData(msurface_t* surf, int surfnum)
{
	lightmap_data_t* lm = &lightmaps[surf->lightmaptexturenum];
	byte* lightmap = surf->samples;
	unsigned int smax = (surf->extents[0] >> surf->lmshift) + 1;
	unsigned int tmax = (surf->extents[1] >> surf->lmshift) + 1;
	unsigned int lightmap_flags;
	int s, t, maps;

	lightmap_flags = 0xFFFFFFFF;
	for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++) {
		lightmap_flags &= ~(0xFF << (8 * maps));
		lightmap_flags |= surf->styles[maps] << (8 * maps);
	}

	for (t = 0; t < tmax; ++t) {
		size_t offset = ((surf->light_t + t) * LIGHTMAP_WIDTH + surf->light_s) * 4;
		int* data = &lm->computeData[offset];
		unsigned int* source = &lm->sourcedata[offset];

		for (s = 0; s < smax; ++s, data += 4, source += 4) {
			data[0] = surfnum;
			data[1] = (s * (1 << surf->lmshift) + surf->texturemins[0] - surf->lmvecs[0][3]);
			data[2] = (t * (1 << surf->lmshift) + surf->texturemins[1] - surf->lmvecs[1][3]);
			data[3] = 0;

			source[0] = source[1] = source[2] = 0;
			source[3] = lightmap_flags;

			if (lightmap) {
				for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++) {
					size_t lightmap_index = (maps * smax * tmax + t * smax + s) * 3;

					source[0] |= ((unsigned int)lightmap[lightmap_index + 0]) << (8 * maps);
					source[1] |= ((unsigned int)lightmap[lightmap_index + 1]) << (8 * maps);
					source[2] |= ((unsigned int)lightmap[lightmap_index + 2]) << (8 * maps);
				}
			}
		}
	}
}

static void R_LightmapCreateForSurface(msurface_t *surf, int surfnum)
{
	int smax, tmax;
	byte *base;

	smax = (surf->extents[0] >> surf->lmshift) + 1;
	tmax = (surf->extents[1] >> surf->lmshift) + 1;

	if (smax > LIGHTMAP_WIDTH) {
		Host_Error("%s: smax = %d > BLOCK_WIDTH", __func__, smax);
	}
	if (tmax > LIGHTMAP_HEIGHT) {
		Host_Error("%s: tmax = %d > BLOCK_HEIGHT", __func__, tmax);
	}
	if (smax * tmax > MAX_LIGHTMAP_SIZE) {
		Host_Error("%s: smax * tmax = %d > MAX_LIGHTMAP_SIZE", __func__, smax * tmax);
	}

	surf->lightmaptexturenum = LightmapAllocBlock(smax, tmax, &surf->light_s, &surf->light_t);

	base = lightmaps[surf->lightmaptexturenum].rawdata + (surf->light_t * LIGHTMAP_WIDTH + surf->light_s) * 4;
	numdlights = 0;
	R_BuildLightmapData(surf, surfnum);
	R_BuildLightMap(surf, base, LIGHTMAP_WIDTH * 4);
}

static int R_LightmapSurfaceSortFunction(const void* lhs_, const void* rhs_)
{
	const msurface_t* lhs = *(const msurface_t**)lhs_;
	const msurface_t* rhs = *(const msurface_t**)rhs_;

	if (r_lightmap_packbytexture.integer == 1) {
		int lhs_size = ((lhs->extents[0] >> lhs->lmshift) + 1) * ((lhs->extents[1] >> lhs->lmshift) + 1);
		int rhs_size = ((rhs->extents[0] >> rhs->lmshift) + 1) * ((rhs->extents[1] >> rhs->lmshift) + 1);

		return rhs_size - lhs_size;
	}
	else if (r_lightmap_packbytexture.integer == 2) {
		int lhs_width = ((lhs->extents[0] >> lhs->lmshift) + 1);
		int lhs_height = ((lhs->extents[1] >> lhs->lmshift) + 1);
		int rhs_width = ((rhs->extents[0] >> rhs->lmshift) + 1);
		int rhs_height = ((rhs->extents[1] >> rhs->lmshift) + 1);

		int height_diff = rhs_height - lhs_height;
		int width_diff = rhs_width - lhs_width;

		if (height_diff) {
			return height_diff;
		}
		return width_diff;
	}
	else {
		return 0;
	}
}

//Builds the lightmap texture with all the surfaces from all brush models
//Only called when map is initially loaded
void R_BuildLightmaps(void)
{
	int i, j, t;
	model_t	*m;

	if (lightmaps) {
		for (i = 0; i < lightmap_array_size; ++i) {
			lightmaps[i].drawflat_chain = NULL;
			lightmaps[i].drawflat_chain_tail = &lightmaps[i].drawflat_chain;
			memset(lightmaps[i].allocated, 0, sizeof(lightmaps[i].allocated));
		}
	}
	else {
		lightmaps = Q_malloc(sizeof(*lightmaps) * LIGHTMAP_ARRAY_GROWTH);
		lightmap_array_size = LIGHTMAP_ARRAY_GROWTH;
		for (i = 0; i < lightmap_array_size; ++i) {
			lightmaps[i].drawflat_chain_tail = &lightmaps[i].drawflat_chain;
		}
	}
	last_lightmap_updated = 0;

	gl_invlightmaps = R_UseImmediateOpenGL();

	r_framecount = 1;		// no dlightcache
	for (j = 1; j < MAX_MODELS; j++) {
		if (!(m = cl.model_precache[j])) {
			break;
		}
		if (m->name[0] == '*') {
			continue;
		}

		// mark all surfaces as needing lightmap
		for (i = 0; i < m->numsurfaces; i++) {
			m->surfaces[i].surfacenum = i;
			m->surfaces[i].lightmaptexturenum = -1;
		}

		// assign in order based on texture and then by size
		if (r_lightmap_packbytexture.integer) {
			msurface_t** surfaces = Q_malloc(m->numsurfaces * sizeof(msurface_t*));

			for (t = 0; t < m->numtextures; ++t) {
				int surface_count = 0;

				// create list of surfaces that need processed for this texture
				for (i = 0; i < m->numsurfaces; i++) {
					qbool isTurb = (m->surfaces[i].flags & SURF_DRAWTURB);
					qbool isSky = (m->surfaces[i].flags & SURF_DRAWSKY);

					if (m->surfaces[i].texinfo->miptex != t) {
						continue;
					}
					if (isSky || m->surfaces[i].lightmaptexturenum >= 0) {
						continue;
					}
					if (!isTurb && (m->surfaces[i].texinfo->flags & TEX_SPECIAL)) {
						continue;
					}

					surfaces[surface_count++] = &m->surfaces[i];
				}

				// sort, biggest first
				qsort(surfaces, surface_count, sizeof(surfaces[0]), R_LightmapSurfaceSortFunction);

				//
				for (i = 0; i < surface_count; ++i) {
					qbool isTurb = (surfaces[i]->flags & SURF_DRAWTURB);

					if (!isTurb) {
						R_LightmapCreateForSurface(surfaces[i], m->isworldmodel ? surfaces[i]->surfacenum : -1);
					}
					R_BuildSurfaceDisplayList(m, surfaces[i]);
				}
			}

			Q_free(surfaces);
		}

		// Double-check: go through all textures again
		for (i = 0; i < m->numsurfaces; i++) {
			qbool isTurb = (m->surfaces[i].flags & SURF_DRAWTURB);
			qbool isSky = (m->surfaces[i].flags & SURF_DRAWSKY);

			if (isSky || m->surfaces[i].lightmaptexturenum >= 0) {
				continue;
			}
			if (!isTurb && (m->surfaces[i].texinfo->flags & TEX_SPECIAL)) {
				continue;
			}

			if (!isTurb || !(m->surfaces[i].texinfo->flags & TEX_SPECIAL)) {
				R_LightmapCreateForSurface(m->surfaces + i, m->isworldmodel ? m->surfaces[i].surfacenum : -1);
			}
			R_BuildSurfaceDisplayList(m, m->surfaces + i);
		}
	}

	// upload all lightmaps that were filled
	renderer.CreateLightmapTextures();

	for (i = 0; i < lightmap_array_size; i++) {
		if (!lightmaps[i].allocated[0]) {
			break;		// not used anymore
		}
		lightmaps[i].modified = false;
		lightmaps[i].change_area.l = LIGHTMAP_WIDTH;
		lightmaps[i].change_area.t = LIGHTMAP_HEIGHT;
		lightmaps[i].change_area.w = 0;
		lightmaps[i].change_area.h = 0;
		renderer.BuildLightmap(i);
	}
}

void R_InvalidateLightmapTextures(void)
{
	int i;

	if (renderer.InvalidateLightmapTextures) {
		renderer.InvalidateLightmapTextures();
	}

	for (i = 0; i < lightmap_array_size; ++i) {
		R_TextureReferenceInvalidate(lightmaps[i].gl_texref);
	}

	Q_free(lightmaps);
	lightmap_array_size = 0;
	last_lightmap_updated = 0;
}

void R_LightmapShutdown(void)
{
	Q_free(lightmaps);
	lightmap_array_size = 0;

	if (renderer.LightmapShutdown) {
		renderer.LightmapShutdown();
	}
}

// mark all surfaces so ALL light maps will reload in R_RenderDynamicLightmaps()
void R_ForceReloadLightMaps(void)
{
	model_t	*m;
	int i, j;

	Com_DPrintf("forcing of reloading all light maps!\n");

	for (j = 1; j < MAX_MODELS; j++) {
		if (!(m = cl.model_precache[j])) {
			break;
		}

		if (m->name[0] == '*') {
			continue;
		}

		for (i = 0; i < m->numsurfaces; i++) {
			m->surfaces[i].cached_dlight = true; // kinda hack, so we force reload light map
		}
	}
}

qbool R_FullBrightAllowed(void)
{
	extern cvar_t r_fullbright;

	return r_fullbright.value && r_refdef2.allow_cheats;
}

void R_CheckReloadLightmaps(void)
{
	static qbool allowed;
	static qbool hardware_lighting;
	static float modulate;

	// not changed, nothing to do
	if (allowed == R_FullBrightAllowed() && hardware_lighting == R_HardwareLighting() && modulate == gl_modulate.value) {
		return;
	}

	// ok, it changed, lets update all our light maps...
	allowed = R_FullBrightAllowed();
	hardware_lighting = R_HardwareLighting();
	modulate = gl_modulate.value;

	R_ForceReloadLightMaps();
}

unsigned int R_LightmapCount(void)
{
	return lightmap_array_size;
}

struct msurface_s* R_DrawflatLightmapChain(int i)
{
	return lightmaps[i].drawflat_chain;
}

void R_ClearDrawflatLightmapChain(int i)
{
	lightmaps[i].drawflat_chain = NULL;
	lightmaps[i].drawflat_chain_tail = &lightmaps[i].drawflat_chain;
}

void R_AddDrawflatChainSurface(struct msurface_s* surf, qbool floor)
{
	lightmap_data_t* lm = &lightmaps[surf->lightmaptexturenum];

	if (!lm->drawflat_chain_tail) {
		lm->drawflat_chain_tail = &lm->drawflat_chain;
	}

	if (floor) {
		*lm->drawflat_chain_tail = surf;
		lm->drawflat_chain_tail = &surf->drawflatchain;
		surf->drawflatchain = NULL;
	}
	else {
		surf->drawflatchain = lm->drawflat_chain;
		if (lm->drawflat_chain_tail == &lm->drawflat_chain) {
			lm->drawflat_chain_tail = &surf->drawflatchain;
		}
		lm->drawflat_chain = surf;
	}
}
