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

typedef enum {
	pt_static, pt_grav, pt_slowgrav, pt_fire, pt_explode, pt_explode2, pt_blob, pt_blob2, pt_rail
} ptype_t;

typedef struct particle_s {
	vec3_t      org;
	float       color;
	vec3_t      vel;
	float       ramp;
	float       die;
	ptype_t     type;
	struct particle_s *next;
} particle_t;

typedef struct glm_particle_s {
	vec3_t      gl_org;
	float       gl_scale;
	float       gl_color[4];
} glm_particle_t;

//#define DEFAULT_NUM_PARTICLES	2048
#define ABSOLUTE_MIN_PARTICLES	512
#define ABSOLUTE_MAX_PARTICLES	8192

cvar_t r_particles_count = {"r_particles_count", "2048"};
cvar_t r_drawparticles = { "r_drawparticles", "1" };

// Which particles to draw this frame
static particle_t particles[ABSOLUTE_MAX_PARTICLES];
static glm_particle_t glparticles[ABSOLUTE_MAX_PARTICLES];

static int	ramp1[8] = {0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61};
static int	ramp2[8] = {0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66};
static int	ramp3[8] = {0x6d, 0x6b, 6, 5, 4, 3};

static particle_t	*active_particles, *free_particles;
static int			r_numparticles;

vec3_t				r_pright, r_pup, r_ppn;

float crand(void)
{
        return (rand()&32767)* (2.0/32767) - 1;
}

void Classic_LoadParticleTexures (void) {
	int	i, x, y;
	unsigned int data[32][32];

	// clear to transparent white
	for (i = 0; i < 32 * 32; i++)
	{
		((unsigned *) data)[i] = LittleLong(0x00FFFFFF);
	}

	// draw a circle in the top left corner or squire, depends of cvar
	for (x = 0; x < 16; x++)
	{
		for (y = 0; y < 16; y++)
		{
			if ( gl_squareparticles.integer || ((x - 7.5) * (x - 7.5) + (y - 7.5) * (y - 7.5) <= 8 * 8))
				data[y][x] = LittleLong(0xFFFFFFFF);	// solid white
		}
	}


	// TEX_NOSCALE - so no affect from gl_picmip and gl_maxsize
	particletexture = GL_LoadTexture("particles:classic", 32, 32, (byte*) data, TEX_MIPMAP | TEX_ALPHA | TEX_NOSCALE, 4);

	if (!particletexture)
		Sys_Error("Classic_LoadParticleTexures: can't load texture");
}

void Classic_AllocParticles (void)
{
	r_numparticles = bound(ABSOLUTE_MIN_PARTICLES, r_particles_count.integer, ABSOLUTE_MAX_PARTICLES);
}

void Classic_InitParticles (void) {
	if (!r_numparticles)
		Classic_AllocParticles ();
	else
		Classic_ClearParticles (); // also re-alloc particles

	Classic_LoadParticleTexures();
}

void Classic_ClearParticles (void) {
	int		i;

	if (!r_numparticles)
		return;

	Classic_AllocParticles ();	// and alloc again
		
	free_particles = &particles[0];
	active_particles = NULL;

	for (i = 0;i < r_numparticles; i++)
		particles[i].next = &particles[i+1];
	particles[r_numparticles - 1].next = NULL;
}

#ifndef CLIENTONLY
void R_ReadPointFile_f (void) {
	vfsfile_t *v;
	char line[1024];
    char *s;
	vec3_t org;
	int c;
	particle_t *p;
	char name[MAX_OSPATH];

	if (!com_serveractive)
		return;

	snprintf (name, sizeof(name), "maps/%s.pts", host_mapname.string);

	if (!(v = FS_OpenVFS(name, "rb", FS_ANY))) {
		Com_Printf ("couldn't open %s\n", name);
		return;
	}

	Com_Printf ("Reading %s...\n", name);
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
			Com_Printf ("Not enough free particles\n");
			break;
		}
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		
		p->die = 99999;
		p->color = (-c)&15;
		p->type = pt_static;
		VectorClear (p->vel);
		VectorCopy (org, p->org);
	}

	VFS_CLOSE(v);
	Com_Printf ("%i points read\n", c);
}
#endif

void Classic_ParticleExplosion (vec3_t org) {
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
		} else {
			p->type = pt_explode2;
			for (j = 0; j < 3; j++) {
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand()%512) - 256;
			}
		}
	}
}

void Classic_BlobExplosion (vec3_t org) {
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
		} else {
			p->type = pt_blob2;
			p->color = 150 + rand() % 6;
			for (j = 0; j < 3; j++) {
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
	}
}

