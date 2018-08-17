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
// snd_voip.c -- voice over ip (mm3)
// Original code taken from ezquake 2.2, which used speex
// Now using SDL audio capture (requires SDL 2.0.5 or above)

#include <SDL.h>
#include "quakedef.h"
#include "qsound.h"

#ifdef FTE_PEXT2_VOICECHAT

#include <speex/speex.h>
#include <speex/speex_preprocess.h>

// FTEQW type and naming compatibility
// it's not really necessary, simple find & replace would do the job too
#ifdef _MSC_VER
#define VARGS __cdecl
#endif
#ifndef VARGS
#define VARGS
#endif

static void S_Voip_Play_Callback (cvar_t *var, char *string, qbool *cancel);
void CL_SendClientCommand (qbool reliable, char *format, ...);
void S_RawAudio (int sourceid, byte *data, unsigned int speed, unsigned int samples, unsigned int channelsnum, unsigned int width);

static cvar_t s_inputdevice = { "s_inputdevice", "0" };                                // SDL device to use as microphone
static cvar_t cl_voip_send = { "cl_voip_send", "0" };                                  // Sends voice-over-ip data to the server whenever it is set.
static cvar_t cl_voip_vad_threshhold = { "cl_voip_vad_threshhold", "15" };             // This is the threshhold for voice-activation-detection when sending voip data.
static cvar_t cl_voip_vad_delay = { "cl_voip_vad_delay", "0.3" };                      // Keeps sending voice data for this many seconds after voice activation would normally stop.
static cvar_t cl_voip_capturingvol = { "cl_voip_capturingvol", "0.5" };                // Multiplier applied while capturing, to avoid your audio from being heard by others.
static cvar_t cl_voip_play = { "cl_voip_play", "1", CVAR_NONE, S_Voip_Play_Callback }; // Enables voip playback from other clients connected to server.
static cvar_t cl_voip_micamp = { "cl_voip_micamp", "2" };                              // Amplifies your microphone when using voip.
static cvar_t cl_voip_demorecord = { "cl_voip_demorecord", "1" };                      // Record VOIP in demo.
static cvar_t cl_voip_showmeter = { "cl_voip_showmeter", "1" };                        // Show speech volume above the hud.  0 = hide, 1=when transmitting, 2=even when voice-activation not triggered
static cvar_t cl_voip_showmeter_x = { "cl_voip_showmeter_x", "0" };                    // horizontal coordinate
static cvar_t cl_voip_showmeter_y = { "cl_voip_showmeter_y", "0" };                    // vertical coordinate

static float voicevolumemod = 1; // voice volume modifier.

float S_VoipVoiceTransmitVolume(void)
{
	return voicevolumemod;
}

static struct
{
	qbool inited;
	qbool loaded;

	SpeexBits encbits;
	void *encoder;
	SpeexPreprocessState *preproc;
	unsigned int framesize;
	unsigned int samplerate;

	SpeexBits decbits[MAX_CLIENTS];
	void *decoder[MAX_CLIENTS];
	unsigned char decseq[MAX_CLIENTS];	/*sender's sequence, to detect+cover minor packetloss*/
	unsigned char decgen[MAX_CLIENTS];	/*last generation. if it changes, we flush speex to reset packet loss*/
	float decamp[MAX_CLIENTS];	/*amplify them by this*/
	float lastspoke[MAX_CLIENTS];	/*time when they're no longer considered talking. if future, they're talking*/

	unsigned char capturebuf[32768]; /*pending data*/
	unsigned int capturepos;/*amount of pending data*/
	unsigned int encsequence;/*the outgoing sequence count*/
	unsigned int generation;/*incremented whenever capture is restarted*/
	qbool wantsend;	/*set if we're capturing data to send*/
	float voiplevel;	/*your own voice level*/
	unsigned int dumps;	/*trigger a new generation thing after a bit*/
	unsigned int keeps;	/*for vad_delay*/

	void *driverctx;	/*capture driver context*/
} s_speex;

