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

    $Id: snd_mem.c,v 1.16 2007-10-25 14:54:30 dkure Exp $
*/
// snd_mem.c -- sound caching

#include "quakedef.h"
#include "fmod.h"
#include "qsound.h"
#ifndef OLD_WAV_LOADING
#include "sndfile.h"
#endif

#define LINEARUPSCALE(in, inrate, insamps, out, outrate, outlshift, outrshift) \
	{ \
		scale = inrate / (double)outrate; \
		infrac = floor(scale * 65536); \
		outsamps = insamps / scale; \
		inaccum = 0; \
		outnlsamps = floor(1.0 / scale); \
		outsamps -= outnlsamps; \
	\
		while (outsamps) \
		{ \
			*out = ((0xFFFF - inaccum)*in[0] + inaccum*in[1]) >> (16 - outlshift + outrshift); \
			inaccum += infrac; \
			in += (inaccum >> 16); \
			inaccum &= 0xFFFF; \
			out++; \
			outsamps--; \
		} \
		while (outnlsamps) \
		{ \
			*out = (*in >> outrshift) << outlshift; \
			out++; \
			outnlsamps--; \
		} \
	}

#define LINEARUPSCALESTEREO(in, inrate, insamps, out, outrate, outlshift, outrshift) \
	{ \
		scale = inrate / (double)outrate; \
		infrac = floor(scale * 65536); \
		outsamps = insamps / scale; \
		inaccum = 0; \
		outnlsamps = floor(1.0 / scale); \
		outsamps -= outnlsamps; \
	\
		while (outsamps) \
		{ \
			out[0] = ((0xFFFF - inaccum)*in[0] + inaccum*in[2]) >> (16 - outlshift + outrshift); \
			out[1] = ((0xFFFF - inaccum)*in[1] + inaccum*in[3]) >> (16 - outlshift + outrshift); \
			inaccum += infrac; \
			in += (inaccum >> 16) * 2; \
			inaccum &= 0xFFFF; \
			out += 2; \
			outsamps--; \
		} \
		while (outnlsamps) \
		{ \
			out[0] = (in[0] >> outrshift) << outlshift; \
			out[1] = (in[1] >> outrshift) << outlshift; \
			out += 2; \
			outnlsamps--; \
		} \
	}

#define LINEARUPSCALESTEREOTOMONO(in, inrate, insamps, out, outrate, outlshift, outrshift) \
	{ \
		scale = inrate / (double)outrate; \
		infrac = floor(scale * 65536); \
		outsamps = insamps / scale; \
		inaccum = 0; \
		outnlsamps = floor(1.0 / scale); \
		outsamps -= outnlsamps; \
	\
		while (outsamps) \
		{ \
			*out = ((((0xFFFF - inaccum)*in[0] + inaccum*in[2]) >> (16 - outlshift + outrshift)) + \
				(((0xFFFF - inaccum)*in[1] + inaccum*in[3]) >> (16 - outlshift + outrshift))) >> 1; \
			inaccum += infrac; \
			in += (inaccum >> 16) * 2; \
			inaccum &= 0xFFFF; \
			out++; \
			outsamps--; \
		} \
		while (outnlsamps) \
		{ \
			out[0] = (((in[0] >> outrshift) << outlshift) + ((in[1] >> outrshift) << outlshift)) >> 1; \
			out++; \
			outnlsamps--; \
		} \
	}

#define LINEARDOWNSCALE(in, inrate, insamps, out, outrate, outlshift, outrshift) \
	{ \
		scale = outrate / (double)inrate; \
		infrac = floor(scale * 65536); \
		inaccum = 0; \
		insamps--; \
		outsampleft = 0; \
	\
		while (insamps) \
		{ \
			inaccum += infrac; \
			if (inaccum >> 16) \
			{ \
				inaccum &= 0xFFFF; \
				outsampleft += (infrac - inaccum) * (*in); \
				*out = outsampleft >> (16 - outlshift + outrshift); \
				out++; \
				outsampleft = inaccum * (*in); \
			} \
			else \
				outsampleft += infrac * (*in); \
			in++; \
			insamps--; \
		} \
		outsampleft += (0xFFFF - inaccum) * (*in);\
		*out = outsampleft >> (16 - outlshift + outrshift); \
	}

