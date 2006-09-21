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

#include "quakedef.h"


//VULT
#include "vx_vertexlights.h"


entity_t	r_worldentity;

qbool	r_cache_thrash;		// compatability

vec3_t		modelorg, r_entorigin;
entity_t	*currententity;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

mplane_t	frustum[4];

int			c_brush_polys, c_alias_polys;

int			particletexture;	// little dot for particles
int			playertextures;		// up to 16 color translated skins
int			playerfbtextures[MAX_CLIENTS];
int			skyboxtextures;
int			underwatertexture, detailtexture;	

float		gldepthmin, gldepthmax;	// for gl_ztrick

// view origin
vec3_t		vup, vpn, vright;
vec3_t		r_origin;

float	r_world_matrix[16];
float	r_base_world_matrix[16];

// screen size info
refdef_t	r_refdef;
refdef2_t	r_refdef2;

mleaf_t		*r_viewleaf, *r_oldviewleaf;
mleaf_t		*r_viewleaf2, *r_oldviewleaf2;	// for watervis hack

texture_t	*r_notexture_mip;

int			d_lightstylevalue[256];	// 8.8 fraction of base light value

cvar_t cl_multiview = {"cl_multiview", "0" };
cvar_t cl_mvdisplayhud = {"cl_mvdisplayhud", "1"};
cvar_t cl_mvinset = {"cl_mvinset", "0"};
cvar_t cl_mvinsetcrosshair = {"cl_mvinsetcrosshair", "1"};
cvar_t cl_mvinsethud = {"cl_mvinsethud", "1"};

cvar_t	r_drawentities = {"r_drawentities", "1"};
cvar_t	r_lerpframes = {"r_lerpframes", "1"};
cvar_t	r_lerpmuzzlehack = {"r_lerpmuzzlehack", "1"};
cvar_t	r_drawflame = {"r_drawflame", "1"};
cvar_t	r_speeds = {"r_speeds", "0"};
cvar_t	r_fullbright = {"r_fullbright", "0"};
cvar_t	r_lightmap = {"r_lightmap", "0"};
cvar_t	gl_shaftlight = {"gl_shaftlight", "1"};
cvar_t	r_shadows = {"r_shadows", "0"};
cvar_t	r_wateralpha = {"gl_turbalpha", "1"};
cvar_t	r_dynamic = {"r_dynamic", "1"};
cvar_t	r_novis = {"r_novis", "0"};
cvar_t	r_netgraph = {"r_netgraph", "0"};
cvar_t	r_netstats = {"r_netstats", "0"};
cvar_t	r_fullbrightSkins = {"r_fullbrightSkins", "1"};
cvar_t	r_fastsky = {"r_fastsky", "0"};
cvar_t  r_fastturb = {"r_fastturb", "0"};
// START shaman RFE 1022504
// cvar_t r_skycolor = {"r_skycolor", "4"};
cvar_t	r_skycolor   = {"r_skycolor", "40 80 150"};
cvar_t  r_telecolor  = {"r_telecolor", "255 60 60"};
cvar_t  r_lavacolor  = {"r_lavacolor", "80 0 0"};
cvar_t  r_slimecolor = {"r_slimecolor", "10 60 10"};
cvar_t  r_watercolor = {"r_watercolor", "50 80 120"};
// END shaman RFE 1022504
qbool OnChange_r_drawflat(cvar_t *v, char *skyname);
cvar_t	r_drawflat   = {"r_drawflat", "0", 0, OnChange_r_drawflat};
cvar_t	r_wallcolor  = {"r_wallcolor", "255 255 255", 0, OnChange_r_drawflat};
cvar_t	r_floorcolor = {"r_floorcolor", "50 100 150", 0, OnChange_r_drawflat};

cvar_t	r_farclip			= {"r_farclip", "4096"};
qbool OnChange_r_skyname(cvar_t *v, char *s);
cvar_t	r_skyname			= {"r_skyname", "bloody-marvelous512", 0, OnChange_r_skyname};
cvar_t	gl_detail			= {"gl_detail","0"};			
// START shaman :: balancing variables
cvar_t	gl_caustics			= {"gl_caustics", "0"}; // 1		
cvar_t  gl_waterfog			= {"gl_turbfog", "0"}; // 2			
cvar_t  gl_waterfog_density = {"gl_turbfogDensity", "1"};	
// END shaman :: balancing variables

cvar_t  gl_lumaTextures = {"gl_lumaTextures", "1"};	
cvar_t	gl_subdivide_size = {"gl_subdivide_size", "128", CVAR_ARCHIVE};
cvar_t	gl_clear = {"gl_clear", "0"};
qbool OnChange_gl_clearColor(cvar_t *v, char *s);
cvar_t	gl_clearColor = {"gl_clearColor", "0 0 0", 0, OnChange_gl_clearColor};
cvar_t	gl_cull = {"gl_cull", "1"};

cvar_t	gl_ztrick = {"gl_ztrick", "1"};

cvar_t	gl_smoothmodels = {"gl_smoothmodels", "1"};
cvar_t	gl_affinemodels = {"gl_affinemodels", "0"};
// START shaman :: balancing variables
cvar_t	gl_polyblend = {"gl_polyblend", "1"}; // 0
// END shaman :: balancing variables
cvar_t	gl_flashblend = {"gl_flashblend", "0"};
cvar_t	gl_playermip = {"gl_playermip", "0"};
cvar_t	gl_nocolors = {"gl_nocolors", "0"};
cvar_t	gl_finish = {"gl_finish", "0"};
cvar_t	gl_fb_bmodels = {"gl_fb_bmodels", "1"};
cvar_t	gl_fb_models = {"gl_fb_models", "1"};
cvar_t	gl_lightmode = {"gl_lightmode", "2"};
cvar_t	gl_loadlitfiles = {"gl_loadlitfiles", "1"};
cvar_t	gl_colorlights = {"gl_colorlights", "1"};

// START shaman :: balancing variables
cvar_t gl_solidparticles = {"gl_solidparticles", "0"}; // 1
cvar_t gl_part_explosions = {"gl_part_explosions", "0"}; // 1
cvar_t gl_part_trails = {"gl_part_trails", "0"}; // 1
cvar_t gl_part_spikes = {"gl_part_spikes", "0"}; // 1
cvar_t gl_part_gunshots = {"gl_part_gunshots", "0"}; // 1
cvar_t gl_part_blood = {"gl_part_blood", "0"}; // 1
cvar_t gl_part_telesplash = {"gl_part_telesplash", "0"}; // 1
cvar_t gl_part_blobs = {"gl_part_blobs", "0"}; // 1
cvar_t gl_part_lavasplash = {"gl_part_lavasplash", "0"}; // 1
cvar_t gl_part_inferno = {"gl_part_inferno", "0"}; // 1
// END shaman :: balancing variables

// START shaman RFE 1032143 {
cvar_t  gl_fogenable		= {"gl_fog", "0"};
// END shaman RFE 1032143
cvar_t  gl_fogstart			= {"gl_fogstart", "50.0"};
cvar_t  gl_fogend			= {"gl_fogend", "800.0"};
cvar_t  gl_fogred			= {"gl_fogred", "0.6"};
cvar_t  gl_foggreen			= {"gl_foggreen", "0.5"};
cvar_t  gl_fogblue			= {"gl_fogblue", "0.4"};
cvar_t  gl_fogsky			= {"gl_fogsky", "1"}; 

cvar_t	model_color_backpack		= {"model_color_backpack", "255 255 255"};
cvar_t	model_color_enemy			= {"model_color_enemy", "255 255 255"};
cvar_t	model_color_team			= {"model_color_team", "255 255 255"};
cvar_t	model_color_world			= {"model_color_world", "255 255 255"};
cvar_t	model_color_missile			= {"model_color_missile", "255 255 255"};
cvar_t	model_color_quad			= {"model_color_quad", "255 255 255"};
cvar_t	model_color_armor			= {"model_color_armor", "255 255 255"};
cvar_t	model_color_pent			= {"model_color_pent", "255 255 255"};
cvar_t	model_color_gren			= {"model_color_gren", "255 255 255"};
cvar_t	model_color_spike			= {"model_color_spike", "255 255 255"};
cvar_t	model_color_v_shot			= {"model_color_v_shot", "255 255 255"};
cvar_t	model_color_bolt			= {"model_color_bolt", "255 255 255"};
cvar_t	model_color_bolt2			= {"model_color_bolt2", "255 255 255"};
cvar_t	model_color_bolt3			= {"model_color_bolt3", "255 255 255"};
cvar_t	model_color_s_spike			= {"model_color_s_spike", "255 255 255"};
cvar_t	model_color_ring			= {"model_color_ring", "255 255 255"};
cvar_t	model_color_suit			= {"model_color_suit", "255 255 255"};
cvar_t	model_color_rl				= {"model_color_rl", "255 255 255"};
cvar_t	model_color_lg				= {"model_color_lg", "255 255 255"};
cvar_t	model_color_mega			= {"model_color_mega", "255 255 255"};
cvar_t	model_color_ssg				= {"model_color_ssg", "255 255 255"};
cvar_t	model_color_ng				= {"model_color_ng", "255 255 255"};
cvar_t	model_color_sng				= {"model_color_sng", "255 255 255"};
cvar_t	model_color_gl				= {"model_color_gl", "255 255 255"};
cvar_t	model_color_gib1			= {"model_color_gib1", "255 255 255"};
cvar_t	model_color_gib2			= {"model_color_gib2", "255 255 255"};
cvar_t	model_color_gib3			= {"model_color_gib3", "255 255 255"};
cvar_t	model_color_health25		= {"model_color_health25", "255 255 255"};
cvar_t	model_color_health10		= {"model_color_health10", "255 255 255"};
cvar_t	model_color_rocks			= {"model_color_rocks", "255 255 255"};
cvar_t	model_color_rocks1			= {"model_color_rocks1", "255 255 255"};
cvar_t	model_color_cells			= {"model_color_cells", "255 255 255"};
cvar_t	model_color_cells1			= {"model_color_cells1", "255 255 255"};
cvar_t	model_color_nails			= {"model_color_nails", "255 255 255"};
cvar_t	model_color_nails1			= {"model_color_nails1", "255 255 255"};
cvar_t	model_color_shells			= {"model_color_shells", "255 255 255"};
cvar_t	model_color_shells1			= {"model_color_shells1", "255 255 255"};

