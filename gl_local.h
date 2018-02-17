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
#include <OpenGL/glext.h>

#else // __APPLE__

#include <GL/gl.h>

#ifdef __GNUC__
#include <GL/glext.h>
#endif // __GNUC__

#ifdef _MSC_VER
#include <glext.h>
#endif

#ifdef FRAMEBUFFERS
#include "GL/glext.h"
#endif

#ifndef _WIN32
#include <GL/glx.h>
#endif // _WIN32
#endif // __APPLE__

#include "gl_texture.h"
#ifdef FRAMEBUFFERS
#include "gl_framebuffer.h"
#endif

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

typedef struct r_frame_stats_classic_s {
	int brush_polys;
	int alias_polys;
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
	int lightmap_updates;
	int draw_calls;
	int subdraw_calls;

	double start_time;
} r_frame_stats_t;

extern r_frame_stats_t frameStats;

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
extern	cvar_t	r_wateralpha;
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
extern	cvar_t gl_powerupshells_style;
extern	cvar_t gl_powerupshells_size;

extern cvar_t gl_gammacorrection;
extern cvar_t gl_modulate;

extern cvar_t gl_max_size, gl_scaleModelTextures, gl_scaleTurbTextures, gl_miptexLevel;

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
void CalcCausticTexCoords(float *v, float *s, float *t);
void EmitCausticsPolys (void);
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
void GL_AliasModelAddToVBO(model_t* mod, aliashdr_t* hdr, buffer_ref vbo, buffer_ref ssbo, int position);

// gl_rsurf.c

void EmitDetailPolys (void);
void R_DrawBrushModel (entity_t *e);
void R_DrawWorld (void);
void R_DrawWaterSurfaces (void);
void R_DrawAlphaChain(msurface_t* alphachain);
void GL_BuildLightmaps (void);

qbool R_FullBrightAllowed(void);
void R_Check_R_FullBright(void);

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

//combine extension
#define GL_COMBINE_EXT				0x8570
#define GL_COMBINE_RGB_EXT			0x8571
#define GL_RGB_SCALE_EXT			0x8573

typedef void (APIENTRY *lpMTexFUNC) (GLenum, GLfloat, GLfloat);
typedef void (APIENTRY *lpSelTexFUNC) (GLenum);

extern lpMTexFUNC qglMultiTexCoord2f;
extern lpSelTexFUNC qglActiveTexture;

extern double gldepthmin, gldepthmax;
extern byte color_white[4], color_black[4];
extern qbool gl_mtexable;
extern int gl_textureunits;
extern qbool gl_combine, gl_add_ext;
extern qbool gl_support_arb_texture_non_power_of_two;

void Check_Gamma (unsigned char *pal);
void VID_SetPalette (unsigned char *palette);
void GL_Init (void);

// General
typedef const GLubyte* (APIENTRY *glGetStringi_t)(GLenum name, GLuint index);

// VAOs
typedef void (APIENTRY *glGenVertexArrays_t)(GLsizei n, GLuint* arrays);
typedef void (APIENTRY *glBindVertexArray_t)(GLuint arrayNum);
typedef void (APIENTRY *glDeleteVertexArrays_t)(GLsizei n, const GLuint* arrays);
typedef void (APIENTRY *glEnableVertexAttribArray_t)(GLuint index);
typedef void (APIENTRY *glVertexAttribPointer_t)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer);
typedef void (APIENTRY *glVertexAttribIPointer_t)(GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid* pointer);
typedef void (APIENTRY *glVertexAttribDivisor_t)(GLuint index, GLuint divisor);

