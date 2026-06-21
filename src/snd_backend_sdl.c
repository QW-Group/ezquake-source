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

#include <SDL.h>

#include "quakedef.h"
#include "qsound.h"
#include "snd_backend.h"

static SDL_AudioDeviceID audiodevid;

static void S_SDL_callback(void *userdata, Uint8 *stream, int len)
{
	S_MixOutput(stream, len, true);
}

void S_Backend_Update(void)
{
}

void S_Backend_ListDrivers(void)
{
	int i = 0, numdrivers;

	numdrivers = SDL_GetNumAudioDrivers();

	Com_Printf("Audio driver support compiled into SDL:\n");
	for (; i < numdrivers; i++) {
		Com_Printf("%s\n", SDL_GetAudioDriver(i));
	}
}

static void S_SDL_ListAudioDevicesInternal(qbool capture)
{
	int i = 0, numdevices;

	numdevices = SDL_GetNumAudioDevices(capture); /* arg is iscapture */
	for (; i < numdevices; i++) {
		Com_Printf(" %2d  %s\n", i+1, SDL_GetAudioDeviceName(i, capture));
	}
}

void S_Backend_ListAudioDevices(void)
{
	Com_Printf("Playback devices:\n");
	Com_Printf(" id  device name\n-------------------------\n");
	Com_Printf("  0  system default\n");
	S_SDL_ListAudioDevicesInternal(false);

	Com_Printf("\nInput devices:\n");
	Com_Printf(" id  device name\n-------------------------\n");
	Com_Printf("  0  system default\n");
	S_SDL_ListAudioDevicesInternal(true);
}

void S_Backend_PrintSoundInfo(void)
{
	Com_Printf("backend: SDL");
	if (SDL_WasInit(SDL_INIT_AUDIO) != 0) {
		Com_Printf(" (%s)", SDL_GetCurrentAudioDriver());
	}
	Com_Printf("\n");
}

void S_Backend_Shutdown(void)
{
	Con_Printf("Shutting down SDL audio.\n");

	if (audiodevid) {
		SDL_CloseAudioDevice(audiodevid);
		audiodevid = 0;
	}

	if (SDL_WasInit(SDL_INIT_AUDIO) != 0)
		SDL_QuitSubSystem(SDL_INIT_AUDIO);

	S_MixerLock_Shutdown();

	if (shw != NULL) {
		Q_free(shw);
		shw = NULL;
	}
}

qbool S_Backend_Init(void)
{
	SDL_AudioSpec desired, obtained;
	soundhw_t *shw_tmp = NULL;
	int ret = 0;
	const char *requested_device = NULL;

	if (SDL_WasInit(SDL_INIT_AUDIO) == 0)
		ret = SDL_InitSubSystem(SDL_INIT_AUDIO);

	if (ret == -1) {
		Con_Printf("Couldn't initialize SDL audio: %s\n", SDL_GetError());
		return false;
	}

	S_MixerLock_Init();

	memset(&desired, 0, sizeof(desired));
	desired.freq = S_SampleRateFromKHzCvar();
	desired.samples = S_DefaultBufferSamplesForKHz(s_khz.integer);

	desired.format = AUDIO_S16LSB;
	desired.channels = 2;
	if (s_desiredsamples.integer) {
		int desired_samples = 1;

		// make sure it's a power of 2
		while (desired_samples < s_desiredsamples.integer)
			desired_samples <<= 1;

		desired.samples = desired_samples;
	}
	desired.callback = S_SDL_callback;

	/* Make audiodevice list start from index 1 so that 0 can be system default */
	if (s_audiodevice.integer > 0) {
		requested_device = SDL_GetAudioDeviceName(s_audiodevice.integer - 1, 0);
	}

	if ((audiodevid = SDL_OpenAudioDevice(requested_device, 0, &desired, &obtained, 0)) <= 0) {
		Com_Printf("sound: couldn't open SDL audio: %s\n", SDL_GetError());
		if (requested_device != NULL) {
			Com_Printf("sound: retrying with default audio device\n");
			if ((audiodevid = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0)) <= 0) {
				Com_Printf("sound: failure again, aborting...\n");
				return false;
			}
			Cvar_LatchedSet(&s_audiodevice, "0");
		}
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

	shw_tmp = Q_calloc(1, sizeof(*shw_tmp));
	if (!shw_tmp) {
		Com_Printf("Failed to alloc memory for sound structure\n");
		goto fail;
	}

	shw_tmp->khz = obtained.freq;
	shw_tmp->numchannels = obtained.channels;
	shw_tmp->samplebits = obtained.format & 0xFF;
	shw_tmp->samples = 65536;

	Cvar_AutoSetInt(&s_desiredsamples, obtained.samples);

	shw = shw_tmp;

	Com_Printf("Using SDL audio driver: %s @ %d Hz\n", SDL_GetCurrentAudioDriver(), obtained.freq);

	SDL_PauseAudioDevice(audiodevid, 0);

	return true;

fail:
	S_Backend_Shutdown();
	return false;
}
