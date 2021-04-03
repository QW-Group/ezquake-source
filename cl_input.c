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
// cl.input.c  -- builds an intended movement command to send to the server

#include "quakedef.h"
#include "movie.h"
#include "gl_model.h"
#include "teamplay.h"
#include "input.h"
#include "pmove.h"		// PM_FLY etc
#include "rulesets.h"

static int IN_BestWeapon_Common(int implicit, int* weapon_order, qbool rendering_only);
#ifdef C_ASSERT
C_ASSERT(sizeof(cl.weapon_order_clientside) == sizeof(cl.weapon_order));
#endif

#ifdef MVD_PEXT1_DEBUG_WEAPON
static void IN_SendWeaponSelection(int items, int* stats, int* weapon_list, int weapon_choice);
#else
#define IN_SendWeaponSelection(...)
#endif

cvar_t cl_anglespeedkey       = {"cl_anglespeedkey","1.5"};
cvar_t cl_backspeed           = {"cl_backspeed","400"};
cvar_t cl_c2spps              = {"cl_c2spps","0"};
cvar_t cl_c2sImpulseBackup    = {"cl_c2sImpulseBackup","3"};
cvar_t cl_c2sdupe             = {"cl_c2sdupe", "0"};
cvar_t cl_forwardspeed        = {"cl_forwardspeed","400"};
cvar_t cl_smartjump           = {"cl_smartjump", "1"};
cvar_t cl_iDrive              = {"cl_iDrive", "0", 0, Rulesets_OnChange_cl_iDrive};
cvar_t cl_movespeedkey        = {"cl_movespeedkey","2.0"};
cvar_t cl_nodelta             = {"cl_nodelta","0"};
cvar_t cl_pitchspeed          = {"cl_pitchspeed","150"};
cvar_t cl_upspeed             = {"cl_upspeed","400"};
cvar_t cl_sidespeed           = {"cl_sidespeed","400"};
cvar_t cl_yawspeed            = {"cl_yawspeed","140"};
cvar_t cl_weaponhide          = {"cl_weaponhide", "0"};
cvar_t cl_weaponpreselect     = {"cl_weaponpreselect", "0"};
cvar_t cl_weaponforgetorder   = {"cl_weaponforgetorder", "0"};
cvar_t cl_weaponforgetondeath = {"cl_weaponforgetondeath", "0"};
cvar_t cl_weaponhide_axe      = {"cl_weaponhide_axe", "0"};
cvar_t freelook               = {"freelook","1"};
cvar_t lookspring             = {"lookspring","0"};
cvar_t lookstrafe             = {"lookstrafe","0"};

cvar_t sensitivity          = {"sensitivity","12"};
cvar_t cursor_sensitivity   = {"scr_cursor_sensitivity", "1"};
cvar_t m_pitch              = {"m_pitch","0.022"};
cvar_t m_yaw                = {"m_yaw","0.022"};
cvar_t m_forward            = {"m_forward","1"};
cvar_t m_side               = {"m_side","0.8"};
cvar_t m_accel              = {"m_accel", "0"};
cvar_t m_accel_offset       = {"m_accel_offset", "0"};
cvar_t m_accel_power        = {"m_accel_power", "2"};
cvar_t m_accel_senscap      = {"m_accel_senscap", "0"};

#ifdef JSS_CAM
cvar_t cam_zoomspeed = {"cam_zoomspeed", "300"};
cvar_t cam_zoomaccel = {"cam_zoomaccel", "2000"};
#endif

extern cvar_t cl_pext_serversideweapon;
extern cvar_t cl_independentPhysics;
extern qbool physframe;
extern double physframetime;

#define CL_INPUT_WEAPONHIDE_DUE_TO_DEATH(health) ( \
	(cl_weaponforgetondeath.integer && (health) <= 0) \
)
#define CL_INPUT_WEAPONHIDE(health) ( \
	(cl_weaponhide.integer == 1) || \
	((cl_weaponhide.integer == 2) && (cl.deathmatch == 1)) || \
	CL_INPUT_WEAPONHIDE_DUE_TO_DEATH(health) \
)

/*
===============================================================================
KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as a parameter to the command so it can be matched up with
the release.

state bit 0 is the current state of the key
state bit 1 is edge triggered on the up to down transition
state bit 2 is edge triggered on the down to up transition
===============================================================================
*/

kbutton_t in_mlook, in_klook;
kbutton_t in_left, in_right, in_forward, in_back;
kbutton_t in_lookup, in_lookdown, in_moveleft, in_moveright;
kbutton_t in_strafe, in_speed, in_use, in_jump, in_attack, in_attack2;
kbutton_t in_up, in_down;

static int in_next_impulse;
static qbool suppress_hide;

// Over-writes weapon selection list
// Called even if server-side weapon switching is enabled
static void ForgetWeaponOrder(int impulse, int* weapon_order)
{
	weapon_order[0] = impulse;
	weapon_order[1] = (cl_weaponhide_axe.integer || impulse == 2 ? 1 : 2);
	weapon_order[2] = (weapon_order[1] == 1 ? 0 : 1);
	weapon_order[3] = 0;
}

static void SetNextImpulse(int impulse, qbool from_weapon_script, qbool set_best_weapon)
{
#ifdef MVD_PEXT1_SERVERSIDEWEAPON
	if (from_weapon_script && (cls.mvdprotocolextensions1 & MVD_PEXT1_SERVERSIDEWEAPON) && cl_pext_serversideweapon.integer) {
		in_next_impulse = 0;
		suppress_hide = false;
		if (set_best_weapon && cl_weaponforgetorder.integer) {
			// we can't over-write cl.weapon_order so keep a copy and use that until the server confirms best selection
			memcpy(cl.weapon_order_clientside, cl.weapon_order, sizeof(cl.weapon_order_clientside));
			cl.weapon_order_use_clientside = true;
			ForgetWeaponOrder(impulse, cl.weapon_order_clientside);
		}
		return;
	}
#endif

	suppress_hide = !from_weapon_script;
	in_next_impulse = impulse;

	if (set_best_weapon && cl_weaponforgetorder.integer) {
		ForgetWeaponOrder(impulse, cl.weapon_order);
	}
}

#define VOID_KEY (-1)

void KeyDown_common (kbutton_t *b, int k)
{
	if (k == b->down[0] || k == b->down[1])
		return;		// repeating key

	if (!b->down[0]) {
		b->down[0] = k;
	} else if (!b->down[1]) {
		b->down[1] = k;
	} else {
		Com_Printf ("Three keys down for a button!\n");
		return;
	}

	if (b->state & 1)
		return;		// still down
	b->state |= 1 + 2;	// down + impulse down
	b->downtime = curtime;
}

