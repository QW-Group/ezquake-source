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

	$Id: sv_user.c,v 1.24 2007-01-07 20:35:24 tonik Exp $
*/
// sv_user.c -- server code for moving users

#include "qwsvdef.h"
#include "pmove.h"

edict_t	*sv_player;

usercmd_t	cmd;

cvar_t	sv_spectalk = {"sv_spectalk", "1"};
cvar_t	sv_mapcheck	= {"sv_mapcheck", "1"};

qbool OnChange_sv_maxpitch (cvar_t *var, char *value);
qbool OnChange_sv_minpitch (cvar_t *var, char *value);
cvar_t	sv_maxpitch = {"sv_maxpitch", "80", 0, OnChange_sv_maxpitch};
cvar_t	sv_minpitch = {"sv_minpitch", "-70", 0, OnChange_sv_minpitch};

extern	vec3_t	player_mins;

extern int fp_messages, fp_persecond, fp_secondsdead;
extern cvar_t	sv_floodprotmsg;
extern cvar_t	sv_pausable;
extern cvar_t	pm_bunnyspeedcap;
extern cvar_t	pm_ktjump;
extern cvar_t	pm_slidefix;
extern cvar_t	pm_airstep;
extern cvar_t	pm_pground;

extern double	sv_frametime;

//
// pitch clamping
//
// All this OnChange code is because we want the cvar names to have sv_ prefixes,
// but don't want them in serverinfo (save a couple of bytes of space)
// Value sanity checks are also done here
//
qbool OnChange_sv_maxpitch (cvar_t *var, char *value) {
	float	newval;
	char	*newstr;

	newval = bound (0, Q_atof(value), 89.0f);
	if (newval == var->value)
		return true;

	Cvar_SetValue (var, newval);
	newstr = (newval == 80.0f) ? "" : Q_ftos(newval);	// don't show default values in serverinfo
	Info_SetValueForKey (svs.info, "maxpitch", newstr, MAX_SERVERINFO_STRING);
	SV_SendServerInfoChange("maxpitch", newstr);
	return false;
}

qbool OnChange_sv_minpitch (cvar_t *var, char *value) {
	float	newval;
	char	*newstr;

	newval = bound (-89.0f, Q_atof(value), 0.0f);
	if (newval == var->value)
		return true;

	Cvar_SetValue (var, newval);
	newstr = (newval == -70.0f) ? "" : Q_ftos(newval);	// don't show default values in serverinfo
	Info_SetValueForKey (svs.info, "minpitch", newstr, MAX_SERVERINFO_STRING);
	SV_SendServerInfoChange("minpitch", newstr);
	return false;
}


/*
============================================================
USER STRINGCMD EXECUTION
sv_client and sv_player will be valid.
============================================================
*/

//Sends the first message from the server to a connected client.
//This will be sent on the initial connection and upon each server load.
void Cmd_New_f (void) {
	char *gamedir, info[MAX_SERVERINFO_STRING];
	int playernum;

	if (sv_client->state == cs_spawned)
		return;

	sv_client->state = cs_connected;
	sv_client->connection_started = svs.realtime;

	// send the info about the new client to all connected clients
	//	SV_FullClientUpdate (sv_client, &sv.reliable_datagram);
	//	sv_client->sendinfo = true;

	gamedir = Info_ValueForKey (svs.info, "*gamedir");
	if (!gamedir[0])
		gamedir = "qw";

	//NOTE:  This doesn't go through ClientReliableWrite since it's before the userspawns.
	//These functions are written to not overflow
	if (sv_client->num_backbuf) {
		Com_Printf ("WARNING %s: [SV_New] Back buffered (%d0), clearing\n", sv_client->name, sv_client->netchan.message.cursize); 
		sv_client->num_backbuf = 0;
		SZ_Clear(&sv_client->netchan.message);
	}

	// send the serverdata
	MSG_WriteByte (&sv_client->netchan.message, svc_serverdata);
	MSG_WriteLong (&sv_client->netchan.message, PROTOCOL_VERSION);
	MSG_WriteLong (&sv_client->netchan.message, svs.spawncount);
	MSG_WriteString (&sv_client->netchan.message, gamedir);

	playernum = NUM_FOR_EDICT(sv_client->edict)-1;
	if (sv_client->spectator)
		playernum |= 128;
	MSG_WriteByte (&sv_client->netchan.message, playernum);

	// send full levelname
	MSG_WriteString (&sv_client->netchan.message, PR_GetString(sv.edicts->v.message));

	// send the movevars
	MSG_WriteFloat(&sv_client->netchan.message, movevars.gravity);
	MSG_WriteFloat(&sv_client->netchan.message, movevars.stopspeed);
	MSG_WriteFloat(&sv_client->netchan.message, movevars.maxspeed);
	MSG_WriteFloat(&sv_client->netchan.message, movevars.spectatormaxspeed);
	MSG_WriteFloat(&sv_client->netchan.message, movevars.accelerate);
	MSG_WriteFloat(&sv_client->netchan.message, movevars.airaccelerate);
	MSG_WriteFloat(&sv_client->netchan.message, movevars.wateraccelerate);
	MSG_WriteFloat(&sv_client->netchan.message, movevars.friction);
	MSG_WriteFloat(&sv_client->netchan.message, movevars.waterfriction);
	MSG_WriteFloat(&sv_client->netchan.message, movevars.entgravity);

	// send music
	MSG_WriteByte (&sv_client->netchan.message, svc_cdtrack);
	MSG_WriteByte (&sv_client->netchan.message, sv.edicts->v.sounds);

	// send server info string
	strcpy (info, svs.info);

	// append skybox name if there's enough room
	if (sv.sky[0]) {
		if (!strstr(svs.info, "\\sky\\") && strlen(info) + 5 + strlen(sv.sky) < MAX_SERVERINFO_STRING) {
			strcat (info, "\\sky\\");
			strcat (info, sv.sky);
		}
	}

	MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
	MSG_WriteString (&sv_client->netchan.message, va("fullserverinfo \"%s\"\n", info));
}

void Cmd_Soundlist_f (void) {
	char **s;
	unsigned n;

	if (sv_client->state != cs_connected) {
		Com_Printf ("soundlist not valid -- already spawned\n");
		return;
	}

	// handle the case of a level changing while a client was connecting
	if (atoi(Cmd_Argv(1)) != svs.spawncount) {
		SV_ClearReliable (sv_client);
		Com_Printf ("Cmd_Soundlist_f from different level\n");
		Cmd_New_f ();
		return;
	}

	n = atoi(Cmd_Argv(2));
	if (n + 1>= MAX_SOUNDS) {
		SV_ClearReliable (sv_client);
		SV_ClientPrintf (sv_client, PRINT_HIGH, "Cmd_Soundlist_f: Invalid soundlist index\n");
		SV_DropClient (sv_client);
		return;
	}

	//NOTE:  This doesn't go through ClientReliableWrite since it's before the userspawns.
	//These functions are written to not overflow
	if (sv_client->num_backbuf) {
		Com_Printf ("WARNING %s: [SV_Soundlist] Back buffered (%d0), clearing\n", sv_client->name, sv_client->netchan.message.cursize); 
		sv_client->num_backbuf = 0;
		SZ_Clear(&sv_client->netchan.message);
	}

	MSG_WriteByte (&sv_client->netchan.message, svc_soundlist);
	MSG_WriteByte (&sv_client->netchan.message, n);
	for (s = sv.sound_precache + 1 + n ; *s && sv_client->netchan.message.cursize < (MAX_MSGLEN / 2); s++, n++)
		MSG_WriteString (&sv_client->netchan.message, *s);

	MSG_WriteByte (&sv_client->netchan.message, 0);

	// next msg
	MSG_WriteByte (&sv_client->netchan.message, *s ? n : 0);
}

