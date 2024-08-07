/*
Copyright (C) 2000-2003       Anton Gavrilov, A Nourai

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

$Id: teamplay.c,v 1.96 2007-10-23 08:51:02 himan Exp $
*/

#include "quakedef.h"
#include "teamplay.h"

/********************************* .LOC FILES *********************************/

static locdata_t *locdata = NULL;
static int loc_count = 0;

static void TP_ClearLocs(void)
{
	locdata_t *node, *temp;

	for (node = locdata; node; node = temp) {
		Q_free(node->name);
		temp = node->next;
		Q_free(node);
	}

	locdata = NULL;
	loc_count = 0;
}

static void TP_ClearLocs_f(void)
{
	int num_locs = 0;

	if (Cmd_Argc() > 1) {
		Com_Printf("clearlocs : Clears all locs in memory.\n");
		return;
	}

	num_locs = loc_count;
	TP_ClearLocs();

	Com_Printf("Cleared %d locs.\n", num_locs);
}

static void TP_AddLocNode(vec3_t coord, char *name)
{
	locdata_t *newnode, *node;

	newnode = (locdata_t *)Q_malloc(sizeof(locdata_t));
	newnode->name = Q_strdup(name);
	newnode->next = NULL;
	memcpy(newnode->coord, coord, sizeof(vec3_t));

	if (!locdata) {
		locdata = newnode;
		loc_count++;
		return;
	}

	for (node = locdata; node->next; node = node->next)
		;

	node->next = newnode;
	loc_count++;
}

#define SKIPBLANKS(ptr) while (*ptr == ' ' || *ptr == '\t' || *ptr == '\r') ptr++
#define SKIPTOEOL(ptr) {while (*ptr != '\n' && *ptr != 0) ptr++; if (*ptr == '\n') ptr++;}

qbool TP_LoadLocFile(char *path, qbool quiet)
{
	char *buf, *p, locname[MAX_OSPATH] = { 0 }, location[MAX_LOC_NAME];
	int i, n, sign, line, nameindex, overflow;
	vec3_t coord;

	if (!*path) {
		return false;
	}

	strlcpy(locname, "locs/", sizeof(locname));
	if (strlen(path) + strlen(locname) + 2 + 4 > MAX_OSPATH) {
		Com_Printf("TP_LoadLocFile: path name > MAX_OSPATH\n");
		return false;
	}

	strlcat(locname, path, sizeof(locname) - strlen(locname));
	COM_DefaultExtension(locname, ".loc", sizeof(locname));

	if (!(buf = (char *)FS_LoadHeapFile(locname, NULL))) {
		if (!quiet) {
			Com_Printf("Could not load %s\n", locname);
		}
		return false;
	}

	TP_ClearLocs();

	// Parse the whole file now
	p = buf;
	line = 1;

	while (1) {
		SKIPBLANKS(p);

		if (!*p) {
			goto _endoffile;
		}
		else if (*p == '\n') {
			p++;
			goto _endofline;
		}
		else if (*p == '/' && p[1] == '/') {
			SKIPTOEOL(p);
			goto _endofline;
		}

		// parse three ints
		for (i = 0; i < 3; i++) {
			n = 0;
			sign = 1;
			while (1) {
				switch (*p++) {
					case ' ':
					case '\t':
						goto _next;
					case '-':
						if (n) {
							Com_Printf("Locfile error (line %d): unexpected '-'\n", line);
							SKIPTOEOL(p);
							goto _endofline;
						}
						sign = -1;
						break;
					case '0': case '1': case '2': case '3': case '4':
					case '5': case '6': case '7': case '8': case '9':
						n = n * 10 + (p[-1] - '0');
						break;
					default:	// including eol or eof
						Com_Printf("Locfile error (line %d): couldn't parse coords\n", line);
						SKIPTOEOL(p);
						goto _endofline;
				}
			}
		_next:
			n *= sign;
			coord[i] = n / 8.0;

			SKIPBLANKS(p);
		}

		// parse location name
		overflow = nameindex = 0;
		while (1) {
			switch (*p) {
				case '\r':
					p++;
					break;
				case '\n':
				case '\0':
					location[nameindex] = 0;
					TP_AddLocNode(coord, location);
					if (*p == '\n')
						p++;
					goto _endofline;
				default:
					if (nameindex < MAX_LOC_NAME - 1) {
						location[nameindex++] = *p;
					}
					else if (!overflow) {
						overflow = 1;
						Com_Printf("Locfile warning (line %d): truncating loc name to %d chars\n", line, MAX_LOC_NAME - 1);
					}
					p++;
			}
		}
	_endofline:
		line++;
	}
_endoffile:

	Q_free(buf);

	if (loc_count) {
		if (!quiet) {
			Com_Printf("Loaded locfile \"%s\" (%i loc points)\n", COM_SkipPath(locname), loc_count); // loc_numentries);
		}
	}
	else {
		TP_ClearLocs();
		if (!quiet) {
			Com_Printf("Locfile \"%s\" was empty\n", COM_SkipPath(locname));
		}
	}

	return true;
}

