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
#include "r_framestats.h"
#include "r_trace.h"
#include "r_local.h"

#ifndef APIENTRY
#define APIENTRY
#endif

//#define GL_PARANOIA

void R_TimeRefresh_f (void);
texture_t *R_TextureAnimation(entity_t* ent, texture_t *base);

//====================================================

extern	entity_t	r_worldentity;
extern	vec3_t		modelorg;
extern	int			r_visframecount;
extern	int			r_framecount;
extern	mplane_t	frustum[4];

// view origin
extern	vec3_t	vup;
extern	vec3_t	vpn;
extern	vec3_t	vright;
extern	vec3_t	r_origin;

// screen size info
extern	texture_t	*r_notexture_mip;
extern	unsigned int d_lightstylevalue[256];	// 8.8 fraction of base light value

extern	texture_ref netgraphtexture;

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
extern	cvar_t	gl_polyblend;
extern	cvar_t	gl_nocolors;
extern	cvar_t	gl_finish;
extern	cvar_t	gl_fb_bmodels;
extern	cvar_t	gl_fb_models;
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

extern	float	r_world_matrix[16];

extern	const char *gl_vendor;
extern	const char *gl_renderer;
extern	const char *gl_version;
extern	const char *gl_extensions;

#define ISUNDERWATER(contents) (contents == CONTENTS_WATER || contents == CONTENTS_SLIME || contents == CONTENTS_LAVA)
//#define TruePointContents(p) CM_HullPointContents(&cl.worldmodel->hulls[0], 0, p)
#define TruePointContents(p) CM_HullPointContents(&cl.clipmodels[1]->hulls[0], 0, p) // ?TONIK?

// gl_rmain.c
qbool R_CullBox (vec3_t mins, vec3_t maxs);
qbool R_CullSphere (vec3_t centre, float radius);
void R_RotateForEntity (entity_t *e);
void R_BrightenScreen (void);

// gl_refrag.c
void R_StoreEfrags (efrag_t **ppefrag);

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

extern cvar_t vid_renderer;
extern cvar_t vid_gl_core_profile;

// Which renderer to use
#define GL_UseGLSL()              (vid_renderer.integer == 1)
#define GL_UseImmediateMode()     (vid_renderer.integer == 0)

#define GL_VersionAtLeast(major, minor) (glConfig.majorVersion > (major) || (glConfig.majorVersion == (major) && glConfig.minorVersion >= (minor)))

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

void GL_TextureEnvMode(GLenum mode);
void GL_TextureEnvModeForUnit(GLenum unit, GLenum mode);

#define GL_ALPHATEST_NOCHANGE 0
#define GL_ALPHATEST_ENABLED  1
#define GL_ALPHATEST_DISABLED 2
#define GL_BLEND_NOCHANGE 0
#define GL_BLEND_ENABLED  4
#define GL_BLEND_DISABLED 8

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

//#define glEnable GL_Enable
//#define glDisable GL_Disable
#define glBegin GL_Begin
#define glEnd GL_End
#define glVertex2f GL_Vertex2f
#define glVertex3f GL_Vertex3f
#define glVertex3fv GL_Vertex3fv

void GL_Begin(GLenum primitive);
void GL_End(void);
void GL_Vertex2f(GLfloat x, GLfloat y);
void GL_Vertex3f(GLfloat x, GLfloat y, GLfloat z);
void GL_Vertex3fv(const GLfloat* v);

int GL_PopulateVBOForBrushModel(model_t* m, void* vbo_buffer, int vbo_pos);
int GL_MeasureVBOSizeForBrushModel(model_t* m);
void GL_CreateAliasModelVBO(buffer_ref instance_vbo);
void GL_CreateBrushModelVBO(buffer_ref instance_vbo);

void GL_UseProgram(GLuint program);
void GL_InitTextureState(void);
void GL_InvalidateTextureReferences(GLuint texture);

// gl_fog.c
void GL_EnableFog(void);
void GL_DisableFog(void);
void GL_ConfigureFog(void);
void GL_EnableWaterFog(int contents);

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

void GLM_DrawWaterSurfaces(void);

void GL_BuildCommonTextureArrays(qbool vid_restart);
void GL_CreateModelVBOs(qbool vid_restart);

// 
void R_RenderDynamicLightmaps(msurface_t *fa);
void R_DrawViewModel(void);
void R_LightmapFrameInit(void);
void R_UploadChangedLightmaps(void);

