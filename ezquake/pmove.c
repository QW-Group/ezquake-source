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

#include "quakedef.h"
#include "pmove.h"

movevars_t		movevars;
playermove_t	pmove;

float		frametime;

vec3_t		forward, right, up;

vec3_t	player_mins = {-16, -16, -24};
vec3_t	player_maxs = {16, 16, 32};

#define	STEPSIZE	18
#define	MIN_STEP_NORMAL	0.7		// roughly 45 degrees
#define	STOP_EPSILON	0.1

#define pm_flyfriction 4

#define BLOCKED_FLOOR	1 
#define BLOCKED_STEP	2 
#define BLOCKED_OTHER	4 
#define BLOCKED_ANY		7 


void PM_InitBoxHull (void);

void PM_Init (void) {
	PM_InitBoxHull ();
}

//Slide off of the impacting object
//returns the blocked flags (1 = floor, 2 = step / wall)
int PM_ClipVelocity (vec3_t in, vec3_t normal, vec3_t out, float overbounce) {
	float backoff, change;
	int i, blocked;

	blocked = 0;
	if (normal[2] > 0)
		blocked |= BLOCKED_FLOOR;		// floor
	else if (!normal[2])
		blocked |= BLOCKED_STEP;		// step

	backoff = DotProduct (in, normal) * overbounce;

	for (i = 0; i < 3; i++) {
		change = normal[i] * backoff;
		out[i] = in[i] - change;
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
	}

	return blocked;
}

#define	MAX_CLIP_PLANES	5
//The basic solid body movement clip that slides along multiple planes
int PM_SlideMove (void) {
	int bumpcount, numbumps, i, j, blocked, numplanes;
	vec3_t dir, planes[MAX_CLIP_PLANES], primal_velocity, original_velocity, end;
	float d, time_left;
	pmtrace_t trace;

	numbumps = 4;

	blocked = 0;
	VectorCopy (pmove.velocity, original_velocity);
	VectorCopy (pmove.velocity, primal_velocity);
	numplanes = 0;

	time_left = frametime;

	for (bumpcount = 0; bumpcount < numbumps; bumpcount++) {
		VectorMA(pmove.origin, time_left, pmove.velocity, end);
		trace = PM_PlayerTrace (pmove.origin, end);

		if (trace.startsolid || trace.allsolid) {
			// entity is trapped in another solid
			VectorClear (pmove.velocity);
			return 3;
		}

		if (trace.fraction > 0) {	
			// actually covered some distance
			VectorCopy (trace.endpos, pmove.origin);
			numplanes = 0;
		}

		if (trace.fraction == 1)
			 break;		// moved the entire distance

		// save entity for contact
		if (pmove.numtouch < MAX_PHYSENTS) {
			pmove.touchindex[pmove.numtouch] = trace.ent;
			pmove.numtouch++;
		}

		if (trace.plane.normal[2] >= MIN_STEP_NORMAL)
			blocked |= BLOCKED_FLOOR;
		else if (!trace.plane.normal[2])
			blocked |= BLOCKED_STEP;
		else
			blocked |= BLOCKED_OTHER;

		time_left -= time_left * trace.fraction;

		// cliped to another plane
		if (numplanes >= MAX_CLIP_PLANES) {	
			// this shouldn't really happen
			VectorClear (pmove.velocity);
			break;
		}

		VectorCopy (trace.plane.normal, planes[numplanes]);
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		for (i = 0; i < numplanes; i++) {
			PM_ClipVelocity (original_velocity, planes[i], pmove.velocity, 1);
			for (j = 0; j < numplanes; j++) {
				if (j != i) {
					if (DotProduct (pmove.velocity, planes[j]) < 0)
						break;	// not ok
				}
			}
			if (j == numplanes)
				break;
		}

		if (i != numplanes) {
			// go along this plane
		} else {	
			// go along the crease
			if (numplanes != 2) {
				VectorClear (pmove.velocity);
				break;
			}
			CrossProduct (planes[0], planes[1], dir);
			d = DotProduct (dir, pmove.velocity);
			VectorScale (dir, d, pmove.velocity);
		}

		// if velocity is against the original velocity, stop dead to avoid tiny occilations in sloping corners
		if (DotProduct (pmove.velocity, primal_velocity) <= 0) {
			VectorClear (pmove.velocity);
			break;
		}
	}

	if (pmove.waterjumptime)
		VectorCopy (primal_velocity, pmove.velocity);
	return blocked;
}