void TP_LoadLocFile_f(void)
{
	if (Cmd_Argc() != 2) {
		Com_Printf("loadloc <filename> : load a loc file\n");
		return;
	}
	TP_LoadLocFile(Cmd_Argv(1), false);
}

qbool TP_SaveLocFile(char *path, qbool quiet)
{
	locdata_t	*node;
	char		*buf;
	char		locname[MAX_OSPATH];

	// Prevents saving of junk data
	if (!loc_count) {
		Com_Printf("TP_SaveLocFile: There is no locations to save.\n");
		return false;
	}

	// Make sure we have a path to work with.
	if (!*path) {
		return false;
	}

	// Check if the filename is too long.
	if (strlen(path) > MAX_LOC_NAME) {
		Com_Printf("TP_SaveLocFile: Filename too long. Max allowed is %d characters\n", MAX_LOC_NAME);
		return false;
	}

	// Get the default path for loc-files and make sure the path
	// won't be too long.
	strlcpy(locname, "locs/", sizeof(locname));
	if (strlen(path) + strlen(locname) + 2 + 4 > MAX_OSPATH) {
		Com_Printf("TP_SaveLocFile: path name > MAX_OSPATH\n");
		return false;
	}

	// Add an extension if it doesn't exist already.
	strlcat(locname, path, sizeof(locname) - strlen(locname));
	COM_DefaultExtension(locname, ".loc", sizeof(locname));

	// Allocate a buffer to hold the file contents.
	buf = (char *)Q_calloc(loc_count * (MAX_LOC_NAME + 24), sizeof(char));

	if (!buf) {
		Com_Printf("TP_SaveLocFile: Could not initialize buffer.\n");
		return false;
	}

	// Write all the nodes to the buffer.
	//for (node = locdata; node; node = node->next) {
	node = locdata;
	while (node) {
		char row[2 * MAX_LOC_NAME];

		snprintf(row, sizeof(row), "%4d %4d %4d %s\n", Q_rint(8 * node->coord[0]), Q_rint(8 * node->coord[1]), Q_rint(8 * node->coord[2]), node->name);
		strlcat(buf, row, (loc_count * (MAX_LOC_NAME + 24)));
		node = node->next;
	}

	// Try writing the buffer containing the locs to file.
	if (!FS_WriteFile(locname, buf, strlen(buf))) {
		if (!quiet) {
			Com_Printf("TP_SaveLocFile: Could not open %s for writing\n", locname);
		}

		// Make sure we free our buffer.
		Q_free(buf);

		return false;
	}

	// Make sure we free our buffer.
	Q_free(buf);

	return true;
}

void TP_SaveLocFile_f(void)
{
	if (Cmd_Argc() != 2) {
		Com_Printf("saveloc <filename> : save a loc file\n");
		return;
	}
	TP_SaveLocFile(Cmd_Argv(1), false);
}

