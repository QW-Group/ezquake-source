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

$Id: gl_model.c,v 1.41 2007-10-07 08:06:33 tonik Exp $
*/
// gl_brushmodel.c  -- model loading and caching of brush models

#include "quakedef.h"
#include <limits.h>
#include "gl_model.h"
#include "teamplay.h"
#include "rulesets.h"
#include "wad.h"
#include "crc.h"
#include "fmod.h"
#include "utils.h"
#include "glsl/constants.glsl"
#include "r_brushmodel_sky.h"
#include "r_local.h"
#include "r_texture.h"
#include "r_matrix.h"
#include "r_lighting.h"
#include "r_lightmaps.h"
#include "r_framestats.h"
#include "r_brushmodel.h"
#include "r_trace.h"
#include "r_renderer.h"
#include "r_state.h"
#include "tr_types.h"

vec3_t modelorg;

extern msurface_t* skychain;
extern msurface_t* alphachain;
char* TranslateTextureName(texture_t *tx);
qbool Mod_LoadExternalTexture(model_t* loadmodel, texture_t *tx, int mode, int brighten_flag);

model_t* Mod_FindName(const char *name);

static void SetTextureFlags(model_t* mod, msurface_t* out, int surfnum)
{
	out->texinfo->surfaces++;

	// set the drawing flags flag
	// sky, turb and alpha should be mutually exclusive
	if (Mod_IsSkyTextureName(mod, out->texinfo->texture->name)) {	// sky
		out->flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
		R_SkySurfacesBuildPolys(out);	// build gl polys
		out->texinfo->skippable = false;
		return;
	}

	if (Mod_IsTurbTextureName(mod, out->texinfo->texture->name)) {	// turbulent
		out->flags |= SURF_DRAWTURB;
		out->texinfo->skippable = false;

		// check if texture should receive no lighting.
		if (out->texinfo->flags & TEX_SPECIAL) {
			out->flags |= SURF_DRAWTILED;
		} else {
			out->texinfo->texture->isLitTurb = true;
		}
		R_TurbSurfacesSubdivide(out);	// cut up polygon for warps
		return;
	}

	if (Mod_IsAlphaTextureName(mod, out->texinfo->texture->name)) {
		out->flags |= SURF_DRAWALPHA;
		out->texinfo->skippable = false;
		out->texinfo->texture->isAlphaTested = true;
	}
}

static byte* LoadColoredLighting(char *name, char **litfilename, int *filesize)
{
	qbool system;
	byte *data;
	char *groupname, *mapname;
	extern cvar_t gl_loadlitfiles;

	if (!(gl_loadlitfiles.integer == 1 || gl_loadlitfiles.integer == 2)) {
		return NULL;
	}

	mapname = TP_MapName();
	groupname = TP_GetMapGroupName(mapname, &system);

	if (strcmp(name, va("maps/%s.bsp", mapname))) {
		return NULL;
	}

	*litfilename = va("maps/lits/%s.lit", mapname);
	data = FS_LoadGameHunkFile(*litfilename, filesize);

	if (!data) {
		*litfilename = va("maps/%s.lit", mapname);
		data = FS_LoadGameHunkFile(*litfilename, filesize);
	}

	if (!data) {
		*litfilename = va("lits/%s.lit", mapname);
		data = FS_LoadGameHunkFile(*litfilename, filesize);
	}

	if (!data && groupname && !system) {
		*litfilename = va("maps/%s.lit", groupname);
		data = FS_LoadGameHunkFile(*litfilename, filesize);
	}

	if (!data && groupname && !system) {
		*litfilename = va("lits/%s.lit", groupname);
		data = FS_LoadGameHunkFile(*litfilename, filesize);
	}

	return data;
}

// Points the surface at its light samples, whose offset counts samples, or
// bytes in Half-Life maps. The lightmap builders and R_LightFromSurface read
// one lightmap of the surface's size for each of its styles, so that much
// must lie inside the light data. Where it doesn't, as at the end of the data
// when the light tool rounded a face's extents a sample smaller than
// CalcSurfaceExtents does, the surface gets no samples and no styles, as the
// light tools write a face without light data, and is drawn unlit. Sky and
// unlit turbulent surfaces (SURF_DRAWTILED) get no lightmap and aren't
// sampled, so their offset isn't checked. The extents, lmshift and flags of
// the surface must be set.
static void SetSurfaceLighting(model_t *loadmodel, msurface_t *out, int surfnum, byte *styles, int lightofs)
{
	int i, numstyles, smax, tmax, samplesize = (int)loadmodel->lightdatasamplesize;
	long long start, length;

	for (i = 0; i < MAXLIGHTMAPS; i++) {
		out->styles[i] = styles[i];
	}
	i = LittleLong(lightofs);
	if (i == -1 || loadmodel->lightdata == NULL) {
		out->samples = NULL;
		return;
	}

	numstyles = 0;
	while (numstyles < MAXLIGHTMAPS && out->styles[numstyles] != 255) {
		numstyles++;
	}
	smax = (out->extents[0] >> out->lmshift) + 1;
	tmax = (out->extents[1] >> out->lmshift) + 1;
	start = loadmodel->bspversion == HL_BSPVERSION ? i : (long long)i * samplesize;
	length = smax * tmax > 0 ? (long long)numstyles * smax * tmax * samplesize : 0;
	if (numstyles && !(out->flags & SURF_DRAWTILED) && (start < 0 || length > loadmodel->lightdata_length - start)) {
		Com_DPrintf("[%s] surface %d has light samples outside the light data (offset %d), drawing it unlit\n", loadmodel->name, surfnum, i);
		for (i = 0; i < MAXLIGHTMAPS; i++) {
			out->styles[i] = 255;
		}
		out->samples = NULL;
		return;
	}
	out->samples = loadmodel->lightdata + start;
}

// Stores the texture coordinate x, in units of 16 texels, in *units, kept
// within MAX_TEXTURE_COORD texels of 0. Returns false if x lies further out,
// or is a NaN, which the floats of a texinfo and a vertex can give.
static qbool Mod_TextureCoordUnits(double x, int *units)
{
	if (x >= -MAX_TEXTURE_COORD / 16 && x <= MAX_TEXTURE_COORD / 16) {
		*units = (int)x;
		return true;
	}
	*units = x > 0 ? MAX_TEXTURE_COORD / 16 : -MAX_TEXTURE_COORD / 16;
	return false;
}

//Fills in s->texturemins[] and s->extents[]
static void CalcSurfaceExtents(model_t* loadmodel, msurface_t *s) {
	float mins[2], maxs[2], val;
	int i,j, e, bmins[2], bmaxs[2];
	long long extent;
	qbool within;
	mvertex_t *v;
	mtexinfo_t *tex;

	mins[0] = mins[1] = BRUSHMODEL_MAX_SURFACE_EXTENTS;
	maxs[0] = maxs[1] = BRUSHMODEL_MIN_SURFACE_EXTENTS;

	tex = s->texinfo;

	for (i = 0; i < s->numedges; i++) {
		e = loadmodel->surfedges[s->firstedge+i];
		if (e >= 0) {
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		}
		else {
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];
		}

		for (j = 0; j < 2; j++) {
			// The following calculation is sensitive to floating-point
			// precision. It needs to produce the same result that the
			// light compiler does, because R_BuildLightMap uses surf->
			// extents to know the width/height of a surface's lightmap,
			// and incorrect rounding here manifests itself as patches
			// of "corrupted" looking lightmaps.
			// Most light compilers are win32 executables, so they use
			// x87 floating point. This means the multiplies and adds
			// are done at 80-bit precision, and the result is rounded
			// down to 32-bits and stored in val.
			// Adding the casts to double seems to be good enough to fix
			// lighting glitches when Quakespasm is compiled as x86_64
			// and using SSE2 floating-point. A potential trouble spot
			// is the hallway at the beginning of mfxsp17. -- ericw
			val = (double) v->position[0] * (double) tex->vecs[j][0] +
				  (double) v->position[1] * (double) tex->vecs[j][1] +
				  (double) v->position[2] * (double) tex->vecs[j][2] +
				  (double) tex->vecs[j][3];
			if (i == 0 || val < mins[j]) {
				mins[j] = val;
			}
			if (i == 0 || val > maxs[j]) {
				maxs[j] = val;
			}
		}
	}

	for (i = 0; i < 2; i++) {
		// the lightmap builders and RecursiveLightPoint take the texture
		// minimum, and its difference from a coordinate of the surface, in
		// an int; no surface of a real map comes near the limit
		within = Mod_TextureCoordUnits(floor(mins[i] / 16), &bmins[i]);
		if (!Mod_TextureCoordUnits(ceil(maxs[i] / 16), &bmaxs[i]) || !within) {
			Com_DPrintf("[%s] surface %d has texture coordinates beyond %d texels, which misplaces its light\n", loadmodel->name, (int)(s - loadmodel->surfaces), MAX_TEXTURE_COORD);
		}

		s->texturemins[i] = bmins[i] * 16;
		// lightmaps are built from the extents of surfaces without
		// TEX_SPECIAL, so these must not wrap in the short
		extent = ((long long)bmaxs[i] - bmins[i]) * 16;
		if (!(tex->flags & TEX_SPECIAL) && (extent < 0 || extent > 512 /* 256 */)) {
			Host_Error("CalcSurfaceExtents: Bad surface extents");
		}
		s->extents[i] = (short)extent;
	}
}

// the deepest node tree CM_LoadMap allows the world
#define MAX_NODE_DEPTH 4096

