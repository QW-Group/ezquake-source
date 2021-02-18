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
	int i;

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
		out->flags |= (SURF_DRAWTURB | SURF_DRAWTILED);
		out->texinfo->skippable = false;

		for (i = 0; i < 2; i++) {
			out->extents[i] = 16384;
			out->texturemins[i] = -8192;
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
	data = FS_LoadHunkFile (*litfilename, filesize);

	if (!data) {
		*litfilename = va("maps/%s.lit", mapname);
		data = FS_LoadHunkFile (*litfilename, filesize);
	}

	if (!data) {
		*litfilename = va("lits/%s.lit", mapname);
		data = FS_LoadHunkFile (*litfilename, filesize);
	}

	if (!data && groupname && !system) {
		*litfilename = va("maps/%s.lit", groupname);
		data = FS_LoadHunkFile (*litfilename, filesize);
	}

	if (!data && groupname && !system) {
		*litfilename = va("lits/%s.lit", groupname);
		data = FS_LoadHunkFile (*litfilename, filesize);
	}

	return data;
}

static void SetSurfaceLighting(model_t* loadmodel, msurface_t* out, byte* styles, int lightofs)
{
	int i;
	for (i = 0; i < MAXLIGHTMAPS; i++) {
		out->styles[i] = styles[i];
	}
	i = LittleLong(lightofs);
	if (i == -1) {
		out->samples = NULL;
	}
	else {
		out->samples = loadmodel->lightdata + (loadmodel->bspversion == HL_BSPVERSION ? i : i * 3);
	}
}

//Fills in s->texturemins[] and s->extents[]
static void CalcSurfaceExtents(model_t* loadmodel, msurface_t *s) {
	float mins[2], maxs[2], val;
	int i,j, e, bmins[2], bmaxs[2];
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
			val = v->position[0] * tex->vecs[j][0] +
				v->position[1] * tex->vecs[j][1] +
				v->position[2] * tex->vecs[j][2] +
				tex->vecs[j][3];
			if (i == 0 || val < mins[j]) {
				mins[j] = val;
			}
			if (i == 0 || val > maxs[j]) {
				maxs[j] = val;
			}
		}
	}

	for (i = 0; i < 2; i++) {
		bmins[i] = floor(mins[i]/16);
		bmaxs[i] = ceil(maxs[i]/16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
		if (!(tex->flags & TEX_SPECIAL) && s->extents[i] > 512 /* 256 */) {
			Host_Error("CalcSurfaceExtents: Bad surface extents");
		}
	}
}

static void Mod_SetParent(mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents < 0) {
		return;
	}
	Mod_SetParent(node->children[0], node);
	Mod_SetParent(node->children[1], node);
}