//Each intersection will try to step over the obstruction instead of sliding along it.
void PM_StepSlideMove (void) {
	vec3_t dest;
	pmtrace_t trace;
	vec3_t original, originalvel, down, up, downvel;
	float downdist, updist;

	// try sliding forward both on ground and up 16 pixels
	// take the move that goes farthest
	VectorCopy (pmove.origin, original);
	VectorCopy (pmove.velocity, originalvel);

	if (!PM_SlideMove())
		return;		// moved the entire distance

	VectorCopy (pmove.origin, down);
	VectorCopy (pmove.velocity, downvel);

	VectorCopy (original, pmove.origin);
	VectorCopy (originalvel, pmove.velocity);

	// move up a stair height
	VectorCopy (pmove.origin, dest);
	dest[2] += STEPSIZE;
	trace = PM_PlayerTrace (pmove.origin, dest);
	if (!trace.startsolid && !trace.allsolid)
		VectorCopy (trace.endpos, pmove.origin);

	PM_SlideMove ();

	// press down the stepheight
	VectorCopy (pmove.origin, dest);
	dest[2] -= STEPSIZE;
	trace = PM_PlayerTrace (pmove.origin, dest);
	if (trace.fraction != 1 && trace.plane.normal[2] < MIN_STEP_NORMAL)
		goto usedown;
	if (!trace.startsolid && !trace.allsolid)
		VectorCopy (trace.endpos, pmove.origin);

	if (pmove.origin[2] < original[2])
		goto usedown;

	VectorCopy (pmove.origin, up);

	// decide which one went farther
	downdist = (down[0] - original[0]) * (down[0] - original[0]) + (down[1] - original[1]) * (down[1] - original[1]);
	updist = (up[0] - original[0]) * (up[0] - original[0]) + (up[1] - original[1]) * (up[1] - original[1]);

	if (downdist >= updist) {
usedown:
		VectorCopy (down, pmove.origin);
		VectorCopy (downvel, pmove.velocity);
	} else { // copy z value from slide move
		pmove.velocity[2] = downvel[2];
	}
	// if at a dead stop, retry the move with nudges to get around lips
}

//Handles both ground friction and water friction
void PM_Friction (void) {
	float	speed, newspeed, control;
	float	friction;
	float	drop;
	vec3_t	start, stop;
	pmtrace_t	trace;
	
	if (pmove.waterjumptime)
		return;

	speed = VectorLength(pmove.velocity);
	if (speed < 1) {
		pmove.velocity[0] = pmove.velocity[1] = 0;
		if (pmove.pm_type == PM_FLY)
			pmove.velocity[2] = 0;
		return;
	}

	if (pmove.waterlevel >= 2) {
		// apply water friction, even if in fly mode
		drop = speed * movevars.waterfriction * pmove.waterlevel * frametime;
	} else if (pmove.pm_type == PM_FLY) {
		// apply flymode friction
		drop = speed * pm_flyfriction * frametime;
	} else if (pmove.onground) {
		// apply ground friction
		friction = movevars.friction;

		// if the leading edge is over a dropoff, increase friction
		if (pmove.onground) {
			start[0] = stop[0] = pmove.origin[0] + pmove.velocity[0]/speed*16;
			start[1] = stop[1] = pmove.origin[1] + pmove.velocity[1]/speed*16;
			start[2] = pmove.origin[2] + player_mins[2];
			stop[2] = start[2] - 34;

			trace = PM_PlayerTrace (start, stop);

			if (trace.fraction == 1)
				friction *= 2;
		}

		control = speed < movevars.stopspeed ? movevars.stopspeed : speed;
		drop = control * friction * frametime;
	}
	else
		return;		// in air, no friction


// scale the velocity
	newspeed = speed - drop;
	newspeed = max(newspeed, 0);
	newspeed /= speed;

	VectorScale (pmove.velocity, newspeed, pmove.velocity);
}