#define LINEARDOWNSCALESTEREO(in, inrate, insamps, out, outrate, outlshift, outrshift) \
	{ \
		scale = outrate / (double)inrate; \
		infrac = floor(scale * 65536); \
		inaccum = 0; \
		insamps--; \
		outsampleft = 0; \
		outsampright = 0; \
	\
		while (insamps) \
		{ \
			inaccum += infrac; \
			if (inaccum >> 16) \
			{ \
				inaccum &= 0xFFFF; \
				outsampleft += (infrac - inaccum) * in[0]; \
				outsampright += (infrac - inaccum) * in[1]; \
				out[0] = outsampleft >> (16 - outlshift + outrshift); \
				out[1] = outsampright >> (16 - outlshift + outrshift); \
				out += 2; \
				outsampleft = inaccum * in[0]; \
				outsampright = inaccum * in[1]; \
			} \
			else \
			{ \
				outsampleft += infrac * in[0]; \
				outsampright += infrac * in[1]; \
			} \
			in += 2; \
			insamps--; \
		} \
		outsampleft += (0xFFFF - inaccum) * in[0];\
		outsampright += (0xFFFF - inaccum) * in[1];\
		out[0] = outsampleft >> (16 - outlshift + outrshift); \
		out[1] = outsampright >> (16 - outlshift + outrshift); \
	}

#define LINEARDOWNSCALESTEREOTOMONO(in, inrate, insamps, out, outrate, outlshift, outrshift) \
	{ \
		scale = outrate / (double)inrate; \
		infrac = floor(scale * 65536); \
		inaccum = 0; \
		insamps--; \
		outsampleft = 0; \
	\
		while (insamps) \
		{ \
			inaccum += infrac; \
			if (inaccum >> 16) \
			{ \
				inaccum &= 0xFFFF; \
				outsampleft += (infrac - inaccum) * ((in[0] + in[1]) >> 1); \
				*out = outsampleft >> (16 - outlshift + outrshift); \
				out++; \
				outsampleft = inaccum * ((in[0] + in[1]) >> 1); \
			} \
			else \
				outsampleft += infrac * ((in[0] + in[1]) >> 1); \
			in += 2; \
			insamps--; \
		} \
		outsampleft += (0xFFFF - inaccum) * ((in[0] + in[1]) >> 1);\
		*out = outsampleft >> (16 - outlshift + outrshift); \
	}

#define STANDARDRESCALE(in, inrate, insamps, out, outrate, outlshift, outrshift) \
	{ \
		scale = inrate / (double)outrate; \
		infrac = floor(scale * 65536); \
		outsamps = insamps / scale; \
		inaccum = 0; \
	\
		while (outsamps) \
		{ \
			*out = (*in >> outrshift) << outlshift; \
			inaccum += infrac; \
			in += (inaccum >> 16); \
			inaccum &= 0xFFFF; \
			out++; \
			outsamps--; \
		} \
	}

#define STANDARDRESCALESTEREO(in, inrate, insamps, out, outrate, outlshift, outrshift) \
	{ \
		scale = inrate / (double)outrate; \
		infrac = floor(scale * 65536); \
		outsamps = insamps / scale; \
		inaccum = 0; \
	\
		while (outsamps) \
		{ \
			out[0] = (in[0] >> outrshift) << outlshift; \
			out[1] = (in[1] >> outrshift) << outlshift; \
			inaccum += infrac; \
			in += (inaccum >> 16) * 2; \
			inaccum &= 0xFFFF; \
			out += 2; \
			outsamps--; \
		} \
	}

