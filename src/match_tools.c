/*

Copyright (C) 2002-2003       A Nourai

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

	$Id: match_tools.c,v 1.32 2007-10-13 01:59:54 himan Exp $
*/


#include "quakedef.h"
#include <time.h>
#include "logging.h"
#include "gl_model.h"
#include "teamplay.h"
#include "utils.h"
#include <curl/curl.h>
#include "sha1.h"
#include <time.h>

void MT_Challenge_StartMatch(void);
void MT_Challenge_Cancel(void);
void MT_Challenge_InitCvars(void);

#define MAX_STATIC_STRING 1024

cvar_t match_format_solo = {"match_format_solo", "solo/%n - [%M]"};
cvar_t match_format_coop = {"match_format_coop", "coop/%n - [%C_player_coop] - [%M]"};
cvar_t match_format_race = {"match_format_race", "race/%n - [race] - [%M]"};

cvar_t match_format_duel = {"match_format_duel", "duel/%n - %p%v%e - [dmm%D] - [%M]"};
cvar_t match_format_ffa = {"match_format_ffa", "ffa/%n - [%C_player_ffa] - [%M]"};
cvar_t match_format_2on2 = {"match_format_2on2", "2on2/%n - [%k%v%l] - [%M]"};
cvar_t match_format_3on3 = {"match_format_3on3", "tdm/%n - [%Oon%E_%t%v%e] - [%M]"};
cvar_t match_format_4on4 = {"match_format_4on4", "tdm/%n - [%Oon%E_%t%v%e] - [%M]"};
cvar_t match_format_tdm = {"match_format_tdm", "tdm/%n - [%Oon%E_%t%v%e] - [%M]"};
cvar_t match_format_multiteam = {"match_format_multiteam", "tdm/%n - [%a_%b] - [%M]"};

cvar_t match_format_arena = {"match_format_arena", "arena/%n - %p%v%e - [%F_frags] - [%M]"};
cvar_t match_format_tf_duel = {"match_format_tf_duel", "tfduel/%n - %p%v%e [%M]"};
cvar_t match_format_tf_clanwar = {"match_format_tf_clanwar", "tfwar/%n - [%Oon%E_%t%v%e] - [%M]"};

cvar_t match_name_and = {"match_name_and", "_&_"};			
cvar_t match_name_versus = {"match_name_versus", "_vs_"};	
cvar_t match_name_on = {"match_name_on", "on"};				
cvar_t match_name_nick = {"match_name_nick", ""};			
cvar_t match_name_spec = {"match_name_spec", "(SPEC)"};		

char *CL_DemoDirectory(void);

static char *MT_CleanString(char *string, qbool allow_spaces_and_slashes) {
	byte *in, *out, c, d, *disallowed;
	static byte buf[MAX_STATIC_STRING], badchars[] = {' ', '\\', '/', '?', '*', ':', '<', '>', '"', '|', '\0'};
	extern char readableChars[];

	disallowed = allow_spaces_and_slashes ? badchars + 3 : badchars;

	#define CLEANCHAR(c) \
		((readableChars[(byte) c] < ' ' || strchr((char *)disallowed, readableChars[(byte) c])) ? '_' : readableChars[(byte) c])

	in = (byte *) string;
	out = buf;

	while ((c = *in++) && out - buf < sizeof(buf) - 1) {
		d = CLEANCHAR(c);
		*out++ = d;
		if (d == '_') {
			for ( ; *in && CLEANCHAR(*in) == '_'; in++)
				;
		}
	}

	#undef CLEANCHAR

	*out = 0;
	Util_Process_Filename((char *) buf);
	return (char *) buf;
}

char* MT_PlayerName(void)
{
	return TP_PlayerName();
}

char* MT_PlayerTeam(void)
{
	return TP_PlayerTeam();
}

char* MT_EnemyName(void)
{
	int i;
	char *myname, *name;
	static char	enemyname[MAX_INFO_STRING];

	enemyname[0] = 0;
	myname = MT_PlayerName ();

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].name[0] && !cl.players[i].spectator) {
			name = Info_ValueForKey(cl.players[i].userinfo, "name");
			if (strcmp(name, myname)) {
				strlcpy(enemyname, name, sizeof (enemyname));
				return enemyname;
			}
		}
	}
	return enemyname;
}

static char *MT_EnemyTeam(void) {
	int i;
	char *myteam, *team;
	static char	enemyteam[MAX_INFO_STRING];

	enemyteam[0] = 0;
	myteam = MT_PlayerTeam();

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].name[0] && !cl.players[i].spectator) {
			team = cl.players[i].team;
			if (team[0] && strcmp(team, myteam)) {
				strlcpy (enemyteam, team, sizeof (enemyteam));
				return enemyteam;
			}
		}
	}
	return enemyteam;
}

int MT_CountPlayers(void)
{
	return TP_CountPlayers();
}

