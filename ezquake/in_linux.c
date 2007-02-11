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

	$Id: in_linux.c,v 1.3 2007-02-11 23:28:16 qqshka Exp $
*/
#include "quakedef.h"


cvar_t	m_filter = {"m_filter", "0"};
cvar_t	cl_keypad = {"cl_keypad", "1"};
cvar_t	_windowed_mouse = {"_windowed_mouse",
#ifdef NDEBUG
	"1"
#else
			"0"
#endif
, CVAR_ARCHIVE};
// cvar_t _windowed_mouse = {"_windowed_mouse", "1", CVAR_ARCHIVE};

	
extern int mx, my;
extern qbool mouseinitialized;


void IN_StartupMouse( void );

#ifdef GLQUAKE
void IN_DeactivateMouse( void );
#endif

void IN_MouseMove (usercmd_t *cmd)
{
	static int old_mouse_x = 0, old_mouse_y = 0;
	float mouse_x, mouse_y;

//#if defined  (_Soft_X11) || defined (_Soft_SVGA)

	if (!mouseinitialized)
		return;

//#endif
#ifdef _Soft_SVGA
	// poll mouse values
	while (mouse_update())
#endif

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
	
	mx = my = 0; // clear for next update
}

void IN_Move (usercmd_t *cmd)
{
	IN_MouseMove(cmd);
}

void IN_Init (void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_INPUT_MOUSE);
	Cvar_Register(&m_filter);
#ifndef _Soft_SVGA
	Cvar_Register(&_windowed_mouse);
#endif

	Cvar_SetCurrentGroup(CVAR_GROUP_INPUT_KEYBOARD);
	Cvar_Register(&cl_keypad);
	Cvar_ResetCurrentGroup();

#ifdef WITH_KEYMAP
	IN_StartupKeymap();
#endif // WITH_KEYMAP

	if (!COM_CheckParm ("-nomouse"))
		IN_StartupMouse();
	else
    mouseinitialized = false;
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

//#endif
}

int ctrlDown = 0;
int shiftDown = 0;
int altDown = 0;

int isAltDown(void) {return altDown;}
int isCtrlDown(void) {return ctrlDown;}
int isShiftDown(void) {return shiftDown;}
