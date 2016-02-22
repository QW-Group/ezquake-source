/*
   Copyright (C) 2007-2008 Andrey Nazarov
   Copyright (C) 2014-2015 ezQuake team

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
   */

//
// snd_sdl.c
//

#include "quakedef.h"
#include "qsound.h"
#include <SDL.h>

extern qbool ActiveApp, Minimized;
extern cvar_t sys_inactivesound;

SDL_mutex *smutex;

extern void S_MixerThread(void);
static void Filler(void *userdata, Uint8 *stream, int len)
{
	shm->buffer = stream;
	shm->samples = len / 2;
	S_MixerThread();
	shm->snd_sent += len;

	// Implicit Minimized in first case
	if ((sys_inactivesound.integer == 0 && !ActiveApp) || (sys_inactivesound.integer == 2 && Minimized)) {
		SDL_memset(stream, 0, len);
	}
}

void SNDDMA_Shutdown(void)
{
	Con_Printf("Shutting down SDL audio.\n");

	SDL_CloseAudio();

	if (SDL_WasInit(SDL_INIT_AUDIO) != 0)
		SDL_QuitSubSystem(SDL_INIT_AUDIO);

#if 0
	if (smutex) {
		SDL_DestroyMutex(smutex);
		smutex = NULL;
	}
#endif

	shm->snd_sent = 0;
	shm->samples = 0;
}

qbool SNDDMA_Init(void)
{
	SDL_AudioSpec desired, obtained;
	int ret = 0;

	if (SDL_WasInit(SDL_INIT_AUDIO) == 0)
		ret = SDL_InitSubSystem(SDL_INIT_AUDIO);

	if (ret == -1) {
		Con_Printf("Couldn't initialize SDL audio: %s\n", SDL_GetError());
		return false;
	}

	if (!smutex) {
		smutex = SDL_CreateMutex();
	}

	memset(&desired, 0, sizeof(desired));
	switch (s_khz.integer) {
		case 48:
			desired.freq = 48000;
			desired.samples = 512;
			break;
		case 44:
			desired.freq = 44100;
			desired.samples = 512;
			break;
		case 22:
			desired.freq = 22050;
			desired.samples = 256;
			break;
		default:
			desired.freq = 11025;
			desired.samples = 128;
			break;
	}

	desired.format = AUDIO_S16LSB;
	desired.channels = 2;
	desired.callback = Filler;
	ret = SDL_OpenAudio(&desired, &obtained);
	if (ret == -1) {
		Com_Printf("Couldn't open SDL audio: %s\n", SDL_GetError());
		return false;
	}

	if (obtained.format != AUDIO_S16LSB) {
		Com_Printf("SDL audio format %d unsupported.\n", obtained.format);
		goto fail;
	}

	if (obtained.channels != 1 && obtained.channels != 2) {
		Com_Printf("SDL audio channels %d unsupported.\n", obtained.channels);
		goto fail;
	}

	shm->format.speed = obtained.freq;
	shm->format.channels = obtained.channels;
	shm->format.width = 2;
	shm->samples = 32768; // * obtained.channels;
	shm->buffer = NULL; //Q_malloc(shm->samples * 2);
	shm->samplepos = 0;
	shm->sampleframes = shm->samples / shm->format.channels;
	shm->snd_sent = 0;
	soundtime = paintedtime = 0;

	Com_Printf("Using SDL audio driver: %s @ %d Hz\n", SDL_GetCurrentAudioDriver(), obtained.freq);

	SDL_PauseAudio(0);

	return true;

fail:
	SNDDMA_Shutdown();
	return false;
}

void SNDDMA_BeginPainting(void)
{
	SDL_LockAudio();
}

void SNDDMA_Submit(void)
{
	SDL_UnlockAudio();
}

int SNDDMA_GetDMAPos()
{
	shm->samplepos = shm->snd_sent/2;
	return shm->samplepos;;
}
