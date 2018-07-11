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
#include "gl_local.h"
#include "vx_stuff.h"
#include "vx_vertexlights.h"
#include "utils.h"
#include "qsound.h"
#include "hud.h"
#include "hud_common.h"
#include "gl_bloom.h"
#include "rulesets.h"
#include "teamplay.h"
#include "gl_sprite3d.h"
#include "r_performance.h"
#include "qmb_particles.h"
#include "r_chaticons.h"
#include "r_matrix.h"
#include "r_local.h"
#include "r_state.h"
#include "gl_aliasmodel.h"
#include "gl_sky.h"
#include "r_brushmodel.h"
#include "r_lighting.h"
#include "r_buffers.h"

void GLM_ScreenDrawStart(void);

void OnChange_gl_clearColor(cvar_t *v, char *s, qbool *cancel);
void SCR_OnChangeMVHudPos(cvar_t *var, char *newval, qbool *cancel);
void OnChange_r_drawflat(cvar_t *v, char *skyname, qbool *cancel);
void OnChange_r_skyname(cvar_t *v, char *s, qbool *cancel);
void R_MarkLeaves(void);
void R_InitBubble(void);
void R_InitAliasModelCvars(void);
void R_CreateWorldTextureChains(void);
static void R_SetupGL(void);
static void R_RenderTransparentWorld(void);

void GLM_RenderView(void);

static rendering_state_t sprite_entity_state;

extern msurface_t *alphachain;
extern vec3_t     lightspot;

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
vec3_t    r_origin; // view origin
float     r_world_matrix[16];
float     r_base_world_matrix[16];
float     clearColor[3] = {0, 0, 0};
int       r_visframecount;                    // bumped when going to a new PVS
int       r_framecount;                       // used for dlight push checking
int       lightmode = 2;
unsigned int d_lightstylevalue[256];             // 8.8 fraction of base light value
texture_ref underwatertexture, detailtexture, solidtexture;

void OnSquareParticleChange(cvar_t *var, char *value, qbool *cancel)
{
	Cvar_SetIgnoreCallback(var, value);

	Classic_InitParticles();
}

static void OnDynamicLightingChange(cvar_t* var, char* value, qbool* cancel)
{
	if (GL_UseImmediateMode() && atoi(value) > 1) {
		Con_Printf("Hardware lighting not supported when not using GLSL\n");
		*cancel = true;
		return;
	}
}

