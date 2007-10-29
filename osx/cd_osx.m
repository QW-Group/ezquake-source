//_________________________________________________________________________________________________________________________nFO
// "cd_osx.m" - MacOS X audio CD driver.
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2006 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
// Quakeª is copyrighted by id software		[http://www.idsoftware.com].
//
// Version History:
// v1.1.0: Threaded playback via pthreads (to guarantee playback while moving/resizing the window).
// v1.0.9: Rewritten. Uses now QuickTime for playback.
//	       Added support for MP3 and MP4 [AAC] playback.
// v1.0.3: Fixed an issue with requesting a track number greater than the max number.
// v1.0.1: Added "cdda" as extension for detection of audio-tracks [required by MacOS X v10.1 or later]
// v1.0.0: Initial release.
//____________________________________________________________________________________________________________________iNCLUDES

#pragma mark =Includes=

#import <Cocoa/Cocoa.h>
#import <CoreAudio/AudioHardware.h>
#import <QuickTime/QuickTime.h>
#import <sys/mount.h>
#import <pthread.h>

#import "quakedef.h"
#import "Quake.h"
#import "cd_osx.h"
#import "sys_osx.h"

#pragma mark -

//___________________________________________________________________________________________________________________vARIABLES

#pragma mark =Variables=

extern cvar_t				bgmvolume;

static Movie				gCDController = NULL;
static NSMutableArray *		gCDTrackList;
static char					gCDDevice[MAX_OSPATH];
static volatile BOOL		gCDLoop,
							gCDNextTrack,
							gCDThreadIsRunning = NO,
							gCDThreadShouldExit = NO,
							gCDThreadRequestsNextTrack = NO;
static UInt16				gCDTrackCount,
							gCDCurTrack;
static pthread_t			gCDThread;
static pthread_mutex_t 		gCDThreadMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t		gCDThreadCondition = PTHREAD_COND_INITIALIZER;

#pragma mark -

//_________________________________________________________________________________________________________fUNCTION_pROTOTYPES

#pragma mark =Function Prototypes=

static void		CDAudio_Error (cderror_t theErrorNumber);
static SInt32	CDAudio_StripVideoTracks (Movie theMovie);
static void		CDAudio_SafePath (const char *thePath);
static void		CDAudio_AddTracks2List (NSString *theMountPath, NSArray *theExtensions);
static void *	CDAudio_UpdateThread (void *theParameter);
static void		CDAudio_ExitThread (void);
void		CDAudio_Shutdown (void);
static void 	CD_f (void);

#pragma mark -

//_____________________________________________________________________________________________________________CDAudio_Error()

void	CDAudio_Error (cderror_t theErrorNumber)
{
    if ([[NSApp delegate] mediaFolder] == NULL)
    {
        Com_Printf ("Audio-CD driver: ");
    }
    else
    {
        Com_Printf ("MP3/MP4 driver: ");
    }
    
    switch (theErrorNumber)
    {
        case CDERR_ALLOC_TRACK:
            Com_Printf ("Failed to allocate track!\n");
            break;
        case CDERR_MOVIE_DATA:
            Com_Printf ("Failed to retrieve track data!\n");
            break;
        case CDERR_AUDIO_DATA:
            Com_Printf ("File without audio track!\n");
            break;
        case CDERR_QUICKTIME_ERROR:
            Com_Printf ("QuickTime error!\n");
            break;
        case CDERR_THREAD_ERROR:
            Com_Printf ("Failed to initialize thread!\n");
            break;
        case CDERR_NO_MEDIA_FOUND:
            Com_Printf ("No Audio-CD found.\n");
            break;
        case CDERR_MEDIA_TRACK:
            Com_Printf ("Failed to retrieve media track!\n");
            break;
        case CDERR_MEDIA_TRACK_CONTROLLER:
            Com_Printf ("Failed to retrieve track controller!\n");
            break;
        case CDERR_EJECT:
            Com_Printf ("Can\'t eject Audio-CD!\n");
            break;
        case CDERR_NO_FILES_FOUND:
            if ([[NSApp delegate] mediaFolder] == NULL)
            {
                Com_Printf ("No audio tracks found.\n");
            }
            else
            {
                Com_Printf ("No files found with the extension \'.mp3\' or \'.mp4\'!\n");
            }
            break;
    }
}

//__________________________________________________________________________________________________CDAudio_StripVideoTracks()

