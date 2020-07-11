/*
Copyright (C) 2018 ezQuake team

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

// GL_program.c
// - All glsl program-related code

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "r_program.h"
#include "tr_types.h"

typedef enum {
	renderer_classic = 1,
	renderer_modern,
} renderer_id;

#define GL_DefineProgram_VF(program_id, name, expect_params, sourcename, renderer, compile_function) \
	{ \
		extern unsigned char sourcename##_vertex_glsl[]; \
		extern unsigned int sourcename##_vertex_glsl_len; \
		extern unsigned char sourcename##_fragment_glsl[]; \
		extern unsigned int sourcename##_fragment_glsl_len; \
		extern qbool compile_function(void); \
		memset(&program_data[program_id].shaders, 0, sizeof(program_data[program_id].shaders)); \
		program_data[program_id].friendly_name = name; \
		program_data[program_id].needs_params = expect_params; \
		program_data[program_id].shaders[shadertype_vertex].text = (const char*)sourcename##_vertex_glsl; \
		program_data[program_id].shaders[shadertype_vertex].length = sourcename##_vertex_glsl_len; \
		program_data[program_id].shaders[shadertype_fragment].text = (const char*)sourcename##_fragment_glsl; \
		program_data[program_id].shaders[shadertype_fragment].length = sourcename##_fragment_glsl_len; \
		program_data[program_id].initialised = true; \
		program_data[program_id].renderer_id = renderer; \
		program_data[program_id].compile_func = compile_function; \
	}

#define GL_DefineProgram_CS(program_id, name, expect_params, sourcename, renderer, compile_function) \
	{ \
		extern unsigned char sourcename##_compute_glsl[]; \
		extern unsigned int sourcename##_compute_glsl_len; \
		extern qbool compile_function(void); \
		memset(program_data[program_id].shaders, 0, sizeof(program_data[program_id].shaders)); \
		program_data[program_id].friendly_name = name; \
		program_data[program_id].needs_params = expect_params; \
		program_data[program_id].shaders[shadertype_compute].text = (const char*)sourcename##_compute_glsl; \
		program_data[program_id].shaders[shadertype_compute].length = sourcename##_compute_glsl_len; \
		program_data[program_id].initialised = true; \
		program_data[program_id].renderer_id = renderer; \
		program_data[program_id].compile_func = compile_function; \
	}

static void GL_BuildCoreDefinitions(void);
static qbool GL_CompileComputeShaderProgram(r_program_id program_id, const char* shadertext, unsigned int length);

static const GLenum glBarrierFlags[] = {
	GL_SHADER_IMAGE_ACCESS_BARRIER_BIT,
	GL_TEXTURE_FETCH_BARRIER_BIT
};

#ifdef C_ASSERT
C_ASSERT(sizeof(glBarrierFlags) / sizeof(glBarrierFlags[0]) == r_program_memory_barrier_count);
#endif

enum {
	shadertype_vertex,
	shadertype_fragment,
	shadertype_geometry,
	shadertype_compute,

	shadertype_count
};

typedef struct gl_shader_def_s {
	const char* text;
	unsigned int length;
} gl_shader_def_t;

typedef qbool(*program_compile_func_t)(void);

typedef struct gl_program_s {
	const char* friendly_name;
	qbool needs_params;
	program_compile_func_t compile_func;
	qbool initialised;
	gl_shader_def_t shaders[shadertype_count];

	GLuint shader_handles[shadertype_count];
	GLuint program;
	GLbitfield memory_barrier;

	char* included_definitions;
	qbool uniforms_found;

	unsigned int custom_options;
	qbool force_recompile;
	renderer_id renderer_id;
} gl_program_t;

typedef struct {
	r_program_id program_id;
	const char* name;
	int count;
	qbool transpose;

	qbool found;
	int location;
	int int_value;
} r_program_uniform_t;

static r_program_uniform_t program_uniforms[] = {
	// r_program_uniform_aliasmodel_drawmode
	{ r_program_aliasmodel, "mode", 1, false },
	// r_program_uniform_brushmodel_outlines
	{ r_program_brushmodel, "draw_outlines", 1, false },
	// r_program_uniform_brushmodel_sampler
	{ r_program_brushmodel, "SamplerNumber", 1, false },
	// r_program_uniform_sprite3d_alpha_test
	{ r_program_sprite3d, "alpha_test", 1, false },
	// r_program_uniform_hud_circle_matrix
	{ r_program_hud_circles, "matrix", 1, false },
	// r_program_uniform_hud_circle_color
	{ r_program_hud_circles, "color", 1, false },
	// r_program_uniform_post_process_glc_gamma
	{ r_program_post_process_glc, "gamma", 1, false },
	// r_program_uniform_post_process_glc_base,
	{ r_program_post_process_glc, "base", 1, false },
	// r_program_uniform_post_process_glc_overlay,
	{ r_program_post_process_glc, "overlay", 1, false },
	// r_program_uniform_post_process_glc_v_blend,
	{ r_program_post_process_glc, "v_blend", 1, false },
	// r_program_uniform_post_process_glc_contrast,
	{ r_program_post_process_glc, "contrast", 1, false },
	// r_program_uniform_sky_glc_cameraPosition,
	{ r_program_sky_glc, "cameraPosition", 1, false },
	// r_program_uniform_sky_glc_speedscale,
	{ r_program_sky_glc, "skySpeedscale", 1, false },
	// r_program_uniform_sky_glc_speedscale2,
	{ r_program_sky_glc, "skySpeedscale2", 1, false },
	// r_program_uniform_sky_glc_skyTex,
	{ r_program_sky_glc, "skyTex", 1, false },
	// r_program_uniform_sky_glc_skyDomeTex,
	{ r_program_sky_glc, "skyDomeTex", 1, false },
	// r_program_uniform_sky_glc_skyDomeCloudTex,
	{ r_program_sky_glc, "skyDomeCloudTex", 1, false },
	// r_program_uniform_turb_glc_texSampler,
	{ r_program_turb_glc, "texSampler", 1, false },
	// r_program_uniform_turb_glc_time,
	{ r_program_turb_glc, "time", 1, false },
	// r_program_uniform_caustics_glc_texSampler,
	{ r_program_caustics_glc, "texSampler", 1, false },
	// r_program_uniform_caustics_glc_time,
	{ r_program_caustics_glc, "time", 1, false },
	// r_program_uniform_aliasmodel_std_glc_angleVector,
	{ r_program_aliasmodel_std_glc, "angleVector", 1, false },
	// r_program_uniform_aliasmodel_std_glc_shadelight,
	{ r_program_aliasmodel_std_glc, "shadelight", 1, false },
	// r_program_uniform_aliasmodel_std_glc_ambientlight,
	{ r_program_aliasmodel_std_glc, "ambientlight", 1, false },
	// r_program_uniform_aliasmodel_std_glc_texSampler,
	{ r_program_aliasmodel_std_glc, "texSampler", 1, false },
	// r_program_uniform_aliasmodel_std_glc_fsTextureEnabled,
	{ r_program_aliasmodel_std_glc, "fsTextureEnabled", 1, false },
	// r_program_uniform_aliasmodel_std_glc_fsMinLumaMix,
	{ r_program_aliasmodel_std_glc, "fsMinLumaMix", 1, false },
	// r_program_uniform_aliasmodel_std_glc_time,
	{ r_program_aliasmodel_std_glc, "time", 1, false },
	// r_program_uniform_aliasmodel_std_glc_fsCausticEffects
	{ r_program_aliasmodel_std_glc, "fsCausticEffects", 1, false },
	// r_program_uniform_aliasmodel_std_glc_lerpFraction
	{ r_program_aliasmodel_std_glc, "lerpFraction", 1, false },
	// r_program_uniform_aliasmodel_std_glc_causticsSampler
	{ r_program_aliasmodel_std_glc, "causticsSampler", 1, false },
	// r_program_uniform_aliasmodel_shell_glc_fsBaseColor1,
	{ r_program_aliasmodel_shell_glc, "fsBaseColor1", 1, false },
	// r_program_uniform_aliasmodel_shell_glc_fsBaseColor2,
	{ r_program_aliasmodel_shell_glc, "fsBaseColor2", 1, false },
	// r_program_uniform_aliasmodel_shell_glc_texSampler,
	{ r_program_aliasmodel_shell_glc, "texSampler", 1, false },
	// r_program_uniform_aliasmodel_shell_glc_lerpFraction,
	{ r_program_aliasmodel_shell_glc, "lerpFraction", 1, false },
	// r_program_uniform_aliasmodel_shell_glc_scroll,
	{ r_program_aliasmodel_shell_glc, "scroll", 1, false },
	// r_program_uniform_aliasmodel_shadow_glc_lerpFraction,
	{ r_program_aliasmodel_shadow_glc, "lerpFraction", 1, false },
	// r_program_uniform_aliasmodel_shadow_glc_shadevector,
	{ r_program_aliasmodel_shadow_glc, "shadevector", 1, false },
	// r_program_uniform_aliasmodel_shadow_glc_lheight,
	{ r_program_aliasmodel_shadow_glc, "lheight", 1, false },
	// r_program_uniform_world_drawflat_glc_wallcolor,
	{ r_program_world_drawflat_glc, "wallcolor", 1, false },
	// r_program_uniform_world_drawflat_glc_floorcolor,
	{ r_program_world_drawflat_glc, "floorcolor", 1, false },
	// r_program_uniform_world_drawflat_glc_skycolor,
	{ r_program_world_drawflat_glc, "skycolor", 1, false },
	// r_program_uniform_world_drawflat_glc_watercolor,
	{ r_program_world_drawflat_glc, "watercolor", 1, false },
	// r_program_uniform_world_drawflat_glc_slimecolor,
	{ r_program_world_drawflat_glc, "slimecolor", 1, false },
	// r_program_uniform_world_drawflat_glc_lavacolor,
	{ r_program_world_drawflat_glc, "lavacolor", 1, false },
	// r_program_uniform_world_drawflat_glc_telecolor,
	{ r_program_world_drawflat_glc, "telecolor", 1, false },
	// r_program_uniform_world_textured_glc_lightmapSampler,
	{ r_program_world_textured_glc, "lightmapSampler", 1, false },
	// r_program_uniform_world_textured_glc_texSampler,
	{ r_program_world_textured_glc, "texSampler", 1, false },
	// r_program_uniform_world_textured_glc_lumaSampler,
	{ r_program_world_textured_glc, "lumaSampler", 1, false },
	// r_program_uniform_world_textured_glc_causticSampler
	{ r_program_world_textured_glc, "causticsSampler", 1, false },
	// r_program_uniform_world_textured_glc_detailSampler
	{ r_program_world_textured_glc, "detailSampler", 1, false },
	// r_program_uniform_world_textured_glc_time
	{ r_program_world_textured_glc, "time", 1, false },
	// r_program_uniform_world_textured_glc_lumaScale
	{ r_program_world_textured_glc, "lumaMultiplier", 1, false },
	// r_program_uniform_world_textured_glc_fbScale
	{ r_program_world_textured_glc, "fbMultiplier", 1, false },
	// r_program_uniform_sprites_glc_materialSampler,
	{ r_program_sprites_glc, "materialSampler", 1, false },
	// r_program_uniform_sprites_glc_alphaThreshold,
	{ r_program_sprites_glc, "alphaThreshold", 1, false },
	// r_program_uniform_hud_images_glc_primarySampler,
	{ r_program_hud_images_glc, "primarySampler", 1, false },
	// r_program_uniform_hud_images_glc_secondarySampler
	{ r_program_hud_images_glc, "secondarySampler", 1, false },
};

#ifdef C_ASSERT
C_ASSERT(sizeof(program_uniforms) / sizeof(program_uniforms[0]) == r_program_uniform_count);
#endif

typedef struct r_program_attribute_s {
	r_program_id program_id;
	const char* name;

	qbool found;
	GLint location;
} r_program_attribute_t;

static r_program_attribute_t program_attributes[] = {
	// r_program_attribute_aliasmodel_std_glc_flags
	{ r_program_aliasmodel_std_glc, "flags" },
	// r_program_attribute_aliasmodel_shell_glc_flags
	{ r_program_aliasmodel_shell_glc, "flags" },
	// r_program_attribute_aliasmodel_shadow_glc_flags
	{ r_program_aliasmodel_shadow_glc, "flags" },
	// r_program_attribute_world_drawflat_style
	{ r_program_world_drawflat_glc, "style" },
	// r_program_attribute_world_textured_style
	{ r_program_world_textured_glc, "style" },
	// r_program_attribute_world_textured_detailCoord
	{ r_program_world_textured_glc, "detailCoordInput" },
};

#ifdef C_ASSERT
C_ASSERT(sizeof(program_attributes) / sizeof(program_attributes[0]) == r_program_attribute_count);
#endif

static gl_program_t program_data[r_program_count];

// Cached OpenGL state
static r_program_id currentProgram = 0;

// Shader functions
typedef GLuint(APIENTRY *glCreateShader_t)(GLenum shaderType);
typedef void (APIENTRY *glShaderSource_t)(GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
typedef void (APIENTRY *glCompileShader_t)(GLuint shader);
typedef void (APIENTRY *glDeleteShader_t)(GLuint shader);
typedef void (APIENTRY *glGetShaderInfoLog_t)(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRY *glGetShaderiv_t)(GLuint shader, GLenum pname, GLint* params);
typedef void (APIENTRY *glGetShaderSource_t)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* source);

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
typedef void (APIENTRY *glUniform1i_t)(GLint location, GLint v0);
typedef void (APIENTRY *glUniform1f_t)(GLint location, GLfloat value);
typedef void (APIENTRY *glUniform2fv_t)(GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRY *glUniform3fv_t)(GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRY *glUniform4fv_t)(GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRY *glUniformMatrix4fv_t)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRY *glProgramUniform1i_t)(GLuint program, GLuint location, GLint v0);
typedef void (APIENTRY *glProgramUniform1f_t)(GLuint program, GLuint location, GLfloat value);
typedef void (APIENTRY *glProgramUniform2fv_t)(GLuint program, GLuint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRY *glProgramUniform3fv_t)(GLuint program, GLuint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRY *glProgramUniform4fv_t)(GLuint program, GLuint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRY *glProgramUniformMatrix4fv_t)(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);

// Attributes
typedef GLint (APIENTRY *glGetAttribLocation_t)(GLuint program, const GLchar* name);

// Compute shaders
typedef void (APIENTRY *glDispatchCompute_t)(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
typedef void (APIENTRY *glMemoryBarrier_t)(GLbitfield barriers);

// Shader functions
static glCreateShader_t      qglCreateShader = NULL;
static glShaderSource_t      qglShaderSource = NULL;
static glCompileShader_t     qglCompileShader = NULL;
static glDeleteShader_t      qglDeleteShader = NULL;
static glGetShaderInfoLog_t  qglGetShaderInfoLog = NULL;
static glGetShaderSource_t   qglGetShaderSource = NULL;
static glGetShaderiv_t       qglGetShaderiv = NULL;

// Program functions
static glCreateProgram_t     qglCreateProgram = NULL;
static glLinkProgram_t       qglLinkProgram = NULL;
static glDeleteProgram_t     qglDeleteProgram = NULL;
static glGetProgramiv_t      qglGetProgramiv = NULL;
static glGetProgramInfoLog_t qglGetProgramInfoLog = NULL;
static glUseProgram_t        qglUseProgram = NULL;
static glAttachShader_t      qglAttachShader = NULL;
static glDetachShader_t      qglDetachShader = NULL;

// Uniform functions
static glGetUniformLocation_t      qglGetUniformLocation = NULL;
static glUniform1i_t               qglUniform1i;
static glUniform1f_t               qglUniform1f;
static glUniform2fv_t              qglUniform2fv;
static glUniform3fv_t              qglUniform3fv;
static glUniform4fv_t              qglUniform4fv;
static glUniformMatrix4fv_t        qglUniformMatrix4fv;
static glProgramUniform1i_t        qglProgramUniform1i;
static glProgramUniform1f_t        qglProgramUniform1f;
static glProgramUniform2fv_t       qglProgramUniform2fv;
static glProgramUniform3fv_t       qglProgramUniform3fv;
static glProgramUniform4fv_t       qglProgramUniform4fv;
static glProgramUniformMatrix4fv_t qglProgramUniformMatrix4fv;

// Attribute functions
static glGetAttribLocation_t         qglGetAttribLocation;

// Compute shaders
static glDispatchCompute_t         qglDispatchCompute;
static glMemoryBarrier_t           qglMemoryBarrier;

#define MAX_SHADER_COMPONENTS 6
#define EZQUAKE_DEFINITIONS_STRING "#ezquake-definitions"

static char core_definitions[512];

// GLM Utility functions
static void GL_ConPrintShaderLog(GLuint shader)
{
	GLint log_length;
	GLint src_length;
	char* buffer;

	qglGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
	qglGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &src_length);
	if (log_length) {
		GLsizei written;

		buffer = Q_malloc(max(log_length, src_length));
		qglGetShaderInfoLog(shader, log_length, &written, buffer);
		Con_Printf(buffer);

		Q_free(buffer);
	}
}

static void GL_ConPrintProgramLog(GLuint program)
{
	GLint log_length;
	char* buffer;

	qglGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
	if (log_length) {
		GLsizei written;

		buffer = Q_malloc(log_length);
		qglGetProgramInfoLog(program, log_length, &written, buffer);
		Con_Printf(buffer);
		Q_free(buffer);
	}
}

// Uniform utility functions
static r_program_uniform_t* GL_ProgramUniformFind(r_program_uniform_id uniform_id)
{
	r_program_uniform_t* uniform = &program_uniforms[uniform_id];
	r_program_id program_id = uniform->program_id;

	if (program_data[program_id].program && !uniform->found) {
		uniform->location = qglGetUniformLocation(program_data[program_id].program, uniform->name);
		uniform->found = true;
	}

	return uniform;
}

static void R_ProgramFindUniformsForProgram(r_program_id program_id)
{
	r_program_uniform_id u;

	for (u = 0; u < r_program_uniform_count; ++u) {
		if (program_uniforms[u].program_id == program_id) {
			program_uniforms[u].found = false;
			GL_ProgramUniformFind(u);
		}
	}
}

static r_program_attribute_t* GL_ProgramAttributeFind(r_program_attribute_id attribute_id)
{
	r_program_attribute_t* attribute = &program_attributes[attribute_id];
	r_program_id program_id = attribute->program_id;

	if (program_data[program_id].program && !attribute->found) {
		attribute->location = qglGetAttribLocation(program_data[program_id].program, attribute->name);
		attribute->found = true;
	}

	return attribute;
}

static void R_ProgramFindAttributesForProgram(r_program_id program_id)
{
	r_program_attribute_id a;

	for (a = 0; a < r_program_attribute_count; ++a) {
		if (program_attributes[a].program_id == program_id) {
			program_attributes[a].found = false;
			GL_ProgramAttributeFind(a);
		}
	}
}

static qbool GL_CompileShader(GLsizei shaderComponents, const char* shaderText[], GLint shaderTextLength[], GLenum shaderType, GLuint* shaderId)
{
	GLuint shader;
	GLint result;

	*shaderId = 0;
	shader = qglCreateShader(shaderType);
	if (shader) {
		qglShaderSource(shader, shaderComponents, shaderText, shaderTextLength);
		qglCompileShader(shader);
		qglGetShaderiv(shader, GL_COMPILE_STATUS, &result);
		if (result) {
			*shaderId = shader;
			return true;
		}

		Con_Printf("Shader->Compile(%X) failed\n", shaderType);
		GL_ConPrintShaderLog(shader);
		qglDeleteShader(shader);
	}
	else {
		Con_Printf("glCreateShader failed\n");
	}
	return false;
}

// Couldn't find standard library call to do this (!?)
// Search for <search_string> in non-nul-terminated <source>
static const char* safe_strstr(const char* source, size_t max_length, const char* search_string)
{
	size_t search_length = strlen(search_string);
	const char* position;

	position = (const char*)memchr(source, search_string[0], max_length);
	while (position) {
		// Move along
		if (max_length < (position - source)) {
			break;
		}
		max_length -= (position - source);

		if (max_length < search_length) {
			break;
		}
		if (!memcmp(position, search_string, search_length)) {
			return position;
		}

		// Try again
		source = position;
		position = (const char*)memchr(source + 1, search_string[0], max_length);
	}

	return NULL;
}

static int GL_InsertDefinitions(
	const char* strings[],
	GLint lengths[],
	const char* definitions
)
{
	static unsigned char *glsl_constants_glsl = (unsigned char *)"", *glsl_common_glsl = (unsigned char *)"";
	unsigned int glsl_constants_glsl_len = 0, glsl_common_glsl_len = 0;
	const char* break_point;

	if (!strings[0] || !strings[0][0]) {
		return 0;
	}

#ifdef RENDERER_OPTION_MODERN_OPENGL
	if (R_UseModernOpenGL()) {
		extern unsigned char constants_glsl[], common_glsl[];
		extern unsigned int constants_glsl_len, common_glsl_len;

		glsl_constants_glsl = constants_glsl;
		glsl_common_glsl = common_glsl;
		glsl_constants_glsl_len = constants_glsl_len;
		glsl_common_glsl_len = common_glsl_len;
	}
#endif

	break_point = safe_strstr(strings[0], lengths[0], EZQUAKE_DEFINITIONS_STRING);
	
	if (break_point) {
		int position = break_point - strings[0];

		lengths[5] = lengths[0] - position - strlen(EZQUAKE_DEFINITIONS_STRING);
		lengths[4] = definitions ? strlen(definitions) : 0;
		lengths[3] = strlen(core_definitions);
		lengths[2] = glsl_common_glsl_len;
		lengths[1] = glsl_constants_glsl_len;
		lengths[0] = position;
		strings[5] = break_point + strlen(EZQUAKE_DEFINITIONS_STRING);
		strings[4] = definitions ? definitions : "";
		strings[3] = core_definitions;
		strings[2] = (const char*)glsl_common_glsl;
		strings[1] = (const char*)glsl_constants_glsl;

		return 6;
	}

	return 1;
}

static qbool GL_CompileProgram(
	gl_program_t* program
)
{
	GLuint shaders[shadertype_count] = { 0 };
	GLuint program_handle = 0;
	GLint result = 0;
	int i;

	const char* friendlyName = program->friendly_name;
	GLsizei vertex_components = 1;
	const char* vertex_shader_text[MAX_SHADER_COMPONENTS] = { program->shaders[shadertype_vertex].text, "", "", "", "", "" };
	GLint vertex_shader_text_length[MAX_SHADER_COMPONENTS] = { program->shaders[shadertype_vertex].length, 0, 0, 0, 0, 0 };
	GLsizei geometry_components = 1;
	const char* geometry_shader_text[MAX_SHADER_COMPONENTS] = { program->shaders[shadertype_geometry].text, "", "", "", "", "" };
	GLint geometry_shader_text_length[MAX_SHADER_COMPONENTS] = { program->shaders[shadertype_geometry].length, 0, 0, 0, 0, 0 };
	GLsizei fragment_components = 1;
	const char* fragment_shader_text[MAX_SHADER_COMPONENTS] = { program->shaders[shadertype_fragment].text, "", "", "", "", "" };
	GLint fragment_shader_text_length[MAX_SHADER_COMPONENTS] = { program->shaders[shadertype_fragment].length, 0, 0, 0, 0, 0 };

	Con_Printf("Compiling: %s\n", friendlyName);

	vertex_components = GL_InsertDefinitions(vertex_shader_text, vertex_shader_text_length, program->included_definitions);
	geometry_components = GL_InsertDefinitions(geometry_shader_text, geometry_shader_text_length, program->included_definitions);
	fragment_components = GL_InsertDefinitions(fragment_shader_text, fragment_shader_text_length, program->included_definitions);

	if (GL_CompileShader(vertex_components, vertex_shader_text, vertex_shader_text_length, GL_VERTEX_SHADER, &shaders[shadertype_vertex])) {
		if (geometry_shader_text[0] == NULL || GL_CompileShader(geometry_components, geometry_shader_text, geometry_shader_text_length, GL_GEOMETRY_SHADER, &shaders[shadertype_geometry])) {
			if (GL_CompileShader(fragment_components, fragment_shader_text, fragment_shader_text_length, GL_FRAGMENT_SHADER, &shaders[shadertype_fragment])) {
				Con_DPrintf("Shader compilation completed successfully\n");

				program_handle = qglCreateProgram();
				if (program_handle) {
					qglAttachShader(program_handle, shaders[shadertype_fragment]);
					qglAttachShader(program_handle, shaders[shadertype_vertex]);
					if (shaders[shadertype_geometry]) {
						qglAttachShader(program_handle, shaders[shadertype_geometry]);
					}
					qglLinkProgram(program_handle);
					qglGetProgramiv(program_handle, GL_LINK_STATUS, &result);

#if 0
					{
						int length = 0;
						char* src;

						qglGetShaderiv(fragment_shader, GL_SHADER_SOURCE_LENGTH, &length);
						src = Q_malloc(length + 1);
						qglGetShaderSource(fragment_shader, length, NULL, src);

						Con_Printf("Fragment-shader\n%s", src);
						Q_free(src);
					}
#endif

					if (result) {
						Con_DPrintf("ShaderProgram.Link() was successful\n");
						memcpy(program->shader_handles, shaders, sizeof(shaders));
						program->program = program_handle;
						program->uniforms_found = false;
						program->force_recompile = false;

						GL_TraceObjectLabelSet(GL_PROGRAM, program->program, -1, program->friendly_name);
						return true;
					}
					else {
						Con_Printf("ShaderProgram.Link() failed\n");
						GL_ConPrintProgramLog(program_handle);
					}
				}
			}
			else {
				Con_Printf("FragmentShader.Compile() failed\n");
			}
		}
		else {
			Con_Printf("GeometryShader.Compile() failed\n");
		}
	}
	else {
		Con_Printf("VertexShader.Compile() failed\n");
	}

	if (program_handle) {
		qglDeleteProgram(program_handle);
	}
	for (i = 0; i < sizeof(shaders) / sizeof(shaders[0]); ++i) {
		if (shaders[i]) {
			qglDeleteShader(shaders[i]);
		}
	}
	return false;
}

static void GL_CleanupShader(GLuint program, GLuint shader)
{
	if (shader) {
		if (program) {
			qglDetachShader(program, shader);
		}
		qglDeleteShader(shader);
	}
}

// Called during vid_shutdown
void GL_ProgramsShutdown(qbool restarting)
{
	r_program_id p;
	r_program_uniform_id u;
	r_program_attribute_id a;
	gl_program_t* prog;

	R_ProgramUse(r_program_none);

	// Detach & delete shaders
	for (p = r_program_none, prog = &program_data[p]; p < r_program_count; ++p, ++prog) {
		int i;

		for (i = 0; i < sizeof(prog->shaders) / sizeof(prog->shaders[0]); ++i) {
			GL_CleanupShader(prog->program, prog->shader_handles[i]);
			prog->shader_handles[i] = 0;
		}
	}

	for (p = r_program_none, prog = &program_data[p]; p < r_program_count; ++p, ++prog) {
		if (prog->program) {
			qglDeleteProgram(prog->program);
			prog->program = 0;
		}

		if (!restarting) {
			// Keep definitions if we're about to recompile after restart
			Q_free(prog->included_definitions);
		}
	}

	for (u = 0; u < r_program_uniform_count; ++u) {
		program_uniforms[u].found = false;
		program_uniforms[u].location = -1;
		program_uniforms[u].int_value = 0;
	}

	for (a = 0; a < r_program_attribute_count; ++a) {
		program_attributes[a].found = false;
		program_attributes[a].location = -1;
	}
}

static qbool GL_AppropriateRenderer(renderer_id renderer)
{
	if (R_UseImmediateOpenGL() && renderer == renderer_classic) {
		return true;
	}
	if (R_UseModernOpenGL() && renderer == renderer_modern) {
		return true;
	}
	return false;
}

// Called during vid_restart, as starting up again
void GL_ProgramsInitialise(void)
{
	r_program_id p;

	if (!(glConfig.supported_features & R_SUPPORT_RENDERING_SHADERS)) {
		return;
	}

	GL_BuildCoreDefinitions();

	program_data[r_program_none].friendly_name = "(none)";

	for (p = r_program_none; p < r_program_count; ++p) {
		if (!GL_AppropriateRenderer(program_data[p].renderer_id)) {
			continue;
		}

		if (!program_data[p].program && !program_data[p].needs_params && program_data[p].initialised) {
			gl_shader_def_t* compute = &program_data[p].shaders[shadertype_compute];
			if (compute->length) {
				GL_CompileComputeShaderProgram(p, compute->text, compute->length);
			}
			else {
				GL_CompileProgram(&program_data[p]);
			}
			R_ProgramFindUniformsForProgram(p);
			R_ProgramFindAttributesForProgram(p);
		}
	}
}

qbool R_ProgramRecompileNeeded(r_program_id program_id, unsigned int options)
{
	//
	const gl_program_t* program = &program_data[program_id];

	return (!program->program) || program->force_recompile || program->custom_options != options;
}

void GL_CvarForceRecompile(void)
{
	r_program_id p;

	for (p = r_program_none; p < r_program_count; ++p) {
		program_data[p].force_recompile = true;
	}

	GL_BuildCoreDefinitions();
}

static qbool GL_CompileComputeShaderProgram(r_program_id program_id, const char* shadertext, unsigned int length)
{
	gl_program_t* program = &program_data[program_id];
	const char* shader_text[MAX_SHADER_COMPONENTS] = { shadertext, "", "", "", "", "" };
	GLint shader_text_length[MAX_SHADER_COMPONENTS] = { length, 0, 0, 0, 0, 0 };
	int components;
	GLuint shader;

	program->program = 0;

	components = GL_InsertDefinitions(shader_text, shader_text_length, "");
	if (GL_CompileShader(components, shader_text, shader_text_length, GL_COMPUTE_SHADER, &shader)) {
		GLuint shader_program = qglCreateProgram();
		if (shader_program) {
			GLint result;

			qglAttachShader(shader_program, shader);
			qglLinkProgram(shader_program);
			qglGetProgramiv(shader_program, GL_LINK_STATUS, &result);

			if (result) {
				Con_DPrintf("ShaderProgram.Link() was successful\n");
				program->shader_handles[shadertype_compute] = shader;
				program->program = shader_program;
				program->uniforms_found = false;
				program->force_recompile = false;

				GL_TraceObjectLabelSet(GL_PROGRAM, program->program, -1, program->friendly_name);
				return true;
			}
			else {
				Con_Printf("ShaderProgram.Link() failed\n");
				GL_ConPrintProgramLog(shader_program);
			}
		}
	}

	return false;
}

void GL_LoadProgramFunctions(void)
{
	qbool rendering_shaders_support = true;

	glConfig.supported_features &= ~(R_SUPPORT_RENDERING_SHADERS | R_SUPPORT_COMPUTE_SHADERS);
	{
		// These are all core in OpenGL 2.0
		GL_LoadMandatoryFunctionExtension(glCreateShader, rendering_shaders_support);
		GL_LoadMandatoryFunctionExtension(glShaderSource, rendering_shaders_support);
		GL_LoadMandatoryFunctionExtension(glGetShaderSource, rendering_shaders_support);
		GL_LoadMandatoryFunctionExtension(glCompileShader, rendering_shaders_support);
		GL_LoadMandatoryFunctionExtension(glDeleteShader, rendering_shaders_support);
		GL_LoadMandatoryFunctionExtension(glGetShaderInfoLog, rendering_shaders_support);
		GL_LoadMandatoryFunctionExtension(glGetShaderiv, rendering_shaders_support);

		GL_LoadMandatoryFunctionExtension(glCreateProgram, rendering_shaders_support);
		GL_LoadMandatoryFunctionExtension(glLinkProgram, rendering_shaders_support);
		GL_LoadMandatoryFunctionExtension(glDeleteProgram, rendering_shaders_support);
		GL_LoadMandatoryFunctionExtension(glUseProgram, rendering_shaders_support);
		GL_LoadMandatoryFunctionExtension(glAttachShader, rendering_shaders_support);
		GL_LoadMandatoryFunctionExtension(glDetachShader, rendering_shaders_support);
		GL_LoadMandatoryFunctionExtension(glGetProgramInfoLog, rendering_shaders_support);
		GL_LoadMandatoryFunctionExtension(glGetProgramiv, rendering_shaders_support);

		GL_LoadMandatoryFunctionExtension(glGetUniformLocation, rendering_shaders_support);
		GL_LoadMandatoryFunctionExtension(glUniform1i, rendering_shaders_support);
		GL_LoadMandatoryFunctionExtension(glUniform1f, rendering_shaders_support);
		GL_LoadMandatoryFunctionExtension(glUniform2fv, rendering_shaders_support);
		GL_LoadMandatoryFunctionExtension(glUniform3fv, rendering_shaders_support);
		GL_LoadMandatoryFunctionExtension(glUniform4fv, rendering_shaders_support);
		GL_LoadMandatoryFunctionExtension(glUniformMatrix4fv, rendering_shaders_support);

		GL_LoadMandatoryFunctionExtension(glGetAttribLocation, rendering_shaders_support);

		glConfig.supported_features |= (rendering_shaders_support ? R_SUPPORT_RENDERING_SHADERS : 0);
	}

	if (GL_UseDirectStateAccess() || GL_VersionAtLeast(4, 1)) {
		GL_LoadOptionalFunction(glProgramUniform1i);
		GL_LoadOptionalFunction(glProgramUniform1f);
		GL_LoadOptionalFunction(glProgramUniform2fv);
		GL_LoadOptionalFunction(glProgramUniform3fv);
		GL_LoadOptionalFunction(glProgramUniform4fv);
		GL_LoadOptionalFunction(glProgramUniformMatrix4fv);
	}

	if (SDL_GL_ExtensionSupported("GL_ARB_compute_shader") && SDL_GL_ExtensionSupported("GL_ARB_shader_image_load_store")) {
		qbool compute_shaders_support = true;

		GL_LoadMandatoryFunctionExtension(glDispatchCompute, compute_shaders_support);
		GL_LoadMandatoryFunctionExtension(glMemoryBarrier, compute_shaders_support);

		glConfig.supported_features |= (compute_shaders_support ? R_SUPPORT_COMPUTE_SHADERS : 0);
	}
}

void R_ProgramUse(r_program_id program_id)
{
	if (program_id != currentProgram) {
		if (qglUseProgram) {
			qglUseProgram(program_data[program_id].program);
			R_TraceLogAPICall("R_ProgramUse(%s)", program_data[program_id].friendly_name);
		}

		currentProgram = program_id;
	}

#ifdef RENDERER_OPTION_MODERN_OPENGL
	if (R_UseModernOpenGL() && program_id != r_program_none) {
		GLM_UploadFrameConstants();
	}
#endif
}

r_program_id R_ProgramInUse(void)
{
	return currentProgram;
}

void R_ProgramInitialiseState(void)
{
	currentProgram = 0;

	GL_BuildCoreDefinitions();
}

// Compute shaders
static void GL_ProgramComputeDispatch(r_program_id program_id, unsigned int num_groups_x, unsigned int num_groups_y, unsigned int num_groups_z)
{
	R_ProgramUse(program_id);
	qglDispatchCompute(num_groups_x, num_groups_y, num_groups_z);
	if (program_data[program_id].memory_barrier) {
		qglMemoryBarrier(program_data[program_id].memory_barrier);
	}
}

void R_ProgramComputeDispatch(r_program_id program_id, unsigned int num_groups_x, unsigned int num_groups_y, unsigned int num_groups_z)
{
	GL_ProgramComputeDispatch(program_id, num_groups_x, num_groups_y, num_groups_z);
}

void R_ProgramComputeSetMemoryBarrierFlag(r_program_id program_id, r_program_memory_barrier_id barrier_id)
{
	program_data[program_id].memory_barrier |= glBarrierFlags[barrier_id];
}

// Wrappers
int R_ProgramCustomOptions(r_program_id program_id)
{
	return program_data[program_id].custom_options;
}

void R_ProgramSetCustomOptions(r_program_id program_id, int options)
{
	program_data[program_id].custom_options = options;
}

qbool R_ProgramReady(r_program_id program_id)
{
	return program_data[program_id].program != 0;
}

void R_ProgramUniform1i(r_program_uniform_id uniform_id, int value)
{
	r_program_uniform_t* uniform = GL_ProgramUniformFind(uniform_id);

	if (uniform->location >= 0) {
		if (qglProgramUniform1i) {
			qglProgramUniform1i(program_data[uniform->program_id].program, uniform->location, value);
		}
		else {
			R_ProgramUse(uniform->program_id);
			qglUniform1i(uniform->location, value);
		}
		uniform->int_value = value;
		R_TraceLogAPICall("%s(%s/%s,%d)", __FUNCTION__, program_data[uniform->program_id].friendly_name, uniform->name, value);
	}
}

void R_ProgramUniform1f(r_program_uniform_id uniform_id, float value)
{
	r_program_uniform_t* uniform = GL_ProgramUniformFind(uniform_id);

	if (uniform->location >= 0) {
		if (qglProgramUniform1f) {
			qglProgramUniform1f(program_data[uniform->program_id].program, uniform->location, value);
		}
		else {
			R_ProgramUse(uniform->program_id);
			qglUniform1f(uniform->location, value);
		}
		uniform->int_value = value;
		R_TraceLogAPICall("%s(%s/%s,%f)", __FUNCTION__, program_data[uniform->program_id].friendly_name, uniform->name, value);
	}
}

void R_ProgramUniform2fv(r_program_uniform_id uniform_id, const float* values)
{
	r_program_uniform_t* uniform = GL_ProgramUniformFind(uniform_id);

	if (uniform->location >= 0) {
		if (qglProgramUniform2fv) {
			qglProgramUniform2fv(program_data[uniform->program_id].program, uniform->location, uniform->count, values);
		}
		else {
			R_ProgramUse(uniform->program_id);
			qglUniform2fv(uniform->location, uniform->count, values);
		}
		R_TraceLogAPICall("%s(%s/%s)", __FUNCTION__, program_data[uniform->program_id].friendly_name, uniform->name);
	}
}

void R_ProgramUniform3fv(r_program_uniform_id uniform_id, const float* values)
{
	r_program_uniform_t* uniform = GL_ProgramUniformFind(uniform_id);

	if (uniform->location >= 0) {
		if (qglProgramUniform3fv) {
			qglProgramUniform3fv(program_data[uniform->program_id].program, uniform->location, uniform->count, values);
		}
		else {
			R_ProgramUse(uniform->program_id);
			qglUniform3fv(uniform->location, uniform->count, values);
		}
		R_TraceLogAPICall("%s(%s/%s)", __FUNCTION__, program_data[uniform->program_id].friendly_name, uniform->name);
	}
}

void R_ProgramUniform3fNormalize(r_program_uniform_id uniform_id, const byte* values)
{
	float float_values[3];

	float_values[0] = values[0] * 1.0f / 255;
	float_values[1] = values[1] * 1.0f / 255;
	float_values[2] = values[2] * 1.0f / 255;

	R_ProgramUniform3fv(uniform_id, float_values);
}

void R_ProgramUniform4fv(r_program_uniform_id uniform_id, const float* values)
{
	r_program_uniform_t* uniform = GL_ProgramUniformFind(uniform_id);

	if (uniform->location >= 0) {
		if (qglProgramUniform4fv) {
			qglProgramUniform4fv(program_data[uniform->program_id].program, uniform->location, uniform->count, values);
		}
		else {
			R_ProgramUse(uniform->program_id);
			qglUniform4fv(uniform->location, uniform->count, values);
		}
		R_TraceLogAPICall("%s(%s/%s)", __FUNCTION__, program_data[uniform->program_id].friendly_name, uniform->name);
	}
}

void R_ProgramUniformMatrix4fv(r_program_uniform_id uniform_id, const float* values)
{
	r_program_uniform_t* uniform = GL_ProgramUniformFind(uniform_id);

	if (uniform->location >= 0) {
		if (qglProgramUniformMatrix4fv) {
			qglProgramUniformMatrix4fv(program_data[uniform->program_id].program, uniform->location, uniform->count, uniform->transpose, values);
		}
		else {
			R_ProgramUse(uniform->program_id);
			qglUniformMatrix4fv(uniform->location, uniform->count, uniform->transpose, values);
		}
		R_TraceLogAPICall("%s(%s/%s)", __FUNCTION__, program_data[uniform->program_id].friendly_name, uniform->name);
	}
}

int R_ProgramUniformGet1i(r_program_uniform_id uniform_id, int default_value)
{
	if (program_uniforms[uniform_id].location >= 0) {
		return program_uniforms[uniform_id].int_value;
	}
	return default_value;
}

qbool R_ProgramCompile(r_program_id program_id)
{
	return R_ProgramCompileWithInclude(program_id, NULL);
}

qbool R_ProgramCompileWithInclude(r_program_id program_id, const char* included_definitions)
{
	gl_program_t* program = &program_data[program_id];

	memset(program->shader_handles, 0, sizeof(program->shader_handles));
	Q_free(program->included_definitions);
	program->included_definitions = included_definitions ? Q_strdup(included_definitions) : NULL;

	if (GL_CompileProgram(program)) {
		R_ProgramFindUniformsForProgram(program_id);
		R_ProgramFindAttributesForProgram(program_id);
		return true;
	}
	return false;
}

static void GL_BuildCoreDefinitions(void)
{
	// Set common definitions here (none yet)
	memset(core_definitions, 0, sizeof(core_definitions));

#ifdef RENDERER_OPTION_MODERN_OPENGL
	GL_DefineProgram_VF(r_program_aliasmodel, "aliasmodel", true, draw_aliasmodel, renderer_modern, GLM_CompileAliasModelProgram);
	GL_DefineProgram_VF(r_program_brushmodel, "brushmodel", true, draw_world, renderer_modern, GLM_CompileDrawWorldProgram);
	GL_DefineProgram_VF(r_program_sprite3d, "3d-sprites", false, draw_sprites, renderer_modern, GLM_Compile3DSpriteProgram);
	GL_DefineProgram_VF(r_program_hud_images, "image-draw", true, hud_draw_image, renderer_modern, GLM_CreateMultiImageProgram);
	GL_DefineProgram_VF(r_program_hud_circles, "circle-draw", false, hud_draw_circle, renderer_modern, GLM_CompileHudCircleProgram);
	GL_DefineProgram_VF(r_program_post_process, "post-process-screen", true, post_process_screen, renderer_modern, GLM_CompilePostProcessProgram);
	GL_DefineProgram_CS(r_program_lightmap_compute, "lightmaps", false, lighting, renderer_modern, GLM_CompileLightmapComputeProgram);
	GL_DefineProgram_VF(r_program_fx_world_geometry, "world-geometry", true, fx_world_geometry, renderer_modern, GLM_CompileWorldGeometryProgram);
#endif

#ifdef RENDERER_OPTION_CLASSIC_OPENGL
	GL_DefineProgram_VF(r_program_post_process_glc, "post-process-screen", true, glc_post_process_screen, renderer_classic, GLC_CompilePostProcessProgram);
	GL_DefineProgram_VF(r_program_sky_glc, "sky-rendering", true, glc_sky, renderer_classic, GLC_SkyProgramCompile);
	GL_DefineProgram_VF(r_program_turb_glc, "turb-rendering", true, glc_turbsurface, renderer_classic, GLC_TurbSurfaceProgramCompile);
	GL_DefineProgram_VF(r_program_caustics_glc, "caustics-rendering", true, glc_caustics, renderer_classic, GLC_CausticsProgramCompile);
	GL_DefineProgram_VF(r_program_aliasmodel_std_glc, "aliasmodel-std", true, glc_aliasmodel_std, renderer_classic, GLC_AliasModelStandardCompile);
	GL_DefineProgram_VF(r_program_aliasmodel_shell_glc, "aliasmodel-shell", true, glc_aliasmodel_shell, renderer_classic, GLC_AliasModelShellCompile);
	GL_DefineProgram_VF(r_program_aliasmodel_shadow_glc, "aliasmodel-shadow", true, glc_aliasmodel_shadow, renderer_classic, GLC_AliasModelShadowCompile);
	GL_DefineProgram_VF(r_program_world_drawflat_glc, "drawflat-world", true, glc_world_drawflat, renderer_classic, GLC_DrawflatProgramCompile);
	GL_DefineProgram_VF(r_program_world_textured_glc, "textured-world", true, glc_world_textured, renderer_classic, GLC_PreCompileWorldPrograms);
	GL_DefineProgram_VF(r_program_world_secondpass_glc, "secondpass-world", true, glc_world_secondpass, renderer_classic, GLC_PreCompileWorldPrograms);
	GL_DefineProgram_VF(r_program_sprites_glc, "3d-sprites", true, glc_draw_sprites, renderer_classic, GLC_CompileSpriteProgram);
	GL_DefineProgram_VF(r_program_hud_images_glc, "hud-images", true, glc_hud_images, renderer_classic, GLC_ProgramHudImagesCompile);
#endif
}

int R_ProgramAttributeLocation(r_program_attribute_id attr_id)
{
	if (attr_id < 0 || attr_id >= r_program_attribute_count || !program_attributes[attr_id].found) {
		return -1;
	}

	return program_attributes[attr_id].location;
}

r_program_id R_ProgramForAttribute(r_program_attribute_id attr_id)
{
	if (attr_id < 0 || attr_id >= r_program_attribute_count) {
		return r_program_none;
	}

	return program_attributes[attr_id].program_id;
}

void R_ProgramCompileAll(void)
{
	int i;

	for (i = 0; i < r_program_count; ++i) {
		if (!GL_AppropriateRenderer(program_data[i].renderer_id)) {
			continue;
		}

		if (program_data[i].initialised && program_data[i].compile_func) {
			program_data[i].compile_func();
		}
	}
}
