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

// Scripted particle effects spawned at a point or in a box, clipped decals,
// the lights and sounds of effects, and the traces particles collide with.

#include "quakedef.h"
#include "gl_model.h"
#include "pmove.h"
#include "qsound.h"
#include "r_part_script_local.h"

// The vertex normal table of r_aliasmodel.c, which no header declares.
extern float r_avertexnormals[NUMVERTEXNORMALS][3];

// Sound names effects may play. S_FindName keeps every name in a fixed table
// shared with the server's precaches and Sys_Errors when it is full, so names
// beyond these are not played. Kept across PScript_Shutdown, which only runs
// as the client exits.
#define PSCRIPT_MAX_SOUNDNAMES 256

typedef struct {
	char name[MAX_QPATH];
	qbool missing; // failed to load, so not tried again until the sets reload
} pscript_soundname_t;

static pscript_soundname_t pscript_soundnames[PSCRIPT_MAX_SOUNDNAMES];
static int pscript_numsoundnames;

// snd_main.c: whether S_PrecacheSound loads the file.
extern cvar_t s_precache;

// Lights and sounds go to code that converts distances to int: the dlight
// list of r_lightmaps.c multiplies a light's reach by 256, and SND_Spatialize
// scales a sound's distance by its volume. Both stay within int up to this
// many units, far beyond any BSP29 map but not runaway or hostile values.
#define PSCRIPT_MAX_COORD (1 << 20)

// Widest clipped decal, in units. Clipping visits every world surface within
// its reach, so a huge decal walks the whole map, and particles that splat
// into decals can ask for thousands in a frame.
#define PSCRIPT_MAX_DECALSIZE 256

// Per-normal angular speeds of the synced field pattern, the Quake
// brightfield; zero until first used.
static float avelocities[NUMVERTEXNORMALS][2];

// A brush entity particles collide with. The clip model is looked up by
// index at trace time rather than kept as a pointer: temp entities parsed in
// the packet that changes the map are spawned before the list is rebuilt,
// and cl.clipmodels only ever holds models of the loaded map.
typedef struct {
	int number; // in cl_entities, from 1
	int modelindex;
	vec3_t origin;
} pscript_traceent_t;

static pscript_traceent_t traceents[MAX_PHYSENTS];
static int numtraceents;

typedef struct {
	pscript_type_t *ptype;
	int entity; // what the decal lands on, as in pscript_clippeddecal_t
	int modelindex;
	vec3_t center; // in the space of the model clipped to
	vec3_t normal; // into the surface
	vec3_t tangent1;
	vec3_t tangent2;

	float scale0; // fades vertices away from the plane of the center
	float scale1;
	float scale2;

	float bias1;
	float bias2;
} pscript_decalctx_t;

// What a point effect does after one type of its assoc chain.
typedef enum {
	CHAIN_NEXT,         // continue with the assoc type and the trail state's assoc
	CHAIN_NEXT_SAME,    // continue with the assoc type on the same trail state
	CHAIN_NEXT_SURFACE, // CHAIN_NEXT_SAME, along the normal of the surface a decal found
	CHAIN_STOP          // end the effect
} pscript_chainstep_t;

// Returns count limited to what one call could ever spawn, or 0 when it is
// not finite, so counts from configs and servers convert to int and the
// fraction a trail state carries to the next call stays finite.
float PScript_BoundCount(float count)
{
	if (!isfinite(count)) {
		return 0;
	}
	return bound(-PSCRIPT_MAX_PARTICLES, count, PSCRIPT_MAX_PARTICLES);
}

// Adds the world-space jitter of a PT_WORLDSPACERAND type to a spawned
// particle: one random point of the unit ball scales both orgwrand and
// velwrand, then velbias is added to the velocity.
void PScript_WorldSpaceRandom(const pscript_type_t *ptype, pscript_particle_t *p)
{
	vec3_t r;

	do {
		r[0] = crand();
		r[1] = crand();
		r[2] = crand();
	} while (DotProduct(r, r) > 1);

	p->org[0] += r[0] * ptype->orgwrand[0];
	p->org[1] += r[1] * ptype->orgwrand[1];
	p->org[2] += r[2] * ptype->orgwrand[2];
	p->vel[0] += r[0] * ptype->velwrand[0];
	p->vel[1] += r[1] * ptype->velwrand[1];
	p->vel[2] += r[2] * ptype->velwrand[2];
	VectorAdd(p->vel, ptype->velbias, p->vel);
}

// Returns the world contents at p as PSCRIPT_CONTENTS_* bits. Brush entities
// are not tested. Empty while no map is loaded.
unsigned int PScript_PointContents(vec3_t p)
{
	hull_t *hull;

	if (!cl.clipmodels[1]) {
		return PSCRIPT_CONTENTS_EMPTY;
	}

	hull = &cl.clipmodels[1]->hulls[0];
	switch (CM_HullPointContents(hull, hull->firstclipnode, p)) {
		case CONTENTS_EMPTY: return PSCRIPT_CONTENTS_EMPTY;
		case CONTENTS_SOLID: return PSCRIPT_CONTENTS_SOLID;
		case CONTENTS_SLIME: return PSCRIPT_CONTENTS_SLIME;
		case CONTENTS_LAVA: return PSCRIPT_CONTENTS_LAVA;
		case CONTENTS_SKY: return PSCRIPT_CONTENTS_SKY;
		default:
			// Water, and the currents and other liquids of converted maps.
			return PSCRIPT_CONTENTS_WATER;
	}
}

// False when ptype may not spawn at org: notunderwater types stay out of
// their fluidmask contents and underwater types in them. Every type may
// while no map is loaded or r_part_contentswitch is off.
qbool PScript_ContentsAllow(const pscript_type_t *ptype, vec3_t org)
{
	unsigned int contents;

	if (!r_part_contentswitch.integer || !(ptype->flags & (PT_TRUNDERWATER | PT_TROVERWATER)) || !cl.clipmodels[1]) {
		return true;
	}
	contents = PScript_PointContents(org);
	if ((ptype->flags & PT_TROVERWATER) && (contents & ptype->fluidmask)) {
		return false;
	}
	return !((ptype->flags & PT_TRUNDERWATER) && !(contents & ptype->fluidmask));
}

// Returns the inwater type of ptype when it has one and org is in a fluid,
// else ptype. Point effects switch once, at the head of the chain; trails
// switch at every step.
pscript_type_t *PScript_InwaterType(pscript_type_t *ptype, vec3_t org)
{
	if (r_part_contentswitch.integer && PART_VALID(ptype->inwater) && (PScript_PointContents(org) & PSCRIPT_CONTENTS_FLUID)) {
		return &pscript_types[ptype->inwater];
	}
	return ptype;
}

