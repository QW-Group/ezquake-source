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

	$Id: utils.h,v 1.21 2007-09-15 15:26:17 cokeman1982 Exp $

*/

#ifndef __UTILS_H__
#define __UTILS_H__

#define	PLAYER_ID_NOMATCH		-1
#define	PLAYER_NAME_NOMATCH		-2
#define	PLAYER_NUM_NOMATCH		-3

/// 
/// General Utils
///

char *CreateSpaces(int amount);
char *SecondsToMinutesString(int print_time);
char *SecondsToHourString(int time);
char *ColorNameToRGBString(char *color_name);
byte *StringToRGB(char *s);
int StringToRGB_W(char *s, byte *rgb);
void RGBToString(const byte *rgb, char *s);
void TrackerStringToRGB_W(const char *s, byte *rgb);
int ParseFloats(char *s, float *f, int *f_size);

// don't count ezquake color sequence
int strlen_color(const char *str);
int strlen_color_by_terminator(const char *str, char terminator);
// skip ezquake color sequence
void Util_SkipEZColors(char *dst, const char *src, size_t size);
void Util_SkipChars(const char *src, const char *chars, char *dst, size_t dstsize);
void str_align_right (char *target, size_t size, const char *source, size_t length);

// skip whitespace from both beginning and end
char *str_trim(char *str);

int HexToInt(char c);

///
/// Game Mode utils
///

char *get_ktx_mode(void);
qbool check_ktx_ca(void);
qbool check_ktx_wo(void);
qbool check_ktx_ca_wo(void);

///
/// Filename utils
///

int Util_Extend_Filename(char *filename, char **ext);
qbool Util_Is_Valid_Filename(char *s);
qbool Util_Is_Valid_FilenameEx(char *s, qbool drive_prefix_valid);
char *Util_Invalid_Filename_Msg(char *s);
void Util_Process_Filename(char *string);
void Util_Process_FilenameEx(char *string, qbool allow_root);
void Util_ToValidFileName(const char* input, char* output, size_t buffersize);

///
/// Player utils
///

int Player_IdtoSlot (int id);
int Player_SlottoId (int slot);
int Player_NametoSlot(char *name);
int Player_StringtoSlot(char *arg, qbool use_regular_expressions, qbool prioritise_user_id);
int Player_NumtoSlot (int num);
int Player_GetSlot(char *arg, qbool prioritise_user_id);
int Player_GetTrackId(int player_id);
char *Player_MyName(void);

///
/// Nick completion related
///


// yet another utility, there also exist at least one similar function Player_StripNameColor(), but not the same
void RemoveColors (char *name, size_t len);

#define FBN_IGNORE_SPECS		(1<<0)
#define FBN_IGNORE_PLAYERS		(1<<1)
#define FBN_IGNORE_QTVSPECS		(1<<2)

qbool FindBestNick (const char *nick, int flags, char *result, size_t result_len);

///
/// Clipboard
///
void CopyToClipboard(const char *text);
char *ReadFromClipboard(void);

///
/// TF Team-Color Utils
///

char *Utils_TF_ColorToTeam(int);
int Utils_TF_TeamToColor(char *);

///
/// String Utils 
///

qbool Util_F_Match (const char *msg, char *f_req);
void Replace_In_String (char *src,int n, char delim, int arg, ...);
/// converts fun text to string prepared to sort
void FunToSort(char *text);
/// compares two fun strings
int funcmp(const char *s1, const char *s2);
#ifdef UNUSED_CODE
unsigned char CharToBrown(unsigned char ch);
unsigned char CharToWhite(unsigned char ch);
#endif
void CharsToBrown(char* start, char* end);
char *CharsToBrownStatic(char *in);
void CharsToWhite(char* start, char* end);

///
/// REGEXP
///

qbool Utils_RegExpMatch(char *regexp, char *matchstring);
qbool Utils_RegExpGetGroup(char *regexp, char *matchstring, const char **resultstring, int *resultlength, int group);
void Utils_RegExpFreeSubstring(char* substring);

// regexp match support for group operations in scripts
qbool IsRegexp(const char *str);
qbool ReSearchInit(const char *wildcard);
qbool ReSearchInitEx(const char* wildcard, qbool case_sensitive);
qbool ReSearchMatch(const char *str);
void ReSearchDone(void);

///
/// RANDOM GENERATORS
///

float f_rnd( float from, float to );
int i_rnd( int from, int to );

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
qbool Util_GetNextWordwrapString(const char *input, char *target, int start_index, int *end_index, int target_max_size, int max_pixel_width, int char_size);

#endif /* !__UTILS_H__ */
