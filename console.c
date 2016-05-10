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

	$Id: console.c,v 1.63 2007-10-04 13:48:11 dkure Exp $
*/
// console.c

#include "quakedef.h"
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#else
#include <stdio.h>
#endif
#include "gl_model.h"
#include "gl_local.h"
#if defined(_WIN32) || defined(__linux__) || defined(__FreeBSD__)
#include "tr_types.h"
#endif // _WIN32 || __linux__ || __FreeBSD__
#include "keys.h"
#include "ignore.h"
#include "logging.h"
#include "utils.h"
#include "localtime.h"
#include "irc.h"


#define		MINIMUM_CONBUFSIZE	(1 << 15)
#define		DEFAULT_CONBUFSIZE	(1 << 16)
#define		MAXIMUM_CONBUFSIZE	(1 << 22)

console_t	con;
int         con_margin=0;       // kazik: margin on the left side

qbool    con_addtimestamp;

int			con_ormask;
int 		con_linewidth;		// characters across screen
int			con_totallines;		// total lines in console scrollback
float		con_cursorspeed = 4;

cvar_t		con_notify = {"con_notify", "1"};
cvar_t		_con_notifylines = {"con_notifylines","4"};
cvar_t		con_notifytime = {"con_notifytime","3"};		//seconds
cvar_t		con_wordwrap = {"con_wordwrap","1"};
cvar_t		con_clearnotify = {"con_clearnotify","1"};
//cvar_t	    xyzh                = {"x", "$x", CVAR_ROM};

cvar_t		con_highlight  		= {"con_highlight","0"};
cvar_t		con_highlight_mark 	= {"con_highlight_mark",""};

cvar_t      con_sound_mm1_file      = {"s_mm1_file",      "misc/talk.wav"};
cvar_t      con_sound_mm2_file      = {"s_mm2_file",      "misc/talk.wav"};
cvar_t      con_sound_spec_file     = {"s_spec_file",     "misc/talk.wav"};
cvar_t      con_sound_other_file    = {"s_otherchat_file",    "misc/talk.wav"};
cvar_t      con_sound_mm1_volume    = {"s_mm1_volume",    "1"};
cvar_t      con_sound_mm2_volume    = {"s_mm2_volume",    "1"};
cvar_t      con_sound_spec_volume   = {"s_spec_volume",   "1"};
cvar_t      con_sound_other_volume  = {"s_otherchat_volume",  "1"};

cvar_t      con_timestamps          = {"con_timestamps", "0"};
cvar_t      con_shift               = {"con_shift", "-10"};
cvar_t      cl_textEncoding         = {"cl_textencoding", "0"};

#define	NUM_CON_TIMES 16
float		con_times[NUM_CON_TIMES];	// cls.realtime time the line was generated
										// for transparent notify lines

int			con_vislines;
int			con_notifylines;			// scan lines to clear for notify lines

qbool	con_initialized = false;
qbool	con_suppress = false;

FILE		*qconsole_log = NULL;


char *months[12] = {
    "jan",
    "feb",
    "mar",
    "apr",
    "may",
    "jun",
    "jul",
    "aug",
    "sep",
    "oct",
    "nov",
    "dec" };

char *days[7] = {
    "Sun",
    "Mon",
    "Tue",
    "Wed",
    "Thu",
    "Fri",
    "Sat"}; 

void Date_f (void)
{
    // Sun  Nov 05 2000, 16:39
    SYSTEMTIME tm;
    char *day, *month;
    GetLocalTime(&tm);
    month = months[tm.wMonth-1];
    day = days[tm.wDayOfWeek];

    con_ormask = 128;
    Com_Printf ("%s %s %02d, %2d:%02d %d\n", day, month, tm.wDay, tm.wHour, tm.wMinute, tm.wYear);
    con_ormask = 0;
}

void MakeStringRed(char *s)
{
    while (*s)
    {
        if (*s >= '0'  &&  *s <= '9')
            *s |= 128;
        s++;
    }
}

