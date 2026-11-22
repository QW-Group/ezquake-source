/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2016      Spike

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

// Scripted particle effects: cvars and commands, lifecycle, the effect type
// registry and its lookup, set loading and the effect textures.

#include <limits.h>

#include "quakedef.h"
#include "gl_model.h"
#include "rulesets.h"
#include "crc.h"
#include "r_texture.h"
#include "r_renderer.h"
#include "r_part_script_local.h"

// Longer r_partredirect chains resolve to no effect, which also ends loops.
#define PSCRIPT_MAX_REDIRECT_HOPS 5

// Every effect name lookup walks the redirect list once per hop.
#define PSCRIPT_MAX_REDIRECTS 1024

// Every refresh matches each known model against all associations.
#define PSCRIPT_MAX_ASSOCS 1024

// Creating a type compares its name with every existing one, so loading
// grows quadratically with the number of types.
#define PSCRIPT_MAX_TYPES 4096

// Each image, premul mode and filter takes a texture slot until vid_restart,
// and the renderer calls Sys_Error when MAX_GLTEXTURES run out.
#define PSCRIPT_MAX_IMAGES 256

typedef struct pscript_set_s {
	struct pscript_set_s *next;
	char name[MAX_QPATH];
	qbool found;
} pscript_set_t;

typedef struct pscript_redirect_s {
	struct pscript_redirect_s *next;
	char *from;
	char *to;
	qbool fromset; // made by a particle set, not the console
} pscript_redirect_t;

typedef struct pscript_assoc_s {
	struct pscript_assoc_s *next;
	char model[MAX_QPATH];
	char effect[MAX_QPATH];
	unsigned int flags; // PSCRIPT_EMIT_*, for emitters
	qbool trail;
} pscript_assoc_t;

typedef enum {
	PSCRIPT_TEX_WHITE,
	PSCRIPT_TEX_BEAM,
	PSCRIPT_TEX_TRIFAN,
	PSCRIPT_TEX_CLASSIC,
	PSCRIPT_TEX_BALL,
	PSCRIPT_TEX_FUZZY,
	PSCRIPT_TEX_COUNT
} pscript_builtin_t;

pscript_type_t *pscript_types;
int pscript_numtypes;
pscript_type_t *pscript_runlist;
qbool pscript_effects_dirty;
int pscript_pe_default = P_INVALID;
int pscript_pe_size2 = P_INVALID;
int pscript_pe_size3 = P_INVALID;
int pscript_pe_colors[256];
float pscript_frametime;
int pscript_hostframe = 1;
int pscript_refreshcount;
qbool pscript_expsprite = true;

static int pscript_typecapacity;
static qbool pscript_sets_dirty;
qbool pscript_anyloaded;
static qbool pscript_active;
static qbool pscript_restricted; // as the last PScript_BeginFrame found it
static double pscript_last;
// The host frame's time is still to be simulated: by the first view, so new
// particles move before they are drawn, or at the end of a frame without one.
static qbool pscript_sim_pending;
static pscript_set_t *pscript_sets;
static pscript_redirect_t *pscript_redirects;
static pscript_assoc_t *pscript_assocs;
static int pscript_effectids[PSCRIPT_EFFECT_COUNT];
static texture_ref pscript_builtin_textures[PSCRIPT_TEX_COUNT];
// Identifiers of the effect images that took a texture slot since
// PScript_Init last reloaded the textures.
static char pscript_images[PSCRIPT_MAX_IMAGES][MAX_QPATH];
static int pscript_numimages;

// The images types loaded lately, misses included. Many types share one
// particle font, and R_LoadTextureImage reads and converts the file on every
// call.
#define PSCRIPT_IMAGE_CACHE 16
static struct {
	char identifier[MAX_QPATH];
	texture_ref texture;
} pscript_image_cache[PSCRIPT_IMAGE_CACHE];
static int pscript_image_cache_next;

