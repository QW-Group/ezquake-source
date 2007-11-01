//_________________________________________________________________________________________________________________________nFO
// "Quake.m" - the controller.
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2006 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
// Quakeª is copyrighted by id software		[http://www.idsoftware.com].
//
//____________________________________________________________________________________________________________________iNCLUDES

#pragma mark =Includes=

#import <Cocoa/Cocoa.h>
#import <fcntl.h>
#import <unistd.h>

#import "NSToolbarPrivate.h"
#import "FDModifierCheck.h"

#import "quakedef.h"
#import "keys.h"
#import "Quake.h"
#import "cd_osx.h"
#import "in_osx.h"
#import "snd_osx.h"
#import "sys_osx.h"
#import "vid_osx.h"

#pragma mark -

//_____________________________________________________________________________________________________________________dEFINES

#pragma mark =Defines=

#define	GAME_COMMAND		"-game"
#define	QUAKE_BASE_PATH		"."

#pragma mark -

//______________________________________________________________________________________________________________________mACROS

#pragma mark =Macros=

#define	SYS_DURING		NS_DURING
#define SYS_HANDLER		NS_HANDLER								\
                                {									\
                                    NSString	*myException = [localException reason];			\
                                                                                                        \
                                    if (myException == NULL)						\
                                    {									\
                                        myException = @"Unknown exception!";				\
                                    }									\
                                    NSLog (@"An exception has occured: %@\n", myException);		\
                                    NSRunCriticalAlertPanel (@"An exception has occured:", myException,	\
                                                             NULL, NULL, NULL);				\
                                }									\
                                NS_ENDHANDLER;

#pragma mark -

//_________________________________________________________________________________________________________fUNCTION_pROTOTYPES

#pragma mark =Function Prototypes=

extern void 			M_Menu_Quit_f (void);

#pragma mark -

//______________________________________________________________________________________iNTERFACE_Quake_(NSApplicationDefined)

#pragma mark =Interfaces=

@interface Quake (NSApplicationDefined)
- (BOOL) application: (NSApplication *) theSender openFile: (NSString *) theFilePath;
- (void) applicationDidFinishLaunching: (NSNotification *) theNote;
- (NSApplicationTerminateReply) applicationShouldTerminate: (NSApplication *) theSender;
- (void) applicationDidResignActive: (NSNotification *) theNotification;
- (void) applicationDidBecomeActive: (NSNotification *) theNotification;
- (void) applicationWillHide: (NSNotification *) theNotification;
- (void) applicationWillUnhide: (NSNotification *) theNotification;
@end

//___________________________________________________________________________________________________iNTERFACE_Quake_(Toolbar)

@interface Quake (Toolbar)
- (void) awakeFromNib;
- (BOOL) validateToolbarItem: (NSToolbarItem *) theItem;
- (NSToolbarItem *) toolbar: (NSToolbar *) theToolbar itemForItemIdentifier: (NSString *) theIdentifier
                                                  willBeInsertedIntoToolbar: (BOOL) theFlag;
- (NSArray *) toolbarDefaultItemIdentifiers: (NSToolbar*) theToolbar;
- (NSArray *) toolbarAllowedItemIdentifiers: (NSToolbar*) theToolbar;
- (void) addToolbarItem: (NSMutableDictionary *) theDict identifier: (NSString *) theIdentifier
                  label: (NSString *) theLabel paletteLabel: (NSString *) thePaletteLabel
                toolTip: (NSString *) theToolTip image: (id) theItemContent selector: (SEL) theAction;
- (void) changeView: (NSView *) theView title: (NSString *) theTitle;
- (IBAction) showAboutView: (id) theSender;
- (IBAction) showDisplaysView: (id) theSender;
- (IBAction) showSoundView: (id) theSender;
- (IBAction) showCLIView: (id) theSender;
@end

//__________________________________________________________________________________________iNTERFACE_Quake_(InterfaceActions)

@interface Quake (InterfaceActions)
#if defined (GLQUAKE)
- (IBAction) buildResolutionList: (id) theSender;
- (IBAction) toggleColorsEnabled: (id) theSender;
#endif /* GLQUAKE */
- (IBAction) toggleParameterTextField: (id) theSender;
- (IBAction) toggleMP3Playback: (id) theSender;
- (IBAction) selectMP3Folder: (id) theSender;
- (void) closeMP3Sheet: (NSOpenPanel *) theSheet returnCode: (int) theCode contextInfo: (void *) theInfo;
- (IBAction) stopMediaScan: (id) theSender;
- (IBAction) newGame: (id) theSender;
- (IBAction) pasteString: (id) theSender;
- (IBAction) visitFOD: (id) theSender;
@end

//__________________________________________________________________________________________________iNTERFACE_Quake_(Services)

@interface Quake (Services)
- (void) connectToServer: (NSPasteboard *) thePasteboard userData:(NSString *)theData
                   error: (NSString **) theError;
@end

//___________________________________________________________________________________________________iNTERFACE_Quake_(Private)

@interface Quake (Private)
- (void) buildDisplayList;
- (void) scanMediaThread: (id) theSender;
- (void) setupParameterUI:  (NSUserDefaults *) theDefaults;
- (void) setupDialog: (NSTimer *) theTimer;
- (void) saveCheckBox: (NSButton *) theButton initial: (NSString *) theInitial
              default: (NSString *) theDefault userDefaults: (NSUserDefaults *) theUserDefaults;
- (void) saveString: (NSString *) theString initial: (NSString *) theInitial
            default: (NSString *) theDefault userDefaults: (NSUserDefaults *) theUserDefaults;
- (BOOL) isEqualTo: (NSString *) theString;
- (NSString *) displayModeToString: (NSDictionary *) theDisplayMode;
- (void) renderFrame: (NSTimer *) theTimer;
- (void) installFrameTimer;
- (void) fireFrameTimer: (NSNotification *) theNotification;
@end

#pragma mark -

//________________________________________________________________________________________________________iMPLEMENTATION_Quake

@implementation Quake : NSObject

//__________________________________________________________________________________________________________________initialize