// Sets the parents of node, at depth depth, and of everything below it. The
// renderer walks the tree by recursion and up from the leafs, so a node may
// be reached only once, which keeps the tree from looping or sharing a
// subtree, and no deeper than MAX_NODE_DEPTH. Leafs may be shared.
static void Mod_SetParent(model_t *loadmodel, mnode_t *node, mnode_t *parent, int depth)
{
	if (node->contents < 0) {
		node->parent = parent;
		return;
	}
	// the root is reached without a parent, the others get one
	if (node->parent || (parent && node == loadmodel->nodes)) {
		Host_Error("[%s] node %d is reached twice", loadmodel->name, (int)(node - loadmodel->nodes));
	}
	if (depth > MAX_NODE_DEPTH) {
		Host_Error("[%s] node tree is deeper than %d", loadmodel->name, MAX_NODE_DEPTH);
	}
	node->parent = parent;
	Mod_SetParent(loadmodel, node->children[0], node, depth + 1);
	Mod_SetParent(loadmodel, node->children[1], node, depth + 1);
}

// Checks the tree of a submodel, which R_MarkLights walks by recursion, from
// node, at depth depth, down, as Mod_SetParent checks the tree from node 0:
// no node may be reached twice, and none deeper than MAX_NODE_DEPTH. A node
// of the tree from node 0, or of the tree of an earlier submodel, which
// reached marks, has been reached already; if it is the head node, the
// submodel's tree is part of a checked tree.
static void Mod_CheckSubmodelTree(model_t *loadmodel, byte *reached, mnode_t *node, int depth)
{
	int nodenum;

	if (node->contents < 0) {
		return;
	}
	nodenum = node - loadmodel->nodes;
	if (node->parent || node == loadmodel->nodes || reached[nodenum]) {
		if (depth == 1) {
			return;
		}
		Host_Error("[%s] node %d is reached twice", loadmodel->name, nodenum);
	}
	if (depth > MAX_NODE_DEPTH) {
		Host_Error("[%s] node tree is deeper than %d", loadmodel->name, MAX_NODE_DEPTH);
	}
	reached[nodenum] = 1;
	Mod_CheckSubmodelTree(loadmodel, reached, node->children[0], depth + 1);
	Mod_CheckSubmodelTree(loadmodel, reached, node->children[1], depth + 1);
}

// Sets up the light data of a model: a Half-Life map's own, or the BSPX
// LIGHTING_E5BGR9 or RGBLIGHTING lump, which a .lit file replaces, or the
// mono lighting in RGB. MOD_HDRLIGHTING tells the readers of the samples
// that they are E5BGR9, so it is set only while those are in use.
static void Mod_LoadLighting(model_t* loadmodel, lump_t* l, byte* mod_base, bspx_header_t* bspx_header)
{
	int i, lit_ver, mark;
	byte *in, *out, *data;
	char *litfilename;
	int filesize;
	extern cvar_t gl_loadlitfiles;
	qbool load_inline;


	loadmodel->lightdata = NULL;
	loadmodel->lightdata_length = 0;
	loadmodel->flags &= ~MOD_HDRLIGHTING;

	if (loadmodel->bspversion == HL_BSPVERSION && l->filelen > 0) {
		loadmodel->lightdata = (byte *) Hunk_AllocName(l->filelen, loadmodel->name);
		loadmodel->lightdata_length = l->filelen;
		loadmodel->lightdatasamplesize = 3;
		memcpy (loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
		return;
	}

	load_inline = gl_loadlitfiles.integer == 1 || gl_loadlitfiles.integer == 3;
	if (!load_inline && (l->filelen <= 0 && !R_FullBrightAllowed())) {
		Con_Printf("No vanilla lighting, using inline to satisfy ruleset.\n");
		load_inline = true;
	}

	if (load_inline) {
		int threshold = (lightmode == 1 ? 255 : lightmode == 2 ? 170 : 128);
		int lumpsize;
		byte *rgb;

		rgb = Mod_BSPX_FindLump(bspx_header, "LIGHTING_E5BGR9", &lumpsize, mod_base);
		if (rgb && lumpsize % 4 == 0 && (lumpsize == l->filelen * 4 || l->filelen <= 0)) {
			loadmodel->lightdata = (byte *) Hunk_AllocName (lumpsize, loadmodel->name);
			loadmodel->lightdata_length = lumpsize;
			loadmodel->lightdatasamplesize = 4;
			memcpy(loadmodel->lightdata, rgb, lumpsize);
			loadmodel->flags |= MOD_HDRLIGHTING;
			for (i = 0; i < lumpsize / 4; i++) { //native endian...
				((int*)loadmodel->lightdata)[i] = LittleLong(((int*)loadmodel->lightdata)[i]);
			}
		} else {
			rgb = Mod_BSPX_FindLump(bspx_header, "RGBLIGHTING", &lumpsize, mod_base);
			// Sanity-check size if vanilla lit exists
			if (rgb && lumpsize % 3 == 0 && (lumpsize == l->filelen * 3 || l->filelen <= 0)) {
				loadmodel->lightdata = (byte *) Hunk_AllocName(lumpsize, loadmodel->name);
				loadmodel->lightdata_length = lumpsize;
				loadmodel->lightdatasamplesize = 3;
				memcpy(loadmodel->lightdata, rgb, lumpsize);
				// we trust the inline RGB data to be bug free so we don't check it against the mono lightmap
				// what we do though is prevent color wash-out in brightly lit areas
				// (one day we may do it in R_BuildLightMap instead)
				out = loadmodel->lightdata;
				for (i = 0; i < lumpsize / 3; i++, out += 3) {
					int m = max(out[0], max(out[1], out[2]));
					if (m > threshold) {
						out[0] = out[0] * threshold / m;
						out[1] = out[1] * threshold / m;
						out[2] = out[2] * threshold / m;
					}
				}
				// all done, but we let them override it with a .lit
			}
		}
	}

	// Missing or corrupt vanilla lighting needed for .lit files, bail.
	if (l->filelen <= 0 || l->filelen >= INT_MAX / 3) {
		return;
	}

	//check for a .lit file
	mark = Hunk_LowMark();
	data = LoadColoredLighting(loadmodel->name, &litfilename, &filesize);
	if (data) {
		if (filesize < 8 || strncmp((char *)data, "QLIT", 4)) {
			Com_Printf("Corrupt .lit file (%s)...ignoring\n", COM_SkipPath(litfilename));
		} else if (l->filelen * 3 + 8 != filesize) {
			Com_Printf("Warning: .lit file (%s) has incorrect size\n", COM_SkipPath(litfilename));
		} else if ((lit_ver = LittleLong(((int *)data)[1])) != 1) {
			Com_Printf("Unknown .lit file version (v%d)\n", lit_ver);
		} else {
			extern cvar_t gl_oldlitscaling;
			if (developer.integer || cl_warncmd.integer) {
				Com_Printf("Static coloured lighting loaded\n");
			}
			loadmodel->lightdata = data + 8;
			loadmodel->lightdata_length = l->filelen * 3;
			loadmodel->lightdatasamplesize = 3;
			loadmodel->flags &= ~MOD_HDRLIGHTING;

			in = mod_base + l->fileofs;
			out = loadmodel->lightdata;
			if (gl_oldlitscaling.integer) {
				// old way (makes colored areas too dark)
				for (i = 0; i < l->filelen; i++, in++, out+=3) {
					float m, s;

					m = max(out[0], max(out[1], out[2]));
					if (!m) {
						out[0] = out[1] = out[2] = *in;
					} else {
						s = *in / m;
						out[0] = (int) (s * out[0]);
						out[1] = (int) (s * out[1]);
						out[2] = (int) (s * out[2]);
					}
				}
			}
			else {
				// new way
				float threshold = (lightmode == 1 ? 255 : lightmode == 2 ? 170 : 128);
				for (i = 0; i < l->filelen; i++, in++, out+=3) {
					float r, g, b, m, p, s;
					if (!out[0] && !out[1] && !out[2]) {
						out[0] = out[1] = out[2] = *in;
						continue;
					}

					// calculate perceived brightness of the color sample
					// kudos to Darel Rex Finley for his HSP color model
					p = sqrt(out[0]*out[0]*0.241 + out[1]*out[1]*0.691 + out[2]*out[2]*0.068);
					// scale to match perceived brightness of monochrome sample
					s = *in / p;
					r = s * out[0];
					g = s * out[1];
					b = s * out[2];
					m = max(r, max(g, b));
					if (m > threshold) {
						// scale down to avoid color washout
						r *= threshold/m;
						g *= threshold/m;
						b *= threshold/m;
					}
					out[0] = (int) (r + 0.5);
					out[1] = (int) (g + 0.5);
					out[2] = (int) (b + 0.5);
				}
			}
			return;
		}
		Hunk_FreeToLowMark (mark);
	}

	if (loadmodel->lightdata) {
		return;		// we have loaded inline RGB data
	}

	//no .lit found, expand the white lighting data to color
	loadmodel->lightdata = (byte *) Hunk_AllocName (l->filelen * 3, va("%s_@lightdata", loadmodel->name));
	loadmodel->lightdata_length = l->filelen * 3;
	loadmodel->lightdatasamplesize = 3;
	in = mod_base + l->fileofs;
	out = loadmodel->lightdata;
	for (i = 0; i < l->filelen; i++, out += 3) {
		out[0] = out[1] = out[2] = *in++;
	}
}

static qbool Mod_LoadExternalSkyTexture(texture_t *tx)
{
	char *altname, *mapname;
	char solidname[MAX_QPATH], alphaname[MAX_QPATH];
	char altsolidname[MAX_QPATH], altalphaname[MAX_QPATH];
	byte alphapixel = 255;
	int flags = TEX_MIPMAP | (!gl_scaleskytextures.integer ? TEX_NOSCALE : 0);

	if (!R_ExternalTexturesEnabled(true)) {
		return false;
	}

	altname = TranslateTextureName (tx);
	mapname = Cvar_String("mapname");
	snprintf (solidname, sizeof(solidname), "%s_solid", tx->name);
	snprintf (alphaname, sizeof(alphaname), "%s_alpha", tx->name);

	solidskytexture = R_LoadTextureImage (va("textures/%s/%s", mapname, solidname), solidname, 0, 0, flags);
	if (!R_TextureReferenceIsValid(solidskytexture) && altname) {
		snprintf(altsolidname, sizeof(altsolidname), "%s_solid", altname);
		solidskytexture = R_LoadTextureImage (va("textures/%s", altsolidname), altsolidname, 0, 0, flags);
	}
	if (!R_TextureReferenceIsValid(solidskytexture)) {
		solidskytexture = R_LoadTextureImage(va("textures/%s", solidname), solidname, 0, 0, flags);
	}
	if (!R_TextureReferenceIsValid(solidskytexture)) {
		return false;
	}

	alphaskytexture = R_LoadTextureImage (va("textures/%s/%s", mapname, alphaname), alphaname, 0, 0, TEX_ALPHA | TEX_PREMUL_ALPHA | flags);
	if (!R_TextureReferenceIsValid(alphaskytexture) && altname) {
		snprintf (altalphaname, sizeof(altalphaname), "%s_alpha", altname);
		alphaskytexture = R_LoadTextureImage (va("textures/%s", altalphaname), altalphaname, 0, 0, TEX_ALPHA | TEX_PREMUL_ALPHA | flags);
	}
	if (!R_TextureReferenceIsValid(alphaskytexture)) {
		alphaskytexture = R_LoadTextureImage(va("textures/%s", alphaname), alphaname, 0, 0, TEX_ALPHA | TEX_PREMUL_ALPHA | flags);
	}
	if (!R_TextureReferenceIsValid(alphaskytexture)) {
		// Load a texture consisting of a single transparent pixel
		alphaskytexture = R_LoadTexture(alphaname, 1, 1, &alphapixel, TEX_ALPHA | TEX_PREMUL_ALPHA | flags, 1);
	}
	return true;
}

/* Allocate hunk-owned rows once per BSP load; inline models share them. */
static void Mod_AllocVisBuffers(model_t *model)
{
	size_t rowbytes = (((size_t)model->numleafs + 31) >> 5) * 4;

	if (rowbytes > INT_MAX)
		Host_Error("Mod_AllocVisBuffers: visibility row exceeds allocator limit");
	model->vis_rowbytes = (int)rowbytes;
	model->vis_novis = (byte *)Hunk_AllocName((int)rowbytes, "r_novis");
	model->vis_decompressed = (byte *)Hunk_AllocName((int)rowbytes, "r_visdec");
	model->vis_merged = (byte *)Hunk_AllocName((int)rowbytes, "r_vismerge");
	memset(model->vis_novis, 0xff, rowbytes);
}

static void Mod_LoadVisibility(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	if (l->filelen <= 0) {
		loadmodel->visdata = NULL;
		return;
	}
	loadmodel->visdata = (byte*)Hunk_AllocName(l->filelen, loadmodel->name);
	loadmodel->visdata_length = l->filelen;
	memcpy(loadmodel->visdata, mod_base + l->fileofs, l->filelen);
}

static void Mod_LoadSubmodels(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	dmodel_t *in, *out;
	int i, j, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Host_Error("Mod_LoadSubmodels: funny lump size in %s", loadmodel->name);
		return;
	}
	count = l->filelen / sizeof(*in);
	if (count > MAX_MODELS) {
		Host_Error("Mod_LoadSubmodels : count > MAX_MODELS");
		return;
	}
	if (count > INT_MAX / sizeof(*out)) {
		Host_Error("Mod_LoadSubmodels : count > %d", INT_MAX);
		return;
	}
	out = (dmodel_t *) Hunk_AllocName ( count*sizeof(*out), loadmodel->name);

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for (i = 0; i < count; i++, in++, out++)	{
		for (j = 0; j < 3; j++) {	// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
			out->origin[j] = LittleFloat (in->origin[j]);
		}
		for (j = 0; j < MAX_MAP_HULLS; j++) {
			out->headnode[j] = LittleLong(in->headnode[j]);
		}
		out->visleafs = LittleLong (in->visleafs);
		out->firstface = LittleLong (in->firstface);
		out->numfaces = LittleLong (in->numfaces);
	}
}