static qbool S_Speex_Init (void)
{
	int i;
	const SpeexMode *mode;
	if (s_speex.inited)
		return s_speex.loaded;
	s_speex.inited = true;

	mode = speex_lib_get_mode (SPEEX_MODEID_NB);
	speex_bits_init (&s_speex.encbits);
	speex_bits_reset (&s_speex.encbits);

	s_speex.encoder = speex_encoder_init (mode);

	speex_encoder_ctl (s_speex.encoder, SPEEX_GET_FRAME_SIZE, &s_speex.framesize);
	speex_encoder_ctl (s_speex.encoder, SPEEX_GET_SAMPLING_RATE, &s_speex.samplerate);
	s_speex.samplerate = 11025;
	speex_encoder_ctl (s_speex.encoder, SPEEX_SET_SAMPLING_RATE, &s_speex.samplerate);

	s_speex.preproc = speex_preprocess_state_init (s_speex.framesize, s_speex.samplerate);

	i = 1;
	speex_preprocess_ctl (s_speex.preproc, SPEEX_PREPROCESS_SET_DENOISE, &i);

	i = 1;
	speex_preprocess_ctl (s_speex.preproc, SPEEX_PREPROCESS_SET_AGC, &i);

	for (i = 0; i < MAX_CLIENTS; i++) {
		speex_bits_init (&s_speex.decbits[i]);
		speex_bits_reset (&s_speex.decbits[i]);
		s_speex.decoder[i] = speex_decoder_init (mode);
		s_speex.decamp[i] = 1;
	}
	s_speex.loaded = true;
	return s_speex.loaded;
}

static int S_CaptureDriverInit (int sampleRate)
{
	SDL_AudioDeviceID inputdevid = 0;
	SDL_AudioSpec desired, obtained;
	int ret = 0;
	const char *requested_device = NULL;

	if (SDL_WasInit (SDL_INIT_AUDIO) == 0)
		ret = SDL_InitSubSystem (SDL_INIT_AUDIO);

	if (ret == -1) {
		Con_Printf ("Couldn't initialize SDL audio: %s\n", SDL_GetError ());
		return false;
	}

	memset (&desired, 0, sizeof (desired));
	desired.freq = sampleRate;
	desired.samples = 64;
	desired.format = AUDIO_S16LSB;
	desired.channels = 1;

	/* Make audiodevice list start from index 1 so that 0 can be system default */
	if (s_inputdevice.integer > 0) {
		requested_device = SDL_GetAudioDeviceName (s_inputdevice.integer - 1, 0);
	}

	if ((inputdevid = SDL_OpenAudioDevice (requested_device, 1, &desired, &obtained, 0)) <= 0) {
		Com_Printf ("sound: couldn't open SDL audio: %s\n", SDL_GetError ());
		if (requested_device != NULL) {
			Com_Printf ("sound: retrying with default audio device\n");
			if ((inputdevid = SDL_OpenAudioDevice (NULL, 1, &desired, &obtained, 0)) <= 0) {
				Com_Printf ("sound: failure again, aborting...\n");
				return 0;
			}
			Cvar_LatchedSet (&s_inputdevice, "0");
		}
	}

	if (obtained.format != AUDIO_S16LSB) {
		Com_Printf ("SDL audio format %d unsupported.\n", obtained.format);
		SDL_CloseAudioDevice (inputdevid);
		inputdevid = 0;
		return 0;
	}

	if (obtained.channels != 1 && obtained.channels != 2) {
		Com_Printf ("SDL audio channels %d unsupported.\n", obtained.channels);
		SDL_CloseAudioDevice (inputdevid);
		inputdevid = 0;
		return 0;
	}

	Com_Printf ("Using SDL audio capture driver: %s @ %d Hz (samplerate %d)\n", SDL_GetCurrentAudioDriver (), obtained.freq, obtained.samples);
	SDL_PauseAudioDevice (inputdevid, 0);

	return inputdevid;
}

static void S_CaptureDriverStart (void *ctx)
{
	SDL_AudioDeviceID inputdevid = (SDL_AudioDeviceID)ctx;

	SDL_PauseAudioDevice (inputdevid, 0);
}

