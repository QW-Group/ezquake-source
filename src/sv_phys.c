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
// sv_phys.c

#ifndef CLIENTONLY
#include "qwsvdef.h"

/*


pushmove objects do not obey gravity, and do not interact with each other or trigger fields, but block normal movement and push normal objects when they move.

onground is set for toss objects when they come to a complete rest.  it is set for steping or walking objects 

doors, plats, etc are SOLID_BSP, and MOVETYPE_PUSH
bonus items are SOLID_TRIGGER touch, and MOVETYPE_TOSS
corpses are SOLID_NOT and MOVETYPE_TOSS
crates are SOLID_BBOX and MOVETYPE_TOSS
walking monsters are SOLID_SLIDEBOX and MOVETYPE_STEP
flying/floating monsters are SOLID_SLIDEBOX and MOVETYPE_FLY

solid_edge items only clip against bsp models.

*/

cvar_t	sv_maxvelocity		= { "sv_maxvelocity","2000"};

cvar_t	sv_gravity		= { "sv_gravity", "800"};
cvar_t	sv_stopspeed		= { "sv_stopspeed", "100"};
cvar_t	sv_maxspeed		= { "sv_maxspeed", "320"};
cvar_t	sv_spectatormaxspeed 	= { "sv_spectatormaxspeed", "500"};
cvar_t	sv_accelerate		= { "sv_accelerate", "10"};
cvar_t	sv_airaccelerate	= { "sv_airaccelerate", "10"};

cvar_t	sv_antilag		= { "sv_antilag", "", CVAR_SERVERINFO};
cvar_t	sv_antilag_no_pred	= { "sv_antilag_no_pred", "", CVAR_SERVERINFO}; // "negative" cvar so it doesn't show on serverinfo for no reason
cvar_t	sv_antilag_projectiles	= { "sv_antilag_projectiles", "", CVAR_SERVERINFO};

cvar_t	sv_wateraccelerate	= { "sv_wateraccelerate", "10"};
cvar_t	sv_friction		= { "sv_friction", "4"};
cvar_t	sv_waterfriction	= { "sv_waterfriction", "4"};
cvar_t	pm_ktjump		= { "pm_ktjump", "1", CVAR_SERVERINFO};
cvar_t	pm_bunnyspeedcap	= { "pm_bunnyspeedcap", "", CVAR_SERVERINFO};
cvar_t	pm_slidefix		= { "pm_slidefix", "", CVAR_SERVERINFO};
void OnChange_pm_airstep (cvar_t *var, char *value, qbool *cancel);
cvar_t	pm_airstep		= { "pm_airstep", "", CVAR_SERVERINFO, OnChange_pm_airstep};
cvar_t	pm_pground		= { "pm_pground", "", CVAR_SERVERINFO|CVAR_ROM};
cvar_t  pm_rampjump     = { "pm_rampjump", "", CVAR_SERVERINFO };

double	sv_frametime;


// when pm_airstep is 1, set pm_pground to 1, and vice versa
// airstep works best with pground on
void OnChange_pm_airstep (cvar_t *var, char *value, qbool *cancel)
{
	float val = Q_atoi(value);
	Cvar_SetROM (&pm_pground, val ? "1" : "");
}


void SV_Physics_Toss (edict_t *ent);


/*
================
SV_CheckVelocity
================
*/
void SV_CheckVelocity (edict_t *ent)
{
	int i;
	float wishspeed;

	//
	// bound velocity
	//
	for (i=0 ; i<3 ; i++)
	{
		if (IS_NAN(ent->v->velocity[i]))
		{
			Con_DPrintf ("Got a NaN velocity on %s\n", PR_GetEntityString(ent->v->classname));
			ent->v->velocity[i] = 0;
		}
		if (IS_NAN(ent->v->origin[i]))
		{
			Con_DPrintf ("Got a NaN origin on %s\n", PR_GetEntityString(ent->v->classname));
			ent->v->origin[i] = 0;
		}
/*		if (ent->v->velocity[i] > sv_maxvelocity.value)
			ent->v->velocity[i] = sv_maxvelocity.value;
		else if (ent->v->velocity[i] < -sv_maxvelocity.value)
			ent->v->velocity[i] = -sv_maxvelocity.value;
*/
	}

	// SV_MAXVELOCITY fix by Maddes
	wishspeed = VectorLength(ent->v->velocity);
	if (wishspeed > sv_maxvelocity.value)
	{
		VectorScale (ent->v->velocity, sv_maxvelocity.value/wishspeed, ent->v->velocity);
		wishspeed = sv_maxvelocity.value;
	}
}

