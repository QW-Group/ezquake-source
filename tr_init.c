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

	$Id: tr_init.c,v 1.24 2007-10-14 17:54:12 himan Exp $

*/
// tr_init.c -- functions that are not called every frame


#include "quakedef.h"
#ifdef GLQUAKE
#include "gl_model.h"
#include "gl_local.h"
#ifdef GLSL
#include "gl_shader.h"
#endif // GLSL
#if defined(_WIN32) || defined(__linux__) || defined(__FreeBSD__)
#include "tr_types.h"
#endif // _WIN32 || __linux__ || __FreeBSD__
#endif

glconfig_t	glConfig;
//glstate_t	glState;

//
// latched variables that can only change over a restart
//

cvar_t	r_glDriver 			= { "vid_glDriver", OPENGL_DRIVER_NAME, CVAR_ARCHIVE | CVAR_LATCH };
cvar_t	r_allowExtensions	= { "vid_allowExtensions",	"1",	CVAR_ARCHIVE | CVAR_LATCH };

//cvar_t r_texturebits		= { "vid_texturebits",		"0",	CVAR_ARCHIVE | CVAR_LATCH };
cvar_t	r_colorbits			= { "vid_colorbits",		"0",	CVAR_ARCHIVE | CVAR_LATCH };
cvar_t	r_stereo			= { "vid_stereo",			"0",	CVAR_ARCHIVE | CVAR_LATCH };
#ifdef __linux__
cvar_t	r_stencilbits		= { "vid_stencilbits",		"0",	CVAR_ARCHIVE | CVAR_LATCH };
#else
cvar_t	r_stencilbits		= { "vid_stencilbits",		"8",	CVAR_ARCHIVE | CVAR_LATCH };
#endif
cvar_t	r_depthbits			= { "vid_depthbits",		"0",	CVAR_ARCHIVE | CVAR_LATCH };
//cvar_t	r_overBrightBits= { "vid_overBrightBits",	"1",	CVAR_ARCHIVE | CVAR_LATCH };
cvar_t	r_mode				= { "vid_mode",				"3",	CVAR_ARCHIVE | CVAR_LATCH };
cvar_t	r_fullscreen		= { "vid_fullscreen",		"1",	CVAR_ARCHIVE | CVAR_LATCH };
cvar_t	r_customwidth		= { "vid_customwidth",		"1600",	CVAR_ARCHIVE | CVAR_LATCH };
cvar_t	r_customheight		= { "vid_customheight",		"1024",	CVAR_ARCHIVE | CVAR_LATCH };
cvar_t	r_customaspect		= { "vid_customaspect",		"1",	CVAR_ARCHIVE | CVAR_LATCH }; // qqshka: unused even in q3, but I keep cvar, just do not register it
cvar_t	r_displayRefresh	= { "vid_displayfrequency", "0",	CVAR_ARCHIVE | CVAR_LATCH };
cvar_t	vid_borderless		= { "vid_borderless",		"0",	CVAR_ARCHIVE | CVAR_LATCH };
//cvar_t	r_intensity		= { "vid_intensity",		"1",	CVAR_LATCH };

//
// archived variables that can change at any time
//
cvar_t	r_ignoreGLErrors	= { "vid_ignoreGLErrors",	"1",	CVAR_ARCHIVE | CVAR_SILENT };
//cvar_t	r_textureMode	= { "vid_textureMode",	"GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE };
cvar_t	r_swapInterval		= { "vid_vsync",			"0",	CVAR_ARCHIVE | CVAR_SILENT };
#ifdef __MACOS__
//cvar_t	r_gamma			= { "vid_gamma",			"1.2",	CVAR_ARCHIVE | CVAR_SILENT };
#else
//cvar_t	r_gamma			= { "vid_gamma",			"1",	CVAR_ARCHIVE | CVAR_SILENT };
#endif

void OnChange_vid_pos(cvar_t *var, char *string, qbool *cancel);
cvar_t	vid_xpos			= { "vid_xpos",				"3",	CVAR_ARCHIVE | CVAR_SILENT, OnChange_vid_pos};
cvar_t	vid_ypos			= { "vid_ypos",				"22",	CVAR_ARCHIVE | CVAR_SILENT, OnChange_vid_pos};
cvar_t	vid_minpos  = { "vid_minpos",	"0",	CVAR_ARCHIVE | CVAR_SILENT};