SInt32	CDAudio_StripVideoTracks (Movie theMovie)
{
    SInt64 	myTrackCount, i;
    Track 	myCurTrack;
    OSType 	myMediaType;
	
    myTrackCount = GetMovieTrackCount (theMovie);

    for (i = myTrackCount; i >= 1; i--)
    {
        myCurTrack = GetMovieIndTrack (theMovie, i);
        GetMediaHandlerDescription (GetTrackMedia (myCurTrack), &myMediaType, NULL, NULL);
        if (myMediaType != SoundMediaType && myMediaType != MusicMediaType)
        {
            DisposeMovieTrack (myCurTrack);
        }
    }

    return (GetMovieTrackCount (theMovie));
}

//____________________________________________________________________________________________________CDAudio_AddTracks2List()

void	CDAudio_AddTracks2List (NSString *theMountPath, NSArray *theExtensions)
{
    NSFileManager		*myFileManager = [NSFileManager defaultManager];
    
    if (myFileManager != NULL)
    {
        NSDirectoryEnumerator	*myDirEnum = [myFileManager enumeratorAtPath: theMountPath];

        if (myDirEnum != NULL)
        {
            NSString	*myFilePath;
            SInt32	myIndex, myExtensionCount;
            
            myExtensionCount = [theExtensions count];
            
            // get all audio tracks:
            while ((myFilePath = [myDirEnum nextObject]))
            {
                if ([[NSApp delegate] abortMediaScan] == YES)
                {
                    break;
                }

                for (myIndex = 0; myIndex < myExtensionCount; myIndex++)
                {
                    if ([[myFilePath pathExtension] isEqualToString: [theExtensions objectAtIndex: myIndex]])
                    {
                        NSString	*myFullPath = [theMountPath stringByAppendingPathComponent: myFilePath];
                        NSURL		*myMoviePath = [NSURL fileURLWithPath: myFullPath];
                        NSMovie		*myMovie = NULL;
                        
                        myMovie = [[NSMovie alloc] initWithURL: myMoviePath byReference: YES];
                        if (myMovie != NULL)
                        {
                            Movie	myQTMovie = [myMovie QTMovie];
                            
                            if (myQTMovie != NULL)
                            {
                                // add only movies with audiotacks and use only the audio track:
                                if (CDAudio_StripVideoTracks (myQTMovie) > 0)
                                {
                                    [gCDTrackList addObject: myMovie];
                                }
                                else
                                {
                                    CDAudio_Error (CDERR_AUDIO_DATA);
                                }
                            }
                            else
                            {
                                CDAudio_Error (CDERR_MOVIE_DATA);
                            }
                        }
                        else
                        {
                            CDAudio_Error (CDERR_ALLOC_TRACK);
                        }
                    }
                }
            }
        }
    }
    gCDTrackCount = [gCDTrackList count];
}

//__________________________________________________________________________________________________________CDAudio_SafePath()

void	CDAudio_SafePath (const char *thePath)
{
    SInt32	myStrLength = 0;

    if (thePath != NULL)
    {
        SInt32		i;
        
        myStrLength = strlen (thePath);
        if (myStrLength > MAX_OSPATH - 1)
        {
            myStrLength = MAX_OSPATH - 1;
        }
        for (i = 0; i < myStrLength; i++)
        {
            gCDDevice[i] = thePath[i];
        }
    }
    gCDDevice[myStrLength] = 0x00;
}

//______________________________________________________________________________________________________CDAudio_GetTrackList()

