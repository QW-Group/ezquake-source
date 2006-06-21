#include "quakedef.h"
#include "common.h"
#include "q_shared.h"

#ifndef __STATS_GRID__H__
#define __STATS_GRID__H__

#define ROUND(f)   ((f>=0)?(int)(f + .5):(int)(f - .5))

#define TEAM_COUNT		2
#define STATS_TEAM1		0
#define STATS_TEAM2		1

typedef struct
{
	float			weight;					// Weight of the box. Between 0.0 and 1.0
	float			change_time;			// The last time the weight was changed. (Used for fading weight).
	float			death_weight;			// The amount of deaths by this team in this cell (Never fades).
} cell_weight_t;

typedef struct
{
	cell_weight_t	teams[TEAM_COUNT];		// The team weights for this cell.
	float			tl_x;					// Top left x position of the cell.
	float			tl_y;					// Top left y position of the cell.
} stats_cell_t;

typedef struct
{
	char	name[MAX_INFO_STRING];			// Team name.
	int		color;							// Team color.
	int		hold_count;						// The amount of visited cells that this team "holds".
} stats_team_t;

typedef struct
{
	stats_cell_t	**cells;				// The cells.
	float			falloff_interval;		// The duration since the last weight change
											// to decrease the weight of the cells in the grid.
	float			falloff_value;			// The amount the cell weight should decrease by
											// at each falloff interval.
	int				cell_length;			// Cell side length.
	int				row_count;				// Row count.
	int				col_count;				// Column count.
	int				width;					// The width of the grid.
	int				height;					// The height of the grid.
	stats_team_t	teams[TEAM_COUNT];		// The teams (No more than 2). 
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
