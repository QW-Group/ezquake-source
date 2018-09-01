/*
Copyright (C) 2011 ezQuake team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Loads and does all the framebuffer stuff

// Seems the original was for camquake & ported across to draw the hud on, but not used
//   Re-using file for deferred rendering & post-processing in modern

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "gl_framebuffer.h"
#include "tr_types.h"
#include "r_texture.h"
#include "gl_texture_internal.h"
#include "r_renderer.h"

// OpenGL functionality from elsewhere
GLuint GL_TextureNameFromReference(texture_ref ref);

// OpenGL wrapper functions
static void GL_RenderBufferStorage(GLuint renderBuffer, GLenum internalformat, GLsizei width, GLsizei height);
static void GL_FramebufferRenderbuffer(GLuint fbref, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
static GLenum GL_CheckFramebufferStatus(GLuint fbref);
static void GL_FramebufferTexture(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level);
static void GL_GenRenderBuffers(GLsizei n, GLuint* buffers);
static void GL_GenFramebuffers(GLsizei n, GLuint* buffers);

#define MAX_FRAMEBUFFERS 16

typedef struct framebuffer_data_s {
	GLuint glref;

	texture_ref rgbaTexture;
	GLuint depthBuffer;
	GLsizei width;
	GLsizei height;
	GLenum status;

	int next_free;
} framebuffer_data_t;

static framebuffer_data_t framebuffer_data[MAX_FRAMEBUFFERS];
static int framebuffers;
const framebuffer_ref null_framebuffer_ref = { 0 };

// 
typedef void (APIENTRY *glGenFramebuffers_t)(GLsizei n, GLuint* ids);
typedef void (APIENTRY *glDeleteFramebuffers_t)(GLsizei n, GLuint* ids);
typedef void (APIENTRY *glBindFramebuffer_t)(GLenum target, GLuint framebuffer);

typedef void (APIENTRY *glGenRenderbuffers_t)(GLsizei n, GLuint* ids);
typedef void (APIENTRY *glDeleteRenderbuffers_t)(GLsizei n, GLuint* ids);
typedef void (APIENTRY *glBindRenderbuffer_t)(GLenum target, GLuint renderbuffer);
typedef void (APIENTRY *glRenderbufferStorage_t)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRY *glNamedRenderbufferStorage_t)(GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRY *glFramebufferRenderbuffer_t)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (APIENTRY *glNamedFramebufferRenderbuffer_t)(GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);

typedef void (APIENTRY *glFramebufferTexture_t)(GLenum target, GLenum attachment, GLuint texture, GLint level);
typedef void (APIENTRY *glNamedFramebufferTexture_t)(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level);
typedef GLenum (APIENTRY *glCheckFramebufferStatus_t)(GLenum target);
typedef GLenum (APIENTRY *glCheckNamedFramebufferStatus_t)(GLuint framebuffer, GLenum target);
typedef void (APIENTRY *glBlitFramebuffer_t)(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
typedef void (APIENTRY *glBlitNamedFramebuffer_t)(GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);

static glGenFramebuffers_t qglGenFramebuffers;
static glDeleteFramebuffers_t qglDeleteFramebuffers;
static glBindFramebuffer_t qglBindFramebuffer;

static glGenRenderbuffers_t qglGenRenderbuffers;
static glDeleteRenderbuffers_t qglDeleteRenderbuffers;
static glBindRenderbuffer_t qglBindRenderbuffer;
static glRenderbufferStorage_t qglRenderbufferStorage;
static glNamedRenderbufferStorage_t qglNamedRenderbufferStorage;
static glFramebufferRenderbuffer_t qglFramebufferRenderbuffer;
static glNamedFramebufferRenderbuffer_t qglNamedFramebufferRenderbuffer;

static glFramebufferTexture_t qglFramebufferTexture;
static glNamedFramebufferTexture_t qglNamedFramebufferTexture;
static glCheckFramebufferStatus_t qglCheckFramebufferStatus;
static glCheckNamedFramebufferStatus_t qglCheckNamedFramebufferStatus;

static glBlitFramebuffer_t qglBlitFramebuffer;
static glBlitNamedFramebuffer_t qglBlitNamedFramebuffer;

static qbool framebuffers_supported;

//
// Initialize framebuffer stuff, Loads procadresses and such.
//
void GL_InitialiseFramebufferHandling(void)
{
	glConfig.supported_features &= ~(R_SUPPORT_FRAMEBUFFERS | R_SUPPORT_FRAMEBUFFERS_BLIT);
	framebuffers_supported = true;

	if (GL_VersionAtLeast(3,0) || SDL_GL_ExtensionSupported("GL_ARB_framebuffer_object")) {
		GL_LoadMandatoryFunction(glGenFramebuffers, framebuffers_supported);
		GL_LoadMandatoryFunction(glDeleteFramebuffers, framebuffers_supported);
		GL_LoadMandatoryFunction(glBindFramebuffer, framebuffers_supported);

		GL_LoadMandatoryFunction(glGenRenderbuffers, framebuffers_supported);
		GL_LoadMandatoryFunction(glDeleteRenderbuffers, framebuffers_supported);
		GL_LoadMandatoryFunction(glBindRenderbuffer, framebuffers_supported);
		GL_LoadMandatoryFunction(glRenderbufferStorage, framebuffers_supported);
		GL_LoadMandatoryFunction(glFramebufferRenderbuffer, framebuffers_supported);
		GL_LoadMandatoryFunction(glBlitFramebuffer, framebuffers_supported);

		GL_LoadMandatoryFunction(glFramebufferTexture, framebuffers_supported);
		GL_LoadMandatoryFunction(glCheckFramebufferStatus, framebuffers_supported);

		glConfig.supported_features |= (framebuffers_supported ? (R_SUPPORT_FRAMEBUFFERS | R_SUPPORT_FRAMEBUFFERS_BLIT) : 0);
	}
	else if (SDL_GL_ExtensionSupported("GL_EXT_framebuffer_object")) {
		GL_LoadMandatoryFunctionEXT(glGenFramebuffers, framebuffers_supported);
		GL_LoadMandatoryFunctionEXT(glDeleteFramebuffers, framebuffers_supported);
		GL_LoadMandatoryFunctionEXT(glBindFramebuffer, framebuffers_supported);

		GL_LoadMandatoryFunctionEXT(glGenRenderbuffers, framebuffers_supported);
		GL_LoadMandatoryFunctionEXT(glDeleteRenderbuffers, framebuffers_supported);
		GL_LoadMandatoryFunctionEXT(glBindRenderbuffer, framebuffers_supported);
		GL_LoadMandatoryFunctionEXT(glRenderbufferStorage, framebuffers_supported);
		GL_LoadMandatoryFunctionEXT(glFramebufferRenderbuffer, framebuffers_supported);
		if (SDL_GL_ExtensionSupported("GL_EXT_framebuffer_blit")) {
			GL_LoadOptionalFunctionEXT(glBlitFramebuffer);
		}

		GL_LoadMandatoryFunctionEXT(glFramebufferTexture, framebuffers_supported);
		GL_LoadMandatoryFunctionEXT(glCheckFramebufferStatus, framebuffers_supported);

		glConfig.supported_features |= (framebuffers_supported ? (R_SUPPORT_FRAMEBUFFERS) : 0);
		glConfig.supported_features |= (framebuffers_supported && (qglBlitFramebuffer != NULL) ? (R_SUPPORT_FRAMEBUFFERS_BLIT) : 0);
	}
	else {
		framebuffers_supported = false;
	}

	if (GL_UseDirectStateAccess()) {
		GL_LoadOptionalFunction(glNamedRenderbufferStorage);
		GL_LoadOptionalFunction(glNamedFramebufferRenderbuffer);
		GL_LoadOptionalFunction(glBlitNamedFramebuffer);
		GL_LoadOptionalFunction(glNamedFramebufferTexture);
		GL_LoadOptionalFunction(glCheckNamedFramebufferStatus);
	}

	framebuffers = 1;
	memset(framebuffer_data, 0, sizeof(framebuffer_data));
}

framebuffer_ref GL_FramebufferCreate(int width, int height, qbool is3d)
{
	framebuffer_data_t* fb = NULL;
	framebuffer_ref ref;
	int i;

	if (!framebuffers_supported) {
		return null_framebuffer_ref;
	}

	for (i = 1; fb == NULL && i < framebuffers; ++i) {
		if (!framebuffer_data[i].glref) {
			fb = &framebuffer_data[i];
			break;
		}
	}

	if (fb == NULL && framebuffers < sizeof(framebuffer_data) / sizeof(framebuffer_data[0])) {
		fb = &framebuffer_data[framebuffers++];
	}
	else if (fb == NULL) {
		Sys_Error("Too many framebuffers allocated");
	}

	memset(fb, 0, sizeof(*fb));

	// Render to texture
	R_AllocateTextureReferences(texture_type_2d, width, height, TEX_NOSCALE | (is3d ? 0 : TEX_ALPHA), 1, &fb->rgbaTexture);
	renderer.TextureLabelSet(fb->rgbaTexture, is3d ? "framebuffer-texture(3d)" : "framebuffer-texture(2d)");
	renderer.TextureSetFiltering(fb->rgbaTexture, texture_minification_linear, texture_minification_linear);
	renderer.TextureWrapModeClamp(fb->rgbaTexture);

	// Create frame buffer with texture & depth
	GL_GenFramebuffers(1, &fb->glref);
	GL_TraceObjectLabelSet(GL_FRAMEBUFFER, fb->glref, -1, is3d ? "framebuffer(3D)" : "framebuffer(2D)");

	// Depth buffer
	if (is3d) {
		extern cvar_t r_24bit_depth;
		GLenum depthFormat = r_24bit_depth.integer ? GL_DEPTH_COMPONENT24 : GL_DEPTH_COMPONENT;

		GL_GenRenderBuffers(1, &fb->depthBuffer);
		GL_TraceObjectLabelSet(GL_RENDERBUFFER, fb->depthBuffer, -1, "depth-buffer");
		GL_RenderBufferStorage(fb->depthBuffer, depthFormat, width, height);
		GL_FramebufferRenderbuffer(fb->glref, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fb->depthBuffer);
	}

	GL_FramebufferTexture(fb->glref, GL_COLOR_ATTACHMENT0, GL_TextureNameFromReference(fb->rgbaTexture), 0);

	fb->status = GL_CheckFramebufferStatus(fb->glref);
	assert(fb->status == GL_FRAMEBUFFER_COMPLETE);
	fb->width = width;
	fb->height = height;

	qglBindFramebuffer(GL_FRAMEBUFFER, 0);

	ref.index = fb - framebuffer_data;
	return ref;
}

void GL_FramebufferDelete(framebuffer_ref* pref)
{
	if (pref && GL_FramebufferReferenceIsValid(*pref)) {
		framebuffer_data_t* fb = &framebuffer_data[pref->index];
		if (fb->depthBuffer) {
			qglDeleteRenderbuffers(1, &fb->depthBuffer);
		}
		if (R_TextureReferenceIsValid(fb->rgbaTexture)) {
			R_DeleteTexture(&fb->rgbaTexture);
		}
		if (fb->glref) {
			qglDeleteFramebuffers(1, &fb->glref);
		}

		memset(fb, 0, sizeof(*fb));
		GL_FramebufferReferenceInvalidate(*pref);
	}
}

void GL_FramebufferStartUsing(framebuffer_ref ref)
{
	qglBindFramebuffer(GL_FRAMEBUFFER, framebuffer_data[ref.index].glref);
}

void GL_FramebufferStartUsingScreen(void)
{
	qglBindFramebuffer(GL_FRAMEBUFFER, 0);
}

texture_ref GL_FramebufferTextureReference(framebuffer_ref ref, int index)
{
	return framebuffer_data[ref.index].rgbaTexture;
}

// OpenGL wrapper functions
static void GL_RenderBufferStorage(GLuint renderBuffer, GLenum internalformat, GLsizei width, GLsizei height)
{
	if (qglNamedRenderbufferStorage) {
		qglNamedRenderbufferStorage(renderBuffer, internalformat, width, height);
	}
	else if (qglBindRenderbuffer && qglRenderbufferStorage) {
		qglBindRenderbuffer(GL_RENDERBUFFER, renderBuffer);
		qglRenderbufferStorage(GL_RENDERBUFFER, internalformat, width, height);
	}
	else {
		Sys_Error("ERROR: %s called without driver support", __FUNCTION__);
	}
}

static void GL_FramebufferRenderbuffer(GLuint fbref, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
	if (qglNamedFramebufferRenderbuffer) {
		qglNamedFramebufferRenderbuffer(fbref, attachment, renderbuffertarget, renderbuffer);
	}
	else if (qglBindFramebuffer && qglFramebufferRenderbuffer) {
		qglBindFramebuffer(GL_FRAMEBUFFER, fbref);
		qglFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, renderbuffertarget, renderbuffer);
	}
	else {
		Sys_Error("ERROR: %s called without driver support", __FUNCTION__);
	}
}

static GLenum GL_CheckFramebufferStatus(GLuint fbref)
{
	if (qglCheckNamedFramebufferStatus) {
		return qglCheckNamedFramebufferStatus(fbref, GL_FRAMEBUFFER);
	}
	else if (qglBindFramebuffer && qglCheckFramebufferStatus) {
		qglBindFramebuffer(GL_FRAMEBUFFER, fbref);
		return qglCheckFramebufferStatus(GL_FRAMEBUFFER);
	}
	else {
		Sys_Error("ERROR: %s called without driver support", __FUNCTION__);
		return 0;
	}
}

static void GL_FramebufferTexture(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level)
{
	if (qglNamedFramebufferTexture) {
		qglNamedFramebufferTexture(framebuffer, attachment, texture, level);
	}
	else if (qglBindFramebuffer && qglFramebufferTexture) {
		qglBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		qglFramebufferTexture(GL_FRAMEBUFFER, attachment, texture, level);
	}
	else {
		Sys_Error("ERROR: %s called without driver support", __FUNCTION__);
	}
}

static void GL_GenRenderBuffers(GLsizei n, GLuint* buffers)
{
	int i;

	qglGenRenderbuffers(n, buffers);

	for (i = 0; i < n; ++i) {
		qglBindRenderbuffer(GL_RENDERBUFFER, buffers[i]);
	}
}

static void GL_GenFramebuffers(GLsizei n, GLuint* buffers)
{
	int i;

	qglGenFramebuffers(n, buffers);

	for (i = 0; i < n; ++i) {
		qglBindFramebuffer(GL_FRAMEBUFFER, buffers[i]);
	}
}

int GL_FrameBufferWidth(framebuffer_ref ref)
{
	return framebuffer_data[ref.index].width;
}

int GL_FrameBufferHeight(framebuffer_ref ref)
{
	return framebuffer_data[ref.index].height;
}

void GL_FramebufferBlitSimple(framebuffer_ref source, framebuffer_ref destination)
{
	GLint srcTL[2] = { 0, 0 };
	GLint srcSize[2];
	GLint destTL[2] = { 0, 0 };
	GLint destSize[2];
	GLenum filter = GL_NEAREST;

	srcSize[0] = GL_FramebufferReferenceIsValid(source) ? GL_FrameBufferWidth(source) : glConfig.vidWidth;
	srcSize[1] = GL_FramebufferReferenceIsValid(source) ? GL_FrameBufferHeight(source) : glConfig.vidHeight;
	destSize[0] = GL_FramebufferReferenceIsValid(destination) ? GL_FrameBufferWidth(destination) : glConfig.vidWidth;
	destSize[1] = GL_FramebufferReferenceIsValid(destination) ? GL_FrameBufferHeight(destination) : glConfig.vidHeight;
	if (srcSize[0] != destSize[0] || srcSize[1] != destSize[1]) {
		filter = GL_LINEAR;
	}

	if (qglBlitNamedFramebuffer) {
		qglBlitNamedFramebuffer(
			framebuffer_data[source.index].glref, framebuffer_data[destination.index].glref,
			srcTL[0], srcTL[1], srcTL[0] + srcSize[0], srcTL[1] + srcSize[1],
			destTL[0], destTL[1], destTL[0] + destSize[0], destTL[1] + destSize[1],
			GL_COLOR_BUFFER_BIT, filter
		);
	}
	else if (qglBlitFramebuffer) {
		// ARB_framebuffer
		qglBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer_data[source.index].glref);
		qglBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer_data[destination.index].glref);

		qglBlitFramebuffer(
			srcTL[0], srcTL[1], srcTL[0] + srcSize[0], srcTL[1] + srcSize[1],
			destTL[0], destTL[1], destTL[0] + destSize[0], destTL[1] + destSize[1],
			GL_COLOR_BUFFER_BIT, filter
		);
	}
	else {
		// Shouldn't have been called, not supported
	}
}

// --- Wrapper functionality over, rendering logic below ---
static framebuffer_ref framebuffer3d;
static framebuffer_ref framebuffer2d;

extern cvar_t vid_framebuffer;
extern cvar_t vid_framebuffer_palette;

qbool GL_FramebufferEnabled3D(void)
{
	return vid_framebuffer.integer && GL_FramebufferReferenceIsValid(framebuffer3d);
}

qbool GL_FramebufferEnabled2D(void)
{
	return GL_FramebufferReferenceIsValid(framebuffer2d);
}

static void VID_FramebufferFlip(void)
{
	qbool flip3d = vid_framebuffer.integer && GL_FramebufferReferenceIsValid(framebuffer3d);
	qbool flip2d = vid_framebuffer.integer == USE_FRAMEBUFFER_3DONLY && GL_FramebufferReferenceIsValid(framebuffer2d);

	if (flip3d || flip2d) {
		extern cvar_t vid_framebuffer_blit;

		// Screen-wide framebuffer without any processing required, so we can just blit
		qbool should_blit = (
			vid_framebuffer_palette.integer == 0 &&
			vid_framebuffer.integer != USE_FRAMEBUFFER_3DONLY &&
			vid_framebuffer_blit.integer &&
			(glConfig.supported_features & R_SUPPORT_FRAMEBUFFERS_BLIT)
		);

		// render to screen from now on
		GL_FramebufferStartUsingScreen();

		if (should_blit && flip3d) {
			// Blit to screen
			GL_FramebufferBlitSimple(framebuffer3d, null_framebuffer_ref);
		}
		else {
			renderer.RenderFramebuffers(flip3d ? framebuffer3d : null_framebuffer_ref, flip2d ? framebuffer2d : null_framebuffer_ref);
		}
	}
}

static qbool VID_FramebufferInit(framebuffer_ref* framebuffer, int effective_width, int effective_height, qbool is3D)
{
	if (effective_width && effective_height) {
		if (!GL_FramebufferReferenceIsValid(*framebuffer)) {
			*framebuffer = GL_FramebufferCreate(effective_width, effective_height, is3D);
		}
		else if (GL_FrameBufferWidth(*framebuffer) != effective_width || GL_FrameBufferHeight(*framebuffer) != effective_height) {
			GL_FramebufferDelete(framebuffer);

			*framebuffer = GL_FramebufferCreate(effective_width, effective_height, is3D);
		}

		if (GL_FramebufferReferenceIsValid(*framebuffer)) {
			GL_FramebufferStartUsing(*framebuffer);
			return true;
		}
	}
	else {
		if (GL_FramebufferReferenceIsValid(*framebuffer)) {
			GL_FramebufferDelete(framebuffer);
		}
	}

	return false;
}

void GL_FramebufferScreenDrawStart(void)
{
	if (vid_framebuffer.integer) {
		VID_FramebufferInit(&framebuffer3d, VID_ScaledWidth3D(), VID_ScaledHeight3D(), true);
	}
}

qbool GL_Framebuffer2DSwitch(void)
{
	if (vid_framebuffer.integer == USE_FRAMEBUFFER_3DONLY) {
		if (VID_FramebufferInit(&framebuffer2d, glConfig.vidWidth, glConfig.vidHeight, false)) {
			R_Viewport(0, 0, glConfig.vidWidth, glConfig.vidHeight);
			glClear(GL_COLOR_BUFFER_BIT);
			return true;
		}
	}

	R_Viewport(glx, gly, glwidth, glheight);
	return false;
}

void GL_FramebufferPostProcessScreen(void)
{
	extern cvar_t vid_framebuffer, vid_framebuffer_palette;
	qbool framebuffer_active = (GL_FramebufferReferenceIsValid(framebuffer3d) || GL_FramebufferReferenceIsValid(framebuffer2d));

	if (framebuffer_active) {
		R_Viewport(glx, gly, glConfig.vidWidth, glConfig.vidHeight);

		VID_FramebufferFlip();

		if (!vid_framebuffer_palette.integer) {
			// Hardware palette changes
			V_UpdatePalette();
		}
	}
	else {
		// Hardware palette changes
		V_UpdatePalette();
	}
}