static const struct {
	const char *name;
	const char *fallback;
} pscript_effectnames[PSCRIPT_EFFECT_COUNT] = {
	[PSCRIPT_TE_SPIKE] = { "TE_SPIKE" },
	[PSCRIPT_TE_SUPERSPIKE] = { "TE_SUPERSPIKE" },
	[PSCRIPT_TE_BULLET] = { "TE_BULLET" },
	[PSCRIPT_TE_SUPERBULLET] = { "TE_SUPERBULLET" },
	[PSCRIPT_TE_GUNSHOT] = { "TE_GUNSHOT" },
	[PSCRIPT_TE_QWGUNSHOT] = { "TE_GUNSHOT", "TE_QWGUNSHOT" },
	[PSCRIPT_TE_BLOOD] = { "TE_BLOOD" },
	[PSCRIPT_TE_QWBLOOD] = { "TE_BLOOD", "TE_QWBLOOD" },
	[PSCRIPT_TE_LIGHTNINGBLOOD] = { "TE_LIGHTNINGBLOOD" },
	[PSCRIPT_TE_EXPLOSION] = { "TE_EXPLOSION" },
	[PSCRIPT_TE_TAREXPLOSION] = { "TE_TAREXPLOSION" },
	[PSCRIPT_TE_WIZSPIKE] = { "TE_WIZSPIKE" },
	[PSCRIPT_TE_KNIGHTSPIKE] = { "TE_KNIGHTSPIKE" },
	[PSCRIPT_TE_LAVASPLASH] = { "TE_LAVASPLASH" },
	[PSCRIPT_TE_TELEPORT] = { "TE_TELEPORT" },
	[PSCRIPT_TE_RAILTRAIL] = { "TE_RAILTRAIL" },
	[PSCRIPT_TE_MUZZLEFLASH] = { "TE_MUZZLEFLASH" },
	[PSCRIPT_TE_EXPLOSION2] = { "TE_EXPLOSION2" },
	[PSCRIPT_TE_GUNSHOTQUAD] = { "TE_GUNSHOTQUAD" },
	[PSCRIPT_TE_SPIKEQUAD] = { "TE_SPIKEQUAD" },
	[PSCRIPT_TE_SUPERSPIKEQUAD] = { "TE_SUPERSPIKEQUAD" },
	[PSCRIPT_TE_EXPLOSIONQUAD] = { "TE_EXPLOSIONQUAD" },
	[PSCRIPT_TE_SPARK] = { "TE_SPARK" },
	[PSCRIPT_TE_SMALLFLASH] = { "TE_SMALLFLASH" },
	[PSCRIPT_TE_FLAMEJET] = { "TE_FLAMEJET" },
	[PSCRIPT_TE_PLASMABURN] = { "TE_PLASMABURN" },
	// r_part configs name this te_nexbeam; DarkPlaces effectinfo calls it TE_TEI_G3.
	[PSCRIPT_TE_TEI_G3] = { "te_nexbeam", "TE_TEI_G3" },
	[PSCRIPT_TE_TEI_SMOKE] = { "TE_TEI_SMOKE" },
	[PSCRIPT_TE_TEI_BIGEXPLOSION] = { "TE_TEI_BIGEXPLOSION" },
	[PSCRIPT_TE_TEI_PLASMAHIT] = { "TE_TEI_PLASMAHIT" },
	[PSCRIPT_TR_ROCKET] = { "TR_ROCKET" },
	[PSCRIPT_TR_GRENADE] = { "TR_GRENADE" },
	[PSCRIPT_TR_BLOOD] = { "TR_BLOOD" },
	[PSCRIPT_TR_SLIGHTBLOOD] = { "TR_SLIGHTBLOOD" },
	[PSCRIPT_TR_WIZSPIKE] = { "TR_WIZSPIKE" },
	[PSCRIPT_TR_KNIGHTSPIKE] = { "TR_KNIGHTSPIKE" },
	[PSCRIPT_TR_VORESPIKE] = { "TR_VORESPIKE" },
	[PSCRIPT_TE_LIGHTNING1] = { "TE_LIGHTNING1" },
	[PSCRIPT_TE_LIGHTNING1_END] = { "TE_LIGHTNING1_END" },
	[PSCRIPT_TE_LIGHTNING2] = { "TE_LIGHTNING2" },
	[PSCRIPT_TE_LIGHTNING2_END] = { "TE_LIGHTNING2_END" },
	[PSCRIPT_TE_LIGHTNING3] = { "TE_LIGHTNING3" },
	[PSCRIPT_TE_LIGHTNING3_END] = { "TE_LIGHTNING3_END" },
	[PSCRIPT_TE_BEAM] = { "te_beam" },
	[PSCRIPT_TE_BEAM_END] = { "te_beam_end" },
};

// Names older configs give effects that the engine spawns by a newer name.
static const struct {
	const char *oldname;
	const char *newname;
} pscript_legacynames[] = {
	{ "t_rocket", "TR_ROCKET" },
	{ "t_grenade", "TR_GRENADE" },
	{ "t_gib", "TR_BLOOD" },
	{ "te_plasma", "TE_TEI_PLASMAHIT" },
	{ "te_smoke", "TE_TEI_SMOKE" },
	{ NULL, NULL }
};

// OnChange runs before the new value is stored, so the next PScript_BeginFrame
// reloads the sets from the stored value.
static void PScript_OnChangeParticleDesc(cvar_t *var, char *value, qbool *cancel)
{
	pscript_sets_dirty = true;
}

// Surface emitters are only built while r_part_rain is on.
static void PScript_OnChangeRain(cvar_t *var, char *value, qbool *cancel)
{
	pscript_effects_dirty = true;
}

static cvar_t r_particledesc = { "r_particledesc", "", CVAR_NOTFROMSERVER, PScript_OnChangeParticleDesc };
cvar_t r_part_rain = { "r_part_rain", "1", 0, PScript_OnChangeRain };
cvar_t r_part_rain_quantity = { "r_part_rain_quantity", "1" };
cvar_t r_part_rain_prewarm = { "r_part_rain_prewarm", "1" };
cvar_t r_bouncysparks = { "r_bouncysparks", "1" };
cvar_t r_decal_noperpendicular = { "r_decal_noperpendicular", "1" };
cvar_t r_particle_tracelimit = { "r_particle_tracelimit", "65536" };
cvar_t r_part_sparks = { "r_part_sparks", "1" };
cvar_t r_part_sparks_trifan = { "r_part_sparks_trifan", "1" };
cvar_t r_part_sparks_textured = { "r_part_sparks_textured", "1" };
cvar_t r_part_beams = { "r_part_beams", "1" };
cvar_t r_part_contentswitch = { "r_part_contentswitch", "1" };
cvar_t r_part_density = { "r_part_density", "1" };
cvar_t r_part_maxparticles = { "r_part_maxparticles", "65536" };
cvar_t r_part_maxdecals = { "r_part_maxdecals", "8192" };

static const char *PScript_LegacyName(const char *name)
{
	int i;

	for (i = 0; pscript_legacynames[i].oldname; i++) {
		if (!strcmp(name, pscript_legacynames[i].oldname)) {
			return pscript_legacynames[i].newname;
		}
	}
	return name;
}

// A name that can only be read as particles/<name>.cfg. Set names taken from
// maps and effect lookups must pass this.
static qbool PScript_SafeSetName(const char *name)
{
	const char *c;

	if (!name[0] || strlen(name) >= MAX_QPATH) {
		return false;
	}
	for (c = name; *c; c++) {
		if (!(*c >= 'a' && *c <= 'z') && !(*c >= 'A' && *c <= 'Z') && !(*c >= '0' && *c <= '9') && *c != '_' && *c != '-') {
			return false;
		}
	}
	return true;
}

// Moves the types to a block of twice the capacity. The run list points into
// the array, so its links are rebased while the old block is still valid.
static void PScript_GrowTypes(void)
{
	int capacity = pscript_typecapacity ? pscript_typecapacity * 2 : 64;
	pscript_type_t *types = Q_calloc(capacity, sizeof(*types));
	pscript_type_t *old = pscript_types;
	pscript_type_t **link;
	int i;

	if (old) {
		memcpy(types, old, pscript_numtypes * sizeof(*types));
	}
	for (i = 0; i < pscript_numtypes; i++) {
		if (types[i].nexttorun) {
			types[i].nexttorun = types + (types[i].nexttorun - old);
		}
		types[i].runlink = NULL;
	}
	if (pscript_runlist) {
		pscript_runlist = types + (pscript_runlist - old);
	}
	for (link = &pscript_runlist, i = 0; *link && i < pscript_numtypes; link = &(*link)->nexttorun, i++) {
		(*link)->runlink = link;
	}

	Q_free(old);
	pscript_types = types;
	pscript_typecapacity = capacity;
}

