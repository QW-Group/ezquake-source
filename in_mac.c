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
// in_mac.c -- macintosh mouse support using InputSprocket

#include "quakedef.h"

#include <Quickdraw.h>
#include <DrawSprocket/DrawSprocket.h>
#include <AGL/agl.h>
#include <Errors.h>

#include <CarbonEvents.h>
#include <CGDirectDisplay.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <drivers/event_status_driver.h>

#include "mac.h"

#define MIN_PITCH		-70
#define MAX_PITCH		80

extern CGDirectDisplayID vid_CGDevices[];
extern CGDisplayCount gCGNumDevices;

extern void OnMouseButton (EventRecord *myEvent, qbool down);

static pascal OSStatus IN_CarbonMouseEvents (EventHandlerCallRef handlerChain, EventRef event, void *userData);

// Points to either ISp or CG mouse delta routines
void (*IN_GetDelta)(void);
static void IN_GetDelta_CG (void);

int mouse_buttons = 3;
qbool suspend_mouse = false;
qbool disconneted_mouse = false;

#ifndef DEBUG
#define DEBUG 0
#endif

/*  ISp - re-worked this a bit... It's essentially the
	same deal with nicer icons and hacky dpad support.
	Joystick axis is not quite working (not that I have a joystick to test with...)
*/
#define CONTROL_NEEDS	23
//#define ISP_AXIS      0	// FIXME!
#define ISP_DELTA      	0
#define ISP_MWHEEL      2
#define	ISP_BUTTONS	   	3
#define	ISP_AUX			6
#define	ISP_DPAD		22

qbool Sprockets_Active = false;

// External prototypes
extern void Mac_StartGL (void);
extern void Mac_StopGL (void);

extern qbool is3dfx, inwindow;
extern int gScreenWidth;
extern int gScreenHeight;
extern AGLContext gContext;
extern DSpContextReference gSprocketContext;
extern DisplayIDType gDisplayID;
extern Fixed gRefreshRate;	

// Function Protos
void IN_ShowCursor (void);
void IN_HideCursor (void);
void IN_SuspendMouse (void);
void IN_ResumeMouse (void);
void IN_ConnectMouse(void);
void IN_DisconnectMouse(void);

// pOx - mouse smoothing...
cvar_t m_filter = {"m_filter","0.1", true};
cvar_t m_filtercutoff = {"m_filtercutoff","80", true};
#define	M_FILTERLEVEL	 10
static float	history[M_FILTERLEVEL][2];
static qbool	filter_dirty;

// compatibility with old Quake -- setting to 0 disables KP_* codes
cvar_t	cl_keypad = {"cl_keypad","0"};

// These are only meant to be used internally (despite the _f)
void Release_Cursor_f (void)
{
	IN_SuspendMouse();
	IN_ShowCursor();
}

void Capture_Cursor_f (void)
{
	IN_HideCursor();
	IN_ResumeMouse();
}

void Force_Centerview_f (void)
{
	if (concussioned) return;
	cl.viewangles[PITCH] = 0; 
}


/*
=============
IN_ClearMouseFilter

Clear the history buffer if no movement
=============
*/
static void IN_ClearMouseFilter (void)
{
	int	i;
   
	for (i = 0; i < M_FILTERLEVEL; i++)
		history[i][0] = history[i][1] = 0;
	
	filter_dirty = false;
}

/*
=============
IN_Init

startup InputSprockets mouse input
TODO: add check to ensure InputSprockets is present.
============= 
*/
void IN_Init (void)
{
	// OSX Mouse events
	EventTypeSpec appEventList[] = {
	{kEventClassMouse, kEventMouseDown},
	{kEventClassMouse, kEventMouseUp},
	{kEventClassMouse, kEventMouseWheelMoved},
	{kEventClassMouse, kEventMouseMoved},
	{kEventClassMouse, kEventMouseDragged}
	};
	
	//InstallStandardEventHandler(GetApplicationEventTarget());// Need this?
	InstallApplicationEventHandler(NewEventHandlerUPP(IN_CarbonMouseEvents), 5, appEventList, 0, NULL);
	
	Sprockets_Active = false;
	
	// Set the get delta pointer to the CoreGraphics method
	IN_GetDelta = (void *)IN_GetDelta_CG;
	 
	IN_ConnectMouse ();
	
	// These are used internally, but we can expose them for testing
	// NOTE: These don't really work in windowed mode! (since auto capture/release is in effect)
	if (COM_CheckParm ("-indebug"))
	{
		Cmd_AddCommand ("_cursor_capture", Capture_Cursor_f);
		Cmd_AddCommand ("_cursor_release", Release_Cursor_f);
	}
	
	Cmd_AddCommand ("force_centerview", Force_Centerview_f);

	Cvar_Register (&m_filtercutoff);
	Cvar_Register (&m_filter);
	Cvar_Register (&cl_keypad);
	
    Sys_Printf("IN_Init success\n");
}

