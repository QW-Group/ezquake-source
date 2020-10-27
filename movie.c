/*

Copyright (C) 2001-2002       A Nourai

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

	$Id: movie.c,v 1.26 2007-10-11 06:46:12 borisu Exp $
*/

#include "quakedef.h"
#include "utils.h"
#include "qsound.h"
#include "image.h"
#ifdef _WIN32
#include "movie_avi.h"	//joe: capturing to avi
#include <windows.h>
#else
	#include <time.h>
#endif

static void OnChange_movie_dir(cvar_t *var, char *string, qbool *cancel);
static void WAVCaptureStop (void);
static void WAVCaptureStart (void);
void SCR_Movieshot (char *);	//joe: capturing to avi

//joe: capturing audio
// Variables for buffering audio
static short capture_audio_samples[44100];	// big enough buffer for 1fps at 44100Hz
static int captured_audio_samples;
static qbool frame_has_sound = false;

#ifdef _WIN32
void OnChange_movie_codec(cvar_t *var, char *string, qbool *cancel);
LONG Movie_CurrentLength(void);

static char movie_avi_filename[MAX_OSPATH]; // Stores the user's requested filename
static void Movie_Start_AVI_Capture(void);
static int avi_number = 0;

//joe: capturing to avi
static qbool movie_is_avi = false;
qbool movie_avi_loaded = false, movie_acm_loaded = false;
static char avipath[256];
static FILE *avifile = NULL;
cvar_t   movie_codec      = {"demo_capture_codec", "0", 0, OnChange_movie_codec };	// Capturing to avi
cvar_t   movie_mp3        = {"demo_capture_mp3", "0"};
cvar_t   movie_mp3_kbps   = {"demo_capture_mp3_kbps", "128"};
cvar_t   movie_vid_maxlen = {"demo_capture_vid_maxlen", "0"};
#endif

cvar_t          movie_fps                = {"demo_capture_fps", "30.0"};
static cvar_t   movie_dir                = {"demo_capture_dir",  "capture", 0, OnChange_movie_dir};
cvar_t          movie_steadycam          = {"demo_capture_steadycam", "0"};
static cvar_t   movie_background_threads = {"demo_capture_background_threads", "0"};

extern cvar_t scr_sshot_type;

static unsigned char aviSoundBuffer[4096]; // Static buffer for mixing

static double movie_real_start_time;
static volatile qbool movie_is_capturing = false;
static double movie_start_time;
static double movie_len;
static int movie_frame_count;
static char image_ext[4];
static qbool capturing_apng;
static int apng_expected_frames;


#ifdef _WIN32
	static SYSTEMTIME movie_start_date;
#else
	struct tm movie_start_date;
#endif

qbool Movie_IsCapturing(void)
{
	return cls.demoplayback && !cls.timedemo && movie_is_capturing;
}

double Movie_Frametime(void)
{
	double time = (double)(movie_fps.value > 0 ? 1.0 / movie_fps.value : 1 / 30.0);

	return bound(1.0 / 1000, time, 1.0);;
}

double Movie_InputFrametime(void)
{
	if (movie_steadycam.value) {
		return movie_fps.value > 0 ? 1.0 / movie_fps.value : 1 / 30.0;
	}

	return cls.trueframetime;
}

