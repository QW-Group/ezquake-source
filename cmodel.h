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

// cmodel.h
#ifndef __CMODEL_H__
#define __CMODEL_H__

#include "bspfile.h"

// mplane->type
enum {
	SIDE_FRONT = 0,
	SIDE_BACK = 1,
	SIDE_ON = 2
};


// plane_t structure
// !!! if this is changed, it must be changed in asm_i386.h too !!!
typedef struct mplane_s {
	vec3_t	normal;
	float	dist;
	byte	type;			// for texture axis selection and fast side tests
	byte	signbits;		// signx + signy<<1 + signz<<1
	byte	pad[2];
} mplane_t;

// !!! if this is changed, it must be changed in asm_i386.h too !!!
typedef struct {
	mclipnode_t	*clipnodes;
	mplane_t	*planes;
	int			firstclipnode;
	int			lastclipnode;
	vec3_t		clip_mins;
	vec3_t		clip_maxs;
} hull_t;

typedef struct {
	vec3_t	normal;
	float	dist;
} plane_t;

typedef struct {
	qbool	allsolid;			// if true, plane is not valid
	qbool	startsolid;			// if true, the initial point was in a solid area
	qbool	inopen, inwater;
	float	fraction;			// time completed, 1.0 = didn't hit anything
	vec3_t	endpos;				// final position
	plane_t	plane;				// surface normal at impact
	union {						// entity the surface is on
		int		entnum;			// for pmove
		struct edict_s	*ent;	// for sv_world
	} e;
} trace_t;

typedef struct {
	vec3_t	mins, maxs;
	vec3_t	origin;
	hull_t	hulls[MAX_MAP_HULLS];
} cmodel_t;

hull_t *CM_HullForBox (vec3_t mins, vec3_t maxs);
int CM_HullPointContents (hull_t *hull, int num, vec3_t p);
trace_t CM_HullTrace (hull_t *hull, vec3_t start, vec3_t end);
struct cleaf_s *CM_PointInLeaf (const vec3_t p);
int CM_Leafnum (const struct cleaf_s *leaf);
int CM_LeafAmbientLevel (const struct cleaf_s *leaf, int ambient_channel);
byte *CM_LeafPVS (const struct cleaf_s *leaf);
byte *CM_LeafPHS (const struct cleaf_s *leaf); // only for the server
byte *CM_FatPVS (vec3_t org);
int CM_FindTouchedLeafs (const vec3_t mins, const vec3_t maxs, int leafs[], int maxleafs, int headnode, int *topnode);
char *CM_EntityString (void);
int CM_NumInlineModels (void);
cmodel_t *CM_InlineModel (char *name);
void CM_InvalidateMap (void);
cmodel_t *CM_LoadMap (char *name, qbool clientload, unsigned *checksum, unsigned *checksum2);
void CM_Init (void);

#endif /* !__CMODEL_H__ */
