/*
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

#include <stdint.h>

#include "SDL.h"
#include "SDL_joystick.h"
#include "common.h"
#include "cvar.h"
#include "quakedef.h"
#include "input.h"
#include "keys.h"
#include "movie.h"

#ifndef _WIN32
#include <sys/time.h>
#endif

#ifdef __APPLE__
#include "in_osx.h"
#endif

static void in_joystick_callback(cvar_t *var, char *value, qbool *cancel);

cvar_t	m_filter        = {"m_filter",       "0", CVAR_SILENT};
cvar_t	cl_keypad       = {"cl_keypad",      "1", CVAR_SILENT};

/*  Joystick cvars.  */
cvar_t	in_joystick		= {"joystick",			"0",	CVAR_SILENT, in_joystick_callback };
cvar_t	joy_index		= {"joyindex",			"0",	CVAR_SILENT };
cvar_t	joy_name		= {"joyname",			"joystick", CVAR_SILENT };
cvar_t	joy_advanced		= {"joyadvanced",		"0",	CVAR_SILENT };
cvar_t	joy_advaxisx		= {"joyadvaxisx",		"0",	CVAR_SILENT };
cvar_t	joy_advaxisy		= {"joyadvaxisy",		"0",	CVAR_SILENT };
cvar_t	joy_advaxisz		= {"joyadvaxisz",		"0",	CVAR_SILENT };
cvar_t	joy_advaxisr		= {"joyadvaxisr",		"0",	CVAR_SILENT };
cvar_t	joy_advaxisu		= {"joyadvaxisu",		"0",	CVAR_SILENT };
cvar_t	joy_advaxisv		= {"joyadvaxisv",		"0",	CVAR_SILENT };
cvar_t	joy_forwardthreshold	= {"joyforwardthreshold",	"0.15",	CVAR_SILENT };
cvar_t	joy_sidethreshold	= {"joysidethreshold",		"0.15",	CVAR_SILENT };
cvar_t	joy_flythreshold	= {"joyflythreshold",		"0.15",	CVAR_SILENT };
cvar_t	joy_pitchthreshold	= {"joypitchthreshold",		"0.15",	CVAR_SILENT };
cvar_t	joy_yawthreshold	= {"joyyawthreshold",		"0.15",	CVAR_SILENT };
cvar_t	joy_flysensitivity	= {"joyflysensitivity",		"-1.0",	CVAR_SILENT };
cvar_t	joy_forwardsensitivity	= {"joyforwardsensitivity",	"-1.0",	CVAR_SILENT };
cvar_t	joy_sidesensitivity	= {"joysidesensitivity",	"-1.0",	CVAR_SILENT };
cvar_t	joy_pitchsensitivity	= {"joypitchsensitivity",	"1.0",	CVAR_SILENT };
cvar_t	joy_yawsensitivity	= {"joyyawsensitivity",		"-1.0",	CVAR_SILENT };


extern cvar_t	movie_steadycam, movie_fps;
extern int	mx, my;
extern qbool	mouseinitialized;

extern void IN_StartupMouse(void);
extern void IN_DeactivateMouse(void);
extern void IN_Restart_f(void);


/***************************************************************************
 * Mouse-y things.
 */
