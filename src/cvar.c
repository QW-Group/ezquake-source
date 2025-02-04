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
// cvar.c -- dynamic variable tracking

#ifdef SERVERONLY
#include "qwsvdef.h"

#define VAR_HASHPOOL_SIZE 32
#else
#include "common.h"
#include "teamplay.h"
#include "rulesets.h"
#include "keys.h"
#include "utils.h"
#include "parser.h"
#include "r_renderer.h"

static void Cvar_ApplyLatchedUpdate(cvar_t* var);

void CL_UserinfoChanged(char *key, char *value);
void SV_ServerinfoChanged(char *key, char *value);
void Help_DescribeCvar(cvar_t *v);
void VID_ReloadCvarChanged(cvar_t* var);

extern cvar_t r_fullbrightSkins;
extern cvar_t cl_fakeshaft;
extern cvar_t allow_scripts;

static cvar_t cvar_viewdefault = {"cvar_viewdefault", "1"};
static cvar_t cvar_viewhelp    = {"cvar_viewhelp",    "1"};
static cvar_t cvar_viewlatched = {"cvar_viewlatched", "1"};

#define VAR_HASHPOOL_SIZE 1024

static void Cvar_AddCvarToGroup(cvar_t *var);
#endif

static cvar_t *cvar_hash[VAR_HASHPOOL_SIZE];
cvar_t *cvar_vars;
static char	*cvar_null_string = "";

// Use this to walk through all vars
cvar_t* Cvar_Next(cvar_t *var)
{
	if (var) {
		return var->next;
	}
	else {
		return cvar_vars;
	}
}

cvar_t *Cvar_Find(const char *var_name)
{
	cvar_t *var;
	int key = Com_HashKey (var_name) % VAR_HASHPOOL_SIZE;

	for (var = cvar_hash[key]; var; var = var->hash_next) {
		if (!strcasecmp(var_name, var->name)) {
			return var;
		}
	}

	return NULL;
}

float Cvar_Value (const char *var_name)
{
	cvar_t *var = Cvar_Find (var_name);

	if (!var) {
		return 0;
	}
	return Q_atof (var->string);
}

char *Cvar_String (const char *var_name)
{
	cvar_t *var = Cvar_Find (var_name);

	if (!var)
		return cvar_null_string;
	return var->string;
}

void SV_SendServerInfoChange(char *key, char *value);

char* Cvar_ServerInfoValue(char* key, char* value)
{
	// force serverinfo "0" vars to be "".
	// meag: deathmatch is a special case as clients default 'not in serverinfo' to non-coop
	if (!strcmp(value, "0") && strcmp(key, "deathmatch")) {
		return "";
	}
	return value;
}

void Cvar_SetEx(cvar_t *var, char *value, qbool ignore_callback)
{
	static qbool changing = false;
	char *new_val;
#ifndef SERVERONLY
	extern cvar_t cl_warncmd;
	extern cvar_t re_subi[10];
	float test;
	qbool same_value;
#endif

	if (!var)
		return;

	// force serverinfo "0" vars to be "".
	if (var->flags & CVAR_SERVERINFO) {
		value = Cvar_ServerInfoValue(var->name, value);
	}

#ifndef SERVERONLY
	// C code may wrongly use Cvar_Set on non registered variable, some 99.99% accurate check
	// variables for internal triggers are not registered intentionally
	if (var < re_subi || var > re_subi + 9) {
		if (!var->next /* this is fast, but a bit flawed logic */ && !Cvar_Find(var->name)) {
			Com_Printf("Cvar_Set: on non linked var %s\n", var->name);
			return;
		}
	}
#endif

	if (var->flags & CVAR_ROM) {
#ifndef SERVERONLY
		Com_Printf ("\"%s\" is write protected\n", var->name);
#endif
		return;
	}

#ifndef SERVERONLY
	if (var->flags & CVAR_RULESET_MIN) {
		test  = Q_atof (value);
		if (test < var->minrulesetvalue) {
			Com_Printf ("min \"%s\" is limited to %0.2f\n", var->name,var->minrulesetvalue);
			return;
		}
	}

	if (var->flags & CVAR_RULESET_MAX) {
		test  = Q_atof (value);
		if (test > var->maxrulesetvalue) {
			Com_Printf ("max \"%s\" is limited to %0.2f\n", var->name,var->maxrulesetvalue);
			return;
		}
	}

	if ((var->flags & CVAR_INIT) && host_initialized) {
		if (cl_warncmd.value || developer.value)
			Com_Printf ("\"%s\" can only be changed with \"+set %s %s\" on the command line.\n", var->name, var->name, value);
		return;
	}

	// We do this before OnChange check, which means no OnChange check for latched cvars.
	if (var->flags & CVAR_LATCH)
	{
		if (var->latchedString)
		{
			if (strcmp(value, var->latchedString) == 0)
				return; // latched string alredy has this value
			Q_free(var->latchedString); // switching latching string to other, so free it
		}
		else
		{
			if (strcmp(value, var->string) == 0)
				return; // we change string value to the same, do no create latched string then
		}

		// HACK: sometime I need somehow silently change latched cvars.
		//       So, flag CVAR_SILENT for latched cvars mean no this warning.
		//       However keep this flag always on latched variable is stupid imo.
		//       So, do not mix CVAR_LATCHED | CVAR_SILENT in cvar definition.
		if ( !(var->flags & CVAR_SILENT) ) {
			const char* restartcmd = "vid_restart (video/graphics)";
			if (strncmp(var->name, "in_", 3) == 0) {
				restartcmd = "in_restart (input)";
			}
			else if (strncmp(var->name, "s_", 2) == 0) {
				restartcmd = "s_restart (sound)";
			}
			Com_Printf ("%s needs %s to take effect.\n", var->name, restartcmd);
		}
		var->latchedString = Q_strdup_named(value, var->name);
		var->modified = true; // set to true even car->string is not changed yet, that how q3 does
		return;
	}
#endif

	if (!ignore_callback && var->OnChange && !changing) {
		qbool cancel = false;

		changing = true;
		var->flags &= ~(CVAR_AUTOSETRECENT);
		var->OnChange(var, value, &cancel);
		changing = false;

		if (cancel) {
			return; // change rejected.
		}
	}

#ifndef SERVERONLY
	same_value = value && var->string && (value == var->string || !strcmp (value, var->string));
#endif
	// dup string first (before free) since 'value' and 'var->string' can point at the same memory area.
	new_val = Q_strdup_named(value, var->name);
	// free the old value string
	Q_free(var->string);
	// assign new value
	var->string = new_val;
	var->value = Q_atof (var->string);
#ifndef SERVERONLY
	var->integer = Q_atoi (var->string);
	if (var->flags & CVAR_TRACKERCOLOR) {
		TrackerStringToRGB_W(var->string, var->color);
	}
	else {
		StringToRGB_W(var->string, var->color);
	}
	if (!same_value && !(var->flags & CVAR_AUTOSETRECENT)) {
		Cvar_AutoReset (var);
	}
	var->flags &= ~(CVAR_AUTOSETRECENT);
	var->modified = true;
#endif

#ifndef CLIENTONLY
	if (var->flags & CVAR_SERVERINFO) {
		SV_ServerinfoChanged(var->name, var->string);
	}
#endif

#ifndef SERVERONLY
	if (var->flags & CVAR_USERINFO) {
		CL_UserinfoChanged(var->name, var->string);
	}
	if (var->flags & CVAR_RECOMPILE_PROGS) {
		renderer.CvarForceRecompile(var);
	}
	if ((var->flags & CVAR_RELOAD_GFX) && host_everything_loaded && !same_value) {
		VID_ReloadCvarChanged(var);
	}
#endif
}

