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

	$Id: utils.h,v 1.15 2007-05-28 10:47:36 johnnycz Exp $

*/

#ifndef __UTILS_H__
#define __UTILS_H__

#define	PLAYER_ID_NOMATCH		-1
#define	PLAYER_NAME_NOMATCH		-2
#define	PLAYER_NUM_NOMATCH		-3

char *CreateSpaces(int amount);
char *SecondsToMinutesString(int print_time);
char *SecondsToHourString(int time);
char *ColorNameToRGBString(char *color_name);
byte *StringToRGB(char *s);
int ParseFloats(char *s, float *f, int *f_size);

int Util_Extend_Filename(char *filename, char **ext);
qbool Util_Is_Valid_Filename(char *s);
qbool Util_Is_Valid_FilenameEx(char *s, qbool drive_prefix_valid);
char *Util_Invalid_Filename_Msg(char *s);
void Util_Process_Filename(char *string);
void Util_Process_FilenameEx(char *string, qbool allow_root);

int Player_IdtoSlot (int id);
int Player_SlottoId (int slot);
int Player_NametoSlot(char *name);
int Player_StringtoSlot(char *arg);
int Player_NumtoSlot (int num);
int Player_GetSlot(char *arg);
char *Player_MyName(void);

qbool Util_F_Match (const char *msg, char *f_req);

char *Utils_TF_ColorToTeam(int);
int Utils_TF_TeamToColor(char *);

void Replace_In_String (char *src,int n, char delim, int arg, ...);

qbool Utils_RegExpMatch(char *regexp, char *matchstring);
qbool Utils_RegExpGetGroup(char *regexp, char *matchstring, const char **resultstring, int *resultlength, int group);

float f_rnd( float from, float to );
int i_rnd( int from, int to );

#endif /* !__UTILS_H__ */
