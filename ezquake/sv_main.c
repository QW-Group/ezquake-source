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

#include "qwsvdef.h"
#include "pmove.h"
#include "version.h"

int			current_skill;			// for entity spawnflags checking

netadr_t	master_adr[MAX_MASTERS];	// address of group servers

client_t	*sv_client;					// current client

cvar_t	sv_mintic = {"sv_mintic", "0.013"};	// bound the size of the
cvar_t	sv_maxtic = {"sv_maxtic", "0.1"};	//

cvar_t	sv_timeout = {"timeout","65"};		// seconds without any message
cvar_t	sv_zombietime = {"zombietime", "2"};	// seconds to sink messages
											// after disconnect

#ifdef SERVERONLY
cvar_t	rcon_password = {"rcon_password", ""};	// password for remote server commands
cvar_t	password = {"password", ""};	// password for entering the game
#else
extern cvar_t rcon_password;
extern cvar_t password;
#endif
cvar_t	spectator_password = {"spectator_password", ""};	// password for entering as a sepctator

cvar_t	allow_download = {"allow_download", "1"};
cvar_t	allow_download_skins = {"allow_download_skins", "1"};
cvar_t	allow_download_models = {"allow_download_models", "1"};
cvar_t	allow_download_sounds = {"allow_download_sounds", "1"};
cvar_t	allow_download_maps = {"allow_download_maps", "1"};
cvar_t	allow_download_pakmaps = {"allow_download_pakmaps", "0"};

cvar_t	sv_highchars = {"sv_highchars", "1"};
cvar_t	sv_phs = {"sv_phs", "1"};
cvar_t	sv_pausable = {"pausable", "1"};
cvar_t	sv_maxrate = {"sv_maxrate", "0"};
cvar_t	sv_fastconnect = {"sv_fastconnect", "0"};

cvar_t	sv_halflifebsp = {"halflifebsp", "0", CVAR_ROM};

// game rules mirrored in svs.info
cvar_t	fraglimit = {"fraglimit", "0", CVAR_SERVERINFO};
cvar_t	timelimit = {"timelimit", "0", CVAR_SERVERINFO};
cvar_t	teamplay = {"teamplay", "0", CVAR_SERVERINFO};
cvar_t	samelevel = {"samelevel", "0", CVAR_SERVERINFO};
cvar_t	maxclients = {"maxclients", "8", CVAR_SERVERINFO};
cvar_t	maxspectators = {"maxspectators", "8", CVAR_SERVERINFO};
cvar_t	deathmatch = {"deathmatch", "1", CVAR_SERVERINFO};			// 0, 1, or 2
cvar_t	hostname = {"hostname", "unnamed", CVAR_SERVERINFO};
cvar_t	watervis = {"watervis", "0", CVAR_SERVERINFO};
cvar_t	coop = {"coop", "0", CVAR_SERVERINFO};

cvar_t	skill = {"skill", "1"};

FILE	*sv_fraglogfile;

void SV_AcceptClient (netadr_t adr, int userid, char *userinfo);
void Master_Shutdown (void);

//============================================================================

qboolean ServerPaused(void) {
	return sv.paused;
}

/*
Used by SV_Shutdown to send a final message to all connected clients before the server goes down.
The messages are sent immediately, not just stuck on the outgoing message list, because the
server is going to totally exit after returning from this function.
*/
void SV_FinalMessage (char *message) {
	int i;
	client_t *cl;

	if (!sv.state)
		return;

	SZ_Clear (&net_message);
	MSG_WriteByte (&net_message, svc_print);
	MSG_WriteByte (&net_message, PRINT_HIGH);
	MSG_WriteString (&net_message, message);
	MSG_WriteByte (&net_message, svc_disconnect);

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
		if (cl->state >= cs_spawned)
			Netchan_Transmit (&cl->netchan, net_message.cursize, net_message.data);
}

//Quake calls this before calling Sys_Quit or Sys_Error
void SV_Shutdown (char *finalmsg) {
	SV_FinalMessage (finalmsg);

	Master_Shutdown ();
	NET_ServerConfig (false);

	if (sv_fraglogfile) {
		fclose (sv_fraglogfile);
		sv_fraglogfile = NULL;
	}

	memset (&sv, 0, sizeof(sv));
	sv.state = ss_dead;
	com_serveractive = false;

	memset (svs.clients, 0, sizeof(svs.clients));
}

