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
*/

#include "quakedef.h"

cvar_t cl_parsefrags = {"cl_parseFrags", "0"};
cvar_t cl_loadFragfiles = {"cl_loadFragfiles", "0"};

#define FRAGFILE_VERSION	"1.00"

#define MAX_WEAPON_CLASSES		64
#define MAX_FRAG_DEFINITIONS	512
#define MAX_FRAGMSG_LENGTH		256	

typedef enum msgtype_s {
	mt_fragged,	
	mt_frags,	
	mt_tkills,	
	mt_tkilled,	

	mt_death,	
	mt_suicide,	
	mt_frag,	
	mt_tkill,	
	mt_flagtouch,
	mt_flagdrop,
	mt_flagcap} 
msgtype_t;

typedef struct wclass_s {
	char	*keyword;
	char	*name;
	char	*shortname;
} wclass_t;

typedef struct fragmsg_s {
	char		*msg1;
	char		*msg2;
	msgtype_t	type;
	int			wclass_index;
} fragmsg_t;

typedef struct fragdef_s {
	qboolean	active;		
	qboolean	flagalerts;	

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
		return (a != b) ? a - b : -1 * Q_strcasecmp(s1, s2);
	} else {	
		return MYISLOWER(a) ? 1 : MYISLOWER(b) ? -1 : a != b ? a - b : -1 * Q_strcasecmp(s1, s2);
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

static void InitFragDefs(void) {
	int i;

	#define CHECK_AND_FREE(x)	{if (x) Z_Free(x);}

	CHECK_AND_FREE(fragdefs.gamedir);
	CHECK_AND_FREE(fragdefs.title);
	CHECK_AND_FREE(fragdefs.author);
	CHECK_AND_FREE(fragdefs.email);
	CHECK_AND_FREE(fragdefs.webpage);

	for (i = 0; i < fragdefs.num_fragmsgs; i++) {
		CHECK_AND_FREE(fragdefs.msgdata[i].msg1);
		CHECK_AND_FREE(fragdefs.msgdata[i].msg2);
	}

	for (i = 0; i < num_wclasses; i++) {
		CHECK_AND_FREE(wclasses[i].keyword);
		CHECK_AND_FREE(wclasses[i].name);
		CHECK_AND_FREE(wclasses[i].shortname);
	}

	memset(&fragdefs, 0, sizeof(fragdefs));
	memset(wclasses, 0, sizeof(wclasses));

	wclasses[0].name = CopyString("Unknown");
	num_wclasses = 1;	

	#undef CHECK_AND_FREE
}

#define _checkargs(x)		if (c != x) {																			\
								Com_Printf("Fragfile warning (line %d): %d tokens expected\n", line, x);			\
								goto nextline;																		\
							}

#define _checkargs2(x, y)	if (c != x && c != y) {																	\
								Com_Printf("Fragfile warning (line %d): %d or %d tokens expected\n", line, x, y);	\
								goto nextline;																		\
							}

#define	_check_version_defined	if (!gotversion) {																				\
									Com_Printf("Fragfile error: #FRAGFILE VERSION must be defined at the head of the file\n");	\
									goto end;																					\
								}

