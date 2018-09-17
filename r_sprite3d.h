
#ifndef EZQUAKE_R_SPRITE3D_HEADER
#define EZQUAKE_R_SPRITE3D_HEADER

#include "r_state.h"

// Billboards
typedef enum {
	SPRITE3D_ENTITIES,
	SPRITE3D_PARTICLES_CLASSIC,
	SPRITE3D_PARTICLES_NEW_p_spark,
	SPRITE3D_PARTICLES_NEW_p_smoke,
	SPRITE3D_PARTICLES_NEW_p_fire,
	SPRITE3D_PARTICLES_NEW_p_bubble,
	SPRITE3D_PARTICLES_NEW_p_lavasplash,
	SPRITE3D_PARTICLES_NEW_p_gunblast,
	SPRITE3D_PARTICLES_NEW_p_chunk,
	SPRITE3D_PARTICLES_NEW_p_shockwave,
	SPRITE3D_PARTICLES_NEW_p_inferno_flame,
	SPRITE3D_PARTICLES_NEW_p_inferno_trail,
	SPRITE3D_PARTICLES_NEW_p_sparkray,
	SPRITE3D_PARTICLES_NEW_p_staticbubble,
	SPRITE3D_PARTICLES_NEW_p_trailpart,
	SPRITE3D_PARTICLES_NEW_p_dpsmoke,
	SPRITE3D_PARTICLES_NEW_p_dpfire,
	SPRITE3D_PARTICLES_NEW_p_teleflare,
	SPRITE3D_PARTICLES_NEW_p_blood1,
	SPRITE3D_PARTICLES_NEW_p_blood2,
	SPRITE3D_PARTICLES_NEW_p_blood3,
	//VULT PARTICLES
	SPRITE3D_PARTICLES_NEW_p_rain,
	SPRITE3D_PARTICLES_NEW_p_alphatrail,
	SPRITE3D_PARTICLES_NEW_p_railtrail,
	SPRITE3D_PARTICLES_NEW_p_streak,
	SPRITE3D_PARTICLES_NEW_p_streaktrail,
	SPRITE3D_PARTICLES_NEW_p_streakwave,
	SPRITE3D_PARTICLES_NEW_p_lightningbeam,
	SPRITE3D_PARTICLES_NEW_p_vxblood,
	SPRITE3D_PARTICLES_NEW_p_lavatrail,
	SPRITE3D_PARTICLES_NEW_p_vxsmoke,
	SPRITE3D_PARTICLES_NEW_p_vxsmoke_red,
	SPRITE3D_PARTICLES_NEW_p_muzzleflash,
	SPRITE3D_PARTICLES_NEW_p_inferno,
	SPRITE3D_PARTICLES_NEW_p_2dshockwave,
	SPRITE3D_PARTICLES_NEW_p_vxrocketsmoke,
	SPRITE3D_PARTICLES_NEW_p_trailbleed,
	SPRITE3D_PARTICLES_NEW_p_bleedspike,
	SPRITE3D_PARTICLES_NEW_p_flame,
	SPRITE3D_PARTICLES_NEW_p_bubble2,
	SPRITE3D_PARTICLES_NEW_p_bloodcloud,
	SPRITE3D_PARTICLES_NEW_p_chunkdir,
	SPRITE3D_PARTICLES_NEW_p_smallspark,
	SPRITE3D_PARTICLES_NEW_p_slimeglow,
	SPRITE3D_PARTICLES_NEW_p_slimebubble,
	SPRITE3D_PARTICLES_NEW_p_blacklavasmoke,
	SPRITE3D_PARTICLES_NEW_p_entitytrail,
	SPRITE3D_FLASHBLEND_LIGHTS,
	SPRITE3D_CORONATEX_STANDARD,
	SPRITE3D_CORONATEX_GUNFLASH,
	SPRITE3D_CORONATEX_EXPLOSIONFLASH1,
	SPRITE3D_CORONATEX_EXPLOSIONFLASH2,
	SPRITE3D_CORONATEX_EXPLOSIONFLASH3,
	SPRITE3D_CORONATEX_EXPLOSIONFLASH4,
	SPRITE3D_CORONATEX_EXPLOSIONFLASH5,
	SPRITE3D_CORONATEX_EXPLOSIONFLASH6,
	SPRITE3D_CORONATEX_EXPLOSIONFLASH7,
	SPRITE3D_CHATICON_AFK_CHAT,
	SPRITE3D_CHATICON_CHAT,
	SPRITE3D_CHATICON_AFK,

	MAX_SPRITE3D_BATCHES
} sprite3d_batch_id;

// Meag: use this as sanity check that any new particles have had batch created
#define SPRITE3D_PARTICLES_NEW_LIMIT (SPRITE3D_FLASHBLEND_LIGHTS)

typedef struct r_sprite3d_vert_s {
	float position[3];
	float tex[3];
	byte color[4];
} r_sprite3d_vert_t;

typedef enum {
	r_primitive_triangle_strip,
	r_primitive_triangle_fan,
	r_primitive_triangles,

	r_primitive_count
} r_primitive_id;

void R_Sprite3DInitialiseBatch(sprite3d_batch_id type, r_state_id textured_state, r_state_id untextured_state, texture_ref texture, int index, r_primitive_id primitive_type);
r_sprite3d_vert_t* R_Sprite3DAddEntry(sprite3d_batch_id type, int verts_required);
r_sprite3d_vert_t* R_Sprite3DAddEntrySpecific(sprite3d_batch_id type, int verts_required, texture_ref texture, int index);
void R_Sprite3DSetVert(r_sprite3d_vert_t* vert, float x, float y, float z, float s, float t, byte color[4], int texture_index);
void R_Sprite3DRender(r_sprite3d_vert_t* vert, vec3_t origin, vec3_t up, vec3_t right, float scale_up, float scale_down, float scale_left, float scale_right, float s, float t, int index);

// Internal for renderers
void R_Sprite3DCreateIndexBuffer(void);
void R_Sprite3DCreateVBO(void);

#endif // EZQUAKE_R_SPRITE3D_HEADER
