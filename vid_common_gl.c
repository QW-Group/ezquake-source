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
glBindBuffer_t     glBindBufferExt = NULL;
glBufferData_t     glBufferDataExt = NULL;
glBufferSubData_t  glBufferSubDataExt = NULL;
glGenBuffers_t     glGenBuffers = NULL;

// VAO functions
glGenVertexArrays_t         glGenVertexArrays = NULL;
glBindVertexArray_t         glBindVertexArray = NULL;
glEnableVertexAttribArray_t glEnableVertexAttribArray = NULL;
glVertexAttribPointer_t     glVertexAttribPointer = NULL;

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
glGetUniformLocation_t   glGetUniformLocation = NULL;
glUniform1f_t            glUniform1f;
glUniform2f_t            glUniform2f;
glUniform3f_t            glUniform3f;
glUniform4f_t            glUniform4f;
glUniform1i_t            glUniform1i;
glUniformMatrix4fv_t     glUniformMatrix4fv;

// Texture functions 
glActiveTexture_t        glActiveTexture;

static qbool vbo_supported = false;
static qbool shaders_supported = false;
static unsigned int vbo_number = 1;
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

qbool CheckExtension (const char *extension) {
	const char *start;
	char *where, *terminator;

	if (!gl_extensions && !(gl_extensions = (const char*) glGetString (GL_EXTENSIONS)))
		return false;


	if (!extension || *extension == 0 || strchr (extension, ' '))
		return false;

	for (start = gl_extensions; (where = strstr(start, extension)); start = terminator) {
		terminator = where + strlen (extension);
		if ((where == start || *(where - 1) == ' ') && (*terminator == 0 || *terminator == ' '))
			return true;
	}
	return false;
}

static void CheckMultiTextureExtensions(void)
{
	if (!COM_CheckParm("-nomtex") && CheckExtension("GL_ARB_multitexture")) {
		if (strstr(gl_renderer, "Savage"))
			return;
		qglMultiTexCoord2f = SDL_GL_GetProcAddress("glMultiTexCoord2fARB");
		qglActiveTexture = SDL_GL_GetProcAddress("glActiveTextureARB");
		if (!qglMultiTexCoord2f || !qglActiveTexture)
			return;
		Com_Printf_State(PRINT_OK, "Multitexture extensions found\n");
		gl_mtexable = true;
	}

	glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, (GLint *)&gl_textureunits);
	gl_textureunits = min(gl_textureunits, 4);

	if (COM_CheckParm("-maxtmu2") /*|| !strcmp(gl_vendor, "ATI Technologies Inc.")*/ || gl_maxtmu2.value)
		gl_textureunits = min(gl_textureunits, 2);

	if (gl_textureunits < 2)
		gl_mtexable = false;

	if (!gl_mtexable) {
		gl_textureunits = 1;
	}
	else {
		Com_Printf_State(PRINT_OK, "Enabled %i texture units on hardware\n", gl_textureunits);
	}
}

static void CheckShaderExtensions(void)
{
	int gl_version;

	shaders_supported = vbo_supported = false;
	glBindBufferExt = NULL;
	glBufferDataExt = NULL;
	glBufferSubDataExt = NULL;

	if (COM_CheckParm("-modern") && SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &gl_version) == 0) {
		if (gl_version >= 2) {
			glBindBufferExt = (glBindBuffer_t)SDL_GL_GetProcAddress("glBindBuffer");
			glBufferDataExt = (glBufferData_t)SDL_GL_GetProcAddress("glBufferData");
			glBufferSubDataExt = (glBufferSubData_t)SDL_GL_GetProcAddress("glBufferSubData");

			shaders_supported = true;
			OPENGL_LOAD_SHADER_FUNCTION(glCreateShader);
			OPENGL_LOAD_SHADER_FUNCTION(glShaderSource);
			OPENGL_LOAD_SHADER_FUNCTION(glCompileShader);
			OPENGL_LOAD_SHADER_FUNCTION(glDeleteShader);
			OPENGL_LOAD_SHADER_FUNCTION(glGetShaderInfoLog);
			OPENGL_LOAD_SHADER_FUNCTION(glGetShaderiv);

			OPENGL_LOAD_SHADER_FUNCTION(glGenBuffers);

			OPENGL_LOAD_SHADER_FUNCTION(glGenVertexArrays);
			OPENGL_LOAD_SHADER_FUNCTION(glBindVertexArray);
			OPENGL_LOAD_SHADER_FUNCTION(glEnableVertexAttribArray);
			OPENGL_LOAD_SHADER_FUNCTION(glVertexAttribPointer);

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
			OPENGL_LOAD_SHADER_FUNCTION(glUniform2f);
			OPENGL_LOAD_SHADER_FUNCTION(glUniform3f);
			OPENGL_LOAD_SHADER_FUNCTION(glUniform4f);
			OPENGL_LOAD_SHADER_FUNCTION(glUniform1i);
			OPENGL_LOAD_SHADER_FUNCTION(glUniformMatrix4fv);

			OPENGL_LOAD_SHADER_FUNCTION(glActiveTexture);
		}
		else if (SDL_GL_ExtensionSupported("GL_ARB_vertex_buffer_object")) {
			glBindBufferExt = (glBindBuffer_t)SDL_GL_GetProcAddress("glBindBufferARB");
			glBufferDataExt = (glBufferData_t)SDL_GL_GetProcAddress("glBufferDataARB");
			glBufferSubDataExt = (glBufferSubData_t)SDL_GL_GetProcAddress("glBufferSubDataARB");
		}

		vbo_supported = glBindBufferExt && glBufferDataExt && glBufferSubDataExt;
	}
	vbo_number = 1;
}

