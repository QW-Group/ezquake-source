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

#include "qwsvdef.h"
#include "pmove.h"

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

cvar_t	sv_maxvelocity		 = {"sv_maxvelocity","2000"}; 
cvar_t	sv_gravity			 = {"sv_gravity", "800"};
cvar_t	pm_stopspeed		 = {"sv_stopspeed", "100"};
cvar_t	pm_maxspeed			 = {"sv_maxspeed", "320"};
cvar_t	pm_spectatormaxspeed = {"sv_spectatormaxspeed", "500"};
cvar_t	pm_accelerate		 = {"sv_accelerate", "10"};
cvar_t	pm_airaccelerate	 = {"sv_airaccelerate", "10"};
cvar_t	pm_wateraccelerate	 = {"sv_wateraccelerate", "10"};
cvar_t	pm_friction			 = {"sv_friction", "4"};
cvar_t	pm_waterfriction	 = {"sv_waterfriction", "4"};
cvar_t	pm_ktjump			 = {"pm_ktjump", "0.5", CVAR_SERVERINFO};
cvar_t	pm_bunnyspeedcap	 = {"pm_bunnyspeedcap", "", CVAR_SERVERINFO};
cvar_t	pm_slidefix			 = {"pm_slidefix", "", CVAR_SERVERINFO};

double	sv_frametime;


#define	MOVE_EPSILON	0.01

void SV_Physics_Toss (edict_t *ent);

void SV_CheckAllEnts (void) {
	int e;
	edict_t *check;

	// see if any solid entities are inside the final position
	check = NEXT_EDICT(sv.edicts);
	for (e = 1; e < sv.num_edicts; e++, check = NEXT_EDICT(check)) {
		if (check->free)
			continue;
		if (check->v.movetype == MOVETYPE_PUSH || check->v.movetype == MOVETYPE_NONE || check->v.movetype == MOVETYPE_NOCLIP)
			continue;

		if (SV_TestEntityPosition (check))
			Com_Printf ("entity in invalid position\n");
	}
}

void SV_CheckVelocity (edict_t *ent) {
	int i;
	float wishspeed;

	// bound velocity
	for (i = 0; i < 3; i++) {
		if (IS_NAN(ent->v.velocity[i])) {
			Com_DPrintf ("Got a NaN velocity on %s\n", PR_GetString(ent->v.classname));
			ent->v.velocity[i] = 0;
		}
		if (IS_NAN(ent->v.origin[i])) {
			Com_DPrintf ("Got a NaN origin on %s\n", PR_GetString(ent->v.classname));
			ent->v.origin[i] = 0;
		}
	}

	// SV_MAXVELOCITY fix by Maddes
	wishspeed = VectorLength(ent->v.velocity);
	if (wishspeed > sv_maxvelocity.value) {
		VectorScale (ent->v.velocity, sv_maxvelocity.value/wishspeed, ent->v.velocity);
		wishspeed = sv_maxvelocity.value;
	}
}

/*
Runs thinking code if time.  There is some play in the exact time the think function will be called, because 
it is called before any movement is done in a frame.  Not used for pushmove objects, because they must be exact.
Returns false if the entity removed itself.
*/
qboolean SV_RunThink (edict_t *ent) {
	float thinktime;

	do {
		thinktime = ent->v.nextthink;
		if (thinktime <= 0)
			return true;
		if (thinktime > sv.time + sv_frametime)
			return true;
		
		if (thinktime < sv.time)
			thinktime = sv.time;	// don't let things stay in the past.
									// it is possible to start that way
									// by a trigger with a local time.
		ent->v.nextthink = 0;
		pr_global_struct->time = thinktime;
		pr_global_struct->self = EDICT_TO_PROG(ent);
		pr_global_struct->other = EDICT_TO_PROG(sv.edicts);
		PR_ExecuteProgram (ent->v.think);

		if (ent->free)
			return false;
	} while (1);

	return true;
}

