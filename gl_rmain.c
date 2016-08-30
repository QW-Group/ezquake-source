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


void CI_Init(void);
void OnChange_gl_clearColor(cvar_t *v, char *s, qbool *cancel);
void SCR_OnChangeMVHudPos(cvar_t *var, char *newval, qbool *cancel);
void OnChange_r_drawflat(cvar_t *v, char *skyname, qbool *cancel);
void OnChange_r_skyname(cvar_t *v, char *s, qbool *cancel);
void R_MarkLeaves(void);
void R_InitBubble(void);

extern msurface_t *alphachain;
extern cvar_t     maxclients;
extern vec3_t     lightcolor;
extern vec3_t     lightspot;
extern float      bubblecolor[NUM_DLIGHTTYPES][4];

#define NUMVERTEXNORMALS 162
#define SHADEDOT_QUANT   64

// precalculated dot products for quantized angles
byte      r_avertexnormal_dots[SHADEDOT_QUANT][NUMVERTEXNORMALS] =
#include "anorm_dots.h"
;
float     r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};
byte      *shadedots = r_avertexnormal_dots[0];


texture_t *r_notexture_mip = NULL;
refdef2_t r_refdef2;                          // screen size info
refdef_t  r_refdef;                           // screen size info
entity_t  r_worldentity;
entity_t  *currententity;
mplane_t  frustum[4];
mleaf_t   *r_viewleaf;
mleaf_t   *r_oldviewleaf;
mleaf_t   *r_viewleaf2;                       // for watervis hack
mleaf_t   *r_oldviewleaf2;                    // for watervis hack
vec3_t    dlight_color;
vec3_t    modelorg, r_entorigin;
vec3_t    vup, vpn, vright;                   // view origin
vec3_t    r_origin; // view origin
vec3_t    vertexlight;
vec3_t    shadevector;
float     gldepthmin, gldepthmax;
float     r_world_matrix[16];
float     r_base_world_matrix[16];
float     r_framelerp;
float     r_modelalpha;
float     r_lerpdistance;
float     r_modelcolor[3];
float     r_shellcolor[3];
float     shadelight;
float     ambientlight;
float     apitch;
float     ayaw;
float     clearColor[3] = {0, 0, 0};
qbool     r_cache_thrash;                     // compatability
qbool     full_light;
int       lastposenum;
int       shelltexture = 0;
int       r_visframecount;                    // bumped when going to a new PVS
int       r_framecount;                       // used for dlight push checking
int       c_brush_polys;
int       c_alias_polys;
int       lightmode = 2;
int       sceneblur_texture;                  // motion blur.
int       d_lightstylevalue[256];             // 8.8 fraction of base light value
int       particletexture;                    // little dot for particles
int       playertextures;                     // up to 16 color translated skins
int       playernmtextures[MAX_CLIENTS];
int       playerfbtextures[MAX_CLIENTS];
int       skyboxtextures[MAX_SKYBOXTEXTURES];
int       underwatertexture, detailtexture;

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
cvar_t r_lerpmuzzlehack                    = {"r_lerpmuzzlehack", "1"};
cvar_t r_drawflame                         = {"r_drawflame", "1"};
cvar_t r_speeds                            = {"r_speeds", "0"};
cvar_t r_fullbright                        = {"r_fullbright", "0"};
cvar_t r_lightmap                          = {"r_lightmap", "0"};
cvar_t gl_shaftlight                       = {"gl_shaftlight", "1"};
cvar_t r_shadows                           = {"r_shadows", "0"};
cvar_t r_wateralpha                        = {"gl_turbalpha", "1"};
cvar_t r_dynamic                           = {"r_dynamic", "1"};
cvar_t r_novis                             = {"r_novis", "0"};
cvar_t r_netgraph                          = {"r_netgraph", "0"};
cvar_t r_netstats                          = {"r_netstats", "0"};
cvar_t r_fullbrightSkins                   = {"r_fullbrightSkins", "1", 0, Rulesets_OnChange_r_fullbrightSkins};
cvar_t r_enemyskincolor                    = {"r_enemyskincolor", "", CVAR_COLOR};
cvar_t r_teamskincolor                     = {"r_teamskincolor",  "", CVAR_COLOR};
cvar_t r_skincolormode                     = {"r_skincolormode",  "0"};
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
cvar_t gl_cull                             = {"gl_cull", "1"};
cvar_t gl_smoothmodels                     = {"gl_smoothmodels", "1"};
cvar_t gl_affinemodels                     = {"gl_affinemodels", "0"};
cvar_t gl_polyblend                        = {"gl_polyblend", "1"}; // 0
cvar_t gl_flashblend                       = {"gl_flashblend", "0"};
cvar_t gl_rl_globe                         = {"gl_rl_globe", "0"};
cvar_t gl_playermip                        = {"gl_playermip", "0"};
cvar_t gl_nocolors                         = {"gl_nocolors", "0"};
cvar_t gl_finish                           = {"gl_finish", "0"};
cvar_t gl_fb_bmodels                       = {"gl_fb_bmodels", "1"};
cvar_t gl_fb_models                        = {"gl_fb_models", "1"};
cvar_t gl_lightmode                        = {"gl_lightmode", "2"};
cvar_t gl_loadlitfiles                     = {"gl_loadlitfiles", "1"};
cvar_t gl_colorlights                      = {"gl_colorlights", "1"};
cvar_t gl_solidparticles                   = {"gl_solidparticles", "0"}; // 1
cvar_t gl_squareparticles                  = {"gl_squareparticles", "0", CVAR_LATCH};
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
cvar_t gl_particle_style                   = {"gl_particle_style", "0"}; // 0 - round, 1 - square (sw style)
cvar_t gl_part_detpackexplosion_fire_color = {"gl_part_detpackexplosion_fire_color", "", CVAR_COLOR};
cvar_t gl_part_detpackexplosion_ray_color  = {"gl_part_detpackexplosion_ray_color", "", CVAR_COLOR};
cvar_t gl_powerupshells                    = {"gl_powerupshells", "1"};
cvar_t gl_powerupshells_style              = {"gl_powerupshells_style", "0"};
cvar_t gl_powerupshells_size               = {"gl_powerupshells_size", "5"};
cvar_t gl_powerupshells_effect1level       = {"gl_powerupshells_effect1level", "0.75"};
cvar_t gl_powerupshells_base1level         = {"gl_powerupshells_base1level", "0.05"};
cvar_t gl_powerupshells_effect2level       = {"gl_powerupshells_effect2level", "0.4"};
cvar_t gl_powerupshells_base2level         = {"gl_powerupshells_base2level", "0.1"};
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

typedef struct custom_model_color_s {
	cvar_t color_cvar;
	cvar_t fullbright_cvar;
	cvar_t* amf_cvar;
	int model_hint;
} custom_model_color_t;

