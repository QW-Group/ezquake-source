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

// sv_save.c

#ifdef SERVERONLY
#include "qwsvdef.h"
#else
#include "quakedef.h"
#include "vfs.h"
#include "server.h"
#include "sv_world.h"
#endif

extern cvar_t maxclients;

#define	SAVEGAME_COMMENT_LENGTH	39
#define	SAVEGAME_VERSION	6

static void SV_SaveGameFileName(char* buffer, int buffer_size, char* name)
{
#ifdef SERVERONLY
	snprintf (buffer, buffer_size, "%s/save/%s", fs_gamedir, name);
#else
	snprintf (buffer, buffer_size, "%s/save/%s", com_gamedir, name);
#endif
}

//Writes a SAVEGAME_COMMENT_LENGTH character comment
void SV_SavegameComment (char *buffer) {
	int i;
	char kills[20];
#ifdef SERVERONLY
	char *mapname = sv.mapname;
	int killed_monsters = (int)PR_GLOBAL(killed_monsters);
	int total_monsters = (int)PR_GLOBAL(total_monsters);
#else
	char *mapname = cl.levelname;
	int killed_monsters = cl.stats[STAT_MONSTERS];
	int total_monsters = cl.stats[STAT_TOTALMONSTERS];
#endif
	if (!mapname || !*mapname)
		mapname = "Unnamed_Level";

	for (i = 0; i < SAVEGAME_COMMENT_LENGTH; i++)
		buffer[i] = ' ';
	memcpy (buffer, mapname, min(strlen(mapname), 21));
	snprintf (kills, sizeof (kills), "kills:%3i/%-3i", killed_monsters, total_monsters);
	memcpy (buffer + 22, kills, strlen(kills));

	// convert space to _ to make stdio happy
	for (i = 0; i < SAVEGAME_COMMENT_LENGTH; i++)
		if (buffer[i] == ' ')
			buffer[i] = '_';

	buffer[SAVEGAME_COMMENT_LENGTH] = 0;
}

void SV_SaveGame_f(void)
{
	char fname[MAX_OSPATH], comment[SAVEGAME_COMMENT_LENGTH+1];
	FILE *f;
	int i;

	if (Cmd_Argc() != 2) {
		Con_Printf ("Usage: %s <savefname> : save a game\n", Cmd_Argv(0));
		return;
	} else if (strstr(Cmd_Argv(1), "..")) {
		Con_Printf ("Relative pathfnames are not allowed.\n");
		return;
	} else if (sv.state != ss_active) {
		Con_Printf ("Not playing a local game.\n");
		return;
#ifndef SERVERONLY
	} else if (cl.intermission) {
		Con_Printf ("Can't save in intermission.\n");
		return;
#endif
	} else if (deathmatch.value != 0 || coop.value != 0 || maxclients.value != 1) {
		Con_Printf ("Can't save multiplayer games.\n");
		return;
	}

	for (i = 1; i < MAX_CLIENTS; i++) {
		if (svs.clients[i].state == cs_spawned) {
			Con_Printf ("Can't save multiplayer games.\n");
			return;
		}
	}	

	if (svs.clients[0].state != cs_spawned) {
		Con_Printf ("Can't save, client #0 not spawned.\n");
		return;
	} else if (svs.clients[0].edict->v.health <= 0) {
		Con_Printf ("Can't save game with a dead player\n");
		// in fact, we can, but does it make sense?
		return;
	}

	SV_SaveGameFileName (fname, sizeof(fname), Cmd_Argv(1));
	COM_DefaultExtension (fname, ".sav");
	
	Con_Printf ("Saving game to %s...\n", fname);
	if (!(f = fopen (fname, "w"))) {		
		FS_CreatePath (fname);
		if (!(f = fopen (fname, "w"))) {
			Con_Printf ("ERROR: couldn't open.\n");
			return;
		}
	}

	fprintf (f, "%i\n", SAVEGAME_VERSION);
	SV_SavegameComment (comment);
	fprintf (f, "%s\n", comment);
	for (i = 0 ; i < NUM_SPAWN_PARMS; i++)
		fprintf (f, "%f\n", svs.clients->spawn_parms[i]);
	fprintf (f, "%d\n", current_skill);
	fprintf (f, "%s\n", sv.mapname);
	fprintf (f, "%f\n", sv.time);

	// write the light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++) {
		if (sv.lightstyles[i])
			fprintf (f, "%s\n", sv.lightstyles[i]);
		else
			fprintf (f,"m\n");
	}

	ED_WriteGlobals (f);
	for (i = 0; i < sv.num_edicts; i++) {
		ED_Write (f, EDICT_NUM(i));
		fflush (f);
	}
	fclose (f);
	Con_Printf ("done.\n");

	// force cache rebuild.
	FS_FlushFSHash();
}