//Two entities have touched, so run their touch functions
void SV_Impact (edict_t *e1, edict_t *e2) {
	int old_self, old_other;
	
	old_self = pr_global_struct->self;
	old_other = pr_global_struct->other;
	
	pr_global_struct->time = sv.time;
	if (e1->v.touch && e1->v.solid != SOLID_NOT) {
		pr_global_struct->self = EDICT_TO_PROG(e1);
		pr_global_struct->other = EDICT_TO_PROG(e2);
		PR_ExecuteProgram (e1->v.touch);
	}
	
	if (e2->v.touch && e2->v.solid != SOLID_NOT) {
		pr_global_struct->self = EDICT_TO_PROG(e2);
		pr_global_struct->other = EDICT_TO_PROG(e1);
		PR_ExecuteProgram (e2->v.touch);
	}

	pr_global_struct->self = old_self;
	pr_global_struct->other = old_other;
}

//Slide off of the impacting object
//returns the blocked flags (1 = floor, 2 = step / wall)
#define	STOP_EPSILON	0.1

int ClipVelocity (vec3_t in, vec3_t normal, vec3_t out, float overbounce) {
	float backoff, change;
	int i, blocked;

	blocked = 0;
	if (normal[2] > 0)
		blocked |= 1;		// floor
	if (!normal[2])
		blocked |= 2;		// step

	backoff = DotProduct (in, normal) * overbounce;

	for (i = 0; i < 3; i++) {
		change = normal[i]*backoff;
		out[i] = in[i] - change;
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
	}

	return blocked;
}

/*
The basic solid body movement clip that slides along multiple planes
Returns the clipflags if the velocity was modified (hit something solid)
1 = floor
2 = wall / step
4 = dead stop
If steptrace is not NULL, the trace of any vertical wall hit will be stored
*/
#define	MAX_CLIP_PLANES	5
int SV_FlyMove (edict_t *ent, float time, trace_t *steptrace, int type) {
	int bumpcount, numbumps, numplanes, i, j, blocked;
	float d, time_left;
	vec3_t dir, planes[MAX_CLIP_PLANES], primal_velocity, original_velocity, new_velocity, end;
	trace_t trace;
	
	numbumps = 4;
	
	blocked = 0;
	VectorCopy (ent->v.velocity, original_velocity);
	VectorCopy (ent->v.velocity, primal_velocity);
	numplanes = 0;
	
	time_left = time;

	for (bumpcount = 0; bumpcount < numbumps; bumpcount++) {
		for (i= 0; i < 3; i++)
			end[i] = ent->v.origin[i] + time_left * ent->v.velocity[i];

		trace = SV_Move (ent->v.origin, ent->v.mins, ent->v.maxs, end, type, ent);

		if (trace.allsolid) {	
			// entity is trapped in another solid
			VectorClear (ent->v.velocity);
			return 3;
		}

		if (trace.fraction > 0)	{	
			// actually covered some distance
			VectorCopy (trace.endpos, ent->v.origin);
			VectorCopy (ent->v.velocity, original_velocity);
			numplanes = 0;
		}

		if (trace.fraction == 1)
			 break;		// moved the entire distance

		if (!trace.ent)
			Host_Error ("SV_FlyMove: !trace.ent");

		if (trace.plane.normal[2] > 0.7) {
			blocked |= 1;		// floor
			if (trace.ent->v.solid == SOLID_BSP) {
				ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
				ent->v.groundentity = EDICT_TO_PROG(trace.ent);
			}
		}
		if (!trace.plane.normal[2]) {
			blocked |= 2;		// step
			if (steptrace)
				*steptrace = trace;	// save for player extrafriction
		}

		// run the impact function
		SV_Impact (ent, trace.ent);
		if (ent->free)
			break;		// removed by the impact function

		
		time_left -= time_left * trace.fraction;
		
		// cliped to another plane
		if (numplanes >= MAX_CLIP_PLANES) {	
			// this shouldn't really happen
			VectorClear (ent->v.velocity);
			return 3;
		}

		VectorCopy (trace.plane.normal, planes[numplanes]);
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		for (i = 0; i < numplanes; i++) {
			ClipVelocity (original_velocity, planes[i], new_velocity, 1);
			for (j = 0; j < numplanes; j++)
				if (j != i) {
					if (DotProduct (new_velocity, planes[j]) < 0)
						break;	// not ok
				}
			if (j == numplanes)
				break;
		}
		
		if (i != numplanes) {	
			// go along this plane
			VectorCopy (new_velocity, ent->v.velocity);
		} else {	// go along the crease
			if (numplanes != 2) {
//				Com_Printf ("clip velocity, numplanes == %i\n",numplanes);
				VectorClear (ent->v.velocity);
				return 7;
			}
			CrossProduct (planes[0], planes[1], dir);
			d = DotProduct (dir, ent->v.velocity);
			VectorScale (dir, d, ent->v.velocity);
		}

		// if original velocity is against the original velocity, stop dead
		// to avoid tiny occilations in sloping corners
		if (DotProduct (ent->v.velocity, primal_velocity) <= 0) {
			VectorClear (ent->v.velocity);
			return blocked;
		}
	}

	return blocked;
}