custom_model_color_t custom_model_colors[] = {
	// LG beam
	{
		{ "gl_custom_lg_color", "", CVAR_COLOR },
		{ "gl_custom_lg_fullbright", "1" },
		&amf_lightning,
		MOD_THUNDERBOLT
	},
	// Rockets
	{
		{ "gl_custom_rocket_color", "", CVAR_COLOR },
		{ "gl_custom_rocket_fullbright", "1" },
		NULL,
		MOD_ROCKET
	},
	// Grenades
	{
		{ "gl_custom_grenade_color", "", CVAR_COLOR },
		{ "gl_custom_grenade_fullbright", "1" },
		NULL,
		MOD_GRENADE
	},
	// Spikes
	{
		{ "gl_custom_spike_color", "", CVAR_COLOR },
		{ "gl_custom_spike_fullbright", "1" },
		&amf_part_spikes,
		MOD_SPIKE
	}
};

custom_model_color_t* custom_model = NULL;

void GL_PolygonOffset(float factor, float units)
{
	if (factor || units)
	{
		glEnable (GL_POLYGON_OFFSET_FILL);
		glEnable (GL_POLYGON_OFFSET_LINE);

		glPolygonOffset(factor, units);
	}
	else
	{
		glDisable (GL_POLYGON_OFFSET_FILL);
		glDisable (GL_POLYGON_OFFSET_LINE);
	}
}

//Returns true if the box is completely outside the frustom
qbool R_CullBox(vec3_t mins, vec3_t maxs)
{
	int i;

	for (i = 0; i < 4; i++) {
		if (BOX_ON_PLANE_SIDE (mins, maxs, &frustum[i]) == 2)
			return true;
	}
	return false;
}

//Returns true if the sphere is completely outside the frustum
qbool R_CullSphere(vec3_t centre, float radius)
{
	int i;
	mplane_t *p;

	for (i = 0, p = frustum; i < 4; i++, p++) {
		if (PlaneDiff(centre, p) <= -radius)
			return true;
	}

	return false;
}

void R_RotateForEntity(entity_t *e)
{
	glTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);

	glRotatef (e->angles[1], 0, 0, 1);
	glRotatef (-e->angles[0], 0, 1, 0);
	glRotatef (e->angles[2], 1, 0, 0);
}


mspriteframe_t *R_GetSpriteFrame(entity_t *e, msprite2_t *psprite)
{
	mspriteframe_t  *pspriteframe;
	mspriteframe2_t *pspriteframe2;
	int i, numframes, frame, offset;
	float fullinterval, targettime, time;

	frame = e->frame;

	if (frame >= psprite->numframes || frame < 0) {
		Com_DPrintf ("R_GetSpriteFrame: no such frame %d (model %s)\n", frame, e->model->name);
		return NULL;
	}

	offset    = psprite->frames[frame].offset;
	numframes = psprite->frames[frame].numframes;

	if (offset < (int)sizeof(msprite2_t) || numframes < 1) {
		Com_Printf ("R_GetSpriteFrame: wrong sprite\n");
		return NULL;
	}

	if (psprite->frames[frame].type == SPR_SINGLE) {
		pspriteframe  = (mspriteframe_t* )((byte*)psprite + offset);
	}
	else {
		pspriteframe2 = (mspriteframe2_t*)((byte*)psprite + offset);

		fullinterval = pspriteframe2[numframes-1].interval;

		time = r_refdef2.time;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by 0
		targettime = time - ((int) (time / fullinterval)) * fullinterval;

		for (i = 0; i < (numframes - 1); i++)
			if (pspriteframe2[i].interval > targettime)
				break;

		pspriteframe = &pspriteframe2[i].frame;
	}

	return pspriteframe;
}