void SV_LoadGame_f(void)
{
	char name[MAX_OSPATH], mapname[MAX_QPATH], str[32 * 1024], *start;
	FILE *f;
	float time, tfloat, spawn_parms[NUM_SPAWN_PARMS];
	edict_t *ent;
	int entnum, version, r;
	unsigned int i;

	if (Cmd_Argc() != 2) {
		Con_Printf ("Usage: %s <savename> : load a game\n", Cmd_Argv(0));
		return;
	}

	SV_SaveGameFileName (name, sizeof(name), Cmd_Argv(1));
	COM_DefaultExtension (name, ".sav");

	Con_Printf ("Loading game from %s...\n", name);
	if (!(f = fopen (name, "rb"))) {
		Con_Printf ("ERROR: couldn't open.\n");
		return;
	}

	if (fscanf (f, "%i\n", &version) != 1) {
		fclose (f);
		Con_Printf ("Error reading savegame data\n");
		return;
	}

	if (version != SAVEGAME_VERSION) {
		fclose (f);
		Con_Printf ("Savegame is version %i, not %i\n", version, SAVEGAME_VERSION);
		return;
	}

	if (fscanf (f, "%s\n", str) != 1) {
		fclose (f);
		Con_Printf ("Error reading savegame data\n");
		return;
	}
	for (i = 0; i < NUM_SPAWN_PARMS; i++) {
		if (fscanf (f, "%f\n", &spawn_parms[i]) != 1) {
			fclose (f);
			Con_Printf ("Error reading savegame data\n");
			return;
		}
	}

	// this silliness is so we can load 1.06 save files, which have float skill values
	if (fscanf (f, "%f\n", &tfloat) != 1) {
		fclose (f);
		Con_Printf ("Error reading savegame data\n");
		return;
	}
	current_skill = (int)(tfloat + 0.1);

	if (fscanf (f, "%s\n", mapname) != 1) {
		fclose (f);
		Con_Printf ("Error reading savegame data\n");
		return;
	}
	if (fscanf (f, "%f\n", &time) != 1) {
		fclose (f);
		Con_Printf ("Error reading savegame data\n");
		return;
	}

#ifndef SERVERONLY
	Host_EndGame();
	CL_BeginLocalConnection ();
#endif

	SV_SpawnServer(mapname, false, NULL, true);

	if (sv.state != ss_active) {
		Con_Printf ("Couldn't load map\n");
		fclose (f);
		return;
	}

	// load the light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++) {
		size_t length;
		if (fscanf (f, "%s\n", str) != 1) {
			Con_Printf("Couldn't read lightstyles\n");
			fclose (f);
			return;
		}
		length = strlen(str) + 1;
		sv.lightstyles[i] = (char *) Hunk_Alloc (length);
		strlcpy (sv.lightstyles[i], str, length);
	}

	// pause until all clients connect
	if (!(sv.paused & 1))
		SV_TogglePause (NULL, 1);

	sv.loadgame = true;

	// load the edicts out of the savegame file
	entnum = -1;		// -1 is the globals
	while (!feof(f)) {
		for (i = 0; i < sizeof(str) - 1; i++) {
			r = fgetc (f);
			if (r == EOF || !r)
				break;
			str[i] = r;
			if (r == '}') {
				i++;
				break;
			}
		}
		if (i == sizeof(str)-1)
			Host_Error ("Loadgame buffer overflow");
		str[i] = 0;
		start = str;
		start = COM_Parse(str);
		if (!com_token[0])
			break;		// end of file

		if (strcmp(com_token,"{"))
			Host_Error ("First token isn't a brace");

		if (entnum == -1) {	
			// parse the global vars
			ED_ParseGlobals (start);
		}
		else {	
			// parse an edict
			ent = EDICT_NUM(entnum);
			ED_ClearEdict (ent); // FIXME: we also clear world edict here, is it OK?
			ED_ParseEdict (start, ent);

			// link it into the bsp tree
			if (!ent->e->free)
				SV_LinkEdict (ent, false);
		}
		entnum++;
	}

	sv.num_edicts = entnum;
	sv.time = time;

	fclose (f);

	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		svs.clients->spawn_parms[i] = spawn_parms[i];
}

