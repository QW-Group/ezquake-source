/*
Copyright (C) 2001-2002 A Nourai
Copyright (C) 2015 ezQuake team

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
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"
#include "mp3_player.h"
#include "input.h"

/* FIXME: Figure out a nicer way to do all this */

static void Rulesets_OnChange_ruleset (cvar_t *var, char *value, qbool *cancel);

static rulesetDef_t rulesetDef = {
	rs_default,
	72.0,
	false,
	false,
	false
};

cvar_t ruleset = {"ruleset", "default", 0, Rulesets_OnChange_ruleset};

qbool RuleSets_DisallowExternalTexture(model_t *mod)
{
	switch (mod->modhint) {
		case MOD_EYES:
			return true;
		case MOD_BACKPACK:
			return rulesetDef.ruleset == rs_smackdown || rulesetDef.ruleset == rs_thunderdome || rulesetDef.ruleset == rs_qcon;
		default:
			return false;
	}
}

qbool RuleSets_DisallowModelOutline(struct model_s *mod)
{
	if (mod == NULL) {
		// World model - only allow in default ruleset, cheats enabled
		return !(r_refdef2.allow_cheats && rulesetDef.ruleset == rs_default);
	}

	switch (mod->modhint) {
		case MOD_EYES:
			return true;
		case MOD_THUNDERBOLT:
			return true;
		case MOD_BACKPACK:
			return rulesetDef.ruleset == rs_qcon || rulesetDef.ruleset == rs_smackdown || rulesetDef.ruleset == rs_thunderdome;
		default:
			return rulesetDef.ruleset == rs_qcon;
	}
}

qbool Rulesets_AllowTimerefresh(void)
{
	switch(rulesetDef.ruleset) {
		case rs_smackdown:
		case rs_thunderdome:
		case rs_qcon:
			return (cl.standby || cl.spectator || cls.demoplayback);
		default:
			return true;
	}
}

qbool Rulesets_AllowNoShadows(void)
{
	switch(rulesetDef.ruleset) {
		case rs_mtfl:
		case rs_smackdown:
		case rs_thunderdome:
		case rs_qcon:
			return false;
		default:
			return true;
	}
}

qbool Rulesets_AllowAlternateModel (const char* modelName)
{
	switch(rulesetDef.ruleset) {
	case rs_qcon:
		if (! strcmp (modelName, "progs/player.mdl"))
			return false;
		return true;
	default:
		return true;
	}
}

float Rulesets_MaxFPS(void)
{
	if (CL_MultiviewEnabled())
		return CL_MultiviewNumberViews()*rulesetDef.maxfps;

	return rulesetDef.maxfps;
}

qbool Rulesets_RestrictTriggers(void)
{
	return rulesetDef.restrictTriggers;
}

qbool Rulesets_RestrictSound(void)
{
	return rulesetDef.restrictSound && cbuf_current != &cbuf_svc;
}

qbool Rulesets_RestrictPacket(void)
{
	return !cl.spectator && !cls.demoplayback && !cl.standby && rulesetDef.restrictPacket;
}

qbool Rulesets_RestrictParticles(void)
{
	return !cl.spectator && !cls.demoplayback && !cl.standby && rulesetDef.restrictParticles && !r_refdef2.allow_cheats;
}

qbool Rulesets_RestrictTCL(void)
{
	switch(rulesetDef.ruleset) {
		case rs_smackdown:
		case rs_thunderdome:
		case rs_qcon:
			return true;
		case rs_mtfl:
		default:
			return false;
	}
}

const char *Rulesets_Ruleset(void)
{
	switch(rulesetDef.ruleset) {
		case rs_mtfl:
			return "MTFL";
		case rs_smackdown:
			return "smackdown";
		case rs_thunderdome:
			return "thunderdome";
		case rs_qcon:
			return "qcon";
		default:
			return "default";
	}
}

