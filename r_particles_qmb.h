
#ifndef EZQUAKE_GL_RPART_HEADER
#define EZQUAKE_GL_RPART_HEADER

#include "r_sprite3d.h"
#include "r_state.h"

#define ABSOLUTE_MIN_PARTICLES				256
#define ABSOLUTE_MAX_PARTICLES				32768

//Better rand nums
#define lhrandom(MIN,MAX) ((rand() & 32767) * (((MAX)-(MIN)) * (1.0f / 32767.0f)) + (MIN))

typedef byte col_t[4];

typedef enum {
	p_spark, p_smoke, p_fire, p_bubble, p_lavasplash, p_gunblast, p_chunk, p_shockwave,
	p_inferno_flame, p_inferno_trail, p_sparkray, p_staticbubble, p_trailpart,
	p_dpsmoke, p_dpfire, p_teleflare, p_blood1, p_blood2, p_blood3,
	//VULT PARTICLES
	p_rain,
	p_alphatrail,
	p_railtrail,
	p_streak,
	p_streaktrail,
	p_streakwave,
	p_lightningbeam,
	p_vxblood,
	p_lavatrail,
	p_vxsmoke,
	p_vxsmoke_red,
	p_muzzleflash,
	p_inferno, //VULT - NOT TO BE CONFUSED WITH THE 0.36 FIREBALL
	p_2dshockwave,
	p_vxrocketsmoke,
	p_trailbleed,
	p_bleedspike,
	p_flame,
	p_bubble2,
	p_bloodcloud,
	p_chunkdir,
	p_smallspark,
	//[HyperNewbie] - particles!
	p_slimeglow, //slime glow
	p_slimebubble, //slime yellowish growing popping bubble
	p_blacklavasmoke,
	p_nailtrail,
	p_flametorch,
	num_particletypes,
} part_type_t;

typedef enum {
	pm_static, pm_normal, pm_bounce, pm_die, pm_nophysics, pm_float,
	//VULT PARTICLES
	pm_rain,
	pm_streak,
	pm_streakwave,
	pm_trail
} part_move_t;

typedef enum {
	ptex_none, ptex_smoke, ptex_bubble, ptex_generic, ptex_dpsmoke, ptex_lava,
	ptex_blueflare, ptex_blood1, ptex_blood2, ptex_blood3,
	//VULT PARTICLES
	ptex_shockwave,
	ptex_lightning,
	ptex_spark,
	num_particletextures,
} part_tex_t;

typedef enum {
	pd_spark, pd_sparkray, pd_billboard, pd_billboard_vel,
	//VULT PARTICLES
	pd_beam,
	pd_hide,
	pd_normal,
	pd_dynamictrail,
	pd_torch
} part_draw_t;

typedef enum {
	BLEND_GL_SRC_ALPHA_GL_ONE_MINUS_SRC_ALPHA,
	BLEND_GL_SRC_ALPHA_GL_ONE,
	BLEND_GL_ONE_GL_ONE,
	BLEND_GL_ZERO_GL_ONE_MINUS_SRC_COLOR_CONSTANT,   // ONE_MINUS_SRC_COLOR but r=g=b, so reduce by constant
	BLEND_GL_ZERO_GL_ONE_MINUS_SRC_COLOR,            // ONE_MINUS_SRC_COLOR but different levels for each

	NUMBER_OF_BLEND_TYPES
} part_blend_id;

typedef struct particle_s {
	struct particle_s *next;
	vec3_t      org, endorg;
	col_t       color;
	float       growth;
	vec3_t      vel;
	float       rotangle;
	float       rotspeed;
	float       size;
	float       start;
	float       die;
	byte        hit;
	byte        texindex;
	byte        bounces;
	byte        initial_alpha;

	int         cached_contents;
	vec3_t      cached_movement;
	float       cached_distance;

	int         entity_ref;
	int         entity_trailnumber;
} particle_t;

typedef struct particle_type_s {
	particle_t	  *start;
	part_type_t	  id;
	part_draw_t	  drawtype;
	part_blend_id blendtype;
	part_tex_t	  texture;
	float		  startalpha;
	float		  grav;
	float		  accel;
	part_move_t	  move;
	float		  custom;

	int           verts_per_primitive;
	sprite3d_batch_id billboard_type;

	int           particles;
	r_state_id    state;
} particle_type_t;

#define	MAX_PTEX_COMPONENTS		8
typedef struct particle_texture_s {
	texture_ref texnum;
	texture_ref tex_array;
	int         tex_index;
	int			components;
	float		coords[MAX_PTEX_COMPONENTS][4];
	float		originalCoords[MAX_PTEX_COMPONENTS][4];
} particle_texture_t;

typedef void(*func_color_transform_t)(col_t input, col_t output);
extern vec3_t zerodir;
extern vec3_t trail_stop;
extern particle_type_t particle_types[num_particletypes];
extern int particle_type_index[num_particletypes];
extern particle_t* free_particles;
extern particle_texture_t particle_textures[num_particletextures];

extern cvar_t amf_part_fulldetail;

void QMB_ProcessParticle(particle_type_t* pt, particle_t* p);
qbool TraceLineN(vec3_t start, vec3_t end, vec3_t impact, vec3_t normal);
extern particle_t* free_particles;
void ParticleStats(int change);

#define R_SIMPLETRAIL_MAXLENGTH    100
#define R_SIMPLETRAIL_NEAR_ALPHA    75

#endif // def(EZQUAKE_GL_RPART_HEADER)
