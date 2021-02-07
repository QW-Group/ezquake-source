/*
Copyright (C) 2013 Anton (tonik) Gavrilov.

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

cvar_t cl_easymove = {"cl_easymove", "0"};
cvar_t cl_autohop = {"cl_autohop", "0"};
qbool em_touch_wall;
qbool em_jump_held;

static float pm_frametime;

static float AngleMod (float a)
{
	if (a > 180)
		a -= 360;
	if (a <= - 180)
		a += 360;
	return a;
}


static float FindMax (float start, float end, float (*f)(float))
{
	float p1, p2;

	while (end - start > 0.01)
	{
		p1 = start + (end - start) / 3.0;
		p2 = start + (end - start) * 2.0 / 3.0;

		if (f(p1) > f(p2))
			end = p2;
		else
			start = p1;
	};

	return (end + start)/2;
};


static void Friction (vec3_t velocity)
{
	float speed, newspeed, control;
	float friction;
	float drop;
	
	speed = VectorLength(velocity);
	if (speed < 1)
	{
		velocity[0] = 0;
		velocity[1] = 0;
		return;
	}

	/* apply ground friction */
	friction = movevars.friction;

	control = speed < movevars.stopspeed ? movevars.stopspeed : speed;
	drop = control*friction*pm_frametime;

	/* scale the velocity */
	newspeed = speed - drop;
	if (newspeed < 0)
		newspeed = 0;

	VectorScale (velocity, newspeed / speed, velocity);
}

static void Accelerate (vec3_t wishdir, float wishspd, vec3_t velocity)
{
	int   i;
	float addspeed, accelspeed, currentspeed;
	float wishspd_orig = wishspd;

	currentspeed = DotProduct (velocity, wishdir); 
	addspeed = wishspd - currentspeed;
	if (addspeed <= 0)
		return;

	accelspeed = movevars.accelerate * wishspd_orig * pm_frametime;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i=0 ; i<3 ; i++)
		velocity[i] += accelspeed*wishdir[i];
}

static vec3_t ra_curvel, ra_intentions;
static float RankAngle (float a)
{
	vec3_t wishdir;
	float  delta;
	vec3_t ang1, ang2;
	vec3_t newvel;

	wishdir[0] = cos(a / 180 * M_PI);
	wishdir[1] = sin(a / 180 * M_PI);
	wishdir[2] = 0;

	VectorCopy (ra_curvel, newvel);
	Friction (newvel);
	Accelerate (wishdir, movevars.maxspeed, newvel);

	vectoangles (newvel, ang1);
	vectoangles (ra_intentions, ang2);

	delta = AngleMod(ang2[YAW] - ang1[YAW]);

	return -fabs(delta);
}

static float BestAngle (vec3_t curvel, vec3_t intentions, qbool onground)
{
	float  a1, a2;
	vec3_t cross;
	vec3_t tmp;
	float  ba;
	float  max;

	a1 = 0;
	a2 = 95;
	CrossProduct (curvel, intentions, cross);
	if (cross[2] < 0)
	{
		a1 = -a1;
		a2 = -a2;
	}

	VectorCopy(curvel, ra_curvel);
	VectorCopy(intentions, ra_intentions);

	vectoangles (curvel, tmp);
	ba = tmp[YAW];

	if (a2 > a1)
		max = FindMax (ba + a1, ba + a2, RankAngle);
	else
		max = FindMax (ba + a2, ba + a1, RankAngle);

	return max;
}

static float ZeroAngle (float speed)
{
	if (!speed)
		return 0;

	return acos(30/speed)/M_PI*180;
}

static void TweakMovement (usercmd_t *cmd)
{
	vec3_t fw, rt;
	vec3_t movevec;
	vec3_t v1, ang;
	float a;
	float delta;
	vec3_t intentions;
	vec3_t intentions_a;
	vec3_t cmd_ang;
	qbool onground;
	static qbool locked = false;
	static double startlock = -1;
	static float olddelta;
	static qbool olddelta_valid = false;
	static short oldmove[2];

	if (!cl_easymove.value || cl.waterlevel)
		return;

	if (!cmd->forwardmove && !cmd->sidemove)
	{
		oldmove[0] = oldmove[1] = 0;
		return;
	}

	onground = (cl.onground && !((cmd->buttons & BUTTON_JUMP) && !em_jump_held));

	VectorCopy (cmd->angles, cmd_ang);
	cmd_ang[PITCH] = 0;
	cmd_ang[ROLL] = 0;
	AngleVectors (cmd_ang, fw, rt, NULL);
	VectorScale (fw, cmd->forwardmove, intentions);
	VectorMA (intentions, cmd->sidemove, rt, intentions);
	vectoangles (intentions, intentions_a);

	VectorCopy (cl.simvel, v1);

	v1[2] = 0;

	if (VectorLength(v1) < 30)
		return;

	vectoangles (v1, ang);
	delta = AngleMod(intentions_a[YAW] - ang[YAW]);

	if (!locked && startlock == -1)
		startlock = curtime;

	if ((cmd->forwardmove != oldmove[0] || cmd->sidemove != oldmove[1])
	&& (!onground || (oldmove[0] || oldmove[1]))) {
		locked = false;
		olddelta_valid = false;
		startlock = curtime;
	}
	oldmove[0] = cmd->forwardmove;
	oldmove[1] = cmd->sidemove;

	if (fabs(delta) < 0.5 || (olddelta_valid && (
		(olddelta >= 0 && delta <= 0) || (olddelta <= 0 && delta >= 0)
		|| fabs(delta) > fabs(olddelta) + 0.1)) ) {
			locked = true;
	}
	olddelta = delta;
	olddelta_valid = true;

	if (curtime - startlock > (onground ? 0.1 : 0.3))
		locked = true;

	if (fabs(delta) > 95) {
		locked = true;
		return;
	}

	if (!locked || em_touch_wall)
		return;

	if (onground)
	{
		if (fabs(delta) < 0.5)
			return;
		a = BestAngle (v1, intentions, onground);
	}
	else
	{
		a = intentions_a[YAW] + (delta > 0 ? 1 : -1)*ZeroAngle(VectorLength(v1));
	}

	VectorClear (ang);
	ang[YAW] = a;
	AngleVectors (ang, movevec, NULL, NULL);
	cmd->forwardmove = DotProduct (movevec, fw) * 500;
	cmd->sidemove = DotProduct (movevec, rt) * 500;
}

static void Autohop (usercmd_t *cmd)
{
	static float oldzvel;

	if (!(cmd->buttons & 2))
	{
		oldzvel = cl.simvel[2];
		return;
	}
	if (!cl_autohop.value)
		return;
	if (cl.simvel[2] <= 0 && oldzvel > 0)
		cmd->buttons &= ~2;
	oldzvel = cl.simvel[2];
}

void CL_EasyMove (usercmd_t *cmd)
{
	if (cl.spectator || cl.intermission || cl.stats[STAT_HEALTH] < 0)
		return;

	pm_frametime = cmd->msec * 0.001;
	TweakMovement (cmd);
	Autohop (cmd);
}