// Shader functions
typedef GLuint (APIENTRY *glCreateShader_t)(GLenum shaderType);
typedef void (APIENTRY *glShaderSource_t)(GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
typedef void (APIENTRY *glCompileShader_t)(GLuint shader);
typedef void (APIENTRY *glDeleteShader_t)(GLuint shader);
typedef void (APIENTRY *glGetShaderInfoLog_t)(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRY *glGetShaderiv_t)(GLuint shader, GLenum pname, GLint* params);

// Program functions
typedef GLuint(APIENTRY *glCreateProgram_t)(void);
typedef void (APIENTRY *glLinkProgram_t)(GLuint program);
typedef void (APIENTRY *glDeleteProgram_t)(GLuint program);
typedef void (APIENTRY *glGetProgramInfoLog_t)(GLuint program, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRY *glUseProgram_t)(GLuint program);
typedef void (APIENTRY *glAttachShader_t)(GLuint program, GLuint shader);
typedef void (APIENTRY *glDetachShader_t)(GLuint program, GLuint shader);
typedef void (APIENTRY *glGetProgramiv_t)(GLuint program, GLenum pname, GLint* params);

// Uniforms
typedef GLint(APIENTRY *glGetUniformLocation_t)(GLuint program, const GLchar* name);
typedef void (APIENTRY *glUniform1f_t)(GLint location, GLfloat v0);
typedef void (APIENTRY *glUniform1fv_t)(GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRY *glUniform2f_t)(GLint location, GLfloat v0, GLfloat v1);
typedef void (APIENTRY *glUniform3f_t)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (APIENTRY *glUniform3fv_t)(GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRY *glUniform4f_t)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (APIENTRY *glUniform1i_t)(GLint location, GLint v0);
typedef void (APIENTRY *glProgramUniform1i_t)(GLuint program, GLint location, GLint v0);
typedef void (APIENTRY *glUniformMatrix4fv_t)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRY *glUniform4fv_t)(GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRY *glUniform1iv_t)(GLint location, GLsizei count, const GLint *value);
typedef GLuint (APIENTRY *glGetUniformBlockIndex_t)(GLuint program, const GLchar * uniformBlockName);
typedef void (APIENTRY *glUniformBlockBinding_t)(GLuint program, GLuint uBlockIndex, GLuint uBlockBinding);
typedef void (APIENTRY *glGetActiveUniformBlockiv_t)(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint *params);

// Textures
typedef void (APIENTRY *glActiveTexture_t)(GLenum texture);
typedef void (APIENTRY *glTexSubImage3D_t)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid * pixels);
typedef void (APIENTRY *glTexStorage2D_t)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRY *glTexStorage3D_t)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
typedef void (APIENTRY *glGenerateMipmap_t)(GLenum target);
typedef void (APIENTRY *glBindTextures_t)(GLuint first, GLsizei count, const GLuint* format);

// Draw functions
typedef void (APIENTRY *glMultiDrawArrays_t)(GLenum mode, const GLint * first, const GLsizei* count, GLsizei drawcount);
typedef void (APIENTRY *glMultiDrawElements_t)(GLenum mode, const GLsizei * count, GLenum type, const GLvoid * const * indices, GLsizei drawcount);
typedef void (APIENTRY *glDrawArraysInstanced_t)(GLenum mode, GLint first, GLsizei count, GLsizei primcount);
typedef void (APIENTRY *glMultiDrawArraysIndirect_t)(GLenum mode, const void *indirect, GLsizei drawcount, GLsizei stride);
typedef void (APIENTRY *glMultiDrawElementsIndirect_t)(GLenum mode, GLenum type, const void* indirect, GLsizei drawcount, GLsizei stride);
typedef void (APIENTRY *glDrawArraysInstancedBaseInstance_t)(GLenum mode, GLint first, GLsizei count, GLsizei primcount, GLuint baseinstance);
typedef void (APIENTRY *glDrawElementsInstancedBaseInstance_t)(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei primcount, GLuint baseinstance);
typedef void (APIENTRY *glDrawElementsInstancedBaseVertexBaseInstance_t)(GLenum mode, GLsizei count, GLenum type, GLvoid* indices, GLsizei primcount, GLint basevertex, GLuint baseinstance);
typedef void (APIENTRY *glPrimitiveRestartIndex_t)(GLuint index);
typedef void (APIENTRY *glDrawElementsBaseVertex_t)(GLenum mode, GLsizei count, GLenum type, GLvoid* indices, GLint basevertex);

