/*

	Options Menu module

	Uses Ctrl_Tab and Settings modules

	Naming convention:
	function mask | usage    | purpose
	--------------|----------|-----------------------------
	Menu_Opt_*    | external | interface for menu.c
	CT_Opt_*      | external | interface for Ctrl_Tab.c

	made by:
		johnnycz, Jan 2006

*/

#include "quakedef.h"
#include "settings.h"
#include "settings_page.h"
#include "Ctrl_EditBox.h"
#include "vx_stuff.h"
#include "vx_tracker.h"
#ifdef GLQUAKE
#include "gl_model.h"
#include "gl_local.h"
#include "tr_types.h"
#else
#include "r_model.h"
#include "r_local.h"
#endif
#include "teamplay.h"
#include "EX_FileList.h"
#include "Ctrl.h"
#include "Ctrl_Tab.h"
#include "input.h"
#include "qsound.h"
#include "menu.h"
#include "keys.h"
#include "hud.h"
#include "hud_common.h"

typedef enum {
	OPTPG_MISC,
	OPTPG_PLAYER,
	OPTPG_FPS,
	OPTPG_HUD,
	OPTPG_DEMO_SPEC,
	OPTPG_BINDS,
	OPTPG_SYSTEM,
	OPTPG_CONFIG,
}	options_tab_t;

CTab_t options_tab;
int options_unichar;	// variable local to this module

#ifdef GLQUAKE
extern cvar_t     scr_scaleMenu;
extern int        menuwidth;
extern int        menuheight;
#else
#define menuwidth vid.width
#define menuheight vid.height
#endif

extern qbool    m_entersound; // todo - put into menu.h
void M_Menu_Help_f (void);	// todo - put into menu.h
extern cvar_t scr_scaleMenu;
qbool OnMenuAdvancedChange(cvar_t*, char*);
cvar_t menu_advanced = {"menu_advanced", "0"};

//=============================================================================
// <SETTINGS>

#ifdef GLQUAKE
enum {mode_fastest, mode_faithful, mode_higheyecandy, mode_eyecandy, mode_undef} fps_mode = mode_undef;
#else
enum {mode_fastest, mode_default, mode_undef} fps_mode = mode_default;
#endif

extern cvar_t scr_fov, scr_newHud, cl_staticsounds, r_fullbrightSkins, cl_deadbodyfilter, cl_muzzleflash, r_fullbright;
extern cvar_t scr_sshot_format;
void ResetConfigs_f(void);

static qbool AlwaysRun(void) { return cl_forwardspeed.value > 200; }
const char* AlwaysRunRead(void) { return AlwaysRun() ? "on" : "off"; }
void AlwaysRunToggle(qbool back) {
	if (AlwaysRun()) {
		Cvar_SetValue (&cl_forwardspeed, 200);
		Cvar_SetValue (&cl_backspeed, 200);
		Cvar_SetValue (&cl_sidespeed, 200);
	} else {
		Cvar_SetValue (&cl_forwardspeed, 400);
		Cvar_SetValue (&cl_backspeed, 400);
		Cvar_SetValue (&cl_sidespeed, 400);
	}
}

static qbool InvertMouse(void) { return m_pitch.value < 0; }
const char* InvertMouseRead(void) { return InvertMouse() ? "on" : "off"; }
void InvertMouseToggle(qbool back) { Cvar_SetValue(&m_pitch, -m_pitch.value); }

static qbool AutoSW(void) { return (w_switch.value > 2) || (!w_switch.value) || (b_switch.value > 2) || (!b_switch.value); }
const char* AutoSWRead(void) { return AutoSW() ? "on" : "off"; }
void AutoSWToggle(qbool back) {
	if (AutoSW()) {
		Cvar_SetValue (&w_switch, 1);
		Cvar_SetValue (&b_switch, 1);
	} else {
		Cvar_SetValue (&w_switch, 8);
		Cvar_SetValue (&b_switch, 8);
	}
}

static int GFXPreset(void) {
	if (fps_mode == mode_undef) {
		switch ((int) cl_deadbodyfilter.value) {
#ifdef GLQUAKE
		case 0: fps_mode = mode_eyecandy; break;
		case 1: fps_mode = cl_muzzleflash.value ? mode_faithful : mode_eyecandy; break;
		default: fps_mode = mode_fastest; break;
#else
		case 0: fps_mode = mode_default; break;
		default: fps_mode = mode_fastest; break;
#endif
		}
	}
	return fps_mode;
}
const char* GFXPresetRead(void) {
	switch (GFXPreset()) {
	case mode_fastest: return "fastest";
#ifdef GLQUAKE
	case mode_higheyecandy: return "high eyecandy";
	case mode_faithful: return "faithful";
	default: return "eyecandy";
#else
	default: return "default";
#endif
	}
}
void GFXPresetToggle(qbool back) {
	if (back) fps_mode--; else fps_mode++;
	if (fps_mode < 0) fps_mode = mode_undef - 1;
	if (fps_mode >= mode_undef) fps_mode = 0;

	switch (GFXPreset()) {
#ifdef GLQUAKE
	case mode_fastest: Cbuf_AddText ("exec cfg/gfx_gl_fast.cfg\n"); return;
	case mode_higheyecandy: Cbuf_AddText ("exec cfg/gfx_gl_higheyecandy.cfg\n"); return;
	case mode_faithful: Cbuf_AddText ("exec cfg/gfx_gl_faithful.cfg\n"); return;
	case mode_eyecandy: Cbuf_AddText ("exec cfg/gfx_gl_eyecandy.cfg\n"); return;
#else
	case mode_fastest: Cbuf_AddText ("exec cfg/gfx_sw_fast.cfg\n"); return;
	case mode_default: Cbuf_AddText ("exec cfg/gfx_sw_default.cfg\n"); return;
#endif
	}
}

const char* amf_tracker_frags_enum[] = {"off", "related", "all" };

const char* mvdautohud_enum[] = { "off", "simple", "customizable" };
const char* mvdautotrack_enum[] = { "off", "auto", "custom", "multitrack", "simple" };
const char* funcharsmode_enum[] = { "ctrl+key", "ctrl+y" };
const char* ignoreopponents_enum[] = { "off", "always", "on match" };
const char* msgfilter_enum[] = { "off", "say+spec", "team", "say+team+spec" };
const char* allowscripts_enum[] = { "off", "simple", "all" };
const char* scrautoid_enum[] = { "off", "nick", "health+armor", "health+armor+type", "all (rl)", "all (best gun)" };
const char* coloredtext_enum[] = { "off", "simple", "frag messages" };
const char* autorecord_enum[] = { "off", "don't save", "auto save" };
const char* hud_enum[] = { "classic", "new", "combined" };
const char* ignorespec_enum[] = { "off", "on (as player)", "on (always)" };

const char* scr_sshot_format_enum[] = {
	"JPG", "jpg", "PNG", "png", "TGA", "tga" };

extern cvar_t mvd_autotrack, mvd_moreinfo, mvd_status, cl_weaponpreselect, cl_weaponhide, con_funchars_mode, con_notifytime, scr_consize, ignore_opponents, _con_notifylines,
	ignore_qizmo_spec, ignore_spec, msg_filter, crosshair, crosshairsize, cl_smartjump, scr_coloredText,
	cl_rollangle, cl_rollspeed, v_gunkick, v_kickpitch, v_kickroll, v_kicktime, v_viewheight, match_auto_sshot, match_auto_record, match_auto_logconsole,
	r_fastturb, r_grenadetrail, cl_drawgun, r_viewmodelsize, r_viewmodeloffset, scr_clock, scr_gameclock, show_fps, rate, cl_c2sImpulseBackup,
	name, team, skin, topcolor, bottomcolor, cl_teamtopcolor, cl_teambottomcolor, cl_teamquadskin, cl_teampentskin, cl_teambothskin, /*cl_enemytopcolor, cl_enemybottomcolor, */
	cl_enemyquadskin, cl_enemypentskin, cl_enemybothskin, demo_dir, qizmo_dir, qwdtools_dir, cl_fakename, cl_fakename_suffix,
	cl_chatsound, con_sound_mm1_volume, con_sound_mm2_volume, con_sound_spec_volume, con_sound_other_volume, s_khz,
	ruleset, scr_sshot_dir, log_dir, cl_nolerp, cl_confirmquit, log_readable, ignore_flood, ignore_flood_duration, con_timestamps, scr_consize, scr_conspeed, cl_chatmode, cl_chasecam,
	enemyforceskins, teamforceskins, cl_vsync_lag_fix, cl_sayfilter_coloredtext, cl_sayfilter_sendboth,
	mvd_autotrack_lockteam, qtv_adjustbuffer, cl_earlypackets, cl_useimagesinfraglog, con_completion_format, vid_wideaspect, menu_ingame 
