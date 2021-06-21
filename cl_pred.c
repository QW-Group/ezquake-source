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
#include "qsound.h"
#include "client.h"

cvar_t	cl_nopred	= {"cl_nopred", "0"};
cvar_t	cl_nopred_weapon = { "cl_nopred_weapon", "0" };
cvar_t	cl_predict_weaponsound = { "cl_predict_weaponsound", "1" };
cvar_t	cl_predict_smoothview = { "cl_predict_smoothview", "1" };
cvar_t	cl_predict_beam = { "cl_predict_beam", "1" };
cvar_t	cl_predict_jump = { "cl_predict_jump", "0" };

extern cvar_t cl_independentPhysics;

#ifdef JSS_CAM
cvar_t cam_thirdperson = {"cam_thirdperson", "0"};
cvar_t cam_dist = {"cam_dist", "100"};
cvar_t cam_lockdir = {"cam_lockdir", "0"};
cvar_t cam_lockpos = {"cam_lockpos", "0"};
static vec3_t saved_angles;
qbool clpred_newpos = false;
#endif

static qbool nolerp[2];
static qbool nolerp_nextpos;

void CL_InitWepSounds(void)
{
	cl_sfx_jump = S_PrecacheSound("player/plyrjmp8.wav");
	cl_sfx_ax1 = S_PrecacheSound("weapons/ax1.wav");
	cl_sfx_axhit1 = S_PrecacheSound("player/axhit2.wav");
	cl_sfx_sg = S_PrecacheSound("weapons/guncock.wav");
	cl_sfx_ssg = S_PrecacheSound("weapons/shotgn2.wav");
	cl_sfx_ng = S_PrecacheSound("weapons/rocket1i.wav");
	cl_sfx_sng = S_PrecacheSound("weapons/spike2.wav");
	cl_sfx_gl = S_PrecacheSound("weapons/grenade.wav");
	cl_sfx_rl = S_PrecacheSound("weapons/sgun1.wav");
	cl_sfx_lg = S_PrecacheSound("weapons/lstart.wav");
	cl_sfx_lghit = S_PrecacheSound("weapons/lhit.wav");
	cl_sfx_coil = S_PrecacheSound("weapons/coilgun.wav");
	cl_sfx_hook = S_PrecacheSound("weapons/chain1.wav");
}

