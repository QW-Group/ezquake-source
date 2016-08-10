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


cvar_t	cl_nopred	= {"cl_nopred", "0"};
cvar_t cl_pushlatency = {"pushlatency", "-999"};

extern cvar_t cl_independentPhysics;

#ifdef JSS_CAM
cvar_t cam_thirdperson = {"cam_thirdperson", "0"};
cvar_t cam_dist = {"cam_dist", "100"};
cvar_t cam_lockdir = {"cam_lockdir", "0"};
cvar_t cam_lockpos = {"cam_lockpos", "0"};
static vec3_t saved_angles;
qbool clpred_newpos = false;
#endif

void CL_PredictUsercmd (player_state_t *from, player_state_t *to, usercmd_t *u) {
	// split up very long moves
	if (u->msec > 50) {
		player_state_t temp;
		usercmd_t split;

		split = *u;
		split.msec /= 2;

		CL_PredictUsercmd (from, &temp, &split);
		CL_PredictUsercmd (&temp, to, &split);
		return;
	}

	VectorCopy (from->origin, pmove.origin);
	VectorCopy (u->angles, pmove.angles);
	VectorCopy (from->velocity, pmove.velocity);

	pmove.jump_msec = (cl.z_ext & Z_EXT_PM_TYPE) ? 0 : from->jump_msec;
	pmove.jump_held = from->jump_held;
	pmove.waterjumptime = from->waterjumptime;
	pmove.pm_type = from->pm_type;
	pmove.onground = from->onground;
	pmove.cmd = *u;

#ifdef JSS_CAM
	if (cam_lockdir.value) {
		VectorCopy (saved_angles, pmove.cmd.angles);
		VectorCopy (saved_angles, pmove.angles);
	}
	else
		VectorCopy (pmove.cmd.angles, saved_angles);
#endif

	movevars.entgravity = cl.entgravity;
	movevars.maxspeed = cl.maxspeed;
	movevars.bunnyspeedcap = cl.bunnyspeedcap;

	PM_PlayerMove ();

	to->waterjumptime = pmove.waterjumptime;
	to->pm_type = pmove.pm_type;
	to->jump_held = pmove.jump_held;
	to->jump_msec = pmove.jump_msec;
	pmove.jump_msec = 0;

	VectorCopy (pmove.origin, to->origin);
	VectorCopy (pmove.angles, to->viewangles);
	VectorCopy (pmove.velocity, to->velocity);
	to->onground = pmove.onground;

	to->weaponframe = from->weaponframe;
}

//Used when cl_nopred is 1 to determine whether we are on ground, otherwise stepup smoothing code produces ugly jump physics
void CL_CategorizePosition (void) {
	if (cl.spectator && cl.playernum == cl.viewplayernum) {
		cl.onground = false;	// in air
		return;
	}
	VectorClear (pmove.velocity);
	VectorCopy (cl.simorg, pmove.origin);
	pmove.numtouch = 0;
	PM_CategorizePosition ();
	cl.onground = pmove.onground;
}

//Smooth out stair step ups.
//Called before CL_EmitEntities so that the player's lightning model origin is updated properly
void CL_CalcCrouch (void) {
	qbool teleported;
	static vec3_t oldorigin = {0, 0, 0};
	static float oldz = 0, extracrouch = 0, crouchspeed = 100;

	teleported = !VectorL2Compare(cl.simorg, oldorigin, 48);
	VectorCopy (cl.simorg, oldorigin);

	if (teleported) {
		// possibly teleported or respawned
		oldz = cl.simorg[2];
		extracrouch = 0;
		crouchspeed = 100;
		cl.crouch = 0;
		VectorCopy (cl.simorg, oldorigin);
		return;
	}

	if (cl.onground && cl.simorg[2] - oldz > 0) {
		if (cl.simorg[2] - oldz > 20) {
			// if on steep stairs, increase speed
			if (crouchspeed < 160) {
				extracrouch = cl.simorg[2] - oldz - cls.frametime * 200 - 15;
				extracrouch = min(extracrouch, 5);
			}
			crouchspeed = 160;
		}

		oldz += cls.frametime * crouchspeed;
		if (oldz > cl.simorg[2])
			oldz = cl.simorg[2];

		if (cl.simorg[2] - oldz > 15 + extracrouch)
			oldz = cl.simorg[2] - 15 - extracrouch;
		extracrouch -= cls.frametime * 200;
		extracrouch = max(extracrouch, 0);

		cl.crouch = oldz - cl.simorg[2];
	} else {
		// in air or moving down
		oldz = cl.simorg[2];
		cl.crouch += cls.frametime * 150;
		if (cl.crouch > 0)
			cl.crouch = 0;
		crouchspeed = 100;
		extracrouch = 0;
	}
}