void PM_Accelerate (vec3_t wishdir, float wishspeed, float accel) {
	float addspeed, accelspeed, currentspeed;

	if (pmove.pm_type == PM_DEAD)
		return;
	if (pmove.waterjumptime)
		return;

	currentspeed = DotProduct (pmove.velocity, wishdir);
	addspeed = wishspeed - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = accel * frametime * wishspeed;
	if (accelspeed > addspeed)
		accelspeed = addspeed;
	
	VectorMA(pmove.velocity, accelspeed, wishdir, pmove.velocity);
}

void PM_AirAccelerate (vec3_t wishdir, float wishspeed, float accel) {
	float addspeed, accelspeed, currentspeed, wishspd = wishspeed, originalspeed, newspeed, speedcap;
		
	if (pmove.pm_type == PM_DEAD)
		return;
	if (pmove.waterjumptime)
		return;

	if (movevars.bunnyspeedcap > 0)
		originalspeed = sqrt(pmove.velocity[0] * pmove.velocity[0] + pmove.velocity[1] * pmove.velocity[1]);

	wishspd = min(wishspd, 30);
	currentspeed = DotProduct (pmove.velocity, wishdir);
	addspeed = wishspd - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = accel * wishspeed * frametime;
	accelspeed = min(accelspeed, addspeed);
	
	VectorMA(pmove.velocity, accelspeed, wishdir, pmove.velocity);

	if (movevars.bunnyspeedcap > 0) {
		newspeed = sqrt(pmove.velocity[0] * pmove.velocity[0] + pmove.velocity[1] * pmove.velocity[1]);
		if (newspeed > originalspeed) {
			speedcap = movevars.maxspeed * movevars.bunnyspeedcap;
			if (newspeed > speedcap) {
				if (originalspeed < speedcap)
					originalspeed = speedcap;
				pmove.velocity[0] *= originalspeed / newspeed;
				pmove.velocity[1] *= originalspeed / newspeed;
			}
		}
	}
}

void PM_WaterMove (void) {
	int i;
	vec3_t wishvel, wishdir;
	float wishspeed;

	// user intentions
	for (i = 0; i < 3; i++)
		wishvel[i] = forward[i] * pmove.cmd.forwardmove + right[i] * pmove.cmd.sidemove;

	if (pmove.pm_type != PM_FLY && !pmove.cmd.forwardmove && !pmove.cmd.sidemove && !pmove.cmd.upmove)
		wishvel[2] -= 60;		// drift towards bottom
	else
		wishvel[2] += pmove.cmd.upmove;

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	if (wishspeed > movevars.maxspeed) {
		VectorScale (wishvel, movevars.maxspeed/wishspeed, wishvel);
		wishspeed = movevars.maxspeed;
	}
	wishspeed *= 0.7;

	// water acceleration
	PM_Accelerate (wishdir, wishspeed, movevars.wateraccelerate);

	PM_StepSlideMove ();
}

void PM_FlyMove (void) {
	int		i;
	vec3_t	wishvel, wishdir;
	float	wishspeed;

	for (i = 0; i < 3; i++)
		wishvel[i] = forward[i] * pmove.cmd.forwardmove + right[i] * pmove.cmd.sidemove;
	
	wishvel[2] += pmove.cmd.upmove;

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);
	
	if (wishspeed > movevars.maxspeed) {
		VectorScale (wishvel, movevars.maxspeed/wishspeed, wishvel);
		wishspeed = movevars.maxspeed;
	}
	
	PM_Accelerate (wishdir, wishspeed, movevars.accelerate);
	
	PM_StepSlideMove ();
}