qbool KeyUp_common (kbutton_t *b, int k)
{
	if (k == VOID_KEY) { // typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		b->state &= ~1;		// now up
		b->state |= 4; 		// impulse up
		b->uptime = curtime;
		return true;
	}

	if (b->down[0] == k)
		b->down[0] = 0;
	else if (b->down[1] == k)
		b->down[1] = 0;
	else
		return true;		// key up without coresponding down (menu pass through)

	if (b->down[0] || b->down[1])
		return false;		// some other key is still holding it down

	if (!(b->state & 1))
		return true;		// still up (this should not happen)
	b->state &= ~1;		// now up
	b->state |= 4; 		// impulse up
	b->uptime = curtime;
	return true;
}

void KeyDown(kbutton_t *b)
{
	int k = VOID_KEY;
	char *c = Cmd_Argv(1);
	if (*c) {
		k = atoi(c);
	}

	KeyDown_common(b, k);
}

// returns whether the button is now up, will not be if other key is holding it down
qbool KeyUp(kbutton_t *b)
{
	int k = VOID_KEY;
	char *c = Cmd_Argv(1);
	if (*c) {
		k = atoi(c);
	}

	return KeyUp_common(b, k);
}


void IN_KLookDown(void)
{
	KeyDown(&in_klook);
}
void IN_KLookUp(void)
{
	KeyUp(&in_klook);
}

void IN_MLookDown(void)
{
	KeyDown(&in_mlook);
}
void IN_MLookUp(void)
{
	if (concussioned) {
		return;
	}

	KeyUp(&in_mlook);

	if (!mlook_active && lookspring.value) {
		V_StartPitchDrift();
	}
}

#define PROTECTEDKEY()                                 \
	if (allow_scripts.integer == 0) {                  \
		Com_Printf("Movement scripts are disabled\n"); \
		return;                                        \
	}

void IN_UpDown(void) {PROTECTEDKEY(); KeyDown(&in_up);}
void IN_UpUp(void) {PROTECTEDKEY(); KeyUp(&in_up);}
void IN_DownDown(void) {PROTECTEDKEY(); KeyDown(&in_down);}
void IN_DownUp(void) {PROTECTEDKEY(); KeyUp(&in_down);}
void IN_LeftDown(void) {PROTECTEDKEY(); KeyDown(&in_left);}
void IN_LeftUp(void) {PROTECTEDKEY(); KeyUp(&in_left);}
void IN_RightDown(void) {PROTECTEDKEY(); KeyDown(&in_right);}
void IN_RightUp(void) {PROTECTEDKEY(); KeyUp(&in_right);}
void IN_ForwardDown(void) {PROTECTEDKEY(); KeyDown(&in_forward);}
void IN_ForwardUp(void) {PROTECTEDKEY(); KeyUp(&in_forward);}
void IN_BackDown(void) {PROTECTEDKEY(); KeyDown(&in_back);}
void IN_BackUp(void) {PROTECTEDKEY(); KeyUp(&in_back);}
void IN_LookupDown(void) {PROTECTEDKEY(); KeyDown(&in_lookup);}
void IN_LookupUp(void) {PROTECTEDKEY(); KeyUp(&in_lookup);}
void IN_LookdownDown(void) {PROTECTEDKEY(); KeyDown(&in_lookdown);}
void IN_LookdownUp(void) {PROTECTEDKEY(); KeyUp(&in_lookdown);}
void IN_MoveleftDown(void) {PROTECTEDKEY(); KeyDown(&in_moveleft);}
void IN_MoveleftUp(void) {PROTECTEDKEY(); KeyUp(&in_moveleft);}
void IN_MoverightDown(void) {PROTECTEDKEY(); KeyDown(&in_moveright);}
void IN_MoverightUp(void) {PROTECTEDKEY(); KeyUp(&in_moveright);}

void IN_SpeedDown(void) {KeyDown(&in_speed);}
void IN_SpeedUp(void) {KeyUp(&in_speed);}
void IN_StrafeDown(void) {KeyDown(&in_strafe);}
void IN_StrafeUp(void) {KeyUp(&in_strafe);}

// Returns true if given command is a protected movement command and was executed successfully
qbool Key_TryMovementProtected(const char *cmd, qbool down, int key)
{
	typedef void (*KeyPress_fnc) (kbutton_t *b, int key);
	KeyPress_fnc f = down ? KeyDown_common : (KeyPress_fnc)KeyUp_common;
	kbutton_t *b = NULL;

	if      (strcmp(cmd, "+forward") == 0) b = &in_forward;
	else if (strcmp(cmd, "+back") == 0) b = &in_back;
	else if (strcmp(cmd, "+moveleft") == 0) b = &in_moveleft;
	else if (strcmp(cmd, "+moveright") == 0) b = &in_moveright;
	else if (strcmp(cmd, "+left") == 0) b = &in_left;
	else if (strcmp(cmd, "+right") == 0) b = &in_right;
	else if (strcmp(cmd, "+lookup") == 0) b = &in_lookup;
	else if (strcmp(cmd, "+lookdown") == 0) b = &in_lookdown;
	else if (strcmp(cmd, "+moveup") == 0) b = &in_up;
	else if (strcmp(cmd, "+movedown") == 0) b = &in_down;

	if (b) {
		f(b, key);
		return true;
	}
	else {
		return false;
	}
}

void IN_AttackDown(void)
{
	int best;
	if (cl_weaponpreselect.value && (best = IN_BestWeapon(false))) {
		SetNextImpulse(best, true, true);
	}

	KeyDown(&in_attack);
}

// Checks if we have a keycode at the end, e.g. +fire 8 5 3 120
// if it's >= 32, treat is as keycodes, otherwise an impulse
static qbool IN_IsLastArgKeyCode(void)
{
	return atoi(Cmd_Argv(Cmd_Argc() - 1)) >= 32;
}

void IN_FireDown(void)
{
	int key_code = VOID_KEY;
	int last_arg_idx = Cmd_Argc() - 1;
	int i;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <weapon number>\n", Cmd_Argv(0));
		return;
	}

	if (IN_IsLastArgKeyCode()) {
		key_code = Q_atoi(Cmd_Argv(last_arg_idx));
		last_arg_idx--;
	}

	for (i = 1; i <= last_arg_idx && i <= MAXWEAPONS; i++) {
		int desired_impulse = Q_atoi(Cmd_Argv(i));
		cl.weapon_order[i - 1] = desired_impulse;
	}

	for (; i <= MAXWEAPONS; i++) {
		cl.weapon_order[i - 1] = 0;
	}

	if (cl_pext_serversideweapon.integer) {
		cl.weapon_order_sequence_set = cls.netchan.outgoing_sequence;
	}

	SetNextImpulse(IN_BestWeapon(false), true, true);

	KeyDown_common(&in_attack, key_code);
}

