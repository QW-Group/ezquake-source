//_________________________________________________________________________________________________________________________nFO
// "FDLinkView.m" - Provides an URL style link button.
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2006 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
//
// IMPORTANT: THIS PIECE OF CODE MAY NOT BE USED WITH SHAREWARE OR COMMERCIAL APPLICATIONS WITHOUT PERMISSION.
//	          IT IS PROVIDED "AS IS" AND IS STILL COPYRIGHTED BY FRUITZ OF DOJO.
//
//____________________________________________________________________________________________________________________iNCLUDES

#pragma mark ¥Includes

#import <Cocoa/Cocoa.h>
#import "FDLinkView.h"

#pragma mark -

//_________________________________________________________________________________________________________iNTERFACE (pRIVATE)

#pragma mark ¥Private

@interface FDLinkView (private)

- (void) initHandCursor;
- (void) initFontAttributes;
- (NSDictionary *) fontAttributesWithColor: (NSColor *) theColor;
- (void) openURL;

@end

#pragma mark -

//______________________________________________________________________________________________________________iMPLEMENTATION

@implementation FDLinkView

//_______________________________________________________________________________________________________________initWithFrame

- (id) initWithFrame: (NSRect) theFrame
{
    self = [super initWithFrame: theFrame];
	
    if (self == nil)
    {
        return (nil);
    }
    
    [self initHandCursor];
    [self initFontAttributes];

    return (self);
}

//_____________________________________________________________________________________________________________________dealloc

- (void) dealloc
{
    if (mHandCursor != nil)
    {
        [mHandCursor release];
        mHandCursor = NULL;
    }
	
    if (mURLString != nil)
    {
        [mURLString release];
        mURLString = nil;
    }
	
    if (mFontAttributesRed != nil)
    {
        [mFontAttributesRed release];
        mFontAttributesRed = nil;
    }
	
    if (mFontAttributesBlue != nil)
    {
        [mFontAttributesBlue release];
        mFontAttributesBlue = nil;
    }
    
    [super dealloc];
}

//_______________________________________________________________________________________________________________setURLString:

- (void) setURLString: (NSString *) theURL
{
    if (mURLString != nil)
    {
        [mURLString release];
        mURLString = nil;
    }
    
    if (theURL != nil)
    {
        mURLString = [[NSString stringWithString: theURL] retain];
    }

    [self setNeedsDisplay: YES];
}

//___________________________________________________________________________________________________________________drawRect:

- (void) drawRect: (NSRect) theRect
{
    if (mURLString == nil)
    {
        return;
    }

    // draw the text:
    if (mMouseIsDown == YES)
    {
        [mURLString drawAtPoint: NSMakePoint (0.0, 0.0) withAttributes: mFontAttributesRed];
    }
    else
    {
        [mURLString drawAtPoint: NSMakePoint (0.0, 0.0) withAttributes: mFontAttributesBlue];
    }
}

//__________________________________________________________________________________________________________________mouseDown:

- (void) mouseDown: (NSEvent *) theEvent;
{
    NSEvent *	myNextEvent;
    NSPoint 	myLocation;

    if (mURLString == nil)
    {
        return;
    }
    
    mMouseIsDown = YES;
    [self setNeedsDisplay:YES];

    myNextEvent = [NSApp nextEventMatchingMask: NSLeftMouseUpMask
                                        untilDate: [NSDate distantFuture]
                                        inMode: NSEventTrackingRunLoopMode
                                        dequeue: YES];
    myLocation = [self convertPoint: [myNextEvent locationInWindow] fromView: nil];
    if (NSMouseInRect (myLocation, [self bounds], NO))
    {
        [self openURL];
    }
    
    mMouseIsDown = NO;
    [self setNeedsDisplay:YES];
}

//_____________________________________________________________________________________________________________resetCursrRects

- (void) resetCursorRects
{
    if (mHandCursor != NULL && mURLString != NULL)
    {
        [self addCursorRect: [self bounds] cursor: mHandCursor];
    }
}

@end

#pragma mark -

//____________________________________________________________________________________________________iMPLEMENTATION (pRIVATE)

@implementation FDLinkView (private)

//______________________________________________________________________________________________________________initHandCursor

- (void) initHandCursor
{
    mHandCursor = [[NSCursor alloc] initWithImage: [NSImage imageNamed: @"HandCursor"]
                                          hotSpot: NSMakePoint (5.0, 1.0)];
    if (mHandCursor != NULL)
    {
        [self addCursorRect: [self bounds] cursor: mHandCursor];
        [mHandCursor setOnMouseEntered: YES];
    }
}

//__________________________________________________________________________________________________________initFontAttributes

- (void) initFontAttributes
{
    mFontAttributesRed = [self fontAttributesWithColor: [NSColor redColor]];
    mFontAttributesBlue = [self fontAttributesWithColor: [NSColor blueColor]];
}

//____________________________________________________________________________________________________fontAttributesWithColor:

- (NSDictionary *) fontAttributesWithColor: (NSColor *) theColor
{
    return ([[NSDictionary alloc] initWithObjects: [NSArray arrayWithObjects:
                                                        [NSFont systemFontOfSize: [NSFont systemFontSize]],
                                                        theColor,
                                                        [NSNumber numberWithInt: NSSingleUnderlineStyle],
                                                        nil
                                                   ]
                                          forKeys: [NSArray arrayWithObjects:
                                                        NSFontAttributeName,
                                                        NSForegroundColorAttributeName,
                                                        NSUnderlineStyleAttributeName,
                                                        nil
                                                   ]
            ]);
}

//_____________________________________________________________________________________________________________________openURL

- (void) openURL
{
    if (mURLString != NULL)
    {
        NSWorkspace	*myWorkspace;

        myWorkspace = [NSWorkspace sharedWorkspace];
		
        if (myWorkspace != NULL)
        {
            NSURL		*myURL;
    
            myURL = [NSURL URLWithString: mURLString];
			
            if (myURL != NULL)
            {
                [myWorkspace openURL: myURL];
            }
        }
    }
}

@end

//_________________________________________________________________________________________________________________________eOF