void Cmd_Modellist_f (void) {
	char **s;
	unsigned n;

	if (sv_client->state != cs_connected) {
		Com_Printf ("modellist not valid -- already spawned\n");
		return;
	}
	
	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount ) {
		SV_ClearReliable (sv_client);
		Com_Printf ("Cmd_Modellist_f from different level\n");
		Cmd_New_f ();
		return;
	}

	n = atoi(Cmd_Argv(2));
	if (n + 1 >= MAX_MODELS) {
		SV_ClearReliable (sv_client);
		SV_ClientPrintf (sv_client, PRINT_HIGH, 
			"Cmd_Modellist_f: Invalid modellist index\n");
		SV_DropClient (sv_client);
		return;
	}

	//NOTE:  This doesn't go through ClientReliableWrite since it's before the user spawns.
	//These functions are written to not overflow
	if (sv_client->num_backbuf) {
		Com_Printf ("WARNING %s: [SV_Modellist] Back buffered (%d0), clearing\n", sv_client->name, sv_client->netchan.message.cursize); 
		sv_client->num_backbuf = 0;
		SZ_Clear(&sv_client->netchan.message);
	}

	MSG_WriteByte (&sv_client->netchan.message, svc_modellist);
	MSG_WriteByte (&sv_client->netchan.message, n);
	for (s = sv.model_precache + 1 + n; *s && sv_client->netchan.message.cursize < (MAX_MSGLEN / 2); s++, n++)
		MSG_WriteString (&sv_client->netchan.message, *s);
	MSG_WriteByte (&sv_client->netchan.message, 0);

	// next msg
	MSG_WriteByte (&sv_client->netchan.message, *s ? n : 0);
}

void Cmd_PreSpawn_f (void) {
	unsigned int check;
	int buf;

	if (sv_client->state != cs_connected) {
		Com_Printf ("prespawn not valid -- already spawned\n");
		return;
	}

	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount ) {
		SV_ClearReliable (sv_client);
		Com_Printf ("Cmd_PreSpawn_f from different level\n");
		Cmd_New_f ();
		return;
	}

	buf = atoi(Cmd_Argv(2));
	if (buf >= sv.num_signon_buffers)
		buf = 0;

	if (!buf) {
		// should be three numbers following containing checksums
		check = atoi(Cmd_Argv(3));

		//Com_DPrintf ("Client check = %d\n", check);

		if (sv_mapcheck.value && check != sv.worldmodel->checksum &&
			check != sv.worldmodel->checksum2) {
			SV_ClientPrintf (sv_client, PRINT_HIGH, 
				"Map model file does not match (%s), %i != %i/%i.\n"
				"You may need a new version of the map, or the proper install files.\n",
				sv.modelname, check, sv.worldmodel->checksum, sv.worldmodel->checksum2);
			SV_DropClient (sv_client); 
			return;
		}
		sv_client->checksum = check;
	}

	//NOTE:  This doesn't go through ClientReliableWrite since it's before the user spawns.
	//These functions are written to not overflow
	if (sv_client->num_backbuf) {
		Com_Printf ("WARNING %s: [SV_PreSpawn] Back buffered (%d0), clearing\n", sv_client->name, sv_client->netchan.message.cursize); 
		sv_client->num_backbuf = 0;
		SZ_Clear(&sv_client->netchan.message);
	}

	SZ_Write (&sv_client->netchan.message, 
		sv.signon_buffers[buf],
		sv.signon_buffer_size[buf]);

	buf++;
	if (buf == sv.num_signon_buffers){	
		// all done prespawning
		MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, va("cmd spawn %i 0\n",svs.spawncount) );
	} else {	
		// need to prespawn more
		MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, 
			va("cmd prespawn %i %i\n", svs.spawncount, buf) );
	}
}

void Cmd_Spawn_f (void) {
	int i;
	client_t *client;
	edict_t	*ent;
	unsigned n;

	if (sv_client->state != cs_connected) {
		Com_Printf ("Spawn not valid -- already spawned\n");
		return;
	}

	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount ) {
		SV_ClearReliable (sv_client);
		Com_Printf ("Cmd_Spawn_f from different level\n");
		Cmd_New_f ();
		return;
	}

	n = atoi(Cmd_Argv(2));
	if (n >= MAX_CLIENTS) {
		SV_ClientPrintf (sv_client, PRINT_HIGH, "Cmd_Spawn_f: Invalid client start\n");
		SV_DropClient (sv_client); 
		return;
	}

	// send all current names, colors, and frag counts
	// FIXME: is this a good thing?
	SZ_Clear (&sv_client->netchan.message);

	// send current status of all other players

	// normally this could overflow, but no need to check due to backbuf
	for (i = n, client = svs.clients + n ; i < MAX_CLIENTS; i++, client++)
		SV_FullClientUpdateToClient (client, sv_client);
	
	// send all current light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++) {
		if ((!sv.lightstyles[i] || !sv.lightstyles[i][0]) && sv_fastconnect.value)
			continue;		// don't send empty lightstyle strings
		ClientReliableWrite_Begin (sv_client, svc_lightstyle, 
			3 + (sv.lightstyles[i] ? strlen(sv.lightstyles[i]) : 1));
		ClientReliableWrite_Byte (sv_client, (char)i);
		ClientReliableWrite_String (sv_client, sv.lightstyles[i]);
	}

	// set up the edict
	ent = sv_client->edict;

	if (sv.loadgame) { 
		// loaded games are already fully initialized 
		// if this is the last client to be connected, unpause 
		Cvar_ForceSet (&sv_paused, "0");
	} else {
		memset (&ent->v, 0, progs->entityfields * 4);
		ent->v.colormap = NUM_FOR_EDICT(ent);
		ent->v.team = 0;	// FIXME
		ent->v.netname = PR_SetString(sv_client->name);
	}

	sv_client->entgravity = 1.0;
	if (fofs_gravity)
		EdictFieldFloat(ent, fofs_gravity) = 1.0;

	sv_client->maxspeed = pm_maxspeed.value;
	if (fofs_maxspeed)
		EdictFieldFloat(ent, fofs_maxspeed) = pm_maxspeed.value;

	// force stats to be updated
	memset (sv_client->stats, 0, sizeof(sv_client->stats));

	ClientReliableWrite_Begin (sv_client, svc_updatestatlong, 6);
	ClientReliableWrite_Byte (sv_client, STAT_TOTALSECRETS);
	ClientReliableWrite_Long (sv_client, pr_global_struct->total_secrets);

	ClientReliableWrite_Begin (sv_client, svc_updatestatlong, 6);
	ClientReliableWrite_Byte (sv_client, STAT_TOTALMONSTERS);
	ClientReliableWrite_Long (sv_client, pr_global_struct->total_monsters);

	ClientReliableWrite_Begin (sv_client, svc_updatestatlong, 6);
	ClientReliableWrite_Byte (sv_client, STAT_SECRETS);
	ClientReliableWrite_Long (sv_client, pr_global_struct->found_secrets);

	ClientReliableWrite_Begin (sv_client, svc_updatestatlong, 6);
	ClientReliableWrite_Byte (sv_client, STAT_MONSTERS);
	ClientReliableWrite_Long (sv_client, pr_global_struct->killed_monsters);

	// get the client to check and download skins
	// when that is completed, a begin command will be issued
	ClientReliableWrite_Begin (sv_client, svc_stufftext, 8);
	ClientReliableWrite_String (sv_client, "skins\n" );
}

