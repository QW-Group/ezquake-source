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

static rendering_state_t states[r_state_count];

typedef void (APIENTRY *glBindTextures_t)(GLuint first, GLsizei count, const GLuint* format);
typedef void (APIENTRY *glBindImageTexture_t)(GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format);
static glBindImageTexture_t     qglBindImageTexture;
static glBindTextures_t         qglBindTextures;
glActiveTexture_t               qglActiveTexture;

void R_InitialiseBrushModelStates(void);
void R_InitialiseStates(void);
void R_Initialise2DStates(void);
void R_InitialiseEntityStates(void);
void GLC_InitialiseSkyStates(void);
void R_InitialiseWorldStates(void);

// FIXME: currentWidth & currentHeight should be initialised to dimensions of window
GLint currentViewportX, currentViewportY;
GLsizei currentViewportWidth, currentViewportHeight;

// VAOs
static r_vao_id currentVAO = vao_none;
static const char* vaoNames[vao_count] = {
	"none", "aliasmodel", "brushmodel", "3d-sprites",
	"hud:circles", "hud:images", "hud:lines", "hud:polygons",
	"post-process"
};

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
	GLuint bound_textures[MAX_LOGGED_TEXTURE_UNITS];
	GLuint bound_arrays[MAX_LOGGED_TEXTURE_UNITS];
	qbool texunitenabled[MAX_LOGGED_TEXTURE_UNITS];
	GLenum unit_texture_mode[MAX_LOGGED_TEXTURE_UNITS];
	image_unit_binding_t bound_images[MAX_LOGGED_IMAGE_UNITS];
} texture_state_t;
*/
typedef struct {
	rendering_state_t rendering_state;
	//texture_state_t textures;
} opengl_state_t;

static GLenum currentTextureUnit;
static GLuint bound_textures[MAX_LOGGED_TEXTURE_UNITS];
static GLuint bound_arrays[MAX_LOGGED_TEXTURE_UNITS];
static image_unit_binding_t bound_images[MAX_LOGGED_IMAGE_UNITS];

static opengl_state_t opengl;
static GLenum glDepthFunctions[r_depthfunc_count];
static GLenum glCullFaceValues[r_cullface_count];
static GLenum glBlendFuncValuesSource[r_blendfunc_count];
static GLenum glBlendFuncValuesDestination[r_blendfunc_count];
static GLenum glPolygonModeValues[r_polygonmode_count];
static GLenum glAlphaTestModeValues[r_alphatest_func_count];
static GLenum glTextureEnvModeValues[r_texunit_mode_count];

#ifdef WITH_OPENGL_TRACE
static const char* txtDepthFunctions[r_depthfunc_count];
static const char* txtCullFaceValues[r_cullface_count];
static const char* txtBlendFuncNames[r_blendfunc_count];
static const char* txtPolygonModeValues[r_polygonmode_count];
static const char* txtAlphaTestModeValues[r_alphatest_func_count];
static const char* txtTextureEnvModeValues[r_texunit_mode_count];
#endif

rendering_state_t* R_InitRenderingState(r_state_id id, qbool default_state, const char* name, r_vao_id vao)
{
	rendering_state_t* state = &states[id];
	SDL_Window* window = SDL_GL_GetCurrentWindow();
	int i;

	strlcpy(state->name, name, sizeof(state->name));

	state->depth.func = r_depthfunc_less;
	state->depth.nearRange = 0;
	state->depth.farRange = 1;
	state->depth.test_enabled = false;
	state->depth.mask_enabled = false;

	state->blendFunc = r_blendfunc_overwrite;
	state->blendingEnabled = false;
	state->alphaTesting.enabled = false;
	state->alphaTesting.value = 0;
	state->alphaTesting.func = r_alphatest_func_always;

	state->currentViewportX = 0;
	state->currentViewportY = 0;
	SDL_GL_GetDrawableSize(window, &state->currentViewportWidth, &state->currentViewportHeight);

	state->cullface.enabled = false;
	state->cullface.mode = r_cullface_back;

	state->framebuffer_srgb = false;
	state->line.smooth = false;
	state->line.width = 1.0f;
	state->fog.enabled = false;
	state->polygonOffset.option = r_polygonoffset_disabled;
	state->polygonOffset.factor = 0;
	state->polygonOffset.units = 0;
	state->polygonOffset.fillEnabled = false;
	state->polygonOffset.lineEnabled = false;
	state->polygonMode = r_polygonmode_fill;
	state->clearColor[0] = state->clearColor[1] = state->clearColor[2] = 0;
	state->clearColor[3] = 1;
	state->color[0] = state->color[1] = state->color[2] = state->color[3] = 1;
	state->colorMask[0] = state->colorMask[1] = state->colorMask[2] = state->colorMask[3] = true;

	for (i = 0; i < sizeof(state->textureUnits) / sizeof(state->textureUnits[0]); ++i) {
		state->textureUnits[i].enabled = false;
		state->textureUnits[i].mode = GL_MODULATE;
	}

	state->vao_id = vao;

	if (default_state) {
		state->cullface.mode = r_cullface_front;
		state->cullface.enabled = true;

		state->depth.func = r_depthfunc_lessorequal;
		state->alphaTesting.enabled = false;
		state->alphaTesting.value = 0.666f;
		state->alphaTesting.func = r_alphatest_func_greater;
		state->blendingEnabled = false;
		state->depth.test_enabled = true;
		state->depth.mask_enabled = true;

#ifndef __APPLE__
		state->clearColor[0] = 0;
		state->clearColor[1] = 0;
		state->clearColor[2] = 0;
		state->clearColor[3] = 1;
#else
		state->clearColor[0] = 0.2;
		state->clearColor[1] = 0.2;
		state->clearColor[2] = 0.2;
		state->clearColor[3] = 1;
#endif
		state->polygonMode = r_polygonmode_fill;
		state->blendFunc = r_blendfunc_premultiplied_alpha;
		state->textureUnits[0].enabled = false;
		state->textureUnits[0].mode = r_texunit_mode_replace;

		state->framebuffer_srgb = (gl_gammacorrection.integer > 0);
	}

	state->initialized = true;
	return state;
}

