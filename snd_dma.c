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
// snd_dma.c -- main control for any streaming sound output device

#include "quakedef.h"
#include "sound.h"

#ifdef _WIN32
#include "winquake.h"
#endif

void S_Play_f (void);
void S_PlayVol_f (void);
void S_SoundList_f (void);
void S_Update_ ();
void S_StopAllSounds (qboolean clear);
void S_StopAllSounds_f (void);

// =======================================================================
// Internal sound data & structures
// =======================================================================

channel_t	channels[MAX_CHANNELS];
int			total_channels;

int			snd_blocked = 0;
qboolean	snd_initialized = false;

static qboolean		snd_ambient = 1;

// pointer should go away
volatile dma_t *shm = 0;
volatile dma_t sn;

static vec3_t	listener_origin;
static vec3_t	listener_forward;
static vec3_t	listener_right;
static vec3_t	listener_up;
static vec_t	sound_nominal_clip_dist = 1000.0;

static int	soundtime;		// sample PAIRS
int   		paintedtime; 	// sample PAIRS

#define	MAX_SFX	512
static sfx_t	*known_sfx;		// hunk allocated [MAX_SFX]
static int		num_sfx;

static sfx_t	*ambient_sfx[NUM_AMBIENTS];

static int 		desired_speed = 11025;
static int 		desired_bits = 16;

static int sound_started = 0;

cvar_t bgmvolume = {"bgmvolume", "1", CVAR_ARCHIVE};
cvar_t s_initsound = {"s_initsound", "1"};
cvar_t s_volume = {"volume", "0.7", CVAR_ARCHIVE};
cvar_t s_nosound = {"s_nosound", "0"};
cvar_t s_precache = {"s_precache", "1"};
cvar_t s_loadas8bit = {"s_loadas8bit", "0"};
cvar_t s_khz = {"s_khz", "11"};
cvar_t s_ambientlevel = {"s_ambientlevel", "0.3"};
cvar_t s_ambientfade = {"s_ambientfade", "100"};
cvar_t s_noextraupdate = {"s_noextraupdate", "0"};
cvar_t s_show = {"s_show", "0"};
cvar_t s_mixahead = {"s_mixahead", "0.1", CVAR_ARCHIVE};
cvar_t s_swapstereo = {"s_swapstereo", "0"};

// ====================================================================
// User-setable variables
// ====================================================================

// Fake dma is a synchronous faking of the DMA progress used for
// isolating performance in the renderer.  The fakedma_updates is
// number of times S_Update() is called per second.

qboolean fakedma = false;
int fakedma_updates = 15;

void S_AmbientOff (void) {
	snd_ambient = false;
}

void S_AmbientOn (void) {
	snd_ambient = true;
}

void S_SoundInfo_f (void) {
	if (!sound_started || !shm) {
		Com_Printf ("sound system not started\n");
		return;
	}

    Com_Printf ("%5d stereo\n", shm->channels - 1);
    Com_Printf ("%5d samples\n", shm->samples);
    Com_Printf ("%5d samplepos\n", shm->samplepos);
    Com_Printf ("%5d samplebits\n", shm->samplebits);
    Com_Printf ("%5d submission_chunk\n", shm->submission_chunk);
    Com_Printf ("%5d speed\n", shm->speed);
    Com_Printf ("0x%x dma buffer\n", shm->buffer);
	Com_Printf ("%5d total_channels\n", total_channels);
}

void S_Startup (void) {
	int rc;

	if (!snd_initialized)
		return;

	if (!fakedma) {
		rc = SNDDMA_Init();

		if (!rc) {
#ifndef	_WIN32
			Com_Printf ("S_Startup: SNDDMA_Init failed.\n");
#endif
			sound_started = 0;
			return;
		}
	}

	sound_started = 1;
}

void SND_Restart_f (void) {}

