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
#include "crc.h"

serverPersistent_t	svs;	// persistent server info
server_t sv;				// local server

char localmodels[MAX_MODELS][5];			// inline model names for precache

char localinfo[MAX_LOCALINFO_STRING + 1];	// local game info

int SV_ModelIndex (char *name) {
	int i;
	
	if (!name || !name[0])
		return 0;

	for (i = 1; i < MAX_MODELS && sv.model_precache[i]; i++)
		if (!strcmp(sv.model_precache[i], name))
			return i;
	if (i == MAX_MODELS || !sv.model_precache[i])
		Host_Error ("SV_ModelIndex: model %s not precached", name);
	return i;
}

//Moves to the next signon buffer if needed
void SV_FlushSignon (void) {
	if (sv.signon.cursize < sv.signon.maxsize - 512)
		return;

	if (sv.num_signon_buffers == MAX_SIGNON_BUFFERS - 1)
		Host_Error ("sv.num_signon_buffers == MAX_SIGNON_BUFFERS-1");

	sv.signon_buffer_size[sv.num_signon_buffers - 1] = sv.signon.cursize;
	sv.signon.data = sv.signon_buffers[sv.num_signon_buffers];
	sv.num_signon_buffers++;
	sv.signon.cursize = 0;
}

//Entity baselines are used to compress the update messages to the clients --
//only the fields that differ from the baseline will be transmitted
void SV_CreateBaseline (void) {
	int i, entnum;	
	edict_t *svent;

	for (entnum = 0; entnum < sv.num_edicts; entnum++)	{
		svent = EDICT_NUM(entnum);
		if (svent->free)
			continue;
		// create baselines for all player slots, and any other edict that has a visible model
		if (entnum > MAX_CLIENTS && !svent->v.modelindex)
			continue;

		// create entity baseline
		VectorCopy (svent->v.origin, svent->baseline.origin);
		VectorCopy (svent->v.angles, svent->baseline.angles);
		svent->baseline.frame = svent->v.frame;
		svent->baseline.skinnum = svent->v.skin;
		if (entnum > 0 && entnum <= MAX_CLIENTS) {
			svent->baseline.colormap = entnum;
			svent->baseline.modelindex = SV_ModelIndex("progs/player.mdl");
		} else {
			svent->baseline.colormap = 0;
			svent->baseline.modelindex = SV_ModelIndex(PR_GetString(svent->v.model));
		}

		// flush the signon message out to a separate buffer if
		// nearly full
		SV_FlushSignon ();

		// add to the message
		MSG_WriteByte (&sv.signon,svc_spawnbaseline);		
		MSG_WriteShort (&sv.signon,entnum);

		MSG_WriteByte (&sv.signon, svent->baseline.modelindex);
		MSG_WriteByte (&sv.signon, svent->baseline.frame);
		MSG_WriteByte (&sv.signon, svent->baseline.colormap);
		MSG_WriteByte (&sv.signon, svent->baseline.skinnum);
		for (i = 0; i < 3; i++) {
			MSG_WriteCoord(&sv.signon, svent->baseline.origin[i]);
			MSG_WriteAngle(&sv.signon, svent->baseline.angles[i]);
		}
	}
}

//Grabs the current state of the progs serverinfo flags and each
//client for saving across the transition to another level
void SV_SaveSpawnparms (void) {
	int i, j;

	if (!sv.state)
		return;		// no progs loaded yet

	// serverflags is the only game related thing maintained
	svs.serverflags = pr_global_struct->serverflags;

	for (i = 0, sv_client = svs.clients; i < MAX_CLIENTS; i++, sv_client++) {
		if (sv_client->state != cs_spawned)
			continue;

		// needs to reconnect
		sv_client->state = cs_connected;

		// call the progs to get default spawn parms for the new client
		pr_global_struct->self = EDICT_TO_PROG(sv_client->edict);
		PR_ExecuteProgram (pr_global_struct->SetChangeParms);
		for (j = 0; j < NUM_SPAWN_PARMS; j++)
			sv_client->spawn_parms[j] = (&pr_global_struct->parm1)[j];
	}
}

