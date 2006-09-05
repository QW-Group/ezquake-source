/*
	$Id: mvd_utils.c,v 1.31 2006-09-05 21:37:18 johnnycz Exp $
*/

#include "quakedef.h"
#include "cl_screen.h"
#include "parser.h"
#include "localtime.h"
//#include "teamplay.h"

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

#define gt_unknown	3
#define gt_4on4	3	
#define gt_3on3	2	
#define gt_2on2	1	
#define gt_1on1	0
#define mvd_gt_types 5

typedef struct mvd_ks_w_s {
	int kills;
	int teamkills;
	int lastkills;
} mvd_ks_w_t;

typedef struct mvd_ks_s {
	mvd_ks_w_t normal[7];
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

typedef struct mvd_runs_s {
	double time;
	int frags;
	int teamfrags;
} mvd_runs_t ;

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
    int value;
    int lfw;				//last fired weapon
	mvd_ds_t das;			//dead alive stats 
	mvd_pw_t info[15];		//iteam stats
	mvd_runs_t runs[512];	//
	mvd_ks_t killstats;		// killstats
	int spawntelefrags;
	int teamspawntelefrags;
	int lastfrags;
	int run;
	int firstrun;
} mvd_info_t;

typedef struct mvd_gt_info_s {
	int id;
	char *name;
} mvd_gt_info_t;

static mvd_gt_info_t mvd_gt_info[mvd_gt_types] = {
	{gt_1on1,"duel"},
	{gt_2on2,"2on2"},
	{gt_3on3,"3on3"},
	{gt_4on4,"4on4"},
	{gt_unknown,"unknown"},
};

typedef struct mvd_cg_info_s {
	char mapname[1024];
	char team1[1024];
	char team2[1024];
	char hostname[1024];
	int gametype;
	int timelimit;
	int pcount;
} mvd_cg_info_s;

mvd_cg_info_s mvd_cg_info;

typedef struct mvd_wp_info_s {
	int id;
	char *name;
} mvd_wp_info_t;


static mvd_wp_info_t mvd_wp_info[mvd_info_types] = {
	{AXE_INFO,"axe"},
	{SG_INFO,"sg"},
	{SSG_INFO,"ssg"},
	{NG_INFO,"ng"},
	{SNG_INFO,"sng"},
	{GL_INFO,"gl"},
	{RL_INFO,"rl"},
	{LG_INFO,"lg"},
	{RING_INFO,"ring"},
	{QUAD_INFO,"quad"},
	{PENT_INFO,"pent"},
	{GA_INFO,"ga"},
	{YA_INFO,"ya"},
	{RA_INFO,"ra"},
	{MH_INFO,"mh"},
};

// NEW VERSION

typedef struct runs_s {
	double starttime;
	double endtime;
} runs_t;

typedef struct kill_s {
	int type; //0 - kill, 1 - selfkill
	double time;
	vec3_t location;
	int lwf ;
} kill_t;

typedef struct death_s	{
	double time;
	vec3_t location;
	int id;
} death_t;

typedef struct spawn_s {
	double time;
	vec3_t location;
} spawn_t;

typedef struct items_s {
	int took;
	int lost;
} items_t;

typedef struct mvd_event_s {
	int	type;	// what happened ?	0-spawn,1-death,2-kill,3-teamkill,4-took,5-powerup_end
	double	time;	// when did it happen ?
	vec3_t	loc;	// where did it happen ?
	int	info;	// for item on tooks
			// lfw on deaths
			// lfw on kills 0-7 axe-lg,-1 spawn
			//
	int	k_id;	// userid of the guy we killed
} mvd_event_t;

typedef struct mvd_new_info_s {
	int id;		//id cl.players[id]
	int value;	//mvd_info/mvd_autotrack value
	
	mvd_event_t	*event;
	items_t		item_info[mvd_info_types];
	player_state_t	*p_state ;
	player_info_t	*p_info;
	//p_state = cl.frames[cl.parsecount & UPDATE_MASK].playerstate[i];
	//p_info_players = cl.players[i];
	int lwf;	// last weapon fired
	mvd_info_t info;
} mvd_new_info_t;// mvd_new_info;

typedef struct mvd_new_info_cg_s {
	double game_starttime;
}mvd_new_info_cg_t; // mvd_new_info_cg;

//extern mt_matchstate_t matchstate;
//extern matchinfo_t matchinfo;
extern	centity_t		cl_entities[CL_MAX_EDICTS];
extern	entity_t		cl_static_entities[MAX_STATIC_ENTITIES];
double lasttime1 ,lasttime2;
double lasttime = 0;
double gamestart_time ;
int got_game_infos = 0;


extern cvar_t tp_name_quad, tp_name_pent, tp_name_ring, tp_name_separator, tp_name_backpack, tp_name_suit;
extern cvar_t tp_name_axe, tp_name_sg, tp_name_ssg, tp_name_ng, tp_name_sng, tp_name_gl, tp_name_rl, tp_name_lg,tp_name_ga,tp_name_ya,tp_name_ra,tp_name_mh;
extern cvar_t tp_name_none, tp_weapon_order;
char mvd_info_best_weapon[20];
extern int loc_loaded;
char mvd_status_xml_file[1024];



extern qbool TP_LoadLocFile (char *path, qbool quiet);
extern char *TP_LocationName(vec3_t location);
extern char *Weapon_NumToString(int num);

//matchinfo_t *MT_GetMatchInfo(void);

//char Mention_Win_Buf[80][1024];

mvd_new_info_t mvd_new_info[MAX_CLIENTS];

int FindBestNick(char *s, int use);
int MVD_AutoTrackBW_f(int i);
int axe_val, sg_val, ssg_val, ng_val, sng_val, gl_val, rl_val, lg_val, ga_val, ya_val, ra_val, ring_val, quad_val, pent_val ;

int mvd_demo_track_run = 0;
int last_track;

//event_handler_t *events=NULL;

// mvd_info cvars
cvar_t			mvd_info		= {"mvd_info", "0"};
cvar_t			mvd_info_show_header	= {"mvd_info_show_header", "0"};
cvar_t			mvd_info_setup		= {"mvd_info_setup", "%6n %3f %10l %4a %4h %5W"};
cvar_t			mvd_info_x		= {"mvd_info_x", "0"};
cvar_t			mvd_info_y		= {"mvd_info_y", "0"};

// mvd-announcer
cvar_t			mvd_moreinfo		= {"mvd_moreinfo", "0"};

// mvd_stats cvars
cvar_t			mvd_status		= {"mvd_status","0"};
cvar_t			mvd_status_x		= {"mvd_status_x","0"};
cvar_t			mvd_status_y		= {"mvd_status_y","0"};

// mvd_autotrack cvars
cvar_t mvd_autotrack = {"mvd_autotrack", "0"};
cvar_t mvd_autotrack_1on1 = {"mvd_autotrack_1on1", "%a * %A + 50 * %W + %p + %f"};
cvar_t mvd_autotrack_1on1_values = {"mvd_autotrack_1on1_values", "1 2 3 2 3 5 8 8 1 2 3 0 0 0"}; 
cvar_t mvd_autotrack_2on2 = {"mvd_autotrack_2on2", "%a * %A + 50 * %W + %p + %f"};
cvar_t mvd_autotrack_2on2_values = {"mvd_autotrack_2on2_values", "1 2 3 2 3 5 8 8 1 2 3 500 900 1000"}; 
cvar_t mvd_autotrack_4on4  = {"mvd_autotrack_4on4", "%a * %A + 50 * %W + %p + %f"};
cvar_t mvd_autotrack_4on4_values = {"mvd_autotrack_4on4_values", "1 2 4 2 4 6 10 10 1 2 3 500 900 1000"}; 
cvar_t mvd_autotrack_custom = {"mvd_autotrack_custom", "%a * %A + 50 * %W + %p + %f"};
cvar_t mvd_autotrack_custom_values = {"mvd_autotrack_custom_values", "1 2 3 2 3 5 6 6 1 2 3 500 900 1000"}; 

