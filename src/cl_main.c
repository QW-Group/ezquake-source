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
#include "tr_types.h"
#include "teamplay.h"
#include "tp_triggers.h"
#include "rulesets.h"
#include "version.h"
#include "stats_grid.h"
#include "fmod.h"
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
#include "pmove.h"
#include "vx_tracker.h"
#include "menu_demo.h"
#include "r_local.h"
#include "r_renderer.h"
#include "r_performance.h"
#include "r_program.h"

extern qbool ActiveApp, Minimized;

#ifndef CLIENTONLY
static void Dev_PhysicsNormalSet(void);
static void Dev_PhysicsNormalSave(void);
static void Dev_PhysicsNormalShow(void);
#endif

static void Cl_Reset_Min_fps_f(void);
void CL_QWURL_ProcessChallenge(const char *parameters);

// cl_input.c
void onchange_pext_serversideweapon(cvar_t* var, char* value, qbool* cancel);
void onchange_hud_performance_average(cvar_t* var, char* value, qbool* cancel);

#ifdef MVD_PEXT1_HIDDEN_MESSAGES
// cl_parse.c
void CL_ParseHiddenDataMessage(void);
#endif

static void AuthUsernameChanged(cvar_t* var, char* value, qbool* cancel);

cvar_t	allow_scripts = {"allow_scripts", "2", 0, Rulesets_OnChange_allow_scripts};
cvar_t	rcon_password = {"rcon_password", ""};
cvar_t	rcon_address = {"rcon_address", ""};
cvar_t	cl_crypt_rcon = {"cl_crypt_rcon", "1"};

cvar_t	cl_timeout = {"cl_timeout", "60"};

cvar_t	cl_delay_packet = {"cl_delay_packet", "0", 0, Rulesets_OnChange_cl_delay_packet};
cvar_t  cl_delay_packet_target = { "cl_delay_packet_target", "0", 0, Rulesets_OnChange_cl_delay_packet };
cvar_t  cl_delay_packet_dev = { "cl_delay_packet_deviation", "0", 0, Rulesets_OnChange_cl_delay_packet };

cvar_t	cl_shownet = {"cl_shownet", "0"};	// can be 0, 1, or 2
#if defined(PROTOCOL_VERSION_FTE) || defined(PROTOCOL_VERSION_FTE2) || defined(PROTOCOL_VERSION_MVD1)
cvar_t  cl_pext = {"cl_pext", "1"};					// allow/disallow protocol extensions at all.
													// some extensions can be explicitly controlled.
cvar_t  cl_pext_limits = { "cl_pext_limits", "1" }; // enhanced protocol limits
cvar_t  cl_pext_other = {"cl_pext_other", "0"};		// extensions which does not have own variables should be controlled by this variable.
cvar_t  cl_pext_warndemos = { "cl_pext_warndemos", "1" }; // if set, user will be warned when saving demos that are not backwards compatible
cvar_t  cl_pext_lagteleport = { "cl_pext_lagteleport", "0" }; // server-side adjustment of yaw angle through teleports
#ifdef MVD_PEXT1_SERVERSIDEWEAPON
cvar_t  cl_pext_serversideweapon = { "cl_pext_serversideweapon", "0", 0, onchange_pext_serversideweapon }; // server-side weapon selection
#endif
#endif
#ifdef FTE_PEXT_256PACKETENTITIES
cvar_t	cl_pext_256packetentities = {"cl_pext_256packetentities", "1"};
#endif
#ifdef FTE_PEXT_CHUNKEDDOWNLOADS
cvar_t  cl_pext_chunkeddownloads  = {"cl_pext_chunkeddownloads", "1"};
cvar_t  cl_chunksperframe  = {"cl_chunksperframe", "30"};
#endif

#ifdef FTE_PEXT_FLOATCOORDS
cvar_t  cl_pext_floatcoords  = {"cl_pext_floatcoords", "1"};
#endif

#ifdef FTE_PEXT_TRANS
cvar_t cl_pext_alpha = {"cl_pext_alpha", "1"};
#endif

#ifdef CLIENTONLY
#define PROCESS_SERVERPACKETS_IMMEDIATELY (0)
#else
static cvar_t cl_sv_packetsync = { "cl_sv_packetsync", "1" };
#define PROCESS_SERVERPACKETS_IMMEDIATELY (cl_sv_packetsync.integer)
#endif


cvar_t	cl_sbar		= {"cl_sbar", "0"};
cvar_t	cl_hudswap	= {"cl_hudswap", "0"};
cvar_t	cl_maxfps	= {"cl_maxfps", "0"};
cvar_t	cl_maxfps_menu	= {"cl_maxfps_menu", "0"};
cvar_t	cl_physfps	= {"cl_physfps", "0"};	//#fps
cvar_t	cl_physfps_spectator = {"cl_physfps_spectator", "77"};
cvar_t  cl_independentPhysics = {"cl_independentPhysics", "1", 0, Rulesets_OnChange_indphys};

cvar_t	cl_predict_players = {"cl_predict_players", "1"};
cvar_t	cl_solid_players = {"cl_solid_players", "1"};
cvar_t	cl_predict_half = {"cl_predict_half", "0"};

cvar_t	hud_fps_min_reset_interval = {"hud_fps_min_reset_interval", "30"};
cvar_t  hud_frametime_max_reset_interval = { "hud_frametime_max_reset_interval", "30" };
cvar_t  hud_performance_average = { "hud_performance_average", "1", 0, onchange_hud_performance_average };

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

cvar_t r_rocketlight            = {"r_rocketLight", "1"};
cvar_t r_rocketlightcolor       = {"r_rocketLightColor", "0"};
cvar_t r_explosionlightcolor    = {"r_explosionLightColor", "0"};
cvar_t r_explosionlight         = {"r_explosionLight", "1"};
cvar_t r_flagcolor              = {"r_flagColor", "0"};
cvar_t r_lightflicker           = {"r_lightflicker", "1"};
cvar_t r_powerupglow            = {"r_powerupGlow", "1"};
cvar_t cl_novweps               = {"cl_novweps", "0"};
cvar_t r_drawvweps              = {"r_drawvweps", "1"};
cvar_t r_rockettrail            = {"r_rocketTrail", "1"}; // 9
cvar_t r_grenadetrail           = {"r_grenadeTrail", "1"}; // 3
cvar_t r_railtrail              = {"r_railTrail", "1"};
cvar_t r_instagibtrail          = {"r_instagibTrail", "1"};
cvar_t r_explosiontype          = {"r_explosionType", "1"}; // 7
cvar_t r_telesplash             = {"r_telesplash", "1"}; // disconnect
cvar_t r_shaftalpha             = {"r_shaftalpha", "1"};
cvar_t r_lightdecayrate         = {"r_lightdecayrate", "2"}; // default 2, as CL_DecayLights() used to get called twice per frame
cvar_t r_lightmap_lateupload    = {"r_lightmap_lateupload", "0"};
cvar_t r_lightmap_packbytexture = {"r_lightmap_packbytexture", "2"};

