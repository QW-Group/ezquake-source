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
// world.c -- world query functions

#ifndef CLIENTONLY
#include "qwsvdef.h"

/*

entities never clip against themselves, or their owner

line of sight checks trace->crosscontent, but bullets don't

*/


typedef struct
{
	vec3_t		boxmins, boxmaxs;// enclose the test object along entire move
	float		*mins, *maxs;	// size of the moving object
	vec3_t		mins2, maxs2;	// size when clipping against mosnters
	float		*start, *end;
	trace_t		trace;
	int			type;
	edict_t		*passedict;
} moveclip_t;


/*
================
SV_HullForEntity

Returns a hull that can be used for testing or clipping an object of mins/maxs
size.
Offset is filled in to contain the adjustment that must be added to the
testing object's origin to get a point to use with the returned hull.
================
*/
hull_t *SV_HullForEntity (edict_t *ent, vec3_t mins, vec3_t maxs, vec3_t offset)
{
	vec3_t size, hullmins, hullmaxs;
	cmodel_t *model;
	hull_t *hull;


	// decide which clipping hull to use, based on the size
	if (ent->v->solid == SOLID_BSP)
	{	// explicit hulls in the BSP model
		if (ent->v->movetype != MOVETYPE_PUSH)
			SV_Error ("SOLID_BSP without MOVETYPE_PUSH");

		if ((unsigned)ent->v->modelindex >= MAX_MODELS)
			SV_Error ("SV_HullForEntity: ent.modelindex >= MAX_MODELS");

		model = sv.models[(int)ent->v->modelindex];
		if (!model)
			SV_Error ("SOLID_BSP with a non-bsp model");

		VectorSubtract (maxs, mins, size);
		if (size[0] < 3)
			hull = &model->hulls[0];
		else if (size[0] <= 32)
			hull = &model->hulls[1];
		else
			hull = &model->hulls[2];

		// calculate an offset value to center the origin
		VectorSubtract (hull->clip_mins, mins, offset);
		VectorAdd (offset, ent->v->origin, offset);
	}
	else
	{	// create a temp hull from bounding box sizes

		VectorSubtract (ent->v->mins, maxs, hullmins);
		VectorSubtract (ent->v->maxs, mins, hullmaxs);
		hull = CM_HullForBox (hullmins, hullmaxs);
		
		VectorCopy (ent->v->origin, offset);
	}


	return hull;
}

/*
===============================================================================

ENTITY AREA CHECKING

===============================================================================
*/

// ClearLink is used for new headnodes
void ClearLink (link_t *l)
{
	l->prev = l->next = l;
}

void RemoveLink (link_t *l)
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

void InsertLinkBefore (link_t *l, link_t *before)
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}
void InsertLinkAfter (link_t *l, link_t *after)
{
	l->next = after->next;
	l->prev = after;
	l->prev->next = l;
	l->next->prev = l;
}

//============================================================================

// well, here should be all things related to world but atm it antilag only
typedef struct world_s
{
// { sv_antilag related
	float lagentsfrac;
	laggedentinfo_t *lagents;
	unsigned int maxlagents;
	client_t *owner;
	qbool ray_narrow;
	float ray_narrow_pad;
	float move_radius;
// }
} world_t;

static world_t w;

areanode_t sv_areanodes[AREA_NODES];
int sv_numareanodes;

/*
===============
SV_CreateAreaNode
===============
*/
areanode_t *SV_CreateAreaNode (int depth, vec3_t mins, vec3_t maxs)
{
	areanode_t	*anode;
	vec3_t		size;
	vec3_t		mins1, maxs1, mins2, maxs2;

	anode = &sv_areanodes[sv_numareanodes];
	sv_numareanodes++;

	ClearLink (&anode->trigger_edicts);
	ClearLink (&anode->solid_edicts);

	if (depth == AREA_DEPTH)
	{
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}

	VectorSubtract (maxs, mins, size);
	if (size[0] > size[1])
		anode->axis = 0;
	else
		anode->axis = 1;

	anode->dist = 0.5 * (maxs[anode->axis] + mins[anode->axis]);
	VectorCopy (mins, mins1);
	VectorCopy (mins, mins2);
	VectorCopy (maxs, maxs1);
	VectorCopy (maxs, maxs2);

	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;

	anode->children[0] = SV_CreateAreaNode (depth+1, mins2, maxs2);
	anode->children[1] = SV_CreateAreaNode (depth+1, mins1, maxs1);

	return anode;
}

/*
===============
SV_ClearWorld
===============
*/
void SV_ClearWorld (void)
{
	memset (sv_areanodes, 0, sizeof(sv_areanodes));
	sv_numareanodes = 0;
	SV_CreateAreaNode (0, sv.worldmodel->mins, sv.worldmodel->maxs);
}


/*
===============
SV_UnlinkEdict
===============
*/
void SV_UnlinkEdict (edict_t *ent)
{
	if (!ent->e.area.prev)
		return;		// not linked in anywhere
	RemoveLink (&ent->e.area);
	ent->e.area.prev = ent->e.area.next = NULL;
}

/*
====================
SV_AreaEdicts
====================
*/
int SV_AreaEdicts (vec3_t mins, vec3_t maxs, edict_t **edicts, int max_edicts, int area)
{
	link_t		*l, *start;
	edict_t		*touch;
	int			stackdepth = 0, count = 0;
	areanode_t	*localstack[AREA_NODES], *node = sv_areanodes;

// touch linked edicts
	while (1)
	{
		if (area == AREA_SOLID)
			start = &node->solid_edicts;
		else
			start = &node->trigger_edicts;

		for (l = start->next ; l != start ; l = l->next)
		{
			touch = EDICT_FROM_AREA(l);
			if (touch->v->solid == SOLID_NOT)
				continue;

			if (mins[0] > touch->v->absmax[0]
						 || mins[1] > touch->v->absmax[1]
						 || mins[2] > touch->v->absmax[2]
						 || maxs[0] < touch->v->absmin[0]
						 || maxs[1] < touch->v->absmin[1]
						 || maxs[2] < touch->v->absmin[2])
				continue;

			if (count == max_edicts)
				return count;
			edicts[count++] = touch;
		}

		if (node->axis == -1)
			goto checkstack;		// terminal node

		// recurse down both sides
		if (maxs[node->axis] > node->dist)
		{
			if (mins[node->axis] < node->dist)
			{
				localstack[stackdepth++] = node->children[0];
				node = node->children[1];
				continue;
			}
			node = node->children[0];
			continue;
		}
		if (mins[node->axis] < node->dist)
		{
			node = node->children[1];
			continue;
		}

checkstack:
		if (!stackdepth)
		return count;
		node = localstack[--stackdepth];
	}

	return count;
}

/*
====================
SV_TouchLinks
====================
*/
static void SV_TouchLinks ( edict_t *ent, areanode_t *node )
{
	int			i, numtouch;
	edict_t		*touchlist[MAX_EDICTS], *touch;
	int			old_self, old_other;

	numtouch = SV_AreaEdicts(ent->v->absmin, ent->v->absmax, touchlist, sv.max_edicts, AREA_TRIGGERS);

// touch linked edicts
	for (i = 0; i < numtouch; i++)
	{
		touch = touchlist[i];
		if (touch == ent)
			continue;
		if (!touch->v->touch || touch->v->solid != SOLID_TRIGGER)
			continue;

		old_self = pr_global_struct->self;
		old_other = pr_global_struct->other;

		pr_global_struct->self = EDICT_TO_PROG(touch);
		pr_global_struct->other = EDICT_TO_PROG(ent);
		pr_global_struct->time = sv.time;
		PR_EdictTouch (touch->v->touch);

		pr_global_struct->self = old_self;
		pr_global_struct->other = old_other;
	}
}