void IN_AttackUp_CommonHide(void)
{
	if (CL_INPUT_WEAPONHIDE(cl.stats[STAT_HEALTH])) {
#ifdef MVD_PEXT1_SERVERSIDEWEAPON
		if ((cls.mvdprotocolextensions1 & MVD_PEXT1_SERVERSIDEWEAPON) && cl_pext_serversideweapon.integer) {
			SetNextImpulse(0, false, false);
			return;
		}
#endif
		if (cl_weaponhide_axe.integer) {
			// always switch to axe because user wants to
			SetNextImpulse(1, false, false);
			return;
		}

		// performs "weapon 2 1"
		// that means: if player has shotgun and shells, select shotgun, otherwise select axe
		qbool can_shotgun = ((cl.stats[STAT_ITEMS] & IT_SHOTGUN) && cl.stats[STAT_SHELLS] >= 1);
		can_shotgun |= (CL_INPUT_WEAPONHIDE_DUE_TO_DEATH(cl.stats[STAT_HEALTH]));
		SetNextImpulse(can_shotgun ? 2 : 1, false, false);
	}
}

void IN_FireUp(void)
{
	int key_code = VOID_KEY;

	if (IN_IsLastArgKeyCode()) {
		key_code = Q_atoi(Cmd_Argv(Cmd_Argc() - 1));
	}

	if (KeyUp_common(&in_attack, key_code)) {
		IN_AttackUp_CommonHide();
	}
}


void IN_AttackUp(void)
{
	qbool up = KeyUp(&in_attack);
	if (up) {
		IN_AttackUp_CommonHide();
	}
}

void IN_UseDown (void) {KeyDown(&in_use);}
void IN_UseUp (void) {KeyUp(&in_use);}
void IN_Attack2Down (void) { KeyDown(&in_attack2);}
void IN_Attack2Up (void) { KeyUp(&in_attack2);}


void IN_JumpDown(void)
{
	qbool up;
	int pmt;

	if (cls.state != ca_active || !cl_smartjump.value)
		up = false;
	else if (cls.demoplayback && !cls.mvdplayback)
		up = false; // use jump instead of up in demos unless its MVD and I have no idea why QWD have this restriction.
	else if (cl.spectator)
		up = (Cam_TrackNum() == -1); // NOTE: cl.spectator is non false during MVD playback, so this code executed.
	else if (cl.stats[STAT_HEALTH] <= 0)
		up = false;
	else if (cl.validsequence && (
	((pmt = cl.frames[cl.validsequence & UPDATE_MASK].playerstate[cl.playernum].pm_type) == PM_FLY)
	|| pmt == PM_SPECTATOR || pmt == PM_OLD_SPECTATOR))
		up = true;
	else if (cl.waterlevel >= 2 && !(cl.teamfortress && (in_forward.state & 1)))
		up = true;
	else
		up = false;

	KeyDown(up ? &in_up : &in_jump);
}
void IN_JumpUp(void)
{
	if (cl_smartjump.value)
		KeyUp(&in_up);
	KeyUp(&in_jump);
}

// called within 'impulse' or 'weapon' commands, remembers it's first 10 (MAXWEAPONS) arguments
void IN_RememberWpOrder (void)
{
	int i, c;

	c = Cmd_Argc() - 1;
	for (i = 0; i < MAXWEAPONS; i++) {
		cl.weapon_order[i] = (i < c) ? Q_atoi(Cmd_Argv(i + 1)) : 0;
	}

	if (cl_pext_serversideweapon.integer) {
		cl.weapon_order_sequence_set = cls.netchan.outgoing_sequence;
	}
}

// picks the best available (carried & having some ammunition) weapon according to users current preference
// or if the intersection (wished * carried) is empty
// select the top wished weapon
int IN_BestWeapon(qbool rendering_only)
{
	return IN_BestWeapon_Common(cl.weapon_order[0], cl.weapon_order, rendering_only);
}

// picks the best available (carried & having some ammunition) weapon according to users current preference
// or if the intersection (wished * carried) is empty
// select the current weapon
int IN_BestWeaponReal(qbool rendering_only)
{
	if (cl.weapon_order_use_clientside && rendering_only) {
		return IN_BestWeapon_Common(in_next_impulse, cl.weapon_order_clientside, rendering_only);
	}
	return IN_BestWeapon_Common(in_next_impulse, cl.weapon_order, rendering_only);
}

// finds the best weapon from the carried weapons; if none is found, returns implicit
static int IN_BestWeapon_Common(int implicit, int* weapon_order, qbool rendering_only)
{
	int i, imp, items;
	int best = implicit;

	items = cl.stats[STAT_ITEMS];

	for (i = MAXWEAPONS - 1; i >= 0; i--)
	{
		imp = weapon_order[i];
		if (imp < 1 || imp > 8)
			continue;

		switch (imp)
		{
			case 1:
				if (items & IT_AXE)
					best = 1;
				break;
			case 2:
				if (items & IT_SHOTGUN && cl.stats[STAT_SHELLS] >= 1)
					best = 2;
				break;
			case 3:
				if (items & IT_SUPER_SHOTGUN && cl.stats[STAT_SHELLS] >= 2)
					best = 3;
				break;
			case 4:
				if (items & IT_NAILGUN && cl.stats[STAT_NAILS] >= 1)
					best = 4;
				break;
			case 5:
				if (items & IT_SUPER_NAILGUN && cl.stats[STAT_NAILS] >= 2)
					best = 5;
				break;
			case 6:
				if (items & IT_GRENADE_LAUNCHER && cl.stats[STAT_ROCKETS] >= 1)
					best = 6;
				break;
			case 7:
				if (items & IT_ROCKET_LAUNCHER && cl.stats[STAT_ROCKETS] >= 1)
					best = 7;
				break;
			case 8:
				if (items & IT_LIGHTNING && cl.stats[STAT_CELLS] >= 1)
					best = 8;
		}
	}

	if (!rendering_only) {
		IN_SendWeaponSelection(items, cl.stats, cl.weapon_order, best);
	}
	return best;
}

void IN_Impulse(void)
{
	int best;
	int first = Q_atoi(Cmd_Argv(1));

	SetNextImpulse(first, false, false);
	if (Cmd_Argc() <= 2) {
		return;
	}

	// If more than one argument, select immediately the best weapon.
	IN_RememberWpOrder();
	if ((best = IN_BestWeapon(false))) {
		SetNextImpulse(best, true, true);
	}
	else {
		SetNextImpulse(first, true, false);
	}
}

