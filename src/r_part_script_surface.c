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

// Surface emitters: while r_part_rain is set, brush model surfaces whose
// texture has an effect (tex_<texture>, or the one a worldspawn
// _texpart_<texture> key names) spawn it at random points near a view and in
// its PVS, at a rate proportional to their area and the effect's
// rainfrequency. With r_part_rain_prewarm, a leaf of the world that comes
// into view also gets, pre-aged, what the world's surfaces would have
// emitted into it while it was out of view.

#include <float.h>
#include "quakedef.h"
#include "gl_model.h"
#include "r_local.h"
#include "r_part_script_local.h"

// r_model.c has no header for its model table.
extern model_t mod_known[MAX_MODELS];
extern int mod_numknown;
// Declared for the OpenGL code only, in gl_local.h.
extern cvar_t r_novis;

#define PSCRIPT_SURF_MAXVERTS 64
// About 9 MB of triangle blocks, so no map can make a rebuild allocate
// without bound.
#define PSCRIPT_SURF_MAXTRIS (1 << 17)
// Spawn attempts per triangle and per host frame. A triangle that needs more
// drops the rest, so it can't starve the others of the frame's attempts.
#define PSCRIPT_SURF_MAXTRIEMITS 1024
// Spawn attempts per host frame over all triangles, which bounds the time
// emission takes whatever the rates.
#define PSCRIPT_SURF_MAXEMITS (1 << 15)
// Worldspawn overrides kept per map, so one map can't grow the table.
#define PSCRIPT_SURF_MAXTEXEFFECTS 256
// Texture names in a BSP fill all 16 bytes without a terminator at full length.
#define PSCRIPT_SURF_TEXNAMESIZE (sizeof(((texture_t *)0)->name) + 1)
#define PSCRIPT_SURF_EFFECTUNKNOWN (P_INVALID - 1)
// Views one host frame emits for, as many as cl_multiview draws. A further
// view of the frame shows what was spawned for these.
#define PSCRIPT_SURF_MAXVIEWS 4

// Prewarming: when a world leaf comes back into view, the emission points
// that land in it for a while also spawn the particles it missed, pre-aged.
// A return after less than this many seconds out of view is PVS flicker,
// which the particles still falling cover.
#define PSCRIPT_SURF_REVEALGAP 0.5
// The window after a reveal in which the leaf's points add what it missed
// lasts at least this many seconds, and long enough for this many of a
// triangle's points.
#define PSCRIPT_SURF_MINWINDOW 0.2
#define PSCRIPT_SURF_WINDOWPOINTS 4
// Most extra particles one point adds; the window lengthens so that it
// suffices.
#define PSCRIPT_SURF_MAXEXTRAS 64
// Random points tried for each extra particle until one lands in the leaf.
#define PSCRIPT_SURF_MAXTRIES 16
// Most seconds of missed emission replayed, for types that live all but
// forever.
#define PSCRIPT_SURF_MAXREPLAY 20
// Pre-aged effects per host frame. Each particle of a colliding type traces
// its whole flight.
#define PSCRIPT_SURF_MAXAGED 1024
// Assoc chain steps walked for a triangle's longest life; chains can loop.
#define PSCRIPT_SURF_MAXCHAIN 64
// Maps with more leaves than this are not prewarmed.
#define PSCRIPT_SURF_MAXLEAVES (1 << 20)

typedef struct pscript_texeffect_s {
	char texture[PSCRIPT_SURF_TEXNAMESIZE];
	char effect[2 * MAX_QPATH]; // namespace.name, each shorter than MAX_QPATH
} pscript_texeffect_t;

typedef struct pscript_surfbuild_s {
	texture_t **textures; // texture array whose effect types effects[] holds
	int numtextures;
	int *effects; // type, P_INVALID or PSCRIPT_SURF_EFFECTUNKNOWN per texture
	int numtris;
	int numskipped; // surfaces with PSCRIPT_SURF_MAXVERTS or more vertices
	int lifetype;   // the type lifetime was found for
	float lifetime;
} pscript_surfbuild_t;

// A world leaf that an emission point has landed in, by row bit (leaf
// number - 1). Times are on the world's emission clock.
typedef struct pscript_surfleaf_s {
	double lastseen; // when last in view, once seen
	double revealed; // when it last came into view after being out of it
	float missed;    // seconds of emission missed before then; 0 for none
	byte tracked;
	byte seen;
} pscript_surfleaf_t;

