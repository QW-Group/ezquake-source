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
#include "input.h"
#include "pmove.h"		
#include "teamplay.h"	
#include "movie.h"

cvar_t	cl_nodelta = {"cl_nodelta","0"};
cvar_t	cl_c2spps = {"cl_c2spps","0"};
cvar_t	cl_c2sImpulseBackup = {"cl_c2sImpulseBackup","3"};

cvar_t	cl_smartjump = {"cl_smartjump", "0"};


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

kbutton_t	in_mlook, in_klook;
kbutton_t	in_left, in_right, in_forward, in_back;
kbutton_t	in_lookup, in_lookdown, in_moveleft, in_moveright;
kbutton_t	in_strafe, in_speed, in_use, in_jump, in_attack;
kbutton_t	in_up, in_down;

int			in_impulse;

void KeyDown (kbutton_t *b) {
	int k;
	char *c;
	
	c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
		k = -1;		// typed manually at the console for continuous down

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
}

void KeyUp (kbutton_t *b) {
	int k;
	char *c;
	
	c = Cmd_Argv(1);
	if (c[0]) {
		k = atoi(c);
	} else { // typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		b->state = 4;	// impulse up
		return;
	}

	if (b->down[0] == k)
		b->down[0] = 0;
	else if (b->down[1] == k)
		b->down[1] = 0;
	else
		return;		// key up without coresponding down (menu pass through)

	if (b->down[0] || b->down[1])
		return;		// some other key is still holding it down

	if (!(b->state & 1))
		return;		// still up (this should not happen)
	b->state &= ~1;		// now up
	b->state |= 4; 		// impulse up
}

void IN_KLookDown (void) {KeyDown(&in_klook);}
void IN_KLookUp (void) {KeyUp(&in_klook);}

void IN_MLookDown (void) {KeyDown(&in_mlook);}
void IN_MLookUp (void) {
	KeyUp(&in_mlook);
	if (!mlook_active && lookspring.value)
		V_StartPitchDrift();
}

void IN_UpDown(void) {KeyDown(&in_up);}
void IN_UpUp(void) {KeyUp(&in_up);}
void IN_DownDown(void) {KeyDown(&in_down);}
void IN_DownUp(void) {KeyUp(&in_down);}
void IN_LeftDown(void) {KeyDown(&in_left);}
void IN_LeftUp(void) {KeyUp(&in_left);}
void IN_RightDown(void) {KeyDown(&in_right);}
void IN_RightUp(void) {KeyUp(&in_right);}
void IN_ForwardDown(void) {KeyDown(&in_forward);}
void IN_ForwardUp(void) {KeyUp(&in_forward);}
void IN_BackDown(void) {KeyDown(&in_back);}
void IN_BackUp(void) {KeyUp(&in_back);}
void IN_LookupDown(void) {KeyDown(&in_lookup);}
void IN_LookupUp(void) {KeyUp(&in_lookup);}
void IN_LookdownDown(void) {KeyDown(&in_lookdown);}
void IN_LookdownUp(void) {KeyUp(&in_lookdown);}
void IN_MoveleftDown(void) {KeyDown(&in_moveleft);}
void IN_MoveleftUp(void) {KeyUp(&in_moveleft);}
void IN_MoverightDown(void) {KeyDown(&in_moveright);}
void IN_MoverightUp(void) {KeyUp(&in_moveright);}

void IN_SpeedDown(void) {KeyDown(&in_speed);}
void IN_SpeedUp(void) {KeyUp(&in_speed);}
void IN_StrafeDown(void) {KeyDown(&in_strafe);}
void IN_StrafeUp(void) {KeyUp(&in_strafe);}

void IN_AttackDown(void) {KeyDown(&in_attack);}
void IN_AttackUp(void) {KeyUp(&in_attack);}

void IN_UseDown (void) {KeyDown(&in_use);}
void IN_UseUp (void) {KeyUp(&in_use);}


void IN_JumpDown(void) {
	qboolean condition;

	condition = (cls.state == ca_active && cl_smartjump.value);
	if (condition && cl.stats[STAT_HEALTH] > 0 && !cls.demoplayback && !cl.spectator && 
		cl.waterlevel >= 2 && (!cl.teamfortress || !(in_forward.state & 1))
	)
		KeyDown(&in_up);
	else if (condition && cl.spectator && Cam_TrackNum() == -1)
		KeyDown(&in_up);
	else
		KeyDown(&in_jump);
}

void IN_JumpUp(void) {
	if (cl_smartjump.value)
		KeyUp(&in_up);
	KeyUp(&in_jump);
}



//Tonik void IN_Impulse (void) {in_impulse=Q_atoi(Cmd_Argv(1));}

