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

	$Id: rulesets.c,v 1.61 2007-10-14 17:54:12 himan Exp $
*/

#include "quakedef.h"
#ifdef GLQUAKE
#include "gl_model.h"
#include "gl_local.h"
#else
#include "r_model.h"
#include "r_local.h"
#endif


static void Rulesets_OnChange_ruleset (cvar_t *var, char *value, qbool *cancel);


typedef struct locked_cvar_s
{
	cvar_t *var;
	char *value;
} locked_cvar_t;

typedef struct limited_cvar_max_s
{
	cvar_t *var;
	char *maxrulesetvalue;
} limited_cvar_max_t;

typedef struct limited_cvar_min_s
{
	cvar_t *var;
	char *minrulesetvalue;
} limited_cvar_min_t;

typedef enum {rs_default, rs_smackdown, rs_mtfl} ruleset_t;

typedef struct rulesetDef_s
{
	ruleset_t ruleset;
	float maxfps;
	qbool restrictTriggers;
	qbool restrictPacket;
	qbool restrictParticles;
} rulesetDef_t;

static rulesetDef_t rulesetDef = {rs_default, 72.0, false, false, false};
cvar_t ruleset = {"ruleset", "default", 0, Rulesets_OnChange_ruleset};

qbool RuleSets_DisallowExternalTexture (model_t *mod)
{
	switch (mod->modhint) {
	case MOD_EYES:
		return true;
	case MOD_BACKPACK:
		return rulesetDef.ruleset == rs_smackdown;
	default:
		return false;
	}
}

qbool Rulesets_AllowTimerefresh (void)
{
	switch(rulesetDef.ruleset) {
	case rs_smackdown:
		//return cl.standby;
		return (cl.standby || cl.spectator || cls.demoplayback);
	default:
		return true;
	}
}

qbool Rulesets_AllowNoShadows (void)
{
	switch(rulesetDef.ruleset) {
	case rs_mtfl:
		return false;
	case rs_smackdown:
		return false;
	default:
		return true;
	}
}

float Rulesets_MaxFPS (void)
{
	if (cl_multiview.value && cls.mvdplayback)
		return nNumViews*rulesetDef.maxfps;

	return rulesetDef.maxfps;
}

qbool Rulesets_RestrictTriggers (void)
{
	return rulesetDef.restrictTriggers;
}

qbool Rulesets_RestrictPacket (void)
{
	return !cl.spectator && !cls.demoplayback && !cl.standby && rulesetDef.restrictPacket;
}

qbool Rulesets_RestrictParticles (void)
{
	return !cl.spectator && !cls.demoplayback && !cl.standby && rulesetDef.restrictParticles && !r_refdef2.allow_cheats;
}

qbool Rulesets_RestrictTCL(void)
{
	switch(rulesetDef.ruleset) {
	case rs_mtfl:
		return false;
	case rs_smackdown:
		return true;
	default:
		return false;
	}
}

char *Rulesets_Ruleset (void)
{
	switch(rulesetDef.ruleset) {
		case rs_smackdown:
			return "smackdown";
		case rs_mtfl:
			return "MTFL";
		default:
			return "default";
	}
}

static void Rulesets_Smackdown (qbool enable)
{
	extern cvar_t cl_independentPhysics, cl_c2spps;
	extern cvar_t cl_hud;
	extern cvar_t cl_rollalpha;
	extern cvar_t r_shiftbeam;
	extern cvar_t allow_scripts;
#ifndef GLQUAKE
	extern cvar_t r_aliasstats;
#endif
	int i;

	locked_cvar_t disabled_cvars[] = {
		{&allow_scripts, "0"},      // disable movement scripting
		{&cl_hud, "0"},				// allows you place any text on the screen & filter incoming messages (hud strings)
		{&cl_rollalpha, "20"},		// allows you to not dodge while seeing enemies dodging
		{&r_shiftbeam, "0"},		// perphaps some people would think this allows you to aim better (maybe should be added for demo playback and spectating only)
#ifndef GLQUAKE
		{&r_aliasstats, "0"}
#endif
	};
	
	if (enable) {
		for (i = 0; i < (sizeof(disabled_cvars) / sizeof(disabled_cvars[0])); i++) {
			Cvar_RulesetSet(disabled_cvars[i].var, disabled_cvars[i].value, 2);
			Cvar_Set(disabled_cvars[i].var, disabled_cvars[i].value);
			Cvar_SetFlags(disabled_cvars[i].var, Cvar_GetFlags(disabled_cvars[i].var) | CVAR_ROM);
		}
	
		if (cl_independentPhysics.value) {
			Cvar_Set(&cl_c2spps, "0"); // people were complaining that player move is jerky with this. however this has not much to do with independent physics, but people are too paranoid about it
			Cvar_SetFlags(&cl_c2spps, Cvar_GetFlags(&cl_c2spps) | CVAR_ROM);
		}
	
		rulesetDef.maxfps = 77;
		rulesetDef.restrictTriggers = true;
		rulesetDef.restrictPacket = true; // packet command could have been exploited for external timers
		rulesetDef.restrictParticles = true;
		rulesetDef.ruleset = rs_smackdown;
	} else {
		for (i = 0; i < (sizeof(disabled_cvars) / sizeof(disabled_cvars[0])); i++)
			Cvar_SetFlags(disabled_cvars[i].var, Cvar_GetFlags(disabled_cvars[i].var) & ~CVAR_ROM);
		
		if (cl_independentPhysics.value)
			Cvar_SetFlags(&cl_c2spps, Cvar_GetFlags(&cl_c2spps) & ~CVAR_ROM);

		rulesetDef.maxfps = 72.0;
		rulesetDef.restrictTriggers = false;
		rulesetDef.restrictPacket = false;
		rulesetDef.restrictParticles = false;
		rulesetDef.ruleset = rs_default;
	}
}

