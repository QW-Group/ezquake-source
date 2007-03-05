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

	$Id: pmovetst.c,v 1.7 2007-03-05 00:16:24 disconn3ct Exp $
*/
#include "quakedef.h"

static	hull_t		box_hull;
static	dclipnode_t	box_clipnodes[6];
static	mplane_t	box_planes[6];

extern	vec3_t player_mins;
extern	vec3_t player_maxs;

int PM_HullPointContents (hull_t *hull, int num, vec3_t p) {
	float d;
	dclipnode_t *node;
	mplane_t *plane;

	while (num >= 0) {
		if (num < hull->firstclipnode || num > hull->lastclipnode)
			Sys_Error ("PM_HullPointContents: bad node number");
	
		node = hull->clipnodes + num;
		plane = hull->planes + node->planenum;

		d = PlaneDiff(p, plane);
		num = (d < 0) ? node->children[1] : node->children[0];
	}
	
	return num;
}

int PM_PointContents (vec3_t p) {
	float d;
	dclipnode_t	*node;
	mplane_t *plane;
	hull_t *hull;
	int num;

	hull = &pmove.physents[0].model->hulls[0];

	num = hull->firstclipnode;

	while (num >= 0) {
		if (num < hull->firstclipnode || num > hull->lastclipnode)
			Sys_Error ("PM_HullPointContents: bad node number");

		node = hull->clipnodes + num;
		plane = hull->planes + node->planenum;
		
		d = PlaneDiff(p, plane);
		num = (d < 0) ? node->children[1] : node->children[0];
	}

	return num;
}

/*
===============================================================================
LINE TESTING IN HULLS
===============================================================================
*/

// 1/32 epsilon to keep floating point happy
#define	DIST_EPSILON	(0.03125)

qbool PM_RecursiveHullCheck (hull_t *hull, int num, float p1f, float p2f, vec3_t p1, vec3_t p2, trace_t *trace) {
	dclipnode_t	*node;
	mplane_t *plane;
	float t1, t2, frac, midf;
	int i, side;
	vec3_t mid;

	// check for empty
	if (num < 0) {
		if (num != CONTENTS_SOLID) {
			trace->allsolid = false;
			if (num == CONTENTS_EMPTY)
				trace->inopen = true;
			else
				trace->inwater = true;
		} else {
			trace->startsolid = true;
		}
		return true;		// empty
	}

	if (num < hull->firstclipnode || num > hull->lastclipnode)
		Sys_Error ("PM_RecursiveHullCheck: bad node number");

	// find the point distances
	node = hull->clipnodes + num;
	plane = hull->planes + node->planenum;

	if (plane->type < 3) {
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
	} else {
		t1 = DotProduct (plane->normal, p1) - plane->dist;
		t2 = DotProduct (plane->normal, p2) - plane->dist;
	}

	if (t1 >= 0 && t2 >= 0)
		return PM_RecursiveHullCheck (hull, node->children[0], p1f, p2f, p1, p2, trace);
	if (t1 < 0 && t2 < 0)
		return PM_RecursiveHullCheck (hull, node->children[1], p1f, p2f, p1, p2, trace);

	// put the crosspoint DIST_EPSILON pixels on the near side
	if (t1 < 0)
		frac = (t1 + DIST_EPSILON)/(t1 - t2);
	else
		frac = (t1 - DIST_EPSILON)/(t1 - t2);
	frac = bound(0, frac, 1);
		
	midf = p1f + (p2f - p1f)*frac;
	for (i = 0; i < 3; i++)
		mid[i] = p1[i] + frac * (p2[i] - p1[i]);

	side = (t1 < 0);

	// move up to the node
	if (!PM_RecursiveHullCheck (hull, node->children[side], p1f, midf, p1, mid, trace) )
		return false;

#ifdef PARANOID
	if (PM_HullPointContents (pm_hullmodel, mid, node->children[side]) == CONTENTS_SOLID) {
		Com_Printf ("mid PointInHullSolid\n");
		return false;
	}
#endif

	if (PM_HullPointContents (hull, node->children[side ^ 1], mid) != CONTENTS_SOLID)	// go past the node
		return PM_RecursiveHullCheck (hull, node->children[side ^ 1], midf, p2f, mid, p2, trace);

	if (trace->allsolid)
		return false;		// never got out of the solid area

	// the other side of the node is solid, this is the impact point
	if (!side) {
		VectorCopy (plane->normal, trace->plane.normal);
		trace->plane.dist = plane->dist;
	} else {
		VectorNegate (plane->normal, trace->plane.normal);
		trace->plane.dist = -plane->dist;
	}

	while (PM_HullPointContents (hull, hull->firstclipnode, mid) == CONTENTS_SOLID) { 
		// shouldn't really happen, but does occasionally
		frac -= 0.1;
		if (frac < 0) {
			trace->fraction = midf;
			VectorCopy (mid, trace->endpos);
			Com_DPrintf ("backup past 0\n");
			return false;
		}
		midf = p1f + (p2f - p1f) * frac;
		for (i = 0; i < 3; i++)
			mid[i] = p1[i] + frac * (p2[i] - p1[i]);
	}

	trace->fraction = midf;
	VectorCopy (mid, trace->endpos);

	return false;
}

