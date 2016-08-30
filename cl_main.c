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
// cl_main.c  -- client main loop

#include "quakedef.h"
#include "cdaudio.h"
#include "cl_slist.h"
#include "movie.h"
#include "logging.h"
#include "ignore.h"
#include "fchecks.h"
#include "config_manager.h"
#include "mp3_player.h"
#include "mvd_utils.h"
#include "EX_browser.h"
#include "EX_qtvlist.h"
#include "qtv.h"
#include "keys.h"
#include "hud.h"
#include "hud_common.h"
#include "hud_editor.h"
#include "input.h"
#include "gl_model.h"
#include "gl_local.h"
#include "tr_types.h"
#include "teamplay.h"
#include "tp_triggers.h"
#include "rulesets.h"
#include "version.h"
#include "stats_grid.h"
#include "fmod.h"
#include "modules.h"
#include "sbar.h"
#include "utils.h"
#include "qsound.h"
#include "menu.h"
#include "image.h"
#ifndef _WIN32
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#ifndef CLIENTONLY
#include "server.h"
#endif
#include "fs.h"
#include "help.h"
#include "irc.h"
#ifdef _DEBUG
#include "parser.h"
#endif
extern qbool ActiveApp, Minimized;

static void Cl_Reset_Min_fps_f(void);

cvar_t	allow_scripts = {"allow_scripts", "2", 0, Rulesets_OnChange_allow_scripts};
cvar_t	rcon_password = {"rcon_password", ""};
cvar_t	rcon_address = {"rcon_address", ""};
cvar_t	cl_crypt_rcon = {"cl_crypt_rcon", "1"};

cvar_t	cl_timeout = {"cl_timeout", "60"};

cvar_t	cl_delay_packet = {"cl_delay_packet", "0", 0, Rulesets_OnChange_cl_delay_packet};

cvar_t	cl_shownet = {"cl_shownet", "0"};	// can be 0, 1, or 2
#if defined(PROTOCOL_VERSION_FTE) || defined(PROTOCOL_VERSION_FTE2)
cvar_t  cl_pext = {"cl_pext", "1"};					// allow/disallow protocol extensions at all.
													// some extensions can be explicitly controlled.
cvar_t  cl_pext_other = {"cl_pext_other", "0"};		// extensions which does not have own variables should be controlled by this variable.
#endif
#ifdef FTE_PEXT_256PACKETENTITIES
cvar_t	cl_pext_256packetentities = {"cl_pext_256packetentities", "1"};
#endif
#ifdef FTE_PEXT_CHUNKEDDOWNLOADS
cvar_t  cl_pext_chunkeddownloads  = {"cl_pext_chunkeddownloads", "1"};
cvar_t  cl_chunksperframe  = {"cl_chunksperframe", "5"};
#endif

#ifdef FTE_PEXT_FLOATCOORDS
cvar_t  cl_pext_floatcoords  = {"cl_pext_floatcoords", "1"};
#endif

#ifdef FTE_PEXT_TRANS
cvar_t cl_pext_alpha = {"cl_pext_alpha", "1"};
#endif

cvar_t	cl_sbar		= {"cl_sbar", "0"};
cvar_t	cl_hudswap	= {"cl_hudswap", "0"};
cvar_t	cl_maxfps	= {"cl_maxfps", "0"};
cvar_t	cl_physfps	= {"cl_physfps", "0"};	//#fps
cvar_t	cl_physfps_spectator = {"cl_physfps_spectator", "30"};
cvar_t  cl_independentPhysics = {"cl_independentPhysics", "1", 0, Rulesets_OnChange_indphys};

cvar_t	cl_predict_players = {"cl_predict_players", "1"};
cvar_t	cl_solid_players = {"cl_solid_players", "1"};
cvar_t	cl_predict_half = {"cl_predict_half", "0"};

cvar_t  show_fps2 = {"scr_scoreboard_drawfps","0"};
cvar_t	hud_fps_min_reset_interval = {"hud_fps_min_reset_interval", "30"};

cvar_t  localid = {"localid", ""};

static qbool allowremotecmd = true;

cvar_t	cl_deadbodyfilter = {"cl_deadbodyFilter", "0"};
cvar_t	cl_gibfilter = {"cl_gibFilter", "0"};
cvar_t	cl_backpackfilter = {"cl_backpackfilter", "0"};
cvar_t	cl_muzzleflash = {"cl_muzzleflash", "1"};
cvar_t	cl_rocket2grenade = {"cl_r2g", "0"};
cvar_t	cl_demospeed = {"cl_demospeed", "1"};
cvar_t	cl_staticsounds = {"cl_staticSounds", "1"};
cvar_t	cl_fakeshaft = {"cl_fakeshaft", "0", 0, Rulesets_OnChange_cl_fakeshaft};
cvar_t	cl_fakeshaft_extra_updates = {"cl_fakeshaft_extra_updates", "1"};
cvar_t	cl_parseWhiteText = {"cl_parseWhiteText", "1"};
cvar_t	cl_filterdrawviewmodel = {"cl_filterdrawviewmodel", "0"};
cvar_t	cl_demoPingInterval = {"cl_demoPingInterval", "5"};
cvar_t  demo_getpings      = {"demo_getpings",    "1"};
cvar_t	cl_chatsound = {"s_chat_custom", "1"};
cvar_t	cl_confirmquit = {"cl_confirmquit", "0"}; // , CVAR_INIT
cvar_t	cl_fakename = {"cl_fakename", ""};
cvar_t	cl_fakename_suffix = {"cl_fakename_suffix", ": "};
cvar_t	qizmo_dir = {"qizmo_dir", "qizmo"};
cvar_t	qwdtools_dir = {"qwdtools_dir", "qwdtools"};
void OnChangeColorForcing (cvar_t *var, char *value, qbool *cancel);
void OnChangeDemoTeamplay (cvar_t *var, char *value, qbool *cancel);
cvar_t  cl_demoteamplay = {"cl_demoteamplay", "0", 0, OnChangeDemoTeamplay};	// for NQ demos where we need to say it is teamplay rather than FFA

cvar_t	cl_earlypackets = {"cl_earlypackets", "1"};

cvar_t	cl_restrictions = {"cl_restrictions", "0"}; // 1 is FuhQuake and QW262 defaults

cvar_t cl_floodprot			= {"cl_floodprot", "0"};
cvar_t cl_fp_messages		= {"cl_fp_messages", "4"};
cvar_t cl_fp_persecond		= {"cl_fp_persecond", "4"};
cvar_t cl_cmdline			= {"cl_cmdline", "", CVAR_ROM};
cvar_t cl_useproxy			= {"cl_useproxy", "0"};
cvar_t cl_proxyaddr         = {"cl_proxyaddr", ""};
cvar_t cl_window_caption	= {"cl_window_caption", "1"};

cvar_t cl_model_bobbing		= {"cl_model_bobbing", "1"};
cvar_t cl_nolerp			= {"cl_nolerp", "0"}; // 0 is good for indep-phys, 1 is good for old-phys

//this var has effect only if cl_nolerp is 1 and indep-phys enabled
//setting it to 0 removes jerking when standing on platforms
cvar_t cl_nolerp_on_entity	= {"cl_nolerp_on_entity", "0"};

cvar_t cl_newlerp				= {"cl_newlerp", "0"};
cvar_t cl_lerp_monsters			= {"cl_lerp_monsters", "1"};
cvar_t cl_fix_mvd				= {"cl_fix_mvd", "0"};

cvar_t r_rocketlight			= {"r_rocketLight", "1"};
cvar_t r_rocketlightcolor		= {"r_rocketLightColor", "0"};
cvar_t r_explosionlightcolor	= {"r_explosionLightColor", "0"};
cvar_t r_explosionlight			= {"r_explosionLight", "1"};
cvar_t r_flagcolor				= {"r_flagColor", "0"};
cvar_t r_lightflicker			= {"r_lightflicker", "1"};
cvar_t r_powerupglow			= {"r_powerupGlow", "1"};
cvar_t cl_novweps				= {"cl_novweps", "0"};
cvar_t r_drawvweps				= {"r_drawvweps", "1"};
cvar_t r_rockettrail			= {"r_rocketTrail", "1"}; // 9
cvar_t r_grenadetrail			= {"r_grenadeTrail", "1"}; // 3
cvar_t r_railtrail				= {"r_railTrail", "1"};
cvar_t r_instagibtrail				= {"r_instagibTrail", "1"};
cvar_t r_explosiontype			= {"r_explosionType", "1"}; // 7
cvar_t r_telesplash				= {"r_telesplash", "1"}; // disconnect
cvar_t r_shaftalpha				= {"r_shaftalpha", "1"};

// info mirrors
cvar_t  password                = {"password", "", CVAR_USERINFO};
cvar_t  spectator               = {"spectator", "", CVAR_USERINFO};
void CL_OnChange_name_validate(cvar_t *var, char *val, qbool *cancel);
cvar_t  name                    = {"name", "player", CVAR_USERINFO, CL_OnChange_name_validate};
cvar_t  team                    = {"team", "", CVAR_USERINFO};
cvar_t  topcolor                = {"topcolor","", CVAR_USERINFO};
cvar_t  bottomcolor             = {"bottomcolor","", CVAR_USERINFO};
cvar_t  skin                    = {"skin", "", CVAR_USERINFO};
cvar_t  rate                    = {"rate", "5760", CVAR_USERINFO};
void OnChange_AppliedAfterReconnect (cvar_t *var, char *value, qbool *cancel);
cvar_t  mtu                     = {"mtu", "", CVAR_USERINFO, OnChange_AppliedAfterReconnect};
cvar_t  msg                     = {"msg", "1", CVAR_USERINFO};
cvar_t  noaim                   = {"noaim", "1", CVAR_USERINFO};
cvar_t  w_switch                = {"w_switch", "", CVAR_USERINFO};
cvar_t  b_switch                = {"b_switch", "", CVAR_USERINFO};
cvar_t  railcolor               = {"railcolor", "", CVAR_USERINFO};
cvar_t  gender                  = {"gender", "", CVAR_USERINFO};

cvar_t  cl_mediaroot            = {"cl_mediaroot", "0"};

cvar_t  msg_filter              = {"msg_filter", "0"};

cvar_t  cl_onload               = {"cl_onload", "menu"};

#ifdef WIN32
cvar_t cl_verify_qwprotocol     = {"cl_verify_qwprotocol", "1"};
#endif // WIN32

cvar_t demo_autotrack           = {"demo_autotrack", "0"}; // use or not autotrack info from mvd demos

/// persistent client state
clientPersistent_t	cls;

/// client state
clientState_t		cl;

centity_t       cl_entities[CL_MAX_EDICTS];
efrag_t			cl_efrags[MAX_EFRAGS];
entity_t		cl_static_entities[MAX_STATIC_ENTITIES];
lightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
dlight_t		cl_dlights[MAX_DLIGHTS];
unsigned int cl_dlight_active[MAX_DLIGHTS/32];

// refresh list
visentlist_t	cl_firstpassents, cl_visents, cl_alphaents;

double		connect_time = 0;		// for connection retransmits
qbool		connected_via_proxy = false;

qbool	host_skipframe;			// used in demo playback

byte		*host_basepal = NULL;
byte		*host_colormap = NULL;

int		fps_count;
double		lastfps;

static int next_spec_track = -1;

static void CL_Multiview(void);
static void CL_MultiviewSaveValues (void);
static void CL_MultiviewRestoreValues (void);
static void CL_MultiviewOverrideValues (void);
static qbool CL_MultiviewCvarResetRequired (void);

// emodel and pmodel are encrypted to prevent llamas from easily hacking them
char emodel_name[] = { 'e'^0xe5, 'm'^0xe5, 'o'^0xe5, 'd'^0xe5, 'e'^0xe5, 'l'^0xe5, 0 };
char pmodel_name[] = { 'p'^0xe5, 'm'^0xe5, 'o'^0xe5, 'd'^0xe5, 'e'^0xe5, 'l'^0xe5, 0 };

static void simple_crypt (char *buf, int len) {
	while (len--)
		*buf++ ^= 0xe5;
}

