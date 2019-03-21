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
// gl_model.c  -- model loading and caching

// models are the only shared resource between a client and server running on the same machine.

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "teamplay.h"
#include "rulesets.h"
#include "wad.h"
#include "crc.h"
#include "fmod.h"
#include "utils.h"

//VULT MODELS
void Mod_AddModelFlags(model_t *mod);

model_t	*loadmodel;
char	loadname[32];	// for hunk tags

void Mod_LoadSpriteModel (model_t *mod, void *buffer);
void Mod_LoadBrushModel (model_t *mod, void *buffer, int filesize);
void Mod_LoadAliasModel (model_t *mod, void *buffer, int filesize);
model_t *Mod_LoadModel (model_t *mod, qbool crash);
void *Mod_BSPX_FindLump(char *lumpname, int *plumpsize);

byte	mod_novis[MAX_MAP_LEAFS/8];

#define	MAX_MOD_KNOWN	512
model_t	mod_known[MAX_MOD_KNOWN];
int		mod_numknown;

void Mod_Init (void) {
	memset (mod_novis, 0xff, sizeof(mod_novis));
}

//Caches the data if needed
void *Mod_Extradata (model_t *mod) {
	void *r;

	r = Cache_Check (&mod->cache);
	if (r)
		return r;

	Mod_LoadModel (mod, true);

	if (!mod->cache.data)
		Sys_Error ("Mod_Extradata: caching failed");
	return mod->cache.data;
}

mleaf_t *Mod_PointInLeaf (vec3_t p, model_t *model) {
	mnode_t *node;
	float d;
	mplane_t *plane;

	if (!model || !model->nodes)
		Sys_Error ("Mod_PointInLeaf: bad model");

	node = model->nodes;
	while (1) {
		if (node->contents < 0)
			return (mleaf_t *)node;
		plane = node->plane;
		d = PlaneDiff(p, plane);
		node = (d > 0) ? node->children[0] : node->children[1];
	}

	return NULL;	// never reached
}

