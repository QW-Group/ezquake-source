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
// gl_local.h -- private refresh defs
#ifndef __GL_LOCAL_H__
#define __GL_LOCAL_H__

#ifdef __APPLE__

#include <OpenGL/gl.h>
#ifdef TRAVIS_BUILD
#include "opengl/glext.h"
#else
#include <OpenGL/glext.h>
#endif

#else // __APPLE__

#include <GL/gl.h>

#ifdef __GNUC__
#include <GL/glext.h>
#endif // __GNUC__

#ifdef _MSC_VER
#include <glext.h>
#endif

#ifndef _WIN32
#include <GL/glx.h>
#endif // _WIN32
#endif // __APPLE__

#include "gl_texture.h"

#ifndef APIENTRY
#define APIENTRY
#endif

//#define GL_PARANOIA

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
void QMB_CalculateParticles(void);
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
extern	vec3_t		modelorg;
extern	entity_t	*currententity;
extern	int			r_visframecount;
extern	int			r_framecount;
extern	mplane_t	frustum[4];

typedef enum {
	polyTypeWorldModel,
	polyTypeAliasModel,
	polyTypeBrushModel,

	polyTypeMaximum
} frameStatsPolyType;

typedef struct r_frame_stats_classic_s {
	int polycount[polyTypeMaximum];
} r_frame_stats_classic_t;

typedef struct r_frame_stats_modern_s {
	int world_batches;
	int buffer_uploads;
	int multidraw_calls;
} r_frame_stats_modern_t;

typedef struct r_frame_stats_s {
	r_frame_stats_classic_t classic;
	r_frame_stats_modern_t modern;

	int texture_binds;
	unsigned int lightmap_min_changed;
	unsigned int lightmap_max_changed;
	int lightmap_updates;
	int draw_calls;
	int subdraw_calls;

	double start_time;
	double end_time;
} r_frame_stats_t;

extern r_frame_stats_t frameStats, prevFrameStats;

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
extern	unsigned int d_lightstylevalue[256];	// 8.8 fraction of base light value

#define MAX_SKYBOXTEXTURES 6
extern	texture_ref netgraphtexture;
extern	texture_ref skyboxtextures[MAX_SKYBOXTEXTURES];
extern	texture_ref skytexturenum;		// index in cl.loadmodel, not gl texture object
extern	texture_ref underwatertexture, detailtexture, solidtexture;
extern	texture_ref shelltexture;

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
extern	cvar_t	r_drawflame;
extern	cvar_t	r_speeds;
extern	cvar_t	r_fullbright;
extern	cvar_t	r_shadows;
extern	cvar_t	r_mirroralpha;
extern	cvar_t	r_dynamic;
extern	cvar_t	r_novis;
extern	cvar_t	r_netgraph;
extern	cvar_t	r_netstats;
extern	cvar_t	r_fullbrightSkins;
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
extern	cvar_t	gl_polyblend;
extern	cvar_t	gl_flashblend;
extern	cvar_t	gl_rl_globe;
extern	cvar_t	gl_nocolors;
extern	cvar_t	gl_finish;
extern	cvar_t	gl_fb_bmodels;
extern	cvar_t	gl_fb_models;
extern	cvar_t	gl_lightmode;
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
extern  cvar_t gl_part_bubble;
extern	cvar_t gl_part_detpackexplosion_fire_color;
extern	cvar_t gl_part_detpackexplosion_ray_color;

extern	cvar_t gl_powerupshells;
extern	cvar_t gl_powerupshells_size;

extern cvar_t gl_gammacorrection;
extern cvar_t gl_modulate;

extern	int		lightmode;		// set to gl_lightmode on mapchange

extern	float	r_world_matrix[16];

extern	const char *gl_vendor;
extern	const char *gl_renderer;
extern	const char *gl_version;
extern	const char *gl_extensions;

#define ISUNDERWATER(contents) (contents == CONTENTS_WATER || contents == CONTENTS_SLIME || contents == CONTENTS_LAVA)
//#define TruePointContents(p) CM_HullPointContents(&cl.worldmodel->hulls[0], 0, p)
#define TruePointContents(p) CM_HullPointContents(&cl.clipmodels[1]->hulls[0], 0, p) // ?TONIK?

qbool R_PointIsUnderwater(vec3_t point);

// gl_chaticons.c
void DrawChatIcons(void);

// gl_warp.c
void GL_SubdivideSurface (msurface_t *fa);
void GL_BuildSkySurfacePolys (msurface_t *fa);
void EmitBothSkyLayers (msurface_t *fa);
void EmitWaterPolys (msurface_t *fa);
void EmitSkyPolys (msurface_t *fa, qbool mtex);
void R_DrawSky (void);
void R_LoadSky_f(void);
void R_AddSkyBoxSurface (msurface_t *fa);
void R_InitSky (texture_t *mt);	// called at level load
qbool R_DrawWorldOutlines(void);

extern qbool	r_skyboxloaded;

// gl_draw.c
void GL_Set2D (void);

// gl_rmain.c
qbool R_CullBox (vec3_t mins, vec3_t maxs);
qbool R_CullSphere (vec3_t centre, float radius);
void R_RotateForEntity (entity_t *e);
void R_PolyBlend (void);
void R_BrightenScreen (void);

