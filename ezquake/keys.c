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

#include "quakedef.h"
#include "keys.h"
#include "menu.h"
#ifdef _WINDOWS
#include <windows.h>
#endif

//key up events are sent even if in console mode

cvar_t	cl_chatmode = {"cl_chatmode", "2"};

#define		MAXCMDLINE	256
char	key_lines[32][MAXCMDLINE];
int		key_linepos;
int		key_lastpress;

int		edit_line=0;
int		history_line=0;

keydest_t	key_dest;

char		*keybindings[256];
qboolean	consolekeys[256];	// if true, can't be rebound while in console
qboolean	menubound[256];		// if true, can't be rebound while in menu
int			keyshift[256];		// key to map to if shift held down in console
int			key_repeats[256];	// if > 1, it is autorepeating
qboolean	keydown[256];
qboolean	keyactive[256];	

typedef struct {
	char	*name;
	int		keynum;
} keyname_t;

keyname_t keynames[] = {
	{"TAB", K_TAB},
	{"ENTER", K_ENTER},
	{"ESCAPE", K_ESCAPE},
	{"SPACE", K_SPACE},
	{"BACKSPACE", K_BACKSPACE},

	{"CAPSLOCK",K_CAPSLOCK},
	{"PRINTSCR", K_PRINTSCR},
	{"SCRLCK", K_SCRLCK},
	{"SCROLLOCK", K_SCRLCK},	// FIXME
	{"PAUSE", K_PAUSE},

	{"UPARROW", K_UPARROW},
	{"DOWNARROW", K_DOWNARROW},
	{"LEFTARROW", K_LEFTARROW},
	{"RIGHTARROW", K_RIGHTARROW},

	{"ALT", K_ALT},
	{"LALT", K_LALT},
	{"RALT", K_RALT},
	{"CTRL", K_CTRL},
	{"LCTRL", K_LCTRL},
	{"RCTRL", K_RCTRL},
	{"SHIFT", K_SHIFT},
	{"LSHIFT", K_LSHIFT},
	{"RSHIFT", K_RSHIFT},

	{"WINKEY", K_WIN},
	{"LWINKEY", K_LWIN},
	{"RWINKEY", K_RWIN},
	{"POPUPMENU", K_MENU},
	
	// Keypad stuff..

	{"NUMLOCK", KP_NUMLOCK},
	{"KP_NUMLCK", KP_NUMLOCK},
	{"KP_NUMLOCK", KP_NUMLOCK},
	{"KP_SLASH", KP_SLASH},
	{"KP_DIVIDE", KP_SLASH},
	{"KP_STAR", KP_STAR},
	{"KP_MULTIPLY", KP_STAR},

	{"KP_MINUS", KP_MINUS},

	{"KP_HOME", KP_HOME},
	{"KP_7", KP_HOME},
	{"KP_UPARROW", KP_UPARROW},
	{"KP_8", KP_UPARROW},
	{"KP_PGUP", KP_PGUP},
	{"KP_9", KP_PGUP},
	{"KP_PLUS", KP_PLUS},

	{"KP_LEFTARROW", KP_LEFTARROW},
	{"KP_4", KP_LEFTARROW},
	{"KP_5", KP_5},
	{"KP_RIGHTARROW", KP_RIGHTARROW},
	{"KP_6", KP_RIGHTARROW},

	{"KP_END", KP_END},
	{"KP_1", KP_END},
	{"KP_DOWNARROW", KP_DOWNARROW},
	{"KP_2", KP_DOWNARROW},
	{"KP_PGDN", KP_PGDN},
	{"KP_3", KP_PGDN},

	{"KP_INS", KP_INS},
	{"KP_0", KP_INS},
	{"KP_DEL", KP_DEL},
	{"KP_ENTER", KP_ENTER},

	{"F1", K_F1},
	{"F2", K_F2},
	{"F3", K_F3},
	{"F4", K_F4},
	{"F5", K_F5},
	{"F6", K_F6},
	{"F7", K_F7},
	{"F8", K_F8},
	{"F9", K_F9},
	{"F10", K_F10},
	{"F11", K_F11},
	{"F12", K_F12},

	{"INS", K_INS},
	{"DEL", K_DEL},
	{"PGDN", K_PGDN},
	{"PGUP", K_PGUP},
	{"HOME", K_HOME},
	{"END", K_END},

	{"MOUSE1", K_MOUSE1},
	{"MOUSE2", K_MOUSE2},
	{"MOUSE3", K_MOUSE3},
	{"MOUSE4", K_MOUSE4},
	{"MOUSE5", K_MOUSE5},
	{"MOUSE6", K_MOUSE6},
	{"MOUSE7", K_MOUSE7},
	{"MOUSE8", K_MOUSE8},

	{"JOY1", K_JOY1},
	{"JOY2", K_JOY2},
	{"JOY3", K_JOY3},
	{"JOY4", K_JOY4},

	{"AUX1", K_AUX1},
	{"AUX2", K_AUX2},
	{"AUX3", K_AUX3},
	{"AUX4", K_AUX4},
	{"AUX5", K_AUX5},
	{"AUX6", K_AUX6},
	{"AUX7", K_AUX7},
	{"AUX8", K_AUX8},
	{"AUX9", K_AUX9},
	{"AUX10", K_AUX10},
	{"AUX11", K_AUX11},
	{"AUX12", K_AUX12},
	{"AUX13", K_AUX13},
	{"AUX14", K_AUX14},
	{"AUX15", K_AUX15},
	{"AUX16", K_AUX16},
	{"AUX17", K_AUX17},
	{"AUX18", K_AUX18},
	{"AUX19", K_AUX19},
	{"AUX20", K_AUX20},
	{"AUX21", K_AUX21},
	{"AUX22", K_AUX22},
	{"AUX23", K_AUX23},
	{"AUX24", K_AUX24},
	{"AUX25", K_AUX25},
	{"AUX26", K_AUX26},
	{"AUX27", K_AUX27},
	{"AUX28", K_AUX28},
	{"AUX29", K_AUX29},
	{"AUX30", K_AUX30},
	{"AUX31", K_AUX31},
	{"AUX32", K_AUX32},

	{"MWHEELUP", K_MWHEELUP},
	{"MWHEELDOWN", K_MWHEELDOWN},

	{"SEMICOLON", ';'},	// because a raw semicolon separates commands

	{NULL,0}
};

