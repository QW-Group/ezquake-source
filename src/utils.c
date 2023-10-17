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

	$Id: utils.c,v 1.47 2007-09-30 22:59:24 disconn3ct Exp $
*/

#include "quakedef.h"
#include <pcre2.h>
#include "hud.h"
#include "utils.h"
#include "qtv.h"
#include "teamplay.h"
#include "q_shared.h"

int TP_CategorizeMessage (const char *s, int *offset);

/************************************** General Utils **************************************/

void str_align_right (char *target, size_t size, const char *source, size_t length)
{
	if (length > size - 1)
		length = size - 1;

	if (strlen(source) >= length) {
		strlcpy(target, source, size);
		target[length] = 0;
	} else {
		int i;

		for (i = 0; i < length - strlen(source); i++) {
			target[i] = ' ';
		}

		strlcpy(target + i, source, size - i);
	}
}

char *str_repeat (char *str, int amount)
{
	char *ret = NULL;
	int i = 0;

	if (str == NULL)
		return NULL;

	if (amount <= 0)
		amount = 0;

	ret = (char *) Q_calloc(strlen(str) * amount + 1, sizeof(char)); 

	for (i = 0; i < amount; i++)
		strcat(ret, str);

	return ret;
}

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

#define RGB_COLOR_RED					"255 0 0"
#define RGB_COLOR_GREEN					"0 255 0"
#define RGB_COLOR_BLUE					"0 0 255"
#define RGB_COLOR_BLACK					"0 0 0"
#define RGB_COLOR_WHITE					"255 255 255"
#define RGB_COLOR_YELLOW				"255 255 0"
#define RGB_COLOR_PINK					"255 0 255"

#define COLOR_CHECK(_colorname, _colorstring, _rgbstring) \
	if(!strncmp(_colorstring, _colorname, sizeof(_colorstring))) return _rgbstring

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

/// \brief converts "255 255 0 128" to (255, 255, 0, 128) array and saves it to rgb output argument
/// \param[in] s string in "R G B A" format
/// \param[out] rgb array of 4 bytes
int StringToRGB_W(char *s, byte *rgb)
{
	int i;
	char buf[20]; // "255 255 255 255" - the longest possible string
	char *result;

	rgb[0] = rgb[1] = rgb[2] = rgb[3] = 255;

	strlcpy(buf, s, sizeof(buf));
	result = strtok(buf, " ");

	for (i = 0; i < 4 && result; i++, result = strtok(NULL, " ")) {
		rgb[i] = (byte) Q_atoi(result);
	}

	// TODO: Ok to do this in software also?
	// Use normal quake pallete if not all arguments where given.
	if (i < 3)
	{
		byte *col = (byte *) &d_8to24table[rgb[0]];
		rgb[0] = col[0];
		rgb[1] = col[1];
		rgb[2] = col[2];
	}

	return i;
}

void TrackerStringToRGB_W(const char *s, byte *rgb)
{
	rgb[0] = rgb[1] = rgb[2] = rgb[3] = 255;

	if (s[0] >= '0' && s[0] <= '9' && s[1] >= '0' && s[1] <= '9' && s[2] >= '0' && s[2] <= '9') {
		rgb[0] = ((s[0] - '0') / 9.0f) * 255;
		rgb[1] = ((s[1] - '0') / 9.0f) * 255;
		rgb[2] = ((s[2] - '0') / 9.0f) * 255;
	}
}

byte* StringToRGB(char *s)
{
	static byte rgb[4];
	StringToRGB_W(s, rgb);
	return rgb;
}

/*
   float f[10];
   int size = sizeof(f)/sizeof(f[0]);

// this will fill "f" with succesfully parsed floats from first string parammeter
// "size" will contain count of parsed floats
ParseFloats("1.0 2.0 999 5", f, &size);
*/
int ParseFloats(char *s, float *f, int *f_size) {
	int i, argc;
	tokenizecontext_t ctx;

	if (!s || !f || !f_size) {
		Sys_Error("ParseFloats() wrong params");
		return 0;
	}

	if (f_size[0] <= 0)
		return (f_size[0] = 0); // array have no size, unusual but no crime

	Cmd_TokenizeStringEx(&ctx, s);

	argc = min(Cmd_ArgcEx(&ctx), f_size[0]);

	for(i = 0; i < argc; i++)
		f[i] = Q_atof(Cmd_ArgvEx(&ctx, i));

	for( ; i < f_size[0]; i++)
		f[i] = 0; // zeroing unused elements

	return (f_size[0] = argc);
}