// Returns the type with this config and name, creating an unloaded one if
// needed, or NULL when PSCRIPT_MAX_TYPES are in use. A "ns." prefix of name
// replaces config. Creating a type can move the array, which invalidates
// every pointer to a type, including ones passed in here.
pscript_type_t *PScript_GetParticleType(const char *config, const char *name)
{
	char cfgbuf[MAX_QPATH];
	char namebuf[MAX_QPATH];
	const char *dot = strchr(name, '.');
	pscript_type_t *ptype;
	int i;

	if (dot && dot - name < MAX_QPATH - 1) {
		memcpy(cfgbuf, name, dot - name);
		cfgbuf[dot - name] = 0;
		name = dot + 1;
	}
	else {
		strlcpy(cfgbuf, config, sizeof(cfgbuf));
	}
	strlcpy(namebuf, PScript_LegacyName(name), sizeof(namebuf));

	for (i = 0; i < pscript_numtypes; i++) {
		ptype = &pscript_types[i];
		if (!strcasecmp(ptype->name, namebuf) && !strcasecmp(ptype->config, cfgbuf)) {
			return ptype;
		}
	}

	if (pscript_numtypes == PSCRIPT_MAX_TYPES) {
		Con_DPrintf("Particle effect %s.%s ignored: more than %d effects\n", cfgbuf, namebuf, PSCRIPT_MAX_TYPES);
		return NULL;
	}
	if (pscript_numtypes == pscript_typecapacity) {
		PScript_GrowTypes();
	}
	ptype = &pscript_types[pscript_numtypes++];
	memset(ptype, 0, sizeof(*ptype));
	strlcpy(ptype->name, namebuf, sizeof(ptype->name));
	strlcpy(ptype->config, cfgbuf, sizeof(ptype->config));
	ptype->assoc = P_INVALID;
	ptype->inwater = P_INVALID;
	ptype->cliptype = P_INVALID;
	ptype->emit = P_INVALID;
	if (pscript_numtypes == PSCRIPT_MAX_TYPES) {
		Con_Printf("Particle effects: %d effect types in use, further ones are ignored\n", PSCRIPT_MAX_TYPES);
	}

	pscript_effects_dirty = true;
	return ptype;
}

// The index of PScript_GetParticleType(config, name), with its caveats, or
// P_INVALID when it returns NULL.
int PScript_AllocateParticleType(const char *config, const char *name)
{
	pscript_type_t *ptype = PScript_GetParticleType(config, name);

	return ptype ? (int)(ptype - pscript_types) : P_INVALID;
}

// Adds ptype at the head of the run list unless it is in it. A walk of the
// list that is under way does not visit a type added this way.
void PScript_LinkRunList(pscript_type_t *ptype)
{
	if (ptype->state & PS_INRUNLIST) {
		return;
	}
	ptype->runlink = &pscript_runlist;
	ptype->nexttorun = pscript_runlist;
	if (pscript_runlist) {
		pscript_runlist->runlink = &ptype->nexttorun;
	}
	pscript_runlist = ptype;
	ptype->state |= PS_INRUNLIST;
}

// Removes ptype from the run list if it is in it. Keeps nexttorun, so a walk
// standing on ptype can continue with ptype->nexttorun.
void PScript_UnlinkRunList(pscript_type_t *ptype)
{
	if (!(ptype->state & PS_INRUNLIST)) {
		return;
	}
	*ptype->runlink = ptype->nexttorun;
	if (ptype->nexttorun) {
		ptype->nexttorun->runlink = ptype->runlink;
	}
	ptype->runlink = NULL;
	ptype->state &= ~PS_INRUNLIST;
}

static void PScript_GenerateTexture(pscript_builtin_t kind, byte *data, int size)
{
	static const byte fuzzy[16][16] = {
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0 },
		{ 0, 0, 0, 1, 1, 1, 1, 1, 3, 1, 1, 2, 1, 0, 0, 0 },
		{ 0, 0, 0, 1, 1, 1, 1, 4, 4, 4, 5, 4, 2, 1, 1, 0 },
		{ 0, 0, 1, 1, 6, 5, 5, 8, 6, 8, 3, 6, 3, 2, 1, 0 },
		{ 0, 0, 1, 5, 6, 7, 5, 6, 8, 8, 8, 3, 3, 1, 0, 0 },
		{ 0, 0, 0, 1, 6, 8, 9, 9, 9, 9, 4, 6, 3, 1, 0, 0 },
		{ 0, 0, 2, 1, 7, 7, 9, 9, 9, 9, 5, 3, 1, 0, 0, 0 },
		{ 0, 0, 2, 4, 6, 8, 9, 9, 9, 9, 8, 6, 1, 0, 0, 0 },
		{ 0, 0, 2, 2, 3, 5, 6, 8, 9, 8, 8, 4, 4, 1, 0, 0 },
		{ 0, 0, 1, 2, 4, 1, 8, 7, 8, 8, 6, 5, 4, 1, 0, 0 },
		{ 0, 1, 1, 1, 7, 8, 1, 6, 7, 5, 4, 7, 1, 0, 0, 0 },
		{ 0, 1, 2, 1, 1, 5, 1, 3, 4, 3, 1, 1, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	};
	float half = size * 0.5f - 1;
	float dx, dy;
	byte *p;
	int x, y, r;

	for (y = 0; y < size; y++) {
		for (x = 0; x < size; x++) {
			p = data + (y * size + x) * 4;
			p[0] = p[1] = p[2] = p[3] = 255;
			switch (kind) {
				case PSCRIPT_TEX_WHITE:
					break;
				case PSCRIPT_TEX_BEAM:
					dy = (y - size * 0.5f) / half;
					p[3] = bound(0, 256 * (1 - dy * dy), 255);
					break;
				case PSCRIPT_TEX_TRIFAN:
					dx = x / half;
					dy = y / half;
					p[0] = p[1] = p[2] = bound(0, 256 * (1 - (dx + dy)), 255);
					p[3] = p[0] / 2;
					break;
				case PSCRIPT_TEX_CLASSIC:
					// An antialiased circle filling the top left quarter.
					r = min((x - 16) * (x - 16) + (y - 16) * (y - 16), 255);
					p[3] = min(8 * (255 - r), 255);
					break;
				case PSCRIPT_TEX_BALL:
					dx = (x - size * 0.5f) / half;
					dy = (y - size * 0.5f) / half;
					p[3] = bound(0, 256 * (1 - (dx * dx + dy * dy)), 255);
					break;
				default:
					p[3] = fuzzy[x & 15][y & 15] * 255 / 9;
					break;
			}
		}
	}
}