/*
==============================================================================
			LINE TYPING INTO THE CONSOLE
==============================================================================
*/

qboolean CheckForCommand (void) {
	char command[256], *s;

	Q_strncpyz(command, key_lines[edit_line] + 1, sizeof(command));
	for (s = command; *s > ' '; s++)
		;
	*s = 0;

	return (Cvar_FindVar(command) || Cmd_FindCommand(command) || Cmd_FindAlias(command) || Cmd_IsLegacyCommand(command));
}

//===================================================================
//  Advanced command completion

#define COLUMNWIDTH 20
#define MINCOLUMNWIDTH 18	// the last column may be slightly smaller

void PaddedPrint (char *s) {	
	extern int con_linewidth;
	int	nextcolx = 0;

	if (con.x)
		nextcolx = (int)((con.x + COLUMNWIDTH)/COLUMNWIDTH)*COLUMNWIDTH;

	if (nextcolx > con_linewidth - MINCOLUMNWIDTH
		|| (con.x && nextcolx + strlen(s) >= con_linewidth))
		Com_Printf ("\n");

	if (con.x)
		Com_Printf (" ");
	while (con.x % COLUMNWIDTH)
		Com_Printf (" ");
	Com_Printf ("%s", s);
}

static char	compl_common[64];
static int	compl_clen;
static int	compl_len;

static void FindCommonSubString (char *s) {
	if (!compl_clen) {
		Q_strncpyz (compl_common, s, sizeof(compl_common));
		compl_clen = strlen (compl_common);
	} else {
		while (compl_clen > compl_len && Q_strncasecmp(s, compl_common, compl_clen))
			compl_clen--;
	}
}


int Cmd_CompleteCountPossible (char *partial);
int Cmd_AliasCompleteCountPossible (char *partial);
int Cvar_CompleteCountPossible (char *partial);
void CompleteName(void);

extern cmd_function_t *cmd_functions;
extern cmd_alias_t *cmd_alias;
extern cvar_t *cvar_vars;

