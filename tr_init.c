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

	$Id: tr_init.c,v 1.2 2007-02-01 17:39:06 qqshka Exp $

*/
// tr_init.c -- functions that are not called every frame


#include "quakedef.h"


glconfig_t	glConfig;
//glstate_t	glState;

//
// latched variables that can only change over a restart
//

cvar_t	r_glDriver 			= { "r_glDriver", OPENGL_DRIVER_NAME, CVAR_ARCHIVE | CVAR_LATCH };
cvar_t	r_allowExtensions	= { "r_allowExtensions","1",	CVAR_ARCHIVE | CVAR_LATCH };

//cvar_t r_texturebits = { "r_texturebits", "0", CVAR_ARCHIVE | CVAR_LATCH };
cvar_t	r_colorbits			= { "r_colorbits",		"0",	CVAR_ARCHIVE | CVAR_LATCH };
cvar_t	r_stereo			= { "r_stereo",			"0",	CVAR_ARCHIVE | CVAR_LATCH };
#ifdef __linux__
cvar_t	r_stencilbits		= { "r_stencilbits",	"0",	CVAR_ARCHIVE | CVAR_LATCH };
#else
cvar_t	r_stencilbits		= { "r_stencilbits",	"8",	CVAR_ARCHIVE | CVAR_LATCH };
#endif
cvar_t	r_depthbits			= { "r_depthbits",		"0",	CVAR_ARCHIVE | CVAR_LATCH };
//cvar_t	r_overBrightBits	= { "r_overBrightBits", "1", CVAR_ARCHIVE | CVAR_LATCH };
cvar_t	r_mode				= { "r_mode",			"3",	CVAR_ARCHIVE | CVAR_LATCH };
cvar_t	r_fullscreen		= { "r_fullscreen",		"1",	CVAR_ARCHIVE | CVAR_LATCH };
cvar_t	r_customwidth		= { "r_customwidth",	"1600",	CVAR_ARCHIVE | CVAR_LATCH };
cvar_t	r_customheight		= { "r_customheight",	"1024",	CVAR_ARCHIVE | CVAR_LATCH };
cvar_t	r_customaspect		= { "r_customaspect",	"1",	CVAR_ARCHIVE | CVAR_LATCH }; // qqshka: unused even in q3, but I keep cvar, just do not register it
cvar_t	r_displayRefresh	= { "r_displayRefresh", "0",	CVAR_ARCHIVE | CVAR_LATCH };
//cvar_t	r_intensity		= { "r_intensity",		"1",	CVAR_LATCH };

//
// archived variables that can change at any time
//
cvar_t	r_ignoreGLErrors	= { "r_ignoreGLErrors", "1",	CVAR_ARCHIVE };
//cvar_t	r_textureMode	= { "r_textureMode",	"GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE };
cvar_t	r_swapInterval		= { "r_swapInterval",	"0",	CVAR_ARCHIVE };
#ifdef __MACOS__
//cvar_t	r_gamma			= { "r_gamma",			"1.2",	CVAR_ARCHIVE };
#else
//cvar_t	r_gamma			= { "r_gamma",			"1",	CVAR_ARCHIVE };
#endif

cvar_t	vid_xpos			= { "vid_xpos",			"3",	CVAR_ARCHIVE };
cvar_t	vid_ypos			= { "vid_ypos",			"22",	CVAR_ARCHIVE };

qbool OnChange_r_con_xxx (cvar_t *var, char *string);
cvar_t	r_conwidth			= { "r_conwidth",		"640",	CVAR_NO_RESET, OnChange_r_con_xxx };
cvar_t	r_conheight			= { "r_conheight",		"0",	CVAR_NO_RESET, OnChange_r_con_xxx }; // default is 0, so i can sort out is user specify conheight on cmd line or something

cvar_t	vid_ref				= { "vid_ref",			"gl",	CVAR_ROM }; // may be rename to r_ref ???
cvar_t  vid_hwgammacontrol	= { "r_hwgammacontrol", "1",    CVAR_ARCHIVE };
#ifdef _WIN32
cvar_t  vid_flashonactivity = { "vid_flashonactivity", "1", CVAR_ARCHIVE };
cvar_t	_windowed_mouse		= { "_windowed_mouse",	"1",	CVAR_ARCHIVE }; // actually that more like input, but input registered after video in windows
#endif

cvar_t	r_verbose			= { "r_verbose",		"0",	0 };
//cvar_t	r_logFile		= { "r_logFile",		"0",	CVAR_CHEAT };


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

		strcpy( renderer_buffer, glConfig.renderer_string );
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

