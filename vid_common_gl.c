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

#ifdef WITH_NVTX
#include "nvToolsExt.h"
#endif

void GL_AlphaFunc(GLenum func, GLclampf threshold);

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

int anisotropy_ext = 0;

qbool gl_mtexable = false;
int gl_textureunits = 1;
lpMTexFUNC qglMultiTexCoord2f = NULL;
lpSelTexFUNC qglActiveTexture = NULL;

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

// VBO functions
glBindBuffer_t     glBindBuffer = NULL;
glBufferData_t     glBufferData = NULL;
glBufferSubData_t  glBufferSubData = NULL;
glGenBuffers_t     glGenBuffers = NULL;
glDeleteBuffers_t  glDeleteBuffers = NULL;
glBindBufferBase_t glBindBufferBase = NULL;

// VAO functions
glGenVertexArrays_t         glGenVertexArrays = NULL;
glBindVertexArray_t         glBindVertexArray = NULL;
glEnableVertexAttribArray_t glEnableVertexAttribArray = NULL;
glDeleteVertexArrays_t      glDeleteVertexArrays = NULL;
glVertexAttribPointer_t     glVertexAttribPointer = NULL;
glVertexAttribIPointer_t    glVertexAttribIPointer = NULL;
glVertexAttribDivisor_t     glVertexAttribDivisor = NULL;

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
glTexStorage2D_t         glTexStorage2D;
glTexSubImage3D_t        glTexSubImage3D;
glTexStorage3D_t         glTexStorage3D;
glGenerateMipmap_t       glGenerateMipmap;

// Draw functions
glMultiDrawArrays_t      glMultiDrawArrays;
glMultiDrawElements_t    glMultiDrawElements;
glDrawArraysInstanced_t  glDrawArraysInstanced;
glMultiDrawArraysIndirect_t glMultiDrawArraysIndirect;
glMultiDrawElementsIndirect_t glMultiDrawElementsIndirect;
glDrawArraysInstancedBaseInstance_t glDrawArraysInstancedBaseInstance;
glDrawElementsInstancedBaseInstance_t glDrawElementsInstancedBaseInstance;
glDrawElementsInstancedBaseVertexBaseInstance_t glDrawElementsInstancedBaseVertexBaseInstance;
glPrimitiveRestartIndex_t glPrimitiveRestartIndex;

static qbool vbo_supported = false;
static qbool shaders_supported = false;
static int modern_only = -1;

qbool GL_ShadersSupported(void)
{
	if (modern_only < 0) {
		modern_only = COM_CheckParm("-modern");
	}

	return modern_only || shaders_supported;
}