void CompleteCommand (void) {
	char *cmd, token[MAXCMDLINE], *s, temp[MAXCMDLINE];
	int c, a, v, start, end, i, diff_len, size;

	if (key_linepos < 2 || isspace(key_lines[edit_line][key_linepos - 1]))	
		return;

	for (start = key_linepos - 1; start >= 1 && !isspace(key_lines[edit_line][start]); start--)
		;
	if (start == 0)	
		start = 1;
	if (isspace(key_lines[edit_line][start]))	
		start++;
	end = key_linepos - 1;

	size = min(end - start + 1, sizeof(token) - 1);
	memcpy(token, &key_lines[edit_line][start], size);
	token[size] = 0;

	s = token;
	if (*s == '\\' || *s == '/') {
		s++;
		start++;
	}
	if (start > end)
		return;	

	compl_len = strlen (s);
	compl_clen = 0;

	c = Cmd_CompleteCountPossible (s);
	a = Cmd_AliasCompleteCountPossible (s);
	v = Cvar_CompleteCountPossible (s);

	if (c + a + v > 1) {
		cmd_function_t	*cmd;
		cmd_alias_t *alias;
		cvar_t	*var;

		Com_Printf ("\n");

		if (c) {
			Com_Printf ("\x02" "Commands:\n");
			for (cmd=cmd_functions ; cmd ; cmd=cmd->next) {
				if (!Q_strncasecmp (s, cmd->name, compl_len)) {
					PaddedPrint (cmd->name);
					FindCommonSubString (cmd->name);
				}
			}
			if (con.x)
				Com_Printf ("\n");
		}

		if (v) {
			Com_Printf ("\x02" "Variables:\n");
			for (var=cvar_vars ; var ; var=var->next) {
				if (!Q_strncasecmp (s, var->name, compl_len)) {
					PaddedPrint (var->name);
					FindCommonSubString (var->name);
				}
			}
			if (con.x)
				Com_Printf ("\n");
		}

		if (a) {
			Com_Printf ("\x02" "Aliases:\n");
			for (alias=cmd_alias ; alias ; alias=alias->next)
				if (!Q_strncasecmp (s, alias->name, compl_len)) {
					PaddedPrint (alias->name);
					FindCommonSubString (alias->name);
				}
			if (con.x)
				Com_Printf ("\n");
		}

	}

	if (c + a + v == 1) {
		cmd = Cmd_CompleteCommand (s);
		if (!cmd)
			cmd = Cvar_CompleteVariable (s);
		if (!cmd)
			return;	// this should never happen
	} else if (compl_clen) {
		compl_common[compl_clen] = 0;
		cmd = compl_common;
	} else {
		CompleteName();
		return;
	}
	diff_len = strlen(cmd) - (end - start + 1);
	Q_strncpyz(temp, key_lines[edit_line] + end + 1, sizeof(temp));
	Q_strncpyz(key_lines[edit_line] + end + 1 + diff_len, temp, MAXCMDLINE - (end + 1 + diff_len));
	for (i = 0; start + i < MAXCMDLINE && i < strlen(cmd); i++)	
		key_lines[edit_line][start + i] = cmd[i];
	key_linepos += diff_len;
	key_lines[edit_line][min(key_linepos + strlen(temp), MAXCMDLINE - 1)] = 0;
	if (start == 1 && key_linepos + strlen(temp) < MAXCMDLINE - 1) {
		for (i = key_linepos + strlen(temp); i > 0; i--)
			key_lines[edit_line][i + 1] = key_lines[edit_line][i];
		key_lines[edit_line][1] = '/';
		key_linepos++;
	}
	if (c + a + v == 1 && !key_lines[edit_line][key_linepos] && key_linepos < MAXCMDLINE - 1) {
		key_lines[edit_line][key_linepos] = ' ';
		key_lines[edit_line][++key_linepos] = 0;
	}
}



extern char readableChars[];
char disallowed[] = {'\n', '\f', '\\', '/', '\"', ' ' , ';'};

void RemoveColors(char *name) {
	char *s = name;

	if (!s || !*s)
		return;

	while (*s) {
		*s = readableChars[(unsigned char) *s] & 127;
		if (strchr(disallowed, *s))
			*s = '_';
		s++;
	}
	// get rid of whitespace
	s = name;
	for (s = name; *s == '_'; s++) ;
	memmove(name, s, strlen(s) + 1);
	
	for (s = name + strlen(name); s > name  &&  (*(s - 1) == '_'); s--) ;

	*s = 0;

	if (!name[0])
		strcpy(name, "_");
}


int FindBestNick (char *s) {
	int i, j, bestplayer = -1, best = -1;
	char name[MAX_SCOREBOARDNAME], *match;

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].name[0]) {
			Q_strncpyz(name, cl.players[i].name, sizeof(name));
			RemoveColors(name);
			for (j = 0; j < strlen(name); j++)
				name[j] = tolower(name[j]);
			if ((match = strstr(name, s))  &&  (best == -1 || match - name < best)) {
				best = match - name;
				bestplayer = i;
			}
		}
	}
	return bestplayer;
}



void CompleteName(void) {
    char s[MAXCMDLINE], t[MAXCMDLINE], *p, *q;
    int best, diff, i;

    p = q = key_lines[edit_line] + key_linepos;
    while (--p >= key_lines[edit_line] + 1)
        if (!(  (*p >= 32 && *p <= 127) && !strchr(disallowed, *p) )) 
             break;
    p++;
    if (q - p <= 0)
        return;

    Q_strncpyz(s, p, q - p + 1);

	best = FindBestNick (s);
    if (best >= 0) {
        Q_strncpyz(t, cl.players[best].name, sizeof(t));
		
		for (i = 0; t[i]; i++) {
			if ((127 & t[i]) == ' ') {
				int k;

				if ((k = strlen(t)) < MAXCMDLINE - 2) {
					memmove(t + 1, t, k + 1);
					t[k + 2] = 0;
					t[k + 1] = t[0] = '\"';
				}
				break;
			}
		}
		diff = strlen(t) - strlen(s);
	
		memmove(q + diff, q, strlen(q) + 1);
		memmove(p, t, strlen(t));
		key_linepos += diff;
		if (!key_lines[edit_line][key_linepos] && key_linepos < MAXCMDLINE - 1) {	
			key_lines[edit_line][key_linepos] = ' ';
			key_lines[edit_line][++key_linepos] = 0;
		}
	}
}

