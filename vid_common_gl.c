/*
Copyright (C) 2002-2003 A Nourai

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

// vid_common_gl.c -- Common code for vid_wgl.c and vid_glx.c

#include <SDL.h>

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "tr_types.h"
#include "image.h"

#ifdef WITH_NVTX
#define DEBUG_FRAME_DEPTH_CHARS 2
#include "nvToolsExt.h"
#endif

static qbool dev_frame_debug_queued;

// <DSA-functions (4.5)>
// These allow modification of textures without binding (-bind-to-edit)
typedef void (APIENTRY *glCreateTextures_t)(GLenum target, GLsizei n, GLuint* textures);
typedef void (APIENTRY *glGetnTexImage_t)(GLenum target, GLint level, GLenum format, GLenum type, GLsizei bufSize, void* pixels);
typedef void (APIENTRY *glGetTextureImage_t)(GLuint texture, GLint level, GLenum format, GLenum type, GLsizei bufSize, void *pixels);
typedef void (APIENTRY *glGetTextureLevelParameterfv_t)(GLuint texture, GLint level, GLenum pname, GLfloat *params);
typedef void (APIENTRY *glGetTextureLevelParameteriv_t)(GLuint texture, GLint level, GLenum pname, GLint *params);
typedef void (APIENTRY *glTextureParameteri_t)(GLuint texture, GLenum pname, GLint param);
typedef void (APIENTRY *glTextureParameterf_t)(GLuint texture, GLenum pname, GLfloat param);
typedef void (APIENTRY *glTextureParameterfv_t)(GLuint texture, GLenum pname, const GLfloat *params);
typedef void (APIENTRY *glTextureParameteriv_t)(GLuint texture, GLenum pname, const GLint *params);
typedef void (APIENTRY *glGenerateTextureMipmap_t)(GLuint texture);
typedef void (APIENTRY *glTextureStorage3D_t)(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
typedef void (APIENTRY *glTextureStorage2D_t)(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRY *glTextureSubImage2D_t)(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRY *glTextureSubImage3D_t)(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid * pixels);
 
static glGetTextureLevelParameterfv_t glGetTextureLevelParameterfv = NULL;
static glGetTextureLevelParameteriv_t glGetTextureLevelParameteriv = NULL;
static glGenerateTextureMipmap_t glGenerateTextureMipmap = NULL;
static glGetTextureImage_t glGetTextureImage = NULL;
static glCreateTextures_t glCreateTextures = NULL;
static glGetnTexImage_t glGetnTexImage = NULL;
static glTextureParameteri_t glTextureParameteri = NULL;
static glTextureParameterf_t glTextureParameterf = NULL;
static glTextureParameterfv_t glTextureParameterfv = NULL;
static glTextureParameteriv_t glTextureParameteriv = NULL;
static glTextureStorage3D_t glTextureStorage3D = NULL;
static glTextureStorage2D_t glTextureStorage2D = NULL;
static glTextureSubImage2D_t glTextureSubImage2D = NULL;
static glTextureSubImage3D_t glTextureSubImage3D = NULL;

#define OPENGL_LOAD_DSA_FUNCTION(x) x = (x ## _t)SDL_GL_GetProcAddress(#x);

// <debug-functions (4.3)>
//typedef void (APIENTRY *DEBUGPROC)(GLenum source, GLenum type, GLuint id, GLenum severity,  GLsizei length, const GLchar *message, const void *userParam);
typedef void (APIENTRY *glDebugMessageCallback_t)(GLDEBUGPROC callback, void* userParam);
// </debug-functions>

// <draw-functions (various)>
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
glMultiDrawArrays_t                               glMultiDrawArrays;
glMultiDrawElements_t                             glMultiDrawElements;
glMultiDrawArraysIndirect_t                       glMultiDrawArraysIndirect;
glMultiDrawElementsIndirect_t                     glMultiDrawElementsIndirect;
glDrawArraysInstancedBaseInstance_t               glDrawArraysInstancedBaseInstance;
glDrawElementsInstancedBaseInstance_t             glDrawElementsInstancedBaseInstance;
glDrawElementsInstancedBaseVertexBaseInstance_t   glDrawElementsInstancedBaseVertexBaseInstance;
glPrimitiveRestartIndex_t                         glPrimitiveRestartIndex;
glDrawElementsBaseVertex_t                        glDrawElementsBaseVertex;
// </draw-functions>

GLuint GL_TextureNameFromReference(texture_ref ref);
GLenum GL_TextureTargetFromReference(texture_ref ref);

void GL_BindBuffer(buffer_ref ref);

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

int anisotropy_ext = 0;

qbool gl_mtexable = false;
int gl_textureunits = 1;
lpMTexFUNC qglMultiTexCoord2f = NULL;
lpSelTexFUNC qglActiveTexture = NULL;
PFNGLCLIENTACTIVETEXTUREPROC qglClientActiveTexture = NULL;

qbool gl_combine = false;

qbool gl_add_ext = false;

float vid_gamma = 1.0;
byte vid_gamma_table[256];

unsigned short d_8to16table[256];
unsigned d_8to24table[256];
unsigned d_8to24table2[256];

byte color_white[4] = {255, 255, 255, 255};
byte color_black[4] = {0, 0, 0, 255};

void OnChange_gl_ext_texture_compression(cvar_t *, char *, qbool *);

cvar_t	gl_strings = {"gl_strings", "", CVAR_ROM | CVAR_SILENT};
cvar_t	gl_ext_texture_compression = {"gl_ext_texture_compression", "0", CVAR_SILENT, OnChange_gl_ext_texture_compression};
cvar_t  gl_maxtmu2 = {"gl_maxtmu2", "0", CVAR_LATCH};

// GL_ARB_texture_non_power_of_two
qbool gl_support_arb_texture_non_power_of_two = false;
cvar_t gl_ext_arb_texture_non_power_of_two = {"gl_ext_arb_texture_non_power_of_two", "1", CVAR_LATCH};

// Debugging
void APIENTRY MessageCallback( GLenum source,
	GLenum type,
	GLuint id,
	GLenum severity,
	GLsizei length,
	const GLchar* message,
	const void* userParam
)
{
	char buffer[1024] = { 0 };

	snprintf(buffer, sizeof(buffer) - 1,
		"GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
		(type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type, severity, message);

	OutputDebugString(buffer);
}

// Shader functions
glCreateShader_t      glCreateShader = NULL;
glShaderSource_t      glShaderSource = NULL;
glCompileShader_t     glCompileShader = NULL;
glDeleteShader_t      glDeleteShader = NULL;
glGetShaderInfoLog_t  glGetShaderInfoLog = NULL;
glGetShaderiv_t       glGetShaderiv = NULL;

// Program functions
glCreateProgram_t     glCreateProgram = NULL;
glLinkProgram_t       glLinkProgram = NULL;
glDeleteProgram_t     glDeleteProgram = NULL;
glGetProgramiv_t      glGetProgramiv = NULL;
glGetProgramInfoLog_t glGetProgramInfoLog = NULL;
glUseProgram_t        glUseProgram = NULL;
glAttachShader_t      glAttachShader = NULL;
glDetachShader_t      glDetachShader = NULL;

// Uniforms
glGetUniformLocation_t      glGetUniformLocation = NULL;
glUniform1f_t               glUniform1f;
glUniform1fv_t              glUniform1fv;
glUniform2f_t               glUniform2f;
glUniform3f_t               glUniform3f;
glUniform3fv_t              glUniform3fv;
glUniform4f_t               glUniform4f;
glUniform1i_t               glUniform1i;
glProgramUniform1i_t        glProgramUniform1i;
glUniformMatrix4fv_t        glUniformMatrix4fv;
glUniform4fv_t              glUniform4fv;
glUniform1iv_t              glUniform1iv;
glGetUniformBlockIndex_t    glGetUniformBlockIndex;
glUniformBlockBinding_t     glUniformBlockBinding;
glGetActiveUniformBlockiv_t glGetActiveUniformBlockiv;

// Texture functions 
glActiveTexture_t        glActiveTexture;
static glTexStorage2D_t         glTexStorage2D;
static glTexSubImage3D_t        glTexSubImage3D;
static glTexStorage3D_t         glTexStorage3D;
static glGenerateMipmap_t       glGenerateMipmap;
glBindTextures_t                glBindTextures;

glObjectLabel_t glObjectLabel;
glGetObjectLabel_t glGetObjectLabel;

static qbool shaders_supported = false;
static int modern_only = -1;

qbool GL_ShadersSupported(void)
{
	if (modern_only < 0) {
		modern_only = COM_CheckParm("-modern");
	}

	return modern_only || shaders_supported;
}

#define OPENGL_LOAD_SHADER_FUNCTION(x) \
	if (shaders_supported) { \
		x = (x ## _t)SDL_GL_GetProcAddress(#x); \
		shaders_supported = (x != NULL); \
	}

/************************************* EXTENSIONS *************************************/