void OnChange_r_con_xxx (cvar_t *var, char *string, qbool *cancel);
cvar_t	r_conwidth			= { "vid_conwidth",			"640",	CVAR_NO_RESET | CVAR_SILENT, OnChange_r_con_xxx };
cvar_t	r_conheight			= { "vid_conheight",		"0",	CVAR_NO_RESET | CVAR_SILENT, OnChange_r_con_xxx }; // default is 0, so i can sort out is user specify conheight on cmd line or something

cvar_t	vid_ref				= { "vid_ref",				"gl",	CVAR_ROM | CVAR_SILENT };
cvar_t  vid_hwgammacontrol	= { "vid_hwgammacontrol", 	"2",    CVAR_ARCHIVE | CVAR_SILENT };
#ifdef _WIN32
cvar_t  vid_flashonactivity = { "vid_flashonactivity",	"1", CVAR_ARCHIVE | CVAR_SILENT };
cvar_t	_windowed_mouse		= { "_windowed_mouse",		"1",	CVAR_ARCHIVE | CVAR_SILENT }; // actually that more like input, but input registered after video in windows
#endif

cvar_t	r_verbose			= { "vid_verbose",			"0",	CVAR_SILENT };
//cvar_t	r_logFile		= { "vid_logFile",			"0",	CVAR_CHEAT | CVAR_SILENT};

// print gl extension in /gfxinfo
cvar_t  r_showextensions	= { "vid_showextensions", 	"0",	CVAR_SILENT };

// aspect ratio for widescreens
void OnChange_vid_wideaspect (cvar_t *var, char *string, qbool *cancel);
cvar_t	vid_wideaspect		= {"vid_wideaspect", "0", CVAR_NO_RESET | CVAR_SILENT, OnChange_vid_wideaspect};

void ( APIENTRY * qglMultiTexCoord2fARB )( GLenum texture, GLfloat s, GLfloat t );
void ( APIENTRY * qglActiveTextureARB )( GLenum texture );
void ( APIENTRY * qglClientActiveTextureARB )( GLenum texture );

static void GfxInfo_f( void );


static void AssertCvarRange( cvar_t *cv, float minVal, float maxVal, qbool shouldBeIntegral )
{
	if ( shouldBeIntegral )
	{
		if ( ( int ) cv->value != cv->integer )
		{
			ST_Printf( PRINT_WARNING, "WARNING: cvar '%s' must be integral (%f)\n", cv->name, cv->value );
			Cvar_SetValue( cv, cv->integer );
		}
	}

	if ( cv->value < minVal )
	{
		ST_Printf( PRINT_WARNING, "WARNING: cvar '%s' out of range (%f < %f)\n", cv->name, cv->value, minVal );
		Cvar_SetValue( cv, minVal );
	}
	else if ( cv->value > maxVal )
	{
		ST_Printf( PRINT_WARNING, "WARNING: cvar '%s' out of range (%f > %f)\n", cv->name, cv->value, maxVal );
		Cvar_SetValue( cv, maxVal );
	}
}


/*
** InitOpenGL
**
** This function is responsible for initializing a valid OpenGL subsystem.  This
** is done by calling GLimp_Init (which gives us a working OGL subsystem) then
** setting variables, checking GL constants, and reporting the gfx system config
** to the user.
*/
static void InitOpenGL( void )
{
	char renderer_buffer[1024];

	//
	// initialize OS specific portions of the renderer
	//
	// GLimp_Init directly or indirectly references the following cvars:
	//		- r_fullscreen
	//		- r_glDriver
	//		- r_mode
	//		- r_(color|depth|stencil)bits
	//		- r_ignorehwgamma
	//		- r_gamma
	//
	
	if ( glConfig.vidWidth == 0 )
	{
		GLint		temp;
		
		GLimp_Init();

		strlcpy( renderer_buffer, glConfig.renderer_string , sizeof (renderer_buffer) );
		Q_strlwr( renderer_buffer );

		// OpenGL driver constants
		qglGetIntegerv( GL_MAX_TEXTURE_SIZE, &temp );
		glConfig.maxTextureSize = temp;

		// stubbed or broken drivers may have reported 0...
		if ( glConfig.maxTextureSize <= 0 ) 
		{
			glConfig.maxTextureSize = 0;
		}
	}

	// print info
	if ( !host_initialized || r_verbose.integer )
		GfxInfo_f();

	// set default state
	GL_SetDefaultState();
}