;
#ifdef _WIN32
extern cvar_t demo_format, sys_highpriority, cl_window_caption, sys_inactivesound, vid_flashonactivity;
void CL_RegisterQWURLProtocol_f(void);
#endif
#ifdef GLQUAKE
extern cvar_t scr_autoid, gl_crosshairalpha, gl_smoothfont, amf_hidenails, amf_hiderockets, gl_anisotropy, gl_lumaTextures, gl_textureless, gl_colorlights, scr_conalpha, scr_conback, gl_clear, gl_powerupshells, gl_powerupshells_size,
	scr_teaminfo
;
#endif

void CL_Autotrack_f(void);

const char* bandwidth_enum[] = { 
	"Modem (33k)", "3800", "Modem (56k)", "5670", 
	"ISDN (112k)", "9856", "Cable (128k)", "14336",
	"ADSL (> 256k)", "30000" };

const char* cl_c2sImpulseBackup_enum[] = {
	"Perfect", "0", "Low", "2", "Medium", "4",
	"High", "6" };
	

const char* ignore_flood_enum[] = {
	"Off", "0", "say/spec", "1", "say/say_team/spec", "2" };

#ifdef _WIN32
const char* demoformat_enum[] = { "QuakeWorld Demo", "qwd", "Qizmo compressed QWD", "qwz", "MultiViewDemo", "mvd" };
#endif

const char* cl_chatmode_enum[] = {
	"All Commands", "All Chat", "First Word" };

const char* con_completion_format_enum[] = {
	"Old", "New:Current+Default", "New:Current", "New:Default"
};
	
const char* scr_conback_enum[] = {
	"Off", "On Load", "Always", };

const char* s_khz_enum[] = {
	"11 kHz", "11", "22 kHz", "22", "44 kHz", "44"
#if defined(__linux__)
	,"48 kHz", "48"
#endif
};

const char* cl_nolerp_enum[] = {"on", "off"};
const char* ruleset_enum[] = { "ezQuake default", "default", "Smackdown", "smackdown", "Moscow TF League", "mtfl" };
const char *mediaroot_enum[] = { "relative to exe", "relative to home", "full path" };
const char *teamforceskins_enum[] = { "off", "use player's name", "use player's userid", "set t1, t2, t3, ..." };
const char *enemyforceskins_enum[] = { "off", "use player's name", "use player's userid", "set e1, e2, e3, ..." };

#ifdef _WIN32
const char *priority_enum[] = { "low", "-1", "normal", "0", "high", "1" };
#endif


void DefaultConfig(void) { Cbuf_AddText("cfg_reset full\n"); }

settings_page settmisc;

void CT_Opt_Settings_Draw (int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page) {
	Settings_Draw(x, y, w, h, &settmisc);
}

int CT_Opt_Settings_Key (int k, wchar unichar, CTab_t *tab, CTabPage_t *page) {
	return Settings_Key(&settmisc, k, unichar);
}

void OnShow_SettMain(void) { Settings_OnShow(&settmisc); }

qbool CT_Opt_Settings_Mouse_Event(const mouse_state_t *ms)
{
	return Settings_Mouse_Event(&settmisc, ms);
}

// </SETTINGS>
//=============================================================================

settings_page settview;

void CT_Opt_View_Draw (int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page) {
	Settings_Draw(x, y, w, h, &settview);
}

int CT_Opt_View_Key (int k, wchar unichar, CTab_t *tab, CTabPage_t *page) {
	return Settings_Key(&settview, k, unichar);
}

void OnShow_SettView(void) { Settings_OnShow(&settview); }

qbool CT_Opt_View_Mouse_Event(const mouse_state_t *ms)
{
	return Settings_Mouse_Event(&settview, ms);
}
settings_page settplayer;
void CT_Opt_Player_Draw (int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page) {
	Settings_Draw(x, y, w, h, &settplayer);
}

int CT_Opt_Player_Key (int k, wchar unichar, CTab_t *tab, CTabPage_t *page) {
	return Settings_Key(&settplayer, k, unichar);
}

void OnShow_SettPlayer(void) { Settings_OnShow(&settplayer); }

qbool CT_Opt_Player_Mouse_Event(const mouse_state_t *ms)
{
	return Settings_Mouse_Event(&settplayer, ms);
}


//=============================================================================
// <BINDS>

extern cvar_t in_mouse, in_m_smooth, m_rate, in_m_os_parameters;
#ifdef _WIN32
const char* in_mouse_enum[] = { "off", "system", "Direct Input", "Raw Input" };
#elif defined __linux__
const char* in_mouse_enum[] = { "off", "DGA Mouse", "X Mouse", "EVDEV Mouse" };
#else
const char* in_mouse_enum[] = { "off", "DGA Mouse", "X Mouse" };
#endif
const char* in_m_os_parameters_enum[] = { "off", "Keep accel settings", "Keep speed settings", "Keep all settings" };

void Menu_Input_Restart(void) { Cbuf_AddText("in_restart\n"); }

settings_page settbinds;

void CT_Opt_Binds_Draw (int x2, int y2, int w, int h, CTab_t *tab, CTabPage_t *page) {
	Settings_Draw(x2, y2, w, h, &settbinds);
}

int CT_Opt_Binds_Key (int k, wchar unichar, CTab_t *tab, CTabPage_t *page) {
	return Settings_Key(&settbinds, k, unichar);
}

void OnShow_SettBinds(void) { Settings_OnShow(&settbinds); }

qbool CT_Opt_Binds_Mouse_Event(const mouse_state_t *ms)
{
	return Settings_Mouse_Event(&settbinds, ms);
}

// </BINDS>
//=============================================================================



//=============================================================================
// <FPS>
extern cvar_t v_bonusflash;
extern cvar_t cl_rocket2grenade;
extern cvar_t v_damagecshift;
extern cvar_t r_fastsky;
extern cvar_t r_drawflame;
#ifdef GLQUAKE
extern cvar_t gl_simpleitems;
extern cvar_t gl_simpleitems_orientation;
extern cvar_t gl_part_inferno;
extern cvar_t amf_lightning;
#endif
extern cvar_t r_drawflat;

const char* explosiontype_enum[] =
{ "fire + sparks", "fire only", "teleport", "blood", "big blood", "dbl gunshot", "blob effect", "big explosion", "plasma", "sparks", "off" };

const char* muzzleflashes_enum[] =
{ "off", "on", "own off" };

const char* simpleitemsorientation_enum[] =
{"Parallel upright", "Facing upright", "Parallel", "Oriented"};

const char* deadbodyfilter_enum[] =
{ "off", "decent", "instant" };

const char* rocketmodel_enum[] =
{ "rocket", "grenade" };

const char* rockettrail_enum[] =
{ "off", "normal", "grenade", "alt normal", "slight blood", "big blood", "tracer 1", "tracer 2", "plasma", "lavaball", "fuel rod", "plasma 2", "alt normal 2", "motion trail" };

const char* powerupglow_enum[] =
{ "off", "on", "own off" };

const char* grenadetrail_enum[] =
{ "off", "normal", "grenade", "alt normal", "slight blood", "big blood", "tracer 1", "tracer 2", "plasma", "lavaball", "fuel rod", "plasma 2", "alt normal 2", "motion trail" };

extern cvar_t cl_maxfps;
const char* FpslimitRead(void) {
	switch ((int) cl_maxfps.value) {
	case 0: return "no limit";
	case 72: return "72 (old std)";
	case 77: return "77 (standard)";
	default: return "custom";
	}
}
void FpslimitToggle(qbool back) {
	if (back) {
		switch ((int) cl_maxfps.value) {
		case 0: Cvar_SetValue(&cl_maxfps, 77); return;
		case 72: Cvar_SetValue(&cl_maxfps, 0); return;
		case 77: Cvar_SetValue(&cl_maxfps, 72); return;
		}
	} else {
		switch ((int) cl_maxfps.value) {
		case 0: Cvar_SetValue(&cl_maxfps, 72); return;
		case 72: Cvar_SetValue(&cl_maxfps, 77); return;
		case 77: Cvar_SetValue(&cl_maxfps, 0); return;
		}
	}
}

#ifdef GLQUAKE
const char* gl_max_size_enum[] = {
	"low", "1", "medium", "8", "high", "256", "max", "2048"
};

extern cvar_t gl_texturemode;
const char* gl_texturemode_enum[] = {
	"very low", "GL_NEAREST_MIPMAP_NEAREST",
	"low", "GL_LINEAR",
	"medium", "GL_LINEAR_MIPMAP_NEAREST",
	"high", "GL_LINEAR_MIPMAP_LINEAR",
	"very high", "GL_NEAREST"
};
#endif