rendering_state_t* R_Init3DSpriteRenderingState(r_state_id id, const char* name)
{
	rendering_state_t* state = R_InitRenderingState(id, true, name, vao_3dsprites);

	state->fog.enabled = false;
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	state->cullface.enabled = false;
	state->alphaTesting.enabled = false;
	state->alphaTesting.func = r_alphatest_func_greater;
	state->alphaTesting.value = 0.333f;

	return state;
}

#define GL_ApplySimpleToggle(state, current, field, option) \
	if (state->field != current->field) { \
		if (state->field) { \
			glEnable(option); \
			GL_LogAPICall("glEnable(" # option ")"); \
		} \
		else { \
			glDisable(option); \
			GL_LogAPICall("glDisable(" # option ")"); \
		} \
		current->field = state->field; \
	}

static void GL_ApplyRenderingState(r_state_id id)
{
	rendering_state_t* state = &states[id];
	extern cvar_t gl_brush_polygonoffset;
	rendering_state_t* current = &opengl.rendering_state;

	GL_EnterTracedRegion(va("GLC_ApplyRenderingState(%s)", state->name), true);

	if (state->depth.func != current->depth.func) {
		glDepthFunc(glDepthFunctions[current->depth.func = state->depth.func]);
		GL_LogAPICall("glDepthFunc(%s)", txtDepthFunctions[current->depth.func]);
	}
	if (state->depth.nearRange != current->depth.nearRange || state->depth.farRange != current->depth.farRange) {
		glDepthRange(
			current->depth.nearRange = state->depth.nearRange,
			current->depth.farRange = state->depth.farRange
		);
		GL_LogAPICall("glDepthRange(%f,%f)", state->depth.nearRange, state->depth.farRange);
	}
	if (state->cullface.mode != current->cullface.mode) {
		glCullFace(glCullFaceValues[current->cullface.mode = state->cullface.mode]);
		GL_LogAPICall("glCullFace(%s)", txtCullFaceValues[state->cullface.mode]);
	}
	if (state->blendFunc != current->blendFunc) {
		current->blendFunc = state->blendFunc;
		glBlendFunc(
			glBlendFuncValuesSource[state->blendFunc],
			glBlendFuncValuesDestination[state->blendFunc]
		);
		GL_LogAPICall("glBlendFunc(%s)", txtBlendFuncNames[state->blendFunc]);
	}
	if (state->line.width != current->line.width) {
		glLineWidth(current->line.width = state->line.width);
		GL_LogAPICall("glLineWidth(%f)", current->line.width);
	}
	GL_ApplySimpleToggle(state, current, depth.test_enabled, GL_DEPTH_TEST);
	if (state->depth.mask_enabled != current->depth.mask_enabled) {
		glDepthMask((current->depth.mask_enabled = state->depth.mask_enabled) ? GL_TRUE : GL_FALSE);
		GL_LogAPICall("glDepthMask(%s)", current->depth.mask_enabled ? "on" : "off");
	}
	GL_ApplySimpleToggle(state, current, framebuffer_srgb, GL_FRAMEBUFFER_SRGB);
	GL_ApplySimpleToggle(state, current, cullface.enabled, GL_CULL_FACE);
	GL_ApplySimpleToggle(state, current, line.smooth, GL_LINE_SMOOTH);
	if (gl_fogenable.integer) {
		GL_ApplySimpleToggle(state, current, fog.enabled, GL_FOG);
	}
	else if (current->fog.enabled) {
		glDisable(GL_FOG);
		GL_LogAPICall("glDisable(GL_FOG)");
		current->fog.enabled = false;
	}
	if (state->polygonOffset.option != current->polygonOffset.option || gl_brush_polygonoffset.modified) {
		float factor = (state->polygonOffset.option == r_polygonoffset_standard ? 0.05 : 1);
		float units = (state->polygonOffset.option == r_polygonoffset_standard ? bound(0, gl_brush_polygonoffset.value, 25.0) : 1);
		qbool enabled = (state->polygonOffset.option == r_polygonoffset_standard || state->polygonOffset.option == r_polygonoffset_outlines) && units > 0;

		if (enabled) {
			if (!current->polygonOffset.fillEnabled) {
				glEnable(GL_POLYGON_OFFSET_FILL);
				GL_LogAPICall("glEnable(GL_POLYGON_OFFSET_FILL)");
				current->polygonOffset.fillEnabled = true;
			}
			if (!current->polygonOffset.lineEnabled) {
				glEnable(GL_POLYGON_OFFSET_LINE);
				GL_LogAPICall("glEnable(GL_POLYGON_OFFSET_LINE)");
				current->polygonOffset.lineEnabled = true;
			}

			if (current->polygonOffset.factor != factor || current->polygonOffset.units != units) {
				glPolygonOffset(factor, units);
				GL_LogAPICall("glPolygonOffset(factor %f, units %f)", factor, units);

				current->polygonOffset.factor = factor;
				current->polygonOffset.units = units;
			}
		}
		else {
			if (current->polygonOffset.fillEnabled) {
				glDisable(GL_POLYGON_OFFSET_FILL);
				GL_LogAPICall("glDisable(GL_POLYGON_OFFSET_FILL)");
				current->polygonOffset.fillEnabled = false;
			}
			if (current->polygonOffset.lineEnabled) {
				glDisable(GL_POLYGON_OFFSET_LINE);
				GL_LogAPICall("glDisable(GL_POLYGON_OFFSET_LINE)");
				current->polygonOffset.lineEnabled = false;
			}
		}

		gl_brush_polygonoffset.modified = false;
		current->polygonOffset.option = state->polygonOffset.option;
	}
	if (state->polygonMode != current->polygonMode) {
		glPolygonMode(GL_FRONT_AND_BACK, glPolygonModeValues[current->polygonMode = state->polygonMode]);

		GL_LogAPICall("glPolygonMode(%s)", txtPolygonModeValues[state->polygonMode]);
	}
	if (state->clearColor[0] != current->clearColor[0] || state->clearColor[1] != current->clearColor[1] || state->clearColor[2] != current->clearColor[2] || state->clearColor[3] != current->clearColor[3]) {
		glClearColor(
			current->clearColor[0] = state->clearColor[0],
			current->clearColor[1] = state->clearColor[1],
			current->clearColor[2] = state->clearColor[2],
			current->clearColor[3] = state->clearColor[3]
		);
		GL_LogAPICall("glClearColor(...)");
	}
	GL_ApplySimpleToggle(state, current, blendingEnabled, GL_BLEND);
	if (state->colorMask[0] != current->colorMask[0] || state->colorMask[1] != current->colorMask[1] || state->colorMask[2] != current->colorMask[2] || state->colorMask[3] != current->colorMask[3]) {
		glColorMask(
			(current->colorMask[0] = state->colorMask[0]) ? GL_TRUE : GL_FALSE,
			(current->colorMask[1] = state->colorMask[1]) ? GL_TRUE : GL_FALSE,
			(current->colorMask[2] = state->colorMask[2]) ? GL_TRUE : GL_FALSE,
			(current->colorMask[3] = state->colorMask[3]) ? GL_TRUE : GL_FALSE
		);
		GL_LogAPICall("glColorMask(%s,%s,%s,%s)", state->colorMask[0] ? "on" : "off", state->colorMask[1] ? "on" : "off", state->colorMask[2] ? "on" : "off", state->colorMask[3] ? "on" : "off");
	}
	if (state->vao_id != currentVAO) {
		R_BindVertexArray(state->vao_id);
	}

#ifdef WITH_OPENGL_TRACE
	GL_DebugState();
#endif
	GL_LeaveTracedRegion(true);
}

static void GLC_ApplyRenderingState(r_state_id id)
{
	rendering_state_t* current = &opengl.rendering_state;
	rendering_state_t* state = &states[id];
	int i;

	// Alpha-testing
	GL_ApplySimpleToggle(state, current, alphaTesting.enabled, GL_ALPHA_TEST);
	if (current->alphaTesting.enabled && (state->alphaTesting.func != current->alphaTesting.func || state->alphaTesting.value != current->alphaTesting.value)) {
		glAlphaFunc(
			glAlphaTestModeValues[current->alphaTesting.func = state->alphaTesting.func],
			current->alphaTesting.value = state->alphaTesting.value
		);
		GL_LogAPICall("glAlphaFunc(%s %f)", txtAlphaTestModeValues[state->alphaTesting.func], state->alphaTesting.value);
	}

	// Texture units
	for (i = 0; i < sizeof(current->textureUnits) / sizeof(current->textureUnits[0]) && i < glConfig.texture_units; ++i) {
		if (state->textureUnits[i].enabled != current->textureUnits[i].enabled) {
			if ((current->textureUnits[i].enabled = state->textureUnits[i].enabled)) {
				GL_SelectTexture(GL_TEXTURE0 + i);
				glEnable(GL_TEXTURE_2D);
				GL_LogAPICall("Enabled texturing on unit %d", i);
				if (state->textureUnits[i].mode != current->textureUnits[i].mode) {
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, glTextureEnvModeValues[current->textureUnits[i].mode = state->textureUnits[i].mode]);
					GL_LogAPICall("texture unit mode[%d] = %s", i, txtTextureEnvModeValues[state->textureUnits[i].mode]);
				}
			}
			else {
				GL_SelectTexture(GL_TEXTURE0 + i);
				glDisable(GL_TEXTURE_2D);
				GL_LogAPICall("Disabled texturing on unit %d", i);
			}
		}
		else if (current->textureUnits[i].enabled && state->textureUnits[i].mode != current->textureUnits[i].mode) {
			GL_SelectTexture(GL_TEXTURE0 + i);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, glTextureEnvModeValues[current->textureUnits[i].mode = state->textureUnits[i].mode]);
			GL_LogAPICall("texture unit mode[%d] = %s", i, txtTextureEnvModeValues[state->textureUnits[i].mode]);
		}
	}

	// Color
	if (!current->colorValid || current->color[0] != state->color[0] || current->color[1] != state->color[1] || current->color[2] != state->color[2] || current->color[3] != state->color[3]) {
		glColor4f(
			current->color[0] = state->color[0],
			current->color[1] = state->color[1],
			current->color[2] = state->color[2],
			current->color[3] = state->color[3]
		);
		current->colorValid = true;
	}

	GL_ApplyRenderingState(id);
}