cvar_t mvd_multitrack_1 = {"mvd_multitrack_1", "%f"};
cvar_t mvd_multitrack_1_values = {"mvd_multitrack_1_values", "1 2 3 2 3 5 8 8 1 2 3 0 0 0"};
cvar_t mvd_multitrack_2 = {"mvd_multitrack_2", "%W"};
cvar_t mvd_multitrack_2_values = {"mvd_multitrack_2_values", "1 2 3 2 3 5 8 8 1 2 3 0 0 0"};
cvar_t mvd_multitrack_3 = {"mvd_multitrack_3", "%h"};
cvar_t mvd_multitrack_3_values = {"mvd_multitrack_3_values", "1 2 3 2 3 5 8 8 1 2 3 0 0 0"};
cvar_t mvd_multitrack_4 = {"mvd_multitrack_4", "%A"};
cvar_t mvd_multitrack_4_values = {"mvd_multitrack_4_values", "1 2 3 2 3 5 8 8 1 2 3 0 0 0"};

cvar_t mvd_write_xml = {"mvd_write_xml","0"};

int multitrack_id_1 = 0;
int multitrack_id_2 = 0;
int multitrack_id_3 = 0;
int multitrack_id_4 = 0;

char *multitrack_val ;
char *multitrack_str ;



typedef struct bp_var_s{
	int id;
	int val;
	} bp_var_t;

bp_var_t bp_var[MAX_CLIENTS];


char *Make_Red (char *s,int i){
	static char buf[1024];
	char *p,*ret;
	buf[0]= 0;
	ret=buf;
	for (p=s;*p;p++){
		if (!strspn(p,"1234567890.") || !(i))
		*ret++=*p | 128;
		else
		*ret++=*p;
	}
	*ret=0;
    return buf;
}

void MVD_Init_Info_f (void) {
	int i;
	int z;

	if (!got_game_infos){

	for (z=0,i=0;i<=MAX_CLIENTS;i++){
		if (!cl.players[i].name[0] || cl.players[i].spectator)
				continue;
		mvd_new_info[z].id = i;
		mvd_new_info[z++].p_info = &cl.players[i];	
	}

	strcpy(mvd_cg_info.mapname,TP_MapName());
	mvd_cg_info.timelimit=cl.timelimit;

	
	strcpy(mvd_cg_info.team1,mvd_new_info[0].p_info->team);
	for (i = 0; i < z; i++) {
		if(strcmp(mvd_new_info[i].p_info->team,mvd_cg_info.team1)){
			strcpy(mvd_cg_info.team2,mvd_new_info[i].p_info->team	);
			break;
		}
	}
	

	if (z==2)
		mvd_cg_info.gametype=0;
	else if (z==4)
		mvd_cg_info.gametype=1;
	else if (z==6)
		mvd_cg_info.gametype=2;
	else if (z==8)
		mvd_cg_info.gametype=3;
	else 
		mvd_cg_info.gametype=4;

	strcpy(mvd_cg_info.hostname,Info_ValueForKey(cl.serverinfo,"hostname"));

	mvd_cg_info.pcount = z;
	}
	got_game_infos=1;
	if (got_game_infos){
		for (i = 0; i < mvd_cg_info.pcount; i++)
			mvd_new_info[i].p_state = &cl.frames[cl.parsecount & UPDATE_MASK].playerstate[mvd_new_info[i].id];
	}
	
}


void MVD_Demo_Track (void){
	extern char track_name[16];
    extern cvar_t demo_playlist_track_name;
	
	int track_player ;

	
	#ifdef DEBUG
	printf("MVD_Demo_Track Started\n");
	#endif
		
	
	if(strlen(track_name)){
		track_player=FindBestNick(track_name,1);
		if (track_player != -1 )
			Cbuf_AddText (va("track %s\n",cl.players[track_player].name));
	}else if (strlen(demo_playlist_track_name.string)){
		track_player=FindBestNick(demo_playlist_track_name.string,1);
		if (track_player != -1 )
			Cbuf_AddText (va("track %s\n",cl.players[track_player].name));
	}

	mvd_demo_track_run = 1;
	#ifdef DEBUG
	printf("MVD_Demo_Track Stopped\n");
	#endif
}


int MVD_BestWeapon (int i) {
	int x;
	char *t[] = {tp_weapon_order.string, "78654321", NULL}, **s;	
	for (s = t; *s; s++) {
		for (x = 0 ; x < strlen(*s) ; x++) {
			switch ((*s)[x]) {
				case '1': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_AXE) return IT_AXE; break;
				case '2': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_SHOTGUN) return IT_SHOTGUN; break;
				case '3': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_SUPER_SHOTGUN) return IT_SUPER_SHOTGUN; break;
				case '4': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_NAILGUN) return IT_NAILGUN; break;
				case '5': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_SUPER_NAILGUN) return IT_SUPER_NAILGUN; break;
				case '6': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_GRENADE_LAUNCHER) return IT_GRENADE_LAUNCHER; break;
				case '7': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER) return IT_ROCKET_LAUNCHER; break;
				case '8': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_LIGHTNING) return IT_LIGHTNING; break;
			}
		}
	}
	return 0;
}

char *MVD_BestWeapon_strings (int i) {
	switch (MVD_BestWeapon(i)) {
		case IT_AXE: return tp_name_axe.string;
		case IT_SHOTGUN: return tp_name_sg.string;
		case IT_SUPER_SHOTGUN: return tp_name_ssg.string;
		case IT_NAILGUN: return tp_name_ng.string;
		case IT_SUPER_NAILGUN: return tp_name_sng.string;
		case IT_GRENADE_LAUNCHER: return tp_name_gl.string;
		case IT_ROCKET_LAUNCHER: return tp_name_rl.string;
		case IT_LIGHTNING: return tp_name_lg.string;
		default: return tp_name_none.string;	
	}
}

char *MVD_Weapon_strings (int i) {
	switch (i) {
		case IT_AXE: return tp_name_axe.string;
		case IT_SHOTGUN: return tp_name_sg.string;
		case IT_SUPER_SHOTGUN: return tp_name_ssg.string;
		case IT_NAILGUN: return tp_name_ng.string;
		case IT_SUPER_NAILGUN: return tp_name_sng.string;
		case IT_GRENADE_LAUNCHER: return tp_name_gl.string;
		case IT_ROCKET_LAUNCHER: return tp_name_rl.string;
		case IT_LIGHTNING: return tp_name_lg.string;
		default: return tp_name_none.string;	
	}
}

int MVD_Weapon_LWF (int i) {
	switch (i) {
		case IT_AXE: return 0;
		case IT_SHOTGUN: return 1;
		case IT_SUPER_SHOTGUN: return 2;
		case IT_NAILGUN: return 3;
		case IT_SUPER_NAILGUN: return 4;
		case IT_GRENADE_LAUNCHER: return 5;
		case IT_ROCKET_LAUNCHER: return 6;
		case IT_LIGHTNING: return 7;
		default: return 666;	
	}
}
				
char *MVD_BestAmmo (int i) {

	switch (MVD_BestWeapon(i)) {
		case IT_SHOTGUN: case IT_SUPER_SHOTGUN:
			return va("%i",mvd_new_info[i].p_info->stats[STAT_SHELLS]);
				
		case IT_NAILGUN: case IT_SUPER_NAILGUN:
			return va("%i",mvd_new_info[i].p_info->stats[STAT_NAILS]);
				
		case IT_GRENADE_LAUNCHER: case IT_ROCKET_LAUNCHER:
			return va("%i",mvd_new_info[i].p_info->stats[STAT_ROCKETS]);
			
		case IT_LIGHTNING:
			return va("%i",mvd_new_info[i].p_info->stats[STAT_CELLS]);
			
		default: return "0";
	}
}


