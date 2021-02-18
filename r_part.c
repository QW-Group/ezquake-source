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
#include "particles_classic.h"
#include "r_sprite3d.h"
#include "rulesets.h"
#include "r_texture.h"
#include "qmb_particles.h"
#include "glm_particles.h"
#include "r_state.h"
#include "r_local.h"
#include "r_renderer.h"
#include "r_framestats.h"

#ifndef RENDERER_OPTION_MODERN_OPENGL
int particletexture_array_index = 0;
float particletexture_array_xpos_tr = 0.9999f;
float particletexture_array_ypos_tr = 0.9999f;
float particletexture_array_max_s = 1.0f;
float particletexture_array_max_t = 1.0f;
#else
extern float particletexture_array_max_s;
extern float particletexture_array_max_t;
#endif

texture_ref particletexture;
const int default_size = 32;

extern cvar_t gl_part_blood;
extern cvar_t gl_part_gunshots;
extern cvar_t gl_part_trails;
extern cvar_t gl_part_spikes;
extern cvar_t gl_part_explosions;
extern cvar_t gl_part_blobs;
extern cvar_t gl_part_lavasplash;
extern cvar_t gl_part_telesplash;

cvar_t r_particles_count = { "r_particles_count", "2048" };
static cvar_t r_drawparticles = { "r_drawparticles", "1" };

// Which particles to draw this frame
typedef struct glm_particle_s {
	vec3_t      gl_org;
	float       gl_scale;
	byte        gl_color[4];
} glm_particle_t;

qparticle_t particles[ABSOLUTE_MAX_PARTICLES];
glm_particle_t glparticles[ABSOLUTE_MAX_PARTICLES];
r_sprite3d_vert_t glvertices[ABSOLUTE_MAX_PARTICLES * 3];

static int	ramp1[8] = { 0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61 };
static int	ramp2[8] = { 0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66 };
static int	ramp3[8] = { 0x6d, 0x6b, 6, 5, 4, 3 };

static int r_numparticles;
static int r_numactiveparticles;

float crand(void)
{
	return (rand() & 32767) * (2.0 / 32767) - 1;
}

