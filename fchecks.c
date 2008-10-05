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

	$Id: fchecks.c,v 1.19 2007-09-14 13:29:28 disconn3ct Exp $
*/

#include "quakedef.h"
#include "rulesets.h"
#include "version.h"
#include "auth.h"
#include "fmod.h"
#include "utils.h"
#include "modules.h"


static float
f_system_reply_time,
f_cmdline_reply_time,
f_scripts_reply_time,
f_fshaft_reply_time,
f_ruleset_reply_time,
f_reply_time,
f_mod_reply_time,
f_version_reply_time,
f_skins_reply_time,
f_server_reply_time;

cvar_t allow_f_system  = {"allow_f_system",  "1"};
cvar_t allow_f_cmdline = {"allow_f_cmdline", "1"};

extern cvar_t enemyforceskins;
extern cvar_t cl_independentPhysics;
extern cvar_t allow_scripts;
extern cvar_t cl_delay_packet;
extern cvar_t r_fullbrightSkins;
extern cvar_t cl_fakeshaft;


static void FChecks_VersionResponse (void)
{
	if (Modules_SecurityLoaded())
		Cbuf_AddText (va("say ezQuake %s " QW_PLATFORM ":" QW_RENDERER "  crc: %s\n", VersionString(), Auth_Generate_Crc()));
	else
		Cbuf_AddText (va("say ezQuake %s " QW_PLATFORM ":" QW_RENDERER "\n", VersionString()));
}

static void FChecks_FServerResponse (void)
{
	netadr_t adr;

	if (!NET_StringToAdr (cls.servername, &adr))
		return;

	if (adr.port == 0)
		adr.port = BigShort (PORT_SERVER);

	if (Modules_SecurityLoaded())
		Cbuf_AddText(va("say ezQuake f_server response: %s  crc: %s\n", NET_AdrToString(adr), Auth_Generate_Crc()));
	else
		Cbuf_AddText(va("say ezQuake f_server response: %s\n", NET_AdrToString(adr)));
}

static void FChecks_SkinsResponse (float fbskins)
{
	char *sf = enemyforceskins.integer ? ", enemy skins forcing" : ""; 

	if (fbskins > 0)
		Cbuf_AddText (va("say all skins %d%% fullbright%s\n", (int) (fbskins * 100), sf));
	else
		Cbuf_AddText (va("say not using fullbright skins%s\n", sf));
}

static void FChecks_ScriptsResponse (void)
{
	if (allow_scripts.integer < 1)
		Cbuf_AddText ("say not using scripts\n");
	else if (allow_scripts.integer < 2)
		Cbuf_AddText ("say using simple scripts\n");
	else
		Cbuf_AddText ("say using advanced scripts\n");
}

static qbool FChecks_ScriptsRequest (const char *s)
{
	if (cl.spectator || (f_scripts_reply_time && cls.realtime - f_scripts_reply_time < 20))
		return false;

	if (Util_F_Match(s, "f_scripts")) {
		FChecks_ScriptsResponse();
		f_scripts_reply_time = cls.realtime;
		return true;
	}

	return false;
}

static void FChecks_FakeshaftResponse (void)
{
	if (cl_fakeshaft.value > 0.999)
		Cbuf_AddText("say fakeshaft on\n");
	else if (cl_fakeshaft.value < 0.001)
		Cbuf_AddText("say fakeshaft off\n");
	else
		Cbuf_AddText(va("say fakeshaft %.1f%%\n", cl_fakeshaft.value * 100.0));
}

static qbool FChecks_FakeshaftRequest (const char *s)
{
	if (cl.spectator || (f_fshaft_reply_time && cls.realtime - f_fshaft_reply_time < 20))
		return false;

	if (Util_F_Match(s, "f_fakeshaft"))	{
		FChecks_FakeshaftResponse();
		f_fshaft_reply_time = cls.realtime;
		return true;
	}

	return false;
}

static qbool FChecks_VersionRequest (const char *s)
{
	if (cl.spectator || (f_version_reply_time && cls.realtime - f_version_reply_time < 20))
		return false;

	if (Util_F_Match(s, "f_version")) {
		FChecks_VersionResponse();
		f_version_reply_time = cls.realtime;
		return true;
	}

	return false;
}

