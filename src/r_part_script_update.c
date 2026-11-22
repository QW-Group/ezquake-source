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

// Scripted particles: instance pools, simulation, expiry and geometry.

#include <limits.h>
#include "quakedef.h"
#include "r_part_script_local.h"
#include "r_scenetris.h"

pscript_particle_t *pscript_free_particles;
pscript_beamseg_t *pscript_free_beams;
pscript_clippeddecal_t *pscript_free_decals;
double pscript_particletime;
float pscript_sintable[256];

static pscript_particle_t *pool_particles;
static int pool_numparticles;
static int particle_recycle;

static pscript_beamseg_t *pool_beams;

static pscript_clippeddecal_t *pool_decals;
static int pool_numdecals;
static int decal_recycle;

static float flurrytime;

// The current view's sprite axes, 1.5 times vright and vup.
static vec3_t pright, pup;

// Unclipped decals lie flat in the world's XY plane.
static const vec3_t udecal_right = { 1, 0, 0 };
static const vec3_t udecal_up = { 0, 1, 0 };

// Set when cl_ents.c draws packet entities at their latest origin instead of
// moving them between packets.
extern cvar_t cl_nolerp;
extern qbool cl_nolerp_on_entity_flag;

// Particles killed during a simulation pass. They return to the free list
// only once the pass is over, so a beam segment can still read its
// particle's die time to learn that it died.
typedef struct {
	pscript_particle_t *head;
	pscript_particle_t *tail;
} pscript_killlist_t;

// Fills the table behind PSIN and PCOS: one period in 256 steps.
void PScript_InitSinTable(void)
{
	int i;

	for (i = 0; i < 256; i++) {
		pscript_sintable[i] = sin(i * M_PI / 128);
	}
}

// Returns the index into the table behind PSIN and PCOS for an angle in
// radians. Spins and the field clock grow without bound, so angles beyond int
// range are first reduced to their place in the turn; non-finite ones give 0.
int PScript_SinIndex(double radians)
{
	double steps = radians * (128 / M_PI);

	if (!(steps > INT_MIN && steps < INT_MAX)) {
		steps = isfinite(steps) ? fmod(steps, 256) : 0;
	}
	return (int)steps & 255;
}

// Clears every type's instance lists and empties the run list, without
// returning anything to the free lists.
static void PScript_ClearTypeInstances(void)
{
	int i;

	for (i = 0; i < pscript_numtypes; i++) {
		pscript_type_t *ptype = &pscript_types[i];

		ptype->particles = NULL;
		ptype->clippeddecals = NULL;
		ptype->beams = NULL;
		ptype->expiring = NULL;
		ptype->expiringbeams = NULL;
		ptype->nexttorun = NULL;
		ptype->runlink = NULL;
		ptype->state &= ~PS_INRUNLIST;
	}
	pscript_runlist = NULL;
}

// Returns every instance to the free lists, empties the run list, restarts
// the particle clock, then resets the trail states while the beam pool exists.
void PScript_ResetPools(void)
{
	int i;

	pscript_free_particles = NULL;
	if (pool_particles) {
		for (i = 0; i < pool_numparticles - 1; i++) {
			pool_particles[i].next = &pool_particles[i + 1];
		}
		pool_particles[pool_numparticles - 1].next = NULL;
		pscript_free_particles = pool_particles;
	}

	pscript_free_decals = NULL;
	if (pool_decals) {
		for (i = 0; i < pool_numdecals - 1; i++) {
			pool_decals[i].next = &pool_decals[i + 1];
		}
		pool_decals[pool_numdecals - 1].next = NULL;
		pscript_free_decals = pool_decals;
	}

	pscript_free_beams = NULL;
	if (pool_beams) {
		for (i = 0; i < PSCRIPT_MAX_BEAMSEGS; i++) {
			pool_beams[i].p = NULL;
			pool_beams[i].flags = BS_DEAD;
			pool_beams[i].next = (i < PSCRIPT_MAX_BEAMSEGS - 1 ? &pool_beams[i + 1] : NULL);
		}
		pscript_free_beams = pool_beams;
	}

	PScript_ClearTypeInstances();
	particle_recycle = 0;
	decal_recycle = 0;
	pscript_particletime = 0;
	flurrytime = 0;
	PScript_ResetTrailstates();
}

// Allocates the pools if needed and empties them. A pool whose size cvar
// changed is reallocated, so only call this when no instance needs to survive.
void PScript_StartupPools(void)
{
	int numparticles = bound(1, r_part_maxparticles.integer, PSCRIPT_MAX_PARTICLES);
	int numdecals = bound(1, r_part_maxdecals.integer, PSCRIPT_MAX_DECALS);

	if (numparticles != pool_numparticles) {
		Q_free(pool_particles);
		pool_particles = Q_calloc(numparticles, sizeof(*pool_particles));
		pool_numparticles = numparticles;
	}
	if (numdecals != pool_numdecals) {
		Q_free(pool_decals);
		pool_decals = Q_calloc(numdecals, sizeof(*pool_decals));
		pool_numdecals = numdecals;
	}
	if (!pool_beams) {
		pool_beams = Q_calloc(PSCRIPT_MAX_BEAMSEGS, sizeof(*pool_beams));
	}

	PScript_InitSinTable();
	PScript_AllocTrailstates();
	PScript_ResetPools();
}