cvar_t	model_color_backpack_enable			= {"model_color_backpack_enable", "0"};
cvar_t	model_color_enemy_enable			= {"model_color_enemy_enable", "0"};
cvar_t	model_color_team_enable				= {"model_color_team_enable", "0"};
cvar_t	model_color_world_enable			= {"model_color_world_enable", "0"};
cvar_t	model_color_missile_enable			= {"model_color_missile_enable", "0"};
cvar_t	model_color_quad_enable				= {"model_color_quad_enable", "0"};
cvar_t	model_color_armor_enable			= {"model_color_armor_enable", "0"};
cvar_t	model_color_pent_enable				= {"model_color_pent_enable", "0"};
cvar_t	model_color_gren_enable				= {"model_color_gren_enable", "0"};
cvar_t	model_color_spike_enable			= {"model_color_spike_enable", "0"};
cvar_t	model_color_v_shot_enable			= {"model_color_v_shot_enable", "0"};
cvar_t	model_color_bolt_enable				= {"model_color_bolt_enable", "0"};
cvar_t	model_color_bolt2_enable			= {"model_color_bolt2_enable", "0"};
cvar_t	model_color_bolt3_enable			= {"model_color_bolt3_enable", "0"};
cvar_t	model_color_s_spike_enable			= {"model_color_s_spike_enable", "0"};
cvar_t	model_color_ring_enable				= {"model_color_ring_enable", "0"};
cvar_t	model_color_suit_enable				= {"model_color_suit_enable", "0"};
cvar_t	model_color_rl_enable				= {"model_color_rl_enable", "0"};
cvar_t	model_color_lg_enable				= {"model_color_lg_enable", "0"};
cvar_t	model_color_mega_enable				= {"model_color_mega_enable", "0"};
cvar_t	model_color_ssg_enable				= {"model_color_ssg_enable", "0"};
cvar_t	model_color_ng_enable				= {"model_color_ng_enable", "0"};
cvar_t	model_color_sng_enable				= {"model_color_sng_enable", "0"};
cvar_t	model_color_gl_enable				= {"model_color_gl_enable", "0"};
cvar_t	model_color_gib1_enable				= {"model_color_gib1_enable", "0"};
cvar_t	model_color_gib2_enable				= {"model_color_gib2_enable", "0"};
cvar_t	model_color_gib3_enable				= {"model_color_gib3_enable", "0"};
cvar_t	model_color_health25_enable			= {"model_color_health25_enable", "0"};
cvar_t	model_color_health10_enable			= {"model_color_health10_enable", "0"};
cvar_t	model_color_rocks_enable			= {"model_color_rocks_enable", "0"};
cvar_t	model_color_rocks1_enable			= {"model_color_rocks1_enable", "0"};
cvar_t	model_color_cells_enable			= {"model_color_cells_enable", "0"};
cvar_t	model_color_cells1_enable			= {"model_color_cells1_enable", "0"};
cvar_t	model_color_nails_enable			= {"model_color_nails_enable", "0"};
cvar_t	model_color_nails1_enable			= {"model_color_nails1_enable", "0"};
cvar_t	model_color_shells_enable			= {"model_color_shells_enable", "0"};
cvar_t	model_color_shells1_enable			= {"model_color_shells1_enable", "0"};

cvar_t	colored_model_randomc				= {"colored_model_randomc", "0"};

int		lightmode = 2;

model_color_t model_color_variable[MODEL_COLOR_COUNT] = {
	{&model_color_world_enable,		&model_color_world,		"world",	""},
	{&model_color_enemy_enable,		&model_color_enemy,		"enemy",	"progs/player.mdl"},
	{&model_color_team_enable,		&model_color_team,		"team",		"progs/player.mdl"},
	{&model_color_backpack_enable,	&model_color_backpack,	"backpack",	"progs/backpack.mdl"},
	{&model_color_missile_enable,	&model_color_missile,	"missle",	"progs/missile.mdl"},
	{&model_color_quad_enable,		&model_color_quad,		"quad",		"progs/quaddama.mdl"},
	{&model_color_armor_enable,		&model_color_armor,		"armor",	"progs/armor.mdl"},
	{&model_color_pent_enable,		&model_color_pent,		"pent",		"progs/invulner.mdl"},
	{&model_color_gren_enable,		&model_color_gren,		"grenade",	"progs/grenade.mdl"},
	{&model_color_s_spike_enable,	&model_color_s_spike,	"s_spike",	"progs/s_spike.mdl"},
	{&model_color_spike_enable,		&model_color_spike,		"spike",	"progs/spike.mdl"},
	{&model_color_v_shot_enable,	&model_color_v_shot,	"v_shot",	"progs/v_shot.mdl"},
	{&model_color_bolt_enable,		&model_color_bolt,		"bolt",		"progs/bolt.mdl"},
	{&model_color_bolt2_enable,		&model_color_bolt2,		"bolt2",	"progs/bolt2.mdl"},
	{&model_color_bolt3_enable,		&model_color_bolt3,		"bolt3",	"progs/bolt3.mdl"},
	{&model_color_ring_enable,		&model_color_ring,		"ring",		"progs/invisibl.mdl"},
	{&model_color_suit_enable,		&model_color_suit,		"suit",		"progs/suit.mdl"},
	{&model_color_rl_enable,		&model_color_rl,		"rl",		"progs/g_rock2.mdl"},
	{&model_color_lg_enable,		&model_color_lg,		"lg",		"progs/g_light.mdl"},
	{&model_color_mega_enable,		&model_color_mega,		"mega",		"maps/b_bh100.bsp"},
	{&model_color_ssg_enable,		&model_color_ssg,		"ssg",		"progs/g_shot.mdl"},
	{&model_color_ng_enable,		&model_color_ng,		"ng",		"progs/g_nail.mdl"},
	{&model_color_sng_enable,		&model_color_sng,		"sng",		"progs/g_nail2.mdl"},
	{&model_color_gl_enable,		&model_color_gl,		"gl",		"progs/g_rock.mdl"},
	{&model_color_gib1_enable,		&model_color_gib1,		"gib1",		"progs/gib1.mdl"},
	{&model_color_gib2_enable,		&model_color_gib2,		"gib2",		"progs/gib2.mdl"},
	{&model_color_gib3_enable,		&model_color_gib3,		"gib3",		"progs/gib3.mdl"},
	{&model_color_health25_enable,	&model_color_health25,	"health25",	"maps/b_bh25.bsp"},
	{&model_color_health10_enable,	&model_color_health10,	"health10", "maps/b_bh10.bsp"},
	{&model_color_rocks_enable,		&model_color_rocks,		"rocks",	"maps/b_rock0.bsp"},
	{&model_color_rocks1_enable,	&model_color_rocks1,	"rocks1",	"maps/b_rock1.bsp"},
	{&model_color_cells_enable,		&model_color_cells,		"cells",	"maps/b_batt0.bsp"},
	{&model_color_cells1_enable,	&model_color_cells1,	"cells1",	"maps/b_batt1.bsp"},
	{&model_color_nails_enable,		&model_color_nails,		"nails",	"maps/b_nail0.bsp"},
	{&model_color_nails1_enable,	&model_color_nails1,	"nails1",	"maps/b_nail1.bsp"},
	{&model_color_shells_enable,	&model_color_shells,	"shells",	"maps/b_shell0.bsp"},
	{&model_color_shells1_enable,	&model_color_shells1,	"shells1",	"maps/b_shell1.bsp"}
};

//static int deathframes[] = { 49, 60, 69, 77, 84, 93, 102, 0 };

void R_MarkLeaves (void);
void R_InitBubble (void);

char *Model_Color_GetTextureModeName(int texturemode)
{
	int n;
	char *text;

	if (texturemode == MODEL_COLOR_MODE_MODULATE)
	{
		n = strlen ("Modulate");
		text = Q_calloc(n + 1, sizeof(char));
		strcpy(text, "Modulate");
	}
	else if (texturemode == MODEL_COLOR_MODE_BLEND)
	{
		n = strlen ("Blend");
		text = Q_calloc(n + 1, sizeof(char));
		strcpy(text, "Blend");
	}
	else if (texturemode == MODEL_COLOR_MODE_DECAL)
	{
		n = strlen ("Decal");
		text = Q_calloc(n + 1, sizeof(char));
		strcpy(text, "Decal");
	}
	else if (texturemode == MODEL_COLOR_MODE_ADD)
	{
		n = strlen ("Add");
		text = Q_calloc(n + 1, sizeof(char));
		strcpy(text, "Add");
	}
	else if (texturemode == MODEL_COLOR_MODE_REPLACE)
	{
		n = strlen ("Replace");
		text = Q_calloc(n + 1, sizeof(char));
		strcpy(text, "Replace");
	}
	else
	{
		n = strlen ("Color off");
		text = Q_calloc(n + 1, sizeof(char));
		strcpy(text, "Color off");
	}

	return text;
}

void Model_Color_Print(model_color_t model_color_var)
{
	char *texture_mode = NULL;
	byte *col = StringToRGB(model_color_var.color->string);
	texture_mode = Model_Color_GetTextureModeName(model_color_var.enable->value);

	// Emphasise the specified model.
	Com_Printf("%10s - %3d %3d %3d - (%d) %s\n", 
		model_color_var.name, 
		(int)col[0],
		(int)col[1],
		(int)col[2],
		(int)model_color_var.enable->value, texture_mode);

	Q_free(texture_mode);
}