byte *Mod_DecompressVis (byte *in, model_t *model) {
	static byte	decompressed[MAX_MAP_LEAFS / 8];
	int c, row;
	byte *out;

	row = (model->numleafs + 7) >> 3;
	out = decompressed;

	if (!in) {	// no vis info, so make all visible
		while (row) {
			*out++ = 0xff;
			row--;
		}
		return decompressed;
	}

	do {
		if (*in) {
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		while (c) {
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);

	return decompressed;
}

byte *Mod_LeafPVS (mleaf_t *leaf, model_t *model) {
	if (leaf == model->leafs)
		return mod_novis;
	return Mod_DecompressVis (leaf->compressed_vis, model);
}

void Mod_ClearAll (void) {
	int i;
	model_t	*mod;

	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
		if (mod->type != mod_alias && mod->type != mod_alias3 && mod->type != mod_sprite)
			mod->needload = true;
}

model_t *Mod_FindName (char *name) {
	int i;
	model_t	*mod;

	if (!name[0])
		Sys_Error ("Mod_ForName: NULL name");

	// search the currently loaded models
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
		if (!strcmp (mod->name, name) )
			break;

	if (i == mod_numknown) {
		if (mod_numknown == MAX_MOD_KNOWN)
			Sys_Error ("mod_numknown == MAX_MOD_KNOWN");
		strlcpy (mod->name, name, sizeof (mod->name));
		mod->needload = true;
		mod_numknown++;
	}
	return mod;
}

void Mod_TouchModel (char *name) {
	model_t	*mod;

	mod = Mod_FindName (name);

	if (!mod->needload)	{
		if (mod->type == mod_alias || mod->type == mod_alias3 || mod->type == mod_sprite)
			Cache_Check (&mod->cache);
	}
}

// this is callback from VID after a vid_restart
void Mod_TouchModels (void)
{
	int i;
	model_t *mod;

	if (cls.state != ca_active)
		return; // seems we are not loaded models yet, so no need to do anything

	for (i = 1; i < MAX_MODELS; i++)
	{
		if (!cl.model_name[i][0])
			break;

		mod = cl.model_precache[i];

		if (mod->type == mod_alias || mod->type == mod_alias3 || mod->type == mod_sprite)
			Mod_Extradata (mod);
	}
}

//Loads a model into the cache
model_t *Mod_LoadModel (model_t *mod, qbool crash) {
	void *d;
	unsigned *buf;
	int namelen;
	int filesize;

	if (!mod->needload)	{
		if (mod->type == mod_alias || mod->type == mod_alias3 || mod->type == mod_sprite) {
			d = Cache_Check (&mod->cache);
			if (d)
				return mod;
		} else {
			return mod; // not cached at all
		}
	}

	namelen = strlen (mod->name);
	buf = NULL;
	if (namelen >= 4 && (!strcmp (mod->name + namelen - 4, ".mdl") ||
		(namelen >= 9 && mod->name[5] == 'b' && mod->name[6] == '_' && !strcmp (mod->name + namelen - 4, ".bsp"))))
	{
		char newname[MAX_QPATH];
		COM_StripExtension (mod->name, newname, sizeof(newname));
		COM_DefaultExtension (newname, ".md3");
		buf = (unsigned *) FS_LoadTempFile (newname, &filesize);
	}

	// load the file
	if (!buf)
		buf = (unsigned *) FS_LoadTempFile (mod->name, &filesize);
	if (!buf) {
		if (crash)
			Host_Error ("Mod_LoadModel: %s not found", mod->name);
		return NULL;
	}

	// allocate a new model
	COM_FileBase (mod->name, loadname);
	loadmodel = mod;
	FMod_CheckModel(mod->name, buf, filesize);

	// call the apropriate loader
	mod->needload = false;

	switch (LittleLong(*((unsigned *)buf))) {
	case IDPOLYHEADER:
		Mod_LoadAliasModel (mod, buf, filesize);
		break;

	case MD3_IDENT:
		Mod_LoadAlias3Model (mod, buf, filesize);
 		break;

	case IDSPRITEHEADER:
		Mod_LoadSpriteModel (mod, buf);
		break;

	default:
		Mod_LoadBrushModel (mod, buf, filesize);
		break;
	}

	return mod;
}

//Loads in a model for the given name
model_t *Mod_ForName (char *name, qbool crash) {
	model_t	*mod;

	mod = Mod_FindName (name);
	return Mod_LoadModel (mod, crash);
}

qbool Img_HasFullbrights (byte *pixels, int size) {
    int i;

    for (i = 0; i < size; i++)
        if (pixels[i] >= 224)
            return true;

    return false;
}

/*
===============================================================================
					BRUSHMODEL LOADING
===============================================================================
*/


#define ISTURBTEX(name)		((loadmodel->bspversion != HL_BSPVERSION && (name)[0] == '*') ||	\
							 (loadmodel->bspversion == HL_BSPVERSION && (name)[0] == '!'))

#define ISSKYTEX(name)		((name)[0] == 's' && (name)[1] == 'k' && (name)[2] == 'y')

#define ISALPHATEX(name)	((name)[0] == '{')

byte	*mod_base;
static struct bspx_header_s *bspx_header;

/* Some id maps have textures with identical names but different looks.
We hardcode a list of names, checksums and alternative names to provide a way
for external texture packs to differentiate them. */
static struct {
	int md4;
	char *origname, *newname;
} translate_names[] = {
//	{ 0x8a010dc0, "sky4", "sky4" },
	{ 0xde688b77, "sky4", "sky1" },
	{ 0x45d110ec, "metal5_2", "metal5_2_arc" },
	{ 0x0d275f87, "metal5_2", "metal5_2_x" },
	{ 0xf8e27da8, "metal5_4", "metal5_4_arc" },
	{ 0xa301c52e, "metal5_4", "metal5_4_double" },
	{ 0xfaa8bf77, "metal5_8", "metal5_8_back" },
	{ 0x88792923, "metal5_8", "metal5_8_rune" },
	{ 0xfe4f9f5a, "plat_top1", "plat_top1_bolt" },
	{ 0x9ac3fccf, "plat_top1", "plat_top1_cable" },
	{ 0, NULL, NULL },
};

static char *TranslateTextureName (texture_t *tx)
{
	int i;
	int checksum = 0;
	qbool checksum_done = false;

	for (i = 0; translate_names[i].origname; i++) {
		if (strcmp(tx->name, translate_names[i].origname))
			continue;
		if (!checksum_done) {
			checksum = Com_BlockChecksum(tx+1, tx->width*tx->height);
			checksum_done = true;
			//Com_DPrintf ("checksum(\"%s\") = 0x%x\n", tx->name, checksum);
		}
		if (translate_names[i].md4 == checksum) {
			//Com_DPrintf ("Translating %s --> %s\n", tx->name, translate_names[i].newname);
			assert (strlen(translate_names[i].newname) < sizeof(tx->name));
			return translate_names[i].newname;
		}
	}

	return NULL;
}

int Mod_LoadExternalTexture(texture_t *tx, int mode, int brighten_flag) {
	char *name, *altname, *mapname, *groupname;

	if (loadmodel->isworldmodel) {
		if (!gl_externalTextures_world.value)
			return 0;
	} else {
		if (!gl_externalTextures_bmodels.value)
			return 0;
	}

	name = tx->name;
	altname = TranslateTextureName (tx);
	mapname = TP_MapName();
	groupname = TP_GetMapGroupName(mapname, NULL);

	if (loadmodel->isworldmodel) {
		if ((tx->gl_texturenum = GL_LoadTextureImage (va("textures/%s/%s", mapname, name), name, 0, 0, mode | brighten_flag))) {
			if (!ISTURBTEX(name))
				tx->fb_texturenum = GL_LoadTextureImage (va("textures/%s/%s_luma", mapname, name), va("@fb_%s", name), 0, 0, mode | TEX_LUMA);
		} else {
			if (groupname) {
				if ((tx->gl_texturenum = GL_LoadTextureImage (va("textures/%s/%s", groupname, name), name, 0, 0, mode | brighten_flag))) {
					if (!ISTURBTEX(name))
						tx->fb_texturenum = GL_LoadTextureImage (va("textures/%s/%s_luma", groupname, name), va("@fb_%s", name), 0, 0, mode | TEX_LUMA);
				}
			}
		}
	} else {
		if ((tx->gl_texturenum = GL_LoadTextureImage (va("textures/bmodels/%s", name), name, 0, 0, mode | brighten_flag))) {
			if (!ISTURBTEX(name))
				tx->fb_texturenum = GL_LoadTextureImage (va("textures/bmodels/%s_luma", name), va("@fb_%s", name), 0, 0, mode | TEX_LUMA);
		}
	}

	if (!tx->gl_texturenum && altname) {
		if ((tx->gl_texturenum = GL_LoadTextureImage (va("textures/%s", altname), altname, 0, 0, mode | brighten_flag))) {
			if (!ISTURBTEX(name))
				tx->fb_texturenum = GL_LoadTextureImage (va("textures/%s_luma", altname), va("@fb_%s", altname), 0, 0, mode | TEX_LUMA);
		}
	}

	if (!tx->gl_texturenum) {
		if ((tx->gl_texturenum = GL_LoadTextureImage (va("textures/%s", name), name, 0, 0, mode | brighten_flag))) {
			if (!ISTURBTEX(name))
				tx->fb_texturenum = GL_LoadTextureImage (va("textures/%s_luma", name), va("@fb_%s", name), 0, 0, mode | TEX_LUMA);
		}
	}

	if (tx->fb_texturenum)
		tx->isLumaTexture = true;

	return tx->gl_texturenum;
}

static qbool Mod_LoadExternalSkyTexture (texture_t *tx)
{
	char *altname, *mapname;
	char solidname[MAX_QPATH], alphaname[MAX_QPATH];
	char altsolidname[MAX_QPATH], altalphaname[MAX_QPATH];
	byte alphapixel = 255;
	extern int solidskytexture, alphaskytexture;

	if (!gl_externalTextures_world.value)
		return false;

	altname = TranslateTextureName (tx);
	mapname = Cvar_String("mapname");
	snprintf (solidname, sizeof(solidname), "%s_solid", tx->name);
	snprintf (alphaname, sizeof(alphaname), "%s_alpha", tx->name);

	solidskytexture = GL_LoadTextureImage (va("textures/%s/%s", mapname, solidname), solidname, 0, 0, 0);
	if (!solidskytexture && altname) {
		snprintf (altsolidname, sizeof(altsolidname), "%s_solid", altname);
		solidskytexture = GL_LoadTextureImage (va("textures/%s", altsolidname), altsolidname, 0, 0, 0);
	}
	if (!solidskytexture)
		solidskytexture = GL_LoadTextureImage (va("textures/%s", solidname), solidname, 0, 0, 0);
	if (!solidskytexture)
		return false;

	alphaskytexture = GL_LoadTextureImage (va("textures/%s/%s", mapname, alphaname), alphaname, 0, 0, TEX_ALPHA);
	if (!alphaskytexture && altname) {
		snprintf (altalphaname, sizeof(altalphaname), "%s_alpha", altname);
		alphaskytexture = GL_LoadTextureImage (va("textures/%s", altalphaname), altalphaname, 0, 0, TEX_ALPHA);
	}
	if (!alphaskytexture)
		alphaskytexture = GL_LoadTextureImage (va("textures/%s", alphaname), alphaname, 0, 0, TEX_ALPHA);
	if (!alphaskytexture) {
		// Load a texture consisting of a single transparent pixel
		alphaskytexture = GL_LoadTexture (alphaname, 1, 1, &alphapixel, TEX_ALPHA, 1);
	}
	return true;
}

void R_LoadBrushModelTextures (model_t *m)
{
	char		*texname;
	texture_t	*tx;
	int			i, texmode, noscale_flag, alpha_flag, brighten_flag, mipTexLevel;
	byte		*data;
	int			width, height;

	loadmodel = m;

	// try load simple textures
	Mod_AddModelFlags(m);
	memset(loadmodel->simpletexture, 0, sizeof(loadmodel->simpletexture));
	loadmodel->simpletexture[0] = Mod_LoadSimpleTexture(loadmodel, 0);

	if (!loadmodel->textures)
		return;

//	Com_Printf("lm %d %s\n", lightmode, loadmodel->name);

	for (i = 0; i < loadmodel->numtextures; i++)
	{
		tx = loadmodel->textures[i];
		if (!tx)
			continue;

		if (tx->loaded)
			continue; // seems alredy loaded

		tx->gl_texturenum = tx->fb_texturenum = 0;

//		Com_Printf("tx %s\n", tx->name);

		if (loadmodel->isworldmodel && loadmodel->bspversion != HL_BSPVERSION && ISSKYTEX(tx->name)) {
			if (!Mod_LoadExternalSkyTexture (tx))
				R_InitSky (tx);
			tx->loaded = true;
			continue; // mark as loaded
		}

		noscale_flag = 0;
		noscale_flag = (!gl_scaleModelTextures.value && !loadmodel->isworldmodel) ? TEX_NOSCALE : noscale_flag;
		noscale_flag = (!gl_scaleTurbTextures.value  && ISTURBTEX(tx->name))      ? TEX_NOSCALE : noscale_flag;

		mipTexLevel  = noscale_flag ? 0 : gl_miptexLevel.value;

		texmode      = TEX_MIPMAP | noscale_flag;
		brighten_flag = (!ISTURBTEX(tx->name) && (lightmode == 2)) ? TEX_BRIGHTEN : 0;
		alpha_flag = ISALPHATEX(tx->name) ? TEX_ALPHA : 0;

		if (Mod_LoadExternalTexture(tx, texmode, brighten_flag)) {
			tx->loaded = true; // mark as loaded
			continue;
		}

		if (loadmodel->bspversion == HL_BSPVERSION) {
			if ((data = WAD3_LoadTexture(tx))) {
				fs_netpath[0] = 0;
				tx->gl_texturenum = GL_LoadTexturePixels (data, tx->name, tx->width, tx->height, texmode | alpha_flag);
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
		} else {
			texname = r_notexture_mip->name;
			width   = r_notexture_mip->width  >> mipTexLevel;
			height  = r_notexture_mip->height >> mipTexLevel;
			data     = (byte *) r_notexture_mip + r_notexture_mip->offsets[mipTexLevel];
		}

		tx->gl_texturenum = GL_LoadTexture (texname, width, height, data, texmode | alpha_flag | brighten_flag, 1);
		if (!ISTURBTEX(tx->name) && Img_HasFullbrights(data, width * height))
			tx->fb_texturenum = GL_LoadTexture (va("@fb_%s", texname), width, height, data, texmode | alpha_flag | TEX_FULLBRIGHT, 1);

		tx->loaded = true; // mark as loaded
	}
}

void Mod_LoadTextures (lump_t *l) {
	int i, j, num, max, altmax, pixels;
	miptex_t *mt;
	texture_t *tx, *tx2, *anims[10], *altanims[10];
	dmiptexlump_t *m;

	if (!l->filelen) {
		loadmodel->textures = NULL;
		return;
	}

	m = (dmiptexlump_t *) (mod_base + l->fileofs);
	m->nummiptex = LittleLong (m->nummiptex);
	loadmodel->numtextures = m->nummiptex;
	loadmodel->textures = (texture_t **) Hunk_AllocName (m->nummiptex * sizeof(*loadmodel->textures), loadname);

	for (i = 0; i < m->nummiptex; i++) {
		m->dataofs[i] = LittleLong(m->dataofs[i]);
		if (m->dataofs[i] == -1)
			continue;

		mt = (miptex_t *)((byte *)m + m->dataofs[i]);
		mt->width  = LittleLong (mt->width);
		mt->height = LittleLong (mt->height);
		for (j = 0; j < MIPLEVELS; j++)
			mt->offsets[j] = LittleLong (mt->offsets[j]);

		if ( (mt->width & 15) || (mt->height & 15) )
			Host_Error ("Texture %s is not 16 aligned", mt->name);

		pixels = mt->width * mt->height / 64 * 85;// some magic numbers?
		if (loadmodel->bspversion == HL_BSPVERSION)
			pixels += 2 + 256*3;	/* palette and unknown two bytes */
		tx = (texture_t *) Hunk_AllocName (sizeof(texture_t) + pixels, loadname);
		loadmodel->textures[i] = tx;

		memcpy (tx->name, mt->name, sizeof(tx->name));

		if (!tx->name[0]) {
			snprintf(tx->name, sizeof(tx->name), "unnamed%d", i);
			Com_DPrintf("Warning: unnamed texture in %s, renaming to %s\n", loadmodel->name, tx->name);
		}

		tx->width  = mt->width;
		tx->height = mt->height;
		tx->loaded = false; // so texture will be reloaded

		if (loadmodel->bspversion == HL_BSPVERSION)
		{
			if (!mt->offsets[0])
				continue;

			// the pixels immediately follow the structures
			memcpy (tx+1, mt+1, pixels);
			for (j = 0; j < MIPLEVELS; j++)
				tx->offsets[j] = mt->offsets[j] + sizeof(texture_t) - sizeof(miptex_t);
			continue;
		}

		if (mt->offsets[0])
		{
			// the pixels immediately follow the structures
			memcpy (tx+1, mt+1, pixels);

			for (j = 0; j < MIPLEVELS; j++)
				tx->offsets[j] = mt->offsets[j] + sizeof(texture_t) - sizeof(miptex_t);

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
			}
		}
	}

	R_LoadBrushModelTextures (loadmodel);

	// sequence the animations
	for (i = 0; i < m->nummiptex; i++) {
		tx = loadmodel->textures[i];
		if (!tx || tx->name[0] != '+')
			continue;
		if (tx->anim_next)
			continue;	// already sequenced

		// find the number of frames in the animation
		memset (anims, 0, sizeof(anims));
		memset (altanims, 0, sizeof(altanims));

		max = tx->name[1];
		altmax = 0;
		if (max >= 'a' && max <= 'z')
			max -= 'a' - 'A';
		if (max >= '0' && max <= '9') {
			max -= '0';
			altmax = 0;
			anims[max] = tx;
			max++;
		} else if (max >= 'A' && max <= 'J') {
			altmax = max - 'A';
			max = 0;
			altanims[altmax] = tx;
			altmax++;
		} else {
			Host_Error ("Mod_LoadTextures: Bad animating texture %s", tx->name);
		}

		for (j = i + 1; j < m->nummiptex; j++) {
			tx2 = loadmodel->textures[j];
			if (!tx2 || tx2->name[0] != '+')
				continue;
			if (strcmp (tx2->name+2, tx->name+2))
				continue;

			num = tx2->name[1];
			if (num >= 'a' && num <= 'z')
				num -= 'a' - 'A';
			if (num >= '0' && num <= '9') {
				num -= '0';
				anims[num] = tx2;
				if (num+1 > max)
					max = num + 1;
			} else if (num >= 'A' && num <= 'J') {
				num = num - 'A';
				altanims[num] = tx2;
				if (num+1 > altmax)
					altmax = num+1;
			} else {
				Host_Error ("Mod_LoadTextures: Bad animating texture %s", tx->name);
			}
		}

#define	ANIM_CYCLE	2
		// link them all together
		for (j = 0; j < max; j++) {
			tx2 = anims[j];
			if (!tx2)
				Host_Error ("Mod_LoadTextures: Missing frame %i of %s",j, tx->name);
			tx2->anim_total = max * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j + 1) * ANIM_CYCLE;
			tx2->anim_next = anims[ (j + 1)%max ];
			if (altmax)
				tx2->alternate_anims = altanims[0];
		}
		for (j = 0; j < altmax; j++) {
			tx2 = altanims[j];
			if (!tx2)
				Host_Error ("Mod_LoadTextures: Missing frame %i of %s",j, tx->name);
			tx2->anim_total = altmax * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j + 1) * ANIM_CYCLE;
			tx2->anim_next = altanims[ (j + 1) % altmax ];
			if (max)
				tx2->alternate_anims = anims[0];
		}
	}
}

