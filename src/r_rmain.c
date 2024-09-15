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
#include "gl_model.h"
#include "vx_stuff.h"
#include "vx_vertexlights.h"
#include "utils.h"
#include "qsound.h"
#include "hud.h"
#include "hud_common.h"
#include "rulesets.h"
#include "teamplay.h"
#include "r_sprite3d.h"
#include "r_performance.h"
#include "r_chaticons.h"
#include "r_matrix.h"
#include "r_local.h"
#include "r_state.h"
#include "r_brushmodel_sky.h"
#include "r_brushmodel.h"
#include "r_lighting.h"
#include "r_buffers.h"
#include "r_draw.h"
#include "r_aliasmodel.h"
#include "r_lightmaps.h"
#include "r_trace.h"
#include "r_renderer.h"

#define R_FogAvailable() (!COM_CheckParm(cmdline_param_client_nomultitexturing))

void GLM_ScreenDrawStart(void);

// Move to API
void GLM_SetupGL(void);
void GLC_SetupGL(void);
void GLM_PreRenderView(void);
void GLC_PreRenderView(void);
void GLM_DrawSpriteModel(entity_t *e);
void GLC_DrawSpriteModel(entity_t *e);
void GLM_PolyBlend(float v_blend[4]);
void GLC_PolyBlend(float v_blend[4]);
void GLM_InitialiseAliasModelBatches(void);

void R_TimeRefresh_f(void);
static void R_DrawEntities(void);
void R_InitOtherTextures(void);
void R_DrawViewModel(void);

void OnChange_gl_clearColor(cvar_t *v, char *s, qbool *cancel);
void SCR_OnChangeMVHudPos(cvar_t *var, char *newval, qbool *cancel);
void OnChange_r_drawflat(cvar_t *v, char *skyname, qbool *cancel);
void OnChange_r_skyname(cvar_t *v, char *s, qbool *cancel);
void R_MarkLeaves(void);
void R_InitBubble(void);
void R_InitAliasModelCvars(void);
void R_CreateWorldTextureChains(void);
static void R_SetupGL(void);

void GLM_RenderView(void);

void SCR_SetupDamageIndicators(void);

extern msurface_t *alphachain;

texture_t *r_notexture_mip = NULL;
refdef2_t r_refdef2;                          // screen size info
refdef_t  r_refdef;                           // screen size info
entity_t  r_worldentity;
mplane_t  frustum[4];
mleaf_t   *r_viewleaf;
mleaf_t   *r_oldviewleaf;
mleaf_t   *r_viewleaf2;                       // for watervis hack
mleaf_t   *r_oldviewleaf2;                    // for watervis hack
vec3_t    vup, vpn, vright;                   // view origin
vec3_t    vup_noroll, vright_noroll;          // view directions, with roll=0
vec3_t    r_origin; // view origin
float     clearColor[3] = {0, 0, 0};
int       r_visframecount;                    // bumped when going to a new PVS
int       r_framecount;                       // used for dlight push checking
int       lightmode = 2;
unsigned int d_lightstylevalue[256];             // 8.8 fraction of base light value
texture_ref underwatertexture, detailtexture, solidwhite_texture, solidblack_texture, transparent_texture;

void OnSquareParticleChange(cvar_t *var, char *value, qbool *cancel)
{
	Cvar_SetIgnoreCallback(var, value);

	Classic_InitParticles();
}

static void OnDynamicLightingChange(cvar_t* var, char* value, qbool* cancel)
{
	if (R_UseImmediateOpenGL() && atoi(value) > 1) {
		Con_Printf("Hardware lighting not supported when not using GLSL\n");
		*cancel = true;
		return;
	}
}

static void OnRemoveCollinearVerticesChanged(cvar_t* var, char* value, qbool* cancel)
{
#if defined(__APPLE__) && defined(__aarch64__)
	// At least arm64 based MacBook laptops are known to require this workaround.
	Cvar_SetIgnoreCallback(var, "1");
	*cancel = true;
#endif
}

cvar_t cl_multiview                        = {"cl_multiview", "0" };
cvar_t cl_mvdisplayhud                     = {"cl_mvdisplayhud", "1"};
cvar_t cl_mvhudvertical                    = {"cl_mvhudvertical", "0"};
cvar_t cl_mvhudflip                        = {"cl_mvhudflip", "0"};
cvar_t cl_mvhudpos                         = {"cl_mvhudpos", "bottom center"};
cvar_t cl_mvinset                          = {"cl_mvinset", "0"};
cvar_t cl_mvinsetcrosshair                 = {"cl_mvinsetcrosshair", "1"};
cvar_t cl_mvinsethud                       = {"cl_mvinsethud", "1"};
cvar_t cl_mvinset_offset_x                 = {"cl_mvinset_offset_x", "0"};
cvar_t cl_mvinset_offset_y                 = {"cl_mvinset_offset_y", "0"};
cvar_t cl_mvinset_size_x                   = {"cl_mvinset_size_x", "0.333"};
cvar_t cl_mvinset_size_y                   = {"cl_mvinset_size_y", "0.333"};
cvar_t cl_mvinset_top                      = {"cl_mvinset_top", "1"};
cvar_t cl_mvinset_right                    = {"cl_mvinset_right", "1"};