#define STANDARDRESCALESTEREOTOMONO(in, inrate, insamps, out, outrate, outlshift, outrshift) \
	{ \
		scale = inrate / (double)outrate; \
		infrac = floor(scale * 65536); \
		outsamps = insamps / scale; \
		inaccum = 0; \
	\
		while (outsamps) \
		{ \
			out[0] = (((in[0] >> outrshift) << outlshift) + ((in[1] >> outrshift) << outlshift)) >> 1; \
			inaccum += infrac; \
			in += (inaccum >> 16) * 2; \
			inaccum &= 0xFFFF; \
			out++; \
			outsamps--; \
		} \
	}

#define QUICKCONVERT(in, insamps, out, outlshift, outrshift) \
	{ \
		while (insamps) \
		{ \
			*out = (*in >> outrshift) << outlshift; \
			out++; \
			in++; \
			insamps--; \
		} \
	}

#define QUICKCONVERTSTEREOTOMONO(in, insamps, out, outlshift, outrshift) \
	{ \
		while (insamps) \
		{ \
			*out = (((in[0] >> outrshift) << outlshift) + ((in[1] >> outrshift) << outlshift)) >> 1; \
			out++; \
			in += 2; \
			insamps--; \
		} \
	}

// SND_ResampleStream: takes a sound stream and converts with given parameters. Limited to
// 8-16-bit signed conversions and mono-to-mono/stereo-to-stereo conversions.
// Not an in-place algorithm.
void SND_ResampleStream (void *in, int inrate, int inwidth, int inchannels, int insamps, void *out, int outrate, int outwidth, int outchannels, int resampstyle)
{
	double scale;
	signed char *in8 = (signed char *)in;
	short *in16 = (short *)in;
	signed char *out8 = (signed char *)out;
	short *out16 = (short *)out;
	int outsamps, outnlsamps, outsampleft, outsampright;
	int infrac, inaccum;

	if (insamps <= 0)
		return;

	if (inchannels == outchannels && inwidth == outwidth && inrate == outrate)
	{
		memcpy(out, in, inwidth*insamps*inchannels);
		return;
	}

	if (inchannels == 1 && outchannels == 1)
	{
		if (inwidth == 1)
		{
			if (outwidth == 1)
			{
				if (inrate < outrate) // upsample
				{
					if (resampstyle)
						LINEARUPSCALE(in8, inrate, insamps, out8, outrate, 0, 0)
					else
						STANDARDRESCALE(in8, inrate, insamps, out8, outrate, 0, 0)
				}
				else // downsample
				{
					if (resampstyle > 1)
						LINEARDOWNSCALE(in8, inrate, insamps, out8, outrate, 0, 0)
					else
						STANDARDRESCALE(in8, inrate, insamps, out8, outrate, 0, 0)
				}
				return;
			}
			else
			{
				if (inrate == outrate) // quick convert
					QUICKCONVERT(in8, insamps, out16, 8, 0)
				else if (inrate < outrate) // upsample
				{
					if (resampstyle)
						LINEARUPSCALE(in8, inrate, insamps, out16, outrate, 8, 0)
					else
						STANDARDRESCALE(in8, inrate, insamps, out16, outrate, 8, 0)
				}
				else // downsample
				{
					if (resampstyle > 1)
						LINEARDOWNSCALE(in8, inrate, insamps, out16, outrate, 8, 0)
					else
						STANDARDRESCALE(in8, inrate, insamps, out16, outrate, 8, 0)
				}
				return;
			}
		}
		else // 16-bit
		{
			if (outwidth == 2)
			{
				if (inrate < outrate) // upsample
				{
					if (resampstyle)
						LINEARUPSCALE(in16, inrate, insamps, out16, outrate, 0, 0)
					else
						STANDARDRESCALE(in16, inrate, insamps, out16, outrate, 0, 0)
				}
				else // downsample
				{
					if (resampstyle > 1)
						LINEARDOWNSCALE(in16, inrate, insamps, out16, outrate, 0, 0)
					else
						STANDARDRESCALE(in16, inrate, insamps, out16, outrate, 0, 0)
				}
				return;
			}
			else
			{
				if (inrate == outrate) // quick convert
					QUICKCONVERT(in16, insamps, out8, 0, 8)
				else if (inrate < outrate) // upsample
				{
					if (resampstyle)
						LINEARUPSCALE(in16, inrate, insamps, out8, outrate, 0, 8)
					else
						STANDARDRESCALE(in16, inrate, insamps, out8, outrate, 0, 8)
				}
				else // downsample
				{
					if (resampstyle > 1)
						LINEARDOWNSCALE(in16, inrate, insamps, out8, outrate, 0, 8)
					else
						STANDARDRESCALE(in16, inrate, insamps, out8, outrate, 0, 8)
				}
				return;
			}
		}
	}
	else if (outchannels == 2 && inchannels == 2)
	{
		if (inwidth == 1)
		{
			if (outwidth == 1)
			{
				if (inrate < outrate) // upsample
				{
					if (resampstyle)
						LINEARUPSCALESTEREO(in8, inrate, insamps, out8, outrate, 0, 0)
					else
						STANDARDRESCALESTEREO(in8, inrate, insamps, out8, outrate, 0, 0)
				}
				else // downsample
				{
					if (resampstyle > 1)
						LINEARDOWNSCALESTEREO(in8, inrate, insamps, out8, outrate, 0, 0)
					else
						STANDARDRESCALESTEREO(in8, inrate, insamps, out8, outrate, 0, 0)
				}
			}
			else
			{
				if (inrate == outrate) // quick convert
				{
					insamps *= 2;
					QUICKCONVERT(in8, insamps, out16, 8, 0)
				}
				else if (inrate < outrate) // upsample
				{
					if (resampstyle)
						LINEARUPSCALESTEREO(in8, inrate, insamps, out16, outrate, 8, 0)
					else
						STANDARDRESCALESTEREO(in8, inrate, insamps, out16, outrate, 8, 0)
				}
				else // downsample
				{
					if (resampstyle > 1)
						LINEARDOWNSCALESTEREO(in8, inrate, insamps, out16, outrate, 8, 0)
					else
						STANDARDRESCALESTEREO(in8, inrate, insamps, out16, outrate, 8, 0)
				}
			}
		}
		else // 16-bit
		{
			if (outwidth == 2)
			{
				if (inrate < outrate) // upsample
				{
					if (resampstyle)
						LINEARUPSCALESTEREO(in16, inrate, insamps, out16, outrate, 0, 0)
					else
						STANDARDRESCALESTEREO(in16, inrate, insamps, out16, outrate, 0, 0)
				}
				else // downsample
				{
					if (resampstyle > 1)
						LINEARDOWNSCALESTEREO(in16, inrate, insamps, out16, outrate, 0, 0)
					else
						STANDARDRESCALESTEREO(in16, inrate, insamps, out16, outrate, 0, 0)
				}
			}
			else 
			{
				if (inrate == outrate) // quick convert
				{
					insamps *= 2;
					QUICKCONVERT(in16, insamps, out8, 0, 8)
				}
				else if (inrate < outrate) // upsample
				{
					if (resampstyle)
						LINEARUPSCALESTEREO(in16, inrate, insamps, out8, outrate, 0, 8)
					else
						STANDARDRESCALESTEREO(in16, inrate, insamps, out8, outrate, 0, 8)
				}
				else // downsample
				{
					if (resampstyle > 1)
						LINEARDOWNSCALESTEREO(in16, inrate, insamps, out8, outrate, 0, 8)
					else
						STANDARDRESCALESTEREO(in16, inrate, insamps, out8, outrate, 0, 8)
				}
			}
		}
	}
#if 0
	else if (outchannels == 1 && inchannels == 2)
	{
		if (inwidth == 1)
		{
			if (outwidth == 1)
			{
				if (inrate < outrate) // upsample
				{
					if (resampstyle)
						LINEARUPSCALESTEREOTOMONO(in8, inrate, insamps, out8, outrate, 0, 0)
					else
						STANDARDRESCALESTEREOTOMONO(in8, inrate, insamps, out8, outrate, 0, 0)
				}
				else // downsample
					STANDARDRESCALESTEREOTOMONO(in8, inrate, insamps, out8, outrate, 0, 0)
			}
			else
			{
				if (inrate == outrate) // quick convert
					QUICKCONVERTSTEREOTOMONO(in8, insamps, out16, 8, 0)
				else if (inrate < outrate) // upsample
				{
					if (resampstyle)
						LINEARUPSCALESTEREOTOMONO(in8, inrate, insamps, out16, outrate, 8, 0)
					else
						STANDARDRESCALESTEREOTOMONO(in8, inrate, insamps, out16, outrate, 8, 0)
				}
				else // downsample
					STANDARDRESCALESTEREOTOMONO(in8, inrate, insamps, out16, outrate, 8, 0)
			}
		}
		else // 16-bit
		{
			if (outwidth == 2)
			{
				if (inrate < outrate) // upsample
				{
					if (resampstyle)
						LINEARUPSCALESTEREOTOMONO(in16, inrate, insamps, out16, outrate, 0, 0)
					else
						STANDARDRESCALESTEREOTOMONO(in16, inrate, insamps, out16, outrate, 0, 0)
				}
				else // downsample
					STANDARDRESCALESTEREOTOMONO(in16, inrate, insamps, out16, outrate, 0, 0)
			}
			else 
			{
				if (inrate == outrate) // quick convert
					QUICKCONVERTSTEREOTOMONO(in16, insamps, out8, 0, 8)
				else if (inrate < outrate) // upsample
				{
					if (resampstyle)
						LINEARUPSCALESTEREOTOMONO(in16, inrate, insamps, out8, outrate, 0, 8)
					else
						STANDARDRESCALESTEREOTOMONO(in16, inrate, insamps, out8, outrate, 0, 8)
				}
				else // downsample
					STANDARDRESCALESTEREOTOMONO(in16, inrate, insamps, out8, outrate, 0, 8)
			}
		}
	}
#endif
}

