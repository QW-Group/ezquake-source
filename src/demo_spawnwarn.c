/*
 * Copyright (C) 2026 Oscar Linderholm <osm@recv.se>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "quakedef.h"
#include "demo_spawnwarn.h"

#define MAX_DEMO_SPAWNWARN_POINTS 128

static const double demo_spawnwarn_ignore_duration = 1.0;
static const vec3_t demo_spawnwarn_touch_mins = { -16, -16, -24 };
static const vec3_t demo_spawnwarn_touch_maxs = { 16, 16, 32 };

typedef struct
{
	char classname[MAX_COM_TOKEN];
	char target[MAX_COM_TOKEN];
	char targetname[MAX_COM_TOKEN];
	vec3_t origin;
	qbool has_origin;
	qbool has_target;
	qbool has_targetname;
} demo_spawnwarn_entity_t;

typedef struct
{
	char targetname[MAX_COM_TOKEN];
	vec3_t origin;
} demo_spawnwarn_teleport_destination_t;

typedef struct
{
	char target[MAX_COM_TOKEN];
} demo_spawnwarn_teleport_trigger_t;

static vec3_t cl_spawnwarn_points[MAX_DEMO_SPAWNWARN_POINTS];
static int cl_num_spawnwarn_points;
static qbool cl_spawnwarn_active;
static double cl_spawnwarn_warning_suppressed_until;
static qbool cl_spawnwarn_point_overflow_warned;

cvar_t demo_spawnwarn = { "demo_spawnwarn", "0" };
cvar_t demo_spawnwarn_text = {
	"demo_spawnwarn_text",
	"you crossed a spawn"
};

extern temp_entity_list_t temp_entities;

static void CL_SpawnWarn_ResetState(void)
{
	memset(cl_spawnwarn_points, 0, sizeof(cl_spawnwarn_points));
	cl_num_spawnwarn_points = 0;
	cl_spawnwarn_active = false;
	cl_spawnwarn_warning_suppressed_until = 0;
	cl_spawnwarn_point_overflow_warned = false;
}

static qbool CL_SpawnWarn_ParseEntityOrigin(const char *value, vec3_t origin)
{
	return sscanf(value, "%f %f %f", &origin[0], &origin[1], &origin[2]) == 3;
}

static void CL_SpawnWarn_WarnOverflow(void)
{
	if (!cl_spawnwarn_point_overflow_warned)
	{
		Con_Printf("demo_spawnwarn: too many points on map, limit is %d\n",
			MAX_DEMO_SPAWNWARN_POINTS);
		cl_spawnwarn_point_overflow_warned = true;
	}
}

static void CL_SpawnWarn_AddPoint(const vec3_t origin)
{
	int point_index;

	if (cl_num_spawnwarn_points >= MAX_DEMO_SPAWNWARN_POINTS)
	{
		CL_SpawnWarn_WarnOverflow();
		return;
	}

	point_index = cl_num_spawnwarn_points++;
	VectorCopy(origin, cl_spawnwarn_points[point_index]);
}

static void CL_SpawnWarn_ClearEntity(demo_spawnwarn_entity_t *entity)
{
	memset(entity, 0, sizeof(*entity));
}

static void CL_SpawnWarn_AddTeleportTrigger(
	const char *target,
	demo_spawnwarn_teleport_trigger_t triggers[],
	int *num_triggers)
{
	int trigger_index;

	if (*num_triggers >= MAX_DEMO_SPAWNWARN_POINTS)
	{
		CL_SpawnWarn_WarnOverflow();
		return;
	}

	trigger_index = *num_triggers;
	strlcpy(triggers[trigger_index].target,
		target,
		sizeof(triggers[trigger_index].target)
	);
	*num_triggers = trigger_index + 1;
}

static void CL_SpawnWarn_AddTeleportDestination(
	const char *targetname,
	const vec3_t origin,
	demo_spawnwarn_teleport_destination_t destinations[],
	int *num_destinations)
{
	int destination_index;

	if (*num_destinations >= MAX_DEMO_SPAWNWARN_POINTS)
	{
		CL_SpawnWarn_WarnOverflow();
		return;
	}

	destination_index = *num_destinations;
	strlcpy(destinations[destination_index].targetname,
		targetname,
		sizeof(destinations[destination_index].targetname));
	VectorCopy(origin, destinations[destination_index].origin);
	*num_destinations = destination_index + 1;
}

static void CL_SpawnWarn_CollectPointData(
	const demo_spawnwarn_entity_t *entity,
	demo_spawnwarn_teleport_trigger_t triggers[],
	int *num_triggers,
	demo_spawnwarn_teleport_destination_t destinations[],
	int *num_destinations)
{
	if (!strcmp(entity->classname, "info_player_deathmatch") && entity->has_origin)
	{
		CL_SpawnWarn_AddPoint(entity->origin);
	}
	else if (!strcmp(entity->classname, "trigger_teleport")
		&& entity->has_target && !entity->has_targetname)
	{
		// Store the trigger target so we can resolve the exit entity
		// later.
		CL_SpawnWarn_AddTeleportTrigger(entity->target, triggers, num_triggers);
	}
	else if (entity->has_targetname && entity->has_origin)
	{
		// Store possible teleport destinations; matching targetname
		// becomes the exit.
		CL_SpawnWarn_AddTeleportDestination(entity->targetname,
			entity->origin,
			destinations,
			num_destinations);
	}
}

static void CL_SpawnWarn_ResolveTeleportExitPoints(
	const demo_spawnwarn_teleport_trigger_t triggers[],
	int num_triggers,
	const demo_spawnwarn_teleport_destination_t destinations[],
	int num_destinations)
{
	int i;

	for (i = 0; i < num_triggers; i++)
	{
		int j;

		for (j = 0; j < num_destinations; j++)
		{
			if (!strcmp(triggers[i].target, destinations[j].targetname))
			{
				CL_SpawnWarn_AddPoint(destinations[j].origin);
				break;
			}
		}
	}
}

static qbool CL_SpawnWarn_ParseEntityBlock(
	const char **data_p,
	demo_spawnwarn_entity_t *entity)
{
	char key[MAX_COM_TOKEN];
	const char *data = *data_p;

	CL_SpawnWarn_ClearEntity(entity);

	while (1)
	{
		data = COM_Parse(data);
		if (!data)
		{
			Con_DPrintf(
				"CL_SpawnWarn_ParseEntityBlock: unexpected end of entity key\n");
			return false;
		}

		if (com_token[0] == '}')
		{
			*data_p = data;
			return true;
		}

		strlcpy(key, com_token, sizeof(key));

		data = COM_Parse(data);
		if (!data)
		{
			Con_DPrintf(
				"CL_SpawnWarn_ParseEntityBlock: unexpected end of entity value\n");
			return false;
		}

		if (!strcmp(key, "classname"))
		{
			strlcpy(entity->classname, com_token, sizeof(entity->classname));
		}
		else if (!strcmp(key, "origin"))
		{
			entity->has_origin = CL_SpawnWarn_ParseEntityOrigin(com_token,
				entity->origin);
		}
		else if (!strcmp(key, "target"))
		{
			entity->has_target = true;
			strlcpy(entity->target, com_token, sizeof(entity->target));
		}
		else if (!strcmp(key, "targetname"))
		{
			entity->has_targetname = true;
			strlcpy(entity->targetname, com_token, sizeof(entity->targetname));
		}
	}
}

static qbool CL_SpawnWarn_ParseEntityLump(const char *entstring)
{
	demo_spawnwarn_teleport_destination_t teleport_destinations[MAX_DEMO_SPAWNWARN_POINTS];
	demo_spawnwarn_teleport_trigger_t teleport_triggers[MAX_DEMO_SPAWNWARN_POINTS];
	const char *data;
	int num_teleport_destinations = 0;
	int num_teleport_triggers = 0;

	if (!entstring || !entstring[0])
	{
		return true;
	}

	data = entstring;
	while ((data = COM_Parse(data)) != NULL)
	{
		demo_spawnwarn_entity_t entity;

		if (com_token[0] != '{')
		{
			Con_DPrintf("CL_SpawnWarn_ParseEntityLump: found %s when expecting {\n",
				com_token);
			CL_SpawnWarn_ResetState();
			return false;
		}

		if (!CL_SpawnWarn_ParseEntityBlock(&data, &entity))
		{
			CL_SpawnWarn_ResetState();
			return false;
		}

		CL_SpawnWarn_CollectPointData(&entity,
			teleport_triggers,
			&num_teleport_triggers,
			teleport_destinations,
			&num_teleport_destinations);
	}

	CL_SpawnWarn_ResolveTeleportExitPoints(teleport_triggers,
		num_teleport_triggers,
		teleport_destinations,
		num_teleport_destinations);

	return true;
}

void CL_SpawnWarn_LoadPoints(void)
{
	CL_SpawnWarn_ResetState();

	if (!cls.demoplayback)
	{
		return;
	}

	CL_SpawnWarn_ParseEntityLump(CM_EntityString());
}

static qbool CL_SpawnWarn_BoxesTouch(
	const vec3_t absmin1,
	const vec3_t absmax1,
	const vec3_t absmin2,
	const vec3_t absmax2)
{
	return !(absmin1[0] > absmax2[0] || absmin1[1] > absmax2[1]
		|| absmin1[2] > absmax2[2]
		|| absmax1[0] < absmin2[0] || absmax1[1] < absmin2[1]
		|| absmax1[2] < absmin2[2]);
}

static void CL_SpawnWarn_CheckTeleportSuppression(double current_time)
{
	int i;

	for (i = 0; i < MAX_TEMP_ENTITIES; i++)
	{
		temp_entity_t *temp = &temp_entities.list[i];
		vec3_t teleport_absmin, teleport_absmax, player_absmin, player_absmax;

		if (temp->type != TE_TELEPORT
			|| current_time - temp->time > demo_spawnwarn_ignore_duration)
		{
			continue;
		}

		VectorAdd(temp->pos, demo_spawnwarn_touch_mins, teleport_absmin);
		VectorAdd(temp->pos, demo_spawnwarn_touch_maxs, teleport_absmax);
		VectorAdd(cl.simorg, demo_spawnwarn_touch_mins, player_absmin);
		VectorAdd(cl.simorg, demo_spawnwarn_touch_maxs, player_absmax);

		if (CL_SpawnWarn_BoxesTouch(teleport_absmin,
			teleport_absmax,
			player_absmin,
			player_absmax))
		{
			CL_SpawnWarn_SuppressAfterTeleport();
		}
	}
}

static void CL_SpawnWarn_SuppressWarning(void)
{
	if (!cls.demoplayback)
	{
		return;
	}

	cl_spawnwarn_warning_suppressed_until = max(
		cl_spawnwarn_warning_suppressed_until,
		cls.demotime + demo_spawnwarn_ignore_duration);
}

void CL_SpawnWarn_ClearPoints(void)
{
	CL_SpawnWarn_ResetState();
}

void CL_SpawnWarn_SuppressAfterRespawn(void)
{
	CL_SpawnWarn_SuppressWarning();
}

void CL_SpawnWarn_SuppressAfterTeleport(void)
{
	CL_SpawnWarn_SuppressWarning();
}

void CL_SpawnWarn_UpdateWarning(void)
{
	qbool any_spawn_triggered = false;
	double current_time = cls.demotime;
	int i;

	if (!cls.demoplayback)
	{
		return;
	}

	if (!demo_spawnwarn.integer)
	{
		return;
	}

	if (cl.stats[STAT_HEALTH] <= 0)
	{
		return;
	}

	CL_SpawnWarn_CheckTeleportSuppression(current_time);

	if (current_time < cl_spawnwarn_warning_suppressed_until)
	{
		cl_spawnwarn_active = false;
		return;
	}

	for (i = 0; i < cl_num_spawnwarn_points; i++)
	{
		vec3_t spawn_absmin, spawn_absmax, player_absmin, player_absmax;

		VectorAdd(cl_spawnwarn_points[i], demo_spawnwarn_touch_mins, spawn_absmin);
		VectorAdd(cl_spawnwarn_points[i], demo_spawnwarn_touch_maxs, spawn_absmax);
		VectorAdd(cl.simorg, demo_spawnwarn_touch_mins, player_absmin);
		VectorAdd(cl.simorg, demo_spawnwarn_touch_maxs, player_absmax);

		if (CL_SpawnWarn_BoxesTouch(spawn_absmin,
			spawn_absmax,
			player_absmin,
			player_absmax))
		{
			any_spawn_triggered = true;
		}
	}

	if (any_spawn_triggered && !cl_spawnwarn_active)
	{
		SCR_CenterPrint(demo_spawnwarn_text.string);
		cl_spawnwarn_active = true;
	}
	else if (!any_spawn_triggered)
	{
		cl_spawnwarn_active = false;
	}
}
