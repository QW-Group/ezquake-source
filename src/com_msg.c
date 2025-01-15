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

    $Id: com_msg.c,v 1.6 2006-07-24 15:13:28 disconn3ct Exp $
*/

#include "quakedef.h"

/*
==============================================================================
                          MESSAGE IO FUNCTIONS
==============================================================================
Handles byte ordering and avoids alignment errors
*/

#ifdef FTE_PEXT_FLOATCOORDS

int msg_coordsize = 2; // 2 or 4.
int msg_anglesize = 1; // 1 or 2.

float MSG_FromCoord(coorddata c, int bytes)
{
	switch(bytes)
	{
	case 2:	//encode 1/8th precision, giving -4096 to 4096 map sizes
		return LittleShort(c.b2)/8.0f;
	case 4:
		return LittleFloat(c.f);
	default:
		Host_Error("MSG_FromCoord: not a sane size");
		return 0;
	}
}

coorddata MSG_ToCoord(float f, int bytes)	//return value should be treated as (char*)&ret;
{
	coorddata r;
	switch(bytes)
	{
	case 2:
		r.b4 = 0;
		if (f >= 0)
			r.b2 = LittleShort((short)(f*8+0.5f));
		else
			r.b2 = LittleShort((short)(f*8-0.5f));
		break;
	case 4:
		r.f = LittleFloat(f);
		break;
	default:
		Host_Error("MSG_ToCoord: not a sane size");
		r.b4 = 0;
	}

	return r;
}

coorddata MSG_ToAngle(float f, int bytes)	//return value is NOT byteswapped.
{
	coorddata r;
	switch(bytes)
	{
	case 1:
		r.b4 = 0;
		if (f >= 0)
			r.b[0] = (int)(f*(256.0f/360.0f) + 0.5f) & 255;
		else
			r.b[0] = (int)(f*(256.0f/360.0f) - 0.5f) & 255;
		break;
	case 2:
		r.b4 = 0;
		if (f >= 0)
			r.b2 = LittleShort((int)(f*(65536.0f/360.0f) + 0.5f) & 65535);
		else
			r.b2 = LittleShort((int)(f*(65536.0f/360.0f) - 0.5f) & 65535);
		break;
//	case 4:
//		r.f = LittleFloat(f);
//		break;
	default:
		Host_Error("MSG_ToAngle: not a sane size");
		r.b4 = 0;
	}

	return r;
}

#endif