void PM_AirMove (void) {
	int i;
	vec3_t wishvel, wishdir;
	float fmove, smove, wishspeed;

	fmove = pmove.cmd.forwardmove;
	smove = pmove.cmd.sidemove;
	
	forward[2] = 0;
	right[2] = 0;
	VectorNormalize (forward);
	VectorNormalize (right);

	for (i = 0; i < 2; i++)
		wishvel[i] = forward[i] * fmove + right[i] * smove;
	wishvel[2] = 0;

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	// clamp to server defined max speed
	if (wishspeed > movevars.maxspeed) {
		VectorScale (wishvel, movevars.maxspeed/wishspeed, wishvel);
		wishspeed = movevars.maxspeed;
	}
	
	if (pmove.onground) {
		if (pmove.velocity[2] > 0 || !movevars.slidefix)
			pmove.velocity[2] = 0;
		PM_Accelerate (wishdir, wishspeed, movevars.accelerate);
		pmove.velocity[2] -= movevars.entgravity * movevars.gravity * frametime;

		if (!movevars.slidefix)
			pmove.velocity[2] = 0;

		if (!pmove.velocity[0] && !pmove.velocity[1]) {
			pmove.velocity[2] = 0;
			return;
		}

		PM_StepSlideMove ();
	} else {	
		// not on ground, so little effect on velocity
		PM_AirAccelerate (wishdir, wishspeed, movevars.accelerate);

		// add gravity
		pmove.velocity[2] -= movevars.entgravity * movevars.gravity * frametime;

		PM_SlideMove ();
	}
}


pmplane_t	groundplane;

void PM_CategorizePosition (void) {
	vec3_t point;
	int cont;
	pmtrace_t trace;

	// if the player hull point one unit down is solid, the player is on ground

	// see if standing on something solid
	point[0] = pmove.origin[0];
	point[1] = pmove.origin[1];
	point[2] = pmove.origin[2] - 1;
	if (pmove.velocity[2] > 180) {
		pmove.onground = false;
	} else {
		trace = PM_PlayerTrace (pmove.origin, point);
		if (trace.fraction == 1 || trace.plane.normal[2] < MIN_STEP_NORMAL) {
			pmove.onground = false;
		} else {
			pmove.onground = true;
			pmove.groundent = trace.ent;
			groundplane = trace.plane;
			pmove.waterjumptime = 0;
		}

		// standing on an entity other than the world
		if (trace.ent > 0) {
			if (pmove.numtouch < MAX_PHYSENTS) {
				pmove.touchindex[pmove.numtouch] = trace.ent;
				pmove.numtouch++;
			}
		}
	}

	// get waterlevel
	pmove.waterlevel = 0;
	pmove.watertype = CONTENTS_EMPTY;

	point[2] = pmove.origin[2] + player_mins[2] + 1;
	cont = PM_PointContents (point);

	if (cont <= CONTENTS_WATER) {
		pmove.watertype = cont;
		pmove.waterlevel = 1;
		point[2] = pmove.origin[2] + (player_mins[2] + player_maxs[2]) * 0.5;
		cont = PM_PointContents (point);
		if (cont <= CONTENTS_WATER) {
			pmove.waterlevel = 2;
			point[2] = pmove.origin[2] + 22;
			cont = PM_PointContents (point);
			if (cont <= CONTENTS_WATER)
				pmove.waterlevel = 3;
		}
	}

	// snap to ground unless in fly mode or underwater
	if (pmove.onground && pmove.pm_type != PM_FLY && pmove.waterlevel < 3) {
		if (!trace.startsolid && !trace.allsolid)
			VectorCopy (trace.endpos, pmove.origin);
	}
}

