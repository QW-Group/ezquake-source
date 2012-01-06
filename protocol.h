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
// protocol.h -- communications protocols
#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#define	PROTOCOL_VERSION	28

//===============================================

#define	PORT_CLIENT	27001
#define	PORT_MASTER	27000
#define	PORT_SERVER	27500
#define PORT_QUAKETV 27900

//===============================================

// fte protocol extensions.

#define PROTOCOL_VERSION_FTE			(('F'<<0) + ('T'<<8) + ('E'<<16) + ('X' << 24))
#define PROTOCOL_VERSION_FTE2			(('F'<<0) + ('T'<<8) + ('E'<<16) + ('2' << 24))

//
// Not all of that really supported.
// Some supported by client only.
//

#ifdef PROTOCOL_VERSION_FTE

#define	FTE_PEXT_TRANS				0x00000008	// .alpha support
#define FTE_PEXT_ACCURATETIMINGS	0x00000040
#define FTE_PEXT_HLBSP				0x00000200	//stops fte servers from complaining
#define FTE_PEXT_MODELDBL			0x00001000
#define FTE_PEXT_ENTITYDBL			0x00002000	//max of 1024 ents instead of 512
#define FTE_PEXT_ENTITYDBL2			0x00004000	//max of 1024 ents instead of 512
#define FTE_PEXT_FLOATCOORDS		0x00008000	//supports floating point origins.
#define FTE_PEXT_SPAWNSTATIC2		0x00400000	//Sends an entity delta instead of a baseline.
#define FTE_PEXT_256PACKETENTITIES	0x01000000	//Client can recieve 256 packet entities.
#define FTE_PEXT_CHUNKEDDOWNLOADS	0x20000000	//alternate file download method. Hopefully it'll give quadroupled download speed, especially on higher pings.

#endif // PROTOCOL_VERSION_FTE

#ifdef PROTOCOL_VERSION_FTE2

#define FTE_PEXT2_VOICECHAT			0x00000002

#endif // PROTOCOL_VERSION_FTE2

//==============================================

//
// ZQuake protocol extensions (*z_ext serverinfo key)
//
#define Z_EXT_PM_TYPE		(1<<0)	// basic PM_TYPE functionality (reliable jump_held)
#define Z_EXT_PM_TYPE_NEW	(1<<1)	// adds PM_FLY, PM_SPECTATOR
#define Z_EXT_VIEWHEIGHT	(1<<2)	// STAT_VIEWHEIGHT
#define Z_EXT_SERVERTIME	(1<<3)	// STAT_TIME
#define Z_EXT_PITCHLIMITS	(1<<4)	// serverinfo maxpitch & minpitch
#define Z_EXT_JOIN_OBSERVE	(1<<5)	// server: "join" and "observe" commands are supported
									// client: on-the-fly spectator <-> player switching supported
#define Z_EXT_PF_ONGROUND	(1<<6)	// server: PF_ONGROUND is valid for all svc_playerinfo
#define Z_EXT_VWEP			(1<<7)	// ZQ_VWEP extension
#define Z_EXT_PF_SOLID		(1<<8)

// what our client supports
#define CLIENT_EXTENSIONS ( \
		Z_EXT_PM_TYPE | \
		Z_EXT_PM_TYPE_NEW | \
		Z_EXT_VIEWHEIGHT | \
		Z_EXT_SERVERTIME | \
		Z_EXT_PITCHLIMITS | \
		Z_EXT_JOIN_OBSERVE | \
		Z_EXT_PF_ONGROUND | \
		Z_EXT_VWEP | \
		Z_EXT_PF_SOLID \
)

// what our server supports
#define SERVER_EXTENSIONS ( \
		Z_EXT_PM_TYPE | \
		Z_EXT_PM_TYPE_NEW | \
		Z_EXT_VIEWHEIGHT | \
		Z_EXT_SERVERTIME | \
		Z_EXT_PITCHLIMITS | \
		Z_EXT_JOIN_OBSERVE | \
		Z_EXT_PF_ONGROUND | \
		Z_EXT_VWEP | \
		Z_EXT_PF_SOLID \
)

//=========================================

// out of band message id bytes

// M = master, S = server, C = client, A = any
// the second character will always be \n if the message isn't a single
// byte long (?? not true anymore?)

#define	S2C_CHALLENGE		'c'
#define	S2C_CONNECTION		'j'
#define	A2A_PING			'k'	// respond with an A2A_ACK
#define	A2A_ACK				'l'	// general acknowledgement without info
#define	A2A_NACK			'm'	// [+ comment] general failure
#define A2A_ECHO			'e' // for echoing
#define	A2C_PRINT			'n'	// print a message on client