// this is callback from VID after a vid_restart
void Mod_ReloadModelsTextures (void)
{
	int i, j;
	model_t *m;
	texture_t *tx;

	if (cls.state != ca_active)
		return; // seems we are not loaded models yet, so no need to reload textures

	// mark all textures as not loaded, for brush models only, this speed up loading.
	// seems textures shared between models, so we mark textures from ALL models than actually load textures, that why we use two for()
	for (i = 1; i < MAX_MODELS; i++)
	{
		if (!cl.model_name[i][0])
			break;

		m = cl.model_precache[i];
		if (m->type != mod_brush)
			continue; // actual only for brush models

		if (!m->textures)
			continue; // hmm

		for (j = 0; j < m->numtextures; j++)
		{
			tx = m->textures[j];
			if (!tx)
				continue;

			tx->loaded = false; // so texture will be reloaded
		}
	}

	// now actually load textures for brush models
	for (i = 1; i < MAX_MODELS; i++)
	{
		if (!cl.model_name[i][0])
			break;

		m = cl.model_precache[i];
		if (m->type != mod_brush)
			continue; // actual only for brush models

		R_LoadBrushModelTextures (m);
	}
}


static byte *LoadColoredLighting(char *name, char **litfilename, int *filesize) {
	qbool system;
	byte *data;
	char *groupname, *mapname;
	extern cvar_t gl_loadlitfiles;

	if (! (gl_loadlitfiles.integer == 1 || gl_loadlitfiles.integer == 2))
		return NULL;

	mapname = TP_MapName();
	groupname = TP_GetMapGroupName(mapname, &system);

	if (strcmp(name, va("maps/%s.bsp", mapname)))
		return NULL;

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

void Mod_LoadLighting (lump_t *l) {
	int i, lit_ver, mark;
	byte *in, *out, *data;
	char *litfilename;
	int filesize;
	extern cvar_t gl_loadlitfiles;

	loadmodel->lightdata = NULL;
	if (!l->filelen)
		return;

	if (loadmodel->bspversion == HL_BSPVERSION) {
		loadmodel->lightdata = (byte *) Hunk_AllocName(l->filelen, loadname);
		memcpy (loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
		return;
	}

	if (gl_loadlitfiles.integer == 1 || gl_loadlitfiles.integer == 3) {
		int threshold = (lightmode == 1 ? 255 : lightmode == 2 ? 170 : 128);
		int lumpsize;
		byte *rgb = Mod_BSPX_FindLump("RGBLIGHTING", &lumpsize);
		if (rgb && lumpsize == l->filelen * 3) {
			loadmodel->lightdata = (byte *) Hunk_AllocName(lumpsize, loadname);
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
			if (developer.integer || cl_warncmd.integer)
				Com_Printf("Static coloured lighting loaded\n");
			loadmodel->lightdata = data + 8;

			in = mod_base + l->fileofs;
			out = loadmodel->lightdata;
			if (Cvar_Value("gl_oldlitscaling")) {
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
			} else {
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
	
	if (loadmodel->lightdata)
		return;		// we have loaded inline RGB data
	
	//no .lit found, expand the white lighting data to color
	loadmodel->lightdata = (byte *) Hunk_AllocName (l->filelen * 3, va("%s_@lightdata", loadmodel->name));
	in = mod_base + l->fileofs;
	out = loadmodel->lightdata;
	for (i = 0; i < l->filelen; i++, out += 3)
		out[0] = out[1] = out[2] = *in++;
}

void Mod_LoadVisibility (lump_t *l) {
	if (!l->filelen) {
		loadmodel->visdata = NULL;
		return;
	}
	loadmodel->visdata = (byte *) Hunk_AllocName (l->filelen, loadname);
	memcpy (loadmodel->visdata, mod_base + l->fileofs, l->filelen);
}


static void Mod_ParseWadsFromEntityLump (lump_t *l)
{
	char *data;
	char *s, key[1024], value[1024];
	int i, j, k;

	if (!l->filelen)
		return;

	data = (char *)(mod_base + l->fileofs);
	data = COM_Parse(data);
	if (!data)
		return;

	if (com_token[0] != '{')
		return; // error

	while (1) {
		if (!(data = COM_Parse(data)))
			return; // error

		if (com_token[0] == '}')
			break; // end of worldspawn

		strlcpy(key, (com_token[0] == '_') ? com_token + 1 : com_token, sizeof(key));

		for (s = key + strlen(key) - 1; s >= key && *s == ' '; s--)		// remove trailing spaces
			*s = 0;

		if (!(data = COM_Parse(data)))
			return; // error

		strlcpy(value, com_token, sizeof(value));

		if (!strcmp("sky", key) || !strcmp("skyname", key))
			Cvar_Set(&r_skyname, value);

		if (!strcmp("wad", key)) {
			j = 0;
			for (i = 0; i < strlen(value); i++) {
				if (value[i] != ';' && value[i] != '\\' && value[i] != '/' && value[i] != ':')
					break;
			}
			if (!value[i])
				continue;
			for ( ; i < sizeof(value); i++) {
				// ignore path - the \\ check is for HalfLife... stupid windoze 'programmers'...
				if (value[i] == '\\' || value[i] == '/' || value[i] == ':') {
					j = i + 1;
				} else if (value[i] == ';' || value[i] == 0) {
					k = value[i];
					value[i] = 0;
					if (value[j])
						WAD3_LoadWadFile (value + j);
					j = i + 1;
					if (!k)
						break;
				}
			}
		}
	}
}

void Mod_LoadVertexes (lump_t *l) {
	dvertex_t *in;
	mvertex_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadVertexes: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mvertex_t *) Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for (i = 0; i < count; i++, in++, out++)	{
		out->position[0] = LittleFloat (in->point[0]);
		out->position[1] = LittleFloat (in->point[1]);
		out->position[2] = LittleFloat (in->point[2]);
	}
}

void Mod_LoadSubmodels (lump_t *l) {
	dmodel_t *in, *out;
	int i, j, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadSubmodels: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	if (count > MAX_MODELS)
		Host_Error("Mod_LoadSubmodels : count > MAX_MODELS");
	out = (dmodel_t *) Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for (i = 0; i < count; i++, in++, out++)	{
		for (j = 0; j < 3; j++) {	// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
			out->origin[j] = LittleFloat (in->origin[j]);
		}
		for (j = 0; j < MAX_MAP_HULLS; j++)
			out->headnode[j] = LittleLong (in->headnode[j]);
		out->visleafs = LittleLong (in->visleafs);
		out->firstface = LittleLong (in->firstface);
		out->numfaces = LittleLong (in->numfaces);
	}
}

void Mod_LoadEdges (lump_t *l) {
	dedge_t *in;
	medge_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadEdges: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (medge_t *) Hunk_AllocName ( (count + 1) * sizeof(*out), loadname);

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for (i = 0; i < count; i++, in++, out++)	{
		out->v[0] = (unsigned short) LittleShort(in->v[0]);
		out->v[1] = (unsigned short) LittleShort(in->v[1]);
	}
}

void Mod_LoadEdgesBSP2 (lump_t *l) {
	dedge29a_t *in;
	medge_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadEdges: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (medge_t *) Hunk_AllocName ( (count + 1) * sizeof(*out), loadname);

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->v[0] = LittleLong(in->v[0]);
		out->v[1] = LittleLong(in->v[1]);
	}
}

void Mod_LoadTexinfo (lump_t *l) {
	texinfo_t *in;
	mtexinfo_t *out;
	int i, j, k, count, miptex;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadTexinfo: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mtexinfo_t *) Hunk_AllocName (count*sizeof(*out), loadname);

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 2; j++)
			for (k = 0; k < 4; k++)
				out->vecs[j][k] = LittleFloat (in->vecs[j][k]);

		miptex = LittleLong (in->miptex);
		out->flags = LittleLong (in->flags);

		if (!loadmodel->textures) {
			out->texture = r_notexture_mip;	// checkerboard texture
			out->flags = 0;
		} else {
			if (miptex >= loadmodel->numtextures)
				Host_Error ("Mod_LoadTexinfo: miptex >= loadmodel->numtextures");
			out->texture = loadmodel->textures[miptex];
			if (!out->texture) {
				out->texture = r_notexture_mip; // texture not found
				out->flags = 0;
			}
		}
	}
}