//Called when the player is totally leaving the server, either willingly or unwillingly.
//This is NOT called if the entire server is quiting or crashing.
void SV_DropClient (client_t *drop) {
	// add the disconnect
	MSG_WriteByte (&drop->netchan.message, svc_disconnect);

	if (drop->state == cs_spawned)
		if (!drop->spectator) {
			// call the prog function for removing a client
			// this will set the body to a dead frame, among other things
			pr_global_struct->self = EDICT_TO_PROG(drop->edict);
			PR_ExecuteProgram (pr_global_struct->ClientDisconnect);
		} else if (SpectatorDisconnect) {
			// call the prog function for removing a client
			// this will set the body to a dead frame, among other things
			pr_global_struct->self = EDICT_TO_PROG(drop->edict);
			PR_ExecuteProgram (SpectatorDisconnect);
		}

	if (drop->spectator)
		Com_Printf ("Spectator %s removed\n",drop->name);
	else
		Com_Printf ("Client %s removed\n",drop->name);

	if (drop->download) {
		fclose (drop->download);
		drop->download = NULL;
	}
	if (drop->upload) {
		fclose (drop->upload);
		drop->upload = NULL;
	}
	*drop->uploadfn = 0;

	drop->state = cs_zombie;		// become free in a few seconds
	drop->connection_started = svs.realtime;	// for zombie timeout

	drop->old_frags = 0;
	drop->edict->v.frags = 0;
	drop->name[0] = 0;
	memset (drop->userinfo, 0, sizeof(drop->userinfo));

	// send notification to all remaining clients
	SV_FullClientUpdate (drop, &sv.reliable_datagram);
}

//====================================================================

int SV_CalcPing (client_t *cl) {
	float ping;
	int i, count;
	register client_frame_t *frame;

	ping = 0;
	count = 0;
	for (frame = cl->frames, i = 0; i < UPDATE_BACKUP; i++, frame++) {
		if (frame->ping_time > 0) {
			ping += frame->ping_time;
			count++;
		}
	}
	if (!count)
		return 9999;
	ping /= count;

	return ping*1000;
}

//Writes all update values to a sizebuf
void SV_FullClientUpdate (client_t *client, sizebuf_t *buf) {
	int i;
	char info[MAX_INFO_STRING];

	i = client - svs.clients;

	if (client->state == cs_free && sv_fastconnect.value)
		return;

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
	MSG_WriteFloat (buf, svs.realtime - client->connection_started);

	strcpy (info, client->userinfo);
	Info_RemovePrefixedKeys (info, '_');	// server passwords, etc

	MSG_WriteByte (buf, svc_updateuserinfo);
	MSG_WriteByte (buf, i);
	MSG_WriteLong (buf, client->userid);
	MSG_WriteString (buf, info);
}

//Writes all update values to a client's reliable stream
void SV_FullClientUpdateToClient (client_t *client, client_t *cl) {
	ClientReliableCheckBlock(cl, 24 + strlen(client->userinfo));
	if (cl->num_backbuf) {
		SV_FullClientUpdate (client, &cl->backbuf);
		ClientReliable_FinishWrite(cl);
	} else
		SV_FullClientUpdate (client, &cl->netchan.message);
}

/*
==============================================================================
CONNECTIONLESS COMMANDS
==============================================================================
*/

//Responds with all the info that qplug or qspy can see
//This message can be up to around 5k with worst case string lengths.
void SVC_Status (void) {
	client_t *cl;
	int i, ping, top, bottom;

	SV_BeginRedirect (RD_PACKET);
	Com_Printf ("%s\n", svs.info);
	for (i = 0; i < MAX_CLIENTS; i++) {
		cl = &svs.clients[i];
		if ((cl->state == cs_connected || cl->state == cs_spawned ) && !cl->spectator) {
			top = atoi(Info_ValueForKey (cl->userinfo, "topcolor"));
			bottom = atoi(Info_ValueForKey (cl->userinfo, "bottomcolor"));
			top = (top < 0) ? 0 : ((top > 13) ? 13 : top);
			bottom = (bottom < 0) ? 0 : ((bottom > 13) ? 13 : bottom);
			ping = SV_CalcPing (cl);
			Com_Printf ("%i %i %i %i \"%s\" \"%s\" %i %i\n", cl->userid, 
				cl->old_frags, (int)(svs.realtime - cl->connection_started)/60,
				ping, cl->name, Info_ValueForKey (cl->userinfo, "skin"), top, bottom);
		}
	}
	SV_EndRedirect ();
}

#define	LOG_HIGHWATER	(MAX_DATAGRAM - 128)
#define	LOG_FLUSH		10*60
void SV_CheckLog (void) {
	sizebuf_t *sz;

	sz = &svs.log[svs.logsequence&1];

	// bump sequence if allmost full, or ten minutes have passed and
	// there is something still sitting there
	if (sz->cursize > LOG_HIGHWATER || (svs.realtime - svs.logtime > LOG_FLUSH && sz->cursize)) {
		// swap buffers and bump sequence
		svs.logtime = svs.realtime;
		svs.logsequence++;
		sz = &svs.log[svs.logsequence&1];
		sz->cursize = 0;
		Com_DPrintf ("beginning fraglog sequence %i\n", svs.logsequence);
	}
}

/*
Responds with all the logged frags for ranking programs.
If a sequence number is passed as a parameter and it is the same as the current sequence,
an A2A_NACK will be returned instead of the data.
*/
void SVC_Log (void) {
	int seq;
	char data[MAX_DATAGRAM+64];

	if (Cmd_Argc() == 2)
		seq = atoi(Cmd_Argv(1));
	else
		seq = -1;

	if (seq == svs.logsequence-1 || !sv_fraglogfile) {	
		// they already have this data, or we aren't logging frags
		data[0] = A2A_NACK;
		NET_SendPacket (NS_SERVER, 1, data, net_from);
		return;
	}

	Com_DPrintf ("sending log %i to %s\n", svs.logsequence-1, NET_AdrToString(net_from));

	sprintf (data, "stdlog %i\n", svs.logsequence-1);
	strcat (data, (char *)svs.log_buf[((svs.logsequence-1)&1)]);

	NET_SendPacket (NS_SERVER, strlen(data)+1, data, net_from);
}

