/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================

$Id: win_glimp.c,v 1.26 2007-10-27 20:23:36 tonik Exp $

*/
/*
** WIN_GLIMP.C
**
** This file contains ALL Win32 specific stuff having to do with the
** OpenGL refresh.  When a port is being made the following functions
** must be implemented by the port:
**
** GLimp_EndFrame
** GLimp_Init
** GLimp_LogComment
** GLimp_Shutdown
**
** Note that the GLW_xxx functions are Windows specific GL-subsystem
** related functions that are relevant ONLY to win_glimp.c
*/


#include "quakedef.h"
#include "resource.h"
#include "winquake.h"
#include "input.h"
#ifdef GLQUAKE
#include "gl_model.h"
#include "gl_local.h"
#if defined(_WIN32) || defined(__linux__) || defined(__FreeBSD__)
#include "tr_types.h"
#endif // _WIN32 || __linux__ || __FreeBSD__
#else
#include "r_model.h"
#include "r_local.h"
#endif
#include "rulesets.h"
#include "sbar.h"
#include "keys.h"


extern	HWND	mainwindow;
extern  LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

HMONITOR		prevMonitor; // The previous monitor the window was on before getting destroyed.
MONITORINFOEX	prevMonInfo; // Information about the previous monitor the window was on before getting destroyed.

// exported to the client
qbool	vid_vsync_on;
double	vid_vsync_lag;
double	vid_last_swap_time;

void WG_CheckHardwareGamma( void );
void WG_RestoreGamma( void );
void WG_CheckNeedSetDeviceGammaRamp (void);

typedef enum {
	RSERR_OK,

	RSERR_INVALID_FULLSCREEN,
	RSERR_INVALID_MODE,

	RSERR_UNKNOWN
} rserr_t;

#define TRY_PFD_SUCCESS		0
#define TRY_PFD_FAIL_SOFT	1
#define TRY_PFD_FAIL_HARD	2

#define	WINDOW_CLASS_NAME	"ezQuake"
#define WINDOW_DEFAULT_NAME "ezQuake"

static void		GLW_InitExtensions( void );
static rserr_t	GLW_SetMode( const char *drivername, 
							 int mode, 
							 int colorbits, 
							 qbool cdsFullscreen );

//
// variable declarations
//
glwstate_t glw_state;

cvar_t	r_allowSoftwareGL = { "vid_allowSoftwareGL", "0", CVAR_LATCH }; // don't abort out if the pixelformat claims software
cvar_t	r_maskMinidriver  = { "vid_maskMinidriver",  "0", CVAR_LATCH }; // allow a different dll name to be treated as if it were opengl32.dll

static qbool s_classRegistered = false;

// VVD: didn't restore gamma after ALT+TAB on some ATI video cards (or drivers?...) 
// HACK!!! FIXME {
cvar_t	vid_forcerestoregamma = {"vid_forcerestoregamma", "0", CVAR_SILENT};
int		restore_gamma = 0;
// }

//
// function declaration
//

// FIXME: this is a stubs for now...

void	 QGL_EnableLogging( qbool enable ) { /* TODO */ };

void ( APIENTRY * qglGetIntegerv )(GLenum pname, GLint *params);
GLenum ( APIENTRY * qglGetError )(void);
const GLubyte * ( APIENTRY * qglGetString )(GLenum name);

BOOL  ( WINAPI * qwglCopyContext)(HGLRC, HGLRC, UINT);
HGLRC ( WINAPI * qwglCreateContext)(HDC);
HGLRC ( WINAPI * qwglCreateLayerContext)(HDC, int);
BOOL  ( WINAPI * qwglDeleteContext)(HGLRC);
HGLRC ( WINAPI * qwglGetCurrentContext)(VOID);
HDC   ( WINAPI * qwglGetCurrentDC)(VOID);
PROC  ( WINAPI * qwglGetProcAddress)(LPCSTR);
BOOL  ( WINAPI * qwglMakeCurrent)(HDC, HGLRC);
BOOL  ( WINAPI * qwglDescribeLayerPlane)(HDC, int, int, UINT, LPLAYERPLANEDESCRIPTOR);
int   ( WINAPI * qwglSetLayerPaletteEntries)(HDC, int, int, int, CONST COLORREF *);
int   ( WINAPI * qwglGetLayerPaletteEntries)(HDC, int, int, int, COLORREF *);
BOOL  ( WINAPI * qwglRealizeLayerPalette)(HDC, int, BOOL);
BOOL  ( WINAPI * qwglSwapLayerBuffers)(HDC, UINT);
BOOL  ( WINAPI * qwglShareLists)(HGLRC, HGLRC);
BOOL  ( WINAPI * qwglUseFontBitmaps)(HDC, DWORD, DWORD, DWORD);
BOOL  ( WINAPI * qwglUseFontOutlines)(HDC, DWORD, DWORD, DWORD, FLOAT, FLOAT, int, LPGLYPHMETRICSFLOAT);

int   ( WINAPI * qwglChoosePixelFormat )(HDC, CONST PIXELFORMATDESCRIPTOR *);
int   ( WINAPI * qwglDescribePixelFormat) (HDC, int, UINT, LPPIXELFORMATDESCRIPTOR);
int   ( WINAPI * qwglGetPixelFormat)(HDC);
BOOL  ( WINAPI * qwglSetPixelFormat)(HDC, int, CONST PIXELFORMATDESCRIPTOR *);
BOOL  ( WINAPI * qwglSwapBuffers)(HDC);

int ( WINAPI * qwglSwapIntervalEXT)( int interval );
BOOL  ( WINAPI * qwglGetDeviceGammaRamp3DFX)( HDC, LPVOID );
BOOL  ( WINAPI * qwglSetDeviceGammaRamp3DFX)( HDC, LPVOID );

// Finds out what monitor the window is currently on.
HMONITOR VID_GetCurrentMonitor()
{
	return MonitorFromWindow(mainwindow, MONITOR_DEFAULTTOPRIMARY);
}

// Finds out info about the current monitor.
MONITORINFOEX VID_GetCurrentMonitorInfo(HMONITOR monitor)
{
	MONITORINFOEX inf;
	memset(&inf, 0, sizeof(MONITORINFOEX));
	inf.cbSize = sizeof(MONITORINFOEX);
	GetMonitorInfo(monitor, (LPMONITORINFO)&inf);
	return inf;
}

qbool QGL_Init( const char *dllname ) {
	// bombastic function
	ST_Printf( PRINT_R_VERBOSE, "...initializing QGL\n" );

	qglGetIntegerv				 = glGetIntegerv;
	qglGetError					 = glGetError;
	qglGetString				 = glGetString;

	qwglCopyContext              = wglCopyContext;
	qwglCreateContext            = wglCreateContext;
	qwglCreateLayerContext       = wglCreateLayerContext;
	qwglDeleteContext            = wglDeleteContext;
	qwglDescribeLayerPlane       = wglDescribeLayerPlane;
	qwglGetCurrentContext        = wglGetCurrentContext;
	qwglGetCurrentDC             = wglGetCurrentDC;
	qwglGetLayerPaletteEntries   = wglGetLayerPaletteEntries;
	qwglGetProcAddress           = wglGetProcAddress;
	qwglMakeCurrent              = wglMakeCurrent;
	qwglRealizeLayerPalette      = wglRealizeLayerPalette;
	qwglSetLayerPaletteEntries   = wglSetLayerPaletteEntries;
	qwglShareLists               = wglShareLists;
	qwglSwapLayerBuffers         = wglSwapLayerBuffers;
	qwglUseFontBitmaps           = wglUseFontBitmapsA;
	qwglUseFontOutlines          = wglUseFontOutlinesA;

	qwglChoosePixelFormat        = ChoosePixelFormat;
	qwglDescribePixelFormat      = DescribePixelFormat;
	qwglGetPixelFormat           = GetPixelFormat;
	qwglSetPixelFormat           = SetPixelFormat;
	qwglSwapBuffers              = SwapBuffers;

	qwglSwapIntervalEXT			 = 0;
	qwglGetDeviceGammaRamp3DFX	 = NULL;
	qwglSetDeviceGammaRamp3DFX	 = NULL;

	qglActiveTextureARB			 = 0;
	qglClientActiveTextureARB	 = 0;
	qglMultiTexCoord2fARB		 = 0;

	return true;
}