#define MODEL_COLOR_LINE	"----------------------------------------\n"

void R_Model_Color_f(void)
{
	int x;

	if (Cmd_Argc() == 2)
	{
		// Example: model_color world

		// Header.
		Com_Printf("%10s - %3s %3s %3s - Texture mode\n", "Name", "R", "G", "B");
		Com_Printf(MODEL_COLOR_LINE);

		// Print the current color for the current model.
		for (x = 0; x < MODEL_COLOR_COUNT; x++)
		{
			if (!strcmp(Cmd_Argv(1), model_color_variable[x].name))
			{
				Model_Color_Print(model_color_variable[x]);
			}
		}
		Com_Printf(MODEL_COLOR_LINE);
	}
	else if (Cmd_Argc() == 3) 
	{
		// Example: model_color world 2

		// Find the model and set it's enable mode.
		for (x = 0; x < MODEL_COLOR_COUNT; x++)
		{
			if (!strcmp(Cmd_Argv(1), model_color_variable[x].name))
			{
				Cvar_Set(model_color_variable[x].enable, Cmd_Argv(2));
			}
		}
	}
	else if (Cmd_Argc() == 5 || Cmd_Argc() == 6) 
	{
		// Example: model_color world 255 0 0

		for (x = 0; x < MODEL_COLOR_COUNT; x++)
		{
			if (!strcmp(Cmd_Argv(1), model_color_variable[x].name))
			{
				Cvar_Set(model_color_variable[x].color, va("%s %s %s", Cmd_Argv(2), Cmd_Argv(3), Cmd_Argv(4)));
				
				// Set the texture mode if it's available.
				if (Cmd_Argc() == 6)
				{
					Cvar_Set(model_color_variable[x].enable, Cmd_Argv(5));
				}
			}
		}
	}
	else
	{
		// Header.
		Com_Printf("%10s - %3s %3s %3s - Texture mode\n", "Name", "R", "G", "B");
		Com_Printf(MODEL_COLOR_LINE);
		// Print the current colors for all the models.
		for (x = 0; x < MODEL_COLOR_COUNT; x++)
		{
			Model_Color_Print(model_color_variable[x]);
		}

		// Show a usage message.
		Com_Printf(MODEL_COLOR_LINE);
		Com_Printf("Usage:\nTo set color: %s modelname R G B\nTo set enable mode: %s modelname [texturemode]\n", Cmd_Argv(0), Cmd_Argv(0));
		Com_Printf("Where texturemode is between 0-5.\n");
	}
}


//Returns true if the box is completely outside the frustom
qbool R_CullBox (vec3_t mins, vec3_t maxs) {
	int i;

	for (i = 0; i < 4; i++) {
		if (BOX_ON_PLANE_SIDE (mins, maxs, &frustum[i]) == 2)
			return true;
	}
	return false;
}

//Returns true if the sphere is completely outside the frustum
qbool R_CullSphere (vec3_t centre, float radius) {
	int i;
	mplane_t *p;

	for (i = 0, p = frustum; i < 4; i++, p++) {
		if (PlaneDiff(centre, p) <= -radius)
			return true;
	}

	return false;
}

void R_RotateForEntity (entity_t *e) {
	glTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);

	glRotatef (e->angles[1], 0, 0, 1);
	glRotatef (-e->angles[0], 0, 1, 0);
	glRotatef (e->angles[2], 1, 0, 0);
}


mspriteframe_t *R_GetSpriteFrame (entity_t *currententity) {
	msprite_t *psprite;
	mspritegroup_t *pspritegroup;
	mspriteframe_t *pspriteframe;
	int i, numframes, frame;
	float *pintervals, fullinterval, targettime, time;

	psprite = currententity->model->cache.data;
	frame = currententity->frame;

	if (frame >= psprite->numframes || frame < 0) {
		Com_Printf ("R_GetSpriteFrame: no such frame %d\n", frame);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE) {
		pspriteframe = psprite->frames[frame].frameptr;
	} else {
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = r_refdef2.time;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by 0
		targettime = time - ((int) (time / fullinterval)) * fullinterval;

		for (i = 0; i < (numframes - 1); i++) {
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}

void R_DrawSpriteModel (entity_t *e) {
	vec3_t point, right, up;
	mspriteframe_t *frame;
	msprite_t *psprite;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	frame = R_GetSpriteFrame (e);
	psprite = currententity->model->cache.data;

	if (psprite->type == SPR_ORIENTED) {
		// bullet marks on walls
		AngleVectors (currententity->angles, NULL, right, up);
	} else if (psprite->type == SPR_FACING_UPRIGHT) {
		VectorSet (up, 0, 0, 1);
		right[0] = e->origin[1] - r_origin[1];
		right[1] = -(e->origin[0] - r_origin[0]);
		right[2] = 0;
		VectorNormalizeFast (right);
	} else if (psprite->type == SPR_VP_PARALLEL_UPRIGHT) {
		VectorSet (up, 0, 0, 1);
		VectorCopy (vright, right);
	} else {	// normal sprite
		VectorCopy (vup, up);
		VectorCopy (vright, right);
	}

    GL_Bind(frame->gl_texturenum);

	glBegin (GL_QUADS);

	glTexCoord2f (0, 1);
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->left, right, point);
	glVertex3fv (point);

	glTexCoord2f (0, 0);
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->left, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 0);
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->right, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 1);
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->right, right, point);
	glVertex3fv (point);

	glEnd ();
}


#define NUMVERTEXNORMALS	162

vec3_t	shadevector;

qbool	full_light;
float		shadelight, ambientlight;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 64
byte	r_avertexnormal_dots[SHADEDOT_QUANT][NUMVERTEXNORMALS] =
#include "anorm_dots.h"
;

byte	*shadedots = r_avertexnormal_dots[0];

int		lastposenum;

float	r_framelerp;
float	r_modelalpha;
float	r_lerpdistance;

//VULT COLOURED MODEL LIGHTS
extern vec3_t lightcolor;
float apitch, ayaw;
vec3_t vertexlight;

void GL_DrawAliasFrame(aliashdr_t *paliashdr, int pose1, int pose2, qbool mtex) {
    int *order, count;
	vec3_t interpolated_verts;
    float l, lerpfrac;
    trivertx_t *verts1, *verts2;
	//VULT COLOURED MODEL LIGHTS
	int i;
	vec3_t lc;

	lerpfrac = r_framelerp;
	lastposenum = (lerpfrac >= 0.5) ? pose2 : pose1;	

    verts2 = verts1 = (trivertx_t *) ((byte *) paliashdr + paliashdr->posedata);

    verts1 += pose1 * paliashdr->poseverts;
    verts2 += pose2 * paliashdr->poseverts;

    order = (int *) ((byte *) paliashdr + paliashdr->commands);

	if (r_modelalpha < 1)
		glEnable(GL_BLEND);

    while ((count = *order++)) {
		if (count < 0) {
			count = -count;
			glBegin(GL_TRIANGLE_FAN);
		} else if (count > 0) {
			glBegin(GL_TRIANGLE_STRIP);
		}

		do {
			// texture coordinates come from the draw list
			if (mtex) {
				qglMultiTexCoord2f (GL_TEXTURE0_ARB, ((float *) order)[0], ((float *) order)[1]);
				qglMultiTexCoord2f (GL_TEXTURE1_ARB, ((float *) order)[0], ((float *) order)[1]);
			} else {
				glTexCoord2f (((float *) order)[0], ((float *) order)[1]);
			}

			order += 2;

			if ((currententity->flags & RF_LIMITLERP)) {
				lerpfrac = VectorL2Compare(verts1->v, verts2->v, r_lerpdistance) ? r_framelerp : 1;
			}

			// VULT VERTEX LIGHTING
			if (amf_lighting_vertex.value && !full_light)
			{
				l = VLight_LerpLight(verts1->lightnormalindex, verts2->lightnormalindex, lerpfrac, apitch, ayaw);
			}
			else
			{
				l = FloatInterpolate(shadedots[verts1->lightnormalindex], lerpfrac, shadedots[verts2->lightnormalindex]) / 127.0;
				l = (l * shadelight + ambientlight) / 256.0;
			}
			l = min(l , 1);
			//VULT COLOURED MODEL LIGHTS
			if (amf_lighting_colour.value && !full_light)
			{
				for (i=0;i<3;i++)
					lc[i] = lightcolor[i] / 256 + l;
					//Com_Printf("rgb light : %f %f %f\n", lc[0], lc[1], lc[2]);
					glColor4f(lc[0],lc[1],lc[2], r_modelalpha);
			}
			else
			{
				byte *col, color[3];
				int c;
				
				// Loop through all color models (excluding the world which has index 0).
				for (c = 1; c < MODEL_COLOR_COUNT; c++)
				{
					// Only change color for models that have it enabled.
					if(!strcmp(currententity->model->name, model_color_variable[c].model) && model_color_variable[c].enable->value)
					{
						// We have to threat player.mdl as a special case.
						if (c == MODEL_COLOR_ENEMY || c == MODEL_COLOR_TEAM)
						{
							if (strcmp(cl.players[cl.playernum].team, currententity->scoreboard->team))
							{
								// If we're drawing an enemy.
								col = StringToRGB(model_color_variable[MODEL_COLOR_ENEMY].color->string);
								memcpy(color, col, 3);
								glColor4f((float)color[0], (float)color[1], (float)color[2], r_modelalpha);
							}
							else
							{
								// Drawing a teammate.
								col = StringToRGB(model_color_variable[MODEL_COLOR_TEAM].color->string);
								memcpy(color, col, 3);
								glColor4f((float)color[0], (float)color[1], (float)color[2], r_modelalpha);
							}
						}
				
						// Get the color from the color string and set the color.
						col = StringToRGB(model_color_variable[c].color->string);
						memcpy(color, col, 3);
						glColor4f((float)color[0], (float)color[1], (float)color[2], r_modelalpha);
						break;
					}
					else
					{
						glColor4f (l, l, l, r_modelalpha);
					}
				}
			}

			VectorInterpolate(verts1->v, lerpfrac, verts2->v, interpolated_verts);
			glVertex3fv(interpolated_verts);
			

			verts1++;
			verts2++;
		} while (--count);

		glEnd();
    }

	if (r_modelalpha < 1)
		glDisable(GL_BLEND);
}