/*
================
ResampleSfx
================
*/
static void ResampleSfx (sfx_t *sfx, int inrate, int inchannels, int inwidth, int insamps, int inloopstart, byte *data)
{
	extern cvar_t s_linearresample;
	double scale;
	sfxcache_t	*sc;
	int len;
	int outsamps;
	int outwidth;
	int outchannels = 1; // inchannels;

	scale = shw->khz / (double)inrate;
	outsamps = insamps * scale;
	if (s_loadas8bit.integer < 0)
		outwidth = 2;
	else if (s_loadas8bit.integer)
		outwidth = 1;
	else
		outwidth = inwidth;
	len = outsamps * outwidth * outchannels;

	sc = sfx->buf = Q_malloc(len + sizeof(sfxcache_t));
	if (!sc)
	{
		return;
	}

	sc->format.channels = outchannels;
	sc->format.width = outwidth;
	sc->format.speed = shw->khz;
	sc->total_length = outsamps;
	if (inloopstart == -1)
		sc->loopstart = inloopstart;
	else
		sc->loopstart = inloopstart * scale;

	SND_ResampleStream (data, 
		inrate, 
		inwidth, 
		inchannels, 
		insamps, 
		sc->data, 
		sc->format.speed, 
		sc->format.width, 
		sc->format.channels, 
		s_linearresample.integer);
}

