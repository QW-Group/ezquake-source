/*
 * Copyright (C) 2026 Oscar Linderholm <osm@recv.se>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include "quakedef.h"
#include "qsound.h"
#include "sndfile.h"

#define QIZMO_VOICE_GSM_BYTES	33
#define QIZMO_VOICE_RATE	8000
#define QIZMO_VOICE_PCM_SAMPLES	160

// Keep a bounded GSM segment so libsndfile decodes with prior GSM state. Each
// frame is 20 ms, at the cap, playback continues from a fresh segment.
#define QIZMO_VOICE_MAX_FRAMES	1024

// libsndfile decodes from file-like objects, so expose the accumulated GSM
// bytes as a read-only in-memory file.
typedef struct qizmo_voice_sfvio_s {
	const byte *data;
	sf_count_t size;
	sf_count_t position;
} qizmo_voice_sfvio_t;

static sf_count_t S_QizmoVoice_GetFileLen(void *user_data)
{
	qizmo_voice_sfvio_t *vio = user_data;
	return vio->size;
}

static sf_count_t S_QizmoVoice_Seek(sf_count_t offset, int whence, void *user_data)
{
	qizmo_voice_sfvio_t *vio = user_data;

	if (whence == SEEK_CUR) {
		vio->position += offset;
	}
	else if (whence == SEEK_SET) {
		vio->position = offset;
	}
	else if (whence == SEEK_END) {
		vio->position = vio->size + offset;
	}

	if (vio->position < 0) {
		vio->position = 0;
	}
	else if (vio->position > vio->size) {
		vio->position = vio->size;
	}

	return vio->position;
}

static sf_count_t S_QizmoVoice_Read(void *ptr, sf_count_t count, void *user_data)
{
	qizmo_voice_sfvio_t *vio = user_data;
	sf_count_t available = vio->size - vio->position;

	if (count > available) {
		count = available;
	}

	memcpy(ptr, vio->data + vio->position, count);
	vio->position += count;
	return count;
}

static sf_count_t S_QizmoVoice_Tell(void *user_data)
{
	qizmo_voice_sfvio_t *vio = user_data;
	return vio->position;
}

// Decode the segment from the beginning so the GSM decoder sees the prior
// frame history it needs. The caller submits only the newest decoded frame.
static sf_count_t S_QizmoVoice_DecodeSegment(const byte *gsm_segment,
	int segment_frames, short *decoded_pcm)
{
	static SF_VIRTUAL_IO sfvio = {
		S_QizmoVoice_GetFileLen,
		S_QizmoVoice_Seek,
		S_QizmoVoice_Read,
		NULL,
		S_QizmoVoice_Tell
	};
	SF_INFO sfinfo;
	qizmo_voice_sfvio_t sfviodata;
	SNDFILE *sndfile;
	sf_count_t decoded_samples;

	memset(&sfinfo, 0, sizeof(sfinfo));
	sfinfo.samplerate = QIZMO_VOICE_RATE;
	sfinfo.channels = 1;
	sfinfo.format = SF_FORMAT_RAW | SF_FORMAT_GSM610;

	sfviodata.data = gsm_segment;
	sfviodata.size = segment_frames * QIZMO_VOICE_GSM_BYTES;
	sfviodata.position = 0;

	sndfile = sf_open_virtual(&sfvio, SFM_READ, &sfinfo, &sfviodata);
	if (!sndfile) {
		Com_DPrintf("Qizmo voice: could not open GSM frame: %s\n", sf_strerror(NULL));
		return 0;
	}

	decoded_samples = sf_readf_short(sndfile, decoded_pcm,
		segment_frames * QIZMO_VOICE_PCM_SAMPLES);
	sf_close(sndfile);
	return decoded_samples;
}

void S_QizmoVoice_PlayFrame(int sequence, int voice_id, const byte *frame_data, int bytes)
{
	static byte gsm_segment[QIZMO_VOICE_MAX_FRAMES * QIZMO_VOICE_GSM_BYTES];
	static short decoded_pcm[QIZMO_VOICE_MAX_FRAMES * QIZMO_VOICE_PCM_SAMPLES];
	static int last_sequence;
	static int last_voice_id;
	static int segment_frames;
	static qbool have_sequence;
	sf_count_t decoded_samples;
	short *latest_pcm_frame;
	byte *gsm_frame;

	if (!snd_initialized || !snd_started) {
		return;
	}

	if (bytes != QIZMO_VOICE_GSM_BYTES) {
		Com_DPrintf("Qizmo voice: unexpected GSM frame size %d\n", bytes);
		return;
	}

	// The Qizmo sequence groups packets into one voice transmission. A new
	// voice id, sequence, gap, or the bounded history limit starts a fresh GSM
	// decoder segment.
	if (sequence == 0 || !have_sequence ||
		last_voice_id != voice_id ||
		((last_sequence + 1) & 0x3ff) != sequence ||
		segment_frames >= QIZMO_VOICE_MAX_FRAMES) {
		segment_frames = 0;
	}

	gsm_frame = gsm_segment + segment_frames * QIZMO_VOICE_GSM_BYTES;
	memcpy(gsm_frame, frame_data, QIZMO_VOICE_GSM_BYTES);

	// Qizmo stores GSM frames with a different high nibble than standard raw
	// GSM 06.10. Preserve the low bits and normalize the magic nibble.
	gsm_frame[0] = (gsm_frame[0] & 0x0f) | 0xd0;

	segment_frames++;
	last_sequence = sequence;
	last_voice_id = voice_id;
	have_sequence = true;

	decoded_samples = S_QizmoVoice_DecodeSegment(gsm_segment, segment_frames, decoded_pcm);
	if (decoded_samples < QIZMO_VOICE_PCM_SAMPLES) {
		Com_DPrintf("Qizmo voice: decoded only %d PCM samples\n", (int)decoded_samples);
		return;
	}

	latest_pcm_frame = decoded_pcm + decoded_samples - QIZMO_VOICE_PCM_SAMPLES;

	S_RawAudio(RAW_SOURCE_QIZMO_VOICE,
		(byte *)latest_pcm_frame,
		QIZMO_VOICE_RATE,
		QIZMO_VOICE_PCM_SAMPLES,
		1,
		sizeof(short));
}