// vid_common_gl.c
// gl_texture.c
GLuint GL_TextureNameFromReference(texture_ref ref);
GLenum GL_TextureTargetFromReference(texture_ref ref);

static void GL_BindTextureUnitImpl(GLuint unit, texture_ref reference, qbool always_select_unit)
{
	int unit_num = unit - GL_TEXTURE0;
	GLuint texture = GL_TextureNameFromReference(reference);
	GLenum targetType = GL_TextureTargetFromReference(reference);

	if (unit_num >= 0 && unit_num < sizeof(bound_arrays) / sizeof(bound_arrays[0])) {
		if (targetType == GL_TEXTURE_2D_ARRAY) {
			if (bound_arrays[unit_num] == texture) {
				if (always_select_unit) {
					GL_SelectTexture(unit);
				}
				return;
			}
		}
		else if (targetType == GL_TEXTURE_2D) {
			if (bound_textures[unit_num] == texture) {
				if (always_select_unit) {
					GL_SelectTexture(unit);
				}
				return;
			}
		}
	}

	GL_SelectTexture(unit);
	GL_BindTexture(targetType, texture, true);
	return;
}

static void GLM_ApplyRenderingState(r_state_id id)
{
	GL_ApplyRenderingState(id);
}

void GL_EnsureTextureUnitBound(int unit, texture_ref reference)
{
	GL_BindTextureUnitImpl(GL_TEXTURE0 + unit, reference, false);
}