static void CL_FixupModelNames (void) {
	simple_crypt (emodel_name, sizeof(emodel_name) - 1);
	simple_crypt (pmodel_name, sizeof(pmodel_name) - 1);
}

void OnChange_AppliedAfterReconnect (cvar_t *var, char *value, qbool *cancel)
{
	if (cls.state != ca_disconnected)
	{
		Com_Printf ("%s change will be applied after reconnect!\n", var->name);
	}
}


#ifdef WIN32

#define QW_URL_ROOT_REGKEY			"Software\\Classes\\qw"
#define QW_URL_OPEN_CMD_REGKEY		QW_URL_ROOT_REGKEY"\\shell\\Open\\Command"
#define QW_URL_DEFAULTICON_REGKEY	QW_URL_ROOT_REGKEY"\\DefaultIcon"

//
// Checks if this client is the default qw protocol handler.
//
qbool CL_CheckIfQWProtocolHandler()
{
	DWORD type;
	char reg_path[1024];
	DWORD len = (DWORD) sizeof(reg_path);
	HKEY hk;

	if (RegOpenKey(HKEY_CURRENT_USER, QW_URL_OPEN_CMD_REGKEY, &hk) != 0)
	{
		return false;
	}

	// Get the size we need to read.
	if (RegQueryValueEx(hk, NULL, 0, &type, (BYTE *) reg_path, &len) == ERROR_SUCCESS)
	{
		char expanded_reg_path[MAX_PATH];

		// Expand any environment variables in the reg value so that we get a real path to compare with.
		ExpandEnvironmentStrings(reg_path, expanded_reg_path, sizeof(expanded_reg_path));

		#if 0
		// This checks if the current exe is associated with, otherwise it will prompt the user
		// a bit more "in the face" if the user has more than one ezquake version.
		{
			char exe_path[MAX_PATH];
		
			// Get the long path of the current process.
			// C:\Program Files\Quake\ezquake-gl.exe
			Sys_GetFullExePath(exe_path, sizeof(exe_path), true);

			if (strstri(reg_path, exe_path) || strstri(expanded_reg_path, exe_path))
			{
				// This exe is associated with the qw:// protocol, return true.
				CloseHandle(hk);
				return true;
			}

			// Get the short path and check if that matches instead.
			// C:\Program~1\Quake\ezquake-gl.exe
			Sys_GetFullExePath(exe_path, sizeof(exe_path), false);

			if (strstri(reg_path, exe_path) || strstri(expanded_reg_path, exe_path))
			{
				CloseHandle(hk);
				return true;
			}
		}
		#else
		// Only check if ezquake is in the string that associates with the qw:// protocol
		// so if you have several ezquake exes it won't bug you if you just switch between those
		// (Only one will be registered as the protocol handler though ofcourse).

		if (strstri(reg_path, "ezquake"))
		{
			CloseHandle(hk);
			return true;
		}

		#endif
	}

	CloseHandle(hk);
	return false;
}

void CL_RegisterQWURLProtocol_f(void)
{
	//
	// Note!
	// HKEY_CLASSES_ROOT is a "merged view" of both: “HKEY_LOCAL_MACHINE\Software\Classes” 
	// and “HKEY_CURRENT_USER\Software\Classes”. 
	// User specific settings has priority over machine settings.
	//
	// If you try to write to HKEY_CLASSES_ROOT directly, it will default to
	// trying to write to the machine specific settings. If the user isn't
	// admin this will fail. On Vista this requires UAC usage.
	// Because of this, we always write specifically to "HKEY_CURRENT_USER\Software\Classes"
	//

	HKEY keyhandle;
	char exe_path[MAX_PATH];

	Sys_GetFullExePath(exe_path, sizeof(exe_path), true);

	//
	// HKCU\qw\shell\Open\Command
	//
	{
		char open_cmd[1024];
		snprintf(open_cmd, sizeof(open_cmd), "\"%s\" +qwurl %%1", exe_path);

		// Open / Create the key.
		if (RegCreateKeyEx(HKEY_CURRENT_USER,		// A handle to an open subkey.
						QW_URL_OPEN_CMD_REGKEY,		// Subkey.
						0,							// Reserved, must be 0.
						NULL,						// Class, ignored.
						REG_OPTION_NON_VOLATILE,	// Save the change to disk.
						KEY_WRITE,					// Access rights.
						NULL,						// Security attributes (NULL means default, inherited from direct parent).
						&keyhandle,					// Handle to the created key.
						NULL))						// Don't care if the key existed or not.
		{
			Com_Printf_State(PRINT_WARNING, "Could not create HKCU\\"QW_URL_OPEN_CMD_REGKEY"\n");
			return;
		}

		// Set the key value.
		if (RegSetValueEx(keyhandle, NULL, 0, REG_SZ, (BYTE *) open_cmd,  strlen(open_cmd) * sizeof(char)))
		{
			Com_Printf_State(PRINT_WARNING, "Could not set HKCU\\"QW_URL_OPEN_CMD_REGKEY"\\@\n");
			RegCloseKey(keyhandle);
			return;
		}

		RegCloseKey(keyhandle);
	}

	//
	// HKCU\qw\DefaultIcon
	//
	{
		char default_icon[1024];
		snprintf(default_icon, sizeof(default_icon), "\"%s\",1", exe_path);

		// Open / Create the key.
		if (RegCreateKeyEx(HKEY_CURRENT_USER, QW_URL_DEFAULTICON_REGKEY, 
			0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &keyhandle, NULL))
		{
			Com_Printf_State(PRINT_WARNING, "Could not create HKCU\\"QW_URL_OPEN_CMD_REGKEY"\n");
			return;
		}

		// Set the key value.
		if (RegSetValueEx(keyhandle, NULL, 0, REG_SZ, (BYTE *) default_icon, strlen(default_icon) * sizeof(char)))
		{
			Com_Printf_State(PRINT_WARNING, "Could not set HKCU\\"QW_URL_OPEN_CMD_REGKEY"\\@\n");
			RegCloseKey(keyhandle);
			return;
		}

		RegCloseKey(keyhandle);
	}

	//
	// HKCU\qw
	//
	{
		char protocol_name[] = "URL:QW Protocol";

		// Open / Create the key.
		if (RegCreateKeyEx(HKEY_CURRENT_USER, QW_URL_ROOT_REGKEY, 
			0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &keyhandle, NULL))
		{
			Com_Printf_State(PRINT_WARNING, "Could not create HKCU\\qw\n");
			return;
		}

		// Set the protocol name.
		if (RegSetValueEx(keyhandle, NULL, 0, REG_SZ, (BYTE *) protocol_name, strlen(protocol_name) * sizeof(char)))
		{
			Com_Printf_State(PRINT_WARNING, "Could not set HKCU\\qw\\@\n");
			RegCloseKey(keyhandle);
			return;
		}

		if (RegSetValueEx(keyhandle, "URL Protocol", 0, REG_SZ, (BYTE *) "", sizeof(char)))
		{
			Com_Printf_State(PRINT_WARNING, "Could not set HKCU\\qw\\URL Protocol\n");
			RegCloseKey(keyhandle);
			return;
		}

		RegCloseKey(keyhandle);
	}
}

#endif // WIN32

//============================================================================

char *CL_Macro_ConnectionType(void) 
{
	char *s;
	static char macrobuf[16];

	s = (cls.state < ca_connected) ? "disconnected" : cl.spectator ? "spectator" : "player";
	strlcpy(macrobuf, s, sizeof(macrobuf));
	return macrobuf;
}

char *CL_Macro_Demoplayback(void) 
{
	char *s;
	static char macrobuf[16];

	s = cls.mvdplayback ? "mvdplayback" : cls.demoplayback ? "qwdplayback" : "0";
	strlcpy(macrobuf, s, sizeof(macrobuf));
	return macrobuf;
}

char *CL_Macro_Demotime(void)
{   
	// Intended for scripted & timed camera movement
	static char macrobuf[16];

	snprintf(macrobuf, sizeof(macrobuf), "%f", (float) cls.demotime);
	return macrobuf;
}

char *CL_Macro_Rand(void)
{
	// Returns a number in range <0..1)
	static char macrobuf[16];

	snprintf(macrobuf, sizeof(macrobuf), "%f", (double) rand() / RAND_MAX);
	return macrobuf;
}

char *CL_Macro_Serverstatus(void)
{
	char *s;
	static char macrobuf[16];

	s = (cls.state < ca_connected) ? "disconnected" : cl.standby ? "standby" : "normal";
	strlcpy(macrobuf, s, sizeof(macrobuf));
	return macrobuf;
}

char *CL_Macro_ServerIp(void) 
{
	return NET_AdrToString(cls.server_adr);
}

char *CL_Macro_Conwidth(void) 
{
	static char macrobuf[16];
	snprintf(macrobuf, sizeof(macrobuf), "%i", vid.conwidth);
	return macrobuf;
}

char *CL_Macro_Conheight(void)
{
	static char macrobuf[16];
	snprintf(macrobuf, sizeof(macrobuf), "%i", vid.conheight);
	return macrobuf;
}

int CL_ClientState (void)
{
	return cls.state;
}

void CL_MakeActive(void) 
{
	cls.state = ca_active;
	if (cls.demoplayback) 
	{
		host_skipframe = true;
		demostarttime = cls.demotime;
	}

	if (!cls.demoseeking) {
		Con_ClearNotify ();
	}
	TP_ExecTrigger ("f_spawn");
}

// Cvar system calls this when a CVAR_USERINFO cvar changes
void CL_UserinfoChanged (char *key, char *string) 
{
	char *s;

	s = TP_ParseFunChars (string, false);

	if (strcmp(s, Info_ValueForKey (cls.userinfo, key))) 
	{
		Info_SetValueForKey (cls.userinfo, key, s, MAX_INFO_STRING);

		if (cls.state >= ca_connected)
		{
			if (cls.mvdplayback == QTV_PLAYBACK)
			{
				QTV_Cmd_Printf(QTV_EZQUAKE_EXT_SETINFO, "setinfo \"%s\" \"%s\"", key, s);
			}
			else
			{
				MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
				SZ_Print (&cls.netchan.message, va("setinfo \"%s\" \"%s\"", key, s));
			}
		}
	}
}

#ifdef PROTOCOL_VERSION_FTE
unsigned int CL_SupportedFTEExtensions (void)
{
	unsigned int fteprotextsupported = 0;

	if (!cl_pext.value)
		return 0;

#ifdef FTE_PEXT_CHUNKEDDOWNLOADS
	if (cl_pext_chunkeddownloads.value)
		fteprotextsupported |= FTE_PEXT_CHUNKEDDOWNLOADS;
#endif

#ifdef FTE_PEXT_256PACKETENTITIES
	if (cl_pext_256packetentities.value)
		fteprotextsupported |= FTE_PEXT_256PACKETENTITIES;
#endif

#ifdef FTE_PEXT_FLOATCOORDS
	if (cl_pext_floatcoords.value)
		fteprotextsupported |= FTE_PEXT_FLOATCOORDS;
#endif

#ifdef FTE_PEXT_TRANS
	if (cl_pext_alpha.value)
		fteprotextsupported |= FTE_PEXT_TRANS;
#endif

	if (cl_pext_other.value)
	{
#ifdef FTE_PEXT_ACCURATETIMINGS
		fteprotextsupported |= FTE_PEXT_ACCURATETIMINGS;
#endif
#ifdef FTE_PEXT_HLBSP
		fteprotextsupported |= FTE_PEXT_HLBSP;
#endif
#ifdef FTE_PEXT_MODELDBL
		fteprotextsupported |= FTE_PEXT_MODELDBL;
#endif
#ifdef FTE_PEXT_ENTITYDBL
		fteprotextsupported |= FTE_PEXT_ENTITYDBL;
#endif
#ifdef FTE_PEXT_ENTITYDBL2
		fteprotextsupported |= FTE_PEXT_ENTITYDBL2;
#endif
#ifdef FTE_PEXT_SPAWNSTATIC2
		fteprotextsupported |= FTE_PEXT_SPAWNSTATIC2;
#endif
	}

	return fteprotextsupported;
}
#endif // PROTOCOL_VERSION_FTE