static void Mod_LoadTextures(model_t* mod, lump_t *l, byte* mod_base)
{
	int i, j, num, max, altmax, pixels, room;
	miptex_t *mt;
	texture_t *tx, *tx2, *anims[10], *altanims[10];
	dmiptexlump_t *m;

	if (l->filelen <= 0) {
		mod->textures = NULL;
		return;
	}

	m = (dmiptexlump_t *) (mod_base + l->fileofs);
	if (l->filelen < (int)sizeof(m->nummiptex)) {
		Host_Error("Mod_LoadTextures: texture lump of %d bytes in %s", l->filelen, mod->name);
	}
	m->nummiptex = LittleLong (m->nummiptex);
	if (m->nummiptex < 0 || m->nummiptex > INT_MAX / sizeof(*mod->textures)) {
		Host_Error("Mod_LoadTextures: nummiptex %d (0-%d)", m->nummiptex, INT_MAX / sizeof(*mod->textures));
		return;
	}
	if (m->nummiptex > (l->filelen - (int)sizeof(m->nummiptex)) / (int)sizeof(m->dataofs[0])) {
		Host_Error("Mod_LoadTextures: %d texture offsets do not fit a lump of %d bytes in %s", m->nummiptex, l->filelen, mod->name);
	}
	mod->numtextures = m->nummiptex;
	mod->textures = (texture_t **) Hunk_AllocName(m->nummiptex * sizeof(*mod->textures), mod->name);

	for (i = 0; i < m->nummiptex; i++) {
		m->dataofs[i] = LittleLong(m->dataofs[i]);
		if (m->dataofs[i] == -1) {
			continue;
		}

		// the offset counts from the start of the lump; the pixels are checked
		// below, as not all textures have them
		if (m->dataofs[i] < 0 || m->dataofs[i] > l->filelen - (int)sizeof(miptex_t)) {
			Host_Error("Miptex %d has data offset %d (-1 to %d)", i, m->dataofs[i], l->filelen - (int)sizeof(miptex_t));
		}

		mt = (miptex_t *)((byte *)m + m->dataofs[i]);
		room = l->filelen - m->dataofs[i] - (int)sizeof(miptex_t);
		mt->width  = LittleLong(mt->width);
		mt->height = LittleLong(mt->height);
		for (j = 0; j < MIPLEVELS; j++) {
			mt->offsets[j] = LittleLong(mt->offsets[j]);
		}

		// a name that fills its 16 bytes has no NUL, so print at most those
		if ((mt->width & 15) || (mt->height & 15)) {
			Host_Error("Texture %.*s is not 16 aligned", (int)sizeof(mt->name), mt->name);
		}
		if (mt->width < 0 || mt->height < 0) {
			Host_Error("Texture %.*s negative dimensions: %dx%d", (int)sizeof(mt->name), mt->name, mt->width, mt->height);
		}
		if (mt->height && mt->width > INT_MAX / mt->height / 3) {
			Host_Error("Texture %.*s excessive size: %dx%d", (int)sizeof(mt->name), mt->name, mt->width, mt->height);
		}

		// Everything that refuses the texture runs before its pixels are
		// allocated, as a size the file can't hold would exhaust the hunk
		pixels = 0;
		if (mt->width && mt->height && mt->offsets[0]) {
			pixels = mt->width * mt->height / 64 * 85; // some magic numbers?
			if (mod->bspversion == HL_BSPVERSION) {
				pixels += 2 + 256 * 3; /* palette and unknown two bytes */
				// the pixels immediately follow the structures, then the
				// palette, which some maps leave out
				if (pixels - 2 - 256 * 3 > room) {
					Host_Error("Texture %.*s: offset %d pixelsize %d, lumpsize %d", (int)sizeof(mt->name), mt->name, m->dataofs[i], pixels, l->filelen);
				}
			}
			else {
				if (pixels > room) {
					Host_Error("Texture %.*s: offset %d pixelsize %d, lumpsize %d", (int)sizeof(mt->name), mt->name, m->dataofs[i], pixels, l->filelen);
				}
				// the sky, the r_fastturb colour and an upload at full size
				// read mip level 0
				if (mt->offsets[0] < sizeof(miptex_t) || mt->offsets[0] - sizeof(miptex_t) > pixels - mt->width * mt->height) {
					Host_Error("Texture %.*s: mip level 0 at offset %u lies outside its pixels", (int)sizeof(mt->name), mt->name, mt->offsets[0]);
				}
			}
		}
		tx = (texture_t *)Hunk_AllocName(sizeof(texture_t) + pixels, mod->name);
		mod->textures[i] = tx;

		memcpy(tx->name, mt->name, sizeof(tx->name));
		if (!tx->name[0]) {
			snprintf(tx->name, sizeof(tx->name), "unnamed%d", i);
			Com_DPrintf("Warning: unnamed texture in %s, renaming to %s\n", mod->name, tx->name);
		}

		tx->width  = mt->width;
		tx->height = mt->height;
		tx->index = i;
		tx->loaded = false; // so texture will be reloaded

		if (tx->width == 0 || tx->height == 0) {
			Con_Printf("Warning: Skipping zero size texture %s in %s\n", tx->name, mod->name);
			continue;
		}

		if (mod->bspversion == HL_BSPVERSION) {
			if (!mt->offsets[0]) {
				continue;
			}

			// what would lie past the lump stays 0
			memcpy(tx + 1, mt + 1, min(pixels, room));
			for (j = 0; j < MIPLEVELS; j++) {
				tx->offsets[j] = mt->offsets[j] + sizeof(texture_t) - sizeof(miptex_t);
			}
			continue;
		}

		if (mt->offsets[0]) {
			// the pixels immediately follow the structures
			memcpy(tx+1, mt+1, pixels);

			// The renderer uploads the mip level gl_miptexLevel picks, or level
			// 0 for a texture it doesn't scale. A smaller level that lies
			// outside the pixels gets no offset, and the renderer uploads the
			// nearest larger level that has one instead.
			tx->offsets[0] = mt->offsets[0] + sizeof(texture_t) - sizeof(miptex_t);
			for (j = 1; j < MIPLEVELS; j++) {
				if (mt->offsets[j] < sizeof(miptex_t) || mt->offsets[j] - sizeof(miptex_t) > pixels - (mt->width >> j) * (mt->height >> j)) {
					Com_DPrintf("Texture %s in %s: mip level %d at offset %u lies outside its pixels\n", tx->name, mod->name, j, mt->offsets[j]);
					tx->offsets[j] = 0;
				}
				else {
					tx->offsets[j] = mt->offsets[j] + sizeof(texture_t) - sizeof(miptex_t);
				}
			}

			// HACK HACK HACK
			if (!strcmp(mt->name, "shot1sid") && mt->width==32 && mt->height==32
				&& CRC_Block((byte*)(mt+1), mt->width*mt->height) == 65393)
			{	// This texture in b_shell1.bsp has some of the first 32 pixels painted white.
				// They are invisible in software, but look really ugly in GL. So we just copy
				// 32 pixels from the bottom to make it look nice.
				memcpy (tx+1, (byte *)(tx+1) + 32*31, 32);
			}

			// just for r_fastturb's sake
			{
				byte *data = (byte *) &d_8to24table[*((byte *) mt + mt->offsets[0] + ((mt->height * mt->width) >> 1))];
				tx->flatcolor3ub = (255u << 24) + (data[0] << 0) + (data[1] << 8) + (data[2] << 16);

				if (strstr(tx->name, "water") || strstr(tx->name, "mwat")) {
					tx->turbType = TEXTURE_TURB_WATER;
				}
				else if (strstr(tx->name, "slime")) {
					tx->turbType = TEXTURE_TURB_SLIME;
				}
				else if (strstr(tx->name, "lava")) {
					tx->turbType = TEXTURE_TURB_LAVA;
				}
				else if (strstr(tx->name, "tele")) {
					tx->turbType = TEXTURE_TURB_TELE;
				}
			}
		}
	}

	R_LoadBrushModelTextures(mod);

	// sequence the animations
	for (i = 0; i < m->nummiptex; i++) {
		tx = mod->textures[i];
		if (!tx || tx->name[0] != '+') {
			continue;
		}
		if (tx->anim_next) {
			continue; // already sequenced
		}

		// find the number of frames in the animation
		memset (anims, 0, sizeof(anims));
		memset (altanims, 0, sizeof(altanims));

		max = tx->name[1];
		altmax = 0;
		if (max >= 'a' && max <= 'z') {
			max -= 'a' - 'A';
		}
		if (max >= '0' && max <= '9') {
			max -= '0';
			altmax = 0;
			anims[max] = tx;
			max++;
		}
		else if (max >= 'A' && max <= 'J') {
			altmax = max - 'A';
			max = 0;
			altanims[altmax] = tx;
			altmax++;
		}
		else {
			Host_Error ("Mod_LoadTextures: Bad animating texture %s", tx->name);
		}

		for (j = i + 1; j < m->nummiptex; j++) {
			tx2 = mod->textures[j];
			if (!tx2 || tx2->name[0] != '+') {
				continue;
			}
			if (strcmp(tx2->name + 2, tx->name + 2)) {
				continue;
			}

			num = tx2->name[1];
			if (num >= 'a' && num <= 'z') {
				num -= 'a' - 'A';
			}
			if (num >= '0' && num <= '9') {
				num -= '0';
				anims[num] = tx2;
				if (num + 1 > max) {
					max = num + 1;
				}
			}
			else if (num >= 'A' && num <= 'J') {
				num = num - 'A';
				altanims[num] = tx2;
				if (num + 1 > altmax) {
					altmax = num + 1;
				}
			}
			else {
				Host_Error ("Mod_LoadTextures: Bad animating texture %s", tx->name);
			}
		}

#define	ANIM_CYCLE	2
		// link them all together
		for (j = 0; j < max && j < sizeof(anims) / sizeof(anims[0]); j++) {
			tx2 = anims[j];
			if (!tx2) {
				Host_Error("Mod_LoadTextures: Missing frame %i of %s", j, tx->name);
				return;
			}
			tx2->anim_total = max * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j + 1) * ANIM_CYCLE;
			tx2->anim_next = anims[(j + 1) % max];
			if (altmax) {
				tx2->alternate_anims = altanims[0];
			}
		}
		for (j = 0; j < altmax && j < sizeof(altanims) / sizeof(altanims[0]); j++) {
			tx2 = altanims[j];
			if (!tx2) {
				Host_Error("Mod_LoadTextures: Missing frame %i of %s", j, tx->name);
				return;
			}
			tx2->anim_total = altmax * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j + 1) * ANIM_CYCLE;
			tx2->anim_next = altanims[ (j + 1) % altmax ];
			if (max) {
				tx2->alternate_anims = anims[0];
			}
		}
	}
}