static void CheckMultiTextureExtensions(void)
{
	if (!COM_CheckParm("-nomtex") && SDL_GL_ExtensionSupported("GL_ARB_multitexture")) {
		if (strstr(gl_renderer, "Savage")) {
			return;
		}
		qglMultiTexCoord2f = SDL_GL_GetProcAddress("glMultiTexCoord2fARB");
		qglActiveTexture = SDL_GL_GetProcAddress("glActiveTextureARB");
		qglClientActiveTexture = SDL_GL_GetProcAddress("glClientActiveTexture");
		if (!qglMultiTexCoord2f || !qglActiveTexture || !qglClientActiveTexture) {
			return;
		}
		Com_Printf_State(PRINT_OK, "Multitexture extensions found\n");
		gl_mtexable = true;
	}

	gl_textureunits = min(glConfig.texture_units, 4);

	if (COM_CheckParm("-maxtmu2") /*|| !strcmp(gl_vendor, "ATI Technologies Inc.")*/ || gl_maxtmu2.value) {
		gl_textureunits = min(gl_textureunits, 2);
	}

	if (gl_textureunits < 2) {
		gl_mtexable = false;
	}

	if (!gl_mtexable) {
		gl_textureunits = 1;
	}
	else {
		Com_Printf_State(PRINT_OK, "Enabled %i texture units on hardware\n", gl_textureunits);
	}
}