cvar_t r_drawentities                      = {"r_drawentities", "1"};
cvar_t r_drawworld                         = {"r_drawworld", "1"};
cvar_t r_lerpframes                        = {"r_lerpframes", "1"};
cvar_t r_drawflame                         = {"r_drawflame", "1"};
cvar_t r_drawdisc                          = {"r_drawdisc", "1"};
cvar_t r_speeds                            = {"r_speeds", "0"};
cvar_t r_fullbright                        = {"r_fullbright", "0"};
cvar_t r_shadows                           = {"r_shadows", "0"};
cvar_t r_wateralpha                        = {"gl_turbalpha", "1"};
#if defined(EZ_MULTIPLE_RENDERERS) || defined(RENDERER_OPTION_MODERN_OPENGL)
cvar_t r_dynamic                           = {"r_dynamic", "2", 0, OnDynamicLightingChange };
#else
cvar_t r_dynamic                           = {"r_dynamic", "1", 0, OnDynamicLightingChange };
#endif
cvar_t r_novis                             = {"r_novis", "0"};
cvar_t r_netgraph                          = {"r_netgraph", "0"};
cvar_t r_netstats                          = {"r_netstats", "0"};
cvar_t r_fastsky                           = {"r_fastsky", "0"};
cvar_t r_fastturb                          = {"r_fastturb", "0"};
cvar_t r_skycolor                          = {"r_skycolor", "40 80 150", CVAR_COLOR};
cvar_t r_telecolor                         = {"r_telecolor", "255 60 60", CVAR_COLOR};
cvar_t r_lavacolor                         = {"r_lavacolor", "80 0 0", CVAR_COLOR};
cvar_t r_slimecolor                        = {"r_slimecolor", "10 60 10", CVAR_COLOR};
cvar_t r_watercolor                        = {"r_watercolor", "10 50 80", CVAR_COLOR};
cvar_t r_drawflat                          = {"r_drawflat", "0", 0, OnChange_r_drawflat};
cvar_t r_drawflat_mode                     = {"r_drawflat_mode", "0", 0, OnChange_r_drawflat};
cvar_t r_wallcolor                         = {"r_wallcolor", "255 255 255", CVAR_COLOR, OnChange_r_drawflat};
cvar_t r_floorcolor                        = {"r_floorcolor", "50 100 150", CVAR_COLOR, OnChange_r_drawflat};
cvar_t gl_textureless                      = {"gl_textureless", "0", 0, OnChange_r_drawflat}; //Qrack
cvar_t r_farclip                           = {"r_farclip", "8192", CVAR_RULESET_MAX | CVAR_RULESET_MIN, NULL, 8192.0f, R_MAXIMUM_FARCLIP, R_MINIMUM_FARCLIP }; // previous default was 4096. 8192 helps some TF players in big maps
cvar_t r_skyname                           = {"r_skyname", "", 0, OnChange_r_skyname};
cvar_t gl_detail                           = {"gl_detail","0"};
cvar_t gl_brush_polygonoffset              = {"gl_brush_polygonoffset", "2.0"}; // This is the one to adjust if you notice flicker on lift @ e1m1 for instance, for z-fighting
cvar_t gl_brush_polygonoffset_factor       = {"gl_brush_polygonoffset_factor", "0.05"};
cvar_t gl_caustics                         = {"gl_caustics", "0"}; // 1
cvar_t gl_lumatextures                     = {"gl_lumatextures", "1"};
cvar_t gl_subdivide_size                   = {"gl_subdivide_size", "64"};
cvar_t gl_clear                            = {"gl_clear", "0"};
cvar_t gl_clearColor                       = {"gl_clearColor", "0 0 0", CVAR_COLOR, OnChange_gl_clearColor};
cvar_t gl_polyblend                        = {"gl_polyblend", "1"}; // 0
cvar_t gl_flashblend                       = {"gl_flashblend", "0"};
cvar_t gl_rl_globe                         = {"gl_rl_globe", "0"};
cvar_t gl_playermip                        = {"gl_playermip", "0"};
cvar_t gl_nocolors                         = {"gl_nocolors", "0"};
cvar_t gl_finish                           = {"gl_finish", "0"};
cvar_t gl_fb_bmodels                       = {"gl_fb_bmodels", "1"};
cvar_t gl_fb_models                        = {"gl_fb_models", "1"};
cvar_t gl_lightmode                        = {"gl_lightmode", "1"};
cvar_t gl_loadlitfiles                     = {"gl_loadlitfiles", "1"};
cvar_t gl_oldlitscaling                    = {"gl_oldlitscaling", "0"};
cvar_t gl_colorlights                      = {"gl_colorlights", "1"};
cvar_t gl_squareparticles                  = {"gl_squareparticles", "0", 0, OnSquareParticleChange};
cvar_t gl_part_explosions                  = {"gl_part_explosions", "0"}; // 1
cvar_t gl_part_bloodtrails                 = {"gl_part_bloodtrails", "1"}; // 1 was the default behaviour before this became a cvar
cvar_t gl_part_trails                      = {"gl_part_trails", "0"}; // 1
cvar_t gl_part_tracer1_color               = {"gl_part_tracer1_color", "0 124 0", CVAR_COLOR};
cvar_t gl_part_tracer2_color               = {"gl_part_tracer2_color", "255 77 0", CVAR_COLOR};
cvar_t gl_part_tracer1_size                = {"gl_part_tracer1_size", "3.75", CVAR_RULESET_MAX | CVAR_RULESET_MIN, NULL, 3.75f, 10.f, 0.f};
cvar_t gl_part_tracer1_time                = {"gl_part_tracer1_time", "0.5", CVAR_RULESET_MAX | CVAR_RULESET_MIN, NULL, 0.5f, 3.f, 0.f};
cvar_t gl_part_tracer2_size                = {"gl_part_tracer2_size", "3.75", CVAR_RULESET_MAX | CVAR_RULESET_MIN, NULL, 3.75f, 10.f, 0.f};
cvar_t gl_part_tracer2_time                = {"gl_part_tracer2_time", "0.5", CVAR_RULESET_MAX | CVAR_RULESET_MIN, NULL, 0.5f, 3.f, 0.f};
cvar_t gl_part_spikes                      = {"gl_part_spikes", "0"}; // 1
cvar_t gl_part_gunshots                    = {"gl_part_gunshots", "0"}; // 1
cvar_t gl_part_blood                       = {"gl_part_blood", "0"}; // 1
cvar_t gl_part_telesplash                  = {"gl_part_telesplash", "0"}; // 1
cvar_t gl_part_blobs                       = {"gl_part_blobs", "0"}; // 1
cvar_t gl_part_lavasplash                  = {"gl_part_lavasplash", "0"}; // 1
cvar_t gl_part_inferno                     = {"gl_part_inferno", "0"}; // 1
cvar_t gl_part_bubble                      = {"gl_part_bubble", "1"}; // would prefer 0 but default was 1
cvar_t gl_part_detpackexplosion_fire_color = {"gl_part_detpackexplosion_fire_color", "", CVAR_COLOR};
cvar_t gl_part_detpackexplosion_ray_color  = {"gl_part_detpackexplosion_ray_color", "", CVAR_COLOR};
cvar_t gl_powerupshells                    = {"gl_powerupshells", "1"};