void QGL_Shutdown( void ) {
	ST_Printf( PRINT_R_VERBOSE, "...shutting down QGL\n" );

	qglGetIntegerv				 = NULL;
	qglGetError					 = NULL;
	qglGetString				 = NULL;

	qwglCopyContext              = NULL;
	qwglCreateContext            = NULL;
	qwglCreateLayerContext       = NULL;
	qwglDeleteContext            = NULL;
	qwglDescribeLayerPlane       = NULL;
	qwglGetCurrentContext        = NULL;
	qwglGetCurrentDC             = NULL;
	qwglGetLayerPaletteEntries   = NULL;
	qwglGetProcAddress           = NULL;
	qwglMakeCurrent              = NULL;
	qwglRealizeLayerPalette      = NULL;
	qwglSetLayerPaletteEntries   = NULL;
	qwglShareLists               = NULL;
	qwglSwapLayerBuffers         = NULL;
	qwglUseFontBitmaps           = NULL;
	qwglUseFontOutlines          = NULL;

	qwglChoosePixelFormat        = NULL;
	qwglDescribePixelFormat      = NULL;
	qwglGetPixelFormat           = NULL;
	qwglSetPixelFormat           = NULL;
	qwglSwapBuffers              = NULL;
}


/*
** GLW_StartDriverAndSetMode
*/
static qbool GLW_StartDriverAndSetMode( const char *drivername, 
										   int mode, 
										   int colorbits,
										   qbool cdsFullscreen )
{
	rserr_t err;

	err = GLW_SetMode( drivername, mode, colorbits, cdsFullscreen );

	switch ( err )
	{
	case RSERR_INVALID_FULLSCREEN:
		ST_Printf( PRINT_R_VERBOSE, "...WARNING: fullscreen unavailable in this mode\n" );
		return false;
	case RSERR_INVALID_MODE:
		ST_Printf( PRINT_R_VERBOSE, "...WARNING: could not set the given mode (%d)\n", mode );
		return false;
	default:
		break;
	}
	return true;
}

/*
** ChoosePFD
**
** Helper function that replaces ChoosePixelFormat.
*/
#define MAX_PFDS 256

static int GLW_ChoosePFD( HDC hDC, PIXELFORMATDESCRIPTOR *pPFD )
{
	PIXELFORMATDESCRIPTOR pfds[MAX_PFDS+1];
	int maxPFD = 0;
	int i;
	int bestMatch = 0;

	ST_Printf( PRINT_R_VERBOSE, "...GLW_ChoosePFD( %d, %d, %d )\n", ( int ) pPFD->cColorBits, ( int ) pPFD->cDepthBits, ( int ) pPFD->cStencilBits );

	// count number of PFDs
	if ( glConfig.driverType > GLDRV_ICD )
	{
		maxPFD = qwglDescribePixelFormat( hDC, 1, sizeof( PIXELFORMATDESCRIPTOR ), &pfds[0] );
	}
	else
	{
		maxPFD = DescribePixelFormat( hDC, 1, sizeof( PIXELFORMATDESCRIPTOR ), &pfds[0] );
	}
	if ( maxPFD > MAX_PFDS )
	{
		ST_Printf( PRINT_WARNING, "...numPFDs > MAX_PFDS (%d > %d)\n", maxPFD, MAX_PFDS );
		maxPFD = MAX_PFDS;
	}

	ST_Printf( PRINT_R_VERBOSE, "...%d PFDs found\n", maxPFD - 1 );

	// grab information
	for ( i = 1; i <= maxPFD; i++ )
	{
		if ( glConfig.driverType > GLDRV_ICD )
		{
			qwglDescribePixelFormat( hDC, i, sizeof( PIXELFORMATDESCRIPTOR ), &pfds[i] );
		}
		else
		{
			DescribePixelFormat( hDC, i, sizeof( PIXELFORMATDESCRIPTOR ), &pfds[i] );
		}
	}

	// look for a best match
	for ( i = 1; i <= maxPFD; i++ )
	{
		//
		// make sure this has hardware acceleration
		//
		if ( ( pfds[i].dwFlags & PFD_GENERIC_FORMAT ) != 0 ) 
		{
			if ( !r_allowSoftwareGL.integer )
			{
				if ( r_verbose.integer )
				{
					ST_Printf( PRINT_ALL, "...PFD %d rejected, software acceleration\n", i );
				}
				continue;
			}
		}

		// verify pixel type
		if ( pfds[i].iPixelType != PFD_TYPE_RGBA )
		{
			if ( r_verbose.integer )
			{
				ST_Printf( PRINT_ALL, "...PFD %d rejected, not RGBA\n", i );
			}
			continue;
		}

		// verify proper flags
		if ( ( ( pfds[i].dwFlags & pPFD->dwFlags ) & pPFD->dwFlags ) != pPFD->dwFlags ) 
		{
			if ( r_verbose.integer )
			{
				ST_Printf( PRINT_ALL, "...PFD %d rejected, improper flags (%x instead of %x)\n", i, pfds[i].dwFlags, pPFD->dwFlags );
			}
			continue;
		}

		// verify enough bits
		if ( pfds[i].cDepthBits < 15 )
		{
			continue;
		}
		if ( ( pfds[i].cStencilBits < 4 ) && ( pPFD->cStencilBits > 0 ) )
		{
			continue;
		}

		//
		// selection criteria (in order of priority):
		// 
		//  PFD_STEREO
		//  colorBits
		//  depthBits
		//  stencilBits
		//
		if ( bestMatch )
		{
			// check stereo
			if ( ( pfds[i].dwFlags & PFD_STEREO ) && ( !( pfds[bestMatch].dwFlags & PFD_STEREO ) ) && ( pPFD->dwFlags & PFD_STEREO ) )
			{
				bestMatch = i;
				continue;
			}
			
			if ( !( pfds[i].dwFlags & PFD_STEREO ) && ( pfds[bestMatch].dwFlags & PFD_STEREO ) && ( pPFD->dwFlags & PFD_STEREO ) )
			{
				bestMatch = i;
				continue;
			}

			// check color
			if ( pfds[bestMatch].cColorBits != pPFD->cColorBits )
			{
				// prefer perfect match
				if ( pfds[i].cColorBits == pPFD->cColorBits )
				{
					bestMatch = i;
					continue;
				}
				// otherwise if this PFD has more bits than our best, use it
				else if ( pfds[i].cColorBits > pfds[bestMatch].cColorBits )
				{
					bestMatch = i;
					continue;
				}
			}

			// check depth
			if ( pfds[bestMatch].cDepthBits != pPFD->cDepthBits )
			{
				// prefer perfect match
				if ( pfds[i].cDepthBits == pPFD->cDepthBits )
				{
					bestMatch = i;
					continue;
				}
				// otherwise if this PFD has more bits than our best, use it
				else if ( pfds[i].cDepthBits > pfds[bestMatch].cDepthBits )
				{
					bestMatch = i;
					continue;
				}
			}

			// check stencil
			if ( pfds[bestMatch].cStencilBits != pPFD->cStencilBits )
			{
				// prefer perfect match
				if ( pfds[i].cStencilBits == pPFD->cStencilBits )
				{
					bestMatch = i;
					continue;
				}
				// otherwise if this PFD has more bits than our best, use it
				else if ( ( pfds[i].cStencilBits > pfds[bestMatch].cStencilBits ) && 
					 ( pPFD->cStencilBits > 0 ) )
				{
					bestMatch = i;
					continue;
				}
			}
		}
		else
		{
			bestMatch = i;
		}
	}
	
	if ( !bestMatch )
		return 0;

	if ( ( pfds[bestMatch].dwFlags & PFD_GENERIC_FORMAT ) != 0 )
	{
		if ( !r_allowSoftwareGL.integer )
		{
			ST_Printf( PRINT_R_VERBOSE, "...no hardware acceleration found\n" );
			return 0;
		}
		else
		{
			ST_Printf( PRINT_R_VERBOSE, "...using software emulation\n" );
		}
	}
	else if ( pfds[bestMatch].dwFlags & PFD_GENERIC_ACCELERATED )
	{
		ST_Printf( PRINT_R_VERBOSE, "...MCD acceleration found\n" );
	}
	else
	{
		ST_Printf( PRINT_R_VERBOSE, "...hardware acceleration found\n" );
	}

	*pPFD = pfds[bestMatch];

	return bestMatch;
}