static void CheckShaderExtensions(void)
{
	shaders_supported = false;

	GL_InitialiseBufferHandling();
	GL_InitialiseFramebufferHandling();

	if (COM_CheckParm("-modern")) {
		if (glConfig.majorVersion >= 2) {
			shaders_supported = true;
			OPENGL_LOAD_SHADER_FUNCTION(glCreateShader);
			OPENGL_LOAD_SHADER_FUNCTION(glShaderSource);
			OPENGL_LOAD_SHADER_FUNCTION(glCompileShader);
			OPENGL_LOAD_SHADER_FUNCTION(glDeleteShader);
			OPENGL_LOAD_SHADER_FUNCTION(glGetShaderInfoLog);
			OPENGL_LOAD_SHADER_FUNCTION(glGetShaderiv);

			OPENGL_LOAD_SHADER_FUNCTION(glCreateProgram);
			OPENGL_LOAD_SHADER_FUNCTION(glLinkProgram);
			OPENGL_LOAD_SHADER_FUNCTION(glDeleteProgram);
			OPENGL_LOAD_SHADER_FUNCTION(glUseProgram);
			OPENGL_LOAD_SHADER_FUNCTION(glAttachShader);
			OPENGL_LOAD_SHADER_FUNCTION(glDetachShader);
			OPENGL_LOAD_SHADER_FUNCTION(glGetProgramInfoLog);
			OPENGL_LOAD_SHADER_FUNCTION(glGetProgramiv);

			OPENGL_LOAD_SHADER_FUNCTION(glGetUniformLocation);
			OPENGL_LOAD_SHADER_FUNCTION(glUniform1f);
			OPENGL_LOAD_SHADER_FUNCTION(glUniform1fv);
			OPENGL_LOAD_SHADER_FUNCTION(glUniform2f);
			OPENGL_LOAD_SHADER_FUNCTION(glUniform3f);
			OPENGL_LOAD_SHADER_FUNCTION(glUniform3fv);
			OPENGL_LOAD_SHADER_FUNCTION(glUniform4f);
			OPENGL_LOAD_SHADER_FUNCTION(glUniform1i);
			OPENGL_LOAD_SHADER_FUNCTION(glUniform4fv);
			OPENGL_LOAD_SHADER_FUNCTION(glUniform1iv);
			OPENGL_LOAD_SHADER_FUNCTION(glProgramUniform1i);
			OPENGL_LOAD_SHADER_FUNCTION(glUniformMatrix4fv);
			OPENGL_LOAD_SHADER_FUNCTION(glGetUniformBlockIndex);
			OPENGL_LOAD_SHADER_FUNCTION(glUniformBlockBinding);
			OPENGL_LOAD_SHADER_FUNCTION(glGetUniformBlockIndex);
			OPENGL_LOAD_SHADER_FUNCTION(glUniformBlockBinding);
			OPENGL_LOAD_SHADER_FUNCTION(glGetActiveUniformBlockiv);

			OPENGL_LOAD_SHADER_FUNCTION(glActiveTexture);
			OPENGL_LOAD_SHADER_FUNCTION(glTexSubImage3D);
			OPENGL_LOAD_SHADER_FUNCTION(glTexStorage2D);
			OPENGL_LOAD_SHADER_FUNCTION(glTexStorage3D);
			OPENGL_LOAD_SHADER_FUNCTION(glGenerateMipmap);

			OPENGL_LOAD_SHADER_FUNCTION(glMultiDrawElementsIndirect);
			OPENGL_LOAD_SHADER_FUNCTION(glDrawArraysInstancedBaseInstance);
			OPENGL_LOAD_SHADER_FUNCTION(glDrawElementsInstancedBaseInstance);
			OPENGL_LOAD_SHADER_FUNCTION(glDrawElementsInstancedBaseVertexBaseInstance);

			OPENGL_LOAD_DSA_FUNCTION(glGetTextureLevelParameterfv);
			OPENGL_LOAD_DSA_FUNCTION(glGetTextureLevelParameterfv);
			OPENGL_LOAD_DSA_FUNCTION(glGetTextureLevelParameteriv);
			OPENGL_LOAD_DSA_FUNCTION(glGenerateTextureMipmap);
			OPENGL_LOAD_DSA_FUNCTION(glGetTextureImage);
			OPENGL_LOAD_DSA_FUNCTION(glCreateTextures);
			OPENGL_LOAD_DSA_FUNCTION(glGetnTexImage);
			OPENGL_LOAD_DSA_FUNCTION(glTextureParameteri);
			OPENGL_LOAD_DSA_FUNCTION(glTextureParameterf);
			OPENGL_LOAD_DSA_FUNCTION(glTextureParameterfv);
			OPENGL_LOAD_DSA_FUNCTION(glTextureParameteriv);
			OPENGL_LOAD_DSA_FUNCTION(glTextureStorage3D);
			OPENGL_LOAD_DSA_FUNCTION(glTextureStorage2D);
			OPENGL_LOAD_DSA_FUNCTION(glTextureSubImage2D);
			OPENGL_LOAD_DSA_FUNCTION(glTextureSubImage3D);
		}
	}

#ifdef _WIN32
	// During init, enable debug output
	if (GL_ShadersSupported() && IsDebuggerPresent()) {
		glDebugMessageCallback_t glDebugMessageCallback = (glDebugMessageCallback_t)SDL_GL_GetProcAddress("glDebugMessageCallback");

		if (glDebugMessageCallback) {
			glEnable(GL_DEBUG_OUTPUT);
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			glDebugMessageCallback((GLDEBUGPROC)MessageCallback, 0);
		}
	}
#endif
	glPrimitiveRestartIndex = (glPrimitiveRestartIndex_t)SDL_GL_GetProcAddress("glPrimitiveRestartIndex");
	glObjectLabel = (glObjectLabel_t)SDL_GL_GetProcAddress("glObjectLabel");
	glGetObjectLabel = (glGetObjectLabel_t)SDL_GL_GetProcAddress("glGetObjectLabel");
	glMultiDrawArrays = (glMultiDrawArrays_t)SDL_GL_GetProcAddress("glMultiDrawArrays");
	glMultiDrawElements = (glMultiDrawElements_t)SDL_GL_GetProcAddress("glMultiDrawElements");
	glDrawElementsBaseVertex = (glDrawElementsBaseVertex_t)SDL_GL_GetProcAddress("glDrawElementsBaseVertex");

	if (glPrimitiveRestartIndex) {
		glEnable(GL_PRIMITIVE_RESTART);
		if (glConfig.majorVersion > 4 || (glConfig.majorVersion == 4 && glConfig.minorVersion >= 3)) {
			glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
		}
		else {
			glPrimitiveRestartIndex(~(GLuint)0);
		}
	}

	// 4.4 - binds textures to consecutive texture units
	glBindTextures = SDL_GL_GetProcAddress("glBindTextures");
}