// 0: off, 1: on, 2: in water only
cvar_t r_fx_fog                            = {"r_fx_fog", "0"};
// 0: off, 1: dm & single-player, 2: single-player only
cvar_t r_fx_fog_usemap                     = {"r_fx_fog_usemap", "2"};
cvar_t r_fx_fog_model                      = {"r_fx_fog_model", "2"};
cvar_t r_fx_fog_density                    = {"r_fx_fog_density", "0.125"};
cvar_t r_fx_fog_start                      = {"r_fx_fog_start", "50.0"};
cvar_t r_fx_fog_end                        = {"r_fx_fog_end", "800.0"};
cvar_t r_fx_fog_color_air                  = {"r_fx_fog_color_air", "153 128 103", CVAR_COLOR};
cvar_t r_fx_fog_color_water                = {"r_fx_fog_color_water", "32 64 128", CVAR_COLOR};
cvar_t r_fx_fog_color_lava                 = {"r_fx_fog_color_lava", "255 64 0", CVAR_COLOR};
cvar_t r_fx_fog_color_slime                = {"r_fx_fog_color_slime", "128 255 0", CVAR_COLOR};
cvar_t r_fx_fog_sky                        = {"r_fx_fog_sky", "0.3"};

cvar_t gl_simpleitems                      = {"gl_simpleitems", "0"};
cvar_t gl_simpleitems_size                 = {"gl_simpleitems_size", "16"};
cvar_t gl_simpleitems_orientation          = {"gl_simpleitems_orientation", "2"};
cvar_t gl_modulate                         = {"gl_modulate", "1"};

cvar_t gl_outline                          = {"gl_outline", "0"};
cvar_t gl_outline_color_world              = {"gl_outline_color_world", "0 0 0"};
cvar_t gl_outline_color_model              = {"gl_outline_color_model", "0 0 0"};
cvar_t gl_outline_scale_world              = {"gl_outline_scale_world", "1"};
cvar_t gl_outline_scale_model              = {"gl_outline_scale_model", "1"};
cvar_t gl_outline_world_depth_threshold    = {"gl_outline_world_depth_threshold", "4"};
cvar_t gl_outline_world_normal_threshold   = {"gl_outline_world_normal_threshold", "0.997"};
cvar_t gl_outline_use_player_color         = {"gl_outline_use_player_color", "0"};
cvar_t gl_spec_xray                        = {"gl_spec_xray", "0"};
cvar_t gl_spec_xray_distance               = {"gl_spec_xray_distance", "1500"};
cvar_t gl_outline_color_team               = {"gl_outline_color_team", ""};
cvar_t gl_outline_color_enemy              = {"gl_outline_color_enemy", ""};

cvar_t gl_smoothmodels                     = {"gl_smoothmodels", "1"};

cvar_t gl_vbo_clientmemory                 = {"gl_vbo_clientmemory", "0", CVAR_LATCH_GFX };

cvar_t r_remove_collinear_vertices         = {"r_remove_collinear_vertices", "0", 0, OnRemoveCollinearVerticesChanged};

//Returns true if the box is completely outside the frustom
qbool R_CullBox(vec3_t mins, vec3_t maxs)
{
	int i;

	for (i = 0; i < 4; i++) {
		if (BOX_ON_PLANE_SIDE(mins, maxs, &frustum[i]) == 2) {
			return true;
		}
	}
	return false;
}

//Returns true if the sphere is completely outside the frustum
qbool R_CullSphere(vec3_t centre, float radius)
{
	int i;
	mplane_t *p;

	for (i = 0, p = frustum; i < 4; i++, p++) {
		if (PlaneDiff(centre, p) <= -radius) {
			return true;
		}
	}

	return false;
}

static int SignbitsForPlane(mplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test
	bits = 0;
	for (j = 0; j < 3; j++) {
		if (out->normal[j] < 0) {
			bits |= 1 << j;
		}
	}
	return bits;
}

