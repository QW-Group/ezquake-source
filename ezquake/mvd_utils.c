#include "quakedef.h"
#include "common.h"
#include "teamplay.h"
#include "cl_screen.h"
#include "utils.h"



extern cvar_t tp_name_quad, tp_name_pent, tp_name_ring, tp_name_separator;
extern cvar_t tp_name_axe, tp_name_sg, tp_name_ssg, tp_name_ng, tp_name_sng, tp_name_gl, tp_name_rl, tp_name_lg;
extern cvar_t tp_name_none, tp_weapon_order;
char mvd_info_best_weapon[20], mvd_info_best_ammo[20];
extern int loc_loaded;

extern qboolean TP_LoadLocFile (char *path, qboolean quiet);
extern char *TP_LocationName(vec3_t location);
extern char *Weapon_NumToString(int num);

int MVD_AutoTrackBW_f(int i);

// mvd_info cvars
cvar_t			mvd_info		= {"mvd_info", "0"};
cvar_t			mvd_info_setup	= {"mvd_info_setup", "%n %f %l %a %h %w"};
cvar_t			mvd_info_x		= {"mvd_info_x", "0"};
cvar_t			mvd_info_y		= {"mvd_info_y", "0"};

// mvd_autotrack cvars
cvar_t mvd_autotrack = {"mvd_autotrack", "0"};


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

void MVD_INFO_Usage_f (void) {
	Com_Printf("mvd_info_setup usage:\n");
	Com_Printf("%%a - Armor\n");
	Com_Printf("%%f - Frags\n");
	Com_Printf("%%h - Health\n");
	Com_Printf("%%l - Location\n");
	Com_Printf("%%n - Nickname\n");
	Com_Printf("%%p - Powerups\n");
	Com_Printf("%%P - Ping\n");
	Com_Printf("%%w - Current Weapon:ammo\n");
	Com_Printf("%%w - Best Weapon:ammo\n");

}

