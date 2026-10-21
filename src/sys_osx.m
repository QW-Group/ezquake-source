#import <AppKit/Appkit.h>
#import <Foundation/Foundation.h>
#import <SDL3/SDL.h>
#import "common.h"

@interface URL : NSObject
- (void)getURL:(NSAppleEventDescriptor*)event withReplyEvent:(NSAppleEventDescriptor*)reply;
@end

@implementation URL
- (void)getURL:(NSAppleEventDescriptor*)event withReplyEvent:(NSAppleEventDescriptor*)reply
{
	Cbuf_AddText(va("qwurl \"%s\"\n", [[[event paramDescriptorForKeyword:keyDirectObject] stringValue] cStringUsingEncoding:NSASCIIStringEncoding]));
}
@end

static URL *url = NULL;

static void EzClearBookmarkedDirectory(void);

void init_macos_extras(void)
{
	if (url == NULL) {
		url = [[URL alloc] init];
	}

	[[NSAppleEventManager sharedAppleEventManager] setEventHandler:url andSelector:@selector(getURL:withReplyEvent:) forEventClass:kInternetEventClass andEventID:kAEGetURL];

	Cmd_AddCommand("sys_forget_sandbox", EzClearBookmarkedDirectory);
}

static void EzSimpleAlert(const char *title, const char *message)
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, title, message, NULL);
}

static BOOL EzIsValidEzQuakeDirectory(const char *directory)
{
	if (!SDL_GetPathInfo(va("%s/id1/pak0.pak", directory), NULL)) {
		EzSimpleAlert("Game directory not found", "Select a directory containing 'id1/pak0.pak'.");
		return NO;
	}

	return YES;
}

typedef struct {
	char path[MAX_OSPATH];
	BOOL selected;
	SDL_AtomicInt done;
} EzFolderChoice;

static void SDLCALL EzFolderChosen(void *userdata, const char * const *filelist, int filter)
{
	EzFolderChoice *choice = (EzFolderChoice *)userdata;

	if (filelist && filelist[0]) {
		strlcpy(choice->path, filelist[0], sizeof(choice->path));
		choice->selected = YES;
	}

	SDL_SetAtomicInt(&choice->done, 1);
}

// Returns NO when the user cancels
static BOOL EzChooseFolder(char *path, int length)
{
	EzFolderChoice choice = { 0 };

	SDL_ShowOpenFolderDialog(EzFolderChosen, &choice, NULL, NULL, false);

	// Without a parent window the dialog is modal and has usually answered already
	while (!SDL_GetAtomicInt(&choice.done)) {
		SDL_PumpEvents();
		SDL_Delay(10);
	}

	if (choice.selected) {
		strlcpy(path, choice.path, length);
	}

	return choice.selected;
}

static BOOL EzCheckExistingSettings(char *basedir, int length)
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	NSData *bookmarkData = [defaults objectForKey:@"basedir"];
	NSString *version = [defaults stringForKey:@"version"];
	BOOL bookmarkDataIsStale = NO;
	NSError *error = nil;
	NSURL *resolvedURL;

	if (!(bookmarkData && version)) {
		return NO;
	}

	resolvedURL = [NSURL URLByResolvingBookmarkData:bookmarkData
	                                        options:NSURLBookmarkResolutionWithSecurityScope
	                                  relativeToURL:nil
	                            bookmarkDataIsStale:&bookmarkDataIsStale
	                                          error:&error];
	if (bookmarkDataIsStale || error) {
		NSString *msg = error.localizedDescription ? error.localizedDescription : @"Bookmark data is stale";
		EzSimpleAlert("Error resolving bookmark", [msg UTF8String]);
		return NO;
	}

	if (![resolvedURL startAccessingSecurityScopedResource]) {
		EzSimpleAlert("Failed to start accessing security-scoped resource", "");
		return NO;
	}

	const char *resolvedPath = [[resolvedURL path] UTF8String];
	if (EzIsValidEzQuakeDirectory(resolvedPath)) {
		strlcpy(basedir, resolvedPath, length);
		return YES;
	}

	return NO;
}

void SysLibrarySupportDir(char *basedir, int length)
{
	@autoreleasepool {
		// Need to temporarily start SDL here to not break initialization of application which
		// prevents both the dummy menu, and more importantly mouseMovedHandler from working.
		SDL_Window *window = SDL_CreateWindow(
				"ezQuake", 0, 0, SDL_WINDOW_HIDDEN
		);

		if (EzCheckExistingSettings(basedir, length)) {
			SDL_DestroyWindow(window);
			return;
		}

		char path[MAX_OSPATH];
		while (EzChooseFolder(path, sizeof(path))) {
			if (!EzIsValidEzQuakeDirectory(path)) {
				continue;
			}

			// The sandbox lets this process use the chosen directory, so it can be bookmarked
			NSURL *directoryURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path] isDirectory:YES];
			NSError *error = nil;
			NSData *bookmarkData = [directoryURL bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
			                              includingResourceValuesForKeys:nil
			                                               relativeToURL:nil
			                                                       error:&error];
			if (error) {
				EzSimpleAlert("Error creating bookmark", [error.localizedDescription UTF8String]);
				break;
			}

			[[NSUserDefaults standardUserDefaults] setObject:bookmarkData forKey:@"basedir"];
			[[NSUserDefaults standardUserDefaults] setObject:@VERSION forKey:@"version"];

			strlcpy(basedir, path, length);
			break;
		}

		SDL_DestroyWindow(window);
	}
}

static void EzClearBookmarkedDirectory(void) {
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

	NSData *bookmarkData = [defaults objectForKey:@"basedir"];
	if (bookmarkData) {
		NSError *error = nil;
		BOOL bookmarkDataIsStale = NO;

		NSURL *resolvedURL = [NSURL URLByResolvingBookmarkData:bookmarkData
		                                               options:NSURLBookmarkResolutionWithSecurityScope
		                                         relativeToURL:nil
		                                   bookmarkDataIsStale:&bookmarkDataIsStale
		                                                 error:&error];
		if (!error && resolvedURL) {
			[resolvedURL stopAccessingSecurityScopedResource];
		}

		[defaults removeObjectForKey:@"basedir"];
		[defaults removeObjectForKey:@"version"];
		[defaults synchronize];

		Con_Printf("Sandbox directory cleared.\n");
	} else {
		Con_Printf("No sandbox directory selected.\n");
	}
}
