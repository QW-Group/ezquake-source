
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

#define	MAX_FAKEPROJ 32
typedef struct
{
	model_t *model;
	int type, effects, flags;
	float starttime, endtime, parttime;
	vec3_t start, vel, avel, angs, org, partorg;
	centity_trail_t trails[4];
} fproj_t;

void Fproj_Physics_Bounce(fproj_t *proj, float dt);
void CL_MatchFakeProjectile(centity_t *cent);
void CL_CreateBeam(int type, int ent, vec3_t start, vec3_t end);
fproj_t *CL_CreateFakeNail(void);
fproj_t *CL_CreateFakeSuperNail(void);
fproj_t *CL_CreateFakeGrenade(void);
fproj_t *CL_CreateFakeRocket(void);

//r_part_trails.c
void R_MissileTrail(centity_t *cent, int trail_num);
int fix_trail_num_for_grens(int trail_num);