// Created on first use, with straight alpha for every look: unlike file
// textures they are never premultiplied, so a premultiplied look draws the
// whole quad at its colour times alpha.
static texture_ref PScript_BuiltinTexture(pscript_builtin_t kind)
{
	static const char *const names[PSCRIPT_TEX_COUNT] = { "white", "beam", "trifan", "classic", "ball", "fuzzy" };
	static const int sizes[PSCRIPT_TEX_COUNT] = { 4, 64, 64, 64, 64, 16 };
	texture_ref *texture = &pscript_builtin_textures[kind];
	char identifier[MAX_QPATH];
	int size = sizes[kind];
	byte *data;

	if (R_TextureReferenceIsValid(*texture)) {
		return *texture;
	}

	data = Q_malloc(size * size * 4);
	PScript_GenerateTexture(kind, data, size);
	snprintf(identifier, sizeof(identifier), "pscript:%s", names[kind]);
	*texture = R_LoadTexture(identifier, size, size, data, TEX_ALPHA | TEX_NOSCALE, 4);
	Q_free(data);
	return *texture;
}

// Loads the image at path unless it would take a slot beyond
// PSCRIPT_MAX_IMAGES. The path's checksum in the identifier keeps apart paths
// that differ only beyond the identifier's length.
static texture_ref PScript_LoadImage(const char *path, int premul, qbool nearest)
{
	char identifier[MAX_QPATH];
	int mode = TEX_ALPHA | TEX_NOSCALE;
	texture_ref texture;
	qbool known = false;
	int i;

	if (premul) {
		mode |= TEX_PREMUL_ALPHA;
	}
	if (premul == 2) {
		mode |= TEX_ZERO_ALPHA;
	}
	if (nearest) {
		mode |= TEX_NO_TEXTUREMODE;
	}
	snprintf(identifier, sizeof(identifier), "pscript%d%d:%04x:%s", premul, nearest ? 1 : 0, (unsigned int)CRC_Block((byte *)path, strlen(path)), path);

	for (i = 0; i < PSCRIPT_IMAGE_CACHE; i++) {
		if (!strcmp(pscript_image_cache[i].identifier, identifier)) {
			return pscript_image_cache[i].texture;
		}
	}
	for (i = 0; i < pscript_numimages && !known; i++) {
		known = !strcmp(pscript_images[i], identifier);
	}
	if (!known && pscript_numimages == PSCRIPT_MAX_IMAGES) {
		return null_texture_reference;
	}

	texture = R_LoadTextureImage(path, identifier, 0, 0, mode);
	if (R_TextureReferenceIsValid(texture)) {
		if (nearest) {
			renderer.TextureSetFiltering(texture, texture_minification_nearest, texture_magnification_nearest);
		}
		if (!known) {
			strlcpy(pscript_images[pscript_numimages++], identifier, sizeof(pscript_images[0]));
			if (pscript_numimages == PSCRIPT_MAX_IMAGES) {
				Con_Printf("Particle effects: %d images in use, further ones are not loaded\n", PSCRIPT_MAX_IMAGES);
			}
		}
	}
	strlcpy(pscript_image_cache[pscript_image_cache_next].identifier, identifier, sizeof(pscript_image_cache[0].identifier));
	pscript_image_cache[pscript_image_cache_next].texture = texture;
	pscript_image_cache_next = (pscript_image_cache_next + 1) % PSCRIPT_IMAGE_CACHE;
	return texture;
}

// Forgets the cached images: their references after a renderer restart, and
// the files another set or game directory may now provide.
static void PScript_FlushImageCache(void)
{
	memset(pscript_image_cache, 0, sizeof(pscript_image_cache));
	pscript_image_cache_next = 0;
}

// Loads <texname>, else textures/<texname>, else a generated texture that
// suits the look, with texture coordinates reset to cover it.
// particles/particlefont is not looked for under textures/: ezQuake's own
// textures/particles/particlefont is the QMB atlas, not the DarkPlaces font.
void PScript_LoadTexture(pscript_type_t *ptype)
{
	char path[MAX_QPATH + 16];
	char base[MAX_QPATH];
	int premul = bound(0, ptype->looks.premul, 2);
	pscript_builtin_t kind;

	if (ptype->texname[0]) {
		ptype->looks.texture = PScript_LoadImage(ptype->texname, premul, ptype->looks.nearest);
		COM_StripExtension(ptype->texname, base, sizeof(base));
		if (!R_TextureReferenceIsValid(ptype->looks.texture) && strcasecmp(base, "particles/particlefont")) {
			snprintf(path, sizeof(path), "textures/%s", ptype->texname);
			ptype->looks.texture = PScript_LoadImage(path, premul, ptype->looks.nearest);
		}
		if (R_TextureReferenceIsValid(ptype->looks.texture)) {
			return;
		}
	}

	ptype->s1 = 0;
	ptype->t1 = 0;
	ptype->s2 = 1;
	ptype->t2 = 1;
	ptype->randsmax = 1;
	if (ptype->looks.type == PT_SPARK) {
		kind = PSCRIPT_TEX_WHITE;
	}
	else if (ptype->looks.type == PT_BEAM) {
		kind = PSCRIPT_TEX_BEAM;
	}
	else if (ptype->looks.type == PT_SPARKFAN) {
		kind = PSCRIPT_TEX_TRIFAN;
	}
	else if (strstr(ptype->texname, "classicparticle")) {
		kind = PSCRIPT_TEX_CLASSIC;
		ptype->s2 = 0.5;
		ptype->t2 = 0.5;
	}
	else if (strstr(ptype->texname, "glow") || strstr(ptype->texname, "ball") || ptype->looks.type == PT_TEXTUREDSPARK) {
		kind = PSCRIPT_TEX_BALL;
	}
	else {
		kind = PSCRIPT_TEX_FUZZY;
	}
	ptype->looks.texture = PScript_BuiltinTexture(kind);
}

