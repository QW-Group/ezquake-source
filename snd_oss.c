/*
Copyright (C) 2006-2007 Mark Olsen (ported to ezQuake by dimman)

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

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "quakedef.h"
#include "qsound.h"

extern cvar_t s_oss_device;

struct oss_private
{
	int fd;
};

static struct oss_private *pdriver;

static int oss_getdmapos(void)
{
	struct count_info count;

	if (ioctl(pdriver->fd, SNDCTL_DSP_GETOPTR, &count) != -1)
		shm->samplepos = count.ptr/(shm->format.width);
	else
		shm->samplepos = 0;

	return shm->samplepos;
}

static void oss_shutdown(void)
{

	munmap(shm->buffer, shm->samples*(shm->format.width));
	close(pdriver->fd);

	free(pdriver);
}

static qbool oss_init_internal(qsoundhandler_t *sd, const char *device, int rate, int channels, int bits)
{
	int i;

	int capabilities;
	int dspformats;
	struct audio_buf_info info;
	struct oss_private *p;

	p = malloc(sizeof(*p));
	if (p)
	{
		p->fd = open(device, O_RDWR|O_NONBLOCK);
		if (p->fd != -1)
		{
			if ((ioctl(p->fd, SNDCTL_DSP_RESET, 0) >= 0)
			&& (ioctl(p->fd, SNDCTL_DSP_GETCAPS, &capabilities) != -1 && (capabilities&(DSP_CAP_TRIGGER|DSP_CAP_MMAP)) == (DSP_CAP_TRIGGER|DSP_CAP_MMAP))
			&& (ioctl(p->fd, SNDCTL_DSP_GETOSPACE, &info) != -1))
			{
				if ((shm->buffer = mmap(0, info.fragstotal * info.fragsize, PROT_WRITE, MAP_SHARED, p->fd, 0)) != MAP_FAILED)
				{
					ioctl(p->fd, SNDCTL_DSP_GETFMTS, &dspformats);

					if ((!(dspformats&AFMT_S16_LE) && (dspformats&AFMT_U8)) || bits == 8)
						shm->format.width = 1;
					else if ((dspformats&AFMT_S16_LE))
						shm->format.width = 2;
					else
						shm->format.width = 0;

					if (shm->format.width)
					{
						i = 0;
						if (ioctl(p->fd, SNDCTL_DSP_SPEED, &rate) >= 0)
						{
							if (channels == 1)
							{
								shm->format.channels = 1;
								i = 0;
							}
							else
							{
								shm->format.channels = 2;
								i = 1;
							}

							if (ioctl(p->fd, SNDCTL_DSP_STEREO, &i) >= 0)
							{
								if (shm->format.width == 2)
									i = AFMT_S16_LE;
								else
									i = AFMT_S8;

								if (ioctl(p->fd, SNDCTL_DSP_SETFMT, &i) >= 0)
								{
									i = 0;
									if (ioctl(p->fd, SNDCTL_DSP_SETTRIGGER, &i) >= 0)
									{
										i = PCM_ENABLE_OUTPUT;
										if (ioctl(p->fd, SNDCTL_DSP_SETTRIGGER, &i) >= 0)
										{
											shm->samples = info.fragstotal * info.fragsize / (shm->format.width);
											shm->samplepos = 0;
											shm->format.speed = rate;

											pdriver = p;

											sd->GetDMAPos = oss_getdmapos;
											sd->GetAvail = NULL;
											sd->Submit = NULL;
											sd->Shutdown = oss_shutdown;
											sd->name = "snd_oss";

											return 1;
										}
									}
								}
							}
						}
					}
					munmap(shm->buffer, info.fragstotal * info.fragsize);
				}
			}
			close(p->fd);
		}
		free(p);
	}
	return 0;
}

static qbool oss_init(qsoundhandler_t *sd, int rate, int channels, int bits)
{
	qbool ret;

	ret = oss_init_internal(sd, s_oss_device.string, rate, channels, bits);
	if (ret == 0 && strcmp(s_oss_device.string, "/dev/dsp") != 0)
	{
		Com_Printf("Opening \"%s\" failed, trying \"/dev/dsp\"\n", s_oss_device.string);

		ret = oss_init_internal(sd, "/dev/dsp", rate, channels, bits);
	}

	return ret;
}

qbool SNDDMA_Init_OSS(qsoundhandler_t *sd) {
	int rate, channels, bits;
	channels = s_stereo.integer ? 2 : 1;
	bits = 16;
	if(s_bits.value) {
                bits = s_bits.integer;
                if(bits != 8 && bits != 16) {
                        Com_Printf("Error: invalid s_bits value \"%d\". Valid (8 or 16)", bits);
                        return false;
                }
        }
        rate = SND_Rate(s_khz.integer);

	return oss_init(sd, rate, channels, bits);
}
