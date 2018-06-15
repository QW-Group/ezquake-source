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

*/
// cmodel.c - collision model.

#ifdef SERVERONLY
#include "qwsvdef.h"
#else
#include "common.h"
#include "cvar.h"
#endif

typedef struct cnode_s {
	// common with leaf
	int                contents; // 0, to differentiate from leafs
	struct cnode_s     *parent;

	// node specific
	mplane_t           *plane;
	struct cnode_s     *children[2];	
} cnode_t;

typedef struct cleaf_s {
	// common with node
	int                contents;         // a negative contents number
	struct cnode_s     *parent;

	// leaf specific
	byte               ambient_sound_level[NUM_AMBIENTS];
} cleaf_t;

static char			loadname[32];	// for hunk tags

static char			map_name[MAX_QPATH];
static unsigned int	map_checksum, map_checksum2;

static int			numcmodels;
static cmodel_t		map_cmodels[MAX_MAP_MODELS];

static mplane_t		*map_planes;
static int			numplanes;

static cnode_t		*map_nodes;
static int			numnodes;

static mclipnode_t	*map_clipnodes;
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


/*
===============================================================================

HULL BOXES

===============================================================================
*/

static hull_t		box_hull;
static mclipnode_t	box_clipnodes[6];
static mplane_t		box_planes[6];