// This is the same command as impulse but cl_weaponpreselect can be used in here, while for impulses cannot be used.
void IN_Weapon(void)
{
	int c, best, mode, first;

	if ((c = Cmd_Argc() - 1) < 1) {
		Com_Printf("Usage: %s w1 [w2 [w3..]]\nWill pre-select best available weapon from given sequence.\n", Cmd_Argv(0));
		return;
	}

	first = Q_atoi (Cmd_Argv (1));
	if (first == 10) {
		int temp = IN_BestWeapon(false);
		int temp_order[10] = {
			temp % 8 + 1,
			(temp + 1) % 8 + 1,
			(temp + 2) % 8 + 1,
			(temp + 3) % 8 + 1,
			(temp + 4) % 8 + 1,
			(temp + 5) % 8 + 1,
			(temp + 6) % 8 + 1,
			(temp + 7) % 8 + 1,
			0,
			0
		};

		cl.weapon_order[0] = best = IN_BestWeapon_Common(1, temp_order, false);
		if (cl_pext_serversideweapon.integer) {
			cl.weapon_order_sequence_set = cls.netchan.outgoing_sequence;
		}
	}
	else if (first == 12) {
		int temp = IN_BestWeapon(false);
		int temp_order[10] = {
			8 - ((8 - temp + 1) % 8),
			8 - ((8 - temp + 2) % 8),
			8 - ((8 - temp + 3) % 8),
			8 - ((8 - temp + 4) % 8),
			8 - ((8 - temp + 5) % 8),
			8 - ((8 - temp + 6) % 8),
			8 - ((8 - temp + 7) % 8),
			8 - ((8 - temp + 8) % 8),
			0,
			0
		};

		cl.weapon_order[0] = best = IN_BestWeapon_Common(1, temp_order, false);
		cl.weapon_order_sequence_set = cls.netchan.outgoing_sequence;
	}
	else {
		// read user input
		IN_RememberWpOrder ();

		best = IN_BestWeapon(false);
	}

	mode = (int) cl_weaponpreselect.value;

	// cl_weaponpreselect behaviour:
	// 0: select best weapon right now
	// 1: always only pre-select; switch to it on +attack
	// 2: user is holding +attack -> select, otherwise just pre-select
	// 3: same like 1, but only in deathmatch 1
	// 4: same like 2, but only in deathmatch 1
	if (mode == 3) {
		mode = (cl.deathmatch == 1) ? 1 : 0;
	}
	else if (mode == 4) {
		mode = (cl.deathmatch == 1) ? 2 : 0;
	}

	switch (mode)
	{
		case 2:
			if ((in_attack.state & 3) && best) {
				// user is holding +attack and there is some weapon available
				SetNextImpulse(best, true, true);
			}
			break;
		case 1:
			break;	// don't select weapon immediately
		default:
		case 0:	// no pre-selection
			if (best) {
				SetNextImpulse(best, true, true);
			}
			break;
	}
}

/*
Returns 0.25 if a key was pressed and released during the frame,
0.5 if it was pressed and held
0 if held then released, and
1.0 if held for the entire time
*/
float CL_KeyState (kbutton_t *key, qbool lookbutton)
{
	float val;
	qbool impulsedown, impulseup, down;

	impulsedown = key->state & 2;
	impulseup = key->state & 4;
	down = key->state & 1;
	val = 0.0;

	if (impulsedown && !impulseup)
	{
		if (down)
			val = lookbutton ? 0.5 : 1.0;	// pressed and held this frame
		else
			val = 0;	// I_Error ();
	}
	if (impulseup && !impulsedown)
	{
		if (down)
			val = 0.0;	// I_Error ();
		else
			val = 0.0;	// released this frame
	}
	if (!impulsedown && !impulseup)
	{
		if (down)
			val = 1.0;	// held the entire frame
		else
			val = 0.0;	// up the entire frame
	}
	if (impulsedown && impulseup)
	{
		if (down)
			val = 0.75;	// released and re-pressed this frame
		else
			val = 0.25;	// pressed and released this frame
	}

	key->state &= 1;	// clear impulses

	return val;
}

//==========================================================================

void CL_Rotate_f(void)
{
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <degrees>\n", Cmd_Argv(0));
		return;
	}

	if ((cl.fpd & FPD_LIMIT_YAW) || allow_scripts.value < 2) {
		return;
	}

	cl.viewangles[YAW] += atof(Cmd_Argv(1));
	cl.viewangles[YAW] = anglemod(cl.viewangles[YAW]);
}

// Moves the local angle positions.
void CL_AdjustAngles(void)
{
	float basespeed, speed, up, down, frametime;

	frametime = cls.trueframetime;
	if (Movie_IsCapturing()) {
		frametime = Movie_InputFrametime();
	}

	basespeed = ((in_speed.state & 1) ? cl_anglespeedkey.value : 1);

	if (!(in_strafe.state & 1)) {
		speed = basespeed * cl_yawspeed.value;
		if ((cl.fpd & FPD_LIMIT_YAW) || allow_scripts.value < 2)
			speed = bound(-900, speed, 900);
		speed *= frametime;
		cl.viewangles[YAW] -= speed * CL_KeyState(&in_right, true);
		cl.viewangles[YAW] += speed * CL_KeyState(&in_left, true);
		if (cl.viewangles[YAW] < 0)
			cl.viewangles[YAW] += 360.0;
		else if (cl.viewangles[YAW] > 360)
			cl.viewangles[YAW] -= 360.0;
	}

	speed = basespeed * cl_pitchspeed.value;
	if ((cl.fpd & FPD_LIMIT_PITCH) || allow_scripts.value == 0)
		speed = bound(-700, speed, 700);
	speed *= frametime;
	if (in_klook.state & 1)
	{
		V_StopPitchDrift ();
		cl.viewangles[PITCH] -= speed * CL_KeyState(&in_forward, true);
		cl.viewangles[PITCH] += speed * CL_KeyState(&in_back, true);
	}


	up = CL_KeyState(&in_lookup, true);
	down = CL_KeyState(&in_lookdown, true);
	cl.viewangles[PITCH] -= speed * up;
	cl.viewangles[PITCH] += speed * down;
	if (up || down)
		V_StopPitchDrift();

	if (cl.viewangles[PITCH] > cl.maxpitch)
		cl.viewangles[PITCH] = cl.maxpitch;
	if (cl.viewangles[PITCH] < cl.minpitch)
		cl.viewangles[PITCH] = cl.minpitch;

	//cl.viewangles[PITCH] = bound(cl.min!pitch, cl.viewangles[PITCH], cl.ma!xpitch);
	cl.viewangles[ROLL] = bound(-50, cl.viewangles[ROLL], 50);
}