+ (void) initialize
{
    NSUserDefaults	*myDefaults = [NSUserDefaults standardUserDefaults];
    NSString		*myDefaultPath = [[[[NSBundle mainBundle] bundlePath]
                                                stringByDeletingLastPathComponent]
                                                stringByAppendingPathComponent: INITIAL_BASE_PATH];

    // register application defaults:
    [myDefaults registerDefaults: [NSDictionary dictionaryWithObjects:
                                    [NSArray arrayWithObjects: myDefaultPath,
                                                               INITIAL_USE_MP3,
                                                               INITIAL_MP3_PATH,
                                                               INITIAL_USE_PARAMETERS,
                                                               INITIAL_PARAMETERS,
                                                               INITIAL_OPTION_KEY,
                                                               INITIAL_DISPLAY,
                                                               INITIAL_FADE_ALL,
                                                               INITIAL_CUR_WINDOW_WIDTH,
                                                               INITIAL_WINDOW_WIDTH,
                                                               INITIAL_GL_DISPLAY,
                                                               INITIAL_GL_DISPLAY_MODE,
                                                               INITIAL_GL_COLORS,
                                                               INITIAL_GL_SAMPLES,
                                                               INITIAL_GL_FADE_ALL,
                                                               INITIAL_GL_FULLSCREEN,
                                                               INITIAL_GL_OPTION_KEY,
                                                               NULL]
                                    forKeys: 
                                    [NSArray arrayWithObjects: DEFAULT_BASE_PATH,
                                                               DEFAULT_USE_MP3,
                                                               DEFAULT_MP3_PATH,
                                                               DEFAULT_USE_PARAMETERS,
                                                               DEFAULT_PARAMETERS,
                                                               DEFAULT_OPTION_KEY,
                                                               DEFAULT_DISPLAY,
                                                               DEFAULT_FADE_ALL,
                                                               DEFAULT_CUR_WINDOW_WIDTH,
                                                               DEFAULT_WINDOW_WIDTH,
                                                               DEFAULT_GL_DISPLAY,
                                                               DEFAULT_GL_DISPLAY_MODE,
                                                               DEFAULT_GL_COLORS,
                                                               DEFAULT_GL_SAMPLES,
                                                               DEFAULT_GL_FADE_ALL,
                                                               DEFAULT_GL_FULLSCREEN,
                                                               DEFAULT_GL_OPTION_KEY,
                                                               NULL]
                                  ]];
}

//_____________________________________________________________________________________________________________________dealloc

- (void) dealloc
{
    if (mRequestedCommands != NULL)
    {
        [mRequestedCommands release];
        mRequestedCommands = NULL;
    }
    
    if (mEmptyView != NULL)
    {
        [mEmptyView release];
        mEmptyView = NULL;
    }

    if (mDistantPast != NULL)
    {
        [mDistantPast release];
        mDistantPast = NULL;
    }
    
    if (mModFolder != NULL)
    {
        [mModFolder release];
        mModFolder = NULL;
    }
    
    [super dealloc];
}

//_________________________________________________________________________________________________________stringToParameters:

- (void) stringToParameters: (NSString *) theString
{
    NSArray				*mySeparatedArguments;
    NSMutableArray      *myNewArguments;
    NSCharacterSet		*myQuotationMarks;
    NSString			*myArgument;
    char				**myNewArgValues;
    SInt32				i;
    
    // get all parameters separated by a space:
    mySeparatedArguments = [theString componentsSeparatedByString: @" "];
    
    // no parameters at all?
    if (mySeparatedArguments == NULL || [mySeparatedArguments count] == 0)
    {
        return;
    }
    
    // concatenate parameters that start on " and end on ":
    myNewArguments = [NSMutableArray arrayWithCapacity: 0];
    myQuotationMarks = [NSCharacterSet characterSetWithCharactersInString: @"\""];
    
    for (i = 0; i < [mySeparatedArguments count]; i++)
    {
        myArgument = [mySeparatedArguments objectAtIndex: i];
        if (myArgument != NULL && [myArgument length] != 0)
        {
            if ([myArgument characterAtIndex: 0] == '\"')
            {
                myArgument = [NSString stringWithString: @""];
                for (; i < [mySeparatedArguments count]; i++)
                {
                    myArgument = [myArgument stringByAppendingString: [mySeparatedArguments objectAtIndex: i]];
                    if ([myArgument characterAtIndex: [myArgument length] - 1] == '\"')
                    {
                        break;
                    }
                    else
                    {
                        if (i < [mySeparatedArguments count] - 1)
                        {
                            myArgument = [myArgument stringByAppendingString: @" "];
                        }
                    }
                }
            }
            myArgument = [myArgument stringByTrimmingCharactersInSet: myQuotationMarks];
            if (myArgument != NULL && [myArgument length] != 0)
            {
                [myNewArguments addObject: myArgument];
            }
        }
    }

    gSysArgCount = [myNewArguments count] + 1;
    myNewArgValues = (char **) malloc (sizeof(char *) * gSysArgCount);
    SYS_CHECK_MALLOC (myNewArgValues);

    myNewArgValues[0] = gSysArgValues[0];
    gSysArgValues = myNewArgValues;
    
    // insert the new parameters:
    for (i = 0; i < [myNewArguments count]; i++)
    {
        char *	myCString = (char *) [[myNewArguments objectAtIndex: i] cString];
        
        gSysArgValues[i+1] = (char *) malloc (strlen (myCString) + 1);
        SYS_CHECK_MALLOC (gSysArgValues[i+1]);
        strcpy (gSysArgValues[i+1], myCString);
    }
}

//__________________________________________________________________________________________________________________wasDragged

- (BOOL) wasDragged
{
    return (mModFolder != NULL ? YES : NO);
}

//_____________________________________________________________________________________________________________hostInitialized

- (BOOL) hostInitialized
{
    return (mHostInitialized);
}

//_________________________________________________________________________________________________________setHostInitialized:

- (void) setHostInitialized: (BOOL) theState
{
    mHostInitialized = theState;
}

//_________________________________________________________________________________________________________allowAppleScriptRun

- (BOOL) allowAppleScriptRun
{
    return (mAllowAppleScriptRun);
}

//_______________________________________________________________________________________________________enableAppleScriptRun:

- (void) enableAppleScriptRun: (BOOL) theState
{
    mAllowAppleScriptRun = theState;
}

//_____________________________________________________________________________________________________________requestCommand:

- (void) requestCommand: (NSString *) theCommand
{
    if (mRequestedCommands == NULL)
    {
        mRequestedCommands = [[NSMutableArray alloc] initWithCapacity: 0];
		
        if (mRequestedCommands == NULL)
        {
            return;
        }
    }
    [mRequestedCommands addObject: theCommand];
}

//_________________________________________________________________________________________________________________distantPast

- (NSDate *) distantPast
{
    return (mDistantPast);
}

//___________________________________________________________________________________________________________________modFolder

- (NSString *) modFolder
{
    return (mModFolder);
}

//_________________________________________________________________________________________________________________mediaFolder

- (NSString *) mediaFolder
{
    return (mMP3Folder);
}

//______________________________________________________________________________________________________________abortMediaScan

- (BOOL) abortMediaScan
{
    return (mMediaScanCanceled);
}

@end

//_________________________________________________________________________________iMPLEMENTATION_Quake_(NSApplicationDefined)

@implementation Quake (NSApplicationDefined)

//_______________________________________________________________________________________________________application:openFile:

- (BOOL) application: (NSApplication *) theSender openFile: (NSString *) theFilePath
{
    // allow only dragging one time:
    if (mDenyDrag == YES)
    {
        return (NO);
    }
	
    mDenyDrag = YES;
    
    if (gSysArgCount > 2)
    {
        return (NO);
    }    
    
    // we have received a filepath:
    if (theFilePath != NULL)
    {
    
        char 		*myMod  = (char *) [[theFilePath lastPathComponent] fileSystemRepresentation];
        char		**myNewArgValues;
        BOOL		myDirectory;
        
        // is the filepath a folder?
        if (![[NSFileManager defaultManager] fileExistsAtPath: theFilePath isDirectory: &myDirectory])
        {
            Sys_Error ("The dragged item is not a valid file!");
        }
        if (myDirectory == NO)
        {
            Sys_Error ("The dragged item is not a folder!");
        }
        
        // prepare the new command line options:
        myNewArgValues = (char **) malloc (sizeof(char *) * 3);
        SYS_CHECK_MALLOC (myNewArgValues);
        gSysArgCount = 3;
        myNewArgValues[0] = gSysArgValues[0];
        gSysArgValues = myNewArgValues;
        gSysArgValues[1] = GAME_COMMAND;
        gSysArgValues[2] = (char *) malloc (strlen (myMod) + 1);
        SYS_CHECK_MALLOC (gSysArgValues[2]);
        strcpy (gSysArgValues[2], myMod);
        
        // get the path of the mod [compare it with the id1 path later]:
        mModFolder = [[theFilePath stringByDeletingLastPathComponent] retain];
        
        return (YES);
    }
    return (NO);
}

//______________________________________________________________________________________________applicationDidFinishLaunching:

- (void) applicationDidFinishLaunching: (NSNotification *) theNote
{
    SYS_DURING
    {
        NSTimer		*myTimer;
        
        // enable the AppleScript run command:
        [self enableAppleScriptRun: YES];
        
        // don't accept any drags from this point on!
        mDenyDrag = YES;

        // check if the user has pressed the Option key on startup:
        mOptionPressed = [FDModifierCheck checkForOptionKey];

        // examine the "id1" folder and check if the MOD has the right location:
        Sys_CheckForIDDirectory ();

        // show the settings dialog after 0.5s (required to recognize the "run" AppleScript command):
        myTimer = [NSTimer scheduledTimerWithTimeInterval: 0.5f
                                                   target: self
                                                 selector: @selector (setupDialog:)
                                                 userInfo: NULL
                                                  repeats: NO];
        
        // just in case:
        if (myTimer == NULL)
        {
            [self setupDialog: NULL];
        }
    }
    SYS_HANDLER;
}

//_________________________________________________________________________________________________applicationShouldTerminate:

- (NSApplicationTerminateReply) applicationShouldTerminate: (NSApplication *) theSender
{
    if ([self hostInitialized] == YES)
    {
        if ([NSApp isHidden] == YES || [NSApp isActive] == NO)
        {
            [NSApp activateIgnoringOtherApps: YES];
        }
        
        if (gVidDisplayFullscreen == NO && gVidWindow != NULL)
        {
            if ([gVidWindow isMiniaturized] == YES)
            {
                [gVidWindow deminiaturize: NULL];
            }
            
            [gVidWindow orderFront: NULL];
        }
        
        M_Menu_Quit_f ();
		
        return (NSTerminateCancel);
    }
    
    return (NSTerminateNow);
}

//_________________________________________________________________________________________________applicationDidResignActive:

- (void) applicationDidResignActive: (NSNotification *) theNotification
{
    if ([self hostInitialized] == NO)
    {
        return;
    }
    
    IN_ShowCursor (YES);
    IN_SetF12EjectEnabled (YES);
}

//_________________________________________________________________________________________________applicationDidBecomeActive:

- (void) applicationDidBecomeActive: (NSNotification *) theNotification
{
    if ([self hostInitialized] == NO)
    {
        return;
    }
    
    if (gVidDisplayFullscreen == YES || (gInMouseEnabled == YES && _windowed_mouse.value != 0.0f))
    {
        IN_ShowCursor (NO);
    }
    
    IN_SetF12EjectEnabled (NO);
}

//________________________________________________________________________________________________________applicationWillHide:

- (void) applicationWillHide: (NSNotification *) theNotification
{
    if ([self hostInitialized] == NO)
    {
        return;
    }

    S_StopAllSounds (YES);
    SNDDMA_Shutdown ();
    CDAudio_Enable (NO);
#if !defined (GLQUAKE)
    VID_HideFullscreen (YES);
#endif /* !GLQUAKE */
    IN_ShowCursor (YES);
    IN_SetF12EjectEnabled (YES);

    if (mFrameTimer != NULL)
    {
        [mFrameTimer invalidate];
        mFrameTimer = NULL;
    }
}

//______________________________________________________________________________________________________applicationWillUnhide:

- (void) applicationWillUnhide: (NSNotification *) theNotification
{
    if ([self hostInitialized] == NO)
    {
        return;
    }
    
    if (gVidDisplayFullscreen == YES || (gInMouseEnabled == YES && _windowed_mouse.value != 0.0f))
    {
        IN_ShowCursor (NO);
    }
    
    IN_SetF12EjectEnabled (NO);
#if !defined (GLQUAKE)
    VID_HideFullscreen (NO);
#endif /* !GLQUAKE */
    CDAudio_Enable (YES);
    SNDDMA_Init ();
    
    [self installFrameTimer];
}

//_______________________________________________________________________________________applicationDidChangeScreenParameters:
//[[NSNotificationCenter defaultCenter] addObserver: self selector: @selector (applicationDidChangeScreenParameters:) name: NSApplicationDidChangeScreenParametersNotification object: NULL];
- (void) applicationDidChangeScreenParameters: (NSNotification *) theNotification
{
NSLog (@"A!");
    // do we have a windowed mode?
    if (gVidWindow != NULL)
    {
        NSRect	myScreenRect = [[gVidWindow screen] visibleFrame],
                myWindowRect = [gVidWindow frame];
NSLog (@"B!");
        // is our window larger than the display?
        if (NSWidth (myWindowRect) > NSWidth (myScreenRect) || NSHeight (myWindowRect) > NSHeight (myScreenRect))
        {
            NSRect	myContentRect = [[gVidWindow contentView] frame];
            NSSize	myMinSize = [gVidWindow minSize],
                        myBorder =  NSMakeSize (NSWidth (myWindowRect) - NSWidth (myContentRect),
                                    NSHeight (myWindowRect) - NSHeight (myContentRect));
            float	myAspect = (float) myMinSize.width / (float) myMinSize.height;

NSLog (@"C!");
            // add the window border to the minsize:
            myMinSize.width += myBorder.width;
            myMinSize.height += myBorder.height;

            // subtract the window border from the screen rect:
            myScreenRect.size.width -= myBorder.width;
            myScreenRect.size.height -= myBorder.height;

            // apply aspect-ratio to the visible rect of our screen:
            if (NSWidth (myScreenRect) / NSHeight (myScreenRect) > myAspect)
            {
                myScreenRect.size.width = NSHeight (myScreenRect) * myAspect;
            }
            else
            {
                myScreenRect.size.height = NSWidth (myScreenRect) / myAspect;
            } 
            
            myWindowRect.size = myScreenRect.size;
            
            // check the minimum size:
            if (NSWidth (myWindowRect) < myMinSize.width || NSHeight (myWindowRect) < myMinSize.height)
            {
                myScreenRect.size = myMinSize;
            }
            
            // center the window:
            myWindowRect.origin.x = NSMinX (myScreenRect) + NSWidth (myScreenRect) / 2.0f - NSWidth (myWindowRect) / 2.0f;
            myWindowRect.origin.y = NSMinY (myScreenRect) + NSHeight (myScreenRect) / 2.0f - NSHeight (myWindowRect) / 2.0f;
            
            // set the frame:
            [gVidWindow setFrame: myWindowRect display: YES];
        }
    }
}