void R_SetFrustum(void)
{
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

static void R_ConfigureFog(int contents)
{
	int i;
	qbool use_map_fog;

	// enable it even if not showing this frame, to prevent potential program re-compiles when going in/out of water
	if (!R_FogAvailable()) {
		r_refdef2.fog_enabled = r_refdef2.fog_render = false;
		return;
	}

	use_map_fog = cl.map_fog_enabled && (r_fx_fog_usemap.integer == 1 || (r_fx_fog_usemap.integer == 2 && Ruleset_IsLocalSinglePlayerGame()));
	r_refdef2.fog_enabled = (use_map_fog || r_fx_fog.integer == 1 || r_fx_fog.integer == 2);
	r_refdef2.fog_render = (use_map_fog || r_fx_fog.integer == 1 || (r_fx_fog.integer == 2 && ISUNDERWATER(contents)));
	r_refdef2.fog_density = 0;
	r_refdef2.fog_calculation = fogcalc_none;

	if (r_refdef2.fog_render) {
		if (use_map_fog) {
			r_refdef2.fog_density = bound(0, cl.map_fog_density * cl.map_fog_density, 1);
			r_refdef2.fog_calculation = fogcalc_exp2;
		}
		else {
			r_refdef2.fog_density = bound(0, r_fx_fog_density.value / 64.0f, 1);

			if (r_fx_fog_model.integer == 0) {
				r_refdef2.fog_calculation = fogcalc_linear;
			}
			else if (r_fx_fog_model.integer == 1) {
				r_refdef2.fog_calculation = fogcalc_exp;
			}
			else if (r_fx_fog_model.integer == 2) {
				r_refdef2.fog_calculation = fogcalc_exp2;
				r_refdef2.fog_density = r_refdef2.fog_density * r_refdef2.fog_density;
			}
		}

		if (use_map_fog && cl.map_fog_sky >= 0) {
			r_refdef2.fog_sky = cl.map_fog_sky;
		}
		else {
			r_refdef2.fog_sky = r_refdef2.fog_density != 0 ? bound(0, r_fx_fog_sky.value, 1) : 0;
		}
	}
	r_refdef2.fog_linear_start = r_fx_fog_start.value;
	r_refdef2.fog_linear_end = r_fx_fog_end.value;

	if (contents == CONTENTS_LAVA) {
		VectorScale(r_fx_fog_color_lava.color, 1 / 255.0f, r_refdef2.fog_color);
	}
	else if (contents == CONTENTS_SLIME) {
		VectorScale(r_fx_fog_color_slime.color, 1 / 255.0f, r_refdef2.fog_color);
	}
	else if (contents == CONTENTS_WATER) {
		VectorScale(r_fx_fog_color_water.color, 1 / 255.0f, r_refdef2.fog_color);
	}
	else if (use_map_fog) {
		VectorCopy(cl.map_fog_color, r_refdef2.fog_color);
	}
	else {
		VectorScale(r_fx_fog_color_air.color, 1 / 255.0f, r_refdef2.fog_color);
	}
	for (i = 0; i < 3; ++i) {
		r_refdef2.fog_skycolor[i] = r_refdef2.fog_color[i] * r_refdef2.fog_sky + r_skycolor.color[i] * (1.0f - r_refdef2.fog_sky) / 255.0f;
	}
	r_refdef2.fog_color[3] = r_refdef2.fog_skycolor[3] = 1.0f;
}

void R_SetupFrame(void)
{
	vec3_t testorigin;
	mleaf_t	*leaf;

	R_AnimateLight ();

	r_framecount++;

	// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);
	AngleVectors (r_refdef.viewangles, vpn, vright, vup);
	if (r_refdef.viewangles[ROLL] != 0) {
		vec3_t noroll_angles = { r_refdef.viewangles[0], r_refdef.viewangles[1], 0 };

		AngleVectors(noroll_angles, NULL, vright_noroll, vup_noroll);
	}
	else {
		VectorCopy(vright, vright_noroll);
		VectorCopy(vup, vup_noroll);
	}

	// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_oldviewleaf2 = r_viewleaf2;

	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);
	r_viewleaf2 = NULL;

	// FIXME: might need to test falling out bottom of water as well?

	// check above and below so crossing solid water doesn't draw wrong
	if (r_viewleaf->contents <= CONTENTS_WATER && r_viewleaf->contents >= CONTENTS_LAVA) {
		// look up a bit
		VectorCopy (r_origin, testorigin);
		testorigin[2] += 10;
		leaf = Mod_PointInLeaf (testorigin, cl.worldmodel);
		if (leaf->contents == CONTENTS_EMPTY) {
			r_viewleaf2 = leaf;
		}
	}
	else if (r_viewleaf->contents == CONTENTS_EMPTY) {
		// FIXME: If we test down and find CONTENTS_SOLID then we should reduce viewheight_test and try again?

		// look down a bit
		VectorCopy(r_origin, testorigin);
		testorigin[2] -= r_refdef.viewheight_test;
		leaf = Mod_PointInLeaf(testorigin, cl.worldmodel);
		if (leaf->contents <= CONTENTS_WATER && leaf->contents >= CONTENTS_LAVA) {
			r_viewleaf2 = leaf;
		}
	}

	V_SetContentsColor(r_viewleaf->contents);
	R_ConfigureFog(r_viewleaf->contents);
	V_CalcBlend();

	R_LightmapFrameInit();
}

static void MYgluPerspective(double fovy, double aspect, double zNear, double zFar)
{
	double xmin, xmax, ymin, ymax;

	ymax = zNear * tan(fovy * M_PI / 360.0);
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	if (cl_multiview.value == 2 && !cl_mvinset.value && cls.mvdplayback) {
		R_Frustum(xmin, xmax, ymin + (ymax - ymin)*0.25, ymax - (ymax - ymin)*0.25, zNear, zFar);
	}
	else if (CL_MultiviewActiveViews() == 3) {
		if (CL_MultiviewCurrentView() == 2) {
			R_Frustum(xmin, xmax, ymin + (ymax - ymin)*0.25, ymax - (ymax - ymin)*0.25, zNear, zFar);
		}
		else {
			R_Frustum(xmin, xmax, ymin, ymax, zNear, zFar);
		}
	}
	else {
		R_Frustum(xmin, xmax, ymin, ymax, zNear, zFar);
	}
}

