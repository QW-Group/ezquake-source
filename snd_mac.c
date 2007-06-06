/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <OSUtils.h>
#include <Sound.h>
#include <DrawSprocket/DrawSprocket.h>
#include "qsound.h"

#include "quakedef.h"
#include "mac.h"
#include "CarbonSndPlayDB.h"
#define SndPlayDoubleBuffer CarbonSndPlayDoubleBuffer
#define SND_COMPLETION snd_completion

int gFrequency;

static pascal void snd_completion(SndChannelPtr pChannel, SndDoubleBufferPtr pDoubleBuffer);

static unsigned char dma[64*1024];
#define SoundBufferSize (1 * 1024)
SndChannelPtr  			gChannel = nil;            // pointer to the sound channel
SndDoubleBufferHeader 	gHeader;             // the double buffer header
static char sRandomBuf[SoundBufferSize];

qbool SNDDMA_Init(void)
{
	OSErr err;
	int i;

	// Just in case
	if (!gFrequency)
	{
		Com_Printf ("WARNING: Couldn't read sound rate preference\n");
		gFrequency = 22050;
	}
	
	for (i = 0; i < sizeof (sRandomBuf); i++)
		sRandomBuf[i] = rand();
	
	err = SndNewChannel (&gChannel, sampledSynth, initStereo | initNoInterp | initNoDrop, nil);
	if (err != noErr)
	{
		Com_Printf("Failed to allocate sound channel. OS error %d\n",err);
		return false;
	}

	gHeader.dbhNumChannels     = 2;
	gHeader.dbhSampleSize      = 16;
	gHeader.dbhSampleRate      = (unsigned long)gFrequency << 16;
	gHeader.dbhCompressionID   = notCompressed;
	gHeader.dbhPacketSize      = 0; // compression related
	gHeader.dbhDoubleBack      = SND_COMPLETION;
	if (!gHeader.dbhDoubleBack)
	{
		Com_Printf("Failed to allocate sound channel completion routine.\n");
		return false;
	}

	shm->format.channels = gHeader.dbhNumChannels;
	shm->format.width = gHeader.dbhSampleSize / 8;
	shm->format.speed = gFrequency;
	shm->samples = sizeof(dma) / (shm->format.width);
	shm->sampleframes = shm->samples / shm->format.channels;
	shm->samplepos = 0;
	shm->buffer = dma;

	for (i = 0; i < 2; i++)
	{
#undef malloc
#undef free
		gHeader.dbhBufferPtr[i] = malloc(SoundBufferSize + sizeof(SndDoubleBuffer)+0x10);
		if (!gHeader.dbhBufferPtr[i])
		{
			Com_Printf("Failed to allocate sound channel memory.\n");
			return false;
		}
		// ahh, the sounds of silence
		memset(gHeader.dbhBufferPtr[i]->dbSoundData, 0x00, SoundBufferSize);

//		// start with some horrible noise so we can guage whats going on.
//		memcpy(gHeader.dbhBufferPtr[i]->dbSoundData,sRandomBuf,SoundBufferSize);

		gHeader.dbhBufferPtr[i]->dbNumFrames   = SoundBufferSize/(shm->format.width)/shm->format.channels;
		gHeader.dbhBufferPtr[i]->dbFlags       = dbBufferReady;
		gHeader.dbhBufferPtr[i]->dbUserInfo[0] = 0;
//		gHeader.dbhBufferPtr[i]->dbUserInfo[1] = SetCurrentA5 ();
		
	}


	err = SndPlayDoubleBuffer (gChannel, &gHeader);
	if (err != noErr)
	{
		Com_Printf("Failed to start sound. OS error %d\n",err);
		return false;
	}

	return true;
}

void SNDDMA_Shutdown(void)
{
	int i;
	if (gChannel)
	{
		SCStatus myStatus;
		// Tell them to stop
		gHeader.dbhBufferPtr[0]->dbFlags |= dbLastBuffer;
		gHeader.dbhBufferPtr[1]->dbFlags |= dbLastBuffer;

		// Wait for the sound's end by checking the channel status.}
		do
		{
			SndChannelStatus(gChannel, sizeof(myStatus), &myStatus);
		}
		while (myStatus.scChannelBusy);

		SndDisposeChannel (gChannel, true);
		gChannel = nil;
		
	}
	
	for (i = 0; i < 2; i++)
		free (gHeader.dbhBufferPtr[i]);
}

void SNDDMA_Submit(void)
{
	// we're completion routine based....
}

int sDMAOffset;
int SNDDMA_GetDMAPos(void)
{
	return sDMAOffset/(shm->format.width);
}

static pascal void snd_completion(SndChannelPtr channel, SndDoubleBufferPtr buffer)
{
	int sampleOffset = sDMAOffset/(shm->format.width);
	int maxSamples = SoundBufferSize/(shm->format.width);
	int dmaSamples = sizeof(dma)/(shm->format.width);
	if (paintedtime  - sampleOffset > maxSamples ||
		sampleOffset - paintedtime  < dmaSamples-maxSamples)
	{
		memcpy(buffer->dbSoundData,dma+sDMAOffset,SoundBufferSize);
		sDMAOffset += SoundBufferSize;
		if (sDMAOffset >= sizeof(dma))
			sDMAOffset = 0;
	}
	else
		memset(buffer->dbSoundData, 0, SoundBufferSize);
//		memcpy(buffer->dbSoundData,sRandomBuf,SoundBufferSize); // nasty sound

	// ready to go!
	buffer->dbNumFrames = SoundBufferSize / (shm->format.width) / shm->format.channels;
	buffer->dbFlags |= dbBufferReady;
}
