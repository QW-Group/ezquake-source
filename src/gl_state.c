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

// gl_state.c
// State caching for OpenGL

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "tr_types.h"
#include "r_state.h"
#include "r_texture.h"
#include "r_vao.h"
#include "glm_vao.h"
#include "glc_vao.h"
#include "vk_vao.h"
#include "r_matrix.h"
#include "r_buffers.h"
#include "glm_local.h"
#include "gl_texture_internal.h"
#include "r_renderer.h"
#include "r_program.h"
#include "glc_state.h"

static void GLC_ToggleAlphaTesting(const rendering_state_t* state, rendering_state_t* current);
static rendering_state_t states[r_state_count];

// Texture functions
GL_StaticProcedureDeclaration(glBindTextures, "first=%u, count=%d, format=%p", GLuint first, GLsizei count, const GLuint* format)
GL_StaticProcedureDeclaration(glBindImageTexture, "unit=%u, texture=%u, level=%d, layered=%d, layer=%d, access=%u, format=%u", GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format)
GL_StaticProcedureDeclaration(glActiveTexture, "target=%u", GLenum target)

#ifdef RENDERER_OPTION_CLASSIC_OPENGL
GL_StaticProcedureDeclaration(glMultiTexCoord2f, "target=%u, s=%f, t=%f", GLenum target, GLfloat s, GLfloat t)
GL_StaticProcedureDeclaration(glClientActiveTexture, "target=%u", GLenum target)
#endif

#ifdef WITH_RENDERING_TRACE
static const char* gl_friendlyTextureTargetNames[] = {
	"GL_TEXTURE_2D",              // texture_type_2d,
	"GL_TEXTURE_2D_ARRAY",        // texture_type_2d_array,
	"GL_TEXTURE_CUBE_MAP",        // texture_type_cubemap,
	"GL_TEXTURE_2D_MULTISAMPLE"   // texture_type_2d_multisampled,
};

static const char* fogModeDescriptions[] = {
	"disabled",
	"enabled"
};
#ifdef C_ASSERT
C_ASSERT(sizeof(fogModeDescriptions) / sizeof(fogModeDescriptions[0]) == r_fogmode_count);
#endif
#endif

void R_InitialiseStates(void);

// VAOs
static r_vao_id currentVAO = vao_none;
static const char* vaoNames[] = {
	"none",
	"aliasmodel",
	"brushmodel",
	"3d-sprites",
	"hud:circles",
	"hud:images",
	"hud:lines",
	"hud:polygons",
	"post-process",
	"powerupshell",
	"brushmodel_details",
	"brushmodel_lightmap_pass",
	"brushmodel_lm_unit1",
	"brushmodel_simpletex",
	"vao_hud_images_non_glsl"
};

#ifdef C_ASSERT
C_ASSERT(sizeof(vaoNames) / sizeof(vaoNames[0]) == vao_count);
#endif

#define MAX_LOGGED_TEXTURE_UNITS 32
#define MAX_LOGGED_IMAGE_UNITS   32

typedef struct image_unit_binding_s {
	GLuint texture;
	GLint level;
	GLboolean layered;
	GLint layer;
	GLenum access;
	GLenum format;
} image_unit_binding_t;

static void GL_BindTexture(GLenum targetType, GLuint texnum, qbool warning);

/*typedef struct {
	GLenum currentTextureUnit;
	GLenum bound_texture_state[texture_type_count][MAX_LOGGED_TEXTURE_UNITS];
	image_unit_binding_t bound_images[MAX_LOGGED_IMAGE_UNITS];

	GLenum unit_texture_mode[texture_type_count][MAX_LOGGED_TEXTURE_UNITS]
	qbool texunitenabled[MAX_LOGGED_TEXTURE_UNITS];
} texture_state_t;
*/
typedef struct {
	rendering_state_t rendering_state;
	//texture_state_t textures;
} opengl_state_t;

static GLenum currentTextureUnit;
static GLuint bound_texture_state[texture_type_count][MAX_LOGGED_TEXTURE_UNITS];
static image_unit_binding_t bound_images[MAX_LOGGED_IMAGE_UNITS];

static opengl_state_t opengl;
static GLenum glDepthFunctions[] = {
	GL_LESS,   // r_depthfunc_less,
	GL_EQUAL,  // r_depthfunc_equal,
	GL_LEQUAL, // r_depthfunc_lessorequal,
};
static GLenum glReversedDepthFunctions[] = {
	GL_GREATER, // inv(r_depthfunc_less)
	GL_EQUAL,   // inv(r_depthfunc_equal)
	GL_GEQUAL   // inv(r_depthfunc_lessorequal)
};
static GLenum glCullFaceValues[] = {
	GL_FRONT,  // r_cullface_front
	GL_BACK,   // r_cullface_back
};
static GLenum glBlendFuncValuesSource[] = {
	GL_ONE, // r_blendfunc_overwrite,
	GL_ONE, // r_blendfunc_additive_blending,
	GL_ONE, // r_blendfunc_premultiplied_alpha,
	GL_DST_COLOR, // r_blendfunc_src_dst_color_dest_zero,
	GL_DST_COLOR, // r_blendfunc_src_dst_color_dest_one,
	GL_DST_COLOR, // r_blendfunc_src_dst_color_dest_src_color,
	GL_ZERO, // r_blendfunc_src_zero_dest_one_minus_src_color,
	GL_ZERO, // r_blendfunc_src_zero_dest_src_color,
	GL_ONE, // r_blendfunc_src_one_dest_zero,
	GL_ZERO, // r_blendfunc_src_zero_dest_one,
	GL_ONE, // r_blendfunc_src_one_dest_one_minus_src_color,
};
static GLenum glBlendFuncValuesDestination[] = {
	GL_ZERO, // r_blendfunc_overwrite,
	GL_ONE, // r_blendfunc_additive_blending,
	GL_ONE_MINUS_SRC_ALPHA, // r_blendfunc_premultiplied_alpha,
	GL_ZERO, // r_blendfunc_src_dst_color_dest_zero,
	GL_ONE, // r_blendfunc_src_dst_color_dest_one,
	GL_SRC_COLOR, // r_blendfunc_src_dst_color_dest_src_color,
	GL_ONE_MINUS_SRC_COLOR, // r_blendfunc_src_zero_dest_one_minus_src_color,
	GL_SRC_COLOR, // r_blendfunc_src_zero_dest_src_color,
	GL_ZERO, // r_blendfunc_src_one_dest_zero,
	GL_ONE, // r_blendfunc_src_zero_dest_one,
	GL_ONE_MINUS_SRC_COLOR, // r_blendfunc_src_one_dest_one_minus_src_color,
};
static GLenum glPolygonModeValues[] = {
	GL_FILL, // r_polygonmode_fill,
	GL_LINE, // r_polygonmode_line,
};
static GLenum glAlphaTestModeValues[] = {
	GL_ALWAYS,  // r_alphatest_func_always,
	GL_GREATER, // r_alphatest_func_greater,
};
static GLenum glTextureEnvModeValues[] = {
	GL_BLEND, GL_REPLACE, GL_MODULATE, GL_DECAL, GL_ADD
};

#ifdef C_ASSERT
C_ASSERT(sizeof(glBlendFuncValuesSource) / sizeof(glBlendFuncValuesSource[0]) == r_blendfunc_count);
C_ASSERT(sizeof(glBlendFuncValuesDestination) / sizeof(glBlendFuncValuesDestination[0]) == r_blendfunc_count);
C_ASSERT(sizeof(glTextureEnvModeValues) / sizeof(glTextureEnvModeValues[0]) == r_texunit_mode_count);
C_ASSERT(sizeof(glDepthFunctions) / sizeof(glDepthFunctions[0]) == r_depthfunc_count);
C_ASSERT(sizeof(glReversedDepthFunctions) / sizeof(glReversedDepthFunctions[0]) == r_depthfunc_count);
C_ASSERT(sizeof(glCullFaceValues) / sizeof(glCullFaceValues[0]) == r_cullface_count);
C_ASSERT(sizeof(glPolygonModeValues) / sizeof(glPolygonModeValues[0]) == r_polygonmode_count);
C_ASSERT(sizeof(glAlphaTestModeValues) / sizeof(glAlphaTestModeValues[0]) == r_alphatest_func_count);
#endif

#ifdef WITH_RENDERING_TRACE
static const char* txtDepthFunctions[] = {
	"<", // r_depthfunc_less,
	"=", // r_depthfunc_equal,
	"<=", // r_depthfunc_lessorequal,
};
static const char* txtReversedDepthFunctions[] = {
	">", // r_depthfunc_less,
	"=", // r_depthfunc_equal,
	">=", // r_depthfunc_lessorequal,
};
static const char* txtCullFaceValues[] = {
	"front", // r_cullface_front,
	"back", // r_cullface_back,
};
static const char* txtBlendFuncNames[] = {
	"overwrite", // r_blendfunc_overwrite,
	"additive",        // r_blendfunc_additive_blending,
	"premul-alpha",    // r_blendfunc_premultiplied_alpha,
	"src_dst_color_dest_zero", // r_blendfunc_src_dst_color_dest_zero
	"src_dst_color_dest_one", // r_blendfunc_src_dst_color_dest_one
	"src_dst_color_dest_src_color", // r_blendfunc_src_dst_color_dest_src_color
	"src_zero_dest_one_minus_src_color", // r_blendfunc_src_zero_dest_one_minus_src_color
	"src_zero_dest_src_color", // r_blendfunc_src_zero_dest_src_color
	"src_one_dest_zero", // r_blendfunc_src_one_dest_zero
	"src_zero_dest_one", // r_blendfunc_src_zero_dest_one
	"src_one_dest_one_minus_src_color", // "r_blendfunc_src_one_dest_one_minus_src_color"
};
static const char* txtPolygonModeValues[] = {
	"fill", // r_polygonmode_fill,
	"line", // r_polygonmode_line,
};
static const char* txtAlphaTestModeValues[] = {
	"always", // r_alphatest_func_always,
	"greater", // r_alphatest_func_greater,
};
static const char* txtTextureEnvModeValues[] = {
	"GL_BLEND", "GL_REPLACE", "GL_MODULATE", "GL_DECAL", "GL_ADD"
};

#ifdef C_ASSERT
C_ASSERT(sizeof(txtDepthFunctions) / sizeof(txtDepthFunctions[0]) == r_depthfunc_count);
C_ASSERT(sizeof(txtReversedDepthFunctions) / sizeof(txtReversedDepthFunctions[0]) == r_depthfunc_count);
C_ASSERT(sizeof(txtCullFaceValues) / sizeof(txtCullFaceValues[0]) == r_cullface_count);
C_ASSERT(sizeof(txtBlendFuncNames) / sizeof(txtCullFaceValues[0]) == r_blendfunc_count);
C_ASSERT(sizeof(txtPolygonModeValues) / sizeof(txtPolygonModeValues[0]) == r_polygonmode_count);
C_ASSERT(sizeof(txtAlphaTestModeValues) / sizeof(txtAlphaTestModeValues[0]) == r_alphatest_func_count);
C_ASSERT(sizeof(txtTextureEnvModeValues) / sizeof(txtTextureEnvModeValues[0]) == r_texunit_mode_count);
#endif
#endif

