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

	$Id: utils.c,v 1.27 2006-12-07 11:53:47 disconn3ct Exp $
*/

#include "quakedef.h"

int TP_CategorizeMessage (char *s, int *offset);

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
	snprintf (time, sizeof(time), "%i%i:%i%i", tens_minutes, minutes, tens_seconds, seconds);
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
	snprintf (time, sizeof(time), "%i%i:%i%i:%i%i", tens_hours, hours, tens_minutes, minutes, tens_seconds, seconds);
	return time;
}

#ifdef GLQUAKE
#define RGB_COLOR_RED					"255 0 0"
#define RGB_COLOR_GREEN					"0 255 0"
#define RGB_COLOR_BLUE					"0 0 255"
#define RGB_COLOR_BLACK					"0 0 0"
#define RGB_COLOR_WHITE					"255 255 255"
#define RGB_COLOR_YELLOW				"255 255 0"
#define RGB_COLOR_PINK					"255 0 255"
#else
#define RGB_COLOR_RED					"251"
#define RGB_COLOR_GREEN					"63"
#define RGB_COLOR_BLUE					"208"
#define RGB_COLOR_BLACK					"0"
#define RGB_COLOR_WHITE					"254"
#define RGB_COLOR_YELLOW				"182"
#define RGB_COLOR_PINK					"144"
#endif

#define COLOR_CHECK(_colorname, _colorstring, _rgbstring) \
	if(!strncmp(_colorstring, _colorname, strlen(_colorstring))) return _rgbstring

char *ColorNameToRGBString(char *color_name)
{
	COLOR_CHECK(color_name, "red",		RGB_COLOR_RED);
	COLOR_CHECK(color_name, "green",	RGB_COLOR_GREEN);
	COLOR_CHECK(color_name, "blue",		RGB_COLOR_BLUE);
	COLOR_CHECK(color_name, "black",	RGB_COLOR_BLACK);
	COLOR_CHECK(color_name, "yellow",	RGB_COLOR_YELLOW);
	COLOR_CHECK(color_name, "pink",		RGB_COLOR_PINK);
	COLOR_CHECK(color_name, "white",	RGB_COLOR_WHITE);

	return color_name;
}

byte *StringToRGB(char *s) {
	int i;
	static byte rgb[4];

	rgb[0] = rgb[1] = rgb[2] = rgb[3] = 255;

	Cmd_TokenizeString(s);

	#ifdef GLQUAKE
	// Use normal quake pallete if not all arguments where given.
	if(Cmd_Argc() < 3)
	{
		byte *col = (byte *) &d_8to24table[(byte) Q_atof(s)];
		rgb[0] = col[0];
		rgb[1] = col[1];
		rgb[2] = col[2];
	}
	else
	#endif
	{
		for(i = 0; i < min(Cmd_Argc(), sizeof(rgb)/sizeof(rgb[0])); i++)
		{
			rgb[i] = (byte) Q_atof(Cmd_Argv(i));
		}
	}
	return rgb;
}

/************************************** File Utils **************************************/

