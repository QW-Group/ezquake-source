/*
	$Id: mvd_utils.c,v 1.19 2005-09-10 11:06:17 disconn3ct Exp $
*/

#include "quakedef.h"
#include "cl_screen.h"
#include "parser.h"

extern cvar_t tp_name_quad, tp_name_pent, tp_name_ring, tp_name_separator;
extern cvar_t tp_name_axe, tp_name_sg, tp_name_ssg, tp_name_ng, tp_name_sng, tp_name_gl, tp_name_rl, tp_name_lg;
extern cvar_t tp_name_none, tp_weapon_order;
char mvd_info_best_weapon[20], mvd_info_best_ammo[20];
extern int loc_loaded;

extern qbool TP_LoadLocFile (char *path, qbool quiet);
extern char *TP_LocationName(vec3_t location);
extern char *Weapon_NumToString(int num);

int FindBestNick(char *s, int use);
int MVD_AutoTrackBW_f(int i);
int axe_val, sg_val, ssg_val, ng_val, sng_val, gl_val, rl_val, lg_val, ga_val, ya_val, ra_val, ring_val, quad_val, pent_val ;

int mvd_demo_track_run = 0;
int last_track;

// mvd_info cvars
cvar_t			mvd_info		= {"mvd_info", "0"};
cvar_t			mvd_info_setup	= {"mvd_info_setup", "%6n %3f %10l %4a %4h %3w"};
cvar_t			mvd_info_x		= {"mvd_info_x", "0"};
cvar_t			mvd_info_y		= {"mvd_info_y", "0"};

