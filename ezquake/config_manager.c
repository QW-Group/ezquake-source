/*

Copyright (C) 2001-2002		  A	Nourai

This program is	free software; you can redistribute	it and/or
modify it under	the	terms of the GNU General Public	License
as published by	the	Free Software Foundation; either version 2
of the License,	or (at your	option)	any	later version.

This program is	distributed	in the hope	that it	will be	useful,
but	WITHOUT	ANY	WARRANTY; without even the implied warranty	of
MERCHANTABILITY	or FITNESS FOR A PARTICULAR	PURPOSE.  

See	the	included (GNU.txt) GNU General Public License for more details.

You	should have	received a copy	of the GNU General Public License
along with this	program; if	not, write to the Free Software
Foundation,	Inc., 59 Temple	Place -	Suite 330, Boston, MA  02111-1307, USA.
*/

#include "quakedef.h"
#include "input.h"
#include "utils.h"
#include "teamplay.h"

void Key_WriteBindings (FILE *);
char *Key_KeynumToString (int keynum);
qboolean Key_IsLeftRightSameBind(int b);
void DumpMapGroups(FILE *f);
void TP_DumpTriggers(FILE *);
void TP_DumpMsgFilters(FILE *f);
void TP_ResetAllTriggers(void);
qboolean Cmd_DeleteAlias (char *);
void DumpFlagCommands(FILE *);
void Cvar_ResetVar (cvar_t *var);
int Cvar_CvarCompare (const void *p1, const void *p2);
int Cmd_AliasCompare (const void *p1, const void *p2);

extern char	*keybindings[256];

extern cvar_group_t *cvar_groups;
extern cmd_alias_t *cmd_alias;
extern cvar_t *cvar_vars;

extern kbutton_t	in_mlook, in_klook,	in_left, in_right, in_forward, in_back;
extern kbutton_t	in_lookup, in_lookdown,	in_moveleft, in_moveright;
extern kbutton_t	in_strafe, in_speed, in_use, in_jump, in_attack, in_up,	in_down;

extern qboolean		sb_showscores, sb_showteamscores;

extern int		cl_teamtopcolor, cl_teambottomcolor, cl_enemytopcolor, cl_enemybottomcolor;
extern char		allskins[128];

cvar_t	cfg_save_unchanged	=	{"cfg_save_unchanged", "0"};
cvar_t	cfg_save_userinfo	=	{"cfg_save_userinfo", "2"};
cvar_t	cfg_legacy_exec		=	{"cfg_legacy_exec", "1"};
cvar_t	cfg_legacy_write	=	{"cfg_legacy_write", "0"};

cvar_t	cfg_save_cvars		=	{"cfg_save_cvars", "1"};
cvar_t	cfg_save_aliases	=	{"cfg_save_aliases", "1"};
cvar_t	cfg_save_cmds		=	{"cfg_save_cmds", "1"};
cvar_t	cfg_save_binds		=	{"cfg_save_binds", "1"};
cvar_t	cfg_save_sysinfo	=	{"cfg_save_sysinfo", "0"};
cvar_t	cfg_save_cmdline	=	{"cfg_save_cmdline", "1"};

cvar_t	cfg_backup			=	{"cfg_backup", "0"};

/************************************ DUMP FUNCTIONS ************************************/