//===================================================================

static void AdjustConsoleHeight (int delta) {
	extern cvar_t scr_consize;
	int height;

	if (cls.state != ca_active && !cl.intermission)
		return;
	height = (scr_consize.value * vid.height + delta + 5) / 10;
	height *= 10;
	if (delta < 0 && height < 30)
		height = 30;
	if (delta > 0 && height > vid.height - 10)
		height = vid.height - 10;
	Cvar_SetValue (&scr_consize, (float)height / vid.height);
}

//Interactive line editing and console scrollback
void Key_Console (int key) {
	int i, len;

	switch (key) {
	    case K_ENTER:
			// backslash text are commands
			if (key_lines[edit_line][1] == '/' && key_lines[edit_line][2] == '/')
				goto no_lf;

			if ((keydown[K_CTRL] || keydown[K_SHIFT]) && cls.state >= ca_connected) {
				if (keydown[K_CTRL])
					Cbuf_AddText ("say_team ");
				else
					Cbuf_AddText ("say ");
				Cbuf_AddText (key_lines[edit_line] + 1);
			} else if (key_lines[edit_line][1] == '\\' || key_lines[edit_line][1] == '/') {
				Cbuf_AddText (key_lines[edit_line] + 2);	// skip the ]/
			} else if (cl_chatmode.value != 1 && CheckForCommand()) {
				Cbuf_AddText (key_lines[edit_line] + 1);	// valid command
			} else if ((cls.state >= ca_connected && cl_chatmode.value == 2) || cl_chatmode.value == 1) {
				if (cls.state < ca_connected)	// can happen if cl_chatmode is 1
					goto no_lf;					// drop the whole line

				Cbuf_AddText ("say ");
				Cbuf_AddText (key_lines[edit_line] + 1);
			} else {
				Cbuf_AddText (key_lines[edit_line] + 1);	// skip the ]
			}

			Cbuf_AddText ("\n");
no_lf:
			Com_Printf ("%s\n",key_lines[edit_line]);
			edit_line = (edit_line + 1) & 31;
			history_line = edit_line;
			key_lines[edit_line][0] = ']';
			key_lines[edit_line][1] = 0;
			key_linepos = 1;
			if (cls.state == ca_disconnected)
				SCR_UpdateScreen ();	// force an update, because the command
										// may take some time
			return;

		case K_TAB:
			// command completion
			if (keydown[K_CTRL])
				CompleteName ();
			else
				CompleteCommand ();
			return;

		case K_BACKSPACE:
			if (key_linepos > 1) {
				strcpy(key_lines[edit_line] + key_linepos - 1, key_lines[edit_line] + key_linepos);
				key_linepos--;
			}
			return;

		case K_DEL:
			if (key_linepos < strlen(key_lines[edit_line]))
				strcpy(key_lines[edit_line] + key_linepos, key_lines[edit_line] + key_linepos + 1);
			return;

		case K_RIGHTARROW:
			if (keydown[K_CTRL]) {
				// word right
				i = strlen(key_lines[edit_line]);
				while (key_linepos < i && key_lines[edit_line][key_linepos] != ' ')
					key_linepos++;
				while (key_linepos < i && key_lines[edit_line][key_linepos] == ' ')
					key_linepos++;
				return;
			}
			if (key_linepos < strlen(key_lines[edit_line]))
				key_linepos++;
			return;

	    case K_LEFTARROW:
			if (keydown[K_CTRL]) {
				// word left
				while (key_linepos > 1 && key_lines[edit_line][key_linepos-1] == ' ')
					key_linepos--;
				while (key_linepos > 1 && key_lines[edit_line][key_linepos-1] != ' ')
					key_linepos--;
				return;
			}
			if (key_linepos > 1)
				key_linepos--;
			return;

	    case K_UPARROW:
			if (keydown[K_CTRL]) {
				AdjustConsoleHeight (-10);
				return;
			}
			do {
				history_line = (history_line - 1) & 31;
			} while (history_line != edit_line
					&& !key_lines[history_line][1]);
			if (history_line == edit_line)
				history_line = (edit_line+1)&31;
			strcpy(key_lines[edit_line], key_lines[history_line]);
			key_linepos = strlen(key_lines[edit_line]);
			return;

		case K_DOWNARROW:
			if (keydown[K_CTRL]) {
				AdjustConsoleHeight (10);
				return;
			}
			if (history_line == edit_line) 
				return;
			do {
				history_line = (history_line + 1) & 31;
			} while (history_line != edit_line
				&& !key_lines[history_line][1]);

			if (history_line == edit_line) {
				key_lines[edit_line][0] = ']';
				key_lines[edit_line][1] = 0;
				key_linepos = 1;
			} else {
				strcpy(key_lines[edit_line], key_lines[history_line]);
				key_linepos = strlen(key_lines[edit_line]);
			}
			return;

	    case K_PGUP:
	    case K_MWHEELUP:
			if (keydown[K_CTRL] && key == K_PGUP)
				con.display -= ((int)scr_conlines - 22) >> 3;
			else
				con.display -= 2;
			if (con.display - con.current + con.numlines < 0)
				con.display = con.current - con.numlines;
			return;

	    case K_MWHEELDOWN:
	    case K_PGDN:
			if (keydown[K_CTRL] && key == K_PGDN)
				con.display += ((int)scr_conlines - 22) >> 3;
			else
				con.display += 2;
			if (con.display - con.current > 0)
				con.display = con.current;
			return;

	    case K_HOME:
			if (keydown[K_CTRL])
				con.display = con.current - con.numlines;
			else
				key_linepos = 1;
			return;

	    case K_END:
			if (keydown[K_CTRL])
				con.display = con.current;
			else
				key_linepos = strlen(key_lines[edit_line]);
			return;
	}
	if ((key == 'V' || key == 'v') && keydown[K_CTRL]) {
		char *clipText;
	
		if ((clipText = Sys_GetClipboardData())) {
			len = strlen(clipText);
			if (len + strlen(key_lines[edit_line]) > MAXCMDLINE - 1)
				len = MAXCMDLINE - 1 - strlen(key_lines[edit_line]);
			if (len > 0) {	// insert the string
				memmove (key_lines[edit_line] + key_linepos + len,
					key_lines[edit_line] + key_linepos, strlen(key_lines[edit_line]) - key_linepos + 1);
				memcpy (key_lines[edit_line] + key_linepos, clipText, len);
				key_linepos += len;
			}
		}
		return;
	}

	if (key == 'u' && keydown[K_CTRL]) {			
		if (key_linepos > 1) {
			strcpy(key_lines[edit_line] + 1, key_lines[edit_line] + key_linepos);
			key_linepos = 1;
		}
		return;
	}

	if (key < 32 || key > 127)
		return;	// non printable

	if (keydown[K_CTRL]) {
		if (key >= '0' && key <= '9')
				key = key - '0' + 0x12;	// yellow number
		else switch (key) {
			case '[': key = 0x10; break;
			case ']': key = 0x11; break;
			case 'g': key = 0x86; break;
			case 'r': key = 0x87; break;
			case 'y': key = 0x88; break;
			case 'b': key = 0x89; break;
			case '(': key = 0x80; break;
			case '=': key = 0x81; break;
			case ')': key = 0x82; break;
			case 'a': key = 0x83; break;
			case '<': key = 0x1d; break;
			case '-': key = 0x1e; break;
			case '>': key = 0x1f; break;
			case ',': key = 0x1c; break;
			case '.': key = 0x9c; break;
			case 'B': key = 0x8b; break;
			case 'C': key = 0x8d; break;
		}
	}

	if (keydown[K_ALT])
		key |= 128;		// red char

	i = strlen(key_lines[edit_line]);
	if (i >= MAXCMDLINE-1)
		return;

	// This also moves the ending \0
	memmove (key_lines[edit_line]+key_linepos+1, key_lines[edit_line]+key_linepos, i-key_linepos+1);
	key_lines[edit_line][key_linepos] = key;
	key_linepos++;
}

