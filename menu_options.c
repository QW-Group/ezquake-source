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
	last edit:
		$Id: menu_options.c,v 1.15 2007-01-14 14:44:36 johnnycz Exp $

*/

#include "quakedef.h"
#include "settings.h"
#include "settings_page.h"

typedef enum {
	OPTPG_SETTINGS,
	OPTPG_PLAYER,
	OPTPG_FPS,
	OPTPG_HUD,
	OPTPG_MULTIVIEW,
	OPTPG_BINDS,
	OPTPG_VIDEO,
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
extern cvar_t scr_scaleMenu;


//=============================================================================
// <SETTINGS>

#ifdef GLQUAKE
enum {mode_fastest, mode_faithful, mode_eyecandy, mode_movies, mode_undef} fps_mode = mode_undef;
#else
enum {mode_fastest, mode_default, mode_undef} fps_mode = mode_default;
#endif

extern cvar_t scr_fov, scr_newHud, cl_staticsounds, r_fullbrightSkins, cl_deadbodyfilter, cl_muzzleflash;
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
		case 1: fps_mode = cl_muzzleflash.value ? mode_faithful : mode_movies; break;
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
	case mode_eyecandy: return "eyecandy";
	case mode_faithful: return "faithful";
	default: return "movies";
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
	case mode_eyecandy: Cbuf_AddText ("exec cfg/gfx_gl_eyecandy.cfg\n"); return;
	case mode_faithful: Cbuf_AddText ("exec cfg/gfx_gl_faithful.cfg\n"); return;
	case mode_movies: Cbuf_AddText ("exec cfg/gfx_gl_movies.cfg\n"); return;
#else
	case mode_fastest: Cbuf_AddText ("exec cfg/gfx_sw_fast.cfg\n"); return;
	case mode_default: Cbuf_AddText ("exec cfg/gfx_sw_default.cfg\n"); return;
#endif
	}
}

const char* mvdautohud_enum[] = { "off", "simple", "customizable" };
const char* mvdautotrack_enum[] = { "off", "auto", "custom", "multitrack" };
const char* funcharsmode_enum[] = { "ctrl+key", "ctrl+y" };
const char* ignoreopponents_enum[] = { "off", "always", "on match" };
const char* msgfilter_enum[] = { "off", "say+spec", "team", "say+team+spec" };
const char* allowscripts_enum[] = { "off", "simple", "all" };
const char* scrautoid_enum[] = { "off", "nick", "health+armor", "health+armor+type", "all (rl)", "all (best gun)" };
const char* coloredtext_enum[] = { "off", "simple", "frag messages" };
const char* autorecord_enum[] = { "off", "don't save", "auto save" };
const char* hud_enum[] = { "old", "new", "combined" };

const char* SshotformatRead(void) {
	return scr_sshot_format.string;
}
void SshotformatToggle(qbool back) {
	if (!strcmp(scr_sshot_format.string, "jpg")) Cvar_Set(&scr_sshot_format, "png");
	else if (!strcmp(scr_sshot_format.string, "png")) Cvar_Set(&scr_sshot_format, "tga");
	else if (!strcmp(scr_sshot_format.string, "tga")) Cvar_Set(&scr_sshot_format, "jpg");
}

extern cvar_t mvd_autotrack, mvd_moreinfo, mvd_status, cl_weaponpreselect, cl_weaponhide, con_funchars_mode, con_notifytime, scr_consize, ignore_opponents, _con_notifylines,
	ignore_qizmo_spec, ignore_spec, msg_filter, sys_highpriority, crosshair, crosshairsize, cl_smartjump, scr_coloredText,
	cl_rollangle, cl_rollspeed, v_gunkick, v_kickpitch, v_kickroll, v_kicktime, v_viewheight, match_auto_sshot, match_auto_record, match_auto_logconsole,
	r_fastturb, r_grenadetrail, cl_drawgun, r_viewmodelsize, r_viewmodeloffset, scr_clock, scr_gameclock, show_fps, rate, cl_c2sImpulseBackup,
	name, team, skin, topcolor, bottomcolor, cl_teamtopcolor, cl_teambottomcolor, cl_teamquadskin, cl_teampentskin, cl_teambothskin, /*cl_enemytopcolor, cl_enemybottomcolor, */
	cl_enemyquadskin, cl_enemypentskin, cl_enemybothskin, demo_dir, qizmo_dir, qwdtools_dir, cl_fakename,
	cl_chatsound, con_sound_mm1_volume, con_sound_mm2_volume, con_sound_spec_volume, con_sound_other_volume
