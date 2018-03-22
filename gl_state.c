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

void GL_BindBuffer(buffer_ref ref);
void GL_SetElementArrayBuffer(buffer_ref buffer);
const buffer_ref null_buffer_reference = { 0 };

// FIXME: currentWidth & currentHeight should be initialised to dimensions of window
GLint currentViewportX, currentViewportY;
GLsizei currentViewportWidth, currentViewportHeight;

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

typedef struct {
	GLenum currentTextureUnit;
	GLuint bound_textures[MAX_LOGGED_TEXTURE_UNITS];
	GLuint bound_arrays[MAX_LOGGED_TEXTURE_UNITS];
	qbool texunitenabled[MAX_LOGGED_TEXTURE_UNITS];
	GLenum unit_texture_mode[MAX_LOGGED_TEXTURE_UNITS];
	image_unit_binding_t bound_images[MAX_LOGGED_IMAGE_UNITS];
} texture_state_t;

typedef struct {
	rendering_state_t rendering_state;
	texture_state_t textures;
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

void R_InitRenderingState(rendering_state_t* state, qbool default_state)
{
	SDL_Window* window = SDL_GL_GetCurrentWindow();
	int i;

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
}

void R_Init3DSpriteRenderingState(rendering_state_t* state)
{
	R_InitRenderingState(state, true);

	state->fog.enabled = false;
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	state->cullface.enabled = false;
	state->alphaTesting.enabled = true;
	state->alphaTesting.func = r_alphatest_func_greater;
	state->alphaTesting.value = 0.333f;
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

void GL_ApplyRenderingState(rendering_state_t* state)
{
	extern cvar_t gl_brush_polygonoffset;
	rendering_state_t* current = &opengl.rendering_state;

	if (state->depth.func != current->depth.func) {
		glDepthFunc(glDepthFunctions[current->depth.func = state->depth.func]);
	}
	if (state->depth.nearRange != current->depth.nearRange || state->depth.farRange != current->depth.farRange) {
		glDepthRange(
			current->depth.nearRange = state->depth.nearRange,
			current->depth.farRange = state->depth.farRange
		);
	}
	if (state->cullface.mode != current->cullface.mode) {
		glCullFace(glCullFaceValues[current->cullface.mode = state->cullface.mode]);
	}
	if (state->blendFunc != current->blendFunc) {
		current->blendFunc = state->blendFunc;
		glBlendFunc(
			glBlendFuncValuesSource[state->blendFunc],
			glBlendFuncValuesDestination[state->blendFunc]
		);
	}
	if (state->line.width != current->line.width) {
		glLineWidth(current->line.width = state->line.width);
	}
	GL_ApplySimpleToggle(state, current, depth.test_enabled, GL_DEPTH_TEST);
	if (state->depth.mask_enabled != current->depth.mask_enabled) {
		glDepthMask((current->depth.mask_enabled = state->depth.mask_enabled) ? GL_TRUE : GL_FALSE);
	}
	GL_ApplySimpleToggle(state, current, framebuffer_srgb, GL_FRAMEBUFFER_SRGB);
	GL_ApplySimpleToggle(state, current, cullface.enabled, GL_CULL_FACE);
	GL_ApplySimpleToggle(state, current, line.smooth, GL_LINE_SMOOTH);
	GL_ApplySimpleToggle(state, current, fog.enabled, GL_FOG);
	if (state->polygonOffset.option != current->polygonOffset.option || gl_brush_polygonoffset.modified) {
		float factor = (state->polygonOffset.option == r_polygonoffset_standard ? 0.05 : 1);
		float units = (state->polygonOffset.option == r_polygonoffset_standard ? bound(0, gl_brush_polygonoffset.value, 25.0) : 1);
		qbool enabled = (state->polygonOffset.option == r_polygonoffset_standard || state->polygonOffset.option == r_polygonoffset_outlines) && units > 0;

		if (enabled) {
			if (!current->polygonOffset.fillEnabled) {
				glEnable(GL_POLYGON_OFFSET_FILL);
				current->polygonOffset.fillEnabled = true;
			}
			if (!current->polygonOffset.lineEnabled) {
				glEnable(GL_POLYGON_OFFSET_LINE);
				current->polygonOffset.lineEnabled = true;
			}

			if (current->polygonOffset.factor != factor || current->polygonOffset.units != units) {
				glPolygonOffset(factor, units);

				current->polygonOffset.factor = factor;
				current->polygonOffset.units = units;
			}
		}
		else {
			if (current->polygonOffset.fillEnabled) {
				glDisable(GL_POLYGON_OFFSET_FILL);
				current->polygonOffset.fillEnabled = false;
			}
			if (current->polygonOffset.lineEnabled) {
				glDisable(GL_POLYGON_OFFSET_LINE);
				current->polygonOffset.lineEnabled = false;
			}
		}

		gl_brush_polygonoffset.modified = false;
		current->polygonOffset.option = state->polygonOffset.option;
	}
	if (state->polygonMode != current->polygonMode) {
		glPolygonMode(GL_FRONT_AND_BACK, glPolygonModeValues[current->polygonMode = state->polygonMode]);

		GL_LogAPICall("glPolygonMode(%s)", state->polygonMode == r_polygonmode_fill ? "fill" : (state->polygonMode == r_polygonmode_line ? "lines" : "???"));
	}
	if (state->clearColor[0] != current->clearColor[0] || state->clearColor[1] != current->clearColor[1] || state->clearColor[2] != current->clearColor[2] || state->clearColor[3] != current->clearColor[3]) {
		glClearColor(
			current->clearColor[0] = state->clearColor[0],
			current->clearColor[1] = state->clearColor[1],
			current->clearColor[2] = state->clearColor[2],
			current->clearColor[3] = state->clearColor[3]
		);
	}
	GL_ApplySimpleToggle(state, current, blendingEnabled, GL_BLEND);
	if (state->colorMask[0] != current->colorMask[0] || state->colorMask[1] != current->colorMask[1] || state->colorMask[2] != current->colorMask[2] || state->colorMask[3] != current->colorMask[3]) {
		glColorMask(
			(current->colorMask[0] = state->colorMask[0]) ? GL_TRUE : GL_FALSE,
			(current->colorMask[1] = state->colorMask[1]) ? GL_TRUE : GL_FALSE,
			(current->colorMask[2] = state->colorMask[2]) ? GL_TRUE : GL_FALSE,
			(current->colorMask[3] = state->colorMask[3]) ? GL_TRUE : GL_FALSE
		);
	}

	if (GL_UseImmediateMode()) {
		int i;

		// Alpha-testing
		GL_ApplySimpleToggle(state, current, alphaTesting.enabled, GL_ALPHA_TEST);
		if (current->alphaTesting.enabled && (state->alphaTesting.func != current->alphaTesting.func || state->alphaTesting.value != current->alphaTesting.value)) {
			glAlphaFunc(
				glAlphaTestModeValues[current->alphaTesting.func = state->alphaTesting.func],
				current->alphaTesting.value = state->alphaTesting.value
			);
		}

		// Texture units
		for (i = 0; i < sizeof(current->textureUnits) / sizeof(current->textureUnits[0]); ++i) {
			if (state->textureUnits[i].enabled != current->textureUnits[i].enabled) {
				GL_SelectTexture(GL_TEXTURE0 + i);
				if ((current->textureUnits[i].enabled = state->textureUnits[i].enabled)) {
					glEnable(GL_TEXTURE_2D);
					if (state->textureUnits[i].mode != current->textureUnits[i].mode) {
						glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, glTextureEnvModeValues[current->textureUnits[i].mode = state->textureUnits[i].mode]);
					}
				}
				else {
					glDisable(GL_TEXTURE_2D);
				}
			}
		}

		// Color
		if (current->color[0] != state->color[0] || current->color[1] != state->color[1] || current->color[2] != state->color[2] || current->color[3] != state->color[3]) {
			glColor4fv(state->color);
			current->color[0] = state->color[0];
			current->color[1] = state->color[1];
			current->color[2] = state->color[2];
			current->color[3] = state->color[3];
		}
	}
}

//static int old_alphablend_flags = 0;
static void GLC_DisableTextureUnitOnwards(int first);

// vid_common_gl.c
#ifdef WITH_OPENGL_TRACE
static const char* TexEnvName(GLenum mode)
{
	switch (mode) {
	case GL_MODULATE:
		return "GL_MODULATE";
	case GL_REPLACE:
		return "GL_REPLACE";
	case GL_BLEND:
		return "GL_BLEND";
	case GL_DECAL:
		return "GL_DECAL";
	case GL_ADD:
		return "GL_ADD";
	default:
		return "???";
	}
}
#endif

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

void GL_EnsureTextureUnitBound(GLuint unit, texture_ref reference)
{
	GL_BindTextureUnitImpl(unit, reference, false);
}

void GL_BindTextureUnit(GLuint unit, texture_ref reference)
{
	GL_BindTextureUnitImpl(unit, reference, true);
}

void GL_Viewport(int x, int y, int width, int height)
{
	if (x != currentViewportX || y != currentViewportY || width != currentViewportWidth || height != currentViewportHeight) {
		glViewport(x, y, width, height);

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

	R_InitRenderingState(&opengl.rendering_state, false);
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

	GL_InitialiseBufferState();
	GL_InitialiseProgramState();
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
		GL_LogAPICall("glBindTexture(unit=GL_TEXTURE%d, target=GL_TEXTURE_2D, texnum=%u[%s])", currentTextureUnit - GL_TEXTURE0, texnum, GL_UseGLSL() ? "" : GL_TextureIdentifierByGLReference(texnum));
	}
	else if (targetType == GL_TEXTURE_2D_ARRAY) {
		if (bound_arrays[currentTextureUnit - GL_TEXTURE0] == texnum) {
			return;
		}

		bound_arrays[currentTextureUnit - GL_TEXTURE0] = texnum;
		glBindTexture(GL_TEXTURE_2D_ARRAY, texnum);
		GL_LogAPICall("glBindTexture(unit=GL_TEXTURE%d, target=GL_TEXTURE_2D_ARRAY, texnum=%u[%s])", currentTextureUnit - GL_TEXTURE0, texnum, GL_UseGLSL() ? "" : GL_TextureIdentifierByGLReference(texnum));
	}
	else {
		// No caching...
		glBindTexture(targetType, texnum);
		GL_LogAPICall("glBindTexture(unit=GL_TEXTURE%d, target=<other>, texnum=%u[%s])", currentTextureUnit - GL_TEXTURE0, texnum, GL_UseGLSL() ? "" : GL_TextureIdentifierByGLReference(texnum));
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
/*
void GLC_DisableAllTexturing(void)
{
	GLC_DisableTextureUnitOnwards(0);
}

void GLC_EnableTMU(GLenum target)
{
	if (GL_UseImmediateMode()) {
		GL_SelectTexture(target);
		glEnable(GL_TEXTURE_2D);
	}
}

void GLC_DisableTMU(GLenum target)
{
	if (GL_UseImmediateMode()) {
		GL_SelectTexture(target);
		glDisable(GL_TEXTURE_2D);
	}
}

void GLC_EnsureTMUEnabled(GLenum textureUnit)
{
	if (GL_UseImmediateMode()) {
		if (texunitenabled[textureUnit - GL_TEXTURE0]) {
			return;
		}

		GLC_EnableTMU(textureUnit);
	}
}

void GLC_EnsureTMUDisabled(GLenum textureUnit)
{
	if (GL_UseImmediateMode()) {
		if (!texunitenabled[textureUnit - GL_TEXTURE0]) {
			return;
		}

		GLC_DisableTMU(textureUnit);
	}
}
*/

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

/*void GL_TextureEnvModeForUnit(GLenum unit, GLenum mode)
{
	if (GL_UseImmediateMode() && mode != unit_texture_mode[unit - GL_TEXTURE0]) {
		GL_SelectTexture(unit);
		GL_TextureEnvMode(mode);
	}
}

void GL_TextureEnvMode(GLenum mode)
{
	if (GL_UseImmediateMode() && mode != unit_texture_mode[currentTextureUnit - GL_TEXTURE0]) {
		GL_LogAPICall("GL_TextureEnvMode(GL_TEXTURE%d, mode=%s)", currentTextureUnit - GL_TEXTURE0, TexEnvName(mode));
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode);
		unit_texture_mode[currentTextureUnit - GL_TEXTURE0] = mode;
	}
}*/
/*
static void GLC_DisableTextureUnitOnwards(int first)
{
	int i;

	for (i = first; i < sizeof(texunitenabled) / sizeof(texunitenabled[0]); ++i) {
		if (texunitenabled[i]) {
			GLC_EnsureTMUDisabled(GL_TEXTURE0 + i);
		}
	}
	GL_SelectTexture(GL_TEXTURE0);
}

void GLC_InitTextureUnitsNoBind1(GLenum envMode0)
{
	GLC_DisableTextureUnitOnwards(1);
	GL_TextureEnvModeForUnit(GL_TEXTURE0, envMode0);
}

void GLC_InitTextureUnits1(texture_ref texture0, GLenum envMode0)
{
	GLC_DisableTextureUnitOnwards(1);

	GLC_EnsureTMUEnabled(GL_TEXTURE0);
	GL_EnsureTextureUnitBound(GL_TEXTURE0, texture0);
	GL_TextureEnvModeForUnit(GL_TEXTURE0, envMode0);
}

void GLC_InitTextureUnits2(texture_ref texture0, GLenum envMode0, texture_ref texture1, GLenum envMode1)
{
	GLC_DisableTextureUnitOnwards(2);

	GLC_EnsureTMUEnabled(GL_TEXTURE0);
	GL_EnsureTextureUnitBound(GL_TEXTURE0, texture0);
	GL_TextureEnvModeForUnit(GL_TEXTURE0, envMode0);

	GLC_EnsureTMUEnabled(GL_TEXTURE1);
	GL_EnsureTextureUnitBound(GL_TEXTURE1, texture1);
	GL_TextureEnvModeForUnit(GL_TEXTURE1, envMode1);
}
*/

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

void GL_BindTextures(GLuint first, GLsizei count, const texture_ref* textures)
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
			if (GL_TextureReferenceIsValid(textures[i])) {
				GL_EnsureTextureUnitBound(GL_TEXTURE0 + first + i, textures[i]);
			}
		}
	}

#ifdef WITH_OPENGL_TRACE
	if (GL_LoggingEnabled())
	{
		static char temp[1024];

		temp[0] = '\0';
		for (i = 0; i < count; ++i) {
			if (i) {
				strlcat(temp, ",", sizeof(temp));
			}
			strlcat(temp, GL_TextureIdentifier(textures[i]), sizeof(temp));
		}
		GL_LogAPICall("glBindTextures(GL_TEXTURE%d, %d[%s])", first, count, temp);
	}
#endif
}