// Parses particles/<name>.cfg ("./name" reads <name>.cfg in the game
// directory), or imports an explicit effectinfo* set. Each name is read at
// most once until the sets unload. Implicit sets are weak: they don't replace
// effects an explicit set defined. Returns true when a file was read; may
// move the type array.
static qbool PScript_LoadSet(const char *name, qbool implicit, qbool warn)
{
	pscript_set_t **link;
	pscript_set_t *set;
	char path[MAX_QPATH + 16];
	byte *data;
	int size = 0;

	if (!name[0]) {
		return false;
	}
	if (strlen(name) >= MAX_QPATH) {
		if (warn) {
			Con_Printf("Particle set name too long: %s\n", name);
		}
		return false;
	}
	for (link = &pscript_sets; *link; link = &(*link)->next) {
		if (!strcasecmp((*link)->name, name)) {
			return false;
		}
	}

	// Parsing gets the recorded copy: tokenizing the file clobbers com_token,
	// which name may be.
	set = Q_calloc(1, sizeof(*set));
	strlcpy(set->name, name, sizeof(set->name));
	*link = set;

	if (!strncmp(set->name, "./", 2)) {
		snprintf(path, sizeof(path), "%s.cfg", set->name + 2);
	}
	else {
		snprintf(path, sizeof(path), "particles/%s.cfg", set->name);
	}
	data = FS_LoadHeapFile(path, &size);
	if (!data && strchr(set->name, '/')) {
		snprintf(path, sizeof(path), "%s.cfg", set->name);
		data = FS_LoadHeapFile(path, &size);
	}

	if (data) {
		set->found = true;
		PScript_ParseParticleEffectFile(set->name, implicit, (const char *)data, max(size, 0));
		Q_free(data);
		return true;
	}

	// The importer reports a missing or unreadable file itself.
	if (!implicit && (!strcmp(set->name, "effectinfo") || !strncmp(set->name, "effectinfo_", 11))) {
		set->found = PScript_ImportEffectInfo(set->name);
		return set->found;
	}

	if (warn) {
		Con_Printf("Couldn't find particle description %s\n", set->name);
	}
	return false;
}

static void PScript_FreeAssocs(void)
{
	pscript_assoc_t *a;

	while (pscript_assocs) {
		a = pscript_assocs;
		pscript_assocs = a->next;
		Q_free(a);
	}
}

// Marks every type unloaded and forgets the sets and what they made
// (redirects, associations, explosion sprite choice), so a map's set does
// not carry over to the maps after it.
// Live instances keep using their type, so a ramp is freed together with
// resetting the ramp mode that guards it.
static void PScript_UnloadSets(void)
{
	pscript_redirect_t **link;
	pscript_redirect_t *r;
	pscript_type_t *ptype;
	pscript_set_t *set;
	int i;

	pscript_expsprite = true;

	for (link = &pscript_redirects; *link;) {
		r = *link;
		if (r->fromset) {
			*link = r->next;
			Q_free(r);
		}
		else {
			link = &r->next;
		}
	}
	PScript_FreeAssocs();

	for (i = 0; i < pscript_numtypes; i++) {
		ptype = &pscript_types[i];
		ptype->texname[0] = 0;
		ptype->scale = 0;
		ptype->loaded = 0;
		Q_free(ptype->ramp);
		ptype->rampmode = RAMP_NONE;
		ptype->rampindexes = 0;
	}

	while (pscript_sets) {
		set = pscript_sets;
		pscript_sets = set->next;
		Q_free(set);
	}
	PScript_ForgetMissingSounds();
}

// Loads the sets r_particledesc names and the current map's map_<name> set.
static void PScript_ReloadSets(void)
{
	char token[MAX_COM_TOKEN];
	char base[32];
	char name[MAX_QPATH];
	const char *s = r_particledesc.string;

	pscript_sets_dirty = false;
	PScript_UnloadSets();
	PScript_FlushImageCache();

	while ((s = COM_Parse(s))) {
		strlcpy(token, com_token, sizeof(token));
		PScript_LoadSet(token, false, true);
	}

	if (cl.worldmodel) {
		COM_FileBase(cl.worldmodel->name, base);
		if (PScript_SafeSetName(base)) {
			snprintf(name, sizeof(name), "map_%s", base);
			PScript_LoadSet(name, false, false);
		}
	}
}

// An empty config matches any.
static int PScript_LookupType(const char *config, const char *name)
{
	int i;

	for (i = 0; i < pscript_numtypes; i++) {
		if (pscript_types[i].loaded && !strcasecmp(pscript_types[i].name, name) && (!config[0] || !strcasecmp(pscript_types[i].config, config))) {
			return i;
		}
	}
	return P_INVALID;
}

// Returns the final name, which may point into the redirect list, or NULL
// when the chain is too long.
static const char *PScript_ResolveRedirects(const char *name)
{
	pscript_redirect_t *r = pscript_redirects;
	int hops = 0;

	while (r) {
		if (strcasecmp(r->from, name)) {
			r = r->next;
			continue;
		}
		if (hops++ == PSCRIPT_MAX_REDIRECT_HOPS) {
			return NULL;
		}
		name = r->to;
		r = pscript_redirects;
	}
	return name;
}

