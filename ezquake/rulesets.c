/*

Copyright (C) 2001-2002       A Nourai

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "quakedef.h"

typedef struct locked_cvar_s {
	cvar_t *var;
	char *value;
} locked_cvar_t;

typedef enum {rs_default, rs_smackdown} ruleset_t;

static ruleset_t ruleset;

static float maxfps = 72;
static qboolean allow_triggers = true;
static qboolean restrictTriggers = false;

static qboolean notimers = false;

qboolean RuleSets_DisallowExternalTexture(model_t *mod) {
	switch (mod->modhint) {
		case MOD_EYES: return true;
		case MOD_BACKPACK: return (ruleset == rs_smackdown);
	}
	return false;
}

qboolean Rulesets_NoTimers(void) {
	return (!cl.spectator && !cls.demoplayback && notimers);
}

qboolean Rulesets_AllowTimerefresh(void) {
	switch(ruleset) {
	case rs_smackdown:
		return cl.standby;
	default:
		return true;
	}
}

float Rulesets_MaxFPS(void) {
	return maxfps;
}

qboolean Rulesets_AllowTriggers(void) {
	return allow_triggers;
}

char *Rulesets_Ruleset(void) {
	switch (ruleset) {
	case rs_smackdown:
		return "smackdown";
	default:
		return "default";
	}
}

static void Rulesets_Smackdown(void) {
	extern cvar_t tp_triggers, tp_msgtriggers, cl_trueLightning, scr_clock, r_aliasstats;
	int i;

	locked_cvar_t disabled_cvars[] = {
		{&tp_msgtriggers, "0"},
		{&cl_trueLightning, "0"},
	#ifdef GLQUAKE
	#else
		{&r_aliasstats, "0"},
	#endif
	};

	for (i = 0; i < (sizeof(disabled_cvars) / sizeof(disabled_cvars[0])); i++) {
		Cvar_Set(disabled_cvars[i].var, disabled_cvars[i].value);
		Cvar_SetFlags(disabled_cvars[i].var, Cvar_GetFlags(disabled_cvars[i].var) | CVAR_ROM);
	}

	maxfps = 77;
	allow_triggers = false;
	notimers = true;
	ruleset = rs_smackdown;
	restrictTriggers = true;
}

static void Rulesets_Default(void) {
	ruleset = rs_default;
}

void Rulesets_Init(void) {
	int temp;

	if ((temp = COM_CheckParm("-ruleset")) && temp + 1 < com_argc) {
		if (!Q_strcasecmp(com_argv[temp + 1], "smackdown")) {
			Rulesets_Smackdown();
			Com_Printf("Ruleset Smackdown initialized\n");
			return;
		} else if (Q_strcasecmp(com_argv[temp + 1], "default")){
			Com_Printf("Unknown ruleset \"%s\"\n", com_argv[temp + 1]);
		}
	}
	Rulesets_Default();
}

qboolean Rulesets_RestrictTriggers(void) {
	return restrictTriggers;
}