void SV_AddGravity (edict_t *ent, float scale) {
	ent->v.velocity[2] -= scale * movevars.gravity * sv_frametime;
}

/*
===============================================================================
PUSHMOVE
===============================================================================
*/

//Does not change the entities velocity at all
trace_t SV_PushEntity (edict_t *ent, vec3_t push) {
	trace_t trace;
	vec3_t end;

	VectorAdd (ent->v.origin, push, end);

	if (ent->v.movetype == MOVETYPE_FLYMISSILE)
		trace = SV_Move (ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_MISSILE, ent);
	else if (ent->v.solid == SOLID_TRIGGER || ent->v.solid == SOLID_NOT)
		// only clip against bmodels
		trace = SV_Move (ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NOMONSTERS, ent);
	else
		trace = SV_Move (ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent);

	VectorCopy (trace.endpos, ent->v.origin);
	SV_LinkEdict (ent, true);

	if (trace.ent)
		SV_Impact (ent, trace.ent);		

	return trace;
}					

qboolean SV_Push (edict_t *pusher, vec3_t move) {
	int i, e, num_moved;
	edict_t *check, *block;
	vec3_t mins, maxs, pushorig, moved_from[MAX_EDICTS];
	edict_t *moved_edict[MAX_EDICTS];
	float solid_save;

	for (i = 0; i < 3; i++) {
		mins[i] = pusher->v.absmin[i] + move[i];
		maxs[i] = pusher->v.absmax[i] + move[i];
	}

	VectorCopy (pusher->v.origin, pushorig);
	
	// move the pusher to its final position

	VectorAdd (pusher->v.origin, move, pusher->v.origin);
	SV_LinkEdict (pusher, false);

	// see if any solid entities are inside the final position
	num_moved = 0;
	check = NEXT_EDICT(sv.edicts);
	for (e = 1; e < sv.num_edicts; e++, check = NEXT_EDICT(check)) {
		if (check->free)
			continue;
		if (check->v.movetype == MOVETYPE_PUSH || check->v.movetype == MOVETYPE_NONE || check->v.movetype == MOVETYPE_NOCLIP)
			continue;

		solid_save = pusher->v.solid;
		pusher->v.solid = SOLID_NOT;
		block = SV_TestEntityPosition (check);
		pusher->v.solid = solid_save;
		if (block)
			continue;

		// if the entity is standing on the pusher, it will definately be moved
		if ( ! ( ((int)check->v.flags & FL_ONGROUND) && PROG_TO_EDICT(check->v.groundentity) == pusher) ) {
			if ( 
					check->v.absmin[0] >= maxs[0]
					|| check->v.absmin[1] >= maxs[1]
					|| check->v.absmin[2] >= maxs[2]
					|| check->v.absmax[0] <= mins[0]
					|| check->v.absmax[1] <= mins[1]
					|| check->v.absmax[2] <= mins[2] 
			)
				continue;

			// see if the ent's bbox is inside the pusher's final position
			if (!SV_TestEntityPosition (check))
				continue;
		}

		// remove the onground flag for non-players 
		if (check->v.movetype != MOVETYPE_WALK) 
			check->v.flags = (int) check->v.flags & ~FL_ONGROUND;

		VectorCopy (check->v.origin, moved_from[num_moved]);
		moved_edict[num_moved] = check;
		num_moved++;

		// try moving the contacted entity 
		VectorAdd (check->v.origin, move, check->v.origin);
		block = SV_TestEntityPosition (check);
		if (!block) {	
			// pushed ok
			SV_LinkEdict (check, false);
			continue;
		}

		// if it is ok to leave in the old position, do it
		VectorSubtract (check->v.origin, move, check->v.origin);
		block = SV_TestEntityPosition (check);
		if (!block) {
			num_moved--;
			continue;
		}

		// if it is still inside the pusher, block
		if (check->v.mins[0] == check->v.maxs[0]) {
			SV_LinkEdict (check, false);
			continue;
		}
		if (check->v.solid == SOLID_NOT || check->v.solid == SOLID_TRIGGER)	{	
			// corpse
			check->v.mins[0] = check->v.mins[1] = 0;
			VectorCopy (check->v.mins, check->v.maxs);
			SV_LinkEdict (check, false);
			continue;
		}
		
		VectorCopy (pushorig, pusher->v.origin);
		SV_LinkEdict (pusher, false);

		// if the pusher has a "blocked" function, call it
		// otherwise, just stay in place until the obstacle is gone
		if (pusher->v.blocked) {
			pr_global_struct->self = EDICT_TO_PROG(pusher);
			pr_global_struct->other = EDICT_TO_PROG(check);
			PR_ExecuteProgram (pusher->v.blocked);
		}
		
		// move back any entities we already moved
		for (i = 0; i < num_moved; i++) {
			VectorCopy (moved_from[i], moved_edict[i]->v.origin);
			SV_LinkEdict (moved_edict[i], false);
		}
		return false;
	}

	return true;
}

