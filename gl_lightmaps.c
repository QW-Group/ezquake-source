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
#include "glsl/constants.glsl"

// Lightmap size
#define	LIGHTMAP_WIDTH  128
#define	LIGHTMAP_HEIGHT 128

#define MAX_LIGHTMAP_SIZE (32 * 32) // it was 4096 for quite long time
#define LIGHTMAP_ARRAY_GROWTH 4

typedef struct glRect_s {
	unsigned char l, t, w, h;
} glRect_t;

typedef struct dlightinfo_s {
	int local[2];
	int rad;
	int minlight;	// rad - minlight
	int lnum; // reference to cl_dlights[]
	int color[3];
} dlightinfo_t;

static unsigned int blocklights[MAX_LIGHTMAP_SIZE * 3];

typedef struct lightmap_data_s {
	byte rawdata[4 * LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT];
	int computeData[4 * LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT];
	unsigned int sourcedata[4 * LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT];
	int allocated[LIGHTMAP_WIDTH];

	texture_ref gl_texref;
	glpoly_t* poly_chain;
	glRect_t change_area;
	qbool modified;
} lightmap_data_t;

static lightmap_data_t* lightmaps;
static unsigned int last_lightmap_updated;
static unsigned int lightmap_array_size;
static texture_ref lightmap_texture_array;
static texture_ref lightmap_data_array;
static texture_ref lightmap_source_array;
static unsigned int lightmap_depth;

// Hardware lighting: flag surfaces as we add them
static buffer_ref ssbo_surfacesTodo;
static int maximumSurfaceNumber;
static unsigned int* surfaceTodoData;
static int surfaceTodoLength;

static qbool gl_invlightmaps = true;

static dlightinfo_t dlightlist[MAX_DLIGHTS];
static int numdlights;

