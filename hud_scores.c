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

#include "quakedef.h"
#include "common_draw.h"
#include "hud.h"
#include "hud_common.h"
#include "teamplay.h"
#include "Ctrl.h"
#include "vx_stuff.h"
#include "mvd_utils_common.h"
#include "sbar.h"

qbool TP_ReadMacroFormat(char* s, int* fixed_width, int* alignment, char** new_s);
char* TP_AlignMacroText(char* text, int fixed_width, int alignment);

static cvar_t hud_sortrules_teamsort = { "hud_sortrules_teamsort", "0" };
static cvar_t hud_sortrules_playersort = { "hud_sortrules_playersort", "1" };
static cvar_t hud_sortrules_includeself = { "hud_sortrules_includeself", "1" };

static int playersort_rules;
static int teamsort_rules;

static int HUD_ComparePlayers(const void *vp1, const void *vp2)
{
	const sort_players_info_t *p1 = vp1;
	const sort_players_info_t *p2 = vp2;

	int r = 0;
	player_info_t *i1 = &cl.players[p1->playernum];
	player_info_t *i2 = &cl.players[p2->playernum];

	if (i1->spectator && !i2->spectator) {
		r = -1;
	}
	else if (!i1->spectator && i2->spectator) {
		r = 1;
	}
	else if (i1->spectator && i2->spectator) {
		r = Q_strcmp2(i1->name, i2->name);
	}
	else {
		//
		// Both are players.
		//
		if ((playersort_rules & 2) && cl.teamplay && p1->team && p2->team) {
			// Leading team on top, sort players inside of the teams.

			// Teamsort 1, first sort on team frags.
			if (teamsort_rules == 1) {
				r = p1->team->frags - p2->team->frags;
			}

			// sort on team name only.
			r = (r == 0) ? -Q_strcmp2(p1->team->name, p2->team->name) : r;
		}

		if (playersort_rules & 1) {
			r = (r == 0) ? i1->frags - i2->frags : r;
		}
		r = (r == 0) ? -Q_strcmp2(i1->name, i2->name) : r;
	}

	r = (r == 0) ? (p1->playernum - p2->playernum) : r;

	// qsort() sorts ascending by default, we want descending.
	// So negate the result.
	return -r;
}

static int HUD_CompareTeams(const void *vt1, const void *vt2)
{
	int r = 0;
	const sort_teams_info_t *t1 = vt1;
	const sort_teams_info_t *t2 = vt2;

	if (hud_sortrules_teamsort.integer == 1) {
		r = (t1->frags - t2->frags);
	}
	r = !r ? -Q_strcmp2(t1->name, t2->name) : r;

	// qsort() sorts ascending by default, we want descending.
	// So negate the result.
	return -r;
}