static pscript_texeffect_t pscript_texeffects[PSCRIPT_SURF_MAXTEXEFFECTS];
static int pscript_numtexeffects;

// The views drawn so far in host frame pscript_surfviewframe: their origins
// and PVS rows of pscript_surfvisrowbytes each.
static vec3_t pscript_surfvieworg[PSCRIPT_SURF_MAXVIEWS];
static byte *pscript_surfvis;
static int pscript_surfvisrowbytes;
static int pscript_surfnumviews;
static int pscript_surfviewframe;

// The leaves of pscript_surfleafmodel, its numleafs of them, and the indices
// of the tracked ones. When allocation failed, the model is kept with no
// leaves, so it isn't retried every frame.
static model_t *pscript_surfleafmodel;
static pscript_surfleaf_t *pscript_surfleaves;
static int pscript_surfnumleaves;
static int *pscript_surftracked;
static int pscript_surfnumtracked;

// Also called at client shutdown.
void PScript_FreeSurfaceLeaves(void)
{
	Q_free(pscript_surfleaves);
	Q_free(pscript_surftracked);
	pscript_surfleafmodel = NULL;
	pscript_surfnumleaves = 0;
	pscript_surfnumtracked = 0;
}

// Called when every particle is dropped: the tracked leaves count as never
// seen, so each prewarms fully the next time it is in view.
void PScript_ForgetSurfaceLeaves(void)
{
	int i;

	for (i = 0; i < pscript_surfnumtracked; i++) {
		pscript_surfleaves[pscript_surftracked[i]].seen = false;
		pscript_surfleaves[pscript_surftracked[i]].missed = 0;
	}
}

static void PScript_FreeSurfaceTris(model_t *mod)
{
	pscript_surftriblock_t *block;

	while (mod->surftriblocks) {
		block = mod->surftriblocks;
		mod->surftriblocks = block->next;
		Q_free(block);
	}
	mod->surftris = NULL;
	mod->surftritime = 0;
}

// Must run before mod's brush data is freed or reloaded, because each
// triangle points at its msurface_t.
void PScript_ClearSurfaceParticles(model_t *mod)
{
	if (mod == pscript_surfleafmodel) {
		PScript_FreeSurfaceLeaves();
	}
	PScript_FreeSurfaceTris(mod);
}

// Frees the surface emitters of every model slot, including models of
// earlier maps, and the recorded views. The leaves of the world are kept for
// the rebuilt emitters, as its particles live on, with their times moved to
// the emission clock that restarts at 0.
void PScript_ClearAllSurfaceParticles(void)
{
	pscript_surfleaf_t *leaf;
	int i;

	if (pscript_surfleafmodel) {
		for (i = 0; i < pscript_surfnumtracked; i++) {
			leaf = &pscript_surfleaves[pscript_surftracked[i]];
			leaf->lastseen -= pscript_surfleafmodel->surftritime;
			leaf->revealed -= pscript_surfleafmodel->surftritime;
		}
	}
	for (i = 0; i < mod_numknown; i++) {
		PScript_FreeSurfaceTris(&mod_known[i]);
	}
	Q_free(pscript_surfvis);
	pscript_surfvisrowbytes = 0;
	pscript_surfnumviews = 0;
}

// Case-insensitive.
static pscript_texeffect_t *PScript_FindTextureEffect(const char *texture)
{
	int i;

	for (i = 0; i < pscript_numtexeffects; i++) {
		if (!strcasecmp(pscript_texeffects[i].texture, texture)) {
			return &pscript_texeffects[i];
		}
	}
	return NULL;
}

// Forgets the worldspawn overrides. Called before a map's worldspawn is
// parsed, which also happens again on vid_restart.
void PScript_ClearTextureEffects(void)
{
	if (pscript_numtexeffects) {
		pscript_numtexeffects = 0;
		pscript_effects_dirty = true;
	}
}

