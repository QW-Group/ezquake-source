/*
Copyright (C) 2007 Paul Archer

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

    $Id: snd_ov.c,v 1.1 2007-10-25 14:54:30 dkure Exp $
*/

// snd_ov.c -- sound caching for ogg vorbis
#include "quakedef.h"
#include "fmod.h"
#include "qsound.h"
#include "modules.h"	// qlib_dllfunction_t stuff

#ifdef WITH_OGG_VORBIS

#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

/* Vorbis function pointers */
static int (*qov_clear)          (OggVorbis_File *vf);
static int (*qov_open_callbacks) (void *datasource, OggVorbis_File *vf,
						char *initial, long ibytes, ov_callbacks callbacks);

static ogg_int64_t (*qov_pcm_total)  (OggVorbis_File *vf,int i);

static vorbis_info    *(*qov_info)    (OggVorbis_File *vf,int link);
static vorbis_comment *(*qov_comment) (OggVorbis_File *vf,int link);

static long (*qov_read) (OggVorbis_File *vf,char *buffer,int length,
						int bigendianp,int word,int sgned,int *bitstream);

static QLIB_HANDLETYPE_T libvorbis_handle = NULL;
#define NUM_VORBISPROCS   (sizeof(vorbisProcs)/sizeof(vorbisProcs[0]))
static qlib_dllfunction_t vorbisProcs[] = {
	{"ov_clear", 			(void **) &qov_clear},
	{"ov_open_callbacks",	(void **) &qov_open_callbacks},
	{"ov_pcm_total", 		(void **) &qov_pcm_total},
	{"ov_info", 			(void **) &qov_info},
	{"ov_comment", 			(void **) &qov_comment},
	{"ov_read", 			(void **) &qov_read},
};

qbool vorbis_CheckActive(void) 
{
	return !!libvorbis_handle;
}

void vorbis_FreeLibrary(void) {
	if (libvorbis_handle) {
		QLIB_FREELIBRARY(libvorbis_handle);
	}
	// Maybe need to clear all the function pointers too
}

void vorbis_LoadLibrary(void) 
{
#ifdef _WIN32
	libvorbis_handle = LoadLibrary("libvorbisfile.dll");
#else // _WIN32
	libvorbis_handle = dlopen("libvorbisfile.so", RTLD_NOW);
#endif // _WIN32
	if (!libvorbis_handle)
		return;

	if (!QLib_ProcessProcdef(libvorbis_handle, vorbisProcs, NUM_VORBISPROCS)) {
		vorbis_FreeLibrary();
		return;
	}
}

static size_t
vorbis_callback_read (void *ptr, size_t size, size_t nmemb,  
					  void *datasource)  
{
	vfsfile_t *f = (vfsfile_t *) datasource;
	return VFS_READ(f, ptr, size*nmemb, NULL);
}

static int
vorbis_callback_seek (void *datasource, ogg_int64_t offset, int whence)
{
	vfsfile_t *f = (vfsfile_t *) datasource;
	return VFS_SEEK(f, offset, whence);
}

static int
vorbis_callback_close (void *datasource)
{
	vfsfile_t *f = (vfsfile_t *) datasource;
	VFS_CLOSE(f);
	return 0;
}

static long
vorbis_callback_tell (void *datasource)
{
	vfsfile_t *f = (vfsfile_t *) datasource;
	return VFS_TELL(f);
}

