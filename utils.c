/*

Copyright (C) 2001-2002       A Nourai

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
#include "utils.h"

int TP_CategorizeMessage (char *s, int *offset);
char *COM_FileExtension (char *in);

/************************************** General Utils **************************************/

char *CreateSpaces(int amount) {
	static char spaces[1024];
	int size;

	size = bound(1, amount, sizeof(spaces) - 1);
	memset(spaces, ' ', size);
	spaces[size] = 0;

	return spaces;
}

char *SecondsToMinutesString(int print_time) {
	static char time[128];
	int tens_minutes, minutes, tens_seconds, seconds;

	tens_minutes = fmod (print_time / 600, 6);
	minutes = fmod (print_time / 60, 10);
	tens_seconds = fmod (print_time / 10, 6);
	seconds = fmod (print_time, 10);
	Q_snprintfz (time, sizeof(time), "%i%i:%i%i", tens_minutes, minutes, tens_seconds, seconds);
	return time;
}

char *SecondsToHourString(int print_time) {
	static char time[128];
	int tens_hours, hours,tens_minutes, minutes, tens_seconds, seconds;

	tens_hours = fmod (print_time / 36000, 10);
	hours = fmod (print_time / 3600, 10);
	tens_minutes = fmod (print_time / 600, 6);
	minutes = fmod (print_time / 60, 10);
	tens_seconds = fmod (print_time / 10, 6);
	seconds = fmod (print_time, 10);
	Q_snprintfz (time, sizeof(time), "%i%i:%i%i:%i%i", tens_hours, hours, tens_minutes, minutes, tens_seconds, seconds);
	return time;
}

#ifdef GLQUAKE
byte *StringToRGB(char *s) {
	byte *col;
	static byte rgb[4];

	Cmd_TokenizeString(s);
	if (Cmd_Argc() == 3) {
		rgb[0] = (byte) Q_atoi(Cmd_Argv(0));
		rgb[1] = (byte) Q_atoi(Cmd_Argv(1));
		rgb[2] = (byte) Q_atoi(Cmd_Argv(2));
	} else {
		col = (byte *) &d_8to24table[(byte) Q_atoi(s)];
		rgb[0] = col[0];
		rgb[1] = col[1];
		rgb[2] = col[2];
	}
	rgb[3] = 255;
	return rgb;
}
#endif

/************************************** File Utils **************************************/

int Util_Extend_Filename(char *filename, char **ext) {
	char extendedname[1024], **s;
	int i, offset;
	FILE *f;

	Q_strncpyz(extendedname, filename, sizeof(extendedname));
	offset = strlen(extendedname);

	i = -1;
	while(1) {
		if (++i == 1000)
			break;
		for (s = ext; *s; s++) { 
			Q_snprintfz (extendedname + offset, sizeof(extendedname) - offset, "_%03i.%s", i, *s);
			if (f = fopen(extendedname, "rb")) {
				fclose(f);
				break;
			}
		}
		if (!*s)
			return i;
	}
	return -1;
}

void Util_Process_Filename(char *string) {
	int i;

	if (!string)
		return;

	for (i = 0; i < strlen(string); i++)
		if (string[i] == '\\')
			string[i] = '/';
	if (string[0] == '/')
		for (i = 1; i <= strlen(string); i++)
			string[i - 1] = string[i];
}

qboolean Util_Is_Valid_Filename(char *s) {
	static char badchars[] = {'?', '*', ':', '<', '>', '"'};

	if (!s || !*s)
		return false;

	if (strstr(s, "../") || strstr(s, "..\\") )
		return false;

	while (*s) {
		if (*s < 32 || *s >= 127 || strchr(badchars, *s))
			return false;
		s++;
	}
	return true;
}

char *Util_Invalid_Filename_Msg(char *s) {
	static char err[192];

	if (!s)
		return NULL;

	Q_snprintfz(err, sizeof(err), "%s is not a valid filename (?*:<>\" are illegal characters)\n", s);
	return err;
}

/************************************* Player Utils *************************************/

static int Player_Compare (const void *p1, const void *p2) {
	player_info_t *player1, *player2;
	int team_comp;

    player1 = *((player_info_t **) p1);
	player2 = *((player_info_t **) p2);

	if (player1->spectator)
		return player2->spectator ? (player1 - player2) : 1;
	if (player2->spectator)
		return -1;

	if (cl.teamplay && (team_comp = strcmp(player1->team, player2->team)))
		return team_comp;

	return (player1 - player2);
}

int Player_NumtoSlot (int num) {
	int count, i;
	player_info_t *players[MAX_CLIENTS];

	for (count = i = 0; i < MAX_CLIENTS; i++)
		if (cl.players[i].name[0])
			players[count++] = &cl.players[i];
	
	qsort(players, count, sizeof(player_info_t *), Player_Compare);

	if (num < 1 || num > count)
		return PLAYER_NUM_NOMATCH;

	for (i = 0; i < MAX_CLIENTS; i++)
		if (&cl.players[i] == players[num-1])
			return i;

	return PLAYER_NUM_NOMATCH;
}

