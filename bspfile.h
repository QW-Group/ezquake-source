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

#ifndef __BSPFILE_H__
#define __BSPFILE_H__

// upper design bounds

#define	MAX_MAP_HULLS		4

#define	MAX_MAP_MODELS		512
#define	MAX_MAP_BRUSHES		4096
#define	MAX_MAP_ENTITIES	1024
#define	MAX_MAP_ENTSTRING	65536

#define	MAX_MAP_PLANES		8192
#define	MAX_MAP_NODES		32767		// because negative shorts are contents
#define	MAX_MAP_CLIPNODES	32767		//
#define	MAX_MAP_LEAFS		32767		// 
#define	MAX_MAP_VERTS		65535
#define	MAX_MAP_FACES		65535
#define	MAX_MAP_MARKSURFACES 65535
#define	MAX_MAP_TEXINFO		4096
#define	MAX_MAP_EDGES		256000
#define	MAX_MAP_SURFEDGES	512000
#define	MAX_MAP_MIPTEX		0x200000
#define	MAX_MAP_LIGHTING	0x100000
#define	MAX_MAP_VISIBILITY	0x100000

// key / value pair sizes

#define	MAX_KEY		32
#define	MAX_VALUE	1024


//=============================================================================


#define Q1_BSPVERSION     29
#define HL_BSPVERSION     30
#define Q1_BSPVERSION29a  (('2') + ('P' << 8) + ('S' << 16) + ('B' << 24))
#define Q1_BSPVERSION2    (('B') + ('S' << 8) + ('P' << 16) + ('2' << 24))

typedef struct
{
	int		fileofs, filelen;
} lump_t;

#define	LUMP_ENTITIES	0
#define	LUMP_PLANES		1
#define	LUMP_TEXTURES	2
#define	LUMP_VERTEXES	3
#define	LUMP_VISIBILITY	4
#define	LUMP_NODES		5
#define	LUMP_TEXINFO	6
#define	LUMP_FACES		7
#define	LUMP_LIGHTING	8
#define	LUMP_CLIPNODES	9
#define	LUMP_LEAFS		10
#define	LUMP_MARKSURFACES 11
#define	LUMP_EDGES		12
#define	LUMP_SURFEDGES	13
#define	LUMP_MODELS		14

#define	HEADER_LUMPS	15

typedef struct
{
	float       mins[3], maxs[3];
	float       origin[3];
	int	        headnode[MAX_MAP_HULLS];
	int         visleafs;                    // not including the solid leaf 0
	int         firstface, numfaces;
} dmodel_t;


typedef struct
{
	int         version;
	lump_t      lumps[HEADER_LUMPS];
} dheader_t;

typedef struct
{
	int         nummiptex;
	int         dataofs[4];                  // [nummiptex]
} dmiptexlump_t;

#define	MIPLEVELS	4
typedef struct miptex_s
{
	char        name[16];
	unsigned    width, height;
	unsigned    offsets[MIPLEVELS];		// four mip maps stored
} miptex_t;


typedef struct
{
	float       point[3];
} dvertex_t;


// 0-2 are axial planes
#define	PLANE_X			0
#define	PLANE_Y			1
#define	PLANE_Z			2

// 3-5 are non-axial planes snapped to the nearest
#define	PLANE_ANYX		3
#define	PLANE_ANYY		4
#define	PLANE_ANYZ		5

typedef struct
{
	float       normal[3];
	float       dist;
	int         type;             // PLANE_X - PLANE_ANYZ ?remove? trivial to regenerate
} dplane_t;



#define	CONTENTS_EMPTY		-1
#define	CONTENTS_SOLID		-2
#define	CONTENTS_WATER		-3
#define	CONTENTS_SLIME		-4
#define	CONTENTS_LAVA		-5
#define	CONTENTS_SKY		-6

typedef struct
{
	int             planenum;
	short           children[2];	// negative numbers are -(leafs+1), not nodes
	short           mins[3];        // for sphere culling
	short           maxs[3];
	unsigned short  firstface;
	unsigned short  numfaces;       // counting both sides
} dnode_t;

typedef struct
{
	int             planenum;
	int             children[2];    // negative numbers are -(leafs+1), not nodes
	short           mins[3];        // for sphere culling
	short           maxs[3];
	unsigned int    firstface;
	unsigned int    numfaces;       // counting both sides
} dnode29a_t;

typedef struct
{
	int             planenum;
	int             children[2];    // negative numbers are -(leafs+1), not nodes
	float           mins[3];        // for sphere culling
	float           maxs[3];
	unsigned int    firstface;
	unsigned int    numfaces;       // counting both sides
} dnode_bsp2_t;

typedef struct
{
	int         planenum;
	short       children[2];        // negative numbers are contents
} dclipnode_t;

typedef struct
{
	int         planenum;
	int         children[2];        // negative numbers are contents
} dclipnode29a_t;

typedef struct
{
	int         planenum;
	int         children[2];        // negative numbers are contents
} mclipnode_t;

typedef struct texinfo_s
{
	float		vecs[2][4];         // [s/t][xyz offset]
	int			miptex;
	int			flags;
} texinfo_t;
#define	TEX_SPECIAL		1           // sky or slime, no lightmap or 256 subdivision

// note that edge 0 is never used, because negative edge nums are used for
// counterclockwise use of the edge in a face
typedef struct {
	unsigned short	v[2];           // vertex numbers
} dedge_t;

typedef struct {
	unsigned int	v[2];           // vertex numbers
} dedge29a_t;

#define	MAXLIGHTMAPS	4
typedef struct {
	short       planenum;
	short       side;

	int         firstedge;      // we must support > 64k edges
	short       numedges;	
	short       texinfo;

	// lighting info
	byte        styles[MAXLIGHTMAPS];
	int         lightofs;       // start of [numstyles*surfsize] samples
} dface_t;

typedef struct {
	int         planenum;
	int         side;

	int         firstedge;      // we must support > 64k edges
	int         numedges;
	int         texinfo;

	// lighting info
	byte        styles[MAXLIGHTMAPS];
	int         lightofs;       // start of [numstyles*surfsize] samples
} dface29a_t;

#define	AMBIENT_WATER   0
#define	AMBIENT_SKY     1
#define	AMBIENT_SLIME   2
#define	AMBIENT_LAVA    3

#define	NUM_AMBIENTS    4       // automatic ambient sounds

// leaf 0 is the generic CONTENTS_SOLID leaf, used for all solid areas
// all other leafs need visibility info
typedef struct {
	int            contents;
	int            visofs;          // -1 = no visibility info

	short          mins[3];         // for frustum culling
	short          maxs[3];

	unsigned short firstmarksurface;
	unsigned short nummarksurfaces;

	byte           ambient_level[NUM_AMBIENTS];
} dleaf_t;

typedef struct {
	int            contents;
	int            visofs;          // -1 = no visibility info

	short          mins[3];         // for frustum culling
	short          maxs[3];

	unsigned int   firstmarksurface;
	unsigned int   nummarksurfaces;

	byte           ambient_level[NUM_AMBIENTS];
} dleaf29a_t;

typedef struct {
	int            contents;
	int            visofs;          // -1 = no visibility info

	float          mins[3];         // for frustum culling
	float          maxs[3];

	unsigned int   firstmarksurface;
	unsigned int   nummarksurfaces;

	byte           ambient_level[NUM_AMBIENTS];
} dleaf_bsp2_t;

#endif /* !__BSPFILE_H__ */