void SV_PushMove (edict_t *pusher, float movetime) {
	int i;
	vec3_t move;

	if (!pusher->v.velocity[0] && !pusher->v.velocity[1] && !pusher->v.velocity[2]) {
		pusher->v.ltime += movetime;
		return;
	}

	for (i = 0; i < 3; i++)
		move[i] = pusher->v.velocity[i] * movetime;

	if (SV_Push (pusher, move))
		pusher->v.ltime += movetime;
}

void SV_Physics_Pusher (edict_t *ent) {
	float l, thinktime, oldltime, movetime;
	vec3_t oldorg, move;

	oldltime = ent->v.ltime;
	
	thinktime = ent->v.nextthink;
	if (thinktime < ent->v.ltime + sv_frametime) {
		movetime = thinktime - ent->v.ltime;
		if (movetime < 0)
			movetime = 0;
	} else {
		movetime = sv_frametime;
	}

	if (movetime)
		SV_PushMove (ent, movetime);	// advances ent->v.ltime if not blocked
		
	if (thinktime > oldltime && thinktime <= ent->v.ltime) {
		VectorCopy (ent->v.origin, oldorg);
		ent->v.nextthink = 0;
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(ent);
		pr_global_struct->other = EDICT_TO_PROG(sv.edicts);
		PR_ExecuteProgram (ent->v.think);
		if (ent->free)
			return;
		VectorSubtract (ent->v.origin, oldorg, move);

		l = VectorLength(move);
		if (l > 1.0/64)	{
			VectorCopy (oldorg, ent->v.origin);
			SV_Push (ent, move);
		}
	}
}