/*
** void GLW_CreatePFD
**
** Helper function zeros out then fills in a PFD
*/
static void GLW_CreatePFD( PIXELFORMATDESCRIPTOR *pPFD, int colorbits, int depthbits, int stencilbits, qbool stereo )
{
    PIXELFORMATDESCRIPTOR src = 
	{
		sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
		1,								// version number
		PFD_DRAW_TO_WINDOW |			// support window
		PFD_SUPPORT_OPENGL |			// support OpenGL
		PFD_DOUBLEBUFFER,				// double buffered
		PFD_TYPE_RGBA,					// RGBA type
		24,								// 24-bit color depth
		0, 0, 0, 0, 0, 0,				// color bits ignored
		0,								// no alpha buffer
		0,								// shift bit ignored
		0,								// no accumulation buffer
		0, 0, 0, 0, 					// accum bits ignored
		24,								// 24-bit z-buffer	
		8,								// 8-bit stencil buffer
		0,								// no auxiliary buffer
		PFD_MAIN_PLANE,					// main layer
		0,								// reserved
		0, 0, 0							// layer masks ignored
    };

	src.cColorBits = colorbits;
	src.cDepthBits = depthbits;
	src.cStencilBits = stencilbits;

	if ( stereo )
	{
		ST_Printf( PRINT_R_VERBOSE, "...attempting to use stereo\n" );
		src.dwFlags |= PFD_STEREO;
		glConfig.stereoEnabled = true;
	}
	else
	{
		glConfig.stereoEnabled = false;
	}

	*pPFD = src;
}

/*
** GLW_MakeContext
*/
static int GLW_MakeContext( PIXELFORMATDESCRIPTOR *pPFD )
{
	int pixelformat;

	//
	// don't putz around with pixelformat if it's already set (e.g. this is a soft
	// reset of the graphics system)
	//
	if ( !glw_state.pixelFormatSet )
	{
		//
		// choose, set, and describe our desired pixel format.  If we're
		// using a minidriver then we need to bypass the GDI functions,
		// otherwise use the GDI functions.
		//
		if ( ( pixelformat = GLW_ChoosePFD( glw_state.hDC, pPFD ) ) == 0 )
		{
			ST_Printf( PRINT_R_VERBOSE, "...GLW_ChoosePFD failed\n");
			return TRY_PFD_FAIL_SOFT;
		}
		ST_Printf( PRINT_R_VERBOSE, "...PIXELFORMAT %d selected\n", pixelformat );

		if ( glConfig.driverType > GLDRV_ICD )
		{
			qwglDescribePixelFormat( glw_state.hDC, pixelformat, sizeof( *pPFD ), pPFD );
			if ( qwglSetPixelFormat( glw_state.hDC, pixelformat, pPFD ) == FALSE )
			{
				ST_Printf ( PRINT_R_VERBOSE, "...qwglSetPixelFormat failed\n");
				return TRY_PFD_FAIL_SOFT;
			}
		}
		else
		{
			DescribePixelFormat( glw_state.hDC, pixelformat, sizeof( *pPFD ), pPFD );

			if ( SetPixelFormat( glw_state.hDC, pixelformat, pPFD ) == FALSE )
			{
				ST_Printf (PRINT_R_VERBOSE, "...SetPixelFormat failed\n" );
				return TRY_PFD_FAIL_SOFT;
			}
		}

		glw_state.pixelFormatSet = true;
	}

	//
	// startup the OpenGL subsystem by creating a context and making it current
	//
	if ( !glw_state.hGLRC )
	{
		ST_Printf( PRINT_R_VERBOSE, "...creating GL context: " );
		if ( ( glw_state.hGLRC = qwglCreateContext( glw_state.hDC ) ) == 0 )
		{
			ST_Printf (PRINT_R_VERBOSE, "failed\n");

			return TRY_PFD_FAIL_HARD;
		}
		ST_Printf( PRINT_R_VERBOSE, "succeeded\n" );

		ST_Printf( PRINT_R_VERBOSE, "...making context current: " );
		if ( !qwglMakeCurrent( glw_state.hDC, glw_state.hGLRC ) )
		{
			qwglDeleteContext( glw_state.hGLRC );
			glw_state.hGLRC = NULL;
			ST_Printf (PRINT_R_VERBOSE, "failed\n");
			return TRY_PFD_FAIL_HARD;
		}
		ST_Printf( PRINT_R_VERBOSE, "succeeded\n" );
	}

	return TRY_PFD_SUCCESS;
}


/*
** GLW_InitDriver
**
** - get a DC if one doesn't exist
** - create an HGLRC if one doesn't exist
*/
static qbool GLW_InitDriver( const char *drivername, int colorbits )
{
	int		tpfd;
	int		depthbits, stencilbits;
	static PIXELFORMATDESCRIPTOR pfd;		// save between frames since 'tr' gets cleared

	ST_Printf( PRINT_R_VERBOSE, "Initializing OpenGL driver\n" );

	//
	// get a DC for our window if we don't already have one allocated
	//
	if ( glw_state.hDC == NULL )
	{
		ST_Printf( PRINT_R_VERBOSE, "...getting DC: " );

		if ( ( glw_state.hDC = GetDC( mainwindow ) ) == NULL )
		{
			ST_Printf( PRINT_R_VERBOSE, "failed\n" );
			return false;
		}
		ST_Printf( PRINT_R_VERBOSE, "succeeded\n" );
	}

	if ( colorbits == 0 )
	{
		colorbits = glw_state.desktopBitsPixel;
	}

	//
	// implicitly assume Z-buffer depth == desktop color depth
	//
	if ( r_depthbits.integer == 0 ) {
		if ( colorbits > 16 ) {
			depthbits = 24;
		} else {
			depthbits = 16;
		}
	} else {
		depthbits = r_depthbits.integer;
	}

	//
	// do not allow stencil if Z-buffer depth likely won't contain it
	//
	stencilbits = r_stencilbits.integer;
	if ( depthbits < 24 )
	{
		stencilbits = 0;
	}

	//
	// make two attempts to set the PIXELFORMAT
	//

	//
	// first attempt: r_colorbits, depthbits, and r_stencilbits
	//
	if ( !glw_state.pixelFormatSet )
	{
		GLW_CreatePFD( &pfd, colorbits, depthbits, stencilbits, r_stereo.integer );
		if ( ( tpfd = GLW_MakeContext( &pfd ) ) != TRY_PFD_SUCCESS )
		{
			if ( tpfd == TRY_PFD_FAIL_HARD )
			{
				ST_Printf( PRINT_WARNING, "...failed hard\n" );
				return false;
			}

			//
			// punt if we've already tried the desktop bit depth and no stencil bits
			//
			if ( ( r_colorbits.integer == glw_state.desktopBitsPixel ) &&
				 ( stencilbits == 0 ) )
			{
				ReleaseDC( mainwindow, glw_state.hDC );
				glw_state.hDC = NULL;

				ST_Printf( PRINT_R_VERBOSE, "...failed to find an appropriate PIXELFORMAT\n" );

				return false;
			}

			//
			// second attempt: desktop's color bits and no stencil
			//
			if ( colorbits > glw_state.desktopBitsPixel )
			{
				colorbits = glw_state.desktopBitsPixel;
			}
			GLW_CreatePFD( &pfd, colorbits, depthbits, 0, r_stereo.integer );
			if ( GLW_MakeContext( &pfd ) != TRY_PFD_SUCCESS )
			{
				if ( glw_state.hDC )
				{
					ReleaseDC( mainwindow, glw_state.hDC );
					glw_state.hDC = NULL;
				}

				ST_Printf( PRINT_R_VERBOSE, "...failed to find an appropriate PIXELFORMAT\n" );

				return false;
			}
		}

		/*
		** report if stereo is desired but unavailable
		*/
		if ( !( pfd.dwFlags & PFD_STEREO ) && ( r_stereo.integer != 0 ) ) 
		{
			ST_Printf( PRINT_R_VERBOSE, "...failed to select stereo pixel format\n" );
			glConfig.stereoEnabled = false;
		}
	}

	/*
	** store PFD specifics 
	*/
	glConfig.colorBits = ( int ) pfd.cColorBits;
	glConfig.depthBits = ( int ) pfd.cDepthBits;
	glConfig.stencilBits = ( int ) pfd.cStencilBits;

	return true;
}