static void Rulesets_MTFL (qbool enable)
{
/* TODO:
f_flashout trigger
block all other ways to made textures flat(simple)
?disable external textures for detpacks, grenades, sentry, disp, etc?
*/
	extern cvar_t cl_c2spps, r_fullbrightSkins;
#ifdef GLQUAKE
	extern cvar_t amf_detpacklights;
	extern cvar_t gl_picmip, gl_max_size, r_drawflat;
	extern cvar_t vid_hwgammacontrol;
	extern cvar_t gl_textureless;
#endif

	int i = 0;

	locked_cvar_t disabled_cvars[] = {
#ifdef GLQUAKE
		{&r_drawflat, "0"},
		{&amf_detpacklights, "0"},
		{&vid_hwgammacontrol, "2"},
		{&gl_textureless, "0"},
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

	if (enable) {
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
	} else {
		for (i = 0; i < (sizeof(disabled_cvars) / sizeof(disabled_cvars[0])); i++)
			Cvar_SetFlags(disabled_cvars[i].var, Cvar_GetFlags(disabled_cvars[i].var) & ~CVAR_ROM);
#ifdef GLQUAKE
		for (i = 0; i < (sizeof(limited_max_cvars) / sizeof(limited_max_cvars[0])); i++)
			Cvar_SetFlags(limited_max_cvars[i].var, Cvar_GetFlags(limited_max_cvars[i].var) & ~CVAR_RULESET_MAX);

		for (i = 0; i < (sizeof(limited_min_cvars) / sizeof(limited_min_cvars[0])); i++)
			Cvar_SetFlags(limited_min_cvars[i].var, Cvar_GetFlags(limited_min_cvars[i].var) & ~CVAR_RULESET_MIN);
#endif
		rulesetDef.ruleset = rs_default;
	}
}

static void Rulesets_Default (void)
{
	rulesetDef.ruleset = rs_default;
}

void Rulesets_Init (void)
{
	int temp;

	Cvar_SetCurrentGroup(CVAR_GROUP_USERINFO);
	Cvar_Register (&ruleset);

	if ((temp = COM_CheckParm ("-ruleset")) && temp + 1 < COM_Argc()) {
		if (!strcasecmp (COM_Argv(temp + 1), "smackdown")) {
			Cvar_Set (&ruleset, "smackdown");
			return;
		} else if (!strcasecmp (COM_Argv(temp + 1), "mtfl")) {
			Cvar_Set (&ruleset, "mtfl");
			return;
		} else if (strcasecmp (COM_Argv(temp + 1), "default")){
			Cvar_Set (&ruleset, "default");
			return;
		} else {
			Rulesets_Default ();
			return;
		}
	}
}

void Rulesets_OnChange_indphys (cvar_t *var, char *value, qbool *cancel)
{
	if (cls.state != ca_disconnected) {
		Com_Printf ("%s can be changed only when disconnected\n", var->name);
		*cancel = true;
	}
	else *cancel = false;
}

void Rulesets_OnChange_r_fullbrightSkins (cvar_t *var, char *value, qbool *cancel)
{
	char *fbs;
	qbool fbskins_policy = (cls.demoplayback || cl.spectator) ? 1 :
		*(fbs = Info_ValueForKey(cl.serverinfo, "fbskins")) ? bound(0, Q_atof(fbs), 1) :
		cl.teamfortress ? 0 : 1;
	float fbskins = bound (0.0, Q_atof (value), fbskins_policy);

	if (!cl.spectator && cls.state != ca_disconnected) {
		if (fbskins > 0.0)
			Cbuf_AddText (va("say all skins %d%% fullbright\n", (int) (fbskins * 100.0)));
		else
			Cbuf_AddText (va("say not using fullbright skins\n"));
	}
}

void Rulesets_OnChange_allow_scripts (cvar_t *var, char *value, qbool *cancel)
{
	char *p;
	qbool progress;
	int val;

	p = Info_ValueForKey (cl.serverinfo, "status");
	progress = (strstr (p, "left")) ? true : false;
	val = Q_atoi (value);;

	if (cls.state >= ca_connected && progress && !cl.spectator) {
		Com_Printf ("%s changes are not allowed during the match.\n", var->name);
		*cancel = true;
		return;
	}

	if (!cl.spectator && cls.state != ca_disconnected) {
		if (val < 1)
			Cbuf_AddText ("say not using scripts\n");
		else if (val < 2)
			Cbuf_AddText ("say using simple scripts\n");
		else
			Cbuf_AddText ("say using advanced scripts\n");
	}
}

void Rulesets_OnChange_cl_delay_packet(cvar_t *var, char *value, qbool *cancel)
{
	int ival = Q_atoi(value);	// this is used in the code
	float fval = Q_atof(value); // this is used to check value validity

	if (ival == var->integer && fval == var->value) {
		// no change
		return;
	}

	if (fval < 0) {
		Com_Printf("%s doesn't allow negative values\n", var->name);
		*cancel = true;
		return;
	}

	if (cls.state == ca_active) {
		if (cl.standby) {
			// allow in standby
			Cbuf_AddText(va("say delay packet: %d ms\n", ival));
		}
		else {
			// disallow during the match
			Com_Printf("%s changes are not allowed during the match\n", var->name);
			*cancel = true;
		}
	}
	else {
		// allow in not fully connected state
	}
}

void Rulesets_OnChange_cl_iDrive(cvar_t *var, char *value, qbool *cancel)
{
	int ival = Q_atoi(value);	// this is used in the code
	float fval = Q_atof(value); // this is used to check value validity

	if (ival == var->integer && fval == var->value) {
		// no change
		return;
	}

	if (fval != 0 && fval != 1) {
		Com_Printf("Invalid value for %s, use 0 or 1.\n", var->name);
		*cancel = true;
		return;
	}

	if (cls.state == ca_active) {
		if (cl.standby) {
			// allow in standby
			Cbuf_AddText(va("say side step aid (strafescript): %s\n", ival ? "on" : "off"));
		}
		else {
			// disallow during the match
			Com_Printf("%s changes are not allowed during the match\n", var->name);
			*cancel = true;
		}
	}
	else {
		// allow in not fully connected state
	}
}

void Rulesets_OnChange_cl_fakeshaft (cvar_t *var, char *value, qbool *cancel)
{
	float fakeshaft = Q_atof (value);

	if (!cl.spectator && cls.state != ca_disconnected) {
		if (fakeshaft > 0.999)
			Cbuf_AddText ("say fakeshaft on\n");
		else if (fakeshaft < 0.001)
			Cbuf_AddText ("say fakeshaft off\n");
		else
			Cbuf_AddText (va("say fakeshaft %.1f%%\n", fakeshaft * 100.0));
	}
}

static void Rulesets_OnChange_ruleset (cvar_t *var, char *value, qbool *cancel)
{
	extern void Cmd_ReInitAllMacro (void);

	if (cls.state != ca_disconnected) {
		Com_Printf ("%s can be changed only when disconnected\n", var->name);
		*cancel = true;
		return;
	}

	if (strncasecmp (value, "smackdown", 9) && strncasecmp (value, "mtfl", 4) && strncasecmp (value, "default", 7)) {
		Com_Printf_State (PRINT_INFO, "Unknown ruleset \"%s\"\n", value);
		*cancel = true;
		return;
	}

	// All checks passed  so we can remove old ruleset and set a new one
	switch (rulesetDef.ruleset) {
		case rs_smackdown:
			Rulesets_Smackdown (false);
			break;
		case rs_mtfl:
			Rulesets_MTFL (false);
			break;
		case rs_default:
			break;
		default:
			break;
	}

	// we need to mark custom textures in the memory (like for backpack and eyes) to be reloaded again
	Cache_Flush ();

	if (!strncasecmp (value, "smackdown", 9)) {
		Rulesets_Smackdown (true);
		Com_Printf_State (PRINT_OK, "Ruleset Smackdown initialized\n");
	} else if (!strncasecmp (value, "mtfl", 4)) {
		Rulesets_MTFL (true);
		Com_Printf_State (PRINT_OK, "Ruleset MTFL initialized\n");
	} else if (!strncasecmp (value, "default", 7)) {
		Rulesets_Default ();
		Com_Printf_State (PRINT_OK, "Ruleset default initialized\n");
	} else {
		Sys_Error ("OnChange_ruleset: WTF?\n");
		// this will never happen
		*cancel = true;
		return;
	}

	Cmd_ReInitAllMacro ();
}