void SV_SpawnSpectator (void) {
	int i;
	edict_t	*e;

	VectorClear (sv_player->v.origin);
	VectorClear (sv_player->v.view_ofs);
	sv_player->v.view_ofs[2] = 22;
	sv_player->v.fixangle = true;
	sv_player->v.movetype = MOVETYPE_NOCLIP;	// progs can change this to MOVETYPE_FLY, for example


	// search for an info_playerstart to spawn the spectator at
	for (i = MAX_CLIENTS - 1; i < sv.num_edicts; i++) {
		e = EDICT_NUM(i);
		if (!strcmp(PR_GetString(e->v.classname), "info_player_start")) {
			VectorCopy (e->v.origin, sv_player->v.origin);
			VectorCopy (e->v.angles, sv_player->v.angles);
			return;
		}
	}
}

void Cmd_Begin_f (void) {
	unsigned pmodel = 0, emodel = 0;
	int i;

	if (sv_client->state == cs_spawned)
		return; // don't begin again
	
	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount ) {
		SV_ClearReliable (sv_client);
		Com_Printf ("Cmd_Begin_f from different level\n");
		Cmd_New_f ();
		return;
	}

	sv_client->state = cs_spawned;

	if (!sv.loadgame) {
		if (sv_client->spectator) {
			SV_SpawnSpectator ();

			if (SpectatorConnect) {
				// copy spawn parms out of the client_t
				for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
					(&pr_global_struct->parm1)[i] = sv_client->spawn_parms[i];
		
				// call the spawn function
				pr_global_struct->time = sv.time;
				pr_global_struct->self = EDICT_TO_PROG(sv_player);
				PR_ExecuteProgram (SpectatorConnect);
			}
		} else {
			// copy spawn parms out of the client_t
			for (i = 0; i < NUM_SPAWN_PARMS; i++)
				(&pr_global_struct->parm1)[i] = sv_client->spawn_parms[i];

			// call the spawn function
			pr_global_struct->time = sv.time;
			pr_global_struct->self = EDICT_TO_PROG(sv_player);
			PR_ExecuteProgram (pr_global_struct->ClientConnect);

			// actually spawn the player
			pr_global_struct->time = sv.time;
			pr_global_struct->self = EDICT_TO_PROG(sv_player);
			PR_ExecuteProgram (pr_global_struct->PutClientInServer);	
		}
	}

	// clear the net statistics, because connecting gives a bogus picture
	sv_client->netchan.frame_latency = 0;
	sv_client->netchan.frame_rate = 0;
	sv_client->netchan.drop_count = 0;
	sv_client->netchan.good_count = 0;

	//check he's not cheating

	pmodel = atoi(Info_ValueForKey (sv_client->userinfo, "pmodel"));
	emodel = atoi(Info_ValueForKey (sv_client->userinfo, "emodel"));

	if (pmodel != sv.model_player_checksum ||
		emodel != sv.eyes_player_checksum)
		SV_BroadcastPrintf (PRINT_HIGH, "%s WARNING: non standard player/eyes model detected\n", sv_client->name);

	// if we are paused, tell the client
	if (sv_paused.value) {
		ClientReliableWrite_Begin (sv_client, svc_setpause, 2);
		ClientReliableWrite_Byte (sv_client, sv_paused.value);
		SV_ClientPrintf(sv_client, PRINT_HIGH, "Server is paused.\n");
	}

	if (sv.loadgame) {
		// send a fixangle over the reliable channel to make sure it gets there
		// Never send a roll angle, because savegames can catch the server
		// in a state where it is expecting the client to correct the angle
		// and it won't happen if the game was just loaded, so you wind up
		// with a permanent head tilt
		edict_t *ent;

		ent = EDICT_NUM( 1 + (sv_client - svs.clients) );
		MSG_WriteByte (&sv_client->netchan.message, svc_setangle);
		for (i = 0; i < 2; i++)
			MSG_WriteAngle (&sv_client->netchan.message, ent->v.v_angle[i]);
		MSG_WriteAngle (&sv_client->netchan.message, 0);
	}

	sv_client->lastservertimeupdate = -99;		// force an update
}

//=============================================================================

void Cmd_NextDL_f (void) {
	byte buffer[FILE_TRANSFER_BUF_SIZE];
	int r, percent, size;

	if (!sv_client->download)
		return;

	r = sv_client->downloadsize - sv_client->downloadcount;
	r = min(r, 768);
	r = fread (buffer, 1, r, sv_client->download);
	ClientReliableWrite_Begin (sv_client, svc_download, 6+r);
	ClientReliableWrite_Short (sv_client, r);

	sv_client->downloadcount += r;
	size = sv_client->downloadsize;
	if (!size)
		size = 1;
	percent = sv_client->downloadcount*100/size;
	ClientReliableWrite_Byte (sv_client, percent);
	ClientReliableWrite_SZ (sv_client, buffer, r);

	if (sv_client->downloadcount != sv_client->downloadsize)
		return;

	fclose (sv_client->download);
	sv_client->download = NULL;

}

void OutofBandPrintf (netadr_t where, char *fmt, ...) {
	va_list argptr;
	char send[1024];

	send[0] = 0xff;
	send[1] = 0xff;
	send[2] = 0xff;
	send[3] = 0xff;
	send[4] = A2C_PRINT;
	va_start (argptr, fmt);
	vsnprintf (send + 5, sizeof(send) - 5, fmt, argptr);
	va_end (argptr);

	NET_SendPacket (NS_SERVER, strlen(send)+1, send, where);
}

void SV_NextUpload (void) {
	int percent, size;

	if (!*sv_client->uploadfn) {
		SV_ClientPrintf(sv_client, PRINT_HIGH, "Upload denied\n");
		ClientReliableWrite_Begin (sv_client, svc_stufftext, 8);
		ClientReliableWrite_String (sv_client, "stopul\n");

		// suck out rest of packet
		size = MSG_ReadShort ();	MSG_ReadByte ();
		msg_readcount += size;
		return;
	}

	size = MSG_ReadShort ();
	percent = MSG_ReadByte ();

	if (!sv_client->upload) {
		sv_client->upload = fopen(sv_client->uploadfn, "wb");
		if (!sv_client->upload) {
			Sys_Printf("Can't create %s\n", sv_client->uploadfn);
			ClientReliableWrite_Begin (sv_client, svc_stufftext, 8);
			ClientReliableWrite_String (sv_client, "stopul\n");
			*sv_client->uploadfn = 0;
			return;
		}
		Sys_Printf("Receiving %s from %d...\n", sv_client->uploadfn, sv_client->userid);
		if (sv_client->remote_snap)
			OutofBandPrintf(sv_client->snap_from, "Server receiving %s from %d...\n", sv_client->uploadfn, sv_client->userid);
	}

	fwrite (net_message.data + msg_readcount, 1, size, sv_client->upload);
	msg_readcount += size;

	Com_DPrintf ("UPLOAD: %d received\n", size);

	if (percent != 100) {
		ClientReliableWrite_Begin (sv_client, svc_stufftext, 8);
		ClientReliableWrite_String (sv_client, "nextul\n");
	} else {
		fclose (sv_client->upload);
		sv_client->upload = NULL;

		Sys_Printf("%s upload completed.\n", sv_client->uploadfn);

		if (sv_client->remote_snap) {
			char *p;

			if ((p = strchr(sv_client->uploadfn, '/')) != NULL)
				p++;
			else
				p = sv_client->uploadfn;
			OutofBandPrintf(sv_client->snap_from, "%s upload completed.\nTo download, enter:\ndownload %s\n", 
				sv_client->uploadfn, p);
		}
	}

}