static void Mod_LoadVertexes(model_t* mod, lump_t *l, byte* mod_base)
{
	dvertex_t *in;
	mvertex_t *out;
	int i, count;
	int max_vertices = (INT_MAX / sizeof(*out));

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen <= 0) {
		Host_Error("Mod_LoadVertexes: lump size <= 0 in %s", mod->name);
		return;
	}
	if (l->filelen % sizeof(*in)) {
		Host_Error("Mod_LoadVertexes: funny lump size in %s", mod->name);
		return;
	}
	count = l->filelen / sizeof(*in);
	if (count > max_vertices) {
		Host_Error("Mod_LoadVertexes: invalid vertex count (%d vs 0-%d)", count, max_vertices);
		return;
	}
	out = (mvertex_t *)Hunk_AllocName(count * sizeof(*out), mod->name);

	mod->vertexes = out;
	mod->numvertexes = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->position[0] = LittleFloat(in->point[0]);
		out->position[1] = LittleFloat(in->point[1]);
		out->position[2] = LittleFloat(in->point[2]);
	}
}

// Refuses an edge whose vertices lie outside the vertex lump. A surface edge
// can't use edge 0, as it can't be negated, so some compilers leave it
// uninitialised: it isn't checked, and Mod_CheckFaceIndexes refuses its use.
static void Mod_CheckEdge(model_t *loadmodel, int edgenum, const medge_t *edge)
{
	unsigned int numvertexes = (unsigned int)loadmodel->numvertexes;

	if (edgenum > 0 && (edge->v[0] >= numvertexes || edge->v[1] >= numvertexes)) {
		Host_Error("[%s] edge %d references invalid vertexes %u and %u/%d", loadmodel->name, edgenum, edge->v[0], edge->v[1], loadmodel->numvertexes);
	}
}

static void Mod_LoadEdges(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	dedge_t *in;
	medge_t *out;
	int i, count;
	int max_edges = (INT_MAX / sizeof(*out));

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Host_Error("Mod_LoadEdges: funny lump size in %s", loadmodel->name);
		return;
	}
	count = l->filelen / sizeof(*in);
	if (count < 0 || count + 1 >= max_edges) {
		Host_Error("Mod_LoadEdges: invalid edge count (%d vs 0-%d)", count, max_edges);
		return;
	}
	out = (medge_t *) Hunk_AllocName ( (count + 1) * sizeof(*out), loadmodel->name);

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->v[0] = (unsigned short) LittleShort(in->v[0]);
		out->v[1] = (unsigned short) LittleShort(in->v[1]);
		Mod_CheckEdge(loadmodel, i, out);
	}
}

static void Mod_LoadEdgesBSP2(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	dedge29a_t *in;
	medge_t *out;
	int i, count;
	int max_edges = (INT_MAX / sizeof(*out));

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadEdges: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	if (count < 0 || count + 1 >= max_edges) {
		Host_Error("Mod_LoadEdges: invalid edge count (%d vs 0-%d)", count, max_edges);
		return;
	}
	out = (medge_t *) Hunk_AllocName ( (count + 1) * sizeof(*out), loadmodel->name);

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->v[0] = LittleLong(in->v[0]);
		out->v[1] = LittleLong(in->v[1]);
		Mod_CheckEdge(loadmodel, i, out);
	}
}

static void Mod_LoadSurfedges(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	int i, count, *in, *out;
	int max_edges = (INT_MAX / sizeof(*out));

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadSurfedges: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	if (count < 0 || count + 1 >= max_edges) {
		Host_Error("Mod_LoadSurfEdges: invalid edge count (%d vs 0-%d)", count, max_edges);
		return;
	}
	out = (int *) Hunk_AllocName ( count*sizeof(*out), loadmodel->name);

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	// Mod_CheckFaceIndexes checks the ones a face uses
	for (i = 0; i < count; i++) {
		out[i] = LittleLong(in[i]);
	}
}