// Makes the world's surfaces with this texture emit the named effect instead
// of tex_<texture>; an empty effect makes them emit nothing.
void PScript_SetTextureEffect(const char *texture, const char *effect)
{
	pscript_texeffect_t *te;

	if (!texture[0] || strlen(texture) >= sizeof(pscript_texeffects[0].texture) || strlen(effect) >= sizeof(pscript_texeffects[0].effect)) {
		Con_DPrintf("Ignoring worldspawn key texpart_%s\n", texture);
		return;
	}

	te = PScript_FindTextureEffect(texture);
	if (!te) {
		if (pscript_numtexeffects == PSCRIPT_SURF_MAXTEXEFFECTS) {
			Con_DPrintf("Ignoring texpart_%s: more than %d texture effects\n", texture, PSCRIPT_SURF_MAXTEXEFFECTS);
			return;
		}
		te = &pscript_texeffects[pscript_numtexeffects++];
		strlcpy(te->texture, texture, sizeof(te->texture));
	}
	strlcpy(te->effect, effect, sizeof(te->effect));
	pscript_effects_dirty = true;
}

// Returns the effect type that surfaces with this texture emit, or
// P_INVALID. The worldspawn overrides apply only to the world's textures,
// which its inline models share. May load a set implicitly and so
// reallocate pscript_types.
static int PScript_TextureEffectType(const texture_t *tex, qbool overrides)
{
	char name[PSCRIPT_SURF_TEXNAMESIZE];
	char effect[sizeof("tex_") - 1 + PSCRIPT_SURF_TEXNAMESIZE];
	const pscript_texeffect_t *te;
	size_t len;

	for (len = 0; len < sizeof(tex->name) && tex->name[len]; len++) {
		name[len] = tex->name[len];
	}
	name[len] = 0;

	te = overrides ? PScript_FindTextureEffect(name) : NULL;
	if (te) {
		return te->effect[0] ? PScript_FindParticleType(te->effect) : P_INVALID;
	}

	// A #suffix carries shader arguments and is not part of the effect
	// name.
	name[strcspn(name, "#")] = 0;
	snprintf(effect, sizeof(effect), "tex_%s", name);
	return PScript_FindParticleType(effect);
}

// Returns the seconds between two emissions of tri, or 0 when it must not
// emit. factor is r_part_rain_quantity / 10000 times the scale squared.
static double PScript_SurfTriInterval(const pscript_surftri_t *tri, double factor)
{
	double rate, interval;

	if (!PART_VALID(tri->ptype) || !pscript_types[tri->ptype].loaded) {
		return 0;
	}
	rate = tri->area * factor * pscript_types[tri->ptype].rainfrequency;
	if (!(rate > 0 && rate <= DBL_MAX)) {
		return 0;
	}
	interval = 1 / rate;
	return interval <= DBL_MAX ? interval : 0;
}

// Returns the longest die of the types along ptype's assoc chain that spawn
// pre-aged particles, at most PSCRIPT_SURF_MAXREPLAY.
static float PScript_SurfEffectLife(int ptype)
{
	const pscript_type_t *type;
	float life = 0;
	int steps;

	for (steps = 0; steps < PSCRIPT_SURF_MAXCHAIN && PART_VALID(ptype); steps++) {
		type = &pscript_types[ptype];
		if (type->loaded && type->looks.type != PT_CDECAL && type->looks.type != PT_BEAM && type->die > life) {
			life = type->die;
		}
		ptype = type->assoc;
	}
	return min(life, PSCRIPT_SURF_MAXREPLAY);
}