void GL_BindTextureUnit(GLuint unit, texture_ref reference)
{
	GL_BindTextureUnitImpl(unit, reference, true);
}

void R_Viewport(int x, int y, int width, int height)
{
	if (x != currentViewportX || y != currentViewportY || width != currentViewportWidth || height != currentViewportHeight) {
		renderer.Viewport(x, y, width, height);

		currentViewportX = x;
		currentViewportY = y;
		currentViewportWidth = width;
		currentViewportHeight = height;
	}
}

void GL_GetViewport(int* view)
{
	view[0] = opengl.rendering_state.currentViewportX;
	view[1] = opengl.rendering_state.currentViewportY;
	view[2] = opengl.rendering_state.currentViewportWidth;
	view[3] = opengl.rendering_state.currentViewportHeight;
}

void GL_InitialiseState(void)
{
	glDepthFunctions[r_depthfunc_less] = GL_LESS;
	glDepthFunctions[r_depthfunc_equal] = GL_EQUAL;
	glDepthFunctions[r_depthfunc_lessorequal] = GL_LEQUAL;
	glCullFaceValues[r_cullface_back] = GL_BACK;
	glCullFaceValues[r_cullface_front] = GL_FRONT;
	glBlendFuncValuesSource[r_blendfunc_overwrite] = GL_ONE;
	glBlendFuncValuesSource[r_blendfunc_additive_blending] = GL_ONE;
	glBlendFuncValuesSource[r_blendfunc_premultiplied_alpha] = GL_ONE;
	glBlendFuncValuesSource[r_blendfunc_src_dst_color_dest_zero] = GL_DST_COLOR;
	glBlendFuncValuesSource[r_blendfunc_src_dst_color_dest_one] = GL_DST_COLOR;
	glBlendFuncValuesSource[r_blendfunc_src_dst_color_dest_src_color] = GL_DST_COLOR;
	glBlendFuncValuesSource[r_blendfunc_src_zero_dest_one_minus_src_color] = GL_ZERO;
	glBlendFuncValuesSource[r_blendfunc_src_zero_dest_src_color] = GL_ZERO;
	glBlendFuncValuesSource[r_blendfunc_src_one_dest_zero] = GL_ONE;
	glBlendFuncValuesSource[r_blendfunc_src_zero_dest_one] = GL_ZERO;
	glBlendFuncValuesDestination[r_blendfunc_overwrite] = GL_ZERO;
	glBlendFuncValuesDestination[r_blendfunc_additive_blending] = GL_ONE;
	glBlendFuncValuesDestination[r_blendfunc_premultiplied_alpha] = GL_ONE_MINUS_SRC_ALPHA;
	glBlendFuncValuesDestination[r_blendfunc_src_dst_color_dest_zero] = GL_ZERO;
	glBlendFuncValuesDestination[r_blendfunc_src_dst_color_dest_one] = GL_ONE;
	glBlendFuncValuesDestination[r_blendfunc_src_dst_color_dest_src_color] = GL_SRC_COLOR;
	glBlendFuncValuesDestination[r_blendfunc_src_zero_dest_one_minus_src_color] = GL_ONE_MINUS_SRC_COLOR;
	glBlendFuncValuesDestination[r_blendfunc_src_zero_dest_src_color] = GL_SRC_COLOR;
	glBlendFuncValuesDestination[r_blendfunc_src_one_dest_zero] = GL_ZERO;
	glBlendFuncValuesDestination[r_blendfunc_src_zero_dest_one] = GL_ONE;
	glPolygonModeValues[r_polygonmode_fill] = GL_FILL;
	glPolygonModeValues[r_polygonmode_line] = GL_LINE;
	glAlphaTestModeValues[r_alphatest_func_always] = GL_ALWAYS;
	glAlphaTestModeValues[r_alphatest_func_greater] = GL_GREATER;
	glTextureEnvModeValues[r_texunit_mode_blend] = GL_BLEND;
	glTextureEnvModeValues[r_texunit_mode_replace] = GL_REPLACE;
	glTextureEnvModeValues[r_texunit_mode_modulate] = GL_MODULATE;
	glTextureEnvModeValues[r_texunit_mode_decal] = GL_DECAL;
	glTextureEnvModeValues[r_texunit_mode_add] = GL_ADD;
#ifdef WITH_OPENGL_TRACE
	txtDepthFunctions[r_depthfunc_less] = "<";
	txtDepthFunctions[r_depthfunc_equal] = "=";
	txtDepthFunctions[r_depthfunc_lessorequal] = "<=";
	txtCullFaceValues[r_cullface_back] = "back";
	txtCullFaceValues[r_cullface_front] = "front";
	txtBlendFuncNames[r_blendfunc_overwrite] = "overwrite";
	txtBlendFuncNames[r_blendfunc_additive_blending] = "additive";
	txtBlendFuncNames[r_blendfunc_premultiplied_alpha] = "premul-alpha";
	txtBlendFuncNames[r_blendfunc_src_dst_color_dest_zero] = "r_blendfunc_src_dst_color_dest_zero";
	txtBlendFuncNames[r_blendfunc_src_dst_color_dest_one] = "r_blendfunc_src_dst_color_dest_one";
	txtBlendFuncNames[r_blendfunc_src_dst_color_dest_src_color] = "r_blendfunc_src_dst_color_dest_src_color";
	txtBlendFuncNames[r_blendfunc_src_zero_dest_one_minus_src_color] = "r_blendfunc_src_zero_dest_one_minus_src_color";
	txtBlendFuncNames[r_blendfunc_src_zero_dest_src_color] = "r_blendfunc_src_zero_dest_src_color";
	txtBlendFuncNames[r_blendfunc_src_one_dest_zero] = "r_blendfunc_src_one_dest_zero";
	txtBlendFuncNames[r_blendfunc_src_zero_dest_one] = "r_blendfunc_src_zero_dest_one";
	txtPolygonModeValues[r_polygonmode_fill] = "fill";
	txtPolygonModeValues[r_polygonmode_line] = "line";
	txtAlphaTestModeValues[r_alphatest_func_always] = "always";
	txtAlphaTestModeValues[r_alphatest_func_greater] = "greater";
	txtTextureEnvModeValues[r_texunit_mode_blend] = "blend";
	txtTextureEnvModeValues[r_texunit_mode_replace] = "replace";
	txtTextureEnvModeValues[r_texunit_mode_modulate] = "modulate";
	txtTextureEnvModeValues[r_texunit_mode_decal] = "decal";
	txtTextureEnvModeValues[r_texunit_mode_add] = "add";
#endif

	R_InitRenderingState(r_state_default_opengl, false, "opengl", vao_none);
	R_ApplyRenderingState(r_state_default_opengl);
	R_InitialiseBrushModelStates();
	R_InitialiseStates();
	R_Initialise2DStates();
	R_InitialiseEntityStates();
	GLC_InitialiseSkyStates();
	R_InitialiseWorldStates();

	GLM_SetIdentityMatrix(GLM_ProjectionMatrix());
	GLM_SetIdentityMatrix(GLM_ModelviewMatrix());

	memset(bound_textures, 0, sizeof(bound_textures));
	memset(bound_arrays, 0, sizeof(bound_arrays));
	memset(bound_images, 0, sizeof(bound_images));

	buffers.InitialiseState();
	GLM_InitialiseProgramState();
}

