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

static SDL_AudioStream *audiostream;

static void S_SDL_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
	Uint8 *data;

	if (additional_amount <= 0) {
		return;
	}

	data = SDL_stack_alloc(Uint8, additional_amount);
	if (!data) {
		return;
	}

	S_MixOutput(data, additional_amount, true);
	SDL_PutAudioStreamData(stream, data, additional_amount);
	SDL_stack_free(data);
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
	int i, numdevices;
	SDL_AudioDeviceID *devices = capture ? SDL_GetAudioRecordingDevices(&numdevices) : SDL_GetAudioPlaybackDevices(&numdevices);

	if (!devices) {
		return;
	}

	for (i = 0; i < numdevices; i++) {
		Com_Printf(" %2d  %s\n", i + 1, SDL_GetAudioDeviceName(devices[i]));
	}
	SDL_free(devices);
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

	if (audiostream) {
		SDL_DestroyAudioStream(audiostream);
		audiostream = NULL;
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
	SDL_AudioSpec desired;
	soundhw_t *shw_tmp = NULL;
	int ret = 0, desired_samples = 0;
	SDL_AudioDeviceID requested_device = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;

	if (SDL_WasInit(SDL_INIT_AUDIO) == 0)
		ret = SDL_InitSubSystem(SDL_INIT_AUDIO);

	if (ret == -1) {
		Con_Printf("Couldn't initialize SDL audio: %s\n", SDL_GetError());
		return false;
	}

	S_MixerLock_Init();

	memset(&desired, 0, sizeof(desired));
	desired.freq = S_SampleRateFromKHzCvar();
	desired_samples = S_DefaultBufferSamplesForKHz(s_khz.integer);

	desired.format = SDL_AUDIO_S16LE;
	desired.channels = 2;
	if (s_desiredsamples.integer) {
		desired_samples = 1;

		// make sure it's a power of 2
		while (desired_samples < s_desiredsamples.integer)
			desired_samples <<= 1;
	}

	/* Make audiodevice list start from index 1 so that 0 can be system default */
	if (s_audiodevice.integer > 0) {
		int numdevices = 0;
		SDL_AudioDeviceID *devices = SDL_GetAudioPlaybackDevices(&numdevices);

		if (devices) {
			if (s_audiodevice.integer - 1 < numdevices) {
				requested_device = devices[s_audiodevice.integer - 1];
			}
			SDL_free(devices);
		}
	}

	audiostream = SDL_OpenAudioDeviceStream(requested_device, &desired, S_SDL_callback, NULL);
	if (!audiostream && requested_device != SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK) {
		Com_Printf("sound: couldn't open SDL audio: %s\n", SDL_GetError());
		Com_Printf("sound: retrying with default audio device\n");
		audiostream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired, S_SDL_callback, NULL);
		if (audiostream) {
			Cvar_LatchedSet(&s_audiodevice, "0");
		}
	}

	if (!audiostream) {
		Com_Printf("sound: couldn't open SDL audio: %s\n", SDL_GetError());
		return false;
	}

	shw_tmp = Q_calloc(1, sizeof(*shw_tmp));
	if (!shw_tmp) {
		Com_Printf("Failed to alloc memory for sound structure\n");
		goto fail;
	}

	shw_tmp->khz = desired.freq;
	shw_tmp->numchannels = desired.channels;
	shw_tmp->samplebits = 16;
	shw_tmp->samples = 65536;

	Cvar_AutoSetInt(&s_desiredsamples, desired_samples);

	shw = shw_tmp;

	Com_Printf("Using SDL audio driver: %s @ %d Hz\n", SDL_GetCurrentAudioDriver(), desired.freq);

	SDL_ResumeAudioStreamDevice(audiostream);

	return true;

fail:
	S_Backend_Shutdown();
	return false;
}
