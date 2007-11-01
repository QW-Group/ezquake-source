//_________________________________________________________________________________________________________________________nFO
// "vid_osx.m" - MacOS X Video driver
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				2001-2006 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
// Quake is copyrighted by id software		[http://www.idsoftware.com].
//
// Version History:
// v1.1.0: Improved performance in windowed mode.
//         Window can be resized.
//	       Changed "minimized in Dock mode": now plays in the document miniwindow rather than inside the application icon.
//	       Screenshots are saved in PNG format now.
//			Video mode list is getting sorted.
// v1.0.9: Moved functions for capturing and fading displays to "vid_shared_osx.m".
//	       Added "fade all displays" option.
//	       Added display selection.
// v1.0.7: Added variables "gl_fsaa" and "gl_pntriangles" for compatibility reasons.
// v1.0.5: Added windowed display modes.
//	       Added "minimized in Dock" mode.
//	       Displays are now catured manually due to a bug with CGReleaseAllDisplays().
//         Reduced the fade duration to 1.0s.
// v1.0.2: Added "DrawSprocket" style gamma fading.
//	       Fixed "Draw_Pic: bad coordinates" bug [for effective buffersizes < 320x240].
//         Default video mode is now: 640x480, 67hz.
//         Recognizes all supported [more than 15] video modes via console or config.cfg.
//         Some internal changes.
// v1.0.0: Initial release.
//____________________________________________________________________________________________________________________iNCLUDES

#pragma mark =Includes=

#import <AppKit/AppKit.h>

#import "FDScreenshot.h"

#import "Quake.h"
#import "QuakeView.h"
#import "quakedef.h"
#import "d_local.h"
#import "in_osx.h"
#import "vid_osx.h"

#pragma mark -

//_____________________________________________________________________________________________________________________dEFINES

#pragma mark =Defines=

#undef	VID_CAPTURE_ALL_DISPLAYS

#define VID_WINDOWED_MODES_NUM		3
#define	VID_BLACK_COLOR				0x00000000000000
#define	VID_NO_BUFFER_UPDATE		0
#define	VID_BUFFER_UPDATE			1

#pragma mark -

//____________________________________________________________________________________________________________________tYPEDEFS

#pragma mark =TypeDefs=

//typedef int				SInt;
//typedef unsigned int		UInt;

typedef enum 		{
                                VID_BLIT_2X1 = 0,
                                VID_BLIT_1X1,
                                VID_BLIT_1X2,
                                VID_BLIT_2X2,
                                VID_BLIT_WIN
                        } 	vid_blitmode_t;

typedef struct		{
                                UInt16			Width;
                                UInt16			Height;
                                UInt16			OffWidth;
                                UInt16			OffHeight;
                                vid_blitmode_t	BlitMode;
                                void			(*Blitter)();
                                UInt8			*OffBuffer;
                                short			*ZBuffer;
                                UInt8			*SurfCache;
                        }	vid_mode_t;

typedef struct		{
                                UInt16			Width;
                                UInt16			Height;
                                BOOL			Fullscreen;
                                char			Desc[128];
                        }	vid_modedesc_t;

#pragma mark -

//___________________________________________________________________________________________________________________vARIABLES

#pragma mark =Variables=

extern viddef_t				vid;

cvar_t						vid_mode				= { "vid_mode", "0", 0 };
cvar_t						vid_redrawfull			= { "vid_redrawfull", "0", 0 };
cvar_t						vid_wait				= { "vid_wait", "0", 0 };
cvar_t						vid_overbright			= { "gamma_overbright", "1", 1 };
cvar_t						_vid_default_mode		= { "_vid_default_mode", "0", 1 };
cvar_t						_vid_default_blit_mode	= { "_vid_default_blit_mode", "0", 1 };
cvar_t						_windowed_mouse			= { "_windowed_mouse","0", 0 };
cvar_t						gl_anisotropic			= { "gl_anisotropic", "0", 1 };
cvar_t						gl_truform				= { "gl_truform", "-1", 1 };
cvar_t						gl_multitexture			= { "gl_multitexture", "0", 1 };

unsigned short				d_8to16table[256];
unsigned					d_8to24table[256];
byte						vid_current_palette[768];	// save for mode changes

NSWindow *					gVidWindow = NULL;
BOOL						gVidDisplayFullscreen = YES,
							gVidIsMinimized,
							gVidFadeAllDisplays = NO;
float						gVidWindowPosX,
							gVidWindowPosY;
CGDirectDisplayID			gVidDisplayList[VID_MAX_DISPLAYS];
CGDisplayCount				gVidDisplayCount;
UInt32						gVidDisplay;

static CFDictionaryRef		gVidOriginalMode;
static CGDirectPaletteRef 	gVidPalette;
static CFDictionaryRef		gVidGameMode;
static UInt8				gVidBackingBuffer[24*24];
static vid_mode_t			gVidGraphMode;
static vid_modedesc_t *		gVidModeList;
static SInt16				gVidModeNums,
							gVidCurMode,
							gVidOldMode;
static UInt16				gVidDefaultMode = 0;
static char *				gVidBlitModeStr[] = { "2x1", "1x1", "1x2", "2x2", "" };
static SInt8				gVidMenuLine;
static double				gVidEndTestTime;
static BOOL					gVidIsInited = NO,
							gVidPaletteChanged = NO,
							gVidDefaultModeSet = NO,
							gVidTesting = NO;
static QuakeView *			gVidWindowView = NULL;
static NSImage *			gVidMiniWindow = NULL;
static NSImage *			gVidGrowboxImage = NULL;
static NSBitmapImageRep	*	gVidWindowBuffer = NULL;
static NSRect				gVidMiniWindowRect;
static UInt32				gVidWindowPalette[256];  

#if defined (QUAKE_WORLD)
static NSString				*gVidWindowTitle = NULL;
#endif /* QUAKE_WORLD */
                                                              
#pragma mark -

//_________________________________________________________________________________________________________fUNCTION_pROTOTYPES

#pragma mark =Function Prototypes=

BOOL		VID_HideFullscreen (BOOL);
void		VID_FadeGammaIn (BOOL, float);
BOOL		VID_GetModeList (void);
void		VID_GetBuffers (void);
void		VID_FlushBuffers (void);
UInt16 		VID_GetRowBytes (void);
void		VID_ClearScreen1x1 (void);
void		VID_Blit1x1 (void);
void		VID_Blit1x2 (void);
void		VID_Blit2x1 (void);
void		VID_Blit2x2 (void);
void		VID_SetBlitter (vid_blitmode_t, BOOL);
void		VID_OpenWindow (UInt16, UInt16);
void		VID_CloseWindow ();
qbool	VID_Screenshot (SInt8 *theFilename, void *theBitmap, UInt32 theWidth, UInt32 theHeight, UInt32 theRowbytes);
void		VID_SetOriginalMode (float);
void		VID_DescribeCurrentMode_f (void);
void		VID_DescribeMode_f (void);
void		VID_DescribeModes_f (void);
void		VID_ForceMode_f (void);
void		VID_NumModes_f (void);
void		VID_TestMode_f (void);
void 		VID_MenuKey (int);
void 		VID_MenuDraw (void);

static void	VID_InsertMode (UInt16 *, UInt16, UInt16, BOOL);

#pragma mark -

//____________________________________________________________________________________________________________VID_GetBuffers()

void	VID_GetBuffers (void)
{
    // just security:
    if (gVidGameMode == NULL)
    {
        Sys_Error("Video buffer request too early!\n");
    }
    
    // get the buffers:
    gVidGraphMode.OffBuffer = malloc(gVidGraphMode.OffWidth * gVidGraphMode.OffHeight * sizeof (UInt8)
                                     + gVidGraphMode.OffWidth * gVidGraphMode.OffHeight * sizeof (short)
                                     + D_SurfaceCacheForRes (gVidGraphMode.OffWidth, gVidGraphMode.OffHeight));
    if (gVidGraphMode.OffBuffer == NULL)
    {
        Sys_Error("Not enough memory for video buffers left!\n");
    }
    gVidGraphMode.ZBuffer = (short *) (gVidGraphMode.OffBuffer
                                       + gVidGraphMode.OffWidth * gVidGraphMode.OffHeight * sizeof (UInt8));
    gVidGraphMode.SurfCache = gVidGraphMode.OffBuffer
                            + gVidGraphMode.OffWidth * gVidGraphMode.OffHeight * sizeof (UInt8)
                            + gVidGraphMode.OffWidth * gVidGraphMode.OffHeight * sizeof (short);
    
    if (gVidDisplayFullscreen == NO)
    {
        // obtain window buffer:
        gVidWindowBuffer = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes: NULL
                                                                   pixelsWide: gVidGraphMode.Width
                                                                   pixelsHigh: gVidGraphMode.Height
                                                                bitsPerSample: 8
                                                              samplesPerPixel: 4
                                                                     hasAlpha: YES
                                                                     isPlanar: NO
                                                               colorSpaceName: NSDeviceRGBColorSpace
                                                                  bytesPerRow: gVidGraphMode.Width * 4
                                                                 bitsPerPixel: 32];
        if (gVidWindowBuffer == NULL)
        {
            Sys_Error ("Unabled to allocate the window buffer!\n");
        }
        else
        {
            if (gVidWindowView != NULL)
            {
                [gVidWindowView setBuffer: gVidWindowBuffer];
            }
        }
    }
    else
    {
        // required to make sure that there is no garbled stuff left from the cursor:
        IN_ShowCursor (NO);
        
        // clear the display:
        VID_ClearScreen1x1();
    }
}

