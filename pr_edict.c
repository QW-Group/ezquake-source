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
// sv_edict.c -- entity dictionary

#include "qwsvdef.h"
#include "crc.h"

dprograms_t		*progs;
dfunction_t		*pr_functions;
char			*pr_strings;
ddef_t			*pr_fielddefs;
ddef_t			*pr_globaldefs;
dstatement_t	*pr_statements;
globalvars_t	*pr_global_struct;
float			*pr_globals;			// same as pr_global_struct
int				pr_edict_size;	// in bytes

int type_size[8] = {1, sizeof(void *) / 4, 1 ,3, 1, 1, sizeof(void *) / 4, sizeof(void *) / 4};

ddef_t *ED_FieldAtOfs (int ofs);
qboolean ED_ParseEpair (void *base, ddef_t *key, char *s);

#define	MAX_FIELD_LEN	64
#define GEFV_CACHESIZE	2

typedef struct {
	ddef_t	*pcache;
	char	field[MAX_FIELD_LEN];
} gefv_cache;

static gefv_cache	gefvCache[GEFV_CACHESIZE] = {{NULL, ""}, {NULL, ""}};

func_t SpectatorConnect;
func_t SpectatorThink;
func_t SpectatorDisconnect;

func_t SV_ParseClientCommand;		//KRIMZON_SV_PARSECLIENTCOMMAND


//Sets everything to NULL
void ED_ClearEdict (edict_t *e) {
	memset (&e->v, 0, progs->entityfields * 4);
	e->free = false;
}

/*
Either finds a free edict, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it can cause the 
client to think the entity morphed into something else instead of being removed and 
recreated, which can cause interpolated angles and bad trails.
*/
edict_t *ED_Alloc (void) {
	int i;
	edict_t *e;

	for (i = MAX_CLIENTS + 1; i < sv.num_edicts; i++) {
		e = EDICT_NUM(i);
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if (e->free && ( e->freetime < 2 || sv.time - e->freetime > 0.5 ) ) {
			ED_ClearEdict (e);
			return e;
		}
	}

	if (i == MAX_EDICTS) {
		Com_Printf ("WARNING: ED_Alloc: no free edicts\n");
		i--;	// step on whatever is the last edict
		e = EDICT_NUM(i);
		SV_UnlinkEdict(e);
	} else {
		sv.num_edicts++;
	}
	e = EDICT_NUM(i);
	ED_ClearEdict (e);

	return e;
}

//Marks the edict as free
//FIXME: walk all entities and NULL out references to this entity
void ED_Free (edict_t *ed) {
	SV_UnlinkEdict (ed);		// unlink from world bsp

	ed->free = true;
	ed->v.model = 0;
	ed->v.takedamage = 0;
	ed->v.modelindex = 0;
	ed->v.colormap = 0;
	ed->v.skin = 0;
	ed->v.frame = 0;
	VectorClear (ed->v.origin);
	VectorClear (ed->v.angles);
	ed->v.nextthink = -1;
	ed->v.solid = 0;

	ed->freetime = sv.time;
}

//===========================================================================

ddef_t *ED_GlobalAtOfs (int ofs) {
	ddef_t *def;
	int i;
	
	for (i = 0; i < progs->numglobaldefs; i++) {
		def = &pr_globaldefs[i];
		if (def->ofs == ofs)
			return def;
	}
	return NULL;
}

ddef_t *ED_FieldAtOfs (int ofs) {
	ddef_t *def;
	int i;
	
	for (i = 0; i < progs->numfielddefs; i++) {
		def = &pr_fielddefs[i];
		if (def->ofs == ofs)
			return def;
	}
	return NULL;
}

ddef_t *ED_FindField (char *name) {
	ddef_t *def;
	int i;
	
	for (i = 0; i < progs->numfielddefs; i++) {
		def = &pr_fielddefs[i];
		if (!strcmp(PR_GetString(def->s_name),name) )
			return def;
	}
	return NULL;
}

ddef_t *ED_FindGlobal (char *name) {
	ddef_t *def;
	int i;
	
	for (i = 0; i < progs->numglobaldefs; i++) {
		def = &pr_globaldefs[i];
		if (!strcmp(PR_GetString(def->s_name), name) )
			return def;
	}
	return NULL;
}

dfunction_t *ED_FindFunction (char *name) {
	dfunction_t *func;
	int i;
	
	for (i = 0; i < progs->numfunctions; i++) {
		func = &pr_functions[i];
		if (!strcmp(PR_GetString(func->s_name),name) )
			return func;
	}
	return NULL;
}

