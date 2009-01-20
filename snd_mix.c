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
#ifdef _WIN32
#include "winquake.h"
#include "movie.h" //joe: capturing audio
#endif


#define PAINTBUFFER_SIZE 512
typedef struct portable_samplepair_s {
	int left;
	int right;
} portable_samplepair_t;
portable_samplepair_t paintbuffer[PAINTBUFFER_SIZE];
int snd_scaletable[32][256];
int snd_vol, *snd_p;
short *snd_out;


int snd_linear_count;

#ifdef _WIN32
extern char *DSoundError (int error);
#endif

#ifndef id386
static void Snd_WriteLinearBlastStereo16 (void)
{
	int val, i;

	for (i = 0; i < snd_linear_count; i += 2) {
		val = (snd_p[i]*snd_vol)>>8;
		snd_out[i] = bound (-32768, val, 32767);
		val = (snd_p[i+1]*snd_vol)>>8;
		snd_out[i+1] = bound (-32768, val, 32767);
	}
}
#else
void Snd_WriteLinearBlastStereo16 (void);
#endif

static void Snd_WriteLinearBlastStereo16_SwapStereo (void)
{
	int val, i;

	for (i = 0; i < snd_linear_count; i +=2 ) {
		val = (snd_p[i+1]*snd_vol)>>8;
		snd_out[i] = bound (-32768, val, 32767);
		val = (snd_p[i]*snd_vol)>>8;
		snd_out[i+1] = bound (-32768, val, 32767);
	}
}

static void S_TransferStereo16 (int endtime)
{
	int lpaintedtime, lpos;
	DWORD *pbuf;
#ifdef _WIN32
	int reps;
	DWORD dwSize = 0,dwSize2 = 0, *pbuf2;
	HRESULT hresult;
#endif

	snd_vol = s_volume.value * 256;

	snd_p = (int *) paintbuffer;
	lpaintedtime = paintedtime;

#ifdef _WIN32
	if (pDSBuf) {
		reps = 0;

		while ((hresult = pDSBuf->lpVtbl->Lock(pDSBuf, 0, gSndBufSize, &pbuf, &dwSize,
					&pbuf2, &dwSize2, 0)) != DS_OK) {
			if (hresult != DSERR_BUFFERLOST) {
				Com_Printf ("S_TransferStereo16: Lock failed with error '%s'\n", DSoundError(hresult));
				S_Shutdown ();
				return;
			} else {
				pDSBuf->lpVtbl->Restore (pDSBuf);
			}

			if (++reps > 2)
				return;
		}
	} else
#endif
		pbuf = (DWORD *)shm->buffer;

	while (lpaintedtime < endtime) {
		// handle recirculating buffer issues
		lpos = lpaintedtime & ((shm->samples>>1) - 1);

		snd_out = (short *) pbuf + (lpos << 1);

		snd_linear_count = (shm->samples>>1) - lpos;
		if (lpaintedtime + snd_linear_count > endtime)
			snd_linear_count = endtime - lpaintedtime;

		snd_linear_count <<= 1;

		// write a linear blast of samples
		if (s_swapstereo.value)
			Snd_WriteLinearBlastStereo16_SwapStereo ();
		else
			Snd_WriteLinearBlastStereo16 ();

		snd_p += snd_linear_count;
		lpaintedtime += (snd_linear_count>>1);

		//joe: capturing audio
#ifdef _WIN32
		Movie_TransferStereo16 ();
#endif
	}

#ifdef _WIN32
	if (pDSBuf)
		pDSBuf->lpVtbl->Unlock(pDSBuf, pbuf, dwSize, NULL, 0);
#endif
}

static void S_TransferPaintBuffer(int endtime)
{
	int out_idx, out_mask, count, step, val, snd_vol, *p;
	DWORD *pbuf;
#ifdef _WIN32
	int reps;
	DWORD dwSize = 0, dwSize2, *pbuf2;
	HRESULT hresult;
#endif

	if (shm->format.width == 2 && shm->format.channels == 2) {
		S_TransferStereo16 (endtime);
		return;
	}

	p = (int *) paintbuffer;
	count = (endtime - paintedtime) * shm->format.channels;
	out_mask = shm->samples - 1;
	out_idx = paintedtime * shm->format.channels & out_mask;
	step = 3 - shm->format.channels;
	snd_vol = s_volume.value * 256;

#ifdef _WIN32
	if (pDSBuf) {
		reps = 0;

		while ((hresult = pDSBuf->lpVtbl->Lock(pDSBuf, 0, gSndBufSize, &pbuf, &dwSize,
					&pbuf2,&dwSize2, 0)) != DS_OK) {
			if (hresult != DSERR_BUFFERLOST) {
				Com_Printf ("S_TransferPaintBuffer: Lock failed with error '%s'\n", DSoundError(hresult));
				S_Shutdown ();
				return;
			} else {
				pDSBuf->lpVtbl->Restore (pDSBuf);
			}

			if (++reps > 2)
				return;
		}
	} else
#endif
		pbuf = (DWORD *)shm->buffer;

	if (shm->format.width == 2) {
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
	} else if (shm->format.width == 1) {
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

#ifdef _WIN32
	if (pDSBuf) {
		DWORD dwNewpos, dwWrite;
		int il = paintedtime;
		int ir = endtime - paintedtime;

		ir += il;

		pDSBuf->lpVtbl->Unlock(pDSBuf, pbuf, dwSize, NULL, 0);

		pDSBuf->lpVtbl->GetCurrentPosition(pDSBuf, &dwNewpos, &dwWrite);

//		if ((dwNewpos >= il) && (dwNewpos <= ir))
//			Com_Printf ("%d-%d p %d c\n", il, ir, dwNewpos);
	}
#endif
}


/*
===============================================================================
CHANNEL MIXING
===============================================================================
*/

#ifndef id386
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
#else
void SND_PaintChannelFrom8 (channel_t *ch, sfxcache_t *sc, int endtime);
#endif

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
			snd_scaletable[i][j] = ((j < 128) ? j : j - 0xff) * i * 8; // snd_scaletable[i][j] = ((signed char) j) * i * 8;
}

void S_PaintChannels (int endtime)
{
	int ltime, count, end;
	unsigned int i;
	sfxcache_t *sc;
	channel_t *ch;

	while (paintedtime < endtime) {
		// if paintbuffer is smaller than DMA buffer
		end = endtime;
		if (endtime - paintedtime > PAINTBUFFER_SIZE)
			end = paintedtime + PAINTBUFFER_SIZE;

		// clear the paint buffer
		memset (paintbuffer, 0, (end - paintedtime) * sizeof(portable_samplepair_t));

		// paint in the channels.
		ch = channels;
		for (i = 0; i < total_channels; i++, ch++) {
			if (!ch->sfx)
				continue;
			if (!ch->leftvol && !ch->rightvol)
				continue;
			sc = S_LoadSound (ch->sfx);
			if (!sc)
				continue;

			ltime = paintedtime;

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
		paintedtime = end;
	}
}
