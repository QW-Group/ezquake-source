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

    $Id: snd_mem.c,v 1.6 2006-05-04 19:46:31 disconn3ct Exp $
*/
// snd_mem.c -- sound caching

#include "quakedef.h"


static void ResampleSfx (sfx_t *sfx, int inrate, int inwidth, unsigned char *data)
{
	int sample, samplefrac, fracstep, outcount, srcsample, i;
	float stepscale;
	sfxcache_t *sc;

	if (!(sc = (sfxcache_t *) Cache_Check (&sfx->cache)))
		return;

	stepscale = (float) inrate / shm->format.speed; // this is usually 0.5, 1, or 2

	outcount = (int) ((double) sc->length * (double) shm->format.speed / (double) inrate);
	sc->length = outcount;
	if (sc->loopstart != -1)
		sc->loopstart = sc->loopstart / stepscale;

	sc->speed = shm->format.speed;
	sc->width = (s_loadas8bit.value) ? 1 : inwidth;
	sc->stereo = 0;

	// resample / decimate to the current source rate

	if (stepscale == 1 && inwidth == 1 && sc->width == 1) {
		// fast special case
		for (i = 0; i < outcount; i++)
			((signed char *)sc->data)[i] = (int) ((unsigned char)(data[i]) - 128);
	} else {
		// general case
		samplefrac = 0;
		fracstep = (int) (stepscale * 256);
		for (i = 0 ;i < outcount ;i++) {
			srcsample = samplefrac >> 8;
			samplefrac += fracstep;

			if (inwidth == 2)
				sample = LittleShort(((short *)data)[srcsample]);
			else
				sample = (int)((unsigned char)(data[srcsample]) - 128) << 8;
	
			if (sc->width == 2)
				((short *)sc->data)[i] = sample;
			else
				((signed char *)sc->data)[i] = sample >> 8;
		}
	}
}

/*
===============================================================================
WAV loading
===============================================================================
*/

static unsigned char *data_p;
static unsigned char *iff_end;
static unsigned char *last_chunk;
static unsigned char *iff_data;
static int iff_chunk_len;

static short GetLittleShort(void)
{
	short val;

	val = BuffLittleShort (data_p);
	data_p += 2;

	return val;
}

static int GetLittleLong(void)
{
	int val = 0;

	val = BuffLittleLong (data_p);
	data_p += 4;

	return val;
}

static void FindNextChunk(char *name)
{
	while (1) {
		data_p=last_chunk;

		if (data_p >= iff_end) { // didn't find the chunk
			data_p = NULL;
			return;
		}

		data_p += 4;
		iff_chunk_len = GetLittleLong();
		if (iff_chunk_len < 0) {
			data_p = NULL;
			return;
		}

		data_p -= 8;
		last_chunk = data_p + 8 + ( (iff_chunk_len + 1) & ~1 );
		if (!strncmp((const char *)data_p, name, 4))
			return;
	}
}

static void FindChunk(char *name)
{
	last_chunk = iff_data;
	FindNextChunk (name);
}

static wavinfo_t GetWavinfo (char *name, unsigned char *wav, int wavlength)
{
	int samples, format, i;
	wavinfo_t info;

	memset (&info, 0, sizeof(info));

	if (!wav)
		return info;

	iff_data = wav;
	iff_end = wav + wavlength;

	// find "RIFF" chunk
	FindChunk("RIFF");
	if (!(data_p && !strncmp((const char *)(data_p+8), "WAVE", 4))) {
		Com_Printf ("Missing RIFF/WAVE chunks\n");
		return info;
	}

	// get "fmt " chunk
	iff_data = data_p + 12;
	FindChunk("fmt ");
	if (!data_p) {
		Com_Printf ("Missing fmt chunk\n");
		return info;
	}

	data_p += 8;
	format = GetLittleShort();
	if (format != 1) {
		Com_Printf ("Microsoft PCM format only\n");
		return info;
	}

	info.channels = GetLittleShort();
	info.rate = GetLittleLong();
	data_p += 4 + 2;
	info.width = GetLittleShort() / 8;

	// get cue chunk
	FindChunk("cue ");
	if (data_p) {
		data_p += 32;
		info.loopstart = GetLittleLong();

		// if the next chunk is a LIST chunk, look for a cue length marker
		FindNextChunk ("LIST");
		if (data_p) {
			// this is not a proper parse, but it works with cooledit...
			if (!strncmp ((const char *)(data_p + 28), "mark", 4)) {
				data_p += 24;
				i = GetLittleLong (); // samples in loop
				info.samples = info.loopstart + i;
			}
		}
	} else
		info.loopstart = -1;

	// find data chunk
	FindChunk("data");
	if (!data_p) {
		Com_Printf ("Missing data chunk\n");
		return info;
	}

	data_p += 4;
	samples = GetLittleLong () / info.width / info.channels;

	if (info.samples) {
		if (samples < info.samples)
			Sys_Error ("Sound %s has a bad loop length", name);
	} else
		info.samples = samples;

	info.dataofs = data_p - wav;

	return info;
}


//=============================================================================
sfxcache_t *S_LoadSound (sfx_t *s)
{
	unsigned char stackbuf[1*1024]; // avoid dirtying the cache heap
	char namebuffer[256];
	unsigned char *data;
	sfxcache_t *sc;
	wavinfo_t info;
	int len;

	// see if still in memory
	if ((sc = (sfxcache_t *) Cache_Check (&s->cache)))
		return sc;

	// load it in
	strcpy(namebuffer, "sound/");
	strcat(namebuffer, s->name);

	if (!(data = FS_LoadStackFile (namebuffer, stackbuf, sizeof(stackbuf)))) {
		Com_Printf ("Couldn't load %s\n", namebuffer);
		return NULL;
	}

	FMod_CheckModel(namebuffer, data, com_filesize);

	info = GetWavinfo (s->name, data, com_filesize);
	if (info.channels != 1) {
		Com_Printf ("%s is a stereo sample\n",s->name);
		return NULL;
	}

	len = (int) ((double) info.samples * (double) shm->format.speed / (double) info.rate);
	len = len * info.width * info.channels;

	if (!(sc = (sfxcache_t *) Cache_Alloc (&s->cache, len + sizeof(sfxcache_t), s->name)))
		return NULL;

	sc->length = info.samples;
	sc->loopstart = info.loopstart;
	sc->speed = info.rate;
	sc->width = info.width;
	sc->stereo = info.channels;

	ResampleSfx (s, sc->speed, sc->width, data + info.dataofs);

	return sc;
}
