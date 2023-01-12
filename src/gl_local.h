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
#ifdef GL_GLEXT_VERSION
#undef GL_GLEXT_VERSION
#endif
#ifdef GL_DRAW_FRAMEBUFFER_BINDING
#undef GL_DRAW_FRAMEBUFFER_BINDING
#endif
#include "opengl/glext.h"  // Should be <OpenGL/glext.h> but appears broken on newer macOS

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

#include "gl_model.h"
#include "r_framestats.h"
#include "r_trace.h"
#include "r_local.h"
//#include "gl_texture.h"

#ifndef APIENTRY
#define APIENTRY
#endif

void R_TimeRefresh_f (void);
texture_t *R_TextureAnimation(entity_t* ent, texture_t *base);

//====================================================

// view origin
extern	vec3_t	vup;
extern	vec3_t	vpn;
extern	vec3_t	vright;
extern	vec3_t	r_origin;

// screen size info
extern	texture_t	*r_notexture_mip;

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
extern	cvar_t	r_drawflat;
extern	cvar_t	r_drawflat_mode;
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
extern  cvar_t  gl_fog;
extern  cvar_t  gl_fog_density;
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

extern cvar_t vid_gammacorrection;

// gl_rmain.c
qbool R_CullBox (vec3_t mins, vec3_t maxs);
qbool R_CullSphere (vec3_t centre, float radius);
void R_BrightenScreen (void);

extern int dlightcolor[NUM_DLIGHTTYPES][3];

// gl_ngraph.c
//void R_NetGraph (void); // HUD -> hexum
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

extern byte color_white[4], color_black[4];
extern qbool gl_mtexable;
extern int gl_textureunits;

void GL_LoadProgramFunctions(void);
void GL_LoadStateFunctions(void);
void GL_LoadTextureManagementFunctions(void);
void GL_LoadDrawFunctions(void);
void GL_InitialiseDebugging(void);

// Which renderer to use
#define GL_VersionAtLeast(major, minor) (glConfig.majorVersion > (major) || (glConfig.majorVersion == (major) && glConfig.minorVersion >= (minor)))

void GL_TextureInitialiseState(void);
void GL_InvalidateTextureReferences(GLuint texture);

// Functions
void GL_BeginDrawSprites(void);
void GL_EndDrawSprites(void);

// 
void R_RenderDynamicLightmaps(msurface_t *fa, qbool world);
void R_DrawViewModel(void);
void R_LightmapFrameInit(void);
void R_UploadChangedLightmaps(void);
void R_UploadLightMap(int textureUnit, int lightmapnum);

mspriteframe_t* R_GetSpriteFrame(entity_t *e, msprite2_t *psprite);

void GLC_DrawSpriteModel(entity_t* e);
void GLC_PolyBlend(float v_blend[4]);
void GLC_BrightenScreen(void);
void GLC_EmitCausticsPolys(void);
void GLC_DrawWorld(void);
void GLC_ClearTextureChains(void);
qbool GLC_SetTextureLightmap(int textureUnit, int lightmap_num);
qbool GLC_IsLightmapBound(int textureUnit, int lightmap_num);
texture_ref GLC_LightmapTexture(int index);
void GLC_ClearLightmapPolys(void);
void GLC_AddToLightmapChain(msurface_t* s);
void GLC_LightmapUpdate(int index);
glpoly_t* GLC_LightmapChain(int i);
int GLC_LightmapCount(void);
void GLC_DrawMapOutline(model_t *model);
void GLC_MultiTexCoord2f(GLenum target, float s, float t);

void GLM_DrawSpriteModel(entity_t* e);
void GLM_PolyBlend(float v_blend[4]);
texture_ref GLM_LightmapArray(void);
void GLM_PostProcessScreen(void);
void GLM_RenderView(void);
void GLM_UploadFrameConstants(void);
void GLM_PrepareWorldModelBatch(void);

void GL_InitialiseState(void);

byte* SurfaceFlatTurbColor(texture_t* texture);

struct custom_model_color_s;

void R_StateBeginAlphaLineRGB(float thickness);
void R_SetupFrame(void);

void GL_InitialiseFramebufferHandling(void);