eval_t *GetEdictFieldValue(edict_t *ed, char *field) {
	ddef_t *def = NULL;
	int i;
	static int rep = 0;

	for (i = 0; i < GEFV_CACHESIZE; i++) {
		if (!strcmp(field, gefvCache[i].field)) {
			def = gefvCache[i].pcache;
			goto Done;
		}
	}

	def = ED_FindField (field);

	if (strlen(field) < MAX_FIELD_LEN) {
		gefvCache[rep].pcache = def;
		strcpy (gefvCache[rep].field, field);
		rep ^= 1;
	}

Done:
	if (!def)
		return NULL;

	return (eval_t *) ((char *)&ed->v + def->ofs*4);
}

//Returns a string describing *data in a type specific manner
char *PR_ValueString (etype_t type, eval_t *val) {
	static char	line[256];
	ddef_t *def;
	dfunction_t *f;
	
	type &= ~DEF_SAVEGLOBAL;

	switch (type) {
	case ev_string:
		sprintf (line, "%s", PR_GetString(val->string));
		break;
	case ev_entity:	
		sprintf (line, "entity %i", NUM_FOR_EDICT(PROG_TO_EDICT(val->edict)) );
		break;
	case ev_function:
		f = pr_functions + val->function;
		sprintf (line, "%s()", PR_GetString(f->s_name));
		break;
	case ev_field:
		def = ED_FieldAtOfs ( val->_int );
		sprintf (line, ".%s", PR_GetString(def->s_name));
		break;
	case ev_void:
		sprintf (line, "void");
		break;
	case ev_float:
		sprintf (line, "%5.1f", val->_float);
		break;
	case ev_vector:
		sprintf (line, "'%5.1f %5.1f %5.1f'", val->vector[0], val->vector[1], val->vector[2]);
		break;
	case ev_pointer:
		sprintf (line, "pointer");
		break;
	default:
		sprintf (line, "bad type %i", type);
		break;
	}
	
	return line;
}

//Returns a string describing *data in a type specific manner
//Easier to parse than PR_ValueString
char *PR_UglyValueString (etype_t type, eval_t *val) {
	static char line[256];
	ddef_t *def;
	dfunction_t *f;
	
	type &= ~DEF_SAVEGLOBAL;

	switch (type) {
	case ev_string:
		sprintf (line, "%s", PR_GetString(val->string));
		break;
	case ev_entity:	
		sprintf (line, "%i", NUM_FOR_EDICT(PROG_TO_EDICT(val->edict)));
		break;
	case ev_function:
		f = pr_functions + val->function;
		sprintf (line, "%s", PR_GetString(f->s_name));
		break;
	case ev_field:
		def = ED_FieldAtOfs ( val->_int );
		sprintf (line, "%s", PR_GetString(def->s_name));
		break;
	case ev_void:
		sprintf (line, "void");
		break;
	case ev_float:
		sprintf (line, "%f", val->_float);
		break;
	case ev_vector:
		sprintf (line, "%f %f %f", val->vector[0], val->vector[1], val->vector[2]);
		break;
	default:
		sprintf (line, "bad type %i", type);
		break;
	}
	
	return line;
}

//Returns a string with a description and the contents of a global, padded to 20 field width
char *PR_GlobalString (int ofs) {
	char *s;
	int i;
	ddef_t *def;
	void *val;
	static char	line[128];
	
	val = (void *) &pr_globals[ofs];
	def = ED_GlobalAtOfs(ofs);
	if (!def) {
		sprintf (line,"%i(???)", ofs);
	} else {
		s = PR_ValueString (def->type, val);
		Q_snprintfz (line, sizeof(line), "%i(%s)%s", ofs, PR_GetString(def->s_name), s);
	}
	
	i = strlen(line);
	for ( ; i < 20; i++)
		strcat (line, " ");
	strcat (line, " ");

	return line;
}

char *PR_GlobalStringNoContents (int ofs) {
	int i;
	ddef_t *def;
	static char	line[128];

	def = ED_GlobalAtOfs(ofs);
	if (!def)
		sprintf (line,"%i(???)", ofs);
	else
		sprintf (line,"%i(%s)", ofs, PR_GetString(def->s_name));
	
	i = strlen(line);
	for ( ; i < 20; i++)
		strcat (line," ");
	strcat (line," ");
		
	return line;
}