// Returns the loaded type for an effect name after redirects, or P_INVALID.
// A plain name matches any config; "ns.effect" loads particles/<ns>.cfg as a
// weak set when needed, which can move the type array.
int PScript_FindParticleType(const char *fullname)
{
	char config[MAX_QPATH];
	char name[MAX_QPATH];
	const char *effect = PScript_ResolveRedirects(fullname);
	const char *dot;
	int type;

	if (!effect) {
		return P_INVALID;
	}

	// Copied, because loading a set can rewrite the redirect list and the
	// caller's buffer.
	dot = strchr(effect, '.');
	if (dot && dot - effect < MAX_QPATH - 1) {
		memcpy(config, effect, dot - effect);
		config[dot - effect] = 0;
		effect = dot + 1;
	}
	else {
		config[0] = 0;
	}
	strlcpy(name, PScript_LegacyName(effect), sizeof(name));

	type = PScript_LookupType(config, name);
	if (type == P_INVALID && PScript_SafeSetName(config) && PScript_LoadSet(config, true, true)) {
		type = PScript_LookupType(config, name);
	}
	return type;
}

// These are effects of events and moving entities, so under rulesets that
// restrict particles they give P_INVALID and the hooks fall back to ezQuake's
// own effects.
int PScript_EffectType(pscript_effect_t effect)
{
	if ((int)effect < 0 || effect >= PSCRIPT_EFFECT_COUNT || Rulesets_RestrictParticles()) {
		return P_INVALID;
	}
	return pscript_effectids[effect];
}

// A redirect can point an effect name at a set that is not loaded yet.
qbool PScript_HasRedirects(void)
{
	return pscript_redirects != NULL;
}

// Whether the explosion sprite is drawn on top of a scripted explosion; sets
// that draw their own fireball turn it off with cl_expsprite.
qbool PScript_ExplosionSprite(void)
{
	return pscript_expsprite;
}

// Its lookups can load namespaced sets, which ask for another refresh; that
// stops because each set loads once.
static void PScript_Refresh(void)
{
	extern model_t mod_known[];
	extern int mod_numknown;
	int i, type;

	pscript_effects_dirty = false;

	pscript_pe_default = PScript_FindParticleType("PE_DEFAULT");
	pscript_pe_size2 = PScript_FindParticleType("PE_SIZE2");
	pscript_pe_size3 = PScript_FindParticleType("PE_SIZE3");
	for (i = 0; i < 256; i++) {
		pscript_pe_colors[i] = PScript_FindParticleType(va("pe_%i", i));
	}
	for (i = 0; i < PSCRIPT_EFFECT_COUNT; i++) {
		type = P_INVALID;
		if (pscript_effectnames[i].name) {
			type = PScript_FindParticleType(pscript_effectnames[i].name);
		}
		if (type == P_INVALID && pscript_effectnames[i].fallback) {
			type = PScript_FindParticleType(pscript_effectnames[i].fallback);
		}
		pscript_effectids[i] = type;
	}

	pscript_anyloaded = false;
	for (i = 0; i < pscript_numtypes; i++) {
		if (pscript_types[i].loaded) {
			pscript_anyloaded = true;
			break;
		}
	}

	// Inline models (*N) are copies of the world model that are never
	// loaded on their own, so only this reaches them.
	for (i = 0; i < mod_numknown; i++) {
		PScript_UpdateModelEffects(&mod_known[i]);
	}

	PScript_RebuildSurfaceEmitters();
	pscript_refreshcount = pscript_refreshcount == INT_MAX ? 1 : pscript_refreshcount + 1;
}

// r_partredirect [from [to]]; an empty or identical to removes the redirect.
// A set's redirect (fromset) lasts until the sets unload and never replaces
// one made at the console, so the user's choice wins.
void PScript_PartRedirectCmd(tokenizecontext_t *ctx, qbool fromset)
{
	const char *from = Cmd_ArgvEx(ctx, 1);
	const char *to = Cmd_ArgvEx(ctx, 2);
	pscript_redirect_t **link;
	pscript_redirect_t *r;
	size_t fromlen, tolen;
	int count = 0;

	if (!from[0]) {
		for (r = pscript_redirects; r; r = r->next) {
			Con_Printf("%s -> %s\n", r->from, r->to);
		}
		return;
	}

	for (link = &pscript_redirects; *link; link = &(*link)->next, count++) {
		if (!strcasecmp((*link)->from, from)) {
			break;
		}
	}
	if (Cmd_ArgcEx(ctx) == 2) {
		if (*link) {
			Con_Printf("particle %s is currently remapped to %s\n", (*link)->from, (*link)->to);
		}
		else {
			Con_Printf("particle %s is not remapped\n", from);
		}
		return;
	}

	if (*link) {
		if (fromset && !(*link)->fromset) {
			Con_DPrintf("r_partredirect %s ignored: the user redirected it\n", from);
			return;
		}
		r = *link;
		*link = r->next;
		Q_free(r);
	}
	if (to[0] && strcasecmp(from, to)) {
		// For a new name count is the list's length; a replaced redirect was
		// found before the end, so replacing one always succeeds.
		if (count >= PSCRIPT_MAX_REDIRECTS) {
			Con_DPrintf("r_partredirect %s ignored: more than %d redirects\n", from, PSCRIPT_MAX_REDIRECTS);
			return;
		}
		fromlen = strlen(from);
		tolen = strlen(to);
		r = Q_malloc(sizeof(*r) + fromlen + 1 + tolen + 1);
		r->from = (char *)(r + 1);
		r->to = r->from + fromlen + 1;
		memcpy(r->from, from, fromlen + 1);
		memcpy(r->to, to, tolen + 1);
		r->fromset = fromset;
		r->next = pscript_redirects;
		pscript_redirects = r;
	}

	pscript_effects_dirty = true;
}

static void PScript_PartRedirect_f(void)
{
	tokenizecontext_t ctx;

	Cmd_SaveContext(&ctx);
	PScript_PartRedirectCmd(&ctx, false);
}

