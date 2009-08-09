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

	$Id: gl_local.h,v 1.35 2007-09-17 20:13:52 qqshka Exp $

*/
// gl_local.h -- private refresh defs
#ifndef __GL_LOCAL_H__
#define __GL_LOCAL_H__

#ifdef GLSL
#include "glew.h"
#endif // GLSL

#ifdef __APPLE__

#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <OpenGL/glext.h>
#include "gl_model.h"

#else // __APPLE__

#include <GL/gl.h>
#include <GL/glu.h>

#ifdef __GNUC__
#include <GL/glext.h>
#endif // __GNUC__

#ifdef FRAMEBUFFERS
#include "GL/glext.h"
#endif

#ifndef _WIN32
#include <GL/glx.h>
#endif // _WIN32
#endif // __APPLE__

#ifdef GLSL
#ifdef  _WIN32
    #include    "wglew.h"
#else
    #include    "glxew.h"
#endif
#endif // GLSL


#include "gl_texture.h"
#ifdef FRAMEBUFFERS
#include "gl_framebuffer.h"
#endif

#ifndef APIENTRY
#define APIENTRY
#endif

void GL_BeginRendering (int *x, int *y, int *width, int *height);
void GL_EndRendering (void);

typedef struct {
	float	x, y, z;
	float	s, t;
	float	r, g, b;
} glvert_t;

extern glvert_t glv;

extern	int glx, gly, glwidth, glheight;

#define ALIAS_BASE_SIZE_RATIO		(1.0 / 11.0)
					// normalizing factor so player model works out to about
					//  1 pixel per triangle
#define	MAX_LBM_HEIGHT		480

#define SKYSHIFT		7
#define	SKYSIZE			(1 << SKYSHIFT)
#define SKYMASK			(SKYSIZE - 1)

#define BACKFACE_EPSILON	0.01

void R_TimeRefresh_f (void);
texture_t *R_TextureAnimation (texture_t *base);

//====================================================


void QMB_InitParticles(void);
void QMB_ClearParticles(void);
void QMB_DrawParticles(void);

void QMB_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count);
void QMB_ParticleTrail (vec3_t start, vec3_t end, vec3_t *, trail_type_t type);
void QMB_ParticleRailTrail (vec3_t start, vec3_t end, int color_num);
void QMB_BlobExplosion (vec3_t org);
void QMB_ParticleExplosion (vec3_t org);
void QMB_LavaSplash (vec3_t org);
void QMB_TeleportSplash (vec3_t org);

void QMB_DetpackExplosion (vec3_t org);

void QMB_InfernoFlame (vec3_t org);
void QMB_StaticBubble (entity_t *ent);

extern qbool qmb_initialized;


//====================================================

extern	entity_t	r_worldentity;
extern	qbool	r_cache_thrash;		// compatability
extern	vec3_t		modelorg, r_entorigin;
extern	entity_t	*currententity;
extern	int			r_visframecount;
extern	int			r_framecount;
extern	mplane_t	frustum[4];
extern	int			c_brush_polys, c_alias_polys;

// view origin
extern	vec3_t	vup;
extern	vec3_t	vpn;
extern	vec3_t	vright;
extern	vec3_t	r_origin;

// screen size info
extern	refdef_t	r_refdef;
extern	mleaf_t		*r_viewleaf, *r_oldviewleaf;
extern	mleaf_t		*r_viewleaf2, *r_oldviewleaf2;	// for watervis hack
extern	texture_t	*r_notexture_mip;
extern	int			d_lightstylevalue[256];	// 8.8 fraction of base light value

extern	int	particletexture;
extern	int	netgraphtexture;
extern	int	playertextures;
extern	int	playernmtextures[MAX_CLIENTS];
extern	int	playerfbtextures[MAX_CLIENTS];
extern	int	skyboxtextures;
extern	int	skytexturenum;		// index in cl.loadmodel, not gl texture object
extern	int underwatertexture, detailtexture;
extern	int shelltexture;

// Tomaz - Fog Begin
extern  cvar_t  gl_fogenable;
extern  cvar_t  gl_fogstart;
extern  cvar_t  gl_fogend;
extern  cvar_t  gl_fogred;
extern  cvar_t  gl_fogblue;
extern  cvar_t  gl_foggreen;
extern  cvar_t  gl_fogsky;
// Tomaz - Fog End