void S_Init (void) {
//	Com_Printf ("\nSound Initialization\n");

	Cvar_SetCurrentGroup(CVAR_GROUP_SOUND);
	Cvar_Register(&bgmvolume);
	Cvar_Register(&s_volume);
	Cvar_Register(&s_initsound);
	Cvar_Register(&s_nosound);
	Cvar_Register(&s_precache);
	Cvar_Register(&s_loadas8bit);
	Cvar_Register(&s_khz);
	Cvar_Register(&s_ambientlevel);
	Cvar_Register(&s_ambientfade);
	Cvar_Register(&s_noextraupdate);
	Cvar_Register(&s_show);
	Cvar_Register(&s_mixahead);
	Cvar_Register(&s_swapstereo);

	Cvar_ResetCurrentGroup();

	// compatibility with old configs
	Cmd_AddLegacyCommand ("volume", "s_volume");
	Cmd_AddLegacyCommand ("nosound", "s_nosound");
	Cmd_AddLegacyCommand ("precache", "s_precache");
	Cmd_AddLegacyCommand ("loadas8bit", "s_loadas8bit");
	Cmd_AddLegacyCommand ("ambient_level", "s_ambientlevel");
	Cmd_AddLegacyCommand ("ambient_fade", "s_ambientfade");
	Cmd_AddLegacyCommand ("snd_noextraupdate", "s_noextraupdate");
	Cmd_AddLegacyCommand ("snd_show", "s_show");
	Cmd_AddLegacyCommand ("_snd_mixahead", "s_mixahead");

	if (COM_CheckParm("-nosound") || !s_initsound.value) {
		Cmd_AddLegacyCommand ("play", "");	// just suppress warnings
		return;
	}

	if (COM_CheckParm("-simsound"))
		fakedma = true;

	Cmd_AddCommand("snd_restart", SND_Restart_f);
	Cmd_AddCommand("play", S_Play_f);
	Cmd_AddCommand("playvol", S_PlayVol_f);
	Cmd_AddCommand("stopsound", S_StopAllSounds_f);
	Cmd_AddCommand("soundlist", S_SoundList_f);
	Cmd_AddCommand("soundinfo", S_SoundInfo_f);

	if (host_memsize < 0x800000) {
		Cvar_Set (&s_loadas8bit, "1");
		Com_Printf ("loading all sounds as 8bit\n");
	}

	snd_initialized = true;

	S_Startup ();

	SND_InitScaletable ();

	known_sfx = Hunk_AllocName (MAX_SFX*sizeof(sfx_t), "sfx_t");
	num_sfx = 0;

	// create a piece of DMA memory

	if (fakedma) {
		shm = (void *) Hunk_AllocName(sizeof(*shm), "shm");
		shm->splitbuffer = 0;
		shm->samplebits = 16;
		shm->speed = 22050;
		shm->channels = 2;
		shm->samples = 32768;
		shm->samplepos = 0;
		shm->soundalive = true;
		shm->gamealive = true;
		shm->submission_chunk = 1;
		shm->buffer = Hunk_AllocName(1<<16, "shmbuf");
	}

	ambient_sfx[AMBIENT_WATER] = S_PrecacheSound ("ambience/water1.wav");
	ambient_sfx[AMBIENT_SKY] = S_PrecacheSound ("ambience/wind2.wav");

	S_StopAllSounds (true);
}

// =======================================================================
// Shutdown sound engine
// =======================================================================

void S_Shutdown (void) {
	if (!sound_started)
		return;

	if (shm)
		shm->gamealive = 0;

	shm = 0;
	sound_started = 0;

	if (!fakedma)
		SNDDMA_Shutdown();
}

// =======================================================================
// Load a sound
// =======================================================================

sfx_t *S_FindName (char *name) {
	int i;
	sfx_t *sfx;

	if (!name)
		Sys_Error ("S_FindName: NULL");

	if (strlen(name) >= MAX_QPATH)
		Sys_Error ("Sound name too long: %s", name);

	// see if already loaded
	for (i = 0; i < num_sfx; i++) {
		if (!strcmp(known_sfx[i].name, name))
			return &known_sfx[i];
	}

	if (num_sfx == MAX_SFX)
		Sys_Error ("S_FindName: out of sfx_t");

	sfx = &known_sfx[i];
	strcpy (sfx->name, name);

	num_sfx++;

	return sfx;
}