// Tonik -->
void IN_Impulse (void) {
	int best, i, imp, items;

	in_impulse = Q_atoi(Cmd_Argv(1));

	if (Cmd_Argc() <= 2)
		return;

	items = cl.stats[STAT_ITEMS];
	best = 0;

	for (i = Cmd_Argc() - 1; i > 0; i--) {
		imp = Q_atoi(Cmd_Argv(i));
		if (imp < 1 || imp > 8)
			continue;

		switch (imp) {
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
	
	if (best)
		in_impulse = best;
}
// <-- Tonik

/*
Returns 0.25 if a key was pressed and released during the frame,
0.5 if it was pressed and held
0 if held then released, and
1.0 if held for the entire time
*/
float CL_KeyState (kbutton_t *key) {
	float val;
	qboolean impulsedown, impulseup, down;
	
	impulsedown = key->state & 2;
	impulseup = key->state & 4;
	down = key->state & 1;
	val = 0;
	
	if (impulsedown && !impulseup) {
		if (down)
			val = 0.5;	// pressed and held this frame
		else
			val = 0;	//	I_Error ();
	}
	if (impulseup && !impulsedown) {
		if (down)
			val = 0;	//	I_Error ();
		else
			val = 0;	// released this frame
	}
	if (!impulsedown && !impulseup)	{
		if (down)
			val = 1.0;	// held the entire frame
		else
			val = 0;	// up the entire frame
	}
	if (impulsedown && impulseup) {
		if (down)
			val = 0.75;	// released and re-pressed this frame
		else
			val = 0.25;	// pressed and released this frame
	}

	key->state &= 1;		// clear impulses
	
	return val;
}

//==========================================================================

cvar_t	cl_upspeed = {"cl_upspeed","200"};
cvar_t	cl_forwardspeed = {"cl_forwardspeed","200",CVAR_ARCHIVE};
cvar_t	cl_backspeed = {"cl_backspeed","200",CVAR_ARCHIVE};
cvar_t	cl_sidespeed = {"cl_sidespeed","350",CVAR_ARCHIVE};

cvar_t	cl_movespeedkey = {"cl_movespeedkey","2.0",CVAR_ARCHIVE};
cvar_t	cl_anglespeedkey = {"cl_anglespeedkey","1.5"};

cvar_t	cl_yawspeed = {"cl_yawspeed","140"};
cvar_t	cl_pitchspeed = {"cl_pitchspeed","150"};

cvar_t	lookspring = {"lookspring","0",CVAR_ARCHIVE};
cvar_t	lookstrafe = {"lookstrafe","0",CVAR_ARCHIVE};
cvar_t	sensitivity = {"sensitivity","3",CVAR_ARCHIVE};
cvar_t	freelook = {"freelook","1",CVAR_ARCHIVE};

cvar_t	m_pitch = {"m_pitch","0.022", CVAR_ARCHIVE};
cvar_t	m_yaw = {"m_yaw","0.022"};
cvar_t	m_forward = {"m_forward","1"};
cvar_t	m_side = {"m_side","0.8"};
cvar_t	m_accel = {"m_accel", "0"};


void CL_Rotate_f (void) {
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <degrees>\n", Cmd_Argv(0));
		return;
	}
	if (cl.fpd & FPD_LIMIT_YAW)
		return;
	cl.viewangles[YAW] += atof(Cmd_Argv(1));
	cl.viewangles[YAW] = anglemod(cl.viewangles[YAW]);
}

//Moves the local angle positions
void CL_AdjustAngles (void) {
	float basespeed, speed, up, down, frametime;

	if (Movie_IsCapturing() && movie_steadycam.value)
		frametime = movie_fps.value > 0 ? 1.0 / movie_fps.value : 1 / 30.0;
	else
		frametime = cls.trueframetime;

	basespeed = ((in_speed.state & 1) ? cl_anglespeedkey.value : 1);

	
	if (!(in_strafe.state & 1)) {
		speed = basespeed * cl_yawspeed.value;
		if (cl.fpd & FPD_LIMIT_YAW)
			speed = bound(-900, speed, 900);
		speed *= frametime;
		cl.viewangles[YAW] -= speed * CL_KeyState(&in_right);
		cl.viewangles[YAW] += speed * CL_KeyState(&in_left);
		cl.viewangles[YAW] = anglemod(cl.viewangles[YAW]);
	}

	
	speed = basespeed * cl_pitchspeed.value;
	if (cl.fpd & FPD_LIMIT_PITCH)
		speed = bound(-700, speed, 700);
	speed *= frametime;
	if (in_klook.state & 1)	{
		V_StopPitchDrift ();
		cl.viewangles[PITCH] -= speed * CL_KeyState(&in_forward);
		cl.viewangles[PITCH] += speed * CL_KeyState(&in_back);
	}

	
	up = CL_KeyState(&in_lookup);
	down = CL_KeyState(&in_lookdown);
	cl.viewangles[PITCH] -= speed * up;
	cl.viewangles[PITCH] += speed * down;
	if (up || down)
		V_StopPitchDrift();

	cl.viewangles[PITCH] = bound(-70, cl.viewangles[PITCH], 80);
	cl.viewangles[ROLL] = bound(-50, cl.viewangles[ROLL], 50);		
}

//Send the intended movement message to the server
void CL_BaseMove (usercmd_t *cmd) {	
	CL_AdjustAngles ();
	
	memset (cmd, 0, sizeof(*cmd));
	
	VectorCopy (cl.viewangles, cmd->angles);
	if (in_strafe.state & 1) {
		cmd->sidemove += cl_sidespeed.value * CL_KeyState (&in_right);
		cmd->sidemove -= cl_sidespeed.value * CL_KeyState (&in_left);
	}

	cmd->sidemove += cl_sidespeed.value * CL_KeyState (&in_moveright);
	cmd->sidemove -= cl_sidespeed.value * CL_KeyState (&in_moveleft);

	cmd->upmove += cl_upspeed.value * CL_KeyState (&in_up);
	cmd->upmove -= cl_upspeed.value * CL_KeyState (&in_down);

	if (!(in_klook.state & 1)) {	
		cmd->forwardmove += cl_forwardspeed.value * CL_KeyState (&in_forward);
		cmd->forwardmove -= cl_backspeed.value * CL_KeyState (&in_back);
	}	

	// adjust for speed key
	if (in_speed.state & 1)	{
		cmd->forwardmove *= cl_movespeedkey.value;
		cmd->sidemove *= cl_movespeedkey.value;
		cmd->upmove *= cl_movespeedkey.value;
	}	
}

int MakeChar (int i) {
	i &= ~3;
	if (i < -127 * 4)
		i = -127 * 4;
	if (i > 127 * 4)
		i = 127 * 4;
	return i;
}

void CL_FinishMove (usercmd_t *cmd) {
	int i, ms;
	float frametime;
	static double extramsec = 0;

	if (Movie_IsCapturing() && movie_steadycam.value)
		frametime = movie_fps.value > 0 ? 1.0 / movie_fps.value : 1 / 30.0;
	else
		frametime = cls.trueframetime;

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

	// send milliseconds of time to apply the move
	extramsec += frametime * 1000;
	ms = extramsec;
	extramsec -= ms;
	if (ms > 250)
		ms = 100;		// time was unreasonable
	cmd->msec = ms;

	VectorCopy (cl.viewangles, cmd->angles);

	cmd->impulse = in_impulse;
	in_impulse = 0;

	// chop down so no extra bits are kept that the server wouldn't get
	cmd->forwardmove = MakeChar (cmd->forwardmove);
	cmd->sidemove = MakeChar (cmd->sidemove);
	cmd->upmove = MakeChar (cmd->upmove);

	for (i = 0; i < 3; i++)
		cmd->angles[i] = (Q_rint(cmd->angles[i] * 65536.0 / 360.0) & 65535) * (360.0 / 65536.0);
}

void CL_SendCmd (void) {
	sizebuf_t buf;
	byte data[128];
	usercmd_t *cmd, *oldcmd;
	int i, checksumIndex, lost;
	qboolean dontdrop;
	static float pps_balance = 0;
	static int dropcount = 0;

	if (cls.demoplayback && !cls.mvdplayback)	
		return; // sendcmds come from the demo

	// save this command off for prediction
	i = cls.netchan.outgoing_sequence & UPDATE_MASK;
	cmd = &cl.frames[i].cmd;
	cl.frames[i].senttime = cls.realtime;
	cl.frames[i].receivedtime = -1;		// we haven't gotten a reply yet

	// get basic movement from keyboard
	CL_BaseMove (cmd);

	// allow mice or other external controllers to add to the move
	IN_Move (cmd);

	// if we are spectator, try autocam
	if (cl.spectator)
		Cam_Track(cmd);

	CL_FinishMove(cmd);

	Cam_FinishMove(cmd);

	if (cls.mvdplayback) {		
		cls.netchan.outgoing_sequence++;
		return;
	}

	SZ_Init (&buf, data, sizeof(data)); 

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
	if (cl_c2sImpulseBackup.value >= 2)
		dontdrop = dontdrop || cmd->impulse;
	MSG_WriteDeltaUsercmd (&buf, &nullcmd, cmd);
	oldcmd = cmd;

	i = (cls.netchan.outgoing_sequence - 1) & UPDATE_MASK;
	cmd = &cl.frames[i].cmd;
	if (cl_c2sImpulseBackup.value >= 3)
		dontdrop = dontdrop || cmd->impulse;
	MSG_WriteDeltaUsercmd (&buf, oldcmd, cmd);
	oldcmd = cmd;

	i = (cls.netchan.outgoing_sequence) & UPDATE_MASK;
	cmd = &cl.frames[i].cmd;
	if (cl_c2sImpulseBackup.value >= 1)
		dontdrop = dontdrop || cmd->impulse;
	MSG_WriteDeltaUsercmd (&buf, oldcmd, cmd);

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
	} else {
		cl.frames[cls.netchan.outgoing_sequence & UPDATE_MASK].delta_sequence = -1;
	}

	if (cls.demorecording)
		CL_WriteDemoCmd (cmd);

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

	// deliver the message
	Netchan_Transmit (&cls.netchan, buf.cursize, buf.data);	
}