//__________________________________________________________________________________________________________VID_FlushBuffers()

void	VID_FlushBuffers (void)
{
    // release the buffers:
    if (gVidGraphMode.OffBuffer != NULL)
    {
        D_FlushCaches ();
        free (gVidGraphMode.OffBuffer);
        gVidGraphMode.OffBuffer = NULL;
    }
    
    if (gVidWindowBuffer != NULL)
    {
        [gVidWindowBuffer release];
        gVidWindowBuffer = NULL;
    }
}

//____________________________________________________________________________________________________________VID_LockBuffer()

#ifdef QUAKE_WORLD

void 	VID_LockBuffer (void)
{
}

#endif /* QUAKE_WORLD */

//__________________________________________________________________________________________________________VID_UnlockBuffer()

#ifdef QUAKE_WORLD

void	VID_UnlockBuffer (void)
{
}

#endif /* QUAKE_WORLD */

//________________________________________________________________________________________________________VID_HideFullscreen()

BOOL	VID_HideFullscreen (BOOL theState)
{
    static BOOL		myOldState = NO;
    
    if (myOldState == theState || gVidDisplayFullscreen == NO || !gVidGameMode)
    {
        return (YES);
    }
    
    if (theState == YES)
    {
        // switch to original mode!
        VID_SetOriginalMode (0.0f);
    }
    else
    {
        // zero gamma:
        VSH_FadeGammaOut (gVidFadeAllDisplays, 0.0f);

        // capture the displays:
        if (VSH_CaptureDisplays (gVidFadeAllDisplays) == NO)
        {
            Sys_Error ("Unable to capture displays!\n");
        } 
 
        // switch to the old game mode!
        if (CGDisplaySwitchToMode (gVidDisplayList[gVidDisplay], gVidGameMode) != CGDisplayNoErr)
        {
            Sys_Error ("VID_HideFullscreen(): failed to return to old game mode!\n");
        }

        // clear the screen [because of the 2x1 blitter]:
        if (gVidGraphMode.BlitMode == VID_BLIT_2X1)
        {
            VID_ClearScreen1x1 ();
        }

        // restore the old palette:
        if (gVidPalette != NULL && CGDisplayCanSetPalette(gVidDisplayList[gVidDisplay]))
        {
            gVidPaletteChanged = YES;
        }
        
        // bring the gamma back:
        VSH_FadeGammaIn (gVidFadeAllDisplays, 0.0f);
    }
    
    myOldState = theState;
    return (YES);
}

//____________________________________________________________________________________________________________VID_InsertMode()

void	VID_InsertMode (UInt16 *theMode, UInt16 theWidth, UInt16 theHeight, BOOL theFullscreen)
{
    char	myResolution[16];
    UInt16	i;
    
    // add each resolution only once!
    if (*theMode >= VID_WINDOWED_MODES_NUM)
    {
        for (i= VID_WINDOWED_MODES_NUM; i < *theMode; i++)
        {
            if(theWidth == gVidModeList[i].Width && theHeight == gVidModeList[i].Height)
            {
                return;
            }
        }
    }
    
    // add values to the mode list:
    gVidModeList[*theMode].Width	= theWidth;
    gVidModeList[*theMode].Height	= theHeight;
    gVidModeList[*theMode].Fullscreen	= theFullscreen;						                                         
    // generate the description for the video menu:
    snprintf (myResolution, 16, "%dx%d", theWidth, theHeight);
    if (theFullscreen == YES)
    {
        snprintf (gVidModeList[*theMode].Desc, 128, "%-9s - fullscreen", myResolution);
    }
    else
    {
        snprintf (gVidModeList[*theMode].Desc, 128, "%-9s -   windowed", myResolution);
    }

    // look for our default resolution:
//    if (theWidth == VID_DEFAULT_WIDTH && theHeight == VID_DEFAULT_HEIGHT)
//   {
//        Cvar_SetValue (&_vid_default_mode, *theMode); 
//        gVidDefaultMode = *theMode;
//    }

    (*theMode)++;
}

//___________________________________________________________________________________________________________VID_GetModeList()

BOOL	VID_GetModeList (void)
{
    NSArray	*			myDisplayModes;
	NSMutableArray *	mySortedModes;
    UInt16				i, j,
						myWidth,
						myHeight,
						myDepth;
    
    // retrieve a list with all display modes:
    myDisplayModes = [(NSArray *) CGDisplayAvailableModes (gVidDisplayList[gVidDisplay]) retain];
	
    if (myDisplayModes == nil)
    {
        Sys_Error ("Unable to get list of available display modes.");
    }
    
	// sort the displat mode list:
	mySortedModes = [[NSMutableArray alloc] initWithArray: myDisplayModes];

	[mySortedModes sortUsingFunction: VSH_SortDisplayModesCbk context: nil];
	
    // count 8 bit display modes:
    gVidModeNums = VID_WINDOWED_MODES_NUM;
	
    for (i = 0; i < [mySortedModes count]; i++)
    {
        myDepth = [[[mySortedModes objectAtIndex:i] objectForKey: (NSString *)kCGDisplayBitsPerPixel] intValue];
        
		if (myDepth == 8)
        {
            gVidModeNums++;
        }
    }

    // any 8 bit modes present? [can't happen if windowed modes activated]
    if (gVidModeNums == 0)
    {
        Sys_Error ("Unable to get list of 8 bit display modes.");
    }
    
    // get memory for the mode list:
    gVidModeList = malloc (gVidModeNums * sizeof (vid_modedesc_t));

    if (gVidModeList == NULL)
    {
        Sys_Error ("Out of memory.");
    }

    // 320 x 240, windowed:
    j = 0;
    VID_InsertMode (&j, 320, 240, NO);

    // 640 x 480, windowed:
    j = 1;
    VID_InsertMode (&j, 640, 480, NO);

    // 800 x 600, windowed:
    j = 2;
    VID_InsertMode (&j, 800, 600, NO);

    // build the mode list:
    for (i = 0; i < [mySortedModes count]; i++)
    {
        myDepth = [[[mySortedModes objectAtIndex:i] objectForKey: (NSString *)kCGDisplayBitsPerPixel] intValue];
		
        if(myDepth == 8)
        {
            myWidth = [[[mySortedModes objectAtIndex:i] objectForKey: (NSString *)kCGDisplayWidth] intValue];
            myHeight = [[[mySortedModes objectAtIndex:i] objectForKey: (NSString *)kCGDisplayHeight] intValue];
                           
            // being a little bit over secure:
            if (j < gVidModeNums)
            {
                // insert the new mode into the list:
                VID_InsertMode (&j, myWidth, myHeight, YES);
            }
        }
    }
	
    gVidModeNums = j;
    
    [myDisplayModes release];
	[mySortedModes release];
	
    return (YES);
}

//____________________________________________________________________________________________________________VID_SetPalette()

void	VID_SetPalette (unsigned char *thePalette)
{
    UInt8	*myPalette = (UInt8 *) gVidWindowPalette,
			i = 0;

	// required for mode changes
	if (thePalette != vid_current_palette)
	{
		memcpy(vid_current_palette, thePalette, 768);
	}		
	
    if (CGDisplayCanSetPalette (gVidDisplayList[gVidDisplay]))
    {
        CGDeviceColor 		mySampleTable[256];

        // convert the palette to float:
        do
        {
            mySampleTable[i].red = (float) thePalette[i * 3] / 256.0f;
            mySampleTable[i].green = (float) thePalette[i * 3 + 1] / 256.0f;
            mySampleTable[i].blue = (float) thePalette[i * 3 + 2] / 256.0f;
            i++;
        } while (i != 0);
        
        // create a palette for core graphics:
        gVidPalette = CGPaletteCreateWithSamples (mySampleTable, 256);
    }

    // required for windowed mode!    
    do
    {
        myPalette[0] = thePalette[0];
        myPalette[1] = thePalette[1];
        myPalette[2] = thePalette[2];
        myPalette[3] = 0xff;
        
        myPalette	+= 4;
        thePalette	+= 3;
        i++;
    } while (i != 0);
    
    if (gVidPalette == NULL && gVidDisplayFullscreen == YES)
    {
        Com_Printf ("Can\'t create palette...\n");
    }
    else
    {
        gVidPaletteChanged = YES;
    }
}

//__________________________________________________________________________________________________________VID_ShiftPalette()

void	VID_ShiftPalette (unsigned char *palette)
{
    VID_SetPalette (palette);
}

//___________________________________________________________________________________________________________VID_GetRowBytes()