/*
====================
SV_LinkToLeafs
====================
*/
void SV_LinkToLeafs (edict_t *ent)
{
	int	i, leafnums[MAX_ENT_LEAFS];

	ent->e.num_leafs = CM_FindTouchedLeafs (ent->v->absmin, ent->v->absmax, leafnums,
					      MAX_ENT_LEAFS, 0, NULL);
	for (i = 0; i < ent->e.num_leafs; i++) {
		// ent->e.leafnums are real leafnum minus one (for pvs checks)
		ent->e.leafnums[i] = leafnums[i] - 1;
	}
}


/*
===============
SV_LinkEdict

===============
*/
void SV_LinkEdict (edict_t *ent, qbool touch_triggers)
{
	areanode_t	*node;
	
	if (ent->e.area.prev)
		SV_UnlinkEdict (ent);	// unlink from old position

	if (ent == sv.edicts)
		return;		// don't add the world

	if (ent->e.free)
		return;

// set the abs box
	VectorAdd (ent->v->origin, ent->v->mins, ent->v->absmin);
	VectorAdd (ent->v->origin, ent->v->maxs, ent->v->absmax);

	//
// to make items easier to pick up and allow them to be grabbed off
// of shelves, the abs sizes are expanded
	//
	if ((int)ent->v->flags & FL_ITEM)
	{
		ent->v->absmin[0] -= 15;
		ent->v->absmin[1] -= 15;
		ent->v->absmax[0] += 15;
		ent->v->absmax[1] += 15;
	}
	else
	{	// because movement is clipped an epsilon away from an actual edge,
		// we must fully check even when bounding boxes don't quite touch
		ent->v->absmin[0] -= 1;
		ent->v->absmin[1] -= 1;
		ent->v->absmin[2] -= 1;
		ent->v->absmax[0] += 1;
		ent->v->absmax[1] += 1;
		ent->v->absmax[2] += 1;
	}

// link to PVS leafs
	if (ent->v->modelindex)
		SV_LinkToLeafs (ent);
	else
		ent->e.num_leafs = 0;

	if (ent->v->solid == SOLID_NOT)
		return;

// find the first node that the ent's box crosses
	node = sv_areanodes;
	while (1)
	{
		if (node->axis == -1)
			break;
		if (ent->v->absmin[node->axis] > node->dist)
			node = node->children[0];
		else if (ent->v->absmax[node->axis] < node->dist)
			node = node->children[1];
		else
			break; // crosses the node
	}
	
// link it in	

	if (ent->v->solid == SOLID_TRIGGER)
		InsertLinkBefore (&ent->e.area, &node->trigger_edicts);
	else
		InsertLinkBefore (&ent->e.area, &node->solid_edicts);
	
// if touch_triggers, touch all entities at this node and decend for more
	if (touch_triggers)
		SV_TouchLinks ( ent, sv_areanodes );
}



/*
===============================================================================
 
POINT TESTING IN HULLS

===============================================================================
*/

/*
==================
SV_PointContents
 
==================
*/
int SV_PointContents (vec3_t p)
{
	return CM_HullPointContents (&sv.worldmodel->hulls[0], sv.worldmodel->hulls[0].firstclipnode, p);
}

//===========================================================================

/*
============
SV_TestEntityPosition

A small wrapper around SV_BoxInSolidEntity that never clips against the
supplied entity.
============
*/
edict_t	*SV_TestEntityPosition (edict_t *ent)
{
	trace_t	trace;

	if (ent->v->solid == SOLID_TRIGGER || ent->v->solid == SOLID_NOT)
		// only clip against bmodels
		trace = SV_Trace (ent->v->origin, ent->v->mins, ent->v->maxs, ent->v->origin, MOVE_NOMONSTERS, ent);
	else
		trace = SV_Trace(ent->v->origin, ent->v->mins, ent->v->maxs, ent->v->origin, MOVE_NORMAL, ent);
	
	if (trace.startsolid)
		return sv.edicts;
		
	return NULL;
}

/*
==================
SV_ClipMoveToEntity

Handles selection or creation of a clipping hull, and offseting (and
eventually rotation) of the end points
==================
*/
trace_t SV_ClipMoveToEntity (edict_t *ent, vec3_t *eorg, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	trace_t		trace;
	vec3_t		offset;
	vec3_t		start_l, end_l;
	hull_t		*hull;

	// get the clipping hull
	hull = SV_HullForEntity (ent, mins, maxs, offset);

	// { well, its hack for sv_antilag
	if (eorg)
		VectorCopy((*eorg), offset);
	// }

	VectorSubtract (start, offset, start_l);
	VectorSubtract (end, offset, end_l);

	// trace a line through the apropriate clipping hull
	trace = CM_HullTrace (hull, start_l, end_l);

	// fix trace up by the offset
	VectorAdd (trace.endpos, offset, trace.endpos);

	// did we clip the move?
	if (trace.fraction < 1 || trace.startsolid)
		trace.e.ent = ent;

	return trace;
}

//===========================================================================

/*
====================
SV_ClipToLinks

Mins and maxs enclose the entire area swept by the move
====================
*/
void SV_ClipToLinks ( areanode_t *node, moveclip_t *clip )
{
	int			i, numtouch;
	edict_t		*touchlist[MAX_EDICTS], *touch;
	trace_t		trace;

	numtouch = SV_AreaEdicts (clip->boxmins, clip->boxmaxs, touchlist, sv.max_edicts, AREA_SOLID);

	// touch linked edicts
	for (i = 0; i < numtouch; i++)
	{
		// might intersect, so do an exact clip
		if (clip->trace.allsolid)
			return; // return!!!

		touch = touchlist[i];
		if (touch == clip->passedict)
			continue;
		if (touch->v->solid == SOLID_TRIGGER)
			SV_Error ("Trigger in clipping list");

		if ((clip->type & MOVE_NOMONSTERS) && touch->v->solid != SOLID_BSP)
			continue;

		if (clip->passedict && clip->passedict->v->size[0] && !touch->v->size[0])
			continue;	// points never interact

		if (clip->type & MOVE_LAGGED)
		{
			//can't touch lagged ents - we do an explicit test for them later in SV_AntilagClipCheck.
			if (touch->e.entnum - 1 < w.maxlagents)
				if (w.lagents[touch->e.entnum - 1].present)
					continue;
		}

		if (clip->passedict)
		{
			if (PROG_TO_EDICT(touch->v->owner) == clip->passedict)
				continue;	// don't clip against own missiles
			if (PROG_TO_EDICT(clip->passedict->v->owner) == touch)
				continue;	// don't clip against owner
		}

		if ((int)touch->v->flags & FL_MONSTER)
			trace = SV_ClipMoveToEntity (touch, NULL, clip->start, clip->mins2, clip->maxs2, clip->end);
		else
			trace = SV_ClipMoveToEntity (touch, NULL, clip->start, clip->mins, clip->maxs, clip->end);

		// qqshka: I have NO idea why we keep startsolid but let do it.

		// make sure we keep a startsolid from a previous trace
		clip->trace.startsolid |= trace.startsolid;

		if ( trace.allsolid || trace.fraction < clip->trace.fraction )
		{
			// set edict
			trace.e.ent = touch;
			// make sure we keep a startsolid from a previous trace
			trace.startsolid |= clip->trace.startsolid;
			// bit by bit copy trace struct
			clip->trace = trace;
		}
	}
}