void Cvar_SetIgnoreCallback(cvar_t *var, char *value)
{
	Cvar_SetEx(var, value, true);
}

void Cvar_Set(cvar_t *var, char *value)
{
	Cvar_SetEx(var, value, false);
}

void Cvar_ForceSet (cvar_t *var, char *value)
{
	int saved_flags;

	if (!var)
		return;

	saved_flags = var->flags;
	var->flags &= ~CVAR_ROM;
	Cvar_Set (var, value);
	var->flags = saved_flags;
}

void Cvar_SetByName (const char *var_name, char *value)
{
	cvar_t	*var;

	var = Cvar_Find (var_name);
	if (!var)
	{	// there is an error in C code if this happens
		Con_DPrintf ("Cvar_Set: variable %s not found\n", var_name);
		return;
	}

	Cvar_Set (var, value);
}

static void Cvar_ValueToString(char* val, int size, float value)
{
	int	i;

	snprintf(val, size - 1, "%f", value);
	for (i = strlen(val) - 1; i > 0 && val[i] == '0'; i--) {
		val[i] = 0;
	}

	if (val[i] == '.') {
		val[i] = '\0';
	}
}

void Cvar_SetValue (cvar_t *var, const float value)
{
	char val[128];

	Cvar_ValueToString(val, sizeof(val), value);

	Cvar_Set(var, val);
}

void Cvar_SetValueByName(const char *var_name, const float value)
{
	char val[32];

	snprintf(val, sizeof(val), "%.8g", value);
	Cvar_SetByName(var_name, val);
}

void Cvar_Register(cvar_t *var)
{
	int key;
	cvar_t *old = Cvar_Find(var->name);

	// All variables must be named :)
	if (var->name[0] == '\0')
		return;

#ifdef SERVERONLY
	char* value;

	// first check to see if it has already been defined
	if (old) {
		Con_Printf("Can't register variable %s, already defined\n", var->name);
		return;
	}

	// check for overlap with a command
	if (Cmd_Exists(var->name)) {
		Con_Printf("Cvar_Register: %s is a command\n", var->name);
		return;
	}

	// link the variable in
	key = Com_HashKey(var->name);
	var->hash_next = cvar_hash[key];
	cvar_hash[key] = var;
	var->next = cvar_vars;
	cvar_vars = var;

	// set it through the function to be consistent
	value = var->string;
	var->string = Q_strdup("");
	Cvar_SetROM(var, value);
#else
	// we already register cvar, warn about it
	if (old && !(old->flags & CVAR_USER_CREATED)) {
		// allow re-register latched cvar
		if (old->flags & CVAR_LATCH) {
			// if we have a latched string, take that value now
			Cvar_ApplyLatchedUpdate(old);
			Cvar_AutoReset(old);

			return;
		}

		// warn if CVAR_SILENT is not set
		if (!(old->flags & CVAR_SILENT)) {
			Com_Printf("Can't register variable %s, already defined\n", var->name);
		}

		return;
	}

	if (old && old == var) {
		Sys_Error("Cvar_Register: something wrong with %s", var->name);
	}

	if (var->defaultvalue) {
		Sys_Error("Cvar_Register: defaultvalue already set for %s", var->name);
	}

	var->defaultvalue = Q_strdup_named(var->string, var->name);
	if (old) {
		char string[512];
		qbool queued_replace;

		// Trigger change later if this is a proper cvar replacing a user one
		queued_replace = (old->flags & CVAR_USER_CREATED) && (var->flags & CVAR_QUEUED_TRIGGER) && !(var->flags & CVAR_USER_CREATED);
		if (queued_replace) {
			// Created by executing config, now we have the 'proper' cvar - store old value to trigger later
			var->latchedString = Q_strdup_named(old->string, var->name);
		}

		var->flags |= old->flags & ~(CVAR_USER_CREATED | CVAR_TEMP);
		strlcpy(string, (var->flags & CVAR_ROM) || queued_replace ? var->string : old->string, sizeof(string));
		Cvar_Delete(old->name);
		var->string = Q_strdup_named(string, var->name);
	}
	else {
		// allocate the string on zone because future sets will Q_free it
		var->string = Q_strdup_named(var->string, var->name);
	}

	var->value = Q_atof(var->string);
	if (((var->flags & CVAR_RULESET_MIN) && var->value < var->minrulesetvalue) ||
	    ((var->flags & CVAR_RULESET_MAX) && var->value > var->maxrulesetvalue)) {
		char val[128];

		var->value = bound(var->minrulesetvalue, var->value, var->maxrulesetvalue);
		Cvar_ValueToString(val, sizeof(val), var->value);

		Q_free(var->string);
		var->string = Q_strdup_named(val, var->name);
	}
	var->integer = Q_atoi(var->string);
	StringToRGB_W(var->string, var->color);
	var->modified = true;

	// link the variable in
	key = Com_HashKey(var->name) % VAR_HASHPOOL_SIZE;
	var->hash_next = cvar_hash[key];
	cvar_hash[key] = var;
	var->next = cvar_vars;
	cvar_vars = var;

	Cvar_AddCvarToGroup(var);

#ifndef CLIENTONLY
	if (var->flags & CVAR_SERVERINFO) {
		SV_ServerinfoChanged(var->name, var->string);
	}
#endif

	if (var->flags & CVAR_USERINFO) {
		CL_UserinfoChanged(var->name, var->string);
	}
#endif // !SERVERONLY
}