//Fills in s->texturemins[] and s->extents[]
void CalcSurfaceExtents (msurface_t *s) {
	float mins[2], maxs[2], val;
	int i,j, e, bmins[2], bmaxs[2];
	mvertex_t *v;
	mtexinfo_t *tex;

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;

	for (i = 0; i < s->numedges; i++) {
		e = loadmodel->surfedges[s->firstedge+i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];

		for (j = 0; j < 2; j++) {
			val = v->position[0] * tex->vecs[j][0] +
				v->position[1] * tex->vecs[j][1] +
				v->position[2] * tex->vecs[j][2] +
				tex->vecs[j][3];
			if (i == 0 || val < mins[j])
				mins[j] = val;
			if (i == 0 || val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i = 0; i < 2; i++) {
		bmins[i] = floor(mins[i]/16);
		bmaxs[i] = ceil(maxs[i]/16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
		if ( !(tex->flags & TEX_SPECIAL) && s->extents[i] > 512 /* 256 */ )
			Host_Error ("CalcSurfaceExtents: Bad surface extents");
	}
}

static void SetSurfaceLighting(msurface_t* out, byte* styles, int lightofs)
{
	int i;
	for (i = 0; i < MAXLIGHTMAPS; i++)
		out->styles[i] = styles[i];
	i = LittleLong(lightofs);
	if (i == -1)
		out->samples = NULL;
	else
		out->samples = loadmodel->lightdata + (loadmodel->bspversion == HL_BSPVERSION ? i : i * 3);
}

static void SetTextureFlags(msurface_t* out)
{
	int i;

	// set the drawing flags flag
	// sky, turb and alpha should be mutually exclusive
	if (ISSKYTEX(out->texinfo->texture->name)) {	// sky
		out->flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
		GL_BuildSkySurfacePolys (out);	// build gl polys
		return;
	}

	if (ISTURBTEX(out->texinfo->texture->name)) {	// turbulent
		out->flags |= (SURF_DRAWTURB | SURF_DRAWTILED);
		for (i = 0; i < 2; i++) {
			out->extents[i] = 16384;
			out->texturemins[i] = -8192;
		}
		GL_SubdivideSurface (out);	// cut up polygon for warps
		return;
	}

	if (ISALPHATEX(out->texinfo->texture->name))
		out->flags |= SURF_DRAWALPHA;
}

void Mod_LoadFaces (lump_t *l) {
	dface_t *in;
	msurface_t *out;
	int count, surfnum, planenum, side;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadFaces: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (msurface_t *) Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for (surfnum = 0; surfnum < count; surfnum++, in++, out++) {
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);
		out->flags = 0;

		planenum = LittleShort(in->planenum);
		side = LittleShort(in->side);
		if (side)
			out->flags |= SURF_PLANEBACK;

		out->plane = loadmodel->planes + planenum;
		out->texinfo = loadmodel->texinfo + LittleShort (in->texinfo);

		CalcSurfaceExtents (out);

		// lighting info
		SetSurfaceLighting(out, in->styles, in->lightofs);

		SetTextureFlags(out);
	}
}

void Mod_LoadFacesBSP2 (lump_t *l) {
	dface29a_t *in;
	msurface_t *out;
	int count, surfnum, planenum, side;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadFaces: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (msurface_t *) Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for (surfnum = 0; surfnum < count; surfnum++, in++, out++) {
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleLong(in->numedges);
		out->flags = 0;

		planenum = LittleLong(in->planenum);
		side = LittleLong(in->side);
		if (side)
			out->flags |= SURF_PLANEBACK;

		out->plane = loadmodel->planes + planenum;
		out->texinfo = loadmodel->texinfo + LittleLong(in->texinfo);

		CalcSurfaceExtents (out);

		// lighting info
		SetSurfaceLighting(out, in->styles, in->lightofs);

		SetTextureFlags(out);
	}
}

void Mod_SetParent (mnode_t *node, mnode_t *parent) {
	node->parent = parent;
	if (node->contents < 0)
		return;
	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}

void Mod_LoadNodes (lump_t *l) {
	int i, j, count, p;
	dnode_t *in;
	mnode_t *out;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadNodes: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mnode_t *) Hunk_AllocName (count * sizeof(*out), loadname);

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
			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t *) (loadmodel->leafs + (-1 - p));
		}
	}

	Mod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs
}

void Mod_LoadNodes29a (lump_t *l) {
	int i, j, count, p;
	dnode29a_t *in;
	mnode_t *out;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadNodes: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mnode_t *) Hunk_AllocName (count * sizeof(*out), loadname);

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
			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t *) (loadmodel->leafs + (-1 - p));
		}
	}

	Mod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs
}

void Mod_LoadNodesBSP2 (lump_t *l) {
	int i, j, count, p;
	dnode_bsp2_t *in;
	mnode_t *out;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadNodes: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mnode_t *) Hunk_AllocName (count * sizeof(*out), loadname);

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

void Mod_LoadLeafs (lump_t *l) {
	dleaf_t *in;
	mleaf_t *out;
	int i, j, count, p;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadLeafs: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mleaf_t *) Hunk_AllocName ( count*sizeof(*out), loadname);

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

void Mod_LoadLeafs29a (lump_t *l) {
	dleaf29a_t *in;
	mleaf_t *out;
	int i, j, count, p;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadLeafs: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mleaf_t *) Hunk_AllocName ( count*sizeof(*out), loadname);

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

void Mod_LoadLeafsBSP2 (lump_t *l) {
	dleaf_bsp2_t *in;
	mleaf_t *out;
	int i, j, count, p;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadLeafs: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mleaf_t *) Hunk_AllocName ( count*sizeof(*out), loadname);

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

void Mod_LoadMarksurfaces (lump_t *l) {
	int i, j, count;
	short *in;
	msurface_t **out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadMarksurfaces: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (msurface_t **) Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for (i = 0; i < count; i++) {
		j = LittleShort(in[i]);
		if (j >= loadmodel->numsurfaces)
			Host_Error ("Mod_LoadMarksurfaces: bad surface number");
		out[i] = loadmodel->surfaces + j;
	}
}

void Mod_LoadMarksurfacesBSP2 (lump_t *l) {
	int i, j, count;
	int *in;
	msurface_t **out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadMarksurfaces: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (msurface_t **) Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for (i = 0; i < count; i++) {
		j = LittleLong(in[i]);
		if (j >= loadmodel->numsurfaces)
			Host_Error ("Mod_LoadMarksurfaces: bad surface number");
		out[i] = loadmodel->surfaces + j;
	}
}

void Mod_LoadSurfedges (lump_t *l) {
	int i, count, *in, *out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadSurfedges: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (int *) Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for (i = 0; i < count; i++)
		out[i] = LittleLong (in[i]);
}

void Mod_LoadPlanes (lump_t *l) {
	int i, j, count, bits;
	mplane_t *out;
	dplane_t *in;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadPlanes: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mplane_t *) Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for (i = 0; i < count; i++, in++, out++)	{
		bits = 0;
		for (j = 0; j < 3 ; j++) {
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1 << j;
		}

		out->dist = LittleFloat (in->dist);
		out->type = LittleLong (in->type);
		out->signbits = bits;
	}
}

float RadiusFromBounds (vec3_t mins, vec3_t maxs) {
	int i;
	vec3_t corner;

	for (i = 0; i < 3; i++)
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);

	return VectorLength (corner);
}

//============================================================
// BSPX loading

typedef struct bspx_header_s {
    char id[4];  // 'BSPX'
    int numlumps;
} bspx_header_t;

// lumps immediately follow:
typedef struct {
    char lumpname[24];
    int fileofs;
    int filelen;
} bspx_lump_t;

static void Mod_LoadBSPX (int filesize) {
	dheader_t *header;
	bspx_header_t *xheader;
	bspx_lump_t *lump;
	int i;
	int xofs;
	
	bspx_header = NULL;
	
	// find end of last lump
	header = (dheader_t *) mod_base;
	xofs = 0;
	for (i = 0; i < HEADER_LUMPS; i++)
		xofs = max(xofs, header->lumps[i].fileofs + header->lumps[i].filelen);
	
	if (xofs + sizeof(bspx_header_t) > filesize)
		return;

	xheader = (bspx_header_t *) (mod_base + xofs);
	xheader->numlumps = LittleLong (xheader->numlumps);

	if (xheader->numlumps < 0 || xofs + sizeof(bspx_header_t) + xheader->numlumps * sizeof(bspx_lump_t) > filesize)
		return;

	// byte-swap and check sanity
	lump = (bspx_lump_t *) (xheader + 1); // lumps immediately follow the header
	for (i = 0; i < xheader->numlumps; i++, lump++) {
		lump->lumpname[sizeof(lump->lumpname)-1] = '\0'; // make sure it ends with zero
		lump->fileofs = LittleLong(lump->fileofs);
		lump->filelen = LittleLong(lump->filelen);
		if (lump->fileofs < 0 || lump->filelen < 0 || (unsigned)(lump->fileofs + lump->filelen) > (unsigned)filesize)
			return;
	}

	// success
	bspx_header = xheader;
}

void *Mod_BSPX_FindLump(char *lumpname, int *plumpsize)
{
	int i;
	bspx_lump_t *lump;

	if (!bspx_header)
		return NULL;

	lump = (bspx_lump_t *) (bspx_header + 1);
	for (i = 0; i < bspx_header->numlumps; i++, lump++) {
		if (!strcmp(lump->lumpname, lumpname)) {
			if (plumpsize)
				*plumpsize = lump->filelen;
			return mod_base + lump->fileofs;
		}
	}

	return NULL;
}

//============================================================