void Classic_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count) {
	int i, j, scale;
	particle_t *p;

	scale = (count > 130) ? 3 : (count > 20) ? 2  : 1;

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

void Classic_LavaSplash (vec3_t org) {
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

				VectorNormalizeFast (dir);
				vel = 50 + (rand() & 63);
				VectorScale (dir, vel, p->vel);
			}
		}
	}
}

void Classic_TeleportSplash (vec3_t org) {
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

				VectorNormalizeFast (dir);
				vel = 50 + (rand() & 63);
				VectorScale (dir, vel, p->vel);
			}
		}
	}
}

void Classic_ParticleTrail (vec3_t start, vec3_t end, vec3_t *trail_origin, trail_type_t type) {
	vec3_t point, delta, dir;
	float len;
	int i, j, num_particles;
	particle_t *p;
	static int tracercount;

	VectorCopy (start, point);
	VectorSubtract (end, start, delta);
	if (!(len = VectorLength (delta)))
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

	if (!(num_particles = (int) len))
		goto done;

	VectorScale (delta, 1.0 / num_particles, delta);

	for (i = 0; i < num_particles && free_particles; i++) {
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorClear (p->vel);
		p->die = r_refdef2.time + 2;

		switch(type) {		
		case GRENADE_TRAIL:
			p->ramp = (rand() & 3) + 2;
			p->color = ramp3[(int) p->ramp];
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

			VectorCopy (point, p->org);
			if (tracercount & 1) {
				p->vel[0] = 90 * dir[1];
				p->vel[1] = 90 * -dir[0];
			} else {
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
			p->color = ramp3[(int) p->ramp];
			p->type = pt_fire;
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() % 6) - 3);
			break;
		case ROCKET_TRAIL:
		default:		
			p->ramp = (rand() & 3);
			p->color = ramp3[(int) p->ramp];
			p->type = pt_fire;
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() % 6) - 3);
			break;
		}
		VectorAdd (point, delta, point);
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

static glm_program_t billboardProgram;
static int billboard_modelViewMatrix;
static int billboard_projectionMatrix;
static int billboard_materialTex;
static int billboard_apply_texture;
static int billboard_alpha_texture;

qbool GLM_CreateVGFProgram(const char* friendlyName, const char* vertex_shader_text, const char* geometry_shader_text, const char* fragment_shader_text, glm_program_t* program);

void GLM_CreateParticleProgram()
{
	if (!billboardProgram.program) {
		const char* vertexShaderText =
			"#version 430\n"
			"\n"
			"layout(location = 0) in vec3 position;\n"
			"layout(location = 1) in float scale;\n"
			"layout(location = 2) in vec4 colour;\n"
			"\n"
			"out float pointScale;\n"
			"out vec4 pointColour;\n"
			"\n"
			"uniform mat4 modelViewMatrix;\n"
			"uniform mat4 projectionMatrix;\n"
			"\n"
			"void main()\n"
			"{\n"
			"    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1);\n"
			"    pointScale = scale;\n"
			"    pointColour = colour;\n"
			"}\n";

		// Geometry shader converts single points to billboards
		const char* geometryShaderText =
			"#version 430\n"
			"\n"
			"layout(points) in;\n"
			"layout(triangle_strip, max_vertices = 4) out;\n"
			"\n"
			"in float pointScale[1];\n"
			"in vec4 pointColour[1];\n"
			"\n"
			"out vec2 TextureCoord;\n"
			"out vec4 fragColour;\n"
			"\n"
			"void main() {\n"
			"    float size = pointScale[0];\n"
			"    fragColour = pointColour[0];\n"
			"\n"
			"    gl_Position = gl_in[0].gl_Position + vec4(0, 0.0, 0.0, 0.0);\n"
			"    TextureCoord = vec2(0, 0);\n"
			"    EmitVertex();\n"
			"\n"
			"    gl_Position = gl_in[0].gl_Position + vec4(0, size, 0.0, 0.0);\n"
			"    TextureCoord = vec2(0, 1);\n"
			"    EmitVertex();\n"
			"\n"
			"    gl_Position = gl_in[0].gl_Position + vec4(size, 0, 0.0, 0.0);\n"
			"    TextureCoord = vec2(1, 0);\n"
			"    EmitVertex();\n"
			"\n"
			"    gl_Position = gl_in[0].gl_Position + vec4(size, size, 0.0, 0.0);\n"
			"    TextureCoord = vec2(1,1);\n"
			"    EmitVertex();\n"
			"\n"
			"    EndPrimitive();\n"
			"}\n";

		const char* fragmentShaderText =
			"#version 430\n"
			"\n"
			"uniform sampler2D materialTex;\n"
			"uniform bool apply_texture;\n"
			"uniform bool alpha_texture;\n"
			"\n"
			"in vec2 TextureCoord;\n"
			"in vec4 fragColour;\n"
			"out vec4 frag_colour;\n"
			"\n"
			"void main()\n"
			"{\n"
			"    vec4 texColor;\n"
			"    vec4 lmColor;\n"
			"\n"
			"    if (apply_texture) {\n"
			"        texColor = texture(materialTex, TextureCoord);\n"
			"        if (alpha_texture && texColor.a != 1.0) {\n"
			"            discard;"
			"        }\n"
			"    }\n"
			"    else {\n"
			"        texColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
			"    }\n"
			"    frag_colour = texColor * fragColour;"
			"}\n";

		// Initialise program for drawing image
		GLM_CreateVGFProgram("Billboards", vertexShaderText, geometryShaderText, fragmentShaderText, &billboardProgram);

		billboard_modelViewMatrix = glGetUniformLocation(billboardProgram.program, "modelViewMatrix");
		billboard_projectionMatrix = glGetUniformLocation(billboardProgram.program, "projectionMatrix");
		billboard_materialTex = glGetUniformLocation(billboardProgram.program, "materialTex");
		billboard_apply_texture = glGetUniformLocation(billboardProgram.program, "apply_texture");
		billboard_alpha_texture = glGetUniformLocation(billboardProgram.program, "alpha_texture");
	}
}