// info mirrors
cvar_t  password                = {"password", "", CVAR_USERINFO};
cvar_t  spectator               = {"spectator", "", CVAR_USERINFO_NO_CFG_RESET };
void CL_OnChange_name_validate(cvar_t *var, char *val, qbool *cancel);
cvar_t  name                    = {"name", "player", CVAR_USERINFO, CL_OnChange_name_validate};
cvar_t  team                    = {"team", "", CVAR_USERINFO_NO_CFG_RESET };
cvar_t  topcolor                = {"topcolor","", CVAR_USERINFO_NO_CFG_RESET };
cvar_t  bottomcolor             = {"bottomcolor","", CVAR_USERINFO_NO_CFG_RESET };
cvar_t  skin                    = {"skin", "", CVAR_USERINFO_NO_CFG_RESET };
cvar_t  rate                    = {"rate", "25000", CVAR_USERINFO};
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

// Authentication
cvar_t cl_username              = {"cl_username", "", CVAR_QUEUED_TRIGGER, AuthUsernameChanged};
static void CL_Authenticate_f(void);

// antilag debugging
cvar_t cl_debug_antilag_view    = { "cl_debug_antilag_view", "0" };
cvar_t cl_debug_antilag_ghost   = { "cl_debug_antilag_ghost", "0" };
cvar_t cl_debug_antilag_self    = { "cl_debug_antilag_self", "0" };
cvar_t cl_debug_antilag_lines   = { "cl_debug_antilag_lines", "0" };
cvar_t cl_debug_antilag_send    = { "cl_debug_antilag_send", "0" };

// weapon-switching debugging
cvar_t cl_debug_weapon_view     = { "cl_debug_weapon_view", "0" };

/// persistent client state
clientPersistent_t	cls;

/// client state
clientState_t		cl;

centity_t       cl_entities[CL_MAX_EDICTS];
entity_t		cl_static_entities[MAX_STATIC_ENTITIES];
lightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
dlight_t		cl_dlights[MAX_DLIGHTS];
unsigned int cl_dlight_active[MAX_DLIGHTS/32];

// refresh list
visentlist_t    cl_visents;

double		connect_time = 0;		// for connection retransmits
qbool		connected_via_proxy = false;

qbool	host_skipframe;			// used in demo playback

byte		*host_basepal = NULL;
byte		*host_colormap = NULL;

qbool physframe;
double physframetime;

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
#ifdef DEBUG_MEMORY_ALLOCATIONS
	Sys_Printf("\nevent,active (map=%s)\n", host_mapname.string);
#endif
	// last chance
	CachePics_AtlasFrame();
	// compile all programs
	if (GL_Supported(R_SUPPORT_RENDERING_SHADERS)) {
		R_ProgramCompileAll();
	}

	cls.state = ca_active;
	if (cls.demoplayback) 
	{
		host_skipframe = true;
		demostarttime = cls.demotime;
	}

	if (key_dest == key_startupdemo_game) {
		key_dest = key_game;
	}

	if (!cls.demoseeking) {
		Con_ClearNotify ();
	}
	TP_ExecTrigger("f_spawn");
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

	if (cl_pext_limits.value) {
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

	if (cl_pext_other.value)
	{
#ifdef FTE_PEXT_ACCURATETIMINGS
		fteprotextsupported |= FTE_PEXT_ACCURATETIMINGS;
#endif
#ifdef FTE_PEXT_HLBSP
		fteprotextsupported |= FTE_PEXT_HLBSP;
#endif
	}

	return fteprotextsupported;
}
#endif // PROTOCOL_VERSION_FTE

#ifdef PROTOCOL_VERSION_FTE2
unsigned int CL_SupportedFTEExtensions2 (void)
{
	unsigned int fteprotextsupported2 = 0
#ifdef FTE_PEXT2_VOICECHAT
		| FTE_PEXT2_VOICECHAT
#endif
		;

	if (!cl_pext.value)
		return 0;

	return fteprotextsupported2;
}
#endif // PROTOCOL_VERSION_FTE2

#ifdef PROTOCOL_VERSION_MVD1
unsigned int CL_SupportedMVDExtensions1(void)
{
	unsigned int extensions_supported = 0;

	if (!cl_pext.value) {
		return 0;
	}

#ifdef MVD_PEXT1_FLOATCOORDS
	if (cl_pext_floatcoords.value) {
		extensions_supported |= MVD_PEXT1_FLOATCOORDS;
	}
#endif

#ifdef MVD_PEXT1_HIGHLAGTELEPORT
	if (cl_pext_lagteleport.integer & 1) {
		extensions_supported |= MVD_PEXT1_HIGHLAGTELEPORT;
	}
#endif

#ifdef MVD_PEXT1_SERVERSIDEWEAPON
	if (cl_pext_serversideweapon.integer) {
		extensions_supported |= MVD_PEXT1_SERVERSIDEWEAPON;
	}
#endif

#ifdef MVD_PEXT1_DEBUG_ANTILAG
	if (cl_debug_antilag_send.integer) {
		extensions_supported |= MVD_PEXT1_DEBUG_ANTILAG;
	}
#endif

	return extensions_supported;
}
#endif

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
#ifdef PROTOCOL_VERSION_MVD1
	,
#endif
#endif // PROTOCOL_VERSION_FTE2
#ifdef PROTOCOL_VERSION_MVD1
	unsigned int mvdpext1
#endif
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
#ifdef PROTOCOL_VERSION_MVD1
	cls.mvdprotocolextensions1 = (mvdpext1 & CL_SupportedMVDExtensions1());
#endif

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

#ifdef PROTOCOL_VERSION_MVD1
	if (cls.mvdprotocolextensions1) {
		char tmp[128];
		snprintf(tmp, sizeof(tmp), "0x%x 0x%x\n", PROTOCOL_VERSION_MVD1, cls.mvdprotocolextensions1);
		Com_Printf_State(PRINT_DBG, "0x%x is mvd protocol ver and 0x%x is mvdprotocolextensions1\n", PROTOCOL_VERSION_MVD1, cls.mvdprotocolextensions1);
		strlcat(data, tmp, sizeof(data));
	}
#endif

	NET_SendPacket(NS_CLIENT, strlen(data), data, cls.server_adr);
}

// Resend a connect message if the last one has timed out
void CL_CheckForResend (void) 
{
	char data[2048];
	double t1, t2;

#ifndef CLIENTONLY
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
	#ifdef PROTOCOL_VERSION_MVD1
                ,
	#endif
#endif // PROTOCOL_VERSION_FTE
#ifdef PROTOCOL_VERSION_MVD1
                svs.mvdprotocolextension1
#endif
		);
		
		// FIXME: cls.state = ca_connecting so that we don't send the packet twice?
		return;
	}