//Returns false if the given player position is not valid (in solid)
qbool PM_TestPlayerPosition (vec3_t pos) {
	int i;
	physent_t *pe;
	vec3_t mins, maxs, pos_l, offset;
	hull_t *hull;

	for (i = 0; i < pmove.numphysent; i++) {
		pe = &pmove.physents[i];
		// get the clipping hull
		if (pe->model) {
			hull = &pmove.physents[i].model->hulls[1];
			VectorSubtract(hull->clip_mins, player_mins, offset);
			VectorAdd(offset, pe->origin, offset);
			VectorSubtract(pos, offset, pos_l);
		} else{
			VectorSubtract (pe->mins, player_maxs, mins);
			VectorSubtract (pe->maxs, player_mins, maxs);
			hull = CM_HullForBox (mins, maxs);
			VectorSubtract(pos, pe->origin, pos_l);
		}

		if (PM_HullPointContents (hull, hull->firstclipnode, pos_l) == CONTENTS_SOLID)
			return false;
	}

	return true;
}

trace_t PM_PlayerTrace (vec3_t start, vec3_t end) {
	trace_t trace, total;
	vec3_t offset, start_l, end_l, mins, maxs;
	hull_t *hull;
	int i;
	physent_t *pe;

	// fill in a default trace
	memset (&total, 0, sizeof(trace_t));
	total.fraction = 1;
	total.e.entnum = -1;
	VectorCopy (end, total.endpos);

	for (i = 0; i < pmove.numphysent; i++) {
		pe = &pmove.physents[i];
		// get the clipping hull
		if (pe->model) {
			hull = &pmove.physents[i].model->hulls[1];
			VectorSubtract(hull->clip_mins, player_mins, offset);
			VectorAdd(offset, pe->origin, offset);
		} else {
			VectorSubtract (pe->mins, player_maxs, mins);
			VectorSubtract (pe->maxs, player_mins, maxs);
			hull = CM_HullForBox (mins, maxs);
			VectorCopy(pe->origin, offset);
		}

		VectorSubtract (start, offset, start_l);
		VectorSubtract (end, offset, end_l);

		// fill in a default trace
		memset (&trace, 0, sizeof(trace_t));
		trace.fraction = 1;
		trace.allsolid = true;
		//trace.startsolid = true;
		VectorCopy (end, trace.endpos);

		// trace a line through the appropriate clipping hull
		PM_RecursiveHullCheck (hull, hull->firstclipnode, 0, 1, start_l, end_l, &trace);

		if (trace.allsolid)
			trace.startsolid = true;
		if (trace.startsolid)
			trace.fraction = 0;

		// did we clip the move?
		if (trace.fraction < total.fraction) {
			// fix trace up by the offset
			VectorAdd (trace.endpos, offset, trace.endpos);
			total = trace;
			total.e.entnum = i;
		}

	}

	return total;
}

//FIXME: merge with PM_PlayerTrace (PM_Move?)
trace_t PM_TraceLine (vec3_t start, vec3_t end) {
	trace_t trace, total;
	vec3_t offset, start_l, end_l;
	hull_t *hull;
	int i;
	physent_t *pe;

	// fill in a default trace
	memset (&total, 0, sizeof(trace_t));
	total.fraction = 1;
	total.e.entnum = -1;
	VectorCopy (end, total.endpos);

	for (i = 0; i < pmove.numphysent; i++) {
		pe = &pmove.physents[i];
	// get the clipping hull
		if (pe->model)
			hull = &pmove.physents[i].model->hulls[0];
		else
			hull = CM_HullForBox (pe->mins, pe->maxs);

		// PM_HullForEntity (ent, mins, maxs, offset);
		VectorCopy (pe->origin, offset);

		VectorSubtract (start, offset, start_l);
		VectorSubtract (end, offset, end_l);

		// fill in a default trace
		memset (&trace, 0, sizeof(trace_t));
		trace.fraction = 1;
		trace.allsolid = true;
		//trace.startsolid = true;
		VectorCopy (end, trace.endpos);

		// trace a line through the apropriate clipping hull
		PM_RecursiveHullCheck (hull, hull->firstclipnode, 0, 1, start_l, end_l, &trace);

		if (trace.allsolid)
			trace.startsolid = true;
		if (trace.startsolid)
			trace.fraction = 0;

		// did we clip the move?
		if (trace.fraction < total.fraction) {
			// fix trace up by the offset
			VectorAdd (trace.endpos, offset, trace.endpos);
			total = trace;
			total.e.entnum = i;
		}

	}

	return total;
}