//Just responds with an acknowledgement
void SVC_Ping (void) {
	char data;

	data = A2A_ACK;

	NET_SendPacket (NS_SERVER, 1, &data, net_from);
}

//Returns a challenge number that can be used in a subsequent client_connect command.
//We do this to prevent denial of service attacks that flood the server with invalid connection IPs.
//With a challenge, they must give a valid IP address.
void SVC_GetChallenge (void) {
	int i, oldest, oldestTime;

	oldest = 0;
	oldestTime = 0x7fffffff;

	// see if we already have a challenge for this ip
	for (i = 0; i < MAX_CHALLENGES; i++) {
		if (NET_CompareBaseAdr (net_from, svs.challenges[i].adr))
			break;
		if (svs.challenges[i].time < oldestTime) {
			oldestTime = svs.challenges[i].time;
			oldest = i;
		}
	}

	if (i == MAX_CHALLENGES) {
		// overwrite the oldest
		svs.challenges[oldest].challenge = (rand() << 16) ^ rand();
		svs.challenges[oldest].adr = net_from;
		svs.challenges[oldest].time = svs.realtime;
		i = oldest;
	}

	// send it back
	Netchan_OutOfBandPrint (NS_SERVER, net_from, "%c%i", S2C_CHALLENGE, 
			svs.challenges[i].challenge);
}

