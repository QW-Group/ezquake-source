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

	$Id: r_part.c,v 1.19 2007-10-29 00:13:26 d3urk Exp $

*/

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "particles_classic.h"
#include "glm_texture_arrays.h"
#include "gl_billboards.h"
#include "rulesets.h"

static texture_ref particletexture;
texture_ref particletexture_array;
int particletexture_array_index;
static float particletexture_scale_s;
static float particletexture_scale_t;
const int default_size = 32;

cvar_t r_particles_count = { "r_particles_count", "2048" };
static cvar_t r_drawparticles = { "r_drawparticles", "1" };

// Which particles to draw this frame
particle_t particles[ABSOLUTE_MAX_PARTICLES];
glm_particle_t glparticles[ABSOLUTE_MAX_PARTICLES];

static int	ramp1[8] = { 0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61 };
static int	ramp2[8] = { 0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66 };
static int	ramp3[8] = { 0x6d, 0x6b, 6, 5, 4, 3 };

static particle_t	*active_particles, *free_particles;
static int			r_numparticles;

float crand(void)
{
	return (rand() & 32767) * (2.0 / 32767) - 1;
}

static byte* Classic_CreateParticleTexture(int width, int height)
{
	byte* data = Q_malloc(sizeof(byte) * width * height * 4);
	int x, y;
	int size = min(width, height);
	float quarter = size / 4 - 0.5f;
	extern cvar_t gl_squareparticles;

	// draw a circle in the top left corner or square, depends of cvar
	if (gl_squareparticles.integer) {
		for (x = 0; x < size / 2; x++) {
			for (y = 0; y < size / 2; y++) {
				data[(x + y * width) * 4 + 0] = 255;
				data[(x + y * width) * 4 + 1] = 255;
				data[(x + y * width) * 4 + 2] = 255;
				data[(x + y * width) * 4 + 3] = 255;
			}
		}
	}
	else {
		for (x = 0; x < size / 2; x++) {
			for (y = 0; y < size / 2; y++) {
				float edge0 = size / 4.0f;
				float dist = sqrt((x - quarter) * (x - quarter) + (y - quarter) * (y - quarter));

				if (dist < edge0) {
					float edge1 = edge0 - size / 16;

					if (dist < edge1) {
						data[(x + y * width) * 4 + 0] = 255;
						data[(x + y * width) * 4 + 1] = 255;
						data[(x + y * width) * 4 + 2] = 255;
						data[(x + y * width) * 4 + 3] = 255;
					}
					else {
						// smoothstep
						float t = bound((dist - edge1) / (edge0 - edge1), 0, 1);
						float f = 1 - t * t * (3.0 - 2.0 * t);

						data[(x + y * width) * 4 + 0] = 255 * f;
						data[(x + y * width) * 4 + 1] = 255 * f;
						data[(x + y * width) * 4 + 2] = 255 * f;
						data[(x + y * width) * 4 + 3] = 255 * f;
					}
				}
			}
		}
	}

	// draw a square in top-right
	for (x = size - 4; x < size; ++x) {
		for (y = size - 4; y < size; ++y) {
			data[(x + y * width) * 4 + 0] = 255;
			data[(x + y * width) * 4 + 1] = 255;
			data[(x + y * width) * 4 + 2] = 255;
			data[(x + y * width) * 4 + 3] = 255;
		}
	}

	return data;
}

