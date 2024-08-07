#import <AppKit/Appkit.h>
#import <Foundation/Foundation.h>
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

void init_url_handler(void)
{
	if (url == NULL)
	{
		url = [[URL alloc] init];
	}
	
	[[NSAppleEventManager sharedAppleEventManager] setEventHandler:url andSelector:@selector(getURL:withReplyEvent:) forEventClass:kInternetEventClass andEventID:kAEGetURL];
}

void SysLibrarySupportDir(char *basedir, int length)
{
	@autoreleasepool {
		NSArray *paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
		NSString *app_support_path = [paths firstObject];
		NSString *ezquake_path = [app_support_path stringByAppendingPathComponent:@"ezQuake"];

		const char *raw = [ezquake_path UTF8String];
		strncpy(basedir, raw, length - 1);
		basedir[length - 1] = '\0';
    }
}