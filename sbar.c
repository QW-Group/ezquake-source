/*
Copyright (C) 1996-1997 Id Software, Inc.
 
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

	$Id: sbar.c,v 1.42 2007-09-14 13:29:29 disconn3ct Exp $
*/
// sbar.c -- status bar code

#include "quakedef.h"
#ifndef CLIENTONLY
#include "server.h"
#endif
#include "hud.h"
#include "hud_common.h"
#include "vx_stuff.h"
#include "gl_model.h"
#include "gl_local.h"
#include "teamplay.h"
#include "utils.h"
#include "sbar.h"
#include "keys.h"

#include "qsound.h"

int sb_updates;		// if >= vid.numpages, no update needed
extern cvar_t show_fps2;

// --> mqwcl 0.96 oldhud customisation
//cvar_t  sbar_teamfrags = {"scr_sbar_teamfrags", "1"};
//cvar_t  sbar_fraglimit = {"scr_sbar_fraglimit", "1"};

cvar_t  sbar_drawfaceicon   = {"scr_sbar_drawfaceicon", "1"};
cvar_t  sbar_drawammoicon   = {"scr_sbar_drawammoicon", "1"};
cvar_t  sbar_drawarmoricon  = {"scr_sbar_drawarmoricon", "1"};
cvar_t  sbar_drawguns       = {"scr_sbar_drawguns", "1"};
cvar_t  sbar_drawammocounts = {"scr_sbar_drawammocounts", "1"};
cvar_t  sbar_drawitems      = {"scr_sbar_drawitems", "1"};
cvar_t  sbar_drawsigils     = {"scr_sbar_drawsigils", "1"};
cvar_t  sbar_drawhealth     = {"scr_sbar_drawhealth", "1"};;
cvar_t  sbar_drawarmor      = {"scr_sbar_drawarmor",  "1"};;
cvar_t  sbar_drawammo       = {"scr_sbar_drawammo",   "1"};;

cvar_t  hud_centerranking   = {"scr_scoreboard_centered",   "1"};
cvar_t  hud_rankingpos_y      = {"scr_scoreboard_posy",       "0"};
cvar_t  hud_rankingpos_x      = {"scr_scoreboard_posx",       "0"};
cvar_t  hud_faderankings    = {"scr_scoreboard_fadescreen", "0"};
//cvar_t  hud_ranks_separate  = {"scr_ranks_separate",   "1"};
// <-- mqwcl 0.96 oldhud customisation

#define STAT_MINUS		10	// num frame for '-' stats digit
mpic_t		*sb_nums[2][11];
mpic_t		*sb_colon, *sb_slash;
mpic_t		*sb_ibar;
mpic_t		*sb_sbar;
mpic_t		*sb_scorebar;

mpic_t		*sb_weapons[7][8];	// 0 is active, 1 is owned, 2-5 are flashes
mpic_t		*sb_ammo[4];
mpic_t		*sb_sigil[4];
mpic_t		*sb_armor[3];
mpic_t		*sb_items[32];

mpic_t	*sb_faces[5][2];		// 0 is dead, 1-4 are alive
// 0 is static, 1 is temporary animation
mpic_t	*sb_face_invis;
mpic_t	*sb_face_quad;
mpic_t	*sb_face_invuln;
mpic_t	*sb_face_invis_invuln;

qbool	sb_showscores;
qbool	sb_showteamscores;

int			sb_lines;			// scan lines to draw

void Draw_AlphaFill (int x, int y, int w, int h, byte c, float alpha);

static int	sbar_xofs;

cvar_t	scr_centerSbar = {"scr_centerSbar", "1"};

cvar_t	scr_compactHud = {"scr_compactHud", "0"};
cvar_t	scr_compactHudAlign = {"scr_compactHudAlign", "0"};

cvar_t	scr_drawHFrags = {"scr_drawHFrags", "1"};
cvar_t	scr_drawVFrags = {"scr_drawVFrags", "1"};

cvar_t scr_scoreboard_afk = {"scr_scoreboard_afk", "1"};
cvar_t scr_scoreboard_afk_style = {"scr_scoreboard_afk_style", "1"};

cvar_t	scr_scoreboard_teamsort = {"scr_scoreboard_teamsort", "1"};
cvar_t	scr_scoreboard_forcecolors = {"scr_scoreboard_forcecolors", "1"};
cvar_t	scr_scoreboard_showfrags = {"scr_scoreboard_showfrags", "1"};
cvar_t	scr_scoreboard_drawtitle = {"scr_scoreboard_drawtitle", "1"};
cvar_t	scr_scoreboard_borderless = {"scr_scoreboard_borderless", "1"};
cvar_t	scr_scoreboard_spectator_name = {"scr_scoreboard_spectator_name", "\xF3\xF0\xE5\xE3\xF4\xE1\xF4\xEF\xF2"}; // brown "spectator". old: &cF20s&cF50p&cF80e&c883c&cA85t&c668a&c55At&c33Bo&c22Dr
cvar_t	scr_scoreboard_kill_color = {"scr_scoreboard_kill_color", "0B4"};
cvar_t	scr_scoreboard_death_color = {"scr_scoreboard_death_color", "F00"};
cvar_t	scr_scoreboard_tk_color = {"scr_scoreboard_tk_color", "FF0"};
cvar_t	scr_scoreboard_fillalpha = {"scr_scoreboard_fillalpha", "0.7"};
cvar_t	scr_scoreboard_fillcolored = {"scr_scoreboard_fillcolored", "2"};


// VFrags: only draw the frags for the first player when using mvinset
#define MULTIVIEWTHISPOV() ((!cl_multiview.value) || (cl_mvinset.value && CL_MultiviewCurrentView() == 1))

static qbool Sbar_IsStandardBar(void) {
	// Old status bar is turned on, or the screen size is less than full width
	return cl_sbar.value || scr_viewsize.value < 100;
}

static qbool Sbar_ShowScoreboardIndicator (void)
{
	if (!cls.mvdplayback || !cl_multiview.value)
		return true;

	if (cl_multiview.integer == 1 || (cl_multiview.integer == 2 && cl_mvinset.integer))
		return true;

	return false;
}

/********************************** CONTROL **********************************/

//Tab key down
void Sbar_ShowTeamScores (void) {
	if (sb_showteamscores)
		return;

	sb_showteamscores = true;
	sb_updates = 0;
}

//Tab key up
void Sbar_DontShowTeamScores (void) {
	sb_showteamscores = false;
	sb_updates = 0;
}

//Tab key down
void Sbar_ShowScores (void) {
	if (sb_showscores)
		return;

	sb_showscores = true;
	sb_updates = 0;
}

//Tab key up
void Sbar_DontShowScores (void) {
	sb_showscores = false;
	sb_updates = 0;
}

void Sbar_Changed (void) {
	sb_updates = 0;	// update next frame
}

/************************************ INIT ************************************/

