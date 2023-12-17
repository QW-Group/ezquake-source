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

#ifndef SERVERONLY
void CL_ClearState(void);
void CL_ClearQueuedPackets(void);
#endif

server_static_t	svs;				// persistent server info
server_t		sv;					// local server
demo_t			demo;				// server demo struct

char	localmodels[MAX_MODELS][5];	// inline model names for precache

//char localinfo[MAX_LOCALINFO_STRING+1]; // local game info
ctxinfo_t _localinfo_;

int fofs_items2;
int fofs_maxspeed, fofs_gravity;
int fofs_movement;
int fofs_vw_index;
int fofs_hideentity;
int fofs_trackent;
int fofs_visibility;
int fofs_hide_players;
int fofs_teleported;

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
	edict_t  *svent;
	int      entnum;

	for (entnum = 0; entnum < sv.num_edicts ; entnum++)
	{
		svent = EDICT_NUM(entnum);
		if (svent->e.free)
			continue;
		// create baselines for all player slots,
		// and any other edict that has a visible model
		if (entnum > MAX_CLIENTS && !svent->v->modelindex)
			continue;

		//
		// create entity baseline
		//
		svent->e.baseline.number = entnum;
		VectorCopy (svent->v->origin, svent->e.baseline.origin);
		VectorCopy (svent->v->angles, svent->e.baseline.angles);
		svent->e.baseline.frame = svent->v->frame;
		svent->e.baseline.skinnum = svent->v->skin;
		if (entnum > 0 && entnum <= MAX_CLIENTS)
		{
			svent->e.baseline.colormap = entnum;
			svent->e.baseline.modelindex = SV_ModelIndex("progs/player.mdl");
		}
		else
		{
			svent->e.baseline.colormap = 0;
			svent->e.baseline.modelindex = svent->v->modelindex;
		}
	}
	sv.num_baseline_edicts = sv.num_edicts;
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
		PR_GameSetChangeParms();
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
		else if (!strcmp (mdl, "progs/newplayer.mdl"))
			return 62211;
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

This is called from the SV_Map_f() function, and when loading .sav files
================
*/
void SV_SpawnServer(char *mapname, qbool devmap, char* entityfile, qbool loading_savegame)
{
	extern func_t ED_FindFunctionOffset (char *name);

	edict_t *ent;
	int i;
	int skill_level = current_skill;

	extern cvar_t sv_loadentfiles, sv_loadentfiles_dir;
	char *entitystring;
	char oldmap[MAP_NAME_LEN];
	extern qbool	sv_allow_cheats;
	extern cvar_t	sv_cheats, sv_paused, sv_bigcoords;

	sv_error = false;

	// store old map name
	snprintf (oldmap, MAP_NAME_LEN, "%s", sv.mapname);

	Con_DPrintf ("SpawnServer: %s\n",mapname);

#ifndef SERVERONLY
	// As client+server we do it here.
	// As serveronly we do it in NET_Init().
	NET_InitServer();
#endif

	SV_SaveSpawnparms ();
	SV_LoadAccounts();

#ifdef USE_PR2
	// remove bot clients
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if( sv_vm && svs.clients[i].isBot )
		{
			svs.clients[i].old_frags = 0;
			svs.clients[i].edict->v->frags = 0.0;
			svs.clients[i].name[0] = 0;
			svs.clients[i].state = cs_free;
			Info_RemoveAll(&svs.clients[i]._userinfo_ctx_);
			Info_RemoveAll(&svs.clients[i]._userinfoshort_ctx_);
			SV_FullClientUpdate(&svs.clients[i], &sv.reliable_datagram);
			svs.clients[i].isBot = 0;
		}
	}
#endif

	// Shutdown game.
	PR_GameShutDown();
	PR_UnLoadProgs();

	svs.spawncount++; // any partially connected client will be restarted

#ifndef SERVERONLY
	com_serveractive = false;
#endif
	sv.state = ss_dead;
	sv.paused = false;
	Cvar_SetROM(&sv_paused, "0");

	Host_ClearMemory();

#ifndef SERVERONLY
	if (!oldmap[0]) {
		Cbuf_InsertTextEx(&cbuf_server, "exec server.cfg\n");
		Cbuf_ExecuteEx(&cbuf_server);
	}
#endif

	if (loading_savegame) {
		Cvar_SetValue(&skill, skill_level);
		Cvar_SetValue(&deathmatch, 0);
		Cvar_SetValue(&coop, 0);
		Cvar_SetValue(&teamplay, 0);
		Cvar_SetValue(&maxclients, 1);
		Cvar_Set(&sv_progsname, "spprogs"); // force progsname
#ifdef USE_PR2
		Cvar_SetValue(&sv_progtype, 0); // force .dat
#endif
	}

#ifdef FTE_PEXT_FLOATCOORDS
	if (sv_bigcoords.value)
	{
		msg_coordsize = 4;
		msg_anglesize = 2;
	}
	else
	{
		msg_coordsize = 2;
		msg_anglesize = 1;
	}