void GL_CheckExtensions (void)
{
	CheckMultiTextureExtensions ();
	CheckShaderExtensions();

	gl_combine = SDL_GL_ExtensionSupported("GL_ARB_texture_env_combine");
	gl_add_ext = SDL_GL_ExtensionSupported("GL_ARB_texture_env_add");

	if (SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic")) {
		int gl_anisotropy_factor_max;

		anisotropy_ext = 1;

		glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_anisotropy_factor_max);

		Com_Printf_State(PRINT_OK, "Anisotropic Filtering Extension Found (%d max)\n",gl_anisotropy_factor_max);
	}

	if (SDL_GL_ExtensionSupported("GL_ARB_texture_compression")) {
		Com_Printf_State(PRINT_OK, "Texture compression extensions found\n");
		Cvar_SetCurrentGroup(CVAR_GROUP_TEXTURES);
		Cvar_Register (&gl_ext_texture_compression);
		Cvar_ResetCurrentGroup();
	}

	// GL_ARB_texture_non_power_of_two
	// NOTE: we always register cvar even if ext is not supported.
	// cvar added just to be able force OFF an extension.
	Cvar_SetCurrentGroup(CVAR_GROUP_TEXTURES);
	Cvar_Register (&gl_ext_arb_texture_non_power_of_two);
	Cvar_ResetCurrentGroup();

	gl_support_arb_texture_non_power_of_two =
		gl_ext_arb_texture_non_power_of_two.integer && SDL_GL_ExtensionSupported("GL_ARB_texture_non_power_of_two");
	Com_Printf_State(PRINT_OK, "GL_ARB_texture_non_power_of_two extension %s\n", 
		gl_support_arb_texture_non_power_of_two ? "found" : "not found");
}