// Debug functions
typedef void (APIENTRY *glObjectLabel_t)(GLenum identifier, GLuint name, GLsizei length, const char* label);

// VAO functions
extern glBindVertexArray_t         glBindVertexArray;
extern glEnableVertexAttribArray_t glEnableVertexAttribArray;
extern glVertexAttribPointer_t     glVertexAttribPointer;
extern glVertexAttribIPointer_t    glVertexAttribIPointer;
extern glVertexAttribDivisor_t     glVertexAttribDivisor;

// Shader functions
extern glCreateShader_t      glCreateShader;
extern glShaderSource_t      glShaderSource;
extern glCompileShader_t     glCompileShader;
extern glDeleteShader_t      glDeleteShader;
extern glGetShaderInfoLog_t  glGetShaderInfoLog;
extern glGetShaderiv_t       glGetShaderiv;

// Program functions
extern glCreateProgram_t     glCreateProgram;
extern glLinkProgram_t       glLinkProgram;
extern glDeleteProgram_t     glDeleteProgram;
extern glGetProgramInfoLog_t glGetProgramInfoLog;
extern glUseProgram_t        glUseProgram;
extern glAttachShader_t      glAttachShader;
extern glDetachShader_t      glDetachShader;
extern glGetProgramiv_t      glGetProgramiv;

// Uniforms
extern glGetUniformLocation_t   glGetUniformLocation;
extern glUniform1f_t            glUniform1f;
extern glUniform1fv_t           glUniform1fv;
extern glUniform2f_t            glUniform2f;
extern glUniform3f_t            glUniform3f;
extern glUniform3fv_t           glUniform3fv;
extern glUniform4f_t            glUniform4f;
extern glUniform1i_t            glUniform1i;
extern glProgramUniform1i_t     glProgramUniform1i;
extern glUniform4fv_t           glUniform4fv;
extern glUniform1iv_t           glUniform1iv;
extern glUniformMatrix4fv_t     glUniformMatrix4fv;
extern glGetUniformBlockIndex_t glGetUniformBlockIndex;
extern glUniformBlockBinding_t  glUniformBlockBinding;
extern glGetActiveUniformBlockiv_t glGetActiveUniformBlockiv;

// Textures
extern glActiveTexture_t        glActiveTexture;
extern glBindTextures_t         glBindTextures;

// Draw functions
extern glMultiDrawArrays_t      glMultiDrawArrays;
extern glMultiDrawElements_t    glMultiDrawElements;
extern glDrawArraysInstanced_t  glDrawArraysInstanced;
extern glMultiDrawArraysIndirect_t glMultiDrawArraysIndirect;
extern glMultiDrawElementsIndirect_t glMultiDrawElementsIndirect;
extern glDrawArraysInstancedBaseInstance_t glDrawArraysInstancedBaseInstance;
extern glDrawElementsInstancedBaseInstance_t glDrawElementsInstancedBaseInstance;
extern glPrimitiveRestartIndex_t glPrimitiveRestartIndex;
extern glDrawElementsInstancedBaseVertexBaseInstance_t glDrawElementsInstancedBaseVertexBaseInstance;
extern glDrawElementsBaseVertex_t glDrawElementsBaseVertex;

// Debug functions
extern glObjectLabel_t glObjectLabel;

qbool GL_ShadersSupported(void);
qbool GL_VBOsSupported(void);

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
#define GLM_SHADER_COUNT    3