UInt16	VID_GetRowBytes (void)
{
    return (CGDisplayBytesPerRow (gVidDisplayList[gVidDisplay]));
}

//________________________________________________________________________________________________________VID_ClearScreen1x1()

void	VID_ClearScreen1x1 (void)
{
    UInt64	*myScreen;
    UInt32	myWidthLoop, myRowBytes, i, j;
 
    // get the base address of the display:         
    if ((myScreen = CGDisplayBaseAddress (gVidDisplayList[gVidDisplay])) == NULL)
    {
        return;
    }
    
    myRowBytes = (VID_GetRowBytes () - gVidGraphMode.Width) / sizeof(UInt64);

    // clear the display, use a different routine, if we have row bytes:    
    if (myRowBytes != 0)
    {
        myWidthLoop = gVidGraphMode.Width / sizeof(UInt64);

        for (i = 0; i < gVidGraphMode.Height; i++)
        {
            for (j = 0; j < myWidthLoop; j++)
            {
                *myScreen++ = VID_BLACK_COLOR;
            }
            myScreen += myRowBytes;
        }
    }
    else
    {
        myWidthLoop = gVidGraphMode.Height * (gVidGraphMode.Width / sizeof(UInt64));

        for (i = 0; i < myWidthLoop; i++)
        {
            *myScreen++ = VID_BLACK_COLOR;
        }
    }
}

//_______________________________________________________________________________________________________________VID_Blit1x1()

void	VID_Blit1x1(void)
{
    UInt64	*myScreen, *myOffScreen, myRowBytes;
    UInt32	myWidthLoop, i, j;

    // get the base address of the display:      
    if ((myScreen = CGDisplayBaseAddress(gVidDisplayList[gVidDisplay])) == NULL) return;
    
    myOffScreen = (UInt64 *) gVidGraphMode.OffBuffer;
    myRowBytes = (VID_GetRowBytes() - gVidGraphMode.Width) / sizeof (UInt64);
    
    // center the rectangle on the screen:
    if (gVidGraphMode.Height != gVidGraphMode.OffHeight)
    {
        myScreen += VID_GetRowBytes () * ((gVidGraphMode.Height- gVidGraphMode.OffHeight) >> 1) /sizeof(UInt64);
    }
    if (gVidGraphMode.Width != gVidGraphMode.OffWidth)
    {
        myScreen += ((gVidGraphMode.Width - gVidGraphMode.OffWidth) >> 1) / sizeof (UInt64);
        myRowBytes += (gVidGraphMode.Width - gVidGraphMode.OffWidth) / sizeof (UInt64);
    }
    myWidthLoop = gVidGraphMode.OffWidth / sizeof (UInt64);

    // wait for the VBL, if requested:
    if (vid_wait.value)
    {
        CGDisplayWaitForBeamPositionOutsideLines (gVidDisplayList[gVidDisplay], 0, 1);
    }

    // change the palette:
    if (gVidPaletteChanged  == YES && CGDisplayCanSetPalette (gVidDisplayList[gVidDisplay]))
    {
        CGDisplaySetPalette (gVidDisplayList[gVidDisplay], gVidPalette);
        gVidPaletteChanged = NO;
    }
    
    // blit the offscreen buffer to the video card, use a different routine, if we there are row bytes:
    if (myRowBytes)
    {
        myWidthLoop = gVidGraphMode.OffWidth / sizeof (UInt64);
        
        for (i = 0; i < gVidGraphMode.OffHeight; i++)
        {
            for (j = 0; j < myWidthLoop; j++)
            {
				*myScreen++ = *myOffScreen++;
            }
            myScreen += myRowBytes;
        }
    }
    else
    {
        myWidthLoop = gVidGraphMode.OffHeight * gVidGraphMode.OffWidth / sizeof (UInt64);
        
        for (i = 0; i < myWidthLoop; i++)
        {
			*myScreen++ = *myOffScreen++;
        }
    }
}

//_______________________________________________________________________________________________________________VID_Blit1x2()

void	VID_Blit1x2 (void)
{
    UInt64	*myScreen, *myScreen2, *myOffScreen, myRowBytes;
    UInt16	myWidthLoop, i, j;

    // get the base address of the display:    
    if ((myScreen = CGDisplayBaseAddress (gVidDisplayList[gVidDisplay])) == NULL)
    {
        return;
    }
    
    myScreen2 = myScreen + VID_GetRowBytes() / sizeof (UInt64);
    myOffScreen = (UInt64 *) gVidGraphMode.OffBuffer;
    
    myRowBytes = ((VID_GetRowBytes() << 1) - gVidGraphMode.Width) / sizeof (UInt64);
    myWidthLoop = gVidGraphMode.OffWidth / sizeof (UInt64);

    // wait for the VBL, if requested:
    if (vid_wait.value)
    {
        CGDisplayWaitForBeamPositionOutsideLines (gVidDisplayList[gVidDisplay], 0, 1);
    }

    // change the palette:
    if (gVidPaletteChanged == YES && CGDisplayCanSetPalette (gVidDisplayList[gVidDisplay]))
    {
        CGDisplaySetPalette (gVidDisplayList[gVidDisplay], gVidPalette);
        gVidPaletteChanged = NO;
    }

    // blit the offscreen buffer to the video card:
    for (i = 0; i < gVidGraphMode.OffHeight; i++)
    {
        for (j = 0; j < myWidthLoop; j++)
        {
            *myScreen++		= *myOffScreen;
            *myScreen2++	= *myOffScreen++;
        }
        myScreen += myRowBytes;
        myScreen2 += myRowBytes;
    }
}

//_______________________________________________________________________________________________________________VID_Blit2x1()

void	VID_Blit2x1 (void)
{
    UInt64	*myScreen, myPixels, myRowBytes;
    UInt8	*myOffScreen;
    UInt16	myWidthLoop, i, j;

    // get the base address of the display:
    if ((myScreen = CGDisplayBaseAddress (gVidDisplayList[gVidDisplay])) == NULL)
    {
        return;
    }
    
    myOffScreen = (UInt8 *) gVidGraphMode.OffBuffer;
    
    myRowBytes = ((VID_GetRowBytes() << 1) - gVidGraphMode.Width) / sizeof (UInt64);
    myWidthLoop = gVidGraphMode.OffWidth >> 2;
    
    // wait for the VBL, if requested:
    if (vid_wait.value)
    {
        CGDisplayWaitForBeamPositionOutsideLines (gVidDisplayList[gVidDisplay], 0, 1);
    }
    
    // change the palette:
    if (gVidPaletteChanged == YES && CGDisplayCanSetPalette (gVidDisplayList[gVidDisplay]))
    {
        CGDisplaySetPalette (gVidDisplayList[gVidDisplay], gVidPalette);
        gVidPaletteChanged = NO;
    }
    
    // blit the offscreen buffer to the video card:
    for (i = 0; i < gVidGraphMode.OffHeight; i++)
    {
        for (j = 0; j < myWidthLoop; j++)
        {
#ifdef __i386__

            myPixels  = ((UInt64) (*myOffScreen++));
            myPixels |= ((UInt64) (*myOffScreen++) << 16);
            myPixels |= ((UInt64) (*myOffScreen++) << 32);
            myPixels |= ((UInt64) (*myOffScreen++) << 48);

#else

            myPixels  = ((UInt64) (*myOffScreen++) << 48);
            myPixels |= ((UInt64) (*myOffScreen++) << 32);
            myPixels |= ((UInt64) (*myOffScreen++) << 16);
            myPixels |= ((UInt64) (*myOffScreen++));

#endif // __i386__

			*myScreen++ = myPixels | (myPixels << 8);
        }
		
        myScreen += myRowBytes;
    }
 }

//_______________________________________________________________________________________________________________VID_Blit2x2()

void	VID_Blit2x2 (void)
{
    UInt64	*myScreen, *myScreen2, myPixels, myRowBytes;
    UInt8	*myOffScreen;
    UInt16	myWidthLoop, i, j;
    
    // get the base address of the display:
    if ((myScreen = CGDisplayBaseAddress (gVidDisplayList[gVidDisplay])) == NULL)
    {
        return;
    }
    
    myScreen2 = myScreen + VID_GetRowBytes() / sizeof (UInt64);
    myOffScreen = (UInt8 *) gVidGraphMode.OffBuffer;
    
    myRowBytes = ((VID_GetRowBytes() << 1) - gVidGraphMode.Width) / sizeof (UInt64);
    myWidthLoop = gVidGraphMode.OffWidth >> 2;
    
    // wait for the VBL, if requested:
    if (vid_wait.value)
    {
        CGDisplayWaitForBeamPositionOutsideLines (gVidDisplayList[gVidDisplay], 0, 1);
    }
    
    // change the palette:
    if (gVidPaletteChanged == YES && CGDisplayCanSetPalette (gVidDisplayList[gVidDisplay]))
    {
        CGDisplaySetPalette (gVidDisplayList[gVidDisplay], gVidPalette);
        gVidPaletteChanged = NO;
    }
    
    // blit the offscreen buffer to the video card:
    for (i = 0; i < gVidGraphMode.OffHeight; i++)
    {
        for (j = 0; j < myWidthLoop; j++)
        {
#ifdef __i386__

            myPixels  = ((UInt64) (*myOffScreen++));
            myPixels |= ((UInt64) (*myOffScreen++) << 16);
            myPixels |= ((UInt64) (*myOffScreen++) << 32);
            myPixels |= ((UInt64) (*myOffScreen++) << 48);

#else

            myPixels  = ((UInt64) (*myOffScreen++) << 48);
            myPixels |= ((UInt64) (*myOffScreen++) << 32);
            myPixels |= ((UInt32) (*myOffScreen++) << 16);
            myPixels |= ((UInt64) (*myOffScreen++));

#endif // __i386__

            myPixels |= (myPixels << 8);
			
            *myScreen++ = myPixels;
            *myScreen2++ = myPixels;
        }
        myScreen += myRowBytes;
        myScreen2 += myRowBytes;
    }
}