#define	S2M_HEARTBEAT		'a'	// + serverinfo + userlist + fraglist
#define	A2C_CLIENT_COMMAND	'B'	// + command line
#define	S2M_SHUTDOWN		'C'


//==================
// note that there are some defs.qc that mirror to these numbers
// also related to svc_strings[] in cl_parse
//==================

//
// server to client
#define	svc_bad					0
#define	svc_nop					1
#define	svc_disconnect			2
#define	svc_updatestat			3		// [byte] [byte]
#define	nq_svc_version			4		// [long] server version
#define	nq_svc_setview			5		// [short] entity number
#define	svc_sound				6		// <see code>
#define	nq_svc_time				7		// [float] server time
#define	svc_print				8		// [byte] id [string] null terminated string
#define	svc_stufftext			9		// [string] stuffed into client's console buffer
										// the string should be \n terminated
#define	svc_setangle			10		// [angle3] set the view angle to this absolute value
	
#define	svc_serverdata			11		// [long] protocol ...
#define	svc_lightstyle			12		// [byte] [string]
#define	nq_svc_updatename		13		// [byte] [string]
#define	svc_updatefrags			14		// [byte] [short]
#define	nq_svc_clientdata		15		// <shortbits + data>
#define	svc_stopsound			16		// <see code>
#define	nq_svc_updatecolors		17		// [byte] [byte] [byte]
#define	nq_svc_particle			18		// [vec3] <variable>
#define	svc_damage				19
	
#define	svc_spawnstatic			20
#define	svc_fte_spawnstatic2	21		// @!@!@!
#define	svc_spawnbaseline		22
	
#define	svc_temp_entity			23		// variable
#define	svc_setpause			24		// [byte] on / off
#define nq_svc_signonnum		25		// [byte]  used for the signon sequence

#define	svc_centerprint			26		// [string] to put in center of the screen

#define	svc_killedmonster		27
#define	svc_foundsecret			28

#define	svc_spawnstaticsound	29		// [coord3] [byte] samp [byte] vol [byte] aten

#define	svc_intermission		30		// [vec3_t] origin [vec3_t] angle
#define	svc_finale				31		// [string] text

#define	svc_cdtrack				32		// [byte] track
#define svc_sellscreen			33

#define nq_svc_cutscene			34		// same as svc_smallkick

#define	svc_smallkick			34		// set client punchangle to 2
#define	svc_bigkick				35		// set client punchangle to 4

#define	svc_updateping			36		// [byte] [short]
#define	svc_updateentertime		37		// [byte] [float]

#define	svc_updatestatlong		38		// [byte] [long]

#define	svc_muzzleflash			39		// [short] entity

#define	svc_updateuserinfo		40		// [byte] slot [long] uid
										// [string] userinfo

#define	svc_download			41		// [short] size [size bytes]
#define	svc_playerinfo			42		// variable
#define	svc_nails				43		// [byte] num [48 bits] xyzpy 12 12 12 4 8 
#define	svc_chokecount			44		// [byte] packets choked
#define	svc_modellist			45		// [strings]
#define	svc_soundlist			46		// [strings]
#define	svc_packetentities		47		// [...]
#define	svc_deltapacketentities	48		// [...]
#define svc_maxspeed			49		// maxspeed change, for prediction
#define svc_entgravity			50		// gravity change, for prediction
#define svc_setinfo				51		// setinfo on a client
#define svc_serverinfo			52		// serverinfo
#define svc_updatepl			53		// [byte] [byte]
#define svc_nails2				54		// [byte] num [52 bits] nxyzpy 8 12 12 12 4 8
										// mvdsv extended svcs (for mvd playback)
#ifdef FTE_PEXT_MODELDBL
#define	svc_fte_modellistshort	60		// [strings]
#endif

#define svc_fte_spawnbaseline2	66
#define svc_qizmovoice			83

#ifdef FTE_PEXT2_VOICECHAT
#define svc_fte_voicechat	    84 		// FTE voice chat.
#endif

//==============================================

//
// client to server
//
#define	clc_bad			0
#define	clc_nop 		1
//define	clc_doublemove	2
#define	clc_move		3		// [[usercmd_t]
#define	clc_stringcmd	4		// [string] message
#define	clc_delta		5		// [byte] sequence number, requests delta compression of message
#define clc_tmove		6		// teleport request, spectator only
#define clc_upload		7		//

#ifdef FTE_PEXT2_VOICECHAT
#define clc_voicechat	83		// FTE voice chat.
#endif

//==============================================

// playerinfo flags from server
// playerinfo always sends: playernum, flags, origin[] and framenumber

