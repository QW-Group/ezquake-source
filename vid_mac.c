/*
Copyright (C) 1996-1997 Id Software, Inc.

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
// gl_vidmac.c -- MacOS GL vid component
#define __OPENTRANSPORTPROVIDERS__

#include "quakedef.h"
#include "gl_local.h"
#include "keys.h"

#include <DrawSprocket.h>
#include <AGL/agl.h>
#include "mac.h"

#define DEFAULT_WIDTH	800
#define DEFAULT_HEIGHT	600

extern qbool	scr_skipupdate;
qbool		windowed = false;// Startup in a window
qbool		inwindow = false;// Can be changed at runtime

qbool		vid_initialized = false;
unsigned char	vid_curpal[256*3];

// kazik -->
int ctrlDown = 0;
int shiftDown = 0;
int altDown = 0;
// kazik <--

glvert_t glv;

extern cvar_t gl_ztrick;

extern viddef_t	vid; // global video state

void GL_Init (void);

qbool isPermedia = false;
qbool is3dfx = false;
qbool isATI = false;
qbool isRadeon = false;

// pOx - Global GL Extension flags
GLint gl_ext_multitexture_units = 0;
GLfloat gl_ext_anisotropic_max = 0;
qbool gl_ext_packedpixel = false;
qbool gl_ext_texture_env_combine = false;
qbool gl_ext_texture_env_add = false;
qbool gl_ext_compiled_vertex_array = false;

//====================================

// pOx - GL video stuff - don't duplicate software names!
cvar_t	vid_mode = {"vid_mode", "0", false};// Saved in prefs, not config (list generated for each device at runtime)
cvar_t	gl_vid_windowed = {"gl_vid_windowed", "0", false};// Saved in prefs, not config - 1 = in a window
cvar_t	gl_vid_screen = {"gl_vid_screen", "0", false};// Saved in prefs, not config
cvar_t	gl_vid_colorbits = {"gl_vid_colorbits", "32", false};// Saved in prefs, not config
cvar_t	vid_vsync = {"vid_vsync", "0", true};// VBL sync
cvar_t	_windowed_mouse = {"_windowed_mouse","1", CVAR_ARCHIVE};
qbool OnTextureBitsChange (cvar_t *var, char *value)
{
	int tb = Q_atoi(value);

	if (tb != 0 && tb != 16 && tb != 32){
		Com_Printf ("Invalid gl_texturebits value\n");
		return true;
	}
	switch (tb){
		case 16:
			gl_solid_format = GL_RGB5;
			gl_alpha_format = GL_RGBA4;
			break;
		case 32:
			gl_solid_format = GL_RGB8;
			gl_alpha_format = GL_RGBA8;
			break;
		default:
			gl_solid_format = 3;
			gl_alpha_format = 4;
	}

	return false;
}
cvar_t	gl_texturebits	= {"gl_texturebits", "32", 0, OnTextureBitsChange};

cvar_t	vid_hwgammacontrol = {"vid_hwgammacontrol","1", 0}; 

qbool	vid_gammaworks = false;
qbool	vid_hwgamma_enabled;

void RestoreHWGamma (void);

// Globals (see Mac.h for comments)
video_mode_t 	vid_modes[MAX_VIDEODEVICES][MAX_DISPLAYMODES];
video_device_t 	vid_devices[MAX_VIDEODEVICES];
video_profile_t vid_currentprofile, vid_newprofile;

// This bunches up all our screen info into one handy set of pointers
video_ptr_t		gScreen;
// Current screen is:
// gScreen.mode-> width, height, refresh
// gScreen.device-> id, handle
// gScreen.profile-> colorbits, texturebits

int numDevices = 0;// Total devices found (must be at least 1)

DSpContextReference gSprocketContext = NULL;
AGLContext 			gContext = NULL;
Rect 				gWindowRect;
Point 				glWindowPos;		// glWindow's position
WindowRef 			glWindow = NULL;	// Used in windowed mode
WindowRef 			glFullWindow = NULL;	// Fullscreen window for classic
//CGrafPtr 			gGLPort = NULL;		// DSp Front Buffer (OSX), or glFullWindow window port

// on-the-fly resolution switching
qbool 	video_restart = false;// don't re-intialize certain things

CGDirectDisplayID 	vid_CGDevices[MAX_VIDEODEVICES];
CGDisplayCount 		gCGNumDevices;
CFDictionaryRef 	gCGoriginalMode;
CGGammaValue 		gOriginalRedTable[256];
CGGammaValue 		gOriginalGreenTable[256];
CGGammaValue 		gOriginalBlueTable[256];
unsigned short		*currentgammaramp = NULL;

// For testing (by necessity fot ATI cards)
#define	USE_AGL 1
#define USE_DSP 2
int 	fullscreenX = 0;


static RGBColor sRGBWhite = { 0xFFFF, 0xFFFF, 0xFFFF };
static RGBColor sRGBBlack = { 0x00CC, 0x00CC, 0x00CC };

// Global prototypes
void MacSetupScreen (void);
void MacShutdownScreen (void);
GLboolean _aglSetDrawableMonitor (AGLContext inContext, Boolean inWindow);
void IN_HideCursor (void);
void IN_ShowCursor (void);
void Release_Cursor_f (void);
void Capture_Cursor_f (void);

// Local prototypes
static AGLContext setupAGL (void);
static void cleanupAGL (AGLContext ctx);


/*
=================
MacSetupScreen

We have a display in mind by the time we get here. This reserves our DSp context on that display
=================
*/
void MacSetupScreen ()
{
	OSStatus theErr;
	DSpContextAttributes foundAttributes;
	DSpContextAttributes wantedAttributes;

	if (gScreen.profile->colorbits != 32 && gScreen.profile->colorbits != 16)
		Sys_Error("Only 32 and 16 bit modes are supported! (Requested: %d)", gScreen.profile->colorbits);

	if (!gScreen.device->id)
		Sys_Error ("Couldn't get a valid display id.");

	// Setup up our global vendor flags (we can just read the device flags, but Quake expects these)
	is3dfx = (gScreen.device->vendor & MAC_GL_3DFX);
	isATI = (gScreen.device->vendor & MAC_GL_ATI);
	isRadeon = (gScreen.device->vendor & MAC_GL_RADEON);

	if (is3dfx)
	{
		// Force 16bit textures on Voodoo 2/3 unless you want an acid trip...
		if (gScreen.device->vendor & MAC_GL_V2_V3)
		{
		 	if (gScreen.profile->texturebits != 16)
		 	{
			 	gScreen.profile->texturebits = 16;
				Cvar_Set (&gl_texturebits, "16");
			 	gl_solid_format = GL_RGB5;
			 	gl_alpha_format = GL_RGBA4;
			}
		}

		return;// Done - 3Dfx cards always use aglSetFullscreen

		// FIXME! Should set fullscreenX for 3Dfx cards to be neat...
		// Though we don't have to worry about this right now (maybe never)
	}

	// Avoid z fighting bug in Radeon driver
//	if (isRadeon) Cvar_Set (&gl_ztrick, "0");

	// ATI fullscreen on X uses DSp to avoid nasty flicker bug
	// We also use DSp if the card doesn't think it can handle the AGL method, or on LCD's (which seem to crap out)
	// Aparently multi-head systems crap out on AGL_FULLSCREEN pixelformats as well - this sucks...
	fullscreenX = (isATI ||
				   !(gScreen.device->vendor & MAC_GL_FULLSCREEN) ||
				   !gScreen.mode->refresh ||
				   numDevices > 1)
				   ? USE_DSP : USE_AGL;

	// force DSp for now..
	fullscreenX = USE_DSP;

	// Allow fullscreen overides for testing
	if (COM_CheckParm("-useAGL"))
		fullscreenX = USE_AGL;
	else if (COM_CheckParm("-useDSp"))
		fullscreenX = USE_DSP;

	// Save out the current (desktop) mode so we can switch out when windowed or on 'Go to Finder'
	gCGoriginalMode = CGDisplayCurrentMode(vid_CGDevices[gScreen.profile->screen]);

	// Save out the current (desktop) gamma
	// (usually done in VID_FadeOut, but we don't fade when starting in a window)
	if (inwindow && !vid_initialized)
	{
		// FIXME! - Switching to another screen in-game won't save the new screen's gamma 
		// unless we're windowed. Not even sure the multi-screen stuff works in the first place...

		CGTableCount sampleCount;

		CGGetDisplayTransferByTable(vid_CGDevices[gScreen.profile->screen], 256,
			gOriginalRedTable, gOriginalGreenTable, gOriginalBlueTable, &sampleCount);
	}

	// No need for a DSp context if we're using aglSetFullscreen
	if (fullscreenX == USE_AGL) return;

	if (gSprocketContext)
	{
		// We've been here before, clean things up
		DSpContext_Release (gSprocketContext);
		gSprocketContext = NULL;
	}

	// clear out the attributes we'll be passing to DSp
	BlockZero (&wantedAttributes, sizeof (DSpContextAttributes));

	// now fill in the fields we're interested in
	wantedAttributes.displayWidth			= gScreen.mode->width;
	wantedAttributes.displayHeight			= gScreen.mode->height;
	wantedAttributes.frequency			= gScreen.mode->refresh;
	wantedAttributes.colorNeeds			= kDSpColorNeeds_Require;
	wantedAttributes.displayBestDepth		= gScreen.profile->colorbits;
	wantedAttributes.backBufferBestDepth		= gScreen.profile->colorbits;
	wantedAttributes.displayDepthMask		= kDSpDepthMask_All;
	wantedAttributes.backBufferDepthMask		= kDSpDepthMask_All;
	wantedAttributes.pageCount 			= 1;

	theErr = DSpFindBestContextOnDisplayID (&wantedAttributes, &gSprocketContext, gScreen.device->id);
	if (theErr) Sys_Error ("Couldn't find a valid context on current display.");

	// see what we actually found
	theErr = DSpContext_GetAttributes (gSprocketContext, &foundAttributes);
	if (theErr) goto bail;

	// debugging...
	Com_DPrintf ("Found Refresh: %i Mode Refresh: %i\n", (int)foundAttributes.frequency>>16, (int)gScreen.mode->refresh>>16);

	// Print some warnings if stuff got overidden - THESE SHOULD *NEVER* HAPPEN!!
	if (wantedAttributes.frequency != foundAttributes.frequency)
		Com_Printf ("WARNING: Refresh is out of range. Using closest match...\n");

	if (wantedAttributes.displayWidth != foundAttributes.displayWidth)
		Com_Printf ("WARNING: Screen width is out of range. Using closest match...\n");

	if (wantedAttributes.displayHeight != foundAttributes.displayHeight)
		Com_Printf ("WARNING: Screen height is out of range. Using closest match...\n");

	wantedAttributes.frequency 		 = foundAttributes.frequency;
	wantedAttributes.displayWidth 		 = foundAttributes.displayWidth;
	wantedAttributes.displayHeight 		 = foundAttributes.displayHeight;
	wantedAttributes.pageCount		 = 1;					// only the front buffer is needed
	wantedAttributes.contextOptions		 = 0 | kDSpContextOption_DontSyncVBL;	// no page flipping and no VBL sync needed

	// Reserve our context (may not even get used at this point)
	theErr = DSpContext_Reserve (gSprocketContext, &wantedAttributes);
	if (theErr) goto bail;

	return;

bail:
	Sys_Error("Unable to create the display!");
}