#endif

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
		Cbuf_AddText(va("join %s\n", connection_str));
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
		char *password = command + 3;

		if (*password == '/') {
			*password = ' ';
		}
		else {
			*password = '\0';
		}

		Cbuf_AddText(va("qtvplay %s%s\n", connection_str, password));
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
	extern cshift_t	cshift_empty;
	extern void CL_ProcessServerInfo (void);

	S_StopAllSounds();

	Com_DPrintf ("Clearing memory\n");

	if (!com_serveractive) {
		Host_ClearMemory();
	}

	CL_ClearTEnts ();
	CL_ClearScene ();

	CL_ClearPredict();

	if (cls.state == ca_active) {
		int ideal_track = cl.ideal_track;
		int autocam = cl.autocam;

		// Wipe the entire cl structure.
		memset(&cl, 0, sizeof(cl));

		cl.ideal_track = ideal_track;
		cl.autocam = autocam;
	}
	else {
		// Wipe the entire cl structure.
		memset(&cl, 0, sizeof(cl));
	}

	// default weapon selection if nothing else replaces it
	cl.weapon_order[0] = 2;
	cl.weapon_order[1] = 1;
	cl.weapon_order[2] = 0;

	SZ_Clear (&cls.netchan.message);

	// Clear other arrays.
	memset(cl_dlight_active, 0, sizeof(cl_dlight_active));
	memset(cl_lightstyle, 0, sizeof(cl_lightstyle));
	memset(cl_entities, 0, sizeof(cl_entities));
	memset(cl_static_entities, 0, sizeof(cl_static_entities));

	// Set entnum for all entity baselines
	for (i = 0; i < sizeof(cl_entities) / sizeof(cl_entities[0]); ++i) {
		cl_entities[i].baseline.number = i;
	}

	// Set default viewheight for mvd, we copy cl.players[].stats[] to cl_stats[] in Cam_Lock() when pov changes.
	for (i = 0; i < MAX_CLIENTS; i++)
		cl.players[i].stats[STAT_VIEWHEIGHT] = DEFAULT_VIEWHEIGHT;
	// Set default viewheight for normal game/current pov.
	cl.stats[STAT_VIEWHEIGHT] = DEFAULT_VIEWHEIGHT;

	R_Init_EFrags();

	memset(&cshift_empty, 0, sizeof(cshift_empty));

	// Clear shownick structs
	SCR_ClearShownick();

	// Clear teaminfo structs
	SCR_ClearTeamInfo();

	// Clear weapon stats structs
	SCR_ClearWeaponStats();

	SCR_CenterPrint_Clear();

	if (!com_serveractive)
		Cvar_ForceSet (&host_mapname, ""); // Notice mapname not valid yet.

	cl.fakeshaft_policy = 1;

	// Default teamnames for TF
	strlcpy(cl.fixed_team_names[0], "blue", sizeof(cl.fixed_team_names[0]));
	strlcpy(cl.fixed_team_names[1], "red", sizeof(cl.fixed_team_names[1]));
	strlcpy(cl.fixed_team_names[2], "yell", sizeof(cl.fixed_team_names[2]));
	strlcpy(cl.fixed_team_names[3], "gren", sizeof(cl.fixed_team_names[3]));

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
	CL_MultiviewResetCvars ();

	// Stop sounds (especially looping!)
	S_StopAllSounds();

	MT_Disconnect();

	//
	R_OnDisconnect();

	if (cls.demorecording && cls.state != ca_disconnected) {
		CL_Stop_f();
	}

	if (cls.mvdrecording && cls.state != ca_disconnected) {
		extern void CL_StopMvd_f(void);

		CL_StopMvd_f();
	}

	if (cls.demoplayback) {
		CL_StopPlayback();
	} 
	else if (cls.state != ca_disconnected) {
		final[0] = clc_stringcmd;
		strlcpy ((char *)(final + 1), "drop", sizeof (final) - 1);
		Netchan_Transmit (&cls.netchan, 6, final);
		Netchan_Transmit (&cls.netchan, 6, final);
		Netchan_Transmit (&cls.netchan, 6, final);

		CL_UnqueOutputPacket(true);

		// TCP connect, that gives TCP a chance to transfer data to the server...
		if (cls.sockettcp != INVALID_SOCKET) {
			Sys_MSleep(1000);
		}
	}

	memset(&cls.netchan, 0, sizeof(cls.netchan));
	memset(&cls.server_adr, 0, sizeof(cls.server_adr));
	cls.state = ca_disconnected;
	cls.fteprotocolextensions = cls.fteprotocolextensions2 = cls.mvdprotocolextensions1 = 0;
	connect_time = 0;

	Cam_Reset();

	if (cls.download) {
		CL_FinishDownload();
	}
	else {
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
	if (cls.sockettcp != INVALID_SOCKET) {
		closesocket(cls.sockettcp);
		cls.sockettcp = INVALID_SOCKET;
	}

	cls.qport++; // A hack I picked up from qizmo.

	SZ_Clear(&cls.cmdmsg);

	// So join/observe not confused
	Info_SetValueForStarKey(cl.serverinfo, "*z_ext", "", sizeof(cl.serverinfo));
	cl.z_ext = 0;

	// well, we need free qtv users before new connection
	QTV_FreeUserList();

	Cvar_ForceSet(&host_mapname, ""); // Notice mapname not valid yet
}

void CL_Disconnect_f (void) 
{
	cl.intermission = 0;
	CL_Demo_Disconnected();

	Host_EndGame();
}