void Classic_LoadParticleTexures(int width, int height)
{
	byte* data = Classic_CreateParticleTexture(width, height);

	// TEX_NOSCALE - so no affect from gl_picmip and gl_maxsize
	particletexture = GL_LoadTexture("particles:classic", width, height, data, TEX_MIPMAP | TEX_ALPHA | TEX_NOSCALE, 4);

	Q_free(data);

	if (!GL_TextureReferenceIsValid(particletexture)) {
		Sys_Error("Classic_LoadParticleTexures: can't load texture");
		return;
	}

	if (GL_ShadersSupported() && GL_TextureReferenceIsValid(particletexture_array)) {
		int width = GL_TextureWidth(particletexture_array);
		int height = GL_TextureHeight(particletexture_array);
		byte* data = Classic_CreateParticleTexture(width, height);

		GL_TexSubImage3D(GL_TEXTURE0, particletexture_array, 0, 0, 0, particletexture_array_index, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
		GL_GenerateMipmap(GL_TEXTURE0, particletexture_array);

		Q_free(data);
	}
}

void Classic_AllocParticles(void)
{
	r_numparticles = bound(ABSOLUTE_MIN_PARTICLES, r_particles_count.integer, ABSOLUTE_MAX_PARTICLES);
}

void Classic_InitParticles(void)
{
	if (!r_numparticles) {
		Classic_AllocParticles();
	}
	else {
		Classic_ClearParticles(); // also re-alloc particles
	}

	Classic_LoadParticleTexures(default_size, default_size);
}

void Classic_ClearParticles(void)
{
	int		i;

	if (!r_numparticles)
		return;

	Classic_AllocParticles();	// and alloc again

	free_particles = &particles[0];
	active_particles = NULL;

	for (i = 0;i < r_numparticles; i++)
		particles[i].next = &particles[i + 1];
	particles[r_numparticles - 1].next = NULL;
}

#ifndef CLIENTONLY
void R_ReadPointFile_f(void)
{
	vfsfile_t *v;
	char line[1024];
	char *s;
	vec3_t org;
	int c;
	particle_t *p;
	char name[MAX_OSPATH];

	if (!com_serveractive)
		return;

	snprintf(name, sizeof(name), "maps/%s.pts", host_mapname.string);

	if (!(v = FS_OpenVFS(name, "rb", FS_ANY))) {
		Com_Printf("couldn't open %s\n", name);
		return;
	}

	Com_Printf("Reading %s...\n", name);
	c = 0;
	while (1) {
		VFS_GETS(v, line, sizeof(line));
		s = COM_Parse(line);
		org[0] = atof(com_token);

		s = COM_Parse(s);
		if (!s)
			break;
		org[1] = atof(com_token);

		s = COM_Parse(s);
		if (!s)
			break;
		org[2] = atof(com_token);
		if (COM_Parse(s))
			break;

		c++;
		if (!free_particles) {
			Com_Printf("Not enough free particles\n");
			break;
		}
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = 99999;
		p->color = (-c) & 15;
		p->type = pt_static;
		VectorClear(p->vel);
		VectorCopy(org, p->org);
	}

	VFS_CLOSE(v);
	Com_Printf("%i points read\n", c);
}
#endif

void Classic_ParticleExplosion(vec3_t org)
{
	int	i, j;
	particle_t	*p;

	if (r_explosiontype.value != 9) {
		CL_ExplosionSprite(org);
	}

	if (r_explosiontype.value == 1)
		return;

	for (i = 0; i < 1024; i++) {
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = r_refdef2.time + 5;
		p->color = ramp1[0];
		p->ramp = rand() & 3;
		if (i & 1) {
			p->type = pt_explode;
			for (j = 0; j < 3; j++) {
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
		else {
			p->type = pt_explode2;
			for (j = 0; j < 3; j++) {
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
	}
}

void Classic_BlobExplosion(vec3_t org)
{
	int i, j;
	particle_t *p;

	for (i = 0; i < 1024; i++) {
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = r_refdef2.time + 1 + (rand() & 8) * 0.05;

		if (i & 1) {
			p->type = pt_blob;
			p->color = 66 + rand() % 6;
			for (j = 0; j < 3; j++) {
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
		else {
			p->type = pt_blob2;
			p->color = 150 + rand() % 6;
			for (j = 0; j < 3; j++) {
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
	}
}

void Classic_RunParticleEffect(vec3_t org, vec3_t dir, int color, int count)
{
	int i, j, scale;
	particle_t *p;

	scale = (count > 130) ? 3 : (count > 20) ? 2 : 1;

	if (color == 256)	// gunshot magic
		color = 0;

	for (i = 0; i < count; i++) {
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = r_refdef2.time + 0.1 * (rand() % 5);
		p->color = (color & ~7) + (rand() & 7);
		p->type = pt_grav;
		for (j = 0; j < 3; j++) {
			p->org[j] = org[j] + scale * ((rand() & 15) - 8);
			p->vel[j] = dir[j] * 15;
		}
	}
}

void Classic_LavaSplash(vec3_t org)
{
	int i, j, k;
	particle_t *p;
	float vel;
	vec3_t dir;

	for (i = -16; i < 16; i++) {
		for (j = -16; j < 16; j++) {
			for (k = 0; k < 1; k++) {
				if (!free_particles)
					return;
				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;

				p->die = r_refdef2.time + 2 + (rand() & 31) * 0.02;
				p->color = 224 + (rand() & 7);
				p->type = pt_grav;

				dir[0] = j * 8 + (rand() & 7);
				dir[1] = i * 8 + (rand() & 7);
				dir[2] = 256;

				p->org[0] = org[0] + dir[0];
				p->org[1] = org[1] + dir[1];
				p->org[2] = org[2] + (rand() & 63);

				VectorNormalizeFast(dir);
				vel = 50 + (rand() & 63);
				VectorScale(dir, vel, p->vel);
			}
		}
	}
}

void Classic_TeleportSplash(vec3_t org)
{
	int i, j, k;
	particle_t *p;
	float vel;
	vec3_t dir;

	for (i = -16; i < 16; i += 4) {
		for (j = -16; j < 16; j += 4) {
			for (k = -24; k < 32; k += 4) {
				if (!free_particles)
					return;
				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;

				p->die = r_refdef2.time + 0.2 + (rand() & 7) * 0.02;
				p->color = 7 + (rand() & 7);
				p->type = pt_grav;

				dir[0] = j * 8;
				dir[1] = i * 8;
				dir[2] = k * 8;

				p->org[0] = org[0] + i + (rand() & 3);
				p->org[1] = org[1] + j + (rand() & 3);
				p->org[2] = org[2] + k + (rand() & 3);

				VectorNormalizeFast(dir);
				vel = 50 + (rand() & 63);
				VectorScale(dir, vel, p->vel);
			}
		}
	}
}

void Classic_ParticleTrail(vec3_t start, vec3_t end, vec3_t *trail_origin, trail_type_t type)
{
	vec3_t point, delta, dir;
	float len;
	int i, j, num_particles;
	particle_t *p;
	static int tracercount;

	VectorCopy(start, point);
	VectorSubtract(end, start, delta);
	if (!(len = VectorLength(delta)))
		goto done;
	VectorScale(delta, 1 / len, dir);	//unit vector in direction of trail

	switch (type) {
	case ALT_ROCKET_TRAIL:
		len /= 1.5; break;
	case BLOOD_TRAIL:
		len /= 6; break;
	default:
		len /= 3; break;
	}

	if (!(num_particles = (int)len))
		goto done;

	VectorScale(delta, 1.0 / num_particles, delta);

	for (i = 0; i < num_particles && free_particles; i++) {
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorClear(p->vel);
		p->die = r_refdef2.time + 2;

		switch (type) {
		case GRENADE_TRAIL:
			p->ramp = (rand() & 3) + 2;
			p->color = ramp3[(int)p->ramp];
			p->type = pt_fire;
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() % 6) - 3);
			break;
		case BLOOD_TRAIL:
			p->type = pt_slowgrav;
			p->color = 67 + (rand() & 3);
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() % 6) - 3);
			break;
		case BIG_BLOOD_TRAIL:
			p->type = pt_slowgrav;
			p->color = 67 + (rand() & 3);
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() % 6) - 3);
			break;
		case TRACER1_TRAIL:
		case TRACER2_TRAIL:
			p->die = r_refdef2.time + 0.5;
			p->type = pt_static;
			if (type == TRACER1_TRAIL)
				p->color = 52 + ((tracercount & 4) << 1);
			else
				p->color = 230 + ((tracercount & 4) << 1);

			tracercount++;

			VectorCopy(point, p->org);
			if (tracercount & 1) {
				p->vel[0] = 90 * dir[1];
				p->vel[1] = 90 * -dir[0];
			}
			else {
				p->vel[0] = 90 * -dir[1];
				p->vel[1] = 90 * dir[0];
			}
			break;
		case VOOR_TRAIL:
			p->color = 9 * 16 + 8 + (rand() & 3);
			p->type = pt_static;
			p->die = r_refdef2.time + 0.3;
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() & 15) - 8);
			break;
		case ALT_ROCKET_TRAIL:
			p->ramp = (rand() & 3);
			p->color = ramp3[(int)p->ramp];
			p->type = pt_fire;
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() % 6) - 3);
			break;
		case ROCKET_TRAIL:
		default:
			p->ramp = (rand() & 3);
			p->color = ramp3[(int)p->ramp];
			p->type = pt_fire;
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() % 6) - 3);
			break;
		}
		VectorAdd(point, delta, point);
	}
