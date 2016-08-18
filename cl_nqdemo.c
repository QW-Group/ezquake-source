/*
Copyright (C) 2011 tonik

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "cdaudio.h"
#include "stats_grid.h"
#include "sbar.h"
#include "qsound.h"
#include "hud.h"
#include "hud_common.h"
#include "vx_stuff.h"
#include "settings.h"
#include "teamplay.h"
#ifdef _WIN32
#include "movie.h"
#endif

#define MAX_BIG_MSGLEN 8000
int CL_Demo_Read(void *buf, int size, qbool peek);
#define SCR_EndLoadingPlaque()
int cl_entframecount;
#define CL_EntityParticles(a)
extern cvar_t cl_rocket2grenade;
#define R_ModelFlags(model) model->flags
#define V_AddDlight(a,b,c,d,e)

void CL_FindModelNumbers (void);
void TP_NewMap (void);
void CL_ParseBaseline (entity_state_t *es);
void CL_ParseStatic (qbool extended);
void CL_ParseStaticSound (void);

#define	NQ_PROTOCOL_VERSION	15

#define	NQ_MAX_CLIENTS	16
#define NQ_SIGNONS		4

#define	NQ_MAX_EDICTS	600			// Must be <= MAX_CL_EDICTS!

// if the high bit of the servercmd is set, the low bits are fast update flags:
#define	NQ_U_MOREBITS	(1<<0)
#define	NQ_U_ORIGIN1	(1<<1)
#define	NQ_U_ORIGIN2	(1<<2)
#define	NQ_U_ORIGIN3	(1<<3)
#define	NQ_U_ANGLE2		(1<<4)
#define	NQ_U_NOLERP		(1<<5)		// don't interpolate movement
#define	NQ_U_FRAME		(1<<6)
#define NQ_U_SIGNAL		(1<<7)		// just differentiates from other updates
#define	NQ_U_ANGLE1		(1<<8)
#define	NQ_U_ANGLE3		(1<<9)
#define	NQ_U_MODEL		(1<<10)
#define	NQ_U_COLORMAP	(1<<11)
#define	NQ_U_SKIN		(1<<12)
#define	NQ_U_EFFECTS	(1<<13)
#define	NQ_U_LONGENTITY	(1<<14)

#define	SU_VIEWHEIGHT	(1<<0)
#define	SU_IDEALPITCH	(1<<1)
#define	SU_PUNCH1		(1<<2)
#define	SU_PUNCH2		(1<<3)
#define	SU_PUNCH3		(1<<4)
#define	SU_VELOCITY1	(1<<5)
#define	SU_VELOCITY2	(1<<6)
#define	SU_VELOCITY3	(1<<7)
//define	SU_AIMENT		(1<<8)  AVAILABLE BIT
#define	SU_ITEMS		(1<<9)
#define	SU_ONGROUND		(1<<10)		// no data follows, the bit is it
#define	SU_INWATER		(1<<11)		// no data follows, the bit is it
#define	SU_WEAPONFRAME	(1<<12)
#define	SU_ARMOR		(1<<13)
#define	SU_WEAPON		(1<<14)

// a sound with no channel is a local only sound
#define	NQ_SND_VOLUME		(1<<0)		// a byte
#define	NQ_SND_ATTENUATION	(1<<1)		// a byte
#define	NQ_SND_LOOPING		(1<<2)		// a long


//=========================================================================================

qbool	nq_drawpings;	// for sbar code

static qbool	nq_player_teleported;	// hacky

static vec3_t	nq_last_fixangle;

static int		nq_num_entities;
static int		nq_viewentity;
static int		nq_forcecdtrack;
static int		nq_signon;
static int		nq_maxclients;
static float	nq_mtime[2];
static vec3_t	nq_mvelocity[2];
static vec3_t	nq_mviewangles[2];
static vec3_t	nq_mviewangles_temp;
static qbool	standard_quake = true;
static demoseekingtype_t nq_lastseektype = DST_SEEKING_NONE;

static void CL_CheckForNQDSeekPointFound(void) 
{
	float demotime = nq_mtime[0];

	// Check for rewind only the first time we want to seek through the demo 
	if (cls.demoseeking != nq_lastseektype)
		CL_Demo_Check_For_Rewind(nq_mtime[0]);
	nq_lastseektype = cls.demoseeking;
		
	if (cls.demoseeking == DST_SEEKING_STATUS)
		CL_Demo_Jump_Status_Check();

	// If we found demomark, we should stop seeking, so reset time to the proper value.
	if (cls.demoseeking == DST_SEEKING_FOUND) 
	{
		extern cvar_t demo_jump_rewind;

		cls.demotime = demotime; // this will trigger seeking stop

		if (demo_jump_rewind.value < 0) {
			CL_Demo_Jump (-demo_jump_rewind.value, -1, DST_SEEKING_NORMAL);
		}
	}

	// If we've reached our seek goal, stop seeking.
	if (cls.demoseeking && cls.demotime <= demotime && cls.state >= ca_active)
	{
		cls.demoseeking = DST_SEEKING_NONE;
		cls.demotime = demotime; 

		if (cls.demorewinding)
		{
			CL_Demo_Stop_Rewinding();
		}
	}
}

static qbool CL_GetNQDemoMessage (void)
{
	int i;
	float f;
	extern qbool pb_ensure(void);

	if(!pb_ensure())
		return false;

	// decide if it is time to grab the next message		
	if (cls.state == ca_active				// always grab until fully connected
		&& !(cl.paused & PAUSED_SERVER))	// or if the game was paused by server
	{
		if (cls.timedemo)
		{
			if (cls.framecount == cls.td_lastframe)
				return false;		// already read this frame's message
			cls.td_lastframe = cls.framecount;
			// if this is the second frame, grab the real td_starttime
			// so the bogus time on the first frame doesn't count
			if (cls.framecount == cls.td_startframe + 1)
				cls.td_starttime = Sys_DoubleTime();
		}
		else if (cl.time <= nq_mtime[0] && ! cls.demoseeking)
			return false;		// don't need another message yet
	}

	// get the next message
	CL_Demo_Read(&net_message.cursize, 4, false);
	for (i=0 ; i<3 ; i++) {
		CL_Demo_Read(&f, 4, false);
		nq_mviewangles_temp[i] = LittleFloat (f);
	}

	net_message.cursize = LittleLong (net_message.cursize);
	if (net_message.cursize > MAX_BIG_MSGLEN)
		Host_Error ("Demo message > MAX_BIG_MSGLEN");

	CL_Demo_Read(net_message.data, net_message.cursize, false);
	return true;
}


static void NQD_BumpEntityCount (int num)
{
	if (num >= nq_num_entities)
		nq_num_entities = num + 1;
}


static void NQD_ParseClientdata (int bits)
{
	int		i, j;
	extern player_state_t view_message;

	if (bits & SU_VIEWHEIGHT)
		cl.stats[STAT_VIEWHEIGHT] = MSG_ReadChar ();
	else
		cl.stats[STAT_VIEWHEIGHT] = DEFAULT_VIEWHEIGHT;

	if (bits & SU_IDEALPITCH)
		MSG_ReadChar ();		// ignore
	
	VectorCopy (nq_mvelocity[0], nq_mvelocity[1]);
	for (i=0 ; i<3 ; i++)
	{
		if (bits & (SU_PUNCH1<<i) ) {
			if (i == 0)
				cl.punchangle = MSG_ReadChar ();
			else
				MSG_ReadChar();			// ignore
		}
		if (bits & (SU_VELOCITY1<<i) )
			nq_mvelocity[0][i] = MSG_ReadChar()*16;
		else
			nq_mvelocity[0][i] = 0;
	}

// [always sent]	if (bits & SU_ITEMS)
	i = MSG_ReadLong ();			// FIXME, check SU_ITEMS anyway? -- Tonik

	if (cl.stats[STAT_ITEMS] != i)
	{	// set flash times
		Sbar_Changed ();
		for (j=0 ; j<32 ; j++)
			if ( (i & (1<<j)) && !(cl.stats[STAT_ITEMS] & (1<<j)))
				cl.item_gettime[j] = cl.time;
		cl.stats[STAT_ITEMS] = i;
	}
		
	cl.onground = (bits & SU_ONGROUND) != 0;
//	cl.inwater = (bits & SU_INWATER) != 0;

	if (bits & SU_WEAPONFRAME)
		view_message.weaponframe = MSG_ReadByte ();
	else
		view_message.weaponframe = 0;

	if (bits & SU_ARMOR)
		i = MSG_ReadByte ();
	else
		i = 0;
	if (cl.stats[STAT_ARMOR] != i)
	{
		cl.stats[STAT_ARMOR] = i;
		Sbar_Changed ();
	}

	if (bits & SU_WEAPON)
		i = MSG_ReadByte ();
	else
		i = 0;
	if (cl.stats[STAT_WEAPON] != i)
	{
		cl.stats[STAT_WEAPON] = i;
		Sbar_Changed ();
	}
	
	i = MSG_ReadShort ();
	if (cl.stats[STAT_HEALTH] != i)
	{
		cl.stats[STAT_HEALTH] = i;
		Sbar_Changed ();
	}

	i = MSG_ReadByte ();
	if (cl.stats[STAT_AMMO] != i)
	{
		cl.stats[STAT_AMMO] = i;
		Sbar_Changed ();
	}

	for (i=0 ; i<4 ; i++)
	{
		j = MSG_ReadByte ();
		if (cl.stats[STAT_SHELLS+i] != j)
		{
			cl.stats[STAT_SHELLS+i] = j;
			Sbar_Changed ();
		}
	}

	i = MSG_ReadByte ();

	if (standard_quake)
	{
		if (cl.stats[STAT_ACTIVEWEAPON] != i)
		{
			cl.stats[STAT_ACTIVEWEAPON] = i;
			Sbar_Changed ();
		}
	}
	else
	{
		if (cl.stats[STAT_ACTIVEWEAPON] != (1<<i))
		{
			cl.stats[STAT_ACTIVEWEAPON] = (1<<i);
			Sbar_Changed ();
		}
	}
}

static void NQD_SetSpectatorFlag(player_info_t* player)
{
	player->spectator = (cl.teamplay && player->name[0] && player->real_topcolor == 0 && player->real_bottomcolor == 0 && player->frags <= -99);
}

void NQD_SetSpectatorFlags(void)
{
	int i;

	for (i = 0; i < sizeof(cl.players) / sizeof(cl.players[0]); ++i)
		NQD_SetSpectatorFlag(&cl.players[i]);
}

/*
==================
NQD_ParseUpdatecolors
==================
*/
static void NQD_ParseUpdatecolors (void)
{
	int	i, colors;
	int top, bottom;
	qbool client_team_changed = false;
	qbool player_skin_changed = false;

	i = MSG_ReadByte ();
	if (i >= nq_maxclients)
		Host_Error ("CL_ParseServerMessage: svc_updatecolors > NQ_MAX_CLIENTS");
	colors = MSG_ReadByte ();

	// fill in userinfo values
	top = min(colors & 15, 13);
	bottom = min((colors >> 4) & 15, 13);

	client_team_changed = (cl.playernum == i && bottom != cl.players[i].real_bottomcolor);
	player_skin_changed = (top != cl.players[i].real_topcolor || bottom != cl.players[i].real_bottomcolor);

	Info_SetValueForKey (cl.players[i].userinfo, "topcolor", va("%i", top), MAX_INFO_KEY);
	Info_SetValueForKey (cl.players[i].userinfo, "bottomcolor", va("%i", bottom), MAX_INFO_KEY);

	cl.players[i].real_topcolor = top;
	cl.players[i].real_bottomcolor = bottom;

	NQD_SetSpectatorFlag(&cl.players[i]);

	// Update team (based on bottom colour)
	if (cl.players[i].spectator)
	{
		Info_SetValueForKey (cl.players[i].userinfo, "team", "", MAX_INFO_KEY);
		strlcpy(cl.players[i].team, "", sizeof(cl.players[i].team));
	}
	else 
	{
		char* bottom_as_string = SettingColorName(bottom); 
		Info_SetValueForKey (cl.players[i].userinfo, "team", bottom_as_string, MAX_INFO_KEY);
		strlcpy(cl.players[i].team, bottom_as_string, sizeof(cl.players[i].team));
	}

	// Update skins
	if (TP_NeedRefreshSkins() && client_team_changed)
		TP_RefreshSkins();
	else if (player_skin_changed)
		TP_RefreshSkin(i);
	Sbar_Changed ();
}

			
/*
==================
NQD_ParsePrint
==================
*/
static void NQD_ParsePrint (void)
{
	extern cvar_t	cl_chatsound;

	char *s = MSG_ReadString();
	if (s[0] == 1) {	// chat
		if (cl_chatsound.value)
			S_LocalSound ("misc/talk.wav");
	}
	Com_Printf ("%s", s);
}


