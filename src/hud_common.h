/*
Copyright (C) 2011 azazello and ezQuake team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
//
// common HUD elements
// like clock etc..
//

#ifndef __HUD_COMMON__H__
#define __HUD_COMMON__H__

extern hud_t *hud_netgraph;

void SCR_HUD_Netgraph(hud_t *hud);
void SCR_HUD_DrawNetStats(hud_t *hud);

void HUD_NewMap(void);
void HUD_NewRadarMap(void);
void SCR_HUD_DrawRadar(hud_t *hud);

void HudCommon_Init(void);
void SCR_HUD_DrawNum(hud_t *hud, int num, qbool low, float scale, int style, int digits, char *s_align, qbool proportional);
void SCR_HUD_DrawNum2(hud_t* hud, int num, qbool low, float scale, int style, int digits, char* s_align, qbool proportional, qbool draw_content, byte *text_color_low, byte *text_color_normal);

extern qbool autohud_loaded;
extern cvar_t mvd_autohud;
void HUD_AutoLoad_MVD(int autoload);

// player sorting
// for frags and players
typedef struct sort_teams_info_s {
	char *name;
	int  frags;
	int  min_ping;
	int  avg_ping;
	int  max_ping;
	int  nplayers;
	int  top, bottom;   // leader colours
	int  rlcount;       // Number of RLs present in the team. (Cokeman 2006-05-27)
	int  lgcount;       // number of LGs present in the team
	int  weapcount;     // number of players with weapons (RLs | LGs) in the team
	int  ra_taken;      // Total red armors taken
	int  ya_taken;      // Total yellow armors taken
	int  ga_taken;      // Total green armors taken
	int  mh_taken;      // Total megahealths taken
	int  quads_taken;   // Total quads taken
	int  pents_taken;   // Total pents taken
	int  rings_taken;   // Total rings taken
	int  stack;         // Total damage the team can take
	float ra_lasttime;  // last time ra taken
	float ya_lasttime;  // last time ya taken
	float ga_lasttime;  // last time ga taken
	float mh_lasttime;  // last time mh taken
	float q_lasttime;   // last time quad taken
	float p_lasttime;   // last time pent taken
	float r_lasttime;   // last time ring taken
}
sort_teams_info_t;

typedef struct sort_players_info_s {
	int playernum;
	sort_teams_info_t *team;
}
sort_players_info_t;

extern sort_players_info_t sorted_players_by_frags[MAX_CLIENTS];
extern sort_players_info_t sorted_players[MAX_CLIENTS];
extern sort_teams_info_t   sorted_teams[MAX_CLIENTS];
extern int n_teams;
extern int n_players;
extern int n_spectators;

int HUD_Stats(int stat_num);
extern cvar_t cl_weaponpreselect;

#define HUD_SCOREBOARD_ALL			0xffffffff
#define HUD_SCOREBOARD_SORT_TEAMS	(1 << 0)
#define HUD_SCOREBOARD_SORT_PLAYERS	(1 << 1)
#define HUD_SCOREBOARD_UPDATE		(1 << 2)
#define HUD_SCOREBOARD_AVG_PING		(1 << 3)
void HUD_Sort_Scoreboard(int flags);

extern int active_player_position;
extern int active_team_position;

void SCR_HUD_MultiLineString(hud_t* hud, const char* in, qbool large_font, int alignment, float scale, qbool proportional);

float SCR_HUD_TotalStrength(float health, float armorValue, float armorType);

float SCR_HUD_ArmorType(int items);

#endif // __HUD_COMMON__H__
