/*
Copyright (C) 2003       A Nourai

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

    $Id: fragstats.c,v 1.22 2007-10-04 14:56:55 dkure Exp $
*/

#include "quakedef.h"
#include "vx_stuff.h"
#include "vx_tracker.h"

cvar_t cl_parsefrags = {"cl_parseFrags", "1"};
cvar_t cl_showFragsMessages = {"con_fragmessages", "1"};
cvar_t cl_loadFragfiles = {"cl_loadFragfiles", "1"};

cvar_t cl_useimagesinfraglog = {"cl_useimagesinfraglog", "0"};

#define FUH_FRAGFILE_VERSION_1_00	"1.00" /* for compatibility with fuh */
#define FRAGFILE_VERSION_1_00		"ezquake-1.00" /* fuh suggest such format */

void Update_FlagStatus(int player_num, char *team, qbool got_flag);

typedef enum msgtype_s {
	mt_fragged,
	mt_frags,
	mt_tkills,
	mt_tkilled,

	mt_death,
	mt_suicide,
	mt_frag,
	mt_tkill,
	mt_tkilled_unk,
	mt_flagtouch,
	mt_flagdrop,
	mt_flagcap}
msgtype_t;

typedef struct wclass_s {
	char	*keyword;
	char	*name;
	char	*shortname;
	char	*imagename;
} wclass_t;

typedef struct fragmsg_s {
	char		*msg1;
	char		*msg2;
	msgtype_t	type;
	int			wclass_index;
} fragmsg_t;

typedef struct fragdef_s {
	qbool	active;
	qbool	flagalerts;

	int			version;
	char		*gamedir;

	char		*title;
	char		*description;
	char		*author;
	char		*email;
	char		*webpage;

	fragmsg_t	*fragmsgs[MAX_FRAG_DEFINITIONS];
	fragmsg_t	msgdata[MAX_FRAG_DEFINITIONS];
	int			num_fragmsgs;
} fragdef_t;

static fragdef_t fragdefs;
static wclass_t wclasses[MAX_WEAPON_CLASSES];
static int num_wclasses;

static int fragmsg1_indexes[26];


#define MYISLOWER(c)	(c >= 'a' && c <= 'z')
int Compare_FragMsg (const void *p1, const void *p2) {
	unsigned char a, b;
	char *s, *s1, *s2;

	s1 = (*((fragmsg_t **) p1))->msg1;
	s2 = (*((fragmsg_t **) p2))->msg1;

	for (s = s1; *s && isspace(*s & 127); s++)
		;
	a = tolower(*s & 127);

	for (s = s2; *s && isspace(*s & 127); s++)
		;
	b = tolower(*s & 127);

	if (MYISLOWER(a) && MYISLOWER(b)) {
		return (a != b) ? a - b : -1 * strcasecmp(s1, s2);
	} else {
		return MYISLOWER(a) ? 1 : MYISLOWER(b) ? -1 : a != b ? a - b : -1 * strcasecmp(s1, s2);
	}
}

static void Build_FragMsg_Indices(void) {
	int i, j = -1, c;
	char *s;

	for (i = 0; i < fragdefs.num_fragmsgs; i++) {
		for (s = fragdefs.fragmsgs[i]->msg1; *s && isspace(*s & 127); s++)
			;
		c = tolower(*s & 127);
		if (!MYISLOWER(c))
			continue;

		if (c == 'a' + j)
			continue;
		while (++j < c - 'a')
			fragmsg1_indexes[j] = -1;

		fragmsg1_indexes[j] = i;
	}

	while (++j <= 'z' - 'a')
		fragmsg1_indexes[j] = -1;
}