void IN_MouseMove (usercmd_t *cmd)
{
	static int old_mouse_x = 0, old_mouse_y = 0;

	if (!mouseinitialized)
		return;

	//
	// Do not move the player if we're in HUD editor or menu mode.
	// And don't apply ingame sensitivity, since that will make movements jerky.
	//
	if (! IN_MouseTrackingRequired())
	{
		// Normal game mode.
		float mouse_x, mouse_y;

		if (m_filter.value) {
			float filterfrac = bound (0.0f, m_filter.value, 1.0f) / 2.0f;
			mouse_x = (mx * (1.0f - filterfrac) + old_mouse_x * filterfrac);
			mouse_y = (my * (1.0f - filterfrac) + old_mouse_y * filterfrac);
		} else {
			mouse_x = mx;
			mouse_y = my;
		}

		old_mouse_x = mx;
		old_mouse_y = my;

		if (m_accel.value > 0.0f) {
			float accelsens = sensitivity.value;
			float mousespeed = (sqrt (mx * mx + my * my)) / (1000.0f * (float) cls.trueframetime);

			mousespeed -= m_accel_offset.value;
			if (mousespeed > 0) {
				mousespeed *= m_accel.value;
				if (m_accel_power.value > 1) {
					accelsens += exp((m_accel_power.value - 1) * log(mousespeed));
				} else {
					accelsens = 1;
				}
			}
			if (m_accel_senscap.value > 0 && accelsens > m_accel_senscap.value) {
				accelsens = m_accel_senscap.value;
			}

			mouse_x *= accelsens;
			mouse_y *= accelsens;
		} else {
			mouse_x *= sensitivity.value;
			mouse_y *= sensitivity.value;
		}

		// add mouse X/Y movement to cmd
		if ((in_strafe.state & 1) || (lookstrafe.value && mlook_active))
			cmd->sidemove += m_side.value * mouse_x;
		else if (!cl.paused || cls.demoplayback || cl.spectator)
			cl.viewangles[YAW] -= m_yaw.value * mouse_x;

		if (mlook_active)
			V_StopPitchDrift ();

		if (mlook_active && !(in_strafe.state & 1))
		{
			if (!cl.paused || cls.demoplayback || cl.spectator) {
				cl.viewangles[PITCH] += m_pitch.value * mouse_y;
			}
			if (cl.viewangles[PITCH] > cl.maxpitch)
				cl.viewangles[PITCH] = cl.maxpitch;
			if (cl.viewangles[PITCH] < cl.minpitch)
				cl.viewangles[PITCH] = cl.minpitch;
		} else {
			cmd->forwardmove -= m_forward.value * mouse_y;
		}
	}
	else {
		old_mouse_x = mx * cursor_sensitivity.value;
		old_mouse_y = my * cursor_sensitivity.value;
	}

	mx = my = 0; // clear for next update
}


/***************************************************************************
 * Stick-y things.
 */
enum joy_axes {
	JOY_AXIS_X = 0,
	JOY_AXIS_Y,
	JOY_AXIS_Z,
	JOY_AXIS_R,
	JOY_AXIS_U,
	JOY_AXIS_V,
	JOY_MAX_AXES,

	JOY_ABSOLUTE_AXIS = 0x0000,		// control like a joystick
	JOY_RELATIVE_AXIS = 0x0100,		// control like a mouse, spinner, trackball
};

enum _ControlList {
	AXIS__NONE = 0,
	AXIS_FORWARD,
	AXIS_LOOK,
	AXIS_SIDE,
	AXIS_TURN,
	AXIS_FLY,
};

static SDL_Joystick	*joy_dev;
static uint32_t		joy_prevbuttons;
static uint16_t		axis_map[JOY_MAX_AXES];
static uint16_t		control_map[JOY_MAX_AXES];
static uint16_t		joy_numbuttons,
			joy_numaxes;
static int16_t		joy_devidx = -1;
static uint8_t		joy_prevpov;
static qbool		joy_avail, joy_haspov, joy_advancedinit;

static void in_joystick_callback(cvar_t *var, char *value, qbool *cancel)
{
  if ((var == &in_joystick) && (atoi(value) != in_joystick.value)) {
    Cvar_SetValue(&in_joystick, atoi(value));
    IN_Restart_f();
  }
}

/*
 * The user needs to invoke this command after changing the 'joyadv*' cvars.
 */
