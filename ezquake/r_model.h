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

	$Id: r_model.h,v 1.9 2007-03-11 02:01:01 disconn3ct Exp $
*/

#ifndef __MODEL__
#define __MODEL__

#include "modelgen.h"
#include "spritegn.h"
#include "bspfile.h"

/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/


/*
==============================================================================

BRUSH MODELS

==============================================================================
*/


//
// in memory representation
//
// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct mvertex_s {
	vec3_t		position;
} mvertex_t;

typedef struct texture_s {
	char				name[16];
	unsigned			width, height;
	int					anim_total;					// total tenths in sequence ( 0 = no)
	int					anim_min, anim_max;			// time for this frame min <=time< max
	struct texture_s	*anim_next;					// in the animation sequence
	struct texture_s	*alternate_anims;			// bmodels in frmae 1 use these
	unsigned			offsets[MIPLEVELS];			// four mip maps stored
	int					flatcolor3ub;				// hetman: r_drawflat for software builds
} texture_t;


#define	SURF_PLANEBACK		2
#define	SURF_DRAWSKY		4
#define SURF_DRAWSPRITE		8
#define SURF_DRAWTURB		16
#define SURF_DRAWTILED		32
#define SURF_DRAWBACKGROUND	64
#define SURF_FORBRUSH       128 // / hetman: r_drawflat for software builds

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct medge_s {
	unsigned short		v[2];
	unsigned int		cachededgeoffset;
} medge_t;

typedef struct mtexinfo_s {
	float				vecs[2][4];
	float				mipadjust;
	texture_t			*texture;
	int					flags;
} mtexinfo_t;

typedef struct msurface_s {
	int					visframe;					// should be drawn when node is crossed

	int					dlightframe;
	int					dlightbits;

	mplane_t			*plane;
	int					flags;

	int					firstedge;					// look up in model->surfedges[], negative numbers
	int					numedges;					// are backwards edges
	
	// surface generation data
	struct surfcache_s	*cachespots[MIPLEVELS];

	short				texturemins[2];
	short				extents[2];

	mtexinfo_t			*texinfo;
	
	// lighting info
	byte				styles[MAXLIGHTMAPS];
	byte				*samples;					// [numstyles*surfsize]
} msurface_t;

typedef struct mnode_s {
	// common with leaf
	int					contents;					// 0, to differentiate from leafs
	int					visframe;					// node needs to be traversed if current
	
	short				minmaxs[6];					// for bounding box culling

	struct mnode_s		*parent;

	// node specific
	mplane_t			*plane;
	struct mnode_s		*children[2];	

	unsigned short		firstsurface;
	unsigned short		numsurfaces;
} mnode_t;



typedef struct mleaf_s {
	// common with node
	int					contents;					// wil be a negative contents number
	int					visframe;					// node needs to be traversed if current

	short				minmaxs[6];					// for bounding box culling

	struct mnode_s		*parent;

	// leaf specific
	byte				*compressed_vis;
	struct efrag_s		*efrags;

	msurface_t			**firstmarksurface;
	int					nummarksurfaces;
	int					key;						// BSP sequence number for leaf's contents
} mleaf_t;


/*
==============================================================================

SPRITE MODELS

==============================================================================
*/


// FIXME: shorten these?
typedef struct mspriteframe_s {
	int					width;
	int					height;
	float				up, down, left, right;
	byte				pixels[4];
} mspriteframe_t;

typedef struct mspritegroup_s {
	int					numframes;
	float				*intervals;
	mspriteframe_t		*frames[1];
} mspritegroup_t;

typedef struct mspriteframedesc_s {
	spriteframetype_t	type;
	mspriteframe_t		*frameptr;
} mspriteframedesc_t;

typedef struct msprite_s {
	int					type;
	int					maxwidth;
	int					maxheight;
	int					numframes;
	float				beamlength; // remove?
	void				*cachespot; // remove?
	mspriteframedesc_t	frames[1];
} msprite_t;