int strlen_color_by_terminator(const char *str, char terminator)
{
	int len = 0;

	if ( !str )
		return 0;

	while ( str[0] && str[0] != terminator )
	{
		if (str[0] == '&')
		{
			if (str[1] == 'c' && HexToInt(str[2]) >= 0 && HexToInt(str[3]) >= 0 && HexToInt(str[4]) >= 0)
			{
				str += 5; // skip "&cRGB"
				continue;
			}
			else if (str[1] == 'r')
			{
				str += 2; // skip "&r"
				continue;
			}
		}

		len++;
		str++;
	}

	return len;
}

// don't count ezquake color sequence
int strlen_color(const char *str)
{
	return strlen_color_by_terminator(str, 0);
}

// skip ezquake color sequence
void Util_SkipEZColors(char *dst, const char *src, size_t size)
{
	if (!dst || !src) {
		Sys_Error("Util_SkipColors: invalid input params");
		return;
	}

	if ( !size )
		return; // no space

	while ( src[0] && size )
	{
		if ( src[0] == '&' )
		{
			if ( src[1] == 'c' && HexToInt(src[2]) >= 0 && HexToInt(src[3]) >= 0 && HexToInt(src[4]) >= 0 )
			{
				src += 5; // skip "&cRGB"
				continue;
			}
			else if ( src[1] == 'r' )
			{
				src += 2; // skip "&r"
				continue;
			}
		}

		*dst++ = *src++;
		size--;
	}

	if ( !size )
		dst--; // seems we truncate string, step back for null terminator

	dst[0] = 0;
}

// strips all @chars chars from the @src string
void Util_SkipChars(const char *src, const char *chars, char *dst, size_t dstsize) {
	if (!dst || !src || !dstsize) {
		Com_Printf("Util_SkipChatWhite: illegal argument\n");
		return;
	}
	else {
		char *dstlast = dst + dstsize - 1;
		size_t chars_len = strlen(chars);
		int i;

		while (*src && dst < dstlast) {
			char c = *src++;
			qbool skipped = false;
			for (i = 0; i < chars_len; i++) {
				if (c == chars[i]) {
					skipped = true;
					break;
				}
			}
			if (!skipped) {
				*dst++ = c;
			}
		}
		*dst = '\0';
	}
}

char *str_trim(char *str)
{
	char *start; // points at first non-whitespace
	char *end; // points at last non-whitespace
	char *pos = str, *orig_str = str;

	while (*pos && isspace(*pos)) {
		pos++;
	}

	if (*pos == '\0') {
		*str = '\0';
		return orig_str;
	}

	start = pos;
	end = pos++;

	while (*pos) {
		if (!isspace(*pos)) {
			end = pos;
		}
		pos++;
	}

	if (start != orig_str || (end + 1) != pos) {
		for (pos = start; pos <= end; pos++) {
			*str++ = *pos;
		}

		*str = '\0';
	}

	return orig_str;
}

int HexToInt(char c)
{
	if (isdigit(c))
		return c - '0';
	else if (c >= 'a' && c <= 'f')
		return 10 + c - 'a';
	else if (c >= 'A' && c <= 'F')
		return 10 + c - 'A';
	else
		return -1;
}

/************************************** Game Mode Utils *********************************/

char *get_ktx_mode (void)
{
	return Info_ValueForKey(cl.serverinfo, "mode");
}

qbool check_ktx_ca (void)	// playing clan arena
{
	char *ktxmode = get_ktx_mode();

	return (strstr(ktxmode, "-ca") != NULL);
}

qbool check_ktx_wo (void)	// playing wipeout
{
	char *ktxmode = get_ktx_mode();

	return (strstr(ktxmode, "-wo") != NULL);
}