/*
==================
SV_MoveBounds
==================
*/
void SV_MoveBounds (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, vec3_t boxmins, vec3_t boxmaxs)
{
#if 0
	// debug to test against everything
	boxmins[0] = boxmins[1] = boxmins[2] = -9999;
	boxmaxs[0] = boxmaxs[1] = boxmaxs[2] = 9999;
#else
	int		i;

	for (i=0 ; i<3 ; i++)
	{
		if (end[i] > start[i])
		{
			boxmins[i] = start[i] + mins[i] - 1;
			boxmaxs[i] = end[i] + maxs[i] + 1;
		}
		else
		{
			boxmins[i] = end[i] + mins[i] - 1;
			boxmaxs[i] = start[i] + maxs[i] + 1;
		}
	}
#endif
}

//=============================================

void SV_AntilagReset (edict_t *ent)
{
	client_t *cl;

	if (ent->e.entnum == 0 || ent->e.entnum > MAX_CLIENTS)
		return;

	cl = &svs.clients[ent->e.entnum - 1];
	cl->antilag_position_next = 0;
	cl->antilag_position_count = 0;
	cl->antilag_pending_discontinuity = true;
	memset(cl->antilag_positions, 0, sizeof(cl->antilag_positions));
	cl->antilag_recent_head = 0;
	cl->antilag_recent_count = 0;
	memset(&cl->antilag_last_decision, 0, sizeof(cl->antilag_last_decision));
	memset(cl->antilag_recent, 0, sizeof(cl->antilag_recent));
}

static int SV_AntilagOldestIndex(client_t *cl)
{
	int sample_count = bound(0, cl->antilag_position_count, MAX_ANTILAG_POSITIONS);

	if (sample_count <= 0) {
		return 0;
	}

	return (cl->antilag_position_next - sample_count + MAX_ANTILAG_POSITIONS) % MAX_ANTILAG_POSITIONS;
}

static qbool SV_AntilagPositionAlive(client_t *cl)
{
	return (cl->spectator || cl->edict->v->health > 0);
}

void SV_AntilagRecord(client_t *cl, qbool discontinuity)
{
	antilag_position_t *position;
	int index;
	int newest;
	qbool mark_discontinuity;
	qbool alive_now;
	double history_window;
	double teleport_dist;
	vec3_t origin_delta;

	if (!cl || !cl->edict) {
		return;
	}

	alive_now = SV_AntilagPositionAlive(cl);
	mark_discontinuity = (discontinuity || cl->antilag_pending_discontinuity);
	newest = (cl->antilag_position_next + MAX_ANTILAG_POSITIONS - 1) % MAX_ANTILAG_POSITIONS;

	if (cl->antilag_position_count > 0) {
		antilag_position_t *newest_position = &cl->antilag_positions[newest];
		qbool newest_alive = ((newest_position->flags & ANTILAG_POSITION_ALIVE) != 0);

		if (newest_alive != alive_now) {
			mark_discontinuity = true;
		}

		teleport_dist = max(0.0, sv_antilag_teleport_dist.value);
		if (teleport_dist > 0.0) {
			double teleport_dist_sq = teleport_dist * teleport_dist;
			double moved_dist_sq;

			VectorSubtract(cl->edict->v->origin, newest_position->origin, origin_delta);
			moved_dist_sq = DotProduct(origin_delta, origin_delta);
			if (moved_dist_sq > teleport_dist_sq) {
				mark_discontinuity = true;
			}
		}
	}

	// Drop duplicate/out-of-order timestamps, but keep discontinuity markers if needed.
	if (cl->antilag_position_count > 0 && cl->antilag_positions[newest].localtime >= cl->localtime) {
		if (mark_discontinuity) {
			cl->antilag_positions[newest].flags |= ANTILAG_POSITION_DISCONTINUITY;
			cl->antilag_pending_discontinuity = false;
		}
		return;
	}

	index = cl->antilag_position_next;
	position = &cl->antilag_positions[index];
	position->localtime = cl->localtime;
	VectorCopy(cl->edict->v->origin, position->origin);
	VectorCopy(cl->edict->v->mins, position->mins);
	VectorCopy(cl->edict->v->maxs, position->maxs);
	position->flags = 0;
	if (mark_discontinuity) {
		position->flags |= ANTILAG_POSITION_DISCONTINUITY;
	}
	if (alive_now) {
		position->flags |= ANTILAG_POSITION_ALIVE;
	}
	cl->antilag_pending_discontinuity = false;

	cl->antilag_position_next = (cl->antilag_position_next + 1) % MAX_ANTILAG_POSITIONS;
	if (cl->antilag_position_count < MAX_ANTILAG_POSITIONS) {
		cl->antilag_position_count++;
	}

	history_window = max(0.0, sv_antilag_maxunlag.value);
	if (history_window <= 0) {
		return;
	}

	// Keep only entries inside the configured rewind window.
	while (cl->antilag_position_count > 1) {
		int oldest = SV_AntilagOldestIndex(cl);

		if (cl->localtime - cl->antilag_positions[oldest].localtime <= history_window) {
			break;
		}

		cl->antilag_position_count--;
	}
}

qbool SV_AntilagFindHistory(client_t *cl, double target_time, const antilag_position_t **base, const antilag_position_t **interpolate)
{
	const antilag_position_t *previous;
	const antilag_position_t *current;
	const antilag_position_t *oldest;
	const antilag_position_t *newest;
	int sample_count;
	int oldest_index;
	int newest_index;
	int i;

	if (!cl) {
		return false;
	}

	sample_count = bound(0, cl->antilag_position_count, MAX_ANTILAG_POSITIONS);
	if (sample_count <= 0) {
		return false;
	}

	oldest_index = SV_AntilagOldestIndex(cl);
	newest_index = (cl->antilag_position_next + MAX_ANTILAG_POSITIONS - 1) % MAX_ANTILAG_POSITIONS;
	oldest = &cl->antilag_positions[oldest_index];
	newest = &cl->antilag_positions[newest_index];

	if (sample_count == 1 || target_time <= oldest->localtime) {
		*base = *interpolate = oldest;
		return true;
	}

	if (target_time >= newest->localtime) {
		*base = *interpolate = newest;
		return true;
	}

	previous = oldest;
	for (i = 1; i < sample_count; ++i) {
		int index = (oldest_index + i) % MAX_ANTILAG_POSITIONS;
		current = &cl->antilag_positions[index];

		if (target_time <= current->localtime) {
			if ((previous->flags | current->flags) & ANTILAG_POSITION_DISCONTINUITY) {
				if (fabs(target_time - previous->localtime) <= fabs(current->localtime - target_time)) {
					*base = *interpolate = previous;
				}
				else {
					*base = *interpolate = current;
				}
			}
			else {
				*base = previous;
				*interpolate = current;
			}
			return true;
		}

		previous = current;
	}

	*base = *interpolate = newest;
	return true;
}

