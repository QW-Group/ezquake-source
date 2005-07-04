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

#include "quakedef.h"
#include "common.h"
#include "console.h"
#include "rulesets.h"

#include "utils.h"

extern void CL_UserinfoChanged (char *key, char *value);
extern void SV_ServerinfoChanged (char *key, char *value);

extern cvar_t r_fullbrightSkins;
extern cvar_t allow_scripts;

static cvar_t *cvar_hash[32];
cvar_t *cvar_vars;

cvar_t	cvar_viewdefault = {"cvar_viewdefault", "1"};

cvar_t *Cvar_FindVar (char *var_name) {
	cvar_t *var;
	int key;

	key = Com_HashKey (var_name);
	for (var = cvar_hash[key]; var; var = var->hash_next) {
		if (!Q_strcasecmp (var_name, var->name)) {
			return var;
		}
	}

	return NULL;
}



void Cvar_ResetVar (cvar_t *var) {
	if (var && strcmp(var->string, var->defaultvalue))
		Cvar_Set(var, var->defaultvalue);
}

void Cvar_Reset_f (void) {
	int c;
	cvar_t *var;
	char *s;

	if ((c = Cmd_Argc()) != 2) {
		Com_Printf("Usage: %s <variable>\n", Cmd_Argv(0));
		return;
	}

	s = Cmd_Argv(1);

	if (var = Cvar_FindVar(s))
		Cvar_ResetVar(var);
	else
		Com_Printf("%s : No variable with name %s\n", Cmd_Argv(0), s);
}





void Cvar_SetDefault(cvar_t *var, float value) {
	char	val[128];
	int i;
	
	Q_snprintfz (val, sizeof(val), "%f", value);
	for (i = strlen(val) - 1; i > 0 && val[i] == '0'; i--)
		val[i] = 0;
	if (val[i] == '.')
		val[i] = 0;
	Z_Free (var->defaultvalue);
	var->defaultvalue = CopyString(val);
	Cvar_Set(var, val);
}



float Cvar_VariableValue (char *var_name) {
	cvar_t *var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
		return 0;
	return Q_atof (var->string);
}

char *Cvar_VariableString (char *var_name) {
	cvar_t *var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
		return "";
	return var->string;
}

char *Cvar_CompleteVariable (char *partial) {
	cvar_t *cvar;
	int len;

	len = strlen(partial);

	if (!len)
		return NULL;

	// check exact match
	for (cvar = cvar_vars; cvar; cvar = cvar->next)
		if (!Q_strcasecmp (partial,cvar->name))
			return cvar->name;

	// check partial match
	for (cvar = cvar_vars; cvar; cvar = cvar->next)
		if (!Q_strncasecmp (partial,cvar->name, len))
			return cvar->name;

	return NULL;
}

int Cvar_CompleteCountPossible (char *partial) {
	cvar_t *cvar;
	int len, c = 0;

	if (!(len = strlen(partial)))
		return 0;

	// check partial match
	for (cvar = cvar_vars; cvar; cvar = cvar->next)
		if (!Q_strncasecmp (partial, cvar->name, len))
			c++;

	return c;
}
void Cvar_RulesetSet(cvar_t *var, char *val, int m) {
	float rulesetval_f;
	rulesetval_f=Q_atoi(val);
	if (m==0) {
		var->minrulesetvalue=rulesetval_f;
	} else if (m==1) {
		var->maxrulesetvalue=rulesetval_f;
	} else if (m==2) {
		var->minrulesetvalue=rulesetval_f;
		var->maxrulesetvalue=rulesetval_f;
	} else {
	return;
	}
}