// Send the intended movement message to the server.
void CL_BaseMove(usercmd_t *cmd)
{
	float sidespeed = (float)fabs(cl_sidespeed.value);
	float upspeed = (float)fabs(cl_upspeed.value);
	float forwardspeed = (float)fabs(cl_forwardspeed.value);
	float backspeed = (float)fabs(cl_backspeed.value);
	float speedmodifier = (float)fabs(cl_movespeedkey.value);

	CL_AdjustAngles();

	memset(cmd, 0, sizeof(*cmd));

	VectorCopy(cl.viewangles, cmd->angles);

	if (cl_iDrive.integer) {
		float s1, s2;

		if (in_strafe.state & 1) {
			s1 = CL_KeyState (&in_right, false);
			s2 = CL_KeyState (&in_left, false);

			if (s1 && s2) {
				if (in_right.downtime > in_left.downtime)
					s2 = 0;
				if (in_right.downtime < in_left.downtime)
					s1 = 0;
			}

			cmd->sidemove += sidespeed * s1;
			cmd->sidemove -= sidespeed * s2;
		}

		s1 = CL_KeyState (&in_moveright, false);
		s2 = CL_KeyState (&in_moveleft, false);

		if (s1 && s2) {
			if (in_moveright.downtime > in_moveleft.downtime)
				s2 = 0;
			if (in_moveright.downtime < in_moveleft.downtime)
				s1 = 0;
		}

		cmd->sidemove += sidespeed * s1;
		cmd->sidemove -= sidespeed * s2;

		s1 = CL_KeyState (&in_up, false);
		s2 = CL_KeyState (&in_down, false);

		if (s1 && s2) {
			if (in_up.downtime > in_down.downtime)
				s2 = 0;
			if (in_up.downtime < in_down.downtime)
				s1 = 0;
		}

		cmd->upmove += upspeed * s1;
		cmd->upmove -= upspeed * s2;

		if (!(in_klook.state & 1)) {
			s1 = CL_KeyState (&in_forward, false);
			s2 = CL_KeyState (&in_back, false);

			if (s1 && s2)
			{
				if (in_forward.downtime > in_back.downtime)
					s2 = 0;
				if (in_forward.downtime < in_back.downtime)
					s1 = 0;
			}

			cmd->forwardmove += forwardspeed * s1;
			cmd->forwardmove -= backspeed * s2;
		}
	}
	else {
		if (in_strafe.state & 1) {
			cmd->sidemove += sidespeed * CL_KeyState (&in_right, false);
			cmd->sidemove -= sidespeed * CL_KeyState (&in_left, false);
		}

		cmd->sidemove += sidespeed * CL_KeyState (&in_moveright, false);
		cmd->sidemove -= sidespeed * CL_KeyState (&in_moveleft, false);

		cmd->upmove += upspeed * CL_KeyState (&in_up, false);
		cmd->upmove -= upspeed * CL_KeyState (&in_down, false);

		if (!(in_klook.state & 1)) {
			cmd->forwardmove += forwardspeed * CL_KeyState (&in_forward, false);
			cmd->forwardmove -= backspeed * CL_KeyState (&in_back, false);
		}
	}

	// adjust for speed key
	if (in_speed.state & 1) {
		cmd->forwardmove *= speedmodifier;
		cmd->sidemove *= speedmodifier;
		cmd->upmove *= speedmodifier;
	}

	#ifdef JSS_CAM
	{
		static float zoomspeed = 0;
		extern cvar_t cam_thirdperson, cam_lockpos;

		if ((cls.demoplayback || cl.spectator) && cam_thirdperson.integer && !cam_lockpos.integer) {
			zoomspeed -= CL_KeyState(&in_forward, false) * cls.trueframetime * cam_zoomaccel.value;
			zoomspeed += CL_KeyState(&in_back, false) * cls.trueframetime * cam_zoomaccel.value;
			if (!CL_KeyState(&in_forward, false) && !CL_KeyState(&in_back, false)) {
				if (zoomspeed > 0) {
					zoomspeed -= cls.trueframetime * cam_zoomaccel.value;
					if (zoomspeed < 0)
						zoomspeed = 0;
				} else if (zoomspeed < 0) {
					zoomspeed += cls.trueframetime * cam_zoomaccel.value;
					if (zoomspeed > 0)
						zoomspeed = 0;
				}
			}
			zoomspeed = bound (-cam_zoomspeed.value, zoomspeed, cam_zoomspeed.value);

			if (zoomspeed) {
				extern cvar_t cam_dist;
				float dist = cam_dist.value;

				dist += cls.trueframetime * zoomspeed;
				if (dist < 0) {
					dist = 0;
				}
				Cvar_SetValue(&cam_dist, dist);
			}
		}
	}
	#endif // JSS_CAM
}

int MakeChar(int i)
{
	i &= ~3;
	if (i < -127 * 4)
		i = -127 * 4;
	if (i > 127 * 4)
		i = 127 * 4;
	return i;
}

void CL_FinishMove(usercmd_t *cmd)
{
	int i, ms;
	float frametime;
	static double extramsec = 0;
	extern cvar_t allow_scripts;

	frametime = cls.trueframetime;
	if (Movie_IsCapturing()) {
		frametime = Movie_InputFrametime();
	}

	// figure button bits
	if ( in_attack.state & 3 )
		cmd->buttons |= 1;
	in_attack.state &= ~2;

	if (in_jump.state & 3)
		cmd->buttons |= 2;
	in_jump.state &= ~2;

	if (in_use.state & 3)
		cmd->buttons |= 4;
	in_use.state &= ~2;

	if (in_attack2.state & 3)
		cmd->buttons |= 8;
	in_attack2.state &= ~2;

	// send milliseconds of time to apply the move
	//#fps	extramsec += cls.frametime * 1000;
	extramsec += (cl_independentPhysics.value == 0 ? frametime : physframetime) * 1000;	//#fps
	ms = extramsec;
	extramsec -= ms;

	if (ms > 250) {
		ms = 100;		// time was unreasonable
	}

	cmd->msec = ms;

	VectorCopy (cl.viewangles, cmd->angles);

#ifdef MVD_PEXT1_SERVERSIDEWEAPON
	if (!(cls.mvdprotocolextensions1 & MVD_PEXT1_SERVERSIDEWEAPON)) {
#endif
		if (!cl.spectator && CL_INPUT_WEAPONHIDE_DUE_TO_DEATH(cl.stats[STAT_HEALTH])) {
			cl.weapon_order[0] = in_next_impulse = (cl_weaponhide_axe.integer ? 1 : 2);
			cl.weapon_order[1] = 1;
			cl.weapon_order[2] = 0;
		}
#ifdef MVD_PEXT1_SERVERSIDEWEAPON
	}
#endif

	// shaman RFE 1030281 {
	// KTPro's KFJump == impulse 156
	// KTPro's KRJump == impulse 164
	if ( *Info_ValueForKey(cl.serverinfo, "kmod") && (
		((in_next_impulse == 156) && (cl.fpd & FPD_LIMIT_YAW || allow_scripts.value < 2)) ||
		((in_next_impulse == 164) && (cl.fpd & FPD_LIMIT_PITCH || allow_scripts.value == 0))
		)
	) {
		cmd->impulse = 0;
	}
	else {
		cmd->impulse = in_next_impulse;
	}
	// } shaman RFE 1030281
	in_next_impulse = 0;

	// chop down so no extra bits are kept that the server wouldn't get
	cmd->forwardmove = MakeChar(cmd->forwardmove);
	cmd->sidemove = MakeChar(cmd->sidemove);
	cmd->upmove = MakeChar(cmd->upmove);

	for (i = 0; i < 3; i++) {
		cmd->angles[i] = (Q_rint(cmd->angles[i] * 65536.0 / 360.0) & 65535) * (360.0 / 65536.0);
	}
}