#ifdef PROTOCOL_VERSION_FTE2
unsigned int CL_SupportedFTEExtensions2 (void)
{
	unsigned int fteprotextsupported2 = 0;

	if (!cl_pext.value)
		return 0;

	return fteprotextsupported2;
}
#endif // PROTOCOL_VERSION_FTE2


// Called by CL_Connect_f and CL_CheckResend
static void CL_SendConnectPacket(
#ifdef PROTOCOL_VERSION_FTE
							unsigned int ftepext
#ifdef PROTOCOL_VERSION_FTE2
							,
#endif // PROTOCOL_VERSION_FTE2
#endif // PROTOCOL_VERSION_FTE
#ifdef PROTOCOL_VERSION_FTE2
							unsigned int ftepext2
#endif // PROTOCOL_VERSION_FTE2

								) 
{
	char data[2048];
	char biguserinfo[MAX_INFO_STRING + 32];
	int extensions;
	extern cvar_t cl_novweps;

	if (cls.state != ca_disconnected)
		return;

	#ifdef PROTOCOL_VERSION_FTE
	cls.fteprotocolextensions  = (ftepext & CL_SupportedFTEExtensions());
	#endif // PROTOCOL_VERSION_FTE
	#ifdef PROTOCOL_VERSION_FTE2
	cls.fteprotocolextensions2  = (ftepext2 & CL_SupportedFTEExtensions2());
	#endif // PROTOCOL_VERSION_FTE

	connect_time = cls.realtime; // For retransmit requests
	cls.qport = Cvar_Value("qport");

	// Let the server know what extensions we support.
	strlcpy (biguserinfo, cls.userinfo, sizeof (biguserinfo));
	extensions = CLIENT_EXTENSIONS &~ (cl_novweps.value ? Z_EXT_VWEP : 0);
	Info_SetValueForStarKey (biguserinfo, "*z_ext", va("%i", extensions), sizeof(biguserinfo));

	snprintf(data, sizeof(data), "\xff\xff\xff\xff" "connect %i %i %i \"%s\"\n", PROTOCOL_VERSION, cls.qport, cls.challenge, biguserinfo);

	#ifdef PROTOCOL_VERSION_FTE
	if (cls.fteprotocolextensions) 
	{
		char tmp[128];
		snprintf(tmp, sizeof(tmp), "0x%x 0x%x\n", PROTOCOL_VERSION_FTE, cls.fteprotocolextensions);
		Com_Printf_State(PRINT_DBG, "0x%x is fte protocol ver and 0x%x is fteprotocolextensions\n", PROTOCOL_VERSION_FTE, cls.fteprotocolextensions);
		strlcat(data, tmp, sizeof(data));
	}
	#endif // PROTOCOL_VERSION_FTE 

	#ifdef PROTOCOL_VERSION_FTE2
	if (cls.fteprotocolextensions2) 
	{
		char tmp[128];
		snprintf(tmp, sizeof(tmp), "0x%x 0x%x\n", PROTOCOL_VERSION_FTE2, cls.fteprotocolextensions2);
		Com_Printf_State(PRINT_DBG, "0x%x is fte protocol ver and 0x%x is fteprotocolextensions2\n", PROTOCOL_VERSION_FTE2, cls.fteprotocolextensions2);
		strlcat(data, tmp, sizeof(data));
	}
	#endif // PROTOCOL_VERSION_FTE2 


	NET_SendPacket(NS_CLIENT, strlen(data), data, cls.server_adr);
}

// Resend a connect message if the last one has timed out
void CL_CheckForResend (void) 
{
	char data[2048];
	double t1, t2;

	if (cls.state == ca_disconnected && com_serveractive) 
	{
		// if the local server is running and we are not, then connect
		strlcpy (cls.servername, "local", sizeof(cls.servername));
		NET_StringToAdr("local", &cls.server_adr);

		// We don't need a challenge on the local server.
		CL_SendConnectPacket(
#ifdef PROTOCOL_VERSION_FTE
				svs.fteprotocolextensions
	#ifdef PROTOCOL_VERSION_FTE2
				,
	#endif // PROTOCOL_VERSION_FTE2
#endif // PROTOCOL_VERSION_FTE
#ifdef PROTOCOL_VERSION_FTE2
				svs.fteprotocolextensions2
#endif // PROTOCOL_VERSION_FTE
				);
		
		// FIXME: cls.state = ca_connecting so that we don't send the packet twice?
		return;
	}

	if (cls.state != ca_disconnected || !connect_time)
		return;
	if (cls.realtime - connect_time < 5.0)
		return;

	t1 = Sys_DoubleTime();
	if (!NET_StringToAdr(cls.servername, &cls.server_adr)) 
	{
		Com_Printf("Bad server address\n");
		connect_time = 0;
		return;
	}

	t2 = Sys_DoubleTime();
	connect_time = cls.realtime + t2 - t1;	// for retransmit requests

	if (cls.server_adr.port == 0)
		cls.server_adr.port = BigShort(PORT_SERVER);

	Com_Printf("&cf11connect:&r %s...\n", cls.servername);
	snprintf(data, sizeof(data), "\xff\xff\xff\xff" "getchallenge\n");
	NET_SendPacket(NS_CLIENT, strlen(data), data, cls.server_adr);
}

void CL_BeginServerConnect(void) 
{
	connect_time = -999;	// CL_CheckForResend() will fire immediately
	CL_CheckForResend();
}

static void CL_QWURL_ProcessChallenge(const char *parameters)
{
	extern cvar_t match_auto_logupload_token;
	extern cvar_t match_challenge;

	// parameters is expected to be of the format "?token=<string>&otherparam=<string>&..."
	char info_buf[1024];
	char *wp = info_buf;
	const char *rp = parameters;
	size_t write_len = 0;
	ctxinfo_t ctx;
	char *token;
	memset(&ctx, 0, sizeof(ctxinfo_t));
	ctx.max = 20;

	while (*rp && write_len < 1022) {
		char c = *rp++;
		if (c == '?') {
			c = '\\';
		}
		else if (c == '&') {
			c = '\\';
		}
		else if (c == '=') {
			c = '\\';
		}

		*wp++ = c;
		write_len++;
	}

	*wp++ = '\0';

	Info_Convert(&ctx, info_buf);

	token = Info_Get(&ctx, "token");

	Info_RemoveAll(&ctx);

	if (*token) {
		Cvar_Set(&match_auto_logupload_token, token);
		Cvar_Set(&match_challenge, "1");
		Com_Printf("Joining challenge ...\n");
	}
	else {
		Com_Printf("Challenge token not found in the URL\n");
	}
}

//
// Parses a QW-URL of the following format 
// (this can be associated with ezquake in windows by setting some reg info):
// qw://server:port/command
//
// Supported commands:
// - join/connect
// - spectate/observe
// - qtv
//
void CL_QWURL_f (void)
{
	char *connection_str = NULL;
	char *command = NULL;

	if (Cmd_Argc() != 2) 
	{
		Com_Printf ("Usage: %s <qw-url>\n", Cmd_Argv(0));
		return;
	}

	// Strip the leading qw:// first.
	{
		char qws_str[] = "qw://";
		int qws_len	= sizeof(qws_str) - 1;

		connection_str = Cmd_Argv(1);

		if (!strncasecmp(qws_str, connection_str, qws_len))
		{
			connection_str += qws_len;
		}
		else
		{
			Com_Printf("%s: The QW-URL must start with qw://\n", Cmd_Argv(0));
			return;
		}
	}

	// Find the first "/" and treat what's after it as the command.	
	if ((command = strchr(connection_str, '/')))
	{
		// Null terminate the server name string.
		*command = 0;
		command++;
	}
	else
	{
		// No command given.
		command = "";
	}

	// Default to connecting.
	if (!strcmp(command, "") || !strncasecmp(command, "join", 4) || !strncasecmp(command, "connect", 7))
	{
		Cbuf_AddText(va("connect %s\n", connection_str));
	}
	else if (!strncmp(command, "challenge?", 10))
	{
		CL_QWURL_ProcessChallenge(command + 9);
		Cbuf_AddText(va("connect %s\n", connection_str));
	}
	else if (!strncasecmp(command, "spectate", 8) || !strncasecmp(command, "observe", 7))
	{
		Cbuf_AddText(va("observe %s\n", connection_str));
	}
	else if (!strncasecmp(command, "qtv", 3))
	{
		char *password = command + 4;

		Cbuf_AddText(va("qtvplay %s%s\n", connection_str, ((*password) ? va(" %s", password) : "")));
	}
	else
	{
		Com_Printf("%s: Illegal command %s\n", Cmd_Argv(0), command);
	}
}

void CL_Connect_f (void) 
{
	qbool proxy;
	char *connect_addr = NULL;
	char *server_buf = NULL;

	if (Cmd_Argc() != 2) 
	{
		Com_Printf ("Usage: %s <server>\n", Cmd_Argv(0));
		return;
	}

	// in this part proxy means QWFWD proxy
	if (cl_proxyaddr.string[0]) {
		char *secondproxy;
		if ((secondproxy = strchr(cl_proxyaddr.string, '@'))) {
			size_t prx_buf_len = strlen(cl_proxyaddr.string) + strlen(Cmd_Argv(1)) + 2;
			char *prx_buf = (char *) Q_malloc(prx_buf_len);
			server_buf = (char *) Q_malloc(strlen(cl_proxyaddr.string) + 1); // much more than needed

			strlcpy(server_buf, cl_proxyaddr.string, secondproxy - cl_proxyaddr.string + 1);
			connect_addr = server_buf;
			
			strlcpy(prx_buf, secondproxy + 1, prx_buf_len);
			strlcat(prx_buf, "@", prx_buf_len);
			strlcat(prx_buf, Cmd_Argv(1), prx_buf_len);
			Info_SetValueForKeyEx(cls.userinfo, "prx", prx_buf, MAX_INFO_STRING, false);
			Q_free(prx_buf);
		}
		else {
			Info_SetValueForKey (cls.userinfo, "prx", Cmd_Argv(1), MAX_INFO_STRING);
#if 0 // FIXME: qqshka: disabled untill one explain that it does and why.
			if (cls.state >= ca_connected) {
				Cmd_ForwardToServer ();
			}
#endif
			connect_addr = cl_proxyaddr.string;
		}
		connected_via_proxy = true;
	}
	else
	{
		connect_addr = Cmd_Argv(1);
		connected_via_proxy = false;
	}

	// in this part proxy means Qizmo proxy
	proxy = cl_useproxy.value && CL_ConnectedToProxy();

	if (proxy)
	{
		Cbuf_AddText(va("say ,connect %s\n", connect_addr));
	} 
	else
	{
		Host_EndGame();
		strlcpy(cls.servername, connect_addr, sizeof(cls.servername));
		CL_BeginServerConnect();
	}

	if (server_buf) Q_free(server_buf);
}

void CL_Connect_BestRoute_f(void)
{
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <address>\nConnects to given server via fastest available path (ping-wise).\n", Cmd_Argv(0));
		Com_Printf("Requires Server Browser refreshed with sb_findroutes 1\n");
		return;
	}
	else {
		netadr_t adr;
		if (!NET_StringToAdr(Cmd_Argv(1), &adr)) {
			Com_Printf("Invalid address\n");
			return;
		}

		if (adr.port == 0)
			adr.port = htons(27500);

		SB_PingTree_DumpPath(&adr);
		SB_PingTree_ConnectBestPath(&adr);
	}
}