void Cvar_Set (cvar_t *var, char *value) {
#ifndef SERVERONLY
	extern cvar_t cl_warncmd;	
#endif
	static qboolean	changing = false;
	float test ;

	if (!var)
		return;

	if (var->flags & CVAR_ROM) {
		if (con_initialized)
			Com_Printf ("\"%s\" is write protected\n", var->name);
		return;
	}

	if (var->flags & CVAR_RULESET_MIN){
	test  = Q_atof (value);
		if (test < var->minrulesetvalue){	
		if (con_initialized)
			Com_Printf ("min \"%s\" is limited to %0.2f\n", var->name,var->minrulesetvalue);
		return;
		}
	}

	if (var->flags & CVAR_RULESET_MAX){
	test  = Q_atof (value);
		if (test > var->maxrulesetvalue){	
		if (con_initialized)
			Com_Printf ("max \"%s\" is limited to %0.2f\n", var->name,var->maxrulesetvalue);
		return;
		}
	}

	if ((var->flags & CVAR_INIT) && host_initialized) {
#ifndef SERVERONLY
		if (cl_warncmd.value || developer.value)
			Com_Printf ("\"%s\" can only be changed with \"+set %s %s\" on the command line.\n", var->name, var->name, value);
#endif
		return;
	}

	if (var->OnChange && !changing) {
		changing = true;
		if (var->OnChange(var, value)) {
			changing = false;
			return;
		}
		changing = false;
	}

	Z_Free (var->string);	// free the old value string

	var->string = CopyString (value);
	var->value = Q_atof (var->string);

#ifndef CLIENTONLY
	if (var->flags & CVAR_SERVERINFO)
		SV_ServerinfoChanged (var->name, var->string);
#endif

#ifndef SERVERONLY
	if (var->flags & CVAR_USERINFO)
		CL_UserinfoChanged (var->name, var->string);
#endif

	if (!cl.spectator && cls.state != ca_disconnected) {

		if (!strcmp(var->name, "r_fullbrightSkins")) {
			float fbskins;		
			fbskins = bound(0, var->value, cl.fbskins);	
			if (fbskins > 0) {
				Cbuf_AddText (va("say all skins %d%% fullbright\n", (int) (fbskins * 100)));	
			}
			else {
				Cbuf_AddText (va("say not using fullbright skins\n"));	
			}
		}
		else if (!strcmp(var->name, "allow_scripts")) {
			if (allow_scripts.value < 1)
				Cbuf_AddText("say not using scripts\n");
			else if (allow_scripts.value < 2 || com_blockscripts)
				Cbuf_AddText("say using simple scripts\n");
			else
				Cbuf_AddText("say using advanced scripts\n");
		}
	}
}

void Cvar_ForceSet (cvar_t *var, char *value) {
	int saved_flags;

	if (!var)
		return;

	saved_flags = var->flags;
	var->flags &= ~CVAR_ROM;
	Cvar_Set (var, value);
	var->flags = saved_flags;
}

void Cvar_SetValue (cvar_t *var, float value) {
	char val[128];
	int	i;

	Q_snprintfz (val, sizeof(val), "%f", value);

	for (i = strlen(val) - 1; i > 0 && val[i] == '0'; i--)
		val[i] = 0;
	if (val[i] == '.')
		val[i] = 0;

	Cvar_Set (var, val);
}

int Cvar_GetFlags (cvar_t *var) {
	return var->flags;
}

void Cvar_SetFlags (cvar_t *var, int flags) {
	var->flags = flags;
}

static cvar_group_t *current = NULL;
cvar_group_t	*cvar_groups = NULL;

#define CVAR_GROUPS_DEFINE_VARIABLES
#include "cvar_groups.h"
#undef CVAR_GROUPS_DEFINE_VARIABLES

static cvar_group_t *Cvar_AddGroup(char *name) {
	static qboolean initialised = false;
	cvar_group_t *newgroup;
	char **s;

	if (!initialised) {
		initialised = true;
		for (s = cvar_groups_list; *s; s++)
			Cvar_AddGroup(*s);
	}

	for (newgroup = cvar_groups; newgroup; newgroup = newgroup->next)
		if (!Q_strcasecmp(newgroup->name, name))
			return newgroup;

	newgroup = Q_Malloc(sizeof(cvar_group_t));
	Q_strncpyz(newgroup->name, name, sizeof(newgroup->name));
	newgroup->count = 0;
	newgroup->head = NULL;
	newgroup->next = cvar_groups;
	cvar_groups = newgroup;

	return newgroup;
}

void Cvar_SetCurrentGroup(char *name) {
	cvar_group_t *group;

	for (group = cvar_groups; group; group = group->next) {
		if (!strcmp(group->name, name)) {
			current = group;
			return;
		}
	}
	current = Cvar_AddGroup(name);
}

void Cvar_ResetCurrentGroup(void) {
	current = NULL;
}

