/*
Copyright (C) 2002-2003, Dr Labman, A. Nourai

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
#include "vx_stuff.h"
#include "pmove.h"
#include "glm_texture_arrays.h"
#include "r_particles_qmb.h"
#include "qmb_particles.h"
#include "r_texture.h"
#include "r_matrix.h"
#include "r_state.h"

//VULT
static float varray_vertex[16];

//VULT PARTICLES
void RainSplash(vec3_t org);
void ParticleStats (int change);
void VX_ParticleTrail (vec3_t start, vec3_t end, float size, float time, col_t color);
static void R_PreCalcBeamVerts(vec3_t org1, vec3_t org2, vec3_t right1, vec3_t right2);
static void R_CalcBeamVerts(float *vert, const vec3_t org1, const vec3_t org2, const vec3_t right1, const vec3_t right2, float width);

typedef struct part_blend_info_s {
	r_blendfunc_t func;

	func_color_transform_t color_transform;
} part_blend_info_t;

static void blend_premult_alpha(col_t input, col_t output)
{
	float alpha = input[3] / 255.0f;

	output[0] = input[0] * alpha;
	output[1] = input[1] * alpha;
	output[2] = input[2] * alpha;
	output[3] = input[3];
}

static void blend_additive(col_t input, col_t output)
{
	float alpha = input[3] / 255.0f;

	output[0] = input[0] * alpha;
	output[1] = input[1] * alpha;
	output[2] = input[2] * alpha;
	output[3] = 0;
}

static void blend_one_one(col_t input, col_t output)
{
	output[0] = input[0];
	output[1] = input[1];
	output[2] = input[2];
	output[3] = 0;
}

static void blend_color_constant(col_t input, col_t output)
{
	output[0] = output[1] = output[2] = 0;
	output[3] = input[3];
}

static part_blend_info_t blend_options[NUMBER_OF_BLEND_TYPES] = {
	// BLEND_GL_SRC_ALPHA_GL_ONE_MINUS_SRC_ALPHA
	{ r_blendfunc_premultiplied_alpha, blend_premult_alpha },
	// BLEND_GL_SRC_ALPHA_GL_ONE (additive)
	{ r_blendfunc_premultiplied_alpha, blend_additive },
	// BLEND_GL_ONE_GL_ONE, (meag: was blend_additive... telesplash only)
	{ r_blendfunc_premultiplied_alpha, blend_one_one },
	// BLEND_GL_ZERO_GL_ONE_MINUS_SRC_COLOR_CONSTANT
	{ r_blendfunc_premultiplied_alpha, blend_color_constant },
	// BLEND_GL_ZERO_GL_ONE_MINUS_SRC_COLOR, (this is now for varying color only, can't convert?)
	{ r_blendfunc_src_zero_dest_one_minus_src_color, blend_premult_alpha }
};

#define MAX_BEAM_TRAIL 10
#define TEXTURE_DETAILS(x) (R_UseModernOpenGL() ? x->tex_array : x->texnum),(x->tex_index)

static float sint[7] = {0.000000, 0.781832, 0.974928, 0.433884, -0.433884, -0.974928, -0.781832};
static float cost[7] = {1.000000, 0.623490, -0.222521, -0.900969, -0.900969, -0.222521, 0.623490};

static particle_t* particles;
particle_t* free_particles;
particle_texture_t particle_textures[num_particletextures];
particle_type_t particle_types[num_particletypes];
int particle_type_index[num_particletypes];

static int r_numparticles;		
static int particle_count = 0;
static float particle_time;		

vec3_t zerodir = { 22, 22, 22 };
vec3_t trail_stop;

qbool qmb_initialized = false;

static cvar_t gl_clipparticles = {"gl_clipparticles", "1"};
static cvar_t gl_part_cache = { "gl_part_cache", "1", CVAR_LATCH };
cvar_t gl_bounceparticles = {"gl_bounceparticles", "1"};
cvar_t amf_part_fulldetail = { "gl_particle_fulldetail", "0", CVAR_LATCH };

static int ParticleContents(particle_t* p, vec3_t movement)
{
	if (gl_part_cache.integer) {
		float moved;

		VectorAdd(p->cached_movement, movement, p->cached_movement);
		moved = p->cached_movement[0] * p->cached_movement[0] + p->cached_movement[1] * p->cached_movement[1] + p->cached_movement[2] * p->cached_movement[2];

		if (p->cached_distance <= moved) {
			p->cached_contents = CM_CachedHullPointContents(&cl.clipmodels[1]->hulls[0], 0, p->org, &p->cached_distance);

			VectorClear(p->cached_movement);
			p->cached_distance *= p->cached_distance;
		}

		return p->cached_contents;
	}
	else {
		return CM_HullPointContents(&cl.clipmodels[1]->hulls[0], 0, p->org);
	}
}

qbool TraceLineN (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal)
{
	trace_t trace = PM_TraceLine (start, end);

	VectorCopy (trace.endpos, impact);

	if (normal)
		VectorCopy (trace.plane.normal, normal);

	if (!trace.allsolid)
		return false;

	if (trace.startsolid)
		return false;

	return true;
}

#define ADD_PARTICLE_TEXTURE(_ptex, _texnum, _texindex, _components, _s1, _t1, _s2, _t2)	\
do {																						\
	particle_textures[_ptex].texnum = _texnum;												\
	particle_textures[_ptex].components = _components;										\
	particle_textures[_ptex].coords[_texindex][0] = (_s1 + 1) / 256.0;						\
	particle_textures[_ptex].coords[_texindex][1] = (_t1 + 1) / 256.0;						\
	particle_textures[_ptex].coords[_texindex][2] = (_s2 - 1) / 256.0;						\
	particle_textures[_ptex].coords[_texindex][3] = (_t2 - 1) / 256.0;						\
	memcpy(particle_textures[_ptex].originalCoords, particle_textures[_ptex].coords, sizeof(particle_textures[_ptex].originalCoords)); \
} while(0);

static void QMB_AddParticleType(part_type_t id, part_draw_t drawtype, int blendtype, part_tex_t texture_id, float startalpha, float grav, float accel, part_move_t move, float custom, int verts_per_primitive, int* count, const char* name)
{
	particle_type_t* type = &particle_types[*count];

	if (blend_options[blendtype].func == r_blendfunc_src_zero_dest_one_minus_src_color) {
		if (R_TextureReferenceIsValid(particle_textures[texture_id].texnum)) {
			type->state = r_state_particles_qmb_textured_blood;
		}
		else {
			Sys_Error("No rendering state available for QMB particle type %s", name);
		}
	}
	else if (blend_options[blendtype].func == r_blendfunc_premultiplied_alpha) {
		if (R_TextureReferenceIsValid(particle_textures[texture_id].texnum)) {
			type->state = r_state_particles_qmb_textured;
		}
		else {
			type->state = r_state_particles_qmb_untextured;
		}
	}
	else {
		Sys_Error("No rendering state available for QMB particle type %s", name);
	}
	type->blendtype = blendtype;
	type->id = id;
	type->drawtype = drawtype;
	type->texture = texture_id;
	type->startalpha = startalpha;
	type->grav = 9.8 * grav;
	type->accel = accel;
	type->move = move;
	type->custom = custom;
	type->verts_per_primitive = verts_per_primitive;
	type->billboard_type = SPRITE3D_PARTICLES_NEW_p_spark + id;
	particle_type_index[id] = *count;
	*count = *count + 1;
}

static int QMB_CompareParticleType(const void* lhs_, const void* rhs_)
{
	const particle_type_t* lhs = (const particle_type_t*)lhs_;
	const particle_type_t* rhs = (const particle_type_t*)rhs_;
	int comparison;

	comparison = lhs->blendtype - rhs->blendtype;
	comparison = comparison ? comparison : lhs->texture - rhs->texture;

	return comparison;
}

static void QMB_SortParticleTypes(void)
{
	int i;

	// To reduce state changes: Sort by blend mode, then texture
	qsort(particle_types, num_particletypes, sizeof(particle_types[0]), QMB_CompareParticleType);

	// Fix up references
	for (i = 0; i < num_particletypes; ++i) {
		particle_type_index[particle_types[i].id] = i;
	}
}

void QMB_AllocParticles(void)
{
	extern cvar_t r_particles_count;

	r_numparticles = bound(ABSOLUTE_MIN_PARTICLES, r_particles_count.integer, ABSOLUTE_MAX_PARTICLES);

	if (particles || r_numparticles < 1) {
		// seems QMB_AllocParticles() called from wrong place
		Sys_Error("QMB_AllocParticles: internal error");
	}

	// can't alloc on Hunk, using native memory
	particles = (particle_t *)Q_malloc(r_numparticles * sizeof(particle_t));
}

static void QMB_PreMultiplyAlpha(byte* data, int width, int height)
{
	int x, y;

	// Adjust particle font as we simplified the blending rules...
	for (x = 0; x < width; ++x) {
		for (y = 0; y < height; ++y) {
			byte* base = data + (x + y * width) * 4;

			// Pre-multiply alpha
			base[0] = (byte)(((int)base[0] * (int)base[3]) / 255.0);
			base[1] = (byte)(((int)base[1] * (int)base[3]) / 255.0);
			base[2] = (byte)(((int)base[2] * (int)base[3]) / 255.0);
		}
	}
}

static void QMB_LoadTextureSubImage(part_tex_t tex, const char* id, const byte* pixels, byte* temp_buffer, int full_width, int full_height, int texIndex, int components, int sub_x, int sub_y, int sub_x2, int sub_y2)
{
	const int mode = TEX_ALPHA | TEX_COMPLAIN | TEX_NOSCALE | TEX_MIPMAP;
	texture_ref tex_ref;
	int y;
	int width = sub_x2 - sub_x;
	int height = sub_y2 - sub_y;

	width = (width * full_width) / 256;
	height = (height * full_height) / 256;
	sub_x = (sub_x * full_width) / 256;
	sub_y = (sub_y * full_height) / 256;

	for (y = 0; y < height; ++y) {
		memcpy(temp_buffer + y * width * 4, pixels + ((sub_y + y) * full_width + sub_x) * 4, width * 4);
	}

	QMB_PreMultiplyAlpha(temp_buffer, width, height);

	tex_ref = R_LoadTexture(id, width, height, temp_buffer, mode, 4);

	ADD_PARTICLE_TEXTURE(tex, tex_ref, texIndex, components, 0, 0, 256, 256);
}

static texture_ref QMB_LoadTextureImage(const char* path)
{
	const int mode = TEX_ALPHA | TEX_COMPLAIN | TEX_NOSCALE | TEX_MIPMAP | TEX_PREMUL_ALPHA;

	return R_LoadTextureImage(path, NULL, 0, 0, mode);
}

void QMB_InitParticles(void)
{
	int	i, count = 0;
	texture_ref shockwave_texture, lightning_texture, spark_texture;

	Cvar_Register(&amf_part_fulldetail);
	Cvar_Register(&gl_part_cache);
	if (!host_initialized && COM_CheckParm(cmdline_param_client_detailtrails)) {
		Cvar_LatchedSetValue(&amf_part_fulldetail, 1);
	}

	if (!qmb_initialized) {
		if (!host_initialized) {
			Cvar_SetCurrentGroup(CVAR_GROUP_PARTICLES);
			Cvar_Register(&gl_clipparticles);
			Cvar_Register(&gl_bounceparticles);
			Cvar_ResetCurrentGroup();
		}

		Q_free(particles); // yeah, shit happens, work around
		QMB_AllocParticles();
	}
	else {
		QMB_ClearParticles (); // also re-allocc particles
		qmb_initialized = false; // so QMB particle system will be turned off if we fail to load some texture
	}

	ADD_PARTICLE_TEXTURE(ptex_none, null_texture_reference, 0, 1, 0, 0, 0, 0);

	// Move particle fonts off atlas and into their own textures...
	{
		int real_width, real_height;
		byte* original;
		byte* temp_buffer;

		original = R_LoadImagePixels("textures/particles/particlefont", 0, 0, TEX_ALPHA | TEX_COMPLAIN | TEX_NOSCALE | TEX_MIPMAP, &real_width, &real_height);
		if (!original) {
			return;
		}

		temp_buffer = Q_malloc(real_width * real_height * 4);
		QMB_LoadTextureSubImage(ptex_blood1, "qmb:blood1", original, temp_buffer, real_width, real_height, 0, 1, 0, 0, 64, 64);
		QMB_LoadTextureSubImage(ptex_blood2, "qmb:blood2", original, temp_buffer, real_width, real_height, 0, 1, 64, 0, 128, 64);
		QMB_LoadTextureSubImage(ptex_lava, "qmb:lava", original, temp_buffer, real_width, real_height, 0, 1, 128, 0, 192, 64);
		QMB_LoadTextureSubImage(ptex_blueflare, "qmb:blueflare", original, temp_buffer, real_width, real_height, 0, 1, 192, 0, 256, 64);
		QMB_LoadTextureSubImage(ptex_generic, "qmb:generic", original, temp_buffer, real_width, real_height, 0, 1, 0, 96, 96, 192);
		QMB_LoadTextureSubImage(ptex_smoke, "qmb:smoke", original, temp_buffer, real_width, real_height, 0, 1, 96, 96, 192, 192);
		QMB_LoadTextureSubImage(ptex_blood3, "qmb:blood3", original, temp_buffer, real_width, real_height, 0, 1, 192, 96, 256, 160);
		QMB_LoadTextureSubImage(ptex_bubble, "qmb:bubble", original, temp_buffer, real_width, real_height, 0, 1, 192, 160, 224, 192);
		for (i = 0; i < 8; i++) {
			QMB_LoadTextureSubImage(ptex_dpsmoke, va("qmb:smoke%d", i), original, temp_buffer, real_width, real_height, i, 8, i * 32, 64, (i + 1) * 32, 96);
		}
		Q_free(temp_buffer);
		Q_free(original);
	}

	//VULT PARTICLES
	shockwave_texture = QMB_LoadTextureImage("textures/shockwavetex");
	if (!R_TextureReferenceIsValid(shockwave_texture)) {
		return;
	}
	lightning_texture = QMB_LoadTextureImage("textures/zing1");
	if (!R_TextureReferenceIsValid(lightning_texture)) {
		return;
	}
	spark_texture = QMB_LoadTextureImage("textures/sparktex");
	if (!R_TextureReferenceIsValid(spark_texture)) {
		return;
	}
	ADD_PARTICLE_TEXTURE(ptex_shockwave, shockwave_texture, 0, 1, 0, 0, 256, 256);
	ADD_PARTICLE_TEXTURE(ptex_lightning, lightning_texture, 0, 1, 0, 0, 256, 256);
	ADD_PARTICLE_TEXTURE(ptex_spark, spark_texture, 0, 1, 0, 0, 32, 32);

	QMB_AddParticleType(p_spark, pd_spark, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_none, 255, -32, 0, pm_bounce, 1.3, 9, &count, "part:spark");
	QMB_AddParticleType(p_sparkray, pd_sparkray, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_none, 255, -0, 0, pm_nophysics, 0, 9, &count, "part:sparkray");
	QMB_AddParticleType(p_gunblast, pd_spark, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_none, 255, -16, 0, pm_bounce, 1.3, 9, &count, "part:gunblast");

	QMB_AddParticleType(p_fire, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_generic, 204, 0, -2.95, pm_die, 0, 4, &count, "part:fire");
	QMB_AddParticleType(p_chunk, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_generic, 255, -16, 0, pm_bounce, 1.475, 4, &count, "part:chunk");
	QMB_AddParticleType(p_shockwave, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_generic, 255, 0, -4.85, pm_nophysics, 0, 4, &count, "part:shockwave");
	QMB_AddParticleType(p_inferno_flame, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_generic, 153, 0, 0, pm_static, 0, 4, &count, "part:inferno_flame");
	QMB_AddParticleType(p_inferno_trail, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_generic, 204, 0, 0, pm_die, 0, 4, &count, "part:inferno_trail");
	QMB_AddParticleType(p_trailpart, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_generic, 230, 0, 0, pm_static, 0, 4, &count, "part:trailpart");
	QMB_AddParticleType(p_smoke, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_smoke, 140, 3, 0, pm_normal, 0, 4, &count, "part:smoke");
	QMB_AddParticleType(p_dpfire, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_dpsmoke, 144, 0, 0, pm_die, 0, 4, &count, "part:dpfire");
	QMB_AddParticleType(p_dpsmoke, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_dpsmoke, 85, 3, 0, pm_die, 0, 4, &count, "part:dpsmoke");

	QMB_AddParticleType(p_teleflare, pd_billboard, BLEND_GL_ONE_GL_ONE, ptex_blueflare, 255, 0, 0, pm_die, 0, 4, &count, "part:teleflare");
	QMB_AddParticleType(p_blood1, pd_billboard, BLEND_GL_ZERO_GL_ONE_MINUS_SRC_COLOR, ptex_blood1, 255, -20, 0, pm_die, 0, 4, &count, "part:blood1");
	QMB_AddParticleType(p_blood2, pd_billboard_vel, BLEND_GL_ZERO_GL_ONE_MINUS_SRC_COLOR, ptex_blood2, 255, -25, 0, pm_die, 0.018, 4, &count, "part:blood2");

	QMB_AddParticleType(p_lavasplash, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE_MINUS_SRC_ALPHA, ptex_lava, 170, 0, 0, pm_nophysics, 0, 4, &count, "part:lavasplash");
	QMB_AddParticleType(p_blood3, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE_MINUS_SRC_ALPHA, ptex_blood3, 255, -20, 0, pm_normal, 0, 4, &count, "part:blood3");
	QMB_AddParticleType(p_bubble, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE_MINUS_SRC_ALPHA, ptex_bubble, 204, 8, 0, pm_float, 0, 4, &count, "part:bubble");
	QMB_AddParticleType(p_staticbubble, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE_MINUS_SRC_ALPHA, ptex_bubble, 204, 0, 0, pm_static, 0, 4, &count, "part:staticbubble");

	//VULT PARTICLES
	QMB_AddParticleType(p_rain, pd_hide, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_none, 100, 0, 0, pm_rain, 0, 4, &count, "part:rain");
	QMB_AddParticleType(p_alphatrail, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_generic, 100, 0, 0, pm_static, 0, 4, &count, "part:alphatrail");
	QMB_AddParticleType(p_railtrail, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_generic, 255, 0, 0, pm_die, 0, 4, &count, "part:railtrail");

	QMB_AddParticleType(p_vxblood, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE_MINUS_SRC_ALPHA, ptex_blood3, 255, -38, 0, pm_normal, 0, 4, &count, "part:vxblood"); //HyperNewbie - Blood does NOT glow like fairy light
	QMB_AddParticleType(p_streak, pd_hide, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_none, 255, -64, 0, pm_streak, 1.5, 4, &count, "part:streak"); //grav was -64
	QMB_AddParticleType(p_streakwave, pd_hide, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_none, 255, 0, 0, pm_streakwave, 0, 4, &count, "part:streakwave");
	QMB_AddParticleType(p_lavatrail, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_generic, 255, 3, 0, pm_normal, 0, 4, &count, "part:lavatrail");
	QMB_AddParticleType(p_vxsmoke, pd_billboard, BLEND_GL_ZERO_GL_ONE_MINUS_SRC_COLOR_CONSTANT, ptex_smoke, 140, 3, 0, pm_normal, 0, 4, &count, "part:vxsmoke");
	QMB_AddParticleType(p_vxsmoke_red, pd_billboard, BLEND_GL_ZERO_GL_ONE_MINUS_SRC_COLOR, ptex_smoke, 140, 3, 0, pm_normal, 0, 4, &count, "part:vxsmoke_red");
	QMB_AddParticleType(p_muzzleflash, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_generic, 128, 0, 0, pm_die, 0, 4, &count, "part:muzzleflash");
	QMB_AddParticleType(p_2dshockwave, pd_normal, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_shockwave, 255, 0, 0, pm_static, 0, 4, &count, "part:2dshockwave");
	QMB_AddParticleType(p_vxrocketsmoke, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_generic, 128, 0, 0, pm_normal, 0, 4, &count, "part:vxrocketsmoke");
	QMB_AddParticleType(p_flame, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_generic, 200, 10, 0, pm_die, 0, 4, &count, "part:flame");
	QMB_AddParticleType(p_trailbleed, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_generic, 200, 0, 0, pm_static, 0, 4, &count, "part:flamebleed");
	QMB_AddParticleType(p_bleedspike, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_generic, 200, 0, 0, pm_static, 0, 4, &count, "part:bleedspike");
	QMB_AddParticleType(p_lightningbeam, pd_beam, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_lightning, 128, 0, 0, pm_static, 0, 4, &count, "part:lgbeam");
	QMB_AddParticleType(p_bubble2, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE_MINUS_SRC_ALPHA, ptex_bubble, 204, 1, 0, pm_float, 0, 4, &count, "part:bubble2");
	QMB_AddParticleType(p_bloodcloud, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE_MINUS_SRC_ALPHA, ptex_generic, 255, -3, 0, pm_normal, 0, 4, &count, "part:bloodcloud");
	QMB_AddParticleType(p_chunkdir, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_generic, 255, -32, 0, pm_bounce, 1.475, 4, &count, "part:chunkdir");
	QMB_AddParticleType(p_smallspark, pd_beam, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_spark, 255, -64, 0, pm_bounce, 1.5, 4, &count, "part:smallspark"); //grav was -64

	//HyperNewbie particles
	QMB_AddParticleType(p_slimeglow, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_lava, 72, 0, 0, pm_nophysics, 0, 4, &count, "part:slimeglow"); //Glow
	QMB_AddParticleType(p_slimebubble, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE_MINUS_SRC_ALPHA, ptex_bubble, 204, 0, 0, pm_static, 0, 4, &count, "part:slimebubble");
	QMB_AddParticleType(p_blacklavasmoke, pd_billboard, BLEND_GL_ZERO_GL_ONE_MINUS_SRC_COLOR_CONSTANT, ptex_smoke, 140, 3, 0, pm_normal, 0, 4, &count, "part:blacklavasmoke");

	//Allow overkill trails
	if (amf_part_fulldetail.integer) {
		QMB_AddParticleType(p_streaktrail, pd_billboard, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_generic, 128, 0, 0, pm_static, 0, 4, &count, "part:streaktrail");
	}
	else {
		QMB_AddParticleType(p_streaktrail, pd_beam, BLEND_GL_SRC_ALPHA_GL_ONE, ptex_none, 128, 0, 0, pm_die, 0, 4, &count, "part:streaktrail");
	}
	QMB_SortParticleTypes();

	qmb_initialized = true;
}

void QMB_ShutdownParticles(void)
{
	Q_free(particles);
}

void QMB_ClearParticles (void)
{
	int	i;

	if (!qmb_initialized) {
		return;
	}

	Q_free(particles);		// free
	QMB_AllocParticles ();	// and alloc again

	particle_count = 0;
	memset(particles, 0, r_numparticles * sizeof(particle_t));
	free_particles = &particles[0];

	for (i = 0; i + 1 < r_numparticles; i++) {
		particles[i].next = &particles[i + 1];
	}
	particles[r_numparticles - 1].next = NULL;

	for (i = 0; i < num_particletypes; i++) {
		particle_types[i].start = NULL;
	}

	//VULT STATS
	ParticleCount = 0;
	ParticleCountHigh = 0;
}

static void QMB_BillboardAddVert(r_sprite3d_vert_t* vert, particle_type_t* type, float x, float y, float z, float s, float t, col_t color, int texture_index)
{
	part_blend_info_t* blend = &blend_options[type->blendtype];
	col_t new_color;

	blend->color_transform(color, new_color);

	R_Sprite3DSetVert(vert, x, y, z, s, t, new_color, texture_index);
}

__inline static void CALCULATE_PARTICLE_BILLBOARD(particle_texture_t* ptex, particle_type_t* type, particle_t * p, vec3_t coord[4], int pos)
{
	part_blend_info_t* blend = &blend_options[type->blendtype];
	vec3_t verts[4];
	float scale = p->size;
	r_sprite3d_vert_t* vert;
	col_t new_color;

	vert = R_Sprite3DAddEntry(type->billboard_type, 4);
	if (!vert) {
		return;
	}

	if (p->rotspeed) {
		matrix3x3_t rotate_matrix;
		Matrix3x3_CreateRotate(rotate_matrix, DEG2RAD(p->rotangle), vpn);

		Matrix3x3_MultiplyByVector(verts[0], (const vec_t (*)[3]) rotate_matrix, coord[0]);
		Matrix3x3_MultiplyByVector(verts[1], (const vec_t (*)[3]) rotate_matrix, coord[1]);
		// do some fast math for verts[2] and verts[3].
		VectorNegate(verts[0], verts[2]);
		VectorNegate(verts[1], verts[3]);

		VectorMA(p->org, scale, verts[0], verts[0]);
		VectorMA(p->org, scale, verts[1], verts[1]);
		VectorMA(p->org, scale, verts[2], verts[2]);
		VectorMA(p->org, scale, verts[3], verts[3]);
	}
	else {
		VectorMA(p->org, scale, coord[0], verts[0]);
		VectorMA(p->org, scale, coord[1], verts[1]);
		VectorMA(p->org, scale, coord[2], verts[2]);
		VectorMA(p->org, scale, coord[3], verts[3]);
	}

	// Set color
	blend->color_transform(p->color, new_color);

	R_Sprite3DSetVert(vert++, verts[0][0], verts[0][1], verts[0][2], ptex->coords[p->texindex][0], ptex->coords[p->texindex][3], new_color, ptex->tex_index);
	R_Sprite3DSetVert(vert++, verts[3][0], verts[3][1], verts[3][2], ptex->coords[p->texindex][2], ptex->coords[p->texindex][3], new_color, ptex->tex_index);
	R_Sprite3DSetVert(vert++, verts[1][0], verts[1][1], verts[1][2], ptex->coords[p->texindex][0], ptex->coords[p->texindex][1], new_color, ptex->tex_index);
	R_Sprite3DSetVert(vert++, verts[2][0], verts[2][1], verts[2][2], ptex->coords[p->texindex][2], ptex->coords[p->texindex][1], new_color, ptex->tex_index);

	return;
}

static void QMB_FillParticleVertexBuffer(void)
{
	vec3_t billboard[4], velcoord[4];
	particle_type_t* pt;
	particle_t* p;
	int i, j, k;
	int l;
	int pos = 0;

	VectorAdd(vup, vright, billboard[2]);
	VectorSubtract(vright, vup, billboard[3]);
	VectorNegate(billboard[2], billboard[0]);
	VectorNegate(billboard[3], billboard[1]);

	for (i = 0; i < num_particletypes; i++) {
		qbool first = true;

		pt = &particle_types[i];

		if (!pt->start) {
			continue;
		}
		if (pt->drawtype == pd_hide) {
			continue;
		}

		//VULT PARTICLES
		switch (pt->drawtype) {
		case pd_beam:
			for (p = pt->start; p; p = p->next) {
				particle_texture_t* ptex = &particle_textures[ptex_lightning];
				vec3_t right1, right2;
				float half_t = (ptex->coords[0][1] + ptex->coords[0][3]) * 0.5f;
				int trail_parts = min(amf_part_traildetail.integer, MAX_BEAM_TRAIL);

				if (i != particle_type_index[p_lightningbeam]) {
					trail_parts = 1;
				}

				if (particle_time < p->start || particle_time >= p->die) {
					continue;
				}

				if (first) {
					R_Sprite3DInitialiseBatch(pt->billboard_type, pt->state, pt->state, TEXTURE_DETAILS(ptex), r_primitive_triangle_strip);
					first = false;
				}

				R_PreCalcBeamVerts(p->org, p->endorg, right1, right2);
				for (l = trail_parts; l > 0; l--) {
					r_sprite3d_vert_t* vert = R_Sprite3DAddEntry(pt->billboard_type, 6);
					if (!vert) {
						break;
					}

					R_CalcBeamVerts(varray_vertex, p->org, p->endorg, right1, right2, p->size / (l * amf_part_trailwidth.value));

					QMB_BillboardAddVert(vert++, pt, varray_vertex[4], varray_vertex[5], varray_vertex[6], ptex->coords[0][2], ptex->coords[0][3], p->color, ptex->tex_index);
					QMB_BillboardAddVert(vert++, pt, varray_vertex[8], varray_vertex[9], varray_vertex[10], ptex->coords[0][0], ptex->coords[0][3], p->color, ptex->tex_index);

					// near center
					QMB_BillboardAddVert(vert++, pt, (varray_vertex[0] + varray_vertex[4]) / 2, (varray_vertex[1] + varray_vertex[5]) / 2, (varray_vertex[2] + varray_vertex[6]) / 2, ptex->coords[0][2], half_t, p->color, ptex->tex_index);
					// far center
					QMB_BillboardAddVert(vert++, pt, (varray_vertex[8] + varray_vertex[12]) / 2, (varray_vertex[9] + varray_vertex[13]) / 2, (varray_vertex[10] + varray_vertex[14]) / 2, ptex->coords[0][0], half_t, p->color, ptex->tex_index);

					QMB_BillboardAddVert(vert++, pt, varray_vertex[0], varray_vertex[1], varray_vertex[2], ptex->coords[0][2], ptex->coords[0][1], p->color, ptex->tex_index);
					QMB_BillboardAddVert(vert++, pt, varray_vertex[12], varray_vertex[13], varray_vertex[14], ptex->coords[0][0], ptex->coords[0][1], p->color, ptex->tex_index);
				}
			}
			break;
		case pd_spark:
		case pd_sparkray:
			for (p = pt->start; p; p = p->next) {
				vec3_t neworg;
				float* point;
				byte farColor[4];
				particle_texture_t* ptex = &particle_textures[ptex_none];
				r_sprite3d_vert_t* vert;

				if (particle_time < p->start || particle_time >= p->die) {
					continue;
				}

				if (first) {
					R_Sprite3DInitialiseBatch(pt->billboard_type, pt->state, pt->state, TEXTURE_DETAILS(ptex), r_primitive_triangle_fan);
					first = false;
				}

				vert = R_Sprite3DAddEntry(pt->billboard_type, pt->verts_per_primitive);
				if (vert) {
					vec3_t v;

					if (!TraceLineN(p->endorg, p->org, neworg, NULL)) {
						VectorCopy(p->org, neworg);
					}

					point = (pt->drawtype == pd_spark ? p->org : p->endorg);
					farColor[0] = p->color[0] >> 1;
					farColor[1] = p->color[1] >> 1;
					farColor[2] = p->color[2] >> 1;
					farColor[3] = 0;

					QMB_BillboardAddVert(vert++, pt, point[0], point[1], point[2], ptex->coords[0][0], ptex->coords[0][1], p->color, -1);
					if (pt->drawtype == pd_spark) {
						for (j = 7; j >= 0; j--) {
							for (k = 0; k < 3; k++) {
								v[k] = p->org[k] - p->vel[k] / 8 + vright[k] * cost[j % 7] * p->size + vup[k] * sint[j % 7] * p->size;
							}
							QMB_BillboardAddVert(vert++, pt, v[0], v[1], v[2], ptex->coords[0][0], ptex->coords[0][1], farColor, -1);
						}
					}
					else {
						for (j = 7; j >= 0; j--) {
							for (k = 0; k < 3; k++) {
								v[k] = neworg[k] + vright[k] * cost[j % 7] * p->size + vup[k] * sint[j % 7] * p->size;
							}
							QMB_BillboardAddVert(vert++, pt, v[0], v[1], v[2], ptex->coords[0][0], ptex->coords[0][1], farColor, -1);
						}
					}
				}
			}
			break;
		case pd_billboard:
		case pd_billboard_vel:
			{
				int drawncount = 0;
				particle_texture_t* ptex = &particle_textures[pt->texture];

				for (p = pt->start; p; p = p->next) {
					if (particle_time < p->start || particle_time >= p->die) {
						continue;
					}

					if (first) {
						R_Sprite3DInitialiseBatch(pt->billboard_type, pt->state, pt->state, TEXTURE_DETAILS(ptex), r_primitive_triangle_strip);
						first = false;
					}

					if (pt->drawtype == pd_billboard) {
						if (gl_clipparticles.integer) {
							if (drawncount >= 3 && VectorSupCompare(p->org, r_origin, 30)) {
								continue;
							}
							drawncount++;
						}

						CALCULATE_PARTICLE_BILLBOARD(ptex, pt, p, billboard, pos);
					}
					else if (pt->drawtype == pd_billboard_vel) {
						vec3_t up, right;

						VectorCopy(p->vel, up);
						CrossProduct(vpn, up, right);
						VectorNormalizeFast(right);
						VectorScale(up, pt->custom, up);

						VectorAdd(up, right, velcoord[2]);
						VectorSubtract(right, up, velcoord[3]);
						VectorNegate(velcoord[2], velcoord[0]);
						VectorNegate(velcoord[3], velcoord[1]);

						CALCULATE_PARTICLE_BILLBOARD(ptex, pt, p, velcoord, pos);
					}
				}
			}
			break;

			//VULT PARTICLES - This produces the shockwave effect
			//Mind you it can be used for more than that... Decals come to mind first
		case pd_normal:
			{
				float matrix[16];
				particle_texture_t* ptex = &particle_textures[pt->texture];

				for (p = pt->start; p; p = p->next) {
					float vector[4][4];
					r_sprite3d_vert_t* vert;

					if (particle_time < p->start || particle_time >= p->die) {
						continue;
					}

					if (first) {
						R_Sprite3DInitialiseBatch(pt->billboard_type, pt->state, pt->state, TEXTURE_DETAILS(ptex), r_primitive_triangle_strip);
						first = false;
					}

					vert = R_Sprite3DAddEntry(pt->billboard_type, 4);
					if (vert) {
						R_SetIdentityMatrix(matrix);
						R_TransformMatrix(matrix, p->org[0], p->org[1], p->org[2]);
						R_ScaleMatrix(matrix, p->size, p->size, p->size);
						R_RotateMatrix(matrix, p->endorg[0], 0, 1, 0);
						R_RotateMatrix(matrix, p->endorg[1], 0, 0, 1);
						R_RotateMatrix(matrix, p->endorg[2], 1, 0, 0);

						R_MultiplyVector3f(matrix, -p->size, -p->size, 0, vector[0]);
						R_MultiplyVector3f(matrix, p->size, -p->size, 0, vector[1]);
						R_MultiplyVector3f(matrix, p->size, p->size, 0, vector[2]);
						R_MultiplyVector3f(matrix, -p->size, p->size, 0, vector[3]);

						QMB_BillboardAddVert(vert++, pt, vector[0][0], vector[0][1], vector[0][2], ptex->coords[0][0], ptex->coords[0][1], p->color, ptex->tex_index);
						QMB_BillboardAddVert(vert++, pt, vector[3][0], vector[3][1], vector[3][2], ptex->coords[0][0], ptex->coords[0][3], p->color, ptex->tex_index);
						QMB_BillboardAddVert(vert++, pt, vector[1][0], vector[1][1], vector[1][2], ptex->coords[0][2], ptex->coords[0][1], p->color, ptex->tex_index);
						QMB_BillboardAddVert(vert++, pt, vector[2][0], vector[2][1], vector[2][2], ptex->coords[0][2], ptex->coords[0][3], p->color, ptex->tex_index);

						if ((vert = R_Sprite3DAddEntry(pt->billboard_type, 4))) {
							R_RotateMatrix(matrix, 180, 1, 0, 0);

							R_MultiplyVector3f(matrix, -p->size, -p->size, 0, vector[0]);
							R_MultiplyVector3f(matrix, p->size, -p->size, 0, vector[1]);
							R_MultiplyVector3f(matrix, p->size, p->size, 0, vector[2]);
							R_MultiplyVector3f(matrix, -p->size, p->size, 0, vector[3]);

							QMB_BillboardAddVert(vert++, pt, vector[0][0], vector[0][1], vector[0][2], ptex->coords[0][0], ptex->coords[0][1], p->color, ptex->tex_index);
							QMB_BillboardAddVert(vert++, pt, vector[3][0], vector[3][1], vector[3][2], ptex->coords[0][0], ptex->coords[0][3], p->color, ptex->tex_index);
							QMB_BillboardAddVert(vert++, pt, vector[1][0], vector[1][1], vector[1][2], ptex->coords[0][2], ptex->coords[0][1], p->color, ptex->tex_index);
							QMB_BillboardAddVert(vert++, pt, vector[2][0], vector[2][1], vector[2][2], ptex->coords[0][2], ptex->coords[0][3], p->color, ptex->tex_index);
						}
					}
				}
			}
			break;
		default:
			assert(!"QMB_DrawParticles: unexpected drawtype");
			break;
		}
	}
}

void QMB_ProcessParticle(particle_type_t* pt, particle_t* p)
{
	float grav = movevars.gravity / 800.0;
	vec3_t oldorg, stop, normal, movement;
	int contents;
	float bounce;

	p->size += p->growth * cls.frametime;

	if (p->size <= 0) {
		p->die = 0;
		return;
	}

	//VULT PARTICLE
	if (pt->id == p_streaktrail || pt->id == p_lightningbeam) {
		p->color[3] = p->bounces * ((p->die - particle_time) / (p->die - p->start));
	}
	else {
		p->color[3] = pt->startalpha * ((p->die - particle_time) / (p->die - p->start));
	}

	if (p->color[3] <= 0) {
		p->die = 0;
		return;
	}

	p->rotangle += p->rotspeed * cls.frametime;

	if (p->hit) {
		return;
	}

	//VULT - switched these around so velocity is scaled before gravity is applied
	VectorScale(p->vel, 1 + pt->accel * cls.frametime, p->vel);
	p->vel[2] += pt->grav * grav * cls.frametime;

	switch (pt->move) {
		case pm_static:
			break;
		case pm_normal:
			VectorCopy(p->org, oldorg);
			VectorScale(p->vel, cls.frametime, movement);
			VectorAdd(p->org, movement, p->org);
			if (ParticleContents(p, movement) == CONTENTS_SOLID) {
				p->hit = 1;
				VectorCopy(oldorg, p->org);
				VectorClear(p->vel);
			}
			break;
		case pm_float:
			VectorScale(p->vel, cls.frametime, movement);
			movement[2] += p->size + 1;
			VectorAdd(p->org, movement, p->org);
			contents = ParticleContents(p, movement);
			if (!ISUNDERWATER(contents)) {
				p->die = 0;
			}
			p->org[2] -= p->size + 1;
			break;
		case pm_nophysics:
			VectorMA(p->org, cls.frametime, p->vel, p->org);
			break;
		case pm_die:
			VectorScale(p->vel, cls.frametime, movement);
			VectorAdd(p->org, movement, p->org);
			if (CONTENTS_SOLID == ParticleContents(p, movement)) {
				p->die = 0;
			}
			break;
		case pm_bounce:
			if (!gl_bounceparticles.value || p->bounces) {
				if (pt->id == p_smallspark) {
					VectorCopy(p->org, p->endorg);
				}

				VectorScale(p->vel, cls.frametime, movement);
				VectorAdd(p->org, movement, p->org);
				if (CONTENTS_SOLID == ParticleContents(p, movement)) {
					p->die = 0;
				}
			}
			else {
				VectorCopy(p->org, oldorg);
				if (pt->id == p_smallspark) {
					VectorCopy(oldorg, p->endorg);
				}
				VectorScale(p->vel, cls.frametime, movement);
				VectorAdd(p->org, movement, p->org);
				if (CONTENTS_SOLID == ParticleContents(p, movement)) {
					if (TraceLineN(oldorg, p->org, stop, normal)) {
						VectorCopy(stop, p->org);
						bounce = -pt->custom * DotProduct(p->vel, normal);
						VectorMA(p->vel, bounce, normal, p->vel);
						p->bounces++;
						if (pt->id == p_smallspark) {
							VectorCopy(stop, p->endorg);
						}
					}
				}
			}
			break;
			//VULT PARTICLES
		case pm_rain:
			VectorCopy(p->org, oldorg);
			VectorScale(p->vel, cls.frametime, movement);
			VectorAdd(p->org, movement, p->org);
			contents = ParticleContents(p, movement);
			if (ISUNDERWATER(contents) || contents == CONTENTS_SOLID) {
				if (!amf_weather_rain_fast.value || amf_weather_rain_fast.value == 2) {
					vec3_t rorg;
					VectorCopy(oldorg, rorg);
					//Find out where the rain should actually hit
					//This is a slow way of doing it, I'll fix it later maybe...
					while (1) {
						rorg[2] = rorg[2] - 0.5f;
						contents = TruePointContents(rorg);
						if (contents == CONTENTS_WATER) {
							if (amf_weather_rain_fast.value == 2) {
								break;
							}
							RainSplash(rorg);
							break;
						}
						else if (contents == CONTENTS_SOLID) {
							byte col[3] = { 128,128,128 };
							SparkGen(rorg, col, 3, 50, 0.15);
							break;
						}
					}
					VectorCopy(rorg, p->org);
					VX_ParticleTrail(oldorg, p->org, p->size, 0.2, p->color);
				}
				p->die = 0;
			}
			else {
				VX_ParticleTrail(oldorg, p->org, p->size, 0.2, p->color);
			}
			break;
			//VULT PARTICLES
		case pm_streak:
			VectorCopy(p->org, oldorg);
			VectorScale(p->vel, cls.frametime, movement);
			VectorAdd(p->org, movement, p->org);
			if (CONTENTS_SOLID == ParticleContents(p, movement)) {
				if (TraceLineN(oldorg, p->org, stop, normal)) {
					VectorCopy(stop, p->org);
					bounce = -pt->custom * DotProduct(p->vel, normal);
					VectorMA(p->vel, bounce, normal, p->vel);
				}
			}
			VX_ParticleTrail(oldorg, p->org, p->size, 0.2, p->color);
			if (VectorLength(p->vel) == 0) {
				p->die = 0;
			}
			break;
		case pm_streakwave:
			VectorCopy(p->org, oldorg);
			VectorMA(p->org, cls.frametime, p->vel, p->org);
			VX_ParticleTrail(oldorg, p->org, p->size, 0.5, p->color);
			p->vel[0] = 19 * p->vel[0] / 20;
			p->vel[1] = 19 * p->vel[1] / 20;
			p->vel[2] = 19 * p->vel[2] / 20;
			break;
		default:
			assert(!"QMB_UpdateParticles: unexpected pt->move");
			break;
	}
}

// TODO: Split up
static void QMB_UpdateParticles(void)
{
	int i;
	particle_type_t *pt;
	particle_t *p;
	particle_t **prev;

	if (!qmb_initialized) {
		return;
	}

	particle_count = 0;

	//VULT PARTICLES
	WeatherEffect();

	for (i = 0; i < num_particletypes; i++) {
		pt = &particle_types[i];

#ifdef _WIN32
		if (pt && ((uintptr_t)pt->start == 1)) {
			/* hack! fixme!
			 * for some reason in some occasions - MS VS 2005 compiler
			 * this address doesn't point to 0 as other unitialized do, but to 0x00000001 */
			pt->start = NULL;
			Com_DPrintf("ERROR: particle_type[%i].start == 1\n", i);
		}