#define	PF_MSEC			(1 << 0)
#define	PF_COMMAND		(1 << 1)
#define	PF_VELOCITY1	(1 << 2)
#define	PF_VELOCITY2	(1 << 3)
#define	PF_VELOCITY3	(1 << 4)
#define	PF_MODEL		(1 << 5)
#define	PF_SKINNUM		(1 << 6)
#define	PF_EFFECTS		(1 << 7)
#define	PF_WEAPONFRAME	(1 << 8)		// only sent for view player
#define	PF_DEAD			(1 << 9)		// don't block movement any more
#define	PF_GIB			(1 << 10)		// offset the view height differently
// bits 11..13 are player move type bits (ZQuake extension)
#define PF_PMC_SHIFT	11
#define	PF_PMC_MASK		7
#define	PF_ONGROUND		(1<<14)			// ZQuake extension
#define	PF_SOLID		(1<<15)			// ZQuake extension

// encoded player move types
#define PMC_NORMAL				0		// normal ground movement
#define PMC_NORMAL_JUMP_HELD	1		// normal ground novement + jump_held
#define PMC_OLD_SPECTATOR		2		// fly through walls (QW compatibility mode)
#define PMC_SPECTATOR			3		// fly through walls
#define PMC_FLY					4		// fly, bump into walls
#define PMC_NONE				5		// can't move (client had better lerp the origin...)
#define PMC_LOCK				6		// server controls view angles
#define PMC_EXTRA3				7		// future extension

//==============================================

// if the high bit of the client to server byte is set, the low bits are
// client move cmd bits
// ms and angle2 are always sent, the others are optional
#define	CM_ANGLE1 	(1 << 0)
#define	CM_ANGLE3 	(1 << 1)
#define	CM_FORWARD	(1 << 2)
#define	CM_SIDE		(1 << 3)
#define	CM_UP		(1 << 4)
#define	CM_BUTTONS	(1 << 5)
#define	CM_IMPULSE	(1 << 6)
#define	CM_ANGLE2 	(1 << 7)

//==============================================

//
// Player flags in mvd demos.
// Should be in server.h but unfortunately shared with cl_demo.c.
//

#define DF_ORIGIN		1
#define DF_ANGLES		(1 << 3)
#define DF_EFFECTS		(1 << 6)
#define DF_SKINNUM		(1 << 7)
#define DF_DEAD			(1 << 8)
#define DF_GIB			(1 << 9)
#define DF_WEAPONFRAME	(1 << 10)
#define DF_MODEL		(1 << 11)


//==============================================

// the first 16 bits of a packetentities update holds 9 bits
// of entity number and 7 bits of flags
#define	U_ORIGIN1	(1 << 9)
#define	U_ORIGIN2	(1 << 10)
#define	U_ORIGIN3	(1 << 11)
#define	U_ANGLE2	(1 << 12)
#define	U_FRAME		(1 << 13)
#define	U_REMOVE	(1 << 14)	// REMOVE this entity, don't add it
#define	U_MOREBITS	(1 << 15)

// if MOREBITS is set, these additional flags are read in next
#define	U_ANGLE1	(1 << 0)
#define	U_ANGLE3	(1 << 1)
#define	U_MODEL		(1 << 2)
#define	U_COLORMAP	(1 << 3)
#define	U_SKIN		(1 << 4)
#define	U_EFFECTS	(1 << 5)
#define	U_SOLID		(1 << 6)	// the entity should be solid for prediction


#define	U_CHECKMOREBITS	((1<<9) - 1) /* MVDSV compatibility */


#ifdef PROTOCOL_VERSION_FTE
#define U_FTE_EVENMORE	(1<<7)		//extension info follows

//fte extensions
//EVENMORE flags
#ifdef FTE_PEXT_SCALE
#define U_FTE_SCALE		(1<<0)		//scaler of alias models
#endif
#ifdef FTE_PEXT_TRANS
#define U_FTE_TRANS		(1<<1)		//transparency value
#endif
#ifdef FTE_PEXT_TRANS
#define	PF_TRANS_Z			(1<<17)
#endif
#ifdef FTE_PEXT_FATNESS
#define U_FTE_FATNESS	(1<<2)		//byte describing how fat an alias model should be. 
								//moves verticies along normals
								// Useful for vacuum chambers...