/*
** GLW_CreateWindow
**
** Responsible for creating the Win32 window and initializing the OpenGL driver.
*/
static qbool GLW_CreateWindow( const char *drivername, int width, int height, int colorbits, qbool cdsFullscreen )
{
	RECT			r;
	int				stylebits;
	int				x, y, w, h;
	int				exstyle;

	//
	// register the window class if necessary
	//
	if ( !s_classRegistered )
	{
		WNDCLASS wc;

		memset( &wc, 0, sizeof( wc ) );

		wc.style         = 0;
		wc.lpfnWndProc   = (WNDPROC)MainWndProc;
		wc.cbClsExtra    = 0;
		wc.cbWndExtra    = 0;
		wc.hInstance     = global_hInstance;
		wc.hIcon         = LoadIcon ( global_hInstance, MAKEINTRESOURCE (IDI_APPICON) );;
		wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
		wc.hbrBackground = (void *)COLOR_GRAYTEXT;
		wc.lpszMenuName  = 0;
		wc.lpszClassName = WINDOW_CLASS_NAME;

		if ( !RegisterClass( &wc ) )
		{
			ST_Printf( PRINT_ERR_FATAL, "GLW_CreateWindow: could not register window class" );
		}
		s_classRegistered = true;
		ST_Printf( PRINT_R_VERBOSE, "...registered window class\n" );
	}

	//
	// create the HWND if one does not already exist
	//
	if ( !mainwindow )
	{
		//
		// compute width and height
		//
		r.left = 0;
		r.top = 0;
		r.right  = width;
		r.bottom = height;

		if ( cdsFullscreen || !strcasecmp( _3DFX_DRIVER_NAME, drivername ) )
		{
			#ifdef _DEBUG
			exstyle   = 0; // this must allow debug in full screen
			#else
			exstyle   = WS_EX_TOPMOST;
			#endif
			stylebits = WS_POPUP|WS_VISIBLE|WS_SYSMENU;
		}
		else
		{
			exstyle   = 0;

			if (vid_borderless.integer)
				stylebits = WS_CHILD|WS_CLIPCHILDREN|WS_VISIBLE|WS_SYSMENU;
			else
				stylebits = WS_OVERLAPPED|WS_BORDER|WS_CAPTION|WS_VISIBLE|WS_SYSMENU;

			AdjustWindowRect (&r, stylebits, FALSE);
		}

		w = r.right - r.left;
		h = r.bottom - r.top;

		if ( cdsFullscreen || !strcasecmp( _3DFX_DRIVER_NAME, drivername ) )
		{
			// TODO: Have a cvar that lets you default to fullscreen on a specific monitor.

			// Do we have a multimonitor setup?
			if (prevMonInfo.cbSize && prevMonitor)
			{
				// When going fullscreen on a monitor other than the primary one
				// we can't just position ourselves at 0,0 since that would
				// be the primary monitors origin...
				RECT rc = prevMonInfo.rcMonitor;
				x = rc.left;
				y = rc.top;
				w = rc.right - rc.left;
				h = rc.bottom - rc.top;
			}
			else
			{
				x = 0;
				y = 0;
			}
		}
		else
		{
			// TODO: Don't assume we only have one monitor here, adjust the coordinates only if they're outside ALL monitors in a multi-monitor setup.
			x = vid_xpos.integer;
			y = vid_ypos.integer;

			// adjust window coordinates if necessary 
			// so that the window is completely on screen
			if ( x < vid_minpos.integer )
				x = vid_minpos.integer;
			if ( y < vid_minpos.integer )
				y = vid_minpos.integer;

			if ( w < glw_state.desktopWidth &&
				 h < glw_state.desktopHeight )
			{
				if ( x + w > glw_state.desktopWidth )
					x = ( glw_state.desktopWidth - w );
				if ( y + h > glw_state.desktopHeight )
					y = ( glw_state.desktopHeight - h );
			}
		}

		mainwindow = CreateWindowEx (
			 exstyle, 
			 WINDOW_CLASS_NAME,
			 WINDOW_DEFAULT_NAME,
			 stylebits,
			 x, y, w, h,
			 (stylebits & WS_CHILD) ? GetDesktopWindow() : NULL, // child require parent window
			 NULL,
			 global_hInstance,
			 NULL);

		if ( !mainwindow )
		{
			ST_Printf (PRINT_ERR_FATAL, "GLW_CreateWindow() - Couldn't create window");
		}
	
		ShowWindow( mainwindow, SW_SHOW );
		UpdateWindow( mainwindow );
		ST_Printf( PRINT_R_VERBOSE, "...created window@%d,%d (%dx%d)\n", x, y, w, h );
	}
	else
	{
		ST_Printf( PRINT_R_VERBOSE, "...window already present, CreateWindowEx skipped\n" );
	}

	if ( !GLW_InitDriver( drivername, colorbits ) )
	{
		ShowWindow( mainwindow, SW_HIDE );
		DestroyWindow( mainwindow );
		mainwindow = NULL;

		return false;
	}

	SetForegroundWindow( mainwindow );
	SetFocus( mainwindow );

	return true;
}

static void PrintCDSError( int value )
{
	switch ( value )
	{
	case DISP_CHANGE_RESTART:
		ST_Printf( PRINT_R_VERBOSE, "restart required\n" );
		break;
	case DISP_CHANGE_BADPARAM:
		ST_Printf( PRINT_R_VERBOSE, "bad param\n" );
		break;
	case DISP_CHANGE_BADFLAGS:
		ST_Printf( PRINT_R_VERBOSE, "bad flags\n" );
		break;
	case DISP_CHANGE_FAILED:
		ST_Printf( PRINT_R_VERBOSE, "DISP_CHANGE_FAILED\n" );
		break;
	case DISP_CHANGE_BADMODE:
		ST_Printf( PRINT_R_VERBOSE, "bad mode\n" );
		break;
	case DISP_CHANGE_NOTUPDATED:
		ST_Printf( PRINT_R_VERBOSE, "not updated\n" );
		break;
	default:
		ST_Printf( PRINT_R_VERBOSE, "unknown error %d\n", value );
		break;
	}
}

