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
#include "pmove.h"
#include "gl_local.h"

#define DEFAULT_NUM_PARTICLES				4096
#define ABSOLUTE_MIN_PARTICLES				256
#define ABSOLUTE_MAX_PARTICLES				32768

typedef byte col_t[4];

typedef enum {
	p_spark, p_smoke, p_fire, p_bubble, p_lavasplash, p_gunblast, p_chunk, p_shockwave,
	p_inferno_flame, p_inferno_trail, p_sparkray, p_staticbubble, p_trailpart,
	p_dpsmoke, p_dpfire, p_teleflare, p_blood1, p_blood2, p_blood3,
	num_particletypes,
} part_type_t;

typedef enum {
	pm_static, pm_normal, pm_bounce, pm_die, pm_nophysics, pm_float,
} part_move_t;

typedef enum {
	ptex_none, ptex_smoke, ptex_bubble, ptex_generic, ptex_dpsmoke, ptex_lava,
	ptex_blueflare, ptex_blood1, ptex_blood2, ptex_blood3,
	num_particletextures,
} part_tex_t;

typedef enum {
	pd_spark, pd_sparkray, pd_billboard, pd_billboard_vel,
} part_draw_t;

typedef struct particle_s {
	struct particle_s *next;
	vec3_t		org, endorg;
	col_t		color;
	float		growth;		
	vec3_t		vel;
	float		rotangle;
	float		rotspeed;	
	float		size;
	float		start;		
	float		die;		
	byte		hit;		
	byte		texindex;	
	byte		bounces;	
} particle_t;

typedef struct particle_tree_s {
	particle_t	*start;
	part_type_t	id;
	part_draw_t	drawtype;
	int			SrcBlend;
	int			DstBlend;
	part_tex_t	texture;
	float		startalpha;	
	float		grav;
	float		accel;
	part_move_t	move;
	float		custom;		
} particle_type_t;


#define	MAX_PTEX_COMPONENTS		8
typedef struct particle_texture_s {
	int			texnum;
	int			components;
	float		coords[MAX_PTEX_COMPONENTS][4];		
} particle_texture_t;


static float sint[7] = {0.000000, 0.781832, 0.974928, 0.433884, -0.433884, -0.974928, -0.781832};
static float cost[7] = {1.000000, 0.623490, -0.222521, -0.900969, -0.900969, -0.222521, 0.623490};

static particle_t *particles, *free_particles;
static particle_type_t particle_types[num_particletypes];
static int particle_type_index[num_particletypes];	
static particle_texture_t particle_textures[num_particletextures];

static int r_numparticles;		
static vec3_t zerodir = {22, 22, 22};
static int particle_count = 0;
static float particle_time;		
static vec3_t trail_stop;


qboolean qmb_initialized = false;


cvar_t gl_clipparticles = {"gl_clipparticles", "1"};
cvar_t gl_bounceparticles = {"gl_bounceparticles", "1"};


#define TruePointContents(p) PM_HullPointContents(&cl.worldmodel->hulls[0], 0, p)

#define ISUNDERWATER(x) ((x) == CONTENTS_WATER || (x) == CONTENTS_SLIME || (x) == CONTENTS_LAVA)

static qboolean TraceLineN (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal) {
	pmtrace_t trace;

	memset (&trace, 0, sizeof(trace));
	trace.fraction = 1;
	if (PM_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, start, end, &trace))
		return false;
	VectorCopy (trace.endpos, impact);
	if (normal)
		VectorCopy (trace.plane.normal, normal);

	return true;
}

static byte *ColorForParticle(part_type_t type) {
	int lambda;
	static col_t color;

	switch (type) {
	case p_spark:
		color[0] = 224 + (rand() & 31); color[1] = 100 + (rand() & 31); color[2] = 0;
		break;
	case p_smoke:
		color[0] = color[1] = color[2] = 255;
		break;
	case p_fire:
		color[0] = 255; color[1] = 142; color[2] = 62;
		break;
	case p_bubble:
	case p_staticbubble:
		color[0] = color[1] = color[2] = 192 + (rand() & 63);
		break;
	case p_teleflare:
	case p_lavasplash:
		color[0] = color[1] = color[2] = 128 + (rand() & 127);
		break;
	case p_gunblast:
		color[0]= 224 + (rand() & 31); color[1] = 170 + (rand() & 31); color[2] = 0;
		break;
	case p_chunk:
		color[0] = color[1] = color[2] = (32 + (rand() & 127));
		break;
	case p_shockwave:
		color[0] = color[1] = color[2] = 64 + (rand() & 31);
		break;
	case p_inferno_flame:
	case p_inferno_trail:
		color[0] = 255; color[1] = 77; color[2] = 13;
		break;
	case p_sparkray:
		color[0] = 255; color[1] = 102; color[2] = 25;
		break;
	case p_dpsmoke:
		color[0] = color[1] = color[2] = 48 + (((rand() & 0xFF) * 48) >> 8);
		break;
	case p_dpfire:
		lambda = rand() & 0xFF;
		color[0] = 160 + ((lambda * 48) >> 8); color[1] = 16 + ((lambda * 148) >> 8); color[2] = 16 + ((lambda * 16) >> 8);
		break;
	case p_blood1:
	case p_blood2:
		color[0] = color[1] = color[2] = 180 + (rand() & 63);
		break;
	case p_blood3:
		color[0] = (100 + (rand() & 31)); color[1] = color[2] = 0;
		break;
	default:
		assert(!"ColorForParticle: unexpected type");
		break;
	}
	return color;
}