done:
	VectorCopy(point, *trail_origin);
}




// deurk: ported from zquake, thx Tonik
void Classic_ParticleRailTrail(vec3_t start, vec3_t end, int color)
{
	vec3_t          move, vec, right, up, dir;
	float           len, dec, d, c, s;
	int             i, j;
	particle_t      *p;

	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	len = VectorNormalize(vec);

	MakeNormalVectors(vec, right, up);

	// color spiral
	for (i = 0; i < len; i++) {
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->type = pt_rail;

		p->die = r_refdef2.time + 2;

		d = i * 0.1;
		c = cos(d);
		s = sin(d);

		VectorScale(right, c, dir);
		VectorMA(dir, s, up, dir);

		//p->alpha = 1.0;
		//p->alphavel = -1.0 / (1+frand()*0.2);
		p->color = color + (rand() & 7);
		for (j = 0; j < 3; j++) {
			p->org[j] = move[j] + dir[j] * 3;
			p->vel[j] = dir[j] * 2; //p->vel[j] = dir[j]*6;
		}

		VectorAdd(move, vec, move);
	}

	dec = 1.5;
	VectorScale(vec, dec, vec);
	VectorCopy(start, move);

	// white core
	while (len > 0) {
		len -= dec;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->type = pt_rail;

		p->die = r_refdef2.time + 2;

		//p->alpha = 1.0;
		//p->alphavel = -1.0 / (0.6+frand()*0.2);
		p->color = 0x0 + (rand() & 15);

		for (j = 0; j < 3; j++) {
			p->org[j] = move[j] + crand() * 2;
			p->vel[j] = crand()*0.5; //p->vel[j] = crand()*3;
		}

		VectorAdd(move, vec, move);
	}
}