//For debugging
void ED_Print (edict_t *ed) {
	int l, *v, i, j, type;
	ddef_t *d;
	char *name;

	if (ed->free) {
		Com_Printf ("FREE\n");
		return;
	}

	for (i = 1; i < progs->numfielddefs; i++)	{
		d = &pr_fielddefs[i];
		name = PR_GetString(d->s_name);
		if (name[strlen(name)-2] == '_')
			continue;	// skip _x, _y, _z vars

		v = (int *)((char *)&ed->v + d->ofs*4);

		// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;
		
		for (j = 0; j < type_size[type]; j++)
			if (v[j])
				break;
		if (j == type_size[type])
			continue;

		Com_Printf ("%s",name);
		l = strlen (name);
		while (l++ < 15)
			Com_Printf (" ");

		Com_Printf ("%s\n", PR_ValueString(d->type, (eval_t *) v));		
	}
}

//For savegames
void ED_Write (FILE *f, edict_t *ed) {
	ddef_t *d;
	int *v, i, j, type;
	char *name;

	fprintf (f, "{\n");

	if (ed->free) {
		fprintf (f, "}\n");
		return;
	}

	for (i = 1; i < progs->numfielddefs; i++) {
		d = &pr_fielddefs[i];
		name = PR_GetString(d->s_name);
		if (name[strlen(name) - 2] == '_')
			continue;	// skip _x, _y, _z vars
			
		v = (int *)((char *)&ed->v + d->ofs*4);

		// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;
		for (j = 0; j < type_size[type]; j++)
			if (v[j])
				break;
		if (j == type_size[type])
			continue;
	
		fprintf (f,"\"%s\" ",name);
		fprintf (f,"\"%s\"\n", PR_UglyValueString(d->type, (eval_t *)v));		
	}

	fprintf (f, "}\n");
}

void ED_PrintNum (int ent) {
	ED_Print (EDICT_NUM(ent));
}

//For debugging, prints all the entities in the current server
void ED_PrintEdicts (void) {
	int i;

	if (sv.state < ss_active)
		return;

	Com_Printf ("%i entities\n", sv.num_edicts);
	for (i = 0; i < sv.num_edicts; i++) {
		Com_Printf ("\nEDICT %i:\n",i);
		ED_PrintNum (i);
	}
}

//For debugging, prints a single edicy
void ED_PrintEdict_f (void) {
	int i;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <edict num>\n", Cmd_Argv(0));
		return;
	}

	if (sv.state < ss_active)
		return;

	i = Q_atoi (Cmd_Argv(1));
	if (i < 0 || i >= sv.num_edicts) {
		Com_Printf("Error: Bad edict (#%d)\n", i);
		return;
	}

	Com_Printf ("\n EDICT %i:\n",i);
	ED_PrintNum (i);
}

//For debugging
void ED_Count (void) {
	int i, active, models, solid, step;
	edict_t *ent;

	if (sv.state < ss_active)
		return;

	active = models = solid = step = 0;
	for (i = 0; i < sv.num_edicts; i++) {
		ent = EDICT_NUM(i);
		if (ent->free)
			continue;
		active++;
		if (ent->v.solid)
			solid++;
		if (ent->v.model)
			models++;
		if (ent->v.movetype == MOVETYPE_STEP)
			step++;
	}

	Com_Printf ("num_edicts:%3i\n", sv.num_edicts);
	Com_Printf ("active    :%3i\n", active);
	Com_Printf ("view      :%3i\n", models);
	Com_Printf ("touch     :%3i\n", solid);
	Com_Printf ("step      :%3i\n", step);

}

/*
==============================================================================
					ARCHIVING GLOBALS
FIXME: need to tag constants, doesn't really work
==============================================================================
*/

void ED_WriteGlobals (FILE *f) {
	ddef_t *def;
	int i, type;
	char *name;

	fprintf (f,"{\n");
	for (i = 0; i<progs->numglobaldefs; i++) {
		def = &pr_globaldefs[i];
		type = def->type;
		if (!(def->type & DEF_SAVEGLOBAL))
			continue;
		type &= ~DEF_SAVEGLOBAL;

		if (type != ev_string && type != ev_float && type != ev_entity)
			continue;

		name = PR_GetString(def->s_name);
		fprintf (f,"\"%s\" ", name);
		fprintf (f,"\"%s\"\n", PR_UglyValueString(type, (eval_t *)&pr_globals[def->ofs]));		
	}
	fprintf (f,"}\n");
}

