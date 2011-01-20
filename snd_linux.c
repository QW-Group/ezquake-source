/*
(C) 2005 Contributors of the ZQuake Project

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

    $Id: snd_linux.c,v 1.13 2007-09-26 13:53:42 tonik Exp $
*/

#include "quakedef.h"
#include "qsound.h"

#ifndef __FreeBSD__

// Note: The functions here keep track of if the sound system is inited.
// They perform checks so that the real functions are only called if appropriate.

// Prototypes
//FIXME
/*qbool SNDDMA_Init_ALSA(sounddriver_t *sd);
int SNDDMA_GetDMAPos_ALSA(void);
void SNDDMA_Shutdown_ALSA(void);
void SNDDMA_Submit_ALSA(unsigned int count);*/
#endif

qbool SNDDMA_Init_OSS(void);
int SNDDMA_GetDMAPos_OSS(void);
void SNDDMA_Shutdown_OSS(void);

////////////////////////////
// external cvars
///////////////////////////

//FIXME
/*
extern cvar_t s_oss_device;
extern cvar_t s_alsa_device;*/


//========================================================================
// SOUND CAPTURING
//========================================================================

#ifdef FTE_PEXT2_VOICECHAT

typedef struct
{
	char	dummy; // just so its not empty.
} dsndcapture_t;

void *DSOUND_Capture_Init (int rate)
{
	dsndcapture_t *result;

	Com_DPrintf("DSOUND_Capture_Init: rate %d\n", rate);
	result = Z_Malloc(sizeof(*result));
	Com_DPrintf("DSOUND_Capture_Init: OK\n");
	return result;	
}

void DSOUND_Capture_Start(void *ctx)
{
	dsndcapture_t *c = ctx;
}

void DSOUND_Capture_Stop(void *ctx)
{
	dsndcapture_t *c = ctx;
}

void DSOUND_Capture_Shutdown(void *ctx)
{
	dsndcapture_t *c = ctx;
	Z_Free(ctx);
}

unsigned int DSOUND_Capture_Update(void *ctx, unsigned char *buffer, unsigned int minbytes, unsigned int maxbytes)
{
	dsndcapture_t *c = ctx;
	return 0; // how much data is in buffer.
}

snd_capture_driver_t DSOUND_Capture =
{
	DSOUND_Capture_Init,
	DSOUND_Capture_Start,
	DSOUND_Capture_Update,
	DSOUND_Capture_Stop,
	DSOUND_Capture_Shutdown
};

#endif // FTE_PEXT2_VOICECHAT