static void Mod_LoadLighting(model_t* loadmodel, lump_t* l, byte* mod_base, bspx_header_t* bspx_header)
{
	int i, lit_ver, mark;
	byte *in, *out, *data;
	char *litfilename;
	int filesize;
	extern cvar_t gl_loadlitfiles;

	loadmodel->lightdata = NULL;
	if (!l->filelen) {
		return;
	}

	if (loadmodel->bspversion == HL_BSPVERSION) {
		loadmodel->lightdata = (byte *) Hunk_AllocName(l->filelen, loadmodel->name);
		memcpy (loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
		return;
	}

	if (gl_loadlitfiles.integer == 1 || gl_loadlitfiles.integer == 3) {
		int threshold = (lightmode == 1 ? 255 : lightmode == 2 ? 170 : 128);
		int lumpsize;
		byte *rgb = Mod_BSPX_FindLump(bspx_header, "RGBLIGHTING", &lumpsize, mod_base);
		if (rgb && lumpsize == l->filelen * 3) {
			loadmodel->lightdata = (byte *) Hunk_AllocName(lumpsize, loadmodel->name);
			memcpy(loadmodel->lightdata, rgb, lumpsize);
			// we trust the inline RGB data to be bug free so we don't check it against the mono lightmap
			// what we do though is prevent color wash-out in brightly lit areas
			// (one day we may do it in R_BuildLightMap instead)
			out = loadmodel->lightdata;
			for (i = 0; i < l->filelen; i++, out += 3) {
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
	int flags = (gl_scaleskytextures.integer ? TEX_NOSCALE : 0);

	if (!R_ExternalTexturesEnabled(true)) {
		return false;
	}

	altname = TranslateTextureName (tx);
	mapname = Cvar_String("mapname");
	snprintf (solidname, sizeof(solidname), "%s_solid", tx->name);
	snprintf (alphaname, sizeof(alphaname), "%s_alpha", tx->name);

	solidskytexture = R_LoadTextureImage (va("textures/%s/%s", mapname, solidname), solidname, 0, 0, 0);
	if (!R_TextureReferenceIsValid(solidskytexture) && altname) {
		snprintf(altsolidname, sizeof(altsolidname), "%s_solid", altname);
		solidskytexture = R_LoadTextureImage (va("textures/%s", altsolidname), altsolidname, 0, 0, 0);
	}
	if (!R_TextureReferenceIsValid(solidskytexture)) {
		solidskytexture = R_LoadTextureImage(va("textures/%s", solidname), solidname, 0, 0, 0);
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

static void Mod_LoadVisibility(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	if (!l->filelen) {
		loadmodel->visdata = NULL;
		return;
	}
	loadmodel->visdata = (byte *) Hunk_AllocName (l->filelen, loadmodel->name);
	memcpy (loadmodel->visdata, mod_base + l->fileofs, l->filelen);
}

static void Mod_LoadSubmodels(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	dmodel_t *in, *out;
	int i, j, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Host_Error("Mod_LoadSubmodels: funny lump size in %s", loadmodel->name);
	}
	count = l->filelen / sizeof(*in);
	if (count > MAX_MODELS) {
		Host_Error("Mod_LoadSubmodels : count > MAX_MODELS");
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
	int i, j, num, max, altmax, pixels;
	miptex_t *mt;
	texture_t *tx, *tx2, *anims[10], *altanims[10];
	dmiptexlump_t *m;

	if (!l->filelen) {
		mod->textures = NULL;
		return;
	}

	m = (dmiptexlump_t *) (mod_base + l->fileofs);
	m->nummiptex = LittleLong (m->nummiptex);
	mod->numtextures = m->nummiptex;
	mod->textures = (texture_t **) Hunk_AllocName(m->nummiptex * sizeof(*mod->textures), mod->name);

	for (i = 0; i < m->nummiptex; i++) {
		m->dataofs[i] = LittleLong(m->dataofs[i]);
		if (m->dataofs[i] == -1) {
			continue;
		}

		mt = (miptex_t *)((byte *)m + m->dataofs[i]);
		mt->width  = LittleLong(mt->width);
		mt->height = LittleLong(mt->height);
		for (j = 0; j < MIPLEVELS; j++) {
			mt->offsets[j] = LittleLong(mt->offsets[j]);
		}

		if ((mt->width & 15) || (mt->height & 15)) {
			Host_Error("Texture %s is not 16 aligned", mt->name);
		}

		pixels = mt->width * mt->height / 64 * 85;// some magic numbers?
		if (mod->bspversion == HL_BSPVERSION) {
			pixels += 2 + 256 * 3;	/* palette and unknown two bytes */
		}
		tx = (texture_t *) Hunk_AllocName (sizeof(texture_t) + pixels, mod->name);
		mod->textures[i] = tx;

		memcpy (tx->name, mt->name, sizeof(tx->name));
		if (!tx->name[0]) {
			snprintf(tx->name, sizeof(tx->name), "unnamed%d", i);
			Com_DPrintf("Warning: unnamed texture in %s, renaming to %s\n", mod->name, tx->name);
		}

		tx->width  = mt->width;
		tx->height = mt->height;
		tx->index = i;
		tx->loaded = false; // so texture will be reloaded

		if (mod->bspversion == HL_BSPVERSION) {
			if (!mt->offsets[0]) {
				continue;
			}

			// the pixels immediately follow the structures
			memcpy (tx+1, mt+1, pixels);
			for (j = 0; j < MIPLEVELS; j++) {
				tx->offsets[j] = mt->offsets[j] + sizeof(texture_t) - sizeof(miptex_t);
			}
			continue;
		}

		if (mt->offsets[0]) {
			// the pixels immediately follow the structures
			memcpy (tx+1, mt+1, pixels);

			for (j = 0; j < MIPLEVELS; j++) {
				tx->offsets[j] = mt->offsets[j] + sizeof(texture_t) - sizeof(miptex_t);
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
				tx->flatcolor3ub = (255 << 24) + (data[0] << 0) + (data[1] << 8) + (data[2] << 16);

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
		for (j = 0; j < max; j++) {
			tx2 = anims[j];
			if (!tx2) {
				Host_Error("Mod_LoadTextures: Missing frame %i of %s", j, tx->name);
			}
			tx2->anim_total = max * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j + 1) * ANIM_CYCLE;
			tx2->anim_next = anims[ (j + 1)%max ];
			if (altmax) {
				tx2->alternate_anims = altanims[0];
			}
		}
		for (j = 0; j < altmax; j++) {
			tx2 = altanims[j];
			if (!tx2) {
				Host_Error("Mod_LoadTextures: Missing frame %i of %s", j, tx->name);
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

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Host_Error("Mod_LoadVertexes: funny lump size in %s", mod->name);
	}
	count = l->filelen / sizeof(*in);
	out = (mvertex_t *)Hunk_AllocName(count * sizeof(*out), mod->name);

	mod->vertexes = out;
	mod->numvertexes = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->position[0] = LittleFloat(in->point[0]);
		out->position[1] = LittleFloat(in->point[1]);
		out->position[2] = LittleFloat(in->point[2]);
	}
}

static void Mod_LoadEdges(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	dedge_t *in;
	medge_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Host_Error("Mod_LoadEdges: funny lump size in %s", loadmodel->name);
	}
	count = l->filelen / sizeof(*in);
	out = (medge_t *) Hunk_AllocName ( (count + 1) * sizeof(*out), loadmodel->name);

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->v[0] = (unsigned short) LittleShort(in->v[0]);
		out->v[1] = (unsigned short) LittleShort(in->v[1]);
	}
}

static void Mod_LoadEdgesBSP2(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	dedge29a_t *in;
	medge_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadEdges: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (medge_t *) Hunk_AllocName ( (count + 1) * sizeof(*out), loadmodel->name);

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->v[0] = LittleLong(in->v[0]);
		out->v[1] = LittleLong(in->v[1]);
	}
}

static void Mod_LoadSurfedges(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	int i, count, *in, *out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadSurfedges: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (int *) Hunk_AllocName ( count*sizeof(*out), loadmodel->name);

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for (i = 0; i < count; i++) {
		out[i] = LittleLong(in[i]);
	}
}

static void Mod_LoadPlanes(model_t* model, lump_t* l, byte* mod_base)
{
	int i, j, count, bits;
	mplane_t *out;
	dplane_t *in;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Host_Error("Mod_LoadPlanes: funny lump size in %s", model->name);
	}
	count = l->filelen / sizeof(*in);
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
	int i, j, count;
	int *in;
	msurface_t **out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Host_Error("Mod_LoadMarksurfaces: funny lump size in %s", loadmodel->name);
	}
	count = l->filelen / sizeof(*in);
	out = (msurface_t **)Hunk_AllocName(count * sizeof(*out), loadmodel->name);

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for (i = 0; i < count; i++) {
		j = LittleLong(in[i]);
		if (j >= loadmodel->numsurfaces) {
			Host_Error("Mod_LoadMarksurfaces: bad surface number");
		}
		out[i] = loadmodel->surfaces + j;
	}
}

static void Mod_LoadMarksurfaces(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	int i, j, count;
	short *in;
	msurface_t **out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Host_Error("Mod_LoadMarksurfaces: funny lump size in %s", loadmodel->name);
	}
	count = l->filelen / sizeof(*in);
	out = (msurface_t **) Hunk_AllocName ( count*sizeof(*out), loadmodel->name);

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for (i = 0; i < count; i++) {
		j = LittleShort(in[i]);
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

	if (!l->filelen) {
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

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadTexinfo: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
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
				Host_Error("Mod_LoadTexinfo: miptex >= loadmodel->numtextures");
			}
			out->texture = loadmodel->textures[out->miptex];
			if (!out->texture) {
				out->texture = r_notexture_mip; // texture not found
				out->flags = 0;
			}
		}
	}
}

static void Mod_LoadFaces(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	dface_t *in;
	msurface_t *out;
	int count, surfnum, planenum, side;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadFaces: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (msurface_t *) Hunk_AllocName ( count*sizeof(*out), loadmodel->name);

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for (surfnum = 0; surfnum < count; surfnum++, in++, out++) {
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);
		out->flags = 0;

		planenum = LittleShort(in->planenum);
		side = LittleShort(in->side);
		if (side) {
			out->flags |= SURF_PLANEBACK;
		}

		out->plane = loadmodel->planes + planenum;
		out->texinfo = loadmodel->texinfo + LittleShort (in->texinfo);

		CalcSurfaceExtents(loadmodel, out);

		// lighting info
		SetSurfaceLighting(loadmodel, out, in->styles, in->lightofs);

		SetTextureFlags(loadmodel, out, surfnum);
	}
}

