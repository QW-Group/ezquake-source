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

	$Id: sv_init.c 768 2008-02-29 06:56:15Z qqshka $
*/

#include "qwsvdef.h"
//#include "crc.c"

server_static_t	svs;				// persistent server info
server_t		sv;					// local server
demo_t			demo;				// server demo struct
//entity_state_t	cl_state_entities[MAX_CLIENTS][UPDATE_BACKUP][MAX_PACKET_ENTITIES]; // client entities

char	localmodels[MAX_MODELS][5];	// inline model names for precache

//char localinfo[MAX_LOCALINFO_STRING+1]; // local game info
ctxinfo_t _localinfo_;

#ifdef USE_PR2
//storage for client names for -progtype 0 (VM_NONE)
char clientnames[MAX_CLIENTS][CLIENT_NAME_LEN]; //clientnames for -progtype 0
#endif

int fofs_items2;
int fofs_maxspeed, fofs_gravity;
int fofs_movement;
int fofs_vw_index;
int fofs_hideentity;

/*
================
SV_ModelIndex

================
*/
int SV_ModelIndex (char *name)
{
	int i;

	if (!name || !name[0])
		return 0;

	for (i=0 ; i<MAX_MODELS && sv.model_precache[i] ; i++)
		if (!strcmp(sv.model_precache[i], name))
			return i;
	if (i==MAX_MODELS || !sv.model_precache[i])
		SV_Error ("SV_ModelIndex: model %s not precached", name);
	return i;
}

/*
================
SV_FlushSignon

Moves to the next signon buffer if needed
================
*/
void SV_FlushSignon (void)
{
	if (sv.signon.cursize < sv.signon.maxsize - 512)
		return;

	if (sv.num_signon_buffers == MAX_SIGNON_BUFFERS-1)
		SV_Error ("sv.num_signon_buffers == MAX_SIGNON_BUFFERS-1");

	sv.signon_buffer_size[sv.num_signon_buffers-1] = sv.signon.cursize;
	sv.signon.data = sv.signon_buffers[sv.num_signon_buffers];
	sv.num_signon_buffers++;
	sv.signon.cursize = 0;
}

/*
================
SV_CreateBaseline

Entity baselines are used to compress the update messages
to the clients -- only the fields that differ from the
baseline will be transmitted
================
*/
static void SV_CreateBaseline (void)
{
	int			i;
	edict_t			*svent;
	int				entnum;

	for (entnum = 0; entnum < sv.num_edicts ; entnum++)
	{
		svent = EDICT_NUM(entnum);
		if (svent->e->free)
			continue;
		// create baselines for all player slots,
		// and any other edict that has a visible model
		if (entnum > MAX_CLIENTS && !svent->v.modelindex)
			continue;

		//
		// create entity baseline
		//
		VectorCopy (svent->v.origin, svent->e->baseline.origin);
		VectorCopy (svent->v.angles, svent->e->baseline.angles);
		svent->e->baseline.frame = svent->v.frame;
		svent->e->baseline.skinnum = svent->v.skin;
		if (entnum > 0 && entnum <= MAX_CLIENTS)
		{
			svent->e->baseline.colormap = entnum;
			svent->e->baseline.modelindex = SV_ModelIndex("progs/player.mdl");
		}
		else
		{
			svent->e->baseline.colormap = 0;
			svent->e->baseline.modelindex = SV_ModelIndex(
#ifdef USE_PR2
				PR2_GetString(svent->v.model)
#else
				PR_GetString(svent->v.model)
#endif
			                             );
		}

		//
		// flush the signon message out to a separate buffer if
		// nearly full
		//
		SV_FlushSignon ();

		//
		// add to the message
		//
		MSG_WriteByte (&sv.signon,svc_spawnbaseline);
		MSG_WriteShort (&sv.signon,entnum);

		MSG_WriteByte (&sv.signon, svent->e->baseline.modelindex);
		MSG_WriteByte (&sv.signon, svent->e->baseline.frame);
		MSG_WriteByte (&sv.signon, svent->e->baseline.colormap);
		MSG_WriteByte (&sv.signon, svent->e->baseline.skinnum);
		for (i=0 ; i<3 ; i++)
		{
			MSG_WriteCoord(&sv.signon, svent->e->baseline.origin[i]);
			MSG_WriteAngle(&sv.signon, svent->e->baseline.angles[i]);
		}
	}
}


