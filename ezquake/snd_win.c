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

#include "quakedef.h"
#include "winquake.h"
#include "sound.h"


#define iDirectSoundCreate(a,b,c)	pDirectSoundCreate(a,b,c)

HRESULT (WINAPI *pDirectSoundCreate)(GUID FAR *lpGUID, LPDIRECTSOUND FAR *lplpDS, IUnknown FAR *pUnkOuter);
HRESULT (WINAPI *pDirectSoundEnumerate)(LPDSENUMCALLBACKA pDSEnumCallback, LPVOID pContext);

// 64K is > 1 second at 16-bit, 22050 Hz
#define	WAV_BUFFERS				64
#define	WAV_MASK				0x3F
#define	WAV_BUFFER_SIZE			0x0400
#define SECONDARY_BUFFER_SIZE	0x10000

typedef enum {SIS_SUCCESS, SIS_FAILURE, SIS_NOTAVAIL} sndinitstat;

static qboolean	wavonly;
static qboolean	dsound_init;
static qboolean	wav_init;
static qboolean	snd_firsttime = true, snd_isdirect, snd_iswave;
static qboolean	primary_format_set;

static int	sample16;
static int	snd_sent, snd_completed;

/* 
 * Global variables. Must be visible to window-procedure function 
 *  so it can unlock and free the data block after it has been played. 
 */ 

static int		currentenum = 0;
static LPGUID	dsdevice = NULL;

HANDLE		hData;
HPSTR		lpData, lpData2;

HGLOBAL		hWaveHdr;
LPWAVEHDR	lpWaveHdr;

HWAVEOUT    hWaveOut; 

WAVEOUTCAPS	wavecaps;

DWORD	gSndBufSize;

MMTIME		mmstarttime;

LPDIRECTSOUND pDS;
LPDIRECTSOUNDBUFFER pDSBuf, pDSPBuf;

HINSTANCE hInstDS;

qboolean SNDDMA_InitDirect (void);
qboolean SNDDMA_InitWav (void);

BOOL CALLBACK DS_EnumDevices(LPGUID lpGUID, LPCTSTR lpszDesc, LPCTSTR lpszDrvName, LPVOID lpContext) {
	if (*((int *) lpContext) == currentenum++) {
		dsdevice = lpGUID;
		return FALSE;
	} else {
		return TRUE;
	}
}

void S_BlockSound (void) {

	// DirectSound takes care of blocking itself
	if (snd_iswave) {
		snd_blocked++;

		if (snd_blocked == 1)
			waveOutReset (hWaveOut);
	}
}

void S_UnblockSound (void) {

	// DirectSound takes care of blocking itself
	if (snd_iswave)
		snd_blocked--;
}

void FreeSound (void) {
	int i;

	if (pDSBuf) {
		pDSBuf->lpVtbl->Stop(pDSBuf);
		pDSBuf->lpVtbl->Release(pDSBuf);
	}

	// only release primary buffer if it's not also the mixing buffer we just released
	if (pDSPBuf && pDSBuf != pDSPBuf)
		pDSPBuf->lpVtbl->Release(pDSPBuf);

	if (pDS) {
		pDS->lpVtbl->SetCooperativeLevel (pDS, mainwindow, DSSCL_NORMAL);
		pDS->lpVtbl->Release(pDS);
	}

	if (hWaveOut) {
		waveOutReset (hWaveOut);

		if (lpWaveHdr) {
			for (i = 0; i < WAV_BUFFERS; i++)
				waveOutUnprepareHeader (hWaveOut, lpWaveHdr+i, sizeof(WAVEHDR));
		}

		waveOutClose (hWaveOut);

		if (hWaveHdr) {
			GlobalUnlock(hWaveHdr); 
			GlobalFree(hWaveHdr);
		}

		if (hData) {
			GlobalUnlock(hData);
			GlobalFree(hData);
		}

	}

	pDS = NULL;
	pDSBuf = NULL;
	pDSPBuf = NULL;
	hWaveOut = 0;
	hData = 0;
	hWaveHdr = 0;
	lpData = NULL;
	lpWaveHdr = NULL;
	dsound_init = false;
	wav_init = false;
}

