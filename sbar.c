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
#include <jansson.h>
#ifndef CLIENTONLY
#include "server.h"
#endif
#include "hud.h"
#include "hud_common.h"
#include "vx_stuff.h"
#include "gl_model.h"
#include "r_texture.h"
#include "teamplay.h"
#include "utils.h"
#include "sbar.h"
#include "keys.h"

#include "qsound.h"

int CL_LoginImageId(const char* name);
static int JSON_readint(json_t* json);
static const char* JSON_readstring(json_t* json);
static mpic_t* CL_LoginFlag(int id);
qbool CL_LoginImageLoad(const char* path);
static void OnChange_scr_scoreboard_login_flagfile(cvar_t*, char*, qbool*);

typedef struct loginimage_s {
	char name[16];
	mpic_t pic;
} loginimage_t;

static struct {
	loginimage_t* images;
	size_t image_count;
	int bot_image_index;
	int max_width;
	int max_height;
} login_image_data;

#define FONT_WIDTH                 8 // Used for allocating space for scoreboard columns
#define SHORT_SPECTATOR_NAME_LEN   5 // if it's not teamplay, there is only room for 4 characters here

int sb_updates;		// if >= vid.numpages, no update needed

// --> mqwcl 0.96 oldhud customisation
//cvar_t  sbar_teamfrags = {"scr_sbar_teamfrags", "1"};
//cvar_t  sbar_fraglimit = {"scr_sbar_fraglimit", "1"};

cvar_t  sbar_drawfaceicon   = {"scr_sbar_drawfaceicon",     "1"};
cvar_t  sbar_drawammoicon   = {"scr_sbar_drawammoicon",     "1"};
cvar_t  sbar_drawarmoricon  = {"scr_sbar_drawarmoricon",    "1"};
cvar_t  sbar_drawguns       = {"scr_sbar_drawguns",         "1"};
cvar_t  sbar_drawammocounts = {"scr_sbar_drawammocounts",   "1"};
cvar_t  sbar_drawitems      = {"scr_sbar_drawitems",        "1"};
cvar_t  sbar_drawsigils     = {"scr_sbar_drawsigils",       "1"};
cvar_t  sbar_drawhealth     = {"scr_sbar_drawhealth",       "1"};
cvar_t  sbar_drawarmor      = {"scr_sbar_drawarmor",        "1"};
cvar_t  sbar_drawarmor666   = {"scr_sbar_drawarmor666",     "1"};
cvar_t  sbar_drawammo       = {"scr_sbar_drawammo",         "1"};
cvar_t  sbar_lowammo        = {"scr_sbar_lowammo",          "5"};

cvar_t  hud_centerranking              = { "scr_scoreboard_centered",   "1" };
cvar_t  hud_rankingpos_y               = { "scr_scoreboard_posy",       "0" };
cvar_t  hud_rankingpos_x               = { "scr_scoreboard_posx",       "0" };
cvar_t  hud_faderankings               = { "scr_scoreboard_fadescreen", "0" };
cvar_t  scr_scoreboard_login_names     = { "scr_scoreboard_login_names", "1" };
cvar_t  scr_scoreboard_login_indicator = { "scr_scoreboard_login_indicator", "&cffc*&r" };
cvar_t  scr_scoreboard_login_color     = { "scr_scoreboard_login_color", "255 255 192" };
cvar_t  scr_scoreboard_login_flagfile  = { "scr_scoreboard_login_flagfile", "flags", 0, OnChange_scr_scoreboard_login_flagfile };

//cvar_t  hud_ranks_separate  = {"scr_ranks_separate",   "1"};
// <-- mqwcl 0.96 oldhud customisation

mpic_t		*sb_nums[2][11];
mpic_t		*sb_colon, *sb_slash;
mpic_t		*sb_ibar;
mpic_t		*sb_sbar;
mpic_t		*sb_scorebar;
mpic_t      sb_ib_ammo[4];

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

static int	sbar_xofs;

cvar_t	scr_centerSbar                = {"scr_centerSbar",                "1"};

cvar_t	scr_compactHud                = {"scr_compactHud",                "0"};
cvar_t	scr_compactHudAlign           = {"scr_compactHudAlign",           "0"};

cvar_t	scr_drawHFrags                = {"scr_drawHFrags",                "1"};
cvar_t	scr_drawVFrags                = {"scr_drawVFrags",                "1"};

cvar_t scr_scoreboard_afk             = {"scr_scoreboard_afk",            "1"};
cvar_t scr_scoreboard_afk_style       = {"scr_scoreboard_afk_style",      "1"};

cvar_t	scr_scoreboard_teamsort       = {"scr_scoreboard_teamsort",       "1"};
cvar_t	scr_scoreboard_forcecolors    = {"scr_scoreboard_forcecolors",    "1"};
cvar_t	scr_scoreboard_showfrags      = {"scr_scoreboard_showfrags",      "1"};
cvar_t	scr_scoreboard_showflagstats  = {"scr_scoreboard_showflagstats",  "0"};
cvar_t	scr_scoreboard_drawtitle      = {"scr_scoreboard_drawtitle",      "1"};
cvar_t	scr_scoreboard_borderless     = {"scr_scoreboard_borderless",     "1"};
cvar_t	scr_scoreboard_spectator_name = {"scr_scoreboard_spectator_name", "\xF3\xF0\xE5\xE3\xF4\xE1\xF4\xEF\xF2"}; // brown "spectator". old: &cF20s&cF50p&cF80e&c883c&cA85t&c668a&c55At&c33Bo&c22Dr
cvar_t	scr_scoreboard_fillalpha      = {"scr_scoreboard_fillalpha",      "0.7"};
cvar_t	scr_scoreboard_fillcolored    = {"scr_scoreboard_fillcolored",    "2"};
cvar_t  scr_scoreboard_proportional   = {"scr_scoreboard_proportional",   "0"};

// VFrags: only draw the frags for the first player when using mvinset
#define MULTIVIEWTHISPOV() ((!cl_multiview.value) || (cl_mvinset.value && CL_MultiviewCurrentView() == 1))