//A connection request that did not come from the master
void SVC_DirectConnect (void) {
	char userinfo[1024], *s;
	static int	userid;
	netadr_t adr;
	int i, edictnum, clients, spectators, qport, version, challenge;
	client_t *cl, *newcl, temp;
	edict_t *ent;
	qboolean spectator;

	version = atoi(Cmd_Argv(1));
	if (version != PROTOCOL_VERSION) {
		Netchan_OutOfBandPrint (NS_SERVER, net_from, "%c\nServer is version %4.2f.\n", A2C_PRINT, QW_VERSION);
		Com_Printf ("* rejected connect from version %i\n", version);
		return;
	}

	qport = atoi(Cmd_Argv(2));

	challenge = atoi(Cmd_Argv(3));

	// note an extra byte is needed to replace spectator key
	Q_strncpyz (userinfo, Cmd_Argv(4), sizeof(userinfo)-1);

	// see if the challenge is valid
	if (net_from.type != NA_LOOPBACK) {
		for (i = 0; i < MAX_CHALLENGES; i++) {
			if (NET_CompareBaseAdr (net_from, svs.challenges[i].adr)) {
				if (challenge == svs.challenges[i].challenge)
					break;		// good
				Netchan_OutOfBandPrint (NS_SERVER, net_from, "%c\nBad challenge.\n", A2C_PRINT);
				return;
			}
		}
		if (i == MAX_CHALLENGES) {
			Netchan_OutOfBandPrint (NS_SERVER, net_from, "%c\nNo challenge for address.\n", A2C_PRINT);
			return;
		}
	}

	// check for password or spectator_password
	s = Info_ValueForKey (userinfo, "spectator");
	if (s[0] && strcmp(s, "0")) {
		if (spectator_password.string[0] && 
			Q_strcasecmp(spectator_password.string, "none") &&
			strcmp(spectator_password.string, s) )
		{	// failed
			Com_Printf ("%s:spectator password failed\n", NET_AdrToString (net_from));
			Netchan_OutOfBandPrint (NS_SERVER, net_from, "%c\nrequires a spectator password\n\n", A2C_PRINT);
			return;
		}
		Info_RemoveKey (userinfo, "spectator"); // remove passwd
		Info_SetValueForStarKey (userinfo, "*spectator", "1", MAX_INFO_STRING);
		spectator = true;
	} else {
		s = Info_ValueForKey (userinfo, "password");
		if (password.string[0] && Q_strcasecmp(password.string, "none") && strcmp(password.string, s) ) {
			Com_Printf ("%s:password failed\n", NET_AdrToString (net_from));
			Netchan_OutOfBandPrint (NS_SERVER, net_from, "%c\nserver requires a password\n\n", A2C_PRINT);
			return;
		}
		spectator = false;
		Info_RemoveKey (userinfo, "password"); // remove passwd
	}

	adr = net_from;
	userid++;	// so every client gets a unique id

	newcl = &temp;
	memset (newcl, 0, sizeof(client_t));

	newcl->userid = userid;

	// works properly
	if (!sv_highchars.value) {
		byte *p, *q;

		for (p = (byte *)newcl->userinfo, q = (byte *) userinfo; 
			*q && p < (byte *)newcl->userinfo + sizeof(newcl->userinfo)-1; q++)
			if (*q > 31 && *q <= 127)
				*p++ = *q;
	} else {
		Q_strncpyz (newcl->userinfo, userinfo, sizeof(newcl->userinfo));
	}

	// if there is already a slot for this ip, drop it
	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++,cl++) {
		if (cl->state == cs_free)
			continue;
		if (NET_CompareBaseAdr (adr, cl->netchan.remote_address)
			&& ( cl->netchan.qport == qport || adr.port == cl->netchan.remote_address.port )) {
			if (cl->state == cs_connected) {
				Com_Printf ("%s:dup connect\n", NET_AdrToString (adr));
				userid--;
				return;
			}

			Com_Printf ("%s:reconnect\n", NET_AdrToString (adr));
			SV_DropClient (cl);
			break;
		}
	}

	// count up the clients and spectators
	clients = 0;
	spectators = 0;
	for (i = 0,cl=svs.clients; i < MAX_CLIENTS; i++,cl++) {
		if (cl->state == cs_free)
			continue;
		if (cl->spectator)
			spectators++;
		else
			clients++;
	}

	// if at server limits, refuse connection
	if ( maxclients.value > MAX_CLIENTS )
		Cvar_SetValue (&maxclients, MAX_CLIENTS);
	if (maxspectators.value > MAX_CLIENTS)
		Cvar_SetValue (&maxspectators, MAX_CLIENTS);
	if (maxspectators.value + maxclients.value > MAX_CLIENTS)
		Cvar_SetValue (&maxspectators, MAX_CLIENTS - maxclients.value);
	if ((spectator && spectators >= (int)maxspectators.value) || (!spectator && clients >= (int)maxclients.value)) {
		Com_Printf ("%s:full connect\n", NET_AdrToString (adr));
		Netchan_OutOfBandPrint (NS_SERVER, adr, "%c\nserver is full\n\n", A2C_PRINT);
		return;
	}

	// find a client slot
	newcl = NULL;
	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++,cl++) {
		if (cl->state == cs_free) {
			newcl = cl;
			break;
		}
	}
	if (!newcl) {
		Com_Printf ("WARNING: miscounted available clients\n");
		return;
	}
	
	// build a new connection
	// accept the new client
	// this is the only place a client_t is ever initialized
	*newcl = temp;

	Netchan_OutOfBandPrint (NS_SERVER, adr, "%c", S2C_CONNECTION );

	edictnum = (newcl-svs.clients)+1;
	
	Netchan_Setup (NS_SERVER, &newcl->netchan, adr, qport);

	newcl->state = cs_connected;

	SZ_Init (&newcl->datagram, newcl->datagram_buf, sizeof(newcl->datagram_buf));
	newcl->datagram.allowoverflow = true;

	// spectator mode can ONLY be set at join time
	newcl->spectator = spectator;

	// extract extensions mask
	newcl->extensions = atoi(Info_ValueForKey(newcl->userinfo, "*z_ext"));
	Info_RemoveKey (newcl->userinfo, "*z_ext");

	ent = EDICT_NUM(edictnum);	
	newcl->edict = ent;

	// parse some info from the info strings
	SV_ExtractFromUserinfo (newcl);

	// JACK: Init the floodprot stuff.
	for (i = 0; i < 10; i++)
		newcl->whensaid[i] = 0.0;
	newcl->whensaidhead = 0;
	newcl->lockedtill = 0;

	// call the progs to get default spawn parms for the new client
	PR_ExecuteProgram (pr_global_struct->SetNewParms);
	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		newcl->spawn_parms[i] = (&pr_global_struct->parm1)[i];

	if (newcl->spectator)
		Com_Printf ("Spectator %s connected\n", newcl->name);
	else
		Com_DPrintf ("Client %s connected\n", newcl->name);
	newcl->sendinfo = true;
}

int Rcon_Validate (void) {
	if (!strlen (rcon_password.string))
		return 0;

	if (strcmp (Cmd_Argv(1), rcon_password.string) )
		return 0;

	return 1;
}

/*
A client issued an rcon command.
Shift down the remaining args
Redirect all printfs
*/
void SVC_RemoteCommand (void) {
	if (!Rcon_Validate ()) {
		Com_Printf ("Bad rcon from %s:\n%s\n", NET_AdrToString (net_from), net_message.data + 4);

		SV_BeginRedirect (RD_PACKET);
		Com_Printf ("Bad rcon_password.\n");
	} else {
		Com_Printf ("Rcon from %s:\n%s\n", NET_AdrToString (net_from), net_message.data + 4);
		SV_BeginRedirect (RD_PACKET);

		Cmd_ExecuteString (Cmd_MakeArgs(2));
	}
	SV_EndRedirect ();
}

