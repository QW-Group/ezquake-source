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
// pmove_weapon.c -- antilag 1 client-side weapon prediction
//
// A QuakeC-style re-implementation of the standard QW weapon logic
// (attack timing, ammo, weapon switching, view weapon animation), driven
// from CL_PredictUsercmd for the local player when the server supports
// MVD_PEXT1_WEAPONPREDICTION. Effects are not applied directly; they are
// queued as prediction events and played back by CL_PlayEvents.

#ifdef MVD_PEXT1_WEAPONPREDICTION

#include "quakedef.h"
#include "pmove.h"
#include "qsound.h"
#include "keys.h"

#define IT_HOOK 32768

int pmove_nopred_weapon;

// Queue a predicted sound
void PM_SoundEffect(sfx_t *sample, int chan)
{
	prediction_event_sound_t *s_event = Q_malloc(sizeof(prediction_event_sound_t));

	s_event->chan = chan;
	s_event->sample = sample;
	s_event->vol = 1;
	s_event->frame_num = pmove.frame_current;
	s_event->next = p_event_sound;
	p_event_sound = s_event;
}

// Queue a predicted weapon sound unless filtered out by the
// cl_predict_weaponsound bitmask (weap is the weapon's filter bit)
static void PM_SoundEffect_Weapon(sfx_t *sample, int chan, int weap)
{
	if (cl_predict_weaponsound.integer == 0)
		return;
	if (cl_predict_weaponsound.integer & weap)
		return;

	PM_SoundEffect(sample, chan);
}

static prediction_event_fakeproj_t *PM_AddEvent_FakeProj(int type)
{
	prediction_event_fakeproj_t *proj;

	if (type == IT_LIGHTNING) {
		if (!cl_predict_beam.integer)
			return NULL;
	}
	else {
		if (!cl_predict_projectiles.integer)
			return NULL;
	}

	proj = Q_malloc(sizeof(prediction_event_fakeproj_t));
	proj->type = type;
	proj->frame_num = pmove.frame_current;
	proj->next = p_event_fakeproj;
	p_event_fakeproj = proj;
	return proj;
}

// Should the server sound be replaced by its predicted counterpart,
// given the cl_predict_weaponsound bitmask?
int PM_FilterWeaponSound(byte sound_num)
{
	const char *name = cl.sound_precache[sound_num]->name;

	if (cl_predict_weaponsound.integer & 2)
		if (strcmp(name, "weapons/ax1.wav") == 0)
			return false;
	if (cl_predict_weaponsound.integer & 4)
		if (strcmp(name, "weapons/guncock.wav") == 0)
			return false;
	if (cl_predict_weaponsound.integer & 8)
		if (strcmp(name, "weapons/shotgn2.wav") == 0)
			return false;
	if (cl_predict_weaponsound.integer & 16)
		if (strcmp(name, "weapons/rocket1i.wav") == 0)
			return false;
	if (cl_predict_weaponsound.integer & 32)
		if (strcmp(name, "weapons/spike2.wav") == 0)
			return false;
	if (cl_predict_weaponsound.integer & 64)
		if (strcmp(name, "weapons/grenade.wav") == 0)
			return false;
	if (cl_predict_weaponsound.integer & 128)
		if (strcmp(name, "weapons/sgun1.wav") == 0)
			return false;
	if (cl_predict_weaponsound.integer & 256)
	{
		if (strcmp(name, "weapons/lstart.wav") == 0)
			return false;
		if (strcmp(name, "weapons/lhit.wav") == 0)
			return false;
	}

	return true;
}