#define ADD_PARTICLE_TEXTURE(_ptex, _texnum, _texindex, _components, _s1, _t1, _s2, _t2)	\
do {																						\
	particle_textures[_ptex].texnum = _texnum;												\
	particle_textures[_ptex].components = _components;										\
	particle_textures[_ptex].coords[_texindex][0] = (_s1 + 1) / 256.0;						\
	particle_textures[_ptex].coords[_texindex][1] = (_t1 + 1) / 256.0;						\
	particle_textures[_ptex].coords[_texindex][2] = (_s2 - 1) / 256.0;						\
	particle_textures[_ptex].coords[_texindex][3] = (_t2 - 1) / 256.0;						\
} while(0);

#define ADD_PARTICLE_TYPE(_id, _drawtype, _SrcBlend, _DstBlend, _texture, _startalpha, _grav, _accel, _move, _custom)	\
do {																													\
	particle_types[count].id = (_id);																					\
	particle_types[count].drawtype = (_drawtype);																		\
	particle_types[count].SrcBlend = (_SrcBlend);																		\
	particle_types[count].DstBlend = (_DstBlend);																		\
	particle_types[count].texture = (_texture);																			\
	particle_types[count].startalpha = (_startalpha);																	\
	particle_types[count].grav = 9.8 * (_grav);																			\
	particle_types[count].accel = (_accel);																				\
	particle_types[count].move = (_move);																				\
	particle_types[count].custom = (_custom);																			\
	particle_type_index[_id] = count;																					\
	count++;																											\
} while(0);

void QMB_InitParticles (void) {
	int	i, count = 0, particlefont;

	Cvar_SetCurrentGroup(CVAR_GROUP_PARTICLES);
	Cvar_Register (&gl_clipparticles);
	Cvar_Register (&gl_bounceparticles);

	Cvar_ResetCurrentGroup();

	if (!(particlefont = GL_LoadTextureImage ("textures/particles/particlefont", "qmb:particlefont", 256, 256, TEX_ALPHA | TEX_COMPLAIN))) 
		return;		

	
	ADD_PARTICLE_TEXTURE(ptex_none, 0, 0, 1, 0, 0, 0, 0);	
	ADD_PARTICLE_TEXTURE(ptex_blood1, particlefont, 0, 1, 0, 0, 64, 64);
	ADD_PARTICLE_TEXTURE(ptex_blood2, particlefont, 0, 1, 64, 0, 128, 64);
	ADD_PARTICLE_TEXTURE(ptex_lava, particlefont, 0, 1, 128, 0, 192, 64);
	ADD_PARTICLE_TEXTURE(ptex_blueflare, particlefont, 0, 1, 192, 0, 256, 64);
	ADD_PARTICLE_TEXTURE(ptex_generic, particlefont, 0, 1, 0, 96, 96, 192);
	ADD_PARTICLE_TEXTURE(ptex_smoke, particlefont, 0, 1, 96, 96, 192, 192);
	ADD_PARTICLE_TEXTURE(ptex_blood3, particlefont, 0, 1, 192, 96, 256, 160);
	ADD_PARTICLE_TEXTURE(ptex_bubble, particlefont, 0, 1, 192, 160, 224, 192);
	for (i = 0; i < 8; i++)
		ADD_PARTICLE_TEXTURE(ptex_dpsmoke, particlefont, i, 8, i * 32, 64, (i + 1) * 32, 96);

	
	if ((i = COM_CheckParm ("-particles")) && i + 1 < com_argc)	{
		r_numparticles = (int) (Q_atoi(com_argv[i + 1]));
		r_numparticles = bound(ABSOLUTE_MIN_PARTICLES, r_numparticles, ABSOLUTE_MAX_PARTICLES);
	} else {
		r_numparticles = DEFAULT_NUM_PARTICLES;
	}
	particles = Hunk_AllocName(r_numparticles * sizeof(particle_t), "qmb:particles");

	
	ADD_PARTICLE_TYPE(p_spark, pd_spark, GL_SRC_ALPHA, GL_ONE, ptex_none, 255, -32, 0, pm_bounce, 1.3);
	ADD_PARTICLE_TYPE(p_sparkray, pd_sparkray, GL_SRC_ALPHA, GL_ONE, ptex_none, 255, -0, 0, pm_nophysics, 0);
	ADD_PARTICLE_TYPE(p_gunblast, pd_spark, GL_SRC_ALPHA, GL_ONE, ptex_none, 255, -16, 0, pm_bounce, 1.3);

	ADD_PARTICLE_TYPE(p_fire, pd_billboard, GL_SRC_ALPHA, GL_ONE, ptex_generic, 204, 0, -2.95, pm_die, 0);
	ADD_PARTICLE_TYPE(p_chunk, pd_billboard, GL_SRC_ALPHA, GL_ONE, ptex_generic, 255, -16, 0, pm_bounce, 1.475);
	ADD_PARTICLE_TYPE(p_shockwave, pd_billboard, GL_SRC_ALPHA, GL_ONE, ptex_generic, 255, 0, -4.85, pm_nophysics, 0);
	ADD_PARTICLE_TYPE(p_inferno_flame, pd_billboard, GL_SRC_ALPHA, GL_ONE, ptex_generic, 153, 0, 0, pm_static, 0);
	ADD_PARTICLE_TYPE(p_inferno_trail, pd_billboard, GL_SRC_ALPHA, GL_ONE, ptex_generic, 204, 0, 0, pm_die, 0);
	ADD_PARTICLE_TYPE(p_trailpart, pd_billboard, GL_SRC_ALPHA, GL_ONE, ptex_generic, 230, 0, 0, pm_static, 0);
	ADD_PARTICLE_TYPE(p_smoke, pd_billboard, GL_SRC_ALPHA, GL_ONE, ptex_smoke, 140, 3, 0, pm_normal, 0);
	ADD_PARTICLE_TYPE(p_dpfire, pd_billboard, GL_SRC_ALPHA, GL_ONE, ptex_dpsmoke, 144, 0, 0, pm_die, 0);
	ADD_PARTICLE_TYPE(p_dpsmoke, pd_billboard, GL_SRC_ALPHA, GL_ONE, ptex_dpsmoke, 85, 3, 0, pm_die, 0);

	ADD_PARTICLE_TYPE(p_teleflare, pd_billboard, GL_ONE, GL_ONE, ptex_blueflare, 255, 0, 0, pm_die, 0);

	ADD_PARTICLE_TYPE(p_blood1, pd_billboard, GL_ZERO, GL_ONE_MINUS_SRC_COLOR, ptex_blood1, 255, -20, 0, pm_die, 0);
	ADD_PARTICLE_TYPE(p_blood2, pd_billboard_vel, GL_ZERO, GL_ONE_MINUS_SRC_COLOR, ptex_blood2, 255, -25, 0, pm_die, 0.018);

	ADD_PARTICLE_TYPE(p_lavasplash, pd_billboard, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, ptex_lava, 170, 0, 0, pm_nophysics, 0);
	ADD_PARTICLE_TYPE(p_blood3, pd_billboard, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, ptex_blood3, 255, -20, 0, pm_normal, 0);
	ADD_PARTICLE_TYPE(p_bubble, pd_billboard, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, ptex_bubble, 204, 8, 0, pm_float, 0);
	ADD_PARTICLE_TYPE(p_staticbubble, pd_billboard, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, ptex_bubble, 204, 0, 0, pm_static, 0);

	qmb_initialized = true;
}