qbool SV_AntilagResolvePosition(client_t *cl, double target_time, vec3_t out_origin, antilag_resolve_mode_t *mode)
{
	const antilag_position_t *base = NULL, *interpolate = NULL;
	const antilag_position_t *oldest;
	const antilag_position_t *newest;
	int sample_count;
	int oldest_index;
	int newest_index;
	double span;
	double factor;

	if (mode) {
		*mode = antilag_resolve_none;
	}

	if (!SV_AntilagFindHistory(cl, target_time, &base, &interpolate)) {
		return false;
	}

	sample_count = bound(0, cl->antilag_position_count, MAX_ANTILAG_POSITIONS);
	oldest_index = SV_AntilagOldestIndex(cl);
	newest_index = (cl->antilag_position_next + MAX_ANTILAG_POSITIONS - 1) % MAX_ANTILAG_POSITIONS;
	oldest = &cl->antilag_positions[oldest_index];
	newest = &cl->antilag_positions[newest_index];

	if (base == interpolate) {
		VectorCopy(base->origin, out_origin);

		if (mode) {
			if (sample_count == 1) {
				*mode = antilag_resolve_single;
			}
			else if (base == oldest) {
				*mode = antilag_resolve_oldest;
			}
			else if (base == newest) {
				*mode = antilag_resolve_newest;
			}
			else {
				*mode = antilag_resolve_discontinuity;
			}
		}
		return true;
	}

	span = interpolate->localtime - base->localtime;
	if (span <= 0 || IS_NAN(span)) {
		// Fallback policy for invalid brackets: use nearest sample.
		if (fabs(target_time - base->localtime) <= fabs(interpolate->localtime - target_time)) {
			VectorCopy(base->origin, out_origin);
		}
		else {
			VectorCopy(interpolate->origin, out_origin);
		}

		if (mode) {
			*mode = antilag_resolve_bad_interval;
		}
		return true;
	}

	factor = (target_time - base->localtime) / span;
	if (IS_NAN(factor)) {
		factor = 0;
	}
	factor = bound(0, factor, 1);
	VectorInterpolate(base->origin, factor, interpolate->origin, out_origin);

	if (mode) {
		*mode = antilag_resolve_interpolate;
	}
	return true;
}

static client_t *SV_AntilagProjectileOwnerClient(edict_t *projectile)
{
	edict_t *owner_edict;
	int owner_entnum;

	if (!projectile || !projectile->v->owner) {
		return NULL;
	}

	owner_edict = PROG_TO_EDICT(projectile->v->owner);
	if (!owner_edict || owner_edict->e.free) {
		return NULL;
	}

	owner_entnum = owner_edict->e.entnum;
	if (owner_entnum < 1 || owner_entnum > MAX_CLIENTS) {
		return NULL;
	}

	if (svs.clients[owner_entnum - 1].isBot) {
		return NULL;
	}

	return &svs.clients[owner_entnum - 1];
}

static qbool SV_AntilagListContainsToken(const char *list, const char *token)
{
	const char *cursor;
	const char *start;
	int token_len;
	int token_entry_len;

	if (!list || !list[0] || !token || !token[0]) {
		return false;
	}

	token_len = (int)strlen(token);
	if (token_len <= 0) {
		return false;
	}

	cursor = list;
	while (*cursor) {
		while (*cursor && (*cursor == ',' || *cursor == ';' || *cursor == '|' || *cursor <= ' ')) {
			cursor++;
		}
		if (!*cursor) {
			break;
		}

		start = cursor;
		while (*cursor && *cursor != ',' && *cursor != ';' && *cursor != '|' && *cursor > ' ') {
			cursor++;
		}

		token_entry_len = (int)(cursor - start);
		if (token_entry_len == token_len && !strncasecmp(start, token, token_len)) {
			return true;
		}
	}

	return false;
}

static qbool SV_AntilagProjectileOptInAllowed(edict_t *projectile)
{
	const char *classname = "";
	qbool explicit_optin;
	qbool allow_list_match;
	qbool deny_list_match;
	int optin_mode;

	if (!projectile) {
		return false;
	}

	if (projectile->v->classname) {
		classname = PR_GetEntityString(projectile->v->classname);
	}

	explicit_optin = (((int)projectile->v->flags & FL_LAGGEDMOVE) != 0);
	allow_list_match = SV_AntilagListContainsToken(sv_antilag_projectile_allow.string, classname);
	deny_list_match = SV_AntilagListContainsToken(sv_antilag_projectile_deny.string, classname);
	optin_mode = bound(0, sv_antilag_projectile_optin.integer, 2);

	if (deny_list_match) {
		return false;
	}
	if (optin_mode == 0) {
		return true;
	}
	if (optin_mode == 1) {
		return (explicit_optin || allow_list_match);
	}

	return allow_list_match;
}

static qbool SV_AntilagProjectileOwnerFresh(client_t *owner)
{
	double stale_limit;
	double age;

	if (!owner || owner->state != cs_spawned || owner->spectator || owner->laggedents_count == 0) {
		return false;
	}

	stale_limit = max(0.0, sv_antilag_projectile_owner_stale.value);
	age = sv.time - owner->laggedents_time;
	if (age < 0) {
		age = 0;
	}

	if (stale_limit <= 0) {
		return true;
	}

	return age <= stale_limit;
}

static float SV_AntilagProjectileBlendFraction(client_t *owner)
{
	float fraction;
	double nudge_window;
	double rewind_depth;

	if (!owner) {
		return 0.0f;
	}

	fraction = bound(0.0f, owner->laggedents_frac, 1.0f);
	if (SV_AntilagProjectileModeResolved() != 1) {
		return fraction;
	}

	nudge_window = max(0.0, sv_antilag_projectile_nudge.value);
	if (nudge_window <= 0) {
		return 0.0f;
	}

	rewind_depth = sv.time - owner->laggedents_time;
	if (rewind_depth <= 0) {
		return fraction;
	}

	return (float)bound(0.0, (fraction * (nudge_window / rewind_depth)), fraction);
}

static qbool SV_AntilagProjectileShouldUseLagged(edict_t *projectile, client_t **owner_out)
{
	client_t *owner;
	int mode;

	if (!projectile || SV_AntilagGameplayModeResolved() != 2) {
		return false;
	}

	mode = SV_AntilagProjectileModeResolved();
	if (mode == 0) {
		return false;
	}

	if (!SV_AntilagProjectileOptInAllowed(projectile)) {
		return false;
	}

	owner = SV_AntilagProjectileOwnerClient(projectile);
	if (!SV_AntilagProjectileOwnerFresh(owner)) {
		return false;
	}

	if (owner_out) {
		*owner_out = owner;
	}
	return true;
}

int SV_AntilagApplyTracePolicy(int traceflags, edict_t *passedict, antilag_trace_policy_t policy)
{
	int flags = (traceflags & ~MOVE_LAGGED);
	int antilag_mode;
	int entnum;

	if (!passedict) {
		return flags;
	}

	antilag_mode = SV_AntilagGameplayModeResolved();
	if (!antilag_mode) {
		return flags;
	}

	entnum = passedict->e.entnum;
	if (entnum >= 1 && entnum <= MAX_CLIENTS && svs.clients[entnum - 1].isBot) {
		return flags;
	}

	switch (policy) {
		case antilag_trace_hitscan:
		case antilag_trace_melee:
			// Phase 10 policy: rollout stage gates when gameplay rewind can be applied.
			if (antilag_mode >= 1) {
				flags |= MOVE_LAGGED;
			}
			break;
		case antilag_trace_projectile:
			if (SV_AntilagProjectileShouldUseLagged(passedict, NULL)) {
				flags |= MOVE_LAGGED;
			}
			break;
		default:
			break;
	}

	return flags;
}

trace_t SV_AntilagTrace(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int traceflags, edict_t *passedict, antilag_trace_policy_t policy)
{
	return SV_Trace(start, mins, maxs, end, SV_AntilagApplyTracePolicy(traceflags, passedict, policy), passedict);
}

static qbool SV_AntilagReplayVecEqual(vec3_t a, vec3_t b, float epsilon)
{
	return fabs(a[0] - b[0]) <= epsilon
		&& fabs(a[1] - b[1]) <= epsilon
		&& fabs(a[2] - b[2]) <= epsilon;
}