void R_SetViewports(int glx, int x, int gly, int y2, int w, int h, float max)
{
	//
	// Setup Multiview-viewports
	//
	if (max == 1) {
		R_Viewport(glx + x, gly + y2, w, h);
		return;
	}
	else if (max == 2 && cl_mvinset.value) {
		if (CL_MultiviewCurrentView() == 2) {
			R_Viewport(glx + x, gly + y2, w, h);
		}
		else if (CL_MultiviewCurrentView() == 1) {
			int height = cl_sbar.integer ? h : glheight;
			int inset_left = glx + x + (cl_mvinset_right.integer ? glwidth - cl_mvinset_size_x.value * glwidth : 0) + cl_mvinset_offset_x.value;
			int inset_top = gly + y2 + (cl_mvinset_top.integer ? height - cl_mvinset_size_y.value * height : 0) - cl_mvinset_offset_y.value;
			int inset_width = w * cl_mvinset_size_x.value;
			int inset_height = h * cl_mvinset_size_y.value;

			CL_MultiviewInsetSetScreenCoordinates(inset_left, inset_top, inset_width, inset_height);
			R_Viewport(inset_left, inset_top, inset_width, inset_height);
		}
		else {
			Com_Printf("ERROR!\n");
		}
		return;
	}
	else if (max == 2 && !cl_mvinset.value) {
		if (CL_MultiviewCurrentView() == 2) {
			R_Viewport(0, h / 2, w, h / 2);
		}
		else if (CL_MultiviewCurrentView() == 1) {
			R_Viewport(0, 0, w, h / 2 - 1);
		}
		else {
			Com_Printf("ERROR!\n");
		}
		return;
	}
	else if (max == 3) {
		if (CL_MultiviewCurrentView() == 2) {
			R_Viewport(0, h / 2, w, h / 2);
		}
		else if (CL_MultiviewCurrentView() == 3) {
			R_Viewport(0, 0, w / 2, h / 2 - 1);
		}
		else {
			R_Viewport(w / 2, 0, w / 2, h / 2 - 1);
		}
		return;
	}
	else {
		if (CL_MultiviewCurrentView() == 2) {
			R_Viewport(0, h / 2, w / 2, h / 2);
		}
		else if (CL_MultiviewCurrentView() == 3) {
			R_Viewport(w / 2, h / 2, w / 2, h / 2);
		}
		else if (CL_MultiviewCurrentView() == 4) {
			R_Viewport(0, 0, w / 2, h / 2 - 1);
		}
		else if (CL_MultiviewCurrentView() == 1) {
			R_Viewport(w / 2, 0, w / 2, h / 2 - 1);
		}
	}

	return;
}

float R_FarPlaneZ(void)
{
	return bound(R_MINIMUM_FARCLIP, r_farclip.value, R_MAXIMUM_FARCLIP);
}

float R_NearPlaneZ(void)
{
	extern cvar_t r_nearclip;

	return bound(R_MINIMUM_NEARCLIP, r_nearclip.value, R_MAXIMUM_NEARCLIP);
}

static void R_SetupViewport(void)
{
	float screenaspect;
	extern int glwidth, glheight;
	int x, x2, y2, y, w, h;

	// set up viewpoint
	x = r_refdef.vrect.x * glwidth / vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth / vid.width;
	y = (vid.height - r_refdef.vrect.y) * glheight / vid.height;
	y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight / vid.height;

	// fudge around because of frac screen scale
	if (x > 0) {
		x--;
	}
	if (x2 < glwidth) {
		x2++;
	}
	if (y2 < 0) {
		y2--;
	}
	if (y < glheight) {
		y++;
	}

	w = x2 - x;
	h = y - y2;

	// Multiview
	R_SetFullScreenViewport(glx + x, gly + y2, w, h);
	if (CL_MultiviewEnabled() && CL_MultiviewCurrentView() != 0) {
		R_SetViewports(glx, x, gly, y2, w, h, cl_multiview.value);
	}
	if (!CL_MultiviewEnabled()) {
		R_Viewport(glx + x, gly + y2, w, h);
	}

	screenaspect = (float)r_refdef.vrect.width / r_refdef.vrect.height;

	R_IdentityProjectionView();
	MYgluPerspective(r_refdef.fov_y, screenaspect, R_NearPlaneZ(), R_FarPlaneZ());
}

static void R_SetupGL(void)
{
	R_SetupViewport();

	R_StateDefault3D();

	renderer.SetupGL();
}