/*
** GLW_SetMode
*/
static rserr_t GLW_SetMode( const char *drivername, 
						    int mode, 
							int colorbits, 
							qbool cdsFullscreen )
{
	HDC hDC;
	const char *win_fs[] = { "W", "FS" };
	int		cdsRet;
	DEVMODE dm;
		
	//
	// print out informational messages
	//
	ST_Printf( PRINT_R_VERBOSE, "...setting mode %d:", mode );
	if ( !R_GetModeInfo( &glConfig.vidWidth, &glConfig.vidHeight, &glConfig.windowAspect, mode ) )
	{
		ST_Printf( PRINT_R_VERBOSE, " invalid mode\n" );
		return RSERR_INVALID_MODE;
	}
	ST_Printf( PRINT_R_VERBOSE, " %d %d %s\n", glConfig.vidWidth, glConfig.vidHeight, win_fs[cdsFullscreen] );

	//
	// check our desktop attributes
	//
	hDC = GetDC( GetDesktopWindow() );
	glw_state.desktopBitsPixel = GetDeviceCaps( hDC, BITSPIXEL );
	glw_state.desktopWidth = GetDeviceCaps( hDC, HORZRES );
	glw_state.desktopHeight = GetDeviceCaps( hDC, VERTRES );
	ReleaseDC( GetDesktopWindow(), hDC );

	//
	// verify desktop bit depth
	//
	if ( glConfig.driverType != GLDRV_VOODOO )
	{
		if ( glw_state.desktopBitsPixel < 15 || glw_state.desktopBitsPixel == 24 )
		{
			if ( colorbits == 0 || ( !cdsFullscreen && colorbits >= 15 ) )
			{
				if ( MessageBox( NULL,
							"It is highly unlikely that a correct\n"
							"windowed display can be initialized with\n"
							"the current desktop display depth.  Select\n"
							"'OK' to try anyway.  Press 'Cancel' if you\n"
							"have a 3Dfx Voodoo, Voodoo-2, or Voodoo Rush\n"
							"3D accelerator installed, or if you otherwise\n"
							"wish to quit.",
							"Low Desktop Color Depth",
							MB_OKCANCEL | MB_ICONEXCLAMATION ) != IDOK )
				{
					return RSERR_INVALID_MODE;
				}
			}
		}
	}

	// do a CDS if needed
	if ( cdsFullscreen )
	{
		memset( &dm, 0, sizeof( dm ) );
		dm.dmSize = sizeof( dm );
		
		dm.dmPelsWidth  = glConfig.vidWidth;
		dm.dmPelsHeight = glConfig.vidHeight;
		dm.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT;

		if ( r_displayRefresh.integer != 0 )
		{
			dm.dmDisplayFrequency = r_displayRefresh.integer;
			dm.dmFields |= DM_DISPLAYFREQUENCY;
		}
		
		// try to change color depth if possible
		if ( colorbits != 0 )
		{
			if ( glw_state.allowdisplaydepthchange )
			{
				dm.dmBitsPerPel = colorbits;
				dm.dmFields |= DM_BITSPERPEL;
				ST_Printf( PRINT_R_VERBOSE, "...using colorsbits of %d\n", colorbits );
			}
			else
			{
				ST_Printf( PRINT_R_VERBOSE, "WARNING:...changing depth not supported on Win95 < pre-OSR 2.x\n" );
			}
		}
		else
		{
			ST_Printf( PRINT_R_VERBOSE, "...using desktop display depth of %d\n", glw_state.desktopBitsPixel );
		}

		//
		// if we're already in fullscreen then just create the window
		//
		if ( glw_state.cdsFullscreen )
		{
			ST_Printf( PRINT_R_VERBOSE, "...already fullscreen, avoiding redundant CDS\n" );
		}
		//
		// need to call CDS
		//
		else
		{
			ST_Printf( PRINT_R_VERBOSE, "...calling CDS: " );

			// Try setting the exact mode requested, because some drivers don't report
			// the low res modes in EnumDisplaySettings, but still work
			if ( ( cdsRet = ChangeDisplaySettingsEx( prevMonInfo.szDevice, &dm, NULL, CDS_FULLSCREEN, NULL) ) == DISP_CHANGE_SUCCESSFUL )
			{
				ST_Printf( PRINT_R_VERBOSE, "ok\n" );

				glw_state.cdsFullscreen = true;
			}
			else
			{
				//
				// the exact mode failed, so scan EnumDisplaySettings for the next largest mode
				//
				DEVMODE		devmode;
				int			modeNum;

				ST_Printf( PRINT_R_VERBOSE, "failed, " );

				PrintCDSError( cdsRet );
			
				ST_Printf( PRINT_R_VERBOSE, "...trying next higher resolution:" );
				
				// we could do a better matching job here...
				for ( modeNum = 0 ; ; modeNum++ ) 
				{
					if ( !EnumDisplaySettings( NULL, modeNum, &devmode ) ) 
					{
						modeNum = -1;
						break;
					}
					if ( devmode.dmPelsWidth >= glConfig.vidWidth
						&& devmode.dmPelsHeight >= glConfig.vidHeight
						&& devmode.dmBitsPerPel >= 15 ) 
					{
						break;
					}
				}

				if ( modeNum != -1 && ( cdsRet = ChangeDisplaySettingsEx( prevMonInfo.szDevice, &devmode, NULL, CDS_FULLSCREEN, NULL ) ) == DISP_CHANGE_SUCCESSFUL )
				{
					ST_Printf( PRINT_R_VERBOSE, " ok\n" );
					
					glw_state.cdsFullscreen = true;
				}
				else
				{
					ST_Printf( PRINT_R_VERBOSE, " failed, " );
					
					PrintCDSError( cdsRet );
					
					ST_Printf( PRINT_R_VERBOSE, "...restoring display settings\n" );
					ChangeDisplaySettings( 0, 0 );
					
					glw_state.cdsFullscreen = false;
					glConfig.isFullscreen = false;

					return RSERR_INVALID_FULLSCREEN;
				}
			}
		}
	}
	else
	{
		//
		// Windowed
		//

		if ( glw_state.cdsFullscreen )
		{
			ChangeDisplaySettings( 0, 0 );
		}

		glw_state.cdsFullscreen = false;
	}

	// IMPORTANT! We need to create the window after we have set the things above, some gfx drivers
	// seem to get fps drops if we do it the other way around.
	if ( !GLW_CreateWindow( drivername, glConfig.vidWidth, glConfig.vidHeight, colorbits, cdsFullscreen) )
	{
		ST_Printf( PRINT_R_VERBOSE, "...restoring display settings\n" );
		ChangeDisplaySettings( 0, 0 );
		return RSERR_INVALID_MODE;
	}

	// Make sure we get the proper info for multiple monitor support
	// and position the window correctly after the window is created.
	if (cdsFullscreen)
	{
		RECT rc;
		MONITORINFOEX monInfo;

		// We need to get the new monitor information since the resolution might have changed.
		monInfo = VID_GetCurrentMonitorInfo(prevMonitor);
		rc = monInfo.rcMonitor;

		MoveWindow(mainwindow, rc.left, rc.top, glConfig.vidWidth, glConfig.vidHeight, false);
	}

	//
	// success, now check display frequency, although this won't be valid on Voodoo(2)
	//
	memset( &dm, 0, sizeof( dm ) );
	dm.dmSize = sizeof( dm );
	
	// alt + tab
	{
		glw_state.dm = dm;
		glw_state.vid_canalttab    = false;
		glw_state.vid_wassuspended = false;
	}
	
	if ( EnumDisplaySettings( NULL, ENUM_CURRENT_SETTINGS, &dm ) )
	{
		glConfig.displayFrequency = dm.dmDisplayFrequency;
		// alt + tab
		glw_state.dm = dm;
		glw_state.vid_canalttab = true;
	}

	// NOTE: this is overridden later on standalone 3Dfx drivers
	glConfig.isFullscreen = cdsFullscreen;

	return RSERR_OK;
}

/*
** GLW_InitExtensions
*/
static void GLW_InitExtensions( void )
{
#ifdef GLSL

	/*	glew is not shaders lib, its OpenGL extensions lib,
		but atm we use it for shaders only, so surround with #ifdef */


	// glew lib unable to deinit, so init it even r_allowExtensions is zero
	if ( glewInit () != GLEW_OK )
	{
		ST_Printf( PRINT_R_VERBOSE, "*** ERROR in glewInit ***\n" );
	}
#endif

	if ( !r_allowExtensions.integer )
	{
		ST_Printf( PRINT_ALL, "*** IGNORING OPENGL EXTENSIONS ***\n" );
		return;
	}

	ST_Printf( PRINT_R_VERBOSE, "Initializing OpenGL extensions\n" );

	// WGL_EXT_swap_control
	qwglSwapIntervalEXT = ( BOOL (WINAPI *)(int)) qwglGetProcAddress( "wglSwapIntervalEXT" );
	if ( qwglSwapIntervalEXT )
	{
		ST_Printf( PRINT_R_VERBOSE, "...using WGL_EXT_swap_control\n" );
		r_swapInterval.modified = true;	// force a set next frame
	}
	else
	{
		ST_Printf( PRINT_R_VERBOSE, "...WGL_EXT_swap_control not found\n" );
	}

	// WGL_3DFX_gamma_control
	qwglGetDeviceGammaRamp3DFX = NULL;
	qwglSetDeviceGammaRamp3DFX = NULL;

	if ( strstr( glConfig.extensions_string, "WGL_3DFX_gamma_control" ) )
	{
//		if ( !r_ignorehwgamma->integer && r_ext_gamma_control->integer )
//		{
			qwglGetDeviceGammaRamp3DFX = ( BOOL ( WINAPI * )( HDC, LPVOID ) ) qwglGetProcAddress( "wglGetDeviceGammaRamp3DFX" );
			qwglSetDeviceGammaRamp3DFX = ( BOOL ( WINAPI * )( HDC, LPVOID ) ) qwglGetProcAddress( "wglSetDeviceGammaRamp3DFX" );

			if ( qwglGetDeviceGammaRamp3DFX && qwglSetDeviceGammaRamp3DFX )
			{
				ST_Printf( PRINT_R_VERBOSE, "...using WGL_3DFX_gamma_control\n" );
			}
			else
			{
				qwglGetDeviceGammaRamp3DFX = NULL;
				qwglSetDeviceGammaRamp3DFX = NULL;
			}
//		}
//		else
//		{
//			ST_Printf( PRINT_R_VERBOSE, "...ignoring WGL_3DFX_gamma_control\n" );
//		}
	}
	else
	{
		ST_Printf( PRINT_R_VERBOSE, "...WGL_3DFX_gamma_control not found\n" );
	}
}


/*
** GLW_CheckOSVersion
*/
static qbool GLW_CheckOSVersion( void )
{
#define OSR2_BUILD_NUMBER 1111

	OSVERSIONINFO	vinfo;

	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	glw_state.allowdisplaydepthchange = false;

	if ( GetVersionEx( &vinfo) )
	{
		if ( vinfo.dwMajorVersion > 4 )
		{
			glw_state.allowdisplaydepthchange = true;
		}
		else if ( vinfo.dwMajorVersion == 4 )
		{
			if ( vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT )
			{
				glw_state.allowdisplaydepthchange = true;
			}
			else if ( vinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS )
			{
				if ( LOWORD( vinfo.dwBuildNumber ) >= OSR2_BUILD_NUMBER )
				{
					glw_state.allowdisplaydepthchange = true;
				}
			}
		}
	}
	else
	{
		ST_Printf( PRINT_R_VERBOSE, "GLW_CheckOSVersion() - GetVersionEx failed\n" );
		return false;
	}

	return true;
}

