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
// sv_user.c -- server code for moving users

#ifndef CLIENTONLY
#include "qwsvdef.h"

static void SV_ClientDownloadComplete(client_t* cl);

edict_t	*sv_player;

usercmd_t	cmd;

cvar_t	sv_spectalk = {"sv_spectalk", "1"};
cvar_t	sv_sayteam_to_spec = {"sv_sayteam_to_spec", "1"};
cvar_t	sv_mapcheck = {"sv_mapcheck", "1"};
cvar_t	sv_minping = {"sv_minping", "0"};
cvar_t	sv_maxping = {"sv_maxping", "0"};
cvar_t	sv_enable_cmd_minping = {"sv_enable_cmd_minping", "1"};

cvar_t	sv_kickuserinfospamtime = {"sv_kickuserinfospamtime", "3"};
cvar_t	sv_kickuserinfospamcount = {"sv_kickuserinfospamcount", "300"};

cvar_t	sv_maxuploadsize = {"sv_maxuploadsize", "1048576"};

#ifdef FTE_PEXT_CHUNKEDDOWNLOADS
cvar_t  sv_downloadchunksperframe = {"sv_downloadchunksperframe", "15"};
#endif

#ifdef FTE_PEXT2_VOICECHAT
// Enable reception of voice packets.
cvar_t sv_voip = {"sv_voip", "1"};
// Record voicechat into mvds. Requires player support. 0=noone, 1=everyone, 2=spectators only.
cvar_t sv_voip_record = {"sv_voip_record", "0"};
// Echo voice packets back to their sender, a debug/test setting.
cvar_t sv_voip_echo = {"sv_voip_echo", "0"};
#endif

#ifdef MVD_PEXT1_SERVERSIDEWEAPON
static qbool SV_ClientExtensionWeaponSwitch(client_t* cl);
#endif

#ifdef MVD_PEXT1_DEBUG
typedef struct {
	int msec;
	byte model;
	vec3_t pos;
	qbool present;
} antilag_client_info_t;

static struct {
	antilag_client_info_t antilag_clients[MAX_CLIENTS];
} debug_info;

#ifdef MVD_PEXT1_SERVERSIDEWEAPON
static void SV_DebugServerSideWeaponScript(client_t* cl, int best_impulse);
static void SV_DebugServerSideWeaponInstruction(client_t* cl);
#endif
cvar_t sv_debug_weapons = { "sv_debug_weapons", "0" };
#endif

// These don't need any protocol extensions
cvar_t sv_debug_usercmd = { "sv_debug_usercmd", "0" };
cvar_t sv_debug_antilag = { "sv_debug_antilag", "0" };


#ifdef MVD_PEXT1_SERVERSIDEWEAPON
static void SV_UserSetWeaponRank(client_t* cl, const char* new_wrank);
#endif
static void SV_DebugClientCommand(byte playernum, const usercmd_t* cmd, int dropnum_);

extern	vec3_t	player_mins;

extern int	fp_messages, fp_persecond, fp_secondsdead;
extern char	fp_msg[];
extern cvar_t   pausable;
extern cvar_t   pm_bunnyspeedcap;
extern cvar_t   pm_ktjump;
extern cvar_t   pm_slidefix;
extern cvar_t   pm_airstep;
extern cvar_t   pm_pground;
extern cvar_t   pm_rampjump;
extern double	sv_frametime;

//bliP: init ->
extern cvar_t	sv_unfake; //bliP: 24/9 kickfake to unfake
extern cvar_t	sv_kicktop;
extern cvar_t	sv_speedcheck; //bliP: 24/9
//<-

static void OnChange_sv_maxpitch (cvar_t *var, char *str, qbool *cancel);
static void OnChange_sv_minpitch (cvar_t *var, char *str, qbool *cancel);
cvar_t	sv_maxpitch = {"sv_maxpitch", "80", 0, OnChange_sv_maxpitch};
cvar_t	sv_minpitch = {"sv_minpitch", "-70", 0, OnChange_sv_minpitch};

static void SetUpClientEdict (client_t *cl, edict_t *ent);

static qbool IsLocalIP(netadr_t a)
{
	return a.ip[0] == 10 || (a.ip[0] == 172 && (a.ip[1] & 0xF0) == 16)
	       || (a.ip[0] == 192 && a.ip[1] == 168) || a.ip[0] >= 224;
}
static qbool IsInetIP(netadr_t a)
{
	return a.ip[0] != 127 && !IsLocalIP(a);
}

//
// pitch clamping
//
// All this OnChange code is because we want the cvar names to have sv_ prefixes,
// but don't want them in serverinfo (save a couple of bytes of space)
// Value sanity checks are also done here
//
static void OnChange_sv_maxpitch (cvar_t *var, char *str, qbool *cancel)
{
	float	newval;
	char	*newstr;

	*cancel = true;

	newval = bound (0, Q_atof(str), 89.9f);
	if (newval == var->value)
		return;

	Cvar_SetValue (var, newval);
	newstr = (newval == 80.0f) ? (char *)"" : var->string;	// don't show default values in serverinfo
	SV_ServerinfoChanged("maxpitch", newstr);
}

static void OnChange_sv_minpitch (cvar_t *var, char *str, qbool *cancel)
{
	float	newval;
	char	*newstr;

	*cancel = true;

	newval = bound (-89.9f, Q_atof(str), 0.0f);
	if (newval == var->value)
		return;

	Cvar_SetValue (var, newval);
	newstr = (newval == -70.0f) ? (char *)"" : var->string;	// don't show default values in serverinfo
	SV_ServerinfoChanged("minpitch", newstr);
}

/*
============================================================

USER STRINGCMD EXECUTION

sv_client and sv_player will be valid.
============================================================
*/

/*
==================
PlayerCheckPing

Check that player's ping falls below sv_maxping value
==================
*/
qbool PlayerCheckPing(void)
{

	if (sv_client->maxping_met) return true;

	int maxping = Q_atof(sv_maxping.string);
	int playerping = sv_client->frames[sv_client->netchan.incoming_acknowledged & UPDATE_MASK].ping_time * 1000;

	if (maxping && playerping > maxping)
	{
		SV_ClientPrintf(sv_client, PRINT_HIGH, "\nYour ping is too high for this server!  Maximum ping is set to %i, your ping is %i.\nForcing spectator.\n\n", maxping, playerping);
		return false;
	}
	sv_client->maxping_met = true;
	return true;
}

/*
================
Cmd_New_f

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each server load.
================
*/
int SV_VIPbyIP (netadr_t adr);
static void Cmd_New_f (void)
{
	char		*gamedir;
	int			playernum;
	extern cvar_t sv_serverip;
	extern cvar_t sv_getrealip;

	if (sv_client->state == cs_spawned)
		return;

	if (!SV_ClientConnectedTime(sv_client) || sv_client->state == cs_connected) {
		SV_SetClientConnectionTime(sv_client);
	}

	sv_client->spawncount = svs.spawncount;

	// request protocol extensions.
	if (sv_client->process_pext)
	{
		MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, "cmd pext\n");
		return;
	}

	// do not proceed if realip is unknown
    if (sv_client->state == cs_preconnected && !sv_client->realip.ip[0] && (int)sv_getrealip.value)
	{
		char *server_ip = sv_serverip.string[0] ? sv_serverip.string : NET_AdrToString(net_local_sv_ipadr);

		if (!((IsLocalIP(net_local_sv_ipadr) && IsLocalIP(sv_client->netchan.remote_address))  ||
		        (IsInetIP (net_local_sv_ipadr) && IsInetIP (sv_client->netchan.remote_address))) &&
		        sv_client->netchan.remote_address.ip[0] != 127 && !sv_serverip.string[0])
		{
			Sys_Printf ("WARNING: Incorrect server ip address: %s\n"
			            "Set hostname in your operation system or set correctly sv_serverip cvar.\n",
			            server_ip);
			*(int *)&sv_client->realip = *(int *)&sv_client->netchan.remote_address;
			sv_client->state = cs_connected;
		}
		else
		{
			if (sv_client->realip_count++ < 10)
			{
				sv_client->state = cs_preconnected;
				MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
				MSG_WriteString (&sv_client->netchan.message,
								 va("packet %s \"ip %d %d\"\ncmd new\n", server_ip,
									sv_client - svs.clients, sv_client->realip_num));
			}
			if (SV_ClientConnectedTime(sv_client) > 3 || sv_client->realip_count > 10) {
				if ((int)sv_getrealip.value == 2) {
					Netchan_OutOfBandPrint (NS_SERVER, net_from, "%c\nFailed to validate client's IP.\n\n", A2C_PRINT);
					sv_client->rip_vip = 2;
				}
				sv_client->state = cs_connected;
			}
			else {
				return;
			}
		}
	}

	// rip_vip means that client can be connected if he has VIP for he's real ip
	// drop him if he hasn't
	if (sv_client->rip_vip == 1)
	{
		if ((sv_client->vip = SV_VIPbyIP(sv_client->realip)) == 0)
		{
			Sys_Printf ("%s:full connect\n", NET_AdrToString (net_from));
			Netchan_OutOfBandPrint (NS_SERVER, net_from,
			                        "%c\nserver is full\n\n", A2C_PRINT);
		}
		else
			sv_client->rip_vip = 0;
	}

	// we can be connected now, announce it, and possibly login
	if (!sv_client->rip_vip)
	{
		if (sv_client->state == cs_preconnected)
		{
			// get highest VIP level
			if (sv_client->vip < SV_VIPbyIP(sv_client->realip))
				sv_client->vip = SV_VIPbyIP(sv_client->realip);

			if (sv_client->vip && sv_client->spectator)
				Sys_Printf ("VIP spectator %s connected\n", sv_client->name);
			else if (sv_client->spectator)
				Sys_Printf ("Spectator %s connected\n", sv_client->name);
			else
				Sys_Printf ("Client %s connected\n", sv_client->name);

			Info_SetStar (&sv_client->_userinfo_ctx_, "*VIP", sv_client->vip ? va("%d", sv_client->vip) : "");

			// now we are connected
			sv_client->state = cs_connected;
		}

		if (!SV_Login(sv_client))
			return;

		// If logins are mandatory, check
		if (SV_LoginRequired(sv_client)) {
			return;
		}

		//bliP: cuff, mute ->
		sv_client->lockedtill = SV_RestorePenaltyFilter(sv_client, ft_mute);
		sv_client->cuff_time = SV_RestorePenaltyFilter(sv_client, ft_cuff);
		//<-
	}
	// send the info about the new client to all connected clients
	//	SV_FullClientUpdate (sv_client, &sv.reliable_datagram);
	//	sv_client->sendinfo = true;

	gamedir = Info_ValueForKey (svs.info, "*gamedir");
	if (!gamedir[0])
		gamedir = "qw";

#ifdef FTE_PEXT_FLOATCOORDS
	if (msg_coordsize > 2 && !(sv_client->fteprotocolextensions & FTE_PEXT_FLOATCOORDS))
	{
		SV_ClientPrintf(sv_client, 2, "\n\n\n\n"
			"Your client lacks the necessary extensions\n"
			"  to connect to this server.\n"
			"Set /cl_pext_floatcoords 1, or upgrade.\n"
			"Please upgrade to one of the following:\n"
			"> ezQuake 2.2 (https://ezquake.github.io)\n"
			"> fodquake 0.4 (http://fodquake.net)\n"
			"> FTEQW (http://fte.triptohell.info/)\n");
		if (!sv_client->spectator) {
			SV_DropClient (sv_client);
			return;
		}
		if (!SV_SkipCommsBotMessage(sv_client)) {
			return;
		}
	}
#endif

#ifdef FTE_PEXT_ENTITYDBL
	if (sv.max_edicts > 512 && !(sv_client->fteprotocolextensions & FTE_PEXT_ENTITYDBL)) {
		SV_ClientPrintf(sv_client, 2, "\n\nWARNING:\n"
			"Your client lacks support for extended\n"
			"  entity limits, some enemies/projectiles\n"
			"  may be invisible to you.\n"
			"Please upgrade to one of the following:\n"
			"> ezQuake 2.2 (https://ezquake.github.io)\n"
			"> fodquake 0.4 (http://fodquake.net)\n"
			"> FTEQW (http://fte.triptohell.info/)\n");
	}
#endif

	//NOTE:  This doesn't go through ClientReliableWrite since it's before the user
	//spawns.  These functions are written to not overflow
	if (sv_client->num_backbuf)
	{
		Con_Printf("WARNING %s: [SV_New] Back buffered (%d0), clearing\n",
		           sv_client->name, sv_client->netchan.message.cursize);
		sv_client->num_backbuf = 0;
		SZ_Clear(&sv_client->netchan.message);
	}

	// send the serverdata
	MSG_WriteByte  (&sv_client->netchan.message, svc_serverdata);
#ifdef PROTOCOL_VERSION_FTE
	if (sv_client->fteprotocolextensions) // let the client know
	{
		unsigned int ext = sv_client->fteprotocolextensions;

#ifdef FTE_PEXT_FLOATCOORDS
		if (msg_coordsize == 2) //we're not using float orgs on this level.
			ext &= ~FTE_PEXT_FLOATCOORDS;
#endif

		MSG_WriteLong (&sv_client->netchan.message, PROTOCOL_VERSION_FTE);
		MSG_WriteLong (&sv_client->netchan.message, ext);
	}
#endif
#ifdef PROTOCOL_VERSION_FTE2
	if (sv_client->fteprotocolextensions2) // let the client know
	{
		MSG_WriteLong (&sv_client->netchan.message, PROTOCOL_VERSION_FTE2);
		MSG_WriteLong (&sv_client->netchan.message, sv_client->fteprotocolextensions2);
	}
#endif // PROTOCOL_VERSION_FTE2
#ifdef PROTOCOL_VERSION_MVD1
	if (sv_client->mvdprotocolextensions1) {
		MSG_WriteLong (&sv_client->netchan.message, PROTOCOL_VERSION_MVD1);
		MSG_WriteLong (&sv_client->netchan.message, sv_client->mvdprotocolextensions1);
	}
#endif
	MSG_WriteLong  (&sv_client->netchan.message, PROTOCOL_VERSION);
	MSG_WriteLong  (&sv_client->netchan.message, svs.spawncount);
	MSG_WriteString(&sv_client->netchan.message, gamedir);

	playernum = NUM_FOR_EDICT(sv_client->edict)-1;
	if (sv_client->spectator)
		playernum |= 128;
	MSG_WriteByte (&sv_client->netchan.message, playernum);

	// send full levelname
	if (sv_client->rip_vip)
		MSG_WriteString (&sv_client->netchan.message, "");
	else
		MSG_WriteString (&sv_client->netchan.message, PR_GetEntityString(sv.edicts->v->message));

	// send the movevars
	MSG_WriteFloat(&sv_client->netchan.message, movevars.gravity);
	MSG_WriteFloat(&sv_client->netchan.message, movevars.stopspeed);
	MSG_WriteFloat(&sv_client->netchan.message, /* sv_client->maxspeed */ movevars.maxspeed); // FIXME: this does't work, Tonik?
	MSG_WriteFloat(&sv_client->netchan.message, movevars.spectatormaxspeed);
	MSG_WriteFloat(&sv_client->netchan.message, movevars.accelerate);
	MSG_WriteFloat(&sv_client->netchan.message, movevars.airaccelerate);
	MSG_WriteFloat(&sv_client->netchan.message, movevars.wateraccelerate);
	MSG_WriteFloat(&sv_client->netchan.message, movevars.friction);
	MSG_WriteFloat(&sv_client->netchan.message, movevars.waterfriction);
	MSG_WriteFloat(&sv_client->netchan.message, /* sv_client->entgravity */ movevars.entgravity); // FIXME: this does't work, Tonik?

	if (!sv_client->spectator) {
		if (!PlayerCheckPing()) {
			sv_client->old_frags = 0;
			SV_SetClientConnectionTime(sv_client);
			sv_client->spectator = true;
			sv_client->spec_track = 0;
			Info_SetStar(&sv_client->_userinfo_ctx_, "*spectator", "1");
			Info_SetStar(&sv_client->_userinfoshort_ctx_, "*spectator", "1");
		}
	}

	if (sv_client->rip_vip)
	{
		SV_LogPlayer(sv_client, va("dropped %d", sv_client->rip_vip), 1);
		SV_DropClient (sv_client);
		return;
	}

	// send music
	MSG_WriteByte (&sv_client->netchan.message, svc_cdtrack);
	MSG_WriteByte (&sv_client->netchan.message, sv.edicts->v->sounds);

	// send server info string
	MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
	MSG_WriteString (&sv_client->netchan.message, va("fullserverinfo \"%s\"\n", svs.info) );

	//bliP: player logging
	SV_LogPlayer(sv_client, "connect", 1);
}

/*
==================
Cmd_Soundlist_f
==================
*/
static void Cmd_Soundlist_f (void)
{
	char		**s;
	unsigned	n;

	if (sv_client->state != cs_connected)
	{
		Con_Printf ("soundlist not valid -- already spawned\n");
		return;
	}

	// handle the case of a level changing while a client was connecting
	if (Q_atoi(Cmd_Argv(1)) != svs.spawncount)
	{
		SV_ClearReliable (sv_client);
		Con_Printf ("SV_Soundlist_f from different level\n");
		Cmd_New_f ();
		return;
	}

	n = Q_atoi(Cmd_Argv(2));
	if (n >= MAX_SOUNDS)
	{
		SV_ClearReliable (sv_client);
		SV_ClientPrintf (sv_client, PRINT_HIGH,
		                 "SV_Soundlist_f: Invalid soundlist index\n");
		SV_DropClient (sv_client);
		return;
	}

	//NOTE:  This doesn't go through ClientReliableWrite since it's before the user
	//spawns.  These functions are written to not overflow
	if (sv_client->num_backbuf)
	{
		Con_Printf("WARNING %s: [SV_Soundlist] Back buffered (%d0), clearing\n", sv_client->name, sv_client->netchan.message.cursize);
		sv_client->num_backbuf = 0;
		SZ_Clear(&sv_client->netchan.message);
	}

	MSG_WriteByte (&sv_client->netchan.message, svc_soundlist);
	MSG_WriteByte (&sv_client->netchan.message, n);
	for (s = sv.sound_precache+1 + n ;
	        *s && sv_client->netchan.message.cursize < (MAX_MSGLEN/2);
	        s++, n++)
		MSG_WriteString (&sv_client->netchan.message, *s);

	MSG_WriteByte (&sv_client->netchan.message, 0);

	// next msg
	if (*s)
		MSG_WriteByte (&sv_client->netchan.message, n);
	else
		MSG_WriteByte (&sv_client->netchan.message, 0);
}

static char *TrimModelName (const char *full)
{
	static char shortn[MAX_QPATH];
	int len;

	if (!strncmp(full, "progs/", 6) && !strchr(full + 6, '/'))
		strlcpy (shortn, full + 6, sizeof(shortn));		// strip progs/
	else
		strlcpy (shortn, full, sizeof(shortn));

	len = strlen(shortn);
	if (len > 4 && !strcmp(shortn + len - 4, ".mdl")
		&& strchr(shortn, '.') == shortn + len - 4)
	{	// strip .mdl
		shortn[len - 4] = '\0';
	}

	return shortn;
}