cvar_t cl_multiview                        = {"cl_multiview", "0" };
cvar_t cl_mvdisplayhud                     = {"cl_mvdisplayhud", "1"};
cvar_t cl_mvhudvertical                    = {"cl_mvhudvertical", "0"};
cvar_t cl_mvhudflip                        = {"cl_mvhudflip", "0"};
cvar_t cl_mvhudpos                         = {"cl_mvhudpos", "bottom center"};
cvar_t cl_mvinset                          = {"cl_mvinset", "0"};
cvar_t cl_mvinsetcrosshair                 = {"cl_mvinsetcrosshair", "1"};
cvar_t cl_mvinsethud                       = {"cl_mvinsethud", "1"};
cvar_t r_drawentities                      = {"r_drawentities", "1"};
cvar_t r_lerpframes                        = {"r_lerpframes", "1"};
cvar_t r_drawflame                         = {"r_drawflame", "1"};
cvar_t r_drawdisc                          = {"r_drawdisc", "1"};
cvar_t r_speeds                            = {"r_speeds", "0"};
cvar_t r_fullbright                        = {"r_fullbright", "0"};
cvar_t r_shadows                           = {"r_shadows", "0"};
cvar_t r_wateralpha                        = {"gl_turbalpha", "1"};
cvar_t r_dynamic                           = {"r_dynamic", "1", 0, OnDynamicLightingChange };
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
cvar_t r_wallcolor                         = {"r_wallcolor", "255 255 255", CVAR_COLOR, OnChange_r_drawflat};
cvar_t r_floorcolor                        = {"r_floorcolor", "50 100 150", CVAR_COLOR, OnChange_r_drawflat};
cvar_t gl_textureless                      = {"gl_textureless", "0", 0, OnChange_r_drawflat}; //Qrack
cvar_t r_farclip                           = {"r_farclip", "8192"}; // previous default was 4096. 8192 helps some TF players in big maps
cvar_t r_skyname                           = {"r_skyname", "", 0, OnChange_r_skyname};
cvar_t gl_detail                           = {"gl_detail","0"};
cvar_t gl_brush_polygonoffset              = {"gl_brush_polygonoffset", "2.0"}; // This is the one to adjust if you notice flicker on lift @ e1m1 for instance, for z-fighting
cvar_t gl_caustics                         = {"gl_caustics", "0"}; // 1
cvar_t gl_waterfog                         = {"gl_turbfog", "0"}; // 2
cvar_t gl_waterfog_density                 = {"gl_turbfogDensity", "1"};
cvar_t gl_waterfog_color_water             = {"gl_turbfog_color_water", "32 64 128", CVAR_COLOR};
cvar_t gl_waterfog_color_lava              = {"gl_turbfog_color_lava", "255 64 0", CVAR_COLOR};
cvar_t gl_waterfog_color_slime             = {"gl_turbfog_color_slime", "128 255 0", CVAR_COLOR};
cvar_t gl_lumaTextures                     = {"gl_lumaTextures", "1"};
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
cvar_t gl_powerupshells_size               = {"gl_powerupshells_size", "5"};
cvar_t gl_fogenable                        = {"gl_fog", "0"};
cvar_t gl_fogstart                         = {"gl_fogstart", "50.0"};
cvar_t gl_fogend                           = {"gl_fogend", "800.0"};
cvar_t gl_fogred                           = {"gl_fogred", "0.6"};
cvar_t gl_foggreen                         = {"gl_foggreen", "0.5"};
cvar_t gl_fogblue                          = {"gl_fogblue", "0.4"};
cvar_t gl_fogsky                           = {"gl_fogsky", "1"};
cvar_t gl_simpleitems                      = {"gl_simpleitems", "0"};
cvar_t gl_simpleitems_size                 = {"gl_simpleitems_size", "16"};
cvar_t gl_simpleitems_orientation          = {"gl_simpleitems_orientation", "2"};
cvar_t gl_motion_blur                      = {"gl_motion_blur", "0"};
cvar_t gl_motion_blur_fps                  = {"gl_motion_blur_fps", "77"};
cvar_t gl_motion_blur_norm                 = {"gl_motion_blur_norm", "0.5"};
cvar_t gl_motion_blur_hurt                 = {"gl_motion_blur_hurt", "0.5"};
cvar_t gl_motion_blur_dead                 = {"gl_motion_blur_dead", "0.5"};
cvar_t gl_gammacorrection                  = {"gl_gammacorrection", "0", CVAR_LATCH};
cvar_t gl_modulate                         = {"gl_modulate", "1"};
cvar_t gl_outline                          = {"gl_outline", "0"};
cvar_t gl_outline_width                    = {"gl_outline_width", "2"};

cvar_t gl_vbo_clientmemory                 = {"gl_vbo_clientmemory", "0", CVAR_LATCH};

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

void R_DrawSpriteModel(entity_t *e)
{
	if (GL_UseGLSL()) {
		GLM_DrawSpriteModel(e);
	}
	else {
		GLC_DrawSpriteModel(e);
	}
}

qbool R_CanDrawSimpleItem(entity_t* e)
{
	int skin;

	if (!gl_simpleitems.integer || !e || !e->model) {
		return false;
	}

	skin = e->skinnum >= 0 && e->skinnum < MAX_SIMPLE_TEXTURES ? e->skinnum : 0;

	if (GL_UseGLSL()) {
		return GL_TextureReferenceIsValid(e->model->simpletexture_array[skin]);
	}
	else {
		return GL_TextureReferenceIsValid(e->model->simpletexture[skin]);
	}
}