/*
=================
MacShutdownScreen
=================
*/
void MacShutdownScreen(void)
{	
	if (!inwindow && fullscreenX == USE_AGL)
	{
		CGDisplaySwitchToMode(vid_CGDevices[gScreen.profile->screen], gCGoriginalMode);
		CGDisplayRelease(vid_CGDevices[gScreen.profile->screen]);
	}

	if (gSprocketContext)
	{
		if (!inwindow)
			DSpContext_SetState (gSprocketContext, kDSpContextState_Inactive);
		
		DSpContext_Release (gSprocketContext);
		gSprocketContext = NULL;
	}

	if (glFullWindow != NULL)
	{
		DisposeWindow (glFullWindow);
		glFullWindow = NULL;
	}

	if (glWindow != NULL)
	{
		DisposeWindow (glWindow);
		glWindow = NULL;
	}
}


void FixATIRect ()
{
	if (isATI && !inwindow && fullscreenX == USE_AGL)
	{
		// FIXME! - WTF? Drivers? Quake? Me?

		// It seems like the ATI driver's swap buffer goes from 0,0 to screenwidth-1,screenheight-1
		// when trying to use a fullscreen drawable. Scissoring the swap rect to the
		// correct dimensions, or just turning off the rect seems to fix this, but the image
		// displays some 'flickering' artifacts...

		// NOTE: AGL_BUFFER_RECT has no effect in fullscreen

		if (COM_CheckParm("-scissor"))
		{
			GLint	rect[4] = {0, 0};

    		rect[2] = gScreen.mode->width;
    		rect[3] = gScreen.mode->height;

			aglSetInteger (gContext, AGL_SWAP_RECT, rect);
			aglEnable (gContext, AGL_SWAP_RECT);

			glScissor (rect[0], rect[1], rect[2], rect[3]);
			glEnable (GL_SCISSOR_TEST);
		}
		else
		{
			aglDisable (gContext, AGL_SWAP_RECT);
			glDisable (GL_SCISSOR_TEST);
		}
	}
}


/*
=================
GL_SwapInterval
=================
*/
GLint vblsave;
void GL_SwapInterval ()
{
	GLint swap;

	swap = vid_vsync.value ? 1 : 0;
	aglSetInteger(gContext, AGL_SWAP_INTERVAL, &swap);

	vblsave = vid_vsync.value;
}

void CheckArrayExtensions (void);
void CheckTextureExtensions (void);
void GL_DescribeRenderer_f (void);

/*
=================
Mac_StartGL
=================
*/
void Mac_StartGL (void)
{
	gContext = setupAGL ();

	if (!gContext) {
		inwindow = true;// Don't bother with fades if we're fatal
		Sys_Error("Couldn't create an OpenGL context.");
	}

	FixATIRect ();

	CheckArrayExtensions ();
	CheckTextureExtensions ();

	// Announce our hardware specs	
	GL_DescribeRenderer_f ();

	GL_Init ();
}


