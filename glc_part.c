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

extern particle_t particles[ABSOLUTE_MAX_PARTICLES];
extern glm_particle_t glparticles[ABSOLUTE_MAX_PARTICLES];

void GLC_DrawParticles(int particles_to_draw, qbool square)
{
	extern cvar_t gl_particle_style;
	vec3_t up, right;
	int i;

	if (gl_particle_style.integer) {
		// for sw style particles
		glDisable(GL_TEXTURE_2D); // don't use texture
		glBegin(GL_QUADS);
	}
	else {
		glBegin(GL_TRIANGLES);
	}

	VectorScale(vup, 1.5, up);
	VectorScale(vright, 1.5, right);

	for (i = 0; i < particles_to_draw; ++i) {
		glm_particle_t* glpart = &glparticles[i];
		float scale = glpart->gl_scale;

		glColor4fv(glpart->gl_color);
		glTexCoord2f(0, 0); glVertex3fv(glpart->gl_org);
		glTexCoord2f(1, 0); glVertex3f(glpart->gl_org[0] + up[0] * scale, glpart->gl_org[1] + up[1] * scale, glpart->gl_org[2] + up[2] * scale);

		if (gl_particle_style.integer) {
			//4th point for sw style particle
			glTexCoord2f(1, 1); glVertex3f(glpart->gl_org[0] + (right[0] + up[0]) * scale, glpart->gl_org[1] + (right[1] + up[1]) * scale, glpart->gl_org[2] + (right[2] + up[2]) * scale);
		}
		glTexCoord2f(0, 1); glVertex3f(glpart->gl_org[0] + right[0] * scale, glpart->gl_org[1] + right[1] * scale, glpart->gl_org[2] + right[2] * scale);
	}
	glEnd();
	glEnable(GL_TEXTURE_2D);
	glColor3ubv(color_white);
}