void CL_SendClientCommand(qbool reliable, char *format, ...)
{
	va_list		argptr;
	char		string[2048];

	if (cls.demoplayback || cls.state == ca_disconnected) {
		return;	// no point.
	}

	va_start (argptr, format);
	vsnprintf (string, sizeof(string), format, argptr);
	va_end (argptr);

	if (reliable) {
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, string);
	} else {
		MSG_WriteByte (&cls.cmdmsg, clc_stringcmd);
		MSG_WriteString (&cls.cmdmsg, string);
	}
}

#ifdef MVD_PEXT1_DEBUG_ANTILAG
void IN_SendPredictedPlayerPositions(sizebuf_t* buffer)
{
	byte players = 0;
	int i, msec[MAX_CLIENTS];
	vec3_t pos[MAX_CLIENTS];
	qbool present[MAX_CLIENTS];

	if (!(cls.mvdprotocolextensions1 & MVD_PEXT1_DEBUG_ANTILAG)) {
		return;
	}

	for (i = 0; i < MAX_CLIENTS; ++i) {
		if (i == cl.playernum) {
			present[i] = true;
			VectorCopy(cl.simorg, pos[i]);
			msec[i] = 0;
		}
		else {
			present[i] = CL_DrawnPlayerPosition(i, pos[i], &msec[i]);
			msec[i] = bound(0, msec[i], 255);
		}

		if (present[i]) {
			++players;
		}
	}

	MSG_WriteByte(buffer, clc_mvd_debug);
	MSG_WriteByte(buffer, clc_mvd_debug_type_antilag);
	MSG_WriteByte(buffer, players);
	for (i = 0; i < MAX_CLIENTS; ++i) {
		if (present[i]) {
			MSG_WriteByte(buffer, i);
			MSG_WriteByte(buffer, msec[i]);
			MSG_WriteByte(buffer, 0);           // model: for future use to determine the prediction model used
			MSG_WriteFloat(buffer, LittleFloat(pos[i][0]));
			MSG_WriteFloat(buffer, LittleFloat(pos[i][1]));
			MSG_WriteFloat(buffer, LittleFloat(pos[i][2]));
		}
	}
}
#endif

#ifdef MVD_PEXT1_DEBUG_WEAPON
static void IN_SendWeaponSelection(int items, int* stats, int* weapon_list, int weapon_choice)
{
	int i;

	if (!(cls.mvdprotocolextensions1 & MVD_PEXT1_DEBUG_WEAPON)) {
		return;
	}

	MSG_WriteByte(&cls.netchan.message, clc_mvd_debug);
	MSG_WriteByte(&cls.netchan.message, clc_mvd_debug_type_weapon);
	MSG_WriteLong(&cls.netchan.message, LittleLong(items));
	MSG_WriteByte(&cls.netchan.message, min(255, stats[STAT_SHELLS]));
	MSG_WriteByte(&cls.netchan.message, min(255, stats[STAT_NAILS]));
	MSG_WriteByte(&cls.netchan.message, min(255, stats[STAT_ROCKETS]));
	MSG_WriteByte(&cls.netchan.message, min(255, stats[STAT_CELLS]));
	MSG_WriteByte(&cls.netchan.message, weapon_choice);
	for (i = 0; i < MAXWEAPONS; ++i) {
		MSG_WriteByte(&cls.netchan.message, weapon_list[i]);
		if (!weapon_list[i]) {
			break;
		}
	}
	if (i >= MAXWEAPONS) {
		MSG_WriteByte(&cls.netchan.message, 0);
	}
}
#endif

#ifdef MVD_PEXT1_SERVERSIDEWEAPON
void IN_SendServerSideWeaponSwitch(sizebuf_t* buffer)
{
	int i = 0;
	int flags = 0;

	if (!(cls.mvdprotocolextensions1 & MVD_PEXT1_SERVERSIDEWEAPON)) {
		return;
	}

	// Compare our current preferred weaponlist to the server's confirmed list
	if (cls.netchan.incoming_acknowledged >= cl.weapon_order_sequence_set) {
		return;
	}

	// build flags
	if (cl_pext_serversideweapon.integer) {
		int max_impulse = 0;

		if (cls.mvdprotocolextensions1 & MVD_PEXT1_SERVERSIDEWEAPON2) {
			// lookup highest impulse used to see if we need full impulses
			for (i = 0; i < MAXWEAPONS; ++i) {
				max_impulse = max(cl.weapon_order[i], max_impulse);
			}
		}

		flags = MSG_EncodeMVDSVWeaponFlags(cl.deathmatch, cl_weaponpreselect.integer, (suppress_hide ? 0 : cl_weaponhide.integer), cl_weaponhide_axe.integer, cl_weaponforgetorder.integer, cl_weaponforgetondeath.integer, max_impulse);
	}

	// send data
	MSG_WriteByte(buffer, clc_mvd_weapon);
	MSG_WriteByte(buffer, flags);
	if (flags & clc_mvd_weapon_forget_ranking) {
		int offset = min(cls.netchan.outgoing_sequence - cl.weapon_order_sequence_set, 255);
		MSG_WriteByte(buffer, offset);
	}
	if (cl_pext_serversideweapon.integer) {
		if (flags & clc_mvd_weapon_full_impulse) {
			for (i = 0; i < MAXWEAPONS; ++i) {
				MSG_WriteByte(buffer, cl.weapon_order[i]);
				if (!cl.weapon_order[i]) {
					break;
				}
			}
		}
		else {
			for (i = 0; i < MAXWEAPONS; i += 2) {
				int lhs = bound(0, cl.weapon_order[i], 15);
				int rhs = bound(0, cl.weapon_order[i + 1], 15);

				MSG_WriteByte(buffer, (lhs << 4) | rhs);
				if (!lhs || !rhs) {
					break;
				}
			}
		}
	}
	if (i >= MAXWEAPONS || !cl_pext_serversideweapon.integer) {
		MSG_WriteByte(buffer, 0);
	}
}
#endif // MVD_PEXT1_SERVERSIDEWEAPON

int cmdtime_msec = 0;

