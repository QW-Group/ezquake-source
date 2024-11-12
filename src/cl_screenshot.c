/*
Copyright (C) 1996-2003 Id Software, Inc., A Nourai

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

$Id: cl_screen.c,v 1.156 2007-10-29 00:56:47 qqshka Exp $
*/

#include "quakedef.h"
#include "gl_model.h"
#include "image.h"
#ifdef _WIN32
#include "movie.h"	//joe: capturing to avi
#include "movie_avi.h"	//
#endif
#include "utils.h"
#include "r_local.h"
#include "r_renderer.h"
#ifdef X11_GAMMA_WORKAROUND
#include "tr_types.h"
#endif

#define	DEFAULT_SSHOT_FORMAT "png"

static void OnChange_scr_allowsnap(cvar_t *, char *, qbool *);

static cvar_t scr_sshot_autoname = { "sshot_autoname", "0" };
cvar_t scr_allowsnap      = { "scr_allowsnap", "1", 0, OnChange_scr_allowsnap };
cvar_t scr_sshot_format = { "sshot_format", DEFAULT_SSHOT_FORMAT };
cvar_t scr_sshot_dir = { "sshot_dir", "" };

static int scr_autosshot_countdown = 0;
static char auto_matchname[2 * MAX_OSPATH];

/******************************** SCREENSHOTS ********************************/

#define SSHOT_FAILED		-1
#define SSHOT_FAILED_QUIET	-2		//failed but don't print an error message
#define SSHOT_SUCCESS		0

static char *SShot_ExtForFormat(int format)
{
	switch (format) {
		case IMAGE_PCX:		return ".pcx";
		case IMAGE_TGA:		return ".tga";
		case IMAGE_JPEG:	return ".jpg";
		case IMAGE_PNG:		return ".png";
	}
	assert(!"SShot_ExtForFormat: unknown format");
	return "err";
}

static image_format_t SShot_FormatForName(char *name)
{
	char *ext;

	ext = COM_FileExtension(name);

	if (!strcasecmp(ext, "tga"))
		return IMAGE_TGA;

#ifdef WITH_PNG
	else if (!strcasecmp(ext, "png"))
		return IMAGE_PNG;
#endif

#ifdef WITH_JPEG
	else if (!strcasecmp(ext, "jpg"))
		return IMAGE_JPEG;
#endif

#ifdef WITH_PNG
	else if (!strcasecmp(scr_sshot_format.string, "png") || !strcasecmp(ext, "apng"))
		return IMAGE_PNG;
#endif

#ifdef WITH_JPEG
	else if (!strcasecmp(scr_sshot_format.string, "jpg") || !strcasecmp(scr_sshot_format.string, "jpeg"))
		return IMAGE_JPEG;
#endif

	else
		return IMAGE_TGA;
}

static char *Sshot_SshotDirectory(void)
{
	static char dir[MAX_PATH];

	strlcpy(dir, FS_LegacyDir(scr_sshot_dir.string), sizeof(dir));

	return dir;
}

#ifdef X11_GAMMA_WORKAROUND
extern unsigned short ramps[3][4096];
#else
extern unsigned short ramps[3][256];
#endif

//applies hwgamma to RGB data
static void applyHWGamma(byte *buffer, size_t size)
{
	int i;

	if (vid_hwgamma_enabled) {
		for (i = 0; i < size; i += 3) {
			int r = buffer[i + 0];
			int g = buffer[i + 1];
			int b = buffer[i + 2];

#ifdef X11_GAMMA_WORKAROUND
			if (glConfig.gammacrap.size >= 256 && glConfig.gammacrap.size <= 4096) {
				r = (int)((r * glConfig.gammacrap.size) / 256.0f);
				g = (int)((g * glConfig.gammacrap.size) / 256.0f);
				b = (int)((b * glConfig.gammacrap.size) / 256.0f);
			}
#endif
			buffer[i + 0] = ramps[0][r] >> 8;
			buffer[i + 1] = ramps[1][g] >> 8;
			buffer[i + 2] = ramps[2][b] >> 8;
		}
	}
}

