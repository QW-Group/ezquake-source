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
// cl_main.c  -- client main loop

#include "quakedef.h"
#include "winquake.h"
#include "cdaudio.h"
#include "cl_slist.h"
#include "input.h"
#include "keys.h"
#include "menu.h"
#include "sbar.h"
#include "sound.h"
#include "version.h"
#include "teamplay.h"
#include "image.h"

#include "movie.h"
#include "auth.h"
#include "logging.h"
#include "ignore.h"
#include "fmod.h"
#include "fchecks.h"
#include "rulesets.h"
#include "modules.h"
#include "config_manager.h"
#include "mp3_player.h"

#ifndef _WIN32
#include <netdb.h>		
#endif

cvar_t	rcon_password = {"rcon_password", ""};
cvar_t	rcon_address = {"rcon_address", ""};

cvar_t	cl_timeout = {"cl_timeout", "60"};

cvar_t	cl_shownet = {"cl_shownet", "0"};	// can be 0, 1, or 2

cvar_t	cl_sbar		= {"cl_sbar", "0", CVAR_ARCHIVE};
cvar_t	cl_hudswap	= {"cl_hudswap", "0", CVAR_ARCHIVE};
cvar_t	cl_maxfps	= {"cl_maxfps", "0", CVAR_ARCHIVE};

cvar_t	cl_predictPlayers = {"cl_predictPlayers", "1"};
cvar_t	cl_solidPlayers = {"cl_solidPlayers", "1"};

cvar_t  localid = {"localid", ""};

static qboolean allowremotecmd = true;

cvar_t	cl_deadbodyfilter = {"cl_deadbodyFilter", "0"};
cvar_t	cl_gibfilter = {"cl_gibFilter", "0"};
cvar_t	cl_muzzleflash = {"cl_muzzleflash", "1"};
cvar_t	cl_rocket2grenade = {"cl_r2g", "0"};
cvar_t	cl_demospeed = {"cl_demospeed", "1"};
cvar_t	cl_staticsounds = {"cl_staticSounds", "1"};
cvar_t	cl_trueLightning = {"cl_trueLightning", "0"};
cvar_t	cl_parseWhiteText = {"cl_parseWhiteText", "1"};
cvar_t	cl_filterdrawviewmodel = {"cl_filterdrawviewmodel", "0"};
cvar_t	cl_oldPL = {"cl_oldPL", "0"};
cvar_t	cl_demoPingInterval = {"cl_demoPingInterval", "5"};
cvar_t	cl_chatsound = {"cl_chatsound", "1"};
cvar_t	cl_confirmquit = {"cl_confirmquit", "1", CVAR_INIT};
cvar_t	default_fov = {"default_fov", "0"};
cvar_t	qizmo_dir = {"qizmo_dir", "qizmo"};

cvar_t cl_floodprot			= {"cl_floodprot", "0"};		
cvar_t cl_fp_messages		= {"cl_fp_messages", "4"};		
cvar_t cl_fp_persecond		= {"cl_fp_persecond", "4"};		
cvar_t cl_cmdline			= {"cl_cmdline", "", CVAR_ROM};	
cvar_t cl_useproxy			= {"cl_useproxy", "0"};			

cvar_t cl_model_bobbing		= {"cl_model_bobbing", "1"};	
cvar_t cl_nolerp			= {"cl_nolerp", "1"};

cvar_t r_rocketlight			= {"r_rocketLight", "1"};
cvar_t r_rocketlightcolor		= {"r_rocketLightColor", "0"};
cvar_t r_explosionlightcolor	= {"r_explosionLightColor", "0"};
cvar_t r_explosionlight			= {"r_explosionLight", "1"};
cvar_t r_explosiontype			= {"r_explosionType", "0"};
cvar_t r_flagcolor				= {"r_flagColor", "0"};
cvar_t r_lightflicker			= {"r_lightflicker", "1"};
cvar_t r_rockettrail			= {"r_rocketTrail", "1"};
cvar_t r_grenadetrail			= {"r_grenadeTrail", "1"};
cvar_t r_powerupglow			= {"r_powerupGlow", "1"};