static void Rulesets_Smackdown(qbool enable)
{
	extern cvar_t cl_independentPhysics, cl_c2spps;
	extern cvar_t cl_hud;
	extern cvar_t cl_rollalpha;
	extern cvar_t r_shiftbeam;
	extern cvar_t allow_scripts;
	extern cvar_t cl_iDrive;
	int i;

	locked_cvar_t disabled_cvars[] = {
		{&allow_scripts, "0"},  // disable movement scripting
		{&cl_iDrive, "0"},      // disable strafing aid
		{&cl_hud, "0"},         // allows you place any text on the screen & filter incoming messages (hud strings)
		{&cl_rollalpha, "20"},  // allows you to not dodge while seeing enemies dodging
		{&r_shiftbeam, "0"}     // perphaps some people would think this allows you to aim better (maybe should be added for demo playback and spectating only)
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

static void Rulesets_Qcon(qbool enable)
{
	extern cvar_t cl_independentPhysics, cl_c2spps;
	extern cvar_t cl_hud;
	extern cvar_t cl_rollalpha;
	extern cvar_t r_shiftbeam;
	extern cvar_t allow_scripts;
	extern cvar_t cl_iDrive;
	int i;

	locked_cvar_t disabled_cvars[] = {
		{&allow_scripts, "0"},  // disable movement scripting
		{&cl_iDrive, "0"},      // disable strafing aid
		{&cl_hud, "0"},         // allows you place any text on the screen & filter incoming messages (hud strings)
		{&cl_rollalpha, "20"},  // allows you to not dodge while seeing enemies dodging
		{&r_shiftbeam, "0"}     // perphaps some people would think this allows you to aim better (maybe should be added for demo playback and spectating only)
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
		rulesetDef.restrictSound = true;
		rulesetDef.ruleset = rs_qcon;

		MP3_Shutdown ();
	} else {
		for (i = 0; i < (sizeof(disabled_cvars) / sizeof(disabled_cvars[0])); i++)
			Cvar_SetFlags(disabled_cvars[i].var, Cvar_GetFlags(disabled_cvars[i].var) & ~CVAR_ROM);

		if (cl_independentPhysics.value)
			Cvar_SetFlags(&cl_c2spps, Cvar_GetFlags(&cl_c2spps) & ~CVAR_ROM);

		rulesetDef.maxfps = 72.0;
		rulesetDef.restrictTriggers = false;
		rulesetDef.restrictPacket = false;
		rulesetDef.restrictParticles = false;
		rulesetDef.restrictSound = false;
		rulesetDef.ruleset = rs_default;
	}
}
static void Rulesets_Thunderdome(qbool enable)
{
	extern cvar_t cl_independentPhysics, cl_c2spps;
	extern cvar_t cl_hud;
	extern cvar_t r_shiftbeam;
	extern cvar_t allow_scripts;
	extern cvar_t cl_iDrive;
	int i;

	locked_cvar_t disabled_cvars[] = {
		{&allow_scripts, "0"},  // disable movement scripting
		{&cl_iDrive, "0"},      // disable strafing aid
		{&cl_hud, "0"},         // allows you place any text on the screen & filter incoming messages (hud strings)
		{&r_shiftbeam, "0"}     // perphaps some people would think this allows you to aim better (maybe should be added for demo playback and spectating only)
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
		rulesetDef.restrictParticles = false;
		rulesetDef.ruleset = rs_thunderdome;
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
static void Rulesets_MTFL(qbool enable)
{
	/* TODO:
	   f_flashout trigger
	   block all other ways to made textures flat(simple)
	   ?disable external textures for detpacks, grenades, sentry, disp, etc?
	 */
	extern cvar_t cl_c2spps, r_fullbrightSkins;
	extern cvar_t amf_detpacklights;
	extern cvar_t gl_picmip, gl_max_size, r_drawflat;
	extern cvar_t gl_textureless;

	int i = 0;

	locked_cvar_t disabled_cvars[] = {
		{&r_drawflat, "0"},
		{&amf_detpacklights, "0"},
		{&gl_textureless, "0"},
		{&r_fullbrightSkins, "0"},
		{&cl_c2spps, "0"},
	};

	limited_cvar_max_t limited_max_cvars[] = {
		{&gl_picmip, "3"},
	};

	limited_cvar_min_t limited_min_cvars[] = {
		{&gl_max_size, "512"},
	};

	if (enable) {
		for (; i < (sizeof(disabled_cvars) / sizeof(disabled_cvars[0])); i++) {
			Cvar_RulesetSet(disabled_cvars[i].var, disabled_cvars[i].value, 2);
			Cvar_Set(disabled_cvars[i].var, disabled_cvars[i].value);
			Cvar_SetFlags(disabled_cvars[i].var, Cvar_GetFlags(disabled_cvars[i].var) | CVAR_ROM);
		}

		for (i = 0; i < (sizeof(limited_max_cvars) / sizeof(limited_max_cvars[0])); i++) {
			Cvar_RulesetSet(limited_max_cvars[i].var, limited_max_cvars[i].maxrulesetvalue, 1);
			Cvar_SetFlags(limited_max_cvars[i].var, Cvar_GetFlags(limited_max_cvars[i].var) | CVAR_RULESET_MAX);
		}

		for (i = 0; i < (sizeof(limited_min_cvars) / sizeof(limited_min_cvars[0])); i++) {
			Cvar_RulesetSet(limited_min_cvars[i].var, limited_min_cvars[i].minrulesetvalue, 0);
			Cvar_SetFlags(limited_min_cvars[i].var, Cvar_GetFlags(limited_min_cvars[i].var) | CVAR_RULESET_MIN);
		}

		rulesetDef.ruleset = rs_mtfl;
	} else {
		for (i = 0; i < (sizeof(disabled_cvars) / sizeof(disabled_cvars[0])); i++)
			Cvar_SetFlags(disabled_cvars[i].var, Cvar_GetFlags(disabled_cvars[i].var) & ~CVAR_ROM);

		for (i = 0; i < (sizeof(limited_max_cvars) / sizeof(limited_max_cvars[0])); i++)
			Cvar_SetFlags(limited_max_cvars[i].var, Cvar_GetFlags(limited_max_cvars[i].var) & ~CVAR_RULESET_MAX);

		for (i = 0; i < (sizeof(limited_min_cvars) / sizeof(limited_min_cvars[0])); i++)
			Cvar_SetFlags(limited_min_cvars[i].var, Cvar_GetFlags(limited_min_cvars[i].var) & ~CVAR_RULESET_MIN);

		rulesetDef.ruleset = rs_default;
	}
}

static void Rulesets_Default(void)
{
	rulesetDef.ruleset = rs_default;
}

void Rulesets_Init(void)
{
	int temp;

	Cvar_SetCurrentGroup(CVAR_GROUP_USERINFO);
	Cvar_Register(&ruleset);

	if ((temp = COM_CheckParm("-ruleset")) && temp + 1 < COM_Argc()) {
		if (!strcasecmp(COM_Argv(temp + 1), "smackdown")) {
			Cvar_Set(&ruleset, "smackdown");
			return;
		} else if (!strcasecmp(COM_Argv(temp + 1), "thunderdome")) {
			Cvar_Set(&ruleset, "thunderdome");
			return;
		} else if (!strcasecmp(COM_Argv(temp + 1), "mtfl")) {
			Cvar_Set(&ruleset, "mtfl");
			return;
		} else if (strcasecmp(COM_Argv(temp + 1), "qcon")) {
			Cvar_Set(&ruleset, "qcon");
			return;
		} else if (strcasecmp(COM_Argv(temp + 1), "default")){
			Cvar_Set(&ruleset, "default");
			return;
		} else {
			Rulesets_Default();
			return;
		}
	}
}

void Rulesets_OnChange_indphys(cvar_t *var, char *value, qbool *cancel)
{
	if (cls.state != ca_disconnected) {
		Com_Printf("%s can be changed only when disconnected\n", var->name);
		*cancel = true;
	}
	else *cancel = false;
}

void Rulesets_OnChange_r_fullbrightSkins(cvar_t *var, char *value, qbool *cancel)
{
	char *fbs;
	qbool fbskins_policy = (cls.demoplayback || cl.spectator) ? 1 :
		*(fbs = Info_ValueForKey(cl.serverinfo, "fbskins")) ? bound(0, Q_atof(fbs), 1) :
		cl.teamfortress ? 0 : 1;
	float fbskins = bound(0.0, Q_atof (value), fbskins_policy);

	if (!cl.spectator && cls.state != ca_disconnected) {
		if (fbskins > 0.0) {
			Cbuf_AddText(va("say all skins %d%% fullbright\n", (int) (fbskins * 100.0)));
		} else {
			Cbuf_AddText(va("say not using fullbright skins\n"));
		}
	}
}

void Rulesets_OnChange_allow_scripts (cvar_t *var, char *value, qbool *cancel)
{
	char *p;
	qbool progress;
	int val;

	p = Info_ValueForKey(cl.serverinfo, "status");
	progress = (strstr (p, "left")) ? true : false;
	val = Q_atoi(value);

	if (cls.state >= ca_connected && progress && !cl.spectator) {
		Com_Printf ("%s changes are not allowed during the match.\n", var->name);
		*cancel = true;
		return;
	}

	IN_ClearProtectedKeys ();

	if (!cl.spectator && cls.state != ca_disconnected) {
		if (val < 1) {
			Cbuf_AddText("say not using scripts\n");
		} else if (val < 2) {
			Cbuf_AddText("say using simple scripts\n");
		} else {
			Cbuf_AddText("say using advanced scripts\n");
		}
	}
}

void Rulesets_OnChange_cl_delay_packet(cvar_t *var, char *value, qbool *cancel)
{
	int ival = Q_atoi(value);

	if (ival == var->integer) {
		// no change
		return;
	}

	if (var == &cl_delay_packet && (ival < 0 || ival > CL_MAX_PACKET_DELAY * 2)) {
		Com_Printf("%s must be between 0 and %d\n", var->name, CL_MAX_PACKET_DELAY * 2);
		*cancel = true;
		return;
	}

	if (var == &cl_delay_packet_dev && (ival < 0 || ival > CL_MAX_PACKET_DELAY_DEVIATION)) {
		Com_Printf("%s must be between 0 and %d\n", var->name, CL_MAX_PACKET_DELAY_DEVIATION);
		*cancel = true;
		return;
	}

	if (cls.state == ca_active) {
		if ((cl.standby) || (cl.teamfortress)) {
			char announce[128];

			if (var == &cl_delay_packet) {
				snprintf(announce, sizeof(announce), "say delay packet: %d ms (%d dev)\n", ival, cl_delay_packet_dev.integer);
			}
			else {
				snprintf(announce, sizeof(announce), "say delay packet: %d ms (%d dev)\n", cl_delay_packet.integer, ival);
			}

			// allow in standby or teamfortress. For teamfortress, more often than not
			// People 1on1 without "match mode" and they may want to sync pings.
			Cbuf_AddText(announce);
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
	} else {
		// allow in not fully connected state
	}
}

void Rulesets_OnChange_cl_fakeshaft(cvar_t *var, char *value, qbool *cancel)
{
	float fakeshaft = Q_atof(value);


	if (!cl.spectator && cls.state != ca_disconnected) {
		if (fakeshaft == 2)
			Cbuf_AddText("say fakeshaft 2 (emulation of fakeshaft 0 for servers with antilag feature)\n");
		else if (fakeshaft > 0.999)
			Cbuf_AddText("say fakeshaft on\n");
		else if (fakeshaft < 0.001)
			Cbuf_AddText("say fakeshaft off\n");
		else
			Cbuf_AddText(va("say fakeshaft %.1f%%\n", fakeshaft * 100.0));
	}
}

static void Rulesets_OnChange_ruleset(cvar_t *var, char *value, qbool *cancel)
{
	extern void Cmd_ReInitAllMacro(void);

	if (cls.state != ca_disconnected) {
		Com_Printf("%s can be changed only when disconnected\n", var->name);
		*cancel = true;
		return;
	}

	if (strncasecmp(value, "smackdown", sizeof("smackdown")) &&
			strncasecmp(value, "thunderdome", sizeof("thunderdome")) &&
			strncasecmp(value, "mtfl", sizeof("mtfl")) &&
			strncasecmp(value, "qcon", sizeof("qcon")) &&
			strncasecmp(value, "default", sizeof("default"))) {
		Com_Printf_State(PRINT_INFO, "Unknown ruleset \"%s\"\n", value);
		*cancel = true;
		return;
	}

	// All checks passed  so we can remove old ruleset and set a new one
	switch (rulesetDef.ruleset) {
		case rs_mtfl:
			Rulesets_MTFL(false);
			break;
		case rs_smackdown:
			Rulesets_Smackdown(false);
			break;
		case rs_qcon:
			Rulesets_Qcon(false);
			break;
		case rs_thunderdome:
			Rulesets_Thunderdome(false);
			break;
		case rs_default:
			break;
		default:
			break;
	}

	// we need to mark custom textures in the memory (like for backpack and eyes) to be reloaded again
	Cache_Flush();

	if (!strncasecmp(value, "smackdown", sizeof("smackdown"))) {
		Rulesets_Smackdown(true);
		Com_Printf_State(PRINT_OK, "Ruleset Smackdown initialized\n");
	} else if (!strncasecmp(value, "mtfl", sizeof("mtfl"))) {
		Rulesets_MTFL(true);
		Com_Printf_State(PRINT_OK, "Ruleset MTFL initialized\n");
	} else if (!strncasecmp(value, "thunderdome", sizeof("thunderdome"))) {
		Rulesets_Thunderdome(true);
		Com_Printf_State(PRINT_OK, "Ruleset Thunderdome initialized\n");
	} else if (!strncasecmp(value, "qcon", sizeof("qcon"))) {
		Rulesets_Qcon(true);
		Com_Printf_State(PRINT_OK, "Ruleset Qcon initialized\n");
	} else if (!strncasecmp(value, "default", sizeof("default"))) {
		Rulesets_Default();
		Com_Printf_State(PRINT_OK, "Ruleset default initialized\n");
	} else {
		Sys_Error("OnChange_ruleset: WTF?\n");
		// this will never happen
		*cancel = true;
		return;
	}

	Cmd_ReInitAllMacro();
	IN_ClearProtectedKeys();
}

int Rulesets_MaxSequentialWaitCommands(void)
{
	switch (rulesetDef.ruleset) {
	case rs_qcon:
		return 10;
	default:
		return 32768;
	}
}

qbool Ruleset_BlockHudPicChange(void)
{
	switch (rulesetDef.ruleset) {
	case rs_qcon:
		return cls.state != ca_disconnected && !(cl.standby || cl.spectator || cls.demoplayback);
	default:
		return false;
	}
}

qbool Ruleset_AllowPolygonOffset(entity_t* ent)
{
	switch (rulesetDef.ruleset) {
	case rs_qcon:
		return false;
	case rs_default:
		return true;
	default:
		return ent->model && ent->model->isworldmodel;
	}
}
