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
		$Id: menu_options.c,v 1.5 2007-01-12 16:26:06 johnnycz Exp $

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

void DefaultConfig(void) { Cbuf_AddText("cfg_reset\n"); }

setting settgeneral_tab[] = {
	ADDSET_SEPARATOR("Video"),
	ADDSET_NUMBER	("Gamma", v_gamma, 0.1, 2.0, 0.1),
	ADDSET_NUMBER	("Contrast", v_contrast, 1, 5, 0.1),
	ADDSET_NUMBER	("Screen Size", scr_viewsize, 30, 120, 5),
	ADDSET_NUMBER	("Field of View", scr_fov, 40, 140, 2),
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
	ADDSET_SEPARATOR("Head Up Display"),
	ADDSET_BOOL		("New HUD", scr_newHud),
	ADDSET_BOOL		("Old Status Bar", cl_sbar),
	ADDSET_BOOL		("Old HUD Left", cl_hudswap),
	ADDSET_SEPARATOR("Miscellaneous"),
	ADDSET_ACTION	("Go To Console", Con_ToggleConsole_f),
	ADDSET_ACTION	("Default Config", DefaultConfig)
};

settings_tab settgeneral;

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
extern cvar_t gl_part_inferno;
extern cvar_t amf_lightning;

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

setting settfps_tab[] = {
	ADDSET_SEPARATOR("Presets"),
	ADDSET_ACTION	("Load Fast Preset", LoadFastPreset),
	ADDSET_ACTION	("Load HQ preset", LoadHQPreset),
	ADDSET_SEPARATOR("Graphics settings"),
	ADDSET_NAMED	("Explosion Type", r_explosiontype, explosiontype_enum),
	ADDSET_NAMED	("Muzzleflashes", cl_muzzleflash, muzzleflashes_enum),
	ADDSET_BOOL		("Gib Filter", cl_gibfilter),
	ADDSET_NAMED	("Dead Body Filter", cl_deadbodyfilter, deadbodyfilter_enum),
	ADDSET_NAMED	("Rocket Model", cl_rocket2grenade, rocketmodel_enum),
	ADDSET_NAMED	("Rocket Trail", r_rockettrail, rockettrail_enum),
	ADDSET_BOOL		("Rocket Light", r_rocketlight),
	ADDSET_BOOL		("Damage Flash", v_damagecshift),
	ADDSET_BOOL		("Pickup Flashes", v_bonusflash),
	ADDSET_NAMED	("Powerup Glow", r_powerupglow, powerupglow_enum),
	ADDSET_BOOL		("Fast Sky", r_fastsky),
#ifdef GLQUAKE
	ADDSET_BOOL		("Fast Lights", gl_flashblend),
	ADDSET_BOOL		("Dynamic Ligts", r_dynamic),
	ADDSET_BOOL		("Particle Shaft", amf_lightning)
#endif
};

settings_tab settfps;

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
	settgeneral.set_tab = settgeneral_tab;
	settgeneral.set_marked = 0;
	settgeneral.set_count = sizeof(settgeneral_tab) / sizeof(setting);

	settfps.set_tab = settfps_tab;
	settfps.set_marked = 0;
	settfps.set_count = sizeof(settfps_tab) / sizeof(setting);

	CTab_Init(&options_tab);
	CTab_AddPage(&options_tab, "main", OPTPG_SETTINGS, NULL, CT_Opt_Settings_Draw, CT_Opt_Settings_Key);
	CTab_AddPage(&options_tab, "graphics", OPTPG_FPS, NULL, CT_Opt_FPS_Draw, CT_Opt_FPS_Key);
	CTab_AddPage(&options_tab, "player", OPTPG_PLAYER, NULL, CT_Opt_Player_Draw, CT_Opt_Player_Key);
	CTab_AddPage(&options_tab, "controls", OPTPG_BINDS, NULL, CT_Opt_Binds_Draw, CT_Opt_Binds_Key);
	CTab_AddPage(&options_tab, "video", OPTPG_VIDEO, NULL, CT_Opt_Video_Draw, CT_Opt_Video_Key);
	CTab_SetCurrentId(&options_tab, OPTPG_SETTINGS);
}