void PScript_FreePools(void)
{
	PScript_ClearTypeInstances();

	Q_free(pool_particles);
	Q_free(pool_beams);
	Q_free(pool_decals);
	pool_numparticles = 0;
	pool_numdecals = 0;
	pscript_free_particles = NULL;
	pscript_free_beams = NULL;
	pscript_free_decals = NULL;

	PScript_FreeTrailstates();
}

// Returns ptype's instances to the free lists before it is redefined, except
// beam segments a trail state holds as its last: those stay on ptype->beams,
// marked dead. Does not unlink ptype from the run list.
void PScript_ReleaseTypeInstances(pscript_type_t *ptype)
{
	pscript_particle_t *p;
	pscript_clippeddecal_t *d;
	pscript_beamseg_t *b, *held = NULL;

	while ((p = ptype->particles)) {
		ptype->particles = p->next;
		if (ptype->emittime < 0) {
			PScript_DelinkTrailstate(&p->state.trailstate);
		}
		p->next = pscript_free_particles;
		pscript_free_particles = p;
	}

	while ((d = ptype->clippeddecals)) {
		ptype->clippeddecals = d->next;
		d->next = pscript_free_decals;
		pscript_free_decals = d;
	}

	while ((b = ptype->beams)) {
		ptype->beams = b->next;
		if (b->flags & BS_LASTSEG) {
			b->flags |= BS_DEAD;
			b->next = held;
			held = b;
		}
		else {
			b->next = pscript_free_beams;
			pscript_free_beams = b;
		}
	}
	ptype->beams = held;
	ptype->expiring = NULL;
	ptype->expiringbeams = NULL;
}

// All zero when the pools are not allocated.
void PScript_CountInstances(const pscript_type_t *ptype, pscript_counts_t *counts)
{
	const pscript_particle_t *p;
	const pscript_beamseg_t *b;
	const pscript_clippeddecal_t *d;

	memset(counts, 0, sizeof(*counts));
	if (!pool_particles) {
		return;
	}

	for (p = ptype->particles; p; p = p->next) {
		counts->particles++;
	}
	for (b = ptype->beams; b; b = b->next) {
		counts->beams++;
	}
	for (d = ptype->clippeddecals; d; d = d->next) {
		counts->decals++;
	}
}

// All zero when the pools are not allocated.
void PScript_CountFree(pscript_counts_t *counts)
{
	const pscript_particle_t *p;
	const pscript_beamseg_t *b;
	const pscript_clippeddecal_t *d;

	memset(counts, 0, sizeof(*counts));
	if (!pool_particles) {
		return;
	}

	for (p = pscript_free_particles; p; p = p->next) {
		counts->particles++;
	}
	for (b = pscript_free_beams; b; b = b->next) {
		counts->beams++;
	}
	for (d = pscript_free_decals; d; d = d->next) {
		counts->decals++;
	}
}

// Updates the colour, alpha and, unless scale is NULL, the scale of an
// instance of ptype that dies at die, for a step of dt seconds: absolute
// ramps set their value at its age, rates add rate times dt. Nothing changes
// before its last ptype->die seconds, as in the extra 20 s of splatted decals.
void PScript_ApplyRamp(const pscript_type_t *ptype, float die, float dt, pscript_vec4_t rgba, float *scale)
{
	const pscript_ramp_t *ramp, *ramp2;
	float left = die - pscript_particletime;
	float pos, frac;
	int index;

	if (!(ptype->die > 0) || left > ptype->die) {
		return;
	}

	if (ptype->rampmode == RAMP_NONE) {
		if (dt == 0) {
			return;
		}
		if (pscript_particletime < die - ptype->die + ptype->rgbchangetime) {
			rgba[0] += dt * ptype->rgbchange[0];
			rgba[1] += dt * ptype->rgbchange[1];
			rgba[2] += dt * ptype->rgbchange[2];
		}
		rgba[3] += dt * ptype->alphachange;
		if (scale) {
			*scale += dt * ptype->scaledelta;
		}
		return;
	}

	if (!ptype->ramp || ptype->rampindexes <= 0 || (ptype->rampmode == RAMP_DELTA && dt == 0)) {
		return;
	}

	// Clamped as a float: a randdie larger than die or a non-finite die
	// would otherwise index outside the ramp.
	pos = ptype->rampindexes * (ptype->die - left) / ptype->die;
	if (!(pos > 0)) {
		pos = 0;
	}
	else if (pos > ptype->rampindexes - 1) {
		pos = ptype->rampindexes - 1;
	}
	index = (int)pos;
	ramp = &ptype->ramp[index];

	switch (ptype->rampmode) {
		case RAMP_NEAREST:
			VectorCopy(ramp->rgb, rgba);
			rgba[3] = ramp->alpha;
			if (scale) {
				*scale = ramp->scale;
			}
			break;
		case RAMP_LERP:
			ramp2 = &ptype->ramp[min(index + 1, ptype->rampindexes - 1)];
			frac = pos - index;
			PS_VectorInterpolate(ramp->rgb, frac, ramp2->rgb, rgba);
			PS_FloatInterpolate(ramp->alpha, frac, ramp2->alpha, rgba[3]);
			if (scale) {
				PS_FloatInterpolate(ramp->scale, frac, ramp2->scale, *scale);
			}
			break;
		case RAMP_DELTA:
			PS_VectorMA(rgba, dt, ramp->rgb, rgba);
			rgba[3] -= dt * ramp->alpha;
			if (scale) {
				*scale += dt * ramp->scale;
			}
			break;
		default:
			break;
	}
}

