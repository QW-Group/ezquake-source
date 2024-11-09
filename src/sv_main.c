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

#ifndef CLIENTONLY
#include "qwsvdef.h"

#ifdef SERVERONLY

qbool       host_initialized;
qbool       host_everything_loaded;	// true if OnChange() applied to every var, end of Host_Init()

double      curtime;			// not bounded or scaled, shared by local client and server.
double      realtime;			// affected by pause, you should not use it unless it something like physics and such.

static int  host_hunklevel;

#else

qbool       server_cfg_done = false;
double      realtime;			// affected by pause, you should not use it unless it something like physics and such.

#endif

int		current_skill;			// for entity spawnflags checking

client_t	*sv_client;			// current client

char		master_rcon_password[128] = "";	//bliP: password for remote server commands

cvar_t	sv_mintic = {"sv_mintic","0.013"};	// bound the size of the
cvar_t	sv_maxtic = {"sv_maxtic","0.1"};	// physics time tic
cvar_t	sv_maxfps = {"maxfps", "77", CVAR_SERVERINFO};  // It actually should be called maxpps (max packets per second).
														// It was serverinfo variable for quite long time, lets legolize it as cvar.
														// Sad part is what we can't call it like sv_maxfps since clients relay on its name 'maxfps' already.

void OnChange_sysselecttimeout_var (cvar_t *var, char *value, qbool *cancel);
cvar_t	sys_select_timeout = {"sys_select_timeout", "10000", 0, OnChange_sysselecttimeout_var}; // microseconds.

cvar_t	sys_restart_on_error = {"sys_restart_on_error", "0"};
cvar_t  sv_mod_extensions = { "sv_mod_extensions", "2", CVAR_ROM };

#ifdef SERVERONLY
cvar_t  sys_simulation = { "sys_simulation", "0" };
cvar_t	developer = {"developer", "0"};		// show extra messages
cvar_t	version = {"version", "", CVAR_ROM};
#endif

cvar_t	timeout = {"timeout", "65"};		// seconds without any message
cvar_t	zombietime = {"zombietime", "2"};	// seconds to sink messages
// after disconnect

#ifdef SERVERONLY
cvar_t	rcon_password = {"rcon_password", ""};	// password for remote server commands
cvar_t	password = {"password", ""};	// password for entering the game
#else
// client already have such variables.
extern cvar_t rcon_password;
extern cvar_t password;
#endif

cvar_t	sv_hashpasswords = {"sv_hashpasswords", "1"}; // 0 - plain passwords; 1 - hashed passwords
cvar_t	sv_crypt_rcon = {"sv_crypt_rcon", "1"}; // use SHA1 for encryption of rcon_password and using timestamps
// Time in seconds during which in rcon command this encryption is valid (change only with master_rcon_password).
cvar_t	sv_timestamplen = {"sv_timestamplen", "60"};
cvar_t	sv_rconlim = {"sv_rconlim", "10"};	// rcon bandwith limit: requests per second

//bliP: telnet log level
void OnChange_telnetloglevel_var (cvar_t *var, char *string, qbool *cancel);
cvar_t  telnet_log_level = {"telnet_log_level", "0", 0, OnChange_telnetloglevel_var};
//<-

cvar_t	frag_log_type = {"frag_log_type", "0"};
//	frag log type:
//		0 - old style (  qwsv - v0.165)
//		1 - new style (v0.168 - v0.172)

void OnChange_qconsolelogsay_var (cvar_t *var, char *string, qbool *cancel);
cvar_t	qconsole_log_say = {"qconsole_log_say", "0", 0, OnChange_qconsolelogsay_var};
// logging "say" and "say_team" messages to the qconsole_PORT.log file

cvar_t	sys_command_line = {"sys_command_line", "", CVAR_ROM};

cvar_t	sv_use_dns = {"sv_use_dns", "0"}; // 1 - use DNS lookup in status command, 0 - don't use
cvar_t	spectator_password = {"spectator_password", ""};	// password for entering as a sepctator
cvar_t	vip_password = {"vip_password", ""};	// password for entering as a VIP sepctator
cvar_t	vip_values = {"vip_values", ""};

cvar_t	allow_download = {"allow_download", "1"};
cvar_t	allow_download_skins = {"allow_download_skins", "1"};
cvar_t	allow_download_models = {"allow_download_models", "1"};
cvar_t	allow_download_sounds = {"allow_download_sounds", "1"};
cvar_t	allow_download_maps	= {"allow_download_maps", "1"};
cvar_t	allow_download_pakmaps = {"allow_download_pakmaps", "1"};
cvar_t	allow_download_demos = {"allow_download_demos", "1"};
cvar_t	allow_download_other = {"allow_download_other", "0"};
//bliP: init ->
cvar_t	download_map_url = {"download_map_url", ""};

cvar_t	sv_specprint = {"sv_specprint", "0"};
cvar_t	sv_reconnectlimit = {"sv_reconnectlimit", "0"};

void OnChange_admininfo_var (cvar_t *var, char *string, qbool *cancel);
cvar_t  sv_admininfo = {"sv_admininfo", "", 0, OnChange_admininfo_var};

cvar_t	sv_unfake = {"sv_unfake", "1"}; //bliP: 24/9 kickfake to unfake
cvar_t	sv_kicktop = {"sv_kicktop", "1"};

cvar_t	sv_allowlastscores = {"sv_allowlastscores", "1"};

cvar_t	sv_maxlogsize = {"sv_maxlogsize", "0"};
//bliP: 24/9 ->
void OnChange_logdir_var (cvar_t *var, char *string, qbool *cancel);
cvar_t  sv_logdir = {"sv_logdir", ".", 0, OnChange_logdir_var};

cvar_t  sv_speedcheck = {"sv_speedcheck", "1"};
//<-
//<-
//cvar_t	sv_highchars = {"sv_highchars", "1"};
cvar_t	sv_phs = {"sv_phs", "1"};
cvar_t	pausable = {"pausable", "0"};
cvar_t	sv_maxrate = {"sv_maxrate", "0"};
cvar_t	sv_getrealip = {"sv_getrealip", "1"};
cvar_t	sv_serverip = {"sv_serverip", ""};
cvar_t	sv_forcespec_onfull = {"sv_forcespec_onfull", "2"};
cvar_t	sv_maxdownloadrate = {"sv_maxdownloadrate", "0"};

cvar_t  sv_loadentfiles = {"sv_loadentfiles", "1"}; //loads .ent files by default if there
cvar_t  sv_loadentfiles_dir = {"sv_loadentfiles_dir", ""}; // check for .ent file in maps/sv_loadentfiles_dir first then just maps/
cvar_t	sv_default_name = {"sv_default_name", "unnamed"};

void sv_mod_msg_file_OnChange(cvar_t *cvar, char *value, qbool *cancel);
cvar_t	sv_mod_msg_file = {"sv_mod_msg_file", "", CVAR_NONE, sv_mod_msg_file_OnChange};

cvar_t  sv_reliable_sound = {"sv_reliable_sound", "0"};

//
// game rules mirrored in svs.info
//
cvar_t	fraglimit = {"fraglimit","0",CVAR_SERVERINFO};
cvar_t	timelimit = {"timelimit","0",CVAR_SERVERINFO};
cvar_t	teamplay = {"teamplay","0",CVAR_SERVERINFO};
cvar_t	maxclients = {"maxclients","24",CVAR_SERVERINFO};
cvar_t	maxspectators = {"maxspectators","8",CVAR_SERVERINFO};
cvar_t	maxvip_spectators = {"maxvip_spectators","0"/*,CVAR_SERVERINFO*/};
cvar_t	deathmatch = {"deathmatch","3",CVAR_SERVERINFO};
cvar_t	watervis = {"watervis","0",CVAR_SERVERINFO};
cvar_t	serverdemo = {"serverdemo","",CVAR_SERVERINFO | CVAR_ROM};

cvar_t	samelevel = {"samelevel","1"}; // dont delete this variable - it used by mods
cvar_t	skill = {"skill", "1"}; // dont delete this variable - it used by mods
cvar_t	coop = {"coop", "0"}; // dont delete this variable - it used by mods

cvar_t	sv_paused = {"sv_paused", "0", CVAR_ROM};

cvar_t	hostname = {"hostname", "unnamed", CVAR_SERVERINFO};

cvar_t sv_forcenick = {"sv_forcenick", "0"}; //0 - don't force; 1 - as login;
cvar_t sv_registrationinfo = {"sv_registrationinfo", ""}; // text shown before "enter login"

// We need this cvar, because some mods didn't allow us to go at some placeses of, for example, start map.
cvar_t registered = {"registered", "1", CVAR_ROM};

cvar_t	sv_halflifebsp = {"halflifebsp", "0", CVAR_ROM};
cvar_t  sv_bspversion = {"sv_bspversion", "1", CVAR_ROM};

// If set, don't send broadcast messages, entities or player info to ServeMe bot
cvar_t sv_serveme_fix = { "sv_serveme_fix", "1", CVAR_ROM };

#ifdef FTE_PEXT_FLOATCOORDS
cvar_t sv_bigcoords = {"sv_bigcoords", "", CVAR_SERVERINFO};
#endif
#ifdef MVD_PEXT1_SERVERSIDEWEAPON
// Only enabled on KTX mod (see sv_init)
cvar_t sv_pext_mvdsv_serversideweapon = { "sv_pext_mvdsv_serversideweapon", "1" };
#endif

cvar_t sv_extlimits = { "sv_extlimits", "2" };

qbool sv_error = false;

client_t *WatcherId = NULL; // QW262

//============================================================================

qbool GameStarted(void)
{
	mvddest_t *d;

	for (d = demo.dest; d; d = d->nextdest)
		if (d->desttype != DEST_STREAM) // oh, its not stream, treat as "game is started"
			break;

	return (d || strncasecmp(Info_ValueForKey(svs.info, "status"), "Standby", 8));
}
/*
================
SV_Shutdown

Quake calls this before calling Sys_Quit or Sys_Error
================
*/
void SV_Shutdown (char *finalmsg)
{
	int i;

	if (!sv.state)
		return; // already shutdown. FIXME: what about error during SV_SpawnServer() ?

	SV_FinalMessage(finalmsg);

	Master_Shutdown ();

	for (i = MIN_LOG; i < MAX_LOG; ++i)
	{
		if (logs[i].sv_logfile)
		{
			fclose (logs[i].sv_logfile);
			logs[i].sv_logfile = NULL;
		}
	}
	if (sv.mvdrecording)
		SV_MVDStop_f();

#ifndef SERVER_ONLY
	NET_CloseServer ();
#endif

#if defined(SERVERONLY) && defined(WWW_INTEGRATION)
	Central_Shutdown();
#endif

	// Shutdown game.
	PR_GameShutDown();
	PR_UnLoadProgs();

	memset (&sv, 0, sizeof(sv));
	sv.state = ss_dead;
#ifndef SERVERONLY
	com_serveractive = false;
	{
		extern  ctxinfo_t _localinfo_;

		Info_RemoveAll(&_localinfo_);
		for (i = 0; i < MAX_CLIENTS; ++i) {
			Info_RemoveAll(&svs.clients[i]._userinfo_ctx_);
			Info_RemoveAll(&svs.clients[i]._userinfoshort_ctx_);
		}
	}
#endif

	memset (svs.clients, 0, sizeof(svs.clients));
	svs.lastuserid = 0;
	svs.serverflags = 0;
}

/*
================
SV_Error

Sends a datagram to all the clients informing them of the server crash,
then stops the server
================
*/
void SV_Error (char *error, ...)
{
	static char string[1024];
	va_list argptr;

	sv_error = true;

	va_start (argptr, error);
	vsnprintf (string, sizeof (string), error, argptr);
	va_end (argptr);

	SV_Shutdown (va ("SV_Error: %s\n", string));

	Host_EndGame();
	Host_Error("SV_Error: %s", string);
}

static void SV_FreeHeadDelayedPacket(client_t *cl) {
	if (cl->packets) {
		packet_t *next = cl->packets->next;
		cl->packets->next = svs.free_packets;
		svs.free_packets = cl->packets;
		cl->packets = next;
	}
}


void SV_FreeDelayedPackets (client_t *cl) {
	while (cl->packets)
		SV_FreeHeadDelayedPacket(cl);
}

/*
==================
SV_FinalMessage

Used by SV_Error and SV_Quit_f to send a final message to all connected
clients before the server goes down.  The messages are sent immediately,
not just stuck on the outgoing message list, because the server is going
to totally exit after returning from this function.
==================
*/
void SV_FinalMessage (const char *message)
{
	client_t *cl;
	int i;

	SZ_Clear (&net_message);
	MSG_WriteByte (&net_message, svc_print);
	MSG_WriteByte (&net_message, PRINT_HIGH);
	MSG_WriteString (&net_message, message);
	MSG_WriteByte (&net_message, svc_disconnect);

	for (i=0, cl = svs.clients ; i<MAX_CLIENTS ; i++, cl++)
		if (cl->state >= cs_spawned
#ifdef USE_PR2
			&& !cl->isBot
#endif
		) {
			Netchan_Transmit(&cl->netchan, net_message.cursize
				, net_message.data);
		}
}



/*
=====================
SV_DropClient

Called when the player is totally leaving the server, either willingly
or unwillingly.  This is NOT called if the entire server is quiting
or crashing.
=====================
*/
void SV_DropClient(client_t* drop)
{
	//bliP: cuff, mute ->
	SV_SavePenaltyFilter (drop, ft_mute, drop->lockedtill);
	SV_SavePenaltyFilter (drop, ft_cuff, drop->cuff_time);
	//<-

	//bliP: player logging
	if (drop->name[0])
		SV_LogPlayer(drop, "disconnect", 1);
	//<-

	// add the disconnect
#ifdef USE_PR2
	if( drop->isBot )
	{
		extern void RemoveBot(client_t *cl);
		RemoveBot(drop);
		return;
	}
#endif
	MSG_WriteByte (&drop->netchan.message, svc_disconnect);

	if (drop->state == cs_spawned)
	{
		// call the prog function for removing a client
		// this will set the body to a dead frame, among other things
		pr_global_struct->self = EDICT_TO_PROG(drop->edict);
		PR_GameClientDisconnect(drop->spectator);
	}

	if (drop->spectator)
		Con_Printf ("Spectator %s removed\n",drop->name);
	else
		Con_Printf ("Client %s removed\n",drop->name);

	if (drop->download)
	{
		VFS_CLOSE(drop->download);
		drop->download = NULL;
	}
	if (drop->upload)
	{
		fclose (drop->upload);
		drop->upload = NULL;
	}
	*drop->uploadfn = 0;

	SV_Logout(drop);

	drop->state = cs_zombie;		    // become free in a few seconds
	SV_SetClientConnectionTime(drop);   // for zombie timeout

// MD -->
	if (drop == WatcherId)
		WatcherId = NULL;
// <-- MD

	drop->old_frags = 0;
	drop->edict->v->frags = 0.0;
	drop->name[0] = 0;

	Info_RemoveAll(&drop->_userinfo_ctx_);
	Info_RemoveAll(&drop->_userinfoshort_ctx_);

	// send notification to all remaining clients
	SV_FullClientUpdate(drop, &sv.reliable_datagram);
}


//====================================================================