// JPG's ProQuake hacks
static int ReadPQByte (void) {
	int word;
	word = MSG_ReadByte() * 16;
	word += MSG_ReadByte() - 272;
	return word;
}
static int ReadPQShort (void) {
	int word;
	word = ReadPQByte() * 256;
	word += ReadPQByte();
	return word;
}

/*
==================
NQD_ParseStufftext
==================
*/
static void NQD_ParseStufftext (void)
{
	char	*s;
	byte	*p;

	if (msg_readcount + 7 <= net_message.cursize &&
		net_message.data[msg_readcount] == 1 && net_message.data[msg_readcount + 1] == 7)
	{
		int num, ping;
		MSG_ReadByte();
		MSG_ReadByte();
		while ((ping = ReadPQShort()) != 0)
		{
			num = ping / 4096;
			if ((unsigned int)num >= nq_maxclients)
				Host_Error ("Bad ProQuake message");
			cl.players[num].ping = ping & 4095;
			nq_drawpings = true;
		}
		// fall through to stufftext parsing (yes that's how it's intended by JPG)
	}

	s = MSG_ReadString ();
	Com_DPrintf ("stufftext: %s\n", s);

	for (p = (byte *)s; *p; p++) {
		if (*p > 32 && *p < 128)
			goto ok;
	}
	// ignore weird ProQuake stuff
	return;

ok:
	Cbuf_AddTextEx (&cbuf_svc, s);
}