/*
==================
Cmd_Modellist_f
==================
*/
static void Cmd_Modellist_f (void)
{
	char		**s;
	unsigned	n;
	int         i;
	unsigned    maxclientsupportedmodels;

	if (sv_client->state != cs_connected) {
		Con_Printf ("modellist not valid -- already spawned\n");
		return;
	}

	// handle the case of a level changing while a client was connecting
	if (Q_atoi(Cmd_Argv(1)) != svs.spawncount) {
		SV_ClearReliable (sv_client);
		Con_Printf ("SV_Modellist_f from different level\n");
		Cmd_New_f ();
		return;
	}

	n = Q_atoi(Cmd_Argv(2));
	if (n >= MAX_MODELS) {
		SV_ClearReliable (sv_client);
		SV_ClientPrintf (sv_client, PRINT_HIGH, "SV_Modellist_f: Invalid modellist index\n");
		SV_DropClient (sv_client);
		return;
	}

	if (n == 0 && (sv_client->extensions & Z_EXT_VWEP) && sv.vw_model_name[0]) {
		int i;
		char ss[1024] = "//vwep ";
		// send VWep precaches
		for (i = 0, s = sv.vw_model_name; i < MAX_VWEP_MODELS; s++, i++) {
			if (!*s || !**s) {
				break;
			}
			if (i > 0) {
				strlcat(ss, " ", sizeof(ss));
			}
			strlcat(ss, TrimModelName(*s), sizeof(ss));
		}
		strlcat(ss, "\n", sizeof(ss));
		if (ss[strlen(ss) - 1] == '\n') {       // didn't overflow?
			ClientReliableWrite_Begin (sv_client, svc_stufftext, 2 + strlen(ss));
			ClientReliableWrite_String (sv_client, ss);
		}
	}

	//NOTE:  This doesn't go through ClientReliableWrite since it's before the user
	//spawns.  These functions are written to not overflow
	if (sv_client->num_backbuf) {
		Con_Printf("WARNING %s: [SV_Modellist] Back buffered (%d0), clearing\n", sv_client->name, sv_client->netchan.message.cursize);
		sv_client->num_backbuf = 1;

		SZ_Clear(&sv_client->netchan.message);
	}

#ifdef FTE_PEXT_MODELDBL
	if (n > 255) {
		MSG_WriteByte(&sv_client->netchan.message, svc_fte_modellistshort);
		MSG_WriteShort(&sv_client->netchan.message, n);
	}
	else
#endif
	{
		MSG_WriteByte(&sv_client->netchan.message, svc_modellist);
		MSG_WriteByte(&sv_client->netchan.message, n);
	}

	maxclientsupportedmodels = 256;
#ifdef FTE_PEXT_MODELDBL
	if (sv_client->fteprotocolextensions & FTE_PEXT_MODELDBL) {
		maxclientsupportedmodels *= 2;
	}
#endif

	s = sv.model_precache + 1 + n;
	for (i = 1 + n; i < maxclientsupportedmodels && *s && (((i-1)&255) == 0 || sv_client->netchan.message.cursize < (MAX_MSGLEN/2)); i++, s++) {
		MSG_WriteString (&sv_client->netchan.message, *s);
	}
	n = i - 1;

	if (!s[0] || n == maxclientsupportedmodels - 1) {
		n = 0;
	}

	// next msg (nul terminator then next request indicator)
	MSG_WriteByte (&sv_client->netchan.message, 0);
	MSG_WriteByte (&sv_client->netchan.message, n);
}

/*
==================
Cmd_PreSpawn_f
==================
*/
static void Cmd_PreSpawn_f (void)
{
	unsigned int buf;
	unsigned int check;
	int i, j;

	if (sv_client->state != cs_connected)
	{
		Con_Printf ("prespawn not valid -- already spawned\n");
		return;
	}

	// handle the case of a level changing while a client was connecting
	if (Q_atoi(Cmd_Argv(1)) != svs.spawncount)
	{
		SV_ClearReliable (sv_client);
		Con_Printf ("SV_PreSpawn_f from different level\n");
		Cmd_New_f ();
		return;
	}

	buf = Q_atoi(Cmd_Argv(2));
	if (buf >= sv.num_signon_buffers + sv.static_entity_count + sv.num_baseline_edicts)
		buf = 0;

	if (!buf)
	{
		// should be three numbers following containing checksums
		check = Q_atoi(Cmd_Argv(3));

		//		Con_DPrintf("Client check = %d\n", check);

		if ((int)sv_mapcheck.value && check != sv.map_checksum &&
			check != sv.map_checksum2)
		{
			SV_ClientPrintf (sv_client, PRINT_HIGH,
			                 "Map model file does not match (%s), %i != %i/%i.\n"
			                 "You may need a new version of the map, or the proper install files.\n",
					 sv.modelname, check, sv.map_checksum, sv.map_checksum2);
			SV_DropClient (sv_client);
			return;
		}
		sv_client->checksum = check;
	}

	if (SV_SkipCommsBotMessage(sv_client)) {
		// skip pre-spawning
		MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, va("cmd spawn %i 0\n", svs.spawncount) );
		return;
	}

	//NOTE:  This doesn't go through ClientReliableWrite since it's before the user
	//spawns.  These functions are written to not overflow
	if (sv_client->num_backbuf)
	{
		Con_Printf("WARNING %s: [SV_PreSpawn] Back buffered (%d0), clearing\n", sv_client->name, sv_client->netchan.message.cursize);
		sv_client->num_backbuf = 0;
		SZ_Clear(&sv_client->netchan.message);
	}

	if (buf < sv.static_entity_count) {
		entity_state_t from = { 0 };

		while (buf < sv.static_entity_count) {
			entity_state_t* s = &sv.static_entities[buf];

			if (sv_client->netchan.message.cursize >= (sv_client->netchan.message.maxsize / 2)) {
				break;
			}

			if (sv_client->fteprotocolextensions & FTE_PEXT_SPAWNSTATIC2) {
				MSG_WriteByte(&sv_client->netchan.message, svc_fte_spawnstatic2);
				SV_WriteDelta(sv_client, &from, s, &sv_client->netchan.message, true);
			}
			else if (s->modelindex < 256) {
				MSG_WriteByte(&sv_client->netchan.message, svc_spawnstatic);
				MSG_WriteByte(&sv_client->netchan.message, s->modelindex);
				MSG_WriteByte(&sv_client->netchan.message, s->frame);
				MSG_WriteByte(&sv_client->netchan.message, s->colormap);
				MSG_WriteByte(&sv_client->netchan.message, s->skinnum);
				for (i = 0; i < 3; ++i) {
					MSG_WriteCoord(&sv_client->netchan.message, s->origin[i]);
					MSG_WriteAngle(&sv_client->netchan.message, s->angles[i]);
				}
			}

			++buf;
		}
	}
	else if (buf < sv.static_entity_count + sv.num_baseline_edicts) {
		static entity_state_t empty_baseline = { 0 };

		for (i = buf - sv.static_entity_count; i < sv.num_baseline_edicts; ++i) {
			edict_t* svent = EDICT_NUM(i);
			entity_state_t* s = &svent->e.baseline;

			if (sv_client->netchan.message.cursize >= (sv_client->netchan.message.maxsize / 2)) {
				break;
			}

			if (!s->number || !s->modelindex || !memcmp(s, &empty_baseline, sizeof(empty_baseline))) {
				++buf;
				continue;
			}

			if (sv_client->fteprotocolextensions & FTE_PEXT_SPAWNSTATIC2) {
				MSG_WriteByte(&sv_client->netchan.message, svc_fte_spawnbaseline2);
				SV_WriteDelta(sv_client, &empty_baseline, s, &sv_client->netchan.message, true);
			}
			else if (s->modelindex < 256) {
				MSG_WriteByte(&sv_client->netchan.message, svc_spawnbaseline);
				MSG_WriteShort(&sv_client->netchan.message, i);
				MSG_WriteByte(&sv_client->netchan.message, svent->e.baseline.modelindex);
				MSG_WriteByte(&sv_client->netchan.message, svent->e.baseline.frame);
				MSG_WriteByte(&sv_client->netchan.message, svent->e.baseline.colormap);
				MSG_WriteByte(&sv_client->netchan.message, svent->e.baseline.skinnum);
				for (j = 0; j < 3; j++) {
					MSG_WriteCoord(&sv_client->netchan.message, svent->e.baseline.origin[j]);
					MSG_WriteAngle(&sv_client->netchan.message, svent->e.baseline.angles[j]);
				}
			}
			++buf;
		}
	}
	else {
		SZ_Write(
			&sv_client->netchan.message,
			sv.signon_buffers[buf - sv.static_entity_count - sv.num_baseline_edicts],
			sv.signon_buffer_size[buf - sv.static_entity_count - sv.num_baseline_edicts]
		);
		++buf;
	}

	if (buf == sv.num_signon_buffers + sv.static_entity_count + sv.num_baseline_edicts) {
		// all done prespawning
		MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, va("cmd spawn %i 0\n", svs.spawncount) );
	}
	else {
		// need to prespawn more
		MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message,
		                 va("cmd prespawn %i %i\n", svs.spawncount, buf) );
	}
}

/*
==================
Cmd_Spawn_f
==================
*/
static void Cmd_Spawn_f (void)
{
	int         i;
	client_t    *client;
	unsigned    n;

	if (sv_client->state != cs_connected)
	{
		Con_Printf ("Spawn not valid -- already spawned\n");
		return;
	}

	// handle the case of a level changing while a client was connecting
	if (Q_atoi(Cmd_Argv(1)) != svs.spawncount)
	{
		SV_ClearReliable (sv_client);
		Con_Printf ("SV_Spawn_f from different level\n");
		Cmd_New_f ();
		return;
	}

	n = Q_atoi(Cmd_Argv(2));
	if (n >= MAX_CLIENTS)
	{
		SV_ClientPrintf (sv_client, PRINT_HIGH,
		                 "SV_Spawn_f: Invalid client start\n");
		SV_DropClient (sv_client);
		return;
	}

	// send all current names, colors, and frag counts
	// FIXME: is this a good thing?
	SZ_Clear (&sv_client->netchan.message);

	// send current status of all other players

	// normally this could overflow, but no need to check due to backbuf
	for (i=n, client = svs.clients + n ; i<MAX_CLIENTS && sv_client->netchan.message.cursize < (MAX_MSGLEN/2); i++, client++)
		SV_FullClientUpdateToClient (client, sv_client);

	if (i < MAX_CLIENTS)
	{
		MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message,
		                 va("cmd spawn %i %d\n", svs.spawncount, i) );
		return;
	}

	// send all current light styles
	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		ClientReliableWrite_Begin (sv_client, svc_lightstyle,
		                           3 + (sv.lightstyles[i] ? strlen(sv.lightstyles[i]) : 1));
		ClientReliableWrite_Byte (sv_client, (char)i);
		ClientReliableWrite_String (sv_client, sv.lightstyles[i]);
	}

	if (sv.loadgame)
	{
		// loaded games are already fully initialized 
		// if this is the last client to be connected, unpause

		// gravity and maxspeed are optional, set it if save file omited it.
		edict_t	*ent = sv_client->edict;
		sv_client->entgravity = fofs_gravity ? EdictFieldFloat(ent, fofs_gravity) : 1.0;
		sv_client->maxspeed = fofs_maxspeed? EdictFieldFloat(ent, fofs_maxspeed) : (int)sv_maxspeed.value;

		if (sv.paused & 1)
			SV_TogglePause (NULL, 1);
	}
	else
	{
		SetUpClientEdict(sv_client, sv_client->edict);
	}

	//
	// force stats to be updated
	//
	memset (sv_client->stats, 0, sizeof(sv_client->stats));

	ClientReliableWrite_Begin (sv_client, svc_updatestatlong, 6);
	ClientReliableWrite_Byte (sv_client, STAT_TOTALSECRETS);
	ClientReliableWrite_Long (sv_client, PR_GLOBAL(total_secrets));

	ClientReliableWrite_Begin (sv_client, svc_updatestatlong, 6);
	ClientReliableWrite_Byte (sv_client, STAT_TOTALMONSTERS);
	ClientReliableWrite_Long (sv_client, PR_GLOBAL(total_monsters));

	ClientReliableWrite_Begin (sv_client, svc_updatestatlong, 6);
	ClientReliableWrite_Byte (sv_client, STAT_SECRETS);
	ClientReliableWrite_Long (sv_client, PR_GLOBAL(found_secrets));

	ClientReliableWrite_Begin (sv_client, svc_updatestatlong, 6);
	ClientReliableWrite_Byte (sv_client, STAT_MONSTERS);
	ClientReliableWrite_Long (sv_client, PR_GLOBAL(killed_monsters));

	// get the client to check and download skins
	// when that is completed, a begin command will be issued
	ClientReliableWrite_Begin (sv_client, svc_stufftext, 8);
	ClientReliableWrite_String (sv_client, "skins\n" );
}

/*
==================
SV_SpawnSpectator
==================
*/
static void SV_SpawnSpectator (void)
{
	int i;
	edict_t *e;

	VectorClear (sv_player->v->origin);
	VectorClear (sv_player->v->view_ofs);
	sv_player->v->view_ofs[2] = 22;
	sv_player->v->fixangle = true;
	sv_player->v->movetype = MOVETYPE_NOCLIP; // progs can change this to MOVETYPE_FLY, for example

	// search for an info_playerstart to spawn the spectator at
	for (i=MAX_CLIENTS-1 ; i<sv.num_edicts ; i++)
	{
		e = EDICT_NUM(i);
		if (!strcmp(PR_GetEntityString(e->v->classname), "info_player_start"))
		{
			VectorCopy (e->v->origin, sv_player->v->origin);
			VectorCopy (e->v->angles, sv_player->v->angles);
			return;
		}
	}
}

/*
==================
Cmd_Begin_f
==================
*/
static void Cmd_Begin_f (void)
{
	unsigned pmodel = 0, emodel = 0;
	int i;

	if (sv_client->state == cs_spawned)
		return; // don't begin again

	// handle the case of a level changing while a client was connecting
	if (Q_atoi(Cmd_Argv(1)) != svs.spawncount)
	{
		SV_ClearReliable (sv_client);
		Con_Printf ("SV_Begin_f from different level\n");
		Cmd_New_f ();
		return;
	}

	sv_client->state = cs_spawned;

	if (!sv.loadgame)
	{
		if (sv_client->spectator)
			SV_SpawnSpectator ();

		// copy spawn parms out of the client_t
		for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
			(&PR_GLOBAL(parm1))[i] = sv_client->spawn_parms[i];

		// call the spawn function
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(sv_player);
		G_FLOAT(OFS_PARM0) = (float) sv_client->vip;
		PR_GameClientConnect(sv_client->spectator);

		// actually spawn the player
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(sv_player);
		PR_GamePutClientInServer(sv_client->spectator);
	}

	// clear the net statistics, because connecting gives a bogus picture
	sv_client->netchan.frame_latency = 0;
	sv_client->netchan.frame_rate = 0;
	sv_client->netchan.drop_count = 0;
	sv_client->netchan.good_count = 0;

	//check he's not cheating
	if (!sv_client->spectator)
	{
		if (!*Info_Get (&sv_client->_userinfo_ctx_, "pmodel") ||
		        !*Info_Get (&sv_client->_userinfo_ctx_, "emodel")) //bliP: typo? 2nd pmodel to emodel
			SV_BroadcastPrintf (PRINT_HIGH, "%s WARNING: missing player/eyes model checksum\n", sv_client->name);
		else
		{
			pmodel = Q_atoi(Info_Get (&sv_client->_userinfo_ctx_, "pmodel"));
			emodel = Q_atoi(Info_Get (&sv_client->_userinfo_ctx_, "emodel"));

			if (!(pmodel == sv.model_newplayer_checksum || pmodel == sv.model_player_checksum) || emodel != sv.eyes_player_checksum)
				SV_BroadcastPrintf (PRINT_HIGH, "%s WARNING: non standard player/eyes model detected\n", sv_client->name);
		}
	}

	// if we are paused, tell the client
	if (sv.paused)
	{
		ClientReliableWrite_Begin (sv_client, svc_setpause, 2);
		ClientReliableWrite_Byte (sv_client, sv.paused);
		SV_ClientPrintf(sv_client, PRINT_HIGH, "Server is paused.\n");
	}

	if (sv.loadgame)
	{
		// send a fixangle over the reliable channel to make sure it gets there
		// Never send a roll angle, because savegames can catch the server
		// in a state where it is expecting the client to correct the angle
		// and it won't happen if the game was just loaded, so you wind up
		// with a permanent head tilt
		edict_t *ent;

		ent = EDICT_NUM( 1 + (sv_client - svs.clients) );
		MSG_WriteByte (&sv_client->netchan.message, svc_setangle);
		for (i = 0; i < 2; i++)
			MSG_WriteAngle (&sv_client->netchan.message, ent->v->v_angle[i]);
		MSG_WriteAngle (&sv_client->netchan.message, 0);
	}

	sv_client->lastservertimeupdate = -99; // update immediately
}

//=============================================================================

/*
==================
SV_DownloadNextFile
==================
*/
static qbool SV_DownloadNextFile (void)
{
	int		num;
	char		*name, n[MAX_OSPATH];
	unsigned char	all_demos_downloaded[]	= "All demos downloaded.\n";
	unsigned char	incorrect_demo_number[]	= "Incorrect demo number.\n";

	switch (sv_client->demonum[0])
	{
	case 1:
		if (sv_client->demolist)
		{
			Con_Printf((char *)Q_redtext(all_demos_downloaded));
			sv_client->demolist = false;
		}
		sv_client->demonum[0] = 0;
	case 0:
		return false;
	default:;
	}

	num = sv_client->demonum[--(sv_client->demonum[0])];
	if (num == 0)
	{
		Con_Printf((char *)Q_redtext(incorrect_demo_number));
		return SV_DownloadNextFile();
	}
	if (!(name = SV_MVDNum(num)))
	{
		Con_Printf((char *)Q_yelltext((unsigned char*)va("Demo number %d not found.\n",
			(num & 0xFF000000) ? -(num >> 24) : 
				((num & 0x00800000) ? (num | 0xFF000000) : num) )));
		return SV_DownloadNextFile();
	}
	//Con_Printf("downloading demos/%s\n",name);
	snprintf(n, sizeof(n), "download demos/%s\n", name);

	ClientReliableWrite_Begin (sv_client, svc_stufftext, strlen(n) + 2);
	ClientReliableWrite_String (sv_client, n);

	return true;
}

/*
==================
SV_CompleteDownoload
==================

This is a sub routine for  SV_NextDownload(), called when download complete, we set up some fields for sv_client.

*/

void SV_CompleteDownoload(void)
{
	unsigned char download_completed[] = "Download completed.\n";

	if (!sv_client->download)
		return;

	SV_ClientDownloadComplete(sv_client);

	Con_Printf((char *)Q_redtext(download_completed));

	if (SV_DownloadNextFile())
		return;

	// if map changed tell the client to reconnect
	if (sv_client->spawncount != svs.spawncount)
	{
		char *str = "changing\n"
		            "reconnect\n";

		ClientReliableWrite_Begin (sv_client, svc_stufftext, strlen(str)+2);
		ClientReliableWrite_String (sv_client, str);
	}
}

/*
==================
Cmd_NextDownload_f
==================
*/

#ifdef FTE_PEXT_CHUNKEDDOWNLOADS

// qqshka: percent is optional, u can't relay on it

void SV_NextChunkedDownload(int chunknum, int percent, int chunked_download_number)
{
#define CHUNKSIZE 1024
	char buffer[CHUNKSIZE];
	int i;
	int maxchunks = bound(1, (int)sv_downloadchunksperframe.value, 30);

	sv_client->file_percent = bound(0, percent, 100); //bliP: file percent

	if (chunknum < 0)
	{  // qqshka: FTE's chunked download does't have any way of signaling what client complete dl-ing, so doing it this way.
		SV_CompleteDownoload();
		return;
	}

	// Check if too much requests or client sent something wrong
	if (sv_client->download_chunks_perframe >= maxchunks || chunked_download_number < 1)
		return;

	if (!sv_client->download_chunks_perframe) // ignore "rate" if not first packet per frame
		if (sv_client->datagram.cursize + CHUNKSIZE+5+50 > sv_client->datagram.maxsize)
			return;	//choked!

	if (VFS_SEEK(sv_client->download, chunknum*CHUNKSIZE, SEEK_SET))
		return; // FIXME: ERROR of some kind

	i = VFS_READ(sv_client->download, buffer, CHUNKSIZE, NULL);

	if (i > 0)
	{
		byte data[1+ (sizeof("\\chunk")-1) + 4 + 1 + 4 + CHUNKSIZE]; // byte + (sizeof("\\chunk")-1) + long + byte + long + CHUNKSIZE
		sizebuf_t *msg, msg_oob;

		if (sv_client->download_chunks_perframe)
		{
			msg = &msg_oob;

			SZ_Init (&msg_oob, data, sizeof(data));

			MSG_WriteByte(msg, A2C_PRINT);
			SZ_Write(msg, "\\chunk", sizeof("\\chunk")-1);
			MSG_WriteLong(msg, chunked_download_number); // return back, so they sure what it proper chunk
		}
		else
			msg = &sv_client->datagram;

		if (i != CHUNKSIZE)
			memset(buffer+i, 0, CHUNKSIZE-i);

		MSG_WriteByte(msg, svc_download);
		MSG_WriteLong(msg, chunknum);
		SZ_Write(msg, buffer, CHUNKSIZE);

		if (sv_client->download_chunks_perframe)
			Netchan_OutOfBand (NS_SERVER, sv_client->netchan.remote_address, msg->cursize, msg->data);
	}
	else {
		; // FIXME: EOF/READ ERROR
	}

	sv_client->download_chunks_perframe++;
}

#endif