void MVD_Info (void){
	char str[1024];
	char mvd_info_final_string[1024], mvd_info_powerups[20], mvd_info_header_string[1024];
	char *mapname;
	int x,y,z,i = 0;

	#ifdef DEBUG
	printf("MVD_Info Started\n");
	#endif
	
	z=1;
	
	if (loc_loaded == 0){
		mapname = TP_MapName();
		TP_LoadLocFile (mapname, true);
		loc_loaded = 1;
	}

	if (!mvd_info.value)
		return;
	
	if (!cls.mvdplayback)
		return;
	
	x = ELEMENT_X_COORD(mvd_info);
	y = ELEMENT_Y_COORD(mvd_info);

	if (mvd_info_show_header.value){
		strlcpy(mvd_info_header_string, mvd_info_setup.string, sizeof(mvd_info_header_string));
		Replace_In_String(mvd_info_header_string,sizeof(mvd_info_header_string),'%',\
			10,\
			"a","Armor",\
			"f","Frags",\
			"h","Health",\
			"l","Location",\
			"n","Nick",\
			"P","Ping",\
			"p","Powerup",\
			"v","Value",\
			"w","Cur.Weap.",\
			"W","Best Weap.");
		strlcpy(mvd_info_header_string, Make_Red(mvd_info_header_string, 0), sizeof(mvd_info_header_string));
		Draw_String (x, y+((z++)*8), mvd_info_header_string);
	}

	for ( i=0 ; i<mvd_cg_info.pcount ; i++ ){
	

	mvd_info_powerups[0]=0;
	if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_QUAD)
		strlcpy(mvd_info_powerups, tp_name_quad.string, sizeof(mvd_info_powerups));
              
	if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_INVULNERABILITY) {
		if (mvd_info_powerups[0])
			strlcat(mvd_info_powerups, tp_name_separator.string, sizeof(mvd_info_powerups));
		strlcat(mvd_info_powerups, tp_name_pent.string, sizeof(mvd_info_powerups));
	}

	if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_INVISIBILITY) {
		if (mvd_info_powerups[0])
			strlcat(mvd_info_powerups, tp_name_separator.string, sizeof(mvd_info_powerups));
		strlcat(mvd_info_powerups, tp_name_ring.string, sizeof(mvd_info_powerups));
	}


	strlcpy(mvd_info_final_string, mvd_info_setup.string, sizeof(mvd_info_final_string));
    Replace_In_String(mvd_info_final_string,sizeof(mvd_info_final_string),'%',\
			10,\
			"w",va("%s:%i",Weapon_NumToString(mvd_new_info[i].p_info->stats[STAT_ACTIVEWEAPON]),mvd_new_info[i].p_info->stats[STAT_AMMO]),\
			"W",va("%s:%s",MVD_BestWeapon_strings(i),MVD_BestAmmo(i)),\
			"a",va("%i",mvd_new_info[i].p_info->stats[STAT_ARMOR]),\
			"f",va("%i",mvd_new_info[i].p_info->frags),\
			"h",va("%i",mvd_new_info[i].p_info->stats[STAT_HEALTH]),\
			"l",TP_LocationName(mvd_new_info[i].p_state->origin),\
			"n",mvd_new_info[i].p_info->name,\
			"P",va("%i",mvd_new_info[i].p_info->ping),\
			"p",mvd_info_powerups,\
			"v",va("%i",cl_multiview.value ? 0 : mvd_new_info[i].value));
	strlcpy(str, mvd_info_final_string, sizeof(str));
	Draw_String (x, y+((z++)*8), str);
	
	#ifdef DEBUG
	printf("MVD_Info Stopped\n");
	#endif
	}
}


int MVD_FindBestPlayer_f(void){

	int bp_id, status, i, h, y = 0;
	
	char val[80],string1[1024];
	int bp_at,bp_bw,bp_pw,bp_h;
	int lastval = 0;
	
	#ifdef DEBUG
	printf("MVD_FindBestPlayer_f Started\n");
	#endif
	
	if (mvd_cg_info.gametype == 2)
		strlcpy(val, mvd_autotrack_1on1_values.string, sizeof(val));
	else if (mvd_cg_info.gametype == 4)
		strlcpy(val, mvd_autotrack_2on2_values.string, sizeof(val));
	else if (mvd_cg_info.gametype == 8)
		strlcpy(val, mvd_autotrack_4on4_values.string, sizeof(val));
	else if (mvd_autotrack.value == 2)
		strlcpy(val, mvd_autotrack_custom_values.string, sizeof(val));
	else
		strlcpy(val, mvd_autotrack_1on1_values.string, sizeof(val));
	
	
	// checking if mvd_autotrack_xonx_values have right format
	if (strtok(val, " ") != NULL){
		for(y=1;y<=13;y++){
			if (strtok(NULL, " ") != NULL){
			}else{
				//Com_Printf("mvd_autotrack aborting due to wrong use of mvd_autotrack_*_value\n");
				Cvar_SetValue(&mvd_autotrack,0);
				return 0;
			}
		}
	} else {
		//Com_Printf("mvd_autotrack aborting due to wrong use of mvd_autotrack_*_value\n");
		Cvar_SetValue(&mvd_autotrack,0);
	return 0;
	}

	if (mvd_cg_info.gametype == 2)
		strlcpy(val, mvd_autotrack_1on1_values.string, sizeof(val));
	else if (mvd_cg_info.gametype == 4)
		strlcpy(val, mvd_autotrack_2on2_values.string, sizeof(val));
	else if (mvd_cg_info.gametype == 8)
		strlcpy(val, mvd_autotrack_4on4_values.string, sizeof(val));
	else if (mvd_autotrack.value == 2)
		strlcpy(val, mvd_autotrack_custom_values.string, sizeof(val));
	else
		strlcpy(val, mvd_autotrack_1on1_values.string, sizeof(val));

	if (mvd_autotrack.value == 2)
		strlcpy(val, mvd_autotrack_custom_values.string, sizeof(val));
	else if (mvd_autotrack.value == 3 && cl_multiview.value)
		strlcpy(val, multitrack_val, sizeof(val));
	
	// getting values
	//val = mvd_autotrack_1on1_values.string;
	axe_val	= Q_atoi(strtok(val, " "));
	sg_val	= Q_atoi(strtok(NULL, " "));
	ssg_val	= Q_atoi(strtok(NULL, " "));
	ng_val	= Q_atoi(strtok(NULL, " "));
	sng_val	= Q_atoi(strtok(NULL, " "));
	gl_val	= Q_atoi(strtok(NULL, " "));
	rl_val	= Q_atoi(strtok(NULL, " "));
	lg_val	= Q_atoi(strtok(NULL, " "));
	ga_val	= Q_atoi(strtok(NULL, " "));
	ya_val	= Q_atoi(strtok(NULL, " "));
	ra_val	= Q_atoi(strtok(NULL, " "));
	ring_val= Q_atoi(strtok(NULL, " "));
	quad_val= Q_atoi(strtok(NULL, " "));
	pent_val= Q_atoi(strtok(NULL, " "));
	//axe_val=sg_val=ssg_val=ng_val=sng_val=gl_val=rl_val=lg_val=ga_val=ya_val=ra_val=ring_val=quad_val=pent_val=10;
	
	
	for ( i=0; i<mvd_cg_info.pcount ; i++ ){
	
		// get bestweapon
		
		bp_bw = MVD_AutoTrackBW_f(i);	

		// get armortype
		if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_ARMOR1)
			bp_at = ga_val ;
		else if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_ARMOR2)
			bp_at = ya_val ;
		else if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_ARMOR3)
			bp_at = ra_val ;
		else
			bp_at = 0;
		
		// get powerup type
		bp_pw=0;
		if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_QUAD)
			bp_pw += quad_val;
		if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_INVULNERABILITY)
			bp_pw += pent_val;
		if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_INVISIBILITY)
			bp_pw += ring_val;
		
		// get health
		bp_h=mvd_new_info[i].p_info->stats[STAT_HEALTH];
		
	if (mvd_cg_info.pcount == 2)
		strlcpy(string1, mvd_autotrack_1on1.string, sizeof(string1));
	else if (mvd_cg_info.pcount == 4)
		strlcpy(string1,mvd_autotrack_2on2.string, sizeof(string1));
	else if (mvd_cg_info.pcount == 8)
		strlcpy(string1,mvd_autotrack_4on4.string, sizeof(string1));
	else
		strlcpy(string1,mvd_autotrack_1on1.string, sizeof(string1));
		
	if (mvd_autotrack.value == 2)
		strlcpy(string1, mvd_autotrack_custom.string, sizeof(string1));
	else if (mvd_autotrack.value == 3 && cl_multiview.value)
		strlcpy(string1, multitrack_str, sizeof(string1));
		
	//Com_Printf("String1b : %s\n",string1);
	Replace_In_String(string1,sizeof(string1),'%',6,\
		"a",va("%i",mvd_new_info[i].p_info->stats[STAT_ARMOR]),\
		"A",va("%i",bp_at),\
		"f",va("%i",mvd_new_info[i].p_info->frags),\
		"h",va("%i",mvd_new_info[i].p_info->stats[STAT_HEALTH]),\
		"W",va("%i",bp_bw),\
		"p",va("%i",bp_pw));
		
		status=eval_string_int(string1);

		if(status<0){
			Cvar_SetValue(&mvd_autotrack,0);
			return(0);
		}

		mvd_new_info[i].value = atoi(string1);
		
	}

	lastval=0;
	bp_id=0;
	for ( h=0 ; h<mvd_cg_info.pcount ; h++ ){
		if(lastval < mvd_new_info[h].value){
			lastval = mvd_new_info[h].value;
			bp_id 	= mvd_new_info[h].id;
		}
	}
	#ifdef DEBUG
	printf("MVD_FindBestPlayer_f Stopped %i\n",bp_id);
	#endif
	return bp_id ;
}

