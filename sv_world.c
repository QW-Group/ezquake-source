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
	if (ent->v.solid == SOLID_BSP)
	{	// explicit hulls in the BSP model
		if (ent->v.movetype != MOVETYPE_PUSH)
			SV_Error ("SOLID_BSP without MOVETYPE_PUSH");

		if ((unsigned)ent->v.modelindex >= MAX_MODELS)
			SV_Error ("SV_HullForEntity: ent.modelindex >= MAX_MODELS");

		model = sv.models[(int)ent->v.modelindex];
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
		VectorAdd (offset, ent->v.origin, offset);
	}
	else
	{	// create a temp hull from bounding box sizes

		VectorSubtract (ent->v.mins, maxs, hullmins);
		VectorSubtract (ent->v.maxs, mins, hullmaxs);
		hull = CM_HullForBox (hullmins, hullmaxs);
		
		VectorCopy (ent->v.origin, offset);
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
	if (!ent->e->area.prev)
		return;		// not linked in anywhere
	RemoveLink (&ent->e->area);
	ent->e->area.prev = ent->e->area.next = NULL;
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
			if (touch->v.solid == SOLID_NOT)
				continue;

			if (mins[0] > touch->v.absmax[0]
						 || mins[1] > touch->v.absmax[1]
						 || mins[2] > touch->v.absmax[2]
						 || maxs[0] < touch->v.absmin[0]
						 || maxs[1] < touch->v.absmin[1]
						 || maxs[2] < touch->v.absmin[2])
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

	numtouch = SV_AreaEdicts (ent->v.absmin, ent->v.absmax, touchlist, sv.max_edicts, AREA_TRIGGERS);

// touch linked edicts
	for (i = 0; i < numtouch; i++)
	{
		touch = touchlist[i];
		if (touch == ent)
			continue;
		if (!touch->v.touch || touch->v.solid != SOLID_TRIGGER)
			continue;

		old_self = pr_global_struct->self;
		old_other = pr_global_struct->other;

		pr_global_struct->self = EDICT_TO_PROG(touch);
		pr_global_struct->other = EDICT_TO_PROG(ent);
		pr_global_struct->time = sv.time;
		PR_EdictTouch (touch->v.touch);

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

	ent->e->num_leafs = CM_FindTouchedLeafs (ent->v.absmin, ent->v.absmax, leafnums,
					      MAX_ENT_LEAFS, 0, NULL);
	for (i = 0; i < ent->e->num_leafs; i++) {
		// ent->e->leafnums are real leafnum minus one (for pvs checks)
		ent->e->leafnums[i] = leafnums[i] - 1;
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
	
	if (ent->e->area.prev)
		SV_UnlinkEdict (ent);	// unlink from old position
		
	if (ent == sv.edicts)
		return;		// don't add the world

	if (ent->e->free)
		return;

// set the abs box
	VectorAdd (ent->v.origin, ent->v.mins, ent->v.absmin);
	VectorAdd (ent->v.origin, ent->v.maxs, ent->v.absmax);

	//
// to make items easier to pick up and allow them to be grabbed off
// of shelves, the abs sizes are expanded
	//
	if ((int)ent->v.flags & FL_ITEM)
	{
		ent->v.absmin[0] -= 15;
		ent->v.absmin[1] -= 15;
		ent->v.absmax[0] += 15;
		ent->v.absmax[1] += 15;
	}
	else
	{	// because movement is clipped an epsilon away from an actual edge,
		// we must fully check even when bounding boxes don't quite touch
		ent->v.absmin[0] -= 1;
		ent->v.absmin[1] -= 1;
		ent->v.absmin[2] -= 1;
		ent->v.absmax[0] += 1;
		ent->v.absmax[1] += 1;
		ent->v.absmax[2] += 1;
	}
	
// link to PVS leafs
	if (ent->v.modelindex)
		SV_LinkToLeafs (ent);
	else
		ent->e->num_leafs = 0;

	if (ent->v.solid == SOLID_NOT)
		return;

// find the first node that the ent's box crosses
	node = sv_areanodes;
	while (1)
	{
		if (node->axis == -1)
			break;
		if (ent->v.absmin[node->axis] > node->dist)
			node = node->children[0];
		else if (ent->v.absmax[node->axis] < node->dist)
			node = node->children[1];
		else
			break;		// crosses the node
	}
	
// link it in	

	if (ent->v.solid == SOLID_TRIGGER)
		InsertLinkBefore (&ent->e->area, &node->trigger_edicts);
	else
		InsertLinkBefore (&ent->e->area, &node->solid_edicts);
	
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

	if (ent->v.solid == SOLID_TRIGGER || ent->v.solid == SOLID_NOT)
		// only clip against bmodels
		trace = SV_Trace (ent->v.origin, ent->v.mins, ent->v.maxs, ent->v.origin, MOVE_NOMONSTERS, ent);
	else
		trace = SV_Trace (ent->v.origin, ent->v.mins, ent->v.maxs, ent->v.origin, MOVE_NORMAL, ent);
	
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
		if (touch->v.solid == SOLID_TRIGGER)
			SV_Error ("Trigger in clipping list");

		if ((clip->type & MOVE_NOMONSTERS) && touch->v.solid != SOLID_BSP)
			continue;

		if (clip->passedict && clip->passedict->v.size[0] && !touch->v.size[0])
			continue;	// points never interact

		if (clip->type & MOVE_LAGGED)
		{
			//can't touch lagged ents - we do an explicit test for them later in SV_AntilagClipCheck.
			if (touch->e->entnum - 1 < w.maxlagents)
				if (w.lagents[touch->e->entnum - 1].present)
					continue;
		}

		if (clip->passedict)
		{
			if (PROG_TO_EDICT(touch->v.owner) == clip->passedict)
				continue;	// don't clip against own missiles
			if (PROG_TO_EDICT(clip->passedict->v.owner) == touch)
				continue;	// don't clip against owner
		}

		if ((int)touch->v.flags & FL_MONSTER)
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
	if (ent->e->entnum == 0 || ent->e->entnum > MAX_CLIENTS)
		return;

	svs.clients[ent->e->entnum - 1].antilag_position_next = 0;
}

void SV_AntilagClipSetUp ( areanode_t *node, moveclip_t *clip )
{
	edict_t *passedict = clip->passedict;

	clip->type &= ~MOVE_LAGGED;

	if (passedict->e->entnum && passedict->e->entnum <= MAX_CLIENTS)
	{
		clip->type |= MOVE_LAGGED;
		w.lagents = svs.clients[passedict->e->entnum-1].laggedents;
		w.maxlagents = svs.clients[passedict->e->entnum-1].laggedents_count;
		w.lagentsfrac = svs.clients[passedict->e->entnum-1].laggedents_frac;
	}
	else if (passedict->v.owner)
	{
		int owner = PROG_TO_EDICT(passedict->v.owner)->e->entnum;

		if (owner && owner <= MAX_CLIENTS)
		{
			clip->type |= MOVE_LAGGED;
			w.lagents = svs.clients[owner-1].laggedents;
			w.maxlagents = svs.clients[owner-1].laggedents_count;
			w.lagentsfrac = svs.clients[owner-1].laggedents_frac;
		}
	}
}

void SV_AntilagClipCheck ( areanode_t *node, moveclip_t *clip )
{
	trace_t trace;
	edict_t *touch;
	vec3_t lp;
	int i;

	for (i = 0; i < w.maxlagents; i++)
	{
		if (clip->trace.allsolid)
			return; // return!!!

		if (!w.lagents[i].present)
			continue;

		touch = EDICT_NUM(i + 1);
		if (touch->v.solid == SOLID_NOT)
			continue;
		if (touch == clip->passedict)
			continue;
		if (touch->v.solid == SOLID_TRIGGER)
			SV_Error ("Trigger (%s) in clipping list", PR_GetEntityString(touch->v.classname));

		if ((clip->type & MOVE_NOMONSTERS) && touch->v.solid != SOLID_BSP)
			continue;

		VectorInterpolate(touch->v.origin, w.lagentsfrac, w.lagents[i].laggedpos, lp);

		if (   clip->boxmins[0] > lp[0]+touch->v.maxs[0]
			|| clip->boxmins[1] > lp[1]+touch->v.maxs[1]
			|| clip->boxmins[2] > lp[2]+touch->v.maxs[2]
			|| clip->boxmaxs[0] < lp[0]+touch->v.mins[0]
			|| clip->boxmaxs[1] < lp[1]+touch->v.mins[1]
			|| clip->boxmaxs[2] < lp[2]+touch->v.mins[2] )
			continue;

		if (clip->passedict && clip->passedict->v.size[0] && !touch->v.size[0])
			continue;	// points never interact

		if (clip->passedict)
		{
			if (PROG_TO_EDICT(touch->v.owner) == clip->passedict)
				continue;	// don't clip against own missiles
			if (PROG_TO_EDICT(clip->passedict->v.owner) == touch)
				continue;	// don't clip against owner
		}

		if ((int)touch->v.flags & FL_MONSTER)
			trace = SV_ClipMoveToEntity (touch, &lp, clip->start, clip->mins2, clip->maxs2, clip->end);
		else
			trace = SV_ClipMoveToEntity (touch, &lp, clip->start, clip->mins, clip->maxs, clip->end);

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