// True once a particle's die time has passed. Written so that a NaN die,
// from a hostile die or randdie, counts as dead rather than as immortal.
static qbool PScript_ParticleDead(const pscript_particle_t *p)
{
	return !(p->die >= pscript_particletime);
}

// True when a segment is dead or its particle is. A dead segment's particle
// may already belong to someone else, so the flag is tested first.
static qbool PScript_BeamSegDead(const pscript_beamseg_t *b)
{
	return (b->flags & BS_DEAD) || !b->p || PScript_ParticleDead(b->p);
}

static void PScript_KillParticle(pscript_killlist_t *kills, pscript_particle_t *p, qbool trails)
{
	if (trails) {
		PScript_DelinkTrailstate(&p->state.trailstate);
	}
	p->next = kills->head;
	kills->head = p;
	if (!kills->tail) {
		kills->tail = p;
	}
}

// When a pool is exhausted, marks the next 256 instances in pool order as
// dead so that this pass frees them and spawning can continue next frame.
static void PScript_RecycleWhenFull(void)
{
	int i;

	if (!pscript_free_decals) {
		for (i = 0; i < 256; i++) {
			pool_decals[decal_recycle].die = -1;
			if (++decal_recycle >= pool_numdecals) {
				decal_recycle = 0;
			}
		}
	}
	if (!pscript_free_particles) {
		for (i = 0; i < 256; i++) {
			pool_particles[particle_recycle].die = -1;
			if (++particle_recycle >= pool_numparticles) {
				particle_recycle = 0;
			}
		}
	}
}

// Frees the type's expired decals and ramps the others.
static void PScript_SimulateDecals(pscript_type_t *type, float dt)
{
	pscript_clippeddecal_t **link = &type->clippeddecals;
	pscript_clippeddecal_t *d;

	while ((d = *link)) {
		if (!(d->die >= pscript_particletime)) {
			*link = d->next;
			d->next = pscript_free_decals;
			pscript_free_decals = d;
			continue;
		}
		PScript_ApplyRamp(type, d->die, dt, d->rgba, NULL);
		link = &d->next;
	}
}

// Handles a particle of type whose move from oldorg hit a surface at impact,
// on the brush entity numbered entnum or, for 0, the world. Returns true when
// the particle dies of it: it splats into a decal, turns into the type's clip
// effect, or has bounced to a near stop.
static qbool PScript_ClipParticle(pscript_type_t *type, pscript_particle_t *p, vec3_t impact, vec3_t normal, int entnum, float dt)
{
	float dist, count;

	if (type->clipbounce < 0) {
		if (type->clipbounce == -2) {
			PScript_SpawnClippedDecal(type, impact, normal, p->scale * (1.5f + frandom() * 0.5f) * 0.5f, false, entnum);
		}
		return true;
	}

	if (type->cliptype == (int)(type - pscript_types)) {
		dist = -type->clipbounce * DotProduct(p->vel, normal);
		PS_VectorMA(p->vel, dist, normal, p->vel);
		VectorCopy(impact, p->org);

		// An untextured sprite that has all but stopped would otherwise sit
		// on the floor until its die time.
		return !type->texname[0] && VectorLength(p->vel) < 1000 * dt && type->looks.type == PT_NORMAL;
	}

	VectorNormalize(p->vel);
	// A negative count times a clip type's negative count spawns particles.
	// A clip type with count 0 gives an infinite or NaN count, which the
	// spawner turns into none.
	count = type->clipcount / pscript_types[type->cliptype].count;
	if (type->clipbounce) {
		PS_VectorScale(normal, type->clipbounce, normal);
		PScript_RunEffect(impact, normal, count, type->cliptype, NULL);
	}
	else {
		PScript_RunEffect(impact, p->vel, count, type->cliptype, NULL);
	}
	return true;
}