void HUD_Sort_Scoreboard(int flags)
{
	int i;
	int team;
	int active_player = -1;
	qbool common_fragcounts = cl.teamfortress && cl.teamplay && cl.scoring_system == SCORING_SYSTEM_DEFAULT;

	active_player_position = -1;
	active_team_position = -1;

	n_teams = 0;
	n_players = 0;
	n_spectators = 0;

	playersort_rules = hud_sortrules_playersort.integer;
	teamsort_rules = hud_sortrules_teamsort.integer;

	// This taken from score_bar logic
	if (cls.demoplayback && !cl.spectator && !cls.mvdplayback) {
		active_player = cl.playernum;
	}
	else if ((cls.demoplayback || cl.spectator) && Cam_TrackNum() >= 0) {
		active_player = cl.spec_track;
	}
	else {
		active_player = cl.playernum;
	}

	// Set team properties.
	if (flags & HUD_SCOREBOARD_UPDATE) {
		memset(sorted_teams, 0, sizeof(sorted_teams));

		for (i = 0; i < MAX_CLIENTS; i++) {
			if (cl.players[i].name[0] && !cl.players[i].spectator) {
				if (cl.players[i].frags == 0 && cl.players[i].team[0] == '\0' && !strcmp(cl.players[i].name, "[ServeMe]")) {
					continue;
				}

				// Find players team
				for (team = 0; team < n_teams; team++) {
					if (cl.teamplay && !strcmp(cl.players[i].team, sorted_teams[team].name) && sorted_teams[team].name[0]) {
						break;
					}
					if (!cl.teamplay && !strcmp(cl.players[i].name, sorted_teams[team].name) && sorted_teams[team].name[0]) {
						break;
					}
				}

				// The team wasn't found in the list of existing teams
				// so add a new team.
				if (team == n_teams) {
					team = n_teams++;
					sorted_teams[team].min_ping = 999;
					sorted_teams[team].top = Sbar_TopColor(&cl.players[i]);
					sorted_teams[team].bottom = Sbar_BottomColor(&cl.players[i]);
					if (cl.teamplay) {
						sorted_teams[team].name = cl.players[i].team;
					}
					else {
						sorted_teams[team].name = cl.players[i].name;
					}
				}

				if (sorted_teams[team].nplayers >= 1) {
					common_fragcounts &= (sorted_teams[team].frags / sorted_teams[team].nplayers == cl.players[i].frags);
				}

				sorted_teams[team].nplayers++;
				if (cl.scoring_system == SCORING_SYSTEM_TEAMFRAGS) {
					if (sorted_teams[team].nplayers == 1) {
						sorted_teams[team].frags = cl.players[i].frags;
					}
					else {
						sorted_teams[team].frags = max(cl.players[i].frags, sorted_teams[team].frags);
					}
				}
				else {
					sorted_teams[team].frags += cl.players[i].frags;
				}
				sorted_teams[team].avg_ping += cl.players[i].ping;
				sorted_teams[team].min_ping = min(sorted_teams[team].min_ping, cl.players[i].ping);
				sorted_teams[team].max_ping = max(sorted_teams[team].max_ping, cl.players[i].ping);
				sorted_teams[team].stack += SCR_HUD_TotalStrength(cl.players[i].stats[STAT_HEALTH], cl.players[i].stats[STAT_ARMOR], SCR_HUD_ArmorType(cl.players[i].stats[STAT_ITEMS]));

				// The total RL count for the players team.
				if (cl.players[i].stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER) {
					sorted_teams[team].rlcount++;
				}
				if (cl.players[i].stats[STAT_ITEMS] & IT_LIGHTNING) {
					sorted_teams[team].lgcount++;
				}
				if (cl.players[i].stats[STAT_ITEMS] & (IT_ROCKET_LAUNCHER | IT_LIGHTNING)) {
					sorted_teams[team].weapcount++;
				}

				if (cls.mvdplayback) {
					mvd_new_info_t* stats = MVD_StatsForPlayer(&cl.players[i]);
					sort_teams_info_t* t = &sorted_teams[team];

					if (stats) {
						t->quads_taken += stats->mvdinfo.itemstats[QUAD_INFO].count;
						t->pents_taken += stats->mvdinfo.itemstats[PENT_INFO].count;
						t->rings_taken += stats->mvdinfo.itemstats[RING_INFO].count;
						t->ga_taken += stats->mvdinfo.itemstats[GA_INFO].count;
						t->ya_taken += stats->mvdinfo.itemstats[YA_INFO].count;
						t->ra_taken += stats->mvdinfo.itemstats[RA_INFO].count;
						t->mh_taken += stats->mvdinfo.itemstats[MH_INFO].count;
						t->q_lasttime = max(t->q_lasttime, stats->mvdinfo.itemstats[QUAD_INFO].starttime);
						t->p_lasttime = max(t->p_lasttime, stats->mvdinfo.itemstats[PENT_INFO].starttime);
						t->r_lasttime = max(t->r_lasttime, stats->mvdinfo.itemstats[RING_INFO].starttime);
						t->ga_lasttime = max(t->ga_lasttime, stats->mvdinfo.itemstats[GA_INFO].starttime);
						t->ya_lasttime = max(t->ya_lasttime, stats->mvdinfo.itemstats[YA_INFO].starttime);
						t->ra_lasttime = max(t->ra_lasttime, stats->mvdinfo.itemstats[RA_INFO].starttime);
						t->mh_lasttime = max(t->mh_lasttime, stats->mvdinfo.itemstats[MH_INFO].starttime);
					}
				}

				// Set player data.
				sorted_players[n_players + n_spectators].playernum = i;
				if (i == active_player && !cl.players[i].spectator) {
					active_player_position = n_players + n_spectators;
				}

				// Increase the count.
				if (cl.players[i].spectator) {
					n_spectators++;
				}
				else {
					n_players++;
				}
			}
		}
	}

	// Calc avg ping.
	if (flags & HUD_SCOREBOARD_AVG_PING) {
		for (team = 0; team < n_teams; team++) {
			sorted_teams[team].avg_ping /= sorted_teams[team].nplayers;
		}
	}

	// Adjust team scores for team fortress scoring (required so it matches +showteamscores)
	if (cl.teamfortress && common_fragcounts) {
		for (team = 0; team < n_teams; ++team) {
			if (sorted_teams[team].nplayers) {
				sorted_teams[team].frags /= sorted_teams[team].nplayers;
			}
		}
	}

	// Sort teams.
	if (flags & HUD_SCOREBOARD_SORT_TEAMS) {
		active_team_position = -1;

		qsort(sorted_teams, n_teams, sizeof(sort_teams_info_t), HUD_CompareTeams);

		// if set, need to make sure that the player's team is first or second position
		if (hud_sortrules_includeself.integer && active_player_position >= 0) {
			player_info_t *player = &cl.players[sorted_players[active_player_position].playernum];
			sorted_players[active_player_position].team = NULL;

			// Find players team.
			for (team = 0; team < n_teams; team++) {
				const char* team_name = (cl.teamplay ? player->team : player->name);
				if (!strcmp(team_name, sorted_teams[team].name) && sorted_teams[team].name[0]) {
					if (hud_sortrules_includeself.integer == 1 && team > 0) {
						sort_teams_info_t temp = sorted_teams[0];
						sorted_teams[0] = sorted_teams[team];
						sorted_teams[team] = temp;
					}
					else if (hud_sortrules_includeself.integer == 2 && team > 1) {
						sort_teams_info_t temp = sorted_teams[1];
						sorted_teams[1] = sorted_teams[team];
						sorted_teams[team] = temp;
					}
					break;
				}
			}
		}

		// BUGFIX, this needs to happen AFTER the team array has been sorted, otherwise the
		// players might be pointing to the incorrect team address.
		for (i = 0; i < MAX_CLIENTS; i++) {
			player_info_t *player = &cl.players[sorted_players[i].playernum];
			sorted_players[i].team = NULL;

			// Find players team.
			for (team = 0; team < n_teams; team++) {
				if (!strcmp(player->team, sorted_teams[team].name) && sorted_teams[team].name[0]) {
					sorted_players[i].team = &sorted_teams[team];
					if (sorted_players[i].playernum == active_player) {
						active_team_position = team;
					}
					break;
				}
			}
		}
	}

	// Sort players by frags only
	if (flags & HUD_SCOREBOARD_SORT_PLAYERS) {
		int oldrules = playersort_rules;

		playersort_rules = 1;
		qsort(sorted_players, n_players + n_spectators, sizeof(sort_players_info_t), HUD_ComparePlayers);
		playersort_rules = oldrules;

		memcpy(sorted_players_by_frags, sorted_players, sizeof(sorted_players_by_frags));
	}

	// Sort players.
	if (flags & HUD_SCOREBOARD_SORT_PLAYERS) {
		qsort(sorted_players, n_players + n_spectators, sizeof(sort_players_info_t), HUD_ComparePlayers);

		if (!cl.teamplay) {
			// Re-find player
			active_player_position = -1;
			for (i = 0; i < n_players + n_spectators; ++i) {
				if (sorted_players[i].playernum == active_player) {
					active_player_position = i;
					if (hud_sortrules_includeself.integer == 1 && i > 0) {
						sort_players_info_t temp = sorted_players[0];
						sorted_players[0] = sorted_players[i];
						sorted_players[i] = temp;
						active_player_position = 0;
					}
					else if (hud_sortrules_includeself.integer == 2 && i > 1) {
						sort_players_info_t temp = sorted_players[1];
						sorted_players[1] = sorted_players[i];
						sorted_players[i] = temp;
						active_player_position = 1;
					}
				}
			}
		}
	}
}