settings_page settfps;

void CT_Opt_FPS_Draw (int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page)
{
	Settings_Draw(x, y, w, h, &settfps);
}

int CT_Opt_FPS_Key (int k, wchar unichar, CTab_t *tab, CTabPage_t *page) {
	return Settings_Key(&settfps, k, unichar);
}

void OnShow_SettFPS(void) { Settings_OnShow(&settfps); }

qbool CT_Opt_FPS_Mouse_Event(const mouse_state_t *ms)
{
	return Settings_Mouse_Event(&settfps, ms);
}


// </FPS>
//=============================================================================


//=============================================================================
// <SYSTEM>

#ifdef GLQUAKE

// these variables serve as a temporary storage for user selected settings
// they get initialized with current settings when the page is showed
typedef struct menu_video_settings_s {
	int res;
	int bpp;
	qbool fullscreen;
	cvar_t freq;	// this is not an int just because we need to fool settings_page module
} menu_system_settings_t;
qbool mss_askmode = false;

// here we store the configuration that user selected in menu
menu_system_settings_t mss_selected;

// here we store the current video config in case the new video settings weren't successfull
menu_system_settings_t mss_previous;

// will apply given video settings
static void ApplyVideoSettings(const menu_system_settings_t *s) {
#ifndef __APPLE__
	Cvar_SetValue(&r_mode, s->res);
	Cvar_SetValue(&r_colorbits, s->bpp);
	Cvar_SetValue(&r_displayRefresh, s->freq.value);
	Cvar_SetValue(&r_fullscreen, s->fullscreen);
#endif
	Cbuf_AddText("vid_restart\n");
    Com_Printf("askmode: %s\n", mss_askmode ? "on" : "off");
}

// will store current video settings into the given structure
static void StoreCurrentVideoSettings(menu_system_settings_t *out) {
#ifndef __APPLE__
	out->res = (int) r_mode.value;
	out->bpp = (int) r_colorbits.value;
	Cvar_SetValue(&out->freq, r_displayRefresh.value);
	out->fullscreen = (int) r_fullscreen.value;
#endif
}

// performed when user hits the "apply" button
void VideoApplySettings (void)
{
	StoreCurrentVideoSettings(&mss_previous);

	ApplyVideoSettings(&mss_selected);

	mss_askmode = true;
}

// two possible results of the "keep these video settings?" dialogue
static void KeepNewVideoSettings (void) { mss_askmode = false; }
static void CancelNewVideoSettings (void) {
	mss_askmode = false;
	ApplyVideoSettings(&mss_previous);
}

// this is a duplicate from tr_init.c!
const char* glmodes[] = { "320x240", "400x300", "512x384", "640x480", "800x600", "960x720", "1024x768", "1152x864", "1280x1024", "1600x1200", "2048x1536", "1920x1200", "1920x1080", "1680x1050", "1440x900", "1280x800" };
int glmodes_size = sizeof(glmodes) / sizeof(char*);

const char* BitDepthRead(void) { return mss_selected.bpp == 32 ? "32 bit" : mss_selected.bpp == 16 ? "16 bit" : "use desktop settings"; }
const char* ResolutionRead(void) { return glmodes[bound(0, mss_selected.res, glmodes_size-1)]; }
const char* FullScreenRead(void) { return mss_selected.fullscreen ? "on" : "off"; }

void ResolutionToggle(qbool back) {
	if (back) mss_selected.res--; else mss_selected.res++;
	mss_selected.res = (mss_selected.res + glmodes_size) % glmodes_size;
}
void BitDepthToggle(qbool back) {
	if (back) {
		switch (mss_selected.bpp) {
		case 0: mss_selected.bpp = 32; return;
		case 16: mss_selected.bpp = 0; return;
		default: mss_selected.bpp = 16; return;
		}
	} else {
		switch (mss_selected.bpp) {
		case 0: mss_selected.bpp = 16; return;
		case 16: mss_selected.bpp = 32; return;
		case 32: mss_selected.bpp = 0; return;
		}
	}
}
void FullScreenToggle(qbool back) { mss_selected.fullscreen = mss_selected.fullscreen ? 0 : 1; }

#else
#ifdef _WIN32

qbool mss_software_change_resolution_mode = false;

static void System_ChangeResolution(void)
{
	mss_software_change_resolution_mode = true;
}
#endif
#endif

settings_page settsystem;

void CT_Opt_System_Draw (int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page)
{
#ifndef GLQUAKE	// SOFT
#ifdef _WIN32
	if(mss_software_change_resolution_mode)
	{
		(*vid_menudrawfn) ();
	}
	else
	{
		Settings_Draw(x,y,w,h, &settsystem);
	}
#else
	Settings_Draw(x,y,w,h, &settsystem);
#endif
#else	// GL
	#define ASKBOXWIDTH 300
	if(mss_askmode)
	{
		UI_DrawBox((w-ASKBOXWIDTH)/2, h/2 - 16, ASKBOXWIDTH, 32);
		UI_Print_Center((w-ASKBOXWIDTH)/2, h/2 - 8, ASKBOXWIDTH, "Do you wish to keep these settings?", false);
		UI_Print_Center((w-ASKBOXWIDTH)/2, h/2, ASKBOXWIDTH, "(y/n)", true);
	}
	else
	{
		Settings_Draw(x,y,w,h, &settsystem);
	}
#endif
}

int CT_Opt_System_Key (int key, wchar unichar, CTab_t *tab, CTabPage_t *page)
{
#ifndef GLQUAKE	// SOFT
#ifdef _WIN32
	if(mss_software_change_resolution_mode)
	{

		if(key == K_ESCAPE || key == K_MOUSE2)
		{
			mss_software_change_resolution_mode = false;
		}

		(*vid_menukeyfn) (key);
		// i was too lazy&scared to change vid_menukeyfn functions out there
		// so because i know what keys have some function in there, i list them here:
		return key == K_ESCAPE || key == K_MOUSE2 || key == K_LEFTARROW || key == K_RIGHTARROW || key == K_DOWNARROW || key == K_UPARROW || key == K_ENTER || key == 'd';
	}
	else
	{
		return Settings_Key(&settsystem, key, unichar);
	}
#else
	return Settings_Key(&settsystem, key, unichar);
#endif
#else	// GL
	if (mss_askmode)
	{

		if (key == 'y' || key == K_ENTER)
		{
			KeepNewVideoSettings();
		}
		else if(key == 'n' || key == K_ESCAPE)
		{
			CancelNewVideoSettings();
		}
		return true;
	}
	else
	{
		return Settings_Key(&settsystem, key, unichar);
	}
#endif
}

void OnShow_SettSystem(void)
{
#ifdef GLQUAKE
	StoreCurrentVideoSettings(&mss_selected);
#endif
	Settings_OnShow(&settsystem);
}

qbool CT_Opt_System_Mouse_Event(const mouse_state_t *ms)
{
	return Settings_Mouse_Event(&settsystem, ms);
}


// </SYSTEM>

// *********
// <CONFIG>

// Menu Options Config Page Mode

filelist_t configs_filelist;
cvar_t  cfg_browser_showsize		= {"cfg_browser_showsize",		"1"};
cvar_t  cfg_browser_showdate		= {"cfg_browser_showdate",		"1"};
cvar_t  cfg_browser_showtime		= {"cfg_browser_showtime",		"1"};
cvar_t  cfg_browser_sortmode		= {"cfg_browser_sortmode",		"1"};
cvar_t  cfg_browser_showstatus		= {"cfg_browser_showstatus",	"1"};
cvar_t  cfg_browser_stripnames		= {"cfg_browser_stripnames",	"1"};
cvar_t  cfg_browser_interline		= {"cfg_browser_interline",	"0"};
cvar_t  cfg_browser_scrollnames	= {"cfg_browser_scrollnames",	"1"};
cvar_t	cfg_browser_democolor		= {"cfg_browser_democolor",	"255 255 255 255", CVAR_COLOR};	// White.
cvar_t	cfg_browser_selectedcolor	= {"cfg_browser_selectedcolor","0 150 235 255", CVAR_COLOR};	// Light blue.
cvar_t	cfg_browser_dircolor		= {"cfg_browser_dircolor",		"170 80 0 255", CVAR_COLOR};	// Redish.
#ifdef WITH_ZIP
cvar_t	cfg_browser_zipcolor		= {"cfg_browser_zipcolor",		"255 170 0 255", CVAR_COLOR};	// Orange.
#endif

CEditBox filenameeb;

enum { MOCPM_SETTINGS, MOCPM_CHOOSECONFIG, MOCPM_CHOOSESCRIPT, MOCPM_ENTERFILENAME } MOpt_configpage_mode = MOCPM_SETTINGS;

