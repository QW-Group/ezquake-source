/*
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

	$Id: cmodel.c,v 1.5 2007-03-08 21:11:38 disconn3ct Exp $
*/
// cmodel.c

#include "common.h"


typedef struct cnode_s {
	// common with leaf
	int				contents; // 0, to differentiate from leafs
	struct cnode_s	*parent;

	// node specific
	mplane_t	*plane;
	struct cnode_s	*children[2];	
} cnode_t;



typedef struct cleaf_s {
	// common with node
	int				contents; // a negative contents number
	struct cnode_s	*parent;

// leaf specific
	byte			ambient_sound_level[NUM_AMBIENTS];
} cleaf_t;


/*
static char			loadname[32];	// for hunk tags

static char			map_name[MAX_QPATH];
static unsigned int	map_checksum, map_checksum2;

static int			numcmodels;
static cmodel_t		map_cmodels[MAX_MAP_MODELS];

static mplane_t		*map_planes;
static int			numplanes;

static cnode_t		*map_nodes;
static int			numnodes;

static dclipnode_t	*map_clipnodes;
static int			numclipnodes;

static cleaf_t		*map_leafs;
static int			numleafs;
static int			visleafs;

static byte			map_novis[MAX_MAP_LEAFS/8];

static byte			*map_pvs;					// fully expanded and decompressed
static byte			*map_phs;					// only valid if we are the server
static int			map_vis_rowbytes;			// for both pvs and phs
static int			map_vis_rowlongs;			// map_vis_rowbytes / 4

static char			*map_entitystring;

static qbool		map_halflife;

static byte			*cmod_base;					// for CM_Load* functions
*/

/*
===============================================================================

HULL BOXES

===============================================================================
*/

static hull_t		box_hull;
static dclipnode_t	box_clipnodes[6];
static mplane_t		box_planes[6];

/*
** CM_InitBoxHull
**
** Set up the planes and clipnodes so that the six floats of a bounding box
** can just be stored out and get a proper hull_t structure.
*/
static void CM_InitBoxHull (void)
{
	int		i;
	int		side;

	box_hull.clipnodes = box_clipnodes;
	box_hull.planes = box_planes;
	box_hull.firstclipnode = 0;
	box_hull.lastclipnode = 5;

	for (i=0 ; i<6 ; i++) {
		box_clipnodes[i].planenum = i;
		side = i&1;
		box_clipnodes[i].children[side] = CONTENTS_EMPTY;
		box_clipnodes[i].children[side^1] = (i != 5) ? (i + 1) : CONTENTS_SOLID;
		box_planes[i].type = i>>1;
		box_planes[i].normal[i>>1] = 1;
	}
}

/*
** CM_HullForBox
**
** To keep everything totally uniform, bounding boxes are turned into small
** BSP trees instead of being compared directly.
*/
hull_t *CM_HullForBox (vec3_t mins, vec3_t maxs)
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = mins[0];
	box_planes[2].dist = maxs[1];
	box_planes[3].dist = mins[1];
	box_planes[4].dist = maxs[2];
	box_planes[5].dist = mins[2];

	return &box_hull;
}

int CM_HullPointContents (hull_t *hull, int num, vec3_t p)
{
	dclipnode_t *node;
	mplane_t *plane;
	float d;

	while (num >= 0) {
		if (num < hull->firstclipnode || num > hull->lastclipnode)
			Sys_Error ("CM_HullPointContents: bad node number");

		node = hull->clipnodes + num;
		plane = hull->planes + node->planenum;
		
		d = PlaneDiff (p, plane);
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
#define DIST_EPSILON (0.03125)

static hull_t trace_hull;
static trace_t trace_trace;

static qbool RecursiveHullTrace (int num, float p1f, float p2f, vec3_t p1, vec3_t p2)
{
	float t1, t2, frac, midf;
	dclipnode_t *node;
	mplane_t *plane;
	int side, i;
	vec3_t mid;

	// check for empty
	if (num < 0) {
		if (num != CONTENTS_SOLID) {
			trace_trace.allsolid = false;
			if (num == CONTENTS_EMPTY)
				trace_trace.inopen = true;
			else
				trace_trace.inwater = true;
		} else {
			trace_trace.startsolid = true;
		}

		return true; // empty
	}

	// FIXME, check at load time
	if (num < trace_hull.firstclipnode || num > trace_hull.lastclipnode)
		Sys_Error ("RecursiveHullTrace: bad node number");

	// find the point distances
	node = trace_hull.clipnodes + num;
	plane = trace_hull.planes + node->planenum;

	if (plane->type < 3) {
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
	} else {
		t1 = DotProduct (plane->normal, p1) - plane->dist;
		t2 = DotProduct (plane->normal, p2) - plane->dist;
	}
	
	if (t1 >= 0 && t2 >= 0)
		return RecursiveHullTrace (node->children[0], p1f, p2f, p1, p2);
	if (t1 < 0 && t2 < 0)
		return RecursiveHullTrace (node->children[1], p1f, p2f, p1, p2);

	// put the crosspoint DIST_EPSILON pixels on the near side
	frac = (t1<0) ? (t1 + DIST_EPSILON)/(t1-t2) : (t1 - DIST_EPSILON)/(t1-t2);
	frac = bound (0, frac, 1);
		
	midf = p1f + (p2f - p1f) * frac;
	for (i = 0; i < 3; i++)
		mid[i] = p1[i] + frac * (p2[i] - p1[i]);

	side = (t1 < 0);

	// move up to the node
	if (!RecursiveHullTrace (node->children[side], p1f, midf, p1, mid))
		return false;

	if (CM_HullPointContents (&trace_hull, node->children[side^1], mid) != CONTENTS_SOLID)
		return RecursiveHullTrace (node->children[side^1], midf, p2f, mid, p2); // go past the node
	
	if (trace_trace.allsolid)
		return false; // never got out of the solid area
		
	// the other side of the node is solid, this is the impact point
	if (!side) {
		VectorCopy (plane->normal, trace_trace.plane.normal);
		trace_trace.plane.dist = plane->dist;
	} else {
		VectorNegate (plane->normal, trace_trace.plane.normal);
		trace_trace.plane.dist = -plane->dist;
	}

	while (CM_HullPointContents (&trace_hull, trace_hull.firstclipnode, mid) == CONTENTS_SOLID) {
		// shouldn't really happen, but does occasionally
		frac -= 0.1;
		if (frac < 0) {
			trace_trace.fraction = midf;
			VectorCopy (mid, trace_trace.endpos);
//			Com_DPrintf ("backup past 0\n");
			return false;
		}
		midf = p1f + (p2f - p1f) * frac;
		for (i=0 ; i<3 ; i++)
			mid[i] = p1[i] + frac * (p2[i] - p1[i]);
	}

	trace_trace.fraction = midf;
	VectorCopy (mid, trace_trace.endpos);

	return false;
}

// trace a line through the supplied clipping hull
// does not fill trace.e.ent
trace_t CM_HullTrace (hull_t *hull, vec3_t start, vec3_t end)
{
	// fill in a default trace
	memset (&trace_trace, 0, sizeof(trace_trace));
	trace_trace.fraction = 1;
	trace_trace.allsolid = true;
//	trace_trace.startsolid = true; // this was (commented out) in pmovetst.c, why? -- Tonik
	VectorCopy (end, trace_trace.endpos);

	trace_hull = *hull;

	RecursiveHullTrace (trace_hull.firstclipnode, 0, 1, start, end);

	return trace_trace;
}

//===========================================================================

void CM_Init (void)
{
//	memset (map_novis, 0xff, sizeof(map_novis));
	CM_InitBoxHull ();
}
