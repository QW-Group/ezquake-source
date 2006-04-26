// Sound Renderer
// Uses either ALSA or OSS as back-end
// (C) 2005  Contributors of the ZQuake Project
// Licenced under the GNU General Public Licence v2.
// See `COPYING' for details.

#include "quakedef.h"

qbool SNDDMA_ALSA;
int snd_inited;
// Note: The functions here keep track of if the sound system is inited.
//       They perform checks so that the real functions are only called if
//       appropriate.


// Prototypes

qbool SNDDMA_Init_ALSA(void);
qbool SNDDMA_Init_OSS(void);
int SNDDMA_GetDMAPos_ALSA(void);
int SNDDMA_GetDMAPos_OSS(void);
void SNDDMA_Shutdown_ALSA(void);
void SNDDMA_Shutdown_OSS(void);
void SNDDMA_Submit_ALSA(void);
void SNDDMA_Submit_OSS(void);


// Main functions

qbool SNDDMA_Init(void)
{
	int retval;

	// Give user the option to force OSS...
	if(COM_CheckParm("-noalsa")) /* || Cvar_VariableValue("s_noalsa"))*/ {
		// User wants us to use OSS...
		Com_Printf("sound: Using OSS at user's request...\n");
		retval = SNDDMA_Init_OSS();
	} else {
		// Try ALSA first...
		Com_Printf("sound: Attempting to initialise ALSA...\n");
		retval = SNDDMA_Init_ALSA();
		if( retval ) {
			SNDDMA_ALSA = true;
		} else {
			// Fall back to OSS...
			SNDDMA_ALSA = false;
			Com_Printf("sound: Falling back to OSS...\n");
			retval = SNDDMA_Init_OSS();
		}
	}

	snd_inited = retval;
	return retval;
}

int SNDDMA_GetDMAPos(void)
{
	if( snd_inited ) {
		if( SNDDMA_ALSA )
			return SNDDMA_GetDMAPos_ALSA();
		else
			return SNDDMA_GetDMAPos_OSS();
	} else
		return 0;
}

void SNDDMA_Shutdown(void)
{
	if (snd_inited) {
		if( SNDDMA_ALSA )
			SNDDMA_Shutdown_ALSA();
		else
			SNDDMA_Shutdown_OSS();

		snd_inited = 0;
	}
}

/*
==============
SNDDMA_Submit
 
Send sound to device if buffer isn't really the dma buffer
===============
*/
void SNDDMA_Submit(void)
{
	if( snd_inited ) {
		if( SNDDMA_ALSA )
			SNDDMA_Submit_ALSA();
		// OSS doesn't use this so no need to call it.
	}
}