static qbool SCR_Hud_GetScores(int* team, int* enemy, char** teamName, char** enemyName)
{
	qbool swap = false;

	*team = *enemy = 0;
	*teamName = *enemyName = NULL;

	if (cl.teamplay) {
		*team = sorted_teams[0].frags;
		*teamName = sorted_teams[0].name;

		if (n_teams > 1) {
			if (hud_sortrules_includeself.integer >= 1 && active_team_position > 1) {
				*enemy = sorted_teams[active_team_position].frags;
				*enemyName = sorted_teams[active_team_position].name;
			}
			else {
				*enemy = sorted_teams[1].frags;
				*enemyName = sorted_teams[1].name;
			}

			swap = (hud_sortrules_includeself.integer == 1 && active_team_position > 0);
		}
	}
	else if (cl.deathmatch) {
		*team = cl.players[sorted_players[0].playernum].frags;
		*teamName = cl.players[sorted_players[0].playernum].name;

		if (n_players > 1) {
			if (hud_sortrules_includeself.integer >= 1 && active_player_position > 1) {
				*enemy = cl.players[sorted_players[active_player_position].playernum].frags;
				*enemyName = cl.players[sorted_players[active_player_position].playernum].name;
			}
			else {
				*enemy = cl.players[sorted_players[1].playernum].frags;
				*enemyName = cl.players[sorted_players[1].playernum].name;
			}

			swap = (hud_sortrules_includeself.integer == 1 && active_player_position > 0);
		}
	}

	if (!*teamName) {
		*teamName = "T";
	}
	if (!*enemyName) {
		*enemyName = "E";
	}

	if (swap) {
		int temp_frags = *team;
		char* temp_name = *teamName;

		*team = *enemy;
		*teamName = *enemyName;
		*enemy = temp_frags;
		*enemyName = temp_name;
	}

	return swap;
}