extern	cvar_t	r_drawentities;
extern	cvar_t	r_drawworld;
extern	cvar_t	r_drawflame;
extern	cvar_t	r_speeds;
extern	cvar_t	r_fullbright;
extern	cvar_t	r_lightmap;
extern	cvar_t	r_shadows;
extern	cvar_t	r_mirroralpha;
extern	cvar_t	r_wateralpha;
extern	cvar_t	r_dynamic;
extern	cvar_t	r_novis;
extern	cvar_t	r_netgraph;
extern	cvar_t	r_netstats;
extern	cvar_t	r_fullbrightSkins;
extern	cvar_t	r_enemyskincolor;
extern	cvar_t	r_teamskincolor;
extern	cvar_t	r_skincolormode;
extern	cvar_t	r_fastsky;
extern	cvar_t	r_skycolor;
extern	cvar_t	r_farclip;
extern	cvar_t	r_nearclip;
extern	cvar_t	r_drawflat;
extern	cvar_t	r_wallcolor;
extern	cvar_t	r_floorcolor;
extern	cvar_t	r_bloom;
extern	cvar_t	r_bloom_alpha;
extern	cvar_t	r_bloom_diamond_size;
extern	cvar_t	r_bloom_intensity;
extern	cvar_t	r_bloom_darken;
extern	cvar_t	r_bloom_sample_size;
extern	cvar_t	r_bloom_fast_sample;

extern	cvar_t	r_skyname;
extern  cvar_t  gl_caustics;
extern  cvar_t  gl_detail;
extern  cvar_t  gl_waterfog;
extern  cvar_t  gl_waterfog_density;

extern	cvar_t	gl_subdivide_size;
extern	cvar_t	gl_clear;
extern	cvar_t	gl_cull;
extern	cvar_t	gl_smoothmodels;
extern	cvar_t	gl_affinemodels;
extern	cvar_t	gl_polyblend;
extern	cvar_t	gl_flashblend;
extern	cvar_t	gl_rl_globe;
extern	cvar_t	gl_nocolors;
extern	cvar_t	gl_finish;
extern	cvar_t	gl_fb_bmodels;
extern	cvar_t	gl_fb_models;
extern	cvar_t	gl_lightmode;
extern  cvar_t  gl_solidparticles;
extern	cvar_t	gl_playermip;


extern  cvar_t gl_part_explosions;
extern  cvar_t gl_part_trails;
extern  cvar_t gl_part_spikes;
extern  cvar_t gl_part_gunshots;
extern  cvar_t gl_part_blood;
extern  cvar_t gl_part_telesplash;
extern  cvar_t gl_part_blobs;
extern  cvar_t gl_part_lavasplash;
extern	cvar_t gl_part_inferno;
extern	cvar_t gl_part_detpackexplosion_fire_color;
extern	cvar_t gl_part_detpackexplosion_ray_color;

extern	cvar_t gl_powerupshells;
extern	cvar_t gl_powerupshells_style;
extern	cvar_t gl_powerupshells_size;

extern cvar_t gl_max_size, gl_scaleModelTextures, gl_scaleTurbTextures, gl_miptexLevel;
extern cvar_t gl_externalTextures_world, gl_externalTextures_bmodels;

extern cvar_t vid_wideaspect;

extern	int		lightmode;		// set to gl_lightmode on mapchange

extern	float	r_world_matrix[16];

extern	const char *gl_vendor;
extern	const char *gl_renderer;
extern	const char *gl_version;
extern	const char *gl_extensions;

#define ISUNDERWATER(x) ((x) == CONTENTS_WATER || (x) == CONTENTS_SLIME || (x) == CONTENTS_LAVA)
//#define TruePointContents(p) CM_HullPointContents(&cl.worldmodel->hulls[0], 0, p)
#define TruePointContents(p) CM_HullPointContents(&cl.clipmodels[1]->hulls[0], 0, p) // ?TONIK?

// gl_warp.c
void GL_SubdivideSurface (msurface_t *fa);
void GL_BuildSkySurfacePolys (msurface_t *fa);
void EmitBothSkyLayers (msurface_t *fa);
void EmitWaterPolys (msurface_t *fa);
void EmitSkyPolys (msurface_t *fa, qbool mtex);
void CalcCausticTexCoords(float *v, float *s, float *t);
void EmitCausticsPolys (void);
void R_DrawSkyChain (void);
void R_DrawSky (void);
void R_LoadSky_f(void);
void R_AddSkyBoxSurface (msurface_t *fa);
void R_InitSky (texture_t *mt);	// called at level load