sndinitstat SNDDMA_InitDirect (void) {
	DSBUFFERDESC dsbuf;
	DSBCAPS dsbcaps;
	DWORD dwSize, dwWrite;
	DSCAPS dscaps;
	WAVEFORMATEX format, pformat; 
	HRESULT hresult;
	int reps, temp, devicenum;

	memset ((void *)&sn, 0, sizeof (sn));

	shm = &sn;

	shm->channels = 2;
	shm->samplebits = 16;
	shm->speed = (s_khz.value == 44) ? 44100 : (s_khz.value == 22) ? 22050 : 11025;

	memset (&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = shm->channels;
    format.wBitsPerSample = shm->samplebits;
    format.nSamplesPerSec = shm->speed;
    format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
    format.cbSize = 0;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign; 

	if (!hInstDS) {
		hInstDS = LoadLibrary("dsound.dll");
		
		if (hInstDS == NULL) {
			Com_Printf ("Couldn't load dsound.dll\n");
			return SIS_FAILURE;
		}

		pDirectSoundCreate = (void *)GetProcAddress(hInstDS,"DirectSoundCreate");

		if (!pDirectSoundCreate) {
			Com_Printf ("Couldn't get DS proc addr\n");
			return SIS_FAILURE;
		}

		if (!(pDirectSoundEnumerate = (void *) GetProcAddress(hInstDS, "DirectSoundEnumerateA"))) {
			Com_Printf ("Couldn't get DirectSoundEnumerateA proc addr\n");
			return SIS_FAILURE;
		}
	}

	dsdevice = NULL;
	if ((temp = COM_CheckParm("-snddev")) && temp + 1 < com_argc) {
		devicenum = Q_atoi(com_argv[temp + 1]);
		currentenum = 0;
		if ((hresult = pDirectSoundEnumerate(DS_EnumDevices, &devicenum)) != DS_OK) {
			Com_Printf ("Couldn't open preferred sound device. Falling back to primary sound device.\n");
			dsdevice = NULL;
		}
	}

	hresult = pDirectSoundCreate(dsdevice, &pDS, NULL);
	if (hresult != DS_OK && dsdevice) {
		Com_Printf ("Couldn't open preferred sound device. Falling back to primary sound device.\n");
		dsdevice = NULL;
		hresult = pDirectSoundCreate(dsdevice, &pDS, NULL);
	}

	if (hresult != DS_OK) {
		if (hresult == DSERR_ALLOCATED) {
			Com_Printf ("DirectSoundCreate failed, hardware already in use\n");
			return SIS_NOTAVAIL;
		}

		Com_Printf ("DirectSound create failed\n");
		return SIS_FAILURE;
	}

	dscaps.dwSize = sizeof(dscaps);

	if (DS_OK != pDS->lpVtbl->GetCaps (pDS, &dscaps))
		Com_Printf ("Couldn't get DS caps\n");

	if (dscaps.dwFlags & DSCAPS_EMULDRIVER) {
		Com_Printf ("No DirectSound driver installed\n");
		FreeSound ();
		return SIS_FAILURE;
	}

	if (DS_OK != pDS->lpVtbl->SetCooperativeLevel (pDS, mainwindow, DSSCL_EXCLUSIVE)) {
		Com_Printf ("Set coop level failed\n");
		FreeSound ();
		return SIS_FAILURE;
	}

	// get access to the primary buffer, if possible, so we can set the sound hardware format
	memset (&dsbuf, 0, sizeof(dsbuf));
	dsbuf.dwSize = sizeof(DSBUFFERDESC);
	dsbuf.dwFlags = DSBCAPS_PRIMARYBUFFER;
	dsbuf.dwBufferBytes = 0;
	dsbuf.lpwfxFormat = NULL;

	memset(&dsbcaps, 0, sizeof(dsbcaps));
	dsbcaps.dwSize = sizeof(dsbcaps);
	primary_format_set = false;

	if (!COM_CheckParm ("-snoforceformat")) {
		if (DS_OK == pDS->lpVtbl->CreateSoundBuffer(pDS, &dsbuf, &pDSPBuf, NULL)) {
			pformat = format;

			if (DS_OK != pDSPBuf->lpVtbl->SetFormat (pDSPBuf, &pformat)) {
//				if (snd_firsttime)
//					Com_Printf ("Set primary sound buffer format: no\n");
			} else {
//				if (snd_firsttime)
//					Com_Printf ("Set primary sound buffer format: yes\n");

				primary_format_set = true;
			}
		}
	}

	if (!primary_format_set || !COM_CheckParm ("-primarysound")) {
		// create the secondary buffer we'll actually work with
		memset (&dsbuf, 0, sizeof(dsbuf));
		dsbuf.dwSize = sizeof(DSBUFFERDESC);
		dsbuf.dwFlags = DSBCAPS_CTRLFREQUENCY | DSBCAPS_LOCSOFTWARE;
		dsbuf.dwBufferBytes = SECONDARY_BUFFER_SIZE;
		dsbuf.lpwfxFormat = &format;

		memset(&dsbcaps, 0, sizeof(dsbcaps));
		dsbcaps.dwSize = sizeof(dsbcaps);

		if (DS_OK != pDS->lpVtbl->CreateSoundBuffer(pDS, &dsbuf, &pDSBuf, NULL)) {
			Com_Printf ("DS:CreateSoundBuffer Failed");
			FreeSound ();
			return SIS_FAILURE;
		}

		shm->channels = format.nChannels;
		shm->samplebits = format.wBitsPerSample;
		shm->speed = format.nSamplesPerSec;

		if (DS_OK != pDSBuf->lpVtbl->GetCaps (pDSBuf, &dsbcaps)) {
			Com_Printf ("DS:GetCaps failed\n");
			FreeSound ();
			return SIS_FAILURE;
		}

//		if (snd_firsttime)
//			Com_Printf ("Using secondary sound buffer\n");
	} else {
		if (DS_OK != pDS->lpVtbl->SetCooperativeLevel (pDS, mainwindow, DSSCL_WRITEPRIMARY)) {
			Com_Printf ("Set coop level failed\n");
			FreeSound ();
			return SIS_FAILURE;
		}

		if (DS_OK != pDSPBuf->lpVtbl->GetCaps (pDSPBuf, &dsbcaps)) {
			Com_Printf ("DS:GetCaps failed\n");
			return SIS_FAILURE;
		}

		pDSBuf = pDSPBuf;
//		Com_Printf ("Using primary sound buffer\n");
	}

	// Make sure mixer is active
	pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);

/*	if (snd_firsttime)
		Com_Printf ("   %d channel(s)\n"
		               "   %d bits/sample\n"
					   "   %d bytes/sec\n",
					   shm->channels, shm->samplebits, shm->speed);*/
	
	gSndBufSize = dsbcaps.dwBufferBytes;

// initialize the buffer
	reps = 0;

	while ((hresult = pDSBuf->lpVtbl->Lock(pDSBuf, 0, gSndBufSize, &lpData, &dwSize, NULL, NULL, 0)) != DS_OK) {
		if (hresult != DSERR_BUFFERLOST) {
			Com_Printf ("SNDDMA_InitDirect: DS::Lock Sound Buffer Failed\n");
			FreeSound ();
			return SIS_FAILURE;
		}

		if (++reps > 10000) {
			Com_Printf ("SNDDMA_InitDirect: DS: couldn't restore buffer\n");
			FreeSound ();
			return SIS_FAILURE;
		}

	}

	memset(lpData, 0, dwSize);
//		lpData[4] = lpData[5] = 0x7f;	// force a pop for debugging

	pDSBuf->lpVtbl->Unlock(pDSBuf, lpData, dwSize, NULL, 0);

	/* we don't want anyone to access the buffer directly w/o locking it first. */
	lpData = NULL; 

	pDSBuf->lpVtbl->Stop(pDSBuf);
	pDSBuf->lpVtbl->GetCurrentPosition(pDSBuf, &mmstarttime.u.sample, &dwWrite);
	pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);

	shm->soundalive = true;
	shm->splitbuffer = false;
	shm->samples = gSndBufSize/(shm->samplebits/8);
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = (unsigned char *) lpData;
	sample16 = (shm->samplebits/8) - 1;

	dsound_init = true;

	return SIS_SUCCESS;
}