static int particles_to_draw = 0;

// Particles are scaled according to distance from player's POV,
//   so in multi-view we need to recalculate each time
void Classic_ReScaleParticles(void)
{
	float r_partscale = 0.004 * tan(r_refdef.fov_x * (M_PI / 180) * 0.5f);
	int i;

	for (i = 0; i < particles_to_draw; ++i) {
		glm_particle_t* glpart = &glparticles[i];
		float dist = (glpart->gl_org[0] - r_origin[0]) * vpn[0] + (glpart->gl_org[1] - r_origin[1]) * vpn[1] + (glpart->gl_org[2] - r_origin[2]) * vpn[2];

		glpart->gl_scale = 1 + dist * r_partscale;
	}
}

// Moves particles into new locations this frame
void Classic_CalculateParticles(void)
{
	particle_t *p, *kill;
	int i;
	float time2, time3, time1, dvel, frametime, grav;
	unsigned char *at, theAlpha;
	float dist, scale, r_partscale;

	particles_to_draw = 0;
	if (!active_particles || !r_drawparticles.integer) {
		return;
	}

	r_partscale = 0.004 * tan(r_refdef.fov_x * (M_PI / 180) * 0.5f);

	// load texture if not done yet
	if (!GL_TextureReferenceIsValid(particletexture)) {
		Classic_LoadParticleTexures(default_size, default_size);
	}

	frametime = cls.frametime;
	if (ISPAUSED) {
		frametime = 0;
	}
	time3 = frametime * 15;
	time2 = frametime * 10; // 15;
	time1 = frametime * 5;
	grav = frametime * 800 * 0.05;
	dvel = 4 * frametime;

	while (1) {
		kill = active_particles;
		if (kill && kill->die < r_refdef2.time) {
			active_particles = kill->next;
			kill->next = free_particles;
			free_particles = kill;
			continue;
		}
		break;
	}

	for (p = active_particles; p; p = p->next) {
		byte color[4];

		while (1) {
			kill = p->next;
			if (kill && kill->die < r_refdef2.time) {
				p->next = kill->next;
				kill->next = free_particles;
				free_particles = kill;
				continue;
			}
			break;
		}

		// hack a scale up to keep particles from disapearing
		dist = (p->org[0] - r_origin[0]) * vpn[0] + (p->org[1] - r_origin[1]) * vpn[1] + (p->org[2] - r_origin[2]) * vpn[2];
		scale = 1 + dist * r_partscale;

		at = (byte *)&d_8to24table[(int)p->color];
		if (p->type == pt_fire) {
			theAlpha = 255 * (6 - p->ramp) / 6;
		}
		else {
			theAlpha = 255;
		}

		color[0] = *at;
		color[1] = *(at + 1);
		color[2] = *(at + 2);
		color[3] = theAlpha;

		{
			glm_particle_t* glpart = &glparticles[particles_to_draw++];
			glpart->gl_color[0] = color[0] * 1.0 / 255;
			glpart->gl_color[1] = color[1] * 1.0 / 255;
			glpart->gl_color[2] = color[2] * 1.0 / 255;
			glpart->gl_color[3] = color[3] * 1.0 / 255;
			glpart->gl_scale = scale;
			VectorCopy(p->org, glpart->gl_org);
		}

		p->org[0] += p->vel[0] * frametime;
		p->org[1] += p->vel[1] * frametime;
		p->org[2] += p->vel[2] * frametime;

		switch (p->type) {
		case pt_static:
			break;
		case pt_fire:
			p->ramp += time1;
			if (p->ramp >= 6) {
				p->die = -1;
			}
			else {
				p->color = ramp3[(int)p->ramp];
			}
			p->vel[2] += grav;
			break;
		case pt_explode:
			p->ramp += time2;
			if (p->ramp >= 8) {
				p->die = -1;
			}
			else {
				p->color = ramp1[(int)p->ramp];
			}
			for (i = 0; i < 3; i++) {
				p->vel[i] += p->vel[i] * dvel;
			}
			p->vel[2] -= grav * 30;
			break;
		case pt_explode2:
			p->ramp += time3;
			if (p->ramp >= 8) {
				p->die = -1;
			}
			else {
				p->color = ramp2[(int)p->ramp];
			}
			for (i = 0; i < 3; i++) {
				p->vel[i] -= p->vel[i] * frametime;
			}
			p->vel[2] -= grav * 30;
			break;
		case pt_blob:
			for (i = 0; i < 3; i++) {
				p->vel[i] += p->vel[i] * dvel;
			}
			p->vel[2] -= grav;
			break;
		case pt_blob2:
			for (i = 0; i < 2; i++) {
				p->vel[i] -= p->vel[i] * dvel;
			}
			p->vel[2] -= grav;
			break;
		case pt_slowgrav:
		case pt_grav:
			p->vel[2] -= grav;
			break;
		case pt_rail:
			break;
		}
	}
}