#endif

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
	sv.max_edicts = MAX_EDICTS_SAFE;

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
	PR_LoadProgs ();
#ifdef WITH_NQPROGS
	PR_InitPatchTables();
#endif
	PR_InitProg();

	for (i = 0; i < sv.max_edicts; i++)
	{
		sv.edicts[i].v = (entvars_t *)((byte *)sv.game_edicts + i * pr_edict_size);
		sv.edicts[i].e.entnum = i;
		sv.edicts[i].e.area.ed = &sv.edicts[i]; // yeah, pretty funny, but this help to find which edict_t own this area (link_t)
		PR_ClearEdict(&sv.edicts[i]);
	}

	fofs_items2 = ED_FindFieldOffset ("items2"); // ZQ_ITEMS2 extension
	fofs_maxspeed = ED_FindFieldOffset ("maxspeed");
	fofs_gravity = ED_FindFieldOffset ("gravity");
	fofs_movement = ED_FindFieldOffset ("movement");
	fofs_vw_index = ED_FindFieldOffset ("vw_index");
	fofs_hideentity = ED_FindFieldOffset ("hideentity");
	fofs_trackent = ED_FindFieldOffset ("trackent");
	fofs_visibility = ED_FindFieldOffset ("visclients");
	fofs_hide_players = ED_FindFieldOffset ("hideplayers");
	fofs_teleported = ED_FindFieldOffset ("teleported");

#ifdef MVD_PEXT1_HIGHLAGTELEPORT
	if (fofs_teleported) {
		svs.mvdprotocolextension1 |= MVD_PEXT1_HIGHLAGTELEPORT;
	}
	else {
		svs.mvdprotocolextension1 &= ~MVD_PEXT1_HIGHLAGTELEPORT;
	}
#endif
#ifdef MVD_PEXT1_SERVERSIDEWEAPON
	{
		extern cvar_t sv_pext_mvdsv_serversideweapon;

		// Cheap 'ktx' detection
		if (sv_pext_mvdsv_serversideweapon.value && strstr(Cvar_String("qwm_name"), "KTX")) {
			svs.mvdprotocolextension1 |= MVD_PEXT1_SERVERSIDEWEAPON;
#ifdef MVD_PEXT1_SERVERSIDEWEAPON2
			svs.mvdprotocolextension1 |= MVD_PEXT1_SERVERSIDEWEAPON2;
#endif
		}
		else {
			svs.mvdprotocolextension1 &= ~MVD_PEXT1_SERVERSIDEWEAPON;
#ifdef MVD_PEXT1_SERVERSIDEWEAPON2
			svs.mvdprotocolextension1 &= ~MVD_PEXT1_SERVERSIDEWEAPON2;
#endif
		}

	}
#endif
#ifdef MVD_PEXT1_DEBUG_ANTILAG
	{
		extern cvar_t sv_debug_antilag;

		if (sv_debug_antilag.value) {
			svs.mvdprotocolextension1 |= MVD_PEXT1_DEBUG_ANTILAG;
		}
		else {
			svs.mvdprotocolextension1 &= ~MVD_PEXT1_DEBUG_ANTILAG;
		}
	}
#endif
#ifdef MVD_PEXT1_DEBUG_WEAPON
	{
		extern cvar_t sv_debug_weapons;

		if (sv_debug_weapons.value) {
			svs.mvdprotocolextension1 |= MVD_PEXT1_DEBUG_WEAPON;
		}
		else {
			svs.mvdprotocolextension1 &= ~MVD_PEXT1_DEBUG_WEAPON;
		}
	}
#endif

	// find optional QC-exported functions.
	// we have it here, so we set it to NULL in case of PR2 progs.
	mod_SpectatorConnect = ED_FindFunctionOffset ("SpectatorConnect");
	mod_SpectatorThink = ED_FindFunctionOffset ("SpectatorThink");
	mod_SpectatorDisconnect = ED_FindFunctionOffset ("SpectatorDisconnect");
	mod_ChatMessage = ED_FindFunctionOffset ("ChatMessage");
	mod_UserInfo_Changed = ED_FindFunctionOffset ("UserInfo_Changed");
	mod_ConsoleCmd = ED_FindFunctionOffset ("ConsoleCmd");
	mod_UserCmd = ED_FindFunctionOffset ("UserCmd");
	mod_localinfoChanged = ED_FindFunctionOffset ("localinfoChanged");
	GE_ClientCommand = ED_FindFunctionOffset ("GE_ClientCommand");
	GE_PausedTic = ED_FindFunctionOffset ("GE_PausedTic");
	GE_ShouldPause = ED_FindFunctionOffset ("GE_ShouldPause");

	// leave slots at start for clients only
	sv.num_edicts = MAX_CLIENTS+1;
	for (i=0 ; i<MAX_CLIENTS ; i++)
	{
		ent = EDICT_NUM(i+1);
		// restore client name.
		PR_SetEntityString(ent, ent->v->netname, svs.clients[i].name);
		// reserve edict.
		svs.clients[i].edict = ent;
		//ZOID - make sure we update frags right
		svs.clients[i].old_frags = 0;
	}

	// fill sv.mapname and sv.modelname with new map name
	strlcpy (sv.mapname, mapname, sizeof(sv.mapname));
	snprintf (sv.modelname, sizeof(sv.modelname), "maps/%s.bsp", sv.mapname);