void QMB_ClearParticles (void) {
	int	i;

	if (!qmb_initialized)
		return;

	particle_count = 0;
	memset(particles, 0, r_numparticles * sizeof(particle_t));
	free_particles = &particles[0];

	for (i = 0; i + 1 < r_numparticles; i++)
		particles[i].next = &particles[i + 1];
	particles[r_numparticles - 1].next = NULL;

	for (i = 0; i < num_particletypes; i++)
		particle_types[i].start = NULL;
}

static void QMB_UpdateParticles(void) {
	int i, contents;
	float grav, bounce;
	vec3_t oldorg, stop, normal;
	particle_type_t *pt;
	particle_t *p, *kill;

	particle_count = 0;
	grav = movevars.gravity / 800.0;

	for (i = 0; i < num_particletypes; i++) {
		pt = &particle_types[i];
		
		if (pt->start) {
			for (p = pt->start; p && p->next; ) {
				kill = p->next;
				if (kill->die <= particle_time) {
					p->next = kill->next;
					kill->next = free_particles;
					free_particles = kill;
				} else {
					p = p->next;
				}
			}
			if (pt->start->die <= particle_time) {
				kill = pt->start;
				pt->start = kill->next;
				kill->next = free_particles;
				free_particles = kill;
			}
		}

		for (p = pt->start; p; p = p->next) {
			if (particle_time < p->start)
				continue;
			
			particle_count++;
			
			p->size += p->growth * cls.frametime;
			
			if (p->size <= 0) {
				p->die = 0;
				continue;
			}
			
			p->color[3] = pt->startalpha * ((p->die - particle_time) / (p->die - p->start));
			
			p->rotangle += p->rotspeed * cls.frametime;
			
			if (p->hit)
				continue;
			
			p->vel[2] += pt->grav * grav * cls.frametime;
			
			VectorScale(p->vel, 1 + pt->accel * cls.frametime, p->vel)
			
			switch (pt->move) {
			case pm_static:
				break;
			case pm_normal:
				VectorCopy(p->org, oldorg);
				VectorMA(p->org, cls.frametime, p->vel, p->org);
				if (CONTENTS_SOLID == TruePointContents (p->org)) {
					p->hit = 1;
					VectorCopy(oldorg, p->org);
					VectorClear(p->vel);
				}
				break;
			case pm_float:
				VectorMA(p->org, cls.frametime, p->vel, p->org);
				p->org[2] += p->size + 1;		
				contents = TruePointContents(p->org);
				if (!ISUNDERWATER(contents))
					p->die = 0;
				p->org[2] -= p->size + 1;
				break;
			case pm_nophysics:
				VectorMA(p->org, cls.frametime, p->vel, p->org);
				break;
			case pm_die:
				VectorMA(p->org, cls.frametime, p->vel, p->org);
				if (CONTENTS_SOLID == TruePointContents (p->org))
					p->die = 0;
				break;
			case pm_bounce:
				
				if (!gl_bounceparticles.value || p->bounces) {
					VectorMA(p->org, cls.frametime, p->vel, p->org);
					if (CONTENTS_SOLID == TruePointContents (p->org))
						p->die = 0;
				} else {
					VectorCopy(p->org, oldorg);
					VectorMA(p->org, cls.frametime, p->vel, p->org);
					if (CONTENTS_SOLID == TruePointContents (p->org)) {
						if (TraceLineN(oldorg, p->org, stop, normal)) {
							VectorCopy(stop, p->org);
							bounce = -pt->custom * DotProduct(p->vel, normal);
							VectorMA(p->vel, bounce, normal, p->vel);
							p->bounces++;
						}
					}
				}
				break;
			default:
				assert(!"QMB_UpdateParticles: unexpected pt->move");
				break;
			}
		}
	}
}