typedef struct glm_program_s {
	GLuint vertex_shader;
	GLuint geometry_shader;
	GLuint fragment_shader;
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

#define GL_VFDeclare(name) \
	extern unsigned char glsl_##name##_vertex_glsl[];\
	extern unsigned int glsl_##name##_vertex_glsl_len;\
	extern unsigned char glsl_##name##_fragment_glsl[];\
	extern unsigned int glsl_##name##_fragment_glsl_len;

#define GL_VFParams(name) \
	(const char*)glsl_##name##_vertex_glsl,\
	glsl_##name##_vertex_glsl_len,\
	(const char*)glsl_##name##_fragment_glsl,\
	glsl_##name##_fragment_glsl_len

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
	extern unsigned char glsl_##name##_vertex_glsl[];\
	extern unsigned int glsl_##name##_vertex_glsl_len;\
	extern unsigned char glsl_##name##_geometry_glsl[];\
	extern unsigned int glsl_##name##_geometry_glsl_len;\
	extern unsigned char glsl_##name##_fragment_glsl[];\
	extern unsigned int glsl_##name##_fragment_glsl_len;
#define GL_VGFParams(name) \
	(const char*)glsl_##name##_vertex_glsl,\
	glsl_##name##_vertex_glsl_len,\
	(const char*)glsl_##name##_geometry_glsl,\
	glsl_##name##_geometry_glsl_len,\
	(const char*)glsl_##name##_fragment_glsl,\
	glsl_##name##_fragment_glsl_len

#define glColor3f GL_Color3f
#define glColor4f GL_Color4f
#define glColor3fv GL_Color3fv
#define glColor3ubv GL_Color3ubv
#define glColor4ubv GL_Color4ubv
#define glColor4ub GL_Color4ub
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


void GL_Color3f(float r, float g, float b);
void GL_Color4f(float r, float g, float b, float a);
void GL_Color3fv(const float* rgbVec);
void GL_Color3ubv(const GLubyte* rgbVec);
void GL_Color4ubv(const GLubyte* rgbaVec);
void GL_Color4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a);

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

int GL_PopulateVBOForBrushModel(model_t* m, vbo_world_vert_t* vbo_buffer, int vbo_pos);
int GL_MeasureVBOSizeForBrushModel(model_t* m);
void GL_CreateAliasModelVAO(buffer_ref aliasModelVBO, buffer_ref instanceVBO);
void GL_CreateBrushModelVAO(buffer_ref instance_vbo);

void GL_UseProgram(GLuint program);
void GL_DepthFunc(GLenum func);
void GL_DepthRange(double nearVal, double farVal);
void GL_CullFace(GLenum mode);
void GL_BlendFunc(GLenum sfactor, GLenum dfactor);
void GL_BindVertexArray(glm_vao_t* vao);
void GL_ShadeModel(GLenum model);
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
void GL_UnBindBuffer(GLenum target);
void GL_ResizeBuffer(buffer_ref vbo, size_t size, void* data);
size_t GL_VBOSize(buffer_ref vbo);

#ifdef WITH_NVTX
#define ENTER_STATE GL_EnterTracedRegion(__FUNCTION__, true)
#define MIDDLE_STATE GL_MarkEvent(__FUNCTION__)
#define LEAVE_STATE GL_LeaveTracedRegion(true)
#define GL_EnterRegion(x) GL_EnterTracedRegion(x, false)
#define GL_LeaveRegion() GL_LeaveTracedRegion(false)
void GL_EnterTracedRegion(const char* regionName, qbool trace_only);
void GL_LeaveTracedRegion(qbool trace_only);
void GL_PrintState(void);
void GL_ResetRegion(qbool start);
void GL_LogAPICall(const char* message, ...);
void GL_MarkEvent(const char* message, ...);
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
#define GL_PrintState()
#endif

#define NUMVERTEXNORMALS 162
#define SHADEDOT_QUANT   64

#define	MD3_INTERP_MAXDIST  300

// Lightmap size
#define	LIGHTMAP_WIDTH  128
#define	LIGHTMAP_HEIGHT 128

// 
#define CHARSET_CHARS_PER_ROW	16
#define CHARSET_WIDTH			1.0
#define CHARSET_HEIGHT			1.0
#define CHARSET_CHAR_WIDTH		(CHARSET_WIDTH / CHARSET_CHARS_PER_ROW)
#define CHARSET_CHAR_HEIGHT		(CHARSET_HEIGHT / CHARSET_CHARS_PER_ROW)

// Functions

void GLM_ExitBatchedPolyRegion(void);