//
// TODO: decide what to do in freefly mode (and how to catch it?!), now all score_* hud elements just draws "0"
//
static void SCR_HUD_DrawScoresTeam(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *digits, *align, *colorize, *proportional;
	int teamFrags = 0, enemyFrags = 0;
	char* teamName = 0, *enemyName = 0;

	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		digits = HUD_FindVar(hud, "digits");
		align = HUD_FindVar(hud, "align");
		colorize = HUD_FindVar(hud, "colorize");
		proportional = HUD_FindVar(hud, "proportional");
	}

	SCR_Hud_GetScores(&teamFrags, &enemyFrags, &teamName, &enemyName);

	SCR_HUD_DrawNum(hud, teamFrags, (colorize->integer) ? (teamFrags < 0 || colorize->integer > 1) : false, scale->value, style->value, digits->value, align->string, proportional->integer);
}

static void SCR_HUD_DrawScoresEnemy(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *digits, *align, *colorize, *proportional;
	int teamFrags = 0, enemyFrags = 0;
	char* teamName = 0, *enemyName = 0;

	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		digits = HUD_FindVar(hud, "digits");
		align = HUD_FindVar(hud, "align");
		colorize = HUD_FindVar(hud, "colorize");
		proportional = HUD_FindVar(hud, "proportional");
	}

	SCR_Hud_GetScores(&teamFrags, &enemyFrags, &teamName, &enemyName);

	if (!cl.teamplay) {
		// Ignore enemyFrags now, and go by highest opponent that isn't current player
		if (n_players > 1) {
			if (!strcmp(cl.players[sorted_players_by_frags[0].playernum].name, teamName)) {
				enemyFrags = cl.players[sorted_players_by_frags[1].playernum].frags;
				enemyName = cl.players[sorted_players_by_frags[1].playernum].name;
			}
			else {
				enemyFrags = cl.players[sorted_players_by_frags[0].playernum].frags;
				enemyName = cl.players[sorted_players_by_frags[0].playernum].name;
			}
		}
	}

	SCR_HUD_DrawNum(hud, enemyFrags, (colorize->integer) ? (enemyFrags < 0 || colorize->integer > 1) : false, scale->value, style->value, digits->value, align->string, proportional->integer);
}