// info mirrors
cvar_t	password = {"password", "", CVAR_USERINFO};
cvar_t	spectator = {"spectator", "", CVAR_USERINFO};
cvar_t	name = {"name", "player", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	team = {"team", "", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	topcolor = {"topcolor","0", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	bottomcolor = {"bottomcolor","0", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	skin = {"skin", "", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	rate = {"rate", "5760", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	msg = {"msg", "1", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	noaim = {"noaim", "0", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	w_switch = {"w_switch", "", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	b_switch = {"b_switch", "", CVAR_ARCHIVE|CVAR_USERINFO};

clientPersistent_t	cls;
clientState_t		cl;

centity_t		cl_entities[MAX_EDICTS];
efrag_t			cl_efrags[MAX_EFRAGS];
entity_t		cl_static_entities[MAX_STATIC_ENTITIES];
lightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
dlight_t		cl_dlights[MAX_DLIGHTS];

// refresh list
#ifdef GLQUAKE
visentlist_t	cl_firstpassents, cl_visents, cl_alphaents;
#else
visentlist_t	cl_visents, cl_visbents;
#endif

double		connect_time = 0;		// for connection retransmits

qboolean	host_skipframe;			// used in demo playback

byte		*host_basepal;
byte		*host_colormap;

int			fps_count;

// emodel and pmodel are encrypted to prevent llamas from easily hacking them
char emodel_name[] = { 'e'^0xe5, 'm'^0xe5, 'o'^0xe5, 'd'^0xe5, 'e'^0xe5, 'l'^0xe5, 0 };
char pmodel_name[] = { 'p'^0xe5, 'm'^0xe5, 'o'^0xe5, 'd'^0xe5, 'e'^0xe5, 'l'^0xe5, 0 };

static void simple_crypt (char *buf, int len) {
	while (len--)
		*buf++ ^= 0xe5;
}

static void CL_FixupModelNames (void) {
	simple_crypt (emodel_name, sizeof(emodel_name) - 1);
	simple_crypt (pmodel_name, sizeof(pmodel_name) - 1);
}

//============================================================================

char *CL_Macro_ConnectionType(void) {
	char *s;
	static char macrobuf[16];

	s = (cls.state < ca_connected) ? "disconnected" : cl.spectator ? "spectator" : "player";
	Q_strncpyz(macrobuf, s, sizeof(macrobuf));
	return macrobuf;
}

char *CL_Macro_Demoplayback(void) {
	char *s;
	static char macrobuf[16];

	s = cls.mvdplayback ? "mvdplayback" : cls.demoplayback ? "qwdplayback" : "0";
	Q_strncpyz(macrobuf, s, sizeof(macrobuf));
	return macrobuf;
}

char *CL_Macro_Serverstatus(void) {
	char *s;
	static char macrobuf[16];

	s = (cls.state < ca_connected) ? "disconnected" : cl.standby ? "standby" : "normal";
	Q_strncpyz(macrobuf, s, sizeof(macrobuf));
	return macrobuf;
}

int CL_ClientState (void) {
	return cls.state;
}

void CL_MakeActive(void) {
	cls.state = ca_active;
	if (cls.demoplayback) {
		host_skipframe = true;
		demostarttime = cls.demotime;		
	}

	if (!cls.demoplayback)
		VID_SetCaption (va("FuhQuake: %s", cls.servername));

	Con_ClearNotify ();
	TP_ExecTrigger ("f_spawn");
}

//Cvar system calls this when a CVAR_USERINFO cvar changes
void CL_UserinfoChanged (char *key, char *string) {
	char *s;

	s = TP_ParseFunChars (string, false);

	if (strcmp(s, Info_ValueForKey (cls.userinfo, key))) {
		Info_SetValueForKey (cls.userinfo, key, s, MAX_INFO_STRING);

		if (cls.state >= ca_connected) {
			MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
			SZ_Print (&cls.netchan.message, va("setinfo \"%s\" \"%s\"\n", key, s));
		}
	}
}

//called by CL_Connect_f and CL_CheckResend
void CL_SendConnectPacket (void) {
	netadr_t adr;
	char data[2048], biguserinfo[MAX_INFO_STRING + 32];
	double t1, t2;

	if (cls.state != ca_disconnected)
		return;

// JACK: Fixed bug where DNS lookups would cause two connects real fast
// Now, adds lookup time to the connect time.
	t1 = Sys_DoubleTime ();
	if (!NET_StringToAdr (cls.servername, &adr)) {
		Com_Printf ("Bad server address\n");
		connect_time = 0;
		return;
	}
	t2 = Sys_DoubleTime ();
	connect_time = cls.realtime + t2 - t1;	// for retransmit requests

	if (adr.port == 0)
		adr.port = BigShort (PORT_SERVER);

	cls.qport = Cvar_VariableValue("qport");

	// let the server know what extensions we support
	Q_strncpyz (biguserinfo, cls.userinfo, sizeof(biguserinfo));
	Info_SetValueForStarKey (biguserinfo, "*z_ext", va("%i", CL_SUPPORTED_EXTENSIONS), sizeof(biguserinfo));

	sprintf (data, "\xff\xff\xff\xff" "connect %i %i %i \"%s\"\n", PROTOCOL_VERSION, cls.qport, cls.challenge, biguserinfo);
	NET_SendPacket (NS_CLIENT, strlen(data), data, adr);
}

//Resend a connect message if the last one has timed out
void CL_CheckForResend (void) {
	netadr_t adr;
	char data[2048];
	double t1, t2;

	if (cls.state == ca_disconnected && com_serveractive) {
		// if the local server is running and we are not, then connect
		Q_strncpyz (cls.servername, "local", sizeof(cls.servername));
		CL_SendConnectPacket ();	// we don't need a challenge on the local server
		// FIXME: cls.state = ca_connecting so that we don't send the packet twice?
		return;
	}

	if (cls.state != ca_disconnected || !connect_time)
		return;
	if (cls.realtime - connect_time < 5.0)
		return;

	t1 = Sys_DoubleTime ();
	if (!NET_StringToAdr (cls.servername, &adr)) {
		Com_Printf ("Bad server address\n");
		connect_time = 0;
		return;
	}
	t2 = Sys_DoubleTime ();
	connect_time = cls.realtime + t2 - t1;	// for retransmit requests

	if (adr.port == 0)
		adr.port = BigShort (PORT_SERVER);

	Com_Printf ("Connecting to %s...\n", cls.servername);
	sprintf (data, "\xff\xff\xff\xff" "getchallenge\n");
	NET_SendPacket (NS_CLIENT, strlen(data), data, adr);
}

void CL_BeginServerConnect(void) {
	connect_time = -999;	// CL_CheckForResend() will fire immediately
	CL_CheckForResend();
}

void CL_Connect_f (void) {
	qboolean proxy;

	if (Cmd_Argc() != 2) {
		Com_Printf ("Usage: %s <server>\n", Cmd_Argv(0));
		return;
	}

	proxy = cl_useproxy.value && CL_ConnectedToProxy();

	if (proxy) {
		Cbuf_AddText(va("say ,connect %s", Cmd_Argv(1)));
	} else {
		Host_EndGame();
		Q_strncpyz(cls.servername, Cmd_Argv (1), sizeof(cls.servername));
		CL_BeginServerConnect();
	}
}




qboolean CL_ConnectedToProxy(void) {
	cmd_alias_t *alias = NULL;
	char **s, *qizmo_aliases[] = {	"ezcomp", "ezcomp2", "ezcomp3", 
									"f_sens", "f_fps", "f_tj", "f_ta", NULL};

	if (cls.state < ca_active)
		return false;
	for (s = qizmo_aliases; *s; s++) {
		if (!(alias = Cmd_FindAlias(*s)) || !(alias->flags & ALIAS_SERVER))
			return false;
	}
	return true;
}

void CL_Join_f (void) {
	qboolean proxy;

	proxy = cl_useproxy.value && CL_ConnectedToProxy();

	if (Cmd_Argc() > 2) {
		Com_Printf ("Usage: %s [server]\n", Cmd_Argv(0));
		return; 
	}

	Cvar_Set(&spectator, "");

	if (Cmd_Argc() == 2)
		Cbuf_AddText(va("%s %s\n", proxy ? "say ,connect" : "connect", Cmd_Argv(1)));
	else
		Cbuf_AddText(va("%s\n", proxy ? "say ,reconnect" : "reconnect"));
}

void CL_Observe_f (void) {
	qboolean proxy;

	proxy = cl_useproxy.value && CL_ConnectedToProxy();

	if (Cmd_Argc() > 2) {
		Com_Printf ("Usage: %s [server]\n", Cmd_Argv(0));
		return; 
	}

	if (!spectator.string[0] || !strcmp(spectator.string, "0"))
		Cvar_SetValue(&spectator, 1);

	if (Cmd_Argc() == 2)
		Cbuf_AddText(va("%s %s\n", proxy ? "say ,connect" : "connect", Cmd_Argv(1)));
	else
		Cbuf_AddText(va("%s\n", proxy ? "say ,reconnect" : "reconnect"));
}





void CL_DNS_f (void) {
	char address[128], *s;
	struct hostent *h;
	struct in_addr addr;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <address>\n", Cmd_Argv(0));
		return;
	}
	Q_strncpyz(address, Cmd_Argv(1), sizeof(address));
	if (s = strchr(address, ':'))
		*s = 0;
	if (((int) addr.s_addr = inet_addr(address)) == INADDR_NONE) {
		//forward lookup
		if (!(h = gethostbyname(address))) {
			Com_Printf("Couldn't resolve %s\n", address);
		} else {
			addr.s_addr = *(int *) h->h_addr_list[0];
			Com_Printf("Resolved %s to %s\n", address, inet_ntoa(addr));
		}
		return;
	}
	//reverse lookup ip address
	if (!(h = gethostbyaddr((char *) &addr, sizeof(addr), AF_INET)))
		Com_Printf("Couldn't resolve %s\n", address);
	else
		Com_Printf("Resolved %s to %s\n", address, h->h_name);
}



void CL_ClearState (void) {
	int i;
	extern float scr_centertime_off;

	S_StopAllSounds (true);

	Com_DPrintf ("Clearing memory\n");

	if (!com_serveractive)
		Host_ClearMemory();

	CL_ClearTEnts ();
	CL_ClearScene ();

	// wipe the entire cl structure
	memset (&cl, 0, sizeof(cl));

	SZ_Clear (&cls.netchan.message);

	// clear other arrays
	memset (cl_efrags, 0, sizeof(cl_efrags));
	memset (cl_dlights, 0, sizeof(cl_dlights));
	memset (cl_lightstyle, 0, sizeof(cl_lightstyle));
	memset (cl_entities, 0, sizeof(cl_entities));

	// make sure no centerprint messages are left from previous level
	scr_centertime_off = 0;

	// allocate the efrags and chain together into a free list
	cl.free_efrags = cl_efrags;
	for (i = 0; i < MAX_EFRAGS - 1; i++)
		cl.free_efrags[i].entnext = &cl.free_efrags[i + 1];
	cl.free_efrags[i].entnext = NULL;
}

//Sends a disconnect message to the server
//This is also called on Host_Error, so it shouldn't cause any errors
void CL_Disconnect (void) {
	byte final[10];

	connect_time = 0;
	cl.teamfortress = false;

	VID_SetCaption("FuhQuake");

	// stop sounds (especially looping!)
	S_StopAllSounds (true);

	MT_Disconnect();	

	if (cls.demorecording && cls.state != ca_disconnected)
		CL_Stop_f();

	if (cls.demoplayback) {
		CL_StopPlayback();
	} else if (cls.state != ca_disconnected) {
		final[0] = clc_stringcmd;
		strcpy (final + 1, "drop");
		Netchan_Transmit (&cls.netchan, 6, final);
		Netchan_Transmit (&cls.netchan, 6, final);
		Netchan_Transmit (&cls.netchan, 6, final);
	}

	memset(&cls.netchan, 0, sizeof(cls.netchan));
	cls.state = ca_disconnected;

	Cam_Reset();

	if (cls.download) {
		fclose(cls.download);
		cls.download = NULL;
	}

	CL_StopUpload();
	DeleteServerAliases();	
}

void CL_Disconnect_f (void) {
	cl.intermission = 0;
	Host_EndGame();
}

//The server is changing levels
void CL_Reconnect_f (void) {
	if (cls.download)  // don't change when downloading
		return;

	S_StopAllSounds (true);

	if (cls.state == ca_connected) {
		Com_Printf ("reconnecting...\n");
		MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");
		return;
	}

	if (!*cls.servername) {
		Com_Printf ("No server to reconnect to.\n");
		return;
	}

	Host_EndGame();
	CL_BeginServerConnect();
}

//Responses to broadcasts, etc
void CL_ConnectionlessPacket (void) {
	char *s, cmdtext[2048], data[6];
	int c;

    MSG_BeginReading ();
    MSG_ReadLong ();        // skip the -1

	c = MSG_ReadByte ();
	if (!cls.demoplayback)
		Com_Printf ("%s: ", NET_AdrToString (net_from));

	switch(c) {
	case S2C_CONNECTION:
		Com_Printf ("connection\n");
		if (cls.state >= ca_connected) {
			if (!cls.demoplayback)
				Com_Printf ("Dup connect received.  Ignored.\n");
			break;
		}
		Netchan_Setup (NS_CLIENT, &cls.netchan, net_from, cls.qport);
		MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");
		cls.state = ca_connected;
		Com_Printf ("Connected.\n");
		allowremotecmd = false; // localid required now for remote cmds
		break;

	case A2C_CLIENT_COMMAND:	// remote command from gui front end
		Com_Printf ("client command\n");

		if (!NET_IsLocalAddress(net_from)) {
			Com_Printf ("Command packet from remote host.  Ignored.\n");
			return;
		}
#ifdef _WIN32
		ShowWindow (mainwindow, SW_RESTORE);
		SetForegroundWindow (mainwindow);
#endif
		s = MSG_ReadString ();
		Q_strncpyz (cmdtext, s, sizeof(cmdtext));
		s = MSG_ReadString ();

		while (*s && isspace(*s))
			s++;
		while (*s && isspace(s[strlen(s) - 1]))
			s[strlen(s) - 1] = 0;

		if (!allowremotecmd && (!*localid.string || strcmp(localid.string, s))) {
			if (!*localid.string) {
				Com_Printf ("===========================\n");
				Com_Printf ("Command packet received from local host, but no "
					"localid has been set.  You may need to upgrade your server "
					"browser.\n");
				Com_Printf ("===========================\n");
			} else {
				Com_Printf ("===========================\n");
				Com_Printf ("Invalid localid on command packet received from local host. "
					"\n|%s| != |%s|\n"
					"You may need to reload your server browser and FuhQuake.\n",
					s, localid.string);
				Com_Printf ("===========================\n");
				Cvar_Set(&localid, "");
			}
		} else {
			Cbuf_AddText (cmdtext);
			Cbuf_AddText ("\n");
			allowremotecmd = false;
		}
		break;

	case A2C_PRINT:		// print command from somewhere
		Com_Printf ("print\n");

		s = MSG_ReadString ();
		Com_Printf ("%s", s);
		break;

	case A2A_PING:		// ping from somewhere
		Com_Printf ("ping\n");

		data[0] = 0xff;
		data[1] = 0xff;
		data[2] = 0xff;
		data[3] = 0xff;
		data[4] = A2A_ACK;
		data[5] = 0;

		NET_SendPacket (NS_CLIENT, 6, &data, net_from);
		break;

	case S2C_CHALLENGE:
		Com_Printf ("challenge\n");

		s = MSG_ReadString ();
		cls.challenge = atoi(s);
		CL_SendConnectPacket ();
		break;

	case svc_disconnect:
		if (cls.demoplayback) {
			Com_Printf("\n======== End of demo ========\n\n");
			Host_EndGame();
			Host_Abort();
		}
		break;

	default:		
		Com_Printf ("unknown:  %c\n", c);
		break;
	}
}

//Handles playback of demos, on top of NET_ code
qboolean CL_GetMessage (void) {
#ifdef _WIN32
	CL_CheckQizmoCompletion ();
#endif

	if (cls.demoplayback)
		return CL_GetDemoMessage();

	if (!NET_GetPacket(NS_CLIENT))
		return false;

	return true;
}

void CL_ReadPackets (void) {
	while (CL_GetMessage()) {
		// remote command packet
		if (*(int *)net_message.data == -1)	{
			CL_ConnectionlessPacket ();
			continue;
		}

		if (net_message.cursize < 8 && !cls.mvdplayback) {	
			Com_DPrintf ("%s: Runt packet\n", NET_AdrToString(net_from));
			continue;
		}

		// packet from server
		if (!cls.demoplayback && !NET_CompareAdr (net_from, cls.netchan.remote_address)) {
			Com_DPrintf ("%s: sequenced packet without connection\n", NET_AdrToString(net_from));
			continue;
		}

		if (cls.mvdplayback) {		
			MSG_BeginReading ();
		} else {
			if (!Netchan_Process(&cls.netchan))
				continue;			// wasn't accepted for some reason
		}
		CL_ParseServerMessage ();
	}

	// check timeout
	if (!cls.demoplayback && cls.state >= ca_connected ) {
		if (curtime - cls.netchan.last_received > (cl_timeout.value > 0 ? cl_timeout.value : 60)) {
			Com_Printf("\nServer connection timed out.\n");
			Host_EndGame();
			return;
		}
	}
}

void CL_SendToServer (void) {
	// when recording demos, request new ping times every cl_demoPingInterval.value seconds
	if (cls.demorecording && !cls.demoplayback && cls.state == ca_active && cl_demoPingInterval.value > 0) {
		if (cls.realtime - cl.last_ping_request > cl_demoPingInterval.value) {
			cl.last_ping_request = cls.realtime;
			MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
			SZ_Print (&cls.netchan.message, "pings");
		}
	}

	// send intentions now
	// resend a connection request if necessary
	if (cls.state == ca_disconnected)
		CL_CheckForResend ();
	else
		CL_SendCmd ();
}

//=============================================================================

void CL_SaveArgv(int argc, char **argv) {
	char saved_args[512];
	int i, total_length, length;
	qboolean first = true;

	length = total_length = saved_args[0] = 0;
	for (i = 0; i < argc; i++){
		if (!argv[i][0])
			continue;
		if (!first && total_length + 1 < sizeof(saved_args)) {
			strcat(saved_args, " ");
			total_length++;
		}
		first = false;
		length = strlen(argv[i]);
		if (total_length + length < sizeof(saved_args)) {
			strcat(saved_args, argv[i]);
			total_length += length;
		}
	}
	Cvar_ForceSet(&cl_cmdline, saved_args);
}

void CL_InitCommands (void);

void CL_InitLocal (void) {
	extern cvar_t baseskin, noskins;

	Cvar_SetCurrentGroup(CVAR_GROUP_CHAT);
	Cvar_Register (&cl_parseWhiteText);
	Cvar_Register (&cl_chatsound);

	Cvar_Register (&cl_floodprot);
	Cvar_Register (&cl_fp_messages );
	Cvar_Register (&cl_fp_persecond);


	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register (&cl_shownet);

	Cvar_SetCurrentGroup(CVAR_GROUP_SBAR);
	Cvar_Register (&cl_sbar);
	Cvar_Register (&cl_hudswap);

	Cvar_SetCurrentGroup(CVAR_GROUP_VIEWMODEL);
	Cvar_Register (&cl_filterdrawviewmodel);

	Cvar_SetCurrentGroup(CVAR_GROUP_EYECANDY);
	Cvar_Register (&cl_model_bobbing);
	Cvar_Register (&cl_nolerp);
	Cvar_Register (&cl_maxfps);
	Cvar_Register (&cl_deadbodyfilter);
	Cvar_Register (&cl_gibfilter);
	Cvar_Register (&cl_muzzleflash);
	Cvar_Register (&cl_rocket2grenade);
	Cvar_Register (&r_explosiontype);
	Cvar_Register (&r_lightflicker);
	Cvar_Register (&r_rockettrail);
	Cvar_Register (&r_grenadetrail);
	Cvar_Register (&r_powerupglow);
	Cvar_Register (&r_rocketlight);
	Cvar_Register (&r_explosionlight);
	Cvar_Register (&r_rocketlightcolor);
	Cvar_Register (&r_explosionlightcolor);
	Cvar_Register (&r_flagcolor);
	Cvar_Register (&cl_trueLightning);

	Cvar_SetCurrentGroup(CVAR_GROUP_SKIN);
	Cvar_Register (&noskins);
	Cvar_Register (&baseskin);

	Cvar_SetCurrentGroup(CVAR_GROUP_DEMO);
	Cvar_Register (&cl_demospeed);
	Cvar_Register (&cl_demoPingInterval);
	Cvar_Register (&qizmo_dir);

	Cvar_SetCurrentGroup(CVAR_GROUP_SOUND);
	Cvar_Register (&cl_staticsounds);

	Cvar_SetCurrentGroup(CVAR_GROUP_USERINFO);
	Cvar_Register (&team);
	Cvar_Register (&spectator);
	Cvar_Register (&skin);
	Cvar_Register (&rate);
	Cvar_Register (&noaim);
	Cvar_Register (&name);
	Cvar_Register (&msg);
	Cvar_Register (&topcolor);
	Cvar_Register (&bottomcolor);
	Cvar_Register (&w_switch);
	Cvar_Register (&b_switch);

	Cvar_SetCurrentGroup(CVAR_GROUP_VIEW);
	Cvar_Register (&default_fov);

	Cvar_SetCurrentGroup(CVAR_GROUP_NETWORK);
	Cvar_Register (&cl_predictPlayers);
	Cvar_Register (&cl_solidPlayers);
	Cvar_Register (&cl_oldPL);
	Cvar_Register (&cl_timeout);
	Cvar_Register (&cl_useproxy);

	Cvar_SetCurrentGroup(CVAR_GROUP_NO_GROUP);
	Cvar_Register (&password);
	Cvar_Register (&rcon_password);
	Cvar_Register (&rcon_address);
	Cvar_Register (&localid);
	Cvar_Register (&cl_warncmd);
	Cvar_Register (&cl_cmdline);

	Cvar_ResetCurrentGroup();

	Cvar_Register (&cl_confirmquit);

 	Info_SetValueForStarKey (cls.userinfo, "*FuhQuake", FUH_VERSION, MAX_INFO_STRING);

	Cmd_AddLegacyCommand ("demotimescale", "cl_demospeed");

	CL_InitCommands ();

	Cmd_AddCommand ("disconnect", CL_Disconnect_f);
	Cmd_AddCommand ("connect", CL_Connect_f);

	Cmd_AddCommand ("join", CL_Join_f);
	Cmd_AddCommand ("observe", CL_Observe_f);


	Cmd_AddCommand ("dns", CL_DNS_f);

	Cmd_AddCommand ("reconnect", CL_Reconnect_f);

	Cmd_AddMacro("connectiontype", CL_Macro_ConnectionType);
	Cmd_AddMacro("demoplayback", CL_Macro_Demoplayback);
	Cmd_AddMacro("matchstatus", CL_Macro_Serverstatus);
}

void CL_Init (void) {
	if (dedicated)
		return;

	cls.state = ca_disconnected;

	strcpy (cls.gamedirfile, com_gamedirfile);
	strcpy (cls.gamedir, com_gamedir);

	W_LoadWadFile ("gfx.wad");

	Modules_Init();
	FChecks_Init();				

	host_basepal = (byte *) FS_LoadHunkFile ("gfx/palette.lmp");
	if (!host_basepal)
		Sys_Error ("Couldn't load gfx/palette.lmp");		
	FMod_CheckModel("gfx/palette.lmp", host_basepal, com_filesize);

	host_colormap = (byte *) FS_LoadHunkFile ("gfx/colormap.lmp");
	if (!host_colormap)
		Sys_Error ("Couldn't load gfx/colormap.lmp");
	FMod_CheckModel("gfx/colormap.lmp", host_colormap, com_filesize); 

	Sys_mkdir(va("%s/qw", com_basedir));
	Sys_mkdir(va("%s/fuhquake", com_basedir));	

	Key_Init ();
	V_Init ();

#ifdef __linux__
	IN_Init ();
#endif

	VID_Init (host_basepal);

#ifndef __linux__
	IN_Init ();
#endif

	Image_Init();
	Draw_Init ();
	SCR_Init ();
	R_Init ();

	S_Init ();

	CDAudio_Init ();

	CL_InitLocal ();
	CL_FixupModelNames ();
	CL_InitInput ();
	CL_InitEnts ();
	CL_InitTEnts ();
	CL_InitPrediction ();
	CL_InitCam ();
	TP_Init ();
	Sbar_Init ();
	M_Init ();

	NET_ClientConfig (true);

	SList_Init ();
	SList_Load ();


	MT_Init();
	CL_Demo_Init();
	Ignore_Init();
	Auth_Init();
	Log_Init();
	Movie_Init();
	ConfigManager_Init();
	Rulesets_Init();
	Stats_Init();
	MP3_Init();
}

//============================================================================

void CL_BeginLocalConnection (void) {
	S_StopAllSounds (true);

	// make sure we're not connected to an external server,
	// and demo playback is stopped
	if (!com_serveractive)
		CL_Disconnect ();

	cl.worldmodel = NULL;

	if (cls.state == ca_active)
		cls.state = ca_connected;
}

static double CL_MinFrameTime (void) {
	double fps, fpscap;

	if (cls.timedemo || Movie_IsCapturing())
		return 0;

	if (cls.demoplayback) {
		if (!cl_maxfps.value)
			return 0;
		fps = max (30.0, cl_maxfps.value);
	} else {
		fpscap = cl.maxfps ? max (30.0, cl.maxfps) : Rulesets_MaxFPS();

		fps = cl_maxfps.value ? bound (30.0, cl_maxfps.value, fpscap) : com_serveractive ? fpscap : bound (30.0, rate.value / 80.0, fpscap);
	}

	return 1 / fps;
}

void CL_Frame (double time) {

	static double extratime = 0.001;
	double minframetime;

	extratime += time;
	minframetime = CL_MinFrameTime();

	if (extratime < minframetime) {
#ifdef _WIN32
		extern cvar_t sys_yieldcpu;
		if (sys_yieldcpu.value)
			Sys_MSleep(0);
#endif
		return;
	}

	cls.trueframetime = extratime - 0.001;
	cls.trueframetime = max(cls.trueframetime, minframetime);
	extratime -= cls.trueframetime;

	if (Movie_IsCapturing())
		cls.frametime = Movie_StartFrame();
	else
		cls.frametime = min(0.2, cls.trueframetime);

	if (cls.demoplayback) {
		if (cl.paused & PAUSED_DEMO)
			cls.frametime = 0;
		else if (!cls.timedemo)
			cls.frametime *= bound(0, cl_demospeed.value, 20);

		if (!host_skipframe)
			cls.demotime += cls.frametime;
		host_skipframe = false;
	}

	cls.realtime += cls.frametime;

	if (!cl.paused)
		cl.time += cls.frametime;

	// get new key events
	Sys_SendKeyEvents();

	// allow mice or other external controllers to add commands
	IN_Commands();

	// process console commands
	Cbuf_Execute();

	if (com_serveractive)
		SV_Frame(cls.frametime);

	// fetch results from server
	CL_ReadPackets();

	TP_UpdateSkins();


	if (cls.mvdplayback)
		MVD_Interpolate();


	// process stuffed commands
	Cbuf_ExecuteEx(&cbuf_svc);

	CL_SendToServer();

	if (cls.state >= ca_onserver) {	// !!! Tonik
		Cam_SetViewPlayer();

		// Set up prediction for other players
		CL_SetUpPlayerPrediction(false);

		// do client side motion prediction
		CL_PredictMove();

		// Set up prediction for other players
		CL_SetUpPlayerPrediction(true);

		// build a refresh entity list
		CL_EmitEntities();
	}

	// update video
	SCR_UpdateScreen();

	CL_DecayLights();

	// update audio
	if (cls.state == ca_active)
		S_Update(r_origin, vpn, vright, vup);
	else
		S_Update(vec3_origin, vec3_origin, vec3_origin, vec3_origin);

	CDAudio_Update();
	MP3_Frame();
	MT_Frame();

	if (Movie_IsCapturing())		
		Movie_FinishFrame();

	cls.framecount++;
	fps_count++;
}

//============================================================================

void CL_Shutdown (void) {
	CL_Disconnect();

	CL_WriteConfiguration();

	SList_Shutdown();
	CDAudio_Shutdown();
	S_Shutdown();
	MP3_Shutdown();
	IN_Shutdown ();
	Modules_Shutdown();
	Log_Shutdown();
	if (host_basepal)
		VID_Shutdown();
}
