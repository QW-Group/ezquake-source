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
#include "gl_texture_internal.h"

#ifdef WITH_RENDERING_TRACE
static int debug_frame_depth = 0;
static unsigned long regions_trace_only;
static qbool dev_frame_debug_queued;
static FILE* debug_frame_out;
void GL_VerifyState(FILE* output);

#define DEBUG_FRAME_DEPTH_CHARS 2
#endif

GLuint GL_TextureNameFromReference(texture_ref ref);

// <debug-functions (4.3)>
GL_StaticProcedureDeclaration(glDebugMessageCallback, "callback=%p, userParam=%p", GLDEBUGPROC callback, void* userParam)
GL_StaticProcedureDeclaration(glDebugMessageControl, "source=%u, type=%u, severity=%u, count=%d, ids=%p, enabled=%d", GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint* ids, GLboolean enabled)

#ifdef WITH_RENDERING_TRACE
GL_StaticProcedureDeclaration(glPushDebugGroup, "source=%u, id=%u, length=%d, message=%s", GLenum source, GLuint id, GLsizei length, const char* message)
GL_StaticProcedureDeclaration(glPopDebugGroup, "", void)
GL_StaticProcedureDeclaration(glObjectLabel, "identifier=%u, name=%u, length=%d, label=%s", GLenum identifier, GLuint name, GLsizei length, const char* label)
GL_StaticProcedureDeclaration(glGetObjectLabel, "identifier=%u, name=%u, bufSize=%d, length=%p, label=%s", GLenum identifier, GLuint name, GLsizei bufSize, GLsizei* length, char* label)
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
	// FIXME: think this is caused by having fixed maximum array of samplers
	// - either bind to solidblack or have multiple programs and switch depending on # of texture units being used
	if (severity == GL_DEBUG_SEVERITY_LOW && strstr(message, "texture object (0) bound to")) {
		return;
	}

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

#ifdef WITH_RENDERING_TRACE
		if (debug_frame_out) {
			fprintf(debug_frame_out, "%s", buffer);
			fflush(debug_frame_out);
		}
#endif
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
		GL_LoadOptionalFunction(glDebugMessageCallback);
		GL_LoadOptionalFunction(glDebugMessageControl);

		if (GL_Available(glDebugMessageCallback) && !COM_CheckParm(cmdline_param_client_nocallback)) {
			glEnable(GL_DEBUG_OUTPUT);
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			GL_Procedure(glDebugMessageCallback, (GLDEBUGPROC)MessageCallback, 0);
		}

		if (GL_Available(glDebugMessageControl) && !COM_CheckParm(cmdline_param_client_nocallback)) {
			GL_Procedure(glDebugMessageControl, GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
		}
	}

#ifdef WITH_RENDERING_TRACE
	GL_LoadOptionalFunction(glObjectLabel);
	GL_LoadOptionalFunction(glGetObjectLabel);
	GL_LoadOptionalFunction(glPushDebugGroup);
	GL_LoadOptionalFunction(glPopDebugGroup);
#endif
}

#ifdef WITH_RENDERING_TRACE
void R_TraceEnterRegion(const char* regionName, qbool trace_only)
{
	if (debug_frame_out) {
		fprintf(debug_frame_out, "Enter: %.*s %s {\n", debug_frame_depth, "                                                          ", regionName);
		fflush(debug_frame_out);
		debug_frame_depth += DEBUG_FRAME_DEPTH_CHARS;
	}
	else if (R_UseModernOpenGL()) {
		if (!trace_only && GL_Available(glPushDebugGroup)) {
			GL_Procedure(glPushDebugGroup, GL_DEBUG_SOURCE_APPLICATION, 0, -1, regionName);
		}
	}

	regions_trace_only <<= 1;
	regions_trace_only &= (trace_only ? 1 : 0);
}

void R_TraceLeaveRegion(qbool trace_only)
{
	if (debug_frame_out) {
		debug_frame_depth -= DEBUG_FRAME_DEPTH_CHARS;
		debug_frame_depth = max(debug_frame_depth, 0);
		fprintf(debug_frame_out, "Leave: %.*s }\n", debug_frame_depth, "                                                          ");
		fflush(debug_frame_out);
	}
	else if (R_UseModernOpenGL()) {
		if (!trace_only && GL_Available(glPopDebugGroup)) {
			GL_Procedure(glPopDebugGroup);
		}
	}
}

qbool R_TraceLoggingEnabled(void)
{
	return debug_frame_out != NULL;
}

