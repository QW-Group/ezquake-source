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
// console.c

#include "quakedef.h"
#include "keys.h"
#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
#else
#include <stdio.h>
#endif


#include "ignore.h"
#include "logging.h"

#define		MINIMUM_CONBUFSIZE	(1 << 15)
#define		DEFAULT_CONBUFSIZE	(1 << 16)
#define		MAXIMUM_CONBUFSIZE	(1 << 22)

console_t	con;

int			con_ormask;
int 		con_linewidth;		// characters across screen
int			con_totallines;		// total lines in console scrollback
float		con_cursorspeed = 4;

cvar_t		_con_notifylines = {"con_notifylines","4"};
cvar_t		con_notifytime = {"con_notifytime","3"};		//seconds
cvar_t		con_wordwrap = {"con_wordwrap","1"};
cvar_t		con_clearnotify = {"con_clearnotify","1"};

#define	NUM_CON_TIMES 16
float		con_times[NUM_CON_TIMES];	// cls.realtime time the line was generated
										// for transparent notify lines

int			con_vislines;
int			con_notifylines;			// scan lines to clear for notify lines

#define		MAXCMDLINE	256
extern	char	key_lines[32][MAXCMDLINE];
extern	int		edit_line;
extern	int		key_linepos;

qboolean	con_initialized = false;
qboolean	con_suppress = false;

FILE		*qconsole_log;

void Key_ClearTyping (void) {
	key_lines[edit_line][1] = 0;	// clear any typing
	key_linepos = 1;
}

void Con_ToggleConsole_f (void) {
	Key_ClearTyping ();

	if (key_dest == key_console) {
		if (!SCR_NEED_CONSOLE_BACKGROUND)
			key_dest = key_game;
	} else {
		key_dest = key_console;
	}

	if (con_clearnotify.value)
		Con_ClearNotify ();
}

void Con_Clear_f (void) {
	con.numlines = 0;
	memset (con.text, ' ', con.maxsize);
	con.display = con.current;
}

void Con_ClearNotify (void) {
	int i;

	for (i = 0; i < NUM_CON_TIMES; i++)
		con_times[i] = 0;
}

void Con_MessageMode_f (void) {
	if (cls.state != ca_active)
		return;

	chat_team = false;
	key_dest = key_message;
	chat_buffer[0] = 0;
	chat_linepos = 0;
}

void Con_MessageMode2_f (void) {
	if (cls.state != ca_active)
		return;

	chat_team = true;
	key_dest = key_message;
	chat_buffer[0] = 0;
	chat_linepos = 0;
}

