/*
Copyright (C) 2001-2002 A Nourai

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but	WITHOUT ANY WARRANTY; without even the implied warranty	of
MERCHANTABILITY or FITNESS FOR A PARTICULARPURPOSE.

See the included (GNU.txt) GNU General Public License for more details.

You	should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

    $Id: config_manager.c,v 1.49 2007-10-11 05:55:47 dkure Exp $
*/

#include "quakedef.h"
#include "input.h"
#include "utils.h"
#include "keys.h"
#include "fs.h"
#include "config_manager.h"
#include "version.h"


char *Key_KeynumToString (int keynum);

void DumpSkyGroups(FILE *f);
qbool Key_IsLeftRightSameBind(int b);
void DumpMapGroups(FILE *f);
void TP_DumpTriggers(FILE *);
void TP_DumpMsgFilters(FILE *f);
void TP_ResetAllTriggers(void);
qbool Cmd_DeleteAlias (char *);
void DumpFlagCommands(FILE *);
int Cvar_CvarCompare (const void *p1, const void *p2);
int Cmd_AliasCompare (const void *p1, const void *p2);

extern void WriteSourcesConfiguration(FILE *f);
void MarkDefaultSources(void);

extern cvar_group_t *cvar_groups;
extern cmd_alias_t *cmd_alias;
extern cvar_t *cvar_vars;

extern kbutton_t	in_mlook, in_klook,	in_left, in_right, in_forward, in_back;
extern kbutton_t	in_lookup, in_lookdown,	in_moveleft, in_moveright;
extern kbutton_t	in_strafe, in_speed, in_use, in_jump, in_attack, in_up,	in_down;

extern qbool		sb_showscores, sb_showteamscores;

extern cvar_t		cl_teamtopcolor, cl_teambottomcolor, cl_enemytopcolor, cl_enemybottomcolor;
extern char		allskins[128];

cvar_t	cfg_save_unchanged	=	{"cfg_save_unchanged", "0"};
cvar_t	cfg_save_userinfo	=	{"cfg_save_userinfo", "2"};
cvar_t	cfg_save_onquit		=	{"cfg_save_onquit", "0"};
cvar_t	cfg_save_cvars		=	{"cfg_save_cvars", "1"};
cvar_t	cfg_save_aliases	=	{"cfg_save_aliases", "1"};
cvar_t	cfg_save_cmds		=	{"cfg_save_cmds", "1"};
cvar_t	cfg_save_binds		=	{"cfg_save_binds", "1"};
cvar_t	cfg_save_sysinfo	=	{"cfg_save_sysinfo", "0"};
cvar_t	cfg_save_cmdline	=	{"cfg_save_cmdline", "1"};
cvar_t	cfg_backup			=	{"cfg_backup", "0"};
cvar_t	cfg_legacy_exec		=	{"cfg_legacy_exec", "1"};
cvar_t  cfg_use_home		=	{"cfg_use_home", "0"};
cvar_t  cfg_use_gamedir		=	{"cfg_use_gamedir", "0"};

/************************************ DUMP FUNCTIONS ************************************/

#define BIND_ALIGN_COL 20
void DumpBindings (FILE *f)
{
	int i, leftright;
	char *spaces, *string;
	qbool printed = false;

	fprintf(f, "unbindall\n");
	for (i = 0; i < (sizeof(keybindings) / sizeof(*keybindings)); i++) {

		leftright = Key_IsLeftRightSameBind(i) ? 1 : 0;
		if (keybindings[i] || leftright) {
			printed = true;
			string = Key_KeynumToString(i);
			spaces = CreateSpaces(BIND_ALIGN_COL - strlen(string) - 6);
			if (i == ';')
				fprintf (f, "bind  \";\"%s\"%s\"\n", spaces, keybindings[i]);
			else
				fprintf (f, "bind  %s%s\"%s\"\n", string, spaces, keybindings[leftright ? i + 1 : i]);

			if (leftright)
				i += 2;
		}
	}
	if (!printed)
		fprintf(f, "//no bindings\n");
}

static qbool Config_Unsaved_Cvar(const char *name)
{
	return (!strcmp(name, "cl_delay_packet"))
        || (!strcmp(name, "cl_proxyaddr"))
        || (!strcmp(name, "hud_planmode"))
        || (!strcmp(name, "con_bindphysical"));
}