void MakeStringYellow(char *s)
{
    while (*s)
    {
        if (*s >= '0'  &&  *s <= '9')
            *s -= 30;
        s++;
    }
}

/*
==================
Calendar_f - prints calendar
==================
*/
void Calendar_f(void)
{
    int alldays, day, column;
    SYSTEMTIME tm;
    GetLocalTime(&tm);

    Date_f();
    Com_Printf (" mo tu we th fr sa su\n");

    alldays = 31;
    day = tm.wDay;
    day -= (tm.wDayOfWeek==0 ? 7 : tm.wDayOfWeek) - 1;
    day = (day % 7);
    if (day > 1)
        day -= 7;
    column = 0;

    switch (tm.wMonth)
    {
    case  1:    alldays = 31; break;
	case  2:    alldays = (tm.wYear % 400 ? 28 : (tm.wYear % 100 ? 29 : (tm.wYear % 4) ? 28 : 29)); break;
    case  3:    alldays = 31; break;
    case  4:    alldays = 30; break;
    case  5:    alldays = 31; break;
    case  6:    alldays = 30; break;
    case  7:    alldays = 31; break;
    case  8:    alldays = 31; break;
    case  9:    alldays = 30; break;
    case 10:    alldays = 31; break;
    case 11:    alldays = 30; break;
    case 12:    alldays = 31; break;
    default:    alldays = 31; break;
    }

    while (day <= alldays)
    {
        column ++;

        if (day > 0)
        {
            char buf[8];
            snprintf(buf, sizeof (buf), "%3d", day);
            if (day == tm.wDay)
                ; // MakeStringYellow(buf);
            else
            {
                if (column == 7)
                    MakeStringYellow(buf);
                else
                    MakeStringRed(buf);
            }
            Com_Printf ("%s", buf);
        }
        else
            Com_Printf ("   ");


        if (day == alldays  ||  column == 7)
        {
            column = 0;
            Com_Printf ("\n");
        }

        day++;
    }
}

void Con_ToggleConsole_f (void) {
	Key_ClearTyping ();

	if (key_dest == key_console) {
		if (!SCR_NEED_CONSOLE_BACKGROUND)
			key_dest = key_dest_beforecon;
	} else {
		key_dest_beforecon = key_dest;
		key_dest = key_console;
	}

	if (con_clearnotify.value)
		Con_ClearNotify ();
}

void Con_SetColor(int idx_from, int count, int c) {
	int i;

	if (idx_from < 0 || idx_from >= con.maxsize || count < 0 || idx_from + count > con.maxsize)
		Sys_Error("Con_SetColor: wrong idx");;

	for (i = idx_from; i < count; i++) {
		memset(&con.clr[i], 0, sizeof(clrinfo_t)); // zeroing whole struct
		con.clr[i].c = c;
		con.clr[i].i = i;
	}
}

void Con_SetWhite (void) {
// no need for memset(), Con_SetColor() do it too
//	memset (con.clr, 0, con.maxsize * sizeof(clrinfo_t)); // set whole struct array to zero

	Con_SetColor(0, con.maxsize, COLOR_WHITE); // set white color
}

void Con_Clear_f (void) {
	int	i;

	con.numlines = 0;
	for (i = 0; i < con.maxsize; i++)
		con.text[i] = ' ';
	con.display = con.current;
	Con_SetWhite(); // set default color to white
}

void Con_ClearNotify (void) {
	int i;

	for (i = 0; i < NUM_CON_TIMES; i++)
		con_times[i] = 0;
}

void Con_MessageMode_Common (chat_type t) {
	if (cls.state != ca_active)
		return;

	chat_team = t;
	key_dest_beforemm = key_dest; // where to return after we finish typing
	key_dest = key_message;
	chat_buffer[0] = 0;
	chat_linepos = 0;
}

void Con_MessageMode_f (void) {
	Con_MessageMode_Common(chat_mm1);
}