void TP_AddLoc(char *locname)
{
	vec3_t location;

	// We need to be up and running.
	if (cls.state != ca_active) {
		Com_Printf("Need to be active to add a location.\n");
		return;
	}

	if (cl.players[cl.playernum].spectator && Cam_TrackNum() >= 0) {
		VectorCopy(cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK].playerstate[Cam_TrackNum()].origin, location);
	}
	else {
		VectorCopy(cl.simorg, location);
	}

	TP_AddLocNode(location, locname);

	Com_Printf("Added location \"%s\" at (%4.0f, %4.0f, %4.0f)\n", locname, location[0], location[1], location[2]);
}

static void TP_AddLoc_f(void)
{
	if (Cmd_Argc() != 2) {
		Com_Printf("addloc <name of location> : add a location\n");
		return;
	}
	TP_AddLoc(Cmd_Argv(1));
}

static void TP_RemoveClosestLoc(vec3_t location)
{
	float dist, mindist;
	vec3_t vec;
	locdata_t *node, *best, *previous, *best_previous;

	best_previous = previous = best = NULL;
	mindist = 0;

	// Find the closest loc.
	node = locdata;
	while (node) {
		// Get the distance to the loc.
		VectorSubtract(location, node->coord, vec);
		dist = vec[0] * vec[0] + vec[1] * vec[1] + vec[2] * vec[2];

		// Check if it's closer than the previously best.
		if (!best || dist < mindist) {
			best_previous = previous;
			best = node;
			mindist = dist;
		}

		// Advance and save the previous node.
		previous = node;
		node = node->next;
	}

	if (!best) {
		Com_Printf("There is no locations left for deletion!\n");
		return;
	}


	Com_Printf("Removed location \"%s\" at (%4.0f, %4.0f, %4.0f)\n", best->name, best->coord[0], best->coord[1], best->coord[2]);

	// If the node we're trying to delete has a
	// next node attached to it, copy the data from
	// that node into the node we're deleting, and then
	// delete the next node instead.
	if (best->next) {
		locdata_t *temp;

		// Copy the data from the next node into the one we're deleting.
		VectorCopy(best->next->coord, best->coord);

		Q_free(best->name);
		best->name = (char *)Q_calloc(strlen(best->next->name) + 1, sizeof(char));
		strcpy(best->name, best->next->name);

		// Save the pointer to the next node.
		temp = best->next->next;

		// Free the current next node.
		Q_free(best->next->name);
		Q_free(best->next);

		// Set the pointer to the next node.
		best->next = temp;
	}
	else {
		// Free the current node.
		Q_free(best->name);
		Q_free(best);
		best = NULL;

		// Make sure the previous node doesn't point to garbage.
		if (best_previous != NULL) {
			best_previous->next = NULL;
		}
	}

	// Decrease the loc count.
	loc_count--;

	// If this was the last loc, remove the entire node list.
	if (loc_count <= 0) {
		locdata = NULL;
	}
}

static void TP_RemoveLoc_f(void)
{
	if (Cmd_Argc() == 1) {
		if (cl.players[cl.playernum].spectator && Cam_TrackNum() >= 0) {
			TP_RemoveClosestLoc(cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK].playerstate[Cam_TrackNum()].origin);
		}
		else {
			TP_RemoveClosestLoc(cl.simorg);
		}
	}
	else {
		Com_Printf("removeloc : remove the closest location\n");
		return;
	}
}

typedef struct locmacro_s {
	char *macro;
	char *val;
} locmacro_t;

static locmacro_t locmacros[] = {
	{ "ssg", "ssg" },
	{ "ng", "ng" },
	{ "sng", "sng" },
	{ "gl", "gl" },
	{ "rl", "rl" },
	{ "lg", "lg" },
	{ "separator", "-" },
	{ "ga", "ga" },
	{ "ya", "ya" },
	{ "ra", "ra" },
	{ "quad", "quad" },
	{ "pent", "pent" },
	{ "ring", "ring" },
	{ "suit", "suit" },
	{ "mh", "mega" },
};