//A connectionless packet has four leading 0xff characters to distinguish it from a game channel.
//Clients that are in the game can still send connectionless packets.
void SV_ConnectionlessPacket (void) {
	char *s, *c;

	MSG_BeginReading ();
	MSG_ReadLong ();		// skip the -1 marker

	s = MSG_ReadStringLine ();
	s[1023] = 0;

	Cmd_TokenizeString (s);

	c = Cmd_Argv(0);

	if (!strcmp(c, "ping") || (c[0] == A2A_PING && (c[1] == 0 || c[1] == '\n'))) {
		SVC_Ping ();
		return;
	}
	if (c[0] == A2A_ACK && (c[1] == 0 || c[1] == '\n')) {
		Com_Printf ("A2A_ACK from %s\n", NET_AdrToString (net_from));
		return;
	} else if (!strcmp(c, "status")) {
		SVC_Status ();
		return;
	} else if (!strcmp(c, "log")) {
		SVC_Log ();
		return;
	} else if (!strcmp(c, "connect")) {
		SVC_DirectConnect ();
		return;
	} else if (!strcmp(c, "getchallenge")) {
		SVC_GetChallenge ();
		return;
	} else if (!strcmp(c, "rcon")) {
		SVC_RemoteCommand ();
	} else {
		Com_Printf ("bad connectionless packet from %s:\n%s\n", NET_AdrToString (net_from), s);
	}
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

typedef struct {
	unsigned	mask;
	unsigned	compare;
} ipfilter_t;

#define	MAX_IPFILTERS	1024

ipfilter_t	ipfilters[MAX_IPFILTERS];
int			numipfilters;

cvar_t	filterban = {"filterban", "1"};

qboolean StringToFilter (char *s, ipfilter_t *f) {
	char num[128];
	int i, j;
	byte b[4], m[4];

	for (i = 0; i < 4; i++) {
		b[i] = 0;
		m[i] = 0;
	}

	for (i = 0; i < 4; i++) {
		if (*s < '0' || *s > '9') {
			Com_Printf ("Bad filter address: %s\n", s);
			return false;
		}

		j = 0;
		while (*s >= '0' && *s <= '9')
			num[j++] = *s++;
		num[j] = 0;
		b[i] = atoi(num);
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

void SV_AddIP_f (void) {
	int i;
	
	for (i = 0; i < numipfilters; i++)
		if (ipfilters[i].compare == 0xffffffff)
			break;		// free spot
	if (i == numipfilters) {
		if (numipfilters == MAX_IPFILTERS) {
			Com_Printf ("IP filter list is full\n");
			return;
		}
		numipfilters++;
	}

	if (!StringToFilter (Cmd_Argv(1), &ipfilters[i]))
		ipfilters[i].compare = 0xffffffff;
}

void SV_RemoveIP_f (void) {
	ipfilter_t f;
	int i, j;

	if (!StringToFilter (Cmd_Argv(1), &f))
		return;
	for (i = 0; i < numipfilters; i++) {
		if (ipfilters[i].mask == f.mask && ipfilters[i].compare == f.compare) {
			for (j = i + 1; j < numipfilters; j++)
				ipfilters[j - 1] = ipfilters[j];
			numipfilters--;
			Com_Printf ("Removed.\n");
			return;
		}
	}
	Com_Printf ("Didn't find %s.\n", Cmd_Argv(1));
}

void SV_ListIP_f (void) {
	int i;
	byte b[4];

	Com_Printf ("Filter list:\n");
	for (i = 0; i < numipfilters; i++) {
		*(unsigned *)b = ipfilters[i].compare;
		Com_Printf ("%3i.%3i.%3i.%3i\n", b[0], b[1], b[2], b[3]);
	}
}

void SV_WriteIP_f (void) {
	FILE *f;
	char name[MAX_OSPATH];
	byte b[4];
	int i;

	Q_snprintfz (name, sizeof(name), "%s/listip.cfg", com_gamedir);

	Com_Printf ("Writing %s.\n", name);

	if (!(f = fopen (name, "wb")))	{
		Com_Printf ("Couldn't open %s\n", name);
		return;
	}

	for (i = 0 ; i < numipfilters ; i++) {
		*(unsigned *) b = ipfilters[i].compare;
		fprintf (f, "addip %i.%i.%i.%i\n", b[0], b[1], b[2], b[3]);
	}

	fclose (f);
}

void SV_SendBan (void) {
	char data[128];

	data[0] = data[1] = data[2] = data[3] = 0xff;
	data[4] = A2C_PRINT;
	data[5] = 0;
	strcat (data, "\nbanned.\n");
	
	NET_SendPacket (NS_SERVER, strlen(data), data, net_from);
}

//Returns true if net_from.ip is banned
qboolean SV_FilterPacket (void) {
	int	 i;
	unsigned in;

	if (net_from.type == NA_LOOPBACK)
		return false;	// the local client can't be banned

	in = *(unsigned *)net_from.ip;

	for (i = 0; i < numipfilters; i++)
		if ( (in & ipfilters[i].mask) == ipfilters[i].compare)
			return filterban.value;

	return !filterban.value;
}

//============================================================================

void SV_ReadPackets (void) {
	int i, qport;
	client_t *cl;

	while (NET_GetPacket(NS_SERVER)) {
		if (SV_FilterPacket ()) {
			SV_SendBan ();	// tell them we aren't listening...
			continue;
		}

		// check for connectionless packet (0xffffffff) first
		if (*(int *)net_message.data == -1) {
			SV_ConnectionlessPacket ();
			continue;
		}
		
		// read the qport out of the message so we can fix up
		// stupid address translating routers
		MSG_BeginReading ();
		MSG_ReadLong ();		// sequence number
		MSG_ReadLong ();		// sequence number
		qport = MSG_ReadShort () & 0xffff;

		// check for packets from connected clients
		for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++,cl++) {
			if (cl->state == cs_free)
				continue;
			if (!NET_CompareBaseAdr (net_from, cl->netchan.remote_address))
				continue;
			if (cl->netchan.qport != qport)
				continue;
			if (cl->netchan.remote_address.port != net_from.port) {
				Com_DPrintf ("SV_ReadPackets: fixing up a translated port\n");
				cl->netchan.remote_address.port = net_from.port;
			}
			if (Netchan_Process(&cl->netchan)) {	
				// this is a valid, sequenced packet, so process it
				svs.stats.packets++;
				cl->send_message = true;	// reply at end of frame
				if (cl->state != cs_zombie)
					SV_ExecuteClientMessage (cl);
			}
			break;
		}

		if (i != MAX_CLIENTS)
			continue;
	}
}

/*
If a packet has not been received from a client in timeout.value seconds, drop the conneciton.

When a client is normally dropped, the client_t goes into a zombie state for a few seconds
to make sure any final reliable message gets resent if necessary
*/
void SV_CheckTimeouts (void) {
	int i, nclients;
	client_t *cl;
	float droptime;

	droptime = curtime - sv_timeout.value;
	nclients = 0;

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++,cl++) {
		if (cl->state == cs_connected || cl->state == cs_spawned) {
			if (!cl->spectator)
				nclients++;
			if (cl->netchan.last_received < droptime) {
				SV_BroadcastPrintf (PRINT_HIGH, "%s timed out\n", cl->name);
				SV_DropClient (cl); 
				cl->state = cs_free;	// don't bother with zombie state
			}
		}
		if (cl->state == cs_zombie && svs.realtime - cl->connection_started > sv_zombietime.value)
			cl->state = cs_free;	// can now be reused
	}
	if (sv.paused && !nclients) {
		// nobody left, unpause the server
		SV_TogglePause("Pause released since no players are left.\n");
	}
}

//Add them exactly as if they had been typed at the console
void SV_GetConsoleCommands (void) {
	char *cmd;

	while (1) {
		if (!(cmd = Sys_ConsoleInput()))
			break;
		Cbuf_AddText (cmd);
		Cbuf_AddText ("\n");
	}
}

int SV_BoundRate (int rate) {
	if (!rate)
		rate = 2500;

	if (sv_maxrate.value)
		rate = min(rate, sv_maxrate.value);
	else
		rate = min(rate, 10000);

	if (rate < 500)		// not less than 500 no matter what sv_maxrate is
		rate = 500;

	return rate;
}

void SV_CheckVars (void) {
	static char pw[MAX_INFO_STRING] = {0}, spw[MAX_INFO_STRING] = {0};
	static float old_maxrate = 0;
	int v;

	// check password and spectator_password
	if (strcmp(password.string, pw) || strcmp(spectator_password.string, spw)) {
		Q_strncpyz (pw, password.string, sizeof(pw));
		Q_strncpyz (spw, spectator_password.string, sizeof(spw));
		Cvar_Set (&password, pw);
		Cvar_Set (&spectator_password, spw);

		v = 0;
		if (pw && pw[0] && strcmp(pw, "none"))
			v |= 1;
		if (spw && spw[0] && strcmp(spw, "none"))
			v |= 2;

		Com_DPrintf ("Updated needpass.\n");

		Info_SetValueForKey (svs.info, "needpass", v ? va("%i", v) : "", MAX_SERVERINFO_STRING);
	}

	// check sv_maxrate
	if (sv_maxrate.value != old_maxrate) {
		client_t *cl;
		int i;
		char *val;

		old_maxrate = sv_maxrate.value;

		for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
			if (cl->state < cs_connected)
				continue;

			val = Info_ValueForKey (cl->userinfo, "rate");
			cl->netchan.rate = 1.0 / SV_BoundRate (atoi(val));
		}
	}
}