/*
============= 
IN_Shutdown

shutdown InputSprockets mouse input
============= 
*/
void IN_Shutdown (void)
{
	
#ifdef DISABLE_MOUSE
	return;
#endif

    IN_DisconnectMouse();

	return;
}


extern Point			glWindowPos;
static SInt32 			post_the_scroll_hack = 0;
static CGMouseDelta 	CG_dx = 0, CG_dy = 0;
static int				mouse_moved = FALSE;

/*
=============
IN_CenterMouse

Recenter the cursor on X so we don't hang on edges, and to ensure clicks hit the window.
============= 
*/
static void IN_CenterMouse (void)
{
	CGPoint 		spot;

	if (inwindow)
	{	
		spot.x = glWindowPos.h+gScreen.mode->width*0.5;
		spot.y = glWindowPos.v+gScreen.mode->height*0.5;
	}
	else
	{
		spot.x = gScreen.mode->width*0.5;
		spot.y = gScreen.mode->height*0.5;
    }
    
	// Should use "CGGetDisplaysWithPoint" to find the ID in windowed mode?
    CGDisplayMoveCursorToPoint (vid_CGDevices[gScreen.profile->screen], spot);
}

/*
============= 
IN_OverrideSystemPrefs

Disable system mouse scaling and key repeat so controls are consistant in Quake.
To make life easy, we use a mix of NX and HID functions (the NX scaling is either broken or out of date)
According to Darwin's IOEventStatusAPI.c, NXEventHandle & io_connect_t are interchangeable.
Keyboard repeat can adversley affect the mouse on slower systems (?) so it's enabled/disabled in sync with mouse scaling.
============= 
*/
static void IN_OverrideSystemPrefs (qbool state)
{
    NXEventHandle 			handle;
    static qbool         enabled = true;
	static double 			scalingSave = 0.0;
	static double			keyIntervalSave, keyThresholdSave;
	
    if (state == enabled)
        return;
        
    if (!(handle = NXOpenEventStatus ()))
    {
    	Com_DPrintf ("NXOpenEventStatus Failed!\n");
        return;
	}
	
    if (state)
    {
        // Restore Mouse System pref
        IOHIDSetAccelerationWithKey ((io_connect_t)handle, CFSTR(kIOHIDMouseAccelerationType), scalingSave);
    	
    	// Restore Key Repeat System pref
//    	NXSetKeyRepeatInterval (handle, keyIntervalSave);
//    	NXSetKeyRepeatThreshold (handle, keyThresholdSave);
//		NXResetKeyboard (handle);
    }
    else
    {
    	kern_return_t	status;
		
		// Save old mouse speed & disable scaling
		status = IOHIDGetAccelerationWithKey ((io_connect_t)handle, CFSTR(kIOHIDMouseAccelerationType), &scalingSave);
        if (status != kIOReturnSuccess)// Something bad happened...
        	Sys_Printf ("Couldn't get system mouse scaling setting.\n");
        else
            IOHIDSetAccelerationWithKey ((io_connect_t)handle, CFSTR(kIOHIDMouseAccelerationType), -1.0);
            
        // Save old keyboard repeat states & disable
		keyIntervalSave = NXKeyRepeatInterval (handle);
		keyThresholdSave = NXKeyRepeatThreshold (handle);
//		NXSetKeyRepeatInterval (handle, 3456000.0f);
//		NXSetKeyRepeatThreshold (handle, 3456000.0f);
		
		IN_CenterMouse();
		mouse_moved = FALSE;
		CG_dx = CG_dy = 0;
    }
    
    NXCloseEventStatus (handle);
    enabled = state;
}