//Expands the PVS and calculates the PHS
//(Potentially Hearable Set)
void SV_CalcPHS (void) {
	int rowbytes, rowwords, i, j, k, l, index, num, bitbyte, count, vcount;
	unsigned *dest, *src;
	byte *scan;

	Com_DPrintf ("Building PHS...\n");

	num = sv.worldmodel->numleafs;
	rowwords = (num + 31) >> 5;
	rowbytes = rowwords * 4;

	sv.pvs = Hunk_Alloc (rowbytes * num);
	scan = sv.pvs;
	vcount = 0;
	for (i = 0; i < num; i++, scan += rowbytes)	{
		memcpy (scan, Mod_LeafPVS(sv.worldmodel->leafs + i, sv.worldmodel), rowbytes);
		if (i == 0)
			continue;
		for (j = 0; j < num; j++) {
			if ( scan[j >> 3] & (1 << (j & 7)) ) {
				vcount++;
			}
		}
	}

	sv.phs = Hunk_Alloc (rowbytes*num);
	count = 0;
	scan = sv.pvs;
	dest = (unsigned *)sv.phs;
	for (i = 0; i < num; i++, dest += rowwords, scan += rowbytes) {
		memcpy (dest, scan, rowbytes);
		for (j = 0; j < rowbytes; j++) {
			bitbyte = scan[j];
			if (!bitbyte)
				continue;
			for (k = 0; k < 8; k++) {
				if (! (bitbyte & (1 << k)) )
					continue;
				// or this pvs row into the phs
				// +1 because pvs is 1 based
				index = ((j << 3) + k + 1);
				if (index >= num)
					continue;
				src = (unsigned *)sv.pvs + index*rowwords;
				for (l = 0; l < rowwords; l++)
					dest[l] |= src[l];
			}
		}

		if (i == 0)
			continue;
		for (j = 0; j < num; j++)
			if ( ((byte *)dest)[j >> 3] & (1 << (j &7 )) )
				count++;
	}
	Com_DPrintf ("Average leafs visible / hearable / total: %i / %i / %i\n", vcount/num, count/num, num);
}

unsigned SV_CheckModel(char *mdl) {
	byte stackbuf[1024];		// avoid dirtying the cache heap
	byte *buf;
	unsigned short crc;

	buf = (byte *) FS_LoadStackFile (mdl, stackbuf, sizeof(stackbuf));
	if (!buf)
		Host_Error ("SV_CheckModel: could not load %s\n", mdl);
	crc = CRC_Block(buf, com_filesize);

	return crc;
}

cvar_t	sv_loadentfiles = {"sv_loadentfiles", "0"};
void SV_LoadEntFile (void) {
	char name[MAX_OSPATH], *data, crc[32];

	Info_SetValueForStarKey (svs.info,  "*entfile", "", MAX_SERVERINFO_STRING);

	if (!sv_loadentfiles.value)
		return;

	COM_StripExtension (sv.worldmodel->name, name);
	strcat (name, ".ent");

	data = (char *) FS_LoadHunkFile (name);
	if (!data)
		return;

	sv.worldmodel->entities = data;

	Com_DPrintf ("Loaded entfile %s\n", name);

	sprintf (crc, "%i", CRC_Block ((byte *)data, com_filesize));
	Info_SetValueForStarKey (svs.info, "*entfile", crc, MAX_SERVERINFO_STRING);
}

