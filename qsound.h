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
// qsound.h -- client sound i/o functions

#ifndef __QSOUND_H__
#define __QSOUND_H__

#include "q_shared.h"
#include "zone.h"
#include "cvar.h"

typedef struct sfx_s {
	char  name[MAX_QPATH];
	void *buf;
} sfx_t;

// FIXME: REMOVE ME PLZ
typedef struct snd_format_s {
	unsigned int speed;
	unsigned int width;
	unsigned int channels;
} snd_format_t;

typedef struct sfxcache_s {
	snd_format_t	format;
	unsigned int 	total_length;
	int 		loopstart;
	unsigned char	data[1];
} sfxcache_t;

typedef struct soundhw_s {
	unsigned int  numchannels;
	unsigned int  samples;
	unsigned int  samplepos;
	unsigned int  samplebits;
	unsigned int  khz;
	unsigned long snd_sent;
	unsigned char *buffer;
	/* qw related */
	int paintedtime;
	int oldsamplepos;
	int numwraps;
	int queuelen;
} soundhw_t;

extern soundhw_t *shw;

typedef struct channel_s {
	sfx_t		*sfx;			// sfx number
	int		leftvol;		// 0-255 volume
	int		rightvol;		// 0-255 volume
	int		end;			// end time in global paintsamples
	int 		pos;			// sample position in sfx
	int		looping;		// where to loop, -1 = no looping
	int		entnum;			// to allow overriding a specific sound
	int		entchannel;		//
	vec3_t		origin;			// origin of sound effect
	vec_t		dist_mult;		// distance multiplier (attenuation/clipK)
	int		master_vol;		// 0-255 master volume
} channel_t;

typedef struct wavinfo_s {
	int		rate;
	int		width;
	int		channels;
	int		loopstart;
	int		samples;
	int		dataofs;		// chunk starts this many bytes from file start
} wavinfo_t;

void S_Init (void);
void S_Shutdown (void);
void S_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol,  float attenuation);
void S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation);
void S_StopSound (int entnum, int entchannel);
void S_StopAllSounds(void);
void S_Update (vec3_t origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up);

sfx_t *S_PrecacheSound (char *sample);
void S_PaintChannels(int endtime);

void S_LocalSound (char *s);
void S_LocalSoundWithVol(char *sound, float volume);
sfxcache_t *S_LoadSound (sfx_t *s);

void SND_InitScaletable (void);
int SND_Rate(int rate);

void SND_ResampleStream(void *in, int inrate, int inwidth, int inchannels, int insamps,
						void *out, int outrate, int outwidth, int outchannels, int resampstyle);

// ====================================================================
// User-setable variables
// ====================================================================

#define MAX_CHANNELS 512
#define MAX_DYNAMIC_CHANNELS 32


extern channel_t	channels[MAX_CHANNELS];
// 0 to MAX_DYNAMIC_CHANNELS - 1 = normal entity sounds
// MAX_DYNAMIC_CHANNELS to MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS - 1 = water, etc
// MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS to total_channels = static sounds

extern unsigned int	total_channels;

extern qbool		snd_initialized;
extern qbool		snd_started;

extern int		soundtime;

extern cvar_t		s_loadas8bit;
extern cvar_t		s_khz;
extern cvar_t		s_volume;
extern cvar_t		s_swapstereo;
extern cvar_t		bgmvolume;

#endif
