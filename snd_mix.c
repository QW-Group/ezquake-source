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

    $Id: snd_mix.c,v 1.15 2007-03-11 06:01:42 disconn3ct Exp $

*/
// snd_mix.c -- portable code to mix sounds for snd_dma.c

#include "quakedef.h"
#include "qsound.h"
#include "movie.h" // /demo_capture


#define PAINTBUFFER_SIZE 512
typedef struct portable_samplepair_s {
	int left;
	int right;
} portable_samplepair_t;
portable_samplepair_t paintbuffer[PAINTBUFFER_SIZE];
int snd_scaletable[32][256];
int snd_vol, *snd_p;

static int snd_linear_count;
static short *snd_out;

static void Snd_WriteLinearBlastStereo16 (int* input_buffer, short* output_buffer, int snd_vol)
{
	int val, i;

	for (i = 0; i < snd_linear_count; i += 2) {
		val = (input_buffer[i]*snd_vol)>>8;
		output_buffer[i] = bound (-32768, val, 32767);
		val = (input_buffer[i+1]*snd_vol)>>8;
		output_buffer[i+1] = bound (-32768, val, 32767);
	}
}

static void Snd_WriteLinearBlastStereo16_SwapStereo (int* input_buffer, short* output_buffer, int snd_vol)
{
	int val, i;

	for (i = 0; i < snd_linear_count; i +=2 ) {
		val = (input_buffer[i+1]*snd_vol)>>8;
		output_buffer[i] = bound (-32768, val, 32767);
		val = (input_buffer[i]*snd_vol)>>8;
		output_buffer[i+1] = bound (-32768, val, 32767);
	}
}

static void S_TransferStereo16 (int endtime)
{
	int lpaintedtime, lpos, clientVolume;
	DWORD *pbuf;

	clientVolume = snd_vol = (s_volume.value * S_VoipVoiceTransmitVolume()) * 256;

	snd_p = (int *) paintbuffer;
	lpaintedtime = shw->paintedtime;

	pbuf = (DWORD *)shw->buffer;
	while (lpaintedtime < endtime) {

		// handle recirculating buffer issues
		lpos = lpaintedtime % ((shw->samples>>1));
		snd_out = (short *) pbuf + (lpos << 1);

		snd_linear_count = (shw->samples>>1) - lpos;
		if (lpaintedtime + snd_linear_count > endtime)
			snd_linear_count = endtime - lpaintedtime;

		snd_linear_count <<= 1;

		// write a linear blast of samples
		if (s_swapstereo.value)
			Snd_WriteLinearBlastStereo16_SwapStereo (snd_p, snd_out, clientVolume);
		else
			Snd_WriteLinearBlastStereo16 (snd_p, snd_out, clientVolume);

		if (Movie_IsCapturing()) {
			Movie_TransferSound (snd_out, snd_linear_count);
		}

		snd_p += snd_linear_count;
		lpaintedtime += (snd_linear_count>>1);
	}
}

static void S_TransferPaintBuffer(int endtime)
{
	DWORD *pbuf;
	int *p;
	int out_idx;
	int out_mask;
	int count;
	int step;
	int val;
	int snd_vol;

	if (shw->samplebits == 16 && shw->numchannels == 2) {
		S_TransferStereo16(endtime);
		return;
	}

	p = (int *) paintbuffer;
	count = (endtime - shw->paintedtime) * shw->numchannels;
	out_mask = shw->samples - 1;
	out_idx = shw->paintedtime * shw->numchannels & out_mask;
	step = 3 - shw->numchannels;
	snd_vol = (s_volume.value * S_VoipVoiceTransmitVolume()) * 256;

	pbuf = (DWORD *)shw->buffer;

	if (shw->samplebits == 16) {
		short *out = (short *) pbuf;
		while (count--) {
			val = (*p * snd_vol) >> 8;
			p+= step;
			if (val > 0x7fff)
				val = 0x7fff;
			else if (val < (int)0x8000)
				val = (int)0x8000;
			out[out_idx] = val;
			out_idx = (out_idx + 1) & out_mask;
		}
	} else if (shw->samplebits == 8) {
		unsigned char *out = (unsigned char *) pbuf;
		while (count--) {
			val = (*p * snd_vol) >> 8;
			p+= step;
			if (val > 0x7fff)
				val = 0x7fff;
			else if (val < (int)0x8000)
				val = (int)0x8000;
			out[out_idx] = (val>>8) + 128;
			out_idx = (out_idx + 1) & out_mask;
		}
	}
}