sfxcache_t *S_LoadSound(sfx_t *s)
{
	char namebuffer[MAX_OSPATH];
	char extionless[MAX_OSPATH];
	sfxcache_t *sc = NULL;
	int eof = 0;
	int current_section;
	vfsfile_t *f;
	OggVorbis_File oggfile;
	vorbis_info *ogginfo;
	ov_callbacks ovc = {
		vorbis_callback_read,
		vorbis_callback_seek,
		vorbis_callback_close,
		vorbis_callback_tell
	};
	static char pcmbuff[4096];
	
	//unsigned char *data;
	wavinfo_t info;
	int len;
	//int filesize;

	// see if still in memory
	if ((sc = (sfxcache_t *) Cache_Check (&s->cache)))
		return sc;

	if (!vorbis_CheckActive())
		vorbis_LoadLibrary();

	// load it in
	COM_StripExtension(s->name, extionless);
	snprintf (namebuffer, sizeof (namebuffer), "sound/%s.ogg", extionless);

/*	if (!(data = FS_LoadTempFile (namebuffer, &filesize))) {
		Com_Printf ("Couldn't load %s\n", namebuffer);
		return NULL;
	}*/

	if (!(f = FS_OpenVFS(namebuffer, "rb", FS_ANY))) {
		Com_Printf("Couldn't load %s\n", namebuffer);
		return NULL;
	}

	if (qov_open_callbacks(f, &oggfile, NULL, 0, ovc)) {
		Com_Printf("Invalid sound file %s\n", namebuffer);
		VFS_CLOSE(f);
		return NULL;
	}
	if (!(ogginfo = qov_info(&oggfile,-1))) {
		Com_Printf("Unable to retrieve information for %s\n", namebuffer);
		goto fail;
	}

	/* Print some information */
	fprintf(stderr, "%s\n", namebuffer);
	{
		char **ptr = qov_comment(&oggfile,-1)->user_comments;
		while(*ptr){
			fprintf(stderr,"%s\n",*ptr);
			++ptr;
		}
		fprintf(stderr,"\nBitstream is %d channel, %ldHz\n",
				ogginfo->channels, ogginfo->rate);
		fprintf(stderr,"\nDecoded length: %ld samples\n",
				(long) qov_pcm_total(&oggfile,-1));
		fprintf(stderr,"Encoded by: %s\n\n", qov_comment(&oggfile,-1)->vendor);
	}

	info.rate = ogginfo->rate;			// Frequency rate
	info.width = 1;						// 8 bit samples
	info.channels = ogginfo->channels;	// 1 == mono, 2 == stereo
	info.loopstart = -1;				// NOOO idea...
	info.samples = qov_pcm_total(&oggfile,-1);	// Total samples
	info.dataofs = 0;					// offset

	
	//FMod_CheckModel(namebuffer, data, filesize);

	//info = GetWavinfo (s->name, data, filesize);

	// Stereo sounds are allowed (intended for music)
	if (info.channels < 1 || info.channels > 2) {
		Com_Printf("%s has an unsupported number of channels (%i)\n",s->name, info.channels);
		return NULL;
	}

	len = (int) ((double) info.samples * (double) shm->format.speed / (double) info.rate);
	len = len * info.width * info.channels;

	if (!(sc = (sfxcache_t *) Cache_Alloc (&s->cache, len + sizeof(sfxcache_t), s->name)))
		return NULL;

	/* Read the whole Ogg Vorbis file in */
	byte *output = sc->data;
	while (!eof) {
		// 0 == little endian, 1 = big endian
		// 1 == 8 bit samples, 2 = 16 bit samples
		// 1 == signed samples
		long ret = qov_read(&oggfile, pcmbuff, sizeof(pcmbuff), 
				0, info.width, 1, &current_section);
		if (ret == 0) {
			eof = 1;
		} else if (ret < 0) {
			/* error in the stream. Not a problem, just reporting it in
			 * case we (the app) cares. In this case, we don't. */
		} else {
			/* we don't bother dealing with sample rate changes, etc, but
			 * you'll have to */
			memcpy(output, pcmbuff, ret);
			output += ret;
		}
	}
	qov_clear(&oggfile);

	sc->total_length = (unsigned int) info.samples;
	sc->format.speed = info.rate;
	sc->format.width = info.width;
	sc->format.channels = info.channels;
	if (info.loopstart < 0)
		sc->loopstart = -1;
	else
		sc->loopstart = (int)((double)info.loopstart * (double)shm->format.speed / (double)sc->format.speed);

//	ResampleSfx (s, data + info.dataofs, info.samples, &sc->format, sc->data);
	return sc;

fail:
	qov_clear(&oggfile); // Only goto fail if ov_open* succeded
	if (f) {
		VFS_CLOSE(f);
	}
	return NULL;
}

#endif // WITH_OGG_VORBIS
