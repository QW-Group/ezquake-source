//_________________________________________________________________________________________________________________________nFO
// "FDGLScreenshot.m" - Save screenshots (of the current OpenGL context) to various image formats.
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2002-2006 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
//
// IMPORTANT: THIS PIECE OF CODE MAY NOT BE USED WITH SHAREWARE OR COMMERCIAL APPLICATIONS WITHOUT PERMISSION.
//	          IT IS PROVIDED "AS IS" AND IS STILL COPYRIGHTED BY FRUITZ OF DOJO.
//
//____________________________________________________________________________________________________________________iNCLUDES

#import <OpenGL/gl.h>
#import "FDGLScreenshot.h"

//______________________________________________________________________________________________________________iMPLEMENTATION

@implementation FDGLScreenshot

//_________________________________________________________________________________________________________writeToFile:ofType:

+ (BOOL) writeToFile: (NSString *) theFile ofType: (NSBitmapImageFileType) theType
{
    GLint		myViewport[4];
    BOOL		myResult = NO;
    
    // get the window dimensions of the current OpenGL context:
    glGetIntegerv (GL_VIEWPORT, myViewport);
    
    if (myViewport[2] > 0 && myViewport[3] > 0)
    {
        UInt32	myWidth		= myViewport[2],
                myHeight	= myViewport[3],
                myRowbytes	= myWidth * 3,
                i;
        UInt8	*myBuffer	= nil,
                *myBottom	= nil,
                *myTop		= nil,
                myStore;

        // get memory to cache the current OpenGL scene:
        myBuffer = (UInt8 *) malloc (myRowbytes * myHeight);
        if (myBuffer != nil)
        {
            // copy the current OpenGL context to memory:
            glReadPixels (myViewport[0], myViewport[1], myWidth, myHeight, GL_RGB, GL_UNSIGNED_BYTE, myBuffer);
        
            // mirror the buffer [only vertical]:
            myTop = myBuffer;
            myBottom = myBuffer + myRowbytes * (myHeight - 1);
            while (myTop < myBottom)
            {
                for (i = 0; i < myRowbytes; i++)
                {
                    myStore = myTop[i];
                    myTop[i] = myBottom[i];
                    myBottom[i] = myStore;
                }
                myTop += myRowbytes;
                myBottom -= myRowbytes;
            }
            
            // write the file:
            myResult = [FDGLScreenshot writeToFile: theFile
                                            ofType: theType
                                         fromRGB24: myBuffer
                                          withSize: NSMakeSize ((float) myWidth, (float) myHeight)
                                          rowbytes: myRowbytes];

            // get rid of the buffer:
            free (myBuffer);
        }
    }
    
    return (myResult);
}

//_________________________________________________________________________________________________________________writeToBMP:

+ (BOOL) writeToBMP: (NSString *) theFile
{
    return ([FDGLScreenshot writeToFile: theFile ofType: NSBMPFileType]);
}

//_________________________________________________________________________________________________________________writeToGIF:

+ (BOOL) writeToGIF: (NSString *) theFile
{
    return ([FDGLScreenshot writeToFile: theFile ofType: NSGIFFileType]);
}

//________________________________________________________________________________________________________________writeToJPEG:

+ (BOOL) writeToJPEG: (NSString *) theFile
{
    return ([FDGLScreenshot writeToFile: theFile ofType: NSJPEGFileType]);
}

//_________________________________________________________________________________________________________________writeToPNG:

+ (BOOL) writeToPNG: (NSString *) theFile
{
    return ([FDGLScreenshot writeToFile: theFile ofType: NSPNGFileType]);
}

//________________________________________________________________________________________________________________writeToTIFF:

+ (BOOL) writeToTIFF: (NSString *) theFile
{
    return ([FDGLScreenshot writeToFile: theFile ofType: NSTIFFFileType]);
}

@end

//_________________________________________________________________________________________________________________________eOF