static void Mod_LoadPlanes(model_t* model, lump_t* l, byte* mod_base)
{
	int i, j, count, bits;
	mplane_t *out;
	dplane_t *in;
	int max_planes = (INT_MAX / sizeof(*out));

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Host_Error("Mod_LoadPlanes: funny lump size in %s", model->name);
	}
	count = l->filelen / sizeof(*in);
	if (count < 0 || count >= max_planes) {
		Host_Error("Mod_LoadPlanes: invalid plane count (%d vs 0-%d)", count, max_planes);
		return;
	}
	out = (mplane_t *) Hunk_AllocName ( count*sizeof(*out), model->name);

	model->planes = out;
	model->numplanes = count;

	for (i = 0; i < count; i++, in++, out++) {
		bits = 0;
		for (j = 0; j < 3 ; j++) {
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0) {
				bits |= 1 << j;
			}
		}

		out->dist = LittleFloat (in->dist);
		out->type = LittleLong (in->type);
		out->signbits = bits;
	}
}

static void Mod_LoadMarksurfacesBSP2(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	int i, count;
	unsigned int j;
	int *in;
	msurface_t **out;
	int max_surfaces = (INT_MAX / sizeof(*out));

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Host_Error("Mod_LoadMarksurfaces: funny lump size in %s", loadmodel->name);
	}
	count = l->filelen / sizeof(*in);
	if (count < 0 || count >= max_surfaces) {
		Host_Error("Mod_LoadMarksurfaces: invalid marksurface count (%d vs 0-%d)", count, max_surfaces);
		return;
	}
	out = (msurface_t **)Hunk_AllocName(count * sizeof(*out), loadmodel->name);

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for (i = 0; i < count; i++) {
		// Surface indices are unsigned
		j = (unsigned int) LittleLong(in[i]);
		if (j >= loadmodel->numsurfaces) {
			Host_Error("Mod_LoadMarksurfaces: bad surface number");
		}
		out[i] = loadmodel->surfaces + j;
	}
}

static void Mod_LoadMarksurfaces(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	int i, count;
	unsigned short j;
	short *in;
	msurface_t **out;
	int max_surfaces = (INT_MAX / sizeof(*out));

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Host_Error("Mod_LoadMarksurfaces: funny lump size in %s", loadmodel->name);
	}
	count = l->filelen / sizeof(*in);
	if (count < 0 || count >= max_surfaces) {
		Host_Error("Mod_LoadMarksurfaces: invalid marksurface count (%d vs 0-%d)", count, max_surfaces);
		return;
	}
	out = (msurface_t **) Hunk_AllocName ( count*sizeof(*out), loadmodel->name);

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for (i = 0; i < count; i++) {
		// Surface indices are unsigned
		j = (unsigned short) LittleShort(in[i]);
		if (j >= loadmodel->numsurfaces) {
			Host_Error("Mod_LoadMarksurfaces: bad surface number");
		}
		out[i] = loadmodel->surfaces + j;
	}
}

static void Mod_ParseWadsFromEntityLump(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	const char *data;
	char *s, key[1024], value[1024];
	int i, j, k;

	if (l->filelen <= 0) {
		return;
	}

	data = (char *)(mod_base + l->fileofs);
	data = COM_Parse(data);
	if (!data) {
		return;
	}

	if (com_token[0] != '{') {
		return; // error
	}

	while (1) {
		if (!(data = COM_Parse(data))) {
			return; // error
		}

		if (com_token[0] == '}') {
			break; // end of worldspawn
		}

		strlcpy(key, (com_token[0] == '_') ? com_token + 1 : com_token, sizeof(key));
		for (s = key + strlen(key) - 1; s >= key && *s == ' '; s--) {
			// remove trailing spaces
			*s = 0;
		}

		if (!(data = COM_Parse(data))) {
			return; // error
		}

		strlcpy(value, com_token, sizeof(value));
		if (!strcmp("sky", key) || !strcmp("skyname", key)) {
			Cvar_Set(&r_skyname, value);
		}

		if (!strcmp("wad", key)) {
			j = 0;
			for (i = 0; i < strlen(value); i++) {
				if (value[i] != ';' && value[i] != '\\' && value[i] != '/' && value[i] != ':') {
					break;
				}
			}
			if (!value[i]) {
				continue;
			}
			for ( ; i < sizeof(value); i++) {
				// ignore path - the \\ check is for HalfLife
				if (value[i] == '\\' || value[i] == '/' || value[i] == ':') {
					j = i + 1;
				}
				else if (value[i] == ';' || value[i] == 0) {
					k = value[i];
					value[i] = 0;
					if (value[j]) {
						WAD3_LoadWadFile(value + j);
					}
					j = i + 1;
					if (!k) {
						break;
					}
				}
			}
		}
	}
}

static void Mod_LoadTexinfo(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	texinfo_t *in;
	mtexinfo_t *out;
	int i, j, k, count;
	int max_texinfo = (INT_MAX / sizeof(*out));

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Host_Error("Mod_LoadTexinfo: funny lump size in %s", loadmodel->name);
	}
	count = l->filelen / sizeof(*in);
	if (count > max_texinfo) {
		Host_Error("Mod_LoadTexinfo: invalid texinfo count (%d vs 0-%d)", count, max_texinfo);
		return;
	}
	out = (mtexinfo_t *) Hunk_AllocName (count*sizeof(*out), loadmodel->name);

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 2; j++) {
			for (k = 0; k < 4; k++) {
				out->vecs[j][k] = LittleFloat(in->vecs[j][k]);
			}
		}

		out->miptex = LittleLong (in->miptex);
		out->flags = LittleLong (in->flags);

		// Skip texture unless surface flags say otherwise
		out->skippable = out->flags & TEX_SPECIAL;

		if (!loadmodel->textures) {
			out->texture = r_notexture_mip;	// checkerboard texture
			out->flags = 0;
		}
		else {
			if (out->miptex >= loadmodel->numtextures) {
				Host_Error("Mod_LoadTexinfo: %d miptex %d >= loadmodel->numtextures", i, out->miptex);
			}
			if (out->miptex < 0) {
				Host_Error("Mod_LoadTexinfo: %d miptex %d < 0", i, out->miptex);
			}
			out->texture = loadmodel->textures[out->miptex];
			if (!out->texture) {
				out->texture = r_notexture_mip; // texture not found
				out->flags = 0;
			}
		}
	}
}

// Extension to support lightmaps that aren't tied to texture scale.
static int Mod_LoadDecoupledLM(model_t *loadmodel, dlminfo_t *lminfos, int surfnum, msurface_t *out)
{
	dlminfo_t *lminfo;
	unsigned short lmwidth, lmheight;
	int i, j;

	if (lminfos == NULL) {
		return -1;
	}

	lminfo = lminfos + surfnum;

	lmwidth = LittleShort(lminfo->lmwidth);
	lmheight = LittleShort(lminfo->lmheight);

	if (lmwidth <= 0 || lmheight <= 0) {
		return -1;
	}
	// lightmaps are built from the extents of surfaces without TEX_SPECIAL,
	// which these are unless the face falls back on its own lighting, so
	// they must not wrap in the short
	if (LittleLong(lminfo->lightofs) >= 0 && !(out->texinfo->flags & TEX_SPECIAL) && (lmwidth > 32768 || lmheight > 32768)) {
		Host_Error("[%s] surface %d has a decoupled lightmap of %dx%d samples", loadmodel->name, surfnum, lmwidth, lmheight);
	}

	for (i = 0; i < 2; i++) {
		for (j = 0; j < 4; j++) {
			out->lmvecs[i][j] = LittleFloat(lminfo->vecs[i][j]);
		}
	}

	out->extents[0] = (short)(lmwidth - 1);
	out->extents[1] = (short)(lmheight - 1);
	out->lmshift = 0;
	out->texturemins[0] = 0;
	out->texturemins[1] = 0;

	float v0 = VectorLength(out->lmvecs[0]);
	out->lmvlen[0] = v0 > 0.0f ? 1.0f / v0 : 0.0f;

	float v1 = VectorLength(out->lmvecs[1]);
	out->lmvlen[1] = v1 > 0.0f ? 1.0f / v1 : 0.0f;

	return LittleLong(lminfo->lightofs);
}

// Refuses a face whose edges, plane or texinfo lie outside their lumps, as
// everything after the face loaders indexes them without checking.
static void Mod_CheckFaceIndexes(model_t *loadmodel, int surfnum, int firstedge, int numedges, int planenum, int texinfo)
{
	int i, edge;

	if (firstedge < 0 || firstedge >= loadmodel->numsurfedges || numedges < 0 || numedges > loadmodel->numsurfedges - firstedge) {
		Host_Error("[%s] surface %d references invalid edge range [%d length %d, vs %d]", loadmodel->name, surfnum, firstedge, numedges, loadmodel->numsurfedges);
	}
	// Surface edges are only read through the range of a face, so one that no
	// face uses is not checked.
	for (i = 0; i < numedges; i++) {
		edge = loadmodel->surfedges[firstedge + i];
		// an edge used backwards is negated, so edge 0 can't be used
		if (edge == 0 || edge <= -loadmodel->numedges || edge >= loadmodel->numedges) {
			Host_Error("[%s] surface edge %d references invalid edge %d/%d", loadmodel->name, firstedge + i, edge, loadmodel->numedges);
		}
	}
	if (planenum < 0 || planenum >= loadmodel->numplanes) {
		Host_Error("[%s] surface %d references invalid plane %d/%d", loadmodel->name, surfnum, planenum, loadmodel->numplanes);
	}
	if (texinfo < 0 || texinfo >= loadmodel->numtexinfo) {
		Host_Error("[%s] surface %d references invalid texinfo %d/%d", loadmodel->name, surfnum, texinfo, loadmodel->numtexinfo);
	}
}