static void W_SetCurrentAmmo(void)
{
	switch (pmove.weapon)
	{
		case IT_AXE: {
			pmove.current_ammo = 0;
			pmove.weapon_index = 1;
		} break;
		case IT_SHOTGUN: {
			pmove.current_ammo = pmove.ammo_shells;
			if (pmove.client_predflags & PRDFL_COILGUN)
				pmove.weapon_index = 9;
			else
				pmove.weapon_index = 2;
		} break;
		case IT_SUPER_SHOTGUN: {
			pmove.current_ammo = pmove.ammo_shells;
			pmove.weapon_index = 3;
		} break;
		case IT_NAILGUN: {
			pmove.current_ammo = pmove.ammo_nails;
			pmove.weapon_index = 4;
		} break;
		case IT_SUPER_NAILGUN: {
			pmove.current_ammo = pmove.ammo_nails;
			pmove.weapon_index = 5;
		} break;
		case IT_GRENADE_LAUNCHER: {
			pmove.current_ammo = pmove.ammo_rockets;
			pmove.weapon_index = 6;
		} break;
		case IT_ROCKET_LAUNCHER: {
			pmove.current_ammo = pmove.ammo_rockets;
			pmove.weapon_index = 7;
		} break;
		case IT_LIGHTNING: {
			pmove.current_ammo = pmove.ammo_cells;
			pmove.weapon_index = 8;
		} break;
		case IT_HOOK: {
			pmove.current_ammo = 0;
			pmove.weapon_index = 10;
		} break;
	}
}

static int W_BestWeapon(void)
{
	int it = pmove.items;
	char *w_rank = Info_ValueForKey(cls.userinfo, "w_rank");

	if (w_rank != NULL)
	{
		while (*w_rank)
		{
			int weapon = (*w_rank - '0');

			if ((weapon == 8) && (it & IT_LIGHTNING) && (pmove.ammo_cells >= 1))
				return IT_LIGHTNING;
			if ((weapon == 7) && (it & IT_ROCKET_LAUNCHER) && (pmove.ammo_rockets >= 1))
				return IT_ROCKET_LAUNCHER;
			if ((weapon == 6) && (it & IT_GRENADE_LAUNCHER) && (pmove.ammo_rockets >= 1))
				return IT_GRENADE_LAUNCHER;
			if ((weapon == 5) && (it & IT_SUPER_NAILGUN) && (pmove.ammo_nails >= 2))
				return IT_SUPER_NAILGUN;
			if ((weapon == 4) && (it & IT_NAILGUN) && (pmove.ammo_nails >= 1))
				return IT_NAILGUN;
			if ((weapon == 3) && (it & IT_SUPER_SHOTGUN) && (pmove.ammo_shells >= 2))
				return IT_SUPER_SHOTGUN;
			if ((weapon == 2) && (it & IT_SHOTGUN) && (pmove.ammo_shells >= 1))
				return IT_SHOTGUN;
			if ((weapon == 1) && (it & IT_AXE))
				return IT_AXE;

			++w_rank;
		}

		if (it & IT_AXE)
			return IT_AXE;

		return 0;
	}

	// standard behaviour
	if ((pmove.waterlevel <= 1) && (pmove.ammo_cells >= 1) && (it & IT_LIGHTNING))
		return IT_LIGHTNING;
	else if ((pmove.ammo_nails >= 2) && (it & IT_SUPER_NAILGUN))
		return IT_SUPER_NAILGUN;
	else if ((pmove.ammo_shells >= 2) && (it & IT_SUPER_SHOTGUN))
		return IT_SUPER_SHOTGUN;
	else if ((pmove.ammo_nails >= 1) && (it & IT_NAILGUN))
		return IT_NAILGUN;
	else if ((pmove.ammo_shells >= 1) && (it & IT_SHOTGUN))
		return IT_SHOTGUN;

	return (it & IT_AXE ? IT_AXE : 0);
}

static int W_CheckNoAmmo(void)
{
	if (pmove.current_ammo > 0)
		return true;

	if ((pmove.weapon == IT_AXE) || (pmove.weapon == IT_HOOK))
		return true;

	// drop the weapon down
	pmove.weapon = W_BestWeapon();
	W_SetCurrentAmmo();

	return false;
}