rendering_state_t* R_InitRenderingState(r_state_id id, qbool default_state, const char* name, r_vao_id vao)
{
	rendering_state_t* state = &states[id];
	SDL_Window* window = SDL_GL_GetCurrentWindow();

	strlcpy(state->name, name, sizeof(state->name));

	state->depth.func = r_depthfunc_less;
	state->depth.nearRange = 0;
	state->depth.farRange = 1;
	state->depth.test_enabled = false;
	state->depth.mask_enabled = false;

	state->blendFunc = r_blendfunc_overwrite;
	state->blendingEnabled = false;
	R_GLC_ConfigureAlphaTesting(state, false, r_alphatest_func_always, 0);

	state->currentViewportX = 0;
	state->currentViewportY = 0;
	SDL_GL_GetDrawableSize(window, &state->currentViewportWidth, &state->currentViewportHeight);

	state->cullface.enabled = false;
	state->cullface.mode = r_cullface_back;

	state->framebuffer_srgb = false;
	state->line.smooth = false;
	state->line.width = 1.0f;
	state->fog.mode = r_fogmode_disabled;
	state->polygonOffset.option = r_polygonoffset_disabled;
	state->polygonOffset.factor = 0;
	state->polygonOffset.units = 0;
	state->polygonOffset.fillEnabled = false;
	state->polygonOffset.lineEnabled = false;
	state->polygonMode = r_polygonmode_fill;
	state->clearColor[0] = state->clearColor[1] = state->clearColor[2] = state->clearColor[3] = 0;
	state->color[0] = state->color[1] = state->color[2] = state->color[3] = 1;
	state->colorMask[0] = state->colorMask[1] = state->colorMask[2] = state->colorMask[3] = true;

#ifdef RENDERER_OPTION_CLASSIC_OPENGL
	{
		int i;

		memset(state->textureUnits, 0, sizeof(state->textureUnits));

		for (i = 0; i < sizeof(state->textureUnits) / sizeof(state->textureUnits[0]); ++i) {
			state->textureUnits[i].mode = r_texunit_mode_modulate;
			state->textureUnits[i].va.size = 4;
			state->textureUnits[i].va.type = GL_FLOAT;
		}
	}

	state->vertex_array.size = 4;
	state->vertex_array.type = GL_FLOAT;

	state->color_array.size = 4;
	state->color_array.type = GL_FLOAT;

	state->normal_array.type = GL_FLOAT;
#endif

	// Not applied each time, kept distinct
	state->pack_alignment = 4;

	state->vao_id = vao;

	if (default_state) {
		state->cullface.mode = r_cullface_front;
		state->cullface.enabled = true;

		state->depth.func = r_depthfunc_lessorequal;
		R_GLC_ConfigureAlphaTesting(state, false, r_alphatest_func_greater, 0.666f);
		state->blendingEnabled = false;
		state->depth.test_enabled = true;
		state->depth.mask_enabled = true;

		state->clearColor[0] = 0;
		state->clearColor[1] = 0;
		state->clearColor[2] = 0;
		state->clearColor[3] = 1;

		state->polygonMode = r_polygonmode_fill;
		state->blendFunc = r_blendfunc_premultiplied_alpha;
		R_GLC_TextureUnitSet(state, 0, false, r_texunit_mode_replace);
		state->framebuffer_srgb = true;

		state->pack_alignment = 1;
	}

	state->initialized = true;
	return state;
}

rendering_state_t* R_Init3DSpriteRenderingState(r_state_id id, const char* name)
{
	rendering_state_t* state = R_InitRenderingState(id, true, name, vao_3dsprites);

	state->fog.mode = r_fogmode_enabled;
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	state->cullface.enabled = false;
	R_GLC_ConfigureAlphaTesting(state, false, r_alphatest_func_greater, 0.333f);

	return state;
}

