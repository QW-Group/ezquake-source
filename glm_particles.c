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
#include "glm_particles.h"
#include "gl_model.h"
#include "gl_local.h"

extern texture_ref particletexture;
texture_ref particletexture_array;
int particletexture_array_index;
static float particletexture_scale_s;
static float particletexture_scale_t;

byte* Classic_CreateParticleTexture(int width, int height);

void Part_FlagTexturesForArray(texture_flag_t* texture_flags)
{
	if (GL_TextureReferenceIsValid(particletexture)) {
		texture_flags[particletexture.index].flags |= (1 << TEXTURETYPES_SPRITES);
	}
}

void Part_ImportTexturesForArrayReferences(texture_flag_t* texture_flags)
{
	if (GL_TextureReferenceIsValid(particletexture)) {
		texture_array_ref_t* array_ref = &texture_flags[particletexture.index].array_ref[TEXTURETYPES_SPRITES];

		if (GL_TextureWidth(array_ref->ref) != GL_TextureWidth(particletexture) || GL_TextureHeight(array_ref->ref) != GL_TextureHeight(particletexture)) {
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
}

void GLM_LoadParticleTextures(void)
{
	if (GL_TextureReferenceIsValid(particletexture_array)) {
		int width = GL_TextureWidth(particletexture_array);
		int height = GL_TextureHeight(particletexture_array);
		byte* data = Classic_CreateParticleTexture(width, height);

		GL_TexSubImage3D(GL_TEXTURE0, particletexture_array, 0, 0, 0, particletexture_array_index, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
		GL_GenerateMipmap(GL_TEXTURE0, particletexture_array);

		Q_free(data);
	}
}
