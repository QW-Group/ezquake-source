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

extern	GLuint particletexture;
extern	GLuint netgraphtexture;
extern	GLuint playertextures;
extern	GLuint playernmtextures[MAX_CLIENTS];
extern	GLuint playerfbtextures[MAX_CLIENTS];
#define MAX_SKYBOXTEXTURES 6
extern	GLuint skyboxtextures[MAX_SKYBOXTEXTURES];
extern	GLuint skytexturenum;		// index in cl.loadmodel, not gl texture object
extern	GLuint underwatertexture, detailtexture;
extern	GLuint shelltexture;

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
extern	cvar_t	r_skincolormodedead;
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
extern  cvar_t  gl_squareparticles;
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
extern cvar_t gl_externalTextures_world, gl_externalTextures_bmodels;

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

// VBOs
typedef void (APIENTRY *glBindBuffer_t)(GLenum target, GLuint buffer);
typedef void (APIENTRY *glBufferData_t)(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage);
typedef void (APIENTRY *glBufferSubData_t)(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data);
typedef void (APIENTRY *glGenBuffers_t)(GLsizei n, GLuint* buffers);
typedef void (APIENTRY *glDeleteBuffers_t)(GLsizei n, const GLuint* buffers);
typedef void (APIENTRY *glBindBufferBase_t)(GLenum target, GLuint index, GLuint buffer);

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
typedef void (APIENTRY *glTexStorage3D_t)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
typedef void (APIENTRY *glTexImage3D_t)(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid * data);
typedef void (APIENTRY *glGenerateMipmap_t)(GLenum target);

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

// VBO functions
extern glBindBuffer_t        glBindBuffer;
extern glBufferData_t        glBufferData;
extern glBufferSubData_t     glBufferSubData;
extern glGenBuffers_t        glGenBuffers;
extern glDeleteBuffers_t     glDeleteBuffers;
extern glBindBufferBase_t    glBindBufferBase;

// VAO functions
extern glGenVertexArrays_t         glGenVertexArrays;
extern glBindVertexArray_t         glBindVertexArray;
extern glDeleteVertexArrays_t      glDeleteVertexArrays;
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
extern glTexSubImage3D_t        glTexSubImage3D;
extern glTexStorage3D_t         glTexStorage3D;
extern glGenerateMipmap_t       glGenerateMipmap;

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

qbool GL_ShadersSupported(void);
qbool GL_VBOsSupported(void);

void GL_OrthographicProjection(float left, float right, float top, float bottom, float zNear, float zFar);
void GL_TextureEnvMode(GLenum mode);

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
	const char* included_definitions;
	GLuint shader_length[GLM_SHADER_COUNT];
	qbool uniforms_found;
} glm_program_t;

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

void GL_Enable(GLenum option);
void GL_Disable(GLenum option);

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

void GLM_DrawFlatPoly(byte* color, unsigned int vao, int vertices, qbool apply_lightmap);
void GLM_DrawTexturedPoly(byte* color, unsigned int vao, int start, int vertices, qbool apply_lightmap, qbool alpha_test);

void GLM_DebugMatrix(GLenum type, const char* value);

int GLM_PopulateVBOForBrushModel(model_t* m, vbo_world_vert_t* vbo_buffer, int vbo_pos);
int GLM_MeasureVBOSizeForBrushModel(model_t* m);

void GL_UseProgram(GLuint program);
void GL_DepthFunc(GLenum func);
void GL_DepthRange(double nearVal, double farVal);
void GL_CullFace(GLenum mode);
void GL_BlendFunc(GLenum sfactor, GLenum dfactor);
void GL_BindVertexArray(GLuint vao);
void GL_ShadeModel(GLenum model);
void GL_Viewport(GLint x, GLint y, GLsizei width, GLsizei height);
void GL_BindTexture(GLenum targetType, GLuint texnum, qbool warning);
void GL_BindTextureUnit(GLuint unit, GLenum targetType, GLuint texture);
void GL_EnableFog(void);
void GL_DisableFog(void);
void GL_ConfigureFog(void);
void GL_EnableWaterFog(int contents);
void GL_InitTextureState(void);
void GL_DepthMask(GLboolean mask);
void GL_InvalidateTextureReferences(int texture);