/*
================
SV_SaveSpawnparms

Grabs the current state of the progs serverinfo flags 
and each client for saving across the
transition to another level
================
*/
static void SV_SaveSpawnparms (void)
{
	int i, j;

	if (!sv.state)
		return;		// no progs loaded yet

	// serverflags is the only game related thing maintained
	svs.serverflags = PR_GLOBAL(serverflags);

	for (i=0, sv_client = svs.clients ; i<MAX_CLIENTS ; i++, sv_client++)
	{
		if (sv_client->state != cs_spawned)
			continue;

		// needs to reconnect
		sv_client->state = cs_connected;

		// call the progs to get default spawn parms for the new client
		pr_global_struct->self = EDICT_TO_PROG(sv_client->edict);
#ifdef USE_PR2
		if (sv_vm)
			PR2_GameSetChangeParms();
		else
#endif
			PR_ExecuteProgram (PR_GLOBAL(SetChangeParms));
		for (j=0 ; j<NUM_SPAWN_PARMS ; j++)
			sv_client->spawn_parms[j] = (&PR_GLOBAL(parm1))[j];

	}
}

static unsigned SV_CheckModel(char *mdl)
{
	unsigned char *buf;
	unsigned short crc;
	int filesize;
	int mark;

	mark = Hunk_LowMark ();
	buf = (byte *) FS_LoadHunkFile (mdl, &filesize);
	if (!buf)
	{
		if (!strcmp (mdl, "progs/player.mdl"))
			return 33168;
		else if (!strcmp (mdl, "progs/eyes.mdl"))
			return 6967;
		else
			SV_Error ("SV_CheckModel: could not load %s\n", mdl);
	}

	crc = CRC_Block (buf, filesize);
	Hunk_FreeToLowMark (mark);

	return crc;
}

/*
================
SV_SpawnServer

Change the server to a new map, taking all connected
clients along with it.

This is only called from the SV_Map_f() function.
================
*/
dfunction_t *ED_FindFunction (char *name);

void SV_SpawnServer (char *mapname, qbool devmap)
{
	edict_t *ent;
	int i;
#ifdef USE_PR2
	char savenames[MAX_CLIENTS][CLIENT_NAME_LEN];
#endif

//	extern cvar_t version;

	dfunction_t *f;
	extern cvar_t sv_loadentfiles;
	char *entitystring;
	char oldmap[MAP_NAME_LEN];
	extern qbool	sv_allow_cheats;
	extern cvar_t	sv_cheats, sv_paused;

	// store old map name
	snprintf (oldmap, MAP_NAME_LEN, "%s", sv.mapname);

	Con_DPrintf ("SpawnServer: %s\n",mapname);

	NET_InitServer();

	SV_SaveSpawnparms ();
	SV_LoadAccounts();
#ifdef USE_PR2
	//save client names from mod memory before unload mod and clearing VM memory by Hunk_FreeToLowMark
	memset(savenames, 0, sizeof(savenames));
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if( sv_vm && svs.clients[i].isBot ) // remove bot clients
		{
			svs.clients[i].old_frags = 0;
			svs.clients[i].edict->v.frags = 0.0;
			svs.clients[i].name[0] = 0;
			svs.clients[i].state = cs_free;
			Info_RemoveAll(&svs.clients[i]._userinfo_ctx_);
			Info_RemoveAll(&svs.clients[i]._userinfoshort_ctx_);
			SV_FullClientUpdate(&svs.clients[i], &sv.reliable_datagram);
			svs.clients[i].isBot = 0;
		}
		if (svs.clients[i].name)
			strlcpy(savenames[i], svs.clients[i].name, CLIENT_NAME_LEN);
	}
	if ( sv_vm )
		PR2_GameShutDown();
#endif

	svs.spawncount++; // any partially connected client will be restarted

	sv.state = ss_dead;
	com_serveractive = false;
	sv.paused = false;
	Cvar_SetROM(&sv_paused, "0");

	Host_ClearMemory();

//	CM_InvalidateMap ();
//	Hunk_FreeToLowMark (host_hunklevel);

	if ((int)coop.value)
		Cvar_Set (&deathmatch, "0");
	current_skill = (int) (skill.value + 0.5);
	if (current_skill < 0)
		current_skill = 0;
	Cvar_Set (&skill, va("%d", current_skill));
	if (current_skill > 3)
		current_skill = 3;

	if ((sv_cheats.value || devmap) && !sv_allow_cheats) {
		sv_allow_cheats = true;
		Info_SetValueForStarKey (svs.info, "*cheats", "ON", MAX_SERVERINFO_STRING);
	}
	else if ((!sv_cheats.value && !devmap) && sv_allow_cheats) {
		sv_allow_cheats = false;
		Info_SetValueForStarKey (svs.info, "*cheats", "", MAX_SERVERINFO_STRING);
	}


	// wipe the entire per-level structure
	// NOTE: this also set sv.mvdrecording to false, so calling SV_MVD_Record() at end of function
	memset (&sv, 0, sizeof(sv));


	sv.datagram.maxsize = sizeof(sv.datagram_buf);
	sv.datagram.data = sv.datagram_buf;
	sv.datagram.allowoverflow = true;

	sv.reliable_datagram.maxsize = sizeof(sv.reliable_datagram_buf);
	sv.reliable_datagram.data = sv.reliable_datagram_buf;

	sv.multicast.maxsize = sizeof(sv.multicast_buf);
	sv.multicast.data = sv.multicast_buf;

	sv.signon.maxsize = sizeof(sv.signon_buffers[0]);
	sv.signon.data = sv.signon_buffers[0];
	sv.num_signon_buffers = 1;

	sv.time = 1.0;

	// load progs to get entity field count
	// which determines how big each edict is
	// and allocate edicts
