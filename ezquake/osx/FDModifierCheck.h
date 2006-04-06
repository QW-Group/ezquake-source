//_________________________________________________________________________________________________________________________nFO
// "FDModifierCheck.h" - Allows one to check if the specified modifier key is pressed (usually at application launch time).
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2006 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
//
// IMPORTANT: THIS PIECE OF CODE MAY NOT BE USED WITH SHAREWARE OR COMMERCIAL APPLICATIONS WITHOUT PERMISSION.
//	          IT IS PROVIDED "AS IS" AND IS STILL COPYRIGHTED BY FRUITZ OF DOJO.
//
//____________________________________________________________________________________________________________________iNCLUDES

#import <Foundation/Foundation.h>

//___________________________________________________________________________________________________________________iNTERFACE

@interface FDModifierCheck : NSObject
{
}

+ (BOOL) checkForModifier: (UInt32) theModifierKeyMask;

+ (BOOL) checkForAlternateKey;
+ (BOOL) checkForCommandKey;
+ (BOOL) checkForControlKey;
+ (BOOL) checkForOptionKey;

@end

//_________________________________________________________________________________________________________________________eOF