void OnChange_gl_ext_texture_compression(cvar_t *var, char *string, qbool *cancel) {
	float newval = Q_atof(string);

	gl_alpha_format = newval ? GL_COMPRESSED_RGBA_ARB : GL_RGBA8;
	gl_solid_format = newval ? GL_COMPRESSED_RGB_ARB : GL_RGB8;
}

/************************************** GL INIT **************************************/

void GL_Init(void)
{
	gl_vendor = (const char*)glGetString(GL_VENDOR);
	gl_renderer = (const char*)glGetString(GL_RENDERER);
	gl_version = (const char*)glGetString(GL_VERSION);
	if (GL_ShadersSupported()) {
		gl_extensions = "(using modern OpenGL)\n";
	}
	else {
		gl_extensions = (const char*)glGetString(GL_EXTENSIONS);
	}

#if !defined( _WIN32 ) && !defined( __linux__ ) /* we print this in different place on WIN and Linux */
	/* FIXME/TODO: FreeBSD too? */
	Com_Printf_State(PRINT_INFO, "GL_VENDOR: %s\n", gl_vendor);
	Com_Printf_State(PRINT_INFO, "GL_RENDERER: %s\n", gl_renderer);
	Com_Printf_State(PRINT_INFO, "GL_VERSION: %s\n", gl_version);
#endif

	if (COM_CheckParm("-gl_ext")) {
		Com_Printf_State(PRINT_INFO, "GL_EXTENSIONS: %s\n", gl_extensions);
	}

	Cvar_Register(&gl_strings);
	Cvar_ForceSet(&gl_strings, va("GL_VENDOR: %s\nGL_RENDERER: %s\n"
		"GL_VERSION: %s\nGL_EXTENSIONS: %s", gl_vendor, gl_renderer, gl_version, gl_extensions));
	Cvar_Register(&gl_maxtmu2);

	GL_StateDefaultInit();

	GL_CheckExtensions();
}

/************************************* VID GAMMA *************************************/

void Check_Gamma (unsigned char *pal) {
	float inf;
	unsigned char palette[768];
	int i;

	// we do not need this after host initialized
	if (!host_initialized) {
		float old = v_gamma.value;
		char string = v_gamma.string[0];
		if ((i = COM_CheckParm("-gamma")) != 0 && i + 1 < COM_Argc()) {
			vid_gamma = bound(0.3, Q_atof(COM_Argv(i + 1)), 1);
		}
		else {
			vid_gamma = 1;
		}

		Cvar_SetDefault (&v_gamma, vid_gamma);
		// Cvar_SetDefault set not only default value, but also reset to default, fix that
		Cvar_SetValue(&v_gamma, old || string == '0' ? old : vid_gamma);
	}

	if (vid_gamma != 1) {
		for (i = 0; i < 256; i++) {
			inf = 255 * pow((i + 0.5) / 255.5, vid_gamma) + 0.5;
			if (inf > 255) {
				inf = 255;
			}
			vid_gamma_table[i] = inf;
		}
	}
	else {
		for (i = 0; i < 256; i++) {
			vid_gamma_table[i] = i;
		}
	}

	for (i = 0; i < 768; i++) {
		palette[i] = vid_gamma_table[pal[i]];
	}

	memcpy (pal, palette, sizeof(palette));
}

/************************************* HW GAMMA *************************************/

void VID_SetPalette (unsigned char *palette) {
	int i;
	byte *pal;
	unsigned r,g,b, v, *table;

	// 8 8 8 encoding
	// Macintosh has different byte order
	pal = palette;
	table = d_8to24table;
	for (i = 0; i < 256; i++) {
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;
		v = LittleLong ((255 << 24) + (r << 0) + (g << 8) + (b << 16));
		*table++ = v;
	}
	d_8to24table[255] = 0;		// 255 is transparent

	// Tonik: create a brighter palette for bmodel textures
	pal = palette;
	table = d_8to24table2;

	for (i = 0; i < 256; i++) {
		r = pal[0] * (2.0 / 1.5); if (r > 255) r = 255;
		g = pal[1] * (2.0 / 1.5); if (g > 255) g = 255;
		b = pal[2] * (2.0 / 1.5); if (b > 255) b = 255;
		pal += 3;
		*table++ = LittleLong ((255 << 24) + (r << 0) + (g << 8) + (b << 16));
	}
	d_8to24table2[255] = 0;	// 255 is transparent
}