//Non moving objects can only think
void SV_Physics_None (edict_t *ent) {
	// regular thinking
	SV_RunThink (ent);
}

//A moving object that doesn't obey physics
void SV_Physics_Noclip (edict_t *ent) {
	// regular thinking
	if (!SV_RunThink (ent))
		return;

	VectorMA (ent->v.angles, sv_frametime, ent->v.avelocity, ent->v.angles);
	VectorMA (ent->v.origin, sv_frametime, ent->v.velocity, ent->v.origin);

	SV_LinkEdict (ent, false);
}

/*
==============================================================================
TOSS / BOUNCE
==============================================================================
*/

void SV_CheckWaterTransition (edict_t *ent) {
	int cont;

	cont = SV_PointContents (ent->v.origin);
	if (!ent->v.watertype) {	
		// just spawned here
		ent->v.watertype = cont;
		ent->v.waterlevel = 1;
		return;
	}
	
	if (cont <= CONTENTS_WATER) {
		if (ent->v.watertype == CONTENTS_EMPTY) {	
			// just crossed into water
			SV_StartSound (ent, 0, "misc/h2ohit1.wav", 255, 1);
		}		
		ent->v.watertype = cont;
		ent->v.waterlevel = 1;
	} else {
		if (ent->v.watertype != CONTENTS_EMPTY) {	
			// just crossed into water
			SV_StartSound (ent, 0, "misc/h2ohit1.wav", 255, 1);
		}		
		ent->v.watertype = CONTENTS_EMPTY;
		ent->v.waterlevel = cont;
	}
}

