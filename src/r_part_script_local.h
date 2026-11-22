/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2016      Spike

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

// Shared between the r_part_script*.c files only.

#ifndef EZQUAKE_R_PART_SCRIPT_LOCAL_H
#define EZQUAKE_R_PART_SCRIPT_LOCAL_H

#include "r_part_script.h"
#include "r_scenetris.h"

typedef float pscript_vec2_t[2];
typedef float pscript_vec4_t[4];

#define frandom() (rand() * (1.0f / RAND_MAX))
#define crandom() (rand() * (2.0f / RAND_MAX) - 1.0f)
#define hrandom() (rand() * (1.0f / RAND_MAX) - 0.5f)
// -1 to 1 in 32767 steps whatever RAND_MAX is; used by orgwrand/velwrand.
#define crand() (rand() % 32767 / 16383.5f - 1)

// Coarse sine in radians for spawn patterns and sprite rotation, where libm
// is too slow.
#define PSIN(x) (pscript_sintable[PScript_SinIndex(x)])
#define PCOS(x) (pscript_sintable[(PScript_SinIndex(x) + 64) & 255])

// mathlib.h's VectorMA, VectorScale and VectorInterpolate end in ';' and
// share a global temporary, so they can neither be the body of an if with an
// else nor be nested. These evaluate each argument once.
#define PS_VectorMA(a, s, b, c) \
	do { \
		float ps_s_ = (s); \
		(c)[0] = (a)[0] + ps_s_ * (b)[0]; \
		(c)[1] = (a)[1] + ps_s_ * (b)[1]; \
		(c)[2] = (a)[2] + ps_s_ * (b)[2]; \
	} while (0)
#define PS_VectorScale(in, s, out) \
	do { \
		float ps_s_ = (s); \
		(out)[0] = (in)[0] * ps_s_; \
		(out)[1] = (in)[1] * ps_s_; \
		(out)[2] = (in)[2] * ps_s_; \
	} while (0)
#define PS_VectorInterpolate(a, frac, b, c) \
	do { \
		float ps_f_ = (frac); \
		(c)[0] = (a)[0] + ((b)[0] - (a)[0]) * ps_f_; \
		(c)[1] = (a)[1] + ((b)[1] - (a)[1]) * ps_f_; \
		(c)[2] = (a)[2] + ((b)[2] - (a)[2]) * ps_f_; \
	} while (0)
#define PS_FloatInterpolate(a, frac, b, c) \
	do { \
		float ps_a_ = (a); \
		(c) = ps_a_ + ((b) - ps_a_) * (frac); \
	} while (0)

// r_part_maxparticles and r_part_maxdecals are clamped to these. A particle
// takes six vertices and a decal triangle three, and decals are drawn first:
// a full decal pool takes half a view's vertices, leaving half to particles.
#define PSCRIPT_MAX_PARTICLES   (SCENETRIS_MAX_VERTS / 6)
#define PSCRIPT_MAX_DECALS      (SCENETRIS_MAX_VERTS / 6)
#define PSCRIPT_MAX_BEAMSEGS    (1 << 11)
#define PSCRIPT_MAX_TRAILSTATES (1 << 10)

// trailkey_t layout: index + 1 in the low bits, so 0 stays free for "none".
// 12 bits hold PSCRIPT_MAX_TRAILSTATES with room to grow and leave 20 bits of
// generation, about a million reuses of one slot before a stale key aliases.
#define PSCRIPT_TRAILKEY_INDEXBITS 12
#define PSCRIPT_TRAILKEY_INDEXMASK ((1u << PSCRIPT_TRAILKEY_INDEXBITS) - 1)

// Point contents as bits, so effects can test a set of contents at once.
#define PSCRIPT_CONTENTS_EMPTY      0
#define PSCRIPT_CONTENTS_SOLID      1
#define PSCRIPT_CONTENTS_WATER      2
#define PSCRIPT_CONTENTS_SLIME      4
#define PSCRIPT_CONTENTS_LAVA       8
#define PSCRIPT_CONTENTS_SKY        16
#define PSCRIPT_CONTENTS_FLUID      (PSCRIPT_CONTENTS_WATER | PSCRIPT_CONTENTS_SLIME | PSCRIPT_CONTENTS_LAVA | PSCRIPT_CONTENTS_SKY)
#define PSCRIPT_CONTENTS_PLAYERCLIP 0 // Q1 BSP has no clip contents

