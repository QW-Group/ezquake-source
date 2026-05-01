/*
 * Copyright (C) 2026 Oscar Linderholm <osm@recv.se>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <math.h>
#include "quakedef.h"
#include "client.h"
#include "demo_extension.h"
#include "demo_extension_voice.h"
#include "fs.h"
#include "qsound.h"
#include "sndfile.h"

#define DEMO_EXTENSION_VOICE_MAX_PLAYING_TRACKS RAW_SOURCE_DEMO_VOICE_COUNT
#define DEMO_EXTENSION_VOICE_BUFFER_SECONDS 0.25
#define DEMO_EXTENSION_VOICE_PREBUFFER_SECONDS 4.0
#define DEMO_EXTENSION_VOICE_TIME_JUMP_THRESHOLD 0.25

static int DemoExtensionVoice_SourceId(int index)
{
	return RAW_SOURCE_DEMO_VOICE_BASE + index;
}

typedef struct demo_extension_voice_memory_file_s {
	int filesize;
	int position;
	unsigned char *data;
} demo_extension_voice_memory_file_t;

typedef struct demo_extension_voice_track_s {
	qbool playing;
	qbool need_seek;
	char track[MAX_OSPATH];
	char name[MAX_SCOREBOARDNAME];
	char team[MAX_INFO_STRING];
	unsigned char *data;
	demo_extension_voice_memory_file_t memory_file;
	SF_VIRTUAL_IO sfvio;
	SF_INFO sfinfo;
	SNDFILE *sndfile;
	short *pcm;
	sf_count_t pcm_start;
	sf_count_t pcm_frames;
	sf_count_t pcm_capacity;
	sf_count_t pcm_total_frames;
	int pcm_rate;
	sf_count_t decoded_until;
	sf_count_t frames_fed;
	float volume;
	float offset;
} demo_extension_voice_track_t;

static cvar_t demo_voice_enable = {"demo_voice_enable", "1"};
static cvar_t demo_voice_team_filter = {"demo_voice_team_filter", "1"};

static demo_extension_voice_track_t
	demo_extension_voice_tracks[DEMO_EXTENSION_VOICE_MAX_PLAYING_TRACKS];
static int demo_extension_voice_track_count;
static double demo_extension_voice_last_time = -1.0;
static double demo_extension_voice_last_speed = -1.0;
static qbool demo_extension_voice_was_paused = true;

static qbool DemoExtensionVoice_TrackMatchesViewTeam(
	const demo_extension_voice_track_t *track);

static sf_count_t DemoExtensionVoice_VIOGetFilelen(void *user_data)
{
	demo_extension_voice_memory_file_t *file;

	file = user_data;
	return file->filesize;
}

static sf_count_t DemoExtensionVoice_VIOSeek(sf_count_t offset, int whence,
	void *user_data)
{
	demo_extension_voice_memory_file_t *file;

	file = user_data;
	if (whence == SEEK_CUR) {
		file->position += offset;
	}
	else if (whence == SEEK_SET) {
		file->position = offset;
	}
	else if (whence == SEEK_END) {
		file->position = file->filesize + offset;
	}

	if (file->position > file->filesize) {
		file->position = file->filesize;
	}
	else if (file->position < 0) {
		file->position = 0;
	}

	return file->position;
}

static sf_count_t DemoExtensionVoice_VIORead(void *ptr, sf_count_t count, void *user_data)
{
	sf_count_t available;
	demo_extension_voice_memory_file_t *file;

	file = user_data;
	available = min(count, file->filesize - file->position);
	if (available > 0) {
		memcpy(ptr, file->data + file->position, available);
		file->position += available;
	}
	return available;
}

static sf_count_t DemoExtensionVoice_VIOTell(void *user_data)
{
	demo_extension_voice_memory_file_t *file;

	file = user_data;
	return file->position;
}

static void DemoExtensionVoice_ClearTrack(demo_extension_voice_track_t *track, int index)
{
	if (track->playing) {
		S_RawAudio(DemoExtensionVoice_SourceId(index), NULL, 0, 0, 0, 0);
	}

	if (track->sndfile) {
		sf_close(track->sndfile);
	}

	if (track->pcm) {
		Q_free(track->pcm);
	}

	if (track->data) {
		Q_free(track->data);
	}

	memset(track, 0, sizeof(*track));
}

static void DemoExtensionVoice_ClearStreams(void)
{
	int i;

	for (i = 0; i < demo_extension_voice_track_count; ++i) {
		if (demo_extension_voice_tracks[i].playing) {
			S_RawAudio(DemoExtensionVoice_SourceId(i), NULL, 0, 0, 0, 0);
			demo_extension_voice_tracks[i].playing = false;
			demo_extension_voice_tracks[i].need_seek = true;
		}
	}
}

void DemoExtensionVoice_Unload(void)
{
	int i;

	for (i = 0; i < demo_extension_voice_track_count; ++i) {
		DemoExtensionVoice_ClearTrack(&demo_extension_voice_tracks[i], i);
	}

	demo_extension_voice_track_count = 0;
	demo_extension_voice_last_time = -1.0;
	demo_extension_voice_last_speed = -1.0;
	demo_extension_voice_was_paused = true;
}

static qbool DemoExtensionVoice_AllocatePrebuffer(
	demo_extension_voice_track_t *track, const char *path)
{
	int target_rate;
	target_rate = shw ? shw->khz : track->sfinfo.samplerate;

	if (track->sfinfo.frames <= 0 || track->sfinfo.samplerate <= 0 ||
		track->sfinfo.channels <= 0 || target_rate <= 0) {
		Com_Printf("demo_voice: unsupported length/rate in %s\n", path);
		return false;
	}

	track->pcm_capacity = ceil(DEMO_EXTENSION_VOICE_PREBUFFER_SECONDS * target_rate);
	track->pcm = Q_malloc_named(track->pcm_capacity * sizeof(short), path);
	if (!track->pcm) {
		Com_Printf("demo_voice: not enough memory to prebuffer %s\n", path);
		return false;
	}

	track->pcm_start = 0;
	track->pcm_frames = 0;
	track->pcm_total_frames = ceil(
		track->sfinfo.frames * (target_rate / (double)track->sfinfo.samplerate));
	track->pcm_rate = target_rate;
	return true;
}

static qbool DemoExtensionVoice_LoadTrackData(
	demo_extension_voice_track_t *track, const char *path,
	const char *name, const char *team, unsigned char *data, int filesize,
	float offset)
{
	memset(track, 0, sizeof(*track));
	track->data = data;
	track->memory_file.filesize = filesize;
	track->memory_file.position = 0;
	track->memory_file.data = track->data;
	track->sfvio.get_filelen = DemoExtensionVoice_VIOGetFilelen;
	track->sfvio.seek = DemoExtensionVoice_VIOSeek;
	track->sfvio.read = DemoExtensionVoice_VIORead;
	track->sfvio.tell = DemoExtensionVoice_VIOTell;
	track->sfinfo.format = 0;
	track->sndfile = sf_open_virtual(
		&track->sfvio, SFM_READ, &track->sfinfo, &track->memory_file);
	if (!track->sndfile) {
		Com_Printf("demo_voice: couldn't decode %s: %s\n", path, sf_strerror(NULL));
		Q_free(track->data);
		memset(track, 0, sizeof(*track));
		return false;
	}

	if (track->sfinfo.channels < 1 || track->sfinfo.channels > 2 ||
		track->sfinfo.samplerate <= 0) {
		Com_Printf("demo_voice: unsupported channel/rate in %s\n", path);
		DemoExtensionVoice_ClearTrack(track, 0);
		return false;
	}

	if (!DemoExtensionVoice_AllocatePrebuffer(track, path)) {
		DemoExtensionVoice_ClearTrack(track, 0);
		return false;
	}

	strlcpy(track->track, path, sizeof(track->track));
	strlcpy(track->name, name, sizeof(track->name));
	strlcpy(track->team, team, sizeof(track->team));
	track->volume = 1.0f;
	track->offset = offset;
	track->need_seek = true;
	return true;
}

static void DemoExtensionVoice_PrintLoadedTrack(const char *path,
	const char *name, const char *team, float offset)
{
	Com_Printf("demo_voice: loaded %s", path);
	if (offset) {
		Com_Printf(" offset %.3f", offset);
	}
	if (name[0]) {
		Com_Printf(" name \"%s\"", name);
	}
	if (team[0]) {
		Com_Printf(" team \"%s\"", team);
	}
	Com_Printf("\n");
}

static qbool DemoExtensionVoice_IsLoaded(int index)
{
	return demo_extension_voice_tracks[index].sndfile != NULL;
}

static int DemoExtensionVoice_CountLoadedTracks(void)
{
	int i;
	int count = 0;

	for (i = 0; i < demo_extension_voice_track_count; ++i) {
		if (DemoExtensionVoice_IsLoaded(i)) {
			++count;
		}
	}

	return count;
}

static int DemoExtensionVoice_FindFreeTrack(void)
{
	int i;

	for (i = 0; i < demo_extension_voice_track_count; ++i) {
		if (!DemoExtensionVoice_IsLoaded(i)) {
			return i;
		}
	}

	return demo_extension_voice_track_count <
		DEMO_EXTENSION_VOICE_MAX_PLAYING_TRACKS ?
		demo_extension_voice_track_count : -1;
}

static void DemoExtensionVoice_TrimTrackCount(void)
{
	while (demo_extension_voice_track_count > 0 &&
		!DemoExtensionVoice_IsLoaded(demo_extension_voice_track_count - 1)) {
		--demo_extension_voice_track_count;
	}
}

static int DemoExtensionVoice_FindTrack(const char *target);

static void DemoExtensionVoice_Load_f(void)
{
	byte *data;
	int data_len;
	int i;
	int slot;
	float offset = 0;
	const char *path;
	char name[MAX_SCOREBOARDNAME] = "";
	char team[MAX_INFO_STRING] = "";

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <file> [offset <seconds>] [name <name>] "
			"[team <team>]\n", Cmd_Argv(0));
		return;
	}
	slot = DemoExtensionVoice_FindFreeTrack();
	if (slot < 0) {
		Com_Printf("demo_voice: only %d tracks can be played\n",
			DEMO_EXTENSION_VOICE_MAX_PLAYING_TRACKS);
		return;
	}

	path = Cmd_Argv(1);
	for (i = 2; i < Cmd_Argc(); ++i) {
		const char *key = Cmd_Argv(i);
		const char *value;

		if (i + 1 >= Cmd_Argc()) {
			Com_Printf("demo_voice: missing value for load option %s\n", key);
			return;
		}
		value = Cmd_Argv(++i);

		if (!strcasecmp(key, "offset")) {
			offset = Q_atof(value);
		}
		else if (!strcasecmp(key, "name")) {
			strlcpy(name, value, sizeof(name));
		}
		else if (!strcasecmp(key, "team")) {
			strlcpy(team, value, sizeof(team));
		}
		else {
			Com_Printf("demo_voice: unknown load option %s\n", key);
		}
	}

	if (cbuf_current == &cbuf_demo_extension) {
		if (!DemoExtension_LoadFile(path, &data, &data_len)) {
			Com_Printf("demo_voice: couldn't load %s from demo extension\n", path);
			return;
		}
	}
	else {
		data = FS_LoadHeapFile(path, &data_len);
		if (!data) {
			Com_Printf("demo_voice: couldn't load %s\n", path);
			return;
		}
	}

	if (DemoExtensionVoice_LoadTrackData(
			&demo_extension_voice_tracks[slot],
			path, name, team, data, data_len, offset)) {
		if (slot == demo_extension_voice_track_count) {
			++demo_extension_voice_track_count;
		}
		DemoExtensionVoice_PrintLoadedTrack(path, name, team, offset);
	}
}

static void DemoExtensionVoice_Unload_f(void)
{
	int index;
	char track[MAX_OSPATH];

	if (Cmd_Argc() > 2) {
		Com_Printf("Usage: %s [track]\n", Cmd_Argv(0));
		return;
	}

	if (!DemoExtensionVoice_CountLoadedTracks()) {
		Com_Printf("demo_voice: no tracks loaded\n");
		return;
	}

	if (Cmd_Argc() == 1 || !strcasecmp(Cmd_Argv(1), "all")) {
		DemoExtensionVoice_Unload();
		Com_Printf("demo_voice: unloaded\n");
		return;
	}

	index = DemoExtensionVoice_FindTrack(Cmd_Argv(1));
	if (index < 0) {
		Com_Printf("demo_voice: couldn't find track %s\n", Cmd_Argv(1));
		return;
	}

	strlcpy(track, demo_extension_voice_tracks[index].track, sizeof(track));
	DemoExtensionVoice_ClearTrack(&demo_extension_voice_tracks[index], index);
	DemoExtensionVoice_TrimTrackCount();
	Com_Printf("demo_voice: unloaded %s\n", track);
}

static int DemoExtensionVoice_FindTrack(const char *target)
{
	int i;
	qbool numeric = true;

	for (i = 0; target[i]; ++i) {
		if (!isdigit((unsigned char)target[i])) {
			numeric = false;
			break;
		}
	}

	if (numeric && target[0]) {
		i = Q_atoi(target) - 1;
		if (i >= 0 && i < demo_extension_voice_track_count &&
			DemoExtensionVoice_IsLoaded(i)) {
			return i;
		}
		return -1;
	}

	for (i = 0; i < demo_extension_voice_track_count; ++i) {
		const char *label = demo_extension_voice_tracks[i].track;
		const char *basename = COM_SkipPath(label);

		if (!DemoExtensionVoice_IsLoaded(i)) {
			continue;
		}

		if (!strcasecmp(target, label) || !strcasecmp(target, basename) ||
			strstri(label, target) || strstri(basename, target)) {
			return i;
		}
	}

	return -1;
}

static void DemoExtensionVoice_PrintTrackVolume(int index)
{
	Com_Printf("demo_voice: track %d volume %.2f (%s)\n",
		index + 1,
		demo_extension_voice_tracks[index].volume,
		demo_extension_voice_tracks[index].track);
}

static void DemoExtensionVoice_Volume_f(void)
{
	int index;
	float volume;
	const char *target;

	if (Cmd_Argc() < 2 || Cmd_Argc() > 3) {
		Com_Printf("Usage: %s <track|all> [volume]\n", Cmd_Argv(0));
		return;
	}

	if (!DemoExtensionVoice_CountLoadedTracks()) {
		Com_Printf("demo_voice: no tracks loaded\n");
		return;
	}

	target = Cmd_Argv(1);
	if (!strcasecmp(target, "all")) {
		int i;

		if (Cmd_Argc() == 3) {
			volume = bound(0, Q_atof(Cmd_Argv(2)), 4);
			for (i = 0; i < demo_extension_voice_track_count; ++i) {
				if (!DemoExtensionVoice_IsLoaded(i)) {
					continue;
				}
				demo_extension_voice_tracks[i].volume = volume;
			}
		}

		for (i = 0; i < demo_extension_voice_track_count; ++i) {
			if (!DemoExtensionVoice_IsLoaded(i)) {
				continue;
			}
			DemoExtensionVoice_PrintTrackVolume(i);
		}
		return;
	}

	index = DemoExtensionVoice_FindTrack(target);
	if (index < 0) {
		Com_Printf("demo_voice: couldn't find track %s\n", target);
		return;
	}

	if (Cmd_Argc() == 3) {
		volume = bound(0, Q_atof(Cmd_Argv(2)), 4);
		demo_extension_voice_tracks[index].volume = volume;
	}

	DemoExtensionVoice_PrintTrackVolume(index);
}

static void DemoExtensionVoice_Info_f(void)
{
	int i;
	double decoded;
	double fed;
	int loaded_count = DemoExtensionVoice_CountLoadedTracks();

	Com_Printf("demo_voice: %d track%s, %s\n",
		loaded_count,
		loaded_count == 1 ? "" : "s",
		demo_voice_enable.integer ? "enabled" : "disabled");
	Com_Printf("demo_voice: team filter %s\n",
		demo_voice_team_filter.integer ? "enabled" : "disabled");

	for (i = 0; i < demo_extension_voice_track_count; ++i) {
		if (!DemoExtensionVoice_IsLoaded(i)) {
			continue;
		}

		decoded = demo_extension_voice_tracks[i].pcm_rate > 0 ?
			demo_extension_voice_tracks[i].decoded_until /
				(double)demo_extension_voice_tracks[i].pcm_rate :
			0;
		fed = demo_extension_voice_tracks[i].pcm_rate > 0 ?
			demo_extension_voice_tracks[i].frames_fed /
				(double)demo_extension_voice_tracks[i].pcm_rate :
			0;

		Com_Printf("  %d: %s (%d Hz, %d channel%s, volume %.2f, offset %.3f)\n",
			i + 1,
			demo_extension_voice_tracks[i].track,
			demo_extension_voice_tracks[i].sfinfo.samplerate,
			demo_extension_voice_tracks[i].sfinfo.channels,
			demo_extension_voice_tracks[i].sfinfo.channels == 1 ? "" : "s",
			demo_extension_voice_tracks[i].volume,
			demo_extension_voice_tracks[i].offset);
		Com_Printf("     decoded %.2fs, fed %.2fs, %s\n",
			decoded,
			fed,
			demo_extension_voice_tracks[i].playing ? "playing" :
			DemoExtensionVoice_TrackMatchesViewTeam(&demo_extension_voice_tracks[i]) ?
				"not playing" : "filtered");
		if (demo_extension_voice_tracks[i].name[0] || demo_extension_voice_tracks[i].team[0]) {
			Com_Printf("     name \"%s\", team \"%s\"\n",
				demo_extension_voice_tracks[i].name,
				demo_extension_voice_tracks[i].team);
		}
	}
}

void DemoExtensionVoice_StartDemo(void)
{
	int i;

	demo_extension_voice_last_time = -1.0;
	demo_extension_voice_last_speed = -1.0;
	demo_extension_voice_was_paused = true;
	for (i = 0; i < demo_extension_voice_track_count; ++i) {
		if (!DemoExtensionVoice_IsLoaded(i)) {
			continue;
		}
		demo_extension_voice_tracks[i].need_seek = true;
		demo_extension_voice_tracks[i].playing = false;
	}
}

void DemoExtensionVoice_StopDemo(void)
{
	DemoExtensionVoice_ClearStreams();
	demo_extension_voice_last_time = -1.0;
	demo_extension_voice_last_speed = -1.0;
	demo_extension_voice_was_paused = true;
}

static double DemoExtensionVoice_TrackTime(
	const demo_extension_voice_track_t *track, double time)
{
	return time + track->offset;
}

static qbool DemoExtensionVoice_TrackMatchesViewTeam(
	const demo_extension_voice_track_t *track)
{
	player_info_t *view_player;
	char track_team[MAX_INFO_STRING];
	char view_team[MAX_INFO_STRING];

	if (!demo_voice_team_filter.integer || !track->team[0]) {
		return true;
	}
	if (cl.viewplayernum < 0 || cl.viewplayernum >= MAX_CLIENTS) {
		return false;
	}

	view_player = &cl.players[cl.viewplayernum];
	if (!view_player->team[0]) {
		return false;
	}

	strlcpy(track_team, track->team, sizeof(track_team));
	strlcpy(view_team, view_player->team, sizeof(view_team));
	Q_normalizetext(track_team);
	Q_normalizetext(view_team);

	return !strcasecmp(track_team, view_team);
}

static void DemoExtensionVoice_ClearTrackStream(
	demo_extension_voice_track_t *track, int index)
{
	if (track->playing) {
		S_RawAudio(DemoExtensionVoice_SourceId(index), NULL, 0, 0, 0, 0);
		track->playing = false;
	}
	track->need_seek = true;
}

static void DemoExtensionVoice_SeekTrack(demo_extension_voice_track_t *track, double time)
{
	sf_count_t frame;

	frame = DemoExtensionVoice_TrackTime(track, time) * track->pcm_rate;
	if (frame < 0) {
		frame = 0;
	}
	if (track->pcm_total_frames > 0 && frame > track->pcm_total_frames) {
		frame = track->pcm_total_frames;
	}

	track->decoded_until = frame;
	track->need_seek = false;
}

static void DemoExtensionVoice_ScaleSamples(
	const demo_extension_voice_track_t *track, short *samples,
	int count, int active_count)
{
	int i;
	float volume;

	volume = bound(0, track->volume, 4);
	if (active_count > 1) {
		volume /= active_count;
	}
	if (volume == 1.0f) {
		return;
	}

	for (i = 0; i < count; ++i) {
		int value;

		value = samples[i] * volume;
		samples[i] = bound(-32768, value, 32767);
	}
}

static void DemoExtensionVoice_ResetTrackPlayback(
	demo_extension_voice_track_t *track, int index, double time)
{
	if (track->playing) {
		S_RawAudio(DemoExtensionVoice_SourceId(index), NULL, 0, 0, 0, 0);
		track->playing = false;
	}

	DemoExtensionVoice_SeekTrack(track, time);
	track->pcm_start = track->decoded_until;
	track->pcm_frames = 0;
	sf_seek(track->sndfile,
		(track->decoded_until * track->sfinfo.samplerate) / track->pcm_rate,
		SEEK_SET);
}

static void DemoExtensionVoice_GetFrameRange(
	const demo_extension_voice_track_t *track, double time,
	sf_count_t *target_frame, sf_count_t *prebuffer_frame)
{
	double track_time;

	track_time = DemoExtensionVoice_TrackTime(track, time);
	*target_frame =
		(track_time + DEMO_EXTENSION_VOICE_BUFFER_SECONDS) * track->pcm_rate;
	if (*target_frame < 0) {
		*target_frame = 0;
	}

	*prebuffer_frame =
		(track_time + DEMO_EXTENSION_VOICE_PREBUFFER_SECONDS) * track->pcm_rate;
	if (*prebuffer_frame < *target_frame) {
		*prebuffer_frame = *target_frame;
	}

	if (track->sfinfo.frames > 0) {
		sf_count_t max_frame = ceil(
			track->sfinfo.frames *
				(track->pcm_rate / (double)track->sfinfo.samplerate));
		if (*target_frame > max_frame) {
			*target_frame = max_frame;
		}
		if (*prebuffer_frame > max_frame) {
			*prebuffer_frame = max_frame;
		}
	}
}

static int DemoExtensionVoice_MixSourceFrame(
	const demo_extension_voice_track_t *track, short *source,
	sf_count_t source_index, sf_count_t source_frames_read)
{
	sf_count_t source_offset;

	if (source_index >= source_frames_read) {
		source_index = source_frames_read - 1;
	}

	if (track->sfinfo.channels == 1) {
		return source[source_index];
	}

	source_offset = source_index * track->sfinfo.channels;
	return (source[source_offset] + source[source_offset + 1]) / 2;
}

static sf_count_t DemoExtensionVoice_ResampleFrames(
	const demo_extension_voice_track_t *track, short *source,
	sf_count_t source_frames_read, double source_base,
	sf_count_t source_start, short *pcm, sf_count_t output_frames)
{
	sf_count_t i;
	double ratio = track->sfinfo.samplerate / (double)track->pcm_rate;

	for (i = 0; i < output_frames; ++i) {
		double source_pos = source_base + i * ratio - source_start;
		sf_count_t source_index = floor(source_pos);
		double frac = source_pos - source_index;
		int s1, s2;

		if (source_index >= source_frames_read) {
			return i;
		}

		s1 = DemoExtensionVoice_MixSourceFrame(
			track, source, source_index, source_frames_read);
		s2 = DemoExtensionVoice_MixSourceFrame(
			track, source, source_index + 1, source_frames_read);
		pcm[i] = bound(-32768, (int)(s1 + (s2 - s1) * frac), 32767);
	}

	return output_frames;
}

static void DemoExtensionVoice_FillPrebuffer(
	demo_extension_voice_track_t *track, sf_count_t prebuffer_frame)
{
	short source[4096 * 2];
	short pcm[4096];
	double ratio;

	while (track->pcm_start + track->pcm_frames < prebuffer_frame &&
		track->pcm_frames < track->pcm_capacity) {
		sf_count_t output_index;
		sf_count_t output_room;
		sf_count_t output_frames;
		sf_count_t source_start;
		sf_count_t source_frames_needed;
		sf_count_t source_frames_read;
		double source_base;

		output_index = (track->pcm_start + track->pcm_frames) % track->pcm_capacity;
		output_room = min(track->pcm_capacity - output_index,
			track->pcm_capacity - track->pcm_frames);
		output_frames = min(output_room, sizeof(pcm) / sizeof(pcm[0]));
		output_frames = min(output_frames,
			prebuffer_frame - (track->pcm_start + track->pcm_frames));
		if (output_frames <= 0) {
			break;
		}

		ratio = track->sfinfo.samplerate / (double)track->pcm_rate;
		source_base = (track->pcm_start + track->pcm_frames) * ratio;
		source_start = floor(source_base);
		source_frames_needed = ceil(output_frames * ratio + 2);
		source_frames_needed = min(source_frames_needed,
			sizeof(source) / sizeof(source[0]) / track->sfinfo.channels);
		sf_seek(track->sndfile, source_start, SEEK_SET);
		source_frames_read = sf_readf_short(track->sndfile, source, source_frames_needed);
		if (source_frames_read <= 0) {
			break;
		}

		output_frames = DemoExtensionVoice_ResampleFrames(
			track, source, source_frames_read, source_base, source_start,
			pcm, output_frames);
		if (output_frames <= 0) {
			break;
		}

		memcpy(track->pcm + output_index, pcm, output_frames * sizeof(short));
		track->pcm_frames += output_frames;
	}
}

static void DemoExtensionVoice_FeedBufferedAudio(
	demo_extension_voice_track_t *track, int index, sf_count_t target_frame,
	double demo_speed, int active_count)
{
	short pcm[4096];

	while (track->decoded_until < target_frame) {
		sf_count_t read_index;
		sf_count_t frames_needed;
		sf_count_t frames_to_read;

		frames_needed = target_frame - track->decoded_until;
		frames_to_read = min(frames_needed, sizeof(pcm) / sizeof(pcm[0]));
		frames_to_read = min(frames_to_read,
			track->pcm_start + track->pcm_frames - track->decoded_until);
		if (frames_to_read <= 0) {
			break;
		}

		read_index = track->decoded_until % track->pcm_capacity;
		frames_to_read = min(frames_to_read, track->pcm_capacity - read_index);
		memcpy(pcm, track->pcm + read_index, frames_to_read * sizeof(short));
		DemoExtensionVoice_ScaleSamples(track, pcm, frames_to_read, active_count);
		S_RawAudio(DemoExtensionVoice_SourceId(index), (byte *)pcm,
			track->pcm_rate * demo_speed, frames_to_read, 1, 2);
		track->decoded_until += frames_to_read;
		track->frames_fed += frames_to_read;
		track->playing = true;

		if (track->decoded_until > track->pcm_start) {
			sf_count_t consumed = track->decoded_until - track->pcm_start;
			track->pcm_start += consumed;
			track->pcm_frames -= min(consumed, track->pcm_frames);
		}
	}
}

static void DemoExtensionVoice_UpdateTrack(
	demo_extension_voice_track_t *track, int index, double time,
	double demo_speed, qbool jumped, int active_count)
{
	sf_count_t target_frame;
	sf_count_t prebuffer_frame;

	if (track->need_seek || jumped) {
		DemoExtensionVoice_ResetTrackPlayback(track, index, time);
	}

	DemoExtensionVoice_GetFrameRange(track, time, &target_frame, &prebuffer_frame);
	DemoExtensionVoice_FillPrebuffer(track, prebuffer_frame);
	DemoExtensionVoice_FeedBufferedAudio(
		track, index, target_frame, demo_speed, active_count);
}

void DemoExtensionVoice_UpdateFrame(void)
{
	int i;
	double time;
	double delta;
	double demo_speed;
	qbool paused;
	qbool jumped;
	qbool speed_changed;
	int active_count = 0;

	if (!demo_extension_voice_track_count) {
		return;
	}

	paused = !cls.demoplayback || cls.state != ca_active ||
		cls.mvdplayback == QTV_PLAYBACK || demostarttime < 0 ||
		!demo_voice_enable.integer || cls.demoseeking || Demo_GetSpeed() == 0.0;

	if (paused) {
		DemoExtensionVoice_ClearStreams();
		demo_extension_voice_was_paused = true;
		return;
	}

	demo_speed = Demo_GetSpeed();
	time = cls.demotime - demostarttime;
	if (time < 0) {
		time = 0;
	}

	delta = demo_extension_voice_last_time < 0 ?
		0 : fabs(time - demo_extension_voice_last_time);
	speed_changed = demo_extension_voice_last_speed >= 0 &&
		fabs(demo_speed - demo_extension_voice_last_speed) > 0.001;
	jumped = demo_extension_voice_was_paused || speed_changed ||
		(demo_extension_voice_last_time >= 0 &&
			delta > DEMO_EXTENSION_VOICE_TIME_JUMP_THRESHOLD);

	for (i = 0; i < demo_extension_voice_track_count; ++i) {
		if (!DemoExtensionVoice_IsLoaded(i)) {
			continue;
		}
		if (DemoExtensionVoice_TrackMatchesViewTeam(&demo_extension_voice_tracks[i])) {
			++active_count;
		}
		else {
			DemoExtensionVoice_ClearTrackStream(&demo_extension_voice_tracks[i], i);
		}
	}

	for (i = 0; i < demo_extension_voice_track_count; ++i) {
		if (!DemoExtensionVoice_IsLoaded(i)) {
			continue;
		}
		if (DemoExtensionVoice_TrackMatchesViewTeam(&demo_extension_voice_tracks[i])) {
			DemoExtensionVoice_UpdateTrack(&demo_extension_voice_tracks[i], i, time, demo_speed,
				jumped, active_count);
		}
	}

	demo_extension_voice_last_time = time;
	demo_extension_voice_last_speed = demo_speed;
	demo_extension_voice_was_paused = false;
}

void DemoExtensionVoice_Init(void)
{
	Cmd_AddCommand("demo_voice_load", DemoExtensionVoice_Load_f);
	Cmd_AddCommand("demo_voice_unload", DemoExtensionVoice_Unload_f);
	Cmd_AddCommand("demo_voice_info", DemoExtensionVoice_Info_f);
	Cmd_AddCommand("demo_voice_volume", DemoExtensionVoice_Volume_f);

	Cvar_SetCurrentGroup(CVAR_GROUP_DEMO);
	Cvar_Register(&demo_voice_enable);
	Cvar_Register(&demo_voice_team_filter);
	Cvar_ResetCurrentGroup();
}