int Player_IdtoSlot (int id) {
	int j;

	for (j = 0; j < MAX_CLIENTS; j++) {
		if (cl.players[j].name[0] && cl.players[j].userid == id)
			return j;
	}
	return -1;
}

int Player_StringtoSlot(char *arg) {
	int i, slot;

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].name[0] && !strncmp(arg, cl.players[i].name, MAX_SCOREBOARDNAME - 1))
			return i;
	}

	if (!arg[0])
		return PLAYER_NAME_NOMATCH;

	for (i = 0; arg[i]; i++) {
		if (!isdigit(arg[i]))
			return PLAYER_NAME_NOMATCH;
	}
	return ((slot = Player_IdtoSlot(Q_atoi(arg))) >= 0) ? slot : PLAYER_ID_NOMATCH;
}

int Player_NametoSlot(char *name) {
	int i;

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].name[0] && !strncmp(Info_ValueForKey(cl.players[i].userinfo, "name"), name, 31))
			return i;
	}
	return PLAYER_NAME_NOMATCH;
}

int Player_SlottoId (int slot) {	
	return (slot >= 0 && slot < MAX_CLIENTS && cl.players[slot].name[0]) ? cl.players[slot].userid : -1;
}

char *Player_MyName (void) {
	return Info_ValueForKey(cls.demoplayback ? cls.userinfo : cl.players[cl.playernum].userinfo, "name");
}

int Player_GetSlot(char *arg) {
	int response, i;

	if ( (response = Player_StringtoSlot(arg)) >= 0  || response == PLAYER_ID_NOMATCH )
		return response;

	if (arg[0] != '#')
		return response;

	for (i = 1; arg[i]; i++) {
		if (!isdigit(arg[i]))
			return PLAYER_NAME_NOMATCH;
	}
	
	if ((response = Player_NumtoSlot(Q_atoi(arg + 1))) >= 0)
		return response;

	return PLAYER_NUM_NOMATCH;
}

/********************************** String Utils ****************************************/

qboolean Util_F_Match(char *_msg, char *f_request) {
	int offset, i, status, flags;
	char *s, *msg;

	msg = strdup(_msg);
	flags = TP_CategorizeMessage(msg, &offset);

	if (flags != 1 && flags != 4)
		return false;

	for (i = 0, s = msg + offset; i < strlen(s); i++)
		s[i] = s[i] & ~128;		

	if (strstr(s, f_request) != s) {
		free(msg);
		return false;
	}
	status = 0;
	for (s += strlen(f_request); *s; s++) {
		if (isdigit(*s) && status <= 1) {
			status = 1;
		} else if (isspace(*s)) {
			status = (status == 1) ? 2 : status;
		} else {
			free(msg);			
			return false;
		}
	}
	free(msg);
	return true;
}

/********************************** TF Utils ****************************************/

static char *Utils_TF_ColorToTeam_Failsafe(int color) {
	int i, j, teamcounts[8], numteamsseen = 0, best = -1;
	char *teams[MAX_CLIENTS];

	memset(teams, 0, sizeof(teams));
	memset(teamcounts, 0, sizeof(teamcounts));

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!cl.players[i].name[0] || cl.players[i].spectator)
			continue;
		if (cl.players[i].real_bottomcolor != color)
			continue;
		for (j = 0; j < numteamsseen; j++) {
			if (!strcmp(cl.players[i].team, teams[j]))
				break;
		}
		if (j == numteamsseen) {
			teams[numteamsseen] = cl.players[i].team;
			teamcounts[numteamsseen] = 1;
			numteamsseen++;
		} else {
			teamcounts[j]++;
		}
	}
	for (i = 0; i < numteamsseen; i++) {
		if (best == -1 || teamcounts[i] > teamcounts[best])
			best = i;
	}
	return (best == -1) ? "" : teams[best];
}

char *Utils_TF_ColorToTeam(int color) {
	char *s;

	switch (color) {
		case 13:
			if (*(s = Info_ValueForKey(cl.serverinfo, "team1")) || *(s = Info_ValueForKey(cl.serverinfo, "t1")))
				return s;
			break;
		case 4:
			if (*(s = Info_ValueForKey(cl.serverinfo, "team2")) || *(s = Info_ValueForKey(cl.serverinfo, "t2")))
				return s;
			break;
		case 12:
			if (*(s = Info_ValueForKey(cl.serverinfo, "team3")) || *(s = Info_ValueForKey(cl.serverinfo, "t3")))
				return s;
			break;
		case 11:
			if (*(s = Info_ValueForKey(cl.serverinfo, "team4")) || *(s = Info_ValueForKey(cl.serverinfo, "t4")))
				return s;
			break;
		default:
			return "";
	}
	return Utils_TF_ColorToTeam_Failsafe(color);
}

int Utils_TF_TeamToColor(char *team) {
	if (!Q_strcasecmp(team, Utils_TF_ColorToTeam(13)))
		return 13;
	if (!Q_strcasecmp(team, Utils_TF_ColorToTeam(4)))
		return 4;
	if (!Q_strcasecmp(team, Utils_TF_ColorToTeam(12)))
		return 12;
	if (!Q_strcasecmp(team, Utils_TF_ColorToTeam(11)))
		return 11;
	return 0;
}