static void
Joy_AdvancedUpdate_f (void)
{
	// Called once by IN_ReadJoystick and by user whenever an update is needed
	// cvars are now available.
	int		i;
	uint16_t	val;

	// initialize all the maps
	for (i = 0;  i < JOY_MAX_AXES;  ++i) {
		axis_map[i]	= AXIS__NONE;
		control_map[i]	= JOY_ABSOLUTE_AXIS;
		//pdwRawValue[i] = RawValuePointer(i);
	}

	if (!joy_advanced.integer) {
		// Default joystick initialization
		// 2 axes only with joystick control.
		axis_map[JOY_AXIS_X] = AXIS_TURN;
		// dwControlMap[JOY_AXIS_X] = JOY_ABSOLUTE_AXIS;
		axis_map[JOY_AXIS_Y] = AXIS_FORWARD;
		// dwControlMap[JOY_AXIS_Y] = JOY_ABSOLUTE_AXIS;
	}
	else
	{
		if (strcmp (joy_name.string, "joystick")) {
			// Notify user of advanced controller.
			Com_Printf ("\n%s configured\n\n", joy_name.string);
		}

		// Advanced initialization here
		// data supplied by user via joy_axisn cvars.
		// Axis index in bits 7:0.  Relative/absolute flag
		// is bit 8.
		val = joy_advaxisx.integer;
		axis_map[JOY_AXIS_X] = val & 0x00ff;
		control_map[JOY_AXIS_X] = val & JOY_RELATIVE_AXIS;
		val = joy_advaxisy.integer;
		axis_map[JOY_AXIS_Y] = val & 0x00ff;
		control_map[JOY_AXIS_Y] = val & JOY_RELATIVE_AXIS;
		val = joy_advaxisz.integer;
		axis_map[JOY_AXIS_Z] = val & 0x00ff;
		control_map[JOY_AXIS_Z] = val & JOY_RELATIVE_AXIS;
		val = joy_advaxisr.integer;
		axis_map[JOY_AXIS_R] = val & 0x00ff;
		control_map[JOY_AXIS_R] = val & JOY_RELATIVE_AXIS;
		val = joy_advaxisu.integer;
		axis_map[JOY_AXIS_U] = val & 0x00ff;
		control_map[JOY_AXIS_U] = val & JOY_RELATIVE_AXIS;
		val = joy_advaxisv.integer;
		axis_map[JOY_AXIS_V] = val & 0x00ff;
		control_map[JOY_AXIS_V] = val & JOY_RELATIVE_AXIS;
	}

	/*
	 * If the requested joystick device index has changed, try opening it.
	 */
	if (joy_devidx != joy_index.integer) {
		SDL_Joystick *newdev;

		newdev = SDL_JoystickOpen (joy_index.integer);
		if (newdev) {
			if (joy_dev) {
				/*  Close the old one.  */
				SDL_JoystickClose (joy_dev);
			}
			joy_dev		= newdev;
			joy_devidx	= joy_index.integer;
			joy_numaxes	= SDL_JoystickNumAxes (joy_dev);
			joy_numbuttons	= SDL_JoystickNumButtons (joy_dev);
			joy_haspov	= (SDL_JoystickNumHats (joy_dev) > 0);
			if (joy_numbuttons > sizeof (joy_prevbuttons) * 8) {
				/*  We can't handle more than 32 buttons.  */
				joy_numbuttons = sizeof (joy_prevbuttons) * 8;
			}
			Con_Printf ("Opened joystick index %d: \"%s\"\n",
			            joy_index.integer, SDL_JoystickName (joy_dev));
		} else {
			Con_Printf ("Failed to open joystick index %d\n", joy_index.integer);
		}
	}
}

void
IN_Commands (void)
{
	uint32_t	buttonstate, buttonmask;
	int		i, base_key;

	if (!joy_avail  ||  !joy_dev  ||  !in_joystick.integer)
		return;

	/*
	 * Loop through the joystick buttons
	 * First four buttons are mapped to K_JOY[1-4].  Buttons after that are
	 * mapped to K_AUX1+.  POV directions are mapped to K_JOYPOV{UP,RT,DN,LT}.
	 */
	buttonstate = 0;
	for (i = 0;  i < joy_numbuttons;  ++i) {
		uint8_t val = SDL_JoystickGetButton (joy_dev, i);

		buttonstate |= val << i;
		buttonmask = 1 << i;

		if ((buttonstate ^ joy_prevbuttons) & buttonmask) {
			base_key = i < 4 ?  K_JOY1 :  K_AUX1 - 4;
			Key_Event (base_key + i, val != 0);
		}
	}
	joy_prevbuttons = buttonstate;

	if (joy_haspov) {
		/*
		 * SDL_JoystickGetHat() returns a bitmask of the four cardinal
		 * directions in up/right/down/left order from bit 0.  Only
		 * the first POV hat is used.
		 */
		uint8_t povstate = SDL_JoystickGetHat (joy_dev, 0);
		uint8_t povmask;

		for (i = 0;  i < 4;  ++i) {
			povmask = 1 << i;

			if ((povstate ^ joy_prevpov) & povmask) {
				Key_Event (K_JOYPOVUP + i, !!(povstate & povmask));
			}
		}
		joy_prevpov = povstate;
	}
}