void Sbar_Init (void) {
	int i;

	for (i = 0; i < 10; i++) {
		sb_nums[0][i] = Draw_CacheWadPic (va("num_%i",i));
		sb_nums[1][i] = Draw_CacheWadPic (va("anum_%i",i));
	}

	sb_nums[0][10] = Draw_CacheWadPic ("num_minus");
	sb_nums[1][10] = Draw_CacheWadPic ("anum_minus");

	sb_colon = Draw_CacheWadPic ("num_colon");
	sb_slash = Draw_CacheWadPic ("num_slash");

	sb_weapons[0][0] = Draw_CacheWadPic ("inv_shotgun");
	sb_weapons[0][1] = Draw_CacheWadPic ("inv_sshotgun");
	sb_weapons[0][2] = Draw_CacheWadPic ("inv_nailgun");
	sb_weapons[0][3] = Draw_CacheWadPic ("inv_snailgun");
	sb_weapons[0][4] = Draw_CacheWadPic ("inv_rlaunch");
	sb_weapons[0][5] = Draw_CacheWadPic ("inv_srlaunch");
	sb_weapons[0][6] = Draw_CacheWadPic ("inv_lightng");

	sb_weapons[1][0] = Draw_CacheWadPic ("inv2_shotgun");
	sb_weapons[1][1] = Draw_CacheWadPic ("inv2_sshotgun");
	sb_weapons[1][2] = Draw_CacheWadPic ("inv2_nailgun");
	sb_weapons[1][3] = Draw_CacheWadPic ("inv2_snailgun");
	sb_weapons[1][4] = Draw_CacheWadPic ("inv2_rlaunch");
	sb_weapons[1][5] = Draw_CacheWadPic ("inv2_srlaunch");
	sb_weapons[1][6] = Draw_CacheWadPic ("inv2_lightng");

	for (i = 0; i < 5; i++) {
		sb_weapons[2 + i][0] = Draw_CacheWadPic (va("inva%i_shotgun", i + 1));
		sb_weapons[2 + i][1] = Draw_CacheWadPic (va("inva%i_sshotgun", i + 1));
		sb_weapons[2 + i][2] = Draw_CacheWadPic (va("inva%i_nailgun", i + 1));
		sb_weapons[2 + i][3] = Draw_CacheWadPic (va("inva%i_snailgun", i + 1));
		sb_weapons[2 + i][4] = Draw_CacheWadPic (va("inva%i_rlaunch", i + 1));
		sb_weapons[2 + i][5] = Draw_CacheWadPic (va("inva%i_srlaunch", i + 1));
		sb_weapons[2 + i][6] = Draw_CacheWadPic (va("inva%i_lightng", i + 1));
	}

	sb_ammo[0] = Draw_CacheWadPic ("sb_shells");
	sb_ammo[1] = Draw_CacheWadPic ("sb_nails");
	sb_ammo[2] = Draw_CacheWadPic ("sb_rocket");
	sb_ammo[3] = Draw_CacheWadPic ("sb_cells");

	sb_armor[0] = Draw_CacheWadPic ("sb_armor1");
	sb_armor[1] = Draw_CacheWadPic ("sb_armor2");
	sb_armor[2] = Draw_CacheWadPic ("sb_armor3");

	sb_items[0] = Draw_CacheWadPic ("sb_key1");
	sb_items[1] = Draw_CacheWadPic ("sb_key2");
	sb_items[2] = Draw_CacheWadPic ("sb_invis");
	sb_items[3] = Draw_CacheWadPic ("sb_invuln");
	sb_items[4] = Draw_CacheWadPic ("sb_suit");
	sb_items[5] = Draw_CacheWadPic ("sb_quad");

	sb_sigil[0] = Draw_CacheWadPic ("sb_sigil1");
	sb_sigil[1] = Draw_CacheWadPic ("sb_sigil2");
	sb_sigil[2] = Draw_CacheWadPic ("sb_sigil3");
	sb_sigil[3] = Draw_CacheWadPic ("sb_sigil4");

	sb_faces[4][0] = Draw_CacheWadPic ("face1");
	sb_faces[4][1] = Draw_CacheWadPic ("face_p1");
	sb_faces[3][0] = Draw_CacheWadPic ("face2");
	sb_faces[3][1] = Draw_CacheWadPic ("face_p2");
	sb_faces[2][0] = Draw_CacheWadPic ("face3");
	sb_faces[2][1] = Draw_CacheWadPic ("face_p3");
	sb_faces[1][0] = Draw_CacheWadPic ("face4");
	sb_faces[1][1] = Draw_CacheWadPic ("face_p4");
	sb_faces[0][0] = Draw_CacheWadPic ("face5");
	sb_faces[0][1] = Draw_CacheWadPic ("face_p5");

	sb_face_invis = Draw_CacheWadPic ("face_invis");
	sb_face_invuln = Draw_CacheWadPic ("face_invul2");
	sb_face_invis_invuln = Draw_CacheWadPic ("face_inv2");
	sb_face_quad = Draw_CacheWadPic ("face_quad");

	sb_sbar = Draw_CacheWadPic ("sbar");
	sb_ibar = Draw_CacheWadPic ("ibar");
	sb_scorebar = Draw_CacheWadPic ("scorebar");

	Cvar_SetCurrentGroup(CVAR_GROUP_SBAR);
	Cvar_Register (&scr_centerSbar);

	Cvar_Register (&scr_compactHud);
	Cvar_Register (&scr_compactHudAlign);

	// --> mqwcl 0.96 oldhud customisation
	//Cvar_Register (&sbar_teamfrags);
	//Cvar_Register (&sbar_fraglimit);

	Cvar_Register (&sbar_drawfaceicon);
	Cvar_Register (&sbar_drawammoicon);
	Cvar_Register (&sbar_drawarmoricon);
	Cvar_Register (&sbar_drawguns);
	Cvar_Register (&sbar_drawammocounts);
	Cvar_Register (&sbar_drawitems);
	Cvar_Register (&sbar_drawsigils);
	Cvar_Register (&sbar_drawhealth);
	Cvar_Register (&sbar_drawarmor);
	Cvar_Register (&sbar_drawammo);
	Cvar_Register (&hud_centerranking);
	Cvar_Register (&hud_rankingpos_y);
	Cvar_Register (&hud_rankingpos_x);
	Cvar_Register (&hud_faderankings);
	//Cvar_Register (&hud_ranks_separate);
	// <-- mqwcl 0.96 oldhud customisation

	Cvar_Register(&scr_scoreboard_afk);
	Cvar_Register(&scr_scoreboard_afk_style);

	Cvar_Register (&scr_drawHFrags);
	Cvar_Register (&scr_drawVFrags);
	Cvar_Register (&scr_scoreboard_teamsort);
	Cvar_Register (&scr_scoreboard_forcecolors);
	Cvar_Register (&scr_scoreboard_showfrags);
	Cvar_Register (&scr_scoreboard_drawtitle);
	Cvar_Register (&scr_scoreboard_borderless);
	Cvar_Register (&scr_scoreboard_spectator_name);
	Cvar_Register (&scr_scoreboard_kill_color);
	Cvar_Register (&scr_scoreboard_death_color);
	Cvar_Register (&scr_scoreboard_tk_color);
	Cvar_Register (&scr_scoreboard_fillalpha);
	Cvar_Register (&scr_scoreboard_fillcolored);

	Cvar_ResetCurrentGroup();

	Cmd_AddCommand ("+showscores", Sbar_ShowScores);
	Cmd_AddCommand ("-showscores", Sbar_DontShowScores);

	Cmd_AddCommand ("+showteamscores", Sbar_ShowTeamScores);
	Cmd_AddCommand ("-showteamscores", Sbar_DontShowTeamScores);
}

void Request_Pings (void)
{
	if (cls.realtime - cl.last_ping_request > 2)
	{
		cl.last_ping_request = cls.realtime;
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, "pings");
	}
}

/****************************** SUPPORT ROUTINES ******************************/

// drawing routines are relative to the status bar location

static void Sbar_DrawPic (int x, int y, mpic_t *pic) {
	Draw_Pic (x + sbar_xofs, y + (vid.height - SBAR_HEIGHT), pic);
}

//JACK: Draws a portion of the picture in the status bar.
static void Sbar_DrawSubPic(int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height) {
	Draw_SubPic (x, y + (vid.height - SBAR_HEIGHT), pic, srcx, srcy, width, height);
}

static void Sbar_DrawTransPic (int x, int y, mpic_t *pic) {
	Draw_TransPic (x + sbar_xofs, y + (vid.height - SBAR_HEIGHT), pic);
}

//Draws one solid graphics character
static void Sbar_DrawCharacter (int x, int y, int num) {
	Draw_Character (x + 4 + sbar_xofs, y + vid.height - SBAR_HEIGHT, num);
}

void Sbar_DrawString (int x, int y, char *str) {
	Draw_String (x + sbar_xofs, y + vid.height - SBAR_HEIGHT, str);
}

static void Sbar_DrawAltString (int x, int y, char *str) {
	Draw_Alt_String (x + sbar_xofs, y + vid.height - SBAR_HEIGHT, str);
}

static int Sbar_itoa (int num, char *buf) {
	char *str;
	int pow10, dig;

	str = buf;

	if (num < 0) {
		*str++ = '-';
		num = -num;
	}

	for (pow10 = 10 ; num >= pow10 ; pow10 *= 10)
		;

	do {
		pow10 /= 10;
		dig = num / pow10;
		*str++ = '0' + dig;
		num -= dig * pow10;
	} while (pow10 != 1);

	*str = 0;

	return str - buf;
}

void Sbar_DrawNum (int x, int y, int num, int digits, int color) {
	char str[12], *ptr;
	int l, frame;

	l = Sbar_itoa (num, str);
	ptr = str;
	if (l > digits)
		ptr += (l - digits);
	if (l < digits)
		x += (digits - l) * 24;

	while (*ptr) {
		frame = (*ptr == '-') ? STAT_MINUS : *ptr -'0';

		Sbar_DrawTransPic (x, y, sb_nums[color][frame]);
		x += 24;
		ptr++;
	}
}

// this used to be static function
int	Sbar_ColorForMap (int m) {
	m = bound(0, m, 13);

	return 16 * m + 8;
}

// HUD -> hexum

int Sbar_TopColor(player_info_t *player)
{
	return Sbar_ColorForMap(player->topcolor);
}

int Sbar_BottomColor(player_info_t *player)
{
	return Sbar_ColorForMap(player->bottomcolor);
}

// ** HUD -> hexum

/********************************* FRAG SORT *********************************/

int		fragsort[MAX_CLIENTS];
int		scoreboardlines;

#define SCR_TEAM_T_MAXTEAMSIZE	(16 + 1)

typedef struct {
	char team[SCR_TEAM_T_MAXTEAMSIZE];
	int frags;
	int players;
	int plow, phigh, ptotal;
	int topcolor, bottomcolor;
	qbool myteam;
} team_t;
team_t teams[MAX_CLIENTS];

int teamsort[MAX_CLIENTS];
int scoreboardteams;

static __inline int Sbar_PlayerNum(void) {
	int mynum = cl.playernum;

	if (cl.spectator) {
		mynum = Cam_TrackNum ();

		if (mynum < 0)
			mynum = cl.playernum;
	}

	return mynum;
}


static __inline qbool Sbar_IsSpectator(int mynum) {

	return (mynum == cl.playernum) ? cl.spectator : cl.players[mynum].spectator;
}

static void Sbar_SortFrags(qbool spec) {
	int i, j, k;
	static int lastframecount = 0;

	if (!spec && lastframecount && lastframecount == cls.framecount)
		return;


	lastframecount = spec ? 0 : cls.framecount;

	// sort by frags
	scoreboardlines = 0;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].name[0] && (spec || !cl.players[i].spectator)) {
			fragsort[scoreboardlines] = i;
			scoreboardlines++;
			if (cl.players[i].spectator)
				cl.players[i].frags = -999;
		}
	}

	for (i = 0; i < scoreboardlines; i++) {
		for (j = 0; j < scoreboardlines - 1 - i; j++) {
			if (cl.players[fragsort[j]].frags < cl.players[fragsort[j + 1]].frags) {
				k = fragsort[j];
				fragsort[j] = fragsort[j + 1];
				fragsort[j + 1] = k;
			}
		}
	}
}