// Rebuilds the brush entities PScript_TraceLine tests from the latest packet
// entities, as CL_SetSolidEntities does for prediction. Once per host frame.
void PScript_RebuildTraceEntities(void)
{
	packet_entities_t *pak;
	const vec_t *origin;
	int i, count, number, modelindex;
	qbool rd;

	numtraceents = 0;
	if (cls.state < ca_onserver || !cl.clipmodels[1]) {
		return;
	}

	rd = CL_RD_Valid();
	pak = &cl.frames[cl.parsecountmod].packet_entities;
	if (rd) {
		count = CL_RD_SlotLimit();
	}
	else {
		count = bound(0, pak->num_entities, (int)(sizeof(pak->entities) / sizeof(pak->entities[0])));
	}

	for (i = 0; i < count && numtraceents < MAX_PHYSENTS; i++) {
		if (rd) {
			const rd_client_entity_t *ent = CL_RD_Entity(i, false);

			// Player slots and non-BSP solids are boxes, which particles
			// pass through.
			if (i <= MAX_CLIENTS || !ent || ent->state.solidsize != ES_SOLID_BSP) {
				continue;
			}
			number = i;
			modelindex = ent->state.modelindex;
			origin = ent->state.origin;
		}
		else {
			number = pak->entities[i].number;
			modelindex = pak->entities[i].modelindex;
			origin = pak->entities[i].origin;
		}

		if (number <= 0 || number >= CL_MAX_EDICTS || modelindex <= 0 || modelindex >= MAX_MODELS || !cl.clipmodels[modelindex]) {
			continue;
		}
		traceents[numtraceents].number = number;
		traceents[numtraceents].modelindex = modelindex;
		VectorCopy(origin, traceents[numtraceents].origin);
		numtraceents++;
	}
}

// Traces hull 0 of a model placed at origin and keeps the hit if it is nearer
// than *fraction. Returns whether it kept it.
static qbool PScript_TraceHull(hull_t *hull, const vec3_t origin, vec3_t start, vec3_t end, float *fraction, vec3_t impact, vec3_t normal)
{
	trace_t trace;
	vec3_t relstart, relend;

	VectorSubtract(start, origin, relstart);
	VectorSubtract(end, origin, relend);
	trace = CM_HullTrace(hull, relstart, relend);
	if (trace.fraction < *fraction) {
		*fraction = trace.fraction;
		VectorAdd(trace.endpos, origin, impact);
		VectorCopy(trace.plane.normal, normal);
		return true;
	}
	return false;
}

// Returns the fraction of start to end at which the line first hits the world
// or a listed brush entity, 1 without a hit, which leaves impact at end and
// normal (0, 0, 1). Brush entities are traced unrotated, players not at all.
// Unless entnum is NULL, it is set to the number of the entity hit, 0 for the
// world or no hit.
float PScript_TraceLine(vec3_t start, vec3_t end, vec3_t impact, vec3_t normal, int *entnum)
{
	float fraction = 1;
	vec3_t mins, maxs;
	int i, j;

	VectorCopy(end, impact);
	VectorSet(normal, 0, 0, 1);
	if (entnum) {
		*entnum = 0;
	}
	if (!cl.clipmodels[1]) {
		return 1;
	}

	PScript_TraceHull(&cl.clipmodels[1]->hulls[0], vec3_origin, start, end, &fraction, impact, normal);

	for (j = 0; j < 3; j++) {
		mins[j] = min(start[j], end[j]);
		maxs[j] = max(start[j], end[j]);
	}
	for (i = 0; i < numtraceents && fraction > 0; i++) {
		const pscript_traceent_t *ent = &traceents[i];
		cmodel_t *model = cl.clipmodels[ent->modelindex];

		if (!model) {
			continue;
		}
		for (j = 0; j < 3; j++) {
			if (ent->origin[j] + model->mins[j] > maxs[j] || ent->origin[j] + model->maxs[j] < mins[j]) {
				break;
			}
		}
		if (j == 3 && PScript_TraceHull(&model->hulls[0], ent->origin, start, end, &fraction, impact, normal) && entnum) {
			*entnum = ent->number;
		}
	}

	return fraction;
}

// Returns the model of the brush entity numbered entnum that a trace of this
// host frame hit and sets origin to where it was traced, or NULL when the
// entity is no longer in the latest packet with that model, as after a map
// change, or its model is not a loaded brush model.
static model_t *PScript_TraceEntityModel(int entnum, vec3_t origin, int *modelindex)
{
	const pscript_traceent_t *ent;
	const centity_t *cent;
	model_t *mod;
	int i;

	for (i = 0, ent = NULL; i < numtraceents; i++) {
		if (traceents[i].number == entnum) {
			ent = &traceents[i];
			break;
		}
	}
	if (!ent) {
		return NULL;
	}
	cent = &cl_entities[ent->number];
	if (!cl.validsequence || cent->sequence != cl.validsequence || cent->current.modelindex != ent->modelindex) {
		return NULL;
	}
	mod = cl.model_precache[ent->modelindex];
	if (!mod || mod->needload || mod->type != mod_brush) {
		return NULL;
	}
	VectorCopy(ent->origin, origin);
	*modelindex = ent->modelindex;
	return mod;
}

// Converts a light colour channel, 1 for full brightness, to the scale of an
// lt_custom dlight, where 128 is full brightness, within the byte range.
static float PScript_LightLevel(float c)
{
	c *= 128;
	if (!(c > 0)) {
		return 0;
	}
	return min(c, 255);
}

// Returns the entry of a sound name, recording it, or NULL for new names once
// PSCRIPT_MAX_SOUNDNAMES are recorded.
static pscript_soundname_t *PScript_SoundName(const char *name)
{
	pscript_soundname_t *entry;
	int i;

	for (i = 0; i < pscript_numsoundnames; i++) {
		if (!strcmp(pscript_soundnames[i].name, name)) {
			return &pscript_soundnames[i];
		}
	}
	if (pscript_numsoundnames == PSCRIPT_MAX_SOUNDNAMES) {
		return NULL;
	}
	entry = &pscript_soundnames[pscript_numsoundnames++];
	strlcpy(entry->name, name, sizeof(entry->name));
	if (pscript_numsoundnames == PSCRIPT_MAX_SOUNDNAMES) {
		Con_Printf("Particle effects: %d sounds in use, further ones are not played\n", PSCRIPT_MAX_SOUNDNAMES);
	}
	return entry;
}