//Crappy windows multimedia base
qboolean SNDDMA_InitWav (void) {
	WAVEFORMATEX format; 
	int i;
	HRESULT hr;
	UINT_PTR devicenum;
	int temp;
	
	snd_sent = 0;
	snd_completed = 0;

	shm = &sn;

	shm->channels = 2;
	shm->samplebits = 16;
	shm->speed = (s_khz.value == 44) ? 44100 : (s_khz.value == 22) ? 22050 : 11025;

	memset (&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = shm->channels;
	format.wBitsPerSample = shm->samplebits;
	format.nSamplesPerSec = shm->speed;
	format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
	format.cbSize = 0;
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign; 

	devicenum = WAVE_MAPPER;
	if ((temp = COM_CheckParm("-snddev")) && temp + 1 < com_argc)
		devicenum = Q_atoi(com_argv[temp + 1]);

	hr = waveOutOpen((LPHWAVEOUT) &hWaveOut, devicenum, &format, 0, 0L, CALLBACK_NULL);
	if (hr != MMSYSERR_NOERROR && devicenum != WAVE_MAPPER) {
		Com_Printf ("Couldn't open preferred sound device. Falling back to primary sound device.\n");
		hr = waveOutOpen((LPHWAVEOUT)&hWaveOut, WAVE_MAPPER, &format, 0, 0L, CALLBACK_NULL);
	}
	
	/* Open a waveform device for output using window callback. */ 
	if (hr != MMSYSERR_NOERROR) {
		if (hr == MMSYSERR_ALLOCATED)
			Com_Printf ("waveOutOpen failed, hardware already in use\n");
		else
			Com_Printf ("waveOutOpen failed\n");

		return false;
	} 

	/* 
	 * Allocate and lock memory for the waveform data. The memory 
	 * for waveform data must be globally allocated with 
	 * GMEM_MOVEABLE and GMEM_SHARE flags. 
	*/ 
	gSndBufSize = WAV_BUFFERS*WAV_BUFFER_SIZE;
	hData = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, gSndBufSize); 
	if (!hData) { 
		Com_Printf ("Sound: Out of memory.\n");
		FreeSound ();
		return false; 
	}
	lpData = GlobalLock(hData);
	if (!lpData) { 
		Com_Printf ("Sound: Failed to lock.\n");
		FreeSound ();
		return false; 
	} 
	memset (lpData, 0, gSndBufSize);

	/* 
	 * Allocate and lock memory for the header. This memory must 
	 * also be globally allocated with GMEM_MOVEABLE and 
	 * GMEM_SHARE flags. 
	 */ 
	hWaveHdr = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, 
		(DWORD) sizeof(WAVEHDR) * WAV_BUFFERS); 

	if (hWaveHdr == NULL) { 
		Com_Printf ("Sound: Failed to Alloc header.\n");
		FreeSound ();
		return false; 
	} 

	lpWaveHdr = (LPWAVEHDR) GlobalLock(hWaveHdr); 

	if (lpWaveHdr == NULL) { 
		Com_Printf ("Sound: Failed to lock header.\n");
		FreeSound ();
		return false; 
	}

	memset (lpWaveHdr, 0, sizeof(WAVEHDR) * WAV_BUFFERS);

	/* After allocation, set up and prepare headers. */ 
	for (i = 0; i < WAV_BUFFERS; i++) {
		lpWaveHdr[i].dwBufferLength = WAV_BUFFER_SIZE; 
		lpWaveHdr[i].lpData = lpData + i*WAV_BUFFER_SIZE;

		if (waveOutPrepareHeader(hWaveOut, lpWaveHdr+i, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
			Com_Printf ("Sound: failed to prepare wave headers\n");
			FreeSound ();
			return false;
		}
	}

	shm->soundalive = true;
	shm->splitbuffer = false;
	shm->samples = gSndBufSize/(shm->samplebits/8);
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = (unsigned char *) lpData;
	sample16 = (shm->samplebits/8) - 1;

	wav_init = true;

	return true;
}

