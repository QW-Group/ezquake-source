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

// Scripted particle trails between two points, and the trail states that
// carry a trail's progress and beam from one call to the next.

#include "quakedef.h"
#include "r_part_script_local.h"

// Handed out round robin; a new owner takes the oldest slot.
static pscript_trailstate_t *trailstates;
static int ts_cycle;

// Gives up a state and its last beam segment, zeroing all but the generation
// so keys that named the state no longer match.
static void PScript_CleanTrailstate(pscript_trailstate_t *ts)
{
	unsigned int generation = ts->generation;

	if (ts->lastbeam) {
		ts->lastbeam->flags &= ~BS_LASTSEG;
		ts->lastbeam->flags |= BS_NODRAW;
	}
	memset(ts, 0, sizeof(*ts));
	ts->generation = generation;
}

// Returns the state key names, or NULL when the key is 0, malformed, or its
// state was given to another owner since.
static pscript_trailstate_t *PScript_TrailstateForKey(trailkey_t key)
{
	unsigned int index;

	if (!key || !trailstates) {
		return NULL;
	}
	index = (key & PSCRIPT_TRAILKEY_INDEXMASK) - 1;
	if (index >= PSCRIPT_MAX_TRAILSTATES || trailstates[index].key != key) {
		return NULL;
	}
	return &trailstates[index];
}

// Zeroes *tk and frees its state and its assoc chain's. Safe with any key.
void PScript_DelinkTrailstate(trailkey_t *tk)
{
	pscript_trailstate_t *ts;
	trailkey_t key;
	int steps;

	if (!tk) {
		return;
	}
	key = *tk;
	*tk = 0;

	// Each state's key is checked before it is followed, and every state
	// is cleaned once, so the walk ends even if the chain loops.
	for (steps = 0; steps < PSCRIPT_MAX_TRAILSTATES; steps++) {
		ts = PScript_TrailstateForKey(key);
		if (!ts) {
			break;
		}
		key = ts->assoc;
		PScript_CleanTrailstate(ts);
	}
}

// Delinks *tk when typenum is not *tktype, the type it was last used for, so
// a last beam segment is never joined to another type's particles. A zeroed
// key and type are a valid start.
void PScript_MatchTrailstate(trailkey_t *tk, int *tktype, int typenum)
{
	if (*tktype != typenum) {
		PScript_DelinkTrailstate(tk);
		*tktype = typenum;
	}
}

// Returns the state *tk names, or takes the oldest slot for a new one and
// stores its key in *tk. NULL when tk is NULL or the states are unallocated.
pscript_trailstate_t *PScript_FetchTrailstate(trailkey_t *tk)
{
	pscript_trailstate_t *ts;

	if (!tk || !trailstates) {
		return NULL;
	}

	ts = PScript_TrailstateForKey(*tk);
	if (!ts) {
		if (ts_cycle < 0 || ts_cycle >= PSCRIPT_MAX_TRAILSTATES) {
			ts_cycle = 0;
		}
		ts = &trailstates[ts_cycle];
		PScript_CleanTrailstate(ts);
		ts->generation = (ts->generation + 1) & (~0u >> PSCRIPT_TRAILKEY_INDEXBITS);
		ts->key = (ts->generation << PSCRIPT_TRAILKEY_INDEXBITS) | (unsigned int)(ts_cycle + 1);
		ts_cycle++;
		*tk = ts->key;
	}

	return ts;
}

// PScript_FetchTrailstate for typenum. An owner keeps one key whether a type
// or its inwater replacement runs, so a state last used for the other is
// delinked first, with the rest of its chain.
pscript_trailstate_t *PScript_FetchHeadTrailstate(trailkey_t *tk, int typenum)
{
	pscript_trailstate_t *ts;

	if (!tk) {
		return NULL;
	}
	ts = PScript_TrailstateForKey(*tk);
	if (ts && ts->headtype != typenum + 1) {
		PScript_DelinkTrailstate(tk);
	}
	ts = PScript_FetchTrailstate(tk);
	if (ts) {
		ts->headtype = typenum + 1;
	}
	return ts;
}

void PScript_AllocTrailstates(void)
{
	if (!trailstates) {
		trailstates = Q_calloc(PSCRIPT_MAX_TRAILSTATES, sizeof(*trailstates));
	}
}

// Cleans every trail state, keeping generations so old keys never match
// again. Must run while the beam pool exists.
void PScript_ResetTrailstates(void)
{
	int i;

	if (trailstates) {
		for (i = 0; i < PSCRIPT_MAX_TRAILSTATES; i++) {
			PScript_CleanTrailstate(&trailstates[i]);
		}
	}
	ts_cycle = 0;
}