/*
===================
SV_CalcPing

===================
*/
int SV_CalcPing (client_t *cl)
{
	register client_frame_t *frame;
	int count, i;
	float ping;


	//bliP: 999 ping for connecting players
	if (cl->state != cs_spawned)
		return 999;
	//<-

	ping = 0;
	count = 0;
#ifdef USE_PR2
	if (cl->isBot) {
		return 10;
	}
#endif
	for (frame = cl->frames, i=0 ; i<UPDATE_BACKUP ; i++, frame++)
	{
		if (frame->ping_time > 0)
		{
			ping += frame->ping_time;
			count++;
		}
	}
	if (!count)
		return 9999;
	ping /= count;

	return ping*1000;
}

/*
===================
SV_FullClientUpdate

Writes all update values to a sizebuf
===================
*/
void SV_FullClientUpdate (client_t *client, sizebuf_t *buf)
{
	char info[MAX_EXT_INFO_STRING];
	int i;

	i = client - svs.clients;

	//Sys_Printf("SV_FullClientUpdate:  Updated frags for client %d\n", i);

	MSG_WriteByte (buf, svc_updatefrags);
	MSG_WriteByte (buf, i);
	MSG_WriteShort (buf, client->old_frags);

	MSG_WriteByte (buf, svc_updateping);
	MSG_WriteByte (buf, i);
	MSG_WriteShort (buf, SV_CalcPing (client));

	MSG_WriteByte (buf, svc_updatepl);
	MSG_WriteByte (buf, i);
	MSG_WriteByte (buf, client->lossage);

	MSG_WriteByte (buf, svc_updateentertime);
	MSG_WriteByte (buf, i);
	MSG_WriteFloat (buf, SV_ClientGameTime(client));

	Info_ReverseConvert(&client->_userinfoshort_ctx_, info, sizeof(info));
	Info_RemovePrefixedKeys (info, '_');	// server passwords, etc

	MSG_WriteByte (buf, svc_updateuserinfo);
	MSG_WriteByte (buf, i);
	MSG_WriteLong (buf, client->userid);
	MSG_WriteString (buf, info);
}

/*
===================
SV_FullClientUpdateToClient

Writes all update values to a client's reliable stream
===================
*/
void SV_FullClientUpdateToClient (client_t *client, client_t *cl)
{
	char info[MAX_EXT_INFO_STRING];

	Info_ReverseConvert(&client->_userinfoshort_ctx_, info, sizeof(info));

	ClientReliableCheckBlock(cl, 24 + strlen(info));
	if (cl->num_backbuf)
	{
		SV_FullClientUpdate (client, &cl->backbuf);
		ClientReliable_FinishWrite(cl);
	}
	else
		SV_FullClientUpdate (client, &cl->netchan.message);
}

//Returns a unique userid in [1..MAXUSERID] range
#define MAXUSERID 99
int SV_GenerateUserID (void)
{
	client_t *cl;
	int i;


	do {
		svs.lastuserid++;
		if (svs.lastuserid == 1 + MAXUSERID)
			svs.lastuserid = 1;
		for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
			if (cl->state != cs_free && cl->userid == svs.lastuserid)
				break;
	} while (i != MAX_CLIENTS);

	return svs.lastuserid;
}

/*
==============================================================================

CONNECTIONLESS COMMANDS

==============================================================================
*/

/*
SVC_QTVStreams

Responds with info on connected QTV users
*/
static void SVC_QTVUsers (void)
{
	SV_BeginRedirect (RD_PACKET);
	QTV_Streams_UserList ();
	SV_EndRedirect ();
}

/*
================
SVC_Status

Responds with all the info that qplug or qspy can see
This message can be up to around 5k with worst case string lengths.
================
*/
#define STATUS_OLDSTYLE                 0
#define STATUS_SERVERINFO               1
#define STATUS_PLAYERS                  2
#define STATUS_SPECTATORS               4
#define STATUS_SPECTATORS_AS_PLAYERS    8 //for ASE - change only frags: show as "S"
#define STATUS_SHOWTEAMS                16
#define STATUS_SHOWQTV                  32
#define STATUS_SHOWFLAGS                64

static void SVC_Status (void)
{
	int top, bottom, ping, i, opt = 0;
	char *name, *frags;
	client_t *cl;


	if (Cmd_Argc() > 1)
		opt = Q_atoi(Cmd_Argv(1));

	SV_BeginRedirect (RD_PACKET);
	if (opt == STATUS_OLDSTYLE || (opt & STATUS_SERVERINFO))
		Con_Printf ("%s\n", svs.info);
	if (opt == STATUS_OLDSTYLE || (opt & (STATUS_PLAYERS | STATUS_SPECTATORS)))
		for (i = 0; i < MAX_CLIENTS; i++)
		{
			cl = &svs.clients[i];
			if ( (cl->state >= cs_preconnected/* || cl->state == cs_spawned */) &&
			        ( (!cl->spectator && ((opt & STATUS_PLAYERS) || opt == STATUS_OLDSTYLE)) ||
			          ( cl->spectator && ( opt & STATUS_SPECTATORS)) ) )
			{
				top    = Q_atoi(Info_Get (&cl->_userinfo_ctx_, "topcolor"));
				bottom = Q_atoi(Info_Get (&cl->_userinfo_ctx_, "bottomcolor"));
				top    = (top    < 0) ? 0 : ((top    > 13) ? 13 : top);
				bottom = (bottom < 0) ? 0 : ((bottom > 13) ? 13 : bottom);
				ping   = SV_CalcPing (cl);
				name   = cl->name;
				if (cl->spectator)
				{
					if (opt & STATUS_SPECTATORS_AS_PLAYERS)
						frags = "S";
					else
					{
						ping  = -ping;
						frags = "-9999";
						name  = va("\\s\\%s", name);
					}
				}
				else
					frags = va("%i", cl->old_frags);

				Con_Printf ("%i %s %i %i \"%s\" \"%s\" %i %i", cl->userid, frags,
				            (int)(SV_ClientConnectedTime(cl) / 60.0f), ping, name,
				            Info_Get (&cl->_userinfo_ctx_, "skin"), top, bottom);

				if (opt & STATUS_SHOWTEAMS) {
					Con_Printf(" \"%s\"", cl->team);
				}

				if (opt & STATUS_SHOWFLAGS) {
					if (cl->login_flag[0]) {
						Con_Printf(" \"%s\"", cl->login_flag);
					}
					else if (cl->logged_in_via_web || cl->logged > 0) {
						Con_Printf(" \"none\"");
					}
					else {
						Con_Printf(" \"\"");
					}
				}

				Con_Printf("\n");
			}
		}

	if (opt & STATUS_SHOWQTV)
		QTV_Streams_List ();
	SV_EndRedirect ();
}

/*
===================
SVC_LastScores

===================
*/
void SV_LastScores_f (void);
static void SVC_LastScores (void)
{
	if(!(int)sv_allowlastscores.value)
		return;

	SV_BeginRedirect (RD_PACKET);
	SV_LastScores_f ();
	SV_EndRedirect ();
}

/*
===================
SVC_LastStats
===================
*/
void SV_LastStats_f (void);
static void SVC_LastStats (void)
{
	if(!(int)sv_allowlastscores.value)
		return;

	SV_BeginRedirect (RD_PACKET);
	SV_LastStats_f ();
	SV_EndRedirect ();
}

/*
===================
SVC_DemoList
SVC_DemoListRegex
===================
*/
void SV_DemoList_f (void);
static void SVC_DemoList (void)
{
	SV_BeginRedirect (RD_PACKET);
	SV_DemoList_f ();
	SV_EndRedirect ();
}
void SV_DemoListRegex_f (void);
static void SVC_DemoListRegex (void)
{
	SV_BeginRedirect (RD_PACKET);
	SV_DemoListRegex_f ();
	SV_EndRedirect ();
}

/*
===================
SV_CheckLog

===================
*/
#define	LOG_HIGHWATER	(MAX_DATAGRAM - 128)
#define	LOG_FLUSH		10*60
static void SV_CheckLog (void)
{
	sizebuf_t *sz;

	if (sv.state != ss_active)
		return;

	sz = &svs.log[svs.logsequence&1];

	// bump sequence if allmost full, or ten minutes have passed and
	// there is something still sitting there
	if (sz->cursize > LOG_HIGHWATER
	        || (realtime - svs.logtime > LOG_FLUSH && sz->cursize) )
	{
		// swap buffers and bump sequence
		svs.logtime = realtime;
		svs.logsequence++;
		sz = &svs.log[svs.logsequence&1];
		sz->cursize = 0;
		Con_DPrintf ("beginning fraglog sequence %i\n", svs.logsequence);
	}

}

/*
================
SVC_Log

Responds with all the logged frags for ranking programs.
If a sequence number is passed as a parameter and it is
the same as the current sequence, an A2A_NACK will be returned
instead of the data.
================
*/
static void SVC_Log (void)
{
	char data[MAX_DATAGRAM+64];
	int seq;


	if (Cmd_Argc() == 2)
		seq = Q_atoi(Cmd_Argv(1));
	else
		seq = -1;

	if (seq == svs.logsequence-1 || !logs[FRAG_LOG].sv_logfile)
	{	// they already have this data, or we aren't logging frags
		data[0] = A2A_NACK;
		NET_SendPacket (NS_SERVER, 1, data, net_from);
		return;
	}

	Con_DPrintf ("sending log %i to %s\n", svs.logsequence-1, NET_AdrToString(net_from));

	snprintf (data, MAX_DATAGRAM + 64, "stdlog %i\n", svs.logsequence-1);
	strlcat (data, (char *)svs.log_buf[((svs.logsequence-1)&1)], MAX_DATAGRAM + 64);

	NET_SendPacket (NS_SERVER, strlen(data)+1, data, net_from);
}

/*
================
SVC_Ping

Just responds with an acknowledgement
================
*/
static void SVC_Ping (void)
{
	char data = A2A_ACK;

	NET_SendPacket (NS_SERVER, 1, &data, net_from);
}

/*
=================
SVC_GetChallenge

Returns a challenge number that can be used
in a subsequent client_connect command.
We do this to prevent denial of service attacks that
flood the server with invalid connection IPs.  With a
challenge, they must give a valid IP address.
=================
*/
static void SVC_GetChallenge (void)
{
	int oldestTime, oldest, i;
	char buf[256], *over;


	oldest = 0;
	oldestTime = 0x7fffffff;

	// see if we already have a challenge for this ip
	for (i = 0 ; i < MAX_CHALLENGES ; i++)
	{
		if (NET_CompareBaseAdr (net_from, svs.challenges[i].adr))
			break;
		if (svs.challenges[i].time < oldestTime)
		{
			oldestTime = svs.challenges[i].time;
			oldest = i;
		}
	}

	if (i == MAX_CHALLENGES)
	{
		// overwrite the oldest
		svs.challenges[oldest].challenge = (rand() << 16) ^ rand();
		svs.challenges[oldest].adr = net_from;
		svs.challenges[oldest].time = realtime;
		i = oldest;
	}

	// send it back
	snprintf(buf, sizeof(buf), "%c%i", S2C_CHALLENGE, svs.challenges[i].challenge);
	over = buf + strlen(buf) + 1;

#ifdef PROTOCOL_VERSION_FTE
	//tell the client what fte extensions we support
	if (svs.fteprotocolextensions)
	{
		int lng;

		lng = LittleLong(PROTOCOL_VERSION_FTE);
		memcpy(over, &lng, sizeof(int));
		over += 4;

		lng = LittleLong(svs.fteprotocolextensions);
		memcpy(over, &lng, sizeof(int));
		over += 4;
	}
#endif // PROTOCOL_VERSION_FTE

#ifdef PROTOCOL_VERSION_FTE2
	//tell the client what fte extensions2 we support
	if (svs.fteprotocolextensions2)
	{
		int lng;

		lng = LittleLong(PROTOCOL_VERSION_FTE2);
		memcpy(over, &lng, sizeof(int));
		over += 4;

		lng = LittleLong(svs.fteprotocolextensions2);
		memcpy(over, &lng, sizeof(int));
		over += 4;
	}
#endif // PROTOCOL_VERSION_FTE2

#ifdef PROTOCOL_VERSION_MVD1
	// tell the client what mvdsv extensions we support
	if (svs.mvdprotocolextension1) {
		int lng;

		lng = LittleLong(PROTOCOL_VERSION_MVD1);
		memcpy(over, &lng, sizeof(int));
		over += 4;

		lng = LittleLong(svs.mvdprotocolextension1);
		memcpy(over, &lng, sizeof(int));
		over += 4;
	}
#endif

	Netchan_OutOfBand(NS_SERVER, net_from, over-buf, (byte*) buf);
}

static qbool ValidateUserInfo (char *userinfo)
{
	if (strstr(userinfo, "&c") || strstr(userinfo, "&r"))
		return false;

	while (*userinfo)
	{
		if (*userinfo == '\\')
			userinfo++;

		if (*userinfo++ == '\\')
			return false;
		while (*userinfo && *userinfo != '\\')
			userinfo++;
	}
	return true;
}

//==============================================

void FixMaxClientsCvars(void)
{
	if ((int)maxclients.value > MAX_CLIENTS)
		Cvar_SetValue (&maxclients, MAX_CLIENTS);

	if ((int)maxspectators.value > MAX_CLIENTS)
		Cvar_SetValue (&maxspectators, MAX_CLIENTS);

	if ((int)maxvip_spectators.value > MAX_CLIENTS)
		Cvar_SetValue (&maxvip_spectators, MAX_CLIENTS);

	if ((int)maxspectators.value + maxclients.value > MAX_CLIENTS)
		Cvar_SetValue (&maxspectators, MAX_CLIENTS - (int)maxclients.value);

	if ((int)maxspectators.value + maxclients.value + maxvip_spectators.value > MAX_CLIENTS)
		Cvar_SetValue (&maxvip_spectators, MAX_CLIENTS - (int)maxclients.value - (int)maxspectators.value);

}

//==============================================

// see if the challenge is valid
qbool CheckChallange( int challenge )
{
	int i;

	if (net_from.type == NA_LOOPBACK)
		return true; // local client do not need challenge

	for (i = 0; i < MAX_CHALLENGES; i++)
	{
		if (NET_CompareBaseAdr (net_from, svs.challenges[i].adr))
		{
			if (challenge == svs.challenges[i].challenge)
				break;		// good

			Netchan_OutOfBandPrint (NS_SERVER, net_from, "%c\nBad challenge.\n", A2C_PRINT);
			return false;
		}
	}

	if (i == MAX_CHALLENGES)
	{
		Netchan_OutOfBandPrint (NS_SERVER, net_from, "%c\nNo challenge for address.\n", A2C_PRINT);
		return false;
	}

	return true;
}

//==============================================

qbool CheckProtocol( int ver )
{
	if (ver != PROTOCOL_VERSION)
	{
		Netchan_OutOfBandPrint (NS_SERVER, net_from, "%c\nServer is version " QW_VERSION ".\n", A2C_PRINT);
		Con_Printf ("* rejected connect from version %i\n", ver);
		return false;
	}

	return true;
}

//==============================================

qbool CheckUserinfo( char *userinfobuf, unsigned int bufsize, char *userinfo )
{
	strlcpy (userinfobuf, userinfo, bufsize);

	// and now validate userinfo
	if ( !ValidateUserInfo( userinfobuf ) )
	{
		Netchan_OutOfBandPrint (NS_SERVER, net_from, "%c\nInvalid userinfo, perhaps &c sequences. Restart your qwcl\n", A2C_PRINT);
		return false;
	}

	return true;
}