#define POLYGONOFFSET_DISABLED 0
#define POLYGONOFFSET_STANDARD 1
#define POLYGONOFFSET_OUTLINES 2

void GL_PolygonOffset(int options);

// gl_rlight.c
void R_MarkLights (dlight_t *light, int bit, mnode_t *node);
void R_AnimateLight (void);
void R_RenderDlights (void);
int R_LightPoint (vec3_t p);

// gl_refrag.c
void R_StoreEfrags (efrag_t **ppefrag);

// gl_mesh.c
void GL_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr);
void GL_AliasModelAddToVBO(model_t* mod, aliashdr_t* hdr, vbo_model_vert_t* aliasModelBuffer, int position);
void GL_MD3ModelAddToVBO(model_t* mod, vbo_model_vert_t* aliasModelBuffer, int position);

// gl_rsurf.c

void GLC_EmitDetailPolys(qbool use_vbo);
void R_DrawBrushModel (entity_t *e);
void R_DrawWorld (void);
void R_DrawWaterSurfaces (void);
void GLC_DrawAlphaChain(msurface_t* alphachain, frameStatsPolyType polyType);
void GL_BuildLightmaps (void);

qbool R_FullBrightAllowed(void);
void R_Check_ReloadLightmaps(void);
extern int dlightcolor[NUM_DLIGHTTYPES][3];

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

//sRGB gamma correction
#define GL_SRGB8 0x8C41
#define GL_SRGB8_ALPHA8 0x8C43
#define GL_FRAMEBUFFER_SRGB 0x8DB9

typedef void (APIENTRY *glMultiTexCoord2f_t)(GLenum target, GLfloat s, GLfloat t);
typedef void (APIENTRY *glActiveTexture_t)(GLenum target);
typedef void (APIENTRY *glClientActiveTexture_t)(GLenum target);

extern glMultiTexCoord2f_t qglMultiTexCoord2f;
extern glActiveTexture_t qglActiveTexture;
extern glClientActiveTexture_t qglClientActiveTexture;

extern double gldepthmin, gldepthmax;
extern byte color_white[4], color_black[4];
extern qbool gl_mtexable;
extern int gl_textureunits;
extern qbool gl_support_arb_texture_non_power_of_two;

void Check_Gamma (unsigned char *pal);
void VID_SetPalette (unsigned char *palette);
void GL_Init (void);

qbool GLM_LoadProgramFunctions(void);
qbool GLM_LoadStateFunctions(void);
qbool GLM_LoadTextureManagementFunctions(void);
void GL_LoadTextureManagementFunctions(void);
qbool GLM_LoadDrawFunctions(void);
void GL_LoadDrawFunctions(void);
void GL_InitialiseDebugging(void);
void GL_CheckMultiTextureExtensions(void);

qbool GL_BuffersSupported(void);

extern cvar_t vid_renderer;
extern cvar_t vid_gl_core_profile;

// Which renderer to use
#define GL_UseGLSL()              (vid_renderer.integer == 1)
#define GL_UseImmediateMode()     (vid_renderer.integer == 0)

// Debug profile may or may not do anything, but if it does anything it's slower, so only enable in dev mode
#define GL_DebugProfileContext()  (IsDeveloperMode())

// 
#define GL_CoreProfileContext()   (vid_renderer.integer == 1 && vid_gl_core_profile.integer)

#ifdef __APPLE__
// https://www.khronos.org/opengl/wiki/OpenGL_Context
// "Recommendation: You should use the forward compatibility bit only if you need compatibility with MacOS.
//    That API requires the forward compatibility bit to create any core profile context."
#define GL_ForwardOnlyProfile()   (GL_CoreProfileContext())
#else
// There's no reason for this unless we need to check that we're not using deprecated functionality, so keep disabled
#define GL_ForwardOnlyProfile()   (GL_CoreProfileContext() && COM_CheckParm(cmdline_param_client_forwardonlyprofile))
#endif

void GL_OrthographicProjection(float left, float right, float top, float bottom, float zNear, float zFar);
void GL_TextureEnvMode(GLenum mode);
void GL_TextureEnvModeForUnit(GLenum unit, GLenum mode);

#define GL_ALPHATEST_NOCHANGE 0
#define GL_ALPHATEST_ENABLED  1
#define GL_ALPHATEST_DISABLED 2
#define GL_BLEND_NOCHANGE 0
#define GL_BLEND_ENABLED  4
#define GL_BLEND_DISABLED 8

int GL_AlphaBlendFlags(int modes);

void GLM_ScaleMatrix(float* matrix, float x_scale, float y_scale, float z_scale);
void GLM_TransformMatrix(float* matrix, float x, float y, float z);
void GLM_RotateMatrix(float* matrix, float angle, float x, float y, float z);
void GLM_RotateVector(vec3_t vector, float angle, float x, float y, float z);
void GLM_GetMatrix(GLenum type, float* matrix);

#define GLM_VERTEX_SHADER   0
#define GLM_FRAGMENT_SHADER 1
#define GLM_GEOMETRY_SHADER 2
#define GLM_COMPUTE_SHADER  3
#define GLM_SHADER_COUNT    4

