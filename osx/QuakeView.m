//_________________________________________________________________________________________________________________________nFO
// "QuakeView.m" - required to control window resizing, minimizing...
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2006 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
// Quakeª is copyrighted by id software		[http://www.idsoftware.com].
//
//____________________________________________________________________________________________________________________iNCLUDES

#import "QuakeView.h"
#import "quakedef.h"
#import "gl_local.h"
#import "vid_osx.h"

//_________________________________________________________________________________________________________fUNCTION_pROTOTYPES

extern void	M_Menu_Quit_f (void);

//______________________________________________________________________________________________________________iMPLEMENTATION

@implementation QuakeView

//__________________________________________________________________________________________________________________setBuffer:

- (void) setBuffer: (NSBitmapImageRep *) theBuffer
{
    mBitmapBuffer = [theBuffer retain];
}

//______________________________________________________________________________________________________________________buffer

- (NSBitmapImageRep *) buffer
{
    return (mBitmapBuffer);
}

@end

//_________________________________________________________________________________________________iNTERFACE_(DelegateMethods)

@interface QuakeView (DelegateMethods)
@end

//____________________________________________________________________________________________iMPLEMENTATION_(DelegateMethods)

@implementation QuakeView (DelegateMethods)

//_____________________________________________________________________________________________________________________dealloc

- (void) dealloc
{
    [mBitmapBuffer release];
    [super dealloc];
}

//_______________________________________________________________________________________________________acceptsFirstResponder

- (BOOL) acceptsFirstResponder
{
    return (YES);
}

//______________________________________________________________________________________________________________windowDidMove:

- (void) windowDidMove: (NSNotification *) theNotification
{
    NSRect	myRect;

    myRect = [[self window] frame];
    gVidWindowPosX = myRect.origin.x + 1.0f;
    gVidWindowPosY = myRect.origin.y + 1.0f;
}

//______________________________________________________________________________________________________windowWillMiniaturize:

#if defined (GLQUAKE)

- (void) windowWillMiniaturize: (NSNotification *) theNotification
{
    GL_SetMiniWindowBuffer ();
}

#endif /* GLQUAKE */

//_______________________________________________________________________________________________________windowDidMiniaturize:

- (void) windowDidMiniaturize: (NSNotification *) theNotification
{
    gVidIsMinimized = YES;
}

//_____________________________________________________________________________________________________windowDidDeminiaturize:

- (void) windowDidDeminiaturize: (NSNotification *) theNotification
{
    gVidIsMinimized = NO;
}

//____________________________________________________________________________________________________windowWillResize:toSize:

- (NSSize) windowWillResize: (NSWindow *) theSender toSize: (NSSize) theProposedFrameSize
{
    NSRect	myMaxWindowRect	= [[theSender screen] visibleFrame],
			myContentRect	= [[theSender contentView] frame],
			myWindowRect	= [theSender frame];
    NSSize	myMinSize		= [theSender minSize],
			myBorderSize;
    float	myAspect		= myMinSize.width / myMinSize.height;

    // calculate window borders (e.g. titlebar):
    myBorderSize.width	= NSWidth (myWindowRect)  - NSWidth (myContentRect);
    myBorderSize.height	= NSHeight (myWindowRect) - NSHeight (myContentRect);
    
    // remove window borders (like titlebar) for the aspect calculations:
    myMaxWindowRect.size.width	-= myBorderSize.width;
    myMaxWindowRect.size.height	-= myBorderSize.height;
    theProposedFrameSize.width	-= myBorderSize.width;
    theProposedFrameSize.height	-= myBorderSize.height;
    
    // set aspect ratio for the max rectangle:
    if (NSWidth (myMaxWindowRect) / NSHeight (myMaxWindowRect) > myAspect)
    {
        myMaxWindowRect.size.width = NSHeight (myMaxWindowRect) * myAspect;
    }
    else
    {
        myMaxWindowRect.size.height = NSWidth (myMaxWindowRect) / myAspect;
    }

    // set the aspect ratio for the proposed size:
    if (theProposedFrameSize.width / theProposedFrameSize.height > myAspect)
    {
        theProposedFrameSize.width = theProposedFrameSize.height * myAspect;
    }
    else
    {
        theProposedFrameSize.height = theProposedFrameSize.width / myAspect;
    }

    // clamp the window size to our max window rectangle:
    if (theProposedFrameSize.width > NSWidth (myMaxWindowRect) || theProposedFrameSize.height > NSHeight (myMaxWindowRect))
    {
        theProposedFrameSize = myMaxWindowRect.size;
    }

    if (theProposedFrameSize.width < myMinSize.width || theProposedFrameSize.height < myMinSize.height)
    {
        theProposedFrameSize = myMinSize;
    }

    theProposedFrameSize.width += myBorderSize.width;
    theProposedFrameSize.height += myBorderSize.height;

    return (theProposedFrameSize);
}

//____________________________________________________________________________________windowWillUseStandardFrame:defaultFrame:

- (NSRect) windowWillUseStandardFrame: (NSWindow *) theSender defaultFrame: (NSRect) theDefaultFrame
{
	theDefaultFrame.size = [self windowWillResize: theSender toSize: theDefaultFrame.size];
	
	return theDefaultFrame;
}

//__________________________________________________________________________________________________________windowShouldClose:

- (BOOL) windowShouldClose: (id) theSender
{
    BOOL	myResult = ![[self window] isDocumentEdited];

    if (myResult == NO)
    {
        M_Menu_Quit_f ();
    }
    return (myResult);
}

//___________________________________________________________________________________________________________________drawRect:

- (void) drawRect: (NSRect) theRect
{
    // required for resizing and deminiaturizing:
#if !defined (GLQUAKE)

    if (mBitmapBuffer != NULL)
    {
        [mBitmapBuffer drawInRect: [self bounds]];
    }

#else

    extern NSOpenGLContext *	gGLContext;
    extern UInt32				gGLDisplayWidth;
	extern UInt32				gGLDisplayHeight;
    NSRect						myWindowRect = [self frame];

    if (gGLContext != NULL)
    {
        gGLDisplayWidth		= glwidth	= vid.width		= vid.conwidth	= NSWidth (myWindowRect);
        gGLDisplayHeight	= glheight	= vid.height	= vid.conheight	= NSHeight (myWindowRect);
		
        [gGLContext update];
	
        vid.recalc_refdef = 1;

		// [[NSApp delegate] renderFrame: NULL] might not update at all - because of a too small time intervall.
		// Dirty solution: Force two frames to avoid garbled pixels in the window!

		Host_Frame (0.02f);
		Host_Frame (0.02f);
    }

#endif /* !GLQUAKE */
}

@end

//_________________________________________________________________________________________________________________________eOF
