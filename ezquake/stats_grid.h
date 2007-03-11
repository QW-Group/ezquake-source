#include "quakedef.h"
#include "common.h"
#include "q_shared.h"

#ifndef __STATS_GRID__H__
#define __STATS_GRID__H__

#define TEAM_COUNT		2
#define STATS_TEAM1		0
#define STATS_TEAM2		1

typedef struct cell_weight_s
{
	float			weight;					// Weight of the box. Between 0.0 and 1.0
	float			change_time;			// The last time the weight was changed. (Used for fading weight).
	float			death_weight;			// The amount of deaths by this team in this cell (Never fades).
} cell_weight_t;

typedef struct stats_cell_s
{
	cell_weight_t	teams[TEAM_COUNT];		// The team weights for this cell.
	float			tl_x;					// Top left x position of the cell.
	float			tl_y;					// Top left y position of the cell.
} stats_cell_t;

typedef struct stats_team_s
{
	char	name[MAX_INFO_STRING];			// Team name.
	int		color;							// Team color.
	int		hold_count;						// The amount of visited cells that this team "holds".
} stats_team_t;

typedef struct stats_weight_grid_s
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

typedef struct stats_entity_s
{
	char			name[MAX_INFO_STRING];	// The name of the entity (RA, RL, YA, QUAD).
	vec3_t			origin;					// The entitys origin.
	int				teams_hold_count[TEAM_COUNT];
	int				order;
} stats_entity_t;

typedef struct
{
	stats_entity_t	*list;
	int				count;
	float			hold_radius;
	int				longest_name;
	stats_team_t	teams[TEAM_COUNT];
} stats_entities_t;

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
void StatsGrid_ResetHoldItems();
void StatsGrid_SortHoldItems();
void StatsGrid_SetHoldItemOrder(const char *item_name, int order);
void StatsGrid_ResetHoldItemsOrder();

extern stats_weight_grid_t	*stats_grid;			// The weight grid for all the statistics.
extern stats_entities_t		*stats_important_ents;	// A list of "important" entities on the map, and counts on what team holds it.

#endif
