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

#ifndef __UTILS_H_

#define __UTILS_H_

#define	PLAYER_ID_NOMATCH		-1
#define	PLAYER_NAME_NOMATCH		-2
#define	PLAYER_NUM_NOMATCH		-3

char *CreateSpaces(int amount);
char *SecondsToMinutesString(int print_time);
char *SecondsToHourString(int time);
byte *StringToRGB(char *s);

int Util_Extend_Filename(char *filename, char **ext);
qboolean Util_Is_Valid_Filename(char *s);
char *Util_Invalid_Filename_Msg(char *s);
void Util_Process_Filename(char *string);

int Player_IdtoSlot (int id);
int Player_SlottoId (int slot);
int Player_NametoSlot(char *name);
int Player_StringtoSlot(char *arg);
int Player_NumtoSlot (int num);
int Player_GetSlot(char *arg);
char *Player_MyName(void);

qboolean Util_F_Match(char *msg, char *f_req);

char *Utils_TF_ColorToTeam(int);
int Utils_TF_TeamToColor(char *);

#endif