/*
=================
Mac_StopGL
=================
*/
void Mac_StopGL (void)
{
	if (gContext)
	{
		cleanupAGL (gContext);
		gContext = NULL;
	}
}


/*
=================
_aglSetDrawableMonitor

Given an already chosen AGL context and monitor, this routine will take care of the bs
involved with choosing a properly-sized window (on non-fullscreen devices) and it
will activate a fullscreen context if desired for those devices that support that option.
=================
*/
GLboolean _aglSetDrawableMonitor (AGLContext inContext, Boolean inWindow)
{
	GLboolean 	ok;
	OSStatus	theErr;
	Str32		winTitle;

	vid.width = gScreen.mode->width;
	vid.height = gScreen.mode->height;

	inwindow = inWindow;

	// Hide the cursor if fullscreen
	if (!inWindow && !video_restart)
		IN_HideCursor ();

	if (inWindow)
	{
		// glWindowPos is saved in the prefs - FIXME! - make sure this isn't off the screen
		gWindowRect.top = glWindowPos.v;
		gWindowRect.left = glWindowPos.h;
		gWindowRect.right = gWindowRect.left + vid.width;
		gWindowRect.bottom = gWindowRect.top + vid.height;

		// create the window
		sprintf ((char *)winTitle, "%s", kAppName);
		c2pstrcpy (winTitle, winTitle);
		CreateNewWindow (kDocumentWindowClass, kWindowCollapseBoxAttribute, &gWindowRect, &glWindow);
		SetWTitle(glWindow, winTitle);
		if (glWindow == NULL) return 0;

		SetPortWindowPort (glWindow);
		RGBForeColor (&sRGBBlack);
		RGBBackColor (&sRGBWhite);

		// LBO - unfortunately, the following routine requires 8.5 in non-Carbon builds (8.1 in Carbon)
		theErr = SetWindowContentColor (glWindow, &sRGBBlack);
		if (theErr) return 0;

		// Attach OpenGL and show the window
		if (!aglSetDrawable (inContext, GetWindowPort(glWindow)))
			return 0;

		if (!video_restart)
		{
			ShowWindow(glWindow);
			SelectWindow(glWindow);
			BringToFront(glWindow);
		}
	}
	else// Fullscreen
	{
		// Use aglFullScreen for 3Dfx and on X
		if ( ((fullscreenX == USE_AGL) || is3dfx) )
		{
			//
			// FIXME! Should we switch colordepth before going full (ala OSX) for Voodoo4/5 in classic???
			//
			
			ok = aglSetFullScreen (inContext, gScreen.mode->width, gScreen.mode->height, gScreen.mode->refresh>>16, gScreen.profile->screen);
			if (!ok) return 0;
		}
		else
		{
			// Activate the DSp context
			if (DSpContext_SetState (gSprocketContext, kDSpContextState_Active))
				return 0;

			// The not recommended (or OSX) way...
			GrafPtr glPort;
			long deltaV, deltaH;
			Rect portBounds;
			PixMapHandle hPix;
			Rect pixBounds;

			if (DSpContext_GetFrontBuffer (gSprocketContext, &glPort))
				return 0;	

			// there is a problem in Mac OS X GM CoreGraphics that may not size the port pixmap correctly
			// this will check the vertical sizes and offset if required to fix the problem
			// this will not center ports that are smaller then a particular resolution
			hPix = GetPortPixMap (glPort);
			pixBounds = (**hPix).bounds;
			GetPortBounds (glPort, &portBounds);
			deltaV = (portBounds.bottom - portBounds.top) - (pixBounds.bottom - pixBounds.top) +
				(portBounds.bottom - portBounds.top - gScreen.mode->height) / 2;
			deltaH = -(portBounds.right - portBounds.left - gScreen.mode->width) / 2;
			if (deltaV || deltaH)
			{
				GrafPtr pPortSave;
				GetPort (&pPortSave);
				SetPort ((GrafPtr)glPort);
				// set origin to account for CG offset and if requested drawable smaller than screen rez
				SetOrigin (deltaH, deltaV);
				SetPort (pPortSave);
			}

			if (!aglSetDrawable(inContext, glPort))
				return 0;
		}
		if (!video_restart)
			VID_FadeIn ();
	}

	GL_SwapInterval ();// Make sure this gets properly set on restarts

	return 1;
}


/*
=================
getGLVendorInfo

Determine our hardware *before* getting our actual pixel format
=================
*/
static int getGLVendorInfo (GDHandle screen)
{
	GLint          	attrib[] = {AGL_RGBA, AGL_DOUBLEBUFFER, AGL_ACCELERATED, AGL_NO_RECOVERY, AGL_NONE};
	AGLPixelFormat 	fmt;
	AGLContext     	ctx = 0;
	GLboolean      	ok;
	GLint          	result;
	AGLRendererInfo info = 0;
	int		flags = MAC_GL_GENERIC;
	Rect		rect = {50,50,150,150};// make sure it's on the screen

	//
	// Get a generic (accelerated) pixelformat on the specified screen
	//

	fmt = aglChoosePixelFormat (&screen, 1, attrib);
	if (!fmt) goto bail;

	ctx = aglCreateContext (fmt, NULL);
	if(!ctx) goto bail;

	ok = aglSetCurrentContext (ctx);
	if (!ok) goto bail;

	// Seems some systems crap out on glGetXXXX if we're not totally hooked up (we don't actually show this window though)
	CreateNewWindow (kDocumentWindowClass, kWindowCollapseBoxAttribute, &rect, &glWindow);
	if (glWindow == NULL) goto bail;

	ok = aglSetDrawable (ctx, GetWindowPort(glWindow));
	if (!ok) goto bail;

	info = aglQueryRendererInfo (&screen, 1);
	if (!info) goto bail;

	// Let OpenGL tell us what we're running on...

	ok = aglDescribeRenderer (info, AGL_FULLSCREEN, &result);
	if (!ok) goto bail;

	// fullscreen device (dependant on hardware *and* OS)
	if (result) flags |= MAC_GL_FULLSCREEN;

	// ATI (any)
	if (strstr((char*)glGetString(GL_VENDOR), "ATI Technologies"))
		flags |= MAC_GL_ATI;

	// ATI (Radeon class)
	if (strstr((char*)glGetString(GL_RENDERER),"Radeon"))
		flags |= MAC_GL_RADEON;

    // 3Dfx (any)
    if (strstr((char*)glGetString(GL_RENDERER),"3dfx"))
    	flags |= MAC_GL_3DFX;

	// 3Dfx (Voodoo 2/3 - 16bit only)
	if ( strstr((char*)glGetString(GL_RENDERER),"Voodoo 3") || strstr((char*)glGetString(GL_RENDERER),"Voodoo 2") )
		flags |= MAC_GL_V2_V3;

bail:
	// Clean up
	aglDestroyPixelFormat (fmt);
	aglDestroyRendererInfo (info);
	aglSetDrawable (ctx, NULL);
	aglSetCurrentContext (NULL);
	aglDestroyContext (ctx);	
	if (glWindow != NULL) {
		DisposeWindow (glWindow);
		glWindow = NULL;
	}
	
	return flags;
}