//Toss, bounce, and fly movement.  When onground, do nothing.
void SV_Physics_Toss (edict_t *ent) {
	trace_t trace;
	vec3_t move;
	float backoff;

	// regular thinking
	if (!SV_RunThink (ent))
		return;

	if (ent->v.velocity[2] > 0)
		ent->v.flags = (int)ent->v.flags & ~FL_ONGROUND;

	// if onground, return without moving
	if ( ((int)ent->v.flags & FL_ONGROUND) )
		return;

	SV_CheckVelocity (ent);

	// add gravity
	if (ent->v.movetype != MOVETYPE_FLY && ent->v.movetype != MOVETYPE_FLYMISSILE)
		SV_AddGravity (ent, 1.0);

	// move angles
	VectorMA (ent->v.angles, sv_frametime, ent->v.avelocity, ent->v.angles);

	// move origin
	VectorScale (ent->v.velocity, sv_frametime, move);
	trace = SV_PushEntity (ent, move);
	if (trace.fraction == 1)
		return;
	if (ent->free)
		return;
	
	if (ent->v.movetype == MOVETYPE_BOUNCE)
		backoff = 1.5;
	else
		backoff = 1;

	ClipVelocity (ent->v.velocity, trace.plane.normal, ent->v.velocity, backoff);

	// stop if on ground
	if (trace.plane.normal[2] > 0.7) {		
		if (ent->v.velocity[2] < 60 || ent->v.movetype != MOVETYPE_BOUNCE ) {
			ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
			ent->v.groundentity = EDICT_TO_PROG(trace.ent);
			VectorClear (ent->v.velocity);
			VectorClear (ent->v.avelocity);
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
Monsters freefall when they don't have a ground entity, otherwise all movement is done with discrete steps.
This is also used for objects that have become still on the ground, but will fall if the floor is pulled out from under them.
FIXME: is this true?
*/
void SV_Physics_Step (edict_t *ent) {
	qboolean hitsound;
	int movetype;

	// frefall if not onground
	if (!((int) ent->v.flags & (FL_ONGROUND | FL_FLY | FL_SWIM))) {
		if (ent->v.velocity[2] < movevars.gravity*-0.1)
			hitsound = true;
		else
			hitsound = false;

		SV_AddGravity (ent, 1.0);
		SV_CheckVelocity (ent);
		movetype = (ent->v.solid == SOLID_NOT || ent->v.solid == SOLID_TRIGGER) ? MOVE_NOMONSTERS : MOVE_NORMAL;
		SV_FlyMove (ent, sv_frametime, NULL, movetype); 
		SV_LinkEdict (ent, true);

		if ((int) ent->v.flags & FL_ONGROUND) { // just hit ground
			if (hitsound)
				SV_StartSound (ent, 0, "demon/dland2.wav", 255, 1);
		}
	}

	// regular thinking
	SV_RunThink (ent);
	
	SV_CheckWaterTransition (ent);
}

//============================================================================

void SV_ProgStartFrame (void) {
	// let the progs know that a new frame has started
	pr_global_struct->self = EDICT_TO_PROG(sv.edicts);
	pr_global_struct->other = EDICT_TO_PROG(sv.edicts);
	pr_global_struct->time = sv.time;
	PR_ExecuteProgram (pr_global_struct->StartFrame);
}

void SV_RunEntity (edict_t *ent) {
	if (ent->lastruntime == sv.time)
		return;
	ent->lastruntime = sv.time;

	ent->v.lastruntime = (float)svs.realtime; // QW compatibility (FIXME: remove?)

	switch ((int) ent->v.movetype) {
	case MOVETYPE_PUSH:
		SV_Physics_Pusher (ent);
		break;
	case MOVETYPE_NONE:
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
		Host_Error ("SV_Physics: bad movetype %i", (int)ent->v.movetype);			
	}
}

void SV_RunNewmis (void) {
	edict_t	*ent;
	double save_frametime;

	if (!pr_global_struct->newmis)
		return;

	ent = PROG_TO_EDICT(pr_global_struct->newmis);
	pr_global_struct->newmis = 0;

	save_frametime = sv_frametime;
	sv_frametime = 0.05;

	SV_RunEntity (ent);

	sv_frametime = save_frametime;
}

void SV_Physics (void) {
	int i;
	edict_t *ent;

	if (sv.state != ss_active)
		return;

	if (sv.old_time) {
		// don't bother running a frame if sv_mintic seconds haven't passed
		sv_frametime = sv.time - sv.old_time;
		if (sv_frametime < sv_mintic.value)
			return;
		if (sv_frametime > sv_maxtic.value)
			sv_frametime = sv_maxtic.value;
		sv.old_time = sv.time;
	} else {
		sv_frametime = 0.1;		// initialization frame
	}

	pr_global_struct->frametime = sv_frametime;

	SV_ProgStartFrame ();

	// treat each object in turn
	// even the world gets a chance to think
	ent = sv.edicts;
	for (i = 0 ; i<sv.num_edicts ; i++, ent = NEXT_EDICT(ent)) {
		if (ent->free)
			continue;

		if (pr_global_struct->force_retouch)
			SV_LinkEdict (ent, true);	// force retouch even for stationary

		if (i > 0 && i <= MAX_CLIENTS)
			continue;		// clients are run directly from packets

		SV_RunEntity (ent);
		SV_RunNewmis ();
	}
	
	if (pr_global_struct->force_retouch)
		pr_global_struct->force_retouch--;	
}

void SV_SetMoveVars(void) {
	movevars.gravity			= sv_gravity.value; 
	movevars.stopspeed		    = pm_stopspeed.value;		 
	movevars.maxspeed			= pm_maxspeed.value;			 
	movevars.spectatormaxspeed  = pm_spectatormaxspeed.value; 
	movevars.accelerate		    = pm_accelerate.value;		 
	movevars.airaccelerate	    = pm_airaccelerate.value;	 
	movevars.wateraccelerate	= pm_wateraccelerate.value;	   
	movevars.friction			= pm_friction.value;			 
	movevars.waterfriction	    = pm_waterfriction.value;	 
	movevars.entgravity			= 1.0;
	movevars.slidefix           = pm_slidefix.value;
	movevars.ktjump             = pm_ktjump.value;
}