// Appends the triangle v0 v1 v2 of surf to the emitters of mod. Returns false
// when the triangle limit is reached or a block can't be allocated, which
// ends the rebuild; the lists stay valid either way.
static qbool PScript_AddSurfaceTri(model_t *mod, msurface_t *surf, int ptype, vec3_t v0, vec3_t v1, vec3_t v2, pscript_surfbuild_t *build)
{
	pscript_surftriblock_t *block;
	pscript_surftri_t *tri;
	vec3_t x, y, cross;
	double area;

	VectorSubtract(v1, v0, x);
	VectorSubtract(v2, v0, y);
	CrossProduct(x, y, cross);
	// The parallelogram's area, twice the triangle's.
	area = sqrt(DotProduct(cross, cross));
	// Collinear vertices give no area; an area too small for the rate to
	// have an interval never emits.
	if (!(area > 0 && area <= FLT_MAX)) {
		return true;
	}

	if (build->numtris == PSCRIPT_SURF_MAXTRIS) {
		Con_Printf("Surface particles: more than %d triangles, the rest don't emit\n", PSCRIPT_SURF_MAXTRIS);
		return false;
	}
	block = mod->surftriblocks;
	if (!block || block->count == sizeof(block->tris) / sizeof(block->tris[0])) {
		block = Q_tryrealloc(NULL, sizeof(*block));
		if (!block) {
			Con_Printf("Surface particles: out of memory\n");
			return false;
		}
		block->count = 0;
		block->next = mod->surftriblocks;
		mod->surftriblocks = block;
	}

	tri = &block->tris[block->count++];
	VectorCopy(v0, tri->org);
	VectorCopy(x, tri->x);
	VectorCopy(y, tri->y);
	tri->area = area;
	tri->ptype = ptype;
	// A surface's triangles share its type.
	if (build->lifetype != ptype) {
		build->lifetype = ptype;
		build->lifetime = PScript_SurfEffectLife(ptype);
	}
	tri->maxlife = build->lifetime;
	tri->face = surf;
	// A random phase, so that triangles of equal area don't emit in lockstep.
	tri->nexttime = mod->surftritime + frandom() * PScript_SurfTriInterval(tri, r_part_rain_quantity.value / 10000.0);
	tri->next = mod->surftris;
	mod->surftris = tri;
	build->numtris++;
	return true;
}

// The polygon comes from the edge loop because surf->polys are built late and
// in strip order. Returns false when the rebuild must end.
static qbool PScript_BuildSurfaceTris(model_t *mod, msurface_t *surf, int ptype, pscript_surfbuild_t *build)
{
	vec3_t verts[PSCRIPT_SURF_MAXVERTS];
	int i, numverts;

	if (surf->numedges >= PSCRIPT_SURF_MAXVERTS) {
		build->numskipped++;
		return true;
	}
	if (!surf->plane) {
		return true;
	}

	numverts = PScript_SurfacePolygon(mod, surf, verts, PSCRIPT_SURF_MAXVERTS);
	for (i = 2; i < numverts; i++) {
		if (!PScript_AddSurfaceTri(mod, surf, ptype, verts[0], verts[i - 1], verts[i], build)) {
			return false;
		}
	}
	return true;
}

// Builds the emitters of mod's own surfaces: the world's exclude its inline
// models' surfaces. Returns false when the rebuild must end.
static qbool PScript_BuildModelSurfaces(model_t *mod, pscript_surfbuild_t *build)
{
	msurface_t *surf;
	texture_t *tex;
	int i;

	if (!mod->textures || mod->numtextures <= 0 || mod->firstmodelsurface < 0 || mod->nummodelsurfaces <= 0 || mod->firstmodelsurface > mod->numsurfaces - mod->nummodelsurfaces) {
		return true;
	}

	if (build->textures != mod->textures || build->numtextures != mod->numtextures) {
		Q_free(build->effects);
		build->effects = Q_tryrealloc(NULL, mod->numtextures * sizeof(*build->effects));
		if (!build->effects) {
			Con_Printf("Surface particles: out of memory\n");
			return false;
		}
		for (i = 0; i < mod->numtextures; i++) {
			build->effects[i] = PSCRIPT_SURF_EFFECTUNKNOWN;
		}
		build->textures = mod->textures;
		build->numtextures = mod->numtextures;
	}

	for (i = 0; i < mod->nummodelsurfaces; i++) {
		surf = &mod->surfaces[mod->firstmodelsurface + i];
		tex = surf->texinfo ? surf->texinfo->texture : NULL;
		// Missing textures point at r_notexture_mip, which is in no model's array.
		if (!tex || tex->index < 0 || tex->index >= mod->numtextures || mod->textures[tex->index] != tex) {
			continue;
		}
		if (build->effects[tex->index] == PSCRIPT_SURF_EFFECTUNKNOWN) {
			build->effects[tex->index] = PScript_TextureEffectType(tex, mod->textures == cl.worldmodel->textures);
		}
		if (build->effects[tex->index] != P_INVALID && !PScript_BuildSurfaceTris(mod, surf, build->effects[tex->index], build)) {
			return false;
		}
	}
	return true;
}

