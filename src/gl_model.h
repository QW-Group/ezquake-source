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

#ifndef __MODEL__
#define __MODEL__

#include "modelgen.h"
#include "spritegn.h"
#include "bspfile.h"

#define VBO_FIELDOFFSET(type, field) (void*)((uintptr_t)&(((type*)0)->field))
#define VK_VBO_FIELDOFFSET(type, field) (uint32_t)((uintptr_t)&(((type*)0)->field))

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
	char                name[16];
	unsigned int        width, height;
	texture_ref         gl_texturenum;
	texture_ref         fb_texturenum;				//index of fullbright mask or 0
	struct msurface_s*  texturechain;
	struct msurface_s** texturechain_tail;          //Points to the node right after the last non-NULL node in the texturechain.

	int                 anim_total;                 //total tenths in sequence ( 0 = no)
	int                 anim_min, anim_max;         //time for this frame min <=time< max
	struct texture_s*   anim_next;                  //in the animation sequence
	struct texture_s*   alternate_anims;            //bmodels in frame 1 use these
	unsigned int        offsets[MIPLEVELS];         //four mip maps stored
	unsigned int        flatcolor3ub;               //just for r_fastturb's sake
	qbool               loaded;                     //help speed up vid_restart, actual only for brush models
	int                 isLumaTexture;              //fb is luma texture, rather than normal fb
	qbool               isAlphaTested;              //texture is flagged for alpha-testing
	qbool               isLitTurb;                  //turb texture has lightmap
	int                 turbType;

	int                 next_same_size;
	qbool               size_start;

	texture_ref         gl_texture_array;
	unsigned int        gl_texture_index;
	float               gl_texture_scaleS;
	float               gl_texture_scaleT;

	int                 index;
} texture_t;


#define	SURF_PLANEBACK		2
#define	SURF_DRAWSKY		4
#define SURF_DRAWSPRITE		8
#define SURF_DRAWTURB		16
#define SURF_DRAWTILED		32
#define SURF_DRAWBACKGROUND	64
#define SURF_UNDERWATER		128
#define SURF_DRAWALPHA		256
#define SURF_DRAWFLAT_FLOOR 512

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct medge_s {
	unsigned int		v[2];
	unsigned int		cachededgeoffset;
} medge_t;

typedef struct mtexinfo_s {
	float         vecs[2][4];
	texture_t     *texture;
	int           miptex;
	int           flags;
	qbool         skippable;
	int           surfaces;
} mtexinfo_t;

// Filled in CopyVertToBuffer in glm_brushmodel
typedef struct vbo_world_vert_s {
	// Positions 0/4/8
	vec3_t position;

	// Positions 12/16/20
	vec3_t material_coords;

	// Positions 24/28/32
	vec3_t lightmap_coords;

	// Positions 36/40
	float detail_coords[2];

	// Positions 44/48/52
	float flatcolor[3];

	// Position 56
	int surface_num;

	// Position 60 (VBO_WORLD_X)
	int flags;
} vbo_world_vert_t;

// Trying to pad out to keep ATI/AMD happy (doesn't seem to make a difference?)
// https://developer.amd.com/wordpress/media/2012/10/ATI_OpenGL_Programming_and_Optimization_Guide.pdf
typedef struct glc_vbo_world_vert_s {
	float position[4];
	float material_coords[2];
	float detail_coords[2];
	float lightmap_coords[4];

	float flatstyle;
	float padding[3];
} glc_vbo_world_vert_t;

// GLC only...
// For turb surfaces, we use lightmap tex coords to store 8*sin(<s or t>), and detail coords to store 8*cos(<s or t>)
#define VERTEXSIZE 9  //xyz s1t1 s2t2 s3t3 where xyz = vert coords; s1t1 = normal tex coords; 
					  //s2t2 = lightmap tex coords; s3t3 = detail tex coords

typedef struct vbo_model_vert_s {
	vec3_t position;
	int lightnormalindex;
	vec3_t normal;
	int padding2;
	vec3_t direction;
	int padding3;
	float texture_coords[2];
	unsigned int flags;
	int padding4;
} vbo_model_vert_t;