void CL_TCPConnect_f (void)
{
	char buffer[6] = {'q', 'i', 'z', 'm', 'o', '\n'};
	int newsocket;
	int _true = true;

	float giveuptime;

	if (Cmd_Argc() != 2) {
		Com_Printf ("Usage: %s <server>\n", Cmd_Argv(0));
		return;
	}

	Host_EndGame (); // CL_Disconnect_f();

	strlcpy(cls.servername, Cmd_Argv (1), sizeof(cls.servername));

	NET_StringToAdr(cls.servername, &cls.sockettcpdest);

	if (cls.sockettcp != INVALID_SOCKET)
		closesocket(cls.sockettcp);

	cls.sockettcp = INVALID_SOCKET;
	cls.tcpinlen = 0;

	newsocket = TCP_OpenStream(cls.sockettcpdest);
	if (newsocket == INVALID_SOCKET)
	{
		// Failed
		Com_Printf("Failed to connect, server is either down, firewalled, or on a different port\n");
		return;
	}

	Com_Printf("Waiting for confirmation of server (10 secs)\n");

	giveuptime = Sys_DoubleTime() + 10;

#if 1 // qqshka: qizmo sends "qizmo\n" then expects reply, unfortunatelly that does not work for mvdsv
	// that how MVDSV expects, should work with qizmo too
	send(newsocket, buffer, sizeof(buffer), 0);
	memset(buffer, 0, sizeof(buffer));
#endif

	while(giveuptime > Sys_DoubleTime())
	{
		recv(newsocket, buffer, sizeof(buffer), 0);
		if (!strncmp(buffer, "qizmo\n", 6))
		{
			cls.sockettcp = newsocket;
			break;
		}
		SCR_UpdateScreen();
	}

	if (cls.sockettcp == INVALID_SOCKET)
	{
		Com_Printf("Timeout - wrong server type\n");
		closesocket(newsocket);
		return;
	}

	Com_Printf("Confirmed\n");

#if 0 // qqshka: qizmo sends "qizmo\n" then expects reply, unfortunatelly that does not work for mvdsv
	// that how qizmo expects, does not work with MVDSV
	send(cls.sockettcp, buffer, sizeof(buffer), 0);
#endif

	if (setsockopt(cls.sockettcp, IPPROTO_TCP, TCP_NODELAY, (char *)&_true, sizeof(_true)) == -1) {
		Com_Printf ("CL_TCPConnect_f: setsockopt: (%i): %s\n", qerrno, strerror(qerrno));
	}

	CL_BeginServerConnect();
}

qbool CL_ConnectedToProxy(void)
{
	cmd_alias_t *alias = NULL;
	qbool found = true;
	char **s;
	char *qizmo_aliases[] = {	"ezcomp", "ezcomp2", "ezcomp3",
									"f_sens", "f_fps", "f_tj", "f_ta", NULL};
	char *fteqtv_aliases[] = { "+proxleft", "+proxright", NULL }; // who would need more?

	if (cls.state < ca_active)
		return false;

	for (s = qizmo_aliases; *s; s++) 
	{
		if (!(alias = Cmd_FindAlias(*s)) || !(alias->flags & ALIAS_SERVER)) 
		{
			found = false; 
			break;
		}
	}

	if (found)
		return true;

	found = true;
	for (s = fteqtv_aliases; *s; s++) 
	{
		if (!(alias = Cmd_FindAlias(*s)) || !(alias->flags & ALIAS_SERVER)) 
		{
			found = false; break;
		}
	}

	return found;
}

void CL_Join_f (void) 
{
	qbool proxy;

	proxy = cl_useproxy.value && CL_ConnectedToProxy();

	if (Cmd_Argc() > 2) 
	{
		Com_Printf ("Usage: %s [server]\n", Cmd_Argv(0));
		return;
	}

	Cvar_Set(&spectator, "");

	if (Cmd_Argc() == 2) 
	{
		// A server name was given, connect directly or through Qizmo
		Cvar_Set(&spectator, "");
		Cbuf_AddText(va("%s %s\n", proxy ? "say ,connect" : "connect", Cmd_Argv(1)));
		return;
	}

	if (cls.mvdplayback == QTV_PLAYBACK) {
		qtvlist_joinfromqtv_cmd();
		return;
	}

	if (!cls.demoplayback && (cl.z_ext & Z_EXT_JOIN_OBSERVE)) 
	{
		// Server supports the 'join' command, good
		Cmd_ExecuteString("cmd join");
		return;
	}

	Cbuf_AddText(va("%s\n", proxy ? "say ,reconnect" : "reconnect"));
}

void CL_Observe_f (void) 
{
	qbool proxy;

	proxy = cl_useproxy.value && CL_ConnectedToProxy();

	if (Cmd_Argc() > 2) 
	{
		Com_Printf ("Usage: %s [server]\n", Cmd_Argv(0));
		return;
	}

	Cvar_SetValue(&spectator, 1);

	if (Cmd_Argc() == 2) 
	{
		// A server name was given, connect directly or through Qizmo
		Cbuf_AddText(va("%s %s\n", proxy ? "say ,connect" : "connect", Cmd_Argv(1)));
		return;
	}

	if (!cls.demoplayback && (cl.z_ext & Z_EXT_JOIN_OBSERVE))
	{
		// Server supports the 'join' command, good
		Cmd_ExecuteString("cmd observe");
		return;
	}

	Cbuf_AddText(va("%s\n", proxy ? "say ,reconnect" : "reconnect"));
}

// Just toggle mode between spec and player.
void Cl_ToggleSpec_f (void)
{
	if (spectator.string[0])
		CL_Join_f();
	else
		CL_Observe_f();
}

void CL_Hash_f(void)
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: %s <string to hash>\n", Cmd_Argv(0));
		return;
	}

	Com_Printf("%u\n", Com_HashKey(Cmd_Argv(1)));
}

void CL_DNS_f(void) 
{
	char address[128], *s;
	struct hostent *h;
	struct in_addr addr;

	if (Cmd_Argc() != 2) 
	{
		Com_Printf("Usage: %s <address>\n", Cmd_Argv(0));
		return;
	}
	
	strlcpy(address, Cmd_Argv(1), sizeof(address));
	if ((s = strchr(address, ':')))
		*s = 0;
	addr.s_addr = inet_addr(address);
	
	if (inet_addr(address) == INADDR_NONE) 
	{
		// Forward lookup
		if (!(h = gethostbyname(address))) 
		{
			Com_Printf("Couldn't resolve %s\n", address);
		} 
		else 
		{
			addr.s_addr = *(int *) h->h_addr_list[0];
			Com_Printf("Resolved %s to %s\n", address, inet_ntoa(addr));
		}
		return;
	}

	// Reverse lookup ip address
	if (!(h = gethostbyaddr((char *) &addr, sizeof(addr), AF_INET)))
		Com_Printf("Couldn't resolve %s\n", address);
	else
		Com_Printf("Resolved %s to %s\n", address, h->h_name);
}

void SCR_ClearShownick(void);
void SCR_ClearTeamInfo(void);
void SCR_ClearWeaponStats(void);

void CL_ClearState (void) 
{
	int i;
	extern float scr_centertime_off;
	extern cshift_t	cshift_empty;
	extern void CL_ProcessServerInfo (void);

	S_StopAllSounds();

	Com_DPrintf ("Clearing memory\n");

	if (!com_serveractive)
		Host_ClearMemory();

	CL_ClearTEnts ();
	CL_ClearScene ();

	CL_ClearPredict();

	// Wipe the entire cl structure.
	memset(&cl, 0, sizeof(cl));

	SZ_Clear (&cls.netchan.message);

	// Clear other arrays.
	memset(cl_efrags, 0, sizeof(cl_efrags));
	memset(cl_dlight_active, 0, sizeof(cl_dlight_active));
	memset(cl_lightstyle, 0, sizeof(cl_lightstyle));
	memset(cl_entities, 0, sizeof(cl_entities));

	// Set default viewheight for mvd, we copy cl.players[].stats[] to cl_stats[] in Cam_Lock() when pov changes.
	for (i = 0; i < MAX_CLIENTS; i++)
		cl.players[i].stats[STAT_VIEWHEIGHT] = DEFAULT_VIEWHEIGHT;
	// Set default viewheight for normal game/current pov.
	cl.stats[STAT_VIEWHEIGHT] = DEFAULT_VIEWHEIGHT;

	// Make sure no centerprint messages are left from previous level.
	scr_centertime_off = 0;

	// Allocate the efrags and chain together into a free list.
	cl.free_efrags = cl_efrags;
	for (i = 0; i < MAX_EFRAGS - 1; i++)
		cl.free_efrags[i].entnext = &cl.free_efrags[i + 1];
	cl.free_efrags[i].entnext = NULL;

	memset(&cshift_empty, 0, sizeof(cshift_empty));

	// Clear shownick structs
	SCR_ClearShownick();

	// Clear teaminfo structs
	SCR_ClearTeamInfo();

	// Clear weapon stats structs
	SCR_ClearWeaponStats();

	if (!com_serveractive)
		Cvar_ForceSet (&host_mapname, ""); // Notice mapname not valid yet.

	CL_ProcessServerInfo(); // Force set some default variables, because server may not sent fullserverinfo.
}

// Sends a disconnect message to the server
// This is also called on Host_Error, so it shouldn't cause any errors
void CL_Disconnect (void) 
{
	byte final[10];

	connect_time = 0;
	con_addtimestamp = true;

	if (cl.teamfortress)
		V_TF_ClearGrenadeEffects();
	cl.teamfortress = false;

	// Reset values changed by Multiview.
	if (CL_MultiviewCvarResetRequired()) {
		CL_MultiviewRestoreValues ();
		nTrack1duel = nTrack2duel = 0;
		CURRVIEW = 0;
	}

	// Stop sounds (especially looping!)
	S_StopAllSounds();

	MT_Disconnect();

	if (cls.demorecording && cls.state != ca_disconnected)
		CL_Stop_f();

	if (cls.mvdrecording && cls.state != ca_disconnected)
	{
		extern void CL_StopMvd_f(void);

		CL_StopMvd_f();
	}

	if (cls.demoplayback) 
	{
		CL_StopPlayback();
	} 
	else if (cls.state != ca_disconnected) 
	{
		final[0] = clc_stringcmd;
		strlcpy ((char *)(final + 1), "drop", sizeof (final) - 1);
		Netchan_Transmit (&cls.netchan, 6, final);
		Netchan_Transmit (&cls.netchan, 6, final);
		Netchan_Transmit (&cls.netchan, 6, final);

		CL_UnqueOutputPacket(true);

		// TCP connect, that gives TCP a chance to transfer data to the server...
		if (cls.sockettcp != INVALID_SOCKET)
		{
			Sys_MSleep(1000);
		}
	}

	memset(&cls.netchan, 0, sizeof(cls.netchan));
	memset(&cls.server_adr, 0, sizeof(cls.server_adr));
	cls.state = ca_disconnected;
	connect_time = 0;

	Cam_Reset();

	if (cls.download) {
		CL_FinishDownload();
	} else {
		/* Just to make sure it's not in an ambigious state */
		cls.downloadmethod = DL_NONE;
		cls.downloadnumber = 0;
		cls.downloadpercent = 0;
		cls.downloadtype = dl_none;
	}
		

	CL_StopUpload();
	DeleteServerAliases();
	CL_RE_Trigger_ResetLasttime();

	// TCP connect.
	if (cls.sockettcp != INVALID_SOCKET)
	{
		closesocket(cls.sockettcp);
		cls.sockettcp = INVALID_SOCKET;
	}

	cls.qport++; // A hack I picked up from qizmo.

	SZ_Clear(&cls.cmdmsg);

	// So join/observe not confused
	Info_SetValueForStarKey (cl.serverinfo, "*z_ext", "", sizeof(cl.serverinfo));
	cl.z_ext = 0;

	// well, we need free qtv users before new connection
	QTV_FreeUserList();

	Cvar_ForceSet (&host_mapname, ""); // Notice mapname not valid yet
}

void CL_Disconnect_f (void) 
{
	extern int demo_playlist_started;
	extern int mvd_demo_track_run ;
	cl.intermission = 0;
	demo_playlist_started= 0;
	mvd_demo_track_run = 0;

	Host_EndGame();
}

