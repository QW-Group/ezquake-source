
#ifndef EZQUAKE_GL_BILLBOARDS_HEADER
#define EZQUAKE_GL_BILLBOARDS_HEADER

// Billboards
typedef enum {
	BILLBOARD_ENTITIES,
	BILLBOARD_PARTICLES_CLASSIC,
	BILLBOARD_PARTICLES_NEW_p_spark,
	BILLBOARD_PARTICLES_NEW_p_smoke,
	BILLBOARD_PARTICLES_NEW_p_fire,
	BILLBOARD_PARTICLES_NEW_p_bubble,
	BILLBOARD_PARTICLES_NEW_p_lavasplash,
	BILLBOARD_PARTICLES_NEW_p_gunblast,
	BILLBOARD_PARTICLES_NEW_p_chunk,
	BILLBOARD_PARTICLES_NEW_p_shockwave,
	BILLBOARD_PARTICLES_NEW_p_inferno_flame,
	BILLBOARD_PARTICLES_NEW_p_inferno_trail,
	BILLBOARD_PARTICLES_NEW_p_sparkray,
	BILLBOARD_PARTICLES_NEW_p_staticbubble,
	BILLBOARD_PARTICLES_NEW_p_trailpart,
	BILLBOARD_PARTICLES_NEW_p_dpsmoke,
	BILLBOARD_PARTICLES_NEW_p_dpfire,
	BILLBOARD_PARTICLES_NEW_p_teleflare,
	BILLBOARD_PARTICLES_NEW_p_blood1,
	BILLBOARD_PARTICLES_NEW_p_blood2,
	BILLBOARD_PARTICLES_NEW_p_blood3,
	//VULT PARTICLES
	BILLBOARD_PARTICLES_NEW_p_rain,
	BILLBOARD_PARTICLES_NEW_p_alphatrail,
	BILLBOARD_PARTICLES_NEW_p_railtrail,
	BILLBOARD_PARTICLES_NEW_p_streak,
	BILLBOARD_PARTICLES_NEW_p_streaktrail,
	BILLBOARD_PARTICLES_NEW_p_streakwave,
	BILLBOARD_PARTICLES_NEW_p_lightningbeam,
	BILLBOARD_PARTICLES_NEW_p_vxblood,
	BILLBOARD_PARTICLES_NEW_p_lavatrail,
	BILLBOARD_PARTICLES_NEW_p_vxsmoke,
	BILLBOARD_PARTICLES_NEW_p_vxsmoke_red,
	BILLBOARD_PARTICLES_NEW_p_muzzleflash,
	BILLBOARD_PARTICLES_NEW_p_inferno,
	BILLBOARD_PARTICLES_NEW_p_2dshockwave,
	BILLBOARD_PARTICLES_NEW_p_vxrocketsmoke,
	BILLBOARD_PARTICLES_NEW_p_trailbleed,
	BILLBOARD_PARTICLES_NEW_p_bleedspike,
	BILLBOARD_PARTICLES_NEW_p_flame,
	BILLBOARD_PARTICLES_NEW_p_bubble2,
	BILLBOARD_PARTICLES_NEW_p_bloodcloud,
	BILLBOARD_PARTICLES_NEW_p_chunkdir,
	BILLBOARD_PARTICLES_NEW_p_smallspark,
	BILLBOARD_PARTICLES_NEW_p_slimeglow,
	BILLBOARD_PARTICLES_NEW_p_slimebubble,
	BILLBOARD_PARTICLES_NEW_p_blacklavasmoke,
	BILLBOARD_FLASHBLEND_LIGHTS,
	BILLBOARD_CORONATEX_STANDARD,
	BILLBOARD_CORONATEX_GUNFLASH,
	BILLBOARD_CORONATEX_EXPLOSIONFLASH1,
	BILLBOARD_CORONATEX_EXPLOSIONFLASH2,
	BILLBOARD_CORONATEX_EXPLOSIONFLASH3,
	BILLBOARD_CORONATEX_EXPLOSIONFLASH4,
	BILLBOARD_CORONATEX_EXPLOSIONFLASH5,
	BILLBOARD_CORONATEX_EXPLOSIONFLASH6,
	BILLBOARD_CORONATEX_EXPLOSIONFLASH7,
	BILLBOARD_CHATICON_AFK_CHAT,
	BILLBOARD_CHATICON_CHAT,
	BILLBOARD_CHATICON_AFK,

	MAX_BILLBOARD_BATCHES
} billboard_batch_id;

typedef struct gl_billboard_vert_s {
	float position[3];
	float tex[3];
	GLubyte color[4];
} gl_billboard_vert_t;

void GL_BillboardInitialiseBatch(billboard_batch_id type, GLenum blendSource, GLenum blendDestination, texture_ref texture, int index, GLenum primitive_type, qbool depthTest, qbool depthMask);
gl_billboard_vert_t* GL_BillboardAddEntry(billboard_batch_id type, int verts_required);
gl_billboard_vert_t* GL_BillboardAddEntrySpecific(billboard_batch_id type, int verts_required, texture_ref texture, int index);
void GL_BillboardSetVert(gl_billboard_vert_t* vert, float x, float y, float z, float s, float t, GLubyte color[4], int texture_index);
void GL_DrawBillboards(void);
void GLM_SpriteToBillboard(gl_billboard_vert_t* vert, vec3_t origin, vec3_t up, vec3_t right, float scale, float s, float t, int index);

#endif // #ifndef EZQUAKE_GL_BILLBOARDS_HEADER
