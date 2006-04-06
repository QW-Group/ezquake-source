//_________________________________________________________________________________________________________________________nFO
// "QuakeApplication.h" - required for event and AppleScript handling.
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2006 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
// Quakeª is copyrighted by id software		[http://www.idsoftware.com].
//
//__________________________________________________________________________________________________iNTERFACE_QuakeApplication

@interface QuakeApplication : NSApplication

- (void) sendEvent: (NSEvent *) theEvent;
- (void) sendSuperEvent: (NSEvent *) theEvent;
- (void) handleRunCommand: (NSScriptCommand *) theCommand;
- (void) handleConsoleCommand: (NSScriptCommand *) theCommand;

@end

//_________________________________________________________________________________________________________________________eOF