//Try to find a sound device to mix for.
//Returns false if nothing is found.
int SNDDMA_Init(void) {
	sndinitstat	stat;

	if (COM_CheckParm ("-wavonly"))
		wavonly = true;

	dsound_init = wav_init = 0;

	stat = SIS_FAILURE;	// assume DirectSound won't initialize

	/* Init DirectSound */
	if (!wavonly) {
		if (snd_firsttime || snd_isdirect) {
			stat = SNDDMA_InitDirect ();

			if (stat == SIS_SUCCESS) {
				snd_isdirect = true;

				if (snd_firsttime)
					Com_Printf ("DirectSound initialized\n");
			} else {
				snd_isdirect = false;
				Com_Printf ("DirectSound failed to init\n");
			}
		}
	}

	// if DirectSound didn't succeed in initializing, try to initialize waveOut sound
	if (!dsound_init) {
		if (snd_firsttime || snd_iswave) {
			snd_iswave = SNDDMA_InitWav ();

			if (snd_iswave) {
				if (snd_firsttime)
					Com_Printf ("Wave sound initialized\n");
			} else {
				Com_Printf ("Wave sound failed to init\n");
			}
		}
	}

	snd_firsttime = false;

	if (!dsound_init && !wav_init) {
		if (snd_firsttime)
			Com_Printf ("No sound device initialized\n");

		return 0;
	}

	return 1;
}

