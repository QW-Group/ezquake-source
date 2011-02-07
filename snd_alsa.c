/*
Copyright (C) 2010 Mark Olsen (ported to ezQuake by dimman)

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
#if defined(__linux__) || defined(__FreeBSD__)

#include <alsa/asoundlib.h>
#include <dlfcn.h>

#include "quakedef.h"
#include "qsound.h"


////////////////////////////////////////////////////
// 	defines
///////////////////////////////////////////////////

#define PCM_RECOVER_VERBOSE 0

////////////////////////////////////////////////////
// 	private struct
///////////////////////////////////////////////////

struct alsa_private
{
	void *alsasharedobject;
	void *buffer;
	unsigned int buffersamples;
	unsigned int bufferpos;
	unsigned int samplesize;
	snd_pcm_t *pcmhandle;

	snd_pcm_sframes_t (*snd_pcm_avail_update)(snd_pcm_t *pcm);
        int (*snd_pcm_close)(snd_pcm_t *pcm);
        int (*snd_pcm_open)(snd_pcm_t **pcm, const char *name, snd_pcm_stream_t stream, int mode);
        int (*snd_pcm_recover)(snd_pcm_t *pcm, int err, int silent);
        int (*snd_pcm_set_params)(snd_pcm_t *pcm, snd_pcm_format_t format, snd_pcm_access_t access, unsigned int channels, unsigned int rate, int soft_resample, unsigned int latency);
        int (*snd_pcm_start)(snd_pcm_t *pcm);
	snd_pcm_type_t (*snd_pcm_type)(snd_pcm_t *pcm);
        snd_pcm_sframes_t (*snd_pcm_writei)(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size);
        const char *(*snd_strerror)(int errnum);
};
///////////
static struct alsa_private *driver;


/////////////
//
////////////
int SNDDMA_GetDMAPos_ALSA(void);
void SNDDMA_Shutdown_ALSA(void);
void SNDDMA_Submit_ALSA(unsigned int count);


static qbool alsa_initso(struct alsa_private *p);
static int alsa_getavail(void);
static qbool alsa_init_internal(struct sounddriver_t *sd, const char *device, int rate, int channels, int bits);
static void alsa_tryrestart(unsigned int ret, int dorestart);
static void alsa_writestuff(unsigned int max);

extern cvar_t s_alsa_device;
extern cvar_t s_alsa_latency;
extern cvar_t s_alsa_noworkaround;

////////////////////////////////////////////////////
//	 private functions
///////////////////////////////////////////////////

static void alsa_tryrestart(unsigned int ret, int dorestart)
{
	if (dorestart && ret != -EPIPE)
                dorestart = 0;

        ret = driver->snd_pcm_recover(driver->pcmhandle, ret, !PCM_RECOVER_VERBOSE);
        if (ret < 0)
                Com_Printf("ALSA made a boo-boo: %d (%s)\n", (int)ret, driver->snd_strerror(ret));

        if (dorestart)
                driver->snd_pcm_start(driver->pcmhandle);
}

static qbool alsa_init_internal(struct sounddriver_t *sd, const char *device, int rate, int channels, int bits)
{
	struct alsa_private *drive;
	if(!*device)
		return false; // no device set, like snd_device_alsa
	drive = malloc(sizeof(*drive));
	if(!drive)
		return 0;
	if(alsa_initso(drive)) {
		if (drive->snd_pcm_open(&drive->pcmhandle, device, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK) >= 0) {
			if (drive->snd_pcm_set_params(drive->pcmhandle, bits==8?SND_PCM_FORMAT_S8:SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED, channels, rate, 1, s_alsa_latency.value * 1000000) >= 0) {
				drive->buffer = malloc(65536);
				if (drive->buffer) {
				      	memset(drive->buffer, 0, 65536);
					drive->bufferpos = 0;
                                        drive->samplesize = (bits/8);
                                        drive->buffersamples = 65536/drive->samplesize;
					driver = drive;
					memset((void *) shm, 0, sizeof(*shm));

                                        shm->format.channels = channels;

					shm->format.width = (bits==16) ? 2 : 1;

                                        shm->samples = drive->buffersamples;
                                        shm->samplepos = 0;
					shm->sampleframes = drive->buffersamples/2;
                                        shm->format.speed = rate;
                                        shm->buffer = drive->buffer;

                                        alsa_writestuff(drive->buffersamples);
                                        drive->snd_pcm_start(drive->pcmhandle);

					sd->GetDMAPos = SNDDMA_GetDMAPos_ALSA;
					sd->GetAvail = alsa_getavail;
					sd->Submit = SNDDMA_Submit_ALSA;
					sd->Shutdown = SNDDMA_Shutdown_ALSA;
					sd->name = "snd_alsa";

                                        return true;
                                }
                        }
			drive->snd_pcm_close(drive->pcmhandle);
                }

                dlclose(drive->alsasharedobject);
        }

        free(drive);
        return false;
}
static int alsa_getavail(void)
{
	int ret;
	ret = driver->snd_pcm_avail_update(driver->pcmhandle);
	if(ret < 0) {
		alsa_tryrestart(ret, 1);
		ret = driver->snd_pcm_avail_update(driver->pcmhandle);
		if(ret < 0)
			return 0;
	}
	ret *= 2;
	return ret;
}

static void alsa_writestuff(unsigned int max)
{
        unsigned int count;
        snd_pcm_sframes_t ret;
        int avail;

        while(max)
        {
                count = driver->buffersamples - driver->bufferpos;
                if (count > max)
                        count = max;

                avail = alsa_getavail();

		// Test if we are using ALSA direct or through like pulseaudio ...
		if((driver->snd_pcm_type(driver->pcmhandle)==SND_PCM_TYPE_IOPLUG) && !(s_alsa_noworkaround.value)) {
	                /* This workaround is required to keep sound working on Ubuntu 10.04 and 10.10 (Yay for Ubuntu) */
        	        if (avail < 64)
                	        break;

	                avail -= 64;
		}

                if (count > avail)
                        count = avail;

                if (count == 0)
                        break;

                ret = driver->snd_pcm_writei(driver->pcmhandle, driver->buffer + (driver->samplesize * driver->bufferpos), count/2);

		if (ret < 0)
                {
                        alsa_tryrestart(ret, 1);
                        break;
                }

                if (ret <= 0)
                        break;

                max -= ret * 2;
                driver->bufferpos += ret * 2;
		shm->samplepos += ret * 2; //only to show samplepos info @ soundinfo
                if (driver->bufferpos == driver->buffersamples) {
                        driver->bufferpos = 0;
			shm->samplepos = 0; //only to show samplepos info @ soundinfo
		}
        }
}