// Expires, moves, rotates, ramps, emits and collides the particles of a type
// with a non-zero die. Emitted and clip effects may add particles to the head
// of any type's list, this one included; the walk never visits them.
static void PScript_SimulateParticles(pscript_type_t *type, float dt, qbool doflurry, int *traces, pscript_killlist_t *kills)
{
	pscript_particle_t **link = &type->particles;
	pscript_particle_t *p;
	qbool trails = type->emittime < 0;
	qbool emits = PART_VALID(type->emit);
	qbool clips = PART_VALID(type->cliptype) && r_bouncysparks.integer;
	float grav = type->gravity * dt;
	vec3_t friction, oldorg, delta, impact, normal;
	int entnum;
	qbool killed;

	friction[0] = 1 - type->friction[0] * dt;
	friction[1] = 1 - type->friction[1] * dt;
	friction[2] = 1 - type->friction[2] * dt;

	while ((p = *link)) {
		if (PScript_ParticleDead(p)) {
			*link = p->next;
			PScript_KillParticle(kills, p, trails);
			continue;
		}

		VectorCopy(p->org, oldorg);
		if (type->flags & PT_VELOCITY) {
			PS_VectorMA(p->org, dt, p->vel, p->org);
			// Friction slows the velocity gravity has just added to.
			p->vel[2] -= grav;
			if (type->flags & PT_FRICTION) {
				p->vel[0] *= friction[0];
				p->vel[1] *= friction[1];
				p->vel[2] *= friction[2];
			}
			if (type->flurry && doflurry) {
				p->vel[0] += crandom() * type->flurry;
				p->vel[1] += crandom() * type->flurry;
			}
		}

		p->angle += p->rotationspeed * dt;
		PScript_ApplyRamp(type, p->die, dt, p->rgba, &p->scale);

		if (emits) {
			if (trails) {
				PScript_RunTrail(oldorg, p->org, type->emit, dt, 0, &p->state.trailstate);
			}
			else if (p->state.nextemit < pscript_particletime) {
				p->state.nextemit = pscript_particletime + type->emittime + frandom() * type->emitrand;
				PScript_RunEffect(p->org, p->vel, 1, type->emit, NULL);
			}
		}

		killed = false;
		if (clips) {
			// Moves are traced from the last traced position once they add
			// up to more than 10 units, which spares slow particles a trace
			// every frame without letting them pass through anything. Types
			// with clipbounce 0 trace every move, so that particles which
			// settle stay on the surface.
			VectorSubtract(p->org, p->oldorg, delta);
			if (!type->clipbounce || DotProduct(delta, delta) > 10 * 10) {
				if (*traces > 0) {
					--*traces;
					if (PScript_TraceLine(p->oldorg, p->org, impact, normal, &entnum) < 1) {
						killed = PScript_ClipParticle(type, p, impact, normal, entnum, dt);
					}
				}
				VectorCopy(p->org, p->oldorg);
			}
		}

		if (killed) {
			// The die time is how this type's beam segments learn of it.
			p->die = -1;
			// Effects spawned above may have been linked in front of p when
			// p was the head of the list.
			while (*link != p) {
				link = &(*link)->next;
			}
			*link = p->next;
			PScript_KillParticle(kills, p, trails);
		}
		else {
			link = &p->next;
		}
	}
}

// Frees dead beam segments and marks those whose particle died. Segments a
// trail state holds as its last segment are never freed.
static void PScript_SimulateBeams(pscript_type_t *type)
{
	pscript_beamseg_t *b, *bkill;

	while ((b = type->beams) && !(b->flags & BS_LASTSEG) && PScript_BeamSegDead(b)) {
		type->beams = b->next;
		b->next = pscript_free_beams;
		pscript_free_beams = b;
	}

	for (; b; b = b->next) {
		// A segment that is drawn to the next one keeps a dead next segment:
		// it ends the chain, and without it the segment would be drawn to
		// the start of another chain.
		if (b->flags & (BS_LASTSEG | BS_DEAD | BS_NODRAW)) {
			while ((bkill = b->next) && (bkill->flags & BS_DEAD) && !(bkill->flags & BS_LASTSEG)) {
				b->next = bkill->next;
				bkill->next = pscript_free_beams;
				pscript_free_beams = bkill;
			}
		}
		if (PScript_BeamSegDead(b)) {
			b->flags |= BS_DEAD;
		}
	}
}

// Advances every instance in the run list by dt seconds, then the particle
// clock. Once per host frame, before the first view is built. Particles and
// beams of die 0 types are left to PScript_MarkFrameParticles.
void PScript_Simulate(float dt)
{
	pscript_killlist_t kills = { NULL, NULL };
	pscript_type_t *type, *next;
	int traces = r_particle_tracelimit.integer;
	qbool doflurry;

	if (!pool_particles || !pool_decals || !pool_beams) {
		return;
	}

	flurrytime -= dt;
	doflurry = (flurrytime < 0);
	if (doflurry) {
		flurrytime = 0.1f + frandom() * 0.3f;
	}

	PScript_RecycleWhenFull();

	for (type = pscript_runlist; type; type = next) {
		PScript_SimulateDecals(type, dt);
		if (type->die != 0) {
			PScript_SimulateParticles(type, dt, doflurry, &traces, &kills);
			PScript_SimulateBeams(type);
		}

		next = type->nexttorun;
		if (!type->particles && !type->beams && !type->clippeddecals) {
			PScript_UnlinkRunList(type);
		}
	}

	if (kills.head) {
		kills.tail->next = pscript_free_particles;
		pscript_free_particles = kills.head;
	}

	pscript_particletime += dt;
}