/*
=============
SV_RunThink

Runs thinking code if time.  There is some play in the exact time the think
function will be called, because it is called before any movement is done
in a frame.  Not used for pushmove objects, because they must be exact.
Returns false if the entity removed itself.
=============
*/
qbool SV_RunThink (edict_t *ent)
{
	float thinktime;

	do
	{
		thinktime = ent->v->nextthink;
		if (thinktime <= 0)
			return true;
		if (thinktime > sv.time + sv_frametime)
			return true;

		if (thinktime < sv.time)
			thinktime = sv.time; // don't let things stay in the past.
		// it is possible to start that way
		// by a trigger with a local time.
		ent->v->nextthink = 0;
		pr_global_struct->time = thinktime;
		pr_global_struct->self = EDICT_TO_PROG(ent);
		pr_global_struct->other = EDICT_TO_PROG(sv.edicts);
		PR_EdictThink(ent->v->think);

		if (ent->e.free)
			return false;
	} while (1);

	return true;
}

/*
==================
SV_Impact

Two entities have touched, so run their touch functions
==================
*/
void SV_Impact (edict_t *e1, edict_t *e2)
{
	int old_self, old_other;

	old_self = pr_global_struct->self;
	old_other = pr_global_struct->other;

	pr_global_struct->time = sv.time;
	if (e1->v->touch && e1->v->solid != SOLID_NOT)
	{
		pr_global_struct->self = EDICT_TO_PROG(e1);
		pr_global_struct->other = EDICT_TO_PROG(e2);
		PR_EdictTouch(e1->v->touch);
	}

	if (e2->v->touch && e2->v->solid != SOLID_NOT)
	{
		pr_global_struct->self = EDICT_TO_PROG(e2);
		pr_global_struct->other = EDICT_TO_PROG(e1);
		PR_EdictTouch(e2->v->touch);
	}

	pr_global_struct->self = old_self;
	pr_global_struct->other = old_other;
}


/*
==================
ClipVelocity

Slide off of the impacting object
==================
*/
#define	STOP_EPSILON	0.1

void ClipVelocity (vec3_t in, vec3_t normal, vec3_t out, float overbounce)
{
	float backoff;
	float change;
	int i;


	backoff = DotProduct (in, normal) * overbounce;

	for (i=0 ; i<3 ; i++)
	{
		change = normal[i]*backoff;
		out[i] = in[i] - change;
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
	}
}


