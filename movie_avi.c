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

#include <windows.h>
#include <vfw.h>
#include "quakedef.h"
#include "movie_avi.h"
#include "qsound.h"
#ifdef GLQUAKE
#include "gl_model.h"
#include "gl_local.h"
#endif


static void (CALLBACK *qAVIFileInit)(void);
static HRESULT (CALLBACK *qAVIFileOpen)(PAVIFILE *, LPCTSTR, UINT, LPCLSID);
static HRESULT (CALLBACK *qAVIFileCreateStream)(PAVIFILE, PAVISTREAM *, AVISTREAMINFO *);
static HRESULT (CALLBACK *qAVIMakeCompressedStream)(PAVISTREAM *, PAVISTREAM, AVICOMPRESSOPTIONS *, CLSID *);
static HRESULT (CALLBACK *qAVIStreamSetFormat)(PAVISTREAM, LONG, LPVOID, LONG);
static HRESULT (CALLBACK *qAVIStreamWrite)(PAVISTREAM, LONG, LONG, LPVOID, LONG, DWORD, LONG *, LONG *);
static ULONG (CALLBACK *qAVIStreamRelease)(PAVISTREAM);
static ULONG (CALLBACK *qAVIFileRelease)(PAVIFILE);
static void (CALLBACK *qAVIFileExit)(void);

static MMRESULT (ACMAPI *qacmDriverOpen)(LPHACMDRIVER, HACMDRIVERID, DWORD);
static MMRESULT (ACMAPI *qacmDriverDetails)(HACMDRIVERID, LPACMDRIVERDETAILS, DWORD);
static MMRESULT (ACMAPI *qacmDriverEnum)(ACMDRIVERENUMCB, DWORD, DWORD);
static MMRESULT (ACMAPI *qacmFormatTagDetails)(HACMDRIVER, LPACMFORMATTAGDETAILS, DWORD);
static MMRESULT (ACMAPI *qacmStreamOpen)(LPHACMSTREAM, HACMDRIVER, LPWAVEFORMATEX, LPWAVEFORMATEX, LPWAVEFILTER, DWORD, DWORD, DWORD);
static MMRESULT (ACMAPI *qacmStreamSize)(HACMSTREAM, DWORD, LPDWORD, DWORD);
static MMRESULT (ACMAPI *qacmStreamPrepareHeader)(HACMSTREAM, LPACMSTREAMHEADER, DWORD);
static MMRESULT (ACMAPI *qacmStreamUnprepareHeader)(HACMSTREAM, LPACMSTREAMHEADER, DWORD);
static MMRESULT (ACMAPI *qacmStreamConvert)(HACMSTREAM, LPACMSTREAMHEADER, DWORD);
static MMRESULT (ACMAPI *qacmStreamClose)(HACMSTREAM, DWORD);
static MMRESULT (ACMAPI *qacmDriverClose)(HACMDRIVER, DWORD);

static HINSTANCE handle_avi = NULL, handle_acm = NULL;

PAVIFILE	m_file;
PAVISTREAM	m_uncompressed_video_stream;
PAVISTREAM	m_compressed_video_stream;
PAVISTREAM	m_audio_stream;

unsigned long	m_codec_fourcc;
int		m_video_frame_counter;
int		m_video_frame_size;

qbool	m_audio_is_mp3;
int		m_audio_frame_counter;
WAVEFORMATEX	m_wave_format;
MPEGLAYER3WAVEFORMAT mp3_format;
qbool	mp3_driver;
HACMDRIVER	had;
HACMSTREAM	hstr;
ACMSTREAMHEADER	strhdr;

extern qbool movie_avi_loaded, movie_acm_loaded;
extern	cvar_t	movie_codec, movie_fps, movie_mp3, movie_mp3_kbps;

#define AVI_GETFUNC(f) (qAVI##f = (void *)GetProcAddress(handle_avi, "AVI" #f))
#define ACM_GETFUNC(f) (qacm##f = (void *)GetProcAddress(handle_acm, "acm" #f))