;
#ifdef _WIN32
extern cvar_t demo_format;
#endif
#ifdef GLQUAKE
extern cvar_t scr_autoid, gl_smoothfont, amf_hidenails, amf_hiderockets, gl_anisotropy, gl_lumaTextures, gl_textureless, gl_colorlights;
#endif

const char* BandwidthRead(void) {
	if (rate.value < 4000) return "Modem (33k)";
	else if (rate.value < 6000) return "Modem (56k)";
	else if (rate.value < 8000) return "ISDN (112k)";
	else if (rate.value < 15000) return "Cable (128k)";
	else return "ADSL (> 256k)";
}
void BandwidthToggle(qbool back) {
	if (rate.value < 4000) Cvar_SetValue(&rate, 5670);
	else if (rate.value < 6000) Cvar_SetValue(&rate, 7168);
	else if (rate.value < 8000) Cvar_SetValue(&rate, 14336);
	else if (rate.value < 15000) Cvar_SetValue(&rate, 30000);
	else Cvar_SetValue(&rate, 3800);
}
const char* ConQualityRead(void) {
	int q = (int) cl_c2sImpulseBackup.value;
	if (!q) return "Perfect";
	else if (q < 3) return "Low packetloss";
	else if (q < 5) return "Medium packetloss";
	else return "High packetloss";
}
void ConQualityToggle(qbool back) {
	int q = (int) cl_c2sImpulseBackup.value;
	if (!q) Cvar_SetValue(&cl_c2sImpulseBackup, 2);
	else if (q < 3) Cvar_SetValue(&cl_c2sImpulseBackup, 4);
	else if (q < 5) Cvar_SetValue(&cl_c2sImpulseBackup, 6);
	else Cvar_SetValue(&cl_c2sImpulseBackup, 0);
}
#ifdef _WIN32
const char* DemoformatRead(void) {
	return demo_format.string;
}
void DemoformatToggle(qbool back) {
	if (!strcmp(demo_format.string, "qwd")) Cvar_Set(&demo_format, "qwz");
	else if (!strcmp(demo_format.string, "qwz")) Cvar_Set(&demo_format, "mvd");
	else if (!strcmp(demo_format.string, "mvd")) Cvar_Set(&demo_format, "qwd");
}
#endif

void DefaultConfig(void) { Cbuf_AddText("cfg_reset\n"); }

setting settgeneral_arr[] = {
	ADDSET_SEPARATOR("Miscellaneous"),
	ADDSET_ACTION	("Go To Console", Con_ToggleConsole_f),
	ADDSET_ACTION	("Default Config", DefaultConfig),
	ADDSET_SEPARATOR("Video"),
	ADDSET_NUMBER	("Gamma", v_gamma, 0.1, 2.0, 0.1),
	ADDSET_NUMBER	("Contrast", v_contrast, 1, 5, 0.1),
	ADDSET_NUMBER	("Screen Size", scr_viewsize, 30, 120, 5),
	ADDSET_NUMBER	("Field of View", scr_fov, 40, 140, 2),
	ADDSET_NUMBER	("Process Priority", sys_highpriority, -1, 1, 1),
	ADDSET_CUSTOM	("GFX Preset", GFXPresetRead, GFXPresetToggle),
	ADDSET_BOOL		("Fullbright skins", r_fullbrightSkins),
	ADDSET_SEPARATOR("Sound"),
	ADDSET_NUMBER	("Sound Volume", s_volume, 0, 1, 0.05),
	ADDSET_BOOL		("Static Sounds", cl_staticsounds),
	ADDSET_SEPARATOR("Controls"),
	ADDSET_NUMBER	("Mouse Speed", sensitivity, 1, 15, 0.25),
	ADDSET_NUMBER	("M. Acceleration", m_accel, 0, 1, 0.1),
	ADDSET_CUSTOM	("Invert Mouse", InvertMouseRead, InvertMouseToggle),
	ADDSET_CUSTOM	("Gun Autoswitch", AutoSWRead, AutoSWToggle),
	ADDSET_CUSTOM	("Always Run", AlwaysRunRead, AlwaysRunToggle),
	ADDSET_BOOL		("Mouse Look", freelook),
	ADDSET_BOOL		("Smart Jump", cl_smartjump),
	ADDSET_NAMED	("Movement Scripts", allow_scripts, allowscripts_enum),
	ADDSET_SEPARATOR("Connection"),
	ADDSET_CUSTOM	("Bandwidth Limit", BandwidthRead, BandwidthToggle),
	ADDSET_CUSTOM	("Quality", ConQualityRead, ConQualityToggle),
	ADDSET_SEPARATOR("Weapons handling"),
	ADDSET_BOOL		("Preselect", cl_weaponpreselect),
	ADDSET_BOOL		("Auto hide", cl_weaponhide),
	ADDSET_SEPARATOR("Chat settings"),
	ADDSET_NAMED	("Ignore Opponents", ignore_opponents, ignoreopponents_enum),
	ADDSET_BOOL		("Ignore Observers", ignore_qizmo_spec),
	ADDSET_BOOL		("Ignore Spectators", ignore_spec),
	ADDSET_NAMED	("Message Filtering", msg_filter, msgfilter_enum),
	ADDSET_BOOL		("Own Volume Levels", cl_chatsound),
	ADDSET_NUMBER	("General", con_sound_mm1_volume, 0, 1, 0.1),
	ADDSET_NUMBER	("Team Chat", con_sound_mm2_volume, 0, 1, 0.1),
	ADDSET_NUMBER	("Spectators", con_sound_spec_volume, 0, 1, 0.1),
	ADDSET_NUMBER	("Others", con_sound_other_volume, 0, 1, 0.1),
	ADDSET_SEPARATOR("Match Tools"),
	ADDSET_BOOL		("Auto Screenshot", match_auto_sshot),
	ADDSET_NAMED	("Auto Record", match_auto_record, autorecord_enum),
	ADDSET_NAMED	("Auto Log", match_auto_logconsole, autorecord_enum),
	ADDSET_CUSTOM	("Sshot Format", SshotformatRead, SshotformatToggle),
	ADDSET_SEPARATOR("Demos"),
	ADDSET_STRING	("Directory", demo_dir),
#ifdef _WIN32
	ADDSET_CUSTOM	("Format", DemoformatRead, DemoformatToggle),
#endif
	ADDSET_STRING	("Qizmo Dir", qizmo_dir),
	ADDSET_STRING	("QWDTools Dir", qwdtools_dir),
};