// Die 0 particles and beam segments live one frame: marked here at the end of
// a host frame that advanced time and freed when the next such frame begins,
// so every view draws them once and a paused view keeps them. Every type is
// marked before emit effects run, so what they spawn is drawn next frame.
void PScript_MarkFrameParticles(void)
{
	pscript_type_t *type;
	pscript_particle_t *p;

	for (type = pscript_runlist; type; type = type->nexttorun) {
		if (type->die == 0) {
			type->expiring = type->particles;
			type->expiringbeams = type->beams;
		}
	}

	for (type = pscript_runlist; type; type = type->nexttorun) {
		if (PART_VALID(type->emit) && type->emitstart <= 0) {
			for (p = type->expiring; p; p = p->next) {
				PScript_RunEffect(p->org, p->vel, pscript_frametime, type->emit, NULL);
			}
		}
	}
}

// Frees what PScript_MarkFrameParticles marked, except beam segments trail
// states hold as their last, which are marked dead. Runs after the frame's
// temp entities were parsed, so instances spawned since the mark stay.
void PScript_ExpireFrameParticles(void)
{
	pscript_type_t *type, *next;
	pscript_particle_t **plink, *p;
	pscript_beamseg_t **blink, *b;

	if (!pool_particles || !pool_beams) {
		return;
	}

	for (type = pscript_runlist; type; type = next) {
		next = type->nexttorun;
		if (!type->expiring && !type->expiringbeams) {
			continue;
		}

		plink = &type->particles;
		while (*plink && *plink != type->expiring) {
			plink = &(*plink)->next;
		}
		while ((p = *plink)) {
			*plink = p->next;
			p->next = pscript_free_particles;
			pscript_free_particles = p;
		}

		blink = &type->beams;
		while (*blink && *blink != type->expiringbeams) {
			blink = &(*blink)->next;
		}
		while ((b = *blink)) {
			if (b->flags & BS_LASTSEG) {
				b->flags |= BS_DEAD;
				blink = &b->next;
				continue;
			}
			*blink = b->next;
			b->next = pscript_free_beams;
			pscript_free_beams = b;
		}

		type->expiring = NULL;
		type->expiringbeams = NULL;
		if (!type->particles && !type->beams && !type->clippeddecals) {
			PScript_UnlinkRunList(type);
		}
	}
}

// Returns the length of v and stores v scaled to unit length in out, or the
// zero vector when v is zero.
static float VectorNormalize2(const vec3_t v, vec3_t out)
{
	float length = sqrt(DotProduct(v, v));

	if (length) {
		PS_VectorScale(v, 1 / length, out);
	}
	else {
		VectorClear(out);
	}
	return length;
}

// The colour of an instance's vertices: premultiplied looks scale rgb by
// alpha, and additive ones (premul 2) also drop the alpha.
static void PScript_VertexColour(const pscript_looks_t *looks, const float *rgba, float *colour)
{
	float alpha;

	if (!looks->premul) {
		colour[0] = rgba[0];
		colour[1] = rgba[1];
		colour[2] = rgba[2];
		colour[3] = rgba[3];
		return;
	}

	alpha = min(rgba[3], 1);
	colour[0] = rgba[0] * alpha;
	colour[1] = rgba[1] * alpha;
	colour[2] = rgba[2] * alpha;
	colour[3] = (looks->premul == 2 ? 0 : alpha);
}

static void PScript_SetVertex(scenetris_vert_t *v, const vec3_t position, float s, float t, const float *colour)
{
	VectorCopy(position, v->position);
	v->texcoord[0] = s;
	v->texcoord[1] = t;
	v->color[0] = colour[0];
	v->color[1] = colour[1];
	v->color[2] = colour[2];
	v->color[3] = colour[3];
}

// Room for one quad of type, or NULL when the view is full. The quad's
// corners 0 to 3 go into vertices 0, 1, 2 and 5; PScript_CloseQuad then
// completes the triangles 0 1 2 and 0 2 3.
static scenetris_vert_t *PScript_AllocQuad(const pscript_type_t *type, int flags)
{
	return R_SceneTrisAlloc(type->looks.texture, (scenetris_blend_t)type->looks.blendmode, flags, 6);
}

static void PScript_CloseQuad(scenetris_vert_t *v)
{
	v[3] = v[0];
	v[4] = v[2];
}