//If the line width has changed, reformat the buffer
void Con_CheckResize (void) {
	int i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char *tempbuf;

	width = (vid.width >> 3) - 2;

	if (width == con_linewidth)
		return;

	if (width < 1) { // video hasn't been initialized yet
		width = 38;
		con_linewidth = width;
		con_totallines = con.maxsize / con_linewidth;
		memset (con.text, ' ', con.maxsize);
	} else {
		oldwidth = con_linewidth;
		con_linewidth = width;
		oldtotallines = con_totallines;
		con_totallines = con.maxsize / con_linewidth;
		numlines = oldtotallines;

		if (con_totallines < numlines)
			numlines = con_totallines;

		numchars = oldwidth;

		if (con_linewidth < numchars)
			numchars = con_linewidth;

		tempbuf = Hunk_TempAlloc(con.maxsize);
		memcpy (tempbuf, con.text, con.maxsize);
		memset (con.text, ' ', con.maxsize);

		for (i = 0; i < numlines; i++) {
			for (j = 0; j < numchars; j++) {
				con.text[(con_totallines - 1 - i) * con_linewidth + j] =
					tempbuf[((con.current - i + oldtotallines) % oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify ();
	}

	con.current = con_totallines - 1;
	con.display = con.current;
}


char readableChars[256] = {	'.', '_' , '_' , '_' , '_' , '.' , '_' , '_' , '_' , '_' , 10 , '_' , 10 , '>' , '.' , '.',
						'[', ']', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.', '_', '_', '_'};


static void Con_CreateReadableChars(void) {
	int i;

	for (i = 32; i < 127; i++)
		readableChars[i] = readableChars[128 + i] = i;
	readableChars[127] = readableChars[128 + 127] = '_';

	for (i = 0; i < 32; i++)
		readableChars[128 + i] = readableChars[i];
	readableChars[128] = '_';
	readableChars[10 + 128] = '_';
	readableChars[12 + 128] = '_';
}


static void Con_InitConsoleBuffer(console_t *conbuffer, int size) {
	con.maxsize = size;
	con.text = Hunk_AllocName(con.maxsize, "console_buffer");
}

void Con_Init (void) {
	int i, conbufsize;

	if (dedicated)
		return;

	if (COM_CheckParm("-condebug"))
		qconsole_log = fopen(va("%s/qw/qconsole.log",com_basedir), "a");

	if ((i = COM_CheckParm("-conbufsize")) && i + 1 < com_argc) {
		conbufsize = Q_atoi(com_argv[i + 1]) << 10;
		conbufsize = bound (MINIMUM_CONBUFSIZE , conbufsize, MAXIMUM_CONBUFSIZE);
	} else {
		conbufsize = DEFAULT_CONBUFSIZE;
	}
	Con_InitConsoleBuffer(&con, conbufsize);

	con_linewidth = -1;
	Con_CheckResize ();

	Con_CreateReadableChars();	

	con_initialized = true;
	Com_Printf ("Console initialized\n");

	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	// register our commands and cvars
	Cvar_Register (&_con_notifylines);
	Cvar_Register (&con_notifytime);
	Cvar_Register (&con_wordwrap);
	Cvar_Register (&con_clearnotify);

	Cvar_ResetCurrentGroup();

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
}

void Con_Shutdown (void) {
	if (qconsole_log)
		fclose(qconsole_log);
}

void Con_Linefeed (void) {
	con.x = 0;
	if (con.display == con.current)
		con.display++;
	con.current++;
	if (con.numlines < con_totallines)
		con.numlines++;
	memset (&con.text[(con.current%con_totallines)*con_linewidth], ' ', con_linewidth);
}

//Handles cursor positioning, line wrapping, etc
void Con_Print (char *txt) {
	int y, c, l, mask;
	static int cr;

	if (qconsole_log) {
		fprintf(qconsole_log, "%s", txt);
		fflush(qconsole_log);
	}
	if (Log_IsLogging()) {
		if (log_readable.value) {
			char *s, tempbuf[4096];	

			Q_strncpyz(tempbuf, txt, sizeof(tempbuf));
			for (s = tempbuf; *s; s++)
				*s = readableChars[(unsigned char) *s];
			Log_Write(tempbuf);	
		} else {
			Log_Write(txt);	
		}
	}

	if (!con_initialized || con_suppress)
		return;

	if (txt[0] == 1 || txt[0] == 2)	{
		mask = 128;		// go to colored text
		txt++;
	} else {
		mask = 0;
	}

	while ((c = *txt)) {
		// count word length
		for (l = 0; l < con_linewidth; l++) {
			char d = txt[l] & 127;
			if ((con_wordwrap.value && (!txt[l] || d == 0x09 || d == 0x0D || d == 0x0A || d == 0x20)) ||
				(!con_wordwrap.value && txt[l] <= ' ')
			)
				break;
		}

		// word wrap
		if (l != con_linewidth && con.x + l > con_linewidth)
			con.x = 0;

		txt++;

		if (cr) {
			con.current--;
			cr = false;
		}

		if (!con.x) {
			Con_Linefeed ();
		// mark time for transparent overlay
			if (con.current >= 0)
				con_times[con.current % NUM_CON_TIMES] = cls.realtime;
		}

		switch (c) {
			case '\n':
				con.x = 0;
				break;

			case '\r':
				con.x = 0;
				cr = 1;
				break;

			default:	// display character and advance
				y = con.current % con_totallines;
				con.text[y * con_linewidth+con.x] = c | mask | con_ormask;
				con.x++;
				if (con.x >= con_linewidth)
					con.x = 0;
				break;
		}
	}
}


/*
==============================================================================
DRAWING
==============================================================================
*/

//The input line scrolls horizontally if typing goes beyond the right edge
void Con_DrawInput (void) {
	int i;
	char *text, temp[MAXCMDLINE];

	if (key_dest != key_console && cls.state == ca_active)
		return;		// don't draw anything (always draw if not active)

	text = strcpy (temp, key_lines[edit_line]);

	// fill out remainder with spaces
	for (i = strlen(text); i < MAXCMDLINE; i++)
		text[i] = ' ';

	// add the cursor frame
	if ( (int)(curtime*con_cursorspeed) & 1 )
		text[key_linepos] = 11;

	//	prestep if horizontally scrolling
	if (key_linepos >= con_linewidth)
		text += 1 + key_linepos - con_linewidth;

	// draw it
	Draw_String(8, con_vislines-22, text);
}

//Draws the last few lines of output transparently over the game top
void Con_DrawNotify (void) {
	int x, v, skip, maxlines, i;
	char *text, *s;
	float time;

	maxlines = _con_notifylines.value;
	if (maxlines > NUM_CON_TIMES)
		maxlines = NUM_CON_TIMES;
	if (maxlines < 0)
		maxlines = 0;

	v = 0;
	for (i = con.current-maxlines + 1; i <= con.current; i++) {
		if (i < 0)
			continue;
		time = con_times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;
		time = cls.realtime - time;
		if (time > con_notifytime.value)
			continue;
		text = con.text + (i % con_totallines)*con_linewidth;

		clearnotify = 0;
		scr_copytop = 1;

		for (x = 0 ; x < con_linewidth ; x++)
			Draw_Character ( (x+1)<<3, v, text[x]);

		v += 8;
	}


	if (key_dest == key_message) {
		char temp[MAXCMDLINE + 1];

		clearnotify = 0;
		scr_copytop = 1;

		if (chat_team) {
			Draw_String (8, v, "say_team:");
			skip = 11;
		} else {
			Draw_String (8, v, "say:");
			skip = 5;
		}

		// FIXME: clean this up
		s = strcpy (temp, chat_buffer);

		// add the cursor frame
		if ((int) (curtime * con_cursorspeed) & 1) {
			if (chat_linepos == strlen(s))
				s[chat_linepos+1] = '\0';
			s[chat_linepos] = 11;
		}

		// prestep if horizontally scrolling
		if (chat_linepos + skip >= (vid.width >> 3))
			s += 1 + chat_linepos + skip - (vid.width >> 3);

		x = 0;
		while (s[x] && x+skip < (vid.width>>3)) {
			Draw_Character ( (x+skip)<<3, v, s[x]);
			x++;
		}
		v += 8;
	}

	if (v > con_notifylines)
		con_notifylines = v;
}

//Draws the console with the solid background
void Con_DrawConsole (int lines) {
	int i, j, x, y, n, rows, row;
	char *text, dlbar[1024];
#ifdef GLQUAKE
	extern cvar_t gl_conalpha;
#endif

	if (lines <= 0)
		return;

// draw the background
	Draw_ConsoleBackground (lines);

	// draw the text
	con_vislines = lines;

	// changed to line things up better
	rows = (lines - 22) >> 3;		// rows of text to draw

	y = lines - 30;

	row = con.display;

	// draw from the bottom up
	if (con.display != con.current) {
	// draw arrows to show the buffer is backscrolled
		for (x = 0; x < con_linewidth; x += 4)
			Draw_Character ((x + 1) << 3, y, '^');

		y -= 8;
		rows--;
		row--;
	}

	for (i = 0; i < rows; i++, y -= 8, row--) {
		if (row < 0)
			break;
		if (con.current - row >= con_totallines)
			break;		// past scrollback wrap point

		text = con.text + (row % con_totallines)*con_linewidth;

		for (x = 0; x < con_linewidth; x++)
			Draw_Character ((x + 1) << 3, y, text[x]);
	}

	// draw the download bar
	// figure out width
	if (cls.download) {
		if ((text = strrchr(cls.downloadname, '/')) != NULL)
			text++;
		else
			text = cls.downloadname;

		x = con_linewidth - ((con_linewidth * 7) / 40);
		y = x - strlen(text) - 8;
		i = con_linewidth/3;
		if (strlen(text) > i) {
			y = x - i - 11;
			Q_strncpyz (dlbar, text, i+1);
			strcat(dlbar, "...");
		} else {
			strcpy(dlbar, text);
		}
		strcat(dlbar, ": ");
		i = strlen(dlbar);
		dlbar[i++] = '\x80';
		// where's the dot go?
		if (cls.downloadpercent == 0)
			n = 0;
		else
			n = y * cls.downloadpercent / 100;

		for (j = 0; j < y; j++)
			if (j == n)
				dlbar[i++] = '\x83';
			else
				dlbar[i++] = '\x81';
		dlbar[i++] = '\x82';
		dlbar[i] = 0;

		sprintf(dlbar + strlen(dlbar), " %02d%%", cls.downloadpercent);

		// draw it
		y = con_vislines - 22 + 8;
		Draw_String (8, y, dlbar);
	}

	// draw the input prompt, user text, and cursor if desired
	Con_DrawInput ();
}
