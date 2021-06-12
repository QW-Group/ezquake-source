
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
	int type, effects;
	float starttime, endtime;
	vec3_t start, vel, angs;
} fproj_t;

fproj_t *CL_CreateFakeNail(void);
fproj_t *CL_CreateFakeSuperNail(void);
fproj_t *CL_CreateFakeGrenade(void);
fproj_t *CL_CreateFakeRocket(void);