/*
=================
setupAGL
=================
*/
static AGLContext setupAGL (void)
{
	GLint		attrib[64];
	AGLPixelFormat	fmt;
	AGLContext	ctx;
	GLboolean	ok;
	GLint		result;
	int		i = 0;

	// Should probably only do this here on X but...
	if (!inwindow && !video_restart)
		VID_FadeOut ();

	// aglSetFullScreen doesn't let us choose a color depth, so we have to switch manually before going to fullscreen.
	// We also switch the actual resolution here, since it's totally possible that someone has their desktop
	// set to a big resolution where 32bit isn't supported by the card, but they want to play in 32bit in a lower res.
	// To make things more interesting, the pixel format will fail if we don't do this now...
	if (!inwindow && fullscreenX == USE_AGL)
	{
		CFDictionaryRef mode;
		boolean_t 		exactMatch;

		// Don't care about refresh (aglSetFullScreen sets it's own)
		mode = CGDisplayBestModeForParameters(vid_CGDevices[gScreen.profile->screen],
											  (size_t)gScreen.profile->colorbits,
											  (size_t)gScreen.mode->width,
											  (size_t)gScreen.mode->height,
											  &exactMatch );
		if (mode && exactMatch)
		{
			CGDisplayCapture(vid_CGDevices[gScreen.profile->screen]);
			CGDisplaySwitchToMode(vid_CGDevices[gScreen.profile->screen], mode);
		}
		else
			return NULL;
		
		// Need this on X if we want to use the (faster) aglSetFullScreen
		// Some systems WILL CRAP OUT here! - They should default to DSp and only get here via -useAGL
		attrib[i++] = AGL_FULLSCREEN;

		/* 
		I was under the impression that this would change monitor depth in
		fullscreen but it doesn't (hence the CG hack above) what *does* this do?
		attrib[i++] = AGL_PIXEL_SIZE;
		attrib[i++] = gScreen.profile->colorbits;
		*/
	}

	attrib[i++] = AGL_RGBA;
	attrib[i++] = AGL_DOUBLEBUFFER;
	attrib[i++] = AGL_NO_RECOVERY;
	attrib[i++] = AGL_ACCELERATED;

	if(gScreen.profile->texturebits == 32)
	{
		attrib[i++] = AGL_RED_SIZE;
		attrib[i++] = 8;
		attrib[i++] = AGL_GREEN_SIZE;
		attrib[i++] = 8;
		attrib[i++] = AGL_BLUE_SIZE;
		attrib[i++] = 8;
		attrib[i++] = AGL_ALPHA_SIZE;
		attrib[i++] = 0;
	}
	else
	{
		attrib[i++] = AGL_RED_SIZE;
		attrib[i++] = 5;
		attrib[i++] = AGL_GREEN_SIZE;
		attrib[i++] = 5;
		attrib[i++] = AGL_BLUE_SIZE;
		attrib[i++] = 5;
		attrib[i++] = AGL_ALPHA_SIZE;
		attrib[i++] = 0;
	}

	attrib[i++] = AGL_DEPTH_SIZE;
	attrib[i++] = 16;

	attrib[i++] = AGL_NONE;

	// Choose an rgb pixel format
	// if we don't have a GDHandle by now, we've got problems...
	if (gScreen.device->handle)
	{
// I still don't get how this knows which screen we're dealing with...
		// Apple: "Furthermore, the gdev and ndev parameters must be set to NULL
		// and zero, respectively, when the AGL_FULLSCREEN attribute is present."
		if (!inwindow && fullscreenX == USE_AGL)
			fmt = aglChoosePixelFormat (NULL, 0, attrib);
		else
			fmt = aglChoosePixelFormat (&gScreen.device->handle, 1, attrib);
	}
	else
		Sys_Error("Device handle not ready for aglChoosePixelFormat");

	if (fmt == NULL)
		Sys_Error("No GL renderers found.");

	ok = aglDescribePixelFormat (fmt,AGL_RED_SIZE,&result);
	Com_Printf("AGL_RED_SIZE: %d\n",ok?result:-1);
	ok = aglDescribePixelFormat (fmt,AGL_GREEN_SIZE,&result);
	Com_Printf("AGL_GREEN_SIZE: %d\n",ok?result:-1);
	ok = aglDescribePixelFormat (fmt,AGL_BLUE_SIZE,&result);
	Com_Printf("AGL_BLUE_SIZE: %d\n",ok?result:-1);
	ok = aglDescribePixelFormat (fmt,AGL_ALPHA_SIZE,&result);
	Com_Printf("AGL_ALPHA_SIZE: %d\n",ok?result:-1);
	ok = aglDescribePixelFormat (fmt,AGL_DEPTH_SIZE,&result);
	Com_Printf("AGL_DEPTH_SIZE: %d\n",ok?result:-1);

	// Create an AGL context
	ctx = aglCreateContext (fmt, NULL);
	if(ctx == NULL) return NULL;

	// Make the context the current context
	ok = aglSetCurrentContext (ctx);
	if (!ok) return NULL;

	// Create any necessary window and activate the context
	ok = _aglSetDrawableMonitor (ctx, windowed);
	if (!ok) return NULL;

	gl_vendor = (const char *)glGetString (GL_VENDOR);
	gl_renderer = (const char *)glGetString (GL_RENDERER);
	gl_version = (const char *)glGetString (GL_VERSION);
	gl_extensions = (const char *)glGetString (GL_EXTENSIONS);

	// Pixel format is no longer needed
	aglDestroyPixelFormat (fmt);

	return ctx;
}


/*
=================
cleanupAGL
=================
*/
static void cleanupAGL(AGLContext ctx)
{
	aglSetCurrentContext (NULL);
	aglSetDrawable (ctx, NULL);
	aglDestroyContext (ctx);
}

/* pOx - Note about extensions:

	Some tests have shown that multitexture *can* be faster at high reolutions (1024x768+)
	but it's always slower at low resolutions, or if there's a crapload of lightmaps on screen.
	This is probably only true for fast AGP cards with big pipes (like a GF3) - I doubt a Voodoo3
	or rage128 will be able to handle the increased texture bandwidth.
	
	Note that the current mtex implementation simply doesn't work. (Not surprising, as it was written
	before the ARB extension existed). In addition, dynamic lights use the VERY SLOW glTexSubImage
	*redundantly* when mtex is on (i.e. Quake uploads the same lightmap more than once per frame)
	This is a major slowdown (especially on OSX), and there's no easy way to fix it.
	
	We *DO* use multitexture on Sky textures if it's available (toggle with gl_singlepasssky),
	and an "easy" way around the subimage problem would be to only use multitexture with gl_flashblend 1,
	or default to 2 passes for surfaces marked for dynamic uploads.
	
	I have a vertex array renderering routine written that gains a few fps over the direct mode
	rendering currently used, but it's not as big a gain as one might hope. It requires that
	polygons be triangulated on-the-fly so glDrawArray/Elements can call a big list at once, this
	means we create new vertices just so the array can reuse'em - kinda dumb, but it works. There
	are some other issues I have with it, but it may be integrated into a future build.
	
	Still working on packed pixels...
	
	Anisotropic filtering code is in and working (see gl_draw.c)
	
	The other extensions are useful but won't ever be used in standard Quake.
*/