typedef struct glm_program_s {
	GLuint vertex_shader;
	GLuint geometry_shader;
	GLuint fragment_shader;
	GLuint compute_shader;
	GLuint program;

	struct glm_program_s* next;
	const char* friendly_name;
	const char* shader_text[GLM_SHADER_COUNT];
	char* included_definitions;
	GLuint shader_length[GLM_SHADER_COUNT];
	qbool uniforms_found;

	unsigned int custom_options;
	qbool force_recompile;
} glm_program_t;

// Check if a program needs to be recompiled
qbool GLM_ProgramRecompileNeeded(const glm_program_t* program, unsigned int options);

// Flags all programs to be recompiled
// Doesn't immediately recompile, so safe to call during /exec, /cfg_load etc
void GLM_ForceRecompile(void);

qbool GLM_CreateVFProgram(
	const char* friendlyName,
	const char* vertex_shader_text,
	GLuint vertex_shader_text_length,
	const char* fragment_shader_text,
	GLuint fragment_shader_text_length,
	glm_program_t* program
);

qbool GLM_CreateVFProgramWithInclude(
	const char* friendlyName,
	const char* vertex_shader_text,
	GLuint vertex_shader_text_length,
	const char* fragment_shader_text,
	GLuint fragment_shader_text_length,
	glm_program_t* program,
	const char* included_definitions
);

qbool GLM_CompileComputeShaderProgram(glm_program_t* program, const char* shadertext, GLint length);

#define GL_VFDeclare(name) \
	extern unsigned char name##_vertex_glsl[];\
	extern unsigned int name##_vertex_glsl_len;\
	extern unsigned char name##_fragment_glsl[];\
	extern unsigned int name##_fragment_glsl_len;

#define GL_VFParams(name) \
	(const char*)name##_vertex_glsl,\
	name##_vertex_glsl_len,\
	(const char*)name##_fragment_glsl,\
	name##_fragment_glsl_len

qbool GLM_CreateVGFProgram(
	const char* friendlyName,
	const char* vertex_shader_text,
	GLuint vertex_shader_text_length,
	const char* geometry_shader_text,
	GLuint geometry_shader_text_length,
	const char* fragment_shader_text,
	GLuint fragment_shader_text_length,
	glm_program_t* program
);

qbool GLM_CreateVGFProgramWithInclude(
	const char* friendlyName,
	const char* vertex_shader_text,
	GLuint vertex_shader_text_length,
	const char* geometry_shader_text,
	GLuint geometry_shader_text_length,
	const char* fragment_shader_text,
	GLuint fragment_shader_text_length,
	glm_program_t* program,
	const char* included_definitions
);

#define GL_VGFDeclare(name) \
	extern unsigned char name##_vertex_glsl[];\
	extern unsigned int name##_vertex_glsl_len;\
	extern unsigned char name##_geometry_glsl[];\
	extern unsigned int name##_geometry_glsl_len;\
	extern unsigned char name##_fragment_glsl[];\
	extern unsigned int name##_fragment_glsl_len;
#define GL_VGFParams(name) \
	(const char*)name##_vertex_glsl,\
	name##_vertex_glsl_len,\
	(const char*)name##_geometry_glsl,\
	name##_geometry_glsl_len,\
	(const char*)name##_fragment_glsl,\
	name##_fragment_glsl_len

#define glEnable GL_Enable
#define glDisable GL_Disable
#define glBegin GL_Begin
#define glEnd GL_End
#define glVertex2f GL_Vertex2f
#define glVertex3f GL_Vertex3f
#define glVertex3fv GL_Vertex3fv

void GL_Begin(GLenum primitive);
void GL_End(void);
void GL_Enable(GLenum option);
void GL_Disable(GLenum option);
void GL_Vertex2f(GLfloat x, GLfloat y);
void GL_Vertex3f(GLfloat x, GLfloat y, GLfloat z);
void GL_Vertex3fv(const GLfloat* v);

void GL_GetMatrix(GLenum mode, GLfloat* matrix);
void GL_GetViewport(GLint* view);

void GL_IdentityProjectionView(void);
void GL_IdentityModelView(void);
void GL_Rotate(GLenum matrix, float angle, float x, float y, float z);
void GL_Scale(GLenum matrix, float xScale, float yScale, float zScale);
void GL_Translate(GLenum matrix, float x, float y, float z);
void GL_Frustum(double left, double right, double bottom, double top, double zNear, double zFar);

void GL_PopMatrix(GLenum mode, float* matrix);
void GL_PushMatrix(GLenum mode, float* matrix);

void GLM_DebugMatrix(GLenum type, const char* value);

int GL_PopulateVBOForBrushModel(model_t* m, void* vbo_buffer, int vbo_pos);
int GL_MeasureVBOSizeForBrushModel(model_t* m);
void GL_CreateAliasModelVBO(buffer_ref instance_vbo);
void GL_CreateBrushModelVBO(buffer_ref instance_vbo);

void GL_UseProgram(GLuint program);
void GL_DepthFunc(GLenum func);
void GL_DepthRange(double nearVal, double farVal);
void GL_CullFace(GLenum mode);
void GL_BlendFunc(GLenum sfactor, GLenum dfactor);
void GL_BindVertexArray(glm_vao_t* vao);
void GL_Viewport(GLint x, GLint y, GLsizei width, GLsizei height);
void GL_EnableFog(void);
void GL_DisableFog(void);
void GL_ConfigureFog(void);
void GL_EnableWaterFog(int contents);
void GL_InitTextureState(void);
void GL_DepthMask(GLboolean mask);
void GL_InvalidateTextureReferences(GLuint texture);
void GL_PolygonMode(GLenum mode);