#ifndef OLD_WAV_LOADING

/*
===============================================================================
WAV loading
===============================================================================
*/

typedef struct sfviodata_s {
	char *path;
	int filesize;
	int position;
	unsigned char *data;
} sfviodata_t;

sf_count_t SFVIO_GetFilelen(void *user_data)
{
	sfviodata_t *sfviodata;

	sfviodata = user_data;
	return sfviodata->filesize;
}

sf_count_t SFVIO_Seek(sf_count_t offset, int whence, void *user_data)
{
	sfviodata_t *sfviodata;

	sfviodata = user_data;
	if (whence == SEEK_CUR)
	{
		sfviodata->position += offset;
	}
	else if (whence == SEEK_SET)
	{
		sfviodata->position = offset;
	}
	else if (whence == SEEK_END)
	{
		sfviodata->position = sfviodata->filesize + offset;
	}
	if (sfviodata->position > sfviodata->filesize)
	{
		sfviodata->position = sfviodata->filesize;
	}
	else if (sfviodata->position < 0)
	{
		sfviodata->position = 0;
	}
	return sfviodata->position;
}

sf_count_t SFVIO_Read(void *ptr, sf_count_t count, void *user_data)
{
	int i = 0;
	sfviodata_t *sfviodata;

	sfviodata = user_data;
	for (i = 0; i < count; ++i)
	{
		if (sfviodata->position == sfviodata->filesize)
		{
			return i;
		}
		((char *)ptr)[i] = sfviodata->data[sfviodata->position];
		sfviodata->position++;
	}
	return count;
}

