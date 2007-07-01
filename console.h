/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

//
// console
//

#ifndef	_CONSOLE_H_

#define _CONSOLE_H_

typedef struct
{
	wchar	*text;
	int		maxsize;
	int		current;		// line where next message will be printed
	int		x;				// offset in current line for next print
	int		display;		// bottom of console displays this line
	int		numlines;		// number of non-blank text lines, used for backscroling
	clrinfo_t *clr; // clr[maxsize]
} console_t;

extern	console_t	con;

// colored frag messages offsets, used to add team/enemy colors to frag messages
typedef struct cfrags_format_s {
	int p1pos;	// position
	int p1len;	// length, if zero - no coloring
	int p1col;	// color
	int p2pos;
	int p2len;
	int p2col;
	qbool isFragMsg; // true if that was frag msg
} cfrags_format;

extern  qbool    con_addtimestamp;

extern	int			con_ormask;

extern int con_totallines;
extern int con_linewidth;
extern qbool con_initialized, con_suppress;
extern	int	con_notifylines;		// scan lines to clear for notify lines

void Con_CheckResize (void);
void Con_Init (void);
void Con_Shutdown (void);
void Con_DrawConsole (int lines);
void Con_SafePrintf (char *fmt, ...);
void Con_PrintW (wchar *txt);
void Con_Clear_f (void);
void Con_DrawNotify (void);

void SCR_DrawNotify(int x, int y, float scale, int notifyTime, int notifyLines, int notifyCols);

void Con_ClearNotify (void);
void Con_ToggleConsole_f (void);

extern cvar_t con_timestamps;

extern cvar_t con_shift;

extern cvar_t con_sound_mm1_file;
extern cvar_t con_sound_mm1_volume;
extern cvar_t con_sound_mm2_file;
extern cvar_t con_sound_mm2_volume;
extern cvar_t con_sound_spec_file;
extern cvar_t con_sound_spec_volume;
extern cvar_t con_sound_other_file;
extern cvar_t con_sound_other_volume;
extern  int         con_margin;         // kazik: margin on the left side

#endif		//_CONSOLE_H_