// The size of a sprite or fan spark that grows with distance from the view,
// for looks whose scalefactor is not 1.
static float PScript_DistanceScale(const pscript_looks_t *looks, const pscript_particle_t *p, float base, float perunit)
{
	float scale = (p->org[0] - r_origin[0]) * vpn[0] + (p->org[1] - r_origin[1]) * vpn[1] + (p->org[2] - r_origin[2]) * vpn[2];

	scale = scale * p->scale * looks->invscalefactor + p->scale * (looks->scalefactor * 250);
	return (scale < 20 ? base : base + scale * perunit);
}

// A quad around p->org in the plane of right and up, its corners scale times
// those axes away from it and turned by the particle's angle.
static qbool PScript_BuildSprite(const pscript_type_t *type, const pscript_particle_t *p, float scale, const vec3_t right, const vec3_t up, int flags)
{
	scenetris_vert_t *v = PScript_AllocQuad(type, flags);
	float colour[4], x, y;
	vec3_t corner;

	if (!v) {
		return false;
	}

	if (p->angle) {
		x = PSIN(p->angle) * scale;
		y = PCOS(p->angle) * scale;
	}
	else {
		x = 0;
		y = scale;
	}

	PScript_VertexColour(&type->looks, p->rgba, colour);
	PS_VectorMA(p->org, -x, right, corner);
	PS_VectorMA(corner, -y, up, corner);
	PScript_SetVertex(&v[0], corner, p->s1, p->t1, colour);
	PS_VectorMA(p->org, -y, right, corner);
	PS_VectorMA(corner, x, up, corner);
	PScript_SetVertex(&v[1], corner, p->s1, p->t2, colour);
	PS_VectorMA(p->org, x, right, corner);
	PS_VectorMA(corner, y, up, corner);
	PScript_SetVertex(&v[2], corner, p->s2, p->t2, colour);
	PS_VectorMA(p->org, y, right, corner);
	PS_VectorMA(corner, -x, up, corner);
	PScript_SetVertex(&v[5], corner, p->s2, p->t1, colour);
	PScript_CloseQuad(v);
	return true;
}

// A quad stretched along the particle's velocity, facing the view.
static qbool PScript_BuildTexturedSpark(const pscript_type_t *type, const pscript_particle_t *p)
{
	scenetris_vert_t *v = PScript_AllocQuad(type, 0);
	float colour[4], halfscale, length;
	vec3_t movedir, end, toview, side, corner;

	if (!v) {
		return false;
	}

	halfscale = p->scale * 0.5f;
	length = VectorNormalize2(p->vel, movedir);
	if (type->looks.stretch < 0) {
		length = -type->looks.stretch;
	}
	else {
		length *= (type->looks.stretch ? type->looks.stretch : 0.05f);
	}
	if (length < halfscale * type->looks.minstretch) {
		length = halfscale * type->looks.minstretch;
	}

	PScript_VertexColour(&type->looks, p->rgba, colour);

	PS_VectorMA(p->org, -length, movedir, end);
	VectorSubtract(r_origin, end, toview);
	CrossProduct(toview, p->vel, side);
	VectorNormalize(side);
	PS_VectorMA(end, -halfscale, side, corner);
	PScript_SetVertex(&v[0], corner, p->s1, p->t1, colour);
	PS_VectorMA(end, halfscale, side, corner);
	PScript_SetVertex(&v[1], corner, p->s1, p->t2, colour);

	PS_VectorMA(p->org, length, movedir, end);
	VectorSubtract(r_origin, end, toview);
	CrossProduct(toview, p->vel, side);
	VectorNormalize(side);
	PS_VectorMA(end, halfscale, side, corner);
	PScript_SetVertex(&v[2], corner, p->s2, p->t2, colour);
	PS_VectorMA(end, -halfscale, side, corner);
	PScript_SetVertex(&v[5], corner, p->s2, p->t1, colour);
	PScript_CloseQuad(v);
	return true;
}

// A triangle from p->org back along the particle's velocity, facing the view.
static qbool PScript_BuildFanSpark(const pscript_type_t *type, const pscript_particle_t *p)
{
	scenetris_vert_t *v = R_SceneTrisAlloc(type->looks.texture, (scenetris_blend_t)type->looks.blendmode, 0, 3);
	float colour[4], scale;
	vec3_t end, toview, side, corner;

	if (!v) {
		return false;
	}

	scale = PScript_DistanceScale(&type->looks, p, 0.05f, 0.0001f);
	PScript_VertexColour(&type->looks, p->rgba, colour);

	PS_VectorMA(p->org, -scale, p->vel, end);
	VectorSubtract(r_origin, end, toview);
	CrossProduct(toview, p->vel, side);
	VectorNormalize(side);

	PScript_SetVertex(&v[0], p->org, p->s1, p->t1, colour);
	PS_VectorMA(end, -p->scale, side, corner);
	PScript_SetVertex(&v[1], corner, p->s1, p->t2, colour);
	PS_VectorMA(end, p->scale, side, corner);
	PScript_SetVertex(&v[2], corner, p->s2, p->t1, colour);
	return true;
}

