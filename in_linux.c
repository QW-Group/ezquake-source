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

	$Id: in_linux.c,v 1.12 2007-10-04 13:48:11 dkure Exp $
*/

#include "quakedef.h"
#include "input.h"
#include "keys.h"

#ifdef _Soft_SVGA
#include <vgamouse.h>
#endif

#ifdef WITH_JOYSTICK
#include <linux/joystick.h>

#define JOYSTICK_DEVICE_S "/dev/input/js0"
static int joystick = 0;
#endif // WITH_JOYSTICK

#if 0
// joystick defines and variables
// where should defines be moved?
#define JOY_ABSOLUTE_AXIS	0x00000000		// control like a joystick
#define JOY_RELATIVE_AXIS	0x00000010		// control like a mouse, spinner, trackball
#define	JOY_MAX_AXES		6				// X, Y, Z, R, U, V
#define JOY_AXIS_X			0
#define JOY_AXIS_Y			1
#define JOY_AXIS_Z			2
#define JOY_AXIS_R			3
#define JOY_AXIS_U			4
#define JOY_AXIS_V			5

enum _ControlList {
	AxisNada = 0, AxisForward, AxisLook, AxisSide, AxisTurn, AxisFly 
};

DWORD	dwAxisFlags[JOY_MAX_AXES] = {
	JOY_RETURNX, JOY_RETURNY, JOY_RETURNZ, JOY_RETURNR, JOY_RETURNU, JOY_RETURNV
};

DWORD	dwAxisMap[JOY_MAX_AXES];
DWORD	dwControlMap[JOY_MAX_AXES];
PDWORD	pdwRawValue[JOY_MAX_AXES];
#endif

#ifdef WITH_JOYSTICK
// none of these cvars are saved over a session.
// this means that advanced controller configuration needs to be executed each time.
// this avoids any problems with getting back to a default usage or when changing from one controller to another.
// this way at least something works.
cvar_t	in_joystick  = {"joystick","0",CVAR_ARCHIVE};
cvar_t	joy_name     = {"joyname", "joystick"};
cvar_t	joy_advanced = {"joyadvanced", "0"};
cvar_t	joy_advaxisx = {"joyadvaxisx", "0"};
cvar_t	joy_advaxisy = {"joyadvaxisy", "0"};
cvar_t	joy_advaxisz = {"joyadvaxisz", "0"};
cvar_t	joy_advaxisr = {"joyadvaxisr", "0"};
cvar_t	joy_advaxisu = {"joyadvaxisu", "0"};
cvar_t	joy_advaxisv = {"joyadvaxisv", "0"};
cvar_t	joy_forwardthreshold = {"joyforwardthreshold", "0.15"};
cvar_t	joy_sidethreshold = {"joysidethreshold", "0.15"};
cvar_t	joy_flysensitivity = {"joyflysensitivity", "-1.0"};
cvar_t  joy_flythreshold = {"joyflythreshold", "0.15"};
cvar_t	joy_pitchthreshold = {"joypitchthreshold", "0.15"};
cvar_t	joy_yawthreshold = {"joyyawthreshold", "0.15"};
cvar_t	joy_forwardsensitivity = {"joyforwardsensitivity", "-1.0"};
cvar_t	joy_sidesensitivity = {"joysidesensitivity", "-1.0"};
cvar_t	joy_pitchsensitivity = {"joypitchsensitivity", "1.0"};
cvar_t	joy_yawsensitivity = {"joyyawsensitivity", "-1.0"};
cvar_t	joy_wwhack1 = {"joywwhack1", "0.0"};
cvar_t	joy_wwhack2 = {"joywwhack2", "0.0"};
#endif // WITH_JOYSTICK

qbool	joy_avail, joy_advancedinit, joy_haspov;
DWORD		joy_oldbuttonstate, joy_oldpovstate;
//int			joy_id;
//DWORD		joy_flags;
DWORD		joy_numbuttons;

#if 0 
static JOYINFOEX	ji;
#endif

