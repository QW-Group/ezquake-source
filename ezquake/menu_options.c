/*

	Options Menu module

	Stands between menu.c and Ctrl_Tab.c

	Naming convention:
	function mask | usage    | purpose
	--------------|----------|-----------------------------
	Menu_Opt_*    | external | interface for menu.c
	CT_Opt_*      | external | interface for Ctrl_Tab.c
	M_Opt_*_f     | external | commands issued by the user
	Opt_          | internal | internal (static) functions

	made by:
		johnnycz, Jan 2006
	last edit:
		$Id: menu_options.c,v 1.2 2007-01-12 09:54:07 oldmanuk Exp $

*/

#include "quakedef.h"
#include "winquake.h"

typedef enum {
	OPTPG_SETTINGS,
	OPTPG_BINDS,
	OPTPG_VIDEO,
	OPTPG_PLAYER,
	OPTPG_FPS
}	options_tab_t;

CTab_t options_tab;
int options_unichar;	// variable local to this module

#define    OPTIONS_ITEMS    18

int        options_cursor;

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

#ifdef GLQUAKE
enum {mode_fastest, mode_faithful, mode_eyecandy, mode_movies} fps_mode = mode_faithful;
#else
enum {mode_fastest, mode_default} fps_mode = mode_default;
#endif

extern cvar_t scr_fov;

static void M_AdjustSliders (int dir) {
	float valuebuf;
	S_LocalSound ("misc/menu3.wav");

	switch (options_cursor) {
	case 3:    // screen size
		scr_viewsize.value += dir * 10;
		if (scr_viewsize.value < 30)
			scr_viewsize.value = 30;
		if (scr_viewsize.value > 120)
			scr_viewsize.value = 120;
		Cvar_SetValue (&scr_viewsize, scr_viewsize.value);
		break;
	case 4:    // gamma
		v_gamma.value -= dir * 0.05;
		if (v_gamma.value < 0.5)
			v_gamma.value = 0.5;
		if (v_gamma.value > 1)
			v_gamma.value = 1;
		Cvar_SetValue (&v_gamma, v_gamma.value);
		break;
	case 5:    // contrast
		v_contrast.value += dir * 0.1;
		if (v_contrast.value < 1)
			v_contrast.value = 1;
		if (v_contrast.value > 2)
			v_contrast.value = 2;
		Cvar_SetValue (&v_contrast, v_contrast.value);
		break;
	case 6:    // mouse speed
		sensitivity.value += dir * 0.5;
		if (sensitivity.value < 3)
			sensitivity.value = 3;
		if (sensitivity.value > 15)
			sensitivity.value = 15;
		Cvar_SetValue (&sensitivity, sensitivity.value);
		break;
	case 7:    // music volume
		valuebuf = scr_fov.value + dir * 5;
		valuebuf = bound(40, valuebuf, 140);
		Cvar_SetValue (&scr_fov, valuebuf);
		break;
	case 8:    // sfx volume
		s_volume.value += dir * 0.1;
		if (s_volume.value < 0)
			s_volume.value = 0;
		if (s_volume.value > 1)
			s_volume.value = 1;
		Cvar_SetValue (&s_volume, s_volume.value);
		break;

	case 9:    // always run
		if (cl_forwardspeed.value > 200) {
			Cvar_SetValue (&cl_forwardspeed, 200);
			Cvar_SetValue (&cl_backspeed, 200);
			Cvar_SetValue (&cl_sidespeed, 200);
		} else {
			Cvar_SetValue (&cl_forwardspeed, 400);
			Cvar_SetValue (&cl_backspeed, 400);
			Cvar_SetValue (&cl_sidespeed, 400);
		}
		break;

	case 10:    // mouse look
		Cvar_SetValue (&freelook, !freelook.value);
		break;

	case 11:    // invert mouse
		Cvar_SetValue (&m_pitch, -m_pitch.value);
		break;

	case 12:    // autoswitch weapons
		if ((w_switch.value > 2) || (!w_switch.value) || (b_switch.value > 2) || (!b_switch.value))
		{
			Cvar_SetValue (&w_switch, 1);
			Cvar_SetValue (&b_switch, 1);
		} else {
			Cvar_SetValue (&w_switch, 8);
			Cvar_SetValue (&b_switch, 8);
		}
		break;

	case 13:
		Cvar_SetValue (&cl_sbar, !cl_sbar.value);
		break;

	case 14:
		Cvar_SetValue (&cl_hudswap, !cl_hudswap.value);
		break;

	case 17:    // _windowed_mouse
		Cvar_SetValue (&_windowed_mouse, !_windowed_mouse.value);
		break;
	}
}

