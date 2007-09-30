#include "quakedef.h"
#ifdef GLQUAKE
#include "gl_model.h"
#include "gl_local.h"
#else
#include "r_model.h"
#include "r_local.h"
#endif
#include "stats_grid.h"
#include "sbar.h"


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

void StatsGrid_ResetHoldItemsOrder()
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

void StatsGrid_InitHoldItems()
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
		if(!strcmp(cl_visents.list[i].model->name, "progs/invulner.mdl"))
		{
			StatsGrid_SetHoldItemName(temp_ents[ents_count].name, "PENT", ++pent_count);
		}
		else if(!strcmp(cl_visents.list[i].model->name, "progs/quaddama.mdl"))
		{
			StatsGrid_SetHoldItemName(temp_ents[ents_count].name, "QUAD", ++quad_count);
		}
		else if(!strcmp(cl_visents.list[i].model->name, "progs/invisibl.mdl"))
		{
			StatsGrid_SetHoldItemName(temp_ents[ents_count].name, "RING", ++ring_count);
		}
		else if(!strcmp(cl_visents.list[i].model->name, "progs/suit.mdl"))
		{
			StatsGrid_SetHoldItemName(temp_ents[ents_count].name, "SUIT", ++suit_count);
		}
		else if(!strcmp(cl_visents.list[i].model->name, "progs/g_rock2.mdl"))
		{
			StatsGrid_SetHoldItemName(temp_ents[ents_count].name, "RL", ++rl_count);
		}
		else if(!strcmp(cl_visents.list[i].model->name, "progs/g_light.mdl"))
		{
			StatsGrid_SetHoldItemName(temp_ents[ents_count].name, "LG", ++lg_count);
		}
		else if(!strcmp(cl_visents.list[i].model->name, "progs/g_rock.mdl"))
		{
			StatsGrid_SetHoldItemName(temp_ents[ents_count].name, "GL", ++gl_count);
		}
		else if(!strcmp(cl_visents.list[i].model->name, "progs/g_nail2.mdl"))
		{
			StatsGrid_SetHoldItemName(temp_ents[ents_count].name, "SNG", ++sng_count);
		}
		else if(!strcmp(cl_visents.list[i].model->name, "maps/b_bh100.bsp"))
		{
			// Megahealth.
			StatsGrid_SetHoldItemName(temp_ents[ents_count].name, "MH", ++mega_count);
		}
		else if(!strcmp(cl_visents.list[i].model->name, "progs/armor.mdl"))
		{
			if(cl_visents.list[i].skinnum == 0)
			{
				StatsGrid_SetHoldItemName(temp_ents[ents_count].name, "GA", ++ga_count);
			}
			else if(cl_visents.list[i].skinnum == 1)
			{
				StatsGrid_SetHoldItemName(temp_ents[ents_count].name, "YA", ++ya_count);
			}
			else if(cl_visents.list[i].skinnum == 2)
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
		VectorCopy(cl_visents.list[i].origin, temp_ents[ents_count].origin);

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

void StatsGrid_ValidateTeamColors()
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

void StatsGrid_ResetHoldItems()
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

void StatsGrid_ResetHoldItemCounts()
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

void StatsGrid_SortHoldItems()
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

	cell_weight_t *weight_t = NULL;

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

	// Get the weight.
	weight_t = &grid->cells[row][col].teams[team_id];

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

void StatsGrid_Gather()
{
	int i;
	int row, col;
	static int lastframecount = -1;
	player_state_t *state;
	player_info_t *info;

	// Initiate the grid if it hasn't already been initiated.
	// The grid is reset on all level changes.
	if(stats_grid == NULL)
	{
		if (!cl.worldmodel)
			return;

		// TODO: Create cvars that let us set these values.
		StatsGrid_Init(&stats_grid, // The grid to initiate.
			5.0,					// The time in miliseconds between fall offs.
			0.2,					// At each fall off period, how much should be substracted from the weight?
			50,						// The length in quad coordinates of a cells side length.
			fabs(cl.worldmodel->maxs[0] - cl.worldmodel->mins[0]), // Width of the map in quake coordinates.
			fabs(cl.worldmodel->maxs[1] - cl.worldmodel->mins[1]), // Height (if we're talking 2D where Y = height).
			0.0);					// The threshold before a team is considered to "hold" a cell.

		// We failed initializing the grid, so don't do anything.
		if(stats_grid == NULL)
		{
			return;
		}
	}

	// If it's the first time for this level initiate.
	if(stats_important_ents == NULL)
	{
		StatsGrid_InitHoldItems();
	}

	// Only gather once per frame.
	if (cls.framecount == lastframecount)
	{
		return;
	}
	lastframecount = cls.framecount;

	if (!cl.oldparsecount || !cl.parsecount || cls.state < ca_active)
	{
		return;
	}

	// Make sure the team colors are correct.
	StatsGrid_ValidateTeamColors();

	// Get player state so we can know where he is (or on rare occassions, she).
	state = cl.frames[cl.oldparsecount & UPDATE_MASK].playerstate;

	// Get the info for the player.
	info = cl.players;

	for(i = 0; i < MAX_CLIENTS; i++, info++, state++)
	{
		// Skip spectators.
		if(!cl.players[i].name[0] || cl.players[i].spectator)
		{
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
	for(row = 0; row < stats_grid->row_count; row++)
	{
		for(col = 0; col < stats_grid->col_count; col++)
		{
			StatsGrid_DecreaseTeamWeights(stats_grid, row, col, stats_grid->hold_threshold);
		}
	}
}