#ifdef WITH_JOYSTICK
void IN_StartupJoystick (void);
void IN_DeactivateJoystick (void);
void Joy_AdvancedUpdate_f (void);
void IN_JoyMove (usercmd_t *cmd);
#endif // WITH_JOYSTICK

#ifdef NDEBUG
#define win_mouse	"1"
#else
#define win_mouse	"0"
#endif

cvar_t	m_filter        = {"m_filter",       "0", CVAR_SILENT};
cvar_t	cl_keypad       = {"cl_keypad",      "1", CVAR_SILENT};
cvar_t	_windowed_mouse = {"_windowed_mouse", win_mouse, CVAR_ARCHIVE | CVAR_SILENT};
cvar_t	m_showrate      = {"m_showrate",     "0", CVAR_SILENT};

extern int mx, my;
extern qbool mouseinitialized;


void IN_StartupMouse( void );

#ifdef GLQUAKE
void IN_DeactivateMouse( void );
void IN_Restart_f(void);
#endif

float mouse_x, mouse_y;

void IN_MouseMove (usercmd_t *cmd)
{
	static int old_mouse_x = 0, old_mouse_y = 0;

//#if defined  (_Soft_X11) || defined (_Soft_SVGA)

	if (!mouseinitialized)
		return;

//#endif
#ifdef _Soft_SVGA
	// poll mouse values
	while (mouse_update())
		; // FIXME: is there was missed ; or it was ok ?
#endif

	//
	// Do not move the player if we're in HUD editor or menu mode.
	// And don't apply ingame sensitivity, since that will make movements jerky.
	//
	if(key_dest == key_hudeditor || key_dest == key_menu)
	{
		old_mouse_x = mouse_x = mx * cursor_sensitivity.value;
		old_mouse_y = mouse_y = my * cursor_sensitivity.value;
	}
	else
	{
		// Normal game mode.

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

		if (m_accel.value) {
			float mousespeed = (sqrt (mx * mx + my * my)) / (1000.0f * (float) cls.trueframetime);
			mouse_x *= (mousespeed * m_accel.value + sensitivity.value);
			mouse_y *= (mousespeed * m_accel.value + sensitivity.value);
		} else {
			mouse_x *= sensitivity.value;
			mouse_y *= sensitivity.value;
		}

		// add mouse X/Y movement to cmd
		if ((in_strafe.state & 1) || (lookstrafe.value && mlook_active))
			cmd->sidemove += m_side.value * mouse_x;
		else
			cl.viewangles[YAW] -= m_yaw.value * mouse_x;

		if (mlook_active)
			V_StopPitchDrift ();

		if (mlook_active && !(in_strafe.state & 1))
		{
			cl.viewangles[PITCH] += m_pitch.value * mouse_y;
			if (cl.viewangles[PITCH] > cl.maxpitch)
				cl.viewangles[PITCH] = cl.maxpitch;
			if (cl.viewangles[PITCH] < cl.minpitch)
				cl.viewangles[PITCH] = cl.minpitch;
		} else {
			cmd->forwardmove -= m_forward.value * mouse_y;
		}
	}

	mx = my = 0; // clear for next update
}

void IN_Move (usercmd_t *cmd)
{
	static struct timeval old_tv = {0, 0};
	struct timeval tv;
	static long old_mouserate = 0;
	long usec, mouserate;
	if (m_showrate.value && (mx || my)) {
		gettimeofday(&tv, NULL);
		usec = (tv.tv_sec - old_tv.tv_sec) * 1000000L + (tv.tv_usec - old_tv.tv_usec);
		mouserate = usec ? 1000000L / usec : old_mouserate;
		Com_Printf("mouse rate: %4ld\n", (mouserate + old_mouserate) / 2);
		old_tv = tv;
		old_mouserate = mouserate;
	}

	IN_MouseMove (cmd);
}

