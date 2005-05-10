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

typedef struct limited_cvar_s {
	cvar_t *var;
	char *rulesetvalue;
} limited_cvar_t;

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
		default: ;
	}
	return false;
}

qboolean Rulesets_NoTimers(void) {
	return (!cl.spectator && !cls.demoplayback && notimers);
}

qboolean Rulesets_AllowTimerefresh(void) {
	switch(ruleset) {
	case rs_smackdown:
		// START shaman BUG 1020663
		//return cl.standby;
		return (cl.standby || cl.spectator || cls.demoplayback);
		// END shaman BUG 1020663
	default:
		return true;
	}
}

qboolean Rulesets_AllowNoShadows(void) {
	switch(ruleset) {
	case rs_smackdown:
		return false;
	default:
		return true;
	}
}

float Rulesets_MaxFPS(void) {

	if (cl_multiview.value && cls.mvdplayback)
		return nNumViews*maxfps;

	return maxfps; 
}

qboolean Rulesets_AllowTriggers(void) {
	return allow_triggers;
}

char *Rulesets_Ruleset(void) {
	extern cvar_t cl_independentPhysics;
	extern cvar_t allow_scripts;
	char * sRuleset, * sScripts, * sIPhysics;
	sRuleset = (ruleset == rs_smackdown) ? "smackdown " : "default ";
	sScripts = (allow_scripts.value) ? "" : "\x90scripts blocked\x91";
	sIPhysics = (cl_independentPhysics.value) ? "" : "\x90indep. physics off\x91";
	return va("%s %s %s", sRuleset, sScripts, sIPhysics);
}

static void Rulesets_Smackdown(void) {
	extern cvar_t tp_triggers, tp_msgtriggers, cl_trueLightning, scr_clock, r_aliasstats;
	extern cvar_t cl_independentPhysics, cl_c2spps;
#ifdef GLQUAKE
	extern cvar_t amf_camera_death, amf_camera_chase, amf_part_gunshot_type, amf_part_traillen, amf_part_trailtime, amf_part_trailwidth, amf_part_traildetail, amf_part_trailtype, amf_part_sparks, amf_part_spikes, amf_part_gunshot, amf_waterripple, amf_lightning, amf_lightning_size, amf_lightning_size, amf_lightning_sparks;
	extern qboolean qmb_initialized;
#endif
	int i;

#define NOQMB_SKIP_LOCKED 6

	locked_cvar_t disabled_cvars[] = {
#ifdef GLQUAKE
		{&amf_camera_chase, "0"},
		{&amf_camera_death, "0"},
		{&amf_part_sparks, "0"},
		{&amf_waterripple, "0"},
		{&amf_lightning, "0"},
		{&amf_lightning_sparks, "0"},
#endif
		{&tp_msgtriggers, "0"},
		{&cl_trueLightning, "0"},
#ifndef GLQUAKE
		{&r_aliasstats, "0"}
#endif
	};
	
#ifdef GLQUAKE
		limited_cvar_t limited_cvars[] = {
		{&amf_part_gunshot_type, "1"},
		{&amf_part_traillen, "1"},
		{&amf_part_trailtime, "1"},
		{&amf_part_trailwidth, "1"},
		{&amf_part_traildetail, "1"},
		{&amf_part_trailtype, "1"},
		{&amf_part_spikes, "1"},
		{&amf_part_gunshot, "1"},
		{&amf_lightning_size, "1"},
		};
#endif

	i = 0;
#ifdef GLQUAKE
	if (!qmb_initialized) i = NOQMB_SKIP_LOCKED + 1;
#endif

	for (; i < (sizeof(disabled_cvars) / sizeof(disabled_cvars[0])); i++) {
		Cvar_RulesetSet(disabled_cvars[i].var, disabled_cvars[i].value);
		Cvar_Set(disabled_cvars[i].var, disabled_cvars[i].value);
		Cvar_SetFlags(disabled_cvars[i].var, Cvar_GetFlags(disabled_cvars[i].var) | CVAR_ROM);
	}

#ifdef GLQUAKE
	// there are no limited variables when not using GL or when not using -no24bit cmd-line option
	if (qmb_initialized)
	{
		for (i = 0; i < (sizeof(limited_cvars) / sizeof(limited_cvars[0])); i++) {
		Cvar_RulesetSet(limited_cvars[i].var, limited_cvars[i].rulesetvalue);
		Cvar_SetFlags(limited_cvars[i].var, Cvar_GetFlags(limited_cvars[i].var) | CVAR_RULESET_MAX);
		}
	}
#endif

	if (cl_independentPhysics.value)
	{
		Cvar_Set(&cl_c2spps, "0");
		Cvar_SetFlags(&cl_c2spps, Cvar_GetFlags(&cl_c2spps) | CVAR_ROM);
	}

	maxfps = 77;
	allow_triggers = false;
	notimers = true;
	ruleset = rs_smackdown;
	restrictTriggers = true;

/* no more needed since hexum made better fix for tp macros 
	for (i = 0; allowed_smackdown_macros[i]; i++)
		Cmd_SetMacro(allowed_smackdown_macros[i], true);
*/
}

static void Rulesets_Default(void) {
	ruleset = rs_default;
	Cmd_SetAllMacros(true);
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