/*
==================
NQD_ParseServerData
==================
*/
static void NQD_ParseServerData (void)
{
	char	*str;
	int		i;
	int		nummodels, numsounds;
	char	mapname[MAX_QPATH];

	Com_DPrintf ("Serverdata packet received.\n");

	// wipe the client_state_t struct
	CL_ClearState ();

	// parse protocol version number
	i = MSG_ReadLong ();
	if (i != NQ_PROTOCOL_VERSION)
		Host_Error ("Server returned version %i, not %i", i, NQ_PROTOCOL_VERSION);

	// parse maxclients
	nq_maxclients = MSG_ReadByte ();
	if (nq_maxclients < 1 || nq_maxclients > NQ_MAX_CLIENTS)
		Host_Error ("Bad maxclients (%u) from server", nq_maxclients);

	// parse gametype
	cl.gametype = MSG_ReadByte() ? GAME_DEATHMATCH : GAME_COOP;

	// parse signon message
	str = MSG_ReadString ();
	strlcpy (cl.levelname, str, sizeof (cl.levelname));

	// separate the printfs so the server message can have a color
	Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
	Com_Printf ("%c%s\n", 2, str);

//
// first we go through and touch all of the precache data that still
// happens to be in the cache, so precaching something else doesn't
// needlessly purge it
//

	// precache models
	*mapname = 0;
	for (nummodels=1 ; ; nummodels++)
	{
		str = MSG_ReadString ();
		if (!str[0])
			break;
		if (nummodels == MAX_MODELS)
			Host_Error ("Server sent too many model precaches");
		strlcpy (cl.model_name[nummodels], str, sizeof(cl.model_name[0]));
		Mod_TouchModel (str);

		if (nummodels == 1)
			COM_StripExtension (COM_SkipPath(cl.model_name[1]), mapname, sizeof(mapname));
	}

	// precache sounds
	for (numsounds=1 ; ; numsounds++)
	{
		str = MSG_ReadString ();
		if (!str[0])
			break;
		if (numsounds == MAX_SOUNDS)
			Host_Error ("Server sent too many sound precaches");
		strlcpy (cl.sound_name[numsounds], str, sizeof(cl.sound_name[0]));
//		S_TouchSound (str); @ZQ@
	}

	// now we try to load everything else until a cache allocation fails
	cl.clipmodels[1] = CM_LoadMap (cl.model_name[1], true, NULL, &cl.map_checksum2);
	if (!com_serveractive)
		Cvar_ForceSet (&host_mapname, mapname);
	COM_StripExtension (COM_SkipPath(cl.model_name[1]), mapname, sizeof(mapname));
	cl.map_checksum2 = Com_TranslateMapChecksum (mapname, cl.map_checksum2);

	for (i = 1; i < nummodels; i++)
	{
		cl.model_precache[i] = Mod_ForName (cl.model_name[i], false);
		if (cl.model_precache[i] == NULL)
			Host_Error ("Model %s not found", cl.model_name[i]);

		if (cl.model_name[i][0] == '*')
			cl.clipmodels[i] = CM_InlineModel(cl.model_name[i]);
	}

	for (i=1 ; i<numsounds ; i++) {
		cl.sound_precache[i] = S_PrecacheSound (cl.sound_name[i]);
	}


	// local state
	cl.worldmodel = cl.model_precache[1];
	if (!cl.model_precache[1])
		Host_Error ("NQD_ParseServerData: NULL worldmodel");

	Classic_InitParticles (); 
	CL_FindModelNumbers ();
	R_NewMap (false);
	TP_NewMap ();
	MT_NewMap ();
	Stats_NewMap ();

	// Reset the status grid.
	StatsGrid_Remove (&stats_grid);
	StatsGrid_ResetHoldItems ();
	HUD_NewMap ();

	Hunk_Check (); // make sure nothing is hurt

	nq_signon = 0;
	nq_num_entities = 0;
	nq_drawpings = false; // unless we have the ProQuake extension
	cl.servertime_works = true;
//	cl.allow_fbskins = true; @ZQ@
	cls.state = ca_onserver;
	CL_Demo_Check_For_Rewind(nq_mtime[0]);

	// Demos don't store this so we set from user-defined option
	cl.teamplay = cl_demoteamplay.integer;

	nq_lastseektype = DST_SEEKING_NONE;
}