void CT_Opt_Settings_Draw (int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page) {
	float        r;
	char temp[32];
	mpic_t    *p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic (64, 4, p);

	M_Print (16, 32, "    Customize controls");
	M_Print (16, 40, "         Go to console");
	M_Print (16, 48, "     Reset to defaults");

	M_Print (16, 56, "           Screen size");
	r = (scr_viewsize.value - 30) / (120 - 30);
	M_DrawSlider (220, 56, r);

	M_Print (16, 64, "                 Gamma");
	r = (1.0 - v_gamma.value) / 0.5;
	M_DrawSlider (220, 64, r);

	M_Print (16, 72, "              Contrast");
	r = v_contrast.value - 1.0;
	M_DrawSlider (220, 72, r);

	M_Print (16, 80, "           Mouse speed");
	r = (sensitivity.value - 3)/(15 - 3);
	M_DrawSlider (220, 80, r);

	M_Print (16, 88, "         Field of view");
	r = (scr_fov.value - 40) / (140 - 40);
	M_DrawSlider (220, 88, r);

	M_Print (16, 96, "          Sound volume");
	r = s_volume.value;
	M_DrawSlider (220, 96, r);

	M_Print (16, 104,  "            Always run");
	M_DrawCheckbox (220, 104, cl_forwardspeed.value > 200);

	M_Print (16, 112, "            Mouse look");
	M_DrawCheckbox (220, 112, freelook.value);

	M_Print (16, 120, "          Invert mouse");
	M_DrawCheckbox (220, 120, m_pitch.value < 0);

	M_Print (16, 128, "    Autoswitch weapons");
	M_DrawCheckbox (220, 128, !w_switch.value || (w_switch.value > 2) || !b_switch.value || (b_switch.value > 2));

	M_Print (16, 136, "    Use old status bar");
	M_DrawCheckbox (220, 136, cl_sbar.value);

	M_Print (16, 144, "      HUD on left side");
	M_DrawCheckbox (220, 144, cl_hudswap.value);

	M_Print (16, 152, "     Graphics settings");
	switch (fps_mode) {
		case mode_fastest   : strcpy(temp, "fastest"); break;
#ifdef GLQUAKE
		case mode_faithful    : strcpy(temp, "faithful"); break;
		case mode_movies    : strcpy(temp, "movies"); break;
		default : strcpy(temp, "eye candy"); break;
#else
		default : strcpy(temp, "default"); break;
#endif
	}
	M_PrintWhite (220, 152, temp);

	if (vid_menudrawfn)
		M_Print (16, 160, "           Video modes");

#ifdef _WIN32
	if (modestate == MS_WINDOWED)
#else
	if (vid_windowedmouse)
#endif
	{
		M_Print (16, 168, "             Use mouse");
		M_DrawCheckbox (220, 168, _windowed_mouse.value);
	}

	// cursor
	M_DrawCharacter (200, 32 + options_cursor * 8, FLASHINGARROW());
}