settings_page settgeneral;

void CT_Opt_Settings_Draw (int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page) {
	Settings_Draw(x, y, w, h, &settgeneral);
}

int CT_Opt_Settings_Key (int k, CTab_t *tab, CTabPage_t *page) {
	return Settings_Key(&settgeneral, k);
}

// </SETTINGS>
//=============================================================================


settings_page settmultiview;
setting settmultiview_arr[] = {
	ADDSET_SEPARATOR("Multiview"),
	ADDSET_NUMBER	("Multiview", cl_multiview, 0, 4, 1),
	ADDSET_BOOL		("Display HUD", cl_mvdisplayhud),
	ADDSET_BOOL		("HUD Flip", cl_mvhudflip),
	ADDSET_BOOL		("HUD Vertical", cl_mvhudvertical),
	ADDSET_BOOL		("Inset View", cl_mvinset),
	ADDSET_BOOL		("Inset HUD", cl_mvinsethud),
	ADDSET_BOOL		("Inset Cross", cl_mvinsetcrosshair),
	ADDSET_SEPARATOR("Multiview Demos"),
	ADDSET_NAMED	("Autohud", mvd_autohud, mvdautohud_enum),
	ADDSET_NAMED	("Autotrack", mvd_autotrack, mvdautotrack_enum),
	ADDSET_BOOL		("Moreinfo", mvd_moreinfo), 
	ADDSET_BOOL     ("Status", mvd_status),
};

void CT_Opt_Multiview_Draw (int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page) {
	Settings_Draw(x, y, w, h, &settmultiview);
}

int CT_Opt_Multiview_Key (int k, CTab_t *tab, CTabPage_t *page) {
	return Settings_Key(&settmultiview, k);
}