qbool Sbar_IsStandardBar(void)
{
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

void Sbar_Init(void)
{
	int i;

	for (i = 0; i < 10; i++) {
		sb_nums[0][i] = Draw_CacheWadPic(va("num_%i", i), WADPIC_NUM_0 + i);
		sb_nums[1][i] = Draw_CacheWadPic(va("anum_%i", i), WADPIC_ANUM_0 + i);
	}

	sb_nums[0][10] = Draw_CacheWadPic("num_minus", WADPIC_NUM_MINUS);
	sb_nums[1][10] = Draw_CacheWadPic("anum_minus", WADPIC_ANUM_MINUS);

	sb_colon = Draw_CacheWadPic("num_colon", WADPIC_NUM_COLON);
	sb_slash = Draw_CacheWadPic("num_slash", WADPIC_NUM_SLASH);

	sb_weapons[0][0] = Draw_CacheWadPic("inv_shotgun", WADPIC_INV_SHOTGUN);
	sb_weapons[0][1] = Draw_CacheWadPic("inv_sshotgun", WADPIC_INV_SSHOTGUN);
	sb_weapons[0][2] = Draw_CacheWadPic("inv_nailgun", WADPIC_INV_NAILGUN);
	sb_weapons[0][3] = Draw_CacheWadPic("inv_snailgun", WADPIC_INV_SNAILGUN);
	sb_weapons[0][4] = Draw_CacheWadPic("inv_rlaunch", WADPIC_INV_RLAUNCH);
	sb_weapons[0][5] = Draw_CacheWadPic("inv_srlaunch", WADPIC_INV_SRLAUNCH);
	sb_weapons[0][6] = Draw_CacheWadPic("inv_lightng", WADPIC_INV_LIGHTNG);

	sb_weapons[1][0] = Draw_CacheWadPic("inv2_shotgun", WADPIC_INV2_SHOTGUN);
	sb_weapons[1][1] = Draw_CacheWadPic("inv2_sshotgun", WADPIC_INV2_SSHOTGUN);
	sb_weapons[1][2] = Draw_CacheWadPic("inv2_nailgun", WADPIC_INV2_NAILGUN);
	sb_weapons[1][3] = Draw_CacheWadPic("inv2_snailgun", WADPIC_INV2_SNAILGUN);
	sb_weapons[1][4] = Draw_CacheWadPic("inv2_rlaunch", WADPIC_INV2_RLAUNCH);
	sb_weapons[1][5] = Draw_CacheWadPic("inv2_srlaunch", WADPIC_INV2_SRLAUNCH);
	sb_weapons[1][6] = Draw_CacheWadPic("inv2_lightng", WADPIC_INV2_LIGHTNG);

	for (i = 0; i < 5; i++) {
		sb_weapons[2 + i][0] = Draw_CacheWadPic(va("inva%i_shotgun", i + 1), WADPIC_INVA1_SHOTGUN + i);
		sb_weapons[2 + i][1] = Draw_CacheWadPic(va("inva%i_sshotgun", i + 1), WADPIC_INVA1_SSHOTGUN + i);
		sb_weapons[2 + i][2] = Draw_CacheWadPic(va("inva%i_nailgun", i + 1), WADPIC_INVA1_NAILGUN + i);
		sb_weapons[2 + i][3] = Draw_CacheWadPic(va("inva%i_snailgun", i + 1), WADPIC_INVA1_SNAILGUN + i);
		sb_weapons[2 + i][4] = Draw_CacheWadPic(va("inva%i_rlaunch", i + 1), WADPIC_INVA1_RLAUNCH + i);
		sb_weapons[2 + i][5] = Draw_CacheWadPic(va("inva%i_srlaunch", i + 1), WADPIC_INVA1_SRLAUNCH + i);
		sb_weapons[2 + i][6] = Draw_CacheWadPic(va("inva%i_lightng", i + 1), WADPIC_INVA1_LIGHTNG + i);
	}

	sb_ammo[0] = Draw_CacheWadPic("sb_shells", WADPIC_SB_SHELLS);
	sb_ammo[1] = Draw_CacheWadPic("sb_nails", WADPIC_SB_NAILS);
	sb_ammo[2] = Draw_CacheWadPic("sb_rocket", WADPIC_SB_ROCKET);
	sb_ammo[3] = Draw_CacheWadPic("sb_cells", WADPIC_SB_CELLS);

	sb_armor[0] = Draw_CacheWadPic("sb_armor1", WADPIC_SB_ARMOR1);
	sb_armor[1] = Draw_CacheWadPic("sb_armor2", WADPIC_SB_ARMOR2);
	sb_armor[2] = Draw_CacheWadPic("sb_armor3", WADPIC_SB_ARMOR3);

	sb_items[0] = Draw_CacheWadPic("sb_key1", WADPIC_SB_KEY1);
	sb_items[1] = Draw_CacheWadPic("sb_key2", WADPIC_SB_KEY2);
	sb_items[2] = Draw_CacheWadPic("sb_invis", WADPIC_SB_INVIS);
	sb_items[3] = Draw_CacheWadPic("sb_invuln", WADPIC_SB_INVULN);
	sb_items[4] = Draw_CacheWadPic("sb_suit", WADPIC_SB_SUIT);
	sb_items[5] = Draw_CacheWadPic("sb_quad", WADPIC_SB_QUAD);

	sb_sigil[0] = Draw_CacheWadPic("sb_sigil1", WADPIC_SB_SIGIL1);
	sb_sigil[1] = Draw_CacheWadPic("sb_sigil2", WADPIC_SB_SIGIL2);
	sb_sigil[2] = Draw_CacheWadPic("sb_sigil3", WADPIC_SB_SIGIL3);
	sb_sigil[3] = Draw_CacheWadPic("sb_sigil4", WADPIC_SB_SIGIL4);

	sb_faces[4][0] = Draw_CacheWadPic("face1", WADPIC_SB_FACE1);
	sb_faces[4][1] = Draw_CacheWadPic("face_p1", WADPIC_SB_FACE_P1);
	sb_faces[3][0] = Draw_CacheWadPic("face2", WADPIC_SB_FACE2);
	sb_faces[3][1] = Draw_CacheWadPic("face_p2", WADPIC_SB_FACE_P2);
	sb_faces[2][0] = Draw_CacheWadPic("face3", WADPIC_SB_FACE3);
	sb_faces[2][1] = Draw_CacheWadPic("face_p3", WADPIC_SB_FACE_P3);
	sb_faces[1][0] = Draw_CacheWadPic("face4", WADPIC_SB_FACE4);
	sb_faces[1][1] = Draw_CacheWadPic("face_p4", WADPIC_SB_FACE_P4);
	sb_faces[0][0] = Draw_CacheWadPic("face5", WADPIC_SB_FACE5);
	sb_faces[0][1] = Draw_CacheWadPic("face_p5", WADPIC_SB_FACE_P5);

	sb_face_invis = Draw_CacheWadPic("face_invis", WADPIC_FACE_INVIS);
	sb_face_invuln = Draw_CacheWadPic("face_invul2", WADPIC_FACE_INVUL2);
	sb_face_invis_invuln = Draw_CacheWadPic("face_inv2", WADPIC_FACE_INV2);
	sb_face_quad = Draw_CacheWadPic("face_quad", WADPIC_FACE_QUAD);

	sb_sbar = Draw_CacheWadPic("sbar", WADPIC_SB_SBAR);
	sb_ibar = Draw_CacheWadPic("ibar", WADPIC_SB_IBAR);
	sb_scorebar = Draw_CacheWadPic("scorebar", WADPIC_SB_SCOREBAR);

	Cvar_SetCurrentGroup(CVAR_GROUP_SBAR);
	Cvar_Register(&scr_centerSbar);

	Cvar_Register(&scr_compactHud);
	Cvar_Register(&scr_compactHudAlign);

	// --> mqwcl 0.96 oldhud customisation
	//Cvar_Register (&sbar_teamfrags);
	//Cvar_Register (&sbar_fraglimit);
	Cvar_Register(&sbar_drawfaceicon);
	Cvar_Register(&sbar_drawammoicon);
	Cvar_Register(&sbar_drawarmoricon);
	Cvar_Register(&sbar_drawguns);
	Cvar_Register(&sbar_drawammocounts);
	Cvar_Register(&sbar_drawitems);
	Cvar_Register(&sbar_drawsigils);
	Cvar_Register(&sbar_drawhealth);
	Cvar_Register(&sbar_drawarmor);
	Cvar_Register(&sbar_drawarmor666);
	Cvar_Register(&sbar_drawammo);
	Cvar_Register(&sbar_lowammo);
	Cvar_Register(&hud_centerranking);
	Cvar_Register(&hud_rankingpos_y);
	Cvar_Register(&hud_rankingpos_x);
	Cvar_Register(&hud_faderankings);
	Cvar_Register(&scr_scoreboard_login_names);
	Cvar_Register(&scr_scoreboard_login_indicator);
	Cvar_Register(&scr_scoreboard_login_color);
	Cvar_Register(&scr_scoreboard_login_flagfile);
	//Cvar_Register (&hud_ranks_separate);
	// <-- mqwcl 0.96 oldhud customisation

	Cvar_Register(&scr_scoreboard_afk);
	Cvar_Register(&scr_scoreboard_afk_style);

	Cvar_Register(&scr_drawHFrags);
	Cvar_Register(&scr_drawVFrags);
	Cvar_Register(&scr_scoreboard_teamsort);
	Cvar_Register(&scr_scoreboard_forcecolors);
	Cvar_Register(&scr_scoreboard_showfrags);
	Cvar_Register(&scr_scoreboard_showflagstats);
	Cvar_Register(&scr_scoreboard_drawtitle);
	Cvar_Register(&scr_scoreboard_borderless);
	Cvar_Register(&scr_scoreboard_spectator_name);
	Cvar_Register(&scr_scoreboard_fillalpha);
	Cvar_Register(&scr_scoreboard_fillcolored);
	Cvar_Register(&scr_scoreboard_proportional);

	Cvar_ResetCurrentGroup();

	Cmd_AddCommand("+showscores", Sbar_ShowScores);
	Cmd_AddCommand("-showscores", Sbar_DontShowScores);

	Cmd_AddCommand("+showteamscores", Sbar_ShowTeamScores);
	Cmd_AddCommand("-showteamscores", Sbar_DontShowTeamScores);

	CL_LoginImageLoad(scr_scoreboard_login_flagfile.string);
}

void Request_Pings (void)
{
	if (cls.state == ca_active && cls.realtime - cl.last_ping_request > 2)
	{
		cl.last_ping_request = cls.realtime;
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, "pings");
	}
}