// Go to the next weapon with ammo
static int CycleWeaponCommand(void)
{
	int i, it, am;

	if (pmove.client_time < pmove.attack_finished)
		return false;

	it = pmove.items;

	for (i = 0; i < 20; i++) // qqshka, 20 is just from head, but prevent infinite loop
	{
		am = 0;
		switch (pmove.weapon)
		{
		case IT_LIGHTNING:
			pmove.weapon = IT_AXE;
			break;

		case IT_AXE:
			pmove.weapon = IT_HOOK;
			break;

		case IT_HOOK:
			pmove.weapon = IT_SHOTGUN;
			if (pmove.ammo_shells < 1)
				am = 1;
			break;

		case IT_SHOTGUN:
			pmove.weapon = IT_SUPER_SHOTGUN;
			if (pmove.ammo_shells < 2)
				am = 1;
			break;

		case IT_SUPER_SHOTGUN:
			pmove.weapon = IT_NAILGUN;
			if (pmove.ammo_nails < 1)
				am = 1;
			break;

		case IT_NAILGUN:
			pmove.weapon = IT_SUPER_NAILGUN;
			if (pmove.ammo_nails < 2)
				am = 1;
			break;

		case IT_SUPER_NAILGUN:
			pmove.weapon = IT_GRENADE_LAUNCHER;
			if (pmove.ammo_rockets < 1)
				am = 1;
			break;

		case IT_GRENADE_LAUNCHER:
			pmove.weapon = IT_ROCKET_LAUNCHER;
			if (pmove.ammo_rockets < 1)
				am = 1;
			break;

		case IT_ROCKET_LAUNCHER:
			pmove.weapon = IT_LIGHTNING;
			if (pmove.ammo_cells < 1)
				am = 1;
			break;
		}

		if ((it & pmove.weapon) && (am == 0))
		{
			W_SetCurrentAmmo();
			return true;
		}
	}

	return true;
}

// Go to the prev weapon with ammo
static int CycleWeaponReverseCommand(void)
{
	int i, it, am;

	if (pmove.client_time < pmove.attack_finished)
		return false;

	it = pmove.items;

	for (i = 0; i < 20; i++) // qqshka, 20 is just from head, but prevent infinite loop
	{
		am = 0;
		switch (pmove.weapon)
		{
		case IT_LIGHTNING:
			pmove.weapon = IT_ROCKET_LAUNCHER;
			if (pmove.ammo_rockets < 1)
				am = 1;
			break;

		case IT_ROCKET_LAUNCHER:
			pmove.weapon = IT_GRENADE_LAUNCHER;
			if (pmove.ammo_rockets < 1)
				am = 1;
			break;

		case IT_GRENADE_LAUNCHER:
			pmove.weapon = IT_SUPER_NAILGUN;
			if (pmove.ammo_nails < 2)
				am = 1;
			break;

		case IT_SUPER_NAILGUN:
			pmove.weapon = IT_NAILGUN;
			if (pmove.ammo_nails < 1)
				am = 1;
			break;

		case IT_NAILGUN:
			pmove.weapon = IT_SUPER_SHOTGUN;
			if (pmove.ammo_shells < 2)
				am = 1;
			break;

		case IT_SUPER_SHOTGUN:
			pmove.weapon = IT_SHOTGUN;
			if (pmove.ammo_shells < 1)
				am = 1;
			break;

		case IT_SHOTGUN:
			pmove.weapon = IT_HOOK;
			break;

		case IT_HOOK:
			pmove.weapon = IT_AXE;
			break;

		case IT_AXE:
			pmove.weapon = IT_LIGHTNING;
			if (pmove.ammo_cells < 1)
				am = 1;
			break;
		}

		if ((it & pmove.weapon) && (am == 0))
		{
			W_SetCurrentAmmo();
			return true;
		}
	}

	return true;
}

static int W_ChangeWeapon(int impulse)
{
	int fl = 0;
	int am = 0;

	switch (impulse)
	{
		case 1: {
			fl = IT_AXE;
			if (pmove.items & IT_HOOK && pmove.weapon == IT_AXE)
				fl = IT_HOOK;

			am = 1;
		} break;

		case 2: {
			fl = IT_SHOTGUN;
			if (pmove.ammo_shells >= 1)
				am = 1;
		} break;

		case 3: {
			fl = IT_SUPER_SHOTGUN;
			if (pmove.ammo_shells >= 2)
				am = 1;
		} break;

		case 4: {
			fl = IT_NAILGUN;
			if (pmove.ammo_nails >= 1)
				am = 1;
		} break;

		case 5: {
			fl = IT_SUPER_NAILGUN;
			if (pmove.ammo_nails >= 2)
				am = 1;
		} break;

		case 6: {
			fl = IT_GRENADE_LAUNCHER;
			if (pmove.ammo_rockets >= 1)
				am = 1;
		} break;

		case 7: {
			fl = IT_ROCKET_LAUNCHER;
			if (pmove.ammo_rockets >= 1)
				am = 1;
		} break;

		case 8: {
			fl = IT_LIGHTNING;
			if (pmove.ammo_cells >= 1)
				am = 1;
		} break;

		case 22: {
			fl = IT_HOOK;
			am = 1;
		} break;
	}

	if (pmove.items & fl && am)
	{
		if (pmove.weapon != fl)
			pmove.weaponframe = 0;

		pmove.weapon = fl;
		W_SetCurrentAmmo();
	}

	return true;
}