qbool GL_VBOsSupported(void)
{
	return vbo_supported;
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
		if (!qglMultiTexCoord2f || !qglActiveTexture) {
			return;
		}
		Com_Printf_State(PRINT_OK, "Multitexture extensions found\n");
		gl_mtexable = true;
	}

	glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, (GLint *)&gl_textureunits);
	gl_textureunits = min(gl_textureunits, 4);

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
	shaders_supported = vbo_supported = false;
	glBindBuffer = NULL;
	glBufferData = NULL;
	glBufferSubData = NULL;

	if (COM_CheckParm("-modern")) {
		if (glConfig.majorVersion >= 2) {
			glBindBuffer = (glBindBuffer_t)SDL_GL_GetProcAddress("glBindBuffer");
			glBufferData = (glBufferData_t)SDL_GL_GetProcAddress("glBufferData");
			glBufferSubData = (glBufferSubData_t)SDL_GL_GetProcAddress("glBufferSubData");

			shaders_supported = true;
			OPENGL_LOAD_SHADER_FUNCTION(glCreateShader);
			OPENGL_LOAD_SHADER_FUNCTION(glShaderSource);
			OPENGL_LOAD_SHADER_FUNCTION(glCompileShader);
			OPENGL_LOAD_SHADER_FUNCTION(glDeleteShader);
			OPENGL_LOAD_SHADER_FUNCTION(glGetShaderInfoLog);
			OPENGL_LOAD_SHADER_FUNCTION(glGetShaderiv);

			OPENGL_LOAD_SHADER_FUNCTION(glGenBuffers);
			OPENGL_LOAD_SHADER_FUNCTION(glDeleteBuffers);
			OPENGL_LOAD_SHADER_FUNCTION(glBindBufferBase);

			OPENGL_LOAD_SHADER_FUNCTION(glGenVertexArrays);
			OPENGL_LOAD_SHADER_FUNCTION(glBindVertexArray);
			OPENGL_LOAD_SHADER_FUNCTION(glDeleteVertexArrays);
			OPENGL_LOAD_SHADER_FUNCTION(glEnableVertexAttribArray);
			OPENGL_LOAD_SHADER_FUNCTION(glVertexAttribPointer);
			OPENGL_LOAD_SHADER_FUNCTION(glVertexAttribIPointer);
			OPENGL_LOAD_SHADER_FUNCTION(glVertexAttribDivisor);

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

			OPENGL_LOAD_SHADER_FUNCTION(glMultiDrawArrays);
			OPENGL_LOAD_SHADER_FUNCTION(glMultiDrawElements);
			OPENGL_LOAD_SHADER_FUNCTION(glDrawArraysInstanced);
			OPENGL_LOAD_SHADER_FUNCTION(glMultiDrawArraysIndirect);
			OPENGL_LOAD_SHADER_FUNCTION(glMultiDrawElementsIndirect);
			OPENGL_LOAD_SHADER_FUNCTION(glDrawArraysInstancedBaseInstance);
			OPENGL_LOAD_SHADER_FUNCTION(glDrawElementsInstancedBaseInstance);
			OPENGL_LOAD_SHADER_FUNCTION(glDrawElementsInstancedBaseVertexBaseInstance);
			OPENGL_LOAD_SHADER_FUNCTION(glPrimitiveRestartIndex);
		}
		else if (SDL_GL_ExtensionSupported("GL_ARB_vertex_buffer_object")) {
			glBindBuffer = (glBindBuffer_t)SDL_GL_GetProcAddress("glBindBufferARB");
			glBufferData = (glBufferData_t)SDL_GL_GetProcAddress("glBufferDataARB");
			glBufferSubData = (glBufferSubData_t)SDL_GL_GetProcAddress("glBufferSubDataARB");
		}

		vbo_supported = glBindBuffer && glBufferData && glBufferSubData;
	}

	if (glPrimitiveRestartIndex) {
		glEnable(GL_PRIMITIVE_RESTART);
		if (glConfig.majorVersion > 4 || (glConfig.majorVersion == 4 && glConfig.minorVersion >= 3)) {
			glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
		}
		else {
			glPrimitiveRestartIndex(~(GLuint)0);
		}
	}
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

	gl_alpha_format = newval ? GL_COMPRESSED_RGBA_ARB : GL_RGBA;
	gl_solid_format = newval ? GL_COMPRESSED_RGB_ARB : GL_RGB;
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

	if (COM_CheckParm("-gl_ext"))
		Com_Printf_State(PRINT_INFO, "GL_EXTENSIONS: %s\n", gl_extensions);

	Cvar_Register(&gl_strings);
	Cvar_ForceSet(&gl_strings, va("GL_VENDOR: %s\nGL_RENDERER: %s\n"
		"GL_VERSION: %s\nGL_EXTENSIONS: %s", gl_vendor, gl_renderer, gl_version, gl_extensions));
	Cvar_Register(&gl_maxtmu2);
#ifndef __APPLE__
	glClearColor(1, 0, 0, 0);
#else
	glClearColor(0.2, 0.2, 0.2, 1.0);
#endif

	GL_CullFace(GL_FRONT);
	if (!GL_ShadersSupported()) {
		glEnable(GL_TEXTURE_2D);
	}

	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED);
	GL_AlphaFunc(GL_GREATER, 0.666);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	GL_ShadeModel(GL_FLAT);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	GL_TextureEnvMode(GL_REPLACE);

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