// An untextured line from p->org back along a tenth of the velocity, fading
// out towards its end.
static qbool PScript_BuildLineSpark(const pscript_type_t *type, const pscript_particle_t *p)
{
	scenetris_vert_t *v = R_SceneTrisAlloc(null_texture_reference, (scenetris_blend_t)type->looks.blendmode, SCENETRIS_LINES, 2);
	float head[4], tail[4];
	vec3_t end;

	if (!v) {
		return false;
	}

	PScript_VertexColour(&type->looks, p->rgba, head);
	if (type->looks.premul) {
		tail[0] = tail[1] = tail[2] = 0;
	}
	else {
		VectorCopy(p->rgba, tail);
	}
	tail[3] = 0;

	PS_VectorMA(p->org, -0.1f, p->vel, end);
	PScript_SetVertex(&v[0], p->org, p->s1, p->t1, head);
	PScript_SetVertex(&v[1], end, p->s2, p->t2, tail);
	return true;
}

// One corner of a beam quad, at along on the coordinate that runs along the
// beam, and on the edge of p's texture rectangle on its side of the beam.
static void PScript_SetBeamVertex(const pscript_looks_t *looks, scenetris_vert_t *v, const vec3_t position, float along, const pscript_particle_t *p, qbool positive, const float *colour)
{
	if (looks->vbeam) {
		PScript_SetVertex(v, position, positive ? p->s2 : p->s1, along, colour);
	}
	else {
		PScript_SetVertex(v, position, along, positive ? p->t2 : p->t1, colour);
	}
}

// A quad from segment b to the next segment c, each end as wide as its
// particle's scale across the given direction and the line of sight. Beams
// take their particles' colours as they are, even with a premultiplied look.
static qbool PScript_BuildBeamSegment(const pscript_type_t *type, const pscript_beamseg_t *b, const pscript_beamseg_t *c, const vec3_t bdir, const vec3_t cdir)
{
	const pscript_particle_t *p = b->p;
	const pscript_particle_t *q = c->p;
	scenetris_vert_t *v = PScript_AllocQuad(type, 0);
	float s;
	vec3_t toview, side, corner;

	if (!v) {
		return false;
	}

	VectorSubtract(r_origin, q->org, toview);
	VectorNormalize(toview);
	CrossProduct(cdir, toview, side);
	VectorNormalize(side);
	s = c->texture_s * q->angle + pscript_particletime * q->rotationspeed;
	PS_VectorMA(q->org, -q->scale, side, corner);
	PScript_SetBeamVertex(&type->looks, &v[0], corner, s, p, false, q->rgba);
	PS_VectorMA(q->org, q->scale, side, corner);
	PScript_SetBeamVertex(&type->looks, &v[1], corner, s, p, true, q->rgba);

	VectorSubtract(r_origin, p->org, toview);
	VectorNormalize(toview);
	CrossProduct(bdir, toview, side);
	VectorNormalize(side);
	s = b->texture_s * p->angle + pscript_particletime * p->rotationspeed;
	PS_VectorMA(p->org, p->scale, side, corner);
	PScript_SetBeamVertex(&type->looks, &v[2], corner, s, p, true, p->rgba);
	PS_VectorMA(p->org, -p->scale, side, corner);
	PScript_SetBeamVertex(&type->looks, &v[5], corner, s, p, false, p->rgba);
	PScript_CloseQuad(v);
	return true;
}

// Draws each segment to the next one when it is neither dead nor the end of
// its chain and the next one is alive. A segment's direction is the one from
// the segment drawn to it, or the direction stored at spawn when it starts a
// chain.
static void PScript_BuildBeams(const pscript_type_t *type)
{
	const pscript_beamseg_t *b = type->beams;
	const pscript_beamseg_t *c;
	vec3_t bdir, cdir;

	if (!b) {
		return;
	}

	VectorCopy(b->dir, bdir);
	for (; b; b = c) {
		c = b->next;
		if (!c || (b->flags & (BS_LASTSEG | BS_DEAD | BS_NODRAW)) || (c->flags & BS_DEAD) || !b->p || !c->p) {
			if (c) {
				VectorCopy(c->dir, bdir);
			}
			continue;
		}

		VectorSubtract(c->p->org, b->p->org, cdir);
		VectorNormalize(cdir);
		if (!PScript_BuildBeamSegment(type, b, c, bdir, cdir)) {
			return;
		}
		VectorCopy(cdir, bdir);
	}
}