//==============================================

int SV_VIPbyIP(netadr_t adr);
int SV_VIPbyPass (char *pass);

qbool CheckPasswords( char *userinfo, int userinfo_size, qbool *spass_ptr, qbool *vip_ptr, int *spectator_ptr )
{
	int spectator;
	qbool spass, vip;

	char *s = Info_ValueForKey (userinfo, "spectator");
	char *pwd;

	spass = vip = spectator = false;

	if (s[0] && strcmp(s, "0"))
	{
		spass = true;

		// first the pass, then ip
		if ( !( vip = SV_VIPbyPass( s ) ) )
		{
			if ( !( vip = SV_VIPbyPass( Info_ValueForKey( userinfo, "password") ) ) )
			{
				vip = SV_VIPbyIP( net_from );
			}
		}

		pwd = spectator_password.string;

		if (pwd[0] && strcasecmp(pwd, "none") && strcmp(pwd, s))
		{
			spass = false; // failed
		}

		if (!vip && !spass)
		{
			Con_Printf ("%s:spectator password failed\n", NET_AdrToString (net_from));
			Netchan_OutOfBandPrint (NS_SERVER, net_from, "%c\nrequires a spectator password\n\n", A2C_PRINT);

			return false;
		}

		Info_RemoveKey (userinfo, "spectator"); // remove passwd
		Info_SetValueForStarKey (userinfo, "*spectator", "1", userinfo_size);

		spectator = Q_atoi(s);

		if (!spectator)
			spectator = true;
	}
	else
	{
		s = Info_ValueForKey (userinfo, "password");

		// first the pass, then ip
		if (!(vip = SV_VIPbyPass(s)))
		{
			vip = SV_VIPbyIP(net_from);
		}

		pwd = password.string;

		if (!vip && pwd[0] && strcasecmp(pwd, "none") && strcmp(pwd, s))
		{
			Con_Printf ("%s:password failed\n", NET_AdrToString (net_from));
			Netchan_OutOfBandPrint (NS_SERVER, net_from, "%c\nserver requires a password\n\n", A2C_PRINT);

			return false;
		}

		Info_RemoveKey (userinfo, "spectator"); // remove "spectator 0" for example

		spectator = false;
	}

	Info_RemoveKey (userinfo, "password"); // remove passwd

	// copy 
	*spass_ptr     = spass;
	*vip_ptr       = vip;
	*spectator_ptr = spectator;

	return true;
}

//==============================================

qbool CheckReConnect( netadr_t adr, int qport )
{
	int i;
	client_t *cl;

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
	{
		if (cl->state == cs_free)
			continue;

		if (NET_CompareBaseAdr (adr, cl->netchan.remote_address) &&
			(cl->netchan.qport == qport || adr.port == cl->netchan.remote_address.port))
		{
			if (SV_ClientConnectedTime(cl) < sv_reconnectlimit.value)
			{
				Con_Printf ("%s:reconnect rejected: too soon\n", NET_AdrToString (adr));
				return false;
			}

			switch ( cl->state )
			{
				case cs_zombie: // zombie already dropped.
					break;

				case cs_preconnected:
				case cs_connected:
				case cs_spawned:

					SV_DropClient (cl);
					SV_ClearReliable (cl);	// don't send the disconnect
					break;

				default:
					return false; // unknown state, should not be the case.
			}

			cl->state = cs_free;
			Con_Printf ("%s:reconnect\n", NET_AdrToString (adr));
			break;
		}
	}

	return true;
}

//==============================================

void CountPlayersSpecsVips(int *clients_ptr, int *spectators_ptr, int *vips_ptr, client_t **newcl_ptr)
{
	client_t *cl = NULL, *newcl = NULL;
	int clients = 0, spectators = 0, vips = 0;
	int i;

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
	{
		if (cl->state == cs_free)
		{
			if (!newcl)
				newcl = cl; // grab first available slot

			continue;
		}

		if (cl->spectator)
		{
			if (cl->vip)
				vips++;
			else
				spectators++;
		}
		else
		{
			clients++;
		}
	}

	if (clients_ptr)
		*clients_ptr = clients;
	if (spectators_ptr)
		*spectators_ptr = spectators;
	if (vips_ptr)
		*vips_ptr = vips;
	if (newcl_ptr)
		*newcl_ptr = newcl;
}

//==============================================

qbool SpectatorCanConnect(int vip, int spass, int spectators, int vips)
{
	FixMaxClientsCvars(); // not a bad idea

	if (vip)
	{
		if (spass && (spectators < (int)maxspectators.value || vips < (int)maxvip_spectators.value))
			return true;
	}
	else
	{
		if (spass && spectators < (int)maxspectators.value)
			return true;
	}

	return false;
}

qbool PlayerCanConnect(int clients)
{
	FixMaxClientsCvars(); // not a bad idea

	if (clients < (int)maxclients.value)
		return true;

	return false;
}

/*
==================
SVC_DirectConnect

A connection request that did not come from the master
==================
*/
extern void MVD_PlayerReset(int player);

extern char *shortinfotbl[];

static void SVC_DirectConnect (void)
{
	int spectator;
	qbool spass, vip, rip_vip;

	int clients, spectators, vips;
	int qport, i, edictnum;

	client_t *newcl;

	char userinfo[1024];
	char *s;
	netadr_t adr;
	edict_t *ent;

#ifdef PROTOCOL_VERSION_FTE
	unsigned int protextsupported = 0;
#endif // PROTOCOL_VERSION_FTE

#ifdef PROTOCOL_VERSION_FTE2
	unsigned int protextsupported2 = 0;
#endif // PROTOCOL_VERSION_FTE2

#ifdef PROTOCOL_VERSION_MVD1
	unsigned int mvdext_supported1 = 0;
#endif

	// check version/protocol
	if ( !CheckProtocol( Q_atoi( Cmd_Argv( 1 ) ) ) )
		return; // wrong protocol number

	// get qport
	qport = Q_atoi( Cmd_Argv( 2 ) );

	// see if the challenge is valid
	if ( !CheckChallange( Q_atoi( Cmd_Argv( 3 ) ) ) )
		return; // wrong challange

	// and now validate userinfo
	if ( !CheckUserinfo( userinfo, sizeof( userinfo ), Cmd_Argv( 4 ) ) )
		return; // wrong userinfo

//
// WARNING: WARNING: WARNING: using Cmd_TokenizeString() so do all Cmd_Argv() above.
//

	while( !msg_badread )
	{
		Cmd_TokenizeString( MSG_ReadStringLine() );

		switch( Q_atoi( Cmd_Argv( 0 ) ) )
		{
#ifdef PROTOCOL_VERSION_FTE
		case PROTOCOL_VERSION_FTE:
			protextsupported = Q_atoi( Cmd_Argv( 1 ) );
			Con_DPrintf("Client supports 0x%x fte extensions\n", protextsupported);
			break;
#endif // PROTOCOL_VERSION_FTE

#ifdef PROTOCOL_VERSION_FTE2
		case PROTOCOL_VERSION_FTE2:
			protextsupported2 = Q_atoi( Cmd_Argv( 1 ) );
			Con_DPrintf("Client supports 0x%x fte extensions2\n", protextsupported2);
			break;
#endif // PROTOCOL_VERSION_FTE2

#ifdef PROTOCOL_VERSION_MVD1
		case PROTOCOL_VERSION_MVD1:
			mvdext_supported1 = Q_atoi( Cmd_Argv( 1 ) );
			Con_DPrintf("Client supports 0x%x mvdsv extensions\n", mvdext_supported1);
			break;
#endif
		}
	}

	msg_badread = false;

	spass = vip = rip_vip = spectator = false;

	// check for password or spectator_password
	if ( !CheckPasswords( userinfo, sizeof(userinfo), &spass, &vip, &spectator) )
		return; // pass was wrong

	adr = net_from;

	// if there is already a slot for this ip, reuse (changed from drop) it
	if ( !CheckReConnect( adr, qport ) )
		return; // can't do that for some reason

	// count up the clients and spectators
	CountPlayersSpecsVips(&clients, &spectators, &vips, &newcl);

	FixMaxClientsCvars();

	// if at server limits, refuse connection

	if ((spectator && !SpectatorCanConnect(vip, spass, spectators, vips)) || 
	    (!spectator && !PlayerCanConnect(clients)) || 
	    !newcl)
	{
		Sys_Printf ("%s:full connect\n", NET_AdrToString (adr));

		// no way to connect does't matter VIP or whatever, just no free slots
		if (!newcl)
		{
			Netchan_OutOfBandPrint (NS_SERVER, adr, "%c\nserver is full\n\n", A2C_PRINT);
			return;
		}

		// !!! SPECTATOR 2 FEATURE !!!
		if (spectator == 2 && !vip &&  vips < (int)maxvip_spectators.value)
		{
			vip = rip_vip = 1; // yet can be connected if realip is on vip list
		}
		else if (    !spectator && spectators < (int)maxspectators.value
				  && (
				  	      ( (int)sv_forcespec_onfull.value == 2
							&&   (Q_atoi(Info_ValueForKey(userinfo, "svf")) & SVF_SPEC_ONFULL)
				  	      ) 
				   	   		||
						  ( (int)sv_forcespec_onfull.value == 1
							&&   !(Q_atoi(Info_ValueForKey(userinfo, "svf")) & SVF_NO_SPEC_ONFULL)
						  )
				   	 )
				)
		{
			Netchan_OutOfBandPrint (NS_SERVER, adr, "%c\nserver is full: connecting as spectator\n", A2C_PRINT);
			Info_SetValueForStarKey (userinfo, "*spectator", "1", sizeof(userinfo));
			spectator = true;
		}
		else
		{
			Netchan_OutOfBandPrint (NS_SERVER, adr, "%c\nserver is full\n\n", A2C_PRINT);
			return;
		}
	}

	// build a new connection
	// accept the new client
	// this is the only place a client_t is ever initialized
	memset (newcl, 0, sizeof(*newcl));

	newcl->userid = SV_GenerateUserID();

#ifdef PROTOCOL_VERSION_FTE
	newcl->fteprotocolextensions = protextsupported;
#endif // PROTOCOL_VERSION_FTE

#ifdef PROTOCOL_VERSION_FTE2
	newcl->fteprotocolextensions2 = protextsupported2;
#endif // PROTOCOL_VERSION_FTE2

#ifdef PROTOCOL_VERSION_MVD1
	newcl->mvdprotocolextensions1 = mvdext_supported1;
#endif

	newcl->_userinfo_ctx_.max      = MAX_CLIENT_INFOS;
	newcl->_userinfoshort_ctx_.max = MAX_CLIENT_INFOS;
	Info_Convert(&newcl->_userinfo_ctx_, userinfo);

	// request protocol extensions.
	if (*Info_Get(&newcl->_userinfo_ctx_, "Qizmo")
		|| *Info_Get(&newcl->_userinfo_ctx_, "*qtv")
	)
	{
		newcl->process_pext = false; // this whould not work over such proxies.
	}
	else
	{
		newcl->process_pext = true;
	}

	Netchan_OutOfBandPrint (NS_SERVER, adr, "%c", S2C_CONNECTION);

	Netchan_Setup (NS_SERVER, &newcl->netchan, adr, qport, Q_atoi(Info_Get(&newcl->_userinfo_ctx_, "mtu")));

	newcl->state = cs_preconnected;

	newcl->datagram.allowoverflow = true;
	newcl->datagram.data = newcl->datagram_buf;
	newcl->datagram.maxsize = sizeof(newcl->datagram_buf);

	// spectator mode can ONLY be set at join time
	newcl->spectator = spectator;
	newcl->vip = vip;
	newcl->rip_vip = rip_vip;

	// extract extensions mask
	newcl->extensions = Q_atoi(Info_Get(&newcl->_userinfo_ctx_, "*z_ext"));
	Info_Remove (&newcl->_userinfo_ctx_, "*z_ext");

	edictnum = (newcl-svs.clients)+1;
	ent = EDICT_NUM(edictnum);
	ent->e.free = false;
	newcl->edict = ent;
	// restore client name.
	PR_SetEntityString(ent, ent->v->netname, newcl->name);

	s = ( vip ? va("%d", vip) : "" );

	Info_SetStar (&newcl->_userinfo_ctx_, "*VIP", s);

	// copy the most important userinfo into userinfoshort
	// {

	// parse some info from the info strings
	SV_ExtractFromUserinfo (newcl, true);

	for (i = 0; shortinfotbl[i] != NULL; i++)
	{
		s = Info_Get(&newcl->_userinfo_ctx_, shortinfotbl[i]);
		Info_SetStar (&newcl->_userinfoshort_ctx_, shortinfotbl[i], s);
	}

	// move star keys to infoshort
	Info_CopyStar( &newcl->_userinfo_ctx_, &newcl->_userinfoshort_ctx_ );

	// }

	// JACK: Init the floodprot stuff.
	memset(newcl->whensaid, 0, sizeof(newcl->whensaid));
	newcl->whensaidhead = 0;
	newcl->lockedtill = 0;
	newcl->disable_updates_stop = -1.0; // Vladis

	newcl->realip_num = rand();

	//bliP: init
	newcl->spec_print = (int)sv_specprint.value;
	newcl->logincount = 0;
	//<-

#ifdef FTE_PEXT2_VOICECHAT
	SV_VoiceInitClient(newcl);
#endif

	// call the progs to get default spawn parms for the new client
	PR_GameSetNewParms();

	for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
		newcl->spawn_parms[i] = (&PR_GLOBAL(parm1))[i];

	// mvd/qtv related stuff
	// Well, here is a chance what player connect after demo recording started,
	// so demo.info[edictnum - 1].model == player_model so SV_MVDWritePackets() will not wrote player model index,
	// so client during playback this demo will got invisible model, because model index will be 0.
	// Fixing that.
	// Btw, struct demo contain different client specific structs, may be they need clearing too, not sure.
	// Also, we have Cmd_Join_f()/Cmd_Observe_f() which have close behaviour to SVC_DirectConnect(),
	// so I put same demo fix in mentioned functions too.
	MVD_PlayerReset(NUM_FOR_EDICT(newcl->edict) - 1);

	newcl->sendinfo = true;
}

static int char2int (int c)
{
	if (c <= '9' && c >= '0')
		return c - '0';
	else if (c <= 'f' && c >= 'a')
		return c - 'a' + 10;
	else if (c <= 'F' && c >= 'A')
		return c - 'A' + 10;
	return 0;
}
/*
 * rcon_bandlim() - check for rcon requests bandwidth limit
 *
 *      From kernel of the FreeBSD 4.10 release:
 *      sys/netinet/ip_icmp.c(846): int badport_bandlim(int which);
 *
 *	Return false if it is ok to check rcon_password, true if we have
 *	hit our bandwidth limit and it is not ok.
 *
 *	If sv_rconlim.value is <= 0, the feature is disabled and false is returned.
 *
 *	Note that the printing of the error message is delayed so we can
 *	properly print the rcon limit error rate that the system was trying to do
 *	(i.e. 22000/100 rcon pps, etc...).  This can cause long delays in printing
 *	the 'final' error, but it doesn't make sense to solve the printing
 *	delay with more complex code.
 */