void Mod_LoadBrushModel (model_t *mod, void *buffer, int filesize) {
	int i;
	dheader_t *header;
	dmodel_t *bm;

	loadmodel->type = mod_brush;

	header = (dheader_t *)buffer;

	mod->bspversion = LittleLong (header->version);

	if (mod->bspversion != Q1_BSPVERSION && mod->bspversion != HL_BSPVERSION && mod->bspversion != Q1_BSPVERSION2 && mod->bspversion != Q1_BSPVERSION29a)
		Host_Error ("Mod_LoadBrushModel: %s has wrong version number (%i should be %i (Quake), %i (HalfLife), %i (BSP2) or %i (2PSB))", mod->name, mod->bspversion, Q1_BSPVERSION, HL_BSPVERSION, Q1_BSPVERSION2, Q1_BSPVERSION29a);

	loadmodel->isworldmodel = !strcmp(loadmodel->name, va("maps/%s.bsp", host_mapname.string));

	// swap all the lumps
	mod_base = (byte *)header;

	for (i = 0; i < sizeof(dheader_t) / 4; i++)
		((int *) header)[i] = LittleLong (((int *) header)[i]);

	// check for BSPX extensions
	Mod_LoadBSPX (filesize);
	
	// load into heap

	Mod_LoadVertexes (&header->lumps[LUMP_VERTEXES]);
	if (mod->bspversion == Q1_BSPVERSION2 || mod->bspversion == Q1_BSPVERSION29a) {
		Mod_LoadEdgesBSP2(&header->lumps[LUMP_EDGES]);
	}
	else {
		Mod_LoadEdges(&header->lumps[LUMP_EDGES]);
	}
	Mod_LoadSurfedges (&header->lumps[LUMP_SURFEDGES]);
	if (loadmodel->bspversion == HL_BSPVERSION)
		Mod_ParseWadsFromEntityLump (&header->lumps[LUMP_ENTITIES]);
	Mod_LoadTextures (&header->lumps[LUMP_TEXTURES]);
	Mod_LoadLighting (&header->lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes (&header->lumps[LUMP_PLANES]);
	Mod_LoadTexinfo (&header->lumps[LUMP_TEXINFO]);
	if (mod->bspversion == Q1_BSPVERSION2 || mod->bspversion == Q1_BSPVERSION29a) {
		Mod_LoadFacesBSP2(&header->lumps[LUMP_FACES]);
		Mod_LoadMarksurfacesBSP2(&header->lumps[LUMP_MARKSURFACES]);
	}
	else {
		Mod_LoadFaces(&header->lumps[LUMP_FACES]);
		Mod_LoadMarksurfaces(&header->lumps[LUMP_MARKSURFACES]);
	}
	Mod_LoadVisibility (&header->lumps[LUMP_VISIBILITY]);
	if (mod->bspversion == Q1_BSPVERSION29a) {
		Mod_LoadLeafs29a(&header->lumps[LUMP_LEAFS]);
		Mod_LoadNodes29a(&header->lumps[LUMP_NODES]);
	}
	else if (mod->bspversion == Q1_BSPVERSION2) {
		Mod_LoadLeafsBSP2(&header->lumps[LUMP_LEAFS]);
		Mod_LoadNodesBSP2(&header->lumps[LUMP_NODES]);
	}
	else {
		Mod_LoadLeafs(&header->lumps[LUMP_LEAFS]);
		Mod_LoadNodes(&header->lumps[LUMP_NODES]);
	}
	Mod_LoadSubmodels (&header->lumps[LUMP_MODELS]);

	mod->numframes = 2;		// regular and alternate animation

	// set up the submodels (FIXME: this is confusing)
	for (i = 0; i < mod->numsubmodels; i++) {
		bm = &mod->submodels[i];

		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;

		mod->firstnode = bm->headnode[0];
		if ((unsigned)mod->firstnode > loadmodel->numnodes)
			Host_Error ("Inline model %i has bad firstnode", i);

		VectorCopy (bm->maxs, mod->maxs);
		VectorCopy (bm->mins, mod->mins);

		mod->radius = RadiusFromBounds (mod->mins, mod->maxs);

		mod->numleafs = bm->visleafs;

		if (i < mod->numsubmodels - 1) {
			// duplicate the basic information
			char name[16];

			snprintf (name, sizeof (name), "*%i", i+1);
			loadmodel = Mod_FindName (name);
			*loadmodel = *mod;
			strlcpy (loadmodel->name, name, sizeof (loadmodel->name));
			mod = loadmodel;
		}
	}
}

/*
==============================================================================
ALIAS MODELS
==============================================================================
*/

aliashdr_t	*pheader;

stvert_t	stverts[MAXALIASVERTS];
mtriangle_t	triangles[MAXALIASTRIS];

// a pose is a single set of vertexes.  a frame may be
// an animating sequence of poses
trivertx_t	*poseverts[MAXALIASFRAMES];
int			posenum;

//byte player_8bit_texels[320 * 200];
byte player_8bit_texels[256*256]; // Workaround for new player model, isn't proper for "real" quake skins

void *Mod_LoadAliasFrame (void * pin, maliasframedesc_t *frame) {
	trivertx_t *pinframe;
	int i;
	daliasframe_t *pdaliasframe;

	pdaliasframe = (daliasframe_t *)pin;

	strlcpy (frame->name, pdaliasframe->name, sizeof (frame->name));
	frame->firstpose = posenum;
	frame->numposes = 1;

	for (i = 0; i < 3; i++) {
		// these are byte values, so we don't have to worry about endianness
		frame->bboxmin[i] = pdaliasframe->bboxmin.v[i] * pheader->scale[i] + pheader->scale_origin[i];
		frame->bboxmax[i] = pdaliasframe->bboxmax.v[i] * pheader->scale[i] + pheader->scale_origin[i];
	}
	frame->radius = RadiusFromBounds (frame->bboxmin, frame->bboxmax);

	pinframe = (trivertx_t *)(pdaliasframe + 1);

	poseverts[posenum] = pinframe;
	posenum++;

	pinframe += pheader->numverts;

	return (void *)pinframe;
}

void *Mod_LoadAliasGroup (void * pin,  maliasframedesc_t *frame) {
	daliasgroup_t *pingroup;
	int i, numframes;
	daliasinterval_t *pin_intervals;
	void *ptemp;

	pingroup = (daliasgroup_t *) pin;

	numframes = LittleLong (pingroup->numframes);

	frame->firstpose = posenum;
	frame->numposes = numframes;

	for (i = 0; i < 3; i++) {
		// these are byte values, so we don't have to worry about endianness
		frame->bboxmin[i] = pingroup->bboxmin.v[i] * pheader->scale[i] + pheader->scale_origin[i];
		frame->bboxmax[i] = pingroup->bboxmax.v[i] * pheader->scale[i] + pheader->scale_origin[i];
	}
	frame->radius = RadiusFromBounds (frame->bboxmin, frame->bboxmax);

	pin_intervals = (daliasinterval_t *)(pingroup + 1);

	frame->interval = LittleFloat (pin_intervals->interval);

	pin_intervals += numframes;

	ptemp = (void *)pin_intervals;

	for (i = 0; i < numframes; i++) {
		poseverts[posenum] = (trivertx_t *)((daliasframe_t *)ptemp + 1);
		posenum++;

		ptemp = (trivertx_t *)((daliasframe_t *)ptemp + 1) + pheader->numverts;
	}

	return ptemp;
}

//=========================================================

typedef struct {
	short		x, y;
} floodfill_t;

extern unsigned d_8to24table[];

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
}

//Fill background pixels so mipmapping doesn't have haloes - Ed
void Mod_FloodFillSkin( byte *skin, int skinwidth, int skinheight ) {
	byte fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t fifo[FLOODFILL_FIFO_SIZE];
	int inpt = 0, outpt = 0, filledcolor = -1, i;

	if (filledcolor == -1) {
		filledcolor = 0;
		// attempt to find opaque black
		for (i = 0; i < 256; ++i)
			if (d_8to24table[i] == (255 << 0)) { // alpha 1.0
				filledcolor = i;
				break;
			}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255)) {
		//printf( "not filling skin from %d to %d\n", fillcolor, filledcolor );
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt) {
		int x = fifo[outpt].x, y = fifo[outpt].y, fdc = filledcolor;
		byte *pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)				FLOODFILL_STEP( -1, -1, 0 );
		if (x < skinwidth - 1)	FLOODFILL_STEP( 1, 1, 0 );
		if (y > 0)				FLOODFILL_STEP( -skinwidth, 0, -1 );
		if (y < skinheight - 1)	FLOODFILL_STEP( skinwidth, 0, 1 );
		skin[x + skinwidth * y] = fdc;
	}
}

static qbool Mod_IsLumaAllowed(model_t *mod)
{
	switch (mod->modhint)
	{
	case MOD_EYES:
	case MOD_BACKPACK:
	case MOD_PLAYER:

	case MOD_SENTRYGUN: // tf
	case MOD_DETPACK:   // tf

		return false; // no luma for such models

	default:

		return true; // luma allowed
	}
}

static int Mod_LoadExternalSkin(char *identifier, int *fb_texnum)
{
	char loadpath[64] = {0};
	int texmode, texnum;
	qbool luma_allowed = Mod_IsLumaAllowed(loadmodel);

	texnum     = 0;
	*fb_texnum = 0;

	if (RuleSets_DisallowExternalTexture(loadmodel))
		return 0;

	texmode = TEX_MIPMAP;
	if (!gl_scaleModelTextures.value)
		texmode |= TEX_NOSCALE;

	if (texnum)
		return texnum; // wow, we alredy have texnum?

	// try "textures/models/..." path

	snprintf (loadpath, sizeof(loadpath), "textures/models/%s", identifier);
	texnum = GL_LoadTextureImage (loadpath, identifier, 0, 0, texmode);
	if (texnum)
	{
		// not a luma actually, but which suffix use then? _fb or what?
		snprintf (loadpath, sizeof(loadpath), "textures/models/%s_luma", identifier);
		if (luma_allowed)
			*fb_texnum = GL_LoadTextureImage (loadpath, va("@fb_%s", identifier), 0, 0, texmode | TEX_FULLBRIGHT | TEX_ALPHA | TEX_LUMA);

		return texnum;
	}

	// try "textures/..." path

	snprintf (loadpath, sizeof(loadpath), "textures/%s", identifier);
	texnum = GL_LoadTextureImage (loadpath, identifier, 0, 0, texmode);
	if (texnum)
	{
		// not a luma actually, but which suffix use then? _fb or what?
		snprintf (loadpath, sizeof(loadpath), "textures/%s_luma", identifier);
		if (luma_allowed)
			*fb_texnum = GL_LoadTextureImage (loadpath, va("@fb_%s", identifier), 0, 0, texmode | TEX_FULLBRIGHT | TEX_ALPHA | TEX_LUMA);

		return texnum;
	}

	return 0; // we failed miserable
}