extern qbool physframe; // for fps-independent physics

static void CL_LerpMove (qbool angles_lerp)
{	
	static int		lastsequence = 0;
	static vec3_t	lerp_angles[3];
	static vec3_t	lerp_origin[3];
	static double	lerp_times[3];
	static qbool	nolerp[2];
	static double	demo_latency = 0.01;
	float	frac;
	double	simtime;
	int		i;
	int		from, to;
	extern cvar_t cl_nolerp;
	extern int cmdtime_msec;
	extern double physframetime;
	extern qbool cl_nolerp_on_entity_flag;
	double  current_time = cls.demoplayback ? cls.demotime : cls.realtime;
	double  current_lerp_time = cls.demoplayback ? cls.demopackettime : (cmdtime_msec * 0.001);
	qbool   physframe = cls.netchan.outgoing_sequence != lastsequence;

	if ((cl_nolerp.value || cl_nolerp_on_entity_flag)) 
	{
		lastsequence = ((unsigned)-1) >> 1;	//reset
		return;
	}

	if (cls.netchan.outgoing_sequence < lastsequence) 
	{
		// reset
		lastsequence = -1;
		lerp_times[0] = -1;
		demo_latency = 0.01;
	}

	// Independent physics.
	if (physframe) 
	{
		lastsequence = cls.netchan.outgoing_sequence;

		// move along
		lerp_times[2] = lerp_times[1];
		lerp_times[1] = lerp_times[0];
		lerp_times[0] = current_lerp_time;

		VectorCopy (lerp_origin[1], lerp_origin[2]);
		VectorCopy (lerp_origin[0], lerp_origin[1]);
		VectorCopy (cl.simorg, lerp_origin[0]);

		VectorCopy (lerp_angles[1], lerp_angles[2]);
		VectorCopy (lerp_angles[0], lerp_angles[1]);
		VectorCopy (cl.simangles, lerp_angles[0]);

		nolerp[1] = nolerp[0];
		nolerp[0] = false;

		for (i = 0; i < 3; i++)
		{
			if (fabs(lerp_origin[0][i] - lerp_origin[1][i]) > 100)
			{
				break;
			}
		}

		if (i < 3)
		{
			// a teleport or something
			nolerp[0] = true;	
		}
	}

	simtime = current_time - demo_latency;

	// Adjust latency
	if (simtime > lerp_times[0]) 
	{
		// High clamp
		demo_latency = current_time - lerp_times[0];
	}
	else if (simtime < lerp_times[2]) 
	{
		// Low clamp
		demo_latency = current_time - lerp_times[2];
	} 
	else
	{
		// slowly drift down till corrected
		if (physframe)
			demo_latency -= min(physframetime * 0.005, 0.001);
	}

	// decide where to lerp from
	if (simtime > lerp_times[1]) 
	{
		from = 1;
		to = 0;
	} 
	else 
	{
		from = 2;
		to = 1;
	}

	if (nolerp[to])
	{
		return;
	}

    frac = (simtime - lerp_times[from]) / (lerp_times[to] - lerp_times[from]);
    frac = bound (0, frac, 1);

	if ((cl.spectator && cl.viewplayernum != cl.playernum) || angles_lerp)
	{
		// we track someone, so lerp angles
		AngleInterpolate(lerp_angles[from], frac, lerp_angles[to], cl.simangles);
	}

	for (i = 0; i < 3; i++)
	{
		cl.simorg[i] = lerp_origin[from][i] + (lerp_origin[to][i] - lerp_origin[from][i]) * frac;
	}
}

qbool cl_nolerp_on_entity_flag = false;
// function check_standing_on_entity(void)
// raises flag cl_nolerp_on_entity_flag if standing on entity
// and cl_nolerp_on_entity.value is 1
static void check_standing_on_entity(void)
{
  extern cvar_t cl_nolerp;
  extern cvar_t cl_nolerp_on_entity;
  extern cvar_t cl_independentPhysics;
  cl_nolerp_on_entity_flag = 
       (pmove.onground && pmove.groundent > 0 &&
        cl_nolerp_on_entity.value &&
        cl_independentPhysics.value);
}

