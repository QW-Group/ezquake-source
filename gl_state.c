
// gl_state.c
// State caching for OpenGL

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"

// #define GL_PARANOIA

static GLenum currentDepthFunc = GL_LESS;
static double currentNearRange = 0;
static double currentFarRange = 1;
static GLenum currentCullFace = GL_BACK;
static GLenum currentBlendSFactor = GL_ONE;
static GLenum currentBlendDFactor = GL_ZERO;
static GLuint currentVAO = 0;
static GLenum currentShadeModel = GL_SMOOTH;
// FIXME: currentWidth & currentHeight should be initialised to dimensions of window
static GLint currentViewportX = 0, currentViewportY = 0;
static GLsizei currentViewportWidth, currentViewportHeight;
static GLuint currentProgram = 0;
static qbool gl_cullface = false;
static qbool gl_depthTestEnabled = false;
static qbool gl_framebuffer_srgb = false;
static qbool gl_texture_2d = false;
static qbool gl_blend = false;
static qbool gl_cull_face = false;
static GLboolean gl_depth_mask = GL_FALSE;
static int currenttexture = -1;
static GLuint currentTextureArray = -1;

static GLenum oldtarget = GL_TEXTURE0;
static int cnttextures[] = {-1, -1, -1, -1, -1, -1, -1, -1};
static GLuint cntarrays[] = {0, 0, 0, 0, 0, 0, 0, 0};
static qbool texunitenabled[] = { false, false, false, false, false, false, false, false };
static qbool mtexenabled = false;

static GLenum lastTextureMode = GL_MODULATE;
static int old_alphablend_flags = 0;

// FIXME: Add optional support for DSA
void GL_BindTextureUnit(GLuint unit, GLenum targetType, GLuint texture)
{
	int unit_num = unit - GL_TEXTURE0;

	if (unit_num >= 0 && unit_num < sizeof(cntarrays)) {
		if (targetType == GL_TEXTURE_2D_ARRAY) {
			if (cntarrays[unit_num] != texture) {
				GL_SelectTexture(unit);
				GL_BindTexture(targetType, texture, true);
			}
			return;
		}
		else if (targetType == GL_TEXTURE_2D) {
			if (cnttextures[unit_num] != texture) {
				GL_SelectTexture(unit);
				GL_BindTexture(targetType, texture, true);
			}
			return;
		}
	}

	GL_SelectTexture(unit);
	GL_BindTexture(targetType, texture, true);
}

void GL_DepthFunc(GLenum func)
{
	if (func != currentDepthFunc) {
		glDepthFunc(func);
		currentDepthFunc = func;
	}
}

void GL_DepthRange(double nearVal, double farVal)
{
	if (nearVal != currentNearRange || farVal != currentFarRange) {
		glDepthRange(nearVal, farVal);

		currentNearRange = nearVal;
		currentFarRange = farVal;
	}
}

void GL_CullFace(GLenum mode)
{
	if (mode != currentCullFace) {
		glCullFace(mode);
		currentCullFace = mode;
	}
}

void GL_BlendFunc(GLenum sfactor, GLenum dfactor)
{
	if (sfactor != currentBlendSFactor || dfactor != currentBlendDFactor) {
		glBlendFunc(sfactor, dfactor);
		currentBlendSFactor = sfactor;
		currentBlendDFactor = dfactor;
	}
}

void GL_BindVertexArray(GLuint vao)
{
	if (currentVAO != vao) {
		glBindVertexArray(vao);
		currentVAO = vao;
	}
}

void GL_ShadeModel(GLenum model)
{
	if (model != currentShadeModel && !GL_ShadersSupported()) {
		glShadeModel(model);
		currentShadeModel = model;
	}
}

void GL_Viewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
	if (x != currentViewportX || y != currentViewportY || width != currentViewportWidth || height != currentViewportHeight) {
		glViewport(x, y, width, height);

		currentViewportX = x;
		currentViewportY = y;
		currentViewportWidth = width;
		currentViewportHeight = height;
	}
}

void GL_GetViewport(GLint* view)
{
	view[0] = currentViewportX;
	view[1] = currentViewportY;
	view[2] = currentViewportWidth;
	view[3] = currentViewportHeight;
}