static void SCR_HUD_DrawScoresDifference(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *digits, *align, *colorize, *proportional;
	int teamFrags = 0, enemyFrags = 0;
	char* teamName = 0, *enemyName = 0;

	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		digits = HUD_FindVar(hud, "digits");
		align = HUD_FindVar(hud, "align");
		colorize = HUD_FindVar(hud, "colorize");
		proportional = HUD_FindVar(hud, "proportional");
	}

	SCR_Hud_GetScores(&teamFrags, &enemyFrags, &teamName, &enemyName);

	if (!cl.teamplay) {
		// Ignore enemyFrags now, and go by highest opponent that isn't current player
		if (n_players > 1) {
			if (!strcmp(cl.players[sorted_players_by_frags[0].playernum].name, teamName)) {
				enemyFrags = cl.players[sorted_players_by_frags[1].playernum].frags;
				enemyName = cl.players[sorted_players_by_frags[1].playernum].name;
			}
			else {
				enemyFrags = cl.players[sorted_players_by_frags[0].playernum].frags;
				enemyName = cl.players[sorted_players_by_frags[0].playernum].name;
			}
		}
	}

	SCR_HUD_DrawNum(hud, teamFrags - enemyFrags, (colorize->integer) ? ((teamFrags - enemyFrags) < 0 || colorize->integer > 1) : false, scale->value, style->value, digits->value, align->string, proportional->integer);
}

static void SCR_HUD_DrawScoresPosition(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *digits, *align, *colorize, *proportional;
	int teamFrags = 0, enemyFrags = 0, position = 0, i = 0;
	char* teamName = 0, *enemyName = 0;

	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		digits = HUD_FindVar(hud, "digits");
		align = HUD_FindVar(hud, "align");
		colorize = HUD_FindVar(hud, "colorize");
		proportional = HUD_FindVar(hud, "proportional");
	}

	SCR_Hud_GetScores(&teamFrags, &enemyFrags, &teamName, &enemyName);

	position = 1;
	if (cl.teamplay) {
		for (i = 0; i < n_teams; ++i) {
			if (sorted_teams[i].frags > teamFrags) {
				++position;
			}
		}
	}
	else {
		for (i = 0; i < n_players + n_spectators; ++i) {
			if (cl.players[sorted_players[i].playernum].frags > teamFrags) {
				++position;
			}
		}
	}

	SCR_HUD_DrawNum(hud, position, (colorize->integer) ? (position != 1 || colorize->integer > 1) : false, scale->value, style->value, digits->value, align->string, proportional->integer);
}