@end

//______________________________________________________________________________________________iMPLEMENTATION_Quake_(Toolbar)

@implementation Quake (Toolbar)

//________________________________________________________________________________________________________________awakeFromNib

- (void) awakeFromNib
{
    NSToolbar 		*myToolbar = [[[NSToolbar alloc] initWithIdentifier: @"Quake Toolbar"] autorelease];

    // required for event handling:
    mDistantPast = [[NSDate distantPast] retain];

    // initialize the toolbar:
    mToolbarItems = [[NSMutableDictionary dictionary] retain];
    [self addToolbarItem: mToolbarItems identifier: SYS_ABOUT_TOOLBARITEM label: @"About" paletteLabel: @"About"
                 toolTip: @"About Quake." image: @"about.tiff"
                selector: @selector (showAboutView:)];
    [self addToolbarItem: mToolbarItems identifier: SYS_VIDEO_TOOLBARITEM label: @"Displays"
            paletteLabel: @"Displays" toolTip: @"Change display settings." image: @"displays.tiff"
                selector: @selector (showDisplaysView:)];
    [self addToolbarItem: mToolbarItems identifier: SYS_AUDIO_TOOLBARITEM label: @"Sound" paletteLabel: @"Sound"
                 toolTip: @"Change sound settings." image: @"sound.tiff" selector: @selector (showSoundView:)];
    [self addToolbarItem: mToolbarItems identifier: SYS_PARAM_TOOLBARITEM label: @"CLI" paletteLabel: @"CLI"
                 toolTip: @"Set command-line parameters." image: @"cli.tiff"
                 selector: @selector (showCLIView:)];
    [self addToolbarItem: mToolbarItems identifier: SYS_START_TOOLBARITEM label: @"Play" paletteLabel: @"Play"
                 toolTip: @"Start the game." image: @"start.tiff"
                 selector: @selector (newGame:)];
                 
    [myToolbar setDelegate: self];    
    [myToolbar setAllowsUserCustomization: NO];
    [myToolbar setAutosavesConfiguration: NO];
    [myToolbar setDisplayMode: NSToolbarDisplayModeIconAndLabel];
    [settingsWindow setToolbar: myToolbar];
    [self showAboutView: self];
}

//________________________________________________________________________________________________________validateToolbarItem:

- (BOOL) validateToolbarItem: (NSToolbarItem *) theItem
{
    return (YES);
}

//____________________________________________________________________toolbar:itemForItemIdentifier:willBeInsertedIntoToolbar:

- (NSToolbarItem *) toolbar: (NSToolbar *) theToolbar itemForItemIdentifier: (NSString *) theIdentifier
                                                  willBeInsertedIntoToolbar: (BOOL) theFlag
{
    NSToolbarItem *myItem = [mToolbarItems objectForKey: theIdentifier];
    NSToolbarItem *myNewItem = [[[NSToolbarItem alloc] initWithItemIdentifier: theIdentifier] autorelease];
    
    [myNewItem setLabel: [myItem label]];
    [myNewItem setPaletteLabel: [myItem paletteLabel]];
    [myNewItem setImage: [myItem image]];
    [myNewItem setToolTip: [myItem toolTip]];
    [myNewItem setTarget: [myItem target]];
    [myNewItem setAction: [myItem action]];
    [myNewItem setMenuFormRepresentation: [myItem menuFormRepresentation]];

    return (myNewItem);
}

//______________________________________________________________________________________________toolbarDefaultItemIdentifiers:

- (NSArray *) toolbarDefaultItemIdentifiers: (NSToolbar*) theToolbar
{
    return ([NSArray arrayWithObjects: SYS_ABOUT_TOOLBARITEM, SYS_VIDEO_TOOLBARITEM, SYS_AUDIO_TOOLBARITEM, 
                                       SYS_PARAM_TOOLBARITEM, NSToolbarFlexibleSpaceItemIdentifier,
                                       SYS_START_TOOLBARITEM, nil]);
}

//______________________________________________________________________________________________toolbarAllowedItemIdentifiers:

- (NSArray *) toolbarAllowedItemIdentifiers: (NSToolbar*) theToolbar
{
    return ([NSArray arrayWithObjects: SYS_ABOUT_TOOLBARITEM, SYS_VIDEO_TOOLBARITEM, SYS_VIDEO_TOOLBARITEM,
                                       SYS_PARAM_TOOLBARITEM, SYS_START_TOOLBARITEM,
                                       NSToolbarFlexibleSpaceItemIdentifier, nil]);
}

//________________________________________________________addToolbarItem:identifier:label:paletteLabel:toolTip:image:selector:

- (void) addToolbarItem: (NSMutableDictionary *) theDict identifier: (NSString *) theIdentifier
                  label: (NSString *) theLabel paletteLabel: (NSString *) thePaletteLabel
                toolTip: (NSString *) theToolTip image: (id) theItemContent selector: (SEL) theAction
{
    NSToolbarItem	*myItem = [[[NSToolbarItem alloc] initWithItemIdentifier: theIdentifier] autorelease];

    [myItem setLabel: theLabel];
    [myItem setPaletteLabel: thePaletteLabel];
    [myItem setToolTip: theToolTip];
    [myItem setTarget: self];
    [myItem setImage: [NSImage imageNamed: theItemContent]];
    [myItem setAction: theAction];
    [theDict setObject: myItem forKey: theIdentifier];
}

//___________________________________________________________________________________________________________changeView:title:

- (void) changeView: (NSView *) theView title: (NSString *) theTitle
{
    NSRect	myCurFrame, myNewFrame;
    UInt32	myNewHeight;
    
    if (theView == NULL || theView == [settingsWindow contentView])
    {
        return;
    }
    
    if (mEmptyView == NULL)
    {
        mEmptyView = [[settingsWindow contentView] retain];
    }

    myCurFrame = [NSWindow contentRectForFrameRect:[settingsWindow frame] styleMask:[settingsWindow styleMask]];
    [mEmptyView setFrame: myCurFrame];
    [settingsWindow setContentView: mEmptyView];

    [settingsWindow setTitle: [NSString stringWithFormat:
                                                          @"EZQuake"
#if defined (GLQUAKE)
														  @"-GL"
#endif /* GLQUAKE */
                                                          @" (%@)", theTitle]];

    myNewHeight = NSHeight ([theView frame]);
    if ([[settingsWindow toolbar] isVisible])
    {
        myNewHeight += NSHeight ([[[settingsWindow toolbar] _toolbarView] frame]);
    }
    myNewFrame = NSMakeRect (NSMinX (myCurFrame), NSMaxY (myCurFrame ) - myNewHeight,
                             NSWidth (myCurFrame), myNewHeight);
    myNewFrame = [NSWindow frameRectForContentRect: myNewFrame styleMask: [settingsWindow styleMask]];

    [settingsWindow setFrame: myNewFrame display: YES animate: [settingsWindow isVisible]];
    [settingsWindow setContentView: theView];
}

