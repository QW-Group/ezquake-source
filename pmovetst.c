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

	
*/

#ifdef SERVERONLY
#include "qwsvdef.h"
#else
#include "quakedef.h"
#include "pmove.h"
#endif

extern	vec3_t player_mins;
extern	vec3_t player_maxs;


static void PM_TraceBounds (vec3_t start, vec3_t end, vec3_t boxmins, vec3_t boxmaxs)
{
	int i;

	for (i = 0; i < 3; i++) {
		if (end[i] > start[i]) {
			boxmins[i] = start[i] - 1;
			boxmaxs[i] = end[i] + 1;
		} else {
			boxmins[i] = end[i] - 1;
			boxmaxs[i] = start[i] + 1;
		}
	}
}

static qbool PM_CullTraceBox(vec3_t mins, vec3_t maxs, vec3_t offset, vec3_t emins, vec3_t emaxs, vec3_t hullmins, vec3_t hullmaxs) {
	return
		(	mins[0] + hullmins[0] > offset[0] + emaxs[0] || maxs[0] + hullmaxs[0] < offset[0] + emins[0] ||
			mins[1] + hullmins[1] > offset[1] + emaxs[1] || maxs[1] + hullmaxs[1] < offset[1] + emins[1] ||
			mins[2] + hullmins[2] > offset[2] + emaxs[2] || maxs[2] + hullmaxs[2] < offset[2] + emins[2]
		);
}

/*
==================
PM_PointContents
==================
*/
int PM_PointContents (vec3_t p)
{
	hull_t *hull = &pmove.physents[0].model->hulls[0];
	return CM_HullPointContents (hull, hull->firstclipnode, p);
}

/*
==================
PM_PointContents_AllBSPs

Checks world and bsp entities, but not bboxes (like traceline with nomonsters set)
For waterjump test
==================
*/
int PM_PointContents_AllBSPs (vec3_t p)
{
	int         i;
	physent_t   *pe;
	hull_t      *hull;
	vec3_t      test;
	int         result, final;

	final = CONTENTS_EMPTY;
	for (i = 0; i < pmove.numphysent; i++)
	{
		pe = &pmove.physents[i];
		if (!pe->model)
			continue;	// ignore non-bsp
		hull = &pmove.physents[i].model->hulls[0];
		VectorSubtract (p, pe->origin, test);
		result = CM_HullPointContents (hull, hull->firstclipnode, test);
		if (result == CONTENTS_SOLID)
			return CONTENTS_SOLID;
		if (final == CONTENTS_EMPTY)
			final = result;
	}

	return final;
}

/*
================
PM_TestPlayerPosition

Returns false if the given player position is not valid (in solid)
================
*/
qbool PM_TestPlayerPosition (vec3_t pos)
{
	int       i;
	physent_t *pe;
	vec3_t    mins, maxs, offset, test;
	hull_t    *hull;

	for (i = 0; i < pmove.numphysent; i++) {
		pe = &pmove.physents[i];
		// get the clipping hull
		if (pe->model) {
			hull = &pmove.physents[i].model->hulls[1];
			VectorSubtract(hull->clip_mins, player_mins, offset);
			VectorAdd(offset, pe->origin, offset);
		}
		else {
			VectorSubtract(pe->mins, player_maxs, mins);
			VectorSubtract(pe->maxs, player_mins, maxs);
			hull = CM_HullForBox(mins, maxs);
			VectorCopy(pe->origin, offset);
		}

		VectorSubtract(pos, offset, test);

		if (CM_HullPointContents(hull, hull->firstclipnode, test) == CONTENTS_SOLID) {
			return false;
		}
	}

	return true;
}

/*
================
PM_PlayerTrace
================
*/
trace_t PM_PlayerTrace (vec3_t start, vec3_t end)
{
	trace_t   trace, total;
	vec3_t    offset;
	vec3_t    start_l, end_l;
	hull_t    *hull;
	int       i;
	physent_t *pe;
	vec3_t    mins, maxs, tracemins, tracemaxs;

	// fill in a default trace
	memset (&total, 0, sizeof(trace_t));
	total.fraction = 1;
	total.e.entnum = -1;
	VectorCopy (end, total.endpos);

	PM_TraceBounds(start, end, tracemins, tracemaxs);

	for (i = 0; i < pmove.numphysent; i++) {
		pe = &pmove.physents[i];

		// get the clipping hull
		if (pe->model) {
			hull = &pmove.physents[i].model->hulls[1];

			if (i > 0 && PM_CullTraceBox(tracemins, tracemaxs, pe->origin, pe->model->mins, pe->model->maxs, hull->clip_mins, hull->clip_maxs)) {
				continue;
			}

			VectorSubtract(hull->clip_mins, player_mins, offset);
			VectorAdd(offset, pe->origin, offset);
		}
		else {
			VectorSubtract(pe->mins, player_maxs, mins);
			VectorSubtract(pe->maxs, player_mins, maxs);

			if (PM_CullTraceBox(tracemins, tracemaxs, pe->origin, mins, maxs, vec3_origin, vec3_origin)) {
				continue;
			}

			hull = CM_HullForBox(mins, maxs);
			VectorCopy(pe->origin, offset);
		}

		VectorSubtract(start, offset, start_l);
		VectorSubtract(end, offset, end_l);

		// trace a line through the appropriate clipping hull
		trace = CM_HullTrace(hull, start_l, end_l);

		// fix trace up by the offset
		VectorAdd(trace.endpos, offset, trace.endpos);

		if (trace.allsolid) {
			trace.startsolid = true;
		}
		if (trace.startsolid) {
			trace.fraction = 0;
		}

		// did we clip the move?
		if (trace.fraction < total.fraction) {
			total = trace;
			total.e.entnum = i;
		}
	}

	return total;
}

/*
================
PM_TraceLine
================
*/
trace_t PM_TraceLine (vec3_t start, vec3_t end)
{
	int i;
	hull_t *hull;
	physent_t *pe;
	vec3_t offset, start_l, end_l;
	trace_t trace, total;

	// fill in a default trace
	memset (&total, 0, sizeof(trace_t));
	total.fraction = 1;
	total.e.entnum = -1;
	VectorCopy (end, total.endpos);

	for (i = 0; i < pmove.numphysent; i++) {
		pe = &pmove.physents[i];
		// get the clipping hull
		hull = (pe->model) ? (&pmove.physents[i].model->hulls[0]) : (CM_HullForBox(pe->mins, pe->maxs));

		// PM_HullForEntity (ent, mins, maxs, offset);
		VectorCopy(pe->origin, offset);

		VectorSubtract(start, offset, start_l);
		VectorSubtract(end, offset, end_l);

		// trace a line through the appropriate clipping hull
		trace = CM_HullTrace(hull, start_l, end_l);

		// fix trace up by the offset
		VectorAdd(trace.endpos, offset, trace.endpos);

		if (trace.allsolid) {
			trace.startsolid = true;
		}
		if (trace.startsolid) {
			trace.fraction = 0;
		}

		// did we clip the move?
		if (trace.fraction < total.fraction) {
			total = trace;
			total.e.entnum = i;
		}
	}

	return total;
}