// Call whenever the sets, redirects, overrides or r_part_rain change, because
// the triangles hold effect type indices.
void PScript_RebuildSurfaceEmitters(void)
{
	pscript_surfbuild_t build;
	model_t *mod;
	int i;

	PScript_ClearAllSurfaceParticles();
	// Looking an effect up can load a set implicitly, so while r_part_rain is
	// on the effects a map's worldspawn keys name can make a set load; off,
	// none are looked up. Without a loaded effect, a redirect or such a key
	// no texture can have an effect, so the models are not walked at all.
	if (!r_part_rain.value || !cl.worldmodel || (!pscript_anyloaded && !pscript_numtexeffects && !PScript_HasRedirects())) {
		PScript_FreeSurfaceLeaves();
		return;
	}

	memset(&build, 0, sizeof(build));
	build.lifetype = PSCRIPT_SURF_EFFECTUNKNOWN;
	for (i = 1; i < MAX_MODELS; i++) {
		mod = cl.model_precache[i];
		// A model listed twice is built once.
		if (!mod || mod->needload || mod->type != mod_brush || mod->surftriblocks) {
			continue;
		}
		if (!PScript_BuildModelSurfaces(mod, &build)) {
			break;
		}
	}
	if (build.numskipped) {
		Con_DPrintf("Surface particles: %d surfaces with %d or more vertices don't emit\n", build.numskipped, PSCRIPT_SURF_MAXVERTS);
	}
	Q_free(build.effects);
	// Leaves are kept only while the world emits.
	if (!cl.worldmodel->surftris) {
		PScript_FreeSurfaceLeaves();
	}
}

// Records the origin and PVS of the view being drawn, as R_MarkLeaves has
// just marked it, for the emission at the end of the frame. Once per view.
void PScript_AddSurfaceView(void)
{
	int i, rowbytes;
	const byte *pvs;
	byte *row;

	if (pscript_surfviewframe != pscript_hostframe) {
		pscript_surfviewframe = pscript_hostframe;
		pscript_surfnumviews = 0;
	}
	if (!r_part_rain.value || !cl.worldmodel || pscript_surfnumviews == PSCRIPT_SURF_MAXVIEWS) {
		return;
	}

	rowbytes = cl.worldmodel->vis_rowbytes;
	if (rowbytes > pscript_surfvisrowbytes) {
		Q_free(pscript_surfvis);
		pscript_surfvisrowbytes = 0;
		pscript_surfnumviews = 0;
		pscript_surfvis = Q_tryrealloc(NULL, PSCRIPT_SURF_MAXVIEWS * (size_t)rowbytes);
		if (!pscript_surfvis) {
			return;
		}
		pscript_surfvisrowbytes = rowbytes;
	}

	row = pscript_surfvis + pscript_surfnumviews * rowbytes;
	if (r_novis.value) {
		memcpy(row, cl.worldmodel->vis_novis, rowbytes);
	}
	else {
		memcpy(row, Mod_LeafPVS(r_viewleaf, cl.worldmodel), rowbytes);
		if (r_viewleaf2) {
			pvs = Mod_LeafPVS(r_viewleaf2, cl.worldmodel);
			for (i = 0; i < rowbytes; i++) {
				row[i] |= pvs[i];
			}
		}
	}
	VectorCopy(r_origin, pscript_surfvieworg[pscript_surfnumviews]);
	pscript_surfnumviews++;
}

// Turns a model-space vector into world space; in and out must not overlap.
static void PScript_SurfRotate(vec3_t axis[3], const vec3_t in, vec3_t out)
{
	out[0] = axis[0][0] * in[0] + axis[1][0] * in[1] + axis[2][0] * in[2];
	out[1] = axis[0][1] * in[0] + axis[1][1] * in[1] + axis[2][1] * in[2];
	out[2] = axis[0][2] * in[0] + axis[1][2] * in[1] + axis[2][2] * in[2];
}