#undef glColor3f
#undef glColor4f
#undef glColor3fv
#undef glColor3ubv
#undef glColor4ubv
#undef glColor4ub

#undef glDisable
#undef glEnable

void GL_Color3f(float r, float g, float b)
{
	if (GL_ShadersSupported()) {
		// TODO
	}
	else {
		glColor3f(r, g, b);
	}
}

void GL_Color4f(float r, float g, float b, float a)
{
	if (GL_ShadersSupported()) {
		// TODO
	}
	else {
		glColor4f(r, g, b, a);
	}
}

void GL_Color3fv(const float* rgbVec)
{
	if (GL_ShadersSupported()) {
		// TODO
	}
	else {
		glColor3fv(rgbVec);
	}
}

void GL_Color4fv(const float* rgbaVec)
{
	if (GL_ShadersSupported()) {
		// TODO
	}
	else {
		glColor4fv(rgbaVec);
	}
}

void GL_Color3ubv(const GLubyte* rgbVec)
{
	if (GL_ShadersSupported()) {
		// TODO
	}
	else {
		glColor3ubv(rgbVec);
	}
}

void GL_Color4ubv(const GLubyte* rgbaVec)
{
	if (GL_ShadersSupported()) {
		// TODO
	}
	else {
		glColor4ubv(rgbaVec);
	}

}

void GL_Color4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
	if (GL_ShadersSupported()) {
		// TODO
	}
	else {
		glColor4ub(r, g, b, a);
	}
}

#ifdef WITH_NVTX
static int debug_frame_depth = 0;
static unsigned long regions_trace_only;
FILE* debug_frame_out;

void GL_EnterTracedRegion(const char* regionName, qbool trace_only)
{
	if (GL_ShadersSupported()) {
		if (!trace_only) {
			nvtxRangePushA(regionName);
		}
	}
	else if (debug_frame_out) {
		fprintf(debug_frame_out, "Enter: %.*s %s {\n", debug_frame_depth, "                                                          ", regionName);
		debug_frame_depth += DEBUG_FRAME_DEPTH_CHARS;
	}

	regions_trace_only <<= 1;
	regions_trace_only &= (trace_only ? 1 : 0);
}

void GL_LeaveTracedRegion(qbool trace_only)
{
	if (GL_ShadersSupported()) {
		if (!trace_only) {
			nvtxRangePop();
		}
	}
	else if (debug_frame_out) {
		debug_frame_depth -= DEBUG_FRAME_DEPTH_CHARS;
		debug_frame_depth = max(debug_frame_depth, 0);
		fprintf(debug_frame_out, "Leave: %.*s }\n", debug_frame_depth, "                                                          ");
	}
}

void GL_MarkEvent(const char* format, ...)
{
	va_list argptr;
	char msg[4096];

	va_start(argptr, format);
	vsnprintf(msg, sizeof(msg), format, argptr);
	va_end(argptr);

	if (GL_ShadersSupported()) {
		//nvtxMark(va(msg));
	}
	else if (debug_frame_out) {
		fprintf(debug_frame_out, "Event: %.*s %s\n", debug_frame_depth, "                                                          ", msg);
	}
}

qbool GL_LoggingEnabled(void)
{
	return debug_frame_out != NULL;
}

void GL_LogAPICall(const char* format, ...)
{
	if (!GL_ShadersSupported() && debug_frame_out) {
		va_list argptr;
		char msg[4096];

		va_start(argptr, format);
		vsnprintf(msg, sizeof(msg), format, argptr);
		va_end(argptr);

		fprintf(debug_frame_out, "API:   %.*s %s\n", debug_frame_depth, "                                                          ", msg);
	}
}

void GL_ResetRegion(qbool start)
{
	if (start && debug_frame_out) {
		fclose(debug_frame_out);
		debug_frame_out = NULL;
	}
	else if (start && dev_frame_debug_queued) {
		char fileName[MAX_PATH];
#ifndef _WIN32
		time_t t;
		struct tm date;
		t = time(NULL);
		localtime_r(&t, &date);

		snprintf(fileName, sizeof(fileName), "%s/qw/frame_%04d-%02d-%02d_%02d-%02d-%02d.txt",
			com_basedir, date.tm_year, date.tm_mon, date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec);
#else
		SYSTEMTIME date;
		GetLocalTime(&date);

		snprintf(fileName, sizeof(fileName), "%s/qw/frame_%04d-%02d-%02d_%02d-%02d-%02d.txt",
			com_basedir, date.wYear, date.wMonth, date.wDay, date.wHour, date.wMinute, date.wSecond);
#endif

		debug_frame_out = fopen(fileName, "wt");
		dev_frame_debug_queued = false;
	}

	if (!GL_ShadersSupported() && debug_frame_out) {
		fprintf(debug_frame_out, "---Reset---\n");
		debug_frame_depth = 0;
	}
}

