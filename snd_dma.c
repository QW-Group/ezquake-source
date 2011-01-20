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

    $Id: snd_dma.c,v 1.48 2007-09-26 21:51:34 tonik Exp $
*/
// snd_dma.c -- main control for any streaming sound output device

#include "quakedef.h"
#include "qsound.h"
#define SELF_SOUND 0xFFEFFFFF // [EZH] Fan told me 0xFFEFFFFF is damn cool value for it :P

#ifdef _WIN32
#include "winquake.h"
#include "movie.h" //joe: capturing audio
#endif

#ifdef FTE_PEXT2_VOICECHAT
#include "input.h"
#endif

//dimman
#ifdef __linux__
sounddriver_t *sounddriver;
#endif

static void OnChange_s_khz (cvar_t *var, char *string, qbool *cancel);
static void S_Play_f (void);
static void S_PlayVol_f (void);
static void S_SoundList_f (void);
static void S_Update_ ();
static void S_StopAllSounds_f (void);

void IN_Accumulate (void); // S_ExtraUpdate use it
void S_RawClear(void);
void S_RawAudio(int sourceid, byte *data, 
				unsigned int speed, unsigned int samples, unsigned int channelsnum, unsigned int width);

// =======================================================================
// Internal sound data & structures
// =======================================================================

channel_t	channels[MAX_CHANNELS];
unsigned int	total_channels;

int		snd_blocked = 0;
qbool		snd_initialized = false;

int		soundtime;
int		paintedtime;

volatile dma_t	sn;
volatile dma_t	*shm = NULL;


static vec3_t	listener_origin;
static vec3_t	listener_forward;
static vec3_t	listener_right;
static vec3_t	listener_up;
#define sound_nominal_clip_dist 1000.0

// during registration it is possible to have more sounds
// than could actually be referenced during gameplay,
// because we don't want to free anything until we are
// sure we won't need it.
#define MAX_SFX (MAX_SOUNDS*2) // 256 * 2 = 512

static sfx_t	*known_sfx = NULL; // hunk allocated [MAX_SFX]
static int	num_sfx;

static qbool	sound_spatialized = false;

static sfx_t	*ambient_sfx[NUM_AMBIENTS];

// ====================================================================
// User-setable variables
// ====================================================================

cvar_t bgmvolume = {"bgmvolume", "1"};
cvar_t s_volume = {"volume", "0.7"};
cvar_t s_raw_volume = {"s_raw_volume", "1"};
cvar_t s_nosound = {"s_nosound", "0"};
cvar_t s_precache = {"s_precache", "1"};
cvar_t s_loadas8bit = {"s_loadas8bit", "0"};
cvar_t s_ambientlevel = {"s_ambientlevel", "0.3"};
cvar_t s_ambientfade = {"s_ambientfade", "100"};
cvar_t s_noextraupdate = {"s_noextraupdate", "0"};
cvar_t s_show = {"s_show", "0"};
cvar_t s_mixahead = {"s_mixahead", "0.1"};
cvar_t s_swapstereo = {"s_swapstereo", "0"};

cvar_t s_linearresample = {"s_linearresample", "1"};
cvar_t s_linearresample_stream = {"s_linearresample_stream", "0"};

cvar_t s_khz = {"s_khz", "11", CVAR_NONE, OnChange_s_khz};

/////////////////////////////////
// Specific os cvar/defaults
////////////////////////////////

#if defined(__FreeBSD__) || defined(__linux__)
cvar_t s_stereo = {"s_stereo", "1"};
cvar_t s_bits = {"s_bits", "16"};
cvar_t s_oss_device = {"s_oss_device", "/dev/dsp"};
#endif

#ifdef __linux__
cvar_t s_driver = {"s_driver", "alsa"};
cvar_t s_alsa_device = {"s_alsa_device", "default"};
cvar_t s_alsa_latency = {"s_alsa_latency", "0.04"};
#endif

#ifdef __FreeBSD__
cvar_t s_driver = {"s_driver", "oss"};
#endif


///////////////////////////////
// voice communication
//////////////////////////////

#ifdef FTE_PEXT2_VOICECHAT

static void S_Voip_Enable_f(void);
static void S_Voip_Disable_f(void);
static void S_Voip_f(void);

static void S_Voip_Play_Callback(cvar_t *var, char *string, qbool *cancel);
// Sends voice-over-ip data to the server whenever it is set.
cvar_t cl_voip_send = {"cl_voip_send", "0"};
// This is the threshhold for voice-activation-detection when sending voip data.
cvar_t cl_voip_vad_threshhold = {"cl_voip_vad_threshhold", "15"};
// Keeps sending voice data for this many seconds after voice activation would normally stop.
cvar_t cl_voip_vad_delay = {"cl_voip_vad_delay", "0.3"};
// Volume multiplier applied while capturing, to avoid your audio from being heard by others.
cvar_t cl_voip_capturingvol = {"cl_voip_capturingvol", "0.5"};
// Shows your speach volume above the hud. 0=hide, 1=show when transmitting, 2=ignore voice-activation disable.
cvar_t cl_voip_showmeter = {"cl_voip_showmeter", "1"};
cvar_t cl_voip_showmeter_x = {"cl_voip_showmeter_x", "0"};
cvar_t cl_voip_showmeter_y = {"cl_voip_showmeter_y", "0"};
// Enables voip playback.
cvar_t cl_voip_play = {"cl_voip_play", "1", CVAR_NONE, S_Voip_Play_Callback};
// Amplifies your microphone when using voip.
cvar_t cl_voip_micamp = {"cl_voip_micamp", "2"};
#endif

static void S_SoundInfo_f (void)
{
	if (!shm || !sounddriver) {
		Com_Printf ("sound system not started\n");
		return;
	}
#if defined(__linux__) || defined(__FreeBSD__)
	Com_Printf("driver: %s\n", s_driver.string);
#endif
	Com_Printf("%5d speakers\n", shm->format.channels);
	Com_Printf("%5d frames\n", shm->sampleframes);
	Com_Printf("%5d samples\n", shm->samples);
	Com_Printf("%5d samplepos\n", shm->samplepos);
	Com_Printf("%5d samplebits\n", shm->format.width * 8);
	Com_Printf("%5d speed\n", shm->format.speed);
	Com_Printf("%p dma buffer\n", shm->buffer);
	Com_Printf("%5u total_channels\n", total_channels);
}