//============================================================================

qboolean	chat_team;
char		chat_buffer[MAXCMDLINE];
int			chat_linepos = 0;

void Key_Message (int key) {
	int len;

	switch (key) {
	case K_ENTER:
		if (chat_buffer[0]) {
			Cbuf_AddText (chat_team ? "say_team \"" : "say \"");
			Cbuf_AddText(chat_buffer);
			Cbuf_AddText("\"\n");
		}
		key_dest = key_game;
		chat_linepos = 0;
		chat_buffer[0] = 0;
		return;

	case K_ESCAPE:
		key_dest = key_game;
		chat_buffer[0] = 0;
		chat_linepos = 0;
		return;

	case K_HOME:
		chat_linepos = 0;
		return;

	case K_END:
		chat_linepos = strlen(chat_buffer);
		return;

	case K_LEFTARROW:
		if (chat_linepos > 0)
			chat_linepos--;
		return;

	case K_RIGHTARROW:
		if (chat_linepos < strlen(chat_buffer))
			chat_linepos++;
		return;

	case K_BACKSPACE:
		if (chat_linepos > 0) {
			strcpy(chat_buffer + chat_linepos - 1, chat_buffer + chat_linepos);
			chat_linepos--;
		}
		return;

	case K_DEL:
		if (chat_buffer[chat_linepos])
			strcpy(chat_buffer + chat_linepos, chat_buffer + chat_linepos + 1);
		return;
	}

	if ((key == 'V' || key == 'v') && keydown[K_CTRL]) {
		char *clipText;
	
		if ((clipText = Sys_GetClipboardData())) {
			len = strlen(clipText);
			if (len + strlen(key_lines[edit_line]) > MAXCMDLINE - 1)
				len = MAXCMDLINE - 1 - strlen(key_lines[edit_line]);
			if (len > 0) {	// insert the string
				memmove (key_lines[edit_line] + key_linepos + len,
					key_lines[edit_line] + key_linepos, strlen(key_lines[edit_line]) - key_linepos + 1);
				memcpy (key_lines[edit_line] + key_linepos, clipText, len);
				key_linepos += len;
			}
		}
		return;
	}

	if (key < 32 || key > 127)
		return;	// non printable

	len = strlen(chat_buffer);

	if (len >= sizeof(chat_buffer) - 1)
		return; // all full

	// This also moves the ending \0
	memmove (chat_buffer+chat_linepos+1, chat_buffer+chat_linepos, len - chat_linepos + 1);
	chat_buffer[chat_linepos++] = key;
}