void GL_BindBuffer(GLenum target, GLuint buffer);
void GL_GenBuffer(glm_vbo_t* vbo, const char* name);

// Creates buffer, binds to target and initialises with a particular size
void GL_GenFixedBuffer(glm_vbo_t* vbo, GLenum target, const char* name, GLsizei size, void* data, GLenum usage);
void GL_BufferData(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage);
void GL_BufferDataUpdate(GLenum target, GLsizeiptr size, const GLvoid* data);

#ifdef WITH_NVTX
void GL_EnterRegion(const char* regionName);
void GL_LeaveRegion(void);
#else
#define GL_EnterRegion(x)
#define GL_LeaveRegion()
#endif

#define NUMVERTEXNORMALS 162
#define SHADEDOT_QUANT   64

// Lightmap size
#define	LIGHTMAP_WIDTH  128
#define	LIGHTMAP_HEIGHT 128

// Chat icons
typedef byte col_t[4]; // FIXME: why 4?

typedef struct ci_player_s {
	vec3_t		org;
	col_t		color;
	float		rotangle;
	float		size;
	byte		texindex;
	int			flags;
	float       distance;

	player_info_t *player;

} ci_player_t;

typedef enum {
	citex_chat,
	citex_afk,
	citex_chat_afk,
	num_citextures,
} ci_tex_t;

#define	MAX_CITEX_COMPONENTS		8
typedef struct ci_texture_s {
	int			texnum;
	int			components;
	float		coords[MAX_CITEX_COMPONENTS][4];
} ci_texture_t;

// 
#define CHARSET_CHARS_PER_ROW	16
#define CHARSET_WIDTH			1.0
#define CHARSET_HEIGHT			1.0
#define CHARSET_CHAR_WIDTH		(CHARSET_WIDTH / CHARSET_CHARS_PER_ROW)
#define CHARSET_CHAR_HEIGHT		(CHARSET_HEIGHT / CHARSET_CHARS_PER_ROW)

// Functions

void GLM_ExitBatchedPolyRegion(void);

void GLM_DrawSimpleItem(model_t* model, int texture, vec3_t origin, vec3_t angles, float scale, float scale_s, float scale_t);
void GLC_DrawSimpleItem(int simpletexture, vec3_t org, float sprsize, vec3_t up, vec3_t right);

void GL_BeginDrawSprites(void);
void GL_EndDrawSprites(void);
void GL_BeginDrawBrushModels(void);
void GL_BeginDrawAliasModels(void);
void GL_EndDrawAliasModels(void);
void GL_EndDrawBrushModels(void);
void GL_EndDrawEntities(void);

void GLM_MultiplyMatrix(const float* lhs, const float* rhs, float* target);
void GLM_MultiplyVector(const float* matrix, const float* vector, float* result);
void GLM_MultiplyVector3f(const float* matrix, float x, float y, float z, float* result);
void GLM_MultiplyVector3fv(const float* matrix, const vec3_t vector, float* result);
void GLM_DrawWaterSurfaces(void);
void GL_BuildCommonTextureArrays(qbool vid_restart);

void R_DrawAliasModel(entity_t *ent, qbool shell_only);

// 
void R_RenderDynamicLightmaps(msurface_t *fa);
void R_DrawViewModel(void);
void R_RenderAllDynamicLightmaps(model_t *model);
void R_DrawMapOutline(model_t *model);
void R_BlendLightmaps(void);
void R_DrawPowerupShell(
	model_t* model, int effects, int layer_no,
	maliasframedesc_t *oldframe, maliasframedesc_t *frame, aliashdr_t *paliashdr
);
void R_SetupAliasFrame(model_t* model, maliasframedesc_t *oldframe, maliasframedesc_t *frame, aliashdr_t *paliashdr, qbool mtex, qbool scrolldir, qbool outline, int texture, int fb_texture, GLuint textureEnvMode, float scaleS, float scaleT, int effects, qbool is_texture_array, qbool shell_only);