/*
============
SV_FlyMove

The basic solid body movement clip that slides along multiple planes
Returns the clipflags if the velocity was modified (hit something solid)
1 = floor
2 = wall / step
4 = dead stop
If steptrace is not NULL, the trace of any vertical wall hit will be stored
============
*/
#define MAX_CLIP_PLANES	5
int SV_FlyMove (edict_t *ent, float time1, trace_t *steptrace, int type)
{
	int		bumpcount, numbumps;
	vec3_t	dir;
	float	d;
	int		numplanes;
	vec3_t	planes[MAX_CLIP_PLANES];
	vec3_t	primal_velocity, original_velocity, new_velocity;
	int		i, j;
	trace_t	trace;
	vec3_t	end;
	float	time_left;
	int		blocked;

	numbumps = 4;

	blocked = 0;
	VectorCopy (ent->v->velocity, original_velocity);
	VectorCopy (ent->v->velocity, primal_velocity);
	numplanes = 0;

	time_left = time1;

	for (bumpcount=0 ; bumpcount<numbumps ; bumpcount++)
	{
		for (i=0 ; i<3 ; i++)
			end[i] = ent->v->origin[i] + time_left * ent->v->velocity[i];

		trace = SV_Trace (ent->v->origin, ent->v->mins, ent->v->maxs, end, type, ent);

		if (trace.allsolid)
		{	// entity is trapped in another solid
			VectorClear (ent->v->velocity);
			return 3;
		}

		if (trace.fraction > 0)
		{	// actually covered some distance
			VectorCopy (trace.endpos, ent->v->origin);
			VectorCopy (ent->v->velocity, original_velocity);
			numplanes = 0;
		}

		if (trace.fraction == 1)
			break; // moved the entire distance

		if (!trace.e.ent)
			SV_Error ("SV_FlyMove: !trace.e.ent");

		if (trace.plane.normal[2] > 0.7)
		{
			blocked |= 1; // floor
			if (trace.e.ent->v->solid == SOLID_BSP)
			{
				ent->v->flags = (int)ent->v->flags | FL_ONGROUND;
				ent->v->groundentity = EDICT_TO_PROG(trace.e.ent);
			}
		}
		if (!trace.plane.normal[2])
		{
			blocked |= 2; // step
			if (steptrace)
				*steptrace = trace; // save for player extrafriction
		}

		//
		// run the impact function
		//
		SV_Impact (ent, trace.e.ent);
		if (ent->e.free)
			break;	 // removed by the impact function


		time_left -= time_left * trace.fraction;

		// cliped to another plane
		if (numplanes >= MAX_CLIP_PLANES)
		{	// this shouldn't really happen
			VectorClear (ent->v->velocity);
			return 3;
		}

		VectorCopy (trace.plane.normal, planes[numplanes]);
		numplanes++;

		//
		// modify original_velocity so it parallels all of the clip planes
		//
		for (i=0 ; i<numplanes ; i++)
		{
			ClipVelocity (original_velocity, planes[i], new_velocity, 1);
			for (j=0 ; j<numplanes ; j++)
				if (j != i)
				{
					if (DotProduct (new_velocity, planes[j]) < 0)
						break; // not ok
				}
			if (j == numplanes)
				break;
		}

		if (i != numplanes)
		{	// go along this plane
			VectorCopy (new_velocity, ent->v->velocity);
		}
		else
		{	// go along the crease
			if (numplanes != 2)
			{
				// Con_Printf ("clip velocity, numplanes == %i\n",numplanes);
				VectorClear (ent->v->velocity);
				return 7;
			}
			CrossProduct (planes[0], planes[1], dir);
			d = DotProduct (dir, ent->v->velocity);
			VectorScale (dir, d, ent->v->velocity);
		}

		//
		// if original velocity is against the original velocity, stop dead
		// to avoid tiny occilations in sloping corners
		//
		if (DotProduct (ent->v->velocity, primal_velocity) <= 0)
		{
			VectorClear (ent->v->velocity);
			return blocked;
		}
	}

	return blocked;
}


/*
============
SV_AddGravity
============
*/
void SV_AddGravity (edict_t *ent, float scale)
{
	ent->v->velocity[2] -= scale * movevars.gravity * sv_frametime;
}

/*
===============================================================================

PUSHMOVE

===============================================================================
*/

/*
============
SV_PushEntity

Does not change the entities velocity at all
============
*/
trace_t SV_PushEntity (edict_t *ent, vec3_t push, unsigned int traceflags)
{
	trace_t	trace;
	vec3_t	end;

	VectorAdd (ent->v->origin, push, end);

	if ((int)ent->v->flags&FL_LAGGEDMOVE)
		traceflags |= MOVE_LAGGED;

	if (ent->v->movetype == MOVETYPE_FLYMISSILE)
		trace = SV_Trace (ent->v->origin, ent->v->mins, ent->v->maxs, end, MOVE_MISSILE|traceflags, ent);
	else if (ent->v->solid == SOLID_TRIGGER || ent->v->solid == SOLID_NOT)
		// only clip against bmodels
		trace = SV_Trace (ent->v->origin, ent->v->mins, ent->v->maxs, end, MOVE_NOMONSTERS|traceflags, ent);
	else
		trace = SV_Trace (ent->v->origin, ent->v->mins, ent->v->maxs, end, MOVE_NORMAL|traceflags, ent);

	VectorCopy (trace.endpos, ent->v->origin);
	SV_LinkEdict (ent, true);

	if (trace.e.ent)
		SV_Impact (ent, trace.e.ent);

	return trace;
}


