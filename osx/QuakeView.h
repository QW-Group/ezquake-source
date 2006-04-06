//_________________________________________________________________________________________________________________________nFO
// "QuakeView.h" - required to control window resizing, minimizing...
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2006 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
// Quakeª is copyrighted by id software		[http://www.idsoftware.com].
//
//____________________________________________________________________________________________________________________iNCLUDES

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

//___________________________________________________________________________________________________________________iNTERFACE

@interface QuakeView : NSView
{
@private
    NSBitmapImageRep *	mBitmapBuffer;
}

- (void) setBuffer: (NSBitmapImageRep *) theBuffer;
- (NSBitmapImageRep *) buffer;

@end

//_________________________________________________________________________________________________________________________eOF