static qbool alsa_initso(struct alsa_private *p)
{
        p->alsasharedobject = dlopen("libasound.so.2", RTLD_LAZY|RTLD_LOCAL);
        if (p->alsasharedobject == 0)
        {
                p->alsasharedobject = dlopen("libasound.so", RTLD_LAZY|RTLD_LOCAL);
        }

        if (p->alsasharedobject)
        {
                p->snd_pcm_avail_update = dlsym(p->alsasharedobject, "snd_pcm_avail_update");
                p->snd_pcm_close = dlsym(p->alsasharedobject, "snd_pcm_close");
                p->snd_pcm_open = dlsym(p->alsasharedobject, "snd_pcm_open");
                p->snd_pcm_recover = dlsym(p->alsasharedobject, "snd_pcm_recover");
                p->snd_pcm_set_params = dlsym(p->alsasharedobject, "snd_pcm_set_params");
                p->snd_pcm_start = dlsym(p->alsasharedobject, "snd_pcm_start");
                p->snd_pcm_writei = dlsym(p->alsasharedobject, "snd_pcm_writei");
                p->snd_strerror = dlsym(p->alsasharedobject, "snd_strerror");
		p->snd_pcm_type = dlsym(p->alsasharedobject, "snd_pcm_type");

                if (p->snd_pcm_avail_update
                 && p->snd_pcm_close
                 && p->snd_pcm_open
                 && p->snd_pcm_recover
                 && p->snd_pcm_set_params
                 && p->snd_pcm_writei
                 && p->snd_strerror
		 && p->snd_pcm_type)
                {
                        return 1;
                }

                dlclose(p->alsasharedobject);
        }

        return 0;
}



////////////////////////////////////////////////////
//	 public functions
///////////////////////////////////////////////////

int  SNDDMA_GetDMAPos_ALSA (void)
{
	return driver->bufferpos;
}

void SNDDMA_Submit_ALSA (unsigned int count)
{
	alsa_writestuff(count);
}
void SNDDMA_Shutdown_ALSA (void)
{
	if(driver) {
		driver->snd_pcm_close(driver->pcmhandle);
		dlclose(driver->alsasharedobject);
		free(driver);
	} else {
		Com_Printf("SNDDMA_Shutdown_ALSA: Error, tried to shut down sound but sound not initialized");
	}
}
//void SNDDMA_Init_ALSA (int rate, int channels, int bits)
qbool SNDDMA_Init_ALSA (struct sounddriver_t *sd)
{
	qbool ret;
	const char *prevattempt;
	int channels, bits, rate;
	channels = s_stereo.integer ? 2 : 1;
	bits = 16; // made it default to 16
	if(s_bits.value) {
		bits = s_bits.integer;
		if(bits != 8 && bits != 16) {
			Com_Printf("Error: invalid s_bits value \"%d\". Valid (8 or 16)", bits);
			return false;
		}
	}
	rate = SND_Rate(s_khz.integer);

	ret = alsa_init_internal(sd, s_alsa_device.string, rate, channels, bits);

	prevattempt = s_alsa_device.string;
	if (ret == 0 && strcmp(s_alsa_device.string, "default") != 0)
        {
                Com_Printf("Opening \"%s\" failed, trying \"default\"\n", s_alsa_device.string);

                prevattempt = "default";
                ret = alsa_init_internal(sd, "default", rate, channels, bits);
        }
        if (ret == 0 && strcmp(s_alsa_device.string, "hw") != 0)
        {
                Com_Printf("Opening \"%s\" failed, trying \"hw\"\n", prevattempt);
		prevattempt = "hw";
                ret = alsa_init_internal(sd, "hw", rate, channels, bits);
        }

	if (ret == 0 && strcmp(s_alsa_device.string, "plug:hw") != 0)
        {
                Com_Printf("Opening \"%s\" failed, trying \"plug:hw\"\n", prevattempt);
                prevattempt = "plug:hw";
                ret = alsa_init_internal(sd, "plug:hw", rate, channels, bits);
        }

	return ret;
}

#endif // defined(__linux__) || defined(__FreeBSD__)