int SCR_Screenshot(char *name, qbool movie_capture)
{
	scr_sshot_target_t* target_params = Q_malloc(sizeof(scr_sshot_target_t));
	size_t width = renderer.ScreenshotWidth();
	size_t height = renderer.ScreenshotHeight();
	size_t buffer_size = width * height * 3;

	// name is fullpath now
	//	name = (*name == '/') ? name + 1 : name;
	target_params->format = SShot_FormatForName(name);
	strlcpy(target_params->fileName, name, sizeof(target_params->fileName));
	COM_ForceExtension(target_params->fileName, SShot_ExtForFormat(target_params->format));
	target_params->width = width;
	target_params->height = height;

	target_params->buffer = Movie_TempBuffer(width, height);
	target_params->movie_capture = movie_capture;
	if (!target_params->buffer) {
		target_params->buffer = Q_malloc(buffer_size);
		target_params->freeMemory = true;
	}

	renderer.Screenshot(target_params->buffer, buffer_size);

	if (movie_capture && Movie_BackgroundCapture(target_params)) {
		return SSHOT_SUCCESS;
	}

	return SCR_ScreenshotWrite(target_params);
}

int SCR_ScreenshotWrite(scr_sshot_target_t* target_params)
{
	int i, temp;
	int success = SSHOT_FAILED;
	int format = target_params->format;
	byte* buffer = target_params->buffer;
	char* name = target_params->fileName;
	size_t buffersize = target_params->width * target_params->height * 3;

#ifdef WITH_PNG
	if (format == IMAGE_PNG) {
		applyHWGamma(buffer, buffersize);

		if (target_params->movie_capture && Movie_AnimatedPNG()) {
			extern cvar_t movie_fps;

			Image_WriteAPNGFrame(buffer, target_params->width, target_params->height, movie_fps.integer);
		}
		else {
			success = Image_WritePNG(name, image_png_compression_level.value, buffer, target_params->width, target_params->height) ? SSHOT_SUCCESS : SSHOT_FAILED;
		}
	}
#endif

#ifdef WITH_JPEG
	if (format == IMAGE_JPEG) {
		applyHWGamma(buffer, buffersize);
		success = Image_WriteJPEG(
			name, image_jpeg_quality_level.value,
			buffer + buffersize - 3 * target_params->width, -(int)target_params->width, (int)target_params->height
		) ? SSHOT_SUCCESS : SSHOT_FAILED;
	}
#endif

	if (format == IMAGE_TGA) {
		// swap rgb to bgr
		for (i = 0; i < buffersize; i += 3) {
			temp = buffer[i];
			buffer[i] = buffer[i + 2];
			buffer[i + 2] = temp;
		}
		applyHWGamma(buffer, buffersize);
		success = Image_WriteTGA(name, buffer, target_params->width, target_params->height) ? SSHOT_SUCCESS : SSHOT_FAILED;
	}

	if (target_params->freeMemory) {
		Q_free(target_params->buffer);
	}
	Q_free(target_params);
	return success;
}

#define MAX_SCREENSHOT_COUNT	65535