static void Cmd_NextDownload_f (void)
{
	byte    buffer[FILE_TRANSFER_BUF_SIZE];
	int     r, tmp;
	int     percent;
	int     size;
	double  frametime;

	if (!sv_client->download)
		return;

#ifdef FTE_PEXT_CHUNKEDDOWNLOADS
	if (sv_client->fteprotocolextensions & FTE_PEXT_CHUNKEDDOWNLOADS)
	{
		SV_NextChunkedDownload(atoi(Cmd_Argv(1)), atoi(Cmd_Argv(2)), atoi(Cmd_Argv(3)));
		return;
	}
#endif

	tmp = sv_client->downloadsize - sv_client->downloadcount;

	frametime = max(0.05, min(0, sv_client->netchan.frame_rate));
	//Sys_Printf("rate:%f\n", sv_client->netchan.frame_rate);

	r = (int)((curtime + frametime - sv_client->netchan.cleartime)/sv_client->netchan.rate);
	if (r <= 10)
		r = 10;
	if (r > FILE_TRANSFER_BUF_SIZE)
		r = FILE_TRANSFER_BUF_SIZE;

	// don't send too much if already buffering
	if (sv_client->num_backbuf)
		r = 10;

	if (r > tmp)
		r = tmp;

	Con_DPrintf("Downloading: %d", r);
	r = VFS_READ(sv_client->download, buffer, r, NULL);
	Con_DPrintf(" => %d, total: %d => %d", r, sv_client->downloadsize, sv_client->downloadcount);
	ClientReliableWrite_Begin (sv_client, svc_download, 6 + r);
	ClientReliableWrite_Short (sv_client, r);
	sv_client->downloadcount += r;
	if (!(size = sv_client->downloadsize))
		size = 1;
	percent = (sv_client->downloadcount * (double)100.) / size;
	if (percent == 100 && sv_client->downloadcount != sv_client->downloadsize)
		percent = 99;
	else if (percent != 100 && sv_client->downloadcount == sv_client->downloadsize)
		percent = 100;
	Con_DPrintf("; %d\n", percent);
	ClientReliableWrite_Byte (sv_client, percent);
	ClientReliableWrite_SZ (sv_client, buffer, r);
	sv_client->file_percent = percent; //bliP: file percent

	if (sv_client->downloadcount == sv_client->downloadsize)
		SV_CompleteDownoload();
}

static void OutofBandPrintf(netadr_t where, char *fmt, ...)
{
	va_list	 argptr;
	char send1[1024];

	send1[0] = 0xff;
	send1[1] = 0xff;
	send1[2] = 0xff;
	send1[3] = 0xff;
	send1[4] = A2C_PRINT;
	va_start (argptr, fmt);
	vsnprintf (send1 + 5, sizeof(send1) - 5, fmt, argptr);
	va_end (argptr);

	NET_SendPacket (NS_SERVER, strlen(send1) + 1, send1, where);
}

/*
==================
SV_NextUpload
==================
*/
void SV_ReplaceChar(char *s, char from, char to);
void SV_CancelUpload(void)
{
	SV_ClientPrintf(sv_client, PRINT_HIGH, "Upload denied\n");
	ClientReliableWrite_Begin (sv_client, svc_stufftext, 8);
	ClientReliableWrite_String (sv_client, "stopul");
	if (sv_client->upload)
	{
		fclose (sv_client->upload);
		sv_client->upload = NULL;
		sv_client->file_percent = 0; //bliP: file percent
	}
}
static void SV_NextUpload (void)
{
	int	percent;
	int	size;
	char	*name = sv_client->uploadfn;
	//	Sys_Printf("-- %s\n", name);

	SV_ReplaceChar(name, '\\', '/');
	if (!*name || !strncmp(name, "../", 3) || strstr(name, "/../") || *name == '/'
#ifdef _WIN32
	        || (isalpha(*name) && name[1] == ':')
#endif //_WIN32
	   )
	{ //bliP: can't upload back a directory
		SV_CancelUpload();
		// suck out rest of packet
		size = MSG_ReadShort ();
		MSG_ReadByte ();
		if (size > 0)
			msg_readcount += size;
		return;
	}

	size = MSG_ReadShort ();
	sv_client->file_percent = percent = MSG_ReadByte ();

	if (size <= 0 || size >= MAX_DATAGRAM || percent < 0 || percent > 100)
	{
		SV_CancelUpload();
		return;
	}

	if (sv_client->upload)
	{
		int pos = ftell(sv_client->upload);
		if (pos == -1 || (sv_client->remote_snap && (pos + size) > (int)sv_maxuploadsize.value))
		{
			msg_readcount += size;
			SV_CancelUpload();
			return;
		}
	}
	else
	{
		sv_client->upload = fopen(name, "wb");
		if (!sv_client->upload)
		{
			Sys_Printf("Can't create %s\n", name);
			ClientReliableWrite_Begin (sv_client, svc_stufftext, 8);
			ClientReliableWrite_String (sv_client, "stopul");
			*name = 0;
			return;
		}
		Sys_Printf("Receiving %s from %d...\n", name, sv_client->userid);
		if (sv_client->remote_snap)
			OutofBandPrintf(sv_client->snap_from, "Server receiving %s from %d...\n",
			                name, sv_client->userid);

		// force cache rebuild.
		FS_FlushFSHash();
	}

	Sys_Printf("-");
	fwrite (net_message.data + msg_readcount, 1, size, sv_client->upload);
	msg_readcount += size;

	Con_DPrintf ("UPLOAD: %d received\n", size);

	if (percent != 100)
	{
		ClientReliableWrite_Begin (sv_client, svc_stufftext, 8);
		ClientReliableWrite_String (sv_client, "nextul\n");
	}
	else
	{
		fclose (sv_client->upload);
		sv_client->upload = NULL;
		sv_client->file_percent = 0; //bliP: file percent

		Sys_Printf("\n%s upload completed.\n", name);

		if (sv_client->remote_snap)
		{
			char *p;

			if ((p = strchr(name, '/')) != NULL)
				p++;
			else
				p = name;
			OutofBandPrintf(sv_client->snap_from,
			                "%s upload completed.\nTo download, enter:\ndownload %s\n",
			                name, p);
		}
	}
}

static void SV_UserCleanFilename(char* name)
{
	char* p;

	// lowercase name (needed for casesen file systems)
	for (p = name; *p; p++) {
		*p = (char)tolower(*p);
	}
}

/*
==================
Cmd_Download_f
==================
*/
//void SV_ReplaceChar(char *s, char from, char to);
static void Cmd_Download_f(void)
{
	char	*name, n[MAX_OSPATH], *val;
	char alternative_path[MAX_OSPATH];
	extern	cvar_t	allow_download;
	extern	cvar_t	allow_download_skins;
	extern	cvar_t	allow_download_models;
	extern	cvar_t	allow_download_sounds;
	extern	cvar_t	allow_download_maps;
	extern  cvar_t	allow_download_pakmaps;
	extern	cvar_t	allow_download_demos;
	extern	cvar_t	allow_download_other;
	extern  cvar_t  download_map_url; //bliP: download url
	extern	cvar_t	sv_demoDir;
	int i;
	qbool allow_dl = false;

	if (Cmd_Argc() != 2)
	{
		Con_Printf("download [filename]\n");
		return;
	}

	name = Cmd_Argv(1);

	SV_ReplaceChar(name, '\\', '/');

	// couple of checks to not allow dl-ing anything except in quake dir
	if (
		//TODO: split name to pathname and filename and check for 'bad symbols' only in pathname
		*name == '/' // no absolute
		|| !strncmp(name, "../", 3) // no leading ../
		|| strstr(name, "/../") // no /../
		|| ((i = strlen(name)) < 3 ? 0 : !strncmp(name + i - 3, "/..", 4)) // no /.. at end
		|| *name == '.' //relative is pointless
		|| ((i = strlen(name)) < 4 ? 0 : !strncasecmp(name + i - 4, ".log", 5)) // no logs
#ifdef _WIN32
		// no leading X:
	   	|| ( name[0] && name[1] == ':' && ((*name >= 'a' && *name <= 'z') || (*name >= 'A' && *name <= 'Z')))
#endif //_WIN32
	   )
		goto deny_download;

	if (sv_client->special)
		allow_dl = true; // NOTE: user used techlogin, allow dl anything in quake dir in such case!
	else if (!strstr(name, "/"))
		allow_dl = false; // should be in subdir
	else if (!(int)allow_download.value)
		allow_dl = false; // global allow check
	else if (!strncmp(name, "skins/", 6))
		allow_dl = allow_download_skins.value; // skins
	else if (!strncmp(name, "progs/", 6))
		allow_dl = allow_download_models.value; // models
	else if (!strncmp(name, "sound/", 6))
		allow_dl = allow_download_sounds.value; // sounds
	else if (!strncmp(name, "maps/", 5)) // maps, note usage of allow_download_pakmaps a bit below
		allow_dl = allow_download_maps.value; // maps
	else if (!strncmp(name, "demos/", 6) || !strncmp(name, "demonum/", 8))
		allow_dl = allow_download_demos.value; // demos
	else
		allow_dl = allow_download_other.value; // all other stuff

	if (!allow_dl)
		goto deny_download;

	SV_ClientDownloadComplete(sv_client);

	memset(alternative_path, 0, sizeof(alternative_path));
	if ( !strncmp(name, "demos/", 6) && sv_demoDir.string[0])
	{
		snprintf(n,sizeof(n), "%s/%s", sv_demoDir.string, name + 6);
		name = n;

		if (sv_demoDirAlt.string[0]) {
			strlcpy(alternative_path, sv_demoDirAlt.string, sizeof(alternative_path));
			strlcat(alternative_path, "/", sizeof(alternative_path));
			strlcat(alternative_path, name + 6, sizeof(alternative_path));
		}
	}
	else if (!strncmp(name, "demonum/", 8))
	{
		int num = Q_atoi(name + 8);
		if (num == 0 && name[8] != '0')
		{
			char *num_s = name + 8;
			int num_s_len = strlen(num_s);
			for (num = 0; num < num_s_len; num++)
				if (num_s[num] != '.')
				{
					Con_Printf("usage: download demonum/num\n"
					           "if num is negative then download the Nth to last recorded demo, "
					           "also can type any quantity of dots and "
					           "where N dots is the Nth to last recorded demo\n");

					goto deny_download;
				}
			num &= 0xF;
			num <<= 24;
		}
		else
			num &= 0x00FFFFFF;
		name = SV_MVDNum(num);
		if (!name)
		{
			Con_Printf((char *)Q_yelltext((unsigned char*)va("Demo number %d not found.\n",
				(num & 0xFF000000) ? -(num >> 24) : ((num & 0x00800000) ? (num | 0xFF000000) : num) )));
			goto deny_download;
		}
		//Con_Printf("downloading demos/%s\n",name);
		snprintf(n, sizeof(n), "download demos/%s\n", name);

		ClientReliableWrite_Begin (sv_client, svc_stufftext,strlen(n) + 2);
		ClientReliableWrite_String (sv_client, n);
		return;
	}

	SV_UserCleanFilename(name);
	SV_UserCleanFilename(alternative_path);

	sv_client->downloadcount = 0;

#ifdef SERVERONLY
#define CLIENT_DOWNLOAD_RELATIVE_BASE FS_GAME // FIXME: Should we use FS_BASE ???
#else
#define CLIENT_DOWNLOAD_RELATIVE_BASE FS_BASE
#endif

	sv_client->download = FS_OpenVFS(name, "rb", CLIENT_DOWNLOAD_RELATIVE_BASE);
	if (!sv_client->download && alternative_path[0]) {
		sv_client->download = FS_OpenVFS(alternative_path, "rb", CLIENT_DOWNLOAD_RELATIVE_BASE);
	}
	if (sv_client->download) {
		sv_client->downloadsize = VFS_GETLEN(sv_client->download);
	}

	// if not techlogin, perform extra check to block .pak maps
	if (!sv_client->special) {
		// special check for maps that came from a pak file
		if (sv_client->download && !strncmp(name, "maps/", 5) && VFS_COPYPROTECTED(sv_client->download) && !(int)allow_download_pakmaps.value) {
			SV_ClientDownloadComplete(sv_client);
			goto deny_download;
		}
	}

	if (!sv_client->download)
	{
		Sys_Printf ("Couldn't download %s to %s\n", name, sv_client->name);
		goto deny_download;
	}

	// set donwload rate
	val = Info_Get (&sv_client->_userinfo_ctx_, "drate");
	sv_client->netchan.rate = 1. / SV_BoundRate(true, Q_atoi(*val ? val : "99999"));
	// disable duplicate packet setting while downloading
	sv_client->netchan.dupe = 0;

	// all checks passed, start downloading

#ifdef FTE_PEXT_CHUNKEDDOWNLOADS
	if (sv_client->fteprotocolextensions & FTE_PEXT_CHUNKEDDOWNLOADS)
	{
		ClientReliableWrite_Begin (sv_client, svc_download, 10+strlen(name));
		ClientReliableWrite_Long (sv_client, -1);
		ClientReliableWrite_Long (sv_client, sv_client->downloadsize);
		ClientReliableWrite_String (sv_client, name);
	}
#endif

	Cmd_NextDownload_f ();
	Sys_Printf ("Downloading %s to %s\n", name, sv_client->name);

	//bliP: download info/download url ->
	if (!strncmp(name, "maps/", 5))
	{
		SV_ClientPrintf (sv_client, PRINT_HIGH, "Map %s is %.0fKB (%.2fMB)\n",
		                 name, (float)sv_client->downloadsize / 1024,
		                 (float)sv_client->downloadsize / 1024 / 1024);
		if (download_map_url.string[0])
		{
			name += 5;
			SV_ClientPrintf (sv_client, PRINT_HIGH, "Download this map faster:\n");
			SV_ClientPrintf (sv_client, PRINT_HIGH, "%s%s\n\n",
			                 download_map_url.string, name);
		}
	}
	else
	{
		SV_ClientPrintf (sv_client, PRINT_HIGH, "File %s is %.0fKB (%.2fMB)\n",
		                 name, (float)sv_client->downloadsize / 1024,
		                 (float)sv_client->downloadsize / 1024 / 1024);
	}
	//<-

	return;

deny_download:

#ifdef FTE_PEXT_CHUNKEDDOWNLOADS
	if (sv_client->fteprotocolextensions & FTE_PEXT_CHUNKEDDOWNLOADS)
	{
		ClientReliableWrite_Begin (sv_client, svc_download, 10+strlen(name));
		ClientReliableWrite_Long (sv_client, -1);
		ClientReliableWrite_Long (sv_client, -1); // FIXME: -1 = Couldn't download, -2 = deny, we always use -1 atm
		ClientReliableWrite_String (sv_client, name);
	}
	else
#endif
	{
		ClientReliableWrite_Begin (sv_client, svc_download, 4);
		ClientReliableWrite_Short (sv_client, -1);
		ClientReliableWrite_Byte (sv_client, 0);
	}

	SV_DownloadNextFile();

	return;
}

/*
==================
Cmd_DemoDownload_f
==================
*/
static void Cmd_StopDownload_f(void);

static void Cmd_DemoDownload_f(void)
{
	int		i, num, cmd_argv_i_len;
	char		*cmd_argv_i;
	unsigned char	download_queue_cleared[] = "Download queue cleared.\n";
	unsigned char	download_queue_empty[] = "Download queue empty.\n";
	unsigned char	download_queue_already_exists[]	= "Download queue already exists.\n";
	unsigned char	cmdhelp_dldesc[] = "Download a demo from the server your are connected to";
	unsigned char	cmdhelp_dl[] = "cmd dl";
	unsigned char	cmdhelp_pound[] = "#";
	unsigned char	cmdhelp_dot[] = ".";
	unsigned char	cmdhelp_bs[] = "\\";
	unsigned char	cmdhelp_stop[] = "stop";
	unsigned char	cmdhelp_cancel[] = "cancel";

	if (Cmd_Argc() < 2)
	{
		Con_Printf("\n%s\n"
		           "Usage:\n"
		           "  %s %s [%s [%s]]\n"
		           "    \"#\" is one or several numbers from the demo list\n"
		           "  %s %s [%s%s [%s%s%s]]\n"
		           "    Each number of dots represents the Nth last recorded demo\n"
		           "    (Note that you can mix numbers and groups of dots)\n"
		           "  %s [%s|%s|%s]\n"
		           "     \"\\\", \"stop\" or \"cancel\" clear the download queue\n\n",
		           Q_redtext(cmdhelp_dldesc),
		           Q_redtext(cmdhelp_dl), Q_redtext(cmdhelp_pound), Q_redtext(cmdhelp_pound), Q_redtext(cmdhelp_pound),
		           Q_redtext(cmdhelp_dl), Q_redtext(cmdhelp_dot), Q_redtext(cmdhelp_dot), Q_redtext(cmdhelp_dot),
		           Q_redtext(cmdhelp_dot), Q_redtext(cmdhelp_dot), Q_redtext(cmdhelp_dot),
		           Q_redtext(cmdhelp_dl), Q_redtext(cmdhelp_bs), Q_redtext(cmdhelp_stop), Q_redtext(cmdhelp_cancel)
		);
		return;
	}

	if (!strcmp(Cmd_Argv(1), "cancel") || !strcmp(Cmd_Argv(1), "stop"))
	{
		Cmd_StopDownload_f(); // should not have any arguments, so it's OK
		return;
	}

	if (!strcmp(Cmd_Argv(1), "\\"))
	{
		if (sv_client->demonum[0])
		{
			Con_Printf((char *)Q_redtext(download_queue_cleared));
			sv_client->demonum[0] = 0;
		}
		else
			Con_Printf((char *)Q_redtext(download_queue_empty));
		return;
	}

	if (sv_client->demonum[0])
	{
		Con_Printf((char *)Q_redtext(download_queue_already_exists));
		return;
	}

	sv_client->demolist = ((sv_client->demonum[0] = Cmd_Argc()) > 2);
	for (i = 1; i < sv_client->demonum[0]; i++)
	{
		cmd_argv_i = Cmd_Argv(i);
		cmd_argv_i_len = strlen(cmd_argv_i);
		num = Q_atoi(cmd_argv_i);
		if (num == 0 && cmd_argv_i[0] != '0')
		{
			for (num = 0; num < cmd_argv_i_len; num++)
				if (cmd_argv_i[num] != '.')
				{
					num = 0;
					break;
				}
			if (num)
			{
				num &= 0xF;
				num <<= 24;
			}
		}
		else
			num &= 0x00FFFFFF;
		sv_client->demonum[sv_client->demonum[0] - i] = num;
	}
	SV_DownloadNextFile();
}

/*
==================
Cmd_StopDownload_f
==================
*/
static void Cmd_StopDownload_f(void)
{
	unsigned char	download_stopped[] = "Download stopped and download queue cleared.\n";

	if (!sv_client->download)
		return;

	sv_client->downloadcount = sv_client->downloadsize;
	SV_ClientDownloadComplete(sv_client);

#ifdef FTE_PEXT_CHUNKEDDOWNLOADS
	if (sv_client->fteprotocolextensions & FTE_PEXT_CHUNKEDDOWNLOADS)
	{
		char *name = ""; // FIXME: FTE's chunked dl does't support "cmd stopdl", work around

		ClientReliableWrite_Begin (sv_client, svc_download, 10+strlen(name));
		ClientReliableWrite_Long (sv_client, -1);
		ClientReliableWrite_Long (sv_client, -3); // -3 = dl was stopped
		ClientReliableWrite_String (sv_client, name);
	}
	else
#endif
	{
		ClientReliableWrite_Begin (sv_client, svc_download, 6);
		ClientReliableWrite_Short (sv_client, 0);
		ClientReliableWrite_Byte (sv_client, 100);
	}

	sv_client->demonum[0] = 0;
	sv_client->demolist = false;

	Con_Printf ((char *)Q_redtext(download_stopped));
}
//=============================================================================

/*
==================
SV_Say
==================
*/