static void Cvar_ApplyLatchedUpdate(cvar_t* var)
{
	if (var->latchedString) {
		// I did't want bother with all this CVAR_ROM and OnChange handler, just set value
		Q_free(var->string);
		var->string = var->latchedString;
		var->latchedString = NULL;
		var->value = Q_atof(var->string);
		var->integer = Q_atoi(var->string);
		StringToRGB_W(var->string, var->color);
		var->modified = true;
	}
}

qbool Cvar_Command (void)
{
	cvar_t *v;

	// check variables
	if (!(v = Cvar_Find (Cmd_Argv(0))))
		return false;

	if (Cmd_Argc() == 1) {
#ifndef SERVERONLY
		if (cvar_viewhelp.value)
			Help_DescribeCvar (v);

		if (cvar_viewdefault.value) {
			char *spaces = CreateSpaces(strlen(v->name) + 2);

			Con_Printf ("%s : default value is \"%s\"\n", v->name, v->defaultvalue);

			Con_Printf ("%s current value is \"%s\"\n", spaces, v->string);

			if ((v->flags & CVAR_AUTO) && v->autoString) {
				Con_Printf ("%s auto    value is \"%s\"\n", spaces, v->autoString);
			}

			if (cvar_viewlatched.integer && v->latchedString) {
				Con_Printf ("%s latched value is \"%s\"\n", spaces, v->latchedString);
			}
		}
		else {
#endif
			Con_Printf ("\"%s\" is \"%s\"\n", v->name, v->string);
#ifndef SERVERONLY
			if ((v->flags & CVAR_AUTO) && v->autoString) {
				Con_Printf ("auto: \"%s\"\n", v->autoString);
			}

			if (cvar_viewlatched.integer && v->latchedString) {
				Con_Printf ("latched: \"%s\"\n", v->latchedString);
			}
		}
#endif
	}
	else {
#ifndef SERVERONLY
		// RestrictTriggers means that advanced (possibly cheaty) scripts are not allowed
		// So we will force the usage of user-created variables to go through the set command
		if (cbuf_current == &cbuf_server) {
			// Keep the mod from changing user cvars that it didn't create
			if ((v->flags & CVAR_USER_CREATED) && !(v->flags & CVAR_MOD_CREATED)) {
				Con_Printf ("Error: server attempt to \"set\" user created variable [%s]\n", v->name);
				return true;
			}
		}
		else if ((v->flags & CVAR_USER_CREATED) && Rulesets_RestrictTriggers()) {
			Con_Printf ("Current ruleset requires \"set\" with user created variables [%s]\n", v->name);
			return true;
		}
#endif
		Cvar_Set (v, Cmd_MakeArgs(1));
	}

	return true;
}

static void Cvar_Toggle_f_base (qbool use_regex)
{
	cvar_t *var;
	char *name;
	int i;

	if (Cmd_Argc() < 2) {
		Con_Printf("%s <cvar> [<cvar>..]: toggle a cvar on/off\n", Cmd_Argv(0));
		return;
	}

	for (i = 1; i < Cmd_Argc(); i++) {
		name = Cmd_Argv(i);

		if (use_regex && IsRegexp(name)) {
			if (!ReSearchInitEx(name, false)) {
				continue;
			}

			for (var = cvar_vars; var; var = var->next) {
				if (ReSearchMatch(var->name)) {
					Cvar_Set(var, var->value ? "0" : "1");
				}
			}

			ReSearchDone();
		}
		else {
			var = Cvar_Find(name);

			if (!(var)) {
				Con_Printf("Unknown variable \"%s\"\n", name);
				continue;
			}
			Cvar_Set(var, var->value ? "0" : "1");
		}
	}
}

void Cvar_Toggle_f (void)
{
	Cvar_Toggle_f_base(false);
}

void Cvar_Toggle_re_f (void)
{
	Cvar_Toggle_f_base(true);
}

void Cvar_Toggle (cvar_t *var)
{
	Cvar_SetValue (var, (var->value == 0) ? 1 : 0);
}

int Cvar_CvarCompare (const void *p1, const void *p2)
{
	return strcmp((*((cvar_t **) p1))->name, (*((cvar_t **) p2))->name);
}

