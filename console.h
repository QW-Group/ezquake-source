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

#define		CON_TEXTSIZE	32768
typedef struct
{
	char	*text;
	int		maxsize;
	int		current;		// line where next message will be printed
	int		x;				// offset in current line for next print
	int		display;		// bottom of console displays this line
	int		numlines;		// number of non-blank text lines, used for backscroling
} console_t;

extern	console_t	con;
extern	int			con_ormask;

extern int con_totallines;
extern qboolean con_initialized, con_suppress;
extern	int	con_notifylines;		// scan lines to clear for notify lines

void Con_CheckResize (void);
void Con_Init (void);
void Con_Shutdown (void);
void Con_DrawConsole (int lines);
void Con_Print (char *txt);
void Con_Clear_f (void);
void Con_DrawNotify (void);
void Con_ClearNotify (void);
void Con_ToggleConsole_f (void);

#endif		//_CONSOLE_H_