static void SV_Say (qbool team)
{
	qbool    fake = false;
	client_t *client;
	int      j, tmp, cls = 0;
	char     *p;                  // used basically for QC based mods.
	char     text[2048] = {0};    // used if mod does not have own support for say/say_team.
	qbool    write_begin;

	if (Cmd_Argc () < 2)
		return;

	p = Cmd_Args();

	// unfake if requested.
	if (!team && sv_unfake.value)
	{
		char *ch;

		for (ch = p; *ch; ch++)
			if (*ch == 13)
				*ch = '#';
	}

	// remove surrounding " if any.
	if (p[0] == '"' && (j = (int)strlen(p)) > 2 && p[j-1] == '"')
	{
		// form text[].
		snprintf(text, sizeof(text), "%s", p + 1); // skip opening " and copy rest text including closing ".
		text[max(0,(int)strlen(text)-1)] = '\n';   // replace closing " with new line.
	}
	else
	{
		// form text[].
		snprintf(text, sizeof(text), "%s\n", p);
	}

	if (!sv_client->logged && !sv_client->logged_in_via_web)
	{
		SV_ParseLogin(sv_client);
		return;
	}

	// try handle say in the mod.
	SV_EndRedirect ();

	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(sv_player);

	j = PR_ClientSay(team, p);

	SV_BeginRedirect (RD_CLIENT);

	if (j)
		return; // say was handled by mod.

	if (sv_client->spectator && (!(int)sv_spectalk.value || team))
		strlcpy(text, va("[SPEC] %s: %s", sv_client->name, text), sizeof(text));
	else if (team)
		strlcpy(text, va("(%s): %s", sv_client->name, text), sizeof(text));
	else
	{
		strlcpy(text, va("%s: %s", sv_client->name, text), sizeof(text));
	}

	if (fp_messages) {
		if (curtime < sv_client->lockedtill) {
			SV_ClientPrintf(sv_client, PRINT_CHAT, "You can't talk for %d more seconds\n", (int) (sv_client->lockedtill - curtime));
			return;
		}
		tmp = sv_client->whensaidhead - fp_messages + 1;
		if (tmp < 0)
			tmp = 10+tmp;
		if (sv_client->whensaid[tmp] && (curtime - sv_client->whensaid[tmp] < fp_persecond)) {
			sv_client->lockedtill = curtime + fp_secondsdead;
			if (fp_msg[0]) {
				SV_ClientPrintf(sv_client, PRINT_CHAT, "FloodProt: %s\n", fp_msg);
			}
			else {
				SV_ClientPrintf(sv_client, PRINT_CHAT, "FloodProt: You can't talk for %d seconds.\n", fp_secondsdead);
			}
			return;
		}
		sv_client->whensaidhead++;
		if (sv_client->whensaidhead > 9)
			sv_client->whensaidhead = 0;
		sv_client->whensaid[sv_client->whensaidhead] = curtime;
	}

	Sys_Printf ("%s", text);
	SV_Write_Log(CONSOLE_LOG, 1, text);

	fake = ( strchr(text, 13) ? true : false ); // check if string contain "$\"

	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++)
	{
		if (client->state < cs_preconnected)
			continue;

		if (team)
		{
			// the spectator team
			if (sv_client->spectator)
			{
				if (!client->spectator)
					continue;	// on different teams
			}
			else
			{
				if (sv_client == client)
					; // send msg to self anyway
				else if (client->spectator)
				{
					if( !sv_sayteam_to_spec.value // player can't say_team to spec in this case
					    || !fake // self say_team does't contain $\ so this is treat as private message
					    || (client->spec_track <= 0 && strcmp(sv_client->team, client->team)) // spec do not track player and on different team
					    || (client->spec_track > 0 && strcmp(sv_client->team, svs.clients[client->spec_track - 1].team)) // spec track player on different team
					)
						continue;	// on different teams
				}
				else if (coop.value)
					; // allow team messages to everyone in coop from players.
				else if (!teamplay.value)
					continue; // non team game
				else if (strcmp(sv_client->team, client->team))
					continue; // on different teams
			}
		}
		else
		{
			if (sv_client->spectator)
			{
				// check for spectalk off.
				if (!client->spectator && !(int)sv_spectalk.value)
					continue; // off - specs can't talk to players.
			}
		}

		cls |= 1 << j;
		SV_ClientPrintf2(client, PRINT_CHAT, "%s", text);
	}

	if (!sv.mvdrecording || !cls)
		return;

	// non-team messages should be seen always, even if not tracking any player
	if (!team && ((sv_client->spectator && (int)sv_spectalk.value) || !sv_client->spectator))
	{
		write_begin = MVDWrite_Begin (dem_all, 0, strlen(text)+3);
	}
	else
	{
		write_begin = MVDWrite_Begin (dem_multiple, cls, strlen(text)+3);
	}

	if (write_begin)
	{
		MVD_MSG_WriteByte (svc_print);
		MVD_MSG_WriteByte (PRINT_CHAT);
		MVD_MSG_WriteString (text);
	}
}

/*
==================
Cmd_Say_f
==================
*/
static void Cmd_Say_f(void)
{
	SV_Say (false);
}
/*
==================
Cmd_Say_Team_f
==================
*/
static void Cmd_Say_Team_f(void)
{
	SV_Say (true);
}



//============================================================================

/*
=================
Cmd_Pings_f

The client is showing the scoreboard, so send new ping times for all
clients
=================
*/
static void Cmd_Pings_f (void)
{
	client_t *client;
	int j;

	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++)
	{
		if (!(client->state == cs_spawned || (client->state == cs_connected/* && client->spawncount != svs.spawncount*/)) )
			continue;

		ClientReliableWrite_Begin (sv_client, svc_updateping, 4);
		ClientReliableWrite_Byte (sv_client, j);
		ClientReliableWrite_Short (sv_client, SV_CalcPing(client));
		ClientReliableWrite_Begin (sv_client, svc_updatepl, 4);
		ClientReliableWrite_Byte (sv_client, j);
		ClientReliableWrite_Byte (sv_client, client->lossage);
	}
}


/*
==================
Cmd_Kill_f
==================
*/
static void Cmd_Kill_f (void)
{
	if (sv_player->v->health <= 0)
	{
		SV_ClientPrintf (sv_client, PRINT_HIGH, "Can't suicide -- already dead!\n");
		return;
	}

	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(sv_player);
	PR_ClientKill();
}

static void SV_NotifyStreamsOfPause(void)
{
	if (sv.mvdrecording) {
		sizebuf_t		msg;
		byte			msg_buf[20];

		SZ_InitEx(&msg, msg_buf, sizeof(msg_buf), true);
		MSG_WriteByte(&msg, svc_setpause);
		MSG_WriteByte(&msg, sv.paused ? 1 : 0);

		DemoWriteQTV(&msg);
	}
}

/*
==================
SV_TogglePause
==================
*/
void SV_TogglePause (const char *msg, int bit)
{
	int i;
	client_t *cl;
	extern cvar_t sv_paused;

	sv.paused ^= bit;
	
	Cvar_SetROM (&sv_paused, va("%i", sv.paused));

	if (sv.paused)
		sv.pausedsince = Sys_DoubleTime();

	if (msg)
		SV_BroadcastPrintf (PRINT_HIGH, "%s", msg);

	// send notification to all clients
	for (i=0, cl = svs.clients ; i<MAX_CLIENTS ; i++, cl++)
	{
		if (!cl->state)
			continue;
		ClientReliableWrite_Begin (cl, svc_setpause, 2);
		ClientReliableWrite_Byte (cl, sv.paused ? 1 : 0);

		cl->lastservertimeupdate = -99; // force an update to be sent
	}

	// send notification to all streams
	SV_NotifyStreamsOfPause();
}


/*
==================
Cmd_Pause_f
==================
*/
static void Cmd_Pause_f (void)
{
	char st[CLIENT_NAME_LEN + 32];
	qbool newstate;

	newstate = sv.paused ^ 1;

	if (!(int)pausable.value)
	{
		SV_ClientPrintf (sv_client, PRINT_HIGH, "Pause not allowed.\n");
		return;
	}

	if (sv_client->spectator)
	{
		SV_ClientPrintf (sv_client, PRINT_HIGH, "Spectators can not pause.\n");
		return;
	}

	if (GE_ShouldPause) {
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(sv_player);
		G_FLOAT(OFS_PARM0) = newstate;
		PR_ExecuteProgram (GE_ShouldPause);
		if (!G_FLOAT(OFS_RETURN))
			return;		// progs said ignore the request
	}

	if (newstate & 1)
		snprintf (st, sizeof(st), "%s paused the game\n", sv_client->name);
	else
		snprintf (st, sizeof(st), "%s unpaused the game\n", sv_client->name);

	SV_TogglePause(st, 1);
}


/*
=================
Cmd_Drop_f

The client is going to disconnect, so remove the connection immediately
=================
*/
static void Cmd_Drop_f (void)
{
	SV_EndRedirect ();
	if (sv_client->state == cs_zombie) // FIXME
		return; // FIXME
	if (!sv_client->spectator)
		SV_BroadcastPrintf (PRINT_HIGH, "%s dropped\n", sv_client->name);
	SV_DropClient (sv_client);
}

/*
=================
Cmd_PTrack_f

Change the bandwidth estimate for a client
=================
*/
static void Cmd_PTrack_f (void)
{
	int		i;
	edict_t *ent, *tent;

	if (!sv_client->spectator)
		return;

	if (Cmd_Argc() != 2)
	{
		// turn off tracking
		sv_client->spec_track = 0;
		ent = EDICT_NUM(sv_client - svs.clients + 1);
		tent = EDICT_NUM(0);
		ent->v->goalentity = EDICT_TO_PROG(tent);
		return;
	}

	i = Q_atoi(Cmd_Argv(1));
	if (i < 0 || i >= MAX_CLIENTS || svs.clients[i].state != cs_spawned || svs.clients[i].spectator)
	{
		SV_ClientPrintf (sv_client, PRINT_HIGH, "Invalid client to track\n");
		sv_client->spec_track = 0;
		ent = EDICT_NUM(sv_client - svs.clients + 1);
		tent = EDICT_NUM(0);
		ent->v->goalentity = EDICT_TO_PROG(tent);
		return;
	}
	sv_client->spec_track = i + 1; // now tracking

	ent = EDICT_NUM(sv_client - svs.clients + 1);
	tent = EDICT_NUM(i + 1);
	ent->v->goalentity = EDICT_TO_PROG(tent);
}

/*
=================
Cmd_Rate_f

Change the bandwidth estimate for a client
=================
*/
static void Cmd_Rate_f (void)
{
	int		rate;

	if (Cmd_Argc() != 2)
	{
		SV_ClientPrintf (sv_client, PRINT_HIGH, "Current rate is %i\n",
		                 (int)(1.0/sv_client->netchan.rate + 0.5));
		return;
	}

	rate = SV_BoundRate (sv_client->download != NULL, Q_atoi(Cmd_Argv(1)));

	SV_ClientPrintf (sv_client, PRINT_HIGH, "Net rate set to %i\n", rate);
	sv_client->netchan.rate = 1.0/rate;
}

//bliP: upload files ->
/*
=================
Cmd_TechLogin_f
Login to upload
=================
*/
int Master_Rcon_Validate (void);
static void Cmd_TechLogin_f (void)
{
	if (sv_client->logincount > 4) //denied
		return;

	if (Cmd_Argc() < 2)
	{
		if (sv_client->special)
		{
			sv_client->special = false;
			sv_client->logincount = 0;
			SV_ClientPrintf (sv_client, PRINT_HIGH, "Logged out.\n");
		}
		return;
	}

	if (!Master_Rcon_Validate()) //don't even let them know they're wrong
	{
		sv_client->logincount++;
		return;
	}

	sv_client->special = true;
	SV_ClientPrintf (sv_client, PRINT_HIGH, "Logged in.\n");
}

/*
================
Cmd_Upload_f
================
*/
static void Cmd_Upload_f (void)
{
	FILE *f;
	char str[MAX_OSPATH];

	if (sv_client->state != cs_spawned)
		return;

	if (!sv_client->special)
	{
		Con_Printf ("Client not tagged to upload.\n");
		return;
	}

	if (Cmd_Argc() != 3)
	{
		Con_Printf ("upload [local filename] [remote filename]\n");
		return;
	}

	snprintf(sv_client->uploadfn, sizeof(sv_client->uploadfn), "%s", Cmd_Argv(2));

	if (!sv_client->uploadfn[0])
	{ //just in case..
		Con_Printf ("Bad file name.\n");
		return;
	}

	if ((f = fopen(sv_client->uploadfn, "rb")))
	{
		Con_Printf ("File already exists.\n");
		fclose(f);
		return;
	}

	sv_client->remote_snap = false;
	FS_CreatePath (sv_client->uploadfn); //fixed, need to create path
	snprintf (str, sizeof (str), "cmd fileul \"%s\"\n", Cmd_Argv(1));
	ClientReliableWrite_Begin (sv_client, svc_stufftext, strlen(str) + 2);
	ClientReliableWrite_String (sv_client, str);
}
//<-

/*
==================
Cmd_SetInfo_f

Allow clients to change userinfo
==================
*/

char *shortinfotbl[] =
{
	"name",
	"team",
	"skin",
	"topcolor",
	"bottomcolor",
#ifdef CHAT_ICON_EXPERIMENTAL
	"chat",
#endif
	"gender",
	"*auth",
	"*flag",
	//"*client",
	//"*spectator",
	//"*VIP",
	NULL
};

static void Cmd_SetInfo_f (void)
{
	extern cvar_t sv_forcenick;
	sv_client_state_t saved_state;
	char oldval[MAX_EXT_INFO_STRING];
	char info[MAX_EXT_INFO_STRING];

	if (sv_kickuserinfospamtime.value > 0 && (int)sv_kickuserinfospamcount.value > 0)
	{
		if (!sv_client->lastuserinfotime ||
			curtime - sv_client->lastuserinfotime > sv_kickuserinfospamtime.value)
		{
			sv_client->lastuserinfocount = 0;
			sv_client->lastuserinfotime = curtime;
		}
		else if (++(sv_client->lastuserinfocount) > (int)sv_kickuserinfospamcount.value)
		{
			if (!sv_client->drop)
			{
				saved_state = sv_client->state;
				sv_client->state = cs_free;
				SV_BroadcastPrintf (PRINT_HIGH,
				                    "%s was kicked for userinfo spam\n", sv_client->name);
				sv_client->state = saved_state;
				SV_ClientPrintf (sv_client, PRINT_HIGH,
			    	             "You were kicked from the game for userinfo spamming\n");
				SV_LogPlayer (sv_client, "userinfo spam", 1);
				sv_client->drop = true;
			}
			return;
		}
	}

	switch (Cmd_Argc())
	{
		case 1:
			Con_Printf ("User info settings:\n");

			Info_ReverseConvert(&sv_client->_userinfo_ctx_, info, sizeof(info));
			Info_Print(info);
			Con_DPrintf ("[%d/%d]\n", strlen(info), sizeof(info));

			if (developer.value)
			{
				Con_Printf ("User info settings short:\n");
				Info_ReverseConvert(&sv_client->_userinfoshort_ctx_, info, sizeof(info));
				Info_Print(info);
				Con_DPrintf ("[%d/%d]\n", strlen(info), sizeof(info));
			}

			return;
		case 3:
			break;
		default:
			Con_Printf ("usage: setinfo [ <key> <value> ]\n");
			return;
	}

	if (Cmd_Argv(1)[0] == '*')
		return;		// don't set privileged values

	if (strchr(Cmd_Argv(1), '\\') || strchr(Cmd_Argv(2), '\\'))
		return;		// illegal char

	if (strstr(Cmd_Argv(1), "&c") || strstr(Cmd_Argv(1), "&r") || strstr(Cmd_Argv(2), "&c") || strstr(Cmd_Argv(2), "&r"))
		return;

	strlcpy(oldval, Info_Get(&sv_client->_userinfo_ctx_, Cmd_Argv(1)), sizeof(oldval));

	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(sv_player);
	if (PR_UserInfoChanged(0))
		return; // does not allowed to be changed by mod.

	Info_Set (&sv_client->_userinfo_ctx_, Cmd_Argv(1), Cmd_Argv(2));
	// name is extracted below in ExtractFromUserInfo
	//	strlcpy (sv_client->name, Info_ValueForKey (sv_client->userinfo, "name")
	//		, CLIENT_NAME_LEN);
	//	SV_FullClientUpdate (sv_client, &sv.reliable_datagram);
	//	sv_client->sendinfo = true;

	//Info_ValueForKey(sv_client->userinfo, Cmd_Argv(1));
	if (!strcmp(Info_Get(&sv_client->_userinfo_ctx_, Cmd_Argv(1)), oldval))
		return; // key hasn't changed

	if (!strcmp(Cmd_Argv(1), "name"))
	{
		//bliP: mute ->
		if (curtime < sv_client->lockedtill)
		{
			SV_ClientPrintf(sv_client, PRINT_CHAT, "You can't change your name while you're muted\n");
			return;
		}
		//<-
		//VVD: forcenick ->
		//meag: removed sv_login check to allow optional logins... sv_forcenick should still take effect
		if ((int)sv_forcenick.value && /*(int)sv_login.value &&*/ sv_client->login[0])
		{
			// allow differences in case, redtext
			if (Q_namecmp(sv_client->login, Cmd_Argv(2))) {
				SV_ClientPrintf(sv_client, PRINT_CHAT, "You can't change your name while logged in on this server.\n");
				Info_Set(&sv_client->_userinfo_ctx_, "name", sv_client->login);
				strlcpy(sv_client->name, sv_client->login, CLIENT_NAME_LEN);
				MSG_WriteByte(&sv_client->netchan.message, svc_stufftext);
				MSG_WriteString(&sv_client->netchan.message, va("name %s\n", sv_client->login));
				MSG_WriteByte(&sv_client->netchan.message, svc_stufftext);
				MSG_WriteString(&sv_client->netchan.message, va("setinfo name %s\n", sv_client->login));
				return;
			}
		}
		//<-
	}

	//bliP: kick top ->
	if ((int)sv_kicktop.value && !strcmp(Cmd_Argv(1), "topcolor"))
	{
		if (!sv_client->lasttoptime || curtime - sv_client->lasttoptime > 8)
		{
			sv_client->lasttopcount = 0;
			sv_client->lasttoptime = curtime;
		}
		else if (sv_client->lasttopcount++ > 5)
		{
			if (!sv_client->drop)
			{
				saved_state = sv_client->state;
				sv_client->state = cs_free;
				SV_BroadcastPrintf (PRINT_HIGH,
				                    "%s was kicked for topcolor spam\n", sv_client->name);
				sv_client->state = saved_state;
				SV_ClientPrintf (sv_client, PRINT_HIGH,
				                 "You were kicked from the game for topcolor spamming\n");
				SV_LogPlayer (sv_client, "topcolor spam", 1);
				sv_client->drop = true;
			}
			return;
		}
	}
	//<-

	ProcessUserInfoChange (sv_client, Cmd_Argv (1), oldval);
	PR_UserInfoChanged(1);
}

void ProcessUserInfoChange (client_t* sv_client, const char* key, const char* old_value)
{
	int i;

	// process any changed values
	SV_ExtractFromUserinfo (sv_client, !strcmp(key, "name"));

	if (mod_UserInfo_Changed)
	{
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(sv_client->edict);
		PR_SetTmpString(&G_INT(OFS_PARM0), key);
		PR_SetTmpString(&G_INT(OFS_PARM1), old_value);
		PR_SetTmpString(&G_INT(OFS_PARM2), Info_Get(&sv_client->_userinfo_ctx_, key));
		PR_ExecuteProgram (mod_UserInfo_Changed);
	}

	for (i = 0; shortinfotbl[i] != NULL; i++)
	{
		if (!strcmp(key, shortinfotbl[i]))
		{
			char *nuw = Info_Get(&sv_client->_userinfo_ctx_, key);

			Info_SetStar (&sv_client->_userinfoshort_ctx_, key, nuw);

			i = sv_client - svs.clients;
			MSG_WriteByte (&sv.reliable_datagram, svc_setinfo);
			MSG_WriteByte (&sv.reliable_datagram, i);
			MSG_WriteString (&sv.reliable_datagram, key);
			MSG_WriteString (&sv.reliable_datagram, nuw);
			break;
		}
	}
}

/*
==================
SV_ShowServerinfo_f

Dumps the serverinfo info string
==================
*/
static void Cmd_ShowServerinfo_f (void)
{
	Info_Print (svs.info);
}

static void Cmd_NoSnap_f(void)
{
	if (*sv_client->uploadfn)
	{
		*sv_client->uploadfn = 0;
		SV_BroadcastPrintf (PRINT_HIGH, "%s refused remote screenshot\n", sv_client->name);
	}
}