static int MT_CountTeamMembers(char *team)
{
	int i, count = 0;

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!cl.players[i].name[0] || cl.players[i].spectator)
			continue;
		if (!strcmp(cl.players[i].team, team))
			count++;
	}
	return count;
}

static char *MT_NameAndClean_TeamMembers(char *team) {
	static char namebuf[MAX_STATIC_STRING];
	int i;

	namebuf[0] = 0;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!cl.players[i].name[0] || cl.players[i].spectator)
			continue;
		if (!strcmp(cl.players[i].team, team)) {
			if (namebuf[0])
				strlcat(namebuf, match_name_and.string, sizeof (namebuf) - strlen (namebuf));

			strlcat(namebuf, MT_CleanString(cl.players[i].name, false), sizeof (namebuf) - strlen (namebuf));
		}
	}
	return namebuf;
}

static char *MT_MapName(void) {
	static char buf[MAX_OSPATH];

	strlcpy(buf, TP_MapName(), sizeof(buf));
	return buf;
}

static void MT_GetPlayerNames(char *name1, char *name2) {
	int i;
	char *s1 = NULL, *s2 = NULL;

	name1[0] = name2[0] = 0;

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!cl.players[i].name[0] || cl.players[i].spectator)
			continue;
		if (!s1) {
			s1 = cl.players[i].name;
		} else {
			s2 = cl.players[i].name;
			break;
		}
	}
	if (s1)
		strcpy(name1, s1);
	if (s2)
		strcpy(name2, s2);
}

static int MT_GetTeamNames(char teams[][MAX_INFO_STRING], int max) {
	int i, j, count = 0;

	memset(teams, 0, sizeof(*teams));

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!cl.players[i].name[0] || cl.players[i].spectator)
			continue;

		for (j = 0; j < i; j++) {
			if (!cl.players[j].name[0] || cl.players[j].spectator)
				continue;

			if (!strcmp(cl.players[i].team, cl.players[j].team))
				break;
		}
		if (j == i) {
			strlcpy (teams[count], cl.players[i].team, MAX_INFO_STRING);
			count++;
		}
		if (count == max)
			break;
	}
	return count;
}

static char *MT_Serverinfo_Race(void) {
	static char buf[MAX_OSPATH];

	strlcpy(buf, Info_ValueForKey(cl.serverinfo, "race"), sizeof(buf));
	return buf;
}


typedef enum {
	mt_duel, mt_ffa,
	mt_2on2, mt_3on3, mt_4on4, mt_tdm,
	mt_multiteam,
	mt_arena,
	mt_tf_duel, mt_tf_clanwar,
	mt_solo,		
	mt_coop,		
	mt_race,		
	mt_empty,		
	mt_unknown_gamedir,
	mt_unknown,
	mt_numtypes,
} matchtype_t;

typedef struct matchcvar_s {
	matchtype_t matchtype;
	char		*nickname;
	cvar_t		*format;
	qbool	autosshot;
} matchcvar_t;

static matchcvar_t matchcvars[mt_numtypes] = {
	{mt_duel, "duel", &match_format_duel, true},
	{mt_ffa, "ffa", &match_format_ffa, true},
	{mt_2on2, "2on2", &match_format_2on2, true},
	{mt_3on3, "3on3", &match_format_3on3, true},
	{mt_4on4, "4on4", &match_format_4on4, true},
	{mt_tdm, "tdm", &match_format_tdm, true},
	{mt_multiteam, "tdm", &match_format_multiteam, true},
	{mt_arena, "arena", &match_format_arena, true},
	{mt_tf_duel, "tfduel", &match_format_tf_duel, true},
	{mt_tf_clanwar, "tfclanwar", &match_format_tf_clanwar, true},
	{mt_solo, "solo", &match_format_solo, false},
	{mt_coop, "coop", &match_format_coop, false},
	{mt_race, "race", &match_format_race, false},
	{mt_empty, "empty", NULL, false},
	{mt_unknown_gamedir, "unknown", NULL, false},
	{mt_unknown, "unknown", NULL, false},
};

typedef struct matchinfo_s {
	qbool spectator;
	char myname[MAX_INFO_STRING];
	char player1[MAX_INFO_STRING];
	char player2[MAX_INFO_STRING];
	char team1[MAX_INFO_STRING];
	char team2[MAX_INFO_STRING];
	int team1count;
	int team2count;
	int numteams;
	char team1names[MAX_STATIC_STRING];	
	char team2names[MAX_STATIC_STRING];	
	char multiteamnames[MAX_STATIC_STRING];	
	char multiteamcounts[128];				
	int maxteamsize;
	int numplayers;
	int timelimit;
	int fraglimit;
	int teamplay;
	int maxclients;
	int deathmatch;
	char mapname[MAX_OSPATH];
	char gamedir[MAX_OSPATH];
	matchtype_t matchtype;
	char day[8];
	char month[8];
	char year[8];
	char bigyear[8];
	char hour[8];
	char minute[8];
	char second[8];
} matchinfo_t;