/*
==================
GL_CheckErrors
==================
*/
void GL_CheckErrors( void ) {
    int		err;
    char	s[64];

    err = qglGetError();
    if ( err == GL_NO_ERROR ) {
        return;
    }
    if ( r_ignoreGLErrors.integer ) {
        return;
    }
    switch( err ) {
        case GL_INVALID_ENUM:
            strlcpy( s, "GL_INVALID_ENUM", sizeof( s ) );
            break;
        case GL_INVALID_VALUE:
            strlcpy( s, "GL_INVALID_VALUE", sizeof( s ) );
            break;
        case GL_INVALID_OPERATION:
            strlcpy( s, "GL_INVALID_OPERATION", sizeof( s ) );
            break;
        case GL_STACK_OVERFLOW:
            strlcpy( s, "GL_STACK_OVERFLOW", sizeof( s ) );
            break;
        case GL_STACK_UNDERFLOW:
            strlcpy( s, "GL_STACK_UNDERFLOW", sizeof( s ) );
            break;
        case GL_OUT_OF_MEMORY:
            strlcpy( s, "GL_OUT_OF_MEMORY", sizeof( s ) );
            break;
        default:
            snprintf( s, sizeof(s), "%i", err);
            break;
    }

    ST_Printf( PRINT_ERR_FATAL, "GL_CheckErrors: %s", s );
}


/*
** R_GetModeInfo
*/
typedef struct vidmode_s
{
    const char *description;
    int         width, height;
	float		pixelAspect;		// pixel width / height
} vidmode_t;

vidmode_t r_vidModes[] =
{
    { "Mode  0: 320x240",		320,	240,	1 },
    { "Mode  1: 400x300",		400,	300,	1 },
    { "Mode  2: 512x384",		512,	384,	1 },
    { "Mode  3: 640x480",		640,	480,	1 },
    { "Mode  4: 800x600",		800,	600,	1 },
    { "Mode  5: 960x720",		960,	720,	1 },
    { "Mode  6: 1024x768",		1024,	768,	1 },
    { "Mode  7: 1152x864",		1152,	864,	1 },
    { "Mode  8: 1280x1024",		1280,	1024,	1 },
    { "Mode  9: 1600x1200",		1600,	1200,	1 },
    { "Mode 10: 2048x1536",		2048,	1536,	1 },
    { "Mode 11: 1920x1200",		1920,	1200,	1 },
    { "Mode 12: 1920x1080",		1920,	1080,	1 },
    { "Mode 13: 1680x1050",		1680,	1050,	1 },
    { "Mode 14: 1440x900",		1440,	900,	1 },
    { "Mode 15: 1280x800",		1280,	800,	1 }
	//  { "Mode 11: 856x480 (wide)",856,	480,	1 }
};
static int	s_numVidModes = ( sizeof( r_vidModes ) / sizeof( r_vidModes[0] ) );

qbool R_GetModeInfo( int *width, int *height, float *windowAspect, int mode ) {
	vidmode_t	*vm;

    if ( mode < -1 ) {
        return false;
	}
	if ( mode >= s_numVidModes ) {
		return false;
	}

	if ( mode == -1 ) {
		*width = r_customwidth.integer;
		*height = r_customheight.integer;
		*windowAspect = r_customaspect.value;
		return true;
	}

	vm = &r_vidModes[mode];

    *width  = vm->width;
    *height = vm->height;
    *windowAspect = (float)vm->width / ( vm->height * vm->pixelAspect );

    return true;
}

// search mode which match params, in r_vidModes[]
// return -1 if not found and from 0 to s_numVidModes if found
int R_MatchMode( int width, int height )
{
	int i;

	for ( i = 0; i < s_numVidModes && (width || height); i++ )
		if ( (!width || width == r_vidModes[i].width) && (!height || height == r_vidModes[i].height) )
			return i; // found

	return -1; // not found
}

/*
** R_ModeList_f
*/
static void R_ModeList_f( void )
{
	int i;

	ST_Printf( PRINT_ALL, "\n" );
	for ( i = 0; i < s_numVidModes; i++ )
	{
		ST_Printf( PRINT_ALL, "%s\n", r_vidModes[i].description );
	}
	ST_Printf( PRINT_ALL, "\n" );
}