qbool check_ktx_ca_wo (void) // playing clan arena or wipeout
{
	return (check_ktx_ca() || check_ktx_wo());
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

void Util_Process_FilenameEx(char *string, qbool allow_root) {
	int i;

	if (!string)
		return;

	for (i = 0; i < strlen(string); i++)
		if (string[i] == '\\')
			string[i] = '/';

	if (!allow_root && string[0] == '/')
		for (i = 1; i <= strlen(string); i++)
			string[i - 1] = string[i];
}

void Util_ToValidFileName(const char* i, char* o, size_t buffersize)
{
	int k;
	unsigned char c;
	buffersize--;	// keep space for terminating zero

	for (k = 0; *i && k < buffersize; i++, k++) {
		c = (*i) & 127;
		if (c < 32 || c == '?' || c == '*' || c == ':' || c == '<' || c == '>' || c == '"')
			o[k] = '_'; // u can't use skin with such chars, so replace with some safe char
		else
			o[k] = c;
	}
	o[k] = '\0';
}

void Util_Process_Filename(char *string) {
	Util_Process_FilenameEx(string, false);
}

qbool Util_Is_Valid_FilenameEx(char *s, qbool drive_prefix_valid) {
	static char badchars[] = {'?', '*', ':', '<', '>', '"', '\0'};

	if (!s || !*s)
		return false;

	// this will allow things like this: c:/quake/<demo_dir>
	// probably windows specific only
	if ( drive_prefix_valid && (isupper(s[0]) || islower(s[0])) && s[1] == ':' )
		s += 2;

	if (strstr(s, "../") || strstr(s, "..\\") )
		return false;

	while (*s) {
		if (*s < 32 || *s >= 127 || strchr(badchars, *s))
			return false;
		s++;
	}
	return true;
}

qbool Util_Is_Valid_Filename(char *s)
{
	return  Util_Is_Valid_FilenameEx(s, false);
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

int Player_IdStringToSlot(const char* arg)
{
	int i;

	// Check if the argument is a user id instead
	// Make sure all chars in the given arg are digits in that case.
	for (i = 0; arg[i]; i++)
	{
		if (!isdigit((byte)arg[i])) {
			return PLAYER_NAME_NOMATCH;
		}
	}

	// Get player ID.
	return Player_IdtoSlot(Q_atoi(arg));
}

int Player_StringtoSlot(char *arg, qbool use_regular_expression, qbool prioritise_user_id)
{
	int i, slot, arg_length;

	if (!arg[0]) {
		return PLAYER_NAME_NOMATCH;
	}

	if (prioritise_user_id) {
		slot = Player_IdStringToSlot(arg);

		if (slot >= 0) {
			return slot;
		}
	}

	// Match on partial names by only comparing the
	// same amount of chars as in the given argument
	// with all the player names. The first match will be picked.
	arg_length = strlen(arg) + 1;

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
			Q_free(stripped);
			return i;
		}

		Q_free(stripped);
	}

	if (use_regular_expression) {
		// Regexp match against stripped player name if previous attempts have failed
		for (i = 0; i < MAX_CLIENTS; i++) {
			char *stripped = Player_StripNameColor(cl.players[i].name);

			if (cl.players[i].name[0] && Utils_RegExpMatch(arg, stripped)) {
				Q_free(stripped);
				return i;
			}

			Q_free(stripped);
		}
	}

	// Regexp match against stripped player name if previous attempts have failed
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		char *stripped = Player_StripNameColor(cl.players[i].name);

		if (cl.players[i].name[0] && Utils_RegExpMatch(arg, stripped))
		{
			Q_free(stripped);
			return i;
		}

		Q_free(stripped);
	}

	slot = Player_IdStringToSlot(arg);
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

int Player_GetTrackId(int player_id)
{
	int i, count;
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

	// Find track_id of player.
	for (i = 0; i < count; i++)
	{
		if (player_id == players[i]->userid)
		{
			return i+1;
		}
	}
	return -1;
}