#define CONFIG_MAX_COL 60
#define MAX_DUMPED_CVARS 4096
static void DumpVariables(FILE	*f)
{
	cvar_t *var, *sorted_vars[MAX_DUMPED_CVARS];
	cvar_group_t *group;
	char *spaces;
	int count, i, col_size;
	qbool skip_userinfo = false;



	for (col_size = 0,  var = cvar_vars; var; var = var->next) {
		if ( !(
		            (var->flags & (CVAR_USER_CREATED | CVAR_ROM | CVAR_INIT)) ||
		            (var->group && (!strcmp(CVAR_GROUP_NO_GROUP, var->group->name) || !strcmp(CVAR_GROUP_SERVERINFO, var->group->name)) )
		        )) {
			col_size = max(col_size, strlen(var->name));
		}
	}
	col_size = min(col_size + 2, CONFIG_MAX_COL);


	if (cfg_save_unchanged.value) {
		fprintf(f, "//All variables (even those with default values) are listed below.\n");
		fprintf(f, "//You can use \"cfg_save_unchanged 0\" to save only changed variables.\n\n");
	} else {
		fprintf(f, "//Only variables with non-default values are listed below.\n");
		fprintf(f, "//You can use \"cfg_save_unchanged 1\" to save all variables.\n\n");
	}


	for (group = cvar_groups; group; group = group->next)  {

		if (
		    !strcmp(CVAR_GROUP_NO_GROUP, group->name) ||
		    !strcmp(CVAR_GROUP_SERVERINFO, group->name) ||
		    (!cfg_save_userinfo.value && !strcmp(CVAR_GROUP_USERINFO, group->name))
		)
			continue;

		skip_userinfo = ((cfg_save_userinfo.value == 1) && !strcmp(CVAR_GROUP_USERINFO, group->name)) ? true : false;


		for (count = 0, var = group->head; var && count < MAX_DUMPED_CVARS; var = var->next_in_group) {
			if (skip_userinfo && (
			            !strcmp(var->name, "team") || !strcmp(var->name, "skin") ||
			            !strcmp(var->name, "spectator") ||!strcmp(var->name, "name") ||
			            !strcmp(var->name, "topcolor") || !strcmp(var->name, "bottomcolor")
			        ))
				continue;
			
			if (Config_Unsaved_Cvar(var->name)) {
				continue;
			}
			
			if (!(var->flags & (CVAR_USER_CREATED |	CVAR_ROM | CVAR_INIT))) {
				if (cfg_save_unchanged.value || strcmp(var->string, var->defaultvalue)) {
					sorted_vars[count++] = var;
				}
			}
		}
		if (!count)
			continue;


		if (
		    strcmp(group->name, CVAR_GROUP_ITEM_NAMES) &&
		    strcmp(group->name, CVAR_GROUP_ITEM_NEED) &&
		    strcmp(group->name, CVAR_GROUP_USERINFO) &&
		    strcmp(group->name, CVAR_GROUP_SKIN)
		)
			qsort(sorted_vars, count, sizeof (cvar_t *), Cvar_CvarCompare);


		fprintf(f, "//%s\n", group->name);


		for (i = 0; i < count; i++) {
			var = sorted_vars[i];
			if (cfg_save_unchanged.value || strcmp(var->string, var->defaultvalue)) {
				spaces = CreateSpaces(col_size - strlen(var->name));
				fprintf(f, "%s%s\"%s\"\n", var->name, spaces, var->string);
			}
		}

		fprintf(f, "\n");
	}



	for (count = 0, var = cvar_vars; var && count < MAX_DUMPED_CVARS; var = var->next) {
		if (!var->group && !(var->flags & (CVAR_USER_CREATED | CVAR_ROM | CVAR_INIT))) {
			if (cfg_save_unchanged.value || strcmp(var->string, var->defaultvalue)) {
				sorted_vars[count++] = var;
			}
		}
	}
	if (count) {

		qsort(sorted_vars, count, sizeof (cvar_t *), Cvar_CvarCompare);


		fprintf(f, "//Unsorted Variables\n");


		for (i = 0; i < count; i++) {
			var = sorted_vars[i];
			spaces = CreateSpaces(col_size - strlen(var->name));
			fprintf(f, "%s%s\"%s\"\n", var->name, spaces, var->string);
		}

		fprintf(f, "\n");
	}



	for	(col_size = col_size - 2, count = 0, var = cvar_vars; var && count < MAX_DUMPED_CVARS; var = var->next)	{
		if (var->flags & CVAR_USER_CREATED) {
			sorted_vars[count++] = var;
			col_size = max(col_size, strlen(var->name));
		}
	}
	if (!count)
		return;

	col_size = min(col_size + 2, CONFIG_MAX_COL);


	qsort(sorted_vars, count, sizeof (cvar_t *), Cvar_CvarCompare);


	fprintf(f, "//User Created Variables\n");


	for (i = 0; i < count; i++) {
		var = sorted_vars[i];

		spaces = CreateSpaces(col_size - strlen(var->name) - 5);
		fprintf	(f, "%s %s%s\"%s\"\n", (var->teamplay) ? "set_tp" : "set", var->name, spaces, var->string);
	}
}

void DumpVariablesDefaults_f(void)
{
	cvar_t *var;
	cvar_group_t *group;
    char filepath[MAX_PATH];
    FILE *f;
	qbool anyUngrouped = false;

    snprintf(filepath, sizeof(filepath), "%s/ezquake/configs/cvar_defaults.cfg", com_basedir);

    f = fopen(filepath, "w");
    if (!f)
    {
        Com_Printf("Couldn't open %s for writing\n", filepath);
        return;
    }

	for (group = cvar_groups; group; group = group->next) {
	    fprintf(f, "\n//%s\n", group->name);

        for (var = group->head; var; var = var->next_in_group) {
			fprintf(f, "%s \"%s\"\n", var->name, var->defaultvalue);
		}
	}

	// Add those without
	for (var = cvar_vars; var; var = var->next) {
		if (!var->group && !(var->flags & (CVAR_USER_CREATED | CVAR_ROM | CVAR_INIT))) {
			if (!anyUngrouped)
				fprintf(f, "\n//\n");
			fprintf(f, "%s \"%s\"\n", var->name, var->defaultvalue);
			anyUngrouped = true;
		}
	}

    if (fclose(f))
        Com_Printf("Couldn't close %s", filepath);
    else
        Com_Printf("Variables default values dumped to:\n%s\n", filepath);
}

