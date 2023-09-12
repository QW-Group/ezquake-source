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

    $Id: keys.c,v 1.80 2007-09-26 13:53:42 tonik Exp $

*/

#include <wchar.h>
#include "quakedef.h"
#include "textencoding.h"
#include "menu.h"
#include "keys.h"
#include "input.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include "gl_model.h"
#include "tr_types.h"

#include "hud.h"
#include "hud_common.h"
#include "hud_editor.h"
#include "demo_controls.h"
#include "irc.h"
#include "qtv.h"
#include "utils.h"

extern cvar_t sys_disable_alt_enter;

//key up events are sent even if in console mode

cvar_t cl_chatmode                  = {"cl_chatmode", "2"};
cvar_t con_funchars_mode            = {"con_funchars_mode", "0"};
cvar_t con_tilde_mode               = {"con_tilde_mode", "0"};
// values: old style, current + default values , current only, default only, current + default if changed, none.
cvar_t con_completion_format        = {"con_completion_format", "2"};
cvar_t con_hide_chat_input          = {"con_hide_chat_input", "1"};
cvar_t con_completion_changed_mark  = {"con_completion_changed_mark", "1"};
cvar_t con_bindphysical             = {"con_bindphysical", "0"};
cvar_t in_builtinkeymap             = {"in_builtinkeymap", "0"};

char* escape_regex(char* string);
void OnChange_con_prompt_charcode(cvar_t *var, char *string, qbool *cancel);
void OnChange_con_completion_color(cvar_t *var, char *string, qbool *cancel);
cvar_t con_prompt_charcode					= {"con_prompt_charcode", "93", CVAR_NONE, OnChange_con_prompt_charcode};
cvar_t con_completion_color_name			= {"con_completion_color_name", "8ff", CVAR_NONE, OnChange_con_completion_color};
cvar_t con_completion_color_value_current	= {"con_completion_color_value_current", "fff", CVAR_NONE, OnChange_con_completion_color};
cvar_t con_completion_color_value_default	= {"con_completion_color_value_default", "fff", CVAR_NONE, OnChange_con_completion_color};
cvar_t con_completion_color_quotes_current	= {"con_completion_color_quotes_current", "ff8", CVAR_NONE, OnChange_con_completion_color};
cvar_t con_completion_color_quotes_default	= {"con_completion_color_quotes_default", "ff8", CVAR_NONE, OnChange_con_completion_color};
cvar_t con_completion_color_colon			= {"con_completion_color_colon", "fff", CVAR_NONE, OnChange_con_completion_color};
cvar_t con_completion_color_title			= {"con_completion_color_title", "ff3", CVAR_NONE, OnChange_con_completion_color};
cvar_t con_completion_color_changed_mark	= {"con_completion_color_changed_mark", "f30", CVAR_NONE, OnChange_con_completion_color};
cvar_t con_completion_padding				= {"con_completion_padding", "2"};

cvar_t	cl_savehistory = {"cl_savehistory", "1"};
#define		HISTORY_FILE_NAME	"ezquake/.ezquake_history"

wchar	key_lines[CMDLINES][MAXCMDLINE];
int		key_linepos;
int		key_lastpress;

int		edit_line=0;
int		history_line=0;

int del_removes;
int key_lineposorig;
int old_keyline_length;
int last_cmd_length = 0;
int called_second = 0;
int try = 0;
int count = 0;
int count_cmd = 0;
int count_cvar = 0;
int count_alias = 0;

keydest_t	key_dest, key_dest_beforemm, key_dest_beforecon;

char	*keybindings[UNKNOWN + 256];
qbool	consolekeys[UNKNOWN + 256];	// if true, can't be rebound while in console
qbool	hudeditorkeys[UNKNOWN + 256];	// if true, can't be rebound while in hud editor
qbool	democontrolskey[UNKNOWN + 256];
int		keyshift[UNKNOWN + 256];		// key to map to if shift held down in console
int		key_repeats[UNKNOWN + 256];	// if > 1, it is autorepeating
qbool	keydown[UNKNOWN + 256];
qbool	keyactive[UNKNOWN + 256];

// SDL2 sends a KEYDOWN event and then an optional TEXTINPUT event for the actual character generated
// This stores the last quake key (K_*) from the KEYDOWN, or 0 if any subsequent TEXTINPUT event should be ignored.
static int lastKeyDown = 0;
static qbool lastKeyGeneratedCharacter = false;

typedef struct
{
	char *name;
	char *type;
} jogi_avail_complete_t;


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
	{"PRINTSCRN", K_PRINTSCR},
	{"PRINTSCREEN", K_PRINTSCR},
	{"SCRLCK", K_SCRLCK},
	{"SCROLLLOCK", K_SCRLCK},
	{"SCROLLOCK", K_SCRLCK},	// misspelled; kept for compatibility

#ifdef __APPLE__
	{"COMMAND", K_CMD},
	{"PARA", K_PARA},
	{"F13", K_F13},
	{"F14", K_F14},
	{"F15", K_F15},
	{"KP_EQUAL", KP_EQUAL},
#endif

	{"PAUSE", K_PAUSE},

	{"UPARROW", K_UPARROW},
	{"DOWNARROW", K_DOWNARROW},
	{"LEFTARROW", K_LEFTARROW},
	{"RIGHTARROW", K_RIGHTARROW},

	{"ALT", K_ALT},
	{"LALT", K_LALT},
	{"RALT", K_RALT},

	{"ALTGR", K_RALT},
	{"ALTCHAR", K_RALT},

	{"CTRL", K_CTRL},
	{"LCTRL", K_LCTRL},
	{"RCTRL", K_RCTRL},
	{"SHIFT", K_SHIFT},
	{"LSHIFT", K_LSHIFT},
	{"RSHIFT", K_RSHIFT},

	{"WINKEY", K_WIN},
	{"LWINKEY", K_LWIN},
	{"RWINKEY", K_RWIN},
	{"POPUPMENU", K_MENU}, //???

	// special keys
	{"WIN", K_WIN},
	{"LWIN", K_LWIN},
	{"RWIN", K_RWIN},
	{"SUPER", K_WIN},
	{"LSUPER", K_LWIN},
	{"RSUPER", K_RWIN},
	{"MENU", K_MENU},
	{"ISO", K_ISO},

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

	{"JOYPOVUP", K_JOYPOVUP},
	{"JOYPOVRT", K_JOYPOVRT},
	{"JOYPOVDN", K_JOYPOVDN},
	{"JOYPOVLT", K_JOYPOVLT},

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

	{"MWHEELUP", K_MWHEELUP},
	{"MWHEELDOWN", K_MWHEELDOWN},

	{"SEMICOLON", ';'},	// because a raw semicolon separates commands

	{"SECTION", K_SECTION},
	{"ACUTE", K_ACUTE},
	{"DIAERESIS", K_DIAERESIS},
	{"ARING", K_ARING},
	{"ADIAERESIS", K_ADIAERESIS},
	{"ODIAERESIS", K_ODIAERESIS},
	{"WAKE_UP", K_WAKE_UP},

	{NULL,0}
};

mouse_state_t scr_pointer_state;

/*
   ==============================================================================
   Flushing my array
   ==============================================================================
   */
void CompleteCommandNew_Reset (void)
{
	if ((del_removes) || (key_linepos == key_lineposorig))
	{
		del_removes = 0;
		called_second = 0;
		try = 0;
	}
}

/*
   ==============================================================================
   LINE TYPING INTO THE CONSOLE
   ==============================================================================
   */

qbool CheckForCommand (void) 
{
	char command[256], *s;

	strlcpy(command, wcs2str(key_lines[edit_line] + 1), sizeof(command));
	for (s = command; *s > ' '; s++)
		;
	*s = 0;

	return command[0] && (Cvar_Find(command) || Cmd_FindCommand(command) || Cmd_FindAlias(command) || Cmd_IsLegacyCommand(command));
}

//===================================================================
//  Advanced command completion

#define COLUMNWIDTH 20
#define MINCOLUMNWIDTH 18	// the last column may be slightly smaller


