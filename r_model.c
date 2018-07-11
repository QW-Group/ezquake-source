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
#include "teamplay.h"
#include "rulesets.h"
#include "wad.h"
#include "crc.h"
#include "fmod.h"
#include "utils.h"
#include "r_texture.h"

model_t	*loadmodel;
char	loadname[32];	// for hunk tags

static mpic_t simpleitem_textures[MOD_NUMBER_HINTS][MAX_SIMPLE_TEXTURES];

void Mod_LoadSpriteModel(model_t *mod, void *buffer);
void Mod_LoadBrushModel(model_t *mod, void *buffer, int filesize);
void Mod_LoadAliasModel(model_t *mod, void *buffer, int filesize, const char* loadname);
model_t *Mod_LoadModel(model_t *mod, qbool crash);
void Mod_AddModelFlags(model_t *mod);

byte	mod_novis[MAX_MAP_LEAFS/8];

#define	MAX_MOD_KNOWN	512
model_t	mod_known[MAX_MOD_KNOWN];
int		mod_numknown;

void Mod_Init(void)
{
	memset(mod_novis, 0xff, sizeof(mod_novis));
}

//Caches the data if needed
void *Mod_Extradata(model_t *mod)
{
	void *r;

	r = Cache_Check(&mod->cache);
	if (r) {
		return r;
	}

	Mod_LoadModel(mod, true);

	if (!mod->cache.data) {
		Sys_Error("Mod_Extradata: caching failed");
	}
	return mod->cache.data;
}

mleaf_t *Mod_PointInLeaf(vec3_t p, model_t *model)
{
	mnode_t *node;
	float d;
	mplane_t *plane;

	if (!model || !model->nodes) {
		Sys_Error("Mod_PointInLeaf: bad model");
	}

	node = model->nodes;
	while (1) {
		if (node->contents < 0) {
			return (mleaf_t *)node;
		}
		plane = node->plane;
		d = PlaneDiff(p, plane);
		node = (d > 0) ? node->children[0] : node->children[1];
	}

	return NULL;	// never reached
}

byte *Mod_DecompressVis(byte *in, model_t *model)
{
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

byte *Mod_LeafPVS(mleaf_t *leaf, model_t *model)
{
	if (leaf == model->leafs) {
		return mod_novis;
	}
	return Mod_DecompressVis(leaf->compressed_vis, model);
}

void Mod_ClearAll(void)
{
	int i;
	model_t	*mod;

	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++) {
		if (mod->type != mod_alias && mod->type != mod_alias3 && mod->type != mod_sprite) {
			mod->needload = true;
		}
	}
}

model_t *Mod_FindName(char *name)
{
	int i;
	model_t	*mod;

	if (!name[0]) {
		Sys_Error("Mod_ForName: NULL name");
	}

	// search the currently loaded models
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++) {
		if (!strcmp(mod->name, name)) {
			break;
		}
	}

	if (i == mod_numknown) {
		if (mod_numknown == MAX_MOD_KNOWN) {
			Sys_Error("mod_numknown == MAX_MOD_KNOWN");
		}
		strlcpy(mod->name, name, sizeof(mod->name));
		mod->needload = true;
		mod_numknown++;
	}
	return mod;
}

void Mod_TouchModel(char *name)
{
	model_t	*mod;

	mod = Mod_FindName (name);
	if (!mod->needload)	{
		if (mod->type == mod_alias || mod->type == mod_alias3 || mod->type == mod_sprite) {
			Cache_Check(&mod->cache);
		}
	}
}

// this is callback from VID after a vid_restart
void Mod_TouchModels(void)
{
	int i;
	model_t *mod;

	if (cls.state != ca_active) {
		return; // seems we are not loaded models yet, so no need to do anything
	}

	for (i = 1; i < MAX_MODELS; i++) {
		if (!cl.model_name[i][0]) {
			break;
		}

		mod = cl.model_precache[i];

		if (mod->type == mod_alias || mod->type == mod_alias3 || mod->type == mod_sprite) {
			Mod_Extradata(mod);
		}
	}
}

void Mod_ReloadModels(qbool vid_restart)
{
	int i;

	for (i = 1; i < MAX_MODELS; ++i) {
		model_t* mod = cl.model_precache[i];

		if (mod && (mod == cl.worldmodel || !mod->isworldmodel)) {
			if (!vid_restart && (mod->type == mod_alias || mod->type == mod_alias3)) {
				if (mod->vertsInVBO && !mod->temp_vbo_buffer) {
					// Invalidate cache so VBO buffer gets refilled
					Cache_Free(&mod->cache);
				}
			}
			Mod_LoadModel(mod, true);
		}
	}

	for (i = 0; i < MAX_VWEP_MODELS; i++) {
		model_t* mod = cl.vw_model_precache[i];

		if (mod) {
			if (!vid_restart && (mod->type == mod_alias || mod->type == mod_alias3)) {
				if (mod->vertsInVBO && !mod->temp_vbo_buffer) {
					// Invalidate cache so VBO buffer gets refilled
					Cache_Free(&mod->cache);
				}
			}
			Mod_LoadModel(mod, true);
		}
	}
}