void IN_Init (void)
{
#ifdef GLQUAKE
#ifdef WITH_EVDEV
	int i;
#endif
#endif

	Cvar_SetCurrentGroup (CVAR_GROUP_INPUT_MOUSE);
	Cvar_Register (&m_filter);
	Cvar_Register (&m_showrate);
#ifndef _Soft_SVGA
	Cvar_Register (&_windowed_mouse);
#endif

	Cvar_SetCurrentGroup (CVAR_GROUP_INPUT_KEYBOARD);
	Cvar_Register (&cl_keypad);
	Cvar_ResetCurrentGroup ();

	if (!host_initialized)
	{
#ifdef GLQUAKE
		typedef enum { mt_none = 0, mt_dga, mt_normal, mt_evdev } mousetype_t;
		extern cvar_t in_mouse;
#ifdef WITH_EVDEV
		extern cvar_t in_mmt;
		extern cvar_t in_evdevice;
#endif /* !WITH_EVDEV */

		if (COM_CheckParm ("-nodga") || COM_CheckParm ("-nomdga"))
			Cvar_LatchedSetValue (&in_mouse, mt_normal);

#ifdef WITH_EVDEV
		if ((i = COM_CheckParm ("-mevdev")) && (i < COM_Argc() - 1)) {
			Cvar_LatchedSet (&in_evdevice, COM_Argv(i + 1));
			Cvar_LatchedSetValue (&in_mouse, mt_evdev);
		}

		if (COM_CheckParm ("-mmt"))
			Cvar_LatchedSetValue (&in_mmt, 1);
#endif /* !WITH_EVDEV */

		if (COM_CheckParm ("-nomouse"))
			Cvar_LatchedSetValue (&in_mouse, mt_none);

#ifdef WITH_EVDEV
		extern void IN_EvdevList_f(void);
		Cmd_AddCommand ("in_evdevlist", IN_EvdevList_f);
#endif /* !WITH_EVDEV */
#endif /* !GLQUAKE */

#ifdef WITH_KEYMAP
		IN_StartupKeymap();
#endif // WITH_KEYMAP
#ifdef GLQUAKE
		Cmd_AddCommand ("in_restart", IN_Restart_f);
#endif
	}

	IN_StartupMouse ();
#ifdef WITH_JOYSTICK
	IN_StartupJoystick ();
#endif // WITH_JOYSTICK
}

void IN_Shutdown(void)
{
//#if defined  (_Soft_X11) || defined (_Soft_SVGA)

#ifdef _Soft_SVGA
	if (mouseinitialized)
		mouse_close();
#endif

#ifdef GLQUAKE
  IN_DeactivateMouse(); // btw we trying de init this in video shutdown too...
#endif

	mouseinitialized = false;

#ifdef WITH_JOYSTICK
	IN_DeactivateJoystick();
#endif // WITH_JOYSTICK

//#endif
}

#ifdef WITH_JOYSTICK
void IN_StartupJoystick (void) { 
	int value = 0;
        char joy_desc[1024];
 
 	// assume no joystick
	joy_avail = false; 

	// only initialize if the user wants it
	if (!COM_CheckParm ("-joystick")) 
		return; 

	Cvar_SetCurrentGroup(CVAR_GROUP_INPUT_JOY);
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
	Cvar_Register (&joy_forwardsensitivity);
	Cvar_Register (&joy_sidesensitivity);
	Cvar_Register (&joy_flysensitivity);
	Cvar_Register (&joy_pitchsensitivity);
	Cvar_Register (&joy_yawsensitivity);
	Cvar_Register (&joy_wwhack1);
	Cvar_Register (&joy_wwhack2);
	Cvar_ResetCurrentGroup();

	Cmd_AddCommand ("joyadvancedupdate", Joy_AdvancedUpdate_f);
	
	joystick = open (JOYSTICK_DEVICE_S, O_RDONLY|O_NONBLOCK);
        if (joystick == 0) {
		Com_Printf ("\njoystick not found -- driver not present\n\n");
		return;
        }

        // save the joystick's number of buttons and POV status
        ioctl (joystick, JSIOCGNAME (sizeof (joy_desc)), joy_desc);
//	ioctl (joystick, JSIOCGAXES, &value);
        ioctl (joystick, JSIOCGBUTTONS, &value);
	joy_numbuttons = value;
//	joy_haspov = jc.wCaps & JOYCAPS_HASPOV;
	joy_haspov = 0;

	// old button and POV states default to no buttons pressed
	joy_oldbuttonstate = joy_oldpovstate = 0;

	// mark the joystick as available and advanced initialization not completed
	// this is needed as cvars are not available during initialization
	joy_avail = true; 
	joy_advancedinit = false;

	Com_Printf ("\njoystick detected (%s, %d buttons)\n\n", joy_desc, joy_numbuttons); 

//	DEBUG
//	printf ("\njoystick detected (%s, %d buttons)\n\n", joy_desc, joy_numbuttons);
//	fflush(stdout);
}

