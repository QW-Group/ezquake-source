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

/********************************** WRITING **********************************/

void MSG_WriteChar (sizebuf_t *sb, int c) {
	byte *buf;
	
#ifdef PARANOID
	if (c < -128 || c > 127)
		Sys_Error ("MSG_WriteChar: range error");
#endif

	buf = SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void MSG_WriteByte (sizebuf_t *sb, int c) {
	byte *buf;

#ifdef PARANOID
	if (c < 0 || c > 255)
		Sys_Error ("MSG_WriteByte: range error");
#endif

	buf = SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void MSG_WriteShort (sizebuf_t *sb, int c) {
	byte *buf;

#ifdef PARANOID
	if (c < ((short)0x8000) || c > (short)0x7fff)
		Sys_Error ("MSG_WriteShort: range error");
#endif

	buf = SZ_GetSpace (sb, 2);
	buf[0] = c & 0xff;
	buf[1] = c >> 8;
}

void MSG_WriteLong (sizebuf_t *sb, int c) {
	byte *buf;

	buf = SZ_GetSpace (sb, 4);
	buf[0] = c & 0xff;
	buf[1] = (c >> 8) & 0xff;
	buf[2] = (c >> 16) & 0xff;
	buf[3] = c >> 24;
}

void MSG_WriteFloat (sizebuf_t *sb, float f) {
	union {
		float	f;
		int	l;
	} dat;	

	dat.f = f;
	dat.l = LittleLong (dat.l);
	
	SZ_Write (sb, &dat.l, 4);
}

void MSG_WriteString (sizebuf_t *sb, char *s) {
	if (!s)
		SZ_Write (sb, "", 1);
	else
		SZ_Write (sb, s, strlen(s)+1);
}

void MSG_WriteCoord (sizebuf_t *sb, float f) {
	MSG_WriteShort (sb, (int)(f * 8));
}

void MSG_WriteAngle (sizebuf_t *sb, float f) {
	MSG_WriteByte (sb, Q_rint(f * 256.0 / 360.0) & 255);
}

void MSG_WriteAngle16 (sizebuf_t *sb, float f) {
	MSG_WriteShort (sb, Q_rint(f * 65536.0 / 360.0) & 65535);
}

void MSG_WriteDeltaUsercmd (sizebuf_t *buf, usercmd_t *from, usercmd_t *cmd) {
	int bits;

	// send the movement message
	bits = 0;
	if (cmd->angles[0] != from->angles[0])
		bits |= CM_ANGLE1;
	if (cmd->angles[1] != from->angles[1])
		bits |= CM_ANGLE2;
	if (cmd->angles[2] != from->angles[2])
		bits |= CM_ANGLE3;
	if (cmd->forwardmove != from->forwardmove)
		bits |= CM_FORWARD;
	if (cmd->sidemove != from->sidemove)
		bits |= CM_SIDE;
	if (cmd->upmove != from->upmove)
		bits |= CM_UP;
	if (cmd->buttons != from->buttons)
		bits |= CM_BUTTONS;
	if (cmd->impulse != from->impulse)
		bits |= CM_IMPULSE;

    MSG_WriteByte (buf, bits);

	if (bits & CM_ANGLE1)
		MSG_WriteAngle16 (buf, cmd->angles[0]);
	if (bits & CM_ANGLE2)
		MSG_WriteAngle16 (buf, cmd->angles[1]);
	if (bits & CM_ANGLE3)
		MSG_WriteAngle16 (buf, cmd->angles[2]);
	
	if (bits & CM_FORWARD)
		MSG_WriteShort (buf, cmd->forwardmove);
	if (bits & CM_SIDE)
	  	MSG_WriteShort (buf, cmd->sidemove);
	if (bits & CM_UP)
		MSG_WriteShort (buf, cmd->upmove);

 	if (bits & CM_BUTTONS)
	  	MSG_WriteByte (buf, cmd->buttons);
 	if (bits & CM_IMPULSE)
	    MSG_WriteByte (buf, cmd->impulse);
	MSG_WriteByte (buf, cmd->msec);
}

//Writes part of a packetentities message.
//Can delta from either a baseline or a previous packet_entity
void MSG_WriteDeltaEntity (entity_state_t *from, entity_state_t *to, sizebuf_t *msg, qboolean force) {
	int bits, i;
	float miss;

	assert (to->number > 0); 
	assert (to->number < 512); 

	// send an update
	bits = 0;

	for (i = 0; i < 3; i++) {
		miss = to->origin[i] - from->origin[i];
		if ( miss < -0.1 || miss > 0.1 )
			bits |= U_ORIGIN1 << i;
	}

	if ( to->angles[0] != from->angles[0] )
		bits |= U_ANGLE1;
		
	if ( to->angles[1] != from->angles[1] )
		bits |= U_ANGLE2;
		
	if ( to->angles[2] != from->angles[2] )
		bits |= U_ANGLE3;
		
	if ( to->colormap != from->colormap )
		bits |= U_COLORMAP;
		
	if ( to->skinnum != from->skinnum )
		bits |= U_SKIN;
		
	if ( to->frame != from->frame )
		bits |= U_FRAME;
	
	if ( to->effects != from->effects )
		bits |= U_EFFECTS;
	
	if ( to->modelindex != from->modelindex )
		bits |= U_MODEL;

	if (bits & 511)
		bits |= U_MOREBITS;

	if (to->flags & U_SOLID)
		bits |= U_SOLID;

	// write the message

	if (!bits && !force)
		return;		// nothing to send!

	i = to->number | (bits & ~511);
	assert (!(i & U_REMOVE));
	MSG_WriteShort (msg, i);

	if (bits & U_MOREBITS)
		MSG_WriteByte (msg, bits&255);
	if (bits & U_MODEL)
		MSG_WriteByte (msg,	to->modelindex);
	if (bits & U_FRAME)
		MSG_WriteByte (msg, to->frame);
	if (bits & U_COLORMAP)
		MSG_WriteByte (msg, to->colormap);
	if (bits & U_SKIN)
		MSG_WriteByte (msg, to->skinnum);
	if (bits & U_EFFECTS)
		MSG_WriteByte (msg, to->effects);
	if (bits & U_ORIGIN1)
		MSG_WriteCoord (msg, to->origin[0]);		
	if (bits & U_ANGLE1)
		MSG_WriteAngle(msg, to->angles[0]);
	if (bits & U_ORIGIN2)
		MSG_WriteCoord (msg, to->origin[1]);
	if (bits & U_ANGLE2)
		MSG_WriteAngle(msg, to->angles[1]);
	if (bits & U_ORIGIN3)
		MSG_WriteCoord (msg, to->origin[2]);
	if (bits & U_ANGLE3)
		MSG_WriteAngle(msg, to->angles[2]);
}

//Writes a delta update of a packet_entities_t to the message.
void MSG_EmitPacketEntities (packet_entities_t *from, int delta_sequence, packet_entities_t *to, sizebuf_t *msg,
 entity_state_t *(*GetBaseline)(int number)) {
	int oldindex, newindex, oldnum, newnum, oldmax;

	// this is the frame that we are going to delta update from
	if (from) {
		oldmax = from->num_entities;

		MSG_WriteByte (msg, svc_deltapacketentities);
		MSG_WriteByte (msg, delta_sequence);
	} else {
		oldmax = 0;	// no delta update
		from = NULL;

		MSG_WriteByte (msg, svc_packetentities);
	}

	newindex = 0;
	oldindex = 0;
	while (newindex < to->num_entities || oldindex < oldmax) {
		newnum = newindex >= to->num_entities ? 9999 : to->entities[newindex].number;
		oldnum = oldindex >= oldmax ? 9999 : from->entities[oldindex].number;

		if (newnum == oldnum) {
			// delta update from old position
			MSG_WriteDeltaEntity (&from->entities[oldindex], &to->entities[newindex], msg, false);
			oldindex++;
			newindex++;
			continue;
		}

		if (newnum < oldnum) {
			// this is a new entity, send it from the baseline
			MSG_WriteDeltaEntity (GetBaseline(newnum), &to->entities[newindex], msg, true);
			newindex++;
			continue;
		}

		if (newnum > oldnum) {
			// the old entity isn't present in the new message
			MSG_WriteShort (msg, oldnum | U_REMOVE);
			oldindex++;
			continue;
		}
	}

	MSG_WriteShort (msg, 0);	// end of packetentities
}

/********************************** READING **********************************/

int			msg_readcount;
qboolean	msg_badread;

void MSG_BeginReading (void) {
	msg_readcount = 0;
	msg_badread = false;
}

int MSG_GetReadCount(void) {
	return msg_readcount;
}

// returns -1 and sets msg_badread if no more characters are available
int MSG_ReadChar (void) {
	int	c;

	if (msg_readcount + 1 > net_message.cursize) {
		msg_badread = true;
		return -1;
	}

	c = (signed char)net_message.data[msg_readcount];
	msg_readcount++;
	
	return c;
}

int MSG_ReadByte (void) {
	int	c;

	if (msg_readcount + 1 > net_message.cursize) {
		msg_badread = true;
		return -1;
	}

	c = (unsigned char)net_message.data[msg_readcount];
	msg_readcount++;

	return c;
}

int MSG_ReadShort (void) {
	int	c;

	if (msg_readcount + 2 > net_message.cursize) {
		msg_badread = true;
		return -1;
	}

	c = (short)(net_message.data[msg_readcount]
	+ (net_message.data[msg_readcount + 1] << 8));

	msg_readcount += 2;

	return c;
}

int MSG_ReadLong (void) {
	int	c;

	if (msg_readcount + 4 > net_message.cursize) {
		msg_badread = true;
		return -1;
	}

	c = net_message.data[msg_readcount]
		+ (net_message.data[msg_readcount + 1] << 8)
		+ (net_message.data[msg_readcount + 2] << 16)
		+ (net_message.data[msg_readcount + 3] << 24);

	msg_readcount += 4;

	return c;
}

float MSG_ReadFloat (void) {
	union {
		byte b[4];
		float f;
		int	l;
	} dat;

	dat.b[0] = net_message.data[msg_readcount];
	dat.b[1] = net_message.data[msg_readcount + 1];
	dat.b[2] = net_message.data[msg_readcount + 2];
	dat.b[3] = net_message.data[msg_readcount + 3];
	msg_readcount += 4;

	dat.l = LittleLong (dat.l);

	return dat.f;	
}

char *MSG_ReadString (void) {
	static char	string[2048];
	int l,c;

	l = 0;
	do {
		c = MSG_ReadByte ();
		if (c == 255)			// skip these to avoid security problems
			continue;			// with old clients and servers
		if (c == -1 || c == 0)		// msg_badread or end of string
			break;
		string[l] = c;
		l++;
	} while (l < sizeof(string)-1);

	string[l] = 0;

	return string;
}

char *MSG_ReadStringLine (void) {
	static char	string[2048];
	int l, c;

	l = 0;
	do {
		c = MSG_ReadByte ();
		if (c == 255)
			continue;
		if (c == -1 || c == 0 || c == '\n')
			break;
		string[l] = c;
		l++;
	} while (l < sizeof(string) - 1);

	string[l] = 0;

	return string;
}

float MSG_ReadCoord (void) {
	return MSG_ReadShort() * (1.0 / 8);
}

float MSG_ReadAngle (void) {
	return MSG_ReadChar() * (360.0 / 256);
}

float MSG_ReadAngle16 (void) {
	return MSG_ReadShort() * (360.0 / 65536);
}

#define	CM_MSEC	(1 << 7)		// same as CM_ANGLE2

void MSG_ReadDeltaUsercmd (usercmd_t *from, usercmd_t *move, int protoversion) {
	int bits;

	memcpy (move, from, sizeof(*move));

	bits = MSG_ReadByte ();

	if (protoversion == 26) {
		// read current angles
		if (bits & CM_ANGLE1)
			move->angles[0] = MSG_ReadAngle16 ();
		move->angles[1] = MSG_ReadAngle16 ();		// always sent
		if (bits & CM_ANGLE3)
			move->angles[2] = MSG_ReadAngle16 ();

		// read movement
		if (bits & CM_FORWARD)
			move->forwardmove = MSG_ReadChar() << 3;
		if (bits & CM_SIDE)
			move->sidemove = MSG_ReadChar() << 3;
		if (bits & CM_UP)
			move->upmove = MSG_ReadChar() << 3;
	} else {
		// read current angles
		if (bits & CM_ANGLE1)
			move->angles[0] = MSG_ReadAngle16 ();
		if (bits & CM_ANGLE2)
			move->angles[1] = MSG_ReadAngle16 ();
		if (bits & CM_ANGLE3)
			move->angles[2] = MSG_ReadAngle16 ();

		// read movement
		if (bits & CM_FORWARD)
			move->forwardmove = MSG_ReadShort ();
		if (bits & CM_SIDE)
			move->sidemove = MSG_ReadShort ();
		if (bits & CM_UP)
			move->upmove = MSG_ReadShort ();
	}

	// read buttons
	if (bits & CM_BUTTONS)
		move->buttons = MSG_ReadByte ();

	if (bits & CM_IMPULSE)
		move->impulse = MSG_ReadByte ();

	// read time to run command
	if (protoversion == 26) {
		if (bits & CM_MSEC)
			move->msec = MSG_ReadByte ();
	} else {
		move->msec = MSG_ReadByte ();		// always sent
	}
}