void Cmd_Download_f (void) {
	char	name[MAX_QPATH], dirname[MAX_QPATH], *p;
	extern cvar_t	allow_download;
	extern cvar_t	allow_download_skins;
	extern cvar_t	allow_download_models;
	extern cvar_t	allow_download_sounds;
	extern cvar_t	allow_download_maps;
	extern cvar_t	allow_download_pakmaps;
	extern cvar_t	allow_download_gfx;
	extern cvar_t	allow_download_other;

	strlcpy (name, Cmd_Argv(1), sizeof(name));

	if (!allow_download.value || strstr(name, "..") || name[0] == '.' || IS_SLASH(name[0]))
		goto deny_download;

	for (p = name; *p; p++)
		if (IS_SLASH(*p))
			break;
	if (!*p)
		goto deny_download;			// must be in a subdir

	memcpy (dirname, name, p - name);
	dirname[p - name] = 0;

	// categorize download and check permissions
	if (!strcasecmp(dirname, "skins")) {
		if (!allow_download_skins.value)
			goto deny_download;
	} else if (strcasecmp(dirname, "progs")) {
		if (!allow_download_models.value)
			goto deny_download;
	} else if (strcasecmp(dirname, "sound")) {
		if (!allow_download_sounds.value)
			goto deny_download;
	} else if (strcasecmp(dirname, "maps")) {
		if (!allow_download_maps.value)
			goto deny_download;
	} else if (strcasecmp(dirname, "gfx")) {
		if (!allow_download_gfx.value)
			goto deny_download;
	} else if (!allow_download_other.value)
			goto deny_download;

	// cancel current download, if any
	if (sv_client->download) {
		fclose (sv_client->download);
		sv_client->download = NULL;
	}

	// lowercase name (needed for casesen file systems)
	// FIXME: why?	-- Tonik
	for (p = name; *p; p++)
		*p = (char)tolower(*p);

	sv_client->downloadsize = FS_FOpenFile (name, &sv_client->download);
	sv_client->downloadcount = 0;

	if (!sv_client->download) {
		Sys_Printf ("Couldn't download %s to %s\n", name, sv_client->name);
		goto deny_download;
	}

	// special check for maps that came from a pak file
	if (!strcasecmp(dirname, "maps") && file_from_pak && !allow_download_pakmaps.value) {
		fclose (sv_client->download);
		sv_client->download = NULL;
		goto deny_download;
	}

	// all checks passed, start downloading
	Cmd_NextDL_f ();
	Sys_Printf ("Downloading %s to %s\n", name, sv_client->name);
	return;

deny_download:
	ClientReliableWrite_Begin (sv_client, svc_download, 4);
	ClientReliableWrite_Short (sv_client, -1);
	ClientReliableWrite_Byte (sv_client, 0);
}

//=============================================================================

void SV_Say (qbool team) {
	client_t *client;
	int j, tmp;
	char *p, text[2048], t1[32] = "", *t2;

	if (Cmd_Argc () < 2)
		return;

	if (sv_client->state != cs_spawned)
		return;

	if (team)
		strlcpy (t1, Info_ValueForKey (sv_client->userinfo, "team"), sizeof(t1));

	if (sv_client->spectator && (!sv_spectalk.value || team))
		sprintf (text, "[SPEC] %s: ", sv_client->name);
	else if (team)
		sprintf (text, "(%s): ", sv_client->name);
	else {
		sprintf (text, "%s: ", sv_client->name);
	}

	if (fp_messages) {
		if (!sv_paused.value && svs.realtime < sv_client->lockedtill) {
			SV_ClientPrintf(sv_client, PRINT_CHAT,
				"You can't talk for %d more seconds\n", 
					(int) (sv_client->lockedtill - svs.realtime));
			return;
		}
		tmp = sv_client->whensaidhead - fp_messages + 1;
		if (tmp < 0)
			tmp = 10+tmp;
		if (!sv_paused.value &&
			sv_client->whensaid[tmp] && (svs.realtime - sv_client->whensaid[tmp] < fp_persecond)) {
			sv_client->lockedtill = svs.realtime + fp_secondsdead;
			if (sv_floodprotmsg.string[0])
				SV_ClientPrintf(sv_client, PRINT_CHAT,
					"FloodProt: %s\n", sv_floodprotmsg.string);
			else
				SV_ClientPrintf(sv_client, PRINT_CHAT,
					"FloodProt: You can't talk for %d seconds.\n", fp_secondsdead);
			return;
		}
		sv_client->whensaidhead++;
		if (sv_client->whensaidhead > 9)
			sv_client->whensaidhead = 0;
		sv_client->whensaid[sv_client->whensaidhead] = svs.realtime;
	}

	p = Cmd_Args();

	if (*p == '"') {
		p++;
		p[strlen(p) - 1] = 0;
	}

	strcat(text, p);
	strcat(text, "\n");

	Sys_Printf ("%s", text);

	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++) {
		if (client->state != cs_spawned)
			continue;
		if (sv_client->spectator && !sv_spectalk.value)
			if (!client->spectator)
				continue;

		if (team) {
			// the spectator team
			if (sv_client->spectator) {
				if (!client->spectator)
					continue;
			} else {
				t2 = Info_ValueForKey (client->userinfo, "team");
				if (strcmp(t1, t2) || client->spectator)
					continue;	// on different teams
			}
		}
		SV_ClientPrintf(client, PRINT_CHAT, "%s", text);
	}
}

void Cmd_Say_f(void) {
	SV_Say (false);
}

void Cmd_Say_Team_f(void) {
	SV_Say (true);
}

//============================================================================

//The client is showing the scoreboard, so send new ping times for all clients
void Cmd_Pings_f (void) {
	client_t *client;
	int j;

	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++) {
		if (client->state != cs_spawned)
			continue;

		ClientReliableWrite_Begin (sv_client, svc_updateping, 4);
		ClientReliableWrite_Byte (sv_client, j);
		ClientReliableWrite_Short (sv_client, SV_CalcPing(client));
		ClientReliableWrite_Begin (sv_client, svc_updatepl, 3);
		ClientReliableWrite_Byte (sv_client, j);
		ClientReliableWrite_Byte (sv_client, client->lossage);
	}
}

void Cmd_Kill_f (void) {
	if (sv_player->v.health <= 0) {
		SV_ClientPrintf (sv_client, PRINT_HIGH, "Can't suicide -- already dead!\n");
		return;
	}

	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(sv_player);
	PR_ExecuteProgram (pr_global_struct->ClientKill);
}