void GLM_DrawSimpleItem(texture_ref texture_array, int texture_index, float scale_s, float scale_t, vec3_t origin, float scale, vec3_t up, vec3_t right);
void GLC_DrawSimpleItem(texture_ref simpletexture, vec3_t org, float sprsize, vec3_t up, vec3_t right);

void GL_BeginDrawSprites(void);
void GL_EndDrawSprites(void);
void GL_BeginDrawBrushModels(void);
void GL_BeginDrawAliasModels(void);
void GL_EndDrawAliasModels(void);
void GL_EndDrawBrushModels(void);

void GLM_MultiplyMatrix(const float* lhs, const float* rhs, float* target);
void GLM_MultiplyVector(const float* matrix, const float* vector, float* result);
void GLM_MultiplyVector3f(const float* matrix, float x, float y, float z, float* result);
void GLM_MultiplyVector3fv(const float* matrix, const vec3_t vector, float* result);
void GLM_DrawWaterSurfaces(void);

void GL_BuildCommonTextureArrays(qbool vid_restart);
void GL_CreateModelVBOs(qbool vid_restart);
void GLM_CreateBrushModelVAO(buffer_ref instance_vbo);

void R_DrawAliasModel(entity_t *ent);
void R_DrawAliasPowerupShell(entity_t *ent);

// 
void R_RenderDynamicLightmaps(msurface_t *fa);
void R_DrawViewModel(void);
void R_RenderAllDynamicLightmaps(model_t *model);
void GLC_DrawMapOutline(model_t *model);
void R_SetupAliasFrame(model_t* model, maliasframedesc_t *oldframe, maliasframedesc_t *frame, qbool mtex, qbool scrolldir, qbool outline, texture_ref texture, texture_ref fb_texture, int effects, int render_effects);
int R_AliasFramePose(maliasframedesc_t* frame);
void GLC_DrawPowerupShell(model_t* model, int effects, int layer_no, maliasframedesc_t *oldframe, maliasframedesc_t *frame);
void GLM_DrawPowerupShell(model_t* model, int effects, int layer_no, maliasframedesc_t *oldframe, maliasframedesc_t *frame);

void GLM_DrawTexturedWorld(model_t* model);
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
void GLC_CreateLightmapTextures(void);
void GLC_AliasModelPowerupShell(entity_t* ent, maliasframedesc_t* oldframe, maliasframedesc_t* frame);
void GLC_AliasModelShadow(entity_t* ent, aliashdr_t* paliashdr, vec3_t shadevector, vec3_t lightspot);
void GLC_UnderwaterCaustics(entity_t* ent, model_t* clmodel, maliasframedesc_t* oldframe, maliasframedesc_t* frame, aliashdr_t* paliashdr);
void GLC_DrawSpriteModel(entity_t* e);
void GLC_PolyBlend(float v_blend[4]);
void GLC_BrightenScreen(void);
void GLC_DrawVelocity3D(void);
void GLC_RenderSceneBlurDo(float alpha);

void SCR_SetupCI(void);

void GLC_DrawTileClear(texture_ref texnum, int x, int y, int w, int h);
void GLC_Draw_LineRGB(byte* bytecolor, int x_start, int y_start, int x_end, int y_end);
void GLC_Draw_Polygon(int x, int y, vec3_t *vertices, int num_vertices, color_t color);

void GLM_Draw_Polygon(int x, int y, vec3_t *vertices, int num_vertices, color_t color);

void GLC_DrawParticles(int particles_to_draw, qbool square);
void GLC_EmitWaterPoly(msurface_t* fa);
void GLC_DrawSkyChain(void);
void GLC_DrawFlatPoly(glpoly_t* p);
void GLC_EmitCausticsPolys(void);

void GLC_Draw_FadeScreen(float alpha);
void GLC_DrawSkyChain(void);
void GLC_DrawSky(void);
void GLC_DrawWaterSurfaces(void);
void GLC_DrawBrushModel(entity_t* e, model_t* clmodel, qbool caustics);
void GLC_DrawWorld(void);