// The server is changing levels.
void CL_Reconnect_f (void) 
{
	if (cls.download)
		return; // Don't change when downloading.

	S_StopAllSounds();

	if (cls.mvdplayback == QTV_PLAYBACK) 
	{
		return; // Change map during qtv playback.
	}

	if (cls.state == ca_connected) 
	{
		Com_Printf ("reconnecting...\n");
		MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");
		return;
	}

	if (!*cls.servername) 
	{
		Com_Printf ("No server to reconnect to.\n");
		return;
	}

	if (connected_via_proxy) {
		// we were connected via a proxy
		// the target server is (hopefully still) in userinfo/prx
		char *prx = Info_ValueForKey(cls.userinfo, "prx");
		char *lastnode = strrchr(prx, '@');
		if (lastnode) {
			lastnode++;
		}
		else {
			lastnode = prx;
		}
		
		Cbuf_AddText(va("connect %s\n", lastnode));
	}
	else if (!connected_via_proxy && cl_proxyaddr.string[0]) {
		// switch the stuff
		Cbuf_AddText(va("connect %s\n", cls.servername));
	}
	else {
		// good old reconnect, no need to do anything special
		Host_EndGame();
		CL_BeginServerConnect();
	}

	// remember what's the type of this new connection
	connected_via_proxy = cl_proxyaddr.string[0] ? true : false;
}

extern double qstat_senttime;
extern void CL_PrintQStatReply (char *s);

// Responses to broadcasts, etc
void CL_ConnectionlessPacket (void) 
{
	int c;
	char *s, cmdtext[2048];
	
	#ifdef PROTOCOL_VERSION_FTE
	unsigned int pext = 0;
	#endif // PROTOCOL_VERSION_FTE
	#ifdef PROTOCOL_VERSION_FTE2
	unsigned int pext2 = 0;
	#endif // PROTOCOL_VERSION_FTE2

    MSG_BeginReading();
    MSG_ReadLong();	// Skip the -1

	c = MSG_ReadByte();

	if (msg_badread)
		return;	// Runt packet

	switch(c) 
	{
		case S2C_CHALLENGE :
		{
			if (!NET_CompareAdr(net_from, cls.server_adr))
			{
				Com_DPrintf("S2C_CHALLENGE rejected\n");
				Com_DPrintf("net_from       %s\n", NET_AdrToString(net_from));
				Com_DPrintf("cls.server_adr %s\n", NET_AdrToString(cls.server_adr));
				return;
			}
			Com_Printf("&cf55challenge:&r %s\n", NET_AdrToString(net_from));
			cls.challenge = atoi(MSG_ReadString());

			for(;;)
			{
				c = MSG_ReadLong();
				if (msg_badread)
					break;

#ifdef PROTOCOL_VERSION_FTE
				if (c == PROTOCOL_VERSION_FTE)
					pext = MSG_ReadLong();
				else
#endif // PROTOCOL_VERSION_FTE
#ifdef PROTOCOL_VERSION_FTE2
				if (c == PROTOCOL_VERSION_FTE2)
					pext2 = MSG_ReadLong();
				else
#endif // PROTOCOL_VERSION_FTE2
					MSG_ReadLong();
			}

			CL_SendConnectPacket(
#ifdef PROTOCOL_VERSION_FTE
				pext
	#ifdef PROTOCOL_VERSION_FTE2
				,
	#endif // PROTOCOL_VERSION_FTE2
#endif // PROTOCOL_VERSION_FTE
#ifdef PROTOCOL_VERSION_FTE2
				pext2
#endif // PROTOCOL_VERSION_FTE
				);

			break;
		}
		case S2C_CONNECTION :
		{
			if (!NET_CompareAdr(net_from, cls.server_adr))
				return;
			if ((!com_serveractive || developer.value) && !cls.demoplayback)
				Com_Printf("&cff5connection:&r %s\n", NET_AdrToString(net_from));

			if (cls.state >= ca_connected) 
			{
				if (!cls.demoplayback)
					Com_Printf("Dup connect received.  Ignored.\n");
				break;
			}
			Netchan_Setup(NS_CLIENT, &cls.netchan, net_from, cls.qport);
			MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
			MSG_WriteString (&cls.netchan.message, "new");
			cls.state = ca_connected;
			if ((!com_serveractive || developer.value) && !cls.demoplayback)
				Com_Printf("&c1f1connected!&r\n");
			allowremotecmd = false; // localid required now for remote cmds
			break;
		}
		case A2C_CLIENT_COMMAND : 
		{
			// Remote command from gui front end
			Com_Printf ("%s: client command\n", NET_AdrToString (net_from));

			if (net_from.type != net_local_cl_ipadr.type
				|| ((*(unsigned *)net_from.ip != *(unsigned *)net_local_cl_ipadr.ip)
				&& (*(unsigned *)net_from.ip != htonl(INADDR_LOOPBACK))))
			{
				Com_Printf ("Command packet from remote host.  Ignored.\n");
				return;
			}

                        VID_Restore();
			
			s = MSG_ReadString ();
			strlcpy (cmdtext, s, sizeof(cmdtext));
			s = MSG_ReadString ();

			while (*s && isspace(*s))
				s++;
			while (*s && isspace(s[strlen(s) - 1]))
				s[strlen(s) - 1] = 0;

			if (!allowremotecmd && (!*localid.string || strcmp(localid.string, s))) 
			{
				if (!*localid.string) 
				{
					Com_Printf ("===========================\n");
					Com_Printf ("Command packet received from local host, but no "
						"localid has been set.  You may need to upgrade your server "
						"browser.\n");
					Com_Printf ("===========================\n");
				}
				else 
				{
					Com_Printf ("===========================\n");
					Com_Printf ("Invalid localid on command packet received from local host. "
						"\n|%s| != |%s|\n"
						"You may need to reload your server browser and ezQuake.\n",
						s, localid.string);
					Com_Printf ("===========================\n");
					Cvar_Set(&localid, "");
				}
			} 
			else 
			{
				Cbuf_AddText (cmdtext);
				Cbuf_AddText ("\n");
				allowremotecmd = false;
			}
			break;
		}
		case A2C_PRINT:		
		{
			// Print command from somewhere.
			
			#ifdef FTE_PEXT_CHUNKEDDOWNLOADS
			if (net_message.cursize > 100 && !strncmp((char *)net_message.data + 5, "\\chunk", sizeof("\\chunk")-1)) 
			{
				CL_Parse_OOB_ChunkedDownload();
				return;
			}
			#endif // FTE_PEXT_CHUNKEDDOWNLOADS

			if (net_message.data[msg_readcount] == '\\') 
			{
				if (qstat_senttime && curtime - qstat_senttime < 10)
				{
					CL_PrintQStatReply (MSG_ReadString());
					return;
				}
			}

			Com_Printf("%s: print\n", NET_AdrToString(net_from));
			Com_Printf("%s", MSG_ReadString());
			break;
		}
		case svc_disconnect :
		{
			if (cls.demoplayback)
			{
				Com_Printf("\n======== End of demo ========\n\n");
				Host_EndGame();
				Host_Abort();
			}
			break;
		}
	}
}

// Handles playback of demos, on top of NET_ code
qbool CL_GetMessage (void) 
{
	#ifdef _WIN32
	CL_CheckQizmoCompletion ();
	#endif

	if (cls.demoplayback)
		return CL_GetDemoMessage();

	if (!NET_GetPacket(NS_CLIENT))
		return false;

	return true;
}

void CL_ReadPackets (void) 
{
	if (cls.nqdemoplayback) 
	{
		NQD_ReadPackets();
		return;
	}

	while (CL_GetMessage()) 
	{
		// Remote command packet.
		if (*(int *)net_message.data == -1)	
		{
			CL_ConnectionlessPacket();
			Com_DPrintf("Connectionless packet\n");
			continue;
		}

		if (!cls.mvdplayback && (net_message.cursize < 8)) 
		{
			Com_DPrintf("%s: Runt packet\n", NET_AdrToString(net_from));
			continue;
		}

		// Packet from server.
		if (!cls.demoplayback && !NET_CompareAdr(net_from, cls.netchan.remote_address)) 
		{
			Com_DPrintf("%s: sequenced packet without connection\n", NET_AdrToString(net_from));
			continue;
		}

		if (cls.mvdplayback) 
		{
			MSG_BeginReading();
		}
		else 
		{
			if (!Netchan_Process(&cls.netchan))
				continue; // Wasn't accepted for some reason.
		}

		CL_ParseServerMessage();
	}

	// Check timeout.
	if (!cls.demoplayback && cls.state >= ca_connected ) 
	{
		if (curtime - cls.netchan.last_received > (cl_timeout.value > 0 ? cl_timeout.value : 60)) 
		{
			Com_Printf("\nServer connection timed out.\n");
			Host_EndGame();
			return;
		}
	}
}

void CL_SendToServer (void) 
{
	// When recording demos, request new ping times every cl_demoPingInterval.value seconds.
	if (cls.demorecording && !cls.demoplayback && cls.state == ca_active && cl_demoPingInterval.value > 0) 
	{
		if (cls.realtime - cl.last_ping_request > cl_demoPingInterval.value) 
		{
			cl.last_ping_request = cls.realtime;
			MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
			SZ_Print (&cls.netchan.message, "pings");
		}
	}

	// Send intentions now. Resend a connection request if necessary.
	if (cls.state == ca_disconnected)
		CL_CheckForResend ();
	else
		CL_SendCmd ();
}

void CL_OnChange_name_validate(cvar_t *var, char *val, qbool *cancel)
{
	char *clrpart;

	// check for &r
	if (strstr(val, "&r")) {
		*cancel = true;
		Com_Printf("Using color codes in your name is not allowed\n");
		return;
	}
	
	// check for &cRGB
	// RGB has to be a valid hexadecimal number, otherwise it's ok
	clrpart = val;
	do {
		clrpart = strstr(clrpart, "&c");
		if (clrpart) {
			clrpart += 2;
			if (clrpart[0] && clrpart[1] && clrpart[2]) {
				if (HexToInt(clrpart[0]) >= 0 && HexToInt(clrpart[1]) >= 0 && HexToInt(clrpart[2]) >= 0) {
					*cancel = true;
					Com_Printf("Using color codes in your name is not allowed\n");
				}
			}
		}
	} while (clrpart);
}
//=============================================================================

void CL_InitCommands (void);

void CL_Fog_f (void) 
{
	extern cvar_t gl_fogred, gl_foggreen, gl_fogblue, gl_fogenable;
	
	if (Cmd_Argc() == 1) 
	{
		Com_Printf ("\"fog\" is \"%f %f %f\"\n", gl_fogred.value, gl_foggreen.value, gl_fogblue.value);
		return;
	}
	Cvar_SetValue (&gl_fogenable, 1);
	Cvar_SetValue (&gl_fogred, atof(Cmd_Argv(1)));
	Cvar_SetValue (&gl_foggreen, atof(Cmd_Argv(2)));
	Cvar_SetValue (&gl_fogblue, atof(Cmd_Argv(3)));
}