/*
** GLW_LoadOpenGL
**
** GLimp_win.c internal function that attempts to load and use 
** a specific OpenGL DLL.
*/
static qbool GLW_LoadOpenGL( const char *drivername )
{
	char buffer[1024];
	qbool cdsFullscreen;

	strlcpy( buffer, drivername, sizeof(buffer) );
	Q_strlwr(buffer);

	//
	// determine if we're on a standalone driver
	//
	if ( strstr( buffer, "opengl32" ) != 0 || r_maskMinidriver.integer )
	{
		glConfig.driverType = GLDRV_ICD;
	}
	else
	{
		glConfig.driverType = GLDRV_STANDALONE;

		ST_Printf( PRINT_R_VERBOSE, "...assuming '%s' is a standalone driver\n", drivername );

		if ( strstr( buffer, _3DFX_DRIVER_NAME ) )
		{
			glConfig.driverType = GLDRV_VOODOO;
		}
	}

	// disable the 3Dfx splash screen
	_putenv("FX_GLIDE_NO_SPLASH=0");

	//
	// load the driver and bind our function pointers to it
	// 
	if ( QGL_Init( buffer ) ) 
	{
		cdsFullscreen = r_fullscreen.integer;

		// create the window and set up the context
		if ( !GLW_StartDriverAndSetMode( drivername, r_mode.integer, r_colorbits.integer, cdsFullscreen ) )
		{
			// if we're on a 24/32-bit desktop and we're going fullscreen on an ICD,
			// try it again but with a 16-bit desktop
			if ( glConfig.driverType == GLDRV_ICD )
			{
				if ( r_colorbits.integer != 16 ||
					 cdsFullscreen != true ||
					 r_mode.integer != 3 )
				{
					if ( !GLW_StartDriverAndSetMode( drivername, 3, 16, true ) )
					{
						goto fail;
					}
				}
			}
			else
			{
				goto fail;
			}
		}

		if ( glConfig.driverType == GLDRV_VOODOO )
		{
			glConfig.isFullscreen = true;
		}

		return true;
	}
fail:

	QGL_Shutdown();

	return false;
}


void GL_BeginRendering (int *x, int *y, int *width, int *height) {
	*x = *y = 0;
	*width  = glConfig.vidWidth;
	*height = glConfig.vidHeight;

	if (cls.state != ca_active)
		glClear (GL_COLOR_BUFFER_BIT);
}

extern void CheckWindowedMouse(void);	
// FIXME: merge with GLimp_EndFrame() !!!!!!
void GL_EndRendering (void) {
	//
	// swapinterval stuff
	//
	if ( r_swapInterval.modified ) {
		r_swapInterval.modified = false;

		if ( !glConfig.stereoEnabled ) {	// why?
			if ( qwglSwapIntervalEXT ) {
				qwglSwapIntervalEXT( r_swapInterval.value != 0);
				vid_vsync_on = (r_swapInterval.value != 0);
			}
		}
	}

	WG_CheckNeedSetDeviceGammaRamp();

	if (!scr_skipupdate || block_drawing) {

		// Multiview - Only swap the back buffer to front when all views have been drawn in multiview.
		if (cl_multiview.value && cls.mvdplayback) 
		{
			if (CURRVIEW == 1)
			{
				GLimp_EndFrame();
			}
		}
		else 
		{
			// Normal, swap on each frame.
			GLimp_EndFrame(); 
		}
	}

	CheckWindowedMouse();
}

/*
** GLimp_EndFrame
*/
void GLimp_EndFrame (void)
{
	double time_before_swap;
/* move it to GL_EndRendering() temporaly
	//
	// swapinterval stuff
	//
	if ( r_swapInterval.modified ) {
		r_swapInterval.modified = false;

		if ( !glConfig.stereoEnabled ) {	// why?
			if ( qwglSwapIntervalEXT ) {
				qwglSwapIntervalEXT( r_swapInterval.integer );
			}
		}
	}
*/

	time_before_swap = Sys_DoubleTime();
	if ( glConfig.driverType > GLDRV_ICD )
	{
		if ( !qwglSwapBuffers( glw_state.hDC ) )
		{
			ST_Printf( PRINT_ERR_FATAL, "GLimp_EndFrame() - SwapBuffers() failed!\n" );
		}
	}
	else
	{
		SwapBuffers( glw_state.hDC );
	}
	vid_last_swap_time = Sys_DoubleTime();
	vid_vsync_lag = vid_last_swap_time - time_before_swap;

	// check logging
//	QGL_EnableLogging( r_logFile.integer );
}

static void GLW_StartError( void )
{
	ST_Printf( PRINT_ERR_FATAL, 
		"Could not load OpenGL subsystem\n"
		"\n"
		"You need to have proper graphics card drivers installed\n"
		"\n"
		"[GLW_StartOpenGL()]"
	);
}

static void GLW_StartOpenGL( void )
{
	qbool attemptedOpenGL32 = false;
	qbool attempted3Dfx = false;

	attempted3Dfx = true; // FIXME: qqshka, we are not yet really loading standalone lib

	//
	// load and initialize the specific OpenGL driver
	//
	if ( !GLW_LoadOpenGL( r_glDriver.string ) )
	{
		if ( !strcasecmp( r_glDriver.string, OPENGL_DRIVER_NAME ) )
		{
			attemptedOpenGL32 = true;
		}
		else if ( !strcasecmp( r_glDriver.string, _3DFX_DRIVER_NAME ) )
		{
			attempted3Dfx = true;
		}

		if ( !attempted3Dfx )
		{
			attempted3Dfx = true;
			if ( GLW_LoadOpenGL( _3DFX_DRIVER_NAME ) )
			{
				Cvar_Set( &r_glDriver, _3DFX_DRIVER_NAME );
				r_glDriver.modified = false;
			}
			else
			{
				if ( !attemptedOpenGL32 )
				{
					if ( !GLW_LoadOpenGL( OPENGL_DRIVER_NAME ) )
					{
						GLW_StartError();
					}
					Cvar_Set( &r_glDriver, OPENGL_DRIVER_NAME );
					r_glDriver.modified = false;
				}
				else
				{
					GLW_StartError();
				}
			}
		}
		else if ( !attemptedOpenGL32 )
		{
			attemptedOpenGL32 = true;
			if ( GLW_LoadOpenGL( OPENGL_DRIVER_NAME ) )
			{
				Cvar_Set( &r_glDriver, OPENGL_DRIVER_NAME );
				r_glDriver.modified = false;
			}
			else
			{
				GLW_StartError();
			}
		}
		else
		{
			GLW_StartError();
		}
	}
}