#define GL_ApplySimpleToggle(state, current, field, option) \
	if (state->field != current->field) { \
		if (state->field) { \
			glEnable(option); \
			R_TraceLogAPICall("glEnable(" # option ")"); \
		} \
		else { \
			glDisable(option); \
			R_TraceLogAPICall("glDisable(" # option ")"); \
		} \
		current->field = state->field; \
	}

void GL_ApplyRenderingState(r_state_id id)
{
	rendering_state_t* state = &states[id];
	extern cvar_t gl_brush_polygonoffset;
	rendering_state_t* current = &opengl.rendering_state;
	float zRange[2] = {
		glConfig.reversed_depth && false ? 1.0f - state->depth.nearRange : state->depth.nearRange,
		glConfig.reversed_depth && false ? 1.0f - state->depth.farRange : state->depth.farRange,
	};

	R_TraceEnterRegion(va("GL_ApplyRenderingState(%s)", state->name), true);

	if (state->depth.func != current->depth.func) {
		current->depth.func = state->depth.func;
		if (glConfig.reversed_depth) {
			glDepthFunc(glReversedDepthFunctions[current->depth.func]);
			R_TraceLogAPICall("glDepthFunc(reversed(%s)=%s)", txtDepthFunctions[current->depth.func], txtReversedDepthFunctions[current->depth.func]);
		}
		else {
			glDepthFunc(glDepthFunctions[current->depth.func]);
			R_TraceLogAPICall("glDepthFunc(%s)", txtDepthFunctions[current->depth.func]);
		}
	}
	if (zRange[0] != current->depth.nearRange || zRange[1] != current->depth.farRange) {
		glDepthRange(
			current->depth.nearRange = zRange[0],
			current->depth.farRange = zRange[1]
		);
		R_TraceLogAPICall("glDepthRange(%f,%f)", zRange[0], zRange[1]);
	}
	if (state->cullface.mode != current->cullface.mode) {
		glCullFace(glCullFaceValues[current->cullface.mode = state->cullface.mode]);
		R_TraceLogAPICall("glCullFace(%s)", txtCullFaceValues[state->cullface.mode]);
	}
	if (state->blendFunc != current->blendFunc) {
		current->blendFunc = state->blendFunc;
		glBlendFunc(
			glBlendFuncValuesSource[state->blendFunc],
			glBlendFuncValuesDestination[state->blendFunc]
		);
		R_TraceLogAPICall("glBlendFunc(%s)", txtBlendFuncNames[state->blendFunc]);
	}
	if (state->line.width != current->line.width) {
		glLineWidth(current->line.width = state->line.width);
		R_TraceLogAPICall("glLineWidth(%f)", current->line.width);
	}
	GL_ApplySimpleToggle(state, current, depth.test_enabled, GL_DEPTH_TEST);
	if (state->depth.mask_enabled != current->depth.mask_enabled) {
		glDepthMask((current->depth.mask_enabled = state->depth.mask_enabled) ? GL_TRUE : GL_FALSE);
		R_TraceLogAPICall("glDepthMask(%s)", current->depth.mask_enabled ? "on" : "off");
	}
	if (vid_gammacorrection.integer) {
		GL_ApplySimpleToggle(state, current, framebuffer_srgb, GL_FRAMEBUFFER_SRGB);
	}
	GL_ApplySimpleToggle(state, current, cullface.enabled, GL_CULL_FACE);
	GL_ApplySimpleToggle(state, current, line.smooth, GL_LINE_SMOOTH);

	if (state->polygonOffset.option != current->polygonOffset.option || gl_brush_polygonoffset.modified) {
		R_CustomPolygonOffset(state->polygonOffset.option);

		gl_brush_polygonoffset.modified = false;
	}
	if (state->polygonMode != current->polygonMode) {
		glPolygonMode(GL_FRONT_AND_BACK, glPolygonModeValues[current->polygonMode = state->polygonMode]);

		R_TraceLogAPICall("glPolygonMode(%s)", txtPolygonModeValues[state->polygonMode]);
	}
	if (state->clearColor[0] != current->clearColor[0] || state->clearColor[1] != current->clearColor[1] || state->clearColor[2] != current->clearColor[2] || state->clearColor[3] != current->clearColor[3]) {
		glClearColor(
			current->clearColor[0] = state->clearColor[0],
			current->clearColor[1] = state->clearColor[1],
			current->clearColor[2] = state->clearColor[2],
			current->clearColor[3] = state->clearColor[3]
		);
		R_TraceLogAPICall("glClearColor(...)");
	}
	GL_ApplySimpleToggle(state, current, blendingEnabled, GL_BLEND);
	if (state->colorMask[0] != current->colorMask[0] || state->colorMask[1] != current->colorMask[1] || state->colorMask[2] != current->colorMask[2] || state->colorMask[3] != current->colorMask[3]) {
		glColorMask(
			(current->colorMask[0] = state->colorMask[0]) ? GL_TRUE : GL_FALSE,
			(current->colorMask[1] = state->colorMask[1]) ? GL_TRUE : GL_FALSE,
			(current->colorMask[2] = state->colorMask[2]) ? GL_TRUE : GL_FALSE,
			(current->colorMask[3] = state->colorMask[3]) ? GL_TRUE : GL_FALSE
		);
		R_TraceLogAPICall("glColorMask(%s,%s,%s,%s)", state->colorMask[0] ? "on" : "off", state->colorMask[1] ? "on" : "off", state->colorMask[2] ? "on" : "off", state->colorMask[3] ? "on" : "off");
	}
	if (buffers.supported && (state->vao_id != currentVAO || current->glc_vao_force_rebind)) {
		R_BindVertexArray(state->vao_id);
	}

#ifdef WITH_RENDERING_TRACE
	R_TraceDebugState();
#endif
	R_TraceLeaveRegion(true);
}

// vid_common_gl.c
// gl_texture.c
GLuint GL_TextureNameFromReference(texture_ref ref);
GLenum GL_TextureTargetFromReference(texture_ref ref);

void GL_BindTextureToTarget(GLenum textureUnit, GLenum targetType, GLuint name)
{
	GL_SelectTexture(textureUnit);
	GL_BindTexture(targetType, name, true);
}

r_texture_type_id GL_TargetTypeToTextureType(GLuint targetType)
{
	if (targetType == GL_TEXTURE_2D_ARRAY) {
		return texture_type_2d_array;
	}
	else if (targetType == GL_TEXTURE_2D) {
		return texture_type_2d;
	}
	else if (targetType == GL_TEXTURE_CUBE_MAP) {
		return texture_type_cubemap;
	}
	else if (targetType == GL_TEXTURE_2D_MULTISAMPLE) {
		return texture_type_2d_multisampled;
	}
	else {
		Sys_Error("GL_TargetTypeToTextureType(%x) - unsupported", targetType);
		return 0;
	}
}

qbool GL_IsTextureBound(GLuint unit, texture_ref reference)
{
	int unit_num = unit - GL_TEXTURE0;
	GLuint texture = GL_TextureNameFromReference(reference);
	GLenum targetType = GL_TextureTargetFromReference(reference);

	if (unit_num >= 0 && unit_num < sizeof(bound_texture_state) / sizeof(bound_texture_state[0])) {
		return bound_texture_state[GL_TargetTypeToTextureType(targetType)][unit_num] == texture;
	}
	return false;
}

static qbool GL_BindTextureUnitImpl(GLuint unit, texture_ref reference, qbool always_select_unit)
{
	int unit_num = unit - GL_TEXTURE0;
	GLuint texture = GL_TextureNameFromReference(reference);
	GLenum targetType = GL_TextureTargetFromReference(reference);

	if (unit_num >= 0 && unit_num < sizeof(bound_texture_state) / sizeof(bound_texture_state[0])) {
		qbool bound = (bound_texture_state[GL_TargetTypeToTextureType(targetType)][unit_num] == texture);
		if (bound) {
			if (always_select_unit) {
				GL_SelectTexture(unit);
			}
			return false;
		}
	}

	GL_SelectTexture(unit);
	GL_BindTexture(targetType, texture, true);
	return true;
}

#ifdef RENDERER_OPTION_MODERN_OPENGL
void GLM_ApplyRenderingState(r_state_id id)
{
	GL_ApplyRenderingState(id);
}
#endif

qbool GL_EnsureTextureUnitBound(int unit, texture_ref reference)
{
	return GL_BindTextureUnitImpl(GL_TEXTURE0 + unit, reference, false);
}

qbool GL_EnsureTextureUnitBoundAndSelect(int unit, texture_ref reference)
{
	return GL_BindTextureUnitImpl(GL_TEXTURE0 + unit, reference, true);
}

void GL_BindTextureUnit(GLuint unit, texture_ref reference)
{
	GL_BindTextureUnitImpl(unit, reference, true);
}

void R_Viewport(int x, int y, int width, int height)
{
	rendering_state_t* state = &opengl.rendering_state;

	if (x != state->currentViewportX || y != state->currentViewportY || width != state->currentViewportWidth || height != state->currentViewportHeight) {
		renderer.Viewport(x, y, width, height);

		state->currentViewportX = x;
		state->currentViewportY = y;
		state->currentViewportWidth = width;
		state->currentViewportHeight = height;
	}
}

void R_GetViewport(int* view)
{
	rendering_state_t* state = &opengl.rendering_state;

	view[0] = state->currentViewportX;
	view[1] = state->currentViewportY;
	view[2] = state->currentViewportWidth;
	view[3] = state->currentViewportHeight;
}

void R_SetFullScreenViewport(int x, int y, int width, int height)
{
	rendering_state_t* state = &opengl.rendering_state;

	state->fullScreenViewportX = x;
	state->fullScreenViewportY = y;
	state->fullScreenViewportWidth = width;
	state->fullScreenViewportHeight = height;
}

void R_GetFullScreenViewport(int* viewport)
{
	rendering_state_t* state = &opengl.rendering_state;

	viewport[0] = state->fullScreenViewportX;
	viewport[1] = state->fullScreenViewportY;
	viewport[2] = state->fullScreenViewportWidth;
	viewport[3] = state->fullScreenViewportHeight;
}

void GL_InitialiseState(void)
{
	R_InitRenderingState(r_state_default_opengl, false, "opengl", vao_none);
	opengl.rendering_state = states[r_state_default_opengl];
	R_InitialiseStates();

	R_SetIdentityMatrix(R_ProjectionMatrix());
	R_SetIdentityMatrix(R_ModelviewMatrix());

	memset(bound_texture_state, 0, sizeof(bound_texture_state));
	memset(bound_images, 0, sizeof(bound_images));

	if (buffers.supported) {
		buffers.InitialiseState();
	}
	R_ProgramInitialiseState();
}

// These functions taken from gl_texture.c
static void GL_BindTexture(GLenum targetType, GLuint texnum, qbool warning)
{
	r_texture_type_id texture_type = GL_TargetTypeToTextureType(targetType);

	assert(targetType);
	assert(texnum);

	if (bound_texture_state[texture_type][currentTextureUnit - GL_TEXTURE0] == texnum) {
		return;
	}

	R_TraceLogAPICall("glBindTexture(unit=GL_TEXTURE%d, target=%s, texnum=%u[%s])", currentTextureUnit - GL_TEXTURE0, gl_friendlyTextureTargetNames[texture_type], texnum, GL_TextureIdentifierByGLReference(texnum));

	GL_BuiltinProcedure(glBindTexture, "target=%u, texnum=%u", targetType, texnum);
	bound_texture_state[texture_type][currentTextureUnit - GL_TEXTURE0] = texnum;

	++frameStats.texture_binds;
}

void GL_SelectTexture(GLenum textureUnit)
{
	if (textureUnit == currentTextureUnit) {
		return;
	}

	R_TraceLogAPICall("glActiveTexture(GL_TEXTURE%d)", textureUnit - GL_TEXTURE0);
	if (GL_Available(glActiveTexture)) {
		GL_Procedure(glActiveTexture, textureUnit);
	}

	currentTextureUnit = textureUnit;
}

void GL_TextureInitialiseState(void)
{
	// Multi texture.
	currentTextureUnit = GL_TEXTURE0;

	memset(bound_images, 0, sizeof(bound_images));
	memset(bound_texture_state, 0, sizeof(bound_texture_state));

#ifdef RENDERER_OPTION_CLASSIC_OPENGL
	{
		int i;
		for (i = 0; i < sizeof(opengl.rendering_state.textureUnits) / sizeof(opengl.rendering_state.textureUnits[0]); ++i) {
			opengl.rendering_state.textureUnits[i].enabled = false;
			opengl.rendering_state.textureUnits[i].mode = GL_MODULATE;
		}
	}
#endif
}

void GL_InvalidateTextureReferences(GLuint texture)
{
	int i, j;

	// glDeleteTextures(texture) has been called - same reference might be re-used in future
	// If a texture that is currently bound is deleted, the binding reverts to 0 (the default texture)
	for (i = 0; i < sizeof(bound_texture_state) / sizeof(bound_texture_state[0]); ++i) {
		for (j = 0; j < sizeof(bound_texture_state[0]) / sizeof(bound_texture_state[0][0]); ++j) {
			if (bound_texture_state[i][j] == texture) {
				bound_texture_state[i][j] = 0;
			}
		}
	}

	for (i = 0; i < sizeof(bound_images) / sizeof(bound_images[0]); ++i) {
		if (bound_images[i].texture == texture) {
			bound_images[i].texture = 0;
		}
	}
}

void GL_TextureUnitMultiBind(int first, int count, texture_ref* textures)
{
	GLuint glTextures[MAX_LOGGED_TEXTURE_UNITS] = { 0 };
	GLuint postbind_texture_state[texture_type_count][MAX_LOGGED_TEXTURE_UNITS];
	qbool already_bound[MAX_LOGGED_TEXTURE_UNITS] = { false };
	//r_texture_type_id types[MAX_LOGGED_TEXTURE_UNITS] = { 0 };
	qbool multi = false;
	int i, j, first_to_change = -1;

	if (first + count > MAX_LOGGED_TEXTURE_UNITS) {
		Sys_Error("GL_TextureUnitMultiBind(first=%d, count=%d)... not supported", first, count);
		return;
	}

	// assume no change
	memcpy(postbind_texture_state, bound_texture_state, sizeof(postbind_texture_state));

	// find out which texture units we need to change
	for (i = 0; i < count; ++i) {
		int unit = first + i;

		glTextures[i] = GL_TextureNameFromReference(textures[i]);
		already_bound[i] = true;

		if (glTextures[i] == 0) {
			// unbind from all targets...
			for (j = 0; j < sizeof(postbind_texture_state) / sizeof(postbind_texture_state[0]); ++j) {
				if (postbind_texture_state[j][unit]) {
					postbind_texture_state[j][unit] = 0;
					multi = (first_to_change >= 0);
					first_to_change = (first_to_change < 0 ? i : first_to_change);
					already_bound[i] = false;
				}
			}
		}
		else {
			GLenum target = GL_TextureTargetFromReference(textures[i]);
			r_texture_type_id type = GL_TargetTypeToTextureType(target);

			already_bound[i] = (bound_texture_state[type][unit] == glTextures[i]);
			if (!already_bound[i]) {
				first_to_change = (first_to_change < 0 ? i : first_to_change);
				multi = (first_to_change >= 0);
				postbind_texture_state[type][unit] = glTextures[i];
			}
		}
	}

	if (multi && GL_Available(glBindTextures)) {
		GL_Procedure(glBindTextures, first, count, glTextures);

#ifdef WITH_RENDERING_TRACE
		if (R_TraceLoggingEnabled()) {
			static char temp[1024];

			temp[0] = '\0';
			for (i = 0; i < count; ++i) {
				if (i) {
					strlcat(temp, ",", sizeof(temp));
				}
				strlcat(temp, R_TextureIdentifier(textures[i]), sizeof(temp));
			}
			R_TraceLogAPICall("glBindTextures(GL_TEXTURE%d, %d[%s])", first, count, temp);
		}
#endif
	}
	else if (multi) {
		for (i = 0; i < count; ++i) {
			if (!already_bound[i]) {
				renderer.TextureUnitBind(first + i, textures[i]);
			}
		}
	}
	else if (first_to_change >= 0) {
		renderer.TextureUnitBind(first + first_to_change, textures[first_to_change]);
	}
	memcpy(bound_texture_state, postbind_texture_state, sizeof(bound_texture_state));
}

void GL_BindImageTexture(GLuint unit, texture_ref texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format)
{
	GLuint glRef = 0;

	if (R_TextureReferenceIsValid(texture)) {
		glRef = GL_TextureNameFromReference(texture);

		if (unit < MAX_LOGGED_IMAGE_UNITS) {
			if (bound_images[unit].texture == glRef && bound_images[unit].level == level && bound_images[unit].layered == layered && bound_images[unit].layer == layer && bound_images[unit].access == access && bound_images[unit].format == format) {
				return;
			}

			bound_images[unit].texture = glRef;
			bound_images[unit].level = level;
			bound_images[unit].layered = layered;
			bound_images[unit].layer = layer;
			bound_images[unit].access = access;
			bound_images[unit].format = format;
		}
	}
	else {
		if (unit < MAX_LOGGED_IMAGE_UNITS) {
			memset(&bound_images[unit], 0, sizeof(bound_images[unit]));
		}
	}

	GL_Procedure(glBindImageTexture, unit, glRef, level, layered, layer, access, format);
}

#ifdef WITH_RENDERING_TRACE
void R_TracePrintState(FILE* debug_frame_out, int debug_frame_depth)
{
// meag: disabled just for clarity, only enable if you fear the states aren't being reset correctly
//       less useful now we have all the states enumerated, the state manager should be setting everything
#if 0
	debug_frame_depth += 7;

	if (debug_frame_out) {
		rendering_state_t* current = &opengl.rendering_state;

		fprintf(debug_frame_out, "%.*s <state-dump>\n", debug_frame_depth, "                                                          ");
		fprintf(debug_frame_out, "%.*s   Z-Buffer: %s, func %s range %f=>%f [mask %s]\n", debug_frame_depth, "                                                          ", current->depth.test_enabled ? "on" : "off", txtDepthFunctions[current->depth.func], current->depth.nearRange, current->depth.farRange, current->depth.mask_enabled ? "on" : "off");
		fprintf(debug_frame_out, "%.*s   Cull-face: %s, mode %s\n", debug_frame_depth, "                                                          ", current->cullface.enabled ? "enabled" : "disabled", txtCullFaceValues[current->cullface.mode]);
		fprintf(debug_frame_out, "%.*s   Blending: %s, func %s\n", debug_frame_depth, "                                                          ", current->blendingEnabled ? "enabled" : "disabled", txtBlendFuncNames[current->blendFunc]);
#ifdef RENDERER_OPTION_CLASSIC_OPENGL
		fprintf(debug_frame_out, "%.*s   AlphaTest: %s", debug_frame_depth, "                                                          ", current->alphaTesting.enabled ? "on" : "off");
		if (current->alphaTesting.enabled) {
			fprintf(debug_frame_out, "func %s(%f)\n", txtAlphaTestModeValues[current->alphaTesting.func], current->alphaTesting.value);
		}
		else {
			fprintf(debug_frame_out, "\n");
		}
		fprintf(debug_frame_out, "%.*s   Texture units: [", debug_frame_depth, "                                                          ");
		{
			int i;

			for (i = 0; i < sizeof(current->textureUnits) / sizeof(current->textureUnits[0]); ++i) {
				fprintf(debug_frame_out, "%s", i ? "," : "");
				if (current->textureUnits[i].enabled && bound_textures[i]) {
					fprintf(debug_frame_out, "%s(%s)", txtTextureEnvModeValues[current->textureUnits[i].mode], GL_TextureIdentifierByGLReference(bound_textures[i]));
				}
				else if (current->textureUnits[i].enabled) {
					fprintf(debug_frame_out, "%s(<null>)", txtTextureEnvModeValues[current->textureUnits[i].mode]);
				}
				else {
					fprintf(debug_frame_out, "<off>");
				}
			}
		}
		fprintf(debug_frame_out, "]\n");
		{
			float matrix[16];
			int i;

			R_GetModelviewMatrix(matrix);
			fprintf(debug_frame_out, "%.*s   Modelview matrix:\n", debug_frame_depth, "                                                          ");
			for (i = 0; i < 16; i += 4) {
				fprintf(debug_frame_out, "%.*s       [%8.5f %8.5f %8.5f %8.5f]\n", debug_frame_depth, "                                                          ", matrix[i + 0], matrix[i + 1], matrix[i + 2], matrix[i + 3]);
			}

			R_GetProjectionMatrix(matrix);
			fprintf(debug_frame_out, "%.*s   Projection matrix:\n", debug_frame_depth, "                                                          ");
			for (i = 0; i < 16; i += 4) {
				fprintf(debug_frame_out, "%.*s       [%8.5f %8.5f %8.5f %8.5f]\n", debug_frame_depth, "                                                          ", matrix[i + 0], matrix[i + 1], matrix[i + 2], matrix[i + 3]);
			}
		}
#endif
		fprintf(debug_frame_out, "%.*s   glPolygonMode: %s\n", debug_frame_depth, "                                                          ", txtPolygonModeValues[current->polygonMode]);
		fprintf(debug_frame_out, "%.*s   vao: %s\n", debug_frame_depth, "                                                          ", vaoNames[currentVAO]);
#ifdef RENDERER_OPTION_CLASSIC_OPENGL
		if (R_VAOBound() && R_UseImmediateOpenGL()) {
			GLC_PrintVAOState(debug_frame_out, debug_frame_depth, currentVAO);
		}
#endif
		if (buffers.PrintState) {
			buffers.PrintState(debug_frame_out, debug_frame_depth);
		}
		fprintf(debug_frame_out, "%.*s </state-dump>\n", debug_frame_depth, "                                                          ");
	}
#endif
}
#endif

void GL_LoadStateFunctions(void)
{
	glConfig.supported_features &= ~(R_SUPPORT_MULTITEXTURING | R_SUPPORT_IMAGE_PROCESSING);

	GL_LoadOptionalFunction(glActiveTexture);
	glConfig.supported_features |= (GL_Available(glActiveTexture) ? R_SUPPORT_MULTITEXTURING : 0);

	if (SDL_GL_ExtensionSupported("GL_ARB_shader_image_load_store")) {
		GL_LoadOptionalFunction(glBindImageTexture);
		glConfig.supported_features |= (GL_Available(glBindImageTexture) ? R_SUPPORT_IMAGE_PROCESSING : 0);
	}

	// 4.4 - binds textures to consecutive texture units
	GL_InvalidateFunction(glBindTextures);
	if (SDL_GL_ExtensionSupported("GL_ARB_multi_bind") && !COM_CheckParm(cmdline_param_client_nomultibind)) {
		GL_LoadOptionalFunction(glBindTextures);

		// Invalidate if on particular drivers (see github bug #416)
		if (GL_Available(glBindTextures) && glConfig.amd_issues) {
			GL_InvalidateFunction(glBindTextures);
			glConfig.broken_features |= R_BROKEN_GLBINDTEXTURES;
		}
	}
}

#ifdef RENDERER_OPTION_CLASSIC_OPENGL
void GLC_ClientActiveTexture(GLenum texture_unit)
{
	if (GL_Available(glClientActiveTexture)) {
		GL_Procedure(glClientActiveTexture, texture_unit);
	}
	else {
		assert(texture_unit == GL_TEXTURE0);
	}
}
#endif // RENDERER_OPTION_CLASSIC_OPENGL

void R_ApplyRenderingState(r_state_id state)
{
	assert(state != r_state_null);
	if (!state) {
		Con_Printf("ERROR: NULL rendering state\n");
		return;
	}
	assert(states[state].initialized);

	renderer.ApplyRenderingState(state);
}

void R_CustomColor(float r, float g, float b, float a)
{
#ifdef RENDERER_OPTION_CLASSIC_OPENGL
	if (R_UseImmediateOpenGL()) {
		if (!opengl.rendering_state.colorValid || opengl.rendering_state.color[0] != r || opengl.rendering_state.color[1] != g || opengl.rendering_state.color[2] != b || opengl.rendering_state.color[3] != a) {
			glColor4f(
				opengl.rendering_state.color[0] = r,
				opengl.rendering_state.color[1] = g,
				opengl.rendering_state.color[2] = b,
				opengl.rendering_state.color[3] = a
			);
			opengl.rendering_state.colorValid = true;
		}
	}
#endif
}

void R_CustomLineWidth(float width)
{
	if (width != opengl.rendering_state.line.width) {
		if (R_UseImmediateOpenGL() || R_UseModernOpenGL()) {
			glLineWidth(opengl.rendering_state.line.width = width);
			R_TraceLogAPICall("glLineWidth(%f)", width);
		}
		else if (R_UseVulkan()) {
			// Requires VK_DYNAMIC_STATE_LINE_WIDTH
			// Requires wide lines feature (if not, must be 1)

			// vkCmdSetLineWidth(commandBuffer, width)
			opengl.rendering_state.line.width = width;
		}
	}
}

void R_CustomPolygonOffset(r_polygonoffset_t mode)
{
	rendering_state_t* current = &opengl.rendering_state;

	if (mode != current->polygonOffset.option) {
		extern cvar_t gl_brush_polygonoffset, gl_brush_polygonoffset_factor;

		float factor = (mode == r_polygonoffset_standard ? bound(-1, gl_brush_polygonoffset_factor.value, 1) : 1);
		float units = (mode == r_polygonoffset_standard ? bound(0, gl_brush_polygonoffset.value, 2) : 1);
		qbool enabled = (mode == r_polygonoffset_standard || mode == r_polygonoffset_outlines) && units != 0;

		if (enabled) {
			if (!current->polygonOffset.fillEnabled) {
				glEnable(GL_POLYGON_OFFSET_FILL);
				R_TraceLogAPICall("glEnable(GL_POLYGON_OFFSET_FILL)");
				current->polygonOffset.fillEnabled = true;
			}
			if (!current->polygonOffset.lineEnabled) {
				glEnable(GL_POLYGON_OFFSET_LINE);
				R_TraceLogAPICall("glEnable(GL_POLYGON_OFFSET_LINE)");
				current->polygonOffset.lineEnabled = true;
			}

			if (current->polygonOffset.factor != factor || current->polygonOffset.units != units) {
				glPolygonOffset(factor, units);
				R_TraceLogAPICall("glPolygonOffset(factor %f, units %f)", factor, units);

				current->polygonOffset.factor = factor;
				current->polygonOffset.units = units;
			}
		}
		else {
			if (current->polygonOffset.fillEnabled) {
				glDisable(GL_POLYGON_OFFSET_FILL);
				R_TraceLogAPICall("glDisable(GL_POLYGON_OFFSET_FILL)");
				current->polygonOffset.fillEnabled = false;
			}
			if (current->polygonOffset.lineEnabled) {
				glDisable(GL_POLYGON_OFFSET_LINE);
				R_TraceLogAPICall("glDisable(GL_POLYGON_OFFSET_LINE)");
				current->polygonOffset.lineEnabled = false;
			}
		}
		current->polygonOffset.option = mode;
	}
}

void R_CustomColor4ubv(const byte* color)
{
	float r = color[0] / 255.0f;
	float g = color[1] / 255.0f;
	float b = color[2] / 255.0f;
	float a = color[3] / 255.0f;

	R_CustomColor(r, g, b, a);
}

void R_EnableScissorTest(int x, int y, int width, int height)
{
	if (R_UseImmediateOpenGL() || R_UseModernOpenGL()) {
		glEnable(GL_SCISSOR_TEST);
		glScissor(x, y, width, height);
	}
}

void R_DisableScissorTest(void)
{
	if (R_UseImmediateOpenGL() || R_UseModernOpenGL()) {
		glDisable(GL_SCISSOR_TEST);
	}
}

void R_ClearColor(float r, float g, float b, float a)
{
	if (R_UseImmediateOpenGL()) {
		glClearColor(
			opengl.rendering_state.clearColor[0] = r,
			opengl.rendering_state.clearColor[1] = g,
			opengl.rendering_state.clearColor[2] = b,
			opengl.rendering_state.clearColor[3] = a
		);
	}
	else if (R_UseModernOpenGL()) {
		glClearColor(
			opengl.rendering_state.clearColor[0] = r,
			opengl.rendering_state.clearColor[1] = g,
			opengl.rendering_state.clearColor[2] = b,
			opengl.rendering_state.clearColor[3] = a
		);
	}
}

rendering_state_t* R_CopyRenderingState(r_state_id dest_id, r_state_id source_id, const char* name)
{
	rendering_state_t* state = &states[dest_id];
	const rendering_state_t* src = &states[source_id];

	memcpy(state, src, sizeof(*state));
	strlcpy(state->name, name, sizeof(state->name));
	return state;
}

void R_InitialiseVAOState(void)
{
	currentVAO = vao_none;
}

qbool R_VertexArrayCreated(r_vao_id vao)
{
	return renderer.VertexArrayCreated(vao);
}

void R_BindVertexArray(r_vao_id vao)
{
	if (currentVAO != vao || opengl.rendering_state.glc_vao_force_rebind) {
		R_TraceEnterRegion(va("R_BindVertexArray(%s)", vaoNames[vao]), true);

		assert(vao == vao_none || R_VertexArrayCreated(vao));

		renderer.BindVertexArray(vao);

		currentVAO = vao;
		opengl.rendering_state.glc_vao_force_rebind = false;
		R_TraceLeaveRegion(true);
	}
}

void R_GenVertexArray(r_vao_id vao)
{
	renderer.GenVertexArray(vao, vaoNames[vao]);
}

qbool R_VAOBound(void)
{
	return currentVAO != vao_none;
}

void R_BindVertexArrayElementBuffer(r_buffer_id ref)
{
	if (currentVAO != vao_none) {
		renderer.BindVertexArrayElementBuffer(currentVAO, ref);
	}
}

#ifdef RENDERER_OPTION_CLASSIC_OPENGL
void R_GLC_VertexPointer(r_buffer_id buf, qbool enabled, int size, GLenum type, int stride, void* pointer_or_offset)
{
	if (enabled) {
		glc_vertex_array_element_t* array = &opengl.rendering_state.vertex_array;

		if (R_BufferReferenceIsValid(buf)) {
			buffers.Bind(buf);
		}
		else {
			buffers.UnBind(buffertype_vertex);
		}

		if (!R_BufferReferencesEqual(buf, array->buf) || size != array->size || type != array->type || stride != array->stride || pointer_or_offset != array->pointer_or_offset) {
			glVertexPointer(array->size = size, array->type = type, array->stride = stride, array->pointer_or_offset = pointer_or_offset);
			array->buf = buf;
			R_TraceLogAPICall("glVertexPointer(size %d, type %s, stride %d, ptr %p)", size, type == GL_FLOAT ? "FLOAT" : type == GL_UNSIGNED_BYTE ? "UBYTE" : "???", stride, pointer_or_offset);
		}
		if (!opengl.rendering_state.vertex_array.enabled) {
			glEnableClientState(GL_VERTEX_ARRAY);
			R_TraceLogAPICall("glEnableClientState(GL_VERTEX_ARRAY)");
			opengl.rendering_state.vertex_array.enabled = true;
		}
	}
	else if (!enabled && opengl.rendering_state.vertex_array.enabled) {
		glDisableClientState(GL_VERTEX_ARRAY);
		R_TraceLogAPICall("glDisableClientState(GL_VERTEX_ARRAY)");
		opengl.rendering_state.vertex_array.enabled = false;
	}
}

void R_GLC_ColorPointer(r_buffer_id buf, qbool enabled, int size, GLenum type, int stride, void* pointer_or_offset)
{
	if (enabled) {
		glc_vertex_array_element_t* array = &opengl.rendering_state.color_array;

		if (R_BufferReferenceIsValid(buf)) {
			buffers.Bind(buf);
		}
		else {
			buffers.UnBind(buffertype_vertex);
		}

		if (!R_BufferReferencesEqual(buf, array->buf) || size != array->size || type != array->type || stride != array->stride || pointer_or_offset != array->pointer_or_offset) {
			glColorPointer(array->size = size, array->type = type, array->stride = stride, array->pointer_or_offset = pointer_or_offset);
			R_TraceLogAPICall("glColorPointer(size %d, type %s, stride %d, ptr %p)", size, type == GL_FLOAT ? "FLOAT" : type == GL_UNSIGNED_BYTE ? "UBYTE" : "???", stride, pointer_or_offset);
		}
		if (!opengl.rendering_state.color_array.enabled) {
			glEnableClientState(GL_COLOR_ARRAY);
			R_TraceLogAPICall("glEnableClientState(GL_COLOR_ARRAY)");
			opengl.rendering_state.colorValid = false;
			opengl.rendering_state.color_array.enabled = true;
		}
	}
	else if (!enabled && opengl.rendering_state.color_array.enabled) {
		glDisableClientState(GL_COLOR_ARRAY);
		R_TraceLogAPICall("glDisableClientState(GL_COLOR_ARRAY)");
		opengl.rendering_state.colorValid = false;
		opengl.rendering_state.color_array.enabled = false;
	}
}

void R_GLC_NormalPointer(r_buffer_id buf, qbool enabled, int size, GLenum type, int stride, void* pointer_or_offset)
{
	if (enabled) {
		glc_vertex_array_element_t* array = &opengl.rendering_state.normal_array;

		if (R_BufferReferenceIsValid(buf)) {
			buffers.Bind(buf);
		}
		else {
			buffers.UnBind(buffertype_vertex);
		}

		if (!R_BufferReferencesEqual(buf, array->buf) || size != array->size || type != array->type || stride != array->stride || pointer_or_offset != array->pointer_or_offset) {
			glNormalPointer(array->type = type, array->stride = stride, array->pointer_or_offset = pointer_or_offset);
			array->size = size;
			R_TraceLogAPICall("glColorPointer(size %d, type %s, stride %d, ptr %p)", size, type == GL_FLOAT ? "FLOAT" : type == GL_UNSIGNED_BYTE ? "UBYTE" : "???", stride, pointer_or_offset);
		}
		if (!opengl.rendering_state.normal_array.enabled) {
			glEnableClientState(GL_NORMAL_ARRAY);
			R_TraceLogAPICall("glEnableClientState(GL_NORMAL_ARRAY)");
			opengl.rendering_state.normal_array.enabled = true;
		}
	}
	else if (!enabled && opengl.rendering_state.normal_array.enabled) {
		glDisableClientState(GL_NORMAL_ARRAY);
		R_TraceLogAPICall("glDisableClientState(GL_NORMAL_ARRAY)");
		opengl.rendering_state.normal_array.enabled = false;
	}
}

void R_GLC_DisableColorPointer(void)
{
	if (opengl.rendering_state.color_array.enabled) {
		glDisableClientState(GL_COLOR_ARRAY);
		R_TraceLogAPICall("glDisableClientState(GL_COLOR_ARRAY)");
		opengl.rendering_state.colorValid = false;
		opengl.rendering_state.color_array.enabled = false;
		opengl.rendering_state.glc_vao_force_rebind = true;
	}
}

void R_GLC_DisableTexturePointer(int unit)
{
	if (unit < 0 || unit >= sizeof(opengl.rendering_state.textureUnits) / sizeof(opengl.rendering_state.textureUnits[0])) {
		return;
	}

	if (opengl.rendering_state.textureUnits[unit].va.enabled) {
		GLC_ClientActiveTexture(GL_TEXTURE0 + unit);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		R_TraceLogAPICall("glDisableClientState[unit %d](GL_TEXTURE_COORD_ARRAY)", unit);
		opengl.rendering_state.textureUnits[unit].va.enabled = false;
		opengl.rendering_state.glc_vao_force_rebind = true;
	}
}

void R_GLC_TexturePointer(r_buffer_id buf, int unit, qbool enabled, int size, GLenum type, int stride, void* pointer_or_offset)
{
	if (unit < 0 || unit >= sizeof(opengl.rendering_state.textureUnits) / sizeof(opengl.rendering_state.textureUnits[0])) {
		return;
	}

	if (enabled) {
		glc_vertex_array_element_t* array = &opengl.rendering_state.textureUnits[unit].va;
		qbool pointer_call_needed = (!R_BufferReferencesEqual(buf, array->buf) || size != array->size || type != array->type || stride != array->stride || pointer_or_offset != array->pointer_or_offset);

		if (R_BufferReferenceIsValid(buf)) {
			buffers.Bind(buf);
		}
		else {
			buffers.UnBind(buffertype_vertex);
		}

		if (!array->enabled || pointer_call_needed) {
			GLC_ClientActiveTexture(GL_TEXTURE0 + unit);
		}
		if (pointer_call_needed) {
			glTexCoordPointer(array->size = size, array->type = type, array->stride = stride, array->pointer_or_offset = pointer_or_offset);
			array->buf = buf;
			R_TraceLogAPICall("glTexCoordPointer[unit %d](size %d, type %s, stride %d, ptr %p)", unit, size, type == GL_FLOAT ? "FLOAT" : type == GL_UNSIGNED_BYTE ? "UBYTE" : "???", stride, pointer_or_offset);
		}
		if (!array->enabled) {
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			R_TraceLogAPICall("glEnableClientState[unit %d](GL_TEXTURE_COORD_ARRAY)", unit);
			array->enabled = true;
		}
	}
	else if (!enabled) {
		R_GLC_DisableTexturePointer(unit);
	}
}

void GLC_CustomAlphaTesting(qbool enabled)
{
	rendering_state_t state;
	rendering_state_t* current = &opengl.rendering_state;

	if (enabled) {
		R_GLC_ConfigureAlphaTesting(&state, true, r_alphatest_func_greater, 0.333f);
	}
	else {
		R_GLC_ConfigureAlphaTesting(&state, false, r_alphatest_func_always, 0);
	}

	GLC_ToggleAlphaTesting(&state, current);
}

static void GLC_ToggleAlphaTesting(const rendering_state_t* state, rendering_state_t* current)
{
	GL_ApplySimpleToggle(state, current, alphaTesting.enabled, GL_ALPHA_TEST);
	if (current->alphaTesting.enabled && (state->alphaTesting.func != current->alphaTesting.func || state->alphaTesting.value != current->alphaTesting.value)) {
		glAlphaFunc(
			glAlphaTestModeValues[current->alphaTesting.func = state->alphaTesting.func],
			current->alphaTesting.value = state->alphaTesting.value
		);
		R_TraceLogAPICall("glAlphaFunc(%s %f)", txtAlphaTestModeValues[state->alphaTesting.func], state->alphaTesting.value);
	}
}

void GLC_ApplyRenderingState(r_state_id id)
{
	rendering_state_t* current = &opengl.rendering_state;
	rendering_state_t* state = &states[id];
	int i;

	GL_ProcessErrors("Pre-GLC_ApplyRenderingState()");
	R_TraceEnterRegion(va("GLC_ApplyRenderingState(%s)", state->name), true);

	// Alpha-testing
	GLC_ToggleAlphaTesting(state, current);

	// Texture units
	for (i = 0; i < sizeof(current->textureUnits) / sizeof(current->textureUnits[0]) && i < glConfig.texture_units; ++i) {
		if (state->textureUnits[i].enabled != current->textureUnits[i].enabled) {
			if ((current->textureUnits[i].enabled = state->textureUnits[i].enabled)) {
				R_TraceLogAPICall("Enabling texturing on unit %d", i);
				GL_SelectTexture(GL_TEXTURE0 + i);
				glEnable(GL_TEXTURE_2D);
				if (state->textureUnits[i].mode != current->textureUnits[i].mode) {
					R_TraceLogAPICall("texture unit mode[%d] = %s", i, txtTextureEnvModeValues[state->textureUnits[i].mode]);
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, glTextureEnvModeValues[current->textureUnits[i].mode = state->textureUnits[i].mode]);
				}
			}
			else {
				R_TraceLogAPICall("Disabling texturing on unit %d", i);
				GL_SelectTexture(GL_TEXTURE0 + i);
				glDisable(GL_TEXTURE_2D);
			}
		}
		else if (current->textureUnits[i].enabled && state->textureUnits[i].mode != current->textureUnits[i].mode) {
			R_TraceLogAPICall("texture unit mode[%d] = %s", i, txtTextureEnvModeValues[state->textureUnits[i].mode]);
			GL_SelectTexture(GL_TEXTURE0 + i);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, glTextureEnvModeValues[current->textureUnits[i].mode = state->textureUnits[i].mode]);
		}
	}
	GL_ProcessErrors("Post-GLC_ApplyRenderingState(texture-units)");

	// Color
	if (!current->colorValid || current->color[0] != state->color[0] || current->color[1] != state->color[1] || current->color[2] != state->color[2] || current->color[3] != state->color[3]) {
		glColor4f(
			current->color[0] = state->color[0],
			current->color[1] = state->color[1],
			current->color[2] = state->color[2],
			current->color[3] = state->color[3]
		);
		R_TraceLogAPICall("glColor4f(%f %f %f %f)", state->color[0], state->color[1], state->color[2], state->color[3]);
		current->colorValid = true;
	}

	// Fog
	if (r_refdef2.fog_render) {
		if (state->fog.mode == r_fogmode_disabled && current->fog.mode != r_fogmode_disabled) {
			current->fog.mode = r_fogmode_disabled;
			glDisable(GL_FOG);
			R_TraceLogAPICall("glDisable(GL_FOG)");
		}
		else if (state->fog.mode == r_fogmode_enabled) {
			if (current->fog.mode != r_fogmode_enabled) {
				current->fog.mode = r_fogmode_enabled;
				glEnable(GL_FOG);
				if (r_refdef2.fog_calculation == fogcalc_linear) {
					if (current->fog.calculation != r_refdef2.fog_calculation) {
						glFogi(GL_FOG_MODE, GL_LINEAR);
						current->fog.calculation = r_refdef2.fog_calculation;
					}
					if (current->fog.start != r_refdef2.fog_linear_start) {
						glFogf(GL_FOG_START, r_refdef2.fog_linear_start);
						current->fog.start = r_refdef2.fog_linear_start;
					}
					if (current->fog.end != r_refdef2.fog_linear_end) {
						glFogf(GL_FOG_END, r_refdef2.fog_linear_end);
						current->fog.end = r_refdef2.fog_linear_end;
					}
				}
				else if (r_refdef2.fog_calculation == fogcalc_exp) {
					if (current->fog.calculation != r_refdef2.fog_calculation) {
						glFogi(GL_FOG_MODE, GL_EXP);
						current->fog.calculation = r_refdef2.fog_calculation;
					}
					if (current->fog.density != r_refdef2.fog_density) {
						glFogf(GL_FOG_DENSITY, r_refdef2.fog_density);
						current->fog.density = r_refdef2.fog_density;
					}
				}
				else if (r_refdef2.fog_calculation == fogcalc_exp2) {
					if (current->fog.calculation != r_refdef2.fog_calculation) {
						glFogi(GL_FOG_MODE, GL_EXP2);
						current->fog.calculation = r_refdef2.fog_calculation;
					}
					if (current->fog.density != r_refdef2.fog_density) {
						glFogf(GL_FOG_DENSITY, r_refdef2.fog_density);
						current->fog.density = r_refdef2.fog_density;
					}
				}
				R_TraceLogAPICall("glEnable(GL_FOG)");
			}
			if (memcmp(current->fog.color, r_refdef2.fog_color, sizeof(current->fog.color))) {
				glFogfv(GL_FOG_COLOR, r_refdef2.fog_color);
				memcpy(current->fog.color, r_refdef2.fog_color, sizeof(current->fog.color));
				R_TraceLogAPICall("glFogfv(GL_FOG_COLOR, [%d %d %d %d])", (int)(current->fog.color[0] * 255), (int)(current->fog.color[1] * 255), (int)(current->fog.color[2] * 255), (int)(current->fog.color[3] * 255));
			}
		}
	}
	else if (current->fog.mode != r_fogmode_disabled) {
		current->fog.mode = r_fogmode_disabled;
		glDisable(GL_FOG);
		R_TraceLogAPICall("glDisable(GL_FOG)");
	}

	if (state->vao_id) {
		GLC_EnsureVAOCreated(state->vao_id);
	}

	GL_ProcessErrors("Pre-GLC_ApplyRenderingState()");
	GL_ApplyRenderingState(id);
	GL_ProcessErrors("Post-ApplyRenderingState()");

	R_TraceLeaveRegion(true);
}