void GL_InitialiseState(void)
{
	int i;

	currentDepthFunc = GL_LESS;
	currentNearRange = 0;
	currentFarRange = 1;
	currentCullFace = GL_BACK;
	currentBlendSFactor = GL_ONE;
	currentBlendDFactor = GL_ZERO;
	currentVAO = 0;
	currentShadeModel = GL_SMOOTH;
	currentViewportX = 0;
	currentViewportY = 0;
	// FIXME: currentWidth & currentHeight should be initialised to dimensions of window
	currentViewportWidth = 0;
	currentViewportHeight = 0;

	currentProgram = 0;

	gl_cullface = false;
	gl_depthTestEnabled = false;
	gl_framebuffer_srgb = false;
	gl_texture_2d = false;
	gl_blend = false;
	gl_cull_face = false;
	gl_depth_mask = GL_FALSE;
	currenttexture = -1;
	currentTextureArray = -1;
	lastTextureMode = GL_MODULATE;
	old_alphablend_flags = 0;

	GLM_SetIdentityMatrix(GLM_ProjectionMatrix());
	GLM_SetIdentityMatrix(GLM_ModelviewMatrix());

	for (i = 0; i < sizeof(cnttextures) / sizeof(cnttextures); ++i) {
		cnttextures[i] = -1;
		cntarrays[i] = 0;
		texunitenabled[i] = false;
	}
}

// These functions taken from gl_texture.c
void GL_BindTexture(GLenum targetType, GLuint texnum, qbool warning)
{
#ifdef GL_PARANOIA
	if (warning && !glIsTexture(texnum)) {
		Con_Printf("ERROR: Non-texture %d passed to GL_BindTexture\n", texnum);
		return;
	}
#endif

	if (targetType == GL_TEXTURE_2D) {
		if (currenttexture == texnum) {
			return;
		}

		currenttexture = texnum;
		glBindTexture(GL_TEXTURE_2D, texnum);
	}
	else if (targetType == GL_TEXTURE_2D_ARRAY) {
		if (currentTextureArray == texnum) {
			return;
		}

		currentTextureArray = texnum;
		glBindTexture(GL_TEXTURE_2D_ARRAY, texnum);
	}
	else {
		// No caching...
		glBindTexture(targetType, texnum);
	}
}

void GL_Bind(int texnum)
{
	GL_BindTexture(GL_TEXTURE_2D, texnum, true);
}

void GL_BindFirstTime(int texnum)
{
	GL_BindTexture(GL_TEXTURE_2D, texnum, false);
}

void GL_SelectTexture(GLenum target)
{
	if (target == oldtarget) {
		return;
	}

	if (glActiveTexture) {
		glActiveTexture(target);
	}
	else {
		qglActiveTexture(target);
	}

	cnttextures[oldtarget - GL_TEXTURE0] = currenttexture;
	cntarrays[oldtarget - GL_TEXTURE0] = currentTextureArray;
	texunitenabled[oldtarget - GL_TEXTURE0] = gl_texture_2d;
	currenttexture = cnttextures[target - GL_TEXTURE0];
	currentTextureArray = cntarrays[target - GL_TEXTURE0];
	gl_texture_2d = texunitenabled[target - GL_TEXTURE0];
	oldtarget = target;
}

void GL_DisableMultitexture(void)
{
	if (GL_ShadersSupported()) {
		GL_SelectTexture(GL_TEXTURE0);
	}
	else if (mtexenabled) {
		glDisable(GL_TEXTURE_2D);
		GL_SelectTexture(GL_TEXTURE0);
		mtexenabled = false;
	}
}

void GL_EnableMultitexture(void)
{
	if (GL_ShadersSupported()) {
		GL_SelectTexture(GL_TEXTURE1);
	}
	else if (gl_mtexable) {
		GL_SelectTexture(GL_TEXTURE1);
		glEnable(GL_TEXTURE_2D);
		mtexenabled = true;
	}
}

void GL_EnableTMU(GLenum target) 
{
	GL_SelectTexture(target);
	glEnable(GL_TEXTURE_2D);
}