BOOL	CDAudio_GetTrackList (void)
{
    NSAutoreleasePool 		*myPool;
    
    // release previously allocated memory:
    CDAudio_Shutdown ();
    
    // get memory for the new tracklisting:
    gCDTrackList = [[NSMutableArray alloc] init];
    myPool = [[NSAutoreleasePool alloc] init];
    gCDTrackCount = 0;
    
    // Get the current MP3 listing or retrieve the TOC of the AudioCD:
    if ([[NSApp delegate] mediaFolder] != NULL)
    {
        NSString	*myMediaFolder = [[NSApp delegate] mediaFolder];

        CDAudio_SafePath ([myMediaFolder fileSystemRepresentation]);
        Com_Printf ("Scanning for audio tracks. Be patient!\n");
        CDAudio_AddTracks2List (myMediaFolder, [NSArray arrayWithObjects: @"mp3", @"mp4", NULL]);
    }
    else
    {
        NSString		*myMountPath;
        struct statfs  		*myMountList;
        UInt32			myMountCount;

        // get number of mounted devices:
        myMountCount = getmntinfo (&myMountList, MNT_NOWAIT);
        
        // zero devices? return.
        if (myMountCount <= 0)
        {
            [gCDTrackList release];
            gCDTrackList = NULL;
            gCDTrackCount = 0;
            CDAudio_Error (CDERR_NO_MEDIA_FOUND);
            return (0);
        }
        
        while (myMountCount--)
        {
            // is the device read only?
            if ((myMountList[myMountCount].f_flags & MNT_RDONLY) != MNT_RDONLY) continue;
            
            // is the device local?
            if ((myMountList[myMountCount].f_flags & MNT_LOCAL) != MNT_LOCAL) continue;
            
            // is the device "cdda"?
            if (strcmp (myMountList[myMountCount].f_fstypename, "cddafs")) continue;
            
            // is the device a directory?
            if (strrchr (myMountList[myMountCount].f_mntonname, '/') == NULL) continue;
            
            // we have found a Audio-CD!
            Com_Printf ("Found Audio-CD at mount entry: \"%s\".\n", myMountList[myMountCount].f_mntonname);
            
            // preserve the device name:
            CDAudio_SafePath (myMountList[myMountCount].f_mntonname);
            myMountPath = [NSString stringWithCString: myMountList[myMountCount].f_mntonname];
    
            Com_Printf ("Scanning for audio tracks. Be patient!\n");
            CDAudio_AddTracks2List (myMountPath, [NSArray arrayWithObjects: @"aiff", @"cdda", NULL]);
            
            break;
        }
    }
    
    // release the pool:
    [myPool release];
    
    // just security:
    if (![gCDTrackList count])
    {
        [gCDTrackList release];
        gCDTrackList = NULL;
        gCDTrackCount = 0;
        CDAudio_Error (CDERR_NO_FILES_FOUND);
        return (0);
    }
    
    return (1);
}

//______________________________________________________________________________________________________________CDAudio_Play()

void	CDAudio_Play (byte theTrack, qbool theLoop)
{
    gCDNextTrack = NO;
    
    if (gCDTrackList != NULL && gCDTrackCount != 0)
    {
        NSMovie		*myMovie;

        // stop the thread:
        CDAudio_ExitThread ();
        
        // check for mismatching CD track number:
        if (theTrack > gCDTrackCount || theTrack <= 0)
        {
            theTrack = 1;
        }
        gCDCurTrack = 0;

        if (gCDController != NULL && IsMovieDone (gCDController) == NO)
        {
            StopMovie (gCDController);
            gCDController = NULL;
        }
        
        myMovie = [gCDTrackList objectAtIndex: theTrack - 1];
        
        if (myMovie != NULL)
        {
            gCDController = [myMovie QTMovie];
            
            if (gCDController != NULL)
            {
                gCDCurTrack = theTrack;
                gCDLoop = theLoop;
                GoToBeginningOfMovie (gCDController);
                SetMovieActive (gCDController, YES);
                StartMovie (gCDController);
				
				if (GetMoviesError () != noErr)
				{
                    CDAudio_Error (CDERR_QUICKTIME_ERROR);
				}
                else
                {
                    if (pthread_create (&gCDThread, NULL, CDAudio_UpdateThread, NULL) != 0)
                    {
                        CDAudio_Error (CDERR_THREAD_ERROR);
                    }
                    else
                    {
                        gCDThreadIsRunning = YES;
                    }
                }
            }
            else
            {
                CDAudio_Error (CDERR_MEDIA_TRACK);
            }
        }
        else
        {
            CDAudio_Error (CDERR_MEDIA_TRACK_CONTROLLER);
        }
    }
}

//______________________________________________________________________________________________________________CDAudio_Stop()

void	CDAudio_Stop (void)
{
    // just stop the audio IO:
    if (gCDController != NULL && IsMovieDone (gCDController) == NO)
    {
        StopMovie (gCDController);
        GoToBeginningOfMovie (gCDController);
        SetMovieActive (gCDController, NO);
    }
}

//_____________________________________________________________________________________________________________CDAudio_Pause()

void	CDAudio_Pause (void)
{
    if (gCDController != NULL && GetMovieActive (gCDController) == YES && IsMovieDone (gCDController) == NO)
    {
        StopMovie (gCDController);
        SetMovieActive (gCDController, NO);
    }
}

//____________________________________________________________________________________________________________CDAudio_Resume()

void	CDAudio_Resume (void)
{
    if (gCDController != NULL && GetMovieActive (gCDController) == NO && IsMovieDone (gCDController) == NO)
    {
        SetMovieActive (gCDController, YES);
        StartMovie (gCDController);
    }
}

//____________________________________________________________________________________________________________CDAudio_Update()