int Player_GetSlot(char *arg, qbool prioritise_user_id)
{
	int response;

	// Try getting the slot by name or id.
	if ((response = Player_StringtoSlot(arg, false, prioritise_user_id)) >= 0 )
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

/********************************** Nick completion related ****************************************/

const char disallowed_in_nick[] = {'\n', '\f', '\\', '/', '\"', ' ' , ';', '\0'};

// yet another utility, there also exist at least one similar function Player_StripNameColor(), but not the same
void RemoveColors (char *name, size_t len)
{
	extern char readableChars[];
	char *s = name;

	if (!s || !*s)
		return;

	while (*s)
	{
		*s = readableChars[(unsigned char)*s] & 127;

		if (strchr (disallowed_in_nick, *s))
			*s = '_';

		s++;
	}

	// get rid of whitespace
	s = name;
	for (s = name; *s == '_'; s++) ;
	memmove (name, s, strlen(s) + 1);

	for (s = name + strlen(name); s > name  &&  (*(s - 1) == '_'); s--)
		; // empty

	*s = 0;

	if (!name[0])
		strlcpy (name, "_", len);
}

qbool FindBestNick (const char *nick, int flags, char *result, size_t result_len)
{
	int i, bestplayer = -1, best = 999999;
	char name[MAX_SCOREBOARDNAME], *match;

	result[0] = 0;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (flags & FBN_IGNORE_SPECS)
			if (cl.players[i].spectator)
				continue;
		if (flags & FBN_IGNORE_PLAYERS)
			if (!cl.players[i].spectator)
				continue;

		if (!cl.players[i].name[0])
			continue;

		strlcpy(name, cl.players[i].name, sizeof(name));
		RemoveColors (name, sizeof (name));
		for (match = name; match[0]; match++)
			match[0] = tolower(match[0]);

		if (!name[0])
			continue;

		if ((match = strstr(name, nick))  && match - name < best)
		{
			best = match - name;
			bestplayer = i;
		}
	}

	if (bestplayer != -1)
	{
		strlcpy(result, cl.players[bestplayer].name, result_len);
		return true;
	}

	if (flags & FBN_IGNORE_QTVSPECS)
		return false;

	return QTV_FindBestNick (nick, result, result_len);
}


/********************************** Clipboard ****************************************/

#ifndef _WIN32
#define CLIPBOARDSIZE 1024
static char clipboard[CLIPBOARDSIZE] = "\0";    // for clipboard implementation
#endif
// copies given text to clipboard
void CopyToClipboard(const char *text)
{
#ifdef _WIN32
	if (OpenClipboard(NULL))
	{
		LPTSTR  lptstrCopy;
		HGLOBAL hglbCopy;

		EmptyClipboard();
		hglbCopy = GlobalAlloc(GMEM_DDESHARE, strlen(text)+1);
		lptstrCopy = GlobalLock(hglbCopy);
		strcpy((char *)lptstrCopy, text);
		GlobalUnlock(hglbCopy);
		SetClipboardData(CF_TEXT, hglbCopy);

		CloseClipboard();
	}
#else
	strlcpy (clipboard, text, CLIPBOARDSIZE);
	Sys_CopyToClipboard(text);
#endif
}

// reads from clipboard
char *ReadFromClipboard(void)
{
#ifdef _WIN32
	static char clipbuf[1024];
	int     i;
	HANDLE  th;
	char    *clipText;

	clipbuf[0] = 0;

	if (OpenClipboard(NULL))
	{
		th = GetClipboardData(CF_TEXT);
		if (th)
		{
			clipText = GlobalLock(th);
			if (clipText)
			{
				strlcpy(clipbuf, clipText, sizeof (clipbuf));
				for (i=0; i < strlen(clipbuf); i++)
					if (clipbuf[i]=='\n' || clipbuf[i]=='\t' || clipbuf[i]=='\b')
						clipbuf[i] = ' ';
			}
			GlobalUnlock(th);
		}
		CloseClipboard();
	}
	return clipbuf;
#else
	return clipboard;
#endif
}


/********************************** String Utils ****************************************/