static void *Mod_LoadAllSkins (int numskins, daliasskintype_t *pskintype) {
	int i, j, k, s, groupskins, gl_texnum, fb_texnum, texmode;
	char basename[64], identifier[64];
	byte *skin;
	daliasskingroup_t *pinskingroup;
	daliasskininterval_t *pinskinintervals;

	skin = (byte *) (pskintype + 1);

	if (numskins < 1 || numskins > MAX_SKINS)
		Host_Error ("Mod_LoadAllSkins: Invalid # of skins: %d\n", numskins);

	s = pheader->skinwidth * pheader->skinheight;

	COM_StripExtension(COM_SkipPath(loadmodel->name), basename, sizeof(basename));

	texmode = TEX_MIPMAP;
	if (!gl_scaleModelTextures.value && !loadmodel->isworldmodel)
		texmode |= TEX_NOSCALE;

	for (i = 0; i < numskins; i++) {
		if (pskintype->type == ALIAS_SKIN_SINGLE) {
			Mod_FloodFillSkin (skin, pheader->skinwidth, pheader->skinheight);

			// save 8 bit texels for the player model to remap
			if (loadmodel->modhint == MOD_PLAYER) {
				if (s > sizeof(player_8bit_texels))
					Host_Error ("Mod_LoadAllSkins: Player skin too large");
				memcpy (player_8bit_texels, (byte *) (pskintype + 1), s);
			}

			snprintf (identifier, sizeof(identifier), "%s_%i", basename, i);

			gl_texnum = fb_texnum = 0;
			if (!(gl_texnum = Mod_LoadExternalSkin(identifier, &fb_texnum))) {
				gl_texnum = GL_LoadTexture (identifier, pheader->skinwidth, pheader->skinheight,
					(byte *) (pskintype + 1), texmode, 1);

				if (Img_HasFullbrights((byte *)(pskintype + 1),	pheader->skinwidth * pheader->skinheight))
					fb_texnum = GL_LoadTexture (va("@fb_%s", identifier), pheader->skinwidth,
					pheader->skinheight, (byte *) (pskintype + 1), texmode | TEX_FULLBRIGHT, 1);
			}

			pheader->gl_texturenum[i][0] = pheader->gl_texturenum[i][1] =
				pheader->gl_texturenum[i][2] = pheader->gl_texturenum[i][3] = gl_texnum;

			pheader->fb_texturenum[i][0] = pheader->fb_texturenum[i][1] =
				pheader->fb_texturenum[i][2] = pheader->fb_texturenum[i][3] = fb_texnum;

			pskintype = (daliasskintype_t *)((byte *) (pskintype + 1) + s);
		} else {
			// animating skin group.  yuck.
			pskintype++;
			pinskingroup = (daliasskingroup_t *)pskintype;
			groupskins = LittleLong (pinskingroup->numskins);
			pinskinintervals = (daliasskininterval_t *) (pinskingroup + 1);

			pskintype = (void *) (pinskinintervals + groupskins);

			for (j = 0; j < groupskins; j++) {
				Mod_FloodFillSkin (skin, pheader->skinwidth, pheader->skinheight);

				snprintf (identifier, sizeof(identifier), "%s_%i_%i", basename, i, j);

				gl_texnum = fb_texnum = 0;
				if (!(gl_texnum = Mod_LoadExternalSkin(identifier, &fb_texnum))) {
					gl_texnum = GL_LoadTexture (identifier, pheader->skinwidth,
						pheader->skinheight, (byte *) (pskintype), texmode, 1);

					if (Img_HasFullbrights((byte *) (pskintype), pheader->skinwidth*pheader->skinheight))
						fb_texnum = GL_LoadTexture (va("@fb_%s", identifier),
						pheader->skinwidth,  pheader->skinheight, (byte *) (pskintype), texmode | TEX_FULLBRIGHT, 1);
				}

				pheader->gl_texturenum[i][j & 3] = gl_texnum;
				pheader->fb_texturenum[i][j & 3] = fb_texnum;

				pskintype = (daliasskintype_t *) ((byte *) (pskintype) + s);
			}

			for (k = j; j < 4; j++) {
				pheader->gl_texturenum[i][j & 3] = pheader->gl_texturenum[i][j - k];
				pheader->fb_texturenum[i][j & 3] = pheader->fb_texturenum[i][j - k];
			}
		}
	}
	return pskintype;
}

//=========================================================================

void Mod_LoadAliasModel (model_t *mod, void *buffer, int filesize) {
	int i, j, version, numframes, size, start, end, total;
	mdl_t *pinmodel;
	stvert_t *pinstverts;
	dtriangle_t *pintriangles;
	daliasframetype_t *pframetype;
	daliasskintype_t *pskintype;
	aliasframetype_t frametype;

	//VULT MODELS
	Mod_AddModelFlags(mod);

	if (mod->modhint == MOD_PLAYER || mod->modhint == MOD_EYES) {
		mod->crc = CRC_Block(buffer, filesize);
	}

	start = Hunk_LowMark ();

	pinmodel = (mdl_t *)buffer;

	version = LittleLong (pinmodel->version);

	if (version != ALIAS_VERSION) {
		Hunk_FreeToLowMark (start);
		Host_Error ("Mod_LoadAliasModel: %s has wrong version number (%i should be %i)\n", mod->name, version, ALIAS_VERSION);
		return;
	}

	// allocate space for a working header, plus all the data except the frames, skin and group info
	size = 	sizeof (aliashdr_t)
			+ (LittleLong (pinmodel->numframes) - 1) *
			sizeof (pheader->frames[0]);
	pheader = (aliashdr_t *) Hunk_AllocName (size, loadname);

	mod->flags = LittleLong (pinmodel->flags);

	// endian-adjust and copy the data, starting with the alias model header
	pheader->boundingradius = LittleFloat (pinmodel->boundingradius);
	pheader->numskins = LittleLong (pinmodel->numskins);
	pheader->skinwidth = LittleLong (pinmodel->skinwidth);
	pheader->skinheight = LittleLong (pinmodel->skinheight);

	if (pheader->skinheight > MAX_LBM_HEIGHT)
		Host_Error ("Mod_LoadAliasModel: model %s has a skin taller than %d", mod->name, MAX_LBM_HEIGHT);

	pheader->numverts = LittleLong (pinmodel->numverts);

	if (pheader->numverts <= 0)
		Host_Error ("Mod_LoadAliasModel: model %s has no vertices", mod->name);

	if (pheader->numverts > MAXALIASVERTS)
		Host_Error ("Mod_LoadAliasModel: model %s has too many vertices", mod->name);

	pheader->numtris = LittleLong (pinmodel->numtris);

	if (pheader->numtris <= 0)
		Host_Error ("Mod_LoadAliasModel: model %s has no triangles", mod->name);

	pheader->numframes = LittleLong (pinmodel->numframes);
	numframes = pheader->numframes;
	if (numframes < 1)
		Host_Error ("Mod_LoadAliasModel: Invalid # of frames: %d\n", numframes);

	pheader->size = LittleFloat (pinmodel->size) * ALIAS_BASE_SIZE_RATIO;
	mod->synctype = LittleLong (pinmodel->synctype);
	mod->numframes = pheader->numframes;

	for (i = 0; i < 3; i++) {
		pheader->scale[i] = LittleFloat (pinmodel->scale[i]);
		pheader->scale_origin[i] = LittleFloat (pinmodel->scale_origin[i]);
		pheader->eyeposition[i] = LittleFloat (pinmodel->eyeposition[i]);
	}

	// load the skins
	pskintype = (daliasskintype_t *)&pinmodel[1];
	pskintype = Mod_LoadAllSkins (pheader->numskins, pskintype);

	// load base s and t vertices
	pinstverts = (stvert_t *)pskintype;

	for (i = 0; i < pheader->numverts; i++) {
		stverts[i].onseam = LittleLong (pinstverts[i].onseam);
		stverts[i].s = LittleLong (pinstverts[i].s);
		stverts[i].t = LittleLong (pinstverts[i].t);
	}

	// load triangle lists
	pintriangles = (dtriangle_t *)&pinstverts[pheader->numverts];

	for (i = 0; i < pheader->numtris; i++) {
		triangles[i].facesfront = LittleLong (pintriangles[i].facesfront);

		for (j = 0; j < 3; j++)
			triangles[i].vertindex[j] =	LittleLong (pintriangles[i].vertindex[j]);
	}

	// load the frames
	posenum = 0;
	pframetype = (daliasframetype_t *)&pintriangles[pheader->numtris];

	mod->mins[0] = mod->mins[1] = mod->mins[2] = 255;
	mod->maxs[0] = mod->maxs[1] = mod->maxs[2] = 0;

	for (i = 0; i < numframes; i++) {
		frametype = LittleLong (pframetype->type);

		if (frametype == ALIAS_SINGLE)
			pframetype = (daliasframetype_t *) Mod_LoadAliasFrame (pframetype + 1, &pheader->frames[i]);
		else
			pframetype = (daliasframetype_t *) Mod_LoadAliasGroup (pframetype + 1, &pheader->frames[i]);

		for (j = 0; j < 3; j++) {
			mod->mins[j] = min (mod->mins[j], pheader->frames[i].bboxmin[j]);
			mod->maxs[j] = max (mod->maxs[j], pheader->frames[i].bboxmax[j]);
		}
	}

	mod->radius = RadiusFromBounds (mod->mins, mod->maxs);

	pheader->numposes = posenum;

	mod->type = mod_alias;

	// build the draw lists
	GL_MakeAliasModelDisplayLists (mod, pheader);

	// move the complete, relocatable alias model to the cache
	end = Hunk_LowMark ();
	total = end - start;

	Cache_Alloc (&mod->cache, total, loadname);
	if (!mod->cache.data)
		return;
	memcpy (mod->cache.data, pheader, total);

	// try load simple textures
	memset(mod->simpletexture, 0, sizeof(mod->simpletexture));
	for (i = 0; i < MAX_SIMPLE_TEXTURES && i < pheader->numskins; i++)
		mod->simpletexture[i] = Mod_LoadSimpleTexture(mod, i);

	Hunk_FreeToLowMark (start);
}