static qbool rcon_bandlim (void)
{
	static double lticks = 0;
	static int lpackets = 0;

	/*
	 * Return ok status if feature disabled or argument out of
	 * ranage.
	 */

	if ((int)sv_rconlim.value <= 0)
		return false;

	/*
	 * reset stats when cumulative dt exceeds one second.
	 */

	if (realtime - lticks > 1.0)
	{
		if (lpackets > (int)sv_rconlim.value)
			Sys_Printf("WARNING: Limiting rcon response from %d to %d rcon pequests per second from %s\n",
			           lpackets, (int)sv_rconlim.value, NET_AdrToString(net_from));
		lticks = realtime;
		lpackets = 0;
	}

	/*
	 * bump packet count
	 */

	if (++lpackets > (int)sv_rconlim.value)
		return true;

	return false;
}

//bliP: master rcon/logging ->
int Rcon_Validate (char *client_string, char *password1)
{
	unsigned int i;

	if (rcon_bandlim()) {
		return 0;
	}

	if (!strlen(password1)) {
		return 0;
	}

	if ((int)sv_crypt_rcon.value) {
		const char* digest = Cmd_Argv(1);
		const char* time_start = Cmd_Argv(1) + DIGEST_SIZE * 2;

		if (strlen(digest) < DIGEST_SIZE * 2 + sizeof(time_t) * 2) {
			return 0;
		}

		if ((int)sv_timestamplen.value) {
			time_t server_time, client_time = 0;
			double difftime_server_client;

			time(&server_time);
			for (i = 0; i < sizeof(client_time) * 2; i += 2) {
				client_time += (char2int((unsigned char)time_start[i]) << (4 + i * 4)) +
				               (char2int((unsigned char)time_start[i + 1]) << (i * 4));
			}
			difftime_server_client = difftime(server_time, client_time);

			if (difftime_server_client > (double)sv_timestamplen.value || difftime_server_client < -(double)sv_timestamplen.value) {
				return 0;
			}
		}
		SHA1_Init();
		SHA1_Update((unsigned char*)Cmd_Argv(0));
		SHA1_Update((unsigned char*)" ");
		SHA1_Update((unsigned char*)password1);
		SHA1_Update((unsigned char*)time_start);
		SHA1_Update((unsigned char*)" ");
		for (i = 2; (int) i < Cmd_Argc(); i++) {
			SHA1_Update((unsigned char*)Cmd_Argv(i));
			SHA1_Update((unsigned char*)" ");
		}
		if (strncmp(digest, SHA1_Final(), DIGEST_SIZE * 2)) {
			return 0;
		}
	}
	else if (strcmp(Cmd_Argv(1), password1)) {
		return 0;
	}
	return 1;
}

int Master_Rcon_Validate (void)
{
	int i, client_string_len = Cmd_Argc() + 1;
	char *client_string;

	for (i = 0; i < Cmd_Argc(); ++i) {
		client_string_len += strlen(Cmd_Argv(i));
	}
	client_string = (char *) Q_malloc (client_string_len);

	*client_string = 0;
	for (i = 0; i < Cmd_Argc(); ++i) {
		strlcat(client_string, Cmd_Argv(i), client_string_len);
		strlcat(client_string, " ", client_string_len);
	}
	i = Rcon_Validate (client_string, master_rcon_password);
	Q_free(client_string);
	return i;
}

