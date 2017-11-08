
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
static int currenttexture = -1;
static GLuint currentTextureArray = -1;

static GLenum oldtarget = GL_TEXTURE0;
static int cnttextures[] = {-1, -1, -1, -1, -1, -1, -1, -1};
static GLuint cntarrays[] = {0, 0, 0, 0, 0, 0, 0, 0};
static qbool texunitenabled[] = { false, false, false, false, false, false, false, false };
static qbool mtexenabled = false;

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
	currenttexture = -1;
	currentTextureArray = -1;

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
