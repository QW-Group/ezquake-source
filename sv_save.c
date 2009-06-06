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

	$Id: sv_save.c,v 1.13 2007-10-11 05:55:47 dkure Exp $
*/

#ifndef SERVERONLY

#include "quakedef.h"
#include "server.h"
#include "sv_world.h"

extern cvar_t maxclients;

#define	SAVEGAME_COMMENT_LENGTH	39
#define	SAVEGAME_VERSION	6

//Writes a SAVEGAME_COMMENT_LENGTH character comment
void SV_SavegameComment (char *buffer) {
	int i;
	char kills[20];

	for (i = 0; i < SAVEGAME_COMMENT_LENGTH; i++)
		buffer[i] = ' ';
	memcpy (buffer, cl.levelname, min(strlen(cl.levelname), 21));
	snprintf (kills, sizeof (kills), "kills:%3i/%-3i", cl.stats[STAT_MONSTERS], cl.stats[STAT_TOTALMONSTERS]);
	memcpy (buffer + 22, kills, strlen(kills));

	// convert space to _ to make stdio happy
	for (i = 0; i < SAVEGAME_COMMENT_LENGTH; i++)
		if (buffer[i] == ' ')
			buffer[i] = '_';

	buffer[SAVEGAME_COMMENT_LENGTH] = 0;
}

void SV_SaveGame_f (void) {
	char fname[MAX_OSPATH], comment[SAVEGAME_COMMENT_LENGTH+1];
	FILE *f;
	int i;

	if (Cmd_Argc() != 2) {
		Com_Printf ("Usage: %s <savefname> : save a game\n", Cmd_Argv(0));
		return;
	} else if (strstr(Cmd_Argv(1), "..")) {
		Com_Printf ("Relative pathfnames are not allowed.\n");
		return;
	} else if (sv.state != ss_active) {
		Com_Printf ("Not playing a local game.\n");
		return;
	} else if (cl.intermission) {
		Com_Printf ("Can't save in intermission.\n");
		return;
	} else if (deathmatch.value != 0 || coop.value != 0 || maxclients.value != 1) {
		Com_Printf ("Can't save multiplayer games.\n");
		return;
	}

	for (i = 1; i < MAX_CLIENTS; i++) {
		if (svs.clients[i].state == cs_spawned) {
			Com_Printf ("Can't save multiplayer games.\n");
			return;
		}
	}	

	if (svs.clients[0].state != cs_spawned) {
		Com_Printf ("Can't save, client #0 not spawned.\n");
		return;
	} else if (svs.clients[0].edict->v.health <= 0) {
		Com_Printf ("Can't save game with a dead player\n");
		// in fact, we can, but does it make sense?
		return;
	}

	snprintf (fname, sizeof(fname), "%s/save/%s", com_gamedir, Cmd_Argv(1));
	COM_DefaultExtension (fname, ".sav");
	
	Com_Printf ("Saving game to %s...\n", fname);
	if (!(f = fopen (fname, "w"))) {		
		FS_CreatePath (fname);
		if (!(f = fopen (fname, "w"))) {
			Com_Printf ("ERROR: couldn't open.\n");
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
	Com_Printf ("done.\n");
}

void SV_LoadGame_f (void) {
	extern cvar_t sv_progtype;
	char name[MAX_OSPATH], mapname[MAX_QPATH], str[32 * 1024], *start;
	FILE *f;
	float time, tfloat, spawn_parms[NUM_SPAWN_PARMS];
	edict_t *ent;
	int entnum, version, r;
	unsigned int i;

	if (Cmd_Argc() != 2) {
		Com_Printf ("Usage: %s <savename> : load a game\n", Cmd_Argv(0));
		return;
	}

	snprintf (name, sizeof (name), "%s/save/%s", com_gamedir, Cmd_Argv(1));
	COM_DefaultExtension (name, ".sav");

	Com_Printf ("Loading game from %s...\n", name);
	if (!(f = fopen (name, "rb"))) {
		Com_Printf ("ERROR: couldn't open.\n");
		return;
	}

	fscanf (f, "%i\n", &version);
	if (version != SAVEGAME_VERSION) {
		fclose (f);
		Com_Printf ("Savegame is version %i, not %i\n", version, SAVEGAME_VERSION);
		return;
	}

	fscanf (f, "%s\n", str);
	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		fscanf (f, "%f\n", &spawn_parms[i]);

	// this silliness is so we can load 1.06 save files, which have float skill values
	fscanf (f, "%f\n", &tfloat);
	current_skill = (int)(tfloat + 0.1);
	Cvar_Set (&skill, va("%i", current_skill));

	Cvar_SetValue (&deathmatch, 0);
	Cvar_SetValue (&coop, 0);
	Cvar_SetValue (&teamplay, 0);
	Cvar_SetValue (&maxclients, 1);

	Cvar_Set (&sv_progsname, "spprogs"); // force progsname
#ifdef USE_PR2
	Cvar_Set (&sv_progtype, "0"); // force .dat
#endif

	fscanf (f, "%s\n", mapname);
	fscanf (f, "%f\n", &time);

	Host_EndGame();

	CL_BeginLocalConnection ();

	SV_SpawnServer (mapname, false);

	if (sv.state != ss_active) {
		Com_Printf ("Couldn't load map\n");
		return;
	}

	// pause until all clients connect
	if (!(sv.paused & 1))
		SV_TogglePause (NULL, 1);

	sv.loadgame = true;

	// load the light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++) {
		fscanf (f, "%s\n", str);
		sv.lightstyles[i] = (char *) Hunk_Alloc (strlen(str) + 1);
		strlcpy (sv.lightstyles[i], str, strlen(str) + 1);
	}

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
		} else {	
			// parse an edict
			ent = EDICT_NUM(entnum);
			memset (&ent->v, 0, progs->entityfields * 4);
			ent->e->free = false;
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

#endif	//!SERVERONLY