void CL_SendCmd(void)
{
	sizebuf_t buf;
	byte data[1024];
	usercmd_t *cmd, *oldcmd;
	int i, checksumIndex, lost;
	qbool dontdrop;
	static float pps_balance = 0;
	static int dropcount = 0;

	if (cls.demoplayback && !cls.mvdplayback) {
		CL_CalcNet();
		return; // sendcmds come from the demo
	}

	#ifdef FTE_PEXT_CHUNKEDDOWNLOADS
	CL_SendChunkDownloadReq();
	#endif

	// save this command off for prediction
	i = cls.netchan.outgoing_sequence & UPDATE_MASK;
	cmd = &cl.frames[i].cmd;
	cl.frames[i].senttime = cls.realtime;
	cl.frames[i].receivedtime = -1;		// we haven't gotten a reply yet

	// update network stats table
	i = cls.netchan.outgoing_sequence&NETWORK_STATS_MASK;
	network_stats[i].delta = 0;     // filled-in later
	network_stats[i].sentsize = 0;  // filled-in later
	network_stats[i].senttime = cls.realtime;
	network_stats[i].receivedtime = -1;

	// get basic movement from keyboard
	CL_BaseMove(cmd);

	// allow mice or other external controllers to add to the move

	if (cl_independentPhysics.value == 0 || (physframe && cl_independentPhysics.value != 0)) {
		IN_Move(cmd);
	}

	// if we are spectator, try autocam
	if (cl.spectator) {
		Cam_Track(cmd);
	}

	CL_FinishMove(cmd);
	cmdtime_msec += cmd->msec;

	Cam_FinishMove(cmd);

	if (cls.mvdplayback) {
		CL_CalcPlayerFPS(&cl.players[cl.playernum], cmd->msec);
		cls.netchan.outgoing_sequence++;
		return;
	}

	SZ_Init(&buf, data, sizeof(data));

	SZ_Write(&buf, cls.cmdmsg.data, cls.cmdmsg.cursize);

	if (cls.cmdmsg.overflowed) {
		Com_DPrintf("cls.cmdmsg overflowed\n");
	}

	SZ_Clear(&cls.cmdmsg);

#ifdef MVD_PEXT1_SERVERSIDEWEAPON
	// Send this before the clc_move command
	IN_SendServerSideWeaponSwitch(&buf);
#endif
#ifdef MVD_PEXT1_DEBUG_ANTILAG
	IN_SendPredictedPlayerPositions(&buf);
#endif

	// begin a client move command
	MSG_WriteByte (&buf, clc_move);

	// save the position for a checksum byte
	checksumIndex = buf.cursize;
	MSG_WriteByte (&buf, 0);

	// write our lossage percentage
	lost = CL_CalcNet();
	MSG_WriteByte (&buf, (byte)lost);

	// send this and the previous two cmds in the message, so if the last packet was dropped, it can be recovered
	dontdrop = false;

	i = (cls.netchan.outgoing_sequence - 2) & UPDATE_MASK;
	cmd = &cl.frames[i].cmd;

	if (cl_c2sImpulseBackup.value >= 2) {
		dontdrop = dontdrop || cmd->impulse;
	}

	MSG_WriteDeltaUsercmd (&buf, &nullcmd, cmd, cls.mvdprotocolextensions1);
	oldcmd = cmd;

	i = (cls.netchan.outgoing_sequence - 1) & UPDATE_MASK;
	cmd = &cl.frames[i].cmd;

	if (cl_c2sImpulseBackup.value >= 3) {
		dontdrop = dontdrop || cmd->impulse;
	}

	MSG_WriteDeltaUsercmd (&buf, oldcmd, cmd, cls.mvdprotocolextensions1);
	oldcmd = cmd;

	i = (cls.netchan.outgoing_sequence) & UPDATE_MASK;
	cmd = &cl.frames[i].cmd;
	if (cl_c2sImpulseBackup.value >= 1)
		dontdrop = dontdrop || cmd->impulse;
	MSG_WriteDeltaUsercmd (&buf, oldcmd, cmd, cls.mvdprotocolextensions1);

	// calculate a checksum over the move commands
	buf.data[checksumIndex] = COM_BlockSequenceCRCByte(
		buf.data + checksumIndex + 1, buf.cursize - checksumIndex - 1,
		cls.netchan.outgoing_sequence);

	// request delta compression of entities
	if (cls.netchan.outgoing_sequence - cl.validsequence >= UPDATE_BACKUP - 1) {
		cl.validsequence = 0;
		cl.delta_sequence = 0;
	}

	if (cl.delta_sequence && !cl_nodelta.value && cls.state == ca_active) {
		cl.frames[cls.netchan.outgoing_sequence & UPDATE_MASK].delta_sequence = cl.delta_sequence;
		MSG_WriteByte (&buf, clc_delta);
		MSG_WriteByte (&buf, cl.delta_sequence & 255);

		// network stats table
		network_stats[cls.netchan.outgoing_sequence&NETWORK_STATS_MASK].delta = 1;
	} else {
		cl.frames[cls.netchan.outgoing_sequence & UPDATE_MASK].delta_sequence = -1;
	}

	if (cls.demorecording) {
		CL_WriteDemoCmd (cmd);
	}

	if (cl_c2spps.value) {
		pps_balance += cls.frametime;
		// never drop more than 2 messages in a row -- that'll cause PL
		// and don't drop if one of the last two movemessages have an impulse
		if (pps_balance > 0 || dropcount >= 2 || dontdrop) {
			float	pps;
			pps = cl_c2spps.value;
			if (pps < 10) pps = 10;
			if (pps > 72) pps = 72;
			pps_balance -= 1 / pps;
			// bound pps_balance. FIXME: is there a better way?
			if (pps_balance > 0.1) pps_balance = 0.1;
			if (pps_balance < -0.1) pps_balance = -0.1;
			dropcount = 0;
		} else {
			// don't count this message when calculating PL
			cl.frames[i].receivedtime = -3;
			// drop this message
			cls.netchan.outgoing_sequence++;
			dropcount++;
			return;
		}
	} else {
		pps_balance = 0;
		dropcount = 0;
	}

#ifdef FTE_PEXT2_VOICECHAT
	S_Voip_Transmit(clc_voicechat, &buf);
#endif

	cl.frames[cls.netchan.outgoing_sequence&UPDATE_MASK].sentsize = buf.cursize + 8;    // 8 = PACKET_HEADER
	// network stats table
	network_stats[cls.netchan.outgoing_sequence&NETWORK_STATS_MASK].sentsize = buf.cursize + 8;

	//send duplicated packets, if set
	cls.netchan.dupe = bound(0, cl_c2sdupe.value, MAX_DUPLICATE_PACKETS);

	// deliver the message
	Netchan_Transmit (&cls.netchan, buf.cursize, buf.data);
}