typedef struct glpoly_s {
	struct	glpoly_s	*next;
	struct	glpoly_s	*chain;						//next lightmap poly in chain
	struct	glpoly_s	*fb_chain;					//next fb poly in chain
	struct	glpoly_s	*luma_chain;				//next luma poly in chain
	struct	glpoly_s	*caustics_chain;			//next caustic poly in chain
	struct	glpoly_s	*detail_chain;				//next detail poly in chain
	struct  glpoly_s    *drawflat_chain;            //next drawflat poly in chain

	unsigned int        vbo_start;
	int					numverts;
	float				verts[4][VERTEXSIZE];		// variable sized (xyz s1t1 s2t2 l)
} glpoly_t;

typedef struct msurface_s {
	int                 visframe;                   // should be drawn when node is crossed

	mplane_t*           plane;
	int                 flags;

	int                 firstedge;                  // look up in model->surfedges[], negative numbers
	int                 numedges;                   // are backwards edges
	
	short               texturemins[2];
	short               extents[2];

	short               lmshift;

	// gl lightmap coordinates
	int                 light_s;
	int                 light_t;

	glpoly_t            *polys;                     // multiple if warped
	glpoly_t            *subdivided;
	struct	msurface_s  *texturechain;
	struct	msurface_s  *drawflatchain;

	mtexinfo_t          *texinfo;

	// decoupled lm
	float               lmvecs[2][4];
	float               lmvlen[2];

	// lighting info
	int                 dlightframe;
	int                 dlightbits;

	// This is not an OpenGL texture name, it's the lightmap index
	int                 lightmaptexturenum;
	int                 surfacenum;

	byte                styles[MAXLIGHTMAPS];
	int                 cached_light[MAXLIGHTMAPS]; // values currently used in lightmap
	qbool               cached_dlight;              // true if dynamic light in cache
	byte                *samples;                   // [numstyles*surfsize]
} msurface_t;

typedef struct mnode_s {
	// common with leaf
	int					contents;					// 0, to differentiate from leafs
	int					visframe;					// node needs to be traversed if current
	
	float				minmaxs[6];					// for bounding box culling

	struct mnode_s		*parent;

	// node specific
	mplane_t			*plane;
	struct mnode_s		*children[2];	

	unsigned int		firstsurface;
	unsigned int		numsurfaces;
} mnode_t;

typedef struct mleaf_s {
	// common with node
	int					contents;					// wil be a negative contents number
	int					visframe;					// node needs to be traversed if current

	float				minmaxs[6];					// for bounding box culling

	struct mnode_s		*parent;

	// leaf specific
	byte				*compressed_vis;
	struct efrag_s		*efrags;

	msurface_t			**firstmarksurface;
	int					nummarksurfaces;
} mleaf_t;


/*
==============================================================================

SPRITE MODELS

==============================================================================
*/