static void ImpulseCommands(void)
{
	int clear = false;

	if (((pmove.impulse >= 1) && (pmove.impulse <= 8)) || (pmove.impulse == 22))
		clear = W_ChangeWeapon(pmove.impulse);
	else if (pmove.impulse == 10)
		clear = CycleWeaponCommand();
	else if (pmove.impulse == 12)
		clear = CycleWeaponReverseCommand();

	if (clear)
		pmove.impulse = 0;
}

static void launch_spike(float off)
{
	prediction_event_fakeproj_t *newmis;

	if (pmove.weapon == IT_SUPER_NAILGUN)
	{
		newmis = PM_AddEvent_FakeProj(IT_SUPER_NAILGUN);
		PM_SoundEffect_Weapon(cl_sfx_sng, 1, 32);
		off = 0;
	}
	else
	{
		newmis = PM_AddEvent_FakeProj(IT_NAILGUN);
		PM_SoundEffect_Weapon(cl_sfx_ng, 1, 16);
	}

	if (newmis)
	{
		vec3_t forward, right;
		AngleVectors(pmove.cmd.angles, forward, right, NULL);
		VectorScale(forward, 1000, newmis->velocity);

		VectorCopy(pmove.origin, newmis->origin);
		newmis->origin[0] += (forward[0] * 8) + (right[0] * off);
		newmis->origin[1] += (forward[1] * 8) + (right[1] * off);
		newmis->origin[2] += 16 + forward[2] * 8;
		VectorCopy(pmove.cmd.angles, newmis->angles);
		newmis->angles[0] = -newmis->angles[0];
	}
}

static void player_run(void)
{
	pmove.client_nextthink = 0;
	pmove.client_thinkindex = 0;
	pmove.weaponframe = 0;
}

static void anim_axe(void)
{
	pmove.client_nextthink = pmove.client_time + 0.1;
	pmove.weaponframe++;
	pmove.client_thinkindex++;
	if (pmove.client_thinkindex > 4)
		pmove.client_thinkindex = 0;
}

static void player_nail1(void)
{
	pmove.client_nextthink = pmove.client_time + 0.1;
	pmove.client_thinkindex = 2;

	if (!(pmove.cmd.buttons & 1) || pmove.impulse)
	{
		player_run();
		return;
	}

	pmove.weaponframe = pmove.weaponframe + 1;
	if (pmove.weaponframe >= 9)
		pmove.weaponframe = 1;

	pmove.current_ammo = pmove.ammo_nails -= 1;
	pmove.attack_finished = pmove.client_time + 0.2;
	launch_spike(4);
}

static void player_nail2(void)
{
	pmove.client_nextthink = pmove.client_time + 0.1;
	pmove.client_thinkindex = 1;

	if (!(pmove.cmd.buttons & 1) || pmove.impulse)
	{
		player_run();
		return;
	}

	pmove.weaponframe = pmove.weaponframe + 1;
	if (pmove.weaponframe >= 9)
		pmove.weaponframe = 1;

	pmove.current_ammo = pmove.ammo_nails -= 1;
	pmove.attack_finished = pmove.client_time + 0.2;
	launch_spike(-4);
}

static void anim_nailgun(void)
{
	if (pmove.client_thinkindex < 2)
		player_nail1();
	else
		player_nail2();
}

static void player_light_fire(void)
{
	pmove.weaponframe = pmove.weaponframe + 1;
	if (pmove.weaponframe >= 5)
		pmove.weaponframe = 1;

	pmove.attack_finished = pmove.client_time + 0.2;

	pmove.current_ammo = pmove.ammo_cells -= 1;
	if (pmove.waterlevel <= 1)
	{
		prediction_event_fakeproj_t *beam = PM_AddEvent_FakeProj(IT_LIGHTNING);
		if (beam)
		{
			VectorCopy(pmove.origin, beam->origin);
			beam->origin[2] += 16;

			VectorCopy(pmove.cmd.angles, beam->angles);
		}
	}
}