/*
** CM_InitBoxHull
**
** Set up the planes and clipnodes so that the six floats of a bounding box
** can just be stored out and get a proper hull_t structure.
*/
static void CM_InitBoxHull(void)
{
	int side, i;

	box_hull.clipnodes = box_clipnodes;
	box_hull.planes = box_planes;
	box_hull.firstclipnode = 0;
	box_hull.lastclipnode = 5;

	for (i = 0; i < 6; i++) {
		box_clipnodes[i].planenum = i;
		side = i & 1;
		box_clipnodes[i].children[side] = CONTENTS_EMPTY;
		box_clipnodes[i].children[side ^ 1] = (i != 5) ? (i + 1) : CONTENTS_SOLID;
		box_planes[i].type = i >> 1;
		box_planes[i].normal[i >> 1] = 1;
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

int CM_HullPointContents(hull_t *hull, int num, vec3_t p)
{
	mclipnode_t *node;
	mplane_t *plane;
	float d;

	while (num >= 0) {
		if (num < hull->firstclipnode || num > hull->lastclipnode) {
			if (map_halflife && num == hull->lastclipnode + 1) {
				return CONTENTS_EMPTY;
			}
			Sys_Error("CM_HullPointContents: bad node number");
		}

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
#define	DIST_EPSILON	0.03125

enum { TR_EMPTY, TR_SOLID, TR_BLOCKED };

typedef struct {
	hull_t *hull;
	trace_t	trace;
	int leafcount;
} hulltrace_local_t;

//====================
int RecursiveHullTrace (hulltrace_local_t *htl, int num, float p1f, float p2f, const vec3_t p1, const vec3_t p2)
{
	mplane_t	*plane;
	float		t1, t2;
	mclipnode_t *node;
	int			i;
	int			nearside;
	float		frac, midf;
	vec3_t		mid;
	int			check, oldcheck;
	hull_t *hull = htl->hull;
	trace_t *trace = &htl->trace;

start:
	if (num < 0) {
		// this is a leaf node
		htl->leafcount++;
		if (num == CONTENTS_SOLID) {
			if (htl->leafcount == 1)
				trace->startsolid = true;
			return TR_SOLID;
		}
		else {
			if (num == CONTENTS_EMPTY)
				trace->inopen = true;
			else
				trace->inwater = true;
			return TR_EMPTY;
		}
	}

	// FIXME, check at load time
	if (num < hull->firstclipnode || num > hull->lastclipnode)
	{
		if (map_halflife && num == hull->lastclipnode + 1)
			return TR_EMPTY;
		Sys_Error ("RecursiveHullTrace: bad node number");
	}

	node = hull->clipnodes + num;

	//
	// find the point distances
	//
	plane = hull->planes + node->planenum;

	if (plane->type < 3) {
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
	}
	else {
		t1 = DotProduct (plane->normal, p1) - plane->dist;
		t2 = DotProduct (plane->normal, p2) - plane->dist;
	}

	// see which sides we need to consider
	if (t1 >= 0 && t2 >= 0) {
		num = node->children[0];	// go down the front side
		goto start;
	}
	if (t1 < 0 && t2 < 0) {
		num = node->children[1];	// go down the back side
		goto start;
	}

	// find the intersection point
	frac = t1 / (t1 - t2);
	frac = bound (0, frac, 1);
	midf = p1f + (p2f - p1f)*frac;
	for (i = 0; i < 3; i++)
		mid[i] = p1[i] + frac*(p2[i] - p1[i]);

	// move up to the node
	nearside = (t1 < t2) ? 1 : 0;
	check = RecursiveHullTrace (htl, node->children[nearside], p1f, midf, p1, mid);
	if (check == TR_BLOCKED)
		return check;

	// if we started in solid, allow us to move out to an empty area
	if (check == TR_SOLID && (trace->inopen || trace->inwater))
		return check;
	oldcheck = check;

	// go past the node
	check = RecursiveHullTrace (htl, node->children[1 - nearside], midf, p2f, mid, p2);
	if (check == TR_EMPTY || check == TR_BLOCKED)
		return check;

	if (oldcheck != TR_EMPTY)
		return check;	// still in solid

	// near side is empty, far side is solid
	// this is the impact point
	if (!nearside) {
		VectorCopy (plane->normal, trace->plane.normal);
		trace->plane.dist = plane->dist;
	}
	else {
		VectorNegate (plane->normal, trace->plane.normal);
		trace->plane.dist = -plane->dist;
	}

	// put the final point DIST_EPSILON pixels on the near side
	if (t1 < t2)
		frac = (t1 + DIST_EPSILON) / (t1 - t2);
	else
		frac = (t1 - DIST_EPSILON) / (t1 - t2);
	frac = bound (0, frac, 1);
	midf = p1f + (p2f - p1f)*frac;
	for (i = 0; i < 3; i++)
		mid[i] = p1[i] + frac*(p2[i] - p1[i]);

	trace->fraction = midf;
	VectorCopy (mid, trace->endpos);

	return TR_BLOCKED;
}

trace_t CM_HullTrace (hull_t *hull, vec3_t start, vec3_t end)
{
	int check;

	// this structure is passed as a pointer to RecursiveHullTrace
	// so as not to use much stack but still be thread safe
	hulltrace_local_t htl;
	htl.hull = hull;
	htl.leafcount = 0;
	// fill in a default trace
	memset (&htl.trace, 0, sizeof(htl.trace));
	htl.trace.fraction = 1;
	htl.trace.startsolid = false;
	VectorCopy (end, htl.trace.endpos);

	check = RecursiveHullTrace (&htl, hull->firstclipnode, 0, 1, start, end);

	if (check == TR_SOLID) {
		htl.trace.startsolid = htl.trace.allsolid = true;
		// it would be logical to set fraction to 0, but original id code
		// would leave it at 1.   We emulate that just in case.
		// (FIXME: is it just QW, or NQ as well?)
		//htl.trace.fraction = 0;
		VectorCopy (start, htl.trace.endpos);
	}

	return htl.trace;
}

//===========================================================================

int	CM_NumInlineModels (void)
{
	return numcmodels;
}

char *CM_EntityString (void)
{
	return map_entitystring;
}

int CM_Leafnum (const cleaf_t *leaf)
{
	assert (leaf);
	return leaf - map_leafs;
}

int	CM_LeafAmbientLevel (const cleaf_t *leaf, int ambient_channel)
{
	assert ((unsigned)ambient_channel <= NUM_AMBIENTS);
	assert (leaf);

	return leaf->ambient_sound_level[ambient_channel];
}

// always returns a valid cleaf_t pointer
cleaf_t *CM_PointInLeaf (const vec3_t p)
{
	float d;
	cnode_t *node;
	mplane_t *plane;

	if (!numnodes)
		Host_Error ("CM_PointInLeaf: numnodes == 0");

	node = map_nodes;
	while (1) {
		if (node->contents < 0) {
			return (cleaf_t *)node;
		}

		plane = node->plane;
		d = DotProduct(p, plane->normal) - plane->dist;
		node = (d > 0) ? node->children[0] : node->children[1];
	}

	return NULL; // never reached
}


byte *CM_LeafPVS (const cleaf_t *leaf)
{
	if (leaf == map_leafs)
		return map_novis;

	return map_pvs + (leaf - 1 - map_leafs) * map_vis_rowbytes;
}


/*
** only the server may call this
*/
byte *CM_LeafPHS (const cleaf_t *leaf)
{
	if (leaf == map_leafs)
		return map_novis;

	if (!map_phs) {
		return NULL;
	}

	return map_phs + (leaf - 1 - map_leafs) * map_vis_rowbytes;
}

/*
=============================================================================

The PVS must include a small area around the client to allow head bobbing
or other small motion on the client side.  Otherwise, a bob might cause an
entity that should be visible to not show up, especially when the bob
crosses a waterline.

=============================================================================
*/

static int	fatbytes;
static byte	fatpvs[MAX_MAP_LEAFS/8];
static vec3_t	fatpvs_org;

static void AddToFatPVS_r (cnode_t *node)
{
	int i;
	float d;
	byte *pvs;
	mplane_t *plane;

	while (1)
	{ // if this is a leaf, accumulate the pvs bits
		if (node->contents < 0)
		{
			if (node->contents != CONTENTS_SOLID)
			{
				pvs = CM_LeafPVS ( (cleaf_t *)node);
				for (i=0 ; i<fatbytes ; i++)
					fatpvs[i] |= pvs[i];
			}
			return;
		}

		plane = node->plane;
		d = DotProduct (fatpvs_org, plane->normal) - plane->dist;
		if (d > 8)
			node = node->children[0];
		else if (d < -8)
			node = node->children[1];
		else
		{ // go down both
			AddToFatPVS_r (node->children[0]);
			node = node->children[1];
		}
	}
}

/*
=============
CM_FatPVS

Calculates a PVS that is the inclusive or of all leafs within 8 pixels of the
given point.
=============
*/
byte *CM_FatPVS (vec3_t org)
{
	VectorCopy (org, fatpvs_org);

	fatbytes = (visleafs+31)>>3;
	memset (fatpvs, 0, fatbytes);
	AddToFatPVS_r (map_nodes);
	return fatpvs;
}


/*
** Recursively build a list of leafs touched by a rectangular volume
*/
static int leafs_count;
static int leafs_maxcount;
static qbool leafs_overflow;
static int *leafs_list;
static int leafs_topnode;
static vec3_t leafs_mins, leafs_maxs;

static void FindTouchedLeafs_r(const cnode_t *node)
{
	mplane_t *splitplane;
	cleaf_t *leaf;
	int sides;

	while (1) {
		if (node->contents == CONTENTS_SOLID) {
			return;
		}

		// the node is a leaf
		if (node->contents < 0) {
			if (leafs_count == leafs_maxcount) {
				leafs_overflow = true;
				return;
			}

			leaf = (cleaf_t *)node;
			leafs_list[leafs_count++] = leaf - map_leafs;
			return;
		}

		// NODE_MIXED
		splitplane = node->plane;
		sides = BOX_ON_PLANE_SIDE(leafs_mins, leafs_maxs, splitplane);

		// recurse down the contacted sides
		if (sides == 1) {
			node = node->children[0];
		}
		else if (sides == 2) {
			node = node->children[1];
		}
		else {
			if (leafs_topnode == -1) {
				leafs_topnode = node - map_nodes;
			}
			FindTouchedLeafs_r(node->children[0]);
			node = node->children[1];
		}
	}
}

/*
** Returns an array filled with leaf nums
*/
int CM_FindTouchedLeafs (const vec3_t mins, const vec3_t maxs, int leafs[], int maxleafs, int headnode, int *topnode)
{
	leafs_count = 0;
	leafs_maxcount = maxleafs;
	leafs_list = leafs;
	leafs_topnode = -1;
	leafs_overflow = false;
	VectorCopy (mins, leafs_mins);
	VectorCopy (maxs, leafs_maxs);

	FindTouchedLeafs_r (&map_nodes[headnode]);

	if (leafs_overflow) {
		leafs_count = -1;
	}

	if (topnode)
		*topnode = leafs_topnode;

	return leafs_count;
}


/*
===============================================================================

                          BRUSHMODEL LOADING

===============================================================================
*/

static void CM_LoadEntities (lump_t *l)
{
	if (!l->filelen) {
		map_entitystring = NULL;
		return;
	}
	map_entitystring = (char *) Hunk_AllocName (l->filelen, loadname);
	memcpy (map_entitystring, cmod_base + l->fileofs, l->filelen);
}


/*
=================
CM_LoadSubmodels
=================
*/
static void CM_LoadSubmodels (lump_t *l)
{
	dmodel_t *in;
	cmodel_t *out;
	int i, j, count;

	in = (dmodel_t *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Host_Error ("CM_LoadMap: funny lump size");

	count = l->filelen / sizeof(*in);

	if (count < 1)
		Host_Error ("Map with no models");

	if (count > MAX_MAP_MODELS)
		Host_Error ("Map has too many models (%d vs %d)", count, MAX_MAP_MODELS);

	out = map_cmodels;
	numcmodels = count;

	visleafs = LittleLong (in[0].visleafs);

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++) {
			// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
			out->origin[j] = LittleFloat (in->origin[j]);
		}
		for (j = 0; j < MAX_MAP_HULLS; j++) {
			out->hulls[j].planes = map_planes;
			out->hulls[j].clipnodes = map_clipnodes;
			out->hulls[j].firstclipnode = LittleLong (in->headnode[j]);
			out->hulls[j].lastclipnode = numclipnodes - 1;
		}

		VectorClear (out->hulls[0].clip_mins);
		VectorClear (out->hulls[0].clip_maxs);

		if (map_halflife)
		{
			VectorSet (out->hulls[1].clip_mins, -16, -16, -32);
			VectorSet (out->hulls[1].clip_maxs, 16, 16, 32);

			VectorSet (out->hulls[2].clip_mins, -32, -32, -32);
			VectorSet (out->hulls[2].clip_maxs, 32, 32, 32);
			// not really used
			VectorSet (out->hulls[3].clip_mins, -16, -16, -18);
			VectorSet (out->hulls[3].clip_maxs, 16, 16, 18);
		}
		else
		{
			VectorSet (out->hulls[1].clip_mins, -16, -16, -24);
			VectorSet (out->hulls[1].clip_maxs, 16, 16, 32);

			VectorSet (out->hulls[2].clip_mins, -32, -32, -24);
			VectorSet (out->hulls[2].clip_maxs, 32, 32, 64);
		}
	}
}

/*
=================
CM_SetParent
=================
*/
static void CM_SetParent (cnode_t *node, cnode_t *parent)
{
	node->parent = parent;
	if (node->contents < 0)
		return;
	CM_SetParent (node->children[0], node);
	CM_SetParent (node->children[1], node);
}

/*
=================
CM_LoadNodes
=================
*/
static void CM_LoadNodes (lump_t *l)
{
	int i, j, count, p;
	dnode_t *in;
	cnode_t *out;

	in = (dnode_t *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("CM_LoadMap: funny lump size");

	count = l->filelen / sizeof(*in);
	out = (cnode_t *) Hunk_AllocName ( count*sizeof(*out), loadname);

	map_nodes = out;
	numnodes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		p = LittleLong(in->planenum);
		out->plane = map_planes + p;

		for (j=0 ; j<2 ; j++)
		{
			p = LittleShort (in->children[j]);
			out->children[j] = (p >= 0) ? (map_nodes + p) : ((cnode_t *)(map_leafs + (-1 - p)));
		}
	}

	CM_SetParent (map_nodes, NULL); // sets nodes and leafs
}

static void CM_LoadNodes29a(lump_t *l)
{
	int i, j, count, p;
	dnode29a_t *in;
	cnode_t *out;

	in = (dnode29a_t *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("CM_LoadMap: funny lump size");

	count = l->filelen / sizeof(*in);
	out = (cnode_t *)Hunk_AllocName(count * sizeof(*out), loadname);

	map_nodes = out;
	numnodes = count;

	for (i = 0; i < count; i++, in++, out++) {
		p = LittleLong(in->planenum);
		out->plane = map_planes + p;

		for (j = 0; j < 2; j++) {
			p = LittleLong(in->children[j]);
			out->children[j] = (p >= 0) ? (map_nodes + p) : ((cnode_t *)(map_leafs + (-1 - p)));
		}
	}

	CM_SetParent(map_nodes, NULL); // sets nodes and leafs
}

static void CM_LoadNodesBSP2(lump_t *l)
{
	int i, j, count, p;
	dnode_bsp2_t *in;
	cnode_t *out;

	in = (dnode_bsp2_t *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("CM_LoadMap: funny lump size");

	count = l->filelen / sizeof(*in);
	out = (cnode_t *)Hunk_AllocName(count * sizeof(*out), loadname);

	map_nodes = out;
	numnodes = count;

	for (i = 0; i < count; i++, in++, out++) {
		p = LittleLong(in->planenum);
		out->plane = map_planes + p;

		for (j = 0; j < 2; j++) {
			p = LittleLong(in->children[j]);
			out->children[j] = (p >= 0) ? (map_nodes + p) : ((cnode_t *)(map_leafs + (-1 - p)));
		}
	}

	CM_SetParent(map_nodes, NULL); // sets nodes and leafs
}

/*
** CM_LoadLeafs
*/
static void CM_LoadLeafs (lump_t *l)
{
	dleaf_t *in;
	cleaf_t *out;
	int i, j, count, p;

	in = (dleaf_t *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Host_Error ("CM_LoadMap: funny lump size");
	count = l->filelen / sizeof(*in);
	out = (cleaf_t *) Hunk_AllocName ( count*sizeof(*out), loadname);

	map_leafs = out;
	numleafs = count;

	for (i = 0; i < count; i++, in++, out++) {
		p = LittleLong(in->contents);
		out->contents = p;
		for (j = 0; j < 4; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];
	}
}

static void CM_LoadLeafs29a (lump_t *l)
{
	dleaf29a_t *in;

	cleaf_t *out;
	int i, j, count, p;

	in = (dleaf29a_t *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Host_Error("CM_LoadMap: funny lump size");
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);

	map_leafs = out;
	numleafs = count;

	for (i = 0; i < count; i++, in++, out++) {
		p = LittleLong(in->contents);
		out->contents = p;
		for (j = 0; j < 4; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];
	}

}

static void CM_LoadLeafsBSP2 (lump_t *l)
{
	dleaf_bsp2_t *in;

	cleaf_t *out;
	int i, j, count, p;

	in = (dleaf_bsp2_t *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in)) {
		Host_Error("CM_LoadMap: funny lump size");
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);

	map_leafs = out;
	numleafs = count;

	for (i = 0; i < count; i++, in++, out++) {
		p = LittleLong(in->contents);
		out->contents = p;
		for (j = 0; j < 4; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];
	}
}

/*
=================
CM_LoadClipnodes
=================
*/
static void CM_LoadClipnodes(lump_t *l)
{
	dclipnode_t *in;
	mclipnode_t *out;
	int i, count;

	in = (dclipnode_t *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("CM_LoadMap: funny lump size");
	count = l->filelen / sizeof(*in);
	out = (mclipnode_t *)Hunk_AllocName(count * sizeof(*out), loadname);

	map_clipnodes = out;
	numclipnodes = count;

	for (i = 0; i < count; i++, out++, in++) {
		out->planenum = LittleLong(in->planenum);
		out->children[0] = LittleShort(in->children[0]);
		out->children[1] = LittleShort(in->children[1]);
	}
}

static void CM_LoadClipnodesBSP2(lump_t *l)
{
	dclipnode29a_t *in;
	mclipnode_t *out;
	int i, count;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("CM_LoadMap: funny lump size");
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName(count * sizeof(*out), loadname);

	map_clipnodes = out;
	numclipnodes = count;

	for (i = 0; i < count; i++, out++, in++) {
		out->planenum = LittleLong(in->planenum);
		out->children[0] = LittleLong(in->children[0]);
		out->children[1] = LittleLong(in->children[1]);
	}
}

/*
=================
CM_MakeHull0

Deplicate the drawing hull structure as a clipping hull
=================
*/
static void CM_MakeHull0(void)
{
	cnode_t *in, *child;
	mclipnode_t *out;
	int i, j, count;

	in = map_nodes;
	count = numnodes;
	out = (mclipnode_t *)Hunk_AllocName(count * sizeof(*out), loadname);

	// fix up hull 0 in all cmodels
	for (i = 0; i < numcmodels; i++) {
		map_cmodels[i].hulls[0].clipnodes = out;
		map_cmodels[i].hulls[0].lastclipnode = count - 1;
	}

	// build clipnodes from nodes
	for (i = 0; i < count; i++, out++, in++) {
		out->planenum = in->plane - map_planes;
		for (j = 0; j < 2; j++) {
			child = in->children[j];
			out->children[j] = (child->contents < 0) ? (child->contents) : (child - map_nodes);
		}
	}
}

/*
=================
CM_LoadPlanes
=================
*/
static void CM_LoadPlanes(lump_t *l)
{
	int i, j, count, bits;
	mplane_t *out;
	dplane_t *in;

	in = (dplane_t *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Host_Error("CM_LoadMap: funny lump size");

	count = l->filelen / sizeof(*in);
	out = (mplane_t *)Hunk_AllocName(count * sizeof(*out), loadname);

	map_planes = out;
	numplanes = count;

	for (i = 0; i < count; i++, in++, out++) {
		bits = 0;
		for (j = 0; j < 3; j++) {
			out->normal[j] = LittleFloat(in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1 << j;
		}

		out->dist = LittleFloat(in->dist);
		out->type = LittleLong(in->type);
		out->signbits = bits;
	}
}


/*
** DecompressVis
*/
static byte *DecompressVis(byte *in)
{
	static byte decompressed[MAX_MAP_LEAFS / 8];
	int c, row;
	byte *out;

	row = (visleafs + 7) >> 3;
	out = decompressed;

	if (!in) { // no vis info, so make all visible
		while (row) {
			*out++ = 0xff;
			row--;
		}
		return decompressed;
	}

	do {
		if (*in) {
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		while (c) {
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);

	return decompressed;
}


/*
** CM_BuildPVS
**
** Call after CM_LoadLeafs!
*/
static void CM_BuildPVS(lump_t *lump_vis, lump_t *lump_leafs)
{
	byte *visdata, *scan;
	dleaf_t *in;
	int i;

	map_vis_rowlongs = (visleafs + 31) >> 5;
	map_vis_rowbytes = map_vis_rowlongs * 4;
	map_pvs = (byte *)Hunk_Alloc(map_vis_rowbytes * visleafs);

	if (!lump_vis->filelen) {
		memset(map_pvs, 0xff, map_vis_rowbytes * visleafs);
		return;
	}

	// FIXME, add checks for lump_vis->filelen and leafs' visofs

	visdata = cmod_base + lump_vis->fileofs;

	// go through all leafs and decompress visibility data
	in = (dleaf_t *)(cmod_base + lump_leafs->fileofs);
	in++; // pvs row 0 is leaf 1
	scan = map_pvs;
	for (i = 0; i < visleafs; i++, in++, scan += map_vis_rowbytes) {
		int p = LittleLong(in->visofs);
		memcpy(scan, (p == -1) ? map_novis : DecompressVis(visdata + p), map_vis_rowbytes);
	}
}

static void CM_BuildPVS29a(lump_t *lump_vis, lump_t *lump_leafs)
{
	byte *visdata, *scan;
	dleaf29a_t *in;
	int i;

	map_vis_rowlongs = (visleafs + 31) >> 5;
	map_vis_rowbytes = map_vis_rowlongs * 4;
	map_pvs = (byte *)Hunk_Alloc(map_vis_rowbytes * visleafs);

	if (!lump_vis->filelen) {
		memset(map_pvs, 0xff, map_vis_rowbytes * visleafs);
		return;
	}

	// FIXME, add checks for lump_vis->filelen and leafs' visofs

	visdata = cmod_base + lump_vis->fileofs;

	// go through all leafs and decompress visibility data
	in = (dleaf29a_t *)(cmod_base + lump_leafs->fileofs);
	in++; // pvs row 0 is leaf 1
	scan = map_pvs;
	for (i = 0; i < visleafs; i++, in++, scan += map_vis_rowbytes) {
		int p = LittleLong(in->visofs);
		memcpy(scan, (p == -1) ? map_novis : DecompressVis(visdata + p), map_vis_rowbytes);
	}
}

static void CM_BuildPVSBSP2(lump_t *lump_vis, lump_t *lump_leafs)
{
	byte *visdata, *scan;
	dleaf_bsp2_t *in;
	int i;

	map_vis_rowlongs = (visleafs + 31) >> 5;
	map_vis_rowbytes = map_vis_rowlongs * 4;
	map_pvs = (byte *)Hunk_Alloc(map_vis_rowbytes * visleafs);

	if (!lump_vis->filelen) {
		memset(map_pvs, 0xff, map_vis_rowbytes * visleafs);
		return;
	}

	// FIXME, add checks for lump_vis->filelen and leafs' visofs

	visdata = cmod_base + lump_vis->fileofs;

	// go through all leafs and decompress visibility data
	in = (dleaf_bsp2_t *)(cmod_base + lump_leafs->fileofs);
	in++; // pvs row 0 is leaf 1
	scan = map_pvs;
	for (i = 0; i < visleafs; i++, in++, scan += map_vis_rowbytes) {
		int p = LittleLong(in->visofs);
		memcpy(scan, (p == -1) ? map_novis : DecompressVis(visdata + p), map_vis_rowbytes);
	}
}

/*
** CM_BuildPHS
**
** Expands the PVS and calculates the PHS (potentially hearable set)
** Call after CM_BuildPVS (so that map_vis_rowbytes & map_vis_rowlongs are set)
*/
static void CM_BuildPHS (void)
{
	int i, j, k, l, index1, bitbyte;
	unsigned *dest, *src;
	byte *scan;

	map_phs = NULL;
	if (map_vis_rowbytes * visleafs > 0x100000) {
		return;
	}

	map_phs = (byte *) Hunk_Alloc (map_vis_rowbytes * visleafs);
	scan = map_pvs;
	dest = (unsigned *)map_phs;
	for (i = 0; i < visleafs; i++, dest += map_vis_rowlongs, scan += map_vis_rowbytes)
	{
		// copy from pvs
		memcpy (dest, scan, map_vis_rowbytes);

		// or in hearable leafs
		for (j = 0; j < map_vis_rowbytes; j++)
		{
			bitbyte = scan[j];
			if (!bitbyte)
				continue;
			for (k = 0; k < 8; k++)
			{
				if (! (bitbyte & (1<<k)) )
					continue;
				// or this pvs row into the phs
				index1 = (j<<3) + k;
				if (index1 >= visleafs)
					continue;
				src = (unsigned *)map_pvs + index1 * map_vis_rowlongs;
				for (l = 0; l < map_vis_rowlongs; l++)
					dest[l] |= src[l];
			}
		}
	}
}



/*
** hunk was reset by host, so the data is no longer valid
*/
void CM_InvalidateMap (void)
{
	map_name[0] = 0;

	// null out the pointers to turn up any attempt to call CM functions
	map_planes = NULL;
	map_nodes = NULL;
	map_clipnodes = NULL;
	map_leafs = NULL;
	map_pvs = NULL;
	map_phs = NULL;
	map_entitystring = NULL;
}

/*
** CM_LoadMap
*/
typedef void(*BuildPVSFunction)(lump_t *lump_vis, lump_t *lump_leafs);
cmodel_t *CM_LoadMap (char *name, qbool clientload, unsigned *checksum, unsigned *checksum2)
{
#ifndef CLIENTONLY
	extern cvar_t sv_bspversion, sv_halflifebsp;
#endif

	unsigned int i;
	dheader_t *header;
	unsigned int *buf;
	unsigned int *padded_buf = NULL;
	BuildPVSFunction cm_load_pvs_func = CM_BuildPVS;
	qbool pad_lumps = false;
	int required_length = 0;
	int filelen = 0;

	if (map_name[0]) {
		assert(!strcmp(name, map_name));

		if (checksum)
			*checksum = map_checksum;
		*checksum2 = map_checksum2;
		return &map_cmodels[0]; // still have the right version
	}

	// load the file
	buf = (unsigned int *) FS_LoadTempFile (name, &filelen);
	if (!buf)
		Host_Error ("CM_LoadMap: %s not found", name);

	COM_FileBase (name, loadname);

	header = (dheader_t *)buf;

	i = LittleLong (header->version);
	if (i != Q1_BSPVERSION && i != HL_BSPVERSION && i != Q1_BSPVERSION2 && i != Q1_BSPVERSION29a)
		Host_Error ("CM_LoadMap: %s has wrong version number (%i should be %i)", name, i, Q1_BSPVERSION);

	map_halflife = (i == HL_BSPVERSION);

#ifndef CLIENTONLY
	Cvar_SetROM(&sv_halflifebsp, map_halflife ? "1" : "0");
	Cvar_SetROM(&sv_bspversion, i == Q1_BSPVERSION || i == HL_BSPVERSION ? "1" : "2");
#endif

	// swap all the lumps
	for (i = 0; i < sizeof(dheader_t) / 4; i++) {
		((int *)header)[i] = LittleLong(((int *)header)[i]);
	}

	// Align the lumps
	for (i = 0; i < HEADER_LUMPS; ++i) {
		pad_lumps |= (header->lumps[i].fileofs % 4) != 0;

		if (header->lumps[i].fileofs < 0 || header->lumps[i].filelen < 0) {
			Host_Error("CM_LoadMap: %s has invalid lump definitions", name);
		}
		if (header->lumps[i].fileofs + header->lumps[i].filelen > filelen || header->lumps[i].fileofs + header->lumps[i].filelen < 0) {
			Host_Error("CM_LoadMap: %s has invalid lump definitions", name);
		}

		required_length += header->lumps[i].filelen;
	}

	if (pad_lumps) {
		int position = 0;
		int required_size = sizeof(dheader_t) + required_length + HEADER_LUMPS * 4 + 1;
		padded_buf = Q_malloc(required_size);

		// Copy header
		memcpy(padded_buf, buf, sizeof(dheader_t));
		header = (dheader_t*)padded_buf;
		position += sizeof(dheader_t);

		// Copy lumps: align on 4-byte boundary
		for (i = 0; i < HEADER_LUMPS; ++i) {
			if (position % 4) {
				position += 4 - (position % 4);
			}
			if (position + header->lumps[i].filelen > required_size) {
				Host_Error("CM_LoadMap: %s caused error while aligning lumps", name);
			}
			memcpy((byte*)padded_buf + position, ((byte*)buf) + header->lumps[i].fileofs, header->lumps[i].filelen);
			header->lumps[i].fileofs = position;

			position += header->lumps[i].filelen;
		}

		// Use the new buffer
		buf = padded_buf;
	}

	cmod_base = (byte *)header;

	// checksum all of the map, except for entities
	map_checksum = map_checksum2 = 0;
	for (i = 0; i < HEADER_LUMPS; i++) {
		if (i == LUMP_ENTITIES)
			continue;
		map_checksum ^= LittleLong(Com_BlockChecksum(cmod_base + header->lumps[i].fileofs, header->lumps[i].filelen));

		if (i == LUMP_VISIBILITY || i == LUMP_LEAFS || i == LUMP_NODES)
			continue;
		map_checksum2 ^= LittleLong(Com_BlockChecksum(cmod_base + header->lumps[i].fileofs, header->lumps[i].filelen));
	}
	if (checksum)
		*checksum = map_checksum;
	*checksum2 = map_checksum2;

	// load into heap
	CM_LoadPlanes (&header->lumps[LUMP_PLANES]);
	if (LittleLong(header->version) == Q1_BSPVERSION29a) {
		CM_LoadLeafs29a(&header->lumps[LUMP_LEAFS]);
		CM_LoadNodes29a(&header->lumps[LUMP_NODES]);
		CM_LoadClipnodesBSP2(&header->lumps[LUMP_CLIPNODES]);
		cm_load_pvs_func = CM_BuildPVS29a;
	}
	else if (LittleLong(header->version) == Q1_BSPVERSION2) {
		CM_LoadLeafsBSP2(&header->lumps[LUMP_LEAFS]);
		CM_LoadNodesBSP2(&header->lumps[LUMP_NODES]);
		CM_LoadClipnodesBSP2(&header->lumps[LUMP_CLIPNODES]);
		cm_load_pvs_func = CM_BuildPVSBSP2;
	}
	else {
		CM_LoadLeafs(&header->lumps[LUMP_LEAFS]);
		CM_LoadNodes(&header->lumps[LUMP_NODES]);
		CM_LoadClipnodes(&header->lumps[LUMP_CLIPNODES]);
		cm_load_pvs_func = CM_BuildPVS;
	}
	CM_LoadEntities (&header->lumps[LUMP_ENTITIES]);
	CM_LoadSubmodels (&header->lumps[LUMP_MODELS]);

	CM_MakeHull0 ();

	cm_load_pvs_func (&header->lumps[LUMP_VISIBILITY], &header->lumps[LUMP_LEAFS]);

	if (!clientload) // client doesn't need PHS
		CM_BuildPHS ();

	strlcpy (map_name, name, sizeof(map_name));

	Q_free(padded_buf);

	return &map_cmodels[0];
}

cmodel_t *CM_InlineModel (char *name)
{
	int num;

	if (!name || name[0] != '*')
		Host_Error ("CM_InlineModel: bad name");

	num = atoi (name+1);
	if (num < 1 || num >= numcmodels)
		Host_Error ("CM_InlineModel: bad number");

	return &map_cmodels[num];
}

void CM_Init (void)
{
	memset (map_novis, 0xff, sizeof(map_novis));
	CM_InitBoxHull ();
}
