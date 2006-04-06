//_________________________________________________________________________________________________________________________nFO
// "FDModifierCheck.m" - Allows one to check if the specified modifier key is pressed (usually at application launch time).
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2006 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
//
// IMPORTANT: THIS PIECE OF CODE MAY NOT BE USED WITH SHAREWARE OR COMMERCIAL APPLICATIONS WITHOUT PERMISSION.
//	          IT IS PROVIDED "AS IS" AND IS STILL COPYRIGHTED BY FRUITZ OF DOJO.
//
//____________________________________________________________________________________________________________________iNCLUDES

#import <AppKit/AppKit.h>
#import "FDModifierCheck.h"

//______________________________________________________________________________________________________________iMPLEMEMTATION

@implementation FDModifierCheck

//___________________________________________________________________________________________________________checkForModifier:

+ (BOOL) checkForModifier: (UInt32) theModifierKeyMask
{
    NSDate *			myDistantPast = [NSDate distantPast];
    NSEvent *			myEvent;
    NSAutoreleasePool *	myPool;
    BOOL				myModifierWasPressed = NO;    

    // raise shift down/up events [from somewhere deep inside CoreGraphics]:
    CGPostKeyboardEvent ((CGCharCode) 0, (CGKeyCode) 56, YES);
    CGPostKeyboardEvent ((CGCharCode) 0, (CGKeyCode) 56, NO);

    while (1)
    {
        myPool = [[NSAutoreleasePool alloc] init];
        myEvent = [NSApp nextEventMatchingMask: NSFlagsChangedMask
                                     untilDate: myDistantPast
                                        inMode: NSDefaultRunLoopMode
                                       dequeue: YES];
        
        // we are finished when no events are left or if our shift down event has the modifier key flag set:
        if (myEvent == NULL || (myModifierWasPressed = [myEvent modifierFlags] & theModifierKeyMask ? YES : NO) == YES)
        {
            break;
        }
        else
        {
            [NSApp sendEvent: myEvent];
            [myPool release];
        }
    }
    [myPool release];
    
    return (myModifierWasPressed);
}

//________________________________________________________________________________________________________checkForAlternateKey

+ (BOOL) checkForAlternateKey
{
    return ([FDModifierCheck checkForModifier: NSAlternateKeyMask]);
}

//__________________________________________________________________________________________________________checkForCommandKey

+ (BOOL) checkForCommandKey
{
    return ([FDModifierCheck checkForModifier: NSCommandKeyMask]);
}

//__________________________________________________________________________________________________________checkForControlKey

+ (BOOL) checkForControlKey
{
    return ([FDModifierCheck checkForModifier: NSControlKeyMask]);
}

//___________________________________________________________________________________________________________checkForOptionKey

+ (BOOL) checkForOptionKey
{
    return ([FDModifierCheck checkForAlternateKey]);
}

@end

//_________________________________________________________________________________________________________________________eOF