void Cvar_CvarList(qbool use_regex)
{
	static cvar_t *sorted_cvars[4096];
	int i, c, m = 0, count;
	cvar_t *var;
	char *pattern;

#define MAX_SORTED_CVARS (sizeof (sorted_cvars) / sizeof (sorted_cvars[0]))

	for (var = cvar_vars, count = 0; var && count < MAX_SORTED_CVARS; var = var->next, count++) {
		sorted_cvars[count] = var;
	}
	qsort(sorted_cvars, count, sizeof(cvar_t *), Cvar_CvarCompare);

	if (count == MAX_SORTED_CVARS) {
		assert(!"count == MAX_SORTED_CVARS");
	}

	pattern = (Cmd_Argc() > 1) ? Cmd_Argv(1) : NULL;

	if (((c = Cmd_Argc()) > 1) && use_regex) {
		if (!ReSearchInitEx(Cmd_Argv(1), false)) {
			return;
		}
	}

	Con_Printf("List of cvars:\n");
	for (i = 0; i < count; i++) {
		var = sorted_cvars[i];
		if (use_regex) {
			if (!(c == 1 || ReSearchMatch(var->name))) {
				continue;
			}
		}
		else {
			if (pattern && !Q_glob_match(pattern, var->name)) {
				continue;
			}
		}

		Con_Printf("%c%c %s\n",
#ifndef SERVERONLY
			var->flags & CVAR_USERINFO ? 'u' :
#endif
			' ',
#ifndef CLIENTONLY
			var->flags & CVAR_SERVERINFO ? 's' : 
#endif
			' ',
			var->name);
		m++;
	}

	if (use_regex && (c > 1)) {
		ReSearchDone();
	}

	Con_Printf("------------\n%i/%i %svariables\n", m, count, (pattern) ? "matching " : "");
}

void Cvar_CvarList_f (void)
{
	Cvar_CvarList (false);
}

void Cvar_CvarList_re_f (void)
{
	Cvar_CvarList (true);
}

static void Cvar_CvarEdit_f(void)
{
	cvar_t* cvar;
	char *s, final_string[MAXCMDLINE - 1];
	int c;

	c = Cmd_Argc();
	if (c == 1) {
		Com_Printf("%s <name> : modify a cvar\n", Cmd_Argv(0));
		Com_Printf("cvarlist : list all cvars\n");
		return;
	}

	cvar = Cvar_Find(Cmd_Argv(1));
	if (cvar == NULL) {
		Com_Printf("\"%s\" not a valid cvar\n", Cmd_Argv(1));
		return;
	}
	else {
		s = Q_strdup(cvar->string);
	}

	snprintf(final_string, sizeof(final_string), "/%s \"%s\"", Cmd_Argv(1), s);
	Key_ClearTyping();
	memcpy(key_lines[edit_line] + 1, str2wcs(final_string), strlen(final_string) * sizeof(wchar));
	Q_free(s);
}

cvar_t *Cvar_Create(const char *name, const char *string, int cvarflags)
{
	cvar_t *v;
	int key;

	v = Cvar_Find(name);
	if (v) {
#ifndef SERVERONLY
		v->flags &= ~CVAR_TEMP;
#endif
		v->flags |= cvarflags;
		return v;
	}
	v = (cvar_t *)Q_malloc(sizeof(cvar_t));
	// Cvar doesn't exist, so we create it
	v->next = cvar_vars;
	cvar_vars = v;

	key = Com_HashKey(name) % VAR_HASHPOOL_SIZE;
	v->hash_next = cvar_hash[key];
	cvar_hash[key] = v;

	v->name = Q_strdup_named(name, name);
	v->string = Q_strdup_named(string, name);
	v->value = Q_atof(v->string);
	v->flags = cvarflags;
#ifndef SERVERONLY
	v->flags |= CVAR_USER_CREATED;
	v->defaultvalue = Q_strdup_named(string, name);
	if (cbuf_current == &cbuf_server) {
		v->flags |= CVAR_MOD_CREATED;
	}
	v->integer = Q_atoi(v->string);
	StringToRGB_W(v->string, v->color);
	v->modified = true;
#endif

	return v;
}

// returns true if the cvar was found (and deleted)
qbool Cvar_Delete(const char *name)
{
	cvar_t *var, *prev = NULL;
	int key = Com_HashKey(name) % VAR_HASHPOOL_SIZE;

	for (var = cvar_hash[key]; var; var = var->hash_next) {
		if (!strcasecmp(var->name, name)) {
			// unlink from hash
			if (prev) {
				prev->hash_next = var->hash_next;
			}
			else {
				cvar_hash[key] = var->hash_next;
			}
			break;
		}
		prev = var;
	}

	if (!var) {
		return false;
	}

	prev = NULL;
	for (var = cvar_vars; var; var = var->next) {
		if (!strcasecmp(var->name, name)) {
			// unlink from cvar list
			if (prev) {
				prev->next = var->next;
			}
			else {
				cvar_vars = var->next;
			}
#ifndef SERVERONLY
			Q_free(var->defaultvalue);
#endif
			// free
			Q_free(var->string);
			Q_free(var->name);
			Q_free(var);
			return true;
		}
		prev = var;
	}

	Sys_Error("Cvar list broken");
	return false;
}

void Cvar_Set_f (void)
{
	cvar_t *var;
	char *var_name;

	if (Cmd_Argc() != 3) {
		Con_Printf ("Usage: %s <cvar> <value>\n", Cmd_Argv(0));
		return;
	}

	var_name = Cmd_Argv (1);
	var = Cvar_Find (var_name);

	if (var) {
		Cvar_Set(var, Cmd_Argv(2));
	}
	else {
		if (Cmd_Exists(var_name)) {
			Con_Printf("\"%s\" is a command\n", var_name);
			return;
		}

		var = Cvar_Create (var_name, Cmd_Argv(2), cbuf_current == &cbuf_server ? CVAR_MOD_CREATED : CVAR_USER_CREATED);
	}
}

void Cvar_Inc_f(void)
{
	int     c;
	cvar_t  *var;
	float   delta;

	c = Cmd_Argc();
	if (c != 2 && c != 3) {
		Con_Printf("Usage: inc <cvar> [value]\n");
		return;
	}

	var = Cvar_Find(Cmd_Argv(1));
	if (!var) {
		Con_Printf("Unknown variable \"%s\"\n", Cmd_Argv(1));
		return;
	}
	delta = (c == 3) ? atof(Cmd_Argv(2)) : 1;

	Cvar_SetValue(var, var->value + delta);
}

int Cvar_GetFlags (cvar_t *var)
{
	return var->flags;
}