//Loads a model into the cache
model_t *Mod_LoadModel(model_t *mod, qbool crash)
{
	void *d;
	unsigned *buf;
	int namelen;
	int filesize;

	if (!mod->needload) {
		if (mod->type == mod_alias || mod->type == mod_alias3 || mod->type == mod_sprite) {
			d = Cache_Check(&mod->cache);
			if (d) {
				return mod;
			}
		}
		else {
			return mod; // not cached at all
		}
	}

	namelen = strlen(mod->name);
	buf = NULL;
	if (namelen >= 4 && (!strcmp(mod->name + namelen - 4, ".mdl") ||
		(namelen >= 9 && mod->name[5] == 'b' && mod->name[6] == '_' && !strcmp(mod->name + namelen - 4, ".bsp")))) {
		char newname[MAX_QPATH];
		COM_StripExtension(mod->name, newname, sizeof(newname));
		COM_DefaultExtension(newname, ".md3");
		buf = (unsigned *)FS_LoadTempFile(newname, &filesize);
	}

	// load the file
	if (!buf) {
		buf = (unsigned *)FS_LoadTempFile(mod->name, &filesize);
	}
	if (!buf) {
		if (crash) {
			Host_Error("Mod_LoadModel: %s not found", mod->name);
		}
		return NULL;
	}

	// allocate a new model
	COM_FileBase(mod->name, loadname);
	loadmodel = mod;
	FMod_CheckModel(mod->name, buf, filesize);

	// call the apropriate loader
	mod->needload = false;

	switch (LittleLong(*((unsigned *)buf))) {
	case IDPOLYHEADER:
		Mod_LoadAliasModel(mod, buf, filesize, loadname);
		break;

	case MD3_IDENT:
		Mod_LoadAlias3Model(mod, buf, filesize);
		break;

	case IDSPRITEHEADER:
		Mod_LoadSpriteModel(mod, buf);
		break;

	default:
		Mod_LoadBrushModel(mod, buf, filesize);
		break;
	}

	return mod;
}

//Loads in a model for the given name
model_t *Mod_ForName(char *name, qbool crash)
{
	model_t	*mod = Mod_FindName(name);

	return Mod_LoadModel(mod, crash);
}

qbool Img_HasFullbrights(byte *pixels, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		if (pixels[i] >= 224) {
			return true;
		}
	}

	return false;
}

// this is callback from VID after a vid_restart
void Mod_ReloadModelsTextures(void)
{
	int i, j;
	model_t *m;
	texture_t *tx;

	if (cls.state != ca_active) {
		return; // seems we are not loaded models yet, so no need to reload textures
	}

	// mark all textures as not loaded, for brush models only, this speed up loading.
	// seems textures shared between models, so we mark textures from ALL models than actually load textures, that why we use two for()
	for (i = 1; i < MAX_MODELS; i++)
	{
		if (!cl.model_name[i][0]) {
			break;
		}

		m = cl.model_precache[i];
		if (m->type != mod_brush) {
			continue; // actual only for brush models
		}

		if (!m->textures) {
			continue; // hmm
		}

		for (j = 0; j < m->numtextures; j++) {
			tx = m->textures[j];
			if (!tx) {
				continue;
			}

			tx->loaded = false; // so texture will be reloaded
			GL_TextureReferenceInvalidate(tx->gl_texture_array);
		}
	}

	// now actually load textures for brush models
	for (i = 1; i < MAX_MODELS; i++)
	{
		if (!cl.model_name[i][0]) {
			break;
		}

		m = cl.model_precache[i];
		if (m->type != mod_brush) {
			continue; // actual only for brush models
		}

		R_LoadBrushModelTextures(m);
	}
}

float RadiusFromBounds (vec3_t mins, vec3_t maxs)
{
	int i;
	vec3_t corner;

	for (i = 0; i < 3; i++)
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);

	return VectorLength (corner);
}

//============================================================

//=========================================================

