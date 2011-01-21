/*
Copyright (C) 2010 Morten Bojsen-Hansen <morten@alas.dk>
Ported to ezQuake by dimman

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

#include "quakedef.h"
#include "qsound.h"
#include <string.h>
#include <dlfcn.h>
#include <pulse/pulseaudio.h>

#define PULSEAUDIO_CLIENT_NAME "ezquake"
#define PULSEAUDIO_STREAM_NAME "audio stream"


/////////////////////////////////////////////////////////////////////
// cvars
/////////////////////////////////////////////////////////////////////

extern cvar_t s_pulseaudio_latency; // moved to snd_dma.c atm..

/////////////////////////////////////////////////////////////////////
// private data
/////////////////////////////////////////////////////////////////////

struct pulseaudio_private
{
	int bufferpos;
	int buffersize;
	int samplesize;
	void *buffer;

	pa_threaded_mainloop *loop;
	pa_context *context;
	pa_stream *stream;
	pa_sample_spec sample_spec;
	pa_buffer_attr buffer_attr;

	void *sharedobject;

	pa_threaded_mainloop* (*pa_threaded_mainloop_new)();
	void (*pa_threaded_mainloop_free)(pa_threaded_mainloop*);
	int (*pa_threaded_mainloop_start)(pa_threaded_mainloop*);
	void (*pa_threaded_mainloop_stop)(pa_threaded_mainloop*);
	pa_mainloop_api* (*pa_threaded_mainloop_get_api)(pa_threaded_mainloop*);
	void (*pa_threaded_mainloop_lock)(pa_threaded_mainloop*);
	void (*pa_threaded_mainloop_unlock)(pa_threaded_mainloop*);
	void (*pa_threaded_mainloop_wait)(pa_threaded_mainloop*);
	void (*pa_threaded_mainloop_signal)(pa_threaded_mainloop*, int);

	pa_context* (*pa_context_new)(pa_mainloop_api*, const char*);
	void (*pa_context_unref)(pa_context*);
	int (*pa_context_connect)(pa_context*, const char*, pa_context_flags_t, const pa_spawn_api*);
	int (*pa_context_disconnect)(pa_context*);
	pa_context_state_t (*pa_context_get_state)(pa_context*);
	void (*pa_context_set_state_callback)(pa_context*, pa_context_notify_cb_t, void*);

	pa_stream* (*pa_stream_new)(pa_context*, const char*, const pa_sample_spec*, const pa_channel_map*);
	void (*pa_stream_unref)(pa_stream*);
	int (*pa_stream_connect_playback)(pa_stream*, const char*, const pa_buffer_attr*, pa_stream_flags_t, const pa_cvolume*, pa_stream*);
	int (*pa_stream_disconnect)(pa_stream*);
	pa_stream_state_t (*pa_stream_get_state)(pa_stream*);
	void (*pa_stream_set_state_callback)(pa_stream*, pa_stream_notify_cb_t, void*);
	int (*pa_stream_write)(pa_stream*, const void*, size_t, pa_free_cb_t, int64_t, pa_seek_mode_t);
	size_t (*pa_stream_writable_size)(pa_stream*);

	size_t (*pa_usec_to_bytes)(pa_usec_t, const pa_sample_spec*);
};

//////////////////////////////
static struct pulseaudio_private *p;


/////////////////////////////////////////////////////////////////////
// prototypes
/////////////////////////////////////////////////////////////////////

//static void pulseaudio_cvarinit();
static qbool pulseaudio_init(struct sounddriver_t *sd, int rate, int channels, int bits);
static int pulseaudio_getdmapos(void);
static int pulseaudio_getavail(void);
static void pulseaudio_submit(unsigned int count);
static void pulseaudio_shutdown(void);

static qbool pulseaudio_internal_initso(void);
static qbool pulseaudio_internal_initpulse(int rate, int channels, int bits);
static void pulseaudio_internal_submit(unsigned int max);

static void pulseaudio_context_state_callback(pa_context *context, void *userdata);
static void pulseaudio_stream_state_callback(pa_stream *stream, void *userdata);



/////////////////////////////////////////////////////////////////////
// public fuctions
/////////////////////////////////////////////////////////////////////

qbool SNDDMA_Init_PULSEAUDIO (struct sounddriver_t *sd)
{
	qbool ret;
	int channels, rate;
	int bits = 16;
	channels = Cvar_Value("s_stereo") ? 2 : 1;
	if(Cvar_Value("s_bits")) {
		bits = Cvar_Value("s_bits");
		if(bits != 8 && bits != 16) {
			Com_Printf("Error: invalid s_bits value(%d)! Valid are 8 or 16.", bits);
			return false;
		}
	}
	rate = SND_Rate((int)s_khz.value);
	ret = pulseaudio_init(sd, rate, channels, bits);
	return ret;
}
static qbool pulseaudio_init(struct sounddriver_t *sd, int rate, int channels, int bits)
{
	struct pulseaudio_private *tmp;

	if (!(tmp = malloc(sizeof(*tmp))))
	{
		Com_DPrintf("pulseaudio_init: Malloc failed\n");
		pulseaudio_shutdown();
		return false;
	}

	memset(tmp, 0, sizeof(*tmp));
	p=tmp; //make sure to point our static driver to the newly allocated one

	if (!pulseaudio_internal_initso())
	{
		Com_DPrintf("pulseaudio_init: Failed to initialize shared library\n");
		pulseaudio_shutdown();
		return false;
	}

	if (!pulseaudio_internal_initpulse(rate, channels, bits)) {
		Com_DPrintf("pulseaudio_init: Failed to initialize PulseAudio\n");
		pulseaudio_shutdown();
		return false;
	}

	p->bufferpos = 0;
	p->buffersize = p->buffer_attr.tlength;
	p->samplesize = bits/8;

	if (!(p->buffer = malloc(p->buffersize)))
	{
		Com_DPrintf("pulseaudio_init: Unable to allocate buffer");
		pulseaudio_shutdown();
		return false;
	}

	memset(p->buffer, 0, p->buffersize);

	shm->samplepos = 0;
	shm->samples = p->buffersize / p->samplesize;
	shm->sampleframes = (p->buffersize/p->samplesize)/2;
	shm->format.speed = rate;
	shm->format.channels = channels;
	shm->format.width = (bits == 16) ? 2 : 1;
	shm->buffer = p->buffer;

	sd->GetDMAPos = pulseaudio_getdmapos;
	sd->GetAvail = pulseaudio_getavail;
	sd->Submit = pulseaudio_submit;
	sd->Shutdown = pulseaudio_shutdown;
	sd->type = SND_PULSEAUDIO;

	return true;
}

static int pulseaudio_getdmapos(void)
{
	return p->bufferpos / p->samplesize;
}

static int pulseaudio_getavail(void)
{
	int avail;

	avail = p->pa_stream_writable_size(p->stream);

	// in case of error we don't want anything submitted
	if (avail < 0)
		avail = 0;

	return avail / p->samplesize;
}

static void pulseaudio_submit(unsigned int count)
{
	pulseaudio_internal_submit(count*p->samplesize);
}


static void pulseaudio_shutdown(void)
{
 if (p)
        {
                if (p->sharedobject)
                {
                        if (p->loop)
                                p->pa_threaded_mainloop_stop(p->loop);

                        if (p->stream)
                        {
                                p->pa_stream_disconnect(p->stream);
                                p->pa_stream_unref(p->stream);
                                p->stream = NULL;
                        }

                        if (p->context)
                        {
                                p->pa_context_disconnect(p->context);
                                p->pa_context_unref(p->context);
                                p->context = NULL;
                        }

                        if (p->loop)
                        {
                                p->pa_threaded_mainloop_free(p->loop);
                                p->loop = NULL;
                        }

                        dlclose(p->sharedobject);
                        p->sharedobject = NULL;
                }

                if (p->buffer)
                {
                        free(p->buffer);
                        p->buffer = NULL;
                        shm->buffer = NULL;
                }

                free(p);
                p = NULL;
        }
}


/////////////////////////////////////////////////////////////////////
// private functions
/////////////////////////////////////////////////////////////////////


static qbool pulseaudio_internal_initso(void)
{
	p->sharedobject = dlopen("libpulse.so", RTLD_LAZY|RTLD_LOCAL);

	if (!p->sharedobject)
		p->sharedobject = dlopen("libpulse.so.0", RTLD_LAZY|RTLD_LOCAL);

	if (!p->sharedobject)
		return false;

	p->pa_threaded_mainloop_new = dlsym(p->sharedobject, "pa_threaded_mainloop_new");
	p->pa_threaded_mainloop_free = dlsym(p->sharedobject, "pa_threaded_mainloop_free");
	p->pa_threaded_mainloop_start = dlsym(p->sharedobject, "pa_threaded_mainloop_start");
	p->pa_threaded_mainloop_stop = dlsym(p->sharedobject, "pa_threaded_mainloop_stop");
	p->pa_threaded_mainloop_get_api = dlsym(p->sharedobject, "pa_threaded_mainloop_get_api");
	p->pa_threaded_mainloop_lock = dlsym(p->sharedobject, "pa_threaded_mainloop_lock");
	p->pa_threaded_mainloop_unlock = dlsym(p->sharedobject, "pa_threaded_mainloop_unlock");
	p->pa_threaded_mainloop_wait = dlsym(p->sharedobject, "pa_threaded_mainloop_wait");
	p->pa_threaded_mainloop_signal = dlsym(p->sharedobject, "pa_threaded_mainloop_signal");

	p->pa_context_new = dlsym(p->sharedobject, "pa_context_new");
	p->pa_context_unref = dlsym(p->sharedobject, "pa_context_unref");
	p->pa_context_connect = dlsym(p->sharedobject, "pa_context_connect");
	p->pa_context_disconnect = dlsym(p->sharedobject, "pa_context_disconnect");
	p->pa_context_get_state = dlsym(p->sharedobject, "pa_context_get_state");
	p->pa_context_set_state_callback = dlsym(p->sharedobject, "pa_context_set_state_callback");

	p->pa_stream_new = dlsym(p->sharedobject, "pa_stream_new");
	p->pa_stream_unref = dlsym(p->sharedobject, "pa_stream_unref");
	p->pa_stream_connect_playback = dlsym(p->sharedobject, "pa_stream_connect_playback");
	p->pa_stream_disconnect = dlsym(p->sharedobject, "pa_stream_disconnect");
	p->pa_stream_get_state = dlsym(p->sharedobject, "pa_stream_get_state");
	p->pa_stream_set_state_callback = dlsym(p->sharedobject, "pa_stream_set_state_callback");
	p->pa_stream_write = dlsym(p->sharedobject, "pa_stream_write");
	p->pa_stream_writable_size = dlsym(p->sharedobject, "pa_stream_writable_size");

	p->pa_usec_to_bytes = dlsym(p->sharedobject, "pa_usec_to_bytes");

	if (!p->pa_threaded_mainloop_new
		|| !p->pa_threaded_mainloop_free
		|| !p->pa_threaded_mainloop_start
		|| !p->pa_threaded_mainloop_stop
		|| !p->pa_threaded_mainloop_get_api
		|| !p->pa_threaded_mainloop_lock
		|| !p->pa_threaded_mainloop_unlock
		|| !p->pa_threaded_mainloop_wait
		|| !p->pa_threaded_mainloop_signal
		|| !p->pa_context_new
		|| !p->pa_context_unref
		|| !p->pa_context_connect
		|| !p->pa_context_disconnect
		|| !p->pa_context_get_state
		|| !p->pa_context_set_state_callback
		|| !p->pa_stream_new
		|| !p->pa_stream_unref
		|| !p->pa_stream_connect_playback
		|| !p->pa_stream_disconnect
		|| !p->pa_stream_get_state
		|| !p->pa_stream_set_state_callback
		|| !p->pa_stream_write
		|| !p->pa_stream_writable_size
		|| !p->pa_usec_to_bytes)
	{
		return false;
	}

	return true;
}

static qbool pulseaudio_internal_initpulse(int rate, int channels, int bits)
{
	p->sample_spec.format = bits == 16 ? PA_SAMPLE_S16LE : PA_SAMPLE_U8;
	p->sample_spec.rate = rate;
	p->sample_spec.channels = channels;

	p->buffer_attr.fragsize = (uint32_t)-1;
	p->buffer_attr.maxlength = (uint32_t)-1;
	p->buffer_attr.minreq = (uint32_t)-1;
	p->buffer_attr.prebuf = (uint32_t)-1;
	p->buffer_attr.tlength = p->pa_usec_to_bytes(s_pulseaudio_latency.value * 1e6, &p->sample_spec);

	if (!(p->loop = p->pa_threaded_mainloop_new()))
	{
		//DEBUG("Failed to allocate main loop");
		return false;
	}

	if (!(p->context = p->pa_context_new(p->pa_threaded_mainloop_get_api(p->loop), PULSEAUDIO_CLIENT_NAME)))
	{
		//DEBUG("Failed to allocate context");
		return false;
	}

	p->pa_context_set_state_callback(p->context, pulseaudio_context_state_callback, p);

	if (p->pa_context_connect(p->context, NULL, 0, NULL) < 0)
	{
		//DEBUG("Unable to connect to server");
		return false;
	}

	p->pa_threaded_mainloop_lock(p->loop);

	if (p->pa_threaded_mainloop_start(p->loop) < 0)
	{
		//DEBUG("Unable to start main loop");
		p->pa_threaded_mainloop_unlock(p->loop);
		return false;
	}

	// wait until context is ready
	p->pa_threaded_mainloop_wait(p->loop);

	if (p->pa_context_get_state(p->context) != PA_CONTEXT_READY)
	{
		//DEBUG("Unable to connect to server");
		p->pa_threaded_mainloop_unlock(p->loop);
		return false;
	}

	if (!(p->stream = p->pa_stream_new(p->context, PULSEAUDIO_STREAM_NAME, &p->sample_spec, NULL)))
	{
		//DEBUG("Unable to allocate stream");
		p->pa_threaded_mainloop_unlock(p->loop);
		return false;
	}

	p->pa_stream_set_state_callback(p->stream, pulseaudio_stream_state_callback, p);

	if (p->pa_stream_connect_playback(p->stream, NULL, &p->buffer_attr, PA_STREAM_ADJUST_LATENCY, NULL, NULL) < 0)
	{
		//DEBUG("Unable to connect stream");
		p->pa_threaded_mainloop_unlock(p->loop);
		return false;
	}

	// wait until stream is ready
	p->pa_threaded_mainloop_wait(p->loop);	

	if (p->pa_stream_get_state(p->stream) != PA_STREAM_READY)
	{
		//DEBUG("Unable to connect stream");
		p->pa_threaded_mainloop_unlock(p->loop);
		return false;
	}

	p->pa_threaded_mainloop_unlock(p->loop);

	return true;
}

static void pulseaudio_internal_submit(unsigned int max)
{
	unsigned int count;
	int avail;

	//DEBUG("submit(%d)", max);

	p->pa_threaded_mainloop_lock(p->loop);

	while (max)
	{
		count = min(p->buffersize - p->bufferpos, max);
		avail = p->pa_stream_writable_size(p->stream);

		// in case of error, don't submit anything
		if (avail < 0)
			break;

		//DEBUG("count(%d) avail(%d)", count, avail);

		count = min(count, avail);

		if (count == 0)
			break;

		// in case of error, don't submit anything
		if (p->pa_stream_write(p->stream, p->buffer+p->bufferpos, count, NULL, 0, PA_SEEK_RELATIVE) < 0)
		{
			//DEBUG("Unable to write to stream");
			break;
		}

		max -= count;
		p->bufferpos += count;

		if (p->bufferpos == p->buffersize)
			p->bufferpos = 0;
	}

	p->pa_threaded_mainloop_unlock(p->loop);
}



/////////////////////////////////////////////////////////////////////
// callbacks
/////////////////////////////////////////////////////////////////////

static void pulseaudio_context_state_callback(pa_context *context, void *userdata)
{
	struct pulseaudio_private *p;

	p = userdata;

	switch (p->pa_context_get_state(context))
	{
		case PA_CONTEXT_READY:
		case PA_CONTEXT_FAILED:
		case PA_CONTEXT_TERMINATED:
			p->pa_threaded_mainloop_signal(p->loop, 0);
			break;

		default:
			break;
	}
}

static void pulseaudio_stream_state_callback(pa_stream *stream, void *userdata)
{
	struct pulseaudio_private *p;

	p = userdata;

	switch (p->pa_stream_get_state(stream))
	{
		case PA_STREAM_READY:
		case PA_STREAM_FAILED:
		case PA_STREAM_TERMINATED:
			p->pa_threaded_mainloop_signal(p->loop, 0);
			break;

		default:
			break;
	}
}