#ifdef USE_PR2
	sv_vm = (vm_t *) VM_Load(sv_vm, (vm_type_t) (int) sv_progtype.value, sv_progsname.string, sv_syscall, sv_sys_callex);
	if ( sv_vm )
		PR2_InitProg();
	else
#endif
	{
		PR_LoadProgs ();
		PR_InitBuiltins ();
		sv.edicts = (edict_t*) Hunk_AllocName (MAX_EDICTS * pr_edict_size, "edicts");
	}

	for (i = 0; i < MAX_EDICTS; i++)
	{
		ent = EDICT_NUM(i);
		ent->e = &sv.sv_edicts[i]; // assigning ->e field in each edict_t
		ent->e->area.ed = ent; // yeah, pretty funny, but this help to find which edict_t own this area (link_t)
	}

#ifdef USE_PR2
	fofs_items2 = ED2_FindFieldOffset ("items2"); // ZQ_ITEMS2 extension
	fofs_maxspeed = ED2_FindFieldOffset ("maxspeed");
	fofs_gravity = ED2_FindFieldOffset ("gravity");
	fofs_movement = ED2_FindFieldOffset ("movement");
	fofs_vw_index = ED2_FindFieldOffset ("vw_index");
	fofs_hideentity = ED2_FindFieldOffset ("hideentity");
#else
	fofs_items2 = ED_FindFieldOffset ("items2"); // ZQ_ITEMS2 extension
	fofs_maxspeed = ED_FindFieldOffset ("maxspeed");
	fofs_gravity = ED_FindFieldOffset ("gravity");
	fofs_movement = 0;
	fofs_vw_index = ED_FindFieldOffset ("vw_index");
	fofs_hideentity = ED_FindFieldOffset ("hideentity");
#endif

	// leave slots at start for clients only
	sv.num_edicts = MAX_CLIENTS+1;
	for (i=0 ; i<MAX_CLIENTS ; i++)
	{
		ent = EDICT_NUM(i+1);
#ifdef USE_PR2
		//restore client names
		//for -progtype 0 (VM_NONE) names stored in clientnames array
		//for -progtype 1 (VM_NATIVE) and -progtype 2 (VM_BYTECODE)  stored in mod memory
		if(sv_vm)
			svs.clients[i].name = PR2_GetString(ent->v.netname);
		else
			svs.clients[i].name = clientnames[i];
		strlcpy(svs.clients[i].name, savenames[i], CLIENT_NAME_LEN);
#endif
		svs.clients[i].edict = ent;
		//ZOID - make sure we update frags right
		svs.clients[i].old_frags = 0;
	}

	// fill sv.mapname and sv.modelname with new map name
	strlcpy (sv.mapname, mapname, sizeof(sv.mapname));
	snprintf (sv.modelname, sizeof(sv.modelname), "maps/%s.bsp", sv.mapname);
	// set cvar
	Cvar_ForceSet (&host_mapname, mapname);

	if (!(sv.worldmodel = CM_LoadMap (sv.modelname, false, &sv.map_checksum, &sv.map_checksum2))) // true if bad map
	{
		Con_Printf ("Cant load map %s, falling back to %s\n", mapname, oldmap);

		// fill mapname, sv.mapname and sv.modelname with old map name
		strlcpy (sv.mapname, oldmap, sizeof(sv.mapname)); 
		snprintf (sv.modelname, sizeof(sv.modelname), "maps/%s.bsp", sv.mapname);
		mapname = oldmap;

		// and re-load old map
		sv.worldmodel = CM_LoadMap (sv.modelname, false, &sv.map_checksum, &sv.map_checksum2);

		// this should never happen
		if (!sv.worldmodel)
			SV_Error ("CM_LoadMap: bad map");
	}

	SV_ClearWorld (); // clear physics interaction links

#ifdef USE_PR2
	if ( sv_vm )
	{
		sv.sound_precache[0] = "";
		sv.model_precache[0] = "";
	}
	else