//============================================================================

//Returns a key number to be used to index keybindings[] by looking at the given string.
//Single ascii characters return themselves, while the K_* names are matched up.
int Key_StringToKeynum (char *str) {
	keyname_t *kn;
	
	if (!str || !str[0])
		return -1;
	if (!str[1])
		return (unsigned char) str[0];

	for (kn = keynames; kn->name; kn++) {
		if (!Q_strcasecmp(str,kn->name))
			return kn->keynum;
	}
	return -1;
}

//Returns a string (either a single ascii char, or a K_* name) for the given keynum.
//FIXME: handle quote special (general escape sequence?)
char *Key_KeynumToString (int keynum) {
	keyname_t *kn;	
	static char tinystr[2];
	
	if (keynum == -1)
		return "<KEY NOT FOUND>";
	if (keynum > 32 && keynum < 127) {	// printable ascii
		tinystr[0] = keynum;
		tinystr[1] = 0;
		return tinystr;
	}
	
	for (kn=keynames ; kn->name ; kn++)
		if (keynum == kn->keynum)
			return kn->name;

	return "<UNKNOWN KEYNUM>";
}

void Key_SetBinding (int keynum, char *binding) {
	if (keynum == -1)
		return;

	if (keynum == K_CTRL || keynum == K_ALT || keynum == K_SHIFT || keynum == K_WIN) {
		
		Key_SetBinding(keynum + 1, binding);
		Key_SetBinding(keynum + 2, binding);
		return;
	}

	// free old bindings
	if (keybindings[keynum]) {
		Z_Free (keybindings[keynum]);
		keybindings[keynum] = NULL;
	}

	// allocate memory for new binding
	keybindings[keynum] = CopyString (binding);	
}

void Key_Unbind (int keynum) {
	if (keynum == -1)
		return;

	if (keynum == K_CTRL || keynum == K_ALT || keynum == K_SHIFT || keynum == K_WIN) {
		
		Key_Unbind(keynum + 1);
		Key_Unbind(keynum + 2);
		return;
	}

	if (keybindings[keynum]) {
		Z_Free (keybindings[keynum]);
		keybindings[keynum] = NULL;
	}
}