#define MAX_ALIGN_COL 60
static void DumpAliases(FILE *f)
{
	int maxlen, i, j, count, lonely_count, minus_index, minus_count;
	char *spaces;
	cmd_alias_t	*b, *a, *sorted_aliases[1024], *lonely_pluses[512];
	qbool partner, printed;

	fprintf(f, "unaliasall\n");
	for (count = maxlen = 0, a = cmd_alias;	(count < (sizeof(sorted_aliases) / sizeof(*sorted_aliases))) && a; a = a->next) {
		if (!(a->flags & (ALIAS_SERVER|ALIAS_TEMP))) {
			maxlen = max(maxlen, strlen(a->name));
			count++;
		}
	}

	if (!count) {
		fprintf(f, "//no aliases\n");
		return;
	}

	for (i = 0, a = cmd_alias; i < count; a = a->next) {
		if (!(a->flags & (ALIAS_SERVER|ALIAS_TEMP)))
			sorted_aliases[i++] = a;
	}

	qsort(sorted_aliases, count, sizeof (cmd_alias_t *), Cmd_AliasCompare);



	for (minus_index = -1, minus_count = i = 0; i < count; i++) {
		a = sorted_aliases[i];

		if (a->name[0] == '-') {
			if (minus_index == -1)
				minus_index = i;
			minus_count++;
		} else if (a->name[0] != '+')
			break;
	}

	printed = false;

	for (lonely_count = i = 0; i < count; i++) {
		a = sorted_aliases[i];

		if (a->name[0] != '+')
			break;

		for (partner = false, j = minus_index; j >= 0 && j < minus_index + minus_count; j++) {
			b = sorted_aliases[j];

			if (!strcasecmp(b->name + 1, a->name + 1)) {

				spaces = CreateSpaces(maxlen + 3 - strlen(a->name));
				fprintf	(f, "alias %s%s\"%s\"\n", a->name, spaces, a->value);
				spaces = CreateSpaces(maxlen + 3 - strlen(b->name));
				fprintf	(f, "alias %s%s\"%s\"\n", b->name, spaces, b->value);
				printed = partner = true;
				break;
			}
		}

		if (!partner)
			lonely_pluses[lonely_count++] = a;
	}


	for (i = 0; i < lonely_count; i++) {
		a = lonely_pluses[i];

		spaces = CreateSpaces(maxlen + 3 - strlen(a->name));
		fprintf	(f, "alias %s%s\"%s\"\n", a->name, spaces, a->value);
		printed = true;
	}


	for (i = minus_index; i >= 0 && i < minus_index + minus_count; i++) {
		a = sorted_aliases[i];

		for (partner = false, j = 0; j < minus_index; j++) {
			b = sorted_aliases[j];

			if (!strcasecmp(b->name + 1, a->name + 1)) {

				partner = true;
				break;
			}
		}
		if (!partner) {

			spaces = CreateSpaces(maxlen + 3 - strlen(a->name));
			fprintf	(f, "alias %s%s\"%s\"\n", a->name, spaces, a->value);
			printed = true;
		}
	}
	for (i = (minus_index == -1 ? 0 : minus_index + minus_count); i < count; i++) {
		a = sorted_aliases[i];

		if (minus_index != -1 || a->name[0] != '+') {
			if (printed)
				fprintf(f, "\n");
			printed = false;
			spaces = CreateSpaces(maxlen + 3 - strlen(a->name));
			fprintf	(f, "alias %s%s\"%s\"\n", a->name, spaces, a->value);
		}
	}
}

static void DumpPlusCommand(FILE *f, kbutton_t	*b,	const char *name)
{
	if (b->state & 1 && b->down[0] < 0)
		fprintf(f, "+%s\n",	name);
	else
		fprintf(f, "-%s\n",	name);
}

static void DumpPlusCommands(FILE *f)
{
	DumpPlusCommand(f, &in_up, "moveup");
	DumpPlusCommand(f, &in_down, "movedown");
	DumpPlusCommand(f, &in_left, "left");
	DumpPlusCommand(f, &in_right, "right");
	DumpPlusCommand(f, &in_forward,	"forward");
	DumpPlusCommand(f, &in_back, "back");
	DumpPlusCommand(f, &in_lookup, "lookup");
	DumpPlusCommand(f, &in_lookdown, "lookdown");
	DumpPlusCommand(f, &in_strafe, "strafe");
	DumpPlusCommand(f, &in_moveleft, "moveleft");
	DumpPlusCommand(f, &in_moveright, "moveright");
	DumpPlusCommand(f, &in_speed, "speed");
	DumpPlusCommand(f, &in_attack, "attack");
	DumpPlusCommand(f, &in_use,	"use");
	DumpPlusCommand(f, &in_jump, "jump");
	DumpPlusCommand(f, &in_klook, "klook");
	DumpPlusCommand(f, &in_mlook, "mlook");

	fprintf(f, sb_showscores ? "+showscores\n" : "-showscores\n");
	fprintf(f, sb_showteamscores ? "+showteamscores\n" : "-showteamscores\n");
}

static void DumpTeamplay(FILE *f)
{

	if (allskins[0])
		fprintf(f, "allskins \"%s\"\n", allskins);

	fprintf(f, "\n");

	DumpFlagCommands(f);

	fprintf(f, "\n");
	TP_DumpMsgFilters(f);

	fprintf(f, "\n");
	TP_DumpTriggers(f);
}

void DumpFloodProtSettings(FILE *f)
{
	extern int fp_messages, fp_persecond, fp_secondsdead;
	fprintf(f, "floodprot %d %d %d\n", fp_messages, fp_persecond, fp_secondsdead);
}

void DumpMisc(FILE *f)
{

	DumpMapGroups(f);
	fprintf(f, "\n");

	DumpSkyGroups(f);
	fprintf(f, "\n");

	DumpFloodProtSettings(f);
	fprintf(f, "\n");

	fprintf(f, "hud_recalculate\n\n");

	if (cl.teamfortress) {
		if (!strcasecmp(Info_ValueForKey (cls.userinfo, "ec"), "on") ||
		        !strcasecmp(Info_ValueForKey (cls.userinfo, "exec_class"), "on")
		   ) {
			fprintf(f, "setinfo ec on\n");
		}
		if (!strcasecmp(Info_ValueForKey (cls.userinfo, "em"), "on") ||
		        !strcasecmp(Info_ValueForKey (cls.userinfo, "exec_map"), "on")
		   ) {
			fprintf(f, "setinfo em on\n");
		}
	}
}