// QW262 -->
void SV_Admin_f (void)
{
	client_t *cl;
	int i = 0;

	if (Cmd_Argc () == 2 && !strcmp (Cmd_Argv (1), "off") && WatcherId &&
			NET_CompareAdr (WatcherId->netchan.remote_address, net_from))
	{
		Con_Printf ("Rcon Watch stopped\n");
		WatcherId = NULL;
		return;
	}

	if (WatcherId)
		Con_Printf ("Rcon Watch is already being made by %s\n", WatcherId->name);
	else
	{
		for (cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
		{
			if (cl->state != cs_spawned)
				continue;

			if (NET_CompareAdr (cl->netchan.remote_address, net_from))
				break;
		}

		if (i == MAX_CLIENTS)
		{
			Con_Printf ("You are not connected to server!\n");
			return;
		}

		WatcherId = cl;
		Con_Printf ("Rcon Watch started for %s\n", cl->name);
	}
}
// <-- QW262

/*
===============
SVC_RemoteCommand

A client issued an rcon command.
Shift down the remaining args
Redirect all printfs
===============
*/
static void SVC_RemoteCommand (char *remote_command)
{
	int			i;
	char		str[1024];
	char		plain[32];
	char		*p;
	unsigned char *hide;
	client_t	*cl;
	qbool		admin_cmd = false;
	qbool		do_cmd = false;
	qbool		bad_cmd = false;
	qbool		banned = false;


	if (Rcon_Validate (remote_command, master_rcon_password))
	{
		if (SV_FilterPacket()) //banned players can't use rcon, but we log it
			banned = true;
		else
			do_cmd = true;
	}
	else if (Rcon_Validate (remote_command, rcon_password.string))
	{
		admin_cmd = true;
		if (SV_FilterPacket()) //banned players can't use rcon, but we log it
		{
			bad_cmd = true;
			banned = true;
		}
		else
		{
			//
			// the following line prevents exploits like:
			//   coop rm
			//   $coop . *
			// which expands to:
			//   rm . *

			Cmd_ExpandString (remote_command, str); // check *expanded* command

			//
			// since the execution parser is not case sensitive, we
			// must check not only for chmod, but also CHMOD, ChmoD, etc.
			// so we lowercase the whole temporary line before checking

			// VVD: strcmp => strcasecmp and we don't need to do this (yes?)
			//for(i = 0; str[i]; i++)
			//	str[i] = (char)tolower(str[i]);

			Cmd_TokenizeString (str);		// must check *all* tokens, because
											// a command/var may not be the first
											// token -- example: "" ls .

			//
			// normal rcon can't use these commands
			//
			// NOTE: this would still be vulnerable to semicolons if
			// they were still allowed, so keep that in mind before
			// re-enabling them

			for (i = 2; i < Cmd_Argc(); i++)
			{
				const char *tstr = Cmd_Argv(i);

				if(!tstr[0]) // skip leading empty tokens
					continue;

				if (!strcasecmp(tstr, "rm") ||
					!strcasecmp(tstr, "rmdir") ||
					!strcasecmp(tstr, "ls") ||
					!strcasecmp(tstr, "chmod") ||
					!strcasecmp(tstr, "sv_admininfo") ||
					!strcasecmp(tstr, "if") ||
					!strcasecmp(tstr, "localcommand") ||
					!strcasecmp(tstr, "sv_crypt_rcon") ||
					!strcasecmp(tstr, "sv_timestamplen") ||
					!strncasecmp(tstr, "log", 3) ||
					!strcasecmp(tstr, "sys_command_line")
					)
				{
					bad_cmd = true;
				}
				break; // stop after first non-empty token
			}

			Cmd_TokenizeString (remote_command); // restore original tokens
		}
		do_cmd = !bad_cmd;
	}

	//find player name if rcon came from someone on server
	plain[0] = '\0';
	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
	{
		if (cl->state == cs_free)
			continue;
#ifdef USE_PR2
		if (cl->isBot)
			continue;
#endif
		if (!NET_CompareBaseAdr(net_from, cl->netchan.remote_address))
			continue;
		if (cl->netchan.remote_address.port != net_from.port)
			continue;

		strlcpy(plain, cl->name, sizeof(plain));
		Q_normalizetext(plain);

		// we found what we need
		break;
	}

	if (do_cmd)
	{
		if (!(int)sv_crypt_rcon.value)
		{
			hide = net_message.data + 9;
			p = admin_cmd ? rcon_password.string : master_rcon_password;
			while (*p)
			{
				p++;
				*hide++ = '*';
			}
		}

		if (plain[0])
			SV_Write_Log(RCON_LOG, 1, va("Rcon from %s (%s): %s\n", NET_AdrToString(net_from), plain, net_message.data + 4));
		else
			SV_Write_Log(RCON_LOG, 1, va("Rcon from %s: %s\n", NET_AdrToString(net_from), net_message.data + 4));

		Con_Printf("Rcon from %s:\n%s\n", NET_AdrToString(net_from), net_message.data + 4);

		SV_BeginRedirect(RD_PACKET);

		str[0] = '\0';
		for (i = 2; i < Cmd_Argc(); i++)
		{
			strlcat(str, Cmd_Argv(i), sizeof(str));
			strlcat(str, " ", sizeof(str));
		}

		Cmd_ExecuteString(str);
	}
	else
	{
		if (admin_cmd && !(int)sv_crypt_rcon.value)
		{
			hide = net_message.data + 9;
			p = admin_cmd ? rcon_password.string : master_rcon_password;
			while (*p)
			{
				p++;
				*hide++ = '*';
			}
		}

		Con_Printf ("Bad rcon from %s: %s\n", NET_AdrToString(net_from), net_message.data + 4);

		if (!banned)
		{
			if (plain[0])
				SV_Write_Log(RCON_LOG, 1, va("Bad rcon from %s (%s):\n%s\n", NET_AdrToString(net_from), plain, net_message.data + 4));
			else
				SV_Write_Log(RCON_LOG, 1, va("Bad rcon from %s:\n%s\n",	NET_AdrToString (net_from), net_message.data + 4));
		}
		else
		{
			SV_Write_Log(RCON_LOG, 1, va("Rcon from banned IP: %s: %s\n", NET_AdrToString(net_from), net_message.data + 4));
			SV_SendBan();
			return;
		}

		SV_BeginRedirect (RD_PACKET);
		if (admin_cmd)
			Con_Printf ("Command not valid.\n");
		else
			Con_Printf ("Bad rcon_password.\n");
	}
	SV_EndRedirect ();
}
//<-

static void SVC_IP(void)
{
	int num;
	client_t *client;

	if (Cmd_Argc() < 3)
		return;

	num = Q_atoi(Cmd_Argv(1));

	if (num < 0 || num >= MAX_CLIENTS)
		return;

	client = &svs.clients[num];
	if (client->state != cs_preconnected)
		return;

	// prevent cheating
	if (client->realip_num != Q_atoi(Cmd_Argv(2)))
		return;

	// don't override previously set ip
	if (client->realip.ip[0])
		return;

	client->realip = net_from;

	// if banned drop
	if (SV_FilterPacket()/* && !client->vip*/)
		SV_DropClient(client);

}


/*
=================
SV_ConnectionlessPacket

A connectionless packet has four leading 0xff
characters to distinguish it from a game channel.
Clients that are in the game can still send
connectionless packets.
=================
*/

static void SV_ConnectionlessPacket (void)
{
	char	*s;
	char	*c;

	MSG_BeginReading ();
	MSG_ReadLong ();		// skip the -1 marker

	s = MSG_ReadStringLine ();

	Cmd_TokenizeString (s);

	c = Cmd_Argv(0);

	if (!strcmp(c, "ping") || ( c[0] == A2A_PING && (c[1] == 0 || c[1] == '\n')) )
		SVC_Ping ();
	else if (c[0] == A2A_ACK && (c[1] == 0 || c[1] == '\n') )
		Con_Printf ("A2A_ACK from %s\n", NET_AdrToString (net_from));
	else if (!strcmp(c,"status"))
		SVC_Status ();
	else if (!strcmp(c,"log"))
		SVC_Log ();
	else if (!strcmp(c, "rcon"))
		SVC_RemoteCommand (s);
	else if (!strcmp(c, "ip"))
		SVC_IP();
	else if (!strcmp(c,"connect"))
		SVC_DirectConnect ();
	else if (!strcmp(c,"getchallenge"))
		SVC_GetChallenge ();
	else if (!strcmp(c,"lastscores"))
		SVC_LastScores ();
	else if (!strcmp(c,"laststats"))
		SVC_LastStats ();
	else if (!strcmp(c,"dlist"))
		SVC_DemoList ();
	else if (!strcmp(c,"dlistr"))
		SVC_DemoListRegex ();
	else if (!strcmp(c,"dlistregex"))
		SVC_DemoListRegex ();
	else if (!strcmp(c,"demolist"))
		SVC_DemoList ();
	else if (!strcmp(c,"demolistr"))
		SVC_DemoListRegex ();
	else if (!strcmp(c,"demolistregex"))
		SVC_DemoListRegex ();
	else if (!strcmp(c,"qtvusers"))
		SVC_QTVUsers ();
	else
		Con_Printf ("bad connectionless packet from %s:\n%s\n"
		            , NET_AdrToString (net_from), s);
}

/*
==============================================================================

PACKET FILTERING


You can add or remove addresses from the filter list with:

addip <ip>
removeip <ip>

The ip address is specified in dot format, and any unspecified digits will match any value, so you can specify an entire class C network with "addip 192.246.40".

Removeip will only remove an address specified exactly the same way.  You cannot addip a subnet, then removeip a single host.

listip
Prints the current list of filters.

writeip
Dumps "addip <ip>" commands to listip.cfg so it can be execed at a later date.  The filter lists are not saved and restored by default, because I beleive it would cause too much confusion.

filterban <0 or 1>

If 1 (the default), then ip addresses matching the current list will be prohibited from entering the game.  This is the default setting.

If 0, then only addresses matching the list will be allowed.  This lets you easily set up a private game, or a game that only allows players from your local network.


==============================================================================
*/


/*typedef struct
{
	unsigned	mask;
	unsigned	compare;
	int			level;
} ipfilter_t;
*/

#define	MAX_IPFILTERS	1024

ipfilter_t	ipfilters[MAX_IPFILTERS];
int		numipfilters;

ipfilter_t	ipvip[MAX_IPFILTERS];
int		numipvips;

//bliP: cuff, mute ->
penfilter_t	penfilters[MAX_PENFILTERS];
int		numpenfilters;
//<-

cvar_t	filterban = {"filterban", "1"};

/*
=================
StringToFilter
=================
*/
qbool StringToFilter (char *s, ipfilter_t *f)
{
	char	num[128];
	int		i, j;
	byte	b[4];
	byte	m[4];

	for (i=0 ; i<4 ; i++)
	{
		b[i] = 0;
		m[i] = 0;
	}

	for (i=0 ; i<4 ; i++)
	{
		if (*s < '0' || *s > '9')
		{
			//Con_Printf ("Bad filter address: %s\n", s);
			return false;
		}

		j = 0;
		while (*s >= '0' && *s <= '9')
		{
			num[j++] = *s++;
		}
		num[j] = 0;
		b[i] = Q_atoi(num);
		if (b[i] != 0)
			m[i] = 255;

		if (!*s)
			break;
		s++;
	}

	f->mask = *(unsigned *)m;
	f->compare = *(unsigned *)b;

	return true;
}

/*
=================
SV_AddIPVIP_f
=================
*/
static void SV_AddIPVIP_f (void)
{
	int		i, l;
	ipfilter_t f;

	if (!StringToFilter (Cmd_Argv(1), &f))
	{
		Con_Printf ("Bad filter address: %s\n", Cmd_Argv(1));
		return;
	}

	l = Q_atoi(Cmd_Argv(2));

	if (l < 1) l = 1;

	for (i=0 ; i<numipvips ; i++)
		if (ipvip[i].compare == 0xffffffff || (ipvip[i].mask == f.mask
		                                       && ipvip[i].compare == f.compare))
			break;		// free spot
	if (i == numipvips)
	{
		if (numipvips == MAX_IPFILTERS)
		{
			Con_Printf ("VIP spectator IP list is full\n");
			return;
		}
		numipvips++;
	}

	ipvip[i] = f;
	ipvip[i].level = l;
}

/*
=================
SV_RemoveIPVIP_f
=================
*/
static void SV_RemoveIPVIP_f (void)
{
	ipfilter_t	f;
	int		i, j;

	if (!StringToFilter (Cmd_Argv(1), &f))
	{
		Con_Printf ("Bad filter address: %s\n", Cmd_Argv(1));
		return;
	}
	for (i=0 ; i<numipvips ; i++)
		if (ipvip[i].mask == f.mask
		        && ipvip[i].compare == f.compare)
		{
			for (j=i+1 ; j<numipvips ; j++)
				ipvip[j-1] = ipvip[j];
			numipvips--;
			Con_Printf ("Removed.\n");
			return;
		}
	Con_Printf ("Didn't find %s.\n", Cmd_Argv(1));
}

/*
=================
SV_ListIP_f
=================
*/
static void SV_ListIPVIP_f (void)
{
	int		i;
	byte	b[4];

	Con_Printf ("VIP list:\n");
	for (i=0 ; i<numipvips ; i++)
	{
		*(unsigned *)b = ipvip[i].compare;
		Con_Printf ("%3i.%3i.%3i.%3i   level %d\n", b[0], b[1], b[2], b[3], ipvip[i].level);
	}
}

/*
=================
SV_WriteIPVIP_f
=================
*/
static void SV_WriteIPVIP_f (void)
{
	FILE	*f;
	char	name[MAX_OSPATH];
	byte	b[4];
	int		i;

	snprintf (name, MAX_OSPATH, "%s/vip_ip.cfg", fs_gamedir);

	Con_Printf ("Writing %s.\n", name);

	f = fopen (name, "wb");
	if (!f)
	{
		Con_Printf ("Couldn't open %s\n", name);
		return;
	}

	for (i=0 ; i<numipvips ; i++)
	{
		*(unsigned *)b = ipvip[i].compare;
		fprintf (f, "vip_addip %i.%i.%i.%i %d\n", b[0], b[1], b[2], b[3], ipvip[i].level);
	}

	fclose (f);

	// force cache rebuild.
	FS_FlushFSHash();
}


/*
=================
SV_AddIP_f
=================
*/
static void SV_AddIP_f (void)
{
	int		i;
	double	t = 0;
	char	*s;
	time_t	long_time = time(NULL);
	ipfilter_t f;
	ipfiltertype_t ipft = ipft_ban; // default is ban

	if (!StringToFilter (Cmd_Argv(1), &f) || f.compare == 0)
	{
		Con_Printf ("Bad filter address: %s\n", Cmd_Argv(1));
		return;
	}

	s = Cmd_Argv(2);
	if ( !s[0] || !strcmp(s, "ban"))
		ipft = ipft_ban;
	else if (!strcmp(s, "safe"))
		ipft = ipft_safe;
	else {
		Con_Printf ("Wrong filter type %s, use ban or safe\n", Cmd_Argv(2));
		return;
	}

	s = Cmd_Argv(3);
	if (long_time > 0) {
		if (*s == '+')     // "addip 127.0.0.1 ban +10" will ban for 10 seconds from current time
			s++;
		else
			long_time = 0; // "addip 127.0.0.1 ban 1234567" will ban for some seconds since 00:00:00 GMT, January 1, 1970

		t = (sscanf(s, "%lf", &t) == 1) ? t + long_time : 0;
	}

	f.time = t;
	f.type = ipft;

	for (i=0 ; i<numipfilters ; i++)
		if (ipfilters[i].compare == 0xffffffff || (ipfilters[i].mask == f.mask
		        && ipfilters[i].compare == f.compare))
			break;		// free spot
	if (i == numipfilters)
	{
		if (numipfilters == MAX_IPFILTERS)
		{
			Con_Printf ("IP filter list is full\n");
			return;
		}
		numipfilters++;
	}

	ipfilters[i] = f;
}

/*
=================
SV_RemoveIP_f
=================
*/
static void SV_RemoveIP_f (void)
{
	ipfilter_t	f;
	int			i, j;

	if (!StringToFilter (Cmd_Argv(1), &f))
	{
		Con_Printf ("Bad filter address: %s\n", Cmd_Argv(1));
		return;
	}

	for (i=0 ; i<numipfilters ; i++)
		if (ipfilters[i].mask == f.mask
		        && ipfilters[i].compare == f.compare)
		{
			for (j=i+1 ; j<numipfilters ; j++)
				ipfilters[j-1] = ipfilters[j];
			numipfilters--;
			Con_Printf ("Removed.\n");
			return;
		}
	Con_Printf ("Didn't find %s.\n", Cmd_Argv(1));
}

/*
=================
SV_ListIP_f
=================
*/
static void SV_ListIP_f (void)
{
	time_t	long_time = time(NULL);
	int		i;
	byte	b[4];

	Con_Printf ("Filter list:\n");
	for (i=0 ; i<numipfilters ; i++)
	{
		*(unsigned *)b = ipfilters[i].compare;
		Con_Printf ("%3i.%3i.%3i.%3i | ", b[0], b[1], b[2], b[3]);
		switch((int)ipfilters[i].type){
			case ipft_ban:  Con_Printf (" ban"); break;
			case ipft_safe: Con_Printf ("safe"); break;
			default: Con_Printf ("unkn"); break;
		}
		if (ipfilters[i].time)
			Con_Printf (" | %i s", (int)(ipfilters[i].time-long_time));
		Con_Printf ("\n");
	}
}

/*
=================
SV_WriteIP_f
=================
*/
static void SV_WriteIP_f (void)
{
	FILE	*f;
	char	name[MAX_OSPATH], *s;
	byte	b[4];
	int		i;

	snprintf (name, MAX_OSPATH, "%s/listip.cfg", fs_gamedir);

	Con_Printf ("Writing %s.\n", name);

	f = fopen (name, "wb");
	if (!f)
	{
		Con_Printf ("Couldn't open %s\n", name);
		return;
	}

	// write safe filters first
	for (i=0 ; i<numipfilters ; i++)
	{
		if(ipfilters[i].type != ipft_safe)
			continue;

		*(unsigned *)b = ipfilters[i].compare;
		fprintf (f, "addip %i.%i.%i.%i safe %.0f\n", b[0], b[1], b[2], b[3], ipfilters[i].time);
	}

	for (i=0 ; i<numipfilters ; i++)
	{
		if(ipfilters[i].type == ipft_safe)
			continue; // ignore safe, we already save it

		switch((int)ipfilters[i].type){
			case ipft_ban:  s = " ban"; break;
			case ipft_safe: s = "safe"; break;
			default: s = "unkn"; break;
		}
		*(unsigned *)b = ipfilters[i].compare;
		fprintf (f, "addip %i.%i.%i.%i %s %.0f\n", b[0], b[1], b[2], b[3], s, ipfilters[i].time);
	}

	fclose (f);

	// force cache rebuild.
	FS_FlushFSHash();
}

/*
=================
SV_SendBan
=================
*/
void SV_SendBan (void)
{
	char		data[128];

	data[0] = data[1] = data[2] = data[3] = 0xff;
	data[4] = A2C_PRINT;
	data[5] = 0;
	strlcat (data, "\nbanned.\n", sizeof(data));

	NET_SendPacket (NS_SERVER, strlen(data), data, net_from);
}

/*
=================
SV_FilterPacket
=================
*/
qbool SV_FilterPacket (void)
{
	int		i;
	unsigned	in;

	in = *(unsigned *)net_from.ip;

	for (i=0 ; i<numipfilters ; i++)
		if ( ipfilters[i].type == ipft_ban && (in & ipfilters[i].mask) == ipfilters[i].compare )
			return (int)filterban.value;

	return !(int)filterban.value;
}

// { server internal BAN support

#define AF_REAL_ADMIN    (1<<1)    // pass/vip granted admin.

void Do_BanList(ipfiltertype_t ipft)
{
	time_t	long_time = time(NULL);
	int		i;
	byte	b[4];

	for (i=0 ; i<numipfilters ; i++)
	{
		if (ipfilters[i].type != ipft)
			continue;

		*(unsigned *)b = ipfilters[i].compare;
		Con_Printf ("%3i|%3i.%3i.%3i.%3i", i, b[0], b[1], b[2], b[3]);
		switch((int)ipfilters[i].type){
			case ipft_ban:  Con_Printf ("| ban"); break;
			case ipft_safe: Con_Printf ("|safe"); break;
			default: Con_Printf ("|unkn"); break;
		}

		if (ipfilters[i].time) {
			long df = ipfilters[i].time-long_time;
			long d, h, m, s;
			d = df / (60*60*24);
			df -= d * 60*60*24;
			h = df / (60*60);
			df -= h * 60*60;
			m = df /  60;
			df -= m * 60;
			s = df;

			if (d)
				Con_Printf ("|%4ldd:%2ldh", d, h);
			else if (h)
				Con_Printf ("|%4ldh:%2ldm", h, m);
			else
				Con_Printf ("|%4ldm:%2lds", m, s);
		}
		else
			Con_Printf ("|permanent");
		Con_Printf ("\n");
	}
}

void SV_BanList (void)
{
	unsigned char blist[64] = "Ban list:", id[64] = "id", ipmask[64] = "ip mask", type[64] = "type", expire[64] = "expire";

	if (numipfilters < 1) {
		Con_Printf ("Ban list: empty\n");
		return;
	}

	Con_Printf ("%s\n"
				"\235\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236"
				"\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\237\n"
				"%3.3s|%15.15s|%4.4s|%9.9s\n",
				Q_redtext(blist), Q_redtext(id), Q_redtext(ipmask), Q_redtext(type), Q_redtext(expire));

	Do_BanList(ipft_safe);
	Do_BanList(ipft_ban);
}

qbool SV_CanAddBan (ipfilter_t *f)
{
	int i;

	if (f->compare == 0)
		return false;

	for (i=0 ; i<numipfilters ; i++)
		if (ipfilters[i].mask == f->mask && ipfilters[i].compare == f->compare && ipfilters[i].type == ipft_safe)
			return false; // can't add filter f because present "safe" filter

	return true;
}

void SV_RemoveBansIPFilter (int i)
{
	for (; i + 1 < numipfilters; i++)
		ipfilters[i] = ipfilters[i + 1];

	numipfilters--;
}

void SV_CleanBansIPList (void)
{
	time_t	long_time = time(NULL);
	int     i;

	if (sv.state != ss_active)
		return;

	for (i = 0; i < numipfilters;)
	{
		if (ipfilters[i].time && ipfilters[i].time <= long_time)
		{
			SV_RemoveBansIPFilter (i);
		}
		else
			i++;
	}
}

void SV_Cmd_Ban_f(void)
{
	edict_t	*ent;
	eval_t *val;
	double		d;
	int			i, j, t;
	client_t	*cl;
	ipfilter_t  f;
	int			uid;
	int			c;
	char		reason[80] = "", arg2[32], arg2c[sizeof(arg2)], *s;

	// set up the edict
	ent = sv_client->edict;

// ============
// get ADMIN rights from MOD via "mod_admin" field, mod MUST export such field if wanna server ban support
// ============

	val = PR_GetEdictFieldValue(ent, "mod_admin");
	if (!val || !(val->_int & AF_REAL_ADMIN) ) {
		Con_Printf("You are not an admin\n");
		return;
	}

	c = Cmd_Argc ();
	if (c < 3)
	{
		Con_Printf("usage: cmd ban <id/nick> <time<s m h d>> [reason]\n");
		return;
	}

	uid = Q_atoi(Cmd_Argv(1));

	strlcpy(arg2, Cmd_Argv(2), sizeof(arg2));

	// sscanf safe here since sizeof(arg2) == sizeof(arg2c), right?
	if (sscanf(arg2, "%d%s", &t, arg2c) != 2 || strlen(arg2c) != 1) {
		Con_Printf("ban: wrong time arg\n");
		return;
	}

	d = t = bound(0, t, 999);
	switch(arg2c[0]) {
		case 's': break; // seconds is seconds
		case 'm': d *= 60; break; // 60 seconds per minute
		case 'h': d *= 60*60; break; // 3600 seconds per hour
		case 'd': d *= 60*60*24; break; // 86400 seconds per day
		default:
		Con_Printf("ban: wrong time arg\n");
		return;
	}

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
	{
		if (!cl->state)
			continue;
		if (cl->userid == uid || !strcmp(Cmd_Argv(1), cl->name))
		{
			if (c > 3) // serve reason arguments
			{
				strlcpy (reason, " (", sizeof(reason));
				for (j=3 ; j<c; j++)
				{
					strlcat (reason, Cmd_Argv(j), sizeof(reason)-4);
					if (j < c-1)
						strlcat (reason, " ", sizeof(reason)-4);
				}
				if (strlen(reason) < 3)
					reason[0] = '\0';
				else
					strlcat (reason, ")", sizeof(reason));
			}

			s = NET_BaseAdrToString(cl->netchan.remote_address);
			if (!StringToFilter (s, &f))
			{
				Con_Printf ("ban: bad ip address: %s\n", s);
				return;
			}

			if (!SV_CanAddBan(&f))
			{
				Con_Printf ("ban: can't ban such ip: %s\n", s);
				return;
			}

			SV_BroadcastPrintf (PRINT_HIGH, "%s was banned for %d%s%s\n", cl->name, t, arg2c, reason);

			Cbuf_AddText(va("addip %s ban %s%.0lf\n", s, d ? "+" : "", d));
			Cbuf_AddText("writeip\n");
			return;
		}
	}

	Con_Printf ("Couldn't find user %s\n", Cmd_Argv(1));
}

void SV_Cmd_Banip_f(void)
{
	edict_t	*ent;
	eval_t *val;
	byte	b[4];
	double		d;
	int			c, t;
	ipfilter_t  f;
	char		arg2[32], arg2c[sizeof(arg2)];

	// set up the edict
	ent = sv_client->edict;

// ============
// get ADMIN rights from MOD via "mod_admin" field, mod MUST export such field if wanna server ban support
// ============

	val = PR_GetEdictFieldValue(ent, "mod_admin");
	if (!val || !(val->_int & AF_REAL_ADMIN) ) {
		Con_Printf("You are not an admin\n");
		return;
	}

	c = Cmd_Argc ();
	if (c < 3)
	{
		Con_Printf("usage: cmd banip <ip> <time<s m h d>>\n");
		return;
	}

	if (!StringToFilter (Cmd_Argv(1), &f))
	{
		Con_Printf ("ban: bad ip address: %s\n", Cmd_Argv(1));
		return;
	}

	if (!SV_CanAddBan(&f))
	{
		Con_Printf ("ban: can't ban such ip: %s\n", Cmd_Argv(1));
		return;
	}

	strlcpy(arg2, Cmd_Argv(2), sizeof(arg2));

	// sscanf safe here since sizeof(arg2) == sizeof(arg2c), right?
	if (sscanf(arg2, "%d%s", &t, arg2c) != 2 || strlen(arg2c) != 1) {
		Con_Printf("ban: wrong time arg\n");
		return;
	}

	d = t = bound(0, t, 999);
	switch(arg2c[0]) {
		case 's': break; // seconds is seconds
		case 'm': d *= 60; break; // 60 seconds per minute
		case 'h': d *= 60*60; break; // 3600 seconds per hour
		case 'd': d *= 60*60*24; break; // 86400 seconds per day
		default:
		Con_Printf("ban: wrong time arg\n");
		return;
	}

	*(unsigned *)b = f.compare;
	SV_BroadcastPrintf (PRINT_HIGH, "%3i.%3i.%3i.%3i was banned for %d%s\n", b[0], b[1], b[2], b[3], t, arg2c);

	Cbuf_AddText(va("addip %i.%i.%i.%i ban %s%.0lf\n", b[0], b[1], b[2], b[3], d ? "+" : "", d));
	Cbuf_AddText("writeip\n");
}

void SV_Cmd_Banremove_f(void)
{
	edict_t	*ent;
	eval_t *val;
	byte	b[4];
	int		id;

	// set up the edict
	ent = sv_client->edict;

// ============
// get ADMIN rights from MOD via "mod_admin" field, mod MUST export such field if wanna server ban support
// ============

	val = PR_GetEdictFieldValue(ent, "mod_admin");
	if (!val || !(val->_int & AF_REAL_ADMIN) ) {
		Con_Printf("You are not an admin\n");
		return;
	}

	if (Cmd_Argc () < 2)
	{
		Con_Printf("usage: cmd banrem [banid]\n");
		SV_BanList();
		return;
	}

	id = Q_atoi(Cmd_Argv(1));

	if (id < 0 || id >= numipfilters) {
		Con_Printf ("Wrong ban id: %d\n", id);
		return;
	}

	if (ipfilters[id].type == ipft_safe) {
		Con_Printf ("Can't remove such ban with id: %d\n", id);
		return;
	}

	*(unsigned *)b = ipfilters[id].compare;
	SV_BroadcastPrintf (PRINT_HIGH, "%3i.%3i.%3i.%3i was unbanned\n", b[0], b[1], b[2], b[3]);

	SV_RemoveBansIPFilter (id);
	Cbuf_AddText("writeip\n");
}

// } server internal BAN support

/*
=================
SV_VIPbyIP
=================
*/
int SV_VIPbyIP (netadr_t adr)
{
	int		i;
	unsigned	in;

	in = *(unsigned *)adr.ip;

	for (i=0 ; i<numipvips ; i++)
		if ( (in & ipvip[i].mask) == ipvip[i].compare)
			return ipvip[i].level;

	return 0;
}

/*
=================
SV_VIPbyPass
=================
*/
int SV_VIPbyPass (char *pass)
{
	qbool use_value = false;
	int vip_value[MAX_ARGS];
	int i;

	if (!vip_password.string[0] || !strcasecmp(vip_password.string, "none"))
		return 0;

	if (vip_values.string[0]) {
		use_value = true;
		// 2VVD: vip_password count may be not equal vip_values count, what we must do in this case?
		memset((void*)vip_value, 0, sizeof(vip_value));
		Cmd_TokenizeString(vip_values.string);
		for (i = 0; i < Cmd_Argc(); i++)
			vip_value[i] = atoi(Cmd_Argv(i));
	}

	Cmd_TokenizeString(vip_password.string);

	for (i = 0; i < Cmd_Argc(); i++)
		if (!strcmp(Cmd_Argv(i), pass) && strcasecmp(Cmd_Argv(i), "none"))
			return (use_value ? vip_value[i] : i+1);

	return 0;
}

static char *DecodeArgs(char *args)
{
	static char string[1024];
	char *p, key[32], *s, *value, ch, tmp_value[512];

	string[0] = 0;
	p = string;

	while (*args)
	{
		// skip whitespaces
		while (*args && *args <= 32)
			*p++ = *args++;

		if (*args == '\"')
		{
			do *p++ = *args++; while (*args && *args != '\"');
			*p++ = '\"';
			if (*args)
				args++;
		}
		else if (*args == '@' || *args == '$')
		{
			// get the key and read value from localinfo
			ch = *args;
			s = key;
			args++;
			while (*args > 32)
				*s++ = *args++;
			*s = 0;

			if ((value = Info_ValueForKey (svs.info, key)) == NULL || !*value)
				value = Info_Get(&_localinfo_, key);

			if (ch == '$' && value)
			{
				strlcpy(tmp_value, value, sizeof(tmp_value));
				Q_normalizetext(tmp_value);
				value = tmp_value;
			}

			*p++ = '\"';
			if (value)
			{
				while (*value)
					*p++ = *value++;
			}
			*p++ = '\"';
		}
		else {
			while (*args > 32) {
				*p++ = *args++;
			}
		}
	}

	*p = 0;

	return string;
}

void SV_Script_f (void)
{
	char *path, *p;
	extern redirect_t sv_redirected;

	if (Cmd_Argc() < 2)
	{
		Con_Printf("usage: script <path> [<args>]\n");
		return;
	}

	path = Cmd_Argv(1);

	//bliP: 24/9 need subdirs here ->
	if (!strncmp(path, "../", 3) || !strncmp(path, "..\\", 3))
		path += 3;

	if (strstr(path, ".."))
	{
		Con_Printf("Invalid path.\n");
		return;
	}
	//<-

	path = Cmd_Argv(1);

	p = Cmd_Args();
	while (*p > 32)
		p++;
	while (*p && *p <= 32)
		p++;

	p = DecodeArgs(p);

	if (sv_redirected != RD_MOD)
		Sys_Printf("Running %s.qws\n", path);

	Sys_Script(path, va("%d %s", sv_redirected, p));

}

//============================================================================

//bliP: cuff, mute ->
void SV_RemoveIPFilter (int i)
{
	for (; i + 1 < numpenfilters; i++)
		penfilters[i] = penfilters[i + 1];

	numpenfilters--;
}

static void SV_CleanIPList (void)
{
	int     i;

	if (sv.state != ss_active)
		return;

	for (i = 0; i < numpenfilters;)
	{
		if (penfilters[i].time && (penfilters[i].time <= realtime))
		{
			SV_RemoveIPFilter (i);
		}
		else
			i++;
	}
}

static qbool SV_IPCompare (byte *a, byte *b)
{
	int i;

	for (i = 0; i < 1; i++)
		if (((unsigned int *)a)[i] != ((unsigned int *)b)[i])
			return false;

	return true;
}

static void SV_IPCopy (byte *dest, byte *src)
{
	int i;

	for (i = 0; i < 1; i++)
		((unsigned int *)dest)[i] = ((unsigned int *)src)[i];
}

void SV_SavePenaltyFilter (client_t *cl, filtertype_t type, double pentime)
{
	int i;

	if (pentime < curtime)   // no point
		return;

	for (i = 0; i < numpenfilters; i++)
		if (SV_IPCompare (penfilters[i].ip, cl->realip.ip)	&& penfilters[i].type == type)
		{
			return;
		}

	if (numpenfilters == MAX_IPFILTERS)
	{
		return;
	}

	SV_IPCopy (penfilters[numpenfilters].ip, cl->realip.ip);
	penfilters[numpenfilters].time = pentime;
	penfilters[numpenfilters].type = type;
	numpenfilters++;
}

double SV_RestorePenaltyFilter (client_t *cl, filtertype_t type)
{
	int i;
	double time1 = 0.0;

	// search for existing penalty filter of same type
	for (i = 0; i < numpenfilters; i++)
	{
		if (type == penfilters[i].type && SV_IPCompare (cl->realip.ip, penfilters[i].ip))
		{
			time1 = penfilters[i].time;
			SV_RemoveIPFilter (i);
			return time1;
		}
	}
	return time1;
}
//<-

//============================================================================

/*
=================
SV_ReadPackets
=================
*/
static void SV_ReadPackets (void)
{
	client_t *cl;
	int qport;
	int i;

	if (sv.state != ss_active)
		return;

	// first deal with delayed packets from connected clients
	for (i = 0, cl=svs.clients; i < MAX_CLIENTS; i++, cl++)
	{
		if (cl->state == cs_free)
			continue;

		net_from = cl->netchan.remote_address;

		while (cl->packets && (realtime - cl->packets->time >= cl->delay || sv.paused))
		{
			SZ_Clear(&net_message);
			SZ_Write(&net_message, cl->packets->msg.data, cl->packets->msg.cursize);
			SV_ExecuteClientMessage(cl);
			SV_FreeHeadDelayedPacket(cl);
		}
	}

	// now deal with new packets
	while (NET_GetPacket(NS_SERVER))
	{
		if (SV_FilterPacket ())
		{
			SV_SendBan ();	// tell them we aren't listening...
			continue;
		}

		// check for connectionless packet (0xffffffff) first
		if (*(int *)net_message.data == -1)
		{
			SV_ConnectionlessPacket ();
			continue;
		}

		// read the qport out of the message so we can fix up
		// stupid address translating routers
		MSG_BeginReading ();
		MSG_ReadLong (); // sequence number
		MSG_ReadLong (); // sequence number
		qport = MSG_ReadShort () & 0xffff;

		// check which client sent this packet
		for (i=0, cl=svs.clients ; i<MAX_CLIENTS ; i++,cl++)
		{
			if (cl->state == cs_free)
				continue;
			if (!NET_CompareBaseAdr (net_from, cl->netchan.remote_address))
				continue;
			if (cl->netchan.qport != qport)
				continue;
			if (cl->netchan.remote_address.port != net_from.port)
			{
				Con_DPrintf ("SV_ReadPackets: fixing up a translated port\n");
				cl->netchan.remote_address.port = net_from.port;
			}

			break;
		}

		if (i == MAX_CLIENTS)
			continue;

		// ok, we know who sent this packet, but do we need to delay executing it?
		if (cl->delay > 0)
		{
			if (!svs.free_packets) // packet has to be dropped..
				break;

			// insert at end of list
			if (!cl->packets) {
				cl->last_packet = cl->packets = svs.free_packets;
			} else {
				// this works because '=' associates from right to left
				cl->last_packet = cl->last_packet->next = svs.free_packets;
			}

			svs.free_packets = svs.free_packets->next;
			cl->last_packet->next = NULL;

			cl->last_packet->time = realtime;
			SZ_Clear(&cl->last_packet->msg);
			SZ_Write(&cl->last_packet->msg, net_message.data, net_message.cursize);
		}
		else
		{
			SV_ExecuteClientMessage (cl);
		}
	}
}


/*
==================
SV_CheckTimeouts

If a packet has not been received from a client in timeout.value
seconds, drop the conneciton.

When a client is normally dropped, the client_t goes into a zombie state
for a few seconds to make sure any final reliable message gets resent
if necessary
==================
*/
static void SV_CheckTimeouts (void)
{
	int i, nclients;
	float droptime;
	client_t *cl;

	if (sv.state != ss_active)
		return;

	droptime = curtime - timeout.value;
	nclients = 0;

	for (i=0,cl=svs.clients ; i<MAX_CLIENTS ; i++,cl++)
	{
#ifdef USE_PR2
		if( cl->isBot )
			continue;
#endif
		if (cl->state >= cs_preconnected /*|| cl->state == cs_spawned*/)
		{
			if (!cl->spectator)
				nclients++;
			if (cl->netchan.last_received < droptime)
			{
				SV_BroadcastPrintf (PRINT_HIGH, "%s timed out\n", cl->name);
				SV_DropClient (cl);
				cl->state = cs_free;	// don't bother with zombie state
			}
			if (!cl->logged && !cl->logged_in_via_web) {
				SV_LoginCheckTimeOut(cl);
			}
		}
		if (cl->state == cs_zombie && SV_ClientConnectedTime(cl) > zombietime.value)
		{
			cl->state = cs_free;	// can now be reused
		}
	}
	if ((sv.paused & 1) && !nclients)
	{
		// nobody left, unpause the server
		if (GE_ShouldPause) {
			pr_global_struct->time = sv.time;
			pr_global_struct->self = EDICT_TO_PROG(sv.edicts);
			G_FLOAT(OFS_PARM0) = 0 /* newstate = false */;
			PR_ExecuteProgram (GE_ShouldPause);
			if (!G_FLOAT(OFS_RETURN))
				return;		// progs said don't unpause
		}
		SV_TogglePause("Pause released since no players are left.\n", 1);
	}
}

#ifdef SERVERONLY
/*
===================
SV_GetConsoleCommands

Add them exactly as if they had been typed at the console
===================
*/
static void SV_GetConsoleCommands (void)
{
	char	*cmd;

	while (1)
	{
		cmd = Sys_ConsoleInput ();
		if (!cmd)
			break;
		Cbuf_AddText (cmd);
		Cbuf_AddText ("\n");
	}
}
#endif

/*
===================
SV_BoundRate
===================
*/
int SV_BoundRate (qbool dl, int rate)
{
	if (!rate)
		rate = 2500;
	if (dl)
	{
		if (!(int)sv_maxdownloadrate.value && (int)sv_maxrate.value && rate > (int)sv_maxrate.value)
			rate = (int)sv_maxrate.value;

		if (sv_maxdownloadrate.value && rate > sv_maxdownloadrate.value)
			rate = (int)sv_maxdownloadrate.value;
	}
	else
		if ((int)sv_maxrate.value && rate > (int) sv_maxrate.value)
			rate = (int)sv_maxrate.value;

	if (rate < 500)
		rate = 500;

	if (rate > 100000 * MAX_DUPLICATE_PACKETS)
		rate = 100000 * MAX_DUPLICATE_PACKETS;

	return rate;
}


/*
===================
SV_CheckVars

===================
*/

static void SV_CheckVars (void)
{
	static char pw[MAX_KEY_STRING] = {0}, spw[MAX_KEY_STRING] = {0}, vspw[MAX_KEY_STRING]= {0};
	static float old_maxrate = 0, old_maxdlrate = 0;
	int v;

	if (sv.state != ss_active)
		return;

	// check password and spectator_password
	if (strcmp(password.string, pw) ||
		strcmp(spectator_password.string, spw) || strcmp(vip_password.string, vspw))
	{
		strlcpy (pw, password.string, sizeof(pw));
		strlcpy (spw, spectator_password.string, sizeof(spw));
		strlcpy (vspw, vip_password.string, sizeof(vspw));
		Cvar_Set (&password, pw);
		Cvar_Set (&spectator_password, spw);
		Cvar_Set (&vip_password, vspw);

		v = 0;
		if (pw[0] && strcmp(pw, "none"))
			v |= 1;
		if (spw[0] && strcmp(spw, "none"))
			v |= 2;
		if (vspw[0] && strcmp(vspw, "none"))
			v |= 4;

		Con_DPrintf ("Updated needpass.\n");
		if (!v)
			Info_SetValueForKey (svs.info, "needpass", "", MAX_SERVERINFO_STRING);
		else
			Info_SetValueForKey (svs.info, "needpass", va("%i",v), MAX_SERVERINFO_STRING);
	}

	// check sv_maxrate
	if ((int)sv_maxrate.value != old_maxrate || (int)sv_maxdownloadrate.value != old_maxdlrate )
	{
		client_t	*cl;
		int			i;
		char		*val;

		old_maxrate = (int)sv_maxrate.value;
		old_maxdlrate = (int)sv_maxdownloadrate.value;

		for (i=0, cl = svs.clients ; i<MAX_CLIENTS ; i++, cl++)
		{
			if (cl->state < cs_preconnected)
				continue;

			val = Info_Get (&cl->_userinfo_ctx_, cl->download ? "drate" : "rate");
			cl->netchan.rate = 1.0 / SV_BoundRate (cl->download != NULL, Q_atoi(*val ? val : "99999"));
		}
	}
}

static void PausedTic (void)
{
	if (sv.state != ss_active)
		return;

	PR_PausedTic(Sys_DoubleTime() - sv.pausedsince);
}

/*
==================
SV_Frame

==================
*/
void SV_Map (qbool now);
void SV_Frame (double time1)
{
	static double start, end;
	double demo_start, demo_end;

	start = Sys_DoubleTime ();
	svs.stats.idle += start - end;

	// keep the random time dependent
	rand ();

	// decide the simulation time
	if (!sv.paused)
	{
		realtime += time1;
		sv.time += time1;
	}

	// check timeouts
	SV_CheckTimeouts ();

	//bliP: cuff, mute ->
	// clean out expired cuffs/mutes
	SV_CleanIPList ();
	//<-

	// clean out bans
	SV_CleanBansIPList ();

	// toggle the log buffer if full
	SV_CheckLog ();

	SV_MVDStream_Poll();

#ifdef SERVERONLY
	// check for commands typed to the host
	SV_GetConsoleCommands ();

	// process console commands
	Cbuf_Execute ();
#endif

	// check for map change;
	SV_Map(true);

	SV_CheckVars ();

	// get packets
	SV_ReadPackets ();

	// move autonomous things around if enough time has passed
	if (!sv.paused) {
		SV_Physics();
#ifdef USE_PR2
		SV_RunBots();
#endif
	}
	else
		PausedTic ();

	// send messages back to the clients that had packets read this frame
	SV_SendClientMessages ();

#if defined(SERVERONLY) && defined(WWW_INTEGRATION)
	Central_ProcessResponses();
#endif

	demo_start = Sys_DoubleTime ();
	SV_SendDemoMessage();
	demo_end = Sys_DoubleTime ();
	svs.stats.demo += demo_end - demo_start;

	// send a heartbeat to the master if needed
	Master_Heartbeat ();

	// collect timing statistics
	end = Sys_DoubleTime ();
	svs.stats.active += end-start;
	if (++svs.stats.count == STATFRAMES)
	{
		svs.stats.latched_active = svs.stats.active;
		svs.stats.latched_idle = svs.stats.idle;
		svs.stats.latched_packets = svs.stats.packets;
		svs.stats.latched_demo = svs.stats.demo;
		svs.stats.active = 0;
		svs.stats.idle = 0;
		svs.stats.packets = 0;
		svs.stats.count = 0;
		svs.stats.demo = 0;
	}
}

/*
===============
SV_InitLocal
===============
*/
void SV_InitLocal (void)
{
	int		i;
	char	cmd_line[1024] = {0};

	extern	cvar_t	sv_maxvelocity;
	extern	cvar_t	sv_gravity;
	extern	cvar_t	sv_stopspeed;
	extern	cvar_t	sv_spectatormaxspeed;
	extern	cvar_t	sv_accelerate;
	extern	cvar_t	sv_airaccelerate;
	extern	cvar_t	sv_wateraccelerate;
	extern	cvar_t	sv_friction;
	extern	cvar_t	sv_waterfriction;
	extern	cvar_t	sv_nailhack;

	extern cvar_t	sv_maxpitch;
	extern cvar_t	sv_minpitch;

	extern	cvar_t	pm_airstep;
	extern	cvar_t	pm_pground;
	extern  cvar_t  pm_rampjump;
	extern	cvar_t	pm_slidefix;
	extern	cvar_t	pm_ktjump;
	extern	cvar_t	pm_bunnyspeedcap;

	// qws = QuakeWorld Server information
	static cvar_t qws_name = { "qws_name", SERVER_NAME, CVAR_ROM };
	static cvar_t qws_fullname = { "qws_fullname", SERVER_FULLNAME, CVAR_ROM };
	static cvar_t qws_version = { "qws_version", SERVER_VERSION, CVAR_ROM };
	static cvar_t qws_buildnum = { "qws_buildnum", "unknown", CVAR_ROM };
	static cvar_t qws_platform = { "qws_platform", QW_PLATFORM_SHORT, CVAR_ROM };
	static cvar_t qws_builddate = { "qws_builddate", BUILD_DATE, CVAR_ROM };
	static cvar_t qws_homepage = { "qws_homepage", SERVER_HOME_URL, CVAR_ROM };
	// qwm = QuakeWorld Mod information placeholders
	static cvar_t qwm_name = { "qwm_name", "" };
	static cvar_t qwm_fullname = { "qwm_fullname", "" };
	static cvar_t qwm_version = { "qwm_version", "" };
	static cvar_t qwm_buildnum = { "qwm_buildnum", "" };
	static cvar_t qwm_platform = { "qwm_platform", "" };
	static cvar_t qwm_builddate = { "qwm_builddate", "" };
	static cvar_t qwm_homepage = { "qwm_homepage", "" };

	packet_t *packet_freeblock; // initialise delayed packet free block

	SV_InitOperatorCommands	();
	SV_UserInit ();

	Cvar_Register (&sv_getrealip);
	Cvar_Register (&sv_maxdownloadrate);
	Cvar_Register (&sv_serverip);
	Cvar_Register (&sv_forcespec_onfull);

#ifdef SERVERONLY
	Cvar_Register (&rcon_password);
	Cvar_Register (&password);
#endif

	Cvar_Register (&sv_hashpasswords);
	//Added by VVD {
	Cvar_Register (&sv_crypt_rcon);
	Cvar_Register (&sv_timestamplen);
	Cvar_Register (&sv_rconlim);

	Cvar_Register (&telnet_log_level);

	Cvar_Register (&frag_log_type);
	Cvar_Register (&qconsole_log_say);
	Cvar_Register (&sv_use_dns);

	for (i = 0; i < COM_Argc(); i++)
	{
		if (i)
			strlcat(cmd_line, " ", sizeof(cmd_line));
		strlcat(cmd_line, COM_Argv(i), sizeof(cmd_line));
	}
	Cvar_Register (&sys_command_line);
	Cvar_SetROM(&sys_command_line, cmd_line);

	//Added by VVD }
	Cvar_Register (&spectator_password);
	Cvar_Register (&vip_password);
	Cvar_Register (&vip_values);

	Cvar_Register (&sv_nailhack);

	Cvar_Register (&sv_mintic);
	Cvar_Register (&sv_maxtic);
	Cvar_Register (&sv_maxfps);
	Cvar_Register (&sys_select_timeout);
	Cvar_Register (&sys_restart_on_error);

	Cvar_Register (&sv_maxpitch);
	Cvar_Register (&sv_minpitch);

	Cvar_Register (&skill);
	Cvar_Register (&coop);

	Cvar_Register (&fraglimit);
	Cvar_Register (&timelimit);
	Cvar_Register (&teamplay);
	Cvar_Register (&samelevel);
	Cvar_Register (&maxclients);
	Cvar_Register (&maxspectators);
	Cvar_Register (&maxvip_spectators);
	Cvar_Register (&hostname);
	Cvar_Register (&deathmatch);
	Cvar_Register (&watervis);
	Cvar_Register (&serverdemo);
	Cvar_Register (&sv_paused);

	Cvar_Register (&timeout);
	Cvar_Register (&zombietime);

	Cvar_Register (&sv_maxvelocity);
	Cvar_Register (&sv_gravity);
	Cvar_Register (&sv_stopspeed);
	Cvar_Register (&sv_maxspeed);
	Cvar_Register (&sv_spectatormaxspeed);
	Cvar_Register (&sv_accelerate);
	Cvar_Register (&sv_airaccelerate);
	Cvar_Register (&sv_wateraccelerate);
	Cvar_Register (&sv_friction);
	Cvar_Register (&sv_waterfriction);

	Cvar_Register (&sv_antilag);
	Cvar_Register (&sv_antilag_no_pred);
	Cvar_Register (&sv_antilag_projectiles);

	Cvar_Register (&pm_bunnyspeedcap);
	Cvar_Register (&pm_ktjump);
	Cvar_Register (&pm_slidefix);
	Cvar_Register (&pm_pground);
	Cvar_Register (&pm_airstep);
	Cvar_Register (&pm_rampjump);

	Cvar_Register (&filterban);

	Cvar_Register (&allow_download);
	Cvar_Register (&allow_download_skins);
	Cvar_Register (&allow_download_models);
	Cvar_Register (&allow_download_sounds);
	Cvar_Register (&allow_download_maps);
	Cvar_Register (&allow_download_pakmaps);
	Cvar_Register (&allow_download_demos);
	Cvar_Register (&allow_download_other);
	//bliP: init ->
	Cvar_Register (&download_map_url);

	Cvar_Register (&sv_specprint);
	Cvar_Register (&sv_admininfo);
	Cvar_Register (&sv_reconnectlimit);
	Cvar_Register (&sv_maxlogsize);
	//bliP: 24/9 ->
	Cvar_Register (&sv_logdir);
	Cvar_Register (&sv_speedcheck);
	Cvar_Register (&sv_unfake); // kickfake to unfake
	//<-
	Cvar_Register (&sv_kicktop);
	//<-
	Cvar_Register (&sv_allowlastscores);
//	Cvar_Register (&sv_highchars);
	Cvar_Register (&sv_phs);
	Cvar_Register (&pausable);
	Cvar_Register (&sv_maxrate);
	Cvar_Register (&sv_loadentfiles);
	Cvar_Register (&sv_loadentfiles_dir);
	Cvar_Register (&sv_default_name);
	Cvar_Register (&sv_mod_msg_file);
	Cvar_Register (&sv_forcenick);
	Cvar_Register (&sv_registrationinfo);
	Cvar_Register (&registered);

	Cvar_Register (&sv_halflifebsp);
	Cvar_Register (&sv_bspversion);
	Cvar_Register (&sv_serveme_fix);

#ifdef FTE_PEXT_FLOATCOORDS
	Cvar_Register (&sv_bigcoords);
#endif

	Cvar_Register (&sv_extlimits);
#ifdef MVD_PEXT1_SERVERSIDEWEAPON
	Cvar_Register (&sv_pext_mvdsv_serversideweapon);
#endif

	Cvar_Register (&sv_reliable_sound);

	Cvar_Register(&qws_name);
	Cvar_Register(&qws_fullname);
	Cvar_Register(&qws_version);
	if (GIT_COMMIT[0]) {
		qws_buildnum.string = GIT_COMMIT;
	}
	Cvar_Register(&qws_buildnum);
	Cvar_Register(&qws_platform);
	Cvar_Register(&qws_builddate);
	Cvar_Register(&qws_homepage);
	Cvar_Register(&qwm_name);
	Cvar_Register(&qwm_fullname);
	Cvar_Register(&qwm_version);
	Cvar_Register(&qwm_buildnum);
	Cvar_Register(&qwm_platform);
	Cvar_Register(&qwm_builddate);
	Cvar_Register(&qwm_homepage);

	Cvar_Register(&sv_mod_extensions);

// QW262 -->
	Cmd_AddCommand ("svadmin", SV_Admin_f);
// <-- QW262

	Cmd_AddCommand ("addip", SV_AddIP_f);
	Cmd_AddCommand ("removeip", SV_RemoveIP_f);
	Cmd_AddCommand ("listip", SV_ListIP_f);
	Cmd_AddCommand ("writeip", SV_WriteIP_f);
	Cmd_AddCommand ("vip_addip", SV_AddIPVIP_f);
	Cmd_AddCommand ("vip_removeip", SV_RemoveIPVIP_f);
	Cmd_AddCommand ("vip_listip", SV_ListIPVIP_f);
	Cmd_AddCommand ("vip_writeip", SV_WriteIPVIP_f);


	for (i=0 ; i<MAX_MODELS ; i++)
		snprintf (localmodels[i], MODEL_NAME_LEN, "*%i", i);

#ifdef FTE_PEXT_ACCURATETIMINGS
	svs.fteprotocolextensions |= FTE_PEXT_ACCURATETIMINGS;
#endif
#ifdef FTE_PEXT_256PACKETENTITIES
	svs.fteprotocolextensions |= FTE_PEXT_256PACKETENTITIES;
#endif
#ifdef FTE_PEXT_CHUNKEDDOWNLOADS
	svs.fteprotocolextensions |= FTE_PEXT_CHUNKEDDOWNLOADS;
#endif
#ifdef FTE_PEXT_FLOATCOORDS
	svs.fteprotocolextensions |= FTE_PEXT_FLOATCOORDS;
#endif
#ifdef FTE_PEXT_MODELDBL
	svs.fteprotocolextensions |= FTE_PEXT_MODELDBL;
#endif
#ifdef FTE_PEXT_ENTITYDBL
	svs.fteprotocolextensions |= FTE_PEXT_ENTITYDBL;
#endif
#ifdef FTE_PEXT_ENTITYDBL2
	svs.fteprotocolextensions |= FTE_PEXT_ENTITYDBL2;
#endif
#ifdef FTE_PEXT_SPAWNSTATIC2
	svs.fteprotocolextensions |= FTE_PEXT_SPAWNSTATIC2;
#endif
#ifdef FTE_PEXT_TRANS
    svs.fteprotocolextensions |= FTE_PEXT_TRANS;
#endif
#ifdef FTE_PEXT2_VOICECHAT
	svs.fteprotocolextensions2 |= FTE_PEXT2_VOICECHAT;
#endif

#ifdef MVD_PEXT1_FLOATCOORDS
	svs.mvdprotocolextension1 |= MVD_PEXT1_FLOATCOORDS;
#endif
#ifdef MVD_PEXT1_HIGHLAGTELEPORT
	svs.mvdprotocolextension1 |= MVD_PEXT1_HIGHLAGTELEPORT;
#endif
#ifdef MVD_PEXT1_SERVERSIDEWEAPON
	svs.mvdprotocolextension1 |= MVD_PEXT1_SERVERSIDEWEAPON;
#endif
#ifdef MVD_PEXT1_HIDDEN_MESSAGES
	svs.mvdprotocolextension1 |= MVD_PEXT1_HIDDEN_MESSAGES;
#endif
#ifdef MVD_PEXT1_SERVERSIDEWEAPON2
	svs.mvdprotocolextension1 |= MVD_PEXT1_SERVERSIDEWEAPON2;
#endif

	Info_SetValueForStarKey (svs.info, "*version", SERVER_NAME " " SERVER_VERSION, MAX_SERVERINFO_STRING);
	Info_SetValueForStarKey (svs.info, "*z_ext", va("%i", SERVER_EXTENSIONS), MAX_SERVERINFO_STRING);

	// init fraglog stuff
	svs.logsequence = 1;
	svs.logtime = realtime;
	svs.log[0].data = svs.log_buf[0];
	svs.log[0].maxsize = sizeof(svs.log_buf[0]);
	svs.log[0].cursize = 0;
	svs.log[0].allowoverflow = true;
	svs.log[1].data = svs.log_buf[1];
	svs.log[1].maxsize = sizeof(svs.log_buf[1]);
	svs.log[1].cursize = 0;
	svs.log[1].allowoverflow = true;

	packet_freeblock = (packet_t *) Hunk_AllocName (MAX_DELAYED_PACKETS * sizeof(packet_t), "delayed_packets");

	for (i = 0; i < MAX_DELAYED_PACKETS; i++) {
		SZ_Init (&packet_freeblock[i].msg, packet_freeblock[i].buf, sizeof(packet_freeblock[i].buf));
		packet_freeblock[i].next = &packet_freeblock[i + 1];
	}
	packet_freeblock[MAX_DELAYED_PACKETS - 1].next = NULL;
	svs.free_packets = &packet_freeblock[0];
}


//============================================================================

/*
=================
SV_ExtractFromUserinfo

Pull specific info from a newly changed userinfo string
into a more C freindly form.
=================
*/
void SV_ExtractFromUserinfo (client_t *cl, qbool namechanged)
{
	char	*val, *p;
	int		i;
	client_t	*client;
	int		dupc = 1;
	char	newname[CLIENT_NAME_LEN];

	if (namechanged)
	{
		// name for C code
		val = Info_Get (&cl->_userinfo_ctx_, "name");

		// trim user name
		strlcpy (newname, val, sizeof(newname));

		for (p = val; *p; p++)
		{
			if ((*p & 127) == '\\' || *p == '\r' || *p == '\n' || *p == '$' || *p == '#' || *p == '"' || *p == ';')
			{ // illegal characters in name, set some default
				strlcpy(newname, sv_default_name.string, sizeof(newname));
				break;
			}
		}

		for (p = newname; *p && (*p & 127) == ' '; p++)
			; // empty operator

		if (p != newname) // skip prefixed spaces, if any, even whole string of spaces
			strlcpy(newname, p, sizeof(newname));

		for (p = newname + strlen(newname) - 1; p >= newname; p--)
		{
			if (*p && (*p & 127) != ' ') // skip spaces in suffix, if any
			{
				p[1] = 0;
				break;
			}
		}

		if (strcmp(val, newname))
		{
			Info_Set (&cl->_userinfo_ctx_, "name", newname);
			val = Info_Get (&cl->_userinfo_ctx_, "name");
		}

		if (!val[0] || !Q_namecmp(val, "console") || strstr(val, "&c") || strstr(val, "&r"))
		{
			Info_Set (&cl->_userinfo_ctx_, "name", sv_default_name.string);
			val = Info_Get (&cl->_userinfo_ctx_, "name");
		}

		// check to see if another user by the same name exists
		while ( 1 )
		{
			for (i = 0, client = svs.clients ; i<MAX_CLIENTS ; i++, client++)
			{
				if (client->state != cs_spawned || client == cl)
					continue;

				if (!Q_namecmp(client->name, val))
					break;
			}
			if (i != MAX_CLIENTS)
			{ // dup name
				if (strlen(val) > CLIENT_NAME_LEN - 1)
					val[CLIENT_NAME_LEN - 4] = 0;
				p = val;

				if (val[0] == '(')
				{
					if (val[2] == ')')
						p = val + 3;
					else if (val[3] == ')')
						p = val + 4;
				}

				snprintf(newname, sizeof(newname), "(%d)%-.10s", dupc++, p);
				Info_Set (&cl->_userinfo_ctx_, "name", newname);
				val = Info_Get (&cl->_userinfo_ctx_, "name");
			}
			else
				break;
		}

		if (strncmp(val, cl->name, strlen(cl->name) + 1))
		{
			if (!cl->lastnametime || curtime - cl->lastnametime > 5) {
				cl->lastnamecount = 0;
				cl->lastnametime = curtime;
			}
			else if (cl->lastnamecount++ > 4) {
				SV_BroadcastPrintf(PRINT_HIGH, "%s was kicked for name spamming\n", cl->name);
				SV_ClientPrintf(cl, PRINT_HIGH, "You were kicked from the game for name spamming\n");
				SV_LogPlayer(cl, "name spam", 1); //bliP: player logging
				SV_DropClient(cl);
				return;
			}

			if (cl->state >= cs_spawned && !cl->spectator) {
				SV_BroadcastPrintf(PRINT_HIGH, "%s changed name to %s\n", cl->name, val);
			}
		}

		strlcpy(cl->name, val, CLIENT_NAME_LEN);

		if (cl->state >= cs_spawned) //bliP: player logging
			SV_LogPlayer(cl, "name change", 1);
	}

	// team
	val = Info_Get (&cl->_userinfo_ctx_, "team");
	if (strstr(val, "&c") || strstr(val, "&r"))
		Info_Set (&cl->_userinfo_ctx_, "team", "none");
	strlcpy (cl->team, Info_Get (&cl->_userinfo_ctx_, "team"), sizeof(cl->team));

	// rate
	val = Info_Get (&cl->_userinfo_ctx_, cl->download ? "drate" : "rate");
	cl->netchan.rate = 1.0 / SV_BoundRate (cl->download != NULL, Q_atoi(*val ? val : "99999"));

	// s2c packet dupes
	val = Info_Get(&cl->_userinfo_ctx_, "dupe");
	cl->dupe = atoi(val);
	cl->dupe = (int)bound(0, cl->dupe, MAX_DUPLICATE_PACKETS); // 0=1 packet (aka: no dupes)
	cl->netchan.dupe = (cl->download ? 0 : cl->dupe);

	// message level
	val = Info_Get (&cl->_userinfo_ctx_, "msg");
	if (val[0])
		cl->messagelevel = Q_atoi(val);

	//spectator print
	val = Info_Get(&cl->_userinfo_ctx_, "sp");
	if (val[0])
		cl->spec_print = Q_atoi(val);
}


//============================================================================

void OnChange_sysselecttimeout_var (cvar_t *var, char *value, qbool *cancel)
{
	int t = Q_atoi (value);

	if (t < 1000 || t > 1000000)
	{
		Con_Printf("WARNING: sys_select_timeout can't be less then 1000 (1 millisecond) and more then 1 000 000 (1 second).\n");
		*cancel = true;
		return;
	}
}

//bliP: 24/9 logdir ->
void OnChange_logdir_var (cvar_t *var, char *value, qbool *cancel)
{
	if (strstr(value, ".."))
	{
		*cancel = true;
		return;
	}	

	if (value[0])
		Sys_mkdir (value);
}
//<-

//bliP: admininfo ->
void OnChange_admininfo_var (cvar_t *var, char *value, qbool *cancel)
{
	if (value[0])
		Info_SetValueForStarKey (svs.info, "*admin", value, MAX_SERVERINFO_STRING);
	else
		Info_RemoveKey (svs.info, "*admin");
}
//<-

//bliP: telnet log level ->
void OnChange_telnetloglevel_var (cvar_t *var, char *value, qbool *cancel)
{
	logs[TELNET_LOG].log_level = Q_atoi(value);
}
//<-
void OnChange_qconsolelogsay_var (cvar_t *var, char *value, qbool *cancel)
{
	logs[CONSOLE_LOG].log_level = Q_atoi(value);
}

#ifdef SERVERONLY

void COM_Init (void)
{
	Cvar_Register (&developer);
	Cvar_Register (&version);
	Cvar_Register (&sys_simulation);

	Cvar_SetROM(&version, SERVER_NAME " " SERVER_VERSION);
}

//Free hunk memory up to host_hunklevel
//Can only be called when changing levels!
void Host_ClearMemory (void)
{
	if (!host_initialized)
		Sys_Error ("Host_ClearMemory before host initialized");

	CM_InvalidateMap ();

	// any data previously allocated on hunk is no longer valid
	Hunk_FreeToLowMark (host_hunklevel);
}

//memsize is the recommended amount of memory to use for hunk
void Host_InitMemory (int memsize)
{
	int t;

	if (SV_CommandLineUseMinimumMemory())
		memsize = MINIMUM_MEMORY;

	if ((t = SV_CommandLineHeapSizeMemoryKB()) != 0 && t + 1 < COM_Argc())
		memsize = Q_atoi (COM_Argv(t + 1)) * 1024;

	if ((t = SV_CommandLineHeapSizeMemoryMB()) != 0 && t + 1 < COM_Argc())
		memsize = Q_atoi (COM_Argv(t + 1)) * 1024 * 1024;

	if (memsize < MINIMUM_MEMORY)
		Sys_Error ("Only %4.1f megs of memory reported, can't execute game", memsize / (float)0x100000);

	Memory_Init (Q_malloc(memsize), memsize);
}

void Host_Init (int argc, char **argv, int default_memsize)
{
	extern int		hunk_size;
	cvar_t			*v;

	srand((unsigned)time(NULL));

	COM_InitArgv (argc, argv);

	Host_InitMemory (default_memsize);

	Con_Printf ("============= Starting %s =============\n", VersionStringFull());

	Cbuf_Init ();
	Cmd_Init ();
	Cvar_Init ();
	COM_Init ();

	FS_Init ();
	NET_Init ();

	Netchan_Init ();

	Sys_Init ();
	CM_Init ();

	SV_Init ();

	Hunk_AllocName (0, "-HOST_HUNKLEVEL-");
	host_hunklevel = Hunk_LowMark ();

	host_initialized = true;

	// walk through all vars and forse OnChange event if cvar was modified,
	// also apply that to variables which mirrored in userinfo because of cl_parsefunchars was't applyed as this moment,
	// same for serverinfo and may be this fix something also.
	for ( v = NULL; (v = Cvar_Next ( v )); )
	{
		if ( Cvar_GetFlags( v ) & (CVAR_ROM) )
			continue;

		Cvar_Set(v, v->string);
	}

	Con_Printf ("%4.1f megabyte heap\n", (float)hunk_size / (1024 * 1024));
	Con_Printf ("QuakeWorld Initialized\n");
#ifndef	WWW_INTEGRATION
	Con_Printf ("www authentication disabled (no curl support)\n");
#endif

	Cbuf_InsertText ("exec server.cfg\n");

	// process command line arguments
	Cmd_StuffCmds_f ();
	Cbuf_Execute ();

	host_everything_loaded = true;

	SV_Map(true);

	// if a map wasn't specified on the command line, spawn mvdsv-kg map
	if (sv.state == ss_dead)
	{
		Cmd_ExecuteString ("map mvdsv-kg");
		SV_Map(true);
	}

	// last resort - start map
	if (sv.state == ss_dead)
	{
		Cmd_ExecuteString ("map start");
		SV_Map(true);
	}

	if (sv.state == ss_dead)
		SV_Error ("Couldn't spawn a server");

#if defined (_WIN32) && !defined(_CONSOLE)
	{
		void SetWindowText_(char*);
		SetWindowText_(va(SERVER_NAME ":%d - QuakeWorld server", NET_UDPSVPort()));
	}
#endif
}

#endif // SERVERONLY

/*
====================
SV_Init
====================
*/

void SV_Init (void)
{
	memset(&_localinfo_, 0, sizeof(_localinfo_));
	_localinfo_.max = MAX_LOCALINFOS;

	PR_Init ();

	// send immediately
	svs.last_heartbeat = -99999;

	SV_InitLocal ();

	SV_MVDInit ();
	Login_Init ();
#ifndef SERVERONLY
	server_cfg_done = true;
#endif

#if defined(SERVERONLY) && defined(WWW_INTEGRATION)
	Central_Init ();
#endif
}

/*
============
SV_TimeOfDay
============
*/
void SV_TimeOfDay(date_t *date, char *timeformat)
{
	struct tm *newtime;
	time_t long_time;

	time(&long_time);
	newtime = localtime(&long_time);

	//bliP: date check ->
	if (!newtime)
	{
		date->day = 0;
		date->mon = 0;
		date->year = 0;
		date->hour = 0;
		date->min = 0;
		date->sec = 0;
		strlcpy(date->str, "#bad date#", sizeof(date->str));
		return;
	}
	//<-

	date->day = newtime->tm_mday;
	date->mon = newtime->tm_mon;
	date->year = newtime->tm_year + 1900;
	date->hour = newtime->tm_hour;
	date->min = newtime->tm_min;
	date->sec = newtime->tm_sec;
	strftime(date->str, sizeof(date->str)-1, timeformat, newtime);
}

//bliP: player logging ->
/*
============
SV_LogPlayer
============
*/
void SV_LogPlayer(client_t *cl, char *msg, int level)
{
	char info[MAX_EXT_INFO_STRING];
	char name[CLIENT_NAME_LEN];

	Info_ReverseConvert(&cl->_userinfo_ctx_, info, sizeof(info));
	Q_normalizetext(info);
	strlcpy(name, cl->name, sizeof(name));
	Q_normalizetext(name);

	SV_Write_Log(PLAYER_LOG, level,
	             va("%s\\%s\\%i\\%s\\%s\\%i%s\n",
	                msg,
	                name,
	                cl->userid,
	                NET_BaseAdrToString(cl->netchan.remote_address),
	                NET_BaseAdrToString(cl->realip),
	                cl->netchan.remote_address.port,
	                info
	               )
	            );
}

/*
============
SV_Write_Log
============
*/
void SV_Write_Log(int sv_log, int level, char *msg)
{
	static date_t date;
	char *log_msg, *error_msg;

	if (!(logs[sv_log].sv_logfile && *msg))
		return;

	if (logs[sv_log].log_level < level)
		return;

	SV_TimeOfDay(&date, "%a %b %d, %H:%M:%S %Y");

	switch (sv_log)
	{
	case FRAG_LOG:
	case MOD_FRAG_LOG:
		log_msg = msg; // these logs aren't in fs_gamedir
		error_msg = va("Can't write in %s log file: "/*%s/ */"%sN.log.\n",
		               /*fs_gamedir,*/ logs[sv_log].message_on,
		               logs[sv_log].file_name);
		break;
	default:
		log_msg = va("[%s].[%d] %s", date.str, level, msg);
		error_msg = va("Can't write in %s log file: "/*%s/ */"%s%i.log.\n",
		               /*fs_gamedir,*/ logs[sv_log].message_on,
		               logs[sv_log].file_name, NET_UDPSVPort());
	}

	if (fprintf(logs[sv_log].sv_logfile, "%s", log_msg) < 0)
	{
		//bliP: Sys_Error to Con_DPrintf ->
		//VVD: Con_DPrintf to Sys_Printf ->
		Sys_Printf("%s", error_msg);
		//<-
		SV_Logfile(sv_log, false);
	}
	else
	{
		fflush(logs[sv_log].sv_logfile);
		if ((int)sv_maxlogsize.value &&
		        (FS_FileLength(logs[sv_log].sv_logfile) > (int)sv_maxlogsize.value))
		{
			SV_Logfile(sv_log, true);
		}
	}
}

/*
============
Sys_compare_by functions for sort files in list
============
*/
int Sys_compare_by_date (const void *a, const void *b)
{
	return (int)(((file_t *)a)->time - ((file_t *)b)->time);
}

int Sys_compare_by_name (const void *a, const void *b)
{
	return strncmp(((file_t *)a)->name, ((file_t *)b)->name, MAX_DEMO_NAME);
}

// real-world time passed
double SV_ClientConnectedTime(client_t* client)
{
	if (!client->connection_started_curtime) {
		return 0;
	}
	return curtime - client->connection_started_curtime;
}

// affected by pause
double SV_ClientGameTime(client_t* client)
{
	if (!client->connection_started_realtime) {
		return 0;
	}

	return realtime - client->connection_started_realtime;
}

void SV_SetClientConnectionTime(client_t* client)
{
	client->connection_started_realtime = realtime;
	client->connection_started_curtime = curtime;
}

#endif // !CLIENTONLY
