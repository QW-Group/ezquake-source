
#ifndef EZQUAKE_R_FRAMESTATS_HEADER
#define EZQUAKE_R_FRAMESTATS_HEADER

typedef enum {
	polyTypeWorldModel,
	polyTypeAliasModel,
	polyTypeBrushModel,

	polyTypeMaximum
} frameStatsPolyType;

typedef struct r_frame_stats_classic_s {
	int polycount[polyTypeMaximum];
} r_frame_stats_classic_t;

typedef struct r_frame_stats_modern_s {
	int world_batches;
	int buffer_uploads;
	int multidraw_calls;
} r_frame_stats_modern_t;

typedef struct r_frame_stats_s {
	r_frame_stats_classic_t classic;
	r_frame_stats_modern_t modern;

	qbool hotloop;
	int hotloop_mallocs;
	int mallocs;
	int hotloop_stringops;
	int stringops;

	int texture_binds;
	unsigned int lightmap_min_changed;
	unsigned int lightmap_max_changed;
	int lightmap_updates;
	int draw_calls;
	int subdraw_calls;
	int particle_count;

	double start_time;
	double end_time;
} r_frame_stats_t;

extern r_frame_stats_t frameStats, prevFrameStats;
extern cvar_t r_speeds;

#endif // EZQUAKE_R_FRAMESTATS_HEADER