void PM_CheckJump (void) {
	if (pmove.pm_type == PM_FLY)
		return;

	if (pmove.pm_type == PM_DEAD) {
		pmove.jump_held = true;	// don't jump on respawn
		return;
	}

	if (!(pmove.cmd.buttons & BUTTON_JUMP)) {
		pmove.jump_held = false;
		return;
	}

	if (pmove.waterjumptime)
		return;

	if (pmove.waterlevel >= 2) {	
		// swimming, not jumping
		pmove.onground = false;

		if (pmove.watertype == CONTENTS_WATER)
			pmove.velocity[2] = 100;
		else if (pmove.watertype == CONTENTS_SLIME)
			pmove.velocity[2] = 80;
		else
			pmove.velocity[2] = 50;
		return;
	}

	if (!pmove.onground)
		return;		// in air, so no effect

#ifdef SERVERONLY
	if (pmove.jump_held)
		return;		// don't pogo stick
#else
	if (pmove.jump_held && !pmove.jump_msec)
		return;		// don't pogo stick
#endif

	// check for jump bug
	// groundplane normal was set in the call to PM_CategorizePosition
	if (pmove.velocity[2] < 0 && DotProduct(pmove.velocity, groundplane.normal) < -0.1) {
		// pmove.velocity is pointing into the ground, clip it
		PM_ClipVelocity (pmove.velocity, groundplane.normal, pmove.velocity, 1);
	}

	pmove.onground = false;
	pmove.velocity[2] += 270;

	if (movevars.ktjump > 0) {
		if (movevars.ktjump > 1)
			movevars.ktjump = 1;
		if (pmove.velocity[2] < 270)
			pmove.velocity[2] = pmove.velocity[2] * (1 - movevars.ktjump) + 270 * movevars.ktjump;
	}

	pmove.jump_held = true;	// don't jump again until released

#ifndef SERVERONLY
	pmove.jump_msec = pmove.cmd.msec;
#endif
}

void PM_CheckWaterJump (void) {
	vec3_t spot;
	int cont;
	vec3_t flatforward;

	if (pmove.waterjumptime)
		return;

	// don't hop out if we just jumped in
	if (pmove.velocity[2] < -180)
		return;

	// see if near an edge
	flatforward[0] = forward[0];
	flatforward[1] = forward[1];
	flatforward[2] = 0;
	VectorNormalize (flatforward);

	VectorMA (pmove.origin, 24, flatforward, spot);
	spot[2] += 8;
	cont = PM_PointContents (spot);
	if (cont != CONTENTS_SOLID)
		return;
	spot[2] += 24;
	cont = PM_PointContents (spot);
	if (cont != CONTENTS_EMPTY)
		return;
	// jump out of water
	VectorScale (flatforward, 50, pmove.velocity);
	pmove.velocity[2] = 310;
	pmove.waterjumptime = 2;	// safety net
	pmove.jump_held = true;	// don't jump again until released
}

//If pmove.origin is in a solid position, try nudging slightly on all axis to allow for the cut precision of the net coordinates
void PM_NudgePosition (void) {
	vec3_t base;
	int x, y, z, i;
	static int sign[3] = {0, -1, 1};

	VectorCopy (pmove.origin, base);

	for (i = 0; i < 3; i++)
		pmove.origin[i] = ((int) (pmove.origin[i] * 8)) * 0.125;

	for (z = 0; z <= 2; z++) {
		for (y = 0; y <= 2; y++) {
			for (x = 0; x <= 2; x++) {
				pmove.origin[0] = base[0] + (sign[x] * 0.125);
				pmove.origin[1] = base[1] + (sign[y] * 0.125);
				pmove.origin[2] = base[2] + (sign[z] * 0.125);
				if (PM_TestPlayerPosition (pmove.origin))
					return;
			}
		}
	}
	VectorCopy (base, pmove.origin);
}