void CL_PredictMove (void) {
	int i, oldphysent;
	frame_t *from = NULL, *to;
	qbool angles_lerp = false;

	if (cl.paused && !(cls.mvdplayback && cl_multiview.integer))
		return;

	if (cl.intermission) {
		cl.crouch = 0;
		return;
	}

	if (cls.nqdemoplayback)
		return;

	if (!cl.validsequence)
		return;

	if (cls.netchan.outgoing_sequence - cl.validsequence >= UPDATE_BACKUP - 1)
		return;

	VectorCopy (cl.viewangles, cl.simangles);

	// this is the last valid frame received from the server
	to = &cl.frames[cl.validsequence & UPDATE_MASK];


#ifdef JSS_CAM
	if (clpred_newpos && cls.demoplayback && cl.spectator)
		VectorCopy (cl.simorg, to->playerstate[cl.playernum].origin);
	clpred_newpos = false;
#endif

	// FIXME...
	if (cls.demoplayback && cl.spectator && cl.viewplayernum != cl.playernum) {
		VectorCopy (to->playerstate[cl.viewplayernum].velocity, cl.simvel);
#ifdef JSS_CAM
		if (!(cam_thirdperson.value && cam_lockpos.value) && !clpred_newpos)
			VectorCopy (to->playerstate[cl.viewplayernum].origin, cl.simorg);
		if (!cam_thirdperson.value)
			VectorCopy (to->playerstate[cl.viewplayernum].viewangles, cl.simangles);
#endif
		CL_CategorizePosition ();
	}
	else if (to->playerstate[cl.playernum].pm_type == PM_LOCK)
	{
		angles_lerp = true;

		VectorCopy (to->playerstate[cl.playernum].velocity, cl.simvel);
		VectorCopy (to->playerstate[cl.playernum].origin, cl.simorg);
		VectorCopy (to->playerstate[cl.playernum].command.angles, cl.simangles);
		cl.onground = false;
	}
	else
	if ((cl_nopred.value && !cls.mvdplayback) || cl.validsequence + 1 >= cls.netchan.outgoing_sequence) {	
		VectorCopy (to->playerstate[cl.playernum].velocity, cl.simvel);
		VectorCopy (to->playerstate[cl.playernum].origin, cl.simorg);
		CL_CategorizePosition ();
	}
	else if (physframe || !cl_independentPhysics.value)
	{
		oldphysent = pmove.numphysent;
		CL_SetSolidPlayers (cl.playernum);

		// run frames
		for (i = 1; i < UPDATE_BACKUP - 1 && cl.validsequence + i < cls.netchan.outgoing_sequence; i++) {
			from = to;
			to = &cl.frames[(cl.validsequence + i) & UPDATE_MASK];
			CL_PredictUsercmd (&from->playerstate[cl.playernum], &to->playerstate[cl.playernum], &to->cmd);
		}

		pmove.numphysent = oldphysent;

		// save results
		VectorCopy (to->playerstate[cl.playernum].velocity, cl.simvel);
		VectorCopy (to->playerstate[cl.playernum].origin, cl.simorg);
		cl.onground = pmove.onground;
		cl.waterlevel = pmove.waterlevel;
		check_standing_on_entity();
	}

	if (!cls.mvdplayback && cl_independentPhysics.value != 0) {
		CL_LerpMove (angles_lerp || cls.demoplayback);
	}
    CL_CalcCrouch ();

#ifdef JSS_CAM
	if (cam_thirdperson.value && cam_lockpos.value && cl.viewplayernum != cl.playernum) {
		vec3_t v;
		player_state_t *pstate;
		int i;

		i = cl.viewplayernum;

		pstate = &cl.frames[cl.parsecount & UPDATE_MASK].playerstate[i];
		VectorSubtract (pstate->origin, cl.simorg, v);
		vectoangles (v, cl.simangles);
		cl.simangles[PITCH] = -cl.simangles[PITCH];
	}
	else
	if (cam_thirdperson.value && cl.viewplayernum != cl.playernum) {
		int i;
		player_state_t *pstate;
		vec3_t fw, rt, up;

/*
		for (i = 0; i < MAX_CLIENTS; i++)
		{
			if (!cl.players[i].name || cl.players[i].spectator)
				continue;
			goto foundsomeone;
		}
		return;
foundsomeone:
*/
		i = cl.viewplayernum;

		pstate = &cl.frames[cl.parsecount & UPDATE_MASK].playerstate[i];

		AngleVectors (cl.simangles, fw, rt, up);
		VectorMA (pstate->origin, -cam_dist.value, fw, cl.simorg);

	}
#endif	// JSS_CAM
	
}

void CL_InitPrediction (void) {
	Cvar_SetCurrentGroup(CVAR_GROUP_NETWORK);
	Cvar_Register(&cl_nopred);
	Cvar_Register(&cl_pushlatency);

	Cvar_ResetCurrentGroup();

#ifdef JSS_CAM	
	Cvar_SetCurrentGroup(CVAR_GROUP_SPECTATOR);
	Cvar_Register (&cam_thirdperson);
	Cvar_Register (&cam_dist);
	Cvar_Register (&cam_lockdir);
	Cvar_Register (&cam_lockpos);
	Cvar_ResetCurrentGroup();
#endif
}
 