/*
============
SV_Push

============
*/
qbool SV_Push (edict_t *pusher, vec3_t move)
{
	int			i, e;
	edict_t		*check, *block;
	vec3_t		mins, maxs;
	vec3_t		pushorig;
	int			num_moved;
	edict_t		*moved_edict[MAX_EDICTS];
	vec3_t		moved_from[MAX_EDICTS];
	float		solid_save;

	for (i=0 ; i<3 ; i++)
	{
		mins[i] = pusher->v->absmin[i] + move[i];
		maxs[i] = pusher->v->absmax[i] + move[i];
	}

	VectorCopy (pusher->v->origin, pushorig);

	// move the pusher to its final position

	VectorAdd (pusher->v->origin, move, pusher->v->origin);
	SV_LinkEdict (pusher, false);

	// see if any solid entities are inside the final position
	num_moved = 0;
	check = NEXT_EDICT(sv.edicts);
	for (e=1 ; e<sv.num_edicts ; e++, check = NEXT_EDICT(check))
	{
		if (check->e.free)
			continue;
		if (check->v->movetype == MOVETYPE_PUSH
		|| check->v->movetype == MOVETYPE_NONE
		|| check->v->movetype == MOVETYPE_NOCLIP)
			continue;

		solid_save = pusher->v->solid;
		pusher->v->solid = SOLID_NOT;
		block = SV_TestEntityPosition (check);
		pusher->v->solid = solid_save;
		if (block)
			continue;

		// if the entity is standing on the pusher, it will definately be moved
		if ( ! ( ((int)check->v->flags & FL_ONGROUND)
		&& PROG_TO_EDICT(check->v->groundentity) == pusher) )
		{
			if ( check->v->absmin[0] >= maxs[0]
			|| check->v->absmin[1] >= maxs[1]
			|| check->v->absmin[2] >= maxs[2]
			|| check->v->absmax[0] <= mins[0]
			|| check->v->absmax[1] <= mins[1]
			|| check->v->absmax[2] <= mins[2] )
				continue;

			// see if the ent's bbox is inside the pusher's final position
			if (!SV_TestEntityPosition (check))
				continue;
		}

		// remove the onground flag for non-players
		if (check->v->movetype != MOVETYPE_WALK)
			check->v->flags = (int)check->v->flags & ~FL_ONGROUND;

		VectorCopy (check->v->origin, moved_from[num_moved]);
		moved_edict[num_moved] = check;
		num_moved++;

		// try moving the contacted entity
		VectorAdd (check->v->origin, move, check->v->origin);
		block = SV_TestEntityPosition (check);
		if (!block)
		{	// pushed ok
			SV_LinkEdict (check, false);
			continue;
		}

		// if it is ok to leave in the old position, do it
		VectorSubtract (check->v->origin, move, check->v->origin);
		block = SV_TestEntityPosition (check);
		if (!block)
		{
			//if leaving it where it was, allow it to drop to the floor again (useful for plats that move downward)
			//check->v->flags = (int)check->v->flags & ~FL_ONGROUND; // disconnect: is it needed?

			num_moved--;
			continue;
		}

		// if it is still inside the pusher, block
		if (check->v->mins[0] == check->v->maxs[0])
		{
			SV_LinkEdict (check, false);
			continue;
		}
		if (check->v->solid == SOLID_NOT || check->v->solid == SOLID_TRIGGER)
		{	// corpse
			check->v->mins[0] = check->v->mins[1] = 0;
			VectorCopy (check->v->mins, check->v->maxs);
			SV_LinkEdict (check, false);
			continue;
		}

		VectorCopy (pushorig, pusher->v->origin);
		SV_LinkEdict (pusher, false);

		// if the pusher has a "blocked" function, call it
		// otherwise, just stay in place until the obstacle is gone
		if (pusher->v->blocked)
		{
			pr_global_struct->self = EDICT_TO_PROG(pusher);
			pr_global_struct->other = EDICT_TO_PROG(check);
			PR_EdictBlocked (pusher->v->blocked);
		}

		// move back any entities we already moved
		for (i=0 ; i<num_moved ; i++)
		{
			VectorCopy (moved_from[i], moved_edict[i]->v->origin);
			SV_LinkEdict (moved_edict[i], false);
		}
		return false;
	}

	return true;
}