int SCR_GetScreenShotName(char *name, int name_size, char *sshot_dir)
{
	int i = 0;
	char ext[4], basename[64];
	FILE *f;

	ext[0] = 0;

	// Find a file name to save it to
#ifdef WITH_PNG
	if (!strcasecmp(scr_sshot_format.string, "png")) {
		strlcpy(ext, "png", 4);
	}
#endif

#ifdef WITH_JPEG
	if (!strcasecmp(scr_sshot_format.string, "jpeg") || !strcasecmp(scr_sshot_format.string, "jpg")) {
		strlcpy(ext, "jpg", 4);
	}
#endif

	if (!strcasecmp(scr_sshot_format.string, "tga")) {
		strlcpy(ext, "tga", 4);
	}

	if (!strcasecmp(scr_sshot_format.string, "pcx")) {
		strlcpy(ext, "pcx", 4);
	}

	if (!ext[0]) {
		strlcpy(ext, DEFAULT_SSHOT_FORMAT, sizeof(ext));
	}

	if (fabsf(scr_sshot_autoname.value - 1.0f) < 0.0001f) {
		// if sshot_autoname is 1, prefix with map name.
		snprintf(basename, sizeof(basename), "%s_", host_mapname.string);
	}
	else {
		// otherwise prefix with ezquake.
		strcpy(basename, "ezquake");
	}

	for (i = 0; i < MAX_SCREENSHOT_COUNT; i++) {
		snprintf(name, name_size, "%s%03i.%s", basename, i, ext);
		if (!(f = fopen(va("%s/%s", sshot_dir, name), "rb"))) {
			break;  // file doesn't exist
		}
		fclose(f);
	}

	if (i == MAX_SCREENSHOT_COUNT) {
		Com_Printf("Error: Cannot create more than %d screenshots\n", MAX_SCREENSHOT_COUNT);
		return -1;
	}

	return 1;
}

void SCR_ScreenShot_f(void)
{
	char name[MAX_OSPATH], *filename, *sshot_dir;
	int success;

	sshot_dir = Sshot_SshotDirectory();

	if (Cmd_Argc() == 2) {
		strlcpy(name, Cmd_Argv(1), sizeof(name));
	}
	else if (Cmd_Argc() == 1) {
		if (SCR_GetScreenShotName(name, sizeof(name), sshot_dir) < 0) {
			return;
		}
	}
	else {
		Com_Printf("Usage: %s [filename]\n", Cmd_Argv(0));
		return;
	}

	for (filename = name; *filename == '/' || *filename == '\\'; filename++)
		;

	success = SCR_Screenshot(va("%s/%s", sshot_dir, filename), false);

	if (success != SSHOT_FAILED_QUIET) {
		Com_Printf("%s %s\n", success == SSHOT_SUCCESS ? "Wrote" : "Couldn't write", name);
	}
}

void SCR_RSShot_f(void)
{
	int success = SSHOT_FAILED;
	char filename[MAX_PATH];
	int width, height;
	byte *base, *pixels;

	if (CL_IsUploading())
		return;		// already one pending

	if (cls.state < ca_onserver)
		return;		// gotta be connected

	if (!scr_allowsnap.value) {
		MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
		SZ_Print(&cls.netchan.message, "snap\n");
		return;
	}
	
	snprintf(filename, sizeof(filename), "%s/temp/__rsshot__", Sshot_SshotDirectory());

	width = 800; height = 600;
	base = (byte *)Q_malloc((width * height + glwidth * glheight) * 3);
	pixels = base + glwidth * glheight * 3;

	renderer.Screenshot(base, glwidth * glheight * 3);
	Image_Resample(base, glwidth, glheight, pixels, width, height, 3, 0);
#ifdef WITH_JPEG
	success = Image_WriteJPEG(filename, 70, pixels + 3 * width * (height - 1), -width, height) ? SSHOT_SUCCESS : SSHOT_FAILED;
#endif
	if (success == SSHOT_FAILED) {
		success = Image_WriteTGA(filename, pixels, width, height) ? SSHOT_SUCCESS : SSHOT_FAILED;
	}

	Q_free(base);

	if (success == SSHOT_SUCCESS) {
		FILE	*f;
		byte	*screen_shot;
		int	size;
		if ((size = FS_FileOpenRead(filename, &f)) != -1) {
			screen_shot = (byte *)Q_malloc(size);
			if (fread(screen_shot, 1, (size_t)size, f) == (size_t)size) {
				CL_StartUpload(screen_shot, size);
			}

			fclose(f);
			Q_free(screen_shot);
		}
	}

	remove(filename);
}