void PaddedPrint (char *s) 
{
	extern int con_linewidth;
	extern char *str_repeat (char *str, int amount);
	char *pad = NULL;
	int nextcolx = 0;

	if (con_completion_format.integer)  // plain list
	{
		pad = str_repeat(" ", con_completion_padding.integer);
		Com_Printf ("%s&c%s%s&r\n", pad, con_completion_color_name.string, s);
		Q_free(pad);
	}
	else
	{
		if (con.x)
			nextcolx = (int)((con.x + COLUMNWIDTH)/COLUMNWIDTH)*COLUMNWIDTH;

		if (nextcolx > con_linewidth - MINCOLUMNWIDTH || (con.x && nextcolx + strlen(s) >= con_linewidth))
			Com_Printf ("\n");

		if (con.x)
			Com_Printf (" ");
		while (con.x % COLUMNWIDTH)
			Com_Printf (" ");
		Com_Printf ("%s", s);
	}
}

void PaddedPrintMarkAndPad (qbool mark)
{
	extern char *str_repeat (char *str, int amount);

	char* s;
	int padding = con_completion_padding.integer;

	if (mark)
	{
		Com_Printf ("&c%s*", con_completion_color_changed_mark.string);
		padding -= 1;
	}

	s = str_repeat(" ", padding);
	Com_Printf ("%s", s);
	Q_free(s);
}

void PaddedPrintValue (char *s, char *v, char *dv)  // name, value, default value
{
	qbool changed = (strcmp(s, dv) != 0) && (strcmp(dv, v) != 0);
	qbool add_mark = (con_completion_changed_mark.integer > 0) && changed;

	switch (con_completion_format.integer)
	{
		case 1:	// current + default
			if (strcmp(s, dv))
			{
				PaddedPrintMarkAndPad (add_mark);
				Com_Printf ("&c%s%s &c%s:&r &c%s\"&c%s%s&c%s\"&r &c%s:&r &c%s\"&c%s%s&c%s\"&r\n",
						con_completion_color_name.string, s , con_completion_color_colon.string,
						con_completion_color_quotes_current.string, con_completion_color_value_current.string, v,
						con_completion_color_quotes_current.string, con_completion_color_colon.string,
						con_completion_color_quotes_default.string, con_completion_color_value_default.string, dv,
						con_completion_color_quotes_default.string);
			}
			else
			{
				PaddedPrintMarkAndPad (false);
				Com_Printf ("&c%s%s &c%s: &c%s\"&c%s%s&c%s\"&r\n",
						con_completion_color_name.string, s , con_completion_color_colon.string,
						con_completion_color_quotes_current.string, con_completion_color_value_current.string, v,
						con_completion_color_quotes_current.string);
			}
			break;

		case 2:	// current only
			PaddedPrintMarkAndPad (add_mark);
			Com_Printf ("&c%s%s &c%s: &c%s\"&c%s%s&c%s\"&r\n",
					con_completion_color_name.string, s , con_completion_color_colon.string,
					con_completion_color_quotes_current.string, con_completion_color_value_current.string, v,
					con_completion_color_quotes_current.string);
			break;

		case 3:	// default only
			PaddedPrintMarkAndPad (add_mark);
			Com_Printf ("&c%s%s &c%s:&r &c%s\"&c%s%s&c%s\"&r\n",
					con_completion_color_name.string, s, con_completion_color_colon.string,
					con_completion_color_quotes_default.string, con_completion_color_value_default.string,
					dv, con_completion_color_quotes_default.string);
			break;

		case 4:	// current + default if changed
			if(changed)
			{
				if (strcmp(s, dv))
				{
					PaddedPrintMarkAndPad (add_mark);
					Com_Printf ("&c%s%s &c%s:&r &c%s\"&c%s%s&c%s\"&r &c%s:&r &c%s\"&c%s%s&c%s\"&r\n",
							con_completion_color_name.string, s , con_completion_color_colon.string,
							con_completion_color_quotes_current.string, con_completion_color_value_current.string, v,
							con_completion_color_quotes_current.string, con_completion_color_colon.string,
							con_completion_color_quotes_default.string, con_completion_color_value_default.string, dv,
							con_completion_color_quotes_default.string);
				}
				else
				{
					PaddedPrintMarkAndPad (false);
					Com_Printf ("&c%s%s &c%s: &c%s\"&c%s%s&c%s\"&r\n",
							con_completion_color_name.string, s , con_completion_color_colon.string,
							con_completion_color_quotes_current.string, con_completion_color_value_current.string, v,
							con_completion_color_quotes_current.string);
				}
			}
			else
			{
				PaddedPrintMarkAndPad (add_mark);
				Com_Printf ("&c%s%s &c%s: &c%s\"&c%s%s&c%s\"&r\n",
						con_completion_color_name.string, s , con_completion_color_colon.string,
						con_completion_color_quotes_current.string, con_completion_color_value_current.string, v,
						con_completion_color_quotes_current.string);
			}
			break;

		case 5:	// none
			PaddedPrintMarkAndPad (add_mark);
			Com_Printf ("&c%s%s&r\n",
					con_completion_color_name.string, s);
			break;

		default: // old completion
			PaddedPrint(s);
			break;
	}
}

static char	compl_common[64];
static int	compl_clen;
static int	compl_len;