static void Cvar_AddCvarToGroup(cvar_t *var) {
	if (var->group = current) {
		var->next_in_group = current->head;
		current->head = var;
		current->count++;
	} else {
		var->next_in_group = NULL;
	}
}




/*
Adds a freestanding variable to the variable list.

If the variable already exists, the value will not be set
The flags will be or'ed in if the variable exists.
*/
void Cvar_Register (cvar_t *var) {
	char string[512];
	int key;
	cvar_t *old;

	// first check to see if it has already been defined
	old = Cvar_FindVar (var->name);
	if (old && !(old->flags & CVAR_USER_CREATED)) {
		Com_Printf ("Can't register variable %s, already defined\n", var->name);
		return;
	}

	// check for overlap with a command
	if (Cmd_Exists (var->name))	{
		Com_Printf ("Cvar_Register: %s is a command\n", var->name);
		return;
	}

	var->defaultvalue = CopyString (var->string);	
	if (old) {
		var->flags |= old->flags & ~CVAR_USER_CREATED;
		Q_strncpyz (string, old->string, sizeof(string));
		Cvar_Delete (old->name);
		if (!(var->flags & CVAR_ROM))
			var->string = CopyString (string);
		else
			var->string = CopyString (var->string);
	} else {
		// allocate the string on zone because future sets will Z_Free it
		var->string = CopyString (var->string);
	}
	var->value = Q_atof (var->string);

	// link the variable in
	key = Com_HashKey (var->name);
	var->hash_next = cvar_hash[key];
	cvar_hash[key] = var;
	var->next = cvar_vars;
	cvar_vars = var;

	Cvar_AddCvarToGroup(var);	

#ifndef CLIENTONLY
	if (var->flags & CVAR_SERVERINFO)
		SV_ServerinfoChanged (var->name, var->string);
#endif

#ifndef SERVERONLY
	if (var->flags & CVAR_USERINFO)
		CL_UserinfoChanged (var->name, var->string);
#endif
}

qboolean Cvar_Command (void) {
	cvar_t *v;
	char *spaces;

	// check variables
	if (!(v = Cvar_FindVar (Cmd_Argv(0))))
		return false;

	if (Cmd_Argc() == 1) {
		if (cvar_viewdefault.value) {
			Com_Printf ("%s : default value is \"%s\"\n", v->name, v->defaultvalue);
			spaces = CreateSpaces(strlen(v->name) + 2);
			Com_Printf ("%s current value is \"%s\"\n", spaces, v->string);
		} else {
			Com_Printf ("\"%s\" is \"%s\"\n", v->name, v->string);
		}
	} else {
		// hexum - do not allow crafty people to avoid use of "set" with user created variables under ruleset smackdown
		
		if (!Q_strcasecmp(Rulesets_Ruleset(), "smackdown") && (v->flags & CVAR_USER_CREATED)) {
			Com_Printf ("Ruleset smackdown requires use of \"set\" with user created variables\n");
			return true;
		}

		Cvar_Set (v, Cmd_MakeArgs(1));
	}

	return true;
}

//Writes lines containing "set variable value" for all variables with the archive flag set to true.
void Cvar_WriteVariables (FILE *f) {
	cvar_t *var;

	// write builtin cvars in a QW compatible way
	for (var = cvar_vars ; var ; var = var->next)
		if (var->flags & CVAR_ARCHIVE)
			fprintf (f, "%s \"%s\"\n", var->name, var->string);

	// write everything else
	for (var = cvar_vars ; var ; var = var->next)
		if (var->flags & CVAR_USER_ARCHIVE)
			fprintf (f, "seta %s \"%s\"\n", var->name, var->string);
}

void Cvar_Toggle_f (void) {
	cvar_t *var;

	if (Cmd_Argc() != 2) {
		Com_Printf ("toggle <cvar> : toggle a cvar on/off\n");
		return;
	}

	var = Cvar_FindVar (Cmd_Argv(1));
	if (!var) {
		Com_Printf ("Unknown variable \"%s\"\n", Cmd_Argv(1));
		return;
	}
	Cvar_Set (var, var->value ? "0" : "1");
}

int Cvar_CvarCompare (const void *p1, const void *p2) {
    return strcmp((*((cvar_t **) p1))->name, (*((cvar_t **) p2))->name);
}