#define DRAW_PARTICLE_BILLBOARD(_ptex, _p, _coord)			\
	glPushMatrix();											\
	glTranslatef(_p->org[0], _p->org[1], _p->org[2]);		\
	glScalef(_p->size, _p->size, _p->size);					\
	if (_p->rotspeed)										\
		glRotatef(_p->rotangle, vpn[0], vpn[1], vpn[2]);	\
															\
	glColor4ubv(_p->color);									\
															\
	glBegin(GL_QUADS);										\
	glTexCoord2f(_ptex->coords[_p->texindex][0], ptex->coords[_p->texindex][3]); glVertex3fv(_coord[0]);	\
	glTexCoord2f(_ptex->coords[_p->texindex][0], ptex->coords[_p->texindex][1]); glVertex3fv(_coord[1]);	\
	glTexCoord2f(_ptex->coords[_p->texindex][2], ptex->coords[_p->texindex][1]); glVertex3fv(_coord[2]);	\
	glTexCoord2f(_ptex->coords[_p->texindex][2], ptex->coords[_p->texindex][3]); glVertex3fv(_coord[3]);	\
	glEnd();			\
						\
	glPopMatrix();

void QMB_DrawParticles (void) {
	int	i, j, k, drawncount;
	int SrcBlend = GL_SRC_ALPHA, DstBlend = GL_ONE_MINUS_SRC_ALPHA, texture2D = true;
	vec3_t v, up, right, billboard[4], velcoord[4], neworg;
	particle_t *p;
	particle_type_t *pt;
	particle_texture_t *ptex;

	particle_time = cl.time;

	if (!cl.paused)
		QMB_UpdateParticles();

	VectorAdd(vup, vright, billboard[2]);
	VectorSubtract(vright, vup, billboard[3]);
	VectorNegate(billboard[2], billboard[0]);
	VectorNegate(billboard[3], billboard[1]);

	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glShadeModel(GL_SMOOTH);

	for (i = 0; i < num_particletypes; i++) {
		pt = &particle_types[i];
		if (!pt->start)
			continue;

		glBlendFunc(pt->SrcBlend, pt->DstBlend);

		switch(pt->drawtype) {
		case pd_spark:
			glDisable(GL_TEXTURE_2D);
			for (p = pt->start; p; p = p->next) {
				if (particle_time < p->start || particle_time >= p->die)
					continue;

				glBegin(GL_TRIANGLE_FAN);
				glColor4ubv(p->color);
				glVertex3fv(p->org);
				glColor4ub(p->color[0] >> 1, p->color[1] >> 1, p->color[2] >> 1, 0);
				for (j = 7; j >= 0; j--) {
					for (k = 0; k < 3; k++)
						v[k] = p->org[k] - p->vel[k] / 8 + vright[k] * cost[j % 7] * p->size + vup[k] * sint[j % 7] * p->size;
					glVertex3fv(v);
				}
				glEnd();
			}
			break;
		case pd_sparkray:
			glDisable(GL_TEXTURE_2D);
			for (p = pt->start; p; p = p->next) {
				if (particle_time < p->start || particle_time >= p->die)
					continue;

				if (!TraceLineN(p->endorg, p->org, neworg, NULL)) 
					VectorCopy(p->org, neworg);

				glBegin (GL_TRIANGLE_FAN);
				glColor4ubv(p->color);
				glVertex3fv(p->endorg);
				glColor4ub(p->color[0] >> 1, p->color[1] >> 1, p->color[2] >> 1, 0);
				for (j = 7; j >= 0; j--) {
					for (k = 0; k < 3; k++)
						v[k] = neworg[k] + vright[k] * cost[j % 7] * p->size + vup[k] * sint[j % 7] * p->size;
					glVertex3fv (v);
				}
				glEnd();
			}
			break;
		case pd_billboard:
			ptex = &particle_textures[pt->texture];
			glEnable(GL_TEXTURE_2D);
			GL_Bind(ptex->texnum);
			drawncount = 0;
			for (p = pt->start; p; p = p->next) {
				if (particle_time < p->start || particle_time >= p->die)
					continue;

				if (gl_clipparticles.value) {
					if (drawncount >= 3 && VectorSupCompare(p->org, r_origin, 30))
						continue;
					drawncount++;
				}
				DRAW_PARTICLE_BILLBOARD(ptex, p, billboard);
			}
			break;
		case pd_billboard_vel:
			ptex = &particle_textures[pt->texture];
			glEnable(GL_TEXTURE_2D);
			GL_Bind(ptex->texnum);
			for (p = pt->start; p; p = p->next) {
				if (particle_time < p->start || particle_time >= p->die)
					continue;

				VectorCopy (p->vel, up);
				CrossProduct(vpn, up, right);
				VectorNormalizeFast(right);
				VectorScale(up, pt->custom, up);

				VectorAdd(up, right, velcoord[2]);
				VectorSubtract(right, up, velcoord[3]);
				VectorNegate(velcoord[2], velcoord[0]);
				VectorNegate(velcoord[3], velcoord[1]);
				DRAW_PARTICLE_BILLBOARD(ptex, p, velcoord);
			}
			break;
		default:
			assert(!"QMB_DrawParticles: unexpected drawtype");
			break;
		}
	}

	glEnable(GL_TEXTURE_2D);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

#define	INIT_NEW_PARTICLE(_pt, _p, _color, _size, _time)	\
		_p = free_particles;								\
		free_particles = _p->next;							\
		_p->next = _pt->start;								\
		_pt->start = _p;									\
		_p->size = _size;									\
		_p->hit = 0;										\
		_p->start = cl.time;								\
		_p->die = _p->start + _time;						\
		_p->growth = 0;										\
		_p->rotspeed = 0;									\
		_p->texindex = (rand() % particle_textures[_pt->texture].components);	\
		_p->bounces = 0;									\
		VectorCopy(_color, _p->color);


__inline static void AddParticle(part_type_t type, vec3_t org, int count, float size, float time, col_t col, vec3_t dir) {
	byte *color;
	int i, j;
	float tempSize;
	particle_t *p;
	particle_type_t *pt;

	if (!qmb_initialized)
		Sys_Error("QMB particle added without initialization");

	assert(size > 0 && time > 0);

	if (type < 0 || type >= num_particletypes)
		Sys_Error("AddParticle: Invalid type (%d)", type);

	pt = &particle_types[particle_type_index[type]];

	for (i = 0; i < count && free_particles; i++) {
		color = col ? col : ColorForParticle(type);
		INIT_NEW_PARTICLE(pt, p, color, size, time);

		switch (type) {
		case p_spark:
			p->size = 1.175;
			VectorCopy(org, p->org);
			tempSize = size * 2;
			p->vel[0] = (rand() % (int) tempSize) - ((int) tempSize / 2);
			p->vel[1] = (rand() % (int) tempSize) - ((int) tempSize / 2);
			p->vel[2] = (rand() % (int) tempSize) - ((int) tempSize / 3);
			break;
		case p_smoke:
			for (j = 0; j < 3; j++)
				p->org[j] = org[j] + ((rand() & 31) - 16) / 2.0;
			for (j = 0; j < 3; j++)
				p->vel[j] = ((rand() % 10) - 5) / 20.0;
			break;
		case p_fire:
			VectorCopy(org, p->org);
			for (j = 0; j < 3; j++)
				p->vel[j] = ((rand() % 160) - 80) * (size / 25.0);
			break;
		case p_bubble:
			p->start += (rand() & 15) / 36.0;
			p->org[0] = org[0] + ((rand() & 31) - 16);
			p->org[1] = org[1] + ((rand() & 31) - 16);
			p->org[2] = org[2] + ((rand() & 63) - 32);
			VectorClear(p->vel);
			break;
		case p_lavasplash:
			VectorCopy(org, p->org);
			VectorCopy(dir, p->vel);	
			break;
		case p_gunblast:
			p->size = 1;
			VectorCopy(org, p->org);
			p->vel[0] = (rand() & 127) - 64;
			p->vel[1] = (rand() & 127) - 64;
			p->vel[2] = (rand() & 127) - 10;
			break;
		case p_chunk:
			VectorCopy(org, p->org);
			p->vel[0] = (rand() % 40) - 20;
			p->vel[1] = (rand() % 40) - 20;
			p->vel[2] = (rand() % 40) - 5;			
			break;
		case p_shockwave:
			VectorCopy(org, p->org);
			VectorCopy(dir, p->vel);	
			break;
		case p_inferno_trail:
			for (j = 0; j < 3; j++)
				p->org[j] = org[j] + (rand() & 15) - 8;
			for (j = 0; j < 3; j++)
				p->vel[j] = (rand() & 3) - 2;
			p->growth = -1.5;
			break;
		case p_inferno_flame:
			VectorCopy(org, p->org);
			VectorClear(p->vel);
			p->growth = -30;
			break;
		case p_sparkray:
			VectorCopy(org, p->endorg);
			VectorCopy(dir, p->org);	
			for (j = 0; j < 3; j++)
				p->vel[j] = (rand() & 127) - 64;
			p->growth = -16;
			break;
		case p_staticbubble:
			VectorCopy(org, p->org);
			VectorClear(p->vel);
			break;
		case p_teleflare:
			VectorCopy(org, p->org);
			VectorCopy(dir, p->vel);	
			p->growth = 1.75;
			break;
		case p_blood1:
		case p_blood2:
			for (j = 0; j < 3; j++)
				p->org[j] = org[j] + (rand() & 15) - 8;
			for (j = 0; j < 3; j++)
				p->vel[j] = (rand() & 63) - 32;
			break;
		case p_blood3:
			p->size = size * (rand() % 20) / 5.0;
			VectorCopy(org, p->org);
			for (j = 0; j < 3; j++)
				p->vel[j] = (rand() % 40) - 20;
			break;
		default:
			assert(!"AddParticle: unexpected type");
			break;
		}
	}
}

__inline static void AddParticleTrail(part_type_t type, vec3_t start, vec3_t end, float size, float time, col_t col) {
	byte *color;
	int i, j,  num_particles;
	float count, length, theta = 0;
	vec3_t point, delta;
	particle_t *p;
	particle_type_t *pt;

	if (!qmb_initialized)
		Sys_Error("QMB particle added without initialization");

	assert(size > 0 && time > 0);

	if (type < 0 || type >= num_particletypes)
		Sys_Error("AddParticle: Invalid type (%d)", type);

	pt = &particle_types[particle_type_index[type]];

	VectorCopy(start, point);
	VectorSubtract(end, start, delta);
	if (!(length = VectorLength(delta)))
		goto done;

	switch(type) {
	case p_trailpart:
		count = length / 1.1;
		break;
	case p_blood3:
		count = length / 8;
		break;
	case p_smoke:
		count = length / 3.8;
		break;
	case p_dpsmoke:
		count = length / 2.5;
		break;
	case p_dpfire:
		count = length / 2.8;
		break;
	default:
		assert(!"AddParticleTrail: unexpected type");
		break;
	}

	if (!(num_particles = (int) count))
		goto done;

	VectorScale(delta, 1.0 / num_particles, delta);

	for (i = 0; i < num_particles && free_particles; i++) {
		color = col ? col : ColorForParticle(type);
		INIT_NEW_PARTICLE(pt, p, color, size, time);

		switch (type) {
		case p_trailpart:
			VectorCopy (point, p->org);
			VectorClear(p->vel);
			p->growth = -size / time;
			break;
		case p_blood3:
			VectorCopy (point, p->org);
			for (j = 0; j < 3; j++)
				p->org[j] += ((rand() & 15) - 8) / 8.0;
			for (j = 0; j < 3; j++)
				p->vel[j] = ((rand() & 15) - 8) / 2.0;
			p->size = size * (rand() % 20) / 10.0;
			p->growth = 6;
			break;
		case p_smoke:
			VectorCopy (point, p->org);
			for (j = 0; j < 3; j++)
				p->org[j] += ((rand() & 7) - 4) / 8.0;
			p->vel[0] = p->vel[1] = 0;
			p->vel[2] = rand() & 3;
			p->growth = 4.5;
			p->rotspeed = (rand() & 63) + 96;
			break;
		case p_dpsmoke:
			VectorCopy (point, p->org);
			for (j = 0; j < 3; j++)
				p->vel[j] = (rand() % 10) - 5;
			p->growth = 3;
			p->rotspeed = (rand() & 63) + 96;
			break;
		case p_dpfire:
			VectorCopy (point, p->org);
			for (j = 0; j < 3; j++)
				p->vel[j] = (rand() % 40) - 20;
			break;
		default:
			assert(!"AddParticleTrail: unexpected type");
			break;
		}

		VectorAdd(point, delta, point);
	}
done:
	VectorCopy (point, trail_stop);
}

#define ONE_FRAME_ONLY	(0.0001)

void QMB_ParticleExplosion (vec3_t org) {
	int contents;

	contents = TruePointContents(org);
	if (ISUNDERWATER(contents)) {
		AddParticle(p_fire, org, 12, 14, 0.8, NULL, zerodir);
		AddParticle(p_bubble, org, 6, 3.0, 2.5, NULL, zerodir);
		AddParticle(p_bubble, org, 4, 2.35, 2.5, NULL, zerodir);
		if (r_explosiontype.value != 1) {
			AddParticle(p_spark, org, 50, 100, 0.75, NULL, zerodir);
			AddParticle(p_spark, org, 25, 60, 0.75, NULL, zerodir);
		}
	} else {
		AddParticle(p_fire, org, 16, 18, 1, NULL, zerodir);
		if (r_explosiontype.value != 1) {
			AddParticle(p_spark, org, 50, 250, 0.925f, NULL, zerodir);
			AddParticle(p_spark,org, 25, 150, 0.925f,  NULL, zerodir);
		}
	}
}

void QMB_RunParticleEffect (vec3_t org, vec3_t dir, int col, int count) {
	col_t color;
	vec3_t neworg;
	int i, scale, blastcount, blastsize, sparksize, sparkcount, chunkcount, particlecount, bloodcount;
	float blasttime, sparktime;

	if (col == 73) {			
		bloodcount = Q_rint(count / 28.0);
		bloodcount = bound(2, bloodcount, 8);
		for (i = 0; i < bloodcount; i++) {
			AddParticle(p_blood1, org, 1, 4.5, 2 + (rand() & 15) / 16.0, NULL, zerodir);
			AddParticle(p_blood2, org, 1, 4.5, 2 + (rand() & 15) / 16.0, NULL, zerodir);
		}
		return;
	} else if (col == 225) {	
		VectorClear(color);
		scale = (count > 130) ? 3 : (count > 20) ? 2  : 1;
		scale *= 0.758;
		for (i = 0; i < (count >> 1); i++) {
			neworg[0] = org[0] + scale * ((rand() & 15) - 8);
			neworg[1] = org[1] + scale * ((rand() & 15) - 8);
			neworg[2] = org[2] + scale * ((rand() & 15) - 8);
			color[0] = 50 + (rand() & 63);
			AddParticle(p_blood3, org, 1, 1, 2.3, NULL, zerodir);
		}
		return;
	} else if (col == 20 && count == 30) {	
		color[0] = 51; color[2] = 51; color[1] = 255;
		AddParticle(p_chunk, org, 1, 1, 0.75, color, zerodir);  
		AddParticle(p_spark, org, 12 , 75, 0.4, color, zerodir);
		return;
	} else if (col == 226 && count == 20) {	
		color[0] = 230; color[1] = 204; color[2] = 26;
		AddParticle(p_chunk, org, 1, 1, 0.75, color, zerodir);  
		AddParticle(p_spark, org, 12 , 75, 0.4, color, zerodir);
		return;
	}

	switch (count) {
	case 10:	
		AddParticle(p_spark, org, 4, 70, 0.9, NULL, zerodir);
		break;
	case 20:	
		AddParticle(p_spark, org, 8, 85, 0.9, NULL, zerodir);
		break;
	case 30:	
		AddParticle(p_chunk, org, 10, 1, 4, NULL, zerodir);
		AddParticle(p_spark, org, 8, 105, 0.9, NULL, zerodir);
		break;
	default:	
		if (count > 130) {
			scale = 2.274;
			blastcount = 50; blastsize = 50; blasttime = 0.4;
			sparkcount = 6;	sparksize = 70;	sparktime = 0.6;
			chunkcount = 14;
		} else if (count > 20) {
			scale = 1.516;
			blastcount = 30; blastsize = 35; blasttime = 0.25;
			sparkcount = 4;	sparksize = 60;	sparktime = 0.4;
			chunkcount = 7;
		} else {
			scale = 0.758;
			blastcount = 15; blastsize = 5;	blasttime = 0.15;
			sparkcount = 2;	sparksize = 50;	sparktime = 0.25;
			chunkcount = 3;
		}
		particlecount = max(1, count >> 1);
		AddParticle(p_gunblast, org, blastcount, blastsize, blasttime, NULL, zerodir);
		for (i = 0; i < particlecount; i++) {
			neworg[0] = org[0] + scale * ((rand() & 15) - 8);
			neworg[1] = org[1] + scale * ((rand() & 15) - 8);
			neworg[2] = org[2] + scale * ((rand() & 15) - 8);
			AddParticle(p_smoke, neworg, 1, 4, 0.825f + ((rand() % 10) - 5) / 40.0, NULL, zerodir);
			if (i % (particlecount / chunkcount) == 0)
				AddParticle(p_chunk, neworg, 1, 0.75, 3.75, NULL, zerodir);
		}
	}
}

void QMB_ParticleTrail (vec3_t start, vec3_t end, vec3_t *trail_origin, trail_type_t type) {
	col_t color;

	switch (type) {
		case GRENADE_TRAIL:
			AddParticleTrail(p_smoke, start, end, 1.45, 0.825, NULL);
			break;
		case BLOOD_TRAIL:
		case BIG_BLOOD_TRAIL:
			AddParticleTrail(p_blood3, start, end, type == BLOOD_TRAIL ? 1.35 : 2.4, 2, NULL);
			break;
		case TRACER1_TRAIL:
			color[0] = 0; color[1] = 124; color[2] = 0;
			AddParticleTrail (p_trailpart, start, end, 3.75, 0.5, color);
			break;
		case TRACER2_TRAIL:
			color[0] = 255; color[1] = 77; color[2] = 0;
			AddParticleTrail (p_trailpart, start, end, 3.75, 0.5, color);
			break;
		case VOOR_TRAIL:
			color[0] = 77; color[1] = 0; color[2] = 255;
			AddParticleTrail (p_trailpart, start, end, 3.75, 0.5, color);
			break;
		case ALT_ROCKET_TRAIL:
			AddParticleTrail(p_dpfire, start, end, 3, 0.26, NULL);
			AddParticleTrail(p_dpsmoke, start, end, 3, 0.825, NULL);
			break;
		case ROCKET_TRAIL:
		default:
			color[0] = 255; color[1] = 56; color[2] = 9;
			AddParticleTrail(p_trailpart, start, end, 6.2, 0.31, color);
			AddParticleTrail(p_smoke, start, end, 1.8, 0.825, NULL);
			break;
	}

	VectorCopy(trail_stop, *trail_origin);
}

void QMB_BlobExplosion (vec3_t org) {
	float theta;
	col_t color;
	vec3_t neworg, vel;

	color[0] = 60; color[1] = 100; color[2] = 240;
	AddParticle (p_spark, org, 44, 250, 1.15, color, zerodir);

	color[0] = 90; color[1] = 47; color[2] = 207;
	AddParticle(p_fire, org, 15, 30, 1.4, color, zerodir);

	vel[2] = 0;
	for (theta = 0; theta < 2 * M_PI; theta += 2 * M_PI / 70) {
		color[0] = (60 + (rand() & 15)); color[1] = (65 + (rand() & 15)); color[2] = (200 + (rand() & 15));

		
		vel[0] = cos(theta) * 125;
		vel[1] = sin(theta) * 125;
		neworg[0] = org[0] + cos(theta) * 6;
		neworg[1] = org[1] + sin(theta) * 6;
		neworg[2] = org[2] + 0 - 10;
		AddParticle(p_shockwave, neworg, 1, 4, 0.8, color, vel);
		neworg[2] = org[2] + 0 + 10;
		AddParticle(p_shockwave, neworg, 1, 4, 0.8, color, vel);

		
		vel[0] *= 1.15;
		vel[1] *= 1.15;
		neworg[0] = org[0] + cos(theta) * 13;
		neworg[1] = org[1] + sin(theta) * 13;
		neworg[2] = org[2] + 0;
		AddParticle(p_shockwave, neworg, 1, 6, 1.0, color, vel);
	}
}

void QMB_LavaSplash (vec3_t org) {
	int	i, j;
	float vel;
	vec3_t dir, neworg;

	for (i = -16; i < 16; i++) {
		for (j = -16; j < 16; j++) {
			dir[0] = j * 8 + (rand() & 7);
			dir[1] = i * 8 + (rand() & 7);
			dir[2] = 256;

			neworg[0] = org[0] + dir[0];
			neworg[1] = org[1] + dir[1];
			neworg[2] = org[2] + (rand() & 63);

			VectorNormalizeFast (dir);
			vel = 50 + (rand() & 63);
			VectorScale (dir, vel, dir);

			AddParticle(p_lavasplash, neworg, 1, 4.5, 2.6 + (rand() & 31) * 0.02, NULL, dir);
		}
	}
}

void QMB_TeleportSplash (vec3_t org) {
	int i, j, k;
	vec3_t neworg, angle;
	col_t color;

	
	for (i = -12; i <= 12; i += 6) {
		for (j = -12; j <= 12; j += 6) {
			for (k = -24; k <= 32; k += 8) {
				neworg[0] = org[0] + i + (rand() & 3) - 1;
				neworg[1] = org[1] + j + (rand() & 3) - 1;
				neworg[2] = org[2] + k + (rand() & 3) - 1;
				angle[0] = (rand() & 15) - 7;
				angle[1] = (rand() & 15) - 7;
				angle[2] = (rand() % 160) - 80;
				AddParticle(p_teleflare, neworg, 1, 1.8, 0.30 + (rand() & 7) * 0.02, NULL, angle);
			}
		}
	}

	
	VectorSet(color, 140, 140, 255);
	VectorClear(angle);
	for (i = 0; i < 5; i++) {
		angle[2] = 0;
		for (j = 0; j < 5; j++) {
			AngleVectors(angle, NULL, NULL, neworg);
			VectorMA(org, 70, neworg, neworg);	
			AddParticle(p_sparkray, org, 1, 6 + (i & 3), 5,  color, neworg);
			angle[2] += 360 / 5;
		}
		angle[0] += 180 / 5;
	}
}

void QMB_DetpackExplosion (vec3_t org) {
	int i, j, contents;
	float theta;
	vec3_t neworg, angle = {0, 0, 0};

	
	contents = TruePointContents(org);
	if (ISUNDERWATER(contents)) {
		AddParticle(p_bubble, org, 8, 2.8, 2.5, NULL, zerodir);
		AddParticle(p_bubble, org, 6, 2.2, 2.5, NULL, zerodir);
		AddParticle(p_fire, org, 10, 25, 0.75, ColorForParticle(p_inferno_flame), zerodir);
	} else {
		AddParticle(p_fire, org, 14, 33, 0.75, ColorForParticle(p_inferno_flame), zerodir);
	}

	
	for (i = 0; i < 5; i++) {
		angle[2] = 0;
		for (j = 0; j < 5; j++) {
			AngleVectors(angle, NULL, NULL, neworg);
			VectorMA(org, 90, neworg, neworg);	
			AddParticle(p_sparkray, org, 1, 9 + (i & 3), 0.75,  NULL, neworg);
			angle[2] += 360 / 5;
		}
		angle[0] += 180 / 5;
	}

	
	angle[2] = 0;
	for (theta = 0; theta < 2 * M_PI; theta += 2 * M_PI / 90) {
		angle[0] = cos(theta) * 350;
		angle[1] = sin(theta) * 350;
		AddParticle(p_shockwave, org, 1, 14, 0.75, NULL, angle);
	}
}

void QMB_InfernoFlame (vec3_t org) {
	if (cls.frametime) {
		AddParticle(p_inferno_flame, org, 1, 30, 13.125 * cls.frametime,  NULL, zerodir);
		AddParticle(p_inferno_trail, org, 2, 1.75, 45.0 * cls.frametime, NULL, zerodir);
		AddParticle(p_inferno_trail, org, 2, 1.0, 52.5 * cls.frametime, NULL, zerodir);
	}
}

void QMB_StaticBubble (entity_t *ent) {
	AddParticle(p_staticbubble, ent->origin, 1, ent->frame == 1 ? 1.85 : 2.9, ONE_FRAME_ONLY, NULL, zerodir);
}