void MVD_Info (void){
	char str[80];
	char location[13];
	char *tmpstr1, *tmpstr, *info_type;
	char mvd_info_final_string[80], mvd_info_final_string_tmp[80], mvd_info_powerups[20];
	char *mapname;
	int x,y,z,i ;

	player_state_t *mvd_state ;
	player_info_t *mvd_info_players;
	mvd_state = cl.frames[cl.parsecount & UPDATE_MASK].playerstate;
	mvd_info_players = cl.players;


	
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
	//Q_snprintfz(str, sizeof(str),"%10s%6s%6s%7s%6s%10s","Name","Ping","Frags","Health","Armor","Location");
	//Draw_String (x, y, str);
	for ( i=0 ; i<MAX_CLIENTS ; i++ ){
	if (!mvd_info_players[i].name[0])
		continue;
	if (mvd_info_players[i].spectator)
		continue;
	
	Q_strncpyz(location,TP_LocationName(mvd_state[i].origin),sizeof(location));
	
	//Q_strncpyz(tmpstr,scr_mvdinfo_setup.string,sizeof(tmpstr));
	tmpstr=mvd_info_setup.string;
	
	mvd_info_final_string[0]='\0';
	mvd_info_final_string_tmp[0]='\0';
	mvd_info_powerups[0]='\0';
	mvd_info_best_ammo[0]='\0';
	mvd_info_best_weapon[0]='\0';
	while (strstr(tmpstr,"%")){
		
		tmpstr1 = strstr(tmpstr,"%");
		tmpstr=tmpstr1+1;
		info_type = tmpstr1+1;
		switch ( info_type[0] ){
			case 'a' :
				if(strlen(mvd_info_final_string)){
				Q_strncpyz(mvd_info_final_string_tmp,mvd_info_final_string,sizeof(mvd_info_final_string));
				Q_snprintfz(mvd_info_final_string,sizeof(mvd_info_final_string),"%s %3i",mvd_info_final_string_tmp,mvd_info_players[i].stats[STAT_ARMOR]);
				}else{
				Q_snprintfz(mvd_info_final_string,sizeof(mvd_info_final_string),"%3i",mvd_info_players[i].stats[STAT_ARMOR]);
				}
				break;
			case 'f' :
				if(strlen(mvd_info_final_string)){
				Q_strncpyz(mvd_info_final_string_tmp,mvd_info_final_string,sizeof(mvd_info_final_string));
				Q_snprintfz(mvd_info_final_string,sizeof(mvd_info_final_string),"%s %3i",mvd_info_final_string_tmp,mvd_info_players[i].frags);
				}else{
				Q_snprintfz(mvd_info_final_string,sizeof(mvd_info_final_string),"%3i",mvd_info_players[i].frags);
				}
				break;
			case 'h' :
				if(strlen(mvd_info_final_string)){
				Q_strncpyz(mvd_info_final_string_tmp,mvd_info_final_string,sizeof(mvd_info_final_string));
				Q_snprintfz(mvd_info_final_string,sizeof(mvd_info_final_string),"%s %3i",mvd_info_final_string_tmp,mvd_info_players[i].stats[STAT_HEALTH]);
				}else{
				Q_snprintfz(mvd_info_final_string,sizeof(mvd_info_final_string),"%3i",mvd_info_players[i].stats[STAT_HEALTH]);
				}
				break;
			case 'l' :
				if(strlen(mvd_info_final_string)){
				Q_strncpyz(mvd_info_final_string_tmp,mvd_info_final_string,sizeof(mvd_info_final_string));
				Q_snprintfz(mvd_info_final_string,sizeof(mvd_info_final_string),"%s %-13s",mvd_info_final_string_tmp,location);
				}else{
				Q_snprintfz(mvd_info_final_string,sizeof(mvd_info_final_string),"%-13s",location);
				}
				break;
			case 'n' :
				if(strlen(mvd_info_final_string)){
				Q_strncpyz(mvd_info_final_string_tmp,mvd_info_final_string,sizeof(mvd_info_final_string));
				Q_snprintfz(mvd_info_final_string,sizeof(mvd_info_final_string),"%s %-10s",mvd_info_final_string_tmp,mvd_info_players[i].name);
				}else{
				Q_snprintfz(mvd_info_final_string,sizeof(mvd_info_final_string),"%-10s",mvd_info_players[i].name);
				}
				break;
			case 'P' :
				if(strlen(mvd_info_final_string)){
				Q_strncpyz(mvd_info_final_string_tmp,mvd_info_final_string,sizeof(mvd_info_final_string));
				Q_snprintfz(mvd_info_final_string,sizeof(mvd_info_final_string),"%s %3i",mvd_info_final_string_tmp,mvd_info_players[i].ping);
				}else{
				Q_snprintfz(mvd_info_final_string,sizeof(mvd_info_final_string),"%3i",mvd_info_players[i].ping);
				}
				break;
			
			case 'p' :
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
				
				if(strlen(mvd_info_final_string)){
				Q_strncpyz(mvd_info_final_string_tmp,mvd_info_final_string,sizeof(mvd_info_final_string));
				Q_snprintfz(mvd_info_final_string,sizeof(mvd_info_final_string),"%s %-5s",mvd_info_final_string_tmp,mvd_info_powerups);
				}else{
				Q_snprintfz(mvd_info_final_string,sizeof(mvd_info_final_string),"%-5s",mvd_info_powerups);
				}
				break;
			
			case 'w' :
				if(strlen(mvd_info_final_string)){
				Q_strncpyz(mvd_info_final_string_tmp,mvd_info_final_string,sizeof(mvd_info_final_string));
				Q_snprintfz(mvd_info_final_string,sizeof(mvd_info_final_string),"%s %3s:%-3i",mvd_info_final_string_tmp,Weapon_NumToString(mvd_info_players[i].stats[STAT_ACTIVEWEAPON]),mvd_info_players[i].stats[STAT_AMMO]);
				}else{
				Q_snprintfz(mvd_info_final_string,sizeof(mvd_info_final_string),"%3s:%-3i",Weapon_NumToString(mvd_info_players[i].stats[STAT_ACTIVEWEAPON]),mvd_info_players[i].stats[STAT_AMMO]);
				}
				break;
			
			case 'W' :
				if(strlen(mvd_info_final_string)){
				Q_strncpyz(mvd_info_final_string_tmp,mvd_info_final_string,sizeof(mvd_info_final_string));
				Q_snprintfz(mvd_info_final_string,sizeof(mvd_info_final_string),"%s %3s:%-3s",mvd_info_final_string_tmp,MVD_BestWeapon_strings(i),MVD_BestAmmo(i));
				}else{
				Q_snprintfz(mvd_info_final_string,sizeof(mvd_info_final_string),"%3s:%-3s",MVD_BestWeapon_strings(i),MVD_BestAmmo(i));
				}
				break;
			
			default :
				break;
		}
		
	}
	Q_strncpyz(str, mvd_info_final_string, sizeof(str));
	Draw_String (x, y+((z++)*8), str);
	
	}
}


