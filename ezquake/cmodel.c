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

	$Id: cmodel.c,v 1.4 2007-03-07 01:00:32 disconn3ct Exp $
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

	for (i=0 ; i<6 ; i++)
	{
		box_clipnodes[i].planenum = i;
		
		side = i&1;
		
		box_clipnodes[i].children[side] = CONTENTS_EMPTY;
		if (i != 5)
			box_clipnodes[i].children[side^1] = i + 1;
		else
			box_clipnodes[i].children[side^1] = CONTENTS_SOLID;
		
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

void CM_Init (void)
{
//	memset (map_novis, 0xff, sizeof(map_novis));
	CM_InitBoxHull ();
}
