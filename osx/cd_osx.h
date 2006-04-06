//_________________________________________________________________________________________________________________________nFO
// "cd_osx.h" - MacOS X audio CD driver.
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2006 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
// Quakeª is copyrighted by id software		[http://www.idsoftware.com].
//
//____________________________________________________________________________________________________________________tYPEDEFS

#pragma mark =TypeDefs=

typedef enum
{
    CDERR_ALLOC_TRACK = 1,
    CDERR_MOVIE_DATA,
    CDERR_AUDIO_DATA,
    CDERR_QUICKTIME_ERROR,
    CDERR_THREAD_ERROR,
    CDERR_NO_MEDIA_FOUND,
    CDERR_MEDIA_TRACK,
    CDERR_MEDIA_TRACK_CONTROLLER,
    CDERR_EJECT,
    CDERR_NO_FILES_FOUND
} cderror_t;

#pragma mark -

//_________________________________________________________________________________________________________fUNCTION_pROTOTYPES

#pragma mark =Function Prototypes=

BOOL			CDAudio_GetTrackList (void);
void			CDAudio_Enable (BOOL theState);

//_________________________________________________________________________________________________________________________eOF