byte* Classic_CreateParticleTexture(int width, int height)
{
	byte* data = Q_malloc(sizeof(byte) * width * height * 4);
	int x, y;
	int size = min(width, height);
	float quarter = size / 4.0f - 0.5f;
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
	for (x = width - 4; x < width; ++x) {
		for (y = height - 4; y < height; ++y) {
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
	// TEX_NOSCALE - so no affect from gl_picmip and gl_maxsize
	byte* data = Classic_CreateParticleTexture(width, height);
	particletexture = R_LoadTexture("particles:classic", width, height, data, TEX_MIPMAP | TEX_ALPHA | TEX_NOSCALE, 4);
	renderer.TextureWrapModeClamp(particletexture);
	Q_free(data);

	if (!R_TextureReferenceIsValid(particletexture)) {
		Sys_Error("Classic_LoadParticleTexures: can't load texture");
		return;
	}

#ifdef RENDERER_OPTION_MODERN_OPENGL
	if (R_UseModernOpenGL()) {
		GLM_LoadParticleTextures();
	}
#endif
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
	if (!r_numparticles) {
		return;
	}

	Classic_AllocParticles();	// and alloc again

	r_numactiveparticles = 0;
}

#ifndef CLIENTONLY
void R_ReadPointFile_f(void)
{
	vfsfile_t *v;
	char line[1024];
	const char *s;
	vec3_t org;
	int c;
	qparticle_t *p;
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
		if (r_numactiveparticles >= r_numparticles) {
			Com_Printf("Not enough free particles\n");
			break;
		}
		p = &particles[r_numactiveparticles++];

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
	qparticle_t	*p;

	if (r_explosiontype.value != 9) {
		CL_ExplosionSprite(org);
	}

	if (r_explosiontype.value == 1) {
		return;
	}

	for (i = 0; i < 1024; i++) {
		if (r_numactiveparticles >= r_numparticles) {
			return;
		}
		p = &particles[r_numactiveparticles++];

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
	qparticle_t *p;

	for (i = 0; i < 1024; i++) {
		if (r_numactiveparticles >= r_numparticles) {
			return;
		}
		p = &particles[r_numactiveparticles++];

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
	qparticle_t *p;

	scale = (count > 130) ? 3 : (count > 20) ? 2 : 1;

	if (color == 256)	// gunshot magic
		color = 0;

	for (i = 0; i < count; i++) {
		if (r_numactiveparticles >= r_numparticles) {
			return;
		}
		p = &particles[r_numactiveparticles++];

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
	qparticle_t *p;
	float vel;
	vec3_t dir;

	for (i = -16; i < 16; i++) {
		for (j = -16; j < 16; j++) {
			for (k = 0; k < 1; k++) {
				if (r_numactiveparticles >= r_numparticles) {
					return;
				}
				p = &particles[r_numactiveparticles++];

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
	qparticle_t *p;
	float vel;
	vec3_t dir;

	for (i = -16; i < 16; i += 4) {
		for (j = -16; j < 16; j += 4) {
			for (k = -24; k < 32; k += 4) {
				if (r_numactiveparticles >= r_numparticles) {
					return;
				}
				p = &particles[r_numactiveparticles++];

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

void Classic_ParticleTrail(vec3_t start, vec3_t end, vec3_t* trail_stop, trail_type_t type)
{
	vec3_t point, delta, dir;
	float len;
	int i, j, num_particles;
	qparticle_t *p;
	static int tracercount;

	VectorCopy(start, point);
	VectorSubtract(end, start, delta);
	if (!(len = VectorLength(delta))) {
		goto done;
	}
	VectorScale(delta, 1 / len, dir);	//unit vector in direction of trail

	switch (type) {
	case ALT_ROCKET_TRAIL:
		len /= 1.5; break;
	case BLOOD_TRAIL:
		len /= 6; break;
	default:
		len /= 3; break;
	}

	if (!(num_particles = (int)len)) {
		goto done;
	}

	VectorScale(delta, 1.0 / num_particles, delta);

	for (i = 0; i < num_particles && r_numactiveparticles < r_numparticles; i++) {
		p = &particles[r_numactiveparticles++];

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
	if (trail_stop) {
		VectorCopy(point, *trail_stop);
	}
}

// deurk: ported from zquake, thx Tonik
void Classic_ParticleRailTrail(vec3_t start, vec3_t end, int color)
{
	vec3_t          move, vec, right, up, dir;
	float           len, dec, d, c, s;
	int             i, j;
	qparticle_t      *p;

	VectorCopy(start, move);
	VectorSubtract(end, start, vec);
	len = VectorNormalize(vec);

	MakeNormalVectors(vec, right, up);

	// color spiral
	for (i = 0; i < len; i++) {
		if (r_numactiveparticles >= r_numparticles) {
			return;
		}
		p = &particles[r_numactiveparticles++];

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

		if (r_numactiveparticles >= r_numparticles) {
			return;
		}
		p = &particles[r_numactiveparticles++];

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
	vec3_t up, right, scaled_vpn;
	r_sprite3d_vert_t* vert;
	float dist_precalc = 0;

	if (particles_to_draw == 0) {
		return;
	}

	vert = glvertices;
	VectorScale(vup, 1.5, up);
	VectorScale(vright, 1.5, right);
	VectorScale(vpn, r_partscale, scaled_vpn);

	dist_precalc = 1 - DotProduct(r_origin, scaled_vpn);
	for (i = 0; i < particles_to_draw; ++i, vert += 3) {
		glm_particle_t* glpart = &glparticles[i];
		float scale = glpart->gl_scale = dist_precalc + DotProduct(glpart->gl_org, scaled_vpn);

		R_Sprite3DSetVert(vert, glpart->gl_org[0], glpart->gl_org[1], glpart->gl_org[2], 0, 0, vert->color, particletexture_array_index);
		R_Sprite3DSetVert(vert + 1, glpart->gl_org[0] + up[0] * scale, glpart->gl_org[1] + up[1] * scale, glpart->gl_org[2] + up[2] * scale, particletexture_array_max_s, 0, vert->color, particletexture_array_index);
		R_Sprite3DSetVert(vert + 2, glpart->gl_org[0] + right[0] * scale, glpart->gl_org[1] + right[1] * scale, glpart->gl_org[2] + right[2] * scale, 0, particletexture_array_max_t, vert->color, particletexture_array_index);
	}
}

// Prepares particles to be drawn this frame
static void Classic_PrepareParticles(void)
{
	qparticle_t* p;
	unsigned char *at;
	float theAlpha;
	float scale;
	vec3_t up, right;
	int i;
	float dist_precalc;
	vec3_t scaled_vpn;

	particles_to_draw = 0;
	if (r_numactiveparticles == 0 || (!r_drawparticles.integer && !Rulesets_RestrictParticles())) {
		return;
	}

	frameStats.particle_count = r_numactiveparticles;

	VectorScale(vup, 1.5, up);
	VectorScale(vright, 1.5, right);
	VectorScale(vpn, 0.004 * r_refdef2.distanceScale, scaled_vpn);

	// load texture if not done yet
	if (!R_TextureReferenceIsValid(particletexture)) {
		Classic_LoadParticleTexures(default_size, default_size);
	}

	p = particles;
	dist_precalc = 1 - DotProduct(r_origin, scaled_vpn);
	for (i = 0; i < r_numactiveparticles; ++i, ++p) {
		if (p->die >= r_refdef2.time) {
			glm_particle_t* glpart;
			r_sprite3d_vert_t* vert;

			// hack a scale up to keep particles from disapearing
			scale = dist_precalc + DotProduct(p->org, scaled_vpn);

			glpart = &glparticles[particles_to_draw];
			vert = &glvertices[particles_to_draw * 3];
			at = (byte *)&d_8to24table[(int)p->color];

			if (p->type == pt_fire) {
				theAlpha = (6 - p->ramp) / 6;
				glpart->gl_color[0] = at[0] * theAlpha;
				glpart->gl_color[1] = at[1] * theAlpha;
				glpart->gl_color[2] = at[2] * theAlpha;
				glpart->gl_color[3] = 255 * theAlpha;
			}
			else {
				glpart->gl_color[0] = at[0];
				glpart->gl_color[1] = at[1];
				glpart->gl_color[2] = at[2];
				glpart->gl_color[3] = 255;
			}

			glpart->gl_scale = scale;
			VectorCopy(p->org, glpart->gl_org);

			*((unsigned int*)vert->color) = *(unsigned int*)glpart->gl_color;
			VectorSet(vert->tex, 0, 0, particletexture_array_index);
			VectorSet(vert->position, glpart->gl_org[0], glpart->gl_org[1], glpart->gl_org[2]);
			++vert;

			*((unsigned int*)vert->color) = *(unsigned int*)glpart->gl_color;
			VectorSet(vert->tex, particletexture_array_max_s, 0, particletexture_array_index);
			VectorSet(vert->position, glpart->gl_org[0] + up[0] * scale, glpart->gl_org[1] + up[1] * scale, glpart->gl_org[2] + up[2] * scale);
			++vert;

			*((unsigned int*)vert->color) = *(unsigned int*)glpart->gl_color;
			VectorSet(vert->tex, 0, particletexture_array_max_t, particletexture_array_index);
			VectorSet(vert->position, glpart->gl_org[0] + right[0] * scale, glpart->gl_org[1] + right[1] * scale, glpart->gl_org[2] + right[2] * scale);
			++vert;

			++particles_to_draw;
		}
	}
}

void R_InitParticles(void)
{
	if (!host_initialized) {
		int i;

		Cvar_SetCurrentGroup(CVAR_GROUP_PARTICLES);
		Cvar_Register(&r_particles_count);
		Cvar_Register(&r_drawparticles);
		Cvar_ResetCurrentGroup();

		if ((i = COM_CheckParm(cmdline_param_client_particlecount)) && i + 1 < COM_Argc()) {
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

	Classic_PrepareParticles();
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

	if (particles_to_draw) {
		renderer.DrawClassicParticles(particles_to_draw);
	}
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

// Used for temporary entities and untrackable effects
void R_ParticleTrail(vec3_t start, vec3_t end, trail_type_t type)
{
	if (qmb_initialized && gl_part_trails.integer) {
		QMB_ParticleTrail(start, end, type);
	}
	else {
		Classic_ParticleTrail(start, end, NULL, type);
	}
}

void R_EntityParticleTrail(centity_t* cent, trail_type_t type)
{
	if (qmb_initialized && gl_part_trails.integer) {
		QMB_EntityParticleTrail(cent, type);
	}
	else {
		Classic_ParticleTrail(cent->trails[0].stop, cent->lerp_origin, &cent->trails[0].stop, type);
	}
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

// Moves particles into new locations this frame
static void Classic_MoveParticles(void)
{
	qparticle_t *p;
	int i;
	float time2, time3, time1, dvel, frametime, grav;

	if (r_numactiveparticles == 0) {
		return;
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

	p = particles;
	for (i = 0; i < r_numactiveparticles; ++i, ++p) {
		while (p->die < r_refdef2.time) {
			if (i >= r_numactiveparticles - 1) {
				goto finished;
			}

			*p = particles[r_numactiveparticles - 1];
			--r_numactiveparticles;
		}

		VectorMA(p->org, frametime, p->vel, p->org);

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
				VectorMA(p->vel, dvel, p->vel, p->vel);
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
				VectorMA(p->vel, -frametime, p->vel, p->vel);
				p->vel[2] -= grav * 30;
				break;
			case pt_blob:
				VectorMA(p->vel, dvel, p->vel, p->vel);
				p->vel[2] -= grav;
				break;
			case pt_blob2:
				VectorMA(p->vel, -dvel, p->vel, p->vel);
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

finished:
	return;
}

void R_ParticleEndFrame(void)
{
	Classic_MoveParticles();
}