static void Sbar_SortTeams (void) {
	int i, j, k, mynum, playertoteam[MAX_CLIENTS];
	player_info_t *s;
	char t[SCR_TEAM_T_MAXTEAMSIZE];
	static int lastframecount = 0;

	if (!cl.teamplay)
		return;

	if (lastframecount && lastframecount == cls.framecount)
		return;
	lastframecount = cls.framecount;

	scoreboardteams = 0;

	for (i = 0; i < MAX_CLIENTS; i++)
		playertoteam[i] = -1;

	// sort the teams
	memset(teams, 0, sizeof(teams));
	for (i = 0; i < MAX_CLIENTS; i++)
		teams[i].plow = 999;

	mynum = Sbar_PlayerNum();

	for (i = 0; i < MAX_CLIENTS; i++) {
		s = &cl.players[i];
		if (!s->name[0] || s->spectator)
			continue;

		// find his team in the list
		strlcpy (t, s->team, sizeof(t));
		if (!t[0])
			continue; // not on team

		for (j = 0; j < scoreboardteams; j++) {
			if (!strcmp(teams[j].team, t)) {
				playertoteam[i] = j;

				teams[j].frags += s->frags;
				teams[j].players++;
				if (!cl.teamfortress && i == mynum) {
					teams[j].topcolor = scr_scoreboard_forcecolors.value ? s->topcolor : s->real_topcolor;
					teams[j].bottomcolor = scr_scoreboard_forcecolors.value ? s->bottomcolor : s->real_bottomcolor;
				}
				break;
			}
		}
		if (j == scoreboardteams) { // must add him
			j = scoreboardteams++;
			playertoteam[i] = j;

			teams[j].frags = s->frags;
			teams[j].players = 1;
			if (cl.teamfortress) {
				teams[j].topcolor = teams[j].bottomcolor = Utils_TF_TeamToColor(t);
			} else {
				teams[j].bottomcolor = scr_scoreboard_forcecolors.value ? s->bottomcolor : s->real_bottomcolor;
				teams[j].topcolor = scr_scoreboard_forcecolors.value ? s->topcolor : s->real_topcolor;
			}
			strlcpy(teams[j].team, t, sizeof(teams[j].team));

			if (!Sbar_IsSpectator(mynum) && !strncmp(cl.players[mynum].team, t, sizeof(t) - 1))
				teams[j].myteam = true;
			else
				teams[j].myteam = false;
		}
		teams[j].plow = min(s->ping, teams[j].plow);
		teams[j].phigh = max(s->ping, teams[j].phigh);
		teams[j].ptotal += s->ping;
	}

	//prepare for sort
	for (i = 0; i < scoreboardteams; i++)
		teamsort[i] = i;

	// good 'ol bubble sort
	for (i = 0; i < scoreboardteams - 1; i++) {
		for (j = i + 1; j < scoreboardteams; j++) {
			if (teams[teamsort[i]].frags < teams[teamsort[j]].frags) {
				k = teamsort[i];
				teamsort[i] = teamsort[j];
				teamsort[j] = k;
			}
		}
	}


	if (cl.teamfortress) {
		for (i = 0; i < scoreboardteams; i++) {
			float frags = (float) teams[i].frags / teams[i].players;

			if (frags != (int) frags)
				return;

			for (j = 0; j < MAX_CLIENTS; j++) {
				s = &cl.players[j];
				if (!s->name[0] || s->spectator || playertoteam[j] != i)
					continue;

				if (s->frags != frags)
					return;
			}
		}


		for (i = 0; i < scoreboardteams; i++)
			teams[i].frags /= teams[i].players;
	}
}



static int Sbar_SortTeamsAndFrags_Compare(int a, int b) {
	int team_comp, frag_comp;
	char *team_one, *team_two;
	player_info_t *p1, *p2;

	p1 = &cl.players[a];
	p2 = &cl.players[b];

	if (p1->spectator)
		return p2->spectator ? strcasecmp(p1->name, p2->name) : 1;
	else if (p2->spectator)
		return -1;

	team_one = cl.teamfortress ? Utils_TF_ColorToTeam(p1->real_bottomcolor) : p1->team;
	team_two = cl.teamfortress ? Utils_TF_ColorToTeam(p2->real_bottomcolor) : p2->team;

	if ((team_comp = strcmp(team_one, team_two))) {
		int i;
		team_t *t1 = NULL, *t2 = NULL;

		for (i = 0; i < scoreboardteams; i++) {
			if (!strcmp(team_one, teams[i].team)) {
				t1 = &teams[i];
				break;
			}
		}
		for (i = 0; i < scoreboardteams; i++) {
			if (!strcmp(team_two, teams[i].team)) {
				t2 = &teams[i];
				break;
			}
		}
		return (t1 && t2 && t1->frags != t2->frags) ? (t2->frags - t1->frags) : team_comp;
	} else {
		return (frag_comp = p2->frags - p1->frags) ? frag_comp : strcasecmp(p1->name, p2->name);
	}
}

static void Sbar_SortTeamsAndFrags(qbool specs) {
	int i, j, k;
	qbool real_teamplay;

	real_teamplay = cl.teamplay && (TP_CountPlayers() > 2);

	if (!real_teamplay || !scr_scoreboard_teamsort.value) {
		Sbar_SortFrags(specs);
		return;
	}

	scoreboardlines = 0;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].name[0] && (specs || !cl.players[i].spectator)) {
			fragsort[scoreboardlines++] = i;
			if (cl.players[i].spectator)
				cl.players[i].frags = -999;
		}
	}

	Sbar_SortTeams();

	for (i = 0; i < scoreboardlines; i++) {
		for (j = 0; j < scoreboardlines - 1 - i ; j++) {
			if (Sbar_SortTeamsAndFrags_Compare(fragsort[j], fragsort[j + 1]) > 0) {
				k = fragsort[j];
				fragsort[j] = fragsort[j + 1];
				fragsort[j + 1] = k;
			}
		}
	}
}



/********************************* INVENTORY *********************************/

static void Sbar_DrawInventory (void) {
	int i, flashon;
	char num[6];
	float time;
	qbool headsup, hudswap;

	headsup = !Sbar_IsStandardBar();
	hudswap = cl_hudswap.value; // Get that nasty float out :)

	if (!headsup)
	{
		Draw_TileClear (sbar_xofs, vid.height - sb_lines, sbar_xofs + 320, 24);
		Sbar_DrawPic (0, -24, sb_ibar);
	}

	// weapons
	if (sbar_drawguns.value)    // kazik
	{
		for (i = 0; i < 7; i++) {
			if (cl.stats[STAT_ITEMS] & (IT_SHOTGUN << i) ) {
				time = cl.item_gettime[i];
				flashon = (int)((cl.time - time) * 10);
				if (flashon < 0)
					flashon = 0;
				if (flashon >= 10)
					flashon = (cl.stats[STAT_ACTIVEWEAPON] == (IT_SHOTGUN << i) ) ? 1 : 0;
				else
					flashon = (flashon % 5) + 2;

				if (headsup) {
					if (i || vid.height > 200)
						Sbar_DrawSubPic (hudswap ? 0 : vid.width - 24,-68 - (7 - i) * 16 , sb_weapons[flashon][i], 0, 0, 24, 16);

				} else {
					Sbar_DrawPic (i * 24, -16, sb_weapons[flashon][i]);
				}

				if (flashon > 1)
					sb_updates = 0;		// force update to remove flash
			}
		}
	}
	// ammo counts
	if (sbar_drawammocounts.value)  // kazik
	{
		for (i = 0; i < 4; i++) {
			snprintf (num, sizeof(num), "%3i", cl.stats[STAT_SHELLS + i]);
			if (headsup) {
				Sbar_DrawSubPic(hudswap ? 0 : vid.width - 42, -24 - (4 - i) * 11, sb_ibar, 3 + (i * 48), 0, 42, 11);
				if (num[0] != ' ')
					Draw_Character (hudswap ? 7: vid.width - 35, vid.height - SBAR_HEIGHT - 24 - (4 - i) * 11, 18 + num[0] - '0');
				if (num[1] != ' ')
					Draw_Character (hudswap ? 15: vid.width - 27, vid.height - SBAR_HEIGHT - 24 - (4 - i) * 11, 18 + num[1] - '0');
				if (num[2] != ' ')
					Draw_Character (hudswap ? 23: vid.width - 19, vid.height - SBAR_HEIGHT - 24 - (4 - i) * 11, 18 + num[2] - '0');
			} else {
				if (num[0] != ' ')
					Sbar_DrawCharacter ((6 * i + 1) * 8 - 2, -24, 18 + num[0] - '0');
				if (num[1] != ' ')
					Sbar_DrawCharacter ((6 * i + 2) * 8 - 2, -24, 18 + num[1] - '0');
				if (num[2] != ' ')
					Sbar_DrawCharacter ((6 * i + 3) * 8 - 2, -24, 18 + num[2] - '0');
			}
		}
	}
	flashon = 0;
	// items
	if (sbar_drawitems.value)   // kazik
	{
		for (i = 0; i < 6; i++) {
			if (cl.stats[STAT_ITEMS] & (1 << (17 + i))) {
				time = cl.item_gettime[17 + i];
				if (time &&	time > cl.time - 2 && flashon)
					// flash frame
					sb_updates = 0;
				else
					Sbar_DrawPic (192 + i * 16, -16, sb_items[i]);

				if (time &&	time > cl.time - 2)
					sb_updates = 0;
			}
		}
	}

	// sigils
	if (sbar_drawsigils.value)  // kazik
	{
		for (i = 0; i < 4; i++) {
			if (cl.stats[STAT_ITEMS] & (1 << (28 + i)))	{
				time = cl.item_gettime[28 + i];
				if (time &&	time > cl.time - 2 && flashon )
					// flash frame
					sb_updates = 0;
				else
					Sbar_DrawPic (320 - 32 + i * 8, -16, sb_sigil[i]);

				if (time &&	time > cl.time - 2)
					sb_updates = 0;
			}
		}
	}
}