static matchtype_t MT_GetMatchType(matchinfo_t *matchinfo) {

	if (matchinfo->numplayers < 1)
		return mt_empty;


	if (!strcasecmp(matchinfo->gamedir, "qw") && !strcmp(MT_Serverinfo_Race(), matchinfo->mapname))
		return mt_race;


	if (matchinfo->numplayers < 2)
		return mt_solo;


	if (cl.teamfortress) {
		return	(matchinfo->numplayers == 2) ? mt_tf_duel : 
				(matchinfo->numteams < 2) ? mt_unknown : mt_tf_clanwar;
	}

	if (strstr(matchinfo->gamedir, "arena"))
		return mt_arena;

	if (!matchinfo->deathmatch)
		return mt_coop;


	if (matchinfo->numplayers == 2)
		return (!matchinfo->teamplay || matchinfo->numteams == 2) ? mt_duel : mt_unknown;

	if (!matchinfo->teamplay || matchinfo->maxteamsize <= 1)
		return mt_ffa;


	if (matchinfo->numteams !=  2)
		return (matchinfo->numteams > 2) ? mt_multiteam : mt_unknown;

	switch (matchinfo->maxteamsize) {
		case 2: return mt_2on2;
		case 3: return mt_3on3;
		case 4: return mt_4on4;
		default: return mt_tdm;
	}
}

static matchinfo_t *MT_GetMatchInfo(void) {
	static matchinfo_t matchinfo;
	char teamnames[MAX_CLIENTS][MAX_INFO_STRING];
	int i, numteams, maxteamsize, teamsize;
	time_t t;
	struct tm *ptm;

	memset(&matchinfo, 0, sizeof(matchinfo));
	numteams = MT_GetTeamNames(teamnames, MAX_CLIENTS);

	matchinfo.spectator = cl.spectator;
	strlcpy(matchinfo.myname, MT_PlayerName(), sizeof(matchinfo.myname));

	if (cl.spectator) {
		MT_GetPlayerNames(matchinfo.player1, matchinfo.player2);
		strlcpy(matchinfo.team1, teamnames[0], sizeof(matchinfo.team1));
		strlcpy(matchinfo.team2, teamnames[1], sizeof(matchinfo.team2));
	} else {
		strlcpy(matchinfo.player1, MT_PlayerName(), sizeof(matchinfo.player1));
		strlcpy(matchinfo.player2, MT_EnemyName(), sizeof(matchinfo.player2));
		strlcpy(matchinfo.team1, MT_PlayerTeam(), sizeof(matchinfo.team1));
		strlcpy(matchinfo.team2, MT_EnemyTeam(), sizeof(matchinfo.team2));
	}


	matchinfo.team1count = MT_CountTeamMembers(matchinfo.team1);
	matchinfo.team2count = MT_CountTeamMembers(matchinfo.team2);
	matchinfo.numteams = numteams;
	strlcpy(matchinfo.team1names, MT_NameAndClean_TeamMembers(matchinfo.team1), sizeof(matchinfo.team1names));
	strlcpy(matchinfo.team2names, MT_NameAndClean_TeamMembers(matchinfo.team2), sizeof(matchinfo.team2names));


#define CLEANFIELD(x) strlcpy(matchinfo.x, MT_CleanString(matchinfo.x, false), sizeof(matchinfo.x));	
	CLEANFIELD(myname);
	CLEANFIELD(player1);
	CLEANFIELD(player2);
	CLEANFIELD(team1);
	CLEANFIELD(team2);
#undef CLEANFIELD

#define BUF matchinfo.multiteamnames
	for (i = 0; i < numteams; i++) {
		strlcat (BUF, MT_CleanString(teamnames[i], false), sizeof (BUF) - strlen (BUF));
		if (i < numteams - 1)
			strlcat (BUF, match_name_versus.string, sizeof (BUF) - strlen (BUF));
	}
#undef BUF

	maxteamsize = 0;
#define BUF matchinfo.multiteamcounts
	for (i = 0; i < numteams; i++) {
		teamsize = MT_CountTeamMembers(teamnames[i]);
		if (*teamnames[i])
			maxteamsize = max(maxteamsize, teamsize);
		strlcat (BUF, va("%d", teamsize), sizeof (BUF) - strlen (BUF));
		if (i < numteams - 1)
			strlcat(BUF, match_name_on.string, sizeof (BUF) - strlen (BUF));
	}
	matchinfo.maxteamsize = maxteamsize;
#undef BUF

	matchinfo.numplayers = MT_CountPlayers();

	matchinfo.timelimit = Q_atoi(Info_ValueForKey(cl.serverinfo, "timelimit"));
	matchinfo.fraglimit = Q_atoi(Info_ValueForKey(cl.serverinfo, "fraglimit"));
	matchinfo.teamplay = Q_atoi(Info_ValueForKey(cl.serverinfo, "teamplay"));
	matchinfo.maxclients = Q_atoi(Info_ValueForKey(cl.serverinfo, "maxclients"));
	matchinfo.deathmatch = cl.deathmatch;

	strlcpy(matchinfo.mapname, MT_MapName(), sizeof(matchinfo.mapname));
	strlcpy(matchinfo.gamedir, cls.gamedirfile, sizeof(matchinfo.gamedir));

	matchinfo.matchtype = MT_GetMatchType(&matchinfo);

	time(&t);
	if ((ptm = localtime(&t))) {
		strftime (matchinfo.day, sizeof(matchinfo.day) - 1, "%d", ptm);
		strftime (matchinfo.month, sizeof(matchinfo.month) - 1, "%m", ptm);
		strftime (matchinfo.year, sizeof(matchinfo.year) - 1, "%y", ptm);
		strftime (matchinfo.bigyear, sizeof(matchinfo.bigyear) - 1, "%Y", ptm);
		strftime (matchinfo.hour, sizeof(matchinfo.hour) - 1, "%H", ptm);
		strftime (matchinfo.minute, sizeof(matchinfo.minute) - 1, "%M", ptm);
		strftime (matchinfo.second, sizeof(matchinfo.second) - 1, "%S", ptm);
	}

	return &matchinfo;
}

