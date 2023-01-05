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

extern cvar_t r_dynamic;
#define R_NoLighting()       (r_dynamic.integer == 0)
#define R_HardwareLighting() (r_dynamic.integer == 2 && R_UseModernOpenGL())
#define R_SoftwareLighting() (r_dynamic.integer && !R_HardwareLighting())

#endif // EZQUAKE_R_LOCAL_HEADER