#define BIND_ALIGN_COL 20
void DumpBindings (FILE *f) {
	int i, leftright;
	char *spaces, *string;
	qboolean printed = false;

	for (i = 0; i < 256; i++) {
		
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

#define CONFIG_MAX_COL 60
static void DumpVariables(FILE	*f)	{
	cvar_t *var, *sorted_vars[256];
	cvar_group_t *group;
	char *spaces;
	int count, i, col_size;
	qboolean skip_userinfo = false;



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

	
		for (count = 0, var = group->head; var && count < 256; var = var->next_in_group) {
			if (skip_userinfo && (
				!strcmp(var->name, "team") || !strcmp(var->name, "skin") || 
				!strcmp(var->name, "spectator") ||!strcmp(var->name, "name") ||
				!strcmp(var->name, "topcolor") || !strcmp(var->name, "bottomcolor")
			))
				continue;
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
	


	for (count = 0, var = cvar_vars; var && count < 256; var = var->next) {
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



	for	(col_size = col_size - 2, count = 0, var = cvar_vars; var && count < 256; var = var->next)	{
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
		fprintf	(f, "%s %s%s\"%s\"\n", (var->flags & CVAR_USER_ARCHIVE) ? "seta" : "set ", var->name, spaces, var->string);
	}
}

#define MAX_ALIGN_COL 60
static void DumpAliases(FILE *f) {
	int maxlen, i, j, count, lonely_count, minus_index, minus_count;
	char *spaces;
	cmd_alias_t	*b, *a, *sorted_aliases[512], *lonely_pluses[512];
	qboolean partner, printed;

	for (count = maxlen = 0, a = cmd_alias;	count < sizeof(sorted_aliases) && a; a = a->next) {
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

			if (!Q_strcasecmp(b->name + 1, a->name + 1)) {
			
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

			if (!Q_strcasecmp(b->name + 1, a->name + 1)) {
			
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

static void DumpPlusCommand(FILE *f, kbutton_t	*b,	const char *name) {
	if (b->state & 1 && b->down[0] < 0)
		fprintf(f, "+%s\n",	name);
	else
		fprintf(f, "-%s\n",	name);
}

static void DumpPlusCommands(FILE *f) {
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

static void DumpColorForcing(FILE *f, char *name, int topcolor, int bottomcolor) {
	char *spaces;

	spaces = CreateSpaces(13 - strlen(name));
	if (topcolor < 0) {
		if (cfg_save_unchanged.value)
			fprintf	(f,	"%s%soff\n", name, spaces);
	} else {
		if (topcolor != bottomcolor)
			fprintf	(f,	"%s%s%i %i\n", name, spaces, topcolor, bottomcolor);
		else
			fprintf	(f,	"%s%s%i\n", name, spaces, topcolor);
	}
}

static void DumpTeamplay(FILE *f) {

	if (allskins[0])
		fprintf(f, "allskins \"%s\"\n", allskins);

	DumpColorForcing(f, "teamcolor", cl_teamtopcolor, cl_teambottomcolor);
	DumpColorForcing(f, "enemycolor", cl_enemytopcolor, cl_enemybottomcolor);

	fprintf(f, "\n");

	DumpFlagCommands(f);

	fprintf(f, "\n");
	TP_DumpMsgFilters(f);

	fprintf(f, "\n");
	TP_DumpTriggers(f);
}

void DumpMisc(FILE *f) {

	DumpMapGroups(f);

	fprintf(f, "\n");



	if (cl.teamfortress) {
		if (!Q_strcasecmp(Info_ValueForKey (cls.userinfo, "ec"), "on") || 
			!Q_strcasecmp(Info_ValueForKey (cls.userinfo, "exec_class"), "on")
		) {
			fprintf(f, "setinfo ec on\n");
		}
		if (!Q_strcasecmp(Info_ValueForKey (cls.userinfo, "em"), "on") || 
			!Q_strcasecmp(Info_ValueForKey (cls.userinfo, "exec_map"), "on")
		) {
			fprintf(f, "setinfo em on\n");
		}
	}
}

void DumpCmdLine(FILE *f) {
	fprintf(f, "// %s\n", cl_cmdline.string);
}

/************************************ RESET FUNCTIONS ************************************/

static void ResetVariables(int cvar_flags, qboolean userinfo) {
	cvar_t *var;
	qboolean check_userinfos = false;

	if (userinfo) {
		if (!cfg_save_userinfo.value)
			cvar_flags |= CVAR_USERINFO;
		else if (userinfo && cfg_save_userinfo.value == 1)
			check_userinfos = true;
	}

	for (var = cvar_vars; var; var = var->next) {
		if (!(
			(var->flags & (cvar_flags | CVAR_ROM | CVAR_INIT | CVAR_USER_CREATED)) ||
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

static void DeleteUserAliases(void)	{
	cmd_alias_t	*a;

	for	(a = cmd_alias;	a; a = a->next)	{
		if (!(a->flags & (ALIAS_SERVER|ALIAS_TEMP)))
			Cmd_DeleteAlias(a->name);
	}
}

static void DeleteUserVariables(void) {
	cvar_t *var;

	for (var = cvar_vars; var; var = var->next) {
		if (var->flags & CVAR_USER_CREATED)
			Cvar_Delete(var->name);
	}

}

static void ResetPlusCommands(void) {
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

static void ResetTeamplayCommands(void) {
	allskins[0]	= 0;
	Cbuf_AddText("enemycolor off\nteamcolor	off\n");
	Cbuf_AddText("filter clear\n");
	TP_ResetAllTriggers();
	Cbuf_AddText("tp_took default\ntp_pickup default\ntp_point default\n");
}

static void ResetMiscCommands(void) {
	Cbuf_AddText("mapgroup clear\n");

	Info_RemoveKey(cls.userinfo, "ec");
	Info_RemoveKey(cls.userinfo, "exec_class");
	Info_RemoveKey(cls.userinfo, "em");
	Info_RemoveKey(cls.userinfo, "exec_map");
}

/************************************ PRINTING FUNCTIONS ************************************/

#define CONFIG_WIDTH 100

static void Config_PrintBorder(FILE *f) {
	char buf[CONFIG_WIDTH + 1] = {0};

	if (!buf[0]) {
		memset(buf, '/', CONFIG_WIDTH);
		buf[CONFIG_WIDTH] = 0;
	}
	fprintf(f, "%s\n", buf);
}

static void Config_PrintLine(FILE *f, char *title, int width) {
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

static void Config_PrintHeading(FILE *f, char *title) {
	Config_PrintBorder(f);
	Config_PrintLine(f, "", 2);
	Config_PrintLine(f, title, 2);
	Config_PrintLine(f, "", 2);
	Config_PrintBorder(f);
	fprintf(f, "\n\n");
}

static void Config_PrintPreamble(FILE *f) {
	Config_PrintBorder(f);
	Config_PrintBorder(f);
	Config_PrintLine(f, "", 3);
	Config_PrintLine(f, "", 3);
	Config_PrintLine(f, "F U H Q U A K E   C O N F I G U R A T I O N", 3);
	Config_PrintLine(f, "", 3);
	Config_PrintLine(f, "http://www.fuhquake.net/", 3);
	Config_PrintLine(f, "", 3);
	Config_PrintLine(f, "", 3);
	Config_PrintBorder(f);
	Config_PrintBorder(f);
	fprintf(f, "\n\n\n");
}

/************************************ MAIN FUCTIONS	************************************/

static void ResetConfigs(qboolean resetall)	{
	FILE *f;

	ResetVariables(CVAR_SERVERINFO, !resetall);

	DeleteUserAliases();					

	DeleteUserVariables();					

	Cbuf_AddText("unbindall\n");			

	ResetPlusCommands();

	ResetTeamplayCommands();

	ResetMiscCommands();


	Cbuf_AddText ("cl_warncmd 0\n");
	Cbuf_AddText ("exec default.cfg\n");
	if (FS_FOpenFile("autoexec.cfg", &f) != -1) {
		Cbuf_AddText ("exec autoexec.cfg\n");
		fclose(f);
	}
	Cbuf_AddText ("cl_warncmd 1\n");
}

void DumpConfig(char *name)	{
	FILE	*f;
	char	*outfile, *newlines = "\n\n\n\n";

	outfile = va("%s/fuhquake/configs/%s", com_basedir, name);
	if (!(f	= fopen	(outfile, "w"))) {
		COM_CreatePath(outfile);
		if (!(f	= fopen	(outfile, "w"))) {
			Com_Printf ("Couldn't write	%s.\n",	name);
			return;
		}
	}

	Config_PrintPreamble(f);

	if (cfg_save_cmdline.value) {
		Config_PrintHeading(f, "C O M M A N D   L I N E");
		DumpCmdLine(f);
		fprintf(f, newlines);
	}

	if (cfg_save_cvars.value) {
		Config_PrintHeading(f, "V A R I A B L E S");
		DumpVariables(f);
		fprintf(f, newlines);
	}

	if (cfg_save_aliases.value) {
		Config_PrintHeading(f, "A L I A S E S");
		DumpAliases(f);
		fprintf(f, newlines);
	}

	if (cfg_save_cmds.value) {
		Config_PrintHeading(f, "T E A M P L A Y   C O M M A N D S");
		DumpTeamplay(f);
		fprintf(f, newlines);


	#ifdef GLQUAKE
		Config_PrintHeading(f, "M I S C E L L A N E O U S   C O M M A N D S");
		DumpMisc(f);
		fprintf(f, newlines);
	#endif

		Config_PrintHeading(f, "P L U S   C O M M A N D S");
		DumpPlusCommands(f);
		fprintf(f, newlines);
	}

	if (cfg_save_binds.value) {
		Config_PrintHeading(f, "K E Y   B I N D I N G S");
		DumpBindings(f);
	}

	fclose(f);
}

/************************************ API ************************************/

void SaveConfig_f(void)	{
	char *filename, *filename_ext, *backupname_ext;
	FILE *f;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <filename>\n", Cmd_Argv(0));
		return;
	}

	filename = COM_SkipPath(Cmd_Argv(1));
	COM_ForceExtension(filename, ".cfg");


	if (cfg_backup.value) {
		filename_ext = va("%s/fuhquake/configs/%s", com_basedir, filename);
		if ((f = fopen(filename_ext, "r"))) {
			fclose(f);
			backupname_ext = Z_Malloc(strlen(filename_ext) + 4);
			strcpy(backupname_ext, filename_ext);
			strcat(backupname_ext, ".bak");
			if ((f = fopen(backupname_ext, "r"))) {
				fclose(f);
				remove(backupname_ext);
			}
			rename(filename_ext, backupname_ext);
			Z_Free(backupname_ext);
		}
	}

	DumpConfig(filename);
	Com_Printf("Saving configuration to %s\n", filename);
}

void ResetConfigs_f(void) {
	if (Cmd_Argc() != 1) {
		Com_Printf("Usage: %s \n",	Cmd_Argv(0));
		return;
	}
	Com_Printf("Resetting configuration to default state...\n");
	ResetConfigs(true);
}

void LoadConfig_f(void)	{
	FILE *f;
	char *filename;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <filename>\n", Cmd_Argv(0));
		return;
	}
	filename = COM_SkipPath(Cmd_Argv(1));
	COM_ForceExtension(filename, ".cfg");
	if (!(f = fopen(va("%s/fuhquake/configs/%s", com_basedir, filename), "r"))) {
		Com_Printf("Couldn't load config %s\n", filename);
		return;
	}
	fclose(f);

	con_suppress = true;
	ResetConfigs(false);
	con_suppress = false;

	Com_Printf("Loading config %s ...\n", filename);


	Cbuf_AddText ("cl_warncmd 0\n");
	Cbuf_AddText(va("exec configs/%s\n", filename));
	Cbuf_AddText ("cl_warncmd 1\n");
}

void ConfigManager_Init(void) {
	Cmd_AddCommand("cfg_save", SaveConfig_f);
	Cmd_AddCommand("cfg_load", LoadConfig_f);
	Cmd_AddCommand("cfg_reset",	ResetConfigs_f);

	Cvar_SetCurrentGroup(CVAR_GROUP_CONFIG);
	Cvar_Register(&cfg_save_unchanged);
	Cvar_Register(&cfg_save_userinfo);
	Cvar_Register(&cfg_legacy_exec);
	Cvar_Register(&cfg_legacy_write);
	Cvar_Register(&cfg_save_cvars);
	Cvar_Register(&cfg_save_aliases);
	Cvar_Register(&cfg_save_cmds);
	Cvar_Register(&cfg_save_binds);
	Cvar_Register(&cfg_save_cmdline);
	Cvar_Register(&cfg_save_sysinfo);
	Cvar_Register(&cfg_backup);

	Cvar_ResetCurrentGroup();
}