/*
============
SV_PushMove

============
*/
void SV_PushMove (edict_t *pusher, float movetime)
{
	int i;
	vec3_t move;

	if (!pusher->v->velocity[0] && !pusher->v->velocity[1] && !pusher->v->velocity[2])
	{
		pusher->v->ltime += movetime;
		return;
	}

	for (i=0 ; i<3 ; i++)
		move[i] = pusher->v->velocity[i] * movetime;

	if (SV_Push (pusher, move))
		pusher->v->ltime += movetime;
}


/*
================
SV_Physics_Pusher

================
*/
void SV_Physics_Pusher (edict_t *ent)
{
	float thinktime;
	float oldltime;
	float movetime;
	float l;
	vec3_t oldorg, move;

	oldltime = ent->v->ltime;

	thinktime = ent->v->nextthink;
	if (thinktime < ent->v->ltime + sv_frametime)
	{
		movetime = thinktime - ent->v->ltime;
		if (movetime < 0)
			movetime = 0;
	}
	else
		movetime = sv_frametime;

	if (movetime)
	{
		SV_PushMove (ent, movetime);	// advances ent->v->ltime if not blocked
	}

	if (thinktime > oldltime && thinktime <= ent->v->ltime)
	{
		VectorCopy (ent->v->origin, oldorg);
		ent->v->nextthink = 0;
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(ent);
		pr_global_struct->other = EDICT_TO_PROG(sv.edicts);
		PR_EdictThink(ent->v->think);

		if (ent->e.free)
			return;
		VectorSubtract (ent->v->origin, oldorg, move);

		l = VectorLength(move);
		if (l > 1.0/64)
		{
			// Con_Printf ("**** snap: %f\n", VectorLength (l));
			VectorCopy (oldorg, ent->v->origin);
			SV_Push (ent, move);
		}
	}
}

/*
=============
SV_Physics_None

Non moving objects can only think
=============
*/
void SV_Physics_None (edict_t *ent)
{
	// regular thinking
	SV_RunThink (ent);
}

/*
=============
SV_Physics_Noclip

A moving object that doesn't obey physics
=============
*/
void SV_Physics_Noclip (edict_t *ent)
{
	// regular thinking
	if (!SV_RunThink (ent))
		return;

	VectorMA (ent->v->angles, sv_frametime, ent->v->avelocity, ent->v->angles);
	VectorMA (ent->v->origin, sv_frametime, ent->v->velocity, ent->v->origin);

	SV_LinkEdict (ent, false);
}

/*
==============================================================================

TOSS / BOUNCE

==============================================================================
*/

/*
=============
SV_CheckWaterTransition

=============
*/
void SV_CheckWaterTransition (edict_t *ent)
{
	int cont;

	cont = SV_PointContents (ent->v->origin);
	if (!ent->v->watertype)
	{	// just spawned here
		ent->v->watertype = cont;
		ent->v->waterlevel = 1;
		return;
	}

	if (cont <= CONTENTS_WATER)
	{
		if (ent->v->watertype == CONTENTS_EMPTY)
		{	// just crossed into water
			SV_StartSound (ent, 0, "misc/h2ohit1.wav", 255, 1);
		}
		ent->v->watertype = cont;
		ent->v->waterlevel = 1;
	}
	else
	{
		if (ent->v->watertype != CONTENTS_EMPTY)
		{	// just crossed into water
			SV_StartSound (ent, 0, "misc/h2ohit1.wav", 255, 1);
		}
		ent->v->watertype = CONTENTS_EMPTY;
		ent->v->waterlevel = cont;
	}
}