//============================================================================

int nonwideconheight = 0;  // Store original conheight if vid_wideaspect is used

void OnChange_r_con_xxx (cvar_t *var, char *string, qbool *cancel) {
	
	float scale = 1;

	if (var == &r_conwidth) {
		int width = Q_atoi(string);

		width = max(320, width);
		//width &= 0xfff8; // make it a multiple of eight
		
		if (glConfig.vidWidth)
			vid.width = vid.conwidth = width = min(glConfig.vidWidth, width);
		else
			vid.conwidth = width; // issued from cmd line ? then do not set vid.width because code may relay what it 0

		Cvar_SetValue(var, (float)width);
	}
	else if (var == &r_conheight)
	{
		int height = Q_atoi(string);

		if (nonwideconheight==0)
			nonwideconheight=height; // save original conheight

		if (host_everything_loaded)
		{
			if (vid_wideaspect.integer)
			{
				nonwideconheight=height;
				scale = (4.0/3) / (16.0/10); //widescreen				
			}
			else
			{
				scale = 1;
			}

			height = floor(height * scale + 0.5);
			//height &= 0xfff8; // make it a multiple of eight

			if (vid_wideaspect.value != 0)
				Com_Printf("vid_wideaspect enabled - conheight recalculated to %i\n", height);
		}
		else if (host_initialized && vid_wideaspect.integer)
		{
				scale = (16.0/10) / (4.0/3); // standard
				nonwideconheight = floor( height * scale + 0.5);
		}

		height = max(200, height);

		if (glConfig.vidHeight)
			vid.height = vid.conheight = height = min(glConfig.vidHeight, height);
		else
			vid.conheight = height; // issued from cmd line ? then do not set vid.height because code may relay what it 0

		Cvar_SetValue(var, (float)height);
	}
	else
	{
		*cancel = true; // hrm?
		return;
	}

	Draw_AdjustConback ();
	vid.recalc_refdef = 1;
	*cancel = true;
}

//============================================================================

/*
** GL_SetDefaultState
*/
void GL_SetDefaultState( void )
{

}


/*
================
GfxInfo_f
================
*/
void GfxInfo_f( void ) 
{
//	cvar_t *sys_cpustring = Cvar_Get( "sys_cpustring", "", 0 );
#if 0
	const char *enablestrings[] =
	{
		"disabled",
		"enabled"
	};
#endif
	const char *fsstrings[] =
	{
		"windowed",
		"fullscreen"
	};

	ST_Printf( PRINT_ALL, "\nGL_VENDOR: %s\n", glConfig.vendor_string );
	ST_Printf( PRINT_ALL, "GL_RENDERER: %s\n", glConfig.renderer_string );
	ST_Printf( PRINT_ALL, "GL_VERSION: %s\n", glConfig.version_string );
	if ( r_showextensions.value )
		ST_Printf( PRINT_ALL, "GL_EXTENSIONS: %s\n", glConfig.extensions_string );
//	ST_Printf( PRINT_ALL, "GL_MAX_TEXTURE_SIZE: %d\n", glConfig.maxTextureSize );
//	ST_Printf( PRINT_ALL, "GL_MAX_ACTIVE_TEXTURES_ARB: %d\n", glConfig.maxActiveTextures );
	ST_Printf( PRINT_ALL, "\nPIXELFORMAT: color(%d-bits) Z(%d-bit) stencil(%d-bits)\n", glConfig.colorBits, glConfig.depthBits, glConfig.stencilBits );
	ST_Printf( PRINT_ALL, "MODE: %d, %d x %d %s hz:", r_mode.integer, glConfig.vidWidth, glConfig.vidHeight, fsstrings[r_fullscreen.integer == 1] );
	if ( glConfig.displayFrequency )
	{
		ST_Printf( PRINT_ALL, "%d\n", glConfig.displayFrequency );
	}
	else
	{
		ST_Printf( PRINT_ALL, "N/A\n" );
	}
	ST_Printf( PRINT_ALL, "CONRES: %d x %d\n", r_conwidth.integer, r_conheight.integer );

#if 0
	if ( glConfig.deviceSupportsGamma )
	{
		ST_Printf( PRINT_ALL, "GAMMA: hardware w/ %d overbright bits\n", tr.overbrightBits );
	}
	else
	{
		ST_Printf( PRINT_ALL, "GAMMA: software w/ %d overbright bits\n", tr.overbrightBits );
	}
	ST_Printf( PRINT_ALL, "CPU: %s\n", sys_cpustring.string );

	ST_Printf( PRINT_ALL, "texturemode: %s\n", r_textureMode.string );
	ST_Printf( PRINT_ALL, "picmip: %d\n", r_picmip.integer );
	ST_Printf( PRINT_ALL, "texture bits: %d\n", r_texturebits.integer );
	ST_Printf( PRINT_ALL, "multitexture: %s\n", enablestrings[qglActiveTextureARB != 0] );

	if ( r_vertexLight.integer || glConfig.hardwareType == GLHW_PERMEDIA2 )
	{
		ST_Printf( PRINT_ALL, "HACK: using vertex lightmap approximation\n" );
	}
	if ( glConfig.hardwareType == GLHW_RAGEPRO )
	{
		ST_Printf( PRINT_ALL, "HACK: ragePro approximations\n" );
	}
	if ( glConfig.hardwareType == GLHW_RIVA128 )
	{
		ST_Printf( PRINT_ALL, "HACK: riva128 approximations\n" );
	}
	if ( r_finish.integer ) {
		ST_Printf( PRINT_ALL, "Forcing glFinish\n" );
	}
#endif
}

