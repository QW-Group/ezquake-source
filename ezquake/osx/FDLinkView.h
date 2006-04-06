//_________________________________________________________________________________________________________________________nFO
// "FDLinkView.h" - Provides an URL style link button.
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2006 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
//
// IMPORTANT: THIS PIECE OF CODE MAY NOT BE USED WITH SHAREWARE OR COMMERCIAL APPLICATIONS WITHOUT PERMISSION.
//	          IT IS PROVIDED "AS IS" AND IS STILL COPYRIGHTED BY FRUITZ OF DOJO.
//
//___________________________________________________________________________________________________________________iNTERFACE

@interface FDLinkView : NSView
{
@private
    NSCursor *		mHandCursor;
    NSString *		mURLString;
    NSDictionary * 	mFontAttributesBlue;
    NSDictionary * 	mFontAttributesRed;
    BOOL			mMouseIsDown;
}

- (void) setURLString: (NSString *) theURL;

@end

//_________________________________________________________________________________________________________________________eOF
