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

#ifdef WITH_RENDERING_TRACE
#define R_TraceEnterFunctionRegion R_TraceEnterRegion(__FUNCTION__, true)
#define R_TraceLeaveFunctionRegion R_TraceLeaveRegion(true)
#define R_TraceEnterNamedRegion(x) R_TraceEnterRegion(x, false)
#define R_TraceLeaveNamedRegion() R_TraceLeaveRegion(false)
void R_TraceEnterRegion(const char* regionName, qbool trace_only);
void R_TraceLeaveRegion(qbool trace_only);
void R_TracePrintState(FILE* output, int indent);
void R_TraceDebugState(void);
void R_TraceResetRegion(qbool start);
void R_TraceLogAPICallDirect(const char* message, ...);
#define R_TraceLogAPICall(...) { R_TraceLogAPICallDirect(__VA_ARGS__); }
void R_TraceAPI(const char* formatString, ...);
qbool R_TraceLoggingEnabled(void);
void R_TraceTextureLabelGet(unsigned int name, int bufSize, int* length, char* label);
#else
#define R_TraceEnterFunctionRegion
#define R_TraceLeaveFunctionRegion
#define R_TraceEnterNamedRegion(x)
#define R_TraceLeaveNamedRegion()
#define R_TraceEnterRegion(...)
#define R_TraceLeaveRegion(...)
#define R_TracePrintState(...)
#define R_TraceDebugState()
#define R_TraceResetRegion(x)
#define R_TraceLogAPICallDirect(x, ...)
#define R_TraceLogAPICall(...)
#define R_TraceAPI(formatString, ...)
#define R_TraceLoggingEnabled() (false)
#define R_TraceTextureLabelSet(...)
#define R_TraceTextureLabelGet(...)
#endif

#endif // EZQUAKE_R_TRACE_HEADER