sf_count_t SFVIO_Tell(void *user_data)
{
	sfviodata_t *sfviodata;

	sfviodata = user_data;
	return sfviodata->position;
}

static qbool S_ParseCueMark(const byte* chunk, int len, int cue_point_id, int* sample_length)
{
	int pos = 0;
	unsigned int size = 1;

	*sample_length = 0;
	while ((pos < (len - 8)) && (size != 0)) {
		size = BuffLittleLong(chunk + pos + 4);

		// Looking for ltxt chunk with purpose "mark"
		if (size >= 20 && !strncmp(chunk + pos, "ltxt", 4) && !strncmp(chunk + pos + 16, "mark", 4)) {
			// Might be for a different cue point
			if (cue_point_id == BuffLittleLong(chunk + pos + 8)) {
				*sample_length = BuffLittleLong(chunk + pos + 12);
				return true;
			}
		}

		pos += size;
	}

	return false;
}

static qbool S_FindCuePointSampleLength(SNDFILE* sndfile, int cue_point_id, int* sample_length)
{
	SF_CHUNK_INFO chunk_info;
	chunk_info.datalen = 0;
	strlcpy(chunk_info.id, "LIST", sizeof(chunk_info.id));
	chunk_info.id_size = 4;
	SF_CHUNK_ITERATOR* iterator = sf_get_chunk_iterator(sndfile, &chunk_info);
	byte chunk_data[1024];

	*sample_length = 0;

	while (iterator != NULL) {
		if (sf_get_chunk_size(iterator, &chunk_info) != SF_ERR_NO_ERROR) {
			break;
		}

		if (chunk_info.datalen >= 24 && chunk_info.datalen <= sizeof(chunk_data)) {
			chunk_info.data = chunk_data;

			if (sf_get_chunk_data(iterator, &chunk_info) != SF_ERR_NO_ERROR) {
				break;
			}
			else if (chunk_data[0] == 'a' && chunk_data[1] == 'd' && chunk_data[2] == 't' && chunk_data[3] == 'l') {
				if (S_ParseCueMark(chunk_data + 4, chunk_info.datalen - 4, cue_point_id, sample_length)) {
					return true;
				}
			}
		}

		iterator = sf_next_chunk_iterator(iterator);
	}

	return false;
}