void MT_Macrolist_f(void) {
	int argc;

	switch((argc = Cmd_Argc())) {
	case 1:
		Com_Printf("\x02The following macros can be used to name your matches\n");
		Com_Printf("\x02The square brackets apply when spectating a match\n\n");
		Com_Printf("\x02%%n"); Com_Printf(" - your nick [followed by match_name_spec]\n");
		Com_Printf("\x02%%p"); Com_Printf(" - your name [player1's name]\n");
		Com_Printf("\x02%%t"); Com_Printf(" - your team [team1's name]\n");
		Com_Printf("\x02%%e"); Com_Printf(" - enemy nick in duels, enemy team in tp [player2/team2]\n");

		Com_Printf("\x02%%k"); Com_Printf(" - names of players on your team [team1]\n");
		Com_Printf("\x02%%l"); Com_Printf(" - names of players on enemy team [team2]\n");

		Com_Printf("\x02%%O"); Com_Printf(" - number of teammates [number on team1]\n");
		Com_Printf("\x02%%E"); Com_Printf(" - number of enemies [number on team2]\n");
		Com_Printf("\x02%%C"); Com_Printf(" - number of players on the server\n");

		Com_Printf("\x02%%a"); Com_Printf(" - team counts separated by match_name_on (eg 4on3on4)\n");
		Com_Printf("\x02%%b"); Com_Printf(" - team names separated by match_name_versus\n");
		Com_Printf("\x02%%v"); Com_Printf(" - shortcut for $match_name_versus\n");

		Com_Printf("\x02%%T"); Com_Printf(" - timelimit on the server\n");
		Com_Printf("\x02%%F"); Com_Printf(" - fraglimit n the server\n");
		Com_Printf("\x02%%p"); Com_Printf(" - teamplay setting on the server\n");
		Com_Printf("\x02%%D"); Com_Printf(" - deathmatch mode on the server\n");

		Com_Printf("\x02%%M"); Com_Printf(" - mapname\n");
		Com_Printf("\x02%%G"); Com_Printf(" - gamedir (eg. qw, fortress, arena, etc)\n");

		Com_Printf("\x02%%d"); Com_Printf(" - day\n");
		Com_Printf("\x02%%m"); Com_Printf(" - month\n");
		Com_Printf("\x02%%y"); Com_Printf(" - year (without century)\n");
		Com_Printf("\x02%%Y"); Com_Printf(" - year (with century)\n");

		Com_Printf("\x02%%H"); Com_Printf(" - hour\n");
		Com_Printf("\x02%%Q"); Com_Printf(" - minute\n");
		Com_Printf("\x02%%S"); Com_Printf(" - second\n");

		break;
	default:
		Com_Printf("%s : no arguments expected\n", Cmd_Argv(0));
		break;
	}
}