void IN_DeactivateJoystick (void) {
	if (joystick)
		close (joystick);
}

/*
PDWORD RawValuePointer (int axis) {
	switch (axis) {
	case JOY_AXIS_X:
		return &ji.dwXpos;
	case JOY_AXIS_Y:
		return &ji.dwYpos;
	case JOY_AXIS_Z:
		return &ji.dwZpos;
	case JOY_AXIS_R:
		return &ji.dwRpos;
	case JOY_AXIS_U:
		return &ji.dwUpos;
	case JOY_AXIS_V:
		return &ji.dwVpos;
	}
	return NULL;	// shut up compiler
}
*/

void Joy_AdvancedUpdate_f (void) {
/*
	// called once by IN_ReadJoystick and by user whenever an update is needed
	// cvars are now available
	int	i;
	DWORD dwTemp;

	// initialize all the maps
	for (i = 0; i < JOY_MAX_AXES; i++) {
		dwAxisMap[i] = AxisNada;
		dwControlMap[i] = JOY_ABSOLUTE_AXIS;
		pdwRawValue[i] = RawValuePointer(i);
	}

	if (!joy_advanced.value) {
		// default joystick initialization
		// 2 axes only with joystick control
		dwAxisMap[JOY_AXIS_X] = AxisTurn;
		// dwControlMap[JOY_AXIS_X] = JOY_ABSOLUTE_AXIS;
		dwAxisMap[JOY_AXIS_Y] = AxisForward;
		// dwControlMap[JOY_AXIS_Y] = JOY_ABSOLUTE_AXIS;
	} else {
		if (strcmp (joy_name.string, "joystick")) {
			// notify user of advanced controller
			Com_Printf ("\n%s configured\n\n", joy_name.string);
		}

		// advanced initialization here
		// data supplied by user via joy_axisn cvars
		dwTemp = (DWORD) joy_advaxisx.value;
		dwAxisMap[JOY_AXIS_X] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_X] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisy.value;
		dwAxisMap[JOY_AXIS_Y] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_Y] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisz.value;
		dwAxisMap[JOY_AXIS_Z] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_Z] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisr.value;
		dwAxisMap[JOY_AXIS_R] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_R] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisu.value;
		dwAxisMap[JOY_AXIS_U] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_U] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisv.value;
		dwAxisMap[JOY_AXIS_V] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_V] = dwTemp & JOY_RELATIVE_AXIS;
	}

	// compute the axes to collect from DirectInput
	joy_flags = JOY_RETURNCENTERED | JOY_RETURNBUTTONS | JOY_RETURNPOV;
	for (i = 0; i < JOY_MAX_AXES; i++) {
		if (dwAxisMap[i] != AxisNada)
			joy_flags |= dwAxisFlags[i];
	}
*/
}