// Frees the trail states without touching beam segments, which may be gone.
void PScript_FreeTrailstates(void)
{
	Q_free(trailstates);
	ts_cycle = 0;
}

// Sets right and up perpendicular to forward, with right horizontal.
// forward need not be normalised; right always is.
void PScript_VectorVectors(const vec3_t forward, vec3_t right, vec3_t up)
{
	if (!forward[0] && !forward[1]) {
		right[0] = 0;
		right[1] = forward[2] ? -1 : 0;
		right[2] = 0;
	}
	else {
		right[0] = forward[1];
		right[1] = -forward[0];
		right[2] = 0;
		VectorNormalize(right);
	}
	CrossProduct(right, forward, up);
}

// Keeps spiral and circle angles within a turn, where a float still tells
// them apart, whatever the trail length and the config's turn rate.
static float PScript_TrailAngle(float angle)
{
	return isfinite(angle) ? fmod(angle, 2 * M_PI) : 0;
}

// 0 when out of range or not a number.
static int PScript_TruncInt(float value)
{
	return value > -2147483648.0f && value < 2147483648.0f ? (int)value : 0;
}

// Moves an atlas slot past the end of a texture row to the start of the next
// row.
static void PScript_WrapAtlasRow(const pscript_type_t *ptype, pscript_particle_t *p)
{
	float rows;

	if (!(p->s1 >= 1) || !isfinite(p->s1)) {
		return;
	}
	rows = floor(p->s1);
	p->s1 -= rows;
	p->s2 -= rows;
	p->t1 += rows * ptype->texsstride;
	p->t2 += rows * ptype->texsstride;
}

// True when a call from start carries on the beam of span beam type ptype
// that ts ended on: its last point, which never moves, lies no further back
// from start than the distance the state covered past it, give or take a
// unit for rounding.
static qbool PScript_SpanBeamContinues(const pscript_trailstate_t *ts, const pscript_type_t *ptype, const vec3_t start)
{
	const pscript_beamseg_t *b = ts->lastbeam;
	float behind;
	vec3_t back;

	if (!b || !b->p || (b->flags & BS_DEAD) || ts->lastbeamtype != (int)(ptype - pscript_types)) {
		return false;
	}
	behind = ts->u.trail.laststop - ts->u.trail.lastdist + ptype->countspacing;
	VectorSubtract(start, b->p->org, back);
	return VectorLength(back) <= behind + 1;
}