void GL_CheckExtensions (void) {
	CheckMultiTextureExtensions ();
	CheckShaderExtensions();

	gl_combine = CheckExtension("GL_ARB_texture_env_combine");
	gl_add_ext = CheckExtension("GL_ARB_texture_env_add");

	if (CheckExtension("GL_EXT_texture_filter_anisotropic")) {
		int gl_anisotropy_factor_max;

		anisotropy_ext = 1;

		glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_anisotropy_factor_max);

		Com_Printf_State(PRINT_OK, "Anisotropic Filtering Extension Found (%d max)\n",gl_anisotropy_factor_max);
	}


	if (CheckExtension("GL_ARB_texture_compression")) {
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
		gl_ext_arb_texture_non_power_of_two.integer && CheckExtension("GL_ARB_texture_non_power_of_two");
	Com_Printf_State(PRINT_OK, "GL_ARB_texture_non_power_of_two extension %s\n", 
		gl_support_arb_texture_non_power_of_two ? "found" : "not found");
}

void OnChange_gl_ext_texture_compression(cvar_t *var, char *string, qbool *cancel) {
	float newval = Q_atof(string);

	gl_alpha_format = newval ? GL_COMPRESSED_RGBA_ARB : GL_RGBA;
	gl_solid_format = newval ? GL_COMPRESSED_RGB_ARB : GL_RGB;
}

/************************************** GL INIT **************************************/

void GL_Init (void) {
	gl_vendor     = (const char*) glGetString (GL_VENDOR);
	gl_renderer   = (const char*) glGetString (GL_RENDERER);
	gl_version    = (const char*) glGetString (GL_VERSION);
	gl_extensions = (const char*) glGetString (GL_EXTENSIONS);

#if !defined( _WIN32 ) && !defined( __linux__ ) /* we print this in different place on WIN and Linux */
/* FIXME/TODO: FreeBSD too? */
	Com_Printf_State(PRINT_INFO, "GL_VENDOR: %s\n",   gl_vendor);
	Com_Printf_State(PRINT_INFO, "GL_RENDERER: %s\n", gl_renderer);
	Com_Printf_State(PRINT_INFO, "GL_VERSION: %s\n",  gl_version);
#endif

	if (COM_CheckParm("-gl_ext"))
		Com_Printf_State(PRINT_INFO, "GL_EXTENSIONS: %s\n", gl_extensions);

	Cvar_Register (&gl_strings);
	Cvar_ForceSet (&gl_strings, va("GL_VENDOR: %s\nGL_RENDERER: %s\n"
		"GL_VERSION: %s\nGL_EXTENSIONS: %s", gl_vendor, gl_renderer, gl_version, gl_extensions));
    Cvar_Register (&gl_maxtmu2);
#ifndef __APPLE__
	glClearColor (1,0,0,0);
#else
	glClearColor (0.2,0.2,0.2,1.0);
#endif

	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);

	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.666);

	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel (GL_FLAT);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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

#define GLM_Enabled GL_ShadersSupported
void GLM_OrthographicProjection(float left, float right, float top, float bottom, float zNear, float zFar);

void GL_OrthographicProjection(float left, float right, float top, float bottom, float zNear, float zFar)
{
	if (GLM_Enabled()) {
		GLM_OrthographicProjection(left, right, top, bottom, zNear, zFar);
	}
	else {
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(left, right, top, bottom, zNear, zFar);
	}
}

void GL_IdentityModelView(void)
{
	if (!GLM_Enabled()) {
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
	}
}

// TODO: GLM
void GL_PushMatrix(GLenum mode)
{
	if (!GLM_Enabled()) {
		glMatrixMode(mode);
		glPushMatrix();
	}
}

// TODO: GLM
void GL_PopMatrix(GLenum mode)
{
	if (!GLM_Enabled()) {
		glMatrixMode(mode);
		glPopMatrix();
	}
}

// TODO: GLM
void GL_GetMatrix(GLenum mode, GLfloat* matrix)
{
	if (GLM_Enabled()) {
		memset(matrix, 0, sizeof(GLfloat) * 16);
	}
	else {
		glGetFloatv(mode, matrix);
	}
}