/*
===============================================================================
CHANNEL MIXING
===============================================================================
*/

static void SND_PaintChannelFrom8 (channel_t *ch, sfxcache_t *sc, int count)
{
	int data, i;
	int *lscale, *rscale;
	unsigned char *sfx;

	if (ch->leftvol > 255)
		ch->leftvol = 255;
	if (ch->rightvol > 255)
		ch->rightvol = 255;

	lscale = snd_scaletable[ch->leftvol >> 3];
	rscale = snd_scaletable[ch->rightvol >> 3];
	sfx = (unsigned char *) ((signed char *)sc->data + ch->pos);

	for (i = 0; i < count ;i++) {
		data = sfx[i];
		paintbuffer[i].left += lscale[data];
		paintbuffer[i].right += rscale[data];
	}

	ch->pos += count;
}

static void SND_PaintChannelFrom16 (channel_t *ch, sfxcache_t *sc, int count)
{
	int data, left, right, leftvol, rightvol, i;
	signed short *sfx;

	leftvol = ch->leftvol;
	rightvol = ch->rightvol;
	sfx = (signed short *)sc->data + ch->pos;

	for (i = 0; i < count ;i++) {
		data = sfx[i];
		left = (data * leftvol) >> 8;
		right = (data * rightvol) >> 8;
		paintbuffer[i].left += left;
		paintbuffer[i].right += right;
	}

	ch->pos += count;
}

void SND_InitScaletable (void)
{
	int i, j;

	for (i = 0 ; i < 32; i++)
		for (j = 0; j < 256; j++)
			snd_scaletable[i][j] = ((j < 128) ? j : j - 0xff) * i * 8;
}

void S_PaintChannels(int endtime)
{
	int ltime, count, end;
	unsigned int i;
	sfxcache_t *sc;
	channel_t *ch;
	extern cvar_t s_silent_racing;

	while (shw->paintedtime < endtime) {
		// if paintbuffer is smaller than DMA buffer
		end = endtime;
		if (endtime - shw->paintedtime > PAINTBUFFER_SIZE)
			end = shw->paintedtime + PAINTBUFFER_SIZE;

		// clear the paint buffer
		memset (paintbuffer, 0, (end - shw->paintedtime) * sizeof(portable_samplepair_t));

		// paint in the channels.
		ch = channels;
		for (i = 0; i < total_channels; i++, ch++) {
			if (!ch->sfx)
				continue;
			if (!ch->leftvol && !ch->rightvol)
				continue;
			if (cl.racing && s_silent_racing.integer && ch->entnum > 0 && ch->entnum <= MAX_CLIENTS) {
				if (cl.spectator) {
					if (Cam_TrackNum() < 0) {
						// silence all racers
						continue;
					}
					else if (ch->entnum - 1 != Cam_TrackNum()) {
						// not the tracked player
						continue;
					}
				}
				else if (ch->entnum - 1 != cl.playernum) {
					// a different player
					continue;
				}
			}
			sc = S_LoadSound (ch->sfx);
			if (!sc)
				continue;

			ltime = shw->paintedtime;

			while (ltime < end) { // paint up to end
				count = (ch->end < end) ? (ch->end - ltime) : (end - ltime);

				if (count > 0) {
					if (sc->format.width == 1)
						SND_PaintChannelFrom8(ch, sc, count);
					else
						SND_PaintChannelFrom16(ch, sc, count);

					ltime += count;
				}

				// if at end of loop, restart
				if (ltime >= ch->end) {
					if (sc->loopstart >= 0) {
						ch->pos = bound(0, sc->loopstart, (int) sc->total_length - 1);
						ch->end = ltime + (int) sc->total_length - ch->pos;
					} else { // channel just stopped
						ch->sfx = NULL;
						break;
					}
				}
			}
		}

		// transfer out according to DMA format
		S_TransferPaintBuffer(end);
		shw->paintedtime = end;
	}
}