static void S_CaptureDriverStop (void *ctx)
{
	SDL_AudioDeviceID inputdevid = (SDL_AudioDeviceID)ctx;

	SDL_PauseAudioDevice (inputdevid, 1);
}

static void S_CaptureDriverShutdown (void *ctx)
{
	SDL_AudioDeviceID inputdevid = (SDL_AudioDeviceID)ctx;

	if (inputdevid) {
		SDL_CloseAudioDevice (inputdevid);
	}
}

static unsigned int S_CaptureDriverUpdate (void* driverContext, unsigned char* buffer, int minBytes, int maxBytes)
{
	SDL_AudioDeviceID inputdevid = (SDL_AudioDeviceID)driverContext;
	unsigned int available = SDL_GetQueuedAudioSize (inputdevid);

	if (available > minBytes) {
		return SDL_DequeueAudio (inputdevid, buffer, maxBytes);
	}

	return 0;
}

typedef struct voip_data_s {
	unsigned int sender;
	unsigned char gen;
	unsigned char seq;
	int bytes;
	unsigned char data[1024];
} voip_data_t;

// Called when data is received from server
// TODO: store the voip data and then process in the main S_Update() call
void S_Voip_Parse (void)
{
	unsigned int sender;
	int bytes;
	unsigned char data[1024], *start;
	short decodebuf[1024];
	unsigned int decodesamps, len, newseq, drops;
	unsigned char seq, gen;
	float amp = 1;
	unsigned int i;

	sender = MSG_ReadByte ();
	gen = MSG_ReadByte ();
	seq = MSG_ReadByte ();
	bytes = MSG_ReadShort ();

	if (bytes > sizeof (data) || !cl_voip_play.integer || !S_Speex_Init () || (gen & 0xf0)) {
		Com_DPrintf ("skip data: %d\n", bytes);
		MSG_ReadSkip (bytes);
		return;
	}
	MSG_ReadData (data, bytes);

	sender &= MAX_CLIENTS - 1;

	amp = s_speex.decamp[sender];

	decodesamps = 0;
	newseq = 0;
	drops = 0;
	start = data;

	s_speex.lastspoke[sender] = cls.realtime + 0.5;
	if (s_speex.decgen[sender] != gen) {
		speex_bits_reset (&s_speex.decbits[sender]);
		s_speex.decgen[sender] = gen;
		s_speex.decseq[sender] = seq;
	}

	while (bytes > 0) {
		if (decodesamps + s_speex.framesize > sizeof (decodebuf) / sizeof (decodebuf[0])) {
			S_RawAudio (sender, (byte*)decodebuf, s_speex.samplerate, decodesamps, 1, 2);
			decodesamps = 0;
		}

		if (s_speex.decseq[sender] != seq) {
			speex_decode_int (s_speex.decoder[sender], NULL, decodebuf + decodesamps);
			s_speex.decseq[sender]++;
			drops++;
		}
		else {
			bytes--;
			len = *start++;
			speex_bits_read_from (&s_speex.decbits[sender], (char *)start, len);
			bytes -= len;
			start += len;
			speex_decode_int (s_speex.decoder[sender], &s_speex.decbits[sender], decodebuf + decodesamps);
			newseq++;
		}
		if (amp != 1) {
			for (i = decodesamps; i < decodesamps + s_speex.framesize; i++)
				decodebuf[i] *= amp;
		}
		decodesamps += s_speex.framesize;
	}
	s_speex.decseq[sender] += newseq;

	if (drops)
		Con_DPrintf ("%i dropped audio frames\n", drops);

	if (decodesamps > 0) {
		S_RawAudio (sender, (byte*)decodebuf, s_speex.samplerate, decodesamps, 1, 2);
	}
}