#endif // _WIN32

		prev = &pt->start;
		for (p = pt->start; p; ) {
			if (particle_time >= p->die) {
				*prev = p->next;
				p->next = free_particles;
				free_particles = p;
				ParticleStats(-1);
				p = *prev;
			}
			else if (particle_time >= p->start) {
				particle_count++;

				QMB_ProcessParticle(pt, p);

				prev = &p->next;
				p = p->next;
			}
		}
	}
}

void QMB_CalculateParticles(void)
{
	if (!qmb_initialized) {
		return;
	}

	particle_time = r_refdef2.time;

	if (!ISPAUSED) {
		QMB_UpdateParticles();
	}
}
void QMB_DrawParticles(void)
{
	if (!qmb_initialized) {
		return;
	}

	QMB_FillParticleVertexBuffer();
}

//VULT STATS
void ParticleStats (int change)
{
	if (ParticleCount > ParticleCountHigh)
		ParticleCountHigh = ParticleCount;
	ParticleCount+=change;
}

//from darkplaces engine - finds which corner of a particle goes where, so I don't have to :D
static void R_PreCalcBeamVerts(vec3_t org1, vec3_t org2, vec3_t right1, vec3_t right2)
{
	vec3_t diff, normal;

	VectorSubtract(org2, org1, normal);
	VectorNormalize(normal);

	// calculate 'right' vector for start
	VectorSubtract(r_origin, org1, diff);
	VectorMA(diff, 32, vup, diff);
	VectorNormalize(diff);
	CrossProduct(normal, diff, right1);

	// calculate 'right' vector for end
	VectorSubtract(r_origin, org2, diff);
	VectorMA(diff, 32, vup, diff);
	VectorNormalize(diff);
	CrossProduct(normal, diff, right2);
}