/*
===============
R_Register
===============
*/
void R_Register( void ) 
{
	int i;
	extern void VID_Restart_f (void);
	Cvar_SetCurrentGroup(CVAR_GROUP_VIDEO);

	//
	// latched and archived variables
	//
	Cvar_Register (&r_glDriver);
	Cvar_Register (&r_allowExtensions);

//	Cvar_Register (&r_texturebits);
	Cvar_Register (&r_colorbits);
//	Cvar_Register (&r_stereo); // qqshka: unused but saved
	Cvar_Register (&r_stencilbits);
	Cvar_Register (&r_depthbits);
//	Cvar_Register (&r_overBrightBits);
	Cvar_Register (&r_mode);
	Cvar_Register (&r_fullscreen);
	Cvar_Register (&r_customwidth);
	Cvar_Register (&r_customheight);
//	Cvar_Register (&r_customaspect); // qqshka: unused even in q3, but I keep cvar, just do not register it

	//
	// temporary latched variables that can only change over a restart
	//
	Cvar_Register (&r_displayRefresh);
	AssertCvarRange( &r_displayRefresh, 0, 300, true ); // useless in most cases thought

	Cvar_Register (&vid_borderless);
//	Cvar_Register (&r_intensity);

	//
	// archived variables that can change at any time
	//
	Cvar_Register (&r_ignoreGLErrors);
//	Cvar_Register (&r_textureMode);
	Cvar_Register (&r_swapInterval);
//	Cvar_Register (&r_gamma);
	Cvar_Register (&vid_hwgammacontrol);

	Cvar_Register (&r_verbose);
//	Cvar_Register (&r_logFile);

	Cvar_Register (&vid_xpos);
	Cvar_Register (&vid_ypos);
	Cvar_Register (&vid_minpos);
	Cvar_Register (&r_conwidth);
	Cvar_Register (&r_conheight);
	Cvar_Register (&vid_wideaspect);

	if ( !host_initialized ) // compatibility with retarded cmd line, and actually this still needed for some other reasons
	{
		int w = 0, h = 0;

		if (COM_CheckParm("-window") || COM_CheckParm("-startwindowed"))
			Cvar_LatchedSetValue(&r_fullscreen, 0);

		if ((i = COM_CheckParm("-freq")) && i + 1 < COM_Argc())
			Cvar_LatchedSetValue(&r_displayRefresh, Q_atoi(COM_Argv(i + 1)));

		if ((i = COM_CheckParm("-bpp")) && i + 1 < COM_Argc())
			Cvar_LatchedSetValue(&r_colorbits, Q_atoi(COM_Argv(i + 1)));

		w = ((i = COM_CheckParm("-width"))  && i + 1 < COM_Argc()) ? Q_atoi(COM_Argv(i + 1)) : 0;
		h = ((i = COM_CheckParm("-height")) && i + 1 < COM_Argc()) ? Q_atoi(COM_Argv(i + 1)) : 0;

		#ifdef _WIN32
		if (!( // no!
			strcmp (r_displayRefresh.defaultvalue, r_displayRefresh.string) || // refresh rate wasnt changed
			strcmp (r_colorbits.defaultvalue, r_colorbits.string ) || // bpp wasnt changed
			strcmp (r_mode.defaultvalue, r_mode.string ) || // bpp wasnt changed
			w || h) // width and height wasnt changed
			) 
		{
			// ok, pseudo current
			int freq = 0;
			DEVMODE dm;

			memset( &dm, 0, sizeof( dm ) );
			dm.dmSize = sizeof( dm );
			if ( EnumDisplaySettings( NULL, ENUM_CURRENT_SETTINGS, &dm ) ) // FIXME: Do we need to care about which device we get this? With several monitors we might...
				freq = dm.dmDisplayFrequency; // get actual frequency

			w = GetSystemMetrics (SM_CXSCREEN);
			h = GetSystemMetrics (SM_CYSCREEN);
			Cvar_LatchedSetValue(&r_displayRefresh, freq); // current mean current
			Cvar_LatchedSetValue(&r_colorbits, 0); // use desktop bpp
		}
		#endif // _WIN32

		if ( w || h ) 
		{
			int m = R_MatchMode( w, h );

			if (m == -1) { // ok, mode not found, trying custom
				w = w ? w : h * 4 / 3; // guessing width from height may cause some problems thought, because 4/3 uneven
				h = h ? h : w * 3 / 4;
				Cvar_LatchedSetValue(&r_customwidth,  w);
				Cvar_LatchedSetValue(&r_customheight, h);
			}

			Cvar_LatchedSetValue(&r_mode, m);
		}

		if ((i = COM_CheckParm("-conwidth")) && i + 1 < COM_Argc())
			Cvar_SetValue(&r_conwidth, (float)Q_atoi(COM_Argv(i + 1)));
		else // this is ether +set vid_con... or just default value which we select in cvar initialization
			Cvar_SetValue(&r_conwidth, r_conwidth.value); // must trigger callback which validate value
    
		if ((i = COM_CheckParm("-conheight")) && i + 1 < COM_Argc())
			Cvar_SetValue(&r_conheight, (float)Q_atoi(COM_Argv(i + 1)));
		else // this is ether +set vid_con... or just default value which we select in cvar initialization
			 // also select r_conheight with proper aspect ratio if user omit it
			Cvar_SetValue(&r_conheight, r_conheight.value ? r_conheight.value : r_conwidth.value * 3 / 4); // must trigger callback which validate value
	}

	Cvar_Register (&vid_ref);
#ifdef _WIN32
	Cvar_Register (&vid_flashonactivity);
	Cvar_Register (&_windowed_mouse); //that more like an input, but i have serious reason to register it here
#endif

	Cvar_Register (&r_showextensions);

	Cvar_ResetCurrentGroup();

	if ( !host_initialized )
	{
#ifdef _WIN32
		void VID_ShowFreq_f(void);
		Cmd_AddCommand( "vid_showfreq",	VID_ShowFreq_f );
#endif
		Cmd_AddCommand( "vid_modelist",		R_ModeList_f );
		Cmd_AddCommand( "vid_gfxinfo",		GfxInfo_f );
		Cmd_AddCommand( "vid_restart",	VID_Restart_f );
	}
}