// These functions taken from gl_texture.c
static void GL_BindTexture(GLenum targetType, GLuint texnum, qbool warning)
{
	assert(targetType);
	assert(texnum);

#ifdef GL_PARANOIA
	if (warning && !glIsTexture(texnum)) {
		Con_Printf("ERROR: Non-texture %d passed to GL_BindTexture\n", texnum);
		return;
	}

	GL_ProcessErrors("glBindTexture/Prior");
#endif

	if (targetType == GL_TEXTURE_2D) {
		if (bound_textures[currentTextureUnit - GL_TEXTURE0] == texnum) {
			return;
		}

		bound_textures[currentTextureUnit - GL_TEXTURE0] = texnum;
		glBindTexture(GL_TEXTURE_2D, texnum);
		GL_LogAPICall("glBindTexture(unit=GL_TEXTURE%d, target=GL_TEXTURE_2D, texnum=%u[%s])", currentTextureUnit - GL_TEXTURE0, texnum, GL_TextureIdentifierByGLReference(texnum));
	}
	else if (targetType == GL_TEXTURE_2D_ARRAY) {
		if (bound_arrays[currentTextureUnit - GL_TEXTURE0] == texnum) {
			return;
		}

		bound_arrays[currentTextureUnit - GL_TEXTURE0] = texnum;
		glBindTexture(GL_TEXTURE_2D_ARRAY, texnum);
		GL_LogAPICall("glBindTexture(unit=GL_TEXTURE%d, target=GL_TEXTURE_2D_ARRAY, texnum=%u[%s])", currentTextureUnit - GL_TEXTURE0, texnum, GL_TextureIdentifierByGLReference(texnum));
	}
	else {
		// No caching...
		glBindTexture(targetType, texnum);
		GL_LogAPICall("glBindTexture(unit=GL_TEXTURE%d, target=<other>, texnum=%u[%s])", currentTextureUnit - GL_TEXTURE0, texnum, GL_TextureIdentifierByGLReference(texnum));
	}

	++frameStats.texture_binds;

#ifdef GL_PARANOIA
	GL_ProcessErrors("glBindTexture/After");
#endif
}