/*
==================
NQD_ParseStartSoundPacket
==================
*/
static void NQD_ParseStartSoundPacket(void)
{
    vec3_t  pos;
    int 	channel, ent;
    int 	sound_num;
    int 	volume;
    int 	field_mask;
    float 	attenuation;  
 	int		i;
	           
    field_mask = MSG_ReadByte(); 

    if (field_mask & NQ_SND_VOLUME)
		volume = MSG_ReadByte ();
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;
	
    if (field_mask & NQ_SND_ATTENUATION)
		attenuation = MSG_ReadByte () / 64.0;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;
	
	channel = MSG_ReadShort ();
	sound_num = MSG_ReadByte ();

	ent = channel >> 3;
	channel &= 7;

	if (ent > NQ_MAX_EDICTS)
		Host_Error ("NQD_ParseStartSoundPacket: ent = %i", ent);
	
	for (i=0 ; i<3 ; i++)
		pos[i] = MSG_ReadCoord ();
 
    S_StartSound (ent, channel, cl.sound_precache[sound_num], pos, volume/255.0, attenuation);
}       


/*
===============
CL_ParseParticleEffect

Back from NetQuake
===============
*/
static void CL_ParseParticleEffect (void)
{
	vec3_t		org, dir;
	int			i, count, color;

	for (i = 0; i < 3; i++)
		org[i] = MSG_ReadCoord ();
	for (i = 0; i < 3; i++)
		dir[i] = MSG_ReadChar () * (1.0/16);
	count = MSG_ReadByte ();
	color = MSG_ReadByte ();

	// now run the effect
	if (count == 255)
		Classic_ParticleExplosion (org);
	else 
		R_RunParticleEffect (org, dir, color, count);
}