void Cvar_CvarList_f (void) {
	cvar_t *var;
	int i;
	static int count;
	static qboolean sorted = false;
	static cvar_t *sorted_cvars[512];	

#define MAX_SORTED_CVARS (sizeof(sorted_cvars) / sizeof(sorted_cvars[0]))

	if (!sorted) {
		for (var = cvar_vars, count = 0; var && count < MAX_SORTED_CVARS; var = var->next, count++)
			sorted_cvars[count] = var;
		qsort(sorted_cvars, count, sizeof (cvar_t *), Cvar_CvarCompare);
		sorted = true;
	}

	if (count == MAX_SORTED_CVARS)
		assert(!"count == MAX_SORTED_CVARS");

	for (i = 0; i < count; i++) {
		var = sorted_cvars[i];
		Com_Printf ("%c%c%c %s\n",
			var->flags & (CVAR_ARCHIVE|CVAR_USER_ARCHIVE) ? '*' : ' ',
			var->flags & CVAR_USERINFO ? 'u' : ' ',
			var->flags & CVAR_SERVERINFO ? 's' : ' ',
			var->name);
	}

	Com_Printf ("-------------\n%d variables\n", count);
}

cvar_t *Cvar_Create (char *name, char *string, int cvarflags) {
	cvar_t *v;
	int key;

	if ((v = Cvar_FindVar(name)))
		return v;
	v = (cvar_t *) Z_Malloc(sizeof(cvar_t));
	// Cvar doesn't exist, so we create it
	v->next = cvar_vars;
	cvar_vars = v;

	key = Com_HashKey (name);
	v->hash_next = cvar_hash[key];
	cvar_hash[key] = v;

	v->name = CopyString (name);
	v->string = CopyString (string);
	v->defaultvalue = CopyString (string);	
	v->flags = cvarflags;
	v->value = Q_atof (v->string);

	return v;
}

//returns true if the cvar was found (and deleted)
qboolean Cvar_Delete (char *name) {
	cvar_t *var, *prev;
	int key;

	key = Com_HashKey (name);

	prev = NULL;
	for (var = cvar_hash[key]; var; var = var->hash_next) {
		if (!Q_strcasecmp(var->name, name)) {
			// unlink from hash
			if (prev)
				prev->hash_next = var->hash_next;
			else
				cvar_hash[key] = var->hash_next;
			break;
		}
		prev = var;
	}

	if (!var)
		return false;

	prev = NULL;
	for (var = cvar_vars; var; var=var->next)	{
		if (!Q_strcasecmp(var->name, name)) {
			// unlink from cvar list
			if (prev)
				prev->next = var->next;
			else
				cvar_vars = var->next;

			// free
			Z_Free (var->defaultvalue);  
			Z_Free (var->string);
			Z_Free (var->name);
			Z_Free (var);
			return true;
		}
		prev = var;
	}

	assert(!"Cvar list broken");
	return false;
}


static qboolean cvar_seta = false;

void Cvar_Set_f (void) {
	cvar_t *var;
	char *var_name;

	if (Cmd_Argc() != 3) {
		Com_Printf ("Usage: %s <cvar> <value>\n", Cmd_Argv(0));
		return;
	}

	var_name = Cmd_Argv (1);
	var = Cvar_FindVar (var_name);

	if (var) {
		Cvar_Set (var, Cmd_Argv(2));
	} else {
		if (Cmd_Exists(var_name)) {
			Com_Printf ("\"%s\" is a command\n", var_name);
			return;
		}
		var = Cvar_Create (var_name, Cmd_Argv(2), CVAR_USER_CREATED);
	}

	if (cvar_seta)
		var->flags |= CVAR_USER_ARCHIVE;
}