static void Mod_LoadFacesBSP2(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	dface29a_t *in;
	msurface_t *out;
	int count, surfnum, planenum, side;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Host_Error("Mod_LoadFaces: funny lump size in %s", loadmodel->name);
	}
	count = l->filelen / sizeof(*in);
	out = (msurface_t *) Hunk_AllocName ( count*sizeof(*out), loadmodel->name);

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for (surfnum = 0; surfnum < count; surfnum++, in++, out++) {
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleLong(in->numedges);
		out->flags = 0;

		planenum = LittleLong(in->planenum);
		side = LittleLong(in->side);
		if (side) {
			out->flags |= SURF_PLANEBACK;
		}

		out->plane = loadmodel->planes + planenum;
		out->texinfo = loadmodel->texinfo + LittleLong(in->texinfo);

		CalcSurfaceExtents(loadmodel, out);

		// lighting info
		SetSurfaceLighting(loadmodel, out, in->styles, in->lightofs);

		SetTextureFlags(loadmodel, out, surfnum);
	}
}

static void Mod_LoadNodes(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	int i, j, count, p;
	dnode_t *in;
	mnode_t *out;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Host_Error("Mod_LoadNodes: funny lump size in %s", loadmodel->name);
	}
	count = l->filelen / sizeof(*in);
	out = (mnode_t *) Hunk_AllocName (count * sizeof(*out), loadmodel->name);

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) {
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3 + j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleShort (in->firstface);
		out->numsurfaces = LittleShort (in->numfaces);

		for (j = 0; j < 2; j++) {
			p = LittleShort (in->children[j]);
			if (p >= 0) {
				out->children[j] = loadmodel->nodes + p;
			}
			else {
				out->children[j] = (mnode_t *)(loadmodel->leafs + (-1 - p));
			}
		}
	}

	Mod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs
}