void S_TouchSound (char *name) {
	sfx_t *sfx;
	
	if (!sound_started)
		return;

	sfx = S_FindName (name);
	Cache_Check (&sfx->cache);
}

sfx_t *S_PrecacheSound (char *name) {
	sfx_t *sfx;

	if (!sound_started || s_nosound.value)
		return NULL;

	sfx = S_FindName (name);

	// cache it in
	if (s_precache.value)
		S_LoadSound (sfx);

	return sfx;
}

//=============================================================================

channel_t *SND_PickChannel (int entnum, int entchannel) {
    int ch_idx, first_to_die, life_left;

	// Check for replacement sound, or find the best one to replace
    first_to_die = -1;
    life_left = 0x7fffffff;
    for (ch_idx = NUM_AMBIENTS; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; ch_idx++) {
		if (entchannel != 0		// channel 0 never overrides
			&& channels[ch_idx].entnum == entnum
			&& (channels[ch_idx].entchannel == entchannel || entchannel == -1) )
		{	// always override sound from same entity
			first_to_die = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		if (channels[ch_idx].entnum == cl.playernum+1 && entnum != cl.playernum+1 && channels[ch_idx].sfx)
			continue;

		if (channels[ch_idx].end - paintedtime < life_left) {
			life_left = channels[ch_idx].end - paintedtime;
			first_to_die = ch_idx;
		}
   }

	if (first_to_die == -1)
		return NULL;

	if (channels[first_to_die].sfx)
		channels[first_to_die].sfx = NULL;

    return &channels[first_to_die];    
}       

void SND_Spatialize (channel_t *ch) {
    vec_t dot, dist, lscale, rscale, scale;
	vec3_t source_vec;
	sfx_t *snd;

	// anything coming from the view entity will always be full volume
	if (ch->entnum == cl.playernum + 1) {
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
		return;
	}

	// calculate stereo seperation and distance attenuation

	snd = ch->sfx;
	VectorSubtract(ch->origin, listener_origin, source_vec);

	dist = VectorNormalize(source_vec) * ch->dist_mult;
	dot = DotProduct(listener_right, source_vec);

	if (shm->channels == 1) {
		rscale = 1.0;
		lscale = 1.0;
	} else {
		rscale = 1.0 + dot;
		lscale = 1.0 - dot;
	}

	// add in distance effect
	scale = (1.0 - dist) * rscale;
	ch->rightvol = (int) (ch->master_vol * scale);
	if (ch->rightvol < 0)
		ch->rightvol = 0;

	scale = (1.0 - dist) * lscale;
	ch->leftvol = (int) (ch->master_vol * scale);
	if (ch->leftvol < 0)
		ch->leftvol = 0;
}           

// =======================================================================
// Start a sound effect
// =======================================================================

void S_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol, float attenuation) {
	channel_t *target_chan, *check;
	sfxcache_t *sc;
	int vol, ch_idx, skip;

	if (!sound_started)
		return;

	if (!sfx)
		return;

	if (s_nosound.value)
		return;

	vol = fvol * 255;

	// pick a channel to play on
	target_chan = SND_PickChannel(entnum, entchannel);
	if (!target_chan)
		return;
	
	// spatialize
	memset (target_chan, 0, sizeof(*target_chan));
	VectorCopy(origin, target_chan->origin);
	target_chan->dist_mult = attenuation / sound_nominal_clip_dist;
	target_chan->master_vol = vol;
	target_chan->entnum = entnum;
	target_chan->entchannel = entchannel;
	SND_Spatialize(target_chan);

	if (!target_chan->leftvol && !target_chan->rightvol)
		return;		// not audible at all

	// new channel
	sc = S_LoadSound (sfx);
	if (!sc) {
		target_chan->sfx = NULL;
		return;		// couldn't load the sound's data
	}

	target_chan->sfx = sfx;
	target_chan->pos = 0.0;
    target_chan->end = paintedtime + sc->length;	

	// if an identical sound has also been started this frame, offset the pos
	// a bit to keep it from just making the first one louder
	check = &channels[NUM_AMBIENTS];
    for (ch_idx=NUM_AMBIENTS; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; ch_idx++, check++) {
		if (check == target_chan)
			continue;
		if (check->sfx == sfx && !check->pos) {
			skip = rand () % (int)(0.1 * shm->speed);
			if (skip >= target_chan->end)
				skip = target_chan->end - 1;
			target_chan->pos += skip;
			target_chan->end -= skip;
			break;
		}
	}
}

