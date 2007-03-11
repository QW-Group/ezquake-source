//VULTUREIIC

#include "quakedef.h"
#include "gl_model.h"
#include "vx_stuff.h"


#define	MAX_BLURS	128

void TrailStats (int change);
typedef struct 
{
	vec3_t	origin;
	vec3_t angles;
	float alpha;
	float fade;
	model_t	*model;
	int frame;
	int oldframe;
	struct player_info_s	*scoreboard;	// identify player
	float start;
	int skinnum;
} motionblur_t;

motionblur_t	cl_blurs[MAX_BLURS];


//fix this so it doesn't need to use entities later
void CL_UpdateBlurs (void) 
{
	int			i;
	motionblur_t	*blur;
	entity_t	ent;

	memset (&ent, 0, sizeof(entity_t));
	TrailStats (-MotionBlurCount);
	MotionBlurCount = 0;
	for (i=0, blur=cl_blurs ; i< MAX_BLURS ; i++, blur++)	
	{
		if (!blur->model || blur->alpha <= 0)
			continue;
		if (blur->alpha <= 0) 
		{
			blur->model = NULL;
			continue;
		}

		TrailStats (1);
		VectorCopy (blur->origin, ent.origin);
		ent.model = blur->model;
		ent.frame = blur->frame;
		ent.alpha = blur->alpha;
		ent.oldframe = blur->oldframe;
		if (!ISPAUSED)
			blur->alpha -= cls.frametime*blur->fade;
		ent.scoreboard = blur->scoreboard;
		VectorCopy(blur->angles, ent.angles);
		ent.skinnum = blur->skinnum;
		CL_AddEntity (&ent);
	}
}

motionblur_t *CL_AllocBlur (void) 
{
	int		i;
	float	time;
	int		index;
	
	for (i=0 ; i<MAX_BLURS ; i++)
		if (!cl_blurs[i].model)
			return &cl_blurs[i];

	time = cl.time;
	index = 0;

	for (i=0 ; i<MAX_BLURS ; i++)
		if (cl_blurs[i].start < time) 
		{
			time = cl_blurs[i].start;
			index = i;
		}
	return &cl_blurs[index];
}
void CL_CreateBlurs (vec3_t start, vec3_t end, entity_t *ent)
{
	vec3_t		vec;//, impact, normal;
	float		len, maxlen;
	motionblur_t	*blur;

	if (ISPAUSED)
		return;
/*	CL_TraceLine (r_refdef.vieworg, start, impact, normal);
		if (!VectorCompare(impact, start))//Can't see it, so make it fade out(faster)
			return;*/

	if (VectorCompare(start, end)) //Its not moving, so just add a single motion trail
	{
		blur = CL_AllocBlur();
		if (!blur)
			return;

		VectorCopy(start, blur->origin);
		VectorCopy(ent->angles, blur->angles);
		blur->model = ent->model;
		blur->frame = ent->frame;
		blur->oldframe = ent->oldframe;
		blur->scoreboard = ent->scoreboard;
		blur->start = cl.time;
		blur->alpha = 1;
		blur->fade = 4.5;
		blur->skinnum = ent->skinnum;

		return;
	}
	VectorSubtract (end, start, vec);
	len = VectorNormalize(vec);
	maxlen = len;

	while (len > 0) //its on the move! add a trail of trails!
	{
		blur = CL_AllocBlur();
		if (!blur)
			return;
	
		VectorCopy(start, blur->origin);
		VectorCopy(ent->angles, blur->angles);
		blur->model = ent->model;
		blur->frame = ent->frame;
		blur->oldframe = ent->oldframe;
		blur->scoreboard = ent->scoreboard;
		blur->start = cl.time;
		blur->alpha = len/maxlen;
		blur->fade = 6;
		blur->skinnum = ent->skinnum;
		len=-2;
		VectorAdd (start, vec, start);
	}
}

void TrailStats (int change)
{
	if (MotionBlurCount > MotionBlurCountHigh)
		MotionBlurCountHigh = MotionBlurCount;
	MotionBlurCount+=change;
}

void CL_ClearBlurs (void) 
{
	int			i, j;
	motionblur_t	*blur;

	MotionBlurCount = 0;
	for (i=0, blur=cl_blurs ; i< MAX_BLURS ; i++, blur++)	
	{
		blur->model = NULL;
		for (j=0;j<3;j++)
			blur->angles[j] = 0;
		blur->fade = 0;
		blur->frame = 0;
		blur->oldframe = 0;
		for (j=0;j<3;j++)
			blur->origin[j] = 0;
		blur->scoreboard = 0;
		blur->skinnum = 0;
		blur->start = 0;
	}
}