/*
==============
Cmd_MinPing_f
==============
*/
static void Cmd_MinPing_f (void)
{
	float minping;
	switch (Cmd_Argc())
	{
	case 2:
		if (GameStarted())
			Con_Printf("Can't change sv_minping: demo recording or match in progress.\n");
		else if (!(int)sv_enable_cmd_minping.value)
			Con_Printf("Can't change sv_minping: sv_enable_cmd_minping == 0.\n");
		else
		{
			minping = Q_atof(Cmd_Argv(1));
			if (minping < 0 || minping > 300)
				Con_Printf("Value must be >= 0 and <= 300.\n");
			else
				Cvar_SetValue (&sv_minping, (int)minping);
		}
	case 1:
		Con_Printf("sv_minping = %s\n", sv_minping.string);
		break;
	default:
		Con_Printf("usage: minping [<value>]\n<value> = '' show current sv_minping value\n");
	}
}

/*
==============
Cmd_AirStep_f
==============
*/
static void Cmd_AirStep_f (void)
{
	int val;
	unsigned char red_airstep[64] = "pm_airstep";

	if (sv_client->spectator) {
		Con_Printf("Spectators can't change pm_airstep\n");
		return;
	}

	switch (Cmd_Argc())
	{
	case 2:
		if (GameStarted())
			Con_Printf("Can't change pm_airstep: demo recording in progress or serverinfo key status is not 'Standby'.\n");
		else
		{
			val = Q_atoi(Cmd_Argv(1));
			if (val != 0 && val != 1)
				Con_Printf("Value must be 0 or 1.\n");
			else {
				float old = pm_airstep.value; // remember
				Cvar_Set (&pm_airstep, val ? "1" : ""); // set new value

				if (pm_airstep.value != old) { // seems value was changed
					SV_BroadcastPrintf (2, "%s turns %s %s\n", 
								sv_client->name, Q_redtext(red_airstep), pm_airstep.value ? "on" : "off");
					break;
				}
			}
		}
	case 1:
		Con_Printf("pm_airstep = %s\n", pm_airstep.string);
		break;
	default:
		Con_Printf("usage: airstep [0 | 1]\n");
	}
}

/*
==============
Cmd_ShowMapsList_f
==============
*/
static void Cmd_ShowMapsList_f(void)
{
	char	*value, *key;
	int	i, j, len, i_mod_2 = 1;
	unsigned char	ztndm3[] = "ztndm3";
	unsigned char	list_of_custom_maps[] = "list of custom maps";
	unsigned char	end_of_list[] = "end of list";

	Con_Printf("Vote for maps by typing the mapname, for example \"%s\"\n\n---%s\n",
	           Q_redtext(ztndm3), Q_redtext(list_of_custom_maps));

	for (i = LOCALINFO_MAPS_LIST_START; i <= LOCALINFO_MAPS_LIST_END; i++)
	{
		key = va("%d", i);
		value = Info_Get(&_localinfo_, key);
		if (*value)
		{

			if (!(i_mod_2 = i % 2))
			{
				if ((len = 19 - strlen(value)) < 1)
					len = 1;
				for (j = 0; j < len; j++)
					strlcat(value, " ", MAX_KEY_STRING);
			}
			Con_Printf("%s%s", value, i_mod_2 ? "\n" : "");
		}
		else
			break;
	}
	Con_Printf("%s---%s\n", i_mod_2 ? "" : "\n", Q_redtext(end_of_list));
}

static void SetUpClientEdict (client_t *cl, edict_t *ent)
{
	ED_ClearEdict(ent);
	// restore client name.
	PR_SetEntityString(ent, ent->v->netname, cl->name);
	// so spec will have right goalentity - if speccing someone
	if(cl->spectator && cl->spec_track > 0)
		ent->v->goalentity = EDICT_TO_PROG(svs.clients[cl->spec_track-1].edict);

	ent->v->colormap = NUM_FOR_EDICT(ent);

	ent->v->team = 0; // FIXME

	cl->entgravity = 1.0;
	if (fofs_gravity)
		EdictFieldFloat(ent, fofs_gravity) = 1.0;

	cl->maxspeed = sv_maxspeed.value;
	if (fofs_maxspeed)
		EdictFieldFloat(ent, fofs_maxspeed) = (int)sv_maxspeed.value;
}

extern cvar_t spectator_password, password;
extern void MVD_PlayerReset(int player);
extern void CountPlayersSpecsVips(int *clients_ptr, int *spectators_ptr, int *vips_ptr, client_t **newcl_ptr);
extern qbool SpectatorCanConnect(int vip, int spass, int spectators, int vips);
extern qbool PlayerCanConnect(int clients);

/*
==================
Cmd_Join_f

Set client to player mode without reconnecting
==================
*/
static void Cmd_Join_f (void)
{
	int i;
	int clients;

	if (sv_client->state != cs_spawned)
		return;

	if (!sv_client->spectator)
		return; // already a player

	if (!(sv_client->extensions & Z_EXT_JOIN_OBSERVE)) {
		SV_ClientPrintf (sv_client, PRINT_HIGH, "Your QW client doesn't support this command\n");
		return;
	}

	if (password.string[0] && strcmp (password.string, "none")) {
		SV_ClientPrintf (sv_client, PRINT_HIGH, "This server requires a %s password. Please disconnect, set the password and reconnect as %s.\n", "player", "player");
		return;
	}

	// Might have been 'not necessary' for spectator but needed for player
	if (SV_LoginBlockJoinRequest(sv_client)) {
		return;
	}

	if (SV_ClientConnectedTime(sv_client) < 5) {
		SV_ClientPrintf(sv_client, PRINT_HIGH, "Wait %d seconds\n", 5 - (int)SV_ClientConnectedTime(sv_client));
		return;
	}

	// count players already on server
	CountPlayersSpecsVips(&clients, NULL, NULL, NULL);
	if (!PlayerCanConnect(clients)) {
		SV_ClientPrintf (sv_client, PRINT_HIGH, "Can't join, all player slots full\n");
		return;
	}

	if (!PlayerCheckPing()) {
		return;
	}

	// call the prog function for removing a client
	// this will set the body to a dead frame, among other things
	pr_global_struct->self = EDICT_TO_PROG(sv_player);
	PR_GameClientDisconnect(1);

	// this is like SVC_DirectConnect.
	// turn the spectator into a player
	sv_client->old_frags = 0;
	SV_SetClientConnectionTime(sv_client);
	sv_client->spectator = false;
	sv_client->spec_track = 0;
	Info_Remove(&sv_client->_userinfo_ctx_, "*spectator");
	Info_Remove(&sv_client->_userinfoshort_ctx_, "*spectator");

	// like Cmd_Spawn_f()
	SetUpClientEdict (sv_client, sv_client->edict);

	// call the progs to get default spawn parms for the new client
	PR_GameSetNewParms();

	// copy spawn parms out of the client_t
	for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
		sv_client->spawn_parms[i] = (&PR_GLOBAL(parm1))[i];

	// call the spawn function
	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(sv_player);
	G_FLOAT(OFS_PARM0) = (float) sv_client->vip;
	PR_GameClientConnect(0);

	// actually spawn the player
	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(sv_player);
	G_FLOAT(OFS_PARM0) = (float) sv_client->vip;
	PR_GamePutClientInServer(0);

	// look in SVC_DirectConnect() for for extended comment whats this for
	MVD_PlayerReset(NUM_FOR_EDICT(sv_player) - 1);

	// send notification to all clients
	sv_client->sendinfo = true;
}

/*
==================
Cmd_Observe_f

Set client to spectator mode without reconnecting
==================
*/
static void Cmd_Observe_f (void)
{
	int i;
	int spectators, vips;

	if (sv_client->state != cs_spawned)
		return;
	if (sv_client->spectator)
		return; // already a spectator

	if (!(sv_client->extensions & Z_EXT_JOIN_OBSERVE)) {
		SV_ClientPrintf (sv_client, PRINT_HIGH, "Your QW client doesn't support this command\n");
		return;
	}

	if (spectator_password.string[0] && strcmp (spectator_password.string, "none")) {
		SV_ClientPrintf (sv_client, PRINT_HIGH, "This server requires a %s password. Please disconnect, set the password and reconnect as %s.\n", "spectator", "spectator");
		return;
	}

	if (SV_ClientConnectedTime(sv_client) < 5)
	{
		SV_ClientPrintf (sv_client, PRINT_HIGH, "Wait %d seconds\n", 5 - (int)SV_ClientConnectedTime(sv_client));
		return;
	}

	// count spectators already on server
	CountPlayersSpecsVips(NULL, &spectators, &vips, NULL);

	if (!SpectatorCanConnect(sv_client->vip, true/*kinda HACK*/, spectators, vips))
	{
		SV_ClientPrintf (sv_client, PRINT_HIGH, "Can't join, all spectator/vip slots full\n");
		return;
	}

	// call the prog function for removing a client
	// this will set the body to a dead frame, among other things
	pr_global_struct->self = EDICT_TO_PROG(sv_player);
	PR_GameClientDisconnect(0);

	// this is like SVC_DirectConnect.
	// turn the player into a spectator
	sv_client->old_frags = 0;
	SV_SetClientConnectionTime(sv_client);
	sv_client->spectator = true;
	sv_client->spec_track = 0;
	Info_SetStar (&sv_client->_userinfo_ctx_, "*spectator", "1");
	Info_SetStar (&sv_client->_userinfoshort_ctx_, "*spectator", "1");

	// like Cmd_Spawn_f()
	SetUpClientEdict (sv_client, sv_client->edict);

	// call the progs to get default spawn parms for the new client
	PR_GameSetNewParms();

	SV_SpawnSpectator ();

	// copy spawn parms out of the client_t
	for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
		sv_client->spawn_parms[i] = (&PR_GLOBAL(parm1))[i];

	// call the spawn function
	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(sv_player);
	G_FLOAT(OFS_PARM0) = (float) sv_client->vip;
	PR_GameClientConnect(1);

	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(sv_player);
	PR_GamePutClientInServer(1); // let mod know we put spec not player

	// look in SVC_DirectConnect() for for extended comment whats this for
	MVD_PlayerReset(NUM_FOR_EDICT(sv_player) - 1);

	// send notification to all clients
	sv_client->sendinfo = true;
}

#ifdef FTE_PEXT2_VOICECHAT
/*
Privacy issues:
By sending voice chat to a server, you are unsure who might be listening.
Voice can be recorded to an mvd, potentially including voice.
Spectators tracking you are able to hear team chat of your team.
You're never quite sure if anyone might join the server and your team before you finish saying a sentance.
You run the risk of sounds around you being recorded by quake, including but not limited to: TV channels, loved ones, phones, YouTube videos featuring certain moans.
Default on non-team games is to broadcast.
*/

// FTEQW type and naming compatibility
// it's not really necessary, simple find & replace would do the job too

#define qboolean qbool
#define host_client sv_client
#define ival value // for cvars compatibility

#define VOICE_RING_SIZE 512 /*POT*/
struct
{
	struct voice_ring_s
	{
		unsigned int sender;
		unsigned char receiver[MAX_CLIENTS/8];
		unsigned char gen;
		unsigned char seq;
		unsigned int datalen;
		unsigned char data[1024];
	} ring[VOICE_RING_SIZE];
	unsigned int write;
} voice;

void SV_VoiceReadPacket(void)
{
	unsigned int vt = host_client->voice_target;
	unsigned int j;
	struct voice_ring_s *ring;
	unsigned short bytes;
	client_t *cl;
	unsigned char gen = MSG_ReadByte();
	unsigned char seq = MSG_ReadByte();
	/*read the data from the client*/
	bytes = MSG_ReadShort();
	ring = &voice.ring[voice.write & (VOICE_RING_SIZE-1)];
	if (bytes > sizeof(ring->data) || curtime < host_client->lockedtill || !sv_voip.ival) {
		MSG_ReadSkip(bytes);
		return;
	}
	else {
		voice.write++;
		MSG_ReadData(ring->data, bytes);
	}

	ring->datalen = bytes;
	ring->sender = host_client - svs.clients;
	ring->gen = gen;
	ring->seq = seq;

	/*broadcast it its to their team, and its not teamplay*/
	if (vt == VT_TEAM && !teamplay.ival)
		vt = VT_ALL;

	/*figure out which team members are meant to receive it*/
	for (j = 0; j < MAX_CLIENTS/8; j++)
		ring->receiver[j] = 0;
	for (j = 0, cl = svs.clients; j < MAX_CLIENTS; j++, cl++)
	{
		if (cl->state != cs_spawned && cl->state != cs_connected)
			continue;
		/*spectators may only talk to spectators*/
		if (host_client->spectator && !sv_spectalk.ival)
			if (!cl->spectator)
				continue;

		if (vt == VT_TEAM)
		{
			// the spectator team
			if (host_client->spectator)
			{
				if (!cl->spectator)
					continue;
			}
			else
			{
				if (strcmp(cl->team, host_client->team) || cl->spectator)
					continue;	// on different teams
			}
		}
		else if (vt == VT_NONMUTED)
		{
			if (host_client->voice_mute[j>>3] & (1<<(j&3)))
				continue;
		}
		else if (vt >= VT_PLAYERSLOT0)
		{
			if (j != vt - VT_PLAYERSLOT0)
				continue;
		}

		ring->receiver[j>>3] |= 1<<(j&3);
	}

	if (sv.mvdrecording && sv_voip_record.ival && !(sv_voip_record.ival == 2 && !host_client->spectator))
	{
		// non-team messages should be seen always, even if not tracking any player
		if (vt == VT_ALL && (!host_client->spectator || sv_spectalk.ival))
		{
			MVDWrite_Begin (dem_all, 0, ring->datalen+6);
		}
		else
		{
			unsigned int cls;
			cls = ring->receiver[0] |
				(ring->receiver[1]<<8) |
				(ring->receiver[2]<<16) |
				(ring->receiver[3]<<24);

			if (!cls) {
				// prevent dem_multiple(0) being sent
				return;
			}

			MVDWrite_Begin(dem_multiple, cls, ring->datalen + 6);
		}

		MVD_MSG_WriteByte( svc_fte_voicechat);
		MVD_MSG_WriteByte( ring->sender);
		MVD_MSG_WriteByte( ring->gen);
		MVD_MSG_WriteByte( ring->seq);
		MVD_MSG_WriteShort( ring->datalen);
		MVD_SZ_Write(       ring->data, ring->datalen);
	}
}

void SV_VoiceInitClient(client_t *client)
{
	client->voice_target = VT_TEAM;
	client->voice_active = false;
	client->voice_read = voice.write;
	memset(client->voice_mute, 0, sizeof(client->voice_mute));
}

void SV_VoiceSendPacket(client_t *client, sizebuf_t *buf)
{
	unsigned int clno;
	qboolean send;
	struct voice_ring_s *ring;
//	client_t *split;

//	if (client->controller)
//		client = client->controller;
	clno = client - svs.clients;

	if (!(client->fteprotocolextensions2 & FTE_PEXT2_VOICECHAT))
		return;
	if (!client->voice_active || client->num_backbuf)
	{
		client->voice_read = voice.write;
		return;
	}

	while(client->voice_read < voice.write)
	{
		/*they might be too far behind*/
		if (client->voice_read+VOICE_RING_SIZE < voice.write)
			client->voice_read = voice.write - VOICE_RING_SIZE;

		ring = &voice.ring[(client->voice_read) & (VOICE_RING_SIZE-1)];

		/*figure out if it was for us*/
		send = false;
		if (ring->receiver[clno>>3] & (1<<(clno&3)))
			send = true;

		// FIXME: qqshka: well, is it RIGHTWAY at all???
#if 0  // qqshka: I am turned it off.
		/*if you're spectating, you can hear whatever your tracked player can hear*/
		if (client->spectator && client->spec_track)
			if (ring->receiver[(client->spec_track-1)>>3] & (1<<((client->spec_track-1)&3)))
				send = true;
#endif

		if (client->voice_mute[ring->sender>>3] & (1<<(ring->sender&3)))
			send = false;

		if (ring->sender == clno && !sv_voip_echo.ival)
			send = false;

		/*additional ways to block voice*/
		if (client->download)
			send = false;

		if (send)
		{
			if (buf->maxsize - buf->cursize < ring->datalen+5)
				break;
			MSG_WriteByte(buf, svc_fte_voicechat);
			MSG_WriteByte(buf, ring->sender);
			MSG_WriteByte(buf, ring->gen);
			MSG_WriteByte(buf, ring->seq);
			MSG_WriteShort(buf, ring->datalen);
			SZ_Write(buf, ring->data, ring->datalen);
		}
		client->voice_read++;
	}
}

void SV_Voice_Ignore_f(void)
{
	unsigned int other;
	int type = 0;

	if (Cmd_Argc() < 2)
	{
		/*only a name = toggle*/
		type = 0;
	}
	else
	{
		/*mute if 1, unmute if 0*/
		if (atoi(Cmd_Argv(2)))
			type = 1;
		else
			type = -1;
	}
	other = atoi(Cmd_Argv(1));
	if (other >= MAX_CLIENTS)
		return;

	switch(type)
	{
	case -1:
		host_client->voice_mute[other>>3] &= ~(1<<(other&3));
		break;
	case 0:	
		host_client->voice_mute[other>>3] ^= (1<<(other&3));
		break;
	case 1:
		host_client->voice_mute[other>>3] |= (1<<(other&3));
	}
}

void SV_Voice_Target_f(void)
{
	unsigned int other;
	char *t = Cmd_Argv(1);
	if (!strcmp(t, "team"))
		host_client->voice_target = VT_TEAM;
	else if (!strcmp(t, "all"))
		host_client->voice_target = VT_ALL;
	else if (!strcmp(t, "nonmuted"))
		host_client->voice_target = VT_NONMUTED;
	else if (*t >= '0' && *t <= '9')
	{
		other = atoi(t);
		if (other >= MAX_CLIENTS)
			return;
		host_client->voice_target = VT_PLAYERSLOT0 + other;
	}
	else
	{
		/*don't know who you mean, futureproofing*/
		host_client->voice_target = VT_TEAM;
	}
}

void SV_Voice_MuteAll_f(void)
{
	host_client->voice_active = false;
}

void SV_Voice_UnmuteAll_f(void)
{
	host_client->voice_active = true;
}

#endif // FTE_PEXT2_VOICECHAT

/*
 * Parse protocol extensions which supported by client.
 * This is workaround for the proxy case, like: qwfwd. We can't use it in case of qizmo thought.
 */
void Cmd_PEXT_f(void)
{
	int idx;
	int proto_ver, proto_value;

	if (!sv_client->process_pext)
		return; // sorry, we do not expect it right now.

	sv_client->process_pext = false;

	for ( idx = 1; idx < Cmd_Argc(); )
	{
		proto_ver   = Q_atoi(Cmd_Argv(idx++));
		proto_value = Q_atoi(Cmd_Argv(idx++));

		switch( proto_ver )
		{
#ifdef PROTOCOL_VERSION_FTE
		case PROTOCOL_VERSION_FTE:
			// do not reset it.
			if (!sv_client->fteprotocolextensions)
			{
				sv_client->fteprotocolextensions = proto_value & svs.fteprotocolextensions;
				if (sv_client->fteprotocolextensions)
					Con_DPrintf("PEXT: Client supports 0x%x fte extensions\n", sv_client->fteprotocolextensions);
			}
			break;
#endif // PROTOCOL_VERSION_FTE

#ifdef PROTOCOL_VERSION_FTE2
		case PROTOCOL_VERSION_FTE2:
			// do not reset it.
			if (!sv_client->fteprotocolextensions2)
			{
				sv_client->fteprotocolextensions2 = proto_value & svs.fteprotocolextensions2;
				if (sv_client->fteprotocolextensions2)
					Con_DPrintf("PEXT: Client supports 0x%x fte extensions2\n", sv_client->fteprotocolextensions2);
			}
			break;
#endif // PROTOCOL_VERSION_FTE2

#ifdef PROTOCOL_VERSION_MVD1
		case PROTOCOL_VERSION_MVD1:
			if (!sv_client->mvdprotocolextensions1)
			{
				sv_client->mvdprotocolextensions1 = proto_value & svs.mvdprotocolextension1;
				if (sv_client->mvdprotocolextensions1)
					Con_DPrintf("PEXT: Client supports 0x%x mvdsv extensions\n", sv_client->mvdprotocolextensions1);
			}
			break;
#endif
		}
	}

	// we are ready for new command now.
	MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
	MSG_WriteString (&sv_client->netchan.message, "cmd new\n");
}