static int glcVertsPerPrimitive = 0;
static int glcBaseVertsPerPrimitive = 0;
static int glcVertsSent = 0;
static const char* glcPrimitiveName = "?";

#undef glBegin

void GL_Begin(GLenum primitive)
{
	if (GL_UseGLSL()) {
		return;
	}

#ifdef WITH_OPENGL_TRACE
	glcVertsSent = 0;
	glcVertsPerPrimitive = 0;
	glcBaseVertsPerPrimitive = 0;
	glcPrimitiveName = "?";

	switch (primitive) {
	case GL_QUADS:
		glcVertsPerPrimitive = 4;
		glcBaseVertsPerPrimitive = 0;
		glcPrimitiveName = "GL_QUADS";
		break;
	case GL_POLYGON:
		glcVertsPerPrimitive = 0;
		glcBaseVertsPerPrimitive = 0;
		glcPrimitiveName = "GL_POLYGON";
		break;
	case GL_TRIANGLE_FAN:
		glcVertsPerPrimitive = 1;
		glcBaseVertsPerPrimitive = 2;
		glcPrimitiveName = "GL_TRIANGLE_FAN";
		break;
	case GL_TRIANGLE_STRIP:
		glcVertsPerPrimitive = 1;
		glcBaseVertsPerPrimitive = 2;
		glcPrimitiveName = "GL_TRIANGLE_STRIP";
		break;
	case GL_LINE_LOOP:
		glcVertsPerPrimitive = 1;
		glcBaseVertsPerPrimitive = 1;
		glcPrimitiveName = "GL_LINE_LOOP";
		break;
	case GL_LINES:
		glcVertsPerPrimitive = 2;
		glcBaseVertsPerPrimitive = 0;
		glcPrimitiveName = "GL_LINES";
		break;
	}
#endif

	++frameStats.draw_calls;
	glBegin(primitive);
	//GL_LogAPICall("GL_Begin()...");
}

