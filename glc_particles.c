/*
Copyright (C) 2018 ezQuake team.

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
#include "gl_local.h"
#include "gl_sprite3d.h"
#include "glm_particles.h"
#include "particles_classic.h"

void GLC_DrawClassicParticles(int particles_to_draw)
{
	extern texture_ref particletexture;
	r_sprite3d_vert_t* vert;

	GL_Sprite3DInitialiseBatch(SPRITE3D_PARTICLES_CLASSIC, r_state_particles_classic, r_state_particles_classic, particletexture, particletexture_array_index, r_primitive_triangles);
	vert = GL_Sprite3DAddEntry(SPRITE3D_PARTICLES_CLASSIC, 3 * particles_to_draw);
	if (vert) {
		extern r_sprite3d_vert_t glvertices[ABSOLUTE_MAX_PARTICLES * 3];

		memcpy(vert, glvertices, particles_to_draw * 3 * sizeof(glvertices[0]));
	}
}