int MVD_FindBestPlayer_f(void){

	int bp_id,bp_calc_val,i,h,j = 0;
	int bp_at,bp_bw,bp_pw;
	int lastval = 0;
	int z = 0;
	player_info_t *bp_info;
	

	typedef struct bp_var_s{
		int id;
		int val;
	} bp_var_t;
	bp_var_t bp_var[MAX_CLIENTS];
	bp_info = cl.players;


	
	for ( i=0, z=0; i<MAX_CLIENTS ; i++ ){
		if (!bp_info[i].name[0])
			continue;
		if (bp_info[i].spectator)
			continue;
	
		// get bestweapon
		bp_bw = MVD_AutoTrackBW_f(i);
		
		
	
		
		// get armortype
		if (bp_info[i].stats[STAT_ITEMS] & IT_ARMOR1)
			bp_at = 1 ;
		else if (bp_info[i].stats[STAT_ITEMS] & IT_ARMOR2)
			bp_at = 2 ;
		else if (bp_info[i].stats[STAT_ITEMS] & IT_ARMOR3)
			bp_at = 3 ;
		else
			bp_at = 0;
		
		// get powerup type
		bp_pw=0;
		if (bp_info[i].stats[STAT_ITEMS] & IT_QUAD)
			bp_pw += 900;
		if (bp_info[i].stats[STAT_ITEMS] & IT_INVULNERABILITY)
			bp_pw += 1000;
		if (bp_info[i].stats[STAT_ITEMS] & IT_INVISIBILITY)
			bp_pw += 300;
		// calculating players "value"
		bp_calc_val = (bp_info[i].stats[STAT_ARMOR] * bp_at) + (50 * bp_bw) + bp_pw + bp_info[i].frags;
		
		
		bp_var[z].id 	= bp_info[i].userid;
		bp_var[z++].val = bp_calc_val;
	}
	


	// looking whos best
	lastval=0;
	bp_id=0;
	for ( h=0 ; h<z ; h++ ){
		if(lastval < bp_var[h].val){
				lastval = bp_var[h].val;
				bp_id 	= bp_var[h].id;
		}
		
	}
	return(bp_id);
	
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
	int i;
	if (!mvd_autotrack.value)
		return;

	i = MVD_FindBestPlayer_f();
	arg[0]='\0';
	sprintf(arg,"track %i \n",i);
	Cbuf_AddText(arg);
}


void MVD_Mainhook_f (void){
	MVD_AutoTrack_f ();
}

void MVD_Utils_Init (void) {
	Cvar_SetCurrentGroup(CVAR_GROUP_VIEW);
	Cvar_Register (&mvd_info);
	Cvar_Register (&mvd_info_setup);
	Cvar_Register (&mvd_info_x);
	Cvar_Register (&mvd_info_y);
	Cvar_Register (&mvd_autotrack);
	Cvar_ResetCurrentGroup();
	Cmd_AddCommand ("scr_mvdinfo_setup_usage",MVD_INFO_Usage_f);
}

void MVD_Screen (void){
	MVD_Info ();
}