#define NUM_LOCMACROS	(sizeof(locmacros) / sizeof(locmacros[0]))

char *TP_LocationName(vec3_t location)
{
	char *in, *out, *value;
	int i;
	float dist, mindist;
	vec3_t vec;
	static locdata_t *node, *best;
	cvar_t *cvar;
	extern cvar_t tp_name_someplace;
	static qbool recursive;
	static char	buf[1024], newbuf[MAX_LOC_NAME];

	if (!locdata || cls.state != ca_active) {
		return tp_name_someplace.string;
	}

	if (recursive) {
		return "";
	}

	best = NULL;
	mindist = 0;

	for (node = locdata; node; node = node->next) {
		VectorSubtract(location, node->coord, vec);
		dist = vec[0] * vec[0] + vec[1] * vec[1] + vec[2] * vec[2];
		if (!best || dist < mindist) {
			best = node;
			mindist = dist;
		}
	}

	newbuf[0] = 0;
	out = newbuf;
	in = best->name;
	while (*in && out - newbuf < sizeof(newbuf) - 1) {
		if (!strncasecmp(in, "$loc_name_", 10)) {
			in += 10;
			for (i = 0; i < NUM_LOCMACROS; i++) {
				if (!strncasecmp(in, locmacros[i].macro, strlen(locmacros[i].macro))) {
					if ((cvar = Cvar_Find(va("loc_name_%s", locmacros[i].macro))))
						value = cvar->string;
					else
						value = locmacros[i].val;
					if (out - newbuf >= sizeof(newbuf) - strlen(value) - 1)
						goto done_locmacros;
					strcpy(out, value);
					out += strlen(value);
					in += strlen(locmacros[i].macro);
					break;
				}
			}
			if (i == NUM_LOCMACROS) {
				if (out - newbuf >= sizeof(newbuf) - 10 - 1)
					goto done_locmacros;
				strcpy(out, "$loc_name_");
				out += 10;
			}
		}
		else {
			*out++ = *in++;
		}
	}
done_locmacros:
	*out = 0;

	buf[0] = 0;
	recursive = true;
	Cmd_ExpandString(newbuf, buf);
	recursive = false;

	return buf;
}

void TP_LocFiles_Init(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_COMMUNICATION);
	Cmd_AddCommand("locations_loadfile", TP_LoadLocFile_f);
	Cmd_AddLegacyCommand("loadloc", "locations_loadfile");
	Cmd_AddCommand("locations_savefile", TP_SaveLocFile_f);
	Cmd_AddLegacyCommand("saveloc", "locations_savefile");
	Cmd_AddCommand("locations_add", TP_AddLoc_f);
	Cmd_AddLegacyCommand("addloc", "locations_add");
	Cmd_AddCommand("locations_remove", TP_RemoveLoc_f);
	Cmd_AddLegacyCommand("removeloc", "locations_remove");
	Cmd_AddCommand("locations_clearall", TP_ClearLocs_f);
	Cmd_AddLegacyCommand("clearlocs", "locations_clearall");
	Cvar_ResetCurrentGroup();
}

void TP_LocFiles_NewMap(void)
{
	static char last_map[MAX_QPATH] = { 0 };
	char *groupname;
	qbool system;
	char* mapname = TP_MapName();
	extern cvar_t tp_loadlocs;

	if (strcmp(mapname, last_map)) {	// map name has changed
		TP_ClearLocs();					// clear loc file

		// always load when playing back demos (teaminfo might be enabled etc)
		if ((tp_loadlocs.integer && cl.deathmatch) || cls.demoplayback) {
			if (!TP_LoadLocFile(va("%s.loc", mapname), true)) {
				if ((groupname = TP_GetMapGroupName(mapname, &system)) && !system) {
					TP_LoadLocFile(va("%s.loc", groupname), true);
				}
			}
			strlcpy(last_map, mapname, sizeof(last_map));
		}
		else {
			last_map[0] = 0;
		}
	}
}

void TP_LocFiles_Shutdown(void)
{
	TP_ClearLocs();
}