/*
==================
NQD_ParseUpdate

Parse an entity update message from the server
If an entities model or origin changes from frame to frame, it must be
relinked.  Other attributes can change without relinking.
==================
*/
static void NQD_ParseUpdate (int bits)
{
	int			i;
//	model_t		*model;
	int			modnum;
	qbool		forcelink;
	centity_t	*ent;
	entity_state_t	*state;
	int			num;
	int prev_frame;

	if (nq_signon == NQ_SIGNONS - 1)
	{	// first update is the final signon stage
		nq_signon = NQ_SIGNONS;
		Con_ClearNotify ();
		//TP_ExecTrigger ("f_spawn");
		SCR_EndLoadingPlaque ();
		cls.state = ca_active;
		CL_Demo_Check_For_Rewind(nq_mtime[0]);
	}

	if (bits & NQ_U_MOREBITS)
	{
		i = MSG_ReadByte ();
		bits |= (i<<8);
	}

	if (bits & NQ_U_LONGENTITY)	
		num = MSG_ReadShort ();
	else
		num = MSG_ReadByte ();

	if (num >= NQ_MAX_EDICTS)
		Host_Error ("NQD_ParseUpdate: ent > MAX_EDICTS");

	NQD_BumpEntityCount (num);

	ent = &cl_entities[num];
//##	ent->previous = ent->current;
	VectorCopy (ent->current.origin, ent->old_origin);	//##
	VectorCopy (ent->current.angles, ent->old_angles);	//##
	prev_frame = ent->current.frame;
	ent->current = ent->baseline;
	state = &ent->current;
	state->number = num;

	if (ent->sequence != cl_entframecount - 1)
		forcelink = true;	// no previous frame to lerp from
	else
		forcelink = false;
	ent->oldsequence = ent->sequence;
	ent->sequence = cl_entframecount;
	
	if (bits & NQ_U_MODEL)
	{
		modnum = MSG_ReadByte ();
		if (modnum >= MAX_MODELS)
			Host_Error ("CL_ParseModel: bad modnum");
	}
	else
		modnum = ent->baseline.modelindex;
		
//	model = cl.model_precache[modnum];
	if (modnum != state->modelindex)
	{
		state->modelindex = modnum;
		// automatic animation (torches, etc) can be either all together
		// or randomized
		if (modnum)
		{
			/*if (model->synctype == ST_RAND)
				state->syncbase = (float)(rand()&0x7fff) / 0x7fff;
			else
				state->syncbase = 0.0;
				*/
		}
		else
			forcelink = true;	// hack to make null model players work
	}
	
	if (bits & NQ_U_FRAME)
		state->frame = MSG_ReadByte ();
	else
		state->frame = ent->baseline.frame;

	if (bits & NQ_U_COLORMAP)
		i = MSG_ReadByte();
	else
		i = ent->baseline.colormap;

	state->colormap = i;

	if (bits & NQ_U_SKIN)
		state->skinnum = MSG_ReadByte();
	else
		state->skinnum = ent->baseline.skinnum;

	if (bits & NQ_U_EFFECTS)
		state->effects = MSG_ReadByte();
	else
		state->effects = 0;

	if (bits & NQ_U_ORIGIN1)
		state->origin[0] = MSG_ReadCoord ();
	else
		state->origin[0] = ent->baseline.origin[0];
	if (bits & NQ_U_ANGLE1)
		state->angles[0] = MSG_ReadAngle ();
	else
		state->angles[0] = ent->baseline.angles[0];

	if (bits & NQ_U_ORIGIN2)
		state->origin[1] = MSG_ReadCoord ();
	else
		state->origin[1] = ent->baseline.origin[1];
	if (bits & NQ_U_ANGLE2)
		state->angles[1] = MSG_ReadAngle ();
	else
		state->angles[1] = ent->baseline.angles[1];

	if (bits & NQ_U_ORIGIN3)
		state->origin[2] = MSG_ReadCoord ();
	else
		state->origin[2] = ent->baseline.origin[2];
	if (bits & NQ_U_ANGLE3)
		state->angles[2] = MSG_ReadAngle ();
	else
		state->angles[2] = ent->baseline.angles[2];

	if ( bits & NQ_U_NOLERP )
		forcelink = true;

	if (state->frame != prev_frame) {
		ent->frametime = cl.time;
		ent->oldframe = prev_frame;
	}
	
	if ( forcelink )
	{	// didn't have an update last message
		VectorCopy (state->origin, ent->old_origin);
		VectorCopy (state->origin, ent->lerp_origin);
		VectorCopy (state->angles, ent->old_angles);
//we get U_NOLERP for monsters, but we want to lerp them	ent->frametime = -1;
		//ent->forcelink = true;
	}
}


/*
===============
NQD_LerpPoint

Determines the fraction between the last two messages that the objects
should be put at.
===============
*/
static float NQD_LerpPoint (void)
{
	float	f, frac;

	f = nq_mtime[0] - nq_mtime[1];
	
	if (!f || /* cl_nolerp.value || */ cls.timedemo) {
		cl.time = nq_mtime[0];
		return 1;
	}
		
	if (f > 0.1)
	{	// dropped packet, or start of demo
		nq_mtime[1] = nq_mtime[0] - 0.1;
		f = 0.1;
	}
	frac = (cl.time - nq_mtime[1]) / f;
	if (frac < 0)
	{
		if (frac < -0.01)
			cl.time = nq_mtime[1];
		frac = 0;
	}
	else if (frac > 1)
	{
		if (frac > 1.01)
			cl.time = nq_mtime[0];
		frac = 1;
	}
		
	return frac;
}