void R_DrawSpriteModel (entity_t *e)
{
	vec3_t point, right, up;
	mspriteframe_t *frame;
	msprite2_t *psprite;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	psprite = (msprite2_t*)Mod_Extradata (e->model);	//locate the proper data
	frame = R_GetSpriteFrame (e, psprite);

	if (!frame)
		return;

	if (psprite->type == SPR_ORIENTED) {
		// bullet marks on walls
		AngleVectors (e->angles, NULL, right, up);
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


int GL_GenerateShellTexture(void)
{
	int x, y, d;
	byte data[32][32][4];
	for (y = 0;y < 32;y++)
	{
		for (x = 0;x < 32;x++)
		{
			d = (sin(x * M_PI / 8.0f) + cos(y * M_PI / 8.0f)) * 64 + 64;
			if (d < 0)
				d = 0;
			if (d > 255)
				d = 255;
			data[y][x][0] = data[y][x][1] = data[y][x][2] = d;
			data[y][x][3] = 255;
		}
	}

	return GL_LoadTexture("shelltexture", 32, 32, &data[0][0][0], TEX_MIPMAP, 4);
}

void GL_DrawAliasOutlineFrame (aliashdr_t *paliashdr, int pose1, int pose2) 
{
	int *order, count;
	vec3_t interpolated_verts;
	float lerpfrac;
	trivertx_t *verts1, *verts2;

	GL_PolygonOffset(1, 1);

	glCullFace (GL_BACK);
	glPolygonMode (GL_FRONT, GL_LINE);

	// limit outline width, since even width == 3 can be considered as cheat.
	glLineWidth (bound(0.1, gl_outline_width.value, 3.0));

	glColor4f (0.0f, 0.0f, 0.0f, 1.0f);
	glEnable (GL_LINE_SMOOTH);
	glDisable (GL_TEXTURE_2D);

	lerpfrac = r_framelerp;
	lastposenum = (lerpfrac >= 0.5) ? pose2 : pose1;    

	verts2 = verts1 = (trivertx_t *) ((byte *) paliashdr + paliashdr->posedata);

	verts1 += pose1 * paliashdr->poseverts;
	verts2 += pose2 * paliashdr->poseverts;

	order = (int *) ((byte *) paliashdr + paliashdr->commands);

	for ( ;; )
	{
		count = *order++;

		if (!count)
			break;

		if (count < 0)
		{
			count = -count;
			glBegin(GL_TRIANGLE_FAN);
		}
		else
			glBegin(GL_TRIANGLE_STRIP);

		do 
		{
			order += 2;

			if ((currententity->renderfx & RF_LIMITLERP))
				lerpfrac = VectorL2Compare(verts1->v, verts2->v, r_lerpdistance) ? r_framelerp : 1;

			VectorInterpolate(verts1->v, lerpfrac, verts2->v, interpolated_verts);
			glVertex3fv(interpolated_verts);

			verts1++;
			verts2++;
		} 
		while (--count);

		glEnd();
	}

	glColor4f (1, 1, 1, 1);    
	glPolygonMode (GL_FRONT, GL_FILL);
	glDisable (GL_LINE_SMOOTH);
	glCullFace (GL_FRONT);
	glEnable (GL_TEXTURE_2D);    

	GL_PolygonOffset(0, 0);
}

void GL_DrawAliasFrame(aliashdr_t *paliashdr, int pose1, int pose2, qbool mtex, qbool scrolldir)
{
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

	if ( (r_shellcolor[0] || r_shellcolor[1] || r_shellcolor[2]) /* && bound(0, gl_powerupshells.value, 1) */ )
	{
		float scroll[2];
		float v[3];
		float shell_size = bound(0, gl_powerupshells_size.value, 20);

		// LordHavoc: set the state to what we need for rendering a shell
		if (!shelltexture)
			shelltexture = GL_GenerateShellTexture();
		GL_Bind (shelltexture);
		glEnable (GL_BLEND);

		if (gl_powerupshells_style.integer)
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		else
			glBlendFunc(GL_ONE, GL_ONE);

		glColor4f (r_shellcolor[0], r_shellcolor[1], r_shellcolor[2], bound(0, gl_powerupshells.value, 1)); // alpha so we can see colour underneath still

		if (scrolldir)
		{
			scroll[0] = cos(cl.time * -0.5); // FIXME: cl.time ????
			scroll[1] = sin(cl.time * -0.5);
		}
		else
		{
			scroll[0] = cos(cl.time * 1.5);
			scroll[1] = sin(cl.time * 1.1);
		}

		// get the vertex count and primitive type
		for (;;)
		{
			count = *order++;
			if (!count)
				break;

			if (count < 0)
			{
				count = -count;
				glBegin(GL_TRIANGLE_FAN);
			}
			else
				glBegin(GL_TRIANGLE_STRIP);

			do
			{
				glTexCoord2f (((float *) order)[0] * 2.0f + scroll[0], ((float *) order)[1] * 2.0f + scroll[1]);

				order += 2;

				v[0] = r_avertexnormals[verts1->lightnormalindex][0] * shell_size + verts1->v[0];
				v[1] = r_avertexnormals[verts1->lightnormalindex][1] * shell_size + verts1->v[1];
				v[2] = r_avertexnormals[verts1->lightnormalindex][2] * shell_size + verts1->v[2];
				v[0] += lerpfrac * (r_avertexnormals[verts2->lightnormalindex][0] * shell_size + verts2->v[0] - v[0]);
				v[1] += lerpfrac * (r_avertexnormals[verts2->lightnormalindex][1] * shell_size + verts2->v[1] - v[1]);
				v[2] += lerpfrac * (r_avertexnormals[verts2->lightnormalindex][2] * shell_size + verts2->v[2] - v[2]);

				glVertex3f(v[0], v[1], v[2]);

				verts1++;
				verts2++;
			} while (--count);

			glEnd();
		}
		// LordHavoc: reset the state to what the rest of the renderer expects
		glDisable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
	{
		if (r_modelalpha < 1)
			glEnable(GL_BLEND);

		if (custom_model) {
			glDisable(GL_TEXTURE_2D);
			glColor3ubv(custom_model->color_cvar.color);
		}

		for ( ;; )
		{
			count = *order++;
			if (!count)
				break;

			if (count < 0)
			{
				count = -count;
				glBegin(GL_TRIANGLE_FAN);
			}
			else
				glBegin(GL_TRIANGLE_STRIP);

			do {
				// texture coordinates come from the draw list
				if (mtex)
				{
					qglMultiTexCoord2f (GL_TEXTURE0, ((float *) order)[0], ((float *) order)[1]);
					qglMultiTexCoord2f (GL_TEXTURE1, ((float *) order)[0], ((float *) order)[1]);
				}
				else
					glTexCoord2f (((float *) order)[0], ((float *) order)[1]);

				order += 2;

				if ((currententity->renderfx & RF_LIMITLERP))
					lerpfrac = VectorL2Compare(verts1->v, verts2->v, r_lerpdistance) ? r_framelerp : 1;

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
					if (r_modelcolor[0] < 0)
						glColor4f(lc[0], lc[1], lc[2], r_modelalpha); // normal color
					else
						glColor4f(r_modelcolor[0] * lc[0], r_modelcolor[1] * lc[1], r_modelcolor[2] * lc[2], r_modelalpha); // forced
				}
				else if (custom_model == NULL)
				{
					if (r_modelcolor[0] < 0) {
						glColor4f(l, l, l, r_modelalpha); // normal color
					} else {
						glColor4f(r_modelcolor[0] * l, r_modelcolor[1] * l, r_modelcolor[2] * l, r_modelalpha); // forced
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

		if (custom_model) {
			glEnable(GL_TEXTURE_2D);
			custom_model = NULL;
		}
	}
}

void R_SetupAliasFrame(maliasframedesc_t *oldframe, maliasframedesc_t *frame, aliashdr_t *paliashdr, qbool mtex, qbool scrolldir, qbool outline)
{
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

	GL_DrawAliasFrame (paliashdr, oldpose, pose, mtex, scrolldir);

	if (outline)
		GL_DrawAliasOutlineFrame (paliashdr, oldpose, pose) ;
}

void GL_DrawAliasShadow(aliashdr_t *paliashdr, int posenum)
{
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

void R_AliasSetupLighting(entity_t *ent)
{
	int minlight, lnum;
	float add, fbskins;
	unsigned int i;
	unsigned int j;
	unsigned int k;
	vec3_t dist;
	model_t *clmodel;

	//VULT COLOURED MODEL LIGHTING
	float radiusmax = 0;

	clmodel = ent->model;

	custom_model = NULL;
	for (i = 0; i < sizeof (custom_model_colors) / sizeof (custom_model_colors[0]); ++i) {
		custom_model_color_t* test = &custom_model_colors[i];
		if (test->model_hint == clmodel->modhint) {
			if (test->color_cvar.string[0] && (test->amf_cvar == NULL || test->amf_cvar->integer == 0)) {
				custom_model = &custom_model_colors[i];
			}
			break;
		}
	}

	if (custom_model && custom_model->fullbright_cvar.integer) {
		ambientlight = 4096;
		shadelight = 0;
		full_light = true;
		return;
	}

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


/* FIXME: dimman... cache opt from fod */
	//VULT COLOURED MODEL LIGHTS
	if (amf_lighting_colour.value)
	{
		for (i = 0; i < MAX_DLIGHTS/32; i++) {
			if (cl_dlight_active[i]) {
				for (j = 0; j < 32; j++) {
					if ((cl_dlight_active[i]&(1<<j)) && i*32+j < MAX_DLIGHTS) {
						lnum = i*32 + j;

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

							if (cl_dlights[lnum].type == lt_custom) {
								VectorCopy(cl_dlights[lnum].color, dlight_color);
								VectorScale(dlight_color, (1.0/255), dlight_color); // convert color from byte to float
							}
							else
								VectorCopy(bubblecolor[cl_dlights[lnum].type], dlight_color);

							for (k=0;k<3;k++)
							{
								lightcolor[k] = lightcolor[k] + (dlight_color[k]*add)*2;
								if (lightcolor[k] > 256)
								{
									switch (k)
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
						}
					}
				}
			}
		}
	} else {
		for (i = 0; i < MAX_DLIGHTS/32; i++) {
			if (cl_dlight_active[i]) {
				for (j = 0; j < 32; j++) {
					if ((cl_dlight_active[i]&(1<<j)) && i*32+j < MAX_DLIGHTS) {
						lnum = i*32 + j;

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
	if ((ent->renderfx & RF_WEAPONMODEL) && ambientlight < 24)
		ambientlight = shadelight = 24;

	// never allow players to go totally black
	if (clmodel->modhint == MOD_PLAYER || ent->renderfx & RF_PLAYERMODEL) {
		if (ambientlight < 8)
			ambientlight = shadelight = 8;
	}


	if (clmodel->modhint == MOD_PLAYER || ent->renderfx & RF_PLAYERMODEL) {
		fbskins = bound(0, r_fullbrightSkins.value, r_refdef2.max_fbskins);
		if (fbskins == 1 && gl_fb_models.value == 1) {
			ambientlight = shadelight = 4096;
			full_light = true;
		}
		else if (fbskins == 0) {
			ambientlight = max(ambientlight, 8);
			shadelight = max(shadelight, 8);
			full_light = (cl.teamfortress ? true : false); // full_light == true will turn off full bright texture during mdl rendering,
			// that right in TF, they do not use full bright skins.
		}
		else if (fbskins) {
			ambientlight = max(ambientlight, 8 + fbskins * 120);
			shadelight = max(shadelight, 8 + fbskins * 120);
			full_light = true;
		}
	}
	else if (
			!((clmodel->modhint == MOD_EYES || clmodel->modhint == MOD_BACKPACK) && strncasecmp(Rulesets_Ruleset(), "default", 7)) &&
			(gl_fb_models.integer == 1 && clmodel->modhint != MOD_GIB && clmodel->modhint != MOD_VMODEL && !(com_serveractive && cls.state == ca_active && !cl.deathmatch && maxclients.value == 1))
		) {
		ambientlight = shadelight = 4096;
	}

	minlight = cl.minlight;

	if (ambientlight < minlight)
		ambientlight = shadelight = minlight;
}

void R_DrawPowerupShell(int effects, int layer_no, float base_level, float effect_level,
		maliasframedesc_t *oldframe, maliasframedesc_t *frame, aliashdr_t *paliashdr)
{
	base_level = bound(0, base_level, 1);
	effect_level = bound(0, effect_level, 1);

	r_shellcolor[0] = r_shellcolor[1] = r_shellcolor[2] = base_level;

	if (effects & EF_RED)
		r_shellcolor[0] += effect_level;
	if (effects & EF_GREEN)
		r_shellcolor[1] += effect_level;
	if (effects & EF_BLUE)
		r_shellcolor[2] += effect_level;

	GL_DisableMultitexture();
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	R_SetupAliasFrame (oldframe, frame, paliashdr, false, layer_no == 1, false);
}

void R_DrawAliasModel(entity_t *ent)
{
	int i, anim, skinnum, texture, fb_texture, playernum = -1;
	float scale;
	vec3_t mins, maxs;
	aliashdr_t *paliashdr;
	model_t *clmodel;
	maliasframedesc_t *oldframe, *frame;
	cvar_t *cv = NULL;
	byte *color32bit = NULL;
	qbool outline = false;

	//	entity_t *self;
	//static sfx_t *step;//foosteps sounds, commented out
	//static int setstep;

	extern qbool RuleSets_DisallowModelOutline (model_t *mod);
	extern	cvar_t r_viewmodelsize, cl_drawgun;

	VectorCopy (ent->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	//TODO: use modhints here? 
	//VULT CORONAS	
	if (		
			(!strcmp (ent->model->name, "progs/flame.mdl") || 
			 !strcmp (ent->model->name, "progs/flame0.mdl") || 
			 !strcmp (ent->model->name, "progs/flame3.mdl") ) && amf_coronas.value )
	{
		//FIXME: This is slow and pathetic as hell, really we should just check the entity
		//alternativley add some kind of permanent client side TE for the torch
		NewStaticLightCorona (C_FIRE, ent->origin, ent);
	}

	if (ent->model->modhint == MOD_TELEPORTDESTINATION && amf_coronas.value)
	{
		NewStaticLightCorona (C_LIGHTNING, ent->origin, ent);
	}

	clmodel = ent->model;
	paliashdr = (aliashdr_t *) Mod_Extradata (ent->model);	//locate the proper data

	if (ent->frame >= paliashdr->numframes || ent->frame < 0) {
		if (ent->model->modhint != MOD_EYES)
			Com_DPrintf ("R_DrawAliasModel: no such frame %d\n", ent->frame);

		ent->frame = 0;
	}
	if (ent->oldframe >= paliashdr->numframes || ent->oldframe < 0) {
		if (ent->model->modhint != MOD_EYES)
			Com_DPrintf ("R_DrawAliasModel: no such oldframe %d\n", ent->oldframe);

		ent->oldframe = 0;
	}

	frame = &paliashdr->frames[ent->frame];
	oldframe = &paliashdr->frames[ent->oldframe];

	if (!r_lerpframes.value || ent->framelerp < 0 || ent->oldframe == ent->frame)
		r_framelerp = 1.0;
	else
		r_framelerp = min (ent->framelerp, 1);


	//culling
	if (!(ent->renderfx & RF_WEAPONMODEL)) {
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

	if (gl_fogenable.value)
		glEnable(GL_FOG);

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
	} else if (ent->renderfx & RF_WEAPONMODEL) {	
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

	r_modelalpha = ((ent->renderfx & RF_WEAPONMODEL) && gl_mtexable) ? bound(0, cl_drawgun.value, 1) : 1;
	//VULT MOTION TRAILS
	if (ent->alpha)
		r_modelalpha = ent->alpha;

	if(ent->scoreboard)
		playernum = ent->scoreboard - cl.players;

	// we can't dynamically colormap textures, so they are cached separately for the players.  Heads are just uncolored.
	if (!gl_nocolors.value) {
		if (playernum >= 0 && playernum < MAX_CLIENTS) {
			if (!ent->scoreboard->skin)
				CL_NewTranslation(playernum);
			texture    = playernmtextures[playernum];
			fb_texture = playerfbtextures[playernum];
		}
	}
	if (full_light || !gl_fb_models.value)
		fb_texture = 0;

	if (gl_smoothmodels.value)
		glShadeModel (GL_SMOOTH);

	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	if ((ent->model->modhint == MOD_PLAYER || ent->renderfx & RF_PLAYERMODEL)
			&& playernum >= 0 && playernum < MAX_CLIENTS)
	{
		if (cl.teamplay && strcmp(cl.players[playernum].team, TP_SkinForcingTeam()) == 0)
			cv = &r_teamskincolor;
		else 
			cv = &r_enemyskincolor;
	}

	if (cv && cv->string[0])
		color32bit = cv->color;

	r_modelcolor[0] = -1;  // by default no solid fill color for model, using texture

	// Check for outline on models.
	// We don't support outline for transparent models,
	// and we also check for ruleset, since we don't want outline on eyes.
	outline = (gl_outline.integer && r_modelalpha == 1 && !RuleSets_DisallowModelOutline(clmodel));

	if (color32bit) {
		//
		// seems we select force some color for such model
		//

		for (i = 0; i < 3; i++) {
			r_modelcolor[i] = (float)color32bit[i] / 255.0;
			r_modelcolor[i] = bound(0, r_modelcolor[i], 1);
		}

		GL_DisableMultitexture();
		GL_Bind (r_skincolormode.integer ? texture : particletexture); // particletexture is just solid white texture

		//
		// we may use different methods for filling model surfaces, mixing(modulate), replace, add etc..
		//	
		switch(r_skincolormode.integer) {
			case 1:		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);	break;
			case 2:		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);		break;
			case 3:		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);		break;
			case 4:		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);		break;
			default:	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);	break;
		}

		R_SetupAliasFrame (oldframe, frame, paliashdr, false, false, outline);

		r_modelcolor[0] = -1;  // by default no solid fill color for model, using texture
	}
	else
	{
		if (fb_texture && gl_mtexable) {

			GL_DisableMultitexture ();

			GL_Bind (texture);
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

			GL_EnableMultitexture ();
			GL_Bind (fb_texture);

			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

			R_SetupAliasFrame (oldframe, frame, paliashdr, true, false, outline);

			GL_DisableMultitexture ();
		} 
		else 
		{
			GL_DisableMultitexture();
			GL_Bind (texture);

			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

			R_SetupAliasFrame (oldframe, frame, paliashdr, false, false, outline);

			if (fb_texture) {
				glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
				GL_Bind (fb_texture);

				glEnable (GL_BLEND);

				R_SetupAliasFrame (oldframe, frame, paliashdr, false, false, false);

				glDisable (GL_BLEND);
			}
		}
	}

	// FIXME: think need put it after caustics
	if (bound(0, gl_powerupshells.value, 1))
	{
		// always allow powerupshells for specs or demos.
		// do not allow powerupshells for eyes in other cases
		if ( ( cls.demoplayback || cl.spectator ) || ent->model->modhint != MOD_EYES )
		{
			if ((ent->effects & EF_RED) || (ent->effects & EF_GREEN) || (ent->effects & EF_BLUE)) {
				R_DrawPowerupShell(ent->effects, 0, gl_powerupshells_base1level.value,
						gl_powerupshells_effect1level.value, oldframe, frame, paliashdr);
				R_DrawPowerupShell(ent->effects, 1, gl_powerupshells_base2level.value,
						gl_powerupshells_effect2level.value, oldframe, frame, paliashdr);
			}

			memset(r_shellcolor, 0, sizeof(r_shellcolor));
		}
	}

	// Underwater caustics on alias models of QRACK -->
#define GL_RGB_SCALE 0x8573

	if ((gl_caustics.value) && (underwatertexture && gl_mtexable && ISUNDERWATER(TruePointContents(ent->origin))))
	{
		GL_EnableMultitexture ();
		glBindTexture (GL_TEXTURE_2D, underwatertexture);

		glMatrixMode (GL_TEXTURE);
		glLoadIdentity ();
		glScalef (0.5, 0.5, 1);
		glRotatef (r_refdef2.time * 10, 1, 0, 0);
		glRotatef (r_refdef2.time * 10, 0, 1, 0);
		glMatrixMode (GL_MODELVIEW);

		GL_Bind (underwatertexture);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);        
		glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
		glEnable (GL_BLEND);

		R_SetupAliasFrame (oldframe, frame, paliashdr, true, false, false);

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_BLEND);            

		GL_SelectTexture(GL_TEXTURE1);
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
	if ((r_shadows.value && !full_light && !(ent->renderfx & RF_NOSHADOW)) && !ent->alpha) {
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

	if (gl_fogenable.value)
		glDisable(GL_FOG);
}

static qbool R_DrawTrySimpleItem(void)
{
	int sprtype = gl_simpleitems_orientation.integer;
	float sprsize = bound(1, gl_simpleitems_size.value, 16), autorotate;
	int simpletexture;
	vec3_t point, right, up, org, offset;

	if (!currententity || !currententity->model)
		return false;

	if (currententity->skinnum < 0 || currententity->skinnum >= MAX_SIMPLE_TEXTURES)
		simpletexture = currententity->model->simpletexture[0]; // ah...
	else
		simpletexture = currententity->model->simpletexture[currententity->skinnum];

	if (!simpletexture)
		return false;

	autorotate = anglemod(100 * cl.time);

	if (sprtype == SPR_ORIENTED)
	{
		// bullet marks on walls
		vec3_t angles;
		angles[0] = angles[2] = 0;
		angles[1] = autorotate;
		AngleVectors (angles, NULL, right, up);
	} 
	else if (sprtype == SPR_FACING_UPRIGHT)
	{
		VectorSet (up, 0, 0, 1);
		right[0] = currententity->origin[1] - r_origin[1];
		right[1] = -(currententity->origin[0] - r_origin[0]);
		right[2] = 0;
		VectorNormalizeFast (right);
	} 
	else if (sprtype == SPR_VP_PARALLEL_UPRIGHT)
	{
		VectorSet (up, 0, 0, 1);
		VectorCopy (vright, right);
	}
	else
	{	// normal sprite
		VectorCopy (vup, up);
		VectorCopy (vright, right);
	}

	VectorCopy(currententity->origin, org);
	// brush models require some additional centering
	if (currententity->model->type == mod_brush)
	{
		extern cvar_t cl_model_bobbing;

		VectorSubtract(currententity->model->maxs, currententity->model->mins, offset);
		offset[2] = 0;
		VectorMA(org, 0.5, offset, org);

		if (cl_model_bobbing.value)
			org[2] += sin(autorotate / 90 * M_PI) * 5 + 5;
	}
	org[2] += sprsize;

	glPushAttrib(GL_ENABLE_BIT);

	glDisable(GL_CULL_FACE);
	glEnable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);

	GL_Bind(simpletexture);

	glBegin (GL_QUADS);

	glTexCoord2f (0, 1);
	VectorMA (org, -sprsize, up, point);
	VectorMA (point, -sprsize, right, point);
	glVertex3fv (point);

	glTexCoord2f (0, 0);
	VectorMA (org, sprsize, up, point);
	VectorMA (point, -sprsize, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 0);
	VectorMA (org, sprsize, up, point);
	VectorMA (point, sprsize, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 1);
	VectorMA (org, -sprsize, up, point);
	VectorMA (point, sprsize, right, point);
	glVertex3fv (point);

	glEnd ();

	glPopAttrib();

	return true;
}

void R_DrawEntitiesOnList(visentlist_t *vislist)
{
	int i;

	if (!r_drawentities.value || !vislist->count)
		return;

	if (vislist->alpha)
		glEnable (GL_ALPHA_TEST);

	// draw sprites separately, because of alpha_test
	for (i = 0; i < vislist->count; i++) 
	{
		currententity = &vislist->list[i];

		if (gl_simpleitems.value && R_DrawTrySimpleItem())
			continue;

		switch (currententity->model->type) 
		{
			case mod_alias:
				// VULT NAILTRAIL - Hidenails
				if (amf_hidenails.value && currententity->model->modhint == MOD_SPIKE)
					break;
				// VULT ROCKETTRAILS - Hide rockets
				if (amf_hiderockets.value && currententity->model->flags & EF_ROCKET)
					break;
				// VULT CAMERAS - Show/Hide playermodel
				if (currententity->alpha == -1)
				{
					if (cameratype == C_NORMAL)
						break;
					else
						currententity->alpha = 1;
				}
				// VULT MOTION TRAILS
				if (currententity->alpha < 0)
					break;

				// Handle flame/flame0 model changes
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
							if (!ISPAUSED)
								ParticleFire (currententity->origin);
						}
						else if (!strcmp(currententity->model->name, "progs/flame.mdl")
								&& cl_flame0_model /* do we have progs/flame0.mdl? */)
						{
							if (!ISPAUSED)
								ParticleFire (currententity->origin);
							currententity->model = cl_flame0_model;
						}
						else if (!strcmp(currententity->model->name, "progs/flame2.mdl") || !strcmp(currententity->model->name, "progs/flame3.mdl"))
						{
							if (!ISPAUSED)
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

				// Get rid of Z-fighting for textures by offsetting the
				// drawing of entity models compared to normal polygons.
				// dimman: disabled for qcon
				if(gl_brush_polygonoffset.value > 0 && Ruleset_AllowPolygonOffset(currententity)) {
					GL_PolygonOffset(0.05, bound(0, (float)gl_brush_polygonoffset.value, 25.0));
					R_DrawBrushModel(currententity);
					GL_PolygonOffset(0, 0);
				} else {
					R_DrawBrushModel(currententity);
				}

				break;
			case mod_sprite:
				R_DrawSpriteModel (currententity);
				break;
				// not handled
			case mod_spr32:
				break;
		}
	}

	if (vislist->alpha)
		glDisable (GL_ALPHA_TEST);
}

void R_DrawViewModel(void)
{
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
	gun.renderfx = RF_WEAPONMODEL | RF_NOSHADOW;
	if (r_lerpmuzzlehack.value) {
		if (cent->current.modelindex != cl_modelindices[mi_vaxe] &&
				cent->current.modelindex != cl_modelindices[mi_vbio] &&
				cent->current.modelindex != cl_modelindices[mi_vgrap] &&
				cent->current.modelindex != cl_modelindices[mi_vknife] &&
				cent->current.modelindex != cl_modelindices[mi_vknife2] &&
				cent->current.modelindex != cl_modelindices[mi_vmedi] &&
				cent->current.modelindex != cl_modelindices[mi_vspan])
		{
			gun.renderfx |= RF_LIMITLERP;			
			r_lerpdistance =  135;
		}
	}

	gun.effects |= (cl.stats[STAT_ITEMS] & IT_QUAD) ? EF_BLUE : 0;
	gun.effects |= (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) ? EF_RED : 0;
	gun.effects |= (cl.stats[STAT_ITEMS] & IT_SUIT) ? EF_GREEN : 0;

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
}


void R_PolyBlend(void)
{
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

void R_BrightenScreen(void)
{
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
	while (f > 1) 
	{
		if (f >= 2)
		{
			glColor3ubv (color_white);
		}
		else
		{
			glColor3f (f - 1, f - 1, f - 1);
		}

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

int SignbitsForPlane(mplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test
	bits = 0;
	for (j = 0; j < 3; j++) {
		if (out->normal[j] < 0)
			bits |= 1 << j;
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

void MYgluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar)
{
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

void R_SetViewports(int glx, int x, int gly, int y2, int w, int h, float max) 
{
	//
	// Setup Multiview-viewports
	//
	if (max == 1) 
	{
		glViewport (glx + x, gly + y2, w, h);
		return;
	}
	else if (max == 2 && cl_mvinset.value) 
	{
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
	else if (max == 2 && !cl_mvinset.value) 
	{
		if (CURRVIEW == 2)
			glViewport (0, h/2, w, h/2);
		else if (CURRVIEW == 1)
			glViewport (0, 0, w, h/2-1);
		else 
			Com_Printf("ERROR!\n");
		return;

	}
	else if (max == 3) 
	{
		if (CURRVIEW == 2)
			glViewport (0, h/2, w, h/2);
		else if (CURRVIEW == 3)
			glViewport (0, 0, w/2, h/2-1);
		else
			glViewport (w/2, 0, w/2, h/2-1);
		return;
	}
	else 
	{
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

void R_SetupGL(void)
{
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

	// Multiview
	if (CURRVIEW && cl_multiview.value && cls.mvdplayback)
	{
		R_SetViewports(glx, x, gly, y2, w, h, cl_multiview.value);
	}

	if (!cl_multiview.value || !cls.mvdplayback)
	{
		glViewport (glx + x, gly + y2, w, h);
	}

	farclip = max((int) r_farclip.value, 4096);

	screenaspect = (float)r_refdef.vrect.width/r_refdef.vrect.height;

	MYgluPerspective (r_refdef.fov_y, screenaspect, r_nearclip.value, farclip);

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

	if(gl_gammacorrection.integer)
	{
		glEnable(GL_FRAMEBUFFER_SRGB);
	}
	else
	{
		glDisable(GL_FRAMEBUFFER_SRGB);
	}
}

void R_Init(void)
{
	int i;

	Cmd_AddCommand ("loadsky", R_LoadSky_f);
	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f);
#ifndef CLIENTONLY
	Cmd_AddCommand ("pointfile", R_ReadPointFile_f);
#endif
	Cmd_AddCommand ("gl_checkmodels", CheckModels_f);
	Cmd_AddCommand ("gl_inferno", InfernoFire_f);
	Cmd_AddCommand ("gl_setmode", Amf_SetMode_f);

	Cvar_SetCurrentGroup(CVAR_GROUP_EYECANDY);
	Cvar_Register (&r_bloom);
	Cvar_Register (&r_bloom_darken);
	Cvar_Register (&r_bloom_alpha);
	Cvar_Register (&r_bloom_diamond_size);
	Cvar_Register (&r_bloom_intensity);
	Cvar_Register (&r_bloom_sample_size);
	Cvar_Register (&r_bloom_fast_sample);
	Cvar_Register (&r_drawentities);
	Cvar_Register (&r_lerpframes);
	Cvar_Register (&r_lerpmuzzlehack);
	Cvar_Register (&r_drawflame);
	Cvar_Register (&gl_detail);
	Cvar_Register (&gl_powerupshells);
	Cvar_Register (&gl_powerupshells_style);
	Cvar_Register (&gl_powerupshells_size);
	Cvar_Register (&gl_powerupshells_base1level);
	Cvar_Register (&gl_powerupshells_effect1level);
	Cvar_Register (&gl_powerupshells_base2level);
	Cvar_Register (&gl_powerupshells_effect2level);

	Cvar_Register (&gl_simpleitems);
	Cvar_Register (&gl_simpleitems_size);
	Cvar_Register (&gl_simpleitems_orientation);

	Cvar_Register (&gl_motion_blur);
	Cvar_Register (&gl_motion_blur_fps);
	Cvar_Register (&gl_motion_blur_norm);
	Cvar_Register (&gl_motion_blur_hurt);
	Cvar_Register (&gl_motion_blur_dead);

	Cvar_SetCurrentGroup(CVAR_GROUP_PARTICLES);
	Cvar_Register (&gl_solidparticles);
	Cvar_Register (&gl_squareparticles);
	Cvar_Register (&gl_part_explosions);
	Cvar_Register (&gl_part_trails);
	Cvar_Register (&gl_part_tracer1_color);
	Cvar_Register (&gl_part_tracer1_size);
	Cvar_Register (&gl_part_tracer1_time);
	Cvar_Register (&gl_part_tracer2_color);
	Cvar_Register (&gl_part_tracer2_size);
	Cvar_Register (&gl_part_tracer2_time);
	Cvar_Register (&gl_part_spikes);
	Cvar_Register (&gl_part_gunshots);
	Cvar_Register (&gl_part_blood);
	Cvar_Register (&gl_part_telesplash);
	Cvar_Register (&gl_part_blobs);
	Cvar_Register (&gl_part_lavasplash);
	Cvar_Register (&gl_part_inferno);
	Cvar_Register (&gl_particle_style);

	Cvar_Register (&gl_part_detpackexplosion_fire_color);
	Cvar_Register (&gl_part_detpackexplosion_ray_color);

	Cvar_SetCurrentGroup(CVAR_GROUP_TURB);
	Cvar_Register (&r_skyname);
	Cvar_Register (&r_fastsky);
	Cvar_Register (&r_skycolor);
	Cvar_Register (&r_fastturb);

	Cvar_Register (&r_telecolor);
	Cvar_Register (&r_lavacolor);
	Cvar_Register (&r_slimecolor);
	Cvar_Register (&r_watercolor);

	Cvar_Register (&r_novis);
	Cvar_Register (&r_wateralpha);
	Cvar_Register (&gl_caustics);
	if (!COM_CheckParm ("-nomtex")) {
		Cvar_Register (&gl_waterfog);
		Cvar_Register (&gl_waterfog_density);
		Cvar_Register (&gl_waterfog_color_water);
		Cvar_Register (&gl_waterfog_color_lava);
		Cvar_Register (&gl_waterfog_color_slime);
	}

	Cvar_Register (&gl_fogenable); 
	Cvar_Register (&gl_fogstart); 
	Cvar_Register (&gl_fogend); 
	Cvar_Register (&gl_fogsky);
	Cvar_Register (&gl_fogred); 
	Cvar_Register (&gl_fogblue);
	Cvar_Register (&gl_foggreen);

	Cvar_SetCurrentGroup(CVAR_GROUP_BLEND);
	Cvar_Register (&gl_polyblend);

	Cvar_SetCurrentGroup(CVAR_GROUP_SKIN);
	Cvar_Register (&r_fullbrightSkins);
	Cvar_Register (&r_enemyskincolor);
	Cvar_Register (&r_teamskincolor);
	Cvar_Register (&r_skincolormode);

	Cvar_SetCurrentGroup(CVAR_GROUP_LIGHTING);
	Cvar_Register (&r_dynamic);
	Cvar_Register (&gl_fb_bmodels);
	Cvar_Register (&gl_fb_models);
	Cvar_Register (&gl_lightmode);
	Cvar_Register (&gl_flashblend);
	Cvar_Register (&gl_rl_globe);
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
	Cvar_Register (&gl_textureless); //Qrack

	for (i = 0; i < sizeof (custom_model_colors) / sizeof (custom_model_colors[0]); ++i) {
		Cvar_Register (&custom_model_colors[i].color_cvar);
		Cvar_Register (&custom_model_colors[i].fullbright_cvar);
	}

	Cvar_SetCurrentGroup(CVAR_GROUP_OPENGL);
	Cvar_Register (&r_farclip);
	Cvar_Register (&gl_smoothmodels);
	Cvar_Register (&gl_affinemodels);
	Cvar_Register (&gl_clear);
	Cvar_Register (&gl_clearColor);
	Cvar_Register (&gl_cull);

	Cvar_Register(&gl_brush_polygonoffset);

	Cvar_Register (&gl_nocolors);
	Cvar_Register (&gl_finish);
	Cvar_Register (&gl_gammacorrection);
	Cvar_Register (&gl_modulate);

	Cvar_Register (&gl_outline);
	Cvar_Register (&gl_outline_width);

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register (&r_speeds);
	Cvar_Register (&r_netgraph);
	Cvar_Register (&r_netstats);

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

	if (!hud_netgraph)
		hud_netgraph = HUD_Register("netgraph", /*"r_netgraph"*/ NULL, "Shows your network conditions in graph-form. With netgraph you can monitor your latency (ping), packet loss and network errors.",
				HUD_PLUSMINUS | HUD_ON_SCORES, ca_onserver, 0, SCR_HUD_Netgraph,
				"0", "top", "left", "bottom", "0", "0", "0", "0 0 0", NULL,
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
	if (gl_vendor && !strcmp(gl_vendor, "METABYTE/WICKED3D")) 
		Cvar_SetDefault(&gl_solidparticles, 1); 

	R_InitTextures ();	// FIXME: not sure is this safe re-init
	R_InitBubble ();	// safe re-init
	R_InitParticles (); // safe re-init imo
	CI_Init ();			// safe re-init

	//VULT STUFF
	if (qmb_initialized)
	{
		InitVXStuff(); // safe re-init imo
	}
	else
		; // FIXME: hm, in case of vid_restart, what we must do if before vid_restart qmb_initialized was true?

	InitTracker();

	R_InitOtherTextures (); // safe re-init

	R_InitBloomTextures();
}

void R_RenderScene(void)
{
	extern void Skins_PreCache(void);

	vec3_t		colors;

	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	R_Check_R_FullBright(); // check for changes in r_fullbright

	R_MarkLeaves ();	// done here so we know if we're in water

	Skins_PreCache ();  // preache skins if needed

	R_DrawWorld ();		// adds static entities to the list

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
	{
		glDisable(GL_FOG);
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

	glClearColor (clearColor[0], clearColor[1], clearColor[2], 1.0);
}

void R_Clear(void)
{
	int clearbits = 0;

	// This used to cause a bug with some graphics cards when
	// in multiview mode. It would clear all but the last
	// drawn views.
	if (!cl_multiview.value && (gl_clear.value || (!vid_hwgamma_enabled && v_contrast.value > 1)))
	{
		clearbits |= GL_COLOR_BUFFER_BIT;
	}

	if (gl_clear.value)
	{
		if (gl_fogenable.value)
			glClearColor(gl_fogred.value,gl_foggreen.value,gl_fogblue.value,0.5);//Tei custom clear color
		else
			glClearColor (clearColor[0], clearColor[1], clearColor[2], 1.0);
	}

	clearbits |= GL_DEPTH_BUFFER_BIT;
	glClear (clearbits);
	gldepthmin = 0;
	gldepthmax = 1;
	glDepthFunc (GL_LEQUAL);

	glDepthRange (gldepthmin, gldepthmax);
}

// player velocity is drawn on screen
// as 3d vector and its projections
static void draw_velocity_3d(void)
{
	extern cvar_t show_velocity_3d_offset_forward;
	extern cvar_t show_velocity_3d_offset_down;
	extern cvar_t show_velocity_3d;

	vec3_t *origin = &r_refdef.vieworg;
	vec3_t *angles = &r_refdef.viewangles;

	const float vx = cl.simvel[0];
	const float vy = cl.simvel[1];
	const float vz = cl.simvel[2];

	const float yaw_degrees = (*angles)[YAW];
	const float yaw = DEG2RAD(yaw_degrees);

	const double c = cos(yaw);
	const double s = sin(yaw);

	const double scale_factor = 0.04;
	const float v_side = (float) (scale_factor * (-vx * s + vy * c));
	const float v_forward = (float) (scale_factor * (vx * c + vy * s));
	const float v_up = (float) (scale_factor * vz);

	const float line_width = 10.f;
	const float stipple_line_width = 5.f;
	const float stipple_line_colour[3] = { 0.5f, 0.5f, 0.5f };
	const vec3_t v3_zero = {0.f, 0.f, 0.f };

	glPushMatrix();

	glTranslatef((*origin)[0], (*origin)[1], (*origin)[2]);
	glRotatef(yaw_degrees, 0.f, 0.f, 1.f);
	glTranslatef(show_velocity_3d_offset_forward.value,
			0.f, -show_velocity_3d_offset_down.value);

	glPushAttrib(GL_LINE_BIT | GL_TEXTURE_BIT);

	glDisable(GL_TEXTURE_2D);

	switch (show_velocity_3d.integer)
	{
		case 1:                    //show vertical
			glEnable(GL_LINE_STIPPLE);
			glLineStipple(1, 0xFF00);
			glLineWidth(stipple_line_width);

			glColor3fv(stipple_line_colour);
			glBegin(GL_LINES);
			glVertex3f(v_forward, v_side, 0.f);
			glVertex3f(v_forward, v_side, v_up);
			glEnd();

			glDisable(GL_LINE_STIPPLE);
			glLineWidth(line_width);
			glColor3f(0.f, 1.f, 0.f);

			glBegin(GL_LINES);
			glVertex3fv(v3_zero);
			glVertex3f(v_forward, v_side, v_up);
			glEnd();
			//no break here

		case 2:                    //show horizontal velocity only
			glColor3f(1.f, 0.f, 0.f);
			glLineWidth(line_width);
			glBegin(GL_LINES);
			glVertex3fv(v3_zero);
			glVertex3f(v_forward, v_side, 0.f);
			glEnd();

			glEnable(GL_LINE_STIPPLE);
			glLineStipple(1, 0xFF00);
			glColor3fv(stipple_line_colour);
			glLineWidth(stipple_line_width);

			glBegin(GL_LINE_LOOP);
			glVertex3fv(v3_zero);
			glVertex3f(0.f, v_side, 0.f);
			glVertex3f(v_forward, v_side, 0.f);
			glVertex3f(v_forward, 0.f, 0.f);
			glEnd();

		default:
			break;
	}

	glPopAttrib();
	glPopMatrix();
}

/*
   Motion blur effect.
   Stolen from FTE engine.
   */
static void R_RenderSceneBlurDo(float alpha)
{
	static double last_time;
	double current_time = Sys_DoubleTime(), diff_time = current_time - last_time;
	double fps = gl_motion_blur_fps.value > 0 ? gl_motion_blur_fps.value : 77;
	qbool draw = (alpha >= 0); // negative alpha mean we don't draw anything but copy screen only.

	int vwidth = 1, vheight = 1;
	float vs, vt, cs, ct;

	// Remember all attributes.
	glPushAttrib(GL_ALL_ATTRIB_BITS);

	// alpha more than 0.5 are wrong.
	alpha = bound(0.1, alpha, 0.5);

	if (gl_support_arb_texture_non_power_of_two)
	{	//we can use any size, supposedly
		vwidth = glwidth;
		vheight = glheight;
	}
	else
	{	//limit the texture size to square and use padding.
		while (vwidth < glwidth)
			vwidth *= 2;
		while (vheight < glheight)
			vheight *= 2;
	}

	glViewport (0, 0, glwidth, glheight);

	GL_Bind(sceneblur_texture);

	// go 2d
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity ();
	glOrtho(0, glwidth, 0, glheight, -99999, 99999);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity ();

	//blend the last frame onto the scene
	//the maths is because our texture is over-sized (must be power of two)
	cs = vs = (float)glwidth / vwidth * 0.5;
	ct = vt = (float)glheight / vheight * 0.5;
	// qqshka: I don't get what is gl_motionblurscale, so simply removed it.
	vs *= 1;//gl_motionblurscale.value;
	vt *= 1;//gl_motionblurscale.value;

	glDisable(GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_BLEND);

	glColor4f(1, 1, 1, alpha);

	if (draw)
	{
		glBegin(GL_QUADS);
		glTexCoord2f(cs-vs, ct-vt);
		glVertex2f(0, 0);
		glTexCoord2f(cs+vs, ct-vt);
		glVertex2f(glwidth, 0);
		glTexCoord2f(cs+vs, ct+vt);
		glVertex2f(glwidth, glheight);
		glTexCoord2f(cs-vs, ct+vt);
		glVertex2f(0, glheight);
		glEnd();
	}

	// Restore matrices.
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	// With high frame rate frames difference is soo smaaaal, so motion blur almost unnoticeable,
	// so I copy frame not every frame.
	if (diff_time >= 1.0 / fps)
	{
		last_time = current_time;

		//copy the image into the texture so that we can play with it next frame too!
		glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, vwidth, vheight, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	// Restore attributes.
	glPopAttrib();
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

void R_RenderView(void)
{
	extern void DrawCI (void);

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

	{
		extern cvar_t show_velocity_3d;
		if(show_velocity_3d.integer) draw_velocity_3d();
	}

	SCR_SetupAutoID ();

	if (cl_multiview.value && cls.mvdplayback)
	{
		// Only bloom when we have drawn all views when in multiview.
		if (CURRVIEW == 1)
		{
			R_RenderSceneBlur();
			R_BloomBlend();
		}
	} 
	else
	{
		// Normal, bloom on each frame.
		R_RenderSceneBlur();
		R_BloomBlend();
	}

	if (r_speeds.value) {
		time2 = Sys_DoubleTime ();
		Print_flags[Print_current] |= PR_TR_SKIP;
		Com_Printf ("%3i ms  %4i wpoly %4i epoly\n", (int)((time2 - time1) * 1000), c_brush_polys, c_alias_polys); 
	}
}