static void Movie_Start(double _time) 
{
	extern cvar_t scr_sshot_format;

	#ifndef _WIN32
	time_t t;
	t = time(NULL);
	localtime_r(&t, &movie_start_date);
	#else
	GetLocalTime(&movie_start_date);
	#endif
	#ifdef _WIN32
	movie_is_avi = !!avifile; //joe: capturing to avi
	#endif
	movie_len = _time;
	movie_start_time = cls.realtime;

	movie_frame_count = 0;

	#ifdef _WIN32
	if (movie_is_avi)	//joe: capturing to avi
	{
		movie_is_capturing = Capture_Open (avipath);
	}
	else
	#endif
	{
		// DEFAULT_SSHOT_FORMAT
		if (!strcmp(scr_sshot_format.string, "tga")
		 || !strcmp(scr_sshot_format.string, "jpeg")
		 || !strcmp(scr_sshot_format.string, "jpg")
		 || !strcmp(scr_sshot_format.string, "png"))
		{
			strlcpy(image_ext, scr_sshot_format.string, sizeof(image_ext));		
		}
		else if (!strcmp(scr_sshot_format.string, "apng")) {
			strlcpy(image_ext, "png", sizeof(image_ext));
		}
		else
		{
			strlcpy (image_ext, "tga", sizeof (image_ext));
		}
		movie_is_capturing = true;
		WAVCaptureStart ();
	}
	movie_real_start_time = Sys_DoubleTime ();
}

void Movie_Stop(qbool restarting)
{
	if (Movie_AnimatedPNG()) {
		Image_CloseAPNG();
	}
#ifdef _WIN32
	if (movie_is_avi) { //joe: capturing to avi
		Capture_Close ();
		fclose (avifile);
		avifile = NULL;
	}
	if (!restarting) {
		S_StopAllSounds();
		Movie_BackgroundShutdown();

		Com_Printf("Captured %d frames (%.2fs).\n", movie_frame_count, (float) (cls.realtime - movie_start_time));
		Com_Printf("  Time: %5.1f seconds\n", Sys_DoubleTime() - movie_real_start_time);
	}
#endif
	WAVCaptureStop ();
	movie_is_capturing = restarting;
}