void	CDAudio_Update (void)
{
    // send a signal to the playback thread:
    pthread_cond_signal (&gCDThreadCondition);
    
    if (gCDThreadRequestsNextTrack == YES)
    {
        CDAudio_Play (gCDCurTrack + 1, NO);
    }
}

//______________________________________________________________________________________________________CDAudio_UpdateThread()

void *	CDAudio_UpdateThread (void *theParameter)
{
    struct timespec	myTimeSpec = { 0, 250 * 1000 * 1000 };

    while (gCDThreadShouldExit == NO)
    {
        if(gCDController != NULL)
        {
            // update volume settings:
            SetMovieVolume (gCDController, kFullVolume * bgmvolume.value);
    
            if (GetMovieActive (gCDController) == YES)
            {
                if (IsMovieDone (gCDController) == NO)
                {
                    MoviesTask (gCDController, 0);
                }
                else
                {
                    if (gCDLoop == YES)
                    {
                        GoToBeginningOfMovie (gCDController);
                        StartMovie (gCDController);
                    }
                    else
                    {
//                        pthread_mutex_lock (&gCDThreadMutex);
                        gCDThreadRequestsNextTrack = YES;
                        gCDThreadIsRunning = NO;
                        gCDThreadShouldExit = NO;
//                        pthread_mutex_unlock (&gCDThreadMutex);
                        pthread_exit (NULL);
                        return (NULL);
                    }
                }
            }
        }

        // wait for the next update (either fired thru CDAudio_Update () or thru a timeout):
        pthread_cond_timedwait_relative_np (&gCDThreadCondition, &gCDThreadMutex, &myTimeSpec);
    }

//    pthread_mutex_lock (&gCDThreadMutex);
    gCDThreadIsRunning = NO;
    gCDThreadShouldExit = NO;
    gCDThreadRequestsNextTrack = NO;
//    pthread_mutex_unlock (&gCDThreadMutex);
    pthread_exit (NULL);
    
    // just for the compiler's sake:
    return (NULL);
}

//________________________________________________________________________________________________________CDAudio_ExitThread()

void	CDAudio_ExitThread (void)
{
//    pthread_mutex_lock (&gCDThreadMutex);
    if (gCDThreadIsRunning == YES)
    {
        gCDThreadShouldExit = YES;
//        pthread_mutex_unlock (&gCDThreadMutex);
        pthread_cond_signal (&gCDThreadCondition);
        pthread_join (gCDThread, NULL);
    }
    else
    {
//        pthread_mutex_unlock (&gCDThreadMutex);
    }
}

//____________________________________________________________________________________________________________CDAudio_Enable()

void	CDAudio_Enable (BOOL theState)
{
    static BOOL		myCDIsEnabled = YES;
    
    if (myCDIsEnabled != theState)
    {
        static BOOL	myCDWasPlaying = NO;
        
        if (theState == NO)
        {
            if (gCDController != NULL &&
                GetMovieActive (gCDController) == YES &&
                IsMovieDone (gCDController) == NO)
            {
                CDAudio_Pause ();
                myCDWasPlaying = YES;
            }
            else
            {
                myCDWasPlaying = NO;
            }
        }
        else
        {
            if (myCDWasPlaying == YES)
            {
                CDAudio_Resume ();
            }
        }
        myCDIsEnabled = theState;
    }
}

//______________________________________________________________________________________________________________CDAudio_Init()

int	CDAudio_Init (void)
{
    // add "cd" and "mp3" console command:
    if ([[NSApp delegate] mediaFolder] != NULL)
    {
        Cmd_AddCommand ("mp3", CD_f);
        Cmd_AddCommand ("mp4", CD_f);
    }
    Cmd_AddCommand ("cd", CD_f);
    
    gCDCurTrack = 0;
    
    if (gCDTrackList != NULL)
    {
        if ([[NSApp delegate] mediaFolder] == NULL)
        {
            Com_Printf ("QuickTime CD driver initialized...\n");
        }
        else
        {
            Com_Printf ("QuickTime MP3/MP4 driver initialized...\n");
        }

        return (1);
    }
    
    // failure. return 0.
    if ([[NSApp delegate] mediaFolder] == NULL)
    {
        Com_Printf ("QuickTime CD driver failed.\n");
    }
    else
    {
        Com_Printf ("QuickTime MP3/MP4 driver failed.\n");
    }
    
    return (0);
}

//__________________________________________________________________________________________________________CDAudio_Shutdown()