static void Mod_LoadFaces(model_t* loadmodel, lump_t* l, byte* mod_base, bspx_header_t *bspx_header)
{
	dlminfo_t *lminfos;
	dface_t *in;
	msurface_t *out;
	int count, surfnum, planenum, side, lminfosize, lightofs;
	int max_faces = INT_MAX / sizeof(*out);
	int texinfo;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadFaces: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	if (count <= 0 || count > max_faces) {
		Host_Error("Mod_LoadFaces: invalid surface count (%d vs 0-%d)", count, max_faces);
		return;
	}
	out = (msurface_t *) Hunk_AllocName ( count*sizeof(*out), loadmodel->name);

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	lminfos = Mod_BSPX_FindLump(bspx_header, "DECOUPLED_LM", &lminfosize, mod_base);
	if (lminfos != NULL && lminfosize / sizeof(dlminfo_t) != loadmodel->numsurfaces) {
		Com_Printf("[%s] decoupled_lm size %d does not match surface count %d\n", loadmodel->name, lminfosize / sizeof(dlminfo_t), loadmodel->numsurfaces);
		lminfos = NULL;
	}

	for (surfnum = 0; surfnum < count; surfnum++, in++, out++) {
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);
		out->flags = 0;

		texinfo = LittleShort(in->texinfo);
		planenum = LittleShort(in->planenum);
		Mod_CheckFaceIndexes(loadmodel, surfnum, out->firstedge, out->numedges, planenum, texinfo);

		side = LittleShort(in->side);
		if (side) {
			out->flags |= SURF_PLANEBACK;
		}

		out->plane = loadmodel->planes + planenum;
		out->texinfo = loadmodel->texinfo + texinfo;

		lightofs = Mod_LoadDecoupledLM(loadmodel, lminfos, surfnum, out);
		if (lightofs < 0) {
			memcpy(out->lmvecs, out->texinfo->vecs, sizeof(out->lmvecs));
			out->lmshift = DEFAULT_LMSHIFT;
			out->lmvlen[0] = 1.0f;
			out->lmvlen[1] = 1.0f;

			CalcSurfaceExtents(loadmodel, out);

			lightofs = in->lightofs;
		}

		SetTextureFlags(loadmodel, out, surfnum);

		// lighting info
		SetSurfaceLighting(loadmodel, out, surfnum, in->styles, lightofs);
	}
}

static void Mod_LoadFacesBSP2(model_t* loadmodel, lump_t* l, byte* mod_base, bspx_header_t *bspx_header)
{
	dlminfo_t *lminfos;
	dface29a_t *in;
	msurface_t *out;
	int count, surfnum, planenum, side, lminfosize, lightofs, texinfo;
	int max_faces = INT_MAX / sizeof(*out);

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Host_Error("Mod_LoadFaces: funny lump size in %s", loadmodel->name);
	}
	count = l->filelen / sizeof(*in);
	if (count <= 0 || count > max_faces) {
		Host_Error("Mod_LoadFaces: invalid surface count (%d vs 0-%d)", count, max_faces);
		return;
	}
	out = (msurface_t *) Hunk_AllocName ( count*sizeof(*out), loadmodel->name);

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	lminfos = Mod_BSPX_FindLump(bspx_header, "DECOUPLED_LM", &lminfosize, mod_base);
	if (lminfos != NULL && lminfosize / sizeof(dlminfo_t) != loadmodel->numsurfaces) {
		Com_Printf("[%s] decoupled_lm size %d does not match surface count %d\n", loadmodel->name, lminfosize / sizeof(dlminfo_t), loadmodel->numsurfaces);
		lminfos = NULL;
	}

	for (surfnum = 0; surfnum < count; surfnum++, in++, out++) {
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleLong(in->numedges);
		out->flags = 0;

		planenum = LittleLong(in->planenum);
		texinfo = LittleLong(in->texinfo);
		Mod_CheckFaceIndexes(loadmodel, surfnum, out->firstedge, out->numedges, planenum, texinfo);

		side = LittleLong(in->side);
		if (side) {
			out->flags |= SURF_PLANEBACK;
		}

		out->plane = loadmodel->planes + planenum;
		out->texinfo = loadmodel->texinfo + texinfo;

		lightofs = Mod_LoadDecoupledLM(loadmodel, lminfos, surfnum, out);
		if (lightofs < 0) {
			memcpy(out->lmvecs, out->texinfo->vecs, sizeof(out->lmvecs));
			out->lmshift = DEFAULT_LMSHIFT;
			out->lmvlen[0] = 1.0f;
			out->lmvlen[1] = 1.0f;

			CalcSurfaceExtents(loadmodel, out);

			lightofs = in->lightofs;
		}

		SetTextureFlags(loadmodel, out, surfnum);

		// lighting info
		SetSurfaceLighting(loadmodel, out, surfnum, in->styles, lightofs);
	}
}

// Refuses a node whose plane or surfaces lie outside their lumps
static void Mod_CheckNodeIndexes(model_t *loadmodel, int nodenum, int planenum, unsigned int firstsurface, unsigned int numsurfaces)
{
	unsigned int count = (unsigned int)loadmodel->numsurfaces;

	if (planenum < 0 || planenum >= loadmodel->numplanes) {
		Host_Error("[%s] node %d references invalid plane %d/%d", loadmodel->name, nodenum, planenum, loadmodel->numplanes);
	}
	if (firstsurface > count || numsurfaces > count - firstsurface) {
		Host_Error("[%s] node %d references invalid surface range [%u length %u, vs %d]", loadmodel->name, nodenum, firstsurface, numsurfaces, loadmodel->numsurfaces);
	}
}

// Returns child p of a node: node p, or leaf -1 - p for a negative p.
// Everything that walks the tree tells a leaf from a node by its negative
// contents, so a leaf must have them, and the leafs must be loaded first.
static mnode_t *Mod_NodeChild(model_t *loadmodel, int nodenum, int p)
{
	if (p >= 0) {
		if (p >= loadmodel->numnodes) {
			Host_Error("[%s] node %d references invalid node %d/%d", loadmodel->name, nodenum, p, loadmodel->numnodes);
		}
		return loadmodel->nodes + p;
	}

	p = -1 - p;
	if (p >= loadmodel->numleafs) {
		Host_Error("[%s] node %d references invalid leaf %d/%d", loadmodel->name, nodenum, p, loadmodel->numleafs);
	}
	if (loadmodel->leafs[p].contents >= 0) {
		Host_Error("[%s] node %d references leaf %d with contents %d", loadmodel->name, nodenum, p, loadmodel->leafs[p].contents);
	}
	return (mnode_t *)(loadmodel->leafs + p);
}

static void Mod_LoadNodes(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	int i, j, count, p;
	dnode_t *in;
	mnode_t *out;
	int max_nodes = (INT_MAX / sizeof(*out));

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Host_Error("Mod_LoadNodes: funny lump size in %s", loadmodel->name);
	}
	count = l->filelen / sizeof(*in);
	if (count <= 0 || count > max_nodes) {
		Host_Error("Mod_LoadNodes: invalid node count (%d vs 0-%d)", count, max_nodes);
		return;
	}
	out = (mnode_t *) Hunk_AllocName (count * sizeof(*out), loadmodel->name);

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) {
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3 + j] = LittleShort (in->maxs[j]);
		}

		out->firstsurface = LittleShort (in->firstface);
		out->numsurfaces = LittleShort (in->numfaces);
		p = LittleLong(in->planenum);
		Mod_CheckNodeIndexes(loadmodel, i, p, out->firstsurface, out->numsurfaces);
		out->plane = loadmodel->planes + p;

		for (j = 0; j < 2; j++) {
			out->children[j] = Mod_NodeChild(loadmodel, i, LittleShort(in->children[j]));
		}
	}

	Mod_SetParent(loadmodel, loadmodel->nodes, NULL, 1); // sets nodes and leafs
}

static void Mod_LoadNodes29a(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	int i, j, count, p;
	dnode29a_t *in;
	mnode_t *out;
	int max_nodes = (INT_MAX / sizeof(*out));

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Host_Error("Mod_LoadNodes: funny lump size in %s", loadmodel->name);
	}
	count = l->filelen / sizeof(*in);
	if (count <= 0 || count > max_nodes) {
		Host_Error("Mod_LoadNodes: invalid node count (%d vs 0-%d)", count, max_nodes);
		return;
	}
	out = (mnode_t *) Hunk_AllocName (count * sizeof(*out), loadmodel->name);

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) {
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3 + j] = LittleShort (in->maxs[j]);
		}

		out->firstsurface = LittleLong (in->firstface);
		out->numsurfaces = LittleLong (in->numfaces);
		p = LittleLong(in->planenum);
		Mod_CheckNodeIndexes(loadmodel, i, p, out->firstsurface, out->numsurfaces);
		out->plane = loadmodel->planes + p;

		for (j = 0; j < 2; j++) {
			out->children[j] = Mod_NodeChild(loadmodel, i, LittleLong(in->children[j]));
		}
	}

	Mod_SetParent(loadmodel, loadmodel->nodes, NULL, 1); // sets nodes and leafs
}

