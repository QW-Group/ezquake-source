/*
Copyright (C) 2001 Quake done Quick

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
// movie_avi.c

#include "quakedef.h"
#include "movie_avi.h"
#include <windows.h>
#include <vfw.h>

typedef void (CALLBACK *AVI_FileInit_t)(void);
typedef HRESULT (CALLBACK *AVI_FileOpen_t)(PAVIFILE *, LPCTSTR, UINT, LPCLSID);
typedef HRESULT (CALLBACK *AVI_FileCreateStream_t)(PAVIFILE, PAVISTREAM *, AVISTREAMINFO *);
typedef HRESULT (CALLBACK *AVI_MakeCompressedStream_t)(PAVISTREAM *, PAVISTREAM, AVICOMPRESSOPTIONS *, CLSID *);
typedef HRESULT (CALLBACK *AVI_StreamSetFormat_t)(PAVISTREAM, LONG, LPVOID, LONG);
typedef HRESULT (CALLBACK *AVI_StreamWrite_t)(PAVISTREAM, LONG, LONG, LPVOID, LONG, DWORD, LONG *, LONG *);
typedef ULONG (CALLBACK *AVI_StreamRelease_t)(PAVISTREAM);
typedef ULONG (CALLBACK *AVI_FileRelease_t)(PAVIFILE);
typedef void (CALLBACK *AVI_FileExit_t)(void);

static	HINSTANCE	hAvidll = NULL;

AVI_FileInit_t		AVI_FileInit;
AVI_FileOpen_t		AVI_FileOpen;
AVI_FileCreateStream_t	AVI_FileCreateStream;
AVI_MakeCompressedStream_t AVI_MakeCompressedStream;
AVI_StreamSetFormat_t	AVI_StreamSetFormat;
AVI_StreamWrite_t	AVI_StreamWrite;
AVI_StreamRelease_t	AVI_StreamRelease;
AVI_FileRelease_t	AVI_FileRelease;
AVI_FileExit_t		AVI_FileExit;

PAVIFILE	m_file;
PAVISTREAM	m_uncompressed_video_stream;
PAVISTREAM	m_compressed_video_stream;
PAVISTREAM	m_uncompressed_audio_stream;

unsigned long	m_codec_fourcc;
int		m_video_frame_counter;
int		m_video_frame_size;
float		m_fps;
int		m_audio_hz;

int		m_audio_frame_counter;
WAVEFORMATEX	m_wave_format;

extern qboolean movie_avi_loaded;

#define AVI_GETFUNC(f) (AVI_##f = (AVI_##f##_t)GetProcAddress(hAvidll, "AVI" #f))

void Capture_InitAVI (void)
{
	movie_avi_loaded = false;

	if (!(hAvidll = LoadLibrary("avifil32.dll")))
	{
		Com_Printf ("Couldn't load avifil32.dll\n");
		goto fail;
	}

	AVI_GETFUNC(FileInit);
	AVI_GETFUNC(FileOpen);
	AVI_GETFUNC(FileCreateStream);
	AVI_GETFUNC(MakeCompressedStream);
	AVI_GETFUNC(StreamSetFormat);
	AVI_GETFUNC(StreamWrite);
	AVI_GETFUNC(StreamRelease);
	AVI_GETFUNC(FileRelease);
	AVI_GETFUNC(FileExit);

	movie_avi_loaded = AVI_FileInit && AVI_FileOpen && AVI_FileCreateStream && 
			AVI_MakeCompressedStream && AVI_StreamSetFormat && 
			AVI_StreamWrite && AVI_StreamRelease && AVI_FileRelease && 
			AVI_FileExit;

	if (!movie_avi_loaded)
	{
		Com_Printf ("Couldn't initialize avifil32.dll\n");
		goto fail;
	}

	Com_Printf ("Avi capturing initialized\n");
	return;

fail:
	if (hAvidll)
	{
		FreeLibrary (hAvidll);
		hAvidll = NULL;
	}
}

void Capture_Open (char* filename, char *fourcc, float fps, int audio_hz)
{
	HRESULT	hr;

	m_fps = fps;
	m_audio_hz = audio_hz;
	m_video_frame_counter = m_audio_frame_counter = 0;
	m_file = NULL;
	m_codec_fourcc = 0;
	m_compressed_video_stream = m_uncompressed_video_stream = m_uncompressed_audio_stream = NULL;

	if (fourcc)	// codec fourcc supplied
		m_codec_fourcc = mmioFOURCC (*(fourcc+0), *(fourcc+1), *(fourcc+2), *(fourcc+3));

	AVI_FileInit ();
	hr = AVI_FileOpen (&m_file, filename, OF_WRITE | OF_CREATE, NULL);
}

void Capture_Close (void)
{
	if (m_uncompressed_video_stream)
		AVI_StreamRelease (m_uncompressed_video_stream);
	if (m_compressed_video_stream)
		AVI_StreamRelease (m_compressed_video_stream);
	if (m_uncompressed_audio_stream)
		AVI_StreamRelease (m_uncompressed_audio_stream);
	if (m_file)
		AVI_FileRelease (m_file);

	AVI_FileExit ();
}

PAVISTREAM Capture_VideoStream (void)
{
	return m_codec_fourcc ? m_compressed_video_stream : m_uncompressed_video_stream;
}

PAVISTREAM Capture_AudioStream (void)
{
	return m_uncompressed_audio_stream;
}

qboolean Capture_WriteVideo (int width, int height, unsigned char *pixel_buffer)
{
	HRESULT	hr;

	if (!hAvidll || !m_file)
		return false;

	if (!m_video_frame_counter)
	{
		BITMAPINFOHEADER	bitmap_info_header;
		AVISTREAMINFO		stream_header;

		// write header etc based on first_frame
		m_video_frame_size = width * height * 3;

		memset (&bitmap_info_header, 0, sizeof(BITMAPINFOHEADER));
		bitmap_info_header.biSize = 40;
		bitmap_info_header.biWidth = width;
		bitmap_info_header.biHeight = height;
		bitmap_info_header.biPlanes = 1;
		bitmap_info_header.biBitCount = 24;
		bitmap_info_header.biCompression = BI_RGB;
		bitmap_info_header.biSizeImage = m_video_frame_size * 3;

		memset (&stream_header, 0, sizeof(stream_header));
		stream_header.fccType = streamtypeVIDEO;
		stream_header.fccHandler = m_codec_fourcc;
		stream_header.dwScale = 100;
		stream_header.dwRate = (unsigned long)(0.5 + m_fps * 100.0);
		SetRect (&stream_header.rcFrame, 0, 0, width, height);  

		hr = AVI_FileCreateStream (m_file, &m_uncompressed_video_stream, &stream_header);
		if (FAILED(hr))
			return false;

		if (m_codec_fourcc)
		{
			AVICOMPRESSOPTIONS	opts, *aopts[1] = {&opts};

			memset (&opts, 0, sizeof(opts));
			opts.fccType = stream_header.fccType;
			opts.fccHandler = m_codec_fourcc;

			// Make the stream according to compression
			hr = AVI_MakeCompressedStream (&m_compressed_video_stream, m_uncompressed_video_stream, &opts, NULL);
			if (FAILED(hr))
				return false;
		}

		hr = AVI_StreamSetFormat (Capture_VideoStream(), 0, &bitmap_info_header, sizeof(BITMAPINFOHEADER));
		if (FAILED(hr))
			return false;
	}
	else
	{	// check frame size (TODO: other things too?) hasn't changed
		if (m_video_frame_size != width * height * 3)
			return false;
	}

	if (!Capture_VideoStream())
		return false;

	hr = AVI_StreamWrite (Capture_VideoStream(), m_video_frame_counter++, 1, pixel_buffer, m_video_frame_size, AVIIF_KEYFRAME, NULL, NULL);
	if (FAILED(hr))
		return false;

	return true;
}

qboolean Capture_WriteAudio (int samples, unsigned char *sample_buffer)
{
	HRESULT	hr;

	if (!hAvidll || !m_file)
		return false;

	if (!m_audio_frame_counter)
	{
		AVISTREAMINFO	stream_header;

		// write header etc based on first_frame
		memset (&m_wave_format, 0, sizeof(WAVEFORMATEX));
		m_wave_format.wFormatTag = WAVE_FORMAT_PCM; 
		m_wave_format.nChannels = 2;		// always stereo in Quake sound engine
		m_wave_format.nSamplesPerSec = m_audio_hz;
		m_wave_format.wBitsPerSample = 16;	// always 16bit in Quake sound engine
		m_wave_format.nBlockAlign = m_wave_format.wBitsPerSample/8 * m_wave_format.nChannels; 
		m_wave_format.nAvgBytesPerSec = m_wave_format.nSamplesPerSec * m_wave_format.nBlockAlign; 
		m_wave_format.cbSize = 0; 

		memset (&stream_header, 0, sizeof(stream_header));
		stream_header.fccType = streamtypeAUDIO;
		stream_header.dwScale = m_wave_format.nBlockAlign;
		stream_header.dwRate = stream_header.dwScale * (unsigned long)m_wave_format.nSamplesPerSec;
		stream_header.dwSampleSize = m_wave_format.nBlockAlign;

		hr = AVI_FileCreateStream (m_file, &m_uncompressed_audio_stream, &stream_header);
		if (FAILED(hr))
			return false;

		hr = AVI_StreamSetFormat (Capture_AudioStream(), 0, &m_wave_format, sizeof(WAVEFORMATEX));
		if (FAILED(hr))
			return false;
	}

	if (!Capture_AudioStream())
		return false;

	hr = AVI_StreamWrite (Capture_AudioStream(), m_audio_frame_counter++, 1, sample_buffer, samples * m_wave_format.nBlockAlign, AVIIF_KEYFRAME, NULL, NULL);
	if (FAILED(hr))
		return false;

	return true;
}
