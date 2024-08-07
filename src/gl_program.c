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

#define STDOPTIONS_FOG_LINEAR     (1 << 0)
#define STDOPTIONS_FOG_EXP        (1 << 1)
#define STDOPTIONS_FOG_EXP2       (1 << 2)

#define STDOPTIONS_NONE           (0)
#define STDOPTIONS_FOG            (STDOPTIONS_FOG_LINEAR | STDOPTIONS_FOG_EXP | STDOPTIONS_FOG_EXP2)

#define GL_DefineProgram_VF(program_id, name, expect_params, sourcename, renderer, compile_function, standard_flags) \
	{ \
		extern unsigned char sourcename##_vertex_glsl[]; \
		extern unsigned int sourcename##_vertex_glsl_len; \
		extern unsigned char sourcename##_fragment_glsl[]; \
		extern unsigned int sourcename##_fragment_glsl_len; \
		extern qbool compile_function(void); \
		int i; \
\
		for (i = 0; i < MAX_SUBPROGRAMS; ++i) { \
			gl_program_t* prog = R_SpecificSubProgram((program_id), i); \
			memset(&prog->shaders, 0, sizeof(prog->shaders)); \
			strlcpy(prog->friendly_name, va("%s[%d]", (name), i), sizeof(prog->friendly_name)); \
			prog->needs_params = expect_params; \
			prog->shaders[shadertype_vertex].text = (const char*)sourcename##_vertex_glsl; \
			prog->shaders[shadertype_vertex].length = sourcename##_vertex_glsl_len; \
			prog->shaders[shadertype_fragment].text = (const char*)sourcename##_fragment_glsl; \
			prog->shaders[shadertype_fragment].length = sourcename##_fragment_glsl_len; \
			prog->initialised = true; \
			prog->renderer_id = renderer; \
			prog->compile_func = compile_function; \
			prog->standard_option_flags = standard_flags; \
		} \
	}

#define GL_DefineProgram_CS(program_id, name, expect_params, sourcename, renderer, compile_function, option_flags) \
	{ \
		extern unsigned char sourcename##_compute_glsl[]; \
		extern unsigned int sourcename##_compute_glsl_len; \
		extern qbool compile_function(void); \
		int i; \
\
		for (i = 0; i < MAX_SUBPROGRAMS; ++i) { \
			gl_program_t* prog = R_SpecificSubProgram((program_id), i); \
			memset(prog->shaders, 0, sizeof(prog->shaders)); \
			strlcat(prog->friendly_name, name, sizeof(prog->friendly_name)); \
			prog->needs_params = expect_params; \
			prog->shaders[shadertype_compute].text = (const char*)sourcename##_compute_glsl; \
			prog->shaders[shadertype_compute].length = sourcename##_compute_glsl_len; \
			prog->initialised = true; \
			prog->renderer_id = renderer; \
			prog->compile_func = compile_function; \
			prog->standard_option_flags = option_flags; \
		} \
	}

static void GL_BuildCoreDefinitions(void);

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

#define MAX_SUBPROGRAMS 32
#define R_CurrentSubProgram(program_id) (&program_data[(program_id)][program_currentSubProgram[(program_id)]])
#define R_SpecificSubProgram(program_id, sub_id) (&program_data[(program_id)][(sub_id)])

typedef struct {
	qbool found;
	int location;
	int int_value;
} gl_program_uniform_t;

#ifdef RENDERER_OPTION_CLASSIC_OPENGL
typedef struct {
	qbool found;
	GLint location;
} gl_program_attribute_t;
#endif

typedef enum {
	r_program_std_uniform_fog_linear_start,
	r_program_std_uniform_fog_linear_end,
	r_program_std_uniform_fog_density,
	r_program_std_uniform_fog_color,
	r_program_std_uniform_count
} r_program_std_uniform_id;

typedef struct {
	r_program_id program_id;
	const char* name;
	int count;
	qbool transpose;
} r_program_uniform_t;

static r_program_uniform_t program_std_uniforms[] = {
	// r_program_std_uniform_fog_linear_start
	{ r_program_none, "fogMinZ", 1, false },
	// r_program_std_uniform_fog_linear_end,
	{ r_program_none, "fogMaxZ", 1, false },
	// r_program_std_uniform_fog_density,
	{ r_program_none, "fogDensity", 1, false },
	// r_program_std_uniform_fog_color,
	{ r_program_none, "fogColor", 1, false }
};

#ifdef C_ASSERT
C_ASSERT(sizeof(program_std_uniforms) / sizeof(program_std_uniforms[0]) == r_program_std_uniform_count);
#endif

typedef struct gl_program_s {
	char friendly_name[128];
	qbool needs_params;
	program_compile_func_t compile_func;
	qbool initialised;
	gl_shader_def_t shaders[shadertype_count];

	GLuint shader_handles[shadertype_count];
	GLuint program;
	GLbitfield memory_barrier;

	char* included_definitions;
	gl_program_uniform_t uniforms[r_program_uniform_count];
#ifdef RENDERER_OPTION_CLASSIC_OPENGL
	gl_program_attribute_t attributes[r_program_attribute_count];
#endif

	unsigned int custom_options;
	qbool force_recompile;
	renderer_id renderer_id;
	unsigned int standard_options;
	unsigned int standard_option_flags;
	gl_program_uniform_t std_uniforms[r_program_std_uniform_count];
} gl_program_t;

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
	// r_program_uniform_world_textured_glc_r_floorcolor
	{ r_program_world_textured_glc, "r_floorcolor", 1, false },
	// r_program_uniform_world_textured_glc_r_wallcolor
	{ r_program_world_textured_glc, "r_wallcolor", 1, false },
	// r_program_uniform_sprites_glc_materialSampler,
	{ r_program_sprites_glc, "materialSampler", 1, false },
	// r_program_uniform_sprites_glc_alphaThreshold,
	{ r_program_sprites_glc, "alphaThreshold", 1, false },
	// r_program_uniform_hud_images_glc_primarySampler,
	{ r_program_hud_images_glc, "primarySampler", 1, false },
	// r_program_uniform_hud_images_glc_secondarySampler
	{ r_program_hud_images_glc, "secondarySampler", 1, false },
	// r_program_uniform_aliasmodel_outline_glc_lerpFraction
	{ r_program_aliasmodel_outline_glc, "lerpFraction", 1, false },
	// r_program_uniform_aliasmodel_outline_glc_outlineScale
	{ r_program_aliasmodel_outline_glc, "outlineScale", 1, false },
	// r_program_uniform_brushmodel_alphatested_outlines,
	{ r_program_brushmodel_alphatested, "draw_outlines", 1, false },
	// r_program_uniform_brushmodel_alphatested_sampler,
	{ r_program_brushmodel_alphatested, "SamplerNumber", 1, false },
	// r_program_uniform_turb_glc_alpha
	{ r_program_turb_glc, "alpha", 1, false },
	// r_program_uniform_turb_glc_color
	{ r_program_turb_glc, "color", 1, false },
	// r_program_uniform_simple_color
	{ r_program_simple, "color", 1, false },
	// r_program_uniform_world_textures_glc_texture_multiplier
	{ r_program_world_textured_glc, "texture_multiplier", 1, false },
	// r_program_uniform_simple3d_color
	{ r_program_simple3d, "color", 1, false },
	// r_program_uniform_lighting_firstLightmap,
	{ r_program_lightmap_compute, "firstLightmap", 1, false },
	// r_program_uniform_turb_glc_fog_skyFogMix,
	{ r_program_sky_glc, "skyFogMix", 1, false },
	// r_program_uniform_outline_color
	{ r_program_fx_world_geometry, "outline_color", 1, false },
	// r_program_uniform_outline_depth_threshold
	{ r_program_fx_world_geometry, "outline_depth_threshold", 1, false },
	// r_program_uniform_outline_normal_threshold
	{ r_program_fx_world_geometry, "outline_normal_threshold", 1, false },
	// r_program_uniform_outline_scale
	{ r_program_fx_world_geometry, "outline_scale", 1, false },
	// r_program_uniform_aliasmodel_outline_color_model
	{ r_program_aliasmodel, "outline_color", 1, false },
	// r_program_uniform_aliasmodel_outline_color_team
	{ r_program_aliasmodel, "outline_color_team", 1, false },
	// r_program_uniform_aliasmodel_outline_color_enemy
	{ r_program_aliasmodel, "outline_color_enemy", 1, false },
	// r_program_uniform_aliasmodel_outline_use_player_color
	{ r_program_aliasmodel, "outline_use_player_color", 1, false },
	// r_program_uniform_aliasmodel_outline_scale
	{ r_program_aliasmodel, "outline_scale", 1, false },
};