static qbool R_DrawTrySimpleItem(entity_t* ent)
{
	int sprtype = gl_simpleitems_orientation.integer;
	float sprsize = bound(1, gl_simpleitems_size.value, 16), autorotate;
	texture_ref simpletexture;
	vec3_t right, up, org, offset, angles;
	int skin;

	if (!ent || !ent->model) {
		return false;
	}

	skin = ent->skinnum >= 0 && ent->skinnum < MAX_SIMPLE_TEXTURES ? ent->skinnum : 0;
	if (GL_UseGLSL()) {
		simpletexture = ent->model->simpletexture_array[skin];
	}
	else {
		simpletexture = ent->model->simpletexture[skin];
	}
	if (!GL_TextureReferenceIsValid(simpletexture)) {
		return false;
	}

	autorotate = anglemod(100 * cl.time);
	if (sprtype == SPR_ORIENTED) {
		// bullet marks on walls
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
		vectoangles(right, angles);
	}
	else if (sprtype == SPR_VP_PARALLEL_UPRIGHT) {
		VectorSet(up, 0, 0, 1);
		VectorCopy(vright, right);
		vectoangles(right, angles);
	}
	else {
		// normal sprite
		VectorCopy(vup, up);
		VectorCopy(vright, right);
		vectoangles(right, angles);
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

	if (GL_UseGLSL()) {
		model_t* m = ent->model;

		GLM_DrawSimpleItem(
			m->simpletexture_array[skin],
			m->simpletexture_indexes[skin],
			m->simpletexture_scalingS[skin],
			m->simpletexture_scalingT[skin],
			org,
			sprsize,
			up,
			right
		);
	}
	else {
		GLC_DrawSimpleItem(simpletexture, org, sprsize, up, right);
	}
	return true;
}

static int R_DrawEntitiesSorter(const void* lhs_, const void* rhs_)
{
	const visentity_t* lhs = (const visentity_t*) lhs_;
	const visentity_t* rhs = (const visentity_t*) rhs_;

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
	if ((uintptr_t)lhs->ent.model > (uintptr_t)rhs->ent.model) {
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
	if ((uintptr_t)lhs > (uintptr_t)rhs) {
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
				R_DrawBrushModel(&todraw->ent);
				break;
			case mod_sprite:
				if (todraw->ent.model->type == mod_sprite) {
					R_DrawSpriteModel(&todraw->ent);
				}
				else {
					R_DrawTrySimpleItem(&todraw->ent);
				}
				break;
			case mod_alias:
				if (type == visent_shells) {
					R_DrawAliasPowerupShell(&todraw->ent);
				}
				else {
					R_DrawAliasModel(&todraw->ent);
				}
				break;
			case mod_alias3:
				R_DrawAlias3Model(&todraw->ent);
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

	GL_StateBeginPolyBlend();
	if (GL_UseGLSL()) {
		GLM_PolyBlend(v_blend);
	}
	else {
		GLC_PolyBlend(v_blend);
	}
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

void R_SetupFrame(void)
{
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
		if (leaf->contents == CONTENTS_EMPTY) {
			r_viewleaf2 = leaf;
		}
	} else if (r_viewleaf->contents == CONTENTS_EMPTY) {
		// look down a bit
		VectorCopy (r_origin, testorigin);
		testorigin[2] -= 10;
		leaf = Mod_PointInLeaf (testorigin, cl.worldmodel);
		if (leaf->contents <= CONTENTS_WATER &&	leaf->contents >= CONTENTS_LAVA) {
			r_viewleaf2 = leaf;
		}
	}

	V_SetContentsColor(r_viewleaf->contents);
	R_AddWaterfog(r_viewleaf->contents);
	V_CalcBlend();

	memcpy(&prevFrameStats, &frameStats, sizeof(prevFrameStats));
	memset(&frameStats, 0, sizeof(frameStats));
	R_LightmapFrameInit();
}

void MYgluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar)
{
	GLdouble xmin, xmax, ymin, ymax;

	ymax = zNear * tan(fovy * M_PI / 360.0);
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	if (cl_multiview.value == 2 && !cl_mvinset.value && cls.mvdplayback) {
		GL_Frustum(xmin, xmax, ymin + (ymax - ymin)*0.25, ymax - (ymax - ymin)*0.25, zNear, zFar);
	}
	else if (CL_MultiviewActiveViews() == 3) {
		if (CL_MultiviewCurrentView() == 2) {
			GL_Frustum(xmin, xmax, ymin + (ymax - ymin)*0.25, ymax - (ymax - ymin)*0.25, zNear, zFar);
		}
		else {
			GL_Frustum(xmin, xmax, ymin, ymax, zNear, zFar);
		}
	}
	else {
		GL_Frustum(xmin, xmax, ymin, ymax, zNear, zFar);
	}
}

void R_SetViewports(int glx, int x, int gly, int y2, int w, int h, float max)
{
	//
	// Setup Multiview-viewports
	//
	if (max == 1) {
		GL_Viewport(glx + x, gly + y2, w, h);
		return;
	}
	else if (max == 2 && cl_mvinset.value) {
		if (CL_MultiviewCurrentView() == 2) {
			GL_Viewport(glx + x, gly + y2, w, h);
		}
		else if (CL_MultiviewCurrentView() == 1 && !cl_sbar.value) {
			GL_Viewport(glx + x + (glwidth / 3) * 2 + 2, gly + y2 + (glheight / 3) * 2, w / 3, h / 3);
		}
		else if (CL_MultiviewCurrentView() == 1 && cl_sbar.value) {
			GL_Viewport(glx + x + (glwidth / 3) * 2 + 2, gly + y2 + (h / 3) * 2, w / 3, h / 3);
		}
		else {
			Com_Printf("ERROR!\n");
		}
		return;
	}
	else if (max == 2 && !cl_mvinset.value) {
		if (CL_MultiviewCurrentView() == 2) {
			GL_Viewport(0, h / 2, w, h / 2);
		}
		else if (CL_MultiviewCurrentView() == 1) {
			GL_Viewport(0, 0, w, h / 2 - 1);
		}
		else {
			Com_Printf("ERROR!\n");
		}
		return;
	}
	else if (max == 3) {
		if (CL_MultiviewCurrentView() == 2) {
			GL_Viewport(0, h / 2, w, h / 2);
		}
		else if (CL_MultiviewCurrentView() == 3) {
			GL_Viewport(0, 0, w / 2, h / 2 - 1);
		}
		else {
			GL_Viewport(w / 2, 0, w / 2, h / 2 - 1);
		}
		return;
	}
	else {
		if (CL_MultiviewCurrentView() == 2) {
			GL_Viewport(0, h / 2, w / 2, h / 2);
		}
		else if (CL_MultiviewCurrentView() == 3) {
			GL_Viewport(w / 2, h / 2, w / 2, h / 2);
		}
		else if (CL_MultiviewCurrentView() == 4) {
			GL_Viewport(0, 0, w / 2, h / 2 - 1);
		}
		else if (CL_MultiviewCurrentView() == 1) {
			GL_Viewport(w / 2, 0, w / 2, h / 2 - 1);
		}
	}

	return;
}

void R_SetupViewport(void)
{
	float screenaspect;
	extern int glwidth, glheight;
	int x, x2, y2, y, w, h, farclip;

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
	GL_IdentityProjectionView();
	if (CL_MultiviewEnabled() && CL_MultiviewCurrentView() != 0) {
		R_SetViewports(glx, x, gly, y2, w, h, cl_multiview.value);
	}
	if (!CL_MultiviewEnabled()) {
		GL_Viewport(glx + x, gly + y2, w, h);
	}

	farclip = max((int)r_farclip.value, 4096);
	screenaspect = (float)r_refdef.vrect.width / r_refdef.vrect.height;

	MYgluPerspective(r_refdef.fov_y, screenaspect, r_nearclip.value, farclip);
}

static void R_SetupGL(void)
{
	R_SetupViewport();

	GL_StateDefault3D();

	if (GL_UseGLSL()) {
		GLM_SetupGL();
	}
	else {
		GLC_SetupGL();
	}
}

void R_Init(void)
{
	Cmd_AddCommand("loadsky", R_LoadSky_f);
	Cmd_AddCommand("timerefresh", R_TimeRefresh_f);
#ifndef CLIENTONLY
	Cmd_AddCommand("pointfile", R_ReadPointFile_f);
#endif

	Cvar_SetCurrentGroup(CVAR_GROUP_EYECANDY);
#ifdef BLOOM_SUPPORTED
	Cvar_Register(&r_bloom);
	Cvar_Register(&r_bloom_darken);
	Cvar_Register(&r_bloom_alpha);
	Cvar_Register(&r_bloom_diamond_size);
	Cvar_Register(&r_bloom_intensity);
	Cvar_Register(&r_bloom_sample_size);
	Cvar_Register(&r_bloom_fast_sample);
#endif
	Cvar_Register(&r_drawentities);
	Cvar_Register(&r_lerpframes);
	Cvar_Register(&r_drawflame);
	Cvar_Register(&r_drawdisc);
	Cvar_Register(&gl_detail);
	Cvar_Register(&gl_powerupshells);
	Cvar_Register(&gl_powerupshells_size);

	Cvar_Register(&gl_simpleitems);
	Cvar_Register(&gl_simpleitems_size);
	Cvar_Register(&gl_simpleitems_orientation);

	Cvar_Register(&gl_motion_blur);
	Cvar_Register(&gl_motion_blur_fps);
	Cvar_Register(&gl_motion_blur_norm);
	Cvar_Register(&gl_motion_blur_hurt);
	Cvar_Register(&gl_motion_blur_dead);

	Cvar_SetCurrentGroup(CVAR_GROUP_PARTICLES);
	Cvar_Register(&gl_part_explosions);
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
	if (!COM_CheckParm(cmdline_param_client_nomultitexturing)) {
		Cvar_Register(&gl_waterfog);
		Cvar_Register(&gl_waterfog_density);
		Cvar_Register(&gl_waterfog_color_water);
		Cvar_Register(&gl_waterfog_color_lava);
		Cvar_Register(&gl_waterfog_color_slime);
	}

	Cvar_Register(&gl_fogenable);
	Cvar_Register(&gl_fogstart);
	Cvar_Register(&gl_fogend);
	Cvar_Register(&gl_fogsky);
	Cvar_Register(&gl_fogred);
	Cvar_Register(&gl_fogblue);
	Cvar_Register(&gl_foggreen);

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
	Cvar_Register(&gl_lumaTextures);
	Cvar_Register(&r_drawflat);
	Cvar_Register(&r_wallcolor);
	Cvar_Register(&r_floorcolor);
	Cvar_Register(&gl_textureless); //Qrack

	Cvar_SetCurrentGroup(CVAR_GROUP_OPENGL);
	Cvar_Register(&r_farclip);
	Cvar_Register(&gl_clear);
	Cvar_Register(&gl_clearColor);

	Cvar_Register(&gl_brush_polygonoffset);

	Cvar_Register(&gl_nocolors);
	Cvar_Register(&gl_finish);
	Cvar_Register(&gl_gammacorrection);
	Cvar_Register(&gl_modulate);

	Cvar_Register(&gl_outline);
	Cvar_Register(&gl_outline_width);

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
	if (qmb_initialized) {
		InitVXStuff(); // safe re-init imo
	}
	else {
		; // FIXME: hm, in case of vid_restart, what we must do if before vid_restart qmb_initialized was true?
	}

	InitTracker();
	R_InitOtherTextures(); // safe re-init
	R_InitBloomTextures();

	// Init entity state
	R_Init3DSpriteRenderingState(&sprite_entity_state, "sprite_entity_state");
	sprite_entity_state.textureUnits[0].enabled = true;
	sprite_entity_state.textureUnits[0].mode = r_texunit_mode_replace;
}

static void R_RenderScene(void)
{
	visentlist_entrytype_t ent_type;

	GL_EnterRegion("R_DrawWorld");
	R_DrawWorld();		// adds static entities to the list
	GL_LeaveRegion();

	if (GL_WaterAlpha() == 1) {
		R_RenderTransparentWorld();
	}

	if (GL_UseGLSL()) {
		GLM_InitialiseAliasModelBatches();
	}

	if (r_drawentities.integer) {
		GL_EnterRegion("R_DrawEntities");

		GL_Sprite3DInitialiseBatch(SPRITE3D_ENTITIES, &sprite_entity_state, NULL, null_texture_reference, 0, r_primitive_triangle_strip);
		qsort(cl_visents.list, cl_visents.count, sizeof(cl_visents.list[0]), R_DrawEntitiesSorter);
		for (ent_type = visent_firstpass; ent_type < visent_max; ++ent_type) {
			R_DrawEntitiesOnList(&cl_visents, ent_type);
		}
		if (GL_UseGLSL()) {
			R_DrawViewModel();
		}
		GL_LeaveRegion();
	}
}

static void R_RenderTransparentWorld(void)
{
	if (GL_UseGLSL()) {
		GLM_DrawWaterSurfaces();
	}
	else {
		GLC_DrawWaterSurfaces();
	}
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
	int clearbits = 0;

	// This used to cause a bug with some graphics cards when
	// in multiview mode. It would clear all but the last
	// drawn views.
	if (!cl_multiview.integer && (gl_clear.integer || (!vid_hwgamma_enabled && v_contrast.value > 1))) {
		clearbits |= GL_COLOR_BUFFER_BIT;
	}

	// If outside level or in wall, sky is usually visible
	if (r_viewleaf->contents == CONTENTS_SOLID) {
		if (GL_UseGLSL() || r_fastsky.integer) {
			clearbits |= GL_COLOR_BUFFER_BIT;
		}
	}

	if (gl_clear.integer) {
		if (gl_fogenable.integer) {
			R_ClearColor(gl_fogred.value, gl_foggreen.value, gl_fogblue.value, 1.0);//Tei custom clear color
		}
		else {
			R_ClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0);
		}
	}

	clearbits |= GL_DEPTH_BUFFER_BIT;
	glClear(clearbits);
}

// player velocity is drawn on screen
// as 3d vector and its projections
static void GL_DrawVelocity3D(void)
{
	extern cvar_t show_velocity_3d;
	if (!show_velocity_3d.integer) {
		return;
	}

	if (GL_UseGLSL()) {
		GLM_DrawVelocity3D();
	}
	else {
		//GLC_DrawVelocity3D();
	}
}

/*
   Motion blur effect.
   Stolen from FTE engine.
*/
static void R_RenderSceneBlurDo(float alpha)
{
	if (GL_UseGLSL()) {
		GLM_RenderSceneBlurDo(alpha);
	}
	else {
		GLC_RenderSceneBlurDo(alpha);
	}
}

static void R_RenderSceneBlur(void)
{
	if (!gl_motion_blur.integer)
	{
		// Motion blur disabled entirely.
		return;
	}

	// FIXME: Actually here should be some smoothing code for transaction from one case to another,
	// since for example if we turned off blur for everything but hurt, when we feel pain we use blur, but when
	// pain is ended we saddenly turning blur off, that does not look natural.

	if (gl_motion_blur_dead.value && cl.stats[STAT_HEALTH] < 1)
	{
		// We are dead.
		R_RenderSceneBlurDo (gl_motion_blur_dead.value);
	}
	// We are alive, lets check different cases.
	else if (gl_motion_blur_hurt.value && cl.hurtblur > cl.time)
	{
		// Hurt.
		R_RenderSceneBlurDo (gl_motion_blur_hurt.value);
	}
	else if (gl_motion_blur_norm.value)
	{
		// Plain case.
		R_RenderSceneBlurDo (gl_motion_blur_norm.value);
	}
	else
	{
		// We do not really blur anything, just copy image, so if we start bluring it will be smooth transaction.
		R_RenderSceneBlurDo (-1);
	}
}

void R_PostProcessScene(void)
{
	if (GL_UseImmediateMode()) {
		R_RenderSceneBlur();
		R_BloomBlend();
	}
}

void R_PostProcessScreen(void)
{
	if (GL_UseGLSL()) {
		GLM_PostProcessScreen();
	}
	else {
		GLC_BrightenScreen();
		V_UpdatePalette();
	}
}

void R_ScreenDrawStart(void)
{
	if (GL_UseGLSL()) {
		GLM_ScreenDrawStart();
	}
	else {

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
	if (GL_UseImmediateMode()) {
		R_DrawViewModel();
	}

	GL_DrawVelocity3D();

	// While still in 3D mode, calculate the location of labels to be printed in 2D
	SCR_SetupAutoID();
}

void R_RenderView(void)
{
	if (!r_worldentity.model || !cl.worldmodel) {
		Sys_Error("R_RenderView: NULL worldmodel");
	}

	// Wait for previous commands to 'complete'
	if (!r_speeds.integer && gl_finish.integer) {
		glFinish();
	}

	R_SetFrustum();

	R_SetupGL();
	R_Clear();
	R_MarkLeaves();	// done here so we know if we're in water
	R_CreateWorldTextureChains();

	// render normal view (world & entities)
	R_RenderScene();

	// Adds 3d effects (particles, lights, chat icons etc)
	R_Render3DEffects();

	// Draws transparent world surfaces
	R_RenderTransparentWorld();

	// Render billboards
	GL_Draw3DSprites();

	// Draw 3D hud elements
	R_Render3DHud();

	if (GL_UseGLSL()) {
		GLM_RenderView();
	}

	R_PerformanceEndFrame();
}

void R_PreRenderView(void)
{
	if (GL_UseGLSL()) {
		GLM_PreRenderView();
	}
	else {
		GLC_PreRenderView();
	}
}

qbool R_PointIsUnderwater(vec3_t point)
{
	int contents = TruePointContents(point);

	return ISUNDERWATER(contents);
}

void R_EnsureFinished(void)
{
	glFinish();
}