static void LoadFragFile(char *filename, qboolean quiet) {
	int lowmark, c, line, i;
	msgtype_t msgtype;
	char save, *buffer, *start, *end, *token, fragfilename[MAX_OSPATH];
	qboolean gotversion = false, warned_flagmsg_overflow = false;

	InitFragDefs();

	lowmark = Hunk_LowMark();
	Q_strncpyz(fragfilename, filename, sizeof(fragfilename));
	COM_ForceExtension(fragfilename, ".dat");
	if (!(buffer = FS_LoadHunkFile(fragfilename))) {
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

		if (!Q_strcasecmp(Cmd_Argv(0), "#FRAGFILE")) {
		
			_checkargs(3);
			if (!Q_strcasecmp(Cmd_Argv(1), "VERSION")) {
				if (gotversion) {
					Com_Printf("Fragfile error (line %d): VERSION redefined\n", line);
					goto end;
				}
				token = Cmd_Argv(2);
				if (!Q_strcasecmp(token, FRAGFILE_VERSION)) {
					fragdefs.version = (int) (100 * Q_atof(token));
					gotversion = true;
				} else {
					Com_Printf("Error: fragfile version (\"%s\") unsupported \n", token);
					goto end;
				}
			} else if (!Q_strcasecmp(Cmd_Argv(1), "GAMEDIR")) {
				_check_version_defined;
				if (!Q_strcasecmp(Cmd_Argv(2), "ANY"))
					fragdefs.gamedir = NULL;
				else
					fragdefs.gamedir = CopyString(Cmd_Argv(2));
			} else {
				_check_version_defined;
				Com_Printf("Fragfile warning (line %d): unexpected token \"%s\"\n", line, Cmd_Argv(1));
				goto nextline;	
			}
		} else if (!Q_strcasecmp(Cmd_Argv(0), "#META")) {
		
			_check_version_defined;
			_checkargs(3);
			if (!Q_strcasecmp(Cmd_Argv(1), "TITLE")) {
				fragdefs.title = CopyString(Cmd_Argv(2));
			} else if (!Q_strcasecmp(Cmd_Argv(1), "AUTHOR")) {
				fragdefs.author = CopyString(Cmd_Argv(2));
			} else if (!Q_strcasecmp(Cmd_Argv(1), "DESCRIPTION")) {
				fragdefs.description = CopyString(Cmd_Argv(2));
			} else if (!Q_strcasecmp(Cmd_Argv(1), "EMAIL")) {
				fragdefs.email = CopyString(Cmd_Argv(2));
			} else if (!Q_strcasecmp(Cmd_Argv(1), "WEBPAGE")) {
				fragdefs.webpage = CopyString(Cmd_Argv(2));
			} else {
				Com_Printf("Fragfile warning (line %d): unexpected token \"%s\"\n", line, Cmd_Argv(1));
				goto nextline;
			}
		} else if (!Q_strcasecmp(Cmd_Argv(0), "#DEFINE")) {
			_check_version_defined;
			if (!Q_strcasecmp(Cmd_Argv(1), "WEAPON_CLASS") || !Q_strcasecmp(Cmd_Argv(1), "WC")) {
			
				_checkargs2(4, 5);
				token = Cmd_Argv(2);
				if (!Q_strcasecmp(token, "NOWEAPON") || !Q_strcasecmp(token, "NONE") || !Q_strcasecmp(token, "NULL")) {
					Com_Printf("Fragfile warning (line %d): \"%s\" is a reserved keyword\n", line, token);
					goto nextline;
				}
				for (i = 1; i < num_wclasses; i++) {
					if (!Q_strcasecmp(token, wclasses[i].keyword)) {
						Com_Printf("Fragfile warning (line %d): WEAPON_CLASS \"%s\" already defined\n", line, wclasses[i].keyword);
						goto nextline;
					}
				}
				if (num_wclasses == MAX_WEAPON_CLASSES) {
					Com_Printf("Fragfile warning (line %d): only %d WEAPON_CLASS's may be #DEFINE'd\n", line, MAX_WEAPON_CLASSES);
					goto nextline;
				}
				wclasses[num_wclasses].keyword = CopyString(token);
				wclasses[num_wclasses].name = CopyString(Cmd_Argv(3));
				if (c == 5)
					wclasses[num_wclasses].name = CopyString(Cmd_Argv(4));
				num_wclasses++;
			} else if (	!Q_strcasecmp(Cmd_Argv(1), "OBITUARY") || !Q_strcasecmp(Cmd_Argv(1), "OBIT")) {
			
				if (!Q_strcasecmp(Cmd_Argv(2), "PLAYER_DEATH")) {
					_checkargs(5);
					msgtype = mt_death;
				} else if (!Q_strcasecmp(Cmd_Argv(2), "PLAYER_SUICIDE")) {
					_checkargs(5);
					msgtype = mt_suicide;
				} else if (!Q_strcasecmp(Cmd_Argv(2), "X_FRAGS_UNKNOWN")) {
					_checkargs(5);
					msgtype = mt_frag;
				} else if (!Q_strcasecmp(Cmd_Argv(2), "X_TEAMKILLS_UNKNOWN")) {
					_checkargs(5);
					msgtype = mt_tkill;
				} else if (!Q_strcasecmp(Cmd_Argv(2), "X_FRAGS_Y")) {
					_checkargs2(5, 6);
					msgtype = mt_frags;
				} else if (!Q_strcasecmp(Cmd_Argv(2), "X_FRAGGED_BY_Y")) {
					_checkargs2(5, 6);
					msgtype = mt_fragged;
				} else if (!Q_strcasecmp(Cmd_Argv(2), "X_TEAMKILLS_Y")) {
					_checkargs2(5, 6);
					msgtype = mt_tkills;
				} else if (!Q_strcasecmp(Cmd_Argv(2), "X_TEAMKILLED_BY_Y")) {
					_checkargs2(5, 6);
					msgtype = mt_tkilled;
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
				if (!Q_strcasecmp(token, "NOWEAPON") || !Q_strcasecmp(token, "NONE") || !Q_strcasecmp(token, "NULL")) {
					fragdefs.msgdata[fragdefs.num_fragmsgs].wclass_index = 0;
				} else {
					for (i = 1; i < num_wclasses; i++) {
						if (!Q_strcasecmp(token, wclasses[i].keyword)) {
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
				fragdefs.msgdata[fragdefs.num_fragmsgs].msg1 = CopyString(Cmd_Argv(4));
				fragdefs.msgdata[fragdefs.num_fragmsgs].msg2 = (c == 6) ? CopyString(Cmd_Argv(5)) : NULL;
				fragdefs.num_fragmsgs++;
			} else if (!Q_strcasecmp(Cmd_Argv(1), "FLAG_ALERT") || !Q_strcasecmp(Cmd_Argv(1), "FLAG_MSG")) {
			
				_checkargs(4);
				if
				(
					!Q_strcasecmp(Cmd_Argv(2), "X_TOUCHES_FLAG") ||
					!Q_strcasecmp(Cmd_Argv(2), "X_GETS_FLAG") ||
					!Q_strcasecmp(Cmd_Argv(2), "X_TAKES_FLAG")
				) {
					msgtype = mt_flagtouch;
				} else if
				(
					!Q_strcasecmp(Cmd_Argv(2), "X_DROPS_FLAG") ||
					!Q_strcasecmp(Cmd_Argv(2), "X_FUMBLES_FLAG") ||
					!Q_strcasecmp(Cmd_Argv(2), "X_LOSES_FLAG")
				) {
					msgtype = mt_flagdrop;
				} else if
				(
					!Q_strcasecmp(Cmd_Argv(2), "X_CAPTURES_FLAG") ||
					!Q_strcasecmp(Cmd_Argv(2), "X_CAPS_FLAG") ||
					!Q_strcasecmp(Cmd_Argv(2), "X_SCORES")
				) {
					msgtype = mt_flagcap;
				} else {
					Com_Printf("Fragfile warning (line/ %d): unexpected token \"%s\"\n", line, Cmd_Argv(2));
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
				fragdefs.msgdata[fragdefs.num_fragmsgs].msg1 = CopyString(Cmd_Argv(3));
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
	Hunk_FreeToLowMark(lowmark);
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

	int totaldeaths;
	int totalfrags;
	int totalteamkills;
	int totalsuicides;

	int touches;
	int fumbles;
	int captures;
} fragstats_t;

static fragstats_t fragstats[MAX_CLIENTS];
static qboolean flag_dropped, flag_touched, flag_captured;

static void Stats_ParsePrintLine(char *s) {
	int start_search, end_Search, i, j, k, p1len, msg1len, msg2len, p2len, killer, victim;
	fragmsg_t *fragmsg;
	char *start, *name1, *name2, *t;
	player_info_t *player1 = NULL, *player2 = NULL;

	for (i = 0; i < MAX_CLIENTS; i++) {
		start = s;
		player1 = &cl.players[i];
		if (!player1->name[0] || player1->spectator)
			continue;
		name1 = Info_ValueForKey(player1->userinfo, "name");
		p1len = min(strlen(name1), 31);
		if (!strncmp(start, name1, p1len)) {
		
			for (t = start + p1len; *t && isspace(*t & 127); t++)
				;
			k = tolower(*t & 127);
			if (MYISLOWER(k)) {
				start_search = fragmsg1_indexes[k - 'a'];
				end_Search = (k == 'z') ? fragdefs.num_fragmsgs : fragmsg1_indexes[k - 'a' + 1];
			} else {
				start_search = 0;
				end_Search = fragmsg1_indexes[0];
			}
			if (start_search == -1)
				continue;
			if (end_Search == -1)
				end_Search = fragdefs.num_fragmsgs;
		

			for (j = start_search; j < end_Search; j++) {
				start = s + p1len;
				fragmsg = fragdefs.fragmsgs[j];
				msg1len = strlen(fragmsg->msg1);
				if (!strncmp(start, fragmsg->msg1, msg1len)) {
				
					if (
							fragmsg->type == mt_fragged || fragmsg->type == mt_frags || 
							fragmsg->type == mt_tkills || fragmsg->type == mt_tkilled
					) {
					
						for (k = 0; k < MAX_CLIENTS; k++) {
							start = s + p1len + msg1len;
							player2 = &cl.players[k];
							if (!player2->name[0] || player2->spectator)
								continue;
							name2 = Info_ValueForKey(player2->userinfo, "name");
							p2len = min(strlen(name2), 31);
							if (!strncmp(start, name2, p2len)) {
							
								if (fragmsg->msg2) {
								
									if (!*(start = s + p1len + msg1len + p2len))
										continue;

									msg2len = strlen(fragmsg->msg2);
									if (!strncmp(start, fragmsg->msg2, msg2len))
										goto foundmatch;
								} else {
									goto foundmatch;
								}							
							}
						}
					} else {
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

	switch (fragmsg->type) {
	case mt_death:
		fragstats[i].totaldeaths++;
		fragstats[i].wdeaths[fragmsg->wclass_index]++;
		break;

	case mt_suicide:
		fragstats[i].totalsuicides++;
		fragstats[i].totaldeaths++;
		break;

	case mt_fragged:
	case mt_frags:	
		killer = (fragmsg->type == mt_fragged) ? j : i;
		victim = (fragmsg->type == mt_fragged) ? i : j;

		fragstats[killer].kills[victim]++;
		fragstats[killer].totalfrags++;
		fragstats[killer].wkills[fragmsg->wclass_index]++;

		fragstats[victim].deaths[killer]++;
		fragstats[victim].totaldeaths++;
		fragstats[victim].wdeaths[fragmsg->wclass_index]++;
		break;

	case mt_frag:
		fragstats[i].totalfrags++;
		fragstats[i].wkills[fragmsg->wclass_index]++;
		break;

	case mt_tkilled:
	case mt_tkills:	
		killer = (fragmsg->type == mt_tkilled) ? j : i;
		victim = (fragmsg->type == mt_tkilled) ? i : j;

		fragstats[killer].teamkills[victim]++;
		fragstats[killer].totalteamkills++;

		fragstats[victim].teamdeaths[killer]++;
		fragstats[victim].totaldeaths++;	
		break;

	case mt_tkill:	
		fragstats[i].totalteamkills++;
		break;

	case mt_flagtouch:
		fragstats[i].touches++;
		flag_touched = true;
		break;

	case mt_flagdrop:
		fragstats[i].fumbles++;
		flag_dropped = true;
		break;

	case mt_flagcap:
		fragstats[i].captures++;
		flag_captured = true;
		break;

	default:
		break;
	}
}

void Stats_ParsePrint(char *s, int level) {
	char *start, *end, save;

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
			Stats_ParsePrintLine(start);
			*end = save;
		} else {
			Stats_ParsePrintLine(start);
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


	if (!last_gamedir[0] || Q_strcasecmp(last_gamedir, cls.gamedirfile)) {
		if (cl_loadFragfiles.value) {
			Q_strncpyz(last_gamedir, cls.gamedirfile, sizeof(last_gamedir));
			LoadFragFile("fragfile", true);
		} else {
			InitFragDefs();
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

qboolean Stats_IsActive(void) {
	return (fragdefs.active && cl_parsefrags.value) ? true : false;
}

qboolean Stats_IsFlagsParsed(void) {
	return (Stats_IsActive() && fragdefs.flagalerts) ? true : false;
}

void Stats_Init(void) {
	InitFragDefs();

	Cvar_Register(&cl_parsefrags);
	Cvar_Register(&cl_loadFragfiles);
	Cmd_AddCommand("loadFragfile", Load_FragFile_f);
}