/*
===============
RE_Init
===============
*/
void RE_Init( void ) {	
	int	err;

	ST_Printf( PRINT_R_VERBOSE, "----- R_Init -----\n" );

	R_Register();

	InitOpenGL();

	err = qglGetError();
	if ( err != GL_NO_ERROR )
		ST_Printf ( PRINT_R_VERBOSE, "glGetError() = 0x%x\n", err );

	ST_Printf( PRINT_R_VERBOSE, "----- finished R_Init -----\n" );
}

/*
===============
RE_Shutdown
===============
*/
void RE_Shutdown( qbool destroyWindow ) {

	ST_Printf( PRINT_R_VERBOSE, "R_Shutdown( %i )\n", destroyWindow );

	// shut down platform specific OpenGL stuff
	if ( destroyWindow ) {
		GLimp_Shutdown();
	}
}

/******************************************************************************/
//
// OK, BELOW STUFF FROM Q1
//
/******************************************************************************/

/******************************** VID SHUTDOWN ********************************/

void VID_Shutdown (void) {
#ifdef _WIN32

	extern void AppActivate(BOOL fActive, BOOL minimize);

	AppActivate(false, false);

#endif

#ifdef GLSL
	SHD_Shutdown();
#endif

	RE_Shutdown( true );
}

/********************************** VID INIT **********************************/