void R_SetupAliasFrame (maliasframedesc_t *oldframe, maliasframedesc_t *frame, aliashdr_t *paliashdr, qbool mtex) {
	int oldpose, pose, numposes;
	float interval;

	oldpose = oldframe->firstpose;
	numposes = oldframe->numposes;
	if (numposes > 1) {
		interval = oldframe->interval;
		oldpose += (int) (r_refdef2.time / interval) % numposes;
	}

	pose = frame->firstpose;
	numposes = frame->numposes;
	if (numposes > 1) {
		interval = frame->interval;
		pose += (int) (r_refdef2.time / interval) % numposes;
	}

	GL_DrawAliasFrame (paliashdr, oldpose, pose, mtex);
}

extern vec3_t lightspot;

void GL_DrawAliasShadow (aliashdr_t *paliashdr, int posenum) {
	int *order, count;
	vec3_t point;
	float lheight = currententity->origin[2] - lightspot[2], height = 1 - lheight;
	trivertx_t *verts;

	verts = (trivertx_t *) ((byte *) paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *) ((byte *) paliashdr + paliashdr->commands);

	while ((count = *order++)) {
		// get the vertex count and primitive type
		if (count < 0) {
			count = -count;
			glBegin (GL_TRIANGLE_FAN);
		} else {
			glBegin (GL_TRIANGLE_STRIP);
		}

		do {
			//no texture for shadows
			order += 2;

			// normals and vertexes come from the frame list
			point[0] = verts->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
			point[1] = verts->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
			point[2] = verts->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];

			point[0] -= shadevector[0] * (point[2] +lheight);
			point[1] -= shadevector[1] * (point[2] + lheight);
			point[2] = height;
			//height -= 0.001;
			glVertex3fv (point);

			verts++;
		} while (--count);

		glEnd ();
	}	
}

//VULT COLOURED MODEL LIGHTING
vec3_t dlight_color;
extern float bubblecolor[NUM_DLIGHTTYPES][4];
void R_AliasSetupLighting(entity_t *ent) {
	int minlight, lnum;
	float add, fbskins;
	vec3_t dist;
	model_t *clmodel;

	//VULT COLOURED MODEL LIGHTING
	int i;
	float radiusmax = 0;

	clmodel = ent->model;

	// make thunderbolt and torches full light
	if (clmodel->modhint == MOD_THUNDERBOLT) {
		ambientlight = 60 + 150 * bound(0, gl_shaftlight.value, 1);
		shadelight = 0;
		full_light = true;
		return;
	} else if (clmodel->modhint == MOD_FLAME) {
		ambientlight = 255;
		shadelight = 0;
		full_light = true;
		return;
	}

	//normal lighting
	full_light = false;
	ambientlight = shadelight = R_LightPoint (ent->origin);

	//VULT COLOURED MODEL LIGHTS
	if (amf_lighting_colour.value)
	{
		for (lnum = 0; lnum < MAX_DLIGHTS; lnum++)
		{
			if (cl_dlights[lnum].die < r_refdef2.time || !cl_dlights[lnum].radius)
				continue;

			VectorSubtract (ent->origin, cl_dlights[lnum].origin, dist);
			add = cl_dlights[lnum].radius - VectorLength(dist);

			if (add > 0)
			{
				//VULT VERTEX LIGHTING
				if (amf_lighting_vertex.value)
				{
					if (!radiusmax)
					{
						radiusmax = cl_dlights[lnum].radius;
						VectorCopy(cl_dlights[lnum].origin, vertexlight);
					}
					else if (cl_dlights[lnum].radius > radiusmax)
					{
						radiusmax = cl_dlights[lnum].radius;
						VectorCopy(cl_dlights[lnum].origin, vertexlight);
					}
				}
				VectorCopy(bubblecolor[cl_dlights[lnum].type], dlight_color);
				for (i=0;i<3;i++)
				{
					lightcolor[i] = lightcolor[i] + (dlight_color[i]*add)*2;
					if (lightcolor[i] > 256)
					{
						switch (i)
						{
						case 0:
							lightcolor[1] = lightcolor[1] - (1 * lightcolor[1]/3); 
							lightcolor[2] = lightcolor[2] - (1 * lightcolor[2]/3); 
							break;
						case 1:
							lightcolor[0] = lightcolor[0] - (1 * lightcolor[0]/3); 
							lightcolor[2] = lightcolor[2] - (1 * lightcolor[2]/3); 
							break;
						case 2:
							lightcolor[1] = lightcolor[1] - (1 * lightcolor[1]/3); 
							lightcolor[0] = lightcolor[0] - (1 * lightcolor[0]/3); 
							break;
						}
					}
				}
				//ambientlight += add;
			}
		}
	}
	else
	{
		for (lnum = 0; lnum < MAX_DLIGHTS; lnum++) {
			if (cl_dlights[lnum].die < r_refdef2.time || !cl_dlights[lnum].radius)
				continue;

			VectorSubtract (ent->origin, cl_dlights[lnum].origin, dist);
			add = cl_dlights[lnum].radius - VectorLength(dist);

			if (add > 0)
			{
				//VULT VERTEX LIGHTING
				if (amf_lighting_vertex.value)
				{
					if (!radiusmax)
					{
						radiusmax = cl_dlights[lnum].radius;
						VectorCopy(cl_dlights[lnum].origin, vertexlight);
					}
					else if (cl_dlights[lnum].radius > radiusmax)
					{
						radiusmax = cl_dlights[lnum].radius;
						VectorCopy(cl_dlights[lnum].origin, vertexlight);
					}
				}
				ambientlight += add;
			}
		}
	}
	//calculate pitch and yaw for vertex lighting
	if (amf_lighting_vertex.value)
	{
		vec3_t dist, ang;
		apitch = currententity->angles[0];
		ayaw = currententity->angles[1];

		if (!radiusmax)
		{
			vlight_pitch = 45;
			vlight_yaw = 45;
		}
		else
		{
			VectorSubtract (vertexlight, currententity->origin, dist);
			vectoangles(dist, ang);
			vlight_pitch = ang[0];
			vlight_yaw = ang[1];
		}
	}

	// clamp lighting so it doesn't overbright as much
	if (ambientlight > 128)
		ambientlight = 128;
	if (ambientlight + shadelight > 192)
		shadelight = 192 - ambientlight;

	// always give the gun some light
	if ((ent->flags & RF_WEAPONMODEL) && ambientlight < 24)
		ambientlight = shadelight = 24;

	// never allow players to go totally black
	if (clmodel->modhint == MOD_PLAYER) {
		if (ambientlight < 8)
			ambientlight = shadelight = 8;
	}


	if (clmodel->modhint == MOD_PLAYER) {
		fbskins = bound(0, r_fullbrightSkins.value, cl.fbskins);
		if (fbskins == 1 && gl_fb_models.value == 1) {
			ambientlight = shadelight = 4096;
			full_light = true;
		}
		else if (fbskins == 0) {
			ambientlight = max(ambientlight, 8);
			shadelight = max(shadelight, 8);
			full_light = true;
		}
		else if (fbskins) {
			ambientlight = max(ambientlight, 8 + fbskins * 120);
			shadelight = max(shadelight, 8 + fbskins * 120);
			full_light = true;
		}
	}

	minlight = cl.minlight;

	if (ambientlight < minlight)
		ambientlight = shadelight = minlight;
}