/*
=============
SV_Physics_Toss

Toss, bounce, and fly movement.  When onground, do nothing.
=============
*/
void SV_Physics_Toss (edict_t *ent)
{
	trace_t	trace;
	vec3_t	move;
	float	backoff;

	// regular thinking
	if (!SV_RunThink (ent))
		return;

	if (ent->v->velocity[2] > 0)
		ent->v->flags = (int)ent->v->flags & ~FL_ONGROUND;

	// if onground, return without moving
	if ( ((int)ent->v->flags & FL_ONGROUND) )
		return;

	SV_CheckVelocity (ent);

	// add gravity
	if (ent->v->movetype != MOVETYPE_FLY
	&& ent->v->movetype != MOVETYPE_FLYMISSILE)
		SV_AddGravity (ent, 1.0);

	// move angles
	VectorMA (ent->v->angles, sv_frametime, ent->v->avelocity, ent->v->angles);

	// move origin
	VectorScale (ent->v->velocity, sv_frametime, move);
	trace = SV_PushEntity (ent, move, (sv_antilag.value == 2 && sv_antilag_projectiles.value) ? MOVE_LAGGED:0);
	if (trace.fraction == 1)
		return;
	if (ent->e.free)
		return;

	if (ent->v->movetype == MOVETYPE_BOUNCE)
		backoff = 1.5;
	else
		backoff = 1;

	ClipVelocity (ent->v->velocity, trace.plane.normal, ent->v->velocity, backoff);

	// stop if on ground
	if (trace.plane.normal[2] > 0.7)
	{
		if (ent->v->velocity[2] < 60 || ent->v->movetype != MOVETYPE_BOUNCE )
		{
			ent->v->flags = (int)ent->v->flags | FL_ONGROUND;
			ent->v->groundentity = EDICT_TO_PROG(trace.e.ent);
			VectorClear (ent->v->velocity);
			VectorClear (ent->v->avelocity);
		}
	}

	// check for in water
	SV_CheckWaterTransition (ent);
}

/*
===============================================================================

STEPPING MOVEMENT

===============================================================================
*/

/*
=============
SV_Physics_Step

Monsters freefall when they don't have a ground entity, otherwise
all movement is done with discrete steps.

This is also used for objects that have become still on the ground, but
will fall if the floor is pulled out from under them.
FIXME: is this true?
=============
*/
void SV_Physics_Step (edict_t *ent)
{
	qbool hitsound;

	// frefall if not onground
	if ( ! ((int)ent->v->flags & (FL_ONGROUND | FL_FLY | FL_SWIM) ) )
	{
		if (ent->v->velocity[2] < movevars.gravity*-0.1)
			hitsound = true;
		else
			hitsound = false;

		SV_AddGravity (ent, 1.0);
		SV_CheckVelocity (ent);
		// Tonik: the check for SOLID_NOT is to fix the way dead bodies and
		// gibs behave (should not be blocked by players & monsters);
		// The SOLID_TRIGGER check is disabled lest we break frikbots
		if (ent->v->solid == SOLID_NOT /* || ent->v->solid == SOLID_TRIGGER*/)
			SV_FlyMove (ent, sv_frametime, NULL, MOVE_NOMONSTERS);
		else
			SV_FlyMove (ent, sv_frametime, NULL, MOVE_NORMAL);
		SV_LinkEdict (ent, true);

		if ( (int)ent->v->flags & FL_ONGROUND ) // just hit ground
		{
			if (hitsound)
				SV_StartSound (ent, 0, "demon/dland2.wav", 255, 1);
		}
	}

	// regular thinking
	SV_RunThink (ent);

	SV_CheckWaterTransition (ent);
}

//============================================================================

void SV_ProgStartFrame (qbool isBotFrame)
{
	// let the progs know that a new frame has started
	pr_global_struct->self = EDICT_TO_PROG(sv.edicts);
	pr_global_struct->other = EDICT_TO_PROG(sv.edicts);
	pr_global_struct->time = sv.time;
	PR_GameStartFrame(isBotFrame);
}