extern cvar_t cfg_backup, cfg_legacy_exec, cfg_legacy_write, cfg_save_aliases, cfg_save_binds, cfg_save_cmdline,
	cfg_save_cmds, cfg_save_cvars, cfg_save_unchanged, cfg_save_userinfo, cfg_use_home, cfg_save_onquit;

void MOpt_ImportConfig(void) {
	MOpt_configpage_mode = MOCPM_CHOOSECONFIG;
    if (cfg_use_home.value)
        FL_SetCurrentDir(&configs_filelist, com_homedir);
    else
	    FL_SetCurrentDir(&configs_filelist, "./ezquake/configs");
}
void MOpt_ExportConfig(void) {
	MOpt_configpage_mode = MOCPM_ENTERFILENAME;
	filenameeb.text[0] = 0;
	filenameeb.pos = 0;
}

void MOpt_LoadScript(void) {
	MOpt_configpage_mode = MOCPM_CHOOSESCRIPT;
	FL_SetCurrentDir(&configs_filelist, "./ezquake/cfg");
}

void MOpt_CfgSaveAllOn(void) {
	S_LocalSound("misc/basekey.wav");
	Cvar_SetValue(&cfg_backup, 1);
	Cvar_SetValue(&cfg_legacy_exec, 1);
	Cvar_SetValue(&cfg_legacy_write, 0);
	Cvar_SetValue(&cfg_save_aliases, 1);
	Cvar_SetValue(&cfg_save_binds, 1);
	Cvar_SetValue(&cfg_save_cmdline, 1);
	Cvar_SetValue(&cfg_save_cmds, 1);
	Cvar_SetValue(&cfg_save_cvars, 1);
	Cvar_SetValue(&cfg_save_unchanged, 1);
	Cvar_SetValue(&cfg_save_userinfo, 2);
}

const char* MOpt_legacywrite_enum[] = { "off", "non-qw dir frontend.cfg", "also config.cfg", "non-qw config.cfg" };
const char* MOpt_userinfo_enum[] = { "off", "all but player", "all" };

void MOpt_LoadCfg(void) {
	S_LocalSound("misc/basekey.wav");
	Cbuf_AddText("cfg_load\n");
}
void MOpt_SaveCfg(void) { 
	S_LocalSound("doors/runeuse.wav");
	Cbuf_AddText("cfg_save\n");
}

settings_page settconfig;

#define INPUTBOXWIDTH 300
#define INPUTBOXHEIGHT 48

void MOpt_FilenameInputBoxDraw(int x, int y, int w, int h)
{
	UI_DrawBox(x + (w-INPUTBOXWIDTH) / 2, y + (h-INPUTBOXHEIGHT) / 2, INPUTBOXWIDTH, INPUTBOXHEIGHT);
	UI_Print_Center(x, y + h/2 - 8, w, "Enter the config file name", false);
	CEditBox_Draw(&filenameeb, x + (w-INPUTBOXWIDTH)/2 + 16, y + h/2 + 8, true);
}

qbool MOpt_FileNameInputBoxKey(int key)
{
	CEditBox_Key(&filenameeb, key, key);
	return true;
}

char *MOpt_FileNameInputBoxGetText(void)
{
	return filenameeb.text;
}

void CT_Opt_Config_Draw(int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page)
{
	switch (MOpt_configpage_mode) {
	case MOCPM_SETTINGS:
		Settings_Draw(x,y,w,h,&settconfig);
		break;

	case MOCPM_CHOOSESCRIPT:
	case MOCPM_CHOOSECONFIG:
		FL_Draw(&configs_filelist, x,y,w,h);
		break;

	case MOCPM_ENTERFILENAME:
		MOpt_FilenameInputBoxDraw(x,y,w,h);
		break;
	}
}

int CT_Opt_Config_Key(int key, wchar unichar, CTab_t *tab, CTabPage_t *page)
{
	switch (MOpt_configpage_mode) {
	case MOCPM_SETTINGS:
		return Settings_Key(&settconfig, key, unichar);
		break;

	case MOCPM_CHOOSECONFIG:
		if (key == K_ENTER || key == K_MOUSE1) {
			Cbuf_AddText(va("cfg_load \"%s\"\n", COM_SkipPath(FL_GetCurrentEntry(&configs_filelist)->name)));
			MOpt_configpage_mode = MOCPM_SETTINGS;
			return true;
		} else if (key == K_ESCAPE || key == K_MOUSE2) {
			MOpt_configpage_mode = MOCPM_SETTINGS;
			return true;
		} else return FL_Key(&configs_filelist, key);

	case MOCPM_CHOOSESCRIPT:
		if (key == K_ENTER || key == K_MOUSE1) {
			Cbuf_AddText(va("exec \"cfg/%s\"\n", COM_SkipPath(FL_GetCurrentEntry(&configs_filelist)->name)));
			MOpt_configpage_mode = MOCPM_SETTINGS;
			return true;
		} else if (key == K_ESCAPE || key == K_MOUSE2) {
			MOpt_configpage_mode = MOCPM_SETTINGS;
			return true;
		} else return FL_Key(&configs_filelist, key);

	case MOCPM_ENTERFILENAME:
		if (key == K_ENTER || key == K_MOUSE1) {
			Cbuf_AddText(va("cfg_save \"%s\"\n", MOpt_FileNameInputBoxGetText()));
			MOpt_configpage_mode = MOCPM_SETTINGS;
			return true;
        } else if (key == K_ESCAPE || key == K_MOUSE2) {
			MOpt_configpage_mode = MOCPM_SETTINGS;
			return true;
		} else return MOpt_FileNameInputBoxKey(key);
	}

	return false;
}

void OnShow_SettConfig(void) { Settings_OnShow(&settconfig); }

qbool CT_Opt_Config_Mouse_Event(const mouse_state_t *ms)
{
    if (MOpt_configpage_mode == MOCPM_CHOOSECONFIG || MOpt_configpage_mode == MOCPM_CHOOSESCRIPT) {
        if (FL_Mouse_Event(&configs_filelist, ms))
            return true;
        else if (ms->button_up == 1 || ms->button_up == 2)
            return CT_Opt_Config_Key(K_MOUSE1 - 1 + ms->button_up, 0, &options_tab, options_tab.pages + OPTPG_CONFIG);

        return true;
    }
    else
    {
        return Settings_Mouse_Event(&settconfig, ms);
    }
}


// </CONFIG>
// *********

CTabPage_Handlers_t options_misc_handlers = {
	CT_Opt_Settings_Draw,
	CT_Opt_Settings_Key,
	OnShow_SettMain,
	CT_Opt_Settings_Mouse_Event
};

CTabPage_Handlers_t options_player_handlers = {
	CT_Opt_Player_Draw,
	CT_Opt_Player_Key,
	OnShow_SettPlayer,
	CT_Opt_Player_Mouse_Event
};

CTabPage_Handlers_t options_graphics_handlers = {
	CT_Opt_FPS_Draw,
	CT_Opt_FPS_Key,
	OnShow_SettFPS,
	CT_Opt_FPS_Mouse_Event
};

CTabPage_Handlers_t options_view_handlers = {
	CT_Opt_View_Draw,
	CT_Opt_View_Key,
	OnShow_SettView,
	CT_Opt_View_Mouse_Event
};

CTabPage_Handlers_t options_controls_handlers = {
	CT_Opt_Binds_Draw,
	CT_Opt_Binds_Key,
	OnShow_SettBinds,
	CT_Opt_Binds_Mouse_Event
};

CTabPage_Handlers_t options_system_handlers = {
	CT_Opt_System_Draw,
	CT_Opt_System_Key,
	OnShow_SettSystem,
	CT_Opt_System_Mouse_Event
};

CTabPage_Handlers_t options_config_handlers = {
	CT_Opt_Config_Draw,
	CT_Opt_Config_Key,
	OnShow_SettConfig,
	CT_Opt_Config_Mouse_Event
};

void Menu_Options_Key(int key, wchar unichar) {
    int handled = CTab_Key(&options_tab, key, unichar);
	options_unichar = unichar;

	if (!handled && (key == K_ESCAPE || key == K_MOUSE2))
		M_Menu_Main_f();
}

void Menu_Options_Draw(void) {
	int x, y, w, h;

	M_Unscale_Menu();

    // this will add top, left and bottom padding
    // right padding is not added because it causes annoying scrollbar behaviour
    // when mouse gets off the scrollbar to the right side of it
	w = vid.width - OPTPADDING; // here used to be a limit to 512x... size
	h = vid.height - OPTPADDING*2;
	x = OPTPADDING;
	y = OPTPADDING;

	CTab_Draw(&options_tab, x, y, w, h);
}