static GLuint particleVBO;
static GLuint particleVAO;

static GLuint GLM_CreateParticleVAO(void)
{
	if (!particleVBO) {
		glGenBuffers(1, &particleVBO);
		glBindBufferExt(GL_ARRAY_BUFFER, particleVBO);
		glBufferDataExt(GL_ARRAY_BUFFER, sizeof(particles), glparticles, GL_STATIC_DRAW);
	}

	if (!particleVAO) {
		glGenVertexArrays(1, &particleVAO);
		glBindVertexArray(particleVAO);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);
		glBindBufferExt(GL_ARRAY_BUFFER, particleVBO);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm_particle_t), (void*) 0);
		glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(glm_particle_t), (void*) (sizeof(float) * 3));
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(glm_particle_t), (void*) (sizeof(float) * 4));
	}

	return particleVAO;
}

void GLM_DrawParticles(int number, qbool square)
{
	if (!billboardProgram.program) {
		GLM_CreateParticleProgram();
	}

	{
		GLuint vao = GLM_CreateParticleVAO();

		if (billboardProgram.program && vao) {
			float modelViewMatrix[16];
			float projectionMatrix[16];

			GLM_GetMatrix(GL_MODELVIEW, modelViewMatrix);
			GLM_GetMatrix(GL_PROJECTION, projectionMatrix);

			glUseProgram(billboardProgram.program);
			glUniformMatrix4fv(billboard_modelViewMatrix, 1, GL_FALSE, modelViewMatrix);
			glUniformMatrix4fv(billboard_projectionMatrix, 1, GL_FALSE, projectionMatrix);
			glUniform1i(billboard_materialTex, 0);
			glUniform1i(billboard_apply_texture, !square);
			glUniform1i(billboard_alpha_texture, 0);

			glBindVertexArray(vao);
			glDrawArrays(GL_POINTS, 0, number);
		}
	}
}

void GLM_DrawParticle(byte* color, vec3_t origin, float scale, qbool square)
{
	float oldMatrix[16];

	if (!billboardProgram.program) {
		GLM_CreateParticleProgram();
	}

	{
		GLuint vao = GLM_CreateParticleVAO();

		GL_PushMatrix(GL_MODELVIEW_MATRIX, oldMatrix);
		GL_Translate(GL_MODELVIEW, origin[0], origin[1], origin[2]);

		if (billboardProgram.program && vao) {
			float modelViewMatrix[16];
			float projectionMatrix[16];

			GLM_GetMatrix(GL_MODELVIEW, modelViewMatrix);
			GLM_GetMatrix(GL_PROJECTION, projectionMatrix);

			glUseProgram(billboardProgram.program);
			glUniformMatrix4fv(billboard_modelViewMatrix, 1, GL_FALSE, modelViewMatrix);
			glUniformMatrix4fv(billboard_projectionMatrix, 1, GL_FALSE, projectionMatrix);
			glUniform1i(billboard_materialTex, 0);
			glUniform1i(billboard_apply_texture, !square);
			glUniform1i(billboard_alpha_texture, 0);

			glBindVertexArray(vao);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, square ? 4 : 3);
		}

		GL_PopMatrix(GL_MODELVIEW_MATRIX, oldMatrix);
	}
}