// "r_trail <model> <effect>" and "r_effect <model> <effect> [flags]" in a
// particle set. An effect without a namespace is taken from config.
// Players, eyes, flags and a few others are refused, as an effect attached to
// them would show them through walls.
void PScript_AssociateEffectCmd(tokenizecontext_t *ctx, const char *config)
{
	const char *command = Cmd_ArgvEx(ctx, 0);
	const char *model = Cmd_ArgvEx(ctx, 1);
	const char *effect = Cmd_ArgvEx(ctx, 2);
	qbool trail = !strcasecmp(command, "r_trail");
	unsigned int flags = 0;
	char qualified[MAX_QPATH * 2];
	char lowered[MAX_QPATH];
	pscript_assoc_t *a;
	const char *flag;
	int i, count = 0;

	for (i = 3; !trail && i < Cmd_ArgcEx(ctx); i++) {
		flag = Cmd_ArgvEx(ctx, i);
		if (!strcmp(flag, "replace") || !strcmp(flag, "1")) {
			flags |= PSCRIPT_EMIT_REPLACE;
		}
		else if (!strcmp(flag, "forwards") || !strcmp(flag, "forward")) {
			flags |= PSCRIPT_EMIT_FORWARDS;
		}
		else if (strcmp(flag, "0")) {
			Con_DPrintf("%s %s: unknown flag %s\n", command, model, flag);
		}
	}

	// A namespace with a dot could not be told from the effect name again.
	if (config[0] && !strchr(config, '.') && !strchr(effect, '.')) {
		snprintf(qualified, sizeof(qualified), "%s.%s", config, effect);
		effect = qualified;
	}

	if (!model[0] || strlen(model) >= MAX_QPATH || strlen(effect) >= MAX_QPATH) {
		return;
	}
	strlcpy(lowered, model, sizeof(lowered));
	Q_strlwr(lowered);
	if (strstr(lowered, "player") || strstr(lowered, "eyes") || strstr(lowered, "flag") || strstr(lowered, "tf_stan") || strstr(lowered, ".bsp") || strstr(lowered, "turr")) {
		Con_Printf("Not allowed to attach effects to model \"%s\"\n", model);
		return;
	}

	for (a = pscript_assocs; a; a = a->next, count++) {
		if (a->trail == trail && !strcmp(a->model, model)) {
			break;
		}
	}
	if (!a) {
		if (count >= PSCRIPT_MAX_ASSOCS) {
			Con_DPrintf("%s %s ignored: more than %d model effects\n", command, model, PSCRIPT_MAX_ASSOCS);
			return;
		}
		a = Q_calloc(1, sizeof(*a));
		strlcpy(a->model, model, sizeof(a->model));
		a->trail = trail;
		a->next = pscript_assocs;
		pscript_assocs = a;
	}
	strlcpy(a->effect, effect, sizeof(a->effect));
	a->flags = flags;

	pscript_effects_dirty = true;
}

// Can load namespaced sets.
void PScript_UpdateModelEffects(struct model_s *mod)
{
	pscript_assoc_t *a;

	mod->emiteffect = P_INVALID;
	mod->traileffect = P_INVALID;
	mod->emitflags = 0;
	for (a = pscript_assocs; a; a = a->next) {
		if (strcmp(a->model, mod->name)) {
			continue;
		}
		if (a->trail) {
			mod->traileffect = PScript_FindParticleType(a->effect);
		}
		else {
			mod->emiteffect = PScript_FindParticleType(a->effect);
			mod->emitflags = a->flags;
		}
	}
}

static void PScript_PartInfo_f(void)
{
	pscript_counts_t counts, total, spare;
	pscript_type_t *ptype;
	pscript_set_t *set;
	int i, loaded = 0;

	if (!pscript_sets) {
		Con_Printf("No particle sets loaded\n");
	}
	for (set = pscript_sets; set; set = set->next) {
		Con_Printf("Particle set %s%s\n", set->name, set->found ? "" : " (not found)");
	}
	for (i = 0; i < pscript_numtypes; i++) {
		if (pscript_types[i].loaded) {
			loaded++;
		}
	}
	Con_Printf("%d effect types, %d loaded\n", pscript_numtypes, loaded);

	memset(&total, 0, sizeof(total));
	for (ptype = pscript_runlist, i = 0; ptype && i < pscript_numtypes; ptype = ptype->nexttorun, i++) {
		memset(&counts, 0, sizeof(counts));
		PScript_CountInstances(ptype, &counts);
		Con_Printf("%s.%s: %d particles%s, %d beams, %d decals\n", ptype->config, ptype->name, counts.particles, ptype->cliptype >= 0 ? " (+traceline)" : "", counts.beams, counts.decals);
		total.particles += counts.particles;
		total.beams += counts.beams;
		total.decals += counts.decals;
	}
	memset(&spare, 0, sizeof(spare));
	PScript_CountFree(&spare);
	Con_Printf("%d particles, %d free\n", total.particles, spare.particles);
	Con_Printf("%d beam segments, %d free\n", total.beams, spare.beams);
	Con_Printf("%d decals, %d free\n", total.decals, spare.decals);
}

// Called at startup and after every vid_restart, which deletes all textures,
// so every call reloads each type's texture. The first call has the next frame
// load the sets, as a value set before registration never reached OnChange.
void PScript_Init(void)
{
	int i;

	if (!host_initialized) {
		Cvar_SetCurrentGroup(CVAR_GROUP_PARTICLES);
		Cvar_Register(&r_particledesc);
		Cvar_Register(&r_part_rain);
		Cvar_Register(&r_part_rain_quantity);
		Cvar_Register(&r_part_rain_prewarm);
		Cvar_Register(&r_bouncysparks);
		Cvar_Register(&r_decal_noperpendicular);
		Cvar_Register(&r_particle_tracelimit);
		Cvar_Register(&r_part_sparks);
		Cvar_Register(&r_part_sparks_trifan);
		Cvar_Register(&r_part_sparks_textured);
		Cvar_Register(&r_part_beams);
		Cvar_Register(&r_part_contentswitch);
		Cvar_Register(&r_part_density);
		Cvar_Register(&r_part_maxparticles);
		Cvar_Register(&r_part_maxdecals);
		Cvar_ResetCurrentGroup();

		Cmd_AddCommand("r_partinfo", PScript_PartInfo_f);
		Cmd_AddCommand("r_partredirect", PScript_PartRedirect_f);

		for (i = 0; i < PSCRIPT_EFFECT_COUNT; i++) {
			pscript_effectids[i] = P_INVALID;
		}
		for (i = 0; i < 256; i++) {
			pscript_pe_colors[i] = P_INVALID;
		}
		pscript_sets_dirty = true;
	}

	for (i = 0; i < PSCRIPT_TEX_COUNT; i++) {
		R_TextureReferenceInvalidate(pscript_builtin_textures[i]);
	}
	PScript_FlushImageCache();
	pscript_numimages = 0;
	for (i = 0; i < pscript_numtypes; i++) {
		PScript_LoadTexture(&pscript_types[i]);
	}
}