static int SV_AntilagReplayScenario(const char *name, const antilag_position_t *positions, int count, double target_time, vec3_t expected_origin, antilag_resolve_mode_t expected_mode)
{
	client_t test_client;
	antilag_resolve_mode_t mode = antilag_resolve_none;
	vec3_t resolved = { 0 };
	int i;

	memset(&test_client, 0, sizeof(test_client));
	if (count <= 0 || count > MAX_ANTILAG_POSITIONS) {
		Con_Printf("sv_antilag_replaytests: %s invalid test data count=%d\n", name, count);
		return 1;
	}

	for (i = 0; i < count; ++i) {
		test_client.antilag_positions[i] = positions[i];
	}
	test_client.antilag_position_count = count;
	test_client.antilag_position_next = count % MAX_ANTILAG_POSITIONS;

	if (!SV_AntilagResolvePosition(&test_client, target_time, resolved, &mode)) {
		Con_Printf("sv_antilag_replaytests: %s failed to resolve history\n", name);
		return 1;
	}

	if (mode != expected_mode) {
		Con_Printf("sv_antilag_replaytests: %s mode mismatch got=%d expected=%d\n", name, (int)mode, (int)expected_mode);
		return 1;
	}

	if (!SV_AntilagReplayVecEqual(resolved, expected_origin, 0.05f)) {
		Con_Printf("sv_antilag_replaytests: %s origin mismatch got=(%0.2f %0.2f %0.2f) expected=(%0.2f %0.2f %0.2f)\n",
			name,
			resolved[0], resolved[1], resolved[2],
			expected_origin[0], expected_origin[1], expected_origin[2]);
		return 1;
	}

	return 0;
}

static int SV_AntilagReplayTracePolicyScenario(const char *name, int traceflags, edict_t *passedict, antilag_trace_policy_t policy, int rollout_stage, int antilag_mode, qbool attacker_is_bot, int expected_flags)
{
	float saved_antilag_mode;
	int saved_antilag_mode_integer;
	float saved_rollout_stage;
	int saved_rollout_stage_integer;
	qbool saved_is_bot = false;
	int entnum = 0;
	int resolved_flags;

	saved_antilag_mode = sv_antilag.value;
	saved_antilag_mode_integer = sv_antilag.integer;
	saved_rollout_stage = sv_antilag_rollout_stage.value;
	saved_rollout_stage_integer = sv_antilag_rollout_stage.integer;

	sv_antilag.value = (float)bound(0, antilag_mode, 2);
	sv_antilag.integer = bound(0, antilag_mode, 2);
	sv_antilag_rollout_stage.value = (float)bound(0, rollout_stage, 3);
	sv_antilag_rollout_stage.integer = bound(0, rollout_stage, 3);

	if (passedict) {
		entnum = passedict->e.entnum;
		if (entnum >= 1 && entnum <= MAX_CLIENTS) {
			saved_is_bot = svs.clients[entnum - 1].isBot;
			svs.clients[entnum - 1].isBot = attacker_is_bot;
		}
	}

	resolved_flags = SV_AntilagApplyTracePolicy(traceflags, passedict, policy);

	if (passedict && entnum >= 1 && entnum <= MAX_CLIENTS) {
		svs.clients[entnum - 1].isBot = saved_is_bot;
	}
	sv_antilag.value = saved_antilag_mode;
	sv_antilag.integer = saved_antilag_mode_integer;
	sv_antilag_rollout_stage.value = saved_rollout_stage;
	sv_antilag_rollout_stage.integer = saved_rollout_stage_integer;

	if (resolved_flags != expected_flags) {
		Con_Printf("sv_antilag_replaytests: %s policy mismatch got=0x%X expected=0x%X\n",
			name, resolved_flags, expected_flags);
		return 1;
	}

	return 0;
}

static int SV_AntilagReplayProjectilePolicyScenario(const char *name, int traceflags, edict_t *projectile, int expected_flags)
{
	int resolved_flags = SV_AntilagApplyTracePolicy(traceflags, projectile, antilag_trace_projectile);

	if (resolved_flags != expected_flags) {
		Con_Printf("sv_antilag_replaytests: %s policy mismatch got=0x%X expected=0x%X\n",
			name, resolved_flags, expected_flags);
		return 1;
	}

	return 0;
}

static int SV_AntilagReplayFloatScenario(const char *name, float actual, float expected, float epsilon)
{
	if (fabs(actual - expected) > epsilon) {
		Con_Printf("sv_antilag_replaytests: %s float mismatch got=%0.4f expected=%0.4f\n",
			name, actual, expected);
		return 1;
	}

	return 0;
}