void GL_DisableTMU(GLenum target) 
{
	GL_SelectTexture(target);
	glDisable(GL_TEXTURE_2D);
}

void GL_InitTextureState(void)
{
	int i;

	// Multi texture.
	currenttexture = -1;
	oldtarget = GL_TEXTURE0;
	for (i = 0; i < sizeof(cnttextures) / sizeof(cnttextures[0]); i++) {
		cnttextures[i] = -1;
		cntarrays[i] = 0;
	}
	mtexenabled = false;
}

void GL_UseProgram(GLuint program)
{
	if (program != currentProgram) {
		glUseProgram(program);

		currentProgram = program;
	}
}

void GL_DepthMask(GLboolean mask)
{
	if (mask != gl_depth_mask) {
		glDepthMask(mask);

		gl_depth_mask = mask;
	}
}

#undef glEnable
#undef glDisable

void GL_Enable(GLenum option)
{
	if (GL_ShadersSupported() && option == GL_TEXTURE_2D) {
		Con_Printf("WARNING: glEnable(GL_TEXTURE_2D) called in modern\n");
		return;
	}

	if (option == GL_DEPTH_TEST) {
		if (gl_depthTestEnabled) {
			return;
		}

		gl_depthTestEnabled = true;
	}
	else if (option == GL_FRAMEBUFFER_SRGB) {
		if (gl_framebuffer_srgb) {
			return;
		}

		gl_framebuffer_srgb = true;
	}
	else if (option == GL_CULL_FACE) {
		if (gl_cullface) {
			return;
		}

		gl_cullface = true;
	}
	else if (option == GL_TEXTURE_2D) {
		if (gl_texture_2d) {
			return;
		}

		gl_texture_2d = true;
	}
	else if (option == GL_BLEND) {
		if (gl_blend) {
			return;
		}

		gl_blend = true;
	}
	else if (option == GL_CULL_FACE) {
		if (gl_cull_face) {
			return;
		}

		gl_cull_face = true;
	}

	glEnable(option);
#ifdef GL_PARANOIA
	GL_ProcessErrors("glEnable");
#endif
}

void GL_Disable(GLenum option)
{
	if (GL_ShadersSupported() && option == GL_TEXTURE_2D) {
		Con_Printf("WARNING: glDisable(GL_TEXTURE_2D) called in modern\n");
		return;
	}

	if (option == GL_DEPTH_TEST) {
		if (!gl_depthTestEnabled) {
			return;
		}

		gl_depthTestEnabled = false;
	}
	else if (option == GL_FRAMEBUFFER_SRGB) {
		if (!gl_framebuffer_srgb) {
			return;
		}

		gl_framebuffer_srgb = false;
	}
	else if (option == GL_CULL_FACE) {
		if (!gl_cullface) {
			return;
		}

		gl_cullface = false;
	}
	else if (option == GL_TEXTURE_2D) {
		if (!gl_texture_2d) {
			return;
		}

		gl_texture_2d = false;
	}
	else if (option == GL_BLEND) {
		if (!gl_blend) {
			return;
		}

		gl_blend = false;
	}
	else if (option == GL_CULL_FACE) {
		if (!gl_cull_face) {
			return;
		}

		gl_cull_face = false;
	}

	glDisable(option);
}

void GL_TextureEnvMode(GLenum mode)
{
	if (GL_ShadersSupported()) {
		// Just store for now
		lastTextureMode = mode;
	}
	else if (mode != lastTextureMode) {
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode);
		lastTextureMode = mode;
	}
}