#endif
#ifdef FTE_PEXT_MODELDBL
#define U_FTE_MODELDBL	(1<<3)		//extra bit for modelindexes
#endif
#define U_FTE_UNUSED1	(1<<4)
//FIXME: IMPLEMENT
#ifdef FTE_PEXT_ENTITYDBL
#define U_FTE_ENTITYDBL	(1<<5)		//use an extra byte for origin parts, cos one of them is off
#endif
#ifdef FTE_PEXT_ENTITYDBL2
#define U_FTE_ENTITYDBL2 (1<<6)		//use an extra byte for origin parts, cos one of them is off
#endif
#define U_FTE_YETMORE	(1<<7)		//even more extension info stuff.
#define U_FTE_DRAWFLAGS	(1<<8)		//use an extra qbyte for origin parts, cos one of them is off
#define U_FTE_ABSLIGHT	(1<<9)		//Force a lightlevel
#define U_FTE_COLOURMOD	(1<<10)		//rgb
#define U_FTE_DPFLAGS (1<<11)
#define U_FTE_TAGINFO (1<<12)
#define U_FTE_LIGHT (1<<13)
#define	U_FTE_EFFECTS16	(1<<14)
#define U_FTE_FARMORE (1<<15)
#endif

//==============================================

// a sound with no channel is a local only sound
// the sound field has bits 0-2: channel, 3-12: entity
#define	SND_VOLUME		(1 << 15)		// a byte
#define	SND_ATTENUATION	(1 << 14)		// a byte

#define DEFAULT_SOUND_PACKET_VOLUME 255
#define DEFAULT_SOUND_PACKET_ATTENUATION 1.0

// svc_print messages have an id, so messages can be filtered
#define	PRINT_LOW			0
#define	PRINT_MEDIUM		1
#define	PRINT_HIGH			2
#define	PRINT_CHAT			3	// also go to chat buffer

//
// temp entity events
//
#define	TE_SPIKE			0
#define	TE_SUPERSPIKE		1
#define	TE_GUNSHOT			2
#define	TE_EXPLOSION		3
#define	TE_TAREXPLOSION		4
#define	TE_LIGHTNING1		5
#define	TE_LIGHTNING2		6
#define	TE_WIZSPIKE			7
#define	TE_KNIGHTSPIKE		8
#define	TE_LIGHTNING3		9
#define	TE_LAVASPLASH		10
#define	TE_TELEPORT			11
#define	TE_BLOOD			12
#define	TE_LIGHTNINGBLOOD	13

#define NQ_TE_EXPLOSION2	12
#define NQ_TE_BEAM			13

#define	DEFAULT_VIEWHEIGHT	22

/*
==========================================================

  ELEMENTS COMMUNICATED ACROSS THE NET

==========================================================
*/

#define	MAX_CLIENTS		32

#define	UPDATE_BACKUP	64	// copies of entity_state_t to keep buffered (must be power of two)
#define	UPDATE_MASK		(UPDATE_BACKUP - 1)

// entity_state_t is the information conveyed from the server
// in an update message
typedef struct entity_state_s
{
	int		number;			// edict index
	int		flags;			// nolerp, etc

	vec3_t	origin;
	vec3_t	angles;
	int		modelindex;
	int		frame;
	int		colormap;
	int		skinnum;
	int		effects;
#ifndef SERVERONLY
// Our server does not use it.
	byte	trans;
#endif
} entity_state_t;

#define	MAX_PACKET_ENTITIES	64	// doesn't count nails
#define MAX_PEXT256_PACKET_ENTITIES 256 // up to 256 ents, look FTE_PEXT_256PACKETENTITIES
#define MAX_MVD_PACKET_ENTITIES 300 // !!! MUST not be less than any of above values!!!

typedef struct packet_entities_s
{
	int		num_entities;
	entity_state_t	entities[MAX_MVD_PACKET_ENTITIES];
} packet_entities_t;

typedef struct usercmd_s
{
	byte	msec;
	vec3_t	angles;
	short	forwardmove, sidemove, upmove;
	byte	buttons;
	byte	impulse;
} usercmd_t;

// usercmd button bits
#define BUTTON_ATTACK	(1 << 0)
#define BUTTON_JUMP		(1 << 1)
#define BUTTON_USE		(1 << 2)
#define BUTTON_ATTACK2	(1 << 3)

//
// demo recording
//

// TODO: Make into an enum.
#define dem_cmd			0 // A user cmd movement message.
#define dem_read		1 // A net message.
#define dem_set			2 // Appears only once at the beginning of a demo, 
						  // contains the outgoing / incoming sequence numbers at demo start.
#define dem_multiple	3 // MVD ONLY. This message is directed to several clients.
#define	dem_single		4 // MVD ONLY. This message is directed to a single client.
#define dem_stats		5 // MVD ONLY. Stats update for a player.
#define dem_all			6 // MVD ONLY. This message is directed to all clients.

#endif /* !__PROTOCOL_H__ */