void GL_SelectTexture(GLenum textureUnit)
{
	if (textureUnit == currentTextureUnit) {
		return;
	}

#ifdef GL_PARANOIA
	GL_ProcessErrors("glActiveTexture/Prior");
#endif
	if (qglActiveTexture) {
		qglActiveTexture(textureUnit);
	}
#ifdef GL_PARANOIA
	GL_ProcessErrors("glActiveTexture/After");
#endif

	currentTextureUnit = textureUnit;
	GL_LogAPICall("glActiveTexture(GL_TEXTURE%d)", textureUnit - GL_TEXTURE0);
}

void GL_InitTextureState(void)
{
	int i;

	// Multi texture.
	currentTextureUnit = GL_TEXTURE0;

	memset(bound_textures, 0, sizeof(bound_textures));
	memset(bound_arrays, 0, sizeof(bound_arrays));
	for (i = 0; i < sizeof(opengl.rendering_state.textureUnits) / sizeof(opengl.rendering_state.textureUnits[0]); ++i) {
		opengl.rendering_state.textureUnits[i].enabled = false;
		opengl.rendering_state.textureUnits[i].mode = GL_MODULATE;
	}
}

void GL_InvalidateTextureReferences(GLuint texture)
{
	int i;

	// glDeleteTextures(texture) has been called - same reference might be re-used in future
	// If a texture that is currently bound is deleted, the binding reverts to 0 (the default texture)
	for (i = 0; i < sizeof(bound_textures) / sizeof(bound_textures[0]); ++i) {
		if (bound_textures[i] == texture) {
			bound_textures[i] = 0;
		}
		if (bound_arrays[i] == texture) {
			bound_arrays[i] = 0;
		}
	}

	for (i = 0; i < sizeof(bound_images) / sizeof(bound_images[0]); ++i) {
		if (bound_images[i].texture == texture) {
			bound_images[i].texture = 0;
		}
	}
}

void GL_BindTextures(int first, int count, texture_ref* textures)
{
	int i;

	if (qglBindTextures) {
		GLuint glTextures[MAX_LOGGED_TEXTURE_UNITS];
		qbool already_bound = true;

		count = min(count, MAX_LOGGED_TEXTURE_UNITS);
		for (i = 0; i < count; ++i) {
			glTextures[i] = GL_TextureNameFromReference(textures[i]);
			if (i + first < MAX_LOGGED_TEXTURE_UNITS) {
				GLenum target = GL_TextureTargetFromReference(textures[i]);

				if (target == GL_TEXTURE_2D_ARRAY || glTextures[i] == 0) {
					already_bound &= (bound_arrays[i + first] == glTextures[i]);
					bound_arrays[i + first] = glTextures[i];
				}
				if (target == GL_TEXTURE_2D || glTextures[i] == 0) {
					already_bound &= (bound_textures[i + first] == glTextures[i]);
					bound_textures[i + first] = glTextures[i];
				}
			}
		}

		if (!already_bound) {
			qglBindTextures(first, count, glTextures);
		}
	}
	else {
		for (i = 0; i < count; ++i) {
			if (R_TextureReferenceIsValid(textures[i])) {
				R_TextureUnitBind(first + i, textures[i]);
			}
		}
	}

#ifdef WITH_OPENGL_TRACE
	if (GL_LoggingEnabled()) {
		static char temp[1024];

		temp[0] = '\0';
		for (i = 0; i < count; ++i) {
			if (i) {
				strlcat(temp, ",", sizeof(temp));
			}
			strlcat(temp, R_TextureIdentifier(textures[i]), sizeof(temp));
		}
		GL_LogAPICall("glBindTextures(GL_TEXTURE%d, %d[%s])", first, count, temp);
	}
#endif
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

	qglBindImageTexture(unit, glRef, level, layered, layer, access, format);
}