int GL_AlphaBlendFlags(int flags)
{
	if (!GL_ShadersSupported()) {
		if ((flags & GL_ALPHATEST_ENABLED) && !(old_alphablend_flags & GL_ALPHATEST_ENABLED)) {
			glEnable(GL_ALPHA_TEST);
		}
		else if ((flags & GL_ALPHATEST_DISABLED) && (old_alphablend_flags & GL_ALPHATEST_ENABLED)) {
			glDisable(GL_ALPHA_TEST);
		}
		else if (!(flags & (GL_ALPHATEST_ENABLED | GL_ALPHATEST_DISABLED))) {
			flags |= (old_alphablend_flags & GL_ALPHATEST_ENABLED);
		}
	}

	if ((flags & GL_BLEND_ENABLED) && !(old_alphablend_flags & GL_BLEND_ENABLED)) {
		glEnable(GL_BLEND);
	}
	else if (flags & GL_BLEND_DISABLED && (old_alphablend_flags & GL_BLEND_ENABLED)) {
		glDisable(GL_BLEND);
	}
	else if (!(flags & (GL_BLEND_ENABLED | GL_BLEND_DISABLED))) {
		flags |= (old_alphablend_flags & GL_BLEND_ENABLED);
	}

	old_alphablend_flags = flags;
	return old_alphablend_flags;
}

void GL_EnableFog(void)
{
	if (!GLM_Enabled() && gl_fogenable.value) {
		glEnable(GL_FOG);
	}
}

void GL_DisableFog(void)
{
	if (!GLM_Enabled() && gl_fogenable.value) {
		glDisable(GL_FOG);
	}
}

void GL_ConfigureFog(void)
{
	vec3_t colors;

	if (GLM_Enabled()) {
		// TODO
		return;
	}

	// START shaman BUG fog was out of control when fogstart>fogend {
	if (gl_fogenable.value && gl_fogstart.value >= 0 && gl_fogstart.value < gl_fogend.value) {
		// } END shaman BUG fog was out of control when fogstart>fogend
		glFogi(GL_FOG_MODE, GL_LINEAR);
		colors[0] = gl_fogred.value;
		colors[1] = gl_foggreen.value;
		colors[2] = gl_fogblue.value;
		glFogfv(GL_FOG_COLOR, colors);
		glFogf(GL_FOG_START, gl_fogstart.value);
		glFogf(GL_FOG_END, gl_fogend.value);
		glEnable(GL_FOG);
	}
	else {
		glDisable(GL_FOG);
	}
}

void GL_EnableWaterFog(int contents)
{
	extern cvar_t gl_waterfog_color_water;
	extern cvar_t gl_waterfog_color_lava;
	extern cvar_t gl_waterfog_color_slime;

	float colors[4];

	// TODO
	if (!GL_ShadersSupported()) {
		return;
	}

	switch (contents) {
	case CONTENTS_LAVA:
		colors[0] = (float) gl_waterfog_color_lava.color[0] / 255.0;
		colors[1] = (float) gl_waterfog_color_lava.color[1] / 255.0;
		colors[2] = (float) gl_waterfog_color_lava.color[2] / 255.0;
		colors[3] = (float) gl_waterfog_color_lava.color[3] / 255.0;
		break;
	case CONTENTS_SLIME:
		colors[0] = (float) gl_waterfog_color_slime.color[0] / 255.0;
		colors[1] = (float) gl_waterfog_color_slime.color[1] / 255.0;
		colors[2] = (float) gl_waterfog_color_slime.color[2] / 255.0;
		colors[3] = (float) gl_waterfog_color_slime.color[3] / 255.0;
		break;
	default:
		colors[0] = (float) gl_waterfog_color_water.color[0] / 255.0;
		colors[1] = (float) gl_waterfog_color_water.color[1] / 255.0;
		colors[2] = (float) gl_waterfog_color_water.color[2] / 255.0;
		colors[3] = (float) gl_waterfog_color_water.color[3] / 255.0;
		break;
	}

	glFogfv(GL_FOG_COLOR, colors);
	if (((int)gl_waterfog.value) == 2) {
		glFogf(GL_FOG_DENSITY, 0.0002 + (0.0009 - 0.0002) * bound(0, gl_waterfog_density.value, 1));
		glFogi(GL_FOG_MODE, GL_EXP);
	}
	else {
		glFogi(GL_FOG_MODE, GL_LINEAR);
		glFogf(GL_FOG_START, 150.0f);
		glFogf(GL_FOG_END, 4250.0f - (4250.0f - 1536.0f) * bound(0, gl_waterfog_density.value, 1));
	}
	glEnable(GL_FOG);
}