// Lets effects try the sounds that failed to load again, as a reloaded set
// may come with them.
void PScript_ForgetMissingSounds(void)
{
	int i;

	for (i = 0; i < pscript_numsoundnames; i++) {
		pscript_soundnames[i].missing = false;
	}
}

// Returns the dlight for an effect's light keyed by key, or NULL when every
// slot is lit and none holds key, rather than let CL_AllocDlight take over
// another light: emitters would leave rockets and muzzle flashes flickering.
static dlight_t *PScript_AllocDlight(int key)
{
	int i;

	for (i = 0; i < MAX_DLIGHTS; i++) {
		if ((key && cl_dlights[i].key == key) || !(cl_dlight_active[i / 32] & (1u << (i % 32)))) {
			return CL_AllocDlight(key);
		}
	}
	return NULL;
}

// Plays the dlight and a sound, picked by weight, of ptype spawning at org.
// dlkey lets an owner keep reusing one light; 0 takes a new one.
void PScript_EffectSpawned(pscript_type_t *ptype, vec3_t org, int dlkey)
{
	if (!(fabs(org[0]) < PSCRIPT_MAX_COORD && fabs(org[1]) < PSCRIPT_MAX_COORD && fabs(org[2]) < PSCRIPT_MAX_COORD)) {
		return;
	}

	if (ptype->dl_radius[0] || ptype->dl_radius[1]) {
		static int flickertime;
		static unsigned int flicker;
		int now = (int)(cls.realtime * 20);
		float radius, scale;
		dlight_t *dl;
		int i;

		if (flickertime != now) {
			flickertime = now;
			flicker = rand();
		}
		// Spread the flicker over keys so trails sharing a frame don't pulse
		// in step.
		scale = r_lightflicker.value ? ((flicker + (unsigned int)dlkey * 2000u) & 0xffff) * (1.0f / 0xffff) : 0.5f;
		radius = min(ptype->dl_radius[0] + scale * ptype->dl_radius[1], PSCRIPT_MAX_COORD);

		dl = radius > 0 ? PScript_AllocDlight(dlkey) : NULL;
		if (dl) {
			VectorCopy(org, dl->origin);
			dl->radius = radius;
			// A negative time expires at the next decay; NaN never would.
			dl->die = cl.time + (isnan(ptype->dl_time) ? 0 : ptype->dl_time);
			dl->decay = ptype->dl_decay;
			dl->type = lt_custom;
			for (i = 0; i < 3; i++) {
				dl->fadecolor[i] = PScript_LightLevel(ptype->dl_rgb[i]);
				dl->color[i] = (byte)dl->fadecolor[i];
				dl->channelfade[i] = ptype->dl_rgbfade[i] * 128;
				if (dl->channelfade[i]) {
					dl->colorfade = true;
				}
			}
		}
	}

	if (ptype->numsounds > 0 && ptype->sounds) {
		float pick, total;
		int i;

		for (i = 0, total = 0; i < ptype->numsounds; i++) {
			total += ptype->sounds[i].weight;
		}
		pick = frandom() * total;
		for (i = 0, total = 0; i < ptype->numsounds; i++) {
			pscript_sound_t *sound = &ptype->sounds[i];

			total += sound->weight;
			if (pick <= total) {
				pscript_soundname_t *entry = sound->name[0] && sound->vol > 0 ? PScript_SoundName(sound->name) : NULL;

				// s_restart frees every sfx_t, so the sound is looked up by
				// name each time. A missing file is searched for and reported
				// on every try, so one that failed is skipped. S_PrecacheSound
				// has tried unless s_precache is off.
				if (entry && !entry->missing) {
					sfx_t *sfx = S_PrecacheSound(sound->name);

					if (sfx && !sfx->buf && (s_precache.value || !S_LoadSound(sfx))) {
						entry->missing = true;
					}
					else if (sfx) {
						// The mixer works in bytes; attenuation from the
						// network is at most 255 / 64.
						S_StartSound(-1, 0, sfx, org, min(sound->vol, 1), sound->atten > 0 ? min(sound->atten, 4) : 0);
					}
				}
				break;
			}
		}
	}
}

// Sets rgba to the colour and alpha a new instance of ptype starts with.
// dieofs is how much of its life the instance skips, so its per-second
// changes are applied up front; cidxoffset shifts the palette index.
static void PScript_InitColour(const pscript_type_t *ptype, pscript_vec4_t rgba, float dieofs, int cidxoffset)
{
	const byte *palette = (const byte *)d_8to24table;
	vec3_t sync;
	float shared;
	int i;

	rgba[3] = ptype->alpha;
	if (ptype->die) {
		rgba[3] += dieofs * ptype->alphachange;
	}
	rgba[3] += ptype->alpharand * frandom();

	if (ptype->colorindex >= 0) {
		// Unsigned, so a large colorindex from a config wraps instead of
		// overflowing.
		unsigned int cidx = (unsigned int)ptype->colorindex + (unsigned int)cidxoffset;

		if (ptype->colorrand > 0) {
			cidx += rand() % ptype->colorrand;
		}
		if (cidx > 255) {
			rgba[3] /= 2; // Hexen 2 style transparency
		}
		cidx = (cidx & 0xff) * 4;
		rgba[0] = palette[cidx] * (1 / 255.0f);
		rgba[1] = palette[cidx + 1] * (1 / 255.0f);
		rgba[2] = palette[cidx + 2] * (1 / 255.0f);
	}
	else {
		VectorCopy(ptype->rgb, rgba);
	}

	shared = frandom();
	for (i = 0; i < 3; i++) {
		sync[i] = shared * ptype->rgbrandsync[i] + frandom() * (1 - ptype->rgbrandsync[i]);
		rgba[i] += sync[i] * ptype->rgbrand[i] + ptype->rgbchange[i] * dieofs;
	}
}

// PScript_SpawnParticle for a particle that starts age seconds old: the age
// counts as life already used, and it has scaled and turned for that long.
static inline pscript_particle_t *PScript_SpawnAgedParticle(pscript_type_t *ptype, int cidxoffset, float age)
{
	pscript_particle_t *p = pscript_free_particles;
	float dieofs;

	if (!p) {
		return NULL;
	}
	pscript_free_particles = p->next;
	p->next = ptype->particles;
	ptype->particles = p;

	dieofs = ptype->randdie * frandom();
	if (age > 0) {
		dieofs += age;
	}
	p->scale = ptype->scale + ptype->scalerand * frandom();
	// A ramp sets or changes the scale itself.
	if (age > 0 && ptype->rampmode == RAMP_NONE) {
		p->scale += ptype->scaledelta * age;
	}
	PScript_InitColour(ptype, p->rgba, dieofs, cidxoffset);
	if (ptype->emittime < 0) {
		p->state.trailstate = 0;
	}
	else {
		p->state.nextemit = pscript_particletime + ptype->emitstart - dieofs;
	}

	p->rotationspeed = ptype->rotationmin + frandom() * ptype->rotationrand;
	p->angle = ptype->rotationstartmin + frandom() * ptype->rotationstartrand;
	if (age > 0) {
		p->angle += p->rotationspeed * age;
	}
	p->s1 = ptype->s1;
	p->t1 = ptype->t1;
	p->s2 = ptype->s2;
	p->t2 = ptype->t2;
	if (ptype->randsmax > 1) {
		float offset = ptype->texsstride * (rand() % ptype->randsmax);

		p->s1 += offset;
		p->s2 += offset;
	}

	p->die = pscript_particletime + ptype->die - dieofs;
	PScript_ApplyRamp(ptype, p->die, 0, p->rgba, &p->scale);

	return p;
}