/************************************ HUD ************************************/

static void Sbar_DrawFrags_DrawCell(int x, int y, int topcolor, int bottomcolor, int frags, int brackets) {
	char num[4];

	// draw background
	Draw_Fill (sbar_xofs + x * 8 + 10, y, 28, 4, Sbar_ColorForMap (topcolor));
	Draw_Fill (sbar_xofs + x * 8 + 10, y + 4, 28, 3, Sbar_ColorForMap (bottomcolor));

	// draw number
	snprintf (num, sizeof(num), "%3i", frags);

	Sbar_DrawCharacter ((x + 1) * 8 , -24, num[0]);
	Sbar_DrawCharacter ((x + 2) * 8 , -24, num[1]);
	Sbar_DrawCharacter ((x + 3) * 8 , -24, num[2]);

	if (brackets) {
		Sbar_DrawCharacter (x * 8 + 2, -24, 16);
		Sbar_DrawCharacter ((x + 4) * 8 - 4, -24, 17);
	}
}

static void Sbar_DrawFrags (void) {
	int i, k, l, top, bottom, x, y, mynum, myteam = -1;
	player_info_t *s;
	team_t *tm;
	qbool drawn_self = false, drawn_self_team = false;

	if (!scr_drawHFrags.value)
		return;

	x = 23;
	y = vid.height - SBAR_HEIGHT - 23;

	mynum = Sbar_PlayerNum();

	if (!cl.teamplay || scr_drawHFrags.value == 1) {

		Sbar_SortFrags (false);
		l = min(scoreboardlines, 4);

		for (i = 0; i < l; i++) {
			if (i + 1 == l && !drawn_self && !Sbar_IsSpectator(mynum))
				k = mynum;
			else
				k = fragsort[i];

			s = &cl.players[k];

			if (!s->name[0] || s->spectator)
				continue;

			top = scr_scoreboard_forcecolors.value ? s->topcolor : s->real_topcolor;
			bottom = scr_scoreboard_forcecolors.value ? s->bottomcolor : s->real_bottomcolor;
			top = bound(0, top, 13);
			bottom = bound(0, bottom, 13);

			Sbar_DrawFrags_DrawCell(x, y, top, bottom, s->frags, k == mynum);

			if (k == mynum)
				drawn_self = true;

			x += 4;
		}
	} else {
		Sbar_SortTeams();

		for (i = 0; i < scoreboardteams; i++) {
			if (teams[i].myteam) {
				myteam = i;
				break;
			}
		}

		l = min(scoreboardteams, 4);
		for (i = 0; i < l; i++) {
			if (myteam != -1 && i + 1 == l && !drawn_self_team)
				k = myteam;
			else
				k = teamsort[i];

			tm = &teams[k];
			Sbar_DrawFrags_DrawCell(x, y, tm->topcolor, tm->bottomcolor, tm->frags, tm->myteam);

			if (k == myteam)
				drawn_self_team = true;

			x += 4;
		}

		if (scr_drawHFrags.value != 3 && l <= 2 && !Sbar_IsSpectator(mynum)) {

			x += 4 * (3 - l);

			s = &cl.players[mynum];

			top = scr_scoreboard_forcecolors.value ? s->topcolor : s->real_topcolor;
			bottom = scr_scoreboard_forcecolors.value ? s->bottomcolor : s->real_bottomcolor;
			top = bound(0, top, 13);
			bottom = bound(0, bottom, 13);

			Sbar_DrawFrags_DrawCell(x, y, top, bottom, s->frags, 1);
		}
	}
}

static void Sbar_DrawFace (void) {
	int f, anim;

	if ( (cl.stats[STAT_ITEMS] & (IT_INVISIBILITY | IT_INVULNERABILITY) )
			== (IT_INVISIBILITY | IT_INVULNERABILITY) )	{
		Sbar_DrawPic (112, 0, sb_face_invis_invuln);
		return;
	}
	if (cl.stats[STAT_ITEMS] & IT_QUAD) {
		Sbar_DrawPic (112, 0, sb_face_quad );
		return;
	}
	if (cl.stats[STAT_ITEMS] & IT_INVISIBILITY) {
		Sbar_DrawPic (112, 0, sb_face_invis );
		return;
	}
	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) {
		Sbar_DrawPic (112, 0, sb_face_invuln);
		return;
	}

	f = cl.stats[STAT_HEALTH] / 20;
	f = bound (0, f, 4);

	if (cl.time <= cl.faceanimtime)	{
		anim = 1;
		sb_updates = 0;		// make sure the anim gets drawn over
	} else {
		anim = 0;
	}
	Sbar_DrawPic (112, 0, sb_faces[f][anim]);
}


void Draw_AMFStatLoss (int stat, hud_t* hud);

static void Sbar_DrawNormal (void) {
	if (Sbar_IsStandardBar())
		Sbar_DrawPic (0, 0, sb_sbar);

	// armor
	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY)	{
		if (sbar_drawarmor.value)
			Sbar_DrawNum (24, 0, 666, 3, 1);
		if (sbar_drawarmoricon.value)
			Sbar_DrawPic (0, 0, draw_disc);
	} else {
		if (sbar_drawarmor.value)
			Sbar_DrawNum (24, 0, cl.stats[STAT_ARMOR], 3, cl.stats[STAT_ARMOR] <= 25);
		if (sbar_drawarmoricon.value)
		{
			if (cl.stats[STAT_ITEMS] & IT_ARMOR3)
				Sbar_DrawPic (0, 0, sb_armor[2]);
			else if (cl.stats[STAT_ITEMS] & IT_ARMOR2)
				Sbar_DrawPic (0, 0, sb_armor[1]);
			else if (cl.stats[STAT_ITEMS] & IT_ARMOR1)
				Sbar_DrawPic (0, 0, sb_armor[0]);
		}

		if (amf_stat_loss.value)
			Draw_AMFStatLoss (STAT_ARMOR, NULL);
	}

	// face
	if (sbar_drawfaceicon.value)
		Sbar_DrawFace ();

	// health
	if (sbar_drawhealth.value)
	{
		Sbar_DrawNum (136, 0, cl.stats[STAT_HEALTH], 3, cl.stats[STAT_HEALTH] <= 25);
		if (amf_stat_loss.value)
			Draw_AMFStatLoss (STAT_HEALTH, NULL);
	}

	// ammo icon
	if (sbar_drawammoicon.value)    // kazik
	{
		if (cl.stats[STAT_ITEMS] & IT_SHELLS)
			Sbar_DrawPic (224, 0, sb_ammo[0]);
		else if (cl.stats[STAT_ITEMS] & IT_NAILS)
			Sbar_DrawPic (224, 0, sb_ammo[1]);
		else if (cl.stats[STAT_ITEMS] & IT_ROCKETS)
			Sbar_DrawPic (224, 0, sb_ammo[2]);
		else if (cl.stats[STAT_ITEMS] & IT_CELLS)
			Sbar_DrawPic (224, 0, sb_ammo[3]);
	}
	if (sbar_drawammo.value)
		Sbar_DrawNum (248, 0, cl.stats[STAT_AMMO], 3, cl.stats[STAT_AMMO] <= 10);
}

static void Sbar_DrawCompact_WithIcons(void) {
	int i, align, old_sbar_xofs;
	char str[4];

	if (Sbar_IsStandardBar())
		Sbar_DrawPic (0, 0, sb_sbar);

	old_sbar_xofs = sbar_xofs;
	sbar_xofs = scr_centerSbar.value ? (vid.width - 158) >> 1: 0;

	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY)
		Sbar_DrawNum (2, 0, 666, 3, 1);
	else
		Sbar_DrawNum (2, 0, cl.stats[STAT_ARMOR], 3, cl.stats[STAT_ARMOR] <= 25);

	if (cl.stats[STAT_ITEMS] & IT_ARMOR3)
		Sbar_DrawPic (-24, 0, sb_armor[2]);
	else if (cl.stats[STAT_ITEMS] & IT_ARMOR2)
		Sbar_DrawPic (-24, 0, sb_armor[1]);
	else if (cl.stats[STAT_ITEMS] & IT_ARMOR1)
		Sbar_DrawPic (-24, 0, sb_armor[0]);

	Sbar_DrawNum (86, 0, cl.stats[STAT_HEALTH], 3, cl.stats[STAT_HEALTH] <= 25);

	align = scr_compactHudAlign.value ? 1 : 0;
	for (i = 0; i < 4; i++) {
		snprintf(str, sizeof(str), "%d", cl.stats[STAT_SHELLS + i]);
		if (cl.stats[STAT_SHELLS + i] < 5)
			Sbar_DrawAltString(align * 8 * (3 - strlen(str)) + 24 + 32 * i, -12, str);
		else
			Sbar_DrawString(align * 8 * (3 - strlen(str)) + 24 + 32 * i, -12, str);
	}
	for (i = 0; i < 7; i++) {
		if (cl.stats[STAT_ITEMS] & (IT_SHOTGUN << i) ) {
			if (cl.stats[STAT_ACTIVEWEAPON] == (IT_SHOTGUN << i))
				Sbar_DrawPic (align * 8 + 18 + 16 * i, -28, sb_weapons[1][i]);
			else
				Sbar_DrawPic (align * 8 + 18 + 16 * i, -28, sb_weapons[0][i]);
		}
	}
	sbar_xofs = old_sbar_xofs;
}