/*
=============
IN_CarbonMouseEvents

Uses Carbon Events to handle mouse input.
This relies on the Get/WaitNextEvent call in HandleEvents being fired in the main loop.
Using GetNextEvent instead of a timer lets us hog more CPU (which is good in this case).
WaitNextEvent with a 'big' sleep time is used in background.

NOTE: OS 8/9 relies on InputSprocket, and never touches this.
============= 
*/
static pascal OSStatus IN_CarbonMouseEvents (EventHandlerCallRef handlerChain, EventRef event, void *userData)
{
	UInt32 				event_kind;
	UInt32 				event_class;
	EventMouseButton	which_mouse_button;
	SInt32				scroll_wheel_delta;
	OSStatus 			err = eventNotHandledErr;
	CGMouseDelta 		dx, dy;
	extern qbool		background;

	event_kind = GetEventKind(event);
	event_class = GetEventClass(event);
	
	// Totally ignore the mouse if we're shutting down (so error dialogs can handle it properly)
	if (disconneted_mouse) return err;
	
	if (handlerChain)
		err = CallNextEventHandler (handlerChain, event);
		
	if (err == noErr || err == eventNotHandledErr)
	{
		if (event_class == kEventClassMouse)
		{			
			// If we're windowed, and the mouse is enabled, watch for a window drag
			// X prefers this to mouseDown in the main event loop...
			if (suspend_mouse)
			{
				if (event_kind == kEventMouseDown)
				{					
					if (inwindow)
					{
						EventRecord eventRec;
						ConvertEventRefToEventRecord(event, &eventRec);
						OnMouseButton(&eventRec, true);
					}
					
					// So click in About box works...
					if (background || DEBUG) return err;
				}
				return noErr;
			}
			
			switch (event_kind)
			{
				// Carbon tells us when the mouse moves, but CoreGraphics gets the delta
				case kEventMouseMoved:
				case kEventMouseDragged:
					
					CGGetLastMouseDelta (&dx, &dy);
					CG_dx += dx; CG_dy += dy;// Accumulate the delta since we can move *between* frames
					mouse_moved = TRUE;
					//IN_CenterMouse();
					
					err = noErr;
					break;

				case kEventMouseDown:
				case kEventMouseUp:
					
					GetEventParameter(event, kEventParamMouseButton, typeMouseButton, NULL,
									  sizeof(EventMouseButton), NULL, &which_mouse_button);
					
					if (which_mouse_button > 3)
						which_mouse_button += 4;// Jump to AUX keys to match ISp behaviour
					
					Key_Event(K_MOUSE1 - 1 + which_mouse_button, (event_kind == kEventMouseDown));
					
					err = noErr;
					break;
				
				case kEventMouseWheelMoved:
					
					GetEventParameter(event, kEventParamMouseWheelDelta, typeLongInteger,
									  NULL, sizeof(SInt32), NULL, &scroll_wheel_delta);
					
					post_the_scroll_hack = scroll_wheel_delta>0?K_MWHEELUP:K_MWHEELDOWN;// make sure keyup event happens
					Key_Event(post_the_scroll_hack, true);
					
					err = noErr;
					break;
				
				default:
					break;
			}
		}
	}
	return err;
}


/*
=============
IN_Commands

check for mouse button presses
============= 
*/
void IN_Commands (void)
{
#ifdef DISABLE_MOUSE
	return;
#endif
	
	if (suspend_mouse) return;

	// make sure keyup event happens
	if (post_the_scroll_hack) {
		Key_Event(post_the_scroll_hack, false);
		post_the_scroll_hack = 0;
	}
	return;
}

static float posx = 0, posy = 0;

/*
=============
IN_GetDelta_CG

CoreGraphics mouse delta (OSX)
Actually gets the delta in IN_CarbonMouseEvents, we just clean it up here.
FIXME! - How do you 'clear' the delta? (Initial centering move after capturing the cursor seems to count)
============= 
*/
static void IN_GetDelta_CG (void)
{
	static float lastx = 0, lasty = 0;	

	if (mouse_moved)
	{
		// Give m_filter a head start...
		if (m_filter.value)
		{
			posx = ((float)CG_dx + lastx)*0.5;
			posy = ((float)CG_dy + lasty)*0.5;	
		}
		else
		{
			posx = (float)CG_dx;
			posy = (float)CG_dy;
		}
				
		mouse_moved = FALSE;
	}
	else
		posx = posy = 0;
	
	// Doesn't happen on it's own
	CG_dx = CG_dy = 0;
	IN_CenterMouse();
	
	lastx = posx; lasty = posy;
}