static void player_light1(void)
{
	pmove.client_nextthink = pmove.client_time + 0.1;
	pmove.client_thinkindex = 2;

	if (!(pmove.cmd.buttons & 1) || pmove.impulse)
	{
		player_run();
		return;
	}

	player_light_fire();
}

static void player_light2(void)
{
	pmove.client_nextthink = pmove.client_time + 0.1;
	pmove.client_thinkindex = 1;

	if (!(pmove.cmd.buttons & 1) || pmove.impulse)
	{
		player_run();
		return;
	}

	player_light_fire();
}

static void anim_lightning(void)
{
	if (pmove.client_thinkindex < 2)
		player_light1();
	else
		player_light2();
}

static void anim_rocket(void)
{
	pmove.client_nextthink = pmove.client_time + 0.1;
	pmove.weaponframe = pmove.client_thinkindex;
	pmove.client_thinkindex++;
	if (pmove.client_thinkindex > 6)
		pmove.client_thinkindex = 0;
}

static void anim_shotgun(void)
{
	pmove.client_nextthink = pmove.client_time + 0.1;
	pmove.weaponframe = pmove.client_thinkindex;
	pmove.client_thinkindex++;
	if (pmove.client_thinkindex > 6)
		pmove.client_thinkindex = 0;
}

static void execute_clientthink(void)
{
	if (pmove.client_thinkindex == 0)
	{
		player_run();
		return;
	}

	switch (pmove.weapon)
	{
	case IT_AXE: {
		anim_axe();
	} break;
	case IT_SHOTGUN: {
		anim_shotgun();
	} break;
	case IT_SUPER_SHOTGUN: {
		anim_shotgun();
	} break;
	case IT_NAILGUN: {
		anim_nailgun();
	} break;
	case IT_SUPER_NAILGUN: {
		anim_nailgun();
	} break;
	case IT_GRENADE_LAUNCHER: {
		anim_rocket();
	} break;
	case IT_ROCKET_LAUNCHER: {
		anim_rocket();
	} break;
	case IT_LIGHTNING: {
		anim_lightning();
	} break;
	case IT_HOOK: {
		anim_shotgun();
	} break;
	}
}