int CT_Opt_Settings_Key (int k, CTab_t *tab, CTabPage_t *page) {
	int capped;

	capped = true;
	switch (k) {

	case K_ENTER:
		m_entersound = true;
		switch (options_cursor) {
			case 0:
				// todo: remove label for "binds"
				break;
			case 1:
				m_state = m_none;
				key_dest = key_console;
				//            Con_ToggleConsole_f ();
				break;
			case 2:
				Cbuf_AddText ("exec default.cfg\n");
				break;
			case 15:
				//        M_Menu_Fps_f ();
				switch (fps_mode) {
#ifdef GLQUAKE
					case mode_fastest:
						fps_mode = mode_faithful;
						break;
					case mode_faithful:
						fps_mode = mode_eyecandy;
						break;
					case mode_movies:
						fps_mode = mode_fastest;
						break;
					default:
						fps_mode = mode_movies;
						break;
#else
					case mode_fastest:
						fps_mode = mode_default;
						break;
					default:
						fps_mode = mode_fastest;
						break;
#endif
				}

				switch (fps_mode) {
#ifdef GLQUAKE
					case mode_fastest:
						Cbuf_AddText ("exec cfg/gfx_gl_fast.cfg\n");
						if (gl_max_size.value > 64) {
							Cvar_SetValue (&gl_max_size, 1);
						}
						break;
					case mode_faithful:
						Cbuf_AddText ("exec cfg/gfx_gl_faithful.cfg\n");
						Cvar_SetValue (&gl_max_size, gl_max_size_default);
						break;
					case mode_movies:
						Cbuf_AddText ("exec cfg/gfx_gl_movies.cfg\n");
						Cvar_SetValue (&gl_max_size, gl_max_size_default);
						break;
					default:
						Cbuf_AddText ("exec cfg/gfx_gl_eyecandy.cfg\n");
						Cvar_SetValue (&gl_max_size, gl_max_size_default);
						break;
#else
					case mode_fastest:
						Cbuf_AddText ("exec cfg/gfx_sw_fast.cfg\n");
						break;
					case mode_default:
						Cbuf_AddText ("exec cfg/gfx_sw_default.cfg\n");
						break;
#endif
				}

				break;
			case 16:
				// todo: remove from menu - M_Menu_Video_f ();
				break;
			default:
				M_AdjustSliders (1);
				break;
		}
		capped = true;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		options_cursor--;
		if (options_cursor < 0)
			options_cursor = OPTIONS_ITEMS - 1;
		capped = true;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		options_cursor++;
		if (options_cursor >= OPTIONS_ITEMS)
			options_cursor = 0;
		capped = true;
		break;

	case K_HOME:
		S_LocalSound ("misc/menu1.wav");
		options_cursor = 0;
		capped = true;
		break;

	case K_END:
		S_LocalSound ("misc/menu1.wav");
		options_cursor = OPTIONS_ITEMS - 1;
		capped = true;
		break;

	case K_LEFTARROW:
		M_AdjustSliders (-1);
		capped = true;
		break;

	case K_RIGHTARROW:
		M_AdjustSliders (1);
		capped = true;
		break;

	default: capped = false; break;
	}

	if (k == K_UPARROW || k == K_END || k == K_PGDN) {
#ifdef _WIN32
		if (options_cursor == 17 && modestate != MS_WINDOWED)
#else
		if (options_cursor == 17 && !vid_windowedmouse)
#endif
				options_cursor = 16;

		if (options_cursor == 16 && vid_menudrawfn == NULL)
			options_cursor = 15;
	} else {
		if (options_cursor == 16 && vid_menudrawfn == NULL)
			options_cursor = 17;

#ifdef _WIN32
		if (options_cursor == 17 && modestate != MS_WINDOWED)
#else
		if (options_cursor == 17 && !vid_windowedmouse)
#endif
				options_cursor = 0;
	}

	return capped;
}


//=============================================================================
/* BINDS MENU */

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

//void M_Menu_Keys_f (void) {
	// todo: legacy command
	// M_EnterMenu (m_keys);
//}

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



//=============================================================================
/* FPS SETTINGS MENU */

#define    FPS_ITEMS    15

int        fps_cursor = 0;

extern cvar_t v_bonusflash;
extern cvar_t cl_rocket2grenade;
extern cvar_t v_damagecshift;
extern cvar_t r_fastsky;
extern cvar_t r_drawflame;
extern cvar_t gl_part_inferno;

// static void M_Menu_Fps_f (void) {
	// todo: add legacy command for this
	// M_EnterMenu (m_fps);
// }

#define ALIGN_FPS_OPTIONS    208