void DumpCmdLine(FILE *f)
{
	fprintf(f, "// %s\n", cl_cmdline.string);
}

void DumpHUD262(FILE *f)
{
	hud_element_t *elem;
	char *type;
	char *param;
	int i, mask;
	extern hud_element_t *hud_list;

	elem = hud_list;

	for(i = 0; elem; elem = elem->next, i++) {
		if (elem->flags & HUD_CVAR) {
			type = "cvar";
			param = ((cvar_t*)elem->contents)->name;
		} else if (elem->flags & HUD_STRING) {
			type = "str";
			param = elem->contents;
		} else {
			continue;
		}

		mask = (elem->flags & HUD_BLINK_F) ? 1 : 0 + (elem->flags & HUD_BLINK_B) ? 2 : 0;
		fprintf(f, "hud262_add %s %s %s\n", elem->name, type, param);
		fprintf(f, "hud262_alpha %s %1.2f\n", elem->name, elem->alpha);
		fprintf(f, "hud262_bg %s %d\n", elem->name, elem->coords[3]);
		fprintf(f, "hud262_blink %s %d %d\n", elem->name, (int) ceil(elem->blink * 1000.0), mask);
		fprintf(f, "hud262_%s %s\n", (elem->flags & HUD_ENABLED) ? "enable" : "disable", elem->name);
		fprintf(f, "hud262_position %s %d %d %d\n", elem->name, elem->coords[0], elem->coords[1], elem->coords[2]);
		fprintf(f, "hud262_width %s %d\n", elem->name, elem->width);
	}
}

/************************************ RESET FUNCTIONS ************************************/

static void ResetVariables(int cvar_flags, qbool userinfo)
{
	cvar_t *var;
	qbool check_userinfos = false;

	if (userinfo) {
		if (!cfg_save_userinfo.value)
			cvar_flags |= CVAR_USERINFO;
		else if (userinfo && cfg_save_userinfo.value == 1)
			check_userinfos = true;
	}

	for (var = cvar_vars; var; var = var->next) {
		if (!(
		            (var->flags & (cvar_flags | CVAR_ROM | CVAR_INIT | CVAR_USER_CREATED | CVAR_NO_RESET)) ||
		            (var->group && !strcmp(var->group->name, CVAR_GROUP_NO_GROUP))
		        )) {
			if (check_userinfos && (
			            !strcmp(var->name, "team") || !strcmp(var->name, "skin") ||
			            !strcmp(var->name, "spectator") ||!strcmp(var->name, "name") ||
			            !strcmp(var->name, "topcolor") || !strcmp(var->name, "bottomcolor")
			        ))
				continue;
			Cvar_ResetVar(var);
		}
	}
}

static void DeleteUserAliases(void)
{
	cmd_alias_t *a, *next;

	for (a = cmd_alias; a; a = next) {
		next = a->next;

		if (!(a->flags & (ALIAS_SERVER|ALIAS_TEMP)))
			Cmd_DeleteAlias(a->name);
	}
}

static void DeleteUserVariables(void)
{
	cvar_t *var, *next;

	for (var = cvar_vars; var; var = next) {
		next = var->next;

		if (var->flags & CVAR_USER_CREATED)
			Cvar_Delete(var->name);
	}

}

static void ResetPlusCommands(void)
{
	Cbuf_AddText("-moveup;-movedown\n");
	Cbuf_AddText("-left;-right\n");
	Cbuf_AddText("-forward;-back\n");
	Cbuf_AddText("-lookup;-lookdown\n");
	Cbuf_AddText("-strafe\n");
	Cbuf_AddText("-moveleft;-moveright\n");
	Cbuf_AddText("-speed\n");
	Cbuf_AddText("-attack\n");
	Cbuf_AddText("-use\n");
	Cbuf_AddText("-jump\n");
	Cbuf_AddText("-klook\n");
	Cbuf_AddText("-mlook\n");

	Cbuf_AddText("-showscores\n");
	Cbuf_AddText("-showteamscores\n");
}