int SV_AntilagRunReplayTests(void)
{
	int failures = 0;
	antilag_position_t positions[4];
	vec3_t expected;
	edict_t policy_attacker;
	int policy_baseflags;

	memset(positions, 0, sizeof(positions));

	// Peek shot: target exits cover between two snapshots.
	positions[0].localtime = 10.000; VectorSet(positions[0].origin, 0, 0, 0);   positions[0].flags = ANTILAG_POSITION_ALIVE;
	positions[1].localtime = 10.050; VectorSet(positions[1].origin, 32, 0, 0);  positions[1].flags = ANTILAG_POSITION_ALIVE;
	positions[2].localtime = 10.100; VectorSet(positions[2].origin, 64, 0, 0);  positions[2].flags = ANTILAG_POSITION_ALIVE;
	VectorSet(expected, 48, 0, 0);
	failures += SV_AntilagReplayScenario("peek_shot", positions, 3, 10.075, expected, antilag_resolve_interpolate);

	memset(positions, 0, sizeof(positions));
	// Crossing strafe: fast side-crossing around shot time.
	positions[0].localtime = 20.000; VectorSet(positions[0].origin, -80, 0, 0); positions[0].flags = ANTILAG_POSITION_ALIVE;
	positions[1].localtime = 20.030; VectorSet(positions[1].origin, -20, 0, 0); positions[1].flags = ANTILAG_POSITION_ALIVE;
	positions[2].localtime = 20.060; VectorSet(positions[2].origin,  20, 0, 0); positions[2].flags = ANTILAG_POSITION_ALIVE;
	positions[3].localtime = 20.090; VectorSet(positions[3].origin,  80, 0, 0); positions[3].flags = ANTILAG_POSITION_ALIVE;
	VectorSet(expected, 0, 0, 0);
	failures += SV_AntilagReplayScenario("crossing_strafe", positions, 4, 20.045, expected, antilag_resolve_interpolate);

	memset(positions, 0, sizeof(positions));
	// Jitter target: uneven command/sample spacing should still interpolate deterministically.
	positions[0].localtime = 30.000; VectorSet(positions[0].origin,  0, 0, 0);  positions[0].flags = ANTILAG_POSITION_ALIVE;
	positions[1].localtime = 30.008; VectorSet(positions[1].origin,  8, 0, 0);  positions[1].flags = ANTILAG_POSITION_ALIVE;
	positions[2].localtime = 30.041; VectorSet(positions[2].origin, 41, 0, 0);  positions[2].flags = ANTILAG_POSITION_ALIVE;
	positions[3].localtime = 30.043; VectorSet(positions[3].origin, 43, 0, 0);  positions[3].flags = ANTILAG_POSITION_ALIVE;
	VectorSet(expected, 24, 0, 0);
	failures += SV_AntilagReplayScenario("jitter_target", positions, 4, 30.024, expected, antilag_resolve_interpolate);

	memset(&policy_attacker, 0, sizeof(policy_attacker));
	policy_attacker.e.entnum = 1;
	policy_baseflags = MOVE_NOMONSTERS;

	// Phase 6/10 policy parity: stage 1 enables hitscan/melee, stage 0 keeps instrumentation-only behavior.
	failures += SV_AntilagReplayTracePolicyScenario("policy_hitscan_mode1", policy_baseflags, &policy_attacker, antilag_trace_hitscan, 1, 1, false, policy_baseflags | MOVE_LAGGED);
	failures += SV_AntilagReplayTracePolicyScenario("policy_melee_mode1", policy_baseflags, &policy_attacker, antilag_trace_melee, 1, 1, false, policy_baseflags | MOVE_LAGGED);
	failures += SV_AntilagReplayTracePolicyScenario("policy_hitscan_mode0", policy_baseflags | MOVE_LAGGED, &policy_attacker, antilag_trace_hitscan, 1, 0, false, policy_baseflags);
	failures += SV_AntilagReplayTracePolicyScenario("policy_hitscan_rollout_stage0", policy_baseflags | MOVE_LAGGED, &policy_attacker, antilag_trace_hitscan, 0, 2, false, policy_baseflags);

	// Projectile policy remains isolated from mode 1 and bot attackers always bypass lagged rewinds.
	failures += SV_AntilagReplayTracePolicyScenario("policy_projectile_mode1_off", policy_baseflags, &policy_attacker, antilag_trace_projectile, 1, 1, false, policy_baseflags);
	failures += SV_AntilagReplayTracePolicyScenario("policy_hitscan_bot_bypass", policy_baseflags, &policy_attacker, antilag_trace_hitscan, 3, 2, true, policy_baseflags);
	failures += SV_AntilagReplayTracePolicyScenario("policy_melee_null_attacker", policy_baseflags | MOVE_LAGGED, NULL, antilag_trace_melee, 3, 2, false, policy_baseflags);

	// Phase 7 projectile policy matrix: mode tiers, staleness, opt-in/allow/deny, and blend behavior.
	if (sv.edicts && sv.num_edicts > 1 && pr_edict_size > 0) {
		edict_t projectile;
		entvars_t projectile_vars;
		edict_t *owner_edict = EDICT_NUM(1);
		client_t *owner_client = &svs.clients[0];
		double saved_sv_time = sv.time;
		float saved_antilag_mode = sv_antilag.value;
		int saved_antilag_mode_integer = sv_antilag.integer;
		float saved_rollout_stage = sv_antilag_rollout_stage.value;
		int saved_rollout_stage_integer = sv_antilag_rollout_stage.integer;
		float saved_playtest_signoff = sv_antilag_projectile_playtest_signoff.value;
		int saved_playtest_signoff_integer = sv_antilag_projectile_playtest_signoff.integer;
		float saved_full_admin = sv_antilag_projectile_full_admin.value;
		int saved_full_admin_integer = sv_antilag_projectile_full_admin.integer;
		float saved_projectile_mode = sv_antilag_projectile_mode.value;
		int saved_projectile_mode_integer = sv_antilag_projectile_mode.integer;
		float saved_optin_mode = sv_antilag_projectile_optin.value;
		int saved_optin_mode_integer = sv_antilag_projectile_optin.integer;
		float saved_stale_limit = sv_antilag_projectile_owner_stale.value;
		float saved_nudge_window = sv_antilag_projectile_nudge.value;
		sv_client_state_t saved_owner_state = owner_client->state;
		int saved_owner_spectator = owner_client->spectator;
		unsigned int saved_owner_laggedents_count = owner_client->laggedents_count;
		float saved_owner_laggedents_frac = owner_client->laggedents_frac;
		float saved_owner_laggedents_time = owner_client->laggedents_time;
		qbool saved_owner_is_bot = owner_client->isBot;
		char *saved_allow_list = sv_antilag_projectile_allow.string;
		char *saved_deny_list = sv_antilag_projectile_deny.string;
		char projectile_classname[] = "rocket";

		memset(&projectile, 0, sizeof(projectile));
		memset(&projectile_vars, 0, sizeof(projectile_vars));
		projectile.v = &projectile_vars;
		projectile.e.entnum = MAX_CLIENTS + 1;
		projectile_vars.owner = EDICT_TO_PROG(owner_edict);
		PR1_SetString(&projectile_vars.classname, projectile_classname);

		sv.time = 100.0;
		sv_antilag.value = 2.0f;
		sv_antilag.integer = 2;
		sv_antilag_rollout_stage.value = 3.0f;
		sv_antilag_rollout_stage.integer = 3;
		sv_antilag_projectile_playtest_signoff.value = 1.0f;
		sv_antilag_projectile_playtest_signoff.integer = 1;
		sv_antilag_projectile_full_admin.value = 1.0f;
		sv_antilag_projectile_full_admin.integer = 1;
		sv_antilag_projectile_mode.value = 2.0f;
		sv_antilag_projectile_mode.integer = 2;
		sv_antilag_projectile_optin.value = 0.0f;
		sv_antilag_projectile_optin.integer = 0;
		sv_antilag_projectile_owner_stale.value = 0.25f;
		sv_antilag_projectile_nudge.value = 0.05f;
		sv_antilag_projectile_allow.string = "";
		sv_antilag_projectile_deny.string = "";
		projectile_vars.flags = 0;
		owner_client->state = cs_spawned;
		owner_client->spectator = 0;
		owner_client->laggedents_count = 1;
		owner_client->laggedents_frac = 0.8f;
		owner_client->laggedents_time = (float)(sv.time - 0.2);
		owner_client->isBot = false;

		failures += SV_AntilagReplayProjectilePolicyScenario("policy_projectile_mode2_full", policy_baseflags, &projectile, policy_baseflags | MOVE_LAGGED);

		sv_antilag_projectile_mode.value = 1.0f;
		sv_antilag_projectile_mode.integer = 1;
		failures += SV_AntilagReplayProjectilePolicyScenario("policy_projectile_mode1_limited", policy_baseflags, &projectile, policy_baseflags | MOVE_LAGGED);
		failures += SV_AntilagReplayFloatScenario("policy_projectile_blend_mode1_nudged", SV_AntilagProjectileBlendFraction(owner_client), 0.20f, 0.001f);

		sv_antilag_rollout_stage.value = 2.0f;
		sv_antilag_rollout_stage.integer = 2;
		sv_antilag_projectile_playtest_signoff.value = 0.0f;
		sv_antilag_projectile_playtest_signoff.integer = 0;
		failures += SV_AntilagReplayProjectilePolicyScenario("policy_projectile_rollout_stage2_no_signoff", policy_baseflags, &projectile, policy_baseflags);
		sv_antilag_projectile_playtest_signoff.value = 1.0f;
		sv_antilag_projectile_playtest_signoff.integer = 1;
		failures += SV_AntilagReplayProjectilePolicyScenario("policy_projectile_rollout_stage2_signoff", policy_baseflags, &projectile, policy_baseflags | MOVE_LAGGED);

		sv_antilag_rollout_stage.value = 0.0f;
		sv_antilag_rollout_stage.integer = 0;
		failures += SV_AntilagReplayProjectilePolicyScenario("policy_projectile_rollout_stage0_off", policy_baseflags, &projectile, policy_baseflags);
		sv_antilag_rollout_stage.value = 3.0f;
		sv_antilag_rollout_stage.integer = 3;

		sv_antilag_projectile_mode.value = 2.0f;
		sv_antilag_projectile_mode.integer = 2;
		sv_antilag_projectile_full_admin.value = 0.0f;
		sv_antilag_projectile_full_admin.integer = 0;
		failures += SV_AntilagReplayProjectilePolicyScenario("policy_projectile_mode2_admin_gate", policy_baseflags, &projectile, policy_baseflags | MOVE_LAGGED);
		failures += SV_AntilagReplayFloatScenario("policy_projectile_mode2_admin_gate_blend", SV_AntilagProjectileBlendFraction(owner_client), 0.20f, 0.001f);
		sv_antilag_projectile_full_admin.value = 1.0f;
		sv_antilag_projectile_full_admin.integer = 1;
		failures += SV_AntilagReplayFloatScenario("policy_projectile_blend_mode2_full", SV_AntilagProjectileBlendFraction(owner_client), 0.80f, 0.001f);

		sv_antilag_projectile_mode.value = 0.0f;
		sv_antilag_projectile_mode.integer = 0;
		failures += SV_AntilagReplayProjectilePolicyScenario("policy_projectile_mode0_off", policy_baseflags | MOVE_LAGGED, &projectile, policy_baseflags);

		sv_antilag_projectile_mode.value = 2.0f;
		sv_antilag_projectile_mode.integer = 2;
		owner_client->laggedents_time = (float)(sv.time - 1.0);
		failures += SV_AntilagReplayProjectilePolicyScenario("policy_projectile_owner_stale", policy_baseflags, &projectile, policy_baseflags);

		sv_antilag_projectile_owner_stale.value = 0.0f;
		failures += SV_AntilagReplayProjectilePolicyScenario("policy_projectile_owner_stale_unbounded", policy_baseflags, &projectile, policy_baseflags | MOVE_LAGGED);
		sv_antilag_projectile_owner_stale.value = 0.25f;
		owner_client->laggedents_time = (float)(sv.time - 0.2);

		sv_antilag_projectile_optin.value = 1.0f;
		sv_antilag_projectile_optin.integer = 1;
		projectile_vars.flags = 0;
		failures += SV_AntilagReplayProjectilePolicyScenario("policy_projectile_optin1_requires_optin", policy_baseflags, &projectile, policy_baseflags);

		projectile_vars.flags = FL_LAGGEDMOVE;
		failures += SV_AntilagReplayProjectilePolicyScenario("policy_projectile_optin1_flag", policy_baseflags, &projectile, policy_baseflags | MOVE_LAGGED);

		projectile_vars.flags = 0;
		sv_antilag_projectile_allow.string = "rocket";
		failures += SV_AntilagReplayProjectilePolicyScenario("policy_projectile_optin1_allow", policy_baseflags, &projectile, policy_baseflags | MOVE_LAGGED);

		sv_antilag_projectile_optin.value = 2.0f;
		sv_antilag_projectile_optin.integer = 2;
		sv_antilag_projectile_allow.string = "";
		projectile_vars.flags = FL_LAGGEDMOVE;
		failures += SV_AntilagReplayProjectilePolicyScenario("policy_projectile_optin2_flag_only_off", policy_baseflags, &projectile, policy_baseflags);

		projectile_vars.flags = 0;
		sv_antilag_projectile_allow.string = "rocket";
		failures += SV_AntilagReplayProjectilePolicyScenario("policy_projectile_optin2_allow", policy_baseflags, &projectile, policy_baseflags | MOVE_LAGGED);

		sv_antilag_projectile_optin.value = 0.0f;
		sv_antilag_projectile_optin.integer = 0;
		sv_antilag_projectile_allow.string = "rocket";
		sv_antilag_projectile_deny.string = "rocket";
		projectile_vars.flags = FL_LAGGEDMOVE;
		failures += SV_AntilagReplayProjectilePolicyScenario("policy_projectile_deny_overrides_allow", policy_baseflags, &projectile, policy_baseflags);

		projectile_vars.flags = 0;
		sv_antilag_projectile_allow.string = "";
		sv_antilag_projectile_deny.string = "";
		owner_client->isBot = true;
		failures += SV_AntilagReplayProjectilePolicyScenario("policy_projectile_owner_bot_bypass", policy_baseflags, &projectile, policy_baseflags);

		owner_client->isBot = false;
		owner_client->state = cs_connected;
		failures += SV_AntilagReplayProjectilePolicyScenario("policy_projectile_owner_state_gate", policy_baseflags, &projectile, policy_baseflags);

		owner_client->state = cs_spawned;
		owner_client->spectator = 1;
		failures += SV_AntilagReplayProjectilePolicyScenario("policy_projectile_owner_spectator_gate", policy_baseflags, &projectile, policy_baseflags);

		owner_client->spectator = 0;
		owner_client->laggedents_count = 0;
		failures += SV_AntilagReplayProjectilePolicyScenario("policy_projectile_owner_no_history", policy_baseflags, &projectile, policy_baseflags);

		owner_client->laggedents_count = 1;
		owner_client->laggedents_frac = 0.8f;
		owner_client->laggedents_time = (float)sv.time;
		sv_antilag_projectile_mode.value = 1.0f;
		sv_antilag_projectile_mode.integer = 1;
		sv_antilag_projectile_nudge.value = 0.0f;
		failures += SV_AntilagReplayFloatScenario("policy_projectile_blend_mode1_zero_nudge", SV_AntilagProjectileBlendFraction(owner_client), 0.0f, 0.001f);
		sv_antilag_projectile_nudge.value = 0.05f;
		failures += SV_AntilagReplayFloatScenario("policy_projectile_blend_nonpositive_depth", SV_AntilagProjectileBlendFraction(owner_client), 0.8f, 0.001f);

		sv.time = saved_sv_time;
		sv_antilag.value = saved_antilag_mode;
		sv_antilag.integer = saved_antilag_mode_integer;
		sv_antilag_rollout_stage.value = saved_rollout_stage;
		sv_antilag_rollout_stage.integer = saved_rollout_stage_integer;
		sv_antilag_projectile_playtest_signoff.value = saved_playtest_signoff;
		sv_antilag_projectile_playtest_signoff.integer = saved_playtest_signoff_integer;
		sv_antilag_projectile_full_admin.value = saved_full_admin;
		sv_antilag_projectile_full_admin.integer = saved_full_admin_integer;
		sv_antilag_projectile_mode.value = saved_projectile_mode;
		sv_antilag_projectile_mode.integer = saved_projectile_mode_integer;
		sv_antilag_projectile_optin.value = saved_optin_mode;
		sv_antilag_projectile_optin.integer = saved_optin_mode_integer;
		sv_antilag_projectile_owner_stale.value = saved_stale_limit;
		sv_antilag_projectile_nudge.value = saved_nudge_window;
		sv_antilag_projectile_allow.string = saved_allow_list;
		sv_antilag_projectile_deny.string = saved_deny_list;
		owner_client->state = saved_owner_state;
		owner_client->spectator = saved_owner_spectator;
		owner_client->laggedents_count = saved_owner_laggedents_count;
		owner_client->laggedents_frac = saved_owner_laggedents_frac;
		owner_client->laggedents_time = saved_owner_laggedents_time;
		owner_client->isBot = saved_owner_is_bot;
	}
	else {
		Con_Printf("sv_antilag_replaytests: skipped phase7 projectile policy checks (server edicts unavailable)\n");
	}

	if (failures == 0) {
		Con_Printf("sv_antilag_replaytests: all deterministic replay scenarios passed\n");
	}

	return failures;
}