qbool OnChange_r_con_xxx (cvar_t *var, char *string) {
	if (var == &r_conwidth) {
		int width = Q_atoi(string);

		width = max(320, width);
		width &= 0xfff8; // make it a multiple of eight

		if (glConfig.vidWidth)
			vid.width = vid.conwidth = width = min(glConfig.vidWidth, width);
		else
			vid.conwidth = width; // issued from cmd line ? then do not set vid.width because code may relay what it 0

		Cvar_SetValue(var, (float)width);
	}
	else if (var == &r_conheight) {
		int height = Q_atoi(string);

		height = max(200, height);
//		height &= 0xfff8; // make it a multiple of eight

		if (glConfig.vidHeight)
			vid.height = vid.conheight = height = min(glConfig.vidHeight, height);
		else
			vid.conheight = height; // issued from cmd line ? then do not set vid.height because code may relay what it 0

		Cvar_SetValue(var, (float)height);
	}
	else
		return true; // hrm?

	Draw_AdjustConback ();
	vid.recalc_refdef = 1;
	return true;
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
	const char *enablestrings[] =
	{
		"disabled",
		"enabled"
	};
	const char *fsstrings[] =
	{
		"windowed",
		"fullscreen"
	};

	ST_Printf( PRINT_ALL, "\nGL_VENDOR: %s\n", glConfig.vendor_string );
	ST_Printf( PRINT_ALL, "GL_RENDERER: %s\n", glConfig.renderer_string );
	ST_Printf( PRINT_ALL, "GL_VERSION: %s\n", glConfig.version_string );
//	ST_Printf( PRINT_ALL, "GL_EXTENSIONS: %s\n", glConfig.extensions_string );
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
#ifdef __linux__
	Cvar_Register (&r_stencilbits);
#else
	Cvar_Register (&r_stencilbits);
#endif
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
	AssertCvarRange( &r_displayRefresh, 0, 200, true );
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
	Cvar_Register (&r_conwidth);
	Cvar_Register (&r_conheight);

	if (!host_initialized) // compatibility with retarded cmd line, and actually this still needed for some other reasons
	{
		if ((i = COM_CheckParm("-conwidth")) && i + 1 < com_argc)
			Cvar_SetValue(&r_conwidth, (float)Q_atoi(com_argv[i + 1]));
		else // this is ether +set vid_con... or just default value which we select in cvar initialization
			Cvar_SetValue(&r_conwidth, r_conwidth.value); // must trigger callback which validate value
    
		if ((i = COM_CheckParm("-conheight")) && i + 1 < com_argc)
			Cvar_SetValue(&r_conheight, (float)Q_atoi(com_argv[i + 1]));
		else // this is ether +set vid_con... or just default value which we select in cvar initialization
			 // also select r_conheight with proper aspect ratio if user omit it
			Cvar_SetValue(&r_conheight, r_conheight.value ? r_conheight.value : r_conwidth.value * 3 / 4); // must trigger callback which validate value
	}

	Cvar_Register (&vid_ref);
#ifdef _WIN32
	Cvar_Register (&vid_flashonactivity);
	Cvar_Register (&_windowed_mouse); //that more like an input, but i have serious reason to register it here
#endif

	Cvar_ResetCurrentGroup();

	// make sure all the commands added here are also
	// removed in R_Shutdown
	Cmd_AddCommand( "modelist",		R_ModeList_f );
	Cmd_AddCommand( "gfxinfo",		GfxInfo_f );
	Cmd_AddCommand( "vid_restart",	VID_Restart_f );
}

/*
===============
RE_Init
===============
*/
void RE_Init( void ) {	
	int	err;

	ST_Printf( PRINT_ALL, "----- R_Init -----\n" );

	R_Register();

	InitOpenGL();

	err = qglGetError();
	if ( err != GL_NO_ERROR )
		ST_Printf (PRINT_ALL, "glGetError() = 0x%x\n", err);

	ST_Printf( PRINT_ALL, "----- finished R_Init -----\n" );
}

/*
===============
RE_Shutdown
===============
*/
void RE_Shutdown( qbool destroyWindow ) {

	ST_Printf( PRINT_ALL, "R_Shutdown( %i )\n", destroyWindow );

	// FIXME/TODO
	// Cmd_RemoveCommand ( "gfxinfo" );
	// Cmd_RemoveCommand ( "modelist" );

	// shut down platform specific OpenGL stuff
	if ( destroyWindow ) {
		GLimp_Shutdown();
	}
}