void CheckTextureExtensions (void)
{
	// This may not be the first time through
	gl_ext_multitexture_units = 0;
	gl_ext_anisotropic_max = 0;
	gl_ext_packedpixel = false;
	gl_ext_texture_env_combine = false;
	gl_ext_texture_env_add = false;

	// Only used on skies... (leave gl_mtexable=false or DIE! - not joking, you will crash hard)
	if ( strstr(gl_extensions, "GL_ARB_multitexture ") )
	{
		// True if gl_ext_multitexture_units > 0
		glGetIntegerv (GL_MAX_TEXTURE_UNITS_ARB, (GLint *)&gl_ext_multitexture_units);
		
		if (gl_ext_multitexture_units < 2)// This should never happen (can't mtex with 1 unit!)
			gl_ext_multitexture_units = 0;
	}

	// Not Yet...
	if ( strstr(gl_extensions, "GL_APPLE_packed_pixels ") ||
		 strstr(gl_extensions, "GL_APPLE_packed_pixel ") )
		gl_ext_packedpixel = true;

	// Not used in standard Quake
	if ( strstr(gl_extensions, "GL_EXT_texture_env_combine ") ||
		  strstr(gl_extensions, "GL_ARB_texture_env_combine ") )
		gl_ext_texture_env_combine = true;

	// Not used in standard Quake
	if ( strstr(gl_extensions, "GL_ARB_texture_env_add ") ||
		 strstr(gl_extensions, "GL_EXT_texture_env_add ") )
		gl_ext_texture_env_add = true;
}

void CheckArrayExtensions (void)
{
	gl_ext_compiled_vertex_array = false;

	// Not used...
	if ( strstr(gl_extensions, "GL_EXT_compiled_vertex_array ") )
		gl_ext_compiled_vertex_array = true;
}

/*
=================
GL_PrintAvailableExtensions
=================
*/
void GL_PrintAvailableExtensions (void)
{
	if (gl_ext_multitexture_units > 1)
		Com_Printf ("%i multitexture units available.\n", gl_ext_multitexture_units);

	if (gl_ext_anisotropic_max)
		Com_Printf ("Anisotropic filtering available.\n");

	if (gl_ext_packedpixel)
		Com_Printf ("Packed pixels available.\n");

	if (gl_ext_texture_env_combine)
		Com_Printf ("Texture combine env available.\n");

	if (gl_ext_texture_env_add)
		Com_Printf ("Texture add env available.\n");

	if (gl_ext_compiled_vertex_array)
		Com_Printf ("Compiled vertex arrays available.\n");
}


/*
=================
GL_DescribeRenderer_f
=================
*/
void GL_DescribeRenderer_f (void)
{
	qbool ext = true;

	if (ext)
		Com_Printf("OpenGL Driver:");

	Com_Printf ("\nVENDOR: %s\n", gl_vendor);
	Com_Printf ("RENDERER: %s\n", gl_renderer);
	Com_Printf ("VERSION: %s\n", gl_version);

	if (!ext)// print all extensions
	{
		int i, len, count;
		char temp[256];
		
		Com_Printf ("EXTENSIONS:\n");
		
		len = strlen(gl_extensions);
		temp[0] = '\0';
				
		for (i = count = 0; i < len; i++)
		{
			if (gl_extensions[i] == ' ')
			{
				temp[count] = '\0';
				Com_Printf(" %s\n", temp);
				temp[0] = '\0';
				count = 0;
			}
			else
			{
				temp[count] = gl_extensions[i];
				count++;
			}
		}
	}
	else
		GL_PrintAvailableExtensions ();// just describe ones we care about
}

/*
=================
GL_BeginRendering
=================
*/
void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = gScreen.mode->width;
	*height = gScreen.mode->height;
}

void GL_EndRendering (void)
{
	static qbool old_hwgamma_enabled;

	if (gContext == NULL) return;

	vid_hwgamma_enabled = vid_hwgammacontrol.value && vid_gammaworks
		&& !background && !inwindow;
	if (vid_hwgamma_enabled != old_hwgamma_enabled) {
		old_hwgamma_enabled = vid_hwgamma_enabled;
		if (vid_hwgamma_enabled && currentgammaramp)
			VID_SetDeviceGammaRamp (currentgammaramp);
		else
			RestoreHWGamma ();
	}

	// Check if VBL sync needs updating
	if (vblsave != vid_vsync.value)
		GL_SwapInterval ();
		
	if (!scr_skipupdate || block_drawing)
		aglSwapBuffers(gContext);
}

/*
=================
VID_FadeOut

Wrap auto-fades so we can control OS specific stuff (may go total CG for X in the future)
=================
*/
void VID_FadeOut (void)
{
	if ((inwindow || background || !vid_initialized))
	{
		CGTableCount sampleCount;

		// Save out the current (desktop) gamma
		CGGetDisplayTransferByTable(vid_CGDevices[gScreen.profile->screen], 256,
			gOriginalRedTable, gOriginalGreenTable, gOriginalBlueTable, &sampleCount);
		
	}
#ifdef USEDSPFADES
		DSpContext_FadeGammaOut (NULL, NULL);
#endif
}

/*
=================
VID_FadeIn
=================
*/
void VID_FadeIn (void)
{
#ifdef USEDSPFADES
	DSpContext_FadeGammaIn (NULL, NULL);
#endif
}

/*
=================
VID_ShiftPalette

Mac Monitor Gamma... OSX is a pain (DSp doesn't brighten)
=================
*/
void VID_ShiftPalette (unsigned char *palette)
{
	if (inwindow || background)
	{
		CGSetDisplayTransferByTable(vid_CGDevices[gScreen.profile->screen], 256,
									gOriginalRedTable, gOriginalGreenTable, gOriginalBlueTable);
	}
	else
	{
		if (vid_hwgamma_enabled)
		{
			V_UpdatePalette();
		}
		else
		{
			CGGammaValue redTable[256];
			CGGammaValue greenTable[256];
			CGGammaValue blueTable[256];
			int i;

			for (i = 0; i < 256 ; i++)
			{
				redTable[i] = i/255.0*v_gamma.value;
				greenTable[i] = i/255.0*v_gamma.value;
				blueTable[i] = i/255.0*v_gamma.value;
			}
			
			CGSetDisplayTransferByTable(vid_CGDevices[gScreen.profile->screen], 256,
										redTable, greenTable, blueTable);
		}
	}
}

static qbool customgamma = false;

/*
======================
VID_SetDeviceGammaRamp

Note: ramps must point to a static array
======================
*/
void VID_SetDeviceGammaRamp (unsigned short *ramps)
{
	static CGGammaValue gamma[3][256];
	size_t i;

	if (!vid_gammaworks || !vid_hwgamma_enabled)
		return;

	for (i = 0; i < 256; ++i)
	{
		gamma[0][i] = ramps[0 * 256 + i] / 65535.0;
		gamma[1][i] = ramps[1 * 256 + i] / 65535.0;
		gamma[2][i] = ramps[2 * 256 + i] / 65535.0;
	}

	CGSetDisplayTransferByTable (vid_CGDevices[gScreen.profile->screen],
								 256, gamma[0], gamma[1], gamma[2]);
	customgamma = true;
	currentgammaramp = ramps;
}

