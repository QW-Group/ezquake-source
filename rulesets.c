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

	$Id: rulesets.c,v 1.33 2006-03-06 17:33:18 vvd0 Exp $

*/

#include "quakedef.h"

typedef struct locked_cvar_s {
	cvar_t *var;
	char *value;
} locked_cvar_t;

typedef struct limited_cvar_max_s {
	cvar_t *var;
	char *maxrulesetvalue;
} limited_cvar_max_t;

typedef struct limited_cvar_min_s {
	cvar_t *var;
	char *minrulesetvalue;
} limited_cvar_min_t;

typedef enum {rs_default, rs_smackdown, rs_mtfl} ruleset_t;

typedef struct rulesetDef_s {
	ruleset_t ruleset;
	float maxfps;
	qbool restrictTriggers;
	qbool restrictPacket;
	qbool restrictRJScripts;
} rulesetDef_t;

static rulesetDef_t rulesetDef = {rs_default, 72, false, false, false};

qbool RuleSets_DisallowRJScripts(void) {
	return rulesetDef.restrictRJScripts;
}

qbool RuleSets_DisallowExternalTexture(model_t *mod) {
	switch (mod->modhint) {
		case MOD_EYES: return true;
		case MOD_BACKPACK: return (rulesetDef.ruleset == rs_smackdown);
		default: return false;
	}
}

qbool Rulesets_AllowTimerefresh(void) {
	switch(rulesetDef.ruleset) {
	case rs_smackdown:
		// START shaman BUG 1020663
		//return cl.standby;
		return (cl.standby || cl.spectator || cls.demoplayback);
		// END shaman BUG 1020663
	default:
		return true;
	}
}

qbool Rulesets_AllowNoShadows(void) {
	switch(rulesetDef.ruleset) {
	case rs_mtfl:
		return false;
	case rs_smackdown:
		return false;
	default:
		return true;
	}
}

float Rulesets_MaxFPS(void) {

	if (cl_multiview.value && cls.mvdplayback)
		return nNumViews*rulesetDef.maxfps;

	return rulesetDef.maxfps;
}

qbool Rulesets_RestrictTriggers(void) {
	return rulesetDef.restrictTriggers;
}

qbool Rulesets_RestrictPacket(void) {
	return !cl.spectator && !cls.demoplayback && !cl.standby && rulesetDef.restrictPacket;
}

char *Rulesets_Ruleset(void) {
	switch(rulesetDef.ruleset) {
		case rs_smackdown:
			return "smackdown";
		case rs_mtfl:
			return "MTFL";
		default:
			return "default";
	}
}

static void Rulesets_Smackdown(void) {
	extern cvar_t cl_trueLightning;
	extern cvar_t cl_independentPhysics, cl_c2spps;
	extern cvar_t cl_hud;
	extern cvar_t cl_rollalpha;
	extern cvar_t r_shiftbeam;
#ifndef GLQUAKE
	extern cvar_t r_aliasstats;
#endif
#ifdef GLQUAKE
	extern cvar_t amf_camera_death, amf_camera_chase, amf_part_gunshot_type, amf_part_traillen, amf_part_trailtime, amf_part_trailwidth, amf_part_traildetail, amf_part_trailtype, amf_part_sparks, amf_part_spikes, amf_part_gunshot, amf_waterripple, amf_lightning, amf_lightning_size, amf_lightning_size, amf_lightning_sparks;
	extern qbool qmb_initialized;
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
		{&cl_trueLightning, "0"},
		{&cl_hud, "0"},
		{&cl_rollalpha, "20"},
		{&r_shiftbeam, "0"},
#ifndef GLQUAKE
		{&r_aliasstats, "0"}
#endif
	};
	