// Rendering functions
void GL_DrawArrays(GLenum mode, GLint first, GLsizei count);
void GL_MultiDrawArrays(GLenum mode, GLint* first, GLsizei* count, GLsizei primcount);
void GL_DrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, GLvoid* indices, GLint basevertex);
void GL_DrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);
void GL_MultiDrawArraysIndirect(GLenum mode, const void* indirect, GLsizei drawcount, GLsizei stride);
void GL_MultiDrawElementsIndirect(GLenum mode, GLenum type, const void* indirect, GLsizei drawcount, GLsizei stride);
void GL_DrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count, GLenum type, GLvoid* indices, GLsizei primcount, GLint basevertex, GLuint baseinstance);
qbool GL_DrawElementsBaseVertexAvailable(void);

void GL_BindImageTexture(GLuint unit, texture_ref texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format);
GLenum GL_ProcessAllErrors(const char* message);

#ifdef WITH_RENDERING_TRACE
#define GL_ProcessErrors GL_ProcessAllErrors

#define GL_LoadRequiredFunction(varName, functionName)           (((varName) = (functionName##_t)SDL_GL_GetProcAddress(#functionName)) != NULL)
#define GL_LoadMandatoryFunction(functionName,testFlag)          { testFlag &= ((q##functionName##_impl = (functionName##_t)SDL_GL_GetProcAddress(#functionName)) != NULL); }
#define GL_LoadMandatoryFunctionEXT(functionName,testFlag)       { testFlag &= ((q##functionName##_impl = (functionName##_t)SDL_GL_GetProcAddress(#functionName "EXT")) != NULL); }
#define GL_LoadMandatoryFunctionExtension(functionName,testFlag) { testFlag &= ((q##functionName##_impl = (functionName##_t)SDL_GL_GetProcAddress(#functionName)) != NULL); }
#define GL_InvalidateFunction(functionName)      { q##functionName##_impl = NULL; }
#define GL_LoadOptionalFunction(functionName)    { q##functionName##_impl = (functionName##_t)SDL_GL_GetProcAddress(#functionName); }
#define GL_LoadOptionalFunctionEXT(functionName) { q##functionName##_impl = (functionName##_t)SDL_GL_GetProcAddress(#functionName "EXT"); }
#define GL_LoadOptionalFunctionARB(functionName) { q##functionName##_impl = (functionName##_t)SDL_GL_GetProcAddress(#functionName "ARB"); }
#define GL_UseDirectStateAccess() (SDL_GL_ExtensionSupported("GL_ARB_direct_state_access"))
#define GL_StaticProcedureDeclaration(name, formatString, ...) \
	typedef void (APIENTRY *name ## _t)(__VA_ARGS__); \
	static name ## _t    q ## name ## _impl; \
	static const char* q ## name ## _formatString = formatString;
#define GL_StaticFunctionDeclaration(name, formatString, resultFormatString, returnType, ...)\
	typedef returnType (APIENTRY *name ## _t)(__VA_ARGS__); \
	static name ## _t    q ## name ## _impl; \
	static const char* q ## name ## _formatString = formatString; \
	static const char* q ## name ## _resultString = resultFormatString; \
	static returnType GL_Wrapper_ ## name(__VA_ARGS__)

#define GL_StaticFunctionWrapperBody(name, returnType, ...) \
{ \
	returnType result; \
	if (COM_CheckParm(cmdline_param_client_video_r_trace)) { \
		const char* args = va(q ## name ## _formatString, __VA_ARGS__); \
\
		R_TraceAPI("%s(%s)@%s,%d", #name, args, __FILE__, __LINE__); \
	} \
	result = q ## name ## _impl(__VA_ARGS__); \
	if (COM_CheckParm(cmdline_param_client_video_r_debug) || COM_CheckParm(cmdline_param_client_video_r_trace)) { \
		GL_ProcessErrors(#name); \
		R_TraceAPI(q ## name ## _resultString, result); \
	} \
\
	return result; \
}

#define GL_StaticFunctionWrapperBodyNoArgs(name, returnType) \
{ \
	returnType result; \
	if (COM_CheckParm(cmdline_param_client_video_r_trace)) { \
		R_TraceAPI("%s()@%s,%d", #name, __FILE__, __LINE__); \
	} \
	result = q ## name ## _impl(); \
	if (COM_CheckParm(cmdline_param_client_video_r_debug) || COM_CheckParm(cmdline_param_client_video_r_trace)) { \
		GL_ProcessErrors(#name); \
		R_TraceAPI(q ## name ## _resultString, result); \
	} \
\
	return result; \
}

#define GL_Procedure(name, ...) \
{ \
	if (COM_CheckParm(cmdline_param_client_video_r_debug)) { \
		const char* ez_gldebug_args = va(q ## name ## _formatString, __VA_ARGS__); \
\
		R_TraceAPI("%s(%s)@%s,%d", #name, ez_gldebug_args, __FILE__, __LINE__); \
	} \
	q ## name ## _impl(__VA_ARGS__); \
	if (COM_CheckParm(cmdline_param_client_video_r_debug) || COM_CheckParm(cmdline_param_client_video_r_trace)) { \
		GL_ProcessErrors(#name); \
	} \
}

#define GL_ProcedureNoArgs(name) \
{ \
	if (COM_CheckParm(cmdline_param_client_video_r_trace)) { \
		R_TraceAPI("%s()@%s,%d", #name, __FILE__, __LINE__); \
	} \
	q ## name ## _impl(); \
	if (COM_CheckParm(cmdline_param_client_video_r_debug) || COM_CheckParm(cmdline_param_client_video_r_trace)) { \
		GL_ProcessErrors(#name); \
	} \
}

#define GL_ProcedureReturnError(name, ...) \
{ \
	GLenum ez_gl_error = GL_NO_ERROR; \
	if (COM_CheckParm(cmdline_param_client_video_r_trace)) { \
		const char* ez_gldebug_args = va(q ## name ## _formatString, __VA_ARGS__); \
\
		R_TraceAPI("%s(%s)@%s,%d", #name, ez_gldebug_args, __FILE__, __LINE__); \
	} \
	q ## name ## _impl(__VA_ARGS__); \
	if (COM_CheckParm(cmdline_param_client_video_r_debug) || COM_CheckParm(cmdline_param_client_video_r_trace)) { \
		ez_gl_error = GL_ProcessErrors(#name); \
	} \
	else { \
		ez_gl_error = glGetError(); \
	} \
	return ez_gl_error; \
}

#define GL_ProcedureReturnIfError(name, ...) \
{ \
	GLenum ez_gl_error = GL_NO_ERROR; \
	if (COM_CheckParm(cmdline_param_client_video_r_trace)) { \
		const char* ez_gldebug_args = va(q ## name ## _formatString, __VA_ARGS__); \
\
		R_TraceAPI("%s(%s)@%s,%d", #name, ez_gldebug_args, __FILE__, __LINE__); \
	} \
	q ## name ## _impl(__VA_ARGS__); \
	if (COM_CheckParm(cmdline_param_client_video_r_debug) || COM_CheckParm(cmdline_param_client_video_r_trace)) { \
		ez_gl_error = GL_ProcessErrors(#name); \
	} \
	else { \
		ez_gl_error = glGetError(); \
	} \
	if (ez_gl_error != GL_NO_ERROR) { \
		return ez_gl_error; \
	} \
}

#define GL_BuiltinProcedure(name, formatString, ...) \
{ \
	if (COM_CheckParm(cmdline_param_client_video_r_trace)) { \
		const char* ez_gldebug_args = va(formatString, __VA_ARGS__); \
\
		R_TraceAPI("%s(%s)@%s,%d", #name, ez_gldebug_args, __FILE__, __LINE__); \
	} \
	name(__VA_ARGS__); \
	if (COM_CheckParm(cmdline_param_client_video_r_debug) || COM_CheckParm(cmdline_param_client_video_r_trace)) { \
		GL_ProcessErrors(#name); \
	} \
}
#define GL_Function(name, ...)    GL_Wrapper_ ## name(__VA_ARGS__)
#define GL_FunctionNoArgs(name)   GL_Wrapper_ ## name()
#define GL_BuiltinFunction(name, returnType, ...)
#define GL_Available(name)        ((q ## name ## _impl) != NULL)
#else
// No direct tracing
#ifndef EZ_OPENGL_NO_EXTENSIONS
#define GL_LoadRequiredFunction(varName, functionName) (((varName) = (functionName##_t)SDL_GL_GetProcAddress(#functionName)) != NULL)
#define GL_LoadMandatoryFunction(functionName,testFlag) { testFlag &= ((q##functionName##_impl = (functionName##_t)SDL_GL_GetProcAddress(#functionName)) != NULL); }
#define GL_LoadMandatoryFunctionEXT(functionName,testFlag) { testFlag &= ((q##functionName##_impl = (functionName##_t)SDL_GL_GetProcAddress(#functionName "EXT")) != NULL); }
#define GL_LoadMandatoryFunctionExtension(functionName,testFlag) { testFlag &= ((q##functionName##_impl = (functionName##_t)SDL_GL_GetProcAddress(#functionName)) != NULL); }
#define GL_LoadOptionalFunction(functionName) { q##functionName##_impl = (functionName##_t)SDL_GL_GetProcAddress(#functionName); }
#define GL_LoadOptionalFunctionEXT(functionName) { q##functionName##_impl = (functionName##_t)SDL_GL_GetProcAddress(#functionName "EXT"); }
#define GL_LoadOptionalFunctionARB(functionName) { q##functionName##_impl = (functionName##_t)SDL_GL_GetProcAddress(#functionName "ARB"); }
#define GL_UseDirectStateAccess() (SDL_GL_ExtensionSupported("GL_ARB_direct_state_access"))
#else
#define GL_LoadMandatoryFunction(functionName,testFlag) { q##functionName##_impl = NULL; testFlag = false; }
#define GL_LoadMandatoryFunctionEXT(functionName,testFlag) { q##functionName##_impl = NULL; testFlag = false; }
#define GL_LoadMandatoryFunctionExtension(functionName,testFlag) { q##functionName##_impl = NULL; testFlag = false; }
#define GL_LoadOptionalFunction(functionName) { q##functionName##_impl = NULL; }
#define GL_LoadOptionalFunctionEXT(functionName) { q##functionName##_impl = NULL; }
#define GL_LoadOptionalFunctionARB(functionName) { q##functionName##_impl = NULL; }
#define GL_UseDirectStateAccess() (false)
#endif
#define GL_InvalidateFunction(functionName)      { q##functionName##_impl = NULL; }
#define GL_StaticProcedureDeclaration(name, formatString, ...) \
	typedef void (APIENTRY *name ## _t)(__VA_ARGS__); \
	static name ## _t    q ## name ## _impl;
#define GL_StaticFunctionDeclaration(name, formatString, resultFormatString, returnType, ...) \
	typedef returnType (APIENTRY *name ## _t)(__VA_ARGS__); \
	static name ## _t    q ## name ## _impl;
#define GL_StaticFunctionWrapperBody(...)
#define GL_StaticFunctionWrapperBodyNoArgs(...)
#define GL_Procedure(name, ...)   { q ## name ## _impl(__VA_ARGS__); }
#define GL_ProcedureNoArgs(name)  { q ## name ## _impl(); }
#define GL_BuiltinProcedure(name, formatString, ...) { name(__VA_ARGS__); }
#define GL_Function(name, ...) q ## name ## _impl(__VA_ARGS__)
#define GL_FunctionNoArgs(name)   q ## name ## _impl()
#define GL_Available(name) ((q ## name ## _impl) != NULL)
#define GL_ProcedureReturnError(name, ...) { q ## name ## _impl(__VA_ARGS__); return glGetError(); }
#define GL_ProcedureReturnIfError(name, ...) { \
	GLenum ez_gl_error; \
	q ## name ## _impl(__VA_ARGS__); \
	ez_gl_error = glGetError(); \
	if (ez_gl_error != GL_NO_ERROR) { \
		return ez_gl_error; \
	} \
}
#define GL_ProcessErrors(text)
#endif // 

void GLC_ClientActiveTexture(GLenum texture_unit);
void GL_ConsumeErrors(void);

void VK_PrintGfxInfo(void);

// Trace functions
#ifdef WITH_RENDERING_TRACE
void GL_TraceObjectLabelSet(GLenum identifier, GLuint name, int length, const char* label);
void GL_TraceObjectLabelGet(GLenum identifier, GLuint name, int bufSize, int* length, char* label);
#else
#define GL_TraceObjectLabelSet(...)
#define GL_TraceObjectLabelGet(...)
#endif

// For context creation
typedef struct opengl_version_s {
	int majorVersion;
	int minorVersion;
	qbool core;
	qbool legacy;
} opengl_version_t;

SDL_GLContext GL_SDL_CreateBestContext(SDL_Window* window, const opengl_version_t* versions, int count);

void GL_PackAlignment(int alignment_in_bytes);

#endif /* !__GL_LOCAL_H__ */
