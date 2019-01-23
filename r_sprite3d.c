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

// 3D sprites
#include "quakedef.h"
#include "gl_model.h"
#include "r_sprite3d.h"
#include "r_sprite3d_internal.h"
#include "r_state.h"
#include "r_buffers.h"
#include "tr_types.h"
#include "r_renderer.h"
#include "r_texture.h"

int indexes_start_quads, indexes_start_flashblend, indexes_start_sparks;

static const char* batch_type_names[] = {
	"ENTITIES",
	"PARTICLES_CLASSIC",
	"PARTICLES_NEW_p_spark",
	"PARTICLES_NEW_p_smoke",
	"PARTICLES_NEW_p_fire",
	"PARTICLES_NEW_p_bubble",
	"PARTICLES_NEW_p_lavasplash",
	"PARTICLES_NEW_p_gunblast",
	"PARTICLES_NEW_p_chunk",
	"PARTICLES_NEW_p_shockwave",
	"PARTICLES_NEW_p_inferno_flame",
	"PARTICLES_NEW_p_inferno_trail",
	"PARTICLES_NEW_p_sparkray",
	"PARTICLES_NEW_p_staticbubble",
	"PARTICLES_NEW_p_trailpart",
	"PARTICLES_NEW_p_dpsmoke",
	"PARTICLES_NEW_p_dpfire",
	"PARTICLES_NEW_p_teleflare",
	"PARTICLES_NEW_p_blood1",
	"PARTICLES_NEW_p_blood2",
	"PARTICLES_NEW_p_blood3",

	//VULT PARTICLES
	"PARTICLES_NEW_p_rain",
	"PARTICLES_NEW_p_alphatrail",
	"PARTICLES_NEW_p_railtrail",
	"PARTICLES_NEW_p_streak",
	"PARTICLES_NEW_p_streaktrail",
	"PARTICLES_NEW_p_streakwave",
	"PARTICLES_NEW_p_lightningbeam",
	"PARTICLES_NEW_p_vxblood",
	"PARTICLES_NEW_p_lavatrail",
	"PARTICLES_NEW_p_vxsmoke",
	"PARTICLES_NEW_p_vxsmoke_red",
	"PARTICLES_NEW_p_muzzleflash",
	"PARTICLES_NEW_p_inferno",
	"PARTICLES_NEW_p_2dshockwave",
	"PARTICLES_NEW_p_vxrocketsmoke",
	"PARTICLES_NEW_p_trailbleed",
	"PARTICLES_NEW_p_bleedspike",
	"PARTICLES_NEW_p_flame",
	"PARTICLES_NEW_p_bubble2",
	"PARTICLES_NEW_p_bloodcloud",
	"PARTICLES_NEW_p_chunkdir",
	"PARTICLES_NEW_p_smallspark",
	"PARTICLES_NEW_p_slimeglow",
	"PARTICLES_NEW_p_slimebubble",
	"PARTICLES_NEW_p_blacklavasmoke",
	"PARTICLES_NEW_p_entitytrail",
	"PARTICLES_NEW_p_flametorch",
	"FLASHBLEND_LIGHTS",
	"CORONATEX_STANDARD",
	"CORONATEX_GUNFLASH",
	"CORONATEX_EXPLOSIONFLASH1",
	"CORONATEX_EXPLOSIONFLASH2",
	"CORONATEX_EXPLOSIONFLASH3",
	"CORONATEX_EXPLOSIONFLASH4",
	"CORONATEX_EXPLOSIONFLASH5",
	"CORONATEX_EXPLOSIONFLASH6",
	"CORONATEX_EXPLOSIONFLASH7",
	"CHATICON_AFK_CHAT",
	"CHATICON_CHAT",
	"CHATICON_AFK"
};

#ifdef C_ASSERT
C_ASSERT(sizeof(batch_type_names) / sizeof(batch_type_names[0]) == MAX_SPRITE3D_BATCHES);
#endif

r_sprite3d_vert_t verts[MAX_VERTS_PER_SCENE];
gl_sprite3d_batch_t batches[MAX_SPRITE3D_BATCHES];
unsigned int batchMapping[MAX_SPRITE3D_BATCHES];
unsigned int batchCount;
unsigned int vertexCount;
static unsigned int indexData[INDEXES_MAX_QUADS * 4 + INDEXES_MAX_SPARKS * 9 + INDEXES_MAX_FLASHBLEND * 18 + (INDEXES_MAX_QUADS + INDEXES_MAX_SPARKS + INDEXES_MAX_FLASHBLEND) * 3];
static int sprites_in_batch[sprite_vertpool_count];