// disconnect -->
void Cvar_UnSet_f (void) {
	cvar_t		*var;
	char		*name;
	int		i;
	qboolean	re_search;
	

	if (Cmd_Argc() < 2) {
		Com_Printf ("unset <name> [<name2>..]: erase an existing variable\n");
		return;
	}

	for (i=1; i<Cmd_Argc(); i++) {
		name = Cmd_Argv(i);

		if ((re_search = IsRegexp(name))) 
			if(!ReSearchInit(name))
				continue;

		if (re_search) {
			for (var = cvar_vars ; var ; var=var->next) { // ffs
				if (ReSearchMatch(var->name)) {
					if (var->flags & CVAR_USER_CREATED) {
						Cvar_Delete(var->name);
					} else {
						Com_Printf("Can't delete not user created cvars");
					return;
					}
				}
			}
		} else {
			var = Cvar_FindVar(name);

			if (!var) {
				Com_Printf("Can't unset \"%s\": no such cvar\n", name);
				continue;
			}

			if (var->flags & CVAR_USER_CREATED) {
				Cvar_Delete(name);
			} else {
				Com_Printf("Can't delete not user created cvars");
				return;
			}
		}
		if (re_search)
			ReSearchDone();
	}
}
// <-- disconnect

void Cvar_Seta_f (void) {
	cvar_seta = true;
	Cvar_Set_f ();
	cvar_seta = false;
}

// --> from qw262

void Cvar_Set_Calc_f(void)
{
	cvar_t	*var, *var2;
	char	*var_name;
	char	*op;
	char	*a2, *a3;
	float	num1;
	float	num2;
	float	result;
	int		division_by_zero = 0;
	int		pos, len;
	char	buf[1024];

	var_name = Cmd_Argv (1);
	var = Cvar_FindVar (var_name);
	
	if (!var)
		var = Cvar_Create (var_name, Cmd_Argv(2), CVAR_USER_CREATED);

	a2 = Cmd_Argv(2);
	a3 = Cmd_Argv(3);

	if (!strcmp (a2, "strlen")) {
		Cvar_SetValue (var, strlen(a3));
		return;
	} else if (!strcmp (a2, "int")) {
		Cvar_SetValue (var, (int)Q_atof(a3));
		return;
	} else if (!strcmp (a2, "substr")) {
		int		var2len;
		var2 = Cvar_FindVar (a3);
		if (!var2) {
			Com_Printf ("Unknown variable \"%s\"\n", a3);
			return;
		}

		pos = atoi(Cmd_Argv(4));
		if (Cmd_Argc() < 6)
			len = 1;
		else
			len = atoi(Cmd_Argv(5));

		if ( len == 0 ) {
			Cvar_Set(var, "");
			return;
		}

		if ( len < 0 || pos < 0) {
			Com_Printf("substr: invalid len or pos\n");
			return;
		}

		var2len = strlen(var2->string);
		if ( var2len < pos) {
			Com_Printf("substr: string length exceeded\n");
			return;
		}

		len = min ( var2len - pos, len );
		strncpy(buf, var2->string+pos, len);
		buf[len] = '\0';
		Cvar_Set(var, buf);
		return;
	} else if (!strcmp (a2, "set_substr")) {
		int		var1len,var2len;
		char	buf[1024];
		var2 = Cvar_FindVar (a3);
		if (!var2) {
			Com_Printf ("Unknown variable \"%s\"\n", a3);
			return;
		}
		var1len = strlen(var->string);
		var2len = strlen(var2->string);
		pos = atoi(Cmd_Argv(4));
		
		strcpy(buf,var->string);
		if (pos + var2len > var1len){ // need to expand
			int i;
			for (i = var1len; i < pos+var2len; i++)
				buf[i] = ' ';
			buf[pos+var2len] = 0;
		}
		
		strncpy(buf+pos, var2->string, var2len);
		Cvar_Set(var, buf);
		return;
	} else if (!strcmp (a2, "pos")) {
		var2 = Cvar_FindVar (a3);
		if (!var2) {
			Com_Printf ("Unknown variable \"%s\"\n", a3);
			return;
		}
		op = strstr (var2->string, Cmd_Argv(4));
		if (op)
			Cvar_SetValue (var, op - var2->string);
		else
			Cvar_SetValue (var, -1);
		return;
	} 

	num1 = Q_atof(a2);
	op = a3;
	num2 = Q_atof(Cmd_Argv(4));

	if (!strcmp(op, "+"))
		result = num1 + num2;
	else if (!strcmp(op, "-"))
		result = num1 - num2;
	else if (!strcmp(op, "*"))
		result = num1 * num2;
	else if (!strcmp(op, "/")) {
		if (num2 != 0)
			result = num1 / num2;
		else 
			division_by_zero = 1;
	} else if (!strcmp(op, "%")) {
		if ((int)num2 != 0)
			result = (int)num1 % (int)num2;
		else 
			division_by_zero = 1;
	} else if (!strcmp(op, "div")) {
		if ((int)num2 != 0)
			result = (int)num1 / (int)num2;
		else 
			division_by_zero = 1;
	} else if (!strcmp(op, "and")) {
		result = (int)num1 & (int)num2;
	} else if (!strcmp(op, "or")) {
		result = (int)num1 | (int)num2;
	} else if (!strcmp(op, "xor")) {
		result = (int)num1 ^ (int)num2;
	}else {
		Com_Printf("set_calc: unknown operator: %s\nvalid operators are: + - * / div %% and or xor\n", op);
		// Com_Printf("set_calc: unknown command: %s\nvalid commands are: strlen int substr set_substr pos\n", a2);
		return;
	}
	
	if (division_by_zero) {
		Com_Printf("set_calc: can't divide by zero\n");
		result = 999;
	}

	Cvar_SetValue (var, result);
}