void R_DrawAliasModel (entity_t *ent) {
	int i, anim, skinnum, texture, fb_texture;
	float scale;
	vec3_t mins, maxs;
	aliashdr_t *paliashdr;
	model_t *clmodel;
	maliasframedesc_t *oldframe, *frame;

	//	entity_t *self;
	//static sfx_t *step;//foosteps sounds, commented out
	//static int setstep;

	extern	cvar_t r_viewmodelsize, cl_drawgun;
	
	VectorCopy (ent->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	//TODO: use modhints here? 
	//VULT CORONAS
	if ((!strcmp (ent->model->name, "progs/flame.mdl") /*|| !strcmp (ent->model->name, "progs/flame2.mdl")*/
	|| !strcmp (ent->model->name, "progs/flame3.mdl")) && (  (amf_coronas.value/* && (rand() % 2 < 2)*/)
	|| !strcmp(ent->model->name, "progs/flame0.mdl")) )//joe: support coronas for non-flamed torch too
	{
		//FIXME: This is slow and pathetic as hell, really we should just check the entity
		//alternativley add some kind of permanent client side TE for the torch
		NewStaticLightCorona (C_FIRE, ent->origin);
	}

	if (ent->model->modhint == MOD_TELEPORTDESTINATION && amf_coronas.value)
	{
		NewStaticLightCorona (C_LIGHTNING, ent->origin);
	}

/* joe: replaced this with my own stuff - see R_DrawEntitiesOnList()
	if (amf_part_fire.value && (!strcmp (ent->model->name, "progs/flame.mdl") || !strcmp (ent->model->name, "progs/flame2.mdl") || !strcmp (ent->model->name, "progs/flame3.mdl")))
	{
		if (!strcmp (ent->model->name, "progs/flame.mdl") && !cl.paused)
			ParticleFire(ent->origin);
		else if (!strcmp (ent->model->name, "progs/flame2.mdl") || !strcmp (ent->model->name, "progs/flame3.mdl"))
		{
			if (!cl.paused)
				ParticleFire(ent->origin);
			return;
		}
	}
*/

	clmodel = ent->model;
	paliashdr = (aliashdr_t *) Mod_Extradata (ent->model);	//locate the proper data

	if (ent->frame >= paliashdr->numframes || ent->frame < 0) {
		Com_DPrintf ("R_DrawAliasModel: no such frame %d\n", ent->frame);
		ent->frame = 0;
	}
	if (ent->oldframe >= paliashdr->numframes || ent->oldframe < 0) {
		Com_DPrintf ("R_DrawAliasModel: no such oldframe %d\n", ent->oldframe);
		ent->oldframe = 0;
	}

	frame = &paliashdr->frames[ent->frame];
	oldframe = &paliashdr->frames[ent->oldframe];

#if 0
//TODO:
// cheat protection
// limit footsteps sounds to self 
// add self footsteps :D
// State: crap	
	self = ent;
	if(ent->stepframe != ent->frame && ent->model->modhint == MOD_PLAYER && (self->frame == 1||self->frame == 4||self->frame == 7 || self->frame ==10))
	{
		//Com_Printf("foot..\n");
			//va("footsteps/step%d.wav",(int)(rand()%3+1)));

		if (!setstep)
		{
			step = S_PrecacheSound ("footsteps/step1.wav");
			setstep=true;
		}

		S_StartSound (-1, 0, step , ent->origin, 1, 1);

		ent->stepframe = ent->frame;
	}
#endif

	if (!r_lerpframes.value || ent->framelerp < 0 || ent->oldframe == ent->frame)
		r_framelerp = 1.0;
	else
		r_framelerp = min (ent->framelerp, 1);


	//culling
	if (!(ent->flags & RF_WEAPONMODEL)) {
		if (ent->angles[0] || ent->angles[1] || ent->angles[2]) {
			if (R_CullSphere (ent->origin, max(oldframe->radius, frame->radius)))
				return;
		} else {
			if (r_framelerp == 1) {	
				VectorAdd(ent->origin, frame->bboxmin, mins);
				VectorAdd(ent->origin, frame->bboxmax, maxs);
			} else {
				for (i = 0; i < 3; i++) {
					mins[i] = ent->origin[i] + min (oldframe->bboxmin[i], frame->bboxmin[i]);
					maxs[i] = ent->origin[i] + max (oldframe->bboxmax[i], frame->bboxmax[i]);
				}
			}
			if (R_CullBox (mins, maxs))
				return;
		}
	}

	//get lighting information
	R_AliasSetupLighting(ent);

	shadedots = r_avertexnormal_dots[((int) (ent->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];

	//draw all the triangles
	c_alias_polys += paliashdr->numtris;
	glPushMatrix ();
	R_RotateForEntity (ent);

	if (clmodel->modhint == MOD_EYES) {
		glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2] - (22 + 8));
		// double size of eyes, since they are really hard to see in gl
		glScalef (paliashdr->scale[0] * 2, paliashdr->scale[1] * 2, paliashdr->scale[2] * 2);
	} else if (ent->flags & RF_WEAPONMODEL) {	
		scale = 0.5 + bound(0, r_viewmodelsize.value, 1) / 2;
        glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
        glScalef (paliashdr->scale[0] * scale, paliashdr->scale[1], paliashdr->scale[2]);
    } else {
		glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		glScalef (paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);
	}

	

	anim = (int) (r_refdef2.time * 10) & 3;
	skinnum = ent->skinnum;
	if (skinnum >= paliashdr->numskins || skinnum < 0) {
		Com_DPrintf ("R_DrawAliasModel: no such skin # %d\n", skinnum);
		skinnum = 0;
	}

	texture = paliashdr->gl_texturenum[skinnum][anim];
	fb_texture = paliashdr->fb_texturenum[skinnum][anim];

	


	r_modelalpha = ((ent->flags & RF_WEAPONMODEL) && gl_mtexable) ? bound(0, cl_drawgun.value, 1) : 1;
	//VULT MOTION TRAILS
	if (ent->alpha)
		r_modelalpha = ent->alpha;

	// we can't dynamically colormap textures, so they are cached separately for the players.  Heads are just uncolored.
	if (ent->scoreboard && !gl_nocolors.value) {
		i = ent->scoreboard - cl.players;
		if (i >= 0 && i < MAX_CLIENTS) {
			if (!ent->scoreboard->skin)
				CL_NewTranslation(i);
		    texture = playertextures + i;
			fb_texture = playerfbtextures[i];
		}
	}
	if (full_light || !gl_fb_models.value)
		fb_texture = 0;

	if (gl_smoothmodels.value)
		glShadeModel (GL_SMOOTH);

	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	

	if (fb_texture && gl_mtexable) {
		
		GL_DisableMultitexture ();
		GL_Bind (texture);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		GL_EnableMultitexture ();
		GL_Bind (fb_texture);

		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

		R_SetupAliasFrame (oldframe, frame, paliashdr, true);

		GL_DisableMultitexture ();
	} 
	else 
	{
		byte *col, color[3]; 
		int c;
		qbool model_color_found = false;

		GL_DisableMultitexture();

		// Skip the world model (index 0).
		for (c = 1; c < MODEL_COLOR_COUNT && !model_color_found; c++)
		{
			if (   model_color_variable[c].enable->value > MODEL_COLOR_MODE_NORMAL 
				&& model_color_variable[c].enable->value <= MODEL_COLOR_MODE_REPLACE 
				&& !strcmp(clmodel->name, model_color_variable[c].model))
			{
				// Set the texture mode.
				if(model_color_variable[c].enable->value == MODEL_COLOR_MODE_MODULATE)
				{
					glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				}
				else if(model_color_variable[c].enable->value == MODEL_COLOR_MODE_BLEND)
				{
					glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
				}
				else if(model_color_variable[c].enable->value == MODEL_COLOR_MODE_DECAL)
				{
					glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
				}
				else if(model_color_variable[c].enable->value == MODEL_COLOR_MODE_ADD)
				{
					glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
				}
				else if(model_color_variable[c].enable->value == MODEL_COLOR_MODE_REPLACE)
				{
					glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
				}

				// Flag that we have found at least one model.
				model_color_found = true;

				col = StringToRGB(model_color_variable[c].color->string);
				memcpy(color, col, 3);
				glColor3ubv(color);
			}
		}

		// Reset the texture mode.
		if (!model_color_found)
		{
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		}
		
		GL_Bind (texture);
		R_SetupAliasFrame (oldframe, frame, paliashdr, false);

		if (fb_texture) {
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glEnable (GL_ALPHA_TEST);
			GL_Bind (fb_texture);

			R_SetupAliasFrame (oldframe, frame, paliashdr, false);

			glDisable (GL_ALPHA_TEST);
		}
	}

// Underwater caustics on alias models of QRACK -->
	#define ISUNDERWATER(x) ((x) == CONTENTS_WATER || (x) == CONTENTS_SLIME || (x) == CONTENTS_LAVA)
	#define TruePointContents(p) PM_HullPointContents(&cl.worldmodel->hulls[0], 0, p)
	#define GL_RGB_SCALE 0x8573

	if ((gl_caustics.value) && (underwatertexture && gl_mtexable && ISUNDERWATER(TruePointContents(ent->origin))))
	{
		GL_EnableMultitexture ();
		glBindTexture (GL_TEXTURE_2D, underwatertexture);

		glMatrixMode (GL_TEXTURE);
		glLoadIdentity ();
		glScalef (0.5, 0.5, 1);
		glRotatef (cls.realtime * 10, 1, 0, 0);
		glRotatef (cls.realtime * 10, 0, 1, 0);
		glMatrixMode (GL_MODELVIEW);

		GL_Bind (underwatertexture);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);        
		glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
		glEnable (GL_BLEND);

		R_SetupAliasFrame (oldframe, frame, paliashdr, true);

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_BLEND);            

		GL_SelectTexture(GL_TEXTURE1_ARB);
		glTexEnvi (GL_TEXTURE_ENV, GL_RGB_SCALE, 1);
		glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glDisable (GL_TEXTURE_2D);

		glMatrixMode (GL_TEXTURE);
		glLoadIdentity ();
		glMatrixMode (GL_MODELVIEW);

		GL_DisableMultitexture ();
	}
// <-- Underwater caustics on alias models of QRACK

	glShadeModel (GL_FLAT);
	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glPopMatrix ();

	//VULT MOTION TRAILS - No shadows on motion trails
	if ((r_shadows.value && !full_light && !(ent->flags & RF_NOSHADOW)) && !ent->alpha) {
		float theta;
		static float shadescale = 0;

		if (!shadescale)
			shadescale = 1 / sqrt(2);
		theta = -ent->angles[1] / 180 * M_PI;

		VectorSet(shadevector, cos(theta) * shadescale, sin(theta) * shadescale, shadescale);

		glPushMatrix ();
		glTranslatef (ent->origin[0],  ent->origin[1],  ent->origin[2]);
		glRotatef (ent->angles[1],  0, 0, 1);

		glDisable (GL_TEXTURE_2D);
		glEnable (GL_BLEND);
		glColor4f (0, 0, 0, 0.5);
		GL_DrawAliasShadow (paliashdr, lastposenum);
		glEnable (GL_TEXTURE_2D);
		glDisable (GL_BLEND);

		glPopMatrix ();
	}

	glColor3ubv (color_white);
}


