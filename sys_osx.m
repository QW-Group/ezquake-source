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

void init_url_handler()
{
	if (url == NULL)
	{
		url = [[URL alloc] init];
	}
	
	[[NSAppleEventManager sharedAppleEventManager] setEventHandler:url andSelector:@selector(getURL:withReplyEvent:) forEventClass:kInternetEventClass andEventID:kAEGetURL];
}