#if defined(SERVERONLY) && defined(WWW_INTEGRATION)
// { Central login
void Cmd_Login_f(void)
{
	extern cvar_t sv_login;

	if (sv_client->state != cs_spawned && !(int)sv_login.value) {
		SV_ClientPrintf2(sv_client, PRINT_HIGH, "Cannot login during connection\n");
		return;
	}

	if (Cmd_Argc() != 2) {
		SV_ClientPrintf2(sv_client, PRINT_HIGH, "Usage: login <username>\n");
		return;
	}

	if (curtime - sv_client->login_request_time < LOGIN_MIN_RETRY_TIME) {
		SV_ClientPrintf2(sv_client, PRINT_HIGH, "Please wait and try again\n");
		return;
	}

	if (sv_client->logged_in_via_web) {
		SV_ClientPrintf2(sv_client, PRINT_HIGH, "You are already logged in as '%s'\n", sv_client->login);
		return;
	}

	if (sv_client->state != cs_spawned) {
		SV_ParseWebLogin(sv_client);
	}
	else {
		Central_GenerateChallenge(sv_client, Cmd_Argv(1), false);
	}
}

void Cmd_ChallengeResponse_f(void)
{
	if (Cmd_Argc() != 2) {
		MSG_WriteByte(&sv_client->netchan.message, svc_print);
		MSG_WriteByte(&sv_client->netchan.message, PRINT_HIGH);
		MSG_WriteString(&sv_client->netchan.message, "Usage: challenge-response <response>\n");
		return;
	}

	if (!sv_client->login_challenge[0]) {
		MSG_WriteByte(&sv_client->netchan.message, svc_print);
		MSG_WriteByte(&sv_client->netchan.message, PRINT_HIGH);
		MSG_WriteString(&sv_client->netchan.message, "Please wait and try again\n");
		return;
	}

	Central_VerifyChallengeResponse(sv_client, sv_client->login_challenge, Cmd_Argv(1));
}

void Cmd_Logout_f(void)
{
	extern cvar_t sv_login;

	if (sv_client->logged_in_via_web) {
		if (sv_client->login[0]) {
			SV_BroadcastPrintf(PRINT_HIGH, "%s logged out\n", sv_client->name);
		}

		SV_Logout(sv_client);

		// 
		if (!(int)sv_login.value || ((int)sv_login.value == 1 && sv_client->spectator)) {
			sv_client->logged = -1;
		}
	}

	// If logins are mandatory then treat as disconnect
	if ((int)sv_login.value > 1 || ((int)sv_login.value == 1 && !sv_client->spectator)) {
		SV_DropClient(sv_client);
	}
}
// } Central login
#endif

void SV_DemoList_f(void);
void SV_DemoListRegex_f(void);
void SV_MVDInfo_f(void);
void SV_LastScores_f(void);

// { bans
void SV_Cmd_Ban_f(void);
void SV_Cmd_Banip_f(void);
void SV_Cmd_Banremove_f(void);
// } bans

// { qtv
void Cmd_Qtvusers_f (void);
// }

// { cheats
void SV_God_f (void);
void SV_Give_f (void);
void SV_Noclip_f (void);
void SV_Fly_f (void);
// }

#if defined(SERVERONLY) && defined(WWW_INTEGRATION)
// { central login
void Cmd_Login_f(void);
void Cmd_Logout_f(void);
void Cmd_ChallengeResponse_f(void);
// }
#endif

typedef struct
{
	char	*name;
	void	(*func) (void);
	qbool	overrideable;
}
ucmd_t;


static ucmd_t ucmds[] =
{
	{"new", Cmd_New_f, false},
	{"modellist", Cmd_Modellist_f, false},
	{"soundlist", Cmd_Soundlist_f, false},
	{"prespawn", Cmd_PreSpawn_f, false},
	{"spawn", Cmd_Spawn_f, false},
	{"begin", Cmd_Begin_f, false},

	{"drop", Cmd_Drop_f, false},
	{"pings", Cmd_Pings_f, false},

	// issued by hand at client consoles
	{"rate", Cmd_Rate_f, true},
	{"kill", Cmd_Kill_f, true},
	{"pause", Cmd_Pause_f, true},

	{"say", Cmd_Say_f, true},
	{"say_team", Cmd_Say_Team_f, true},

	{"setinfo", Cmd_SetInfo_f, false},

	{"serverinfo", Cmd_ShowServerinfo_f, false},

	{"download", Cmd_Download_f, false},
	{"nextdl", Cmd_NextDownload_f, false},
	{"dl", Cmd_DemoDownload_f, false},

	{"ptrack", Cmd_PTrack_f, false}, //ZOID - used with autocam

	//bliP: file upload ->
	{"techlogin", Cmd_TechLogin_f, false},
	{"upload", Cmd_Upload_f, false},
	//<-

	{"snap", Cmd_NoSnap_f, false},
	{"stopdownload", Cmd_StopDownload_f, false},
	{"stopdl", Cmd_StopDownload_f, false},
	{"dlist", SV_DemoList_f, false},
	{"dlistr", SV_DemoListRegex_f, false},
	{"dlistregex", SV_DemoListRegex_f, false},
	{"demolist", SV_DemoList_f, false},
	{"demolistr", SV_DemoListRegex_f, false},
	{"demolistregex", SV_DemoListRegex_f, false},
	{"demoinfo", SV_MVDInfo_f, false},
	{"lastscores", SV_LastScores_f, false},
	{"minping", Cmd_MinPing_f, true},
	{"airstep", Cmd_AirStep_f, true},
	{"maps", Cmd_ShowMapsList_f, true},
	{"ban", SV_Cmd_Ban_f, true}, // internal server ban support
	{"banip", SV_Cmd_Banip_f, true}, // internal server ban support
	{"banrem", SV_Cmd_Banremove_f, true}, // internal server ban support

	{"join", Cmd_Join_f, true},
	{"observe", Cmd_Observe_f, true},

	{"qtvusers", Cmd_Qtvusers_f, true},

	// cheat commands
	{"god", SV_God_f, true},
	{"give", SV_Give_f, true},
	{"noclip", SV_Noclip_f, true},
	{"fly", SV_Fly_f, true},

#ifdef FTE_PEXT2_VOICECHAT
	{"voicetarg", SV_Voice_Target_f, false},
	{"vignore", SV_Voice_Ignore_f, false},	/*ignore/mute specific player*/
	{"muteall", SV_Voice_MuteAll_f, false},	/*disables*/
	{"unmuteall", SV_Voice_UnmuteAll_f, false}, /*reenables*/
#endif

	{"pext", Cmd_PEXT_f, false}, // user reply with supported protocol extensions.

#if defined(SERVERONLY) && defined(WWW_INTEGRATION)
	{"login", Cmd_Login_f, false},
	{"login-response", Cmd_ChallengeResponse_f, false},
	{"logout", Cmd_Logout_f, false},
#endif

	{NULL, NULL}
};

static qbool SV_ExecutePRCommand (void)
{
	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(sv_player);
	return PR_ClientCmd();
}

/*
==================
SV_ExecuteUserCommand
==================
*/
static void SV_ExecuteUserCommand (char *s)
{
	ucmd_t *u;

	Cmd_TokenizeString (s);
	sv_player = sv_client->edict;

	SV_BeginRedirect (RD_CLIENT);

	for (u=ucmds ; u->name ; u++) {
		if (!strcmp (Cmd_Argv(0), u->name) ) {
			if (!u->overrideable) {
				u->func();
				goto out;
			}
			break;
		}
	}

	if (SV_ExecutePRCommand())
		goto out;

	if (u->name)
		u->func();	
	else
		Con_Printf("Bad user command: %s\n", Cmd_Argv(0));

out:
	SV_EndRedirect ();
}

/*
===========================================================================

USER CMD EXECUTION

===========================================================================
*/

/*
====================
AddLinksToPmove

====================
*/
static void AddLinksToPmove ( areanode_t *node )
{
	link_t		*l, *next;
	edict_t		*check;
	int 		pl;
	int 		i;
	physent_t	*pe;
	vec3_t		pmove_mins, pmove_maxs;

	for (i=0 ; i<3 ; i++)
	{
		pmove_mins[i] = pmove.origin[i] - 256;
		pmove_maxs[i] = pmove.origin[i] + 256;
	}

	pl = EDICT_TO_PROG(sv_player);

	// touch linked edicts
	for (l = node->solid_edicts.next ; l != &node->solid_edicts ; l = next)
	{
		next = l->next;
		check = EDICT_FROM_AREA(l);

		if (check->v->owner == pl)
			continue;		// player's own missile
		if (check->v->solid == SOLID_BSP
				|| check->v->solid == SOLID_BBOX
				|| check->v->solid == SOLID_SLIDEBOX)
		{
			if (check == sv_player)
				continue;

			for (i=0 ; i<3 ; i++)
				if (check->v->absmin[i] > pmove_maxs[i]
				|| check->v->absmax[i] < pmove_mins[i])
					break;
			if (i != 3)
				continue;
			if (pmove.numphysent == MAX_PHYSENTS)
				return;
			pe = &pmove.physents[pmove.numphysent];
			pmove.numphysent++;

			VectorCopy (check->v->origin, pe->origin);
			pe->info = NUM_FOR_EDICT(check);
			if (check->v->solid == SOLID_BSP) {
				if ((unsigned)check->v->modelindex >= MAX_MODELS)
					SV_Error ("AddLinksToPmove: check->v->modelindex >= MAX_MODELS");
				pe->model = sv.models[(int)(check->v->modelindex)];
				if (!pe->model)
					SV_Error ("SOLID_BSP with a non-bsp model");
			}
			else
			{
				pe->model = NULL;
				VectorCopy (check->v->mins, pe->mins);
				VectorCopy (check->v->maxs, pe->maxs);
			}
		}
	}

	// recurse down both sides
	if (node->axis == -1)
		return;

	if ( pmove_maxs[node->axis] > node->dist )
		AddLinksToPmove ( node->children[0] );
	if ( pmove_mins[node->axis] < node->dist )
		AddLinksToPmove ( node->children[1] );
}

int SV_PMTypeForClient (client_t *cl)
{
	if (cl->edict->v->movetype == MOVETYPE_NOCLIP) {
		if (cl->extensions & Z_EXT_PM_TYPE_NEW)
			return PM_SPECTATOR;
		return PM_OLD_SPECTATOR;
	}

	if (cl->edict->v->movetype == MOVETYPE_FLY)
		return PM_FLY;

	if (cl->edict->v->movetype == MOVETYPE_NONE)
		return PM_NONE;

	if (cl->edict->v->movetype == MOVETYPE_LOCK)
		return PM_LOCK;

	if (cl->edict->v->health <= 0)
		return PM_DEAD;

	return PM_NORMAL;
}

/*
===========
SV_PreRunCmd
===========
Done before running a player command.  Clears the touch array
*/
static byte playertouch[(MAX_EDICTS+7)/8];

void SV_PreRunCmd(void)
{
	memset(playertouch, 0, sizeof(playertouch));
}

/*
===========
SV_RunCmd
===========
*/
void SV_RunCmd (usercmd_t *ucmd, qbool inside, qbool second_attempt) //bliP: 24/9
{
	int i, n;
	vec3_t originalvel, offset;
	qbool onground;
	//bliP: 24/9 anti speed ->
	int tmp_time;
	int blocked;

	if (!inside && (int)sv_speedcheck.value
#ifdef USE_PR2
		&& !sv_client->isBot
#endif
	)
	{
		/* AM101 method */
		tmp_time = Q_rint((realtime - sv_client->last_check) * 1000); // ie. Old 'timepassed'
		if (tmp_time)
		{
			if (ucmd->msec > tmp_time)
			{
				tmp_time += sv_client->msecs; // use accumulated msecs
				if (ucmd->msec > tmp_time)
				{ // If still over...
					ucmd->msec = tmp_time;
					sv_client->msecs = 0;
				}
				else
				{
					sv_client->msecs = tmp_time - ucmd->msec; // readjust to leftovers
				}
			}
			else
			{
				// Add up extra msecs
				sv_client->msecs += (tmp_time - ucmd->msec);
			}
		}

		sv_client->last_check = realtime;

		/* Cap it */
		if (sv_client->msecs > 500)
			sv_client->msecs = 500;
		else if (sv_client->msecs < 0)
			sv_client->msecs = 0;
	}
	//<-
	cmd = *ucmd;

	// chop up very long command
	if (cmd.msec > 50)
	{
		int oldmsec;
		oldmsec = ucmd->msec;
		cmd.msec = oldmsec/2;
		SV_RunCmd (&cmd, true, second_attempt);
		cmd.msec = oldmsec/2;
		cmd.impulse = 0;
		SV_RunCmd (&cmd, true, second_attempt);
		return;
	}

	// copy humans' intentions to progs
	sv_player->v->button0 = ucmd->buttons & 1;
	sv_player->v->button2 = (ucmd->buttons & 2) >> 1;
	sv_player->v->button1 = (ucmd->buttons & 4) >> 2;
	if (ucmd->impulse)
		sv_player->v->impulse = ucmd->impulse;
	if (fofs_movement) {
		EdictFieldVector(sv_player, fofs_movement)[0] = ucmd->forwardmove;
		EdictFieldVector(sv_player, fofs_movement)[1] = ucmd->sidemove;
		EdictFieldVector(sv_player, fofs_movement)[2] = ucmd->upmove;
	}
	// bliP: cuff
	if (sv_client->cuff_time > curtime)
		sv_player->v->button0 = sv_player->v->impulse = 0;
	//<-

	// clamp view angles
	ucmd->angles[PITCH] = bound(sv_minpitch.value, ucmd->angles[PITCH], sv_maxpitch.value);
	if (!sv_player->v->fixangle && ! second_attempt)
		VectorCopy (ucmd->angles, sv_player->v->v_angle);

	// model angles
	// show 1/3 the pitch angle and all the roll angle
	if (sv_player->v->health > 0)
	{
		if (!sv_player->v->fixangle)
		{
			sv_player->v->angles[PITCH] = -sv_player->v->v_angle[PITCH]/3;
			sv_player->v->angles[YAW] = sv_player->v->v_angle[YAW];
		}
		sv_player->v->angles[ROLL] = 0;
	}

	sv_frametime = ucmd->msec * 0.001;
	if (sv_frametime > 0.1)
		sv_frametime = 0.1;

	// Don't run think function twice...
	if (!sv_client->spectator && !second_attempt)
	{
		vec3_t	oldvelocity;
		float	old_teleport_time;

		VectorCopy (sv_player->v->velocity, originalvel);
		onground = (int) sv_player->v->flags & FL_ONGROUND;

		VectorCopy (sv_player->v->velocity, oldvelocity);
		old_teleport_time = sv_player->v->teleport_time;

		PR_GLOBAL(frametime) = sv_frametime;
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(sv_player);
		PR_GameClientPreThink(0);

		if (pr_nqprogs)
		{
			sv_player->v->teleport_time = old_teleport_time;
			VectorCopy (oldvelocity, sv_player->v->velocity);
		}

		if ( onground && originalvel[2] < 0 && sv_player->v->velocity[2] == 0 &&
		     originalvel[0] == sv_player->v->velocity[0] &&
		     originalvel[1] == sv_player->v->velocity[1] )
		{
			// don't let KTeams mess with physics
			sv_player->v->velocity[2] = originalvel[2];
		}

		SV_RunThink (sv_player);
	}

	// copy player state to pmove
	VectorSubtract (sv_player->v->mins, player_mins, offset);
	VectorAdd (sv_player->v->origin, offset, pmove.origin);
	VectorCopy (sv_player->v->velocity, pmove.velocity);
	VectorCopy (sv_player->v->v_angle, pmove.angles);
	pmove.waterjumptime = sv_player->v->teleport_time;
	pmove.cmd = *ucmd;
	pmove.pm_type = SV_PMTypeForClient (sv_client);
	pmove.onground = ((int)sv_player->v->flags & FL_ONGROUND) != 0;
	pmove.jump_held = sv_client->jump_held;
	pmove.jump_msec = 0;
	
	// let KTeams "broken ankle" code work
	if (
#if 0
FIXME
	PR_GetEdictFieldValue(sv_player, "brokenankle")
	&&
#endif
	(pmove.velocity[2] == -270) && (pmove.cmd.buttons & BUTTON_JUMP))
		pmove.jump_held = true;

	// build physent list
	pmove.numphysent = 1;
	pmove.physents[0].model = sv.worldmodel;
	AddLinksToPmove ( sv_areanodes );

	// fill in movevars
	movevars.entgravity = sv_client->entgravity;
	movevars.maxspeed = sv_client->maxspeed;
	movevars.bunnyspeedcap = pm_bunnyspeedcap.value;
	movevars.ktjump = pm_ktjump.value;
	movevars.slidefix = ((int)pm_slidefix.value != 0);
	movevars.airstep = ((int)pm_airstep.value != 0);
	movevars.pground = ((int)pm_pground.value != 0);
	movevars.rampjump = ((int)pm_rampjump.value != 0);

	// do the move
	blocked = PM_PlayerMove ();

#ifdef USE_PR2
	// This is a temporary hack for Frogbots, who adjust after bumping into things
	// Better would be to provide a way to simulate a move command, but at least this doesn't require API change
	if (blocked && !second_attempt && sv_client->isBot && sv_player->v->blocked)
	{
		pr_global_struct->self = EDICT_TO_PROG(sv_player);

		// Don't store in the bot's entity as we will run this again
		VectorSubtract (pmove.origin, offset, pr_global_struct->trace_endpos);
		VectorCopy (pmove.velocity, pr_global_struct->trace_plane_normal);
		if (pmove.onground)
		{
			pr_global_struct->trace_allsolid = (int) sv_player->v->flags | FL_ONGROUND;
			pr_global_struct->trace_ent = EDICT_TO_PROG(EDICT_NUM(pmove.physents[pmove.groundent].info));
		} else {
			pr_global_struct->trace_allsolid = (int) sv_player->v->flags & ~FL_ONGROUND;
		}

		// Give the mod a chance to replace the command
		PR_EdictBlocked (sv_player->v->blocked);

		// Run the command again
		SV_RunCmd (ucmd, false, true);
		return;
	}
#endif

	// get player state back out of pmove
	sv_client->jump_held = pmove.jump_held;
	sv_player->v->teleport_time = pmove.waterjumptime;
	if (pr_nqprogs)
		sv_player->v->flags = ((int)sv_player->v->flags & ~FL_WATERJUMP) | (pmove.waterjumptime ? FL_WATERJUMP : 0);
	sv_player->v->waterlevel = pmove.waterlevel;
	sv_player->v->watertype = pmove.watertype;
	if (pmove.onground)
	{
		sv_player->v->flags = (int) sv_player->v->flags | FL_ONGROUND;
		sv_player->v->groundentity = EDICT_TO_PROG(EDICT_NUM(pmove.physents[pmove.groundent].info));
	} else {
		sv_player->v->flags = (int) sv_player->v->flags & ~FL_ONGROUND;
	}

	VectorSubtract (pmove.origin, offset, sv_player->v->origin);
	VectorCopy (pmove.velocity, sv_player->v->velocity);

	VectorCopy (pmove.angles, sv_player->v->v_angle);

	if (sv_player->v->solid != SOLID_NOT)
	{
		// link into place and touch triggers
		SV_LinkEdict (sv_player, true);

		// touch other objects
		for (i=0 ; i<pmove.numtouch ; i++)
		{
			edict_t *ent;
			n = pmove.physents[pmove.touchindex[i]].info;
			ent = EDICT_NUM(n);
			if (!ent->v->touch || (playertouch[n/8]&(1<<(n%8))))
				continue;
			pr_global_struct->self = EDICT_TO_PROG(ent);
			pr_global_struct->other = EDICT_TO_PROG(sv_player);
			PR_EdictTouch (ent->v->touch);
			playertouch[n/8] |= 1 << (n%8);
		}
	}
}

#ifdef MVD_PEXT1_SERVERSIDEWEAPON
typedef struct ssw_info_s {
	int impulse_set;
	int hide_weapon;
	int best_weapon;
	qbool hiding;
	qbool firing;
} ssw_info_t;