static void FindCommonSubString (char *s) 
{
	if (!compl_clen)
	{
		strlcpy (compl_common, s, sizeof(compl_common));
		compl_clen = strlen (compl_common);
	} 
	else
	{
		while (compl_clen > compl_len && strncasecmp(s, compl_common, compl_clen))
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

void CompleteCommandNew (void)
{
	char *cmd, token[MAXCMDLINE], *s, *escaped;
	wchar temp[MAXCMDLINE];
	int c, a, v, start, end, i, diff_len, size, my_string_length;

	char completebuff[MAXCMDLINE];
	cmd_alias_t *s_a;
	cmd_function_t *s_c;
	cvar_t *s_v;
	int s_count;

	static cmd_alias_t *sorted_aliases[4096];
	static cmd_function_t *sorted_cmds[4096];
	static cvar_t *sorted_cvars[4096];
	static jogi_avail_complete_t jogi_avail_complete[4096];

#define MAX_SORTED_ALIASES (sizeof(sorted_aliases) / sizeof(sorted_aliases[0]))
#define MAX_SORTED_CVARS (sizeof (sorted_cvars) / sizeof (sorted_cvars[0]))
#define MAX_SORTED_CMDS (sizeof (sorted_cmds) / sizeof (sorted_cmds[0]))

	extern int Cmd_AliasCompare (const void *,const void *);
	extern int Cmd_CommandCompare (const void *, const void *);
	extern int Cvar_CvarCompare (const void *, const void *);

	if (!
			(key_linepos < 2
			 || isspace (key_lines[edit_line][key_linepos - 1]))
			&&  !called_second)
	{

		count = 0;
		count_cmd = 0;
		count_cvar = 0;
		count_alias = 0;
		memset(jogi_avail_complete, 0, sizeof(jogi_avail_complete));

		called_second = 1;
		if (key_linepos < 2 || isspace (key_lines[edit_line][key_linepos - 1]))
			return;

		for (start = key_linepos - 1;
				start >= 1 && !isspace (key_lines[edit_line][start]);
				start--)
			;
		if (start == 0)
			start = 1;
		if (isspace (key_lines[edit_line][start]))
			start++;
		end = key_linepos - 1;

		size = min (end - start + 1, sizeof (token) - 1);
		memcpy (token, wcs2str(&key_lines[edit_line][start]), size);
		token[size] = 0;

		s = token;
		if (*s == '\\' || *s == '/' || *s == '$')
		{
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

		if (c + a + v > 1)
		{
			Com_Printf ("\n");

			if (c)
			{
				legacycmd_t* lcmd;

				if (con_completion_format.integer) {
					Com_Printf("&c%sCommands (%d):&r\n", con_completion_color_title.string, c);
				}
				else {
					Com_Printf("\x02" "Commands (%d):\n", c);
				}

				escaped = escape_regex(s);
				snprintf(completebuff, sizeof(completebuff), "^((?i)%s)", escaped);
				Q_free(escaped);

				for (s_c = cmd_functions, s_count = 0; s_c && s_count < MAX_SORTED_CMDS; s_c = s_c->next) {
					if (Utils_RegExpMatch(completebuff, s_c->name)) {
						sorted_cmds[s_count++] = s_c;
					}
				}

				// Legacy commands
				for (lcmd = legacycmds; lcmd; lcmd = lcmd->next) {
					if (Utils_RegExpMatch(completebuff, lcmd->dummy_cmd.name)) {
						sorted_cmds[s_count++] = &lcmd->dummy_cmd;
					}
				}

				qsort(sorted_cmds, s_count, sizeof (cmd_function_t *), Cmd_CommandCompare);

				for (i = 0; i < s_count; i++) {
					PaddedPrint (sorted_cmds[i]->name);
					FindCommonSubString (sorted_cmds[i]->name);
					jogi_avail_complete[count].name = sorted_cmds[i]->name;
					jogi_avail_complete[count].type = "command";
					count++;
					count_cmd++;
				}

				if (con.x)
					Com_Printf ("\n");
			}

			if (v)
			{
				if (con_completion_format.integer)
					Com_Printf ("&c%sVariables (%d):&r\n", con_completion_color_title.string, v);
				else
					Com_Printf ("\x02" "Variables (%d):\n", v);

				escaped = escape_regex(s);
				snprintf(completebuff, sizeof(completebuff), "^((?i)%s)", escaped);
				Q_free(escaped);

				for (s_v = cvar_vars, s_count = 0; s_v && s_count < MAX_SORTED_CVARS; s_v = s_v->next) {
					if (Utils_RegExpMatch(completebuff, s_v->name)) {
						sorted_cvars[s_count++] = s_v;
					}
				}

				qsort(sorted_cvars, s_count, sizeof (cvar_t *), Cvar_CvarCompare);

				for (i = 0; i < s_count; i++) {
					PaddedPrintValue (sorted_cvars[i]->name, sorted_cvars[i]->string, sorted_cvars[i]->defaultvalue);
					FindCommonSubString (sorted_cvars[i]->name);
					jogi_avail_complete[count].name = sorted_cvars[i]->name;
					jogi_avail_complete[count].type = "variable";
					count++;
					count_cvar++;
				}

				if (con.x)
					Com_Printf ("\n");
			}

			if (a)
			{
				if (con_completion_format.integer)
					Com_Printf ("&c%sAliases (%d):&r\n", con_completion_color_title.string, a);
				else
					Com_Printf ("\x02" "Aliases (%d):\n", a);

				escaped = escape_regex(s);
				snprintf(completebuff, sizeof(completebuff), "^((?i)%s)", escaped);
				Q_free(escaped);

				for (s_a = cmd_alias, s_count = 0; s_a && s_count < MAX_SORTED_ALIASES; s_a = s_a->next) {
					if (Utils_RegExpMatch(completebuff, s_a->name)) {
						sorted_aliases[s_count++] = s_a;
					}
				}

				qsort(sorted_aliases, s_count, sizeof (cmd_alias_t *), Cmd_AliasCompare);				

				for (i = 0; i < s_count; i++) {
					PaddedPrintValue(sorted_aliases[i]->name, sorted_aliases[i]->value, sorted_aliases[i]->name);
					FindCommonSubString(sorted_aliases[i]->name);
					jogi_avail_complete[count].name = sorted_aliases[i]->name;
					jogi_avail_complete[count].type = "alias";
					count++;
					count_alias++;
				}

				if (con.x) {
					Com_Printf("\n");
				}
			}
		}

		if (c + a + v == 1)
		{
			cmd = Cmd_CompleteCommand (s);
			if (!cmd)
				cmd = Cvar_CompleteVariable (s);
			if (!cmd)
				return;	// this should never happen
		}
		else if (compl_clen)
		{
			compl_common[compl_clen] = 0;
			cmd = compl_common;
		}
		else
		{
			CompleteName ();
			return;
		}

		diff_len = strlen (cmd) - (end - start + 1);
		qwcslcpy (temp, key_lines[edit_line] + end + 1, sizeof(temp)/sizeof(temp[0]));
		qwcslcpy (key_lines[edit_line] + end + 1 + diff_len, temp, MAXCMDLINE - (end + 1 + diff_len));

		for (i = 0; start + i < MAXCMDLINE && i < strlen (cmd); i++)
		{
			key_lines[edit_line][start + i] = char2wc(cmd[i]);
		}

		key_linepos += diff_len;
		key_lines[edit_line][min
			(key_linepos + qwcslen(temp),
			 MAXCMDLINE - 1)] = 0;
		if (start == 1 && key_linepos + qwcslen(temp) < MAXCMDLINE - 1)
		{
			for (i = key_linepos + qwcslen(temp); i > 0; i--)
			{
				key_lines[edit_line][i + 1] = key_lines[edit_line][i];
			}
			key_lines[edit_line][1] = '/';
			key_linepos++;
		}
		if (c + a + v == 1 && !key_lines[edit_line][key_linepos] && key_linepos < MAXCMDLINE - 1)
		{
			key_lines[edit_line][key_linepos] = ' ';
			key_lines[edit_line][++key_linepos] = 0;
		}

		while (!(isspace (key_lines[edit_line][key_linepos])) && (key_lines[edit_line][key_linepos] != '\0'))
		{
			qwcscpy (key_lines[edit_line] + key_linepos,
					key_lines[edit_line] + key_linepos + 1);
		}

		key_lineposorig = key_linepos;
		try = 0;
		last_cmd_length = 0;
		old_keyline_length = qwcslen( key_lines[edit_line] );
	}
	else if ((key_linepos >= 2
				|| isspace (key_lines[edit_line][key_linepos - 1]))
			&&  called_second
			&& (key_linepos == key_lineposorig) && (old_keyline_length == (int) qwcslen(key_lines[edit_line])))
	{
		if (count != try)
		{
			int len;
			char text[50];
			int test;
			int testvar;
			try++;
			//Com_Printf("%i\n",try);
			test = try - 1;
			testvar = key_linepos;

			while ((testvar != 0)
					&& !(isspace (key_lines[edit_line][testvar]))
					&& (key_lines[edit_line][testvar] != '\\')
					&& (key_lines[edit_line][testvar] != '$')
					&& (key_lines[edit_line][testvar] != '/'))
			{
				testvar--;
			}

			testvar = key_linepos - testvar;

			my_string_length =	strlen (jogi_avail_complete[test].name);

			memset(text, 0, sizeof(text));

			for (i = 0; i < bound(0, my_string_length-testvar+1, sizeof(text)); i++)
			{
				text[i] = jogi_avail_complete[test].name[i + testvar -1 ];
			}

			len = strlen (text);

			memmove (key_lines[edit_line] + key_linepos + len,
					key_lines[edit_line] + key_linepos +
					last_cmd_length,
					(MAXCMDLINE - key_linepos + 1 -
					 last_cmd_length)*sizeof(wchar));
			memcpy (key_lines[edit_line] + key_linepos, str2wcs(text),
					len * sizeof(wchar));

			del_removes = 1;
			last_cmd_length = strlen (text);
		}
		else if (count == try)
		{
			try = 0;
		}
		old_keyline_length = qwcslen( key_lines[edit_line] );
	}
	else if ((key_linepos >= 2
				|| isspace (key_lines[edit_line][key_linepos - 1]))
			&& called_second
			&& (key_linepos != key_lineposorig))
	{
		try = 0;
		called_second = 0;
		del_removes = 0;
		CompleteCommandNew ();
	}
}

static void CompleteName_InsertNick(const char *name, wchar *s, wchar *p, wchar *q)
{
	int i, diff;
	wchar t[MAXCMDLINE];

	qwcslcpy(t, str2wcs(name), sizeof(t)/sizeof(t[0]));

	for (i = 0; t[i]; i++)
	{
		if ((127 & t[i]) == ' ')
		{
			int k;

			if ((k = qwcslen(t)) < MAXCMDLINE - 2)
			{
				memmove(t + 1, t, (k + 1)*sizeof(wchar));
				t[k + 2] = 0;
				t[k + 1] = t[0] = '\"';
			}
			break;
		}
	}
	diff = qwcslen(t) - qwcslen(s);

	memmove(q + diff, q, (qwcslen(q) + 1)*sizeof(wchar));
	memmove(p, t, qwcslen(t)*sizeof(wchar));
	key_linepos += diff;
	if (!key_lines[edit_line][key_linepos] && key_linepos < MAXCMDLINE - 1)
	{
		key_lines[edit_line][key_linepos] = ' ';
		key_lines[edit_line][++key_linepos] = 0;
	}
}


void CompleteName(void)
{
	extern const char disallowed_in_nick[]; // utils.c

	wchar s[MAXCMDLINE], *p, *q;
	char name[128] = {0};

	p = q = key_lines[edit_line] + key_linepos;
	while (--p >= key_lines[edit_line] + 1)
		if (!(  (*(signed short *)p >= 32) && !strchr(disallowed_in_nick, wc2char(*p)) ))
			break;
	p++;
	if (q - p <= 0)
		return;

	qwcslcpy (s, p, q - p + 1);

	if (FindBestNick(wcs2str(s), 0, name, sizeof(name)))
		CompleteName_InsertNick(name, s, p, q);
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

static qbool yellowchars = false;
qbool con_redchars    = false;


// 1) clear any typing
// 2) put what was typed in console to the history if any
void Key_ClearTyping (void)
{
	//if new input is the same as previous one or the line is empty
	// do not increment edit_line
	if((wcscmp ((wchar_t*)key_lines[edit_line], (wchar_t*)key_lines[(edit_line - 1) & (CMDLINES - 1)]))
			&& key_lines[edit_line][1])
		edit_line = (edit_line + 1) & (CMDLINES - 1);

	history_line = edit_line;
	key_lines[edit_line][0] = con_prompt_charcode.integer;
	key_lines[edit_line][1] = 0;
	key_linepos = 1;
}

void Key_Console_Backspace(void)
{
	if (key_linepos > 1)
	{
		qwcscpy(key_lines[edit_line] + key_linepos - 1, key_lines[edit_line] + key_linepos);
		key_linepos--;
	}
}

//
// Interactive line editing and console scrollback.
//
void Key_Console (int key, int unichar)
{
	int i, len;
	qbool print_in_console = true;

	switch (key)
	{
		case 'M': case 'm': case 'J': case 'j':		//^M,^J = Enter
			{
				if (!keydown[K_CTRL])
					break;
				// Fall through.
			}
		case K_ENTER:
			{
				CompleteCommandNew_Reset ();
				con_redchars = false;

				// Backslash text are commands.
				if (key_lines[edit_line][1] != '/' || key_lines[edit_line][2] != '/')
				{
					qbool no_lf = true;

					// If Ctrl+enter was pressed, regard the chat as a team message.
					if (((keydown[K_CTRL] && key == K_ENTER) || keydown[K_SHIFT]) && cls.state >= ca_connected)
					{
						// Do we have something to say?
						if (*(key_lines[edit_line] + 1)) 
						{
							Cbuf_AddText ((keydown[K_CTRL] && key == K_ENTER) ? "say_team \"" : "say \"");
							Cbuf_AddText (encode_say(key_lines[edit_line] + 1));
							Cbuf_AddText ("\"");
						}
						else
						{
							no_lf = false;									
						}
					}
					else
					{
						if (key_lines[edit_line][1] == '\\' || key_lines[edit_line][1] == '/')
						{
							Cbuf_AddText (encode_say(key_lines[edit_line] + 2));	// skip the ]/
						}
						else
						{
							// Check if it's a chat message or a command.
							if (cls.state == ca_disconnected || (cl_chatmode.value != 1 && CheckForCommand()))
							{
								Cbuf_AddText (encode_say(key_lines[edit_line] + 1));	// Valid command.
							}
							else
							{
								// It's a chat message.
								if (cls.state >= ca_connected)	// Can happen if cl_chatmode is 1
								{
									if (cl_chatmode.value == 2 || cl_chatmode.value == 1)
									{
										if (*(key_lines[edit_line] + 1)) // Do we have something to say?
										{
											Cbuf_AddText ("say \"");
											Cbuf_AddText (encode_say(key_lines[edit_line] + 1));
											Cbuf_AddText ("\"");

											if (con_hide_chat_input.integer)
												print_in_console = false;
										}
										else
										{
											no_lf = false;									
										}
									}
									else
									{
										Cbuf_AddText (encode_say(key_lines[edit_line] + 1));	// skip the ]
									}
								}
								else
								{
									no_lf = false;
								}
							}
						}
					}
					if (no_lf)
					{
						Cbuf_AddText ("\n");
					}
				}

				// cut'n'paste rocks!
				if (!print_in_console)
					Print_flags[Print_current] |= PR_SKIP;

				Con_PrintW (key_lines[edit_line]);	// FIXME logging

				// cut'n'paste rocks!
				if (!print_in_console)
					Print_flags[Print_current] |= PR_SKIP;

				Con_PrintW (str2wcs("\n"));

				Key_ClearTyping ();

				if (cls.state == ca_disconnected)
					SCR_UpdateScreen ();	// force an update, because the command
				// may take some time
				return;
			}
		case K_TAB:
			{
				// Command completion
				if (!(keydown[K_SHIFT]) && keydown[K_CTRL])
				{
					CompleteName ();
				}
				else
				{
					CompleteCommandNew ();
				}
				return;
			}
		case 'H': case 'h':		// ^H = BACKSPACE
			{
				if (!keydown[K_CTRL] || keydown[K_ALT])
				{
					break;
				}
			}
		case K_BACKSPACE:
			{
				// added by jogi start
				CompleteCommandNew_Reset();
				// added by jogi stop

				// removes word before cursor
				if(keydown[K_CTRL])
				{
					len = 0;

					while(key_linepos > 1 && key_lines[edit_line][key_linepos - 1] == ' ')
					{
						key_linepos--;
						len++;
					}

					while(key_linepos > 1 && key_lines[edit_line][key_linepos - 1] != ' ')
					{
						key_linepos--;
						len++;
					}

					// remove spaces after this word leaving only last one
					while(key_linepos < qwcslen(key_lines[edit_line]) && key_lines[edit_line][key_linepos - 1] == ' ' && key_lines[edit_line][key_linepos - 2] == ' ')
					{
						key_linepos--;
						len++;
					}

					qwcscpy(key_lines[edit_line] + key_linepos, key_lines[edit_line] + key_linepos + len);
				}
				else
				{
					Key_Console_Backspace();
				}

				// disable red chars mode if the last character was deleted
				if(key_linepos == 1 && qwcslen(key_lines[edit_line]) == 1)
					con_redchars = false;

				return;
			}
		case K_DEL:
			{
				// added by jogi start
				if((del_removes) && (key_linepos == key_lineposorig))
				{
					qwcscpy(key_lines[edit_line] + key_linepos, key_lines[edit_line] + key_linepos + last_cmd_length);

					del_removes = 0;
					called_second = 0;
					try = 0;
				}
				// added by jogi stopp

				// removes word after cursor
				if(keydown[K_CTRL])
				{
					i = key_linepos;
					len = 0;				

					while(key_linepos < qwcslen(key_lines[edit_line]) && key_lines[edit_line][i] == ' ')
					{
						i++;
						len++;
					}

					// don't remove regular chars if we removed spaces before
					if(len == 0)
					{
						while(key_linepos < qwcslen(key_lines[edit_line]) && key_lines[edit_line][i] != ' ' && key_lines[edit_line][i] != 0)
						{
							i++;
							len++;
						}

						// remove spaces after this word
						while(key_linepos < qwcslen(key_lines[edit_line]) && key_lines[edit_line][i] == ' ')
						{
							i++;
							len++;
						}
					}

					qwcscpy(key_lines[edit_line] + key_linepos, key_lines[edit_line] + key_linepos + len);
				}
				else
				{
					if(key_linepos < qwcslen(key_lines[edit_line]))
					{
						qwcscpy(key_lines[edit_line] + key_linepos, key_lines[edit_line] + key_linepos + 1);
					}
				}

				// disable red chars mode if the last character was deleted
				if (key_linepos == 1 && qwcslen(key_lines[edit_line]) == 1)
					con_redchars = false;

				return;
			}
		case K_RIGHTARROW:
			{
				if (keydown[K_CTRL])
				{
					// word right
					i = qwcslen(key_lines[edit_line]);
					while (key_linepos < i
							&& key_lines[edit_line][key_linepos] != ' ')
						key_linepos++;
					while (key_linepos < i
							&& key_lines[edit_line][key_linepos] == ' ')
						key_linepos++;
					return;
				}
				// added by jogi start
				CompleteCommandNew_Reset();
				// added by jogi stop
				if (key_linepos < qwcslen(key_lines[edit_line]))
					key_linepos++;
				return;
			}
		case K_LEFTARROW:
			{
				if (keydown[K_CTRL])
				{
					// word left
					while (key_linepos > 1
							&& key_lines[edit_line][key_linepos - 1] ==
							' ')
						key_linepos--;
					while (key_linepos > 1
							&& key_lines[edit_line][key_linepos - 1] !=
							' ')
						key_linepos--;
					return;
				}
				// addeded by jogi start
				CompleteCommandNew_Reset();
				// addeded by jogi start

				if (key_linepos > 1)
					key_linepos--;
				return;
			}
		case 'P': case 'p':		// ^P = back in history
			{
				if (!keydown[K_CTRL] || keydown[K_ALT])
				{
					break;
				}
			}
		case K_UPARROW:
			{
				if (key == K_UPARROW && keydown[K_CTRL])
				{
					AdjustConsoleHeight (-10);
					return;
				}

				do
				{
					history_line = (history_line - 1) & (CMDLINES - 1);
				} while (history_line != edit_line && !key_lines[history_line][1]);

				if (history_line == edit_line)
				{
					history_line = (edit_line + 1) & (CMDLINES - 1);
				}

				qwcscpy(key_lines[edit_line], key_lines[history_line]);
				key_linepos = qwcslen(key_lines[edit_line]);
				return;
			}
		case 'N': case 'n':		// ^N = forward in history
			{
				if (!keydown[K_CTRL] || keydown[K_ALT])
				{
					break;
				}
			}
		case K_DOWNARROW:
			{
				if (key == K_DOWNARROW && keydown[K_CTRL])
				{
					AdjustConsoleHeight (10);
					return;
				}

				if (history_line == edit_line)
				{
					return;
				}

				do
				{
					history_line = (history_line + 1) & (CMDLINES - 1);
				} while (history_line != edit_line && !key_lines[history_line][1]);

				if (history_line == edit_line)
				{
					key_lines[edit_line][0] = con_prompt_charcode.integer;
					key_lines[edit_line][1] = 0;
					key_linepos = 1;
				}
				else
				{
					qwcscpy(key_lines[edit_line], key_lines[history_line]);
					key_linepos = qwcslen(key_lines[edit_line]);
				}
				return;
			}
		case K_PGUP:
		case K_MWHEELUP:
			{
				if (keydown[K_CTRL] && key == K_PGUP)
				{
					con.display -= ((int)scr_conlines - 22) >> 3;
				}
				else
				{
					con.display -= 2;
				}

				if (con.display - con.current + con.numlines < 0)
				{
					con.display = con.current - con.numlines;
				}
				return;
			}
		case K_MWHEELDOWN:
		case K_PGDN:
			{
				if (keydown[K_CTRL] && key == K_PGDN)
				{
					con.display += ((int)scr_conlines - 22) >> 3;
				}
				else
				{
					con.display += 2;
				}

				if (con.display - con.current > 0)
				{
					con.display = con.current;
				}
				return;
			}
		case K_HOME:
			{
				if (keydown[K_CTRL])
				{
					con.display = con.current - con.numlines;
				}
				else
				{
					key_linepos = 1;
				}
				return;
			}
		case K_END:
			{
				if (keydown[K_CTRL])
				{
					con.display = con.current;
				}
				else
				{
					key_linepos = qwcslen(key_lines[edit_line]);
				}
				return;
			}
	}

	if (((key == 'V' || key == 'v') && 
#ifndef __APPLE__
				keydown[K_CTRL]
#else
				keydown[K_CMD]
#endif
				&& !keydown[K_ALT])
			|| ((key == K_INS || key == KP_INS) && keydown[K_SHIFT]))
	{
		wchar *clipText;

		if ((clipText = Sys_GetClipboardTextW()))
		{
			len = qwcslen(clipText);
			if (len + qwcslen(key_lines[edit_line]) > MAXCMDLINE - 1)
			{
				len = MAXCMDLINE - 1 - qwcslen(key_lines[edit_line]);
			}

			if (len > 0)
			{
				// Insert the string.
				memmove (key_lines[edit_line] + key_linepos + len,
						key_lines[edit_line] + key_linepos, (qwcslen(key_lines[edit_line]) - key_linepos + 1)*sizeof(wchar));
				memcpy (key_lines[edit_line] + key_linepos, clipText, len*sizeof(wchar));
				key_linepos += len;
			}
		}
		return;
	}

	if (key == 'u' && keydown[K_CTRL] && !keydown[K_ALT])
	{
		if (key_linepos > 1)
		{
			qwcscpy(key_lines[edit_line] + 1, key_lines[edit_line] + key_linepos);
			key_linepos = 1;
		}
		return;
	}

	if (!unichar)
		return;	// non printable

	if (con_funchars_mode.value)
	{
		// CTRL+y toggles yellowchars
		if (keydown[K_CTRL] && key == 'y' && !keydown[K_RALT] && !keydown[K_ALT]) {
			yellowchars = !yellowchars;
			con_redchars    = false;
			Com_Printf( "input of yellow numbers is now o%s!\n", yellowchars ? "n" : "ff" );
			return;
		}

		// CTRL+r toggles red chars
		if (keydown[K_CTRL] && key == 'r' && !keydown[K_RALT] && !keydown[K_ALT]) {
			con_redchars    = !con_redchars;
			if ( yellowchars )
			{
				Com_Printf( "input of yellow numbers is now off!\n" );
			}
			yellowchars = false;
			return;
		}
	}

	if ( yellowchars || (keydown[K_CTRL] && !(con_funchars_mode.value)))
	{
		if (unichar >= '0' && unichar <= '9')
		{
			unichar = unichar - '0' + 0x12;	// yellow number
		}
		else
		{
			switch (unichar)
			{
				case '[': unichar = 0x10; break;
				case ']': unichar = 0x11; break;
				case 'g': unichar = 0x86; break; // ctrl+g green led
				case 'r': unichar = 0x87; break; // ctrl+r red led
				case 'y': unichar = 0x88; break; // ctrl+y yellow led
				case 'b': unichar = 0x89; break; // ctrl+b blue led
				case 'w': unichar = 0x84; break; // ctrl+w white led
				case '(': unichar = 0x80; break;
				case '=': unichar = 0x81; break;
				case ')': unichar = 0x82; break;
				case 'a': unichar = 0x83; break;
				case '<': unichar = 0x1d; break;
				case '-': unichar = 0x1e; break;
				case '>': unichar = 0x1f; break;
				case ',': unichar = 0x1c; break;
				case '.': unichar = 0x9c; break;
				case 'B': unichar = 0x8b; break;
				case 'C': unichar = 0x8d; break;
			}
		}
	}

	if (con_redchars || (keydown[K_ALT] && !(con_funchars_mode.value)))
	{
		unichar |= 128;		// red char
	}

	i = qwcslen(key_lines[edit_line]);
	if (i >= MAXCMDLINE-1)
	{
		return;
	}

	// This also moves the ending \0
	memmove (key_lines[edit_line]+key_linepos+1, key_lines[edit_line]+key_linepos, (i-key_linepos+1)*sizeof(wchar));
	key_lines[edit_line][key_linepos] = unichar;
	key_linepos++;
	CompleteCommandNew_Reset ();
	lastKeyGeneratedCharacter = true;
}

//============================================================================

chat_type chat_team;

qbool chat_observers;	// added by jogi
qbool chat_server;		// added by jogi
wchar		chat_buffer[MAXCMDLINE];
int			chat_linepos = 0;

void Key_Message (int key, wchar unichar) {
	int len;

	switch (key)
	{
		case K_ENTER:
			if (chat_buffer[0])
			{
				qbool irccommand = false;
				switch (chat_team) {
					case chat_mm2: Cbuf_AddText("say_team \""); break;
					case chat_qtvtogame: 
						       if (cls.mvdplayback == QTV_PLAYBACK) {
							       Cbuf_AddText("say \"say_game ");  // QTV parses the text, no say_game command has been implemented
						       } else {
							       Cbuf_AddText("// "); // silence output to remove "unknown command" error message
							       Com_Printf("&cf00Error&r: &c090messagemodeqtvtogame&r requires you to be connected to a &c666QTV&r server\n");
						       }
						       break;
#ifdef WITH_IRC
					case chat_irc:
						       if (chat_buffer[0] == '/') {
							       irccommand = true;
							       Cbuf_AddText("irc ");	
						       }
						       else {
							       Cbuf_AddText("irc say \"");
						       }
						       break;
#endif

					default:
					case chat_mm1: Cbuf_AddText("say \""); break;
				}

				if (irccommand) {
					Cbuf_AddText(encode_say(chat_buffer+1));
				}
				else {
					Cbuf_AddText(encode_say(chat_buffer));
					Cbuf_AddText("\"\n");
				}
			}

			if (key_dest_beforemm != key_message && key_dest_beforemm != key_console)
				key_dest = key_dest_beforemm;
			else
				key_dest = key_game;

			chat_linepos = 0;
			chat_buffer[0] = 0;
			return;

		case K_ESCAPE:
			if (key_dest_beforemm != key_message && key_dest_beforemm != key_console)
				key_dest = key_dest_beforemm;
			else
				key_dest = key_game;

			chat_buffer[0] = 0;
			chat_linepos = 0;
			return;

		case K_HOME:
			chat_linepos = 0;
			return;

		case K_END:
			chat_linepos = qwcslen(chat_buffer);
			return;

		case K_LEFTARROW:
			if (chat_linepos > 0)
				chat_linepos--;
			return;

		case K_RIGHTARROW:
			if (chat_linepos < qwcslen(chat_buffer))
				chat_linepos++;
			return;

		case K_BACKSPACE:
			if (chat_linepos > 0)
			{
				qwcscpy(chat_buffer + chat_linepos - 1, chat_buffer + chat_linepos);
				chat_linepos--;
			}
			return;

		case K_DEL:
			if (chat_buffer[chat_linepos])
				qwcscpy(chat_buffer + chat_linepos, chat_buffer + chat_linepos + 1);
			return;

		case K_PGDN:
#ifdef WITH_IRC
			if (chat_team == chat_irc) {
				IRC_NextChan();
			}
#endif
			break;

		case K_PGUP:
#ifdef WITH_IRC
			if (chat_team == chat_irc) {
				IRC_PrevChan();
			}
#endif
			break;
	}

	if (((key == 'V' || key == 'v') && keydown[K_CTRL] && !keydown[K_ALT])
			|| ((key == K_INS || key == KP_INS) && keydown[K_SHIFT]))
	{
		wchar *clipText;

		if ((clipText = Sys_GetClipboardTextW()))
		{
			len = qwcslen(clipText);
			if (len + qwcslen(chat_buffer) > MAXCMDLINE - 1)
				len = MAXCMDLINE - 1 - qwcslen(chat_buffer);
			if (len > 0)
			{
				// insert the string
				memmove (chat_buffer + chat_linepos + len,
						chat_buffer + chat_linepos, (qwcslen(chat_buffer) - chat_linepos + 1)*sizeof(wchar));
				memcpy (chat_buffer + chat_linepos, clipText, len * sizeof(wchar));
				chat_linepos += len;
			}
		}
		return;
	}

	if (!unichar)
		return;	// non printable

	len = qwcslen(chat_buffer);

	if (len >= sizeof(chat_buffer)/sizeof(wchar) - 1)
		return; // all full

	// This also moves the ending \0
	memmove (chat_buffer+chat_linepos+1, chat_buffer+chat_linepos, (len - chat_linepos + 1)*sizeof(wchar));
	chat_buffer[chat_linepos++] = unichar;
	lastKeyGeneratedCharacter = true;
}

//============================================================================

//Returns a key number to be used to index keybindings[] by looking at the given string.
//Single ascii characters return themselves, while the K_* names are matched up.
#define UNKNOWN_S "UNKNOWN"

byte Key_CharacterToQuakeCode(char ch);

int Key_StringToKeynum (const char *str)
{
	keyname_t *kn;

	if (!str || !str[0])
		return -1;

	if (!str[1] && !con_bindphysical.value)
	{
		int value = Key_CharacterToQuakeCode(str[0]);

		if (value != 0)
			return value;
	}

	if (!str[1])
		return (int)(unsigned char)str[0];

	for (kn = keynames; kn->name; kn++) {
		if (!strcasecmp (str,kn->name))
			return kn->keynum;
	}

	return -1;
}

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

void Key_SetBinding (int keynum, const char *binding) {
	if (keynum == -1)
		return;

#ifndef __APPLE__
	if (keynum == K_CTRL || keynum == K_ALT || keynum == K_SHIFT || keynum == K_WIN) {
		Key_SetBinding(keynum + 1, binding);
		Key_SetBinding(keynum + 2, binding);
		return;
	}
#endif

	// free (and hence Q_free) is safe to call with a NULL argument
	Q_free (keybindings[keynum]);
	keybindings[keynum] = Q_strdup(binding);
}

void Key_Unbind (int keynum) {
	if (keynum == -1)
		return;

	if (keynum == K_CTRL || keynum == K_ALT || keynum == K_SHIFT || keynum == K_WIN) {
		Key_Unbind(keynum + 1);
		Key_Unbind(keynum + 2);
		return;
	}

	// free (and hence Q_free) is safe to call with a NULL argument
	Q_free (keybindings[keynum]);
	keybindings[keynum] = NULL;
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

	for (i = 0; i < sizeof(keybindings) / sizeof(*keybindings); i++)
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
qbool Key_IsLeftRightSameBind(int b) {
	if (b < 0 || b >= (sizeof(keybindings) / sizeof(*keybindings)) - 2)
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
#ifndef __APPLE__
		if ((b == K_CTRL || b == K_ALT || b == K_SHIFT || b == K_WIN) && (keybindings[b + 1] || keybindings[b + 2])) {

			if (keybindings[b + 1] && keybindings[b + 2] && !strcmp(keybindings[b + 1], keybindings[b + 2])) {
				Com_Printf ("\"%s\" = \"%s\"\n", Cmd_Argv(1), keybindings[b + 1]);
			} else {
				Key_PrintBindInfo(b + 1, NULL);
				Key_PrintBindInfo(b + 2, NULL);
			}
		} else
#endif
		{

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

	for (i = 0; i < (sizeof(keybindings) / sizeof(*keybindings)); i++) {
		if (Key_IsLeftRightSameBind(i)) {
			Com_Printf ("%s \"%s\"\n", Key_KeynumToString(i), keybindings[i + 1]);
			i += 2;
		} else {
			if (keybindings[i])
				Com_Printf ("%s \"%s\"\n", Key_KeynumToString(i), keybindings[i]);
		}
	}
}

void Key_BindEdit_f(void) {
	char *keybinding, final_string[MAXCMDLINE - 1];
	int keynum, argc = Cmd_Argc();

	if(argc < 2) {
		Com_Printf("%s <key> : modify a bind\n", Cmd_Argv(0));
		Com_Printf("bindlist : list all binds\n");
		return;
	}

	keynum = Key_StringToKeynum(Cmd_Argv(1));
	if(keynum == -1) {
		Com_Printf("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	keybinding = keybindings[keynum] ? keybindings[keynum] : "";
	strlcpy(final_string, "/bind \"", sizeof(final_string));
	strlcat(final_string, Cmd_Argv(1), sizeof(final_string));
	strlcat(final_string, "\" \"", sizeof(final_string));
	strlcat(final_string, keybinding, sizeof(final_string));
	strlcat(final_string, "\"", sizeof(final_string));
	Key_ClearTyping();
	key_linepos = 8 + (int)strlen(Cmd_Argv(1)) + 3; // move to where the commands are in the bind
	memcpy(key_lines[edit_line] + 1, str2wcs(final_string), (strlen(final_string) + 1) * sizeof(wchar));
}

void History_Init (void)
{
	int i, c;
	FILE *hf;

	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register (&cl_savehistory);
	Cvar_ResetCurrentGroup();

	for (i = 0; i < CMDLINES; i++) 
	{
		key_lines[i][0] = con_prompt_charcode.integer;
		key_lines[i][1] = 0;
	}
	key_linepos = 1;

	if (cl_savehistory.value)
	{
		char filename[MAX_OSPATH] = {0};

		snprintf(&filename[0], sizeof(filename), "%s/" HISTORY_FILE_NAME, com_basedir);
		if ((hf = fopen(filename, "rt")))
		{
			do
			{
				i = 1;
				do
				{
					c = fgetc(hf);
					key_lines[edit_line][i++] = c;
				} while (c != '\n' && c != EOF && i < MAXCMDLINE);
				key_lines[edit_line][i - 1] = 0;
				edit_line = (edit_line + 1) & (CMDLINES - 1);
			} while (c != EOF && edit_line < CMDLINES);
			fclose(hf);

			history_line = edit_line = (edit_line - 1) & (CMDLINES - 1);
			key_lines[edit_line][0] = con_prompt_charcode.integer;
			key_lines[edit_line][1] = 0;
		}
	}
}

void History_Shutdown (void)
{
	int i;
	FILE *hf;

	if (cl_savehistory.value)
	{
		char filename[MAX_OSPATH] = {0};

		snprintf(&filename[0], sizeof(filename), "%s/" HISTORY_FILE_NAME, com_basedir);
		if ((hf = fopen(filename, "wt")))
		{
			i = edit_line;
			do
			{
				i = (i + 1) & (CMDLINES - 1);
			} while (i != edit_line && !key_lines[i][1]);

			do
			{
				fprintf(hf, "%s\n", /*wcs2str*/encode_say(key_lines[i] + 1));
				i = (i + 1) & (CMDLINES - 1);
			} while (i != edit_line && key_lines[i][1]);
			fclose(hf);
		}
	}
}
// } Added by VVD

void Key_Init (void) {
	int i;

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
#ifdef __APPLE__
	consolekeys[K_CMD] = true;
#endif
	consolekeys[K_MWHEELUP] = true;
	consolekeys[K_MWHEELDOWN] = true;
	consolekeys[K_WIN] = true;
	consolekeys[K_LWIN] = true;
	consolekeys[K_RWIN] = true;
	consolekeys[K_MENU] = true;
	consolekeys['`'] = false;
	consolekeys['~'] = false;

	for (i = 0; i < sizeof(keyshift) / sizeof(*keyshift); i++)
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

	memset(hudeditorkeys, true, 512 * sizeof(qbool));
	hudeditorkeys[K_ALT] = true;
	hudeditorkeys[K_LALT] = true;
	hudeditorkeys[K_SHIFT] = true;
	hudeditorkeys[K_LSHIFT] = true;
	hudeditorkeys[K_CTRL] = true;
	hudeditorkeys[K_LCTRL] = true;
	hudeditorkeys['h'] = true;
	hudeditorkeys['p'] = true;
	hudeditorkeys[K_SPACE] = true;
	hudeditorkeys[K_F1] = true;
	hudeditorkeys[K_F2] = true;
	hudeditorkeys[K_F3] = true;
	hudeditorkeys[K_F4] = true;
	hudeditorkeys[K_MOUSE1] = true;
	hudeditorkeys[K_MOUSE2] = true;
	hudeditorkeys[K_MOUSE3] = true;

	memset(&scr_pointer_state, 0, sizeof(mouse_state_t));

	// register our functions
	Cmd_AddCommand("bindlist",Key_BindList_f);
	Cmd_AddCommand("bind",Key_Bind_f);
	Cmd_AddCommand("bindedit", Key_BindEdit_f);
	Cmd_AddCommand("unbind",Key_Unbind_f);
	Cmd_AddCommand("unbindall",Key_Unbindall_f);
	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register(&cl_chatmode);
	Cvar_Register(&con_funchars_mode);
	Cvar_Register(&con_tilde_mode);
	Cvar_Register(&con_completion_format);
	Cvar_Register(&con_completion_color_value_current);
	Cvar_Register(&con_completion_color_value_default);
	Cvar_Register(&con_completion_color_name);
	Cvar_Register(&con_completion_color_quotes_current);
	Cvar_Register(&con_completion_color_quotes_default);
	Cvar_Register(&con_completion_color_colon);
	Cvar_Register(&con_completion_color_changed_mark);
	Cvar_Register(&con_prompt_charcode);
	Cvar_Register(&con_hide_chat_input);
	Cvar_Register(&con_completion_padding);
	Cvar_Register(&con_completion_color_title);
	Cvar_Register(&con_completion_changed_mark);
	Cvar_Register(&con_bindphysical);
	Cvar_Register(&in_builtinkeymap);

	Cvar_ResetCurrentGroup();
}

// sends new mouse state message to active module and it's windows
// returns:
//   true: message was received and handled
//   false: message wasn't handled by any window
static qbool Mouse_EventDispatch(void)
{
	qbool mouse_handled = false;

	// Send mouse cursor status to appropriate windows
	switch (key_dest) 
	{
		case key_menu: 
			mouse_handled = Menu_Mouse_Event(&scr_pointer_state);
			break;
		case key_hudeditor: 
			mouse_handled = HUD_Editor_MouseEvent(&scr_pointer_state);
			break;
		case key_demo_controls:
			mouse_handled = DemoControls_MouseEvent(&scr_pointer_state);
			break;
		default:
			break;
			// unhandled
		case key_game:
		case key_console:
		case key_message:
			break;
	}

	return mouse_handled;
}

// called by Key_Event, updates button states
qbool Mouse_ButtonEvent(int key, qbool down)
{
	if (key >= K_MOUSE1 && key <= K_MOUSE8)
	{	// in this case we convert the button number to the range 1..8
		key = key - K_MOUSE1 + 1;   // get the button number, starting from 1
		key = bound(1, key, 8);
		scr_pointer_state.buttons[key] = down;
	}
	else if (key != K_MWHEELDOWN && key != K_MWHEELUP)
	{
		// not a mouse button received
		return false;
	}

	scr_pointer_state.button_down = down ? key : 0;
	scr_pointer_state.button_up   = down ? 0 : key;

	// report if the button event has been handled or not
	return Mouse_EventDispatch();
}

// called by cl_screen.c each time it figures out that the mouse has moved
void Mouse_MoveEvent(void)
{
	// no button has been pressed
	scr_pointer_state.button_down = 0;
	scr_pointer_state.button_up = 0;

	// the rest of scr_pointer_state has already been updated by cl_screen module

	Mouse_EventDispatch();  // so just dispatch the message with new state
}

qbool Key_IsConsoleToggle (int key)
{
	return (key == '`' || key == '~');
}

// Will tell if the key should be currently translated into a command bound to it
// or send it to some client module.
static qbool Key_ConsoleKey(int key)
{
	// This makes it possible to type chars under tilde key into the console.
	qbool con_toggle = Key_IsConsoleToggle(key);
	qbool con_key = (con_tilde_mode.integer && con_toggle && (con_tilde_mode.integer == 1 || !CONSOLE_LINE_EMPTY())) ? true : consolekeys[key];
	qbool hud_key = (con_tilde_mode.integer && con_toggle) ? true : hudeditorkeys[key];
	qbool demo_controls_key = (con_tilde_mode.integer && con_toggle) ? true : democontrolskey[key];
	qbool con_keypad_key = cl_keypad.integer && key >= KP_HOME && key <= KP_ENTER;

	if (key_dest == key_menu && Menu_ExecuteKey(key))
		return false;

	if ((key_dest == key_console || key_dest == key_message) && !con_key && !con_keypad_key)
		return false;

	if (key_dest == key_game && (cls.state == ca_active || !con_key))
		return false;

	if (key_dest == key_hudeditor && !hud_key)
		return false;

	if (key_dest == key_demo_controls && !demo_controls_key)
		return false;

	return true;
}

// Called by the system between frames for both key up and key down events Should NOT be called during an interrupt!
void Key_EventEx (int key, wchar unichar, qbool down)
{
	char *kb, cmd[1024];

	// FIXME: disconnect: really FIXME CTRL+r or CTRL+[ with in_builinkeymap 1 cause to unichar < 32 
	if (/*unichar < 32 ||*/ (unichar > 127 && unichar <= 256))
		unichar = 0;

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
	if (down) 
	{
		key_repeats[key]++;

		if (key_repeats[key] > 1) 
		{
			if ((key != K_BACKSPACE && key != K_DEL
						&& key != K_LEFTARROW && key != K_RIGHTARROW
						&& key != K_UPARROW && key != K_DOWNARROW
						&& key != K_PGUP && key != K_PGDN && (key < 32 || key > 126 || key == '`'))
					|| (key_dest == key_game && cls.state == ca_active))
			{
				return;	// ignore most autorepeats
			}
		}
	}

	// Handle escape specialy, so the user can never unbind it.
	if (key == K_ESCAPE)
	{
		if (!down)
		{
			return;
		}

		if (KeyDestStartupDemo(key_dest)) {
			Cbuf_AddText("togglemenu\n");
			return;
		}

		switch (key_dest) 
		{
			case key_message:
				Key_Message (key, unichar);
				break;
			case key_menu:
				M_Keydown (key, unichar);
				break;
			case key_game:
				M_ToggleMenu_f ();
				break;
			case key_console:
				if (!SCR_NEED_CONSOLE_BACKGROUND) {
					Con_ToggleConsole_f();
				}
				else {
					M_ToggleMenu_f();
				}
				break;
			case key_hudeditor:
				HUD_Editor_Key(key, unichar, down);
				break;
			case key_demo_controls:
				DemoControls_KeyEvent(key, unichar, down);
				break;

			default:
				assert(!"Bad key_dest");
		}

		return;
	}

	// Special case for accessing the menu via mouse when in console in disconnected mode:
	if (key == K_MOUSE2 && key_dest == key_game && cls.state == ca_disconnected)
	{
		M_ToggleMenu_f();
	}

	if (!down)
	{
		// Key up event.
		if (key_dest == key_hudeditor) 
		{
			HUD_Editor_Key(key, unichar, down);
		}
		else if (key_dest == key_demo_controls)
		{
			DemoControls_KeyEvent(key, unichar, down);
		}

		// Key up events only generate commands if the game key binding is a button command (leading + sign).
		// These will occur even in console mode, to keep the character from continuing an action started before a
		// console switch.  Button commands include the kenum as a parameter, so multiple downs can be matched with ups
		{
			kb = keybindings[key];
			if (kb)
			{
				if (Key_TryMovementProtected(kb, down, key)) {
					// this was a protected movement binding and was executed
					return;
				}

				if (kb[0] == '+' && keyactive[key]) 
				{
					snprintf (cmd, sizeof (cmd), "-%s %i\n", kb+1, key);
					Cbuf_AddText (cmd);
					keyactive[key] = false;
				}
			}
			if (keyshift[key] != key) 
			{
				kb = keybindings[keyshift[key]];
				if (kb && kb[0] == '+' && keyactive[keyshift[key]]) 
				{
					snprintf (cmd, sizeof (cmd), "-%s %i\n", kb+1, key);
					Cbuf_AddText (cmd);
					keyactive[keyshift[key]] = false;
				}
			}
		}

		return;
	}

	// switch windowed<->fullscreen if pressed alt+enter, I succeed only with left alt, dunno why...
	if (key == K_ENTER && !sys_disable_alt_enter.integer && keydown[K_ALT] && (key_dest == key_console || key_dest == key_game))
	{
		Key_ClearStates(); // Zzzz
		con_suppress = true;
		Cvar_SetValue( &r_fullscreen, !r_fullscreen.integer );
		Cbuf_AddText( "vid_restart\n" );
		Cbuf_Execute();
		con_suppress = false;
		return;
	}

	// if not a consolekey, send to the interpreter no matter what mode is
	if (!Key_ConsoleKey(key)) 
	{
		kb = keybindings[key];

		if (kb) 
		{
			if (Key_TryMovementProtected(kb, down, key)) {
				// this was a protected movement binding and was executed
				return;
			}

			if (kb[0] == '+')
			{	
				// Button commands add keynum as a parm.
				snprintf (cmd, sizeof (cmd), "%s %i\n", kb, key);
				Cbuf_AddText (cmd);
				keyactive[key] = true;
			} 
			else 
			{
				Cbuf_AddText (kb);
				Cbuf_AddText ("\n");
			}
		}

		return;
	}

	if (!down) {
		return;	 // Other systems only care about key down events.
	}

#ifdef _WIN32
	{
		extern cvar_t con_toggle_deadkey;

		if (Key_IsConsoleToggle (key) && con_toggle_deadkey.integer) {
			Sys_CancelDeadKey ();
		}
	}
#endif

	if (keydown[K_SHIFT])
		key = keyshift[key];

	// Key down event.
	switch (key_dest) 
	{
		case key_message:
			Key_Message (key, unichar);
			break;

		case key_menu:
			M_Keydown (key, unichar);
			break;

		case key_game:
		case key_console:
			Key_Console (key, unichar);
			break;

		case key_hudeditor:
			HUD_Editor_Key(key, unichar, down);
			break;

		case key_demo_controls:
			DemoControls_KeyEvent(key, unichar, down);
			break;

		case key_startupdemo_browser:
			Cbuf_AddText("menu_slist\n");
			break;
		case key_startupdemo_console:
		case key_startupdemo_game:
			key_dest = key_console;
			break;
		case key_startupdemo_menu:
			Cbuf_AddText("togglemenu\n");
			break;

		default:
			assert(!"Bad key_dest");
	}
}

void Key_Event (int key, qbool down)
{
	qbool processTextInput = (key_dest == key_console || key_dest == key_message);
	qbool consoleToggle = (key == '`' || key == '~') && !con_tilde_mode.integer;
	wchar unichar = 0;

	assert (key >= 0 && key <= 255);

	if (key >= K_MOUSE1 && key <= K_MOUSE8)
	{
		// if the Mouse_ButtonEvent return true means that the window which received
		// a mouse click handled it and we do not have to send old
		// K_MOUSE* key event
		if (Mouse_ButtonEvent(key, down)) 
			return;
	}

	if (key == K_MWHEELDOWN || key == K_MWHEELUP) {
		// same logic applies here as for handling K_MOUSE1..8 buttons
		if (Mouse_ButtonEvent(key, down)) 
			return;
	}

	// Caps lock shouldn't trigger bind in chat/console
	if (key == K_CAPSLOCK && processTextInput) {
		return;
	}

	lastKeyGeneratedCharacter = false;
	unichar = keydown[K_SHIFT] ? keyshift[key] : key;
	if (unichar < 32 || unichar > 127)
		unichar = 0;

	Key_EventEx (key, unichar, down);

	// Store this as we may need it for subsequent SDL_TextInput event
	lastKeyDown = 0;
	if (down && processTextInput && !consoleToggle)
		lastKeyDown = key;
}

void Key_Event_TextInput(wchar unichar)
{
	if (in_builtinkeymap.value)
		return;
	if (!lastKeyDown)
		return;

	if (key_dest == key_message)
	{
		if (lastKeyGeneratedCharacter)
			Key_Message(K_BACKSPACE, K_BACKSPACE);
		Key_Message(lastKeyDown, unichar);
	}
	else if (key_dest == key_console)
	{
		if (lastKeyGeneratedCharacter)
			Key_Console_Backspace();
		Key_Console(lastKeyDown, unichar);
	}
}

void Key_ClearStates (void) 
{
	int		i;

	for (i = 0; i < sizeof(keydown) / sizeof(*keydown); i++) 
	{
		if (keydown[i])
			Key_Event(i, false);

		keydown[i] = false;
		key_repeats[i] = false;
	}
}

void OnChange_con_prompt_charcode (cvar_t *var, char *string, qbool *cancel)
{	
	int i, charcode = Q_atoi(string);
	*cancel = true;

	if ((charcode > 31) && (charcode <= 255) && (charcode != con_prompt_charcode.integer))
	{
		// changes prompt in current line
		key_lines[edit_line][0] = charcode;

		// changes prompt in all lines
		for (i = 0; i < CMDLINES; i++)
		{
			if (qwcslen(key_lines[i]) > 1)
				key_lines[i][0] = charcode;
		}

		*cancel = false;
	}
}

void OnChange_con_completion_color (cvar_t *var, char *string, qbool *cancel)
{
	if (!Utils_RegExpMatch("^[0-9a-fA-F]{3}$", string))
		*cancel = true;
}

char* escape_regex(char* string)
{
	// TODO: Rename and move this to a more appropriate place so other functions may use it (utils.c ?)
	int i, j, len;
	char *out = "";

	len = strlen(string);
	out = (char*) Q_malloc((len * 2 + 1) * sizeof(char));

	for(i = 0, j = 0; i < len; i++)
	{
		switch(string[i])
		{
			case '+':
			case '.':
			case '[':
			case ']':
				out[j++] = '\\';
				out[j++] = string[i];
				break;
			default:
				out[j++] = string[i];
				break;
		}
	}

	out[j++] = '\0';

	return out;
}

void Key_Shutdown(void)
{
	int i;

	for (i = 0; i < sizeof(keybindings) / sizeof(keybindings[0]); ++i) {
		Q_free(keybindings[i]);
	}
}