// PLAYER TAB
setting settplayer_arr[] = {
	ADDSET_BOOL		("Advanced Options", menu_advanced),
	ADDSET_SEPARATOR("Player Settings"),
	ADDSET_STRING	("Name", name),
	ADDSET_STRING	("Teamchat Name", cl_fakename),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_STRING	("Teamchat Name Suffix", cl_fakename_suffix),
	ADDSET_BASIC_SECTION(),
	ADDSET_STRING	("Team", team),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_SKIN		("Skin", skin),
	ADDSET_BASIC_SECTION(),
	ADDSET_COLOR	("Shirt Color", topcolor),
	ADDSET_COLOR	("Pants Color", bottomcolor),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_BOOL		("Fullbright Skins", r_fullbrightSkins),
	ADDSET_ENUM    	("Ruleset", ruleset, ruleset_enum),
	ADDSET_BASIC_SECTION(),
	
	ADDSET_SEPARATOR("Weapon Handling"),
	ADDSET_CUSTOM	("Gun Autoswitch", AutoSWRead, AutoSWToggle, "Switches to the weapon picked up if it is more powerful than what you're currently holding."),
	ADDSET_BOOL		("Gun Preselect", cl_weaponpreselect),
	ADDSET_BOOL		("Gun Auto Hide", cl_weaponhide),
	
    ADDSET_SEPARATOR("Movement"),
	ADDSET_CUSTOM	("Always Run", AlwaysRunRead, AlwaysRunToggle, "Maximum forward speed at all times."),
	ADDSET_ADVANCED_SECTION(),
    ADDSET_BOOL		("Smart Jump", cl_smartjump),
	ADDSET_NAMED	("Movement Scripts", allow_scripts, allowscripts_enum),
	ADDSET_BASIC_SECTION(),
	
	ADDSET_SEPARATOR("Team Skin & Colors"),
	ADDSET_COLOR	("Shirt Color", cl_teamtopcolor),
	ADDSET_COLOR	("Pants Color", cl_teambottomcolor),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_SKIN		("Skin", cl_teamskin),
	ADDSET_NAMED	("Force Skins", teamforceskins, teamforceskins_enum),
	ADDSET_SKIN		("Quad Skin", cl_teamquadskin),
	ADDSET_SKIN		("Pent Skin", cl_teampentskin),
	ADDSET_SKIN		("Quad+Pent Skin", cl_teambothskin),
	ADDSET_BASIC_SECTION(),
	
	ADDSET_SEPARATOR("Enemy Skin & Colors"),
	ADDSET_COLOR	("Shirt Color", cl_enemytopcolor),
	ADDSET_COLOR	("Pants Color", cl_enemybottomcolor),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_SKIN		("Skin", cl_enemyskin),
	ADDSET_NAMED	("Force Skins", enemyforceskins, enemyforceskins_enum),
	ADDSET_SKIN		("Quad Skin", cl_enemyquadskin),
	ADDSET_SKIN		("Pent Skin", cl_enemypentskin),
	ADDSET_SKIN		("Quad+Pent Skin", cl_enemybothskin),
	ADDSET_BASIC_SECTION(),
};

// GRAPHICS TAB
setting settfps_arr[] = {
	ADDSET_BOOL		("Advanced Options", menu_advanced),
	
	ADDSET_SEPARATOR("Presets"),
	ADDSET_CUSTOM	("GFX Preset", GFXPresetRead, GFXPresetToggle, "Select different graphic presets."),

	ADDSET_SEPARATOR("Field Of View"),
	ADDSET_ADVANCED_SECTION(),
#ifdef GLQUAKE
	ADDSET_NUMBER("Draw Distance", r_farclip, 4096, 8192, 4096),
#endif
	ADDSET_BASIC_SECTION(),
	ADDSET_NUMBER	("View Size (fov)", scr_fov, 40, 140, 2),
	ADDSET_NUMBER	("Screen Size", scr_viewsize, 30, 120, 5),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_NUMBER	("Rollangle", cl_rollangle, 0, 30, 2),
	ADDSET_NUMBER	("Rollspeed", cl_rollspeed, 0, 30, 2),
	ADDSET_BOOL		("Gun Kick", v_gunkick),
	ADDSET_NUMBER	("Kick Pitch", v_kickpitch, 0, 10, 0.5),
	ADDSET_NUMBER	("Kick Roll", v_kickroll, 0, 10, 0.5),
	ADDSET_NUMBER	("Kick Time", v_kicktime, 0, 10, 0.5),
	ADDSET_NUMBER	("View Height", v_viewheight, -7, 6, 0.5),
	ADDSET_BASIC_SECTION(),

#ifdef GLQUAKE
	ADDSET_ADVANCED_SECTION(),	
	ADDSET_SEPARATOR("Textures"),
	ADDSET_BOOL		("Luma", gl_lumaTextures),
	ADDSET_ENUM 	("Detail", gl_max_size, gl_max_size_enum),
	ADDSET_NUMBER	("Miptex", gl_miptexLevel, 0, 3, 1),
	ADDSET_BOOL		("No Textures", gl_textureless),
	ADDSET_BASIC_SECTION(),
#endif	
	
	ADDSET_SEPARATOR("Player & Weapon Model"),
	ADDSET_ADVANCED_SECTION(),
#ifdef GLQUAKE
	ADDSET_BOOL		("Powerup Luma", gl_powerupshells),
	ADDSET_NUMBER	("Powerup Luma Size", gl_powerupshells_size, 0, 10, 1),
	ADDSET_NUMBER	("Weapon Opacity", cl_drawgun, 0, 1, 0.05),
#else
	ADDSET_BOOL		("Weapon Show", cl_drawgun),
#endif
	ADDSET_BASIC_SECTION(),
	ADDSET_NUMBER	("Weapon Size", r_viewmodelsize, 0.1, 1, 0.05),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_NUMBER	("Weapon Shift", r_viewmodeloffset, -10, 10, 1),
	ADDSET_NAMED	("Weapon Muzzleflashes", cl_muzzleflash, muzzleflashes_enum),
	ADDSET_BASIC_SECTION(),
	
	ADDSET_SEPARATOR("Environment"),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_BOOL		("Fullbright World", r_fullbright),
	ADDSET_BOOL		("Simple Sky", r_fastsky),
	ADDSET_BOOL		("Simple Walls", r_drawflat),
	ADDSET_BOOL		("Simple Turbs", r_fastturb),
#ifdef GLQUAKE
	ADDSET_BOOL		("Simple Items", gl_simpleitems),
	ADDSET_NAMED	("Simple Items Orientation", gl_simpleitems_orientation, simpleitemsorientation_enum),
#endif
	ADDSET_BOOL		("Draw Flame", r_drawflame),
	ADDSET_BOOL		("Backpack Filter", cl_backpackfilter),
	ADDSET_BASIC_SECTION(),
	ADDSET_BOOL		("Gib Filter", cl_gibfilter),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_NAMED	("Dead Body Filter", cl_deadbodyfilter, deadbodyfilter_enum),
	ADDSET_BASIC_SECTION(),
	
	ADDSET_SEPARATOR("Projectiles"),
	ADDSET_NAMED	("Explosion Type", r_explosiontype, explosiontype_enum),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_NAMED	("Rocket Model", cl_rocket2grenade, rocketmodel_enum),
	ADDSET_BASIC_SECTION(),
	ADDSET_NAMED	("Rocket Trail", r_rockettrail, rockettrail_enum),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_BOOL		("Rocket Light", r_rocketlight),
	ADDSET_NAMED	("Grenade Trail", r_grenadetrail, grenadetrail_enum),
	ADDSET_BASIC_SECTION(),
	ADDSET_NUMBER	("Fakeshaft", cl_fakeshaft, 0, 1, 0.05),
#ifdef GLQUAKE
	ADDSET_ADVANCED_SECTION(),
	ADDSET_BOOL		("Hide Nails", amf_hidenails),
	ADDSET_BOOL		("Hide Rockets", amf_hiderockets),
	ADDSET_BASIC_SECTION(),
#endif

	ADDSET_SEPARATOR("Lighting"),
#ifdef GLQUAKE
	ADDSET_BOOL		("GL Bloom", r_bloom),
#endif
	ADDSET_NAMED	("Powerup Glow", r_powerupglow, powerupglow_enum),
	ADDSET_NUMBER	("Damage Flash", v_damagecshift, 0, 1, 0.1),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_BOOL		("Pickup Flash", v_bonusflash),
	ADDSET_BASIC_SECTION(),
#ifdef GLQUAKE
	ADDSET_BOOL		("Colored Lights", gl_colorlights),
	ADDSET_BOOL		("Fast Lights", gl_flashblend),
	ADDSET_BOOL		("Dynamic Lights", r_dynamic),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_NUMBER	("Light Mode", gl_lightmode, 0, 2, 1),
	ADDSET_BOOL		("Particle Shaft", amf_lightning),
	ADDSET_BASIC_SECTION(),
#endif
};