void CL_PredictUsercmd (player_state_t *from, player_state_t *to, usercmd_t *u, int local) {
	// split up very long moves
	if (u->msec > 50) {
		player_state_t temp;
		usercmd_t split;

		split = *u;
		split.msec /= 2;

		CL_PredictUsercmd (from, &temp, &split, local);
		CL_PredictUsercmd (&temp, to, &split, local);
		return;
	}

	VectorCopy (from->origin, pmove.origin);
	VectorCopy (u->angles, pmove.angles);
	VectorCopy (from->velocity, pmove.velocity);

	// We only care about these values for our local player
	if (local)
	{
		pmove.client_time = from->client_time;
		pmove.attack_finished = from->attack_finished;
		pmove.client_nextthink = from->client_nextthink;
		pmove.client_thinkindex = from->client_thinkindex;
		pmove.client_ping = from->client_ping;
		pmove.client_predflags = from->client_predflags;

		pmove.ammo_shells = from->ammo_shells;
		pmove.ammo_nails = from->ammo_nails;
		pmove.ammo_rockets = from->ammo_rockets;
		pmove.ammo_cells = from->ammo_cells;

		pmove.weaponframe = from->weaponframe;
		pmove.weapon = from->weapon;
		pmove.items = from->items;

		if (cls.mvdprotocolextensions1 & MVD_PEXT1_SERVERSIDEWEAPON)
		{
			if (pmove_playeffects)
				from->impulse = pmove.impulse;
		}

		pmove.impulse = from->impulse;
	}

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

	PM_PlayerMove();

	if (local)
	{
		if (!pmove_nopred_weapon && cls.mvdprotocolextensions1 & MVD_PEXT1_WEAPONPREDICTION)
			PM_PlayerWeapon();
		else
			pmove.impulse = 0;

		to->client_time = pmove.client_time;
		to->attack_finished = pmove.attack_finished;
		to->client_nextthink = pmove.client_nextthink;
		to->client_thinkindex = pmove.client_thinkindex;
		to->client_ping = from->client_ping;
		to->client_predflags = from->client_predflags;

		to->ammo_shells = pmove.ammo_shells;
		to->ammo_nails = pmove.ammo_nails;
		to->ammo_rockets = pmove.ammo_rockets;
		to->ammo_cells = pmove.ammo_cells;

		to->weaponframe = pmove.weaponframe;
		to->weapon = pmove.weapon;
		to->items = pmove.items;

		to->impulse = pmove.impulse;
	}

	to->waterjumptime = pmove.waterjumptime;
	to->pm_type = pmove.pm_type;
	to->jump_held = pmove.jump_held;
	to->jump_msec = pmove.jump_msec;
	pmove.jump_msec = 0;

	VectorCopy (pmove.origin, to->origin);
	VectorCopy (pmove.angles, to->viewangles);
	VectorCopy (pmove.velocity, to->velocity);
	to->onground = pmove.onground;
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
void CL_CalcCrouch (void)
{
	qbool teleported;
	static vec3_t oldorigin = {0, 0, 0};
	static float oldz = 0, extracrouch = 0, crouchspeed = 100;

	teleported = nolerp[0] || !VectorL2Compare(cl.simorg, oldorigin, 48);
	VectorCopy(cl.simorg, oldorigin);

	if (teleported) {
		// possibly teleported or respawned
		oldz = cl.simorg[2];
		extracrouch = 0;
		crouchspeed = 100;
		cl.crouch = 0;
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
	}
	else {
		// in air or moving down
		oldz = cl.simorg[2];
		cl.crouch += cls.frametime * 150;
		if (cl.crouch > 0)
			cl.crouch = 0;
		crouchspeed = 100;
		extracrouch = 0;
	}
}

void CL_DisableLerpMove(void)
{
	nolerp[0] = nolerp[1] = nolerp_nextpos = true;
}

static void CL_LerpMove (qbool angles_lerp)
{	
	static int		lastsequence = 0;
	static vec3_t	lerp_angles[3];
	static vec3_t	lerp_origin[3];
	static double	lerp_times[3];
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
		nolerp[0] = nolerp_nextpos;
		nolerp_nextpos = false;

		for (i = 0; i < 3; i++)
		{
			if (fabs(lerp_origin[0][i] - lerp_origin[1][i]) > 100)
			{
				break;
			}
		}

		if (i < 3)
		{
			extern cvar_t cl_earlypackets;

			// a teleport or something
			nolerp[0] = true;

			// cl.simangles will already be set, don't lerp there either
			// (gives a flash of looking in wrong direction at teleport entrance)
			nolerp[1] |= (cl_earlypackets.integer);
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
	if (simtime > lerp_times[1]) {
		from = 1;
		to = 0;
	} 
	else {
		from = 2;
		to = 1;
	}

	if (nolerp[to]) {
		return;
	}

    frac = (simtime - lerp_times[from]) / (lerp_times[to] - lerp_times[from]);
    frac = bound (0, frac, 1);

	if ((cl.spectator && cl.viewplayernum != cl.playernum) || angles_lerp) {
		// we track someone, so lerp angles
		AngleInterpolate(lerp_angles[from], frac, lerp_angles[to], cl.simangles);
	}

	for (i = 0; i < 3; i++)	{
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

void CL_PredictMove (qbool physframe) {
	int i, oldphysent;
	frame_t *from = NULL, *to;
	qbool angles_lerp = false;

	if (cl.paused && !CL_MultiviewEnabled())
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
	else if ((cl_nopred.value && !cls.mvdplayback) || cl.validsequence + 1 >= cls.netchan.outgoing_sequence) {
		VectorCopy (to->playerstate[cl.playernum].velocity, cl.simvel);
		VectorCopy (to->playerstate[cl.playernum].origin, cl.simorg);
		CL_CategorizePosition ();
	}
	else if (physframe || !cl_independentPhysics.value)
	{
		oldphysent = pmove.numphysent;
		CL_SetSolidPlayers (cl.playernum);

		pmove_playeffects = false;
		pmove_nopred_weapon = (cl_nopred_weapon.integer || pmove.client_predflags == PRDFL_FORCEOFF || cl.spectator);

		// run frames
		for (i = 1; i < UPDATE_BACKUP - 1 && cl.validsequence + i < cls.netchan.outgoing_sequence; i++) {
			if (cl.validsequence + i > pmove.effect_frame)
			{
				pmove_playeffects = true;
				pmove.last_frame = pmove.effect_frame;
				pmove.effect_frame = cl.validsequence + i;
			}

			from = to;
			to = &cl.frames[(cl.validsequence + i) & UPDATE_MASK];
			CL_PredictUsercmd (&from->playerstate[cl.playernum], &to->playerstate[cl.playernum], &to->cmd, true);

			if (i == 1)
				pmove.weapon_serverstate = to->playerstate[cl.playernum].weapon;

			if ((cl.validsequence + i) == cl.simerr_frame)
			{
				vec3_t diff;
				VectorSubtract(cl.simerr_org, to->playerstate[cl.playernum].origin, diff);
				if (VectorLength(diff) > 4 && VectorLength(diff) < 64)
				{
					float mult;
					mult = min(13 / cls.latency, 1);
					VectorScale(diff, mult, diff);

					VectorAdd(diff, cl.simerr_nudge, cl.simerr_nudge);
				}
			}
		}

		pmove.numphysent = oldphysent;

		// save results
		VectorCopy (to->playerstate[cl.playernum].velocity, cl.simvel);
		VectorCopy (to->playerstate[cl.playernum].origin, cl.simorg);

		//
		// error smoothing
		if (cl_predict_smoothview.value >= 0.1 && !cl.spectator)
		{
			cl.simerr_frame = cl.validsequence + i - 1;
			VectorCopy(to->playerstate[cl.playernum].origin, cl.simerr_org);
			float nudge = VectorLength(cl.simerr_nudge);
			vec3_t nudge_norm; VectorCopy(cl.simerr_nudge, nudge_norm);
			VectorNormalize(nudge_norm);

			float check_deltatime = cl.time - cl.simerr_lastcheck;
			cl.simerr_lastcheck = cl.time;

			float nudge_mult = bound(0.1, 2 - cl_predict_smoothview.value, 2);

			nudge = min(nudge, 64);
			if (nudge < 220 * nudge_mult * check_deltatime)
				nudge = 0;
			else if (nudge < 8)
				nudge -= 200 * nudge_mult * check_deltatime;
			else if (nudge < 16)
				nudge -= 500 * nudge_mult * check_deltatime;
			else if (nudge < 32)
				nudge -= 800 * nudge_mult * check_deltatime;
			else
				nudge -= 1400 * nudge_mult * check_deltatime;
			nudge = max(0, nudge); // in case we overshot due to low framerate or something

			VectorScale(nudge_norm, nudge, cl.simerr_nudge);

			vec3_t goal;
			VectorAdd(cl.simorg, cl.simerr_nudge, goal);
			trace_t checktrace = PM_PlayerTrace(cl.simorg, goal);
			VectorCopy(checktrace.endpos, cl.simorg);
		}
		//
		//

		cl.simwep = pmove.weapon_index;
		cl.simwepframe = pmove.weaponframe;
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
	else if (cam_thirdperson.value && cl.viewplayernum != cl.playernum) {
		int i;
		player_state_t *pstate;
		vec3_t fw, rt, up;

		i = cl.viewplayernum;

		pstate = &cl.frames[cl.parsecount & UPDATE_MASK].playerstate[i];

		AngleVectors (cl.simangles, fw, rt, up);
		VectorMA (pstate->origin, -cam_dist.value, fw, cl.simorg);
	}
#endif	// JSS_CAM
	
}

void CL_InitPrediction(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_NETWORK);
	Cvar_Register(&cl_nopred);
	Cvar_Register(&cl_nopred_weapon);
	Cvar_Register(&cl_predict_weaponsound);
	Cvar_Register(&cl_predict_smoothview);
	Cvar_Register(&cl_predict_beam);
	Cvar_Register(&cl_predict_jump);
	Cvar_ResetCurrentGroup();

	CL_InitWepSounds();

#ifdef JSS_CAM
	Cvar_SetCurrentGroup(CVAR_GROUP_SPECTATOR);
	Cvar_Register(&cam_thirdperson);
	Cvar_Register(&cam_dist);
	Cvar_Register(&cam_lockdir);
	Cvar_Register(&cam_lockpos);
	Cvar_ResetCurrentGroup();
#endif
}
 