//return the current sample position (in mono samples read) inside the recirculating dma buffer,
//so the mixing code will know how many sample are required to fill it up.
int SNDDMA_GetDMAPos(void) {
	MMTIME mmtime;
	int s;
	DWORD dwWrite;

	if (dsound_init) {
		mmtime.wType = TIME_SAMPLES;
		pDSBuf->lpVtbl->GetCurrentPosition(pDSBuf, &mmtime.u.sample, &dwWrite);
		s = mmtime.u.sample - mmstarttime.u.sample;
	} else if (wav_init) {
		s = snd_sent * WAV_BUFFER_SIZE;
	}

	s >>= sample16;

	s &= (shm->samples-1);

	return s;
}

//Send sound to device if buffer isn't really the dma buffer
void SNDDMA_Submit(void) {
	LPWAVEHDR h;
	int wResult;

	if (!wav_init)
		return;

	// find which sound blocks have completed
	while (1) {
		if ( snd_completed == snd_sent ) {
			Com_DPrintf ("Sound overrun\n");
			break;
		}

		if (!(lpWaveHdr[ snd_completed & WAV_MASK].dwFlags & WHDR_DONE))
			break;

		snd_completed++;	// this buffer has been played
	}

	// submit two new sound blocks
	while (((snd_sent - snd_completed) >> sample16) < 4) {
		h = lpWaveHdr + ( snd_sent&WAV_MASK );

		snd_sent++;
		/* 
		 * Now the data block can be sent to the output device. The 
		 * waveOutWrite function returns immediately and waveform 
		 * data is sent to the output device in the background. 
		 */ 
		wResult = waveOutWrite(hWaveOut, h, sizeof(WAVEHDR)); 

		if (wResult != MMSYSERR_NOERROR) { 
			Com_Printf ("Failed to write block to device\n");
			FreeSound ();
			return; 
		} 
	}
}

//Reset the sound device for exiting
void SNDDMA_Shutdown(void) {
	FreeSound ();
}
