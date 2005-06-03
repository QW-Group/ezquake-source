// disconnect: someday here will be some code =:-)

#include <stdio.h>

#include "quakedef.h"
#include "sound.h"

static int			 snd_inited = 0;

qboolean SNDDMA_Init_ALSA(void)
{
	snd_inited = 0;
	return 0;
}


int SNDDMA_GetDMAPos_ALSA(void)
{
	return (snd_inited);
}


void SNDDMA_Shutdown_ALSA(void)
{
	snd_inited = 0;
}


/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void SNDDMA_Submit_ALSA(void)
{
}