void GL_GetViewport(GLint* view)
{
	if (GLM_Enabled()) {

	}
	else {
		glGetIntegerv(GL_VIEWPORT, (GLint *)view);
	}
}

static GLenum lastTextureMode = GL_MODULATE;

void GL_TextureEnvMode(GLenum mode)
{
	if (GL_ShadersSupported()) {
		// Just store for now
		lastTextureMode = mode;
	}
	else {
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode);
	}
}


// GLM Utility functions
void GLM_ConPrintShaderLog(GLuint shader)
{
	GLint log_length;
	char* buffer;

	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
	if (log_length) {
		GLsizei written;

		buffer = Q_malloc(log_length);
		glGetShaderInfoLog(shader, log_length, &written, buffer);
		Con_Printf(buffer);
		Q_free(buffer);
	}
}

void GLM_ConPrintProgramLog(GLuint program)
{
	GLint log_length;
	char* buffer;

	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
	if (log_length) {
		GLsizei written;

		buffer = Q_malloc(log_length);
		glGetProgramInfoLog(program, log_length, &written, buffer);
		Con_Printf(buffer);
		Q_free(buffer);
	}
}

static qbool GLM_CompileShader(const char* shaderText, GLenum shaderType, GLuint* shaderId)
{
	GLuint shader;
	GLint result;

	*shaderId = 0;
	shader = glCreateShader(shaderType);
	if (shader) {
		glShaderSource(shader, 1, &shaderText, NULL);
		glCompileShader(shader);
		glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
		if (result) {
			*shaderId = shader;
			return true;
		}

		Con_Printf("Shader->Compile(%X) failed\n", shaderType);
		GLM_ConPrintShaderLog(shader);
		glDeleteShader(shader);
	}
	else {
		Con_Printf("glCreateShader failed\n");
	}
	return false;
}

qbool GLM_CreateSimpleProgram(const char* friendlyName, const char* vertex_shader_text, const char* fragment_shader_text, glm_program_t* program)
{
	GLuint vertex_shader = 0;
	GLuint fragment_shader = 0;
	GLuint shader_program = 0;

	Con_Printf("--[ %s ]--\n", friendlyName);
	if (GL_ShadersSupported()) {
		GLint result = 0;

		if (GLM_CompileShader(vertex_shader_text, GL_VERTEX_SHADER, &vertex_shader)) {
			if (GLM_CompileShader(fragment_shader_text, GL_FRAGMENT_SHADER, &fragment_shader)) {
				Con_Printf("Shader compilation completed successfully\n");

				shader_program = glCreateProgram();
				if (shader_program) {
					glAttachShader(shader_program, fragment_shader);
					glAttachShader(shader_program, vertex_shader);
					glLinkProgram(shader_program);
					glGetProgramiv(shader_program, GL_LINK_STATUS, &result);

					if (result) {
						Con_Printf("ShaderProgram.Link() was successful\n");
						program->fragment_shader = fragment_shader;
						program->vertex_shader = vertex_shader;
						program->program = shader_program;
						return true;
					}
					else {
						Con_Printf("ShaderProgram.Link() failed\n");
						GLM_ConPrintProgramLog(shader_program);
					}
				}
			}
			else {
				Con_Printf("FragmentShader.Compile() failed\n");
				GLM_ConPrintShaderLog(fragment_shader);
			}
		}
	}
	else {
		Con_Printf("Shaders not supported\n");
		return false;
	}

	if (shader_program) {
		glDeleteProgram(shader_program);
	}
	if (fragment_shader) {
		glDeleteShader(fragment_shader);
	}
	if (vertex_shader) {
		glDeleteShader(vertex_shader);
	}
	return false;
}

#undef glColor3f
#undef glColor4f
#undef glColor3fv
#undef glColor3ubv
#undef glColor4ubv
#undef glColor4ub

void GL_Color3f(float r, float g, float b)
{
	if (GL_ShadersSupported()) {

	}
	else {
		glColor3f(r, g, b);
	}
}

void GL_Color4f(float r, float g, float b, float a)
{
	if (GL_ShadersSupported()) {

	}
	else {
		glColor4f(r, g, b, a);
	}
}

void GL_Color3fv(const float* rgbVec)
{
	if (GL_ShadersSupported()) {

	}
	else {
		glColor3fv(rgbVec);
	}
}

void GL_Color3ubv(const GLubyte* rgbVec)
{
	if (GL_ShadersSupported()) {

	}
	else {
		glColor3ubv(rgbVec);
	}

}

void GL_Color4ubv(const GLubyte* rgbaVec)
{
	if (GL_ShadersSupported()) {

	}
	else {
		glColor4ubv(rgbaVec);
	}

}

void GL_Color4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
	if (GL_ShadersSupported()) {

	}
	else {
		glColor4ub(r, g, b, a);
	}
}