// Spawns the particles of one type of a trail from startpos to end, over
// timeinterval seconds. ts carries the distance covered and the last beam
// segment from the previous call, or is NULL.
static void PScript_TrailSpawnType(vec3_t startpos, vec3_t end, pscript_type_t *ptype, float timeinterval, pscript_trailstate_t *ts)
{
	vec3_t vec, vstep, right, up, start;
	float len, step, stop, fcount, density, extra, veladd, tdegree, sdegree, nrfirst, nrlast, turn, spanstart = 0;
	int count, tcount;
	qbool beam = ptype->looks.type == PT_BEAM, span = false;
	pscript_beamseg_t *b = NULL, *bfirst = NULL;
	pscript_particle_t *p;

	if (!PScript_ContentsAllow(ptype, startpos)) {
		return;
	}

	if (ptype->spawntime && ts) {
		if (ts->u.effect.statetime > pscript_particletime) {
			return;
		}
		ts->u.effect.statetime = pscript_particletime + ptype->spawntime;
		// statetime shares storage with the trail length, which must not
		// overwrite it.
		ts = NULL;
	}

	if (ptype->spawnchance < frandom()) {
		return;
	}

	// Expiry frees every beam segment of a type whose particles live one
	// frame, so no state may hold one as its last.
	if (!(ptype->die > 0)) {
		ts = NULL;
	}

	VectorCopy(startpos, start);
	VectorSubtract(end, start, vec);
	len = VectorNormalize(vec);
	if (!isfinite(len)) {
		return;
	}

	// No countextra for particles that outlive the frame while time stands
	// still, where they would pile up; cl.paused would miss a demo frozen by
	// cl_demospeed 0.
	density = r_part_density.value;
	extra = !(pscript_frametime > 0) && (ptype->die || ptype->randdie) ? 0 : ptype->countextra;
	if (ptype->flags & PT_SPANBEAM) {
		// A call that carries on the state's beam places a point every
		// countspacing units travelled, so the beam's cost follows the
		// distance and not the frame rate. Any other call spans start to end
		// with one segment, a point at each, and starts the state's beam
		// over. Neither depends on the density.
		if (ts && !PScript_SpanBeamContinues(ts, ptype, start)) {
			if (ts->lastbeam) {
				ts->lastbeam->flags &= ~BS_LASTSEG;
				ts->lastbeam->flags |= BS_NODRAW;
				ts->lastbeam = NULL;
			}
			ts->u.trail.laststop = 0;
			ts->u.trail.lastdist = 0;
		}
		span = !ts || !ts->lastbeam;
		if (span) {
			// The texture starts at the span's first point, so it stays put
			// there when the span's length changes.
			spanstart = len;
			step = len;
			len += step;
		}
		else {
			// The next point lies lastdist - laststop past start.
			step = ptype->countspacing;
			PS_VectorMA(start, ts->u.trail.lastdist - ts->u.trail.laststop, vec, start);
		}
	}
	else if (ptype->countspacing) {
		// A particle every countspacing units, or countextra more spread
		// evenly over the segment; the per-second count is unused.
		step = ptype->countspacing / density;
		if (extra) {
			fcount = extra;
			if (step > 0) {
				fcount += len / step;
			}
			step = len / fcount;
		}
	}
	else {
		// The per-second count and countextra, spread evenly over the
		// segment. A call that spawns none leaves the trail state alone.
		fcount = PScript_BoundCount(ptype->count * density * timeinterval + extra + ptype->countoverflow);
		count = (int)fcount;
		ptype->countoverflow = fcount - count;
		if (count <= 0) {
			return;
		}
		step = len / count;
	}

	if (ptype->flags & PT_AVERAGETRAIL) {
		// Shrink the step so that the last point lands on end.
		float tavg = len / step;

		if (tavg > 0 && isfinite(tavg)) {
			tavg /= ceil(tavg);
			step *= tavg;
			len += step;
		}
	}

	PS_VectorScale(vec, step, vstep);

	if (ts) {
		stop = ts->u.trail.laststop + len;
		ts->u.trail.laststop = stop;
		len = ts->u.trail.lastdist;
	}
	else {
		stop = len;
		len = 0;
	}

	// One particle per step up to and past stop, so a fraction of a step
	// still spawns one. A step that is not positive spawns none, rather than
	// all of them at one point.
	fcount = len < stop && step > 0 ? PScript_BoundCount((stop - len) / step) : 0;
	count = fcount > 0 ? (int)ceil(fcount) : 0;
	if (!count) {
		step = 0;
		VectorClear(vstep);
	}

	tdegree = 2 * M_PI / 256;
	sdegree = 0;
	VectorClear(right);
	VectorClear(up);
	switch (ptype->spawnmode) {
		case SM_SPIRAL:
			PScript_VectorVectors(vec, right, up);
			if (ptype->spawnparam1) {
				tdegree = 2 * M_PI / ptype->spawnparam1; // spawnparam1 is the length of a turn
			}
			sdegree = ptype->spawnparam2 * (M_PI / 180);
			break;
		case SM_CIRCLE:
			PScript_VectorVectors(vec, right, up);
			break;
		default:
			break;
	}

	nrfirst = ptype->flags & PT_NOSPREADFIRST ? len + step * 1.5f : len;
	nrlast = ptype->flags & PT_NOSPREADLAST ? stop : stop + step;
	veladd = -ptype->veladd;

	while (count-- > 0) {
		len += step;

		if (!pscript_free_particles || (beam && !pscript_free_beams)) {
			len = stop;
			break;
		}

		tcount = PScript_TruncInt(ptype->spawnparam1 ? len * ptype->count / ptype->spawnparam1 : len * ptype->count);
		// Quake's tracers alternate between two palette rows.
		p = PScript_SpawnParticle(ptype, ptype->flags & PT_CITRACER ? (tcount & 4) << 1 : 0);
		if (beam) {
			b = PScript_AppendBeamSeg(&bfirst, b, p, len - spanstart, vec);
		}
		if (ptype->randsmax > 1) {
			PScript_WrapAtlasRow(ptype, p);
		}

		VectorClear(p->vel);
		if (len < nrfirst || len >= nrlast) {
			PS_VectorScale(vec, veladd, p->vel);
			VectorCopy(start, p->org);
		}
		else {
			switch (ptype->spawnmode) {
				case SM_TRACER:
					if (tcount & 1) {
						p->vel[0] = vec[1] * ptype->spawnvel;
						p->vel[1] = -vec[0] * ptype->spawnvel;
						p->org[0] = vec[1] * ptype->areaspread;
						p->org[1] = -vec[0] * ptype->areaspread;
					}
					else {
						p->vel[0] = -vec[1] * ptype->spawnvel;
						p->vel[1] = vec[0] * ptype->spawnvel;
						p->org[0] = -vec[1] * ptype->areaspread;
						p->org[1] = vec[0] * ptype->areaspread;
					}
					p->vel[0] += vec[0] * veladd;
					p->vel[1] += vec[1] * veladd;
					p->vel[2] = vec[2] * veladd;
					p->org[0] += start[0];
					p->org[1] += start[1];
					p->org[2] = start[2];
					break;
				case SM_SPIRAL:
				{
					float tsin, tcos;

					turn = PScript_TrailAngle(len * tdegree + sdegree);
					tcos = PCOS(turn);
					tsin = PSIN(turn);
					p->org[0] = start[0] + (right[0] * tcos + up[0] * tsin) * ptype->areaspread;
					p->org[1] = start[1] + (right[1] * tcos + up[1] * tsin) * ptype->areaspread;
					p->org[2] = start[2] + (right[2] * tcos + up[2] * tsin) * ptype->areaspread;
					p->vel[0] = vec[0] * veladd + (right[0] * tcos + up[0] * tsin) * ptype->spawnvel;
					p->vel[1] = vec[1] * veladd + (right[1] * tcos + up[1] * tsin) * ptype->spawnvel;
					p->vel[2] = vec[2] * veladd + (right[2] * tcos + up[2] * tsin) * ptype->spawnvel;
					break;
				}
				case SM_BALL:
					p->org[0] = crandom();
					p->org[1] = crandom();
					p->org[2] = crandom();
					VectorNormalize(p->org);
					PS_VectorScale(p->org, frandom(), p->org);
					p->vel[0] = vec[0] * veladd + p->org[0] * ptype->spawnvel;
					p->vel[1] = vec[1] * veladd + p->org[1] * ptype->spawnvel;
					p->vel[2] = vec[2] * veladd + p->org[2] * ptype->spawnvelvert;
					p->org[0] = p->org[0] * ptype->areaspread + start[0];
					p->org[1] = p->org[1] * ptype->areaspread + start[1];
					p->org[2] = p->org[2] * ptype->areaspreadvert + start[2];
					break;
				case SM_CIRCLE:
				{
					float tsin, tcos, angle;

					turn = len * tdegree;
					angle = PScript_TrailAngle(turn);
					tcos = PCOS(angle);
					tsin = PSIN(angle);
					p->org[0] = start[0] + (right[0] * tcos + up[0] * tsin) * ptype->areaspread + vstep[0] * turn;
					p->org[1] = start[1] + (right[1] * tcos + up[1] * tsin) * ptype->areaspread + vstep[1] * turn;
					p->org[2] = start[2] + (right[2] * tcos + up[2] * tsin) * ptype->areaspread + vstep[2] * turn * 50;
					p->vel[0] = vec[0] * veladd + (right[0] * tcos + up[0] * tsin) * ptype->spawnvel;
					p->vel[1] = vec[1] * veladd + (right[1] * tcos + up[1] * tsin) * ptype->spawnvel;
					p->vel[2] = vec[2] * veladd + (right[2] * tcos + up[2] * tsin) * ptype->spawnvel;
					break;
				}
				case SM_DISTBALL:
				{
					// crandom() * crandom() concentrates them near spawnparam2.
					float rdist = ptype->spawnparam2 - crandom() * (1 - crandom() * ptype->spawnparam1);

					p->org[0] = crandom();
					p->org[1] = crandom();
					p->org[2] = crandom();
					VectorNormalize(p->org);
					PS_VectorScale(p->org, rdist, p->org);
					p->vel[0] = vec[0] * veladd + p->org[0] * ptype->spawnvel;
					p->vel[1] = vec[1] * veladd + p->org[1] * ptype->spawnvel;
					p->vel[2] = vec[2] * veladd + p->org[2] * ptype->spawnvelvert;
					p->org[0] = p->org[0] * ptype->areaspread + start[0];
					p->org[1] = p->org[1] * ptype->areaspread + start[1];
					p->org[2] = p->org[2] * ptype->areaspreadvert + start[2];
					break;
				}
				default:
					p->org[0] = crandom();
					p->org[1] = crandom();
					p->org[2] = crandom();
					p->vel[0] = vec[0] * veladd + p->org[0] * ptype->spawnvel;
					p->vel[1] = vec[1] * veladd + p->org[1] * ptype->spawnvel;
					p->vel[2] = vec[2] * veladd + p->org[2] * ptype->spawnvelvert;
					p->org[0] = p->org[0] * ptype->areaspread + start[0];
					p->org[1] = p->org[1] * ptype->areaspread + start[1];
					p->org[2] = p->org[2] * ptype->areaspreadvert + start[2];
					break;
			}

			if (ptype->orgadd) {
				PS_VectorMA(p->org, ptype->orgadd, vec, p->org);
			}
		}
		if (ptype->flags & PT_WORLDSPACERAND) {
			PScript_WorldSpaceRandom(ptype, p);
		}
		VectorAdd(p->org, ptype->orgbias, p->org);
		VectorCopy(p->org, p->oldorg);

		VectorAdd(start, vstep, start);
		if (ptype->countrand) {
			float rstep = frandom() / ptype->countrand;

			if (isfinite(rstep)) {
				PS_VectorMA(start, rstep, vec, start);
				step += rstep;
			}
		}
	}

	if (ts) {
		ts->u.trail.lastdist = isfinite(len) ? len : stop;
		if (span) {
			ts->u.trail.lastdist -= spanstart;
			// The end point covers the state up to where it lies, and the
			// next point follows countspacing units on, its texture
			// coordinate as far past the end point's.
			ts->u.trail.laststop = ts->u.trail.lastdist - ptype->countspacing;
		}

		if (beam) {
			if (b) {
				if (ts->lastbeam && ts->lastbeamtype == (int)(ptype - pscript_types)) {
					// Continue the chain from the segment the last call
					// ended on, so the beam has no gap.
					b->next = ts->lastbeam->next;
					ts->lastbeam->next = bfirst;
					ts->lastbeam->flags &= ~BS_LASTSEG;
				}
				else {
					// A last segment of another type, which the inwater
					// switch can leave under the key, is on that type's
					// list; release it, or nothing ever frees it.
					if (ts->lastbeam) {
						ts->lastbeam->flags &= ~BS_LASTSEG;
						ts->lastbeam->flags |= BS_NODRAW;
					}
					b->next = ptype->beams;
					ptype->beams = bfirst;
				}
				b->flags |= BS_LASTSEG;
				ts->lastbeam = b;
				ts->lastbeamtype = (int)(ptype - pscript_types);
			}

			// A beam cut short by an empty pool must not be joined to the
			// next call's segments.
			if ((!pscript_free_particles || !pscript_free_beams) && ts->lastbeam) {
				ts->lastbeam->flags &= ~BS_LASTSEG;
				ts->lastbeam->flags |= BS_NODRAW;
				ts->lastbeam = NULL;
			}
		}
	}
	else if (b) {
		b->flags |= BS_NODRAW;
		b->next = ptype->beams;
		ptype->beams = bfirst;
	}

	PScript_LinkRunList(ptype);
}