//______________________________________________________________________________________________________________showAboutView:

- (IBAction) showAboutView: (id) theSender
{
    [self changeView: aboutView title: @"About"];
}

//___________________________________________________________________________________________________________showDisplaysView:

- (IBAction) showDisplaysView: (id) theSender
{
    [self changeView: videoView title: @"Displays"];
}

//______________________________________________________________________________________________________________showSoundView:

- (IBAction) showSoundView: (id) theSender
{
    [self changeView: audioView title: @"Sound"];
}

//________________________________________________________________________________________________________________showCLIView:

- (IBAction) showCLIView: (id) theSender
{
    [self changeView: parameterView title: @"CLI"];
}

@end

//_____________________________________________________________________________________iMPLEMENTATION_Quake_(InterfaceActions)

@implementation Quake (InterfaceActions)


#if defined (GLQUAKE)

- (IBAction) buildResolutionList: (id) theSender
{
    CGDisplayCount		myDisplayIndex;
    CGDirectDisplayID	mySelectedDisplay;
    NSArray				*myDisplayModes;
    UInt16				myResolutionIndex = 0, myModeCount, i;
    UInt32				myColors, myDepth;
    NSDictionary		*myCurMode;
    NSString			*myDescription, *myOldResolution = NULL;

    // get the old resolution:
    if (mModeList != NULL)
    {
        // modelist can have a count of zero...
        if([mModeList count] > 0)
        {
            myOldResolution = [self displayModeToString: [mModeList objectAtIndex: [modePopUp indexOfSelectedItem]]];
        }
        [mModeList release];
        mModeList = NULL;
    }
    
    // figure out which display was selected by the user:
    myDisplayIndex = [displayPopUp indexOfSelectedItem];
    if (myDisplayIndex >= gVidDisplayCount)
    {
        myDisplayIndex = 0;
        [displayPopUp selectItemAtIndex: myDisplayIndex];
    }
    mySelectedDisplay = gVidDisplayList[myDisplayIndex];
    
    // get the list of display modes:
    myDisplayModes = [(NSArray *) CGDisplayAvailableModes (mySelectedDisplay) retain];
    SYS_CHECK_MALLOC (myDisplayModes);
    mModeList = [[NSMutableArray alloc] init];
    SYS_CHECK_MALLOC (mModeList);
    
    // filter modes:
    myModeCount = [myDisplayModes count];
    myColors = ([colorsPopUp indexOfSelectedItem] == 0) ? 16 : 32;
    for (i = 0; i < myModeCount; i++)
    {
        myCurMode = [myDisplayModes objectAtIndex: i];
        myDepth = [[myCurMode objectForKey: (NSString *) kCGDisplayBitsPerPixel] intValue];
        if (myColors == myDepth)
        {
            UInt16	j = 0;
            
            // I got double entries while testing [OSX bug?], so we have to check:
            while (j < [mModeList count])
            {
                if ([[self displayModeToString: [mModeList objectAtIndex: j]] isEqualToString:
                    [self displayModeToString: myCurMode]])
                {
                    break;
                }
                j++;
            }
            if (j == [mModeList count])
            {
                [mModeList addObject: myCurMode];
            }
        }
    }

	[mModeList sortUsingFunction: VSH_SortDisplayModesCbk context: nil];

    // Fill the popup with the resulting modes:
    [modePopUp removeAllItems];
    myModeCount = [mModeList count];
    if (myModeCount == 0)
    {
        [modePopUp addItemWithTitle: @"not available!"];
    }
    else
    {
        for (i = 0; i < myModeCount; i++)
        {
            myDescription = [self displayModeToString: [mModeList objectAtIndex: i]];
            if(myOldResolution != nil)
            {
                if([myDescription isEqualToString: myOldResolution]) myResolutionIndex = i;
            }
            [modePopUp addItemWithTitle: myDescription];
        }
    }
    [modePopUp selectItemAtIndex: myResolutionIndex];

    if (myModeCount <= 1)
    {
        [modePopUp setEnabled: NO];
    }
    else
    {
        [modePopUp setEnabled: YES];
    }

    // last not least check for multisample buffers:
    if (GL_CheckARBMultisampleExtension (mySelectedDisplay) == YES)
    {
        [samplesPopUp setEnabled: YES];
    }
    else
    {
        [samplesPopUp setEnabled: NO];
        [samplesPopUp selectItemAtIndex: 0];
    }

    // clean up:
    if (myDisplayModes != NULL)
    {
        [myDisplayModes release];
    }
}

//________________________________________________________________________________________________________toggleColorsEnabled:

- (IBAction) toggleColorsEnabled: (id) theSender
{
    [colorsPopUp setEnabled: [fullscreenCheckBox state]];
}

#endif /* GLQUAKE */

//___________________________________________________________________________________________________toggleParameterTextField:

- (IBAction) toggleParameterTextField: (id) theSender
{
    [parameterTextField setEnabled: [parameterCheckBox state]];
}

//__________________________________________________________________________________________________________toggleMP3Playback:

- (IBAction) toggleMP3Playback: (id) theSender
{
    BOOL	myState = [mp3CheckBox state];
    
    [mp3Button setEnabled: myState];
    [mp3TextField setEnabled: myState];
}

//____________________________________________________________________________________________________________selectMP3Folder:

- (IBAction) selectMP3Folder: (id) theSender
{
    // prepare the sheet, if not already done:
    NSOpenPanel*    mp3Panel = [NSOpenPanel openPanel];

    [mp3Panel setAllowsMultipleSelection: NO];
    [mp3Panel setCanChooseFiles: NO];
    [mp3Panel setCanChooseDirectories: YES];
    [mp3Panel setAccessoryView: mp3HelpView];
    [mp3Panel setDirectory: [mp3TextField stringValue]];
    [mp3Panel setTitle: @"Select the folder that holds the MP3 files:"];
    
    // show the sheet:
    [mp3Panel beginSheetForDirectory: @""
                                 file: NULL
                                types: NULL
                       modalForWindow: settingsWindow
                        modalDelegate: self
                       didEndSelector: @selector (closeMP3Sheet:returnCode:contextInfo:)
                          contextInfo: NULL];
}

//_______________________________________________________________________________________closeMP3Sheet:returnCode:contextInfo:

- (void) closeMP3Sheet: (NSOpenPanel *) theSheet returnCode: (int) theCode contextInfo: (void *) theInfo
{
    [theSheet close];

    // do nothing on cancel:
    if (theCode != NSCancelButton)
    {
        NSArray *		myFolderArray;

        // get the path of the selected folder;
        myFolderArray = [theSheet filenames];
        if ([myFolderArray count] > 0)
        {
            [mp3TextField setStringValue: [myFolderArray objectAtIndex: 0]];
        }
    }
}

//______________________________________________________________________________________________________________stopMediaScan:

- (IBAction) stopMediaScan: (id) theSender
{
    mMediaScanCanceled = YES;
}