#ifdef WITH_OPENGL_TRACE
void GL_PrintState(FILE* debug_frame_out, int debug_frame_depth)
{
	int i;

	debug_frame_depth += 7;

	if (debug_frame_out) {
		rendering_state_t* current = &opengl.rendering_state;

		fprintf(debug_frame_out, "%.*s <state-dump>\n", debug_frame_depth, "                                                          ");
		fprintf(debug_frame_out, "%.*s   Z-Buffer: %s, func %s range %f=>%f [mask %s]\n", debug_frame_depth, "                                                          ", current->depth.test_enabled ? "on" : "off", txtDepthFunctions[current->depth.func], current->depth.nearRange, current->depth.farRange, current->depth.mask_enabled ? "on" : "off");
		fprintf(debug_frame_out, "%.*s   Cull-face: %s, mode %s\n", debug_frame_depth, "                                                          ", current->cullface.enabled ? "enabled" : "disabled", txtCullFaceValues[current->cullface.mode]);
		fprintf(debug_frame_out, "%.*s   Blending: %s, func %s\n", debug_frame_depth, "                                                          ", current->blendingEnabled ? "enabled" : "disabled", txtBlendFuncNames[current->blendFunc]);
		fprintf(debug_frame_out, "%.*s   AlphaTest: %s, func %s(%f)\n", debug_frame_depth, "                                                          ", current->alphaTesting.enabled ? "on" : "off", txtAlphaTestModeValues[current->alphaTesting.func], current->alphaTesting.value);
		fprintf(debug_frame_out, "%.*s   Texture units: [", debug_frame_depth, "                                                          ");
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
		fprintf(debug_frame_out, "]\n");
		fprintf(debug_frame_out, "%.*s   glPolygonMode: %s\n", debug_frame_depth, "                                                          ", txtPolygonModeValues[current->polygonMode]);
		fprintf(debug_frame_out, "%.*s   vao: %s\n", debug_frame_depth, "                                                          ", vaoNames[currentVAO]);
		if (renderer.VAOBound()) {
			if (R_UseImmediateOpenGL()) {
				GLC_PrintVAOState(debug_frame_out, debug_frame_depth, currentVAO);
			}
		}
		buffers.PrintState(debug_frame_out, debug_frame_depth);
		fprintf(debug_frame_out, "%.*s </state-dump>\n", debug_frame_depth, "                                                          ");
	}
}
#endif

qbool GLM_LoadStateFunctions(void)
{
	qbool all_available = true;

	GL_LoadMandatoryFunctionExtension(glActiveTexture, all_available);
	GL_LoadMandatoryFunctionExtension(glBindImageTexture, all_available);

	// 4.4 - binds textures to consecutive texture units
	GL_LoadOptionalFunction(glBindTextures);

	return all_available;
}

void GLC_ClientActiveTexture(GLenum texture_unit)
{
	if (qglClientActiveTexture) {
		qglClientActiveTexture(texture_unit);
	}
	else {
		assert(texture_unit == GL_TEXTURE0);
	}
}

void R_ApplyRenderingState(r_state_id state)
{
	assert(state != r_state_null);
	if (!state) {
		Con_Printf("ERROR: NULL rendering state\n");
		return;
	}
	assert(states[state].initialized);

	if (R_UseImmediateOpenGL()) {
		GLC_ApplyRenderingState(state);
	}
	else if (R_UseModernOpenGL()) {
		GLM_ApplyRenderingState(state);
	}
	else if (R_UseVulkan()) {

	}
}

void R_CustomColor(float r, float g, float b, float a)
{
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
}