#ifndef SERVERONLY
qbool Cvar_ForceCallback(cvar_t *var)
{
	qbool cancel = false;

	if (!var || !var->OnChange)
		return cancel;

	var->OnChange(var, var->string, &cancel);
	return cancel;
}

void Cvar_ResetVar (cvar_t *var)
{
	if (var && strcmp(var->string, var->defaultvalue)) {
		Cvar_Set(var, var->defaultvalue);
	}
}

void Cvar_Reset(qbool use_regex)
{
	qbool re_search = false;
	cvar_t *var;
	char *name;
	int i;

	if (Cmd_Argc() < 2) {
		Com_Printf("%s <cvar> [<cvar2>..]: reset variable to it default value\n", Cmd_Argv(0));
		return;
	}

	for (i = 1; i < Cmd_Argc(); i++) {
		name = Cmd_Argv(i);

		if (use_regex && (re_search = IsRegexp(name))) {
			if (!ReSearchInitEx(name, false)) {
				continue;
			}
		}

		if (use_regex && re_search) {
			for (var = cvar_vars; var; var = var->next) {
				if (ReSearchMatch(var->name)) {
					Cvar_ResetVar(var);
				}
			}
		}
		else {
			if ((var = Cvar_Find(name))) {
				Cvar_ResetVar(var);
			}
			else {
				Com_Printf("%s : No variable with name %s\n", Cmd_Argv(0), name);
			}
		}
		if (use_regex && re_search) {
			ReSearchDone();
		}
	}
}

void Cvar_Reset_f (void)
{
	Cvar_Reset(false);
}

void Cvar_Reset_re_f (void)
{
	Cvar_Reset(true);
}

void Cvar_SetDefaultAndValue(cvar_t *var, float default_value, float actual_value)
{
	char val[128];
	int i;

	snprintf (val, sizeof(val), "%f", default_value);
	for (i = strlen(val) - 1; i > 0 && val[i] == '0'; i--)
		val[i] = 0;
	if (val[i] == '.')
		val[i] = 0;
	Q_free(var->defaultvalue);
	var->defaultvalue = Q_strdup_named(val, var->name);
	Cvar_SetValue(var, actual_value);
}

char *Cvar_CompleteVariable (char *partial)
{
	cvar_t *cvar;
	int len;

	if (!(len = strlen(partial)))
		return NULL;

	// check exact match
	for (cvar = cvar_vars; cvar; cvar = cvar->next)
		if (!strcasecmp (partial,cvar->name))
			return cvar->name;

	// check partial match
	for (cvar = cvar_vars; cvar; cvar = cvar->next)
		if (!strncasecmp (partial,cvar->name, len))
			return cvar->name;

	return NULL;
}

int Cvar_CompleteCountPossible (char *partial)
{
	cvar_t *cvar;
	int len, c = 0;

	if (!(len = strlen(partial)))
		return 0;

	// check partial match
	for (cvar = cvar_vars; cvar; cvar = cvar->next)
		if (!strncasecmp (partial, cvar->name, len))
			c++;

	return c;
}

void Cvar_RulesetSet(cvar_t *var, char *val, int m)
{
	float rulesetval_f = Q_atof (val);

	switch (m) {
	case 0:
		var->minrulesetvalue = rulesetval_f;
		break;
	case 1:
		var->maxrulesetvalue = rulesetval_f;
		break;
	case 2:
		var->minrulesetvalue = rulesetval_f;
		var->maxrulesetvalue = rulesetval_f;
		break;
	default:
		return;
	}
}

void Cvar_AutoSet(cvar_t *var, char *value)
{
	if (!var || !(var->flags & CVAR_AUTO) || !value) {
		return;
	}

	Q_free(var->autoString);

	var->autoString = Q_strdup(value);
	var->flags |= CVAR_AUTOSETRECENT;
}

void Cvar_AutoSetInt(cvar_t *var, int value)
{
	char val[128];

	if (!var || !(var->flags & CVAR_AUTO)) {
		return;
	}

	Q_free(var->autoString);

	snprintf(&val[0], sizeof(val), "%d", value);

	var->autoString = Q_strdup_named(val, var->name);
	var->flags |= CVAR_AUTOSETRECENT;
}

void Cvar_AutoReset(cvar_t *var)
{
	if (!var) {
		return;
	}

	Q_free(var->autoString);
}

// silently set value for latched cvar
void Cvar_LatchedSet(cvar_t *var, char *value)
{
	// yea, do not mess things
	if (!(var->flags & CVAR_LATCH)) {
		Sys_Error("Cvar_LatchedSet: non latched var %s", var->name);
	}

	var->flags |= CVAR_SILENT; // shut up warnings, at least some...
	Cvar_Set(var, value);
	// remove this flag, btw if variable have this flag before set this flag will be removed anyway..
	var->flags &= ~CVAR_SILENT;
	// actually set value
	Cvar_Register(var);
}

// silently set value for latched cvar
void Cvar_LatchedSetValue(cvar_t *var, float value)
{
	// yea, do not mess things
	if (!(var->flags & CVAR_LATCH)) {
		Sys_Error("Cvar_LatchedSet: non latched var %s", var->name);
	}

	var->flags |= CVAR_SILENT; // shut up warnings, at least some...
	Cvar_SetValue(var, value);
	// remove this flag, btw if variable have this flag before set this flag will be removed anyway..
	var->flags &= ~CVAR_SILENT;
	// actually set value
	Cvar_Register(var);
}

void Cvar_SetFlags(cvar_t *var, int flags)
{
	var->flags = flags;
}

static cvar_group_t *current = NULL;
cvar_group_t *cvar_groups = NULL;

#define CVAR_GROUPS_DEFINE_VARIABLES
#include "cvar_groups.h"
#undef CVAR_GROUPS_DEFINE_VARIABLES