void SV_Frame (double time) {
	static double start, end;

	start = Sys_DoubleTime ();
	svs.stats.idle += start - end;
	
	// keep the random time dependent
	rand ();

	// decide the simulation time
	if (!sv.paused) {
		svs.realtime += time;
		sv.time += time;
	}

	// check timeouts
	SV_CheckTimeouts ();

	// toggle the log buffer if full
	SV_CheckLog ();

	// move autonomous things around if enough time has passed
	if (!sv.paused)
		SV_Physics ();

	// get packets
	SV_ReadPackets ();

	if (dedicated) {
		// check for commands typed to the host
		SV_GetConsoleCommands ();
		
		// process console commands
		Cbuf_Execute ();
	}

	SV_CheckVars ();

	// send messages back to the clients that had packets read this frame
	SV_SendClientMessages ();

	// send a heartbeat to the master if needed
	Master_Heartbeat ();

	// collect timing statistics
	end = Sys_DoubleTime ();
	svs.stats.active += end-start;
	if (++svs.stats.count == STATFRAMES) {
		svs.stats.latched_active = svs.stats.active;
		svs.stats.latched_idle = svs.stats.idle;
		svs.stats.latched_packets = svs.stats.packets;
		svs.stats.active = 0;
		svs.stats.idle = 0;
		svs.stats.packets = 0;
		svs.stats.count = 0;
	}
}