void R_DrawEntitiesOnList (visentlist_t *vislist) {
	int i;

	if (!r_drawentities.value || !vislist->count)
		return;

	if (vislist->alpha)
		glEnable (GL_ALPHA_TEST);

	// draw sprites separately, because of alpha_test
	for (i = 0; i < vislist->count; i++) {
		currententity = &vislist->list[i];
		switch (currententity->model->type) {
			case mod_alias:
				//VULT NAILTRAIL - Hidenails
				if (amf_hidenails.value && currententity->model->modhint == MOD_SPIKE)
					break;
				//VULT ROCKETTRAILS - Hide rockets
				if (amf_hiderockets.value && currententity->model->flags & EF_ROCKET)
					break;
				//VULT CAMERAS - Show/Hide playermodel
				if (currententity->alpha == -1)
				{
					 if (cameratype == C_NORMAL)
						break;
					 else
						currententity->alpha = 1;
				}
				//VULT MOTION TRAILS
				if (currententity->alpha < 0)
					break;

				//joe: handle flame/flame0 model changes
				if (qmb_initialized)
				{
					if (!amf_part_fire.value && !strcmp(currententity->model->name, "progs/flame0.mdl"))
					{
						currententity->model = cl.model_precache[cl_modelindices[mi_flame]];
					}
					else if (amf_part_fire.value)
					{
						if (!strcmp(currententity->model->name, "progs/flame0.mdl"))
						{
							if (!cl.paused)
								ParticleFire (currententity->origin);
						}
						else if (!strcmp(currententity->model->name, "progs/flame.mdl")
							&& cl_flame0_model /* do we have progs/flame0.mdl? */)
						{
							if (!cl.paused)
								ParticleFire (currententity->origin);
							currententity->model = cl_flame0_model;
						}
						else if (!strcmp(currententity->model->name, "progs/flame2.mdl") || !strcmp(currententity->model->name, "progs/flame3.mdl"))
						{
							if (!cl.paused)
								ParticleFire (currententity->origin);
							continue;
						}
					}
				}

				R_DrawAliasModel (currententity);
				break;
			case mod_alias3:
				R_DrawAlias3Model (currententity);
				break;
			case mod_brush:
				R_DrawBrushModel (currententity);
				break;
			case mod_sprite:
				R_DrawSpriteModel (currententity);
				break;
		}
	}

	if (vislist->alpha)
		glDisable (GL_ALPHA_TEST);
}

void R_DrawViewModel (void) {
	centity_t *cent;
	static entity_t gun;

	//VULT CAMERA - Don't draw gun in external camera
	if (cameratype != C_NORMAL)
		return;

	if (!r_drawentities.value || !cl.viewent.current.modelindex)
		return;

	memset(&gun, 0, sizeof(gun));
	cent = &cl.viewent;
	currententity = &gun;

	if (!(gun.model = cl.model_precache[cent->current.modelindex]))
		Host_Error ("R_DrawViewModel: bad modelindex");

	VectorCopy(cent->current.origin, gun.origin);
	VectorCopy(cent->current.angles, gun.angles);
	gun.colormap = vid.colormap;
	gun.flags = RF_WEAPONMODEL | RF_NOSHADOW;
	if (r_lerpmuzzlehack.value) {
		if (cent->current.modelindex != cl_modelindices[mi_vaxe] &&
			cent->current.modelindex != cl_modelindices[mi_vbio] &&
			cent->current.modelindex != cl_modelindices[mi_vgrap] &&
			cent->current.modelindex != cl_modelindices[mi_vknife] &&
			cent->current.modelindex != cl_modelindices[mi_vknife2] &&
			cent->current.modelindex != cl_modelindices[mi_vmedi] &&
			cent->current.modelindex != cl_modelindices[mi_vspan])
		{
			gun.flags |= RF_LIMITLERP;			
			r_lerpdistance =  135;
		}
	}


	gun.frame = cent->current.frame;
	if (cent->frametime >= 0 && cent->frametime <= cl.time) {
		gun.oldframe = cent->oldframe;
		gun.framelerp = (cl.time - cent->frametime) * 10;
	} else {
		gun.oldframe = gun.frame;
		gun.framelerp = -1;
	}


	// hack the depth range to prevent view model from poking into walls
	glDepthRange (gldepthmin, gldepthmin + 0.3 * (gldepthmax - gldepthmin));
	//R_DrawAliasModel (currententity);
	switch(currententity->model->type)
	{
	case mod_alias:
		R_DrawAliasModel (currententity);
		break;
	case mod_alias3:
		R_DrawAlias3Model (currententity);
		break;
	default:
		Com_Printf("Not drawing view model of type %i\n", currententity->model->type);
		break;
	} 
	glDepthRange (gldepthmin, gldepthmax);
/*	
	//
	// THIS CODE FUCKS UP WEAPON ANIMATIONS!
	// (Toniks refdef2, static rendering port from zQuake)
	//
	centity_t *cent;
	static entity_t gun;

	//VULT CAMERA - Don't draw gun in external camera
	if (cameratype != C_NORMAL)
		return;

	if (!r_drawentities.value || !cl.viewent.current.modelindex)
		return;

	memset(&gun, 0, sizeof(gun));
	cent = &cl.viewent;
	currententity = &gun;

	if (!(gun.model = cl.model_precache[cent->current.modelindex]))
		Host_Error ("R_DrawViewModel: bad modelindex");

	VectorCopy(cent->current.origin, gun.origin);
	VectorCopy(cent->current.angles, gun.angles);
	gun.colormap = vid.colormap;
	gun.flags = RF_WEAPONMODEL | RF_NOSHADOW;
	if (r_lerpmuzzlehack.value) {
		if (cent->current.modelindex != cl_modelindices[mi_vaxe] &&
			cent->current.modelindex != cl_modelindices[mi_vbio] &&
			cent->current.modelindex != cl_modelindices[mi_vgrap] &&
			cent->current.modelindex != cl_modelindices[mi_vknife] &&
			cent->current.modelindex != cl_modelindices[mi_vknife2] &&
			cent->current.modelindex != cl_modelindices[mi_vmedi] &&
			cent->current.modelindex != cl_modelindices[mi_vspan])
		{
			gun.flags |= RF_LIMITLERP;			
			r_lerpdistance =  135;
		}
	}


	gun.frame = cent->current.frame;
	if (cent->frametime >= 0 && cent->frametime <= r_refdef2.time) {
		gun.oldframe = cent->oldframe;
		gun.framelerp = (r_refdef2.time - cent->frametime) * 10;
	} else {
		gun.oldframe = gun.frame;
		gun.framelerp = -1;
	}


	// hack the depth range to prevent view model from poking into walls
	glDepthRange (gldepthmin, gldepthmin + 0.3 * (gldepthmax - gldepthmin));
	//R_DrawAliasModel (currententity);
	switch(currententity->model->type)
	{
	case mod_alias:
		R_DrawAliasModel (currententity);
		break;
	case mod_alias3:
		R_DrawAlias3Model (currententity);
		break;
	default:
		Com_Printf("Not drawing view model of type %i\n", currententity->model->type);
		break;
	} 
	glDepthRange (gldepthmin, gldepthmax);
*/
}


void R_PolyBlend (void) {
	extern cvar_t gl_hwblend;

	if (vid_hwgamma_enabled && gl_hwblend.value && !cl.teamfortress)
		return;
	if (!v_blend[3])
		return;

	glDisable (GL_ALPHA_TEST);
	glEnable (GL_BLEND);
	glDisable (GL_TEXTURE_2D);

	glColor4fv (v_blend);

	glBegin (GL_QUADS);
	glVertex2f (r_refdef.vrect.x, r_refdef.vrect.y);
	glVertex2f (r_refdef.vrect.x + r_refdef.vrect.width, r_refdef.vrect.y);
	glVertex2f (r_refdef.vrect.x + r_refdef.vrect.width, r_refdef.vrect.y + r_refdef.vrect.height);
	glVertex2f (r_refdef.vrect.x, r_refdef.vrect.y + r_refdef.vrect.height);
	glEnd ();

	glDisable (GL_BLEND);
	glEnable (GL_TEXTURE_2D);
	glEnable (GL_ALPHA_TEST);

	glColor3ubv (color_white);
}

void R_BrightenScreen (void) {
	extern float vid_gamma;
	float f;

	if (vid_hwgamma_enabled)
		return;
	if (v_contrast.value <= 1.0)
		return;

	f = min (v_contrast.value, 3);
	f = pow (f, vid_gamma);
	
	glDisable (GL_TEXTURE_2D);
	glEnable (GL_BLEND);
	glBlendFunc (GL_DST_COLOR, GL_ONE);
	glBegin (GL_QUADS);
	while (f > 1) {
		if (f >= 2)
			glColor3ubv (color_white);
		else
			glColor3f (f - 1, f - 1, f - 1);
		glVertex2f (0, 0);
		glVertex2f (vid.width, 0);
		glVertex2f (vid.width, vid.height);
		glVertex2f (0, vid.height);
		f *= 0.5;
	}
	glEnd ();
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
	glColor3ubv (color_white);
}

int SignbitsForPlane (mplane_t *out) {
	int	bits, j;

	// for fast box on planeside test
	bits = 0;
	for (j = 0; j < 3; j++) {
		if (out->normal[j] < 0)
			bits |= 1 << j;
	}
	return bits;
}


void R_SetFrustum (void) {
	int i;

	// rotate VPN right by FOV_X/2 degrees
	RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_refdef.fov_x / 2 ) );
	// rotate VPN left by FOV_X/2 degrees
	RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_refdef.fov_x / 2 );
	// rotate VPN up by FOV_X/2 degrees
	RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_refdef.fov_y / 2 );
	// rotate VPN down by FOV_X/2 degrees
	RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_refdef.fov_y / 2 ) );

	for (i = 0; i < 4; i++) {
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}

