//_________________________________________________________________________________________________________________________nFO
// "FDScreenshot.m" - Save screenshots (raw bitmap data) to various image formats.
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2002-2006 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
//
// IMPORTANT: THIS PIECE OF CODE MAY NOT BE USED WITH SHAREWARE OR COMMERCIAL APPLICATIONS WITHOUT PERMISSION.
//	          IT IS PROVIDED "AS IS" AND IS STILL COPYRIGHTED BY FRUITZ OF DOJO.
//
// TODO: Add functions to save the content of a specific display (for software renderer based games).
//____________________________________________________________________________________________________________________iNCLUDES

#import "FDScreenshot.h"

//_________________________________________________________________________________________________________iNTERFACE_(Private)

@interface FDScreenshot (Private)

+ (NSBitmapImageRep *) imageFromRGB24: (void *) theBitmap withSize: (NSSize) theSize rowbytes: (SInt32) theRowBytes;

@end

//____________________________________________________________________________________________________iMPLEMENTATION_(Private)

@implementation FDScreenshot (Private)

//___________________________________________________________________________________________imageFromRGB24:withSize:rowbytes:

+ (NSBitmapImageRep *) imageFromRGB24: (void *) theBitmap withSize: (NSSize) theSize rowbytes: (SInt32) theRowBytes
{
    UInt8 *				myPlanes[5] = {(UInt8 *) theBitmap, NULL, NULL, NULL, NULL};
    NSBitmapImageRep *	myImage;
    
    myImage = [[[NSBitmapImageRep alloc] initWithBitmapDataPlanes: myPlanes
                                                       pixelsWide: theSize.width
                                                       pixelsHigh: theSize.height
                                                    bitsPerSample: 8
                                                  samplesPerPixel: 3
                                                         hasAlpha: NO
                                                         isPlanar: NO
                                                   colorSpaceName: NSDeviceRGBColorSpace
                                                      bytesPerRow: theRowBytes
                                                     bitsPerPixel: 24] autorelease];
    return (myImage);
}

@end

//______________________________________________________________________________________________________________iMPLEMENTATION

@implementation FDScreenshot

//_____________________________________________________________________________writeToFile:ofType:fromRGB24:withSize:rowbytes:

+ (BOOL) writeToFile: (NSString *) theFile
              ofType: (NSBitmapImageFileType) theType
           fromRGB24: (void *) theData
            withSize: (NSSize) theSize
            rowbytes: (SInt32) theRowbytes
{
    NSBitmapImageRep *	myImageRep	= [FDScreenshot imageFromRGB24: theData withSize: theSize rowbytes: theRowbytes];
    NSData *			myData		= [myImageRep representationUsingType: theType properties: NULL];

    return ([myData writeToFile: theFile atomically: YES]);
}

//_____________________________________________________________________________________writeToPNG:fromRGB24:withSize:rowbytes:

+ (BOOL) writeToBMP: (NSString *) theFile
          fromRGB24: (void *) theData
           withSize: (NSSize) theSize
           rowbytes: (SInt32) theRowbytes
{
    return ([FDScreenshot writeToFile: theFile
                               ofType: NSBMPFileType
                            fromRGB24: theData
                             withSize: theSize
                             rowbytes: theRowbytes]);
}

//_____________________________________________________________________________________writeToGIF:fromRGB24:withSize:rowbytes:

+ (BOOL) writeToGIF: (NSString *) theFile
          fromRGB24: (void *) theData
           withSize: (NSSize) theSize
           rowbytes: (SInt32) theRowbytes
{
    return ([FDScreenshot writeToFile: theFile
                               ofType: NSGIFFileType 
                            fromRGB24: theData
                             withSize: theSize
                             rowbytes: theRowbytes]);
}

//____________________________________________________________________________________writeToJPEG:fromRGB24:withSize:rowbytes:

+ (BOOL) writeToJPEG: (NSString *) theFile
           fromRGB24: (void *) theData
            withSize: (NSSize) theSize
            rowbytes: (SInt32) theRowbytes
{
    return ([FDScreenshot writeToFile: theFile
                               ofType: NSJPEGFileType
                            fromRGB24: theData
                             withSize: theSize
                             rowbytes: theRowbytes]);
}

//_____________________________________________________________________________________writeToPNG:fromRGB24:withSize:rowbytes:

+ (BOOL) writeToPNG: (NSString *) theFile
          fromRGB24: (void *) theData
           withSize: (NSSize) theSize
           rowbytes: (SInt32) theRowbytes
{
    return ([FDScreenshot writeToFile: theFile
                               ofType: NSPNGFileType
                            fromRGB24: theData
                             withSize: theSize
                             rowbytes: theRowbytes]);
}

//____________________________________________________________________________________writeToTIFF:fromRGB24:withSize:rowbytes:

+ (BOOL) writeToTIFF: (NSString *) theFile
           fromRGB24: (void *) theData
            withSize: (NSSize) theSize
            rowbytes: (SInt32) theRowbytes
{
    return ([FDScreenshot writeToFile: theFile
                               ofType: NSTIFFFileType
                            fromRGB24: theData
                             withSize: theSize
                             rowbytes: theRowbytes]);
}

@end

//_________________________________________________________________________________________________________________________eOF