static void
IN_JoyMove (usercmd_t *cmd)
{
	float	speed, aspeed, frametime;
	float	axisval;
	int	i;

	if (!in_joystick.integer) return;

	if (Movie_IsCapturing()  &&  movie_steadycam.value) {
		frametime = movie_fps.value > 0 ?  1.0 / movie_fps.value
		                                :  1.0 / 30.0;
	} else {
		frametime = cls.trueframetime;
	}

	// Complete initialization if first time in
	// this is needed as cvars are not available at initialization time.
	if (!joy_advancedinit)
	{
		Joy_AdvancedUpdate_f();
		joy_advancedinit = true;
	}

	// Verify joystick is available and that the user wants to use it.
	if (!joy_avail  ||  !joy_dev)
		return;

	speed = (in_speed.state & 1) ? cl_movespeedkey.value : 1.0;
	aspeed = speed * frametime;

	// Loop through the axes.
	for (i = 0;  i < JOY_MAX_AXES;  ++i)
	{
		if (axis_map[i] == AXIS__NONE  ||  i >= joy_numaxes) {
			continue;
		}
		axisval = SDL_JoystickGetAxis (joy_dev, i);

		// Convert range from -32768..32767 to -1..1
		axisval /= 32768.0;

		switch (axis_map[i]) {
		case AXIS_FORWARD:
			if (joy_advanced.integer == 0  &&  mlook_active)
			{
				// user wants forward control to become look control
				if (fabs (axisval) > joy_pitchthreshold.value) {
					// if mouse invert is on, invert the joystick pitch value
					// only absolute control support here (joy_advanced is false)
					if (m_pitch.value < 0.0)
						cl.viewangles[PITCH] -= (axisval * joy_pitchsensitivity.value) * aspeed * cl_pitchspeed.value;
					else
						cl.viewangles[PITCH] += (axisval * joy_pitchsensitivity.value) * aspeed * cl_pitchspeed.value;
					V_StopPitchDrift();
				} else {
					// no pitch movement
					// disable pitch return-to-center unless requested by user
					// *** this code can be removed when the lookspring bug is fixed
					// *** the bug always has the lookspring feature on
					if (lookspring.value == 0.0)
						V_StopPitchDrift();
				}
			} else {
				// user wants forward control to be forward control
				if (fabs (axisval) > joy_forwardthreshold.value)
					cmd->forwardmove += (axisval * joy_forwardsensitivity.value) * speed * cl_forwardspeed.value;
			}
			break;

		case AXIS_SIDE:
			if (fabs (axisval) > joy_sidethreshold.value)
				cmd->sidemove += (axisval * joy_sidesensitivity.value) * speed * cl_sidespeed.value;
			break;

		case AXIS_FLY:
			if (fabs (axisval) > joy_flythreshold.value)
				cmd->upmove += (axisval * joy_flysensitivity.value) * speed * cl_upspeed.value;
			break;

		case AXIS_TURN:
			if ((in_strafe.state & 1)  ||  (lookstrafe.value  &&  mlook_active)) {
				// user wants turn control to become side control
				if (fabs (axisval) > joy_sidethreshold.value)
					cmd->sidemove -= (axisval * joy_sidesensitivity.value) * speed * cl_sidespeed.value;
			} else {
				// user wants turn control to be turn control
				if (fabs (axisval) > joy_yawthreshold.value) {
					if (control_map[i] == JOY_ABSOLUTE_AXIS)
						cl.viewangles[YAW] += (axisval * joy_yawsensitivity.value) * aspeed * cl_yawspeed.value;
					else
						cl.viewangles[YAW] += (axisval * joy_yawsensitivity.value) * speed * 180.0;
				}
			}
			break;

		case AXIS_LOOK:
			if (mlook_active)
			{
				if (fabs (axisval) > joy_pitchthreshold.value)
				{
					// pitch movement detected and pitch movement desired by user
					if (control_map[i] == JOY_ABSOLUTE_AXIS)
						cl.viewangles[PITCH] += (axisval * joy_pitchsensitivity.value) * aspeed * cl_pitchspeed.value;
					else
						cl.viewangles[PITCH] += (axisval * joy_pitchsensitivity.value) * speed * 180.0;
					V_StopPitchDrift();
				}
				else
				{
					// no pitch movement
					// disable pitch return-to-center unless requested by user
					// *** this code can be removed when the lookspring bug is fixed
					// *** the bug always has the lookspring feature on
					if (lookspring.value == 0.0)
						V_StopPitchDrift();
				}
			}
			break;

		default:
			break;
		}
	}

	// Bounds check pitch
	//cl.viewangles[PITCH] = bound (-70, cl.viewangles[PITCH], 80);
	if (cl.viewangles[PITCH] > cl.maxpitch)
		cl.viewangles[PITCH] = cl.maxpitch;
	if (cl.viewangles[PITCH] < cl.minpitch)
		cl.viewangles[PITCH] = cl.minpitch;
}