//Change the server to a new map, taking all connected clients along with it.
//This is only called from the SV_Map_f() function.
void SV_SpawnServer (char *server, qboolean devmap) {
	edict_t *ent;
	int i;
	extern qboolean	sv_allow_cheats;
	extern cvar_t sv_cheats;

	Com_DPrintf ("SpawnServer: %s\n",server);

	SV_SaveSpawnparms ();

	svs.spawncount++;		// any partially connected client will be restarted

	sv.state = ss_dead;
	com_serveractive = false;
	sv.paused = false;

	Host_ClearMemory();

	if (deathmatch.value)
		Cvar_Set (&coop, "0");
	current_skill = (int)(skill.value + 0.5);
	if (current_skill < 0)
		current_skill = 0;
	if (current_skill > 3)
		current_skill = 3;
	Cvar_Set (&skill, va("%d", (int)current_skill));

	if ((sv_cheats.value || devmap) && !sv_allow_cheats) {
		sv_allow_cheats = true;
		Info_SetValueForStarKey (svs.info, "*cheats", "ON", MAX_SERVERINFO_STRING);
	} else if ((!sv_cheats.value && !devmap) && sv_allow_cheats) {
		sv_allow_cheats = false;
		Info_SetValueForStarKey (svs.info, "*cheats", "", MAX_SERVERINFO_STRING);
	}

	// wipe the entire per-level structure
	memset (&sv, 0, sizeof(sv));

	SZ_Init (&sv.datagram, sv.datagram_buf, sizeof(sv.datagram_buf));
	sv.datagram.allowoverflow = true;

	SZ_Init (&sv.reliable_datagram, sv.reliable_datagram_buf, sizeof(sv.reliable_datagram_buf));

	SZ_Init (&sv.multicast, sv.multicast_buf, sizeof(sv.multicast_buf));

	SZ_Init (&sv.master, sv.master_buf, sizeof(sv.master_buf));
	
	SZ_Init (&sv.signon, sv.signon_buffers[0], sizeof(sv.signon_buffers[0]));
	sv.num_signon_buffers = 1;

	// load progs to get entity field count
	// which determines how big each edict is
	PR_LoadProgs ();

	// allocate edicts
	sv.edicts = Hunk_AllocName (MAX_EDICTS*pr_edict_size, "edicts");

	// leave slots at start for clients only
	sv.num_edicts = MAX_CLIENTS+1;
	for (i = 0; i < MAX_CLIENTS; i++) {
		ent = EDICT_NUM(i+1);
		svs.clients[i].edict = ent;
		//ZOID - make sure we update frags right
		svs.clients[i].old_frags = 0;
	}

	sv.time = 1.0;

#ifndef SERVERONLY
	{
		void R_PreMapLoad(char *mapname);
		R_PreMapLoad(server);
	}
#endif

	com_serveractive = true;
	Q_strncpyz (sv.name, server, sizeof(sv.name));
	Q_snprintfz (sv.modelname, sizeof(sv.modelname), "maps/%s.bsp", server);
	sv.worldmodel = Mod_ForName (sv.modelname, true);
	SV_CalcPHS ();

	// clear physics interaction links
	SV_ClearWorld ();

	sv.sound_precache[0] = pr_strings;

	sv.model_precache[0] = pr_strings;
	sv.model_precache[1] = sv.modelname;
	sv.models[1] = sv.worldmodel;
	for (i = 1; i < sv.worldmodel->numsubmodels; i++) {
		sv.model_precache[1+i] = localmodels[i];
		sv.models[i + 1] = Mod_ForName (localmodels[i], false);
	}

	//check player/eyes models for hacks
	sv.model_player_checksum = SV_CheckModel("progs/player.mdl");
	sv.eyes_player_checksum = SV_CheckModel("progs/eyes.mdl");

	// spawn the rest of the entities on the map

	// precache and static commands can be issued during map initialization
	sv.state = ss_loading;

	ent = EDICT_NUM(0);
	ent->free = false;
	ent->v.model = PR_SetString(sv.worldmodel->name);
	ent->v.modelindex = 1;		// world model
	ent->v.solid = SOLID_BSP;
	ent->v.movetype = MOVETYPE_PUSH;

	pr_global_struct->mapname = PR_SetString(sv.name);
	// serverflags are for cross level information (sigils)
	pr_global_struct->serverflags = svs.serverflags;

	// run the frame start qc function to let progs check cvars
	SV_ProgStartFrame ();

	// check for a custom entity file
	SV_LoadEntFile ();

	// load and spawn all other entities
	ED_LoadFromFile (sv.worldmodel->entities);

	// look up some model indexes for specialized message compression
	SV_FindModelNumbers ();

	// all spawning is completed, any further precache statements
	// or prog writes to the signon message are errors
	sv.state = ss_active;

	// run two frames to allow everything to settle
	SV_Physics ();
	sv.time += 0.1;
	SV_Physics ();
	sv.time += 0.1;
	sv.old_time = sv.time;

	// save movement vars
	SV_SetMoveVars();

	// create a baseline for more efficient communications
	SV_CreateBaseline ();
	sv.signon_buffer_size[sv.num_signon_buffers - 1] = sv.signon.cursize;

	Info_SetValueForKey (svs.info, "map", sv.name, MAX_SERVERINFO_STRING);
	Com_DPrintf ("Server spawned.\n");
}
