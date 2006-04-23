//_________________________________________________________________________________________________________________________nFO
// "vid_shared_osx.m" - MacOS X Video driver stuff used by the OpenGL and software renderer versions.
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2006 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
// Quakeª is copyrighted by id software		[http://www.idsoftware.com].
//
//____________________________________________________________________________________________________________________iNCLUDES

#pragma mark =Includes=

#import <Cocoa/Cocoa.h>
#import <ApplicationServices/ApplicationServices.h>

#import "quakedef.h"
#import "vid_osx.h"
#import "sys_osx.h"

#pragma mark -

//___________________________________________________________________________________________________________________vARIABLES

#pragma mark =Variables=

#if defined (GLQUAKE)
vid_gammatable_t *			gVshGammaTable = NULL;
#endif /* GLQUAKE */
vid_gamma_t *				gVshOriginalGamma = NULL;

static CGDirectDisplayID	gVshCapturedDisplayList[VID_MAX_DISPLAYS];
static CGDisplayCount		gVshCapturedDisplayCount = 0;

#pragma mark -

//_________________________________________________________________________________________________________fUNCTION_pROTOTYPES

#pragma mark =Function Prototypes=

static BOOL	VSH_FadeGammaInit (BOOL theFadeOnAllDisplays);

#pragma mark -

//____________________________________________________________________________________________VSH_DisableQuartzInterpolation()

void	VSH_DisableQuartzInterpolation (id theView)
{
    NSGraphicsContext *	myGraphicsContext;
    
    [theView lockFocus];
    myGraphicsContext = [NSGraphicsContext currentContext];
    [myGraphicsContext setImageInterpolation: NSImageInterpolationNone];
    [myGraphicsContext setShouldAntialias: NO];
    [theView unlockFocus];    
}

//_______________________________________________________________________________________________________VSH_CaptureDisplays()

BOOL	VSH_CaptureDisplays (BOOL theCaptureAllDisplays)
{
    CGDisplayErr	myError;
    SInt16		i;
    
    if (theCaptureAllDisplays == NO)
    {
        if (CGDisplayIsCaptured (gVidDisplayList[gVidDisplay]) == NO)
        {
            myError = CGDisplayCapture (gVidDisplayList[gVidDisplay]);
            
			if (myError != CGDisplayNoErr)
            {
                return (NO);
            }
        }
        return (YES);
    }

    // are the displays already captured?
    if (gVshCapturedDisplayCount != 0)
    {
        return (YES);
    }

    // we have to loop manually thru each display, since there is a bug with CGReleaseAllDisplays().
    // [only active displays will be released and not all captured!]

    // get the active display list:
    myError = CGGetActiveDisplayList (VID_MAX_DISPLAYS, gVshCapturedDisplayList, &gVshCapturedDisplayCount);
    if (myError != CGDisplayNoErr || gVshCapturedDisplayCount == 0)
    {
        gVshCapturedDisplayCount = 0;

        return (NO);
    }
    
    // capture each active display:
    for (i = 0; i < gVshCapturedDisplayCount; i++)
    {
        myError = CGDisplayCapture (gVshCapturedDisplayList[i]);
        
        if (myError != CGDisplayNoErr)
        {
            for (; i >= 0; i--)
            {
                CGDisplayRelease (gVshCapturedDisplayList[i]);
            }

            gVshCapturedDisplayCount = 0;

            return (NO);
        }
    }
    
    return (YES);
}

//_______________________________________________________________________________________________________VSH_ReleaseDisplays()

BOOL	VSH_ReleaseDisplays (BOOL theCaptureAllDisplays)
{
    CGDisplayErr	myError;
    SInt16			i;
    
    if (theCaptureAllDisplays == NO)
    {
        if (CGDisplayIsCaptured (gVidDisplayList[gVidDisplay]) == YES)
        {
            myError = CGDisplayRelease (gVidDisplayList[gVidDisplay]);
            if (myError != CGDisplayNoErr)
            {
                return (NO);
            }
        }
        return (YES);
    }

    // are the displays already released?
    if (gVshCapturedDisplayCount == 0)
    {
        return (YES);
    }
    
    // release each captured display:
    for (i = 0; i < gVshCapturedDisplayCount; i++)
    {
        CGDisplayRelease (gVshCapturedDisplayList[i]);
    }
    
    gVshCapturedDisplayCount = 0;
    return (YES);
}

//___________________________________________________________________________________________________VSH_SortDisplayModesCbk()