static void Mod_LoadNodes29a(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	int i, j, count, p;
	dnode29a_t *in;
	mnode_t *out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Host_Error("Mod_LoadNodes: funny lump size in %s", loadmodel->name);
	}
	count = l->filelen / sizeof(*in);
	out = (mnode_t *) Hunk_AllocName (count * sizeof(*out), loadmodel->name);

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) {
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3 + j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleLong (in->firstface);
		out->numsurfaces = LittleLong (in->numfaces);

		for (j = 0; j < 2; j++) {
			p = LittleLong (in->children[j]);
			if (p >= 0) {
				out->children[j] = loadmodel->nodes + p;
			}
			else {
				out->children[j] = (mnode_t *)(loadmodel->leafs + (-1 - p));
			}
		}
	}

	Mod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs
}

static void Mod_LoadNodesBSP2(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	int i, j, count, p;
	dnode_bsp2_t *in;
	mnode_t *out;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Host_Error("Mod_LoadNodes: funny lump size in %s", loadmodel->name);
	}
	count = l->filelen / sizeof(*in);
	out = (mnode_t *) Hunk_AllocName (count * sizeof(*out), loadmodel->name);

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) {
			out->minmaxs[j] = LittleFloat (in->mins[j]);
			out->minmaxs[3 + j] = LittleFloat (in->maxs[j]);
		}

		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleLong (in->firstface);
		out->numsurfaces = LittleLong (in->numfaces);

		for (j = 0; j < 2; j++) {
			p = LittleLong (in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t *) (loadmodel->leafs + (-1 - p));
		}
	}

	Mod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs
}