/*
=============
IN_Move

check for mouse movement
============= 
*/
void IN_Move (usercmd_t *cmd)
{
	float 	weight = 1.0;
	float 	total = 0.0;
	float	hisx, hisy, cutoff;
	int		i;
  	
#ifdef DISABLE_MOUSE
	return;
#endif

	if (suspend_mouse || scr_disabled_for_loading)
	{
		CG_dx = CG_dy = 0;
		posx = posy = 0;
		if (filter_dirty) IN_ClearMouseFilter();
		memset (cmd, 0, sizeof(*cmd));
		return;
	}

	IN_GetDelta ();// actual funtion depends on the current OS
	
	// if no change in position, return
	if (!posx && !posy)
	{
		if (filter_dirty) IN_ClearMouseFilter();
		return;
	}	

// DEBUG - ISp & CG should end up with (around) the same delta by the time we get here.
//if (temp1.value) Com_Printf ("\3IX: %3.2f IY: %3.2f\n", posx, posy);

	posx *= sensitivity.value;
    posy *= sensitivity.value;
	
	if (m_filter.value)
	{
		// The old cutoff values don't make much sense anymore...
		cutoff = m_filtercutoff.value*0.1;
		
		// Don't let this go over 1
		if (m_filter.value > 1.0) m_filter.value = 1.0;
		
		// Bump up sensitivity a bit cause the filter makes ya go SLOOOOW....
		posx *= (1.0+m_filter.value);
		posy *= (1.0+m_filter.value);
		
		// Average the new move with stored movement
		for (i = 0; i < M_FILTERLEVEL; i++)
		{
			posx += weight * history[i][0];
			posy += weight * history[i][1];
			total += weight;
			weight *= m_filter.value;
		}

		posx /= total;
		posy /= total;
		
		// Don't average large movements
		if ( posx < -cutoff || posx > cutoff )
			hisx = 0;
		else
			hisx = posx;
			
		if ( posy < -cutoff || posy > cutoff )
			hisy = 0;
		else
			hisy = posy;
   		
   		// Add the new move to the history buffer
		for (i = M_FILTERLEVEL-1; i >= 1; i--)
		{
			history[i][0] = history[i-1][0];
			history[i][1] = history[i-1][1];
		}      

		history[0][0] = hisx;
		history[0][1] = hisy;
		
		filter_dirty = true;
	}
	
	// Deltas on OSX blow bigtime - boost'em depending on the current framerate
//	posx *= host_frametime*72;
//	posy *= host_frametime*72;

// DEBUG - see what we end up with...
//if (temp1.value) Com_Printf ("\3FX: %3.2f FY: %3.2f\n", posx, posy);

	//
	// add mouse X/Y movement to cmd
	//
	
	if ( (in_strafe.state & 1) || (lookstrafe.value && mlook_active ))
		cmd->sidemove += m_side.value * posx;
	else
		cl.viewangles[YAW] -= m_yaw.value * posx;
	
	// If mouselook is on and there's been motion, don't drift
	if (mlook_active)
		V_StopPitchDrift ();
		
	if (mlook_active && !(in_strafe.state & 1))
	{
		cl.viewangles[PITCH] += (m_pitch.value * posy);
		cl.viewangles[PITCH] = bound(MIN_PITCH, cl.viewangles[PITCH], MAX_PITCH);
	}
	else
		cmd->forwardmove -= m_forward.value * posy;
}

/*
============= 
IN_ConnectMouse

connect mouse to InputSprockets
============= 
*/
void IN_ConnectMouse(void)
{
	IN_OverrideSystemPrefs (false);
    IN_HideCursor();
}

/*
=============
IN_DisconnectMouse
 
disconnect mouse from InputSprockets
============= 
*/
void IN_DisconnectMouse(void)
{
	suspend_mouse = true;
	disconneted_mouse = true;
	
	IN_OverrideSystemPrefs (true);
}

//
// hook/unhook mouse to InputSprockets
//
void IN_SuspendMouse(void)
{

#ifdef DISABLE_MOUSE
	return;
#endif
	
	suspend_mouse = true;

	IN_OverrideSystemPrefs (true);
}

void IN_ResumeMouse(void)
{

#ifdef DISABLE_MOUSE
	return;
#endif
	
	suspend_mouse = false;
	
	IN_OverrideSystemPrefs (false);	
}

//
// show or hide mouse pointer
//
void IN_ShowCursor (void)
{
#ifdef DISABLE_MOUSE
	return;
#endif
	InitCursor();
	ShowCursor();
}

void IN_HideCursor (void)
{
#ifdef DISABLE_MOUSE
	return;
#endif
	HideCursor();
}