// Spawns trail typenum and its assoc chain from start to end. timeinterval
// scales per-second counts; dlkey keys the dlight; tk continues the trail of
// the previous call, or is NULL. False when inactive or typenum is not loaded.
// Ungated by rulesets, for trails that carry no game state.
qbool PScript_RunTrail(vec3_t start, vec3_t end, int typenum, float timeinterval, int dlkey, trailkey_t *tk)
{
	pscript_type_t *ptype;
	pscript_trailstate_t *ts;
	int steps;

	if (!PScript_Active() || !PART_VALID(typenum) || !pscript_types[typenum].loaded) {
		return false;
	}
	ptype = &pscript_types[typenum];

	// At most one step per type, because a config can make the assoc chain
	// loop, through inwater switches too.
	for (steps = 0; steps < pscript_numtypes; steps++) {
		// An unloaded type ends the chain; each type switches on its own.
		if (!ptype->loaded) {
			break;
		}
		ptype = PScript_InwaterType(ptype, start);
		ts = ptype->flags & PT_NOSTATE ? NULL : PScript_FetchHeadTrailstate(tk, (int)(ptype - pscript_types));
		PScript_EffectSpawned(ptype, start, dlkey);
		PScript_TrailSpawnType(start, end, ptype, timeinterval, ts);
		if (!PART_VALID(ptype->assoc)) {
			break;
		}
		tk = ts ? &ts->assoc : NULL;
		ptype = &pscript_types[ptype->assoc];
	}

	return true;
}

// For events and moving entities; fails under rulesets that restrict
// particles.
qbool PScript_ParticleTrail(vec3_t start, vec3_t end, int typenum, float timeinterval, int dlkey, trailkey_t *tk)
{
	return PScript_DynamicActive() && PScript_RunTrail(start, end, typenum, timeinterval, dlkey, tk);
}