//____________________________________________________________________________________________________________________newGame:

- (IBAction) newGame: (id) theSender
{
    SYS_DURING
    {
        NSUserDefaults	*myDefaults = [NSUserDefaults standardUserDefaults];

#if defined (GLQUAKE)

        NSString	*myModeStr;
        UInt32		myMode, myColors;

        // check if display modes are available:
        if ([mModeList count] == 0)
        {
            NSBeginAlertSheet (@"No display modes available!", NULL, NULL, NULL, settingsWindow, NULL,
                                NULL, NULL, NULL, @"Please try other displays and/or color settings.");
            return;
        }

#endif /* GLQUAKE */

        // save the display:
        gVidDisplay = [displayPopUp indexOfSelectedItem];
        [self saveString: [NSString stringWithFormat: @"%d", gVidDisplay] initial: DISPLAY_INITIAL
                                                                          default: DISPLAY_DEFAULT
                                                                     userDefaults: myDefaults];

#if defined (GLQUAKE)

        // save the display mode:
        myMode = [modePopUp indexOfSelectedItem];
        myModeStr = [self displayModeToString: [mModeList objectAtIndex: myMode]];
        [self saveString: myModeStr initial: INITIAL_GL_DISPLAY_MODE default: DEFAULT_GL_DISPLAY_MODE
                                                                userDefaults: myDefaults];
        
        // save the colors:
        myColors = [colorsPopUp indexOfSelectedItem];
        [self saveString: [NSString stringWithFormat: @"%d", myColors] initial: INITIAL_GL_COLORS
                                                                       default: DEFAULT_GL_COLORS
                                                                  userDefaults: myDefaults];
    
        // save the samples:
        if ([samplesPopUp isEnabled] == YES)
            gGLMultiSamples = [samplesPopUp indexOfSelectedItem] << 2;
        else
            gGLMultiSamples = 0;
        [self saveString: [NSString stringWithFormat: @"%d", gGLMultiSamples] initial: INITIAL_GL_SAMPLES
                                                                              default: DEFAULT_GL_SAMPLES
                                                                         userDefaults: myDefaults];
        
        // save the state of the "fullscreen" checkbox:
        [self saveCheckBox: fullscreenCheckBox initial: INITIAL_GL_FULLSCREEN
                                               default: DEFAULT_GL_FULLSCREEN
                                          userDefaults: myDefaults];

#endif /* GLQUAKE */

        // save the state of the "fade all" checkbox:
        [self saveCheckBox: fadeAllCheckBox initial: FADE_ALL_INITIAL default: FADE_ALL_DEFAULT
                                                                 userDefaults: myDefaults];
        
        // save the state of the "MP3" checkbox:
        [self saveCheckBox: mp3CheckBox initial: INITIAL_USE_MP3
                                        default: DEFAULT_USE_MP3
                                   userDefaults: myDefaults];
        
        // save the MP3 path:
        [self saveString: [mp3TextField stringValue] initial: INITIAL_MP3_PATH
                                                     default: DEFAULT_MP3_PATH
                                                userDefaults: myDefaults];
        
        // save the state of the "option key" checkbox:
        [self saveCheckBox: optionCheckBox initial: OPTION_KEY_INITIAL
                                           default: OPTION_KEY_DEFAULT
                                      userDefaults: myDefaults];
        
        // save the state of the "use command line parameters" checkbox:
        [self saveCheckBox: parameterCheckBox initial: INITIAL_USE_PARAMETERS
                                              default: DEFAULT_USE_PARAMETERS
                                         userDefaults: myDefaults];
        
        // save the command line string from the parameter text field [only if no parameters were passed]:
        if ([parameterCheckBox isEnabled] == YES)
        {
            [self saveString: [parameterTextField stringValue] initial: INITIAL_PARAMETERS
                                                               default: DEFAULT_PARAMETERS
                                                          userDefaults: myDefaults];
        
            if ([parameterCheckBox state] == YES)
            {
                [self stringToParameters: [parameterTextField stringValue]];
            }
        }
        
        if ([mp3CheckBox state] == YES)
        {
            mMP3Folder = [mp3TextField stringValue];
            [mediascanTextField setStringValue: @"Scanning folder for MP3 and MP4 files..."];
        }
        else
        {
            [mediascanTextField setStringValue: @"Scanning AudioCDs..."];
        }
        
        
        // synchronize prefs and start Quake:
        [myDefaults synchronize];
        [settingsWindow close];
        
#if defined (GLQUAKE)
        
        gVidDisplayMode = [mModeList objectAtIndex: myMode];
        gVidDisplayFullscreen = [fullscreenCheckBox state];
        
#endif /* GLQUAKE */
        
        gVidFadeAllDisplays = [fadeAllCheckBox state];
        
        SNDDMA_ReserveBufferSize ();
        [mediascanWindow center];
        [mediascanWindow makeKeyAndOrderFront: nil];
        [mediascanProgressIndicator startAnimation: self];
        [[NSDistributedNotificationCenter defaultCenter] addObserver: self
                                                            selector: @selector (fireFrameTimer:)
                                                                name: @"Fire Frame Timer"
                                                              object: NULL];
        
        [NSThread detachNewThreadSelector: @selector (scanMediaThread:) toTarget: self withObject: nil];
    }
    SYS_HANDLER;
}

//________________________________________________________________________________________________________________pasteString:

- (IBAction) pasteString: (id) theSender
{
    extern qbool		keydown[];
    qbool			myOldCommand,
                        myOldVKey;

    // get the old state of the paste keys:
    myOldCommand = keydown[K_CMD];
    myOldVKey = keydown['v'];

    // send the keys required for paste:
    keydown[K_CMD] = true;
    Key_Event ('v', true);

    // set the old state of the paste keys:
    Key_Event ('v', false);
    keydown[K_CMD] = myOldCommand;
}

//___________________________________________________________________________________________________________________visitFOD:

- (IBAction) visitFOD: (id) theSender
{
	[[NSWorkspace sharedWorkspace] openURL: [NSURL URLWithString: EZQUAKE_URL]];
}

@end

//_____________________________________________________________________________________________iMPLEMENTATION_Quake_(Services)

@implementation Quake (Services)

//_____________________________________________________________________________________________connectToServer:userData:error:

- (void) connectToServer: (NSPasteboard *) thePasteboard userData:(NSString *)theData
                   error: (NSString **) theError
{
    NSArray 	*myPasteboardTypes;

    myPasteboardTypes = [thePasteboard types];

    if ([myPasteboardTypes containsObject: NSStringPboardType])
    {
        NSString 	*myRequestedServer;

        myRequestedServer = [thePasteboard stringForType: NSStringPboardType];
        if (myRequestedServer != NULL)
        {
            Cbuf_AddText (va ("connect %s\n", [myRequestedServer cString]));
            return;
        }
    }
	
    *theError = @"Unable to connect to a server: could not find a string on the pasteboard!";
}

@end

//______________________________________________________________________________________________iMPLEMENTATION_Quake_(Private)

@implementation Quake (Private)

//___________________________________________________________________________________________________________buildDisplayList:

- (void) buildDisplayList
{
    boolean_t 	(*qCGDisplayIsMain)(CGDirectDisplayID display) = NULL;
    boolean_t 	(*qCGDisplayIsBuiltin)(CGDirectDisplayID display) = NULL;
    NSString	*myDisplayName;
    UInt32		i;
    
    // retrieve displays list
    if (CGGetActiveDisplayList (VID_MAX_DISPLAYS, gVidDisplayList, &gVidDisplayCount) != CGDisplayNoErr)
    {
        Sys_Error ("Can\'t build display list!");
    }
    
    // get some symbols (10.2+ only?):
    qCGDisplayIsMain = Sys_GetProcAddress ("CGDisplayIsMain", NO);
    qCGDisplayIsBuiltin = Sys_GetProcAddress ("CGDisplayIsBuiltin", NO);
    
    // add the displays to the popup:
    [displayPopUp removeAllItems];
    for (i = 0; i < gVidDisplayCount; i++)
    {
        // try to provide descriptive menu items (10.2+):
        if (qCGDisplayIsMain != NULL && qCGDisplayIsMain (gVidDisplayList[i]) == YES)
        {
            myDisplayName = [NSString stringWithString: @"Main"];
        }
        else
        {
            myDisplayName = [NSString stringWithFormat: @"%d", i];
        }
        
        if (qCGDisplayIsBuiltin != NULL && qCGDisplayIsBuiltin (gVidDisplayList[i]) == YES)
        {
            myDisplayName = [myDisplayName stringByAppendingString: @" (built in)"];
        }
        
        [displayPopUp addItemWithTitle: myDisplayName];
    }

    [displayPopUp selectItemAtIndex: 0];
}

//____________________________________________________________________________________________________________scanMediaThread:

- (void) scanMediaThread: (id) theSender
{
    SYS_DURING
    {
        // scan for media files:
        CDAudio_GetTrackList ();
        
        // post a notification to the main thread:
        [[NSDistributedNotificationCenter defaultCenter] postNotificationName: @"Fire Frame Timer" object:NULL];
        
        // job done, good bye!
        [NSThread exit];
    }
    SYS_HANDLER;
}

//____________________________________________________________________________________________________________setupParameterUI

- (void) setupParameterUI:  (NSUserDefaults *) theDefaults
{
    // check if the user passed parameters from the command line or by dragging a mod:
    if (gSysArgCount > 1)
    {
        NSString	*myParameters;

        // someone passed command line parameters:
        myParameters = [[[NSString alloc] init] autorelease];

        if (myParameters != NULL)
        {
            SInt32	i;
            
            for (i = 1; i < gSysArgCount; i++)
            {
                // surround the string by ", if it contains spaces:
                if (strchr (gSysArgValues[i], ' '))
                {
                    myParameters = [myParameters stringByAppendingFormat: @"\"%s\" ", gSysArgValues[i]];
                }
                else
                {
                    myParameters = [myParameters stringByAppendingFormat: @"%s", gSysArgValues[i]];
                }
                
                // add a space if this was not the last parameter:
                if (i != gSysArgCount - 1)
                {
                    myParameters = [myParameters stringByAppendingString: @" "];
                }
            }

            // display the current parameters:
            [parameterTextField setStringValue: myParameters];
        }

        //don't allow changes:
        [parameterCheckBox setEnabled: NO];
        [parameterTextField setEnabled: NO];
    }
    else
    {
        BOOL	myParametersEnabled;
        
        // get the default command line parameters:
        myParametersEnabled = [theDefaults boolForKey: DEFAULT_USE_PARAMETERS];
        [parameterTextField setStringValue: [theDefaults stringForKey: DEFAULT_PARAMETERS]];
        [parameterCheckBox setState: myParametersEnabled];
        [parameterCheckBox setEnabled: YES];
        [self toggleParameterTextField: NULL];        
    }
}

//________________________________________________________________________________________________________________setupDialog:

- (void) setupDialog: (NSTimer *) theTimer
{
    SYS_DURING
    {
        NSUserDefaults 	*myDefaults = NULL;
        UInt32		myDefaultDisplay;

#if defined (GLQUAKE)

        NSString	*myDescriptionStr = NULL,
                        *myDefaultModeStr = NULL;
        UInt32		i = 0,
                        myDefaultColors,
                        myDefaultSamples;

#endif /* GLQUAKE */

        // don't allow the "run" AppleScript command to be executed anymore:
        [self enableAppleScriptRun: NO];
    
        // set the URL at the FDLinkView:
        [linkView setURLString: EZQUAKE_URL];
    
        myDefaults = [NSUserDefaults standardUserDefaults];
    
        // build the display list:
        [self buildDisplayList];
    
        // set up the displays popup:
        myDefaultDisplay = [[myDefaults stringForKey: DISPLAY_DEFAULT] intValue];
        if (myDefaultDisplay > gVidDisplayCount)
        {
            myDefaultDisplay = 0;
        }
        [displayPopUp selectItemAtIndex: myDefaultDisplay];
        if (gVidDisplayCount <= 1)
        {
            [displayPopUp setEnabled: NO];
            [fadeAllCheckBox setEnabled: NO];
        }
        else
        {
            [displayPopUp setEnabled: YES];
            [fadeAllCheckBox setEnabled: YES];
        }

#if defined (GLQUAKE)

        // set up the colors popup:
        myDefaultColors = [[myDefaults stringForKey: DEFAULT_GL_COLORS] intValue];
        if (myDefaultColors > 1)
        {
            myDefaultDisplay = 1;
        }
        [colorsPopUp selectItemAtIndex: myDefaultColors];
    
        // build the resolution list:
        [self buildResolutionList: nil];
    
        // setup the modes popup:
        myDefaultModeStr = [myDefaults stringForKey: DEFAULT_GL_DISPLAY_MODE];
        while (i < [mModeList count])
        {
            myDescriptionStr = [self displayModeToString: [mModeList objectAtIndex: i]];
            if ([myDefaultModeStr isEqualToString: myDescriptionStr])
            {
                [modePopUp selectItemAtIndex: i];
                break;
            }
            i++;
        }
    
        // setup the samples popup:
        myDefaultSamples = ([[myDefaults stringForKey: DEFAULT_GL_SAMPLES] intValue]) >> 2;
        if (myDefaultSamples > 2)
            myDefaultSamples = 2;
        if ([samplesPopUp isEnabled] == NO)
            myDefaultSamples = 0;
        [samplesPopUp selectItemAtIndex: myDefaultSamples];
    
        // setup checkboxes:
        [fullscreenCheckBox setState: [myDefaults boolForKey: DEFAULT_GL_FULLSCREEN]];
        [self toggleColorsEnabled: self];
    
#endif /* GLQUAKE */

        [fadeAllCheckBox setState: [myDefaults boolForKey: FADE_ALL_DEFAULT]];
    
        // prepare the "MP3 path" textfield:
        [mp3TextField setStringValue: [myDefaults stringForKey: DEFAULT_MP3_PATH]];
        [mp3CheckBox setState: [myDefaults boolForKey: DEFAULT_USE_MP3]];
        [self toggleMP3Playback: self];
    
        [optionCheckBox setState: [myDefaults boolForKey: OPTION_KEY_DEFAULT]];
        
        // setup command line options:
        [self setupParameterUI: myDefaults];
        
        if ([optionCheckBox state] == NO || ([optionCheckBox state] == YES && mOptionPressed == YES))
        {
            // show the startup dialog:
            [settingsWindow center];
            [settingsWindow makeKeyAndOrderFront: nil];
        }
        else
        {
            // start the game immediately:
            [self newGame: nil];
        }
    }
    SYS_HANDLER;
}