void CL_InitInput(void)
{
	Cmd_AddCommand("+moveup",IN_UpDown);
	Cmd_AddCommand("-moveup",IN_UpUp);
	Cmd_AddCommand("+movedown",IN_DownDown);
	Cmd_AddCommand("-movedown",IN_DownUp);
	Cmd_AddCommand("+left",IN_LeftDown);
	Cmd_AddCommand("-left",IN_LeftUp);
	Cmd_AddCommand("+right",IN_RightDown);
	Cmd_AddCommand("-right",IN_RightUp);
	Cmd_AddCommand("+forward",IN_ForwardDown);
	Cmd_AddCommand("-forward",IN_ForwardUp);
	Cmd_AddCommand("+back",IN_BackDown);
	Cmd_AddCommand("-back",IN_BackUp);
	Cmd_AddCommand("+lookup", IN_LookupDown);
	Cmd_AddCommand("-lookup", IN_LookupUp);
	Cmd_AddCommand("+lookdown", IN_LookdownDown);
	Cmd_AddCommand("-lookdown", IN_LookdownUp);
	Cmd_AddCommand("+strafe", IN_StrafeDown);
	Cmd_AddCommand("-strafe", IN_StrafeUp);
	Cmd_AddCommand("+moveleft", IN_MoveleftDown);
	Cmd_AddCommand("-moveleft", IN_MoveleftUp);
	Cmd_AddCommand("+moveright", IN_MoverightDown);
	Cmd_AddCommand("-moveright", IN_MoverightUp);
	Cmd_AddCommand("+speed", IN_SpeedDown);
	Cmd_AddCommand("-speed", IN_SpeedUp);
	Cmd_AddCommand("+attack", IN_AttackDown);
	Cmd_AddCommand("-attack", IN_AttackUp);
	Cmd_AddCommand("+fire", IN_FireDown);
	Cmd_AddCommand("-fire", IN_FireUp);
	Cmd_AddCommand("+attack2", IN_Attack2Down);
	Cmd_AddCommand("-attack2", IN_Attack2Up);
	Cmd_AddCommand("+use", IN_UseDown);
	Cmd_AddCommand("-use", IN_UseUp);
	Cmd_AddCommand("+jump", IN_JumpDown);
	Cmd_AddCommand("-jump", IN_JumpUp);
	Cmd_AddCommand("impulse", IN_Impulse);
	Cmd_AddCommand("weapon", IN_Weapon);
	Cmd_AddCommand("+klook", IN_KLookDown);
	Cmd_AddCommand("-klook", IN_KLookUp);
	Cmd_AddCommand("+mlook", IN_MLookDown);
	Cmd_AddCommand("-mlook", IN_MLookUp);
	Cmd_AddCommand ("rotate",CL_Rotate_f);

	Cvar_SetCurrentGroup(CVAR_GROUP_INPUT_KEYBOARD);

	Cvar_Register(&cl_smartjump);
	Cvar_Register(&cl_weaponhide);
	Cvar_Register(&cl_weaponpreselect);
	Cvar_Register(&cl_weaponforgetorder);
	Cvar_Register(&cl_weaponforgetondeath);
	Cvar_Register(&cl_weaponhide_axe);
	Cvar_Register(&cl_upspeed);
	Cvar_Register(&cl_forwardspeed);
	Cvar_Register(&cl_backspeed);
	Cvar_Register(&cl_sidespeed);
	Cvar_Register(&cl_movespeedkey);
	Cvar_Register(&cl_yawspeed);
	Cvar_Register(&cl_pitchspeed);
	Cvar_Register(&cl_anglespeedkey);
	Cvar_Register(&cl_iDrive);

	Cvar_SetCurrentGroup(CVAR_GROUP_INPUT_MISC);
	Cvar_Register(&lookspring);
	Cvar_Register(&lookstrafe);
	Cvar_Register(&sensitivity);
	Cvar_Register(&freelook);
	
	Cvar_SetCurrentGroup(CVAR_GROUP_MENU);
	Cvar_Register(&cursor_sensitivity);

	Cvar_SetCurrentGroup(CVAR_GROUP_INPUT_MOUSE);
	Cvar_Register(&m_pitch);
	Cvar_Register(&m_yaw);
	Cvar_Register(&m_forward);
	Cvar_Register(&m_side);
	Cvar_Register(&m_accel);
	Cvar_Register(&m_accel_power);
	Cvar_Register(&m_accel_offset);
	Cvar_Register(&m_accel_senscap);

	Cvar_SetCurrentGroup(CVAR_GROUP_NETWORK);
	Cvar_Register(&cl_nodelta);
	Cvar_Register(&cl_c2sImpulseBackup);
	Cvar_Register(&cl_c2spps);
	Cvar_Register(&cl_c2sdupe);

	Cvar_ResetCurrentGroup();

	#ifdef JSS_CAM
	Cvar_SetCurrentGroup(CVAR_GROUP_SPECTATOR);
	Cvar_Register(&cam_zoomspeed);
	Cvar_Register(&cam_zoomaccel);
	Cvar_ResetCurrentGroup();
	#endif // JSS_CAM
}

void IN_ClearProtectedKeys (void)
{
	// Pretend the user entered -moveleft etc at the console
	KeyUp_common(&in_up, VOID_KEY);
	KeyUp_common(&in_down, VOID_KEY);
	KeyUp_common(&in_left, VOID_KEY);
	KeyUp_common(&in_right, VOID_KEY);
	KeyUp_common(&in_forward, VOID_KEY);
	KeyUp_common(&in_back, VOID_KEY);
	KeyUp_common(&in_lookup, VOID_KEY);
	KeyUp_common(&in_lookdown, VOID_KEY);
	KeyUp_common(&in_moveleft, VOID_KEY);
	KeyUp_common(&in_moveright, VOID_KEY);
}

void onchange_pext_serversideweapon(cvar_t* var, char* value, qbool* cancel)
{
	cl.weapon_order_sequence_set = cls.netchan.outgoing_sequence;
}

void IN_ServerSideWeaponSelectionResponse(const char* s)
{
	if ((cls.mvdprotocolextensions1 & MVD_PEXT1_SERVERSIDEWEAPON) && cl_pext_serversideweapon.integer) {
		if (cl_weaponforgetorder.integer) {
			int sequence_set, best_impulse;

			Cmd_TokenizeString(s);

			// expected args: <sequence-set> <best-weapon-impulse>
			if (Cmd_Argc() < 2) {
				return;
			}

			sequence_set = atoi(Cmd_Argv(0));
			best_impulse = atoi(Cmd_Argv(1));

			if (sequence_set == cl.weapon_order_sequence_set && best_impulse) {
				Con_DPrintf("Confirmed best selection: %d vs %d\n", best_impulse, cl.weapon_order[0]);
				cl.weapon_order[0] = best_impulse;
				cl.weapon_order_use_clientside = false;
			}
			else if (sequence_set != cl.weapon_order_sequence_set) {
				Con_DPrintf("Out of date response: %d vs %d\n", sequence_set, cl.weapon_order_sequence_set);
			}
		}
		else {
			cl.weapon_order_use_clientside = false;
		}
	}
}