static void Mod_LoadLeafs(model_t* loadmodel, lump_t* l, byte* mod_base)
{
	dleaf_t *in;
	mleaf_t *out;
	int i, j, count, p;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadLeafs: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mleaf_t *) Hunk_AllocName ( count*sizeof(*out), loadmodel->name);

	loadmodel->leafs = out;
	loadmodel->numleafs = count;
	for (i = 0; i < count; i++, in++, out++)	{
		for (j = 0; j < 3; j++) {
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3 + j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		out->firstmarksurface = loadmodel->marksurfaces +
			LittleShort(in->firstmarksurface);
		out->nummarksurfaces = LittleShort(in->nummarksurfaces);

		p = LittleLong(in->visofs);
		out->compressed_vis = (p == -1) ? NULL : loadmodel->visdata + p;
		out->efrags = NULL;

		if (out->contents != CONTENTS_EMPTY) {
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

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Host_Error("Mod_LoadLeafs: funny lump size in %s", loadmodel->name);
	}
	count = l->filelen / sizeof(*in);
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

		out->firstmarksurface = loadmodel->marksurfaces +
			LittleLong(in->firstmarksurface);
		out->nummarksurfaces = LittleLong(in->nummarksurfaces);

		p = LittleLong(in->visofs);
		out->compressed_vis = (p == -1) ? NULL : loadmodel->visdata + p;
		out->efrags = NULL;

		if (out->contents != CONTENTS_EMPTY) {
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

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Host_Error("Mod_LoadLeafs: funny lump size in %s", loadmodel->name);
	}
	count = l->filelen / sizeof(*in);
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

		out->firstmarksurface = loadmodel->marksurfaces +
			LittleLong(in->firstmarksurface);
		out->nummarksurfaces = LittleLong(in->nummarksurfaces);

		p = LittleLong(in->visofs);
		out->compressed_vis = (p == -1) ? NULL : loadmodel->visdata + p;
		out->efrags = NULL;

		if (out->contents != CONTENTS_EMPTY) {
			for (j = 0; j < out->nummarksurfaces; j++)
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
		}
	}
}

// Called from Mod_LoadModel()
void Mod_LoadBrushModel(model_t *mod, void *buffer, int filesize)
{
	int i;
	dheader_t *header;
	dmodel_t *bm;
	vec3_t normal;
	bspx_header_t* bspx_header;

	mod->type = mod_brush;

	header = (dheader_t *)buffer;

	mod->bspversion = LittleLong(header->version);

	if (mod->bspversion != Q1_BSPVERSION && mod->bspversion != HL_BSPVERSION && mod->bspversion != Q1_BSPVERSION2 && mod->bspversion != Q1_BSPVERSION29a) {
		Host_Error("Mod_LoadBrushModel: %s has wrong version number (%i should be %i (Quake), %i (HalfLife), %i (BSP2) or %i (2PSB))", mod->name, mod->bspversion, Q1_BSPVERSION, HL_BSPVERSION, Q1_BSPVERSION2, Q1_BSPVERSION29a);
	}
	mod->isworldmodel = !strcmp(mod->name, va("maps/%s.bsp", host_mapname.string));

	// swap all the lumps
	for (i = 0; i < sizeof(dheader_t) / 4; i++) {
		((int *)header)[i] = LittleLong(((int *)header)[i]);
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
		Mod_LoadFacesBSP2(mod, &header->lumps[LUMP_FACES], (byte*)header);
		Mod_LoadMarksurfacesBSP2(mod, &header->lumps[LUMP_MARKSURFACES], (byte*)header);
	}
	else {
		Mod_LoadFaces(mod, &header->lumps[LUMP_FACES], (byte*)header);
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

	// regular and alternate animation
	mod->numframes = 2;

	// set up the submodels (FIXME: this is confusing)
	for (i = 0; i < mod->numsubmodels; i++) {
		bm = &mod->submodels[i];

		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;

		mod->firstnode = bm->headnode[0];
		if ((unsigned)mod->firstnode > mod->numnodes) {
			Host_Error("Inline model %i has bad firstnode", i);
		}

		VectorCopy(bm->maxs, mod->maxs);
		VectorCopy(bm->mins, mod->mins);

		mod->radius = RadiusFromBounds(mod->mins, mod->maxs);

		mod->numleafs = bm->visleafs;

		if (i < mod->numsubmodels - 1) {
			// duplicate the basic information
			char name[16];
			model_t* submodel;

			snprintf(name, sizeof(name), "*%i", i + 1);
			submodel = Mod_FindName(name);
			*submodel = *mod;
			strlcpy(submodel->name, name, sizeof(submodel->name));
			mod = submodel;
		}
	}

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