int Util_Extend_Filename(char *filename, char **ext) {
	char extendedname[1024], **s;
	int i, offset;
	FILE *f;

	strlcpy(extendedname, filename, sizeof(extendedname));
	offset = strlen(extendedname);

	i = -1;
	while(1) {
		if (++i == 1000)
			break;
		for (s = ext; *s; s++) { 
			snprintf (extendedname + offset, sizeof(extendedname) - offset, "_%03i.%s", i, *s);
			if ((f = fopen(extendedname, "rb"))) {
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

qbool Util_Is_Valid_Filename(char *s) {
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

	snprintf(err, sizeof(err), "%s is not a valid filename (?*:<>\" are illegal characters)\n", s);
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

int Player_NumtoSlot (int num) 
{
	int count, i;
	player_info_t *players[MAX_CLIENTS];

	// Get all the joined players (including spectators).
	for (count = i = 0; i < MAX_CLIENTS; i++)
	{
		if (cl.players[i].name[0])
		{
			players[count++] = &cl.players[i];
		}
	}
	
	// Sort them according to team and if they're a spectator.
	qsort(players, count, sizeof(player_info_t *), Player_Compare);

	if (num < 1 || num > count)
	{
		return PLAYER_NUM_NOMATCH;
	}

	// Find the ith player.
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (&cl.players[i] == players[num-1])
		{
			return i;
		}
	}

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

char *Player_StripNameColor(const char *name)
{	
	extern char readableChars[]; // console.c
	char *stripped = NULL;
	int i, name_length;
	name_length = strlen(name);

	stripped = (char *)Q_calloc(name_length + 1, sizeof(char));

	// Strip the color by setting bit 7 = 0 on all letters.
	for (i = 0; i < name_length; i++)
	{
		stripped[i] = readableChars[(unsigned char)name[i]] & 127;
	}

	stripped[i] = '\0';

	return stripped;
}

int Player_StringtoSlot(char *arg) 
{
	int i, slot, arg_length;

	if (!arg[0])
	{
		return PLAYER_NAME_NOMATCH;
	}

	// Match on partial names by only comparing the
	// same amount of chars as in the given argument
	// with all the player names. The first match will be picked.
	arg_length = strlen(arg);

	// Try finding the player by comparing the argument to all the player names (CASE SENSITIVE)
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (cl.players[i].name[0] && !strncmp(arg, cl.players[i].name, arg_length))
		{
			return i;
		}
	}

	// Maybe the user didn't use the correct case, so try without CASE SENSITIVITY.
	// (We loop once more so that if the correct case is given, that match will get precedence).
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (cl.players[i].name[0] && !strncasecmp(arg, cl.players[i].name, arg_length))
		{
			return i;
		}
	}

	// If the players name is color coded it's hard to type. 
	// So strip the color codes and see if it's a match.
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		char *stripped = Player_StripNameColor(cl.players[i].name);

		if (cl.players[i].name[0] && !strncasecmp(arg, stripped, arg_length))
		{
			if(stripped != NULL)
			{
				Q_free(stripped);
				stripped = NULL;
			}
			return i;
		}

		if(stripped != NULL)
		{
			Q_free(stripped);
			stripped = NULL;
		}
	}

	// Check if the argument is a user id instead
	// Make sure all chars in the given arg are digits in that case.
	for (i = 0; arg[i]; i++)
	{
		if (!isdigit(arg[i]))
		{
			return PLAYER_NAME_NOMATCH;
		}
	}

	// Get player ID.
	slot = Player_IdtoSlot(Q_atoi(arg));

	return (slot >= 0) ? slot : PLAYER_ID_NOMATCH;
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

int Player_GetSlot(char *arg) 
{
	int response;

	// Try getting the slot by name or id.
	if ((response = Player_StringtoSlot(arg)) >= 0 )
		//|| response == PLAYER_ID_NOMATCH)
	{
		return response;
	}

	// We didn't find any player or ID that matched
	// so we'll try treating it as the players
	// sorted position.
	if (arg[0] != '#')
	{
		return response;
	}
	
	if ((response = Player_NumtoSlot(Q_atoi(arg + 1))) >= 0)
	{
		return response;
	}

	return PLAYER_NUM_NOMATCH;
}

/********************************** String Utils ****************************************/

qbool Util_F_Match(char *_msg, char *f_request) {
	int offset, i, status, flags;
	char *s, *msg;

	msg = Q_strdup(_msg);
	flags = TP_CategorizeMessage(msg, &offset);

	if (flags != 1 && flags != 4)
		return false;

	for (i = 0, s = msg + offset; i < strlen(s); i++)
		s[i] = s[i] & ~128;		

	if (strstr(s, f_request) != s) {
		Q_free(msg);
		return false;
	}
	status = 0;
	for (s += strlen(f_request); *s; s++) {
		if (isdigit(*s) && status <= 1) {
			status = 1;
		} else if (isspace(*s)) {
			status = (status == 1) ? 2 : status;
		} else {
			Q_free(msg);			
			return false;
		}
	}
	Q_free(msg);
	return true;
}


void Replace_In_String (char *src, int n, char delim, int num_args, ...){
	
	va_list ap;
	char msg[1024];
	char count[5];
	int y=0,i=0,j=0,k=0,l=0,m=0;
	char *arg1, *arg2 ;
	
	if (n>sizeof(msg))
		return;

	strlcpy(msg,src,sizeof(msg));

	while (msg[i] != '\0' && j < n) {
		if(msg[i++] != delim)
			src[j++] = msg[i-1];
		else{
			if (msg[i] == '-'){
				m = 1;
				i++;
			}
			if (strspn(msg+i,"1234567890")){
					l = strspn(msg+i,"1234567890");
					if(l>5){
						strncpy(count,msg+i,sizeof(count));
						k = atoi(count);
					}else if(l == 1){
						k = atoi(msg+i);
					}else{
						strncpy(count,msg+i,l);
						k = atoi(count);
					}
			i += l ;
			}
			

			va_start(ap, num_args);
			for(y=0; y < num_args; y++){
				arg1 = va_arg(ap,char *);
				if( (arg2 = va_arg(ap,char *)) == NULL)
					break;

				if (msg[i] == *arg1){
					if (j+strlen(arg2)>n || j + k > n)
						break;

					if(k && m){
                    strcpy(src + j ,va("%-*s",k,arg2));
		    			j+=k;
					}else if (k && !m){
                    strcpy(src + j ,va("%*s",k,arg2));
					j+=k;
					}else{
                    strcpy(src + j ,va("%s",arg2));
					j+=strlen(arg2);
					}
					m=0;
					
					continue;
				}
			}
			va_end(ap);
		i++;
		k=l=0;
		}
	}	
	src[j]='\0';
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
	if (!strcasecmp(team, Utils_TF_ColorToTeam(13)))
		return 13;
	if (!strcasecmp(team, Utils_TF_ColorToTeam(4)))
		return 4;
	if (!strcasecmp(team, Utils_TF_ColorToTeam(12)))
		return 12;
	if (!strcasecmp(team, Utils_TF_ColorToTeam(11)))
		return 11;
	return 0;
}

qbool Utils_RegExpMatch(char *regexp, char *matchstring)
{
	int offsets[HUD_REGEXP_OFFSET_COUNT];
	pcre *re = NULL;
	char *error = NULL;
	int erroffset = 0;
	int match = 0;

	re = pcre_compile(
			regexp,				// The pattern.
			PCRE_CASELESS,		// Case insensitive.
			&error,				// Error message.
			&erroffset,			// Error offset.
			NULL);				// use default character tables.

	// Check for an error compiling the regexp.
	if(error)
	{
		Q_free(error);
		error = NULL;

		if(re)
		{
			Q_free(re);
			re = NULL;
		}

		return false;
	}

	// Check if we have a match.
	if(re && (match = pcre_exec(re, NULL, matchstring, strlen(matchstring), 0, 0, offsets, HUD_REGEXP_OFFSET_COUNT)) >= 0)
	{
		if(re)
		{
			Q_free(re);
			re = NULL;
		}

		return true;
	}

	// Make sure we clean up.
	if(re)
	{
		Q_free(re);
		re = NULL;
	}

	if(error)
	{
		Q_free(error);
		error = NULL;
	}

	return false;
}

qbool Utils_RegExpGetGroup(char *regexp, char *matchstring, char **resultstring, int *resultlength, int group)
{
	int offsets[HUD_REGEXP_OFFSET_COUNT];
	pcre *re = NULL;
	char *error = NULL;
	int erroffset = 0;
	int match = 0;

	re = pcre_compile(
			regexp,				// The pattern.
			PCRE_CASELESS,		// Case insensitive.
			&error,				// Error message.
			&erroffset,			// Error offset.
			NULL);				// use default character tables.

	if(error)
	{
		Q_free(error);
		error = NULL;

		if(re)
		{
			Q_free(re);
			re = NULL;
		}
		return false;
	}

	if(re && (match = pcre_exec(re, NULL, matchstring, strlen(matchstring), 0, 0, offsets, HUD_REGEXP_OFFSET_COUNT)) >= 0)
	{
		int substring_length = 0;
		substring_length = pcre_get_substring (matchstring, offsets, match, group, resultstring);
		
		if (resultlength != NULL)
		{
			(*resultlength) = substring_length;
		}

		if(re)
		{
			Q_free(re);
			re = NULL;
		}

		return (substring_length != PCRE_ERROR_NOSUBSTRING && substring_length != PCRE_ERROR_NOMEMORY);
	}

	if(re)
	{
		Q_free(re);
		re = NULL;
	}

	if(error)
	{
		Q_free(error);
		error = NULL;
	}

	return false;
}

#if (_MSC_VER && (_MSC_VER < 1400))
extern _ftol2(double f);
long _ftol2_sse(double f)
{
	return _ftol2(f);
}
#endif