void SV_TogglePause (const char *msg) {
	int i;
	client_t *cl;
	int	newval;

	if (!msg)
		newval = (int)sv_paused.value ^ 2;
	else
		newval = (int)sv_paused.value ^ 1;

	Cvar_ForceSet (&sv_paused, va("%i", newval));

	if (msg)
		SV_BroadcastPrintf (PRINT_HIGH, "%s", msg);

	// send notification to all clients
	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
		if (cl->state < cs_connected)
			continue;
		ClientReliableWrite_Begin (cl, svc_setpause, 2);
		ClientReliableWrite_Byte (cl, sv_paused.value ? 1 : 0);
		cl->lastservertimeupdate = -99;		// force an update
	}
}

void Cmd_Pause_f (void) {
	char st[sizeof(sv_client->name) + 32];

	if (!sv_pausable.value) {
		SV_ClientPrintf (sv_client, PRINT_HIGH, "Pause not allowed.\n");
		return;
	}

	if (sv_client->spectator) {
		SV_ClientPrintf (sv_client, PRINT_HIGH, "Spectators can not pause.\n");
		return;
	}

	if (!((int)sv_paused.value & 1))
		sprintf (st, "%s paused the game\n", sv_client->name);
	else
		sprintf (st, "%s unpaused the game\n", sv_client->name);

	SV_TogglePause(st);
}

//The client is going to disconnect, so remove the connection immediately
void Cmd_Drop_f (void) {
	SV_EndRedirect ();
	if (!sv_client->spectator)
		SV_BroadcastPrintf (PRINT_HIGH, "%s dropped\n", sv_client->name);
	SV_DropClient (sv_client);	
}

//Change the bandwidth estimate for a client
void Cmd_PTrack_f (void) {
	int i;
	edict_t *ent, *tent;

	if (!sv_client->spectator)
		return;

	if (Cmd_Argc() != 2) {
		// turn off tracking
		sv_client->spec_track = 0;
		ent = EDICT_NUM(sv_client - svs.clients + 1);
		tent = EDICT_NUM(0);
		ent->v.goalentity = EDICT_TO_PROG(tent);
		return;
	}

	i = atoi(Cmd_Argv(1));
	if (i < 0 || i >= MAX_CLIENTS || svs.clients[i].state != cs_spawned ||
		svs.clients[i].spectator) {
		SV_ClientPrintf (sv_client, PRINT_HIGH, "Invalid client to track\n");
		sv_client->spec_track = 0;
		ent = EDICT_NUM(sv_client - svs.clients + 1);
		tent = EDICT_NUM(0);
		ent->v.goalentity = EDICT_TO_PROG(tent);
		return;
	}
	sv_client->spec_track = i + 1; // now tracking

	ent = EDICT_NUM(sv_client - svs.clients + 1);
	tent = EDICT_NUM(i + 1);
	ent->v.goalentity = EDICT_TO_PROG(tent);
}

//Allow clients to change userinfo
void Cmd_SetInfo_f (void) {
	int i;
	char oldval[MAX_INFO_STRING];

	if (Cmd_Argc() == 1) {
		Com_Printf ("User info settings:\n");
		Info_Print (sv_client->userinfo);
		return;
	}

	if (Cmd_Argc() != 3) {
		Com_Printf ("Usage: setinfo [ <key> <value> ]\n");
		return;
	}

	if (!strcmp(Cmd_Argv(1), "*z_ext")) {
		sv_client->extensions = atoi(Cmd_Argv(2));
		return;
	}

	if (Cmd_Argv(1)[0] == '*')
		return;		// don't set priveledged values

	strcpy(oldval, Info_ValueForKey(sv_client->userinfo, Cmd_Argv(1)));

	Info_SetValueForKey (sv_client->userinfo, Cmd_Argv(1), Cmd_Argv(2), MAX_INFO_STRING);
	// name is extracted below in ExtractFromUserInfo
	//	strlcpy (sv_client->name, Info_ValueForKey (sv_client->userinfo, "name")
	//		, sizeof(sv_client->name));
	//	SV_FullClientUpdate (sv_client, &sv.reliable_datagram);
	//	sv_client->sendinfo = true;

	if (!strcmp(Info_ValueForKey(sv_client->userinfo, Cmd_Argv(1)), oldval))
		return; // key hasn't changed

	// process any changed values
	SV_ExtractFromUserinfo (sv_client);

	i = sv_client - svs.clients;
	MSG_WriteByte (&sv.reliable_datagram, svc_setinfo);
	MSG_WriteByte (&sv.reliable_datagram, i);
	MSG_WriteString (&sv.reliable_datagram, Cmd_Argv(1));
	MSG_WriteString (&sv.reliable_datagram, Info_ValueForKey(sv_client->userinfo, Cmd_Argv(1)));
}


//Dumps the serverinfo info string
void Cmd_Serverinfo_f (void) {
	Info_Print (svs.info);
}


// We receive this command if the client doesn't support remote screenshots or has them disabled
void Cmd_Snap_f(void) {
	if (*sv_client->uploadfn) {
		*sv_client->uploadfn = 0;
		SV_BroadcastPrintf (PRINT_HIGH, "%s refused remote screenshot\n", sv_client->name);
	}
}


void SetUpClientEdict (client_t *cl, edict_t *ent)
{
	memset (&ent->v, 0, progs->entityfields * 4);
	ent->v.colormap = NUM_FOR_EDICT(ent);
	ent->v.netname = PR_SetString(cl->name);

	cl->entgravity = 1.0;
	if (fofs_gravity)
		EdictFieldFloat(ent, fofs_gravity) = 1.0;

	cl->maxspeed = pm_maxspeed.value;
	if (fofs_maxspeed)
		EdictFieldFloat(ent, fofs_maxspeed) = pm_maxspeed.value;
}


//Set client to player mode without reconnecting
static void Cmd_Join_f (void)
{
	int		i;
	client_t	*cl;
	int		numclients;
	extern cvar_t	password;

	if (sv_client->state != cs_spawned)
		return;
	if (!sv_client->spectator)
		return;		// already a player

	if (!(sv_client->extensions & Z_EXT_JOIN_OBSERVE)) {
		SV_ClientPrintf (sv_client, PRINT_HIGH, "Your QW client doesn't support this command\n");
		return;
	}

	if (password.string[0] && strcasecmp(password.string, "none")) {
		SV_ClientPrintf (sv_client, PRINT_HIGH, "This server requires a %s password. Please disconnect, set the password and reconnect as %s.\n", "player", "player");
		return;
	}

	// count players already on server
	numclients = 0;
	for (i=0,cl=svs.clients ; i<MAX_CLIENTS ; i++,cl++) {
		if (cl->state != cs_free && !cl->spectator)
			numclients++;
	}
	if (numclients >= maxclients.value) {
		SV_ClientPrintf (sv_client, PRINT_HIGH, "Can't join, all player slots full\n");
		return;
	}

	// call the prog function for removing a client
	// this will set the body to a dead frame, among other things
	pr_global_struct->self = EDICT_TO_PROG(sv_player);
	if (SpectatorDisconnect)
		PR_ExecuteProgram (SpectatorDisconnect);

	sv_client->old_frags = 0;
	SetUpClientEdict (sv_client, sv_client->edict);

	// turn the spectator into a player
	sv_client->spectator = false;
	Info_RemoveKey (sv_client->userinfo, "*spectator");

	// call the progs to get default spawn parms for the new client
	PR_ExecuteProgram (pr_global_struct->SetNewParms);
	for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
		sv_client->spawn_parms[i] = (&pr_global_struct->parm1)[i];

	// call the spawn function
	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(sv_player);
	PR_ExecuteProgram (pr_global_struct->ClientConnect);

	// actually spawn the player
	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(sv_player);
	PR_ExecuteProgram (pr_global_struct->PutClientInServer);	

	// send notification to all clients
	sv_client->sendinfo = true;
}