static void InitFragDefs(qbool restart)
{
	int i;

	Q_free(fragdefs.gamedir);
	Q_free(fragdefs.title);
	Q_free(fragdefs.author);
	Q_free(fragdefs.email);
	Q_free(fragdefs.webpage);
	Q_free(fragdefs.description);

	for (i = 0; i < fragdefs.num_fragmsgs; i++) 
	{
		Q_free(fragdefs.msgdata[i].msg1);
		Q_free(fragdefs.msgdata[i].msg2);
	}

	for (i = 0; i < num_wclasses; i++) 
	{
		Q_free(wclasses[i].keyword);
		Q_free(wclasses[i].name);
		Q_free(wclasses[i].shortname);
		Q_free(wclasses[i].imagename);
	}

	memset(&fragdefs, 0, sizeof(fragdefs));
	memset(wclasses, 0, sizeof(wclasses));

	if (restart) {
		wclasses[0].name = Q_strdup("Unknown");
		num_wclasses = 1;
		VX_TrackerInit();
	}
}

#define _checkargs(x)		if (c != x) {																			\
								Com_Printf("Fragfile warning (line %d): %d tokens expected\n", line, x);			\
								goto nextline;																		\
							}

#define _checkargs2(x, y)	if (c != x && c != y) {																	\
								Com_Printf("Fragfile warning (line %d): %d or %d tokens expected\n", line, x, y);	\
								goto nextline;																		\
							}

#define _checkargs3(x, y)	if (c < x || c > y) {																	\
								Com_Printf("Fragfile warning (line %d): %d to %d tokens expected\n", line, x, y);	\
								goto nextline;																		\
							}


#define	_check_version_defined	if (!gotversion) {																				\
									Com_Printf("Fragfile error: #FRAGFILE VERSION must be defined at the head of the file\n");	\
									goto end;																					\
								}