//=============================================================================


int Mod_LoadExternalSpriteSkin(char *identifier, int framenum) {
	char loadpath[64];
	int texmode, texnum;

	texmode = TEX_MIPMAP | TEX_ALPHA;
	if (!gl_scaleModelTextures.value && !loadmodel->isworldmodel)
		texmode |= TEX_NOSCALE;

	snprintf (loadpath, sizeof(loadpath), "textures/sprites/%s", identifier);
	texnum = GL_LoadTextureImage (loadpath, identifier, 0, 0, texmode);

	if (!texnum) {
		snprintf (loadpath, sizeof(loadpath), "textures/%s", identifier);
		texnum = GL_LoadTextureImage (loadpath, identifier, 0, 0, texmode);
	}

	return texnum;
}

void *Mod_LoadSpriteFrame (void * pin, mspriteframe_t **ppframe, int framenum) {
	dspriteframe_t *pinframe;
	mspriteframe_t *pspriteframe;
	char basename[64], identifier[64];
	int width, height, size, origin[2], texnum, texmode;

	texmode = TEX_MIPMAP | TEX_ALPHA;
	if (!gl_scaleModelTextures.value && !loadmodel->isworldmodel)
		texmode |= TEX_NOSCALE;

	COM_StripExtension(COM_SkipPath(loadmodel->name), basename, sizeof(basename));

	pinframe = (dspriteframe_t *) pin;

	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	size = width * height;

	pspriteframe = (mspriteframe_t *) Hunk_AllocName (sizeof (mspriteframe_t),loadname);

	memset (pspriteframe, 0, sizeof (mspriteframe_t));

	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong (pinframe->origin[0]);
	origin[1] = LittleLong (pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];

	snprintf (identifier, sizeof(identifier), "sprites/%s_%i", basename, framenum);
	if (!(texnum = Mod_LoadExternalSpriteSkin(identifier, framenum)))
		texnum = GL_LoadTexture (identifier, width, height, (byte *) (pinframe + 1), texmode, 1);

	pspriteframe->gl_texturenum = texnum;

	return (void *) ((byte *) pinframe + sizeof (dspriteframe_t) + size);
}

void *Mod_LoadSpriteGroup (void * pin, mspriteframe_t **ppframe, int framenum) {
	dspritegroup_t *pingroup;
	mspritegroup_t *pspritegroup;
	int i, numframes;
	dspriteinterval_t *pin_intervals;
	float *poutintervals;
	void *ptemp;

	pingroup = (dspritegroup_t *) pin;

	numframes = LittleLong (pingroup->numframes);

	if (numframes < 1)
		Host_Error ("Mod_LoadSpriteGroup: numframes < 1");

	pspritegroup = (mspritegroup_t *) Hunk_AllocName (sizeof (mspritegroup_t) +
				(numframes - 1) * sizeof (pspritegroup->frames[0]), loadname);

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *) pspritegroup;

	pin_intervals = (dspriteinterval_t *) (pingroup + 1);

	poutintervals = (float *) Hunk_AllocName (numframes * sizeof (float), loadname);

	pspritegroup->intervals = poutintervals;

	for (i = 0; i < numframes; i++) {
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Host_Error ("Mod_LoadSpriteGroup: interval <= 0");

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *) pin_intervals;

	for (i = 0; i < numframes; i++)
		ptemp = Mod_LoadSpriteFrame (ptemp, &pspritegroup->frames[i], framenum * 100 + i);

	return ptemp;
}

void Mod_LoadSpriteModel (model_t *mod, void *buffer) {
	int i, j, version, numframes, size, start, offset, sztmp;
	dsprite_t *pin;
	msprite_t *psprite;
	dspriteframetype_t *pframetype;

	msprite2_t *psprite2;


	// remember point
	start = Hunk_LowMark ();


	pin = (dsprite_t *)buffer;

	version = LittleLong (pin->version);

	if (version != SPRITE_VERSION) {
		Host_Error ("Mod_LoadSpriteModel: %s has wrong version number (%i should be %i)", mod->name, version, SPRITE_VERSION);
		return;
	}

	numframes = LittleLong (pin->numframes);

	size = sizeof (msprite_t) +	(numframes - 1) * sizeof (psprite->frames);

	psprite = (msprite_t *) Hunk_AllocName (size, loadname);

	psprite->type = LittleLong (pin->type);
	psprite->maxwidth = LittleLong (pin->width);
	psprite->maxheight = LittleLong (pin->height);
	psprite->beamlength = LittleFloat (pin->beamlength);
	mod->synctype = LittleLong (pin->synctype);
	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = -psprite->maxwidth/2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth/2;
	mod->mins[2] = -psprite->maxheight/2;
	mod->maxs[2] = psprite->maxheight/2;

	// load the frames
	if (numframes < 1 || numframes > MAX_SPRITE_FRAMES)
		Host_Error ("Mod_LoadSpriteModel: Invalid # of frames: %d\n", numframes);

	mod->numframes = numframes;

	pframetype = (dspriteframetype_t *)(pin + 1);

	size = sizeof(msprite2_t);

	for (i = 0; i < numframes; i++) {
		spriteframetype_t	frametype;

		frametype = LittleLong (pframetype->type);
		psprite->frames[i].type = frametype;

		if (frametype == SPR_SINGLE) {
			pframetype = (dspriteframetype_t *)	Mod_LoadSpriteFrame (pframetype + 1, &psprite->frames[i].frameptr, i);
			size += (int)sizeof(mspriteframe_t);
		} else {
			pframetype = (dspriteframetype_t *)	Mod_LoadSpriteGroup (pframetype + 1, &psprite->frames[i].frameptr, i);
			size += (((mspritegroup_t*)(psprite->frames[i].frameptr))->numframes) * (int)sizeof(mspriteframe2_t);
		}
	}

	psprite2 = Q_malloc(size); // !!!!!!! Q_free

	psprite2->type		= psprite->type;
	psprite2->maxwidth	= psprite->maxwidth;
	psprite2->maxheight	= psprite->maxheight;
	psprite2->numframes	= psprite->numframes;

	for (i = 0, offset = sizeof(msprite2_t); i < numframes; i++) {
		psprite2->frames[i].type   = psprite->frames[i].type;
		psprite2->frames[i].offset = offset;

		if (psprite2->frames[i].type == SPR_SINGLE) {
			psprite2->frames[i].numframes = 1; // always one

			sztmp = (int)sizeof(mspriteframe_t);
			if (offset + sztmp > size)
				Sys_Error("Mod_LoadSpriteModel: internal error");

			memcpy((byte*)(psprite2) + offset, psprite->frames[i].frameptr, sztmp);
			offset += sztmp;
		}
		else {
			psprite2->frames[i].numframes = ((mspritegroup_t*)(psprite->frames[i].frameptr))->numframes;

			for (j = 0; j < psprite2->frames[i].numframes; j++) {
				mspriteframe2_t spr;

				sztmp = (int)sizeof(mspriteframe2_t);
				if (offset + sztmp > size)
					Sys_Error("Mod_LoadSpriteModel: internal error");

				memset(&spr, 0, sizeof(spr));
				spr.frame    = *(((mspritegroup_t*)(psprite->frames[i].frameptr))->frames[j]); // copy whole struct
				spr.interval =   ((mspritegroup_t*)(psprite->frames[i].frameptr))->intervals[j];
				memcpy((byte*)(psprite2) + offset, &spr, sztmp);
				offset += sztmp;
			}
		}
	}

	mod->type = mod_sprite;

	// move the complete, relocatable model to the cache

	Cache_Alloc (&mod->cache, size, loadname);
	if (!mod->cache.data) {
		Q_free(psprite2);
		Hunk_FreeToLowMark (start);
		return;
	}

	memcpy (mod->cache.data, psprite2, size);

	Q_free(psprite2);
	Hunk_FreeToLowMark (start);
}