//______________________________________________________________________________________________________VID_DrawBufferToRect()

void	VID_DrawBufferToRect (NSRect theRect)
{
    register UInt32	*myDestinationBuffer	= (UInt32 *) [gVidWindowBuffer bitmapData];
    register UInt8	*mySourceBuffer			= gVidGraphMode.OffBuffer,
					*mySourceBufferEnd		= mySourceBuffer + gVidGraphMode.Width * gVidGraphMode.Height;

    if (myDestinationBuffer != NULL)
    {
        // translate 8 bit to 32 bit color:
        while (mySourceBuffer < mySourceBufferEnd)
        {
            *myDestinationBuffer++ = gVidWindowPalette[*mySourceBuffer++];
        }
        
        // draw the image:
        [gVidWindowBuffer drawInRect: theRect];
    }
}

//__________________________________________________________________________________________________________VID_BlitWindowed()

void	VID_BlitWindowed (void)
{
    // any view available?
    if (gVidWindowView != NULL && gVidWindow != NULL)
    {
        gVidPaletteChanged = NO;
    
        if ([gVidWindow isMiniaturized] == YES)
        {
            if (gVidMiniWindow != NULL)
            {
                [gVidMiniWindow lockFocus];
                VID_DrawBufferToRect (gVidMiniWindowRect);
                [gVidMiniWindow unlockFocus];
                [gVidWindow setMiniwindowImage: gVidMiniWindow];
            }
        }
        else
        {
            // we could use QuickDraw here, but there is an issue with QuickDraw:
            // If the user changes the display depth, Quake will hang forever.
    
            [gVidWindowView lockFocus];
            
            // draw the current buffer:
            VID_DrawBufferToRect ([gVidWindowView bounds]);
            
            // draw the growbox (we could avoid this step by using the "display" method,
            // but this will decrease performance much more by drawing a custom growbox: 
            if (gVidGrowboxImage != NULL)
            {
                NSSize	myGrowboxSize = [gVidGrowboxImage size];
                NSRect	myViewRect = [gVidWindowView bounds];
                NSPoint	myGrowboxLocation = NSMakePoint (NSMaxX (myViewRect) - myGrowboxSize.width, NSMinY (myViewRect));
                
                [gVidGrowboxImage compositeToPoint: myGrowboxLocation operation: NSCompositeSourceOver];
            }
            
            [gVidWindowView unlockFocus];
            [gVidWindow flushWindow];
        }
    }
}

//____________________________________________________________________________________________________________VID_SetBlitter()

void	VID_SetBlitter (vid_blitmode_t theBlitMode, BOOL theBufferUpdate)
{
    int	myTemp;

    // don't allow blitmodes other than 1x1 if our display size is < 640x480:
    if (gVidDisplayFullscreen == YES && (gVidGraphMode.Width < 640 || gVidGraphMode.Height < 480))
    {
        if (gVidGraphMode.BlitMode == VID_BLIT_1X1)
        {
            return;
        }
        theBlitMode = VID_BLIT_1X1;
    }

    // return, if the new blitmode is equal to the old one:
    if (theBlitMode == gVidGraphMode.BlitMode && gVidGraphMode.Blitter != NULL)
    {
        return;
    }

    myTemp = scr_disabled_for_loading;
    
    // free the buffers, if a buffer update is requested:
    if (theBufferUpdate == VID_BUFFER_UPDATE)
    {
        scr_disabled_for_loading = 1;
        VID_FlushBuffers ();
    }

    // just a check [in case someone played with config.cfg]:
	if (_vid_default_blit_mode.value < 0)
	{
		_vid_default_blit_mode.value = 0;
	}
	
    if (_vid_default_blit_mode.value > 4)
    {
        _vid_default_blit_mode.value = 3;
    }

    if (gVidDisplayFullscreen == YES)
	{
		if (theBlitMode == VID_BLIT_WIN)
		{
			theBlitMode = VID_BLIT_2X2;
		}
	}
	else
    {
        theBlitMode = VID_BLIT_WIN;
    }

    
    // setup the misc. blitmodes:
    switch (theBlitMode)
    {
        case VID_BLIT_1X2:
            gVidGraphMode.BlitMode = VID_BLIT_1X2;
            gVidGraphMode.Blitter  = &VID_Blit1x2;
            if (theBufferUpdate == VID_NO_BUFFER_UPDATE)
                return;
            gVidGraphMode.OffWidth = gVidGraphMode.Width;
            gVidGraphMode.OffHeight = gVidGraphMode.Height >> 1;
            break;
        case VID_BLIT_2X1:
            gVidGraphMode.BlitMode = VID_BLIT_2X1;
            gVidGraphMode.Blitter  = &VID_Blit2x1;        
            if (theBufferUpdate == VID_NO_BUFFER_UPDATE)
                return;
            gVidGraphMode.OffWidth = gVidGraphMode.Width >> 1;
            gVidGraphMode.OffHeight = gVidGraphMode.Height >> 1;
            break;
        case VID_BLIT_2X2:
            gVidGraphMode.BlitMode = VID_BLIT_2X2;
            gVidGraphMode.Blitter  = &VID_Blit2x2;
            if (theBufferUpdate == VID_NO_BUFFER_UPDATE)
                return;
            gVidGraphMode.OffWidth = gVidGraphMode.Width >> 1;
            gVidGraphMode.OffHeight = gVidGraphMode.Height >> 1;
            break;
        case VID_BLIT_WIN:
            gVidGraphMode.BlitMode = VID_BLIT_WIN;
            gVidGraphMode.Blitter  = &VID_BlitWindowed;
            if (theBufferUpdate == VID_NO_BUFFER_UPDATE)
                return;
            gVidGraphMode.OffWidth = gVidGraphMode.Width;
            gVidGraphMode.OffHeight = gVidGraphMode.Height;
            break;
        case VID_BLIT_1X1:
        default:
            gVidGraphMode.BlitMode = VID_BLIT_1X1;
            gVidGraphMode.Blitter  = &VID_Blit1x1;
            if (theBufferUpdate == VID_NO_BUFFER_UPDATE)
                return;
            gVidGraphMode.OffWidth = gVidGraphMode.Width;
            gVidGraphMode.OffHeight = gVidGraphMode.Height;
            break;
    }
    
    // allocate buffers for the new blitmode:
    VID_GetBuffers ();
    
    // setup vid struct for Quake:
    vid.width = vid.conwidth = gVidGraphMode.OffWidth;
    vid.height = vid.conheight = gVidGraphMode.OffHeight;
    vid.aspect = ((float) gVidGraphMode.OffHeight / (float) gVidGraphMode.OffWidth) * (320.0f / 240.0f);
    vid.numpages = 1;
    vid.colormap = host_colormap;
//    vid.fullbright = 256 - LittleLong (*((int *) vid.colormap + 2048));
    vid.buffer = /*vid.conbuffer =*/ gVidGraphMode.OffBuffer;
    vid.rowbytes = /*vid.conrowbytes =*/ gVidGraphMode.OffWidth;
    vid.direct = 0;
    vid.recalc_refdef = 1;
    
    // get new buffers for Quake:
    d_pzbuffer = gVidGraphMode.ZBuffer;
    D_InitCaches (gVidGraphMode.SurfCache, D_SurfaceCacheForRes (gVidGraphMode.OffWidth, gVidGraphMode.OffHeight));

    scr_disabled_for_loading = myTemp;
}

//__________________________________________________________________________________________________________________VID_Init()