void R_GLC_TextureUnitSet(rendering_state_t* state, int index, qbool enabled, r_texunit_mode_t mode)
{
	state->textureUnits[index].enabled = enabled;
	state->textureUnits[index].mode = mode;
}

void R_GLC_ConfigureAlphaTesting(rendering_state_t* state, qbool enabled, r_alphatest_func_t func, float value)
{
	state->alphaTesting.enabled = enabled;
	state->alphaTesting.func = func;
	state->alphaTesting.value = value;
}

#endif // RENDERER_OPTION_CLASSIC_OPENGL

void R_BufferInvalidateBoundState(r_buffer_id ref)
{
#ifdef RENDERER_OPTION_CLASSIC_OPENGL
	int i;

	if (R_BufferReferencesEqual(opengl.rendering_state.vertex_array.buf, ref)) {
		opengl.rendering_state.vertex_array.buf = r_buffer_none;
	}
	if (R_BufferReferencesEqual(opengl.rendering_state.color_array.buf, ref)) {
		opengl.rendering_state.color_array.buf = r_buffer_none;
	}
	if (R_BufferReferencesEqual(opengl.rendering_state.normal_array.buf, ref)) {
		opengl.rendering_state.normal_array.buf = r_buffer_none;
	}
	for (i = 0; i < sizeof(opengl.rendering_state.textureUnits) / sizeof(opengl.rendering_state.textureUnits[0]); ++i) {
		if (R_BufferReferencesEqual(opengl.rendering_state.textureUnits[i].va.buf, ref)) {
			opengl.rendering_state.textureUnits[i].va.buf = r_buffer_none;
		}
	}
#endif
}