void R_SetupFrame (void) {
	vec3_t testorigin;
	mleaf_t	*leaf;

	R_AnimateLight ();

	r_framecount++;

	// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);
	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

	// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_oldviewleaf2 = r_viewleaf2;

	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);
	r_viewleaf2 = NULL;

	// check above and below so crossing solid water doesn't draw wrong
	if (r_viewleaf->contents <= CONTENTS_WATER && r_viewleaf->contents >= CONTENTS_LAVA) {
		// look up a bit
		VectorCopy (r_origin, testorigin);
		testorigin[2] += 10;
		leaf = Mod_PointInLeaf (testorigin, cl.worldmodel);
		if (leaf->contents == CONTENTS_EMPTY)
			r_viewleaf2 = leaf;
	} else if (r_viewleaf->contents == CONTENTS_EMPTY) {
		// look down a bit
		VectorCopy (r_origin, testorigin);
		testorigin[2] -= 10;
		leaf = Mod_PointInLeaf (testorigin, cl.worldmodel);
		if (leaf->contents <= CONTENTS_WATER &&	leaf->contents >= CONTENTS_LAVA)
			r_viewleaf2 = leaf;
	}

	V_SetContentsColor (r_viewleaf->contents);
	V_AddWaterfog (r_viewleaf->contents);	 
	V_CalcBlend ();

	r_cache_thrash = false;

	c_brush_polys = 0;
	c_alias_polys = 0;
}

__inline void MYgluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar) {
	GLdouble xmin, xmax, ymin, ymax;
	
	ymax = zNear * tan(fovy * M_PI / 360.0);
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	if (cl_multiview.value == 2 && !cl_mvinset.value && cls.mvdplayback)
		glFrustum( xmin, xmax, ymin + (ymax - ymin)*0.25, ymax - (ymax - ymin)*0.25, zNear, zFar);
	else if (cl_multiview.value == 3 && cls.mvdplayback) {
		if (CURRVIEW == 2)
			glFrustum( xmin, xmax, ymin + (ymax - ymin)*0.25, ymax - (ymax - ymin)*0.25, zNear, zFar);
		else
			glFrustum( xmin, xmax, ymin, ymax, zNear, zFar);
	}
	else
		glFrustum( xmin, xmax, ymin, ymax, zNear, zFar);
}

void R_SetViewports(int glx, int x, int gly, int y2, int w, int h, float max) {

	if (max == 1) {
		glViewport (glx + x, gly + y2, w, h);
		return;
	}
	else if (max == 2 && cl_mvinset.value) {
		if (CURRVIEW == 2)
			glViewport (glx + x, gly + y2, w, h);
		else if (CURRVIEW == 1 && !cl_sbar.value)
			glViewport (glx + x + (glwidth/3)*2 + 2, gly + y2 + (glheight/3)*2, w/3, h/3);
		else if (CURRVIEW == 1 && cl_sbar.value)
			glViewport (glx + x + (glwidth/3)*2 + 2, gly + y2 + (h/3)*2, w/3, h/3);
		else 
			Com_Printf("ERROR!\n");
		return;
	}
	else if (max == 2 && !cl_mvinset.value) {
		if (CURRVIEW == 2)
			glViewport (0, h/2, w, h/2);
		else if (CURRVIEW == 1)
			glViewport (0, 0, w, h/2-1);
		else 
			Com_Printf("ERROR!\n");
		return;

	}
	else if (max == 3) {
		if (CURRVIEW == 2)
			glViewport (0, h/2, w, h/2);
		else if (CURRVIEW == 3)
			glViewport (0, 0, w/2, h/2-1);
		else
			glViewport (w/2, 0, w/2, h/2-1);
		return;
	}
	else {
		if (cl_multiview.value > 4)
			cl_multiview.value = 4;

		if (CURRVIEW == 2)
			glViewport (0, h/2, w/2, h/2);
		else if (CURRVIEW == 3)
			glViewport (w/2, h/2, w/2, h/2);
		else if (CURRVIEW == 4)
			glViewport (0, 0, w/2, h/2-1);
		else if (CURRVIEW == 1)
			glViewport (w/2, 0, w/2, h/2-1);
	}

	return;
} 

void R_SetupGL (void) {
	float screenaspect;
	extern int glwidth, glheight;
	int x, x2, y2, y, w, h, farclip;

	// set up viewpoint
	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();
	x = r_refdef.vrect.x * glwidth / vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth / vid.width;
	y = (vid.height-r_refdef.vrect.y) * glheight / vid.height;
	y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight / vid.height;

	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < glwidth)
		x2++;
	if (y2 < 0)
		y2--;
	if (y < glheight)
		y++; 

	w = x2 - x;
	h = y - y2;

	if (CURRVIEW && cl_multiview.value && cls.mvdplayback)
		R_SetViewports(glx,x,gly,y2,w,h,cl_multiview.value);

	if (!cl_multiview.value || !cls.mvdplayback)
		glViewport (glx + x, gly + y2, w, h);

    screenaspect = (float)r_refdef.vrect.width/r_refdef.vrect.height;
	farclip = max((int) r_farclip.value, 4096);
    MYgluPerspective (r_refdef.fov_y, screenaspect, 4, farclip);

	glCullFace(GL_FRONT);

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();

    glRotatef (-90, 1, 0, 0);	    // put Z going up
    glRotatef (90,  0, 0, 1);	    // put Z going up
    glRotatef (-r_refdef.viewangles[2], 1, 0, 0);
    glRotatef (-r_refdef.viewangles[0], 0, 1, 0);
    glRotatef (-r_refdef.viewangles[1], 0, 0, 1);
    glTranslatef (-r_refdef.vieworg[0], -r_refdef.vieworg[1], -r_refdef.vieworg[2]);

	glGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);

	// set drawing parms
	if (gl_cull.value)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);

	if (cl_multiview.value && cls.mvdplayback) {
		glClear (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 1;
		glDepthFunc (GL_LEQUAL);
	}

	glDepthRange (gldepthmin, gldepthmax); 

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);

	glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glHint (GL_FOG_HINT,GL_NICEST);

	glEnable(GL_DEPTH_TEST);
}

void CI_Init (void);

void R_Init (void) {
	Cmd_AddCommand ("loadsky", R_LoadSky_f);
	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f);
#ifndef CLIENTONLY
	Cmd_AddCommand ("pointfile", R_ReadPointFile_f);
