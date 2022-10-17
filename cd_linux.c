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

	$Id: cd_linux.c,v 1.9 2007-10-04 13:48:11 dkure Exp $
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
#ifdef __FreeBSD__
#include <sys/cdio.h>
#else
#include <linux/cdrom.h>
#endif

#include "quakedef.h"
#include "cdaudio.h"
#include "qsound.h"

static qbool cdValid = false;
static qbool	playing = false;
static qbool	wasPlaying = false;
static qbool	initialized = false;
static qbool	enabled = true;
static qbool playLooping = false;
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

#ifdef __FreeBSD__
	if (ioctl(cdfile, CDIOCEJECT) == -1)
		Com_DPrintf ("ioctl cdioceject failed\n");
#else
	if ( ioctl(cdfile, CDROMEJECT) == -1 )
		Com_DPrintf ("ioctl cdromeject failed\n");
#endif
}


static void CDAudio_CloseDoor(void)
{
	if (cdfile == -1 || !enabled)
		return; // no cd init'd

#ifdef __FreeBSD__
	if (ioctl(cdfile, CDIOCCLOSE) == -1)
		Com_DPrintf ("ioctl cdiocclose failed\n");
#else
	if ( ioctl(cdfile, CDROMCLOSETRAY) == -1 )
		Com_DPrintf ("ioctl cdromclosetray failed\n");
#endif
}