void R_Init(void)
{
	Cmd_AddCommand("loadsky", R_LoadSky_f);
	Cmd_AddCommand("timerefresh", R_TimeRefresh_f);
#ifndef CLIENTONLY
	Cmd_AddCommand("dev_pointfile", R_ReadPointFile_f);
#endif

#ifdef RENDERER_OPTION_CLASSIC_OPENGL
	{
		void GLC_BloomRegisterCvars(void);

		GLC_BloomRegisterCvars();
	}
#endif

	Cvar_SetCurrentGroup(CVAR_GROUP_EYECANDY);
	Cvar_Register(&r_drawentities);
	Cvar_Register(&r_drawworld);
	Cvar_Register(&r_lerpframes);
	Cvar_Register(&r_drawflame);
	Cvar_Register(&r_drawdisc);
	Cvar_Register(&gl_detail);
	Cvar_Register(&gl_powerupshells);

	Cvar_Register(&gl_simpleitems);
	Cvar_Register(&gl_simpleitems_size);
	Cvar_Register(&gl_simpleitems_orientation);

	Cvar_SetCurrentGroup(CVAR_GROUP_PARTICLES);
	Cvar_Register(&gl_part_explosions);
	Cvar_Register(&gl_part_bloodtrails);
	Cvar_Register(&gl_part_trails);
	Cvar_Register(&gl_part_tracer1_color);
	Cvar_Register(&gl_part_tracer1_size);
	Cvar_Register(&gl_part_tracer1_time);
	Cvar_Register(&gl_part_tracer2_color);
	Cvar_Register(&gl_part_tracer2_size);
	Cvar_Register(&gl_part_tracer2_time);
	Cvar_Register(&gl_part_spikes);
	Cvar_Register(&gl_part_gunshots);
	Cvar_Register(&gl_part_blood);
	Cvar_Register(&gl_part_telesplash);
	Cvar_Register(&gl_part_blobs);
	Cvar_Register(&gl_part_lavasplash);
	Cvar_Register(&gl_part_inferno);
	Cvar_Register(&gl_part_bubble);
	Cvar_Register(&gl_squareparticles);

	Cvar_Register(&gl_part_detpackexplosion_fire_color);
	Cvar_Register(&gl_part_detpackexplosion_ray_color);

	Cvar_SetCurrentGroup(CVAR_GROUP_TURB);
	Cvar_Register(&r_skyname);
	Cvar_Register(&r_fastsky);
	Cvar_Register(&r_skycolor);
	Cvar_Register(&r_fastturb);

	Cvar_Register(&r_telecolor);
	Cvar_Register(&r_lavacolor);
	Cvar_Register(&r_slimecolor);
	Cvar_Register(&r_watercolor);

	Cvar_Register(&r_novis);
	Cvar_Register(&r_wateralpha);
	Cvar_Register(&gl_caustics);

	// Fog cvars
	if (R_FogAvailable()) {
		Cvar_SetCurrentGroup(CVAR_GROUP_EYECANDY);
		Cvar_Register(&r_fx_fog);
		Cvar_Register(&r_fx_fog_model);
		Cvar_Register(&r_fx_fog_usemap);
		Cvar_Register(&r_fx_fog_density);
		Cvar_Register(&r_fx_fog_start);
		Cvar_Register(&r_fx_fog_end);
		Cvar_Register(&r_fx_fog_sky);
		Cvar_Register(&r_fx_fog_color_air);
		Cvar_Register(&r_fx_fog_color_water);
		Cvar_Register(&r_fx_fog_color_lava);
		Cvar_Register(&r_fx_fog_color_slime);
	}

	Cvar_SetCurrentGroup(CVAR_GROUP_BLEND);
	Cvar_Register(&gl_polyblend);

	R_InitAliasModelCvars();

	Cvar_SetCurrentGroup(CVAR_GROUP_LIGHTING);
	Cvar_Register(&r_dynamic);
	Cvar_Register(&gl_fb_bmodels);
	Cvar_Register(&gl_fb_models);
	Cvar_Register(&gl_lightmode);
	Cvar_Register(&gl_flashblend);
	Cvar_Register(&gl_rl_globe);
	Cvar_Register(&r_shadows);
	Cvar_Register(&r_fullbright);
	Cvar_Register(&gl_loadlitfiles);
	Cvar_Register(&gl_oldlitscaling);
	Cvar_Register(&gl_colorlights);

	Cvar_SetCurrentGroup(CVAR_GROUP_TEXTURES);
	Cvar_Register(&gl_playermip);
	Cvar_Register(&gl_subdivide_size);
	Cvar_Register(&gl_lumatextures);
	Cvar_Register(&r_drawflat);
	Cvar_Register(&r_drawflat_mode);
	Cvar_Register(&r_wallcolor);
	Cvar_Register(&r_floorcolor);
	Cvar_Register(&gl_textureless); //Qrack

	Cvar_SetCurrentGroup(CVAR_GROUP_OPENGL);
	Cvar_Register(&r_farclip);
	Cvar_Register(&gl_clear);
	Cvar_Register(&gl_clearColor);
	Cvar_Register(&gl_brush_polygonoffset);
	Cvar_Register(&gl_brush_polygonoffset_factor);

	Cvar_Register(&gl_nocolors);
	Cvar_Register(&gl_finish);
	Cvar_Register(&gl_modulate);

	Cvar_Register(&gl_outline);
	Cvar_Register(&gl_outline_color_world);
	Cvar_Register(&gl_outline_color_model);
	// Cvar_Register(&gl_outline_scale_world);
	Cvar_Register(&gl_outline_scale_model);
	Cvar_Register(&gl_outline_world_depth_threshold);
	Cvar_Register(&gl_outline_world_normal_threshold);
	Cvar_Register(&gl_outline_use_player_color);
	Cvar_Register(&gl_spec_xray);
	Cvar_Register(&gl_spec_xray_distance);
	Cvar_Register(&gl_outline_color_team);
	Cvar_Register(&gl_outline_color_enemy);
	Cvar_Register(&gl_smoothmodels);

	Cvar_Register(&gl_vbo_clientmemory);

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	if (IsDeveloperMode()) {
		Cvar_Register(&r_speeds);
	}
	Cvar_Register(&r_netgraph);
	Cvar_Register(&r_netstats);

	Cvar_Register(&cl_multiview);
	Cvar_Register(&cl_mvdisplayhud);
	Cvar_Register(&cl_mvhudvertical);
	Cvar_Register(&cl_mvhudflip);
	Cvar_Register(&cl_mvhudpos);
	cl_mvhudpos.OnChange = SCR_OnChangeMVHudPos;
	Cvar_Register(&cl_mvinset);
	Cvar_Register(&cl_mvinsetcrosshair);
	Cvar_Register(&cl_mvinsethud);
	Cvar_Register(&cl_mvinset_offset_x);
	Cvar_Register(&cl_mvinset_offset_y);
	Cvar_Register(&cl_mvinset_size_x);
	Cvar_Register(&cl_mvinset_size_y);
	Cvar_Register(&cl_mvinset_top);
	Cvar_Register(&cl_mvinset_right);

	Cvar_Register(&r_remove_collinear_vertices);

	Cvar_ResetCurrentGroup();

	if (!hud_netgraph) {
		hud_netgraph = HUD_Register(
			"netgraph", /*"r_netgraph"*/ NULL, "Shows your network conditions in graph-form. With netgraph you can monitor your latency (ping), packet loss and network errors.",
			HUD_PLUSMINUS | HUD_ON_SCORES, ca_onserver, 0, SCR_HUD_Netgraph,
			"0", "top", "left", "bottom", "0", "0", "0", "0 0 0", NULL,
			"swap_x", "0",
			"swap_y", "0",
			"inframes", "0",
			"scale", "256",
			"ploss", "1",
			"width", "256",
			"height", "32",
			"lostscale", "1",
			"alpha", "1",
			NULL
		);
	}

	R_InitTextures();	   // FIXME: not sure is this safe re-init
	R_InitBubble();	       // safe re-init
	R_InitParticles();     // safe re-init imo
	R_InitChatIcons();     // safe re-init

	//VULT STUFF
	InitVXStuff(); // safe re-init imo

	InitTracker();
	R_InitOtherTextures(); // safe re-init
	R_InitBloomTextures();
}