#ifdef GLQUAKE
		limited_cvar_max_t limited_max_cvars[] = {
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

	if (!qmb_initialized)
		i = NOQMB_SKIP_LOCKED + 1;
	else
#endif
		i = 0;

	for (; i < (sizeof(disabled_cvars) / sizeof(disabled_cvars[0])); i++) {
		Cvar_RulesetSet(disabled_cvars[i].var, disabled_cvars[i].value, 2);
		Cvar_Set(disabled_cvars[i].var, disabled_cvars[i].value);
		Cvar_SetFlags(disabled_cvars[i].var, Cvar_GetFlags(disabled_cvars[i].var) | CVAR_ROM);
	}

#ifdef GLQUAKE
	// there are no limited variables when not using GL or when not using -no24bit cmd-line option
	if (qmb_initialized)
	{
		for (i = 0; i < (sizeof(limited_max_cvars) / sizeof(limited_max_cvars[0])); i++) {
		Cvar_RulesetSet(limited_max_cvars[i].var, limited_max_cvars[i].maxrulesetvalue, 1);
		Cvar_SetFlags(limited_max_cvars[i].var, Cvar_GetFlags(limited_max_cvars[i].var) | CVAR_RULESET_MAX);
		}
	}
#endif

	if (cl_independentPhysics.value)
	{
		Cvar_Set(&cl_c2spps, "0");
		Cvar_SetFlags(&cl_c2spps, Cvar_GetFlags(&cl_c2spps) | CVAR_ROM);
	}

	rulesetDef.maxfps = 77;
	rulesetDef.restrictTriggers = true;
	rulesetDef.restrictPacket = true;
	rulesetDef.ruleset = rs_smackdown;
}
static void Rulesets_MTFL(void) {
/* TODO:
f_flashout trigger
block all other ways to made textures flat(simple)
?disable external textures for detpacks, grenades, sentry, disp, etc?
*/
	extern cvar_t cl_c2spps, r_fullbrightSkins;
#ifdef GLQUAKE
	extern cvar_t amf_camera_chase, amf_waterripple, amf_detpacklights;
	extern cvar_t gl_picmip, gl_max_size, r_drawflat;
	extern cvar_t vid_hwgammacontrol;
#endif

	int i = 0;

	locked_cvar_t disabled_cvars[] = {
#ifdef GLQUAKE
		{&r_drawflat, "0"},
		{&amf_camera_chase, "0"},
		{&amf_waterripple, "0"},
		{&amf_detpacklights, "0"},
		{&vid_hwgammacontrol, "1"}, 
#endif
		{&r_fullbrightSkins, "0"},
		{&cl_c2spps, "0"},
	};

#ifdef GLQUAKE
	limited_cvar_max_t limited_max_cvars[] = {
		{&gl_picmip, "3"},
	};

	limited_cvar_min_t limited_min_cvars[] = {
		{&gl_max_size, "512"},
	};
#endif

	for (; i < (sizeof(disabled_cvars) / sizeof(disabled_cvars[0])); i++) {
		Cvar_RulesetSet(disabled_cvars[i].var, disabled_cvars[i].value, 2);
		Cvar_Set(disabled_cvars[i].var, disabled_cvars[i].value);
		Cvar_SetFlags(disabled_cvars[i].var, Cvar_GetFlags(disabled_cvars[i].var) | CVAR_ROM);
	}

#ifdef GLQUAKE
	for (i = 0; i < (sizeof(limited_max_cvars) / sizeof(limited_max_cvars[0])); i++) {
		Cvar_RulesetSet(limited_max_cvars[i].var, limited_max_cvars[i].maxrulesetvalue, 1);
		Cvar_SetFlags(limited_max_cvars[i].var, Cvar_GetFlags(limited_max_cvars[i].var) | CVAR_RULESET_MAX);
	}

	for (i = 0; i < (sizeof(limited_min_cvars) / sizeof(limited_min_cvars[0])); i++) {
		Cvar_RulesetSet(limited_min_cvars[i].var, limited_min_cvars[i].minrulesetvalue, 0);
		Cvar_SetFlags(limited_min_cvars[i].var, Cvar_GetFlags(limited_min_cvars[i].var) | CVAR_RULESET_MIN);
	}
#endif

	rulesetDef.ruleset = rs_mtfl;
}

static void Rulesets_Default(void) {
	rulesetDef.ruleset = rs_default;
}

void Rulesets_Init(void) {
	int temp;

	if ((temp = COM_CheckParm("-ruleset")) && temp + 1 < com_argc) {
		if (!Q_strcasecmp(com_argv[temp + 1], "smackdown")) {
			Rulesets_Smackdown();
			Com_Printf("Ruleset Smackdown initialized\n");
			return;
		} else if (!Q_strcasecmp(com_argv[temp + 1], "mtfl")) {
			Rulesets_MTFL();
			Com_Printf("Ruleset MTFL initialized\n");
			return;
		} else if (Q_strcasecmp(com_argv[temp + 1], "default")){
			Com_Printf("Unknown ruleset \"%s\"\n", com_argv[temp + 1]);
		}
	}
	Rulesets_Default();
}