settings_page setthud;
setting setthud_arr[] = {
	ADDSET_SEPARATOR("Head Up Display"),
	ADDSET_NAMED	("HUD Type", scr_newHud, hud_enum),
	ADDSET_NUMBER	("Crosshair", crosshair, 0, 7, 1),
	ADDSET_NUMBER	("Crosshair size", crosshairsize, 0.2, 3, 0.2),
#ifdef GLQUAKE
	ADDSET_NAMED	("Overhead Info", scr_autoid, scrautoid_enum),
#endif
	ADDSET_SEPARATOR("New HUD"),
	ADDSET_BOOLLATE	("Gameclock", hud_gameclock_show),
	ADDSET_BOOLLATE ("Big Gameclock", hud_gameclock_big),
	ADDSET_BOOLLATE ("Teamholdbar", hud_teamholdbar_show),
	ADDSET_BOOLLATE ("Teamholdinfo", hud_teamholdinfo_show),
	ADDSET_BOOLLATE ("FPS", hud_fps_show),
	ADDSET_BOOLLATE ("Clock", hud_clock_show),
#ifdef GLQUAKE
	ADDSET_BOOLLATE ("Radar", hud_radar_show),
#endif
	ADDSET_SEPARATOR("Old HUD"),
	ADDSET_BOOL		("Old Status Bar", cl_sbar),
	ADDSET_BOOL		("Old HUD Left", cl_hudswap),
	ADDSET_BOOL		("Show FPS", show_fps),
	ADDSET_BOOL		("Show Clock", scr_clock),
	ADDSET_BOOL		("Show Gameclock", scr_gameclock),
#ifdef GLQUAKE
	ADDSET_SEPARATOR("Tracker Messages"),
	ADDSET_BOOL		("Flags", amf_tracker_flags),
	ADDSET_BOOL		("Frags", amf_tracker_frags),
	ADDSET_NUMBER	("Messages", amf_tracker_messages, 0, 10, 1),
	ADDSET_BOOL		("Streaks", amf_tracker_streaks),
	ADDSET_NUMBER	("Time", amf_tracker_time, 0.5, 6, 0.5),
	ADDSET_NUMBER	("Scale", amf_tracker_scale, 0.1, 2, 0.1),
	ADDSET_BOOL		("Align Right", amf_tracker_align_right),
	ADDSET_SEPARATOR("Console"),
	ADDSET_NAMED	("Colored Text", scr_coloredText, coloredtext_enum),
	ADDSET_NAMED	("Fun Chars More", con_funchars_mode, funcharsmode_enum),
	ADDSET_NUMBER	("Notify Lines", _con_notifylines, 0, 16, 1),
	ADDSET_NUMBER	("Notify Time", con_notifytime, 0.5, 16, 0.5),
	ADDSET_BOOL		("Timestamps", con_timestamps),
#ifdef GLQUAKE
	ADDSET_BOOL		("Font Smoothing", gl_smoothfont),
#endif
	ADDSET_NUMBER	("Console height", scr_consize, 0.1, 1.0, 0.05),

#endif

};

void CT_Opt_HUD_Draw (int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page) {
	Settings_Draw(x, y, w, h, &setthud);
}

int CT_Opt_HUD_Key (int k, CTab_t *tab, CTabPage_t *page) {
	return Settings_Key(&setthud, k);
}

settings_page settplayer;
setting settplayer_arr[] = {
	ADDSET_SEPARATOR("Own Settings"),
	ADDSET_STRING	("Name", name),
	ADDSET_STRING	("Teamchat Prefix", cl_fakename),
	ADDSET_STRING	("Team", team),
	ADDSET_STRING	("Skin", skin),
	ADDSET_COLOR	("Shirt Color", topcolor),
	ADDSET_COLOR	("Pants Color", bottomcolor),
	ADDSET_SEPARATOR("Teammates"),
	ADDSET_COLOR	("Shirt Color", cl_teamtopcolor),
	ADDSET_COLOR	("Pants Color", cl_teambottomcolor),
	ADDSET_STRING   ("Skin", cl_teamskin),
	ADDSET_STRING	("Quad Skin", cl_teamquadskin),
	ADDSET_STRING	("Pent Skin", cl_teampentskin),
	ADDSET_STRING	("Quad+Pent Skin", cl_teambothskin),
	ADDSET_SEPARATOR("Enemies"),
	ADDSET_COLOR	("Shirt Color", cl_enemytopcolor),
	ADDSET_COLOR	("Pants Color", cl_enemybottomcolor),
	ADDSET_STRING   ("Skin", cl_enemyskin),
	ADDSET_STRING	("Quad Skin", cl_enemyquadskin),
	ADDSET_STRING	("Pent Skin", cl_enemypentskin),
	ADDSET_STRING	("Quad+Pent Skin", cl_enemybothskin),
};

void CT_Opt_Player_Draw (int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page) {
	Settings_Draw(x, y, w, h, &settplayer);
}

int CT_Opt_Player_Key (int k, CTab_t *tab, CTabPage_t *page) {
	return Settings_Key(&settplayer, k);
}

//=============================================================================
// <BINDS>

char *bindnames[][2] =
{
	{"+attack",         "attack"},
	{"+use",            "use"},
	{"+jump",           "jump"},
	{"+forward",        "move forward"},
	{"+back",           "move back"},
	{"+moveleft",       "move left"},
	{"+moveright",      "move right"},
	{"impulse 12",      "previous weapon"},
	{"impulse 10",      "next weapon"},
	{"toggleproxymenu", "proxy menu"},
	{"report",			"tp_report"},
	{"weapon 1",       "axe"},
	{"weapon 2",       "shotgun"},
	{"weapon 3",       "super shotgun"},
	{"weapon 4",       "nailgun"},
	{"weapon 5",       "super nailgun"},
	{"weapon 6",       "grenade launcher"},
	{"weapon 7",       "rocket launcher"},
	{"weapon 8",       "thunderbolt"},
};

