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

#ifndef EZQUAKE_R_TRACE_HEADER
#define EZQUAKE_R_TRACE_HEADER

#ifdef WITH_OPENGL_TRACE
#define ENTER_STATE GL_EnterTracedRegion(__FUNCTION__, true)
#define LEAVE_STATE GL_LeaveTracedRegion(true)
#define GL_EnterRegion(x) GL_EnterTracedRegion(x, false)
#define GL_LeaveRegion() GL_LeaveTracedRegion(false)
void GL_EnterTracedRegion(const char* regionName, qbool trace_only);
void GL_LeaveTracedRegion(qbool trace_only);
void GL_PrintState(FILE* output, int indent);
void GL_DebugState(void);
void GL_ResetRegion(qbool start);
void GL_LogAPICall2(const char* message, ...);
#define GL_LogAPICall(...) { if (R_UseImmediateOpenGL()) { GL_LogAPICall2(__VA_ARGS__); }}
qbool GL_LoggingEnabled(void);
void GL_ObjectLabel(unsigned int identifier, unsigned int name, int length, const char* label);
void GL_GetObjectLabel(unsigned int identifier, unsigned int name, int bufSize, int* length, char* label);
void GL_TextureLabel(unsigned int name, const char* label);
void GL_GetTextureLabel(unsigned int name, int bufSize, int* length, char* label);
#else
#define ENTER_STATE
#define LEAVE_STATE
#define GL_EnterTracedRegion(...)
#define GL_LeaveTracedRegion(...)
#define GL_EnterRegion(x)
#define GL_LeaveRegion()
#define GL_ResetRegion(x)
#define GL_LogAPICall(...)
#define GL_PrintState(...)
#define GL_DebugState()
#define GL_LoggingEnabled() (false)
#define GL_ObjectLabel(...)
#define GL_GetObjectLabel(...)
#endif

#endif // EZQUAKE_R_TRACE_HEADER