void GL_InvalidateViewport(void)
{
	rendering_state_t* state = &opengl.rendering_state;
	state->currentViewportX = 0;
	state->currentViewportY = 0;
	state->currentViewportWidth = 0;
	state->currentViewportHeight = 0;
}

void GL_PackAlignment(int alignment_in_bytes)
{
	if (opengl.rendering_state.pack_alignment != alignment_in_bytes) {
		glPixelStorei(GL_PACK_ALIGNMENT, alignment_in_bytes);
		opengl.rendering_state.pack_alignment = alignment_in_bytes;
	}
}

#ifdef RENDERER_OPTION_CLASSIC_OPENGL
void GLC_MultiTexCoord2f(GLenum target, float s, float t)
{
	if (GL_Available(glMultiTexCoord2f)) {
		GL_Procedure(glMultiTexCoord2f, target, s, t);
	}
}

void GL_CheckMultiTextureExtensions(void)
{
	if (!COM_CheckParm(cmdline_param_client_nomultitexturing) && SDL_GL_ExtensionSupported("GL_ARB_multitexture")) {
		GL_LoadOptionalFunction(glMultiTexCoord2f);
		if (!GL_Available(glMultiTexCoord2f)) {
			GL_LoadOptionalFunctionARB(glMultiTexCoord2f);
		}
		GL_LoadOptionalFunction(glActiveTexture);
		if (!GL_Available(glActiveTexture)) {
			GL_LoadOptionalFunctionARB(glActiveTexture);
		}
		GL_LoadOptionalFunction(glClientActiveTexture);
		if (!GL_Available(glMultiTexCoord2f) || !GL_Available(glActiveTexture) || !GL_Available(glClientActiveTexture)) {
			return;
		}
		Com_Printf_State(PRINT_OK, "Multitexture extensions found\n");
		gl_mtexable = true;
	}

	gl_textureunits = min(glConfig.texture_units, 4);

	if (COM_CheckParm(cmdline_param_client_maximum2textureunits)) {
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
#else
void GL_CheckMultiTextureExtensions(void)
{
	if (!SDL_GL_ExtensionSupported("GL_ARB_multitexture")) {
		Sys_Error("Required extension not supported: GL_ARB_multitexture");
		return;
	}

	GL_LoadOptionalFunction(glActiveTexture);
	if (!GL_Available(glActiveTexture)) {
		Sys_Error("Required OpenGL function not supported: glActiveTexture");
		return;
	}

	gl_textureunits = min(glConfig.texture_units, 4);
	gl_mtexable = true;
}
#endif

#ifdef WITH_RENDERING_TRACE
static int GL_FindSpecificInEnum(GLenum value, GLenum* enum_array, int length)
{
	int i;

	for (i = 0; i < length; ++i) {
		if (value == enum_array[i]) {
			return i;
		}
	}

	return length;
}

static int GL_FindIntegerInEnum(GLenum name, GLenum* enum_array, int length)
{
	GLint glInt = 0;

	GL_BuiltinProcedure(glGetIntegerv, "name=%u, output=%p", name, &glInt);

	return GL_FindSpecificInEnum(glInt, enum_array, length);
}

static float GL_GetFloat(GLenum name)
{
	float result = NAN;

	GL_BuiltinProcedure(glGetFloatv, "name=%u, output=%p", name, &result);

	return result;
}

static r_buffer_id GL_VerifyBufferState(GLenum name)
{
	GLint glInt = 0;

	GL_BuiltinProcedure(glGetIntegerv, "name=%u, output=%p", name, &glInt);

	return r_buffer_none; // FIXME
}

static qbool GL_IsEnabled_(GLenum pname, const char* name)
{
	qbool result;

	R_TraceAPI("glIsEnabled(%s)", name);
	result = glIsEnabled(pname);
	GL_ProcessErrors(va("glIsEnabled(%s)", name));
	return result;
}

#define GL_IsEnabled(pname) GL_IsEnabled_(pname, #pname)

#ifdef RENDERER_OPTION_CLASSIC_OPENGL
static void GLC_VerifyArray(glc_vertex_array_element_t* arr, GLenum enabled, GLenum bufferBinding, GLenum pointer, GLenum size, GLenum stride, GLenum type)
{
	if (renderer.vaos_supported) {
		arr->enabled = GL_IsEnabled(enabled);
		arr->buf = GL_VerifyBufferState(bufferBinding);
		GL_BuiltinProcedure(glGetPointerv, "name=%u, output=%p", pointer, &arr->pointer_or_offset);
		if (size) {
			GL_BuiltinProcedure(glGetIntegerv, "name=%u, output=%p", size, &arr->size);
		}
		GL_BuiltinProcedure(glGetIntegerv, "name=%u, output=%p", stride, &arr->stride);
		GL_BuiltinProcedure(glGetIntegerv, "name=%u, output=%p", type, &arr->type);
	}
	else {
		memset(arr, 0, sizeof(*arr));
		arr->size = (enabled == GL_NORMAL_ARRAY ? 0 : 4);
		arr->type = GL_FLOAT;
	}
}

static void GLC_DownloadVAOState(rendering_state_t* state)
{
	GLenum texture_modes[4] = { 0 };
	int i = 0;

	if (R_UseImmediateOpenGL()) {
		// vertex array
		GLC_VerifyArray(&state->vertex_array, GL_VERTEX_ARRAY, GL_VERTEX_ARRAY_BUFFER_BINDING, GL_VERTEX_ARRAY_POINTER, GL_VERTEX_ARRAY_SIZE, GL_VERTEX_ARRAY_STRIDE, GL_VERTEX_ARRAY_TYPE);
		// color array
		GLC_VerifyArray(&state->color_array, GL_COLOR_ARRAY, GL_COLOR_ARRAY_BUFFER_BINDING, GL_COLOR_ARRAY_POINTER, GL_COLOR_ARRAY_SIZE, GL_COLOR_ARRAY_STRIDE, GL_COLOR_ARRAY_TYPE);
		// normal array
		GLC_VerifyArray(&state->normal_array, GL_NORMAL_ARRAY, GL_NORMAL_ARRAY_BUFFER_BINDING, GL_NORMAL_ARRAY_POINTER, 0, GL_NORMAL_ARRAY_STRIDE, GL_NORMAL_ARRAY_TYPE);

		state->textureUnits[i].mode = r_texunit_mode_modulate;
		state->textureUnits[i].va.type = GL_FLOAT;
		state->textureUnits[i].enabled = false;
		for (i = 0; i < sizeof(state->textureUnits) / sizeof(state->textureUnits[0]); ++i) {
			if (i < glConfig.texture_units) {
				GLC_ClientActiveTexture(GL_TEXTURE0 + i);
				GLC_VerifyArray(&state->textureUnits[i].va, GL_TEXTURE_COORD_ARRAY, GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING, GL_TEXTURE_COORD_ARRAY_POINTER, GL_TEXTURE_COORD_ARRAY_SIZE, GL_TEXTURE_COORD_ARRAY_STRIDE, GL_TEXTURE_COORD_ARRAY_TYPE);
				GL_SelectTexture(GL_TEXTURE0 + i);
				GL_BuiltinProcedure(glGetTexEnviv, "target=%u, pname=%u, params=%p", GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &texture_modes[i]);
				state->textureUnits[i].mode = GL_FindSpecificInEnum(texture_modes[i], glTextureEnvModeValues, sizeof(glTextureEnvModeValues) / sizeof(glTextureEnvModeValues[0]));
				state->textureUnits[i].enabled = GL_IsEnabled(GL_TEXTURE_2D);
			}
			else {
				state->textureUnits[i].va.size = 4;
				state->textureUnits[i].va.type = GL_FLOAT;
			}
		}
	}
}

static void GLC_DownloadAlphaTestingState(rendering_state_t* state)
{
	if (R_UseImmediateOpenGL()) {
		state->alphaTesting.enabled = GL_IsEnabled(GL_ALPHA_TEST);
		state->alphaTesting.func = GL_FindIntegerInEnum(GL_ALPHA_TEST_FUNC, glAlphaTestModeValues, sizeof(glAlphaTestModeValues) / sizeof(glAlphaTestModeValues[0]));
		state->alphaTesting.value = GL_GetFloat(GL_ALPHA_TEST_REF);
	}
}

static qbool GLC_CompareAlphaTesting(FILE* output, const rendering_state_t* expected, const rendering_state_t* found)
{
	if (memcmp(&expected->alphaTesting, &found->alphaTesting, sizeof(expected->alphaTesting))) {
		R_TraceAPI(
			"!ALPHA-TESTING: expected %d,%d,%f found %d,%d,%f",
			expected->alphaTesting.enabled, expected->alphaTesting.func, expected->alphaTesting.value,
			found->alphaTesting.enabled, found->alphaTesting.func, found->alphaTesting.value
		);
		return true;
	}
	return false;
}

static qbool GLC_CompareVertexArray(FILE* output, const glc_vertex_array_element_t* expected, const glc_vertex_array_element_t* found, const char* name)
{
	if (memcmp(expected, found, sizeof(*expected))) {
		R_TraceAPI("!%s: expected enabled(%d),p_offset(%p),buf(%d),size(%d),stride(%d),type(%d), found enabled(%d),p_offset(%p),buf(%d),size(%d),stride(%d),type(%d)", name,
			expected->enabled, expected->pointer_or_offset, expected->buf, expected->size, expected->stride, expected->type,
			found->enabled, found->pointer_or_offset, found->buf, found->size, found->stride, found->type
		);
		return false;
	}
	return false;
}

static qbool GLC_CompareVertexArrays(FILE* output, const rendering_state_t* expected, const rendering_state_t* found)
{
	qbool problem = false;

	problem = GLC_CompareVertexArray(output, &expected->vertex_array, &found->vertex_array, "VertexArray") || problem;
	problem = GLC_CompareVertexArray(output, &expected->color_array, &found->color_array, "ColorArray") || problem;
	problem = GLC_CompareVertexArray(output, &expected->normal_array, &found->normal_array, "NormalArray") || problem;
	problem = GLC_CompareVertexArray(output, &expected->textureUnits[0].va, &found->textureUnits[0].va, "TextureArray0") || problem;
	problem = GLC_CompareVertexArray(output, &expected->textureUnits[1].va, &found->textureUnits[1].va, "TextureArray1") || problem;
	problem = GLC_CompareVertexArray(output, &expected->textureUnits[2].va, &found->textureUnits[2].va, "TextureArray2") || problem;
	problem = GLC_CompareVertexArray(output, &expected->textureUnits[3].va, &found->textureUnits[3].va, "TextureArray3") || problem;

	return problem;
}

static qbool GLC_CompareTextureUnitState(FILE* output, const rendering_state_t* expected, const rendering_state_t* found)
{
	int i;
	qbool problem = false;

	for (i = 0; i < 4; ++i) {
		if (expected->textureUnits[i].mode != found->textureUnits[i].mode || expected->textureUnits[i].enabled != found->textureUnits[i].enabled) {
			R_TraceAPI("!TEXSTATE%d: expected %d,%d found %d,%d", i, expected->textureUnits[i].mode, found->polygonMode);
			problem = true;
		}
	}

	return problem;
}

// Adjust for what OpenGL doesn't know about
#define GLC_AssumeState(state) { from_gl.state = opengl.rendering_state.state; }
#else
#define GLC_DownloadAlphaTestingState(...)
#define GLC_DownloadVAOState(...)
#define GLC_CompareAlphaTesting(...) (false)
#define GLC_CompareVertexArrays(...) (false)
#define GLC_AssumeState(...)
#endif

static void GL_DownloadTextureUnitState(GLuint* bound_textures, GLuint* bound_arrays, GLuint* bound_cubemaps)
{
	int i;
	int units = min(glConfig.texture_units, MAX_LOGGED_TEXTURE_UNITS);

	for (i = 0; i < units; ++i) {
		bound_cubemaps[i] = bound_textures[i] = bound_arrays[i] = 0;

		GL_SelectTexture(GL_TEXTURE0 + i);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound_textures[i]);
		if (GL_Supported(R_SUPPORT_TEXTURE_ARRAYS)) {
			glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &bound_arrays[i]);
		}
		if (GL_Supported(R_SUPPORT_CUBE_MAPS)) {
			glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &bound_cubemaps[i]);
		}
	}
}

