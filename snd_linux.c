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

    $Id: snd_linux.c,v 1.6 2006-04-28 23:38:29 disconn3ct Exp $
*/

#include "quakedef.h"

qbool SNDDMA_ALSA;
int snd_inited; // FIXME: get rid of it
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
	if (COM_CheckParm("-noalsa") || Cvar_VariableValue("s_noalsa")) {
		// User wants us to use OSS...
		SNDDMA_ALSA = false;
		Com_Printf("sound: Using OSS at user's request...\n");
		retval = SNDDMA_Init_OSS();
	} else {
		// Try ALSA first...
		Com_Printf("sound: Attempting to initialise ALSA...\n");
		retval = SNDDMA_Init_ALSA();
		if (retval) {
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
	if (snd_inited) {
		if (SNDDMA_ALSA)
			return SNDDMA_GetDMAPos_ALSA();
		else
			return SNDDMA_GetDMAPos_OSS();
	} else
		return 0;
}

void SNDDMA_Shutdown(void)
{
	if (snd_inited) {
		if (SNDDMA_ALSA)
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
	if (snd_inited) {
		if (SNDDMA_ALSA)
			SNDDMA_Submit_ALSA();
		// OSS doesn't use this so no need to call it.
	}
}