/*
================
SV_RunEntity
================
*/
void SV_RunEntity (edict_t *ent)
{
	if (ent->e.lastruntime == sv.time)
		return;
	ent->e.lastruntime = sv.time;

	switch ((int)ent->v->movetype)
	{
	case MOVETYPE_PUSH:
		SV_Physics_Pusher (ent);
		break;
	case MOVETYPE_NONE:
	case MOVETYPE_LOCK:
		SV_Physics_None (ent);
		break;
	case MOVETYPE_NOCLIP:
		SV_Physics_Noclip (ent);
		break;
	case MOVETYPE_STEP:
		SV_Physics_Step (ent);
		break;
	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
	case MOVETYPE_FLY:
	case MOVETYPE_FLYMISSILE:
		SV_Physics_Toss (ent);
		break;
	default:
		SV_Error ("SV_Physics: bad movetype %i", (int)ent->v->movetype);
	}
}

/*
** SV_RunNQNewmis
** 
** sv_player will be valid
*/
void SV_RunNQNewmis (void)
{
	edict_t	*ent;
	double save_frametime;
	int i, pl;

	pl = EDICT_TO_PROG(sv_player);
	ent = NEXT_EDICT(sv.edicts);
	for (i=1 ; i<sv.num_edicts ; i++, ent = NEXT_EDICT(ent))
	{
		if (ent->e.free)
			continue;
		if (ent->e.lastruntime || ent->v->owner != pl)
			continue;
		if (ent->v->movetype != MOVETYPE_FLY &&
			ent->v->movetype != MOVETYPE_FLYMISSILE && 
			ent->v->movetype != MOVETYPE_BOUNCE) 
			continue;
		if (ent->v->solid != SOLID_BBOX && ent->v->solid != SOLID_TRIGGER)
			continue;

		save_frametime = sv_frametime;
		sv_frametime = 0.05;
		SV_RunEntity (ent);
		sv_frametime = save_frametime;
		return;
	}
}

/*
================
SV_RunNewmis
================
*/
void SV_RunNewmis (void)
{
	edict_t	*ent;
	double save_frametime;

	if (pr_nqprogs)
		return;

	if (!pr_global_struct->newmis)
		return;

	ent = PROG_TO_EDICT(pr_global_struct->newmis);
	pr_global_struct->newmis = 0;

	save_frametime = sv_frametime;
	sv_frametime = 0.05;

	SV_RunEntity (ent);

	sv_frametime = save_frametime;
}

/*
================
SV_Physics
================
*/
void SV_Physics (void)
{
	int i;
	client_t *cl,*savehc;
	edict_t *savesvpl;
	edict_t *ent;

	if (sv.state != ss_active)
		return;

	if (sv.old_time)
	{
		// don't bother running a frame if sv_mintic seconds haven't passed
		sv_frametime = sv.time - sv.old_time;
		if (sv_frametime < (double) sv_mintic.value)
			return;
		if (sv_frametime > (double) sv_maxtic.value)
			sv_frametime = (double) sv_maxtic.value;
		sv.old_time = sv.time;
	}
	else
		sv_frametime = 0.1; // initialization frame

	sv.physicstime = sv.time;

	if (pr_nqprogs)
		NQP_Reset ();

	PR_GLOBAL(frametime) = sv_frametime;

	SV_ProgStartFrame(false);

	//
	// treat each object in turn
	// even the world gets a chance to think
	//
	ent = sv.edicts;
	for (i=0 ; i<sv.num_edicts ; i++, ent = NEXT_EDICT(ent))
	{
		if (ent->e.free)
			continue;

		if (PR_GLOBAL(force_retouch))
			SV_LinkEdict (ent, true);	// force retouch even for stationary

		if (i > 0 && i <= MAX_CLIENTS)
			continue;		// clients are run directly from packets

		SV_RunEntity (ent);
		SV_RunNewmis ();
	}

	if (PR_GLOBAL(force_retouch))
		PR_GLOBAL(force_retouch)--;

	savesvpl = sv_player;
	savehc = sv_client;

	// so spec will have right goalentity - if speccing someone
	for ( i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++ )
	{
		if ( cl->state == cs_free )
			continue;

		sv_client = cl;
		sv_player = cl->edict;

		if (sv_client->spectator && sv_client->spec_track > 0)
			sv_player->v->goalentity = EDICT_TO_PROG(svs.clients[sv_client->spec_track-1].edict);
	}

	sv_player = savesvpl;
	sv_client = savehc;
}