// gl_buffers.c
void GL_InitialiseBufferHandling(void);
void GL_InitialiseBufferState(void);
buffer_ref GL_GenFixedBuffer(GLenum target, const char* name, GLsizei size, void* data, GLenum usage);
void GL_BindAndUpdateBuffer(buffer_ref vbo, size_t size, void* data);
void GL_UpdateBuffer(buffer_ref vbo, size_t size, void* data);
void GL_BindAndUpdateBufferSection(buffer_ref vbo, GLintptr offset, GLsizeiptr size, const GLvoid* data);
void GL_UpdateBufferSection(buffer_ref vbo, GLintptr offset, GLsizeiptr size, const GLvoid* data);
void GL_BindBuffer(buffer_ref ref);
void GL_BindBufferBase(buffer_ref ref, GLuint index);
void GL_BindBufferRange(buffer_ref ref, GLuint index, GLintptr offset, GLsizeiptr size);
void GL_UnBindBuffer(GLenum target);
buffer_ref GL_ResizeBuffer(buffer_ref vbo, size_t size, void* data);
void GL_EnsureBufferSize(buffer_ref ref, size_t size);
size_t GL_BufferSize(buffer_ref vbo);

#ifdef WITH_OPENGL_TRACE
#define ENTER_STATE GL_EnterTracedRegion(__FUNCTION__, true)
#define MIDDLE_STATE GL_MarkEvent(__FUNCTION__)
#define LEAVE_STATE GL_LeaveTracedRegion(true)
#define GL_EnterRegion(x) GL_EnterTracedRegion(x, false)
#define GL_LeaveRegion() GL_LeaveTracedRegion(false)
void GL_EnterTracedRegion(const char* regionName, qbool trace_only);
void GL_LeaveTracedRegion(qbool trace_only);
void GL_PrintState(FILE* output);
void GL_DebugState(void);
void GL_ResetRegion(qbool start);
void GL_LogAPICall(const char* message, ...);
void GL_MarkEvent(const char* message, ...);
qbool GL_LoggingEnabled(void);
void GL_ObjectLabel(GLenum identifier, GLuint name, GLsizei length, const char* label);
void GL_GetObjectLabel(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei* length, char* label);
#else
#define ENTER_STATE
#define MIDDLE_STATE
#define LEAVE_STATE
#define GL_EnterTracedRegion(...)
#define GL_LeaveTracedRegion(...)
#define GL_EnterRegion(x)
#define GL_LeaveRegion()
#define GL_ResetRegion(x)
#define GL_MarkEvent(...)
#define GL_LogAPICall(...)
#define GL_PrintState(...)
#define GL_DebugState()
#define GL_LoggingEnabled() (false)
#define GL_ObjectLabel(...)
#define GL_GetObjectLabel(...)
#endif

#define NUMVERTEXNORMALS 162
#define SHADEDOT_QUANT   64

#define	MD3_INTERP_MAXDIST  300

// 
#define CHARSET_CHARS_PER_ROW	16
#define CHARSET_WIDTH			1.0
#define CHARSET_HEIGHT			1.0
#define CHARSET_CHAR_WIDTH		(CHARSET_WIDTH / CHARSET_CHARS_PER_ROW)
#define CHARSET_CHAR_HEIGHT		(CHARSET_HEIGHT / CHARSET_CHARS_PER_ROW)

// Functions
void GLM_DrawSimpleItem(texture_ref texture_array, int texture_index, float scale_s, float scale_t, vec3_t origin, float scale, vec3_t up, vec3_t right);
void GLC_DrawSimpleItem(texture_ref simpletexture, vec3_t org, float sprsize, vec3_t up, vec3_t right);

void GL_BeginDrawSprites(void);
void GL_EndDrawSprites(void);
void GL_BeginDrawAliasModels(void);
void GL_EndDrawAliasModels(void);

void GLM_MultiplyMatrix(const float* lhs, const float* rhs, float* target);
void GLM_MultiplyVector(const float* matrix, const float* vector, float* result);
void GLM_MultiplyVector3f(const float* matrix, float x, float y, float z, float* result);
void GLM_MultiplyVector3fv(const float* matrix, const vec3_t vector, float* result);
void GLM_DrawWaterSurfaces(void);

void GL_BuildCommonTextureArrays(qbool vid_restart);
void GL_CreateModelVBOs(qbool vid_restart);

void R_DrawAliasModel(entity_t *ent);
void R_DrawAliasPowerupShell(entity_t *ent);

// 
void R_RenderDynamicLightmaps(msurface_t *fa);
void R_DrawViewModel(void);
void R_RenderAllDynamicLightmaps(model_t *model);
void R_LightmapFrameInit(void);
void R_UploadChangedLightmaps(void);

void GLM_RenderView(void);
void GLM_UploadFrameConstants(void);
void GLM_PrepareWorldModelBatch(void);
void GLM_Draw3DSprites(void);
void GLM_Prepare3DSprites(void);