static void LoadFragFile(char *filename, qbool quiet)
{
	int c, line, i;
	msgtype_t msgtype;
	char save, *buffer = NULL, *start, *end, *token, fragfilename[MAX_OSPATH];
	qbool gotversion = false, warned_flagmsg_overflow = false;

	InitFragDefs(true);

	strlcpy(fragfilename, filename, sizeof(fragfilename));
	COM_ForceExtensionEx(fragfilename, ".dat", sizeof(fragfilename));

	// if it fragfile.dat then try to load from ezquake dir first,
	// because we have a bit different fragfile format comparing to fuhquake
	if (!strcasecmp(fragfilename, "fragfile.dat") && (buffer = (char *) FS_LoadHeapFile("../ezquake/fragfile.dat", NULL)))
		strlcpy(fragfilename, "ezquake/fragfile.dat", sizeof(fragfilename));

	if (!buffer && !(buffer = (char *)FS_LoadHeapFile(fragfilename, NULL))) {
		if (!quiet)
			Com_Printf("Couldn't load fragfile \"%s\"\n", fragfilename);
		return;
	}

	line = 1;
	start = end = buffer;
	while (1) {
		for ( ; *end && *end != '\n'; end++)
			;

		save = *end;
		*end = 0;
		Cmd_TokenizeString(start);
		*end = save;

		if (!(c = Cmd_Argc()))
			goto nextline;

		if (!strcasecmp(Cmd_Argv(0), "#FRAGFILE")) {

			_checkargs(3);
			if (!strcasecmp(Cmd_Argv(1), "VERSION")) {
				if (gotversion) {
					Com_Printf("Fragfile error (line %d): VERSION redefined\n", line);
					goto end;
				}
				token = Cmd_Argv(2);
				if (!strcasecmp(token, FUH_FRAGFILE_VERSION_1_00))
					token = FRAGFILE_VERSION_1_00; // so we compatible with old fuh format, because our format include all fuh features + our

				if (!strcasecmp(token, FRAGFILE_VERSION_1_00) && (token = strchr(token, '-'))) {
					token++; // find float component of string like this "ezquake-x.yz"
					fragdefs.version = (int) (100 * Q_atof(token));
					gotversion = true;
				} else {
					Com_Printf("Error: fragfile version (\"%s\") unsupported \n", Cmd_Argv(2));
					goto end;
				}
			} else if (!strcasecmp(Cmd_Argv(1), "GAMEDIR")) {
				_check_version_defined;
				if (!strcasecmp(Cmd_Argv(2), "ANY"))
					fragdefs.gamedir = NULL;
				else
					fragdefs.gamedir = Q_strdup(Cmd_Argv(2));
			} else {
				_check_version_defined;
				Com_Printf("Fragfile warning (line %d): unexpected token \"%s\"\n", line, Cmd_Argv(1));
				goto nextline;
			}
		} else if (!strcasecmp(Cmd_Argv(0), "#META")) {

			_check_version_defined;
			_checkargs(3);
			if (!strcasecmp(Cmd_Argv(1), "TITLE")) {
				fragdefs.title = Q_strdup(Cmd_Argv(2));
			} else if (!strcasecmp(Cmd_Argv(1), "AUTHOR")) {
				fragdefs.author = Q_strdup(Cmd_Argv(2));
			} else if (!strcasecmp(Cmd_Argv(1), "DESCRIPTION")) {
				fragdefs.description = Q_strdup(Cmd_Argv(2));
			} else if (!strcasecmp(Cmd_Argv(1), "EMAIL")) {
				fragdefs.email = Q_strdup(Cmd_Argv(2));
			} else if (!strcasecmp(Cmd_Argv(1), "WEBPAGE")) {
				fragdefs.webpage = Q_strdup(Cmd_Argv(2));
			} else {
				Com_Printf("Fragfile warning (line %d): unexpected token \"%s\"\n", line, Cmd_Argv(1));
				goto nextline;
			}
		} else if (!strcasecmp(Cmd_Argv(0), "#DEFINE")) {
			_check_version_defined;
			if (!strcasecmp(Cmd_Argv(1), "WEAPON_CLASS") || !strcasecmp(Cmd_Argv(1), "WC")) {

				_checkargs3(4, 6);

				token = Cmd_Argv(2);
				if (!strcasecmp(token, "NOWEAPON") || !strcasecmp(token, "NONE") || !strcasecmp(token, "NULL")) {
					Com_Printf("Fragfile warning (line %d): \"%s\" is a reserved keyword\n", line, token);
					goto nextline;
				}
				for (i = 1; i < num_wclasses; i++) {
					if (!strcasecmp(token, wclasses[i].keyword)) {
						Com_Printf("Fragfile warning (line %d): WEAPON_CLASS \"%s\" already defined\n", line, wclasses[i].keyword);
						goto nextline;
					}
				}
				if (num_wclasses == MAX_WEAPON_CLASSES) {
					Com_Printf("Fragfile warning (line %d): only %d WEAPON_CLASS's may be #DEFINE'd\n", line, MAX_WEAPON_CLASSES);
					goto nextline;
				}
				wclasses[num_wclasses].keyword   = Q_strdup(token);
				wclasses[num_wclasses].name      = Q_strdup(Cmd_Argv(3));
				wclasses[num_wclasses].shortname = Q_strdup(Cmd_Argv(4));
				wclasses[num_wclasses].imagename = Q_strdup(Cmd_Argv(5));
				num_wclasses++;
			} else if (	!strcasecmp(Cmd_Argv(1), "OBITUARY") || !strcasecmp(Cmd_Argv(1), "OBIT")) {

				if (!strcasecmp(Cmd_Argv(2), "PLAYER_DEATH")) {
					_checkargs(5);
					msgtype = mt_death;
				} else if (!strcasecmp(Cmd_Argv(2), "PLAYER_SUICIDE")) {
					_checkargs(5);
					msgtype = mt_suicide;
				} else if (!strcasecmp(Cmd_Argv(2), "X_FRAGS_UNKNOWN")) {
					_checkargs(5);
					msgtype = mt_frag;
				} else if (!strcasecmp(Cmd_Argv(2), "X_TEAMKILLS_UNKNOWN")) {
					_checkargs(5);
					msgtype = mt_tkill;
				} else if (!strcasecmp(Cmd_Argv(2), "X_FRAGS_Y")) {
					_checkargs2(5, 6);
					msgtype = mt_frags;
				} else if (!strcasecmp(Cmd_Argv(2), "X_FRAGGED_BY_Y")) {
					_checkargs2(5, 6);
					msgtype = mt_fragged;
				} else if (!strcasecmp(Cmd_Argv(2), "X_TEAMKILLS_Y")) {
					_checkargs2(5, 6);
					msgtype = mt_tkills;
				} else if (!strcasecmp(Cmd_Argv(2), "X_TEAMKILLED_BY_Y")) {
					_checkargs2(5, 6);
					msgtype = mt_tkilled;
				} else if (!strcasecmp(Cmd_Argv(2), "X_TEAMKILLED_UNKNOWN")) {
					_checkargs(5);
					msgtype = mt_tkilled_unk;
				} else {
					Com_Printf("Fragfile warning (line %d): unexpected token \"%s\"\n", line, Cmd_Argv(2));
					goto nextline;
				}

				if (fragdefs.num_fragmsgs == MAX_FRAG_DEFINITIONS) {
					if (!warned_flagmsg_overflow) {
						Com_Printf("Fragfile warning (line %d): only %d OBITUARY's and FLAG_ALERT's's may be #DEFINE'd\n",
						line, MAX_FRAG_DEFINITIONS);
					}
					warned_flagmsg_overflow = true;
					goto nextline;
				}

				if (strlen(Cmd_Argv(4)) >= MAX_FRAGMSG_LENGTH || (c == 6 && strlen(Cmd_Argv(5)) >= MAX_FRAGMSG_LENGTH)) {
					Com_Printf("Fragfile warning (line %d): Message token cannot exceed %d characters\n", line, MAX_FRAGMSG_LENGTH - 1);
					goto nextline;
				}

				token = Cmd_Argv(3);
				if (!strcasecmp(token, "NOWEAPON") || !strcasecmp(token, "NONE") || !strcasecmp(token, "NULL")) {
					fragdefs.msgdata[fragdefs.num_fragmsgs].wclass_index = 0;
				} else {
					for (i = 1; i < num_wclasses; i++) {
						if (!strcasecmp(token, wclasses[i].keyword)) {
							fragdefs.msgdata[fragdefs.num_fragmsgs].wclass_index = i;
							break;
						}
					}
					if (i == num_wclasses) {
						Com_Printf("Fragfile warning (line %d): \"%s\" is not a defined WEAPON_CLASS\n", line, token);
						goto nextline;
					}
				}

				fragdefs.msgdata[fragdefs.num_fragmsgs].type = msgtype;
				fragdefs.msgdata[fragdefs.num_fragmsgs].msg1 = Q_strdup (Cmd_Argv(4));
				fragdefs.msgdata[fragdefs.num_fragmsgs].msg2 = (c == 6) ? Q_strdup(Cmd_Argv(5)) : NULL;
				fragdefs.num_fragmsgs++;
			} else if (!strcasecmp(Cmd_Argv(1), "FLAG_ALERT") || !strcasecmp(Cmd_Argv(1), "FLAG_MSG")) {

				_checkargs(4);
				if
				(
					!strcasecmp(Cmd_Argv(2), "X_TOUCHES_FLAG") ||
					!strcasecmp(Cmd_Argv(2), "X_GETS_FLAG") ||
					!strcasecmp(Cmd_Argv(2), "X_TAKES_FLAG")
				) {
					msgtype = mt_flagtouch;
				} else if
				(
					!strcasecmp(Cmd_Argv(2), "X_DROPS_FLAG") ||
					!strcasecmp(Cmd_Argv(2), "X_FUMBLES_FLAG") ||
					!strcasecmp(Cmd_Argv(2), "X_LOSES_FLAG")
				) {
					msgtype = mt_flagdrop;
				} else if
				(
					!strcasecmp(Cmd_Argv(2), "X_CAPTURES_FLAG") ||
					!strcasecmp(Cmd_Argv(2), "X_CAPS_FLAG") ||
					!strcasecmp(Cmd_Argv(2), "X_SCORES")
				) {
					msgtype = mt_flagcap;
				} else {
					Com_Printf("Fragfile warning (line %d): unexpected token \"%s\"\n", line, Cmd_Argv(2));
					goto nextline;
				}

				if (fragdefs.num_fragmsgs == MAX_FRAG_DEFINITIONS) {
					if (!warned_flagmsg_overflow) {
						Com_Printf("Fragfile warning (line %d): only %d OBITUARY's and FLAG_ALERT's's may be #DEFINE'd\n",
							line, MAX_FRAG_DEFINITIONS);
					}
					warned_flagmsg_overflow = true;
					goto nextline;
				}
				fragdefs.msgdata[fragdefs.num_fragmsgs].type = msgtype;
				fragdefs.msgdata[fragdefs.num_fragmsgs].msg1 = Q_strdup(Cmd_Argv(3));
				fragdefs.msgdata[fragdefs.num_fragmsgs].msg2 = NULL;
				fragdefs.num_fragmsgs++;

				fragdefs.flagalerts = true;
			}
		} else {
			_check_version_defined;
			Com_Printf("Fragfile warning (line %d): unexpected token \"%s\"\n", line, Cmd_Argv(0));
			goto nextline;
		}

nextline:
		if (!*end)
			break;

		start = end = end + 1;
		line++;
	}

	if (fragdefs.num_fragmsgs) {

		for (i = 0; i < fragdefs.num_fragmsgs; i++)
			fragdefs.fragmsgs[i] = &fragdefs.msgdata[i];
		qsort(fragdefs.fragmsgs, fragdefs.num_fragmsgs, sizeof(fragmsg_t *), Compare_FragMsg);
		Build_FragMsg_Indices();

		fragdefs.active = true;
		if (!quiet)
			Com_Printf("Loaded fragfile \"%s\" (%d frag defs)\n", fragfilename, fragdefs.num_fragmsgs);
		goto end;
	} else {
		if (!quiet)
			Com_Printf("Fragfile \"%s\" was empty\n", fragfilename);
		goto end;
	}

end:
	Q_free(buffer);
	VX_TrackerInit();
}