extern qbool	r_skyboxloaded;

// gl_draw.c
void GL_Set2D (void);

// gl_rmain.c
qbool R_CullBox (vec3_t mins, vec3_t maxs);
qbool R_CullSphere (vec3_t centre, float radius);
void R_RotateForEntity (entity_t *e);
void R_PolyBlend (void);
void R_BrightenScreen (void);
void R_DrawEntitiesOnList (visentlist_t *vislist);

// gl_rlight.c
void R_MarkLights (dlight_t *light, int bit, mnode_t *node);
void R_AnimateLight (void);
void R_RenderDlights (void);
int R_LightPoint (vec3_t p);

// gl_refrag.c
void R_StoreEfrags (efrag_t **ppefrag);

// gl_mesh.c
void GL_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr);

// gl_rsurf.c

#define	MAX_LIGHTMAPS		64

void EmitDetailPolys (void);
void R_DrawBrushModel (entity_t *e);
void R_DrawWorld (void);
void R_DrawWaterSurfaces (void);
void R_DrawAlphaChain (void);
void GL_BuildLightmaps (void);

// gl_ngraph.c
//void R_NetGraph (void); // HUD -> hexum
#define MAX_NET_GRAPHHEIGHT 256
void R_MQW_NetGraph(int outgoing_sequence, int incoming_sequence, int *packet_latency,
                int lost, int minping, int avgping, int maxping, int devping,
                int posx, int posy, int width, int height, int revx, int revy);

// gl_rmisc.c
void R_InitOtherTextures(void);

//vid_common_gl.c

//anisotropic filtering
#ifndef GL_EXT_texture_filter_anisotropic
#define GL_EXT_texture_filter_anisotropic 1
#define GL_TEXTURE_MAX_ANISOTROPY_EXT				0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT			0x84FF
#endif

//multitexturing
#define	GL_TEXTURE0_ARB 			0x84C0
#define	GL_TEXTURE1_ARB 			0x84C1
#define	GL_TEXTURE2_ARB 			0x84C2
#define	GL_TEXTURE3_ARB 			0x84C3
#define GL_MAX_TEXTURE_UNITS_ARB	0x84E2

//texture compression
#define GL_COMPRESSED_ALPHA_ARB					0x84E9
#define GL_COMPRESSED_LUMINANCE_ARB				0x84EA
#define GL_COMPRESSED_LUMINANCE_ALPHA_ARB		0x84EB
#define GL_COMPRESSED_INTENSITY_ARB				0x84EC
#define GL_COMPRESSED_RGB_ARB					0x84ED
#define GL_COMPRESSED_RGBA_ARB					0x84EE
#define GL_TEXTURE_COMPRESSION_HINT_ARB			0x84EF
#define GL_TEXTURE_IMAGE_SIZE_ARB				0x86A0
#define GL_TEXTURE_COMPRESSED_ARB				0x86A1
#define GL_NUM_COMPRESSED_TEXTURE_FORMATS_ARB	0x86A2
#define GL_COMPRESSED_TEXTURE_FORMATS_ARB		0x86A3

//combine extension
#define GL_COMBINE_EXT				0x8570
#define GL_COMBINE_RGB_EXT			0x8571
#define GL_RGB_SCALE_EXT			0x8573

typedef void (APIENTRY *lpMTexFUNC) (GLenum, GLfloat, GLfloat);
typedef void (APIENTRY *lpSelTexFUNC) (GLenum);

extern lpMTexFUNC qglMultiTexCoord2f;
extern lpSelTexFUNC qglActiveTexture;

extern float gldepthmin, gldepthmax;
extern byte color_white[4], color_black[4];
extern qbool gl_mtexable;
extern int gl_textureunits;
extern qbool gl_combine, gl_add_ext;
extern qbool gl_allow_ztrick;

qbool CheckExtension (const char *extension);
void Check_Gamma (unsigned char *pal);
void VID_SetPalette (unsigned char *palette);
void GL_Init (void);

#endif /* !__GL_LOCAL_H__ */
