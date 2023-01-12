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
// sv_master.c - send heartbeats to master server

#ifndef CLIENTONLY
#include "qwsvdef.h"

#define HEARTBEAT_SECONDS 300

static netadr_t master_adr[MAX_MASTERS]; // address of group servers

/*
====================
SV_SetMaster_f

Make a master server current
====================
*/
void SV_SetMaster_f (void)
{
	char data[2];
	int i;

	memset (&master_adr, 0, sizeof(master_adr));

	for (i=1 ; i<Cmd_Argc() ; i++)
	{
		if (!strcmp(Cmd_Argv(i), "none") || !NET_StringToAdr (Cmd_Argv(i), &master_adr[i-1]))
		{
			Con_Printf ("Setting nomaster mode.\n");
			return;
		}
		if (master_adr[i-1].port == 0)
			master_adr[i-1].port = BigShort (27000);

		Con_Printf ("Master server at %s\n", NET_AdrToString (master_adr[i-1]));

		Con_Printf ("Sending a ping.\n");

		data[0] = A2A_PING;
		data[1] = 0;
		NET_SendPacket (NS_SERVER, 2, data, master_adr[i-1]);
	}

	svs.last_heartbeat = -99999;
}

/*
==================
SV_Heartbeat_f
==================
*/
void SV_Heartbeat_f (void)
{
	svs.last_heartbeat = -99999;
}

/*
================
Master_Heartbeat

Send a message to the master every few minutes to
let it know we are alive, and log information
================
*/
void Master_Heartbeat (void)
{
	char string[2048];
	int active;
	int i;

	if (sv.state != ss_active)
		return;

	if (realtime - svs.last_heartbeat < HEARTBEAT_SECONDS)
		return; // not time to send yet

	svs.last_heartbeat = realtime;

	//
	// count active users
	//
	active = 0;
	for (i=0 ; i<MAX_CLIENTS ; i++)
		if (svs.clients[i].state == cs_connected ||
				svs.clients[i].state == cs_spawned )
			active++;

	svs.heartbeat_sequence++;
	snprintf (string, sizeof(string), "%c\n%i\n%i\n", S2M_HEARTBEAT,
		  svs.heartbeat_sequence, active);


	// send to group master
	for (i=0 ; i<MAX_MASTERS ; i++)
		if (master_adr[i].port)
		{
			Con_Printf ("Sending heartbeat to %s\n", NET_AdrToString (master_adr[i]));
			NET_SendPacket (NS_SERVER, strlen(string), string, master_adr[i]);
		}
}

/*
=================
Master_Shutdown

Informs all masters that this server is going down
=================
*/
void Master_Shutdown (void)
{
	char string[2048];
	int i;

	snprintf (string, sizeof(string), "%c\n", S2M_SHUTDOWN);

	// send to group master
	for (i=0 ; i<MAX_MASTERS ; i++)
		if (master_adr[i].port)
		{
			Con_Printf ("Sending heartbeat to %s\n", NET_AdrToString (master_adr[i]));
			NET_SendPacket (NS_SERVER, strlen(string), string, master_adr[i]);
		}
}

#endif // !CLIENTONLY