// Takes a particle from the free list onto ptype's list, NULL when it is
// empty, and sets all but org, oldorg and vel. Absolute ramps apply at once,
// as it may be drawn before it is simulated. The caller links the run list.
pscript_particle_t *PScript_SpawnParticle(pscript_type_t *ptype, int cidxoffset)
{
	return PScript_SpawnAgedParticle(ptype, cidxoffset, 0);
}

// Below this friction times age, the move with friction loses its precision
// to the division by friction, so the move without, its limit, is used.
#define PSCRIPT_MIN_FRICTIONAGE 1e-6

// Moves a particle of ptype, spawned age seconds old, from where it spawned
// to where gravity and friction have taken it since; flurry is ignored. Only
// a particle with life left moves. False when it must not appear: its life
// is over, the move hit a surface, so it would have landed already, or the
// result is not finite.
static qbool PScript_PreageParticle(const pscript_type_t *ptype, pscript_particle_t *p, float age)
{
	vec3_t start, impact, normal;
	double accel, f, x, e, vinf, v;
	int i;

	if (!isfinite(p->die)) {
		return false;
	}
	// Die 0 particles live one frame whatever their die time says, and
	// have used up the life in which they would move.
	if (!ptype->die) {
		return true;
	}
	if (!(p->die >= pscript_particletime)) {
		return false;
	}
	if (!(ptype->flags & PT_VELOCITY)) {
		return true;
	}

	VectorCopy(p->org, start);
	for (i = 0; i < 3; i++) {
		accel = i == 2 ? -ptype->gravity : 0;
		f = (ptype->flags & PT_FRICTION) ? ptype->friction[i] : 0;
		x = f * age;
		v = p->vel[i];
		if (fabs(x) > PSCRIPT_MIN_FRICTIONAGE) {
			// The velocity decays towards vinf, where friction cancels
			// gravity.
			e = exp(-x);
			vinf = accel / f;
			p->org[i] += vinf * age + (v - vinf) * (1 - e) / f;
			p->vel[i] = vinf + (v - vinf) * e;
		}
		else {
			p->org[i] += v * age + 0.5 * accel * age * age;
			p->vel[i] = v + accel * age;
		}
		if (!isfinite(p->org[i]) || !isfinite(p->vel[i])) {
			return false;
		}
	}

	return !(PART_VALID(ptype->cliptype) && r_bouncysparks.integer && PScript_TraceLine(start, p->org, impact, normal, NULL) < 1);
}

// Takes a beam segment for p from the free list, which must not be empty, and
// appends it after last, or stores it in *first when last is NULL. texture_s
// is its place along the beam's texture, dir the direction a chain starts in.
pscript_beamseg_t *PScript_AppendBeamSeg(pscript_beamseg_t **first, pscript_beamseg_t *last, pscript_particle_t *p, float texture_s, const vec3_t dir)
{
	pscript_beamseg_t *seg = pscript_free_beams;

	pscript_free_beams = seg->next;
	if (last) {
		last->next = seg;
	}
	else {
		*first = seg;
	}
	seg->p = p;
	seg->flags = 0;
	seg->texture_s = texture_s;
	VectorCopy(dir, seg->dir);
	return seg;
}

// Clip callback: turns each triangle into a decal of the context's type until
// the free list is empty.
static void PScript_AddDecals(void *vctx, vec3_t *points, size_t numtris)
{
	pscript_decalctx_t *ctx = vctx;
	pscript_type_t *ptype = ctx->ptype;
	pscript_clippeddecal_t *d;
	vec3_t vec;
	float dieofs;
	qbool added = false;
	int i;

	for (; numtris > 0 && pscript_free_decals; numtris--, points += 3) {
		d = pscript_free_decals;
		pscript_free_decals = d->next;
		d->next = ptype->clippeddecals;
		ptype->clippeddecals = d;
		d->entity = (unsigned short)ctx->entity;
		d->modelindex = (unsigned short)ctx->modelindex;

		for (i = 0; i < 3; i++) {
			VectorCopy(points[i], d->vertex[i]);
			VectorSubtract(d->vertex[i], ctx->center, vec);
			d->texcoords[i][0] = DotProduct(vec, ctx->tangent1) * ctx->scale1 + ctx->bias1;
			d->texcoords[i][1] = DotProduct(vec, ctx->tangent2) * ctx->scale2 + ctx->bias2;
			// The clipper already rejects surfaces facing away, so without
			// the fade a decal whose center is off the surface stays opaque.
			if (r_decal_noperpendicular.value) {
				d->valpha[i] = 1;
			}
			else {
				d->valpha[i] = 1 - fabs(DotProduct(vec, ctx->normal) * ctx->scale0);
			}
		}

		dieofs = ptype->randdie * frandom();
		PScript_InitColour(ptype, d->rgba, dieofs, 0);
		d->die = pscript_particletime + ptype->die - dieofs;
		// Particles that leave decals (clipbounce -2) keep them for 20 s
		// more than their own life.
		if (ptype->looks.type != PT_CDECAL) {
			d->die += 20;
		}
		PScript_ApplyRamp(ptype, d->die, 0, d->rgba, NULL);
		added = true;
	}

	if (added) {
		PScript_LinkRunList(ptype);
	}
}

