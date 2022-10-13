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

// refresh.h -- public interface to refresh functions

#define	TOP_RANGE		16			// soldier uniform colors
#define	BOTTOM_RANGE	96

//=============================================================================

typedef struct efrag_s {
	struct mleaf_s		*leaf;
	struct efrag_s		*leafnext;
	struct entity_s		*entity;
	struct efrag_s		*entnext;
} efrag_t;

#define RF_WEAPONMODEL      1
#define RF_NOSHADOW         2
#define RF_LIMITLERP        4
#define RF_PLAYERMODEL      8
#define RF_NORMALENT       16
#define RF_CAUSTICS        32
#define RF_ALPHABLEND      64   // bit of a hack - always enable blending (used for 2nd pass when multi-texturing disabled)
#define RF_ADDITIVEBLEND  128
#define RF_ROCKETPACK     256
#define RF_LGPACK         512

#define RF_BACKPACK_FLAGS (RF_ROCKETPACK | RF_LGPACK)

typedef struct custom_model_color_s {
	cvar_t color_cvar;
	cvar_t fullbright_cvar;
	cvar_t* amf_cvar;
	int model_hint;
	int renderfx;
	qbool disable_texturing;
} custom_model_color_t;

typedef struct entity_s {
	vec3_t					origin;
	vec3_t					angles;	
	struct model_s			*model;			// NULL = no model
	byte					*colormap;
	int						skinnum;		// for Alias models
	struct player_info_s	*scoreboard;	// identify player
	int						renderfx;		// RF_ bit mask
	int						effects;		// EF_ flags like EF_BLUE and etc, atm this used for powerup shells

	int						oldframe;
	int						frame;
	float					framelerp;

	struct efrag_s			*efrag;			// linked list of efrags (FIXME)
	int						visframe;		// last frame this entity was found in an active leaf. only used for static objects

	//int						dlightframe;	// dynamic lighting
	//int						dlightbits;

	//VULT MOTION TRAILS
	float alpha;

	// FIXME: could turn these into a union
	struct mnode_s			*topnode;		// for bmodels, first world node that splits bmodel, or NULL if not split

	qbool     full_light;

	float     r_modelcolor[3];
	float     r_modelalpha;
	float     shadelight;
	float     ambientlight;
	custom_model_color_t* custom_model;
	vec3_t    lightcolor; // LordHavoc: used by model rendering
	vec3_t    lightspot;  // used for shadows

	// vertex lighting only
	int       bestlight;
	int       entity_id;
	int       corona_id;
	double    particle_time;

	// outlining
	float     outlineScale;
} entity_t;

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct {
	vrect_t		vrect;				// subwindow in video for refresh
									// FIXME: not need vrect next field here?
	vrect_t		aliasvrect;			// scaled Alias version
	int			vrectright, vrectbottom;	// right & bottom screen coords
	int			aliasvrectright, aliasvrectbottom;	// scaled Alias versions
	float		vrectrightedge;			// rightmost right edge we care about,
										//  for use in edge list
	float		fvrectx, fvrecty;		// for floating-point compares
	float		fvrectx_adj, fvrecty_adj; // left and top edges, for clamping
	int			vrect_x_adj_shift20;	// (vrect.x + 0.5 - epsilon) << 20
	int			vrectright_adj_shift20;	// (vrectright + 0.5 - epsilon) << 20
	float		fvrectright_adj, fvrectbottom_adj;
										// right and bottom edges, for clamping
	float		fvrectright;			// rightmost edge, for Alias clamping
	float		fvrectbottom;			// bottommost edge, for Alias clamping
	float		horizontalFieldOfView;	// at Z = 1.0, this many X is visible 
										// 2.0 = 90 degrees
	float		xOrigin;			// should probably always be 0.5
	float		yOrigin;			// between be around 0.3 to 0.5

	vec3_t		vieworg;
	vec3_t		viewangles;
	int         viewheight_test;

	float		fov_x, fov_y;
	
	int			ambientlight;
} refdef_t;


// eye position, enitity list, etc - filled in before calling R_RenderView (TODO: port from ZQuake)
typedef struct {
	double			time;
	qbool			allow_cheats;
	qbool			allow_lumas;
	float			max_watervis;	// how much water transparency we allow: 0..1 (reverse of alpha)
	float			max_fbskins;	// max player skin brightness 0..1
//	int				viewplayernum;  // don't draw own glow when gl_flashblend 1

//	lightstyle_t    *lightstyles;
	qbool           fog_enabled;    // fog is enabled

	float           cos_time;
	float           sin_time;

	float           wateralpha;
	qbool           drawFlatFloors;
	qbool           drawFlatWalls;
	qbool           solidTexTurb;
	qbool           drawCaustics;
	qbool           drawWorldOutlines;
	float           distanceScale;

	vec3_t          outline_vpn;
	float           outlineBase;

	float           powerup_scroll_params[4];
} refdef2_t;


// refresh

extern refdef_t	r_refdef;
extern refdef2_t r_refdef2;
extern vec3_t r_origin, vpn, vright, vup;
extern vec3_t vright_noroll, vup_noroll;

extern struct texture_s	*r_notexture_mip;

extern entity_t r_worldentity;

void R_Init(void);
void R_InitTextures(void);
void R_PostProcessScene(void);
void R_RenderView(void);		// must set r_refdef first

void R_Init_EFrags (void);
void R_AddEfrags(entity_t *ent);
void R_NewMap(qbool vid_restart);
void R_NewMapPreLoad(void);

void R_PushDlights(void);