// Set client to spectator mode without reconnecting
static void Cmd_Observe_f (void)
{
	int		i;
	client_t	*cl;
	int		numspectators;
	extern cvar_t	maxspectators, spectator_password;

	if (sv_client->state != cs_spawned)
		return;
	if (sv_client->spectator)
		return;		// already a spectator

	if (!(sv_client->extensions & Z_EXT_JOIN_OBSERVE)) {
		SV_ClientPrintf (sv_client, PRINT_HIGH, "Your QW client doesn't support this command\n");
		return;
	}

	if (spectator_password.string[0] && strcasecmp(spectator_password.string, "none")) {
		SV_ClientPrintf (sv_client, PRINT_HIGH, "This server requires a %s password. Please disconnect, set the password and reconnect as %s.\n", "spectator", "spectator");
		return;
	}

	// count spectators already on server
	numspectators = 0;
	for (i=0,cl=svs.clients ; i<MAX_CLIENTS ; i++,cl++) {
		if (cl->state != cs_free && cl->spectator)
			numspectators++;
	}
	if (numspectators >= maxspectators.value) {
		SV_ClientPrintf (sv_client, PRINT_HIGH, "Can't join, all spectator slots full\n");
		return;
	}

	// call the prog function for removing a client
	// this will set the body to a dead frame, among other things
	pr_global_struct->self = EDICT_TO_PROG(sv_player);
	PR_ExecuteProgram (pr_global_struct->ClientDisconnect);

	sv_client->old_frags = 0;
	SetUpClientEdict (sv_client, sv_client->edict);

	// turn the player into a spectator
	sv_client->spectator = true;
	Info_SetValueForStarKey (sv_client->userinfo, "*spectator", "1", MAX_INFO_STRING);

	// call the progs to get default spawn parms for the new client
	PR_ExecuteProgram (pr_global_struct->SetNewParms);
	for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
		sv_client->spawn_parms[i] = (&pr_global_struct->parm1)[i];

	SV_SpawnSpectator ();
	
	// call the spawn function
	if (SpectatorConnect) {
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(sv_player);
		PR_ExecuteProgram (SpectatorConnect);
	}

	// send notification to all clients
	sv_client->sendinfo = true;
}

/*
=============================================================================
CHEAT COMMANDS
=============================================================================
*/

extern qbool	sv_allow_cheats;

//Sets client to godmode
void Cmd_God_f (void) {
	if (!sv_allow_cheats) {
		Com_Printf ("Cheats are not allowed on this server\n");
		return;
	}

	if ((int) (sv_player->v.flags = (int) sv_player->v.flags ^ FL_GODMODE) & FL_GODMODE)
		SV_ClientPrintf (sv_client, PRINT_HIGH, "godmode ON\n");
	else
		SV_ClientPrintf (sv_client, PRINT_HIGH, "godmode OFF\n");
}


void Cmd_Give_f (void) {
	char *t;
	int v;

	if (!sv_allow_cheats) {
		Com_Printf ("Cheats are not allowed on this server\n");
		return;
	}

	t = Cmd_Argv(1);
	v = atoi (Cmd_Argv(2));

	switch (t[0]) {
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		sv_player->v.items = (int) sv_player->v.items | IT_SHOTGUN<< (t[0] - '2');
		break;

	case 's':
		sv_player->v.ammo_shells = v;
		break;		
	case 'n':
		sv_player->v.ammo_nails = v;
		break;		
	case 'r':
		sv_player->v.ammo_rockets = v;
		break;		
	case 'h':
		sv_player->v.health = v;
		break;		
	case 'c':
		sv_player->v.ammo_cells = v;
		break;		
	}
}

void Cmd_Noclip_f (void) {
	if (sv_client->spectator)
		return;

	if (!sv_allow_cheats) {
		Com_Printf ("Cheats are not allowed on this server\n");
		return;
	}

	if (sv_player->v.movetype != MOVETYPE_NOCLIP) {
		sv_player->v.movetype = MOVETYPE_NOCLIP;
		sv_player->v.solid = SOLID_TRIGGER;
		SV_ClientPrintf (sv_client, PRINT_HIGH, "noclip ON\n");
	} else {
		sv_player->v.movetype = MOVETYPE_WALK;
		if (sv_player->v.health > 0)
			sv_player->v.solid = SOLID_SLIDEBOX;
		else
			sv_player->v.solid = SOLID_NOT;
		SV_ClientPrintf (sv_client, PRINT_HIGH, "noclip OFF\n");
	}
}

void Cmd_Fly_f (void) {
	if (sv_client->spectator)
		return;

	if (!sv_allow_cheats) {
		Com_Printf ("Cheats are not allowed on this server\n");
		return;
	}

	if (sv_player->v.solid != SOLID_SLIDEBOX)
		return;		// dead don't fly

	if (sv_player->v.movetype != MOVETYPE_FLY) {
		sv_player->v.movetype = MOVETYPE_FLY;
		SV_ClientPrintf (sv_client, PRINT_HIGH, "flymode ON\n");
	} else {
		sv_player->v.movetype = MOVETYPE_WALK;
		SV_ClientPrintf (sv_client, PRINT_HIGH, "flymode OFF\n");
	}
}

//=============================================================================

typedef struct {
	char		*name;
	void		(*func) (void);
	qbool	overrideable;
} ucmd_t;

ucmd_t ucmds[] = {
	{"new", Cmd_New_f, false},
	{"modellist", Cmd_Modellist_f, false},
	{"soundlist", Cmd_Soundlist_f, false},
	{"prespawn", Cmd_PreSpawn_f, false},
	{"spawn", Cmd_Spawn_f, false},
	{"begin", Cmd_Begin_f, false},

	{"download", Cmd_Download_f, false},
	{"nextdl", Cmd_NextDL_f, false},

	{"drop", Cmd_Drop_f, false},
	{"pings", Cmd_Pings_f, false},

	{"ptrack", Cmd_PTrack_f, false},	// used with autocam

	{"snap", Cmd_Snap_f, false},

	// issued by hand at client consoles
	{"kill", Cmd_Kill_f, true},
	{"pause", Cmd_Pause_f, false},

	{"say", Cmd_Say_f, false},
	{"say_team", Cmd_Say_Team_f, false},

	{"setinfo", Cmd_SetInfo_f, false},
	{"serverinfo", Cmd_Serverinfo_f, false},

	{"join", Cmd_Join_f, false},
	{"observe", Cmd_Observe_f, false},

	// cheat commands
	{"god", Cmd_God_f, true},
	{"give", Cmd_Give_f, true},
	{"noclip", Cmd_Noclip_f, true},
	{"fly", Cmd_Fly_f, true},

	{NULL, NULL, false}
};

int PF_tokenize_impl (char *str);