void CL_InitLocal (void) 
{
	extern cvar_t baseskin, noskins, cl_name_as_skin, enemyforceskins, teamforceskins;
	char st[256];

	extern void Cl_Messages_Init(void);

	Cl_Messages_Init();

	Cvar_SetCurrentGroup(CVAR_GROUP_CHAT);
	Cvar_Register (&cl_parseWhiteText);
	Cvar_Register (&cl_chatsound);
	Cmd_AddLegacyCommand ("cl_chatsound", "s_chat_custom");
	Cvar_Register (&cl_fakename);
	Cvar_Register (&cl_fakename_suffix);

	Cvar_Register (&cl_restrictions);

	Cvar_Register (&cl_floodprot);
	Cvar_Register (&cl_fp_messages);
	Cvar_Register (&cl_fp_persecond);

	Cvar_Register (&msg_filter);

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register (&cl_shownet);
	Cvar_Register (&show_fps2);
	Cmd_AddLegacyCommand ("draw_fps", "scr_scoreboard_drawfps");
	Cvar_Register (&cl_confirmquit);
	Cvar_Register (&cl_window_caption);
	Cvar_Register (&cl_onload);

	#ifdef WIN32
	Cvar_Register (&cl_verify_qwprotocol);
	#endif // WIN32

	Cvar_SetCurrentGroup(CVAR_GROUP_SBAR);
	Cvar_Register (&cl_sbar);
	Cvar_Register (&cl_hudswap);

	Cvar_SetCurrentGroup(CVAR_GROUP_VIEWMODEL);
	Cvar_Register (&cl_filterdrawviewmodel);

	Cvar_SetCurrentGroup(CVAR_GROUP_EYECANDY);
	Cvar_Register (&cl_model_bobbing);
	Cvar_Register (&cl_nolerp);
	Cvar_Register (&cl_nolerp_on_entity);
	Cvar_Register (&cl_newlerp);
	Cvar_Register (&cl_lerp_monsters);
	Cvar_Register (&cl_maxfps);
	Cvar_Register (&cl_physfps);
	Cvar_Register (&hud_fps_min_reset_interval);
	Cvar_Register (&cl_physfps_spectator);
	Cvar_Register (&cl_independentPhysics);
	Cvar_Register (&cl_deadbodyfilter);
	Cvar_Register (&cl_gibfilter);
	Cvar_Register (&cl_backpackfilter);
	Cvar_Register (&cl_muzzleflash);
	Cvar_Register (&cl_rocket2grenade);
	Cvar_Register (&r_explosiontype);
	Cvar_Register (&r_lightflicker);
	Cvar_Register (&r_rockettrail);
	Cvar_Register (&r_grenadetrail);
	Cvar_Register (&r_railtrail);
	Cvar_Register (&r_instagibtrail);
	Cvar_Register (&r_powerupglow);
	Cvar_Register (&cl_novweps);
	Cvar_Register (&r_drawvweps);
	Cvar_Register (&r_rocketlight);
	Cvar_Register (&r_explosionlight);
	Cvar_Register (&r_rocketlightcolor);
	Cvar_Register (&r_explosionlightcolor);
	Cvar_Register (&r_flagcolor);
	Cvar_Register (&cl_fakeshaft);
	Cvar_Register (&cl_fakeshaft_extra_updates);
	Cmd_AddLegacyCommand ("cl_truelightning", "cl_fakeshaft");
	Cvar_Register (&r_telesplash);
	Cvar_Register (&r_shaftalpha);

	Cvar_SetCurrentGroup(CVAR_GROUP_SKIN);
	Cvar_Register (&noskins);
	Cvar_Register (&baseskin);
	Cvar_Register (&cl_name_as_skin);
	Cvar_Register (&enemyforceskins);
	Cvar_Register (&teamforceskins);

	Cvar_SetCurrentGroup(CVAR_GROUP_DEMO);
	Cvar_Register (&cl_demospeed);
	Cvar_Register (&cl_demoPingInterval);
	Cvar_Register (&qizmo_dir);
	Cvar_Register (&qwdtools_dir);
	Cvar_Register (&demo_getpings);
	Cvar_Register (&demo_autotrack);
	Cvar_Register (&cl_demoteamplay);

	Cvar_SetCurrentGroup(CVAR_GROUP_SOUND);
	Cvar_Register (&cl_staticsounds);

	Cvar_SetCurrentGroup(CVAR_GROUP_USERINFO);
	Cvar_Register (&team);
	Cvar_Register (&spectator);
	Cvar_Register (&skin);
	Cvar_Register (&rate);
	Cvar_Register (&mtu);
	Cvar_Register (&name);
	Cvar_Register (&msg);
	Cvar_Register (&noaim);
	Cvar_Register (&topcolor);
	Cvar_Register (&bottomcolor);
	Cvar_Register (&w_switch);
	Cvar_Register (&b_switch);
	Cvar_Register (&railcolor);
	Cvar_Register (&gender);

	Cvar_SetCurrentGroup(CVAR_GROUP_NETWORK);
	Cvar_Register (&cl_predict_players);
	Cvar_Register (&cl_solid_players);
	Cvar_Register (&cl_predict_half);
	Cvar_Register (&cl_timeout);
	Cvar_Register (&cl_useproxy);
	Cvar_Register (&cl_proxyaddr);
	Cvar_Register (&cl_crypt_rcon);
	Cvar_Register (&cl_fix_mvd);

	Cvar_Register (&cl_delay_packet);
	Cvar_Register (&cl_earlypackets);

#if defined(PROTOCOL_VERSION_FTE) || defined(PROTOCOL_VERSION_FTE2)
	Cvar_Register (&cl_pext);
	Cvar_Register (&cl_pext_other);
#endif // PROTOCOL_VERSION_FTE
#ifdef FTE_PEXT_256PACKETENTITIES
	Cvar_Register (&cl_pext_256packetentities);
#endif
#ifdef FTE_PEXT_CHUNKEDDOWNLOADS
	Cvar_Register (&cl_pext_chunkeddownloads);
	Cvar_Register (&cl_chunksperframe);
#endif

#ifdef FTE_PEXT_FLOATCOORDS
	Cvar_Register (&cl_pext_floatcoords);
#endif

#ifdef FTE_PEXT_TRANS
	Cvar_Register(&cl_pext_alpha);
#endif

	Cvar_SetCurrentGroup(CVAR_GROUP_INPUT_KEYBOARD);
	Cvar_Register (&allow_scripts);

	Cvar_SetCurrentGroup(CVAR_GROUP_SYSTEM_SETTINGS);
	Cvar_Register (&cl_mediaroot);

	Cvar_SetCurrentGroup(CVAR_GROUP_NO_GROUP);
	Cvar_Register (&password);
	Cvar_Register (&rcon_password);
	Cvar_Register (&rcon_address);
	Cvar_Register (&localid);
	Cvar_Register (&cl_warncmd);
	Cvar_Register (&cl_cmdline);
	Cvar_ForceSet (&cl_cmdline, com_args_original);
	Cvar_ResetCurrentGroup();

	snprintf(st, sizeof(st), "ezQuake %i", REVISION);

	if (COM_CheckParm ("-norjscripts") || COM_CheckParm ("-noscripts"))
		Cvar_SetValue (&allow_scripts, 0);

 	Info_SetValueForStarKey (cls.userinfo, "*client", st, MAX_INFO_STRING);

	if (COM_CheckParm("-noindphys")) 
	{
		Cvar_SetValue(&cl_independentPhysics, 0);
		Cvar_SetValue(&cl_nolerp, 1);
	}

	Cmd_AddLegacyCommand ("demotimescale", "cl_demospeed");

	CL_InitCommands ();

	Cmd_AddCommand ("disconnect", CL_Disconnect_f);
	Cmd_AddCommand ("connect", CL_Connect_f);
	Cmd_AddCommand ("connectbr", CL_Connect_BestRoute_f);

	Cmd_AddCommand ("qwurl", CL_QWURL_f);

	Cmd_AddCommand ("tcpconnect", CL_TCPConnect_f);

	Cmd_AddCommand ("join", CL_Join_f);
	Cmd_AddCommand ("observe", CL_Observe_f);
	Cmd_AddCommand ("togglespec", Cl_ToggleSpec_f);

	Cmd_AddCommand ("hud_fps_min_reset", Cl_Reset_Min_fps_f);

	#ifdef WIN32
	Cmd_AddCommand ("register_qwurl_protocol", CL_RegisterQWURLProtocol_f);
	#endif // WIN32

	Cmd_AddCommand ("dns", CL_DNS_f);
	Cmd_AddCommand ("hash", CL_Hash_f);
	Cmd_AddCommand ("reconnect", CL_Reconnect_f);

	Cmd_AddMacro("connectiontype", CL_Macro_ConnectionType);
	Cmd_AddMacro("demoplayback", CL_Macro_Demoplayback);
	Cmd_AddMacro("demotime", CL_Macro_Demotime);
	Cmd_AddMacro("rand", CL_Macro_Rand);
	Cmd_AddMacro("matchstatus", CL_Macro_Serverstatus);
	Cmd_AddMacro("serverip", CL_Macro_ServerIp);
	Cmd_AddMacro("conwidth", CL_Macro_Conwidth);
	Cmd_AddMacro("conheight", CL_Macro_Conheight);
}

void GFX_Init (void) 
{
	Draw_Init();
	SCR_Init();
	R_Init();
	Sbar_Init();
	HUD_Editor_Init();	// Need to reload some textures.
}

void ReloadPaletteAndColormap(void)
{
	int filesize;

	// free

	Q_free(host_basepal);
	Q_free(host_colormap);

	// load

	host_basepal = (byte *) FS_LoadHeapFile ("gfx/palette.lmp", &filesize);
	if (!host_basepal)
		Sys_Error ("Couldn't load gfx/palette.lmp");
	FMod_CheckModel("gfx/palette.lmp", host_basepal, filesize);

	host_colormap = (byte *) FS_LoadHeapFile ("gfx/colormap.lmp", &filesize);
	if (!host_colormap)
		Sys_Error ("Couldn't load gfx/colormap.lmp");
	FMod_CheckModel("gfx/colormap.lmp", host_colormap, filesize);
}

void EX_FileList_Init(void);

void CL_Init (void) 
{
	// When ezquake was launched via a webpage (qtv) the working directory wasn't properly
	// set. Changing the directory makes sure it starts out in the directory where ezquake 
	// is located.
	Sys_chdir(com_basedir);

	cls.state = ca_disconnected;
	cls.min_fps = 999999;

	SZ_Init(&cls.cmdmsg, cls.cmdmsg_data, sizeof(cls.cmdmsg_data));
	cls.cmdmsg.allowoverflow = true;

	strlcpy (cls.gamedirfile, com_gamedirfile, sizeof (cls.gamedirfile));
	strlcpy (cls.gamedir, com_gamedir, sizeof (cls.gamedir));

	FChecks_Init();

	ReloadPaletteAndColormap();

	Sys_mkdir(va("%s/qw", com_basedir));
	Sys_mkdir(va("%s/ezquake", com_basedir));

	History_Init();
	V_Init ();
	MVD_Utils_Init ();

	VID_Init(host_basepal);
	IN_Init();

	Image_Init();

	GFX_Init ();

#if defined(FRAMEBUFFERS)
	Framebuffer_Init();
#endif

	S_Init ();

	CDAudio_Init ();

	CL_InitLocal ();
	CL_FixupModelNames ();
	CL_InitInput ();
	CL_InitEnts ();
	CL_InitTEnts ();
	CL_InitTEntsCvar();
	CL_InitPrediction ();
	CL_InitCam ();
	TP_Init ();
	Hud_262Init();
	HUD_Init();
	Help_Init();
	M_Init ();
	EX_FileList_Init();

	SList_Init ();
	SList_Load ();

	MT_Init();
	CL_Demo_Init();
	Ignore_Init();
	Log_Init();
	Movie_Init();

#ifdef _DEBUG
	if (Expr_Run_Unit_Tests() != 0) {
		Sys_Error("One of the expression parser unit tests failed");
	}
#endif

	// moved to host.c:Host_Init()
	//ConfigManager_Init();
	Stats_Init();
	MP3_Init();
	SB_RootInit();
#ifdef WITH_IRC	
	IRC_Init();
#endif

	QTV_Init();

	Sys_InitIPC();

	Rulesets_Init();
}

//============================================================================

void CL_BeginLocalConnection (void) 
{
	S_StopAllSounds();

	// make sure we're not connected to an external server,
	// and demo playback is stopped
	if (!com_serveractive)
		CL_Disconnect ();

	cl.worldmodel = NULL;

	if (cls.state == ca_active)
		cls.state = ca_connected;
}

// automatically pause the game when going into the menus in single player
static void CL_CheckAutoPause (void) 
{
	#ifndef CLIENTONLY

	extern cvar_t maxclients;

	if (com_serveractive && cls.state == ca_active && !cl.deathmatch && maxclients.value == 1
		&& (key_dest == key_menu /*|| key_dest == key_console*/))
	{
		if (!(sv.paused & 2))
			SV_TogglePause (NULL, 2);
	}
	else 
	{
		if (sv.paused & 2)
			SV_TogglePause (NULL, 2);
	}
	#endif // CLIENTONLY
}