void InitHWGamma (void)
{
	vid_gammaworks = true;
	Com_Printf("Hardware Gamma enabled\n");
	vid_hwgamma_enabled = vid_hwgammacontrol.value && vid_gammaworks
		&& !background && !inwindow;
}

void RestoreHWGamma (void)
{
	customgamma = false;
	if (!inwindow)
		CGSetDisplayTransferByTable(vid_CGDevices[gScreen.profile->screen], 256,
			gOriginalRedTable, gOriginalGreenTable, gOriginalBlueTable);
}

/*
=================
VID_Shutdown
=================
*/
void VID_Shutdown (void)
{
	if (!inwindow)
		VID_FadeOut ();

	Mac_StopGL ();
	MacShutdownScreen ();

	if (!inwindow)
		VID_FadeIn ();

	RestoreHWGamma();

	DSpShutdown();

	IN_ShowCursor ();
}


/*
=================
VID_ResCompare

qsort callback (put video modes in a logical order)
=================
*/
int VID_ResCompare(const void *a, const void *b)
{
	int ra, rb;
	video_mode_t *ma = (video_mode_t *)a;
	video_mode_t *mb = (video_mode_t *)b;

	// This is more of a pain than it may seem...
	// Example: 1792x1344 has more pixels than 1920x1080 but the latter should logically come first

	// Why my GF3 lets me switch to 1920x1080 (or 640x818) is another story...

	// Sort by width first, then height, then refresh
	if (ma->width == mb->width)
	{
		if (ma->height == mb->height)
		{
			// This should *never* happen!
			if (ma->refresh == mb->refresh)
			{
				ra = rb = 0;
			}
			else
			{
				ra = ma->refresh>>16;
				rb = mb->refresh>>16;
			}
		}
		else
		{
			ra = ma->height;
			rb = mb->height;
		}
	}
	else
	{
		ra = ma->width;
		rb = mb->width;
	}

	return ra-rb;
}


/*
=================
VID_GetVideoModesForActiveDisplays

Fills in device handles and id's for up to MAX_VIDEODEVICES monitors,
and saves out a list of supported video modes.
=================
*/
void VID_GetVideoModesForActiveDisplays (void)
{
	int 			i, modeCount = 0, j;
	OSErr			err;
	DSpContextReference 	foundCntx;
	DSpContextAttributes 	foundAttr;
	Fixed			refreshRate;
	int			w, h;
	qbool			scrap;

	// Fire up DrawSprocket (we've already checked it's availability & version by now)
	if (DSpStartup()) Sys_Error ("DrawSprocket failed to initialize.");

	CGDisplayErr	CGerr;

	// FIXME! - Can we do this?
	// We assume that vid_devices[gl_vid_screen] == vid_CGDevices[gl_vid_screen]
	CGerr = CGGetActiveDisplayList(MAX_VIDEODEVICES, vid_CGDevices, &gCGNumDevices);
	if ( CGerr != noErr ) Sys_Error ("CGGetActiveDisplayList failed to return a device list");

	for (i = numDevices = 0; i < MAX_VIDEODEVICES; i++)
	{
		if (!i)// Get the first device
			vid_devices[numDevices].handle = DMGetFirstScreenDevice (dmOnlyActiveDisplays);
		else// Get the next device
			vid_devices[numDevices].handle = DMGetNextScreenDevice (vid_devices[numDevices-1].handle, dmOnlyActiveDisplays);

		// If we've got a display, get it's display modes...
		if (vid_devices[numDevices].handle)
		{
			// Get the display id
			err = DMGetDisplayIDByGDevice(vid_devices[numDevices].handle, &vid_devices[numDevices].id, false);
			if (err != noErr) Sys_Error ("Couldn't get display id.");

			// Get the first context as reported by DrawSprocket.
			// The initial version of this routine used pure DisplayManager routines (check the CVS for source)
			// DisplayManager seems to return some garbage modes on OSX, that also happen to *not* be supported
			// by DrawSprocket so as long as we're using DSp for fullscreen, we need to use it here (it's easier anyway)
			if ( !DSpGetFirstContext (vid_devices[numDevices].id, &foundCntx) )
			{
				Sys_Printf ("Gathering video modes for device %i...\n", numDevices);

				// Loop through all contexts on this device, and fill in our global structs
				while (foundCntx)
				{	
					BlockZero (&foundAttr, sizeof (DSpContextAttributes));

					if (DSpContext_GetAttributes(foundCntx, &foundAttr))
						break;// Just keep going (maybe another device will work)

					// All modes support 16 and 32 bit to make life easy...
					// (If a card can't do 32bit at a given resolution, you really shouldn't be playing Quake at that resolution!!)
					// Not sure how Voodoo cards will deal with this - They *should* report 32bit even though they never use it in 3D
					if (foundAttr.displayBestDepth == 32)
					{
						// Get our mode info
						refreshRate = foundAttr.frequency;// 0 is OK (LCD)
						w = foundAttr.displayWidth;
						h = foundAttr.displayHeight;

						// Make sure the mode is valid (in Quake terms)

						// 640x818 and 640x870 are valid on my GF3 under OS9.2.2 (can even set with Monitors CP)
						// These wouldn't be *so* bad since pixels stretch to fit in fullscreen, but Quake chokes on the aspect ratio.
						if ( (w >= 640 && h >= 480) && (w > h) )
						{
							scrap = false;

							// The mode should be OK - Check for duplicates (shouldn't happen)
							for (j = 0; j <= modeCount; j++)
							{
								if (vid_modes[numDevices][j].refresh == refreshRate &&
									vid_modes[numDevices][j].width == w  &&
									vid_modes[numDevices][j].height == h )
									scrap = true;	
							}

							// Save it...
							if (!scrap)
							{
								vid_modes[numDevices][modeCount].refresh = refreshRate;
								vid_modes[numDevices][modeCount].width = w;
								vid_modes[numDevices][modeCount].height = h;	
/*									
								Sys_Printf (" Mode %i: %ix%i,%iHz\n", modeCount, w, h, (int)(refreshRate>>16));
*/
								modeCount++;// Valid mode id (set with vid_mode), not *actual* modes!

								if (modeCount == MAX_DISPLAYMODES)
									break;// Enough already!
							}
						}
						else
							Sys_Printf (" Mode scrapped... (bad dimensions)\n");
					}

					// Get the next context on the current device
					if (DSpGetNextContext(foundCntx, &foundCntx))
						break;// Either none left (kDSpContextNotFoundErr) or something bad happened...		
				}
			}

			// Finish off the device struct
			vid_devices[numDevices].numModes = modeCount;
			vid_devices[numDevices].vendor = getGLVendorInfo(vid_devices[numDevices].handle);

			// Guess this *could* happen (device with no valid modes that is)
			if (modeCount) numDevices++;
		}
		else
			break;
	}

	if (!numDevices)
		Sys_Error ("Can't find any valid devices!");

	// DSp is kinda weird - The 'first' context is your desktop res, the next is the largest resolution.
	// We want a nice ordered list from small to big, with incrementing refresh rates...
	for (i = 0; i < numDevices ; i++)
		qsort((void*)vid_modes[i], vid_devices[i].numModes, sizeof(video_mode_t), VID_ResCompare);

/* DEBUG! - Create a virtual device to test multi-head code
// Better yet, someone send me a PCI Radeon and another electron22blue...
	vid_devices[numDevices].handle = vid_devices[0].handle;
	vid_devices[numDevices].id = vid_devices[0].id;
	vid_devices[numDevices].numModes = 2;
	vid_modes[numDevices][0].refresh = vid_modes[0][2].refresh;
	vid_modes[numDevices][0].width = vid_modes[0][2].width;
	vid_modes[numDevices][0].height = vid_modes[0][2].height;
	vid_modes[numDevices][1].refresh = vid_modes[0][12].refresh;
	vid_modes[numDevices][1].width = vid_modes[0][12].width;
	vid_modes[numDevices][1].height = vid_modes[0][12].height;
	numDevices++;
// DEBUG! */
}