void SV_ExecuteUserCommand (char *s, qbool fromQC) {
	ucmd_t *u;
	char *command;

	Cmd_TokenizeString (s);
	command = Cmd_Argv(0);

	sv_player = sv_client->edict;
	SV_BeginRedirect (RD_CLIENT);

	
	for (u = ucmds; u->name; u++) {
		if (!strcmp (command, u->name)) {
			if (!u->overrideable) {
				u->func();
				goto out;
			}
			break;
		}
	}

	// ZQ_CLIENTCOMMAND extension
	if (GE_ClientCommand) {
		static char cmd_copy[128], args_copy[1024] /* Ouch! */;
		char *p;
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(sv_player);
		strlcpy (cmd_copy, command, sizeof(cmd_copy));
		strlcpy (args_copy, Cmd_Args(), sizeof(args_copy));
		// lowercase command to rule out the possibility of a user executing
		// a command the mod wants to block (e.g. 'join') by using uppercase
		for (p = cmd_copy; *p; p++)
			*p = (char)tolower(*p);
		((int *)pr_globals)[OFS_PARM0] = PR_SetString (cmd_copy);
		((int *)pr_globals)[OFS_PARM1] = PR_SetString (args_copy);
		PF_tokenize_impl (s);
		PR_ExecuteProgram (GE_ClientCommand);
		if (G_FLOAT(OFS_RETURN) != 0)
			goto out;	// the command was handled by the mod
	}

	if (!fromQC && SV_ParseClientCommand) {
		
		SV_EndRedirect ();	
		G_INT(OFS_PARM0) = PR_SetString(s);
		pr_global_struct->self = EDICT_TO_PROG(sv_client->edict);
		PR_ExecuteProgram ((func_t) SV_ParseClientCommand);
		goto out;
	}

	if (u->name)
		u->func();	
	else
		Com_Printf ("Bad user command: %s\n", Cmd_Argv(0));

out:
	SV_EndRedirect ();
}

//=============================================================================