void GLC_Draw_AlphaPieSliceRGB(int x, int y, float radius, float startangle, float endangle, float thickness, qbool fill, color_t color);
void GLM_Draw_SAlphaSubPic2(int x, int y, mpic_t *pic, int src_width, int src_height, float newsl, float newtl, float newsh, float newth, float scale_x, float scale_y, float alpha);
void GLM_Draw_AlphaPieSliceRGB(int x, int y, float radius, float startangle, float endangle, float thickness, qbool fill, color_t color);
void GLM_Draw_LineRGB(byte* color, int x_start, int y_start, int x_end, int y_end);
void GLM_DrawImage(float x, float y, float width, float height, float tex_s, float tex_t, float tex_width, float tex_height, byte* color, qbool alpha, texture_ref texnum, qbool isText);
void GLM_DrawAlphaRectangeRGB(int x, int y, int w, int h, float thickness, qbool fill, byte* bytecolor);
void GLM_DrawParticles(int number, qbool square);
void GLM_Draw_FadeScreen(float alpha);
void GLM_RenderDlight(dlight_t* light);
void GLM_DrawSkyChain(void);
void GLM_DrawSky(void);
void GLM_DrawBrushModel(model_t* model, qbool polygonOffset, qbool caustics);
void GLM_AliasModelShadow(entity_t* ent, aliashdr_t* paliashdr, vec3_t shadevector, vec3_t lightspot);
void GLM_Draw_CharacterBase(int x, int y, wchar num, float scale, qbool apply_overall_alpha, byte color[4], qbool bigchar, qbool gl_statechange);
void GLM_Draw_ResetCharGLState(void);
void GLM_Draw_SetColor(byte* rgba, float alpha);
void GLM_Draw_StringBase_StartString(int x, int y, float scale);
void GL_FlushImageDraw(qbool draw);
void GLM_DrawAccelBar(int x, int y, int length, int charsize, int pos);

void GLM_SetIdentityMatrix(float* matrix);
float* GLM_ModelviewMatrix(void);
float* GLM_ProjectionMatrix(void);
float* GL_MatrixForMode(GLenum type);

void GL_ProcessErrors(const char* message);
#ifdef GL_PARANOIA
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
buffer_ref GL_GenUniformBuffer(const char* name, void* data, GLuint size);
void GL_GenVertexArray(glm_vao_t* vao, const char* name);
void GL_ConfigureVertexAttribPointer(glm_vao_t* vao, buffer_ref vbo, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer);
void GL_ConfigureVertexAttribIPointer(glm_vao_t* vao, buffer_ref vbo, GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid* pointer);
void GL_SetVertexArrayElementBuffer(glm_vao_t* vao, buffer_ref ibo);
void GL_AlphaFunc(GLenum func, GLclampf threshold);

void GL_DeleteModelData(void);
void GL_Hint(GLenum target, GLenum mode);

// --------------
// Texture functions
// --------------

void GL_CreateTextures(GLenum textureUnit, GLenum target, GLsizei n, texture_ref* references);
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

#define GLM_Enabled GL_ShadersSupported

typedef enum glm_uniform_block_id_s {
	// Uniforms
	GL_BINDINGPOINT_FRAMECONSTANTS,

	GL_BINDINGPOINT_DRAWWORLD_CVARS,
	GL_BINDINGPOINT_ALIASMODEL_CVARS,
	GL_BINDINGPOINT_SPRITEDATA_CVARS,

	GL_BINDINGPOINT_SKYDOME_CVARS,
	GL_BINDINGPOINT_SKYBOX_CVARS,

	GL_UNIFORM_BINDINGPOINT_COUNT
} glm_uniform_block_id_t;

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

	//
	float time;
	float gamma3d;
	float gamma2d;
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

#define MAX_WORLDMODEL_MATRICES  32
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
	float alpha;
	GLint samplerBase;
	GLint flags;
	GLint matrixMapping;
} uniform_block_world_calldata_t;

