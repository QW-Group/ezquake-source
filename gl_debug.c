/*
Copyright (C) 2002-2003 A Nourai

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

// gl_debug.c - OpenGL dev-only & debugging wrappers
#include <SDL.h>

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "r_texture.h"
#include "r_renderer.h"
#include "utils.h"

GLuint GL_TextureNameFromReference(texture_ref ref);

// <debug-functions (4.3)>
//typedef void (APIENTRY *DEBUGPROC)(GLenum source, GLenum type, GLuint id, GLenum severity,  GLsizei length, const GLchar *message, const void *userParam);
typedef void (APIENTRY *glDebugMessageCallback_t)(GLDEBUGPROC callback, void* userParam);
typedef void (APIENTRY *glDebugMessageControl_t)(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled);

#ifdef WITH_RENDERING_TRACE
typedef void (APIENTRY *glPushDebugGroup_t)(GLenum source, GLuint id, GLsizei length, const char* message);
typedef void (APIENTRY *glPopDebugGroup_t)(void);
typedef void (APIENTRY *glObjectLabel_t)(GLenum identifier, GLuint name, GLsizei length, const char* label);
typedef void (APIENTRY *glGetObjectLabel_t)(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei* length, char* label);

static glPushDebugGroup_t qglPushDebugGroup;
static glPopDebugGroup_t qglPopDebugGroup;
static glObjectLabel_t qglObjectLabel;
static glGetObjectLabel_t qglGetObjectLabel;
#endif
// </debug-functions>

// Debugging
// In Visual Studio, messages from the driver appear in Output tab
void APIENTRY MessageCallback(GLenum source,
							  GLenum type,
							  GLuint id,
							  GLenum severity,
							  GLsizei length,
							  const GLchar* message,
							  const void* userParam
)
{
	if (source != GL_DEBUG_SOURCE_APPLICATION) {
		char buffer[1024] = { 0 };

		if (type == GL_DEBUG_TYPE_ERROR) {
			snprintf(buffer, sizeof(buffer) - 1,
					 "GL CALLBACK: ** GL ERROR ** type = 0x%x, severity = 0x%x, message = %s\n",
					 type, severity, message);
		}
		else {
			snprintf(buffer, sizeof(buffer) - 1,
					 "GL CALLBACK: type = 0x%x, severity = 0x%x, message = %s\n",
					 type, severity, message);
		}

#ifdef _WIN32
		if (IsDebuggerPresent()) {
			OutputDebugString(buffer);
		}
		else {
			printf("[OGL] %s\n", buffer);
		}
#else
		printf("[OGL] %s\n", buffer);
#endif
	}
}

void GL_InitialiseDebugging(void)
{
	// During init, enable debug output
	if (R_DebugProfileContext()) {
		glDebugMessageCallback_t glDebugMessageCallback = (glDebugMessageCallback_t)SDL_GL_GetProcAddress("glDebugMessageCallback");
		glDebugMessageControl_t glDebugMessageControl = (glDebugMessageControl_t)SDL_GL_GetProcAddress("glDebugMessageControl");

		if (glDebugMessageCallback) {
			glEnable(GL_DEBUG_OUTPUT);
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			glDebugMessageCallback((GLDEBUGPROC)MessageCallback, 0);
		}

		if (glDebugMessageControl) {
			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
		}
	}

#ifdef WITH_RENDERING_TRACE
	qglObjectLabel = (glObjectLabel_t)SDL_GL_GetProcAddress("glObjectLabel");
	qglGetObjectLabel = (glGetObjectLabel_t)SDL_GL_GetProcAddress("glGetObjectLabel");
	qglPushDebugGroup = (glPushDebugGroup_t)SDL_GL_GetProcAddress("glPushDebugGroup");
	qglPopDebugGroup = (glPopDebugGroup_t)SDL_GL_GetProcAddress("glPopDebugGroup");
#endif
}

#ifdef WITH_RENDERING_TRACE
static int debug_frame_depth = 0;
static unsigned long regions_trace_only;
static qbool dev_frame_debug_queued;
static FILE* debug_frame_out;

#define DEBUG_FRAME_DEPTH_CHARS 2

void R_TraceEnterRegion(const char* regionName, qbool trace_only)
{
	if (R_UseModernOpenGL()) {
		if (!trace_only && qglPushDebugGroup) {
			qglPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, regionName);
		}
	}
	else if (debug_frame_out) {
		fprintf(debug_frame_out, "Enter: %.*s %s {\n", debug_frame_depth, "                                                          ", regionName);
		debug_frame_depth += DEBUG_FRAME_DEPTH_CHARS;
	}

	regions_trace_only <<= 1;
	regions_trace_only &= (trace_only ? 1 : 0);
}

void R_TraceLeaveRegion(qbool trace_only)
{
	if (R_UseModernOpenGL()) {
		if (!trace_only && qglPopDebugGroup) {
			qglPopDebugGroup();
		}
	}
	else if (debug_frame_out) {
		debug_frame_depth -= DEBUG_FRAME_DEPTH_CHARS;
		debug_frame_depth = max(debug_frame_depth, 0);
		fprintf(debug_frame_out, "Leave: %.*s }\n", debug_frame_depth, "                                                          ");
	}
}

qbool R_TraceLoggingEnabled(void)
{
	return debug_frame_out != NULL;
}

void R_TraceLogAPICallDirect(const char* format, ...)
{
	if (debug_frame_out) {
		va_list argptr;
		char msg[4096];

		va_start(argptr, format);
		vsnprintf(msg, sizeof(msg), format, argptr);
		va_end(argptr);

		fprintf(debug_frame_out, "API:   %.*s %s\n", debug_frame_depth, "                                                          ", msg);
		fflush(debug_frame_out);
	}
}

void R_TraceResetRegion(qbool start)
{
	if (start && debug_frame_out) {
		fclose(debug_frame_out);
		debug_frame_out = NULL;
	}
	else if (start && dev_frame_debug_queued) {
		char fileName[MAX_PATH];
#ifndef _WIN32
		time_t t;
		struct tm date;
		t = time(NULL);
		localtime_r(&t, &date);

		snprintf(fileName, sizeof(fileName), "%s/qw/frame_%04d-%02d-%02d_%02d-%02d-%02d.txt",
				 com_basedir, 1900 + date.tm_year, 1 + date.tm_mon, date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec);
#else
		SYSTEMTIME date;
		GetLocalTime(&date);

		snprintf(fileName, sizeof(fileName), "%s/qw/frame_%04d-%02d-%02d_%02d-%02d-%02d.txt",
				 com_basedir, date.wYear, date.wMonth, date.wDay, date.wHour, date.wMinute, date.wSecond);
#endif

		debug_frame_out = fopen(fileName, "wt");
		dev_frame_debug_queued = false;
	}

	if (R_UseImmediateOpenGL() && debug_frame_out) {
		fprintf(debug_frame_out, "---Reset---\n");
		debug_frame_depth = 0;
	}
}

void GL_TraceObjectLabelSet(GLenum identifier, GLuint name, int length, const char* label)
{
	if (qglObjectLabel) {
		qglObjectLabel(identifier, name, length, label ? label : "?");
	}
}

void GL_TraceObjectLabelGet(GLenum identifier, GLuint name, int bufSize, int* length, char* label)
{
	label[0] = '\0';
	if (qglGetObjectLabel) {
		qglGetObjectLabel(identifier, name, bufSize, length, label);
	}
}

void GL_TextureLabelSet(texture_ref ref, const char* label)
{
	GL_TraceObjectLabelSet(GL_TEXTURE, GL_TextureNameFromReference(ref), -1, label);
}

void GL_TextureLabelGet(texture_ref ref, int bufSize, int* length, char* label)
{
	GL_TraceObjectLabelGet(GL_TEXTURE, GL_TextureNameFromReference(ref), bufSize, length, label);
}

void Dev_VidFrameTrace(void)
{
	dev_frame_debug_queued = true;
}

void R_TraceDebugState(void)
{
	if (debug_frame_out) {
		R_TracePrintState(debug_frame_out, debug_frame_depth);
	}
}

void Dev_VidTextureDump(void)
{
	char folder[MAX_PATH];
	byte* buffer = NULL;
	int buffer_size = 0;
	int i;
#ifndef _WIN32
	time_t t;
	struct tm date;
	t = time(NULL);
	localtime_r(&t, &date);

	snprintf(folder, sizeof(folder), "%s/qw/textures_%04d-%02d-%02d_%02d-%02d-%02d",
		com_basedir, 1900 + date.tm_year, 1 + date.tm_mon, date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec);
#else
	SYSTEMTIME date;
	GetLocalTime(&date);

	snprintf(folder, sizeof(folder), "%s/qw/textures_%04d-%02d-%02d_%02d-%02d-%02d",
		com_basedir, date.wYear, date.wMonth, date.wDay, date.wHour, date.wMinute, date.wSecond);
#endif

	strlcat(folder, "/", sizeof(folder));
	FS_CreatePath(folder);

	for (i = 0; i < R_TextureCount(); ++i) {
		texture_ref ref = { i };

		if (R_TextureReferenceIsValid(ref)) {
			int size = R_TextureWidth(ref) * R_TextureHeight(ref) * 4;
			if (size > buffer_size) {
				Q_free(buffer);
				buffer = Q_malloc(size);
				buffer_size = size;
			}
			else if (buffer_size) {
				memset(buffer, 0, buffer_size);
			}

			if (!size) {
				continue;
			}

			if (size) {
				char reftext[10];
				char filename[MAX_OSPATH];

				snprintf(reftext, sizeof(reftext), "%03d", i);
				strlcpy(filename, reftext, sizeof(filename));
				strlcat(filename, "-", sizeof(filename));
				strlcat(filename, R_TextureIdentifier(ref), sizeof(filename));
				Util_ToValidFileName(filename, filename, sizeof(filename));

				if (R_TextureType(ref) == texture_type_2d) {
					scr_sshot_target_t* sshot_params = Q_malloc(sizeof(scr_sshot_target_t));

					renderer.TextureGet(ref, buffer_size, buffer, 3);

					COM_ForceExtension(filename, ".jpg");
					sshot_params->buffer = buffer;
					sshot_params->freeMemory = false;
					sshot_params->format = IMAGE_JPEG;
					strlcpy(sshot_params->fileName, folder, sizeof(sshot_params->fileName));
					strlcat(sshot_params->fileName, filename, sizeof(sshot_params->fileName));
					sshot_params->width = R_TextureWidth(ref);
					sshot_params->height = R_TextureHeight(ref);
					SCR_ScreenshotWrite(sshot_params);
				}
				else if (R_TextureType(ref) == texture_type_cubemap) {
					char side_filename[MAX_OSPATH];
					const char* side_labels[] = { "-pos-x", "-neg-x", "-pos-y", "-neg-y", "-pos-z", "-neg-z" };
					int j;

					GL_BindTextureUnit(GL_TEXTURE0, ref);
					for (j = 0; j < 6; ++j) {
						scr_sshot_target_t* sshot_params = Q_malloc(sizeof(scr_sshot_target_t));

						strlcpy(side_filename, filename, sizeof(side_filename));
						strlcat(side_filename, side_labels[j], sizeof(side_filename));
						COM_ForceExtension(side_filename, ".jpg");

						glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + j, 0, GL_RGB, GL_UNSIGNED_BYTE, buffer);

						sshot_params->buffer = buffer;
						sshot_params->freeMemory = false;
						sshot_params->format = IMAGE_JPEG;
						strlcpy(sshot_params->fileName, folder, sizeof(sshot_params->fileName));
						strlcat(sshot_params->fileName, side_filename, sizeof(sshot_params->fileName));
						sshot_params->width = R_TextureWidth(ref);
						sshot_params->height = R_TextureHeight(ref);
						SCR_ScreenshotWrite(sshot_params);
					}
				}
			}
		}
	}

	Q_free(buffer);
}
#endif