// Called just prior to sending command to server
void S_Voip_Transmit (unsigned char clc, sizebuf_t *buf)
{
	unsigned char outbuf[1024];
	unsigned int outpos;//in bytes
	unsigned int encpos;//in bytes
	short *start;
	unsigned char initseq;//in frames
	unsigned int i;
	unsigned int samps;
	float level, f;
	float micamp = cl_voip_micamp.value;
	qbool voipsendenable = (cl_voip_play.integer && (cls.fteprotocolextensions2 & FTE_PEXT2_VOICECHAT));

	if (!voipsendenable) {
		S_Capture_Shutdown();
		return;
	}

	voipsendenable = cl_voip_send.integer > 0;

	if (!s_speex.driverctx) {
		s_speex.voiplevel = -1;
		/*only init the first time capturing is requested*/
		if (!voipsendenable)
			return;

		/*see if we can init speex...*/
		if (!S_Speex_Init ())
			return;

		s_speex.driverctx = (void*)S_CaptureDriverInit (s_speex.samplerate);
	}

	/*couldn't init a driver?*/
	if (!s_speex.driverctx) {
		return;
	}

	if (!voipsendenable && s_speex.wantsend) {
		s_speex.wantsend = false;
		s_speex.capturepos += S_CaptureDriverUpdate (s_speex.driverctx, (unsigned char*)s_speex.capturebuf + s_speex.capturepos, 1, sizeof (s_speex.capturebuf) - s_speex.capturepos);
		S_CaptureDriverStop (s_speex.driverctx);
		/*note: we still grab audio to flush everything that was captured while it was active*/
	}
	else if (voipsendenable && !s_speex.wantsend) {
		s_speex.wantsend = true;
		if (!s_speex.capturepos) {	/*if we were actually still sending, it was probably only off for a single frame, in which case don't reset it*/
			s_speex.dumps = 0;
			s_speex.generation++;
			s_speex.encsequence = 0;
			speex_bits_reset (&s_speex.encbits);
		}
		else {
			s_speex.capturepos += S_CaptureDriverUpdate (s_speex.driverctx, (unsigned char*)s_speex.capturebuf + s_speex.capturepos, 1, sizeof (s_speex.capturebuf) - s_speex.capturepos);
		}
		S_CaptureDriverStart (s_speex.driverctx);

		voicevolumemod = cl_voip_capturingvol.value;
	}

	s_speex.capturepos += S_CaptureDriverUpdate (s_speex.driverctx, (unsigned char*)s_speex.capturebuf + s_speex.capturepos, s_speex.framesize * 2, sizeof (s_speex.capturebuf) - s_speex.capturepos);
	if (!s_speex.wantsend && s_speex.capturepos < s_speex.framesize * 2) {
		s_speex.voiplevel = -1;
		s_speex.capturepos = 0;
		voicevolumemod = 1;
		return;
	}

	initseq = s_speex.encsequence;
	level = 0;
	samps = 0;
	for (encpos = 0, outpos = 0; s_speex.capturepos - encpos >= s_speex.framesize * 2 && sizeof (outbuf) - outpos > 64; s_speex.encsequence++) {
		start = (short*)(s_speex.capturebuf + encpos);

		speex_preprocess_run (s_speex.preproc, start);

		for (i = 0; i < s_speex.framesize; i++) {
			f = start[i] * micamp;
			start[i] = f;
			f = abs (start[i]);
			level += f*f;
		}
		samps += s_speex.framesize;

		speex_bits_reset (&s_speex.encbits);
		speex_encode_int (s_speex.encoder, start, &s_speex.encbits);
		outbuf[outpos] = speex_bits_write (&s_speex.encbits, (char *)outbuf + outpos + 1, sizeof (outbuf) - (outpos + 1));
		outpos += 1 + outbuf[outpos];
		encpos += s_speex.framesize * 2;
	}

	if (samps) {
		float nl = (3000 * level) / (32767.0f * 32767 * samps);

		s_speex.voiplevel = (s_speex.voiplevel * 7 + nl) / 8;
		if (s_speex.voiplevel < cl_voip_vad_threshhold.integer && !(cl_voip_send.integer & 2)) {
			/*try and dump it, it was too quiet, and they're not pressing +voip*/
			if (s_speex.keeps > samps) {
				/*but not instantly*/
				s_speex.keeps -= samps;
			}
			else {
				outpos = 0;
				s_speex.dumps += samps;
				s_speex.keeps = 0;
			}
		}
		else {
			s_speex.keeps = s_speex.samplerate * cl_voip_vad_delay.value;
		}

		if (outpos) {
			if (s_speex.dumps > s_speex.samplerate / 4) {
				s_speex.generation++;
			}
			s_speex.dumps = 0;
		}
	}

	if (outpos && buf->maxsize - buf->cursize >= outpos + 4) {
		MSG_WriteByte (buf, clc);
		MSG_WriteByte (buf, (s_speex.generation & 0x0f)); /*gonna leave that nibble clear here... in this version, the client will ignore packets with those bits set. can use them for codec or something*/
		MSG_WriteByte (buf, initseq);
		MSG_WriteShort (buf, outpos);
		SZ_Write (buf, outbuf, outpos);
	}

	/*remove sent data*/
	memmove (s_speex.capturebuf, s_speex.capturebuf + encpos, s_speex.capturepos - encpos);
	s_speex.capturepos -= encpos;
}