static int CDAudio_GetAudioDiskInfo(void)
{
#ifdef __FreeBSD__
	struct ioc_toc_header tochdr;
#else
	struct cdrom_tochdr tochdr;
#endif

	cdValid = false;

#ifdef __FreeBSD__
	if (ioctl(cdfile, CDIOREADTOCHEADER, &tochdr) == -1)
	{
		Com_DPrintf ("ioctl cdioreadtocheader failed\n");
#else
	if ( ioctl(cdfile, CDROMREADTOCHDR, &tochdr) == -1 )
    {
      Com_DPrintf ("ioctl cdromreadtochdr failed\n");
#endif
	  return -1;
    }

#ifdef __FreeBSD__
	if (tochdr.starting_track < 1)
#else
	if (tochdr.cdth_trk0 < 1)
#endif
	{
		Com_DPrintf ("CDAudio: no music tracks\n");
		return -1;
	}

	cdValid = true;
#ifdef __FreeBSD__
	maxTrack = tochdr.ending_track;
#else
	maxTrack = tochdr.cdth_trk1;
#endif

	return 0;
}


void CDAudio_Play(byte track, qbool looping)
{
#ifdef __FreeBSD__
	struct ioc_read_toc_entry entry;
	struct cd_toc_entry toc_buffer;
	struct ioc_play_track ti;
#else
	struct cdrom_tocentry entry;
	struct cdrom_ti ti;
#endif

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

#ifdef __FreeBSD__
	#define CDROM_DATA_TRACK 4
	bzero((char *)&toc_buffer, sizeof(toc_buffer));
	entry.data_len = sizeof(toc_buffer);
	entry.data = &toc_buffer;
	// don't try to play a non-audio track
	entry.starting_track = track;
	entry.address_format = CD_MSF_FORMAT;
    if ( ioctl(cdfile, CDIOREADTOCENTRYS, &entry) == -1 )
	{
		Com_DPrintf("ioctl cdromreadtocentry failed\n");
		return;
	}
	if (toc_buffer.control == CDROM_DATA_TRACK)
#else
	// don't try to play a non-audio track
	entry.cdte_track = track;
	entry.cdte_format = CDROM_MSF;
    if ( ioctl(cdfile, CDROMREADTOCENTRY, &entry) == -1 )
	{
		Com_DPrintf ("ioctl cdromreadtocentry failed\n");
		return;
	}
	if (entry.cdte_ctrl == CDROM_DATA_TRACK)
#endif
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

#ifdef __FreeBSD__
	ti.start_track = track;
	ti.end_track = track;
	ti.start_index = 1;
	ti.end_index = 99;
#else
	ti.cdti_trk0 = track;
	ti.cdti_trk1 = track;
	ti.cdti_ind0 = 1;
	ti.cdti_ind1 = 99;
#endif

#ifdef __FreeBSD__
	if (ioctl(cdfile, CDIOCPLAYTRACKS, &ti) == -1)
	{
		Com_DPrintf ("ioctl cdiocplaytracks failed\n");
#else
	if ( ioctl(cdfile, CDROMPLAYTRKIND, &ti) == -1 )
    {
		Com_DPrintf ("ioctl cdromplaytrkind failed\n");
#endif
		return;
    }

#ifdef __FreeBSD__
	if (ioctl(cdfile, CDIOCRESUME) == -1)
		Com_DPrintf ("ioctl cdiocresume failed\n");
#else
	if ( ioctl(cdfile, CDROMRESUME) == -1 )
		Com_DPrintf ("ioctl cdromresume failed\n");
#endif

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

#ifdef __FreeBSD__
	if (ioctl(cdfile, CDIOCSTOP) == -1)
		Com_DPrintf ("ioctl cdiocstop failed (%d)\n", errno);
#else
	if ( ioctl(cdfile, CDROMSTOP) == -1 )
		Com_DPrintf ("ioctl cdromstop failed (%d)\n", errno);
#endif

	wasPlaying = false;
	playing = false;
}

void CDAudio_Pause(void)
{
	if (cdfile == -1 || !enabled)
		return;

	if (!playing)
		return;

#ifdef __FreeBSD__
	if (ioctl(cdfile, CDIOCPAUSE) == -1)
		Com_DPrintf ("ioctl cdiocpause failed\n");
#else
	if ( ioctl(cdfile, CDROMPAUSE) == -1 )
		Com_DPrintf ("ioctl cdrompause failed\n");
#endif

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

#ifdef __FreeBSD__
	if (ioctl(cdfile, CDIOCRESUME) == -1)
		Com_DPrintf ("ioctl cdiocresume failed\n");
#else
	if ( ioctl(cdfile, CDROMRESUME) == -1 )
		Com_DPrintf ("ioctl cdromresume failed\n");
#endif
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

	if (!strcasecmp(command, "on")) {
		CHECK_CD_ARGS(2);
		enabled = true;
		return;
	} else if (!strcasecmp(command, "off")) {
		CHECK_CD_ARGS(2);
		CDAudio_Stop();
		enabled = false;
		return;
	} else if (!strcasecmp(command, "reset")) {
		CHECK_CD_ARGS(2);
		enabled = true;
		CDAudio_Stop();
		for (n = 0; n < 100; n++)
			remap[n] = n;
		CDAudio_GetAudioDiskInfo();
		return;
	} else if (!strcasecmp(command, "remap")) {
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
	} else if (!strcasecmp(command, "close")) {
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

	if (!strcasecmp(command, "play"))	{
		CHECK_CD_ARGS(3);
		CDAudio_Play((byte) Q_atoi(Cmd_Argv(2)), false);
	} else if (!strcasecmp(command, "loop"))	{
		CHECK_CD_ARGS(3);
		CDAudio_Play((byte) Q_atoi(Cmd_Argv(2)), true);
	} else if (!strcasecmp(command, "stop"))	{
		CHECK_CD_ARGS(2);
		CDAudio_Stop();
	} else if (!strcasecmp(command, "pause")) {
		CHECK_CD_ARGS(2);
		CDAudio_Pause();
	} else if (!strcasecmp(command, "resume")) {
		CHECK_CD_ARGS(2);
		CDAudio_Resume();
	} else if (!strcasecmp(command, "eject")) {
		CHECK_CD_ARGS(2);
		CDAudio_Stop();
		CDAudio_Eject();
		cdValid = false;
	} else if (!strcasecmp(command, "info"))	{
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
#ifdef __FreeBSD__
	struct ioc_read_subchannel subchnl;
	struct cd_sub_channel_info data;
#else
	struct cdrom_subchnl subchnl;
#endif
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
#if defined(__FreeBSD__)
		subchnl.address_format = CD_MSF_FORMAT;
		subchnl.data_format = CD_CURRENT_POSITION;
		subchnl.data_len = sizeof(data);
		subchnl.track = playTrack;
		subchnl.data = &data;
		if (ioctl(cdfile, CDIOCREADSUBCHANNEL, &subchnl) == -1 ) {
			Com_DPrintf("ioctl cdiocreadsubchannel failed\n");
			playing = false;
			return;
		}
		if (subchnl.data->header.audio_status != CD_AS_PLAY_IN_PROGRESS &&
			subchnl.data->header.audio_status != CD_AS_PLAY_PAUSED) {
			playing = false;
			if (playLooping)
				CDAudio_Play(playTrack, true);
		}
#else
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
#endif
	}
}

int CDAudio_Init(void)
{
	int i;

	if (!COM_CheckParm(cmdline_param_client_cd_audio))
		return -1;

	if ((i = COM_CheckParm(cmdline_param_client_cd_device)) != 0 && i < COM_Argc() - 1)
		strlcpy (cd_dev, COM_Argv(i + 1), sizeof(cd_dev));

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