int MVD_AutoTrackBW_f(int i){
	extern cvar_t tp_weapon_order;
	int j;
	player_info_t *bp_info;
	
	char *t[] = {tp_weapon_order.string, "78654321", NULL}, **s;	

	bp_info = cl.players;

		for (s = t; *s; s++) {
			for (j = 0 ; j < strlen(*s) ; j++) {
				switch ((*s)[j]) {
					case '1': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_AXE) return axe_val; break;
					case '2': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_SHOTGUN) return sg_val; break;
					case '3': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_SUPER_SHOTGUN) return ssg_val; break;
					case '4': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_NAILGUN) return ng_val; break;
					case '5': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_SUPER_NAILGUN) return sng_val; break;
					case '6': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_GRENADE_LAUNCHER) return gl_val; break;
					case '7': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER) return rl_val; break;
					case '8': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_LIGHTNING) return lg_val; break;
				}
			}
		}
	return 0;
}

void MVD_AutoTrack_f(void) {
	char arg[50];
	int id;
	
	#ifdef DEBUG
	printf("MVD_AutoTrack_f Started\n");
	#endif

	if (!mvd_autotrack.value)
		return;

	if (mvd_autotrack.value == 3 && cl_multiview.value > 0){
		if (cl_multiview.value >= 1 ){
			multitrack_str = mvd_multitrack_1.string;
			multitrack_val = mvd_multitrack_1_values.string;
			id = MVD_FindBestPlayer_f();
			if ( id != multitrack_id_1){
				snprintf(arg, sizeof(arg), "track1 #%i \n",id);
				Cbuf_AddText(arg);
				multitrack_id_1 = id;
			}
		}
		if (cl_multiview.value >= 2 ){
			multitrack_str = mvd_multitrack_2.string;
			multitrack_val = mvd_multitrack_2_values.string;
			id = MVD_FindBestPlayer_f();
			if ( id != multitrack_id_2){
				snprintf(arg, sizeof(arg), "track2 #%i \n",id);
				Cbuf_AddText(arg);
				multitrack_id_2 = id;
			}
		}
		if (cl_multiview.value >= 3 ){
			multitrack_str = mvd_multitrack_3.string;
			multitrack_val = mvd_multitrack_3_values.string;
			id = MVD_FindBestPlayer_f();
			if ( id != multitrack_id_3){
				snprintf(arg, sizeof(arg), "track3 #%i \n",id);
				Cbuf_AddText(arg);
				multitrack_id_3 = id;
			}
		}
		if (cl_multiview.value >= 4 ){
			multitrack_str = mvd_multitrack_4.string;
	 		multitrack_val = mvd_multitrack_4_values.string;
			id = MVD_FindBestPlayer_f();
			if ( id != multitrack_id_4){
				snprintf(arg, sizeof(arg), "track4 #%i \n",id);
				Cbuf_AddText(arg);
				multitrack_id_4 = id;
			}
		}

	}else{
	
		id = MVD_FindBestPlayer_f();
		if ( id != last_track){
			snprintf(arg, sizeof(arg), "track #%i \n",id);
			Cbuf_AddText(arg);
			last_track = id;
		}
	}
	#ifdef DEBUG
	printf("MVD_AutoTrack_f Stopped\n");
	#endif
}

void MVD_Status_Announcer_f (int i, int z){
	char *pn;
	vec3_t *pl;
	pn=mvd_new_info[i].p_info->name;
	pl=&mvd_new_info[i].p_state->origin;
	if (mvd_new_info[i].info.info[z].mention==1){
		mvd_new_info[i].info.info[z].mention = 0;
		switch (z){
			case 2: Com_Printf("%s Took %s @ %s\n",pn, tp_name_ssg.string,TP_LocationName(*pl));break;
			case 3: Com_Printf("%s Took %s @ %s\n",pn, tp_name_ng.string,TP_LocationName(*pl));break;
			case 4: Com_Printf("%s Took %s @ %s\n",pn, tp_name_sng.string,TP_LocationName(*pl));break;
			case 5: Com_Printf("%s Took %s @ %s\n",pn, tp_name_gl.string,TP_LocationName(*pl));break;
			case 6: Com_Printf("%s Took %s @ %s\n",pn, tp_name_rl.string,TP_LocationName(*pl));break;
			case 7: Com_Printf("%s Took %s @ %s\n",pn, tp_name_lg.string,TP_LocationName(*pl));break;
			case 8: Com_Printf("%s Took %s @ %s\n",pn, tp_name_ring.string,TP_LocationName(*pl));break;
			case 9: Com_Printf("%s Took %s @ %s\n",pn, tp_name_quad.string,TP_LocationName(*pl));break;
			case 10: Com_Printf("%s Took %s @ %s\n",pn, tp_name_pent.string,TP_LocationName(*pl));break;
			case 11: Com_Printf("%s Took %s @ %s\n",pn, tp_name_ga.string,TP_LocationName(*pl));break;
			case 12: Com_Printf("%s Took %s @ %s\n",pn, tp_name_ya.string,TP_LocationName(*pl));break;
			case 13: Com_Printf("%s Took %s @ %s\n",pn, tp_name_ra.string,TP_LocationName(*pl));break;
			case 14: Com_Printf("%s Took %s @ %s\n",pn, tp_name_mh.string,TP_LocationName(*pl));break;
		}
		return;
	}else if (mvd_new_info[i].info.info[z].mention==-1){
		mvd_new_info[i].info.info[z].mention = 0;
		switch (z){
			case 5: Com_Printf("%s Lost %s @ %s\n",pn, tp_name_gl.string,TP_LocationName(*pl));break;
			case 6: Com_Printf("%s Lost %s @ %s\n",pn, tp_name_rl.string,TP_LocationName(*pl));break;
			case 7: Com_Printf("%s Lost %s @ %s\n",pn, tp_name_lg.string,TP_LocationName(*pl));break;
			case 8: Com_Printf("%s Lost %s @ %s\n",pn, tp_name_ring.string,TP_LocationName(*pl));break;
			case 9: Com_Printf("%s Lost %s @ %s\n",pn, tp_name_quad.string,TP_LocationName(*pl));break;
			case 10: 
				if (mvd_new_info[i].info.info[QUAD_INFO].starttime - cls.demotime < 30){
					Com_Printf("%s Lost %s @ %s\n",pn, tp_name_pent.string,TP_LocationName(*pl));break;
				}else{
					Com_Printf("%s's %s ended\n",pn, tp_name_pent.string,TP_LocationName(*pl));break;
				}
			case 11: Com_Printf("%s Lost %s @ %s\n",pn, tp_name_ga.string,TP_LocationName(*pl));break;
			case 12: Com_Printf("%s Lost %s @ %s\n",pn, tp_name_ya.string,TP_LocationName(*pl));break;
			case 13: Com_Printf("%s Lost %s @ %s\n",pn, tp_name_ra.string,TP_LocationName(*pl));break;
			case 14: Com_Printf("%s Lost %s @ %s\n",pn, tp_name_mh.string,TP_LocationName(*pl));break;
		}
	return;
	}

}

void MVD_Status_WP_f (int i){
	int j,k;
	for (k=j=2;j<8;j++){
		if (!mvd_new_info[i].info.info[j].has && mvd_new_info[i].p_info->stats[STAT_ITEMS] & k){
			mvd_new_info[i].info.info[j].mention = 1;
			mvd_new_info[i].info.info[j].has = 1;
			mvd_new_info[i].info.info[j].count++;
		}
	k=k*2;
	}

}

void MVD_Stats_Cleanup_f (void){
	got_game_infos=0;
}

void MVD_Get_Game_Infos_f (void){

}