static void R_CalcBeamVerts(float *vert, const vec3_t org1, const vec3_t org2, const vec3_t right1, const vec3_t right2, float width)
{
	VectorMA(org1, +width, right1, &vert[0]);
	VectorMA(org1, -width, right1, &vert[4]);
	VectorMA(org2, -width, right2, &vert[8]);
	VectorMA(org2, +width, right2, &vert[12]);
}

int QMB_ParticleTextureCount(void)
{
	return num_particletextures;
}

void QMB_ImportTextureArrayReferences(texture_flag_t* texture_flags)
{
	part_tex_t tex;
	int i;

	for (tex = 0; tex < num_particletextures; ++tex) {
		if (R_TextureReferenceIsValid(particle_textures[tex].texnum)) {
			texture_array_ref_t* array_ref = &texture_flags[particle_textures[tex].texnum.index].array_ref[TEXTURETYPES_SPRITES];

			particle_textures[tex].tex_array = array_ref->ref;
			particle_textures[tex].tex_index = array_ref->index;
			for (i = 0; i < particle_textures[tex].components; ++i) {
				particle_textures[tex].coords[i][0] *= array_ref->scale_s;
				particle_textures[tex].coords[i][2] *= array_ref->scale_s;
				particle_textures[tex].coords[i][1] *= array_ref->scale_t;
				particle_textures[tex].coords[i][3] *= array_ref->scale_t;
			}
		}
	}
}

void QMB_FlagTexturesForArray(texture_flag_t* texture_flags)
{
	part_tex_t tex;

	for (tex = 0; tex < num_particletextures; ++tex) {
		if (R_TextureReferenceIsValid(particle_textures[tex].texnum)) {
			texture_flags[particle_textures[tex].texnum.index].flags |= (1 << TEXTURETYPES_SPRITES);
			memcpy(particle_textures[tex].coords, particle_textures[tex].originalCoords, sizeof(particle_textures[tex].coords));
		}
	}
}