void ED_ParseGlobals (char *data) {
	char keyname[64];
	ddef_t *key;

	while (1) {	
		// parse key
		data = COM_Parse (data);
		if (com_token[0] == '}')
			break;
		if (!data)
			Host_Error ("ED_ParseEntity: EOF without closing brace");

		strcpy (keyname, com_token);

		// parse value	
		data = COM_Parse (data);
		if (!data)
			Host_Error ("ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			Host_Error ("ED_ParseEntity: closing brace without data");

		if (!(key = ED_FindGlobal (keyname))) {
			Com_Printf ("%s is not a global\n", keyname);
			continue;
		}

		if (!ED_ParseEpair ((void *)pr_globals, key, com_token))
			Host_Error ("ED_ParseGlobals: parse error");
	}
}

//============================================================================

char *ED_NewString (char *string) {
	char *new, *new_p;
	int i, l;
	
	l = strlen(string) + 1;
	new = Hunk_Alloc (l);
	new_p = new;

	for (i = 0; i < l; i++) {
		if (string[i] == '\\' && i < l-1) {
			i++;
			if (string[i] == 'n')
				*new_p++ = '\n';
			else
				*new_p++ = '\\';
		} else {
			*new_p++ = string[i];
		}
	}
	
	return new;
}


//Can parse either fields or globals
//returns false if error
qboolean ED_ParseEpair (void *base, ddef_t *key, char *s) {
	int i;
	char string[128], *v, *w;
	ddef_t *def;
	void *d;
	dfunction_t	*func;
	
	d = (void *)((int *)base + key->ofs);
	
	switch (key->type & ~DEF_SAVEGLOBAL) {
	case ev_string:
		*(string_t *)d = PR_SetString(ED_NewString (s));
		break;

	case ev_float:
		*(float *)d = atof (s);
		break;

	case ev_vector:
		strcpy (string, s);
		v = string;
		w = string;
		for (i = 0; i < 3; i++) {
			while (*v && *v != ' ')
				v++;
			*v = 0;
			((float *)d)[i] = atof (w);
			w = v = v+1;
		}
		break;

	case ev_entity:
		*(int *)d = EDICT_TO_PROG(EDICT_NUM(atoi (s)));
		break;

	case ev_field:
		if (!(def = ED_FindField (s))) {
			Com_Printf ("Can't find field %s\n", s);
			return false;
		}
		*(int *)d = G_INT(def->ofs);
		break;

	case ev_function:
		if (!(func = ED_FindFunction (s))) {
			Com_Printf ("Can't find function %s\n", s);
			return false;
		}
		*(func_t *)d = func - pr_functions;
		break;

	default:
		break;
	}
	return true;
}

//Parses an edict out of the given string, returning the new position
//ed should be a properly initialized empty edict.
//Used for initial level load and for savegames.
char *ED_ParseEdict (char *data, edict_t *ent) {
	ddef_t *key;
	qboolean skyhack, anglehack, init;
	char keyname[256], value[1024];

	init = false;

	// clear it
	if (ent != sv.edicts)	// hack
		memset (&ent->v, 0, progs->entityfields * 4);

	// go through all the dictionary pairs
	while (1) {	
		skyhack = anglehack = false;

		// parse key
		data = COM_Parse (data);
		if (com_token[0] == '}')
			break;
		if (!data)
			Host_Error ("ED_ParseEntity: EOF without closing brace");

		Q_strncpyz (keyname, com_token, sizeof(keyname));

		// parse value	
		if (!(data = COM_Parse (data)))
			Host_Error ("ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			Host_Error ("ED_ParseEntity: closing brace without data");

		Q_strncpyz (value, com_token, sizeof(value));

		init = true;

		// keynames with a leading underscore are used for utility comments, and are immediately discarded by quake
		if (keyname[0] == '_')
			continue;

		// anglehack is to allow QuakeEd to write single scalar angles and allow them to be turned into vectors. (FIXME...)
		if (!strcmp(keyname, "angle")) {
			strcpy (keyname, "angles");
			anglehack = true;
		} else if (!strcmp(keyname, "light")) {	// hack for single light def
			strcpy (keyname, "light_lev");
		} else if (!strcmp(keyname, "sky") || !strcmp(keyname, "skyname")) {
			if (ent == sv.edicts && !strstr(value, "..")) {
				skyhack = true;
				Q_strncpyz (sv.sky, value, sizeof(sv.sky));
			}
		}

		if (!(key = ED_FindField (keyname))) {
			if (!skyhack)
				Com_Printf ("%s is not a field\n", keyname);
			continue;
		}

		if (anglehack)
			Q_strncpyz (value, va("0 %s 0", value), sizeof(value));

		if (!ED_ParseEpair ((void *) &ent->v, key, value))
			Host_Error ("ED_ParseEdict: parse error");
	}

	if (!init)
		ent->free = true;

	return data;
}

/*
The entities are directly placed in the array, rather than allocated with
ED_Alloc, because otherwise an error loading the map would have entity
number references out of order.

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.

Used for both fresh maps and savegame loads.  A fresh map would also need
to call ED_CallSpawnFunctions () to let the objects initialize themselves.
*/
void ED_LoadFromFile (char *data) {	
	edict_t *ent;
	int inhibit;
	dfunction_t	*func;
	
	ent = NULL;
	inhibit = 0;
	pr_global_struct->time = sv.time;

	// parse ents
	while (1) {
	// parse the opening brace	
		data = COM_Parse (data);
		if (!data)
			break;
		if (com_token[0] != '{')
			Host_Error ("ED_LoadFromFile: found %s when expecting {",com_token);

		ent = ent ? ED_Alloc () : EDICT_NUM(0);
		data = ED_ParseEdict (data, ent);

		// remove things from different skill levels or deathmatch
		if (deathmatch.value) {
			if (((int)ent->v.spawnflags & SPAWNFLAG_NOT_DEATHMATCH)) {
				ED_Free (ent);
				inhibit++;
				continue;
			}
		} else if ((current_skill == 0 && ((int)ent->v.spawnflags & SPAWNFLAG_NOT_EASY))
			|| (current_skill == 1 && ((int)ent->v.spawnflags & SPAWNFLAG_NOT_MEDIUM))
			|| (current_skill >= 2 && ((int)ent->v.spawnflags & SPAWNFLAG_NOT_HARD)) )
		{
			ED_Free (ent);
			inhibit++;
			continue;
		}

		// immediately call spawn function
		if (!ent->v.classname) {
			Com_Printf ("No classname for:\n");
			ED_Print (ent);
			ED_Free (ent);
			continue;
		}

		// look for the spawn function
		func = ED_FindFunction ( PR_GetString(ent->v.classname) );

		if (!func) {
			Com_Printf ("No spawn function for:\n");
			ED_Print (ent);
			ED_Free (ent);
			continue;
		}

		pr_global_struct->self = EDICT_TO_PROG(ent);
		PR_ExecuteProgram (func - pr_functions);
		SV_FlushSignon();
	}	

	Com_DPrintf ("%i entities inhibited\n", inhibit);
}

void PR_LoadProgs (void) {
	int i, lowmark;
	char num[32];
	dfunction_t *f;

	// flush the non-C variable lookup cache
	for (i = 0; i < GEFV_CACHESIZE; i++)
		gefvCache[i].field[0] = 0;

	if (!deathmatch.value) {
		extern int file_from_gamedir;

		lowmark = Hunk_LowMark();
		
		if ((progs = (dprograms_t *) FS_LoadHunkFile ("spprogs.dat"))) {
			if (file_from_gamedir)
				goto progs_loaded;	
			else
				Hunk_FreeToLowMark(lowmark);
		}
		
		if ((progs = (dprograms_t *) FS_LoadHunkFile ("qwprogs.dat"))) {
			if (file_from_gamedir)
				goto progs_loaded;	
			else
				Hunk_FreeToLowMark(lowmark);
		}
		
		if ((progs = (dprograms_t *) FS_LoadHunkFile ("spprogs.dat")))
			goto progs_loaded;

		
		progs = (dprograms_t *) FS_LoadHunkFile ("qwprogs.dat");
	} else {
		progs = (dprograms_t *) FS_LoadHunkFile ("qwprogs.dat");
	}

progs_loaded:

	if (!progs)
		Host_Error ("PR_LoadProgs: couldn't load qwprogs.dat");

	Com_DPrintf ("Programs occupy %iK.\n", com_filesize / 1024);

	// add prog crc to the serverinfo
	sprintf (num, "%i", CRC_Block ((byte *)progs, com_filesize));
	Info_SetValueForStarKey (svs.info, "*progs", num, MAX_SERVERINFO_STRING);

	// byte swap the header
	for (i = 0; i < sizeof(*progs) / 4; i++)
		((int *) progs)[i] = LittleLong ( ((int *)progs)[i] );		

	if (progs->version != PROG_VERSION)
		Host_Error ("progs.dat has wrong version number (%i should be %i)", progs->version, PROG_VERSION);
	if (progs->crc != PROGHEADER_CRC)
		Host_Error ("You must have the progs.dat from QuakeWorld installed");

	pr_functions = (dfunction_t *) ((byte *) progs + progs->ofs_functions);
	pr_strings = (char *) progs + progs->ofs_strings;
	pr_globaldefs = (ddef_t *) ((byte *) progs + progs->ofs_globaldefs);
	pr_fielddefs = (ddef_t *) ((byte *) progs + progs->ofs_fielddefs);
	pr_statements = (dstatement_t *) ((byte *) progs + progs->ofs_statements);

	num_prstr = 0;

	pr_global_struct = (globalvars_t *) ((byte *) progs + progs->ofs_globals);
	pr_globals = (float *) pr_global_struct;
	
	pr_edict_size = progs->entityfields * 4 + sizeof (edict_t) - sizeof(entvars_t);
	
	// byte swap the lumps
	for (i = 0; i < progs->numstatements; i++) {
		pr_statements[i].op = LittleShort(pr_statements[i].op);
		pr_statements[i].a = LittleShort(pr_statements[i].a);
		pr_statements[i].b = LittleShort(pr_statements[i].b);
		pr_statements[i].c = LittleShort(pr_statements[i].c);
	}

	for (i = 0; i < progs->numfunctions; i++) {
		pr_functions[i].first_statement = LittleLong (pr_functions[i].first_statement);
		pr_functions[i].parm_start = LittleLong (pr_functions[i].parm_start);
		pr_functions[i].s_name = LittleLong (pr_functions[i].s_name);
		pr_functions[i].s_file = LittleLong (pr_functions[i].s_file);
		pr_functions[i].numparms = LittleLong (pr_functions[i].numparms);
		pr_functions[i].locals = LittleLong (pr_functions[i].locals);
	}	

	for (i = 0; i < progs->numglobaldefs; i++) {
		pr_globaldefs[i].type = LittleShort (pr_globaldefs[i].type);
		pr_globaldefs[i].ofs = LittleShort (pr_globaldefs[i].ofs);
		pr_globaldefs[i].s_name = LittleLong (pr_globaldefs[i].s_name);
	}

	for (i = 0; i < progs->numfielddefs; i++) {
		pr_fielddefs[i].type = LittleShort (pr_fielddefs[i].type);
		if (pr_fielddefs[i].type & DEF_SAVEGLOBAL)
			Host_Error ("PR_LoadProgs: pr_fielddefs[i].type & DEF_SAVEGLOBAL");
		pr_fielddefs[i].ofs = LittleShort (pr_fielddefs[i].ofs);
		pr_fielddefs[i].s_name = LittleLong (pr_fielddefs[i].s_name);
	}

	for (i = 0; i < progs->numglobals; i++)
		((int *) pr_globals)[i] = LittleLong (((int *) pr_globals)[i]);

	// Zoid, find the spectator functions
	SpectatorConnect = (f = ED_FindFunction ("SpectatorConnect")) ? (func_t) (f - pr_functions) : 0;
	SpectatorThink = (f = ED_FindFunction ("SpectatorThink")) ? (func_t) (f - pr_functions) : 0;
	SpectatorDisconnect = (f = ED_FindFunction ("SpectatorDisconnect")) ? (func_t) (f - pr_functions) : 0;

	//KRIMZON_SV_PARSECLIENTCOMMAND
	SV_ParseClientCommand = (f = ED_FindFunction ("SV_ParseClientCommand")) ? (func_t) (f - pr_functions) : 0;
}

void PR_Init (void) {
	Cmd_AddCommand ("edict", ED_PrintEdict_f);
	Cmd_AddCommand ("edicts", ED_PrintEdicts);
	Cmd_AddCommand ("edictcount", ED_Count);
	Cmd_AddCommand ("profile", PR_Profile_f);
}

edict_t *EDICT_NUM(int n) {
	if (n < 0 || n >= MAX_EDICTS)
		Host_Error ("EDICT_NUM: bad number %i", n);
	return (edict_t *)((byte *)sv.edicts+ (n)*pr_edict_size);
}

int NUM_FOR_EDICT(edict_t *e) {
	int b;

	b = (byte *) e - (byte *) sv.edicts;
	b = b / pr_edict_size;

	if (b < 0 || b >= sv.num_edicts)
		Host_Error ("NUM_FOR_EDICT: bad pointer");
	return b;
}
