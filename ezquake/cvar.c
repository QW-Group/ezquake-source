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

#include "common.h"
#include "console.h"

#include "utils.h"

extern void CL_UserinfoChanged (char *key, char *value);
extern void SV_ServerinfoChanged (char *key, char *value);

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

void Cvar_Set (cvar_t *var, char *value) {
#ifndef SERVERONLY
	extern cvar_t cl_warncmd;	
#endif
	static qboolean	changing = false;

	if (!var)
		return;

	if (var->flags & CVAR_ROM) {
		if (con_initialized)
			Com_Printf ("\"%s\" is write protected\n", var->name);
		return;
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

void Cvar_Seta_f (void) {
	cvar_seta = true;
	Cvar_Set_f ();
	cvar_seta = false;
}

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
	//Cmd_AddCommand ("seta", Cvar_Seta_f);
	Cmd_AddCommand ("inc", Cvar_Inc_f);


	Cmd_AddCommand ("cvar_reset", Cvar_Reset_f);


	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register (&cvar_viewdefault);		

	Cvar_ResetCurrentGroup();
}