// Called at client shutdown.
void PScript_Shutdown(void)
{
	pscript_redirect_t *r;
	pscript_set_t *set;
	int i;

	PScript_FreePools();
	PScript_ClearAllSurfaceParticles();
	PScript_FreeSurfaceLeaves();
	for (i = 0; i < pscript_numtypes; i++) {
		Q_free(pscript_types[i].sounds);
		Q_free(pscript_types[i].ramp);
	}
	Q_free(pscript_types);
	pscript_numtypes = 0;
	pscript_typecapacity = 0;
	pscript_runlist = NULL;

	while (pscript_sets) {
		set = pscript_sets;
		pscript_sets = set->next;
		Q_free(set);
	}
	while (pscript_redirects) {
		r = pscript_redirects;
		pscript_redirects = r->next;
		Q_free(r);
	}
	PScript_FreeAssocs();

	pscript_pe_default = P_INVALID;
	pscript_pe_size2 = P_INVALID;
	pscript_pe_size3 = P_INVALID;
	for (i = 0; i < 256; i++) {
		pscript_pe_colors[i] = P_INVALID;
	}
	for (i = 0; i < PSCRIPT_EFFECT_COUNT; i++) {
		pscript_effectids[i] = P_INVALID;
	}
	for (i = 0; i < PSCRIPT_TEX_COUNT; i++) {
		R_TextureReferenceInvalidate(pscript_builtin_textures[i]);
	}
	pscript_active = false;
	pscript_restricted = false;
	pscript_sim_pending = false;
	pscript_anyloaded = false;
	pscript_effects_dirty = false;
	pscript_expsprite = true;
	pscript_sets_dirty = true;
}

// Called on map changes, after demo seeks and when rulesets start to restrict
// particles. Drops every live instance; sets and textures stay. The areas
// in view are empty again, so they prewarm.
void PScript_ClearParticles(void)
{
	if (pscript_active) {
		PScript_StartupPools();
	}
	PScript_ForgetSurfaceLeaves();
	pscript_last = r_refdef2.time;
}

// Called by R_NewMap once the map's models are loaded, so hooks later in this
// frame use the new map's effects.
void PScript_NewMap(void)
{
	PScript_ClearParticles();
	PScript_ReloadSets();
	PScript_Refresh();
}

// Called once per host frame after the network messages were parsed and
// before any view is rendered. When rulesets start to restrict particles,
// every instance is dropped so nothing spawned for an event or a moving
// entity outlasts the change.
void PScript_BeginFrame(void)
{
	qbool restricted = Rulesets_RestrictParticles();
	qbool want;

	if (pscript_sets_dirty) {
		PScript_ReloadSets();
		PScript_Refresh();
	}
	else if (pscript_effects_dirty) {
		PScript_Refresh();
	}

	want = pscript_anyloaded && r_drawparticles.integer && renderer.SceneTrisSupported();
	if (want && !pscript_active) {
		PScript_StartupPools();
		pscript_active = true;
		pscript_last = r_refdef2.time;
	}
	else if (!want && pscript_active) {
		PScript_ResetPools();
		pscript_active = false;
	}
	else if (restricted && !pscript_restricted) {
		PScript_ClearParticles();
	}
	pscript_restricted = restricted;

	pscript_frametime = bound(0, r_refdef2.time - pscript_last, 1);
	pscript_last = r_refdef2.time;
	pscript_hostframe = pscript_hostframe == INT_MAX ? 1 : pscript_hostframe + 1;

	if (pscript_active) {
		if (pscript_frametime > 0) {
			PScript_ExpireFrameParticles();
		}
		pscript_sim_pending = true;
		PScript_RebuildTraceEntities();
	}
}

// Called once per rendered view; the first view of a host frame simulates it.
void PScript_DrawParticles(void)
{
	if (!pscript_active) {
		return;
	}
	PScript_AddSurfaceView();
	if (pscript_sim_pending) {
		PScript_Simulate(pscript_frametime);
		pscript_sim_pending = false;
	}
	PScript_BuildGeometry();
}

// Called once per host frame after every view is rendered. One-frame
// particles end when the next frame that advances time begins, so a paused
// view keeps showing them.
void PScript_EndFrame(void)
{
	if (!pscript_active) {
		return;
	}
	if (pscript_sim_pending) {
		PScript_Simulate(pscript_frametime);
		pscript_sim_pending = false;
	}
	if (pscript_frametime > 0) {
		PScript_MarkFrameParticles();
	}
	PScript_RunSurfaceEmitters();
}

// Every spawn function fails while this is false. Rulesets leave it alone:
// effects on the world, on static entities and on the map's inline brush
// models run under all of them, while the entry points for the rest also
// need PScript_DynamicActive.
qbool PScript_Active(void)
{
	return pscript_active;
}

// Effects of events and of other entities can carry game state, such as an
// emitter telling that an item is up, so they are off under rulesets that
// restrict particles.
qbool PScript_DynamicActive(void)
{
	return pscript_active && !Rulesets_RestrictParticles();
}

// True for a submodel of the current map ("*N"), such as a door or a plat.
// These are copies of the world model sharing its surfaces; an external .bsp
// model like a health box has its own.
qbool PScript_InlineModel(const struct model_s *mod)
{
	return mod && mod->type == mod_brush && mod->name[0] == '*' && cl.worldmodel && mod->surfaces == cl.worldmodel->surfaces;
}

// For engine emitters visited once per view: true at most once per host frame
// for a stamp, and only while active and time passes.
qbool PScript_EmitterDue(int *stamp)
{
	if (*stamp == pscript_hostframe) {
		return false;
	}
	*stamp = pscript_hostframe;
	return pscript_active && pscript_frametime > 0;
}
