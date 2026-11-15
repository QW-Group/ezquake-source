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

// PScript_FetchTrailstate for typenum heading a chain. An owner keeps one key
// whether a type or its inwater replacement runs, so a state last used for
// the other is delinked first.
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

// Spawns the particles of one type of a trail from startpos to end, over
// timeinterval seconds. ts carries the distance covered and the last beam
// segment from the previous call, or is NULL.
static void PScript_TrailSpawnType(vec3_t startpos, vec3_t end, pscript_type_t *ptype, float timeinterval, pscript_trailstate_t *ts)
{
	vec3_t vec, vstep, right, up, start;
	float len, step, stop, fcount, density, veladd, tdegree, sdegree, nrfirst, nrlast, turn;
	int count, tcount;
	qbool beam = ptype->looks.type == PT_BEAM;
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
	fcount = !(pscript_frametime > 0) && (ptype->die || ptype->randdie) ? 0 : ptype->countextra;
	fcount += ptype->count * density * timeinterval;
	fcount = PScript_BoundCount(fcount + ptype->countoverflow);
	count = (int)fcount;
	ptype->countoverflow = fcount - count;
	if (count < 0) {
		count = 0;
	}

	// Plus one particle per countspacing units of the trail.
	step = 0;
	if (ptype->countspacing > 0 && density > 0) {
		step = ptype->countspacing / density;
		if (!isfinite(step)) {
			step = 0;
		}
	}

	if ((ptype->flags & PT_AVERAGETRAIL) && step > 0 && len > 0) {
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

	if (step > 0 && len < stop) {
		fcount = PScript_BoundCount((stop - len) / step);
		if (count && len) {
			// Spread the counted particles over the whole distance too.
			count = (int)(count + fcount);
			step = (stop - len) / count;
		}
		else {
			count = (int)(count + fcount);
		}
	}
	else {
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
			b = PScript_AppendBeamSeg(&bfirst, b, p, len, vec);
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
		if (ptype->countrand > 0) {
			float rstep = frandom() / ptype->countrand;

			PS_VectorMA(start, rstep, vec, start);
			step += rstep;
		}
	}

	if (ts) {
		ts->u.trail.lastdist = isfinite(len) ? len : stop;

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
qbool PScript_ParticleTrail(vec3_t start, vec3_t end, int typenum, float timeinterval, int dlkey, trailkey_t *tk)
{
	pscript_type_t *ptype;
	pscript_trailstate_t *ts;
	int steps;

	if (!PScript_Active() || !PART_VALID(typenum) || !pscript_types[typenum].loaded) {
		return false;
	}
	ptype = PScript_InwaterType(&pscript_types[typenum], start);

	// At most one step per type, because a config can make the assoc chain
	// loop.
	for (steps = 0; steps < pscript_numtypes && steps < PSCRIPT_MAX_ASSOCSTEPS; steps++) {
		if (ptype->flags & PT_NOSTATE) {
			ts = NULL;
		}
		else if (!steps) {
			ts = PScript_FetchHeadTrailstate(tk, (int)(ptype - pscript_types));
		}
		else {
			ts = PScript_FetchTrailstate(tk);
		}
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