// Performs all drawing of particles
void Classic_DrawParticles(void)
{
	int i;
	vec3_t up, right;

	if (particles_to_draw == 0) {
		return;
	}

	VectorScale(vup, 1.5, up);
	VectorScale(vright, 1.5, right);

	GL_BillboardInitialiseBatch(BILLBOARD_PARTICLES_CLASSIC, GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ShadersSupported() ? particletexture_array : particletexture, particletexture_array_index, GL_TRIANGLES, true, false);
	if (GL_BillboardAddEntry(BILLBOARD_PARTICLES_CLASSIC, 3 * particles_to_draw)) {
		for (i = 0; i < particles_to_draw; ++i) {
			glm_particle_t* glpart = &glparticles[i];
			float scale = glpart->gl_scale;
			GLubyte color[4];

			color[0] = glpart->gl_color[0] * glpart->gl_color[3] * 255;
			color[1] = glpart->gl_color[1] * glpart->gl_color[3] * 255;
			color[2] = glpart->gl_color[2] * glpart->gl_color[3] * 255;
			color[3] = glpart->gl_color[3] * 255;

			GL_BillboardAddVert(BILLBOARD_PARTICLES_CLASSIC, glpart->gl_org[0], glpart->gl_org[1], glpart->gl_org[2], 0, 0, color);
			GL_BillboardAddVert(BILLBOARD_PARTICLES_CLASSIC, glpart->gl_org[0] + up[0] * scale, glpart->gl_org[1] + up[1] * scale, glpart->gl_org[2] + up[2] * scale, 1, 0, color);
			GL_BillboardAddVert(BILLBOARD_PARTICLES_CLASSIC, glpart->gl_org[0] + right[0] * scale, glpart->gl_org[1] + right[1] * scale, glpart->gl_org[2] + right[2] * scale, 0, 1, color);
		}
	}

	return;
}