#define    NUMCOMMANDS    (sizeof(bindnames)/sizeof(bindnames[0]))

int        keys_cursor;
int        bind_grab;

static void M_UnbindCommand (char *command) {
	int j, l;
	char *b;

	l = strlen(command);

	for (j = 0; j < (sizeof(keybindings) / sizeof(*keybindings)); j++) {
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
			Key_Unbind (j);
	}
}

#define KEYSCOL1 22*8

void CT_Opt_Binds_Draw (int x2, int y2, int w, int h, CTab_t *tab, CTabPage_t *page) {
	int x, y, i, keys[2];
	char *name;

	if (bind_grab)
		UI_Print_Center (x2, y2, w, "Press a key or button for this action", false);
	else
		UI_Print_Center (x2, y2, w, "Enter to change, del to clear", true);

	y2 += 16;
	// search for known bindings
	for (i = 0; i < NUMCOMMANDS; i++) {
		x = x2 + KEYSCOL1 + 3*8;
		y = y2 + 8*i;

		UI_Print (x2 + KEYSCOL1 - min(strlen(bindnames[i][1]), 20) * 8, y, bindnames[i][1], false);

		M_FindKeysForCommand (bindnames[i][0], keys);

		if (keys[0] == -1) {
			UI_Print (x, y, "???", false);
		} else {
#ifdef WITH_KEYMAP
			char    str[256];
			name = Key_KeynumToString (keys[0], str);
#else // WITH_KEYMAP
			name = Key_KeynumToString (keys[0]);
#endif // WITH_KEYMAP else
			UI_Print (x, y, name, false);
			x += strlen(name)*8;
			if (keys[1] != -1) {
				UI_Print (x + 8, y, "or", false);
#ifdef WITH_KEYMAP
				UI_Print (x + 4*8, y, Key_KeynumToString (keys[1], str), false);
#else // WITH_KEYMAP
				UI_Print (x + 4*8, y, Key_KeynumToString (keys[1]), false);
#endif // WITH_KEYMAP else
			}
		}
	}

	if (bind_grab)
		UI_DrawCharacter (x2 + KEYSCOL1 + 8, y2 + keys_cursor*8, '=');
	else
		UI_DrawCharacter (x2 + KEYSCOL1 + 8, y2 + keys_cursor*8, FLASHINGARROW());
}


