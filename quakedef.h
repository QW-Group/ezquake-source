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
// quakedef.h -- primary header for client


//define PARANOID // speed sapping error checking

#ifndef  __QUAKEDEF_H__
#define  __QUAKEDEF_H__

#include "common.h"

#include "vid.h"
#include "screen.h"
#include "render.h"
#include "draw.h"
#include "console.h"
#include "cl_view.h"
#include "fs.h"

#include "client.h"

// particles
typedef enum trail_type_s {
	ROCKET_TRAIL, GRENADE_TRAIL, ALT_ROCKET_TRAIL, BLOOD_TRAIL, BIG_BLOOD_TRAIL,
	TRACER1_TRAIL, TRACER2_TRAIL, VOOR_TRAIL,
	//VULT PARTICLES
	RAIL_TRAIL,
	RAIL_TRAIL2,
	LAVA_TRAIL,
	AMF_ROCKET_TRAIL,
	BLEEDING_TRAIL,
	BLEEDING_TRAIL2,
} trail_type_t;

void R_InitParticles(void);
void R_ClearParticles(void);
void R_DrawParticles(void);
void R_ParticleFrame(void);
void R_ParticleEndFrame(void);
void R_ReadPointFile_f(void);
void R_RunParticleEffect(vec3_t, vec3_t, int, int);
void R_ParticleTrail(vec3_t start, vec3_t end, trail_type_t type);
void R_EntityParticleTrail(centity_t* cent, trail_type_t type);
void R_BlobExplosion(vec3_t);
void R_ParticleExplosion(vec3_t);
void R_LavaSplash(vec3_t);
void R_TeleportSplash(vec3_t);
void Classic_InitParticles(void);
void Classic_ClearParticles(void);
void Classic_RunParticleEffect(vec3_t org, vec3_t dir, int color, int count);
void Classic_ParticleTrail(vec3_t start, vec3_t end, vec3_t *, trail_type_t type);
void Classic_ParticleRailTrail(vec3_t start, vec3_t end, int color);
void Classic_BlobExplosion(vec3_t org);
void Classic_ParticleExplosion(vec3_t org);
void Classic_LavaSplash(vec3_t org);
void Classic_TeleportSplash(vec3_t org);

typedef enum {
	texture_type_2d,
	texture_type_2d_array,
	texture_type_cubemap,
	texture_type_count
} r_texture_type_id;

typedef enum {
	texture_minification_nearest,
	texture_minification_linear,
	texture_minification_nearest_mipmap_nearest,
	texture_minification_linear_mipmap_nearest,
	texture_minification_nearest_mipmap_linear,
	texture_minification_linear_mipmap_linear,

	texture_minification_count
} texture_minification_id;

typedef enum {
	texture_magnification_nearest,
	texture_magnification_linear,

	texture_magnification_count
} texture_magnification_id;

#endif /* !__QUAKEDEF_H__ */