void S_StopSound (int entnum, int entchannel) {
	int i;

	for (i = 0; i < MAX_DYNAMIC_CHANNELS; i++) {
		if (channels[i].entnum == entnum && channels[i].entchannel == entchannel) {
			channels[i].end = 0;
			channels[i].sfx = NULL;
			return;
		}
	}
}

void S_StopAllSounds (qboolean clear) {
	int i;

	if (!sound_started)
		return;

	total_channels = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;	// no statics

	for (i = 0; i < MAX_CHANNELS; i++) {
		if (channels[i].sfx)
			channels[i].sfx = NULL;
	}

	memset(channels, 0, MAX_CHANNELS * sizeof(channel_t));

	if (clear)
		S_ClearBuffer ();
}

void S_StopAllSounds_f (void) {
	S_StopAllSounds (true);
}

void S_ClearBuffer (void) {
	int clear;
		
#ifdef _WIN32
	if (!sound_started || !shm || (!shm->buffer && !pDSBuf))
#else
	if (!sound_started || !shm || !shm->buffer)
#endif
		return;

	clear = (shm->samplebits == 8) ? 0x80 : 0;

#ifdef _WIN32
	if (pDSBuf)
	{
		DWORD dwSize, *pData;
		int reps;
		HRESULT	hresult;

		reps = 0;

		while ((hresult = pDSBuf->lpVtbl->Lock(pDSBuf, 0, gSndBufSize, &pData, &dwSize, NULL, NULL, 0)) != DS_OK) {
			if (hresult != DSERR_BUFFERLOST) {
				Com_Printf ("S_ClearBuffer: DS::Lock Sound Buffer Failed\n");
				S_Shutdown ();
				return;
			}

			if (++reps > 10000) {
				Com_Printf ("S_ClearBuffer: DS: couldn't restore buffer\n");
				S_Shutdown ();
				return;
			}
		}

		memset(pData, clear, shm->samples * shm->samplebits/8);

		pDSBuf->lpVtbl->Unlock(pDSBuf, pData, dwSize, NULL, 0);
	
	}
	else
#endif
	{
		memset(shm->buffer, clear, shm->samples * shm->samplebits/8);
	}
}

void S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation) {
	channel_t *ss;
	sfxcache_t *sc;

	if (!sfx)
		return;

	if (total_channels == MAX_CHANNELS) {
		Com_Printf ("total_channels == MAX_CHANNELS\n");
		return;
	}

	ss = &channels[total_channels];
	total_channels++;

	sc = S_LoadSound (sfx);
	if (!sc)
		return;

	if (sc->loopstart == -1) {
		Com_Printf ("Sound %s not looped\n", sfx->name);
		return;
	}
	
	ss->sfx = sfx;
	VectorCopy (origin, ss->origin);
	ss->master_vol = vol;
	ss->dist_mult = (attenuation/64) / sound_nominal_clip_dist;
    ss->end = paintedtime + sc->length;	
	
	SND_Spatialize (ss);
}

//=============================================================================