//_______________________________________________________________________________________________saveCheckBox:initial:default:

- (void) saveCheckBox: (NSButton *) theButton initial: (NSString *) theInitial
              default: (NSString *) theDefault userDefaults: (NSUserDefaults *) theUserDefaults
{
    // has our checkbox the initial value? if, delete from defaults::
    if ([theButton state] == [self isEqualTo: theInitial])
    {
        [theUserDefaults removeObjectForKey: theDefault];
    }
    else
    {
        // write to defaults:
        if ([theButton state] == YES)
        {
            [theUserDefaults setObject: @"YES" forKey: theDefault];
        }
        else
        {
            [theUserDefaults setObject: @"NO" forKey: theDefault];
        }
    }
}

//_________________________________________________________________________________________________saveString:initial:default:

- (void) saveString: (NSString *) theString initial: (NSString *) theInitial
            default: (NSString *) theDefault userDefaults: (NSUserDefaults *) theUserDefaults
{
    // has our popup menu the initial value? if, delete from defaults:
    if ([theString isEqualToString: theInitial])
    {
        [theUserDefaults removeObjectForKey: theDefault];
    }
    else
    {
        // write to defaults:
        [theUserDefaults setObject: theString forKey: theDefault];
    }
}

//__________________________________________________________________________________________________________________isEqualTo:

- (BOOL) isEqualTo: (NSString *) theString
{
    // just some boolean str compare:
    return [theString isEqualToString:@"YES"];
}

//________________________________________________________________________________________________________displayModeToString:

- (NSString *) displayModeToString: (NSDictionary *) theDisplayMode
{
    // generate a display mode string with the format: "(width)x(height) (frequency)Hz":
    return ([NSString stringWithFormat: @"%dx%d %dHz",
                      [[theDisplayMode objectForKey: (NSString *)kCGDisplayWidth] intValue],
                      [[theDisplayMode objectForKey: (NSString *)kCGDisplayHeight] intValue],
                      [[theDisplayMode objectForKey: (NSString *)kCGDisplayRefreshRate] intValue]]);
}

//_______________________________________________________________________________________________________________renderFrame:

- (void) renderFrame: (NSTimer *) theTimer
{
#ifndef QUAKE_WORLD
    extern SInt32		vcrFile;
    extern SInt32		recording;
#endif /* QUAKE_WORLD */
    static double		myNewFrameTime,
						myFrameTime;

    if ([NSApp isHidden] == YES)
    {
        return;
    }
    
#ifdef QUAKE_WORLD

    myNewFrameTime	= Sys_DoubleTime ();
    myFrameTime		= myNewFrameTime - mOldFrameTime;
    mOldFrameTime	= myNewFrameTime;

#else

    myNewFrameTime = Sys_FloatTime();
    myFrameTime = myNewFrameTime - mOldFrameTime;
    
#if 0
    if (cls.state == ca_dedicated)
    {
        if (myFrameTime < sys_ticrate.value && (vcrFile == -1 || recording))
        {
            usleep(1);
            return;
        }
        myFrameTime = sys_ticrate.value;
    }
    
    if (myFrameTime > sys_ticrate.value * 2)
    {
        mOldFrameTime = myNewFrameTime;
    }
    else
#endif
    {
        mOldFrameTime += myFrameTime;
    }
    
#endif /* QUAKE_WORLD */

    // finally do the frame:
    Host_Frame (myFrameTime);
}

//___________________________________________________________________________________________________________installFrameTimer

- (void) installFrameTimer
{
    if (mFrameTimer == NULL)
    {
         // we may not set the timer interval too small, otherwise we wouldn't get AppleScript commands. odd eh?
        mFrameTimer = [NSTimer scheduledTimerWithTimeInterval: 0.0003f //0.000001f
                                                       target: self
                                                     selector: @selector (renderFrame:)
                                                     userInfo: NULL
                                                      repeats: YES];
        
        if (mFrameTimer == NULL)
        {
            Sys_Error ("Failed to install the renderer loop!");
        }
    }
}

//_____________________________________________________________________________________________________________fireFrameTimer:

- (void) fireFrameTimer: (NSNotification *) theNotification
{
#if 0
    quakeparms_t    myParameters;
#endif

    // close the media scan window:
    [mediascanProgressIndicator stopAnimation: self];
    [mediascanWindow close];
    [[NSNotificationCenter defaultCenter] removeObserver: self name: @"Fire Frame Timer" object: NULL];
    mMediaScanCanceled = NO;
    
    [pasteMenuItem setTarget: self];
    [pasteMenuItem setAction: @selector (pasteString:)];

    // prepare host init:
    signal (SIGFPE, SIG_IGN);
#if 0
    memset (&myParameters, 0x00, sizeof (myParameters));

    COM_InitArgv (gSysArgCount, gSysArgValues);

    myParameters.argc = com_argc;
    myParameters.argv = com_argv;

#ifdef GLQUAKE
    myParameters.memsize = 16*1024*1024*2;
#else
    myParameters.memsize = 8*1024*1024*2;
#endif /* GLQUAKE */
    j = COM_CheckParm ("-mem");
    if (j)
    {
        myParameters.memsize = (int) (Q_atof (com_argv[j + 1]) * 1024 * 1024);
    }
    
    myParameters.membase = malloc (myParameters.memsize);
    myParameters.basedir = QUAKE_BASE_PATH;
    
    fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);
    Host_Init (&myParameters);
#endif
    Host_Init (gSysArgCount, gSysArgValues, 8*1024*1024*2);
    
    [self setHostInitialized: YES];
    [NSApp setServicesProvider: self];

    // disable keyboard repeat:
    IN_SetF12EjectEnabled (NO);

    // some nice console output for credits:

#define MACOSX_VERSION 10.4

#ifdef QUAKE_WORLD

    mOldFrameTime = Sys_DoubleTime ();

#else

    mOldFrameTime = Sys_FloatTime ();

#endif /* QUAKE_WORLD */

    // did we receive an AppleScript command?
    if (mRequestedCommands != NULL)
    {
        while ([mRequestedCommands count] > 0)
        {
            NSString	*myCommand = [mRequestedCommands objectAtIndex: 0];

            Cbuf_AddText (va ("%s\n", [myCommand cString]));
            [mRequestedCommands removeObjectAtIndex: 0];
        }
        [mRequestedCommands release];
        mRequestedCommands = NULL;
    }

    [self installFrameTimer];
}

@end

//_________________________________________________________________________________________________________________________eOF
