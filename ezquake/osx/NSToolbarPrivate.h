//_________________________________________________________________________________________________________________________nFO
// "NSToolbarPrivate.h" - required for getting the height of the startup dialog's toolbar.
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2006 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
// Quakeª is copyrighted by id software		[http://www.idsoftware.com].
//
//____________________________________________________________________________________________________________________iNCLUDES

#pragma mark =Includes=

#import <Cocoa/Cocoa.h>

#pragma mark -

//______________________________________________________________________________________iNTERFACE_NSToolbar_(NSToolbarPrivate)

@interface NSToolbar (NSToolbarPrivate)
- (NSView *) _toolbarView;
@end

//_________________________________________________________________________________________________________________________eOF