void PM_SpectatorMove (void) {
	float speed, drop, friction, control, newspeed, currentspeed, addspeed, accelspeed, fmove, smove, wishspeed;
	int i;
	vec3_t wishvel, wishdir;

	// friction
	speed = VectorLength (pmove.velocity);
	if (speed < 1) {
		VectorClear (pmove.velocity);
	} else {
		friction = movevars.friction * 1.5;	// extra friction
		control = speed < movevars.stopspeed ? movevars.stopspeed : speed;
		drop = control * friction * frametime;

		// scale the velocity
		newspeed = speed - drop;
		if (newspeed < 0)
			newspeed = 0;
		newspeed /= speed;

		VectorScale (pmove.velocity, newspeed, pmove.velocity);
	}

	// accelerate
	fmove = pmove.cmd.forwardmove;
	smove = pmove.cmd.sidemove;

	VectorNormalize (forward);
	VectorNormalize (right);

	for (i = 0; i < 3; i++)
		wishvel[i] = forward[i] * fmove + right[i] * smove;
	wishvel[2] += pmove.cmd.upmove;

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	// clamp to server defined max speed
	if (wishspeed > movevars.spectatormaxspeed)	{
		VectorScale (wishvel, movevars.spectatormaxspeed / wishspeed, wishvel);
		wishspeed = movevars.spectatormaxspeed;
	}

	currentspeed = DotProduct(pmove.velocity, wishdir);
	addspeed = wishspeed - currentspeed;

	// Buggy QW spectator mode, kept for compatibility
	if (pmove.pm_type == PM_OLD_SPECTATOR) {
		if (addspeed <= 0)
			return;
	}

	if (addspeed > 0) {
		accelspeed = movevars.accelerate * frametime * wishspeed;
		accelspeed = min(accelspeed, addspeed);
		VectorMA(pmove.velocity, accelspeed, wishdir, pmove.velocity);
	}

	// move
	VectorMA (pmove.origin, frametime, pmove.velocity, pmove.origin);
}

//Returns with origin, angles, and velocity modified in place.
//Numtouch and touchindex[] will be set if any of the physents were contacted during the move.
void PM_PlayerMove (void) {
	frametime = pmove.cmd.msec * 0.001;
	pmove.numtouch = 0;

	// take angles directly from command
	VectorCopy (pmove.cmd.angles, pmove.angles);
	AngleVectors (pmove.angles, forward, right, up);

	if (pmove.pm_type == PM_SPECTATOR || pmove.pm_type == PM_OLD_SPECTATOR) {
		PM_SpectatorMove ();
		pmove.onground = false;
		return;
	}

	PM_NudgePosition ();

	// set onground, watertype, and waterlevel
	PM_CategorizePosition ();

	if (pmove.waterlevel == 2 && pmove.pm_type != PM_FLY)
		PM_CheckWaterJump ();

	if (pmove.velocity[2] < 0 || pmove.pm_type == PM_DEAD)
		pmove.waterjumptime = 0;

	if (pmove.waterjumptime) {
		pmove.waterjumptime -= frametime;
		if (pmove.waterjumptime < 0)
			pmove.waterjumptime = 0;
	}

#ifndef SERVERONLY
	if (pmove.jump_msec) {
		pmove.jump_msec += pmove.cmd.msec;
		if (pmove.jump_msec > 50)
			pmove.jump_msec = 0;
	}
#endif

	PM_CheckJump ();

	PM_Friction ();

	if (pmove.waterlevel >= 2)
		PM_WaterMove ();
	else if (pmove.pm_type == PM_FLY)
		PM_FlyMove ();
	else
		PM_AirMove ();

	// set onground, watertype, and waterlevel for final spot
	PM_CategorizePosition ();

	// this is to make sure landing sound is not played twice
	// and falling damage is calculated correctly
	if (pmove.onground && pmove.velocity[2] < -300 && DotProduct(pmove.velocity, groundplane.normal) < -0.1)
		PM_ClipVelocity (pmove.velocity, groundplane.normal, pmove.velocity, 1);
}