// True when point is out of solid and in the PVS of a recorded view at most
// maxdist away. A point that several views see passes once, so their
// emission doesn't add up where they overlap. *leafout is the row bit of the
// point's leaf when a view is close enough for it to be looked up and it is
// not solid, else -1.
static qbool PScript_SurfPointVisible(vec3_t point, float maxdist, int *leafout)
{
	int i, rowbytes = cl.worldmodel->vis_rowbytes;
	int leafnum = -1;
	mleaf_t *leaf;
	vec3_t delta;

	*leafout = -1;
	for (i = 0; i < pscript_surfnumviews; i++) {
		VectorSubtract(point, pscript_surfvieworg[i], delta);
		if (DotProduct(delta, delta) > maxdist * maxdist) {
			continue;
		}
		if (leafnum < 0) {
			leaf = Mod_PointInLeaf(point, cl.worldmodel);
			// Row bit n is leaf n + 1; a leaf outside the rows is never marked visible.
			leafnum = leaf - cl.worldmodel->leafs - 1;
			if (leaf->contents == CONTENTS_SOLID || leafnum < 0 || leafnum >= cl.worldmodel->numleafs) {
				return false;
			}
			*leafout = leafnum;
		}
		if (pscript_surfvis[i * rowbytes + (leafnum >> 3)] & (1 << (leafnum & 7))) {
			return true;
		}
	}
	return false;
}

// True when the leaf of row bit leafnum is in the PVS of a recorded view.
static qbool PScript_SurfLeafInView(int leafnum)
{
	int i, rowbytes = cl.worldmodel->vis_rowbytes;

	for (i = 0; i < pscript_surfnumviews; i++) {
		if (pscript_surfvis[i * rowbytes + (leafnum >> 3)] & (1 << (leafnum & 7))) {
			return true;
		}
	}
	return false;
}

// Returns true when mod, the world, has leaf records, allocating them if
// needed; false while r_part_rain_prewarm is off, which frees them.
static qbool PScript_SurfLeavesReady(model_t *mod)
{
	if (!r_part_rain_prewarm.value) {
		PScript_FreeSurfaceLeaves();
		return false;
	}
	if (pscript_surfleafmodel == mod && pscript_surfnumleaves == mod->numleafs) {
		return pscript_surfleaves != NULL;
	}

	PScript_FreeSurfaceLeaves();
	pscript_surfleafmodel = mod;
	pscript_surfnumleaves = mod->numleafs;
	if (mod->numleafs <= 0 || mod->numleafs > PSCRIPT_SURF_MAXLEAVES) {
		return false;
	}
	pscript_surfleaves = Q_tryrealloc(NULL, mod->numleafs * sizeof(*pscript_surfleaves));
	pscript_surftracked = Q_tryrealloc(NULL, mod->numleafs * sizeof(*pscript_surftracked));
	if (!pscript_surfleaves || !pscript_surftracked) {
		Q_free(pscript_surfleaves);
		Q_free(pscript_surftracked);
		Con_DPrintf("Surface particles: out of memory for prewarming\n");
		return false;
	}
	memset(pscript_surfleaves, 0, mod->numleafs * sizeof(*pscript_surfleaves));
	return true;
}

// Stamps the tracked leaves in view at time now as seen, and marks those that
// were out of view since more than PSCRIPT_SURF_REVEALGAP seconds, or never in
// it, as revealed now, with what they missed.
static void PScript_UpdateSurfaceLeaves(double now)
{
	pscript_surfleaf_t *leaf;
	int i;

	for (i = 0; i < pscript_surfnumtracked; i++) {
		if (!PScript_SurfLeafInView(pscript_surftracked[i])) {
			continue;
		}
		leaf = &pscript_surfleaves[pscript_surftracked[i]];
		if (!leaf->seen) {
			leaf->revealed = now;
			leaf->missed = FLT_MAX;
		}
		else if (now - leaf->lastseen > PSCRIPT_SURF_REVEALGAP) {
			leaf->revealed = now;
			leaf->missed = now - leaf->lastseen;
		}
		leaf->seen = true;
		leaf->lastseen = now;
	}
}

// Picks a point of tri at random, placed by axis, origin and scale, and moved
// 0.5 units along normal, off the plane, where it could land in the solid leaf
// behind it. Not uniform over the triangle: points gather toward its edge
// along tri->y.
static void PScript_SurfTriPoint(const pscript_surftri_t *tri, vec3_t axis[3], vec3_t origin, float scale, vec3_t normal, vec3_t point)
{
	vec3_t local;
	float s, t;

	s = frandom() * frandom();
	t = frandom() * (1 - s);
	PS_VectorMA(tri->org, s, tri->x, local);
	PS_VectorMA(local, t, tri->y, local);
	PScript_SurfRotate(axis, local, point);
	PS_VectorMA(origin, scale, point, point);
	PS_VectorMA(point, 0.5f, normal, point);
}