qbool SCR_TakingAutoScreenshot(void)
{
	return scr_autosshot_countdown > 0;
}

void SCR_CheckAutoScreenshot(void)
{
	char *filename, savedname[MAX_PATH], *sshot_dir, *fullsavedname, *ext;
	char *exts[5] = { "pcx", "tga", "png", "jpg", NULL };
	int num;

	if (scr_autosshot_countdown <= 0 || --scr_autosshot_countdown) {
		return;
	}

	if (!cl.intermission) {
		return;
	}

	sshot_dir = Sshot_SshotDirectory();

	// no, sorry
	//	while (*sshot_dir && (*sshot_dir == '/'))
	//		sshot_dir++; // will skip all '/' chars at the beginning

	if (!sshot_dir[0])
		sshot_dir = cls.gamedir;

	for (filename = auto_matchname; *filename == '/' || *filename == '\\'; filename++)
		;

	ext = SShot_ExtForFormat(SShot_FormatForName(filename));

	fullsavedname = va("%s/%s", sshot_dir, filename);
	if ((num = Util_Extend_Filename(fullsavedname, exts)) == -1) {
		Com_Printf("Error: no available filenames\n");
		return;
	}

	snprintf(savedname, sizeof(savedname), "%s_%03i%s", filename, num, ext);
	fullsavedname = va("%s/%s", sshot_dir, savedname);

	renderer.EnsureFinished();

	if ((SCR_Screenshot(fullsavedname, false)) == SSHOT_SUCCESS) {
		Com_Printf("Match scoreboard saved to %s\n", savedname);
	}
}

void SCR_AutoScreenshot(char *matchname)
{
	if (cl.intermission == 1) {
		scr_autosshot_countdown = vid.numpages;
		strlcpy(auto_matchname, matchname, sizeof(auto_matchname));
	}
}

// Capturing to avi.
void SCR_Movieshot(char *name)
{
#ifdef _WIN32
	if (Movie_IsCapturingAVI()) {
		int size = 0;
		// Capturing a movie.
		int i;
		byte *buffer, temp;

		// Set buffer size to fit RGB data for the image.
		size = glwidth * glheight * 3;

		// Allocate the RGB buffer, get the pixels from GL and apply the gamma.
		buffer = (byte *)Q_malloc(size);
		renderer.Screenshot(buffer, size);
		applyHWGamma(buffer, size);

		// We now have a byte buffer with RGB values, but
		// before we write it to the file, we need to swap
		// them to GBR instead, which windows DIBs uses.
		// (There's a GL Extension that allows you to use
		// BGR_EXT instead of GL_RGB in the glReadPixels call
		// instead, but there is no real speed gain using it).
		for (i = 0; i < size; i += 3) {
			// Swap RGB => GBR
			temp = buffer[i];
			buffer[i] = buffer[i + 2];
			buffer[i + 2] = temp;
		}

		// Write the buffer to video.
		Capture_WriteVideo(buffer, size);

		Q_free(buffer);
	}
	else {
		// We're just capturing images.
		SCR_Screenshot(name, true);
	}

#else // _WIN32

	// Capturing to avi only supported in windows yet.
	SCR_Screenshot(name, true);

#endif // _WIN32
}

static void OnChange_scr_allowsnap(cvar_t *var, char *s, qbool *cancel)
{
	*cancel = (cls.state >= ca_connected && cbuf_current == &cbuf_svc);
}

void SShot_RegisterCvars(void)
{
	if (!host_initialized) {
		Cvar_SetCurrentGroup(CVAR_GROUP_SCREENSHOTS);
		Cvar_Register(&scr_allowsnap);
		Cvar_Register(&scr_sshot_autoname);
		Cvar_Register(&scr_sshot_format);
		Cvar_Register(&scr_sshot_dir);
		Cvar_ResetCurrentGroup();

		Cmd_AddCommand("screenshot", SCR_ScreenShot_f);
	}
}