static void GL_DownloadState(rendering_state_t* state, GLuint* gl_bound2d, GLuint* gl_bound3d, GLuint* gl_boundCubemaps)
{
	memset(state, 0, sizeof(*state));

	GLC_DownloadAlphaTestingState(state);

	// Blending - we use a single enum for the pairing of src & dst
	{
		GLint src, dst;
		int i;

		GL_BuiltinProcedure(glGetIntegerv, "name=%u, output=%p", GL_BLEND_SRC, &src);
		GL_BuiltinProcedure(glGetIntegerv, "name=%u, output=%p", GL_BLEND_DST, &dst);

		for (i = 0; i < sizeof(glBlendFuncValuesSource) / sizeof(glBlendFuncValuesSource[0]); ++i) {
			if (glBlendFuncValuesSource[i] == src && glBlendFuncValuesDestination[i] == dst) {
				state->blendFunc = i;
			}
		}
	}
	state->blendingEnabled = GL_IsEnabled(GL_BLEND);

	// Colors
	GL_BuiltinProcedure(glGetFloatv, "pname=%u, params=%p", GL_COLOR_CLEAR_VALUE, state->clearColor);
	{
		GLint colorMask_[4];
		GL_BuiltinProcedure(glGetIntegerv, "name=%u, output=%p", GL_COLOR_WRITEMASK, colorMask_);
		state->colorMask[0] = colorMask_[0];
		state->colorMask[1] = colorMask_[1];
		state->colorMask[2] = colorMask_[2];
		state->colorMask[3] = colorMask_[3];
	}
	if (R_UseImmediateOpenGL()) {
		GL_BuiltinProcedure(glGetFloatv, "pname=%u, params=%p", GL_CURRENT_COLOR, state->color);
	}
	else {
		state->color[0] = state->color[1] = state->color[2] = state->color[3] = 1.0f;
	}

	// cullface
	state->cullface.enabled = GL_IsEnabled(GL_CULL_FACE);
	state->cullface.mode = GL_FindIntegerInEnum(GL_CULL_FACE_MODE, glCullFaceValues, sizeof(glCullFaceValues) / sizeof(glCullFaceValues[0]));

	{
		GLint viewport[4];

		GL_BuiltinProcedure(glGetIntegerv, "pname=%u, params=%p", GL_VIEWPORT, viewport);
		state->currentViewportX = viewport[0];
		state->currentViewportY = viewport[1];
		state->currentViewportWidth = viewport[2];
		state->currentViewportHeight = viewport[3];
	}

	// depth buffer
	{
		float range[2];
		GLint glInt = 0;

		glGetFloatv(GL_DEPTH_RANGE, range);

		state->depth.farRange = range[1];
		state->depth.func = GL_FindIntegerInEnum(GL_DEPTH_FUNC, glDepthFunctions, sizeof(glDepthFunctions) / sizeof(glDepthFunctions[0]));
		glGetIntegerv(GL_DEPTH_WRITEMASK, &glInt);
		state->depth.mask_enabled = (glInt != GL_FALSE);
		state->depth.nearRange = range[0];
		state->depth.test_enabled = GL_IsEnabled(GL_DEPTH_TEST);
	}

	// fog
	if (GL_Supported(R_SUPPORT_FOG)) {
		state->fog.mode = GL_IsEnabled(GL_FOG) ? r_fogmode_enabled : r_fogmode_disabled;
	}

	//
	if (GL_Supported(R_SUPPORT_FRAMEBUFFERS_SRGB)) {
		state->framebuffer_srgb = GL_IsEnabled(GL_FRAMEBUFFER_SRGB);
	}
	else {
		state->framebuffer_srgb = false;
	}

	// TODO: GLC attributes

	// line
	state->line.smooth = GL_IsEnabled(GL_LINE_SMOOTH);
	glGetFloatv(GL_LINE_WIDTH, &state->line.width);
	GL_ProcessErrors("VerifyState[end-3]");

	// 
	{
		GLint polygonModes[2];
		glGetIntegerv(GL_POLYGON_MODE, polygonModes);
		state->polygonMode = GL_FindSpecificInEnum(polygonModes[0], glPolygonModeValues, sizeof(glPolygonModeValues) / sizeof(glPolygonModeValues[0]));
	}

	// polygon offset
	GL_BuiltinProcedure(glGetFloatv, "pname=%u, params=%p", GL_POLYGON_OFFSET_FACTOR, &state->polygonOffset.factor);
	state->polygonOffset.fillEnabled = GL_IsEnabled(GL_POLYGON_OFFSET_FILL);
	state->polygonOffset.lineEnabled = GL_IsEnabled(GL_POLYGON_OFFSET_LINE);
	GL_BuiltinProcedure(glGetFloatv, "pname=%u, params=%p", GL_POLYGON_OFFSET_UNITS, &state->polygonOffset.units);

	// pack offset
	glGetIntegerv(GL_PACK_ALIGNMENT, &state->pack_alignment);

	GL_ProcessErrors("VerifyState[end-2]");
	GLC_DownloadVAOState(state);
	GL_ProcessErrors("VerifyState[end-1]");
	GL_DownloadTextureUnitState(gl_bound2d, gl_bound3d, gl_boundCubemaps);
	GL_ProcessErrors("VerifyState[end]");
}