void	VID_Init (unsigned char *thePalette)
{
    NSString *		myGrowboxPath = NULL;

    // register variables:
    Cvar_Register (&vid_mode);
    Cvar_Register (&vid_redrawfull);
    Cvar_Register (&vid_wait);
    Cvar_Register (&vid_overbright);
    Cvar_Register (&_vid_default_mode);
    Cvar_Register (&_vid_default_blit_mode);
    Cvar_Register (&gl_anisotropic);
    Cvar_Register (&gl_truform);
    Cvar_Register (&gl_multitexture);
#ifndef QUAKE_WORLD
    Cvar_Register (&_windowed_mouse);
#endif /* QUAKE_WORLD */
    
    // register console commands:
    Cmd_AddCommand ("vid_describecurrentmode", VID_DescribeCurrentMode_f);
    Cmd_AddCommand ("vid_describemode", VID_DescribeMode_f);
    Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f);
    Cmd_AddCommand ("vid_forcemode", VID_ForceMode_f);
    Cmd_AddCommand ("vid_nummodes", VID_NumModes_f);
    Cmd_AddCommand ("vid_testmode", VID_TestMode_f);
    
    vid_menudrawfn = VID_MenuDraw;
    vid_menukeyfn = VID_MenuKey;
    
    // retrieve a list of video modes:
    if (!VID_GetModeList())
    {
        Sys_Error ("No valid video modes found!");
    }
    
    gVidCurMode = _vid_default_mode.value;
    
    // revert to mode no. 0, if mode no. not available:
    if (gVidCurMode >= gVidModeNums)
    {
        gVidCurMode = 0;
        Cvar_SetValue (&_vid_default_mode, gVidCurMode);
    }
    
    // save the original display mode:
    gVidOriginalMode = CGDisplayCurrentMode (gVidDisplayList[gVidDisplay]);

    // initialize the miniwindow [will not be used, if alloc fails]:
    gVidMiniWindow = [[NSImage alloc] initWithSize: NSMakeSize (128, 128)];
    gVidMiniWindowRect = NSMakeRect (0.0f, 0.0f, [gVidMiniWindow size].width, [gVidMiniWindow size].height);
    VSH_DisableQuartzInterpolation (gVidMiniWindow);
    
    // initialize the growbox image [required for windowed modes]:
    myGrowboxPath = [[NSBundle mainBundle] pathForResource: @"GrowBox" ofType: @"tiff"];
    if (myGrowboxPath != NULL)
    {
        gVidGrowboxImage = [[NSImage alloc] initWithContentsOfFile: myGrowboxPath];
    }
    
    // hide the cursor:
    IN_ShowCursor (NO);

    // fade gamma out:
//    VSH_FadeGammaOut (gVidFadeAllDisplays, VID_FADE_DURATION);

    // capture display(s):
//    if (VSH_CaptureDisplays (gVidFadeAllDisplays) == NO)
//    {
//        Sys_Error ("Unable to capture display(s)!");
//    }

    // switch to the desired mode:
    if (!VID_SetMode (gVidCurMode, thePalette))
    {
        Sys_Error ("Can\'t initialize video!");
    }

    // setup the blitter:
    VID_SetBlitter (_vid_default_blit_mode.value, VID_NO_BUFFER_UPDATE);

    // fade gamma in:
//    VSH_FadeGammaIn (gVidFadeAllDisplays, 0.0f);

    gVidIsInited = YES;
}

//_______________________________________________________________________________________________________VID_SetOriginalMode()

void	VID_SetOriginalMode (float theFadeDuration)
{
    // not required if we are in windowed mode!
    if (gVidDisplayFullscreen == NO)
    {
        return;
    }

    // zero gamma:
    VSH_FadeGammaOut (gVidFadeAllDisplays, 0.0f);

    // get the original display mode back:
    CGDisplaySwitchToMode (gVidDisplayList[gVidDisplay], gVidOriginalMode);

    // release the displays:
    VSH_ReleaseDisplays (gVidFadeAllDisplays); 

    // bring the gamma back:
    VSH_FadeGammaIn (gVidFadeAllDisplays, theFadeDuration);

}

//______________________________________________________________________________________________________________VID_Shutdown()

void	VID_Shutdown (void)
{
    if (gVidIsInited == YES)
    {
        // close window if available:
        VID_CloseWindow ();
        
#ifdef QUAKE_WORLD
        if (gVidWindowTitle != NULL)
        {
            [gVidWindowTitle release];
            gVidWindowTitle = NULL;
        }
#endif /* QUAKE_WORLD */

        if (gVidMiniWindow != NULL)
        {
            [gVidMiniWindow release];
            gVidMiniWindow = NULL;
        }
        
        if (gVidGrowboxImage != NULL)
        {
            [gVidGrowboxImage release];
            gVidGrowboxImage = NULL;
        }
        
        if (gVidOriginalMode != NULL)
        {
            VID_SetOriginalMode (VID_FADE_DURATION);
            
            // clean up the gamma fading:
            VSH_FadeGammaRelease ();
        }
        
        // free internal buffers:
        VID_FlushBuffers ();
        
        gVidIsInited = NO;
    }
}

//_______________________________________________________________________________________________________________VID_SetMode()

int	VID_SetMode (int theMode, unsigned char *thePalette)
{
    CFDictionaryRef	myGameMode;
    boolean_t		myExactMatch;
    int				myTemp;

    // check if the selected video mode is valid:
    if (theMode < 0 || theMode >= gVidModeNums || (theMode == gVidCurMode && gVidIsInited == YES))
    {
        Com_Printf ("Invalid video mode.\n");
        Cvar_SetValue (&vid_mode, gVidCurMode);
        gVidTesting = NO;
        return (0);
    }
    
    Com_Printf ("Switching to: %s\n", gVidModeList[theMode].Desc);

    if (gVidModeList[theMode].Fullscreen == YES)
    {
        // get the new display mode:
        myGameMode = CGDisplayBestModeForParameters (gVidDisplayList[gVidDisplay],
                                                     8,
                                                     gVidModeList[theMode].Width,
                                                     gVidModeList[theMode].Height,
                                                     &myExactMatch);
        // any matches?
        if (myGameMode == NULL || !myExactMatch) return(0);
        gVidGameMode = myGameMode;

        myTemp = scr_disabled_for_loading;
        scr_disabled_for_loading = 1;

        VID_CloseWindow ();
            
        if (VSH_CaptureDisplays (gVidFadeAllDisplays) == NO)
        {
            Sys_Error ("Unable to capture display(s)!\n");
        }
        
        // switch to the mode:
        if (CGDisplaySwitchToMode (gVidDisplayList[gVidDisplay], myGameMode) != CGDisplayNoErr)
        {
            Sys_Error ("Unable to switch the display mode!");
        }
        
        gVidDisplayFullscreen = YES;
    }
    else
    {
        myTemp = scr_disabled_for_loading;
        scr_disabled_for_loading = 1;
        
        if (gVidDisplayFullscreen == YES)
        {
            // revert to the standard screen resolution:
            VID_SetOriginalMode (0.0f);
        }

        // close existing windows:
        VID_CloseWindow ();

        // open the window:
        VID_OpenWindow (gVidModeList[theMode].Width, gVidModeList[theMode].Height);
        
        gVidGameMode = gVidOriginalMode;
        gVidDisplayFullscreen = NO;
    }
        
    // free all buffers:
    VID_FlushBuffers ();

    gVidGraphMode.Width = gVidModeList[theMode].Width;
    gVidGraphMode.Height = gVidModeList[theMode].Height;
    
    // just a check [in case someone played with config.cfg]:
    if (gVidDisplayFullscreen == YES)
    {
        // don't allow blitmodes other than 1x1 for video modes < 640x480:
        if (gVidGraphMode.Width < 640 || gVidGraphMode.Height < 480)
        {
            VID_SetBlitter (VID_BLIT_1X1, VID_NO_BUFFER_UPDATE);
        }
		
        if (gVidGraphMode.BlitMode == VID_BLIT_WIN)
        {
            VID_SetBlitter (VID_BLIT_2X2, VID_NO_BUFFER_UPDATE);
        }
    }
    else
    {
        if (gVidGraphMode.BlitMode != VID_BLIT_WIN)
        {
            VID_SetBlitter (VID_BLIT_WIN, VID_NO_BUFFER_UPDATE);            
        }
    }
    
    // setup the blit-rectangle:
    switch (gVidGraphMode.BlitMode)
    {
        case VID_BLIT_1X2:
            gVidGraphMode.OffWidth = gVidGraphMode.Width;
            gVidGraphMode.OffHeight = gVidGraphMode.Height >> 1;
            break;
        case VID_BLIT_2X1:
            gVidGraphMode.OffWidth = gVidGraphMode.Width >> 1;
            gVidGraphMode.OffHeight = gVidGraphMode.Height >> 1;
            break;
        case VID_BLIT_2X2:
            gVidGraphMode.OffWidth = gVidGraphMode.Width >> 1;
            gVidGraphMode.OffHeight = gVidGraphMode.Height >> 1;
            break;
        case VID_BLIT_1X1:
        case VID_BLIT_WIN:
        default:
            gVidGraphMode.OffWidth = gVidGraphMode.Width;
            gVidGraphMode.OffHeight = gVidGraphMode.Height;
            break;
    }
   
    // allocate new buffers:
    VID_GetBuffers ();
    
    // setup vid struct for Quake:
    vid.width = vid.conwidth = gVidGraphMode.OffWidth;
    vid.height = vid.conheight = gVidGraphMode.OffHeight;
    vid.aspect = ((float) gVidGraphMode.OffHeight / (float) gVidGraphMode.OffWidth) * (320.0f / 240.0f);
    vid.numpages = 1;
    vid.colormap = host_colormap;
//  vid.fullbright = 256 - LittleLong (*((int *) vid.colormap + 2048));
    vid.buffer = /*vid.conbuffer =*/ gVidGraphMode.OffBuffer;
    vid.rowbytes = /*vid.conrowbytes =*/ gVidGraphMode.OffWidth;
    vid.direct = 0;
    
    // get new buffers for Quake:
    d_pzbuffer = gVidGraphMode.ZBuffer;
    D_InitCaches (gVidGraphMode.SurfCache,
                  D_SurfaceCacheForRes (gVidGraphMode.OffWidth, gVidGraphMode.OffHeight));
    
    if (thePalette)
    {
        VID_SetPalette (thePalette);
    }
    gVidPaletteChanged = YES;

    Cvar_SetValue (&vid_mode, theMode);
    gVidCurMode = theMode;
    vid.recalc_refdef = 1;

    scr_disabled_for_loading = myTemp;
    
    return(1);
}

