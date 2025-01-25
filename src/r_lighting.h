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

#ifndef EZQUAKE_R_LIGHTING_HEADER
#define EZQUAKE_R_LIGHTING_HEADER

extern cvar_t       gl_lightmode;
extern cvar_t       gl_modulate;
extern int          lightmode;              // set to gl_lightmode on mapchange

extern cvar_t       gl_flashblend;
extern cvar_t       gl_rl_globe;

// gl_rlight.c
void R_MarkLights(dlight_t *light, int bit, mnode_t *node);
void R_AnimateLight(void);
void R_RenderDlights(void);
void R_LightEntity(entity_t* ent);

extern unsigned int d_lightstylevalue[256];	// 8.8 fraction of base light value
static const float rgb9e5tab[32] = {
	//multipliers for the 9-bit mantissa, according to the biased mantissa
	//aka: pow(2, biasedexponent - bias-bits) where bias is 15 and bits is 9
	1.0/(1<<24), 1.0/(1<<23), 1.0/(1<<22), 1.0/(1<<21), 1.0/(1<<20), 1.0/(1<<19), 1.0/(1<<18), 1.0/(1<<17),
	1.0/(1<<16), 1.0/(1<<15), 1.0/(1<<14), 1.0/(1<<13), 1.0/(1<<12), 1.0/(1<<11), 1.0/(1<<10), 1.0/(1<<9),
	1.0/(1<<8),  1.0/(1<<7),  1.0/(1<<6),  1.0/(1<<5),  1.0/(1<<4),  1.0/(1<<3),  1.0/(1<<2),  1.0/(1<<1),
	1.0,         1.0*(1<<1),  1.0*(1<<2),  1.0*(1<<3),  1.0*(1<<4),  1.0*(1<<5),  1.0*(1<<6),  1.0*(1<<7),
};

extern cvar_t r_dynamic;
#define R_NoLighting()       (r_dynamic.integer == 0)
#define R_HardwareLighting() (r_dynamic.integer == 2 && R_UseModernOpenGL())
#define R_SoftwareLighting() (r_dynamic.integer && !R_HardwareLighting())

#endif // EZQUAKE_R_LOCAL_HEADER