#define PART_VALID(part) ((part) >= 0 && (part) < pscript_numtypes)

// Same order as scenetris_blend_t, so the renderer can cast.
typedef enum {
	BM_BLEND,       // SRC_ALPHA ONE_MINUS_SRC_ALPHA
	BM_BLENDCOLOUR, // SRC_COLOR ONE_MINUS_SRC_COLOR
	BM_ADDA,        // SRC_ALPHA ONE
	BM_ADDC,        // SRC_COLOR ONE
	BM_SUBTRACT,    // SRC_ALPHA ONE_MINUS_SRC_COLOR
	BM_INVMODA,     // ZERO ONE_MINUS_SRC_ALPHA
	BM_INVMODC,     // ZERO ONE_MINUS_SRC_COLOR
	BM_PREMUL       // ONE ONE_MINUS_SRC_ALPHA
} pscript_blendmode_t;

typedef enum {
	PT_NORMAL,
	PT_SPARK,
	PT_SPARKFAN,
	PT_TEXTUREDSPARK,
	PT_BEAM,
	PT_CDECAL,
	PT_UDECAL,
	PT_INVISIBLE
} pscript_looktype_t;

typedef enum {
	SM_BOX,        // even spread within the area
	SM_CIRCLE,     // around the edge of a circle
	SM_BALL,       // filled sphere
	SM_SPIRAL,     // spiral trail
	SM_TRACER,     // tracer trail
	SM_TELEBOX,    // Q1 teleport box
	SM_LAVASPLASH, // Q1 lava splash
	SM_UNICIRCLE,  // uniform circle
	SM_FIELD,      // synced field (brightfield)
	SM_DISTBALL    // unevenly distributed ball
} pscript_spawnmode_t;

typedef enum {
	RAMP_NONE,
	RAMP_DELTA,
	RAMP_NEAREST,
	RAMP_LERP
} pscript_rampmode_t;

typedef struct pscript_particle_s {
	struct pscript_particle_s *next;
	float die;

	vec3_t org;
	pscript_vec4_t rgba;
	float scale;
	float s1, t1, s2, t2;

	vec3_t oldorg; // last traced position, so short moves skip the trace
	vec3_t vel;
	float angle;
	union {
		float nextemit;        // when the type's emittime >= 0
		trailkey_t trailstate; // when the type's emittime < 0
	} state;
	float rotationspeed;
} pscript_particle_t;

// Triangle of a decal clipped to the surfaces of the world, in world space,
// or of a brush entity, relative to the entity's origin.
typedef struct pscript_clippeddecal_s {
	struct pscript_clippeddecal_s *next;
	float die;
	// The entity number, 0 for the world, and the model index the entity had;
	// the decal is drawn only while the entity still has that model.
	unsigned short entity;
	unsigned short modelindex;

	vec3_t vertex[3];
	pscript_vec2_t texcoords[3];
	float valpha[3];

	pscript_vec4_t rgba;
} pscript_clippeddecal_t;

#define BS_LASTSEG 0x1 // held by a trail state as its last segment: not drawn to next, not freed
#define BS_DEAD    0x2
#define BS_NODRAW  0x4 // end of a chain: not drawn to next

typedef struct pscript_beamseg_s {
	struct pscript_beamseg_s *next;

	pscript_particle_t *p;
	int flags;
	vec3_t dir;

	float texture_s;
} pscript_beamseg_t;