/*
	ezQuake's analogue of +scores of KTX
	( t:x e:x [x] )
*/
static void SCR_HUD_DrawScoresBar(hud_t *hud)
{
	static	cvar_t *scale = NULL, *style, *format_big, *format_small, *frag_length, *reversed_big, *reversed_small, *proportional;
	int		width = 0, height = 0, x, y;
	int		i = 0;

	int		s_team = 0, s_enemy = 0, s_difference = 0;
	char	*n_team = "T", *n_enemy = "E";
	qbool   swappedOrder = false;

	char	buf[MAX_MACRO_STRING];
	char	c, *out, *temp, *in;
	int     frag_digits = 1;

	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		format_big = HUD_FindVar(hud, "format_big");
		format_small = HUD_FindVar(hud, "format_small");
		frag_length = HUD_FindVar(hud, "frag_length");
		reversed_big = HUD_FindVar(hud, "format_reversed_big");
		reversed_small = HUD_FindVar(hud, "format_reversed_small");
		proportional = HUD_FindVar(hud, "proportional");
	}

	frag_digits = max(1, min(frag_length->value, 4));

	swappedOrder = SCR_Hud_GetScores(&s_team, &s_enemy, &n_team, &n_enemy);
	s_difference = s_team - s_enemy;

	// two pots of delicious customized copypasta from math_tools.c
	switch (style->integer) {
		// Big
		case 1:
			in = TP_ParseFunChars(swappedOrder && reversed_big->string[0] ? reversed_big->string : format_big->string, false);
			buf[0] = 0;
			out = buf;

			while ((c = *in++) && (out - buf < MAX_MACRO_STRING - 1)) {
				if ((c == '%') && *in) {
					switch ((c = *in++)) {
						// c = colorize, r = reset
						case 'd':
							temp = va("%d", s_difference);
							width += (s_difference >= 0) ? strlen(temp) * 24 : ((strlen(temp) - 1) * 24) + 16;
							break;
						case 'D':
							temp = va("c%dr", s_difference);
							width += (s_difference >= 0) ? (strlen(temp) - 2) * 24 : ((strlen(temp) - 3) * 24) + 16;
							break;
						case 'e':
							temp = va("%d", s_enemy);
							width += (s_enemy >= 0) ? strlen(temp) * 24 : ((strlen(temp) - 1) * 24) + 16;
							break;
						case 'E':
							temp = va("c%dr", s_enemy);
							width += (s_enemy >= 0) ? (strlen(temp) - 2) * 24 : ((strlen(temp) - 3) * 24) + 16;
							break;
						case 'p':
							temp = va("%d", i + 1);
							width += 24;
							break;
						case 't':
							temp = va("%d", s_team);
							width += (s_team >= 0) ? strlen(temp) * 24 : ((strlen(temp) - 1) * 24) + 16;
							break;
						case 'T':
							temp = va("c%dr", s_team);
							width += (s_team >= 0) ? (strlen(temp) - 2) * 24 : ((strlen(temp) - 3) * 24) + 16;
							break;
						case 'z':
							if (s_difference >= 0) {
								temp = va("%d", s_difference);
								width += strlen(temp) * 24;
							}
							else {
								temp = va("c%dr", -(s_difference));
								width += (strlen(temp) - 2) * 24;
							}
							break;
						case 'Z':
							if (s_difference >= 0) {
								temp = va("c%dr", s_difference);
								width += (strlen(temp) - 2) * 24;
							}
							else {
								temp = va("%d", -(s_difference));
								width += strlen(temp) * 24;
							}
							break;
						default:
							temp = NULL;
							break;
					}

					if (temp != NULL) {
						strlcpy(out, temp, sizeof(buf) - (out - buf));
						out += strlen(temp);
					}
				}
				else if (c == ':' || c == '/' || c == '-' || c == ' ') {
					width += 16;
					*out++ = c;
				}
			}
			*out = 0;
			break;

			// Small
		case 0:
		default:
			in = TP_ParseFunChars(swappedOrder && reversed_small->string[0] ? reversed_small->string : format_small->string, false);
			buf[0] = 0;
			out = buf;

			while ((c = *in++) && (out - buf < MAX_MACRO_STRING - 1)) {
				if ((c == '%') && *in) {
					int fixed_width = 0, alignment = 0;

					if (in[0] == '<') {
						--in;
						TP_ReadMacroFormat(in, &fixed_width, &alignment, &in);
						++in;
					}

					switch ((c = *in++)) {
						case '%':
							temp = "%";
							break;
						case 't':
							temp = va("%*d", frag_digits, s_team);
							break;
						case 'e':
							temp = va("%*d", frag_digits, s_enemy);
							break;
						case 'd':
							temp = va("%d", s_difference);
							break;
						case 'p':
							temp = va("%d", i + 1);
							break;
						case 'T':
							temp = n_team;
							break;
						case 'E':
							temp = n_enemy;
							break;
						case 'D':
							temp = va("%+d", s_difference);
							break;
						default:
							temp = va("%%%c", c);
							break;
					}
					if (fixed_width) {
						temp = TP_AlignMacroText(temp, fixed_width, alignment);
					}
					strlcpy(out, temp, sizeof(buf) - (out - buf));
					out += strlen(temp);
				}
				else {
					*out++ = c;
				}
			}
			*out = 0;
			break;
	}

	switch (style->integer) {
		// Big
		case 1:
			width *= scale->value;
			height = 24 * scale->value;

			if (HUD_PrepareDraw(hud, width, height, &x, &y)) {
				SCR_DrawWadString(x, y, scale->value, buf);
			}
			break;

			// Small
		case 0:
		default:
			width = Draw_StringLengthColors(buf, -1, scale->value, proportional->integer);
			height = 8 * scale->value;

			if (HUD_PrepareDraw(hud, width, height, &x, &y)) {
				Draw_SString(x, y, buf, scale->value, proportional->integer);
			}
			break;
	}
}