void IN_CommandsJoystick (void) {
	int i, key_index;
	DWORD buttonstate;//, povstate;
        struct js_event js;

	if (!joy_avail)
		return;

	if (read (joystick, &js, sizeof (struct js_event)) != sizeof (struct js_event))
          return;

        switch (js.type & ~JS_EVENT_INIT) {
/*
		case JS_EVENT_AXIS:
			break;
*/
		case JS_EVENT_BUTTON:
			buttonstate = js.value;
			i = js.number;
			if (js.value) {
				key_index = (i < 4) ? K_JOY1 : K_AUX1;
				Key_Event (key_index + i, true);
			}

			if (!js.value) {
				key_index = (i < 4) ? K_JOY1 : K_AUX1;
				Key_Event (key_index + i, false);
			}
			joy_oldbuttonstate = buttonstate;
	} 

/*
	if (joy_haspov) {
		// convert POV information into 4 bits of state information
		// this avoids any potential problems related to moving from one
		// direction to another without going through the center position
		povstate = 0;
		if(ji.dwPOV != JOY_POVCENTERED) {
			if (ji.dwPOV == JOY_POVFORWARD)
				povstate |= 0x01;
			if (ji.dwPOV == JOY_POVRIGHT)
				povstate |= 0x02;
			if (ji.dwPOV == JOY_POVBACKWARD)
				povstate |= 0x04;
			if (ji.dwPOV == JOY_POVLEFT)
				povstate |= 0x08;
		}
		// determine which bits have changed and key an auxillary event for each change
		for (i = 0; i < 4; i++) {
			if ((povstate & (1 << i)) && !(joy_oldpovstate & (1 << i)))
				Key_Event (K_AUX29 + i, true);

			if (!(povstate & (1 << i)) && (joy_oldpovstate & (1 << i)))
				Key_Event (K_AUX29 + i, false);
		}
		joy_oldpovstate = povstate;
	}
*/
}

qbool IN_ReadJoystick (void) {
#if 0
	memset (&ji, 0, sizeof(ji));
	ji.dwSize = sizeof(ji);
	ji.dwFlags = joy_flags;

	if (joyGetPosEx (joy_id, &ji) == JOYERR_NOERROR) {
		// this is a hack -- there is a bug in the Logitech WingMan Warrior DirectInput Driver
		// rather than having 32768 be the zero point, they have the zero point at 32668
		// go figure -- anyway, now we get the full resolution out of the device
		if (joy_wwhack1.value != 0.0)
			ji.dwUpos += 100;
		return true;
	} else {
		// read error occurred
		// turning off the joystick seems too harsh for 1 read error,
		// but what should be done?
		// Com_Printf ("IN_ReadJoystick: no response\n");
		// joy_avail = false;
		return false;
	}
#else
	return true;
#endif
}

