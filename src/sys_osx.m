#import <AppKit/Appkit.h>
#import <Foundation/Foundation.h>
#import <SDL.h>
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

static void EzSimpleAlert(NSString *title, NSString *message)
{
	NSAlert *alert = [[NSAlert alloc] init];
	[alert setMessageText:title];
	[alert setInformativeText:message];
	[alert addButtonWithTitle:@"Ok"];
	[alert runModal];
}

static BOOL EzIsValidEzQuakeDirectory(NSString *directoryPath)
{
	NSString *id1Path = [directoryPath stringByAppendingPathComponent:@"id1"];
	NSString *pakFilePath = [id1Path stringByAppendingPathComponent:@"pak0.pak"];

	BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath:pakFilePath];
	if (!exists) {
		EzSimpleAlert(@"Game directory not found", @"Select a directory containing 'id1/pak0.pak'.");
	}

	return exists;
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
		EzSimpleAlert(@"Error resolving bookmark", msg);
		return NO;
	}

	if (![resolvedURL startAccessingSecurityScopedResource]) {
		EzSimpleAlert(@"Failed to start accessing security-scoped resource", @"");
		return NO;
	}

	NSString *resolvedPath = [resolvedURL path];
	if (EzIsValidEzQuakeDirectory(resolvedPath)) {
		strlcpy(basedir, [resolvedPath UTF8String], length);
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
				"ezQuake", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0, SDL_WINDOW_HIDDEN
		);

		if (EzCheckExistingSettings(basedir, length)) {
			SDL_DestroyWindow(window);
			return;
		}

		NSOpenPanel *openPanel = [NSOpenPanel openPanel];
		[openPanel setCanChooseFiles:NO];
		[openPanel setCanChooseDirectories:YES];
		[openPanel setAllowsMultipleSelection:NO];
		[openPanel setMessage:@"Select game directory containing id1/pak0.pak:"];

		BOOL validDirectorySelected = NO;
		while (!validDirectorySelected) {
			if ([openPanel runModal] != NSModalResponseOK) {
				break;
			}

			NSURL *directoryURL = [[openPanel URLs] objectAtIndex:0];
			if (!EzIsValidEzQuakeDirectory([directoryURL path])) {
				continue;
			}

			NSError *error = nil;
			NSData *bookmarkData = [directoryURL bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
			                              includingResourceValuesForKeys:nil
			                                               relativeToURL:nil
			                                                       error:&error];
			if (error) {
				EzSimpleAlert(@"Error creating bookmark", error.localizedDescription);
				break;
			}

			[[NSUserDefaults standardUserDefaults] setObject:bookmarkData forKey:@"basedir"];
			[[NSUserDefaults standardUserDefaults] setObject:@VERSION forKey:@"version"];

			validDirectorySelected = YES;

			NSString *resolvedPath = [directoryURL path];
			strlcpy(basedir, [resolvedPath UTF8String], length);
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