#endif

void GL_AlphaFunc(GLenum func, GLclampf threshold)
{
	if (!GL_ShadersSupported()) {
		glAlphaFunc(func, threshold);
	}
}

void GL_TexSubImage3D(
	GLenum textureUnit, texture_ref texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
	GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid * pixels
)
{
	if (glTextureSubImage3D) {
		glTextureSubImage3D(GL_TextureNameFromReference(texture), level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
	}
	GL_LogAPICall("GL_TexSubImage3D(unit=GL_TEXTURE%d, texture=%u)", textureUnit - GL_TEXTURE0, GL_TextureNameFromReference(texture));
}

/*
void GL_TexImage2D(
	GLenum textureUnit, GLenum target, texture_ref texture, GLint level, GLint internalformat,
	GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels
)
{
	GL_BindTextureUnit(textureUnit, texture);
	glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}
*/

void GL_TexSubImage2D(
	GLenum textureUnit, texture_ref texture, GLint level,
	GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels
)
{
	if (glTextureSubImage2D) {
		GLint textureWidth, textureHeight;

		GL_GetTexLevelParameteriv(GL_TEXTURE0, texture, 0, GL_TEXTURE_WIDTH, &textureWidth);
		GL_GetTexLevelParameteriv(GL_TEXTURE0, texture, 0, GL_TEXTURE_HEIGHT, &textureHeight);

		glTextureSubImage2D(GL_TextureNameFromReference(texture), level, xoffset, yoffset, width, height, format, type, pixels);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
	}
	GL_LogAPICall("GL_TexSubImage2D(unit=GL_TEXTURE%d, texture=%u)", textureUnit - GL_TEXTURE0, GL_TextureNameFromReference(texture));
}

void GL_TexStorage2DImpl(GLenum textureUnit, GLenum target, GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
	if (glTextureStorage2D) {
		glTextureStorage2D(texture, levels, internalformat, width, height);
	}
	else {
		glBindTexture(textureUnit, target);
		if (glTexStorage2D) {
			glTexStorage2D(target, levels, internalformat, width, height);
		}
		else {
			int level;
			for (level = 0; level < levels; ++level) {
				glTexImage2D(target, level, internalformat, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
				width = max(1, width / 2);
				height = max(1, height / 2);
			}
		}
		GL_BindTextureUnit(textureUnit, null_texture_reference);
	}
}

void GL_TexStorage2D(
	GLenum textureUnit, texture_ref texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height
)
{
	if (glTextureStorage2D) {
		glTextureStorage2D(GL_TextureNameFromReference(texture), levels, internalformat, width, height);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		if (glTexStorage2D) {
			glTexStorage2D(target, levels, internalformat, width, height);
		}
		else {
			int level;
			for (level = 0; level < levels; ++level) {
				glTexImage2D(target, level, internalformat, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
				width = max(1, width / 2);
				height = max(1, height / 2);
			}
		}
	}
}

void GL_TexStorage3D(
	GLenum textureUnit, texture_ref texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth
)
{
	if (glTextureStorage3D) {
		glTextureStorage3D(GL_TextureNameFromReference(texture), levels, internalformat, width, height, depth);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		glTexStorage3D(target, levels, internalformat, width, height, depth);
	}
}

void GL_TexParameterf(GLenum textureUnit, texture_ref texture, GLenum pname, GLfloat param)
{
	if (glTextureParameterf) {
		glTextureParameterf(GL_TextureNameFromReference(texture), pname, param);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		glTexParameterf(target, pname, param);
	}
}

void GL_TexParameterfv(GLenum textureUnit, texture_ref texture, GLenum pname, const GLfloat *params)
{
	if (glTextureParameterfv) {
		glTextureParameterfv(GL_TextureNameFromReference(texture), pname, params);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		glTexParameterfv(target, pname, params);
	}
}

void GL_TexParameteri(GLenum textureUnit, texture_ref texture, GLenum pname, GLint param)
{
	if (glTextureParameteri) {
		glTextureParameteri(GL_TextureNameFromReference(texture), pname, param);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		glTexParameteri(target, pname, param);
	}
}

void GL_TexParameteriv(GLenum textureUnit, texture_ref texture, GLenum pname, const GLint *params)
{
	if (glTextureParameteriv) {
		glTextureParameteriv(GL_TextureNameFromReference(texture), pname, params);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		glTexParameteriv(target, pname, params);
	}
}

void GL_CreateTextureNames(GLenum textureUnit, GLenum target, GLsizei n, GLuint* textures)
{
	if (glCreateTextures) {
		glCreateTextures(target, n, textures);
	}
	else {
		int i;

		glGenTextures(n, textures);
		for (i = 0; i < n; ++i) {
			GL_SelectTexture(textureUnit);
			glBindTexture(target, textures[i]);
		}
	}
}

void GL_GetTexLevelParameteriv(GLenum textureUnit, texture_ref texture, GLint level, GLenum pname, GLint* params)
{
	if (glGetTextureLevelParameteriv) {
		glGetTextureLevelParameteriv(GL_TextureNameFromReference(texture), level, pname, params);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		glGetTexLevelParameteriv(target, level, pname, params);
	}
}

void GL_GetTexImage(GLenum textureUnit, texture_ref texture, GLint level, GLenum format, GLenum type, GLsizei bufSize, void* buffer)
{
	// TODO: Use glGetnTexImage() if available (4.5)
	if (glGetTextureImage) {
		glGetTextureImage(GL_TextureNameFromReference(texture), level, format, type, bufSize, buffer);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		if (glGetnTexImage) {
			glGetnTexImage(target, level, format, type, bufSize, buffer);
		}
		else {
			glGetTexImage(target, level, format, type, buffer);
		}
	}
}

void GL_GenerateMipmapWithData(GLenum textureUnit, texture_ref texture, byte* newdata, int width, int height, GLint internal_format)
{
	if (glGenerateTextureMipmap) {
		glGenerateTextureMipmap(GL_TextureNameFromReference(texture));
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		if (glGenerateMipmap) {
			glGenerateMipmap(target);
		}
		else if (newdata) {
			int miplevel = 0;

			// Calculate the mip maps for the images.
			while (width > 1 || height > 1) {
				Image_MipReduce((byte *)newdata, (byte *)newdata, &width, &height, 4);
				miplevel++;
				GL_TexSubImage2D(GL_TEXTURE0, texture, miplevel, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, newdata);
			}
		}
	}
}

void GL_GenerateMipmap(GLenum textureUnit, texture_ref texture)
{
	GL_GenerateMipmapWithData(textureUnit, texture, NULL, 0, 0, 0);
}

void Dev_VidFrameTrace(void)
{
	dev_frame_debug_queued = true;
}

// Wrappers around drawing functions
void GL_MultiDrawArrays(GLenum mode, GLint* first, GLsizei* count, GLsizei primcount)
{
	if (glMultiDrawArrays) {
		glMultiDrawArrays(mode, first, count, primcount);
		++frameStats.draw_calls;
		frameStats.subdraw_calls += primcount;
		GL_LogAPICall("glMultiDrawElements(%d verts)", count);
	}
	else {
		int i;
		for (i = 0; i < primcount; ++i) {
			GL_DrawArrays(mode, first[i], count[i]);
		}
	}
}

void GL_DrawArrays(GLenum mode, GLint first, GLsizei count)
{
	glDrawArrays(mode, first, count);
	++frameStats.draw_calls;
	GL_LogAPICall("glDrawArrays(%d verts)", count);
}

void GL_DrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, GLvoid* indices, GLint basevertex)
{
	if (!glDrawElementsBaseVertex) {
		Sys_Error("glDrawElementsBaseVertex called, not supported");
	}
	glDrawElementsBaseVertex(mode, count, type, indices, basevertex);
	++frameStats.draw_calls;
	GL_LogAPICall("glDrawElements(%d verts)", count);
}

qbool GL_DrawElementsBaseVertexAvailable(void)
{
	return glDrawElementsBaseVertex != NULL;
}

void GL_DrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices)
{
	glDrawElements(mode, count, type, indices);
	++frameStats.draw_calls;
	GL_LogAPICall("glDrawElements(%d verts)", count);
}

void GL_MultiDrawArraysIndirect(GLenum mode, const void* indirect, GLsizei drawcount, GLsizei stride)
{
	glMultiDrawArraysIndirect(mode, indirect, drawcount, stride);
	++frameStats.draw_calls;
	frameStats.subdraw_calls += drawcount;
	GL_LogAPICall("glMultiDrawArraysIndirect(%d subdraws)", drawcount);
}

void GL_MultiDrawElementsIndirect(GLenum mode, GLenum type, const void* indirect, GLsizei drawcount, GLsizei stride)
{
	glMultiDrawElementsIndirect(mode, type, indirect, drawcount, stride);
	++frameStats.draw_calls;
	frameStats.subdraw_calls += drawcount;
	GL_LogAPICall("glMultiDrawElementsIndirect(%d subdraws)", drawcount);
}