typedef struct pscript_trailstate_s {
	trailkey_t key;
	trailkey_t assoc;            // state of the next type in the assoc chain
	unsigned int generation;     // survives cleaning, so reissued keys differ
	pscript_beamseg_t *lastbeam; // flagged BS_LASTSEG
	int lastbeamtype;            // index of the type whose beams lastbeam is on
	int headtype;                // index + 1 of the type last fetched for, or 0
	union {
		struct {
			float lastdist; // trail length at the last particle
			float laststop; // trail length so far
		} trail;
		struct {
			float statetime; // particle clock time before which spawntime blocks
			float emittime;  // fractional count carried to the next emit
		} effect;
	} u;
} pscript_trailstate_t;

// Surface triangle that emits a texture's effect (tex_<name> or texpart_).
typedef struct pscript_surftri_s {
	struct pscript_surftri_s *next;
	vec3_t org;
	vec3_t x;
	vec3_t y;
	float area;
	double nexttime;
	int ptype;
	float maxlife; // longest die along ptype's assoc chain, bounded
	struct msurface_s *face;
} pscript_surftri_t;

typedef struct pscript_surftriblock_s {
	struct pscript_surftriblock_s *next;
	unsigned int count;
	pscript_surftri_t tris[1024];
} pscript_surftriblock_t;

// Render state shared by every instance of a type.
typedef struct pscript_looks_s {
	pscript_looktype_t type;
	pscript_blendmode_t blendmode;
	texture_ref texture;
	qbool nearest;

	float scalefactor;
	float invscalefactor;
	float stretch;
	float minstretch; // limits a spark's length to a multiple of its width
	int premul;       // 0: rgba as is, 1: rgb * a, a (blend), 2: rgb * a, 0 (add)
	qbool vbeam;      // a beam's t runs along it and s across it, instead of the reverse
} pscript_looks_t;

// Absolute values or per-second deltas, depending on the ramp mode.
typedef struct pscript_ramp_s {
	vec3_t rgb;
	float alpha;
	float scale;
} pscript_ramp_t;

typedef struct pscript_sound_s {
	char name[MAX_QPATH];
	float vol;
	float atten;
	float weight;
} pscript_sound_t;

typedef struct pscript_type_s {
	char name[MAX_QPATH];
	char config[MAX_QPATH];
	char texname[MAX_QPATH];

	int numsounds;
	pscript_sound_t *sounds;

	vec3_t rgb;
	float alpha;
	vec3_t rgbchange; // per second
	float alphachange;
	vec3_t rgbrand;
	float alpharand;
	int colorindex; // palette index, or -1 to use rgb
	int colorrand;
	float rgbchangetime;
	vec3_t rgbrandsync; // like rgbrand, but one random value shared by the channels
	float scale;
	float scalerand;
	float die, randdie;
	float veladd, randomveladd;
	float orgadd, randomorgadd;
	float spawnvel, spawnvelvert;
	vec3_t orgbias;
	vec3_t velbias;
	vec3_t orgwrand;
	vec3_t velwrand;
	float flurry;
	unsigned int surfflagmatch; // decals only land where (texinfo flags & mask) == match
	unsigned int surfflagmask;

	float s1, t1, s2, t2;
	float texsstride; // added to s for each random atlas slot
	int randsmax;

	pscript_looks_t looks;

	float spawntime; // minimum seconds between two spawns through one trail state
	float spawnchance;

	float rotationstartmin, rotationstartrand;
	float rotationmin, rotationrand;

	float scaledelta;
	float countextra;
	float count;
	float countrand;
	float countspacing;
	float countoverflow; // fraction of a trail particle carried to the next frame
	float rainfrequency;

	int assoc;
	int cliptype;
	int inwater;
	float clipcount;
	int emit;
	float emittime;
	float emitrand;
	float emitstart;

	float areaspread;
	float areaspreadvert;

	float spawnparam1;
	float spawnparam2;

	pscript_spawnmode_t spawnmode;

	float gravity;
	vec3_t friction;
	float clipbounce;

	vec3_t dl_rgb;
	float dl_radius[2]; // base radius and random extra
	float dl_time;
	float dl_decay; // radius lost per second
	vec3_t dl_rgbfade; // dl_rgb lost per second

	pscript_rampmode_t rampmode;
	int rampindexes;
	pscript_ramp_t *ramp;

	int loaded; // 0 not loaded, 1 loaded from a weak config, 2 loaded
	pscript_particle_t *particles;
	pscript_clippeddecal_t *clippeddecals;
	pscript_beamseg_t *beams;
	// Of a die 0 type, the first particle and beam segment that end when the
	// next host frame that advances time begins, or NULL; every instance
	// after them in particles and beams ends too. Spawning links instances in
	// front, so those spawned since they were marked stay.
	pscript_particle_t *expiring;
	pscript_beamseg_t *expiringbeams;
	struct pscript_type_s *nexttorun;
	struct pscript_type_s **runlink; // the pointer that points at this type in the run list

	unsigned int flags;
	unsigned int fluidmask;
	unsigned int state;
} pscript_type_t;