#ifdef _WIN32
extern void ClearAllStates (void);
#endif

void VID_zzz (void) {
	extern int nonwideconheight;

	vid.width  = vid.conwidth  = min(vid.conwidth,  glConfig.vidWidth);
	vid.height = vid.conheight = min(vid.conheight, glConfig.vidHeight);

	// we need cap cvars, after resolution changed, here may be conwidth > width, so set cvars right
	Cvar_SetValue(&r_conwidth,  r_conwidth.value);  // must trigger callback which validate value
	Cvar_SetValue(&r_conheight, nonwideconheight); // must trigger callback which validate value

	vid.numpages = 2;

	Draw_AdjustConback ();

#ifdef _WIN32
	//fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates ();
#endif

	vid.recalc_refdef = 1;
}

void VID_Init (unsigned char *palette) {

	vid.colormap = host_colormap;

	Check_Gamma(palette);
	VID_SetPalette(palette);

	RE_Init();

#ifdef GLSL
	SHD_Init();
#endif

	VID_zzz();

	GL_Init();
}

void VID_Restart_f (void)
{
	extern void GFX_Init(void);
	extern void ReloadPaletteAndColormap(void);
	extern int nonwideconheight;
	qbool old_con_suppress;

	if (!host_initialized) { // sanity
		Com_Printf("Can't do %s yet\n", Cmd_Argv(0));
		return;
	}

	VID_Shutdown ();

	ReloadPaletteAndColormap();

	VID_Init (host_basepal);

	// force models to reload (just flush, no actual loading code here)
	Cache_Flush();

	// shut up warnings during GFX_Init();
	old_con_suppress = con_suppress;
	con_suppress = (developer.value ? false : true);
	// reload 2D textures, particles textures, some other textures and gfx.wad
	GFX_Init();

	// reload skins
	Skin_Skins_f();

	con_suppress = old_con_suppress;

	// we need done something like for map reloading, for example reload textures for brush models
	R_NewMap(true);

	// force all cached models to be loaded, so no short HDD lag then u walk over level and discover new model
	Mod_TouchModels();

	// window may be re-created, so caption need to be forced to update
	CL_UpdateCaption(true);

}

void OnChange_vid_wideaspect (cvar_t *var, char *string, qbool *cancel) 
{
	extern float nonwidefov;
	extern int nonwideconheight;
	extern cvar_t scr_fov, r_conheight;

	if ( (Q_atoi(string) == vid_wideaspect.value) || (Q_atoi(string) > 1)  || (Q_atoi(string) < 0))
	{
		*cancel = true;
		return;
	}

	Cvar_Set (&vid_wideaspect, string);

	if(!host_everything_loaded)
		return;

	if (nonwidefov != 0 && nonwideconheight != 0)
	{
		if (vid_wideaspect.integer == 0)
		{
			scr_fov.OnChange(&scr_fov, Q_ftos(nonwidefov), cancel);
			r_conheight.OnChange(&r_conheight, Q_ftos(nonwideconheight), cancel);

		}
		else
		{
			scr_fov.OnChange(&scr_fov, Q_ftos(scr_fov.value), cancel);
			r_conheight.OnChange(&r_conheight, Q_ftos(r_conheight.value), cancel);
		}
	}
}

void OnChange_vid_pos(cvar_t *var, char *string, qbool *cancel)
{
#ifdef WIN32
	if (!r_fullscreen.integer)
	{
		extern	HWND	mainwindow;
		if (mainwindow)
		{
			SetWindowPos(	mainwindow,
							NULL,
							var == &vid_xpos ? atof(string) : vid_xpos.value,
							var == &vid_ypos ? atof(string) : vid_ypos.value,
							0,
							0,
							SWP_NOSIZE | SWP_NOZORDER);
		}
	}
#endif
}