static cvar_group_t *Cvar_AddGroup(char *name)
{
	static qbool initialised = false;
	cvar_group_t *newgroup;
	char **s;

	if (!initialised) {
		initialised = true;
		for (s = cvar_groups_list; *s; s++)
			Cvar_AddGroup(*s);
	}

	for (newgroup = cvar_groups; newgroup; newgroup = newgroup->next)
		if (!strcasecmp(newgroup->name, name))
			return newgroup;

	newgroup = (cvar_group_t *) Q_malloc(sizeof(cvar_group_t));
	strlcpy(newgroup->name, name, sizeof(newgroup->name));
	newgroup->count = 0;
	newgroup->head = NULL;
	newgroup->next = cvar_groups;
	cvar_groups = newgroup;

	return newgroup;
}

void Cvar_SetCurrentGroup(char *name)
{
	cvar_group_t *group;

	for (group = cvar_groups; group; group = group->next) {
		if (!strcmp(group->name, name)) {
			current = group;
			return;
		}
	}
	current = Cvar_AddGroup(name);
}

void Cvar_ResetCurrentGroup(void)
{
	current = NULL;
}

static void Cvar_AddCvarToGroup(cvar_t *var)
{
	if ((var->group = current)) {
		var->next_in_group = current->head;
		current->head = var;
		current->count++;
	}
	else {
		var->next_in_group = NULL;
	}
}

void Cvar_Set_tp_f(void)
{
	cvar_t *var;
	char *var_name;

	if (Cmd_Argc() != 3) {
		Com_Printf("Usage: %s <cvar> <value>\n", Cmd_Argv(0));
		return;
	}

	var_name = Cmd_Argv(1);
	var = Cvar_Find(var_name);

	if (var) {
		if (!var->teamplay) {
			Com_Printf("\"%s\" is not a teamplay variable\n", var->name);
			return;
		}
		else {
			Cvar_Set(var, Cmd_Argv(2));
		}
	}
	else {
		if (Cmd_Exists(var_name)) {
			Com_Printf("\"%s\" is a command\n", var_name);
			return;
		}
		var = Cvar_Create(var_name, Cmd_Argv(2), 0);
		var->teamplay = true;
	}
}

static void Cvar_Set_ex_f(void)
{
	cvar_t* var;
	char* var_name;
	char* st = NULL;
	char	text_exp[1024];
	qbool   parse_funchars = !strcasecmp(Cmd_Argv(0), "set_ex");

	if (Rulesets_RestrictSetEx()) {
		Com_Printf("The use of set_ex/set_ex2 is not allowed during matches\n");
		return;
	}

	if (Cmd_Argc() != 3) {
		Com_Printf("usage: %s <cvar> <value>\n", Cmd_Argv(0));
		return;
	}

	var_name = Cmd_Argv(1);
	var = Cvar_Find(var_name);

	if (!var) {
		if (Cmd_Exists(var_name)) {
			Com_Printf("\"%s\" is a command\n", var_name);
			return;
		}
		var = Cvar_Create(var_name, "", 0);
	}

	Cmd_ExpandString(Cmd_Argv(2), text_exp);
	st = TP_ParseMacroString(text_exp);
	if (parse_funchars) {
		st = TP_ParseFunChars(st, false);
	}

	Cvar_Set(var, st);
}

void Cvar_Set_Alias_Str_f (void)
{
	cvar_t		*var;
	char		*var_name;
	char		*alias_name;
	//char		str[1024];
	char		*v /*,*s*/;
	if (Cmd_Argc() != 3) {
		Com_Printf ("usage: set_alias_str <cvar> <alias>\n");
		return;
	}

	var_name = Cmd_Argv (1);
	alias_name = Cmd_Argv (2);
	var = Cvar_Find (var_name);
	v = Cmd_AliasString( alias_name );

	if ( !var) {
		if (Cmd_Exists(var_name)) {
			Com_Printf ("\"%s\" is a command\n", var_name);
			return;
		}
		var = Cvar_Create(var_name, "", 0);
	}

	if (!var) {
		Com_Printf ("Unknown variable \"%s\"\n", var_name);
		return;
	}
	else if (!v) {
		Com_Printf ("Unknown alias \"%s\"\n", alias_name);
		return;
	}
	else {
		Cvar_Set (var, v);
	}
}

void Cvar_Set_Bind_Str_f(void)
{
	cvar_t		*var;
	int			keynum;
	char		*var_name;
	char		*key_name;
	//char		str[1024];
	//char		*v,*s;

	if (Cmd_Argc() != 3) {
		Com_Printf("usage: set_bind_str <cvar> <key>\n");
		return;
	}

	var_name = Cmd_Argv(1);
	key_name = Cmd_Argv(2);
	var = Cvar_Find(var_name);
	keynum = Key_StringToKeynum(key_name);

	if (!var) {
		if (Cmd_Exists(var_name)) {
			Com_Printf("\"%s\" is a command\n", var_name);
			return;
		}
		var = Cvar_Create(var_name, "", 0);
	}

	if (!var) {
		Com_Printf("Unknown variable \"%s\"\n", var_name);
		return;
	}
	else if (keynum == -1) {
		Com_Printf("Unknown key \"%s\"\n", key_name);
		return;
	}
	else {
		if (keybindings[keynum]) {
			Cvar_Set(var, keybindings[keynum]);
		}
		else {
			Cvar_Set(var, "");
		}
	}
}

// disconnect -->
void Cvar_UnSet (qbool use_regex)
{
	cvar_t	*var, *next;
	char	*name;
	int		i;
	qbool	re_search = false;

	if (Cmd_Argc() < 2) {
		Com_Printf ("unset <cvar> [<cvar2>..]: erase user-created variable\n");
		return;
	}

	for (i=1; i<Cmd_Argc(); i++) {
		name = Cmd_Argv(i);

		if (use_regex && (re_search = IsRegexp(name))) {
			if (!ReSearchInitEx(name, false)) {
				continue;
			}
		}

		if (use_regex && re_search) {
			for (var = cvar_vars ; var ; var = next) {
				next = var->next;

				if (ReSearchMatch(var->name)) {
					if ((var->flags & CVAR_USER_CREATED) && !(var->flags & CVAR_MOD_CREATED)) {
						Cvar_Delete(var->name);
					}
					else {
						Com_Printf("Can't delete not user created cvars (\"%s\")\n", var->name);
					}
				}
			}
		}
		else {
			if (!(var = Cvar_Find(name))) {
				Com_Printf("Can't delete \"%s\": no such cvar\n", name);
				continue;
			}

			if ((var->flags & CVAR_USER_CREATED) && !(var->flags & CVAR_MOD_CREATED)) {
				Cvar_Delete(name);
			}
			else {
				Com_Printf("Can't delete not user created cvars (\"%s\")\n", name);
			}
		}
		if (use_regex && re_search)
			ReSearchDone();
	}
}