void GLM_RenderView(void);
void GLM_UploadFrameConstants(void);
void GLM_PrepareWorldModelBatch(void);

void GLC_DrawMapOutline(model_t *model);
void R_SetupAliasFrame(entity_t* ent, model_t* model, maliasframedesc_t *oldframe, maliasframedesc_t *frame, qbool mtex, qbool scrolldir, qbool outline, texture_ref texture, texture_ref fb_texture, int effects, int render_effects);
int R_AliasFramePose(maliasframedesc_t* frame);

void GLM_EnterBatchedWorldRegion(void);
void GLM_DrawSpriteModel(entity_t* e);
void GLM_PolyBlend(float v_blend[4]);
void GLM_DrawVelocity3D(void);
void GLM_RenderSceneBlurDo(float alpha);
mspriteframe_t* R_GetSpriteFrame(entity_t *e, msprite2_t *psprite);

void GLC_ClearTextureChains(void);
void GLC_SetTextureLightmap(int textureUnit, int lightmap_num);
texture_ref GLC_LightmapTexture(int index);
texture_ref GLM_LightmapArray(void);
void GLC_ClearLightmapPolys(void);
void GLC_AddToLightmapChain(msurface_t* s);
void GLC_LightmapUpdate(int index);
GLenum GLC_LightmapDestBlendFactor(void);
glpoly_t* GLC_LightmapChain(int i);
GLenum GLC_LightmapTexEnv(void);
int GLC_LightmapCount(void);
void GLM_CreateLightmapTextures(void);
void GLM_PostProcessScreen(void);
void GLC_CreateLightmapTextures(void);
void GLC_DrawSpriteModel(entity_t* e);
void GLC_PolyBlend(float v_blend[4]);
void GLC_BrightenScreen(void);
//void GLC_DrawVelocity3D(void);
void GLC_RenderSceneBlurDo(float alpha);

void GLC_EmitWaterPoly(msurface_t* fa);
void GLC_DrawFlatPoly(glpoly_t* p);
void GLC_EmitCausticsPolys(qbool use_vbo);

void GLC_DrawWaterSurfaces(void);
void GLC_DrawWorld(void);

/*void GLM_Draw_SAlphaSubPic2(int x, int y, mpic_t *pic, int src_width, int src_height, float newsl, float newtl, float newsh, float newth, float scale_x, float scale_y, float alpha);
void GLM_Draw_AlphaPieSliceRGB(int x, int y, float radius, float startangle, float endangle, float thickness, qbool fill, color_t color);
void GLM_Draw_LineRGB(float thickness, byte* color, int x_start, int y_start, int x_end, int y_end);
void GLM_DrawImage(float x, float y, float width, float height, float tex_s, float tex_t, float tex_width, float tex_height, byte* color, qbool alpha_test, texture_ref texnum, qbool isText, qbool isCrosshair);
void GLM_DrawAlphaRectangleRGB(int x, int y, int w, int h, float thickness, qbool fill, byte* bytecolor);
void GLM_Draw_FadeScreen(float alpha);
float GLM_Draw_CharacterBase(float x, float y, wchar num, float scale, qbool apply_overall_alpha, byte color[4], qbool bigchar, qbool gl_statechange, qbool proportional);
void GLM_Draw_ResetCharGLState(void);
void GLM_Draw_SetColor(byte* rgba);
void GLM_Draw_StringBase_StartString(int x, int y, float scale);
void GLM_PrepareImageDraw(void);
void GLM_DrawAccelBar(int x, int y, int length, int charsize, int pos);
void GLM_Cache2DMatrix(void);
void GLM_UndoLastCharacter(void);
*/

#ifdef GL_PARANOIA
void GL_ProcessErrors(const char* message);
#define GL_Paranoid_Printf(...) Con_Printf(__VA_ARGS__)
#else
#define GL_Paranoid_Printf(...)
#endif
void GLM_DeletePrograms(qbool restarting);
void GLM_InitPrograms(void);
//void GL_DeleteVAOs(void);

void GLC_FreeAliasPoseBuffer(void);
void CachePics_Shutdown(void);

void GL_LightmapShutdown(void);
void GLM_DeleteBrushModelIndexBuffer(void);

void GL_InitialiseState(void);

void GL_ClearModelTextureData(void);
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

void GLC_PreRenderView(void);
void GLM_PreRenderView(void);