#ifdef USE_PR2
void SV_RunBots(void)
{
	int i;
	client_t *cl,*savehc;
	edict_t *savesvpl;
	double max_physfps = sv_maxfps.value;
#ifdef SERVERONLY
	static double extramsec = 0;
#endif

	if (max_physfps < 20 || max_physfps > 1000) {
		max_physfps = 77.0;
	}

	if (sv.state != ss_active || !sv.physicstime)
		return;

#ifdef SERVERONLY
	if (sv.old_bot_time) {
		// don't bother running a frame if 1/fps seconds haven't passed
		double required = (double) 1.0f / max_physfps;

		extramsec += (sv.time - sv.old_bot_time);
		sv.old_bot_time = sv.time;
		if (extramsec < required) {
			return;
		}
		sv_frametime = required;
		extramsec -= required;
	}
	else {
		sv_frametime = 1.0f / max_physfps; // initialization frame
		extramsec = 0;
		sv.old_bot_time = sv.time;
	}
#else
	// On internal server, try and match the user's framerate
	// ... don't run if no time passed, that is a user packet only
	if (sv.old_bot_time && sv.old_bot_time == sv.time) {
		return;
	}
	sv.old_bot_time = sv.time;
#endif

	savesvpl = sv_player;
	savehc = sv_client;

	PR_GLOBAL(frametime) = sv_frametime;
	SV_ProgStartFrame (true);

	//
	// Run bots physics.
	//
	for ( i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++ )
	{
		extern void SV_PreRunCmd(void);
		extern void SV_RunCmd (usercmd_t *ucmd, qbool inside, qbool simulate);
		extern void SV_PostRunCmd(void);

		if ( cl->state == cs_free )
			continue;
		if ( !cl->isBot )
			continue;

		sv_client = cl;
		sv_player = cl->edict;

		SV_PreRunCmd();
		SV_RunCmd (&cl->botcmd, false, false);
		SV_PostRunCmd();

		cl->lastcmd = cl->botcmd;
		cl->lastcmd.buttons = 0;

		memset(&cl->botcmd,0,sizeof(cl->botcmd));

		cl->localtime = sv.time;
		cl->delta_sequence = -1;	// no delta unless requested

		if (sv_antilag.value) {
			if (cl->antilag_position_next == 0 || cl->antilag_positions[(cl->antilag_position_next - 1) % MAX_ANTILAG_POSITIONS].localtime < cl->localtime) {
				cl->antilag_positions[cl->antilag_position_next % MAX_ANTILAG_POSITIONS].localtime = cl->localtime;
				VectorCopy(cl->edict->v->origin, cl->antilag_positions[cl->antilag_position_next % MAX_ANTILAG_POSITIONS].origin);
				cl->antilag_position_next++;
			}
		}
		else {
			cl->antilag_position_next = 0;
		}
	}

	sv_player = savesvpl;
	sv_client = savehc;
}
#endif

void SV_SetMoveVars(void)
{
	movevars.gravity            = sv_gravity.value;
	movevars.stopspeed          = sv_stopspeed.value;
	movevars.maxspeed           = sv_maxspeed.value;
	movevars.spectatormaxspeed  = sv_spectatormaxspeed.value;
	movevars.accelerate         = sv_accelerate.value;
	movevars.airaccelerate      = sv_airaccelerate.value;
	movevars.wateraccelerate    = sv_wateraccelerate.value;
	movevars.friction           = sv_friction.value;
	movevars.waterfriction      = sv_waterfriction.value;
	movevars.entgravity         = 1.0;
}

#endif // !CLIENTONLY