void OnChange_gl_clearColor(cvar_t *v, char *s, qbool *cancel) {
	byte *color;
	char buf[MAX_COM_TOKEN];

	strlcpy(buf,s,sizeof(buf));
	color = StringToRGB(buf);

	clearColor[0] = color[0] / 255.0;
	clearColor[1] = color[1] / 255.0;
	clearColor[2] = color[2] / 255.0;

	R_ClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0);
}

static void R_Clear(void)
{
	qbool clear_color = false;

	// This used to cause a bug with some graphics cards when
	// in multiview mode. It would clear all but the last
	// drawn views.
	clear_color |= (!cl_multiview.integer && (gl_clear.integer || (!vid_hwgamma_enabled && v_contrast.value > 1)));

	// If outside level or in wall, sky is usually visible
	clear_color |= (r_viewleaf->contents == CONTENTS_SOLID && (R_UseModernOpenGL() || r_fastsky.integer));

	if (gl_clear.integer) {
		//Tei custom clear color
		if (r_refdef2.fog_render) {
			R_ClearColor(r_refdef2.fog_color[0], r_refdef2.fog_color[1], r_refdef2.fog_color[2], 1.0);
		}
		else {
			R_ClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0);
		}
	}

	renderer.ClearRenderingSurface(clear_color);
}

void R_PostProcessScene(void)
{
	if (R_UseImmediateOpenGL()) {
		R_BloomBlend();
	}
}

static void R_Render3DEffects(void)
{
	// Adds particles (all types)
	R_DrawParticles();

	// Adds chat icons over player's heads (afk etc)
	R_DrawChatIcons();

	// Run corona logic
	R_DrawCoronas();
}

static void R_Render3DHud(void)
{
	// Draw the player's view model (gun)
	if (R_UseImmediateOpenGL()) {
		R_DrawViewModel();
	}

	// While still in 3D mode, calculate the location of labels to be printed in 2D
	SCR_SetupAutoID();
	SCR_SetupDamageIndicators();
}

void R_RenderView(void)
{
	if (!r_worldentity.model || !cl.worldmodel) {
		Sys_Error("R_RenderView: NULL worldmodel");
	}

	// Wait for previous commands to 'complete'
	if (!r_speeds.integer && gl_finish.integer) {
		renderer.EnsureFinished();
	}

	R_SetFrustum();
	R_SetupGL();
	R_Clear();
	R_MarkLeaves();	// done here so we know if we're in water
	R_CreateWorldTextureChains();

	// render normal view
	R_DrawWorld();		// adds static entities to the list

	if (r_refdef2.wateralpha == 1) {
		renderer.DrawWaterSurfaces();
	}

	R_DrawEntities();

	// Adds 3d effects (particles, lights, chat icons etc)
	R_Render3DEffects();

	// Draws transparent world surfaces
	renderer.DrawWaterSurfaces();

	// Render billboards
	renderer.Draw3DSpritesInline();

	// Draw 3D hud elements
	R_Render3DHud();

	renderer.RenderView();
}

qbool R_PointIsUnderwater(vec3_t point)
{
	int contents = TruePointContents(point);

	return ISUNDERWATER(contents);
}

qbool R_CanDrawSimpleItem(entity_t* e)
{
	int skin;

	if (!gl_simpleitems.integer || !e || !e->model) {
		return false;
	}

	skin = e->skinnum >= 0 && e->skinnum < MAX_SIMPLE_TEXTURES ? e->skinnum : 0;

	if (R_UseModernOpenGL()) {
		return R_TextureReferenceIsValid(e->model->simpletexture_array[skin]);
	}
	else {
		return R_TextureReferenceIsValid(e->model->simpletexture[skin]);
	}
}

