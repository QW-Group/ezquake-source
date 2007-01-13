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
		$Id: menu_options.c,v 1.7 2007-01-13 03:16:22 johnnycz Exp $

*/

#include "quakedef.h"
#include "settings.h"
#include "settings_page.h"

typedef enum {
	OPTPG_SETTINGS,
	OPTPG_FPS,
	OPTPG_PLAYER,
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
	r_fastturb, r_grenadetrail, cl_drawgun, r_viewmodelsize, r_viewmodeloffset;
;
#ifdef GLQUAKE
extern cvar_t scr_autoid, gl_smoothfont, amf_hidenails, amf_hiderockets, gl_anisotropy, gl_lumaTextures, gl_textureless, gl_colorlights;
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
	ADDSET_NUMBER	("Crosshair", crosshair, 0, 7, 1),
	ADDSET_NUMBER	("Crosshair size", crosshairsize, 0.2, 3, 0.2),
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
	ADDSET_SEPARATOR("Head Up Display"),
	ADDSET_BOOL		("New HUD", scr_newHud),
#ifdef GLQUAKE
	ADDSET_NAMED	("Overhead Info", scr_autoid, scrautoid_enum),
#endif
	ADDSET_BOOL		("Old Status Bar", cl_sbar),
	ADDSET_BOOL		("Old HUD Left", cl_hudswap),
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
#ifdef GLQUAKE
	ADDSET_SEPARATOR("Tracker Messages"),
	ADDSET_BOOL		("Flags", amf_tracker_flags),
	ADDSET_BOOL		("Frags", amf_tracker_frags),
	ADDSET_NUMBER	("Messages", amf_tracker_messages, 0, 10, 1),
	ADDSET_BOOL		("Streaks", amf_tracker_streaks),
	ADDSET_BOOL		("Time", amf_tracker_time),
	ADDSET_NUMBER	("Scale", amf_tracker_scale, 0.1, 2, 0.1),
	ADDSET_BOOL		("Align Right", amf_tracker_align_right),
#endif
	ADDSET_SEPARATOR("Weapons handling"),
	ADDSET_BOOL		("Preselect", cl_weaponpreselect),
	ADDSET_BOOL		("Auto hide", cl_weaponhide),
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
	ADDSET_SEPARATOR("Chat settings"),
	ADDSET_NAMED	("Ignore Opponents", ignore_opponents, ignoreopponents_enum),
	ADDSET_BOOL		("Ignore Observers", ignore_qizmo_spec),
	ADDSET_BOOL		("Ignore Spectators", ignore_spec),
	ADDSET_NAMED	("Message Filtering", msg_filter, msgfilter_enum),
	ADDSET_SEPARATOR("Point of View"),
	ADDSET_NUMBER	("Rollangle", cl_rollangle, 0, 30, 2),
	ADDSET_NUMBER	("Rollspeed", cl_rollspeed, 0, 30, 2),
	ADDSET_BOOL		("Gun Kick", v_gunkick),
	ADDSET_NUMBER	("Kick Pitch", v_kickpitch, 0, 10, 0.5),
	ADDSET_NUMBER	("Kick Roll", v_kickroll, 0, 10, 0.5),
	ADDSET_NUMBER	("Kick Time", v_kicktime, 0, 10, 0.5),
	ADDSET_NUMBER	("View Height", v_viewheight, -7, 7, 0.5),
	ADDSET_SEPARATOR("Match Tools"),
	ADDSET_BOOL		("Auto Screenshot", match_auto_sshot),
	ADDSET_NAMED	("Auto Record", match_auto_record, autorecord_enum),
	ADDSET_NAMED	("Auto Log", match_auto_logconsole, autorecord_enum),
	ADDSET_CUSTOM	("Sshot Format", SshotformatRead, SshotformatToggle)
};

// todo on string: demo_format, demo_dir, qizmo_dir, qwdtools_dir

settings_page settgeneral;

void CT_Opt_Settings_Draw (int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page) {
	Settings_Draw(x, y, w, h, &settgeneral);
}

int CT_Opt_Settings_Key (int k, CTab_t *tab, CTabPage_t *page) {
	return Settings_Key(&settgeneral, k);
}

// </SETTINGS>
//=============================================================================


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