// The server is changing levels.
void CL_Reconnect_f (void) 
{
	if (cls.download)
		return; // Don't change when downloading.

	S_StopAllSounds();

	if (cls.mvdplayback) {
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
	#ifdef PROTOCOL_VERSION_MVD1
	unsigned int pext_mvd1 = 0;
	#endif

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
#ifdef PROTOCOL_VERSION_MVD1
				if (c == PROTOCOL_VERSION_MVD1)
					pext_mvd1 = MSG_ReadLong();
				else
#endif
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
#ifdef PROTOCOL_VERSION_MVD1
				,
#endif
#endif // PROTOCOL_VERSION_FTE
#ifdef PROTOCOL_VERSION_MVD1
				pext_mvd1
#endif
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
			Netchan_Setup(NS_CLIENT, &cls.netchan, net_from, cls.qport, 0);
			MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
			MSG_WriteString (&cls.netchan.message, "new");
			cls.state = ca_connected;
#ifdef DEBUG_MEMORY_ALLOCATIONS
			Sys_Printf("\nevent,connected...\n");
#endif
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
				if (KeyDestStartupDemo(key_dest)) {
					key_dest = key_console;
				}

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
	CL_CheckQizmoCompletion ();

	if (cls.demoplayback)
		return CL_GetDemoMessage();

	if (!NET_GetPacket(NS_CLIENT))
		return false;

	return true;
}

static void CL_ReadPackets(void)
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

		if (cls.lastto == 0 && cls.lasttype == dem_multiple) {
#ifdef MVD_PEXT1_HIDDEN_MESSAGES
			if (cls.mvdprotocolextensions1 & MVD_PEXT1_HIDDEN_MESSAGES) {
				CL_ParseHiddenDataMessage();
			}
#endif
			continue;
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

static void CL_InitLocal(void)
{
	char st[256];

	extern void Cl_Messages_Init(void);

	Cl_Messages_Init();

	Cvar_SetCurrentGroup(CVAR_GROUP_CHAT);
	Cvar_Register(&cl_parseWhiteText);
	Cvar_Register(&cl_chatsound);
	Cvar_Register(&cl_fakename);
	Cvar_Register(&cl_fakename_suffix);

	Cvar_Register(&cl_restrictions);

	Cvar_Register(&cl_floodprot);
	Cvar_Register(&cl_fp_messages);
	Cvar_Register(&cl_fp_persecond);

	Cvar_Register(&msg_filter);

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register(&cl_shownet);
	Cvar_Register(&cl_confirmquit);
	Cvar_Register(&cl_window_caption);
	Cvar_Register(&cl_onload);

#ifdef WIN32
	Cvar_Register(&cl_verify_qwprotocol);
#endif // WIN32

	Cvar_SetCurrentGroup(CVAR_GROUP_SBAR);
	Cvar_Register(&cl_sbar);
	Cvar_Register(&cl_hudswap);

	Cvar_SetCurrentGroup(CVAR_GROUP_VIEWMODEL);
	Cvar_Register(&cl_filterdrawviewmodel);

	Cvar_SetCurrentGroup(CVAR_GROUP_EYECANDY);
	Cvar_Register(&cl_model_bobbing);
	Cvar_Register(&cl_nolerp);
	Cvar_Register(&cl_nolerp_on_entity);
	Cvar_Register(&cl_newlerp);
	Cvar_Register(&cl_lerp_monsters);
	Cvar_Register(&cl_maxfps);
	Cvar_Register(&cl_maxfps_menu);
	Cvar_Register(&cl_physfps);
	Cvar_Register(&hud_fps_min_reset_interval);
	Cvar_Register(&hud_frametime_max_reset_interval);
	Cvar_Register(&hud_performance_average);
	Cvar_Register(&cl_physfps_spectator);
	Cvar_Register(&cl_independentPhysics);
	Cvar_Register(&cl_deadbodyfilter);
	Cvar_Register(&cl_gibfilter);
	Cvar_Register(&cl_backpackfilter);
	Cvar_Register(&cl_muzzleflash);
	Cvar_Register(&cl_rocket2grenade);
	Cvar_Register(&r_explosiontype);
	Cvar_Register(&r_lightflicker);
	Cvar_Register(&r_lightmap_lateupload);
	Cvar_Register(&r_lightmap_packbytexture);
	Cvar_Register(&r_rockettrail);
	Cvar_Register(&r_grenadetrail);
	Cvar_Register(&r_railtrail);
	Cvar_Register(&r_instagibtrail);
	Cvar_Register(&r_powerupglow);
	Cvar_Register(&cl_novweps);
	Cvar_Register(&r_drawvweps);
	Cvar_Register(&r_rocketlight);
	Cvar_Register(&r_explosionlight);
	Cvar_Register(&r_rocketlightcolor);
	Cvar_Register(&r_explosionlightcolor);
	Cvar_Register(&r_flagcolor);
	Cvar_Register(&cl_fakeshaft);
	Cvar_Register(&cl_fakeshaft_extra_updates);
	Cvar_Register(&r_telesplash);
	Cvar_Register(&r_shaftalpha);
	Cvar_Register(&r_lightdecayrate);

	Skin_RegisterCvars();

	Cvar_SetCurrentGroup(CVAR_GROUP_DEMO);
	Cvar_Register(&cl_demospeed);
	Cvar_Register(&cl_demoPingInterval);
	Cvar_Register(&qizmo_dir);
	Cvar_Register(&qwdtools_dir);
	Cvar_Register(&demo_getpings);
	Cvar_Register(&demo_autotrack);
	Cvar_Register(&cl_demoteamplay);

	Cvar_SetCurrentGroup(CVAR_GROUP_SOUND);
	Cvar_Register(&cl_staticsounds);

	Cvar_SetCurrentGroup(CVAR_GROUP_USERINFO);
	Cvar_Register(&team);
	Cvar_Register(&spectator);
	Cvar_Register(&skin);
	Cvar_Register(&rate);
	Cvar_Register(&mtu);
	Cvar_Register(&name);
	Cvar_Register(&msg);
	Cvar_Register(&noaim);
	Cvar_Register(&topcolor);
	Cvar_Register(&bottomcolor);
	Cvar_Register(&w_switch);
	Cvar_Register(&b_switch);
	Cvar_Register(&railcolor);
	Cvar_Register(&gender);

	Cvar_SetCurrentGroup(CVAR_GROUP_NETWORK);
	Cvar_Register(&cl_predict_players);
	Cvar_Register(&cl_solid_players);
	Cvar_Register(&cl_predict_half);
	Cvar_Register(&cl_timeout);
	Cvar_Register(&cl_useproxy);
	Cvar_Register(&cl_proxyaddr);
	Cvar_Register(&cl_crypt_rcon);
	Cvar_Register(&cl_fix_mvd);

	Cvar_Register(&cl_delay_packet);
	Cvar_Register(&cl_delay_packet_target);
	Cvar_Register(&cl_delay_packet_dev);
	Cvar_Register(&cl_earlypackets);

#if defined(PROTOCOL_VERSION_FTE) || defined(PROTOCOL_VERSION_FTE2) || defined(PROTOCOL_VERSION_MVD1)
	Cvar_Register(&cl_pext);
	Cvar_Register(&cl_pext_limits);
	Cvar_Register(&cl_pext_other);
	Cvar_Register(&cl_pext_warndemos);
#ifdef MVD_PEXT1_HIGHLAGTELEPORT
	Cvar_Register(&cl_pext_lagteleport);
#endif
#ifdef MVD_PEXT1_SERVERSIDEWEAPON
	Cvar_Register(&cl_pext_serversideweapon);
#endif
#endif // PROTOCOL_VERSION_FTE
#ifdef FTE_PEXT_256PACKETENTITIES
	Cvar_Register(&cl_pext_256packetentities);
#endif
#ifdef FTE_PEXT_CHUNKEDDOWNLOADS
	Cvar_Register(&cl_pext_chunkeddownloads);
	Cvar_Register(&cl_chunksperframe);
#endif

#ifdef FTE_PEXT_FLOATCOORDS
	Cvar_Register(&cl_pext_floatcoords);
#endif

#ifdef FTE_PEXT_TRANS
	Cvar_Register(&cl_pext_alpha);
#endif

	Cvar_SetCurrentGroup(CVAR_GROUP_INPUT_KEYBOARD);
	Cvar_Register(&allow_scripts);

	Cvar_SetCurrentGroup(CVAR_GROUP_SYSTEM_SETTINGS);
	Cvar_Register(&cl_mediaroot);

#ifndef CLIENTONLY
	Cvar_SetCurrentGroup(CVAR_GROUP_COMMUNICATION);
	Cvar_Register(&cl_sv_packetsync);
#endif

	Cvar_SetCurrentGroup(CVAR_GROUP_NO_GROUP);
	Cvar_Register (&password);
	Cvar_Register (&rcon_password);
	Cvar_Register (&rcon_address);
	Cvar_Register (&localid);
	Cvar_Register (&cl_warncmd);
	Cvar_Register (&cl_cmdline);
	Cvar_ForceSet (&cl_cmdline, com_args_original);
	Cvar_ResetCurrentGroup();

	Cvar_Register (&cl_username);
	Cmd_AddCommand("authenticate", CL_Authenticate_f);

	// debugging antilag
	Cvar_Register(&cl_debug_antilag_view);
	Cvar_Register(&cl_debug_antilag_ghost);
	Cvar_Register(&cl_debug_antilag_self);
	Cvar_Register(&cl_debug_antilag_lines);
	Cvar_Register(&cl_debug_antilag_send);

	// debugging weapons
	Cvar_Register(&cl_debug_weapon_view);

	snprintf(st, sizeof(st), "ezQuake %i", REVISION);

	if (COM_CheckParm (cmdline_param_client_norjscripts) || COM_CheckParm (cmdline_param_client_noscripts))
		Cvar_SetValue (&allow_scripts, 0);

 	Info_SetValueForStarKey (cls.userinfo, "*client", st, MAX_INFO_STRING);

	if (COM_CheckParm(cmdline_param_client_noindphys))
	{
		Cvar_SetValue(&cl_independentPhysics, 0);
		Cvar_SetValue(&cl_nolerp, 1);
	}

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

	#if defined WIN32 || defined __linux__
	Cmd_AddCommand ("register_qwurl_protocol", Sys_RegisterQWURLProtocol_f);
	#endif // WIN32 or linux

	Cmd_AddCommand ("dns", CL_DNS_f);
	Cmd_AddCommand ("hash", CL_Hash_f);
	Cmd_AddCommand ("reconnect", CL_Reconnect_f);

	Cmd_AddMacro(macro_connectiontype, CL_Macro_ConnectionType);
	Cmd_AddMacro(macro_demoplayback, CL_Macro_Demoplayback);
	Cmd_AddMacro(macro_demotime, CL_Macro_Demotime);
	Cmd_AddMacro(macro_rand, CL_Macro_Rand);
	Cmd_AddMacro(macro_matchstatus, CL_Macro_Serverstatus);
	Cmd_AddMacro(macro_serverip, CL_Macro_ServerIp);
	Cmd_AddMacro(macro_conwidth, CL_Macro_Conwidth);
	Cmd_AddMacro(macro_conheight, CL_Macro_Conheight);

#ifdef WITH_RENDERING_TRACE
	if (R_DebugProfileContext()) {
		Cmd_AddCommand("dev_gfxtrace", Dev_VidFrameTrace);
		Cmd_AddCommand("dev_gfxtexturedump", Dev_VidTextureDump);
		Cmd_AddCommand("dev_gfxtexturelist", Dev_TextureList);
	}
#endif

#ifndef CLIENTONLY
	if (IsDeveloperMode()) {
		Cmd_AddCommand("dev_physicsnormalset", Dev_PhysicsNormalSet);
		Cmd_AddCommand("dev_physicsnormalshow", Dev_PhysicsNormalShow);
		Cmd_AddCommand("dev_physicsnormalsave", Dev_PhysicsNormalSave);
	}
#endif

	{
		extern void GL_BenchmarkLightmapFormats(void);

		Cmd_AddCommand("dev_gfxbenchmarklightmaps", GL_BenchmarkLightmapFormats);
	}
}

void GFX_Init (void) 
{
	Draw_Init();
	SCR_Init();
	R_Init();
	Sbar_Init();
	HUD_Editor_Init();	// Need to reload some textures.
	VX_TrackerInit();

	CachePics_CreateAtlas();
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
		Sys_Error("Couldn't load gfx/palette.lmp\n\nThis is typically caused by being unable to locate pak0.pak.\nCopy pak0.pak into 'id1' folder, in same directory as executable.");
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
	cls.max_frametime = 1;

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

	if ((cls.state == ca_disconnected) || (Minimized && !cls.download))
		if (cl_maxfps_menu.value >= 30)
			return 1 / cl_maxfps_menu.value;
		else
			return 1 / (r_displayRefresh.autoString ? atoi(r_displayRefresh.autoString) : 30.0);

	if (cls.demoplayback)
	{
		if (!cl_maxfps.value)
			return 0;

		// Multiview.
		fps = max(30.0, cl_maxfps.value);
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

void onchange_hud_performance_average(cvar_t* var, char* value, qbool* cancel)
{
	// Reset on change
	if (strcmp(var->string, value)) {
		Cl_Reset_Min_fps_f();
	}
}

void CL_CalcFPS(void)
{
	double t = Sys_DoubleTime();
	perfinfo_t* stats = &cls.fps_stats;
	double frametime = stats->last_run_time == 0 ? 0 : t - stats->last_run_time;
	double time_since_snapshot = (t - stats->last_snapshot_time);
	stats->last_run_time = t;

	// Average over previous second
	if (time_since_snapshot >= 1.0)
	{
		stats->lastfps_value = (double)stats->fps_count / time_since_snapshot;
		stats->lastframetime_value = time_since_snapshot / max(stats->fps_count, 1);
		stats->fps_count = 0;
		stats->last_snapshot_time = t;
	}

	cls.fps = stats->lastfps_value;
	cls.avg_frametime = stats->lastframetime_value;

	if (hud_performance_average.integer) {
		// update min_fps if last fps is less than our lowest accepted minfps (10.0) or greater than min_reset_interval
		if ((stats->lastfps_value > 10.0 && stats->lastfps_value < cls.min_fps) || ((t - stats->time_of_last_minfps_update) > hud_fps_min_reset_interval.value)) {
			cls.min_fps = stats->lastfps_value;
			stats->time_of_last_minfps_update = t;
		}
		if ((stats->lastframetime_value < 2.0f && stats->lastframetime_value > cls.max_frametime) || ((t - stats->time_of_last_maxframetime_update) > hud_frametime_max_reset_interval.value)) {
			cls.max_frametime = stats->lastframetime_value;
			stats->time_of_last_maxframetime_update = t;
		}
	}
	else if (frametime > 0) {
		// update min_fps if last fps is less than our lowest accepted minfps (10.0) or greater than min_reset_interval
		if (stats->lastfps_value < cls.min_fps || ((t - stats->time_of_last_minfps_update) > hud_fps_min_reset_interval.value)) {
			cls.min_fps = stats->lastfps_value;
			stats->time_of_last_minfps_update = t;
		}
		if (frametime > cls.max_frametime || ((t - stats->time_of_last_maxframetime_update) > hud_frametime_max_reset_interval.value)) {
			cls.max_frametime = frametime;
			stats->time_of_last_maxframetime_update = t;
		}
	}
}

void Cl_Reset_Min_fps_f(void)
{
	cls.min_fps = 9999.0f;
	cls.max_frametime = 0.0f;

	CL_CalcFPS();
}

void CL_QTVPoll (void);
void SB_ExecuteQueuedTriggers (void);

void CL_LinkEntities (void)
{
	if (cls.state >= ca_onserver)
	{
		// actually it should be curtime == cls.netchan.last_received but that did not work on float values...
		qbool recent_packet = (curtime - cls.netchan.last_received < 0.00001);
		qbool setup_player_prediction = ((physframe && cl_independentPhysics.value != 0) || cl_independentPhysics.value == 0);
		setup_player_prediction |= recent_packet && !cls.demoplayback && cl_earlypackets.integer;

		Cam_SetViewPlayer();

		if (setup_player_prediction) {
			// Set up prediction for other players
			CL_SetUpPlayerPrediction(false);
			CL_PredictMove(true);
			CL_SetUpPlayerPrediction(true);
		}
		else {
			// Do client side motion prediction
			CL_PredictMove(false);
		}

		// build a refresh entity list
		CL_EmitEntities();
	}
}

void CL_SoundFrame (void)
{
	if (cls.state == ca_active)
	{
		if (!ISPAUSED) {
			S_Update (r_origin, vpn, vright, vup);
		}
		else {
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
	}
	else
	{
		S_Update (vec3_origin, vec3_origin, vec3_origin, vec3_origin);
	}
}

static void CL_ServerFrame(double frametime)
{
#ifndef CLIENTONLY
	if (com_serveractive) {
		playermove_t oldmove;

		memcpy(&oldmove, &pmove, sizeof(playermove_t));

		SV_Frame(frametime);

		memcpy(&pmove, &oldmove, sizeof(playermove_t));
	}
#endif
}

void CL_Frame(double time)
{
	static double extratime = 0.001;
	double minframetime;
	static double	extraphysframetime;	//#fps
	qbool need_server_frame = false;

	extratime += time;
	minframetime = CL_MinFrameTime();
	CL_MultiviewFrameStart ();

	if (extratime < minframetime) {
		extern cvar_t sys_yieldcpu;
		if (sys_yieldcpu.integer || Minimized) {
			#ifdef _WIN32
			Sys_MSleep(0);
			#else
			usleep( (minframetime - extratime) * 1000 * 1000 );
			#endif
		}

		if (cl_delay_packet.integer || cl_delay_packet_target.integer) {
			CL_QueInputPacket();
			need_server_frame = CL_UnqueOutputPacket(false);
		}

		if (need_server_frame && PROCESS_SERVERPACKETS_IMMEDIATELY) {
			CL_ServerFrame(0);
		}

		return;
	}

	if (cl_delay_packet.integer || cl_delay_packet_target.integer) {
		CL_QueInputPacket();
		need_server_frame = CL_UnqueOutputPacket(false);
	}

	if (VID_VSyncLagFix()) {
		if (need_server_frame && PROCESS_SERVERPACKETS_IMMEDIATELY) {
			CL_ServerFrame(0);
		}
		return;
	}

	cls.trueframetime = extratime - 0.001;
	cls.trueframetime = max(cls.trueframetime, minframetime);
	extratime -= cls.trueframetime;

	if (Movie_IsCapturing()) {
		Movie_StartFrame();
		cls.frametime = Movie_Frametime();
	}
	else {
		cls.frametime = min(0.2, cls.trueframetime);
	}
	
	if (cl_independentPhysics.value != 0)
	{
		double minphysframetime = MinPhysFrameTime();

		extraphysframetime += cls.frametime;
		if (extraphysframetime < minphysframetime) {
			physframe = false;
		}
		else {
			physframe = true;

			// FIXME: this is for the case when actual fps is too low.  Dunno how to do it right
			if (extraphysframetime > minphysframetime * 2) {
				physframetime = extraphysframetime;
			}
			else {
				physframetime = minphysframetime;
			}
			extraphysframetime -= physframetime;
		}
	} 
	else {
		// this vars SHOULD NOT be used in case of cl_independentPhysics == 0, so we just reset it for sanity
		physframetime = extraphysframetime = 0;
		// this var actually used
		physframe = true;
	}

	if (cls.demoplayback) {
		Demo_AdjustSpeed();

		if (cl.paused & PAUSED_DEMO) {
			cls.frametime = 0;
		}
		else if (!cls.timedemo) {
			cls.frametime *= Demo_GetSpeed();
		}
		else if (cls.timedemo == TIMEDEMO_FIXEDFPS) {
			cls.frametime = cls.td_frametime;
		}

		if (!host_skipframe) {
			cls.demotime += cls.frametime;
		}
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

#ifndef CLIENTONLY
		CL_ServerFrame(cls.frametime);
#endif

		// fetch results from server
		CL_ReadPackets();

		TP_UpdateSkins();

		if (cls.mvdplayback && !cls.demoseeking)
		{
			MVD_Interpolate();
			MVD_Mainhook();

			if (!cl.standby && physframe) {
				StatsGrid_Gather();
			}
		}

		// process stuffed commands
		Cbuf_ExecuteEx(&cbuf_svc);

		CL_SendToServer();

		// We need to move the mouse also when disconnected
		// to get the cursor working properly.
		if (cls.state == ca_disconnected) {
			usercmd_t dummy;
			IN_Move(&dummy);
		}

		Sys_SendDeferredKeyEvents();
	}
	else 
	{
		if (physframe) {
			Sys_SendKeyEvents();

			// allow mice or other external controllers to add commands
			IN_Commands();

			// process console commands
			Cbuf_Execute();
			CL_CheckAutoPause ();

#ifndef CLIENTONLY
			CL_ServerFrame(physframetime);
#endif

			// Fetch results from server
			CL_ReadPackets();

			TP_UpdateSkins();

			// Gather MVD stats and interpolate.
			if (cls.mvdplayback && !cls.demoseeking)
			{
				MVD_Interpolate();
				MVD_Mainhook();

				if (!cl.standby && physframe) {
					StatsGrid_Gather();
				}
			}

			// process stuffed commands
			Cbuf_ExecuteEx(&cbuf_svc);

			CL_SendToServer();

			// We need to move the mouse also when disconnected
			if (cls.state == ca_disconnected) {
				usercmd_t dummy;
				IN_Move(&dummy);
			}

			Sys_SendDeferredKeyEvents();
		}
		else {
			if (need_server_frame && PROCESS_SERVERPACKETS_IMMEDIATELY) {
				CL_ServerFrame(0);
			}

			if (!cls.demoplayback && cl_earlypackets.integer) {
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

	{
		// chat icons
		int cif_flags = 0;

		// add chat flag if in console, menus, mm1, mm2 etc...
		cif_flags |= (key_dest != key_game ? CIF_CHAT : 0);

		// add AFK flag if app minimized, or not the focus
		// TODO: may be add afk flag on idle? if no user input in 45 seconds for example?
		cif_flags |= (!ActiveApp || Minimized ? CIF_AFK : 0);

		if (cif_flags != cl.cif_flags) {
			char char_flags[64] = {0};

			if (cif_flags && cls.state >= ca_connected) {
				// put key in userinfo only then we are connected, remove key if we not connected yet
				snprintf(char_flags, sizeof(char_flags), "%d", cif_flags);
			}

			CL_UserinfoChanged("chat", char_flags);
			cl.cif_flags = cif_flags;
		}
	}

	VID_ReloadCheck();

	R_ParticleFrame();

	buffers.StartFrame();

	CachePics_AtlasFrame();

	CL_MultiviewPreUpdateScreen();

	// update video
	if (CL_MultiviewEnabled()) {
		qbool draw_next_view = true;
		qbool first_view = true;

		R_PerformanceBeginFrame();
		if (SCR_UpdateScreenPrePlayerView()) {
			qbool two_pass_rendering = GL_FramebufferEnabled2D();
			renderer.ScreenDrawStart();

			while (draw_next_view) {
				draw_next_view = CL_MultiviewAdvanceView();
				if (!first_view) {
					buffers.EndFrame();
					buffers.StartFrame();
				}
				first_view = false;

				CL_LinkEntities();

				SCR_CalcRefdef();

				SCR_UpdateScreenPlayerView((draw_next_view ? 0 : UPDATESCREEN_POSTPROCESS) | (two_pass_rendering ? UPDATESCREEN_3D_ONLY : 0));

				if (!two_pass_rendering) {
					SCR_DrawMultiviewIndividualElements();
				}
				else {
					SCR_SaveAutoID();
				}

				if (CL_MultiviewCurrentView() == 2 || (CL_MultiviewCurrentView() == 1 && CL_MultiviewActiveViews() == 1)) {
					CL_SoundFrame();
				}

				// Multiview: advance to next player
				CL_MultiviewFrameFinish();
			}

			if (two_pass_rendering) {
				buffers.EndFrame();

				draw_next_view = true;
				while (draw_next_view) {
					draw_next_view = CL_MultiviewAdvanceView();

					// Need to call this again to keep autoid correct
					SCR_RestoreAutoID();

					SCR_UpdateScreenPlayerView(UPDATESCREEN_2D_ONLY);
					SCR_DrawMultiviewIndividualElements();

					// Multiview: advance to next player
					CL_MultiviewFrameFinish();
				}
			}

			SCR_UpdateScreenPostPlayerView();
		}
		else {
			VID_RenderFrameEnd();
		}
		R_PerformanceEndFrame();
	}
	else {
		CL_LinkEntities();

		R_PerformanceBeginFrame();
		SCR_UpdateScreen();
		R_PerformanceEndFrame();

		CL_SoundFrame();
	}

	CL_DecayLights();

	CDAudio_Update();

	MT_Frame();

	if (Movie_IsCapturing()) {
		Movie_FinishFrame();
	}

	cls.framecount++;
	cls.fps_stats.fps_count++;
	CL_CalcFPS();

	VFS_TICK(); // VFS hook for updating some systems

	Sys_ReadIPC();

	CL_QTVPoll();
#ifdef WITH_IRC
	IRC_Update();
#endif

	SB_ExecuteQueuedTriggers();

	R_ParticleEndFrame();

	CL_UpdateCaption(false);
}

//============================================================================

void CL_Shutdown (void) 
{
	CL_Disconnect();
	SList_Shutdown();
	CDAudio_Shutdown();
	S_Shutdown();
	IN_Shutdown ();
	Log_Shutdown();
	if (host_basepal) {
		VID_Shutdown(false);
	}
	History_Shutdown();
	Sys_CloseIPC();
	SB_Shutdown();
	MT_Shutdown();
	Help_Shutdown();
	HUD_Shutdown();
	Cmd_Shutdown();
	Key_Shutdown();
	TP_Shutdown();
	M_Shutdown();
	Stats_Shutdown();
	Draw_Shutdown();
	Cache_Flush();
	Q_free(host_basepal);
	Q_free(host_colormap);
}

void CL_UpdateCaption(qbool force)
{
	static char caption[512] = { 0 };
	char str[512] = { 0 };

	if (!cl_window_caption.value) {
		if (!cls.demoplayback && (cls.state == ca_active)) {
			snprintf(str, sizeof(str), "ezQuake: %s", cls.servername);
		}
		else {
			snprintf(str, sizeof(str), "ezQuake");
		}
	}
	else if (cl_window_caption.integer == 1) {
		snprintf(str, sizeof(str), "%s - %s", CL_Macro_Serverstatus(), MT_ShortStatus());
	}
	else if (cl_window_caption.integer == 2) {
		snprintf(str, sizeof(str), "ezQuake");
	}

	if (force || strcmp(str, caption)) {
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



#ifndef CLIENTONLY

void Dev_PhysicsNormalShow(void)
{
	vec3_t point = { cl.simorg[0], cl.simorg[1], cl.simorg[2] - 1 };
	trace_t trace;
	mphysicsnormal_t physicsnormal;

	if (cls.demoplayback || cls.state != ca_active || !r_refdef2.allow_cheats) {
		// it's not actually a cheat, but using these functions would screw with prediction
		Con_Printf("Not available outwith /devmap\n");
		return;
	}

	trace = PM_PlayerTrace(pmove.origin, point);
	if (trace.fraction == 1 || trace.plane.normal[2] < MIN_STEP_NORMAL) {
		Con_Printf("Not on ground\n");
	}
	else {
		physicsnormal = CM_PhysicsNormal(trace.physicsnormal);

		Con_Printf("Plane normal  : %+f %+f %+f\n", trace.plane.normal[0], trace.plane.normal[1], trace.plane.normal[2]);
		if (!(physicsnormal.flags & PHYSICSNORMAL_SET)) {
			Con_Printf("No custom physics plane found\n");
		}
		else {
			const char* flipx = physicsnormal.flags & PHYSICSNORMAL_FLIPX ? "&cff0" : "&r";
			const char* flipy = physicsnormal.flags & PHYSICSNORMAL_FLIPY ? "&cff0" : "&r";
			const char* flipz = physicsnormal.flags & PHYSICSNORMAL_FLIPZ ? "&cff0" : "&r";

			Con_Printf("Physics normal: %s%+f %s%+f %s%+f&r\n", flipx, physicsnormal.normal[0], flipy, physicsnormal.normal[1], flipz, physicsnormal.normal[2]);
		}
	}
}

void Dev_PhysicsNormalSet(void)
{
	vec3_t point = { cl.simorg[0], cl.simorg[1], cl.simorg[2] - 1 };
	trace_t trace;
	mphysicsnormal_t physicsnormal;
	vec3_t newnormal;
	int newflags = PHYSICSNORMAL_SET;

	if (cls.demoplayback || cls.state != ca_active || !r_refdef2.allow_cheats) {
		// it's not actually a cheat, but using these functions would screw with prediction
		Con_Printf("Not available outwith /devmap\n");
		return;
	}

	if (Cmd_Argc() < 5) {
		Com_Printf("Usage: %s <x> <y> <z> <flags(xyzn)>\n", Cmd_Argv(0));
		return;
	}

	{
		int i;
		const char* flags = Cmd_Argv(4);

		newnormal[0] = atof(Cmd_Argv(1));
		newnormal[1] = atof(Cmd_Argv(2));
		newnormal[2] = atof(Cmd_Argv(3));
		for (i = 0; i < strlen(flags); ++i) {
			if (flags[i] == 'x') {
				newflags |= PHYSICSNORMAL_FLIPX;
			}
			else if (flags[i] == 'y') {
				newflags |= PHYSICSNORMAL_FLIPY;
			}
			else if (flags[i] == 'z') {
				newflags |= PHYSICSNORMAL_FLIPZ;
			}
			else if (flags[i] != 'n') {
				Com_Printf("Unknown flag %c\n", flags[i]);
			}
		}
	}

	if (newnormal[0] || newnormal[1] || newnormal[2]) {
		VectorNormalize(newnormal);
		if (VectorLength(newnormal) != 1) {
			Com_Printf("Error: failed to normalize, found %f %f %f\n", newnormal[0], newnormal[1], newnormal[2]);
			return;
		}
	}

	trace = PM_PlayerTrace(pmove.origin, point);
	if (trace.fraction == 1 || trace.plane.normal[2] < MIN_STEP_NORMAL) {
		Con_Printf("Not on ground\n");
	}
	else {
		physicsnormal = CM_PhysicsNormal(trace.physicsnormal);
		if (!(physicsnormal.flags & PHYSICSNORMAL_SET)) {
			Con_Printf("Unable to determine ground normal\n");
		}
		else {
			CM_PhysicsNormalSet(trace.physicsnormal, newnormal[0], newnormal[1], newnormal[2], newflags);
			Dev_PhysicsNormalShow();
		}
	}
}

void Dev_PhysicsNormalSave(void)
{
	char filename[MAX_OSPATH];
	FILE* out;
	extern cvar_t pm_rampjump;

	if (cls.demoplayback || cls.state != ca_active || !r_refdef2.allow_cheats) {
		// it's not actually a cheat, but using these functions would screw with prediction
		Con_Printf("Not available outwith /devmap\n");
		return;
	}

	snprintf(filename, sizeof(filename), "qw/%s.qpn", host_mapname.string);
	if (!(out = fopen(filename, "wb"))) {
		Con_Printf("Failed: unable to open %s\n", filename);
		return;
	}

	CM_PhysicsNormalDump(out, pm_rampjump.value, 0);
	fclose(out);

	Con_Printf("Wrote %s\n", filename);
}

#endif // !CLIENTONLY

void Cache_Flush(void)
{
	Skin_Clear(false);
	Mod_FreeAllCachedData();
}

static void AuthUsernameChanged(cvar_t* var, char* value, qbool* cancel)
{
	char path[MAX_PATH];
	char filename[MAX_PATH];
	byte* auth_token;
	int auth_token_length;
	int len = strlen(value);
	int i;

	// Don't validate if no changes
	if (!strcmp(var->string, value)) {
		return;
	}

	// Validate new name
	for (i = 0; i < len; ++i) {
		if (isalpha(value[i]) || isdigit(value[i]))
			continue;
		if (value[i] == '_' || value[i] == '[' || value[i] == ']' || value[i] == '(' || value[i] == ')' || value[i] == '.' || value[i] == '-')
			continue;
		// TODO: others

		// Illegal
		Com_Printf("Illegal character %c in username\n", value[i]);
		*cancel = true;
		return;
	}

	if (!value[0]) {
		// Logging out...
		if (cls.state == ca_active && cl.players[cl.playernum].loginname[0]) {
			Cbuf_AddTextEx(&cbuf_main, "cmd logout\n");
		}
		memset(cls.auth_logintoken, 0, sizeof(cls.auth_logintoken));
		return;
	}
	
	// FIXME: server-side, treat new login request as logout?
	if (cls.state == ca_active && cl.players[cl.playernum].loginname[0]) {
		Com_Printf("You are logged in to the server & must logout first\n");
		*cancel = true;
		return;
	}

	// Try and load login token
	strlcpy(filename, value, sizeof(filename));
	COM_ForceExtensionEx(filename, ".apikey", sizeof(filename));

	Cfg_GetConfigPath(path, sizeof(path), filename);

	auth_token = FS_LoadTempFile(path, &auth_token_length);
	if (auth_token == 0) {
		Com_Printf("Unable to load authentication token for '%s'\n", value);
		*cancel = true;
		return;
	}
	else {
		strlcpy(cls.auth_logintoken, (const char*)auth_token, sizeof(cls.auth_logintoken));
		memset(cl.auth_challenge, 0, sizeof(cl.auth_challenge));
		if (cls.state == ca_active) {
			Cbuf_AddTextEx(&cbuf_main, va("cmd login %s\n", value));
		}
	}
}

static void CL_Authenticate_f(void)
{
	if (!cls.auth_logintoken[0] || !cl_username.string[0]) {
		Com_Printf("You must load a login token by setting cl_username first.\n");
		Com_Printf("Login tokens must be located in your configuration directory\n");
		return;
	}

	if (cls.state != ca_active) {
		Com_Printf("Cannot authenticate, not connected\n");
		return;
	}

	if (cl.players[cl.playernum].loginname[0]) {
		Com_Printf("Logging out...\n");
		Cbuf_AddTextEx(&cbuf_main, "cmd logout\n");
		return;
	}

	Com_Printf("Starting authentication process...\n");
	Cbuf_AddTextEx(&cbuf_main, va("cmd login \"%s\"\n", cl_username.string));
}