static void Sbar_DrawCompact(void) {
	int i, align, old_sbar_xofs;
	static char *weapons[7] = {"sg", "bs", "ng", "sn", "gl", "rl", "lg"};
	char str[4];

	if (Sbar_IsStandardBar())
		Sbar_DrawPic (0, 0, sb_sbar);

	old_sbar_xofs = sbar_xofs;
	sbar_xofs = scr_centerSbar.value ? (vid.width - 306) >> 1: 0;

	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY)
		Sbar_DrawNum (2, 0, 666, 3, 1);
	else
		Sbar_DrawNum (2, 0, cl.stats[STAT_ARMOR], 3, cl.stats[STAT_ARMOR] <= 25);

	Sbar_DrawNum (86, 0, cl.stats[STAT_HEALTH], 3, cl.stats[STAT_HEALTH] <= 25);

	align = scr_compactHudAlign.value ? 1 : 0;
	for (i = 0; i < 4; i++) {
		snprintf(str, sizeof(str), "%d", cl.stats[STAT_SHELLS + i]);
		if (cl.stats[STAT_SHELLS + i] < 5)
			Sbar_DrawAltString(align * 8 * (3 - strlen(str)) + 174 + 32 * i, 3, str);
		else
			Sbar_DrawString(align * 8 * (3 - strlen(str)) + 174 + 32 * i, 3, str);
	}
	for (i = 0; i < 7; i++) {
		if (cl.stats[STAT_ITEMS] & (IT_SHOTGUN << i)) {
			if (cl.stats[STAT_ACTIVEWEAPON] == (IT_SHOTGUN << i))
				Sbar_DrawAltString(166 + 5 * 4 * i, 14, weapons[i]);
			else
				Sbar_DrawString(166 + 5 * 4 * i, 14, weapons[i]);
		}
	}
	sbar_xofs = old_sbar_xofs;
}

static void Sbar_DrawCompact_TF(void) {
	int i, align, old_sbar_xofs;
	char str[4];

	if (Sbar_IsStandardBar())
		Sbar_DrawPic (0, 0, sb_sbar);

	old_sbar_xofs = sbar_xofs;
	sbar_xofs = scr_centerSbar.value ? (vid.width - 222) >> 1: 0;

	align = scr_compactHudAlign.value ? 1 : 0;
	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY)
		Sbar_DrawNum (2, 0, 666, 3, 1);
	else
		Sbar_DrawNum (2, 0, cl.stats[STAT_ARMOR], 3, cl.stats[STAT_ARMOR] <= 25);
	Sbar_DrawNum (86, 0, cl.stats[STAT_HEALTH], 3, cl.stats[STAT_HEALTH] <= 25);
	for (i = 0; i < 4; i++) {
		snprintf(str, sizeof(str), "%d", cl.stats[STAT_SHELLS + i]);
		if (cl.stats[STAT_SHELLS + i] < 5)
			Sbar_DrawAltString(align * 8 * (3 - strlen(str)) + 166 + 32 * (i % 2), i >= 2 ? 14 : 3, str);
		else
			Sbar_DrawString(align * 8 * (3 - strlen(str)) + 166 + 32 * (i % 2), i >= 2 ? 14 : 3, str);
	}
	sbar_xofs = old_sbar_xofs;
}

static void Sbar_DrawCompact_Bare (void) {
	int old_sbar_xofs;

	if (Sbar_IsStandardBar())
		Sbar_DrawPic (0, 0, sb_sbar);

	old_sbar_xofs = sbar_xofs;
	sbar_xofs = scr_centerSbar.value ? (vid.width - 158) >> 1: 0;

	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY)
		Sbar_DrawNum (2, 0, 666, 3, 1);
	else
		Sbar_DrawNum (2, 0, cl.stats[STAT_ARMOR], 3, cl.stats[STAT_ARMOR] <= 25);
	Sbar_DrawNum (86, 0, cl.stats[STAT_HEALTH], 3, cl.stats[STAT_HEALTH] <= 25);
	sbar_xofs = old_sbar_xofs;
}

/******************************** SCOREBOARD ********************************/

/*
   ===============
   Sbar_SoloScoreboard -- johnfitz -- new layout
   ===============
   */
void Sbar_SoloScoreboard (void)
{
	char	str[256];
	int		len;

	sprintf (str,"Kills: %i/%i", cl.stats[STAT_MONSTERS], cl.stats[STAT_TOTALMONSTERS]);
	Sbar_DrawString (8, 12, str);

	sprintf (str,"Secrets: %i/%i", cl.stats[STAT_SECRETS], cl.stats[STAT_TOTALSECRETS]);
	Sbar_DrawString (312 - strlen(str)*8, 12, str);

	sprintf (str,"skill %i", (int)(skill.value + 0.5));
	Sbar_DrawString (160 - strlen(str)*4, 12, str);

	strlcpy(str, cl.levelname, sizeof(str));
	strlcat(str, " (", sizeof(str));
	strlcat(str, host_mapname.string, sizeof(str));
	strlcat(str, ")", sizeof(str));
	len = strlen (str);
	Sbar_DrawString (160 - len*4, 4, str);
}

#define SCOREBOARD_LASTROW		(vid.height - 34)

#define	RANK_WIDTH_BASICSTATS	(11 * 8)
#define	RANK_WIDTH_TEAMSTATS	(4 * 8)
#define	RANK_WIDTH_TCHSTATS		(5 * 8)
#define	RANK_WIDTH_CAPSTATS		(5 * 8)

#define RANK_WIDTH_DM				(-8 + 168 + (MAX_SCOREBOARDNAME * 8))
#define RANK_WIDTH_TEAM				(-8 + 208 + (MAX_SCOREBOARDNAME * 8))

#define SCOREBOARD_ALPHA			(0.5 * bound(0, scr_scoreboard_fillalpha.value, 1))
#define SCOREBOARD_HEADINGALPHA		(bound(0, scr_scoreboard_fillalpha.value, 1))

static qbool Sbar_ShowTeamKills(void)
{
	if (cl.teamfortress) {
		return ((cl.teamplay & 21) != 21); // 21 = (1)Teamplay On + (4)Team-members take No damage from direct fire + (16)Team-members take No damage from area-affect weaponry, so its like teamplay is off??
	}
	else {
		// in teamplay 3 it's not possible to make teamkills
		return (cl.teamplay != 3);
	}
}