void CL_InitInput (void) {
	Cmd_AddCommand ("+moveup",IN_UpDown);
	Cmd_AddCommand ("-moveup",IN_UpUp);
	Cmd_AddCommand ("+movedown",IN_DownDown);
	Cmd_AddCommand ("-movedown",IN_DownUp);
	Cmd_AddCommand ("+left",IN_LeftDown);
	Cmd_AddCommand ("-left",IN_LeftUp);
	Cmd_AddCommand ("+right",IN_RightDown);
	Cmd_AddCommand ("-right",IN_RightUp);
	Cmd_AddCommand ("+forward",IN_ForwardDown);
	Cmd_AddCommand ("-forward",IN_ForwardUp);
	Cmd_AddCommand ("+back",IN_BackDown);
	Cmd_AddCommand ("-back",IN_BackUp);
	Cmd_AddCommand ("+lookup", IN_LookupDown);
	Cmd_AddCommand ("-lookup", IN_LookupUp);
	Cmd_AddCommand ("+lookdown", IN_LookdownDown);
	Cmd_AddCommand ("-lookdown", IN_LookdownUp);
	Cmd_AddCommand ("+strafe", IN_StrafeDown);
	Cmd_AddCommand ("-strafe", IN_StrafeUp);
	Cmd_AddCommand ("+moveleft", IN_MoveleftDown);
	Cmd_AddCommand ("-moveleft", IN_MoveleftUp);
	Cmd_AddCommand ("+moveright", IN_MoverightDown);
	Cmd_AddCommand ("-moveright", IN_MoverightUp);
	Cmd_AddCommand ("+speed", IN_SpeedDown);
	Cmd_AddCommand ("-speed", IN_SpeedUp);
	Cmd_AddCommand ("+attack", IN_AttackDown);
	Cmd_AddCommand ("-attack", IN_AttackUp);
	Cmd_AddCommand ("+use", IN_UseDown);
	Cmd_AddCommand ("-use", IN_UseUp);
	Cmd_AddCommand ("+jump", IN_JumpDown);
	Cmd_AddCommand ("-jump", IN_JumpUp);
	Cmd_AddCommand ("impulse", IN_Impulse);
	Cmd_AddCommand ("+klook", IN_KLookDown);
	Cmd_AddCommand ("-klook", IN_KLookUp);
	Cmd_AddCommand ("+mlook", IN_MLookDown);
	Cmd_AddCommand ("-mlook", IN_MLookUp);

	
	Cmd_AddCommand ("rotate",CL_Rotate_f);

	Cvar_SetCurrentGroup(CVAR_GROUP_INPUT_KEYBOARD);

	Cvar_Register (&cl_smartjump);


	Cvar_Register (&cl_upspeed);
	Cvar_Register (&cl_forwardspeed);
	Cvar_Register (&cl_backspeed);
	Cvar_Register (&cl_sidespeed);
	Cvar_Register (&cl_movespeedkey);
	Cvar_Register (&cl_yawspeed);
	Cvar_Register (&cl_pitchspeed);
	Cvar_Register (&cl_anglespeedkey);

	Cvar_SetCurrentGroup(CVAR_GROUP_INPUT_MISC);
	Cvar_Register (&lookspring);
	Cvar_Register (&lookstrafe);
	Cvar_Register (&sensitivity);
	Cvar_Register (&freelook);

	Cvar_SetCurrentGroup(CVAR_GROUP_INPUT_MOUSE);
	Cvar_Register (&m_pitch);
	Cvar_Register (&m_yaw);
	Cvar_Register (&m_forward);
	Cvar_Register (&m_side);
	Cvar_Register (&m_accel);

	Cvar_SetCurrentGroup(CVAR_GROUP_NETWORK);
	Cvar_Register (&cl_nodelta);
	Cvar_Register (&cl_c2sImpulseBackup);
	Cvar_Register (&cl_c2spps);

	Cvar_ResetCurrentGroup();
}

void CL_ClearStates (void) {}
