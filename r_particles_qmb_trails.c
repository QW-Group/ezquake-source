/*
Copyright (C) 2002-2003, Dr Labman, A. Nourai

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
#include "gl_model.h"
#include "vx_stuff.h"
#include "r_particles_qmb.h"
#include "pmove.h"
#include "utils.h"
#include "qmb_particles.h"
#include "r_brushmodel.h" // R_PointIsUnderwater only

//#define qmb_end(cent) ((cent)->current.origin)
#define qmb_end(cent) ((cent)->lerp_origin)

extern cvar_t gl_part_tracer1_color, gl_part_tracer1_size, gl_part_tracer1_time;
extern cvar_t gl_part_tracer2_color, gl_part_tracer2_size, gl_part_tracer2_time;

__inline static void AddParticleTrailImpl(part_type_t type, vec3_t start, vec3_t end, float size, float time, col_t col, centity_t* cent, int trail_index)
{
	byte *color;
	int i, j, num_particles;
	float count = 0.0, theta = 0.0;
	vec3_t point, delta;
	particle_t *p;
	particle_type_t *pt;
	//VULT PARTICLES - for railtrail
	int loops = 0, entity_ref;
	vec3_t vf, vr, radial;

	if (!qmb_initialized) {
		Sys_Error("QMB particle added without initialization");
	}

	entity_ref = cent ? (cent - cl_entities) + 1 : 0;
	entity_ref = bound(0, entity_ref, CL_MAX_EDICTS);

	if (time == 0 || size == 0.0f) {
		return;
	}

	if (type >= num_particletypes) {
		Sys_Error("AddParticle: Invalid type (%d)", type);
	}

	pt = &particle_types[particle_type_index[type]];

	VectorCopy(start, point);
	VectorSubtract(end, start, delta);
	if (pt->drawtype == pd_dynamictrail) {
		count = 1;
	}
	else {
		float length = VectorLength(delta);
		if (length < 1) {
			goto done;
		}

		// length > 140 was old limit in cl_ents.c
		if (length > 140) {
			VectorCopy(end, point);
			goto done;
		}

		switch (type) {
			case p_alphatrail:
			case p_lavatrail:
			case p_bleedspike:
			case p_bubble:
				count = length / 1.1;
				break;
			case p_bubble2:
				count = length / 5;
				break;
			case p_trailpart:
				count = length / 1.1;
				break;
			case p_blood3:
				count = length / 8;
				break;
			case p_smoke:
			case p_vxrocketsmoke:
				count = length / 3.8;
				break;
			case p_dpsmoke:
				count = length / 2.5;
				break;
			case p_dpfire:
				count = length / 2.8;
				break;
				//VULT PARTICLES
			case p_railtrail:
				count = length * 1.6;
				if (!(loops = (int)length / 13.0)) {
					goto done;
				}
				VectorScale(delta, 1 / length, vf);
				VectorVectors(vf, vr, vup);
				break;
			case p_trailbleed:
				count = length / 1.1;
				break;
			default:
				Com_DPrintf("AddParticleTrail: unexpected type %d\n", type);
				break;
		}
	}

	if (!(num_particles = (int)count)) {
		goto done;
	}

	if (!free_particles) {
		// we hit limit, don't try and draw from old position if this resolves
		VectorCopy(end, point);
		goto done;
	}

	VectorScale(delta, 1.0 / num_particles, delta);
	for (i = 0; i < num_particles && free_particles; i++) {
		color = col ? col : ColorForParticle(type);
		INIT_NEW_PARTICLE(pt, p, color, size, time);
		p->entity_ref = entity_ref;
		p->entity_trailindex = trail_index;
		if (entity_ref) {
			p->entity_trailnumber = cent->trail_number;
		}

		if (pt->drawtype == pd_dynamictrail) {
			VectorCopy(start, p->org);
			VectorCopy(end, p->endorg);
		}
		else {
			switch (type) {
				//VULT PARTICLES
				case p_trailpart:
				case p_alphatrail:
					VectorCopy(point, p->org);
					VectorClear(p->vel);
					p->growth = -size / time;
					break;
				case p_blood3:
					VectorCopy(point, p->org);
					for (j = 0; j < 3; j++) {
						p->org[j] += ((rand() & 15) - 8) / 8.0;
					}
					for (j = 0; j < 3; j++) {
						p->vel[j] = ((rand() & 15) - 8) / 2.0;
					}
					p->size = size * (rand() % 20) / 10.0;
					p->growth = 6;
					break;
				case p_smoke:
					VectorCopy(point, p->org);
					for (j = 0; j < 3; j++) {
						p->org[j] += ((rand() & 7) - 4) / 8.0;
					}
					p->vel[0] = p->vel[1] = 0;
					p->vel[2] = rand() & 3;
					p->growth = 4.5;
					p->rotspeed = (rand() & 63) + 96;
					break;
				case p_dpsmoke:
					VectorCopy(point, p->org);
					for (j = 0; j < 3; j++) {
						p->vel[j] = (rand() % 10) - 5;
					}
					p->growth = 3;
					p->rotspeed = (rand() & 63) + 96;
					break;
				case p_dpfire:
					VectorCopy(point, p->org);
					for (j = 0; j < 3; j++) {
						p->vel[j] = (rand() % 40) - 20;
					}
					break;
					//VULT PARTICLES
				case p_railtrail:
					theta += loops * 2 * M_PI / count;
					for (j = 0; j < 3; j++) {
						radial[j] = vr[j] * cos(theta) + vup[j] * sin(theta);
					}
					VectorMA(point, 2.6, radial, p->org);
					for (j = 0; j < 3; j++) {
						p->vel[j] = radial[j] * 5;
					}
					break;
					//VULT PARTICLES
				case p_bubble:
				case p_bubble2:
					VectorCopy(point, p->org);
					for (j = 0; j < 3; j++) {
						p->vel[j] = (rand() % 10) - 5;
					}
					break;
					//VULT PARTICLES
				case p_lavatrail:
					VectorCopy(point, p->org);
					for (j = 0; j < 3; j++) {
						p->org[j] += ((rand() & 7) - 4);
					}
					p->vel[0] = p->vel[1] = 0;
					p->vel[2] = rand() & 3;
					break;
					//VULT PARTICLES
				case p_vxrocketsmoke:
					VectorCopy(point, p->org);
					for (j = 0; j < 3; j++) {
						p->vel[j] = (rand() % 8) - 4;
					}
					break;
					//VULT PARTICLES
				case p_trailbleed:
					p->size = (rand() % (int)size);
					p->growth = -rand() % 5;
					VectorCopy(point, p->org);
					break;
					//VULT PARTICLES
				case p_bleedspike:
					size = 9 * size / 10;
					VectorCopy(point, p->org);
					break;
				default:
					//assert(!"AddParticleTrail: unexpected type"); -> hexum - FIXME not all types are handled, seems to work ok though
					break;
			}
		}

		VectorAdd(point, delta, point);
	}
done:
	if (cent) {
		VectorCopy(point, cent->trails[trail_index].stop);
	}
}

__inline static void AddEntityParticleTrail(part_type_t type, centity_t* cent, int trail_index, float size, float time, col_t color)
{
	AddParticleTrailImpl(type, cent->trails[trail_index].stop, qmb_end(cent), size, time, color, cent, trail_index);
}

__inline static void AddParticleTrail(part_type_t type, vec3_t start, vec3_t stop, float size, float time, col_t color)
{
	AddParticleTrailImpl(type, start, stop, size, time, color, NULL, 0);
}

//VULT PARTICLES
//VULT - Draw a thin white trail that could be used to represent motion for just about anything
void ParticleAlphaTrail(centity_t* cent, float size, float life)
{
	if (amf_underwater_trails.integer && R_PointIsUnderwater(cent->trails[0].stop)) {
		AddEntityParticleTrail(amf_nailtrail_water.integer ? p_bubble2 : p_bubble, cent, 0, 1.5, 0.825, NULL);
	}
	else {
		col_t color = { 10, 10, 10, 0 };

		AddEntityParticleTrail(p_alphatrail, cent, 0, size, life, color);
	}
}

//MEAG: Draw thin trail for nails etc
void ParticleNailTrail(centity_t* client_ent, float size, float life)
{
	if (amf_underwater_trails.integer && R_PointIsUnderwater(client_ent->trails[0].stop)) {
		AddEntityParticleTrail(amf_nailtrail_water.integer ? p_bubble2 : p_bubble, client_ent, 0, 1.5, 0.825, NULL);
	}
	else {
		if (particle_time - client_ent->trails[0].lasttime < MIN_ENTITY_PARTICLE_FRAMETIME) {
			return;
		}
		client_ent->trails[0].lasttime = particle_time;
		AddEntityParticleTrail(p_nailtrail, client_ent, 0, size, life, NULL);
	}
}

//VULT PARTICLES
void FireballTrail(centity_t* cent, byte col[3], float size, float life)
{
	col_t color;

	color[0] = col[0];
	color[1] = col[1];
	color[2] = col[2];
	color[3] = 255;

	//head
	AddEntityParticleTrail(p_trailpart, cent, 0, size * 7, 0.15, color);

	//head-white part
	color[0] = 255; color[1] = 255; color[2] = 255;
	AddEntityParticleTrail(p_trailpart, cent, 1, size * 5, 0.15, color);

	//medium trail
	color[0] = col[0];
	color[1] = col[1];
	color[2] = col[2];
	AddEntityParticleTrail(p_trailpart, cent, 2, size * 3, life, color);
}

//VULT PARTICLES
void FireballTrailWave(centity_t* cent, byte col[3], float size, float life, vec3_t angle)
{
	int i, j;
	vec3_t dir, vec;
	col_t color;
	color[0] = col[0];
	color[1] = col[1];
	color[2] = col[2];
	color[3] = 255;

	AngleVectors(angle, vec, NULL, NULL);
	vec[2] *= -1;
	FireballTrail(cent, col, size, life);
	if (cent->trails[2].lasttime == particle_time) {
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 3; j++) {
				dir[j] = vec[j] * -600 + ((rand() % 100) - 50);
			}
			AddParticle(p_streakwave, qmb_end(cent), 1, 1, 1, color, dir);
		}
	}
}

//VULT PARTICLES
void FuelRodGunTrail(centity_t* cent)
{
	col_t color;
	int i, j;
	vec3_t dir, vec;
	color[3] = 255;

	color[0] = 0; color[1] = 255; color[2] = 0;
	AddEntityParticleTrail(p_trailpart, cent, 0, 15, 0.2, color);
	color[0] = 255; color[1] = 255; color[2] = 255;
	AddEntityParticleTrail(p_trailpart, cent, 1, 10, 0.2, color);
	color[0] = 0; color[1] = 128; color[2] = 0;
	AddEntityParticleTrail(p_trailpart, cent, 2, 10, 0.5, color);
	color[0] = 0; color[1] = 22; color[2] = 0;
	AddEntityParticleTrail(p_trailpart, cent, 3, 2, 3, color);
	if (cent->trails[3].lasttime == particle_time) {
		AngleVectors(cent->current.angles, vec, NULL, NULL);
		color[0] = 75; color[1] = 255; color[2] = 75;
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 3; j++) {
				dir[j] = vec[j] * -300 + ((rand() % 100) - 50);
			}
			dir[2] = dir[2] + 60;
			if (amf_part_trailtype.integer == 2) {
				AddParticle(p_smallspark, qmb_end(cent), 1, 1, 2, color, dir);
			}
			else {
				AddParticle(p_streak, qmb_end(cent), 1, 1, 2, color, dir);
			}
		}
	}
}

//VULT PARTICLES
//VULT - These trails were my initial motivation behind AMFQUAKE.
void VX_ParticleTrail(vec3_t start, vec3_t end, float size, float time, col_t color)
{
	vec3_t		vec;
	float		len;
	static vec3_t zerodir = { 22, 22, 22 };

	time *= amf_part_traillen.value;
	if (!time) {
		return;
	}

	if (amf_part_fulldetail.integer) {
		VectorSubtract(end, start, vec);
		len = VectorNormalize(vec);

		while (len > 0) {
			//ADD PARTICLE HERE
			AddParticle(p_streaktrail, start, 1, size, time, color, zerodir);
			len--;
			VectorAdd(start, vec, start);
		}
	}
	AddParticle(p_streaktrail, start, 1, size, time, color, end);
}

void QMB_ParticleTrail(vec3_t start, vec3_t end, trail_type_t type)
{
	col_t color;

	switch (type) {
		case GRENADE_TRAIL:
			//VULT PARTICLES
			if (amf_underwater_trails.integer && R_PointIsUnderwater(start)) {
				AddParticleTrail(p_bubble, start, end, 1.8, 0.825, NULL);
			}
			else {
				AddParticleTrail(p_smoke, start, end, 1.45, 0.825, NULL);
			}
			break;
		case BLOOD_TRAIL:
		case BIG_BLOOD_TRAIL:
			AddParticleTrail(p_blood3, start, end, type == BLOOD_TRAIL ? 1.35 : 2.4, 2, NULL);
			break;
		case TRACER1_TRAIL:
			AddParticleTrail(p_trailpart, start, end, gl_part_tracer1_size.value, gl_part_tracer1_time.value, gl_part_tracer1_color.color);
			break;
		case TRACER2_TRAIL:
			AddParticleTrail(p_trailpart, start, end, gl_part_tracer2_size.value, gl_part_tracer2_time.value, gl_part_tracer2_color.color);
			break;
		case VOOR_TRAIL:
			color[0] = 77; color[1] = 0; color[2] = 255; color[3] = 255;
			AddParticleTrail(p_trailpart, start, end, 3.75, 0.5, color);
			break;
		case ALT_ROCKET_TRAIL:
			if (amf_underwater_trails.integer && R_PointIsUnderwater(start)) {
				AddParticleTrail(p_bubble, start, end, 1.8, 0.825, NULL);
			}
			else {
				AddParticleTrail(p_dpfire, start, end, 3, 0.26, NULL);
				AddParticleTrail(p_dpsmoke, start, end, 3, 0.825, NULL);
			}
			break;
			//VULT TRAILS
		case RAIL_TRAIL:
		case RAIL_TRAIL2:
			color[0] = 255; color[1] = 255; color[2] = 255; color[3] = 255;
			//VULT PARTICLES
			AddParticleTrail(p_alphatrail, start, end, 2, 0.525, color);
			if (type == RAIL_TRAIL2) {
				color[0] = 255;
				color[1] = 0;
				color[2] = 0;
			}
			else {
				color[0] = 0;
				color[1] = 0;
				color[2] = 255;
			}
			AddParticleTrail(p_railtrail, start, end, 1.3, 0.525, color);
			break;
			//VULT TRAILS
		case LAVA_TRAIL:
			if (amf_underwater_trails.integer && R_PointIsUnderwater(start)) {
				AddParticleTrail(p_bubble, start, end, 1.8, 0.825, NULL);
				color[0] = 25;
				color[1] = 102;
				color[2] = 255;
				color[3] = 255;
			}
			else {
				color[0] = 255;
				color[1] = 102;
				color[2] = 25;
				color[3] = 255;
			}
			AddParticleTrail(p_lavatrail, start, end, 5, 1, color);
			break;
			//VULT TRAILS
		case AMF_ROCKET_TRAIL:
			if (amf_underwater_trails.integer && R_PointIsUnderwater(start)) {
				AddParticleTrail(p_bubble, start, end, 1.8, 0.825, NULL);
			}
			else {
				color[0] = 128; color[1] = 128; color[2] = 128; color[3] = 255;
				AddParticleTrail(p_alphatrail, start, end, 2, 0.6, color);
				AddParticleTrail(p_vxrocketsmoke, start, end, 3, 0.5, color);

				color[0] = 128; color[1] = 56; color[2] = 9; color[3] = 255;
				AddParticleTrail(p_trailpart, start, end, 4, 0.2, color);
			}
			break;
			//VULT PARTICLES
		case BLEEDING_TRAIL:
		case BLEEDING_TRAIL2:
			if (amf_part_blood_color.value == 2) {
				color[0] = 55; color[1] = 102; color[2] = 255;
			}
			//green
			else if (amf_part_blood_color.value == 3) {
				color[0] = 80; color[1] = 255; color[2] = 80;
			}
			//red
			else {
				color[0] = 128; color[1] = 0; color[2] = 0;
			}
			color[3] = 255;
			AddParticleTrail(type == BLEEDING_TRAIL ? p_trailbleed : p_bleedspike, start, end, 4, type == BLEEDING_TRAIL ? 0.5 : 0.2, color);
			break;
		case ROCKET_TRAIL:
		default:
			color[0] = 255; color[1] = 56; color[2] = 9; color[3] = 255;
			AddParticleTrail(p_trailpart, start, end, 6.2, 0.31, color);
			AddParticleTrail(p_smoke, start, end, 1.8, 0.825, NULL);
			break;
	}
}

void QMB_EntityParticleTrail(centity_t* cent, trail_type_t type)
{
	col_t color;

	switch (type) {
		case GRENADE_TRAIL:
			//VULT PARTICLES
			if (amf_underwater_trails.integer && R_PointIsUnderwater(cent->trails[0].stop)) {
				AddEntityParticleTrail(p_bubble, cent, 0, 1.8, 0.825, NULL);
			}
			else {
				AddEntityParticleTrail(p_smoke, cent, 0, 1.45, 0.825, NULL);
			}
			break;
		case BLOOD_TRAIL:
		case BIG_BLOOD_TRAIL:
			AddEntityParticleTrail(p_blood3, cent, 0, type == BLOOD_TRAIL ? 1.35 : 2.4, 2, NULL);
			break;
		case TRACER1_TRAIL:
			AddEntityParticleTrail(p_trailpart, cent, 0, gl_part_tracer1_size.value, gl_part_tracer1_time.value, gl_part_tracer1_color.color);
			break;
		case TRACER2_TRAIL:
			AddEntityParticleTrail(p_trailpart, cent, 0, gl_part_tracer2_size.value, gl_part_tracer2_time.value, gl_part_tracer2_color.color);
			break;
		case VOOR_TRAIL:
			color[0] = 77; color[1] = 0; color[2] = 255; color[3] = 255;
			AddEntityParticleTrail(p_trailpart, cent, 0, 3.75, 0.5, color);
			break;
		case ALT_ROCKET_TRAIL:
			if (amf_underwater_trails.integer && R_PointIsUnderwater(qmb_end(cent))) {
				AddEntityParticleTrail(p_bubble, cent, 0, 1.8, 0.825, NULL);
			}
			else {
				AddEntityParticleTrail(p_dpfire, cent, 0, 3, 0.26, NULL);
				AddEntityParticleTrail(p_dpsmoke, cent, 1, 3, 0.825, NULL);
			}
			break;
			//VULT TRAILS
		case RAIL_TRAIL:
		case RAIL_TRAIL2:
			color[0] = 255; color[1] = 255; color[2] = 255; color[3] = 255;
			//VULT PARTICLES
			AddEntityParticleTrail(p_alphatrail, cent, 0, 2, 0.525, color);
			if (type == RAIL_TRAIL2) {
				color[0] = 255;
				color[1] = 0;
				color[2] = 0;
			}
			else {
				color[0] = 0;
				color[1] = 0;
				color[2] = 255;
			}
			AddEntityParticleTrail(p_railtrail, cent, 1, 1.3, 0.525, color);
			break;
			//VULT TRAILS
		case LAVA_TRAIL:
			if (amf_underwater_trails.integer && R_PointIsUnderwater(qmb_end(cent))) {
				AddEntityParticleTrail(p_bubble, cent, 0, 1.8, 0.825, NULL);
				color[0] = 25;
				color[1] = 102;
				color[2] = 255;
				color[3] = 255;
			}
			else {
				color[0] = 255;
				color[1] = 102;
				color[2] = 25;
				color[3] = 255;
			}
			AddEntityParticleTrail(p_lavatrail, cent, 1, 5, 1, color);
			break;
			//VULT TRAILS
		case AMF_ROCKET_TRAIL:
			if (amf_underwater_trails.integer && R_PointIsUnderwater(qmb_end(cent))) {
				AddEntityParticleTrail(p_bubble, cent, 0, 1.8, 0.825, NULL);
			}
			else {
				color[0] = 128; color[1] = 128; color[2] = 128; color[3] = 255;
				AddEntityParticleTrail(p_alphatrail, cent, 0, 2, 0.6, color);
				AddEntityParticleTrail(p_vxrocketsmoke, cent, 1, 3, 0.5, color);

				color[0] = 128; color[1] = 56; color[2] = 9; color[3] = 255;
				AddEntityParticleTrail(p_trailpart, cent, 2, 4, 0.2, color);
			}
			break;
			//VULT PARTICLES
		case BLEEDING_TRAIL:
		case BLEEDING_TRAIL2:
			if (amf_part_blood_color.value == 2) {
				color[0] = 55; color[1] = 102; color[2] = 255;
			}
			//green
			else if (amf_part_blood_color.value == 3) {
				color[0] = 80; color[1] = 255; color[2] = 80;
			}
			//red
			else {
				color[0] = 128; color[1] = 0; color[2] = 0;
			}
			color[3] = 255;
			AddEntityParticleTrail(type == BLEEDING_TRAIL ? p_trailbleed : p_bleedspike, cent, 0, 4, type == BLEEDING_TRAIL ? 0.5 : 0.2, color);
			break;
		case ROCKET_TRAIL:
		default:
			color[0] = 255; color[1] = 56; color[2] = 9; color[3] = 255;
			AddEntityParticleTrail(p_trailpart, cent, 0, 6.2, 0.31, color);
			AddEntityParticleTrail(p_smoke, cent, 1, 1.8, 0.825, NULL);
			break;
	}
}

// deurk: QMB version of ported zquake rail trail
void QMB_ParticleRailTrail(vec3_t start, vec3_t end, int color_num)
{
	col_t color;

	color[0] = 255; color[1] = 255; color[2] = 255; color[3] = 255;
	AddParticleTrail(p_alphatrail, start, end, 0.5, 0.5, color);
	switch (color_num) {
		case 180:
			color[0] = 91; color[1] = 74; color[2] = 56;
			break;
		case 35:
			color[0] = 121; color[1] = 118; color[2] = 185;
			break;
		case 224:
			color[0] = 206; color[1] = 0; color[2] = 0;
			break;
		case 133:
			color[0] = 107; color[1] = 70; color[2] = 88;
			break;
		case 192:
			color[0] = 255; color[1] = 242; color[2] = 32;
			break;
		case 6:
			color[0] = 200; color[1] = 200; color[2] = 200;
			break;
		default:
			color[0] = 0; color[1] = 0; color[2] = 255;
			break;
	}
	AddParticleTrail(p_alphatrail, start, end, 2, 1, color);
}