static void Sbar_DeathmatchOverlay (int start) {
	int stats_basic, stats_team, stats_touches, stats_caps, playerstats[7];
	int scoreboardsize, colors_thickness, statswidth, stats_xoffset = 0;
	int i, d, k, top, bottom, x, y, xofs, total, p, skip = 10, fragsint;
	int rank_width, leftover, startx, tempx, mynum;
	char num[12], scorerow[64], team[5], name[MAX_SCOREBOARDNAME];
	char myminutes[11];
	char fragsstr[10];
	char *color;
	char *kill_color;
	char *death_color;
	char *tk_color;
	int scr_scoreboard_drawfps;
	player_info_t *s;
	mpic_t *pic;

	scr_scoreboard_drawfps = show_fps2.value && !cl.intermission && !cls.mvdplayback;
	if (!start && hud_faderankings.value)
		Draw_FadeScreen(hud_faderankings.value);

#ifndef CLIENTONLY
	// FIXME
	// check number of connections instead?
	if (cl.gametype == GAME_COOP && com_serveractive && !coop.value)
		return;
#endif

	if (cls.state == ca_active && !cls.demoplayback) {
		// request new ping times every two seconds
		if (cls.realtime - cl.last_ping_request >= 2) {
			cl.last_ping_request = cls.realtime;
			MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
			SZ_Print (&cls.netchan.message, "pings");
		}
	}

	scr_copyeverything = 1;
	scr_fullupdate = 0;

	mynum = Sbar_PlayerNum();

	rank_width = (cl.teamplay ? RANK_WIDTH_TEAM : RANK_WIDTH_DM);


	statswidth = 0;
	stats_basic = stats_team = stats_touches = stats_caps = 0;
	if (scr_scoreboard_showfrags.value && Stats_IsActive()) {
		if (rank_width + statswidth + RANK_WIDTH_BASICSTATS < vid.width - 16) {
			statswidth += RANK_WIDTH_BASICSTATS;
			stats_basic++;
			if (cl.teamplay) {
				if (rank_width + statswidth + RANK_WIDTH_TEAMSTATS < vid.width - 16) {
					if (Sbar_ShowTeamKills()) {
						statswidth += RANK_WIDTH_TEAMSTATS;
						stats_team++;
					}
				}
				if (cl.teamfortress && Stats_IsFlagsParsed()) {
					if (rank_width + statswidth + RANK_WIDTH_TCHSTATS < vid.width - 16) {
						statswidth += RANK_WIDTH_TCHSTATS;
						stats_touches++;
					}
					if (rank_width + statswidth + RANK_WIDTH_CAPSTATS < vid.width - 16) {
						statswidth += RANK_WIDTH_CAPSTATS;
						stats_caps++;
					}
				}
			}
		}
	}
	rank_width += statswidth;



	leftover = rank_width;
	rank_width = bound(0, rank_width, vid.width - 16);
	leftover = max(0, leftover - rank_width);
	if (hud_centerranking.value)
		xofs = ((vid.width - rank_width) >> 1) + hud_rankingpos_x.value;
	else
		xofs = hud_rankingpos_x.value;

	if (start)
		y = start;
	else {
		y = hud_rankingpos_y.value;
		if (y < 0  ||  y > vid.height/2)
			y = 0;
	}

	if (!start) {
		if (scr_scoreboard_drawtitle.value) {
			pic = Draw_CachePic ("gfx/ranking.lmp");
			Draw_Pic (xofs + (rank_width - pic->width) / 2, y, pic);
			start = 36 + hud_rankingpos_y.value;
		} else {
			start = 12 + hud_rankingpos_y.value;
		}
	}

	y = start;

	if (!scr_scoreboard_borderless.value)
		Draw_Fill (xofs - 1, y - 9, rank_width + 2, 1, 0);						//Border - Top
	Draw_AlphaFill (xofs, y - 8, rank_width, 9, 1, SCOREBOARD_HEADINGALPHA);	//Draw heading row

	Draw_String(xofs + 1, y - 8, cl.teamplay ? (scr_scoreboard_drawfps ? " ping pl  fps frags team name" : " ping pl time frags team name") : (scr_scoreboard_drawfps ? " ping pl  fps frags name" : " ping pl time frags name"));


	if (statswidth) {
		char temp[32] = {0};

		if (stats_team)
			strlcat (temp, "kills tks dths", sizeof (temp));
		else
			strlcat(temp, "kills dths", sizeof (temp));
		if (stats_touches)
			strlcat(temp, " tchs", sizeof (temp));
		if (stats_caps)
			strlcat(temp, " caps", sizeof (temp));

		stats_xoffset = (cl.teamplay ? 41 * 8 : 36 * 8);
		Draw_String(xofs + 1 + stats_xoffset, y - 8, temp);
	}


	Draw_Fill (xofs, y + 1, rank_width, 1, 0);	//Border - Top (under header)
	y += 2;												//dont go over the black border, move the rest down
	if (!scr_scoreboard_borderless.value) {
		Draw_Fill (xofs - 1, y - 10, 1, 10, 0);						//Border - Left
		Draw_Fill (xofs - 1 + rank_width + 1, y - 10, 1, 10, 0);	//Border - Right
	}
	startx = x = xofs + 8;


	tempx = xofs + rank_width - MAX_SCOREBOARDNAME * 8 - (48 + statswidth) + leftover;

	Sbar_SortTeamsAndFrags (true);	// scores

	for (i = scoreboardsize = 0; i < scoreboardlines; i++) {
		k = fragsort[i];
		s = &cl.players[k];

		if (s->name[0])
			scoreboardsize++;
	}

	if (y + (scoreboardsize - 1) * skip > SCOREBOARD_LASTROW) {
		colors_thickness = 3;
		skip = 8;
	} else {
		colors_thickness = 4;
	}

	for (i = 0; i < scoreboardlines && y <= SCOREBOARD_LASTROW; i++) {
		color_t background;
		float alpha;
		byte c;

		k = fragsort[i];
		s = &cl.players[k];

		if (!s->name[0])
			continue;

		//render the main background transparencies behind players row
		top = scr_scoreboard_forcecolors.value ? s->topcolor : s->real_topcolor;
		bottom = scr_scoreboard_forcecolors.value ? s->bottomcolor : s->real_bottomcolor;

		if (k == mynum)
		{
			alpha = 1.7 * SCOREBOARD_ALPHA;
			alpha = min(alpha, 0.75);
		}
		else
		{
			alpha = SCOREBOARD_ALPHA;
		}

		if (
				!cl.teamplay || s->spectator ||	!scr_scoreboard_fillcolored.value
				|| (scr_scoreboard_fillcolored.value == 2 && !scr_scoreboard_teamsort.value)
		   )
		{
			c = 2;
		}
		else
		{
			c = Sbar_ColorForMap(bottom);
		}

		if (S_Voip_Speaking (k)) {
			background = RGBA_TO_COLOR (0, 255, 0, (byte)(alpha * 255));
		}
		else {
			background = RGBA_TO_COLOR (host_basepal[c * 3], host_basepal[c * 3 + 1], host_basepal[c * 3 + 2], (byte)(alpha * 255));
		}

		Draw_AlphaFillRGB (xofs, y, rank_width, skip, background);

		if (!scr_scoreboard_borderless.value) {
			Draw_Fill (xofs - 1, y, 1, skip, 0);					//Border - Left
			Draw_Fill (xofs - 1 + rank_width + 1, y, 1, skip, 0);	//Border - Right
		}

		// draw ping
		p = s->ping;
		if (p < 0 || p > 999)
			p = 999;

		snprintf (num, sizeof(num), "&cAAD%4i", p);
		Draw_ColoredString(x, y, num, 0);
		x += 32; // move it forward, ready to print next column

		// draw pl
		p = s->pl;
		if (p == 0) {
			// 0 - white
			snprintf (num, sizeof(num), "%3i", p);
			Draw_ColoredString (x, y, num, 0);
		}
		else if (p < 3) {
			// 1-2 - yellow
			snprintf (num, sizeof(num), "&cdd2%3i", p);
			Draw_ColoredString (x, y, num, 1);
		}
		else if (p < 6) {
			// 3-5 orange
			snprintf (num, sizeof(num), "&cf50%3i", p);
			Draw_ColoredString (x, y, num, 1);
		}
		else {	// 6+ - red
			snprintf (num, sizeof(num), "&cf00%3i", p);
			Draw_ColoredString (x, y, num, 1);
		}

		x += 32;

		// draw time
		total = (cl.intermission ? cl.completed_time : cls.demoplayback ? cls.demotime : cls.realtime) - s->entertime;
		total = (int) total / 60;
		total = bound(0, total, 999); // limit to 3 symbols int

		if (scr_scoreboard_afk.integer && (Q_atoi(Info_ValueForKey(s->userinfo, "chat")) & CIF_AFK)) {
			if (scr_scoreboard_afk_style.integer == 2) {
				snprintf(myminutes, sizeof(myminutes), "&cf11%3i&r", total);
			} else {
				snprintf(myminutes, sizeof(myminutes), "&cf11afk&r");
			}
		} else {
			snprintf (myminutes, sizeof (myminutes), "%3i", total);
		}

		if (scr_scoreboard_drawfps) {
			if (s->last_fps > 0 && !s->spectator) {
				snprintf (myminutes, sizeof (myminutes), "%3i", bound(0, s->last_fps, 999)); // limit to 3 symbols int
				if (s->last_fps < 70) {
					for (d=0; d < strlen(myminutes); d++)
						myminutes[d] ^= 128;
				}
			}
			else {
				snprintf (myminutes, sizeof (myminutes), "   ");
			}
		}

		// draw spectator
		if (s->spectator) {
			snprintf (scorerow, sizeof(scorerow), " %s", myminutes);
			Draw_String (x, y, scorerow); // draw time

			x += 8*5; // move "spectator" 5 symbols right, so time column is not occupied
			if (cl.teamplay) // use columns frags and team
				Draw_ColoredString (x, y, scr_scoreboard_spectator_name.string, 0);
			else {// use only frags column
#define SHORT_SPECTATOR_NAME_LEN 5 // if it's not teamplay, there is only room for 4 characters here
				char short_spectator_name[SHORT_SPECTATOR_NAME_LEN];
				strlcpy(short_spectator_name, scr_scoreboard_spectator_name.string, SHORT_SPECTATOR_NAME_LEN);
				Draw_ColoredString (x, y, short_spectator_name, 0);
			}

			x += cl.teamplay ? 88 : 48; // move across to print the name

			strlcpy(name, s->name, sizeof(name));
			if (leftover > 0) {
				int truncate = (leftover / 8) + (leftover % 8 ? 1 : 0);

				truncate = bound(0, truncate, sizeof(name) - 1);
				name[sizeof(name) - 1 - truncate] = 0;
			}
			Draw_String (x, y, name);	// draw name

			y += skip;
			x = startx;
			continue;
		}

		// print the shirt/pants colour bars
		Draw_Fill (cl.teamplay ? tempx - 40 : tempx, y + 4 - colors_thickness, 40, colors_thickness, Sbar_ColorForMap (top));
		Draw_Fill (cl.teamplay ? tempx - 40 : tempx, y + 4, 40, 4, Sbar_ColorForMap (bottom));

		// name
		strlcpy(name, s->name, sizeof(name));
		if (leftover > 0) {
			int truncate = (leftover / 8) + (leftover % 8 ? 1 : 0);

			truncate = bound(0, truncate, sizeof(name) - 1);
			name[sizeof(name) - 1 - truncate] = 0;
		}

		// frags
		fragsint = bound(-999, s->frags, 9999); // limit to 4 symbols int
		snprintf (fragsstr, sizeof(fragsstr), "%s%3i", (fragsint < 1000 && fragsint > -100) ? " " : "", fragsint);

		// team
		if (cl.teamplay) {
			strlcpy  (team, s->team, sizeof(team));
			snprintf (scorerow, sizeof(scorerow), " %s %4.4s  %-4s %s", myminutes, fragsstr, team, name);
		} else {
			snprintf (scorerow, sizeof(scorerow), " %s %4.4s  %s", myminutes, fragsstr, name);
		}
		Draw_String (x, y, scorerow);

		if (statswidth) {
			Stats_GetBasicStats(s - cl.players, playerstats);
			if (stats_touches || stats_caps)
				Stats_GetFlagStats(s - cl.players, playerstats + 4);

			scorerow[0] = 0;

			kill_color = playerstats[0] == 0 ? "FFF" : scr_scoreboard_kill_color.string;
			death_color = playerstats[1] == 0 ? "FFF" : scr_scoreboard_death_color.string;
			tk_color = playerstats[2] == 0 ? "FFF" : scr_scoreboard_tk_color.string;

			if (stats_team)
				snprintf (scorerow, sizeof(scorerow), " &c%s%3i  &c%s%3i &c%s%3i ", kill_color, playerstats[0], tk_color, playerstats[2], death_color, playerstats[1]);
			else
				snprintf (scorerow, sizeof(scorerow), " &c%s%3i  &c%s%3i ", kill_color, playerstats[0], death_color, playerstats[1]);

			if (stats_touches) // flag touches
			{
				if (playerstats[4] < 1) { color = "FFF"; }		// 0 flag touches white
				else if (playerstats[4] < 2) { color = "FF0"; } // 1 flag touches yellow
				else if (playerstats[4] < 5) { color = "F50"; } // 2-4 flag touches orange
				else if (playerstats[4] < 10) { color = "B3B"; }// 5-9 flag touches pink
				else { color = "0F0"; }							// >9 flag touches green
				strlcat (scorerow, va("  &c%s%2i ", color, playerstats[4]), sizeof (scorerow));
			}

			if (stats_caps) // flag captures
			{
				if (playerstats[6] < 1) { color = "FFF"; }		// 0 caps white
				else if (playerstats[6] < 2) { color = "FF0"; } // 1 cap yellow
				else if (playerstats[6] < 5) { color = "F50"; } // 2-4 caps orange
				else if (playerstats[6] < 10) { color = "B3B"; }// 5-9 caps pink
				else { color = "0F0"; }							// >9 caps green
				strlcat (scorerow, va("  &c%s%2i ", color, playerstats[6]), sizeof (scorerow));
			}

			Draw_ColoredString(x + stats_xoffset - 9 * 8, y, scorerow, 0);
		}

		if (Sbar_ShowScoreboardIndicator()) {
			if (k == mynum) {
				Draw_Character (x + 40 - ((fragsint < 1000 && fragsint > -100) ? 0 : 8), y, 16);
				Draw_Character (x + 40 + 32, y, 17);
			}
		}

		y += skip;
		x = startx;
	}

	if (!scr_scoreboard_borderless.value)
		Draw_Fill (xofs - 1, y - 1, rank_width + 2, 1, 0); //Border - Bottom
}

