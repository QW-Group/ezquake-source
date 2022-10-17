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

#ifdef RENDERER_OPTION_MODERN_OPENGL

#include "quakedef.h"
#include "glm_particles.h"
#include "gl_model.h"
#include "gl_local.h"
#include "gl_texture_internal.h"
#include "gl_sprite3d.h"
#include "particles_classic.h"

extern texture_ref particletexture;
texture_ref particletexture_array;
int particletexture_array_index;
float particletexture_array_xpos_tr;
float particletexture_array_ypos_tr;
float particletexture_array_max_s = 1.0f;
float particletexture_array_max_t = 1.0f;
static float particletexture_scale_s;
static float particletexture_scale_t;

byte* Classic_CreateParticleTexture(int width, int height);

void Part_FlagTexturesForArray(texture_flag_t* texture_flags)
{
	if (R_TextureReferenceIsValid(particletexture)) {
		texture_flags[particletexture.index].flags |= (1 << TEXTURETYPES_SPRITES);
	}
}

void Part_ImportTexturesForArrayReferences(texture_flag_t* texture_flags)
{
	if (R_TextureReferenceIsValid(particletexture)) {
		texture_array_ref_t* array_ref = &texture_flags[particletexture.index].array_ref[TEXTURETYPES_SPRITES];

		if (!R_TexturesAreSameSize(array_ref->ref, particletexture)) {
			int width = R_TextureWidth(array_ref->ref);
			int height = R_TextureHeight(array_ref->ref);
			byte* data = Classic_CreateParticleTexture(width, height);

			GL_TexSubImage3D(0, array_ref->ref, 0, 0, 0, array_ref->index, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);

			Q_free(data);
		}

		{
			float width = R_TextureWidth(array_ref->ref);
			float height = R_TextureHeight(array_ref->ref);

			particletexture_array = array_ref->ref;
			particletexture_array_xpos_tr = (width - 1.0f) / width;
			particletexture_array_ypos_tr = (height - 1.0f) / height;
			particletexture_array_max_s = min(width, height) / width;
			particletexture_array_max_t = min(width, height) / height;
			particletexture_array_index = array_ref->index;
			particletexture_scale_s = array_ref->scale_s;
			particletexture_scale_t = array_ref->scale_t;
		}
	}
}

void GLM_LoadParticleTextures(void)
{
	if (R_TextureReferenceIsValid(particletexture_array)) {
		int width = R_TextureWidth(particletexture_array);
		int height = R_TextureHeight(particletexture_array);
		byte* data = Classic_CreateParticleTexture(width, height);

		GL_TexSubImage3D(0, particletexture_array, 0, 0, 0, particletexture_array_index, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
		GL_TextureMipmapGenerate(particletexture_array);

		Q_free(data);
	}
}

void GLM_DrawClassicParticles(int particles_to_draw)
{
	r_sprite3d_vert_t* vert;

	R_Sprite3DInitialiseBatch(SPRITE3D_PARTICLES_CLASSIC, r_state_particles_classic, particletexture_array, particletexture_array_index, r_primitive_triangles);
	vert = R_Sprite3DAddEntry(SPRITE3D_PARTICLES_CLASSIC, 3 * particles_to_draw);
	if (vert) {
		extern r_sprite3d_vert_t glvertices[ABSOLUTE_MAX_PARTICLES * 3];

		memcpy(vert, glvertices, particles_to_draw * 3 * sizeof(glvertices[0]));
	}
}

#endif // #ifdef RENDERER_OPTION_MODERN_OPENGL