/*
=================
VID_ModeList_f

Spits out all available modes for the current, or requested screen
=================
*/
void VID_ModeList_f (void)
{
	int screen, count, i;

	if (!numDevices)// Not yet
		return;

	// Use current screen
	if (Cmd_Argc() == 1)
	{
		screen = gScreen.profile->screen;
		count = gScreen.device->numModes;
	}
	// Use specified screen
	else if (atoi(Cmd_Argv(1))>numDevices-1)
	{
		Com_Printf ("Requested screen not found\n");
		return;
	}
	else
	{
		screen = atoi(Cmd_Argv(1));
		count = vid_devices[screen].numModes;
	}
	
	Com_Printf ("\nVideo modes for screen %i\n", screen);

	for (i = 0; i < count; i++)
	{
		Com_Printf (" Mode %2i: %ix%i,%iHz\n", i,
			vid_modes[screen][i].width,
			vid_modes[screen][i].height,
			(int)vid_modes[screen][i].refresh>>16);
	}
}


/*
=================
VID_DescribeCurrentMode_f

Spits out info on the current screen
=================
*/
void VID_DescribeCurrentMode_f (void)
{
	if (!numDevices)// Not yet
		return;
	
	Com_Printf ("\n Internal Monitor Id: %i\n", gScreen.profile->screen);
	
	Com_Printf (" Mode: %i (%ix%i,%iHz)\n", gScreen.profile->mode,
		gScreen.mode->width,
		gScreen.mode->height,
		(int)gScreen.mode->refresh>>16);
	
	Com_Printf (" Fullscreen Setup: ");
	
	if (fullscreenX == USE_AGL)
		Com_Printf ("AGL\n");
	else
		Com_Printf ("DSp\n");

	Com_Printf (" Fullscreen Depth: %i bit\n", gScreen.profile->colorbits);
	Com_Printf (" Texture Depth: %i bit\n", gScreen.profile->texturebits);
	Com_Printf (" Windowed: ");
	
	if (inwindow)
		Com_Printf ("Yes\n");
	else
		Com_Printf ("No\n");

	Com_Printf ("\n");
}

void VID_SetConWidth ()
{
	int i;

	if ((i = COM_CheckParm("-conwidth")) != 0)
		vid.conwidth = Q_atoi(COM_Argv(i+1));
	else
		vid.conwidth = vid.width;

	vid.conwidth &= 0xfff8; // make it a multiple of eight

	if (vid.conwidth < 320)
		vid.conwidth = 320;
	else if (vid.conwidth > vid.width)
		vid.conwidth = vid.width;

	// pick a conheight that matches with correct aspect
	vid.conheight = vid.conwidth*3 / 4;

	if ((i = COM_CheckParm("-conheight")) != 0)
		vid.conheight = Q_atoi(COM_Argv(i+1));
	if (vid.conheight < 200)
		vid.conheight = 200;
	else if (vid.conheight > vid.height)
		vid.conheight = vid.height;

	vid.width = vid.conwidth;
	vid.height = vid.conheight;
}

/*
=================
VID_Restart_f

Trashes the DSp context, the OpenGL context (possibly),
& associated windows, then creates new ones.

We only trash the OpenGL context if:
1). The render device has changed
2). The texture depth has changed
3). We're running on X, and go from window<->fullscreen OR change the colordepth! ('Cause X fullscreen is retarded)

If we trash the OpenGL context, the current game is dissconnected so
we can get a new pixel format and reload textures, otherwise we use
the current textures, and just change the screen and/or window size.
=================
*/
void VID_Restart_f (void)
{
	qbool		togglewindowed = false;
	qbool		texturereload = false;
	extern qbool background;
	extern qbool forcewindowed;

// This looks much worse than it really is :)

	if (background) return;

	// Make sure 'Init' routines know this is a restart, not a cold start
	video_restart = true;

	// Fix up our cvars...
	if (gl_vid_screen.value > numDevices-1 || gl_vid_screen.value < 0)
		Cvar_SetValue (&gl_vid_screen, vid_currentprofile.screen);

	if (vid_mode.value > vid_devices[(int)gl_vid_screen.value].numModes-1 || vid_mode.value < 0)
		Cvar_Set (&vid_mode, "0");// Default to mode 0 if we're in trouble

	if (gl_vid_colorbits.value != 16 && gl_vid_colorbits.value != 32)
		Cvar_SetValue (&gl_vid_colorbits, vid_currentprofile.colorbits);

	if (gl_texturebits.value != 16 && gl_texturebits.value != 32)
		Cvar_SetValue (&gl_texturebits, vid_currentprofile.texturebits);
		
	// Can't use fullscreen if forcewindowed (DEBUG, or pre 1.99 DSp on OSX)
	if (gl_vid_windowed.value || forcewindowed)	
		Cvar_Set (&gl_vid_windowed, "1");

	// Fill in our new profile to determine what's changed
	vid_newprofile.screen = (int)gl_vid_screen.value;
	vid_newprofile.mode = (int)vid_mode.value;
	vid_newprofile.colorbits = (int)gl_vid_colorbits.value;
	vid_newprofile.texturebits = (int)gl_texturebits.value;
	vid_newprofile.window = gl_vid_windowed.value ? true : false;

	// Do fades if going from full screen to windowed or vice-versa
	// NOTE: forcewindowed will never get this far so don't worry about it
	if ( (vid_newprofile.window && !inwindow) || (!vid_newprofile.window && inwindow) )
		togglewindowed = true;

	// Avoid scrapping the OpenGL context (texture reloads are a pain)
	if (vid_newprofile.screen != vid_currentprofile.screen ||
		vid_newprofile.texturebits != vid_currentprofile.texturebits
		|| ((togglewindowed || (vid_newprofile.colorbits != vid_currentprofile.colorbits)))
		)
		texturereload = true;

	// Warn that the current game will go away if a new OpenGL context is required
	// (We don't want to deal with reloading textures for an active map!)
	if (texturereload && cls.state != ca_disconnected)
	{
		CL_Disconnect_f ();
	}

	// Good to go, save our new settings
	vid_currentprofile.screen = vid_newprofile.screen;
	vid_currentprofile.mode = vid_newprofile.mode;
	vid_currentprofile.colorbits = vid_newprofile.colorbits;
	vid_currentprofile.texturebits = vid_newprofile.texturebits;
	vid_currentprofile.window = vid_newprofile.window;

	// Don't hang sound
	S_ClearBuffer();

	if (!inwindow || togglewindowed)
		VID_FadeOut ();

	// Get a jump on this even if we trash it later
	// (CGDisplaySwitchToMode will choke if fullscreen on X and the drawable is still attached)
	aglSetDrawable (gContext, NULL);

	// Trash everything..
	MacShutdownScreen();

	// Reset all texture refrences & kill the aglContext
	if (texturereload)
	{
		Mod_ClearAll ();
		Mac_StopGL ();
	}

	// Setup our new global video setting pointers
	gScreen.profile = &vid_currentprofile;
	gScreen.device = &vid_devices[vid_currentprofile.screen];
	gScreen.mode = &vid_modes[vid_currentprofile.screen][vid_currentprofile.mode];

	if (texturereload)
	{
		if (gScreen.profile->texturebits == 16) {
			gl_solid_format = GL_RGB5;
			gl_alpha_format = GL_RGBA4;
		} else {
			gl_solid_format = GL_RGB8;
			gl_alpha_format = GL_RGBA8;
		}
	}

	// Update the global windowed state
	windowed = inwindow = gScreen.profile->window ? true : false;

	// Deal with the cursor before we fade back in	
	if (inwindow && (key_dest == key_menu))
		Release_Cursor_f();
	else
		Capture_Cursor_f();

	// Create our new DSp context
	MacSetupScreen ();

	// Set up OpenGL screen from scratch, & reload all persistant textures
	if (texturereload)
	{
		Mac_StartGL ();
	}
	// Just reconnect our existing aglContext
	else
	{
		FixATIRect ();
		_aglSetDrawableMonitor (gContext, inwindow);
	}

	vid.width = gScreen.mode->width;
	vid.height = gScreen.mode->height;

	VID_SetConWidth();

	// Tell quake to use the (possibly) new dimensions
	vid.recalc_refdef = true;

	video_restart = false;

	// Redraw the screen to avoid white flash
	if (texturereload)
		SCR_UpdateScreen ();

	// Fade in (expect if we were, and still are, in a window)
	if (togglewindowed || !inwindow)
		VID_FadeIn ();

	// Show the window now rather than in _aglSetDrawableMonitor to avoid white texture flash
	if (inwindow)
	{
		ShowWindow(glWindow);
		SelectWindow(glWindow);
		BringToFront(glWindow);
	}

	VID_ShiftPalette(NULL);
}