static void
IN_StartupJoystick (void)
{
	SDL_Joystick	*jdev;
	int		numdevs;

	//COM_CheckParm ("-joystick");
	
	if (!host_initialized) {
		Cvar_SetCurrentGroup (CVAR_GROUP_INPUT_JOYSTICK);
		Cvar_Register (&in_joystick);
		Cvar_Register (&joy_index);
		Cvar_Register (&joy_name);
		Cvar_Register (&joy_advanced);
		Cvar_Register (&joy_advaxisx);
		Cvar_Register (&joy_advaxisy);
		Cvar_Register (&joy_advaxisz);
		Cvar_Register (&joy_advaxisr);
		Cvar_Register (&joy_advaxisu);
		Cvar_Register (&joy_advaxisv);
		Cvar_Register (&joy_forwardthreshold);
		Cvar_Register (&joy_sidethreshold);
		Cvar_Register (&joy_flythreshold);
		Cvar_Register (&joy_pitchthreshold);
		Cvar_Register (&joy_yawthreshold);
		Cvar_Register (&joy_flysensitivity);
		Cvar_Register (&joy_forwardsensitivity);
		Cvar_Register (&joy_sidesensitivity);
		Cvar_Register (&joy_pitchsensitivity);
		Cvar_Register (&joy_yawsensitivity);
		Cvar_ResetCurrentGroup();

		Cmd_AddCommand ("joyadvancedupdate", Joy_AdvancedUpdate_f);
	}

	if (!in_joystick.value) return;

	if (SDL_WasInit (SDL_INIT_JOYSTICK) == 0) {
		int ret = SDL_InitSubSystem (SDL_INIT_JOYSTICK);
		if (ret == -1) {
			Com_Printf ("\nSDL joystick subsystem init failed.\n");
			return;
		}
	}

	/*  See if there are any joysticks at all.  */
	numdevs = SDL_NumJoysticks();
	if (!numdevs) {
		Com_Printf ("\nno joysticks detected by SDL\n\n");
		return;
	}

	/*  Check if we can open any of them.  */
	for (int i = 0;  i < numdevs;  ++i) {
		jdev = SDL_JoystickOpen (i);
		if (jdev) {
			Com_Printf ("Detected joystick %d: %d axes, %d buttons: \"%s\"\n",
			            i,
			            SDL_JoystickNumAxes (jdev),
			            SDL_JoystickNumButtons (jdev),
			            SDL_JoystickName (jdev));
			SDL_JoystickClose (jdev);
			joy_avail = true;
		}
	}
}

void
IN_DeactivateJoystick (void)
{
	// Close the device here.
	return;
}

void IN_Move (usercmd_t *cmd)
{
	IN_MouseMove (cmd);
	IN_JoyMove (cmd);
}

void IN_Init (void)
{
	Cvar_SetCurrentGroup (CVAR_GROUP_INPUT_MOUSE);
	Cvar_Register (&m_filter);

	Cvar_SetCurrentGroup (CVAR_GROUP_INPUT_KEYBOARD);
	Cvar_Register (&cl_keypad);
	Cvar_ResetCurrentGroup ();

	if (!host_initialized) {
		Cmd_AddCommand("in_restart", IN_Restart_f);
	}

	IN_StartupMouse ();
	IN_StartupJoystick();
}

void IN_Shutdown(void)
{
	IN_DeactivateMouse(); // btw we trying de init this in video shutdown too...

#ifdef __APPLE__
	OSX_Mouse_Shutdown(); // Safe to call, will just return if it's not running
#endif

	mouseinitialized = false;
}