static char *MT_ParseFormat(char *format, matchinfo_t *matchinfo) {
	static char buf[MAX_STATIC_STRING];
	char c, *out, *in, *temp;

	buf[0] = 0;
	out = buf;
	in = format;

	while ((c = *in++) && (out - buf < MAX_STATIC_STRING - 1)) {
		if ((c == '%') && *in) {
			switch((c = *in++)) {
				case '%':
					temp = "%"; break;
				case 'n':
					temp = match_name_nick.string[0] ? match_name_nick.string : matchinfo->myname;
					temp = cl.spectator ? va("%s%s", temp, match_name_spec.string) : temp;
					break;
				case 'p':
					temp = matchinfo->player1; break;
				case 't':
					temp = matchinfo->team1; break;
				case 'e':
					temp = matchinfo->numplayers == 2 ? matchinfo->player2 : matchinfo->team2; break;
				case 'k':
					temp = matchinfo->team1names; break;
				case 'l':
					temp = matchinfo->team2names; break;
				case 'O':
					temp = matchinfo->teamplay ? va("%d", matchinfo->team1count) : "1"; break;
				case 'E':
					temp = va("%d", matchinfo->teamplay ? matchinfo->team2count : matchinfo->numplayers - 1); break;
				case 'C':
					temp = va("%d", matchinfo->numplayers); break;
				case 'a':
					temp = matchinfo->multiteamcounts; break;
				case 'b':
					temp = matchinfo->multiteamnames; break;
				case 'v':
					temp = match_name_versus.string; break;
				case 'T':
					temp = va("%d", matchinfo->timelimit); break;
				case 'F':
					temp = va("%d", matchinfo->fraglimit); break;
				case 'N':
					temp = va("%d", matchinfo->teamplay); break;
				case 'D':
					temp = va("%d", matchinfo->deathmatch); break;
				case 'M':
					temp = matchinfo->mapname; break;
				case 'G':
					temp = matchinfo->gamedir; break;
				case 'd':
					temp = matchinfo->day; break;
				case 'm':
					temp =  matchinfo->month; break;
				case 'y':
					temp = matchinfo->year; break;
				case 'Y':
					temp = matchinfo->bigyear; break;
				case 'H':
					temp = matchinfo->hour; break;
				case 'Q':
					temp = matchinfo->minute; break;
				case 'S':
					temp = matchinfo->second; break;
				default:
					temp = va("%%%c", c); break;
			}
			strlcpy(out, temp, sizeof(buf) - (out - buf));
			out += strlen(temp);
		} else {
			*out++ = c;
		}
	}

	*out = 0;
	return buf;
}

static char *MT_NameForMatchInfo(matchinfo_t *matchinfo) {
	char *format = NULL;

	switch (matchinfo->matchtype) {
	case mt_empty:
		format = "%n - [%M]"; break;
	case mt_unknown_gamedir:
		format = "%n - [gamedir - %G, %C players] - [%M]"; break;
	case mt_unknown:
		format = "%n - [Unknown Game, gamedir - %G, %C players] - [%M]"; break;
	default:
		if (matchinfo->matchtype < mt_numtypes) {
			format = matchcvars[matchinfo->matchtype].format->string;
		} else {
			Sys_Error("Macro_Matchdesc : Unknown match type %d", matchinfo->matchtype);
		}
		break;
	}
	if (!format)
		Sys_Error("MT_NameForMatchInfo: NULL format");

	return MT_CleanString(MT_ParseFormat(format, matchinfo), true);
}

char *Macro_MatchName(void) {
	return (cls.state < ca_active) ? "No match in progress" : MT_NameForMatchInfo(MT_GetMatchInfo());
}

char *MT_MatchName(void) {
	static char buf[MAX_STATIC_STRING];

	strlcpy(buf, Macro_MatchName(), sizeof(buf));
	return buf;
}

char *MT_ShortStatus(void)
{
	int maxclients = Q_atoi(Info_ValueForKey(cl.serverinfo, "maxclients"));
	char *mapname = TP_MapName();

	return va("%d/%d - %s", TP_CountPlayers(), maxclients, mapname);
}

#define MT_SCOREBOARD_SHOWIME	4

void MT_TakeScreenshot(void);

cvar_t match_auto_record = {"match_auto_record", "0"};
cvar_t match_auto_logconsole = {"match_auto_logconsole", "1"};
cvar_t match_auto_sshot = {"match_auto_sshot", "0"};
cvar_t match_auto_minlength = {"match_auto_minlength", "30"};
cvar_t match_auto_spectating = {"match_auto_spectating", "0"};
cvar_t match_auto_unminimize = {"match_auto_unminimize", "1"};

typedef struct mt_matchtstate_s {
	qbool standby;
	int intermission;
	int status;
	float starttime;
	float endtime;
	char matchname[2 * MAX_OSPATH];
	matchtype_t matchtype;
} mt_matchstate_t;

static mt_matchstate_t matchstate;

static void MT_Delayed_EndMatch(void) {
	matchstate.endtime = 0;

	Log_AutoLogging_StopMatch();
	CL_AutoRecord_StopMatch();
}

static void MT_EndMatch(void) {
	MT_TakeScreenshot();

	if (!matchstate.status)
		return;

	matchstate.starttime = 0;
	matchstate.status = 0;
 	matchstate.endtime = cls.realtime;
}

static void MT_CancelMatch(void) {
	if (matchstate.endtime) {
		MT_Delayed_EndMatch();
	}

	if (!matchstate.status)
		return;

	matchstate.starttime = 0;
	matchstate.status = 0;
	matchstate.endtime = 0;	

	Log_AutoLogging_CancelMatch();
	CL_AutoRecord_CancelMatch();
	MT_Challenge_Cancel();
}