int CT_Opt_Binds_Key (int k, CTab_t *tab, CTabPage_t *page) {
	int keys[2];

	if (bind_grab) {
		// defining a key
		S_LocalSound ("misc/menu1.wav");
		if (k == K_ESCAPE)
			bind_grab = false;
#ifdef WITH_KEYMAP
		/* Massa: all keys should be bindable, including the console switching */
		else
#else // WITH_KEYMAP
        else if (k != '`')
#endif // WITH_KEYMAP else
            Key_SetBinding (k, bindnames[keys_cursor][0]);

			bind_grab = false;
			return true;
	}

	switch (k) {
		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");
			keys_cursor--;
			if (keys_cursor < 0)
				keys_cursor = NUMCOMMANDS-1;
			break;

		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");
			keys_cursor++;
			if (keys_cursor >= NUMCOMMANDS)
				keys_cursor = 0;
			break;

		case K_HOME:
			S_LocalSound ("misc/menu1.wav");
			keys_cursor = 0;
			break;

		case K_END:
			S_LocalSound ("misc/menu1.wav");
			keys_cursor = NUMCOMMANDS - 1;
			break;

		case K_ENTER:        // go into bind mode
			M_FindKeysForCommand (bindnames[keys_cursor][0], keys);
			S_LocalSound ("misc/menu2.wav");
			if (keys[1] != -1)
				M_UnbindCommand (bindnames[keys_cursor][0]);
			bind_grab = true;
			break;

		case K_DEL:                // delete bindings
			S_LocalSound ("misc/menu2.wav");
			M_UnbindCommand (bindnames[keys_cursor][0]);
			break;

		default:
			return false;
	}

	return true;
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
extern cvar_t gl_part_inferno;
extern cvar_t amf_lightning;
#endif
extern cvar_t r_drawflat;

const char* explosiontype_enum[] =
{ "fire + sparks", "fire only", "teleport", "blood", "big blood", "dbl gunshot", "blob effect", "big explosion", "plasma" };

const char* muzzleflashes_enum[] =
{ "off", "on", "own off" };

const char* deadbodyfilter_enum[] = 
{ "off", "decent", "instant" };

const char* rocketmodel_enum[] =
{ "rocket", "grenade" };

const char* rockettrail_enum[] =
{ "off", "normal", "grenade", "alt normal", "slight blood", "big blood", "tracer 1", "tracer 2", "normal" };

const char* powerupglow_enum[] =
{ "off", "on", "own off" };

void LoadFastPreset(void) {
	Cvar_SetValue (&r_explosiontype, 1);
	Cvar_SetValue (&r_explosionlight, 0);
	Cvar_SetValue (&cl_muzzleflash, 0);
	Cvar_SetValue (&cl_gibfilter, 1);
	Cvar_SetValue (&cl_deadbodyfilter, 1);
	Cvar_SetValue (&r_rocketlight, 0);
	Cvar_SetValue (&r_powerupglow, 0);
	Cvar_SetValue (&r_drawflame, 0);
	Cvar_SetValue (&r_fastsky, 1);
	Cvar_SetValue (&r_rockettrail, 1);
	Cvar_SetValue (&v_damagecshift, 0);
#ifdef GLQUAKE
	Cvar_SetValue (&gl_flashblend, 1);
	Cvar_SetValue (&r_dynamic, 0);
	Cvar_SetValue (&gl_part_explosions, 0);
	Cvar_SetValue (&gl_part_trails, 0);
	Cvar_SetValue (&gl_part_spikes, 0);
	Cvar_SetValue (&gl_part_gunshots, 0);
	Cvar_SetValue (&gl_part_blood, 0);
	Cvar_SetValue (&gl_part_telesplash, 0);
	Cvar_SetValue (&gl_part_blobs, 0);
	Cvar_SetValue (&gl_part_lavasplash, 0);
	Cvar_SetValue (&gl_part_inferno, 0);
#endif
}

void LoadHQPreset(void) {
	Cvar_SetValue (&r_explosiontype, 0);
	Cvar_SetValue (&r_explosionlight, 1);
	Cvar_SetValue (&cl_muzzleflash, 1);
	Cvar_SetValue (&cl_gibfilter, 0);
	Cvar_SetValue (&cl_deadbodyfilter, 0);
	Cvar_SetValue (&r_rocketlight, 1);
	Cvar_SetValue (&r_powerupglow, 2);
	Cvar_SetValue (&r_drawflame, 1);
	Cvar_SetValue (&r_fastsky, 0);
	Cvar_SetValue (&r_rockettrail, 1);
	Cvar_SetValue (&v_damagecshift, 1);
#ifdef GLQUAKE
	Cvar_SetValue (&gl_flashblend, 0);
	Cvar_SetValue (&r_dynamic, 1);
	Cvar_SetValue (&gl_part_explosions, 1);
	Cvar_SetValue (&gl_part_trails, 1);
	Cvar_SetValue (&gl_part_spikes, 1);
	Cvar_SetValue (&gl_part_gunshots, 1);
	Cvar_SetValue (&gl_part_blood, 1);
	Cvar_SetValue (&gl_part_telesplash, 1);
	Cvar_SetValue (&gl_part_blobs, 1);
	Cvar_SetValue (&gl_part_lavasplash, 1);
	Cvar_SetValue (&gl_part_inferno, 1);
#endif
}

const char* grenadetrail_enum[] = { "off", "normal", "grenade", "alt", "blood", "big blood", "tracer1", "tracer2", "plasma", "lavaball", "fuel rod", "plasma rocket" };

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
const char* TexturesdetailRead(void) { // 1, 8, 256, 2048
	if (gl_max_size.value < 8) return "low";
	else if (gl_max_size.value < 200) return "medium";
	else if (gl_max_size.value < 1025) return "high";
	else return "max";
}
void TexturesdetailToggle(qbool back) {
	if (gl_max_size.value < 8) Cvar_SetValue(&gl_max_size, 8);
	else if (gl_max_size.value < 200) Cvar_SetValue(&gl_max_size, 256);
	else if (gl_max_size.value < 1025) Cvar_SetValue(&gl_max_size, 2048);
	else Cvar_SetValue(&gl_max_size, 1);
}
extern cvar_t gl_texturemode;
static int Texturesquality(void) {
	if (!strcmp(gl_texturemode.string, "GL_NEAREST")) return 0;
	else if (!strcmp(gl_texturemode.string, "GL_NEAREST_MIPMAP_NEAREST")) return 1;
	else if (!strcmp(gl_texturemode.string, "GL_LINEAR")) return 2;
	else if (!strcmp(gl_texturemode.string, "GL_LINEAR_MIPMAP_NEAREST")) return 3;
	else return 4;
}
const char* TexturesqualityRead(void) {
	switch (Texturesquality()) {
	case 0: return "very low"; case 1: return "low"; case 2: return "medium"; case 3: return "high"; default: return "very high";
	}
}
void TexturesqualityToggle(qbool back) {
	switch (Texturesquality()) {
	case 0: Cvar_Set(&gl_texturemode, "GL_NEAREST_MIPMAP_NEAREST"); return;
	case 1: Cvar_Set(&gl_texturemode, "GL_LINEAR"); return;
	case 2: Cvar_Set(&gl_texturemode, "GL_LINEAR_MIPMAP_NEAREST"); return;
	case 3: Cvar_Set(&gl_texturemode, "GL_LINEAR_MIPMAP_LINEAR"); return;
	default: Cvar_Set(&gl_texturemode, "GL_NEAREST"); return;
	}
}
#endif

setting settfps_arr[] = {
	ADDSET_SEPARATOR("Presets"),
	ADDSET_ACTION	("Load Fast Preset", LoadFastPreset),
	ADDSET_ACTION	("Load HQ preset", LoadHQPreset),
	ADDSET_SEPARATOR("Miscellaneous"),
	ADDSET_CUSTOM	("FPS Limit", FpslimitRead, FpslimitToggle),
	ADDSET_NAMED	("Muzzleflashes", cl_muzzleflash, muzzleflashes_enum),
	ADDSET_BOOL		("Damage Flash", v_damagecshift),
	ADDSET_BOOL		("Pickup Flashes", v_bonusflash),
	ADDSET_SEPARATOR("Environment"),
	ADDSET_BOOL		("Simple Sky", r_fastsky),
	ADDSET_BOOL		("Simple walls", r_drawflat),
	ADDSET_BOOL		("Simple turbs", r_fastturb), 
	ADDSET_BOOL		("Draw flame", r_drawflame),
	ADDSET_BOOL		("Gib Filter", cl_gibfilter),
	ADDSET_NAMED	("Dead Body Filter", cl_deadbodyfilter, deadbodyfilter_enum),
	ADDSET_SEPARATOR("Projectiles"),
	ADDSET_NAMED	("Explosion Type", r_explosiontype, explosiontype_enum),
	ADDSET_NAMED	("Rocket Model", cl_rocket2grenade, rocketmodel_enum),
	ADDSET_NAMED	("Rocket Trail", r_rockettrail, rockettrail_enum),
	ADDSET_BOOL		("Rocket Light", r_rocketlight),
	ADDSET_NAMED	("Grenade Trail", r_grenadetrail, grenadetrail_enum),
	ADDSET_NUMBER	("Fakeshaft", cl_fakeshaft, 0, 1, 0.05),
#ifdef GLQUAKE
	ADDSET_BOOL		("Hide Nails", amf_hidenails),
	ADDSET_BOOL		("Hide Rockets", amf_hiderockets),
#endif
	ADDSET_SEPARATOR("Lighting"),
	ADDSET_NAMED	("Powerup Glow", r_powerupglow, powerupglow_enum),
#ifdef GLQUAKE
	ADDSET_BOOL		("Colored Lights", gl_colorlights),
	ADDSET_BOOL		("Fast Lights", gl_flashblend),
	ADDSET_BOOL		("Dynamic Ligts", r_dynamic),
	ADDSET_NUMBER	("Light mode", gl_lightmode, 0, 2, 1),
	ADDSET_BOOL		("Particle Shaft", amf_lightning),
#endif
	ADDSET_BOOL		("View Weapon", cl_drawgun),
	ADDSET_NUMBER	("Size", r_viewmodelsize, 0.1, 1, 0.05),
	ADDSET_NUMBER	("Shift", r_viewmodeloffset, -10, 10, 1),
#ifdef GLQUAKE
	ADDSET_SEPARATOR("Textures"),
	ADDSET_NUMBER	("Anisotropy filter", gl_anisotropy, 0, 16, 1),
	ADDSET_BOOL		("Luma", gl_lumaTextures),
	ADDSET_CUSTOM	("Detail", TexturesdetailRead, TexturesdetailToggle),
	ADDSET_NUMBER	("Miptex", gl_miptexLevel, 0, 3, 1),
	ADDSET_BOOL		("No Textures", gl_textureless),
	ADDSET_CUSTOM	("Quality Mode", TexturesqualityRead, TexturesqualityToggle),
	ADDSET_SEPARATOR("Point of View"),
	ADDSET_NUMBER	("Rollangle", cl_rollangle, 0, 30, 2),
	ADDSET_NUMBER	("Rollspeed", cl_rollspeed, 0, 30, 2),
	ADDSET_BOOL		("Gun Kick", v_gunkick),
	ADDSET_NUMBER	("Kick Pitch", v_kickpitch, 0, 10, 0.5),
	ADDSET_NUMBER	("Kick Roll", v_kickroll, 0, 10, 0.5),
	ADDSET_NUMBER	("Kick Time", v_kicktime, 0, 10, 0.5),
	ADDSET_NUMBER	("View Height", v_viewheight, -7, 7, 0.5),
#endif
};

settings_page settfps;

void CT_Opt_FPS_Draw (int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page)
{
	Settings_Draw(x, y, w, h, &settfps);
}

int CT_Opt_FPS_Key (int k, CTab_t *tab, CTabPage_t *page) {
	return Settings_Key(&settfps, k);
}

// </FPS>
//=============================================================================


//=============================================================================
/* VIDEO MENU */

void CT_Opt_Video_Draw (int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page) {
	(*vid_menudrawfn) ();
}

int CT_Opt_Video_Key (int key, CTab_t *tab, CTabPage_t *page) {
	(*vid_menukeyfn) (key);
	// i was too lazy&scared to change all those vid_menukeyfn functions out there
	// so because i know what keys have some function in there, i list them here:
	return key == K_LEFTARROW || key == K_RIGHTARROW || key == K_DOWNARROW || key == K_UPARROW || key == K_ENTER;
}

// </VIDEO>

void Menu_Options_Key(int key, int unichar) {
    int handled = CTab_Key(&options_tab, key);
	options_unichar = unichar;

    if (!handled && (key == K_ESCAPE))
		M_Menu_Main_f();
}

void Menu_Options_Draw(void) {
	int x, y, w, h;
	static int previousPage = -1;
	if (previousPage != options_tab.activePage) {
		switch (options_tab.activePage) {
		case OPTPG_SETTINGS: Settings_OnShow(&settgeneral); break;
		case OPTPG_PLAYER: Settings_OnShow(&settplayer); break;
		case OPTPG_FPS: Settings_OnShow(&settfps); break;
		case OPTPG_HUD: Settings_OnShow(&setthud); break;
		case OPTPG_MULTIVIEW: Settings_OnShow(&settmultiview); break;
		}
	}

	previousPage = options_tab.activePage;

#ifdef GLQUAKE
	// do not scale this menu
	if (scr_scaleMenu.value) {
		menuwidth = vid.width;
		menuheight = vid.height;
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity ();
		glOrtho  (0, menuwidth, menuheight, 0, -99999, 99999);
	}
#endif

	w = min(max(512, 320), vid.width) - 8;
	h = min(max(432, 200), vid.height) - 8;
	x = (vid.width - w) / 2;
	y = (vid.height - h) / 2;

	CTab_Draw(&options_tab, x, y, w, h);
}

void Menu_Options_Init(void) {
	Settings_Page_Init(settgeneral, settgeneral_arr);
	Settings_Page_Init(settfps, settfps_arr);
	Settings_Page_Init(settmultiview, settmultiview_arr);
	Settings_Page_Init(setthud, setthud_arr);
	Settings_Page_Init(settplayer, settplayer_arr);

	CTab_Init(&options_tab);
	CTab_AddPage(&options_tab, "main", OPTPG_SETTINGS, NULL, CT_Opt_Settings_Draw, CT_Opt_Settings_Key);
	CTab_AddPage(&options_tab, "player", OPTPG_PLAYER, NULL, CT_Opt_Player_Draw, CT_Opt_Player_Key);
	CTab_AddPage(&options_tab, "graphics", OPTPG_FPS, NULL, CT_Opt_FPS_Draw, CT_Opt_FPS_Key);
	CTab_AddPage(&options_tab, "hud", OPTPG_HUD, NULL, CT_Opt_HUD_Draw, CT_Opt_HUD_Key);
	CTab_AddPage(&options_tab, "multiview", OPTPG_MULTIVIEW, NULL, CT_Opt_Multiview_Draw, CT_Opt_Multiview_Key);
	CTab_AddPage(&options_tab, "controls", OPTPG_BINDS, NULL, CT_Opt_Binds_Draw, CT_Opt_Binds_Key);
	CTab_AddPage(&options_tab, "video", OPTPG_VIDEO, NULL, CT_Opt_Video_Draw, CT_Opt_Video_Key);
	CTab_SetCurrentId(&options_tab, OPTPG_SETTINGS);
}
