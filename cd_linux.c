/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// cd_linux.c

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <linux/cdrom.h>

#include "quakedef.h"
#include "cdaudio.h"
#include "sound.h"

static qboolean cdValid = false;
static qboolean	playing = false;
static qboolean	wasPlaying = false;
static qboolean	initialized = false;
static qboolean	enabled = true;
static qboolean playLooping = false;
static float	cdvolume;
static byte 	remap[100];
static byte		playTrack;
static byte		maxTrack;

static int cdfile = -1;
static char cd_dev[64] = "/dev/cdrom";

static void CDAudio_Eject(void)
{
	if (cdfile == -1 || !enabled)
		return; // no cd init'd

	if ( ioctl(cdfile, CDROMEJECT) == -1 ) 
		Com_DPrintf ("ioctl cdromeject failed\n");
}


static void CDAudio_CloseDoor(void)
{
	if (cdfile == -1 || !enabled)
		return; // no cd init'd

	if ( ioctl(cdfile, CDROMCLOSETRAY) == -1 ) 
		Com_DPrintf ("ioctl cdromclosetray failed\n");
}

static int CDAudio_GetAudioDiskInfo(void)
{
	struct cdrom_tochdr tochdr;

	cdValid = false;

	if ( ioctl(cdfile, CDROMREADTOCHDR, &tochdr) == -1 ) 
    {
      Com_DPrintf ("ioctl cdromreadtochdr failed\n");
	  return -1;
    }

	if (tochdr.cdth_trk0 < 1)
	{
		Com_DPrintf ("CDAudio: no music tracks\n");
		return -1;
	}

	cdValid = true;
	maxTrack = tochdr.cdth_trk1;

	return 0;
}


void CDAudio_Play(byte track, qboolean looping)
{
	struct cdrom_tocentry entry;
	struct cdrom_ti ti;

	if (cdfile == -1 || !enabled)
		return;
	
	if (!cdValid)
	{
		CDAudio_GetAudioDiskInfo();
		if (!cdValid)
			return;
	}

	track = remap[track];

	if (track < 1 || track > maxTrack)
	{
		Com_DPrintf ("CDAudio: Bad track number %u.\n", track);
		return;
	}

	// don't try to play a non-audio track
	entry.cdte_track = track;
	entry.cdte_format = CDROM_MSF;
    if ( ioctl(cdfile, CDROMREADTOCENTRY, &entry) == -1 )
	{
		Com_DPrintf ("ioctl cdromreadtocentry failed\n");
		return;
	}
	if (entry.cdte_ctrl == CDROM_DATA_TRACK)
	{
		Com_Printf ("CDAudio: track %i is not audio\n", track);
		return;
	}

	if (playing)
	{
		if (playTrack == track)
			return;
		CDAudio_Stop();
	}

	ti.cdti_trk0 = track;
	ti.cdti_trk1 = track;
	ti.cdti_ind0 = 1;
	ti.cdti_ind1 = 99;

	if ( ioctl(cdfile, CDROMPLAYTRKIND, &ti) == -1 ) 
    {
		Com_DPrintf ("ioctl cdromplaytrkind failed\n");
		return;
    }

	if ( ioctl(cdfile, CDROMRESUME) == -1 ) 
		Com_DPrintf ("ioctl cdromresume failed\n");

	playLooping = looping;
	playTrack = track;
	playing = true;

	if (cdvolume == 0.0)
		CDAudio_Pause ();
}


void CDAudio_Stop(void)
{
	if (cdfile == -1 || !enabled)
		return;
	
	if (!playing)
		return;

	if ( ioctl(cdfile, CDROMSTOP) == -1 )
		Com_DPrintf ("ioctl cdromstop failed (%d)\n", errno);

	wasPlaying = false;
	playing = false;
}

void CDAudio_Pause(void)
{
	if (cdfile == -1 || !enabled)
		return;

	if (!playing)
		return;

	if ( ioctl(cdfile, CDROMPAUSE) == -1 ) 
		Com_DPrintf ("ioctl cdrompause failed\n");

	wasPlaying = playing;
	playing = false;
}


void CDAudio_Resume(void)
{
	if (cdfile == -1 || !enabled)
		return;
	
	if (!cdValid)
		return;

	if (!wasPlaying)
		return;
	
	if ( ioctl(cdfile, CDROMRESUME) == -1 ) 
		Com_DPrintf ("ioctl cdromresume failed\n");
	playing = true;
}

#define CHECK_CD_ARGS(x)		\
	if (Cmd_Argc() != x) {		\
	Com_Printf("Usage: %s <on|off|reset|remap|close|play|loop|stop|pause|resume|eject|info>\n", Cmd_Argv(0));	\
	return;					\
	}