void ResetBinds(void)
{
	Key_Unbindall_f();

	Key_SetBinding(K_ESCAPE, "togglemenu");
	Key_SetBinding('`',      "toggleconsole");
#ifdef __APPLE__
	Key_SetBinding('~',      "toggleconsole");
#endif
	Key_SetBinding(K_PAUSE,  "toggleproxymenu");

	Key_SetBinding(K_MOUSE1, "+attack");
	Key_SetBinding(K_CTRL,   "+attack");
	Key_SetBinding(K_MOUSE2, "+jump");
	Key_SetBinding(K_SPACE,  "+jump");

	Key_SetBinding('w',      "+forward");
	Key_SetBinding('s',      "+back");
	Key_SetBinding('a',      "+moveleft");
	Key_SetBinding('d',      "+moveright");
	Key_SetBinding('e',      "impulse 10");
	Key_SetBinding('q',      "impulse 12");
	Key_SetBinding('t',      "messagemode");
	Key_SetBinding('y',      "messagemode2");
	Key_SetBinding('1',		 "impulse 1");
	Key_SetBinding('2',		 "impulse 2");
	Key_SetBinding('3',		 "impulse 3");
	Key_SetBinding('4',		 "impulse 4");
	Key_SetBinding('5',		 "impulse 5");
	Key_SetBinding('6',		 "impulse 6");
	Key_SetBinding('7',		 "impulse 7");
	Key_SetBinding('8',		 "impulse 8");
	Key_SetBinding('9',		 "impulse 9");
	Key_SetBinding('0',		 "impulse 10");

	Key_SetBinding(K_ALT,    "+zoom");
	Key_SetBinding(K_TAB,    "+showscores");

	Key_SetBinding('r',      "tp_msgreport");
	Key_SetBinding('z',      "tp_msgtook");
	Key_SetBinding('x',		 "tp_msgsafe");
	Key_SetBinding('c',      "tp_msghelp");
	Key_SetBinding('v',      "tp_msgpoint");

	// user will get confusing warnings on these if he joins a server where these do not exist
	// maybe some wrappers should be made for these commands which would
	// before calling them check if they exist
    Key_SetBinding('f',      "shownick");
	Key_SetBinding(K_F1,     "yes");
	Key_SetBinding(K_F2,     "agree");
	Key_SetBinding(K_F3,     "ready");
	Key_SetBinding(K_F4,     "break");
	Key_SetBinding(K_F5,     "join");
	Key_SetBinding(K_F6,     "observe");
	Key_SetBinding(K_F10,    "quit");
	Key_SetBinding(K_F12,    "screenshot");
}

static void ResetTeamplayCommands(void)
{
	allskins[0]	= 0;
	Cbuf_AddText("enemycolor off\nteamcolor	off\n");
	Cbuf_AddText("filter clear\n");
	TP_ResetAllTriggers();
	Cbuf_AddText("tp_took default\ntp_pickup default\ntp_point default\n");
}

static void ResetMiscCommands(void)
{
	Cbuf_AddText("mapgroup clear\n");
	Cbuf_AddText("skygroup clear\n");

	MarkDefaultSources();

	Info_RemoveKey(cls.userinfo, "ec");
	Info_RemoveKey(cls.userinfo, "exec_class");
	Info_RemoveKey(cls.userinfo, "em");
	Info_RemoveKey(cls.userinfo, "exec_map");
}

/************************************ PRINTING FUNCTIONS ************************************/

#define CONFIG_WIDTH 100

static void Config_PrintBorder(FILE *f)
{
	char buf[CONFIG_WIDTH + 1] = {0};

	if (!buf[0]) {
		memset(buf, '/', CONFIG_WIDTH);
		buf[CONFIG_WIDTH] = 0;
	}
	fprintf(f, "%s\n", buf);
}

static void Config_PrintLine(FILE *f, char *title, int width)
{
	char buf[CONFIG_WIDTH + 1] = {0};
	int title_len, i;

	width = bound(1, width, CONFIG_WIDTH << 3);

	for (i = 0; i < width; i++)
		buf[i] = buf[CONFIG_WIDTH - 1 - i] = '/';
	memset(buf + width,  ' ', CONFIG_WIDTH - 2 * width);
	if (strlen(title) > CONFIG_WIDTH - (2 * width + 4))
		title = "Config_PrintLine : TITLE TOO BIG";
	title_len = strlen(title);
	memcpy(buf + width + ((CONFIG_WIDTH - title_len - 2 * width) >>	1),	title, title_len);
	buf[CONFIG_WIDTH] = 0;
	fprintf(f, "%s\n", buf);
}

static void Config_PrintHeading(FILE *f, char *title)
{
	Config_PrintBorder(f);
	Config_PrintLine(f, "", 2);
	Config_PrintLine(f, title, 2);
	Config_PrintLine(f, "", 2);
	Config_PrintBorder(f);
	fprintf(f, "\n\n");
}

static void Config_PrintPreamble(FILE *f)
{
	extern cvar_t name;
	char	*newlines = "\n";

	Config_PrintBorder(f);
	Config_PrintBorder(f);
	Config_PrintLine(f, "", 3);
	Config_PrintLine(f, "", 3);
	Config_PrintLine(f, "E Z Q U A K E   C O N F I G U R A T I O N", 3);
	Config_PrintLine(f, "", 3);
	Config_PrintLine(f, "", 3);
	Config_PrintBorder(f);
	Config_PrintBorder(f);
	fprintf(f,"\n// %s's config\n\n", name.string);
	fprintf(f,"// ezQuake %s " __DATE__ ", " __TIME__"\n", VersionString());

	if (cfg_save_cmdline.value) {
		DumpCmdLine(f);
		fprintf(f, "%s", newlines);
		}
}

/************************************ MAIN FUCTIONS	************************************/

static void ResetConfigs(qbool resetall, qbool read_legacy_configs)
{
	vfsfile_t *v;

	ResetVariables(CVAR_SERVERINFO, !resetall);
	DeleteUserAliases();
	DeleteUserVariables();
	ResetBinds();
	ResetPlusCommands();
	ResetTeamplayCommands();
	ResetMiscCommands();

	if (read_legacy_configs)
	{
		Cbuf_AddText ("cl_warncmd 0\n");
		if ((v = FS_OpenVFS("autoexec.cfg", "rb", FS_ANY))) {
			Cbuf_AddText ("exec autoexec.cfg\n");
			VFS_CLOSE(v);
		}
		Cbuf_AddText ("cl_warncmd 1\n");
	}
}