#ifdef C_ASSERT
C_ASSERT(sizeof(program_uniforms) / sizeof(program_uniforms[0]) == r_program_uniform_count);
#endif

#ifdef RENDERER_OPTION_CLASSIC_OPENGL
typedef struct r_program_attribute_s {
	r_program_id program_id;
	const char* name;
} r_program_attribute_t;

static r_program_attribute_t program_attributes[] = {
	// r_program_attribute_aliasmodel_std_glc_flags
	{ r_program_aliasmodel_std_glc, "flags" },
	// r_program_attribute_aliasmodel_shell_glc_flags
	{ r_program_aliasmodel_shell_glc, "flags" },
	// r_program_attribute_aliasmodel_shadow_glc_flags
	{ r_program_aliasmodel_shadow_glc, "flags" },
	// r_program_attribute_aliasmodel_outline_glc_flags
	{ r_program_aliasmodel_outline_glc, "flags" },
	// r_program_attribute_world_drawflat_style
	{ r_program_world_drawflat_glc, "style" },
	// r_program_attribute_world_textured_style
	{ r_program_world_textured_glc, "style" },
	// r_program_attribute_world_textured_detailCoord
	{ r_program_world_textured_glc, "detailCoordInput" },
};

#ifdef C_ASSERT
C_ASSERT(sizeof(program_attributes) / sizeof(program_attributes[0]) == r_program_attribute_count);
#endif // C_ASSERT

static void R_ProgramFindAttributesForProgram(r_program_id program_id);
#else // RENDERER_OPTION_CLASSIC_OPENGL
#define R_ProgramFindAttributesForProgram(x)
#endif // !RENDERER_OPTION_CLASSIC_OPENGL

static qbool GL_CompileComputeShaderProgram(gl_program_t* prog, const char* shadertext, unsigned int length);

static gl_program_t program_data[r_program_count][MAX_SUBPROGRAMS];
static int program_currentSubProgram[r_program_count];

// Cached OpenGL state
static r_program_id currentProgram = 0;
static int currentProgramOptionSet = 0;

// Shader functions
GL_StaticFunctionDeclaration(glCreateShader, "shaderType=%u", "returned=%u", GLuint, GLenum shaderType)
GL_StaticFunctionWrapperBody(glCreateShader, GLuint, shaderType)
GL_StaticProcedureDeclaration(glShaderSource, "shader=%u, count=%d, string=%p, length=%p", GLuint shader, GLsizei count, const GLchar** string, const GLint* length)
GL_StaticProcedureDeclaration(glCompileShader, "shader=%u", GLuint shader)
GL_StaticProcedureDeclaration(glDeleteShader, "shader=%u", GLuint shader)
GL_StaticProcedureDeclaration(glGetShaderInfoLog, "shader=%u, maxLength=%d, length=%p, infoLog=%p", GLuint shader, GLsizei maxLength, GLsizei* length, GLchar* infoLog)
GL_StaticProcedureDeclaration(glGetShaderiv, "shader=%u, pname=%u, params=%p", GLuint shader, GLenum pname, GLint* params)
GL_StaticProcedureDeclaration(glGetShaderSource, "shader=%u, bufSize=%d, length=%p, source=%p", GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* source)

// Program functions
GL_StaticFunctionDeclaration(glCreateProgram, "", "result=%u", GLuint, void)
GL_StaticFunctionWrapperBodyNoArgs(glCreateProgram, GLuint)

GL_StaticProcedureDeclaration(glLinkProgram, "program=%u", GLuint program)
GL_StaticProcedureDeclaration(glDeleteProgram, "program=%u", GLuint program)
GL_StaticProcedureDeclaration(glGetProgramInfoLog, "program=%u, maxLength=%d, length=%p, infoLog=%p", GLuint program, GLsizei maxLength, GLsizei* length, GLchar* infoLog)
GL_StaticProcedureDeclaration(glUseProgram, "program=%u", GLuint program)
GL_StaticProcedureDeclaration(glAttachShader, "program=%u, shader=%u", GLuint program, GLuint shader)
GL_StaticProcedureDeclaration(glDetachShader, "program=%u, shader=%u", GLuint program, GLuint shader)
GL_StaticProcedureDeclaration(glGetProgramiv, "program=%u, pname=%u, params=%p", GLuint program, GLenum pname, GLint* params)

// Uniforms
GL_StaticFunctionDeclaration(glGetUniformLocation, "program=%u, name=%s", "result=%d", GLint, GLuint program, const GLchar* name)
GL_StaticFunctionWrapperBody(glGetUniformLocation, GLint, program, name)

