/*
Copyright (C) 2011 Cokeman

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "stats_grid.h"
#include "sbar.h"
#include "hud.h"
#include "utils.h"
#include "fonts.h"

static hud_t* teamholdinfo;
static hud_t* teamholdbar;

stats_weight_grid_t *stats_grid = NULL;
stats_entities_t *stats_important_ents = NULL;

void StatsGrid_Remove(stats_weight_grid_t **grid)
{
	int row;

	// Nothing to remove.
	if((*grid) == NULL)
	{
		return;
	}

	// Free all the rows.
	if((*grid)->cells != NULL)
	{
		for(row = 0; row < (*grid)->row_count; row++)
		{
			// Make sure it hasn't already been freed.
			if((*grid)->cells[row] != NULL)
			{
				Q_free((*grid)->cells[row]);
			}
		}
	}

	// Free all columns.
	Q_free((*grid)->cells);

	// Free the grid itself.
	Q_free((*grid));
}


void StatsGrid_Init(stats_weight_grid_t **grid,
						 float falloff_interval,
						 float falloff_value,
						 int cell_length,
						 float grid_width,
						 float grid_height,
						 float hold_threshold)
{
	int row;
	int col;

	// Allocate the grid.
	(*grid) = (stats_weight_grid_t *)Q_malloc(sizeof(stats_weight_grid_t));
	memset((*grid), 0, sizeof(stats_weight_grid_t));

	if((*grid) == NULL)
	{
		// Failure.
		return;
	}

	// Get the row and col count.
	(*grid)->row_count = Q_rint(grid_height / cell_length);
	(*grid)->col_count = Q_rint(grid_width / cell_length);

	// Allocate the rows.
	(*grid)->cells = (stats_cell_t **)Q_calloc((*grid)->row_count, sizeof(stats_cell_t *));

	// If we failed allocating the rows, cleanup and return.
	if((*grid)->cells == NULL)
	{
		Q_free(*grid);
		return;
	}

	// Allocate the columns for each row.
	for(row = 0; row < (*grid)->row_count; row++)
	{
		// Allocate memory for the current rows columns.
		(*grid)->cells[row] = (stats_cell_t *)Q_calloc((*grid)->col_count, sizeof(stats_cell_t));

		// If something went wrong cleanup and return.
		if((*grid)->cells[row] == NULL)
		{
			// Make sure we're not trying to free more than we allocated.
			(*grid)->row_count = row;

			// Remove the grid.
			StatsGrid_Remove(grid);

			// Failure.
			return;
		}

		// Set initial values for all the cells.
		for(col = 0; col < (*grid)->col_count; col++)
		{
			(*grid)->cells[row][col].teams[STATS_TEAM1].weight = 0.0;
			(*grid)->cells[row][col].teams[STATS_TEAM1].change_time = 0.0;
			(*grid)->cells[row][col].teams[STATS_TEAM1].death_weight = 0.0;

			(*grid)->cells[row][col].teams[STATS_TEAM2].weight = 0.0;
			(*grid)->cells[row][col].teams[STATS_TEAM2].change_time = 0.0;
			(*grid)->cells[row][col].teams[STATS_TEAM2].death_weight = 0.0;

			// Save the quake coordinates of the cells upper left corner.
			(*grid)->cells[row][col].tl_x = cl.worldmodel->mins[0] + (col * cell_length);
			(*grid)->cells[row][col].tl_y = cl.worldmodel->mins[1] + (row * cell_length);
		}
	}

	// If everything went well set the rest of the stuff.
	(*grid)->falloff_interval	= falloff_interval;
	(*grid)->falloff_value		= falloff_value;
	(*grid)->cell_length		= cell_length;
	(*grid)->width				= grid_width;
	(*grid)->height				= grid_height;

	// We will wait with setting these until the match has started.

	(*grid)->teams[STATS_TEAM1].color			= 0;
	(*grid)->teams[STATS_TEAM2].color			= 0;
	(*grid)->teams[STATS_TEAM1].hold_count		= 0;
	(*grid)->teams[STATS_TEAM2].hold_count		= 0;
	(*grid)->teams[STATS_TEAM1].color			= 0;
	(*grid)->teams[STATS_TEAM2].color			= 0;
	(*grid)->hold_threshold						= hold_threshold;
}

#define STATS_MAX_IMPORTANT_ENTS	16

void StatsGrid_ResetHoldItemsOrder(void)
{
	int i;

	if (stats_important_ents == NULL)
	{
		return;
	}

	for (i = 0; i < stats_important_ents->count; i++)
	{
		stats_important_ents->list[i].order = STATS_MAX_IMPORTANT_ENTS;
	}
}

void StatsGrid_SetHoldItemOrder(const char *item_name, int order)
{
	int i;
	int len = strlen(item_name);

	if (stats_important_ents == NULL)
	{
		return;
	}

	for (i = 0; i < stats_important_ents->count; i++)
	{
		if (!strncasecmp(stats_important_ents->list[i].name, item_name, len))
		{
			stats_important_ents->list[i].order = order;
		}
	}
}

void StatsGrid_SetHoldItemName(char *dst_name, const char *src_name, int count)
{
	// If there are more than one of this item already then name it "ITEM#"
	// RL, RL2, RL3 and so on.
	if(count > 1)
	{
		strcpy(dst_name, va("%s%d", src_name, count));
	}
	else
	{
		strcpy(dst_name, src_name);
	}
}

void StatsGrid_InitHoldItems(void)
{
	int i;
	int ents_count = 0;

	// This is used to keep count of how many of the different
	// types of items that exist on the map, so that they
	// can be named "RL" "RL2" and so on.
	int pent_count	= 0;
	int quad_count	= 0;
	int ring_count	= 0;
	int suit_count	= 0;
	int rl_count	= 0;
	int gl_count	= 0;
	int lg_count	= 0;
	int sng_count	= 0;
	int mega_count	= 0;
	int ra_count	= 0;
	int ya_count	= 0;
	int ga_count	= 0;

	// Buffer.
	stats_entity_t temp_ents[STATS_MAX_IMPORTANT_ENTS];

	// Entities (weapons and such). cl_main.c
	extern visentlist_t cl_visents;

	// Don't create the list before we have any entities to work with.
	if(cl_visents.count <= 0)
	{
		return;
	}

	stats_important_ents = (stats_entities_t *)Q_malloc(sizeof(stats_entities_t));

	// Something bad happened.
	if(stats_important_ents == NULL)
	{
		return;
	}

	// Go through the entities and check for the important ones
	// and save their name and location.
	for (i = 0; i < cl_visents.count && (ents_count < STATS_MAX_IMPORTANT_ENTS); i++)
	{
		if(cl_visents.list[i].ent.model->modhint == MOD_PENT)
		{
			StatsGrid_SetHoldItemName(temp_ents[ents_count].name, "PENT", ++pent_count);
		}
		else if(cl_visents.list[i].ent.model->modhint == MOD_QUAD)
		{
			StatsGrid_SetHoldItemName(temp_ents[ents_count].name, "QUAD", ++quad_count);
		}
		else if(cl_visents.list[i].ent.model->modhint == MOD_RING)
		{
			StatsGrid_SetHoldItemName(temp_ents[ents_count].name, "RING", ++ring_count);
		}
		else if(cl_visents.list[i].ent.model->modhint == MOD_SUIT)
		{
			StatsGrid_SetHoldItemName(temp_ents[ents_count].name, "SUIT", ++suit_count);
		}
		else if(cl_visents.list[i].ent.model->modhint == MOD_ROCKETLAUNCHER)
		{
			StatsGrid_SetHoldItemName(temp_ents[ents_count].name, "RL", ++rl_count);
		}
		else if(cl_visents.list[i].ent.model->modhint == MOD_LIGHTNINGGUN)
		{
			StatsGrid_SetHoldItemName(temp_ents[ents_count].name, "LG", ++lg_count);
		}
		else if(cl_visents.list[i].ent.model->modhint == MOD_GRENADELAUNCHER)
		{
			StatsGrid_SetHoldItemName(temp_ents[ents_count].name, "GL", ++gl_count);
		}
		else if(!strcmp(cl_visents.list[i].ent.model->name, "progs/g_nail2.mdl"))
		{
			StatsGrid_SetHoldItemName(temp_ents[ents_count].name, "SNG", ++sng_count);
		}
		else if (cl_visents.list[i].ent.model->modhint == MOD_MEGAHEALTH)
		{
			// Megahealth.
			StatsGrid_SetHoldItemName(temp_ents[ents_count].name, "MH", ++mega_count);
		}
		else if (cl_visents.list[i].ent.model->modhint == MOD_ARMOR)
		{
			if(cl_visents.list[i].ent.skinnum == 0)
			{
				StatsGrid_SetHoldItemName(temp_ents[ents_count].name, "GA", ++ga_count);
			}
			else if(cl_visents.list[i].ent.skinnum == 1)
			{
				StatsGrid_SetHoldItemName(temp_ents[ents_count].name, "YA", ++ya_count);
			}
			else if(cl_visents.list[i].ent.skinnum == 2)
			{
				StatsGrid_SetHoldItemName(temp_ents[ents_count].name, "RA", ++ra_count);
			}
			else
			{
				continue;
			}
		}
		else
		{
			// The entity wasn't one we wanted.
			continue;
		}

		// Copy the position of the entity into the buffer.
		VectorCopy(cl_visents.list[i].ent.origin, temp_ents[ents_count].origin);

		// Reset the team values.
		temp_ents[ents_count].teams_hold_count[STATS_TEAM1] = 0;
		temp_ents[ents_count].teams_hold_count[STATS_TEAM2] = 0;

		ents_count++;
	}

	// Set the count of found entities and allocate memory for the
	// final list of items.
	stats_important_ents->count = ents_count;
	stats_important_ents->list = Q_calloc(ents_count, sizeof(stats_entity_t));

	// Something bad happened, cleanup.
	if(stats_important_ents->list == NULL)
	{
		Q_free(stats_important_ents);
		return;
	}

	// Copy the entities from the buffer to the final list.
	for(i = 0; i < ents_count; i++)
	{
		memcpy(&stats_important_ents->list[i], &temp_ents[i], sizeof(stats_entity_t));
	}

	// Set the radius around the items that decides if it's being
	// held by a team or not.
	stats_important_ents->hold_radius = 264.0;

	// Get the entity with the longest name to use for padding
	// (so we don't have to calculate this more than once).
	stats_important_ents->longest_name = 0;
	for(i = 0; i < stats_important_ents->count; i++)
	{
		int current = strlen(stats_important_ents->list[i].name);
		stats_important_ents->longest_name = max(current, stats_important_ents->longest_name);
	}
}

void StatsGrid_InitTeamNames(stats_weight_grid_t *grid)
{
	int i;

	// Go through the rest of the players until another team is found, that must be team2.
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		// Skip spectators.
		if(!cl.players[i].name[0] || cl.players[i].spectator)
		{
			continue;
		}

		// Get the first player's team and set that as team1.
		if(!grid->teams[STATS_TEAM1].name[0])
		{
			strlcpy(grid->teams[STATS_TEAM1].name, cl.players[i].team, MAX_INFO_STRING);
			grid->teams[STATS_TEAM1].color = Sbar_BottomColor(&cl.players[i]);
		}

		// If this players team isn't the same as the first players team
		// set this players team as team 2.
		if(strcmp(grid->teams[STATS_TEAM1].name, cl.players[i].team))
		{
			strlcpy(grid->teams[STATS_TEAM2].name, cl.players[i].team, MAX_INFO_STRING);
			grid->teams[STATS_TEAM2].color = Sbar_BottomColor(&cl.players[i]);
			break;
		}
	}
}

void StatsGrid_ValidateTeamColors(void)
{
	// This is needed to handle when a user switches the player being tracked
	// and has "teamcolor" and "enemycolor" set. When you switch to a player
	// that has another team than the one you tracked at match start, the team
	// colors saved on the stats grid will not match the team color of the actual
	// players on the level.

	int player_color;
	player_info_t *player_info;
	int tracked = Cam_TrackNum();

	if (tracked < 0)
	{
		return;
	}

	// Get the tracked player.
	player_info = &cl.players[tracked];

	// Get the team color of the tracked player.
	player_color = Sbar_BottomColor(player_info);

	if (!stats_important_ents || !stats_grid)
	{
		return;
	}

	// If the player being tracked is a member of team 1 for instance but has a
	// team color that doesn't match the one saved for team 1 in the
	// stats grid, then swap the team colors in the stats grid.
	if (!strncmp(stats_grid->teams[STATS_TEAM1].name, player_info->team, sizeof(stats_grid->teams[STATS_TEAM1].name))
		&& stats_grid->teams[STATS_TEAM1].color != player_color)
	{
		int old_team1_color = stats_grid->teams[STATS_TEAM1].color;
		stats_grid->teams[STATS_TEAM1].color = stats_important_ents->teams[STATS_TEAM1].color = player_color;
		stats_grid->teams[STATS_TEAM2].color = stats_important_ents->teams[STATS_TEAM2].color = old_team1_color;
	}
	else if (!strncmp(stats_grid->teams[STATS_TEAM2].name, player_info->team, sizeof(stats_grid->teams[STATS_TEAM2].name))
		&& stats_grid->teams[STATS_TEAM2].color != player_color)
	{
		int old_team2_color = stats_grid->teams[STATS_TEAM2].color;
		stats_grid->teams[STATS_TEAM2].color = stats_important_ents->teams[STATS_TEAM2].color = player_color;
		stats_grid->teams[STATS_TEAM1].color = stats_important_ents->teams[STATS_TEAM1].color = old_team2_color;
	}
}

void StatsGrid_Change(stats_weight_grid_t *grid,
							float falloff_interval,
							float falloff_value,
							int grid_width,
							int grid_height)
{
	int row;
	int col;
	int cell_length;

	// Don't do something stupid.
	if(grid == NULL)
	{
		return;
	}

	// Calculate the new cell length.
	cell_length = grid_height / grid->row_count;

	for(row = 0; row < grid->row_count; row++)
	{
		// Reset the location of the cells.
		for(col = 0; col < grid->col_count; col++)
		{
			grid->cells[row][col].tl_x = col * cell_length;
			grid->cells[row][col].tl_y = row * cell_length;
		}
	}

	grid->falloff_interval	= falloff_interval;
	grid->falloff_value		= falloff_value;
}

void StatsGrid_DecreaseWeight(cell_weight_t *weight, stats_weight_grid_t *grid)
{
	// Decrease the weight of the specified cell.
	if(weight->weight > 0 && (cls.demotime - weight->change_time >= grid->falloff_interval))
	{
		weight->change_time = cls.demotime;
		weight->weight -= grid->falloff_value;
	}
}

void StatsGrid_ResetHoldItems(void)
{
	// Nothing to reset.
	if(stats_important_ents == NULL)
	{
		return;
	}

	// Free any entities in the list.
	if(stats_important_ents->list != NULL)
	{
		Q_free(stats_important_ents->list);
	}

	// Reset.
	Q_free(stats_important_ents);
}

void StatsGrid_ResetHoldItemCounts(void)
{
	int i = 0;

	if(stats_important_ents == NULL)
	{
		return;
	}

	for(i = 0; i < stats_important_ents->count; i++)
	{
		stats_important_ents->list[i].teams_hold_count[STATS_TEAM1] = 0;
		stats_important_ents->list[i].teams_hold_count[STATS_TEAM2] = 0;
	}
}

static int StatsGrid_CompareHoldItems(const void *it1, const void *it2)
{
	const stats_entity_t *ent1 = (stats_entity_t *)it1;
	const stats_entity_t *ent2 = (stats_entity_t *)it2;

	return ent1->order - ent2->order;
}

void StatsGrid_SortHoldItems(void)
{
	if (stats_important_ents == NULL)
	{
		return;
	}

	qsort(stats_important_ents->list, stats_important_ents->count, sizeof(stats_entity_t), StatsGrid_CompareHoldItems);
}

void StatsGrid_CalculateHoldItem(stats_weight_grid_t *grid, int row, int col, float hold_threshold, int team_id)
{
	int i = 0;

	if(stats_important_ents == NULL || stats_important_ents->list == NULL)
	{
		return;
	}

	for(i = 0; i < stats_important_ents->count; i++)
	{
		// Check if the cell is within the "hold radius" for this item
		// if it is, increase the hold count for this team for this item.
		// The team with the most "owned" cells within this radius around
		// the item is considered to hold it.
		if(fabs(grid->cells[row][col].tl_x - stats_important_ents->list[i].origin[0]) <= stats_important_ents->hold_radius
			&& fabs(grid->cells[row][col].tl_y - stats_important_ents->list[i].origin[1]) <= stats_important_ents->hold_radius)
		{
			stats_important_ents->list[i].teams_hold_count[team_id]++;

			// If this is the first time, set the name of the team in the entity struct.
			if(!stats_important_ents->teams[team_id].name[0])
			{
				strlcpy(stats_important_ents->teams[team_id].name, grid->teams[team_id].name, MAX_INFO_STRING);
				stats_important_ents->teams[team_id].color = grid->teams[team_id].color;
			}
		}
	}
}

void StatsGrid_DecreaseTeamWeights(stats_weight_grid_t *grid, int row, int col, float hold_threshold)
{
	// Don't try to do something stupid.
	if(grid == NULL || row < 0 || col < 0)
	{
		return;
	}

	// No point in doing anything if the weights are all zero.
	if(grid->cells[row][col].teams[STATS_TEAM1].weight + grid->cells[row][col].teams[STATS_TEAM2].weight <= 0)
	{
		return;
	}

	// Increase the amount of cells that is being "hold" by the teams.
	if(grid->cells[row][col].teams[STATS_TEAM1].weight > grid->cells[row][col].teams[STATS_TEAM2].weight)
	{
		if(grid->cells[row][col].teams[STATS_TEAM1].weight > hold_threshold)
		{
			grid->teams[STATS_TEAM1].hold_count++;
			StatsGrid_CalculateHoldItem(grid, row, col, hold_threshold, STATS_TEAM1);
		}
	}
	else
	{
		if(grid->cells[row][col].teams[STATS_TEAM2].weight > hold_threshold)
		{
			grid->teams[STATS_TEAM2].hold_count++;
			StatsGrid_CalculateHoldItem(grid, row, col, hold_threshold, STATS_TEAM2);
		}
	}

	// Decrease the weights for both teams.
	StatsGrid_DecreaseWeight(&grid->cells[row][col].teams[STATS_TEAM1], grid);
	StatsGrid_DecreaseWeight(&grid->cells[row][col].teams[STATS_TEAM2], grid);
}

void StatsGrid_SetWeightForPlayer(stats_weight_grid_t *grid,
								  player_state_t *player_state,
								  player_info_t *player_info)
{
	float weight = 0.0;
	int row, col;
	int team_id = 0;

	int row_top;
	int row_bottom;
	int col_left;
	int col_right;

	// FIXME : Bad hack!
	// HACK: This is so that I can keep death status of the players
	// and since all I have to differentiate them is the userid,
	// I have to have enough room to fit the players. Most of this
	// space is unused. Too lazy to do something more fancy atm :S
	#define DEAD_SIZE 1024
	static qbool isdead[DEAD_SIZE];

	// Don't calculate any weights before a match has started.
	// Don't allow setting the weight at the exact time the countdown
	// or standby ends (cl.gametime == 0), that will result in a weight
	// being set in the position that the player was the milisecond before
	// he spawned, so wait one second before starting to set any weights.
	if(cl.countdown				|| cl.standby			|| cl.gametime <= 1
		|| grid == NULL			|| grid->cells == NULL
		|| grid->col_count <= 0 || grid->row_count <= 0
		|| player_info == NULL	|| player_state == NULL )
	{
		return;
	}

	// Set the team names.
	if(!grid->teams[STATS_TEAM1].name[0] || !grid->teams[STATS_TEAM2].name[0])
	{
		StatsGrid_InitTeamNames(grid);
	}

	// TODO: Make this properly (this is just a quick hack atm). Use autotrack values?
	if(player_info->stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER)
	{
		weight += 0.5;
	}
	else if(player_info->stats[STAT_ITEMS] & IT_LIGHTNING)
	{
		weight += 0.4;
	}
	else if(player_info->stats[STAT_ITEMS] & IT_GRENADE_LAUNCHER)
	{
		weight += 0.3;
	}
	else if(player_info->stats[STAT_ITEMS] & IT_SUPER_NAILGUN)
	{
		weight += 0.2;
	}
	else if(player_info->stats[STAT_ITEMS] & IT_SUPER_SHOTGUN)
	{
		weight += 0.2;
	}
	else if(player_info->stats[STAT_ITEMS] & IT_NAILGUN)
	{
		weight += 0.05;
	}

	weight += player_info->stats[STAT_HEALTH] / 1000.0;

	if(player_info->stats[STAT_ITEMS] & IT_ARMOR3)
	{
		weight += (player_info->stats[STAT_ARMOR] * (200.0/500.0)) / 1000.0;
	}
	else if(player_info->stats[STAT_ITEMS] & IT_ARMOR2)
	{
		weight += (player_info->stats[STAT_ARMOR] * (150.0/500.0)) / 1000.0;
	}
	else if(player_info->stats[STAT_ITEMS] & IT_ARMOR1)
	{
		weight += (player_info->stats[STAT_ARMOR] * (100.0/500.0)) / 1000.0;
	}

	//
	// Get the grid cell that the player is located in based on it's quake coordinates.
	//
	row = fabs(cl.worldmodel->mins[1] - player_state->origin[1]) / grid->cell_length;
	col = fabs(cl.worldmodel->mins[0] - player_state->origin[0]) / grid->cell_length;

	// Make sure we're not out of bounds.
	row = min(row, grid->row_count - 1);
	col = min(col, grid->col_count - 1);

	//
	// Get the neighbours
	//
	{
		// The row above (make sure it's >= 0).
		row_top = row - 1;
		row_top = max(0, row_top);

		// Row below.
		row_bottom = row + 1;
		row_bottom = min(grid->row_count - 1, row_bottom);

		// Column to the left.
		col_left = col - 1;
		col_left = max(0, col_left);

		// Column to the right.
		col_right = col + 1;
		col_right = min(grid->col_count - 1, col_right);
	}

	// Get the team.
	team_id = !strcmp(player_info->team, grid->teams[STATS_TEAM1].name) ? STATS_TEAM1 : STATS_TEAM2;

	// Raise the death weight for this cell if the player is dead.
	if((player_info->stats[STAT_HEALTH] > 0) && (player_info->userid < DEAD_SIZE) && isdead[player_info->userid])
	{
		isdead[player_info->userid] = false;
	}

	if(player_info->stats[STAT_HEALTH] <= 0 && (player_info->userid < DEAD_SIZE) && !isdead[player_info->userid])
	{
		#define DEATH_WEIGHT		0.2
		#define DEATH_WEIGHT_CLOSE	DEATH_WEIGHT * 0.8
		#define DEATH_WEIGHT_FAR	DEATH_WEIGHT * 0.5

		isdead[player_info->userid] = true;
		grid->cells[row][col].teams[team_id].death_weight += DEATH_WEIGHT;

		// Top.
		grid->cells[row_top][col].teams[team_id].death_weight += DEATH_WEIGHT_CLOSE;

		// Top Right.
		grid->cells[row_top][col_right].teams[team_id].death_weight = DEATH_WEIGHT_FAR;

		// Right.
		grid->cells[row][col_right].teams[team_id].death_weight = DEATH_WEIGHT_CLOSE;

		// Bottom Right.
		grid->cells[row_bottom][col_right].teams[team_id].death_weight = DEATH_WEIGHT_FAR;

		// Bottom.
		grid->cells[row_bottom][col].teams[team_id].death_weight = DEATH_WEIGHT_CLOSE;

		// Bottom Left.
		grid->cells[row_bottom][col_left].teams[team_id].death_weight = DEATH_WEIGHT_FAR;

		// Left.
		grid->cells[row][col_left].teams[team_id].death_weight = DEATH_WEIGHT_CLOSE;

		// Top Left.
		grid->cells[row_top][col_left].teams[team_id].death_weight = DEATH_WEIGHT_FAR;
	}

	// Only change the weight if the new weight is greater than the current one.
	if(weight >= grid->cells[row][col].teams[team_id].weight)
	{
		float neighbour_weight_close;
		float neighbour_weight_far;

		// Fade the weights for the neighbours. The edge corners
		// are at 50% opacity, and the ones at top/bottom, left/right at 80%.
		neighbour_weight_close	= weight * 0.8;
		neighbour_weight_far	= weight * 0.5;

		// Set the team weight for the current cell + surrounding cells.
		{
			// The current cell.
			grid->cells[row][col].teams[team_id].weight = weight;
			grid->cells[row][col].teams[team_id].change_time = cls.demotime;

			// Top.
			grid->cells[row_top][col].teams[team_id].weight = max(grid->cells[row_top][col].teams[team_id].weight, neighbour_weight_close);
			grid->cells[row_top][col].teams[team_id].change_time = cls.demotime;

			// Top right.
			grid->cells[row_top][col_right].teams[team_id].weight = max(grid->cells[row_top][col_right].teams[team_id].weight, neighbour_weight_far);
			grid->cells[row_top][col_right].teams[team_id].change_time = cls.demotime;

			// Right.
			grid->cells[row][col_right].teams[team_id].weight = max(grid->cells[row][col_right].teams[team_id].weight, neighbour_weight_close);
			grid->cells[row][col_right].teams[team_id].change_time = cls.demotime;

			// Bottom right.
			grid->cells[row_bottom][col_right].teams[team_id].weight = max(grid->cells[row_bottom][col_right].teams[team_id].weight, neighbour_weight_far);
			grid->cells[row_bottom][col_right].teams[team_id].change_time = cls.demotime;

			// Bottom.
			grid->cells[row_bottom][col].teams[team_id].weight = max(grid->cells[row_bottom][col].teams[team_id].weight, neighbour_weight_close);
			grid->cells[row_bottom][col].teams[team_id].change_time = cls.demotime;

			// Bottom left.
			grid->cells[row_bottom][col_left].teams[team_id].weight = max(grid->cells[row_bottom][col_left].teams[team_id].weight, neighbour_weight_far);
			grid->cells[row_bottom][col_left].teams[team_id].change_time = cls.demotime;

			// Left.
			grid->cells[row][col_left].teams[team_id].weight = max(grid->cells[row][col_left].teams[team_id].weight, neighbour_weight_close);
			grid->cells[row][col_left].teams[team_id].change_time = cls.demotime;

			// Top left.
			grid->cells[row_top][col_left].teams[team_id].weight = max(grid->cells[row_top][col_left].teams[team_id].weight, neighbour_weight_far);
			grid->cells[row_top][col_left].teams[team_id].change_time = cls.demotime;
		}
	}
}

void StatsGrid_Gather(void)
{
	int i;
	int row, col;
	static int lastframecount = -1;
	player_state_t *state;
	player_info_t *info;

	// Initiate the grid if it hasn't already been initiated.
	// The grid is reset on all level changes.
	if (stats_grid == NULL) {
		if (!cl.worldmodel) {
			return;
		}

		// TODO: Create cvars that let us set these values.
		StatsGrid_Init(&stats_grid, // The grid to initiate.
			5.0,					// The time in miliseconds between fall offs.
			0.2,					// At each fall off period, how much should be substracted from the weight?
			50,						// The length in quad coordinates of a cells side length.
			fabs(cl.worldmodel->maxs[0] - cl.worldmodel->mins[0]), // Width of the map in quake coordinates.
			fabs(cl.worldmodel->maxs[1] - cl.worldmodel->mins[1]), // Height (if we're talking 2D where Y = height).
			0.0);					// The threshold before a team is considered to "hold" a cell.

		// We failed initializing the grid, so don't do anything.
		if(stats_grid == NULL) {
			return;
		}
	}

	// If it's the first time for this level initiate.
	if (stats_important_ents == NULL) {
		StatsGrid_InitHoldItems();
	}

	if (teamholdinfo == NULL || teamholdbar == NULL) {
		return;
	}
	if (teamholdinfo->show->value == 0 && teamholdbar->show->value == 0) {
		return;
	}

	// Only gather once per frame.
	if (cls.framecount == lastframecount) {
		return;
	}
	lastframecount = cls.framecount;

	if (!cl.oldparsecount || !cl.parsecount || cls.state < ca_active) {
		return;
	}

	// Make sure the team colors are correct.
	StatsGrid_ValidateTeamColors();

	// Get player state so we can know where he is (or on rare occassions, she).
	state = cl.frames[cl.oldparsecount & UPDATE_MASK].playerstate;

	// Get the info for the player.
	info = cl.players;

	for (i = 0; i < MAX_CLIENTS; i++, info++, state++) {
		// Skip spectators.
		if(!cl.players[i].name[0] || cl.players[i].spectator) {
			continue;
		}

		// Calculate the weight for this player and set it at the
		// cell the player is located on in the grid.
		StatsGrid_SetWeightForPlayer(stats_grid, state, info);
	}

	// Reset the amount of cells that each team "holds" since
	// they will be recalculated when the weights are decreased.
	stats_grid->teams[STATS_TEAM1].hold_count = 0;
	stats_grid->teams[STATS_TEAM2].hold_count = 0;
	StatsGrid_ResetHoldItemCounts();

	// Go through all the cells and decrease their weights.
	for(row = 0; row < stats_grid->row_count; row++) {
		for(col = 0; col < stats_grid->col_count; col++) {
			StatsGrid_DecreaseTeamWeights(stats_grid, row, col, stats_grid->hold_threshold);
		}
	}
}




// HUD rendering

#define HUD_TEAMHOLDINFO_STYLE_TEAM_NAMES		0
#define HUD_TEAMHOLDINFO_STYLE_PERCENT_BARS		1
#define HUD_TEAMHOLDINFO_STYLE_PERCENT_BARS2	2

// Team hold filters.
static qbool teamhold_show_pent = false;
static qbool teamhold_show_quad = false;
static qbool teamhold_show_ring = false;
static qbool teamhold_show_suit = false;
static qbool teamhold_show_rl = false;
static qbool teamhold_show_lg = false;
static qbool teamhold_show_gl = false;
static qbool teamhold_show_sng = false;
static qbool teamhold_show_mh = false;
static qbool teamhold_show_ra = false;
static qbool teamhold_show_ya = false;
static qbool teamhold_show_ga = false;

void TeamHold_DrawBars(int x, int y, int width, int height,
					   float team1_percent, float team2_percent,
					   int team1_color, int team2_color,
					   float opacity)
{
	int team1_width = 0;
	int team2_width = 0;
	int bar_height = 0;

	bar_height = Q_rint(height / 2.0);
	team1_width = (int)(width * team1_percent);
	team2_width = (int)(width * team2_percent);

	clamp(team1_width, 0, width);
	clamp(team2_width, 0, width);

	Draw_AlphaFill(x, y, team1_width, bar_height, team1_color, opacity);

	y += bar_height;

	Draw_AlphaFill(x, y, team2_width, bar_height, team2_color, opacity);
}

void TeamHold_DrawPercentageBar(
	int x, int y, int width, int height,
	float team1_percent, float team2_percent,
	int team1_color, int team2_color,
	int show_text, int vertical,
	int vertical_text, float opacity, float scale,
	qbool proportional
)
{
	int _x, _y;
	int _width, _height;

	if (vertical) {
		//
		// Draw vertical.
		//

		// Team 1.
		_x = x;
		_y = y;
		_width = max(0, width);
		_height = Q_rint(height * team1_percent);
		_height = max(0, height);

		Draw_AlphaFill(_x, _y, _width, _height, team1_color, opacity);

		// Team 2.
		_x = x;
		_y = Q_rint(y + (height * team1_percent));
		_width = max(0, width);
		_height = Q_rint(height * team2_percent);
		_height = max(0, _height);

		Draw_AlphaFill(_x, _y, _width, _height, team2_color, opacity);

		// Show the percentages in numbers also.
		if (show_text) {
			// TODO: Move this to a separate function (since it's prett much copy and paste for both teams).
			// Team 1.
			if (team1_percent > 0.05) {
				if (vertical_text) {
					int percent = 0;
					int percent10 = 0;
					int percent100 = 0;

					_x = x + (width / 2) - FontFixedWidth(1, scale / 2, true, proportional);
					_y = Q_rint(y + (height * team1_percent) / 2 - 8 * 1.5 * scale);

					percent = Q_rint(100 * team1_percent);

					if ((percent100 = percent / 100)) {
						Draw_SString(_x, _y, va("%d", percent100), scale, proportional);
						_y += 8 * scale;
					}

					if ((percent10 = percent / 10)) {
						Draw_SString(_x, _y, va("%d", percent10), scale, proportional);
						_y += 8 * scale;
					}

					Draw_SString(_x, _y, va("%d", percent % 10), scale, proportional);
					_y += 8 * scale;

					Draw_SString(_x, _y, "%", scale, proportional);
				}
				else {
					_x = x + (width / 2) - FontFixedWidth(1, 1.5 * scale, 1, proportional);
					_y = Q_rint(y + (height * team1_percent) / 2 - 8 * scale * 0.5);
					Draw_SString(_x, _y, va("%2.0f%%", 100 * team1_percent), scale, false);
				}
			}

			// Team 2.
			if (team2_percent > 0.05) {
				if (vertical_text) {
					int percent = 0;
					int percent10 = 0;
					int percent100 = 0;

					_x = x + (width / 2) - FontFixedWidth(1, scale / 2, true, proportional);
					_y = Q_rint(y + (height * team1_percent) + (height * team2_percent) / 2 - 12);

					percent = Q_rint(100 * team2_percent);

					if ((percent100 = percent / 100)) {
						Draw_SString(_x, _y, va("%d", percent100), scale, proportional);
						_y += 8;
					}

					if ((percent10 = percent / 10)) {
						Draw_SString(_x, _y, va("%d", percent10), scale, proportional);
						_y += 8;
					}

					Draw_SString(_x, _y, va("%d", percent % 10), scale, proportional);
					_y += 8;

					Draw_SString(_x, _y, "%", scale, proportional);
				}
				else {
					_x = x + (width / 2) - FontFixedWidth(1, scale * 1.5, true, proportional);
					_y = Q_rint(y + (height * team1_percent) + (height * team2_percent) / 2 - 4);
					Draw_SString(_x, _y, va("%2.0f%%", 100 * team2_percent), scale, proportional);
				}
			}
		}
	}
	else {
		//
		// Draw horizontal.
		//

		// Team 1.
		_x = x;
		_y = y;
		_width = Q_rint(width * team1_percent);
		_width = max(0, _width);
		_height = max(0, height);

		Draw_AlphaFill(_x, _y, _width, _height, team1_color, opacity);

		// Team 2.
		_x = Q_rint(x + (width * team1_percent));
		_y = y;
		_width = Q_rint(width * team2_percent);
		_width = max(0, _width);
		_height = max(0, height);

		Draw_AlphaFill(_x, _y, _width, _height, team2_color, opacity);

		// Show the percentages in numbers also.
		if (show_text) {
			// Team 1.
			if (team1_percent > 0.05) {
				_x = Q_rint(x + (width * team1_percent) / 2 - FontFixedWidth(1, scale, true, proportional));
				_y = y + (height / 2) - 4 * scale;
				Draw_SString(_x, _y, va("%2.0f%%", 100 * team1_percent), scale, proportional);
			}

			// Team 2.
			if (team2_percent > 0.05) {
				_x = Q_rint(x + (width * team1_percent) + (width * team2_percent) / 2 - FontFixedWidth(1, scale, true, proportional));
				_y = y + (height / 2) - 4 * scale;
				Draw_SString(_x, _y, va("%2.0f%%", 100 * team2_percent), scale, proportional);
			}
		}
	}
}

static void TeamHold_OnChangeItemFilterInfo(cvar_t *var, char *s, qbool *cancel)
{
	char *start = s;
	char *end = start;
	int order = 0;

	// Parse the item filter.
	teamhold_show_rl = Utils_RegExpMatch("RL", s);
	teamhold_show_quad = Utils_RegExpMatch("QUAD", s);
	teamhold_show_ring = Utils_RegExpMatch("RING", s);
	teamhold_show_pent = Utils_RegExpMatch("PENT", s);
	teamhold_show_suit = Utils_RegExpMatch("SUIT", s);
	teamhold_show_lg = Utils_RegExpMatch("LG", s);
	teamhold_show_gl = Utils_RegExpMatch("GL", s);
	teamhold_show_sng = Utils_RegExpMatch("SNG", s);
	teamhold_show_mh = Utils_RegExpMatch("MH", s);
	teamhold_show_ra = Utils_RegExpMatch("RA", s);
	teamhold_show_ya = Utils_RegExpMatch("YA", s);
	teamhold_show_ga = Utils_RegExpMatch("GA", s);

	// Reset the ordering of the items.
	StatsGrid_ResetHoldItemsOrder();

	// Trim spaces from the start of the word.
	while (*start && *start == ' ') {
		start++;
	}

	end = start;

	// Go through the string word for word and set a
	// rising order for each hold item based on their
	// order in the string.
	while (*end) {
		if (*end != ' ') {
			// Not at the end of the word yet.
			end++;
			continue;
		}
		else {
			// We've found a word end.
			char temp[256];

			// Try matching the current word with a hold item
			// and set it's ordering according to it's placement
			// in the string.
			strlcpy(temp, start, min(end - start, sizeof(temp)));
			StatsGrid_SetHoldItemOrder(temp, order);
			order++;

			// Get rid of any additional spaces.
			while (*end && *end == ' ') {
				end++;
			}

			// Start trying to find a new word.
			start = end;
		}
	}

	// Order the hold items.
	StatsGrid_SortHoldItems();
}

void SCR_HUD_DrawTeamHoldInfo(hud_t *hud)
{
	int i;
	int x, y;
	int width, height;

	static cvar_t
		*hud_teamholdinfo_style = NULL,
		*hud_teamholdinfo_opacity,
		*hud_teamholdinfo_width,
		*hud_teamholdinfo_height,
		*hud_teamholdinfo_onlytp,
		*hud_teamholdinfo_itemfilter,
		*hud_teamholdinfo_scale,
		*hud_teamholdinfo_proportional;

	if (hud_teamholdinfo_style == NULL)    // first time
	{
		char val[256];

		hud_teamholdinfo_style = HUD_FindVar(hud, "style");
		hud_teamholdinfo_opacity = HUD_FindVar(hud, "opacity");
		hud_teamholdinfo_width = HUD_FindVar(hud, "width");
		hud_teamholdinfo_height = HUD_FindVar(hud, "height");
		hud_teamholdinfo_onlytp = HUD_FindVar(hud, "onlytp");
		hud_teamholdinfo_itemfilter = HUD_FindVar(hud, "itemfilter");
		hud_teamholdinfo_scale = HUD_FindVar(hud, "scale");
		hud_teamholdinfo_proportional = HUD_FindVar(hud, "proportional");

		// Unecessary to parse the item filter string on each frame.
		hud_teamholdinfo_itemfilter->OnChange = TeamHold_OnChangeItemFilterInfo;

		// Parse the item filter the first time (trigger the OnChange function above).
		strlcpy(val, hud_teamholdinfo_itemfilter->string, sizeof(val));
		Cvar_Set(hud_teamholdinfo_itemfilter, val);
	}

	// Get the height based on how many items we have, or what the user has set it to.
	height = max(0, hud_teamholdinfo_height->value);
	width = max(0, hud_teamholdinfo_width->value);

	// Don't show when not in teamplay/demoplayback.
	if (!HUD_ShowInDemoplayback(hud_teamholdinfo_onlytp->value)) {
		HUD_PrepareDraw(hud, width, height, &x, &y);
		return;
	}

	// We don't have anything to show.
	if (stats_important_ents == NULL || stats_grid == NULL) {
		HUD_PrepareDraw(hud, width, height, &x, &y);
		return;
	}

	if (HUD_PrepareDraw(hud, width, height, &x, &y)) {
		int _y = 0;

		_y = y;

		// Go through all the items and print the stats for them.
		for (i = 0; i < stats_important_ents->count; i++) {
			float team1_percent;
			float team2_percent;
			int team1_hold_count = 0;
			int team2_hold_count = 0;
			int names_width = 0;

			// Don't draw outside the specified height.
			if ((_y - y) + 8 * hud_teamholdinfo_scale->value > height) {
				break;
			}

			// If the item isn't of the specified type, then skip it.
			if (!((teamhold_show_rl && !strncmp(stats_important_ents->list[i].name, "RL", 2))
				|| (teamhold_show_quad && !strncmp(stats_important_ents->list[i].name, "QUAD", 4))
				|| (teamhold_show_ring && !strncmp(stats_important_ents->list[i].name, "RING", 4))
				|| (teamhold_show_pent && !strncmp(stats_important_ents->list[i].name, "PENT", 4))
				|| (teamhold_show_suit && !strncmp(stats_important_ents->list[i].name, "SUIT", 4))
				|| (teamhold_show_lg && !strncmp(stats_important_ents->list[i].name, "LG", 2))
				|| (teamhold_show_gl && !strncmp(stats_important_ents->list[i].name, "GL", 2))
				|| (teamhold_show_sng && !strncmp(stats_important_ents->list[i].name, "SNG", 3))
				|| (teamhold_show_mh && !strncmp(stats_important_ents->list[i].name, "MH", 2))
				|| (teamhold_show_ra && !strncmp(stats_important_ents->list[i].name, "RA", 2))
				|| (teamhold_show_ya && !strncmp(stats_important_ents->list[i].name, "YA", 2))
				|| (teamhold_show_ga && !strncmp(stats_important_ents->list[i].name, "GA", 2))
				)) {
				continue;
			}

			// Calculate the width of the longest item name so we can use it for padding.
			names_width = FontFixedWidth(stats_important_ents->longest_name + 1, hud_teamholdinfo_scale->value, false, hud_teamholdinfo_proportional->integer);

			// Calculate the percentages of this item that the two teams holds.
			team1_hold_count = stats_important_ents->list[i].teams_hold_count[STATS_TEAM1];
			team2_hold_count = stats_important_ents->list[i].teams_hold_count[STATS_TEAM2];

			team1_percent = ((float)team1_hold_count) / (team1_hold_count + team2_hold_count);
			team2_percent = ((float)team2_hold_count) / (team1_hold_count + team2_hold_count);

			team1_percent = fabs(max(0, team1_percent));
			team2_percent = fabs(max(0, team2_percent));

			// Write the name of the item.
			Draw_SColoredStringBasic(x, _y, va("&cff0%s:", stats_important_ents->list[i].name), 0, hud_teamholdinfo_scale->value, hud_teamholdinfo_proportional->integer);

			if (hud_teamholdinfo_style->value == HUD_TEAMHOLDINFO_STYLE_TEAM_NAMES) {
				//
				// Prints the team name that holds the item.
				//
				if (team1_percent > team2_percent) {
					Draw_SColoredStringBasic(x + names_width, _y, stats_important_ents->teams[STATS_TEAM1].name, 0, hud_teamholdinfo_scale->value, hud_teamholdinfo_proportional->integer);
				}
				else if (team1_percent < team2_percent) {
					Draw_SColoredStringBasic(x + names_width, _y, stats_important_ents->teams[STATS_TEAM2].name, 0, hud_teamholdinfo_scale->value, hud_teamholdinfo_proportional->integer);
				}
			}
			else if (hud_teamholdinfo_style->value == HUD_TEAMHOLDINFO_STYLE_PERCENT_BARS) {
				//
				// Show a percentage bar for the item.
				//
				TeamHold_DrawPercentageBar(
					x + names_width, _y,
					Q_rint(hud_teamholdinfo_width->value - names_width), 8,
					team1_percent, team2_percent,
					stats_important_ents->teams[STATS_TEAM1].color,
					stats_important_ents->teams[STATS_TEAM2].color,
					0, // Don't show percentage values, get's too cluttered.
					false,
					false,
					hud_teamholdinfo_opacity->value,
					hud_teamholdinfo_scale->value,
					hud_teamholdinfo_proportional->integer
				);
			}
			else if (hud_teamholdinfo_style->value == HUD_TEAMHOLDINFO_STYLE_PERCENT_BARS2) {
				TeamHold_DrawBars(x + names_width, _y,
								  Q_rint(hud_teamholdinfo_width->value - names_width), 8,
								  team1_percent, team2_percent,
								  stats_important_ents->teams[STATS_TEAM1].color,
								  stats_important_ents->teams[STATS_TEAM2].color,
								  hud_teamholdinfo_opacity->value);
			}

			// Next line.
			_y += 8 * hud_teamholdinfo_scale->value;
		}
	}
}

void SCR_HUD_DrawTeamHoldBar(hud_t *hud)
{
	int x, y;
	int height = 8;
	int width = 0;
	float team1_percent = 0;
	float team2_percent = 0;

	static cvar_t
		*hud_teamholdbar_style = NULL,
		*hud_teamholdbar_opacity,
		*hud_teamholdbar_width,
		*hud_teamholdbar_height,
		*hud_teamholdbar_vertical,
		*hud_teamholdbar_show_text,
		*hud_teamholdbar_onlytp,
		*hud_teamholdbar_vertical_text,
		*hud_teamholdbar_scale,
		*hud_teamholdbar_proportional;

	if (hud_teamholdbar_style == NULL)    // first time
	{
		hud_teamholdbar_style = HUD_FindVar(hud, "style");
		hud_teamholdbar_opacity = HUD_FindVar(hud, "opacity");
		hud_teamholdbar_width = HUD_FindVar(hud, "width");
		hud_teamholdbar_height = HUD_FindVar(hud, "height");
		hud_teamholdbar_vertical = HUD_FindVar(hud, "vertical");
		hud_teamholdbar_show_text = HUD_FindVar(hud, "show_text");
		hud_teamholdbar_onlytp = HUD_FindVar(hud, "onlytp");
		hud_teamholdbar_vertical_text = HUD_FindVar(hud, "vertical_text");
		hud_teamholdbar_scale = HUD_FindVar(hud, "scale");
		hud_teamholdbar_proportional = HUD_FindVar(hud, "proportional");
	}

	height = max(1, hud_teamholdbar_height->value);
	width = max(0, hud_teamholdbar_width->value);

	// Don't show when not in teamplay/demoplayback.
	if (!HUD_ShowInDemoplayback(hud_teamholdbar_onlytp->value)) {
		HUD_PrepareDraw(hud, width, height, &x, &y);
		return;
	}

	if (HUD_PrepareDraw(hud, width, height, &x, &y)) {
		// We need something to work with.
		if (stats_grid != NULL) {
			// Check if we have any hold values to calculate from.
			if (stats_grid->teams[STATS_TEAM1].hold_count + stats_grid->teams[STATS_TEAM2].hold_count > 0) {
				// Calculate the percentage for the two teams for the "team strength bar".
				team1_percent = ((float)stats_grid->teams[STATS_TEAM1].hold_count) / (stats_grid->teams[STATS_TEAM1].hold_count + stats_grid->teams[STATS_TEAM2].hold_count);
				team2_percent = ((float)stats_grid->teams[STATS_TEAM2].hold_count) / (stats_grid->teams[STATS_TEAM1].hold_count + stats_grid->teams[STATS_TEAM2].hold_count);

				team1_percent = fabs(max(0, team1_percent));
				team2_percent = fabs(max(0, team2_percent));
			}
			else {
				Draw_AlphaFill(x, y, hud_teamholdbar_width->value, height, 0, hud_teamholdbar_opacity->value*0.5);
				return;
			}

			// Draw the percentage bar.
			TeamHold_DrawPercentageBar(
				x, y, width, height,
				team1_percent, team2_percent,
				stats_grid->teams[STATS_TEAM1].color,
				stats_grid->teams[STATS_TEAM2].color,
				hud_teamholdbar_show_text->value,
				hud_teamholdbar_vertical->value,
				hud_teamholdbar_vertical_text->value,
				hud_teamholdbar_opacity->value,
				hud_teamholdbar_scale->value,
				hud_teamholdbar_proportional->integer
			);
		}
		else {
			// If there's no stats grid available we don't know what to show, so just show a black frame.
			Draw_AlphaFill(x, y, hud_teamholdbar_width->value, height, 0, hud_teamholdbar_opacity->value * 0.5);
		}
	}
}

void TeamHold_HudInit(void)
{
	teamholdinfo = HUD_Register(
		"teamholdinfo", NULL, "Shows which important items in the level that are being held by the teams.",
		HUD_PLUSMINUS, ca_active, 0, SCR_HUD_DrawTeamHoldInfo,
		"0", "top", "left", "bottom", "0", "0", "0", "0 0 0", NULL,
		"opacity", "0.8",
		"width", "200",
		"height", "8",
		"onlytp", "0",
		"style", "1",
		"itemfilter", "quad ra ya ga mega pent rl quad",
		"scale", "1",
		"proportional", "0",
		NULL
	);

	teamholdbar = HUD_Register(
		"teamholdbar", NULL, "Shows how much of the level (in percent) that is currently being held by either team.",
		HUD_PLUSMINUS, ca_active, 0, SCR_HUD_DrawTeamHoldBar,
		"0", "top", "left", "bottom", "0", "0", "0", "0 0 0", NULL,
		"opacity", "0.8",
		"width", "200",
		"height", "8",
		"vertical", "0",
		"vertical_text", "0",
		"show_text", "1",
		"onlytp", "0",
		"scale", "1",
		"proportional", "0",
		NULL
	);
}