void DumpConfig(char *name)
{
	FILE	*f;
	char	*outfile, *newlines = "\n";

	if (cfg_use_home.integer) // homedir
		outfile = va("%s/%s/%s", com_homedir, (strcmp(com_gamedirfile, "qw") == 0 || !cfg_use_gamedir.integer) ? "" : com_gamedirfile, name);
	else // basedir
		outfile = va("%s/%s/configs/%s", com_basedir, (strcmp(com_gamedirfile, "qw") == 0 || !cfg_use_gamedir.integer) ? "ezquake" : com_gamedirfile, name);

	if (!(f	= fopen	(outfile, "w"))) {
		FS_CreatePath(outfile);
		if (!(f	= fopen	(outfile, "w"))) {
			Com_Printf ("Couldn't write	%s.\n",	name);
			return;
		}
	}

	Com_Printf("Saving configuration to %s\n", outfile);

	Config_PrintPreamble(f);

	if (cfg_save_cvars.value) {
		Config_PrintHeading(f, "V A R I A B L E S");
		DumpVariables(f);
		fprintf(f, "%s", newlines);
	}

	if (cfg_save_cmds.value) {
		Config_PrintHeading(f, "S E L E C T E D   S O U R C E S");
		WriteSourcesConfiguration(f);
		fprintf(f, "%s", newlines);
	}

	if (cfg_save_aliases.value) {
		Config_PrintHeading(f, "A L I A S E S");
		DumpAliases(f);
		fprintf(f, "%s", newlines);
	}

	if (cfg_save_cmds.value) {
		Config_PrintHeading(f, "Q W 2 6 2   H U D");
		DumpHUD262(f);
		fprintf(f, "%s", newlines);

		Config_PrintHeading(f, "T E A M P L A Y   C O M M A N D S");
		DumpTeamplay(f);
		fprintf(f, "%s", newlines);

		Config_PrintHeading(f, "M I S C E L L A N E O U S   C O M M A N D S");
		DumpMisc(f);
		fprintf(f, "%s", newlines);

		Config_PrintHeading(f, "P L U S   C O M M A N D S");
		DumpPlusCommands(f);
		fprintf(f, "%s", newlines);
	}

	if (cfg_save_binds.value) {
		Config_PrintHeading(f, "K E Y   B I N D I N G S");
		DumpBindings(f);
	}

	fclose(f);
}

void DumpHUD(const char *name)
{
	// Dumps all variables from CFG_GROUP_HUD into a file
	extern cvar_t scr_newHud;

	FILE *f;
	int max_width = 0, i = 0, j;
	char *outfile, *spaces;
	cvar_t *var;
	cvar_t *sorted[MAX_DUMPED_CVARS];

	outfile = va("%s/ezquake/configs/%s", com_basedir, name);
	if (!(f	= fopen	(outfile, "w"))) {
		FS_CreatePath(outfile);
		if (!(f	= fopen	(outfile, "w"))) {
			Com_Printf ("Couldn't write	%s.\n",	name);
			return;
		}
	}

	fprintf(f, "//\n");
	fprintf(f, "// Head Up Display Configuration Dump\n");
	fprintf(f, "//\n\n");

	for(var = cvar_vars; var; var = var->next)
		if(var->group && !strcmp(var->group->name, CVAR_GROUP_HUD)) {
			max_width = max(max_width, strlen(var->name));
			sorted[i++] = var;
		}

	max_width++;
	qsort(sorted, i, sizeof(cvar_t *), Cvar_CvarCompare);

	spaces = CreateSpaces(max_width - strlen(scr_newHud.name));
	fprintf(f, "%s%s\"%d\"\n", scr_newHud.name, spaces, scr_newHud.integer);

	for(j = 0; j < i; j++) {
		spaces = CreateSpaces(max_width - strlen(sorted[j]->name));
		fprintf(f, "%s%s\"%s\"\n", sorted[j]->name, spaces, sorted[j]->string);
	}

	fprintf(f, "hud_recalculate\n");

	fclose(f);
}
/************************************ API ************************************/

extern qbool filesystemchanged; // fix bug 2359900

void SaveConfig(const char *cfgname)
{
	char filename[MAX_PATH] = {0}, *filename_ext, *backupname_ext;
	size_t len;
	FILE *f;

	snprintf(filename, sizeof(filename) - 4, "%s", cfgname[0] ? cfgname : MAIN_CONFIG_FILENAME); // use config.cfg if no params was specified

	COM_ForceExtensionEx (filename, ".cfg", sizeof (filename));

	if (cfg_backup.integer) {
		if (cfg_use_home.integer)	// homedir
			filename_ext = va("%s/%s/%s", com_homedir, (strcmp(com_gamedirfile, "qw") == 0 || !cfg_use_gamedir.integer) ? "" : com_gamedirfile, filename);
		else	// basedir
			filename_ext = va("%s/%s/configs/%s", com_basedir, (strcmp(com_gamedirfile, "qw") == 0 || !cfg_use_gamedir.integer) ? "ezquake" : com_gamedirfile, filename);

		if ((f = fopen(filename_ext, "r"))) {
			fclose(f);
			len = strlen(filename_ext) + 5;
			backupname_ext = (char *) Q_malloc(len);
			snprintf (backupname_ext, len, "%s.bak", filename_ext);

			if ((f = fopen(backupname_ext, "r"))) {
				fclose(f);
				remove(backupname_ext);
			}

			rename(filename_ext, backupname_ext);
			Q_free(backupname_ext);
		}
	}

	DumpConfig(filename);
	filesystemchanged = true; // fix bug 2359900
}

void SaveConfig_f(void)
{
	SaveConfig(COM_SkipPath(Cmd_Argv(1)));
}

void Config_QuitSave(void)
{
	if (cfg_save_onquit.integer) {
		SaveConfig("");
	}
}