static void NQD_LerpPlayerinfo (float f)
{
	if (cl.intermission) {
		// just stay there
		return;
	}

	if (nq_player_teleported) {
		VectorCopy (nq_mvelocity[0], cl.simvel);
		VectorCopy (nq_mviewangles[0], cl.viewangles);
		VectorCopy (nq_mviewangles[0], cl.simangles);
		return;
	}

	VectorInterpolate (nq_mvelocity[1], f, nq_mvelocity[0], cl.simvel);
	AngleInterpolate (nq_mviewangles[1], f, nq_mviewangles[0], cl.simangles);
	VectorCopy (cl.simangles, cl.viewangles);
}

static int NQD_FirstPersonCamera(void)
{
	return !Cvar_Value("cam_thirdperson") && !Cvar_Value("cl_camera_tpp");
}

void NQD_LinkEntities (void)
{
	entity_t			ent;
	centity_t			*cent;
	entity_state_t		*state;
	float				f;
	struct model_s		*model;
	int					modelflags;
	vec3_t				cur_origin;
	vec3_t				old_origin;
	float				autorotate;
	int					i;
	int					num;
	customlight_t		cst_lt = { 0 };

	f = NQD_LerpPoint ();

	NQD_LerpPlayerinfo (f);

	autorotate = anglemod (100*cl.time);

	memset (&ent, 0, sizeof(ent));

	for (num = 1; num < nq_num_entities; num++)
	{
		cent = &cl_entities[num];
		state = &cent->current;

		if (cent->sequence != cl_entframecount)
			continue;		// not present in this frame

		VectorCopy (state->origin, cur_origin);

		if (state->effects & EF_BRIGHTFIELD)
			CL_EntityParticles (cur_origin);

		// spawn light flashes, even ones coming from invisible objects
		if (state->effects & EF_MUZZLEFLASH) {
			vec3_t		angles, forward;
			dlight_t	*dl;

			dl = CL_AllocDlight (-num);
			VectorCopy (state->angles, angles);
			AngleVectors (angles, forward, NULL, NULL);
			VectorMA (cur_origin, 18, forward, dl->origin);
			dl->origin[2] += 16;
			dl->radius = 200 + (rand()&31);
			dl->minlight = 32;
			dl->die = cl.time + 0.1;
			dl->type = lt_muzzleflash;
		}
		if (state->effects & EF_BRIGHTLIGHT) {
			if (state->modelindex != cl_modelindices[mi_player] || r_powerupglow.value) {
				vec3_t	tmp;
				VectorCopy (cur_origin, tmp);
				tmp[2] += 16;
				V_AddDlight (state->number, tmp, 400 + (rand()&31), 0, lt_default);
			}
		}
		if (state->effects & EF_DIMLIGHT)
			if (state->modelindex != cl_modelindices[mi_player] || r_powerupglow.value)
				V_AddDlight (state->number, cur_origin, 200 + (rand()&31), 0, lt_default);

		// if set to invisible, skip
		if (!state->modelindex)
			continue;

		cent->current = *state;

		ent.model = model = cl.model_precache[state->modelindex];
		if (!model)
			Host_Error ("CL_LinkPacketEntities: bad modelindex");

		if (cl_rocket2grenade.value && cl_modelindices[mi_grenade]!= -1)
			if (state->modelindex == cl_modelindices[mi_rocket])
				ent.model = cl.model_precache[cl_modelindices[mi_grenade]];

		modelflags = R_ModelFlags (model);

		// rotate binary objects locally
		if (modelflags & EF_ROTATE)
		{
			ent.angles[0] = 0;
			ent.angles[1] = autorotate;
			ent.angles[2] = 0;
		}
		else
			AngleInterpolate (cent->old_angles, f ,cent->current.angles, ent.angles);

		// calculate origin
		for (i = 0; i < 3; i++)
		{
			if (abs(cent->current.origin[i] - cent->old_origin[i]) > 128) {
				// teleport or something, don't lerp
				VectorCopy (cur_origin, ent.origin);
				if (num == nq_viewentity)
					nq_player_teleported = true;
				break;
			}
			ent.origin[i] = cent->old_origin[i] + 
				f * (cur_origin[i] - cent->old_origin[i]);
		}

		if (num == nq_viewentity) {
			VectorCopy (ent.origin, cent->lerp_origin);	// FIXME?

			if (!cls.nqdemoplayback || NQD_FirstPersonCamera())
				continue;			// player entity
		}

		if (cl_deadbodyfilter.value && state->modelindex == cl_modelindices[mi_player]
			&& ( (i=state->frame)==49 || i==60 || i==69 || i==84 || i==93 || i==102) )
			continue;

		if (cl_gibfilter.value &&
			(state->modelindex == cl_modelindices[mi_h_player] || state->modelindex == cl_modelindices[mi_gib1]
			|| state->modelindex == cl_modelindices[mi_gib2] || state->modelindex == cl_modelindices[mi_gib3]))
			continue;

		// set colormap
		if (state->colormap && state->colormap <= MAX_CLIENTS
			&& state->modelindex == cl_modelindices[mi_player]
		) 
		{
			ent.colormap = cl.players[state->colormap-1].translations;
			ent.scoreboard = &cl.players[state->colormap-1];
		}
		else
		{
			ent.colormap = vid.colormap;
			ent.scoreboard = NULL;
		}

		// set skin
		ent.skinnum = state->skinnum;

		// set frame
		ent.frame = state->frame;
		if (cent->frametime >= 0 && cent->frametime <= cl.time) {
			ent.oldframe = cent->oldframe;
			ent.framelerp = (cl.time - cent->frametime) * 10;
			//if (ent.oldframe != ent.frame) 
				//Com_DPrintf("framelerp %f\n", ent.framelerp);
		} else {
			ent.oldframe = ent.frame;
			ent.framelerp = -1;
		}
		
		// add automatic particle trails
		if ((modelflags & ~EF_ROTATE))
		{
			if (false /*cl_entframecount == 1 || cent->lastframe != cl_entframecount-1*/)
			{	// not in last message
				VectorCopy (ent.origin, old_origin);
			}
			else
			{
				VectorCopy (cent->lerp_origin, old_origin);

				for (i=0 ; i<3 ; i++)
					if ( abs(old_origin[i] - ent.origin[i]) > 128)
					{	// no trail if too far
						VectorCopy (ent.origin, old_origin);
						break;
					}
			}

			CL_AddParticleTrail (&ent, cent, &old_origin, &cst_lt, state);
		}

		VectorCopy (ent.origin, cent->lerp_origin);
		cent->sequence = cl_entframecount;
		CL_AddEntity (&ent);
	}

	if (nq_viewentity == 0)
		Host_Error ("viewentity == 0");
	VectorCopy (cl_entities[nq_viewentity].lerp_origin, cl.simorg);
}