void Load_FragFile_f(void) {
	switch (Cmd_Argc()) {
		case 2:
			LoadFragFile(Cmd_Argv(1), false);
			break;
		default:
			Com_Printf("Usage: %s <filename>\n", Cmd_Argv(0));
			break;
	}
}


typedef struct fragstats_s {
	int deaths[MAX_CLIENTS];
	int teamdeaths[MAX_CLIENTS];
	int kills[MAX_CLIENTS];
	int teamkills[MAX_CLIENTS];
	int wdeaths[MAX_WEAPON_CLASSES];
	int wkills[MAX_WEAPON_CLASSES];
	//VULT DISPLAY NAMES
	int wsuicides[MAX_WEAPON_CLASSES];
	int streak;

	int totaldeaths;
	int totalfrags;
	int totalteamkills;
	int totalsuicides;

	int touches;
	int fumbles;
	int captures;
} fragstats_t;

static fragstats_t fragstats[MAX_CLIENTS];
static qbool flag_dropped, flag_touched, flag_captured;

static void Stats_ParsePrintLine(const char *s, cfrags_format *cff, int offset) 
{
	int start_search, end_Search, i, j, k, p1len, msg1len, msg2len, p2len, killer, victim;
	fragmsg_t *fragmsg;
	const char *start, *name1, *name2, *t;
	player_info_t *player1 = NULL, *player2 = NULL;

	for (i = 0; i < MAX_CLIENTS; i++) 
	{
		start = s;
		player1 = &cl.players[i];
	
		if (!player1->name[0] || player1->spectator)
			continue;

		name1 = Info_ValueForKey(player1->userinfo, "name");
		p1len = min(strlen(name1), 31);
		
		if (!strncmp(start, name1, p1len)) 
		{
			cff->p1pos = offset;
			cff->p1len = p1len; 
			cff->p1col = player1->topcolor;
			
			for (t = start + p1len; *t && isspace(*t & 127); t++)
				;

			k = tolower(*t & 127);
			
			if (MYISLOWER(k))
			{
				start_search = fragmsg1_indexes[k - 'a'];
				end_Search = (k == 'z') ? fragdefs.num_fragmsgs : fragmsg1_indexes[k - 'a' + 1];
			}
			else 
			{
				start_search = 0;
				end_Search = fragmsg1_indexes[0];
			}

			if (start_search == -1)
				continue;

			if (end_Search == -1)
				end_Search = fragdefs.num_fragmsgs;

			for (j = start_search; j < end_Search; j++) 
			{
				start = s + p1len;
				fragmsg = fragdefs.fragmsgs[j];
				msg1len = strlen(fragmsg->msg1);
				if (!strncmp(start, fragmsg->msg1, msg1len)) 
				{
					if (fragmsg->type == mt_fragged || fragmsg->type == mt_frags ||
						fragmsg->type == mt_tkills || fragmsg->type == mt_tkilled) 
					{
						for (k = 0; k < MAX_CLIENTS; k++) 
						{
							start = s + p1len + msg1len;
							player2 = &cl.players[k];
							
							if (!player2->name[0] || player2->spectator)
								continue;
							
							name2 = Info_ValueForKey(player2->userinfo, "name");
							p2len = min(strlen(name2), 31);
						
							if (!strncmp(start, name2, p2len)) 
							{
								cff->p2pos = start - s + offset;
								cff->p2len = p2len;
								cff->p2col = player2->topcolor;
								
								if (fragmsg->msg2) 
								{
									if (!*(start = s + p1len + msg1len + p2len))
										continue;

									msg2len = strlen(fragmsg->msg2);
									
									if (!strncmp(start, fragmsg->msg2, msg2len))
										goto foundmatch;
								}
								else 
								{
									goto foundmatch;
								}
							}
						}
					}
					else 
					{
						goto foundmatch;
					}
				}
			}
		}
	}

	return;
	
foundmatch:
	i = player1 - cl.players;
	if (player2)
		j = player2 - cl.players;

	switch (fragmsg->type) 
	{
		case mt_death:
		{
			cff->isFragMsg = true;
			fragstats[i].totaldeaths++;
			fragstats[i].wdeaths[fragmsg->wclass_index]++;
			VX_TrackerDeath(i, fragmsg->wclass_index, fragstats[i].wdeaths[fragmsg->wclass_index]);
			VX_TrackerStreakEnd(i, i, fragstats[i].streak);
			fragstats[i].streak=0;
			break;
		}
		case mt_suicide:
		{
			cff->isFragMsg = true;
			fragstats[i].totalsuicides++;
			fragstats[i].totaldeaths++;
			fragstats[i].wsuicides[fragmsg->wclass_index]++;
			VX_TrackerSuicide(i, fragmsg->wclass_index, fragstats[i].wsuicides[fragmsg->wclass_index]);
			VX_TrackerStreakEnd(i, i, fragstats[i].streak);
			fragstats[i].streak=0;
			break;
		}
		case mt_fragged:
		case mt_frags:
		{
			cff->isFragMsg = true;
			killer = (fragmsg->type == mt_fragged) ? j : i;
			victim = (fragmsg->type == mt_fragged) ? i : j;
			CL_Cam_SetKiller(killer,victim);
			fragstats[killer].kills[victim]++;
			fragstats[killer].totalfrags++;
			fragstats[killer].wkills[fragmsg->wclass_index]++;
			fragstats[victim].deaths[killer]++;
			fragstats[victim].totaldeaths++;
			fragstats[victim].wdeaths[fragmsg->wclass_index]++;
			VX_TrackerFragXvsY(victim, killer, fragmsg->wclass_index,
					fragstats[victim].wdeaths[fragmsg->wclass_index], fragstats[killer].wkills[fragmsg->wclass_index]);
			fragstats[killer].streak++;
			VX_TrackerStreak(killer, fragstats[killer].streak);
			VX_TrackerStreakEnd(victim, killer, fragstats[victim].streak);
			fragstats[victim].streak=0;
			break;
		}
		case mt_frag:
		{
			cff->isFragMsg = true;
			fragstats[i].totalfrags++;
			fragstats[i].wkills[fragmsg->wclass_index]++;
			VX_TrackerOddFrag(i, fragmsg->wclass_index, fragstats[i].wkills[fragmsg->wclass_index]);
			fragstats[i].streak++;
			VX_TrackerStreak(i, fragstats[i].streak);
			break;
		}
		case mt_tkilled:
		case mt_tkills:
		{	
			cff->isFragMsg = true;
			killer = (fragmsg->type == mt_tkilled) ? j : i;
			victim = (fragmsg->type == mt_tkilled) ? i : j;
			CL_Cam_SetKiller(killer,victim);

			fragstats[killer].teamkills[victim]++;
			fragstats[killer].totalteamkills++;

			fragstats[victim].teamdeaths[killer]++;
			fragstats[victim].totaldeaths++;

			VX_TrackerTK_XvsY(victim, killer, fragmsg->wclass_index,
						fragstats[killer].totalteamkills, fragstats[victim].teamdeaths[killer],
						fragstats[killer].totalteamkills, fragstats[killer].teamkills[victim]);
			VX_TrackerStreakEnd(victim, killer, fragstats[victim].streak);
			fragstats[victim].streak=0;
			break;
		}
		case mt_tkilled_unk:
		{
			cff->isFragMsg = true;
			fragstats[i].totaldeaths++;
			VX_TrackerOddTeamkilled(i, fragmsg->wclass_index);
			VX_TrackerStreakEndOddTeamkilled(i, fragstats[i].streak);
			fragstats[i].streak=0;
			break;
		}
		case mt_tkill:
		{
			cff->isFragMsg = true;
			fragstats[i].totalteamkills++;
			VX_TrackerOddTeamkill(i, fragmsg->wclass_index, fragstats[i].totalteamkills);
			// not enough data to stop streaks of killed man
			break;
		}
		case mt_flagtouch:
		{
			fragstats[i].touches++;
			Update_FlagStatus(i, player1->team, true);
			if (cl.playernum == i || (i == Cam_TrackNum() && cl.spectator))
				VX_TrackerFlagTouch(fragstats[i].touches);
			flag_touched = true;
			break;
		}
		case mt_flagdrop:
		{		
			fragstats[i].fumbles++;
			Update_FlagStatus(i, player1->team, false);
			if (cl.playernum == i || (i == Cam_TrackNum() && cl.spectator))
				VX_TrackerFlagDrop(fragstats[i].fumbles);
			flag_dropped = true;
			break;
		}
		case mt_flagcap:
		{		
			fragstats[i].captures++;
			Update_FlagStatus(i, player1->team, false);
			if (cl.playernum == i || (i == Cam_TrackNum() && cl.spectator))
				VX_TrackerFlagCapture(fragstats[i].captures);
			flag_captured = true;
			break;
		}
		default:
			break;
	}
}