/*
==============================================================================

ALIAS MODELS

Alias models are position independent, so the cache manager can move them.
==============================================================================
*/

typedef struct maliasframedesc_s {
	aliasframetype_t	type;
	trivertx_t			bboxmin;
	trivertx_t			bboxmax;
	int					frame;
	char				name[16];
} maliasframedesc_t;

typedef struct {
	aliasskintype_t		type;
	int					skin;
} maliasskindesc_t;

typedef struct maliasgroupframedesc_s {
	trivertx_t			bboxmin;
	trivertx_t			bboxmax;
	int					frame;
} maliasgroupframedesc_t;

typedef struct maliasgroup_s {
	int					numframes;
	int					intervals;
	maliasgroupframedesc_t frames[1];
} maliasgroup_t;

typedef struct maliasskingroup_s {
	int					numskins;
	int					intervals;
	maliasskindesc_t	skindescs[1];
} maliasskingroup_t;

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct mtriangle_s {
	int					facesfront;
	int					vertindex[3];
} mtriangle_t;

typedef struct aliashdr_s {
	int					model;
	int					stverts;
	int					skindesc;
	int					triangles;
	maliasframedesc_t	frames[1];
} aliashdr_t;

//===================================================================

//
// Whole model
//

typedef enum {mod_brush, mod_sprite, mod_alias} modtype_t;

// some models are special
typedef enum {MOD_NORMAL, MOD_PLAYER, MOD_EYES, MOD_FLAME, MOD_THUNDERBOLT, MOD_BACKPACK} modhint_t;

#define	EF_ROCKET	1			// leave a trail
#define	EF_GRENADE	2			// leave a trail
#define	EF_GIB		4			// leave a trail
#define	EF_ROTATE	8			// rotate (bonus items)
#define	EF_TRACER	16			// green split trail
#define	EF_ZOMGIB	32			// small blood trail
#define	EF_TRACER2	64			// orange split trail + rotate
#define	EF_TRACER3	128			// purple trail

typedef struct model_s {
	char				name[MAX_QPATH];
	qbool				needload; // bmodels and sprites don't cache normally

	unsigned short		crc;

	modhint_t			modhint;

	modtype_t			type;
	int					numframes;
	synctype_t			synctype;
	
	int					flags;

	// volume occupied by the model graphics
	vec3_t				mins, maxs;
	float				radius;

	// brush model
	int					firstmodelsurface, nummodelsurfaces;

	// FIXME, don't really need these two
	int					numsubmodels;
	dmodel_t			*submodels;

	int					numplanes;
	mplane_t			*planes;

	int					numleafs; // number of visible leafs, not counting 0
	mleaf_t				*leafs;

	int					numvertexes;
	mvertex_t			*vertexes;

	int					numedges;
	medge_t				*edges;

	int					numnodes;
	int					firstnode;
	mnode_t				*nodes;

	int					numtexinfo;
	mtexinfo_t			*texinfo;

	int					numsurfaces;
	msurface_t			*surfaces;

	int					numsurfedges;
	int					*surfedges;

	int					nummarksurfaces;
	msurface_t			**marksurfaces;

	int					numtextures;
	texture_t			**textures;

	byte				*visdata;
	byte				*lightdata;

	int					bspversion;
	qbool				isworldmodel;

	// additional model data
	cache_user_t		cache; // only access through Mod_Extradata

} model_t;

//============================================================================

void	Mod_Init (void);
void	Mod_ClearAll (void);
model_t *Mod_ForName (char *name, qbool crash);
void	*Mod_Extradata (model_t *mod); // handles caching
void	Mod_TouchModel (char *name);

mleaf_t *Mod_PointInLeaf (float *p, model_t *model);
byte	*Mod_LeafPVS (mleaf_t *leaf, model_t *model);

#endif	// __MODEL__