// Seconds after a reveal in which tri's points add what their leaf missed:
// long enough for several of its points, and for each to need at most
// PSCRIPT_SURF_MAXEXTRAS extras, but at most half the longest life, beyond
// which extras would be spawned too late for most to be alive.
static double PScript_SurfTriWindow(const pscript_surftri_t *tri, double interval)
{
	double window = max(PSCRIPT_SURF_MINWINDOW, PSCRIPT_SURF_WINDOWPOINTS * interval);

	window = max(window, tri->maxlife / (double)PSCRIPT_SURF_MAXEXTRAS);
	return min(window, tri->maxlife / 2.0);
}

// After a point of tri spawned in leaf leafnum of the world within window of
// the leaf's reveal, adds what the leaf missed. The span replayed is the
// emission missed, up to what can still be alive: the effect's longest life
// less the time since the reveal. Each point adds span / window particles on
// average, so the points of the window add up to the span's emission, aged
// from the time since the reveal to that plus the span. Each is spawned at a
// new point of tri that must pass the same tests in the same leaf, so the
// backfill does not spill into neighbours that were in view. Every try costs
// one unit of *budget and every spawn one of *aged.
static void PScript_PrewarmSurfacePoint(model_t *mod, const pscript_surftri_t *tri, vec3_t axis[3], vec3_t origin, float scale, vec3_t normal, int leafnum, double window, int *budget, int *aged)
{
	const pscript_surfleaf_t *leaf = &pscript_surfleaves[leafnum];
	double late = mod->surftritime - leaf->revealed;
	float span;
	int extras, tries, found;
	vec3_t point;

	if (!(leaf->missed > 0) || !(late < window)) {
		return;
	}
	span = min(leaf->missed, tri->maxlife - late);
	if (!(span > 0)) {
		return;
	}

	extras = min((int)(span / window + frandom()), PSCRIPT_SURF_MAXEXTRAS);
	for (; extras > 0; extras--) {
		for (tries = 0; tries < PSCRIPT_SURF_MAXTRIES; tries++) {
			if (!pscript_free_particles || *budget <= 0 || *aged <= 0) {
				return;
			}
			(*budget)--;
			PScript_SurfTriPoint(tri, axis, origin, scale, normal, point);
			if (PScript_SurfPointVisible(point, (1024 + 512) * frandom(), &found) && found == leafnum) {
				break;
			}
		}
		if (tries < PSCRIPT_SURF_MAXTRIES) {
			(*aged)--;
			PScript_RunAgedEffect(point, normal, 1, tri->ptype, NULL, late, span);
		}
	}
}

// axis (the model's x, y and z in world space), origin and scale place the
// model as the renderer draws it. Each attempt costs one unit of *budget; a
// triangle that is cut short drops its backlog rather than carry it over.
// aged, the pre-aged spawns left this frame, is NULL but for the world while
// it has leaf records: its points then track their leaves and prewarm them.
static void PScript_EmitModelSurfaces(model_t *mod, vec3_t axis[3], vec3_t origin, float scale, int *budget, int *aged)
{
	pscript_surftri_t *tri;
	double factor, interval, window = 0;
	int attempts, leafnum;
	vec3_t local, point, normal;

	mod->surftritime += pscript_frametime;
	factor = r_part_rain_quantity.value * scale * scale / 10000.0;
	if (aged) {
		PScript_UpdateSurfaceLeaves(mod->surftritime);
	}

	for (tri = mod->surftris; tri; tri = tri->next) {
		interval = PScript_SurfTriInterval(tri, factor);
		if (interval <= 0) {
			tri->nexttime = mod->surftritime;
			continue;
		}
		// A rate that went up since the last emission applies at once.
		if (tri->nexttime > mod->surftritime + interval) {
			tri->nexttime = mod->surftritime + interval;
		}
		if (tri->nexttime >= mod->surftritime) {
			continue;
		}

		VectorCopy(tri->face->plane->normal, local);
		if (tri->face->flags & SURF_PLANEBACK) {
			VectorNegate(local, local);
		}
		PScript_SurfRotate(axis, local, normal);
		if (aged && tri->maxlife > 0) {
			window = PScript_SurfTriWindow(tri, interval);
		}

		for (attempts = 0; tri->nexttime < mod->surftritime; attempts++) {
			if (!pscript_free_particles) {
				*budget = 0;
			}
			if (attempts == PSCRIPT_SURF_MAXTRIEMITS || *budget <= 0) {
				tri->nexttime = mod->surftritime;
				break;
			}
			tri->nexttime += interval;
			(*budget)--;

			PScript_SurfTriPoint(tri, axis, origin, scale, normal, point);
			if (PScript_SurfPointVisible(point, (1024 + 512) * frandom(), &leafnum)) {
				PScript_RunEffect(point, normal, 1, tri->ptype, NULL);
				if (aged && tri->maxlife > 0) {
					PScript_PrewarmSurfacePoint(mod, tri, axis, origin, scale, normal, leafnum, window, budget, aged);
				}
			}
			// A leaf is tracked from the first point that lands in it, in
			// view or not; its first time in view is a reveal.
			if (aged && leafnum >= 0 && !pscript_surfleaves[leafnum].tracked) {
				pscript_surfleaves[leafnum].tracked = true;
				pscript_surftracked[pscript_surfnumtracked++] = leafnum;
			}
		}
	}
}

