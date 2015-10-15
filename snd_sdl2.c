/*
   Copyright (C) 2007-2008 Andrey Nazarov

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

static void Filler(void *userdata, Uint8 *stream, int len)
{
	int size = shm->samples << 1;
	int pos = shm->samplepos << 1;
	int wrapped = pos + len - size;

	// Implicit Minimized in first case

	if (wrapped < 0) {
		memcpy(stream, shm->buffer + pos, len);
		shm->samplepos += len >> 1;
	} else {
		int remaining = size - pos;
		memcpy(stream, shm->buffer + pos, remaining);
		memcpy(stream + remaining, shm->buffer, wrapped);
		shm->samplepos = wrapped >> 1;
	}

	if ((sys_inactivesound.integer == 0 && !ActiveApp) || (sys_inactivesound.integer == 2 && Minimized)) {
		SDL_memset(stream, 0, len);
	}
}

void SNDDMA_Shutdown(void)
{
	Con_Printf("Shutting down SDL audio.\n");

	SDL_CloseAudio();

	if (SDL_WasInit(SDL_INIT_AUDIO != 0))
		SDL_QuitSubSystem(SDL_INIT_AUDIO);

	if (shm->buffer) {
		Q_free(shm->buffer);
		shm->buffer = NULL;
	}
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

	memset(&desired, 0, sizeof(desired));
	switch (s_khz.integer) {
		case 48:
			desired.freq = 48000;
			break;
		case 44:
			desired.freq = 44100;
			break;
		case 22:
			desired.freq = 22050;
			break;
		default:
			desired.freq = 11025;
			break;
	}

	desired.format = AUDIO_S16LSB;
	desired.samples = 1024;
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
	shm->samples = 0x4000 * obtained.channels;
	shm->buffer = Q_malloc(shm->samples * 2);
	shm->samplepos = 0;
	shm->sampleframes = shm->samples / shm->format.channels;

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
	return shm->samplepos;
}