// Called when VOIP playback is toggled - pass stop/start to server
static void S_Voip_Play_Callback (cvar_t *var, char *string, qbool *cancel)
{
	if (cls.fteprotocolextensions2 & FTE_PEXT2_VOICECHAT) {
		if (atoi (string))
			CL_SendClientCommand (true, "unmuteall");
		else
			CL_SendClientCommand (true, "muteall");
	}
}

// Add to the ignore list
void S_Voip_Ignore (unsigned int slot, qbool ignore)
{
	if (cls.fteprotocolextensions2 & FTE_PEXT2_VOICECHAT) {
		CL_SendClientCommand (true, "vignore %i %i", slot, ignore);
	}
}

// +voip
static void S_Voip_Enable_f (void)
{
	Cvar_SetValue (&cl_voip_send, cl_voip_send.integer | 2);
}

// -voip
static void S_Voip_Disable_f (void)
{
	Cvar_SetValue (&cl_voip_send, cl_voip_send.integer & ~2);
}

// 
void S_Voip_RegisterCvars (void)
{
	Cvar_SetCurrentGroup (CVAR_GROUP_SOUND);
	Cvar_Register (&cl_voip_send);
	Cvar_Register (&cl_voip_vad_threshhold);
	Cvar_Register (&cl_voip_vad_delay);
	Cvar_Register (&cl_voip_capturingvol);
	Cvar_Register (&cl_voip_play);
	Cvar_Register (&cl_voip_micamp);
	Cvar_Register (&cl_voip_demorecord);
	Cvar_Register (&cl_voip_showmeter);
	Cvar_Register (&cl_voip_showmeter_x);
	Cvar_Register (&cl_voip_showmeter_y);

	Cmd_AddCommand ("+voip", S_Voip_Enable_f);
	Cmd_AddCommand ("-voip", S_Voip_Disable_f);
}

// Called after new serverdata received
void S_Voip_MapChange (void)
{
	Cvar_ForceCallback (&cl_voip_play);
}

int S_Voip_Loudness(void)
{
	qbool ignorevad = cl_voip_showmeter.integer == 2;

	if (s_speex.voiplevel > 100)
		return 100;
	if (!s_speex.driverctx || (!ignorevad && s_speex.dumps))
		return -1;
	return s_speex.voiplevel;
}

qbool S_Voip_ShowMeter (int* x, int* y)
{
	*x = cl_voip_showmeter_x.integer + 10;
	*y = cl_voip_showmeter_y.integer + vid.height - 8 - 10;

	return cl_voip_showmeter.integer;
}

qbool S_Voip_Speaking(unsigned int plno)
{
	if (plno == cl.playernum) {
		return s_speex.voiplevel >= cl_voip_vad_threshhold.integer || (cl_voip_send.integer & 2);
	}

	return plno < MAX_CLIENTS && s_speex.lastspoke[plno] > cls.realtime;
}

void S_Capture_Shutdown(void)
{
	S_CaptureDriverShutdown (s_speex.driverctx);
	memset(&s_speex, 0, sizeof(s_speex));
}

#endif
