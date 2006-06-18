
#include "stats_grid.h"

stats_weight_grid_t *stats_grid = NULL;

void StatsGrid_Remove(stats_weight_grid_t **grid)
{
	int row;

	// Nothing to remove.
	if((*grid) == NULL)
	{
		return;
	}
	
	// Free all the rows-
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
	(*grid)->cells = NULL;

	// Free the grid itself.
	Q_free((*grid));
	(*grid) = NULL;
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
	
	if((*grid) == NULL)
	{
		// Failure.
		return;
	}

	// Get the row and col count.
	(*grid)->row_count = ROUND(grid_height / cell_length);
	(*grid)->col_count = ROUND(grid_width / cell_length);

	// Allocate the rows.
	(*grid)->cells = (radar_cell_t **)Q_calloc((*grid)->row_count, sizeof(radar_cell_t));
	
	// If we failed allocating the rows, cleanup and return.
	if((*grid)->cells == NULL)
	{
		Q_free(*grid);
		(*grid) = NULL;
		return;
	}

	// Allocate the columns for each row.
	for(row = 0; row < (*grid)->row_count; row++)
	{
		// Allocate memory for the current rows columns.
		(*grid)->cells[row] = (radar_cell_t *)Q_calloc((*grid)->col_count, sizeof(radar_cell_t));

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
			(*grid)->cells[row][col].team1_weight.weight = 0.0;
			(*grid)->cells[row][col].team1_weight.change_time = 0.0;

			(*grid)->cells[row][col].team2_weight.weight = 0.0;
			(*grid)->cells[row][col].team2_weight.change_time = 0.0;

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
	(*grid)->team1[0]			= 0;
	(*grid)->team2[0]			= 0;
	(*grid)->team1_color		= 0;
	(*grid)->team2_color		= 0;
	(*grid)->team1_hold			= 0;
	(*grid)->team2_hold			= 0;
	(*grid)->hold_threshold		= hold_threshold;
}

void StatsGrid_InitTeamNames(stats_weight_grid_t *grid)
{
	int i;

	// Get the first players team and set that as team1.
	strcpy(grid->team1, cl.players[0].team);
	grid->team1_color = Sbar_BottomColor(&cl.players[0]);

	// Go through the rest of the players until another team is found, that must be team2.
	for (i = 0; i < MAX_CLIENTS; i++) 
	{
		// Skip spectators.
		if(!cl.players[i].name[0] || cl.players[i].spectator)
		{
			continue;
		}

		// If this players team isn't the same as the first players team
		// set this players team as team 2.
		if(strcmp(grid->team1, cl.players[i].team))
		{
			strcpy(grid->team2, cl.players[i].team);
			grid->team2_color = Sbar_BottomColor(&cl.players[i]);
			break;
		}
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

void StatsGrid_DecreaseTeamWeights(stats_weight_grid_t *grid, int row, int col, float hold_threshold)
{
	// Don't try to do something stupid.
	if(grid == NULL || row < 0 || col < 0)
	{
		return;
	}

	// No point in doing anything if the weights are all zero.
	if(grid->cells[row][col].team1_weight.weight + grid->cells[row][col].team2_weight.weight <= 0)
	{
		return;
	}

	// Increase the amount of cells that is being "hold" by the teams.
	if(grid->cells[row][col].team1_weight.weight > grid->cells[row][col].team2_weight.weight)
	{
		if(grid->cells[row][col].team1_weight.weight > hold_threshold)
		{
			grid->team1_hold++;
		}
	}
	else
	{
		if(grid->cells[row][col].team2_weight.weight > hold_threshold)
		{
			grid->team2_hold++;
		}
	}

	// Decrease the weights for both teams.
	StatsGrid_DecreaseWeight(&grid->cells[row][col].team1_weight, grid);
	StatsGrid_DecreaseWeight(&grid->cells[row][col].team2_weight, grid);
}

void StatsGrid_SetWeightForPlayer(stats_weight_grid_t *grid, 
								  player_state_t *player_state, 
								  player_info_t *player_info)
{
	float weight = 0.0;
	int row, col;
	cell_weight_t *weight_t = NULL;

	// Don't calculate any weights before a match has started.
	// Don't allow setting the weight at the exact time the countdown
	// or standby ends (cl.gametime == 0), that will result in a weight
	// being set in the position that the player was the milisecond before.
	if(cl.countdown || cl.standby || cl.gametime <= 0
		|| grid == NULL || grid->cells == NULL			// Make sure we have something to work with.
		|| grid->col_count <= 0 || grid->row_count <= 0
		|| player_info == NULL || player_state == NULL )
	{
		return;
	}

	// Set the team names.
	if(!grid->team1[0] || !grid->team2[0])
	{
		StatsGrid_InitTeamNames(grid);
	}

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

	row = fabs(cl.worldmodel->mins[1] - player_state->origin[1]) / grid->cell_length;
	col = fabs(cl.worldmodel->mins[0] - player_state->origin[0]) / grid->cell_length;

	row = min(row, grid->row_count - 1);
	col = min(col, grid->col_count - 1);

	if(!strcmp(player_info->team, grid->team1))
	{
		weight_t = &grid->cells[row][col].team1_weight;
	}
	else
	{
		weight_t = &grid->cells[row][col].team2_weight;
	}

	if(weight >= weight_t->weight)
	{
		int row_top;
		int row_bottom;

		int col_left;
		int col_right;

		float neighbour_weight_close;
		float neighbour_weight_far;

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

		// Fade the weights for the neighbours. The edge corners
		// are at 50% opacity, and the ones at top/bottom, left/right at 80%.
		neighbour_weight_close	= weight * 0.8;
		neighbour_weight_far	= weight * 0.5;

		if(!strcmp(player_info->team, grid->team1))
		{
			// The current cell.
			grid->cells[row][col].team1_weight.weight = weight;
			grid->cells[row][col].team1_weight.change_time = cls.demotime;

			// Top.
			grid->cells[row_top][col].team1_weight.weight = max(grid->cells[row_top][col].team1_weight.weight, neighbour_weight_close);
			grid->cells[row_top][col].team1_weight.change_time = cls.demotime;

			// Top right.
			grid->cells[row_top][col_right].team1_weight.weight = max(grid->cells[row_top][col_right].team1_weight.weight, neighbour_weight_far);
			grid->cells[row_top][col_right].team1_weight.change_time = cls.demotime;

			// Right.
			grid->cells[row][col_right].team1_weight.weight = max(grid->cells[row][col_right].team1_weight.weight, neighbour_weight_close);
			grid->cells[row][col_right].team1_weight.change_time = cls.demotime;

			// Bottom right.
			grid->cells[row_bottom][col_right].team1_weight.weight = max(grid->cells[row_bottom][col_right].team1_weight.weight, neighbour_weight_far);
			grid->cells[row_bottom][col_right].team1_weight.change_time = cls.demotime;

			// Bottom.
			grid->cells[row_bottom][col].team1_weight.weight = max(grid->cells[row_bottom][col].team1_weight.weight, neighbour_weight_close);
			grid->cells[row_bottom][col].team1_weight.change_time = cls.demotime;

			// Bottom left.
			grid->cells[row_bottom][col_left].team1_weight.weight = max(grid->cells[row_bottom][col_left].team1_weight.weight, neighbour_weight_far);
			grid->cells[row_bottom][col_left].team1_weight.change_time = cls.demotime;

			// Left.
			grid->cells[row][col_left].team1_weight.weight = max(grid->cells[row][col_left].team1_weight.weight, neighbour_weight_close);
			grid->cells[row][col_left].team1_weight.change_time = cls.demotime;

			// Top left.
			grid->cells[row_top][col_left].team1_weight.weight = max(grid->cells[row_top][col_left].team1_weight.weight, neighbour_weight_far);
			grid->cells[row_top][col_left].team1_weight.change_time = cls.demotime;
		}
		else
		{
			// The current cell.
			grid->cells[row][col].team2_weight.weight = weight;
			grid->cells[row][col].team2_weight.change_time = cls.demotime;

			// Top.
			grid->cells[row_top][col].team2_weight.weight = max(grid->cells[row_top][col].team2_weight.weight, neighbour_weight_close);
			grid->cells[row_top][col].team2_weight.change_time = cls.demotime;

			// Top right.
			grid->cells[row_top][col_right].team2_weight.weight = max(grid->cells[row_top][col_right].team2_weight.weight, neighbour_weight_far);
			grid->cells[row_top][col_right].team2_weight.change_time = cls.demotime;

			// Right.
			grid->cells[row][col_right].team2_weight.weight = max(grid->cells[row][col_right].team2_weight.weight, neighbour_weight_close);
			grid->cells[row][col_right].team2_weight.change_time = cls.demotime;

			// Bottom right.
			grid->cells[row_bottom][col_right].team2_weight.weight = max(grid->cells[row_bottom][col_right].team2_weight.weight, neighbour_weight_far);
			grid->cells[row_bottom][col_right].team2_weight.change_time = cls.demotime;

			// Bottom.
			grid->cells[row_bottom][col].team2_weight.weight = max(grid->cells[row_bottom][col].team2_weight.weight, neighbour_weight_close);
			grid->cells[row_bottom][col].team2_weight.change_time = cls.demotime;

			// Bottom left.
			grid->cells[row_bottom][col_left].team2_weight.weight = max(grid->cells[row_bottom][col_left].team2_weight.weight, neighbour_weight_far);
			grid->cells[row_bottom][col_left].team2_weight.change_time = cls.demotime;

			// Left.
			grid->cells[row][col_left].team2_weight.weight = max(grid->cells[row][col_left].team2_weight.weight, neighbour_weight_close);
			grid->cells[row][col_left].team2_weight.change_time = cls.demotime;

			// Top left.
			grid->cells[row_top][col_left].team2_weight.weight = max(grid->cells[row_top][col_left].team2_weight.weight, neighbour_weight_far);
			grid->cells[row_top][col_left].team2_weight.change_time = cls.demotime;
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
		// TODO: Create cvars that let us set these values.
		StatsGrid_Init(&stats_grid, 5.0, 0.2, 50, 
			fabs(cl.worldmodel->maxs[0] - cl.worldmodel->mins[0]), // Width of the map in quake coordinates.
			fabs(cl.worldmodel->maxs[1] - cl.worldmodel->mins[1]), // Height (if we're talking 2D where Y = height).
			0.0);
		
		// We failed initializing the grid, so don't do anything.
		if(stats_grid == NULL)
		{
			return;
		}
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

	// Get player state so we can know where he is (or on rare occassions, she).
	state = cl.frames[cl.oldparsecount & UPDATE_MASK].playerstate;

	// Get the info for the player.
	info = cl.players;

	for(i = 0; i < MAX_CLIENTS; i++, info++, state++)
	{
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
	stats_grid->team1_hold = 0;
	stats_grid->team2_hold = 0;

	// Go through all the cells and decrease their weights.
	for(row = 0; row < stats_grid->row_count; row++)
	{
		for(col = 0; col < stats_grid->col_count; col++)
		{
			StatsGrid_DecreaseTeamWeights(stats_grid, row, col, stats_grid->hold_threshold);
		}
	}
}