// mvd_autotrack cvars
cvar_t mvd_autotrack = {"mvd_autotrack", "0"};
cvar_t mvd_autotrack_1on1 = {"mvd_autotrack_1on1", "%a * %A + 50 * %W + %p + %f"};
cvar_t mvd_autotrack_1on1_values = {"mvd_autotrack_1on1_values", "1 2 3 2 3 5 8 8 1 2 3 0 0 0"}; 
cvar_t mvd_autotrack_2on2 = {"mvd_autotrack_2on2", "%a * %A + 50 * %W + %p + %f"};
cvar_t mvd_autotrack_2on2_values = {"mvd_autotrack_2on2_values", "1 2 3 2 3 5 8 8 1 2 3 500 900 1000"}; 
cvar_t mvd_autotrack_4on4 = {"mvd_autotrack_4on4", "%a * %A + 50 * %W + %p + %f"};
cvar_t mvd_autotrack_4on4_values = {"mvd_autotrack_4on4_values", "1 2 4 2 4 6 10 10 1 2 3 500 900 1000"}; 
cvar_t mvd_autotrack_custom = {"mvd_autotrack_custom", "%a * %A + 50 * %W + %p + %f"};
cvar_t mvd_autotrack_custom_values = {"mvd_autotrack_custom_values", "1 2 3 2 3 6 6 1 2 3 500 900 1000"}; 


	typedef struct bp_var_s{
		int id;
		int val;
	} bp_var_t;

	bp_var_t bp_var[MAX_CLIENTS];



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
	player_info_t *mvd_info_players;
	mvd_info_players = cl.players;
	for (s = t; *s; s++) {
		for (x = 0 ; x < strlen(*s) ; x++) {
			switch ((*s)[x]) {
				case '1': if (mvd_info_players[i].stats[STAT_ITEMS] & IT_AXE) return IT_AXE; break;
				case '2': if (mvd_info_players[i].stats[STAT_ITEMS] & IT_SHOTGUN) return IT_SHOTGUN; break;
				case '3': if (mvd_info_players[i].stats[STAT_ITEMS] & IT_SUPER_SHOTGUN) return IT_SUPER_SHOTGUN; break;
				case '4': if (mvd_info_players[i].stats[STAT_ITEMS] & IT_NAILGUN) return IT_NAILGUN; break;
				case '5': if (mvd_info_players[i].stats[STAT_ITEMS] & IT_SUPER_NAILGUN) return IT_SUPER_NAILGUN; break;
				case '6': if (mvd_info_players[i].stats[STAT_ITEMS] & IT_GRENADE_LAUNCHER) return IT_GRENADE_LAUNCHER; break;
				case '7': if (mvd_info_players[i].stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER) return IT_ROCKET_LAUNCHER; break;
				case '8': if (mvd_info_players[i].stats[STAT_ITEMS] & IT_LIGHTNING) return IT_LIGHTNING; break;
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
				
char *MVD_BestAmmo (int i) {
	player_info_t *mvd_info_players;
	mvd_info_players = cl.players;

	switch (MVD_BestWeapon(i)) {
		case IT_SHOTGUN: case IT_SUPER_SHOTGUN:
			Q_snprintfz(mvd_info_best_ammo, sizeof(mvd_info_best_ammo), "%i", mvd_info_players[i].stats[STAT_SHELLS]);
			return mvd_info_best_ammo;
				
		case IT_NAILGUN: case IT_SUPER_NAILGUN:
			Q_snprintfz(mvd_info_best_ammo, sizeof(mvd_info_best_ammo), "%i", mvd_info_players[i].stats[STAT_NAILS]);
			return mvd_info_best_ammo;
				
		case IT_GRENADE_LAUNCHER: case IT_ROCKET_LAUNCHER:
			Q_snprintfz(mvd_info_best_ammo, sizeof(mvd_info_best_ammo), "%i", mvd_info_players[i].stats[STAT_ROCKETS]);
			return mvd_info_best_ammo;
			
		case IT_LIGHTNING:
			Q_snprintfz(mvd_info_best_ammo, sizeof(mvd_info_best_ammo), "%i", mvd_info_players[i].stats[STAT_CELLS]);
			return mvd_info_best_ammo;
			
		default: return "0";
	}
}


void MVD_Info (void){
	char str[80];
	char mvd_info_final_string[80], mvd_info_powerups[20];
	char *mapname;
	int x,y,z,i,h,value = 0;
	player_state_t *mvd_state ;
	player_info_t *mvd_info_players;
	mvd_state = cl.frames[cl.parsecount & UPDATE_MASK].playerstate;
	mvd_info_players = cl.players;


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

	for ( i=0 ; i<MAX_CLIENTS ; i++ ){
	if (!mvd_info_players[i].name[0])
		continue;
	if (mvd_info_players[i].spectator)
		continue;

	for(h=0;h<MAX_CLIENTS;h++){
		if (mvd_info_players[i].userid == bp_var[h].id)
			value = bp_var[h].val;
	}

	if (mvd_info_players[i].stats[STAT_ITEMS] & IT_QUAD)
		Q_strncpyz(mvd_info_powerups, tp_name_quad.string, sizeof(mvd_info_powerups));
              
	if (mvd_info_players[i].stats[STAT_ITEMS] & IT_INVULNERABILITY) {
		if (mvd_info_powerups[0])
			Q_strncatz(mvd_info_powerups, tp_name_separator.string);
		Q_strncatz(mvd_info_powerups, tp_name_pent.string);
	}

	if (mvd_info_players[i].stats[STAT_ITEMS] & IT_INVISIBILITY) {
		if (mvd_info_powerups[0])
			Q_strncatz(mvd_info_powerups, tp_name_separator.string);
		Q_strncatz(mvd_info_powerups, tp_name_ring.string);
	}


	strcpy(mvd_info_final_string,mvd_info_setup.string);
	printf("works before Replace_In_String ?\n");
	Replace_In_String(mvd_info_final_string,sizeof(mvd_info_final_string),'%',\
			9,\
			"a",va("%i",mvd_info_players[i].stats[STAT_ARMOR]),\
			"f",va("%i",mvd_info_players[i].frags),\
			"h",va("%i",mvd_info_players[i].stats[STAT_HEALTH]),\
			"l",TP_LocationName(mvd_state[i].origin),\
			"n",mvd_info_players[i].name,\
			"P",va("%i",mvd_info_players[i].ping),\
			"p",mvd_info_powerups,\
			"v",va("%i",value),\
			"w",va("%s:%i",Weapon_NumToString(mvd_info_players[i].stats[STAT_ACTIVEWEAPON]),mvd_info_players[i].stats[STAT_AMMO]),\
			"W",va("%s:%s",MVD_BestWeapon_strings(i),MVD_BestAmmo(i)));
	printf("works after Replace_In_String ?\n");
	Q_strncpyz(str, mvd_info_final_string, sizeof(str));
	Draw_String (x, y+((z++)*8), str);
	
	#ifdef DEBUG
	printf("MVD_Info Stopped\n");
	#endif
	}
}


int MVD_FindBestPlayer_f(void){

	int bp_id, status, i, h, y = 0;
	
	char val[80],string1[80];
	int bp_at,bp_bw,bp_pw,bp_h;
	int lastval = 0;
	int z = 0;
	int gtype = 0;

	player_info_t *bp_info;
	bp_info = cl.players;

	#ifdef DEBUG
	printf("MVD_FindBestPlayer_f Started\n");
	#endif
	
	
	for ( i=0; i<MAX_CLIENTS ; i++ ){
		if (!bp_info[i].name[0])
			continue;
		if (bp_info[i].spectator)
			continue;
		gtype++;
	}	
	
	if (gtype == 2)
		strcpy(val,mvd_autotrack_1on1_values.string);
	else if (gtype == 4)
		strcpy(val,mvd_autotrack_2on2_values.string);
	else if (gtype == 8)
		strcpy(val,mvd_autotrack_4on4_values.string);
	else if (mvd_autotrack.value == 2)
		strcpy(val,mvd_autotrack_custom_values.string);
	else
		strcpy(val,mvd_autotrack_1on1_values.string);
	
	
	// checking if mvd_autotrack_xonx_values have right format
	if (strtok(val, " ") != NULL){
		for(y=1;y<=13;y++){
			if (strtok(NULL, " ") != NULL){
			}else{
				Com_Printf("mvd_autotrack aborting due to wrong use of mvd_autotrack_*_value\n");
				Cvar_SetValue(&mvd_autotrack,0);
				return 0;
			}
		}
	} else {
		Com_Printf("mvd_autotrack aborting due to wrong use of mvd_autotrack_*_value\n");
		Cvar_SetValue(&mvd_autotrack,0);
	return 0;
	}

	if (gtype == 2)
		strcpy(val,mvd_autotrack_1on1_values.string);
	else if (gtype == 4)
		strcpy(val,mvd_autotrack_2on2_values.string);
	else if (gtype == 8)
		strcpy(val,mvd_autotrack_4on4_values.string);
	else if (mvd_autotrack.value == 2)
		strcpy(val,mvd_autotrack_custom_values.string);
	else
		strcpy(val,mvd_autotrack_1on1_values.string);
	
	
	// getting values
	//val = mvd_autotrack_1on1_values.string;
	axe_val	=atoi(strtok(val, " "));
	sg_val	=atoi(strtok(NULL, " "));
	ssg_val	=atoi(strtok(NULL, " "));
	ng_val	=atoi(strtok(NULL, " "));
	sng_val	=atoi(strtok(NULL, " "));
	gl_val	=atoi(strtok(NULL, " "));
	rl_val	=atoi(strtok(NULL, " "));
	lg_val	=atoi(strtok(NULL, " "));
	ga_val	=atoi(strtok(NULL, " "));
	ya_val	=atoi(strtok(NULL, " "));
	ra_val	=atoi(strtok(NULL, " "));
	ring_val=atoi(strtok(NULL, " "));
	quad_val=atoi(strtok(NULL, " "));
	pent_val=atoi(strtok(NULL, " "));
	//axe_val=sg_val=ssg_val=ng_val=sng_val=gl_val=rl_val=lg_val=ga_val=ya_val=ra_val=ring_val=quad_val=pent_val=10;
	
	
	for ( i=0, z=0; i<MAX_CLIENTS ; i++ ){
		if (!bp_info[i].name[0])
			continue;
		if (bp_info[i].spectator)
			continue;
	
		// get bestweapon
		
		bp_bw = MVD_AutoTrackBW_f(i);
		

	
		
		// get armortype
		if (bp_info[i].stats[STAT_ITEMS] & IT_ARMOR1)
			bp_at = ga_val ;
		else if (bp_info[i].stats[STAT_ITEMS] & IT_ARMOR2)
			bp_at = ya_val ;
		else if (bp_info[i].stats[STAT_ITEMS] & IT_ARMOR3)
			bp_at = ra_val ;
		else
			bp_at = 0;
		
		// get powerup type
		bp_pw=0;
		if (bp_info[i].stats[STAT_ITEMS] & IT_QUAD)
			bp_pw += quad_val;
		if (bp_info[i].stats[STAT_ITEMS] & IT_INVULNERABILITY)
			bp_pw += pent_val;
		if (bp_info[i].stats[STAT_ITEMS] & IT_INVISIBILITY)
			bp_pw += ring_val;
		
		// get health
		bp_h=bp_info[i].stats[STAT_HEALTH];
		
	if (gtype == 2)
		strcpy(string1,mvd_autotrack_1on1.string);
	else if (gtype == 4)
		strcpy(string1,mvd_autotrack_2on2.string);
	else if (gtype == 8)
		strcpy(string1,mvd_autotrack_4on4.string);
	else if (mvd_autotrack.value == 2)
		strcpy(string1,mvd_autotrack_custom.string);
	else
		strcpy(string1,mvd_autotrack_1on1.string);
		
	
	Replace_In_String(string1,sizeof(string1),'%',6,\
		"a",va("%i",bp_info[i].stats[STAT_ARMOR]),\
		"A",va("%i",bp_at),\
		"f",va("%i",bp_info[i].frags),\
		"h",va("%i",bp_info[i].stats[STAT_HEALTH]),\
		"W",va("%i",bp_bw),\
		"p",va("%i",bp_pw));
		
	
	printf("%s\n",string1);
		status=eval_string_int(string1);

		if(status<0){
			Cvar_SetValue(&mvd_autotrack,0);
			Com_Printf("Error in your mvd_autotrack settings\n");
			return(0);
		}
		printf("%s %i\n",string1,atoi(string1));
				
		bp_var[z].id 	= bp_info[i].userid;
		bp_var[z++].val = atoi(string1);
		
		//Com_Printf("%s\n",string1);
	}
	


	lastval=0;
	bp_id=0;
	for ( h=0 ; h<z ; h++ ){
		if(lastval < bp_var[h].val){
				lastval = bp_var[h].val;
				bp_id 	= bp_var[h].id;
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
					case '1': if (bp_info[i].stats[STAT_ITEMS] & IT_AXE) return 0; break;
					case '2': if (bp_info[i].stats[STAT_ITEMS] & IT_SHOTGUN) return 1; break;
					case '3': if (bp_info[i].stats[STAT_ITEMS] & IT_SUPER_SHOTGUN) return 2; break;
					case '4': if (bp_info[i].stats[STAT_ITEMS] & IT_NAILGUN) return 3; break;
					case '5': if (bp_info[i].stats[STAT_ITEMS] & IT_SUPER_NAILGUN) return 4; break;
					case '6': if (bp_info[i].stats[STAT_ITEMS] & IT_GRENADE_LAUNCHER) return 5; break;
					case '7': if (bp_info[i].stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER) return 6; break;
					case '8': if (bp_info[i].stats[STAT_ITEMS] & IT_LIGHTNING) return 6; break;
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
	
	id = MVD_FindBestPlayer_f();
	if ( id != last_track){
		sprintf(arg,"track %i \n",id);
		Cbuf_AddText(arg);
		last_track = id;
	}
	#ifdef DEBUG
	printf("MVD_AutoTrack_f Stopped\n");
	#endif
}


void MVD_Mainhook_f (void){
	MVD_AutoTrack_f ();
	if (cls.mvdplayback && mvd_demo_track_run == 0) 
	MVD_Demo_Track ();
}

void MVD_Utils_Init (void) {
	Cvar_SetCurrentGroup(CVAR_GROUP_VIEW);
	Cvar_Register (&mvd_info);
	Cvar_Register (&mvd_info_setup);
	Cvar_Register (&mvd_info_x);
	Cvar_Register (&mvd_info_y);
	Cvar_Register (&mvd_autotrack);
	Cvar_Register (&mvd_autotrack_1on1);
	Cvar_Register (&mvd_autotrack_1on1_values);
	Cvar_Register (&mvd_autotrack_2on2);
	Cvar_Register (&mvd_autotrack_2on2_values);
	Cvar_Register (&mvd_autotrack_4on4);
	Cvar_Register (&mvd_autotrack_4on4_values);
	Cvar_Register (&mvd_autotrack_custom);
	Cvar_Register (&mvd_autotrack_custom_values);

	Cvar_ResetCurrentGroup();
}

void MVD_Screen (void){
	MVD_Info ();
}
