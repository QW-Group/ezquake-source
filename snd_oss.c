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

    $Id: snd_oss.c,v 1.24 2006/04/28 23:38:29 disconn3ct Exp $
*/

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/wait.h>
#ifdef __linux__
#include <linux/soundcard.h>
#else
#include <sys/soundcard.h>
#endif
#include <stdio.h>

#include "quakedef.h"
#include "qsound.h"

// Global Variables
static int audio_fd;

// Main functions
qbool SNDDMA_Init_OSS(void)
{
	int rc, fmt, tmp, caps;
	char *snd_dev = NULL;
	struct audio_buf_info info;

	snd_dev = Cvar_String("s_device");

	if ((audio_fd = open(snd_dev, O_RDWR | O_NONBLOCK)) < 0) {
		perror(snd_dev);
		Com_Printf("Could not open %s\n", snd_dev);
		return 0;
	}

	if ((rc = ioctl(audio_fd, SNDCTL_DSP_RESET, 0)) < 0) {
		perror(snd_dev);
		Com_Printf("Could not reset %s\n", snd_dev);
		close(audio_fd);
		return 0;
	}

	if (ioctl(audio_fd, SNDCTL_DSP_GETCAPS, &caps) == -1) {
		perror(snd_dev);
		Com_Printf("Sound driver too old\n");
		close(audio_fd);
		return 0;
	}

	if (!(caps & DSP_CAP_TRIGGER) || !(caps & DSP_CAP_MMAP)) {
		Com_Printf("Sorry but your soundcard can't do this\n");
		close(audio_fd);
		return 0;
	}

	// set sample bits & speed
	shm->format.width  = (int) (s_bits.value / 8);
	shm->format.speed = SND_Rate((int)s_khz.value);
	shm->format.channels = ((int) s_stereo.value == 0) ? 1 : 2;

	if (shm->format.width != 2 && shm->format.width != 1) {
		ioctl(audio_fd, SNDCTL_DSP_GETFMTS, &fmt);
		if (fmt & AFMT_S16_LE) // disconnect: FIXME: what if bigendian?
			shm->format.width = 2;
		else if (fmt & AFMT_U8)
			shm->format.width = 1;
	}


	tmp = (shm->format.channels == 2);
	if ((rc = ioctl(audio_fd, SNDCTL_DSP_STEREO, &tmp)) < 0) {
		perror(snd_dev);
		Com_Printf ("Could not set %s to stereo", snd_dev);
		close(audio_fd);
		return 0;
	}

	if ((rc = ioctl(audio_fd, SNDCTL_DSP_SPEED, &shm->format.speed)) < 0) {
		perror(snd_dev);
		Com_Printf("Could not set %s speed to %d", snd_dev, shm->format.speed);
		close(audio_fd);
		return 0;
	}

	if (shm->format.width == 2) {
		rc = AFMT_S16_LE;
		rc = ioctl(audio_fd, SNDCTL_DSP_SETFMT, &rc);
		if (rc < 0) {
			perror(snd_dev);
			Com_Printf("Could not support 16-bit data.  Try 8-bit.\n");
			close(audio_fd);
			return 0;
		}
	} else if (shm->format.width == 1) {
		rc = AFMT_U8;
		rc = ioctl(audio_fd, SNDCTL_DSP_SETFMT, &rc);
		if (rc < 0) {
			perror(snd_dev);
			Com_Printf("Could not support 8-bit data.\n");
			close(audio_fd);
			return 0;
		}
	} else {
		perror(snd_dev);
		Com_Printf("%d-bit sound not supported.", shm->format.width * 8);
 		close(audio_fd);
 		return 0;
 	}

	if (ioctl(audio_fd, SNDCTL_DSP_GETOSPACE, &info) == -1) {
		perror("GETOSPACE");
		Com_Printf ("Um, can't do GETOSPACE?\n");
		close(audio_fd);
		return 0;
     }

	shm->sampleframes = info.fragstotal * info.fragsize / shm->format.width / shm->format.channels;
	shm->samples = shm->sampleframes * shm->format.channels;

	// memory map the dma buffer
	shm->bufferlength = info.fragstotal * info.fragsize;
	shm->buffer = (unsigned char *) mmap(NULL, shm->bufferlength, PROT_WRITE, MAP_FILE|MAP_SHARED, audio_fd, 0);
	if (!shm->buffer || shm->buffer == (unsigned char *)-1) {
		perror(snd_dev);
		Com_Printf ("Could not mmap %s\n", snd_dev);
		close(audio_fd);
		return 0;
	}

	// toggle the trigger & start her up
	tmp = 0;
	rc  = ioctl(audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp);
	if (rc < 0) {
		perror(snd_dev);
		Com_Printf("Could not toggle.\n");
		close(audio_fd);
		return 0;
	}

	tmp = PCM_ENABLE_OUTPUT;
	rc = ioctl(audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp);
	if (rc < 0) {
		perror(snd_dev);
		Com_Printf("Could not toggle.\n");
		close(audio_fd);
		return 0;
	}

	shm->samplepos = 0;

	return 1;

}

int SNDDMA_GetDMAPos_OSS(void)
{
	struct count_info count;
	char *snd_dev = NULL;

	if (!shm)
		return 0;

	snd_dev = Cvar_String("s_device");

	if (ioctl(audio_fd, SNDCTL_DSP_GETOPTR, &count) == -1) {
		perror(snd_dev);
		Com_Printf("Uh, sound dead.\n");
		close(audio_fd);
		return 0;
	}

	shm->samplepos = count.ptr / shm->format.width;

	return shm->samplepos;

}

void SNDDMA_Shutdown_OSS(void)
{
	int tmp = 0;

	// unmap the memory
	if (shm->buffer)
		munmap(shm->buffer, shm->bufferlength);

	if (audio_fd) {
		// stop the sound
		ioctl(audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp);
		ioctl(audio_fd, SNDCTL_DSP_RESET, 0);

		// close the device
		close(audio_fd);
	}

	audio_fd = -1;
}