// Ranks best weapon for player
void SV_ServerSideWeaponRank(client_t* client, int* best_weapon, int* best_impulse, int* hide_weapon, int* hide_impulse)
{
	entvars_t* ent = client->edict->v;
	int i;
	int items = (int)ent->items;
	int weapon = (int)ent->weapon;
	int shells = (int)ent->ammo_shells;
	int nails = (int)ent->ammo_nails;
	int rockets = (int)ent->ammo_rockets;
	int cells = (int)ent->ammo_cells;

	if (client->weaponswitch_hide == 2 && shells > 0) {
		*hide_weapon = IT_SHOTGUN;
		*hide_impulse = 2;
	}
	else if (client->weaponswitch_hide == 1) {
		*hide_weapon = IT_AXE;
		*hide_impulse = 1;
	}
	else {
		*hide_weapon = 0;
		*hide_impulse = 0;
	}

	// Default to staying on the current weapon, regardless of ammo
	*best_weapon = weapon;
	*best_impulse = 0;

	for (i = 0; i < sizeof(client->weaponswitch_priority) / sizeof(client->weaponswitch_priority[0]); ++i) {
		switch (client->weaponswitch_priority[i]) {
			case 0:
				// end of list
				return;
			case 1:
				if (items & IT_AXE) {
					*best_weapon = IT_AXE;
					*best_impulse = 1;
					return;
				}
				break;
			case 2:
				if ((items & IT_SHOTGUN) && shells > 0) {
					*best_weapon = IT_SHOTGUN;
					*best_impulse = 2;
					return;
				}
				break;
			case 3:
				if ((items & IT_SUPER_SHOTGUN) && shells > 1) {
					*best_weapon = IT_SUPER_SHOTGUN;
					*best_impulse = 3;
					return;
				}
				break;
			case 4:
				if ((items & IT_NAILGUN) && nails > 0) {
					*best_weapon = IT_NAILGUN;
					*best_impulse = 4;
					return;
				}
				break;
			case 5:
				if ((items & IT_SUPER_NAILGUN) && nails > 1) {
					*best_weapon = IT_SUPER_NAILGUN;
					*best_impulse = 5;
					return;
				}
				break;
			case 6:
				if ((items & IT_GRENADE_LAUNCHER) && rockets > 0) {
					*best_weapon = IT_GRENADE_LAUNCHER;
					*best_impulse = 6;
					return;
				}
				break;
			case 7:
				if ((items & IT_ROCKET_LAUNCHER) && rockets > 0) {
					*best_weapon = IT_ROCKET_LAUNCHER;
					*best_impulse = 7;
					return;
				}
				break;
			case 8:
				if ((items & IT_LIGHTNING) && cells > 0) {
					*best_weapon = IT_LIGHTNING;
					*best_impulse = 8;
					return;
				}
				break;
		}
	}

	return;
}

static void SV_NotifyUserOfBestWeapon(client_t* sv_client, int new_impulse)
{
	char stuffcmd_buffer[64];

	strlcpy(stuffcmd_buffer, va("//mvdsv_ssw %d %d\n", sv_client->weaponswitch_sequence_set, new_impulse), sizeof(stuffcmd_buffer));

	ClientReliableWrite_Begin(sv_client, svc_stufftext, 2 + strlen(stuffcmd_buffer));
	ClientReliableWrite_String(sv_client, stuffcmd_buffer);
}

static void SV_ExecuteServerSideWeaponForgetOrder(client_t* sv_client, int best_impulse, int hide_impulse)
{
	char new_wrank[16] = { 0 };

	SV_DebugServerSideWeaponScript(sv_client, best_impulse);
	SV_NotifyUserOfBestWeapon(sv_client, best_impulse);

	// Over-write the list sent with the result
	{
		if (Info_Get(&sv_client->_userinfo_ctx_, "dev")[0] == '1') {
			SV_ClientPrintf(sv_client, PRINT_HIGH, "Best: %d, forgetorder enabled\n", best_impulse);
		}
	}
	sv_client->weaponswitch_priority[0] = best_impulse;
	sv_client->weaponswitch_priority[1] = (hide_impulse == 1 || best_impulse == 2 ? 1 : 2);
	sv_client->weaponswitch_priority[2] = (sv_client->weaponswitch_priority[1] != 1 ? 1 : 0);
	sv_client->weaponswitch_priority[3] = 0;

	new_wrank[0] = '0' + best_impulse;
	if (hide_impulse) {
		new_wrank[1] = '0' + hide_impulse;
		if (hide_impulse == 2) {
			new_wrank[2] = '1';
		}
	}
	else {
		new_wrank[1] = '2';
		new_wrank[2] = '1';
	}

	SV_UserSetWeaponRank(sv_client, new_wrank);
}

static void SV_ExecuteServerSideWeaponHideOnDeath(client_t* sv_client, int hide_impulse, int hide_weapon)
{
	char new_wrank[16] = { 0 };

	if (sv_client->edict->v->health > 0.0f || !sv_client->weaponswitch_hide_on_death) {
		return;
	}

	// might not have general hiding enabled...
	hide_impulse = (hide_impulse == 1 ? 1 : 2);
	hide_weapon = (hide_impulse == 1 ? IT_AXE : IT_SHOTGUN);
	new_wrank[0] = (hide_impulse == 1 ? '1' : '2');
	new_wrank[1] = (hide_impulse == 1 ? '0' : '1');
	sv_client->weaponswitch_priority[0] = (hide_impulse == 1 ? 1 : 2);
	sv_client->weaponswitch_priority[1] = (hide_impulse == 1 ? 1 : 2);
	sv_client->weaponswitch_priority[2] = 0;

	SV_DebugServerSideWeaponScript(sv_client, hide_impulse);
	SV_NotifyUserOfBestWeapon(sv_client, hide_impulse);
	SV_UserSetWeaponRank(sv_client, new_wrank);

	if (Info_Get(&sv_client->_userinfo_ctx_, "dev")[0] == '1' && sv_client->edict->v->weapon != hide_weapon) {
		SV_ClientPrintf(sv_client, PRINT_HIGH, "Hiding on death: %d\n", hide_impulse);
	}
}

static void SV_ServerSideWeaponLogic_PrePostThink(client_t* sv_client, ssw_info_t* ssw)
{
	entvars_t* ent = sv_client->edict->v;
	qbool dev_trace = (Info_Get(&sv_client->_userinfo_ctx_, "dev")[0] == '1');

	ssw->firing = (ent->button0 != 0);

	if ((sv_client->mvdprotocolextensions1 & MVD_PEXT1_SERVERSIDEWEAPON) && sv_client->weaponswitch_enabled) {
		int best_impulse, hide_impulse;
		qbool switch_to_best_weapon = false;
		int mode = sv_client->weaponswitch_mode;

		// modes: 0 immediately choose best, 1 preselect (wait until fire), 2 immediate if firing
		// mode 2 = "preselect(1) when not holding +attack, else immediate(0)"
		if (mode == 2) {
			mode = (ssw->firing ? 0 : 1);
		}
		switch_to_best_weapon = sv_client->weaponswitch_pending && (mode == 0 || ssw->firing) && (ent->health >= 1.0f);

		SV_ServerSideWeaponRank(sv_client, &ssw->best_weapon, &best_impulse, &ssw->hide_weapon, &hide_impulse);

		ssw->hiding = (sv_client->weaponswitch_wasfiring && !ssw->firing && hide_impulse);
		sv_client->weaponswitch_wasfiring |= ssw->firing;

		if (switch_to_best_weapon && sv_client->weaponswitch_forgetorder) {
			SV_ExecuteServerSideWeaponForgetOrder(sv_client, best_impulse, hide_impulse);
		}
		SV_ExecuteServerSideWeaponHideOnDeath(sv_client, hide_impulse, ssw->hide_weapon);

		if (!ent->impulse) {
			if (switch_to_best_weapon) {
				if (best_impulse && ent->weapon != ssw->best_weapon) {
					SV_DebugServerSideWeaponScript(sv_client, best_impulse);

					if (dev_trace) {
						SV_ClientPrintf(sv_client, PRINT_HIGH, "Switching to best weapon: %d\n", best_impulse);
					}

					ent->impulse = best_impulse;
					ssw->impulse_set = 2;
				}
				else {
					sv_client->weaponswitch_pending = false;
				}
			}
			else if (ssw->hiding) {
				if (ent->weapon != ssw->hide_weapon) {
					if (dev_trace) {
						SV_ClientPrintf(sv_client, PRINT_HIGH, "Hiding: %d\n", hide_impulse);
					}
					ent->impulse = hide_impulse;
					ssw->impulse_set = 1;
				}
				else {
					sv_client->weaponswitch_pending = false;
				}
			}
		}
		else {
			if (dev_trace) {
				SV_ClientPrintf(sv_client, PRINT_HIGH, "Non-wp impulse: %f\n", ent->impulse);
			}
		}
		sv_client->weaponswitch_pending &= (ent->health >= 1.0f);
	}
}

static void SV_ServerSideWeaponLogic_PostPostThink(client_t* sv_client, ssw_info_t* ssw)
{
	entvars_t* ent = sv_client->edict->v;
	qbool dev_trace = (Info_Get(&sv_client->_userinfo_ctx_, "dev")[0] == '1');

	if (ssw->impulse_set) {
		qbool hide_failed = (ssw->impulse_set == 1 && ent->weapon != ssw->hide_weapon);
		qbool pickbest_failed = (ssw->impulse_set == 2 && ent->weapon != ssw->best_weapon);
		qbool failure = (hide_failed || pickbest_failed);

		ent->impulse = 0;

		if (dev_trace) {
			if (failure) {
				SV_ClientPrintf(sv_client, PRINT_HIGH, "... %s failed, will try again\n", ssw->impulse_set == 1 ? "hide" : "pickbest");
			}
			else {
				SV_ClientPrintf(sv_client, PRINT_HIGH, "... %s successful, stopping\n", ssw->impulse_set == 1 ? "hide" : "pickbest");
			}
		}

		sv_client->weaponswitch_pending &= failure;
	}
	if (ssw->hiding && ent->weapon == ssw->hide_weapon) {
		if (dev_trace) {
			SV_ClientPrintf(sv_client, PRINT_HIGH, "Hide successful\n");
		}
		sv_client->weaponswitch_wasfiring = false;
	}
	else if (!(ssw->hiding || ssw->firing)) {
		if (sv_client->weaponswitch_wasfiring && dev_trace) {
			SV_ClientPrintf(sv_client, PRINT_HIGH, "No longer firing...\n");
		}
		sv_client->weaponswitch_wasfiring = false;
	}
}
#endif

/*
===========
SV_PostRunCmd
===========
Done after running a player command.
*/
void SV_PostRunCmd(void)
{
	vec3_t originalvel;
	qbool onground;
	// run post-think
#ifdef MVD_PEXT1_SERVERSIDEWEAPON
	ssw_info_t ssw = { 0 };
#endif

	if (!sv_client->spectator)
	{
#ifdef MVD_PEXT1_SERVERSIDEWEAPON
		SV_ServerSideWeaponLogic_PrePostThink(sv_client, &ssw);
#endif

		onground = (int) sv_player->v->flags & FL_ONGROUND;
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(sv_player);
		VectorCopy (sv_player->v->velocity, originalvel);
		PR_GameClientPostThink(0);

		if ( onground && originalvel[2] < 0 && sv_player->v->velocity[2] == 0
		&& originalvel[0] == sv_player->v->velocity[0]
		&& originalvel[1] == sv_player->v->velocity[1] ) {
			// don't let KTeams mess with physics
			sv_player->v->velocity[2] = originalvel[2];
		}

		if (pr_nqprogs)
			VectorCopy (originalvel, sv_player->v->velocity);

		if (pr_nqprogs)
			SV_RunNQNewmis ();
		else
			SV_RunNewmis ();

#ifdef MVD_PEXT1_SERVERSIDEWEAPON
		SV_ServerSideWeaponLogic_PostPostThink(sv_client, &ssw);
#endif
	}
	else
	{
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(sv_player);
		PR_GameClientPostThink(1);
	}
}

/*
SV_UserSetWeaponRank
Sets wrank userinfo for mods to pick best weapon based on user's preferences
*/
#ifdef MVD_PEXT1_SERVERSIDEWEAPON
static void SV_UserSetWeaponRank(client_t* cl, const char* new_wrank)
{
	char old_wrank[128] = { 0 };
	strlcpy(old_wrank, Info_Get(&cl->_userinfo_ctx_, "w_rank"), sizeof(old_wrank));
	if (strcmp(old_wrank, new_wrank)) {
		Info_Set(&cl->_userinfo_ctx_, "w_rank", new_wrank);
		ProcessUserInfoChange(cl, "w_rank", old_wrank);

		if (Info_Get(&cl->_userinfo_ctx_, "dev")[0] == '1') {
			SV_ClientPrintf(cl, PRINT_HIGH, "Setting new w_rank: %s\n", new_wrank);
		}
	}
}
#endif

// SV_RotateCmd
// Rotates client command so a high-ping player can better control direction as they exit teleporters on high-ping
void SV_RotateCmd(client_t* cl, usercmd_t* cmd_)
{
	if (cl->lastteleport_teleport) {
		static vec3_t up = { 0, 0, 1 };
		vec3_t direction = { cmd_->sidemove, cmd_->forwardmove, 0 };
		vec3_t result;

		RotatePointAroundVector(result, up, direction, cl->lastteleport_teleportyaw);

		cmd_->sidemove = result[0];
		cmd_->forwardmove = result[1];
	}
	else {
		cmd_->angles[YAW] = (cl->edict)->v->angles[YAW];
	}
}

/*
===================
SV_ExecuteClientMove

Run one or more client move commands (more than one if some
packets were dropped)
===================
*/
static void SV_ExecuteClientMove(client_t* cl, usercmd_t oldest, usercmd_t oldcmd, usercmd_t newcmd)
{
	int net_drop;
	int playernum = cl - svs.clients;

	if (sv.paused) {
		return;
	}

	SV_PreRunCmd();

	net_drop = cl->netchan.dropped;
	if (net_drop < 20) {
		while (net_drop > 2) {
			SV_DebugClientCommand(playernum, &cl->lastcmd, net_drop);
			SV_RunCmd(&cl->lastcmd, false, false);
			net_drop--;
		}
	}
	if (net_drop > 1) {
		SV_DebugClientCommand(playernum, &oldest, 2);
		SV_RunCmd(&oldest, false, false);
	}
	if (net_drop > 0) {
		SV_DebugClientCommand(playernum, &oldcmd, 1);
		SV_RunCmd(&oldcmd, false, false);
	}
	SV_DebugClientCommand(playernum, &newcmd, 0);
#ifdef MVD_PEXT1_SERVERSIDEWEAPON
	{
		// This is necessary to interrupt LG/SNG where the firing takes place inside animation frames
		if (sv_client->weaponswitch_enabled && sv_client->weaponswitch_pending && !sv_client->edict->v->impulse) {
			sv_client->edict->v->impulse = 255;
			SV_RunCmd(&newcmd, false, false);
			sv_client->edict->v->impulse = 0;
		}
		else {
			SV_RunCmd(&newcmd, false, false);
		}
	}
#else
	SV_RunCmd(&newcmd, false, false);
#endif

	SV_PostRunCmd();
}

#ifdef MVD_PEXT1_DEBUG_ANTILAG
/*
SV_DebugWriteServerAntilagPositions

Writes the position of clients, as rewound by antilag
*/
static void SV_DebugWriteServerAntilagPositions(client_t* cl, int present)
{
	mvdhidden_block_header_t header;
	mvdhidden_antilag_position_header_t antilag_header;
	int i;
	float target_time = cl->laggedents_time;

	header.type_id = mvdhidden_antilag_position;
	header.length = sizeof_mvdhidden_antilag_position_header_t + present * sizeof_mvdhidden_antilag_position_t;

	antilag_header.incoming_seq = LittleLong(cl->netchan.incoming_sequence);
	antilag_header.playernum = cl - svs.clients;
	antilag_header.players = present;
	antilag_header.server_time = LittleFloat(sv.time);
	antilag_header.target_time = LittleFloat(target_time);

	if (MVDWrite_HiddenBlockBegin(sizeof_mvdhidden_block_header_t_range0 + header.length)) {
		header.length = LittleLong(header.length);
		MVD_SZ_Write(&header.length, sizeof(header.length));
		MVD_SZ_Write(&header.type_id, sizeof(header.type_id));
		MVD_SZ_Write(&antilag_header.playernum, sizeof(antilag_header.playernum));
		MVD_SZ_Write(&antilag_header.players, sizeof(antilag_header.players));
		MVD_SZ_Write(&antilag_header.incoming_seq, sizeof(antilag_header.incoming_seq));
		MVD_SZ_Write(&antilag_header.server_time, sizeof(antilag_header.server_time));
		MVD_SZ_Write(&antilag_header.target_time, sizeof(antilag_header.target_time));
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (cl->laggedents[i].present) {
				mvdhidden_antilag_position_t pos = { 0 };
				int j;

				pos.playernum = i;
#ifdef MVD_PEXT1_DEBUG
				if (debug_info.antilag_clients[i].present) {
					pos.playernum |= MVD_PEXT1_ANTILAG_CLIENTPOS;
					pos.msec = debug_info.antilag_clients[i].msec;
					pos.predmodel = debug_info.antilag_clients[i].model;
					VectorCopy(debug_info.antilag_clients[i].pos, pos.clientpos);
				}
#endif
				MVD_SZ_Write(&pos.playernum, sizeof(pos.playernum));
				for (j = 0; j < 3; ++j) {
					pos.pos[j] = LittleFloat(cl->laggedents[i].laggedpos[j]);
					MVD_SZ_Write(&pos.pos[j], sizeof(pos.pos[j]));
				}
				MVD_SZ_Write(&pos.msec, sizeof(pos.msec));
				MVD_SZ_Write(&pos.predmodel, sizeof(pos.predmodel));
				for (j = 0; j < 3; ++j) {
					pos.clientpos[j] = LittleFloat(pos.clientpos[j]);
					MVD_SZ_Write(&pos.clientpos[j], sizeof(pos.clientpos[j]));
				}
			}
		}
	}
}
#endif // MVD_PEXT1_DEBUG_ANTILAG

#ifdef MVD_PEXT1_DEBUG_WEAPON
static void SV_DebugWriteWeaponScript(byte playernum, qbool server_side, int items, byte shells, byte nails, byte rockets, byte cells, byte choice, const char* weaponlist)
{
	if (sv_debug_weapons.value >= 1) {
		mvdhidden_block_header_t header;

		// Write out immediately
		header.type_id = (server_side ? mvdhidden_usercmd_weapons_ss : mvdhidden_usercmd_weapons);
		header.length = LittleLong(10 + strlen(weaponlist) + 1);

		if (MVDWrite_HiddenBlockBegin(sizeof_mvdhidden_block_header_t_range0 + header.length)) {
			MVD_SZ_Write(&header.length, sizeof(header.length));
			MVD_SZ_Write(&header.type_id, sizeof(header.type_id));
			MVD_SZ_Write(&playernum, sizeof(playernum));
			MVD_SZ_Write(&items, sizeof(items));
			MVD_SZ_Write(&shells, sizeof(shells));
			MVD_SZ_Write(&nails, sizeof(nails));
			MVD_SZ_Write(&rockets, sizeof(rockets));
			MVD_SZ_Write(&cells, sizeof(cells));
			MVD_SZ_Write(&choice, sizeof(choice));
			MVD_SZ_Write(weaponlist, strlen(weaponlist) + 1);
		}
	}
}

static void SV_DebugClientSideWeaponScript(client_t* cl)
{
	byte playernum = cl - svs.clients;
	int items = LittleLong(MSG_ReadLong());
	byte shells = MSG_ReadByte();
	byte nails = MSG_ReadByte();
	byte rockets = MSG_ReadByte();
	byte cells = MSG_ReadByte();
	byte choice = MSG_ReadByte();
	const char* weaponlist = MSG_ReadString();

	SV_DebugWriteWeaponScript(playernum, false, items, shells, nails, rockets, cells, choice, weaponlist);
}

#ifdef MVD_PEXT1_SERVERSIDEWEAPON
static void SV_DebugServerSideWeaponInstruction(client_t* cl)
{
	if (sv_debug_weapons.value >= 1) {
		mvdhidden_block_header_t header;

		// Write out immediately
		header.type_id = mvdhidden_usercmd_weapon_instruction;
		header.length = sizeof_mvdhidden_usercmd_weapon_instruction;

		if (MVDWrite_HiddenBlockBegin(sizeof_mvdhidden_block_header_t_range0 + header.length)) {
			byte playernum = cl - svs.clients;
			byte flags = 0;
			int sequence_set = cl->weaponswitch_sequence_set;
			int mode = cl->weaponswitch_mode;
			byte weaponlist[10] = { 0 };

			flags |= (cl->weaponswitch_pending ? MVDHIDDEN_SSWEAPON_PENDING : 0);
			flags |= (cl->weaponswitch_hide == 1 ? MVDHIDDEN_SSWEAPON_HIDE_AXE : (cl->weaponswitch_hide == 2 ? MVDHIDDEN_SSWEAPON_HIDE_SG : 0));
			flags |= (cl->weaponswitch_hide_on_death ? MVDHIDDEN_SSWEAPON_HIDEONDEATH : 0);
			flags |= (cl->weaponswitch_wasfiring ? MVDHIDDEN_SSWEAPON_WASFIRING : 0);
			flags |= (cl->weaponswitch_enabled ? MVDHIDDEN_SSWEAPON_ENABLED : 0);
			flags |= (cl->weaponswitch_forgetorder ? MVDHIDDEN_SSWEAPON_FORGETORDER : 0);

			memcpy(weaponlist, cl->weaponswitch_priority, min(10, sizeof(cl->weaponswitch_priority)));

			MVD_SZ_Write(&header.length, sizeof(header.length));
			MVD_SZ_Write(&header.type_id, sizeof(header.type_id));
			MVD_SZ_Write(&playernum, sizeof(playernum));
			MVD_SZ_Write(&flags, sizeof(flags));
			MVD_SZ_Write(&sequence_set, sizeof(sequence_set));
			MVD_SZ_Write(&mode, sizeof(mode));
			MVD_SZ_Write(weaponlist, sizeof(weaponlist));
		}
	}
}