qbool Util_F_Match (const char *_msg, char *f_request) {
	int offset, i, status, flags;
	char *s, *msg;

	msg = Q_strdup(_msg);
	flags = TP_CategorizeMessage(msg, &offset);

	if (flags != msgtype_normal && flags != msgtype_spec) {
		Q_free(msg);
		return false;
	}

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


void Replace_In_String (char *src, int n, char delim, int num_args, ...)
{
	va_list ap;
	char msg[1024];
	char buf[256];
	char *msgp;
	int i, pad;
	qbool right;
	char *arg1, *arg2;

	// we will write the result back to src in code that follows
	strlcpy(msg,src,sizeof(msg));
	msgp = msg;
	*src = '\0';

	while (*msgp)
	{
		if(*msgp != delim)
		{
			buf[0] = *msgp++;
			buf[1] = '\0';
			strlcat(src, buf, n);
		}
		else
		{
			// process the delimiter
			msgp++;
			if (!*msgp) break;

			// process the initial minus
			right = *msgp == '-';
			if (right) msgp++;
			if (!*msgp) break;

			// process the number
			pad = atoi(msgp);
			while (isdigit(*msgp)) msgp++;
			if (!*msgp) break;

			// go through all available patterns
			va_start(ap, num_args);
			for (i=0; i < num_args; i++)
			{
				arg1 = va_arg(ap,char *);   // the pattern
				if (!arg1) break;

				arg2 = va_arg(ap,char *);   // the string to replace it with
				if (!arg2) break;

				// the pattern matched
				if (*msgp == *arg1)
				{
					if (pad)
					{
						if (right)  snprintf(buf, sizeof(buf)-1, "%-*s", pad, arg2);
						else        snprintf(buf, sizeof(buf)-1, "%*s", pad, arg2);
					}
					else
					{
						strlcpy(buf, arg2, sizeof(buf));
					}

					strlcat(src, buf, n);
					break;
				}
			}
			va_end(ap);

			msgp++;
		}
	}	
}

// compares two fun strings
int funcmp(const char *s1, const char *s2)
{
	char *t1, *t2;
	int ret;

	if (s1 == NULL  &&  s2 == NULL)
		return 0;

	if (s1 == NULL)
		return -1;

	if (s2 == NULL)
		return 1;

	t1 = Q_strdup(s1);
	t2 = Q_strdup(s2);

	FunToSort(t1);
	FunToSort(t2);

	ret = strcmp(t1, t2);

	Q_free(t1);
	Q_free(t2);

	return ret;
}

void FunToSort(char *text)
{
	char *tmp;
	char *s, *d;
	unsigned char c;
	tmp = (char *)Q_malloc(strlen(text) + 1);

	s = text;
	d = tmp;

	while ((c = (unsigned char)(*s++)) != 0) {
		if (c >= 18  &&  c <= 27)
			c += 30;
		else if (c >= 146  &&  c <= 155)
			c -= 98; // Yellow numbers.
		else if (c >= 32  &&  c <= 126)
			c = tolower(c);
		else if (c >= 160  &&  c <= 254)
			c  = tolower(c-128);
		else {
			switch (c) {
				case 1:
				case 2:
				case 3:
				case 4:
				case 6:
				case 7:
				case 8:
				case 9:
				case 132:   // kwadrat
					c = 210; break;
				case 5:
				case 14:
				case 15:
				case 28:
				case 133:
				case 142:
				case 143:
				case 156:   // dot
					c = 201; break;
				case 29:
				case 157:   // <
					c = 202; break;
				case 30:
				case 158:   // -
					c = 203; break;
				case 31:
				case 159:   // >
					c = 204; break;
				case 128:   // '('
					c = 205; break;
				case 129:   // '='
					c = 206; break;
				case 130:   // ')'
					c = 207; break;
				case 131:   // '+'
					c = 208; break;
				case 127:
				case 255:   // <-
					c = 209; break;
				case 134:   // d1
					c = 211; break;
				case 135:   // d2
					c = 212; break;
				case 136:   // d3
					c = 213; break;
				case 137:   // d4
					c = 214; break;
				case 16:
				case 144:   // '['
					c = '['; break;
				case 17:
				case 145:   // ']'
					c = ']'; break;
				case 141:   // '>'
					c = 200; break;
				case 10:
				case 11:
				case 12:
				case 13:
				case 138:
				case 139:
				case 140:   // ' '
					c = ' '; break;
			}
		}

		*d++ = c;
	}
	*d = 0;

	strcpy(text, tmp);
	Q_free(tmp);
}

#ifdef UNUSED_CODE
// todo: these functions are unused and there might already exist
// equivalent functions in our project; in such case remove them
unsigned char CharToBrown(unsigned char ch)
{
	if ( ch > 32 && ch <= 127 )
		return ch + 128;
	else
		return ch;
}

unsigned char CharToWhite(unsigned char ch)
{
	if ( ch > 160 )
		return ch - 128;
	else
		return ch;
}
#endif

void CharsToBrown(char* start, char* end)
{
	char *p = start;

	while (p < end) {
		if ( *p > 32 && *p <= 127 )
			*p += 128;
		p++;
	}
}

char *CharsToBrownStatic(char *in) // Print brown characters to console
{
	static char string[8][1024];
	static int idx = 0;
	int write_pos = 0;

	idx++;
	if (idx == 8)
		idx = 0;

	for (; *in && write_pos < 1023; in++) {
		string[idx][write_pos++] = 128 + *in;
	}
	string[idx][write_pos] = '\0';

	return string[idx];
}

void CharsToWhite(char* start, char* end)
{
	unsigned char *p = (unsigned char *)start;

	while (p < (unsigned char*)end) {
		if ( *p > 160 )
			*p -= 128;
		p++;
	}
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
	switch (color) {
		case 13:
			return cl.fixed_team_names[0];
		case 4:
			return cl.fixed_team_names[1];
		case 12:
			return cl.fixed_team_names[2];
		case 11:
			return cl.fixed_team_names[3];
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

/********************************** REGEXP ****************************************/

qbool Utils_RegExpMatch(char *regexp, char *matchstring)
{
	pcre2_code *re = NULL;
	int error;
	PCRE2_SIZE error_offset = 0;
	int match = 0;
	pcre2_match_data *match_data = NULL;

	re = pcre2_compile(
			(PCRE2_SPTR)regexp,	// The pattern.
			PCRE2_ZERO_TERMINATED,
			PCRE2_CASELESS,		// Case insensitive.
			&error,				// Error message.
			&error_offset,		// Error offset.
			NULL);				// use default character tables.

	// Check for an error compiling the regexp.
	if (!re)
		return false;

	// Check if we have a match.
	match_data = pcre2_match_data_create_from_pattern(re, NULL);
	if (re && (match = pcre2_match(re, (PCRE2_SPTR)matchstring, strlen(matchstring), 0, 0, match_data, NULL)) >= 0) {
		pcre2_match_data_free(match_data);
		pcre2_code_free(re);

		return true;
	}

	// Make sure we clean up.
	if (re) {
		pcre2_match_data_free(match_data);
		pcre2_code_free(re);
	}
	return false;
}

qbool Utils_RegExpGetGroup(char *regexp, char *matchstring, const char **resultstring, int *resultlength, int group)
{
	pcre2_code *re = NULL;
	int error;
	PCRE2_SIZE error_offset = 0;
	int match = 0;
	pcre2_match_data *match_data;

	re = pcre2_compile(
			(PCRE2_SPTR)regexp,	// The pattern.
			PCRE2_ZERO_TERMINATED,
			PCRE2_CASELESS,		// Case insensitive.
			&error,				// Error message.
			&error_offset,		// Error offset.
			NULL);				// use default character tables.

	if (!re)
		return false;

	match_data = pcre2_match_data_create_from_pattern(re, NULL);
	if (re && (match = pcre2_match(re, (PCRE2_SPTR)matchstring, strlen(matchstring), 0, 0, match_data, NULL)) >= 0) {
		int substring_length = 0;
		error = pcre2_substring_get_bynumber (match_data, group, (PCRE2_UCHAR8**)resultstring, (size_t*)&substring_length);

		if (resultlength != NULL) {
			(*resultlength) = substring_length;
		}

		pcre2_match_data_free (match_data);
		if (re) {
			pcre2_code_free(re);
		}

		return (error != PCRE2_ERROR_NOSUBSTRING && error != PCRE2_ERROR_NOMEMORY);
	}

	pcre2_match_data_free (match_data);
	if (re) {
		pcre2_code_free(re);
	}

	return false;
}


// QW262 -->
// regexp match support for group operations in scripts
int			wildcard_level = 0;
pcre2_code	*wildcard_re[4];

qbool IsRegexp(const char *str)
{
	if (*str == '+' || *str == '-') {
		// +/- aliases; valid regexp can not start with +/-
		return false;
	}

	return (strcspn(str, "\\\"()[]{}.*+?^$|")) != strlen(str) ? true : false;
}

qbool ReSearchInit(const char* wildcard)
{
	return ReSearchInitEx(wildcard, true);
}

qbool ReSearchInitEx(const char *wildcard, qbool case_sensitive)
{
	int error;
	PCRE2_SIZE error_offset;

	if (wildcard_level == 4) {
		Com_Printf("Error: Regexp commands nested too deep\n");
		return false;
	}
	wildcard_re[wildcard_level] = pcre2_compile((PCRE2_SPTR)wildcard, PCRE2_ZERO_TERMINATED, (case_sensitive ? 0 : PCRE2_CASELESS), &error, &error_offset, NULL);
	if (!wildcard_re[wildcard_level]) {
		PCRE2_UCHAR error_str[256];
		pcre2_get_error_message(error, error_str, sizeof(error_str));
		Com_Printf ("Invalid regexp: %s %d\n", error_str, error_offset);
		return false;
	}


	wildcard_level++;
	return true;
}

qbool ReSearchMatch (const char *str)
{
	int result;
	pcre2_match_data *match_data = NULL;
	match_data = pcre2_match_data_create_from_pattern(wildcard_re[wildcard_level-1], NULL);
	result = pcre2_match(wildcard_re[wildcard_level-1],
			(PCRE2_SPTR)str, strlen(str), 0, 0, match_data, NULL);
	pcre2_match_data_free(match_data);
	return (result>0) ? true : false;
}

void ReSearchDone (void)
{
	wildcard_level--;
	if (wildcard_re[wildcard_level]) {
		(pcre2_code_free)(wildcard_re[wildcard_level]);
	}
}
// <-- QW262


// ***************** VC issues **********************************

#if (_MSC_VER && (_MSC_VER < 1400))
extern _ftol2(double f);
long _ftol2_sse(double f)
{
	return _ftol2(f);
}
#endif

// ***************** random *************************************

float f_rnd( float from, float to )
{
	float r;

	if ( from >= to )
		return from;

	r = from + (to - from) * ((float)rand() / RAND_MAX);

	return bound(from, r, to);
}

int i_rnd( int from, int to )
{
	float r;

	if ( from >= to )
		return from;

	r = (int)(from + (1.0 + to - from) * ((float)rand() / RAND_MAX));

	return bound(from, r, to);
}

//
// Gets the next part of a string that fits within a specified width.
//
//		input			= The string we want to wordwrap.
//		target			= The string that we should put the next line in.
//		start_index		= The index in the input string where we start wordwrapping.
//		end_index		= The returned end index of the word wrapped string.
//		target_max_size = The length of the target string buffer.
//		max_pixel_width	= The pixel width that the string should be wordwrapped within.
//		char_size		= The size of the characters in the string.
//
qbool Util_GetNextWordwrapString(const char *input, char *target, int start_index, int *end_index_ret, int target_max_size, int max_pixel_width, int char_size)
{
	qbool wrap_found	= false;
	int retval			= true;

	int max_char_width	= Q_rint((float)max_pixel_width / char_size - 1);	// The max number of characters allowed (excluding white spaces at the end).
	int end_index		= start_index;										// The index of the last character to include in this line.
	int input_len		= strlen(input + start_index) + 1;					// Length of the input including \0.
	int max_width		= min(max_char_width, input_len);					// The max width allowed for the line.

	// New lines take precendence as a wrap.
	for (end_index = start_index; end_index < (start_index + max_width); end_index++)
	{
		if (input[end_index] == '\n')
		{
			wrap_found = true;
			break;
		}

		// Have we reached the end of the string?
		if (input[end_index] == '\0')
		{
			retval = false;
			break;
		}
	}

	if (!wrap_found)
	{
		if (input[end_index] != ' ')
		{
			// We're in the middle of a word, try to find a word boundary instead.
			int i;

			for (i = end_index; i > start_index; i--)
			{
				if (input[i] == ' ')
				{
					end_index = i;
					break;
				}
			}
		}
		else
		{
			// Eat up all white spaces at the end of the line.
			while (input[end_index + 1] == ' ')
			{
				end_index++;
			}
		}
	}

	// Return the end index.
	if (end_index_ret)
	{
		(*end_index_ret) = end_index;
	}

	// Copy the wrap string to the target buffer.
	if (target)
	{
		snprintf(target, min(target_max_size, max_width), "%s", input + start_index);
	}

	return retval;
}

void Utils_RegExpFreeSubstring(char* substring)
{
	if (substring) {
		pcre2_substring_free((PCRE2_UCHAR8*)substring);
	}
}

