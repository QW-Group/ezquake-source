
// gl_state.c
// State caching for OpenGL

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"

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

static GLenum oldtarget = GL_TEXTURE0;
static int cnttextures[] = {-1, -1, -1, -1, -1, -1, -1, -1};
static GLuint cntarrays[] = {0, 0, 0, 0, 0, 0, 0, 0};
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
}

// These functions taken from gl_texture.c
static int currenttexture = -1;
static GLuint currentTextureArray = -1;

void GL_Bind(int texnum)
{
	if (currenttexture == texnum) {
		return;
	}

	currenttexture = texnum;
	glBindTexture(GL_TEXTURE_2D, texnum);
}

void GL_BindArray(GLuint texnum)
{
	if (currentTextureArray == texnum) {
		return;
	}

	currentTextureArray = texnum;
	glBindTexture(GL_TEXTURE_2D_ARRAY, texnum);
}

void GL_BindTexture(GLenum targetType, GLuint texnum)
{
	if (targetType == GL_TEXTURE_2D) {
		GL_Bind(texnum);
	}
	else if (targetType == GL_TEXTURE_2D_ARRAY) {
		GL_BindArray(texnum);
	}
	else {
		// No caching...
		glBindTexture(targetType, texnum);
	}
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
	currenttexture = cnttextures[target - GL_TEXTURE0];
	currentTextureArray = cntarrays[target - GL_TEXTURE0];
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