void Classic_DrawParticles(void)
{
	particle_t *p, *kill;
	int i;
	float time2, time3, time1, dvel, frametime, grav;
	unsigned char *at, theAlpha;
	vec3_t up, right;
	float dist, scale, r_partscale;
	extern cvar_t gl_particle_style;
	float oldModelViewMatrix[16];
	int particles_to_draw = 0;

	if (!active_particles) {
		return;
	}

	r_partscale = 0.004 * tan(r_refdef.fov_x * (M_PI / 180) * 0.5f);

	// load texture if not done yet
	if (!particletexture) {
		Classic_LoadParticleTexures();
	}

	if (GL_ShadersSupported()) {
		glActiveTexture(GL_TEXTURE0);
	}
	GL_Bind(particletexture);

	GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	if (!gl_solidparticles.value) {
		glDepthMask(GL_FALSE);
	}

	GL_TextureEnvMode(GL_MODULATE);
	if (!GL_ShadersSupported()) {
		if (gl_particle_style.integer) {
			// for sw style particles
			glDisable(GL_TEXTURE_2D); // don't use texture
			glBegin(GL_QUADS);
		}
		else {
			glBegin(GL_TRIANGLES);
		}
	}

	VectorScale(vup, 1.5, up);
	VectorScale(vright, 1.5, right);

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

		if (GL_ShadersSupported()) {
			glm_particle_t* glpart = &glparticles[particles_to_draw++];
			glpart->gl_color[0] = color[0] * 1.0 / 255;
			glpart->gl_color[1] = color[1] * 1.0 / 255;
			glpart->gl_color[2] = color[2] * 1.0 / 255;
			glpart->gl_color[3] = color[3] * 1.0 / 255;
			glpart->gl_scale = scale;
			VectorCopy(p->org, glpart->gl_org);
		}
		else {
			glColor4ubv(color);
			glTexCoord2f(0, 0); glVertex3fv(p->org);
			glTexCoord2f(1, 0); glVertex3f(p->org[0] + up[0] * scale, p->org[1] + up[1] * scale, p->org[2] + up[2] * scale);

			if (gl_particle_style.integer) {
				//4th point for sw style particle
				glTexCoord2f(1, 1); glVertex3f(p->org[0] + (right[0] + up[0]) * scale, p->org[1] + (right[1] + up[1]) * scale, p->org[2] + (right[2] + up[2]) * scale);
			}
			glTexCoord2f(0, 1); glVertex3f(p->org[0] + right[0] * scale, p->org[1] + right[1] * scale, p->org[2] + right[2] * scale);
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

	if (!GL_ShadersSupported()) {
		glEnd();
		glEnable(GL_TEXTURE_2D);
		glColor3ubv(color_white);
	}
	else {
		GL_PushMatrix(GL_MODELVIEW_MATRIX, oldModelViewMatrix);
		/*
		{
			void* buffer = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
			memcpy(buffer, particles, sizeof(particles[0]) * particles_to_draw);
			glUnmapBuffer(GL_ARRAY_BUFFER);
		}*/
		glBindBufferExt(GL_ARRAY_BUFFER, particleVBO);
		glBufferDataExt(GL_ARRAY_BUFFER, sizeof(glparticles[0]) * particles_to_draw, glparticles, GL_STATIC_DRAW);
		GLM_DrawParticles(particles_to_draw, gl_particle_style.integer);

		GL_PopMatrix(GL_MODELVIEW_MATRIX, oldModelViewMatrix);
	}
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	glDepthMask(GL_TRUE);
	GL_TextureEnvMode(GL_REPLACE);
}



void R_InitParticles(void) {
	if (!host_initialized) {
		int i;

		Cvar_SetCurrentGroup(CVAR_GROUP_PARTICLES);
		Cvar_Register (&r_particles_count);
		Cvar_Register (&r_drawparticles);
		Cvar_ResetCurrentGroup();

		if ((i = COM_CheckParm ("-particles")) && i + 1 < COM_Argc())
			Cvar_SetValue(&r_particles_count, Q_atoi(COM_Argv(i + 1)));
	}

	Classic_InitParticles();
	QMB_InitParticles();
}

void R_ClearParticles(void) {
	Classic_ClearParticles();
	QMB_ClearParticles();
}

void R_DrawParticles(void)
{
	if (!r_drawparticles.integer) {
		return;
	}

	if (GL_ShadersSupported()) {
		Classic_DrawParticles();
		//QMB_DrawParticles();
	}
	else {
		Classic_DrawParticles();
		//QMB_DrawParticles();
	}
}

#define RunParticleEffect(var, org, dir, color, count)		\
	if (qmb_initialized && gl_part_##var.value)				\
		QMB_RunParticleEffect(org, dir, color, count);		\
	else													\
		Classic_RunParticleEffect(org, dir, color, count);

void R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count) {
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

void R_ParticleTrail (vec3_t start, vec3_t end, vec3_t *trail_origin, trail_type_t type) {
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
