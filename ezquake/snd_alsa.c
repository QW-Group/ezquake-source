/*
   Support for the ALSA 1.0.1 sound driver

   Copyright (C) 1999,2000  contributors of the QuakeForge project
   Extensively modified for inclusion in ZQuake as alsa_snd_linux.c

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to:

   Free Software Foundation, Inc.
   59 Temple Place - Suite 330
   Boston, MA  02111-1307, USA
*/

#include <stdio.h>
#include <dlfcn.h>
#include <alsa/asoundlib.h>
#include "quakedef.h"
#include "sound.h"

// Define all dynamic ALSA functions...
#define ALSA_FUNC(ret, func, params) \
static ret (*alsa_##func) params;
#include "snd_alsa_funcs.h"
#undef ALSA_FUNC

// Catch the sizeof functions...
#define snd_pcm_hw_params_sizeof alsa_snd_pcm_hw_params_sizeof
#define snd_pcm_sw_params_sizeof alsa_snd_pcm_sw_params_sizeof

// Global Variables
extern int paintedtime, soundtime;
static snd_pcm_uframes_t buffer_size;
static const char  *pcmname = NULL;
static snd_pcm_t   *pcm;

// Prototypes
int SNDDMA_GetDMAPos_ALSA (void);


// Main functions

qbool SNDDMA_Init_ALSA (void)
{
    int                 err;
    int			bps = -1, stereo = -1;
    unsigned int	rate = 0;
    snd_pcm_hw_params_t	*hw;
    snd_pcm_sw_params_t	*sw;
    snd_pcm_uframes_t   frag_size;
    static void         *alsa_handle;

    if(! (alsa_handle = dlopen("libasound.so.2", RTLD_GLOBAL | RTLD_NOW)) )
        return 0;

#define ALSA_FUNC(ret, func, params) \
    if (!(alsa_##func = dlsym (alsa_handle, #func))) \
    { \
        Sys_Printf ("Couldn't load ALSA function %s\n", #func); \
        dlclose (alsa_handle); \
        alsa_handle = 0; \
        return false; \
    }
#include "snd_alsa_funcs.h"
#undef ALSA_FUNC

    // Allocate memory for configuration of ALSA...
    snd_pcm_hw_params_alloca (&hw);
    snd_pcm_sw_params_alloca (&sw);

    // Check for user-specified parameters...
    pcmname = Cvar_VariableString("s_device");

    if(Cvar_VariableValue("s_bits"))
    {
        bps = Cvar_VariableValue("s_bits");
        if(bps != 16 && bps != 8)
        {
            Sys_Printf("Error: invalid sample bits: %d\n", bps);
            return 0;
        }
    }

    if(Cvar_VariableValue("s_rate")) {
        rate = Cvar_VariableValue("s_rate");
        if(rate != 44100 && rate != 22050 && rate != 11025)
        {
            Sys_Printf("Error: invalid sample rate: %d\n", rate);
            return 0;
        }
    }

    stereo = Cvar_VariableValue("s_stereo");

    // Initialise ALSA...
    err = alsa_snd_pcm_open(&pcm, pcmname, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    if(0 > err)
    {
        Sys_Printf("Error: audio open error: %s\n", alsa_snd_strerror(err));
        return 0;
    }
    Sys_Printf("Using PCM %s.\n", pcmname);

    err = alsa_snd_pcm_hw_params_any (pcm, hw);
    if(0 > err)
    {
        Sys_Printf("ALSA: error setting hw_params_any. %s\n", alsa_snd_strerror(err));
        goto error;
    }

    err = alsa_snd_pcm_hw_params_set_access (pcm, hw, SND_PCM_ACCESS_MMAP_INTERLEAVED);
    if(0 > err)
    {
        Sys_Printf("ALSA: Failure to set interleaved PCM access. %s\n", alsa_snd_strerror(err));
        goto error;
    }

    // Work out which sound format to use...
    switch (bps)
    {
        case -1:
            err = alsa_snd_pcm_hw_params_set_format (pcm, hw, SND_PCM_FORMAT_S16); // was _LE
            if(0 <= err) 
            {
                bps = 16;
            }
            else
            {
                if(0 <= (err = alsa_snd_pcm_hw_params_set_format (pcm, hw, SND_PCM_FORMAT_U8)))
                {
                    bps = 8;
                }
                else
                {
                    Sys_Printf("ALSA: no useable formats. %s\n", alsa_snd_strerror(err));
                    goto error;
                }
            }
            break;
        case 8:
        case 16:
            err = alsa_snd_pcm_hw_params_set_format (pcm, hw, bps == 8 ? SND_PCM_FORMAT_U8 : SND_PCM_FORMAT_S16);
            if(0 > err)
            {
                Sys_Printf("ALSA: no usable formats. %s\n", alsa_snd_strerror(err));
                goto error;
            }
            break;
        default:
            Sys_Printf("ALSA: desired format not supported\n");
            goto error;
    }

    // Work out wether to use stereo or not...
    switch (stereo)
    {
        case -1:
            err = alsa_snd_pcm_hw_params_set_channels (pcm, hw, 2);
            if(0 <= err)
            {
                stereo = 1;
            }
            else
            {
                if(0 <= (err = alsa_snd_pcm_hw_params_set_channels (pcm, hw, 1)))
                {
                    stereo = 0;
                }
                else 
                {
                    Sys_Printf("ALSA: no usable channels. %s\n", alsa_snd_strerror(err));
                    goto error;
                }
            }
            break;
        case 0:
        case 1:
            err = alsa_snd_pcm_hw_params_set_channels (pcm, hw, stereo ? 2 : 1);
            if(0 > err) 
            {
                Sys_Printf("ALSA: no usable channels. %s\n", alsa_snd_strerror(err));
                goto error;
            }
            break;
        default:
            Sys_Printf("ALSA: desired channels not supported\n");
            goto error;
    }

    // Sample rate...
    switch (rate)
    {
        case 0:
            rate = 44100;
            err = alsa_snd_pcm_hw_params_set_rate_near (pcm, hw, &rate, 0);
            if(0 <= err)
            {
                frag_size = 32 * bps;
            } 
            else
            {
                rate = 22050;
                err = alsa_snd_pcm_hw_params_set_rate_near (pcm, hw, &rate, 0);
                if(0 <= err)
                {
                    frag_size = 16 * bps;
                }
                else
                {
                    rate = 11025;
                    err = alsa_snd_pcm_hw_params_set_rate_near (pcm, hw, &rate,
                            0);
                    if(0 <= err)
                    {
                        frag_size = 8 * bps;
                    } 
                    else 
                    {
                        Sys_Printf("ALSA: no usable rates. %s\n", alsa_snd_strerror(err));
                        goto error;
                    }
                }
            }
            break;
        case 11025:
        case 22050:
        case 44100:
            err = alsa_snd_pcm_hw_params_set_rate_near (pcm, hw, &rate, 0);
            if(0 > err)
            {
                Sys_Printf("ALSA: desired rate %i not supported. %s\n", rate, alsa_snd_strerror(err));
                goto error;
            }
            frag_size = 8 * bps * rate / 11025;
            break;
        default:
            Sys_Printf("ALSA: desired rate %i not supported.\n", rate);
            goto error;
    }

    err = alsa_snd_pcm_hw_params_set_period_size_near (pcm, hw, &frag_size, 0);
    if(0 > err)
    {
        Sys_Printf("ALSA: unable to set period size near %i. %s\n", (int)frag_size, alsa_snd_strerror(err));
        goto error;
    }
    err = alsa_snd_pcm_hw_params (pcm, hw);
    if(0 > err)
    {
        Sys_Printf("ALSA: unable to install hw params: %s\n", alsa_snd_strerror(err));
        goto error;
    }
    err = alsa_snd_pcm_sw_params_current (pcm, sw);
    if(0 > err)
    {
        Sys_Printf("ALSA: unable to determine current sw params. %s\n", alsa_snd_strerror(err));
        goto error;
    }
    err = alsa_snd_pcm_sw_params_set_start_threshold (pcm, sw, ~0U);
    if(0 > err)
    {
        Sys_Printf("ALSA: unable to set playback threshold. %s\n", alsa_snd_strerror(err));
        goto error;
    }
    err = alsa_snd_pcm_sw_params_set_stop_threshold (pcm, sw, ~0U);
    if(0 > err)
    {
        Sys_Printf("ALSA: unable to set playback stop threshold. %s\n", alsa_snd_strerror(err));
        goto error;
    }
    err = alsa_snd_pcm_sw_params (pcm, sw);
    if(0 > err)
    {
        Sys_Printf("ALSA: unable to install sw params. %s\n", alsa_snd_strerror(err));
        goto error;
    }

    sn.channels = stereo + 1;

    // don't mix less than this in mono samples:
    err = alsa_snd_pcm_hw_params_get_period_size (hw, (snd_pcm_uframes_t *)&sn.submission_chunk, 0);
    if(0 > err)
    {
        Sys_Printf("ALSA: unable to get period size. %s\n", alsa_snd_strerror(err));
        goto error;
    }

    // Tell the Quake sound system what's going on...
    sn.samplepos = 0;
    sn.samplebits = bps;

    err = alsa_snd_pcm_hw_params_get_buffer_size (hw, &buffer_size);
    if(0 > err)
    {
        Sys_Printf("ALSA: unable to get buffer size. %s\n", alsa_snd_strerror(err));
        goto error;
    }

    sn.samples = buffer_size * sn.channels;
    sn.speed = rate;
    SNDDMA_GetDMAPos();

    // Inform user...
    Sys_Printf("%5d stereo\n", sn.channels - 1);
    Sys_Printf("%5d samples\n", sn.samples);
    Sys_Printf("%5d samplepos\n", sn.samplepos);
    Sys_Printf("%5d samplebits\n", sn.samplebits);
    Sys_Printf("%5d submission_chunk\n", sn.submission_chunk);
    Sys_Printf("%5d speed\n", sn.speed);
    Sys_Printf("0x%lx dma buffer\n", (long)sn.buffer);

    return 1;
error:
    alsa_snd_pcm_close (pcm);
    return 0;
}

int SNDDMA_GetDMAPos_ALSA (void)
{
    const snd_pcm_channel_area_t *areas;
    snd_pcm_uframes_t offset;
    snd_pcm_uframes_t nframes = sn.samples/sn.channels;

    alsa_snd_pcm_avail_update (pcm);
    alsa_snd_pcm_mmap_begin (pcm, &areas, &offset, &nframes);
    offset *= sn.channels;
    nframes *= sn.channels;
    sn.samplepos = offset;
    sn.buffer = areas->addr;
    return sn.samplepos;
}

void SNDDMA_Shutdown_ALSA (void)
{
    alsa_snd_pcm_close (pcm);
}

/*
   SNDDMA_Submit

   Send sound to device if buffer isn't really the dma buffer
*/
void SNDDMA_Submit_ALSA (void)
{
    int state;
    int count = paintedtime - soundtime;
    const snd_pcm_channel_area_t *areas;
    snd_pcm_uframes_t nframes;
    snd_pcm_uframes_t offset;

    if(snd_blocked)
        return;

    nframes = count / sn.channels;

    alsa_snd_pcm_avail_update (pcm);
    alsa_snd_pcm_mmap_begin (pcm, &areas, &offset, &nframes);

    state = alsa_snd_pcm_state (pcm);

    switch (state) 
    {
        case SND_PCM_STATE_PREPARED:
            alsa_snd_pcm_mmap_commit (pcm, offset, nframes);
            alsa_snd_pcm_start (pcm);
            break;
        case SND_PCM_STATE_RUNNING:
            alsa_snd_pcm_mmap_commit (pcm, offset, nframes);
            break;
        default:
            break;
    }
}

/*

    These functions are not currently needed by ZQuake,
    but could be of use in the future...

    Remember, they assume that sound is already inited.

static void SNDDMA_BlockSound_ALSA (void)
{
    if(++snd_blocked == 1)
        alsa_snd_pcm_pause (pcm, 1);
}

static void SNDDMA_UnblockSound_ALSA (void)
{
    if(!snd_blocked)
        return;
    if(!--snd_blocked)
        alsa_snd_pcm_pause (pcm, 0);
}*/