static void SV_DebugServerSideWeaponScript(client_t* cl, int best_impulse)
{
	if (sv_debug_weapons.value >= 1) {
		char old_wrank[128] = { 0 };
		char encoded[128] = { 0 };
		char* w;
		char* o;
		entvars_t* ent = cl->edict->v;

		strlcpy(old_wrank, Info_Get(&cl->_userinfo_ctx_, "w_rank"), sizeof(old_wrank));

		w = old_wrank;
		o = encoded;
		while (*w) {
			*o = (*w - '0');
			++w;
			++o;
		}

		SV_DebugWriteWeaponScript(cl - svs.clients, true, ent->items, ent->ammo_shells, ent->ammo_nails, ent->ammo_rockets, ent->ammo_cells, best_impulse, encoded);
	}
}
#endif

#endif

/*
===================
SV_ExecuteClientMessage
 
The current net_message is parsed for the given client
===================
*/
void SV_ExecuteClientMessage (client_t *cl)
{
	int             c, i;
	char            *s;
	usercmd_t       oldest, oldcmd, newcmd;
	client_frame_t  *frame;
	vec3_t          o;
	qbool           move_issued = false; //only allow one move command
	int             checksumIndex;
	byte            checksum, calculatedChecksum;
	int             seq_hash;

#ifdef MVD_PEXT1_DEBUG
	int             antilag_players_present = 0;
#endif

	if (!Netchan_Process(&cl->netchan))
		return;
//	if (cl->state == cs_zombie) // FIXME
//		return; // FIXME

	// this is a valid, sequenced packet, so process it
	svs.stats.packets++;
	cl->send_message = true; // reply at end of frame
#ifdef FTE_PEXT_CHUNKEDDOWNLOADS
	cl->download_chunks_perframe = 0;
#endif

	// calc ping time
	frame = &cl->frames[cl->netchan.incoming_acknowledged & UPDATE_MASK];
	frame->ping_time = curtime - frame->senttime;

	// update delay based on ping and sv_minping
	if (!cl->spectator && !sv.paused)
	{
		if (frame->ping_time * 1000 > sv_minping.value + 1)
			cl->delay -= 0.001;
		else if (frame->ping_time * 1000 < sv_minping.value)
			cl->delay += 0.001;

		cl->delay = bound(0, cl->delay, 1);
	}

	cl->laggedents_count = 0; // init at least this
	cl->laggedents_frac = 1; // sv_antilag_frac.value;

	if (sv_antilag.value)
	{
//#pragma msg("FIXME: make antilag optionally support non-player ents too")

#define MAX_PREDICTION 0.02
#define MAX_EXTRAPOLATE 0.02

		double target_time, max_physfps = sv_maxfps.value;

		if (max_physfps < 20 || max_physfps > 1000)
			max_physfps = 77.0;

		if (sv_antilag_no_pred.value) {
			target_time = frame->sv_time;
		} else {
			// try to figure out what time client is currently predicting, basically this is just 6.5ms with 13ms ping and 13.5ms with higher
			// might be off with different max_physfps values
			target_time = min(frame->sv_time + (frame->ping_time < MAX_PREDICTION ? 1/max_physfps : MAX_PREDICTION), sv.time);
		}

		for (i = 0; i < MAX_CLIENTS; i++)
		{
			client_t *target_cl = &svs.clients[i];
			antilag_position_t *base, *interpolate = NULL;
			double factor;
			int j;

			// don't hit dead players
			if (target_cl->state != cs_spawned || target_cl->antilag_position_next == 0 || (target_cl->spectator == 0 && target_cl->edict->v->health <= 0)) {
				cl->laggedents[i].present = false;
				continue;
			}

			cl->laggedents[i].present = true;
			++antilag_players_present;

			// target player's movement commands are late, extrapolate his position based on velocity
			if (target_time > target_cl->localtime) {
				VectorMA(target_cl->edict->v->origin, min(target_time - target_cl->localtime, MAX_EXTRAPOLATE), target_cl->edict->v->velocity, cl->laggedents[i].laggedpos);
				continue;
			}

			// we have only one antilagged position, use that
			if (target_cl->antilag_position_next == 1) {
				VectorCopy(target_cl->antilag_positions[0].origin, cl->laggedents[i].laggedpos);
				continue;
			}

			// find the position before target time (base) and the one after that (interpolate)
			for (j = target_cl->antilag_position_next - 2; j > target_cl->antilag_position_next - 1 - MAX_ANTILAG_POSITIONS && j >= 0; j--) {
				if (target_cl->antilag_positions[j % MAX_ANTILAG_POSITIONS].localtime < target_time)
					break;
			}

			base = &target_cl->antilag_positions[j % MAX_ANTILAG_POSITIONS];
			interpolate = &target_cl->antilag_positions[(j + 1) % MAX_ANTILAG_POSITIONS];

			// we have two positions, just interpolate between them
			factor = (target_time - base->localtime) / (interpolate->localtime - base->localtime);
			VectorInterpolate(base->origin, factor, interpolate->origin, cl->laggedents[i].laggedpos);
		}

		cl->laggedents_count = MAX_CLIENTS; // FIXME: well, FTE do it a bit different way...
		cl->laggedents_time = target_time;
	}

	// make sure the reply sequence number matches the incoming
	// sequence number
	if (cl->netchan.incoming_sequence >= cl->netchan.outgoing_sequence)
		cl->netchan.outgoing_sequence = cl->netchan.incoming_sequence;
	else
		cl->send_message = false;	// don't reply, sequences have slipped

	// save time for ping calculations
	cl->frames[cl->netchan.outgoing_sequence & UPDATE_MASK].senttime = curtime;
	cl->frames[cl->netchan.outgoing_sequence & UPDATE_MASK].ping_time = -1;

	sv_client = cl;
	sv_player = sv_client->edict;

	seq_hash = cl->netchan.incoming_sequence;

	// mark time so clients will know how much to predict
	// other players
	cl->localtime = sv.time;
	cl->delta_sequence = -1;	// no delta unless requested
#ifdef MVD_PEXT1_DEBUG
	memset(&debug_info, 0, sizeof(debug_info));
#endif
	while (1)
	{
		if (msg_badread)
		{
			Con_Printf ("SV_ReadClientMessage: badread\n");
			SV_DropClient (cl);
			return;
		}

		c = MSG_ReadByte ();
		if (c == -1)
			break;

		switch (c)
		{
		default:
			Con_Printf ("SV_ReadClientMessage: unknown command char\n");
			SV_DropClient (cl);
			return;

		case clc_nop:
			break;

		case clc_delta:
			cl->delta_sequence = MSG_ReadByte ();
			break;

#ifdef MVD_PEXT1_DEBUG
		case clc_mvd_debug:
		{
			byte type = MSG_ReadByte();
			if (type == clc_mvd_debug_type_antilag) {
				int players = MSG_ReadByte();

				for (i = 0; i < players; ++i) {
					int num = MSG_ReadByte();
					int msec = MSG_ReadByte();
					int model = MSG_ReadByte();
					float x = LittleFloat(MSG_ReadFloat());
					float y = LittleFloat(MSG_ReadFloat());
					float z = LittleFloat(MSG_ReadFloat());

					if (num >= 0 && num < MAX_CLIENTS) {
						debug_info.antilag_clients[num].present = true;
						debug_info.antilag_clients[num].model = model;
						debug_info.antilag_clients[num].msec = msec;
						VectorSet(debug_info.antilag_clients[num].pos, x, y, z);
					}
				}
			}
			else if (type == clc_mvd_debug_type_weapon) {
				SV_DebugClientSideWeaponScript(cl);
			}
			else {
				Con_Printf("SV_ReadClientMessage: unknown debug message type %d\n", type);
				SV_DropClient(cl);
			}
			break;
		}
#endif

#ifdef MVD_PEXT1_SERVERSIDEWEAPON
		case clc_mvd_weapon:
		{
			if (!SV_ClientExtensionWeaponSwitch(cl)) {
				Con_Printf("SV_ClientExtensionWeaponSwitch: corrupt data string\n");
				SV_DropClient(cl);
				return;
			}
			break;
		}
#endif

		case clc_move:
			if (move_issued)
				return;		// someone is trying to cheat...

			move_issued = true;

			checksumIndex = MSG_GetReadCount();
			checksum = (byte)MSG_ReadByte ();

			// read loss percentage
			//bliP: file percent ->
			cl->lossage = MSG_ReadByte();
			if (cl->file_percent)
				cl->lossage = cl->file_percent;

			/*if (cl->state < cs_spawned && cl->download != NULL) {
				if (cl->downloadsize)
					cl->lossage = cl->downloadcount*100/cl->downloadsize;
				else
					cl->lossage = 100;
			}*/
			//<-

#ifndef SERVERONLY
			MSG_ReadDeltaUsercmd (&nullcmd, &oldest, PROTOCOL_VERSION);
			MSG_ReadDeltaUsercmd (&oldest, &oldcmd, PROTOCOL_VERSION);
			MSG_ReadDeltaUsercmd (&oldcmd, &newcmd, PROTOCOL_VERSION);
#else
			MSG_ReadDeltaUsercmd (&nullcmd, &oldest);
			MSG_ReadDeltaUsercmd (&oldest, &oldcmd);
			MSG_ReadDeltaUsercmd (&oldcmd, &newcmd);
#endif

			if ( cl->state != cs_spawned )
				break;

#ifdef CHAT_ICON_EXPERIMENTAL
			s = Info_Get(&cl->_userinfoshort_ctx_, "chat");
			if ( s[0] ) {
// allow movement while in console
//				newcmd.forwardmove = newcmd.sidemove = newcmd.upmove = 0;
				newcmd.buttons &= BUTTON_JUMP; // only jump button allowed while in console

// somemods uses impulses for commands, so let them use
//				newcmd.impulse = 0;
			}
#endif

			// if the checksum fails, ignore the rest of the packet
			calculatedChecksum = COM_BlockSequenceCRCByte(
				net_message.data + checksumIndex + 1,
				MSG_GetReadCount() - checksumIndex - 1,
				seq_hash
			);

			if (calculatedChecksum != checksum)
			{
				Con_DPrintf ("Failed command checksum for %s(%d) (%d != %d)\n", cl->name, cl->netchan.incoming_sequence, checksum, calculatedChecksum);
				return;
			}

#ifdef MVD_PEXT1_HIGHLAGTELEPORT
			if (cl->mvdprotocolextensions1 & MVD_PEXT1_HIGHLAGTELEPORT) {
				if (cl->lastteleport_outgoingseq && cl->netchan.incoming_acknowledged < cl->lastteleport_outgoingseq) {
					if (cl->netchan.incoming_sequence - 2 > cl->lastteleport_incomingseq) {
						SV_RotateCmd(cl, &oldest);
					}
					if (cl->netchan.incoming_sequence - 1 > cl->lastteleport_incomingseq) {
						SV_RotateCmd(cl, &oldcmd);
					}
					SV_RotateCmd(cl, &newcmd);
				}
				else {
					cl->lastteleport_outgoingseq = 0;
				}
			}
#endif

			SV_ExecuteClientMove(cl, oldest, oldcmd, newcmd);

			cl->lastcmd = newcmd;
			cl->lastcmd.buttons = 0; // avoid multiple fires on lag

			if (sv_antilag.value) {
				if (cl->antilag_position_next == 0 || cl->antilag_positions[(cl->antilag_position_next - 1) % MAX_ANTILAG_POSITIONS].localtime < cl->localtime) {
					cl->antilag_positions[cl->antilag_position_next % MAX_ANTILAG_POSITIONS].localtime = cl->localtime;
					VectorCopy(cl->edict->v->origin, cl->antilag_positions[cl->antilag_position_next % MAX_ANTILAG_POSITIONS].origin);
					cl->antilag_position_next++;
				}
			} else {
				cl->antilag_position_next = 0;
			}
			break;

		case clc_stringcmd:
			s = MSG_ReadString ();
			s[1023] = 0;
			SV_ExecuteUserCommand (s);
			break;

		case clc_tmove:
			o[0] = MSG_ReadCoord();
			o[1] = MSG_ReadCoord();
			o[2] = MSG_ReadCoord();
			// only allowed by spectators
			if (sv_client->spectator)
			{
				VectorCopy(o, sv_player->v->origin);
				SV_LinkEdict(sv_player, false);
			}
			break;

		case clc_upload:
			SV_NextUpload();
			break;

#ifdef FTE_PEXT2_VOICECHAT
		case clc_voicechat:
			SV_VoiceReadPacket();
			break;
#endif
		}
	}

#ifdef MVD_PEXT1_DEBUG_ANTILAG
	if (antilag_players_present && sv_debug_antilag.value) {
		SV_DebugWriteServerAntilagPositions(cl, antilag_players_present);
	}
#endif
}

#ifdef MVD_PEXT1_SERVERSIDEWEAPON
static qbool SV_ClientExtensionWeaponSwitch(client_t* cl)
{
	int flags = MSG_ReadByte();
	int weap = 0, w = 0;
	qbool write = true;
	int sequence_set = cl->netchan.incoming_sequence;
	int weapon_hide_selection = 0;
	byte new_selections[MAX_WEAPONSWITCH_OPTIONS] = { 0 };
	char new_wrank[MAX_WEAPONSWITCH_OPTIONS + 1] = { 0 };
	int wrank_index = 0;

	if (flags == -1) {
		return false;
	}

	// This might be a duplicate that should be ignored
	if (flags & clc_mvd_weapon_forget_ranking) {
		int age = MSG_ReadByte();

		if (age < 0) {
			return false;
		}

		sequence_set = cl->netchan.incoming_sequence - age;

		write = (sequence_set > cl->weaponswitch_sequence_set);
	}

	while ((weap = MSG_ReadByte()) > 0) {
		if (flags & clc_mvd_weapon_full_impulse) {
			if (weap && write && w < MAX_WEAPONSWITCH_OPTIONS) {
				// only add 1-8 to the wrank weapon preference string...
				if (weap >= 1 && weap <= 8 && wrank_index < MAX_WEAPONSWITCH_OPTIONS) {
					new_wrank[wrank_index++] = '0' + weap;
				}
				new_selections[w++] = weap;
			}
		}
		else {
			int weap1 = (weap >> 4) & 15;
			int weap2 = (weap & 15);

			weap1 = bound(0, weap1, 9);
			weap2 = bound(0, weap2, 9);

			if (weap1 && write && w < MAX_WEAPONSWITCH_OPTIONS) {
				new_wrank[w] = '0' + weap1;
				new_selections[w++] = weap1;
			}
			if (weap1 && weap2 && write && w < MAX_WEAPONSWITCH_OPTIONS) {
				new_wrank[w] = '0' + weap2;
				new_selections[w++] = weap2;
			}

			if (!weap1 || !weap2) {
				break;
			}
		}
	}
	if (flags & clc_mvd_weapon_hide_axe) {
		weapon_hide_selection = 1;
	}
	else if (flags & clc_mvd_weapon_hide_sg) {
		weapon_hide_selection = 2;
	}

	if (weap < 0) {
		return false;
	}

	cl->weaponswitch_enabled = (flags & clc_mvd_weapon_switching);
	if (write) {
		cl->weaponswitch_sequence_set = sequence_set;
		cl->weaponswitch_forgetorder = (flags & clc_mvd_weapon_forget_ranking);
		cl->weaponswitch_mode = (flags & (clc_mvd_weapon_mode_presel | clc_mvd_weapon_mode_iffiring));
		cl->weaponswitch_hide = weapon_hide_selection;
		cl->weaponswitch_hide_on_death = (flags & clc_mvd_weapon_reset_on_death);
		memcpy(cl->weaponswitch_priority, new_selections, sizeof(new_selections));
		SV_DebugServerSideWeaponInstruction(cl);

		if (!cl->weaponswitch_forgetorder) {
			SV_UserSetWeaponRank(cl, new_wrank);
		}
	}
	cl->weaponswitch_pending |= write;
	cl->weaponswitch_sequence_set = sequence_set;
	return true;
}
#endif // MVD_PEXT1_SERVERSIDEWEAPON

/*
==============
SV_UserInit
==============
*/
void SV_UserInit (void)
{
	Cvar_Register (&sv_spectalk);
	Cvar_Register (&sv_sayteam_to_spec);
	Cvar_Register (&sv_mapcheck);
	Cvar_Register (&sv_minping);
	Cvar_Register (&sv_maxping);
	Cvar_Register (&sv_enable_cmd_minping);
	Cvar_Register (&sv_kickuserinfospamtime);
	Cvar_Register (&sv_kickuserinfospamcount);
	Cvar_Register (&sv_maxuploadsize);
#ifdef FTE_PEXT_CHUNKEDDOWNLOADS
	Cvar_Register (&sv_downloadchunksperframe);
#endif

#ifdef FTE_PEXT2_VOICECHAT
	Cvar_Register (&sv_voip);
	Cvar_Register (&sv_voip_echo);
	Cvar_Register (&sv_voip_record);
#endif

#ifdef MVD_PEXT1_DEBUG
	Cvar_Register(&sv_debug_weapons);
#endif
	Cvar_Register(&sv_debug_antilag);
	Cvar_Register(&sv_debug_usercmd);
}

static void SV_DebugClientCommand(byte playernum, const usercmd_t* usercmd, int dropnum_)
{
	if (playernum >= MAX_CLIENTS) {
		return;
	}

	if (sv_debug_usercmd.value >= 1 || svs.clients[playernum].mvd_write_usercmds) {
		mvdhidden_block_header_t header;
		byte dropnum = min(dropnum_, 255);

		header.type_id = mvdhidden_usercmd;
		header.length = sizeof_mvdhidden_block_header_t_usercmd;

		if (MVDWrite_HiddenBlockBegin(sizeof_mvdhidden_block_header_t_range0 + header.length)) {
			MVD_SZ_Write(&header.length, sizeof(header.length));
			MVD_SZ_Write(&header.type_id, sizeof(header.type_id));

			MVD_SZ_Write(&playernum, sizeof(playernum));
			MVD_SZ_Write(&dropnum, sizeof(dropnum));
			MVD_SZ_Write(&usercmd->msec, sizeof(usercmd->msec));
			MVD_SZ_Write(&usercmd->angles[0], sizeof(usercmd->angles[0]));
			MVD_SZ_Write(&usercmd->angles[1], sizeof(usercmd->angles[1]));
			MVD_SZ_Write(&usercmd->angles[2], sizeof(usercmd->angles[2]));
			MVD_SZ_Write(&usercmd->forwardmove, sizeof(usercmd->forwardmove));
			MVD_SZ_Write(&usercmd->sidemove, sizeof(usercmd->sidemove));
			MVD_SZ_Write(&usercmd->upmove, sizeof(usercmd->upmove));
			MVD_SZ_Write(&usercmd->buttons, sizeof(usercmd->buttons));
			MVD_SZ_Write(&usercmd->impulse, sizeof(usercmd->impulse));
		}
	}
}

static void SV_ClientDownloadComplete(client_t* cl)
{
	if (cl->download) {
		const char* val;

		VFS_CLOSE(cl->download);
		cl->download = NULL;
		cl->file_percent = 0; //bliP: file percent
		// set normal rate
		val = Info_Get(&cl->_userinfo_ctx_, "rate");
		cl->netchan.rate = 1.0 / SV_BoundRate(false, Q_atoi(*val ? val : "99999"));
		// set normal duplicate packets
		cl->netchan.dupe = cl->dupe;
	}
}

#endif // !CLIENTONLY