static double CL_MinFrameTime (void) 
{
	double fps, fpscap;

	if (cls.timedemo || Movie_IsCapturing())
		return 0;

	if (cls.state == ca_disconnected || (Minimized && !cls.download))
		return 1 / 30.0;

	if (cls.demoplayback)
	{
		if (!cl_maxfps.value)
			return 0;

		// Multiview.
		if (cl_multiview.value > 0 && cls.mvdplayback)
		{
			fps = max (30.0, cl_maxfps.value * nNumViews);
		}
		else
		{
			fps = max (30.0, cl_maxfps.value);
		}

	}
	else
	{
		if (cl_independentPhysics.value == 0)
		{
			fpscap = cl.maxfps ? max (30.0, cl.maxfps) : Rulesets_MaxFPS();
			fps = cl_maxfps.value ? bound (30.0, cl_maxfps.value, fpscap) : com_serveractive ? fpscap : bound (30.0, rate.value / 80.0, fpscap);
		}
		else
			fps = cl_maxfps.value ? max(cl_maxfps.value, 30) : 99999; //#fps:
	}

	return 1 / fps;
}

static double MinPhysFrameTime (void)
{
	// Server policy
	float fpscap = (cl.maxfps ? cl.maxfps : 72.0);
	// Use either cl_physfps_spectator or cl_physfps
	float physfps = ((cl.spectator && !cls.demoplayback) ? cl_physfps_spectator.value : cl_physfps.value);

	// this makes things smooth in mvd demo play back, since mvd interpolation applied each frame
	if (cls.demoplayback)
		return 0;

	// the user can lower it for testing (or really shit connection)
	if (physfps)
		fpscap = min(fpscap, physfps);

	// not less than this no matter what
	fpscap = max(fpscap, 10);

	return 1 / fpscap;
}

void CL_CalcFPS(void)
{
	double t;
	static double last_frame_time;
	static double time_of_last_minfps_update;

	t = Sys_DoubleTime();

	if ((t - last_frame_time) >= 1.0)
	{
		lastfps = (double)fps_count / (t - last_frame_time);
		fps_count = 0;
		last_frame_time = t;
	}

	cls.fps = lastfps;
	// update min_fps if last fps is less than our lowest accepted minfps (10.0) or greater than min_reset_interval
	if ((lastfps > 10.0 && lastfps < cls.min_fps) || ((t - time_of_last_minfps_update) > hud_fps_min_reset_interval.value)) { 
		cls.min_fps = lastfps;
		time_of_last_minfps_update = t;
	}
}

void Cl_Reset_Min_fps_f(void)
{
	cls.min_fps = 9999;
	CL_CalcFPS();
}

void CL_QTVPoll (void);
void SB_ExecuteQueuedTriggers (void);

qbool physframe;
double physframetime;

void CL_Frame (double time) 
{
	static double extratime = 0.001;
	double minframetime;
	static double	extraphysframetime;	//#fps

	extern double render_frame_start;

	extratime += time;
	minframetime = CL_MinFrameTime();
	next_spec_track = -1;

	if (extratime < minframetime) 
	{
		extern cvar_t sys_yieldcpu;
		if (sys_yieldcpu.integer || Minimized)
		{
			#ifdef _WIN32
			Sys_MSleep(0);
			#else
			usleep( (minframetime - extratime) * 1000 * 1000 );
			#endif
		}

		if (cl_delay_packet.integer)
		{
			CL_QueInputPacket();
			CL_UnqueOutputPacket(false);
		}

		return;
	}

	if (cl_delay_packet.integer)
	{
		CL_QueInputPacket();
		CL_UnqueOutputPacket(false);
	}

	if (VID_VSyncLagFix())
		return;

	render_frame_start = Sys_DoubleTime();

	cls.trueframetime = extratime - 0.001;
	cls.trueframetime = max(cls.trueframetime, minframetime);
	extratime -= cls.trueframetime;

	if (Movie_IsCapturing())
		cls.frametime = Movie_StartFrame();
	else
		cls.frametime = min(0.2, cls.trueframetime);
	
	if (cl_independentPhysics.value != 0)
	{
		double minphysframetime = MinPhysFrameTime();

		extraphysframetime += cls.frametime;
		if (extraphysframetime < minphysframetime)
			physframe = false;
		else
		{
			physframe = true;

			if (extraphysframetime > minphysframetime*2)// FIXME: this is for the case when
				physframetime = extraphysframetime;		// actual fps is too low
			else										// Dunno how to do it right
				physframetime = minphysframetime;
			extraphysframetime -= physframetime;
		}
	} 
	else 
	{
		// this vars SHOULD NOT be used in case of cl_independentPhysics == 0, so we just reset it for sanity
		physframetime = extraphysframetime = 0;
		// this var actually used
		physframe = true;
	}

	if (cls.demoplayback) 
	{
		Demo_AdjustSpeed();

		if (cl.paused & PAUSED_DEMO)
			cls.frametime = 0;
		else if (!cls.timedemo)
			cls.frametime *= Demo_GetSpeed();

		if (!host_skipframe)
			cls.demotime += cls.frametime;
		host_skipframe = false;
	}

	cls.realtime += cls.frametime;

	if (!ISPAUSED) 
	{
		cl.time += cls.frametime;
		cl.servertime += cls.frametime;
		cl.stats[STAT_TIME] = (int) (cl.servertime * 1000);
		if (cls.demoplayback)
			cl.gametime += cls.frametime;
		else
			cl.gametime = Sys_DoubleTime() - cl.gamestarttime - cl.gamepausetime;
	}
	else
	{
		// We hope here, that pause doesn't take long so we don't get too much de-synced
		// if pause takes too much, we can get into usual clock sync-problems as we did before
		cl.gamepausetime += cls.frametime;
	}

	r_refdef2.time = cl.time;

	// get new key events
	if (cl_independentPhysics.value == 0)
	{
		Sys_SendKeyEvents();

		// allow mice or other external controllers to add commands
		IN_Commands();

		// process console commands
		Cbuf_Execute();
		CL_CheckAutoPause();

//		if (com_serveractive)
			SV_Frame(cls.frametime);

		// fetch results from server
		CL_ReadPackets();

		TP_UpdateSkins();

		if (cls.mvdplayback && !cls.demoseeking)
		{
			MVD_Interpolate();
			MVD_Mainhook();

			if (!cl.standby && physframe)
			{
				StatsGrid_Gather();
			}
		}

		// process stuffed commands
		Cbuf_ExecuteEx(&cbuf_svc);

		CL_SendToServer();

		// We need to move the mouse also when disconnected
		// to get the cursor working properly.
		if(cls.state == ca_disconnected)
		{
			usercmd_t dummy;
			IN_Move(&dummy);
		}
	}
	else 
	{
		if (physframe)
		{
			Sys_SendKeyEvents();

			// allow mice or other external controllers to add commands
			IN_Commands();

			// process console commands
			Cbuf_Execute();
			CL_CheckAutoPause ();

//			if (com_serveractive)
				SV_Frame (physframetime);

			// Fetch results from server
			CL_ReadPackets();

			TP_UpdateSkins();

			// Gather MVD stats and interpolate.
			if (cls.mvdplayback && !cls.demoseeking)
			{
				MVD_Interpolate();
				MVD_Mainhook();

				if (!cl.standby && physframe)
				{
					StatsGrid_Gather();
				}
			}

			// process stuffed commands
			Cbuf_ExecuteEx(&cbuf_svc);

			CL_SendToServer();

			if (cls.state == ca_disconnected) // We need to move the mouse also when disconnected
			{
				usercmd_t dummy;
				IN_Move(&dummy);
			}
		}
		else
		{
			if (!cls.demoplayback && cl_earlypackets.integer)
			{
				CL_ReadPackets(); // read packets ASAP
			}

			if (   (!cls.demoplayback && !cl.spectator) // not demo playback and not a spec
				|| (!cls.demoplayback && cl.spectator && Cam_TrackNum() == -1) // not demo, spec free fly
				|| ( cls.demoplayback && cls.mvdplayback && Cam_TrackNum() == -1) // mvd demo and free fly
				|| cls.state == ca_disconnected // We need to move the mouse also when disconnected
				) 
			{
				usercmd_t dummy;
				Sys_SendKeyEvents();
				IN_Move(&dummy);
			}
		}
	}

	{ // chat icons
		char char_flags[64] = {0};
		int cif_flags = 0;

		if (key_dest != key_game) // add chat flag if in console, menus, mm1, mm2 etc...
			cif_flags |= CIF_CHAT;

		// add AFK flag if app minimized, or not the focus
		// TODO: may be add afk flag on idle? if no user input in 45 seconds for example?
		if (!ActiveApp || Minimized)
			cif_flags |= CIF_AFK;

		if (cif_flags && cls.state >= ca_connected) // put key in userinfo only then we are connected, remove key if we not connected yet
			snprintf(char_flags, sizeof(char_flags), "%d", cif_flags);

		CL_UserinfoChanged ("chat", char_flags);
	}

	if (cls.state >= ca_onserver)
	{
		qbool setup_player_prediction = ((physframe && cl_independentPhysics.value != 0) || cl_independentPhysics.value == 0);
		if (!cls.demoplayback && cl_earlypackets.integer)
		{
			// actually it should be curtime == cls.netchan.last_received but that did not work on float values...
			if (curtime - cls.netchan.last_received < 0.00001)
			{
				if (!setup_player_prediction)
				{
//					Com_DPrintf("force players prediction\n");
					setup_player_prediction = true;
				}
			}
		}
		Cam_SetViewPlayer();

		// Set up prediction for other players
		if (setup_player_prediction)
		{
			CL_SetUpPlayerPrediction(false);
		}

		// Do client side motion prediction
		CL_PredictMove();

		// Set up prediction for other players
		if (setup_player_prediction)
		{
			CL_SetUpPlayerPrediction(true);
		}

		// build a refresh entity list
		CL_EmitEntities();
	}

	if (!CL_MultiviewCvarResetRequired())
	{
		CL_MultiviewSaveValues ();
	}
	if (CL_MultiviewCvarResetRequired() && !cl_multiview.value)
	{
		CL_MultiviewRestoreValues ();
	}

	if (cl_multiview.value > 0 && cls.mvdplayback)
	{
		CL_Multiview();
	}

	// update video
	SCR_UpdateScreen();

	CL_DecayLights();

	// update audio
	if ((CURRVIEW == 2 && cl_multiview.value && cls.mvdplayback) || (!cls.mvdplayback || cl_multiview.value < 2))
	{
		if (cls.state == ca_active)
		{
			if (!ISPAUSED) {
				S_Update (r_origin, vpn, vright, vup);
			} else {
				// do not play loop sounds (lifts etc.) when paused
#ifndef INFINITY
				float temp = 1.0;
				float infinity = temp / (temp - 1.0);
				vec3_t hax = { infinity, infinity, infinity };
#else
				vec3_t hax = { INFINITY, INFINITY, INFINITY };
#endif
				S_Update(hax, vec3_origin, vec3_origin, vec3_origin);
			}
			CL_DecayLights ();
		}
		else
		{
			S_Update (vec3_origin, vec3_origin, vec3_origin, vec3_origin);
		}

		CDAudio_Update();
	}

	MT_Frame();

	if (Movie_IsCapturing())
	{
		Movie_FinishFrame();
	}

	cls.framecount++;

	fps_count++;

	CL_CalcFPS();

	VFS_TICK(); // VFS hook for updating some systems

	Sys_ReadIPC();

	CL_QTVPoll();
#ifdef WITH_IRC
	IRC_Update();
#endif

	SB_ExecuteQueuedTriggers();

	CL_UpdateCaption(false);

	// Multiview: advance to next player
	if (next_spec_track >= 0) {
		spec_track = next_spec_track;
	}
}

//============================================================================

void CL_Shutdown (void) 
{
	CL_Disconnect();
	SList_Shutdown();
	CDAudio_Shutdown();
	S_Shutdown();
	MP3_Shutdown();
	IN_Shutdown ();
	Log_Shutdown();
	if (host_basepal)
		VID_Shutdown();
	History_Shutdown();
	Sys_CloseIPC();
	SB_Shutdown();
	Help_Shutdown();
}