// funny, but this colors differ from bubblecolor[NUM_DLIGHTTYPES][4]
static int dlightcolor[NUM_DLIGHTTYPES][3] = {
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

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
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

		local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3] - surf->texturemins[0];
		local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3] - surf->texturemins[1];

		// check if this dlight will touch the surface
		if (local[1] > 0) {
			tdmin = local[1] - (tmax << 4);
			if (tdmin < 0) {
				tdmin = 0;
			}
		}
		else {
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

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	for (i = 0, light = dlightlist; i < numdlights; i++, light++) {
		irad = light->rad;
		iminlight = light->minlight;

		for (t = 0, _td = light->local[1]; t < tmax; t++, _td -= 16) {
			td = _td < 0 ? -_td : _td;

			if (td < irad) {
				dest = blocklights + t * smax * 3;
				for (s = 0, _sd = light->local[0]; s < smax; s++, _sd -= 16, dest += 3) {
					sd = _sd < 0 ? -_sd : _sd;

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

	surf->cached_dlight = !!numdlights;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
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
				dest[2] = 255 - (r >> 16);
				dest[1] = 255 - (g >> 16);
				dest[0] = 255 - (b >> 16);
			}
			else {
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

static void R_UploadLightMap(GLenum textureUnit, int lightmapnum)
{
	const void* data_source;
	lightmap_data_t* lm = &lightmaps[lightmapnum];

	data_source = lm->rawdata + (lm->change_area.t) * LIGHTMAP_WIDTH * 4;

	lm->modified = false;
	if (GL_TextureReferenceIsValid(lightmap_texture_array)) {
		GL_TexSubImage3D(textureUnit, lightmap_texture_array, 0, 0, lm->change_area.t, lightmapnum, LIGHTMAP_WIDTH, lm->change_area.h, 1, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data_source);
	}
	else {
		GL_TexSubImage2D(textureUnit, lm->gl_texref, 0, 0, lm->change_area.t, LIGHTMAP_WIDTH, lm->change_area.h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data_source);
	}
	lm->change_area.l = LIGHTMAP_WIDTH;
	lm->change_area.t = LIGHTMAP_HEIGHT;
	lm->change_area.h = 0;
	lm->change_area.w = 0;
	++frameStats.lightmap_updates;
}

void R_RenderDynamicLightmaps(msurface_t *fa)
{
	byte *base;
	int maps, smax, tmax;
	glRect_t *theRect;
	qbool lightstyle_modified = false;
	lightmap_data_t* lm;

	if (r_dynamic.integer != 1 && !fa->cached_dlight) {
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

	if (r_dynamic.integer == 1) {
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
	smax = (fa->extents[0] >> 4) + 1;
	tmax = (fa->extents[1] >> 4) + 1;
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
	memset(surfaceTodoData, 0, surfaceTodoLength);
}

static void R_RenderAllDynamicLightmapsForChain(msurface_t* surface, qbool world)
{
	int k;
	msurface_t* s;
	frameStatsPolyType polyType;

	if (!surface) {
		return;
	}

	polyType = world ? polyTypeWorldModel : polyTypeBrushModel;
	for (s = surface; s; s = s->texturechain) {
		k = s->lightmaptexturenum;

		++frameStats.classic.polycount[polyType];

		if (k >= 0 && !(s->flags & (SURF_DRAWTURB | SURF_DRAWSKY))) {
			R_RenderDynamicLightmaps(s);

			if (world && s->surfacenum < maximumSurfaceNumber) {
				surfaceTodoData[s->surfacenum / 32] |= (1 << (s->surfacenum % 32));
			}

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
			R_RenderDynamicLightmaps(s);

			if (world && s->surfacenum < maximumSurfaceNumber) {
				surfaceTodoData[s->surfacenum / 32] |= (1 << (s->surfacenum % 32));
			}

			if (lightmaps[k].modified) {
				frameStats.lightmap_min_changed = min(k, frameStats.lightmap_min_changed);
				frameStats.lightmap_max_changed = max(k, frameStats.lightmap_max_changed);
			}
		}
	}
}

void R_UploadChangedLightmaps(void)
{
	GL_EnterRegion(__FUNCTION__);
	if (r_dynamic.integer == 2) {
		extern GLuint GL_TextureNameFromReference(texture_ref ref);
		static glm_program_t lightmap_program;
		static buffer_ref ssbo_lightingData;

		if (GLM_ProgramRecompileNeeded(&lightmap_program, 0)) {
			extern unsigned char glsl_lighting_compute_glsl[];
			extern unsigned int glsl_lighting_compute_glsl_len;

			if (!GLM_CompileComputeShaderProgram(&lightmap_program, glsl_lighting_compute_glsl, glsl_lighting_compute_glsl_len)) {
				return;
			}
		}

		if (!GL_BufferReferenceIsValid(ssbo_lightingData)) {
			ssbo_lightingData = GL_CreateFixedBuffer(GL_SHADER_STORAGE_BUFFER, "lightstyles", sizeof(d_lightstylevalue), d_lightstylevalue, buffertype_use_once);
		}
		else {
			GL_UpdateBuffer(ssbo_lightingData, sizeof(d_lightstylevalue), d_lightstylevalue);
		}
		GL_BindBufferRange(ssbo_lightingData, EZQ_GL_BINDINGPOINT_LIGHTSTYLES, GL_BufferOffset(ssbo_lightingData), sizeof(d_lightstylevalue));

		GL_UpdateBuffer(ssbo_surfacesTodo, surfaceTodoLength, surfaceTodoData);
		GL_BindBufferRange(ssbo_surfacesTodo, EZQ_GL_BINDINGPOINT_SURFACES_TO_LIGHT, GL_BufferOffset(ssbo_surfacesTodo), surfaceTodoLength);

		GL_BindImageTexture(0, lightmap_source_array, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32UI);
		GL_BindImageTexture(1, lightmap_texture_array, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);
		GL_BindImageTexture(2, lightmap_data_array, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32I);

		GL_UseProgram(lightmap_program.program);
		glDispatchCompute(LIGHTMAP_WIDTH / HW_LIGHTING_BLOCK_SIZE, LIGHTMAP_HEIGHT / HW_LIGHTING_BLOCK_SIZE, lightmap_array_size);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	}
	else if (frameStats.lightmap_min_changed < lightmap_array_size) {
		unsigned int i;

		for (i = frameStats.lightmap_min_changed; i <= frameStats.lightmap_max_changed; ++i) {
			if (lightmaps[i].modified) {
				R_UploadLightMap(GL_TEXTURE0, i);
			}
		}

		frameStats.lightmap_min_changed = lightmap_array_size;
		frameStats.lightmap_max_changed = 0;
	}
	GL_LeaveRegion();
}

void R_RenderAllDynamicLightmaps(model_t *model)
{
	unsigned int i;

	for (i = 0; i < model->numtextures; i++) {
		if (!model->textures[i]) {
			continue;
		}

		R_RenderAllDynamicLightmapsForChain(model->textures[i]->texturechain[0], model->isworldmodel);
		R_RenderAllDynamicLightmapsForChain(model->textures[i]->texturechain[1], model->isworldmodel);
	}

	R_RenderAllDynamicLightmapsForChain(model->drawflat_chain[0], model->isworldmodel);
	R_RenderAllDynamicLightmapsForChain(model->drawflat_chain[1], model->isworldmodel);

	if (!GL_ShadersSupported()) {
		R_UploadChangedLightmaps();
	}
}

// returns a lightmap number and the position inside it
static int LightmapAllocBlock(int w, int h, int *x, int *y)
{
	int i, j, best, best2;
	int texnum;

	if (w < 1 || w > LIGHTMAP_WIDTH || h < 1 || h > LIGHTMAP_HEIGHT) {
		Sys_Error("AllocBlock: Bad dimensions");
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
		GLuint new_size = lightmap_array_size + LIGHTMAP_ARRAY_GROWTH;
		lightmaps = Q_realloc(lightmaps, sizeof(lightmaps[0]) * new_size);
		if (!lightmaps) {
			Sys_Error("AllocBlock: full");
		}
		memset(lightmaps + lightmap_array_size, 0, sizeof(lightmaps[0]) * LIGHTMAP_ARRAY_GROWTH);
		lightmap_array_size = new_size;
	}
	return LightmapAllocBlock(w, h, x, y);
}

void BuildSurfaceDisplayList(model_t* currentmodel, msurface_t *fa)
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
			vec = currentmodel->vertexes[r_pedge->v[0]].position;
		}
		else {
			r_pedge = &pedges[-lindex];
			vec = currentmodel->vertexes[r_pedge->v[1]].position;
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

static void R_BuildLightmapData(msurface_t* surf, int surfnum)
{
	lightmap_data_t* lm = &lightmaps[surf->lightmaptexturenum];
	qbool fullbright = (R_FullBrightAllowed() || !cl.worldmodel || !cl.worldmodel->lightdata);
	byte* lightmap = surf->samples;
	unsigned int smax = (surf->extents[0] >> 4) + 1;
	unsigned int tmax = (surf->extents[1] >> 4) + 1;
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
			data[1] = (s * 16 + surf->texturemins[0] - surf->texinfo->vecs[0][3]);
			data[2] = (t * 16 + surf->texturemins[1] - surf->texinfo->vecs[1][3]);
			data[3] = 0;

			source[0] = source[1] = source[2] = (fullbright ? 0xFFFFFFFF : 0);
			source[3] = lightmap_flags;

			if (lightmap && !fullbright) {
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

static void GL_CreateSurfaceLightmap(msurface_t *surf, int surfnum)
{
	int smax, tmax;
	byte *base;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	if (smax > LIGHTMAP_WIDTH) {
		Host_Error("GL_CreateSurfaceLightmap: smax = %d > BLOCK_WIDTH", smax);
	}
	if (tmax > LIGHTMAP_HEIGHT) {
		Host_Error("GL_CreateSurfaceLightmap: tmax = %d > BLOCK_HEIGHT", tmax);
	}
	if (smax * tmax > MAX_LIGHTMAP_SIZE) {
		Host_Error("GL_CreateSurfaceLightmap: smax * tmax = %d > MAX_LIGHTMAP_SIZE", smax * tmax);
	}

	surf->lightmaptexturenum = LightmapAllocBlock(smax, tmax, &surf->light_s, &surf->light_t);

	base = lightmaps[surf->lightmaptexturenum].rawdata + (surf->light_t * LIGHTMAP_WIDTH + surf->light_s) * 4;
	numdlights = 0;
	R_BuildLightmapData(surf, surfnum);
	R_BuildLightMap(surf, base, LIGHTMAP_WIDTH * 4);
}

//Builds the lightmap texture with all the surfaces from all brush models
//Only called when map is initially loaded
void GL_BuildLightmaps(void)
{
	int i, j;
	model_t	*m;

	if (lightmaps) {
		for (i = 0; i < lightmap_array_size; ++i) {
			memset(lightmaps[i].allocated, 0, sizeof(lightmaps[i].allocated));
		}
	}
	else {
		lightmaps = Q_malloc(sizeof(*lightmaps) * LIGHTMAP_ARRAY_GROWTH);
		lightmap_array_size = LIGHTMAP_ARRAY_GROWTH;
	}
	last_lightmap_updated = 0;

	gl_invlightmaps = !(GL_ShadersSupported() || COM_CheckParm("-noinvlmaps"));

	r_framecount = 1;		// no dlightcache
	maximumSurfaceNumber = 0;
	for (j = 1; j < MAX_MODELS; j++) {
		if (!(m = cl.model_precache[j])) {
			break;
		}
		if (m->name[0] == '*') {
			continue;
		}
		for (i = 0; i < m->numsurfaces; i++) {
			m->surfaces[i].surfacenum = i;
			if (m->surfaces[i].flags & (SURF_DRAWTURB | SURF_DRAWSKY)) {
				continue;
			}
			if (m->surfaces[i].texinfo->flags & TEX_SPECIAL) {
				continue;
			}

			GL_CreateSurfaceLightmap(m->surfaces + i, m->isworldmodel ? i : -1);
			BuildSurfaceDisplayList(m, m->surfaces + i);
		}

		if (m->isworldmodel) {
			maximumSurfaceNumber = max(maximumSurfaceNumber, m->numsurfaces);
		}
	}

	// Round up to nearest 32
	surfaceTodoLength = ((maximumSurfaceNumber + 31) / 32) * sizeof(unsigned int);
	Q_free(surfaceTodoData);
	surfaceTodoData = (unsigned int*)Q_malloc(surfaceTodoLength);

	if (!GL_BufferReferenceIsValid(ssbo_surfacesTodo)) {
		ssbo_surfacesTodo = GL_CreateFixedBuffer(GL_SHADER_STORAGE_BUFFER, "surfaces-to-light", surfaceTodoLength, NULL, buffertype_use_once);
	}
	else {
		GL_EnsureBufferSize(ssbo_surfacesTodo, surfaceTodoLength);
	}

	// upload all lightmaps that were filled
	if (GL_ShadersSupported()) {
		GLM_CreateLightmapTextures();
	}
	else {
		GLC_CreateLightmapTextures();
	}

	for (i = 0; i < lightmap_array_size; i++) {
		if (!lightmaps[i].allocated[0]) {
			break;		// not used anymore
		}
		lightmaps[i].modified = false;
		lightmaps[i].change_area.l = LIGHTMAP_WIDTH;
		lightmaps[i].change_area.t = LIGHTMAP_HEIGHT;
		lightmaps[i].change_area.w = 0;
		lightmaps[i].change_area.h = 0;
		if (GL_ShadersSupported()) {
			GL_TexSubImage3D(
				GL_TEXTURE0, lightmap_texture_array, 0, 0, 0, i,
				LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, 1, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
				lightmaps[i].rawdata
			);

			GL_TexSubImage3D(
				GL_TEXTURE0, lightmap_source_array, 0, 0, 0, i,
				LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, 1, GL_RGBA_INTEGER, GL_UNSIGNED_INT,
				lightmaps[i].sourcedata
			);

			GL_TexSubImage3D(
				GL_TEXTURE0, lightmap_data_array, 0, 0, 0, i,
				LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, 1, GL_RGBA_INTEGER, GL_INT,
				lightmaps[i].computeData
			);
		}
		else {
			GL_TexSubImage2D(
				GL_TEXTURE0, lightmaps[i].gl_texref, 0, 0, 0,
				LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
				lightmaps[i].rawdata
			);
		}
	}
}

void GLC_SetTextureLightmap(GLenum textureUnit, int lightmap_num)
{
	//update lightmap if it has been modified by dynamic lights
	if (lightmaps[lightmap_num].modified) {
		R_UploadLightMap(textureUnit, lightmap_num);
	}
	else {
		GL_EnsureTextureUnitBound(textureUnit, lightmaps[lightmap_num].gl_texref);
	}
}

GLenum GLC_LightmapTexEnv(void)
{
	return gl_invlightmaps ? GL_BLEND : GL_MODULATE;
}

void GLC_SetLightmapTextureEnvironment(GLenum textureUnit)
{
	GL_TextureEnvModeForUnit(textureUnit, GLC_LightmapTexEnv());
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

void GLC_ClearLightmapPolys(void)
{
	int i;

	for (i = 0; i < lightmap_array_size; ++i) {
		lightmaps[i].poly_chain = NULL;
	}
}

texture_ref GLC_LightmapTexture(int index)
{
	if (index < 0 || index >= lightmap_array_size) {
		return lightmaps[0].gl_texref;
	}
	return lightmaps[index].gl_texref;
}

void GLC_CreateLightmapTextures(void)
{
	int i;

	for (i = 0; i < lightmap_array_size; ++i) {
		if (!GL_TextureReferenceIsValid(lightmaps[i].gl_texref)) {
			GL_CreateTexturesWithIdentifier(GL_TEXTURE0, GL_TEXTURE_2D, 1, &lightmaps[i].gl_texref, va("lightmap-%03d", i));
			GL_TexStorage2D(GL_TEXTURE0, lightmaps[i].gl_texref, 1, GL_RGBA8, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT);
			GL_TexParameterf(GL_TEXTURE0, lightmaps[i].gl_texref, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			GL_TexParameterf(GL_TEXTURE0, lightmaps[i].gl_texref, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			GL_TexParameteri(GL_TEXTURE0, lightmaps[i].gl_texref, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			GL_TexParameteri(GL_TEXTURE0, lightmaps[i].gl_texref, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
	}
}

void GLC_LightmapUpdate(int index)
{
	if (index >= 0 && index < lightmap_array_size) {
		if (lightmaps[index].modified) {
			R_UploadLightMap(GL_TEXTURE0, index);
		}
	}
}

void GLC_AddToLightmapChain(msurface_t* s)
{
	s->polys->chain = lightmaps[s->lightmaptexturenum].poly_chain;
	lightmaps[s->lightmaptexturenum].poly_chain = s->polys;
}

glpoly_t* GLC_LightmapChain(int i)
{
	if (i < 0 || i >= lightmap_array_size) {
		return NULL;
	}
	return lightmaps[i].poly_chain;
}

int GLC_LightmapCount(void)
{
	return lightmap_array_size;
}

GLenum GLC_LightmapDestBlendFactor(void)
{
	return gl_invlightmaps ? GL_ONE_MINUS_SRC_COLOR : GL_SRC_COLOR;
}

void GLM_CreateLightmapTextures(void)
{
	int i;

	if (GL_TextureReferenceIsValid(lightmap_texture_array) && lightmap_depth >= lightmap_array_size) {
		return;
	}

	if (GL_TextureReferenceIsValid(lightmap_texture_array)) {
		GL_DeleteTextureArray(&lightmap_texture_array);
	}

	if (GL_TextureReferenceIsValid(lightmap_data_array)) {
		GL_DeleteTextureArray(&lightmap_data_array);
	}

	if (GL_TextureReferenceIsValid(lightmap_source_array)) {
		GL_DeleteTextureArray(&lightmap_source_array);
	}

	GL_CreateTextures(GL_TEXTURE0, GL_TEXTURE_2D_ARRAY, 1, &lightmap_texture_array);
	GL_TexStorage3D(GL_TEXTURE0, lightmap_texture_array, 1, GL_RGBA8, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, lightmap_array_size);
	GL_TexParameteri(GL_TEXTURE0, lightmap_texture_array, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	GL_TexParameteri(GL_TEXTURE0, lightmap_texture_array, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	GL_TexParameteri(GL_TEXTURE0, lightmap_texture_array, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	GL_TexParameteri(GL_TEXTURE0, lightmap_texture_array, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	lightmap_depth = lightmap_array_size;
	for (i = 0; i < lightmap_array_size; ++i) {
		lightmaps[i].gl_texref = lightmap_texture_array;
	}
	if (glObjectLabel) {
		extern GLuint GL_TextureNameFromReference(texture_ref ref);

		glObjectLabel(GL_TEXTURE, GL_TextureNameFromReference(lightmap_texture_array), -1, "lightmap_texture_array");
	}

	GL_CreateTextures(GL_TEXTURE0, GL_TEXTURE_2D_ARRAY, 1, &lightmap_source_array);
	GL_TexStorage3D(GL_TEXTURE0, lightmap_source_array, 1, GL_RGBA32UI, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, lightmap_array_size);
	if (glObjectLabel) {
		extern GLuint GL_TextureNameFromReference(texture_ref ref);

		glObjectLabel(GL_TEXTURE, GL_TextureNameFromReference(lightmap_source_array), -1, "lightmap_source_array");
	}

	GL_CreateTextures(GL_TEXTURE0, GL_TEXTURE_2D_ARRAY, 1, &lightmap_data_array);
	GL_TexStorage3D(GL_TEXTURE0, lightmap_data_array, 1, GL_RGBA32I, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, lightmap_array_size);
	if (glObjectLabel) {
		extern GLuint GL_TextureNameFromReference(texture_ref ref);

		glObjectLabel(GL_TEXTURE, GL_TextureNameFromReference(lightmap_data_array), -1, "lightmap_data_array");
	}
}

texture_ref GLM_LightmapArray(void)
{
	return lightmap_texture_array;
}

void GL_InvalidateLightmapTextures(void)
{
	int i;

	GL_TextureReferenceInvalidate(lightmap_texture_array);
	GL_TextureReferenceInvalidate(lightmap_data_array);
	GL_TextureReferenceInvalidate(lightmap_source_array);
	for (i = 0; i < lightmap_array_size; ++i) {
		GL_TextureReferenceInvalidate(lightmaps[i].gl_texref);
	}

	Q_free(lightmaps);
	lightmap_array_size = 0;
	last_lightmap_updated = 0;
	lightmap_depth = 0;
}