void Stats_ParsePrint(char *s, int level, cfrags_format *cff) {
	char *start, *end, save;
	int offset = 0;

	if (!Stats_IsActive())
		return;

	if (level != PRINT_MEDIUM && level != PRINT_HIGH)
		return;

	start = end = s;
	while (1) {
		for ( ; *end && *end != '\n'; end++)
			;

		if (*end) {
			end++;
			save = *end;
			*end = 0;
			Stats_ParsePrintLine(start, cff, offset);
			*end = save;
			offset += (end - s);
		} else {
			Stats_ParsePrintLine(start, cff, offset);
			break;
		}

		start = end;
	}
}

void Stats_Reset(void) {
	memset(&fragstats, 0, sizeof(fragstats));
	flag_touched = flag_dropped = flag_captured = false;
}

void Stats_NewMap(void) {
	static char last_gamedir[MAX_OSPATH] = {0};

	if (!last_gamedir[0] || strcasecmp(last_gamedir, cls.gamedirfile)) {
		if (cl_loadFragfiles.value) {
			strlcpy(last_gamedir, cls.gamedirfile, sizeof(last_gamedir));
			LoadFragFile("fragfile", true);
		} else {
			InitFragDefs(true);
		}
	}

	Stats_Reset();
}

void Stats_EnterSlot(int num) {
	int i;

	if (num < 0 || num >= MAX_CLIENTS)
		Sys_Error("Stats_EnterSlot: num < 0 || num >= MAX_CLIENTS");

	memset(&fragstats[num], 0, sizeof(fragstats[num]));

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!cl.players[i].name[0] || cl.players[i].spectator)
			continue;
		if (i == num)
			continue;

		fragstats[i].teamdeaths[num] = fragstats[i].deaths[num] = 0;
		fragstats[i].teamkills[num] = fragstats[i].kills[num] = 0;
	}
}