extern char *svc_strings[];
extern const int num_svc_strings;

#define SHOWNET(x) {if(cl_shownet.value==2)Com_Printf ("%3i:%s\n", msg_readcount-1, x);}

static void NQD_ParseServerMessage (void)
{
	int		cmd;
	int		i;
	qbool	message_with_datagram;		// hack to fix glitches when receiving a packet
											// without a datagram

	nq_player_teleported = false;		// OMG, it's a hack!
	message_with_datagram = false;
	cl_entframecount++;

	if (cl_shownet.value == 1)
		Com_Printf ("%i ", net_message.cursize);
	else if (cl_shownet.value == 2)
		Com_Printf ("------------------\n");
	
	cl.onground = false;	// unless the server says otherwise	

//
// parse the message
//
	MSG_BeginReading ();
	while (1)
	{
		if (msg_badread)
			Host_Error ("CL_ParseServerMessage: Bad server message");

		cmd = MSG_ReadByte ();

		if (cmd == -1)
		{
			SHOWNET("END OF MESSAGE");
			if (!message_with_datagram) {
				cl_entframecount--;
			}
			else
			{
				VectorCopy (nq_mviewangles[0], nq_mviewangles[1]);
				VectorCopy (nq_mviewangles_temp, nq_mviewangles[0]);
			}
			return;		// end of message
		}

	// if the high bit of the command byte is set, it is a fast update
		if (cmd & 128)
		{
			SHOWNET("fast update");
			NQD_ParseUpdate (cmd&127);
			continue;
		}

		if (cmd < num_svc_strings)
			SHOWNET(svc_strings[cmd]);
	
	// other commands
		switch (cmd)
		{
		default:
			Host_Error ("CL_ParseServerMessage: Illegible server message (cmd %i)\n", cmd);
			break;

		case svc_nop:
			break;

		case nq_svc_time:
			nq_mtime[1] = nq_mtime[0];
			nq_mtime[0] = MSG_ReadFloat ();
			cl.servertime = nq_mtime[0];
			CL_CheckForNQDSeekPointFound();
			if (demostarttime <= 0) 
				demostarttime = nq_mtime[0];
			if (cls.demotime < demostarttime) 
				cls.demotime = demostarttime;
			message_with_datagram = true;
			break;

		case nq_svc_clientdata:
			i = MSG_ReadShort ();
			NQD_ParseClientdata (i);
			break;

		case nq_svc_version:
			i = MSG_ReadLong ();
			if (i != NQ_PROTOCOL_VERSION)
				Host_Error ("CL_ParseServerMessage: Server is protocol %i instead of %i\n", i, NQ_PROTOCOL_VERSION);
			break;

		case svc_disconnect:
			Com_Printf ("\n======== End of demo ========\n\n");
//##			CL_NextDemo ();
			Host_EndGame ();
			Host_Abort ();
			break;

		case svc_print:
			NQD_ParsePrint ();
			break;
			
		case svc_centerprint:
			SCR_CenterPrint (MSG_ReadString ());
			break;

		case svc_stufftext:
			NQD_ParseStufftext ();
			break;

		case svc_damage:
			V_ParseDamage ();
			break;

		case svc_serverdata:
			NQD_ParseServerData ();
			break;

		case svc_setangle:
			for (i=0 ; i<3 ; i++)
				nq_last_fixangle[i] = cl.simangles[i] = cl.viewangles[i] = MSG_ReadAngle ();
			break;

		case nq_svc_setview:
			nq_viewentity = MSG_ReadShort ();
			if (nq_viewentity <= nq_maxclients)
				cl.playernum = nq_viewentity - 1;
			else	{
				// just let cl.playernum stay where it was
			}
			break;

		case svc_lightstyle:
			i = MSG_ReadByte ();
			if (i >= MAX_LIGHTSTYLES)
				Sys_Error ("svc_lightstyle > MAX_LIGHTSTYLES");
			strlcpy (cl_lightstyle[i].map,  MSG_ReadString(), sizeof(cl_lightstyle[0].map));
			cl_lightstyle[i].length = strlen(cl_lightstyle[i].map);
			break;

		case svc_sound:
			NQD_ParseStartSoundPacket();
			break;

		case svc_stopsound:
			i = MSG_ReadShort();
			S_StopSound(i>>3, i&7);
			break;

		case nq_svc_updatename:
			Sbar_Changed ();
			i = MSG_ReadByte ();
			if (i >= nq_maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatename > NQ_MAX_CLIENTS");
			strlcpy (cl.players[i].name, MSG_ReadString(), sizeof(cl.players[i].name));
			break;

		case svc_updatefrags:
			Sbar_Changed ();
			i = MSG_ReadByte ();
			if (i >= nq_maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatefrags > NQ_MAX_CLIENTS");
			cl.players[i].frags = MSG_ReadShort();
			break;

		case nq_svc_updatecolors:
			NQD_ParseUpdatecolors ();
			break;
			
		case nq_svc_particle:
			CL_ParseParticleEffect ();
			break;

		case svc_spawnbaseline:
			i = MSG_ReadShort ();
			if (i >= NQ_MAX_EDICTS)
				Host_Error ("svc_spawnbaseline: ent > MAX_EDICTS");
			NQD_BumpEntityCount (i);
			CL_ParseBaseline (&cl_entities[i].baseline);
			break;
		case svc_spawnstatic:
			CL_ParseStatic (false);
			break;
		case svc_temp_entity:
			CL_ParseTEnt ();
			break;

		case svc_setpause:
			if (MSG_ReadByte() != 0)
				cl.paused |= PAUSED_SERVER;
			else
				cl.paused &= ~PAUSED_SERVER;

			if (ISPAUSED)
				CDAudio_Pause ();
			else
				CDAudio_Resume ();
			break;

		case nq_svc_signonnum:
			i = MSG_ReadByte ();
			if (i <= nq_signon)
				Host_Error ("Received signon %i when at %i", i, nq_signon);
			nq_signon = i;
			break;

		case svc_killedmonster:
			cl.stats[STAT_MONSTERS]++;
			break;

		case svc_foundsecret:
			cl.stats[STAT_SECRETS]++;
			break;

		case svc_updatestat:
			i = MSG_ReadByte ();
			if (i < 0 || i >= MAX_CL_STATS)
				Sys_Error ("svc_updatestat: %i is invalid", i);
			cl.stats[i] = MSG_ReadLong ();;
			break;

		case svc_spawnstaticsound:
			CL_ParseStaticSound ();
			break;

		case svc_cdtrack:
			cl.cdtrack = MSG_ReadByte ();
			MSG_ReadByte();		// loop track (unused)
			if (nq_forcecdtrack != -1)
				CDAudio_Play ((byte)nq_forcecdtrack, true);
			else
				CDAudio_Play ((byte)cl.cdtrack, true);
			break;

		case svc_intermission:
			cl.intermission = 1;
			cl.completed_time = cl.time;
			cl.solo_completed_time = cl.servertime;
			VectorCopy (nq_last_fixangle, cl.simangles);
			break;

		case svc_finale:
			cl.intermission = 2;
			cl.completed_time = cl.time;
			cl.solo_completed_time = cl.servertime;
			SCR_CenterPrint (MSG_ReadString ());
			VectorCopy (nq_last_fixangle, cl.simangles);
			break;

		case nq_svc_cutscene:
			cl.intermission = 3;
			cl.completed_time = cl.time;
			cl.solo_completed_time = cl.servertime;
			SCR_CenterPrint (MSG_ReadString ());
			VectorCopy (nq_last_fixangle, cl.simangles);
			break;

		case svc_sellscreen:
			break;
		}
	}
}


void NQD_ReadPackets (void)
{
	extern qbool	host_skipframe;

	while (CL_GetNQDemoMessage()) {
		NQD_ParseServerMessage();
	}
	
	CL_SetSolidEntities();

	// If we're seeking, demotime is set to the target time: stop demotime from being advanced as normal
	if (cls.demoseeking)
		host_skipframe = true;
}


void NQD_StartPlayback (void)
{
	byte	c;
	qbool	neg = false;

	extern qbool pb_ensure(void);

	pb_ensure(); // FIXME: zzzz, is it possible to put "track parsing" somewhere in CL_GetNQDemoMessage() ????

	// parse forced cd track
	for (c = 0; c != '\n'; ) {
		CL_Demo_Read(&c, 1, false);

		if (c == '-')
			neg = true;
		else
			nq_forcecdtrack = nq_forcecdtrack * 10 + (c - '0');
	}
	if (neg)
		nq_forcecdtrack = -nq_forcecdtrack;

	cl.spectator = false;
	nq_signon = 0;
	nq_mtime[0] = 0;
	nq_maxclients = 0;
	cl_entframecount = 0;
}