// <-- from qw262


void Cvar_Inc_f (void) {
	int c;
	cvar_t *var;
	float delta;

	c = Cmd_Argc();
	if (c != 2 && c != 3) {
		Com_Printf ("inc <cvar> [value]\n");
		return;
	}

	var = Cvar_FindVar (Cmd_Argv(1));
	if (!var) {
		Com_Printf ("Unknown variable \"%s\"\n", Cmd_Argv(1));
		return;
	}
	delta = (c == 3) ? atof (Cmd_Argv(2)) : 1;

	Cvar_SetValue (var, var->value + delta);
}

void Cvar_Init (void) {
	Cmd_AddCommand ("cvarlist", Cvar_CvarList_f);
	Cmd_AddCommand ("toggle", Cvar_Toggle_f);
	Cmd_AddCommand ("set", Cvar_Set_f);
	Cmd_AddCommand ("unset", Cvar_UnSet_f);
	//Cmd_AddCommand ("seta", Cvar_Seta_f);
	Cmd_AddCommand ("inc", Cvar_Inc_f);
	Cmd_AddCommand ("set_calc", Cvar_Set_Calc_f);

	Cmd_AddCommand ("cvar_reset", Cvar_Reset_f);


	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register (&cvar_viewdefault);		

	Cvar_ResetCurrentGroup();
}

#ifndef SERVERONLY
// QW262 -->
// regexp match support for group operations in scripts
int			wildcard_level = 0;
pcre			*wildcard_re[4];
pcre_extra		*wildcard_re_extra[4];

qboolean IsRegexp(char *str)
{
	if (*str == '+' || *str == '-') // +/- aliases; valid regexp can not start with +/-
		return false;

	return (strcspn(str, "\\\"()[]{}.*+?^$|")) != strlen(str) ? true : false;
}

qboolean ReSearchInit (char *wildcard)
{
	const char	*error;
	int		error_offset;

	if (wildcard_level == 4) {
		Com_Printf("Error: Regexp commands nested too deep\n");
		return false;
	}
	wildcard_re[wildcard_level] = pcre_compile(wildcard, 0, &error, &error_offset, NULL);
	if (error) {
		Com_Printf ("Invalid regexp: %s\n", error);
		return false;
	}

	error = NULL;
	wildcard_re_extra[wildcard_level] = pcre_study(wildcard_re[wildcard_level], 0, &error);
	if (error) {
		Com_Printf ("Regexp study error: %s\n", &error);
		return false;
	}

	wildcard_level++;
	return true;
}

qboolean ReSearchMatch (char *str)
{
	int result;
	int offsets[99];

	result = pcre_exec(wildcard_re[wildcard_level-1], 
		wildcard_re_extra[wildcard_level-1], str, strlen(str), 0, 0, offsets, 99);
	return (result>0) ? true : false;
}

void ReSearchDone (void)
{
	wildcard_level--;
	if (wildcard_re[wildcard_level]) (pcre_free)(wildcard_re[wildcard_level]);
	if (wildcard_re_extra[wildcard_level]) (pcre_free)(wildcard_re_extra[wildcard_level]);
}

// <-- QW262
#endif