sprite_vertpool_id batch_to_vertpool[] = {
	sprite_vertpool_game_entities, // SPRITE3D_ENTITIES,
	sprite_vertpool_simple_particles, // SPRITE3D_PARTICLES_CLASSIC,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_spark,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_smoke,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_fire,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_bubble,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_lavasplash,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_gunblast,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_chunk,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_shockwave,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_inferno_flame,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_inferno_trail,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_sparkray,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_staticbubble,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_trailpart,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_dpsmoke,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_dpfire,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_teleflare,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_blood1,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_blood2,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_blood3,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_rain,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_alphatrail,
	sprite_vertpool_game_annotations, // SPRITE3D_PARTICLES_NEW_p_railtrail,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_streak,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_streaktrail,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_streakwave,
	sprite_vertpool_game_annotations, // SPRITE3D_PARTICLES_NEW_p_lightningbeam,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_vxblood,
	sprite_vertpool_qmb_particles_environment, // SPRITE3D_PARTICLES_NEW_p_lavatrail,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_vxsmoke,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_vxsmoke_red,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_muzzleflash,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_inferno,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_2dshockwave,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_vxrocketsmoke,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_trailbleed,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_bleedspike,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_flame,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_bubble2,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_bloodcloud,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_chunkdir,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_smallspark,
	sprite_vertpool_qmb_particles_environment, // SPRITE3D_PARTICLES_NEW_p_slimeglow,
	sprite_vertpool_qmb_particles_environment, // SPRITE3D_PARTICLES_NEW_p_slimebubble,
	sprite_vertpool_qmb_particles_environment, // SPRITE3D_PARTICLES_NEW_p_blacklavasmoke,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_PARTICLES_NEW_p_entitytrail,
	sprite_vertpool_qmb_particles_environment, // SPRITE3D_PARTICLES_NEW_p_flametorch,
	sprite_vertpool_game_annotations, // SPRITE3D_FLASHBLEND_LIGHTS,  (put it here as it's important to gameplay)
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_CORONATEX_STANDARD,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_CORONATEX_GUNFLASH,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_CORONATEX_EXPLOSIONFLASH1,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_CORONATEX_EXPLOSIONFLASH2,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_CORONATEX_EXPLOSIONFLASH3,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_CORONATEX_EXPLOSIONFLASH4,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_CORONATEX_EXPLOSIONFLASH5,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_CORONATEX_EXPLOSIONFLASH6,
	sprite_vertpool_qmb_particles_normal, // SPRITE3D_CORONATEX_EXPLOSIONFLASH7,
	sprite_vertpool_game_annotations, // SPRITE3D_CHATICON_AFK_CHAT,
	sprite_vertpool_game_annotations, // SPRITE3D_CHATICON_CHAT,
	sprite_vertpool_game_annotations, // SPRITE3D_CHATICON_AFK
};

#ifdef C_ASSERT
C_ASSERT(sizeof(batch_to_vertpool) / sizeof(batch_to_vertpool[0]) == MAX_SPRITE3D_BATCHES);
#endif

static gl_sprite3d_batch_t* BatchForType(sprite3d_batch_id type, qbool allocate)
{
	unsigned int index = batchMapping[type];

	if (index == 0) {
		if (!allocate || batchCount >= sizeof(batches) / sizeof(batches[0])) {
			return NULL;
		}
		batchMapping[type] = index = ++batchCount;
	}

	return &batches[index - 1];
}

void R_Sprite3DInitialiseBatch(sprite3d_batch_id type, r_state_id state, texture_ref texture, int index, r_primitive_id primitive_type)
{
	gl_sprite3d_batch_t* batch = BatchForType(type, true);

	assert(state != r_state_null);

	batch->rendering_state = state;
	batch->texture = texture;
	batch->count = 0;
	batch->primitive_id = primitive_type;
	batch->allSameNumber = true;
	batch->texture_index = index;
	batch->name = batch_type_names[type];
	batch->id = type;
}

r_sprite3d_vert_t* R_Sprite3DAddEntrySpecific(sprite3d_batch_id type, int verts_required, texture_ref texture, int texture_index)
{
	gl_sprite3d_batch_t* batch = BatchForType(type, false);
	int start = vertexCount;
	int sprite_count = sprites_in_batch[batch_to_vertpool[batch->id]];

	if (!batch || sprite_count >= MAX_3DSPRITES_PER_BATCH || (sprite_count + 1) * verts_required >= MAX_VERTS_PER_BATCH) {
		return NULL;
	}
	if (start + verts_required >= MAX_VERTS_PER_SCENE) {
		return NULL;
	}
	batch->firstVertices[batch->count] = start;
	batch->glFirstVertices[batch->count] = start + buffers.BufferOffset(r_buffer_sprite_vertex_data) / sizeof(verts[0]);
	batch->numVertices[batch->count] = verts_required;
	batch->textures[batch->count] = texture;
	batch->textureIndexes[batch->count] = texture_index;

	if (batch->count) {
		batch->allSameNumber &= verts_required == batch->numVertices[0];
	}
	sprites_in_batch[batch_to_vertpool[batch->id]] += 1;
	++batch->count;
	vertexCount += verts_required;
	return &verts[start];
}

r_sprite3d_vert_t* R_Sprite3DAddEntryFixed(sprite3d_batch_id type, int verts_required)
{
	return R_Sprite3DAddEntrySpecific(type, verts_required, null_texture_reference, 0);
}