void SV_InitLocal (void) {
	int i;
	extern cvar_t sv_spectalk, sv_mapcheck, sv_aim, sv_nailhack, sv_loadentfiles, sv_maxvelocity, sv_gravity;
	extern cvar_t pm_stopspeed, pm_spectatormaxspeed, pm_accelerate, pm_airaccelerate, pm_wateraccelerate;
	extern cvar_t pm_friction, pm_waterfriction, pm_bunnyspeedcap, pm_ktjump, pm_slidefix;

	SV_InitOperatorCommands	();

	Cvar_SetCurrentGroup(CVAR_GROUP_NO_GROUP);
	if (dedicated) {
		Cvar_Register (&rcon_password);
		Cvar_Register (&password);
	}
	Cvar_Register (&spectator_password);

	Cvar_SetCurrentGroup(CVAR_GROUP_SERVER_MAIN);
	Cvar_Register (&sv_aim);
	Cvar_Register (&sv_highchars);
	Cvar_Register (&sv_phs);
	Cvar_Register (&sv_pausable);
	Cvar_Register (&sv_nailhack);
	Cvar_Register (&sv_maxrate);
	Cvar_Register (&sv_fastconnect);
	Cvar_Register (&sv_loadentfiles);
	Cvar_Register (&sv_mintic);
	Cvar_Register (&sv_maxtic);
	Cvar_Register (&sv_timeout);
	Cvar_Register (&sv_zombietime);
	Cvar_Register (&sv_spectalk);
	Cvar_Register (&sv_mapcheck);
	Cvar_Register (&sv_halflifebsp);

	Cvar_Register (&filterban);
	Cvar_Register (&skill);
	Cvar_Register (&coop);

	Cvar_SetCurrentGroup(CVAR_GROUP_SERVERINFO);
	Cvar_Register (&deathmatch);
	Cvar_Register (&teamplay);
	Cvar_Register (&fraglimit);
	Cvar_Register (&timelimit);
	Cvar_Register (&samelevel);
	Cvar_Register (&maxclients);
	Cvar_Register (&maxspectators);
	Cvar_Register (&hostname);
	Cvar_Register (&watervis);

	Cvar_SetCurrentGroup(CVAR_GROUP_SERVER_PHYSICS);
	Cvar_Register (&sv_maxvelocity);
	Cvar_Register (&sv_gravity);
	Cvar_Register (&pm_stopspeed);
	Cvar_Register (&pm_maxspeed);
	Cvar_Register (&pm_spectatormaxspeed);
	Cvar_Register (&pm_accelerate);
	Cvar_Register (&pm_airaccelerate);
	Cvar_Register (&pm_wateraccelerate);
	Cvar_Register (&pm_friction);
	Cvar_Register (&pm_waterfriction);
	Cvar_Register (&pm_bunnyspeedcap);
	Cvar_Register (&pm_ktjump);
	Cvar_Register (&pm_slidefix);

	Cvar_SetCurrentGroup(CVAR_GROUP_SERVER_PERMISSIONS);
	Cvar_Register (&allow_download);
	Cvar_Register (&allow_download_skins);
	Cvar_Register (&allow_download_models);
	Cvar_Register (&allow_download_sounds);
	Cvar_Register (&allow_download_maps);
	Cvar_Register (&allow_download_pakmaps);

	Cvar_ResetCurrentGroup();

	Cmd_AddCommand ("addip", SV_AddIP_f);
	Cmd_AddCommand ("removeip", SV_RemoveIP_f);
	Cmd_AddCommand ("listip", SV_ListIP_f);
	Cmd_AddCommand ("writeip", SV_WriteIP_f);

	if (!dedicated)
		Cvar_SetDefault(&sv_mintic, 0);

	for (i = 1; i < MAX_MODELS; i++)
		Q_snprintfz (localmodels[i], sizeof(localmodels[i]), "*%i", i);

	Info_SetValueForStarKey (svs.info, "*version", va("%4.2f", QW_VERSION), MAX_SERVERINFO_STRING);
	Info_SetValueForStarKey (svs.info, "*fuh_version", VersionString(), MAX_SERVERINFO_STRING);
	Info_SetValueForStarKey (svs.info, "*z_ext", va("%i", SV_SUPPORTED_EXTENSIONS), MAX_SERVERINFO_STRING);

	if (strcmp(com_gamedirfile, "qw"))
		Info_SetValueForStarKey (svs.info, "*gamedir", com_gamedirfile, MAX_SERVERINFO_STRING); 

	// init fraglog stuff
	svs.logsequence = 1;
	svs.logtime = svs.realtime;
	SZ_Init (&svs.log[0], svs.log_buf[0], sizeof(svs.log_buf[0]));
	svs.log[0].allowoverflow = true;

	SZ_Init (&svs.log[1], svs.log_buf[1], sizeof(svs.log_buf[1]));
	svs.log[1].allowoverflow = true;
}


//============================================================================