/*
** GLimp_Init
**
** This is the platform specific OpenGL initialization function.  It
** is responsible for loading OpenGL, initializing it, setting
** extensions, creating a window of the appropriate size, doing
** fullscreen manipulations, etc.  Its overall responsibility is
** to make sure that a functional OpenGL subsystem is operating
** when it returns to the ref.
*/
void GLimp_Init( void )
{
	char	buf[1024];
//	cvar_t *lastValidRenderer = Cvar_Get( "vid_lastValidRenderer", "(uninitialized)", CVAR_ARCHIVE );

	ST_Printf( PRINT_R_VERBOSE, "Initializing OpenGL subsystem\n" );

	Cvar_SetCurrentGroup(CVAR_GROUP_VIDEO);

	Cvar_Register (&r_allowSoftwareGL);
	Cvar_Register (&r_maskMinidriver);
	Cvar_Register (&vid_forcerestoregamma);

	Cvar_ResetCurrentGroup();

	//
	// check OS version to see if we can do fullscreen display changes
	//
	if ( !GLW_CheckOSVersion() )
	{
		ST_Printf( PRINT_ERR_FATAL, "GLimp_Init() - incorrect operating system\n" );
	}

	// load appropriate DLL and initialize subsystem
	GLW_StartOpenGL();

	// get our config strings
	strlcpy( glConfig.vendor_string, qglGetString (GL_VENDOR), sizeof( glConfig.vendor_string ) );
	strlcpy( glConfig.renderer_string, qglGetString (GL_RENDERER), sizeof( glConfig.renderer_string ) );
	strlcpy( glConfig.version_string, qglGetString (GL_VERSION), sizeof( glConfig.version_string ) );
	strlcpy( glConfig.extensions_string, qglGetString (GL_EXTENSIONS), sizeof( glConfig.extensions_string ) );

	//
	// chipset specific configuration
	//
	strlcpy( buf, glConfig.renderer_string, sizeof(buf) );
	Q_strlwr( buf );

	//
	// NOTE: if changing cvars, do it within this block.  This allows them
	// to be overridden when testing driver fixes, etc. but only sets
	// them to their default state when the hardware is first installed/run.
	//
#if 0 /* qqshka: sad, that good for q3, but not for ezquake cfg managment */
	if ( Q_stricmp( lastValidRenderer->string, glConfig.renderer_string ) )
	{
		glConfig.hardwareType = GLHW_GENERIC;

		Cvar_Set( &r_textureMode, "GL_LINEAR_MIPMAP_NEAREST" );

		// VOODOO GRAPHICS w/ 2MB
		if ( strstr( buf, "voodoo graphics/1 tmu/2 mb" ) )
		{
			Cvar_Set( &r_picmip, "2" );
		}
		else
		{
			Cvar_Set( &r_picmip, "1" );

			if ( strstr( buf, "rage 128" ) || strstr( buf, "rage128" ) )
			{
				Cvar_Set( &r_finish, "0" );
			}
			// Savage3D and Savage4 should always have trilinear enabled
			else if ( strstr( buf, "savage3d" ) || strstr( buf, "s3 savage4" ) )
			{
				Cvar_Set( &r_texturemode, "GL_LINEAR_MIPMAP_LINEAR" );
			}
		}
	}
#endif
	
	//
	// this is where hardware specific workarounds that should be
	// detected/initialized every startup should go.
	//
	if ( strstr( buf, "banshee" ) || strstr( buf, "voodoo3" ) )
	{
		glConfig.hardwareType = GLHW_3DFX_2D3D;
	}
	else if ( strstr( buf, "intel" ) )
	{
		glConfig.hardwareType = GLHW_INTEL;
	}
	// VOODOO GRAPHICS w/ 2MB
	else if ( strstr( buf, "voodoo graphics/1 tmu/2 mb" ) )
	{
	}
	else if ( strstr( buf, "glzicd" ) )
	{
	}
	else if ( strstr( buf, "rage pro" ) || strstr( buf, "Rage Pro" ) || strstr( buf, "ragepro" ) )
	{
		glConfig.hardwareType = GLHW_RAGEPRO;
	}
	else if ( strstr( buf, "rage 128" ) )
	{
	}
	else if ( strstr( buf, "permedia2" ) )
	{
		glConfig.hardwareType = GLHW_PERMEDIA2;
	}
	else if ( strstr( buf, "riva 128" ) )
	{
		glConfig.hardwareType = GLHW_RIVA128;
	}
	else if ( strstr( buf, "riva tnt " ) )
	{
	}

//	Cvar_Set( "r_lastValidRenderer", glConfig.renderer_string );

	GLW_InitExtensions();
	WG_CheckHardwareGamma();

// FIXME: if we turn off ztrick due to lack of bits in one mode, we do not turn it on if we got enought bits in other mode
	gl_allow_ztrick = true;

	if ( glConfig.depthBits < 24 )
	{
		extern cvar_t	gl_ztrick;

		if (gl_ztrick.value)
			ST_Printf( PRINT_WARNING, "%s will be forced to 0", gl_ztrick.name);

		gl_allow_ztrick = false;
	}
}

/*
** GLimp_Shutdown
**
** This routine does all OS specific shutdown procedures for the OpenGL
** subsystem.
*/
void GLimp_Shutdown( void )
{
//	const char *strings[] = { "soft", "hard" };
	const char *success[] = { "failed", "success" };
	int retVal;

	// FIXME: Brian, we need better fallbacks from partially initialized failures
	if ( !qwglMakeCurrent ) {
		return;
	}

	ST_Printf( PRINT_R_VERBOSE, "Shutting down OpenGL subsystem\n" );

	// restore gamma.  We do this first because 3Dfx's extension needs a valid OGL subsystem
	WG_RestoreGamma();

	// set current context to NULL
	if ( qwglMakeCurrent )
	{
		retVal = qwglMakeCurrent( NULL, NULL ) != 0;

		ST_Printf( PRINT_R_VERBOSE, "...wglMakeCurrent( NULL, NULL ): %s\n", success[retVal] );
	}

	// delete HGLRC
	if ( glw_state.hGLRC )
	{
		retVal = qwglDeleteContext( glw_state.hGLRC ) != 0;
		ST_Printf( PRINT_R_VERBOSE, "...deleting GL context: %s\n", success[retVal] );
		glw_state.hGLRC = NULL;
	}

	// release DC
	if ( glw_state.hDC )
	{
		retVal = ReleaseDC( mainwindow, glw_state.hDC ) != 0;
		ST_Printf( PRINT_R_VERBOSE, "...releasing DC: %s\n", success[retVal] );
		glw_state.hDC   = NULL;
	}

	// destroy window
	if ( mainwindow )
	{
		// Find out the name of the device this window
		// is on (this is for multi-monitor setups).
		// We do this so we can recreate the new window
		// on the same screen later when going to fullscreen.
		prevMonitor = VID_GetCurrentMonitor();
		prevMonInfo = VID_GetCurrentMonitorInfo(prevMonitor);

		ST_Printf( PRINT_R_VERBOSE, "...destroying window\n" );
		ShowWindow( mainwindow, SW_HIDE );
		DestroyWindow( mainwindow );
		mainwindow = NULL;
		glw_state.pixelFormatSet = false;
	}

	// close the r_logFile
	if ( glw_state.log_fp )
	{
		fclose( glw_state.log_fp );
		glw_state.log_fp = 0;
	}

	// reset display settings
	if ( glw_state.cdsFullscreen )
	{
		ST_Printf( PRINT_R_VERBOSE, "...resetting display\n" );
		ChangeDisplaySettings( 0, 0 );
		glw_state.cdsFullscreen = false;
	}

	// alt + tab
	memset( &(glw_state.dm), 0, sizeof( glw_state.dm ) );
	glw_state.vid_canalttab    = false;
	glw_state.vid_wassuspended = false;

	// shutdown QGL subsystem
	QGL_Shutdown();

	memset( &glConfig, 0, sizeof( glConfig ) );
}

/*
** GLimp_LogComment
*/
void GLimp_LogComment( char *comment ) 
{
	if ( glw_state.log_fp ) {
		fprintf( glw_state.log_fp, "%s", comment );
	}
}

/******************************************************************************/
//
// OK, BELOW STUFF FROM Q1
//
/******************************************************************************/

/*************************** DISPLAY FREQUENCY ****************************/

void VID_ShowFreq_f(void) {
	int freq, cnt = 0;
	DEVMODE	testMode;

	if ( !host_initialized || !glw_state.hDC )
		return;

	memset((void*) &testMode, 0, sizeof(testMode));
	testMode.dmSize = sizeof(testMode);

	Com_Printf("Possible display frequency:");

	testMode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;

	//
	// check our attributes
	//
	testMode.dmBitsPerPel = GetDeviceCaps( glw_state.hDC, BITSPIXEL );
	testMode.dmPelsWidth  = GetDeviceCaps( glw_state.hDC, HORZRES );
	testMode.dmPelsHeight = GetDeviceCaps( glw_state.hDC, VERTRES );

	for ( freq = 1; freq < 301; freq++ )
	{
		testMode.dmDisplayFrequency = freq;
		if ( ChangeDisplaySettings (&testMode, CDS_FULLSCREEN | CDS_TEST ) != DISP_CHANGE_SUCCESSFUL )
			continue; // mode can't be set

		Com_Printf(" %d", freq);
		cnt++;
	}

	Com_Printf("%s\n", cnt ? "" : " none");
}

/******************************** WINDOW STUFF ********************************/

int				window_center_x, window_center_y;
RECT			window_rect;

void VID_SetCaption (char *text) 
{
	if (mainwindow)
	{
		SetWindowText(mainwindow, text);
		UpdateWindow(mainwindow);
	}
}

// *sigh* that more input code than video... in q3 it more clear...
void VID_UpdateWindowStatus (void) 
{
	RECT			monitor_rect;
	HMONITOR		hCurrMon;
	MONITORINFOEX	currMonInfo;

	// Get the current monitor and info about it.
	hCurrMon = VID_GetCurrentMonitor();
	currMonInfo = VID_GetCurrentMonitorInfo(hCurrMon);

	monitor_rect = currMonInfo.rcMonitor;
	GetWindowRect( mainwindow, &window_rect );

	// If the window is partially offscreen, we only want to center 
	// the mouse within the part that is actually on screen, otherwise
	// we might center the mouse outside the window and loose focus
	// as the user clicks...
	if (window_rect.left < monitor_rect.left)
		window_rect.left = monitor_rect.left;
	if (window_rect.top < monitor_rect.top)
		window_rect.top = monitor_rect.top;
	if (window_rect.right >= monitor_rect.right)
		window_rect.right = monitor_rect.right - 1;
	if (window_rect.bottom >= monitor_rect.bottom)
		window_rect.bottom = monitor_rect.bottom - 1;

	window_center_x = (window_rect.right + window_rect.left) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	IN_UpdateClipCursor ();
}