void CD_f (void) {
	char *command;
	int ret, n;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <on|off|reset|remap|close|play|loop|stop|pause|resume|eject|info>\n", Cmd_Argv(0));
		return;
	}

	command = Cmd_Argv (1);

	if (!Q_strcasecmp(command, "on")) {
		CHECK_CD_ARGS(2);
		enabled = true;
		return;
	} else if (!Q_strcasecmp(command, "off")) {
		CHECK_CD_ARGS(2);
		CDAudio_Stop();
		enabled = false;
		return;
	} else if (!Q_strcasecmp(command, "reset")) {
		CHECK_CD_ARGS(2);
		enabled = true;
		CDAudio_Stop();
		for (n = 0; n < 100; n++)
			remap[n] = n;
		CDAudio_GetAudioDiskInfo();
		return;
	} else if (!Q_strcasecmp(command, "remap")) {
		ret = Cmd_Argc() - 2;
		if (!ret) {
			for (n = 1; n < 100; n++)
				if (remap[n] != n)
					Com_Printf ("  %u -> %u\n", n, remap[n]);
		} else {
			for (n = 1; n <= ret; n++)
				remap[n] = Q_atoi(Cmd_Argv (n + 1));
		}
		return;
	} else if (!Q_strcasecmp(command, "close")) {
		CHECK_CD_ARGS(2);
		CDAudio_CloseDoor();
		return;
	}

	if (!cdValid) {
		CDAudio_GetAudioDiskInfo();
		if (!cdValid) {
			Com_Printf ("No CD in player.\n");
			return;
		}
	}

	if (!Q_strcasecmp(command, "play"))	{
		CHECK_CD_ARGS(3);
		CDAudio_Play((byte) Q_atoi(Cmd_Argv(2)), false);
	} else if (!Q_strcasecmp(command, "loop"))	{
		CHECK_CD_ARGS(3);
		CDAudio_Play((byte) Q_atoi(Cmd_Argv(2)), true);
	} else if (!Q_strcasecmp(command, "stop"))	{
		CHECK_CD_ARGS(2);
		CDAudio_Stop();
	} else if (!Q_strcasecmp(command, "pause")) {
		CHECK_CD_ARGS(2);
		CDAudio_Pause();
	} else if (!Q_strcasecmp(command, "resume")) {
		CHECK_CD_ARGS(2);
		CDAudio_Resume();
	} else if (!Q_strcasecmp(command, "eject")) {
		CHECK_CD_ARGS(2);
		CDAudio_Stop();
		CDAudio_Eject();
		cdValid = false;
	} else if (!Q_strcasecmp(command, "info"))	{
		CHECK_CD_ARGS(2);
		Com_Printf ("%u tracks\n", maxTrack);
		if (playing)
			Com_Printf ("Currently %s track %u\n", playLooping ? "looping" : "playing", playTrack);
		else if (wasPlaying)
			Com_Printf ("Paused %s track %u\n", playLooping ? "looping" : "playing", playTrack);
		Com_Printf ("Volume is %f\n", cdvolume);
	}
}

void CDAudio_Update(void)
{
	struct cdrom_subchnl subchnl;
	static time_t lastchk;

	if (!enabled)
		return;

	if (bgmvolume.value != cdvolume)
	{
		if (cdvolume)
		{
			Cvar_SetValue (&bgmvolume, 0.0);
			cdvolume = bgmvolume.value;
			CDAudio_Pause ();
		}
		else
		{
			Cvar_SetValue (&bgmvolume, 1.0);
			cdvolume = bgmvolume.value;
			CDAudio_Resume ();
		}
	}

	if (playing && lastchk < time(NULL)) {
		lastchk = time(NULL) + 2; //two seconds between chks
		subchnl.cdsc_format = CDROM_MSF;
		if (ioctl(cdfile, CDROMSUBCHNL, &subchnl) == -1 ) {
			Com_DPrintf ("ioctl cdromsubchnl failed\n");
			playing = false;
			return;
		}
		if (subchnl.cdsc_audiostatus != CDROM_AUDIO_PLAY &&
			subchnl.cdsc_audiostatus != CDROM_AUDIO_PAUSED) {
			playing = false;
			if (playLooping)
				CDAudio_Play(playTrack, true);
		}
	}
}

int CDAudio_Init(void)
{
	int i;

	if (!COM_CheckParm("-cdaudio"))
		return -1;

	if ((i = COM_CheckParm("-cddev")) != 0 && i < com_argc - 1)
		Q_strncpyz (cd_dev, com_argv[i + 1], sizeof(cd_dev));

	if ((cdfile = open(cd_dev, O_RDONLY)) == -1) {
		Com_Printf ("CDAudio_Init: open of \"%s\" failed (%i)\n", cd_dev, errno);
		cdfile = -1;
		return -1;
	}

	for (i = 0; i < 100; i++)
		remap[i] = i;
	initialized = true;
	enabled = true;

	if (CDAudio_GetAudioDiskInfo())
	{
		Com_Printf ("CDAudio_Init: No CD in player.\n");
		cdValid = false;
	}

	Cmd_AddCommand ("cd", CD_f);

	Com_Printf ("CD Audio Initialized\n");

	return 0;
}


void CDAudio_Shutdown(void)
{
	if (!initialized)
		return;
	CDAudio_Stop();
	close(cdfile);
	cdfile = -1;
}