void	CDAudio_Shutdown (void)
{
    // shutdown the audio IO:
    CDAudio_Stop ();
    CDAudio_ExitThread ();

    gCDController = NULL;
    gCDDevice[0] = 0x00;    
    gCDCurTrack = 0;

    if (gCDTrackList != NULL)
    {
       while ([gCDTrackList count])
        {
            NSMovie 	*myMovie = [gCDTrackList objectAtIndex: 0];
            
            [gCDTrackList removeObjectAtIndex: 0];
            [myMovie release];
        }
        [gCDTrackList release];
        gCDTrackList = NULL;
        gCDTrackCount = 0;
    }
}

//______________________________________________________________________________________________________________________CD_f()

void	CD_f (void)
{
    char	*myCommandOption;

    // this command requires options!
    if (Cmd_Argc () < 2)
    {
        return;
    }

    // get the option:
    myCommandOption = Cmd_Argv (1);
    
    // turn CD playback on:
    if (strcasecmp (myCommandOption, "on") == 0)
    {
        if (gCDTrackList == NULL)
        {
            CDAudio_GetTrackList();
        }
        CDAudio_Play(1, 0);
        
		return;
    }
    
    // turn CD playback off:
    if (strcasecmp (myCommandOption, "off") == 0)
    {
        CDAudio_Shutdown ();
        
		return;
    }

    // just for compatibility:
    if (strcasecmp (myCommandOption, "remap") == 0)
    {
        return;
    }

    // reset the current CD:
    if (strcasecmp (myCommandOption, "reset") == 0)
    {
        CDAudio_Stop ();
        if (CDAudio_GetTrackList ())
        {
            if ([[NSApp delegate] mediaFolder] == NULL)
            {
                Com_Printf ("CD");
            }
            else
            {
                Com_Printf ("MP3/MP4 files");
            }
            Com_Printf (" found. %d tracks (\"%s\").\n", gCDTrackCount, gCDDevice);
	}
        else
        {
            CDAudio_Error (CDERR_NO_FILES_FOUND);
        }
        
		return;
    }
    
    // the following commands require a valid track array, so build it, if not present:
    if (gCDTrackCount == 0)
    {
        CDAudio_GetTrackList ();
        if (gCDTrackCount == 0)
        {
            CDAudio_Error (CDERR_NO_FILES_FOUND);
            return;
        }
    }
    
    // play the selected track:
    if (strcasecmp (myCommandOption, "play") == 0)
    {
        CDAudio_Play (atoi (Cmd_Argv (2)), 0);
        
		return;
    }
    
    // loop the selected track:
    if (strcasecmp (myCommandOption, "loop") == 0)
    {
        CDAudio_Play (atoi (Cmd_Argv (2)), 1);
        
		return;
    }
    
    // stop the current track:
    if (strcasecmp (myCommandOption, "stop") == 0)
    {
        CDAudio_Stop ();
        
		return;
    }
    
    // pause the current track:
    if (strcasecmp (myCommandOption, "pause") == 0)
    {
        CDAudio_Pause ();
        
		return;
    }
    
    // resume the current track:
    if (strcasecmp (myCommandOption, "resume") == 0)
    {
        CDAudio_Resume ();
        
		return;
    }
    
    // eject the CD:
    if ([[NSApp delegate] mediaFolder] == NULL && strcasecmp (myCommandOption, "eject") == 0)
    {
        // eject the CD:
        if (gCDDevice[0] != 0x00)
        {
            NSString	*myDevicePath = [NSString stringWithCString: gCDDevice];
            
            if (myDevicePath != NULL)
            {
                CDAudio_Shutdown ();
                
                if (![[NSWorkspace sharedWorkspace] unmountAndEjectDeviceAtPath: myDevicePath])
                {
                    CDAudio_Error (CDERR_EJECT);
                }
            }
            else
            {
                CDAudio_Error (CDERR_EJECT);
            }
        }
        else
        {
            CDAudio_Error (CDERR_NO_MEDIA_FOUND);
        }
        
		return;
    }
    
    // output CD info:
    if (strcasecmp(myCommandOption, "info") == 0)
    {
        if (gCDTrackCount == 0)
        {
            CDAudio_Error (CDERR_NO_FILES_FOUND);
        }
        else
        {
            if (gCDController != NULL && GetMovieActive (gCDController) == YES)
            {
                Com_Printf ("Playing track %d of %d (\"%s\").\n", gCDCurTrack, gCDTrackCount, gCDDevice);
            }
            else
            {
                Com_Printf ("Not playing. Tracks: %d (\"%s\").\n", gCDTrackCount, gCDDevice);
            }
            Com_Printf ("Volume is: %.2f.\n", bgmvolume.value);
        }
        
		return;
    }
}

//_________________________________________________________________________________________________________________________eOF