static void Sbar_TeamOverlay (void) {
	int i, k, x, y, xofs, plow, phigh, pavg, rank_width, skip = 10;
	char num[12], team[5];
	team_t *tm;
	mpic_t *pic;

	if (key_dest == key_console && ! SCR_TakingAutoScreenshot())
		return;

#ifndef CLIENTONLY
	// FIXME
	// check number of connections instead?
	if (cl.gametype == GAME_COOP && com_serveractive && !coop.value)
		return;
#endif

	if (!cl.teamplay) {
		Sbar_DeathmatchOverlay(0);
		return;
	}

	if (hud_faderankings.value)
		Draw_FadeScreen(hud_faderankings.value);

	scr_copyeverything = 1;
	scr_fullupdate = 0;

	rank_width = cl.teamplay ? RANK_WIDTH_TEAM : RANK_WIDTH_DM;
	rank_width = bound(0, rank_width, vid.width - 16);

	y = hud_rankingpos_y.value;
	if (y < 0  ||  y > vid.height/2)
		y = 0;


	if (hud_centerranking.value)
		xofs = ((vid.width - rank_width) >> 1) + hud_rankingpos_x.value;
	else
		xofs = hud_rankingpos_x.value;

	if (scr_scoreboard_drawtitle.value) {
		pic = Draw_CachePic ("gfx/ranking.lmp");
		Draw_Pic (xofs + (rank_width - pic->width) / 2, y, pic);
		y = 26 + hud_rankingpos_y.value;
	} else {
		y = 2 + hud_rankingpos_y.value;
	}

	if (!scr_scoreboard_borderless.value)
		Draw_Fill (xofs - 1, y + 1, rank_width + 2, 1, 0);							//Border - Top
	Draw_AlphaFill (xofs, y + 2, rank_width, skip, 1, SCOREBOARD_HEADINGALPHA);	//draw heading row
	Draw_Fill (xofs, y + 11, rank_width, 1, 0);							//Border - Top (under header)
	y += 2;																		//dont go over the black border, move the rest down
	if (!scr_scoreboard_borderless.value) {
		Draw_Fill (xofs - 1, y, 1, skip, 0);										//Border - Left
		Draw_Fill (xofs - 1 + rank_width + 1, y, 1, skip, 0);						//Border - Right
	}

	x = xofs + rank_width * 0.1 + 8;

	Draw_String(x, y, "low/avg/high team total players");

	y += 10;

	Sbar_SortTeams();		// sort the teams

	for (i = 0; i < scoreboardteams && y <= SCOREBOARD_LASTROW; i++)	{
		k = teamsort[i];
		tm = teams + k;

		Draw_AlphaFill (xofs, y, rank_width, skip, 2, SCOREBOARD_ALPHA);

		if (!scr_scoreboard_borderless.value) {
			Draw_Fill (xofs - 1, y, 1, skip, 0);						//Border - Left
			Draw_Fill (xofs - 1 + rank_width + 1, y, 1, skip, 0);		//Border - Right
		}

		// draw pings
		plow = tm->plow;
		if (plow < 0 || plow > 999)
			plow = 999;

		phigh = tm->phigh;
		if (phigh < 0 || phigh > 999)
			phigh = 999;

		pavg = tm->players ? tm->ptotal / tm->players : 999;
		if (pavg < 0 || pavg > 999)
			pavg = 999;

		snprintf (num, sizeof(num), "%3i/%3i/%3i", plow, pavg, phigh);
		Draw_String (x, y, num);

		// draw team
		strlcpy (team, tm->team, sizeof(team));
		Draw_String (x + 104, y, team);

		// draw total
		snprintf (num, sizeof(num), "%5i", tm->frags);
		Draw_String (x + 104 + 40, y, num);

		// draw players
		snprintf (num, sizeof(num), "%5i", tm->players);
		Draw_String (x + 104 + 88, y, num);

		if (Sbar_ShowScoreboardIndicator()) {
			if (tm->myteam) {
				Draw_Character (x + 104 - 8, y, 16);
				Draw_Character (x + 104 + 32, y, 17);
			}
		}

		y += skip;
	}

	if (!scr_scoreboard_borderless.value)
		Draw_Fill (xofs - 1, y - 1, rank_width + 2, 1, 0);              //Border - Bottom

	y += 14;
	Sbar_DeathmatchOverlay(y);
}


static void Sbar_MiniDeathmatchOverlay (void) {
	int i, k, top, bottom, x, y, mynum, numlines;
	char num[4 + 1], name[16 + 1], team[4 + 1];
	player_info_t *s;
	team_t *tm;

	if (!scr_drawVFrags.value)
		return;

	if (!sb_lines)
		return; // not enuff room

	scr_copyeverything = 1;
	scr_fullupdate = 0;

	mynum = Sbar_PlayerNum();

	x = 324;


	if (vid.width < 640 && cl.teamplay && scr_drawVFrags.value == 2)
		goto drawteams;

	// scores
	Sbar_SortFrags (false);

	if (!scoreboardlines)
		return; // no one there?

	// draw the text
	y = vid.height - sb_lines - 1;
	numlines = sb_lines / 8;
	if (numlines < 3)
		return; // not enough room

	// find us
	for (i = 0; i < scoreboardlines; i++)
		if (fragsort[i] == mynum)
			break;

	if (i == scoreboardlines)	// we're not there, we are probably a spectator, just display top
		i = 0;
	else						// figure out start
		i = i - numlines / 2;

	i = bound(0, i, scoreboardlines - numlines);

	for ( ; i < scoreboardlines && y < vid.height - 8 + 1; i++) {
		k = fragsort[i];
		s = &cl.players[k];

		if (!s->name[0])
			continue;

		top = scr_scoreboard_forcecolors.value ? s->topcolor : s->real_topcolor;
		bottom = scr_scoreboard_forcecolors.value ? s->bottomcolor : s->real_bottomcolor;

		Draw_Fill (x, y + 1, 40, 3, Sbar_ColorForMap (top));
		Draw_Fill (x, y + 4, 40, 4, Sbar_ColorForMap (bottom));

		// draw number
		snprintf (num, sizeof(num), "%3i", s->frags);

		Draw_Character (x + 8 , y, num[0]);
		Draw_Character (x + 16, y, num[1]);
		Draw_Character (x + 24, y, num[2]);

		if (Sbar_ShowScoreboardIndicator())
		{
			if (k == mynum)
			{
				Draw_Character (x, y, 16);
				Draw_Character (x + 32, y, 17);
			}
		}

		// team
		if (cl.teamplay) {
			strlcpy (team, s->team, sizeof(team));
			Draw_String (x + 48, y, team);
		}

		// draw name
		strlcpy (name, s->name, sizeof(name));
		if (cl.teamplay)
			Draw_String (x + 48 + 40, y, name);
		else
			Draw_String (x + 48, y, name);

		y += 8;
	}

	// draw teams if room
	if (vid.width < 640 || !cl.teamplay)
		return;

	Sbar_SortTeams();
	if (!scoreboardteams)
		return;

	// draw seperator
	x += 208;
	for (y = vid.height - sb_lines; y < vid.height - 6; y += 2)
		Draw_Character(x, y, 14);

	x += 16;

drawteams:
	Sbar_SortTeams();

	y = vid.height - sb_lines;
	for (i = 0; i < scoreboardteams && y < vid.height - 8 + 1; i++) {
		k = teamsort[i];
		tm = teams + k;

		//draw name
		strlcpy (team, tm->team, sizeof(team));
		Draw_String (x, y, team);

		// draw total
		Draw_Fill (x + 40, y + 1, 48, 3, Sbar_ColorForMap (tm->topcolor));
		Draw_Fill (x + 40, y + 4, 48, 4, Sbar_ColorForMap (tm->bottomcolor));
		snprintf (num, sizeof(num), "%4i", tm->frags);
		Draw_Character (x + 40 + 8 , y, num[0]);
		Draw_Character (x + 40 + 16, y, num[1]);
		Draw_Character (x + 40 + 24, y, num[2]);
		Draw_Character (x + 40 + 32, y, num[3]);

		if (tm->myteam) {
			Draw_Character (x + 40, y, 16);
			Draw_Character (x + 40 + 40, y, 17);
		}

		y += 8;
	}
}