/****************************** SUPPORT ROUTINES ******************************/

// drawing routines are relative to the status bar location

void Sbar_DrawPic (int x, int y, mpic_t *pic)
{
	Draw_Pic (x + sbar_xofs, y + (vid.height - SBAR_HEIGHT), pic);
}

//JACK: Draws a portion of the picture in the status bar.
static void Sbar_DrawSubPic(int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height)
{
	Draw_SubPic (x, y + (vid.height - SBAR_HEIGHT), pic, srcx, srcy, width, height);
}

static void Sbar_DrawTransPic (int x, int y, mpic_t *pic)
{
	Draw_TransPic (x + sbar_xofs, y + (vid.height - SBAR_HEIGHT), pic);
}

//Draws one solid graphics character
static void Sbar_DrawCharacter (int x, int y, int num)
{
	Draw_Character (x + 4 + sbar_xofs, y + vid.height - SBAR_HEIGHT, num);
}

void Sbar_DrawString(int x, int y, char *str)
{
	Draw_String(x + sbar_xofs, y + vid.height - SBAR_HEIGHT, str);
}

static void Sbar_DrawAltString (int x, int y, char *str) {
	Draw_Alt_String(x + sbar_xofs, y + vid.height - SBAR_HEIGHT, str, 1, false);
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
	return Sbar_ColorForMap(cl.teamfortress ? player->known_team_color : player->topcolor);
}

int Sbar_TopColorScoreboard(player_info_t* player)
{
	return Sbar_ColorForMap(cl.teamfortress ? player->known_team_color : (scr_scoreboard_forcecolors.integer ? player->topcolor : player->real_topcolor));
}

int Sbar_BottomColor(player_info_t *player)
{
	return Sbar_ColorForMap(cl.teamfortress ? player->known_team_color : player->bottomcolor);
}

int Sbar_BottomColorScoreboard(player_info_t* player)
{
	return Sbar_ColorForMap(cl.teamfortress ? player->known_team_color : (scr_scoreboard_forcecolors.integer ? player->bottomcolor : player->real_bottomcolor));
}

// ** HUD -> hexum

/********************************* FRAG SORT *********************************/

static int fragsort[MAX_CLIENTS];
static int scoreboardlines;

#define SCR_TEAM_T_MAXTEAMSIZE	(16 + 1)

typedef struct {
	char team[SCR_TEAM_T_MAXTEAMSIZE];
	int frags;
	int caps;
	int players;
	int plow, phigh, ptotal;
	int topcolor, bottomcolor;
	int known_team_number;
	qbool myteam;
} team_t;
static team_t teams[MAX_CLIENTS];

static int teamsort[MAX_CLIENTS];
static int scoreboardteams;

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