// Spawns a decal of ptype clipped to the surfaces around center, size units
// across, at most PSCRIPT_MAX_DECALSIZE. normal points out of the surface;
// randomslot picks a random atlas slot. entnum is the brush entity
// PScript_TraceLine found the surface on, 0 for the world: the decal is
// clipped to the entity's model and moves with it, or to the world when the
// entity has gone since.
void PScript_SpawnClippedDecal(pscript_type_t *ptype, vec3_t center, vec3_t normal, float size, qbool randomslot, int entnum)
{
	static const vec3_t seed = { 0.5f, 0.5f, 0.5f };
	static const vec3_t altseed = { 0.5f, 0.5f, 0.431f };
	pscript_decalctx_t ctx;
	model_t *mod = NULL;
	vec3_t origin;

	if (!(size > 0) || !pscript_free_decals) {
		return;
	}
	size = min(size, PSCRIPT_MAX_DECALSIZE);

	ctx.ptype = ptype;
	ctx.entity = 0;
	ctx.modelindex = 0;
	if (entnum) {
		mod = PScript_TraceEntityModel(entnum, origin, &ctx.modelindex);
	}
	if (mod) {
		// Entities are traced unrotated, so only the centre moves to their
		// space.
		ctx.entity = entnum;
		VectorSubtract(center, origin, ctx.center);
	}
	else {
		mod = cl.worldmodel;
		VectorCopy(center, ctx.center);
	}
	VectorNegate(normal, ctx.normal);
	if (!(VectorNormalize(ctx.normal) > 0)) {
		return;
	}

	// Random rotation around the normal, so repeated decals don't all
	// line up.
	CrossProduct(ctx.normal, seed, ctx.tangent1);
	if (DotProduct(ctx.tangent1, ctx.tangent1) < 0.0001f) {
		CrossProduct(ctx.normal, altseed, ctx.tangent1);
	}
	RotatePointAroundVector(ctx.tangent2, ctx.normal, ctx.tangent1, frandom() * 360);
	CrossProduct(ctx.normal, ctx.tangent2, ctx.tangent1);
	VectorNormalize(ctx.tangent1);
	VectorNormalize(ctx.tangent2);

	ctx.scale1 = ptype->s2 - ptype->s1;
	ctx.bias1 = ptype->s1 + ctx.scale1 / 2;
	ctx.scale2 = ptype->t2 - ptype->t1;
	ctx.bias2 = ptype->t1 + ctx.scale2 / 2;
	ctx.scale0 = 2.0f / size;
	ctx.scale1 /= size;
	ctx.scale2 /= size;
	if (randomslot && ptype->randsmax > 1) {
		ctx.bias1 += ptype->texsstride * (rand() % ptype->randsmax);
	}

	PScript_ClipDecal(mod, ctx.center, ctx.normal, ctx.tangent2, ctx.tangent1, size, ptype->surfflagmask, ptype->surfflagmatch, PScript_AddDecals, &ctx);
}

static qbool PScript_HasDirection(const vec3_t dir)
{
	return dir && (dir[0] || dir[1] || dir[2]);
}

// Sets axis to the frame an effect spawns in, its third vector along dir,
// when the effect has a direction, and returns how much dir scales veladd:
// its length, or 1 when it is NULL.
static float PScript_EffectAxis(vec3_t dir, vec3_t axis[3])
{
	if (!dir) {
		return 1;
	}
	if (PScript_HasDirection(dir)) {
		VectorCopy(dir, axis[2]);
		VectorNormalize(axis[2]);
		PerpendicularVector(axis[0], axis[2]);
		VectorNormalize(axis[0]);
		CrossProduct(axis[2], axis[0], axis[1]);
		VectorNormalize(axis[1]);
	}
	return VectorLength(dir);
}

// Finds the center and normal of a clipped decal spawned at org: the surface
// within 16 units along dir, which points away from it, when there is a
// direction, else the nearest one along the six axes. Returns the number of
// the brush entity the surface is on, 0 for the world or none.
static int PScript_FindDecalSurface(vec3_t org, vec3_t dir, vec3_t center, vec3_t normal)
{
	vec3_t start, end, offset, impact, hitnormal;
	float fraction, best;
	int i, entnum, hitent = 0;

	if (PScript_HasDirection(dir)) {
		// Normalised, so a velocity used as the direction does not stretch
		// the trace far past org.
		VectorCopy(dir, normal);
		VectorNormalize(normal);
		PS_VectorMA(org, 16, normal, start);
		PS_VectorMA(org, -16, normal, end);
		PScript_TraceLine(start, end, center, hitnormal, &hitent);
		return hitent;
	}

	VectorCopy(org, center);
	VectorSet(normal, 0, 0.73f, 0.73f);
	VectorNormalize(normal);
	best = 1;
	for (i = 0; i < 6; i++) {
		VectorClear(offset);
		offset[i % 3] = i < 3 ? -16 : 16;
		VectorSubtract(org, offset, start);
		VectorAdd(org, offset, end);
		fraction = PScript_TraceLine(start, end, impact, hitnormal, &entnum);
		if (fraction < best) {
			best = fraction;
			VectorCopy(hitnormal, normal);
			VectorCopy(impact, center);
			hitent = entnum;
		}
	}
	return hitent;
}

// Clamps an areaspread for the integer grid of the teleport and lava splash
// patterns, so stepping through it cannot overflow.
static float PScript_GridSpread(float spread)
{
	if (!isfinite(spread)) {
		return 0;
	}
	return bound(-(1 << 20), spread, 1 << 20);
}

// The dlight key of effects run with trail state ts, 0 without one. Each
// state has its own, so an emitter keeps reusing one light, above every
// entity number and its negation, which the engine keys its lights by.
static int PScript_StateLightKey(const pscript_trailstate_t *ts)
{
	return ts ? CL_MAX_EDICTS + (int)(ts->key & PSCRIPT_TRAILKEY_INDEXMASK) : 0;
}