int	VSH_SortDisplayModesCbk(id pMode1, id pMode2, void* pContext)
{
	// used to sort display modes by pixels/refresh

	UInt64	width1		= [[pMode1 objectForKey: (NSString *) kCGDisplayWidth] intValue];
	UInt64	height1		= [[pMode1 objectForKey: (NSString *) kCGDisplayHeight] intValue];
	UInt64	refresh1	= [[pMode1 objectForKey: (NSString *) kCGDisplayRefreshRate] intValue];

	UInt64	width2		= [[pMode2 objectForKey: (NSString *) kCGDisplayWidth] intValue];
	UInt64	height2		= [[pMode2 objectForKey: (NSString *) kCGDisplayHeight] intValue];
	UInt64	refresh2	= [[pMode2 objectForKey: (NSString *) kCGDisplayRefreshRate] intValue];
		
	UInt64	pixels1		= width1 * height1;
	UInt64	pixels2		= width2 * height2;
	int		result		= NSOrderedDescending;
	
	if ((pixels1 < pixels2) || (pixels1 == pixels2 && refresh1 < refresh2))
	{
		result = NSOrderedAscending;
	}

	return result;
}

//_________________________________________________________________________________________________________VSH_FadeGammaInit()

BOOL	VSH_FadeGammaInit (BOOL theFadeOnAllDisplays)
{
    static BOOL		myFadeOnAllDisplays;
    CGDisplayErr	myError;
    UInt32			i;

    // if init fails, no gamma fading will be used!    
    if (gVshOriginalGamma != NULL)
    {
        // initialized, but did we change the number of displays?
        if (theFadeOnAllDisplays == myFadeOnAllDisplays)
        {
            return (YES);
        }
        free (gVshOriginalGamma);
        gVshOriginalGamma = NULL;
    }

    // get the list of displays, in case something bad is going on:
    if (gVidDisplayList == NULL || gVidDisplayCount == 0)
    {
        return (NO);
    }
    
    if (gVidDisplay > gVidDisplayCount)
    {
        gVidDisplay = 0;
    }
    
    if (theFadeOnAllDisplays == NO)
    {
        gVidDisplayList[0] = gVidDisplayList[gVidDisplay];
        gVidDisplayCount = 1;
        gVidDisplay = 0;
    }
    
    // get memory for our original gamma table(s):
    gVshOriginalGamma = malloc (sizeof (vid_gamma_t) * gVidDisplayCount);
    if (gVshOriginalGamma == NULL)
    {
        return (NO);
    }
    
    // store the original gamma values within this table(s):
    for (i = 0; i < gVidDisplayCount; i++)
    {
        if (gVidDisplayCount == 1)
        {
            gVshOriginalGamma[i].displayID = gVidDisplayList[gVidDisplay];
        }
        else
        {
            gVshOriginalGamma[i].displayID = gVidDisplayList[i];
        }
        
        myError = CGGetDisplayTransferByFormula (gVshOriginalGamma[i].displayID,
                                                 &gVshOriginalGamma[i].component[0],
                                                 &gVshOriginalGamma[i].component[1],
                                                 &gVshOriginalGamma[i].component[2],
                                                 &gVshOriginalGamma[i].component[3],
                                                 &gVshOriginalGamma[i].component[4],
                                                 &gVshOriginalGamma[i].component[5],
                                                 &gVshOriginalGamma[i].component[6],
                                                 &gVshOriginalGamma[i].component[7],
                                                 &gVshOriginalGamma[i].component[8]);
        
		// XXX: error checking doesn't seem to work on Intel? always seem to get kCGErrorNoneAvailable
		/*
		if (myError != CGDisplayNoErr)
        {
			Com_Printf("GAMMA FAILED %d\n", myError);
            free (gVshOriginalGamma);
            gVshOriginalGamma = NULL;
            return (NO);
        }
		 */
    }

#if defined (GLQUAKE)
    // just in case...
    if (gVshGammaTable != NULL)
    {
        free (gVshGammaTable);
        gVshGammaTable = NULL;
    }

    // required for our second gamma method:
    gVshGammaTable = (vid_gammatable_t *) malloc (sizeof (vid_gammatable_t));
    if (gVshGammaTable != NULL)
    {
        myError = CGGetDisplayTransferByTable (gVidDisplayList[gVidDisplay], VID_GAMMA_TABLE_SIZE,
                                               gVshGammaTable->red, gVshGammaTable->green, gVshGammaTable->blue,
                                               &gVshGammaTable->count);
        if (myError != CGDisplayNoErr)
        {
            free (gVshGammaTable);
            gVshGammaTable = NULL;
        }
    }
#endif /* GLQUAKE */
    
    myFadeOnAllDisplays = theFadeOnAllDisplays;

    return (YES);
}