// writing functions
void MSG_WriteChar (sizebuf_t *sb, int c)
{
	byte *buf;

#ifdef PARANOID
	if (c < -128 || c > 127)
		Sys_Error ("MSG_WriteChar: range error");
#endif

	buf = SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void MSG_WriteByte (sizebuf_t *sb, int c)
{
	byte *buf;

#ifdef PARANOID
	if (c < 0 || c > 255)
		Sys_Error ("MSG_WriteByte: range error");
#endif

	buf = SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void MSG_WriteShort (sizebuf_t *sb, int c)
{
	byte *buf;

#ifdef PARANOID
	if (c < ((short)0x8000) || c > (short)0x7fff)
		Sys_Error ("MSG_WriteShort: range error");
#endif

	buf = SZ_GetSpace (sb, 2);
	buf[0] = c & 0xff;
	buf[1] = c >> 8;
}

void MSG_WriteLong (sizebuf_t *sb, int c)
{
	byte *buf;

	buf = SZ_GetSpace (sb, 4);
	buf[0] = c & 0xff;
	buf[1] = (c >> 8) & 0xff;
	buf[2] = (c >> 16) & 0xff;
	buf[3] = c >> 24;
}

void MSG_WriteFloat (sizebuf_t *sb, float f)
{
	union {
		float	f;
		int	l;
	} dat;

	dat.f = f;
	dat.l = LittleLong (dat.l);

	SZ_Write (sb, &dat.l, 4);
}

void MSG_WriteString (sizebuf_t *sb, const char *s)
{
	if (!s || !*s)
		SZ_Write (sb, "", 1);
	else
		SZ_Write (sb, s, strlen(s)+1);
}

void MSG_WriteUnterminatedString (sizebuf_t *sb, char *s)
{
	if (s && *s)
		SZ_Write (sb, s, strlen(s));
}

void MSG_WriteCoord (sizebuf_t *sb, float f)
{
#ifdef FTE_PEXT_FLOATCOORDS
	coorddata i = MSG_ToCoord(f, msg_coordsize);
	SZ_Write (sb, (void*)&i, msg_coordsize);
#else
	MSG_WriteShort (sb, (int)(f * 8));
#endif
}

void MSG_WriteAngle16 (sizebuf_t *sb, float f)
{
	MSG_WriteShort (sb, Q_rint(f * 65536.0 / 360.0) & 65535);
}

void MSG_WriteAngle (sizebuf_t *sb, float f)
{
#ifdef FTE_PEXT_FLOATCOORDS
	if (msg_anglesize == 2)
		MSG_WriteAngle16(sb, f);
//	else if (msg_anglesize==4)
//		MSG_WriteFloat(sb, f);
	else
#endif
		MSG_WriteByte (sb, Q_rint(f * 256.0 / 360.0) & 255);
}

int MSG_WriteDeltaUsercmd(sizebuf_t *buf, struct usercmd_s *from, struct usercmd_s *cmd, unsigned int mvdsv_extensions)
{
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
	if (bits & CM_IMPULSE) {
		MSG_WriteByte(buf, cmd->impulse);
	}
	MSG_WriteByte (buf, cmd->msec);

	return bits;
}

//Writes part of a packetentities message.
//Can delta from either a baseline or a previous packet_entity
void MSG_WriteDeltaEntity (entity_state_t *from, entity_state_t *to, sizebuf_t *msg, qbool force, unsigned int fte_extensions, unsigned int mvdsv_extensions)
{
	int bits, i;
#ifdef PROTOCOL_VERSION_FTE
	int evenmorebits = 0;
	unsigned int required_extensions = 0;
#endif

	// send an update
	bits = 0;

	for (i=0 ; i<3 ; i++)
		if ( to->origin[i] != from->origin[i] )
			bits |= U_ORIGIN1<<i;

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

	if (to->modelindex != from->modelindex) {
		bits |= U_MODEL;
#ifdef FTE_PEXT_ENTITYDBL
		if (to->modelindex > 255) {
			evenmorebits |= U_FTE_MODELDBL;
			required_extensions |= FTE_PEXT_MODELDBL;
		}
#endif
	}

	if (bits & U_CHECKMOREBITS)
		bits |= U_MOREBITS;

	if (to->flags & U_SOLID)
		bits |= U_SOLID;

	// Taken from FTE
	if (msg->cursize + 40 > msg->maxsize && !msg->overflow_handler)
	{
		//not enough space in the buffer, don't send the entity this frame. (not sending means nothing changes, and it takes no bytes!!)
		int oldnum = to->number;
		*to = *from;
		if (oldnum && !from->number) {
			to->number = oldnum;
		}
		return;
	}

	//
	// write the message
	//
	if (!to->number) {
		Sys_Error("Unset entity number");
	}

#ifdef PROTOCOL_VERSION_FTE
	if (to->number >= 512)
	{
		if (to->number >= 1024)
		{
			if (to->number >= 1024 + 512) {
				evenmorebits |= U_FTE_ENTITYDBL;
				required_extensions |= FTE_PEXT_ENTITYDBL;
			}

			evenmorebits |= U_FTE_ENTITYDBL2;
			required_extensions |= FTE_PEXT_ENTITYDBL2;
			if (to->number >= 2048) {
				Sys_Error("Entity number >= 2048");
			}
		}
		else {
			evenmorebits |= U_FTE_ENTITYDBL;
			required_extensions |= FTE_PEXT_ENTITYDBL;
		}
	}

#ifdef U_FTE_TRANS
	if (to->trans != from->trans && (fte_extensions & FTE_PEXT_TRANS)) {
		evenmorebits |= U_FTE_TRANS;
		required_extensions |= FTE_PEXT_TRANS;
    }
#endif

#ifdef U_FTE_COLOURMOD
	if ((to->colourmod[0] != from->colourmod[0] ||
		 to->colourmod[1] != from->colourmod[1] ||
		 to->colourmod[2] != from->colourmod[2]) && (fte_extensions & FTE_PEXT_COLOURMOD)) {
		evenmorebits |= U_FTE_COLOURMOD;
		required_extensions |= FTE_PEXT_COLOURMOD;
    }
#endif

	if (evenmorebits&0xff00)
		evenmorebits |= U_FTE_YETMORE;
	if (evenmorebits&0x00ff)
		bits |= U_FTE_EVENMORE;
	if (bits & 511)
		bits |= U_MOREBITS;
#endif

	if (to->number >= MAX_EDICTS)
	{
		/*SV_Error*/
		Con_Printf ("Entity number >= MAX_EDICTS (%d), set to MAX_EDICTS - 1\n", MAX_EDICTS);
		to->number = MAX_EDICTS - 1;
	}

#ifdef PROTOCOL_VERSION_FTE
	if (evenmorebits && (fte_extensions & required_extensions) != required_extensions) {
		return;
	}
#endif
	if (!bits && !force) {
		return;		// nothing to send!
	}

	i = (to->number & U_CHECKMOREBITS) | (bits&~U_CHECKMOREBITS);
	if (i & U_REMOVE)
		Sys_Error ("U_REMOVE");
	MSG_WriteShort (msg, i);

	if (bits & U_MOREBITS)
		MSG_WriteByte (msg, bits&255);
#ifdef PROTOCOL_VERSION_FTE
	if (bits & U_FTE_EVENMORE)
		MSG_WriteByte (msg, evenmorebits&255);
	if (evenmorebits & U_FTE_YETMORE)
		MSG_WriteByte (msg, (evenmorebits>>8)&255);
#endif

	if (bits & U_MODEL)
		MSG_WriteByte (msg, to->modelindex & 255);
	if (bits & U_FRAME)
		MSG_WriteByte (msg, to->frame);
	if (bits & U_COLORMAP)
		MSG_WriteByte (msg, to->colormap);
	if (bits & U_SKIN)
		MSG_WriteByte (msg, to->skinnum);
	if (bits & U_EFFECTS)
		MSG_WriteByte (msg, to->effects);
	if (bits & U_ORIGIN1) {
		if (mvdsv_extensions & MVD_PEXT1_FLOATCOORDS) {
			MSG_WriteLongCoord(msg, to->origin[0]);
		}
		else {
			MSG_WriteCoord(msg, to->origin[0]);
		}
	}
	if (bits & U_ANGLE1)
		MSG_WriteAngle(msg, to->angles[0]);
	if (bits & U_ORIGIN2) {
		if (mvdsv_extensions & MVD_PEXT1_FLOATCOORDS) {
			MSG_WriteLongCoord(msg, to->origin[1]);
		}
		else {
			MSG_WriteCoord(msg, to->origin[1]);
		}
	}
	if (bits & U_ANGLE2)
		MSG_WriteAngle(msg, to->angles[1]);
	if (bits & U_ORIGIN3) {
		if (mvdsv_extensions & MVD_PEXT1_FLOATCOORDS) {
			MSG_WriteLongCoord(msg, to->origin[2]);
		}
		else {
			MSG_WriteCoord(msg, to->origin[2]);
		}
	}
	if (bits & U_ANGLE3)
		MSG_WriteAngle(msg, to->angles[2]);

#ifdef U_FTE_TRANS
	if (evenmorebits & U_FTE_TRANS)
		MSG_WriteByte (msg, to->trans);
#endif

#ifdef U_FTE_COLOURMOD
	if (evenmorebits & U_FTE_COLOURMOD)
	{
		MSG_WriteByte (msg, to->colourmod[0]);
		MSG_WriteByte (msg, to->colourmod[1]);
		MSG_WriteByte (msg, to->colourmod[2]);
	}
#endif
}

/********************************** READING **********************************/

int msg_readcount;
qbool msg_badread;

void MSG_BeginReading (void)
{
	msg_readcount = 0;
	msg_badread = false;
}

int MSG_GetReadCount(void)
{
	return msg_readcount;
}

// returns -1 and sets msg_badread if no more characters are available
int MSG_ReadChar (void)
{
	int	c;

	if (msg_readcount + 1 > net_message.cursize) {
		msg_badread = true;
		return -1;
	}

	c = (signed char)net_message.data[msg_readcount];
	msg_readcount++;

	return c;
}

int MSG_ReadByte (void)
{
	int	c;

	if (msg_readcount + 1 > net_message.cursize) {
		msg_badread = true;
		return -1;
	}

	c = (unsigned char)net_message.data[msg_readcount];
	msg_readcount++;

	return c;
}

int MSG_ReadShort (void)
{
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

int MSG_ReadLong (void)
{
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

float MSG_ReadFloat (void)
{
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

char *MSG_ReadString (void)
{
	static char string[2048];
	unsigned int l;
	int c;

	l = 0;
	do {
		c = MSG_ReadByte ();

		if (c == 255) // skip these to avoid security problems
			continue; // with old clients and servers

		if (c == -1 || c == 0) // msg_badread or end of string
			break;

		string[l] = c;
		l++;
	} while (l < sizeof(string)-1);

	string[l] = 0;

	return string;
}

char *MSG_ReadStringLine (void)
{
	static char	string[2048];
	unsigned int l;
	int c;

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

float MSG_ReadCoord (void)
{
#ifdef FTE_PEXT_FLOATCOORDS

	coorddata c = {0};
	MSG_ReadData(&c, msg_coordsize);
	return MSG_FromCoord(c, msg_coordsize);

#else // FTE_PEXT_FLOATCOORDS

	return MSG_ReadShort() * (1.0 / 8);

#endif // FTE_PEXT_FLOATCOORDS
}

float MSG_ReadFloatCoord(void)
{
	float f;
	MSG_ReadData(&f, 4);
	return LittleFloat(f);
}

void MSG_WriteLongCoord(sizebuf_t* sb, float f)
{
	f = LittleFloat(f);

	MSG_WriteFloat(sb, f);
}

float MSG_ReadAngle16 (void)
{
	return MSG_ReadShort() * (360.0 / 65536);
}

float MSG_ReadAngle (void)
{
#ifdef FTE_PEXT_FLOATCOORDS

	switch(msg_anglesize)
	{
	case 1:
		return MSG_ReadChar() * (360.0/256);
	case 2:
		return MSG_ReadAngle16();
//	case 4:
//		return MSG_ReadFloat();
	default:
		Host_Error("MSG_ReadAngle: Bad angle size\n");
		return 0;
	}

#else // FTE_PEXT_FLOATCOORDS

	return MSG_ReadChar() * (360.0 / 256);

#endif // FTE_PEXT_FLOATCOORDS
}

#define CM_MSEC	(1 << 7) // same as CM_ANGLE2

void MSG_ReadDeltaUsercmd (usercmd_t *from, usercmd_t *move, int protoversion)
{
	int bits;

	memcpy (move, from, sizeof(*move));

	bits = MSG_ReadByte ();

	if (protoversion <= 26) {
		// read current angles
		if (bits & CM_ANGLE1)
			move->angles[0] = MSG_ReadAngle16 ();
		move->angles[1] = MSG_ReadAngle16 (); // always sent
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
	if (protoversion <= 26 && (bits & CM_MSEC)) {
		move->msec = MSG_ReadByte ();
	} 
	else if (protoversion >= 27) {
		move->msec = MSG_ReadByte (); // always sent
	}
}

void MSG_ReadData (void *data, int len)
{
	int	i;

	for (i = 0 ; i < len ; i++)
		((byte *)data)[i] = MSG_ReadByte ();
}

void MSG_ReadSkip(int bytes)
{
	for ( ; !msg_badread && bytes > 0; bytes--)
	{
		MSG_ReadByte ();
	}
}

#ifndef SERVERONLY
byte MSG_EncodeMVDSVWeaponFlags(int deathmatch, int weaponmode, int weaponhide, qbool weaponhide_axe, qbool forgetorder, qbool forgetondeath, int max_impulse)
{
	byte flags = clc_mvd_weapon_switching;
	qbool hide = ((weaponhide == 1) || ((weaponhide == 2) && (deathmatch == 1)));

	if (weaponmode == 3) {
		weaponmode = (deathmatch == 1 ? 1 : 0);
	}
	else if (weaponmode == 4) {
		weaponmode = (deathmatch == 1 ? 2 : 0);
	}

	if (weaponmode == 1) {
		flags |= clc_mvd_weapon_mode_presel;
	}
	else if (weaponmode == 2) {
		flags |= clc_mvd_weapon_mode_iffiring;
	}

	if (forgetorder) {
		flags |= clc_mvd_weapon_forget_ranking;
	}

	if (hide) {
		if (weaponhide_axe) {
			flags |= clc_mvd_weapon_hide_axe;
		}
		else {
			flags |= clc_mvd_weapon_hide_sg;
		}
	}

	flags |= (forgetondeath ? clc_mvd_weapon_reset_on_death : 0);

	flags |= (max_impulse >= 16 ? clc_mvd_weapon_full_impulse : 0);

	return flags;
}
#endif

void MSG_DecodeMVDSVWeaponFlags(int flags, int* weaponmode, int* weaponhide, qbool* forgetorder, int* sequence)
{

}