void IN_JoyMove (usercmd_t *cmd) {
/*
	float speed, aspeed, fAxisValue, fTemp, frametime;
	int i;

	if (Movie_IsCapturing() && movie_steadycam.value)
		frametime = movie_fps.value > 0 ? 1.0 / movie_fps.value : 1 / 30.0;
	else
		frametime = cls.trueframetime;

	// complete initialization if first time in
	// this is needed as cvars are not available at initialization time
	if( joy_advancedinit != true ) {
		Joy_AdvancedUpdate_f();
		joy_advancedinit = true;
	}

	// verify joystick is available and that the user wants to use it
	if (!joy_avail || !in_joystick.value)
		return; 
 
	// collect the joystick data, if possible
	if (IN_ReadJoystick () != true)
		return;

	speed = (in_speed.state & 1) ? cl_movespeedkey.value : 1;
	aspeed = speed * frametime;

	// loop through the axes
	for (i = 0; i < JOY_MAX_AXES; i++) {
		// get the floating point zero-centered, potentially-inverted data for the current axis
		fAxisValue = (float) *pdwRawValue[i];
		// move centerpoint to zero
		fAxisValue -= 32768.0;

		if (joy_wwhack2.value != 0.0) {
			if (dwAxisMap[i] == AxisTurn) {
				// this is a special formula for the Logitech WingMan Warrior
				// y=ax^b; where a = 300 and b = 1.3
				// also x values are in increments of 800 (so this is factored out)
				// then bounds check result to level out excessively high spin rates
				fTemp = 300.0 * pow(abs(fAxisValue) / 800.0, 1.3);
				if (fTemp > 14000.0)
					fTemp = 14000.0;
				// restore direction information
				fAxisValue = (fAxisValue > 0.0) ? fTemp : -fTemp;
			}
		}

		// convert range from -32768..32767 to -1..1 
		fAxisValue /= 32768.0;

		switch (dwAxisMap[i]) {
		case AxisForward:
			if ((joy_advanced.value == 0.0) && mlook_active) {
				// user wants forward control to become look control
				if (fabs(fAxisValue) > joy_pitchthreshold.value) {		
					// if mouse invert is on, invert the joystick pitch value
					// only absolute control support here (joy_advanced is false)
					if (m_pitch.value < 0.0)
						cl.viewangles[PITCH] -= (fAxisValue * joy_pitchsensitivity.value) * aspeed * cl_pitchspeed.value;
					else
						cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity.value) * aspeed * cl_pitchspeed.value;
					V_StopPitchDrift();
				} else {
					// no pitch movement
					// disable pitch return-to-center unless requested by user
					// *** this code can be removed when the lookspring bug is fixed
					// *** the bug always has the lookspring feature on
					if(lookspring.value == 0.0)
						V_StopPitchDrift();
				}
			} else {
				// user wants forward control to be forward control
				if (fabs(fAxisValue) > joy_forwardthreshold.value)
					cmd->forwardmove += (fAxisValue * joy_forwardsensitivity.value) * speed * cl_forwardspeed.value;
			}
			break;

		case AxisSide:
			if (fabs(fAxisValue) > joy_sidethreshold.value)
				cmd->sidemove += (fAxisValue * joy_sidesensitivity.value) * speed * cl_sidespeed.value;
			break;

		case AxisFly:
			if (fabs(fAxisValue) > joy_flythreshold.value)
				cmd->upmove += (fAxisValue * joy_flysensitivity.value) * speed * cl_upspeed.value;
			break;

		case AxisTurn:
			if ((in_strafe.state & 1) || (lookstrafe.value && mlook_active)) {
				// user wants turn control to become side control
				if (fabs(fAxisValue) > joy_sidethreshold.value)
					cmd->sidemove -= (fAxisValue * joy_sidesensitivity.value) * speed * cl_sidespeed.value;
			} else {
				// user wants turn control to be turn control
				if (fabs(fAxisValue) > joy_yawthreshold.value) {
					if(dwControlMap[i] == JOY_ABSOLUTE_AXIS)
						cl.viewangles[YAW] += (fAxisValue * joy_yawsensitivity.value) * aspeed * cl_yawspeed.value;
					else
						cl.viewangles[YAW] += (fAxisValue * joy_yawsensitivity.value) * speed * 180.0;
				}
			}
			break;

		case AxisLook:
			if (mlook_active) {
				if (fabs(fAxisValue) > joy_pitchthreshold.value) {
					// pitch movement detected and pitch movement desired by user
					if(dwControlMap[i] == JOY_ABSOLUTE_AXIS)
						cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity.value) * aspeed * cl_pitchspeed.value;
					else
						cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity.value) * speed * 180.0;
					V_StopPitchDrift();
				} else {
					// no pitch movement
					// disable pitch return-to-center unless requested by user
					// *** this code can be removed when the lookspring bug is fixed
					// *** the bug always has the lookspring feature on
					if(lookspring.value == 0.0)
						V_StopPitchDrift();
				}
			}
			break;

		default:
			break;
		}
	}

	// bounds check pitch
	//cl.viewangles[PITCH] = bound(-70, cl.viewangles[PITCH], 80);
	if (cl.viewangles[PITCH] > cl.maxpitch)
		cl.viewangles[PITCH] = cl.maxpitch;
	if (cl.viewangles[PITCH] < cl.minpitch)
		cl.viewangles[PITCH] = cl.minpitch;
*/
}
#endif // WITH_JOYSTICK

int ctrlDown = 0;
int shiftDown = 0;
int altDown = 0;

int isAltDown(void) {return altDown;}
int isCtrlDown(void) {return ctrlDown;}
int isShiftDown(void) {return shiftDown;}