//Send a message to the master every few minutes to let it know we are alive, and log information
#define	HEARTBEAT_SECONDS	300
void Master_Heartbeat (void) {
	char string[2048];
	int active, i;

	if (svs.realtime - svs.last_heartbeat < HEARTBEAT_SECONDS)
		return;		// not time to send yet

	svs.last_heartbeat = svs.realtime;

	// count active users
	active = 0;
	for (i = 0; i < MAX_CLIENTS; i++)
		if (svs.clients[i].state == cs_connected ||
		svs.clients[i].state == cs_spawned )
			active++;

	svs.heartbeat_sequence++;
	sprintf (string, "%c\n%i\n%i\n", S2M_HEARTBEAT,
		svs.heartbeat_sequence, active);


	// send to group master
	for (i = 0; i < MAX_MASTERS; i++)
		if (master_adr[i].port) {
			Com_Printf ("Sending heartbeat to %s\n", NET_AdrToString (master_adr[i]));
			NET_SendPacket (NS_SERVER, strlen(string), string, master_adr[i]);
		}
}

//Informs all masters that this server is going down
void Master_Shutdown (void) {
	char string[2048];
	int i;

	sprintf (string, "%c\n", S2M_SHUTDOWN);

	// send to group master
	for (i = 0; i < MAX_MASTERS; i++) {
		if (master_adr[i].port) {
			Com_Printf ("Sending heartbeat to %s\n", NET_AdrToString (master_adr[i]));
			NET_SendPacket (NS_SERVER, strlen(string), string, master_adr[i]);
		}
	}
}

//Pull specific info from a newly changed userinfo string into a more C freindly form.
void SV_ExtractFromUserinfo (client_t *cl) {
	char *val, *p, *q, newname[80];
	int i, dupc = 1;
	client_t *client;

	// name for C code
	val = Info_ValueForKey (cl->userinfo, "name");

	// trim user name
	Q_strncpyz (newname, val, sizeof(newname));

	for (p = newname; (*p == ' ' || *p == '\r' || *p == '\n') && *p; p++)
		;

	if (p != newname && !*p) {
		//white space only
		strcpy(newname, "unnamed");
		p = newname;
	}

	if (p != newname && *p) {
		for (q = newname; *p; *q++ = *p++)
			;
		*q = 0;
	}
	for (p = newname + strlen(newname) - 1; p != newname && (*p == ' ' || *p == '\r' || *p == '\n') ; p--)
		;
	p[1] = 0;

	if (strcmp(val, newname)) {
		Info_SetValueForKey (cl->userinfo, "name", newname, MAX_INFO_STRING);
		val = Info_ValueForKey (cl->userinfo, "name");
	}

	if (!val[0] || !Q_strcasecmp(val, "console")) {
		Info_SetValueForKey (cl->userinfo, "name", "unnamed", MAX_INFO_STRING);
		val = Info_ValueForKey (cl->userinfo, "name");
	}

	// check to see if another user by the same name exists
	while (1) {
		for (i = 0, client = svs.clients; i < MAX_CLIENTS; i++, client++) {
			if (client->state != cs_spawned || client == cl)
				continue;
			if (!Q_strcasecmp(client->name, val))
				break;
		}
		if (i != MAX_CLIENTS) { // dup name
			if (strlen(val) > sizeof(cl->name) - 1)
				val[sizeof(cl->name) - 4] = 0;
			p = val;

			if (val[0] == '(')
				if (val[2] == ')')
					p = val + 3;
				else if (val[3] == ')')
					p = val + 4;

			sprintf(newname, "(%d)%-.40s", dupc++, p);
			Info_SetValueForKey (cl->userinfo, "name", newname, MAX_INFO_STRING);
			val = Info_ValueForKey (cl->userinfo, "name");
		} else
			break;
	}

	if (strncmp(val, cl->name, strlen(cl->name))) {
		if (!sv.paused) {
			if (!cl->lastnametime || svs.realtime - cl->lastnametime > 5) {
				cl->lastnamecount = 0;
				cl->lastnametime = svs.realtime;
			} else if (cl->lastnamecount++ > 4) {
				SV_BroadcastPrintf (PRINT_HIGH, "%s was kicked for name spam\n", cl->name);
				SV_ClientPrintf (cl, PRINT_HIGH, "You were kicked from the game for name spamming\n");
				SV_DropClient (cl); 
				return;
			}
		}

		if (cl->state >= cs_spawned && !cl->spectator)
			SV_BroadcastPrintf (PRINT_HIGH, "%s changed name to %s\n", cl->name, val);
	}

	Q_strncpyz (cl->name, val, sizeof(cl->name));

	// rate
	val = Info_ValueForKey (cl->userinfo, "rate");
	cl->netchan.rate = 1.0 / SV_BoundRate (atoi(val));

	// message level
	val = Info_ValueForKey (cl->userinfo, "msg");
	if (strlen(val))
		cl->messagelevel = atoi(val);

}

//============================================================================

void SV_Init (void) {
	PR_Init ();
	SV_InitLocal ();

	if (dedicated)
		NET_ServerConfig (true);

	svs.last_heartbeat = -99999;		// send immediately
}