void AddLinksToPmove ( areanode_t *node ) {
	link_t *l, *next;
	edict_t *check;
	int pl, i;
	physent_t *pe;
	vec3_t pmove_mins, pmove_maxs;

	for (i = 0; i < 3; i++) { 
		pmove_mins[i] = pmove.origin[i] - 256; 
		pmove_maxs[i] = pmove.origin[i] + 256; 
	}

	pl = EDICT_TO_PROG(sv_player);

	// touch linked edicts
	for (l = node->solid_edicts.next; l != &node->solid_edicts; l = next) {
		next = l->next;
		check = EDICT_FROM_AREA(l);

		if (check->v.owner == pl)
			continue;		// player's own missile
		if (check->v.solid == SOLID_BSP || check->v.solid == SOLID_BBOX || check->v.solid == SOLID_SLIDEBOX) {
			if (check == sv_player)
				continue;

			for (i = 0; i < 3; i++)
				if (check->v.absmin[i] > pmove_maxs[i] || check->v.absmax[i] < pmove_mins[i])
					break;
			if (i != 3)
				continue;
			if (pmove.numphysent == MAX_PHYSENTS)
				return;
			pe = &pmove.physents[pmove.numphysent];
			pmove.numphysent++;

			VectorCopy (check->v.origin, pe->origin);
			pe->info = NUM_FOR_EDICT(check);
			if (check->v.solid == SOLID_BSP) {
				pe->model = sv.models[(int)(check->v.modelindex)];
			} else {
				pe->model = NULL;
				VectorCopy (check->v.mins, pe->mins);
				VectorCopy (check->v.maxs, pe->maxs);
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
	if (cl->edict->v.movetype == MOVETYPE_NOCLIP) {
		if (cl->extensions & Z_EXT_PM_TYPE_NEW)
			return PM_SPECTATOR;
		return PM_OLD_SPECTATOR;
	}

	if (cl->edict->v.movetype == MOVETYPE_FLY)
		return PM_FLY;

	if (cl->edict->v.movetype == MOVETYPE_NONE)
		return PM_NONE;

	if (cl->edict->v.health <= 0)
		return PM_DEAD;

	return PM_NORMAL;
}

static byte playertouch[(SV_MAX_EDICTS + 7) / 8];

//Done before running a player command.  Clears the touch array
void SV_PreRunCmd(void) {
	memset(playertouch, 0, sizeof(playertouch));
}

void SV_RunCmd (usercmd_t *ucmd) {
	edict_t *ent;
	int i, n, oldmsec;
	vec3_t originalvel, offset;
	qbool onground;

	cmd = *ucmd;

	// chop up very long commands
	if (cmd.msec > 50) {
		oldmsec = ucmd->msec;
		cmd.msec = oldmsec / 2;
		SV_RunCmd (&cmd);
		cmd.msec = oldmsec / 2;
		cmd.impulse = 0;
		SV_RunCmd (&cmd);
		return;
	}

	sv_player->v.button0 = ucmd->buttons & 1;
	sv_player->v.button2 = (ucmd->buttons & 2) >> 1;
	sv_player->v.button1 = (ucmd->buttons & 4) >> 2;
	if (ucmd->impulse)
		sv_player->v.impulse = ucmd->impulse;

	// clamp view angles
	if (ucmd->angles[PITCH] > sv_maxpitch.value)
		ucmd->angles[PITCH] = sv_maxpitch.value;
	if (ucmd->angles[PITCH] < sv_minpitch.value)
		ucmd->angles[PITCH] = sv_minpitch.value;
	if (!sv_player->v.fixangle)
		VectorCopy (ucmd->angles, sv_player->v.v_angle);

// angles
// show 1/3 the pitch angle and all the roll angle	
	if (sv_player->v.health > 0) {
		if (!sv_player->v.fixangle) {
			sv_player->v.angles[PITCH] = -sv_player->v.v_angle[PITCH] / 3;
			sv_player->v.angles[YAW] = sv_player->v.v_angle[YAW];
		}
		sv_player->v.angles[ROLL] = 0;
	}

	sv_frametime = ucmd->msec * 0.001;
	if (sv_frametime > 0.1)
		sv_frametime = 0.1;

	if (!sv_client->spectator) {
		VectorCopy (sv_player->v.velocity, originalvel);
		onground = (int) sv_player->v.flags & FL_ONGROUND;

		pr_global_struct->frametime = sv_frametime;
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(sv_player);

		PR_ExecuteProgram (pr_global_struct->PlayerPreThink);

		if (onground && originalvel[2] < 0 && sv_player->v.velocity[2] == 0
			&& originalvel[0] == sv_player->v.velocity[0] && originalvel[1] == sv_player->v.velocity[1]
		) {
			// don't let KTeams mess with physics
			sv_player->v.velocity[2] = originalvel[2];
		}

		SV_RunThink (sv_player);
	}

	// copy player state to pmove
	VectorSubtract (sv_player->v.mins, player_mins, offset);
	VectorAdd (sv_player->v.origin, offset, pmove.origin);
	VectorCopy (sv_player->v.velocity, pmove.velocity);
	VectorCopy (sv_player->v.v_angle, pmove.angles);
	pmove.waterjumptime = sv_player->v.teleport_time;
	pmove.cmd = *ucmd;
	pmove.pm_type = SV_PMTypeForClient (sv_client);
	pmove.onground = ((int)sv_player->v.flags & FL_ONGROUND) != 0;
	pmove.jump_held = sv_client->jump_held;
#ifndef SERVERONLY
	pmove.jump_msec = 0;
#endif
	// let KTeams'/KTPro's "broken ankle" code work
	if (fofs_brokenankle && (EdictFieldFloat(sv_player, fofs_brokenankle) != 0 && pmove.velocity[2] == -270) && (pmove.cmd.buttons & BUTTON_JUMP))
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
	movevars.slidefix = (pm_slidefix.value != 0);
	movevars.airstep = (pm_airstep.value != 0);
	movevars.pground = (pm_pground.value != 0);

	// do the move
	PM_PlayerMove ();

	// get player state back out of pmove
	sv_client->jump_held = pmove.jump_held;
	sv_player->v.teleport_time = pmove.waterjumptime;
	sv_player->v.waterlevel = pmove.waterlevel;
	sv_player->v.watertype = pmove.watertype;
	if (pmove.onground) {
		sv_player->v.flags = (int) sv_player->v.flags | FL_ONGROUND;
		sv_player->v.groundentity = EDICT_TO_PROG(EDICT_NUM(pmove.physents[pmove.groundent].info));
	} else {
		sv_player->v.flags = (int) sv_player->v.flags & ~FL_ONGROUND;
	}

	VectorSubtract (pmove.origin, offset, sv_player->v.origin);
	VectorCopy (pmove.velocity, sv_player->v.velocity);
	VectorCopy (pmove.angles, sv_player->v.v_angle);

	if (!sv_client->spectator) {
		// link into place and touch triggers
		SV_LinkEdict (sv_player, true);

		// touch other objects
		for (i = 0; i < pmove.numtouch; i++) {
			n = pmove.physents[pmove.touchindex[i]].info;
			ent = EDICT_NUM(n);
			if (!ent->v.touch || (playertouch[n/8]&(1<<(n%8))))
				continue;
			pr_global_struct->self = EDICT_TO_PROG(ent);
			pr_global_struct->other = EDICT_TO_PROG(sv_player);
			PR_ExecuteProgram (ent->v.touch);
			playertouch[n / 8] |= 1 << (n % 8);
		}
	}
}

//Done after running a player command.
void SV_PostRunCmd (void) {
	vec3_t originalvel;
	qbool onground;

	if (!sv_client->spectator) {
		onground = (int) sv_player->v.flags & FL_ONGROUND;
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(sv_player);
		VectorCopy (sv_player->v.velocity, originalvel);

		PR_ExecuteProgram (pr_global_struct->PlayerPostThink);

		if (onground && originalvel[2] < 0 && sv_player->v.velocity[2] == 0
			&& originalvel[0] == sv_player->v.velocity[0] && originalvel[1] == sv_player->v.velocity[1]
		) {
			// don't let KTeams mess with physics
			sv_player->v.velocity[2] = originalvel[2];
		}

		SV_RunNewmis ();
	} else if (SpectatorThink) {
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(sv_player);
		PR_ExecuteProgram (SpectatorThink);
	}
}

void SV_ExecuteClientMove (client_t *cl, usercmd_t oldest, usercmd_t oldcmd, usercmd_t newcmd) {
	int net_drop;

	if (sv_paused.value)
		return;

	SV_PreRunCmd();

	net_drop = cl->netchan.dropped;
	if (net_drop < 20) {
		while (net_drop > 2) {
			SV_RunCmd (&cl->lastcmd);
			net_drop--;
		}
	}
	if (net_drop > 1)
		SV_RunCmd (&oldest);
	if (net_drop > 0)
		SV_RunCmd (&oldcmd);
	SV_RunCmd (&newcmd);

	SV_PostRunCmd();
}


//The current net_message is parsed for the given client
void SV_ExecuteClientMessage (client_t *cl) {
	int c, seq_hash, checksumIndex;
	char *s;
	usercmd_t oldest, oldcmd, newcmd;
	client_frame_t *frame;
	vec3_t o;
	qbool move_issued = false; //only allow one move command
	byte checksum, calculatedChecksum;

	// calc ping time
	frame = &cl->frames[cl->netchan.incoming_acknowledged & UPDATE_MASK];
	frame->ping_time = svs.realtime - frame->senttime;

	// make sure the reply sequence number matches the incoming sequence number 
	if (cl->netchan.incoming_sequence >= cl->netchan.outgoing_sequence)
		cl->netchan.outgoing_sequence = cl->netchan.incoming_sequence;
	else
		cl->send_message = false;	// don't reply, sequences have slipped		

	// save time for ping calculations
	cl->frames[cl->netchan.outgoing_sequence & UPDATE_MASK].senttime = svs.realtime;
	cl->frames[cl->netchan.outgoing_sequence & UPDATE_MASK].ping_time = -1;

	sv_client = cl;
	sv_player = sv_client->edict;

	seq_hash = cl->netchan.incoming_sequence;

	// mark time so clients will know how much to predict
	// other players
 	cl->cmdtime = svs.realtime;
	cl->delta_sequence = -1;	// no delta unless requested
	while (1) {
		if (msg_badread) {
			Com_Printf ("SV_ReadClientMessage: badread\n");
			SV_DropClient (cl);
			return;
		}	

		c = MSG_ReadByte ();
		if (c == -1)
			break;

		switch (c) {
		default:
			Com_Printf ("SV_ReadClientMessage: unknown command char\n");
			SV_DropClient (cl);
			return;

		case clc_nop:
			break;

		case clc_delta:
			cl->delta_sequence = MSG_ReadByte ();
			break;

		case clc_move:
			if (move_issued)
				return;		// someone is trying to cheat...

			move_issued = true;

			checksumIndex = MSG_GetReadCount();
			checksum = (byte)MSG_ReadByte ();

			// read loss percentage
			cl->lossage = MSG_ReadByte();

			MSG_ReadDeltaUsercmd (&nullcmd, &oldest, PROTOCOL_VERSION);
			MSG_ReadDeltaUsercmd (&oldest, &oldcmd, PROTOCOL_VERSION);
			MSG_ReadDeltaUsercmd (&oldcmd, &newcmd, PROTOCOL_VERSION);

			if ( cl->state != cs_spawned )
				break;

			s = Info_ValueForKey(cl->userinfo, "chat");
			if ( s[0] ) {
// allow movement while in console
//				newcmd.forwardmove = newcmd.sidemove = newcmd.upmove = 0;
				newcmd.buttons &= BUTTON_JUMP; // only jump button allowed while in console
// somemods uses impulses for commands, so let them use
//				newcmd.impulse = 0;
			}

			// if the checksum fails, ignore the rest of the packet
			calculatedChecksum = COM_BlockSequenceCRCByte(
				net_message.data + checksumIndex + 1,
				MSG_GetReadCount() - checksumIndex - 1,
				seq_hash);

			if (calculatedChecksum != checksum) {
				Com_DPrintf ("Failed command checksum for %s(%d) (%d != %d)\n", 
					cl->name, cl->netchan.incoming_sequence, checksum, calculatedChecksum);
				return;
			}

			SV_ExecuteClientMove (cl, oldest, oldcmd, newcmd);

			cl->lastcmd = newcmd;
			cl->lastcmd.buttons = 0; // avoid multiple fires on lag
			break;

		case clc_stringcmd:	
			s = MSG_ReadString ();
			s[1023] = 0;
			SV_ExecuteUserCommand (s, false);
			break;

		case clc_tmove:
			o[0] = MSG_ReadCoord();
			o[1] = MSG_ReadCoord();
			o[2] = MSG_ReadCoord();
			// only allowed by spectators
			if (sv_client->spectator) {
				VectorCopy(o, sv_player->v.origin);
				SV_LinkEdict(sv_player, false);
			}
			break;

		case clc_upload:
			SV_NextUpload();
			break;

		}
	}
}