typedef struct uniform_block_world_s {
	float modelMatrix[MAX_WORLDMODEL_MATRICES][16];
	uniform_block_world_calldata_t calls[MAX_WORLDMODEL_BATCH];
	sampler_mapping_t mappings[MAX_SAMPLER_MAPPINGS];
} uniform_block_world_t;

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

#define MAX_WORLDMODEL_INDEXES (16 * 1024)

typedef struct glm_worldmodel_req_s {
	// This is DrawElementsIndirectCmd, from OpenGL spec
	GLuint count;           // Number of indexes to pull
	GLuint instanceCount;   // Always 1... ?
	GLuint firstIndex;      // Position of first index in array
	GLuint baseVertex;      // Offset of vertices in VBO
	GLuint baseInstance;    // We use this to pull from array of uniforms in shader

	int mvMatrixMapping;
	int flags;
	int samplerMappingBase;
	int samplerMappingCount;
	int firstTexture;
	float alpha;
	qbool polygonOffset;
	qbool worldmodel;
	model_t* model;
} glm_worldmodel_req_t;

void GL_StateBeginAliasOutlineFrame(void);
void GL_StateEndAliasOutlineFrame(void);

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
void GLM_StateBeginDrawBillboards(void);
void GLM_StateEndDrawBillboards(void);
void GLM_StateBeginDrawWorldOutlines(void);
void GLM_StateEndDrawWorldOutlines(void);
void GL_StateBeginAlphaLineRGB(float thickness);
void GL_StateEndAlphaLineRGB(void);
void GLC_StateBeginDrawAliasFrame(texture_ref texture, texture_ref fb_texture, qbool mtex, float alpha, struct custom_model_color_s* custom_model);
void GLC_StateEndDrawAliasFrame(void);
void GLC_StateBeginAliasModelShadow(void);
void GLC_StateEndAliasModelShadow(void);
void GLC_StateBeginDrawFlatModel(void);
void GLC_StateEndDrawFlatModel(void);
void GLC_StateBeginDrawTextureChains(GLenum lightmapTextureUnit, GLenum fullbrightTextureUnit, GLenum fullbrightMode);
void GLC_StateEndDrawTextureChainsFirstPass(GLenum lightmapTextureUnit, GLenum fullbrightTextureUnit);
void GLC_StateEndDrawTextureChains(void);
void GLC_StateBeginTurbPoly(void);
void GLC_StateEndTurbPoly(void);
void GLC_StateEndFastTurbPoly(void);
void GLC_StateBeginFastTurbPoly(byte color[4]);

void GLC_StateBeginBlendLightmaps(void);
void GLC_StateEndBlendLightmaps(void);
void GLC_StateBeginSceneBlur(void);
void GLC_StateEndSceneBlur(void);
void GLC_StateBeginCausticsPolys(void);
void GLC_StateEndCausticsPolys(void);
void GL_StateBeginDrawViewModel(float alpha);
void GL_StateEndDrawViewModel(void);
void GL_StateBeginDrawBrushModel(entity_t* e, qbool polygonOffset);
void GL_StateEndDrawBrushModel(void);
void GL_StateDefault2D(void);
void GL_StateDefault3D(void);
void GL_StateDefaultInit(void);

void GLC_StateBeginDrawBillboards(void);
void GLC_StateEndDrawBillboards(void);
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
void GL_StateBeginNetGraph(void);
void GL_StateEndNetGraph(void);
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

void GL_FlushWorldModelBatch(void);
void GL_InitialiseFramebufferHandling(void);

float GL_WaterAlpha(void);
qbool R_DrawLightmaps(void);

#define VBO_FIELDOFFSET(type, field) (void*)((uintptr_t)&(((type*)0)->field))

typedef enum aliasmodel_draw_batch_s {
	aliasmodel_batch_std_entities,
	aliasmodel_batch_viewmodel
} aliasmodel_draw_batch_t;

void GLM_InitialiseAliasModelBatches(void);
void GLM_PrepareAliasModelBatches(void);
void GLM_DrawAliasModelBatches(void);

#endif /* !__GL_LOCAL_H__ */