void Con_MessageMode2_f (void) {
	if (cls.mvdplayback == QTV_PLAYBACK)
	{
		// mm2 has no meaning in QTV so let's save some keys and make it QTV->game chat
		Con_MessageMode_Common(chat_qtvtogame);
	}
	else
	{
		Con_MessageMode_Common(chat_mm2);
	}
}

#ifdef WITH_IRC
void Con_MessageModeIRC_f (void) {
	Con_MessageMode_Common(chat_irc);
}
#endif

void Con_MessageModeQTVtoGAME_f (void) {
	Con_MessageMode_Common(chat_qtvtogame);
}

//If the line width has changed, reformat the buffer
void Con_CheckResize (void) {
	int i, j, width, oldwidth, oldtotallines, numlines, numchars;
	wchar *tempbuf;

	width = (vid.width >> 3) - 2;

	if (width == con_linewidth)
		return;

	if (width < 1) { // video hasn't been initialized yet
#if defined(_WIN32) || defined(__linux__) || defined(__FreeBSD__)
		cvar_t *cv = Cvar_Find(r_conwidth.name); // r_conwidth not yet registered, but let user specifie it via
													 // config.cfg or somehow else
		if ( cv ) {
			width = max(320, cv->integer);
			width &= 0xfff8; // make it a multiple of eight
			width = (width >> 3) - 2;
			width = max(width, 38);
		}
		else
#endif
			width = 38;

		con_linewidth = width;
		con_totallines = con.maxsize / con_linewidth;
		for (i = 0; i < con.maxsize; i++)
			con.text[i] = ' ';
		Con_SetWhite();
	} else {
		int idx_old, idx_new;
		clrinfo_t *clr;

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

		tempbuf = (wchar *) Hunk_TempAlloc(con.maxsize * sizeof(wchar));
		memcpy (tempbuf, con.text, con.maxsize * sizeof(wchar));
		for (i = 0; i < con.maxsize; i++)
			con.text[i] = ' ';

		clr = Q_malloc(con.maxsize * sizeof(clrinfo_t)); // alloc temporaly
		memcpy(clr, con.clr, con.maxsize * sizeof(clrinfo_t)); // save color array
		Con_SetWhite(); // wipe color array entirely

		for (i = 0; i < numlines; i++) {
			for (j = 0; j < numchars; j++) {
				idx_new = (con_totallines - 1 - i) * con_linewidth + j;
				idx_old = ((con.current - i + oldtotallines) % oldtotallines) * oldwidth + j;
				con.text[idx_new] = tempbuf[idx_old];

				con.clr[idx_new]   = clr[idx_old]; // restore color info
				con.clr[idx_new].i = idx_new; // re-index
			}
		}

		Con_ClearNotify ();
		Q_free(clr); // free
	}

	con.current = con_totallines - 1;
	con.display = con.current;
}


char readableChars[256] = {	'.', '_' , '_' , '_' , '_' , '.' , '_' , '_' , '_' , '_' , '\n' , '_' , '\n' , '>' , '.' , '.',
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
	con.text = (wchar *) Hunk_AllocName(con.maxsize * sizeof(wchar), "console_buffer");
	con.clr = (clrinfo_t *) Hunk_AllocName(con.maxsize * sizeof(clrinfo_t), "console_clr");
}