void Stats_GetBasicStats(int num, int *playerstats) {
	if (num < 0 || num >= MAX_CLIENTS)
		Sys_Error("Stats_EnterSlot: num < 0 || num >= MAX_CLIENTS");

	playerstats[0] = fragstats[num].totalfrags;
	playerstats[1] = fragstats[num].totaldeaths;
	playerstats[2] = fragstats[num].totalteamkills;
	playerstats[3] = fragstats[num].totalsuicides;
}

void Stats_GetFlagStats(int num, int *playerstats) {
	if (num < 0 || num >= MAX_CLIENTS)
		Sys_Error("Stats_EnterSlot: num < 0 || num >= MAX_CLIENTS");

	if (!flag_touched && flag_dropped && flag_captured)
		playerstats[0] = fragstats[num].fumbles + fragstats[num].captures;
	else
		playerstats[0] = fragstats[num].touches;

	playerstats[1] = fragstats[num].fumbles;
	playerstats[2] = fragstats[num].captures;
}

qbool Stats_IsActive(void) {
	return (fragdefs.active && cl_parsefrags.value) ? true : false;
}

qbool Stats_IsFlagsParsed(void) {
	return (Stats_IsActive() && fragdefs.flagalerts) ? true : false;
}

void Stats_Init(void) {
	InitFragDefs(true);
	Cvar_SetCurrentGroup(CVAR_GROUP_SBAR);
	Cvar_Register(&cl_parsefrags);
	Cvar_Register(&cl_showFragsMessages);
	Cvar_Register(&cl_loadFragfiles);
	Cvar_Register(&cl_useimagesinfraglog);
	Cvar_ResetCurrentGroup();
	Cmd_AddCommand("loadFragfile", Load_FragFile_f);
}

//VULT DISPLAYNAMES
char *GetWeaponName (int num)
{
	if (cl_useimagesinfraglog.integer && wclasses[num].imagename && wclasses[num].imagename[0])
		return wclasses[num].imagename;

	if (wclasses[num].shortname && wclasses[num].shortname[0])
		return wclasses[num].shortname;

	if (wclasses[num].name && wclasses[num].name[0])
		return wclasses[num].name;

	return "Unknown";
}

const char* GetWeaponImageName(int num)
{
	if (wclasses[num].imagename && wclasses[num].imagename[0]) {
		return wclasses[num].imagename;
	}

	return NULL;
}

const char* GetWeaponTextName(int num)
{
	if (wclasses[num].shortname && wclasses[num].shortname[0]) {
		return wclasses[num].shortname;
	}

	if (wclasses[num].name && wclasses[num].name[0]) {
		return wclasses[num].name;
	}

	return NULL;
}

void Stats_Shutdown(void)
{
	InitFragDefs(false);
}
