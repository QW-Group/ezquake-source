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
#include "fmod.h"
#include "utils.h"


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
extern cvar_t cl_iDrive;


static void FChecks_VersionResponse (void)
{
	Cbuf_AddText (va("say ezQuake %s " QW_PLATFORM ":" QW_RENDERER "\n", VersionString()));
}

static char *FChecks_FServerResponse_Text(void)
{
	netadr_t adr;

	if (!NET_StringToAdr (cls.servername, &adr))
		return NULL;

	if (adr.port == 0)
		adr.port = BigShort (PORT_SERVER);

	return NET_AdrToString(adr);
}

static void FChecks_FServerResponse (void)
{
	char *text = FChecks_FServerResponse_Text();

	if (!text)
		return;

	Cbuf_AddText(va("say ezQuake f_server response: %s\n", text));
}

static void FChecks_SkinsResponse (float fbskins)
{
	char *sf = enemyforceskins.integer ? ", individual enemy skin forcing" : "";

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
	if (cl_fakeshaft.integer == 2)
		Cbuf_AddText("say fakeshaft 2 (emulation of fakeshaft 0 for servers with antilag feature)\n");
	else if (cl_fakeshaft.value > 0.999)
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

void FChecks_RulesetFeatureAppend(qbool on, const char *code, char *feat_on_buf,
								  size_t feat_on_size, char *feat_off_buf, size_t feat_off_size)
{
	if (on) {
		strlcat(feat_on_buf, code, feat_on_size);
	}
	else {
		strlcat(feat_off_buf, code, feat_off_size);
	}
}

// constructs a string telling which extra features are enabled
// besides those covered by the current ruleset)
// format: [+<enabled features>][-<disabled features>]
const char* FChecks_RulesetAdditionString(void)
{
	char feat_on_buf[16] = "+";
	char feat_off_buf[16] = "-";
	static char features[32] = "";

	*features = '\0';

	#define APPENDFEATURE(on,code) FChecks_RulesetFeatureAppend((on),(code),feat_on_buf,sizeof (feat_on_buf),feat_off_buf,sizeof (feat_off_buf))
	// modified models or sounds
	APPENDFEATURE((strcmp(FMod_Response_Text(), "all models ok") != 0),"m");

	// movement scripts enabled
	APPENDFEATURE((allow_scripts.integer),"s");

	// enemy skin forcing enabled
	APPENDFEATURE((enemyforceskins.integer),"f");

	// cl_iDrive - strafing aid
	APPENDFEATURE((cl_iDrive.integer),"i");
	#undef APPENDFEATURE

	if (strlen(feat_on_buf) > 1) {
		strlcat(features, feat_on_buf, sizeof (features));
	}
	if (strlen(feat_off_buf) > 1) {
		strlcat(features, feat_off_buf, sizeof (features));
	}

	return features;
}

static qbool FChecks_CheckFRulesetRequest (const char *s)
{
	// format of the reply:
	// [nick: ]
	// [padding] - so that "nick: " + padding is 17 chars long
	// [ip address] - padded with spaces to 21 chars
	// - this fits to 38 chars which is length of line with conwidth 320
	// [space]
	// [client version] - padded to 16 chars
	// [ruleset name]
	// [ruleset addition]
	// - these 4 should be less than 38 chars so that reply does never take up more than 2 lines
	char *fServer;
	const char *features;
	char *emptystring = "";
	char *brief_version = "ezq" VERSION_NUMBER;
	const char *ruleset = Rulesets_Ruleset();
	size_t name_len = strlen(cl.players[cl.playernum].name);
	size_t pad_len = 15 - min(name_len, 15);

	if (cl.spectator || (f_ruleset_reply_time && cls.realtime - f_ruleset_reply_time < 20))
		return false;

	if (Util_F_Match(s, "f_ruleset"))	{
		features = FChecks_RulesetAdditionString();
		fServer = FChecks_FServerResponse_Text();
		if (!fServer) {
			fServer = "server-na";
		}

		Cbuf_AddText(va("say \"%*s%21s %16s %s%s\"\n",
			pad_len, emptystring, fServer, brief_version, ruleset, features));
		f_ruleset_reply_time = cls.realtime;
		return true;
	}

	return false;
}

void FChecks_FRuleset_cmd(void)
{
	if (Cmd_Argc() < 2 || strcmp(Cmd_Argv(1), "check") != 0) {
		Com_Printf("Purpose:\n  "
			"All clients on the server should respond to \"f_ruleset\" message with their client settings in the reply\n"
			"Usage:\n  f_ruleset - prints this help\n"
			"  f_ruleset check - sends check message to all players on the server\n"
			"Flags:\n  End of the reply is ruleset name + flags denoting enabled and disabled features:\n"
			"  m - modified models or sounds\n"
			"  s - movement scripts\n"
			"  f - individual enemy skin forcing\n"
			"  i - side step aid (strafescript)\n"
			"  flags that start with + mean feature is enabled, - means disabled\n");
	}
	else {
		Cbuf_AddText("say \"f_ruleset\"\n");
	}		
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
	Cmd_AddCommand ("f_ruleset", FChecks_FRuleset_cmd);
}
