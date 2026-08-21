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

#ifndef __SND_BACKEND_H__
#define __SND_BACKEND_H__

#ifdef __cplusplus
typedef unsigned char byte;
typedef int qbool;
#else
#include "q_shared.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

qbool S_Backend_Init(void);
void S_Backend_Shutdown(void);
void S_Backend_Update(void);
void S_Backend_ListDrivers(void);
void S_Backend_ListAudioDevices(void);
void S_Backend_PrintSoundInfo(void);

void S_MixOutput(byte *stream, int len, qbool blocking_lock);

qbool S_Backend_AllocSoundHardware(int khz, int channels, int samplebits);
void S_Backend_FreeSoundHardware(void);
int S_Backend_DesiredBufferSamples(void);
void S_Backend_SetActualBufferSamples(int samples);

void S_MixerLock_Init(void);
void S_MixerLock_Shutdown(void);
void S_LockMixer(void);
qbool S_TryLockMixer(void);
void S_UnlockMixer(void);

int S_SampleRateFromKHzCvar(void);
int S_DefaultBufferSamplesForKHz(int khz);

#ifdef __cplusplus
}
#endif

#endif /* __SND_BACKEND_H__ */