static qbool R_DrawTrySimpleItem(entity_t* ent)
{
	int sprtype = gl_simpleitems_orientation.integer;
	float sprsize = bound(1, gl_simpleitems_size.value, 16), autorotate;
	texture_ref simpletexture;
	vec3_t right, up, org, offset;
	int skin;

	if (!ent || !ent->model) {
		return false;
	}

	skin = ent->skinnum >= 0 && ent->skinnum < MAX_SIMPLE_TEXTURES ? ent->skinnum : 0;
	if (R_UseModernOpenGL()) {
		simpletexture = ent->model->simpletexture_array[skin];
	}
	else {
		simpletexture = ent->model->simpletexture[skin];
	}
	if (!R_TextureReferenceIsValid(simpletexture)) {
		return false;
	}

	autorotate = anglemod(100 * cl.time);
	if (sprtype == SPR_ORIENTED) {
		// bullet marks on walls
		vec3_t angles;
		angles[0] = angles[2] = 0;
		angles[1] = anglemod(autorotate);
		AngleVectors(angles, NULL, right, up);
	}
	else if (sprtype == SPR_FACING_UPRIGHT) {
		VectorSet(up, 0, 0, 1);
		right[0] = ent->origin[1] - r_origin[1];
		right[1] = -(ent->origin[0] - r_origin[0]);
		right[2] = 0;
		VectorNormalizeFast(right);
	}
	else if (sprtype == SPR_VP_PARALLEL_UPRIGHT) {
		VectorSet(up, 0, 0, 1);
		VectorCopy(vright, right);
	}
	else {
		// normal sprite
		VectorCopy(vup_noroll, up);
		VectorCopy(vright_noroll, right);
	}

	VectorCopy(ent->origin, org);
	// brush models require some additional centering
	if (ent->model->type == mod_brush) {
		extern cvar_t cl_model_bobbing;

		VectorSubtract(ent->model->maxs, ent->model->mins, offset);
		offset[2] = 0;
		VectorMA(org, 0.5, offset, org);

		if (cl_model_bobbing.value) {
			org[2] += sin(autorotate / 90 * M_PI) * 5 + 5;
		}
	}
	org[2] += sprsize;

	renderer.DrawSimpleItem(ent->model, skin, org, sprsize, up, right);
	return true;
}

static int R_DrawEntitiesSorter(const void* lhs_, const void* rhs_)
{
	const visentity_t* lhs = (const visentity_t*)lhs_;
	const visentity_t* rhs = (const visentity_t*)rhs_;

	float alpha_lhs = lhs->type == mod_sprite ? 0.5 : lhs->ent.alpha == 0 ? 1 : lhs->ent.alpha;
	float alpha_rhs = rhs->type == mod_sprite ? 0.5 : rhs->ent.alpha == 0 ? 1 : rhs->ent.alpha;

	// Draw opaque entities first
	if (alpha_lhs == 1 && alpha_rhs != 1) {
		return -1;
	}
	else if (alpha_lhs != 1 && alpha_rhs == 1) {
		return 1;
	}

	if (alpha_lhs != 1) {
		// Furthest first
		if (lhs->distance > rhs->distance) {
			return -1;
		}
		else if (lhs->distance < rhs->distance) {
			return 1;
		}
	}

	// order by brush/alias/etc for batching
	if (lhs->type != rhs->type) {
		return lhs->type < rhs->type ? -1 : 1;
	}

	// Then by model
	if ((uintptr_t)lhs->ent.model < (uintptr_t)rhs->ent.model) {
		return -1;
	}
	if ((uintptr_t)lhs->ent.model >(uintptr_t)rhs->ent.model) {
		return 1;
	}

	if (alpha_lhs == 1) {
		// Closest first
		if (lhs->distance < rhs->distance) {
			return -1;
		}
		else if (lhs->distance > rhs->distance) {
			return 1;
		}
	}

	// Then by position
	if ((uintptr_t)lhs < (uintptr_t)rhs) {
		return -1;
	}
	if ((uintptr_t)lhs >(uintptr_t)rhs) {
		return 1;
	}
	return 0;
}

static void R_DrawEntitiesOnList(visentlist_t *vislist, visentlist_entrytype_t type)
{
	int i;

	if (r_drawentities.integer && vislist->typecount[type] >= 0) {
		for (i = 0; i < vislist->count; i++) {
			visentity_t* todraw = &vislist->list[i];

			if (!todraw->draw[type]) {
				continue;
			}

			switch (todraw->type) {
				case mod_brush:
					R_BrushModelDrawEntity(&todraw->ent);
					break;
				case mod_sprite:
					if (todraw->ent.model->type == mod_sprite) {
						renderer.DrawSpriteModel(&todraw->ent);
					}
					else {
						R_DrawTrySimpleItem(&todraw->ent);
					}
					break;
				case mod_alias:
					if (type != visent_additive) {
						if (type == visent_shells) {
							renderer.DrawAliasModelPowerupShell(&todraw->ent);
						}
						else {
							R_DrawAliasModel(&todraw->ent, type == visent_outlines);
						}
					}
					break;
				case mod_alias3:
					if (type == visent_shells) {
						renderer.DrawAlias3ModelPowerupShell(&todraw->ent);
					}
					else {
						renderer.DrawAlias3Model(&todraw->ent, type == visent_outlines, type == visent_additive);
					}
					break;
				case mod_unknown:
					// keeps compiler happy
					break;
			}
		}
	}
}

void R_PolyBlend(void)
{
	extern cvar_t gl_hwblend;

	if (vid_hwgamma_enabled && gl_hwblend.value && !cl.teamfortress) {
		return;
	}
	if (!v_blend[3]) {
		return;
	}

	renderer.PolyBlend(v_blend);
}

static void R_DrawEntities(void)
{
	visentlist_entrytype_t ent_type;

#ifdef RENDERER_OPTION_MODERN_OPENGL
	if (R_UseModernOpenGL()) {
		GLM_InitialiseAliasModelBatches();
	}
#endif

	if (!r_drawentities.integer) {
		return;
	}

	R_TraceEnterNamedRegion("R_DrawEntities");

	R_Sprite3DInitialiseBatch(SPRITE3D_ENTITIES, r_state_sprites_textured, null_texture_reference, 0, r_primitive_triangle_strip);
	qsort(cl_visents.list, cl_visents.count, sizeof(cl_visents.list[0]), R_DrawEntitiesSorter);
	for (ent_type = 0; ent_type < visent_max; ++ent_type) {
		R_DrawEntitiesOnList(&cl_visents, ent_type);
	}
	if (R_UseModernOpenGL() || R_UseVulkan()) {
		R_DrawViewModel();
	}
	R_TraceLeaveNamedRegion();
}
