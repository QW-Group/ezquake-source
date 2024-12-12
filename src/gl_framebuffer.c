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
#include "rulesets.h"

extern cvar_t vid_framebuffer;
extern cvar_t vid_framebuffer_depthformat;
extern cvar_t vid_software_palette;
extern cvar_t vid_framebuffer_hdr;
extern cvar_t vid_framebuffer_blit;
extern cvar_t vid_framebuffer_smooth;
extern cvar_t vid_framebuffer_multisample;
extern cvar_t gl_outline;

static framebuffer_id VID_MultisampledAlternateId(framebuffer_id id);

#ifndef GL_NEGATIVE_ONE_TO_ONE
#define GL_NEGATIVE_ONE_TO_ONE            0x935E
#endif

#ifndef GL_ZERO_TO_ONE
#define GL_ZERO_TO_ONE                    0x935F
#endif

// If this isn't defined, we use simple texture for depth buffer
//   in theory render buffers _might_ be better, as they can't be read from
#define GL_USE_RENDER_BUFFERS

// OpenGL functionality from elsewhere
GLuint GL_TextureNameFromReference(texture_ref ref);

// OpenGL wrapper functions
#ifdef GL_USE_RENDER_BUFFERS
#define EZ_USE_FIXED_SAMPLE_LOCATIONS (GL_TRUE)
static void GL_RenderBufferStorageMultisample(GLuint renderBuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
static void GL_RenderBufferStorage(GLuint renderBuffer, GLenum internalformat, GLsizei width, GLsizei height);
static void GL_FramebufferRenderbuffer(GLuint fbref, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
static void GL_GenRenderBuffers(GLsizei n, GLuint* buffers);
#else
#define EZ_USE_FIXED_SAMPLE_LOCATIONS (GL_FALSE)
#endif
static GLenum GL_CheckFramebufferStatus(GLuint fbref);
static void GL_FramebufferTexture(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level);
static void GL_GenFramebuffers(GLsizei n, GLuint* buffers);
static void GL_FramebufferBlitSimple(framebuffer_id source_id, framebuffer_id destination_id);

typedef struct framebuffer_data_s {
	GLuint glref;
	texture_ref texture[fbtex_count];
	GLuint depthBuffer;
	GLenum depthFormat;

	GLsizei width;
	GLsizei height;
	GLenum status;
	int samples;
} framebuffer_data_t;

static const char* framebuffer_names[] = {
	"none", // framebuffer_none
	"std", // framebuffer_std,
	"std-ms", // framebuffer_std_ms,
	"hud", // framebuffer_hud
	"hud-ms", // framebuffer_hud_ms,
	"std-blit", // framebuffer_std_blit
	"std-blit-ms", // framebuffer_std_blit_ms
	"hud-blit", // framebuffer_hud_blit
	"hud-blit-ms", // framebuffer_hud_blit_ms
};
static const char* framebuffer_texture_names[] = {
	"std", // fbtex_standard,
	"depth", // fbtex_depth
	"env", // fbtex_bloom,
	"norms", // fbtex_worldnormals,
};
static qbool framebuffer_depth_buffer[] = {
	false, // framebuffer_none
	true, // framebuffer_std
	true, // framebuffer_std_ms
	false, // framebuffer_hud
	false, // framebuffer_hud_ms
	false, // framebuffer_std_blit
	false, // framebuffer_std_blit_ms
	false, // framebuffer_hud_blit
	false, // framebuffer_hud_blit_ms
};
static qbool framebuffer_hdr[] = {
	false, // framebuffer_none
	true, // framebuffer_std
	true, // framebuffer_std_ms
	false, // framebuffer_hud
	false, // framebuffer_hud_ms
	false, // framebuffer_std_blit
	false, // framebuffer_std_blit_ms
	false, // framebuffer_hud_blit
	false, // framebuffer_hud_blit_ms
};
static qbool framebuffer_alpha[] = {
	false, // framebuffer_none
	false, // framebuffer_std
	false, // framebuffer_std_ms
	true, // framebuffer_hud
	true, // framebuffer_hud_ms
	false, // framebuffer_std_blit
	false, // framebuffer_std_blit_ms
	false, // framebuffer_hud_blit
	false, // framebuffer_hud_blit_ms
};
static qbool framebuffer_multisampled[] = {
	false, // framebuffer_none
	false, // framebuffer_std
	true, // framebuffer_std_ms
	false, // framebuffer_hud
	true, // framebuffer_hud_ms
	false, // framebuffer_std_blit
	true, // framebuffer_std_blit_ms
	false, // framebuffer_hud_blit
	true, // framebuffer_hud_blit_ms
};
static framebuffer_id framebuffer_multisample_alternate[] = {
	framebuffer_none,
	// rendering
	framebuffer_std_ms,
	framebuffer_std_ms,
	framebuffer_hud_ms,
	framebuffer_hud_ms,
	// framebuffers used to resolve multisampling
	framebuffer_none,
	framebuffer_none,
	framebuffer_none,
	framebuffer_none,
};

#ifdef C_ASSERT
C_ASSERT(sizeof(framebuffer_names) / sizeof(framebuffer_names[0]) == framebuffer_count);
C_ASSERT(sizeof(framebuffer_texture_names) / sizeof(framebuffer_texture_names[0]) == fbtex_count);
C_ASSERT(sizeof(framebuffer_depth_buffer) / sizeof(framebuffer_depth_buffer[0]) == framebuffer_count);
C_ASSERT(sizeof(framebuffer_multisampled) / sizeof(framebuffer_multisampled[0]) == framebuffer_count);
C_ASSERT(sizeof(framebuffer_alpha) / sizeof(framebuffer_alpha[0]) == framebuffer_count);
C_ASSERT(sizeof(framebuffer_hdr) / sizeof(framebuffer_hdr[0]) == framebuffer_count);
C_ASSERT(sizeof(framebuffer_multisample_alternate) / sizeof(framebuffer_multisample_alternate[0]) == framebuffer_count);
#endif

static framebuffer_data_t framebuffer_data[framebuffer_count];

//
GL_StaticProcedureDeclaration(glGenFramebuffers, "n=%d, ids=%p", GLsizei n, GLuint* ids)
GL_StaticProcedureDeclaration(glDeleteFramebuffers, "n=%d, ids=%p", GLsizei n, GLuint* ids)
GL_StaticProcedureDeclaration(glBindFramebuffer, "target=%u, framebuffer=%u", GLenum target, GLuint framebuffer)

GL_StaticProcedureDeclaration(glGenRenderbuffers, "n=%d, ids=%p", GLsizei n, GLuint* ids)
GL_StaticProcedureDeclaration(glDeleteRenderbuffers, "n=%d, ids=%p", GLsizei n, GLuint* ids)
GL_StaticProcedureDeclaration(glBindRenderbuffer, "target=%u, renderbuffer=%u", GLenum target, GLuint renderbuffer)
GL_StaticProcedureDeclaration(glRenderbufferStorage, "target=%u, internalformat=%u, width=%d, height=%d", GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
GL_StaticProcedureDeclaration(glNamedRenderbufferStorage, "renderbuffer=%u, internalformat=%u, width=%d, height=%d", GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height)
GL_StaticProcedureDeclaration(glFramebufferRenderbuffer, "target=%u, attachment=%u, renderbuffertarget=%u, renderbuffer=%u", GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
GL_StaticProcedureDeclaration(glNamedFramebufferRenderbuffer, "framebuffer=%u, attachment=%u, renderbuffertarget=%u, renderbuffer=%u", GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)

GL_StaticProcedureDeclaration(glFramebufferTexture, "target=%u, attachment=%u, texture=%u, level=%d", GLenum target, GLenum attachment, GLuint texture, GLint level);
GL_StaticProcedureDeclaration(glNamedFramebufferTexture, "framebuffer=%u, attachment=%u, texture=%u, level=%d", GLuint framebuffer, GLenum attachment, GLuint texture, GLint level)
GL_StaticFunctionDeclaration(glCheckFramebufferStatus, "target=%u", "result=%u", GLenum, GLenum target)
GL_StaticFunctionWrapperBody(glCheckFramebufferStatus, GLenum, target)
GL_StaticFunctionDeclaration(glCheckNamedFramebufferStatus, "framebuffer=%u, target=%u", "result=%u", GLenum, GLuint framebuffer, GLenum target)
GL_StaticFunctionWrapperBody(glCheckNamedFramebufferStatus, GLenum, framebuffer, target)
GL_StaticProcedureDeclaration(glBlitFramebuffer, "srcX0=%d, srcY0=%d, srcX1=%d, srcY1=%d, dstX0=%d, dstY0=%d, dstX1=%d, dstY1=%d, mask=%u, filter=%u", GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
GL_StaticProcedureDeclaration(glBlitNamedFramebuffer, "readFrameBuffer=%u, drawFramebuffer=%u, srcX0=%d, srcY0=%d, srcX1=%d, srcY1=%d, dstX0=%d, dstY0=%d, dstX1=%d, dstY1=%d, mask=%u, filter=%u", GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
GL_StaticProcedureDeclaration(glDrawBuffers, "n=%d, bufs=%p", GLsizei n, const GLenum* bufs)
GL_StaticProcedureDeclaration(glClearBufferfv, "buffer=%u, drawbuffer=%d, value=%p", GLenum buffer, GLint drawbuffer, const GLfloat* value)
GL_StaticProcedureDeclaration(glClipControl, "origin=%u, depth=%u", GLenum origin, GLenum depth)

// Multi-sampled
GL_StaticProcedureDeclaration(glRenderbufferStorageMultisample, "target=%x, samples=%d, internalformat=%x, width=%d, height=%d", GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
GL_StaticProcedureDeclaration(glNamedRenderbufferStorageMultisample, "renderbuffer=%x, samples=%d, internalformat=%x, width=%d, height=%d", GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)

static GLenum glDepthFormats[] = { 0, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT32F };
typedef enum { r_depthformat_best, r_depthformat_16bit, r_depthformat_24bit, r_depthformat_32bit, r_depthformat_32bit_float, r_depthformat_count } r_depthformat;

#ifdef C_ASSERT
C_ASSERT(sizeof(framebuffer_names) / sizeof(framebuffer_names[0]) == framebuffer_count);
C_ASSERT(sizeof(framebuffer_texture_names) / sizeof(framebuffer_texture_names[0]) == fbtex_count);
C_ASSERT(sizeof(framebuffer_depth_buffer) / sizeof(framebuffer_depth_buffer[0]) == framebuffer_count);
C_ASSERT(sizeof(glDepthFormats) / sizeof(glDepthFormats[0]) == r_depthformat_count);
#endif

//
// Initialize framebuffer stuff, Loads procaddresses and such.
//
void GL_InitialiseFramebufferHandling(void)
{
	qbool framebuffers_supported = true;

	glConfig.supported_features &= ~(R_SUPPORT_FRAMEBUFFERS | R_SUPPORT_FRAMEBUFFERS_BLIT);

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
		GL_LoadMandatoryFunction(glDrawBuffers, framebuffers_supported);
		GL_LoadMandatoryFunction(glClearBufferfv, framebuffers_supported);

		GL_LoadOptionalFunction(glRenderbufferStorageMultisample);

		glConfig.supported_features |= (framebuffers_supported ? (R_SUPPORT_FRAMEBUFFERS | R_SUPPORT_FRAMEBUFFERS_BLIT) : 0);
		glConfig.supported_features |= (framebuffers_supported && GL_Available(glRenderbufferStorageMultisample) ? R_SUPPORT_FRAMEBUFFER_MS : 0);
		if (GL_VersionAtLeast(3, 0) || SDL_GL_ExtensionSupported("GL_ARB_depth_buffer_float")) {
			glConfig.supported_features |= R_SUPPORT_DEPTH32F;
		}
	}
	else if (SDL_GL_ExtensionSupported("GL_EXT_framebuffer_object") && SDL_GL_ExtensionSupported("GL_ARB_depth_texture")) {
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
		GL_LoadMandatoryFunctionEXT(glDrawBuffers, framebuffers_supported);

		GL_LoadMandatoryFunctionEXT(glFramebufferTexture, framebuffers_supported);
		GL_LoadMandatoryFunctionEXT(glCheckFramebufferStatus, framebuffers_supported);
		GL_LoadMandatoryFunctionEXT(glClearBufferfv, framebuffers_supported);

		glConfig.supported_features |= (framebuffers_supported ? (R_SUPPORT_FRAMEBUFFERS) : 0);
		glConfig.supported_features |= (framebuffers_supported && GL_Available(glBlitFramebuffer) ? (R_SUPPORT_FRAMEBUFFERS_BLIT) : 0);
		glConfig.supported_features |= SDL_GL_ExtensionSupported("GL_ARB_depth_buffer_float") ? R_SUPPORT_DEPTH32F : 0;
	}

	glConfig.supported_features |= SDL_GL_ExtensionSupported("GL_ARB_framebuffer_sRGB") ? R_SUPPORT_FRAMEBUFFERS_SRGB : 0;

	if (GL_UseDirectStateAccess()) {
		GL_LoadOptionalFunction(glNamedRenderbufferStorage);
		GL_LoadOptionalFunction(glNamedFramebufferRenderbuffer);
		GL_LoadOptionalFunction(glBlitNamedFramebuffer);
		GL_LoadOptionalFunction(glNamedFramebufferTexture);
		GL_LoadOptionalFunction(glCheckNamedFramebufferStatus);
		GL_LoadOptionalFunction(glNamedRenderbufferStorageMultisample);
	}

	// meag: disabled (classic needs glFrustum replaced, modern needs non-rubbish viewweapon
	//                 depth hack, and near-plane clipping issues when the player is gibbed)
	/*if (GL_VersionAtLeast(4, 5) || SDL_GL_ExtensionSupported("GL_ARB_clip_control")) {
		GL_LoadOptionalFunction(glClipControl);
	}*/

	memset(framebuffer_data, 0, sizeof(framebuffer_data));
}

void GL_FramebufferSetFiltering(qbool linear)
{
	texture_ref tex = framebuffer_data[framebuffer_std].texture[fbtex_standard];

	if (R_TextureReferenceIsValid(tex)) {
		texture_minification_id min_filter = linear ? texture_minification_linear : texture_minification_nearest;
		texture_magnification_id mag_filter = linear ? texture_magnification_linear : texture_magnification_nearest;

		renderer.TextureSetFiltering(tex, min_filter, mag_filter);
	}
}

static qbool GL_FramebufferCreateRenderingTexture(framebuffer_data_t* fb, fbtex_id tex_id, r_texture_type_id texture_type, GLenum framebuffer_format, const char* label, int width, int height)
{
	GL_CreateTexturesWithIdentifier(texture_type, 1, &fb->texture[tex_id], label);
	GL_TexStorage2D(fb->texture[tex_id], 1, framebuffer_format, width, height, false);
	renderer.TextureLabelSet(fb->texture[tex_id], label);
	renderer.TextureWrapModeClamp(fb->texture[tex_id]);
	renderer.TextureSetFiltering(fb->texture[tex_id], texture_minification_nearest, texture_magnification_nearest);
	R_TextureSetFlag(fb->texture[tex_id], R_TextureGetFlag(fb->texture[tex_id]) | TEX_NO_TEXTUREMODE);
	return true;
}

qbool GL_FramebufferCreate(framebuffer_id id, int width, int height)
{
	framebuffer_data_t* fb = NULL;
	char label[128];
	qbool hdr = (vid_framebuffer_hdr.integer && GL_VersionAtLeast(3, 0) && framebuffer_hdr[id]);
	GLenum framebuffer_format;

	if (!GL_Supported(R_SUPPORT_FRAMEBUFFERS)) {
		return false;
	}

	if (hdr) {
		framebuffer_format = GL_RGB16F;
	}
	else if (vid_gammacorrection.integer) {
		if (GL_Supported(R_SUPPORT_FRAMEBUFFERS_SRGB)) {
			framebuffer_format = framebuffer_alpha[id] ? GL_SRGB8_ALPHA8 : GL_SRGB8;
		}
		else {
			Con_Printf("sRGB framebuffers are not supported on your hardware.\n");
			Cvar_LatchedSetValue(&vid_framebuffer, 0);
			return false;
		}
	}
	else {
		framebuffer_format = framebuffer_alpha[id] ? GL_RGBA8 : GL_RGB8;
	}

	fb = &framebuffer_data[id];
	if (fb->glref) {
		return false;
	}

	memset(fb, 0, sizeof(*fb));

	// Multi-sampling round up to power of 2
	if (framebuffer_multisampled[id]) {
		Q_ROUND_POWER2(vid_framebuffer_multisample.integer, fb->samples);
		fb->samples = bound(0, fb->samples, glConfig.max_multisampling_level);
		if (vid_framebuffer_multisample.integer != fb->samples) {
			Cvar_SetValue(&vid_framebuffer_multisample, fb->samples);
			Con_Printf("&cff0vid_framebuffer_multisample&r has been set to %d\n", fb->samples);
		}
	}

	// Render to texture
	strlcpy(label, framebuffer_names[id], sizeof(label));
	strlcat(label, "/", sizeof(label));
	strlcat(label, framebuffer_texture_names[fbtex_standard], sizeof(label));

	// Create standard rendering texture
	GL_FramebufferCreateRenderingTexture(fb, fbtex_standard, texture_type_2d, framebuffer_format, label, width, height);

	// Create multi-sampled texture
	if (fb->samples) {
		GL_CreateTexturesWithIdentifier(texture_type_2d_multisampled, 1, &fb->texture[fbtex_standard], label);
		GL_TexStorage2DMultisample(fb->texture[fbtex_standard], fb->samples, framebuffer_format, width, height, EZ_USE_FIXED_SAMPLE_LOCATIONS);
	}

	// Set linear filtering if requested
	GL_FramebufferSetFiltering(vid_framebuffer_smooth.integer);

	// Create frame buffer with texture & depth
	GL_GenFramebuffers(1, &fb->glref);
	GL_TraceObjectLabelSet(GL_FRAMEBUFFER, fb->glref, -1, framebuffer_names[id]);

	// Depth buffer
	if (framebuffer_depth_buffer[id]) {
		GLenum depthFormat = glDepthFormats[bound(0, vid_framebuffer_depthformat.integer, r_depthformat_count - 1)];
		if (depthFormat == 0) {
			depthFormat = GL_Available(glClipControl) ? GL_DEPTH_COMPONENT32F : GL_DEPTH_COMPONENT32;
		}
		if (depthFormat == GL_DEPTH_COMPONENT32F && !GL_Supported(R_SUPPORT_DEPTH32F)) {
			depthFormat = GL_DEPTH_COMPONENT32;
		}

#ifdef GL_USE_RENDER_BUFFERS
		GL_GenRenderBuffers(1, &fb->depthBuffer);
		GL_TraceObjectLabelSet(GL_RENDERBUFFER, fb->depthBuffer, -1, "depth-buffer");
		if (fb->samples) {
			GL_RenderBufferStorageMultisample(fb->depthBuffer, fb->samples, fb->depthFormat = depthFormat, width, height);
			GL_FramebufferRenderbuffer(fb->glref, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fb->depthBuffer);
		}
		else {
			GL_RenderBufferStorage(fb->depthBuffer, fb->depthFormat = depthFormat, width, height);
			GL_FramebufferRenderbuffer(fb->glref, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fb->depthBuffer);
		}
#else
		// create depth texture
		strlcpy(label, framebuffer_names[id], sizeof(label));
		strlcat(label, "/", sizeof(label));
		strlcat(label, framebuffer_texture_names[fbtex_depth], sizeof(label));

		GL_CreateTexturesWithIdentifier(texture_type_2d, 1, &fb->texture[fbtex_depth], label);
		GL_TexStorage2D(fb->texture[fbtex_depth], 1, depthFormat, width, height, false);
		renderer.TextureSetFiltering(fb->texture[fbtex_depth], min_filter, mag_filter);
		renderer.TextureWrapModeClamp(fb->texture[fbtex_depth]);

		GL_FramebufferTexture(fb->glref, GL_DEPTH_ATTACHMENT, GL_TextureNameFromReference(fb->texture[fbtex_depth]), 0);
#endif

		if (GL_Available(glClipControl)) {
			if (depthFormat == GL_DEPTH_COMPONENT32F) {
				GL_Procedure(glClipControl, GL_LOWER_LEFT, GL_ZERO_TO_ONE);
				glClearDepth(0.0f);
				glConfig.reversed_depth = true;
			}
			else {
				GL_Procedure(glClipControl, GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE);
				glClearDepth(1.0f);
				glConfig.reversed_depth = false;
			}
		}
		else {
			glConfig.reversed_depth = false;
			glClearDepth(1.0f);
		}
	}

	GL_FramebufferTexture(fb->glref, GL_COLOR_ATTACHMENT0, GL_TextureNameFromReference(fb->texture[fbtex_standard]), 0);

	fb->status = GL_CheckFramebufferStatus(fb->glref);
	fb->width = width;
	fb->height = height;

	GL_Procedure(glBindFramebuffer, GL_FRAMEBUFFER, 0);

	switch (fb->status) {
	case GL_FRAMEBUFFER_COMPLETE:
		return true;
	case GL_FRAMEBUFFER_UNDEFINED:
		Con_Printf("&cf00error&r: the specified framebuffer is the default read or draw framebuffer, but the default framebuffer does not exist\n");
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
		Con_Printf("&cf00error&r: one of the framebuffer attachment points is framebuffer incomplete\n");
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
		Con_Printf("&cf00error&r: framebuffer does not have at least one image attached\n");
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
		Con_Printf("&cf00error&r: incomplete draw buffer\n");
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
		Con_Printf("&cf00error&r: incomplete read buffer\n");
		break;
	case GL_FRAMEBUFFER_UNSUPPORTED:
		Con_Printf("&cf00error&r: combination of formats is not supported\n");
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
		Con_Printf("&cf00error&r: mixture of renderbuffer/texture sampling levels\n");
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
		Con_Printf("&cf00error&r: incomplete layer targets\n");
		break;
	}

	// Rollback, delete objects
	GL_FramebufferDelete(id);
	return false;
}

qbool GL_FramebufferStartWorldNormals(framebuffer_id id)
{
	framebuffer_data_t* fb = NULL;
	GLenum buffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	float clearValue[] = { 0.0f, 0.0f, 0.0f, 0.0f };

	if (!(gl_outline.integer & 2) || !GL_Supported(R_SUPPORT_FRAMEBUFFERS) || !RuleSets_AllowEdgeOutline()) {
		return false;
	}

	id = VID_MultisampledAlternateId(id);
	fb = &framebuffer_data[id];
	if (!fb->glref) {
		return false;
	}

	if (!R_TextureReferenceIsValid(fb->texture[fbtex_worldnormals])) {
		char label[128];

		// normals
		strlcpy(label, framebuffer_names[id], sizeof(label));
		strlcat(label, "/", sizeof(label));
		strlcat(label, framebuffer_texture_names[fbtex_worldnormals], sizeof(label));

		if (fb->samples) {
			GL_CreateTexturesWithIdentifier(texture_type_2d_multisampled, 1, &fb->texture[fbtex_worldnormals], label);
			if (!R_TextureReferenceIsValid(fb->texture[fbtex_worldnormals])) {
				return false;
			}
			GL_TexStorage2DMultisample(fb->texture[fbtex_worldnormals], fb->samples, GL_RGBA16F, fb->width, fb->height, EZ_USE_FIXED_SAMPLE_LOCATIONS);
		}
		else {
			GL_CreateTexturesWithIdentifier(texture_type_2d, 1, &fb->texture[fbtex_worldnormals], label);
			if (!R_TextureReferenceIsValid(fb->texture[fbtex_worldnormals])) {
				return false;
			}
			GL_TexStorage2D(fb->texture[fbtex_worldnormals], 1, GL_RGBA16F, fb->width, fb->height, false);
			renderer.TextureSetFiltering(fb->texture[fbtex_worldnormals], texture_minification_nearest, texture_magnification_nearest);
			renderer.TextureWrapModeClamp(fb->texture[fbtex_worldnormals]);
		}
		R_TextureSetFlag(fb->texture[fbtex_worldnormals], R_TextureGetFlag(fb->texture[fbtex_worldnormals]) | TEX_NO_TEXTUREMODE);
	}

	GL_FramebufferTexture(fb->glref, GL_COLOR_ATTACHMENT1, GL_TextureNameFromReference(fb->texture[fbtex_worldnormals]), 0);
	GL_Procedure(glDrawBuffers, 2, buffers);
	GL_Procedure(glClearBufferfv, GL_COLOR, 1, clearValue);
	return true;
}

static void GL_MultiSamplingResolve(framebuffer_id src, framebuffer_id target, fbtex_id texture, framebuffer_id temp_src, framebuffer_id temp_dst)
{
	// Copy from (multi-sampled) src[texture] => (non-multi-sampled) target[texture]
	framebuffer_data_t* fb_orig_src = &framebuffer_data[src];
	framebuffer_data_t* fb_orig_dst = &framebuffer_data[target];
	framebuffer_data_t* fb_blit_src = &framebuffer_data[temp_src];
	framebuffer_data_t* fb_blit_dst = &framebuffer_data[temp_dst];

	if (!vid_framebuffer_multisample.integer || !fb_orig_src->glref || !fb_orig_dst->glref) {
		// nothing to do
		return;
	}

	if (temp_src) {
		if (fb_blit_src->width != fb_orig_src->width || fb_blit_src->height != fb_orig_src->height) {
			GL_FramebufferDelete(temp_src);
		}
		if (!fb_blit_src->glref && !GL_FramebufferCreate(temp_src, fb_orig_src->width, fb_orig_src->height)) {
			return;
		}
	}
	if (temp_dst) {
		if (fb_blit_dst->width != fb_orig_dst->width || fb_blit_src->height != fb_orig_dst->height) {
			GL_FramebufferDelete(temp_dst);
		}
		if (!fb_blit_dst->glref && !GL_FramebufferCreate(temp_dst, fb_orig_dst->width, fb_orig_dst->height)) {
			return;
		}
	}

	GL_FramebufferTexture(fb_blit_src->glref, GL_COLOR_ATTACHMENT0, GL_TextureNameFromReference(fb_orig_src->texture[texture]), 0);
	GL_FramebufferTexture(fb_blit_dst->glref, GL_COLOR_ATTACHMENT0, GL_TextureNameFromReference(fb_orig_dst->texture[texture]), 0);

	GL_FramebufferBlitSimple(temp_src, temp_dst);

	GL_FramebufferTexture(fb_blit_src->glref, GL_COLOR_ATTACHMENT0, 0, 0);
	GL_FramebufferTexture(fb_blit_dst->glref, GL_COLOR_ATTACHMENT0, 0, 0);
}

qbool GL_FramebufferEndWorldNormals(framebuffer_id id)
{
	framebuffer_data_t* fb = NULL;
	GLenum buffer = GL_COLOR_ATTACHMENT0;

	if (!GL_Supported(R_SUPPORT_FRAMEBUFFERS)) {
		return false;
	}

	id = VID_MultisampledAlternateId(id);
	fb = &framebuffer_data[id];
	if (!fb->glref) {
		return false;
	}

	GL_FramebufferTexture(fb->glref, GL_COLOR_ATTACHMENT1, 0, 0);
	GL_Procedure(glDrawBuffers, 1, &buffer);

	if (fb->samples && id == framebuffer_std_ms) {
		// Resolve multi-samples
		GL_MultiSamplingResolve(framebuffer_std_ms, framebuffer_std, fbtex_worldnormals, framebuffer_std_blit_ms, framebuffer_std_blit);
	}
	return true;
}

void GL_FramebufferDelete(framebuffer_id id)
{
	int i;
	framebuffer_data_t* fb = &framebuffer_data[id];

	if (id != framebuffer_none && fb->glref) {
		if (fb->depthBuffer) {
			GL_Procedure(glDeleteRenderbuffers, 1, &fb->depthBuffer);
		}
		for (i = 0; i < sizeof(fb->texture) / sizeof(fb->texture[0]); ++i) {
			if (R_TextureReferenceIsValid(fb->texture[i])) {
				R_DeleteTexture(&fb->texture[i]);
			}
		}
		if (fb->glref) {
			GL_Procedure(glDeleteFramebuffers, 1, &fb->glref);
		}

		memset(fb, 0, sizeof(*fb));
	}
}

void GL_FramebufferStartUsing(framebuffer_id id)
{
	GL_Procedure(glBindFramebuffer, GL_FRAMEBUFFER, framebuffer_data[id].glref);
}

void GL_FramebufferStartUsingScreen(void)
{
	// resolve any multi-sampling
	GL_MultiSamplingResolve(framebuffer_std_ms, framebuffer_std, fbtex_standard, framebuffer_std_blit_ms, framebuffer_std_blit);
	GL_MultiSamplingResolve(framebuffer_hud_ms, framebuffer_hud, fbtex_standard, framebuffer_hud_blit_ms, framebuffer_hud_blit);

	GL_Procedure(glBindFramebuffer, GL_FRAMEBUFFER, 0);
}

texture_ref GL_FramebufferTextureReference(framebuffer_id id, fbtex_id tex_id)
{
	return framebuffer_data[id].texture[tex_id];
}

// OpenGL wrapper functions
#ifdef GL_USE_RENDER_BUFFERS
static void GL_RenderBufferStorage(GLuint renderBuffer, GLenum internalformat, GLsizei width, GLsizei height)
{
	if (GL_Available(glNamedRenderbufferStorage)) {
		GL_Procedure(glNamedRenderbufferStorage, renderBuffer, internalformat, width, height);
	}
	else if (GL_Available(glBindRenderbuffer) && GL_Available(glRenderbufferStorage)) {
		GL_Procedure(glBindRenderbuffer, GL_RENDERBUFFER, renderBuffer);
		GL_Procedure(glRenderbufferStorage, GL_RENDERBUFFER, internalformat, width, height);
	}
	else {
		Sys_Error("ERROR: %s called without driver support", __func__);
	}
}

static void GL_RenderBufferStorageMultisample(GLuint renderBuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
	if (GL_Available(glNamedRenderbufferStorageMultisample)) {
		GL_Procedure(glNamedRenderbufferStorageMultisample, renderBuffer, samples, internalformat, width, height);
	}
	else if (GL_Available(glBindRenderbuffer) && GL_Available(glRenderbufferStorageMultisample)) {
		GL_Procedure(glBindRenderbuffer, GL_RENDERBUFFER, renderBuffer);
		GL_Procedure(glRenderbufferStorageMultisample, GL_RENDERBUFFER, samples, internalformat, width, height);
	}
	else {
		Sys_Error("ERROR: %s called without driver support", __func__);
	}
}

static void GL_FramebufferRenderbuffer(GLuint fbref, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
	if (GL_Available(glNamedFramebufferRenderbuffer)) {
		GL_Procedure(glNamedFramebufferRenderbuffer, fbref, attachment, renderbuffertarget, renderbuffer);
	}
	else if (GL_Available(glBindFramebuffer) && GL_Available(glFramebufferRenderbuffer)) {
		GL_Procedure(glBindFramebuffer, GL_FRAMEBUFFER, fbref);
		GL_Procedure(glFramebufferRenderbuffer, GL_FRAMEBUFFER, attachment, renderbuffertarget, renderbuffer);
	}
	else {
		Sys_Error("ERROR: %s called without driver support", __func__);
	}
}

static void GL_GenRenderBuffers(GLsizei n, GLuint* buffers)
{
	int i;

	GL_Procedure(glGenRenderbuffers, n, buffers);

	for (i = 0; i < n; ++i) {
		GL_Procedure(glBindRenderbuffer, GL_RENDERBUFFER, buffers[i]);
	}
}
#endif

static GLenum GL_CheckFramebufferStatus(GLuint fbref)
{
	if (GL_Available(glCheckNamedFramebufferStatus)) {
		return GL_Function(glCheckNamedFramebufferStatus, fbref, GL_FRAMEBUFFER);
	}
	else if (GL_Available(glBindFramebuffer) && GL_Available(glCheckFramebufferStatus)) {
		GL_Procedure(glBindFramebuffer, GL_FRAMEBUFFER, fbref);
		return GL_Function(glCheckFramebufferStatus, GL_FRAMEBUFFER);
	}
	else {
		Sys_Error("ERROR: %s called without driver support", __func__);
		return 0;
	}
}

static void GL_FramebufferTexture(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level)
{
	if (GL_Available(glNamedFramebufferTexture)) {
		GL_Procedure(glNamedFramebufferTexture, framebuffer, attachment, texture, level);
	}
	else if (GL_Available(glBindFramebuffer) && GL_Available(glFramebufferTexture)) {
		GL_Procedure(glBindFramebuffer, GL_FRAMEBUFFER, framebuffer);
		GL_Procedure(glFramebufferTexture, GL_FRAMEBUFFER, attachment, texture, level);
	}
	else {
		Sys_Error("ERROR: %s called without driver support", __func__);
	}
}

static void GL_GenFramebuffers(GLsizei n, GLuint* buffers)
{
	int i;

	GL_Procedure(glGenFramebuffers, n, buffers);
	for (i = 0; i < n; ++i) {
		GL_Procedure(glBindFramebuffer, GL_FRAMEBUFFER, buffers[i]);
	}
}

int GL_FrameBufferWidth(framebuffer_id id)
{
	return framebuffer_data[id].width;
}

int GL_FrameBufferHeight(framebuffer_id id)
{
	return framebuffer_data[id].height;
}

static void GL_FramebufferBlitSimple(framebuffer_id source_id, framebuffer_id destination_id)
{
	framebuffer_data_t* src = &framebuffer_data[source_id];
	framebuffer_data_t* dest = &framebuffer_data[destination_id];
	GLint srcTL[2] = { 0, 0 };
	GLint srcSize[2];
	GLint destTL[2] = { 0, 0 };
	GLint destSize[2];
	GLenum filter = GL_NEAREST;

	srcSize[0] = src->glref ? src->width : glConfig.vidWidth;
	srcSize[1] = src->glref ? src->height : glConfig.vidHeight;
	destSize[0] = destination_id ? dest->width : glConfig.vidWidth;
	destSize[1] = destination_id ? dest->height : glConfig.vidHeight;
	if (srcSize[0] != destSize[0] || srcSize[1] != destSize[1]) {
		filter = GL_LINEAR;
	}

	if (GL_Available(glBlitNamedFramebuffer)) {
		GL_Procedure(
			glBlitNamedFramebuffer, src->glref, dest->glref,
			srcTL[0], srcTL[1], srcTL[0] + srcSize[0], srcTL[1] + srcSize[1],
			destTL[0], destTL[1], destTL[0] + destSize[0], destTL[1] + destSize[1],
			GL_COLOR_BUFFER_BIT, filter
		);
	}
	else if (GL_Available(glBlitFramebuffer)) {
		// ARB_framebuffer
		GL_Procedure(glBindFramebuffer, GL_READ_FRAMEBUFFER, src->glref);
		GL_Procedure(glBindFramebuffer, GL_DRAW_FRAMEBUFFER, dest->glref);

		GL_Procedure(
			glBlitFramebuffer,
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
qbool GL_FramebufferEnabled3D(void)
{
	framebuffer_data_t* fb = &framebuffer_data[framebuffer_std];

	return vid_framebuffer.integer && fb->glref && R_TextureReferenceIsValid(fb->texture[fbtex_standard]);
}

qbool GL_FramebufferEnabled2D(void)
{
	framebuffer_data_t* fb = &framebuffer_data[framebuffer_hud];

	return vid_framebuffer.integer && fb->glref && R_TextureReferenceIsValid(fb->texture[fbtex_standard]);
}

static void VID_FramebufferFlip(void)
{
	qbool flip3d = vid_framebuffer.integer && GL_FramebufferEnabled3D();
	qbool flip2d = vid_framebuffer.integer == USE_FRAMEBUFFER_3DONLY && GL_FramebufferEnabled2D();

	if (flip3d || flip2d) {
		// Screen-wide framebuffer without any processing required, so we can just blit
		qbool should_blit = (
			//vid_framebuffer_multisample.integer == 0 &&
			vid_software_palette.integer == 0 &&
			vid_framebuffer.integer != USE_FRAMEBUFFER_3DONLY &&
			vid_framebuffer_blit.integer &&
			(glConfig.supported_features & R_SUPPORT_FRAMEBUFFERS_BLIT)
		);

		// render to screen from now on
		GL_FramebufferStartUsingScreen();

		if (should_blit && flip3d) {
			// Blit to screen
			GL_FramebufferBlitSimple(framebuffer_std, framebuffer_none);
		}
		else {
			renderer.RenderFramebuffers();
		}
	}
	else if (vid_software_palette.integer) {
		renderer.RenderFramebuffers();
	}
}

static void GL_FramebufferEnsureCreated(framebuffer_id id, int width, int height)
{
	framebuffer_data_t* fb = &framebuffer_data[id];
	int expected_samples = framebuffer_multisampled[id] ? vid_framebuffer_multisample.integer : 0;

	if (!fb->glref) {
		GL_FramebufferCreate(id, width, height);
	}
	else if (fb->width != width || fb->height != height || fb->samples != expected_samples) {
		GL_FramebufferDelete(id);

		GL_FramebufferCreate(id, width, height);
	}
}

static void GL_FramebufferEnsureDeleted(framebuffer_id id)
{
	framebuffer_data_t* fb = &framebuffer_data[id];

	if (fb->glref) {
		GL_FramebufferDelete(id);
	}
}

static framebuffer_id VID_MultisampledAlternateId(framebuffer_id id)
{
	if (vid_framebuffer_multisample.integer && framebuffer_multisample_alternate[id] && framebuffer_multisample_alternate[id] != id) {
		return framebuffer_multisample_alternate[id];
	}
	return id;
}

static qbool VID_FramebufferInit(framebuffer_id id, int effective_width, int effective_height)
{
	if (effective_width && effective_height) {
		GL_FramebufferEnsureCreated(id, effective_width, effective_height);

		if (framebuffer_data[id].glref) {
			framebuffer_id ms_alt = VID_MultisampledAlternateId(id);

			if (ms_alt != id) {
				GL_FramebufferEnsureCreated(ms_alt, effective_width, effective_height);

				if (framebuffer_data[ms_alt].glref) {
					GL_FramebufferStartUsing(ms_alt);
					return true;
				}
			}
			else {
				GL_FramebufferStartUsing(id);
				return true;
			}
		}
	}
	else {
		GL_FramebufferEnsureDeleted(id);
	}

	return false;
}

void GL_FramebufferScreenDrawStart(void)
{
	if (vid_framebuffer.integer) {
		VID_FramebufferInit(framebuffer_std, VID_ScaledWidth3D(), VID_ScaledHeight3D());
	}
}

qbool GL_Framebuffer2DSwitch(void)
{
	if (vid_framebuffer.integer == USE_FRAMEBUFFER_3DONLY) {

		if (VID_FramebufferInit(framebuffer_hud, glConfig.vidWidth, glConfig.vidHeight)) {
			R_Viewport(0, 0, glConfig.vidWidth, glConfig.vidHeight);
			R_ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			return true;
		}
	}

	R_Viewport(glx, gly, glwidth, glheight);
	return false;
}

void GL_FramebufferPostProcessScreen(void)
{
	qbool framebuffer_active = GL_FramebufferEnabled3D() || GL_FramebufferEnabled2D();

	if (framebuffer_active || vid_software_palette.integer) {
		R_Viewport(glx, gly, glConfig.vidWidth, glConfig.vidHeight);

		VID_FramebufferFlip();
	}

	if (!vid_software_palette.integer) {
		renderer.BrightenScreen();

		// Hardware palette changes
		V_UpdatePalette();
	}
}

const char* GL_FramebufferZBufferString(framebuffer_id ref)
{
	if (framebuffer_data[ref].depthFormat == GL_DEPTH_COMPONENT16) {
		return "16-bit float z-buffer";
	}
	else if (framebuffer_data[ref].depthFormat == GL_DEPTH_COMPONENT24) {
		return "24-bit float z-buffer";
	}
	else if (framebuffer_data[ref].depthFormat == GL_DEPTH_COMPONENT32) {
		return "32-bit z-buffer";
	}
	else if (framebuffer_data[ref].depthFormat == GL_DEPTH_COMPONENT32F) {
		return "32-bit float z-buffer";
	}
	else {
		return "unknown z-buffer";
	}
}

static qbool GL_ScreenshotFramebuffer(void)
{
	extern cvar_t vid_framebuffer_sshotmode;

	return vid_framebuffer_sshotmode.integer && vid_framebuffer.integer == USE_FRAMEBUFFER_SCREEN && GL_FramebufferEnabled3D();
}

void GL_Screenshot(byte* buffer, size_t size)
{
	size_t width = renderer.ScreenshotWidth();
	size_t height = renderer.ScreenshotHeight();

	GL_PackAlignment(1);
	if (GL_Available(glBindFramebuffer)) {
		if (GL_ScreenshotFramebuffer()) {
			GL_Procedure(glBindFramebuffer, GL_READ_FRAMEBUFFER, framebuffer_data[framebuffer_std].glref);
		}
		else {
			GL_Procedure(glBindFramebuffer, GL_READ_FRAMEBUFFER, 0);
		}
	}
	glReadPixels(0, 0, (GLsizei)width, (GLsizei)height, GL_RGB, GL_UNSIGNED_BYTE, buffer);
}

size_t GL_ScreenshotWidth(void)
{
	if (GL_ScreenshotFramebuffer()) {
		return framebuffer_data[framebuffer_std].width;
	}

	return glConfig.vidWidth;
}

size_t GL_ScreenshotHeight(void)
{
	if (GL_ScreenshotFramebuffer()) {
		return framebuffer_data[framebuffer_std].height;
	}

	return glConfig.vidHeight;
}

int GL_FramebufferMultisamples(framebuffer_id framebuffer)
{
	framebuffer_data_t* fb = &framebuffer_data[framebuffer];

	if (vid_framebuffer.integer && fb->glref && R_TextureReferenceIsValid(fb->texture[fbtex_standard])) {
		return fb->samples;
	}

	return 0;
}

void GL_FramebufferDeleteAll(void)
{
	framebuffer_id i;

	for (i = 0; i < framebuffer_count; ++i) {
		GL_FramebufferEnsureDeleted(i);
	}
}

int GL_FramebufferFxaaPreset(void)
{
	static const int fxaa_cvar_to_preset[18] = {
			0, 10, 11, 12, 13, 14, 15, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 39
	};
	extern cvar_t vid_framebuffer_fxaa;

	return fxaa_cvar_to_preset[bound(0, vid_framebuffer_fxaa.integer, 17)];
}