//________________________________________________________________________________________________________VID_SetWindowTitle()

#ifdef QUAKE_WORLD

void	VID_SetWindowTitle (char *theTitle)
{
    if (gVidWindowTitle != NULL)
    {
        [gVidWindowTitle release];
    }
    gVidWindowTitle = [[NSString alloc] initWithCString: theTitle];

    if (gVidWindow != NULL)
    {
        if (gVidWindowTitle != NULL)
        {
            [gVidWindow setTitle: gVidWindowTitle];
        }
        else
        {
            [gVidWindow setTitle: @"EZQuake"];
        }
    }
}

#endif /* QUAKE_WORLD */

//____________________________________________________________________________________________________________VID_OpenWindow()

void	VID_OpenWindow (UInt16 theWidth, UInt16 theHeight)
{
    NSRect	myContentRect = NSMakeRect (0, 0, theWidth, theHeight);
    
    // open the window:
    gVidWindow = [[NSWindow alloc] initWithContentRect: myContentRect
                                             styleMask: NSTitledWindowMask |
                                                        NSClosableWindowMask |
                                                        NSMiniaturizableWindowMask |
                                                        NSResizableWindowMask
                                               backing: NSBackingStoreBuffered
                                                 defer: NO];

    if (gVidWindow == NULL)
    {
        Sys_Error ("Unable to create window!\n");
    }

#ifdef QUAKE_WORLD
    if (gVidWindowTitle == NULL)
    {
        gVidWindowTitle = [[NSString alloc] initWithString: @"EZQuake"];
        if (gVidWindowTitle == NULL)
        {
            [gVidWindow setTitle: @"EZQuake"];
        }
        else
        {
            [gVidWindow setTitle: gVidWindowTitle];
        }
    }
    else
    {
        [gVidWindow setTitle: gVidWindowTitle];
    }
#else
    [gVidWindow setTitle: @"EZQuake"];
#endif /* QUAKE_WORLD */

    // setup the content view:
    myContentRect = NSMakeRect (0, 0, theWidth, theHeight);
    gVidWindowView = [[QuakeView alloc] initWithFrame: myContentRect];
    
    if (gVidWindowView == NULL)
    {
        Sys_Error ("Unable to create content view!\n");
    }

    [gVidWindow setDocumentEdited: YES];
    [gVidWindow setMinSize: NSMakeSize (theWidth, theHeight)];
    [gVidWindow setShowsResizeIndicator: NO];
    [gVidWindow setBackgroundColor: [NSColor blackColor]];
    [gVidWindow useOptimizedDrawing: YES];
    [gVidWindow setContentView: gVidWindowView];
    [gVidWindow makeFirstResponder: gVidWindowView];
    [gVidWindow setDelegate: gVidWindowView];
    
    [NSApp activateIgnoringOtherApps: YES];
    [gVidWindow center];
    [gVidWindow display];
    [gVidWindow setAcceptsMouseMovedEvents: YES];
    [gVidWindow makeKeyAndOrderFront: nil];
    [gVidWindow makeMainWindow];

    VSH_DisableQuartzInterpolation (gVidWindowView);
}

//___________________________________________________________________________________________________________VID_CloseWindow()

void	VID_CloseWindow ()
{
    // close the old window [will automagically be released when closed]:
    if (gVidWindow != NULL)
    {
        [gVidWindow close];
        gVidWindow = NULL;
    }

    // remove the old view:
    if (gVidWindowView != NULL)
    {
        [gVidWindowView release];
        gVidWindowView = NULL;
    }
    
    // free the RGBA buffer:
    if (gVidWindowBuffer != NULL)
    {
        [gVidWindowBuffer release];
        gVidWindowBuffer = NULL;
    }
}

//____________________________________________________________________________________________________________VID_Screenshot()

qbool VID_Screenshot (SInt8 *theFilename, void *theBitmap, UInt32 theWidth, UInt32 theHeight, UInt32 theRowbytes)
{
    NSString *	myFilename		= [NSString stringWithCString: (const char*) theFilename];
    NSSize		myBitmapSize	= NSMakeSize ((float) theWidth, (float) theHeight);
    
    return ([FDScreenshot writeToPNG: myFilename fromRGB24: theBitmap withSize: myBitmapSize rowbytes: theRowbytes]);
}

//________________________________________________________________________________________________________________VID_Update()

void	VID_Update (vrect_t *theRects)
{
	BOOL	myBlitterChanged = NO;

    // do nothing if not initialized:
    if (gVidIsInited == NO || gVidGraphMode.Blitter == NULL)
    {
        return;
    }
    
    // check for the default value from config.cfg:
    if (gVidDefaultModeSet == NO)
    {
        if (_vid_default_mode.value != gVidDefaultMode)
        {
            // set the default video mode:
            if (_vid_default_mode.value < 0 || _vid_default_mode.value >= gVidModeNums)
			{
                Cvar_SetValue (&_vid_default_mode, 0.0); 
			}
			
            Cvar_SetValue (&vid_mode, _vid_default_mode.value);
            gVidDefaultModeSet = YES;
		}

		if (_vid_default_blit_mode.value)
		{
			if (_vid_default_blit_mode.value < 0 || _vid_default_blit_mode.value > 4)
			{
				Cvar_SetValue (&_vid_default_blit_mode, 3);
			}
				
			gVidDefaultModeSet	= YES;
			myBlitterChanged	= YES;
		}
    }
    
    // if in test mode, check if finished:
    if (gVidTesting == YES)
    {
        if (cls.realtime >= gVidEndTestTime)
        {
            gVidTesting = NO;
            Cvar_SetValue (&vid_mode, gVidOldMode);
        }
    }
    
    // did the user request a new video mode?
    if (vid_mode.value != gVidCurMode)
    {
        S_StopAllSounds (YES);
        VID_SetMode (vid_mode.value, vid_current_palette);
    }
	
	if (myBlitterChanged)
	{
		VID_SetBlitter (_vid_default_blit_mode.value, VID_BUFFER_UPDATE);
	}
	
    if (cl_multiview.value && CURRVIEW == 1 || !cl_multiview.value) {
        // blit the rendered scene to the video card:
        gVidGraphMode.Blitter ();
    }
}

//_________________________________________________________________________________________________________D_BeginDirectRect()

void	D_BeginDirectRect (SInt x, SInt y, UInt8 *theBitmap, SInt theWidth, SInt theHeight)
{
    UInt8	*myScreen, *myScreen2, *myBackingBuffer;
    UInt16	myRowBytes, i, j;

    // just some security checks:
    if (gVidIsInited == NO ||
        ((theWidth > 24) || (theHeight > 24) || (theWidth < 1) || (theHeight < 1)) ||
        theWidth & 0x03 ||
        (myScreen = CGDisplayBaseAddress (gVidDisplayList[gVidDisplay])) == NULL)
    {
        return;
    }

    // get the rowbytes:
    myRowBytes = VID_GetRowBytes () - theWidth; 

    // get the buffer:
    myBackingBuffer = gVidBackingBuffer;

    // wait for the VBL:
    if (vid_wait.value != 0.0f)
    {
        CGDisplayWaitForBeamPositionOutsideLines (gVidDisplayList[gVidDisplay], y, y + theHeight);
    }

    switch (gVidGraphMode.BlitMode)
    {
        case VID_BLIT_1X1:
            // move to the requested position:
            myScreen += VID_GetRowBytes () * (((gVidGraphMode.Height - gVidGraphMode.OffHeight) >> 1) + y);
            myScreen += ((gVidGraphMode.Width - gVidGraphMode.OffWidth) >> 1) + x;
            
            // copy the rectangle to the buffer:
            for (i = 0; i < theHeight; i++)
            {
                for (j = 0; j < theWidth; j++)
                {
                    *myBackingBuffer++	= *myScreen;
                    *myScreen++			= *theBitmap++;
                }
                myScreen += myRowBytes;
            }
            break;
        case VID_BLIT_1X2:
            // move to the requested position:
            myScreen += VID_GetRowBytes () + y;
            myScreen += ((gVidGraphMode.Width - gVidGraphMode.OffWidth) >> 1) + x;

            // copy the rectangle to the buffer:
            for (i = 0; i < theHeight; i++)
            {
                for (j = 0; j < theWidth; j++)
                {
                    *myBackingBuffer++	= *myScreen;
                    *myScreen++			= *theBitmap++;
                }
                myScreen += myRowBytes;
            }
            break;
        case VID_BLIT_2X1:
            // move to the requested position:
            myScreen += VID_GetRowBytes () * (y << 1);
            myScreen += x << 1;
            
            myRowBytes = myRowBytes << 1;

            // copy the rectangle to the buffer:            
            for (i = 0; i < theHeight; i++)
            {
                for (j = 0; j < theWidth; j++)
                {
                    *myBackingBuffer++	= *myScreen;
                    *myScreen++			= *theBitmap;
                    *myScreen++			= *theBitmap++;
                }
                myScreen += myRowBytes;
            }
            break;
        case VID_BLIT_2X2:
            // move to the requested position:
            myScreen += VID_GetRowBytes () * (y << 1);
            myScreen += x << 1;
            myScreen2 = myScreen + VID_GetRowBytes ();
            
            myRowBytes = myRowBytes << 1;

            // copy the rectangle to the buffer:
            for (i = 0; i < theHeight; i++)
            {
                for (j = 0; j < theWidth; j++)
                {
                    *myBackingBuffer++	= *myScreen;
                    *myScreen++			= *theBitmap;
                    *myScreen++			= *theBitmap;
                    *myScreen2++		= *theBitmap;
                    *myScreen2++		= *theBitmap++;

                }
                myScreen += myRowBytes;
                myScreen2 += myRowBytes;
            }
            break;
        case VID_BLIT_WIN:
            break;
    }
}

