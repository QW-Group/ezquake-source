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
// d_init.c: rasterization driver initialization

#include "quakedef.h"
#include "d_local.h"

#define NUM_MIPS	4

cvar_t	d_subdiv16 = {"d_subdiv16", "1"};
cvar_t	d_mipcap = {"d_mipcap", "0"};
cvar_t	d_mipscale = {"d_mipscale", "1"};

surfcache_t		*d_initial_rover;
qboolean		d_roverwrapped;
int				d_minmip;
float			d_scalemip[NUM_MIPS-1];

static float	basemip[NUM_MIPS-1] = {1.0, 0.5*0.8, 0.25*0.8};

extern int		d_aflatcolor;

void (*d_drawspans) (espan_t *pspan);

void D_Init (void) {
	Cvar_SetCurrentGroup(CVAR_GROUP_TEXTURES);
	Cvar_Register (&d_subdiv16);
	Cvar_Register (&d_mipcap);
	Cvar_Register (&d_mipscale);

	Cvar_ResetCurrentGroup();

	r_aliasuvscale = 1.0;
}

void D_EnableBackBufferAccess (void) {
	VID_LockBuffer ();
}

void D_DisableBackBufferAccess (void) {
	VID_UnlockBuffer ();
}

void D_SetupFrame (void) {
	int i;

	d_viewbuffer = r_dowarp ? r_warpbuffer : (void *) vid.buffer;

	screenwidth = r_dowarp ? WARP_WIDTH : vid.rowbytes;

	d_roverwrapped = false;
	d_initial_rover = sc_rover;

	d_minmip = bound(0, d_mipcap.value, 3);

	for (i = 0; i < NUM_MIPS - 1; i++)
		d_scalemip[i] = basemip[i] * d_mipscale.value;

#if	id386
	if (d_subdiv16.value )
		d_drawspans = D_DrawSpans16;
	else
#endif
		d_drawspans = D_DrawSpans8;

	d_aflatcolor = 0;
}