int MVD_Stats_Gather_f (void){
	int death_stats = 0;
	int x,i,z,killdiff;

	MVD_Init_Info_f();

	if(cl.countdown == true)
		return 0;
	if(cl.standby == true)
		return 0;
	//if 

	for ( i=0; i<mvd_cg_info.pcount ; i++ ){
		
		if (mvd_new_info[i].info.firstrun == 0){
			mvd_new_info[i].info.das.alivetimestart = cls.demotime;
			gamestart_time = cls.demotime;
			mvd_new_info[i].info.firstrun = 1;
			mvd_new_info[i].info.lfw = -1;
		}
		// death alive stats
		if (mvd_new_info[i].p_info->stats[STAT_HEALTH]>0 && mvd_new_info[i].info.das.isdead == 1){
			mvd_new_info[i].info.das.isdead = 0;
			mvd_new_info[i].info.das.alivetimestart = cls.demotime;
			mvd_new_info[i].info.lfw = -1;
		}
		
		mvd_new_info[i].info.das.alivetime = cls.demotime - mvd_new_info[i].info.das.alivetimestart;
		if (mvd_new_info[i].p_info->stats[STAT_HEALTH]<=0 && mvd_new_info[i].info.das.isdead != 1){
			mvd_new_info[i].info.das.isdead = 1;
			mvd_new_info[i].info.das.deathcount++;
			death_stats=1;
		}
			
		if (death_stats){
			death_stats=0;
			mvd_new_info[i].info.run++;
			mvd_new_info[i].info.lfw = -1;
			
			for(x=0;x<13;x++){
				
				if (x == MVD_Weapon_LWF(mvd_new_info[i].info.lfw)){
						mvd_new_info[i].info.info[x].mention=-1;
						mvd_new_info[i].info.info[x].lost++;
				}
				
				if (x == QUAD_INFO && mvd_new_info[i].info.info[QUAD_INFO].has){
					if (mvd_new_info[i].info.info[x].starttime - cls.demotime < 30 )
						mvd_new_info[i].info.info[x].run++;
						mvd_new_info[i].info.info[x].lost++;
				}
				mvd_new_info[i].info.info[x].has=0;
			}
		}
		if(!mvd_new_info[i].info.das.isdead){

		

			
		if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_ARMOR1){
			if (mvd_new_info[i].info.info[GA_INFO].has == 0){
				mvd_new_info[i].info.info[GA_INFO].has = 1;
				mvd_new_info[i].info.info[GA_INFO].count++;
				mvd_new_info[i].info.info[GA_INFO].lost=mvd_new_info[i].p_info->stats[STAT_ARMOR];
				mvd_new_info[i].info.info[YA_INFO].has = 0;
				mvd_new_info[i].info.info[RA_INFO].has = 0;
				mvd_new_info[i].info.info[GA_INFO].mention = 1;
			}
			if (mvd_new_info[i].info.info[GA_INFO].lost < mvd_new_info[i].p_info->stats[STAT_ARMOR]){
				mvd_new_info[i].info.info[GA_INFO].count++;
				mvd_new_info[i].info.info[GA_INFO].mention = 1;
			}
			mvd_new_info[i].info.info[GA_INFO].lost=mvd_new_info[i].p_info->stats[STAT_ARMOR];
		}else if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_ARMOR2){
			if (mvd_new_info[i].info.info[YA_INFO].has == 0){
				mvd_new_info[i].info.info[YA_INFO].has = 1;
				mvd_new_info[i].info.info[YA_INFO].count++;
				mvd_new_info[i].info.info[YA_INFO].lost=mvd_new_info[i].p_info->stats[STAT_ARMOR];
				mvd_new_info[i].info.info[GA_INFO].has = 0;
				mvd_new_info[i].info.info[RA_INFO].has = 0;
				mvd_new_info[i].info.info[YA_INFO].mention = 1;
			}
			if (mvd_new_info[i].info.info[YA_INFO].lost < mvd_new_info[i].p_info->stats[STAT_ARMOR]){
				mvd_new_info[i].info.info[YA_INFO].count++;
				mvd_new_info[i].info.info[YA_INFO].mention = 1;
			}
			mvd_new_info[i].info.info[YA_INFO].lost=mvd_new_info[i].p_info->stats[STAT_ARMOR];
		}else if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_ARMOR3){
			if (mvd_new_info[i].info.info[RA_INFO].has == 0){
				mvd_new_info[i].info.info[RA_INFO].has = 1;
				mvd_new_info[i].info.info[RA_INFO].count++;
				mvd_new_info[i].info.info[RA_INFO].lost=mvd_new_info[i].p_info->stats[STAT_ARMOR];
				mvd_new_info[i].info.info[GA_INFO].has = 0;
				mvd_new_info[i].info.info[YA_INFO].has = 0;
				mvd_new_info[i].info.info[RA_INFO].mention = 1;
			}
			if (mvd_new_info[i].info.info[RA_INFO].lost < mvd_new_info[i].p_info->stats[STAT_ARMOR]){
				mvd_new_info[i].info.info[RA_INFO].count++;
				mvd_new_info[i].info.info[RA_INFO].mention = 1;
			}
			mvd_new_info[i].info.info[RA_INFO].lost=mvd_new_info[i].p_info->stats[STAT_ARMOR];
		}
		
		if (!mvd_new_info[i].info.info[MH_INFO].has && mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_SUPERHEALTH){
			mvd_new_info[i].info.info[MH_INFO].mention = 1;
			mvd_new_info[i].info.info[MH_INFO].has = 1;
			mvd_new_info[i].info.info[MH_INFO].count++;
		}
		if (mvd_new_info[i].info.info[MH_INFO].has && !(mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_SUPERHEALTH))
			mvd_new_info[i].info.info[MH_INFO].has = 0;

		if (!mvd_new_info[i].info.info[RING_INFO].has && mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_INVISIBILITY){
			mvd_new_info[i].info.info[RING_INFO].mention = 1;
			mvd_new_info[i].info.info[RING_INFO].has = 1;
			mvd_new_info[i].info.info[QUAD_INFO].starttime = cls.demotime;
			mvd_new_info[i].info.info[RING_INFO].count++;
		}
		if (mvd_new_info[i].info.info[RING_INFO].has && !(mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_INVISIBILITY))
			mvd_new_info[i].info.info[RING_INFO].has = 0;

		if (!mvd_new_info[i].info.info[QUAD_INFO].has && mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_QUAD){
			mvd_new_info[i].info.info[QUAD_INFO].mention = 1;
			mvd_new_info[i].info.info[QUAD_INFO].has = 1;
			mvd_new_info[i].info.info[QUAD_INFO].starttime = cls.demotime;
			mvd_new_info[i].info.info[QUAD_INFO].count++;
		}
		if (mvd_new_info[i].info.info[QUAD_INFO].has && !(mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_QUAD)){
			mvd_new_info[i].info.info[QUAD_INFO].has = 0;
			mvd_new_info[i].info.info[QUAD_INFO].runs[mvd_new_info[i].info.info[QUAD_INFO].run].time = cls.demotime - mvd_new_info[i].info.info[QUAD_INFO].starttime;
			mvd_new_info[i].info.info[QUAD_INFO].run++;
		}

		if (!mvd_new_info[i].info.info[PENT_INFO].has && mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_INVULNERABILITY){
			mvd_new_info[i].info.info[PENT_INFO].mention = 1;
			mvd_new_info[i].info.info[PENT_INFO].has = 1;
			mvd_new_info[i].info.info[PENT_INFO].starttime = cls.demotime;
			mvd_new_info[i].info.info[PENT_INFO].count++;
		}
		if (mvd_new_info[i].info.info[PENT_INFO].has && !(mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_INVULNERABILITY)){
			mvd_new_info[i].info.info[PENT_INFO].has = 0;
			
			mvd_new_info[i].info.info[PENT_INFO].run++;
		}
		for (z=8;z<11;z++){
			if (mvd_new_info[i].info.info[z].has == 1)
				mvd_new_info[i].info.info[z].runs[mvd_new_info[i].info.info[z].run].time = cls.demotime - mvd_new_info[i].info.info[z].starttime;
		}
		
		if (mvd_new_info[i].info.lastfrags != mvd_new_info[i].p_info->frags ){
			if (mvd_new_info[i].info.lastfrags < mvd_new_info[i].p_info->frags){
				killdiff = mvd_new_info[i].p_info->frags - mvd_new_info[i].info.lastfrags;
				for (z=0;z<8;z++){
					if (z == MVD_Weapon_LWF(mvd_new_info[i].info.lfw))
							mvd_new_info[i].info.killstats.normal[z].kills+=killdiff;
				}
				if (mvd_new_info[i].info.lfw == -1)
					mvd_new_info[i].info.spawntelefrags+=killdiff;
				for(z=8;z<11;z++){
					if(mvd_new_info[i].info.info[z].has)
						mvd_new_info[i].info.info[z].runs[mvd_new_info[i].info.info[z].run].frags+=killdiff;
				}
				mvd_new_info[i].info.runs[mvd_new_info[i].info.run].frags++;
				}else if (mvd_new_info[i].info.lastfrags > mvd_new_info[i].p_info->frags){
				killdiff = mvd_new_info[i].info.lastfrags - mvd_new_info[i].p_info->frags ;
				for (z=0;z<8;z++){
					if (z == MVD_Weapon_LWF(mvd_new_info[i].info.lfw))
						mvd_new_info[i].info.killstats.normal[z].teamkills+=killdiff;
				}
				if (mvd_new_info[i].info.lfw == -1){
					mvd_new_info[i].info.teamspawntelefrags+=killdiff;

				}
				for(z=8;z<11;z++){
					if(mvd_new_info[i].info.info[z].has)
						mvd_new_info[i].info.info[z].runs[mvd_new_info[i].info.info[z].run].teamfrags+=killdiff;
				}
				mvd_new_info[i].info.runs[mvd_new_info[i].info.run].teamfrags++;
				}
		

			mvd_new_info[i].info.lastfrags = mvd_new_info[i].p_info->frags ;
		}

		mvd_new_info[i].info.runs[mvd_new_info[i].info.run].time=cls.demotime - mvd_new_info[i].info.das.alivetimestart;

		if (mvd_new_info[i].info.lfw == -1){
			if (mvd_new_info[i].info.lastfrags > mvd_new_info[i].p_info->frags ){
				mvd_new_info[i].info.teamspawntelefrags += mvd_new_info[i].p_info->frags - mvd_new_info[i].info.lastfrags ;
			}else if (mvd_new_info[i].info.lastfrags < mvd_new_info[i].p_info->frags ){
				mvd_new_info[i].info.spawntelefrags += mvd_new_info[i].p_info->frags -mvd_new_info[i].info.lastfrags  ;
			}
			mvd_new_info[i].info.lastfrags = mvd_new_info[i].p_info->frags;
		}

		if (mvd_new_info[i].p_info->name > 0 && mvd_new_info[i].p_state->weaponframe > 0)
				mvd_new_info[i].info.lfw=mvd_new_info[i].p_info->stats[STAT_ACTIVEWEAPON];

		MVD_Status_WP_f(i);

		if (mvd_moreinfo.value)
			for (z=0;z<13;z++)
				MVD_Status_Announcer_f(i,z);
		}

	}	
	
	return 1;

}