r_sprite3d_vert_t* R_Sprite3DAddEntry(sprite3d_batch_id type, int verts_required)
{
	return R_Sprite3DAddEntrySpecific(type, verts_required, null_texture_reference, 0);
}

void R_Sprite3DSetVert(r_sprite3d_vert_t* vert, float x, float y, float z, float s, float t, byte color[4], int texture_index)
{
	extern int particletexture_array_index;
	extern float particletexture_array_xpos_tr;
	extern float particletexture_array_ypos_tr;

	vert->color[0] = color[0];
	vert->color[1] = color[1];
	vert->color[2] = color[2];
	vert->color[3] = color[3];
	VectorSet(vert->position, x, y, z);
	if (texture_index < 0) {
		vert->tex[0] = particletexture_array_xpos_tr;
		vert->tex[1] = particletexture_array_ypos_tr;
		vert->tex[2] = particletexture_array_index;
	}
	else {
		vert->tex[0] = s;
		vert->tex[1] = t;
		vert->tex[2] = texture_index;
	}
}

void R_Sprite3DCreateVBO(void)
{
	if (!R_BufferReferenceIsValid(r_buffer_sprite_vertex_data)) {
		buffers.Create(r_buffer_sprite_vertex_data, buffertype_vertex, "sprite3d-vbo", sizeof(verts), verts, bufferusage_once_per_frame);
	}
}

void R_Sprite3DCreateIndexBuffer(void)
{
	if (!R_BufferReferenceIsValid(r_buffer_sprite_index_data)) {
		// Meag: *3 is for case of primitive restart not being supported
		int i, j;
		int pos = 0;
		int vbo_pos;

		indexes_start_quads = pos;
		for (i = 0, vbo_pos = 0; i < INDEXES_MAX_QUADS; ++i) {
			if (i) {
				if (GL_Supported(R_SUPPORT_PRIMITIVERESTART)) {
					indexData[pos++] = ~(unsigned int)0;
				}
				else {
					indexData[pos++] = vbo_pos - 1;
					indexData[pos++] = vbo_pos;
				}
			}
			for (j = 0; j < 4; ++j) {
				indexData[pos++] = vbo_pos++;
			}
		}

		// These are currently only supported/used if primitive restart supported, as they're rendered as triangle fans
		indexes_start_flashblend = pos;
		for (i = 0, vbo_pos = 0; i < INDEXES_MAX_FLASHBLEND; ++i) {
			if (i) {
				if (GL_Supported(R_SUPPORT_PRIMITIVERESTART)) {
					indexData[pos++] = ~(unsigned int)0;
				}
				else {
					indexData[pos++] = vbo_pos - 1;
					indexData[pos++] = vbo_pos;
				}
			}
			for (j = 0; j < 18; ++j) {
				indexData[pos++] = vbo_pos++;
			}
		}

		// These are currently only supported/used if primitive restart supported, as they're rendered as triangle fans
		indexes_start_sparks = pos;
		for (i = 0, vbo_pos = 0; i < INDEXES_MAX_SPARKS; ++i) {
			if (i) {
				if (GL_Supported(R_SUPPORT_PRIMITIVERESTART)) {
					indexData[pos++] = ~(unsigned int)0;
				}
				else {
					indexData[pos++] = vbo_pos - 1;
					indexData[pos++] = vbo_pos - 1;
					indexData[pos++] = vbo_pos;
				}
			}
			for (j = 0; j < 9; ++j) {
				indexData[pos++] = vbo_pos++;
			}
		}

		buffers.Create(r_buffer_sprite_index_data, buffertype_index, "3dsprite-indexes", sizeof(indexData), indexData, bufferusage_constant_data);
	}
}

void R_Sprite3DRender(r_sprite3d_vert_t* vert, vec3_t origin, vec3_t up, vec3_t right, float scale_up, float scale_down, float scale_left, float scale_right, float s, float t, int index)
{
	static byte color_white[4] = { 255, 255, 255, 255 };
	vec3_t points[4];


	VectorMA(origin, scale_up, up, points[0]);
	VectorMA(points[0], scale_left, right, points[0]);
	VectorMA(origin, scale_down, up, points[1]);
	VectorMA(points[1], scale_left, right, points[1]);
	VectorMA(origin, scale_up, up, points[2]);
	VectorMA(points[2], scale_right, right, points[2]);
	VectorMA(origin, scale_down, up, points[3]);
	VectorMA(points[3], scale_right, right, points[3]);

	R_Sprite3DSetVert(vert++, points[0][0], points[0][1], points[0][2], 0, 0, color_white, index);
	R_Sprite3DSetVert(vert++, points[1][0], points[1][1], points[1][2], 0, t, color_white, index);
	R_Sprite3DSetVert(vert++, points[2][0], points[2][1], points[2][2], s, 0, color_white, index);
	R_Sprite3DSetVert(vert++, points[3][0], points[3][1], points[3][2], s, t, color_white, index);
}

void R_Sprite3DClearBatches(void)
{
	batchCount = vertexCount = 0;
	memset(batchMapping, 0, sizeof(batchMapping));
	memset(sprites_in_batch, 0, sizeof(sprites_in_batch));
}