void CT_Opt_Binds_Draw (int x2, int y2, int w, int h, CTab_t *tab, CTabPage_t *page) {
	int x, y, i, l, keys[2];
	char *name;
	mpic_t *p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/ttl_cstm.lmp");
	M_DrawPic (64, 4, p);

	if (bind_grab)
		M_Print (64, 32, "Press a key or button for this action");
	else
		M_Print (64, 32, "Enter to change, del to clear");

	// search for known bindings
	for (i = 0; i < NUMCOMMANDS; i++) {
		y = 48 + 8*i;

		M_Print (64, y, bindnames[i][1]);

		l = strlen (bindnames[i][0]);

		M_FindKeysForCommand (bindnames[i][0], keys);

		if (keys[0] == -1) {
			M_Print (192, y, "???");
		} else {
#ifdef WITH_KEYMAP
			char    str[256];
			name = Key_KeynumToString (keys[0], str);
#else // WITH_KEYMAP
			name = Key_KeynumToString (keys[0]);
#endif // WITH_KEYMAP else
			M_Print (192, y, name);
			x = strlen(name) * 8;
			if (keys[1] != -1) {
				M_Print (192 + x + 8, y, "or");
#ifdef WITH_KEYMAP
				M_Print (192 + x + 32, y, Key_KeynumToString (keys[1], str));
#else // WITH_KEYMAP
				M_Print (192 + x + 32, y, Key_KeynumToString (keys[1]));
#endif // WITH_KEYMAP else
			}
		}
	}

	if (bind_grab)
		M_DrawCharacter (170, 48 + keys_cursor*8, '=');
	else
		M_DrawCharacter (170, 48 + keys_cursor*8, FLASHINGARROW());
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
extern cvar_t amf_lightning
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
	ADDSET_NUMBER	("Shift", r_viewmodeloffset, -10, 10, 0),
#ifdef GLQUAKE
	ADDSET_SEPARATOR("Textures"),
	ADDSET_NUMBER	("Anisotropy filter", gl_anisotropy, 0, 16, 1),
	ADDSET_BOOL		("Luma", gl_lumaTextures),
	ADDSET_CUSTOM	("Detail", TexturesdetailRead, TexturesdetailToggle),
	ADDSET_NUMBER	("Miptex", gl_miptexLevel, 0, 10, 0.5),
	ADDSET_BOOL		("No Textures", gl_textureless),
	ADDSET_CUSTOM	("Quality Mode", TexturesqualityRead, TexturesqualityToggle),

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

//=============================================================================
/* PLAYER MENU */

int        setup_cursor = 0;
int        setup_cursor_table[] = {40, 56, 80, 104, 140};

char    setup_name[16];
char    setup_team[16];
int        setup_oldtop;
int        setup_oldbottom;
int        setup_top;
int        setup_bottom;

extern cvar_t    name, team;
extern cvar_t    topcolor, bottomcolor;

#define    NUM_SETUP_CMDS    5

static void Opt_Player_Load (void) {
	strlcpy (setup_name, name.string, sizeof(setup_name));
	strlcpy (setup_team, team.string, sizeof(setup_team));
	setup_top = setup_oldtop = (int)topcolor.value;
	setup_bottom = setup_oldbottom = (int)bottomcolor.value;
}

byte identityTable[256];
byte translationTable[256];

void M_BuildTranslationTable(int top, int bottom) {
	int        j;
	byte    *dest, *source;

	for (j = 0; j < 256; j++)
		identityTable[j] = j;
	dest = translationTable;
	source = identityTable;
	memcpy (dest, source, 256);

	if (top < 128)    // the artists made some backwards ranges.  sigh.
		memcpy (dest + TOP_RANGE, source + top, 16);
	else
		for (j = 0; j < 16; j++)
			dest[TOP_RANGE + j] = source[top + 15 - j];

	if (bottom < 128)
		memcpy (dest + BOTTOM_RANGE, source + bottom, 16);
	else
		for (j = 0; j < 16; j++)
			dest[BOTTOM_RANGE + j] = source[bottom + 15 - j];
}

static void M_DrawTransPicTranslate (int x, int y, mpic_t *pic) {
	Draw_TransPicTranslate (x + ((menuwidth - 320) >> 1), y + m_yofs, pic, translationTable);
}

void CT_Opt_Player_Draw (int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page) {
	mpic_t    *p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic (64, 4, p);

	M_Print (64, 40, "Your name");
	M_DrawTextBox (160, 32, 16, 1);
	M_PrintWhite (168, 40, setup_name);

	M_Print (64, 56, "Your team");
	M_DrawTextBox (160, 48, 16, 1);
	M_PrintWhite (168, 56, setup_team);

	M_Print (64, 80, "Shirt color");
	M_Print (64, 104, "Pants color");

	M_DrawTextBox (64, 140-8, 14, 1);
	M_Print (72, 140, "Accept Changes");

	p = Draw_CachePic ("gfx/bigbox.lmp");
	M_DrawTransPic (160, 64, p);
	p = Draw_CachePic ("gfx/menuplyr.lmp");
	M_BuildTranslationTable(setup_top*16, setup_bottom*16);
	M_DrawTransPicTranslate (172, 72, p);

	M_DrawCharacter (56, setup_cursor_table [setup_cursor], FLASHINGARROW());

	if (setup_cursor == 0)
		M_DrawCharacter (168 + 8*strlen(setup_name), setup_cursor_table [setup_cursor], FLASHINGCURSOR());

	if (setup_cursor == 1)
		M_DrawCharacter (168 + 8*strlen(setup_team), setup_cursor_table [setup_cursor], FLASHINGCURSOR());
}

int CT_Opt_Player_Key (int k, CTab_t *tab, CTabPage_t *page) {
	int l;
	int unichar = options_unichar;
	int capped = false;

	switch (k) {
		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");
			setup_cursor--;
			if (setup_cursor < 0)
				setup_cursor = NUM_SETUP_CMDS-1;
			capped = true;
			break;

		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");
			setup_cursor++;
			if (setup_cursor >= NUM_SETUP_CMDS)
				setup_cursor = 0;
			capped = true;
			break;

		case K_HOME:
			S_LocalSound ("misc/menu1.wav");
			setup_cursor = 0;
			capped = true;
			break;

		case K_END:
			S_LocalSound ("misc/menu1.wav");
			setup_cursor = NUM_SETUP_CMDS-1;
			capped = true;
			break;

		case K_LEFTARROW:
			if (setup_cursor < 2)
				return false;
			S_LocalSound ("misc/menu3.wav");
			if (setup_cursor == 2)
				setup_top = setup_top - 1;
			if (setup_cursor == 3)
				setup_bottom = setup_bottom - 1;
			capped = true;
			break;

		case K_RIGHTARROW:
			if (setup_cursor < 2)
				return false;
			//forward:
			S_LocalSound ("misc/menu3.wav");
			if (setup_cursor == 2)
				setup_top = setup_top + 1;
			if (setup_cursor == 3)
				setup_bottom = setup_bottom + 1;
			capped = true;
			break;

		case K_ENTER:
			//        if (setup_cursor == 0 || setup_cursor == 1)
			//            return;

			//        if (setup_cursor == 2 || setup_cursor == 3)
			//            goto forward;

			// setup_cursor == 4 (OK)
			Cvar_Set (&name, setup_name);
			Cvar_Set (&team, setup_team);
			Cvar_Set (&topcolor, va("%i", setup_top));
			Cvar_Set (&bottomcolor, va("%i", setup_bottom));
			m_entersound = true;
			M_Menu_Main_f ();
			capped = true;
			break;

		case K_BACKSPACE:
			if (setup_cursor == 0) {
				if (strlen(setup_name))
					setup_name[strlen(setup_name)-1] = 0;
			} else if (setup_cursor == 1) {
				if (strlen(setup_team))
					setup_team[strlen(setup_team)-1] = 0;
			} else {
				return false;
			}
			capped = true;
			break;

		default:
			if (!unichar)
				break;
			switch (setup_cursor) {
			case 0:
				l = strlen(setup_name);
				if (l < 15) {
					setup_name[l+1] = 0;
					setup_name[l] = wc2char(unichar);
				}
				capped = true;
				break;

			case 1:
				l = strlen(setup_team);
				if (l < 15) {
					setup_team[l + 1] = 0;
					setup_team[l] = wc2char(unichar);
				}
				capped = true;
				break;

			default: capped = false;
			}
	}

	if (setup_top > 13)
		setup_top = 0;
	if (setup_top < 0)
		setup_top = 13;
	if (setup_bottom > 13)
		setup_bottom = 0;
	if (setup_bottom < 0)
		setup_bottom = 13;

	return capped;
}

void Menu_Options_Key(int key, int unichar) {
    int handled = CTab_Key(&options_tab, key);
	options_unichar = unichar;

    if (!handled && (key == K_ESCAPE))
		M_Menu_Main_f();
}

void Menu_Options_Draw(void) {
	int x, y, w, h;
	static int previousPage = 0;
	if (previousPage != options_tab.activePage && options_tab.activePage == OPTPG_PLAYER)
		Opt_Player_Load();

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
	Settings_Page_Init(settgeneral_arr, settgeneral);
	Settings_Page_Init(settfps_arr, settfps);

	CTab_Init(&options_tab);
	CTab_AddPage(&options_tab, "main", OPTPG_SETTINGS, NULL, CT_Opt_Settings_Draw, CT_Opt_Settings_Key);
	CTab_AddPage(&options_tab, "graphics", OPTPG_FPS, NULL, CT_Opt_FPS_Draw, CT_Opt_FPS_Key);
	CTab_AddPage(&options_tab, "player", OPTPG_PLAYER, NULL, CT_Opt_Player_Draw, CT_Opt_Player_Key);
	CTab_AddPage(&options_tab, "controls", OPTPG_BINDS, NULL, CT_Opt_Binds_Draw, CT_Opt_Binds_Key);
	CTab_AddPage(&options_tab, "video", OPTPG_VIDEO, NULL, CT_Opt_Video_Draw, CT_Opt_Video_Key);
	CTab_SetCurrentId(&options_tab, OPTPG_SETTINGS);
}