int CL_IncrLoop(int cview, int max)
{
	return (cview >= max) ? 1 : ++cview;
}

int CL_NextPlayer(int plr)
{
	do
	{
		plr++;
	} while ((plr < MAX_CLIENTS) && (cl.players[plr].spectator || !cl.players[plr].name[0]));

	if (plr >= MAX_CLIENTS)
	{
		plr = -1;

		do
		{
			plr++;
		} while ((plr < MAX_CLIENTS) && (cl.players[plr].spectator || !cl.players[plr].name[0]));
	}

	if (plr >= MAX_CLIENTS)
	{
		plr = 0;
	}

	return plr;
}

int CL_PrevPlayer(int plr)
{
	do
	{
		plr--;
	} while ((plr >= 0) && (cl.players[plr].spectator || !cl.players[plr].name[0]));

	if (plr < 0)
	{
		plr = MAX_CLIENTS;

		do
		{
			plr--;
		} while ((plr >= 0) && (cl.players[plr].spectator || !cl.players[plr].name[0]));
	}

	if (plr < 0)
	{
		plr = 0;
	}

	return plr;
}

// Multiview vars
// ===================================================================================
int		CURRVIEW;					// The current view being drawn in multiview mode.
int		nNumViews;					// The number of views in multiview mode.
qbool	bExitmultiview;				// Used when saving effect values on each frame.

qbool	mv_skinsforced;				// When using teamcolor/enemycolor in multiview we can't just assume
									// that the "teammates" should all be colored in the same color as the
									// person we're tracking (or opposite for enemies), because we're tracking
									// more than one person. Therefore the teamcolor/enemycolor is set only once,
									// or when the player chooses to track an entire team.
int		nPlayernum;

int		mv_trackslots[4];			// The different track slots for each view.
char	currteam[MAX_INFO_STRING];  // The name of the current team being tracked in multiview mode.
int		nSwapPov;					// Change in POV positive for next, negative for previous.
int		nTrack1duel;				// When cl_multiview = 2 and mvinset is on this is the tracking slot for the main view.
int		nTrack2duel;				// When cl_multiview = 2 and mvinset is on this is the tracking slot for the mvinset view.

static void CL_Multiview (void)
{
	static int playernum = 0;

	if (!cls.mvdplayback) {
		return;
	}

	nNumViews = cl_multiview.value;

	// Only refresh skins once (to start with), I think this is the best solution for multiview
	// eg when viewing 4 players in a 2v2.
	// Also refresh them when the player changes what team to track.
	if ((!CURRVIEW && cls.state >= ca_connected) || nSwapPov) {
		TP_RefreshSkins ();
	}

	CL_MultiviewOverrideValues ();

	nPlayernum = playernum;

	// Copy the stats for the player we're about to draw in the next
	// view to the client state, so that the correct stats are drawn
	// in the multiview mini-HUD.
	memcpy(cl.stats, cl.players[playernum].stats, sizeof(cl.stats));

	//
	// Increase the current view being rendered.
	//
	CURRVIEW = CL_IncrLoop(CURRVIEW, cl_multiview.integer);

	if (cl_mvinset.value && cl_multiview.value == 2)
	{
		//
		// Special case for mvinset and tracking 2 people
		// this is meant for spectating duels primarily.
		// Lets the user swap which player is shown in the
		// main view and the mvinset by pressing jump.
		//

		// If both the mvinset and main view is set to show
		// the same player, pick the first player for the main view
		// and the next after that for the mvinset.
		if (nTrack1duel == nTrack2duel)
		{
			nTrack1duel = CL_NextPlayer(-1);
			nTrack2duel = CL_NextPlayer(nTrack1duel);
		}

		// The user pressed jump so we need to swap the pov.
		if (nSwapPov)
		{
			if (nSwapPov > 0) {
				nTrack1duel = CL_NextPlayer(nTrack1duel);
				nTrack2duel = CL_NextPlayer(nTrack2duel);
			} else {
				nTrack1duel = CL_PrevPlayer(nTrack1duel);
				nTrack2duel = CL_PrevPlayer(nTrack2duel);
			}
			nSwapPov = false;
		}
		else
		{
			// Set the playernum based on if we're drawing the mvinset
			// or the main view
			// (nTrack1duel = main view)
			// (nTrack2duel = mvinset)
			playernum = (CURRVIEW == 1) ? nTrack1duel : nTrack2duel;
		}
	}
	else
	{
		//
		// Normal multiview.
		//

		// Start from the first player on each new frame.
		playernum = ((CURRVIEW == 1) ? 0 : playernum);

		//
		// The player pressed jump and wants to change what team is spectated.
		//
		if (nSwapPov && cl_multiview.value >= 2 && cl.teamplay)
		{
			int j;
			int team_slot_count = 0;
			int last_mv_trackslots[4];

			// Save the old track values and reset them.
			memcpy(last_mv_trackslots, mv_trackslots, sizeof(last_mv_trackslots));
			memset(mv_trackslots, -1, sizeof(mv_trackslots));

			// Find the new team.
			for(j = 0; j < MAX_CLIENTS; j++)
			{
				if(!cl.players[j].spectator && cl.players[j].name[0])
				{
					// Find the opposite team from the one we are tracking now.
					if(!currteam[0] || strcmp(currteam, cl.players[j].team))
					{
						strlcpy(currteam, cl.players[j].team, sizeof(currteam));
						break;
					}
				}
			}

			// Find the team members.
			for(j = 0; j < MAX_CLIENTS; j++)
			{
				if(!cl.players[j].spectator
					&& cl.players[j].name[0]
					&& !strcmp(currteam, cl.players[j].team))
				{
					// Find the player slot to track.
					mv_trackslots[team_slot_count] = Player_StringtoSlot(cl.players[j].name);
					team_slot_count++;
				}

				Com_DPrintf("New trackslots: %i %i %i %i\n", mv_trackslots[0], mv_trackslots[1], mv_trackslots[2], mv_trackslots[3]);

				// Don't go out of bounds in the mv_trackslots array.
				if(team_slot_count == 4)
				{
					break;
				}
			}

			if(cl_multiview.value == 2 && team_slot_count == 2)
			{
				// Switch between 2on2 teams.
			}
			else if(cl_multiview.value < team_slot_count || team_slot_count >= 3)
			{
				// We don't want to show all from one team and then one of the enemies...
				cl_multiview.value = team_slot_count;
			}
			else if(team_slot_count == 2)
			{
				// 2on2... one team on top, on on the bottom
				// Swap the teams between the top and bottom in a 4 view setup.
				cl_multiview.value = 4;
				mv_trackslots[MV_VIEW3] = last_mv_trackslots[MV_VIEW1];
				mv_trackslots[MV_VIEW4] = last_mv_trackslots[MV_VIEW2];
				
				Com_DPrintf("Team on top/bottom trackslots: %i %i %i %i\n", 
					mv_trackslots[0], mv_trackslots[1], mv_trackslots[2], mv_trackslots[3]);
			}
		}
		else
		{
			// Check if the track* values have been set by the user,
			// otherwise show the first 4 players.

			if(CURRVIEW >= 1 && CURRVIEW <= 4)
			{
				// If the value of mv_trackslots[i] is negative, it means that view
				// doesn't have any track value set so we need to find someone to track using CL_NextPlayer().
				playernum = ((mv_trackslots[CURRVIEW - 1] < 0) ? CL_NextPlayer(playernum) : mv_trackslots[CURRVIEW - 1]);
			}
		}
	}

	// BUGFIX - Make sure the player we're tracking is still left, might have disconnected since
	// we picked him for tracking (This would result in being thrown into freefly mode and not being able
	// to go back into tracking mode when a player disconnected).
	if (!nSwapPov && (cl.players[playernum].spectator || !cl.players[playernum].name[0]))
	{
		mv_trackslots[CURRVIEW - 1] = -1;
	}

	if (nSwapPov)
	{
		Com_DPrintf("Final trackslots: %i %i %i %i\n", mv_trackslots[0], mv_trackslots[1], mv_trackslots[2], mv_trackslots[3]);
	}

	nSwapPov = false;

	// Set the current player we're tracking for the next view to be drawn.
	// BUGFIX: Only change the spec_track if the new track target is a player.
	if (!cl.players[playernum].spectator && cl.players[playernum].name[0])
	{
		next_spec_track = playernum;
	}

	// Make sure we reset variables we suppressed during multiview drawing.
	bExitmultiview = true;
}

typedef struct mv_temp_cvar_s {
	cvar_t* cvar;
	float   value;
} mv_temp_cvar_t;

extern cvar_t r_lerpframes;

static mv_temp_cvar_t multiviewCvars[] = {
	{ &scr_viewsize,      100.0f },
	{ &cl_fakeshaft,        0.0f },
	{ &gl_polyblend,        1.0f },
	{ &gl_clear,            0.0f },
	{ &r_lerpframes,        1.0f }
};
#define MV_CVAR_VIEWSIZE 0                 // we reference saved value when cl_mvinset specified

static void CL_MultiviewSaveValues (void)
{
	int i = 0;

	for (i = 0; i < sizeof (multiviewCvars) / sizeof (multiviewCvars[0]); ++i) {
		multiviewCvars[i].value = multiviewCvars[i].cvar->value;
	}

	CURRVIEW = 0;
}

static void CL_MultiviewRestoreValues (void)
{
	int i = 0;

	for (i = 0; i < sizeof (multiviewCvars) / sizeof (multiviewCvars[0]); ++i) {
		Cvar_SetValue(multiviewCvars[i].cvar, multiviewCvars[i].value);
	}

	bExitmultiview = false;
}

static void CL_MultiviewOverrideValues (void)
{
	// contrast was disabled for OpenGL build with the note "blanks all but 1 view"
	// this was due to gl_ztrick in R_Clear(void) that would clear these. FIXED
	// v_contrast.value = 1;

	// stop fakeshaft as it lerps with the other views
	if (cl_fakeshaft.value < 1 && cl_fakeshaft.value > 0) {
		cl_fakeshaft.value = 0;
	}

	// allow mvinset 1 to use viewsize value
	scr_viewsize.value = multiviewCvars[MV_CVAR_VIEWSIZE].value;
	if ((!cl_mvinset.value && cl_multiview.value == 2) || cl_multiview.value != 2) {
		scr_viewsize.value = 120;
	}

	// stop small screens
	if (cl_mvinset.value && cl_multiview.value == 2 && scr_viewsize.value < 100) {
		scr_viewsize.value = 100;
	}

	gl_polyblend.value = 0;
	gl_clear.value = 0;

	// stop weapon model lerping as it lerps with the other view
	r_lerpframes.value = 0;
}

static qbool CL_MultiviewCvarResetRequired (void)
{
	return bExitmultiview;
}

void CL_UpdateCaption(qbool force)
{
	static char caption[512] = {0};
	char str[512] = {0};

	if (!cl_window_caption.value)
	{
		if (!cls.demoplayback && (cls.state == ca_active))
			snprintf(str, sizeof(str), "ezQuake: %s", cls.servername);
		else
			snprintf(str, sizeof(str), "ezQuake");
	}
	else
	{
		snprintf(str, sizeof(str), "%s - %s", CL_Macro_Serverstatus(), MT_ShortStatus());
	}

	if (force || strcmp(str, caption))
	{
		VID_SetCaption(str);
		strlcpy(caption, str, sizeof(caption));
	}
}

void OnChangeDemoTeamplay (cvar_t *var, char *value, qbool *cancel)
{
	if (cls.nqdemoplayback) 
	{
		cl.teamplay = (*value != '0');

		NQD_SetSpectatorFlags();

		OnChangeColorForcing(var, value, cancel);
	}
}

// Can be used during rendering to find out what the next spec track will be
//   (especially useful when CURRVIEW == 1 after CL_Multiview() to report who tracked in main view)
int CL_MultiviewNextPlayer (void)
{
	if (next_spec_track >= 0) {
		return next_spec_track;
	}
	return spec_track;
}