static qbool GL_CompareBlending(FILE* output, const rendering_state_t* expected, const rendering_state_t* found)
{
	if (expected->blendingEnabled != found->blendingEnabled || expected->blendFunc != found->blendFunc) {
		R_TraceAPI(
			"!BLENDING: expected %d,%d found %d,%d",
			expected->blendingEnabled, expected->blendFunc,
			found->blendingEnabled, found->blendFunc
		);
		return true;
	}
	return false;
}

static qbool GL_CompareColors(FILE* output, const rendering_state_t* expected, const rendering_state_t* found)
{
	qbool problem = false;
	if (memcmp(expected->clearColor, found->clearColor, sizeof(expected->clearColor))) {
		R_TraceAPI(
			"!CLEARCOLOR: expected %f,%f,%f,%f found %f,%f,%f,%f",
			expected->clearColor[0], expected->clearColor[1], expected->clearColor[2], expected->clearColor[3],
			found->clearColor[0], found->clearColor[1], found->clearColor[2], found->clearColor[3]
		);
		problem = true;
	}
	if (memcmp(expected->colorMask, found->colorMask, sizeof(expected->colorMask))) {
		R_TraceAPI(
			"!COLORMASK: expected %d,%d,%d,%d found %d,%d,%d,%d",
			expected->colorMask[0], expected->colorMask[1], expected->colorMask[2], expected->colorMask[3],
			found->colorMask[0], found->colorMask[1], found->colorMask[2], found->colorMask[3]
		);
		problem = true;
	}
	if (memcmp(expected->color, found->color, sizeof(expected->color))) {
		R_TraceAPI(
			"!COLOR: expected %f,%f,%f,%f found %f,%f,%f,%f",
			expected->color[0], expected->color[1], expected->color[2], expected->color[3],
			found->color[0], found->color[1], found->color[2], found->color[3]
		);
		problem = true;
	}
	return problem;
}