void Cvar_UnSet_f(void)
{
	Cvar_UnSet(false);
}

void Cvar_UnSet_re_f(void)
{
	Cvar_UnSet(true);
}
// <-- disconnect

// QW262 -->

// XXX: this could do with some refactoring
void Cvar_Set_Calc_f(void)
{
	cvar_t *var, *var2;
	char *var_name, *op, *a2, *a3;
	int	pos, len, division_by_zero = 0;
	float num1, num2, result;
	char buf[1024];

	if (Rulesets_RestrictSetCalc()) {
		Com_Printf("The use of set_calc is not allowed during matches\n");
		return;
	}

	var_name = Cmd_Argv (1);
	var = Cvar_Find (var_name);

	if (!var)
		var = Cvar_Create (var_name, Cmd_Argv (2), 0);

	a2 = Cmd_Argv (2);
	a3 = Cmd_Argv (3);

	if (!strcmp (a2, "strlen")) {
		Cvar_SetValue (var, strlen (a3));
		return;
	} else if (!strcmp (a2, "int")) {
		Cvar_SetValue (var, (int) Q_atof (a3));
		return;
	} else if (!strcmp (a2, "substr")) {
		int var2len;
		var2 = Cvar_Find (a3);

		if (!var2) {
			Com_Printf ("Unknown variable \"%s\"\n", a3);
			return;
		}

		pos = atoi (Cmd_Argv (4));
		len = (Cmd_Argc () < 6)? 1 : atoi (Cmd_Argv (5));

		if (len == 0) {
			Cvar_Set (var, "");
			return;
		}

		if (len < 0 || pos < 0) {
			Com_Printf ("substr: invalid len or pos\n");
			return;
		}

		var2len = strlen (var2->string);

		if (var2len < pos) {
			Com_Printf ("substr: string length exceeded\n");
			return;
		}

		len = min (var2len - pos, len);
		strlcpy (buf, var2->string + pos, len);
		Cvar_Set (var, buf);
		return;

	} else if (!strcmp (a2, "set_substr")) {
		int	var1len,var2len;
		var2 = Cvar_Find (a3);

		if (!var2) {
			Com_Printf ("Unknown variable \"%s\"\n", a3);
			return;
		}

		var1len = strlen (var->string);
		var2len = strlen (var2->string);
		pos = atoi (Cmd_Argv (4));

		strlcpy (buf, var->string, sizeof (buf));

		if (pos + var2len > var1len) { // need to expand
			int i;
			for (i = var1len; i < pos + var2len; i++)
				buf[i] = ' ';
			buf[pos+var2len] = 0;
		}

		strlcpy (buf + pos, var2->string, sizeof (buf) - pos);
		Cvar_Set (var, buf);

		return;
	} else if (!strcmp (a2, "pos")) {
		var2 = Cvar_Find (a3);

		if (!var2) {
			Com_Printf ("Unknown variable \"%s\"\n", a3);
			return;
		}

		op = strstr (var2->string, Cmd_Argv (4));
		Cvar_SetValue (var, op ? op - var2->string : -1);
		return;
	} else if (!strcmp (a2, "tobrown")) {
		strlcpy (buf, var->string, sizeof (buf));
		CharsToBrown (buf, buf + strlen (buf));
		Cvar_Set (var, buf);
		return;
	} else if (!strcmp (a2, "towhite")) {
		strlcpy (buf, var->string, sizeof (buf));
		CharsToWhite (buf, buf + strlen (buf));
		Cvar_Set (var, buf);
		return;
	}

	num1 = Q_atof (a2);
	op = a3;
	num2 = Q_atof (Cmd_Argv (4));

	if (!strcmp (op, "+"))
		result = num1 + num2;
	else if (!strcmp (op, "-"))
		result = num1 - num2;
	else if (!strcmp (op, "*"))
		result = num1 * num2;
	else if (!strcmp (op, "/")) {
		if (num2 != 0)
			result = num1 / num2;
		else
			division_by_zero = 1;
	} else if (!strcmp (op, "%")) {
		if ((int) num2 != 0)
			result = (int) num1 % (int) num2;
		else
			division_by_zero = 1;
	} else if (!strcmp (op, "div")) {
		if ((int) num2 != 0)
			result = (int) num1 / (int) num2;
		else
			division_by_zero = 1;
	} else if (!strcmp (op, "and")) {
		result = (int) num1 & (int) num2;
	} else if (!strcmp (op, "or")) {
		result = (int) num1 | (int) num2;
	} else if (!strcmp (op, "xor")) {
		result = (int) num1 ^ (int) num2;
	} else {
		Com_Printf ("set_calc: unknown operator: %s\nvalid operators are: + - * / div %% and or xor\n", op);
		// Com_Printf("set_calc: unknown command: %s\nvalid commands are: strlen int substr set_substr pos\n", a2);
		return;
	}

	if (division_by_zero) {
		Com_Printf ("set_calc: can't divide by zero\n");
		result = 999;
	}

	Cvar_SetValue (var, result);
}

// <-- QW262

