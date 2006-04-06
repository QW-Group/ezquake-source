//_________________________________________________________________________________________________________________________nFO
// "QuakeApplication.m" - required for event and AppleScript handling.
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2006 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
// Quakeª is copyrighted by id software		[http://www.idsoftware.com].
//
//____________________________________________________________________________________________________________________iNCLUDES

#pragma mark =Includes=

#import <Cocoa/Cocoa.h>
#import <stdio.h>

#import "quakedef.h"
#import "Quake.h"
#import "QuakeApplication.h"
#import "sys_osx.h"

#pragma mark -

//_____________________________________________________________________________________________iMPLEMENTATION_QuakeApplication

@implementation QuakeApplication : NSApplication

//__________________________________________________________________________________________________________________sendEvent:

- (void) sendEvent: (NSEvent *) theEvent
{
    // we need to intercept NSApps "sendEvent:" action:
    if ([[self delegate] hostInitialized] == YES)
    {
        if ([self isHidden] == YES)
        {
            [super sendEvent: theEvent];
        }
        else
        {
            Sys_DoEvents (theEvent, [theEvent type]);
        }
    }
    else
    {
        [super sendEvent: theEvent];
    }
}

//_____________________________________________________________________________________________________________sendSuperEvent:

- (void) sendSuperEvent: (NSEvent *) theEvent
{
    [super sendEvent: theEvent];
}

//___________________________________________________________________________________________________________handleRunCommand:

- (void) handleRunCommand: (NSScriptCommand *) theCommand
{
    if ([[self delegate] allowAppleScriptRun] == YES)
    {
        NSDictionary	*myArguments = [theCommand evaluatedArguments];
    
        if (myArguments != NULL)
        {
            NSString 		*myParameters = [myArguments objectForKey: @"QuakeParameters"];
        
            // Check if we got command line parameters:
            if (myParameters != NULL && [myParameters isEqualToString: @""] == NO)
            {
                [[self delegate] stringToParameters: myParameters];
            }
            else
            {
                [[self delegate] stringToParameters: @""];
            }
        }
        [[self delegate] enableAppleScriptRun: NO];
    }
}

//_______________________________________________________________________________________________________handleConsoleCommand:

- (void) handleConsoleCommand: (NSScriptCommand *) theCommand
{
    NSDictionary	*myArguments = [theCommand evaluatedArguments];

    if (myArguments != NULL)
    {
        NSString		*myCommandList= [myArguments objectForKey: @"QuakeCommandlist"];
        
        // Send the console command only if we got commands:
        if (myCommandList != NULL && [myCommandList isEqualToString:@""] == NO)
        {
            // required because of the options dialog.
            // we have to wait with the command until host_init() is finished!
            if ([[self delegate] hostInitialized] == YES)
            {
                Cbuf_AddText (va ("%s\n", [myCommandList cString]));
            }
            else
            {
                [[self delegate] requestCommand: myCommandList];
            }
        }
    }
}

@end

//_________________________________________________________________________________________________________________________eOF