void CT_Opt_FPS_Draw (int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page) {
	mpic_t    *p;
	char temp[32];

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/ttl_cstm.lmp");
	M_DrawPic (64, 4, p);

	M_Print (16, 32, "            Explosions");
	switch ((int) r_explosiontype.value) {
		case 0    : strcpy(temp, "fire + sparks"); break;
		case 1    : strcpy(temp, "fire only"); break;
		case 2    : strcpy(temp, "teleport"); break;
		case 3    : strcpy(temp, "blood"); break;
		case 4    : strcpy(temp, "big blood"); break;
		case 5    : strcpy(temp, "dbl gunshot"); break;
		case 6    : strcpy(temp, "blob effect"); break;
		case 7    : strcpy(temp, "big explosion"); break;
		default : strcpy(temp, "fire + sparks"); break;
	}
	M_Print (ALIGN_FPS_OPTIONS, 32, temp);

	M_Print (16, 40, "         Muzzleflashes");
	M_Print (ALIGN_FPS_OPTIONS, 40, cl_muzzleflash.value == 2 ? "own off" : cl_muzzleflash.value ? "on" : "off");

	M_Print (16, 48, "            Gib filter");
	M_DrawCheckbox (ALIGN_FPS_OPTIONS, 48, cl_gibfilter.value);

	M_Print (16, 56, "      Dead body filter");
	M_Print (ALIGN_FPS_OPTIONS, 56, cl_deadbodyfilter.value == 2 ? "on (instant)" : cl_deadbodyfilter.value ? "on (normal)" : "off");

	M_Print (16, 64, "          Rocket model");
	M_Print (ALIGN_FPS_OPTIONS, 64, cl_rocket2grenade.value ? "grenade" : "normal");

	switch ((int) r_rockettrail.value) {
		case 0    : strcpy(temp, "off"); break;
		case 1    : strcpy(temp, "normal"); break;
		case 2    : strcpy(temp, "grenade"); break;
		case 3    : strcpy(temp, "alt normal"); break;
		case 4    : strcpy(temp, "slight blood"); break;
		case 5    : strcpy(temp, "big blood"); break;
		case 6    : strcpy(temp, "tracer 1"); break;
		case 7    : strcpy(temp, "tracer 2"); break;
		default : strcpy(temp, "normal"); break;
	}

	M_Print (16, 72, "          Rocket trail");
	M_Print (ALIGN_FPS_OPTIONS, 72, temp);

	M_Print (16, 80, "          Rocket light");
	M_DrawCheckbox (ALIGN_FPS_OPTIONS, 80, r_rocketlight.value);

	M_Print (16, 88, "         Damage filter");
	M_DrawCheckbox (ALIGN_FPS_OPTIONS, 88, v_damagecshift.value == 0);

	M_Print (16, 96, "        Pickup flashes");
	M_DrawCheckbox (ALIGN_FPS_OPTIONS, 96, v_bonusflash.value);

	M_Print (16, 104, "         Powerup glow");
	M_Print (ALIGN_FPS_OPTIONS, 104, r_powerupglow.value==2 ? "own off" :
		r_powerupglow.value ? "on" : "off");

	M_Print (16, 112, "         Draw torches");
	M_DrawCheckbox (ALIGN_FPS_OPTIONS, 112, r_drawflame.value);

	M_Print (16, 120, "             Fast sky");
	M_DrawCheckbox (ALIGN_FPS_OPTIONS, 120, r_fastsky.value);

#ifdef GLQUAKE
	M_Print (16, 128, "          Fast lights");
	M_DrawCheckbox (ALIGN_FPS_OPTIONS, 128, gl_flashblend.value);
#endif

	M_PrintWhite (16, 136, "            Fast mode");

	M_PrintWhite (16, 144, "         High quality");

	// cursor
	M_DrawCharacter (196, 32 + fps_cursor * 8, FLASHINGARROW());
}

