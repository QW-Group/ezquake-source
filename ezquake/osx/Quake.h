//_________________________________________________________________________________________________________________________nFO
// "Quake.h" - the controller.
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2006 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
// Quakeª is copyrighted by id software		[http://www.idsoftware.com].
//
//____________________________________________________________________________________________________________________iNCLUDES

#pragma mark =Includes=

#import "FDLinkView.h"

#pragma mark -

//_____________________________________________________________________________________________________________________dEFINES

#pragma mark =Defines=

#define EZQUAKE_URL					@"http://ezquake.sf.net/"

#define	DEFAULT_BASE_PATH			@"Quake ID1 Path"
#define	DEFAULT_OPTION_KEY			@"Quake Dialog Requires Option Key"
#define DEFAULT_USE_MP3				@"Quake Use MP3"
#define DEFAULT_MP3_PATH			@"Quake MP3 Path"
#define DEFAULT_USE_PARAMETERS		@"Quake Use Command-Line Parameters"
#define DEFAULT_PARAMETERS			@"Quake Command-Line Parameters"
#define DEFAULT_FADE_ALL			@"Quake Fade All Displays"
#define DEFAULT_DISPLAY				@"Quake Display"
#define DEFAULT_WINDOW_WIDTH		@"Quake Window Width"
#define DEFAULT_CUR_WINDOW_WIDTH	@"Quake Current Window Width"
#define DEFAULT_GL_DISPLAY			@"GLQuake Display"
#define	DEFAULT_GL_DISPLAY_MODE		@"GLQuake Display Mode"
#define DEFAULT_GL_COLORS			@"GLQuake Display Depth"
#define	DEFAULT_GL_SAMPLES			@"GLQuake Samples"
#define DEFAULT_GL_FADE_ALL			@"GLQuake Fade All Displays"
#define DEFAULT_GL_FULLSCREEN		@"GLQuake Fullscreen"
#define	DEFAULT_GL_OPTION_KEY		@"GLQuake Dialog Requires Option Key"

#define INITIAL_BASE_PATH			@"id1"
#define	INITIAL_OPTION_KEY			@"NO"
#define INITIAL_USE_MP3				@"NO"
#define INITIAL_MP3_PATH			@""
#define INITIAL_USE_PARAMETERS		@"NO"
#define INITIAL_PARAMETERS			@""
#define INITIAL_DISPLAY				@"0"
#define INITIAL_FADE_ALL			@"YES"
#define INITIAL_WINDOW_WIDTH		@"0"
#define INITIAL_CUR_WINDOW_WIDTH	@"0"
#define INITIAL_GL_DISPLAY			@"0"
#define	INITIAL_GL_DISPLAY_MODE		@"640x480 67Hz"
#define INITIAL_GL_COLORS			@"0"
#define	INITIAL_GL_SAMPLES			@"0"
#define INITIAL_GL_FADE_ALL			@"YES"
#define	INITIAL_GL_FULLSCREEN		@"YES"
#define	INITIAL_GL_OPTION_KEY		@"NO"

#define	SYS_ABOUT_TOOLBARITEM		@"Quake About Toolbaritem"
#define SYS_VIDEO_TOOLBARITEM		@"Quake Displays Toolbaritem"
#define	SYS_AUDIO_TOOLBARITEM		@"Quake Sound Toolbaritem"
#define	SYS_PARAM_TOOLBARITEM		@"Quake Parameters Toolbaritem"
#define	SYS_START_TOOLBARITEM		@"Quake Start Toolbaritem"

#if defined (GLQUAKE)
#define	OPTION_KEY_DEFAULT			DEFAULT_GL_OPTION_KEY
#define	OPTION_KEY_INITIAL			INITIAL_GL_OPTION_KEY
#define FADE_ALL_DEFAULT			DEFAULT_GL_FADE_ALL
#define FADE_ALL_INITIAL			DEFAULT_GL_FADE_ALL
#define DISPLAY_DEFAULT				DEFAULT_GL_DISPLAY
#define DISPLAY_INITIAL				INITIAL_GL_DISPLAY
#else
#define	OPTION_KEY_DEFAULT			DEFAULT_OPTION_KEY
#define	OPTION_KEY_INITIAL			INITIAL_OPTION_KEY
#define FADE_ALL_DEFAULT			DEFAULT_FADE_ALL
#define FADE_ALL_INITIAL			DEFAULT_FADE_ALL
#define DISPLAY_DEFAULT				DEFAULT_DISPLAY
#define DISPLAY_INITIAL				INITIAL_DISPLAY
#endif /* GLQUAKE */

#pragma mark -

//_____________________________________________________________________________________________________________iNTERFACE_Quake

#pragma mark =Interface=

@interface Quake : NSObject
{
    IBOutlet NSWindow				*mediascanWindow;
    IBOutlet NSTextField			*mediascanTextField;
    IBOutlet NSProgressIndicator	*mediascanProgressIndicator;

    IBOutlet NSView					*mp3HelpView;

    IBOutlet NSView					*aboutView;
    IBOutlet NSView					*audioView;
    IBOutlet NSView					*parameterView;

    IBOutlet NSView					*videoView;
        
    IBOutlet NSPopUpButton			*displayPopUp;
    IBOutlet NSButton				*fadeAllCheckBox;
    
#if defined (GLQUAKE)
    IBOutlet NSPopUpButton			*modePopUp;
    IBOutlet NSPopUpButton			*colorsPopUp;
    IBOutlet NSPopUpButton			*samplesPopUp;
    IBOutlet NSButton				*fullscreenCheckBox;

    NSMutableArray					*mModeList;
#endif /* GLQUAKE */

    IBOutlet NSButton				*mp3CheckBox;
    IBOutlet NSButton				*mp3Button;
    IBOutlet NSTextField			*mp3TextField;

    IBOutlet NSButton				*optionCheckBox;
    IBOutlet NSButton				*parameterCheckBox;
    IBOutlet NSTextField			*parameterTextField;
    IBOutlet NSMenuItem				*pasteMenuItem;
    IBOutlet NSWindow				*settingsWindow;
    IBOutlet FDLinkView				*linkView;

    NSMutableDictionary				*mToolbarItems;
    NSView							*mEmptyView;
    NSTimer							*mFrameTimer;
    NSMutableArray					*mRequestedCommands;
    NSString						*mMP3Folder,
									*mModFolder;
    NSDate							*mDistantPast;
    double							mOldFrameTime;
    BOOL							mOptionPressed,
									mDenyDrag,
									mHostInitialized,
									mAllowAppleScriptRun,
									mMediaScanCanceled;
}

+ (void) initialize;
- (void) dealloc;

- (void) stringToParameters: (NSString *) theString;
- (void) requestCommand: (NSString *) theCommand;
- (BOOL) wasDragged;
- (BOOL) hostInitialized;
- (void) setHostInitialized: (BOOL) theState;
- (BOOL) allowAppleScriptRun;
- (void) enableAppleScriptRun: (BOOL) theState;
- (NSDate *) distantPast;
- (NSString *) modFolder;
- (NSString *) mediaFolder;
- (BOOL) abortMediaScan;

@end

//_________________________________________________________________________________________________________________________eOF