void Cvar_Set_Eval_f(void)
{
	if (Rulesets_RestrictSetEval()) {
		Com_Printf("The use of set_eval is not allowed during matches\n");
		return;
	}

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage:\n"
			"set_eval <variable> <expression>\n"
			"set_eval is similar to set_calc but supports richer expressions\n");
		return;
	}
	else {
		const char *expression;
		char *var_name;
		cvar_t *var;
		int errn;
		expr_val result;

		var_name = Cmd_Argv (1);
		var = Cvar_Find (var_name);

		if (!var)
			var = Cvar_Create (var_name, Cmd_Argv (1), 0);

		expression = Cmd_MakeArgs(2);

		result = Expr_Eval(expression, NULL, &errn);

		if (errn == EXPR_EVAL_SUCCESS) {
			switch (result.type) {
			case ET_INT:
				Cvar_SetValue(var, result.i_val);
				break;
			case ET_DBL:
				Cvar_SetValue(var, result.d_val);
				break;
			case ET_BOOL:
				Cvar_SetValue(var, result.b_val ? 1 : 0);
				break;
			case ET_STR:
				Cvar_Set(var, result.s_val);
				Q_free(result.s_val);
				break;
			}
		}
		else {
			Com_Printf("Error in expression: %s\n", Parser_Error_Description(errn));
		}
	}
}



// if an unknown command with parameters was encountered when loading
// config.cfg, assume it's a cvar set and spawn a temp var
qbool Cvar_CreateTempVar (void)
{
	char *name = Cmd_Argv(0);
	// FIXME, make sure it's a valid cvar name, return false if not
	Cvar_Create (name, Cmd_MakeArgs(1), CVAR_TEMP);
	return true;
}

// if none of the subsystems claimed the cvar from config.cfg, remove it
void Cvar_CleanUpTempVars (void)
{
	cvar_t *var, *next;

	for (var = cvar_vars; var; var = next) {
		next = var->next;
		if (var->flags & CVAR_TEMP)
			Cvar_Delete (var->name);
	}
}
#endif

void Cvar_Init(void)
{
	Cmd_AddCommand("cvarlist", Cvar_CvarList_f);
	Cmd_AddCommand("cvaredit", Cvar_CvarEdit_f);
	Cmd_AddCommand("cvarlist_re", Cvar_CvarList_re_f);
	Cmd_AddCommand("toggle", Cvar_Toggle_f);
	Cmd_AddCommand("set", Cvar_Set_f);
	Cmd_AddCommand("inc", Cvar_Inc_f);

#ifndef SERVERONLY
	Cmd_AddCommand("set_tp", Cvar_Set_tp_f);
	Cmd_AddCommand("set_ex", Cvar_Set_ex_f);
	Cmd_AddCommand("set_ex2", Cvar_Set_ex_f);
	Cmd_AddCommand("set_alias_str", Cvar_Set_Alias_Str_f);
	Cmd_AddCommand("set_bind_str", Cvar_Set_Bind_Str_f);
	Cmd_AddCommand("unset", Cvar_UnSet_f);
	Cmd_AddCommand("unset_re", Cvar_UnSet_re_f);
	Cmd_AddCommand("toggle_re", Cvar_Toggle_re_f);
	Cmd_AddCommand("cvar_reset", Cvar_Reset_f);
	Cmd_AddCommand("cvar_reset_re", Cvar_Reset_re_f);
	Cmd_AddCommand("set_calc", Cvar_Set_Calc_f);
	Cmd_AddCommand("set_eval", Cvar_Set_Eval_f);

	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register(&cvar_viewdefault);
	Cvar_Register(&cvar_viewhelp);
	Cvar_Register(&cvar_viewlatched);

	Cvar_ResetCurrentGroup();
#endif
}

void Cvar_Shutdown(void)
{
	cvar_t *cvar;
	cvar_t *next;
	cvar_group_t* group;
	cvar_group_t* next_group;

	for (cvar = cvar_vars; cvar; cvar = next) {
		next = cvar->next;

#ifndef SERVERONLY
		Q_free(cvar->latchedString);
		Q_free(cvar->autoString);
		Q_free(cvar->defaultvalue);
#endif
		Q_free(cvar->string);

#ifndef SERVERONLY
		if (cvar->flags & CVAR_USER_CREATED) {
			Q_free(cvar->name);
			Q_free(cvar);
		}
#endif
	}

#ifndef SERVERONLY
	for (group = cvar_groups; group; group = next_group) {
		next_group = group->next;
		Q_free(group);
	}
#endif
}

void Cvar_ExecuteQueuedChanges(void)
{
	cvar_t* cvar;
	qbool vid_restart = false;
	qbool vid_reload = false;
	qbool sound_restart = false;

	for (cvar = cvar_vars; cvar; cvar = cvar->next) {
		if ((cvar->flags & CVAR_QUEUED_TRIGGER) && cvar->latchedString) {
			if (cvar->OnChange) {
				qbool cancel = false;

				cvar->OnChange(cvar, cvar->latchedString, &cancel);
				if (!cancel) {
					Cvar_ApplyLatchedUpdate(cvar);
				}
			}

			Q_free(cvar->latchedString);
		}

		vid_restart |= (cvar->flags & CVAR_LATCH_GFX) && cvar->latchedString;
		vid_reload |= (cvar->flags & CVAR_RELOAD_GFX && cvar->modified);
		sound_restart |= (cvar->flags & CVAR_LATCH_SOUND) && cvar->latchedString;
	}

	if (vid_restart) {
		Cbuf_AddTextEx(&cbuf_main, "\nvid_restart\n");
	}
	else {
		if (sound_restart) {
			Cbuf_AddTextEx(&cbuf_main, "\ns_restart\n");
		}
		if (vid_reload) {
			Cbuf_AddTextEx(&cbuf_main, "\nvid_reload\n");
		}
	}

	Cvar_ClearAllModifiedFlags(CVAR_RELOAD_GFX);
}

void Cvar_ClearAllModifiedFlags(int flags)
{
	cvar_t* cvar;

	for (cvar = cvar_vars; cvar; cvar = cvar->next) {
		if (cvar->flags & flags) {
			cvar->modified = false;
		}
	}
}

qbool Cvar_AnyModified(int flags)
{
	cvar_t* cvar;

	for (cvar = cvar_vars; cvar; cvar = cvar->next) {
		if ((cvar->flags & flags) && cvar->modified) {
			return true;
		}
	}
	return false;
}