static void Mod_LoadNodesBSP2(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	int i, j, count, p;
	dnode_bsp2_t *in;
	mnode_t *out;
	int max_nodes = (INT_MAX / sizeof(*out));

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Host_Error("Mod_LoadNodes: funny lump size in %s", loadmodel->name);
	}
	count = l->filelen / sizeof(*in);
	if (count <= 0 || count > max_nodes) {
		Host_Error("Mod_LoadNodes: invalid node count (%d vs 0-%d)", count, max_nodes);
		return;
	}
	out = (mnode_t *) Hunk_AllocName (count * sizeof(*out), loadmodel->name);

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) {
			out->minmaxs[j] = LittleFloat (in->mins[j]);
			out->minmaxs[3 + j] = LittleFloat (in->maxs[j]);
		}

		out->firstsurface = LittleLong (in->firstface);
		out->numsurfaces = LittleLong (in->numfaces);
		p = LittleLong(in->planenum);
		Mod_CheckNodeIndexes(loadmodel, i, p, out->firstsurface, out->numsurfaces);
		out->plane = loadmodel->planes + p;

		for (j = 0; j < 2; j++) {
			out->children[j] = Mod_NodeChild(loadmodel, i, LittleLong(in->children[j]));
		}
	}

	Mod_SetParent(loadmodel, loadmodel->nodes, NULL, 1); // sets nodes and leafs
}

static qbool ContentsIsLiquid(int contents)
{
	switch (contents) {
	case CONTENTS_WATER:
	case CONTENTS_SLIME:
	case CONTENTS_LAVA:
		return true;
	default:
		return false;
	}
}

// Refuses a leaf whose mark surfaces lie outside their lump
static void Mod_CheckLeafMarksurfaces(model_t *loadmodel, int first_marksurface, int num_marksurfaces)
{
	if (first_marksurface < 0 || first_marksurface > loadmodel->nummarksurfaces) {
		Host_Error("Mod_LoadLeafs: first mark surface invalid (%d vs 0-%d)", first_marksurface, loadmodel->nummarksurfaces);
	}
	if (num_marksurfaces < 0 || num_marksurfaces > loadmodel->nummarksurfaces - first_marksurface) {
		Host_Error("Mod_LoadLeafs: num_marksurfaces invalid (%d + %d vs 0-%d)", first_marksurface, num_marksurfaces, loadmodel->nummarksurfaces);
	}
}