#ifndef SERVERONLY
	// set cvar
	Cvar_ForceSet (&host_mapname, mapname);
#endif

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

	{
		extern cvar_t sv_extlimits, sv_bspversion;

		if (sv_extlimits.value == 0 || (sv_extlimits.value == 2 && sv_bspversion.value < 2)) {
			sv.max_edicts = min(sv.max_edicts, MAX_EDICTS_SAFE);
		}
	}

	sv.map_checksum2 = Com_TranslateMapChecksum (sv.mapname, sv.map_checksum2);
	sv.static_entity_count = 0;

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
	for (i = 1; i < CM_NumInlineModels(); i++)
	{
		sv.model_precache[1+i] = localmodels[i];
		sv.models[i+1] =  CM_InlineModel (localmodels[i]);
	}

	//check player/eyes models for hacks
	sv.model_player_checksum = SV_CheckModel("progs/player.mdl");
	sv.model_newplayer_checksum = SV_CheckModel("progs/newplayer.mdl");
	sv.eyes_player_checksum = SV_CheckModel("progs/eyes.mdl");

	//
	// spawn the rest of the entities on the map
	//

	// precache and static commands can be issued during
	// map initialization
	sv.state = ss_loading;
#ifndef SERVERONLY
	com_serveractive = true;
#endif

	ent = EDICT_NUM(0);
	ent->e.free = false;
	PR_SetEntityString(ent, ent->v->model, sv.modelname);
	ent->v->modelindex = 1;		// world model
	ent->v->solid = SOLID_BSP;
	ent->v->movetype = MOVETYPE_PUSH;

	// information about the server
	PR_SetEntityString(ent, ent->v->netname, VersionStringFull());
	PR_SetEntityString(ent, ent->v->targetname, SERVER_NAME);
	ent->v->impulse = VERSION_NUM;
	ent->v->items = pr_numbuiltins - 1;

	PR_SetGlobalString(PR_GLOBAL(mapname), sv.mapname);
	// serverflags are for cross level information (sigils)
	PR_GLOBAL(serverflags) = svs.serverflags;
	if (pr_nqprogs)
	{
		pr_globals[35] = deathmatch.value;
		pr_globals[36] = coop.value;
		pr_globals[37] = teamplay.value;
		NQP_Reset ();
	}

	if (pr_nqprogs)
	{
		// register the cvars that NetQuake provides for mod use
		const char **var, *nqcvars[] = {"gamecfg", "scratch1", "scratch2", "scratch3", "scratch4",
			"saved1", "saved2", "saved3", "saved4", "savedgamecfg", "temp1", NULL};
		for (var = nqcvars; *var; var++)
			Cvar_Create((char *)/*stupid const warning*/ *var, "0", 0);
	}

	// run the frame start qc function to let progs check cvars
	if (!pr_nqprogs)
		SV_ProgStartFrame (false);

	// ********* External Entity support (.ent file(s) in gamedir/maps) pinched from ZQuake *********
	// load and spawn all other entities
	entitystring = NULL;
	if ((int)sv_loadentfiles.value)
	{
		char ent_path[1024] = {0};

		if (!entityfile || !entityfile[0])
			entityfile = sv.mapname;

		// first try maps/sv_loadentfiles_dir/
		if (sv_loadentfiles_dir.string[0])
		{
			snprintf(ent_path, sizeof(ent_path), "maps/%s/%s.ent", sv_loadentfiles_dir.string, entityfile);
			entitystring = (char *) FS_LoadHunkFile(ent_path, NULL);
		}

		// try maps/ if not loaded yet.
		if (!entitystring)
		{
			snprintf(ent_path, sizeof(ent_path), "maps/%s.ent", entityfile);
			entitystring = (char *) FS_LoadHunkFile(ent_path, NULL);
		}

		if (entitystring) {
			Con_DPrintf ("Using entfile %s\n", ent_path);
		}
	}

	if (!entitystring) {
		entitystring = CM_EntityString();
	}

	PR_LoadEnts(entitystring);
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

	// calltimeofday.
	{
		extern void PF_calltimeofday (void);
		pr_global_struct->time = sv.time;
		pr_global_struct->self = 0;

		PF_calltimeofday();
	}

	Con_DPrintf ("Server spawned.\n");

	// we change map - clear whole demo struct and sent initial state to all dest if any (for QTV only I thought)
	SV_MVD_Record(NULL, true);

#ifndef SERVERONLY
	CL_ClearState();
	CL_ClearQueuedPackets();
#endif
}

#endif // !CLIENTONLY
