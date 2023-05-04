
#include "gl_model.h"

#define	MAX_BEAMS 32
typedef struct
{
	int entity;
	model_t *model;
	float endtime;
	vec3_t start, end;
} beam_t;

#define MAX_EXPLOSIONS 32
typedef struct explosion_s
{
	struct explosion_s *prev, *next;
	vec3_t origin;
	float start;
	model_t *model;
} explosion_t;

#define	MAX_FAKEPROJ 96
typedef struct
{
	int modelindex, dl_key;
	int type, effects, flags, partcount, index;
	float starttime, endtime, parttime;
	vec3_t start, vel, avel, angs, org, partorg;
	centity_t cent;
	centity_trail_t trails[4];
#ifdef MVD_PEXT1_SIMPLEPROJECTILE
	int entnum;
	int	owner;
#endif
} fproj_t;

void Fproj_Physics_Bounce(fproj_t *proj, float dt);
void CL_MatchFakeProjectile(centity_t *cent);
void CL_CreateBeam(int type, int ent, vec3_t start, vec3_t end);
fproj_t *CL_AllocFakeProjectile(void);
fproj_t *CL_CreateFakeNail(void);
fproj_t *CL_CreateFakeSuperNail(void);
fproj_t *CL_CreateFakeGrenade(void);
fproj_t *CL_CreateFakeRocket(void);

#define MAX_PREDEXPLOSIONS 16
typedef struct
{
	float time;
	vec3_t origin;
	float radius;
	int damage;
} predexplosion_t;
void CL_CheckPredictedExplosions(player_state_t *from, player_state_t *to);

#ifdef MVD_PEXT1_SIMPLEPROJECTILE
typedef struct
{
	int active;

	int fproj_number;
	int owner;
	vec3_t origin;
	vec3_t velocity;
	vec3_t angles;
	int modelindex;
	float time_offset;

} cs_sprojectile_t;
#endif

//r_part_trails.c
extern void R_MissileTrail(centity_t *cent, int trail_num);
extern int fix_trail_num_for_grens(int trail_num);