// Spawns one type of a point effect's assoc chain in axis, whose third vector
// is the effect direction; velscale scales veladd. ts may be NULL. A decal
// type stores the normal of the surface it lands on in surfnormal. Each
// particle starts agemin plus up to agerange seconds old; see
// PScript_RunAgedEffect.
static pscript_chainstep_t PScript_RunEffectType(pscript_type_t *ptype, vec3_t org, vec3_t dir, vec3_t axis[3], float velscale, float count, pscript_trailstate_t *ts, vec3_t surfnormal, float agemin, float agerange)
{
	pscript_beamseg_t *b = NULL, *bfirst = NULL;
	pscript_particle_t *p;
	vec3_t ofsvec, arsvec;
	float pcount, m, orgadd, veladd, hspread, vspread, age;
	int i, j, k, l, spawnspc;
	qbool beam = ptype->looks.type == PT_BEAM;
	qbool aged = agemin > 0 || agerange > 0;

	if (!PScript_ContentsAllow(ptype, org)) {
		return CHAIN_NEXT;
	}

	// Aged effects stand in for emission that happened unseen, often many at
	// once, so they play no sound and light nothing.
	if (!aged) {
		PScript_EffectSpawned(ptype, org, PScript_StateLightKey(ts));
	}

	if (ptype->looks.type == PT_CDECAL) {
		vec3_t center;
		int entnum;

		if (!aged && !pscript_free_decals) {
			return CHAIN_STOP;
		}
		// An aged effect still finds the surface, which orients the rest
		// of the chain, but leaves no decal: one per stand-in call would
		// pile up on it at once.
		entnum = PScript_FindDecalSurface(org, dir, center, surfnormal);
		if (!aged) {
			PScript_SpawnClippedDecal(ptype, center, surfnormal, ptype->scale + frandom() * ptype->scalerand, true, entnum);
		}
		return PScript_HasDirection(dir) ? CHAIN_NEXT_SAME : CHAIN_NEXT_SURFACE;
	}

	// The points of a beam are drawn as one chain, which a point that would
	// have landed cannot leave.
	if (aged && beam) {
		return CHAIN_NEXT;
	}

	// A per-frame count means nothing while no time passes.
	if ((ptype->flags & PT_INVFRAMETIME) && !(pscript_frametime > 0)) {
		return CHAIN_NEXT;
	}

	pcount = ptype->countextra + count * (ptype->count + ptype->countrand * frandom());
	if (ptype->flags & PT_INVFRAMETIME) {
		pcount /= pscript_frametime;
	}
	// The carried fraction is already scaled by the density, so only the
	// new count is.
	pcount *= r_part_density.value;
	if (ts) {
		pcount += ts->u.effect.emittime;
	}
	pcount = PScript_BoundCount(pcount);

	spawnspc = 8;
	j = k = l = 0;
	m = 0;
	hspread = PScript_GridSpread(ptype->areaspread);
	vspread = PScript_GridSpread(ptype->areaspreadvert);
	switch (ptype->spawnmode) {
		case SM_UNICIRCLE:
			m = beam ? pcount - 1 : pcount;
			m = m < 1 ? 0 : (float)(2 * M_PI) / m;
			if (ptype->spawnparam1) {
				m *= ptype->spawnparam1;
			}
			// Whole turns change no m * i, so drop them: a float keeps
			// the angles apart only while they are small.
			m = isfinite(m) ? fmod(m, 2 * M_PI) : 0;
			break;
		case SM_TELEBOX:
			spawnspc = 4;
			l = -(int)vspread;
			// fall through
		case SM_LAVASPLASH:
			j = k = -(int)hspread;
			// Vanilla Quake's teleport and lava splash speed spread.
			m = ptype->spawnparam1 ? ptype->spawnparam1 : 0.55752f;
			if (ptype->spawnparam2) {
				spawnspc = ptype->spawnparam2 >= 1 ? (int)min(ptype->spawnparam2, 4096) : 1;
			}
			break;
		case SM_FIELD:
			if (!avelocities[0][0]) {
				for (j = 0; j < NUMVERTEXNORMALS; j++) {
					avelocities[j][0] = (rand() & 255) * 0.01f;
					avelocities[j][1] = (rand() & 255) * 0.01f;
				}
			}
			j = 0;
			m = 0;
			break;
		default:
			break;
	}

	if (ptype->spawntime && ts) {
		if (ts->u.effect.statetime > pscript_particletime) {
			return CHAIN_STOP;
		}
		ts->u.effect.statetime = pscript_particletime + ptype->spawntime;
	}

	if (ptype->spawnchance < frandom()) {
		return CHAIN_STOP;
	}

	// Configs give die 0 types count 1 to mean one particle per call, which
	// countextra 1 and count 0 would say properly.
	if (!ptype->die && ptype->count == 1 && ptype->countrand == 0 && pcount < 1) {
		pcount = 1;
	}

	for (i = 0; i < pcount; i++) {
		if (!pscript_free_particles || (beam && !pscript_free_beams)) {
			break;
		}
		age = agerange > 0 ? agemin + agerange * frandom() : agemin;
		p = PScript_SpawnAgedParticle(ptype, 0, age);
		if (beam) {
			b = PScript_AppendBeamSeg(&bfirst, b, p, i, vec3_origin);
		}

		switch (ptype->spawnmode) {
			case SM_BOX:
				ofsvec[0] = crandom();
				ofsvec[1] = crandom();
				ofsvec[2] = crandom();
				arsvec[0] = ofsvec[0] * ptype->areaspread;
				arsvec[1] = ofsvec[1] * ptype->areaspread;
				arsvec[2] = ofsvec[2] * ptype->areaspreadvert;
				break;
			case SM_TELEBOX:
				VectorSet(ofsvec, k, j, l + 4);
				VectorNormalize(ofsvec);
				PS_VectorScale(ofsvec, 1.0f - frandom() * m, ofsvec);
				VectorSet(arsvec, j + rand() % spawnspc, k + rand() % spawnspc, l + rand() % spawnspc);
				j += spawnspc;
				if (j >= hspread) {
					j = -(int)hspread;
					k += spawnspc;
					if (k >= hspread) {
						k = -(int)hspread;
						l += spawnspc;
						if (l >= vspread) {
							l = -(int)vspread;
						}
					}
				}
				break;
			case SM_LAVASPLASH:
				VectorSet(ofsvec, k + rand() % spawnspc, j + rand() % spawnspc, 256);
				VectorSet(arsvec, ofsvec[0], ofsvec[1], frandom() * ptype->areaspreadvert);
				VectorNormalize(ofsvec);
				PS_VectorScale(ofsvec, 1.0f - frandom() * m, ofsvec);
				j += spawnspc;
				if (j >= hspread) {
					j = -(int)hspread;
					k += spawnspc;
					if (k >= hspread) {
						k = -(int)hspread;
					}
				}
				break;
			case SM_UNICIRCLE:
				VectorSet(ofsvec, PCOS(m * i), PSIN(m * i), 0);
				PS_VectorScale(ofsvec, ptype->areaspread, arsvec);
				break;
			case SM_FIELD:
			{
				float wobble;

				// j walks the vertex normals; each pass around them shifts
				// the phase.
				arsvec[0] = cl.time * avelocities[j][0] + m;
				arsvec[1] = cl.time * avelocities[j][1] + m;
				arsvec[2] = PCOS(arsvec[1]);
				ofsvec[0] = arsvec[2] * PCOS(arsvec[0]);
				ofsvec[1] = arsvec[2] * PSIN(arsvec[0]);
				ofsvec[2] = -PSIN(arsvec[1]);

				wobble = ptype->spawnparam2 * PSIN(cl.time + j + m);
				arsvec[0] = r_avertexnormals[j][0] * (ptype->areaspread + wobble) + ofsvec[0] * ptype->spawnparam1;
				arsvec[1] = r_avertexnormals[j][1] * (ptype->areaspread + wobble) + ofsvec[1] * ptype->spawnparam1;
				arsvec[2] = r_avertexnormals[j][2] * (ptype->areaspreadvert + wobble) + ofsvec[2] * ptype->spawnparam1;
				VectorNormalize(ofsvec);

				j++;
				if (j >= NUMVERTEXNORMALS) {
					j = 0;
					m += 0.1762891f;
				}
				break;
			}
			case SM_DISTBALL:
			{
				// crandom() * crandom() concentrates them near spawnparam2.
				float rdist = ptype->spawnparam2 - crandom() * (1 - crandom() * ptype->spawnparam1);

				ofsvec[0] = hrandom();
				ofsvec[1] = hrandom();
				ofsvec[2] = ptype->areaspreadvert ? hrandom() : 0;
				VectorNormalize(ofsvec);
				PS_VectorScale(ofsvec, rdist, ofsvec);
				arsvec[0] = ofsvec[0] * ptype->areaspread;
				arsvec[1] = ofsvec[1] * ptype->areaspread;
				arsvec[2] = ofsvec[2] * ptype->areaspreadvert;
				break;
			}
			default: // SM_BALL, SM_CIRCLE and the trail-only modes
				ofsvec[0] = hrandom();
				ofsvec[1] = hrandom();
				ofsvec[2] = ptype->areaspreadvert ? hrandom() : 0;
				VectorNormalize(ofsvec);
				if (ptype->spawnmode != SM_CIRCLE) {
					PS_VectorScale(ofsvec, frandom(), ofsvec);
				}
				arsvec[0] = ofsvec[0] * ptype->areaspread;
				arsvec[1] = ofsvec[1] * ptype->areaspread;
				arsvec[2] = ofsvec[2] * ptype->areaspreadvert;
				break;
		}

		orgadd = ptype->orgadd + frandom() * ptype->randomorgadd;
		veladd = (ptype->veladd + frandom() * ptype->randomveladd) * velscale;
		VectorClear(p->vel);
		PS_VectorMA(p->vel, ofsvec[0] * ptype->spawnvel, axis[0], p->vel);
		PS_VectorMA(p->vel, ofsvec[1] * ptype->spawnvel, axis[1], p->vel);
		PS_VectorMA(p->vel, veladd + ofsvec[2] * ptype->spawnvelvert, axis[2], p->vel);
		PS_VectorMA(org, arsvec[0], axis[0], p->org);
		PS_VectorMA(p->org, arsvec[1], axis[1], p->org);
		PS_VectorMA(p->org, orgadd + arsvec[2], axis[2], p->org);
		if (ptype->flags & PT_WORLDSPACERAND) {
			PScript_WorldSpaceRandom(ptype, p);
		}
		VectorAdd(p->org, ptype->orgbias, p->org);
		if (age > 0 && !PScript_PreageParticle(ptype, p, age)) {
			// Still the head of the list: nothing has spawned since.
			ptype->particles = p->next;
			p->next = pscript_free_particles;
			pscript_free_particles = p;
			continue;
		}
		VectorCopy(p->org, p->oldorg);
	}

	if (b) {
		// Drawing takes a segment's direction from the segment before it,
		// which the first segment of a ring lacks.
		if (ptype->spawnmode == SM_UNICIRCLE) {
			VectorSet(arsvec, PCOS(m * (i - 2)), PSIN(m * (i - 2)), 0);
			VectorSubtract(b->p->org, arsvec, bfirst->dir);
			VectorNormalize(bfirst->dir);
		}
		b->flags |= BS_NODRAW;
		b->next = ptype->beams;
		ptype->beams = bfirst;
	}

	if (ts) {
		ts->u.effect.emittime = pcount - i;
	}

	if (ptype->particles || ptype->clippeddecals) {
		PScript_LinkRunList(ptype);
	}

	return CHAIN_NEXT;
}