static void MT_StartMatch(void)
{
	if (matchstate.endtime) {
		MT_Delayed_EndMatch();
	}

	if (matchstate.status) {
		MT_CancelMatch();
	}

	matchstate.status = 1;
	matchstate.starttime = cls.realtime;
	matchstate.endtime = 0;

	// disconnect: match_forcestart resets gameclock
	cl.standby = false;
	cl.countdown = false;
	cl.gametime = 0;
	cl.gamestarttime = Sys_DoubleTime();

	if (cls.state < ca_active) {
		matchstate.matchtype = mt_empty;
		strlcpy(matchstate.matchname, "No match in progress", sizeof(matchstate.matchname));
	} else {
		matchinfo_t *matchinfo = MT_GetMatchInfo();
		matchstate.matchtype = matchinfo->matchtype;
		strlcpy(matchstate.matchname, MT_NameForMatchInfo(matchinfo), sizeof(matchstate.matchname));
	}

	MT_Challenge_StartMatch();

	CL_AutoRecord_StartMatch(matchstate.matchname);
	Log_AutoLogging_StartMatch(matchstate.matchname);

	Stats_Reset();
}

static void MT_ClearClientState(void)
{
	memset(&matchstate, 0, sizeof(matchstate));
	cl.gamestarttime = Sys_DoubleTime();
	cl.gamepausetime = 0;
}

void MT_Frame(void)
{
	if (matchstate.endtime && cls.realtime >= matchstate.endtime + MT_SCOREBOARD_SHOWIME) {
		MT_Delayed_EndMatch();
	}

	if (cls.state != ca_active || cls.demoplayback) {
		return;
	}

	if (matchstate.standby && !cl.standby && !cl.intermission) {
		if (!cl.spectator || match_auto_spectating.value) {
			MT_StartMatch();
		}

		if (match_auto_unminimize.integer == 2 || (match_auto_unminimize.integer == 1 && !cl.spectator)) {
			VID_Restore();
		}
	}

	if (!matchstate.intermission && cl.intermission) {
		MT_EndMatch();
	}
	else if (matchstate.status && !matchstate.standby && cl.standby) {
		MT_CancelMatch();
	}

	matchstate.standby = cl.standby;
	matchstate.intermission = cl.intermission;
}

void MT_NewMap(void)
{
	MT_CancelMatch();
	MT_ClearClientState();
}

void MT_Disconnect(void)
{
	MT_CancelMatch();
	MT_ClearClientState();
}

char* MT_TempDirectory(void)
{
	static char dir[MAX_OSPATH * 2] = {0};

	if (!dir[0]) {
		snprintf(dir, sizeof(dir), "%s/temp", com_homedir);
	}
	return dir;
}

char *MT_TempDemoDirectory(void)
{
	static char dir[MAX_OSPATH * 2] = {0};

	if (!dir[0])
		snprintf(dir, sizeof(dir), "%s/temp", CL_DemoDirectory());
	return dir;
}

void MT_SaveMatch_f(void)
{
	int demo_status, log_status;

	demo_status = CL_AutoRecord_Status();
	log_status = Log_AutoLogging_Status();

	if (Log_TempLogUploadPending()) {
		Com_Printf("Log upload is still in progress\n");
		return;
	}

	if ((demo_status & 2) || (log_status & 2)) {
		Com_Printf("\x02Saving match...\n");
		CL_AutoRecord_SaveMatch();
		Log_AutoLogging_SaveMatch(false);
	}
	else {
		if ((demo_status & 1) || (log_status & 1)) {
			if ((demo_status & 1) && (log_status & 1)) {
				Com_Printf("Auto demo recording and console logging still in progress\n");
			}
			else if ((demo_status & 1)) {
				Com_Printf("Auto demo recording still in progress\n");
			}
			else if ((log_status & 1)) {
				Com_Printf("Auto console logging still in progress\n");
			}
		}
		else {
			Com_Printf("Nothing to save!\n");
		}
	}
}

void MT_Match_ForceStart_f(void)
{
	switch (Cmd_Argc()) {
	case 1:
		if (cls.state != ca_active || cls.demoplayback) {
			Com_Printf("You must be connected to a server before using \"%s\"\n", Cmd_Argv(0));
			return;
		}
		MT_StartMatch();
		break;
	default:
		Com_Printf("%s : no arguments expected\n", Cmd_Argv(0));
		break;
	}
}

void MT_TakeScreenshot(void)
{
	int i;
	qbool have_opponent;

	if (!match_auto_sshot.value || (cl.spectator && !match_auto_spectating.value))
		return;

	//don't bother screen-shotting solo games etc
	if (!matchcvars[matchstate.matchtype].autosshot)
		return;

	//make sure there are actually some frags on the board, and somebody besides us
	have_opponent = false;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!cl.players[i].name[0] || cl.players[i].spectator)
			continue;
		if (i != cl.playernum)
			have_opponent = true;		
	}
	if (!have_opponent) {
		Com_Printf("Auto screenshot cancelled\n");
		return;
	}

	if (!matchstate.status) {
		matchinfo_t *matchinfo = MT_GetMatchInfo();
		matchstate.matchtype = matchinfo->matchtype;
		strlcpy(matchstate.matchname, MT_NameForMatchInfo(matchinfo), sizeof(matchstate.matchname));
	}
	SCR_AutoScreenshot(matchstate.matchname);
}

