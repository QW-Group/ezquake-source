/*
Copyright (C) 2001-2002 jogihoogi

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

$Id: mvd_utils_common.h,v 1.2 2007-09-24 21:34:51 johnnycz Exp $
*/

// Shared declarations for the whole mvd group of tools
// declarations from mvd_utils.c and mvd_autotrack.c

#define MH_INFO		14
#define RA_INFO		13
#define YA_INFO		12
#define GA_INFO		11
#define PENT_INFO	10
#define QUAD_INFO	9
#define RING_INFO	8
#define LG_INFO		7
#define RL_INFO		6
#define GL_INFO		5
#define SNG_INFO	4
#define NG_INFO		3
#define SSG_INFO	2
#define SG_INFO		1
#define AXE_INFO	0
#define mvd_info_types 15

// killstats structures
typedef struct mvd_ks_w_s {
	int kills;
	int teamkills;
	int lastkills;
} mvd_ks_w_t;

typedef struct mvd_ks_s {
	mvd_ks_w_t normal[8];	// weapon kills axe - lg
	mvd_ks_w_t q[7];
	mvd_ks_w_t p[7];
	mvd_ks_w_t r[7];
	mvd_ks_w_t qp[7];
	mvd_ks_w_t qr[7];
	mvd_ks_w_t pr[7];
	mvd_ks_w_t qpr[7];
} mvd_ks_t;

typedef struct  mvd_ds_s {
	int isdead;
	int deathcount;
	double alivetimestart;
	double alivetime;

} mvd_ds_t ;

// keeps runstats from spawn to death
typedef struct mvd_runs_s {
	double time;
	int frags;
	int teamfrags;
	double starttime;

} mvd_runs_t ;

// average run statistics
typedef struct mvd_avgruns_s {
	double avg_time;
	double avg_frags;
	double avg_teamfrags;
} mvd_avgruns_t;

// average run statistics
typedef struct mvd_avgruns_all_s {
	mvd_avgruns_t all;
	// following 3 are not implemented yet
	mvd_avgruns_t quad;
	mvd_avgruns_t pent;
	mvd_avgruns_t ring;
} mvd_avgruns_all_t;

// item stuff
typedef struct mvd_pw_s {
	int has;
	double starttime;
	double ctime;
	int count ;
	int lost ;
	int mention;
	mvd_runs_t runs[512];
	int run;
} mvd_pw_t;

typedef struct mvd_info_s {
    float value;
    int lfw;				//last fired weapon
	mvd_ds_t das;			//dead alive stats
	mvd_pw_t info[15];		//iteam stats
	mvd_runs_t runs[512];	//
	mvd_avgruns_all_t run_stats;	// calculated from other items
	mvd_ks_t killstats;		// killstats
	int spawntelefrags;
	int teamspawntelefrags;
	int teamfrags;			// frags are in cl.players structure
	int lastfrags;
	int run;
	int firstrun;

} mvd_info_t;

typedef struct items_s {
	int took;
	int lost;
} items_t;

typedef struct mvd_event_s {
	int			type;			// what happened ?	0-spawn,1-death,2-kill,3-teamkill,4-took,5-powerup_end
	double		time;		// when did it happen ?
	vec3_t		loc;		// where did it happen ?
	int			info;		// for item on tooks
							// lfw on deaths
							// lfw on kills 0-7 axe-lg,-1 spawn
							//
	int			k_id;		// userid of the guy we killed
} mvd_event_t;

typedef struct mvd_new_info_s {
	int id;							//id cl.players[id]
	float value;						//mvd_info/mvd_autotrack value

	mvd_event_t		*event;
	items_t			item_info[mvd_info_types];
	player_state_t	*p_state ;
	player_info_t	*p_info;
	//p_state = cl.frames[cl.parsecount & UPDATE_MASK].playerstate[i];
	//p_info_players = cl.players[i];
	int lwf;						// last weapon fired
	mvd_info_t info;
} mvd_new_info_t;// mvd_new_info;

extern mvd_new_info_t mvd_new_info[MAX_CLIENTS];

typedef struct mvd_cg_info_s {
	char mapname[1024];
	char team1[1024];
	char team2[1024];
	char hostname[1024];
	int gametype;
	int timelimit;
	int pcount;
	int deathmatch;
} mvd_cg_info_s;

extern mvd_cg_info_s mvd_cg_info;

typedef struct mvd_gt_info_s {
	int id;
	char *name;
} mvd_gt_info_t;

#define gt_unknown	3
#define gt_4on4	3
#define gt_3on3	2
#define gt_2on2	1
#define gt_1on1	0
#define mvd_gt_types 5

extern mvd_gt_info_t mvd_gt_info[mvd_gt_types];

typedef struct mvd_wp_info_s {
	int		id;
	char	*name;
	int		it;
	const char *colored_name;
} mvd_wp_info_t;

extern mvd_wp_info_t mvd_wp_info[mvd_info_types];

// mvd_autotrack:
void MVD_AutoTrack_f(void);
void MVD_AutoTrack_Init(void);

// mvd_xmlstats:
void MVD_XMLStats_Init(void);