int CT_Opt_FPS_Key (int k, CTab_t *tab, CTabPage_t *page) {
	int i;

	switch (k) {
		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");
			fps_cursor--;
#ifndef GLQUAKE
			if (fps_cursor == 12)
				fps_cursor = 11;
#endif
			if (fps_cursor < 0)
				fps_cursor = FPS_ITEMS - 1;
			break;

		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");
			fps_cursor++;
#ifndef GLQUAKE
			if (fps_cursor == 12)
				fps_cursor = 13;
#endif
			if (fps_cursor >= FPS_ITEMS)
				fps_cursor = 0;
			break;

		case K_HOME:
			S_LocalSound ("misc/menu1.wav");
			fps_cursor = 0;
			break;

		case K_END:
			S_LocalSound ("misc/menu1.wav");
			fps_cursor = FPS_ITEMS - 1;
			break;

		case K_RIGHTARROW:
		case K_ENTER:
			S_LocalSound ("misc/menu2.wav");
			switch (fps_cursor) {
				case 0:
					i = r_explosiontype.value + 1;
					if (i > 7 || i < 0)
						i = 0;
					Cvar_SetValue (&r_explosiontype, i);
					break;
				case 1:
					Cvar_SetValue (&cl_muzzleflash, cl_muzzleflash.value == 2 ? 1 : cl_muzzleflash.value ? 0 : 2);
					break;
				case 2:
					Cvar_SetValue (&cl_gibfilter, !cl_gibfilter.value);
					break;
				case 3:
					Cvar_SetValue (&cl_deadbodyfilter, cl_deadbodyfilter.value == 2 ? 1 : cl_deadbodyfilter.value ? 0 : 2);
					break;
				case 4:
					Cvar_SetValue (&cl_rocket2grenade, !cl_rocket2grenade.value);
					break;
				case 5:
					i = r_rockettrail.value + 1;
					if (i < 0 || i > 7)
						i = 0;
					Cvar_SetValue (&r_rockettrail, i);
					break;
				case 6:
					Cvar_SetValue (&r_rocketlight, !r_rocketlight.value);
					break;
				case 7:
					Cvar_SetValue (&v_damagecshift, !v_damagecshift.value);
					break;
				case 8:
					Cvar_SetValue (&v_bonusflash, !v_bonusflash.value);
					break;
				case 9:
					i = r_powerupglow.value + 1;
					if (i < 0 || i > 2)
						i = 0;
					Cvar_SetValue (&r_powerupglow, i);
					break;
				case 10:
					Cvar_SetValue (&r_drawflame, !r_drawflame.value);
					break;
				case 11:
					Cvar_SetValue (&r_fastsky, !r_fastsky.value);
					break;

#ifdef GLQUAKE
				case 12:
					Cvar_SetValue (&gl_flashblend, !gl_flashblend.value);
					Cvar_SetValue (&r_dynamic, gl_flashblend.value);
					break;
#endif

					// fast
				case 13:
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
					break;

					// high quality
				case 14:
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
		default: return false;
	}
	return true;
}


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
	CTab_Init(&options_tab);
	CTab_AddPage(&options_tab, "settings", OPTPG_SETTINGS, NULL, CT_Opt_Settings_Draw, CT_Opt_Settings_Key);
	CTab_AddPage(&options_tab, "binds", OPTPG_BINDS, NULL, CT_Opt_Binds_Draw, CT_Opt_Binds_Key);
	CTab_AddPage(&options_tab, "video", OPTPG_VIDEO, NULL, CT_Opt_Video_Draw, CT_Opt_Video_Key);
	CTab_AddPage(&options_tab, "player", OPTPG_PLAYER, NULL, CT_Opt_Player_Draw, CT_Opt_Player_Key);
	CTab_AddPage(&options_tab, "fps", OPTPG_FPS, NULL, CT_Opt_FPS_Draw, CT_Opt_FPS_Key);
	CTab_SetCurrentId(&options_tab, OPTPG_SETTINGS);

	/*
	Cmd_AddCommand ("menu_keys", M_Menu_Keys_f);
	Cmd_AddCommand ("menu_fps", M_Menu_Fps_f);
	Cmd_AddCommand ("menu_video", M_Menu_Video_f);
	Cmd_AddCommand ("menu_setup", M_Menu_Setup_f);
	*/
}