static qbool S_Startup (void)
{
	if (!snd_initialized)
		return false;

	shm = &sn;
	memset((void *)shm, 0, sizeof(*shm));

	S_RawClear();

///////////////////////////////////////////////
#ifdef __linux__
	sounddriver = malloc(sizeof(*sounddriver));
	char *audio_driver = Cvar_String("s_driver");
	qbool retval = 0;

	if(sounddriver)	{
	//FIXME UGLY UGLY, make this in a cleaner way...

		if(strcmp(audio_driver, "alsa")==0) {
			retval = SNDDMA_Init_ALSA(sounddriver);
		} else if(strcmp(audio_driver, "oss")==0) {
			retval = SNDDMA_Init_OSS(sounddriver);
		}
		else {
			Com_Printf("SNDDMA_Init: Error, unknown s_driver \"%s\"\n", audio_driver);
		}
	}
	if(retval == 0) {
		shm = NULL;
		sound_spatialized = false;
		free(sounddriver);
		return false;
	}

#elif defined(__FreeBSD__)
	//FIXME UGLY UGLY UGLY
	retval = SNDDMA_Init_OSS(sounddriver);
	if(!retval) {
		shm = NULL;
		sound_spatialized = false;
		free(sounddriver);
		return false;
	}
#else
	if (!SNDDMA_Init()) {
		Com_Printf ("S_Startup: SNDDMA_Init failed.\n");
		shm = NULL;
		sound_spatialized = false;
		return false;
	}
#endif
//////////////////////////////////////////////

	return true;
}

void S_Shutdown (void)
{
	if (!shm)
		return;

#ifdef __linux__
	sounddriver->Shutdown();
	free(sounddriver);
	sounddriver = NULL;
#else
	SNDDMA_Shutdown();
#endif

	shm = NULL;
	sound_spatialized = false;
}

static void S_Restart_f (void)
{
	Com_DPrintf("Restarting sound system....\n");
	Cache_Flush();
	S_StopAllSounds (true);

	S_Shutdown();
	Com_DPrintf("sound: Shutdown OK\n");

	if (!S_Startup()) {
		snd_initialized = false;
		return;
	}

	ambient_sfx[AMBIENT_WATER] = S_PrecacheSound ("ambience/water1.wav");
	ambient_sfx[AMBIENT_SKY] = S_PrecacheSound ("ambience/wind2.wav");

	S_StopAllSounds (true);

	if (developer.value)
		S_SoundInfo_f();
}

static void OnChange_s_khz (cvar_t *var, char *string, qbool *cancel) {
	Cbuf_AddText("s_restart\n");
}

void S_Init (void)
{
	if (snd_initialized) { //whoops
		Com_Printf_State (PRINT_INFO, "Sound is already initialized\n");
		return;
	}

	Com_DPrintf("\nSound Initialization\n");

	Cvar_SetCurrentGroup(CVAR_GROUP_SOUND);
	Cvar_Register(&bgmvolume);
	Cvar_Register(&s_volume);
	Cvar_Register(&s_raw_volume);
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

	Cvar_Register(&s_linearresample);
	Cvar_Register(&s_linearresample_stream);

#if (defined(__linux__) || defined(__FreeBSD__))
	Cvar_Register(&s_stereo);
	Cvar_Register(&s_oss_device);
	Cvar_Register(&s_driver);
	Cvar_Register(&s_bits);
#endif
#ifdef __linux__
	Cvar_Register(&s_alsa_device);
	Cvar_Register(&s_alsa_latency);
#endif

#ifdef FTE_PEXT2_VOICECHAT
	Cvar_Register(&cl_voip_send);
	Cvar_Register(&cl_voip_vad_threshhold);
	Cvar_Register(&cl_voip_vad_delay);
	Cvar_Register(&cl_voip_capturingvol);
	Cvar_Register(&cl_voip_showmeter);
	Cvar_Register(&cl_voip_showmeter_x);
	Cvar_Register(&cl_voip_showmeter_y);
	Cvar_Register(&cl_voip_play);
	Cvar_Register(&cl_voip_micamp);

	Cmd_AddCommand("+voip", S_Voip_Enable_f);
	Cmd_AddCommand("-voip", S_Voip_Disable_f);
	Cmd_AddCommand("voip", S_Voip_f);
#endif

	Cvar_ResetCurrentGroup();

	// compatibility with old configs
	Cmd_AddLegacyCommand ("nosound", "s_nosound");
	Cmd_AddLegacyCommand ("precache", "s_precache");
	Cmd_AddLegacyCommand ("loadas8bit", "s_loadas8bit");
	Cmd_AddLegacyCommand ("ambient_level", "s_ambientlevel");
	Cmd_AddLegacyCommand ("ambient_fade", "s_ambientfade");
	Cmd_AddLegacyCommand ("snd_noextraupdate", "s_noextraupdate");
	Cmd_AddLegacyCommand ("snd_show", "s_show");
	Cmd_AddLegacyCommand ("_snd_mixahead", "s_mixahead");

	if (COM_CheckParm("-nosound")) {
		Cmd_AddLegacyCommand ("play", ""); // just suppress warnings
		return;
	}

	Cmd_AddCommand("s_restart", S_Restart_f);
	Cmd_AddLegacyCommand("snd_restart", "s_restart");	// exclusively for Disconnect
	Cmd_AddCommand("play", S_Play_f);
	Cmd_AddCommand("playvol", S_PlayVol_f);
	Cmd_AddCommand("stopsound", S_StopAllSounds_f);
	Cmd_AddCommand("soundlist", S_SoundList_f);
	Cmd_AddCommand("soundinfo", S_SoundInfo_f);

	if (!snd_initialized && host_memsize < 0x800000) {
		Cvar_Set (&s_loadas8bit, "1");
		Com_Printf ("loading all sounds as 8bit\n");
	}

	snd_initialized = true;

	SND_InitScaletable ();

	if (!S_Startup ()) {
		snd_initialized = false;
		 return;
	}

	known_sfx = (sfx_t *) Hunk_AllocName (MAX_SFX * sizeof(sfx_t), "sfx_t");
	num_sfx = 0;

	ambient_sfx[AMBIENT_WATER] = S_PrecacheSound ("ambience/water1.wav");
	ambient_sfx[AMBIENT_SKY] = S_PrecacheSound ("ambience/wind2.wav");

	S_StopAllSounds (true);

}

// =======================================================================
// Load a sound
// =======================================================================