//___________________________________________________________________________________________________________D_EndDirectRect()

void	D_EndDirectRect (SInt x, SInt y, SInt theWidth, SInt theHeight)
{
    UInt8	*myScreen, *myScreen2, *myBackingBuffer;
    UInt16	myRowBytes, i, j;
    
    // just some security checks:
    if (gVidIsInited == NO ||
        ((theWidth > 24) || (theHeight > 24) || (theWidth < 1) || (theHeight < 1)) ||
        theWidth & 0x03 ||
        (myScreen = CGDisplayBaseAddress (gVidDisplayList[gVidDisplay])) == NULL)
    {
        return;
    }

    // get the rowbytes:
    myRowBytes = VID_GetRowBytes () - theWidth;
    
    // get the buffer:
    myBackingBuffer = gVidBackingBuffer;
    
    // wait for the VBL:
    if (vid_wait.value)
    {
        CGDisplayWaitForBeamPositionOutsideLines (gVidDisplayList[gVidDisplay], y, y + theHeight);
    }
    
    switch (gVidGraphMode.BlitMode)
    {
        case VID_BLIT_1X1:
            // move to the requested position:
            myScreen += VID_GetRowBytes () * (((gVidGraphMode.Height - gVidGraphMode.OffHeight) >> 1) + y);
            myScreen += ((gVidGraphMode.Width - gVidGraphMode.OffWidth) >> 1) + x;
                
            // copy the buffer back to the display:
            for (i = 0; i < theHeight; i++)
            {
                for (j = 0; j < theWidth; j++)
                {
                    *myScreen++ = *myBackingBuffer++;
                }
                myScreen += myRowBytes;
            }
            break;
        case VID_BLIT_1X2:
            // move to the requested position:
            myScreen += VID_GetRowBytes() + y;
            myScreen += ((gVidGraphMode.Width - gVidGraphMode.OffWidth) >> 1) + x;

            // copy the buffer back to the display:
            for (i = 0; i < theHeight; i++)
            {
                for (j = 0; j < theWidth; j++)
                {
                    *myScreen++ = *myBackingBuffer++;
                }
                myScreen += myRowBytes;
            }
            break;
        case VID_BLIT_2X1:
            // move to the requested position:
            myScreen += VID_GetRowBytes () * (y << 1);
            myScreen += x << 1;
            
            myRowBytes = myRowBytes << 1; 

            // copy the buffer back to the display:
            for (i = 0; i < theHeight; i++)
            {
                for (j = 0; j < theWidth; j++)
                {
                    *myScreen++ = *myBackingBuffer;
                    *myScreen++ = *myBackingBuffer++;
                }
                myScreen += myRowBytes;
            }
            break;
        case VID_BLIT_2X2:
            // move to the requested position:
            myScreen += VID_GetRowBytes () * (y << 1);
            myScreen += x << 1;
            myScreen2 = myScreen + VID_GetRowBytes ();
            
            myRowBytes = myRowBytes << 1; 

            // copy the buffer back to the display:
            for (i = 0; i < theHeight; i++)
            {
                for (j = 0; j < theWidth; j++)
                {
                    *myScreen++ = *myBackingBuffer;
                    *myScreen++ = *myBackingBuffer;
                    *myScreen2++ = *myBackingBuffer;
                    *myScreen2++ = *myBackingBuffer++;
                }
                myScreen += myRowBytes;
                myScreen2 += myRowBytes;
            }
            break;
        case VID_BLIT_WIN:
            break;
    }
}

//_________________________________________________________________________________________________VID_DescribeCurrentMode_f()

void	VID_DescribeCurrentMode_f (void)
{
    // describe the current video mode:
    Com_Printf ("Current videomode: %s\n", gVidModeList[gVidCurMode].Desc);
    Com_Printf ("Current blitmode: %s\n", gVidBlitModeStr[gVidGraphMode.BlitMode]);
}

//________________________________________________________________________________________________________VID_DescribeMode_f()

void	VID_DescribeMode_f (void)
{
    // describe the requested video mode at the console:
    if (Q_atoi (Cmd_Argv (1)) >= 0 && Q_atoi (Cmd_Argv (1)) < gVidModeNums)
    {
        Com_Printf ("%s\n", gVidModeList[Q_atoi (Cmd_Argv (1))].Desc);
    }
    else
    {
        Com_Printf ("Invalid video mode.\n");
    }
}

//_______________________________________________________________________________________________________VID_DescribeModes_f()

void	VID_DescribeModes_f (void)
{
    UInt16	i;
    
    // list all available video modes to the console:
    for (i = 0; i < gVidModeNums; i++)
    {
        Com_Printf ("%2d: %s\n", i, gVidModeList[i].Desc);
    }
}

//___________________________________________________________________________________________________________VID_ForceMode_f()

void	VID_ForceMode_f (void)
{
    // switch to the selected video mode:
    VID_SetMode (Q_atoi (Cmd_Argv (1)), vid_current_palette);
}

//____________________________________________________________________________________________________________VID_NumModes_f()

void	VID_NumModes_f (void)
{
    // list the number of video modes:
    if (gVidModeNums == 1)
    {
        Com_Printf ("%d video mode is available\n", gVidModeNums);
    }
    else
    {
        Com_Printf ("%d video modes are available\n", gVidModeNums);
    }
}

//____________________________________________________________________________________________________________VID_TestMode_f()

void	VID_TestMode_f (void)
{
    if (gVidTesting == NO)
    {
        double		myTestDuration;

        if (Q_atoi (Cmd_Argv (1)) != gVidCurMode)
        {
            // set the test mode:
            gVidOldMode = gVidCurMode;
            Cvar_SetValue (&vid_mode, Q_atoi (Cmd_Argv (1)));
            gVidTesting = YES;
            
            // set the testtime to 5 seconds:
            myTestDuration = Q_atof (Cmd_Argv (2));
            if (myTestDuration == 0.0)
            {
                myTestDuration = 5.0;
            }
            gVidEndTestTime = cls.realtime + myTestDuration;
	}
    }
    else
    {
        // don't allow overlapping tests:
        Con_Print ("Please wait until the first test has finished!\n");
    }
}

//______________________________________________________________________________________________________________VID_MenuDraw()