static qbool GL_CompareCulling(FILE* output, const rendering_state_t* expected, const rendering_state_t* found)
{
	if (memcmp(&expected->cullface, &found->cullface, sizeof(expected->cullface))) {
		R_TraceAPI(
			"!CULLING: expected %d,%d found %d,%d",
			expected->cullface.enabled, expected->cullface.mode,
			found->cullface.enabled, found->cullface.mode
		);
		return true;
	}
	return false;
}

static qbool GL_CompareViewport(FILE* output, const rendering_state_t* expected, const rendering_state_t* found)
{
	if (expected->currentViewportX != found->currentViewportX ||
		expected->currentViewportHeight != found->currentViewportHeight ||
		expected->currentViewportWidth != found->currentViewportWidth ||
		expected->currentViewportY != found->currentViewportY)
	{
		R_TraceAPI(
			"!VIEWPORT: expected %d,%d,%d,%d found %d,%d,%d,%d",
			expected->currentViewportX, expected->currentViewportY, expected->currentViewportWidth, expected->currentViewportHeight,
			found->currentViewportX, found->currentViewportY, found->currentViewportWidth, found->currentViewportHeight
		);
		return true;
	}
	return false;
}

static qbool GL_CompareDepth(FILE* output, const rendering_state_t* expected, const rendering_state_t* found)
{
	if (memcmp(&expected->depth, &found->depth, sizeof(expected->depth))) {
		R_TraceAPI(
			"!DEPTH: expected %d,%d,%d,%f,%f found %d,%d,%d,%f,%f",
			expected->depth.test_enabled, expected->depth.mask_enabled, expected->depth.func, expected->depth.nearRange, expected->depth.farRange,
			found->depth.test_enabled, found->depth.mask_enabled, found->depth.func, found->depth.nearRange, found->depth.farRange
		);
		return true;
	}
	return false;
}

static qbool GL_CompareLine(FILE* output, const rendering_state_t* expected, const rendering_state_t* found)
{
	if (expected->line.smooth != found->line.smooth || expected->line.width != found->line.width) {
		R_TraceAPI(
			"!LINE: expected %d,%f found %d,%f",
			expected->line.smooth, expected->line.width,
			found->line.smooth, found->line.width
		);
		return true;
	}
	return false;
}

static qbool GL_CompareMisc(FILE* output, const rendering_state_t* expected, const rendering_state_t* found)
{
	qbool problem = false;
	if (expected->fog.mode != found->fog.mode) {
		R_TraceAPI("!FOG: expected %s, found %s", fogModeDescriptions[expected->fog.mode], fogModeDescriptions[found->fog.mode]);
	}
	if (expected->framebuffer_srgb != found->framebuffer_srgb) {
		R_TraceAPI("!SRGB: expected %d, found %d", expected->framebuffer_srgb, found->framebuffer_srgb);
	}
	if (expected->polygonMode != found->polygonMode) {
		R_TraceAPI("!POLYMODE: expected %d, found %d", expected->polygonMode, found->polygonMode);
	}
	return problem;
}


static qbool GL_CompareBoundTextures(FILE* output, const GLuint* expected, const GLuint* found, const char* name)
{
	int units = min(MAX_LOGGED_TEXTURE_UNITS, glConfig.texture_units);
	int i;
	qbool problem = false;

	for (i = 0; i < units; ++i) {
		if (expected[i] != found[i]) {
			R_TraceAPI("!%s%d: expected %u, found %u", name, i, expected[i], found[i]);
			problem = true;
		}
	}

	return problem;
}

static void GL_CompareState(FILE* output, const rendering_state_t* found, const GLuint* gl_bound2d, const GLuint* gl_bound3d, const GLuint* gl_boundCubemap)
{
	qbool problems = false;
	const rendering_state_t* expected = &opengl.rendering_state;

	problems = GL_CompareBlending(output, expected, found) || problems;
	problems = GL_CompareColors(output, expected, found) || problems;
	problems = GL_CompareCulling(output, expected, found) || problems;
	problems = GL_CompareViewport(output, expected, found) || problems;
	problems = GL_CompareDepth(output, expected, found) || problems;
	problems = GL_CompareLine(output, expected, found) || problems;
	problems = GL_CompareMisc(output, expected, found) || problems;

	if (R_UseImmediateOpenGL()) {
		problems = GLC_CompareAlphaTesting(output, expected, found) || problems;
		problems = GLC_CompareVertexArrays(output, expected, found) || problems;
	}

	problems = GL_CompareBoundTextures(output, bound_texture_state[texture_type_2d], gl_bound2d, "2DTEXTURE") || problems;
	problems = GL_CompareBoundTextures(output, bound_texture_state[texture_type_2d_array], gl_bound3d, "TEXTUREARRAY") || problems;
	problems = GL_CompareBoundTextures(output, bound_texture_state[texture_type_cubemap], gl_boundCubemap, "CUBEMAP") || problems;

	if (problems) {
		Con_Printf("*** PROBLEM ***\n");
	}

	return;
}

void GL_VerifyState(FILE* output)
{
	rendering_state_t from_gl;
	GLuint gl_bound2d[MAX_LOGGED_TEXTURE_UNITS] = { 0 };
	GLuint gl_bound3d[MAX_LOGGED_TEXTURE_UNITS] = { 0 };
	GLuint gl_boundCubemap[MAX_LOGGED_TEXTURE_UNITS] = { 0 };

	R_TraceEnterFunctionRegion;
	GL_ProcessErrors(__func__);
	GL_DownloadState(&from_gl, gl_bound2d, gl_bound3d, gl_boundCubemap);

	GLC_AssumeState(normal_array.buf);
	GLC_AssumeState(normal_array.size);
	GLC_AssumeState(color_array.buf);
	GLC_AssumeState(vertex_array.buf);
	GLC_AssumeState(textureUnits[0].va.buf);
	GLC_AssumeState(textureUnits[1].va.buf);
	GLC_AssumeState(textureUnits[2].va.buf);
	GLC_AssumeState(textureUnits[3].va.buf);

	// Compare
	GL_CompareState(output, &from_gl, gl_bound2d, gl_bound3d, gl_boundCubemap);
	R_TraceLeaveFunctionRegion;
}
#endif
