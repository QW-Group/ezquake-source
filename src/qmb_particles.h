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

#ifndef EZQUAKE_QMB_PARTICLES_HEADER
#define EZQUAKE_QMB_PARTICLES_HEADER

//====================================================
void QMB_InitParticles(void);
void QMB_ClearParticles(void);
void QMB_CalculateParticles(void);
void QMB_DrawParticles(void);
void QMB_ShutdownParticles(void);

void QMB_RunParticleEffect(vec3_t org, vec3_t dir, int color, int count);
void QMB_ParticleTrail(vec3_t start, vec3_t end, trail_type_t type);
void QMB_EntityParticleTrail(centity_t* cent, trail_type_t type);
void QMB_ParticleRailTrail(vec3_t start, vec3_t end, int color_num);
void QMB_BlobExplosion(vec3_t org);
void QMB_ParticleExplosion(vec3_t org);
void QMB_LavaSplash(vec3_t org);
void QMB_TeleportSplash(vec3_t org);

void QMB_DetpackExplosion(vec3_t org);

void QMB_InfernoFlame(vec3_t org);
void QMB_StaticBubble(entity_t *ent);

extern qbool qmb_initialized;
extern float particle_time;

#define MIN_ENTITY_PARTICLE_FRAMETIME (0.1)
#define ONE_FRAME_ONLY	(0.0001)

#define	INIT_NEW_PARTICLE(_pt, _p, _color, _size, _time) \
{ \
	_p = free_particles;								\
	free_particles = _p->next;							\
	_p->next = _pt->start;								\
	_pt->start = _p;									\
	_p->size = _size;									\
	_p->hit = 0;										\
	_p->start = r_refdef2.time;							\
	_p->die = _p->start + _time;						\
	_p->growth = 0;										\
	_p->rotspeed = 0;									\
	_p->texindex = (rand() % particle_textures[_pt->texture].components);	\
	_p->bounces = 0;									\
	_p->initial_alpha = _pt->startalpha;                \
	memcpy(_p->color, _color, sizeof(_p->color));		\
	_p->cached_contents = 0;                            \
	_p->cached_distance = 0;                            \
	VectorClear(_p->cached_movement);                   \
	_p->entity_ref = 0;                                 \
	_p->entity_trailnumber = 0;                         \
	ParticleStats(1); \
}

#endif