void R_CustomLineWidth(float width)
{
	if (width != opengl.rendering_state.line.width) {
		if (R_UseImmediateOpenGL() || R_UseModernOpenGL()) {
			glLineWidth(opengl.rendering_state.line.width = width);
			GL_LogAPICall("glLineWidth(%f)", width);
		}
		else if (R_UseVulkan()) {
			// Requires VK_DYNAMIC_STATE_LINE_WIDTH
			// Requires wide lines feature (if not, must be 1)

			// vkCmdSetLineWidth(commandBuffer, width)
			opengl.rendering_state.line.width = width;
		}
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
	if (R_UseImmediateOpenGL()) {
		glEnable(GL_SCISSOR_TEST);
		glScissor(x, y, width, height);
	}
}

void R_DisableScissorTest(void)
{
	if (R_UseImmediateOpenGL()) {
		glDisable(GL_SCISSOR_TEST);
	}
}

void R_ClearColor(float r, float g, float b, float a)
{
	if (R_UseImmediateOpenGL()) {
		glClearColor(r, g, b, a);
	}
	else if (R_UseModernOpenGL()) {
		glClearColor(r, g, b, a);
	}
}

void R_GLC_TextureUnitSet(rendering_state_t* state, int index, qbool enabled, r_texunit_mode_t mode)
{
	state->textureUnits[index].enabled = enabled;
	state->textureUnits[index].mode = mode;
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

	renderer.InitialiseVAOState();
}

qbool R_VertexArrayCreated(r_vao_id vao)
{
	return renderer.VertexArrayCreated(vao);
}

void R_BindVertexArray(r_vao_id vao)
{
	if (currentVAO != vao) {
		assert(vao == vao_none || R_VertexArrayCreated(vao));

		renderer.BindVertexArray(vao);

		currentVAO = vao;
		GL_LogAPICall("BindVertexArray(%s)", vaoNames[vao]);
	}
}

void R_GenVertexArray(r_vao_id vao)
{
	renderer.GenVertexArray(vao, vaoNames[vao]);
}

void R_DeleteVAOs(void)
{
	renderer.DeleteVAOs();
}

qbool R_VAOBound(void)
{
	return currentVAO != vao_none;
}

void R_BindVertexArrayElementBuffer(buffer_ref ref)
{
	if (currentVAO != vao_none) {
		renderer.BindVertexArrayElementBuffer(currentVAO, ref);
	}
}

void R_GLC_VertexPointer(buffer_ref buf, qbool enabled, int size, GLenum type, int stride, void* pointer_or_offset)
{
	if (enabled) {
		if (GL_BufferReferenceIsValid(buf)) {
			buffers.Bind(buf);
		}
		else {
			buffers.UnBind(GL_ARRAY_BUFFER);
		}
		glVertexPointer(size, type, stride, pointer_or_offset);
		GL_LogAPICall("glVertexPointer(size %d, type %s, stride %d, ptr %p)", size, type == GL_FLOAT ? "FLOAT" : type == GL_UNSIGNED_BYTE ? "UBYTE" : "???", stride, pointer_or_offset);
		if (!opengl.rendering_state.glc_vertex_array_enabled) {
			glEnableClientState(GL_VERTEX_ARRAY);
			GL_LogAPICall("glEnableClientState(GL_VERTEX_ARRAY)");
			opengl.rendering_state.glc_vertex_array_enabled = true;
		}
	}
	else if (!enabled && opengl.rendering_state.glc_vertex_array_enabled) {
		glDisableClientState(GL_VERTEX_ARRAY);
		GL_LogAPICall("glDisableClientState(GL_VERTEX_ARRAY)");
		opengl.rendering_state.glc_vertex_array_enabled = false;
	}
}

void R_GLC_ColorPointer(buffer_ref buf, qbool enabled, int size, GLenum type, int stride, void* pointer_or_offset)
{
	if (enabled) {
		if (GL_BufferReferenceIsValid(buf)) {
			buffers.Bind(buf);
		}
		else {
			buffers.UnBind(GL_ARRAY_BUFFER);
		}
		glColorPointer(size, type, stride, pointer_or_offset);
		GL_LogAPICall("glColorPointer(size %d, type %s, stride %d, ptr %p)", size, type == GL_FLOAT ? "FLOAT" : type == GL_UNSIGNED_BYTE ? "UBYTE" : "???", stride, pointer_or_offset);
		if (!opengl.rendering_state.glc_color_array_enabled) {
			glEnableClientState(GL_COLOR_ARRAY);
			GL_LogAPICall("glEnableClientState(GL_COLOR_ARRAY)");
			opengl.rendering_state.colorValid = false;
			opengl.rendering_state.glc_color_array_enabled = true;
		}
	}
	else if (!enabled && opengl.rendering_state.glc_color_array_enabled) {
		glDisableClientState(GL_COLOR_ARRAY);
		GL_LogAPICall("glDisableClientState(GL_COLOR_ARRAY)");
		opengl.rendering_state.colorValid = false;
		opengl.rendering_state.glc_color_array_enabled = false;
	}
}

void R_GLC_TexturePointer(buffer_ref buf, int unit, qbool enabled, int size, GLenum type, int stride, void* pointer_or_offset)
{
	if (unit < 0 || unit >= sizeof(opengl.rendering_state.glc_texture_array_enabled) / sizeof(opengl.rendering_state.glc_texture_array_enabled[0])) {
		return;
	}

	if (enabled) {
		if (GL_BufferReferenceIsValid(buf)) {
			buffers.Bind(buf);
		}
		else {
			buffers.UnBind(GL_ARRAY_BUFFER);
		}
		GLC_ClientActiveTexture(GL_TEXTURE0 + unit);
		glTexCoordPointer(size, type, stride, pointer_or_offset);
		GL_LogAPICall("glTexCoordPointer(size %d, type %s, stride %d, ptr %p)", size, type == GL_FLOAT ? "FLOAT" : type == GL_UNSIGNED_BYTE ? "UBYTE" : "???", stride, pointer_or_offset);
		if (!opengl.rendering_state.glc_texture_array_enabled[unit]) {
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			GL_LogAPICall("glEnableClientState(GL_TEXTURE_COORD_ARRAY)");
			opengl.rendering_state.glc_texture_array_enabled[unit] = true;
		}
	}
	else if (!enabled && opengl.rendering_state.glc_texture_array_enabled[unit]) {
		GLC_ClientActiveTexture(GL_TEXTURE0 + unit);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		GL_LogAPICall("glDisableClientState(GL_TEXTURE_COORD_ARRAY)");
		opengl.rendering_state.glc_texture_array_enabled[unit] = false;
	}
}