#endif

	{
		sv.sound_precache[0] = pr_strings;
		sv.model_precache[0] = pr_strings;
	}
	sv.model_precache[1] = sv.modelname;
	sv.models[1] = sv.worldmodel;
	for (i=1 ; i< CM_NumInlineModels() ; i++)
	{
		sv.model_precache[1+i] = localmodels[i];
		sv.models[i+1] =  CM_InlineModel (localmodels[i]);
	}

	//check player/eyes models for hacks
	sv.model_player_checksum = SV_CheckModel("progs/player.mdl");
	sv.eyes_player_checksum = SV_CheckModel("progs/eyes.mdl");

	//
	// spawn the rest of the entities on the map
	//

	// precache and static commands can be issued during
	// map initialization
	sv.state = ss_loading;
	com_serveractive = true;

	ent = EDICT_NUM(0);
	ent->e->free = false;
#ifdef USE_PR2
	if ( sv_vm )
		strlcpy(PR2_GetString(ent->v.model), sv.modelname, 64);
	else
#endif
		ent->v.model = PR_SetString(sv.modelname);
	ent->v.modelindex = 1;		// world model
	ent->v.solid = SOLID_BSP;
	ent->v.movetype = MOVETYPE_PUSH;

	// information about the server
//	ent->v.netname = PR_SetString(version.string);

//	Com_Printf("KTPRO: %s\n", is_ktpro ? "yes" : "no");

	ent->v.netname = PR_SetString(is_ktpro ? QWE_SERVER_NAME " " QWE_VERSION : VersionString());
	ent->v.targetname = PR_SetString(is_ktpro ? QWE_SERVER_NAME : SERVER_NAME);
	ent->v.impulse = QWE_VERNUM;
	ent->v.items = pr_numbuiltins - 1;

#ifdef USE_PR2
	if(sv_vm)
		strlcpy((char*)PR2_GetString(pr_global_struct->mapname) , sv.mapname, 64);
	else
#endif
	PR_GLOBAL(mapname) = PR_SetString(sv.mapname);
	// serverflags are for cross level information (sigils)
	PR_GLOBAL(serverflags) = svs.serverflags;
	if (pr_nqprogs) {
		pr_globals[35] = deathmatch.value;
		pr_globals[36] = coop.value;
		pr_globals[37] = teamplay.value;
		NQP_Reset ();
	}

	if (pr_nqprogs) {
		// register the cvars that NetQuake provides for mod use
		const char **var, *nqcvars[] = {"gamecfg", "scratch1", "scratch2", "scratch3", "scratch4",
			"saved1", "saved2", "saved3", "saved4", "savedgamecfg", "temp1", NULL};
		for (var = nqcvars; *var; var++)
			Cvar_Create((char *)/*stupid const warning*/*var, "0", 0);
	}

	// run the frame start qc function to let progs check cvars
	if (!pr_nqprogs)
		SV_ProgStartFrame ();

	// ********* External Entity support (.ent file(s) in gamedir/maps) pinched from ZQuake *********
	// load and spawn all other entities
	entitystring = NULL;
	if ((int)sv_loadentfiles.value) {
		entitystring = (char *) FS_LoadHunkFile (va ("maps/%s.ent", sv.mapname), NULL);
		if (entitystring) {
			Con_DPrintf ("Using entfile maps/%s.ent\n", sv.mapname);
		}
	}

	if (!entitystring) {
		Info_SetValueForStarKey (svs.info,  "*entfile", "", MAX_SERVERINFO_STRING);
		entitystring = CM_EntityString();
	}
	
#ifdef USE_PR2
	if ( sv_vm )
		PR2_LoadEnts(entitystring);
	else
#endif
		ED_LoadFromFile (entitystring);
	// ********* End of External Entity support code *********

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
	sv.signon_buffer_size[sv.num_signon_buffers-1] = sv.signon.cursize;

	Info_SetValueForKey (svs.info, "map", sv.mapname, MAX_SERVERINFO_STRING);

#ifdef USE_PR2
	if ( !sv_vm )
#endif
		if ((f = ED_FindFunction ("timeofday")) != NULL)
		{
			date_t date;

			SV_TimeOfDay(&date);

			G_FLOAT(OFS_PARM0) = (float)date.sec;
			G_FLOAT(OFS_PARM1) = (float)date.min;
			G_FLOAT(OFS_PARM2) = (float)date.hour;
			G_FLOAT(OFS_PARM3) = (float)date.day;
			G_FLOAT(OFS_PARM4) = (float)date.mon;
			G_FLOAT(OFS_PARM5) = (float)date.year;
			G_INT(OFS_PARM6) = PR_SetTmpString(date.str);

			pr_global_struct->time = sv.time;
			pr_global_struct->self = 0;

			PR_ExecuteProgram((func_t)(f - pr_functions));
		}

	Con_DPrintf ("Server spawned.\n");

	// we change map - clear whole demo struct and sent initial state to all dest if any (for QTV only I thought)
	SV_MVD_Record(NULL, true);

#ifndef SERVERONLY
	if (!dedicated)
	{
		void CL_ClearState (void);
		CL_ClearState ();
	}
#endif
}

