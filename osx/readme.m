//___________________________________________________________________________________________________________nFO
// "readme.m" - MacOS X help launcher for the installer image.
//
// Written by:	Axel 'awe' Wefers	[mailto:awe@fruitz-of-dojo.de].
//		©2002 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
// Version History:
// v1.0:   Initial release.
//______________________________________________________________________________________________________iNCLUDES

#pragma mark =Includes=

#import <AppKit/AppKit.h>

#pragma mark -

//____________________________________________________________________________________________________iNTERFACES

#pragma mark =ObjC Interfaces=

@interface ReadMe : NSObject
@end

#pragma mark -

//_________________________________________________________________________________________iMPLEMENTATION_ReadMe

@implementation ReadMe

//________________________________________________________________________________applicationDidFinishLaunching:

- (void) applicationDidFinishLaunching: (NSNotification *) theNotification
{
#if 0
    // tell the user that he doesn't need to install this application:
    NSRunInformationalAlertPanel (@"You don't need to copy this \"Read Me\" to your harddrive.",
                                  @"Just use the \"Help\" menu at the desired Quake application, "
                                  @"if you want to access the following documentation later.",
                                  NULL, NULL, NULL);
#endif // 0
                                  
    // show the help and terminate:
    [NSApp showHelp: NULL];
    [NSApp terminate: NULL];
}

@end

//________________________________________________________________________________________________________main()

int	main (int theArgCount, const char **theArgValues)
{
    NSApplication	*myApplication;
    NSAutoreleasePool	*myPool;
    ReadMe 		*myReadMe;

    // we don't want to use a NIB file:
    myPool = [[NSAutoreleasePool alloc] init];
    myApplication = [NSApplication sharedApplication];
    myReadMe = [[ReadMe alloc] init];
    [myApplication setDelegate: myReadMe];
    [myApplication run];
    [myReadMe release];
    [myPool release];
    
    return (0);
}

//___________________________________________________________________________________________________________eOF