//VULT MODELS
//This is incase we want to use a function other than Mod_LoadAliasModel
//It was used in one of the older versions when it supported Q2 Models.
void Mod_AddModelFlags(model_t *mod)
{
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
	else if (!strcmp(mod->name, "progs/flame.mdl")) {
		mod->modhint = MOD_FLAME;
	}
	else if (!strcmp(mod->name, "progs/flame2.mdl")) {
		mod->modhint = MOD_FLAME2;
	}
	else if (!strcmp(mod->name, "progs/flame0.mdl")) {
		mod->modhint = MOD_FLAME0;
	}
	else if (!strcmp(mod->name, "progs/flame3.mdl")) {
		mod->modhint = MOD_FLAME3;
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
	else if (!strcmp(mod->name, "progs/caltrop.mdl") ||
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
		mod->modhint = MOD_TF_TRAIL;
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
	else if (!strcmp(mod->name, "progs/grenade.mdl")) {
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
	else if (!strcmp(mod->name, "progs/suit.mdl")) {
		mod->modhint = MOD_SUIT;
	}
	else if (!strcmp(mod->name, "progs/g_rock.mdl")) {
		mod->modhint = MOD_GRENADELAUNCHER;
	}
	else {
		mod->modhint = MOD_NORMAL;
	}
}

texture_ref Mod_LoadSimpleTexture(model_t *mod, int skinnum)
{
	texture_ref tex = null_texture_reference;
	int texmode = 0;
	char basename[64], indentifier[64];

	if (!mod) {
		return null_texture_reference;
	}

	if (RuleSets_DisallowExternalTexture(mod)) {
		return null_texture_reference;
	}

	// well, it have nothing with luma, but quite same restrictions...
	if ((mod->modhint != MOD_BACKPACK) && !Ruleset_IsLumaAllowed(mod)) {
		return null_texture_reference;
	}

	COM_StripExtension(COM_SkipPath(mod->name), basename, sizeof(basename));

	texmode = TEX_MIPMAP | TEX_ALPHA | TEX_PREMUL_ALPHA;
	if (!gl_scaleModelTextures.value) {
		texmode |= TEX_NOSCALE;
	}

	snprintf(indentifier, sizeof(indentifier), "simple_%s_%d", basename, skinnum);

	if (developer.value > 1) {
		Com_DPrintf("Mod_LoadSimpleTexture: %s ", indentifier);
	}

	if (mod->type == mod_brush) {
		tex = R_LoadTextureImage(va("textures/bmodels/%s", indentifier), indentifier, 0, 0, texmode);
	}
	else if (mod->type == mod_alias || mod->type == mod_alias3) {
		// hack for loading models saved as .bsp under /maps directory
		if (Utils_RegExpMatch("^(?i)maps\\/b_(.*)\\.bsp", mod->name)) {
			tex = R_LoadTextureImage(va("textures/bmodels/%s", indentifier), indentifier, 0, 0, texmode);
		}
		else {
			tex = R_LoadTextureImage(va("textures/models/%s", indentifier), indentifier, 0, 0, texmode);
		}
	}

	if (!GL_TextureReferenceIsValid(tex)) {
		tex = R_LoadTextureImage(va("textures/%s", indentifier), indentifier, 0, 0, texmode);
	}

	if (developer.value > 1) {
		Com_DPrintf("%s\n", GL_TextureReferenceIsValid(tex) ? "OK" : "FAIL");
	}

	if (mod->modhint >= 0 && mod->modhint < MOD_NUMBER_HINTS && skinnum >= 0 && skinnum < MAX_SIMPLE_TEXTURES) {
		mpic_t* pic = &simpleitem_textures[mod->modhint][skinnum];
		pic->texnum = tex;
		pic->width = 16;
		pic->height = 16;
		pic->sl = pic->tl = 0;
		pic->sh = pic->th = 1;

		CachePics_MarkAtlasDirty();
	}

	return tex;
}

void Mod_ClearSimpleTextures(void)
{
	memset(simpleitem_textures, 0, sizeof(simpleitem_textures));
}

mpic_t* Mod_SimpleTextureForHint(int model_hint, int skinnum)
{
	if (model_hint > 0 && model_hint < MOD_NUMBER_HINTS && skinnum >= 0 && skinnum < MAX_SIMPLE_TEXTURES) {
		return &simpleitem_textures[model_hint][skinnum];
	}

	return 0;
}

model_t* Mod_CustomModel(custom_model_id_t id, qbool crash)
{
	switch (id) {
		case custom_model_explosion:
			return Mod_ForName("progs/s_explod.spr", crash);
		case custom_model_bolt:
			return Mod_ForName("progs/bolt.mdl", crash);
		case custom_model_bolt2:
			return Mod_ForName("progs/bolt2.mdl", crash);
		case custom_model_bolt3:
			return Mod_ForName("progs/bolt3.mdl", crash);
		case custom_model_beam:
			return Mod_ForName("progs/beam.mdl", crash);
		case custom_model_flame0:
			return (cl_flame0_model = Mod_ForName("progs/flame0.mdl", crash));
		default:
			return NULL;
	}
}