// PScript_RunEffect for particles that start as if emitted earlier: each
// agemin plus a random part of agerange seconds ago, at most the sum. Ages
// that are not finite and positive count as 0. An aged effect plays no sound
// or light, leaves no clipped decal, spawns no beam, and keeps no state
// between calls, so tk is ignored.
qbool PScript_RunAgedEffect(vec3_t org, vec3_t dir, float count, int typenum, trailkey_t *tk, float agemin, float agerange)
{
	vec3_t axis[3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, -1 } };
	vec3_t surfnormal;
	pscript_type_t *ptype;
	pscript_trailstate_t *ts;
	pscript_chainstep_t step;
	float velscale;
	int steps;

	if (!PScript_Active() || !PART_VALID(typenum) || !pscript_types[typenum].loaded) {
		return false;
	}
	ptype = PScript_InwaterType(&pscript_types[typenum], org);

	if (!(agemin > 0 && isfinite(agemin))) {
		agemin = 0;
	}
	if (!(agerange > 0 && isfinite(agerange) && isfinite(agemin + agerange))) {
		agerange = 0;
	}
	if ((ptype->flags & PT_NOSTATE) || agemin > 0 || agerange > 0) {
		tk = NULL;
	}
	ts = PScript_FetchHeadTrailstate(tk, (int)(ptype - pscript_types));

	velscale = PScript_EffectAxis(dir, axis);

	// At most one step per type, because a config can make the assoc chain
	// loop.
	for (steps = 0; steps < pscript_numtypes; steps++) {
		step = PScript_RunEffectType(ptype, org, dir, axis, velscale, count, ts, surfnormal, agemin, agerange);
		if (step == CHAIN_STOP || !PART_VALID(ptype->assoc)) {
			break;
		}
		if (step == CHAIN_NEXT_SURFACE) {
			// An effect without a direction takes the normal its decal found
			// as its direction from there on.
			dir = surfnormal;
			velscale = PScript_EffectAxis(dir, axis);
		}
		else if (ts && step == CHAIN_NEXT) {
			ts = PScript_FetchTrailstate(&ts->assoc);
		}
		ptype = &pscript_types[ptype->assoc];
	}

	return true;
}

// Spawns effect typenum and its assoc chain at org. dir orients the pattern
// and its length scales veladd; without one, the chain spawns along -z until
// a decal finds a surface normal. tk carries state between calls, or is NULL.
// False when inactive or typenum is not loaded, else true even if nothing
// spawned. Ungated by rulesets, for effects that carry no game state.
qbool PScript_RunEffect(vec3_t org, vec3_t dir, float count, int typenum, trailkey_t *tk)
{
	return PScript_RunAgedEffect(org, dir, count, typenum, tk, 0, 0);
}