// Emits from the brush entities of the latest valid packet; NQ demos keep
// theirs elsewhere. Under rulesets that restrict particles only entities with
// the map's inline brush models emit, not external .bsp models.
static void PScript_EmitEntitySurfaces(int *budget)
{
	packet_entities_t *pak;
	entity_state_t projected, *state;
	const rd_client_entity_t *entity;
	model_t *mod;
	vec3_t angles, axis[3];
	float scale;
	int i, count;
	qbool rd = CL_RD_Valid();
	qbool dynamic;

	if (cls.nqdemoplayback || !cl.validsequence) {
		return;
	}
	dynamic = PScript_DynamicActive();

	pak = &cl.frames[cl.validsequence & UPDATE_MASK].packet_entities;
	count = rd ? CL_RD_SlotLimit() : pak->num_entities;
	for (i = rd ? MAX_CLIENTS + 1 : 0; i < count && *budget > 0; i++) {
		if (rd) {
			entity = CL_RD_Entity(i, false);
			if (!entity) {
				continue;
			}
			CL_RD_ToLegacy(&entity->state, &projected);
			state = &projected;
		}
		else {
			state = &pak->entities[i];
		}

		if ((unsigned int)state->modelindex >= MAX_MODELS) {
			continue;
		}
		mod = cl.model_precache[state->modelindex];
		if (!mod || mod == cl.worldmodel || !mod->surftris || (!dynamic && !PScript_InlineModel(mod))) {
			continue;
		}

		// R_RotateForEntity turns brush models by the negated pitch.
		VectorSet(angles, -state->angles[0], state->angles[1], state->angles[2]);
		AngleVectorsFLU(angles, axis[0], axis[1], axis[2]);
		scale = 1;
#ifdef FTE_PEXT_SCALE
		if (state->scale) {
			scale = state->scale / 16.0f;
		}
#endif
		PScript_EmitModelSurfaces(mod, axis, state->origin, scale, budget, NULL);
	}
}

// Runs the surface emitters for the views recorded this host frame. Called
// once at the end of the frame, so that every view counts.
void PScript_RunSurfaceEmitters(void)
{
	vec3_t axis[3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
	vec3_t origin = { 0, 0, 0 };
	int budget = PSCRIPT_SURF_MAXEMITS;
	int aged = PSCRIPT_SURF_MAXAGED;

	if (!r_part_rain.value || !(r_part_rain_quantity.value > 0) || !(pscript_frametime > 0)) {
		return;
	}
	if (pscript_surfviewframe != pscript_hostframe || !pscript_surfnumviews) {
		return;
	}
	if (!cl.worldmodel || cl.worldmodel->needload || !cl.worldmodel->nodes) {
		return;
	}

	PScript_EmitModelSurfaces(cl.worldmodel, axis, origin, 1, &budget, cl.worldmodel->surftris && PScript_SurfLeavesReady(cl.worldmodel) ? &aged : NULL);
	PScript_EmitEntitySurfaces(&budget);
}