extern qbool forcewindowed;

/*
=================
VID_ToggleWindow_f

A quick window<->fullscreen toggle that doesn't interfere with
other video cvars (can be safely used at any time).
=================
*/
void VID_ToggleWindow_f ()
{
	int old_screen, old_mode, old_colorbits, old_texturebits;

	video_restart = true;// So callback doesn't print

	// Save current settings
	old_screen = gl_vid_screen.value;
	old_mode = vid_mode.value;
	old_colorbits = gl_vid_colorbits.value;
	old_texturebits = gl_texturebits.value;

	// Make sure these aren't changed by video_restart
	Cvar_SetValue (&gl_vid_screen, gScreen.profile->screen);
	Cvar_SetValue (&vid_mode, gScreen.profile->mode);
	Cvar_SetValue (&gl_vid_colorbits, gScreen.profile->colorbits);
	Cvar_SetValue (&gl_texturebits, gScreen.profile->texturebits);

	// Toggle windowed mode (DON'T check gl_vid_windowed! in can be different than actual state)
	if (inwindow)	
		Cvar_Set (&gl_vid_windowed, "0");
	else
		Cvar_Set (&gl_vid_windowed, "1");

	// Do it...
	VID_Restart_f();

	video_restart = true;// 'cause vid_restart think's we're done

	// Restore any cvars that might have been changed, but not committed with a restart
	Cvar_SetValue (&gl_vid_screen, old_screen);
	Cvar_SetValue (&vid_mode, old_mode);
	Cvar_SetValue (&gl_vid_colorbits, old_colorbits);
	Cvar_SetValue (&gl_texturebits, old_texturebits);

	video_restart = false;
}

void Sys_SwitchToFinder_f (void);

/*
===================
VID_Init
===================
*/
void VID_Init (unsigned char *palette)
{
	Cvar_Register (&vid_mode);
	Cvar_Register (&gl_vid_windowed);
	Cvar_Register (&gl_vid_screen);
	Cvar_Register (&gl_vid_colorbits);
	Cvar_Register (&vid_vsync);
	Cvar_Register (&_windowed_mouse);
	Cvar_Register (&vid_hwgammacontrol);
	Cvar_Register (&gl_texturebits);
	
	Cmd_AddCommand ("gl_describerenderer", GL_DescribeRenderer_f);
	Cmd_AddCommand ("vid_modelist", VID_ModeList_f);
	Cmd_AddCommand ("vid_describecurrentmode", VID_DescribeCurrentMode_f);
	Cmd_AddCommand ("vid_restart", VID_Restart_f);
	
	
	// Should really go in 'Sys_Init' or something...
	Cmd_AddCommand ("finder", Sys_SwitchToFinder_f);
	
#ifdef DEBUG
	forcewindowed = true;
#endif
	
	if (!forcewindowed)
		Cmd_AddCommand ("vid_togglewindow", VID_ToggleWindow_f);

	// NOTE: "windowed" is the startup overide, "inwindow" is the runtime setting

	// Force windowed regardless of pref (DEBUG, and OSX with DSp < 1.99)
	if (forcewindowed) inwindow = true;

	// Use the last state we were in for our startup setting
	windowed = inwindow;

	Check_Gamma (palette);
	VID_SetPalette (palette);

	MacSetupScreen ();
	Mac_StartGL ();

	InitHWGamma ();

	vid_initialized = true;

	VID_SetConWidth();

	vid.colormap = host_colormap;
	vid.numpages = 2;

	vid_menudrawfn = NULL;
	vid_menukeyfn = NULL;
}

// TODO: implement this
void VID_SetCaption (char *text) {}

// kazik -->

// FIXME: this must be done somehow different

int isAltDown(void)
{
//    if (GetKeyState(VK_MENU) < 0)
//       return 1;
//    return 0;

    extern qbool    keydown[UNKNOWN + 256];
    return keydown[K_ALT];
}
int isCtrlDown(void)
{
//    if (GetKeyState(VK_CONTROL) < 0)
//        return 1;
//    return 0;

    extern qbool    keydown[UNKNOWN + 256];
    return keydown[K_CTRL];
}
int isShiftDown(void)
{
//    if (GetKeyState(VK_SHIFT) < 0)
//        return 1;
//    return 0;

    extern qbool    keydown[UNKNOWN + 256];
    return keydown[K_SHIFT];
}


// kazik <--