void GLC_DrawMapOutline(model_t *model);
void R_SetupAliasFrame(model_t* model, maliasframedesc_t *oldframe, maliasframedesc_t *frame, qbool mtex, qbool scrolldir, qbool outline, texture_ref texture, texture_ref fb_texture, int effects, int render_effects);
int R_AliasFramePose(maliasframedesc_t* frame);
void GLC_DrawPowerupShell(model_t* model, int effects, maliasframedesc_t *oldframe, maliasframedesc_t *frame);

void GLM_EnterBatchedWorldRegion(void);
void GLM_DrawSpriteModel(entity_t* e);
void GLM_PolyBlend(float v_blend[4]);
void GLM_DrawVelocity3D(void);
void GLM_RenderSceneBlurDo(float alpha);
mspriteframe_t* R_GetSpriteFrame(entity_t *e, msprite2_t *psprite);

void GLC_ClearTextureChains(void);
void GLC_SetTextureLightmap(GLenum textureUnit, int lightmap_num);
void GLC_SetLightmapBlendFunc(void);
texture_ref GLC_LightmapTexture(int index);
texture_ref GLM_LightmapArray(void);
void GLC_ClearLightmapPolys(void);
void GLC_AddToLightmapChain(msurface_t* s);
void GLC_LightmapUpdate(int index);
void GLC_SetLightmapTextureEnvironment(GLenum textureUnit);
GLenum GLC_LightmapDestBlendFactor(void);
glpoly_t* GLC_LightmapChain(int i);
GLenum GLC_LightmapTexEnv(void);
int GLC_LightmapCount(void);
void GLM_CreateLightmapTextures(void);
void GLM_PostProcessScreen(void);
void GLC_CreateLightmapTextures(void);
void GLC_AliasModelShadow(entity_t* ent, aliashdr_t* paliashdr, vec3_t shadevector, vec3_t lightspot);
void GLC_UnderwaterCaustics(entity_t* ent, model_t* clmodel, maliasframedesc_t* oldframe, maliasframedesc_t* frame, aliashdr_t* paliashdr);
void GLC_DrawSpriteModel(entity_t* e);
void GLC_PolyBlend(float v_blend[4]);
void GLC_BrightenScreen(void);
void GLC_DrawVelocity3D(void);
void GLC_RenderSceneBlurDo(float alpha);

void SCR_SetupCI(void);

void GLM_Draw_Polygon(int x, int y, vec3_t *vertices, int num_vertices, color_t color);

void GLC_EmitWaterPoly(msurface_t* fa);
void GLC_DrawSkyChain(void);
void GLC_DrawFlatPoly(glpoly_t* p);
void GLC_EmitCausticsPolys(qbool use_vbo);

void GLC_DrawSkyChain(void);
void GLC_DrawSky(void);
void GLC_DrawWaterSurfaces(void);
void GLC_DrawBrushModel(entity_t* e, model_t* clmodel, qbool caustics);
void GLC_DrawWorld(void);

void GLM_Draw_SAlphaSubPic2(int x, int y, mpic_t *pic, int src_width, int src_height, float newsl, float newtl, float newsh, float newth, float scale_x, float scale_y, float alpha);
void GLM_Draw_AlphaPieSliceRGB(int x, int y, float radius, float startangle, float endangle, float thickness, qbool fill, color_t color);
void GLM_Draw_LineRGB(float thickness, byte* color, int x_start, int y_start, int x_end, int y_end);
void GLM_DrawImage(float x, float y, float width, float height, float tex_s, float tex_t, float tex_width, float tex_height, byte* color, qbool alpha_test, texture_ref texnum, qbool isText, qbool isCrosshair);
void GLM_DrawAlphaRectangleRGB(int x, int y, int w, int h, float thickness, qbool fill, byte* bytecolor);
void GLM_Draw_FadeScreen(float alpha);
void GLM_DrawSkyChain(void);
void GLM_DrawSky(void);
void GLM_DrawBrushModel(model_t* model, qbool polygonOffset, qbool caustics);
void GLM_AliasModelShadow(entity_t* ent, aliashdr_t* paliashdr, vec3_t shadevector, vec3_t lightspot);
float GLM_Draw_CharacterBase(float x, float y, wchar num, float scale, qbool apply_overall_alpha, byte color[4], qbool bigchar, qbool gl_statechange, qbool proportional);
void GLM_Draw_ResetCharGLState(void);
void GLM_Draw_SetColor(byte* rgba);
void GLM_Draw_StringBase_StartString(int x, int y, float scale);
void GL_FlushImageDraw(void);
void GL_EmptyImageQueue(void);
void GLM_PrepareImageDraw(void);
void GLM_DrawAccelBar(int x, int y, int length, int charsize, int pos);
void GLM_Cache2DMatrix(void);
void GLM_UndoLastCharacter(void);

void GLM_SetIdentityMatrix(float* matrix);
float* GLM_ModelviewMatrix(void);
float* GLM_ProjectionMatrix(void);
float* GL_MatrixForMode(GLenum type);

#ifdef GL_PARANOIA
void GL_ProcessErrors(const char* message);
#define GL_Paranoid_Printf(...) Con_Printf(__VA_ARGS__)
#else
#define GL_Paranoid_Printf(...)
#endif
void GLM_DeletePrograms(qbool restarting);
void GLM_InitPrograms(void);
void GL_DeleteBuffers(void);
void GL_DeleteVAOs(void);

#define MAX_CHARSETS 256
#define NUMCROSSHAIRS  6