#define PT_VELOCITY       0x0001 // has velocity modifiers
#define PT_FRICTION       0x0002 // has friction modifiers
#define PT_CITRACER       0x0008 // Q1 tracer palette alternation for colorindex
#define PT_INVFRAMETIME   0x0010 // count is per host frame, not per second
#define PT_AVERAGETRAIL   0x0020 // spread trail points so the last lands on the end
#define PT_NOSTATE        0x0040 // ignore the trail state, careful with assoc
#define PT_NOSPREADFIRST  0x0080 // no random org/vel for the first trail particle
#define PT_NOSPREADLAST   0x0100 // no random org/vel for the last trail particle
#define PT_TROVERWATER    0x0200 // don't spawn in fluidmask contents
#define PT_TRUNDERWATER   0x0400 // only spawn in fluidmask contents
#define PT_WORLDSPACERAND 0x1000 // has orgwrand, velwrand or velbias
#define PT_SPANBEAM       0x2000 // beam of unmoving points: one segment per call, or a point every countspacing units along a trail state

#define PS_INRUNLIST 0x1

typedef struct pscript_counts_s {
	int particles;
	int beams;
	int decals;
} pscript_counts_t;

// numtris triangles, three points each.
typedef void (*pscript_decalcallback_t)(void *ctx, vec3_t *points, size_t numtris);

// r_part_script.c
extern pscript_type_t *pscript_types;
extern qbool pscript_anyloaded; // a loaded effect type exists
qbool PScript_HasRedirects(void);
extern int pscript_numtypes;
extern pscript_type_t *pscript_runlist;
extern qbool pscript_effects_dirty;
extern int pscript_pe_default;
extern int pscript_pe_size2;
extern int pscript_pe_size3;
extern int pscript_pe_colors[256]; // the pe_<n> types, by palette index
extern qbool pscript_expsprite;
extern cvar_t r_bouncysparks;
extern cvar_t r_decal_noperpendicular;
extern cvar_t r_part_rain;
extern cvar_t r_part_rain_quantity;
extern cvar_t r_part_rain_prewarm;
extern cvar_t r_particle_tracelimit;
extern cvar_t r_part_sparks;
extern cvar_t r_part_sparks_trifan;
extern cvar_t r_part_sparks_textured;
extern cvar_t r_part_beams;
extern cvar_t r_part_contentswitch;
extern cvar_t r_part_density;
extern cvar_t r_part_maxparticles;
extern cvar_t r_part_maxdecals;

pscript_type_t *PScript_GetParticleType(const char *config, const char *name);
int PScript_AllocateParticleType(const char *config, const char *name);
void PScript_LoadTexture(pscript_type_t *ptype);
void PScript_PartRedirectCmd(tokenizecontext_t *ctx, qbool fromset);
void PScript_AssociateEffectCmd(tokenizecontext_t *ctx, const char *config);
void PScript_LinkRunList(pscript_type_t *ptype);
void PScript_UnlinkRunList(pscript_type_t *ptype);

// r_part_script_parse.c
void PScript_ResetToDefaults(pscript_type_t *ptype);
char *PScript_ReadLine(char *buffer, size_t buffersize, const char *filedata, size_t filesize, size_t *offset);
void PScript_ParseParticleEffectFile(const char *config, qbool weak, const char *data, size_t size);
void PScript_FinishParticleType(pscript_type_t *ptype);