void	VID_MenuDraw (void)
{
    qpic_t		*myPicture;
    int			myModeNums, myColumn, myRow, i;
    char		myTempStr[100];

    // draw video modes title:
    myPicture = Draw_CachePic ("gfx/vidmodes.lmp");
    M_DrawPic ((320 - myPicture->width) >> 1, 4, myPicture);
    M_Print (13 * VID_FONT_WIDTH, 5 * VID_FONT_HEIGHT, "Display Modes:");

    // limit the mode number:
    myRow = 7 * VID_FONT_HEIGHT;
    if (gVidModeNums > 15) myModeNums = 15;
        else myModeNums = gVidModeNums;
    
    // print the video modes:
    for (i=0 ; i < myModeNums; i++)
    {
        if (strlen (gVidModeList[i].Desc) <= 38)
        {
            myColumn = VID_FONT_WIDTH + ((38 - strlen(gVidModeList[i].Desc)) << 2);
            
            // draw highlighted, if active:
            if (i == gVidCurMode)
            {
                M_PrintWhite (myColumn, myRow, gVidModeList[i].Desc);
            }
            else
            {
                M_Print (myColumn, myRow, gVidModeList[i].Desc);
            }
        }
        else
        {
            snprintf (myTempStr, 100, "%.35s...", gVidModeList[i].Desc);

            // draw highlighted, if active:
            if (i == gVidCurMode)
            {
                M_PrintWhite (1 * VID_FONT_WIDTH, myRow, myTempStr);
            }
            else
            {
                M_Print (1 * VID_FONT_WIDTH, myRow, myTempStr);
            }
        }
        myRow += VID_FONT_HEIGHT;
    }

    if(gVidTesting == YES)
    {
        // skip keybindings, if testing a display mode:
        snprintf (myTempStr, 100, "TESTING %s", gVidModeList[gVidMenuLine].Desc);
        if (strlen (myTempStr) > 40)
        {
            snprintf (myTempStr, 100, "TESTING %.29s...", gVidModeList[gVidMenuLine].Desc);
        }
        M_Print ((40 - strlen(myTempStr)) << 2, 36 + 20 * VID_FONT_HEIGHT, myTempStr);
        M_Print (VID_FONT_WIDTH * 8, 36 + 22 * VID_FONT_HEIGHT, "Please wait 5 seconds...");
    }
    else
    {
        // print the blitmode keys:
        M_Print (16 * VID_FONT_WIDTH, 36 + 18 * VID_FONT_HEIGHT, "Blitmode:");
        
        // don't allow zoomed blitmodes for resolutions < 640x480 and windowed modes:
        if (gVidGraphMode.Width < 640 || gVidGraphMode.Height < 480 || gVidDisplayFullscreen == NO)
        {
            M_Print (1 * VID_FONT_WIDTH, 36 + 19 * VID_FONT_HEIGHT, "Resize the window to scale the display");
        }
        else
        {
            // blitmode keybindings:
            M_Print (1 * VID_FONT_WIDTH, 36 + 19 * VID_FONT_HEIGHT, "[1]     - [2]     - [3]     - [4]");

            // blit 1x1, draw highlighted if current:
            if (gVidGraphMode.BlitMode == VID_BLIT_1X1)
            {
                M_PrintWhite (5 * VID_FONT_WIDTH, 36 + 19 * VID_FONT_HEIGHT, "1x1");
            }
            else
            {
                M_Print (5 * VID_FONT_WIDTH, 36 + 19 * VID_FONT_HEIGHT, "1x1");
            }

            // blit 1x2, draw highlighted if current:
            if (gVidGraphMode.BlitMode == VID_BLIT_1X2)
            {
                M_PrintWhite (15 * VID_FONT_WIDTH, 36 + 19 * VID_FONT_HEIGHT, "1x2");
            }
            else
            {
                M_Print (15 * VID_FONT_WIDTH, 36 + 19 * VID_FONT_HEIGHT, "1x2");
            }

            // blit 2x1, draw highlighted if current:
            if (gVidGraphMode.BlitMode == VID_BLIT_2X1)
            {
                M_PrintWhite (25 * VID_FONT_WIDTH, 36 + 19 * VID_FONT_HEIGHT, "2x1");
            }
            else
            {
                M_Print (25 * VID_FONT_WIDTH, 36 + 19 * VID_FONT_HEIGHT, "2x1");
            }

            // blit 2x2, draw highlighted if current:
            if (gVidGraphMode.BlitMode == VID_BLIT_2X2)
            {
                M_PrintWhite (35 * VID_FONT_WIDTH, 36 + 19 * VID_FONT_HEIGHT, "2x2");
            }
            else
            {
                M_Print (35 * VID_FONT_WIDTH, 36 + 19 * VID_FONT_HEIGHT, "2x2");
            }
        }
        
        // show other key bindings:
        M_Print (8 * VID_FONT_WIDTH, 36 + 20 * VID_FONT_HEIGHT, "Press [Enter] to set mode");
        M_Print (5 * VID_FONT_WIDTH, 36 + 21 * VID_FONT_HEIGHT, "[T] to test mode for 5 seconds");
        M_Print (14 * VID_FONT_WIDTH, 36 + 24 * VID_FONT_HEIGHT, "[Esc] to exit");
                
        // set the current resolution to default:
        snprintf (myTempStr, 100,
                  "[D] to set default: %s %s",
                  gVidModeList[gVidCurMode].Desc,
                  gVidBlitModeStr[gVidGraphMode.BlitMode]);
        if (strlen (myTempStr) > 40)
        {
            snprintf (myTempStr, 100,
                      "[D] to set default: %.13s... %s",
                      gVidModeList[gVidCurMode].Desc,
                      gVidBlitModeStr[gVidGraphMode.BlitMode]);
        }
        M_Print ((40 - strlen(myTempStr)) << 2, 36 + 22 * VID_FONT_HEIGHT, myTempStr);
        
        // current default resolution:
        snprintf (myTempStr, 100,
                  "Current default: %s %s",
                  gVidModeList[(int) _vid_default_mode.value].Desc,
                  gVidBlitModeStr[(int) _vid_default_blit_mode.value]);
        if (strlen (myTempStr) > 40)
        {
            snprintf (myTempStr, 100,
                      "Current default: %.16s... %s",
                      gVidModeList[(int) _vid_default_mode.value].Desc,
                      gVidBlitModeStr[(int) _vid_default_blit_mode.value]);
        }
        M_Print ((40 - strlen(myTempStr)) << 2, 36 + 23 * VID_FONT_HEIGHT, myTempStr);
        
        // draw the cursor for the current menu line:
        myRow = (gVidMenuLine + 7) << 3;
        if (strlen (gVidModeList[gVidMenuLine].Desc) < 38)
        {
            myColumn = (38 - strlen(gVidModeList[gVidMenuLine].Desc)) << 2;
        }
        else
        {
            myColumn = 0;
        }
        M_DrawCharacter (myColumn, myRow, 12 + ((int)(cls.realtime * 4) & 1));
    }
}

//_______________________________________________________________________________________________________________VID_MenuKey()

void VID_MenuKey (int theKey)
{
    // do not check menu keys while testing a display mode:
    if (gVidTesting == YES)
    {
        return;
    }
    
    // check keys:
    switch (theKey)
    {
	case K_ESCAPE:
            S_LocalSound ("misc/menu1.wav");
            M_Menu_Options_f ();
            break;
	case K_UPARROW:
            S_LocalSound ("misc/menu1.wav");
            gVidMenuLine -= 1;
            if (gVidMenuLine < 0)
            {
                if (gVidModeNums > 15)
                    gVidMenuLine = 14;
                else
                {
                    gVidMenuLine = gVidModeNums - 1;
                }
            }
            break;
	case K_DOWNARROW:
            S_LocalSound ("misc/menu1.wav");
            gVidMenuLine += 1;
            if(gVidModeNums > 15)
            {
                if (gVidMenuLine >= 15)
                    gVidMenuLine = 0;
            }
            else
            {
                if (gVidMenuLine >= gVidModeNums)
                    gVidMenuLine = 0;
            }
            break;
	case K_ENTER:
            S_LocalSound ("misc/menu1.wav");
            Cvar_SetValue (&vid_mode, gVidMenuLine);
            break;
	case 'T':
	case 't':
            S_LocalSound ("misc/menu1.wav");
            if (gVidMenuLine != gVidCurMode)
            {
                gVidTesting = YES;
                gVidEndTestTime = cls.realtime + 5.0;
                gVidOldMode = gVidCurMode;
                Cvar_SetValue (&vid_mode, gVidMenuLine);
            }
            break;
	case 'D':
	case 'd':
            S_LocalSound ("misc/menu1.wav");
            gVidDefaultModeSet = YES;
            Cvar_SetValue (&_vid_default_mode, (float) gVidCurMode);
            Cvar_SetValue (&_vid_default_blit_mode, (float) gVidGraphMode.BlitMode);
            break;
        case '1':
            S_LocalSound ("misc/menu1.wav");
            VID_SetBlitter (VID_BLIT_1X1, VID_BUFFER_UPDATE);
            break;
        case '2':
            S_LocalSound ("misc/menu1.wav");
            VID_SetBlitter (VID_BLIT_1X2, VID_BUFFER_UPDATE);
            break;
        case '3':
            S_LocalSound ("misc/menu1.wav");
            VID_SetBlitter (VID_BLIT_2X1, VID_BUFFER_UPDATE);
            break;
        case '4':
            S_LocalSound ("misc/menu1.wav");
            VID_SetBlitter (VID_BLIT_2X2, VID_BUFFER_UPDATE);
            break;
	default:
            break;
    }
}

void VID_SetDeviceGammaRamp (unsigned short *ramps) {}
void VID_SetCaption (char *text) {VID_SetWindowTitle(text);}

int isAltDown(void)
{
    return 0;
}
int isCtrlDown(void)
{
    return 0;
}
int isShiftDown(void)
{
    return 0;
}

// QW262 -->
/*
===================
VID_Console functions map "window" coordinates into "console".
===================
*/
int VID_ConsoleX (int x)
{
	return (x);
}

int VID_ConsoleY (int y)
{
	return (y);
}
// <-- QW262

//_________________________________________________________________________________________________________________________eOF