static void SCR_HUD_DrawOwnFrags(hud_t *hud)
{
	// not implemented yet: scale, color
	// fixme: add appropriate opengl functions that will add alpha, scale and color
	int width;
	int height = LETTERHEIGHT;
	int x, y;
	double alpha;
	static cvar_t
		*hud_ownfrags_timeout = NULL,
		*hud_ownfrags_scale = NULL,
		*hud_ownfrags_proportional = NULL;
	extern qbool hud_editor;

	if (hud_ownfrags_timeout == NULL) {
		// first time
		hud_ownfrags_scale = HUD_FindVar(hud, "scale");
		hud_ownfrags_timeout = HUD_FindVar(hud, "timeout");
		hud_ownfrags_proportional = HUD_FindVar(hud, "proportional");
	}

	if (hud_editor) {
		HUD_PrepareDraw(hud, 10 * LETTERWIDTH, height * hud_ownfrags_scale->value, &x, &y);
	}

	width = VX_OwnFragTextLen(hud_ownfrags_scale->value, hud_ownfrags_proportional->integer);
	if (!width) {
		return;
	}

	if (VX_OwnFragTime() > hud_ownfrags_timeout->value) {
		return;
	}

	height *= hud_ownfrags_scale->value;

	alpha = 2 - hud_ownfrags_timeout->value / VX_OwnFragTime() * 2;
	alpha = bound(0, alpha, 1);

	if (!HUD_PrepareDraw(hud, width, height, &x, &y)) {
		return;
	}

	if (VX_OwnFragTime() < hud_ownfrags_timeout->value) {
		Draw_SString(x, y, VX_OwnFragText(), hud_ownfrags_scale->value, hud_ownfrags_proportional->integer);
	}
}

void Scores_HudInit(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_HUD);
	Cvar_Register(&hud_sortrules_playersort);
	Cvar_Register(&hud_sortrules_teamsort);
	Cvar_Register(&hud_sortrules_includeself);
	Cvar_ResetCurrentGroup();

	HUD_Register(
		"score_team", NULL, "Own scores or team scores.",
		0, ca_active, 0, SCR_HUD_DrawScoresTeam,
		"0", "screen", "left", "bottom", "0", "0", "0.5", "4 8 32", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "0",
		"colorize", "0",
		"proportional", "0",
		NULL
	);

	HUD_Register(
		"score_enemy", NULL, "Scores of enemy or enemy team.",
		0, ca_active, 0, SCR_HUD_DrawScoresEnemy,
		"0", "score_team", "after", "bottom", "0", "0", "0.5", "32 4 0", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "0",
		"colorize", "0",
		"proportional", "0",
		NULL
	);

	HUD_Register(
		"score_difference", NULL, "Difference between teamscores and enemyscores.",
		0, ca_active, 0, SCR_HUD_DrawScoresDifference,
		"0", "score_enemy", "after", "bottom", "0", "0", "0.5", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "0",
		"colorize", "1",
		"proportional", "0",
		NULL
	);

	HUD_Register("score_position", NULL, "Position on scoreboard.",
		0, ca_active, 0, SCR_HUD_DrawScoresPosition,
		"0", "score_difference", "after", "bottom", "0", "0", "0.5", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "0",
		"colorize", "1",
		"proportional", "0",
		NULL
	);

	HUD_Register(
		"score_bar", NULL, "Team, enemy, and difference scores together.",
		HUD_PLUSMINUS, ca_active, 0, SCR_HUD_DrawScoresBar,
		"0", "screen", "center", "console", "0", "0", "0.5", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"format_small", "&c69f%T&r:%t &cf10%E&r:%e $[%D$]",
		"format_big", "%t:%e:%Z",
		"format_reversed_big", "",
		"format_reversed_small", "",
		"frag_length", "0",
		"proportional", "0",
		NULL
	);

	HUD_Register(
		"ownfrags" /* jeez someone give me a better name please */,
		NULL, "Highlights your own frags",
		0, ca_active, 1, SCR_HUD_DrawOwnFrags,
		"1", "screen", "center", "top", "0", "50", "0.2", "0 0 100", NULL,
		/*
		"color", "255 255 255",
		*/
		"timeout", "3",
		"scale", "1.5",
		"proportional", "0",
		NULL
	);
}