void S_UpdateAmbientSounds (void) {
	mleaf_t *l;
	float vol;
	int ambient_channel;
	channel_t *chan;

	if (!snd_ambient)
		return;

	// calc ambient sound levels
	if (!cl.worldmodel)
		return;

	l = Mod_PointInLeaf (listener_origin, cl.worldmodel);
	if (!l || !s_ambientlevel.value) {
		for (ambient_channel = 0 ; ambient_channel< NUM_AMBIENTS ; ambient_channel++)
			channels[ambient_channel].sfx = NULL;
		return;
	}

	for (ambient_channel = 0 ; ambient_channel< NUM_AMBIENTS ; ambient_channel++) {
		chan = &channels[ambient_channel];	
		chan->sfx = ambient_sfx[ambient_channel];
	
		vol = s_ambientlevel.value * l->ambient_sound_level[ambient_channel];
		if (vol < 8)
			vol = 0;

		// don't adjust volume too fast
		if (chan->master_vol < vol) {
			chan->master_vol += cls.frametime * s_ambientfade.value;
			if (chan->master_vol > vol)
				chan->master_vol = vol;
		} else if (chan->master_vol > vol) {
			chan->master_vol -= cls.frametime * s_ambientfade.value;
			if (chan->master_vol < vol)
				chan->master_vol = vol;
		}

		chan->leftvol = chan->rightvol = chan->master_vol;
	}
}

//Called once each time through the main loop
void S_Update (vec3_t origin, vec3_t forward, vec3_t right, vec3_t up) {
	int i, j, total;
	channel_t *ch, *combine;

	if (!sound_started || (snd_blocked > 0))
		return;

	VectorCopy(origin, listener_origin);
	VectorCopy(forward, listener_forward);
	VectorCopy(right, listener_right);
	VectorCopy(up, listener_up);

	// update general area ambient sound sources
	S_UpdateAmbientSounds ();

	combine = NULL;

	// update spatialization for static and dynamic sounds	
	ch = channels+NUM_AMBIENTS;
	for (i = NUM_AMBIENTS; i < total_channels; i++, ch++) {
		if (!ch->sfx)
			continue;
		SND_Spatialize(ch);         // respatialize channel
		if (!ch->leftvol && !ch->rightvol)
			continue;

		// try to combine static sounds with a previous channel of the same
		// sound effect so we don't mix five torches every frame

		if (i >= MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS) {
			// see if it can just use the last one
			if (combine && combine->sfx == ch->sfx) {
				combine->leftvol += ch->leftvol;
				combine->rightvol += ch->rightvol;
				ch->leftvol = ch->rightvol = 0;
				continue;
			}
			// search for one
			combine = channels+MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;
			for (j = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS; j < i; j++, combine++)
				if (combine->sfx == ch->sfx)
					break;

			if (j == total_channels) {
				combine = NULL;
			} else {
				if (combine != ch) {
					combine->leftvol += ch->leftvol;
					combine->rightvol += ch->rightvol;
					ch->leftvol = ch->rightvol = 0;
				}
				continue;
			}
		}
	}

	// debugging output
	if (s_show.value) {
		total = 0;
		ch = channels;
		for (i = 0; i < total_channels; i++, ch++)
			if (ch->sfx && (ch->leftvol || ch->rightvol)) {
				//Com_Printf ("%3i %3i %s\n", ch->leftvol, ch->rightvol, ch->sfx->name);
				total++;
			}

		Com_Printf ("----(%i)----\n", total);
	}

	// mix some sound
	S_Update_();
}

void GetSoundtime (void) {
	int samplepos, fullsamples;
	static int buffers, oldsamplepos;

	fullsamples = shm->samples / shm->channels;

	// it is possible to miscount buffers if it has wrapped twice between calls to S_Update.  Oh well.
	samplepos = SNDDMA_GetDMAPos();

	if (samplepos < oldsamplepos) {
		buffers++;					// buffer wrapped

		if (paintedtime > 0x40000000) {	
			// time to chop things off to avoid 32 bit limits
			buffers = 0;
			paintedtime = fullsamples;
			S_StopAllSounds (true);
		}
	}
	oldsamplepos = samplepos;

	soundtime = buffers*fullsamples + samplepos/shm->channels;
}