void GL_InitialiseState(void);
void GL_GenVertexArray(glm_vao_t* vao, const char* name);
void GL_ConfigureVertexAttribPointer(glm_vao_t* vao, buffer_ref vbo, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer, int divisor);
void GL_ConfigureVertexAttribIPointer(glm_vao_t* vao, buffer_ref vbo, GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid* pointer, int divisor);
void GL_SetVertexArrayElementBuffer(glm_vao_t* vao, buffer_ref ibo);
void GL_AlphaFunc(GLenum func, GLclampf threshold);
void GL_ClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a);

void GL_ClearModelTextureData(void);
void GL_Hint(GLenum target, GLenum mode);
void GL_SetTextureFiltering(GLenum texture_unit, texture_ref texture, GLint minification_filter, GLint magnification_filter);

// --------------
// Texture functions
// --------------

void GL_CreateTextures(GLenum textureUnit, GLenum target, GLsizei n, texture_ref* references);
void GL_CreateTexturesWithIdentifier(GLenum textureUnit, GLenum target, GLsizei n, texture_ref* references, const char* identifier);
void GL_TexStorage2D(GLenum textureUnit, texture_ref reference, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
void GL_TexStorage3D(GLenum textureUnit, texture_ref reference, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
void GL_GenerateMipmap(GLenum textureUnit, texture_ref reference);
void GL_GenerateMipmapWithData(GLenum textureUnit, texture_ref texture, byte* newdata, int width, int height, GLint internal_format);

void GL_TexParameterf(GLenum textureUnit, texture_ref reference, GLenum pname, GLfloat param);
void GL_TexParameterfv(GLenum textureUnit, texture_ref reference, GLenum pname, const GLfloat *params);
void GL_TexParameteri(GLenum textureUnit, texture_ref reference, GLenum pname, GLint param);
void GL_TexParameteriv(GLenum textureUnit, texture_ref reference, GLenum pname, const GLint *params);
void GL_GetTexLevelParameteriv(GLenum textureUnit, texture_ref reference, GLint level, GLenum pname, GLint* params);

void GL_TexSubImage2D(GLenum textureUnit, texture_ref reference, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
void GL_TexSubImage3D(GLenum textureUnit, texture_ref reference, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid * pixels);
void GL_GetTexImage(GLenum textureUnit, texture_ref reference, GLint level, GLenum format, GLenum type, GLsizei bufSize, void* buffer);

void GL_BindTextureUnit(GLuint unit, texture_ref reference);
void GL_EnsureTextureUnitBound(GLuint unit, texture_ref reference);
void GL_BindTextures(GLuint first, GLsizei count, const texture_ref* textures);

byte* SurfaceFlatTurbColor(texture_t* texture);

// Reference cvars for 3D views...
typedef struct uniform_block_frame_constants_s {
	float modelViewMatrix[16];
	float projectionMatrix[16];

	float lightPosition[MAX_DLIGHTS][4];
	float lightColor[MAX_DLIGHTS][4];

	float position[3];
	int lightsActive;

	// Drawflat colors
	float r_wallcolor[4];
	float r_floorcolor[4];
	float r_telecolor[4];
	float r_lavacolor[4];
	float r_slimecolor[4];
	float r_watercolor[4];
	float r_skycolor[4];
	float v_blend[4];

	//
	float time;
	float gamma;
	float contrast;
	int r_alphafont;

	// turb settings
	float skySpeedscale;
	float skySpeedscale2;
	float r_farclip;
	float waterAlpha;

	// drawflat toggles (combine into bitfield?)
	int r_drawflat;
	int r_fastturb;
	int r_fastsky;
	int r_textureless;

	int r_texture_luma_fb;

	// powerup shells round alias models
	float shellSize;
	float shell_base_level1;
	float shell_base_level2;
	float shell_effect_level1;
	float shell_effect_level2;
	float shell_alpha;

	// hardware lighting scale
	float lightScale;
} uniform_block_frame_constants_t;

#define MAX_WORLDMODEL_BATCH     64
#define MAX_SPRITE_BATCH         MAX_STANDARD_ENTITIES
#define MAX_SAMPLER_MAPPINGS    256

typedef struct sampler_mapping_s {
	int samplerIndex;
	float arrayIndex;
	int flags;
	int padding;
} sampler_mapping_t;

typedef struct uniform_block_world_calldata_s {
	float modelMatrix[16];
	float alpha;
	GLint samplerBase;
	GLint flags;
	GLint padding;
} uniform_block_world_calldata_t;

typedef struct uniform_block_aliasmodel_s {
	float modelViewMatrix[16];
	float color[4];
	int amFlags;
	float yaw_angle_rad;
	float shadelight;
	float ambientlight;
	int materialSamplerMapping;
	int lumaSamplerMapping;
	int lerpBaseIndex;
	float lerpFraction;
} uniform_block_aliasmodel_t;

typedef struct block_aliasmodels_s {
	uniform_block_aliasmodel_t models[MAX_STANDARD_ENTITIES];
} uniform_block_aliasmodels_t;

typedef struct uniform_block_sprite_s {
	float modelView[16];
	float tex[2];
	int skinNumber;
	int padding;
} uniform_block_sprite_t;

typedef struct uniform_block_spritedata_s {
	uniform_block_sprite_t sprites[MAX_SPRITE_BATCH];
} uniform_block_spritedata_t;

void GL_PreRenderView(void);
void GLC_PreRenderView(void);
void GLM_PreRenderView(void);

void GLC_SetupGL(void);
void GLM_SetupGL(void);

qbool GL_ExternalTexturesEnabled(qbool worldmodel);

typedef struct glm_worldmodel_req_s {
	// This is DrawElementsIndirectCmd, from OpenGL spec
	GLuint count;           // Number of indexes to pull
	GLuint instanceCount;   // Always 1... ?
	GLuint firstIndex;      // Position of first index in array
	GLuint baseVertex;      // Offset of vertices in VBO
	GLuint baseInstance;    // We use this to pull from array of uniforms in shader

	float mvMatrix[16];
	int flags;
	int samplerMappingBase;
	int samplerMappingCount;
	int firstTexture;
	float alpha;
	qbool polygonOffset;
	qbool worldmodel;
	model_t* model;
} glm_worldmodel_req_t;

struct custom_model_color_s;

void GLC_StateBeginAliasOutlineFrame(void);
void GLC_StateEndAliasOutlineFrame(void);

void GLM_StateBeginAliasOutlineBatch(void);
void GLM_StateEndAliasOutlineBatch(void);

void GLC_StateBeginWaterSurfaces(void);
void GLC_StateEndWaterSurfaces(void);
void GL_StateBeginEntities(visentlist_t* vislist);
void GL_StateEndEntities(visentlist_t* vislist);
void GL_StateBeginPolyBlend(void);
void GL_StateEndPolyBlend(void);
void GLC_StateBeginAlphaChain(void);
void GLC_StateEndAlphaChain(void);
void GLC_StateBeginAlphaChainSurface(msurface_t* s);
void GLC_StateBeginAliasPowerupShell(void);
void GLC_StateEndAliasPowerupShell(void);
void GLC_StateBeginUnderwaterCaustics(void);
void GLC_StateEndUnderwaterCaustics(void);
void GLC_StateBeginMD3Draw(float alpha);
void GLC_StateEndMD3Draw(void);
void GLC_StateBeginBrightenScreen(void);
void GLC_StateEndBrightenScreen(void);
void GLC_StateBeginFastSky(void);
void GLC_StateEndFastSky(void);
void GLC_StateBeginSky(void);
void GLC_StateBeginSkyZBufferPass(void);
void GLC_StateEndSkyZBufferPass(void);
void GLC_StateEndSkyNoZBufferPass(void);
void GLC_StateBeginSkyDome(void);
void GLC_StateBeginSkyDomeCloudPass(void);
void GLC_StateBeginMultiTextureSkyDome(void);
void GLC_StateEndMultiTextureSkyDome(void);
void GLC_StateBeginMultiTextureSkyChain(void);
void GLC_StateEndMultiTextureSkyChain(void);
void GLC_StateBeginSingleTextureSkyPass(void);
void GLC_StateBeginSingleTextureCloudPass(void);
void GLC_StateEndSingleTextureSky(void);
void GLC_StateBeginRenderFullbrights(void);
void GLC_StateEndRenderFullbrights(void);
void GLC_StateBeginRenderLumas(void);
void GLC_StateEndRenderLumas(void);
void GLC_StateBeginEmitDetailPolys(void);
void GLC_StateEndEmitDetailPolys(void);
void GLC_StateBeginDrawMapOutline(void);
void GLC_StateBeginEndMapOutline(void);
void GLM_StateBeginDraw3DSprites(void);
void GLM_StateEndDraw3DSprites(void);
void GLM_StateBeginDrawWorldOutlines(void);
void GLM_StateEndDrawWorldOutlines(void);
void GL_StateBeginAlphaLineRGB(float thickness);
void GL_StateEndAlphaLineRGB(void);
void GLC_StateBeginDrawAliasFrame(texture_ref texture, texture_ref fb_texture, qbool mtex, qbool alpha_blend, struct custom_model_color_s* custom_model);
void GLC_StateEndDrawAliasFrame(void);
void GLC_StateBeginAliasModelShadow(void);
void GLC_StateEndAliasModelShadow(void);
void GLC_StateBeginDrawFlatModel(void);
void GLC_StateEndDrawFlatModel(void);
void GLC_StateBeginDrawTextureChains(model_t* model, GLenum lightmapTextureUnit, GLenum fullbrightTextureUnit, GLenum fullbrightMode);
void GLC_StateEndWorldTextureChains(GLenum lightmapTextureUnit, GLenum fullbrightTextureUnit);
void GLC_StateEndDrawTextureChains(void);
void GLC_StateEndFastTurbPoly(void);
void GLC_StateBeginFastTurbPoly(byte color[4]);

void GLC_StateBeginBlendLightmaps(qbool use_buffers);
void GLC_StateEndBlendLightmaps(void);
void GLC_StateBeginSceneBlur(void);
void GLC_StateEndSceneBlur(void);
void GLC_StateBeginCausticsPolys(void);
void GLC_StateEndCausticsPolys(void);
void GLC_StateBeginDrawViewModel(float alpha);
void GLC_StateEndDrawViewModel(void);
void GL_StateBeginDrawBrushModel(entity_t* e, qbool polygonOffset);
void GL_StateEndDrawBrushModel(void);
void GL_StateDefault2D(void);
void GL_StateDefault3D(void);
void GL_StateDefaultInit(void);

void GLC_StateBeginDraw3DSprites(void);
void GLC_StateEndDraw3DSprites(void);
void GL_StateBeginDrawAliasModel(entity_t* e, aliashdr_t* paliashdr);
void GL_StateEndDrawAliasModel(void);
void GLC_StateBeginSimpleItem(texture_ref simpletexture);
void GLC_StateEndSimpleItem(void);

void GLC_StateBeginBloomDraw(texture_ref texture);
void GLC_StateEndBloomDraw(void);
void GL_StateEndFrame(void);

void GLC_StateBeginImageDraw(void);
void GLC_StateEndImageDraw(void);
void GLC_StateBeginPolyBlend(float v_blend[4]);
void GLC_StateEndPolyBlend(void);
void GLC_StateBeginDrawPolygon(void);
void GLC_StateEndDrawPolygon(int oldFlags);
void GLC_StateBeginDrawAlphaPieSliceRGB(float thickness);
void GLC_StateEndDrawAlphaPieSliceRGB(float thickness);

void GLC_DisableAllTexturing(void);
void GLC_InitTextureUnitsNoBind1(GLenum envMode0);
void GLC_InitTextureUnits1(texture_ref texture0, GLenum envMode0);
void GLC_InitTextureUnits2(texture_ref texture0, GLenum envMode0, texture_ref texture1, GLenum envMode1);

void GLC_PauseMatrixUpdate(void);
void GLC_ResumeMatrixUpdate(void);
void GLC_LoadMatrix(GLenum matrix);
void GLC_AllocateAliasPoseBuffer(void);
void R_SetupFrame(void);

void GL_FlushWorldModelBatch(void);
void GL_InitialiseFramebufferHandling(void);

float GL_WaterAlpha(void);

#define VBO_FIELDOFFSET(type, field) (void*)((uintptr_t)&(((type*)0)->field))

typedef enum aliasmodel_draw_batch_s {
	aliasmodel_batch_std_entities,
	aliasmodel_batch_viewmodel
} aliasmodel_draw_batch_t;

void GLM_InitialiseAliasModelBatches(void);
void GLM_PrepareAliasModelBatches(void);
void GLM_DrawAliasModelBatches(void);
void GLM_DrawAliasModelPostSceneBatches(void);

typedef enum {
	opaque_world,
	alpha_surfaces
} glm_brushmodel_drawcall_type;

void GL_DrawWorldModelBatch(glm_brushmodel_drawcall_type type);

// Rendering functions
void GL_DrawArrays(GLenum mode, GLint first, GLsizei count);
void GL_MultiDrawArrays(GLenum mode, GLint* first, GLsizei* count, GLsizei primcount);
void GL_DrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, GLvoid* indices, GLint basevertex);
void GL_DrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);
void GL_MultiDrawArraysIndirect(GLenum mode, const void* indirect, GLsizei drawcount, GLsizei stride);
void GL_MultiDrawElementsIndirect(GLenum mode, GLenum type, const void* indirect, GLsizei drawcount, GLsizei stride);
qbool GL_DrawElementsBaseVertexAvailable(void);

// Buffers
typedef enum {
	buffertype_unknown,
	buffertype_use_once,
	buffertype_reuse_many,
	buffertype_constant
} buffertype_t;

buffer_ref GL_CreateFixedBuffer(GLenum target, const char* name, GLsizei size, void* data, buffertype_t usage);
void GL_BufferStartFrame(void);
void GL_BufferEndFrame(void);
uintptr_t GL_BufferOffset(buffer_ref ref);

void GL_BindImageTexture(GLuint unit, texture_ref texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format);

#ifndef EZ_OPENGL_NO_EXTENSIONS
#define GL_LoadMandatoryFunctionExtension(functionName,testFlag) { testFlag &= ((q##functionName = (functionName##_t)SDL_GL_GetProcAddress(#functionName)) != NULL); }
#define GL_LoadOptionalFunction(functionName) { q##functionName = (functionName##_t)SDL_GL_GetProcAddress(#functionName); }
#else
#define GL_LoadMandatoryFunctionExtension(functionName,testFlag) { q##functionName = NULL; testFlag = false; }
#define GL_LoadOptionalFunction(functionName) { q##functionName = NULL; }
#endif

void GLM_UploadFrameConstants(void);
void GL_InitialiseProgramState(void);

void GL_Uniform1i(GLint location, GLint value);
void GL_Uniform4fv(GLint location, GLsizei count, GLfloat* values);
void GL_UniformMatrix4fv(GLint location, GLsizei count, qbool transpose, GLfloat* values);
GLint GL_UniformGetLocation(GLuint program, const char* name);

void GL_DispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
void GL_MemoryBarrier(GLbitfield barriers);

void GLC_ClientActiveTexture(GLenum texture_unit);

#define R_NoLighting() (r_dynamic.integer == 0)
#define R_HardwareLighting() (r_dynamic.integer == 2 && GL_UseGLSL())
#define R_SoftwareLighting() (r_dynamic.integer && !R_HardwareLighting())

void GLM_SamplerSetNearest(GLuint texture_unit_number);
void GLM_SamplerClear(GLuint texture_unit_number);
void GL_DeleteSamplers(void);

#endif /* !__GL_LOCAL_H__ */