void Movie_Demo_Capture_f(void)
{
	int argc;
	double duration;
	char *error;
	extern cvar_t scr_sshot_format;

#ifdef _WIN32
	error = va("Usage: %s (\"start\" time [avifile]) | \"stop\"\n", Cmd_Argv(0));
	if ((argc = Cmd_Argc()) != 2 && argc != 3 && argc != 4) {
#else
	error = va("Usage: %s (\"start\" time) | \"stop\"\n", Cmd_Argv(0));
	if ((argc = Cmd_Argc()) != 2 && argc != 3) {
#endif
		Com_Printf(error);
		return;
	}
	if (argc == 2) {
		if (strncasecmp("stop", Cmd_Argv(1), 4))
			Com_Printf(error);
		else if (Movie_IsCapturing()) 
			Movie_Stop(false);
		else
			Com_Printf("%s : Not capturing\n", Cmd_Argv(0));
		return;
	}
	if (strncasecmp("start", Cmd_Argv(1), 5)) {
		Com_Printf(error);
		return;
	} else if (Movie_IsCapturing()) {
		Com_Printf("%s : Already capturing\n", Cmd_Argv(0));
		return;
	}
	if (!cls.demoplayback || cls.timedemo) {
		Com_Printf("%s : Must be playing a demo to capture\n", Cmd_Argv(0));
		return;
	}
	if ((duration = Q_atof(Cmd_Argv(2))) <= 0) {
		Com_Printf("%s : Time argument must be positive\n", Cmd_Argv(0));
		return;
	}
	capturing_apng = false;
#ifdef _WIN32
	//joe: capturing to avi
	if (argc == 4) {
		avi_number = 0;

		strlcpy(movie_avi_filename, Cmd_Argv(3), sizeof(movie_avi_filename)-10);		// Store user's requested filename
		if (!movie_avi_loaded) {
			Com_Printf_State (PRINT_FAIL, "Avi capturing not initialized\n");
			return;
		}

		Movie_Start_AVI_Capture();
	}
	else
#endif
	if (!strcasecmp(scr_sshot_format.string, "apng")) {
		char fname[MAX_OSPATH];
		extern cvar_t image_png_compression_level;
		extern int glwidth, glheight;
#ifndef _WIN32
		time_t t;
		t = time(NULL);
		localtime_r(&t, &movie_start_date);

		snprintf(fname, sizeof(fname), "%s/capture_%02d-%02d-%04d_%02d-%02d-%02d/capture.png",
			movie_dir.string, movie_start_date.tm_mday, movie_start_date.tm_mon, movie_start_date.tm_year,
			movie_start_date.tm_hour, movie_start_date.tm_min, movie_start_date.tm_sec);
#else
		GetLocalTime(&movie_start_date);

		snprintf(fname, sizeof(fname), "%s/capture_%02d-%02d-%04d_%02d-%02d-%02d/capture.png",
			movie_dir.string, movie_start_date.wDay, movie_start_date.wMonth, movie_start_date.wYear,
			movie_start_date.wHour, movie_start_date.wMinute, movie_start_date.wSecond);
#endif

		apng_expected_frames = duration * movie_fps.integer;
		capturing_apng = Image_OpenAPNG(fname, image_png_compression_level.integer, glwidth, glheight, apng_expected_frames);

		if (!capturing_apng) {
			Movie_BackgroundInitialise();
		}
	}
	else {
		Movie_BackgroundInitialise();
	}
	Movie_Start(duration);
}

#ifdef _WIN32
static void Movie_Start_AVI_Capture(void)
{
	++avi_number;

	// If we're going to break up the movie, append number
	char aviname[MAX_OSPATH];
	if (avi_number > 1) {
		snprintf (aviname, sizeof (aviname), "%s-%03d", movie_avi_filename, avi_number);
	}
	else {
		strlcpy (aviname, movie_avi_filename, sizeof (aviname));
	}

	if (!(Util_Is_Valid_Filename(aviname))) {
		Com_Printf(Util_Invalid_Filename_Msg(aviname));
		return;
	}
	COM_ForceExtensionEx (aviname, ".avi", sizeof (aviname));
	snprintf (avipath, sizeof(avipath), "%s/%s/%s", com_basedir, movie_dir.string, aviname);
	if (!(avifile = fopen(avipath, "wb"))) {
		FS_CreatePath (avipath);
		if (!(avifile = fopen(avipath, "wb"))) {
			Com_Printf("Error: Couldn't open %s\n", aviname);
			return;
		}
	}
}
#endif

void Movie_Init(void) {
	Cvar_SetCurrentGroup(CVAR_GROUP_DEMO);
	Cvar_Register(&movie_fps);
	Cvar_Register(&movie_dir);
	Cvar_Register(&movie_background_threads);
	Cvar_Register(&movie_steadycam);

	Cvar_ResetCurrentGroup();

	Cmd_AddCommand("demo_capture", Movie_Demo_Capture_f);

#ifdef _WIN32
	Capture_InitAVI ();		//joe: capturing to avi
	if (!movie_avi_loaded)
		return;

	captured_audio_samples = 0;
	Cvar_SetCurrentGroup(CVAR_GROUP_DEMO);
	Cvar_Register(&movie_codec);
	Cvar_Register(&movie_vid_maxlen);

	Cvar_ResetCurrentGroup();

	Capture_InitACM ();
	if (!movie_acm_loaded)
		return;

	Cvar_SetCurrentGroup(CVAR_GROUP_DEMO);
	Cvar_Register(&movie_mp3);
	Cvar_Register(&movie_mp3_kbps);

	Cvar_ResetCurrentGroup();
#endif
}

void Movie_StartFrame(void) 
{
	if (Cmd_FindAlias("f_captureframe")) {
		Cbuf_AddTextEx (&cbuf_main, "f_captureframe\n");
	}
}

void Movie_FinishFrame(void) 
{
	char fname[128];
	if (!Movie_IsCapturing())
		return;

	#ifdef _WIN32
	if (!movie_is_avi) 
	{
		snprintf(fname, sizeof(fname), "%s/capture_%02d-%02d-%04d_%02d-%02d-%02d/shot-%06d.%s",
			movie_dir.string, movie_start_date.wDay, movie_start_date.wMonth, movie_start_date.wYear,
			movie_start_date.wHour,	movie_start_date.wMinute, movie_start_date.wSecond, movie_frame_count, image_ext);

		con_suppress = true;
	}
	else 
	{
		fname[0] = '\0';
		// Split up if we're over the time limit for each segment
		if (movie_vid_maxlen.integer && Movie_CurrentLength() >= movie_vid_maxlen.value * 1024 * 1024) 
		{
			double original_start_time = movie_start_time;

			// Close existing, and start again
			Movie_Stop(true);
			Movie_Start_AVI_Capture();
			Movie_Start(movie_len);

			// keep track of original start time so we know when to stop for good
			movie_start_time = original_start_time;
		}
	}
	#else
	snprintf(fname, sizeof(fname), "%s/capture_%02d-%02d-%04d_%02d-%02d-%02d/shot-%06d.%s",
		movie_dir.string, movie_start_date.tm_mday, 1 + movie_start_date.tm_mon, 1900 + movie_start_date.tm_year,
		movie_start_date.tm_hour, movie_start_date.tm_min, movie_start_date.tm_sec, movie_frame_count, image_ext);

	con_suppress = true;
	#endif // _WIN32

	SCR_Movieshot (fname);
	movie_frame_count++;

#ifdef _WIN32
	if (!movie_is_avi)
#endif
	{
		con_suppress = false;
	}

	if (Movie_AnimatedPNG()) {
		if (movie_frame_count >= apng_expected_frames) {
			Movie_Stop(false);
		}
	}
	else if (cls.realtime >= movie_start_time + movie_len) {
		Movie_Stop (false);
	}
}

qbool Movie_IsCapturingAVI(void) {
#ifdef _WIN32
	return movie_is_avi && Movie_IsCapturing();
#else
	return false;
#endif
}

static vfsfile_t *wav_output = NULL;
static int wav_sample_count = 0;

static void WAVCaptureStart (void)
{
	// WAV file format details taken from http://soundfile.sapp.org/doc/WaveFormat/
	char fname[MAX_PATH];

	int chunkSize = 0;  // We write 0 to start with, need to replace once we know number of samples
	int fmtBlockSize = LittleLong (16);
	short fmtFormat = LittleShort (1); // PCM, uncompressed
	short fmtNumberChannels = LittleShort (shw->numchannels);
	int fmtSampleRate = LittleLong (shw->khz);
	int fmtBitsPerSample = 16;
	int fmtByteRate = shw->khz * fmtNumberChannels * (fmtBitsPerSample / 8);  // 16 = 16 bit sound
	int fmtBlockAlign = (fmtBitsPerSample / 8) * shw->numchannels; // 16 bit sound

#ifdef _WIN32
	snprintf (fname, sizeof (fname), "%s/capture_%02d-%02d-%04d_%02d-%02d-%02d/audio.wav",
		movie_dir.string, movie_start_date.wDay, movie_start_date.wMonth, movie_start_date.wYear,
		movie_start_date.wHour, movie_start_date.wMinute, movie_start_date.wSecond);
#else
	snprintf (fname, sizeof (fname), "%s/capture_%02d-%02d-%04d_%02d-%02d-%02d/audio.wav",
		movie_dir.string, movie_start_date.tm_mday, 1 + movie_start_date.tm_mon, 1900 + movie_start_date.tm_year,
		movie_start_date.tm_hour, movie_start_date.tm_min, movie_start_date.tm_sec);
#endif
	if (!(wav_output = FS_OpenVFS (fname, "wb", FS_NONE_OS))) {
		FS_CreatePath (fname);
		if (!(wav_output = FS_OpenVFS (fname, "wb", FS_NONE_OS)))
			return;
	}

	// RIFF header
	VFS_WRITE (wav_output, "RIFF", 4);
	VFS_WRITE (wav_output, &chunkSize, 4);
	VFS_WRITE (wav_output, "WAVE", 4);

	// "fmt " chunk (24 bytes total)
	VFS_WRITE (wav_output, "fmt ", 4);
	VFS_WRITE (wav_output, &fmtBlockSize, 4);
	VFS_WRITE (wav_output, &fmtFormat, 2);
	VFS_WRITE (wav_output, &fmtNumberChannels, 2);
	VFS_WRITE (wav_output, &fmtSampleRate, 4);
	VFS_WRITE (wav_output, &fmtByteRate, 4);
	VFS_WRITE (wav_output, &fmtBlockAlign, 2);
	VFS_WRITE (wav_output, &fmtBitsPerSample, 2);

	// "data" chunk
	VFS_WRITE (wav_output, "data", 4);
	VFS_WRITE (wav_output, &chunkSize, 4);

	// actual data will be written as audio mixed each frame
	wav_sample_count = 0;
}

static void WAVCaptureFrame (int samples, byte *sample_buffer)
{
	int i;

	if (wav_output) {
		for (i = 0; i < samples; ++i) {
			unsigned long original = LittleLong (*(unsigned long*)sample_buffer);

			VFS_WRITE (wav_output, &original, 4);
			sample_buffer += 4;
		}

		wav_sample_count += samples;
	}
}

static void WAVCaptureStop (void)
{
	if (wav_output) {
		// Now we know how many samples, so can fill in the blocks in the file
		int dataSize = wav_sample_count * 2 * shw->numchannels; // 16 bit sound, stereo
		int riffChunkSize = 36 + dataSize; // 36 = 4[format field] + (8 + chunk1size)[fmt block] + 8 [subchunk2Id + size fields]

		VFS_SEEK (wav_output, 4, SEEK_SET);
		VFS_WRITE (wav_output, &riffChunkSize, 4);
		VFS_SEEK (wav_output, 40, SEEK_SET);   // RIFF header = 12, fmt chunk 24, "data" 4
		VFS_WRITE (wav_output, &dataSize, 4);
		VFS_CLOSE (wav_output);

		wav_output = 0;
		wav_sample_count = 0;
	}
}

void Movie_MixFrameSound (void (*mixFunction)(void))
{
	int samples_required = (int)(0.5 + Movie_Frametime() * shw->khz) * shw->numchannels - (captured_audio_samples << 1);

	memset(aviSoundBuffer, 0, sizeof(aviSoundBuffer));
	shw->buffer = (unsigned char*)aviSoundBuffer;
	shw->samples = min(samples_required, sizeof(aviSoundBuffer) / 2);
	frame_has_sound = false;

	do {
		mixFunction();
	} while (! frame_has_sound);
}

void Movie_TransferSound(void* data, int snd_linear_count)
{
	int samples_per_frame = (int)(0.5 + Movie_Frametime() * shw->khz);

	// Write some sound
	memcpy(capture_audio_samples + (captured_audio_samples << 1), data, snd_linear_count * shw->numchannels);
	captured_audio_samples += (snd_linear_count >> 1);
	shw->snd_sent += snd_linear_count * shw->numchannels;

	if (captured_audio_samples >= samples_per_frame) {
		// We have enough audio samples to match one frame of video
#ifdef _WIN32
		if (movie_is_avi) {
			Capture_WriteAudio (samples_per_frame, (byte *)capture_audio_samples);
		}
		else {
			WAVCaptureFrame (samples_per_frame, (byte *)capture_audio_samples);
		}
#else
		WAVCaptureFrame (samples_per_frame, (byte *)capture_audio_samples);
#endif
		memcpy (capture_audio_samples, capture_audio_samples + (samples_per_frame << 1), (captured_audio_samples - samples_per_frame) * 2 * shw->numchannels);
		captured_audio_samples -= samples_per_frame;

		frame_has_sound = true;
	}
}

static void OnChange_movie_dir(cvar_t *var, char *string, qbool *cancel) {
	if (Movie_IsCapturing()) {
		Com_Printf("Cannot change demo_capture_dir whilst capturing.  Use 'demo_capture stop' to cease capturing first.\n");
		*cancel = true;
		return;
	} else if (strlen(string) > 31) {
		Com_Printf("demo_capture_dir can only contain a maximum of 31 characters\n");
		*cancel = true;
		return;
	}
	Util_Process_Filename(string);
	if (!(Util_Is_Valid_Filename(string))) {
		Com_Printf(Util_Invalid_Filename_Msg("demo_capture_dir"));
		*cancel = true;
		return;
	}
}

typedef struct background_thread_s {
	SDL_Thread* thread;            // thread handle
	SDL_mutex* mutex;              // mutex
	SDL_sem* finished;             // signals that the background thread should terminate (set by main)
	SDL_sem* signal;               // queue length
	scr_sshot_target_t* data;      // queue data
	byte* buffer;                  // screenshot data
} background_thread_t;

#define MAX_SCREENSHOT_THREADS           8

static background_thread_t threads[MAX_SCREENSHOT_THREADS] = { { 0 } };
static byte* tempBuffer = 0;
static int background_threads = 0;
static int next_thread = 0;
static size_t movie_width = 0;
static size_t movie_height = 0;

static int Movie_BackgroundThread(void* thread_data)
{
	background_thread_t* thread = (background_thread_t*) thread_data;
	while (true) {
		// Wait to be woken up
		SDL_SemWait(thread->signal);

		SDL_LockMutex(thread->mutex);
		if (SDL_SemTryWait(thread->finished) == SDL_MUTEX_TIMEDOUT) {
			SCR_ScreenshotWrite(thread->data);
			SDL_UnlockMutex(thread->mutex);
		}
		else {
			SDL_UnlockMutex(thread->mutex);
			break;
		}
	}

	return 0;
}

qbool Movie_BackgroundInitialise(void)
{
	extern int glwidth, glheight;
	int i;

	background_threads = (int) bound(0, movie_background_threads.integer, MAX_SCREENSHOT_THREADS);
	if (background_threads) {
		memset(&threads, 0, sizeof(threads));
		next_thread = 0;

		// Create background threads in background
		for (i = 0; i < background_threads; ++i) {
			threads[i].mutex = SDL_CreateMutex();
			threads[i].signal = SDL_CreateSemaphore(0);
			threads[i].finished = SDL_CreateSemaphore(0);
			threads[i].buffer = Q_malloc(glwidth * glheight * 3);
			threads[i].thread = SDL_CreateThread(Movie_BackgroundThread, NULL, (void*)&threads[i]);
		}
	}

	movie_height = glheight;
	movie_width = glwidth;
	tempBuffer = Q_malloc(movie_width * movie_height * 3);

	return true;
}

void Movie_BackgroundShutdown(void)
{
	int i;

	for (i = 0; i < background_threads; ++i) {
		SDL_SemPost(threads[i].finished);
		SDL_SemPost(threads[i].signal);
	}

	for (i = 0; i < background_threads; ++i) {
		SDL_WaitThread(threads[i].thread, NULL);
		SDL_DestroySemaphore(threads[i].finished);
		SDL_DestroySemaphore(threads[i].signal);
		SDL_DestroyMutex(threads[i].mutex);

		Q_free(threads[i].buffer);
	}

	Q_free(tempBuffer);

}

byte* Movie_TempBuffer(size_t width, size_t height)
{
	if (width != movie_width || height != movie_height) {
		return NULL;
	}

	return tempBuffer;
}

qbool Movie_BackgroundCapture(scr_sshot_target_t* params)
{
	if (params->buffer != tempBuffer) {
		return false;
	}

	if (Movie_IsCapturing() && ! Movie_IsCapturingAVI() && background_threads) {
		background_thread_t* thread = &threads[next_thread];

		SDL_LockMutex(thread->mutex);
		memcpy(thread->buffer, params->buffer, params->width * params->height * 3);
		params->buffer = thread->buffer;
		thread->data = params;
		SDL_UnlockMutex(thread->mutex);
		SDL_SemPost(thread->signal);

		next_thread = (next_thread + 1) % background_threads;
		return true;
	}

	return false;
}

qbool Movie_AnimatedPNG(void)
{
	return Movie_IsCapturing() && capturing_apng;
}