static void W_Attack(void)
{
	if (!W_CheckNoAmmo())
		return;

	switch (pmove.weapon)
	{
	case IT_AXE: {
		int temp;
		float r;

		pmove.attack_finished = pmove.client_time + 0.5;
		PM_SoundEffect_Weapon(cl_sfx_ax1, 1, 2);

		// crude deterministic substitute for the server's random() swing choice
		temp = (((int)(pmove.client_time * 931.75) << 11) + ((int)(pmove.client_time) >> 6)) % 1000;
		r = abs(temp) / 1000.0;
		if (r < 0.25)
			pmove.weaponframe = 0;
		else if (r < 0.5)
			pmove.weaponframe = 4;
		else if (r < 0.75)
			pmove.weaponframe = 0;
		else
			pmove.weaponframe = 4;

		pmove.client_thinkindex = 1;
		anim_axe();
	} break;
	case IT_SHOTGUN: {
		if (pmove.client_predflags & PRDFL_COILGUN)
		{
			if (pmove.client_predflags & PRDFL_MIDAIR)
				pmove.attack_finished = pmove.client_time + 0.7;
			else
				pmove.attack_finished = pmove.client_time + 0.5;
			PM_SoundEffect_Weapon(cl_sfx_coil, 1, 4);
		}
		else
		{
			pmove.current_ammo = pmove.ammo_shells -= 1;
			pmove.attack_finished = pmove.client_time + 0.5;
			PM_SoundEffect_Weapon(cl_sfx_sg, 1, 4);
		}

		pmove.client_thinkindex = 1;
		anim_shotgun();
	} break;
	case IT_SUPER_SHOTGUN: {
		pmove.attack_finished = pmove.client_time + 0.7;
		PM_SoundEffect_Weapon(cl_sfx_ssg, 1, 8);
		pmove.current_ammo = pmove.ammo_shells -= 1;
		pmove.client_thinkindex = 1;
		anim_shotgun();
	} break;
	case IT_NAILGUN: {
		anim_nailgun();
	} break;
	case IT_SUPER_NAILGUN: {
		anim_nailgun();
	} break;
	case IT_GRENADE_LAUNCHER: {
		prediction_event_fakeproj_t *newmis;

		pmove.attack_finished = pmove.client_time + 0.6;
		pmove.current_ammo = pmove.ammo_rockets -= 1;
		PM_SoundEffect_Weapon(cl_sfx_gl, 1, 64);

		newmis = PM_AddEvent_FakeProj(IT_GRENADE_LAUNCHER);
		if (newmis)
		{
			vec3_t forward, right, up;
			AngleVectors(pmove.cmd.angles, forward, right, up);

			newmis->velocity[0] = forward[0] * 600 + up[0] * 200;
			newmis->velocity[1] = forward[1] * 600 + up[1] * 200;
			newmis->velocity[2] = forward[2] * 600 + up[2] * 200;

			VectorCopy(pmove.origin, newmis->origin);

			vectoangles(newmis->velocity, newmis->angles);
			VectorSet(newmis->avelocity, 300, 300, 300);
		}

		pmove.client_thinkindex = 1;
		anim_rocket();
	} break;
	case IT_ROCKET_LAUNCHER: {
		prediction_event_fakeproj_t *newmis;

		pmove.attack_finished = pmove.client_time + 0.8;
		pmove.current_ammo = pmove.ammo_rockets -= 1;
		PM_SoundEffect_Weapon(cl_sfx_rl, 1, 128);

		newmis = PM_AddEvent_FakeProj(IT_ROCKET_LAUNCHER);
		if (newmis)
		{
			vec3_t forward;
			AngleVectors(pmove.cmd.angles, forward, NULL, NULL);

			if (pmove.client_predflags & PRDFL_MIDAIR)
			{
				VectorScale(forward, 2000, newmis->velocity);
			}
			else
			{
				VectorScale(forward, 1000, newmis->velocity);
			}

			VectorCopy(pmove.origin, newmis->origin);
			newmis->origin[0] += forward[0] * 8;
			newmis->origin[1] += forward[1] * 8;
			newmis->origin[2] += 16 + forward[2] * 8;

			VectorCopy(pmove.cmd.angles, newmis->angles);
			newmis->angles[0] = -newmis->angles[0];
		}

		pmove.client_thinkindex = 1;
		anim_rocket();
	} break;
	case IT_LIGHTNING: {
		PM_SoundEffect_Weapon(cl_sfx_lg, 0, 256);
		anim_lightning();
	} break;
	case IT_HOOK: {
		pmove.attack_finished = pmove.client_time + 0.1;
	} break;
	}
}

void PM_PlayerWeapon(void)
{
	// run this regardless because it sets our model, don't want any ugly prediction errors
	W_SetCurrentAmmo();

	if (pmove.pm_type == PM_DEAD || pmove.pm_type == PM_NONE || pmove.pm_type == PM_LOCK)
	{
		pmove.impulse = 0;
		pmove.attack_finished = pmove.client_time + 0.05;
		pmove.effect_frame = 0;
		W_SetCurrentAmmo();
		return;
	}

	pmove.client_time += (float)pmove.cmd.msec / 1000;

	if (pmove.cmd.impulse_pred)
		pmove.impulse = pmove.cmd.impulse_pred;

	if ((pmove.client_time > pmove.attack_finished) && (pmove.current_ammo == 0)
		&& (pmove.weapon != IT_AXE) && (pmove.weapon != IT_HOOK))
	{
		pmove.weapon = W_BestWeapon();
		W_SetCurrentAmmo();
	}

	if (pmove.client_nextthink && pmove.client_time >= pmove.client_nextthink)
	{
		float held_client_time = pmove.client_time;

		pmove.client_time = pmove.client_nextthink;
		pmove.client_nextthink = 0;
		execute_clientthink();
		pmove.client_time = held_client_time;
	}

	if (pmove.client_time >= pmove.attack_finished)
	{
		ImpulseCommands();

		if (pmove.cmd.buttons & 1 && key_dest == key_game)
		{
			W_Attack();
			W_SetCurrentAmmo();
		}
	}
}

#endif // MVD_PEXT1_WEAPONPREDICTION