void GLC_SetupGL(void);
void GLM_SetupGL(void);

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
void GLM_StateBeginAliasOutlineBatch(void);
void GLM_StateBeginAliasModelBatch(qbool translucent);
void GL_StateBeginPolyBlend(void);
void GLC_StateBeginBrightenScreen(void);
void GLC_StateBeginFastSky(void);
void GLC_StateBeginSkyZBufferPass(void);
void GLC_StateBeginSingleTextureSkyDome(void);
void GLC_StateBeginSingleTextureSkyDomeCloudPass(void);
void GLC_StateBeginMultiTextureSkyDome(void);
void GLC_StateBeginMultiTextureSkyChain(void);
void GLC_StateBeginSingleTextureSkyPass(void);
void GLC_StateBeginSingleTextureCloudPass(void);
void GLC_StateBeginRenderFullbrights(void);
void GLC_StateBeginRenderLumas(void);
void GLC_StateBeginEmitDetailPolys(void);
void GLC_StateBeginDrawMapOutline(void);
void GLM_StateBeginDraw3DSprites(void);
void GLM_StateBeginDrawWorldOutlines(void);
void GLM_BeginDrawWorld(qbool alpha_surfaces, qbool polygon_offset);
void GL_StateBeginAlphaLineRGB(float thickness);
void GLC_StateBeginDrawAliasFrame(texture_ref texture, texture_ref fb_texture, qbool mtex, qbool alpha_blend, struct custom_model_color_s* custom_model, qbool weapon_model);
void GLC_StateBeginAliasModelShadow(void);
void GLC_StateBeginFastTurbPoly(byte color[4]);
void GLC_StateBeginBlendLightmaps(qbool use_buffers);
void GLC_StateBeginSceneBlur(void);
void GLC_StateBeginCausticsPolys(void);
void GL_StateDefault3D(void);
void GL_StateDefaultInit(void);
void GLC_StateBeginBloomDraw(texture_ref texture);
void GLC_StateBeginImageDraw(qbool is_text);
void GLC_StateBeginPolyBlend(float v_blend[4]);
void GLC_StateBeginDrawPolygon(void);
void GLC_StateBeginDrawAlphaPieSliceRGB(float thickness);
void R_SetupFrame(void);

void GL_FlushWorldModelBatch(void);
void GL_InitialiseFramebufferHandling(void);

float GL_WaterAlpha(void);

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

void GL_BindImageTexture(GLuint unit, texture_ref texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format);

#ifndef EZ_OPENGL_NO_EXTENSIONS
#define GL_LoadMandatoryFunctionExtension(functionName,testFlag) { testFlag &= ((q##functionName = (functionName##_t)SDL_GL_GetProcAddress(#functionName)) != NULL); }
#define GL_LoadOptionalFunction(functionName) { q##functionName = (functionName##_t)SDL_GL_GetProcAddress(#functionName); }
#define GL_UseDirectStateAccess() (SDL_GL_ExtensionSupported("GL_ARB_direct_state_access"))
#else
#define GL_LoadMandatoryFunctionExtension(functionName,testFlag) { q##functionName = NULL; testFlag = false; }
#define GL_LoadOptionalFunction(functionName) { q##functionName = NULL; }
#define GL_UseDirectStateAccess() (false)
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

void GLM_StateBeginImageDraw(void);
void GLM_StateBeginPolygonDraw(void);

void GL_Viewport(GLint x, GLint y, GLsizei width, GLsizei height);
void GL_PopulateConfig(void);

/*
void GL_DepthFunc(GLenum func);
void GL_DepthRange(double nearVal, double farVal);
void GL_CullFace(GLenum mode);
void GL_BlendFunc(GLenum sfactor, GLenum dfactor);
void GL_DepthMask(GLboolean mask);
void GL_PolygonMode(GLenum mode);
int GL_AlphaBlendFlags(int modes);
void GL_ClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void GL_Enable(GLenum option);
void GL_PolygonOffset(int options);
void GL_Disable(GLenum option);
void GL_AlphaFunc(GLenum func, GLclampf threshold);

void GLC_DisableAllTexturing(void);
void GLC_InitTextureUnitsNoBind1(GLenum envMode0);
void GLC_InitTextureUnits1(texture_ref texture0, GLenum envMode0);
void GLC_InitTextureUnits2(texture_ref texture0, GLenum envMode0, texture_ref texture1, GLenum envMode1);
*/

#endif /* !__GL_LOCAL_H__ */