//VULT MODELS
//This is incase we want to use a function other than Mod_LoadAliasModel
//It was used in one of the older versions when it supported Q2 Models.
void Mod_AddModelFlags(model_t *mod)
{
	mod->modhint = mod->modhint_trail = MOD_NORMAL;
	if (!strcmp(mod->name, "progs/caltrop.mdl") ||
		!strcmp(mod->name, "progs/biggren.mdl") ||
/*		!strcmp(mod->name, "progs/detpack.mdl") ||
		!strcmp(mod->name, "progs/detpack2.mdl") ||*/
/*		!strcmp(mod->name, "progs/dgib.mdl") ||
		!strcmp(mod->name, "progs/dgib2.mdl") ||
		!strcmp(mod->name, "progs/dgib3.mdl") ||*/
		!strcmp(mod->name, "progs/flare.mdl") ||
		!strcmp(mod->name, "progs/gren1.bsp") ||
		//!strcmp(mod->name, "progs/grenade.mdl") ||
		//!strcmp(mod->name, "progs/grenade2.mdl") ||
		!strcmp(mod->name, "progs/grenade3.mdl") ||
		!strcmp(mod->name, "progs/hook.mdl") ||
/*		!strcmp(mod->name, "progs/tesgib1.mdl") ||
		!strcmp(mod->name, "progs/tesgib2.mdl") ||
		!strcmp(mod->name, "progs/tesgib3.mdl") ||
		!strcmp(mod->name, "progs/tesgib4.mdl") ||
//		!strcmp(mod->name, "progs/turrgun.mdl") || //turrets have dodgy origins, after all
		!strcmp(mod->name, "progs/tgib1.mdl") ||
		!strcmp(mod->name, "progs/tgib2.mdl") ||
		!strcmp(mod->name, "progs/tgib3.mdl") ||*/
		!strcmp(mod->name, "progs/hgren2.mdl")) {
		mod->modhint_trail = MOD_TF_TRAIL;
	}

	//modhints
	if (!strcmp(mod->name, "progs/player.mdl")) {
		mod->modhint = MOD_PLAYER;
	}
	else if (!strcmp(mod->name, "progs/eyes.mdl")) {
		mod->modhint = MOD_EYES;
	}
	else if (!strcmp(mod->name, "fx_tele.mdl")) {
		//Tei, special
		mod->modhint = MOD_TELEPORTDESTINATION;
	}
	else if (!strcmp(mod->name, "progs/flame.mdl") || !strcmp(mod->name, "progs/flame2.mdl")) {
		mod->modhint = MOD_FLAME;
	}
	else if (!strcmp(mod->name, "progs/bolt.mdl") || !strcmp(mod->name, "progs/bolt2.mdl") || !strcmp(mod->name, "progs/bolt3.mdl")) {
		mod->modhint = MOD_THUNDERBOLT;
	}
	else if (!strcmp(mod->name, "progs/minimissile.mdl")) {
		mod->modhint = MOD_CLUSTER;
	}
	else if (!strcmp(mod->name, "progs/flag.mdl") ||
		!strcmp(mod->name, "progs/tf_flag.mdl") ||
		!strcmp(mod->name, "progs/kkr.mdl") ||
		!strcmp(mod->name, "progs/kkb") ||
		!strcmp(mod->name, "progs/w_g_key.mdl") ||
		!strcmp(mod->name, "progs/w_s_key.mdl") ||
		!strcmp(mod->name, "progs/b_g_key.mdl") ||
		!strcmp(mod->name, "progs/b_s_key.mdl") ||
		!strcmp(mod->name, "progs/tf_stan.mdl")) {
		mod->modhint = MOD_FLAG;
	}
	else if (!strcmp(mod->name, "progs/spike.mdl") ||
		!strcmp(mod->name, "progs/s_spike.mdl") ||
		!strcmp(mod->name, "progs/amf_spike.mdl")) {
		mod->modhint = MOD_SPIKE;
	}
	else if (!strcmp(mod->name, "progs/coil.mdl") || !strcmp(mod->name, "progs/tesla.mdl")) {
		mod->modhint = MOD_TESLA;
	}
	else if (!strcmp(mod->name, "progs/turrgun.mdl")) {
		mod->modhint = MOD_SENTRYGUN;
	}
	else if (!strcmp(mod->name, "progs/detpack.mdl")) {
		mod->modhint = MOD_DETPACK;
	}
	else if (!strcmp(mod->name, "progs/laser.mdl")) {
		mod->modhint = MOD_LASER;
	}
	else if (!strcmp(mod->name, "progs/demon.mdl")/* || !strcmp(mod->name, "progs/shambler.mdl") */) {
		mod->modhint = MOD_DEMON;
	}
	else if (!strcmp(mod->name, "progs/soldier.mdl")) {
		mod->modhint = MOD_SOLDIER;
	}
	else if (!strcmp(mod->name, "progs/enforcer.mdl")) {
		mod->modhint = MOD_ENFORCER;
	}
	else if (!strcmp(mod->name, "progs/ogre.mdl")) {
		mod->modhint = MOD_OGRE;
	}
	else if (!strcmp(mod->name, "progs/shambler.mdl")) {
		mod->modhint = MOD_SHAMBLER;
	}
	else if (!strcmp(mod->name, "progs/v_spike.mdl")) {
		mod->modhint = MOD_VOORSPIKE;
	}
	else if (!strcmp(mod->name, "progs/e_spike1.mdl")) {
		mod->modhint = MOD_RAIL;
	}
	else if (!strcmp(mod->name, "progs/e_spike2.mdl")) {
		mod->modhint = MOD_RAIL2;
	}
	else if (!strcmp(mod->name, "progs/lavaball.mdl")) {
		mod->modhint = MOD_LAVABALL;
	}
	else if (!strcmp(mod->name, "progs/dgib.mdl") ||
		!strcmp(mod->name, "progs/dgib2.mdl") ||
		!strcmp(mod->name, "progs/dgib3.mdl") ||
		!strcmp(mod->name, "progs/tesgib1.mdl") ||
		!strcmp(mod->name, "progs/tesgib2.mdl") ||
		!strcmp(mod->name, "progs/tesgib3.mdl") ||
		!strcmp(mod->name, "progs/tesgib4.mdl") ||
		!strcmp(mod->name, "progs/tgib1.mdl") ||
		!strcmp(mod->name, "progs/tgib2.mdl") ||
		!strcmp(mod->name, "progs/tgib3.mdl")) {
		mod->modhint = MOD_BUILDINGGIBS;
	}
	else if (!strcmp(mod->name, "progs/backpack.mdl")) {
		mod->modhint = MOD_BACKPACK;
	}
	else if (!strcmp(mod->name, "progs/gib1.mdl") ||
		!strcmp(mod->name, "progs/gib2.mdl") ||
		!strcmp(mod->name, "progs/gib3.mdl") ||
		!strcmp(mod->name, "progs/h_player.mdl")) {
		mod->modhint = MOD_GIB;
	}
	else if (!strncasecmp(mod->name, "progs/v_", 8)) {
		mod->modhint = MOD_VMODEL;
	}
	else if (!strcmp(mod->name, "progs/missile.mdl")) {
		mod->modhint = MOD_ROCKET;
	}
	else if (!strcmp(mod->name, "progs/grenade.mdl") ||
		!strcmp(mod->name, "progs/flare.mdl") ||
		!strcmp(mod->name, "progs/hgren2.mdl") ||
		!strcmp(mod->name, "progs/biggren.mdl") ||
		!strcmp(mod->name, "progs/grenade2.mdl") ||
		!strcmp(mod->name, "progs/grenade3.mdl") ||
		!strcmp(mod->name, "progs/caltrop.mdl")) {
		mod->modhint = MOD_GRENADE;
	}
	else if (!strcmp(mod->name, "progs/g_rock2.mdl")) {
		mod->modhint = MOD_ROCKETLAUNCHER;
	}
	else if (!strcmp(mod->name, "progs/g_light.mdl")) {
		mod->modhint = MOD_LIGHTNINGGUN;
	}
	else if (!strcmp(mod->name, "progs/quaddama.mdl")) {
		mod->modhint = MOD_QUAD;
	}
	else if (!strcmp(mod->name, "progs/invulner.mdl")) {
		mod->modhint = MOD_PENT;
	}
	else if (!strcmp(mod->name, "progs/invisibl.mdl")) {
		mod->modhint = MOD_RING;
	}
	else if (!strcmp(mod->name, "maps/b_bh100.bsp")) {
		mod->modhint = MOD_MEGAHEALTH;
	}
	else if (!strcmp(mod->name, "progs/armor.mdl")) {
		mod->modhint = MOD_ARMOR;
	}
	else {
		mod->modhint = MOD_NORMAL;
	}
}

static int simpleitem_textures[MOD_NUMBER_HINTS][MAX_SIMPLE_TEXTURES];

int Mod_LoadSimpleTexture(model_t *mod, int skinnum)
{
	int tex = 0, texmode = 0;
	char basename[64], indentifier[64];

	if (!mod) {
		return 0;
	}

	if (RuleSets_DisallowExternalTexture(mod)) {
		return 0;
	}

	if (RuleSets_DisallowSimpleTexture(mod)) {
		return 0;
	}

	COM_StripExtension(COM_SkipPath(mod->name), basename, sizeof(basename));

	texmode = TEX_MIPMAP | TEX_ALPHA;
	if (!gl_scaleModelTextures.value)
		texmode |= TEX_NOSCALE;

	snprintf(indentifier, sizeof(indentifier), "simple_%s_%d", basename, skinnum);

	if (developer.value > 1)
		Com_DPrintf("Mod_LoadSimpleTexture: %s ", indentifier);

	if (mod->type == mod_brush)
	{
		tex = GL_LoadTextureImage (va("textures/bmodels/%s", indentifier), indentifier, 0, 0, texmode);
	}
	else if (mod->type == mod_alias || mod->type == mod_alias3)
	{
		// hack for loading models saved as .bsp under /maps directory
		if (Utils_RegExpMatch("^(?i)maps\\/b_(.*)\\.bsp", mod->name))
		{
			tex = GL_LoadTextureImage (va("textures/bmodels/%s", indentifier), indentifier, 0, 0, texmode);
		}
		else
		{
			tex = GL_LoadTextureImage (va("textures/models/%s", indentifier), indentifier, 0, 0, texmode);
		}
	}

	if (!tex)
		tex = GL_LoadTextureImage (va("textures/%s", indentifier), indentifier, 0, 0, texmode);

	if (developer.value > 1)
		Com_DPrintf("%s\n", tex ? "OK" : "FAIL");

	if (mod->modhint >= 0 && mod->modhint < MOD_NUMBER_HINTS && skinnum >= 0 && skinnum < MAX_SIMPLE_TEXTURES) {
		simpleitem_textures[mod->modhint][skinnum] = tex;
	}

	return tex;
}

void Mod_ClearSimpleTextures(void)
{
	memset(simpleitem_textures, 0, sizeof(simpleitem_textures));
}

int Mod_SimpleTextureForHint(int model_hint, int skinnum)
{
	if (model_hint > 0 && model_hint < MOD_NUMBER_HINTS && skinnum >= 0 && skinnum < MAX_SIMPLE_TEXTURES) {
		return simpleitem_textures[model_hint][skinnum];
	}

	return 0;
}