void GLM_DrawTexturedWorld(model_t* model);
void GLM_DrawSpriteModel(entity_t* e);
void GLM_PolyBlend(float v_blend[4]);
void GLM_BrightenScreen(void);
void GLM_DrawVelocity3D(void);
void GLM_RenderSceneBlurDo(float alpha);
mspriteframe_t* R_GetSpriteFrame(entity_t *e, msprite2_t *psprite);

void GLC_ClearTextureChains(void);
void GLC_SetTextureLightmap(int lightmap_num);
void GLC_SetLightmapBlendFunc(void);
void GLC_MultitextureLightmap(int lightmap_num);
void GLC_SetLightmapTextureEnvironment(void);
void GLC_AliasModelPowerupShell(entity_t* ent, model_t* clmodel, maliasframedesc_t* oldframe, maliasframedesc_t* frame, aliashdr_t* paliashdr);
void GLC_AliasModelShadow(entity_t* ent, aliashdr_t* paliashdr, vec3_t shadevector, vec3_t lightspot);
void GLC_UnderwaterCaustics(entity_t* ent, model_t* clmodel, maliasframedesc_t* oldframe, maliasframedesc_t* frame, aliashdr_t* paliashdr, float scaleS, float scaleT);
void GLC_DrawSpriteModel(entity_t* e);
void GLC_PolyBlend(float v_blend[4]);
void GLC_BrightenScreen(void);
void GLC_DrawVelocity3D(void);
void GLC_RenderSceneBlurDo(float alpha);

void SCR_SetupCI(void);

void GLC_DrawTileClear(int texnum, int x, int y, int w, int h);
void GLC_Draw_LineRGB(byte* bytecolor, int x_start, int y_start, int x_end, int y_end);
void GLC_Draw_Polygon(int x, int y, vec3_t *vertices, int num_vertices, qbool fill, color_t color);

void GLM_Draw_Polygon(int x, int y, vec3_t *vertices, int num_vertices, qbool fill, color_t color);

void GLC_DrawParticles(int particles_to_draw, qbool square);
void GLC_DrawImage(float x, float y, float ofs1, float ofs2, float sl, float tl, float sh, float th);
void GLC_Draw_AlphaPieSliceRGB(int x, int y, float radius, float startangle, float endangle, float thickness, qbool fill, color_t color);
void GLC_Draw_SAlphaSubPic2(int x, int y, mpic_t *pic, int src_width, int src_height, float newsl, float newtl, float newsh, float newth, float scale_x, float scale_y, float alpha);
void GLC_DrawTileClear(int texnum, int x, int y, int w, int h);
void GLC_DrawAlphaRectangeRGB(int x, int y, int w, int h, float thickness, qbool fill, byte* bytecolor);
void GLC_EmitWaterPoly(msurface_t* fa, byte* col, float wateralpha);
void GLC_DrawFlatPoly(glpoly_t* p);
void GLC_EmitCausticsPolys(void);
void GLC_Draw_FadeScreen(float alpha);
void GLC_RenderDlight(dlight_t* light);
void GLC_DrawSkyChain(void);
void GLC_DrawSky(void);
void GLC_DrawSkyFace(int axis);
void GLC_DrawBillboard(ci_texture_t* _ptex, ci_player_t* _p, vec3_t _coord[4]);
void GLC_Draw_CharacterBase(int x, int y, wchar num, float scale, qbool apply_overall_alpha, byte color[4], qbool bigchar, qbool gl_statechange);
void GLC_DrawWaterSurfaces(void);
void GLC_DrawBrushModel(entity_t* e, model_t* clmodel);
void GLC_DrawWorld(void);
void GLC_Draw_ResetCharGLState(void);
void GLC_Draw_SetColor(byte* rgba, float alpha);
void GLC_Draw_StringBase_StartString(int x, int y, float scale);
void GLC_DrawAccelBar(int x, int y, int length, int charsize, int pos);

