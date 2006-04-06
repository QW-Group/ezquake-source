//_________________________________________________________________________________________________________________________nFO
// "FDScreenshot.h" - Save screenshots (raw bitmap data) to various image formats.
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2006 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
//
// IMPORTANT: THIS PIECE OF CODE MAY NOT BE USED WITH SHAREWARE OR COMMERCIAL APPLICATIONS WITHOUT PERMISSION.
//	      IT IS PROVIDED "AS IS" AND IS STILL COPYRIGHTED BY FRUITZ OF DOJO.
//
//____________________________________________________________________________________________________________________iNCLUDES

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

//___________________________________________________________________________________________________________________iNTERFACE

@interface FDScreenshot : NSObject
{
}

+ (BOOL) writeToFile: (NSString *) theFile
              ofType: (NSBitmapImageFileType) theType
           fromRGB24: (void *) theData
            withSize: (NSSize) theSize
            rowbytes: (SInt32) theRowbytes;

+ (BOOL) writeToBMP: (NSString *) theFile
          fromRGB24: (void *) theData
           withSize: (NSSize) theSize
           rowbytes: (SInt32) theRowbytes;

+ (BOOL) writeToGIF: (NSString *) theFile
          fromRGB24: (void *) theData
           withSize: (NSSize) theSize
           rowbytes: (SInt32) theRowbytes;

+ (BOOL) writeToJPEG: (NSString *) theFile
           fromRGB24: (void *) theData
            withSize: (NSSize) theSize
            rowbytes: (SInt32) theRowbytes;

+ (BOOL) writeToPNG: (NSString *) theFile
          fromRGB24: (void *) theData
           withSize: (NSSize) theSize
           rowbytes: (SInt32) theRowbytes;

+ (BOOL) writeToTIFF: (NSString *) theFile
           fromRGB24: (void *) theData
            withSize: (NSSize) theSize
            rowbytes: (SInt32) theRowbytes;

@end

//_________________________________________________________________________________________________________________________eOF
