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

// cvar.h
#ifndef __CVAR_H__
#define __CVAR_H__

#ifndef SERVERONLY
#include "cvar_groups.h"
#endif

// cvar flags
#define CVAR_NONE			 (0)
#define CVAR_ARCHIVE         (1<<0)  // !!!obsolete!!!
#define CVAR_USERINFO        (1<<1)  // mirrored to userinfo
#define CVAR_SERVERINFO      (1<<2)  // mirrored to serverinfo
#define CVAR_ROM             (1<<3)  // read only
#define CVAR_INIT            (1<<4)  // can only be set during initialization
#define	CVAR_USER_CREATED    (1<<5)  // created by a set command
#define	CVAR_USER_ARCHIVE    (1<<6)  // created by a seta command !!!obsolete!!!
#define CVAR_RULESET_MAX     (1<<7)  // limited by ruleset
#define CVAR_RULESET_MIN     (1<<8)  // limited by ruleset
#define CVAR_NO_RESET        (1<<9)  // do not perform reset to default in /cfg_load command, but /cvar_reset will still work
#define CVAR_TEMP            (1<<10) // created during config.cfg execution, before subsystems are initialized
#define CVAR_LATCH           (1<<11) // will only change when C code next does a Cvar_Register(), so it can't be changed
                                     // without proper initialization.  modified will be set, even though the value hasn't changed yet
#define CVAR_SILENT          (1<<12) // skip warning when trying Cvar_Register() second time
#define CVAR_COLOR           (1<<13) // on change convert the string to internal RGBA type
#define CVAR_AUTO            (1<<14) // Cvar can possibly have an automatically calculated value that shouldn't be saved, but still presentable
#define CVAR_MOD_CREATED     (1<<15) // Cvar created by the mod issuing 'set' command
#define CVAR_RECOMPILE_PROGS (1<<16) // Flag all programs to be recompiled if the value changes
#define CVAR_TRACKERCOLOR    (1<<17) // Convert the string from tracker format (0-9)(0-9)(0-9)
#define CVAR_QUEUED_TRIGGER  (1<<18) // Found in config and then registered...
#define CVAR_AUTOSETRECENT   (1<<19) // Ugh... temporary flag so Cvar_SetEx() knows if auto-value set during on-change event
#define CVAR_USERINFONORESET (1<<20) // won't be reset by cfg_reset/cfg_load when 

#define CVAR_USERINFO_NO_CFG_RESET (CVAR_USERINFO | CVAR_USERINFONORESET)

typedef struct cvar_s {
	char    *name;
	char    *string;
	int     flags;
	void    (*OnChange)(struct cvar_s *var, char *value, qbool *cancel);
	float   value;              // may be set in Cvar_Set(), Cvar_Register(), Cvar_Create()

#ifndef SERVERONLY
	float   maxrulesetvalue;
	float   minrulesetvalue;
	char    *defaultvalue;
	char    *latchedString;
	char    *autoString;        // Must be set via Cvar_AutoSet()
	int     integer;            // may be set in Cvar_Set(), Cvar_Register(), Cvar_Create()
	byte    color[4];           // gets set in Cvar_Set, Cvar_Register, Cvar_Create
	qbool   modified;           // set to true in Cvar_Set(), Cvar_Register(), Cvar_Create(), reset to false manually in C code
	qbool   teamplay;           // is this variable protected so that it can be only used within messaging?

	struct cvar_group_s *group;
	struct cvar_s       *next_in_group;
#endif

	struct cvar_s *hash_next;
	struct cvar_s *next;
} cvar_t;

#ifndef SERVERONLY
typedef struct cvar_group_s {
	char	name[65];
	int		count;
	cvar_t	*head;
	struct cvar_group_s *next;
} cvar_group_t;
#else
#define Cvar_SetCurrentGroup(...)		// ezquake compatibility
#define Cvar_ResetCurrentGroup(...)		// ezquake compatibility
#endif

// registers a cvar that already has the name, string, and optionally the
// archive elements set.
void Cvar_Register (cvar_t *variable);

// Use this to walk through all vars
cvar_t *Cvar_Next (cvar_t *var);

// creates a cvar dynamically
cvar_t *Cvar_Create (const char *name, const char *string, int cvarflags);

// equivalent to "<name> <variable>" typed at the console
void Cvar_Set (cvar_t *var, char *value);
void Cvar_SetIgnoreCallback(cvar_t *var, char *value);

// equivalent to "<name> <variable>" typed at the console
void Cvar_SetByName (const char *var_name, char *value);
void Cvar_SetValueByName (const char *var_name, const float value);
void Cvar_LatchedSetValue(cvar_t *var, float value);

// expands value to a string and calls Cvar_Set
void Cvar_SetValue (cvar_t *var, const float value);

// force a set even if the cvar is read only
void Cvar_ForceSet (cvar_t *var, char *value);
#define Cvar_SetROM Cvar_ForceSet

// toggle boolean variable
void Cvar_Toggle (cvar_t *var);

int Cvar_GetFlags (cvar_t *var);

// returns 0 if not defined or non numeric
float Cvar_Value (const char *var_name);

// returns an empty string if not defined
char *Cvar_String (const char *var_name);

// called by Cmd_ExecuteString when Cmd_Argv(0) doesn't match a known
// command.  Returns true if the command was a variable reference that
// was handled. (print or change)
qbool Cvar_Command (void);

cvar_t *Cvar_Find (const char *name);
qbool Cvar_Delete (const char *name);

void Cvar_Init(void);
void Cvar_Shutdown(void);

#ifndef SERVERONLY
// Used for automatically calculated variables (for presentation)
// but not to be saved in config
void Cvar_AutoSet(cvar_t *var, char *value);
void Cvar_AutoSetInt(cvar_t *var, int value);
void Cvar_AutoReset(cvar_t *var);

// equivalent to "<name> <variable>" typed at the console, but silent for latched var
void Cvar_LatchedSet (cvar_t *var, char *value);

// sets ruleset limit for variable
// when ruleset is active you can't set lower/higher value than this
void Cvar_RulesetSet(cvar_t *var, char *val, int m); // m=0 --> min, m=1--> max, m=2-->locked(max&min)

void Cvar_SetFlags (cvar_t *var, int flags);

void Cvar_SetDefaultAndValue(cvar_t *var, float default_value, float actual_value);

// attempts to match a partial variable name for command line completion
// returns NULL if nothing fits
char  *Cvar_CompleteVariable (char *partial);

// call OnChange callback.
qbool Cvar_ForceCallback(cvar_t *var);
void Cvar_ResetVar (cvar_t *var);

void Cvar_SetCurrentGroup(char *name);
void Cvar_ResetCurrentGroup(void);

qbool Cvar_CreateTempVar (void);	// when parsing config.cfg
void Cvar_CleanUpTempVars (void);	// clean up afterwards
#else
// ezquake compatibility - integrate into mvdsv?
#define IsRegexp(...) false
#define ReSearchInit(...) false
#define ReSearchMatch(...) false
#define ReSearchDone(...)
#endif

char* Cvar_ServerInfoValue(char* key, char* value);

void Cvar_ExecuteQueuedChanges(void);

#endif /* !__CVAR_H__ */

