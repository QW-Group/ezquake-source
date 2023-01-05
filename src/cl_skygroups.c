/*

Copyright (C) 2002-2003       A Nourai

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "quakedef.h"
#include "utils.h"

#define FIRSTUSERSKYGROUP (last_system_skygroup ? last_system_skygroup->next : skygroups)
#define MAX_SKYGROUP_MEMBERS	36

typedef struct skygroup_s {
	char groupname[MAX_QPATH];
	char members[MAX_SKYGROUP_MEMBERS][MAX_QPATH];
	struct skygroup_s *next, *prev;
	qbool system;
	int nummembers;
} skygroup_t;

static skygroup_t *skygroups = NULL;
static skygroup_t *last_system_skygroup = NULL;
static qbool skygroups_init = false;

static skygroup_t *GetSkyGroupWithName(char *groupname)
{
	skygroup_t *node;

	for (node = skygroups; node; node = node->next) {
		if (!strcasecmp(node->groupname, groupname))
			return node;
	}
	return NULL;
}

static skygroup_t *GetSkyGroupWithMember(char *member)
{
	int j;
	skygroup_t *node;

	for (node = skygroups; node; node = node->next) {
		for (j = 0; j < node->nummembers; j++) {
			if (!strcasecmp(node->members[j], member))
				return node;
		}
	}
	return NULL;
}

static void DeleteSkyGroup(skygroup_t *group)
{
	if (!group)
		return;

	if (group->prev)
		group->prev->next = group->next;
	if (group->next)
		group->next->prev = group->prev;


	if (group == skygroups) {
		skygroups = skygroups->next;
		if (group == last_system_skygroup)
			last_system_skygroup = NULL;
	}
	else if (group == last_system_skygroup) {
		last_system_skygroup = last_system_skygroup->prev;
	}

	Q_free(group);
}

static void ResetSkyGroupMembers(skygroup_t *group)
{
	int i;

	if (!group)
		return;

	for (i = 0; i < group->nummembers; i++)
		group->members[i][0] = 0;

	group->nummembers = 0;
}

static void DeleteSkyGroupMember(skygroup_t *group, char *member)
{
	int i;

	if (!group)
		return;

	for (i = 0; i < group->nummembers; i++) {
		if (!strcasecmp(member, group->members[i]))
			break;
	}

	if (i == group->nummembers)
		return;

	if (i < group->nummembers - 1)
		memmove(group->members[i], group->members[i + 1], (group->nummembers - 1 - i) * sizeof(group->members[0]));

	group->nummembers--;
}

static void AddSkyGroupMember(skygroup_t *group, char *member)
{
	int i;

	if (!group || group->nummembers == MAX_SKYGROUP_MEMBERS)
		return;

	for (i = 0; i < group->nummembers; i++) {
		if (!strcasecmp(member, group->members[i]))
			return;
	}

	strlcpy(group->members[group->nummembers], member, sizeof(group->members[group->nummembers]));
	group->nummembers++;
}

void MT_SkyGroup_f(void)
{
	int i, c, j;
	qbool removeflag = false;
	skygroup_t *node, *group, *tempnode;
	extern cvar_t r_skyname;
	char *groupname, *member;

	extern int R_SetSky(char *skyname);

	if ((c = Cmd_Argc()) == 1) {
		if (!FIRSTUSERSKYGROUP) {
			Com_Printf("No sky groups defined\n");
		}
		else {
			for (node = FIRSTUSERSKYGROUP; node; node = node->next) {
				Com_Printf("\x02%s: ", node->groupname);
				for (j = 0; j < node->nummembers; j++)
					Com_Printf("%s ", node->members[j]);
				Com_Printf("\n");
			}
		}
		return;
	}

	groupname = Cmd_Argv(1);

	if (c == 2 && !strcasecmp(groupname, "clear")) {
		for (node = FIRSTUSERSKYGROUP; node; node = tempnode) {
			tempnode = node->next;
			DeleteSkyGroup(node);
		}
		return;
	}

	if (Util_Is_Valid_Filename(groupname) == false) {
		Com_Printf("Error: %s is not a valid sky group name\n", groupname);
		return;
	}

	group = GetSkyGroupWithName(groupname);

	if (c == 2) {
		if (!group) {
			Com_Printf("No sky group named \"%s\"\n", groupname);
		}
		else {
			Com_Printf("\x02%s: ", groupname);
			for (j = 0; j < group->nummembers; j++)
				Com_Printf("%s ", group->members[j]);
			Com_Printf("\n");
		}
		return;
	}

	if (group && group->system) {
		Com_Printf("Cannot modify system group \"%s\"\n", groupname);
		return;
	}

	if (c == 3 && !strcasecmp(Cmd_Argv(2), "clear")) {
		if (!group && !strcmp("exmx", groupname))
			Com_Printf("\"%s\" is not a sky group name\n", groupname);
		else
			DeleteSkyGroup(group);
		return;
	}



	if (!group) {
		group = (skygroup_t *)Q_calloc(1, sizeof(skygroup_t));
		strlcpy(group->groupname, groupname, sizeof(group->groupname));
		group->system = !skygroups_init;
		if (skygroups) {
			for (tempnode = skygroups; tempnode->next; tempnode = tempnode->next)
				;
			tempnode->next = group;
			group->prev = tempnode;
		}
		else {
			skygroups = group;
		}
	}
	else {
		member = Cmd_Argv(2);
		if (member[0] != '+' && member[0] != '-')
			ResetSkyGroupMembers(group);
	}

	for (i = 2; i < c; i++) {
		member = Cmd_Argv(i);
		if (member[0] == '+') {
			removeflag = false;
			member++;
		}
		else if (member[0] == '-') {
			removeflag = true;
			member++;
		}

		if (!removeflag && (tempnode = GetSkyGroupWithMember(member)) && tempnode != group) {
			if (cl_warncmd.integer || developer.integer)
				Com_Printf("Warning: \"%s\" is already a member of group \"%s\"...ignoring\n", member, tempnode->groupname);
			continue;
		}

		if (removeflag)
			DeleteSkyGroupMember(group, member);
		else
			AddSkyGroupMember(group, member);
	}

	if (!group->nummembers) {
		DeleteSkyGroup(group);
	}

	R_SetSky(r_skyname.string);
}

void MT_AddSkyGroups(void)
{
	char clear[256] = { 0 };

	strlcat(clear, "skygroup exmy clear", sizeof(clear));
	Cmd_TokenizeString(clear);
	MT_SkyGroup_f();
	skygroups_init = true;
}

char *MT_GetSkyGroupName(char *mapname, qbool *system)
{
	skygroup_t *group;

	group = GetSkyGroupWithMember(mapname);
	if (group && strcmp(mapname, group->groupname)) {
		if (system)
			*system = group->system;
		return group->groupname;
	}
	else {
		return NULL;
	}
}

void DumpSkyGroups(FILE *f)
{
	skygroup_t *node;
	int j;
	if (!FIRSTUSERSKYGROUP) {
		fprintf(f, "skygroup clear\n");
		return;
	}
	for (node = FIRSTUSERSKYGROUP; node; node = node->next) {
		fprintf(f, "skygroup %s ", node->groupname);
		for (j = 0; j < node->nummembers; j++)
			fprintf(f, "%s ", node->members[j]);
		fprintf(f, "\n");
	}
}