void MVD_List_Runs_f (void){
	int i;
	
	for (i=0;i<MAX_MODELS;i++)
		Com_Printf("%s\n",cl.model_name[i]);

	/*
	player_info_t *mvd_stat;
	mvd_stat = cl.players ;
	if (!mvd_new_info[id].info.run)
		return;
		
	Com_Printf("Run stats for %s\n",mvd_stat[spec_track].name);
	for (x=0;x<mvd_new_info[id].info.run;x++){
		Com_Printf ("%2i. Time: %-8.3f Frags: %3i\n",x+1,mvd_new_info[id].info.runs[x].time,mvd_new_info[id].info.runs[x].frags);
	}
	*/
}

void MVD_Status (void){
	int x, y,p ;
	char str[1024];
	int id = 0,i;
	int z = 0;
	double av_f  = 0.0;
	double av_t  = 0.0;
	double av_tk = 0.0;

		
	if (!mvd_status.value)
		return;
	
	if (!cls.mvdplayback)
		return;

	for (i=0;i<mvd_cg_info.pcount;i++)
		if (mvd_new_info[i].id == spec_track)
			id = i;
	
	x = ELEMENT_X_COORD(mvd_status);
	y = ELEMENT_Y_COORD(mvd_status);
	strcpy(str,mvd_new_info[id].p_info->name);
	Draw_ColoredString (x, y+((z++)*8), str,1);
	strcpy(str,"&cf40Took");
	
	Draw_ColoredString (x, y+((z++)*8), str,1);
	
	strcpy(str,va("RL: %i LG: %i GL: %i RA: %i YA: %i GA:%i",\
		mvd_new_info[id].info.info[RL_INFO].count,\
		mvd_new_info[id].info.info[LG_INFO].count,\
		mvd_new_info[id].info.info[GL_INFO].count,\
		mvd_new_info[id].info.info[RA_INFO].count,\
		mvd_new_info[id].info.info[YA_INFO].count,\
		mvd_new_info[id].info.info[RA_INFO].count));
	strcpy(str,Make_Red(str,1));
	Draw_ColoredString (x, y+((z++)*8),str,1); 
    strcpy(str,va("Ring: %i Quad: %i Pent: %i MH: %i",\
		mvd_new_info[id].info.info[RING_INFO].count,\
		mvd_new_info[id].info.info[QUAD_INFO].count,\
		mvd_new_info[id].info.info[PENT_INFO].count,\
		mvd_new_info[id].info.info[MH_INFO].count));
	strcpy(str,Make_Red(str,1));
	Draw_ColoredString (x, y+((z++)*8),str,1);
	
//	Com_Printf("%f %f %f \n",lasttime,mvd_new_info[id].info.das.alivetimestart, mvd_new_info[id].info.das.alivetime);
	if (cls.demotime >+ lasttime + .1){
		lasttime=cls.demotime;
		lasttime1=mvd_new_info[id].info.das.alivetime;
	}
		
	strcpy(str,va("Deaths: %i",mvd_new_info[id].info.das.deathcount));
	strcpy(str,Make_Red(str,1));
	Draw_ColoredString (x, y+((z++)*8),str,1);

	strcpy(str,"Average Run:");
	Draw_ColoredString (x, y+((z++)*8),str,1);

	strcpy(str,"Time      Frags TKS");
	strcpy(str,Make_Red(str,1));
	Draw_ColoredString (x, y+((z++)*8),str,1);

	for (p=0;p<=mvd_new_info[id].info.run;p++){
		av_t += mvd_new_info[id].info.runs[p].time;
		av_f += mvd_new_info[id].info.runs[p].frags;
		av_tk += mvd_new_info[id].info.runs[p].teamfrags;
	}
	if (av_t>0){
	av_t = av_t / (mvd_new_info[id].info.run +1);
	av_f = av_f / (mvd_new_info[id].info.run +1);
	av_tk = av_tk / (mvd_new_info[id].info.run +1);
	}

	strcpy(str,va("%9.3f %3.3f %3.3f",av_t,av_f,av_tk));
	Draw_ColoredString (x, y+((z++)*8),str,1);

	
	strcpy(str,"Last 3 Runs:");
	Draw_ColoredString (x, y+((z++)*8),str,1);

	strcpy(str,"No. Time      Frags TKS");
	strcpy(str,Make_Red(str,1));
	Draw_ColoredString (x, y+((z++)*8),str,1);

	p=mvd_new_info[id].info.run-3;
	if (p<0)
		p=0;
	//&& mvd_new_info[id].info.runs[p].time
	while(p <= mvd_new_info[id].info.run) {
		strcpy(str,va("%3i %9.3f %5i %3i",p+1,mvd_new_info[id].info.runs[p].time,mvd_new_info[id].info.runs[p].frags,mvd_new_info[id].info.runs[p].teamfrags));
		Draw_ColoredString (x, y+((z++)*8),str,1);
		p++;
	}
	strcpy(str,va("Last Fired Weapon: %s",MVD_Weapon_strings(mvd_new_info[id].info.lfw)));
	strcpy(str,Make_Red(str,1));
	Draw_ColoredString (x, y+((z++)*8),str,1);
	
	strcpy(str,"&cf40Lost");
	Draw_ColoredString (x, y+((z++)*8), str,1);
	
	strcpy(str,va("RL: %i LG: %i GL: %i QUAD: %i",\
		mvd_new_info[id].info.info[RL_INFO].lost,\
		mvd_new_info[id].info.info[LG_INFO].lost,\
		mvd_new_info[id].info.info[GL_INFO].lost,\
		mvd_new_info[id].info.info[QUAD_INFO].lost));
	strcpy(str,Make_Red(str,1));
	Draw_ColoredString (x, y+((z++)*8),str,1); 

	strcpy(str,"&cf40Kills");
	Draw_ColoredString (x, y+((z++)*8), str,1);

	strcpy(str,va("RL: %i LG: %i GL: %i SNG: %i NG: %i SSG: %i SG: %i AXE: %i",\
		mvd_new_info[id].info.killstats.normal[RL_INFO].kills,\
		mvd_new_info[id].info.killstats.normal[LG_INFO].kills,\
		mvd_new_info[id].info.killstats.normal[GL_INFO].kills,\
		mvd_new_info[id].info.killstats.normal[SNG_INFO].kills,\
		mvd_new_info[id].info.killstats.normal[NG_INFO].kills,\
		mvd_new_info[id].info.killstats.normal[SSG_INFO].kills,\
		mvd_new_info[id].info.killstats.normal[SG_INFO].kills,\
		mvd_new_info[id].info.killstats.normal[AXE_INFO].kills));
	strcpy(str,Make_Red(str,1));
	Draw_ColoredString (x, y+((z++)*8),str,1);

	strcpy(str,va("SPAWN: %i",\
		mvd_new_info[id].info.spawntelefrags));
	strcpy(str,Make_Red(str,1));
	Draw_ColoredString (x, y+((z++)*8),str,1);

	strcpy(str,"&cf40Teamkills");
	Draw_ColoredString (x, y+((z++)*8), str,1);

	strcpy(str,va("RL: %i LG: %i GL: %i SNG: %i NG: %i SSG: %i SG: %i AXE: %i",\
		mvd_new_info[id].info.killstats.normal[RL_INFO].teamkills,\
		mvd_new_info[id].info.killstats.normal[LG_INFO].teamkills,\
		mvd_new_info[id].info.killstats.normal[GL_INFO].teamkills,\
		mvd_new_info[id].info.killstats.normal[SNG_INFO].teamkills,\
		mvd_new_info[id].info.killstats.normal[NG_INFO].teamkills,\
		mvd_new_info[id].info.killstats.normal[SSG_INFO].teamkills,\
		mvd_new_info[id].info.killstats.normal[SG_INFO].teamkills,\
		mvd_new_info[id].info.killstats.normal[AXE_INFO].teamkills));
	strcpy(str,Make_Red(str,1));
	Draw_ColoredString (x, y+((z++)*8),str,1); 	
	strcpy(str,va("SPAWN: %i",\
		mvd_new_info[id].info.teamspawntelefrags));
	strcpy(str,Make_Red(str,1));
	Draw_ColoredString (x, y+((z++)*8),str,1);

	strcpy(str,"Last 3 Quad Runs:");
	Draw_ColoredString (x, y+((z++)*8),str,1);

	strcpy(str,"No. Time      Frags TKS");
	strcpy(str,Make_Red(str,0));
	Draw_ColoredString (x, y+((z++)*8),str,1);

	p=mvd_new_info[id].info.info[QUAD_INFO].run-3;
	if (p<0)
		p=0;

	while(p <= mvd_new_info[id].info.info[QUAD_INFO].run && mvd_new_info[id].info.info[QUAD_INFO].runs[p].time){
		strcpy(str,va("%3i %9.3f %5i %3i",p+1,mvd_new_info[id].info.info[QUAD_INFO].runs[p].time,mvd_new_info[id].info.info[QUAD_INFO].runs[p].frags,mvd_new_info[id].info.info[QUAD_INFO].runs[p].teamfrags));
		Draw_ColoredString (x, y+((z++)*8),str,1);
		p++;
	}

	
}