void GL_Hint(GLenum target, GLenum mode)
{
	if (!GL_ShadersSupported()) {
		glHint(target, mode);
	}
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
void GL_EnterRegion(const char* regionName)
{
	nvtxRangePushA(regionName);
}

void GL_LeaveRegion(void)
{
	nvtxRangePop();
}
#endif

// Linked list of all vbo buffers
static glm_vbo_t* vbo_list = NULL;

// Linked list of all vao buffers
static glm_vao_t* vao_list = NULL;

// Linked list of all uniform buffers
static glm_ubo_t* ubo_list = NULL;

void GL_GenFixedBuffer(glm_vbo_t* vbo, GLenum target, const char* name, GLsizei size, void* data, GLenum usage)
{
	if (vbo->vbo) {
		glDeleteBuffers(1, &vbo->vbo);
	}
	else {
		vbo->next = vbo_list;
		vbo_list = vbo;
	}
	vbo->name = name;
	vbo->target = target;
	vbo->size = size;
	glGenBuffers(1, &vbo->vbo);

	GL_BindBuffer(target, vbo->vbo);
	glBufferData(target, size, data, usage);
}

void GL_UpdateUBO(glm_ubo_t* ubo, size_t size, void* data)
{
	assert(ubo);
	assert(ubo->ubo);
	assert(data);
	assert(size == ubo->size);

	GL_BindBuffer(GL_UNIFORM_BUFFER, ubo->ubo);
	GL_BufferDataUpdate(GL_UNIFORM_BUFFER, size, data);
}

void GL_UpdateVBO(glm_vbo_t* vbo, size_t size, void* data)
{
	assert(vbo);
	assert(vbo->vbo);
	assert(data);
	assert(size <= vbo->size);

	GL_BindBuffer(vbo->target, vbo->vbo);
	GL_BufferDataUpdate(vbo->target, size, data);
}

void GL_UpdateVBOSection(glm_vbo_t* vbo, GLintptr offset, GLsizeiptr size, const GLvoid* data)
{
	assert(vbo);
	assert(vbo->vbo);
	assert(data);
	assert(offset >= 0);
	assert(offset < vbo->size);
	assert(offset + size <= vbo->size);

	GL_BindBuffer(vbo->target, vbo->vbo);
	GL_BufferSubDataUpdate(vbo->target, offset, size, data);
}

void GL_GenUniformBuffer(glm_ubo_t* ubo, const char* name, void* data, int size)
{
	if (ubo->ubo) {
		glDeleteBuffers(1, &ubo->ubo);
	}
	else {
		ubo->next = ubo_list;
		ubo_list = ubo;
	}
	ubo->name = name;
	ubo->size = size;
	glGenBuffers(1, &ubo->ubo);

	GL_BindBuffer(GL_UNIFORM_BUFFER, ubo->ubo);
	GL_BufferData(GL_UNIFORM_BUFFER, size, data, GL_DYNAMIC_DRAW);
}

void GL_GenVertexArray(glm_vao_t* vao)
{
	if (vao->vao) {
		glDeleteVertexArrays(1, &vao->vao);
	}
	else {
		vao->next = vao_list;
		vao_list = vao;
	}
	glGenVertexArrays(1, &vao->vao);
}

void GL_DeleteBuffers(void)
{
	glm_vao_t* vao = vao_list;
	glm_vbo_t* vbo = vbo_list;
	glm_ubo_t* ubo = ubo_list;

	if (glBindVertexArray) {
		glBindVertexArray(0);
	}
	while (vao) {
		glm_vao_t* prev = vao;

		if (vao->vao) {
			if (glDeleteVertexArrays) {
				glDeleteVertexArrays(1, &vao->vao);
			}
			vao->vao = 0;
		}

		vao = vao->next;
		prev->next = NULL;
	}

	while (vbo) {
		glm_vbo_t* prev = vbo;

		if (vbo->vbo) {
			if (glDeleteBuffers) {
				glDeleteBuffers(1, &vbo->vbo);
			}
			vbo->vbo = 0;
		}

		vbo = vbo->next;
		prev->next = NULL;
	}

	while (ubo) {
		glm_ubo_t* prev = ubo;

		if (ubo->ubo) {
			if (glDeleteBuffers) {
				glDeleteBuffers(1, &ubo->ubo);
			}
			ubo->ubo = 0;
		}

		ubo = ubo->next;
		prev->next = NULL;
	}

	vbo_list = NULL;
	vao_list = NULL;
	ubo_list = NULL;
}

void GL_AlphaFunc(GLenum func, GLclampf threshold)
{
	if (!GL_ShadersSupported()) {
		glAlphaFunc(func, threshold);
	}
}