void R_TraceAPI(const char* format, ...)
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
	else  {

	}
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
		dev_frame_debug_queued = COM_CheckParm(cmdline_param_client_video_r_trace);
	}

	if (start && dev_frame_debug_queued) {
		char fileName[MAX_PATH];
#ifndef _WIN32
		time_t t;
		struct tm date;
		t = time(NULL);
		localtime_r(&t, &date);

		snprintf(fileName, sizeof(fileName), "%s/qw/%s/frame_%04d-%02d-%02d_%02d-%02d-%02d.txt",
				 com_basedir, COM_CheckParm(cmdline_param_client_video_r_trace) ? "trace/" : "", 1900 + date.tm_year, 1 + date.tm_mon, date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec);
#else
		SYSTEMTIME date;
		int i;

		GetLocalTime(&date);

		for (i = 0; i < 1000; ++i) {
			FILE* temp;

			snprintf(fileName, sizeof(fileName), "%s/qw/%s/frame_%04d-%02d-%02d_%02d-%02d-%02d_%04d.txt",
				com_basedir, COM_CheckParm(cmdline_param_client_video_r_trace) ? "trace/" : "", date.wYear, date.wMonth, date.wDay, date.wHour, date.wMinute, date.wSecond, i);

			if ((temp = fopen(fileName, "rt"))) {
				fclose(temp);
				continue;
			}

			break;
		}
#endif

		if (COM_CheckParm(cmdline_param_client_video_r_trace)) {
			Sys_mkdir(va("%s/qw/trace", com_basedir));
		}

		debug_frame_out = fopen(fileName, "wt");
		dev_frame_debug_queued = false;
	}

	if (debug_frame_out) {
		fprintf(debug_frame_out, "---Reset---\n");
		fflush(debug_frame_out);
		debug_frame_depth = 0;
	}
}

void GL_TraceObjectLabelSet(GLenum identifier, GLuint name, int length, const char* label)
{
	if (GL_Available(glObjectLabel)) {
		GL_Procedure(glObjectLabel, identifier, name, length, label ? label : "?");
	}
}

void GL_TraceObjectLabelGet(GLenum identifier, GLuint name, int bufSize, int* length, char* label)
{
	label[0] = '\0';
	if (GL_Available(glGetObjectLabel)) {
		GL_Procedure(glGetObjectLabel, identifier, name, bufSize, length, label);
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

void Dev_VidFrameStart(void)
{
	Dev_VidFrameTrace();
	R_TraceResetRegion(true);
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
	if (COM_CheckParm(cmdline_param_client_verify_glstate)) {
		GL_VerifyState(debug_frame_out);
	}
}

void Dev_VidTextureDump(void)
{
	char folder[MAX_PATH];
	byte* buffer = NULL;
	byte* row = NULL;
	int buffer_size = 0;
	int row_size = 0;
	int i, j;
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
			int row_length = R_TextureWidth(ref) * 4;
			int size = R_TextureWidth(ref) * R_TextureHeight(ref) * 4;
			if (size > buffer_size) {
				Q_free(buffer);
				buffer = Q_malloc(size);
				buffer_size = size;
			}
			else if (buffer_size) {
				memset(buffer, 0, buffer_size);
			}

			if (row_length > row_size) {
				Q_free(row);
				row = Q_malloc(row_length);
				row_size = row_length;
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

					COM_ForceExtension(filename, ".png");
					sshot_params->buffer = buffer;
					sshot_params->freeMemory = false;
					sshot_params->format = IMAGE_PNG;
					strlcpy(sshot_params->fileName, folder, sizeof(sshot_params->fileName));
					strlcat(sshot_params->fileName, filename, sizeof(sshot_params->fileName));
					sshot_params->width = R_TextureWidth(ref);
					sshot_params->height = R_TextureHeight(ref);

					for (j = 0; j < sshot_params->height / 2; ++j) {
						size_t row_bytes = sshot_params->width * 3;
						byte* this_row = &buffer[j * row_bytes];
						byte* flipped_row = &buffer[(sshot_params->height - j - 1) * row_bytes];

						memcpy(row, this_row, row_bytes);
						memcpy(this_row, flipped_row, row_bytes);
						memcpy(flipped_row, row, row_bytes);
					}
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
						COM_ForceExtension(side_filename, ".png");

						glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + j, 0, GL_RGB, GL_UNSIGNED_BYTE, buffer);

						sshot_params->buffer = buffer;
						sshot_params->freeMemory = false;
						sshot_params->format = IMAGE_PNG;
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
	Q_free(row);
}

#endif // WITH_RENDERING_TRACE