//______________________________________________________________________________________________________VSH_FadeGammaRelease()

void	VSH_FadeGammaRelease (void)
{
    if (gVshOriginalGamma != NULL)
    {
        free (gVshOriginalGamma);
        gVshOriginalGamma = NULL;
    }

#if defined (GLQUAKE)    
    if (gVshGammaTable != NULL)
    {
        free (gVshGammaTable);
        gVshGammaTable = NULL;
    }
#endif /* GLQUAKE */
}

//__________________________________________________________________________________________________________VSH_FadeGammaOut()

void	VSH_FadeGammaOut (BOOL theFadeOnAllDisplays, float theDuration)
{
    vid_gamma_t		myCurGamma;
    UInt32			i, j;
    float			myStartTime = 0.0f,
					myCurScale = 0.0f;

    // check if initialized:
    if (VSH_FadeGammaInit (theFadeOnAllDisplays) == NO)
    {
		Com_Printf("Failed to fade!\n");
        return;
    }
    
    // get the time of the fade start:
    myStartTime = Sys_FloatTime ();
    
    // fade for the choosen duration:
    while (1)
    {
        // calculate the current scale and clamp it:
        if (theDuration > 0.0f)
        {
            myCurScale = 1.0f - (Sys_FloatTime () - myStartTime) / theDuration;
            if (myCurScale < 0.0f)
            {
                myCurScale = 0.0f;
            }
        }

        // fade the gamma for each display:        
        for (i = 0; i < gVidDisplayCount; i++)
        {
            // calculate the current intensity for each color component:
            for (j = 1; j < 9; j += 3)
            {
                myCurGamma.component[j] = myCurScale * gVshOriginalGamma[i].component[j];
            }

            // set the current gamma:
            CGSetDisplayTransferByFormula (gVshOriginalGamma[i].displayID,
                                           gVshOriginalGamma[i].component[0],
                                           myCurGamma.component[1],
                                           gVshOriginalGamma[i].component[2],
                                           gVshOriginalGamma[i].component[3],
                                           myCurGamma.component[4],
                                           gVshOriginalGamma[i].component[5],
                                           gVshOriginalGamma[i].component[6],
                                           myCurGamma.component[7],
                                           gVshOriginalGamma[i].component[8]);
        }
        
        // are we finished?
        if(myCurScale <= 0.0f)
        {
            break;
        } 
    }
}

//___________________________________________________________________________________________________________VSH_FadeGammaIn()

void	VSH_FadeGammaIn (BOOL theFadeOnAllDisplays, float theDuration)
{
    vid_gamma_t		myCurGamma;
    float			myStartTime = 0.0f, myCurScale = 1.0f;
    UInt32			i, j;

    // check if initialized:
    if (gVshOriginalGamma == NULL)
    {
        return;
    }
    
    // get the time of the fade start:
    myStartTime = Sys_FloatTime ();
    
    // fade for the choosen duration:
    while (1)
    {
        // calculate the current scale and clamp it:
        if (theDuration > 0.0f)
        {
            myCurScale = (Sys_FloatTime () - myStartTime) / theDuration;
            if (myCurScale > 1.0f)
            {
                myCurScale = 1.0f;
            }
        }

        // fade the gamma for each display:
        for (i = 0; i < gVidDisplayCount; i++)
        {
            // calculate the current intensity for each gamma component:
            for (j = 1; j < 9; j += 3)
            {
                myCurGamma.component[j] = myCurScale * gVshOriginalGamma[i].component[j];
            }

            // set the current gamma:
            CGSetDisplayTransferByFormula (gVshOriginalGamma[i].displayID,
                                           gVshOriginalGamma[i].component[0],
                                           myCurGamma.component[1],
                                           gVshOriginalGamma[i].component[2],
                                           gVshOriginalGamma[i].component[3],
                                           myCurGamma.component[4],
                                           gVshOriginalGamma[i].component[5],
                                           gVshOriginalGamma[i].component[6],
                                           myCurGamma.component[7],
                                           gVshOriginalGamma[i].component[8]);
        }
        
        // are we finished?
        if(myCurScale >= 1.0f)
        {
            break;
        } 
    }
}

//_________________________________________________________________________________________________________________________eOF
