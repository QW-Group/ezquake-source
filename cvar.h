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

#ifndef __CVAR_H__
#define __CVAR_H__

#include "cvar_groups.h"

// cvar flags
#define CVAR_NONE		(0)
#define CVAR_ARCHIVE		(1<<0)
#define CVAR_USERINFO		(1<<1)	// mirrored to userinfo
#define CVAR_SERVERINFO		(1<<2)	// mirrored to serverinfo
#define CVAR_ROM			(1<<3)	// read only
#define CVAR_INIT			(1<<4)	// can only be set during initialization
#define	CVAR_USER_CREATED	(1<<5)	// created by a set command
#define	CVAR_USER_ARCHIVE	(1<<6)	// created by a seta command
#define CVAR_RULESET_MAX	(1<<7)	// limited by ruleset
#define CVAR_RULESET_MIN	(1<<8)	// limited by ruleset
#define CVAR_NO_RESET		(1<<9)	// do not perform reset to default in /cfg_load command, but /cvar_reset will still work
#define CVAR_TEMP			(1<<10)	// created during config.cfg execution, before subsystems are initialized
#define CVAR_LATCH			(1<<11) // will only change when C code next does a Cvar_Register(), so it can't be changed
									// without proper initialization.  modified will be set, even though the value hasn't changed yet
#define CVAR_SILENT			(1<<12) // skip warning when trying Cvar_Register() second time
#define CVAR_COLOR			(1<<13)	// on change convert the string to internal RGBA type

typedef struct cvar_s {
	char	*name;
	char	*string;
	int		flags;
	void	(*OnChange)(struct cvar_s *var, char *value, qbool *cancel);
	float	value;    			// may be set in Cvar_Set(), Cvar_Register(), Cvar_Create()
	float	maxrulesetvalue;
	float	minrulesetvalue;
	char	*defaultvalue;
	char	*latchedString;
	int		integer;			// may be set in Cvar_Set(), Cvar_Register(), Cvar_Create()
	byte	color[4];			// gets set in Cvar_Set, Cvar_Register, Cvar_Create
	qbool	modified;			// set to true in Cvar_Set(), Cvar_Register(), Cvar_Create(), reset to false manually in C code
	qbool   teamplay;			// is this variable protected so that it can be only used within messaging?
	struct cvar_group_s *group;		
	struct cvar_s *next_in_group;	
	struct cvar_s *hash_next;
	struct cvar_s *next;
} cvar_t;

typedef struct cvar_group_s {
	char	name[65];
	int		count;
	cvar_t	*head;
	struct cvar_group_s *next;
} cvar_group_t;


// registers a cvar that already has the name, string, and optionally the
// flags set
void Cvar_Register (cvar_t *var);

// Use this to walk through all vars
cvar_t *Cvar_Next (cvar_t *var);

// creates a cvar dynamically
cvar_t *Cvar_Create (char *name, char *string, int cvarflags);

// equivalent to "<name> <variable>" typed at the console
void Cvar_Set (cvar_t *var, char *value);
// same but silent for latched var
void Cvar_LatchedSet (cvar_t *var, char *value);

// equivalent to "<name> <variable>" typed at the console
void Cvar_SetByName (char *var_name, char *value);
void Cvar_SetValueByName (char *var_name, float value);

// force a set even if the cvar is read only
void Cvar_ForceSet (cvar_t *var, char *value);

// expands value to a string and calls Cvar_Set
void Cvar_SetValue (cvar_t *var, float value);
// same but silent for latched var
void Cvar_LatchedSetValue (cvar_t *var, float value);

// toggle boolean variable
void Cvar_Toggle (cvar_t *var);

// sets ruleset limit for variable
// when ruleset is active you can't set lower/higher value than this
void Cvar_RulesetSet(cvar_t *var, char *val, int m); // m=0 --> min, m=1--> max, m=2-->locked(max&min)

void Cvar_SetFlags (cvar_t *var, int flags);
int Cvar_GetFlags (cvar_t *var);



void Cvar_SetDefault(cvar_t *var, float value);


// returns 0 if not defined or non numeric
float Cvar_Value (char *name);

// returns an empty string if not defined
char *Cvar_String (char *name);

// attempts to match a partial variable name for command line completion
// returns NULL if nothing fits
char  *Cvar_CompleteVariable (char *partial);

// called by Cmd_ExecuteString when Cmd_Argv(0) doesn't match a known
// command.  Returns true if the command was a variable reference that
// was handled. (print or change)
qbool Cvar_Command (void);

// Writes lines containing "set variable value" for all variables
// with the archive flag set to true.
void Cvar_WriteVariables (FILE *f);

cvar_t *Cvar_Find (const char *name);
qbool Cvar_Delete (const char *name);

void Cvar_Init (void);

void Cvar_ResetVar (cvar_t *var);

void Cvar_SetCurrentGroup(char *name);
void Cvar_ResetCurrentGroup(void);

qbool Cvar_CreateTempVar (void);	// when parsing config.cfg
void Cvar_CleanUpTempVars (void);	// clean up afterwards

#endif /* !__CVAR_H__ */
