#include "quakedef.h"
#include "common.h"
#include "q_shared.h"

#ifndef __STATS_GRID__H__
#define __STATS_GRID__H__

#define ROUND(f)   ((f>=0)?(int)(f + .5):(int)(f - .5))

typedef struct
{
	float			weight;				// Weight of the box. Between 0.0 and 1.0
	float			change_time;		// The last time the weight was changed.
} cell_weight_t;

typedef struct
{
	cell_weight_t	team1_weight;		// Team 1's weight for this box.
	cell_weight_t	team2_weight;		// Team 2's weight for this box.
	float			tl_x;				// Top left x position.
	float			tl_y;				// Top left y position.
} radar_cell_t;

typedef struct
{
	char	name[MAX_INFO_STRING];	// Team name.
	int		color;					// Team color.
	int		hold_count;				// The amount of visited cells that this team "holds".
} stats_team_t;						// TODO: Change

typedef struct
{
	radar_cell_t	**cells;				// The cells.
	float			falloff_interval;		// The duration since the last weight change
											// to decrease the weight of the cells in the grid.
	float			falloff_value;			// The amount the cell weight should decrease by
											// at each falloff interval.
	int				cell_length;			// Cell side length.
	int				row_count;				// Row count.
	int				col_count;				// Column count.
	int				width;					// The width of the grid.
	int				height;					// The height of the grid.
	// TODO: Change all this team stuff into an array of stats_team_t struct instead (Allow more than 2 teams maybe?).
	char			team1[MAX_INFO_STRING]; // Name of team 1.
	char			team2[MAX_INFO_STRING]; // Name of team 2.
	int				team1_color;			// Team 1 color.
	int				team2_color;			// Team 2 color.
	int				team1_hold;				// The amount of visited cells where team 1
											// has a greater weight than team2.
	int				team2_hold;
	float			hold_threshold;			// The threshold for the weight that is required before
											// a cell is considered being held by a team. (0.0 is default).
} stats_weight_grid_t;

void StatsGrid_Remove(stats_weight_grid_t **grid);
void StatsGrid_Init(stats_weight_grid_t **grid, 
						 float falloff_interval, 
						 float falloff_value,
						 int cell_length,
						 float grid_width, 
						 float grid_height,
						 float hold_threshold);
void StatsGrid_InitTeamNames(stats_weight_grid_t *grid);
void StatsGrid_Change(stats_weight_grid_t *grid,
							float falloff_interval,
							float falloff_value,
							int grid_width,
							int grid_height);
void StatsGrid_DecreaseWeight(cell_weight_t *weight, stats_weight_grid_t *grid);
void StatsGrid_Gather();

extern stats_weight_grid_t *stats_grid;

#endif