#undef glEnd

void GL_End(void)
{
#ifdef WITH_OPENGL_TRACE
	int primitives;
	const char* count_name = "vertices";
#endif

	if (GL_UseGLSL()) {
		return;
	}

	glEnd();

#ifdef WITH_OPENGL_TRACE
	primitives = max(0, glcVertsSent - glcBaseVertsPerPrimitive);
	if (glcVertsPerPrimitive) {
		primitives = glcVertsSent / glcVertsPerPrimitive;
		count_name = "primitives";
	}
	GL_LogAPICall("glBegin/End(%s: %d %s)", glcPrimitiveName, primitives, count_name);
#endif
}

#undef glVertex2f
#undef glVertex3f
#undef glVertex3fv

void GL_Vertex2f(GLfloat x, GLfloat y)
{
	glVertex2f(x, y);
	++glcVertsSent;
}

void GL_Vertex3f(GLfloat x, GLfloat y, GLfloat z)
{
	glVertex3f(x, y, z);
	++glcVertsSent;
}

void GL_Vertex3fv(const GLfloat* v)
{
	glVertex3fv(v);
	++glcVertsSent;
}

void GL_BindImageTexture(GLuint unit, texture_ref texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format)
{
	GLuint glRef = 0;

	if (GL_TextureReferenceIsValid(texture)) {
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
void GL_PrintState(FILE* debug_frame_out)
{
	int i;

	if (debug_frame_out) {
		rendering_state_t* current = &opengl.rendering_state;

		fprintf(debug_frame_out, "... <state-dump>\n");
		fprintf(debug_frame_out, "..... Z-Buffer: %s, func %s range %f=>%f [mask %s]\n", current->depth.test_enabled ? "on" : "off", txtDepthFunctions[current->depth.func], current->depth.nearRange, current->depth.farRange, current->depth.mask_enabled ? "on" : "off");
		fprintf(debug_frame_out, "..... Cull-face: %s, mode %s\n", current->cullface.enabled ? "enabled" : "disabled", txtCullFaceValues[current->cullface.mode]);
		fprintf(debug_frame_out, "..... Blending: %s, func %s\n", current->blendingEnabled ? "enabled" : "disabled", txtBlendFuncNames[current->blendFunc]);
		fprintf(debug_frame_out, "..... AlphaTest: %s, func %s(%f)\n", current->alphaTesting.enabled ? "on" : "off", txtAlphaTestModeValues[current->alphaTesting.func], current->alphaTesting.value);
		fprintf(debug_frame_out, "..... Texture units: [");
		for (i = 0; i < sizeof(current->textureUnits) / sizeof(current->textureUnits[0]); ++i) {
			fprintf(debug_frame_out, "%s%s", i ? "," : "", current->textureUnits[i].enabled ? txtTextureEnvModeValues[current->textureUnits[i].mode] : "n");
		}
		fprintf(debug_frame_out, "]\n");
		fprintf(debug_frame_out, "..... glPolygonMode: %s\n", txtPolygonModeValues[current->polygonMode]);
		fprintf(debug_frame_out, "... </state-dump>\n");
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

void GL_CheckMultiTextureExtensions(void)
{
	extern cvar_t gl_maxtmu2;

	if (!COM_CheckParm(cmdline_param_client_nomultitexturing) && SDL_GL_ExtensionSupported("GL_ARB_multitexture")) {
		if (strstr(gl_renderer, "Savage")) {
			return;
		}
		qglMultiTexCoord2f = SDL_GL_GetProcAddress("glMultiTexCoord2fARB");
		if (!qglActiveTexture) {
			qglActiveTexture = SDL_GL_GetProcAddress("glActiveTextureARB");
		}
		qglClientActiveTexture = SDL_GL_GetProcAddress("glClientActiveTexture");
		if (!qglMultiTexCoord2f || !qglActiveTexture || !qglClientActiveTexture) {
			return;
		}
		Com_Printf_State(PRINT_OK, "Multitexture extensions found\n");
		gl_mtexable = true;

		qglBindTextures = SDL_GL_GetProcAddress("glBindTextures");
	}

	gl_textureunits = min(glConfig.texture_units, 4);

	if (COM_CheckParm(cmdline_param_client_maximum2textureunits) /*|| !strcmp(gl_vendor, "ATI Technologies Inc.")*/ || gl_maxtmu2.value) {
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

void GLC_ClientActiveTexture(GLenum texture_unit)
{
	if (qglClientActiveTexture) {
		qglClientActiveTexture(texture_unit);
	}
	else {
		assert(texture_unit == GL_TEXTURE0);
	}
}

void R_ApplyRenderingState(rendering_state_t* state)
{
	assert(state->initialized);

	GL_ApplyRenderingState(state);
}

void R_CustomColor(float r, float g, float b, float a)
{
	if (GL_UseImmediateMode()) {
		if (opengl.rendering_state.color[0] != r || opengl.rendering_state.color[1] != g || opengl.rendering_state.color[2] != b || opengl.rendering_state.color[3] != a) {
			glColor4f(
				opengl.rendering_state.color[0] = r,
				opengl.rendering_state.color[1] = g,
				opengl.rendering_state.color[2] = b,
				opengl.rendering_state.color[3] = a
			);
		}
	}
}

void R_EnableScissorTest(int x, int y, int width, int height)
{
	if (GL_UseImmediateMode()) {
		glEnable(GL_SCISSOR_TEST);
		glScissor(x, y, width, height);
	}
}

void R_DisableScissorTest(void)
{
	if (GL_UseImmediateMode()) {
		glDisable(GL_SCISSOR_TEST);
	}
}

void R_ClearColor(float r, float g, float b, float a)
{
	if (GL_UseImmediateMode()) {
		glClearColor(r, g, b, a);
	}
	else if (GL_UseGLSL()) {
		glClearColor(r, g, b, a);
	}
}

void R_GLC_TextureUnitSet(rendering_state_t* state, int index, qbool enabled, r_texunit_mode_t mode)
{
	state->textureUnits[index].enabled = enabled;
	state->textureUnits[index].mode = mode;
}