void IN_Accumulate (void);

void S_ExtraUpdate (void) {
#ifdef _WIN32
	IN_Accumulate ();
#endif

	if (s_noextraupdate.value)
		return;		// don't pollute timings
	S_Update_();
}

void S_Update_ (void) {
	unsigned endtime;
	int samps;
	
	if (!sound_started || (snd_blocked > 0))
		return;

	// Updates DMA time
	GetSoundtime();

	// check to make sure that we haven't overshot
	if (paintedtime < soundtime) {
		//Com_Printf ("S_Update_ : overflow\n");
		paintedtime = soundtime;
	}

	// mix ahead of current position
	endtime = soundtime + s_mixahead.value * shm->speed;
	samps = shm->samples >> (shm->channels - 1);
	if (endtime - soundtime > samps)
		endtime = soundtime + samps;

#ifdef _WIN32
	// if the buffer was lost or stopped, restore it and/or restart it
	{
		DWORD dwStatus;

		if (pDSBuf) {
			if (pDSBuf->lpVtbl->GetStatus (pDSBuf, &dwStatus) != DS_OK)
				Com_Printf ("Couldn't get sound buffer status\n");
			
			if (dwStatus & DSBSTATUS_BUFFERLOST)
				pDSBuf->lpVtbl->Restore (pDSBuf);
			
			if (!(dwStatus & DSBSTATUS_PLAYING))
				pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);
		}
	}
#endif

	S_PaintChannels (endtime);

	SNDDMA_Submit ();
}

/*
===============================================================================
console functions
===============================================================================
*/

void S_Play_f (void) {
	int i;
	char name[256];
	sfx_t *sfx;
	static int hash = 345;

	for (i = 1; i < Cmd_Argc(); i++) {
		strcpy(name, Cmd_Argv(i));
		COM_DefaultExtension (name, ".wav");
		sfx = S_PrecacheSound(name);
		S_StartSound(hash++, 0, sfx, listener_origin, 1.0, 0.0);
	}
}

void S_PlayVol_f (void) {
	int i;
	float vol;
	char name[256];
	sfx_t *sfx;
	static int hash = 543;

	for (i = 1; i < Cmd_Argc(); i += 2) {
		strcpy(name, Cmd_Argv(i));
		COM_DefaultExtension (name, ".wav");
		sfx = S_PrecacheSound(name);
		vol = Q_atof(Cmd_Argv(i + 1));
		S_StartSound(hash++, 0, sfx, listener_origin, vol, 0.0);
	}
}

void S_SoundList_f (void) {
	int i, size, total;
	sfx_t *sfx;
	sfxcache_t *sc;

	total = 0;
	for (sfx = known_sfx, i = 0; i < num_sfx; i++, sfx++) {
		sc = Cache_Check (&sfx->cache);
		if (!sc)
			continue;
		size = sc->length * sc->width * (sc->stereo + 1);
		total += size;
		if (sc->loopstart >= 0)
			Com_Printf ("L");
		else
			Com_Printf (" ");
		Com_Printf ("(%2db) %6i : %s\n",sc->width*8,  size, sfx->name);
	}
	Com_Printf ("Total resident: %i\n", total);
}

void S_LocalSound (char *sound) {
	sfx_t *sfx;

	if (s_nosound.value)
		return;
	if (!sound_started)
		return;

	sfx = S_PrecacheSound (sound);
	if (!sfx) {
		Com_Printf ("S_LocalSound: can't cache %s\n", sound);
		return;
	}
	S_StartSound (cl.playernum+1, -1, sfx, vec3_origin, 1, 0);
}

void S_ClearPrecache (void) {}

void S_BeginPrecaching (void) {}

void S_EndPrecaching (void) {}