void ResetConfigs_f(void)
{
	int argc = Cmd_Argc();
	qbool read_legacy_configs = argc == 2 && !strcmp(Cmd_Argv(1), "full");

	if (argc != 1 && !read_legacy_configs) {
		Com_Printf("Usage: %s [full]\n",	Cmd_Argv(0));
		return;
	}
	Com_Printf("Resetting configuration to default state...\n");

	ResetConfigs(true, read_legacy_configs);
}

// well exec /home/qqshka/ezquake/config.cfg does't work, security or something, so adding this
// so this is some replacement for exec
qbool LoadCfg(FILE *f)
{
	char *fileBuffer;
	char reset_bindphysical[128];
    int size;

	if (!f) {
		return false;
	}

	size = FS_FileLength(f);
	fileBuffer = Q_malloc(size + 1); // +1 for null terminator
	if (fread(fileBuffer, 1, size, f) != size) {
		Com_Printf("Error reading config file\n");
		Q_free(fileBuffer);
		return false;
	}
	fileBuffer[size] = 0;

	sprintf(reset_bindphysical, "\ncon_bindphysical %d\n", con_bindphysical.integer);
	Cbuf_AddText ("con_bindphysical 1\n");
	Cbuf_AddText (fileBuffer);
	Cbuf_AddText (reset_bindphysical);
	Q_free(fileBuffer);
	return true;
}

/*
	example how it works
	
	=== ./ezquake -game testmod -config testcfg
	homedir/testmod/testcfg.cfg (fullname)
	homedir/testcfg.cfg (fullname_moddefault)
	quakedir/testmod/configs/testcfg.cfg
	quakedir/ezquake/configs/testcfg.cfg
	built-in ezquake config
*/
void LoadConfig_f(void)
{
	FILE	*f = NULL;
	char	filename[MAX_PATH] = {0},
			fullname[MAX_PATH] = {0},
			fullname_moddefault[MAX_PATH] = {0},
			*arg1;
	int		use_home;

	arg1 = COM_SkipPathWritable(Cmd_Argv(1));

	snprintf(filename, sizeof(filename) - 4, "%s", arg1[0] ? arg1 : MAIN_CONFIG_FILENAME); // use config.cfg if no params was specified

	COM_ForceExtensionEx (filename, ".cfg", sizeof (filename));
	use_home = cfg_use_home.integer || !host_everything_loaded;

	// home
	snprintf(fullname, sizeof(fullname), "%s/%s%s", com_homedir, (strcmp(com_gamedirfile, "qw") == 0) ? "" : va("%s/", com_gamedirfile), filename);
	snprintf(fullname_moddefault, sizeof(fullname_moddefault), "%s/%s", com_homedir, filename);

	if(use_home && !((f = fopen(fullname, "rb")) && cfg_use_gamedir.integer) && !(f = fopen(fullname_moddefault, "rb")))
	{
		use_home = false;
	}

	// basedir
	snprintf(fullname, sizeof(fullname), "%s/%s/configs/%s", com_basedir, (strcmp(com_gamedirfile, "qw") == 0) ? "ezquake" : com_gamedirfile, filename);
	snprintf(fullname_moddefault, sizeof(fullname_moddefault), "%s/ezquake/configs/%s", com_basedir, filename);

	if(!use_home && !((f = fopen(fullname, "rb")) && cfg_use_gamedir.integer) && !(f = fopen(fullname_moddefault, "rb")))
	{
		Com_Printf("Couldn't load %s %s\n", filename, (cfg_use_gamedir.integer) ? "(using gamedir search)": "(not using gamedir search)");
		return;
	}

	con_suppress = true;
	ResetConfigs(false, true);
	con_suppress = false;

	if(use_home)
		Com_Printf("Loading %s%s (Using Home Directory) ...\n", (strcmp(com_gamedirfile, "qw") == 0) ? "" : va("%s/",com_gamedirfile), filename);
	else
		Com_Printf("Loading %s/configs/%s ...\n", (strcmp(com_gamedirfile, "qw") == 0) ? "ezquake" : com_gamedirfile, filename);
	
	Cbuf_AddText ("cl_warncmd 0\n");

	LoadCfg(f);
	fclose(f);

	/* johnnycz:
	  This should be called with TP_ExecTrigger("f_cfgload"); but definition
	  of f_cfgload alias is stored in config which is waiting to be executed
	  in command queue so nothing would happen. We have to add f_cfgload as
	  a standard command to the queue. Since warnings are off this is OK but
	  regarding to other f_triggers non-standard.
	*/
	Cbuf_AddText ("f_cfgload\n");

	Cbuf_AddText ("cl_warncmd 1\n");
}

void DumpHUD_f(void)
{
	char *filename;
	char buf[MAX_PATH];

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <filename>\n", Cmd_Argv(0));
		return;
	}
	filename = COM_SkipPathWritable(Cmd_Argv(1));
	strlcpy(buf, filename, sizeof(buf));
	COM_ForceExtensionEx (buf, ".cfg", sizeof(buf));
	DumpHUD(buf);
	Com_Printf("HUD variables exported to %s\n",buf);
}

void Config_TroubleShoot_Tip(const char* problem, const char* description,
							 const char* solution, int priority)
{
	con_ormask = 128;
	switch (priority) {
	case 0:
		Com_Printf("Minor issue: ");
		break;
	case 1: default:
		Com_Printf("Major issue: ");
		break;
	}
	con_ormask = 0;

	Com_Printf("%s\n", problem);

	con_margin = 2;
	Com_Printf("%s\n\n", description);
	con_margin = 0;

	con_ormask = 128;
	Com_Printf("Solution: ");
	con_ormask = 0;

	con_margin = 2;
	Com_Printf("%s\n\n", solution);
	con_margin = 0;
}