static float SV_AntilagBoundsRadius(vec3_t mins, vec3_t maxs)
{
	vec3_t extent;

	extent[0] = max(fabs(mins[0]), fabs(maxs[0]));
	extent[1] = max(fabs(mins[1]), fabs(maxs[1]));
	extent[2] = max(fabs(mins[2]), fabs(maxs[2]));

	return VectorLength(extent);
}

static float SV_AntilagPointToSegmentDistanceSq(vec3_t point, vec3_t start, vec3_t end)
{
	vec3_t segment, to_point, closest, diff;
	float segment_len_sq;
	float t;

	VectorSubtract(end, start, segment);
	segment_len_sq = DotProduct(segment, segment);
	if (segment_len_sq <= 0) {
		VectorSubtract(point, start, diff);
		return DotProduct(diff, diff);
	}

	VectorSubtract(point, start, to_point);
	t = DotProduct(to_point, segment) / segment_len_sq;
	t = bound(0, t, 1);
	VectorMA(start, t, segment, closest);
	VectorSubtract(point, closest, diff);
	return DotProduct(diff, diff);
}

static qbool SV_AntilagPassesRayNarrow(moveclip_t *clip, edict_t *touch, vec3_t lagged_origin)
{
	vec3_t center_offset, center;
	float target_radius;
	float corridor_radius;
	float distance_sq;

	if (!w.ray_narrow) {
		return true;
	}

	VectorAdd(touch->v->mins, touch->v->maxs, center_offset);
	VectorScale(center_offset, 0.5f, center_offset);
	VectorAdd(lagged_origin, center_offset, center);

	target_radius = SV_AntilagBoundsRadius(touch->v->mins, touch->v->maxs);
	corridor_radius = target_radius + w.move_radius + w.ray_narrow_pad;
	distance_sq = SV_AntilagPointToSegmentDistanceSq(center, clip->start, clip->end);

	return distance_sq <= corridor_radius * corridor_radius;
}