void R_InitParticles(void)
{
	if (!host_initialized) {
		int i;

		Cvar_SetCurrentGroup(CVAR_GROUP_PARTICLES);
		Cvar_Register(&r_particles_count);
		Cvar_Register(&r_drawparticles);
		Cvar_ResetCurrentGroup();

		if ((i = COM_CheckParm("-particles")) && i + 1 < COM_Argc()) {
			Cvar_SetValue(&r_particles_count, Q_atoi(COM_Argv(i + 1)));
		}
	}

	Classic_InitParticles();
	QMB_InitParticles();
}

void R_ParticleFrame(void)
{
	if (!r_worldentity.model || !cl.worldmodel) {
		return;
	}

	Classic_CalculateParticles();
	QMB_CalculateParticles();
}

void R_ClearParticles(void)
{
	Classic_ClearParticles();
	QMB_ClearParticles();
}

void R_DrawParticles(void)
{
	if (!r_drawparticles.integer && !Rulesets_RestrictParticles()) {
		return;
	}

	if (CL_MultiviewEnabled() && CL_MultiviewCurrentView() != 0) {
		Classic_ReScaleParticles();
	}

	Classic_DrawParticles();
	QMB_DrawParticles();
}

#define RunParticleEffect(var, org, dir, color, count)		\
	if (qmb_initialized && gl_part_##var.value)				\
		QMB_RunParticleEffect(org, dir, color, count);		\
	else													\
		Classic_RunParticleEffect(org, dir, color, count);

void R_RunParticleEffect(vec3_t org, vec3_t dir, int color, int count)
{
	if (color == 73 || color == 225) {
		RunParticleEffect(blood, org, dir, color, count);
		return;
	}

	if (color == 256 /* gunshot magic */) {
		RunParticleEffect(gunshots, org, dir, color, count);
		return;
	}

	switch (count) {
	case 10:
	case 20:
	case 30:
		RunParticleEffect(spikes, org, dir, color, count);
		break;
	case 50: // LG Blood
		RunParticleEffect(blood, org, dir, color, count);
		return;
	default:
		RunParticleEffect(gunshots, org, dir, color, count);
	}
}

void R_ParticleTrail(vec3_t start, vec3_t end, vec3_t *trail_origin, trail_type_t type)
{
	if (qmb_initialized && gl_part_trails.value)
		QMB_ParticleTrail(start, end, trail_origin, type);
	else
		Classic_ParticleTrail(start, end, trail_origin, type);
}

#define ParticleFunction(var, name)				\
void R_##name (vec3_t org) {					\
	if (qmb_initialized && gl_part_##var.value)	\
		QMB_##name(org);						\
	else										\
		Classic_##name(org);					\
}

ParticleFunction(explosions, ParticleExplosion);
ParticleFunction(blobs, BlobExplosion);
ParticleFunction(lavasplash, LavaSplash);
ParticleFunction(telesplash, TeleportSplash);

void Part_FlagTexturesForArray(texture_flag_t* texture_flags)
{
	texture_flags[particletexture.index].flags |= (1 << TEXTURETYPES_SPRITES);
}

void Part_ImportTexturesForArrayReferences(texture_flag_t* texture_flags)
{
	texture_array_ref_t* array_ref = &texture_flags[particletexture.index].array_ref[TEXTURETYPES_SPRITES];

	if (GL_TextureWidth(array_ref->ref) != GL_TextureWidth(particletexture) ||
		GL_TextureHeight(array_ref->ref) != GL_TextureHeight(particletexture)
	) {
		int width = GL_TextureWidth(array_ref->ref);
		int height = GL_TextureHeight(array_ref->ref);
		byte* data = Classic_CreateParticleTexture(width, height);

		GL_TexSubImage3D(GL_TEXTURE0, array_ref->ref, 0, 0, 0, array_ref->index, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);

		Q_free(data);
	}

	particletexture_array = array_ref->ref;
	particletexture_array_index = array_ref->index;
	particletexture_scale_s = array_ref->scale_s;
	particletexture_scale_t = array_ref->scale_t;
}