void Capture_InitAVI (void)
{
	movie_avi_loaded = false;

	if (!(handle_avi = LoadLibrary("avifil32.dll")))
	{
		Com_Printf ("\x02" "Avi capturing module not found\n");
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

	movie_avi_loaded = qAVIFileInit && qAVIFileOpen && qAVIFileCreateStream && 
			qAVIMakeCompressedStream && qAVIStreamSetFormat && qAVIStreamWrite && 
			qAVIStreamRelease && qAVIFileRelease && qAVIFileExit;

	if (!movie_avi_loaded)
	{
		Com_Printf_State (PRINT_FAIL, "Avi capturing module not initialized\n");
		goto fail;
	}

	Com_Printf_State (PRINT_OK, "Avi capturing module initialized\n");
	return;

fail:
	if (handle_avi)
	{
		FreeLibrary (handle_avi);
		handle_avi = NULL;
	}
}

void Capture_InitACM (void)
{
	movie_acm_loaded = false;

	if (!(handle_acm = LoadLibrary("msacm32.dll")))
	{
		Com_Printf ("\x02" "ACM module not found\n");
		goto fail;
	}

	ACM_GETFUNC(DriverOpen);
	ACM_GETFUNC(DriverEnum);
	ACM_GETFUNC(StreamOpen);
	ACM_GETFUNC(StreamSize);
	ACM_GETFUNC(StreamPrepareHeader);
	ACM_GETFUNC(StreamUnprepareHeader);
	ACM_GETFUNC(StreamConvert);
	ACM_GETFUNC(StreamClose);
	ACM_GETFUNC(DriverClose);
	qacmDriverDetails = (void *)GetProcAddress (handle_acm, "acmDriverDetailsA");
	qacmFormatTagDetails = (void *)GetProcAddress (handle_acm, "acmFormatTagDetailsA");

	movie_acm_loaded = qacmDriverOpen && qacmDriverDetails && qacmDriverEnum && 
			qacmFormatTagDetails && qacmStreamOpen && qacmStreamSize && 
			qacmStreamPrepareHeader && qacmStreamUnprepareHeader && 
			qacmStreamConvert && qacmStreamClose && qacmDriverClose;

	if (!movie_acm_loaded)
	{
		Com_Printf_State (PRINT_FAIL, "ACM module not initialized\n");
		goto fail;
	}

	Com_Printf_State (PRINT_OK, "ACM module initialized\n");
	return;

fail:
	if (handle_acm)
	{
		FreeLibrary (handle_acm);
		handle_acm = NULL;
	}
}

PAVISTREAM Capture_VideoStream (void)
{
	return m_codec_fourcc ? m_compressed_video_stream : m_uncompressed_video_stream;
}

BOOL CALLBACK acmDriverEnumCallback (HACMDRIVERID hadid, DWORD_PTR dwInstance, DWORD fdwSupport)
{
	if (fdwSupport & ACMDRIVERDETAILS_SUPPORTF_CODEC)
	{
		int	i;
		ACMDRIVERDETAILS details;

		details.cbStruct = sizeof(details);
		qacmDriverDetails (hadid, &details, 0);
		qacmDriverOpen (&had, hadid, 0);

		for (i = 0 ; i < details.cFormatTags ; i++)
		{
			ACMFORMATTAGDETAILS	fmtDetails;

			memset (&fmtDetails, 0, sizeof(fmtDetails));
			fmtDetails.cbStruct = sizeof(fmtDetails);
			fmtDetails.dwFormatTagIndex = i;
			qacmFormatTagDetails (had, &fmtDetails, ACM_FORMATTAGDETAILSF_INDEX);
			if (fmtDetails.dwFormatTag == WAVE_FORMAT_MPEGLAYER3)
			{
				Com_DPrintf ("MP3-capable ACM codec found: %s\n", details.szLongName);
				mp3_driver = true;

				return false;
			}
		}

		qacmDriverClose (had, 0);
	}

	return true;
}

qbool Capture_Open (char *filename)
{
	HRESULT				hr;
	BITMAPINFOHEADER	bitmap_info_header;
	AVISTREAMINFO		stream_header;
	char				*fourcc;

	m_video_frame_counter = m_audio_frame_counter = 0;
	m_file = NULL;
	m_codec_fourcc = 0;
	m_compressed_video_stream = m_uncompressed_video_stream = m_audio_stream = NULL;
	m_audio_is_mp3 = (qbool)movie_mp3.value;

	if (*(fourcc = movie_codec.string) != '0')	// codec fourcc supplied
	{
		m_codec_fourcc = mmioFOURCC (fourcc[0], fourcc[1], fourcc[2], fourcc[3]);
	}

	qAVIFileInit ();
	hr = qAVIFileOpen (&m_file, filename, OF_WRITE | OF_CREATE, NULL);
	if (FAILED(hr))
	{
		Com_Printf ("ERROR: Couldn't open AVI file\n");
		return false;
	}

	// initialize video data
#ifdef GLQUAKE
	m_video_frame_size = glwidth * glheight * 3;
#else
	m_video_frame_size = vid.width * vid.height * 3;
#endif

	memset (&bitmap_info_header, 0, sizeof(bitmap_info_header));
	bitmap_info_header.biSize = sizeof(BITMAPINFOHEADER);
#ifdef GLQUAKE
	bitmap_info_header.biWidth = glwidth;
	bitmap_info_header.biHeight = glheight;
#else
	bitmap_info_header.biWidth = vid.width;
	bitmap_info_header.biHeight = vid.height;
#endif
	bitmap_info_header.biPlanes = 1;
	bitmap_info_header.biBitCount = 24;
	bitmap_info_header.biCompression = BI_RGB;
	bitmap_info_header.biSizeImage = m_video_frame_size;

	memset (&stream_header, 0, sizeof(stream_header));
	stream_header.fccType = streamtypeVIDEO;
	stream_header.fccHandler = m_codec_fourcc;
	stream_header.dwScale = 1;
	stream_header.dwRate = (unsigned long)(0.5 + movie_fps.value);
	stream_header.dwSuggestedBufferSize = bitmap_info_header.biSizeImage;
	SetRect (&stream_header.rcFrame, 0, 0, bitmap_info_header.biWidth, bitmap_info_header.biHeight);

	hr = qAVIFileCreateStream (m_file, &m_uncompressed_video_stream, &stream_header);
	if (FAILED(hr))
	{
		Com_Printf ("ERROR: Couldn't create video stream\n");
		return false;
	}

	if (m_codec_fourcc)
	{
		AVICOMPRESSOPTIONS	opts;

		memset (&opts, 0, sizeof(opts));
		opts.fccType = stream_header.fccType;
		opts.fccHandler = m_codec_fourcc;

		// Make the stream according to compression
		hr = qAVIMakeCompressedStream (&m_compressed_video_stream, m_uncompressed_video_stream, &opts, NULL);
		if (FAILED(hr))
		{
			Com_Printf ("ERROR: Couldn't make compressed video stream\n");
			return false;
		}
	}

	hr = qAVIStreamSetFormat (Capture_VideoStream(), 0, &bitmap_info_header, bitmap_info_header.biSize);
	if (FAILED(hr))
	{
		Com_Printf ("ERROR: Couldn't set video stream format\n");
		return false;
	}

	// initialize audio data
	memset (&m_wave_format, 0, sizeof(m_wave_format));
	m_wave_format.wFormatTag = WAVE_FORMAT_PCM;
	m_wave_format.nChannels = 2; // always stereo in Quake sound engine
	m_wave_format.nSamplesPerSec = shm ? shm->format.speed : 0;
	m_wave_format.wBitsPerSample = 16; // always 16bit in Quake sound engine
	m_wave_format.nBlockAlign = m_wave_format.wBitsPerSample/8 * m_wave_format.nChannels;
	m_wave_format.nAvgBytesPerSec = m_wave_format.nSamplesPerSec * m_wave_format.nBlockAlign;

	memset (&stream_header, 0, sizeof(stream_header));
	stream_header.fccType = streamtypeAUDIO;
	stream_header.dwScale = m_wave_format.nBlockAlign;
	stream_header.dwRate = stream_header.dwScale * (unsigned long)m_wave_format.nSamplesPerSec;
	stream_header.dwSampleSize = m_wave_format.nBlockAlign;

	hr = qAVIFileCreateStream (m_file, &m_audio_stream, &stream_header);
	if (FAILED(hr))
	{
		Com_Printf ("ERROR: Couldn't create audio stream\n");
		return false;
	}

	if (m_audio_is_mp3)
	{
		MMRESULT	mmr;

		// try to find an MP3 codec
		had = NULL;
		mp3_driver = false;
		qacmDriverEnum (acmDriverEnumCallback, 0, 0);
		if (!mp3_driver)
		{
			Com_Printf ("ERROR: Couldn't find any MP3 decoder\n");
			return false;
		}

		memset (&mp3_format, 0, sizeof(mp3_format));
		mp3_format.wfx.wFormatTag = WAVE_FORMAT_MPEGLAYER3;
		mp3_format.wfx.nChannels = 2;
		mp3_format.wfx.nSamplesPerSec = shm->format.speed;
		mp3_format.wfx.wBitsPerSample = 0;
		mp3_format.wfx.nBlockAlign = 1;
		mp3_format.wfx.nAvgBytesPerSec = movie_mp3_kbps.value * 125;
		mp3_format.wfx.cbSize = MPEGLAYER3_WFX_EXTRA_BYTES;
		mp3_format.wID = MPEGLAYER3_ID_MPEG;
		mp3_format.fdwFlags = MPEGLAYER3_FLAG_PADDING_OFF;
		mp3_format.nBlockSize = mp3_format.wfx.nAvgBytesPerSec / movie_fps.value;
		mp3_format.nFramesPerBlock = 1;
		mp3_format.nCodecDelay = 1393;

		hstr = NULL;
		if ((mmr = qacmStreamOpen(&hstr, had, &m_wave_format, &mp3_format.wfx, NULL, 0, 0, 0)))
		{
			switch (mmr)
			{
			case MMSYSERR_INVALPARAM:
				Com_Printf ("ERROR: Invalid parameters passed to acmStreamOpen\n");
				return false;

			case ACMERR_NOTPOSSIBLE:
				Com_Printf ("ERROR: No ACM filter found capable of decoding MP3\n");
				return false;

			default:
				Com_Printf ("ERROR: Couldn't open ACM decoding stream\n");
				return false;
			}
		}

		hr = qAVIStreamSetFormat (m_audio_stream, 0, &mp3_format, sizeof(MPEGLAYER3WAVEFORMAT));
		if (FAILED(hr))
		{
			Com_Printf ("ERROR: Couldn't set audio stream format\n");
			return false;
		}
	}
	else
	{
		hr = qAVIStreamSetFormat (m_audio_stream, 0, &m_wave_format, sizeof(WAVEFORMATEX));
		if (FAILED(hr))
		{
			Com_Printf ("ERROR: Couldn't set audio stream format\n");
			return false;
		}
	}

	return true;
}

void Capture_Close (void)
{
	if (m_uncompressed_video_stream)
		qAVIStreamRelease (m_uncompressed_video_stream);
	if (m_compressed_video_stream)
		qAVIStreamRelease (m_compressed_video_stream);
	if (m_audio_stream)
		qAVIStreamRelease (m_audio_stream);
	if (m_audio_is_mp3)
	{
		qacmStreamClose (hstr, 0);
		qacmDriverClose (had, 0);
	}
	if (m_file)
		qAVIFileRelease (m_file);

	qAVIFileExit ();
}

void Capture_WriteVideo (byte *pixel_buffer, int size)
{
	HRESULT	hr;

	// Check frame size (TODO: other things too?) hasn't changed
	if (m_video_frame_size != size)
	{
		Com_Printf ("ERROR: Frame size changed\n");
		return;
	}

	if (!Capture_VideoStream())
	{
		Com_Printf ("ERROR: Video stream is NULL\n");
		return;
	}

	// Write the pixel buffer to to the AVIFile, one sample/frame at the time
	// set each frame to be a keyframe (it doesn't depend on previous frames).
	hr = qAVIStreamWrite (Capture_VideoStream(), m_video_frame_counter++, 1, pixel_buffer, m_video_frame_size, AVIIF_KEYFRAME, NULL, NULL);
	if (FAILED(hr))
	{
		Com_Printf ("ERROR: Couldn't write to AVI file\n");
		return;
	}
}

void Capture_WriteAudio (int samples, byte *sample_buffer)
{
	HRESULT		hr = E_UNEXPECTED;
	unsigned long	sample_bufsize;

	if (!m_audio_stream)
	{
		Com_Printf ("ERROR: Audio stream is NULL\n");
		return;
	}

	sample_bufsize = samples * m_wave_format.nBlockAlign;
	if (m_audio_is_mp3)
	{
		MMRESULT	mmr;
		byte		*mp3_buffer;
		unsigned long	mp3_bufsize;

		if ((mmr = qacmStreamSize(hstr, sample_bufsize, &mp3_bufsize, ACM_STREAMSIZEF_SOURCE)))
		{
			Com_Printf ("ERROR: Couldn't get mp3bufsize\n");
			return;
		}
		if (!mp3_bufsize)
		{
			Com_Printf ("ERROR: mp3bufsize is zero\n");
			return;
		}
		mp3_buffer = (byte *) Q_calloc (mp3_bufsize, 1);

		memset (&strhdr, 0, sizeof(strhdr));
		strhdr.cbStruct = sizeof(strhdr);
		strhdr.pbSrc = sample_buffer;
		strhdr.cbSrcLength = sample_bufsize;
		strhdr.pbDst = mp3_buffer;
		strhdr.cbDstLength = mp3_bufsize;

		if ((mmr = qacmStreamPrepareHeader(hstr, &strhdr, 0)))
		{
			Com_Printf ("ERROR: Couldn't prepare header\n");
			Q_free (mp3_buffer);
			return;
		}

		if ((mmr = qacmStreamConvert(hstr, &strhdr, ACM_STREAMCONVERTF_BLOCKALIGN)))
		{
			Com_Printf ("ERROR: Couldn't convert audio stream\n");
			goto clean;
		}

		hr = qAVIStreamWrite (m_audio_stream, m_audio_frame_counter++, 1, mp3_buffer, strhdr.cbDstLengthUsed, AVIIF_KEYFRAME, NULL, NULL);

clean:
		if ((mmr = qacmStreamUnprepareHeader(hstr, &strhdr, 0)))
		{
			Com_Printf ("ERROR: Couldn't unprepare header\n");
			Q_free (mp3_buffer);
			return;
		}

		Q_free (mp3_buffer);
	}
	else
	{
		// The audio is not in MP3 format, just write the WAV data to the avi.
		hr = qAVIStreamWrite (m_audio_stream, m_audio_frame_counter++, 1, sample_buffer, samples * m_wave_format.nBlockAlign, AVIIF_KEYFRAME, NULL, NULL);
	}

	if (FAILED(hr))
	{
		Com_Printf ("ERROR: Couldn't write to AVI file\n");
		return;
	}
}