// For events and moving entities; fails under rulesets that restrict
// particles.
qbool PScript_RunParticleEffectState(vec3_t org, vec3_t dir, float count, int typenum, trailkey_t *tk)
{
	return PScript_DynamicActive() && PScript_RunEffect(org, dir, count, typenum, tk);
}

// For effects on the world, on static entities and on the map's inline brush
// models, which run under every ruleset.
qbool PScript_RunWorldEffect(vec3_t org, vec3_t dir, float count, int typenum, trailkey_t *tk)
{
	return PScript_RunEffect(org, dir, count, typenum, tk);
}

// PScript_RunParticleEffectState for an effect name, without a trail state.
// The lookup may load a namespaced set, so it runs while inactive or
// restricted too.
qbool PScript_RunParticleEffectTypeString(vec3_t org, vec3_t dir, float count, const char *name)
{
	return PScript_RunParticleEffectState(org, dir, count, PScript_FindParticleType(name), NULL);
}

// PScript_RunEffect with typenum's palette colours replaced by colorindex (-1
// for its rgb) plus a random offset below colorrand. Its inwater replacement
// and assoc chain keep their own colours.
static qbool PScript_RunTinted(vec3_t org, vec3_t dir, float count, int typenum, int colorindex, int colorrand)
{
	pscript_type_t *ptype;
	int oldindex, oldrand;
	qbool ran;

	if (!PART_VALID(typenum)) {
		return false;
	}

	// Spawning reads a type's colours only while it runs and never creates
	// a type, so the pointer and the restore stay valid.
	ptype = &pscript_types[typenum];
	oldindex = ptype->colorindex;
	oldrand = ptype->colorrand;
	ptype->colorindex = colorindex;
	ptype->colorrand = colorrand;
	ran = PScript_RunEffect(org, dir, count, typenum, NULL);
	ptype->colorindex = oldindex;
	ptype->colorrand = oldrand;

	return ran;
}

// PScript_RunTinted for events; fails under rulesets that restrict particles.
qbool PScript_RunParticleEffectTinted(vec3_t org, vec3_t dir, float count, int typenum, int colorindex, int colorrand)
{
	return PScript_DynamicActive() && PScript_RunTinted(org, dir, count, typenum, colorindex, colorrand);
}

// Quake's R_RunParticleEffect: pe_<color>, else pe_size3, pe_size2 or
// pe_default by count, tinted to color's palette row. Fails while
// PScript_DynamicActive is false, as the engine runs it for events.
qbool PScript_RunParticleEffect(vec3_t org, vec3_t dir, int color, int count)
{
	char name[32];
	qbool ran;

	if (!PScript_DynamicActive()) {
		return false;
	}

	// The palette's names are looked up once per refresh.
	if (color >= 0 && color < 256) {
		ran = PScript_RunEffect(org, dir, count, pscript_pe_colors[color], NULL);
	}
	else {
		snprintf(name, sizeof(name), "pe_%i", color);
		ran = PScript_RunEffect(org, dir, count, PScript_FindParticleType(name), NULL);
	}
	if (ran) {
		return true;
	}
	if (count > 130 && PScript_RunTinted(org, dir, count, pscript_pe_size3, color & ~7, 8)) {
		return true;
	}
	if (count > 20 && PScript_RunTinted(org, dir, count, pscript_pe_size2, color & ~7, 8)) {
		return true;
	}
	return PScript_RunTinted(org, dir, count, pscript_pe_default, color & ~7, 8);
}

// Spawns count times the type's count single particles of typenum at random
// points of the box minb..maxb. With tint, colour replaces its colorindex.
static qbool PScript_RunBox(int typenum, vec3_t minb, vec3_t maxb, vec3_t dir, float count, qbool tint, int colour)
{
	pscript_type_t *ptype = &pscript_types[typenum];
	float total, invcount;
	int oldindex, i, j;
	vec3_t org;

	if (!ptype->loaded) {
		return false;
	}
	if (!(ptype->count > 0)) {
		return true;
	}

	invcount = 1 / ptype->count;
	// Spawns that produce no particle don't drain the free list, so the
	// count itself must be bounded.
	total = PScript_BoundCount(count * ptype->count);
	oldindex = ptype->colorindex;
	if (tint) {
		ptype->colorindex = colour;
	}
	for (i = 0; i < total && pscript_free_particles; i++) {
		for (j = 0; j < 3; j++) {
			org[j] = minb[j] + frandom() * (maxb[j] - minb[j]);
		}
		PScript_RunEffect(org, dir, invcount, typenum, NULL);
	}
	ptype->colorindex = oldindex;

	return true;
}

// Weather and other box effects sent by DarkPlaces and FTE servers:
// te_<efname>_<colour>, else te_<efname> or pe_default tinted to colour.
// Fails while PScript_DynamicActive is false, as these are events.
qbool PScript_RunParticleWeather(vec3_t minb, vec3_t maxb, vec3_t dir, float count, int colour, const char *efname)
{
	char name[MAX_QPATH * 2];
	qbool tint = false;
	int type;

	if (!PScript_DynamicActive()) {
		return false;
	}

	snprintf(name, sizeof(name), "te_%s_%i", efname, colour);
	type = PScript_FindParticleType(name);
	if (!PART_VALID(type)) {
		snprintf(name, sizeof(name), "te_%s", efname);
		type = PScript_FindParticleType(name);
		if (!PART_VALID(type)) {
			type = pscript_pe_default;
		}
		if (!PART_VALID(type)) {
			return false;
		}
		tint = true;
	}

	return PScript_RunBox(type, minb, maxb, dir, count, tint, colour);
}

// Particle cubes: typenum, else te_cube[_g]_<colour>, else te_cube[_g] or
// pe_default tinted to colour, _g for gravity. Fails while
// PScript_DynamicActive is false, as these are events.
qbool PScript_RunParticleCube(int typenum, vec3_t minb, vec3_t maxb, vec3_t dir, float count, int colour, qbool gravity)
{
	char name[32];
	qbool tint = false;

	if (!PScript_DynamicActive()) {
		return false;
	}

	if (!PART_VALID(typenum)) {
		snprintf(name, sizeof(name), "te_cube%s_%i", gravity ? "_g" : "", colour);
		typenum = PScript_FindParticleType(name);
	}
	if (!PART_VALID(typenum)) {
		snprintf(name, sizeof(name), "te_cube%s", gravity ? "_g" : "");
		typenum = PScript_FindParticleType(name);
		if (!PART_VALID(typenum)) {
			typenum = pscript_pe_default;
		}
		if (!PART_VALID(typenum)) {
			return false;
		}
		tint = true;
	}

	return PScript_RunBox(typenum, minb, maxb, dir, count, tint, colour);
}