void Key_Unbind_f (void) {
	int b;

	if (Cmd_Argc() != 2) {
		Com_Printf ("Usage:  %s <key> : remove commands from a key\n", Cmd_Argv(0));
		return;
	}

	b = Key_StringToKeynum (Cmd_Argv(1));
	if (b == -1) {
		Com_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	Key_Unbind (b);
}

void Key_Unbindall_f (void) {
	int i;
	
	for (i = 0; i < 256; i++)
		if (keybindings[i])
			Key_Unbind (i);
}

static void Key_PrintBindInfo(int keynum, char *keyname) {
	if (!keyname)
		keyname = Key_KeynumToString(keynum);

	if (keynum == -1) {
		Com_Printf ("\"%s\" isn't a valid key\n", keyname);
		return;
	}

	if (keybindings[keynum])
		Com_Printf ("\"%s\" = \"%s\"\n", keyname, keybindings[keynum]);
	else
		Com_Printf ("\"%s\" is not bound\n", keyname);
}

//checks if LCTRL and RCTRL are both bound and bound to the same thing
qboolean Key_IsLeftRightSameBind(int b) {
	if (b < 0 || b >= sizeof(keybindings))
		return false;

	return	(b == K_CTRL || b == K_ALT || b == K_SHIFT || b == K_WIN) &&
			(keybindings[b + 1] && keybindings[b + 2] && !strcmp(keybindings[b + 1], keybindings[b + 2]));
}

void Key_Bind_f (void) {
	int c, b;
	
	c = Cmd_Argc();

	if (c < 2) {
		Com_Printf ("Usage: %s <key> [command] : attach a command to a key\n", Cmd_Argv(0));
		return;
	}
	b = Key_StringToKeynum (Cmd_Argv(1));
	if (b == -1) {
		Com_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}
	if (c == 2) {
		if ((b == K_CTRL || b == K_ALT || b == K_SHIFT || b == K_WIN) && (keybindings[b + 1] || keybindings[b + 2])) {
			
			if (keybindings[b + 1] && keybindings[b + 2] && !strcmp(keybindings[b + 1], keybindings[b + 2])) {
				Com_Printf ("\"%s\" = \"%s\"\n", Cmd_Argv(1), keybindings[b + 1]);
			} else {
				Key_PrintBindInfo(b + 1, NULL);
				Key_PrintBindInfo(b + 2, NULL);
			}
		} else {
			
			//		and the following should print "ctrl (etc) is not bound" since K_CTRL cannot be bound
			Key_PrintBindInfo(b, Cmd_Argv(1));
		}
		return;
	}
	
	// copy the rest of the command line
	Key_SetBinding (b, Cmd_MakeArgs(2));
}

void Key_BindList_f (void) {
	int i;

	for (i = 0; i < 256; i++) {
		if (Key_IsLeftRightSameBind(i)) {
			Com_Printf ("%s \"%s\"\n", Key_KeynumToString(i), keybindings[i + 1]);
			i += 2;	
		} else {
			if (keybindings[i])
				Com_Printf ("%s \"%s\"\n", Key_KeynumToString(i), keybindings[i]);
		}
	}
}

//Writes lines containing "bind key value"
void Key_WriteBindings (FILE *f) {
	int i, leftright;

	for (i = 0; i < 256; i++) {
		leftright = Key_IsLeftRightSameBind(i) ? 1 : 0;
		if (leftright || keybindings[i]) {
			if (i == ';')
				fprintf (f, "bind \";\" \"%s\"\n", keybindings[i]);
			else
				fprintf (f, "bind %s \"%s\"\n", Key_KeynumToString(i), keybindings[leftright ? i + 1 : i]);

			if (leftright)
				i += 2;
		}
	}
}

void Key_Init (void) {
	int i;

	for (i = 0; i < 32; i++) {
		key_lines[i][0] = ']';
		key_lines[i][1] = 0;
	}
	key_linepos = 1;

	// init ascii characters in console mode
	for (i = 32; i < 128; i++)
		consolekeys[i] = true;
	consolekeys[K_ENTER] = true;
	consolekeys[K_TAB] = true;
	consolekeys[K_LEFTARROW] = true;
	consolekeys[K_RIGHTARROW] = true;
	consolekeys[K_UPARROW] = true;
	consolekeys[K_DOWNARROW] = true;
	consolekeys[K_BACKSPACE] = true;
	consolekeys[K_INS] = true;
	consolekeys[K_DEL] = true;
	consolekeys[K_HOME] = true;
	consolekeys[K_END] = true;
	consolekeys[K_PGUP] = true;
	consolekeys[K_PGDN] = true;
	consolekeys[K_ALT] = true;
	consolekeys[K_LALT] = true;
	consolekeys[K_RALT] = true;
	consolekeys[K_CTRL] = true;
	consolekeys[K_LCTRL] = true;
	consolekeys[K_RCTRL] = true;
	consolekeys[K_SHIFT] = true;
	consolekeys[K_LSHIFT] = true;
	consolekeys[K_RSHIFT] = true;
	consolekeys[K_MWHEELUP] = true;
	consolekeys[K_MWHEELDOWN] = true;
	consolekeys['`'] = false;
	consolekeys['~'] = false;

	for (i = 0; i < 256; i++)
		keyshift[i] = i;
	for (i = 'a'; i <= 'z'; i++)
		keyshift[i] = i - 'a' + 'A';
	keyshift['1'] = '!';
	keyshift['2'] = '@';
	keyshift['3'] = '#';
	keyshift['4'] = '$';
	keyshift['5'] = '%';
	keyshift['6'] = '^';
	keyshift['7'] = '&';
	keyshift['8'] = '*';
	keyshift['9'] = '(';
	keyshift['0'] = ')';
	keyshift['-'] = '_';
	keyshift['='] = '+';
	keyshift[','] = '<';
	keyshift['.'] = '>';
	keyshift['/'] = '?';
	keyshift[';'] = ':';
	keyshift['\''] = '"';
	keyshift['['] = '{';
	keyshift[']'] = '}';
	keyshift['`'] = '~';
	keyshift['\\'] = '|';

	menubound[K_ESCAPE] = true;
	for (i = 0; i < 12; i++)
		menubound[K_F1 + i] = true;

	// register our functions
	Cmd_AddCommand ("bindlist",Key_BindList_f);
	Cmd_AddCommand ("bind",Key_Bind_f);
	Cmd_AddCommand ("unbind",Key_Unbind_f);
	Cmd_AddCommand ("unbindall",Key_Unbindall_f);

	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register (&cl_chatmode);

	Cvar_ResetCurrentGroup();
}

//Called by the system between frames for both key up and key down events Should NOT be called during an interrupt!
void Key_Event (int key, qboolean down) {
	char *kb, cmd[1024];

	//	Com_Printf ("%i : %i\n", key, down); //@@@

	if (key == K_LALT || key == K_RALT)
		Key_Event (K_ALT, down);
	else if (key == K_LCTRL || key == K_RCTRL)
		Key_Event (K_CTRL, down);
	else if (key == K_LSHIFT || key == K_RSHIFT)
		Key_Event (K_SHIFT, down);
	else if (key == K_LWIN || key == K_RWIN)
		Key_Event (K_WIN, down);

	keydown[key] = down;

	if (!down)
		key_repeats[key] = 0;

	key_lastpress = key;

	// update auto-repeat status
	if (down) {
		key_repeats[key]++;
		if (key_repeats[key] > 1) {
			if ((key != K_BACKSPACE && key != K_DEL
				&& key != K_LEFTARROW && key != K_RIGHTARROW
				&& key != K_UPARROW && key != K_DOWNARROW
				&& key != K_PGUP && key != K_PGDN && (key < 32 || key > 126 || key == '`'))
				|| (key_dest == key_game && cls.state == ca_active))
				return;	// ignore most autorepeats
		}
	}

	// handle escape specialy, so the user can never unbind it
	if (key == K_ESCAPE) {
		if (!down)
			return;
		switch (key_dest) {
		case key_message:
			Key_Message (key);
			break;
		case key_menu:
			M_Keydown (key);
			break;
		case key_game:
			M_ToggleMenu_f ();
			break;
		case key_console:
			if (!SCR_NEED_CONSOLE_BACKGROUND)
				Con_ToggleConsole_f ();
			else
				M_ToggleMenu_f ();
			break;
		default:
			assert(!"Bad key_dest");
		}
		return;
	}

	// key up events only generate commands if the game key binding is a button command (leading + sign).
	// These will occur even in console mode, to keep the character from continuing an action started before a
	// console switch.  Button commands include the kenum as a parameter, so multiple downs can be matched with ups
	if (!down) {
		kb = keybindings[key];
		if (kb && kb[0] == '+' && keyactive[key]) {
			sprintf (cmd, "-%s %i\n", kb+1, key);
			Cbuf_AddText (cmd);
			keyactive[key] = false;
		}
		if (keyshift[key] != key) {
			kb = keybindings[keyshift[key]];
			if (kb && kb[0] == '+' && keyactive[keyshift[key]]) {
				sprintf (cmd, "-%s %i\n", kb+1, key);
				Cbuf_AddText (cmd);
				keyactive[keyshift[key]] = false;
			}
		}
		return;
	}

	// if not a consolekey, send to the interpreter no matter what mode is
	if	( 
			(key_dest == key_menu && menubound[key]) || 
			((key_dest == key_console || key_dest == key_message) && !consolekeys[key]) || 
			(key_dest == key_game && (cls.state == ca_active || !consolekeys[key]))
		) {
		kb = keybindings[key];
		if (kb) {
			if (kb[0] == '+'){	// button commands add keynum as a parm
				sprintf (cmd, "%s %i\n", kb, key);
				Cbuf_AddText (cmd);
				keyactive[key] = true;
			} else {
				Cbuf_AddText (kb);
				Cbuf_AddText ("\n");
			}
		}
		return;
	}

	if (!down)
		return;		// other systems only care about key down events

	if (keydown[K_SHIFT])
		key = keyshift[key];

	switch (key_dest) {
	case key_message:
		Key_Message (key);
		break;
	case key_menu:
		M_Keydown (key);
		break;

	case key_game:
	case key_console:
		Key_Console (key);
		break;
	default:
		assert(!"Bad key_dest");
	}
}

void Key_ClearStates (void) {
	int		i;

	for (i = 0; i < 256; i++) {
		keydown[i] = false;
		key_repeats[i] = false;
	}
}