// Sets origin to where the brush entity numbered entnum is drawn this frame,
// moved from its previous packet's origin to its latest as CL_LinkPacketEntities
// moves brush models, and returns true when the entity is in the latest packet
// with model modelindex, unrotated, at a finite origin.
static qbool PScript_DecalEntityOrigin(int entnum, int modelindex, vec3_t origin)
{
	const centity_t *cent = &cl_entities[entnum];
	double time;
	float lerp;

	if (!cl.validsequence || cent->sequence != cl.validsequence || cent->current.modelindex != modelindex) {
		return false;
	}
	if (cent->current.angles[0] || cent->current.angles[1] || cent->current.angles[2]) {
		return false;
	}

	if (((cl_nolerp.value || cl_nolerp_on_entity_flag) && !cls.mvdplayback) || cent->deltalerp <= 0) {
		VectorCopy(cent->current.origin, origin);
	}
	else {
		time = cls.mvdplayback ? cls.demotime : cl.time;
		lerp = min((time - cent->startlerp) / cent->deltalerp, 1);
		// Still turning from its old angles.
		if (lerp < 1 && (cent->old_angles[0] || cent->old_angles[1] || cent->old_angles[2])) {
			return false;
		}
		PS_VectorInterpolate(cent->old_origin, lerp, cent->current.origin, origin);
	}
	return isfinite(origin[0]) && isfinite(origin[1]) && isfinite(origin[2]);
}

// Decals on a brush entity are drawn at its current origin, and not at all
// while it is gone, has another model or is rotated, which they do not follow.
static void PScript_BuildClippedDecals(const pscript_type_t *type)
{
	const pscript_clippeddecal_t *d;
	scenetris_vert_t *v;
	float colour[4], vcolour[4];
	vec3_t origin, position;
	int i, entity = 0, modelindex = 0;
	qbool visible = true;

	for (d = type->clippeddecals; d; d = d->next) {
		if (d->entity) {
			// The triangles of one decal are consecutive in the list.
			if (d->entity != entity || d->modelindex != modelindex) {
				entity = d->entity;
				modelindex = d->modelindex;
				visible = PScript_DecalEntityOrigin(entity, modelindex, origin);
			}
			if (!visible) {
				continue;
			}
		}

		v = R_SceneTrisAlloc(type->looks.texture, (scenetris_blend_t)type->looks.blendmode, SCENETRIS_DECAL, 3);
		if (!v) {
			return;
		}

		PScript_VertexColour(&type->looks, d->rgba, colour);
		for (i = 0; i < 3; i++) {
			// valpha fades the decal away from its centre plane; with
			// premultiplied alpha that fades the colour too.
			vcolour[0] = colour[0];
			vcolour[1] = colour[1];
			vcolour[2] = colour[2];
			if (type->looks.premul) {
				PS_VectorScale(vcolour, d->valpha[i], vcolour);
			}
			vcolour[3] = colour[3] * d->valpha[i];
			if (d->entity) {
				VectorAdd(d->vertex[i], origin, position);
				PScript_SetVertex(&v[i], position, d->texcoords[i][0], d->texcoords[i][1], vcolour);
			}
			else {
				PScript_SetVertex(&v[i], d->vertex[i], d->texcoords[i][0], d->texcoords[i][1], vcolour);
			}
		}
	}
}

static void PScript_BuildParticles(const pscript_type_t *type)
{
	const pscript_particle_t *p;
	float scale;

	switch (type->looks.type) {
		case PT_NORMAL:
			for (p = type->particles; p; p = p->next) {
				if (type->looks.scalefactor == 1) {
					scale = p->scale * 0.25f;
				}
				else {
					scale = PScript_DistanceScale(&type->looks, p, 0.25f, 0.001f);
				}
				if (!PScript_BuildSprite(type, p, scale, pright, pup, 0)) {
					return;
				}
			}
			break;
		case PT_UDECAL:
			for (p = type->particles; p; p = p->next) {
				if (!PScript_BuildSprite(type, p, p->scale, udecal_right, udecal_up, SCENETRIS_DECAL)) {
					return;
				}
			}
			break;
		case PT_TEXTUREDSPARK:
			for (p = type->particles; p; p = p->next) {
				if (!PScript_BuildTexturedSpark(type, p)) {
					return;
				}
			}
			break;
		case PT_SPARKFAN:
			for (p = type->particles; p; p = p->next) {
				if (!PScript_BuildFanSpark(type, p)) {
					return;
				}
			}
			break;
		case PT_SPARK:
			for (p = type->particles; p; p = p->next) {
				if (!PScript_BuildLineSpark(type, p)) {
					return;
				}
			}
			break;
		case PT_BEAM:
			PScript_BuildBeams(type);
			break;
		default:
			break;
	}
}

// Submits the current view's triangles and lines for every type in the run
// list. Once per view; changes no state, so every view of a host frame gets
// the same scene. When the scene is full, a type stops at the first instance
// that does not fit, but later types still try: theirs may need fewer.
void PScript_BuildGeometry(void)
{
	const pscript_type_t *type;

	if (!pool_particles) {
		return;
	}

	PS_VectorScale(vup, 1.5f, pup);
	PS_VectorScale(vright, 1.5f, pright);

	for (type = pscript_runlist; type; type = type->nexttorun) {
		PScript_BuildClippedDecals(type);
		PScript_BuildParticles(type);
	}
}