#define MAX_GROUP_MEMBERS	36

typedef struct mapgroup_s {
	char groupname[MAX_QPATH];
	char members[MAX_GROUP_MEMBERS][MAX_QPATH];
	struct mapgroup_s *next, *prev;
	qbool system;
	int nummembers;
} mapgroup_t;

static mapgroup_t *mapgroups = NULL;	
static mapgroup_t *last_system_mapgroup = NULL;
static qbool mapgroups_init = false;	

#define FIRSTUSERGROUP (last_system_mapgroup ? last_system_mapgroup->next : mapgroups)

static mapgroup_t *GetGroupWithName(char *groupname) {
	mapgroup_t *node;

	for (node = mapgroups; node; node = node->next) {
		if (!strcasecmp(node->groupname, groupname))
			return node;
	}
	return NULL;
}

static mapgroup_t *GetGroupWithMember(char *member) {
	int j;
	mapgroup_t *node;

	for (node = mapgroups; node; node = node->next) {
		for (j = 0; j < node->nummembers; j++) {
			if (!strcasecmp(node->members[j], member))
				return node;
		}
	}
	return NULL;
}

static void DeleteMapGroup(mapgroup_t *group) {
	if (!group)
		return;

	if (group->prev)
		group->prev->next = group->next;
	if (group->next)
		group->next->prev = group->prev;

	
	if (group == mapgroups) {
		mapgroups = mapgroups->next;		
		if (group == last_system_mapgroup)
			last_system_mapgroup = NULL;
	} else if (group == last_system_mapgroup) {
		last_system_mapgroup = last_system_mapgroup->prev;
	}

	Q_free(group);
}

static void ResetGroupMembers(mapgroup_t *group) {
	int i;

	if (!group)
		return;

	for (i = 0; i < group->nummembers; i++)
		group->members[i][0] = 0;

	group->nummembers = 0;
}

static void DeleteGroupMember(mapgroup_t *group, char *member) {
	int i;

	if (!group)
		return;

	for (i = 0; i < group->nummembers; i++) {
		if (!strcasecmp(member, group->members[i]))
			break;
	}

	if (i == group->nummembers)	
		return;

	if (i < group->nummembers - 1)
		memmove(group->members[i], group->members[i + 1], (group->nummembers - 1 - i) * sizeof(group->members[0]));

	group->nummembers--;
}

static void AddGroupMember(mapgroup_t *group, char *member) {
	int i;

	if (!group || group->nummembers == MAX_GROUP_MEMBERS)
		return;

	for (i = 0; i < group->nummembers; i++) {		
		if (!strcasecmp(member, group->members[i]))
			return;
	}

	strlcpy(group->members[group->nummembers], member, sizeof(group->members[group->nummembers]));
	group->nummembers++;
}

void MT_MapGroup_f(void) {
	int i, c, j;
	qbool removeflag = false;
	mapgroup_t *node, *group, *tempnode;
	char *groupname, *member;

	if ((c = Cmd_Argc()) == 1) {		
		if (!FIRSTUSERGROUP) {
			Com_Printf("No map groups defined\n");
		} else {
			for (node = FIRSTUSERGROUP; node; node = node->next) {
				Com_Printf("\x02%s: ", node->groupname);
				for (j = 0; j < node->nummembers; j++)
					Com_Printf("%s ", node->members[j]);
				Com_Printf("\n");
			}
		}
		return;
	}

	groupname = Cmd_Argv(1);

	if (c == 2 && !strcasecmp(groupname, "clear")) {	
		for (node = FIRSTUSERGROUP; node; node = tempnode) {
			tempnode = node->next;
			DeleteMapGroup(node);
		}
		return;
	}

	if (Util_Is_Valid_Filename(groupname) == false) {
		Com_Printf("Error: %s is not a valid map group name\n", groupname);
		return;
	}

	group = GetGroupWithName(groupname);

	if (c == 2) {	
		if (!group) {
			Com_Printf("No map group named \"%s\"\n", groupname);
		} else {
			Com_Printf("\x02%s: ", groupname);
			for (j = 0; j < group->nummembers; j++)
				Com_Printf("%s ", group->members[j]);
			Com_Printf("\n");
		}
		return;
	}

	if (group && group->system) {
		Com_Printf("Cannot modify system group \"%s\"\n", groupname);
		return;
	}

	if (c == 3 && !strcasecmp(Cmd_Argv(2), "clear")) {	
		if (!group)
			Com_Printf("\"%s\" is not a map group name\n", groupname);
		else
			DeleteMapGroup(group);
		return;
	}

	if (!group) {	
		group = (mapgroup_t *) Q_calloc(1, sizeof(mapgroup_t));
		strlcpy(group->groupname, groupname, sizeof(group->groupname));
		group->system = !mapgroups_init;
		if (mapgroups) {	
			for (tempnode = mapgroups; tempnode->next; tempnode = tempnode->next)
				;
			tempnode->next = group;
			group->prev = tempnode;
		} else {
			mapgroups = group;
		}
	} else {		
		member = Cmd_Argv(2);
		if (member[0] != '+' && member[0] != '-')
			ResetGroupMembers(group);
	}

	for (i = 2; i < c; i++) {
		member = Cmd_Argv(i);
		if (member[0] == '+') {
			removeflag = false;
			member++;
		} else if (member[0] == '-') {
			removeflag = true;
			member++;
		}

		if (!removeflag && (tempnode = GetGroupWithMember(member)) && tempnode != group) {
			if (cl_warncmd.integer || developer.integer)
				Com_Printf("Warning: \"%s\" is already a member of group \"%s\"...ignoring\n", member, tempnode->groupname);
			continue;
		}

		if (removeflag)
			DeleteGroupMember(group, member);
		else
			AddGroupMember(group, member);
	}

	if (!group->nummembers)	
		DeleteMapGroup(group);
}