sfxcache_t *S_LoadSound (sfx_t *s)
{
	char namebuffer[256];
	unsigned char *data;
	int filesize;
	SF_VIRTUAL_IO sfvio;
	SF_INFO sfinfo;
	sfviodata_t sfviodata;
	short *buf; 
	uint32_t cue_count;
	int loopstart;
	SF_CUES sfcues;
	SNDFILE *sndfile;

	// see if allocated
	if (s->buf)
		return s->buf;

	// load it in
	snprintf(namebuffer, sizeof(namebuffer), "sound/%s", s->name);

	if (!(data = FS_LoadTempFile(namebuffer, &filesize))) {
		Com_Printf ("Couldn't load %s\n", namebuffer);
		return NULL;
	}

	FMod_CheckModel(namebuffer, data, filesize);

	sfvio.get_filelen = SFVIO_GetFilelen;
	sfvio.seek = SFVIO_Seek;
	sfvio.read = SFVIO_Read;
	sfvio.tell = SFVIO_Tell;

	sfinfo.format = 0;

	sfviodata.path = namebuffer;
	sfviodata.position = 0;
	sfviodata.data = data;
	sfviodata.filesize = filesize;

	sndfile = sf_open_virtual(&sfvio, SFM_READ, &sfinfo, &sfviodata);

	buf = (short *)Q_malloc(sfinfo.frames * sfinfo.channels * sizeof(short));
	sf_readf_short(sndfile, buf, sfinfo.frames);
	cue_count = 0;
	sf_command(sndfile, SFC_GET_CUE_COUNT, &cue_count, sizeof(cue_count));
	loopstart = -1;

	if (cue_count != 0) {
		int loop_sample_count;

		sf_command(sndfile, SFC_GET_CUE, &sfcues, sizeof(sfcues)) ;

		if (S_FindCuePointSampleLength(sndfile, sfcues.cue_points[0].position, &loop_sample_count)) {
			loopstart = sfcues.cue_points[0].sample_offset;
			if (loopstart + loop_sample_count > sfinfo.frames) {
				Sys_Error("Sound %s has a bad loop length", s->name);
			}
			sfinfo.frames = loopstart + loop_sample_count;
		}
	}

	sf_close(sndfile);

	if (sfinfo.channels < 1 || sfinfo.channels > 2) {
		Com_Printf("%s has an unsupported number of channels (%i)\n", s->name, sfinfo.channels);
		Q_free(buf);
		return NULL;
	}

	ResampleSfx (s, sfinfo.samplerate, sfinfo.channels, sizeof(short), sfinfo.frames, loopstart, (byte *)buf);
	Q_free(buf);

	return s->buf;
}

#else

/*
===============================================================================
Old WAV loading
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

static void COM_SwapLittleShortBlock (short *s, int size)
{
//FIXME: qqshka: I have no idea that we have to do for PDP endian.

#if defined __BIG_ENDIAN__
//	if (!bigendian)
//		return;

	if (size <= 0)
		return;

	while (size)
	{
		*s = ShortSwap(*s);
		s++;
		size--;
	}
#endif
}

static void COM_CharBias (signed char *c, int size)
{
	if (size <= 0)
		return;

	while (size)
	{
		*c = (*(unsigned char *)c) - 128;
		c++;
		size--;
	}
}

sfxcache_t *S_LoadSound (sfx_t *s)
{
	char namebuffer[256];
	unsigned char *data;
	wavinfo_t info;
	int filesize;

	// see if allocated
	if (s->buf)
		return (sfxcache_t*)s->buf;

	// load it in
	snprintf(namebuffer, sizeof(namebuffer), "sound/%s", s->name);

	if (!(data = FS_LoadTempFile(namebuffer, &filesize))) {
		Com_Printf ("Couldn't load %s\n", namebuffer);
		return NULL;
	}

	FMod_CheckModel(namebuffer, data, filesize);

	info = GetWavinfo (s->name, data, filesize);

	// Stereo sounds are allowed (intended for music)
	if (info.channels < 1 || info.channels > 2) {
		Com_Printf("%s has an unsupported number of channels (%i)\n",s->name, info.channels);
		return NULL;
	}

	if (info.dataofs + info.samples * info.channels > filesize) {
		Com_Printf("%s is corrupt/truncated, delete and re-download\n", s->name);
		return NULL;
	}

	if (info.width == 1)
		COM_CharBias((signed char*)data + info.dataofs, info.samples * info.channels);
	else if (info.width == 2)
		COM_SwapLittleShortBlock((short *)(data + info.dataofs), info.samples * info.channels);

	ResampleSfx (s, info.rate, info.channels, info.width, info.samples, info.loopstart, data + info.dataofs);

	return s->buf;
}

int SND_Rate(int rate)
{
	switch (rate)
	{
		case 48:
			return 48000;
		case 44:
			return 44100;
		case 32:
			return 32000;
		case 24:
			return 24000;
		case 22:
			return 22050;
		case 16:
			return 16000;
		case 12:
			return 12000;
		case 8:
			return 8000;
		default:
			return 11025;
	}
}

#endif