// VIEW TAB
setting settview_arr[] = {
	ADDSET_BOOL		("Advanced Options", menu_advanced),
	ADDSET_SEPARATOR("Head Up Display"),
	ADDSET_NAMED	("HUD Type", scr_newHud, hud_enum),
	ADDSET_NUMBER	("Crosshair", crosshair, 0, 7, 1),
	ADDSET_NUMBER	("Crosshair Size", crosshairsize, 0.2, 3, 0.2),
#ifdef GLQUAKE
	ADDSET_ADVANCED_SECTION(),
	ADDSET_NUMBER	("Crosshair Alpha", gl_crosshairalpha, 0.1, 1, 0.1),
	ADDSET_NAMED	("Overhead Name", scr_autoid, scrautoid_enum),
	ADDSET_BASIC_SECTION(),
#endif

	ADDSET_SEPARATOR("New HUD"),
	ADDSET_BOOLLATE	("Gameclock", hud_gameclock_show),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_BOOLLATE ("Big Gameclock", hud_gameclock_big),
	ADDSET_BASIC_SECTION(),
#ifdef GLQUAKE
	ADDSET_BOOL		("Teaminfo Table", scr_teaminfo),
#endif
	ADDSET_ADVANCED_SECTION(),
#ifdef GLQUAKE
	ADDSET_BOOLLATE ("Own Frags Announcer", hud_ownfrags_show),
#endif
	ADDSET_BOOLLATE ("Teamholdbar", hud_teamholdbar_show),
	ADDSET_BOOLLATE ("Teamholdinfo", hud_teamholdinfo_show),
	ADDSET_BOOLLATE ("Clock", hud_clock_show),
	ADDSET_BASIC_SECTION(),
	ADDSET_BOOLLATE ("FPS", hud_fps_show),
#ifdef GLQUAKE
	ADDSET_ADVANCED_SECTION(),
	ADDSET_BOOLLATE ("Radar", hud_radar_show),
	ADDSET_BASIC_SECTION(),
#endif

	ADDSET_SEPARATOR("Quake Classic HUD"),
	ADDSET_BOOL		("Status Bar", cl_sbar),
	ADDSET_BOOL		("HUD Left", cl_hudswap),
	ADDSET_BOOL		("Show FPS", show_fps),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_BOOL		("Show Clock", scr_clock),
	ADDSET_BASIC_SECTION(),
	ADDSET_BOOL		("Show Gameclock", scr_gameclock),

#ifdef GLQUAKE
	ADDSET_SEPARATOR("Tracker Messages"),
	ADDSET_NUMBER	("Messages", amf_tracker_messages, 0, 10, 1),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_BOOL		("Flags", amf_tracker_flags),
	ADDSET_NAMED	("Frags", amf_tracker_frags, amf_tracker_frags_enum),
	ADDSET_BOOL		("Streaks", amf_tracker_streaks),
	ADDSET_NUMBER	("Time", amf_tracker_time, 0.5, 6, 0.5),
	ADDSET_NUMBER	("Scale", amf_tracker_scale, 0.1, 2, 0.1),
	ADDSET_BOOL		("Use Images", cl_useimagesinfraglog),
	ADDSET_BOOL		("Align Right", amf_tracker_align_right),
	ADDSET_BASIC_SECTION(),
#endif

	ADDSET_ADVANCED_SECTION(),
	ADDSET_SEPARATOR("Message Filtering"),
	ADDSET_NAMED	("Ignore Opponents", ignore_opponents, ignoreopponents_enum),
	ADDSET_NAMED	("Ignore Spectators", ignore_spec, ignorespec_enum),
	ADDSET_BOOL		("Ignore Qizmo Observers", ignore_qizmo_spec),
	ADDSET_ENUM 	("Ignore Flood", ignore_flood, ignore_flood_enum),
	ADDSET_NUMBER 	("Ignore Flood Duration", ignore_flood_duration, 0, 10, 1),
	ADDSET_NAMED	("Message Filtering", msg_filter, msgfilter_enum),

	ADDSET_SEPARATOR("Outgoing Filtering"),
	ADDSET_BOOL		("Filter Colored Text", cl_sayfilter_coloredtext),
	ADDSET_BOOL		("Send #u/#c Versions", cl_sayfilter_sendboth),

	ADDSET_SEPARATOR("Console Options"),
	ADDSET_BOOL		("Timestamps", con_timestamps),
	ADDSET_NAMED	("Chat Mode", cl_chatmode, cl_chatmode_enum),
	ADDSET_NAMED	("Completion Format", con_completion_format, con_completion_format_enum),
	ADDSET_NUMBER	("Size", scr_consize, 0, 1, 0.1),
	ADDSET_NUMBER	("Speed", scr_conspeed, 1000, 9999, 1000),
#ifdef GLQUAKE
	ADDSET_NUMBER	("Alpha", scr_conalpha, 0, 1, 0.1),
	ADDSET_NAMED	("Map Preview", scr_conback, scr_conback_enum),
#endif
	ADDSET_NAMED	("Fun Chars Mode", con_funchars_mode, funcharsmode_enum),
	ADDSET_NAMED	("Colored Text", scr_coloredText, coloredtext_enum),
	ADDSET_NUMBER	("Notify Time", con_notifytime, 0.5, 16, 0.5),
	ADDSET_NUMBER	("Notify Lines", _con_notifylines, 0, 16, 1),
	ADDSET_BOOL		("Confirm Quit", cl_confirmquit),

	ADDSET_SEPARATOR("Demo & Observing"),
	ADDSET_BOOL		("Chasecam", cl_chasecam),

	ADDSET_BASIC_SECTION(),
	ADDSET_SEPARATOR("Demo Playback"),
	ADDSET_NUMBER	("Multiview", cl_multiview, 0, 4, 1),
	ADDSET_BOOL		("Display HUD", cl_mvdisplayhud),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_BOOL		("HUD Flip", cl_mvhudflip),
	ADDSET_BOOL		("HUD Vertical", cl_mvhudvertical),
	ADDSET_BOOL		("Inset View", cl_mvinset),
	ADDSET_BOOL		("Inset HUD", cl_mvinsethud),
	ADDSET_BOOL		("Inset Cross", cl_mvinsetcrosshair),
	ADDSET_BASIC_SECTION(),
	
	ADDSET_SEPARATOR("Multiview Demos"),
	ADDSET_ACTION	("Toggle Autotrack", CL_Autotrack_f, "Toggle auto-tracking of the best player"),
	ADDSET_NAMED	("Autohud", mvd_autohud, mvdautohud_enum),
	ADDSET_NAMED	("Autotrack Type", mvd_autotrack, mvdautotrack_enum),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_BOOL		("Autotrack Lock Team", mvd_autotrack_lockteam),
	ADDSET_BASIC_SECTION(),
	ADDSET_BOOL		("Moreinfo", mvd_moreinfo),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_BOOL     ("Status", mvd_status),
	ADDSET_BASIC_SECTION(),
};