static void Mod_LoadLeafs(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	dleaf_t *in;
	mleaf_t *out;
	int i, j, count, p;
	int max_leafs = (INT_MAX / sizeof(*out));

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadLeafs: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	if (count <= 0 || count > max_leafs) {
		Host_Error("Mod_LoadNodes: invalid node count (%d vs 0-%d)", count, max_leafs);
		return;
	}
	out = (mleaf_t *) Hunk_AllocName ( count*sizeof(*out), loadmodel->name);

	loadmodel->leafs = out;
	loadmodel->numleafs = count;
	for (i = 0; i < count; i++, in++, out++) {
		int first_marksurface, num_marksurfaces;

		for (j = 0; j < 3; j++) {
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3 + j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		first_marksurface = LittleShort(in->firstmarksurface);
		num_marksurfaces = LittleShort(in->nummarksurfaces);
		Mod_CheckLeafMarksurfaces(loadmodel, first_marksurface, num_marksurfaces);

		out->firstmarksurface = loadmodel->marksurfaces + first_marksurface;
		out->nummarksurfaces = num_marksurfaces;

		p = LittleLong(in->visofs);
		if (loadmodel->visdata && (p < -1 || p >= loadmodel->visdata_length)) {
			Host_Error("Mod_LoadLeafs: invalid visibility offset in %s", loadmodel->name);
		}
		out->compressed_vis = (p == -1 || loadmodel->visdata == NULL) ? NULL : loadmodel->visdata + p;
		out->efrags = NULL;

		if (ContentsIsLiquid(out->contents)) {
			for (j = 0; j < out->nummarksurfaces; j++)
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
		}
	}
}

static void Mod_LoadLeafs29a(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	dleaf29a_t *in;
	mleaf_t *out;
	int i, j, count, p;
	int max_leafs = (INT_MAX / sizeof(*out));

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Host_Error("Mod_LoadLeafs: funny lump size in %s", loadmodel->name);
	}
	count = l->filelen / sizeof(*in);
	if (count <= 0 || count > max_leafs) {
		Host_Error("Mod_LoadNodes: invalid node count (%d vs 0-%d)", count, max_leafs);
		return;
	}
	out = (mleaf_t *) Hunk_AllocName (count*sizeof(*out), loadmodel->name);

	loadmodel->leafs = out;
	loadmodel->numleafs = count;
	for (i = 0; i < count; i++, in++, out++)	{
		for (j = 0; j < 3; j++) {
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3 + j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		p = LittleLong(in->firstmarksurface);
		out->nummarksurfaces = LittleLong(in->nummarksurfaces);
		Mod_CheckLeafMarksurfaces(loadmodel, p, out->nummarksurfaces);
		out->firstmarksurface = loadmodel->marksurfaces + p;

		p = LittleLong(in->visofs);
		if (loadmodel->visdata && (p < -1 || p >= loadmodel->visdata_length)) {
			Host_Error("Mod_LoadLeafs: invalid visibility offset in %s", loadmodel->name);
		}
		out->compressed_vis = (p == -1 || loadmodel->visdata == NULL) ? NULL : loadmodel->visdata + p;
		out->efrags = NULL;

		if (ContentsIsLiquid(out->contents)) {
			for (j = 0; j < out->nummarksurfaces; j++)
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
		}
	}
}

static void Mod_LoadLeafsBSP2(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	dleaf_bsp2_t *in;
	mleaf_t *out;
	int i, j, count, p;
	int max_leafs = (INT_MAX / sizeof(*out));

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Host_Error("Mod_LoadLeafs: funny lump size in %s", loadmodel->name);
	}
	count = l->filelen / sizeof(*in);
	if (count <= 0 || count > max_leafs) {
		Host_Error("Mod_LoadNodes: invalid node count (%d vs 0-%d)", count, max_leafs);
		return;
	}
	out = (mleaf_t *) Hunk_AllocName ( count*sizeof(*out), loadmodel->name);

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for (i = 0; i < count; i++, in++, out++)	{
		for (j = 0; j < 3; j++) {
			out->minmaxs[j] = LittleFloat (in->mins[j]);
			out->minmaxs[3 + j] = LittleFloat (in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		p = LittleLong(in->firstmarksurface);
		out->nummarksurfaces = LittleLong(in->nummarksurfaces);
		Mod_CheckLeafMarksurfaces(loadmodel, p, out->nummarksurfaces);
		out->firstmarksurface = loadmodel->marksurfaces + p;

		p = LittleLong(in->visofs);
		if (loadmodel->visdata && (p < -1 || p >= loadmodel->visdata_length)) {
			Host_Error("Mod_LoadLeafs: invalid visibility offset in %s", loadmodel->name);
		}
		out->compressed_vis = (p == -1 || loadmodel->visdata == NULL) ? NULL : loadmodel->visdata + p;
		out->efrags = NULL;

		if (ContentsIsLiquid(out->contents)) {
			for (j = 0; j < out->nummarksurfaces; j++) {
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
			}
		}
	}
}

// Called from Mod_LoadModel()
void Mod_LoadBrushModel(model_t *mod, void *buffer, int filesize)
{
	int i, mark;
	dheader_t *header;
	dmodel_t *bm;
	vec3_t normal;
	bspx_header_t* bspx_header;
	byte *reached;

	mod->type = mod_brush;

	header = (dheader_t *)buffer;

	mod->bspversion = LittleLong(header->version);

	if (mod->bspversion != Q1_BSPVERSION && mod->bspversion != HL_BSPVERSION && mod->bspversion != Q1_BSPVERSION2 && mod->bspversion != Q1_BSPVERSION29a) {
		Host_Error("Mod_LoadBrushModel: %s has wrong version number (%i should be %i (Quake), %i (HalfLife), %i (BSP2) or %i (2PSB))", mod->name, mod->bspversion, Q1_BSPVERSION, HL_BSPVERSION, Q1_BSPVERSION2, Q1_BSPVERSION29a);
	}
	// the model list of the server names the world first
	mod->isworldmodel = !strcmp(mod->name, cl.model_name[1]);

	// Mod_LoadModel only guarantees the version
	if (filesize < (int)sizeof(dheader_t)) {
		Host_Error("Mod_LoadBrushModel: %s is too short", mod->name);
	}

	// swap all the lumps
	for (i = 0; i < sizeof(dheader_t) / 4; i++) {
		((int *)header)[i] = LittleLong(((int *)header)[i]);
	}

	// Check lump sizes
	for (i = 0; i < sizeof(header->lumps) / sizeof(header->lumps[0]); ++i) {
		if (header->lumps[i].fileofs < 0 || header->lumps[i].fileofs > filesize) {
			Host_Error("Mod_LoadBrushModel(%s): lump %d has offset outside file (%d vs %d)", mod->name, i, header->lumps[i].fileofs, filesize);
			return;
		}
		if (header->lumps[i].filelen < 0) {
			Host_Error("Mod_LoadBrushModel(%s): lump %d has negative lump length (%d)", mod->name, i, header->lumps[i].filelen);
			return;
		}
		if (header->lumps[i].filelen > filesize - header->lumps[i].fileofs) {
			Host_Error("Mod_LoadBrushModel(%s): lump %d has length beyond file (%d + %d vs %d)", mod->name, i, header->lumps[i].fileofs, header->lumps[i].filelen, filesize);
			return;
		}
	}

	// check for BSPX extensions
	bspx_header = Mod_LoadBSPX(filesize, (byte*)header);

	// load into heap
	Mod_LoadVertexes(mod, &header->lumps[LUMP_VERTEXES], (byte*)header);
	if (mod->bspversion == Q1_BSPVERSION2 || mod->bspversion == Q1_BSPVERSION29a) {
		Mod_LoadEdgesBSP2(mod, &header->lumps[LUMP_EDGES], (byte*)header);
	}
	else {
		Mod_LoadEdges(mod, &header->lumps[LUMP_EDGES], (byte*)header);
	}
	Mod_LoadSurfedges(mod, &header->lumps[LUMP_SURFEDGES], (byte*)header);
	if (mod->bspversion == HL_BSPVERSION) {
		Mod_ParseWadsFromEntityLump(mod, &header->lumps[LUMP_ENTITIES], (byte*)header);
	}
	Mod_LoadTextures(mod, &header->lumps[LUMP_TEXTURES], (byte*)header);
	Mod_LoadLighting(mod, &header->lumps[LUMP_LIGHTING], (byte*)header, bspx_header);
	Mod_LoadPlanes(mod, &header->lumps[LUMP_PLANES], (byte*)header);
	Mod_LoadTexinfo(mod, &header->lumps[LUMP_TEXINFO], (byte*)header);
	if (mod->bspversion == Q1_BSPVERSION2 || mod->bspversion == Q1_BSPVERSION29a) {
		Mod_LoadFacesBSP2(mod, &header->lumps[LUMP_FACES], (byte*)header, bspx_header);
		Mod_LoadMarksurfacesBSP2(mod, &header->lumps[LUMP_MARKSURFACES], (byte*)header);
	}
	else {
		Mod_LoadFaces(mod, &header->lumps[LUMP_FACES], (byte*)header, bspx_header);
		Mod_LoadMarksurfaces(mod, &header->lumps[LUMP_MARKSURFACES], (byte*)header);
	}
	Mod_LoadVisibility(mod, &header->lumps[LUMP_VISIBILITY], (byte*)header);
	if (mod->bspversion == Q1_BSPVERSION29a) {
		Mod_LoadLeafs29a(mod, &header->lumps[LUMP_LEAFS], (byte*)header);
		Mod_LoadNodes29a(mod, &header->lumps[LUMP_NODES], (byte*)header);
	}
	else if (mod->bspversion == Q1_BSPVERSION2) {
		Mod_LoadLeafsBSP2(mod, &header->lumps[LUMP_LEAFS], (byte*)header);
		Mod_LoadNodesBSP2(mod, &header->lumps[LUMP_NODES], (byte*)header);
	}
	else {
		Mod_LoadLeafs(mod, &header->lumps[LUMP_LEAFS], (byte*)header);
		Mod_LoadNodes(mod, &header->lumps[LUMP_NODES], (byte*)header);
	}
	Mod_LoadSubmodels(mod, &header->lumps[LUMP_MODELS], (byte*)header);
	if (!mod->numsubmodels || mod->submodels[0].visleafs < 0
		|| mod->submodels[0].visleafs >= mod->numleafs) {
		Host_Error("Mod_LoadBrushModel: invalid visibility leaf count in %s", mod->name);
	}
	mod->numleafs = mod->submodels[0].visleafs;
	Mod_AllocVisBuffers(mod);

	// regular and alternate animation
	mod->numframes = 2;

	mark = Hunk_LowMark();
	reached = (byte *)Hunk_AllocName(mod->numnodes, mod->name);

	// set up the submodels (FIXME: this is confusing)
	for (i = 0; i < mod->numsubmodels; i++) {
		bm = &mod->submodels[i];

		if (bm->firstface < 0 || bm->numfaces < 0 || bm->firstface > mod->numsurfaces || bm->numfaces > mod->numsurfaces - bm->firstface) {
			Host_Error("Inline model %i has bad face range [%d length %d, vs %d]", i, bm->firstface, bm->numfaces, mod->numsurfaces);
		}
		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;

		// qbsp gives a submodel whose hull 0 is a single leaf the number of
		// nodes written before it, which is the node count when no submodel
		// with nodes follows
		mod->firstnode = bm->headnode[0];
		if ((unsigned)mod->firstnode > mod->numnodes) {
			Host_Error("Inline model %i has bad firstnode", i);
		}
		if (mod->firstnode < mod->numnodes) {
			Mod_CheckSubmodelTree(mod, reached, mod->nodes + mod->firstnode, 1);
		}

		VectorCopy(bm->maxs, mod->maxs);
		VectorCopy(bm->mins, mod->mins);

		mod->radius = RadiusFromBounds(mod->mins, mod->maxs);

		mod->numleafs = bm->visleafs;

		if (i < mod->numsubmodels - 1) {
			// duplicate the basic information
			char name[16];
			model_t* submodel;

			// The model list names the submodels of the world *1 to *N, and
			// they share the world's arrays. The submodels of any other brush
			// model aren't named, so that they can't replace the world's.
			if (!mod->isworldmodel) {
				break;
			}
			snprintf(name, sizeof(name), "*%i", i + 1);
			submodel = Mod_FindName(name);
			*submodel = *mod;
			strlcpy(submodel->name, name, sizeof(submodel->name));
			mod = submodel;
		}
	}
	Hunk_FreeToLowMark(mark);

	// set wall/ceiling drawflat
	for (i = 0; i < mod->numsurfaces; ++i) {
		msurface_t* s = &mod->surfaces[i];

		VectorCopy(s->plane->normal, normal);
		VectorNormalize(normal);

		if (normal[2] < -0.5 || normal[2] > 0.5) {
			// floor or ceiling
			s->flags |= SURF_DRAWFLAT_FLOOR;
		}
	}
}

// this is initial load, or callback from VID after a vid_restart
void R_LoadBrushModelTextures(model_t *m)
{
	char		*texname;
	texture_t	*tx;
	int			i, texmode, noscale_flag, alpha_flag, brighten_flag, mipTexLevel;
	byte		*data;
	int			width, height;

	// try load simple textures
	Mod_AddModelFlags(m);
	memset(m->simpletexture, 0, sizeof(m->simpletexture));
	m->simpletexture[0] = Mod_LoadSimpleTexture(m, 0);

	if (!m->textures) {
		return;
	}

	//	Com_Printf("lm %d %s\n", lightmode, loadmodel->name);

	for (i = 0; i < m->numtextures; i++)
	{
		tx = m->textures[i];
		if (!tx) {
			continue;
		}

		if (tx->loaded) {
			continue; // seems already loaded
		}

		R_TextureReferenceInvalidate(tx->gl_texturenum);
		R_TextureReferenceInvalidate(tx->fb_texturenum);

		if (m->isworldmodel && m->bspversion != HL_BSPVERSION && Mod_IsSkyTextureName(m, tx->name)) {
			if (!Mod_LoadExternalSkyTexture(tx)) {
				R_InitSky(tx);
			}
			tx->loaded = true;
			continue; // mark as loaded
		}

		noscale_flag = 0;
		noscale_flag = (!gl_scaleModelTextures.value && !m->isworldmodel) ? TEX_NOSCALE : noscale_flag;
		noscale_flag = (!gl_scaleTurbTextures.value  && Mod_IsTurbTextureName(m, tx->name)) ? TEX_NOSCALE : noscale_flag;
		noscale_flag = (!gl_scaleAlphaTextures.value  && Mod_IsAlphaTextureName(m, tx->name)) ? TEX_NOSCALE : noscale_flag;

		mipTexLevel  = noscale_flag ? 0 : gl_miptexLevel.value;

		texmode = TEX_MIPMAP | noscale_flag;
		brighten_flag = (!Mod_IsTurbTextureName(m, tx->name) && (lightmode == 2)) ? TEX_BRIGHTEN : 0;
		alpha_flag = Mod_IsAlphaTextureName(m, tx->name) ? TEX_ALPHA : 0;

		if (Mod_LoadExternalTexture(m, tx, texmode | alpha_flag, brighten_flag)) {
			tx->loaded = true; // mark as loaded
			continue;
		}

		if (m->bspversion == HL_BSPVERSION) {
			if ((data = WAD3_LoadTexture(tx))) {
				fs_netpath[0] = 0;
				tx->gl_texturenum = R_LoadTexturePixels(data, tx->name, tx->width, tx->height, texmode | alpha_flag);
				Q_free(data);
				tx->loaded = true; // mark as loaded
				continue;
			}

			tx->offsets[0] = 0; // this mean use r_notexture_mip, any better solution?
		}

		if (tx->offsets[0]) {
			// Mod_LoadTextures leaves out a mip level that lies outside the pixels
			while (!tx->offsets[mipTexLevel]) {
				mipTexLevel--;
			}
			texname = tx->name;
			width   = tx->width  >> mipTexLevel;
			height  = tx->height >> mipTexLevel;
			data    = (byte *) tx + tx->offsets[mipTexLevel];
		}
		else {
			texname = r_notexture_mip->name;
			width   = r_notexture_mip->width  >> mipTexLevel;
			height  = r_notexture_mip->height >> mipTexLevel;
			data     = (byte *) r_notexture_mip + r_notexture_mip->offsets[mipTexLevel];
		}

		tx->gl_texturenum = R_LoadTexture(texname, width, height, data, texmode | brighten_flag | alpha_flag, 1);
		if (!Mod_IsTurbTextureName(m, tx->name) && Img_HasFullbrights(data, width * height)) {
			tx->fb_texturenum = R_LoadTexture(va("@fb_%s", texname), width, height, data, texmode | TEX_FULLBRIGHT | alpha_flag, 1);
		}
		tx->loaded = true; // mark as loaded
	}
}