static void Sbar_IntermissionNumber (int x, int y, int num, int digits, int color, int rightadjustify) {
	char str[12], *ptr;
	int l, frame;

	l = Sbar_itoa (num, str);
	ptr = str;
	if (l > digits)
		ptr += (l - digits);
	if (rightadjustify && l < digits)
		x += (digits - l) * 24;

	while (*ptr) {
		frame = (*ptr == '-') ? STAT_MINUS : *ptr -'0';
		Draw_TransPic (x, y, sb_nums[color][frame]);
		x += 24;
		ptr++;
	}
}

void Sbar_IntermissionOverlay (void) {
	mpic_t *pic;
	float time;
	int	dig, num, xofs;

	scr_copyeverything = 1;
	scr_fullupdate = 0;

	if (cl.gametype == GAME_DEATHMATCH)	{
		if (cl.teamplay && !sb_showscores)
			Sbar_TeamOverlay ();
		else
			Sbar_DeathmatchOverlay (0);
		return;
	}

	xofs = (vid.width - 320 ) >> 1;

	pic = Draw_CachePic ("gfx/complete.lmp");
	Draw_Pic (xofs + 64, 24, pic);

	pic = Draw_CachePic ("gfx/inter.lmp");
	Draw_TransPic (xofs, 56, pic);

	if (cl.servertime_works)
		time = cl.solo_completed_time;	// we know the exact time
	else
		time = cl.completed_time - cl.players[cl.playernum].entertime;	// use an estimate

	//time
	dig = time / 60;
	dig = max(dig, 0);
	Sbar_IntermissionNumber (xofs + 152, 64, dig, 3, 0, true);
	num = time - dig * 60;
	Draw_TransPic (xofs + 224, 64, sb_colon);
	Draw_TransPic (xofs + 248, 64, sb_nums[0][num / 10]);
	Draw_TransPic (xofs + 272, 64, sb_nums[0][num % 10]);

	//secrets
	Sbar_IntermissionNumber (xofs + 152, 104, cl.stats[STAT_SECRETS], 3, 0, true);
	Draw_TransPic (xofs + 224, 104, sb_slash);
	Sbar_IntermissionNumber (xofs + 248, 104, cl.stats[STAT_TOTALSECRETS], 3, 0, false);

	//monsters
	Sbar_IntermissionNumber (xofs + 152, 144, cl.stats[STAT_MONSTERS], 3, 0, true);
	Draw_TransPic (xofs + 224, 144, sb_slash);
	Sbar_IntermissionNumber (xofs + 248, 144, cl.stats[STAT_TOTALMONSTERS], 3, 0, false);
}

void Sbar_FinaleOverlay (void) {
	mpic_t *pic;

	scr_copyeverything = 1;

	pic = Draw_CachePic ("gfx/finale.lmp");
	Draw_TransPic ( (vid.width-pic->width)/2, 16, pic);
}

/********************************* INTERFACE *********************************/

static void Sbar_DrawTrackingString(void) {
	char st[512];
	extern cvar_t scr_tracking, scr_newHud;

	// If showing ammo on top of status, display higher up
	int y_coordinate = (Sbar_IsStandardBar() ? SBAR_HEIGHT - sb_lines : 0) - 8;

	if (sb_lines > 0 && scr_newHud.value != 1 && cl.spectator && autocam == CAM_TRACK)
	{
		strlcpy(st, scr_tracking.string, sizeof(st));

		Replace_In_String(st, sizeof(st), '%', 2, "n", cl.players[spec_track].name, "t", cl.teamplay ? cl.players[spec_track].team : "");

		// Multiview
		// Fix displaying "tracking .." for both players with inset on
		if (cl_multiview.value != 2 || !cls.mvdplayback)
		{
			Sbar_DrawString(0, y_coordinate, st);
		}
		else if (CL_MultiviewCurrentView() == 1 && cl_mvinset.value)
		{
			Sbar_DrawString(0, y_coordinate, st);
		}
	}
}

void Sbar_Draw(void) {
	qbool headsup;
	extern cvar_t scr_tracking, scr_spectatorMessage, scr_newHud;

	headsup = !Sbar_IsStandardBar();
	if (sb_updates >= vid.numpages && !headsup)
	{
		// Always show who we're tracking
		Sbar_DrawTrackingString();

		return;
	}

	scr_copyeverything = 1;

	sb_updates++;

	if (scr_centerSbar.value)
		sbar_xofs = (vid.width - 320) >> 1;
	else
		sbar_xofs = 0;

	// Top line. Do not show with +showscores
	if (sb_lines > 24 && scr_newHud.value != 1 && !sb_showscores && !sb_showteamscores) 
	{ 
		if (!cl.spectator || autocam == CAM_TRACK)
			Sbar_DrawInventory();

		if (cl.gametype == GAME_DEATHMATCH && (!headsup || vid.width < 512 || (vid.width >= 512 && scr_centerSbar.value )))
			Sbar_DrawFrags();
	}

	// main area
	if (sb_lines > 0 && scr_newHud.value != 1) {  // HUD -> hexum
		if (cl.spectator) {
			if (autocam != CAM_TRACK) {
				if (scr_spectatorMessage.value != 0) {
					Sbar_DrawPic (0, 0, sb_scorebar);
					Sbar_DrawString (160 - 7 * 8,4, "SPECTATOR MODE");
					Sbar_DrawString(160 - 14 * 8 + 4, 12, "Press [ATTACK] for AutoCamera");
				}
			} else {
				if (sb_showscores || sb_showteamscores || cl.stats[STAT_HEALTH] <= 0)
					Sbar_SoloScoreboard ();

				else if (scr_compactHud.value == 4)
					Sbar_DrawCompact_WithIcons();
				else if (scr_compactHud.value == 3)
					Sbar_DrawCompact_Bare();
				else if (scr_compactHud.value == 2)
					Sbar_DrawCompact_TF();
				else if (scr_compactHud.value == 1)
					Sbar_DrawCompact();
				else
					Sbar_DrawNormal();

				Sbar_DrawTrackingString();
			}
		} else if (sb_showscores || sb_showteamscores || cl.stats[STAT_HEALTH] <= 0) {
			Sbar_SoloScoreboard();
		} else if (scr_compactHud.value == 4) {
			Sbar_DrawCompact_WithIcons();
		} else if (scr_compactHud.value == 3) {
			Sbar_DrawCompact_Bare();
		} else if (scr_compactHud.value == 2) {
			Sbar_DrawCompact_TF();
		} else if (scr_compactHud.value == 1) {
			Sbar_DrawCompact();
		} else {
			Sbar_DrawNormal();
		}
	}
	if (sb_lines > 0 && scr_newHud.value == 1 && cl.deathmatch == 0
			&& (sb_showscores || sb_showteamscores || cl.stats[STAT_HEALTH] <= 0)) {
		Sbar_SoloScoreboard();
	}

	//VULT STAT LOSS
	if (amf_stat_loss.value && cl.stats[STAT_HEALTH] <= 0)
	{
		Amf_Reset_DamageStats();
	}

	// main screen deathmatch rankings
	// if we're dead show team scores in team games
	if (cl.stats[STAT_HEALTH] <= 0 && !cl.spectator) {
		if (cl.teamplay && !sb_showscores)
			Sbar_TeamOverlay();
		else
			Sbar_DeathmatchOverlay (0);
	} else if (sb_showscores) {
		Sbar_DeathmatchOverlay (0);
	} else if (sb_showteamscores) {
		Sbar_TeamOverlay();
	}

	if (sb_showscores || sb_showteamscores || cl.stats[STAT_HEALTH] <= 0)
		sb_updates = 0;

	if (scr_newHud.value == 1) // HUD -> hexum
		return;

	// clear unused areas in GL
	if (vid.width > 320 && !headsup) {
		if (scr_centerSbar.value)	// left
			Draw_TileClear (0, vid.height - sb_lines, sbar_xofs, sb_lines);
		Draw_TileClear (320 + sbar_xofs, vid.height - sb_lines, vid.width - (320 + sbar_xofs), sb_lines);	// right
	}
	if (!headsup && cl.spectator && autocam != CAM_TRACK && sb_lines > SBAR_HEIGHT)
		Draw_TileClear (sbar_xofs, vid.height - sb_lines, 320, sb_lines - SBAR_HEIGHT);

	if (vid.width >= 512 && sb_lines > 0 
			&& cl.gametype == GAME_DEATHMATCH && !scr_centerSbar.value 
			&& MULTIVIEWTHISPOV())
	{
		Sbar_MiniDeathmatchOverlay ();
	}
}