GL_StaticProcedureDeclaration(glUniform1i, "location=%d, v0=%d", GLint location, GLint v0)
GL_StaticProcedureDeclaration(glUniform1f, "location=%d, value=%f", GLint location, GLfloat value)
GL_StaticProcedureDeclaration(glUniform2fv, "location=%d, count=%d, value=%p", GLint location, GLsizei count, const GLfloat* value)
GL_StaticProcedureDeclaration(glUniform3fv, "location=%d, count=%d, value=%p", GLint location, GLsizei count, const GLfloat* value)
GL_StaticProcedureDeclaration(glUniform4fv, "location=%d, count=%d, value=%p", GLint location, GLsizei count, const GLfloat* value)
GL_StaticProcedureDeclaration(glUniformMatrix4fv, "location=%d, count=%d, transpose=%d, value=%p", GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
GL_StaticProcedureDeclaration(glProgramUniform1i, "program=%u, location=%u, v0=%d", GLuint program, GLuint location, GLint v0)
GL_StaticProcedureDeclaration(glProgramUniform1f, "program=%u, location=%u, value=%f", GLuint program, GLuint location, GLfloat value)
GL_StaticProcedureDeclaration(glProgramUniform2fv, "program=%u, location=%u, count=%d, value=%p", GLuint program, GLuint location, GLsizei count, const GLfloat* value)
GL_StaticProcedureDeclaration(glProgramUniform3fv, "program=%u, location=%u, count=%d, value=%p", GLuint program, GLuint location, GLsizei count, const GLfloat* value)
GL_StaticProcedureDeclaration(glProgramUniform4fv, "program=%u, location=%u, count=%d, value=%p", GLuint program, GLuint location, GLsizei count, const GLfloat* value)
GL_StaticProcedureDeclaration(glProgramUniformMatrix4fv, "program=%u, location=%d, count=%d, transpose=%d, value=%p", GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)

#ifdef RENDERER_OPTION_CLASSIC_OPENGL
// Attributes
GL_StaticFunctionDeclaration(glGetAttribLocation, "program=%u, name=%s", "result=%d", GLint, GLuint program, const GLchar* name)
GL_StaticFunctionWrapperBody(glGetAttribLocation, GLint, program, name)
#endif

// Compute shaders
GL_StaticProcedureDeclaration(glDispatchCompute, "num_groups_x=%u, num_groups_y=%u, num_groups_z=%u", GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z)
GL_StaticProcedureDeclaration(glMemoryBarrier, "barriers=%u", GLbitfield barriers)

#define MAX_SHADER_COMPONENTS 7
#define EZQUAKE_DEFINITIONS_STRING "#ezquake-definitions"

static char core_definitions[2048];
static char standard_definitions[2048];

static unsigned int R_ProgramStandardOptions(unsigned int option_flags)
{
	unsigned int flags =
		(r_refdef2.fog_calculation == fogcalc_linear ? STDOPTIONS_FOG_LINEAR : 0) |
		(r_refdef2.fog_calculation == fogcalc_exp    ? STDOPTIONS_FOG_EXP    : 0) |
		(r_refdef2.fog_calculation == fogcalc_exp2   ? STDOPTIONS_FOG_EXP2   : 0);

	return flags & option_flags;
}

static void R_ProgramBuildStandardDefines(unsigned int option_flags)
{
	unsigned int options = R_ProgramStandardOptions(option_flags);

	standard_definitions[0] = '\0';
	if (options & STDOPTIONS_FOG) {
		strlcat(standard_definitions, "#define DRAW_FOG\n", sizeof(standard_definitions));
		if (options & STDOPTIONS_FOG_LINEAR) {
			strlcat(standard_definitions, "#define FOG_LINEAR\n", sizeof(standard_definitions));
		}
		else if (options & STDOPTIONS_FOG_EXP) {
			strlcat(standard_definitions, "#define FOG_EXP\n", sizeof(standard_definitions));
		}
		else if (options & STDOPTIONS_FOG_EXP2) {
			strlcat(standard_definitions, "#define FOG_EXP2\n", sizeof(standard_definitions));
		}
	}
}

// GLM Utility functions
static void GL_ConPrintShaderLog(GLuint shader)
{
	GLint log_length;
	GLint src_length;
	char* buffer;

	GL_Procedure(glGetShaderiv, shader, GL_INFO_LOG_LENGTH, &log_length);
	GL_Procedure(glGetShaderiv, shader, GL_SHADER_SOURCE_LENGTH, &src_length);
	if (log_length || (src_length > 0 && developer.integer == 3)) {
		GLsizei written;

		buffer = Q_malloc(max(log_length, src_length) + 1);
		GL_Procedure(glGetShaderInfoLog, shader, log_length, &written, buffer);
		Con_Printf("-- Version: %s\n", glConfig.version_string);
		Con_Printf("--    GLSL: %s\n", glConfig.glsl_version);
		Con_Printf("%s\n", buffer);

		if (developer.integer == 2 || developer.integer == 3) {
			GL_Procedure(glGetShaderSource, shader, src_length, &written, buffer);
			Con_Printf("%s\n", buffer);
		}

		Q_free(buffer);
	}
}

static void GL_ConPrintProgramLog(GLuint program)
{
	GLint log_length;
	char* buffer;

	GL_Procedure(glGetProgramiv, program, GL_INFO_LOG_LENGTH, &log_length);
	if (log_length) {
		GLsizei written;

		buffer = Q_malloc(log_length);
		GL_Procedure(glGetProgramInfoLog, program, log_length, &written, buffer);
		Con_Printf(buffer);
		Q_free(buffer);
	}
}

// Uniform utility functions
static gl_program_uniform_t* GL_ProgramUniformFind(r_program_uniform_id uniform_id)
{
	r_program_uniform_t* uniform = &program_uniforms[uniform_id];
	r_program_id program_id = uniform->program_id;
	gl_program_t* program = R_CurrentSubProgram(program_id);
	gl_program_uniform_t* program_uniform = &program->uniforms[uniform_id];

	if (program->program && !program_uniform->found) {
		program_uniform->location = GL_Function(glGetUniformLocation, program->program, uniform->name);
		program_uniform->found = true;
	}

	return program_uniform;
}

static void R_ProgramFindUniformsForProgram(r_program_id program_id)
{
	r_program_uniform_id u;

	for (u = 0; u < r_program_uniform_count; ++u) {
		if (program_uniforms[u].program_id == program_id) {
			GL_ProgramUniformFind(u);
		}
	}
}

static qbool GL_CompileShader(GLsizei shaderComponents, const char* shaderText[], GLint shaderTextLength[], GLenum shaderType, GLuint* shaderId)
{
	GLuint shader;
	GLint result;

	*shaderId = 0;
	shader = GL_Function(glCreateShader, shaderType);
	if (shader) {
		GL_Procedure(glShaderSource, shader, shaderComponents, shaderText, shaderTextLength);
		GL_Procedure(glCompileShader, shader);
		GL_Procedure(glGetShaderiv, shader, GL_COMPILE_STATUS, &result);
		if (result) {
			*shaderId = shader;
			if (developer.integer == 3) {
				Con_Printf("Shader->Compile(%X) succeeded\n", shaderType);
				GL_ConPrintShaderLog(shader);
			}
			return true;
		}

		Con_Printf("Shader->Compile(%X) failed\n", shaderType);
		GL_ConPrintShaderLog(shader);
		GL_Procedure(glDeleteShader, shader);
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

	while (true) {
		position = (const char*)memchr(source, search_string[0], max_length);
		if (!position) {
			break;
		}

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
		source = position + 1;
		max_length -= 1;
	}

	return NULL;
}

#if 0
static void GL_DebugPrintShaderText(GLuint shader)
{
	int length = 0;
	char* src;

	GL_Procedure(glGetShaderiv, shader, GL_SHADER_SOURCE_LENGTH, &length);
	src = Q_malloc(length + 1);
	GL_Procedure(glGetShaderSource, shader, length, NULL, src);

	DebugOutput(src);
	Q_free(src);
}
#endif

static int GL_InsertDefinitions(
	const char* strings[],
	GLint lengths[],
	const char* definitions
)
{
	static unsigned char *glsl_constants_glsl = (unsigned char *)"", *glsl_common_glsl = (unsigned char *)"";
	unsigned int glsl_constants_glsl_len = 0, glsl_common_glsl_len = 0;
	const char* break_point;
	int i;

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

		lengths[6] = lengths[0] - position - strlen(EZQUAKE_DEFINITIONS_STRING);
		lengths[5] = strlen(core_definitions);
		lengths[4] = definitions ? strlen(definitions) : 0;
		lengths[3] = strlen(standard_definitions);
		lengths[2] = glsl_common_glsl_len;
		lengths[1] = glsl_constants_glsl_len;
		lengths[0] = position;
		strings[6] = break_point + strlen(EZQUAKE_DEFINITIONS_STRING);
		strings[5] = core_definitions;
		strings[4] = definitions ? definitions : "";
		strings[3] = standard_definitions;
		strings[2] = (const char*)glsl_common_glsl;
		strings[1] = (const char*)glsl_constants_glsl;

		// Some drivers interpret length 0 as nul terminated
		// spec is < 0 (https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glShaderSource.xhtml)
		for (i = 0; i < MAX_SHADER_COMPONENTS; ++i) {
			if (lengths[i] == 0) {
				strings[i] = "";
			}
		}

		return 7;
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

	GLsizei vertex_components = 1;
	const char* vertex_shader_text[MAX_SHADER_COMPONENTS] = { program->shaders[shadertype_vertex].text, "", "", "", "", "" };
	GLint vertex_shader_text_length[MAX_SHADER_COMPONENTS] = { program->shaders[shadertype_vertex].length, 0, 0, 0, 0, 0 };
	GLsizei geometry_components = 1;
	const char* geometry_shader_text[MAX_SHADER_COMPONENTS] = { program->shaders[shadertype_geometry].text, "", "", "", "", "" };
	GLint geometry_shader_text_length[MAX_SHADER_COMPONENTS] = { program->shaders[shadertype_geometry].length, 0, 0, 0, 0, 0 };
	GLsizei fragment_components = 1;
	const char* fragment_shader_text[MAX_SHADER_COMPONENTS] = { program->shaders[shadertype_fragment].text, "", "", "", "", "" };
	GLint fragment_shader_text_length[MAX_SHADER_COMPONENTS] = { program->shaders[shadertype_fragment].length, 0, 0, 0, 0, 0 };

	DebugOutput(va("Compiling: %s\n", program->friendly_name));

	R_ProgramBuildStandardDefines(program->standard_option_flags);

	vertex_components = GL_InsertDefinitions(vertex_shader_text, vertex_shader_text_length, program->included_definitions);
	geometry_components = GL_InsertDefinitions(geometry_shader_text, geometry_shader_text_length, program->included_definitions);
	fragment_components = GL_InsertDefinitions(fragment_shader_text, fragment_shader_text_length, program->included_definitions);

	if (GL_CompileShader(vertex_components, vertex_shader_text, vertex_shader_text_length, GL_VERTEX_SHADER, &shaders[shadertype_vertex])) {
		if (geometry_shader_text[0] == NULL || GL_CompileShader(geometry_components, geometry_shader_text, geometry_shader_text_length, GL_GEOMETRY_SHADER, &shaders[shadertype_geometry])) {
			if (GL_CompileShader(fragment_components, fragment_shader_text, fragment_shader_text_length, GL_FRAGMENT_SHADER, &shaders[shadertype_fragment])) {
				Con_DPrintf("Shader compilation completed successfully\n");

				program_handle = GL_FunctionNoArgs(glCreateProgram);
				if (program_handle) {
					GL_Procedure(glAttachShader, program_handle, shaders[shadertype_fragment]);
					GL_Procedure(glAttachShader, program_handle, shaders[shadertype_vertex]);
					if (shaders[shadertype_geometry]) {
						GL_Procedure(glAttachShader, program_handle, shaders[shadertype_geometry]);
					}
					GL_Procedure(glLinkProgram, program_handle);
					GL_Procedure(glGetProgramiv, program_handle, GL_LINK_STATUS, &result);

					if (result) {
						Con_DPrintf("ShaderProgram.Link() was successful\n");
						memcpy(program->shader_handles, shaders, sizeof(shaders));
						program->program = program_handle;
						memset(program->uniforms, 0, sizeof(program->uniforms));
#ifdef RENDERER_OPTION_CLASSIC_OPENGL
						memset(program->attributes, 0, sizeof(program->attributes));
#endif
						program->force_recompile = false;
						program->standard_options = R_ProgramStandardOptions(program->standard_option_flags);

						GL_TraceObjectLabelSet(GL_PROGRAM, program->program, -1, program->friendly_name);
						return true;
					}
					else {
						Con_Printf("ShaderProgram.Link(%s) failed\n", program->friendly_name);
						GL_ConPrintProgramLog(program_handle);
					}
				}
			}
			else {
				Con_Printf("FragmentShader.Compile(%s) failed\n", program->friendly_name);
			}
		}
		else {
			Con_Printf("GeometryShader.Compile(%s) failed\n", program->friendly_name);
		}
	}
	else {
		Con_Printf("VertexShader.Compile(%s) failed\n", program->friendly_name);
	}

	if (program_handle) {
		GL_Procedure(glDeleteProgram, program_handle);
	}
	for (i = 0; i < sizeof(shaders) / sizeof(shaders[0]); ++i) {
		if (shaders[i]) {
			GL_Procedure(glDeleteShader, shaders[i]);
		}
	}
	return false;
}

static void GL_CleanupShader(GLuint program, GLuint shader)
{
	if (shader) {
		if (program) {
			GL_Procedure(glDetachShader, program, shader);
		}
		GL_Procedure(glDeleteShader, shader);
	}
}

// Called during vid_shutdown
void GL_ProgramsShutdown(qbool restarting)
{
	r_program_id p;
	gl_program_t* prog;
	int sub_program;

	R_ProgramUse(r_program_none);

	// Detach & delete shaders
	for (p = r_program_none; p < r_program_count; ++p) {
		for (sub_program = 0; sub_program < MAX_SUBPROGRAMS; ++sub_program) {
			int i;

			prog = R_SpecificSubProgram(p, sub_program);
			for (i = 0; i < sizeof(prog->shaders) / sizeof(prog->shaders[0]); ++i) {
				GL_CleanupShader(prog->program, prog->shader_handles[i]);
				prog->shader_handles[i] = 0;
			}
		}
	}

	for (p = r_program_none; p < r_program_count; ++p) {
		for (sub_program = 0; sub_program < MAX_SUBPROGRAMS; ++sub_program) {
			prog = R_SpecificSubProgram(p, sub_program);
			if (prog->program) {
				GL_Procedure(glDeleteProgram, prog->program);
				prog->program = 0;
			}

			if (!restarting) {
				// Keep definitions if we're about to recompile after restart
				Q_free(prog->included_definitions);
			}

			memset(prog->uniforms, 0, sizeof(prog->uniforms));
#ifdef RENDERER_OPTION_CLASSIC_OPENGL
			memset(prog->attributes, 0, sizeof(prog->attributes));
#endif
		}
	}

	memset(program_currentSubProgram, 0, sizeof(program_currentSubProgram));
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
	int i;

	if (!(glConfig.supported_features & R_SUPPORT_RENDERING_SHADERS)) {
		return;
	}

	GL_BuildCoreDefinitions();

	for (i = 0; i < MAX_SUBPROGRAMS; ++i) {
		gl_program_t* prog = R_SpecificSubProgram(r_program_none, i);

		strlcpy(prog->friendly_name, "(none)", sizeof(prog->friendly_name));
	}

	for (p = r_program_none; p < r_program_count; ++p) {
		for (i = 0; i < MAX_SUBPROGRAMS; ++i) {
			gl_program_t* prog = R_SpecificSubProgram(p, i);

			if (!GL_AppropriateRenderer(prog->renderer_id)) {
				continue;
			}

			if (!prog->program && !prog->needs_params && prog->initialised) {
				gl_shader_def_t* compute = &prog->shaders[shadertype_compute];
				if (compute->length) {
					GL_CompileComputeShaderProgram(prog, compute->text, compute->length);
				}
				else {
					GL_CompileProgram(prog);
				}
				R_ProgramFindUniformsForProgram(p);
				R_ProgramFindAttributesForProgram(p);
			}
		}
	}

	memset(program_currentSubProgram, 0, sizeof(program_currentSubProgram));
}

qbool R_ProgramRecompileNeeded(r_program_id program_id, unsigned int options)
{
	//
	const gl_program_t* program = R_CurrentSubProgram(program_id);
	unsigned int standard_options = R_ProgramStandardOptions(program->standard_option_flags);

	return (!program->program) || program->force_recompile || program->custom_options != options || program->standard_options != standard_options;
}

void GL_CvarForceRecompile(cvar_t* cvar)
{
	r_program_id p;
	int i;

	for (p = r_program_none; p < r_program_count; ++p) {
		for (i = 0; i < MAX_SUBPROGRAMS; ++i) {
			R_SpecificSubProgram(p, i)->force_recompile = true;
		}
	}

	GL_BuildCoreDefinitions();
}

static qbool GL_CompileComputeShaderProgram(gl_program_t* program, const char* shadertext, unsigned int length)
{
	const char* shader_text[MAX_SHADER_COMPONENTS] = { shadertext, "", "", "", "", "" };
	GLint shader_text_length[MAX_SHADER_COMPONENTS] = { length, 0, 0, 0, 0, 0 };
	int components;
	GLuint shader;
	int standard_options = R_ProgramStandardOptions(program->standard_option_flags);

	program->program = 0;

	components = GL_InsertDefinitions(shader_text, shader_text_length, "");
	if (GL_CompileShader(components, shader_text, shader_text_length, GL_COMPUTE_SHADER, &shader)) {
		GLuint shader_program = GL_FunctionNoArgs(glCreateProgram);
		if (shader_program) {
			GLint result;

			GL_Procedure(glAttachShader, shader_program, shader);
			GL_Procedure(glLinkProgram, shader_program);
			GL_Procedure(glGetProgramiv, shader_program, GL_LINK_STATUS, &result);

			if (result) {
				Con_DPrintf("ShaderProgram.Link() was successful\n");
				program->shader_handles[shadertype_compute] = shader;
				program->program = shader_program;
				memset(program->uniforms, 0, sizeof(program->uniforms));
#ifdef RENDERER_OPTION_CLASSIC_OPENGL
				memset(program->attributes, 0, sizeof(program->attributes));
#endif
				program->force_recompile = false;
				program->standard_options = standard_options;

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

#ifdef RENDERER_OPTION_CLASSIC_OPENGL
		GL_LoadMandatoryFunctionExtension(glGetAttribLocation, rendering_shaders_support);
#endif

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
	if (program_id != currentProgram || currentProgramOptionSet != program_currentSubProgram[program_id]) {
		if (GL_Available(glUseProgram)) {
			R_TraceLogAPICall("R_ProgramUse(%s[%d])", R_CurrentSubProgram(program_id)->friendly_name, program_currentSubProgram[program_id]);
			GL_Procedure(glUseProgram, R_CurrentSubProgram(program_id)->program);
		}

		currentProgram = program_id;
		currentProgramOptionSet = program_currentSubProgram[program_id];
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
	GL_Procedure(glDispatchCompute, num_groups_x, num_groups_y, num_groups_z);
}

static void GL_ProgramMemoryBarrier(r_program_id program_id)
{
	if (R_CurrentSubProgram(program_id)->memory_barrier) {
		GL_Procedure(glMemoryBarrier, R_CurrentSubProgram(program_id)->memory_barrier);
	}
}

void R_ProgramMemoryBarrier(r_program_id program_id)
{
	GL_ProgramMemoryBarrier(program_id);
}

void R_ProgramComputeDispatch(r_program_id program_id, unsigned int num_groups_x, unsigned int num_groups_y, unsigned int num_groups_z)
{
	GL_ProgramComputeDispatch(program_id, num_groups_x, num_groups_y, num_groups_z);
}

void R_ProgramComputeSetMemoryBarrierFlag(r_program_id program_id, r_program_memory_barrier_id barrier_id)
{
	R_CurrentSubProgram(program_id)->memory_barrier |= glBarrierFlags[barrier_id];
}

// Wrappers
int R_ProgramCustomOptions(r_program_id program_id)
{
	return R_CurrentSubProgram(program_id)->custom_options;
}

void R_ProgramSetCustomOptions(r_program_id program_id, int options)
{
	R_CurrentSubProgram(program_id)->custom_options = options;
}

qbool R_ProgramReady(r_program_id program_id)
{
	return R_CurrentSubProgram(program_id)->program != 0;
}

void R_ProgramUniform1i(r_program_uniform_id uniform_id, int value)
{
	r_program_uniform_t* base_uniform = &program_uniforms[uniform_id];
	gl_program_uniform_t* program_uniform = GL_ProgramUniformFind(uniform_id);
	gl_program_t* prog = R_CurrentSubProgram(base_uniform->program_id);

	if (program_uniform->location >= 0) {
		R_TraceLogAPICall("%s(%s/%s,%d)", __func__, prog->friendly_name, base_uniform->name, value);
		if (GL_Available(glProgramUniform1i)) {
			GL_Procedure(glProgramUniform1i, prog->program, program_uniform->location, value);
		}
		else {
			R_ProgramUse(base_uniform->program_id);
			GL_Procedure(glUniform1i, program_uniform->location, value);
		}
		program_uniform->int_value = value;
	}
}

void R_ProgramUniform1f(r_program_uniform_id uniform_id, float value)
{
	r_program_uniform_t* base_uniform = &program_uniforms[uniform_id];
	gl_program_uniform_t* program_uniform = GL_ProgramUniformFind(uniform_id);
	gl_program_t* prog = R_CurrentSubProgram(base_uniform->program_id);

	if (program_uniform->location >= 0) {
		R_TraceLogAPICall("%s(%s/%s,%f)", __func__, prog->friendly_name, base_uniform->name, value);
		if (GL_Available(glProgramUniform1f)) {
			GL_Procedure(glProgramUniform1f, prog->program, program_uniform->location, value);
		}
		else {
			R_ProgramUse(base_uniform->program_id);
			GL_Procedure(glUniform1f, program_uniform->location, value);
		}
		program_uniform->int_value = value;
	}
}

void R_ProgramUniform2fv(r_program_uniform_id uniform_id, const float* values)
{
	r_program_uniform_t* base_uniform = &program_uniforms[uniform_id];
	gl_program_uniform_t* program_uniform = GL_ProgramUniformFind(uniform_id);
	gl_program_t* prog = R_CurrentSubProgram(base_uniform->program_id);

	if (program_uniform->location >= 0) {
		R_TraceLogAPICall("%s(%s/%s)", __func__, prog->friendly_name, base_uniform->name);
		if (GL_Available(glProgramUniform2fv)) {
			GL_Procedure(glProgramUniform2fv, prog->program, program_uniform->location, base_uniform->count, values);
		}
		else {
			R_ProgramUse(base_uniform->program_id);
			GL_Procedure(glUniform2fv, program_uniform->location, base_uniform->count, values);
		}
	}
}

void R_ProgramUniform3f(r_program_uniform_id uniform_id, float x, float y, float z)
{
	float vec[3] = { x, y, z };

	R_ProgramUniform3fv(uniform_id, vec);
}

static void R_ProgramStandardUniform3fv(r_program_id program_id, r_program_std_uniform_id id, const float* values)
{
	gl_program_t* program = R_CurrentSubProgram(program_id);
	r_program_uniform_t* std_uniform = &program_std_uniforms[id];
	gl_program_uniform_t* program_uniform = &program->std_uniforms[id];

	if (program->program && !program_uniform->found) {
		program_uniform->location = GL_Function(glGetUniformLocation, program->program, std_uniform->name);
		program_uniform->found = true;
	}

	if (program_uniform->location >= 0) {
		R_TraceLogAPICall("%s(%s/%s)", __func__, program->friendly_name, std_uniform->name);
		if (GL_Available(glProgramUniform3fv)) {
			GL_Procedure(glProgramUniform3fv, program->program, program_uniform->location, std_uniform->count, values);
		}
		else {
			R_ProgramUse(program_id);
			GL_Procedure(glUniform3fv, program_uniform->location, std_uniform->count, values);
		}
	}
}

static void R_ProgramStandardUniform1f(r_program_id program_id, r_program_std_uniform_id id, float value)
{
	gl_program_t* program = R_CurrentSubProgram(program_id);
	r_program_uniform_t* std_uniform = &program_std_uniforms[id];
	gl_program_uniform_t* program_uniform = &program->std_uniforms[id];

	if (program->program && !program_uniform->found) {
		program_uniform->location = GL_Function(glGetUniformLocation, program->program, std_uniform->name);
		program_uniform->found = true;
	}

	if (program_uniform->location >= 0) {
		R_TraceLogAPICall("%s(%s/%s)", __func__, program->friendly_name, std_uniform->name);
		if (GL_Available(glProgramUniform1f)) {
			GL_Procedure(glProgramUniform1f, program->program, program_uniform->location, value);
		}
		else {
			R_ProgramUse(program_id);
			GL_Procedure(glUniform1f, program_uniform->location, value);
		}
	}
}

void R_ProgramUniform3fv(r_program_uniform_id uniform_id, const float* values)
{
	r_program_uniform_t* base_uniform = &program_uniforms[uniform_id];
	gl_program_uniform_t* program_uniform = GL_ProgramUniformFind(uniform_id);
	gl_program_t* prog = R_CurrentSubProgram(base_uniform->program_id);

	if (program_uniform->location >= 0) {
		R_TraceLogAPICall("%s(%s/%s)", __func__, prog->friendly_name, base_uniform->name);
		if (GL_Available(glProgramUniform3fv)) {
			GL_Procedure(glProgramUniform3fv, prog->program, program_uniform->location, base_uniform->count, values);
		}
		else {
			R_ProgramUse(base_uniform->program_id);
			GL_Procedure(glUniform3fv, program_uniform->location, base_uniform->count, values);
		}
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
	r_program_uniform_t* base_uniform = &program_uniforms[uniform_id];
	gl_program_uniform_t* program_uniform = GL_ProgramUniformFind(uniform_id);
	gl_program_t* prog = R_CurrentSubProgram(base_uniform->program_id);

	if (program_uniform->location >= 0) {
		R_TraceLogAPICall("%s(%s/%s)", __func__, prog->friendly_name, base_uniform->name);
		if (GL_Available(glProgramUniform4fv)) {
			GL_Procedure(glProgramUniform4fv, prog->program, program_uniform->location, base_uniform->count, values);
		}
		else {
			R_ProgramUse(base_uniform->program_id);
			GL_Procedure(glUniform4fv, program_uniform->location, base_uniform->count, values);
		}
	}
}

void R_ProgramUniformMatrix4fv(r_program_uniform_id uniform_id, const float* values)
{
	r_program_uniform_t* base_uniform = &program_uniforms[uniform_id];
	gl_program_uniform_t* program_uniform = GL_ProgramUniformFind(uniform_id);
	gl_program_t* prog = R_CurrentSubProgram(base_uniform->program_id);

	if (program_uniform->location >= 0) {
		R_TraceLogAPICall("%s(%s/%s)", __func__, prog->friendly_name, base_uniform->name);
		if (GL_Available(glProgramUniformMatrix4fv)) {
			GL_Procedure(glProgramUniformMatrix4fv, prog->program, program_uniform->location, base_uniform->count, base_uniform->transpose, values);
		}
		else {
			R_ProgramUse(base_uniform->program_id);
			GL_Procedure(glUniformMatrix4fv, program_uniform->location, base_uniform->count, base_uniform->transpose, values);
		}
	}
}

int R_ProgramUniformGet1i(r_program_uniform_id uniform_id, int default_value)
{
	gl_program_uniform_t* program_uniform = GL_ProgramUniformFind(uniform_id);

	if (program_uniform->location >= 0) {
		return program_uniform->int_value;
	}
	return default_value;
}

qbool R_ProgramCompile(r_program_id program_id)
{
	return R_ProgramCompileWithInclude(program_id, NULL);
}

qbool R_ProgramCompileWithInclude(r_program_id program_id, const char* included_definitions)
{
	gl_program_t* program = R_CurrentSubProgram(program_id);

	memset(program->shader_handles, 0, sizeof(program->shader_handles));
	Q_free(program->included_definitions);
	program->included_definitions = included_definitions ? Q_strdup(included_definitions) : NULL;

	if (GL_CompileProgram(program)) {
		if (program_id == currentProgram && GL_Available(glUseProgram)) {
			GL_Procedure(glUseProgram, R_CurrentSubProgram(program_id)->program);
		}
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
	strlcpy(core_definitions, (R_UseModernOpenGL() ? "#define EZ_MODERN_GL\n" : "#define EZ_LEGACY_GL\n"), sizeof(core_definitions));
	strlcat(core_definitions, 
		"#ifdef DRAW_FOG\n"
			"#ifdef FOG_EXP\n"
			"#ifdef EZ_LEGACY_GL\n"
				"uniform float fogDensity;\n"
				"uniform vec3 fogColor;\n"
			"#endif\n"
				"vec4 applyFog(vec4 vecinput, float z) {\n"
				"	float fogmix = exp(-fogDensity * z);\n"
				"	fogmix = clamp(fogmix, 0.0, 1.0); \n"
				"	return vec4(mix(fogColor, vecinput.rgb, fogmix), 1) * vecinput.a; \n"
				"}\n"
				"vec4 applyFogBlend(vec4 vecinput, float z) {\n"
				"	float fogmix = exp(-fogDensity * z);\n"
				"	fogmix = clamp(fogmix, 0.0, 1.0); \n"
				"	return vecinput * vec4(1, 1, 1, fogmix);\n"
				"}\n"
			"#elif defined(FOG_EXP2)\n"
				"const float LOG2 = 1.442695;\n"
			"#ifdef EZ_LEGACY_GL\n"
				"uniform float fogDensity;\n"
				"uniform vec3 fogColor;\n"
			"#endif"
				"\n"
				"vec4 applyFog(vec4 vecinput, float z) {\n"
				"	float fogmix = exp2(-fogDensity * z * z * LOG2);\n"
				"	fogmix = clamp(fogmix, 0.0, 1.0);\n"
				"	return vec4(mix(fogColor, vecinput.rgb, fogmix), 1) * vecinput.a;\n"
				"}\n"
				"vec4 applyFogBlend(vec4 vecinput, float z) {\n"
				"	float fogmix = exp2(-fogDensity * z * z * LOG2);\n"
				"	fogmix = clamp(fogmix, 0.0, 1.0);\n"
				"	return vecinput * vec4(1, 1, 1, fogmix);\n"
				"}\n"
			"#elif defined(FOG_LINEAR)\n"
			"#ifdef EZ_LEGACY_GL\n"
				"uniform float fogMinZ; \n"
				"uniform float fogMaxZ; \n"
				"uniform vec3 fogColor; \n"
			"#endif"
				"\n"
				"vec4 applyFog(vec4 vecinput, float z) {\n"
					"float fogmix = (fogMaxZ - z) / (fogMaxZ - fogMinZ); \n"
					"fogmix = clamp(fogmix, 0.0, 1.0); \n"
					"return vec4(mix(fogColor, vecinput.rgb / vecinput.a, fogmix), 1) * vecinput.a; \n"
				"}\n"
				"vec4 applyFogBlend(vec4 vecinput, float z) {\n"
					"float fogmix = (fogMaxZ - z) / (fogMaxZ - fogMinZ); \n"
					"fogmix = clamp(fogmix, 0.0, 1.0); \n"
					"return vecinput * vec4(1, 1, 1, fogmix);\n"
				"}\n"
			"#else\n"
				"vec4 applyFog(vec4 vecinput, float z) {\n"
				"	return vecinput; \n"
				"}\n"
				"vec4 applyFogBlend(vec4 vecinput, float z) {\n"
				"	return vecinput; \n"
				"}\n"
			"#endif\n"
		"#endif // DRAW_FOG\n", sizeof(core_definitions)
	);

#ifdef RENDERER_OPTION_MODERN_OPENGL
	GL_DefineProgram_VF(r_program_aliasmodel, "aliasmodel", true, draw_aliasmodel, renderer_modern, GLM_CompileAliasModelProgram, STDOPTIONS_FOG);
	GL_DefineProgram_VF(r_program_brushmodel, "brushmodel", true, draw_world, renderer_modern, GLM_CompileDrawWorldProgram, STDOPTIONS_FOG);
	GL_DefineProgram_VF(r_program_brushmodel_alphatested, "brushmodel-alphatested", true, draw_world, renderer_modern, GLM_CompileDrawWorldProgramAlphaTested, STDOPTIONS_FOG);
	GL_DefineProgram_VF(r_program_sprite3d, "3d-sprites", false, draw_sprites, renderer_modern, GLM_Compile3DSpriteProgram, STDOPTIONS_FOG);
	GL_DefineProgram_VF(r_program_hud_images, "image-draw", true, hud_draw_image, renderer_modern, GLM_CreateMultiImageProgram, STDOPTIONS_NONE);
	GL_DefineProgram_VF(r_program_hud_circles, "circle-draw", false, hud_draw_circle, renderer_modern, GLM_CompileHudCircleProgram, STDOPTIONS_NONE);
	GL_DefineProgram_VF(r_program_post_process, "post-process-screen", true, post_process_screen, renderer_modern, GLM_CompilePostProcessProgram, STDOPTIONS_NONE);
	GL_DefineProgram_CS(r_program_lightmap_compute, "lightmaps", false, lighting, renderer_modern, GLM_CompileLightmapComputeProgram, STDOPTIONS_NONE);
	GL_DefineProgram_VF(r_program_fx_world_geometry, "world-geometry", true, fx_world_geometry, renderer_modern, GLM_CompileWorldGeometryProgram, STDOPTIONS_NONE);
	GL_DefineProgram_VF(r_program_simple, "simple", false, simple, renderer_modern, GLM_CompileSimpleProgram, STDOPTIONS_NONE);
	GL_DefineProgram_VF(r_program_simple3d, "simple3d", false, simple3d, renderer_modern, GLM_CompileSimple3dProgram, STDOPTIONS_NONE);
#endif

#ifdef RENDERER_OPTION_CLASSIC_OPENGL
	GL_DefineProgram_VF(r_program_post_process_glc, "post-process-screen", true, glc_post_process_screen, renderer_classic, GLC_CompilePostProcessProgram, STDOPTIONS_NONE);
	GL_DefineProgram_VF(r_program_sky_glc, "sky-rendering", true, glc_sky, renderer_classic, GLC_SkyProgramCompile, STDOPTIONS_FOG);
	GL_DefineProgram_VF(r_program_turb_glc, "turb-rendering", true, glc_turbsurface, renderer_classic, GLC_TurbSurfaceProgramCompile, STDOPTIONS_FOG);
	GL_DefineProgram_VF(r_program_caustics_glc, "caustics-rendering", true, glc_caustics, renderer_classic, GLC_CausticsProgramCompile, STDOPTIONS_FOG);
	GL_DefineProgram_VF(r_program_aliasmodel_std_glc, "aliasmodel-std", true, glc_aliasmodel_std, renderer_classic, GLC_AliasModelStandardCompile, STDOPTIONS_FOG);
	GL_DefineProgram_VF(r_program_aliasmodel_shell_glc, "aliasmodel-shell", true, glc_aliasmodel_shell, renderer_classic, GLC_AliasModelShellCompile, STDOPTIONS_FOG);
	GL_DefineProgram_VF(r_program_aliasmodel_shadow_glc, "aliasmodel-shadow", true, glc_aliasmodel_shadow, renderer_classic, GLC_AliasModelShadowCompile, STDOPTIONS_FOG);
	GL_DefineProgram_VF(r_program_aliasmodel_outline_glc, "aliasmodel-outline", true, glc_aliasmodel_std, renderer_classic, GLC_AliasModelOutlineCompile, STDOPTIONS_FOG);
	GL_DefineProgram_VF(r_program_world_drawflat_glc, "drawflat-world", true, glc_world_drawflat, renderer_classic, GLC_DrawflatProgramCompile, STDOPTIONS_FOG);
	GL_DefineProgram_VF(r_program_world_textured_glc, "textured-world", true, glc_world_textured, renderer_classic, GLC_PreCompileWorldPrograms, STDOPTIONS_FOG);
	GL_DefineProgram_VF(r_program_world_secondpass_glc, "secondpass-world", true, glc_world_secondpass, renderer_classic, GLC_PreCompileWorldPrograms, STDOPTIONS_FOG);
	GL_DefineProgram_VF(r_program_sprites_glc, "3d-sprites", true, glc_draw_sprites, renderer_classic, GLC_CompileSpriteProgram, STDOPTIONS_FOG);
	GL_DefineProgram_VF(r_program_hud_images_glc, "hud-images", true, glc_hud_images, renderer_classic, GLC_ProgramHudImagesCompile, STDOPTIONS_NONE);
#endif
}

#ifdef RENDERER_OPTION_CLASSIC_OPENGL
int R_ProgramAttributeLocation(r_program_attribute_id attr_id)
{
	r_program_id program_id;
	gl_program_attribute_t* attr;

	// FIXME: why is this is in release build?
	if (attr_id < 0 || attr_id >= r_program_attribute_count) {
		return -1;
	}

	program_id = program_attributes[attr_id].program_id;
	attr = &(R_CurrentSubProgram(program_id)->attributes[attr_id]);
	if (!attr->found) {
		return -1;
	}

	return attr->location;
}

r_program_id R_ProgramForAttribute(r_program_attribute_id attr_id)
{
	if (attr_id < 0 || attr_id >= r_program_attribute_count) {
		return r_program_none;
	}

	return program_attributes[attr_id].program_id;
}

static r_program_attribute_t* GL_ProgramAttributeFind(r_program_attribute_id attribute_id)
{
	r_program_attribute_t* attribute = &program_attributes[attribute_id];
	r_program_id program_id = attribute->program_id;
	gl_program_t* program = R_CurrentSubProgram(program_id);
	gl_program_attribute_t* program_attr = &program->attributes[attribute_id];

	if (program->program && !program_attr->found) {
		program_attr->location = GL_Function(glGetAttribLocation, program->program, attribute->name);
		program_attr->found = true;
	}

	return attribute;
}

static void R_ProgramFindAttributesForProgram(r_program_id program_id)
{
	r_program_attribute_id a;

	for (a = 0; a < r_program_attribute_count; ++a) {
		if (program_attributes[a].program_id == program_id) {
			R_CurrentSubProgram(program_id)->attributes[a].found = false;
			GL_ProgramAttributeFind(a);
		}
	}
}
#endif

void R_ProgramCompileAll(void)
{
	int i;
	int sub_program;

	for (i = 0; i < r_program_count; ++i) {
		for (sub_program = 0; sub_program < MAX_SUBPROGRAMS; ++sub_program) {
			gl_program_t* prog = R_SpecificSubProgram(i, sub_program);

			R_ProgramSetSubProgram(i, sub_program);
			if (!GL_AppropriateRenderer(prog->renderer_id)) {
				continue;
			}

			if (prog->initialised && prog->compile_func) {
				prog->compile_func();
			}
		}
		R_ProgramSetSubProgram(i, 0);
	}
}

void R_ProgramSetSubProgram(r_program_id program_id, int sub_index)
{
	program_currentSubProgram[program_id] = sub_index;
}

void R_ProgramSetStandardUniforms(r_program_id program_id)
{
	gl_program_t* program = R_CurrentSubProgram(program_id);

	if (program->standard_options & STDOPTIONS_FOG) {
		if (r_refdef2.fog_enabled) {
			R_ProgramStandardUniform3fv(program_id, r_program_std_uniform_fog_color, r_refdef2.fog_color);

			if (r_refdef2.fog_calculation == fogcalc_linear) {
				R_ProgramStandardUniform1f(program_id, r_program_std_uniform_fog_linear_start, r_refdef2.fog_linear_start);
				R_ProgramStandardUniform1f(program_id, r_program_std_uniform_fog_linear_end, r_refdef2.fog_linear_end);
			}
			else {
				R_ProgramStandardUniform1f(program_id, r_program_std_uniform_fog_density, r_refdef2.fog_density);
			}
		}
	}
}