// CONTROLS TAB
// please try to put mostly binds in here
setting settbinds_arr[] = {
	ADDSET_BOOL		("Advanced Options", menu_advanced),
	
	ADDSET_SEPARATOR("Mouse Settings"),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_BOOL		("Freelook", freelook),
	ADDSET_BASIC_SECTION(),
	ADDSET_NUMBER	("Sensitivity", sensitivity, 1, 20, 0.25), // My sens is 16, so maybe some people have it up to 20?
	ADDSET_ADVANCED_SECTION(),
	ADDSET_NUMBER	("Menu Mouse Sensitivity", cursor_sensitivity, 0.10 , 3, 0.10),
	ADDSET_NUMBER	("Acceleration", m_accel, 0, 1, 0.1),
	ADDSET_BASIC_SECTION(),
	ADDSET_CUSTOM	("Invert Mouse", InvertMouseRead, InvertMouseToggle, "Inverts the Y axis."),
    ADDSET_ADVANCED_SECTION(),
    ADDSET_STRING   ("X-axis Sensitivity", m_yaw),
    ADDSET_STRING   ("Y-axis Sensitivity", m_pitch),
// maybe its okay for FreeBSD and MAC OS X too
#if defined(_WIN32) || (defined(__linux__) && defined(GLQUAKE))
	ADDSET_NAMED    ("Mouse Input", in_mouse, in_mouse_enum),
#endif
#ifdef _WIN32
	ADDSET_STRING   ("DInput: Rate (Hz)", m_rate),
    ADDSET_BOOL     ("DInput: Smoothing", in_m_smooth),
	ADDSET_NAMED    ("OS Mouse: Parms.", in_m_os_parameters, in_m_os_parameters_enum),
#endif
    ADDSET_ACTION   ("Apply", Menu_Input_Restart, "Will restart the mouse input module and apply settings."),
    ADDSET_BASIC_SECTION(),

	ADDSET_SEPARATOR("Movement"),
	ADDSET_BIND("Attack", "+attack"),
	ADDSET_BIND("Jump/Swim up", "+jump"),
	ADDSET_BIND("Move Forward", "+forward"),
	ADDSET_BIND("Move Backward", "+back"),
	ADDSET_BIND("Strafe Left", "+moveleft"),
	ADDSET_BIND("Strafe Right", "+moveright"),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_BIND("Swim Up", "+moveup"),
	ADDSET_BIND("Swim Down", "+movedown"),
	ADDSET_BIND("Zoom In/Out", "+zoom"),
	ADDSET_BASIC_SECTION(),

	ADDSET_SEPARATOR("Weapons"),
	ADDSET_BIND("Previous Weapon", "impulse 12"),
	ADDSET_BIND("Next Weapon", "impulse 10"),
	ADDSET_BIND("Axe", "weapon 1"),
	ADDSET_BIND("Shotgun", "weapon 2"),
	ADDSET_BIND("Super Shotgun", "weapon 3"),
	ADDSET_BIND("Nailgun", "weapon 4"),
	ADDSET_BIND("Super Nailgun", "weapon 5"),
	ADDSET_BIND("Grenade Launcher", "weapon 6"),
	ADDSET_BIND("Rocket Launcher", "weapon 7"),
	ADDSET_BIND("Thunderbolt", "weapon 8"),

	ADDSET_SEPARATOR("Chat settings"),
	ADDSET_BIND	("Chat", "messagemode"),
	ADDSET_BIND	("Teamchat", "messagemode2"),

	ADDSET_SEPARATOR("Miscellaneous"),
	ADDSET_BIND("Show Scores", "+showscores"),
	ADDSET_BIND("Screenshot", "screenshot"),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_BIND("Pause", "pause"),
	ADDSET_BIND("Quit", "quit"),
	ADDSET_BIND("Proxy Menu", "toggleproxymenu"),
	ADDSET_BASIC_SECTION(),

	ADDSET_SEPARATOR("Demo & Spec"),
	ADDSET_BIND("Demo Controls", "demo_controls"),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_BIND("Autotrack", "autotrack"),
	ADDSET_BIND("Play", "cl_demospeed 1;echo Playing demo."),
	ADDSET_BIND("Stop", "disconnect"),
	ADDSET_BIND("Pause", "cl_demospeed 0;echo Demo paused."),
	ADDSET_BIND("Fast Forward", "cl_demospeed 5;echo Demo paused."),
	ADDSET_BASIC_SECTION(),

	ADDSET_SEPARATOR("Teamplay"),
	ADDSET_BIND("Report Status", "tp_msgreport"),
	ADDSET_BIND("Lost Location", "tp_msglost"),
	ADDSET_BIND("Location Safe", "tp_msgsafe"),
	ADDSET_BIND("Point At Item", "tp_msgpoint"),
	ADDSET_BIND("Took Item", "tp_msgtook"),
	ADDSET_BIND("Need Items", "tp_msgneed"),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_BIND("Yes/Ok", "tp_msgyesok"),
	ADDSET_BIND("Coming From Location", "tp_msgcoming"),
	ADDSET_BASIC_SECTION(),
	ADDSET_BIND("Help Location", "tp_msghelp"),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_BIND("Get Quad", "tp_msggetquad"),
	ADDSET_BIND("Get Pent", "tp_msggetpent"),
	ADDSET_BIND("Enemy Quad Dead", "tp_msgquaddead"),
	ADDSET_BIND("Enemy Has Powerup", "tp_msgenemypwr"),
	ADDSET_BIND("Trick At Location", "tp_msgtrick"),
	ADDSET_BIND("Replace At Location", "tp_msgreplace"),
	ADDSET_BIND("You Take Item", "tp_msgutake"),
	ADDSET_BIND("Waiting", "tp_msgwaiting"),
	ADDSET_BIND("Enemy Slipped", "tp_msgslipped"),
	ADDSET_BASIC_SECTION(),
};

// MISC TAB
setting settmisc_arr[] = {
	ADDSET_BOOL		("Advanced Options", menu_advanced),

	//Match Tools
	ADDSET_SEPARATOR("Match Tools"),
	ADDSET_BOOL		("Auto Screenshot", match_auto_sshot),
	ADDSET_ENUM 	("Screenshot Format", scr_sshot_format, scr_sshot_format_enum),
	ADDSET_NAMED	("Auto Record Demo", match_auto_record, autorecord_enum),
#ifdef _WIN32
	ADDSET_ENUM     ("Demo Format", demo_format, demoformat_enum),
#endif
	ADDSET_NAMED	("Auto Log Match", match_auto_logconsole, autorecord_enum),
	ADDSET_BOOL		("Log Readable", log_readable),

	//Paths
	ADDSET_SEPARATOR("Paths"),
	ADDSET_NAMED    ("Media Paths Type", cl_mediaroot, mediaroot_enum),
	ADDSET_STRING   ("Screenshots Path", scr_sshot_dir),
	ADDSET_STRING	("Demos Path", demo_dir),
	ADDSET_STRING   ("Logs Path", log_dir),
	ADDSET_STRING	("Qizmo Path", qizmo_dir),
	ADDSET_STRING	("QWDTools Path", qwdtools_dir),
#ifdef _WIN32
	ADDSET_ACTION	("Set qw:// assoc.", CL_RegisterQWURLProtocol_f,
		"Sets this application as the handler of qw:// URLs, so by double-clicking such links in your operating system, this client will open and connect to given address"),
#endif

	ADDSET_ADVANCED_SECTION(),
	ADDSET_SEPARATOR	("Miscellaneous"),
	ADDSET_NAMED		("Linear Interpolation", cl_nolerp, cl_nolerp_enum),
	ADDSET_BOOL			("In-Game Menu", menu_ingame),
	ADDSET_BASIC_SECTION(),
};

// SYSTEM TAB
setting settsystem_arr[] = {
	ADDSET_BOOL		("Advanced Options", menu_advanced),

	//Video
	ADDSET_SEPARATOR("Video"),
	ADDSET_NUMBER	("Gamma", v_gamma, 0.1, 2.0, 0.1),
	ADDSET_NUMBER	("Contrast", v_contrast, 1, 5, 0.1),
#ifdef GLQUAKE
	ADDSET_ADVANCED_SECTION(),
	ADDSET_BOOL		("Clear Video Buffer", gl_clear),
	ADDSET_NUMBER	("Anisotropy Filter", gl_anisotropy, 0, 16, 1),
	ADDSET_ENUM		("Quality Mode", gl_texturemode, gl_texturemode_enum),
	ADDSET_BASIC_SECTION(),
#endif

#if !defined(__APPLE__) && !defined(_Soft_X11) && !defined(_Soft_SVGA)
	ADDSET_SEPARATOR("Screen Settings"),
#ifdef GLQUAKE
	ADDSET_CUSTOM("Resolution", ResolutionRead, ResolutionToggle, "Change your screen resolution."),
	ADDSET_BOOL("Wide Aspect", vid_wideaspect),
	ADDSET_BOOL("Vertical Sync", r_swapInterval),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_BOOL("Vsync Lag Fix", cl_vsync_lag_fix),
	ADDSET_BASIC_SECTION(),
	ADDSET_CUSTOM("Bit Depth", BitDepthRead, BitDepthToggle, "Choose 16bit or 32bit color mode for your screen."),
	ADDSET_CUSTOM("Fullscreen", FullScreenRead, FullScreenToggle, "Toggle between fullscreen and windowed mode."),
	ADDSET_STRING("Refresh Frequency", mss_selected.freq),
	ADDSET_ACTION("Apply Changes", VideoApplySettings, "Restarts the renderer and applies the selected resolution."),
#else
#ifdef _WIN32
	ADDSET_ACTION("Change Resolution", System_ChangeResolution, "Change Display Resolution"),
#endif
#endif
#endif

	//Font
#ifdef GLQUAKE
	ADDSET_ADVANCED_SECTION(),
	ADDSET_SEPARATOR("Font"),
#ifndef __APPLE__
	ADDSET_NUMBER("Width", r_conwidth, 320, 2048, 8),
	ADDSET_NUMBER("Height", r_conheight, 240, 1538, 4),
#endif
	ADDSET_BOOL("Font Smoothing", gl_smoothfont),
	ADDSET_BASIC_SECTION(),
#endif

	//Sound & Volume
	ADDSET_SEPARATOR("Sound & Volume"),
	ADDSET_NUMBER	("Primary Volume", s_volume, 0, 1, 0.05),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_BOOL		("Self Volume Levels", cl_chatsound),
	ADDSET_NUMBER	("Chat Volume", con_sound_mm1_volume, 0, 1, 0.1),
	ADDSET_NUMBER	("Team Chat Volume", con_sound_mm2_volume, 0, 1, 0.1),
	ADDSET_NUMBER	("Spectator Volume", con_sound_spec_volume, 0, 1, 0.1),
	ADDSET_NUMBER	("Other Volume", con_sound_other_volume, 0, 1, 0.1),
	ADDSET_BOOL		("Static Sounds", cl_staticsounds),
#ifdef _WIN32
	ADDSET_BOOL		("Sounds When Minimized", sys_inactivesound),
#endif
	ADDSET_BASIC_SECTION(),
	ADDSET_ENUM 	("Quality", s_khz, s_khz_enum),

	//Connection
	ADDSET_SEPARATOR("Connection"),
	ADDSET_ENUM 	("Bandwidth Limit", rate, bandwidth_enum),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_BOOL		("Early Packets", cl_earlypackets),
	ADDSET_ENUM		("Packetloss", cl_c2sImpulseBackup, cl_c2sImpulseBackup_enum),
	ADDSET_BOOL		("QTV Buffer Adjusting", qtv_adjustbuffer),
	ADDSET_BASIC_SECTION(),

	ADDSET_ADVANCED_SECTION(),
	ADDSET_SEPARATOR("Miscellaneous"),
	ADDSET_CUSTOM	("FPS Limit", FpslimitRead, FpslimitToggle, "Limits the amount of frames rendered per second. May help with lag; best to consult forums about the best value for your setup."),
#ifdef _WIN32
	ADDSET_ENUM		("Process Priority", sys_highpriority, priority_enum),
	ADDSET_BOOL		("Taskbar Flash", vid_flashonactivity),
	ADDSET_BOOL		("Taskbar Name", cl_window_caption),
#endif
	ADDSET_BASIC_SECTION(),
};