char *mvd_name_to_xml(char *s){
	static char buf[1024];
	char *p;
	int i;
	buf[0]=0;
	for(p=s;*p;p++){
		i=(int)*p;
		if(i<0)
			i+=256;
		if (strlen(buf)>0 ){
			sprintf(buf,"%s",va("%s			<char>%i</char>\n",buf,i));
		}else
			sprintf(buf,"%s",va("\n			<char>%i</char>\n",i));
	}
	return buf;

}




void mvd_s_h (FILE *f){
	fprintf(f,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	fprintf(f,"<?xml-stylesheet type=\"text/xsl\" href=\"mvdstats.xsl\"?>\n");
	fprintf(f,"<mvdstats>\n");
	fprintf(f,"\n");
	fprintf(f,"\n");
	fprintf(f,"<demoinfos>\n");
	fprintf(f,"		<map>%s</map>\n",mvd_cg_info.mapname);
	fprintf(f,"		<gametype>%s</gametype>\n",mvd_gt_info[mvd_cg_info.gametype].name);
	fprintf(f,"		<hostname>%s</hostname>\n",mvd_cg_info.hostname);
	if (mvd_cg_info.gametype != 0 && mvd_cg_info.gametype != 4){
		fprintf(f,"		<team1>%s</team1>\n",mvd_name_to_xml(mvd_cg_info.team1));
		fprintf(f,"		<team2>%s</team2>\n",mvd_name_to_xml(mvd_cg_info.team2));
	}
	fprintf(f,"		<timelimit>%i</timelimit>\n",mvd_cg_info.timelimit);
	

	
	
	fprintf(f,"</demoinfos>\n");
}

void mvd_s_e (FILE *f){
	fprintf(f,"</mvdstats>\n");
	
}




void mvd_s_p (FILE *f,int i,int k){
	int x,y,z;
	
		fprintf(f,"	<player id=\"%i\">\n",k);
		fprintf(f,"		<nick>%s		</nick>\n",mvd_name_to_xml(mvd_new_info[i].p_info->name));
		if (mvd_cg_info.gametype != 0 && mvd_cg_info.gametype != 4)
			fprintf(f,"		<team>%s</team>\n",mvd_name_to_xml(mvd_new_info[i].p_info->team));
		fprintf(f,"		<kills>\n");
		for (z=0,x=AXE_INFO;x<=LG_INFO;x++){
			fprintf(f,"			<%s>%i</%s>\n",mvd_wp_info[x].name,mvd_new_info[i].info.killstats.normal[x].kills,mvd_wp_info[x].name);
			z+=mvd_new_info[i].info.killstats.normal[x].kills;
		}
		fprintf(f,"			<spawn>%i</spawn>\n",mvd_new_info[i].info.spawntelefrags);
		z+=mvd_new_info[i].info.spawntelefrags;
			fprintf(f,"			<all>%i</all>\n",z);
		fprintf(f,"		</kills>\n");
		fprintf(f,"		<teamkills>\n");
		for (z=0,x=AXE_INFO;x<=LG_INFO;x++){
			fprintf(f,"			<%s>%i</%s>\n",mvd_wp_info[x].name,mvd_new_info[i].info.killstats.normal[x].teamkills,mvd_wp_info[x].name);
			z+=mvd_new_info[i].info.killstats.normal[x].teamkills;
		}
		fprintf(f,"			<spawn>%i</spawn>\n",mvd_new_info[i].info.teamspawntelefrags);
		z+=mvd_new_info[i].info.teamspawntelefrags;
			fprintf(f,"			<all>%i</all>\n",z);
	
		fprintf(f,"		</teamkills>\n");
		fprintf(f,"		<deaths>%i</deaths>\n",mvd_new_info[i].info.das.deathcount);
		fprintf(f,"		<took>\n");
		for (x=SSG_INFO;x<=MH_INFO;x++){
			fprintf(f,"			<%s>%i</%s>\n",mvd_wp_info[x].name,mvd_new_info[i].info.info[x].count,mvd_wp_info[x].name);
		}
		fprintf(f,"		</took>\n");
		fprintf(f,"		<lost>\n");
		for (x=SSG_INFO;x<=MH_INFO;x++){
			fprintf(f,"			<%s>%i</%s>\n",mvd_wp_info[x].name,mvd_new_info[i].info.info[x].lost,mvd_wp_info[x].name);
		}
		fprintf(f,"		</lost>\n");

		fprintf(f,"	<runs>\n");
		for(x=0;x<mvd_new_info[i].info.run;x++){
			fprintf(f,"		<run id=\"%i\">\n",x);
			fprintf(f,"			<time>%9.3f</time>\n",mvd_new_info[i].info.runs[x].time);
			fprintf(f,"			<frags>%i</frags>\n",mvd_new_info[i].info.runs[x].frags);
			fprintf(f,"			<teamfrags>%i</teamfrags>\n",mvd_new_info[i].info.runs[x].teamfrags);;
			fprintf(f,"		</run>\n");
		}
		fprintf(f,"	</runs>\n");
		
		for(y=RING_INFO;y<=PENT_INFO ;y++){
			if (mvd_new_info[i].info.info[y].run == 0)
				continue;
			fprintf(f,"	<%s_runs>\n",mvd_wp_info[y].name);

			for(x=0;x<mvd_new_info[i].info.info[y].run;x++){
			fprintf(f,"		<run id=\"%i\">\n",x);
			fprintf(f,"			<time>%9.3f</time>\n",mvd_new_info[i].info.info[y].runs[x].time);
			fprintf(f,"			<frags>%i</frags>\n",mvd_new_info[i].info.info[y].runs[x].frags);
			fprintf(f,"			<teamfrags>%i</teamfrags>\n",mvd_new_info[i].info.info[y].runs[x].teamfrags);;
			fprintf(f,"		</run>\n");
			}
			fprintf(f,"	</%s_runs>\n",mvd_wp_info[y].name);
		}
		
		fprintf(f,"	</player>\n\n");


		
	

}

void mvd_s_t (FILE *f){
	int i,z;
	int static count;

	if (mvd_cg_info.gametype == 0 || mvd_cg_info.gametype == 4)
		return;
	fprintf(f,"<Teamstats>\n");

	fprintf(f,"		<team1>\n");
	fprintf(f,"			<name>%s</name>\n",mvd_name_to_xml(mvd_cg_info.team1));
	fprintf(f,"			<players>\n");
	for (i = 0,z=0; i < mvd_cg_info.pcount; i++) {
		if (strcmp(mvd_new_info[i].p_info->team,mvd_cg_info.team1))
			continue;
		mvd_s_p(f,i,z);
		z++;
	}
	
	fprintf(f,"			</players>\n");

	fprintf(f,"		<took>\n");
	for(z=SSG_INFO;z<=MH_INFO;z++){
		for (i = 0; i < mvd_cg_info.gametype; i++) {
			if (strcmp(mvd_new_info[i].p_info->team,mvd_cg_info.team1))
				continue;
			count+=mvd_new_info[i].info.info[z].count;
			
		}
		fprintf(f,"			<%s>%i</%s>\n",mvd_wp_info[z].name,count,mvd_wp_info[z].name);
		count=0;
	}
	fprintf(f,"		</took>\n");
	
	fprintf(f,"		<lost>\n");
	for(z=SSG_INFO;z<=QUAD_INFO;z++){
		for (i = 0; i < mvd_cg_info.pcount; i++) {
			if (!strcmp(mvd_new_info[i].p_info->team,mvd_cg_info.team1))
				continue;
			count+=mvd_new_info[i].info.info[z].lost;
			
		}
		fprintf(f,"			<%s>%i</%s>\n",mvd_wp_info[z].name,count,mvd_wp_info[z].name);
		count=0;
	}

	fprintf(f,"		</lost>\n");
	
	fprintf(f,"		<kills>\n");
	for(z=AXE_INFO;z<=PENT_INFO;z++){
		for (i = 0; i < mvd_cg_info.pcount; i++) {
			if (!strcmp(mvd_new_info[i].p_info->team,mvd_cg_info.team1))
				continue;
			count+=mvd_new_info[i].info.killstats.normal[z].kills;
			
		}
		fprintf(f,"			<%s>%i</%s>\n",mvd_wp_info[z].name,count,mvd_wp_info[z].name);
		count=0;
	}

	fprintf(f,"		</kills>\n");
	
	fprintf(f,"		</team1>\n");

	fprintf(f,"		<team2>\n");
		fprintf(f,"			<name>%s</name>\n",mvd_name_to_xml(mvd_cg_info.team2));
		fprintf(f,"			<players>\n");
		for (i = 0,z=0; i < mvd_cg_info.pcount; i++) {
			if (!strcmp(mvd_new_info[i].p_info->team,mvd_cg_info.team2))
				continue;
			mvd_s_p(f,i,z);
			z++;
		}
		fprintf(f,"			</players>\n");

		fprintf(f,"		<took>\n");
		for(z=SSG_INFO;z<=MH_INFO;z++){
			for (i = 0; i < mvd_cg_info.gametype; i++) {
				if (!strcmp(mvd_new_info[i].p_info->team,mvd_cg_info.team2))
					continue;
				count+=mvd_new_info[i].info.info[z].count;
				
			}
			fprintf(f,"			<%s>%i</%s>\n",mvd_wp_info[z].name,count,mvd_wp_info[z].name);
			count=0;
		}
		fprintf(f,"		</took>\n");

		fprintf(f,"		<lost>\n");
		for(z=SSG_INFO;z<=QUAD_INFO;z++){
			for (i = 0; i < mvd_cg_info.pcount; i++) {
				if (!strcmp(mvd_new_info[i].p_info->team,mvd_cg_info.team2))
					continue;
				count+=mvd_new_info[i].info.info[z].lost;
				
			}
			fprintf(f,"			<%s>%i</%s>\n",mvd_wp_info[z].name,count,mvd_wp_info[z].name);
			count=0;
		}

		fprintf(f,"		</lost>\n");

		fprintf(f,"		<kills>\n");
		for(z=AXE_INFO;z<=PENT_INFO;z++){
			for (i = 0; i < mvd_cg_info.pcount; i++) {
				if (!strcmp(mvd_new_info[i].p_info->team,mvd_cg_info.team2))
					continue;
				count+=mvd_new_info[i].info.killstats.normal[z].kills;
				
			}
			fprintf(f,"			<%s>%i</%s>\n",mvd_wp_info[z].name,count,mvd_wp_info[z].name);
			count=0;
		}

		fprintf(f,"		</kills>\n");

		fprintf(f,"		</team2>\n");

	
	fprintf(f,"</Teamstats>\n");
	
}

void MVD_Status_Xml (void){
	FILE *f;
	int i;
	if (!mvd_write_xml.value)
		return;
	
	Com_Printf("%s\n",mvd_status_xml_file);
	f=fopen(mvd_status_xml_file,"wb");
	mvd_s_h(f);
	if (mvd_cg_info.gametype!=0 && mvd_cg_info.gametype!=4 ){
		mvd_s_t(f);
	}else{
		for (i=0;i<mvd_cg_info.pcount;i++)
			mvd_s_p(f,i,i);
	}
	mvd_s_e(f);
	fclose(f);
}

void MVD_Testor_f (void) {
	extern qbool Match_Running;
	Com_Printf("%i\n",Match_Running);
	Com_Printf("%s\n",mvd_name_to_xml("01234567890"));
	Com_Printf("%s : %s \n",mvd_cg_info.team1,mvd_cg_info.team2);
}

void MVD_Mainhook_f (void){
	MVD_Stats_Gather_f();
//	MVD_Notice_f();
	MVD_AutoTrack_f ();
	if (cls.mvdplayback && mvd_demo_track_run == 0) 
	MVD_Demo_Track ();
}

void MVD_Utils_Init (void) {
	Cvar_SetCurrentGroup(CVAR_GROUP_MVD);
	Cvar_Register (&mvd_info);
	Cvar_Register (&mvd_info_show_header);
	Cvar_Register (&mvd_info_setup);
	Cvar_Register (&mvd_info_x);
	Cvar_Register (&mvd_info_y);
	Cvar_Register (&mvd_moreinfo);
	Cvar_Register (&mvd_autotrack);
	Cvar_Register (&mvd_autotrack_1on1);
	Cvar_Register (&mvd_autotrack_1on1_values);
	Cvar_Register (&mvd_autotrack_2on2);
	Cvar_Register (&mvd_autotrack_2on2_values);
	Cvar_Register (&mvd_autotrack_4on4);
	Cvar_Register (&mvd_autotrack_4on4_values);
	Cvar_Register (&mvd_autotrack_custom);
	Cvar_Register (&mvd_autotrack_custom_values);

	Cvar_Register (&mvd_multitrack_1);
	Cvar_Register (&mvd_multitrack_1_values);
	Cvar_Register (&mvd_multitrack_2);
	Cvar_Register (&mvd_multitrack_2_values);
	Cvar_Register (&mvd_multitrack_3);
	Cvar_Register (&mvd_multitrack_3_values);
	Cvar_Register (&mvd_multitrack_4);
	Cvar_Register (&mvd_multitrack_4_values);

	Cvar_Register (&mvd_status);
	Cvar_Register (&mvd_status_x);
	Cvar_Register (&mvd_status_y);

	Cvar_Register (&mvd_write_xml);
	
	Cmd_AddCommand ("mvd_runs",MVD_List_Runs_f);
	Cmd_AddCommand ("mvd_xml",MVD_Status_Xml);
#ifdef DEBUG
	Cmd_AddCommand ("mvd_test",MVD_Testor_f);
#endif

	Cvar_ResetCurrentGroup();
}

void MVD_Screen (void){
	MVD_Info ();
	MVD_Status ();
}