void VID_NotifyActivity(void) 
{
    if (!ActiveApp && vid_flashonactivity.value)
        FlashWindow(mainwindow, TRUE);
}

// handle the mouse state when windowed if that's changed
void CheckWindowedMouse(void) 
{
	extern qbool	mouseactive;  // from in_win.c

	static int windowed_mouse;

	if ( glConfig.isFullscreen ) // well, activate mouse in fullscreen mode too
	{
		if (!mouseactive && ActiveApp)
		{
			IN_ActivateMouse ();
			IN_HideMouse ();
		}

		return;
	}

	if ( !_windowed_mouse.value )
	{
		if ( windowed_mouse )
		{
			windowed_mouse = false;

			IN_DeactivateMouse();
			IN_ShowMouse();
		}
	}
	else
	{
		windowed_mouse = true;

		if ((key_dest == key_game || key_dest == key_hudeditor || key_dest == key_menu || key_dest == key_demo_controls) && !mouseactive && ActiveApp)
		{
			IN_ActivateMouse();
			IN_HideMouse();
		}
		else if (mouseactive && (key_dest != key_game && key_dest != key_hudeditor && key_dest != key_demo_controls && key_dest != key_menu))
		{
			IN_DeactivateMouse();
			IN_ShowMouse();
		}
	}
}

/******************************  WG_AppActivate  **********************************/

void WG_AppActivate(BOOL fActive, BOOL minimized) 
{
	if ( fActive )
	{
		if ( glConfig.isFullscreen )
		{
			if ( glw_state.vid_canalttab && glw_state.vid_wassuspended )
			{
				glw_state.vid_wassuspended = false;

				if (ChangeDisplaySettings (&(glw_state.dm), CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
					Sys_Error ("WG_AppActivate: ChangeDisplaySettings failed");

				#ifdef NDEBUG // Some alt+tab work around, bring on top of Z order, debug configuration does't have WS_EX_TOPMOST flag so ...
				SetWindowPos(mainwindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
				#endif
				ShowWindow(mainwindow, SW_SHOWNORMAL);
				
				// Fix for alt-tab bug in NVidia drivers.
				// Make sure we get the proper info for multiple monitor support
				// and position the window correctly after the window is created.
				{
					// We need to get the new monitor information since the resolution might have changed.
					MONITORINFOEX monInfo = VID_GetCurrentMonitorInfo(prevMonitor);
					MoveWindow(mainwindow, monInfo.rcMonitor.left, monInfo.rcMonitor.top, 
							glw_state.dm.dmPelsWidth, glw_state.dm.dmPelsHeight, false);
				}

				Sbar_Changed();
			}
		}
		else if ( !glConfig.isFullscreen && minimized )
		{
			ShowWindow (mainwindow, SW_RESTORE);
		}

		if ( glw_state.vid_canalttab && !minimized )
		{
			// VVD: didn't restore gamma after ALT+TAB on some ATI video cards (or drivers?...)
			// HACK!!! FIXME {
			if (restore_gamma == 0 && (int)vid_forcerestoregamma.value)
				restore_gamma = 1;
			// }
			v_gamma.modified = true; // force reset gamma on next frame
		}
	}
	else
	{
		WG_RestoreGamma ();

		if ( glConfig.isFullscreen )
		{
			if ( glw_state.vid_canalttab )
			{ 
				glw_state.vid_wassuspended = true;

				ChangeDisplaySettings( 0, 0 );
				#ifdef NDEBUG /* some alt+tab work around, bring on bottom of Z order, debug configuration does't have WS_EX_TOPMOST flag so ...*/
				SetWindowPos(mainwindow, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
				#endif
				ShowWindow (mainwindow, SW_MINIMIZE);
			}
		}
	}
}

/********************************** HW GAMMA **********************************/

void VID_ShiftPalette (unsigned char *palette) {}

extern cvar_t	vid_hwgammacontrol; // put here, so u remeber this cvar exist

unsigned short *currentgammaramp = NULL;
static unsigned short systemgammaramp[3][256];

qbool vid_gammaworks	  = false;
qbool vid_hwgamma_enabled = false;
qbool old_hwgamma_enabled = false;
qbool customgamma		  = false;

// !!!!!!!!!
// NOTE: somewhere we use glw_state.hDC somewhere hDC = GetDC( GetDesktopWindow() )
// !!!!!!!!!

// Note: ramps must point to a static array
void VID_SetDeviceGammaRamp(unsigned short *ramps) 
{
	if ( !vid_gammaworks )
		return;

	currentgammaramp = ramps; // is this really need before check for vid_hwgamma_enabled ???

	if ( !vid_hwgamma_enabled )
		return;

	customgamma = true;

	if ( Win2K || WinXP || Win2K3 || WinVISTA || Win7)
	{
		int i, j;

		for (i = 0; i < 128; i++)
			for (j = 0; j < 3; j++)
				ramps[j * 256 + i] = min(ramps[j * 256 + i], (i + 0x80) << 8);

		for (j = 0; j < 3; j++)
			ramps[j * 256 + 128] = min(ramps[j * 256 + 128], 0xFE00);
	}

	if ( qwglSetDeviceGammaRamp3DFX )
		qwglSetDeviceGammaRamp3DFX( glw_state.hDC, ramps );
	else {
		SetDeviceGammaRamp( glw_state.hDC, ramps );
	}
}

void WG_CheckHardwareGamma (void) 
{
	HDC			hDC;

	// main
	vid_gammaworks      = false;
	// damn helpers
	vid_hwgamma_enabled = false;
	old_hwgamma_enabled = false;
	customgamma		    = false;
	currentgammaramp    = NULL;

	v_gamma.modified	= true; // force update on next frame

	if ( COM_CheckParm("-nohwgamma") && (!strncasecmp(Rulesets_Ruleset(), "MTFL", 4)) ) // FIXME
		return;

	if ( qwglGetDeviceGammaRamp3DFX )
	{
		hDC = GetDC( GetDesktopWindow() );
		vid_gammaworks = qwglGetDeviceGammaRamp3DFX( hDC, systemgammaramp );
		ReleaseDC( GetDesktopWindow(), hDC );

		return;
	}

	// non-3Dfx standalone drivers don't support gamma changes, period
	if ( glConfig.driverType == GLDRV_STANDALONE )
	{
		return;
	}

	hDC = GetDC( GetDesktopWindow() );
	vid_gammaworks = GetDeviceGammaRamp( hDC, systemgammaramp );
	ReleaseDC( GetDesktopWindow(), hDC );

	if ( vid_gammaworks && !COM_CheckParm("-nogammareset") )
	{
		int i, j;
		for (i = 0; i < 3; i++)
			for (j = 0; j < 256; j++)
				systemgammaramp[i][j] = (j << 8);
	}
}

void WG_RestoreGamma(void) 
{
	if ( vid_gammaworks && customgamma )
	{
		customgamma = false;

		if ( qwglSetDeviceGammaRamp3DFX )
		{
			qwglSetDeviceGammaRamp3DFX( glw_state.hDC, systemgammaramp );
		}
		else
		{
			HDC hDC;
			
			hDC = GetDC( GetDesktopWindow() );
			SetDeviceGammaRamp( hDC, systemgammaramp );
			ReleaseDC( GetDesktopWindow(), hDC );
		}
	}
}

void WG_CheckNeedSetDeviceGammaRamp(void) 
{
	vid_hwgamma_enabled = vid_hwgammacontrol.value && vid_gammaworks && ActiveApp && !Minimized;
	vid_hwgamma_enabled = vid_hwgamma_enabled && (glConfig.isFullscreen || vid_hwgammacontrol.value == 2);

	if ( vid_hwgamma_enabled != old_hwgamma_enabled )
	{
		old_hwgamma_enabled = vid_hwgamma_enabled;
		if ( vid_hwgamma_enabled && currentgammaramp )
			VID_SetDeviceGammaRamp ( currentgammaramp );
		else
			WG_RestoreGamma ();
	}
}