void GLM_Draw_SAlphaSubPic2(int x, int y, mpic_t *pic, int src_width, int src_height, float newsl, float newtl, float newsh, float newth, float scale_x, float scale_y, float alpha);
void GLM_Draw_AlphaPieSliceRGB(int x, int y, float radius, float startangle, float endangle, float thickness, qbool fill, color_t color);
void GLM_Draw_LineRGB(byte* color, int x_start, int y_start, int x_end, int y_end);
void GLM_DrawImage(float x, float y, float width, float height, int texture_unit, float tex_s, float tex_t, float tex_width, float tex_height, byte* color, qbool alpha, int texnum, qbool isText);
void GLM_DrawAlphaRectangeRGB(int x, int y, int w, int h, float thickness, qbool fill, byte* bytecolor);
void GLM_UpdateParticles(int particles_to_draw);
void GLM_DrawParticles(int number, qbool square);
void GLM_EmitCausticsPolys(void);
void GLM_Draw_FadeScreen(float alpha);
void GLM_RenderDlight(dlight_t* light);
void GLM_DrawSkyChain(void);
void GLM_DrawSky(void);
void GLM_DrawBrushModel(model_t* model, qbool polygonOffset);
void GLM_DrawBillboard(ci_texture_t* _ptex, ci_player_t* _p, vec3_t _coord[4]);
void GLM_AliasModelShadow(entity_t* ent, aliashdr_t* paliashdr, vec3_t shadevector, vec3_t lightspot);
void GLM_Draw_CharacterBase(int x, int y, wchar num, float scale, qbool apply_overall_alpha, byte color[4], qbool bigchar, qbool gl_statechange);
void GLM_Draw_ResetCharGLState(void);
void GLM_Draw_SetColor(byte* rgba, float alpha);
void GLM_Draw_StringBase_StartString(int x, int y, float scale);
void GLM_FlushImageDraw(void);
void GLM_DrawAccelBar(int x, int y, int length, int charsize, int pos);

void GLM_SetIdentityMatrix(float* matrix);
float* GLM_ModelviewMatrix(void);
float* GLM_ProjectionMatrix(void);
float* GL_MatrixForMode(GLenum type);

void GL_ProcessErrors(const char* message);
void GLM_DeletePrograms(void);
void GLM_InitPrograms(void);
void GL_DeleteBuffers(void);

#define MAX_CHARSETS 256
#define NUMCROSSHAIRS  6

void GL_InitialiseState(void);
void GL_GenUniformBuffer(glm_ubo_t* ubo, const char* name, void* data, int size);
void GL_GenVertexArray(glm_vao_t* vao);

void GL_DeleteModelData(void);
void GL_Hint(GLenum target, GLenum mode);

byte* SurfaceFlatTurbColor(texture_t* texture);

#define GLM_Enabled GL_ShadersSupported

enum {
	GL_BINDINGPOINT_REFDEF_CVARS,
	GL_BINDINGPOINT_COMMON2D_CVARS,

	GL_BINDINGPOINT_DRAWWORLD_CVARS,
	GL_BINDINGPOINT_BRUSHMODEL_CVARS,
	GL_BINDINGPOINT_ALIASMODEL_CVARS,
	GL_BINDINGPOINT_SPRITEDATA_CVARS,

	GL_BINDINGPOINT_SKYDOME_CVARS,

	GL_BINDINGPOINT_COUNT
};

void GL_PreRenderView(void);
void GLC_PreRenderView(void);
void GLM_PreRenderView(void);

void GLC_SetupGL(void);
void GLM_SetupGL(void);

// Billboards
typedef enum {
	BILLBOARD_PARTICLES_CLASSIC,
	BILLBOARD_FLASHBLEND_LIGHTS,
	BILLBOARD_CORONAS,
	BILLBOARD_CHATICONS,

	MAX_BILLBOARD_BATCHES
} billboard_batch_id;

void GL_BillboardInitialiseBatch(billboard_batch_id type, GLenum blendSource, GLenum blendDestination, GLuint texture);
qbool GL_BillboardAddEntry(billboard_batch_id type, int verts_required);
void GL_BillboardAddVert(billboard_batch_id type, float x, float y, float z, float s, float t, GLubyte color[4]);
void GL_DrawBillboards(void);

#endif /* !__GL_LOCAL_H__ */