// FIXME: shorten these?
typedef struct mspriteframe_s {
	int                 width;
	int                 height;
	float               up, down, left, right;
	texture_ref         gl_texturenum;
	texture_ref         gl_arraynum;
	int                 gl_arrayindex;

	float               gl_scalingS;
	float               gl_scalingT;
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

#define MAX_SPRITE_FRAMES (512)

typedef struct mspriteframe2_s {
	mspriteframe_t		frame;
	float				interval;
} mspriteframe2_t;

typedef struct mspriteframedesc2_s {
	spriteframetype_t	type;

	int					numframes;			// always 1 for type == SPR_SINGLE
	int					offset;

} mspriteframedesc2_t;

typedef struct msprite2_s {
	int					type;
	int					maxwidth;
	int					maxheight;
	int					numframes;

	mspriteframedesc2_t	frames[MAX_SPRITE_FRAMES];
} msprite2_t; // actual frames follow after struct immidiately


/*
==============================================================================

ALIAS MODELS

Alias models are position independent, so the cache manager can move them.
==============================================================================
*/


typedef struct maliasframedesc_s {
	int					firstpose;
	int					numposes;
	float				interval;
	vec3_t				bboxmin;
	vec3_t				bboxmax;
	float				radius;
	int					frame;
	char				name[16];
	char                groupname[16];
	int                 groupnumber;
	int                 nextpose;
} maliasframedesc_t;

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

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct mtriangle_s {
	int					facesfront;
	int					vertindex[3];
} mtriangle_t;


#define MAX_SKINS 32
typedef struct aliashdr_s {
	int          ident;
	int          version;
	vec3_t       scale;
	vec3_t       scale_origin;
	float        boundingradius;
	vec3_t       eyeposition;
	int          numskins;
	int          skinwidth;
	int          skinheight;
	int          numverts;
	int          numtris;
	int          numframes;
	synctype_t   synctype;
	int          flags;
	float        size;

	int          numposes;
	int          poseverts;
	int          posedata;	// numposes*poseverts ez_trivertx_t
	int          commands;	// gl command list with embedded s/t
	texture_ref  gl_texturenum[MAX_SKINS][4];
	texture_ref  glc_fb_texturenum[MAX_SKINS][4];

	int          vertsPerPose;

	maliasframedesc_t	frames[1];	// variable sized
} aliashdr_t;

//===================================================================

//
// Whole model
//

#define	MAXALIASVERTS	2048
#define	MAXALIASFRAMES	256
#define	MAXALIASTRIS	2048
extern	aliashdr_t		*pheader;
extern	stvert_t		stverts[MAXALIASVERTS];
extern	mtriangle_t		triangles[MAXALIASTRIS];
extern	trivertx_t		*poseverts[MAXALIASFRAMES];

// some models are special
typedef enum 
{
	MOD_NORMAL, 
	MOD_PLAYER, 
	MOD_EYES, 
	MOD_FLAME,
	MOD_THUNDERBOLT,
	MOD_BACKPACK,
	MOD_FLAG,
	MOD_SPIKE, 
	MOD_TF_TRAIL,
	MOD_GRENADE, 
	MOD_TESLA, 
	MOD_SENTRYGUN, 
	MOD_DETPACK,
	MOD_LASER, 
	MOD_DEMON, 
	MOD_SOLDIER,
	MOD_OGRE,
	MOD_ENFORCER,
	MOD_VOORSPIKE,
	MOD_LAVABALL,
	MOD_RAIL, 
	MOD_RAIL2,
	MOD_BUILDINGGIBS,
	MOD_CLUSTER,
	MOD_SHAMBLER, 
	MOD_TELEPORTDESTINATION,
	MOD_GIB,
	MOD_VMODEL,
	MOD_ROCKET,
	MOD_ARMOR,
	MOD_MEGAHEALTH,
	MOD_ROCKETLAUNCHER,
	MOD_LIGHTNINGGUN,
	MOD_QUAD,
	MOD_PENT,
	MOD_RING,
	MOD_FLAME0,
	MOD_FLAME2,
	MOD_FLAME3,
	MOD_SUIT,
	MOD_GRENADELAUNCHER,

	MOD_NUMBER_HINTS
} modhint_t;

#define	EF_ROCKET	1			// leave a trail
#define	EF_GRENADE	2			// leave a trail
#define	EF_GIB		4			// leave a trail
#define	EF_ROTATE	8			// rotate (bonus items)
#define	EF_TRACER	16			// green split trail
#define	EF_ZOMGIB	32			// small blood trail
#define	EF_TRACER2	64			// orange split trail + rotate
#define	EF_TRACER3	128			// purple trail

#define MAX_SIMPLE_TEXTURES 5
#define MAX_TEXTURE_ARRAYS_PER_MODEL 64

typedef struct worldspawn_info_s {
	char skybox_name[MAX_QPATH];
	float fog_density;
	vec3_t fog_color;
	float fog_sky;

	float wateralpha;
	float lavaalpha;
	float telealpha;
	float slimealpha;
} worldspawn_info_t;

typedef struct model_s {
	char				name[MAX_QPATH];
	qbool				needload; // bmodels and sprites don't cache normally

	unsigned short		crc;

	texture_ref         simpletexture[MAX_SIMPLE_TEXTURES]; // for simpleitmes

	modhint_t			modhint;
	modhint_t			modhint_trail;

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

	byte*               visdata;
	int                 visdata_length;
	byte*               lightdata;

	int					bspversion;
	qbool				isworldmodel;

	// additional model data
	void*               cached_data;

	texture_ref         texture_arrays[MAX_TEXTURE_ARRAYS_PER_MODEL];
	float               texture_arrays_scale_s[MAX_TEXTURE_ARRAYS_PER_MODEL];
	float               texture_arrays_scale_t[MAX_TEXTURE_ARRAYS_PER_MODEL];
	unsigned int        texture_array_count;
	int                 texture_array_first[MAX_TEXTURE_ARRAYS_PER_MODEL];

	void*               temp_vbo_buffer;
	float               min_tex[2];
	float               max_tex[2];

	unsigned int        vbo_start;
	int                 vertsInVBO;

	texture_ref         simpletexture_array[MAX_SIMPLE_TEXTURES];
	int                 simpletexture_indexes[MAX_SIMPLE_TEXTURES];
	float               simpletexture_scalingS[MAX_SIMPLE_TEXTURES];
	float               simpletexture_scalingT[MAX_SIMPLE_TEXTURES];

	msurface_t*         drawflat_chain;
	qbool               drawflat_todo;
	qbool               alphapass_todo;
	int                 first_texture_chained;
	int                 last_texture_chained;
	qbool               texturechains_have_lumas;

	int                 renderfx;
} model_t;

//============================================================================

void	Mod_Init (void);
void	Mod_ClearAll (void);
model_t *Mod_ForName (const char *name, qbool crash);
void	*Mod_Extradata (model_t *mod); // handles caching
void	Mod_TouchModel (char *name);
void	Mod_TouchModels (void); // for vid_restart
void Mod_ReloadModels(qbool vid_restart);
void Mod_FreeAllCachedData(void);

mleaf_t *Mod_PointInLeaf(vec3_t p, model_t *model);
byte	*Mod_LeafPVS(mleaf_t *leaf, model_t *model);

qbool	Img_HasFullbrights (byte *pixels, int size);
void	Mod_ReloadModelsTextures (void); // for vid_restart

texture_ref Mod_LoadSimpleTexture(model_t *mod, int skinnum);
void    Mod_ClearSimpleTextures(void);

qbool Mod_IsAlphaTextureName(model_t* model, const char* name);
qbool Mod_IsSkyTextureName(model_t* model, const char* name);
qbool Mod_IsTurbTextureName(model_t* model, const char* name);

float RadiusFromBounds(vec3_t mins, vec3_t maxs);
void Mod_AddModelFlags(model_t *mod);
void R_LoadBrushModelTextures(model_t *m);
void R_BrushModelPolygonToTriangleStrip(glpoly_t* poly);

#include "r_aliasmodel_md3.h"

typedef enum {
	custom_model_explosion,
	custom_model_bolt,
	custom_model_bolt2,
	custom_model_bolt3,
	custom_model_beam,
	custom_model_flame0,

	custom_model_count
} custom_model_id_t;

extern model_t* cl_custommodels[custom_model_count];

model_t* Mod_CustomModel(custom_model_id_t id, qbool crash);

int Mod_ExpectedNextFrame(model_t* mod, int framenum, int framecount);

// aliasmodels
#define NUMVERTEXNORMALS    162
#define SHADEDOT_QUANT       64

#define MD3_INTERP_MAXDIST  300

// brushmodel
extern vec3_t modelorg;
// 
extern int      r_visframecount;
extern int      r_framecount;
extern mplane_t frustum[4];

// sky
#define BACKFACE_EPSILON	0.01


#define DEFAULT_LMSHIFT     4

#endif	// __MODEL__