void MT_AddMapGroups(void) {
	char exmy_group[256] = {0}, exmy_map[6] = {'e', 0, 'm', 0, ' ', 0};
	int i, j;
	mapgroup_t *tempnode;

	strlcat (exmy_group, "mapgroup exmy start ", sizeof (exmy_group));
	for (i = 1; i <= 4; i++) {
		for (j = 1; j <= 8; j++) {		
			exmy_map[1] = i + '0';
			exmy_map[3] = j + '0';
			strlcat(exmy_group, exmy_map, sizeof (exmy_group));
		}
	}
	Cmd_TokenizeString(exmy_group);
	MT_MapGroup_f();
	for (tempnode = mapgroups; tempnode->next; tempnode = tempnode->next)
		;
	last_system_mapgroup = tempnode;
	mapgroups_init = true;
}

char *MT_GetMapGroupName(char *mapname, qbool *system) {
	mapgroup_t *group;

	group = GetGroupWithMember(mapname);
	if (group && strcmp(mapname, group->groupname)) {
		if (system)
			*system = group->system;
		return group->groupname;
	} else {
		return NULL;
	}
}

void DumpMapGroups(FILE *f) {
	mapgroup_t *node;
	int j;

	if (!FIRSTUSERGROUP) {
		fprintf(f, "mapgroup clear\n");
		return;
	}
	for (node = FIRSTUSERGROUP; node; node = node->next) {
		fprintf(f, "mapgroup %s ", node->groupname);
		for (j = 0; j < node->nummembers; j++)
			fprintf(f, "%s ", node->members[j]);
		fprintf(f, "\n");
	}
}

char *Macro_MatchType(void) {
	matchinfo_t *matchinfo;

	if (cls.state < ca_active)
		return "No match in progress";

	matchinfo = MT_GetMatchInfo();

	return matchcvars[matchinfo->matchtype].nickname;
}

void MT_Init(void)
{
	char tmp_path[MAX_OSPATH] = {0};

	snprintf(&tmp_path[0], sizeof(tmp_path), "%s/ezquake/temp", com_basedir);
	Sys_mkdir(tmp_path);

	MT_ClearClientState();

	Cmd_AddMacro(macro_matchname, Macro_MatchName);
	Cmd_AddMacro(macro_matchtype, Macro_MatchType);

	Cmd_AddCommand("match_format_macrolist", MT_Macrolist_f);
	Cmd_AddCommand("match_forcestart", MT_Match_ForceStart_f);
	Cmd_AddCommand("match_save", MT_SaveMatch_f);

	Cvar_SetCurrentGroup(CVAR_GROUP_MATCH_TOOLS);

	Cvar_Register(&match_format_solo);
	Cvar_Register(&match_format_coop);
	Cvar_Register(&match_format_race);
	Cvar_Register(&match_format_duel);
	Cvar_Register(&match_format_ffa);
	Cvar_Register(&match_format_2on2);
	Cvar_Register(&match_format_3on3);
	Cvar_Register(&match_format_4on4);
	Cvar_Register(&match_format_tdm);
	Cvar_Register(&match_format_multiteam);
	Cvar_Register(&match_format_arena);
	Cvar_Register(&match_format_tf_duel);
	Cvar_Register(&match_format_tf_clanwar);

	Cvar_Register(&match_name_and);
	Cvar_Register(&match_name_versus);
	Cvar_Register(&match_name_on);

	Cvar_Register(&match_name_nick);
	Cvar_Register(&match_name_spec);

	Cvar_Register(&match_auto_record);
	Cvar_Register(&match_auto_logconsole);
	Cvar_Register(&match_auto_sshot);
	Cvar_Register(&match_auto_minlength);
	Cvar_Register(&match_auto_spectating);
	Cvar_Register(&match_auto_unminimize);

	Cvar_ResetCurrentGroup();

	MT_Challenge_InitCvars();
}

void MT_Shutdown(void)
{
	mapgroup_t* group = mapgroups;

	while (group) {
		mapgroup_t* next = group->next;
		Q_free(group);
		group = next;
	}
}
