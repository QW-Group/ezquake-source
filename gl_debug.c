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

// <debug-functions (4.3)>
//typedef void (APIENTRY *DEBUGPROC)(GLenum source, GLenum type, GLuint id, GLenum severity,  GLsizei length, const GLchar *message, const void *userParam);
typedef void (APIENTRY *glDebugMessageCallback_t)(GLDEBUGPROC callback, void* userParam);

#ifdef WITH_OPENGL_TRACE
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
#ifdef _WIN32

// Not sure what to do with this on non-windows systems... essentially in Visual Studio, messages
//  from the driver appear in Output tab
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

		OutputDebugString(buffer);
	}
}
#endif

void GL_InitialiseDebugging(void)
{
#ifdef _WIN32
	// During init, enable debug output
	if (GL_DebugProfileContext() && IsDebuggerPresent()) {
		glDebugMessageCallback_t glDebugMessageCallback = (glDebugMessageCallback_t)SDL_GL_GetProcAddress("glDebugMessageCallback");

		if (glDebugMessageCallback) {
			glEnable(GL_DEBUG_OUTPUT);
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			glDebugMessageCallback((GLDEBUGPROC)MessageCallback, 0);
		}
	}
#endif

#ifdef WITH_OPENGL_TRACE
	qglObjectLabel = (glObjectLabel_t)SDL_GL_GetProcAddress("glObjectLabel");
	qglGetObjectLabel = (glGetObjectLabel_t)SDL_GL_GetProcAddress("glGetObjectLabel");
	qglPushDebugGroup = (glPushDebugGroup_t)SDL_GL_GetProcAddress("glPushDebugGroup");
	qglPopDebugGroup = (glPopDebugGroup_t)SDL_GL_GetProcAddress("glPopDebugGroup");
#endif
}

#ifdef WITH_OPENGL_TRACE
static int debug_frame_depth = 0;
static unsigned long regions_trace_only;
static qbool dev_frame_debug_queued;
static FILE* debug_frame_out;

#define DEBUG_FRAME_DEPTH_CHARS 2

void GL_EnterTracedRegion(const char* regionName, qbool trace_only)
{
	if (GL_UseGLSL()) {
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

void GL_LeaveTracedRegion(qbool trace_only)
{
	if (GL_UseGLSL()) {
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

void GL_MarkEvent(const char* format, ...)
{
	va_list argptr;
	char msg[4096];

	va_start(argptr, format);
	vsnprintf(msg, sizeof(msg), format, argptr);
	va_end(argptr);

	if (GL_UseGLSL()) {
		//nvtxMark(va(msg));
	}
	else if (debug_frame_out) {
		fprintf(debug_frame_out, "Event: %.*s %s\n", debug_frame_depth, "                                                          ", msg);
	}
}

qbool GL_LoggingEnabled(void)
{
	return debug_frame_out != NULL;
}

void GL_LogAPICall(const char* format, ...)
{
	if (GL_UseImmediateMode() && debug_frame_out) {
		va_list argptr;
		char msg[4096];

		va_start(argptr, format);
		vsnprintf(msg, sizeof(msg), format, argptr);
		va_end(argptr);

		fprintf(debug_frame_out, "API:   %.*s %s\n", debug_frame_depth, "                                                          ", msg);
	}
}

void GL_ResetRegion(qbool start)
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
				 com_basedir, date.tm_year, date.tm_mon, date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec);
#else
		SYSTEMTIME date;
		GetLocalTime(&date);

		snprintf(fileName, sizeof(fileName), "%s/qw/frame_%04d-%02d-%02d_%02d-%02d-%02d.txt",
				 com_basedir, date.wYear, date.wMonth, date.wDay, date.wHour, date.wMinute, date.wSecond);
#endif

		debug_frame_out = fopen(fileName, "wt");
		dev_frame_debug_queued = false;
	}

	if (GL_UseImmediateMode() && debug_frame_out) {
		fprintf(debug_frame_out, "---Reset---\n");
		debug_frame_depth = 0;
	}
}

void GL_ObjectLabel(GLenum identifier, GLuint name, GLsizei length, const char* label)
{
	if (qglObjectLabel) {
		qglObjectLabel(identifier, name, length, label ? label : "?");
	}
}

void GL_GetObjectLabel(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei* length, char* label)
{
	if (qglGetObjectLabel) {
		qglGetObjectLabel(identifier, name, bufSize, length, label);
	}
}

void Dev_VidFrameTrace(void)
{
	dev_frame_debug_queued = true;
}

void GL_DebugState(void)
{
	if (debug_frame_out) {
		GL_PrintState(debug_frame_out);
	}
}
#endif