void SV_AntilagClipSetUp ( areanode_t *node, moveclip_t *clip )
{
	edict_t *passedict = clip->passedict;
	int entnum = passedict->e.entnum;

	clip->type &= ~MOVE_LAGGED;
	w.owner = NULL;
	w.ray_narrow = false;
	w.ray_narrow_pad = max(0.0f, sv_antilag_ray_narrow_pad.value);
	w.move_radius = SV_AntilagBoundsRadius(clip->mins2, clip->maxs2);

	if (entnum && entnum <= MAX_CLIENTS && !svs.clients[entnum - 1].isBot)
	{
		clip->type |= MOVE_LAGGED;
		w.lagents = svs.clients[entnum - 1].laggedents;
		w.maxlagents = svs.clients[entnum - 1].laggedents_count;
		w.lagentsfrac = svs.clients[entnum - 1].laggedents_frac;
		w.owner = &svs.clients[entnum - 1];
	}
	else
	{
		client_t *owner = NULL;

		if (SV_AntilagProjectileShouldUseLagged(passedict, &owner))
		{
			clip->type |= MOVE_LAGGED;
			w.lagents = owner->laggedents;
			w.maxlagents = owner->laggedents_count;
			w.lagentsfrac = SV_AntilagProjectileBlendFraction(owner);
			w.owner = owner;
		}
	}

	w.ray_narrow = (sv_antilag_ray_narrow.integer != 0);
}

void SV_AntilagClipCheck ( areanode_t *node, moveclip_t *clip )
{
	double perf_start = Sys_DoubleTime();
	unsigned int candidates = 0;
	unsigned int broadphase_rejects = 0;
	unsigned int ray_rejects = 0;
	unsigned int trace_tests = 0;
	trace_t trace;
	edict_t *touch;
	vec3_t lp;
	int i;

	for (i = 0; i < w.maxlagents; i++)
	{
		if (clip->trace.allsolid)
			break;

		if (!w.lagents[i].present)
			continue;

		touch = EDICT_NUM(i + 1);
		if (touch->v->solid == SOLID_NOT)
			continue;
		if (touch == clip->passedict)
			continue;
		if (touch->v->solid == SOLID_TRIGGER)
			SV_Error ("Trigger (%s) in clipping list", PR_GetEntityString(touch->v->classname));

		if ((clip->type & MOVE_NOMONSTERS) && touch->v->solid != SOLID_BSP)
			continue;

		candidates++;
		VectorInterpolate(touch->v->origin, w.lagentsfrac, w.lagents[i].laggedpos, lp);

		if (   clip->boxmins[0] > lp[0]+touch->v->maxs[0]
			|| clip->boxmins[1] > lp[1]+touch->v->maxs[1]
			|| clip->boxmins[2] > lp[2]+touch->v->maxs[2]
			|| clip->boxmaxs[0] < lp[0]+touch->v->mins[0]
			|| clip->boxmaxs[1] < lp[1]+touch->v->mins[1]
			|| clip->boxmaxs[2] < lp[2]+touch->v->mins[2] )
		{
			broadphase_rejects++;
			continue;
		}

		if (!SV_AntilagPassesRayNarrow(clip, touch, lp)) {
			ray_rejects++;
			continue;
		}

		if (clip->passedict && clip->passedict->v->size[0] && !touch->v->size[0])
			continue;	// points never interact

		if (clip->passedict)
		{
			if (PROG_TO_EDICT(touch->v->owner) == clip->passedict)
				continue;	// don't clip against own missiles
			if (PROG_TO_EDICT(clip->passedict->v->owner) == touch)
				continue;	// don't clip against owner
		}

		if ((int)touch->v->flags & FL_MONSTER)
			trace = SV_ClipMoveToEntity (touch, &lp, clip->start, clip->mins2, clip->maxs2, clip->end);
		else
			trace = SV_ClipMoveToEntity (touch, &lp, clip->start, clip->mins, clip->maxs, clip->end);
		trace_tests++;

		// qqshka: I have NO idea why we keep startsolid but let do it.

		// make sure we keep a startsolid from a previous trace
		clip->trace.startsolid |= trace.startsolid;

		if ( trace.allsolid || trace.fraction < clip->trace.fraction )
		{
			// set edict
			trace.e.ent = touch;
			// make sure we keep a startsolid from a previous trace
			trace.startsolid |= clip->trace.startsolid;
			// bit by bit copy trace struct
			clip->trace = trace;
		}
	}

	if (w.owner) {
		w.owner->antilag_clip_candidates += candidates;
		w.owner->antilag_clip_broadphase_rejects += broadphase_rejects;
		w.owner->antilag_clip_ray_rejects += ray_rejects;
		w.owner->antilag_clip_traces += trace_tests;
	}
	SV_AntilagPerfAddClip(Sys_DoubleTime() - perf_start, candidates, broadphase_rejects, ray_rejects, trace_tests);
}

/*
==================
SV_Trace
==================
*/
trace_t SV_Trace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, edict_t *passedict)
{
	moveclip_t	clip;
	int			i;

	memset ( &clip, 0, sizeof ( moveclip_t ) );

	// clip to world
	clip.trace = SV_ClipMoveToEntity ( sv.edicts, NULL, start, mins, maxs, end );

	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.type = type;
	clip.passedict = passedict;

	if (type & MOVE_MISSILE)
	{
		for (i=0 ; i<3 ; i++)
		{
			clip.mins2[i] = -15;
			clip.maxs2[i] = 15;
		}
	}
	else
	{
		VectorCopy (mins, clip.mins2);
		VectorCopy (maxs, clip.maxs2);
	}

	// create the bounding box of the entire move
	SV_MoveBounds ( start, clip.mins2, clip.maxs2, end, clip.boxmins, clip.boxmaxs );

	// set up antilag
	if (clip.type & MOVE_LAGGED)
		SV_AntilagClipSetUp ( sv_areanodes, &clip );

	// clip to entities
	SV_ClipToLinks ( sv_areanodes, &clip );

	// additional antilag clip check
	if (clip.type & MOVE_LAGGED)
		SV_AntilagClipCheck ( sv_areanodes, &clip );

	return clip.trace;
}

#endif // !CLIENTONLY