static sfx_t *S_FindName (char *name)
{
	int i;
	sfx_t *sfx;

	if (!snd_initialized)
		return NULL;

	if (!name)
		Sys_Error ("S_FindName: NULL");

	if (strlen(name) >= MAX_QPATH) {
		Com_Printf ("S_FindName: sound name too long (%s)\n", name);
		return NULL;
	}

	// see if already loaded
	for (i = 0; i < num_sfx; i++) {
		if (!strcmp(known_sfx[i].name, name))
			return &known_sfx[i];
	}

	if (num_sfx == MAX_SFX)
		Sys_Error ("S_FindName: out of sfx_t");

	sfx = &known_sfx[i];
	strlcpy (sfx->name, name, sizeof (sfx->name));

	num_sfx++;

	return sfx;
}

sfx_t *S_PrecacheSound (char *name)
{
	sfx_t *sfx;

	if (!snd_initialized || s_nosound.value)
		return NULL;

	if (name == NULL || name[0] == 0)
		return NULL;

	sfx = S_FindName (name);
	if (sfx == NULL)
		return NULL;

	// cache it in
	if (s_precache.value)
		S_LoadSound (sfx);

	return sfx;
}

//=============================================================================

// picks a channel based on priorities, empty slots, number of channels
static channel_t *SND_PickChannel (int entnum, int entchannel)
{
	int ch_idx, first_to_die, life_left;

	// Check for replacement sound, or find the best one to replace
	first_to_die = -1;
	life_left = 0x7fffffff;
	for (ch_idx = NUM_AMBIENTS; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; ch_idx++) {
		if (entchannel != 0		// channel 0 never overrides
		        && channels[ch_idx].entnum == entnum
		        && (channels[ch_idx].entchannel == entchannel || entchannel == -1) ) {
			// always override sound from same entity
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

// spatializes a channel
static void SND_Spatialize (channel_t *ch)
{
	vec_t dot, dist, lscale, rscale, scale;
	vec3_t source_vec;
	sfx_t *snd;

	// anything coming from the view entity will always be full volume
	if ((ch->entnum == cl.playernum + 1) || (ch->entnum == SELF_SOUND)) {
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
		return;
	}

	// calculate stereo seperation and distance attenuation

	snd = ch->sfx;
	VectorSubtract(ch->origin, listener_origin, source_vec);

	dist = VectorNormalize(source_vec) * ch->dist_mult;
	dot = DotProduct(listener_right, source_vec);

	if (shm->format.channels == 1) {
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

void S_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol, float attenuation)
{
	channel_t *target_chan, *check;
	sfxcache_t *sc;
	int ch_idx, skip;

	if (!shm || !sfx || s_nosound.value)
		return;

	// pick a channel to play on
	target_chan = SND_PickChannel(entnum, entchannel);
	if (!target_chan)
		return;

	// spatialize
	memset (target_chan, 0, sizeof(*target_chan));
	VectorCopy(origin, target_chan->origin);
	target_chan->dist_mult = attenuation / sound_nominal_clip_dist;
	target_chan->master_vol = (int) (fvol * 255);
	target_chan->entnum = entnum;
	target_chan->entchannel = entchannel;
	SND_Spatialize(target_chan);

	if (!target_chan->leftvol && !target_chan->rightvol)
		return; // not audible at all

	// new channel
	sc = S_LoadSound (sfx);
	if (!sc) {
		target_chan->sfx = NULL;
		return; // couldn't load the sound's data
	}

	target_chan->sfx = sfx;
	target_chan->pos = 0.0;
	target_chan->end = paintedtime + (int) sc->total_length;

	// if an identical sound has also been started this frame, offset the pos
	// a bit to keep it from just making the first one louder
	check = &channels[NUM_AMBIENTS];
	for (ch_idx=NUM_AMBIENTS; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; ch_idx++, check++) {
		if (check == target_chan)
			continue;
		if (check->sfx == sfx && !check->pos) {
			skip = rand () % (int)(0.1 * shm->format.speed);
			if (skip >= target_chan->end)
				skip = target_chan->end - 1;
			target_chan->pos += skip;
			target_chan->end -= skip;
			break;
		}
	}
}

void S_StopSound (int entnum, int entchannel)
{
	unsigned int i;

	for (i = 0; i < MAX_DYNAMIC_CHANNELS; i++) {
		if (channels[i].entnum == entnum && channels[i].entchannel == entchannel) {
			channels[i].end = 0;
			channels[i].sfx = NULL;
			return;
		}
	}
}

void S_StopAllSounds (qbool clear)
{
	int i;

	if (!shm)
		return;

	total_channels = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS; // no statics

	for (i = 0; i < MAX_CHANNELS; i++) {
		if (channels[i].sfx)
			channels[i].sfx = NULL;
	}

	memset(channels, 0, MAX_CHANNELS * sizeof(channel_t));

	if (clear)
		S_ClearBuffer ();
}

static void S_StopAllSounds_f (void)
{
	S_StopAllSounds (true);
}


#ifdef _WIN32
extern char *DSoundError (int error);
#endif

void S_ClearBuffer (void)
{
	int clear;

#ifdef _WIN32
	if (!shm || (!shm->buffer && !pDSBuf))
#else
	if (!shm || !shm->buffer)
#endif
		return;

	clear = (shm->format.width == 2) ? 0x80 : 0;

#ifdef _WIN32
	if (pDSBuf) {
		DWORD dwSize;
		DWORD *pData;
		int reps;
		HRESULT hresult;

		reps = 0;

		while ((hresult = pDSBuf->lpVtbl->Lock(pDSBuf, 0, gSndBufSize, &pData, &dwSize, NULL, NULL, 0)) != DS_OK) {
			if (hresult != DSERR_BUFFERLOST) {
				Com_Printf ("S_ClearBuffer: Lock failed with error '%s'\n", DSoundError(hresult));
				S_Shutdown ();
				return;
			} else {
				pDSBuf->lpVtbl->Restore (pDSBuf);
			}

			if (++reps > 2)
				return;
		}

		memset(pData, clear, shm->bufferlength);

		pDSBuf->lpVtbl->Unlock(pDSBuf, pData, dwSize, NULL, 0);

	} else
#endif
	{
		memset(shm->buffer, clear, shm->bufferlength);
	}
}

void S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation)
{
	channel_t *ss;
	sfxcache_t *sc;

	if (!shm || !sfx || s_nosound.value)
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
	ss->master_vol = (int) vol;
	ss->dist_mult = (attenuation/64) / sound_nominal_clip_dist;
	ss->end = paintedtime + (int) sc->total_length;

	SND_Spatialize (ss);
}

//=============================================================================

static void S_UpdateAmbientSounds (void)
{
	struct cleaf_s *leaf;
	int vol;
	int ambient_channel;
	channel_t *chan;

	if (cls.state != ca_active)
		return;

	leaf = CM_PointInLeaf (listener_origin);
	if (!CM_Leafnum(leaf) || !s_ambientlevel.value) {
		for (ambient_channel = 0 ; ambient_channel< NUM_AMBIENTS ; ambient_channel++)
			channels[ambient_channel].sfx = NULL;
		return;
	}

	for (ambient_channel = 0 ; ambient_channel< NUM_AMBIENTS ; ambient_channel++) {
		chan = &channels[ambient_channel];
		chan->sfx = ambient_sfx[ambient_channel];

		vol = (int) (s_ambientlevel.value * CM_LeafAmbientLevel(leaf, ambient_channel));
		if (vol < 8)
			vol = 0;

		// don't adjust volume too fast
		if (chan->master_vol < vol) {
			chan->master_vol += Q_rint (cls.frametime * s_ambientfade.value);
			if (chan->master_vol > vol)
				chan->master_vol = vol;
		} else if (chan->master_vol > vol) {
			chan->master_vol -= Q_rint (cls.frametime * s_ambientfade.value);
			if (chan->master_vol < vol)
				chan->master_vol = vol;
		}

		chan->leftvol = chan->rightvol = chan->master_vol;
	}
}

//Called once each time through the main loop
void S_Update (vec3_t origin, vec3_t forward, vec3_t right, vec3_t up)
{
	unsigned int i, j, total;
	channel_t *ch, *combine;

	if (!snd_initialized || (snd_blocked > 0) || !shm)
		return;

	VectorCopy(origin, listener_origin);
	VectorCopy(forward, listener_forward);
	VectorCopy(right, listener_right);
	VectorCopy(up, listener_up);

	// update general area ambient sound sources
	S_UpdateAmbientSounds ();

	combine = NULL;

	// update spatialization for static and dynamic sounds
	ch = channels + NUM_AMBIENTS;
	for (i = NUM_AMBIENTS; i < total_channels; i++, ch++) {
		if (!ch->sfx)
			continue;
		SND_Spatialize(ch); // respatialize channel
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

	sound_spatialized = true;

	// debugging output
	if (s_show.value) {
		total = 0;
		ch = channels;
		for (i = 0; i < total_channels; i++, ch++)
			if (ch->sfx && (ch->leftvol || ch->rightvol)) {
#if defined(DEBUG) || defined(_DEBUG)
				if (s_show.value == 2)
					Com_Printf ("%3i %3i %s\n", ch->leftvol, ch->rightvol, ch->sfx->name);
#endif
				total++;
			}
		Print_flags[Print_current] |= PR_TR_SKIP;
		Com_Printf ("----(%i)----\n", total);
	}

	// mix some sound
	S_Update_();
}

static void GetSoundtime (void)
{
	int samplepos, fullsamples;
	static int buffers, oldsamplepos;

	//joe: capturing audio
#ifdef _WIN32
	if (Movie_GetSoundtime())
		return;
#endif

	fullsamples = shm->sampleframes;

	// it is possible to miscount buffers if it has wrapped twice between calls to S_Update.  Oh well.
#ifdef __linux__
	samplepos = sounddriver->GetDMAPos();
#else
	samplepos = SNDDMA_GetDMAPos();
#endif

	if (samplepos < oldsamplepos) {
		buffers++; // buffer wrapped

		if (paintedtime > 0x40000000) {
			// time to chop things off to avoid 32 bit limits
			buffers = 0;
			paintedtime = fullsamples;
			S_StopAllSounds (true);
		}
	}

	oldsamplepos = samplepos;

	soundtime = buffers * fullsamples + samplepos / shm->format.channels;
}

void S_ExtraUpdate (void)
{

	//joe: capturing audio
#ifdef _WIN32
	if (Movie_IsCapturing() && movie_is_avi)
		return;
#endif

#ifdef _WIN32
	IN_Accumulate ();
#endif

	if (s_noextraupdate.value || !sound_spatialized)
		return; // don't pollute timings

	S_Update_();
}

static void S_Update_ (void)
{
	unsigned int endtime;
#ifdef _WIN32
	DWORD dwStatus;
#endif

	if (!shm || (snd_blocked > 0))
		return;

	// Updates DMA time
	GetSoundtime();

	// check to make sure that we haven't overshot
	if (paintedtime < soundtime) {
		//Com_Printf ("S_Update_ : overflow\n");
		paintedtime = soundtime;
	}

	// mix ahead of current position
	endtime = soundtime + (unsigned int) (s_mixahead.value * shm->format.speed);
	endtime = min(endtime, (unsigned int)(soundtime + shm->sampleframes));
#ifdef __linux__
	int avail;
	//mix ahead of current position
	if(sounddriver->GetAvail) {
		avail = sounddriver->GetAvail();
		if(avail <= 0)
			return;
		endtime = soundtime + avail;
	}
	//FIXME Look at fodquake source, some more stuff to do later
#endif

#ifdef _WIN32
	// if the buffer was lost or stopped, restore it and/or restart it
	if (pDSBuf) {
		if (pDSBuf->lpVtbl->GetStatus (pDSBuf, &dwStatus) != DS_OK)
			Com_Printf ("Couldn't get sound buffer status\n");

		if (dwStatus & DSBSTATUS_BUFFERLOST)
			pDSBuf->lpVtbl->Restore (pDSBuf);

		if (!(dwStatus & DSBSTATUS_PLAYING))
			pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);
	}

#endif

	S_PaintChannels (endtime);

#if defined(__linux__) || defined(__FreeBSD__)
	if(sounddriver->Submit)
		sounddriver->Submit(paintedtime - soundtime);
#else
	SNDDMA_Submit ();
#endif

}

/*
===============================================================================
console functions
===============================================================================
*/

static void S_Play_f (void)
{
	int i;
	char name[256];
	sfx_t *sfx;

	if (!snd_initialized || s_nosound.value)
		return;

	for (i = 1; i < Cmd_Argc(); i++) {
		strlcpy (name, Cmd_Argv(i), sizeof (name));
		COM_DefaultExtension (name, ".wav");
		sfx = S_PrecacheSound(name);
		S_StartSound(SELF_SOUND, 0, sfx, listener_origin, 1.0, 0.0);
	}
}

static void S_PlayVol_f (void)
{
	int i;
	float vol;
	char name[256];
	sfx_t *sfx;

	if (!snd_initialized || s_nosound.value)
		return;

	for (i = 1; i < Cmd_Argc(); i += 2) {
		strlcpy (name, Cmd_Argv(i), sizeof (name));
		COM_DefaultExtension (name, ".wav");
		sfx = S_PrecacheSound(name);
		vol = Q_atof(Cmd_Argv(i + 1));
		// ezhfan:
		// pnum+1 changed to SELF_SOUND to make sound not to disappear
		S_StartSound(SELF_SOUND, 0, sfx, listener_origin, vol, 0.0);
	}
}

static void S_SoundList_f (void)
{
	int i, total = 0;
	unsigned int size;
	sfx_t *sfx;
	sfxcache_t *sc;

	for (sfx = known_sfx, i = 0; i < num_sfx; i++, sfx++) {
		sc = (sfxcache_t *) Cache_Check (&sfx->cache);
		if (!sc)
			continue;
		size = sc->total_length * sc->format.width * (sc->format.channels);
		total += size;
		if (sc->loopstart >= 0)
			Com_Printf ("L");
		else
			Com_Printf (" ");
		Com_Printf ("(%2db) %6i : %s\n",sc->format.width*8,  size, sfx->name);
	}
	Com_Printf ("Total resident: %i\n", total);
}

void S_LocalSound (char *sound)
{
	sfx_t *sfx;

	if (!snd_initialized || s_nosound.value)
		return;

	if (!(sfx = S_PrecacheSound (sound))) {
		Com_Printf ("S_LocalSound: can't cache %s\n", sound);
		return;
	}

	S_StartSound (cl.playernum+1, -1, sfx, vec3_origin, 1, 0);
}

void S_LocalSoundWithVol(char *sound, float volume)
{
	sfx_t *sfx;

	clamp(volume, 0, 1.0);

	if (!snd_initialized || s_nosound.value)
		return;

	if (!(sfx = S_PrecacheSound (sound))) {
		Com_Printf ("S_LocalSound: can't cache %s\n", sound);
		return;
	}

	S_StartSound (cl.playernum+1, -1, sfx, vec3_origin, volume, 1);
}

#ifdef FTE_PEXT2_VOICECHAT

// FTEQW type and naming compatibility
// it's not really necessary, simple find & replace would do the job too
#ifdef _MSC_VER
#define VARGS __cdecl
#endif
#ifndef VARGS
#define VARGS
#endif

#define qboolean qbool
#define qbyte byte

#define ival integer // for cvars compatibility
#define realtime cls.realtime

//=========================================================================

#include <speex/speex.h>
#include <speex/speex_preprocess.h>

static struct
{
	qboolean inited;
	qboolean loaded;
	dllhandle_t *speexlib;
	dllhandle_t *speexdsplib;

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
	qboolean wantsend;	/*set if we're capturing data to send*/
	float voiplevel;	/*your own voice level*/
	unsigned int dumps;	/*trigger a new generation thing after a bit*/
	unsigned int keeps;	/*for vad_delay*/

	snd_capture_driver_t *driver;/*capture driver's functions*/
	void *driverctx;	/*capture driver context*/
} s_speex;

static const SpeexMode *(VARGS *qspeex_lib_get_mode)(int mode);
static void (VARGS *qspeex_bits_init)(SpeexBits *bits);
static void (VARGS *qspeex_bits_reset)(SpeexBits *bits);
static int (VARGS *qspeex_bits_write)(SpeexBits *bits, char *bytes, int max_len);

static SpeexPreprocessState *(VARGS *qspeex_preprocess_state_init)(int frame_size, int sampling_rate);
static int (VARGS *qspeex_preprocess_ctl)(SpeexPreprocessState *st, int request, void *ptr);
static int (VARGS *qspeex_preprocess_run)(SpeexPreprocessState *st, spx_int16_t *x);

static void * (VARGS *qspeex_encoder_init)(const SpeexMode *mode);
static int (VARGS *qspeex_encoder_ctl)(void *state, int request, void *ptr);
static int (VARGS *qspeex_encode_int)(void *state, spx_int16_t *in, SpeexBits *bits);

static void *(VARGS *qspeex_decoder_init)(const SpeexMode *mode);
static int (VARGS *qspeex_decode_int)(void *state, SpeexBits *bits, spx_int16_t *out);
static void (VARGS *qspeex_bits_read_from)(SpeexBits *bits, char *bytes, int len);

static dllfunction_t qspeexfuncs[] =
{
	{(void*)&qspeex_lib_get_mode, "speex_lib_get_mode"},
	{(void*)&qspeex_bits_init, "speex_bits_init"},
	{(void*)&qspeex_bits_reset, "speex_bits_reset"},
	{(void*)&qspeex_bits_write, "speex_bits_write"},

	{(void*)&qspeex_encoder_init, "speex_encoder_init"},
	{(void*)&qspeex_encoder_ctl, "speex_encoder_ctl"},
	{(void*)&qspeex_encode_int, "speex_encode_int"},

	{(void*)&qspeex_decoder_init, "speex_decoder_init"},
	{(void*)&qspeex_decode_int, "speex_decode_int"},
	{(void*)&qspeex_bits_read_from, "speex_bits_read_from"},

	{NULL}
};

static dllfunction_t qspeexdspfuncs[] =
{
	{(void*)&qspeex_preprocess_state_init, "speex_preprocess_state_init"},
	{(void*)&qspeex_preprocess_ctl, "speex_preprocess_ctl"},
	{(void*)&qspeex_preprocess_run, "speex_preprocess_run"},

	{NULL}
};

extern snd_capture_driver_t DSOUND_Capture;

static qboolean S_Speex_Init(void)
{
	int i;
	const SpeexMode *mode;
	if (s_speex.inited)
		return s_speex.loaded;
	s_speex.inited = true;

	s_speex.speexlib = Sys_LoadLibrary("libspeex", qspeexfuncs);
	if (!s_speex.speexlib)
	{
		Con_Printf("libspeex not found. Voice chat not available.\n");
		return false;
	}

	s_speex.speexdsplib = Sys_LoadLibrary("libspeexdsp", qspeexdspfuncs);
	if (!s_speex.speexdsplib)
	{
		Con_Printf("libspeexdsp not found. Voice chat not available.\n");
		return false;
	}

	mode = qspeex_lib_get_mode(SPEEX_MODEID_NB);


	qspeex_bits_init(&s_speex.encbits);
	qspeex_bits_reset(&s_speex.encbits);

	s_speex.encoder = qspeex_encoder_init(mode);

	qspeex_encoder_ctl(s_speex.encoder, SPEEX_GET_FRAME_SIZE, &s_speex.framesize);
	qspeex_encoder_ctl(s_speex.encoder, SPEEX_GET_SAMPLING_RATE, &s_speex.samplerate);
	s_speex.samplerate = 11025;
	qspeex_encoder_ctl(s_speex.encoder, SPEEX_SET_SAMPLING_RATE, &s_speex.samplerate);

	s_speex.preproc = qspeex_preprocess_state_init(s_speex.framesize, s_speex.samplerate);

	i = 1;
	qspeex_preprocess_ctl(s_speex.preproc, SPEEX_PREPROCESS_SET_DENOISE, &i);

	i = 1;
	qspeex_preprocess_ctl(s_speex.preproc, SPEEX_PREPROCESS_SET_AGC, &i);

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		qspeex_bits_init(&s_speex.decbits[i]);
		qspeex_bits_reset(&s_speex.decbits[i]);
		s_speex.decoder[i] = qspeex_decoder_init(mode);
		s_speex.decamp[i] = 1;
	}
	s_speex.loaded = true;
	return s_speex.loaded;
}

void S_Voip_Parse(void)
{
	unsigned int sender;
	int bytes;
	unsigned char data[1024], *start;
	short decodebuf[1024];
	unsigned int decodesamps, len, newseq, drops;
	unsigned char seq, gen;
	float amp = 1;
	unsigned int i;

	sender = MSG_ReadByte();
	gen = MSG_ReadByte();
	seq = MSG_ReadByte();
	bytes = MSG_ReadShort();

//	Com_DPrintf("data in: %d\n", bytes);

	if (bytes > sizeof(data) || !cl_voip_play.ival || !S_Speex_Init() || (gen & 0xf0))
	{
		Com_DPrintf("skip data: %d\n", bytes);
		MSG_ReadSkip(bytes);
		return;
	}
	MSG_ReadData(data, bytes);

	sender &= MAX_CLIENTS-1;

	amp = s_speex.decamp[sender];

	decodesamps = 0;
	newseq = 0;
	drops = 0;
	start = data;

	s_speex.lastspoke[sender] = realtime + 0.5;
	if (s_speex.decgen[sender] != gen)
	{
		qspeex_bits_reset(&s_speex.decbits[sender]);
		s_speex.decgen[sender] = gen;
		s_speex.decseq[sender] = seq;
	}

	while (bytes > 0)
	{
		if (decodesamps + s_speex.framesize > sizeof(decodebuf)/sizeof(decodebuf[0]))
		{
			S_RawAudio(sender, (qbyte*)decodebuf, s_speex.samplerate, decodesamps, 1, 2);
			decodesamps = 0;
		}

		if (s_speex.decseq[sender] != seq)
		{
			qspeex_decode_int(s_speex.decoder[sender], NULL, decodebuf + decodesamps);
			s_speex.decseq[sender]++;
			drops++;
		}
		else
		{
			bytes--;
			len = *start++;
			qspeex_bits_read_from(&s_speex.decbits[sender], start, len);
			bytes -= len;
			start += len;
			qspeex_decode_int(s_speex.decoder[sender], &s_speex.decbits[sender], decodebuf + decodesamps);
			newseq++;
		}
		if (amp != 1)
		{
			for (i = decodesamps; i < decodesamps+s_speex.framesize; i++)
				decodebuf[i] *= amp;
		}
		decodesamps += s_speex.framesize;
	}
	s_speex.decseq[sender] += newseq;

	if (drops)
		Con_DPrintf("%i dropped audio frames\n", drops);

	if (decodesamps > 0)
		S_RawAudio(sender, (qbyte*)decodebuf, s_speex.samplerate, decodesamps, 1, 2);
}

void S_Voip_Transmit(unsigned char clc, sizebuf_t *buf)
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
	qboolean voipsendenable = true;

	/*if you're sending sound, you should be prepared to accept others yelling at you to shut up*/
	if (!cl_voip_play.ival)
		voipsendenable = false;
	if (!(cls.fteprotocolextensions2 & FTE_PEXT2_VOICECHAT))
		voipsendenable = false;

	if (!voipsendenable)
	{
		if (s_speex.driver)
		{
			if (s_speex.wantsend)
				s_speex.driver->Stop(s_speex.driverctx);
			s_speex.driver->Shutdown(s_speex.driverctx);
			s_speex.driverctx = NULL;
			s_speex.driver = NULL;
		}
		return;
	}

	voipsendenable = cl_voip_send.ival>0;

	if (!s_speex.driver)
	{
		s_speex.voiplevel = -1;
		/*only init the first time capturing is requested*/
		if (!voipsendenable)
			return;

		/*Add new drivers in order of priority*/
		if (!s_speex.driver)
			s_speex.driver = &DSOUND_Capture;

		/*no way to capture audio, give up*/
		if (!s_speex.driver)
			return;

		/*see if we can init speex...*/
		if (!S_Speex_Init())
			return;

		s_speex.driverctx = s_speex.driver->Init(s_speex.samplerate);
	}

	/*couldn't init a driver?*/
	if (!s_speex.driverctx)
	{
		return;
	}

	if (!voipsendenable && s_speex.wantsend)
	{
		s_speex.wantsend = false;
		s_speex.capturepos += s_speex.driver->Update(s_speex.driverctx, (unsigned char*)s_speex.capturebuf + s_speex.capturepos, 1, sizeof(s_speex.capturebuf) - s_speex.capturepos);
		s_speex.driver->Stop(s_speex.driverctx);
		/*note: we still grab audio to flush everything that was captured while it was active*/
	}
	else if (voipsendenable && !s_speex.wantsend)
	{
		s_speex.wantsend = true;
		if (!s_speex.capturepos)
		{	/*if we were actually still sending, it was probably only off for a single frame, in which case don't reset it*/
			s_speex.dumps = 0;
			s_speex.generation++;
			s_speex.encsequence = 0;
			qspeex_bits_reset(&s_speex.encbits);
		}
		else
		{
			s_speex.capturepos += s_speex.driver->Update(s_speex.driverctx, (unsigned char*)s_speex.capturebuf + s_speex.capturepos, 1, sizeof(s_speex.capturebuf) - s_speex.capturepos);
		}
		s_speex.driver->Start(s_speex.driverctx);

		voicevolumemod = cl_voip_capturingvol.value;
	}

	s_speex.capturepos += s_speex.driver->Update(s_speex.driverctx, (unsigned char*)s_speex.capturebuf + s_speex.capturepos, s_speex.framesize*2, sizeof(s_speex.capturebuf) - s_speex.capturepos);

	if (!s_speex.wantsend && s_speex.capturepos < s_speex.framesize*2)
	{
		s_speex.voiplevel = -1;
		s_speex.capturepos = 0;
		voicevolumemod = 1;
		return;
	}

	initseq = s_speex.encsequence;
	level = 0;
	samps=0;
	for (encpos = 0, outpos = 0; s_speex.capturepos-encpos >= s_speex.framesize*2 && sizeof(outbuf)-outpos > 64; s_speex.encsequence++)
	{
		start = (short*)(s_speex.capturebuf + encpos);

		qspeex_preprocess_run(s_speex.preproc, start);

		for (i = 0; i < s_speex.framesize; i++)
		{
			f = start[i] * micamp;
			start[i] = f;
			f = fabs(start[i]);
			level += f*f;
		}
		samps+=s_speex.framesize;

		qspeex_bits_reset(&s_speex.encbits);
		qspeex_encode_int(s_speex.encoder, start, &s_speex.encbits);
		outbuf[outpos] = qspeex_bits_write(&s_speex.encbits, outbuf+outpos+1, sizeof(outbuf) - (outpos+1));
		outpos += 1+outbuf[outpos];
		encpos += s_speex.framesize*2;
	}
	if (samps)
	{
		float nl;
		nl = (3000*level) / (32767.0f*32767*samps);
		s_speex.voiplevel = (s_speex.voiplevel*7 + nl)/8;
		if (s_speex.voiplevel < cl_voip_vad_threshhold.ival && !(cl_voip_send.ival & 2))
		{
			/*try and dump it, it was too quiet, and they're not pressing +voip*/
			if (s_speex.keeps > samps)
			{
				/*but not instantly*/
				s_speex.keeps -= samps;
			}
			else
			{
				outpos = 0;
				s_speex.dumps += samps;
				s_speex.keeps = 0;
			}
		}
		else
			s_speex.keeps = s_speex.samplerate * cl_voip_vad_delay.value;
		if (outpos)
		{
			if (s_speex.dumps > s_speex.samplerate/4)
				s_speex.generation++;
			s_speex.dumps = 0;
		}
	}

	if (outpos && buf->maxsize - buf->cursize >= outpos+4)
	{
		MSG_WriteByte(buf, clc);
		MSG_WriteByte(buf, (s_speex.generation & 0x0f)); /*gonna leave that nibble clear here... in this version, the client will ignore packets with those bits set. can use them for codec or something*/
		MSG_WriteByte(buf, initseq);
		MSG_WriteShort(buf, outpos);
		SZ_Write(buf, outbuf, outpos);

//		Com_DPrintf("data out: %d\n", outpos);
	}

	/*remove sent data*/
	memmove(s_speex.capturebuf, s_speex.capturebuf + encpos, s_speex.capturepos-encpos);
	s_speex.capturepos -= encpos;
}

void S_Voip_Ignore(unsigned int slot, qboolean ignore)
{
	CL_SendClientCommand(true, "vignore %i %i", slot, ignore);
}

static void S_Voip_Enable_f(void)
{
	Cvar_SetValue(&cl_voip_send, cl_voip_send.ival | 2);
}

static void S_Voip_Disable_f(void)
{
	Cvar_SetValue(&cl_voip_send, cl_voip_send.ival & ~2);
}

static void S_Voip_f(void)
{
	int i;
	if (!strcmp(Cmd_Argv(1), "maxgain"))
	{
		i = atoi(Cmd_Argv(2));
		qspeex_preprocess_ctl(s_speex.preproc, SPEEX_PREPROCESS_SET_AGC_MAX_GAIN, &i);
	}
}

static void S_Voip_Play_Callback(cvar_t *var, char *string, qbool *cancel)
{
	if (cls.fteprotocolextensions2 & FTE_PEXT2_VOICECHAT)
	{
		if (var->ival)
			CL_SendClientCommand(true, "unmuteall");
		else
			CL_SendClientCommand(true, "muteall");
	}
}

void S_Voip_MapChange(void)
{
	Cvar_ForceCallback(&cl_voip_play);
}

int S_Voip_Loudness(qboolean ignorevad)
{
	if (s_speex.voiplevel > 100)
		return 100;
	if (!s_speex.driverctx || (!ignorevad && s_speex.dumps))
		return -1;
	return s_speex.voiplevel;
}

qboolean S_Voip_Speaking(unsigned int plno)
{
	if (plno >= MAX_CLIENTS)
		return false;
	return s_speex.lastspoke[plno] > realtime;
}

#endif

//===================================================================
// Streaming audio.
// This is useful when there is one source, and the sound is to be played with no attenuation.
//===================================================================

#define MAX_RAW_CACHE (1024 * 32) // have no idea which size it actually should be.

typedef struct
{
	qbool			inuse;
	int				id;
	sfx_t			sfx;
} streaming_t;

static void S_RawClearStream(streaming_t *s);

#define MAX_RAW_SOURCES (MAX_CLIENTS+1)

streaming_t s_streamers[MAX_RAW_SOURCES] = {0};

void S_RawClear(void)
{
	int i;
	streaming_t *s;

	for (s = s_streamers, i = 0; i < MAX_RAW_SOURCES; i++, s++)
	{
		S_RawClearStream(s);
	}

	memset(s_streamers, 0, sizeof(s_streamers));
}

// Stop playing particular stream and make it free.
static void S_RawClearStream(streaming_t *s)
{
	int i;
	sfxcache_t * currentcache;

	if (!s)
		return;

	// get current cache if any.
	currentcache = (sfxcache_t *)Cache_Check(&s->sfx.cache);
	if (currentcache)
	{
		currentcache->loopstart = -1;	//stop mixing it
	}

	// remove link on sfx from the channels array.
	for (i = 0; i < total_channels; i++)
	{
		if (channels[i].sfx == &s->sfx)
		{
			channels[i].sfx = NULL;
			break;
		}
	}

	// free cache.
	if (s->sfx.cache.data)
		Cache_Free(&s->sfx.cache);

	// clear whole struct.
	memset(s, 0, sizeof(*s));
}

// Searching for free slot or re-use previous one with the same sourceid.
static streaming_t * S_RawGetFreeStream(int sourceid)
{
	int i;
	streaming_t *s, *free = NULL;

	for (s = s_streamers, i = 0; i < MAX_RAW_SOURCES; i++, s++)
	{
		if (!s->inuse)
		{
			if (!free)
			{
				free = s;	// found free stream slot.
			}

			continue;
		}

		if (s->id == sourceid)
		{
			return s; // re-using slot.
		}
	}

	return free;
}

// Streaming audio.
// This is useful when there is one source, and the sound is to be played with no attenuation.
void S_RawAudio(int sourceid, byte *data, 
				unsigned int speed, unsigned int samples, unsigned int channelsnum, unsigned int width)
{
	unsigned int	i;
	int				newsize;
	int				prepadl;
	int				spare;
	int				outsamples;
	double			speedfactor;
	sfxcache_t *	currentcache;
	streaming_t *	s;

	// search for free slot or re-use previous one with the same sourceid.
	s = S_RawGetFreeStream(sourceid);

	if (!s)
	{
		Com_DPrintf("No free audio streams or stream not found\n");
		return;
	}

	// empty data mean someone tell us to shut up particular slot.
	if (!data)
	{
		S_RawClearStream(s);
		return;
	}

	// attempting to add new stream
	if (!s->inuse)
	{
		sfxcache_t * newcache;

		// clear whole struct.
		memset(s, 0, sizeof(*s));
		// allocate cache.
		newsize = MAX_RAW_CACHE; //sizeof(sfxcache_t)

		if (newsize < sizeof(sfxcache_t))
			Sys_Error("MAX_RAW_CACHE too small %d", newsize);

		newcache = Cache_Alloc(&s->sfx.cache, newsize, "rawaudio");
		if (!newcache)
		{
			Com_DPrintf("Cache_Alloc failed\n");
			return;
		}

		s->inuse = true;
		s->id = sourceid;
//		strcpy(s->sfx.name, ""); // FIXME: probably we should put some specific tag name here?
		s->sfx.cache.data = newcache;
		newcache->format.speed = shm->format.speed;
		newcache->format.channels = channelsnum;
		newcache->format.width = width;
		newcache->loopstart = -1;
		newcache->total_length = 0;
		newcache = NULL;

//		Com_Printf("Added new raw stream\n");
	}

	// get current cache if any.
	currentcache = (sfxcache_t *)Cache_Check(&s->sfx.cache);
	if (!currentcache)
	{
		Com_DPrintf("Cache_Check failed\n");
		S_RawClearStream(s);
		return;
	}

	if (   currentcache->format.speed != shm->format.speed
		|| currentcache->format.channels != channelsnum
		|| currentcache->format.width != width)
	{
		currentcache->format.speed = shm->format.speed;
		currentcache->format.channels = channelsnum;
		currentcache->format.width = width;
//		newcache->loopstart = -1;
		currentcache->total_length = 0;
//		Com_Printf("Restarting raw stream\n");
	}

	speedfactor	= (double)speed/shm->format.speed;
	outsamples = samples/speedfactor;

	prepadl = 0x7fffffff;
	// FIXME: qqshka: I have no idea that spike is doing here, really. WTF is prepadl??? PITCHSHIFT ???
	// spike: make sure that we have a prepad.
	for (i = 0; i < total_channels; i++)
	{
		if (channels[i].sfx == &s->sfx)
		{
			if (prepadl > (channels[i].pos/*>>PITCHSHIFT*/))
				prepadl = (channels[i].pos/*>>PITCHSHIFT*/);
			break;
		}
	}

	if (prepadl == 0x7fffffff)
	{
		if (s_show.integer)
			Com_Printf("Wasn't playing\n");
		prepadl = 0;
		spare = 0;
		if (spare > shm->format.speed)
		{
			Com_DPrintf("Sacrificed raw sound stream\n");
			spare = 0;	//too far out. sacrifice it all
		}
	}
	else
	{
		if (prepadl < 0)
			prepadl = 0;
		spare = currentcache->total_length - prepadl;
		if (spare < 0)	//remaining samples since last time
			spare = 0;

		if (spare > shm->format.speed * 2) // more than 2 seconds of sound
		{
			Com_DPrintf("Sacrificed raw sound stream\n");
			spare = 0;	//too far out. sacrifice it all
		}
	}

	newsize = sizeof(sfxcache_t) + (spare + outsamples) * currentcache->format.channels * currentcache->format.width;
	if (newsize >= MAX_RAW_CACHE)
	{
		Com_DPrintf("Cache buffer overflowed\n");
		S_RawClearStream(s);
		return;
	}
	// move along spare/remaning samples in the begging of the buffer.
	memmove(currentcache->data, 
		currentcache->data + prepadl * currentcache->format.channels * currentcache->format.width,
							   spare * currentcache->format.channels * currentcache->format.width);

	currentcache->total_length = spare + outsamples;

	// resample.
	{
		extern cvar_t s_linearresample_stream;
		short *outpos = (short *)(currentcache->data + spare * currentcache->format.channels * currentcache->format.width);
		SND_ResampleStream(data,
			speed,
			width,
			channelsnum,
			samples,
			outpos,
			shm->format.speed,
			currentcache->format.width,
			currentcache->format.channels,
			s_linearresample_stream.integer
			);
	}

	currentcache->loopstart = -1;//currentcache->total_length;

	for (i = 0; i < total_channels; i++)
	{
		if (channels[i].sfx == &s->sfx)
		{
#if 0
			// FIXME: qqshka: hrm, should it be just like this all the time??? I think it should.
			channels[i].pos = 0;
			channels[i].end = paintedtime + currentcache->total_length;
#else
			channels[i].pos -= prepadl; // * channels[i].rate;
			channels[i].end += outsamples;
			channels[i].master_vol = (int) (s_raw_volume.value * 255); // this should changed volume on alredy playing sound.

			if (channels[i].end < paintedtime)
			{
				channels[i].pos = 0;
				channels[i].end = paintedtime + currentcache->total_length;
			}
#endif
			break;
		}
	}

	//this one wasn't playing, lets start it then.
	if (i == total_channels)
	{
//		Com_DPrintf("start sound\n");
		/*slight delay to try to avoid frame rate/etc stops/starts*/
//		S_StartSoundCard(si, -1, 0, &s->sfx, r_origin, 1, 32767, -shm->format.speed*0.02, 0);
		S_StartSound(SELF_SOUND, 0, &s->sfx, r_origin, s_raw_volume.value, 0);
	}
}