void Config_TroubleShoot_f(void)
{
	unsigned problems = 0;
	extern cvar_t r_novis, r_swapInterval;
	extern cvar_t m_filter, sys_yieldcpu, cl_maxfps, hud_planmode;
	extern cvar_t in_raw;

	if (r_novis.value) {
		Config_TroubleShoot_Tip("r_novis is enabled",
			"r_novis causes a major performance hit, it's only useful if you need transparent liquids "
			"on non-vised maps",
			"set r_novis to 0", 1);
		problems++;
	}

	if (m_filter.value) {
		Config_TroubleShoot_Tip("m_filter is enabled",
			"m_filter causes a serious delay in processing of the mouse input data",
			"set m_filter to 0", 1);
		problems++;
	}

	if (cl_maxfps.integer == 0 && sys_yieldcpu.value == 0 && r_swapInterval.value == 0) {
		Config_TroubleShoot_Tip("cl_maxfps, sys_yieldcpu, and vid_vsync are all 0",
			"unlimited FPS with CPU yielding disabled typically leads to interruptions "
			"in reading input devices (keyboard, mouse)",
			"either set sys_yieldcpu 1, vid_vsync 1, or or limit your FPS with cl_maxfps", 1);
		problems++;
	}

	if (cl_maxfps.integer == 0 && sys_yieldcpu.value == 0) {
		Config_TroubleShoot_Tip("cl_maxfps and sys_yieldcpu are 0",
			"unlimited FPS with CPU yielding disabled typically leads to interruptions "
			"in reading input devices (keyboard, mouse)",
			"either set sys_yieldcpu 1 or or limit your FPS with cl_maxfps", 1);
		problems++;
	}

	if (in_raw.integer != 1) {
		Config_TroubleShoot_Tip("in_raw is not set to 1",
			"in_raw 3 enables Raw Input, the smoothest mouse input method. "
			"It is the recommended setting for mouse input",
			"set in_raw to 1", 0);
		problems++;
	}

	if (hud_planmode.value) {
		Config_TroubleShoot_Tip("hud_planmode is enabled",
			"hud_planmode turns on the display of all possible head up display elements "
			"so that you can fine-tune your head up display settings",
			"set hud_planmode to 0", 1);
	}
	if (!problems) {
		Com_Printf("No problems detected. For more help, please visit the forum in %s\n\n", CharsToBrownStatic("www.QuakeWorld.nu"));
	}
}

void Config_LegacyQuake_f(void)
{
	qbool specific = Cmd_Argc() > 1;
	const char *ver = NULL;
	
	if (specific) {
		ver = Cmd_Argv(1);
	}
	
	if (!specific || (strcmp("2.1", ver) == 0)) {
		Cbuf_AddText(
			"hide itemsclock;echo hiding the itemsclock hud element (undo: show itemsclock);"
			"\n"
			);
	}

	if (!specific || (strcmp("2.1", ver) == 0)) {
		Cbuf_AddText(
			"cl_physfps_spectator 0;turning off spectator smoothing (undo: cl_physfps_spectator 30);"
			"\n"
			);
	}

	if (!specific || (strcmp("1.9", ver) == 0)) {
		Cbuf_AddText(
			"hide ownfrags;echo hiding the ownfrags hud element (undo: show ownfrags);"
			"menu_ingame 0;echo turning off ingame menu (undo: menu_ingame 1);"
			"gl_powerupshells 0;echo turning off powerup shell (undo: gl_powerupshells 1);"
			"r_drawvweps 0;echo turning off vweps (undo: r_drawvweps 1);"
			"\n"
			);
	}

	if (!specific || (strcmp("1.8", ver) == 0)) {
		Cbuf_AddText(
			"r_tracker_messages 0;echo turning off tracker messages (undo: r_tracker_messages);"
			"frags extra_spec_info 0;echo disabling extra spec info for frags hud element (undo: frags extra_spec_info 1);"
			"teamfrags extra_spec_info 0;echo disabling extra spec info for teamfrags hud element (undo: teamfrags extra_spec_info 1);"
			"hide radar;echo hiding the radar hud element (undo: show rader);"
			"\n"
		// "r_chaticons_alpha 0;echo chaticon drawing disabled;"
		);
	}
}

void ConfigManager_Init(void)
{
	Cmd_AddCommand("cfg_save", SaveConfig_f);
	Cmd_AddCommand("cfg_load", LoadConfig_f);
	Cmd_AddCommand("cfg_reset",	ResetConfigs_f);
	Cmd_AddCommand("hud_export", DumpHUD_f);
	Cmd_AddCommand("dump_defaults", DumpVariablesDefaults_f);
	Cmd_AddCommand("legacyquake", Config_LegacyQuake_f);
	Cmd_AddCommand("troubleshoot", Config_TroubleShoot_f);

	Cvar_SetCurrentGroup(CVAR_GROUP_CONFIG);
	Cvar_Register(&cfg_save_unchanged);
	Cvar_Register(&cfg_save_userinfo);
	Cvar_Register(&cfg_save_onquit);
	Cvar_Register(&cfg_save_cvars);
	Cvar_Register(&cfg_save_aliases);
	Cvar_Register(&cfg_save_cmds);
	Cvar_Register(&cfg_save_binds);
	Cvar_Register(&cfg_save_cmdline);
	Cvar_Register(&cfg_save_sysinfo);
	Cvar_Register(&cfg_backup);
	Cvar_Register(&cfg_legacy_exec);
	Cvar_Register(&cfg_use_home);
	Cvar_Register(&cfg_use_gamedir);
	Cvar_ResetCurrentGroup();
}