// r_part_script_effectinfo.c
qbool PScript_ImportEffectInfo(const char *config);

// r_part_script_spawn.c
void PScript_EffectSpawned(pscript_type_t *ptype, vec3_t org, int dlkey);
void PScript_ForgetMissingSounds(void);
pscript_particle_t *PScript_SpawnParticle(pscript_type_t *ptype, int cidxoffset);
qbool PScript_RunEffect(vec3_t org, vec3_t dir, float count, int typenum, trailkey_t *tk);
qbool PScript_RunAgedEffect(vec3_t org, vec3_t dir, float count, int typenum, trailkey_t *tk, float agemin, float agerange);
qbool PScript_RunTrail(vec3_t start, vec3_t end, int typenum, float timeinterval, int dlkey, trailkey_t *tk);
pscript_beamseg_t *PScript_AppendBeamSeg(pscript_beamseg_t **first, pscript_beamseg_t *last, pscript_particle_t *p, float texture_s, const vec3_t dir);
qbool PScript_ContentsAllow(const pscript_type_t *ptype, vec3_t org);
pscript_type_t *PScript_InwaterType(pscript_type_t *ptype, vec3_t org);
void PScript_WorldSpaceRandom(const pscript_type_t *ptype, pscript_particle_t *p);
float PScript_BoundCount(float count);
void PScript_SpawnClippedDecal(pscript_type_t *ptype, vec3_t center, vec3_t normal, float size, qbool randomslot, int entnum);
float PScript_TraceLine(vec3_t start, vec3_t end, vec3_t impact, vec3_t normal, int *entnum);
unsigned int PScript_PointContents(vec3_t p);
void PScript_RebuildTraceEntities(void);

// r_part_script_trail.c
pscript_trailstate_t *PScript_FetchTrailstate(trailkey_t *tk);
pscript_trailstate_t *PScript_FetchHeadTrailstate(trailkey_t *tk, int typenum);
void PScript_AllocTrailstates(void);
void PScript_ResetTrailstates(void);
void PScript_FreeTrailstates(void);
void PScript_VectorVectors(const vec3_t forward, vec3_t right, vec3_t up);

// r_part_script_update.c
extern pscript_particle_t *pscript_free_particles;
extern pscript_beamseg_t *pscript_free_beams;
extern pscript_clippeddecal_t *pscript_free_decals;
extern double pscript_particletime;
extern float pscript_sintable[256];

void PScript_InitSinTable(void);
int PScript_SinIndex(double radians);
void PScript_StartupPools(void);
void PScript_ResetPools(void);
void PScript_FreePools(void);
void PScript_ReleaseTypeInstances(pscript_type_t *ptype);
void PScript_CountInstances(const pscript_type_t *ptype, pscript_counts_t *counts);
void PScript_CountFree(pscript_counts_t *counts);
void PScript_Simulate(float dt);
void PScript_BuildGeometry(void);
void PScript_MarkFrameParticles(void);
void PScript_ExpireFrameParticles(void);
void PScript_ApplyRamp(const pscript_type_t *ptype, float die, float dt, pscript_vec4_t rgba, float *scale);

// r_part_script_decal.c
struct model_s;
struct msurface_s;
int PScript_SurfacePolygon(const struct model_s *mod, const struct msurface_s *surf, vec3_t *verts, int maxverts);
void PScript_ClipDecal(struct model_s *mod, vec3_t center, vec3_t normal, vec3_t tangent1, vec3_t tangent2, float size, unsigned int surfflagmask, unsigned int surfflagmatch, pscript_decalcallback_t callback, void *ctx);

// r_part_script_surface.c
void PScript_ClearAllSurfaceParticles(void);
void PScript_FreeSurfaceLeaves(void);
void PScript_ForgetSurfaceLeaves(void);
void PScript_RebuildSurfaceEmitters(void);
void PScript_AddSurfaceView(void);
void PScript_RunSurfaceEmitters(void);

// r_part.c
extern cvar_t r_drawparticles;

#endif // EZQUAKE_R_PART_SCRIPT_LOCAL_H