static qbool Sbar_SortFrags(qbool spec) {
	int i, j, k;
	static int lastframecount = 0;
	static qbool any_flags = false;

	if (!spec && lastframecount && lastframecount == cls.framecount) {
		return any_flags;
	}

	any_flags = false;
	lastframecount = spec ? 0 : cls.framecount;

	// sort by frags
	scoreboardlines = 0;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].name[0]) {
			if (spec || !cl.players[i].spectator) {
				fragsort[scoreboardlines] = i;
				scoreboardlines++;
				if (cl.players[i].spectator) {
					cl.players[i].frags = -999;
				}
			}
			any_flags |= cl.players[i].loginname[0];
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

	return any_flags;
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
		int flagstats[] = { 0, 0, 0 };

		s = &cl.players[i];
		if (!s->name[0] || s->spectator)
			continue;

		// find his team in the list
		strlcpy (t, s->team, sizeof(t));
		if (!t[0])
			continue; // not on team

		Stats_GetFlagStats(s - cl.players, flagstats);

		for (j = 0; j < scoreboardteams; j++) {
			if (!strcmp(teams[j].team, t)) {
				playertoteam[i] = j;

				if (cl.scoring_system == SCORING_SYSTEM_TEAMFRAGS) {
					if (teams[j].players == 0) {
						teams[j].frags = s->frags;
						teams[j].caps = flagstats[2];
					}
					else {
						teams[j].frags = max(s->frags, teams[j].frags);
						teams[j].caps = max(flagstats[2], teams[j].caps);
					}
				}
				else {
					teams[j].frags += s->frags;
					teams[j].caps += flagstats[2];
				}
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
			teams[j].caps = flagstats[2];
			teams[j].players = 1;
			teams[j].known_team_number = s->known_team_color;
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

	if (cl.teamfortress && (cl.scoring_system == SCORING_SYSTEM_DEFAULT)) {
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

		for (i = 0; i < scoreboardteams; i++) {
			teams[i].frags /= teams[i].players;
		}
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

static qbool Sbar_SortTeamsAndFrags(qbool specs) {
	int i, j, k;
	qbool real_teamplay;
	qbool any_flags = false;

	real_teamplay = cl.teamplay && (TP_CountPlayers() > 2);

	if (!real_teamplay || !scr_scoreboard_teamsort.value) {
		return Sbar_SortFrags(specs);
	}

	scoreboardlines = 0;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].name[0]) {
			if (specs || !cl.players[i].spectator) {
				fragsort[scoreboardlines++] = i;
				if (cl.players[i].spectator) {
					cl.players[i].frags = -999;
				}
			}
			any_flags |= cl.players[i].loginname[0];
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
	return any_flags;
}



/********************************* INVENTORY *********************************/
static void Sbar_DrawInventory(void)
{
	int i, flashon;
	char num[6];
	float time;
	qbool headsup, hudswap;

	headsup = !Sbar_IsStandardBar();
	hudswap = cl_hudswap.integer;

	if (!headsup) {
		Draw_TileClear (sbar_xofs, vid.height - sb_lines, sbar_xofs + 320, 24);
		Sbar_DrawPic (0, -24, sb_ibar);
	}

	// weapons
	if (sbar_drawguns.integer) { // kazik
		for (i = 0; i < 7; i++) {
			if (cl.stats[STAT_ITEMS] & (IT_SHOTGUN << i) ) {
				time = cl.item_gettime[i];
				flashon = (int)((cl.time - time) * 10);
				if (flashon < 0) {
					flashon = 0;
				}
				if (flashon >= 10) {
					flashon = (cl.stats[STAT_ACTIVEWEAPON] == (IT_SHOTGUN << i)) ? 1 : 0;
				}
				else {
					flashon = (flashon % 5) + 2;
				}

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
	if (sbar_drawammocounts.integer) { // kazik
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
			}
			else {
				if (num[0] != ' ')
					Sbar_DrawCharacter ((6 * i + 1) * 8 - 2, -24, 18 + num[0] - '0');
				if (num[1] != ' ')
					Sbar_DrawCharacter ((6 * i + 2) * 8 - 2, -24, 18 + num[1] - '0');
				if (num[2] != ' ')
					Sbar_DrawCharacter ((6 * i + 3) * 8 - 2, -24, 18 + num[2] - '0');
			}
		}
	}

	// items
	flashon = 0;
	if (sbar_drawitems.integer) { // kazik
		for (i = 0; i < 6; i++) {
			if (cl.stats[STAT_ITEMS] & (1 << (17 + i))) {
				time = cl.item_gettime[17 + i];
				if (time && time > cl.time - 2 && flashon) {
					// flash frame
					sb_updates = 0;
				}
				else {
					Sbar_DrawPic(192 + i * 16, -16, sb_items[i]);
				}

				if (time && time > cl.time - 2) {
					sb_updates = 0;
				}
			}
		}
	}

	// sigils
	if (sbar_drawsigils.integer) { // kazik
		for (i = 0; i < 4; i++) {
			if (cl.stats[STAT_ITEMS] & (1 << (28 + i)))	{
				time = cl.item_gettime[28 + i];
				if (time && time > cl.time - 2 && flashon) {
					// flash frame
					sb_updates = 0;
				}
				else {
					Sbar_DrawPic(320 - 32 + i * 8, -16, sb_sigil[i]);
				}

				if (time && time > cl.time - 2) {
					sb_updates = 0;
				}
			}
		}
	}
}

/************************************ HUD ************************************/

static void Sbar_DrawFrags_DrawCellPlayer(int x, int y, player_info_t* player, int brackets)
{
	char num[4];

	// draw background
	Draw_Fill (sbar_xofs + x * 8 + 10, y, 28, 4, Sbar_TopColorScoreboard(player));
	Draw_Fill (sbar_xofs + x * 8 + 10, y + 4, 28, 3, Sbar_BottomColorScoreboard(player));

	// draw number
	snprintf (num, sizeof(num), "%3i", bound(-99, player->frags, 999));

	Sbar_DrawCharacter ((x + 1) * 8 , -24, num[0]);
	Sbar_DrawCharacter ((x + 2) * 8 , -24, num[1]);
	Sbar_DrawCharacter ((x + 3) * 8 , -24, num[2]);

	if (brackets) {
		Sbar_DrawCharacter (x * 8 + 2, -24, 16);
		Sbar_DrawCharacter ((x + 4) * 8 - 4, -24, 17);
	}
}

static void Sbar_DrawFrags_DrawTeamCell(int x, int y, team_t* team)
{
	char num[4];
	int frags = team->frags;
	player_info_t p = { 0 };
	p.topcolor = team->topcolor;
	p.bottomcolor = team->bottomcolor;
	p.known_team_color = team->known_team_number;

	// draw background
	Draw_Fill(sbar_xofs + x * 8 + 10, y, 28, 4, Sbar_TopColorScoreboard(&p));
	Draw_Fill(sbar_xofs + x * 8 + 10, y + 4, 28, 3, Sbar_BottomColorScoreboard(&p));

	// draw number
	snprintf(num, sizeof(num), "%3i", bound(-99, frags, 999));

	Sbar_DrawCharacter((x + 1) * 8, -24, num[0]);
	Sbar_DrawCharacter((x + 2) * 8, -24, num[1]);
	Sbar_DrawCharacter((x + 3) * 8, -24, num[2]);

	if (team->myteam) {
		Sbar_DrawCharacter(x * 8 + 2, -24, 16);
		Sbar_DrawCharacter((x + 4) * 8 - 4, -24, 17);
	}
}

static void Sbar_DrawFrags (void) {
	int i, k, l, x, y, mynum, myteam = -1;
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
			if (i + 1 == l && !drawn_self && !Sbar_IsSpectator(mynum)) {
				k = mynum;
			}
			else {
				k = fragsort[i];
			}

			s = &cl.players[k];

			if (!s->name[0] || s->spectator) {
				continue;
			}

			Sbar_DrawFrags_DrawCellPlayer(x, y, s, k == mynum);

			drawn_self |= (k == mynum);

			x += 4;
		}
	}
	else {
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
			Sbar_DrawFrags_DrawTeamCell(x, y, tm);

			if (k == myteam)
				drawn_self_team = true;

			x += 4;
		}

		if (scr_drawHFrags.value != 3 && l <= 2 && !Sbar_IsSpectator(mynum)) {

			x += 4 * (3 - l);

			s = &cl.players[mynum];

			Sbar_DrawFrags_DrawCellPlayer(x, y, s, 1);
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

static void Sbar_DrawNormal (void)
{
	if (Sbar_IsStandardBar())
		Sbar_DrawPic (0, 0, sb_sbar);

	// armor
	if ((cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) && sbar_drawarmor666.value)	{
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

	if ((cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) && sbar_drawarmor666.value)
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
		if (cl.stats[STAT_SHELLS + i] <= sbar_lowammo.integer)
			Sbar_DrawAltString(align * 8 * (3 - strlen(str)) + 24 + 32 * i, -16, str);
		else
			Sbar_DrawString(align * 8 * (3 - strlen(str)) + 24 + 32 * i, -16, str);
	}
	for (i = 0; i < 7; i++) {
		if (cl.stats[STAT_ITEMS] & (IT_SHOTGUN << i) ) {
			if (cl.stats[STAT_ACTIVEWEAPON] == (IT_SHOTGUN << i))
				Sbar_DrawPic (align * 8 + 18 + 16 * i, -32, sb_weapons[1][i]);
			else
				Sbar_DrawPic (align * 8 + 18 + 16 * i, -32, sb_weapons[0][i]);
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

	if ((cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) && sbar_drawarmor666.value)
		Sbar_DrawNum (2, 0, 666, 3, 1);
	else
		Sbar_DrawNum (2, 0, cl.stats[STAT_ARMOR], 3, cl.stats[STAT_ARMOR] <= 25);

	Sbar_DrawNum (86, 0, cl.stats[STAT_HEALTH], 3, cl.stats[STAT_HEALTH] <= 25);

	align = scr_compactHudAlign.value ? 1 : 0;
	for (i = 0; i < 4; i++) {
		snprintf(str, sizeof(str), "%d", cl.stats[STAT_SHELLS + i]);
		if (cl.stats[STAT_SHELLS + i] <= sbar_lowammo.integer)
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
	if ((cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) && sbar_drawarmor666.value)
		Sbar_DrawNum (2, 0, 666, 3, 1);
	else
		Sbar_DrawNum (2, 0, cl.stats[STAT_ARMOR], 3, cl.stats[STAT_ARMOR] <= 25);
	Sbar_DrawNum (86, 0, cl.stats[STAT_HEALTH], 3, cl.stats[STAT_HEALTH] <= 25);
	for (i = 0; i < 4; i++) {
		snprintf(str, sizeof(str), "%d", cl.stats[STAT_SHELLS + i]);
		if (cl.stats[STAT_SHELLS + i] <= sbar_lowammo.integer)
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

	if ((cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) && sbar_drawarmor666.value)
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
#ifndef CLIENTONLY
	sprintf (str,"skill %i", (int)(skill.value + 0.5));
	Sbar_DrawString (160 - strlen(str)*4, 12, str);
#endif
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
		// in teamplay 3 and 4 it's not possible to make teamkills
		return !(cl.teamplay == 3 || cl.teamplay == 4);
	}
}

static void Sbar_DeathmatchOverlay(int start)
{
	int stats_basic, stats_team, stats_touches, stats_caps, playerstats[7];
	int scoreboardsize, colors_thickness, statswidth, stats_xoffset = 0;
	int i, k, x, y, xofs, total, p, skip = 10, fragsint;
	int rank_width, leftover, startx, tempx, mynum;
	char num[12];
	char myminutes[11];
	char fragsstr[10];
	player_info_t *s;
	mpic_t *pic;
	float scale = 1.0f;
	float alpha = 1.0f;
	qbool proportional = scr_scoreboard_proportional.integer;
	qbool any_flags = false;

	if (!start && hud_faderankings.value) {
		Draw_FadeScreen(hud_faderankings.value);
	}

#ifndef CLIENTONLY
	// FIXME
	// check number of connections instead?
	if (cl.gametype == GAME_COOP && com_serveractive && !coop.value) {
		return;
	}
#endif

	if (cls.state == ca_active && !cls.demoplayback) {
		// request new ping times every two seconds
		if (cls.realtime - cl.last_ping_request >= 2) {
			cl.last_ping_request = cls.realtime;
			MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
			SZ_Print(&cls.netchan.message, "pings");
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
				if ((cl.teamfortress || scr_scoreboard_showflagstats.value) && Stats_IsFlagsParsed()) {
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
	if (hud_centerranking.value) {
		xofs = ((vid.width - rank_width) >> 1) + hud_rankingpos_x.value;
	}
	else {
		xofs = hud_rankingpos_x.value;
	}

	if (start) {
		y = start;
	}
	else {
		y = hud_rankingpos_y.value;
		if (y < 0 || y > vid.height / 2) {
			y = 0;
		}
	}

	if (!start) {
		if (scr_scoreboard_drawtitle.value) {
			pic = Draw_CachePic(CACHEPIC_RANKING);
			Draw_Pic(xofs + (rank_width - pic->width) / 2, y, pic);
			start = 36 + hud_rankingpos_y.value;
		}
		else {
			start = 12 + hud_rankingpos_y.value;
		}
	}

	for (i = 0; i < scoreboardlines && y <= SCOREBOARD_LASTROW; i++) {
		k = fragsort[i];
		s = &cl.players[k];

		if (!s->name[0]) {
			continue;
		}

		any_flags |= (s->loginname[0] && scr_scoreboard_login_indicator.string[0]);
	}

	y = start;

	if (!scr_scoreboard_borderless.value) {
		Draw_Fill(xofs - 1, y - 9, rank_width + 2, 1, 0);						//Border - Top
	}
	Draw_AlphaFill(xofs, y - 8, rank_width, 9, 1, SCOREBOARD_HEADINGALPHA);	//Draw heading row

	x = xofs + 1;
	// teamplay: " ping pl  fps frags team name" | " ping pl time frags team name"
	// non-tp  : " ping pl  fps frags name" | " ping pl time frags name"
	x += FONT_WIDTH;
	Draw_SStringAligned(x, y - 8, "ping", scale, alpha, proportional, text_align_right, x + FONT_WIDTH * 4);
	x += 5 * FONT_WIDTH;
	Draw_SStringAligned(x, y - 8, "pl", scale, alpha, proportional, text_align_right, x + FONT_WIDTH * 2);
	x += 3 * FONT_WIDTH;
	Draw_SStringAligned(x, y - 8, "time", scale, alpha, proportional, text_align_right, x + FONT_WIDTH * 4);
	x += 5 * FONT_WIDTH;
	Draw_SStringAligned(x, y - 8, "frags", scale, alpha, proportional, text_align_right, x + FONT_WIDTH * 5);
	x += 6 * FONT_WIDTH;
	if (cl.teamplay) {
		Draw_SStringAligned(x, y - 8, "team", scale, alpha, proportional, text_align_center, x + FONT_WIDTH * 4);
		x += 5 * FONT_WIDTH;
	}
	if (any_flags) {
		x += FONT_WIDTH;
	}
	Draw_SStringAligned(x, y - 8, "name", scale, alpha, proportional, text_align_left, x + FONT_WIDTH * 15);
	x += (any_flags ? 15 : 16) * FONT_WIDTH;
	if (statswidth) {
		stats_xoffset = x;

		Draw_SStringAligned(x, y - 8, "kills", scale, alpha, proportional, text_align_right, x + FONT_WIDTH * 5);
		x += FONT_WIDTH * 6;
		if (stats_team) {
			Draw_SStringAligned(x, y - 8, "tks", scale, alpha, proportional, text_align_right, x + FONT_WIDTH * 3);
			x += FONT_WIDTH * 4;
		}
		Draw_SStringAligned(x, y - 8, "dths", scale, alpha, proportional, text_align_right, x + FONT_WIDTH * 4);
		x += FONT_WIDTH * 5;

		if (stats_touches) {
			Draw_SStringAligned(x, y - 8, "tchs", scale, alpha, proportional, text_align_right, x + FONT_WIDTH * 4);
			x += FONT_WIDTH * 5;
		}

		if (stats_caps) {
			Draw_SStringAligned(x, y - 8, "caps", scale, alpha, proportional, text_align_right, x + FONT_WIDTH * 4);
			x += FONT_WIDTH * 5;
		}
	}

	x = xofs + 1;

	Draw_Fill(xofs, y + 1, rank_width, 1, 0);	//Border - Top (under header)
	y += 2;												//dont go over the black border, move the rest down
	if (!scr_scoreboard_borderless.value) {
		Draw_Fill(xofs - 1, y - 10, 1, 10, 0);						//Border - Left
		Draw_Fill(xofs - 1 + rank_width + 1, y - 10, 1, 10, 0);	//Border - Right
	}
	startx = x = xofs + 8;

	tempx = xofs + rank_width - MAX_SCOREBOARDNAME * 8 - (48 + statswidth) + leftover;

	Sbar_SortTeamsAndFrags(true);	// scores

	for (i = scoreboardsize = 0; i < scoreboardlines; i++) {
		k = fragsort[i];
		s = &cl.players[k];

		if (s->name[0]) {
			scoreboardsize++;
		}
	}

	if (y + (scoreboardsize - 1) * skip > SCOREBOARD_LASTROW) {
		colors_thickness = 3;
		skip = 8;
	}
	else {
		colors_thickness = 4;
	}

	for (i = 0; i < scoreboardlines && y <= SCOREBOARD_LASTROW; i++) {
		color_t background;
		float bk_alpha;
		byte c;
		clrinfo_t color;

		k = fragsort[i];
		s = &cl.players[k];

		if (!s->name[0]) {
			continue;
		}

		//render the main background transparencies behind players row
		if (k == mynum) {
			bk_alpha = 1.7 * SCOREBOARD_ALPHA;
			bk_alpha = min(alpha, 0.75);
		}
		else {
			bk_alpha = SCOREBOARD_ALPHA;
		}

		if (!cl.teamplay || s->spectator || !scr_scoreboard_fillcolored.value ||
			(scr_scoreboard_fillcolored.value == 2 && !scr_scoreboard_teamsort.value)) {
			c = 2;
		}
		else {
			c = Sbar_BottomColorScoreboard(s);
		}

		if (S_Voip_Speaking(k)) {
			background = RGBA_TO_COLOR(0, 255, 0, (byte)(bk_alpha * 255));
		}
		else {
			background = RGBA_TO_COLOR(host_basepal[c * 3], host_basepal[c * 3 + 1], host_basepal[c * 3 + 2], (byte)(bk_alpha * 255));
		}

		Draw_AlphaFillRGB(xofs, y, rank_width, skip, background);

		if (!scr_scoreboard_borderless.value) {
			Draw_Fill(xofs - 1, y, 1, skip, 0);					//Border - Left
			Draw_Fill(xofs - 1 + rank_width + 1, y, 1, skip, 0);	//Border - Right
		}

		// draw ping
		p = s->ping;
		if (p < 0 || p > 999) {
			p = 999;
		}
		snprintf(num, sizeof(num), "%i", p);
		color.c = RGBA_TO_COLOR(0xAA, 0xAA, 0xDD, 0xFF);
		color.i = 0;
		Draw_SColoredStringAligned(x, y, num, &color, 1, scale, alpha, proportional, text_align_right, x + FONT_WIDTH * 4);
		x += 4 * FONT_WIDTH; // move it forward, ready to print next column

		// draw pl
		p = s->pl;
		snprintf(num, sizeof(num), "%i", p);
		if (p == 0) {
			// 0 - white
			color.c = RGBA_TO_COLOR(0xFF, 0xFF, 0xFF, 0xFF);
		}
		else if (p < 3) {
			// 1-2 - yellow
			color.c = RGBA_TO_COLOR(0xCC, 0xDD, 0xDD, 0xFF);
		}
		else if (p < 6) {
			// 3-5 orange
			color.c = RGBA_TO_COLOR(0xFF, 0x55, 0x00, 0xFF);
		}
		else {	// 6+ - red
			color.c = RGBA_TO_COLOR(0xFF, 0x00, 0x00, 0xFF);
		}
		Draw_SColoredStringAligned(x, y, num, &color, 1, scale, alpha, proportional, text_align_right, x + 3 * FONT_WIDTH);
		x += 4 * FONT_WIDTH;

		// draw time
		total = (cl.intermission ? cl.completed_time : cls.demoplayback ? cls.demotime : cls.realtime) - s->entertime;
		total = (int)total / 60;
		total = bound(0, total, 999); // limit to 3 symbols int

		color.c = RGBA_TO_COLOR(255, 255, 255, 255);
		myminutes[0] = '\0';
		snprintf(myminutes, sizeof(myminutes), "%i", total);
		if (scr_scoreboard_afk.integer && (s->chatflag & CIF_AFK)) {
			color.c = RGBA_TO_COLOR(0xFF, 0x11, 0x11, 0xFF);
			if (scr_scoreboard_afk_style.integer == 1) {
				snprintf(myminutes, sizeof(myminutes), "afk");
			}
		}

		Draw_SColoredStringAligned(x, y, myminutes, &color, 1, scale, alpha, proportional, text_align_right, x + 4 * FONT_WIDTH);
		x += 5 * FONT_WIDTH;

		// draw spectator
		if (s->spectator) {
			if (cl.teamplay) {
				// use columns frags and team
				Draw_SStringAligned(x, y, scr_scoreboard_spectator_name.string, scale, alpha, proportional, text_align_left, x + 10 * FONT_WIDTH);
			}
			else {
				// use only frags column
				Draw_SStringAligned(x, y, scr_scoreboard_spectator_name.string, scale, alpha, proportional, text_align_left, x + SHORT_SPECTATOR_NAME_LEN * FONT_WIDTH);
			}

			x += (cl.teamplay ? 11 : 6) * FONT_WIDTH; // move across to print the name

			if (s->loginname[0] && scr_scoreboard_login_indicator.string[0]) {
				mpic_t* flag = CL_LoginFlag(s->loginflag_id);
				if (s->loginflag[0] && flag) {
					Draw_FitPicAlphaCenter(x - FONT_WIDTH * 0.75, y, FONT_WIDTH * 1.6, FONT_WIDTH, flag, 1.0f);
				}
				else {
					Draw_SStringAligned(x - FONT_WIDTH * 0.75, y, scr_scoreboard_login_indicator.string, scale, alpha, proportional, text_align_center, x + FONT_WIDTH * 1.6);
				}
			}
			if (any_flags) {
				x += FONT_WIDTH;
			}
			if (s->loginname[0] && scr_scoreboard_login_names.integer) {
				if (scr_scoreboard_login_color.string[0]) {
					color.c = RGBAVECT_TO_COLOR(scr_scoreboard_login_color.color);
					Draw_SColoredStringAligned(x, y, s->loginname, &color, 1, scale, alpha, proportional, text_align_left, x + FONT_WIDTH * 15);
				}
				else {
					Draw_SStringAligned(x, y, s->loginname, scale, alpha, proportional, text_align_left, x + FONT_WIDTH * 15);
				}
			}
			else {
				Draw_SStringAligned(x, y, s->name, scale, alpha, proportional, text_align_left, x + FONT_WIDTH * 15);
			}

			y += skip;
			x = startx;
			continue;
		}

		// print the shirt/pants colour bars
		Draw_Fill(cl.teamplay ? tempx - 40 : tempx, y + 4 - colors_thickness, 40, colors_thickness, Sbar_TopColorScoreboard(s));
		Draw_Fill(cl.teamplay ? tempx - 40 : tempx, y + 4, 40, 4, Sbar_BottomColorScoreboard(s));

		// frags
		if (Sbar_ShowScoreboardIndicator() && k == mynum) {
			Draw_Character(x, y, 16);
		}
		fragsint = bound(-999, s->frags, 9999); // limit to 4 symbols int
		snprintf(fragsstr, sizeof(fragsstr), "%i", fragsint);
		color.c = RGBA_TO_COLOR(255, 255, 255, 255);
		Draw_SColoredStringAligned(x, y, fragsstr, &color, 1, scale, alpha, proportional, text_align_right, x + 4 * FONT_WIDTH);
		x += 4 * FONT_WIDTH;
		if (Sbar_ShowScoreboardIndicator() && k == mynum) {
			Draw_Character(x, y, 17);
		}
		x += 2 * FONT_WIDTH;

		// team
		if (cl.teamplay) {
			Draw_SColoredStringAligned(x, y, s->team, &color, 1, scale, alpha, proportional, text_align_center, x + 4 * FONT_WIDTH);
			x += 5 * FONT_WIDTH;
		}

		if (s->loginname[0] && scr_scoreboard_login_indicator.string[0]) {
			mpic_t* flag = CL_LoginFlag(s->loginflag_id);
			if (s->loginflag[0] && flag) {
				Draw_FitPicAlphaCenter(x - FONT_WIDTH * 0.75, y, FONT_WIDTH * 1.6, FONT_WIDTH, flag, 1.0f);
			}
			else {
				Draw_SStringAligned(x - FONT_WIDTH * 0.75, y, scr_scoreboard_login_indicator.string, scale, alpha, proportional, text_align_center, x + FONT_WIDTH * 1.6);
			}
		}
		if (any_flags) {
			x += FONT_WIDTH;
		}
		if (s->loginname[0] && scr_scoreboard_login_names.integer) {
			if (scr_scoreboard_login_color.string[0]) {
				color.c = RGBAVECT_TO_COLOR(scr_scoreboard_login_color.color);
				Draw_SColoredStringAligned(x, y, s->loginname, &color, 1, scale, alpha, proportional, text_align_left, x + FONT_WIDTH * 15);
			}
			else {
				Draw_SStringAligned(x, y, s->loginname, scale, alpha, proportional, text_align_left, x + FONT_WIDTH * 15);
			}
		}
		else {
			Draw_SStringAligned(x, y, s->name, scale, alpha, proportional, text_align_left, x + FONT_WIDTH * 15);
		}

		if (statswidth) {
			x = stats_xoffset;
			Stats_GetBasicStats(s - cl.players, playerstats);
			if (stats_touches || stats_caps) {
				Stats_GetFlagStats(s - cl.players, playerstats + 4);
			}

			// kills
			snprintf(num, sizeof(num), "%5i", playerstats[0]);
			color.c = (playerstats[0] == 0 ? RGBA_TO_COLOR(255, 255, 255, 255) : RGBA_TO_COLOR(0, 187, 68, 255));
			Draw_SColoredStringAligned(x, y, num, &color, 1, scale, alpha, proportional, text_align_right, x + 5 * FONT_WIDTH);
			x += 6 * FONT_WIDTH;

			// teamkills
			if (stats_team) {
				snprintf(num, sizeof(num), "%3i", playerstats[2]);
				color.c = (playerstats[2] == 0 ? RGBA_TO_COLOR(255, 255, 255, 255) : RGBA_TO_COLOR(255, 255, 0, 255));
				Draw_SColoredStringAligned(x, y, num, &color, 1, scale, alpha, proportional, text_align_right, x + 3 * FONT_WIDTH);
				x += 4 * FONT_WIDTH;
			}

			// deaths
			snprintf(num, sizeof(num), "%4i", playerstats[1]);
			color.c = (playerstats[1] == 0 ? RGBA_TO_COLOR(255, 255, 255, 255) : RGBA_TO_COLOR(255, 0, 0, 255));
			Draw_SColoredStringAligned(x, y, num, &color, 1, scale, alpha, proportional, text_align_right, x + 4 * FONT_WIDTH);
			x += 5 * FONT_WIDTH;

			if (stats_touches) {
				// flag touches
				if (playerstats[4] < 1) {
					// 0 flag touches white
					color.c = RGBA_TO_COLOR(0xFF, 0xFF, 0xFF, 0xFF);
				}
				else if (playerstats[4] < 2) {
					// 1 flag touches yellow
					color.c = RGBA_TO_COLOR(0xFF, 0xFF, 0x00, 0xFF);
				}
				else if (playerstats[4] < 5) {
					// 2-4 flag touches orange
					color.c = RGBA_TO_COLOR(0xFF, 0x55, 0x00, 0xFF);
				}
				else if (playerstats[4] < 10) {
					// 5-9 flag touches pink
					color.c = RGBA_TO_COLOR(0xBB, 0x33, 0xBB, 0xFF);
				}
				else {
					// >9 flag touches green
					color.c = RGBA_TO_COLOR(0x00, 0xFF, 0x00, 0xFF);
				}
				snprintf(num, sizeof(num), "%4i", playerstats[4]);
				Draw_SColoredStringAligned(x, y, num, &color, 1, scale, alpha, proportional, text_align_right, x + 4 * FONT_WIDTH);
				x += 5 * FONT_WIDTH;
			}

			if (stats_caps) // flag captures
			{
				if (playerstats[6] < 1) {
					// 0 caps white
					color.c = RGBA_TO_COLOR(0xFF, 0xFF, 0xFF, 0xFF);
				}
				else if (playerstats[6] < 2) {
					// 1 cap yellow
					color.c = RGBA_TO_COLOR(0xFF, 0xFF, 0x00, 0xFF);
				}
				else if (playerstats[6] < 5) {
					// 2-4 caps orange
					color.c = RGBA_TO_COLOR(0xFF, 0x55, 0x00, 0xFF);
				}
				else if (playerstats[6] < 10) {
					// 5-9 caps pink
					color.c = RGBA_TO_COLOR(0xBB, 0x33, 0xBB, 0xFF);
				}
				else {
					// >9 caps green
					color.c = RGBA_TO_COLOR(0x00, 0xFF, 0x00, 0xFF);
				}
				snprintf(num, sizeof(num), "%4i", playerstats[6]);
				Draw_SColoredStringAligned(x, y, num, &color, 1, scale, alpha, proportional, text_align_right, x + 4 * FONT_WIDTH);
				x += 5 * FONT_WIDTH;
			}
		}

		y += skip;
		x = startx;
	}

	if (!scr_scoreboard_borderless.value) {
		Draw_Fill(xofs - 1, y - 1, rank_width + 2, 1, 0); //Border - Bottom
	}
}

static void Sbar_TeamOverlay(void)
{
	int i, k, x, y, xofs, plow, phigh, pavg, rank_width, skip = 10;
	char num[12], team[5];
	team_t *tm;
	mpic_t *pic;
	qbool proportional = scr_scoreboard_proportional.integer;
	float lhs;

	if (key_dest == key_console && !SCR_TakingAutoScreenshot())
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
	if (y < 0 || y > vid.height / 2) {
		y = 0;
	}

	if (hud_centerranking.value) {
		xofs = ((vid.width - rank_width) >> 1) + hud_rankingpos_x.value;
	}
	else {
		xofs = hud_rankingpos_x.value;
	}

	if (scr_scoreboard_drawtitle.value) {
		pic = Draw_CachePic(CACHEPIC_RANKING);
		Draw_Pic(xofs + (rank_width - pic->width) / 2, y, pic);
		y = 26 + hud_rankingpos_y.value;
	}
	else {
		y = 2 + hud_rankingpos_y.value;
	}

	if (!scr_scoreboard_borderless.value) {
		Draw_Fill(xofs - 1, y + 1, rank_width + 2, 1, 0);							//Border - Top
	}
	Draw_AlphaFill(xofs, y + 2, rank_width, skip, 1, SCOREBOARD_HEADINGALPHA);	//draw heading row
	Draw_Fill(xofs, y + 11, rank_width, 1, 0);							//Border - Top (under header)
	y += 2;																		//dont go over the black border, move the rest down
	if (!scr_scoreboard_borderless.value) {
		Draw_Fill(xofs - 1, y, 1, skip, 0);										//Border - Left
		Draw_Fill(xofs - 1 + rank_width + 1, y, 1, skip, 0);						//Border - Right
	}

	x = xofs + rank_width * 0.1 + 8;

	lhs = x;

	Draw_SStringAligned(x, y, "low", 1, 1, proportional, text_align_right, x + 3 * FONT_WIDTH);
	x += 3 * FONT_WIDTH;
	Draw_SStringAligned(x, y, "/", 1, 1, proportional, text_align_right, x + FONT_WIDTH);
	x += FONT_WIDTH;
	Draw_SStringAligned(x, y, "avg", 1, 1, proportional, text_align_right, x + FONT_WIDTH * 3);
	x += 3 * FONT_WIDTH;
	Draw_SStringAligned(x, y, "/", 1, 1, proportional, text_align_right, x + FONT_WIDTH);
	x += FONT_WIDTH;
	Draw_SStringAligned(x, y, "high", 1, 1, proportional, text_align_right, x + FONT_WIDTH * 4);
	x += 4 * FONT_WIDTH;
	x += FONT_WIDTH;
	Draw_SStringAligned(x, y, "team", 1, 1, proportional, text_align_center, x + FONT_WIDTH * 4);
	x += 4 * FONT_WIDTH;
	x += FONT_WIDTH;
	Draw_SStringAligned(x, y, (cl.scoring_system == SCORING_SYSTEM_TEAMFRAGS ? "score" : "total"), 1, 1, proportional, text_align_right, x + FONT_WIDTH * 5);
	x += 5 * FONT_WIDTH;
	x += FONT_WIDTH;
	if ((cl.teamfortress || scr_scoreboard_showflagstats.value) && Stats_IsFlagsParsed()) {
		Draw_SStringAligned(x, y, "caps", 1, 1, proportional, text_align_center, x + FONT_WIDTH * 4);
		x += 4 * FONT_WIDTH;
		x += FONT_WIDTH;
	}
	Draw_SStringAligned(x, y, "players", 1, 1, proportional, text_align_left, x + FONT_WIDTH * 7);
	x = lhs;

	y += 10;

	Sbar_SortTeams();		// sort the teams

	for (i = 0; i < scoreboardteams && y <= SCOREBOARD_LASTROW; i++) {
		k = teamsort[i];
		tm = teams + k;
		x = lhs;

		Draw_AlphaFill(xofs, y, rank_width, skip, 2, SCOREBOARD_ALPHA);

		if (!scr_scoreboard_borderless.value) {
			Draw_Fill(xofs - 1, y, 1, skip, 0);						//Border - Left
			Draw_Fill(xofs - 1 + rank_width + 1, y, 1, skip, 0);		//Border - Right
		}

		// draw pings
		plow = tm->plow;
		if (plow < 0 || plow > 999) {
			plow = 999;
		}

		phigh = tm->phigh;
		if (phigh < 0 || phigh > 999) {
			phigh = 999;
		}

		pavg = tm->players ? tm->ptotal / tm->players : 999;
		if (pavg < 0 || pavg > 999) {
			pavg = 999;
		}

		snprintf(num, sizeof(num), "%3i", plow);
		Draw_SStringAligned(x, y, num, 1, 1, proportional, text_align_right, x + 3 * FONT_WIDTH);
		x += 3 * FONT_WIDTH;
		Draw_SStringAligned(x, y, "/", 1, 1, proportional, text_align_right, x + FONT_WIDTH);
		x += FONT_WIDTH;
		snprintf(num, sizeof(num), "%3i", pavg);
		Draw_SStringAligned(x, y, num, 1, 1, proportional, text_align_right, x + 3 * FONT_WIDTH);
		x += 3 * FONT_WIDTH;
		Draw_SStringAligned(x, y, "/", 1, 1, proportional, text_align_right, x + FONT_WIDTH);
		x += FONT_WIDTH;
		snprintf(num, sizeof(num), "%3i", phigh);
		Draw_SStringAligned(x, y, num, 1, 1, proportional, text_align_right, x + 3 * FONT_WIDTH);
		x += 3 * FONT_WIDTH;
		x += FONT_WIDTH;

		// draw team
		if (Sbar_ShowScoreboardIndicator() && tm->myteam) {
			Draw_SStringAligned(x, y, "\020", 1, 1, proportional, text_align_right, x + FONT_WIDTH);
		}
		x += FONT_WIDTH;
		strlcpy(team, tm->team, sizeof(team));
		Draw_SStringAligned(x, y, team, 1, 1, proportional, text_align_center, x + 4 * FONT_WIDTH);
		x += 4 * FONT_WIDTH;
		if (Sbar_ShowScoreboardIndicator() && tm->myteam) {
			Draw_SStringAligned(x, y, "\021", 1, 1, proportional, text_align_left, x + FONT_WIDTH);
		}
		x += FONT_WIDTH;

		// draw total
		snprintf(num, sizeof(num), "%5i", tm->frags);
		Draw_SStringAligned(x, y, num, 1, 1, proportional, text_align_right, x + 5 * FONT_WIDTH);
		x += 5 * FONT_WIDTH;
		x += FONT_WIDTH;

		if ((cl.teamfortress || scr_scoreboard_showflagstats.value) && Stats_IsFlagsParsed()) {
			snprintf(num, sizeof(num), "%4i", tm->caps);
			Draw_SStringAligned(x, y, num, 1, 1, proportional, text_align_right, x + 4 * FONT_WIDTH);
			x += 4 * FONT_WIDTH;
			x += FONT_WIDTH;
		}

		// draw players
		snprintf(num, sizeof(num), "%7i", tm->players);
		Draw_SStringAligned(x, y, num, 1, 1, proportional, text_align_left, x + 7 * FONT_WIDTH);

		y += skip;
	}

	if (!scr_scoreboard_borderless.value) {
		Draw_Fill(xofs - 1, y - 1, rank_width + 2, 1, 0);              //Border - Bottom
	}

	y += 14;
	Sbar_DeathmatchOverlay(y);
}


static void Sbar_MiniDeathmatchOverlay (void) {
	int i, k, x, y, mynum, numlines;
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

		if (!s->name[0]) {
			continue;
		}

		Draw_Fill(x, y + 1, 40, 3, Sbar_TopColorScoreboard(s));
		Draw_Fill(x, y + 4, 40, 4, Sbar_BottomColorScoreboard(s));

		// draw number
		snprintf (num, sizeof(num), "%3i", bound(-99, s->frags, 999));

		Draw_Character (x + 8 , y, num[0]);
		Draw_Character (x + 16, y, num[1]);
		Draw_Character (x + 24, y, num[2]);

		if (Sbar_ShowScoreboardIndicator()) {
			if (k == mynum) {
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
		if (cl.teamplay) {
			Draw_String(x + 48 + 40, y, name);
		}
		else {
			Draw_String(x + 48, y, name);
		}

		y += 8;
	}

	// draw teams if room
	if (vid.width < 640 || !cl.teamplay) {
		return;
	}

	Sbar_SortTeams();
	if (!scoreboardteams) {
		return;
	}

	// draw seperator
	x += 208;
	for (y = vid.height - sb_lines; y < vid.height - 6; y += 2) {
		Draw_Character(x, y, 14);
	}

	x += 16;

drawteams:
	Sbar_SortTeams();

	y = vid.height - sb_lines;
	for (i = 0; i < scoreboardteams && y < vid.height - 8 + 1; i++) {
		k = teamsort[i];
		tm = teams + k;

		//draw name
		strlcpy(team, tm->team, sizeof(team));
		Draw_String(x, y, team);

		// draw total
		Draw_Fill(x + 40, y + 1, 48, 3, Sbar_ColorForMap(tm->topcolor));
		Draw_Fill(x + 40, y + 4, 48, 4, Sbar_ColorForMap(tm->bottomcolor));
		snprintf(num, sizeof(num), "%4i", bound(-999, tm->frags, 9999));
		Draw_Character(x + 40 + 8, y, num[0]);
		Draw_Character(x + 40 + 16, y, num[1]);
		Draw_Character(x + 40 + 24, y, num[2]);
		Draw_Character(x + 40 + 32, y, num[3]);

		if (tm->myteam) {
			Draw_Character(x + 40, y, 16);
			Draw_Character(x + 40 + 40, y, 17);
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

	pic = Draw_CachePic(CACHEPIC_COMPLETE);
	Draw_Pic (xofs + 64, 24, pic);

	pic = Draw_CachePic(CACHEPIC_INTER);
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

	pic = Draw_CachePic (CACHEPIC_FINALE);
	Draw_TransPic ( (vid.width-pic->width)/2, 16, pic);
}

/********************************* INTERFACE *********************************/
void Sbar_Draw(void) {
	qbool headsup;
	extern cvar_t scr_newHud;

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
		if (!cl.spectator || cl.autocam == CAM_TRACK)
			Sbar_DrawInventory();

		if (cl.gametype == GAME_DEATHMATCH && (!headsup || vid.width < 512 || (vid.width >= 512 && scr_centerSbar.value )))
			Sbar_DrawFrags();
	}

	// main area
	if (sb_lines > 0 && scr_newHud.value != 1) {  // HUD -> hexum
		if (cl.spectator) {
			if (cl.autocam != CAM_TRACK) {
				Sbar_DrawSpectatorMessage();
			}
			else {
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
	if (!headsup && cl.spectator && cl.autocam != CAM_TRACK && sb_lines > SBAR_HEIGHT)
		Draw_TileClear (sbar_xofs, vid.height - sb_lines, 320, sb_lines - SBAR_HEIGHT);

	if (vid.width >= 512 && sb_lines > 0 
			&& cl.gametype == GAME_DEATHMATCH && !scr_centerSbar.value 
			&& MULTIVIEWTHISPOV())
	{
		Sbar_MiniDeathmatchOverlay ();
	}
}

int CL_LoginImageId(const char* name)
{
	int index = -1;
	int i;

	if (name[0]) {
		for (i = 0; i < login_image_data.image_count; ++i) {
			if (!strcasecmp(name, login_image_data.images[i].name)) {
				return i;
			}
		}
	}
	return index;
}

int CL_LoginImageBot(void)
{
	return login_image_data.bot_image_index;
}

qbool CL_LoginImageLoad(const char* path)
{
	json_error_t error;
	char truepath[MAX_OSPATH];
	json_t* json;
	loginimage_t* new_login_images;
	size_t new_login_image_count = 0;
	int new_login_bot_image = -1;
	int i, tex_width, tex_height, max_width = 0, max_height = 0;
	json_t* val;

	if (!path[0]) {
		Q_free(login_image_data.images);
		login_image_data.image_count = 0;
		login_image_data.bot_image_index = -1;
		return true;
	}

	strlcpy(truepath, "textures/scoreboard/", sizeof(truepath));
	strlcat(truepath, path, sizeof(truepath));
	COM_ForceExtensionEx(truepath, ".json", sizeof(truepath));
	{
		int json_len;
		char* json_bytes = (char*)FS_LoadHeapFile(truepath, &json_len);
		if (!json_bytes) {
			Con_Printf("Unable to load %s\n", truepath);
			return false;
		}

		json = json_loadb(json_bytes, json_len, 0, &error);
		Q_free(json_bytes);
	}
	if (!json || !json_is_array(json)) {
		Con_Printf("Invalid json file %s\n", truepath);
		if (json) {
			json_decref(json);
		}
		return false;
	}

	new_login_image_count = json_array_size(json);
	new_login_images = Q_malloc(sizeof(new_login_images[0]) * new_login_image_count);
	json_array_foreach(json, i, val) {
		const char* code = JSON_readstring(json_object_get(val, "code"));
		const char* path = JSON_readstring(json_object_get(val, "file"));
		int x = JSON_readint(json_object_get(val, "x"));
		int y = JSON_readint(json_object_get(val, "y"));
		int width = JSON_readint(json_object_get(val, "width"));
		int height = JSON_readint(json_object_get(val, "height"));
		texture_ref texture;

		strlcpy(truepath, "textures/scoreboard/", sizeof(truepath));
		strlcat(truepath, path, sizeof(truepath));
		COM_StripExtension(truepath, truepath, sizeof(truepath));

		if (code == NULL || !code[0]) {
			Q_free(new_login_images);
			Con_Printf("Failed to load screenshot flags: json error on element#%d\n", i);
			return false;
		}

		texture = R_LoadTextureImage(truepath, truepath, 0, 0, TEX_ALPHA | TEX_PREMUL_ALPHA | TEX_NOSCALE);
		if (!R_TextureReferenceIsValid(texture)) {
			Con_Printf("Unable to load %s\n", truepath);
			return false;
		}
		tex_width = R_TextureWidth(texture);
		tex_height = R_TextureHeight(texture);

		x = max(x, 0);
		y = max(y, 0);
		width = (width < 0 ? tex_width : width);
		height = (height < 0 ? tex_height : height);

		width = min(width, tex_width - x);
		height = min(height, tex_height - y);

		strlcpy(new_login_images[i].name, code, sizeof(new_login_images[i].name));
		new_login_images[i].pic.width = width;
		new_login_images[i].pic.height = height;
		new_login_images[i].pic.texnum = texture;
		new_login_images[i].pic.sl = (1.0f * x) / tex_width;
		new_login_images[i].pic.tl = (1.0f * y) / tex_height;
		new_login_images[i].pic.sh = (1.0f * width) / tex_width;
		new_login_images[i].pic.th = (1.0f * height) / tex_height;

		max_width = max(width, max_width);
		max_height = max(height, max_height);
	}

	login_image_data.images = new_login_images;
	login_image_data.image_count = new_login_image_count;
	login_image_data.bot_image_index = new_login_bot_image;
	login_image_data.max_width = max_width;
	login_image_data.max_width = max_height;

	for (i = 0; i < sizeof(cl.players) / sizeof(cl.players[0]); ++i) {
		cl.players[i].loginflag_id = CL_LoginImageId(cl.players[i].loginflag);
	}

	json_decref(json);
	return true;
}

static void OnChange_scr_scoreboard_login_flagfile(cvar_t* cv, char* newvalue, qbool* cancel)
{
	*cancel = CL_LoginImageLoad(newvalue);
}

static mpic_t* CL_LoginFlag(int id)
{
	if (id < 0 || id >= login_image_data.image_count) {
		return NULL;
	}
	return &login_image_data.images[id].pic;
}

static int JSON_readint(json_t* json)
{
	if (json_is_integer(json)) {
		return (int)json_integer_value(json);
	}
	return -1;
}

static const char* JSON_readstring(json_t* json)
{
	if (json_is_string(json)) {
		return json_string_value(json);
	}
	return "";
}