static qbool FChecks_SkinRequest (const char *s)
{
	char *fbs;
	qbool fbskins_policy = (cls.demoplayback || cl.spectator) ? 1 :
		*(fbs = Info_ValueForKey(cl.serverinfo, "fbskins")) ? bound(0, Q_atof(fbs), 1) :
		cl.teamfortress ? 0 : 1;
	float fbskins = bound (0, r_fullbrightSkins.value, fbskins_policy);
	if (cl.spectator || (f_skins_reply_time && cls.realtime - f_skins_reply_time < 20))
		return false;

	if (Util_F_Match(s, "f_skins"))	{
		FChecks_SkinsResponse(fbskins);
		f_skins_reply_time = cls.realtime;
		return true;
	}

	return false;
}

static qbool FChecks_CheckFModRequest (const char *s)
{
	if (cl.spectator || (f_mod_reply_time && cls.realtime - f_mod_reply_time < 20))
		return false;

	if (Util_F_Match(s, "f_modified")) {
		FMod_Response();
		f_mod_reply_time = cls.realtime;
		return true;
	}

	return false;
}

static qbool FChecks_CheckFServerRequest (const char *s)
{
	netadr_t adr;

	if (cl.spectator || (f_server_reply_time && cls.realtime - f_server_reply_time < 20))
		return false;

	if (Util_F_Match(s, "f_server") && NET_StringToAdr (cls.servername, &adr)) {
		FChecks_FServerResponse();
		f_server_reply_time = cls.realtime;
		return true;
	}

	return false;
}

static qbool FChecks_CheckFRulesetRequest (const char *s)
{
	char *sScripts, *sIPhysics, *sDelayP;

	if (cl.spectator || (f_ruleset_reply_time && cls.realtime - f_ruleset_reply_time < 20))
		return false;

	if (Util_F_Match(s, "f_ruleset"))	{
		sScripts = (allow_scripts.integer) ? "" : " \x90rj scripts blocked\x91";
		sIPhysics = (cl_independentPhysics.integer) ? "" : " \x90indep. physics off\x91";
		sDelayP = (cl_delay_packet.integer) ? (" \x90" "packet delay\x91") : "";

		Cbuf_AddText(va("say ezQuake Ruleset: %s%s%s%s\n", Rulesets_Ruleset(), sScripts, sIPhysics, sDelayP));
		f_ruleset_reply_time = cls.realtime;
		return true;
	}

	return false;
}

static qbool FChecks_CmdlineRequest (const char *s)
{
	if (cl.spectator || (f_cmdline_reply_time && cls.realtime - f_cmdline_reply_time < 20))
		return false;

	if (Util_F_Match(s, "f_cmdline"))	{
		if (!allow_f_cmdline.integer) {
			Cbuf_AddText("say disabled\n");
		} else {
			Cbuf_AddText("say ");
			Cbuf_AddText(com_args_original);
			Cbuf_AddText("\n");
		}

		f_cmdline_reply_time = cls.realtime;
		return true;
	}

	return false;
}

static qbool FChecks_SystemRequest (const char *s)
{
	if (cl.spectator || (f_system_reply_time && cls.realtime - f_system_reply_time < 20))
		return false;

	if (Util_F_Match(s, "f_system")) {
		char *sys_string;

		sys_string = (allow_f_system.integer) ? SYSINFO_GetString() : "disabled";

		//if (sys_string != NULL && sys_string[0]) {
		Cbuf_AddText("say ");
		Cbuf_AddText(sys_string);
		Cbuf_AddText("\n");
		//}

		f_system_reply_time = cls.realtime;
		return true;
	}

	return false;
}

void FChecks_CheckRequest (const char *s)
{
	qbool fcheck = false;

	fcheck |= FChecks_VersionRequest (s);
	fcheck |= FChecks_CmdlineRequest (s);
	fcheck |= FChecks_SystemRequest (s);
	fcheck |= FChecks_SkinRequest (s);
	fcheck |= FChecks_ScriptsRequest (s);
	fcheck |= FChecks_FakeshaftRequest (s);
	fcheck |= FChecks_CheckFModRequest (s);
	fcheck |= FChecks_CheckFServerRequest (s);
	fcheck |= FChecks_CheckFRulesetRequest (s);

	if (fcheck)
		f_reply_time = cls.realtime;
}

void FChecks_Init (void)
{
	Cvar_SetCurrentGroup (CVAR_GROUP_CHAT);
	Cvar_Register (&allow_f_system);
	Cvar_Register (&allow_f_cmdline);
	Cvar_ResetCurrentGroup();

	FMod_Init();
	Cmd_AddCommand ("f_server", FChecks_FServerResponse);
}