// CONFIG TAB
setting settconfig_arr[] = {
	ADDSET_BOOL		("Advanced Options", menu_advanced),
	ADDSET_ACTION	("Go To Console", Con_ToggleConsole_f, "Opens the console."),

    ADDSET_SEPARATOR("Load & Save"),
    ADDSET_ACTION("Reload Settings", MOpt_LoadCfg, "Reset the settings to last saved configuration."),
    ADDSET_ACTION("Save Settings", MOpt_SaveCfg, "Save the settings"),
	ADDSET_ADVANCED_SECTION(),
    ADDSET_BOOL("Save To Profile Dir", cfg_use_home),
    ADDSET_BASIC_SECTION(),
	ADDSET_ACTION("Reset To Defaults", DefaultConfig, "Reset all settings to defaults"),

    
	ADDSET_SEPARATOR("Export & Import"),
	ADDSET_ACTION("Load Script", MOpt_LoadScript, "Find and load scripts here."),
	ADDSET_ACTION("Import config ...", MOpt_ImportConfig, "You can load a configuration file from here."),
	ADDSET_ACTION("Export config ...", MOpt_ExportConfig, "Will export your current configuration to a file."),
	
	ADDSET_SEPARATOR("Config Saving Options"),
    ADDSET_ACTION("Reset Saving Options", MOpt_CfgSaveAllOn, "Configuration saving settings will be reset to defaults."),
    ADDSET_BOOL("Auto Save On Quit", cfg_save_onquit),
	ADDSET_BOOL("Save Unchanged Opt.", cfg_save_unchanged),
	ADDSET_ADVANCED_SECTION(),
	ADDSET_BOOL("Backup Old File", cfg_backup),
	ADDSET_NAMED("Load Legacy", cfg_legacy_exec, MOpt_legacywrite_enum),
	ADDSET_BOOL("Save Legacy", cfg_legacy_write),
	ADDSET_BOOL("Aliases", cfg_save_aliases),
	ADDSET_BOOL("Binds", cfg_save_binds),
	ADDSET_BOOL("Cmdline", cfg_save_cmdline),
	ADDSET_BOOL("Init Commands", cfg_save_cmds),
	ADDSET_BOOL("Variables", cfg_save_cvars),
	ADDSET_NAMED("Userinfo", cfg_save_userinfo, MOpt_userinfo_enum)
};

qbool Menu_Options_Mouse_Event(const mouse_state_t *ms)
{
	mouse_state_t nms = *ms;

    if (ms->button_up == 2) {
        Menu_Options_Key(K_MOUSE2, 0);
        return true;
    }

	// we are sending relative coordinates
	nms.x -= OPTPADDING;
	nms.y -= OPTPADDING;
	nms.x_old -= OPTPADDING;
	nms.y_old -= OPTPADDING;

	if (nms.x < 0 || nms.y < 0) return false;

	return CTab_Mouse_Event(&options_tab, &nms);
}
void Menu_Options_Init(void) {
	Settings_MainInit();

	Settings_Page_Init(settmisc, settmisc_arr);
	Settings_Page_Init(settfps, settfps_arr);
	Settings_Page_Init(settview, settview_arr);
	Settings_Page_Init(settplayer, settplayer_arr);
	Settings_Page_Init(settbinds, settbinds_arr);
	Settings_Page_Init(settsystem, settsystem_arr);
	Settings_Page_Init(settconfig, settconfig_arr);

	Cvar_Register(&menu_advanced);
#ifdef GLQUAKE
	// this is here just to not get a crash in Cvar_Set
    mss_selected.freq.name = "menu_tempval_video_freq";
	mss_previous.freq.name = mss_selected.freq.name;
    mss_selected.freq.string = NULL;
    mss_previous.freq.string = NULL;
    mss_selected.freq.next = &mss_selected.freq;
    mss_previous.freq.next = &mss_previous.freq;
#endif

	Cvar_SetCurrentGroup(CVAR_GROUP_CONFIG);
	Cvar_Register(&cfg_browser_showsize);
    Cvar_Register(&cfg_browser_showdate);
    Cvar_Register(&cfg_browser_showtime);
    Cvar_Register(&cfg_browser_sortmode);
    Cvar_Register(&cfg_browser_showstatus);
    Cvar_Register(&cfg_browser_stripnames);
    Cvar_Register(&cfg_browser_interline);
	Cvar_Register(&cfg_browser_scrollnames);
	Cvar_Register(&cfg_browser_selectedcolor);
	Cvar_Register(&cfg_browser_democolor);
	Cvar_Register(&cfg_browser_dircolor);
#ifdef WITH_ZIP
	Cvar_Register(&cfg_browser_zipcolor);
#endif
	Cvar_ResetCurrentGroup();


	FL_Init(&configs_filelist,
        &cfg_browser_sortmode,
        &cfg_browser_showsize,
        &cfg_browser_showdate,
        &cfg_browser_showtime,
        &cfg_browser_stripnames,
        &cfg_browser_interline,
        &cfg_browser_showstatus,
		&cfg_browser_scrollnames,
		&cfg_browser_democolor,
		&cfg_browser_selectedcolor,
		&cfg_browser_dircolor,
#ifdef WITH_ZIP
		&cfg_browser_zipcolor,
#endif
		"./ezquake/configs");
	FL_SetDirUpOption(&configs_filelist, false);
	FL_SetDirsOption(&configs_filelist, false);
	FL_AddFileType(&configs_filelist, 0, ".cfg");
	FL_AddFileType(&configs_filelist, 1, ".txt");

	CEditBox_Init(&filenameeb, 32, 64);

	CTab_Init(&options_tab);
	CTab_AddPage(&options_tab, "Player", OPTPG_PLAYER, &options_player_handlers);
	CTab_AddPage(&options_tab, "Graphics", OPTPG_FPS, &options_graphics_handlers);
	CTab_AddPage(&options_tab, "View", OPTPG_HUD, &options_view_handlers);
	CTab_AddPage(&options_tab, "Controls", OPTPG_BINDS, &options_controls_handlers);
	CTab_AddPage(&options_tab, "Misc", OPTPG_MISC, &options_misc_handlers);
	CTab_AddPage(&options_tab, "System", OPTPG_SYSTEM, &options_system_handlers);
	CTab_AddPage(&options_tab, "Config", OPTPG_CONFIG, &options_config_handlers);
	CTab_SetCurrentId(&options_tab, OPTPG_PLAYER);
}