#endif

	Cmd_AddCommand ("model_color", R_Model_Color_f);

	Cvar_SetCurrentGroup(CVAR_GROUP_PARTICLES);
	Cvar_Register (&gl_solidparticles);
	Cvar_Register (&gl_part_explosions);
	Cvar_Register (&gl_part_trails);
	Cvar_Register (&gl_part_spikes);
	Cvar_Register (&gl_part_gunshots);
	Cvar_Register (&gl_part_blood);
	Cvar_Register (&gl_part_telesplash);
	Cvar_Register (&gl_part_blobs);
	Cvar_Register (&gl_part_lavasplash);
	Cvar_Register (&gl_part_inferno);

	Cvar_SetCurrentGroup(CVAR_GROUP_TURB);
	Cvar_Register (&r_skyname);
	Cvar_Register (&r_fastsky);
	Cvar_Register (&r_skycolor);
	Cvar_Register (&r_fastturb);
	// START shaman RFE 1022504
	Cvar_Register (&r_telecolor);
	Cvar_Register (&r_lavacolor);
	Cvar_Register (&r_slimecolor);
	Cvar_Register (&r_watercolor);
	// END shaman RFE 1022504
	Cvar_Register (&r_novis);
	Cvar_Register (&r_wateralpha);
	Cvar_Register (&gl_caustics);
	if (!COM_CheckParm ("-nomtex")) {
		Cvar_Register (&gl_waterfog);
		Cvar_Register (&gl_waterfog_density);
	}

	Cvar_Register (&gl_fogenable); 
	Cvar_Register (&gl_fogstart); 
	Cvar_Register (&gl_fogend); 
	Cvar_Register (&gl_fogsky);
	Cvar_Register (&gl_fogred); 
	Cvar_Register (&gl_fogblue);
	Cvar_Register (&gl_foggreen);

	Cvar_SetCurrentGroup(CVAR_GROUP_EYECANDY);
	Cvar_Register (&r_drawentities);
	Cvar_Register (&r_lerpframes);
	Cvar_Register (&r_lerpmuzzlehack);
	Cvar_Register (&r_drawflame);
	Cvar_Register (&gl_detail);

	Cvar_SetCurrentGroup(CVAR_GROUP_BLEND);
	Cvar_Register (&gl_polyblend);

	Cvar_SetCurrentGroup(CVAR_GROUP_SKIN);
	Cvar_Register (&r_fullbrightSkins);

	Cvar_SetCurrentGroup(CVAR_GROUP_LIGHTING);
	Cvar_Register (&r_dynamic);
	Cvar_Register (&gl_fb_bmodels);
	Cvar_Register (&gl_fb_models);
	Cvar_Register (&gl_lightmode);
	Cvar_Register (&gl_flashblend);
	Cvar_Register (&r_shadows);
	Cvar_Register (&r_fullbright);
	Cvar_Register (&r_lightmap);
	Cvar_Register (&gl_shaftlight);
	Cvar_Register (&gl_loadlitfiles);
	Cvar_Register (&gl_colorlights);

	Cvar_SetCurrentGroup(CVAR_GROUP_TEXTURES);
	Cvar_Register (&gl_playermip);
	Cvar_Register (&gl_subdivide_size);
	Cvar_Register (&gl_lumaTextures);
	Cvar_Register (&r_drawflat);
	Cvar_Register (&r_wallcolor);
	Cvar_Register (&r_floorcolor);

	Cvar_Register (&model_color_backpack);
	Cvar_Register (&model_color_backpack_enable);
	Cvar_Register (&model_color_enemy);
	Cvar_Register (&model_color_enemy_enable);
	Cvar_Register (&model_color_team);
	Cvar_Register (&model_color_team_enable);
	Cvar_Register (&model_color_world);
	Cvar_Register (&model_color_world_enable);
	Cvar_Register (&model_color_missile);
	Cvar_Register (&model_color_missile_enable);
	Cvar_Register (&model_color_armor);
	Cvar_Register (&model_color_armor_enable);
	Cvar_Register (&model_color_quad);
	Cvar_Register (&model_color_quad_enable);
	Cvar_Register (&model_color_pent);
	Cvar_Register (&model_color_pent_enable);
	Cvar_Register (&model_color_gren);
	Cvar_Register (&model_color_gren_enable);
	Cvar_Register (&model_color_spike);
	Cvar_Register (&model_color_spike_enable);
	Cvar_Register (&model_color_v_shot);
	Cvar_Register (&model_color_v_shot_enable);
	Cvar_Register (&model_color_bolt);
	Cvar_Register (&model_color_bolt_enable);
	Cvar_Register (&model_color_bolt2);
	Cvar_Register (&model_color_bolt2_enable);
	Cvar_Register (&model_color_bolt3);
	Cvar_Register (&model_color_bolt3_enable);
	Cvar_Register (&model_color_s_spike);
	Cvar_Register (&model_color_s_spike_enable);
	Cvar_Register (&model_color_ring);
	Cvar_Register (&model_color_suit);	
	Cvar_Register (&model_color_rl);		
	Cvar_Register (&model_color_lg);		
	Cvar_Register (&model_color_mega);	
	Cvar_Register (&model_color_ssg);		
	Cvar_Register (&model_color_ng);		
	Cvar_Register (&model_color_sng);		
	Cvar_Register (&model_color_gl);		
	Cvar_Register (&model_color_gib1);	
	Cvar_Register (&model_color_gib2);	
	Cvar_Register (&model_color_gib3);	
	Cvar_Register (&model_color_health25);	
	Cvar_Register (&model_color_health10);	
	Cvar_Register (&model_color_rocks);	
	Cvar_Register (&model_color_rocks1);	
	Cvar_Register (&model_color_cells);	
	Cvar_Register (&model_color_cells1);	
	Cvar_Register (&model_color_nails);	
	Cvar_Register (&model_color_nails1);	
	Cvar_Register (&model_color_shells);	
	Cvar_Register (&model_color_shells1);
	Cvar_Register (&model_color_ring_enable);
	Cvar_Register (&model_color_suit_enable);	
	Cvar_Register (&model_color_rl_enable);		
	Cvar_Register (&model_color_lg_enable);		
	Cvar_Register (&model_color_mega_enable);	
	Cvar_Register (&model_color_ssg_enable);		
	Cvar_Register (&model_color_ng_enable);		
	Cvar_Register (&model_color_sng_enable);		
	Cvar_Register (&model_color_gl_enable);		
	Cvar_Register (&model_color_gib1_enable);	
	Cvar_Register (&model_color_gib2_enable);	
	Cvar_Register (&model_color_gib3_enable);	
	Cvar_Register (&model_color_health25_enable);	
	Cvar_Register (&model_color_health10_enable);	
	Cvar_Register (&model_color_rocks_enable);	
	Cvar_Register (&model_color_rocks1_enable);	
	Cvar_Register (&model_color_cells_enable);	
	Cvar_Register (&model_color_cells1_enable);	
	Cvar_Register (&model_color_nails_enable);	
	Cvar_Register (&model_color_nails1_enable);	
	Cvar_Register (&model_color_shells_enable);	
	Cvar_Register (&model_color_shells1_enable);	

	Cvar_Register (&colored_model_randomc);

	Cvar_SetCurrentGroup(CVAR_GROUP_OPENGL);
	Cvar_Register (&r_farclip);
	Cvar_Register (&gl_smoothmodels);
	Cvar_Register (&gl_affinemodels);
	Cvar_Register (&gl_clear);
	Cvar_Register (&gl_clearColor);
	Cvar_Register (&gl_cull);

	Cvar_Register (&gl_ztrick);

	Cvar_Register (&gl_nocolors);
	Cvar_Register (&gl_finish);

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register (&r_speeds);
	Cvar_Register (&r_netgraph);
	Cvar_Register (&r_netstats);

	Cvar_Register(&cl_multiview);
	Cvar_Register(&cl_mvdisplayhud);
	Cvar_Register(&cl_mvinset);
	Cvar_Register(&cl_mvinsetcrosshair);
	Cvar_Register(&cl_mvinsethud);

	Cvar_ResetCurrentGroup();

    hud_netgraph = HUD_Register("netgraph", /*"r_netgraph"*/ NULL, "Shows your network conditions in graph-form. With netgraph you can monitor your latency (ping), packet loss and network errors.",
        HUD_PLUSMINUS | HUD_ON_SCORES, ca_onserver, 0, SCR_HUD_Netgraph,
        "0", "top", "left", "bottom", "0", "0", "0", "0 0 0", 
        "swap_x",       "0",
        "swap_y",       "0",
        "inframes",     "0",
        "scale",        "256",
        "ploss",        "1",
        "width",        "256",
        "height",       "32",
        "lostscale",    "1",
        "full",         "0",
        "alpha",        "1",
        NULL);

	// this minigl driver seems to slow us down if the particles are drawn WITHOUT Z buffer bits 
	if (!strcmp(gl_vendor, "METABYTE/WICKED3D")) 
		Cvar_SetDefault(&gl_solidparticles, 1); 

	if (!gl_allow_ztrick)
		Cvar_SetDefault(&gl_ztrick, 0); 

	R_InitTextures ();
	R_InitBubble ();
	R_InitParticles ();
	CI_Init ();

	//VULT STUFF
	if (qmb_initialized)
		InitVXStuff();

	netgraphtexture = texture_extension_number;
	texture_extension_number++;

	playertextures = texture_extension_number;
	texture_extension_number += MAX_CLIENTS;

	// fullbright skins
	texture_extension_number += MAX_CLIENTS;
	skyboxtextures = texture_extension_number;
	texture_extension_number += 6;

	R_InitOtherTextures ();		
}


extern msurface_t	*alphachain;
void R_RenderScene (void) {

	vec3_t		colors;

	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	R_MarkLeaves ();	// done here so we know if we're in water

	R_DrawWorld ();		// adds static entities to the list

	S_ExtraUpdate ();	// don't let sound get messed up if going slow

	R_DrawEntitiesOnList (&cl_visents);
	R_DrawEntitiesOnList (&cl_alphaents);

	R_DrawWaterSurfaces ();

	GL_DisableMultitexture();

	// START shaman BUG fog was out of control when fogstart>fogend {
	if (gl_fogenable.value && gl_fogstart.value >= 0 && gl_fogstart.value < gl_fogend.value)	// } END shaman BUG fog was out of control when fogstart>fogend
	{
		glFogi(GL_FOG_MODE, GL_LINEAR);
			colors[0] = gl_fogred.value;
			colors[1] = gl_foggreen.value;
			colors[2] = gl_fogblue.value; 
		glFogfv(GL_FOG_COLOR, colors); 
		glFogf(GL_FOG_START, gl_fogstart.value); 
		glFogf(GL_FOG_END, gl_fogend.value); 
		glEnable(GL_FOG);
	}
	else
		glDisable(GL_FOG);

}

int gl_ztrickframe = 0;

qbool OnChange_gl_clearColor(cvar_t *v, char *s) {
	byte *clearColor;

	clearColor = StringToRGB(s);
	glClearColor (clearColor[0] / 255.0, clearColor[1] / 255.0, clearColor[2] / 255.0, 1.0);

	return false;
}

void R_Clear (void) {
	int clearbits = 0;

	if (gl_clear.value || (!vid_hwgamma_enabled && v_contrast.value > 1))
		clearbits |= GL_COLOR_BUFFER_BIT;

	if (gl_clear.value)
	{
		if (gl_fogenable.value)
			glClearColor(gl_fogred.value,gl_foggreen.value,gl_fogblue.value,0.5);//Tei custom clear color
		else
			glClearColor(0,0,0,1);
	}

	if (gl_ztrick.value) {
		if (clearbits)
			glClear (clearbits);

		gl_ztrickframe = !gl_ztrickframe;
		if (gl_ztrickframe) {
			gldepthmin = 0;
			gldepthmax = 0.49999;
			glDepthFunc (GL_LEQUAL);
		} else {
			gldepthmin = 1;
			gldepthmax = 0.5;
			glDepthFunc (GL_GEQUAL);
		}
	} else {
		clearbits |= GL_DEPTH_BUFFER_BIT;
		glClear (clearbits);
		gldepthmin = 0;
		gldepthmax = 1;
		glDepthFunc (GL_LEQUAL);
	}

	glDepthRange (gldepthmin, gldepthmax);
}

void DrawCI (void);

void R_RenderView (void) {
	double time1 = 0, time2;

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	if (r_speeds.value) {
		glFinish ();
		time1 = Sys_DoubleTime ();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	if (gl_finish.value)
		glFinish ();

	R_Clear ();

	// render normal view
	R_RenderScene ();
	R_RenderDlights ();
	R_DrawParticles ();

	DrawCI ();

	//VULT: CORONAS
	//Even if coronas gets turned off, let active ones fade out
	if (amf_coronas.value || CoronaCount)
		R_DrawCoronas();

	R_DrawViewModel ();

	if (r_speeds.value) {
		time2 = Sys_DoubleTime ();
		Print_flags[Print_current] |= PR_TR_SKIP;
		Com_Printf ("%3i ms  %4i wpoly %4i epoly\n", (int)((time2 - time1) * 1000), c_brush_polys, c_alias_polys); 
	}
}