void Con_Init (void) {
	int i, conbufsize;

	//Tei: moved there, because on windows can't capture the output for debug purposes
	if (!qconsole_log && COM_CheckParm("-condebug")) {
		char tmp_path[MAX_OSPATH] = {0};

		snprintf(&tmp_path[0], sizeof(tmp_path), "%s/qw/qconsole.log", com_basedir);
		qconsole_log = fopen(tmp_path, "a");
	}

	if ((i = COM_CheckParm("-conbufsize")) && i + 1 < COM_Argc()) {
		conbufsize = Q_atoi(COM_Argv(i + 1)) << 10;
		conbufsize = bound (MINIMUM_CONBUFSIZE , conbufsize, MAXIMUM_CONBUFSIZE);
	} else {
		conbufsize = DEFAULT_CONBUFSIZE;
	}
	Con_InitConsoleBuffer(&con, conbufsize);

	con_linewidth = -1;
	Con_CheckResize ();

	Con_CreateReadableChars();

	con_initialized = true;
	Com_Printf_State (PRINT_OK, "Console initialized\n");

	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	// register our commands and cvars
	Cvar_Register (&_con_notifylines);
	Cvar_Register (&con_notifytime);
	Cvar_Register (&con_wordwrap);
	Cvar_Register (&con_clearnotify);
	Cvar_Register (&con_notify);
	//Cvar_Register (&xyzh);

	// added by jogi start
	Cvar_Register (&con_highlight);
	Cvar_Register (&con_highlight_mark);
	// added by jogi stop

	Cvar_Register (&con_timestamps); 
	Cvar_Register (&con_shift); 

	Cvar_ResetCurrentGroup();

    Cvar_SetCurrentGroup(CVAR_GROUP_SOUND);

	Cvar_Register (&con_sound_mm1_file); 
	Cvar_Register (&con_sound_mm2_file); 
	Cvar_Register (&con_sound_spec_file); 
	Cvar_Register (&con_sound_other_file); 
	Cvar_Register (&con_sound_mm1_volume); 
	Cvar_Register (&con_sound_mm2_volume); 
	Cvar_Register (&con_sound_spec_volume); 
	Cvar_Register (&con_sound_other_volume); 
	// if you don't like renaming things in this way, let's have some talk with tea - johnnycz
	Cmd_AddLegacyCommand("con_sound_mm1_file", "s_mm1_file");
	Cmd_AddLegacyCommand("con_sound_mm2_file", "s_mm2_file");
	Cmd_AddLegacyCommand("con_sound_spec_file", "s_spec_file");
	Cmd_AddLegacyCommand("con_sound_other_file", "s_otherchat_file");
	Cmd_AddLegacyCommand("con_sound_mm1_volume", "s_mm1_volume");
	Cmd_AddLegacyCommand("con_sound_mm2_volume", "s_mm2_volume");
	Cmd_AddLegacyCommand("con_sound_spec_volume", "s_spec_volume");
	Cmd_AddLegacyCommand("con_sound_other_volume", "s_otherchat_volume");

    Cvar_ResetCurrentGroup();

	Cvar_Register(&cl_textEncoding);

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
#ifdef WITH_IRC
	Cmd_AddCommand ("messagemodeirc", Con_MessageModeIRC_f);
#endif
	Cmd_AddCommand ("messagemodeqtvtogame", Con_MessageModeQTVtoGAME_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
    Cmd_AddCommand ("date", Date_f);
	Cmd_AddCommand ("calendar", Calendar_f);
  
}

void Con_Shutdown (void) {
	if (qconsole_log)
		fclose(qconsole_log);
}

void Con_Linefeed (void) {
	int idx, i;
	con.x = 0;
	con.x = con_margin;    // kazik
	if (con.display == con.current)
		con.display++;
	con.current++;
	if (con.numlines < con_totallines)
		con.numlines++;
	idx = (con.current%con_totallines)*con_linewidth;
	for (i = 0; i < con_linewidth; i++)
		con.text[idx + i] = ' ';
	Con_SetColor(idx, con_linewidth, COLOR_WHITE);

	// mark time for transparent overlay
	if (con.current >= 0) {
		con_times[con.current % NUM_CON_TIMES] = (Print_flags[Print_current] & PR_NONOTIFY ? 0 : cls.realtime);
	}
}

/*
==================
Con_SafePrintf

Okay to call even when the screen can't be updated

==================
*/
void Con_SafePrintf (char *fmt, ...)
{
    va_list     argptr;
    char        msg[1024];
    int         temp;

    va_start (argptr,fmt);
    vsnprintf (msg,sizeof(msg),fmt,argptr);
    va_end (argptr);

    temp = scr_disabled_for_loading;
    scr_disabled_for_loading = true;
    Com_Printf ("%s", msg);
    scr_disabled_for_loading = temp;
}

//Handles cursor positioning, line wrapping, etc
void Con_PrintW (wchar *txt) {
	int y, c, l, d, mask, color = COLOR_WHITE, r, g, b, idx;
	wchar *s;
	static int cr;

	if (!(Print_flags[Print_current] & PR_LOG_SKIP)) {
		if (qconsole_log) {
			char *tempbuf = wcs2str_malloc(txt);
			fprintf(qconsole_log, "%s", tempbuf);
			fflush(qconsole_log);
			Q_free(tempbuf);
		}
		if (Log_IsLogging()) {
			if (log_readable.value) {
				char *s, *tempbuf = wcs2str_malloc(txt);
				for (s = tempbuf; *s; s++)
					*s = readableChars[(unsigned char) *s];
				Log_Write(tempbuf);
				Q_free(tempbuf);
			} else {
				Log_Write(wcs2str(txt));	
			}
		}
	}

	if ((Print_flags[Print_current] & PR_SKIP))
		goto zomfg;

	if (!con_initialized || con_suppress)
		goto zomfg;

	if (txt[0] == 1 || txt[0] == 2)	{
		mask = 128;		// go to colored text
		txt++;
	} else {
		mask = 0;
	}

	while ((c = *txt)) {
		// get color modificator if any
		if (*txt == '&') {
			if (txt[1] == 'c' && txt[2] && txt[3] && txt[4]) {
				r = HexToInt(txt[2]);
				g = HexToInt(txt[3]);
				b = HexToInt(txt[4]);
				if (r >= 0 && g >= 0 && b >= 0) {
					color = RGBA_TO_COLOR(255 * r / 16, 255 * g / 16, 255 * b / 16, 255);
					txt += 5;
					continue; // we got color, get now normal char
				}
            } else if (txt[1] == 'r') {
				color = COLOR_WHITE;
				txt += 2;
				continue; // we got color, get now normal char
			}
		}

		// count word length
		for (s = txt, l = 0; s[0] && l < con_linewidth;) {
			// skip color, not count in word length
			if (*s == '&') {
				if (s[1] == 'c' && s[2] && s[3] && s[4]) {
					r = HexToInt(s[2]);
					g = HexToInt(s[3]);
					b = HexToInt(s[4]);
					if (r >= 0 && g >= 0 && b >= 0) {
						s += 5;
						continue; // we got color, get now normal char
					}
                } else if (s[1] == 'r') {
					s += 2;
					continue; // we got color, get now normal char
				}
			}
		
			d = (s[0] & ~128);
			if (   ( con_wordwrap.value && (!d || d == 0x09 || d == 0x0D || d == 0x0A || d == 0x20))
				|| (!con_wordwrap.value && d <= 32) // 32 is a space as well as 0x20
			   )
				break;

			l++; // increase word length
			s++; // get next char
		}

		// word wrap
		if (l != con_linewidth && con.x + l > con_linewidth)
			con.x = 0;

		txt++;

		if (cr) {
			con.current--;
			cr = false;
		}

		if (!con.x)
			Con_Linefeed ();

		switch (c) {
			case '\n':
				con.x = 0;
				break;

			case '\r':
				con.x = 0;
				cr = 1;
				break;

			default:	// display character and advance
				if (con.x >= con_linewidth)
					Con_Linefeed ();
				y = con.current % con_totallines;
				idx = y * con_linewidth + con.x;
				con.text[idx] = c | (c <= 0x7F ? mask | con_ormask : 0);	// only apply mask if in 'standard' charset
				memset(&con.clr[idx], 0, sizeof(clrinfo_t)); // zeroing whole struct
				con.clr[idx].c = color;
				con.clr[idx].i = idx; // no, that not stupid :P
				con.x++;
				break;
		}
	}
zomfg:
	Print_flags[Print_current] = 0;
}

/*
==============================================================================
DRAWING
==============================================================================
*/

//The input line scrolls horizontally if typing goes beyond the right edge
static void Con_DrawInput(void) {
	int		len, i;
	wchar	*text;
	wchar	temp[MAXCMDLINE + 1];       //+ 1 for cursor if stlen(key_lines[edit_line]) == 255

	if (key_dest != key_console && cls.state == ca_active)
		return;

	qwcslcpy (temp, key_lines[edit_line], MAXCMDLINE);
	len = qwcslen(temp);

	text = temp;

	// fill out remainder with spaces
	for (i = 0; i < MAXCMDLINE - len; i++)
		(text + len)[i] = ' ';
	
	text[MAXCMDLINE] = 0;

	// add the cursor frame
	if ( (int)(curtime*con_cursorspeed) & 1 )
		text[key_linepos] = con_redchars ? (11 + 128) : 11;

	//	prestep if horizontally scrolling
	if (key_linepos >= con_linewidth)
		text += 1 + key_linepos - con_linewidth;

	Draw_StringW (8, con_vislines-22 + bound(0, con_shift.value, 8), text);
}

// Returns first line to start printing from in order to fill up notify area
static int Con_FirstNotifyLine (int notification_lines, float notification_timelimit)
{
	int maxlines = bound(0, notification_lines, NUM_CON_TIMES - 1);
	int first_line = con.current;
	float threshold_time = cls.realtime - notification_timelimit;

	// Work backwards to skip non-ignored lines
	while (first_line >= 0 && first_line > (con.current - NUM_CON_TIMES + 1) && maxlines) {
		float line_time = con_times[first_line % NUM_CON_TIMES];
		if (line_time && line_time > threshold_time) {
			--maxlines;
		}
		if (maxlines)
			--first_line;
	}

	return first_line;
}

//Draws the last few lines of output transparently over the game top
void Con_DrawNotify (void) {
	int x, v, skip = 0, i, idx;
	wchar *text, *s;
	wchar buf[1024];
	clrinfo_t clr[sizeof(buf)];
	float time;
	int first_line = Con_FirstNotifyLine(_con_notifylines.integer, con_notifytime.value);

	if (!con_notify.value)
		return;

	v = 0;
	if (_con_notifylines.integer) {
		for (i = first_line; i <= con.current; i++) {
			if (i < 0)
				continue;
			time = con_times[i % NUM_CON_TIMES];
			if (time == 0)
				continue;
			time = cls.realtime - time;
			if (time > con_notifytime.value)
				continue;
			idx = (i % con_totallines)*con_linewidth;
			text = con.text + idx;

			clearnotify = 0;
			scr_copytop = 1;

			// copy current line to buffer
			for (x = 0; x < con_linewidth; x++) {
				buf[x] = text[x];
				clr[x] = con.clr[idx + x]; // copy whole color struct
				clr[x].i = x; // set proper index
			}
			buf[x] = '\0';
			Draw_ColoredString3W(8, v + bound(0, con_shift.value, 8), buf, clr, con_linewidth, 0);
			v += 8;
		}
	}

	if (key_dest == key_message) {
		wchar temp[MAXCMDLINE + 1];

		clearnotify = 0;
		scr_copytop = 1;

		if (chat_team == chat_mm2) {
			Draw_String (8, v + bound(0, con_shift.value, 8), "say_team:");
			skip = 11;
		}
		else if (chat_team == chat_mm1) {
			Draw_String (8, v + bound(0, con_shift.value, 8), "say:");
			skip = 5;
		}
#ifdef WITH_IRC
		else if (chat_team == chat_irc) {
			char dest[256];
			
			strlcpy(dest, IRC_GetCurrentChan(), sizeof(dest));
			strlcat(dest, ":", sizeof(dest));
			skip = strlen(dest) + 1; // is this correct? not sure

			Draw_String (8, v + bound(0, con_shift.value, 8), dest);
		}
#endif
		else if (chat_team == chat_qtvtogame) {
			Draw_String (8, v + bound(0, con_shift.value, 8), "say_game:");
			skip = 11;
		}

		// FIXME: clean this up
		qwcslcpy (temp, chat_buffer, sizeof(temp) / sizeof(wchar));
		s = temp;

		// add the cursor frame
		if ((int) (curtime * con_cursorspeed) & 1) {
			if (chat_linepos == (int) qwcslen(s))
				s[chat_linepos+1] = '\0';
			s[chat_linepos] = 11;
		}

		// prestep if horizontally scrolling
		if (chat_linepos + skip >= (vid.width >> 3))
			s += 1 + chat_linepos + skip - (vid.width >> 3);

		x = 0;
		while (s[x] && x+skip < (vid.width>>3)) {
			Draw_CharacterW ( (x+skip)<<3, v + bound(0, con_shift.value, 8), s[x]);
			x++;
		}
		v += 8;
	}

	if (v > con_notifylines)
		con_notifylines = v + bound(0, con_shift.value, 8);
}

// Draws the last few lines of output as a custom HUD element.
void SCR_DrawNotify(int posX, int posY, float scale, int notifyTime, int notifyLines, int notifyCols)
{
	int v, skip, i, j, k, idx, draw, offset;
	wchar *text, *s;
	wchar buf[1024];
	clrinfo_t clr[sizeof(buf)];
	float time;
	int first_line = Con_FirstNotifyLine(notifyLines, notifyTime);

	if (notifyCols > (con_linewidth))
		notifyCols = con_linewidth;

	if (notifyCols < 10)
		notifyCols = 10;

	v = 0;
	if (notifyLines) {
		for (i = first_line; i <= con.current; i++) {
			if (i < 0)
				continue;

			time = con_times[i % NUM_CON_TIMES];
			if (time == 0)
				continue;

			time = cls.realtime - time;
			if (time > notifyTime)
				continue;

			idx = (i % con_totallines)*con_linewidth;
			text = con.text + idx;

			clearnotify = 0;
			scr_copytop = 1;

			// Copy current line to buffer
			offset = 0;
			draw = 0;
			for (j = 0; j < con_linewidth; j++)
			{
				// each new line of a notify hud element
				if ((j % notifyCols) == 0 && j != 0)
				{
					for (k = 0; k < (j - offset); ++k)
					{
						if (buf[k] != ' ')
						{
							draw = 1;
							break;
						}
					}

					buf[j - offset] = '\0';
					offset = j;
				}
				else if (j == (con_linewidth - 1)) // Ending of the string.
				{
					for (k = 0; k < (j - offset); ++k)
					{
						if (buf[k] != ' ')
						{
							draw = 1;
							break;
						}
					}

					buf[j - offset] = '\0';
				}

				// Output.
				if (draw)
				{
					if (!draw)
						continue;

					Draw_SColoredString(
						posX,
						v + posY,
						buf,
						clr,
						notifyCols,
						0,
						scale
						);

					// move text down
					v += (8 * scale);

					if (v > (notifyLines * scale))
						notifyLines = v;

					draw = 0;
				}

				buf[j - offset] = text[j];
				clr[j - offset] = con.clr[idx + j]; // copy whole color struct
				clr[j - offset].i = j; // set proper index
			}
		}
	}

	if (key_dest == key_message)
	{
		wchar temp[MAXCMDLINE + 1];

		clearnotify = 0;
		scr_copytop = 1;

		if (chat_team)
		{
			Draw_SString (posX, v + posY, "say_team: ", scale);
			skip = 10;
		}
		else
		{
			Draw_SString (posX, v + posY, "say: ", scale);
			skip = 5;
		}

		// FIXME: clean this up
		qwcslcpy (temp, chat_buffer, sizeof(temp) / sizeof(wchar));
		s = temp;

		// Add the cursor frame.
		if ((int) (curtime * con_cursorspeed) & 1)
		{
			if (chat_linepos == (int) qwcslen(s))
				s[chat_linepos+1] = '\0';
			s[chat_linepos] = 11;
		}


		if (chat_linepos + skip >= notifyCols)
			s += 1 + chat_linepos + skip - notifyCols;

		j = 0;

		while (s[j] && (j + skip < notifyCols))
		{
			Draw_SCharacter (
				posX + (j + skip) * 8 * scale,
				v + posY,
				s[j],
				scale
				);

			j++;
		}

		v += (8 * scale);
		if (v > con_notifylines)
			con_notifylines = v + bound(0, con_shift.value, 8);
	}
}

//Draws the console with the solid background
void Con_DrawConsole (int lines) {
	int i, j, x, y, n=0, rows, row, idx;
	wchar *wtext;
	char *text, dlbar[1024];
	wchar buf[1024];
	clrinfo_t clr[sizeof(buf)];

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
			Draw_Character ((x + 1) << 3, y + bound(0, con_shift.value, 8), '^');

		y -= 8;
		rows--;
		row--;
	}

	for (i = 0; i < rows; i++, y -= 8, row--) {
		if (row < 0)
			break;
		if (con.current - row >= con_totallines)
			break;		// past scrollback wrap point

		idx = (row % con_totallines)*con_linewidth;
		wtext = con.text + idx;

		// copy current line to buffer
		for(x = 0; x < con_linewidth; x++) {
			buf[x] = wtext[x];
			clr[x] = con.clr[idx + x]; // copy whole color struct
			clr[x].i = x; // set proper index
		}
		buf[x] = '\0';

		Draw_ColoredString3W( 1 << 3, y + bound(0, con_shift.value, 8), buf, clr, con_linewidth, 0);
	}

	// draw the download bar
	// figure out width
	if (cls.download || cls.upload) {
		int	slider_box_position = -1;

		if (cls.download) {
			if ((text = strrchr(cls.downloadname, '/')) != NULL)
				text++;
			else
				text = cls.downloadname;
		} else if (cls.upload) {
			if ((text = strrchr(cls.uploadname, '/')) != NULL)
				text++;
			else
				text = cls.uploadname;
		} else
			return;

		x = con_linewidth - ((con_linewidth * 7) / 40);
		y = x - strlen(text) - 8;
		i = con_linewidth/3;
		if (strlen(text) > i) {
			y = x - i - 11;
			strlcpy (dlbar, text, i);
			strlcat (dlbar, "...", sizeof (dlbar));
		} else {
			strlcpy (dlbar, text, sizeof (dlbar));
		}
		strlcat (dlbar, ": ", sizeof (dlbar));
		i = strlen (dlbar);
		dlbar[i++] = '\x80';
		// where's the dot go?
		if (cls.download) {
			if (cls.downloadpercent == 0)
				n = 0;
			else
				n = y * cls.downloadpercent / 100;
		} else if (cls.upload) {
			if (cls.uploadpercent == 0)
				n = 0;
		else
			n = y * cls.uploadpercent / 100;
		}

		for (j = 0; j < y; j++)
		{
			if (j == n)
				slider_box_position = i;

			dlbar[i++] = '\x81';
		}
		if (slider_box_position >= 0)
			dlbar[slider_box_position] = '\x83';
		dlbar[i++] = '\x82';
		dlbar[i] = 0;

		i = strlen (dlbar);
		if (cls.download)
			snprintf (dlbar + i, sizeof (dlbar), " %02d%%(%dkb/s)", cls.downloadpercent, cls.downloadrate);
		else if (cls.upload)
			snprintf (dlbar + i, sizeof (dlbar), " %02d%%(%dkb/s)", cls.uploadpercent, cls.uploadrate);
		else
			return;

		// draw it
		y = con_vislines - 22 + 8;
		Draw_String (8, y + bound(0, con_shift.value, 8), dlbar);
	}

	// draw the input prompt, user text, and cursor if desired
	Con_DrawInput ();
}

