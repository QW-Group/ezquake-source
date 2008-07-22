//_________________________________________________________________________________________________________________________nFO
// "in_osx.m" - MacOS X mouse driver
//
// Written by:	Axel 'awe' Wefers			[mailto:awe@fruitz-of-dojo.de].
//				©2001-2006 Fruitz Of Dojo 	[http://www.fruitz-of-dojo.de].
//
// Quakeª is copyrighted by id software		[http://www.idsoftware.com].
//
// Version History:
// v1.0.8: F12 eject is now disabled while Quake is running.
// v1.0.6: Mouse sensitivity works now as expected.
// v1.0.5: Reworked whole mouse handling code [required for windowed mouse].
// v1.0.0: Initial release.
//____________________________________________________________________________________________________________________iNCLUDES

#pragma mark =Includes=

#import <AppKit/AppKit.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/hidsystem/IOHIDLib.h>
#import <IOKit/hidsystem/IOHIDParameter.h>
#import <IOKit/hidsystem/event_status_driver.h>

#import "quakedef.h"
#import "keys.h"
#import "input.h"
#import "in_osx.h"
#import "vid_osx.h"

#pragma mark -

//_____________________________________________________________________________________________________________________sTATICS

#pragma mark =Variables=

cvar_t				aux_look = {"auxlook","1", true};
cvar_t				m_filter = {"m_filter", "1"};
// compatibility with old Quake -- setting to 0 disables KP_* codes
cvar_t				cl_keypad = {"cl_keypad","0"};
BOOL				gInMouseEnabled;
UInt8				gInSpecialKey[] = {
										K_UPARROW,    K_DOWNARROW,   K_LEFTARROW,    K_RIGHTARROW,
											 K_F1,           K_F2,          K_F3,            K_F4,
											 K_F5,           K_F6,          K_F7,            K_F8,
											 K_F9,          K_F10,         K_F11,           K_F12,
											K_F13,          K_F14,         K_F15,				0,
												0,				0,				0,				0,
												0,				0,				0,				0,
												0,				0,				0,				0,
												0,				0,				0,				0,
												0,				0,				0,          K_INS,
											K_DEL,		   K_HOME,				0,			K_END,
										   K_PGUP,         K_PGDN,	            0,				0,
										  K_PAUSE,				0,	            0,				0,
												0,				0,	            0,				0,
												0, 	   KP_NUMLOCK,				0,				0,                                                                        					    0, 	  	     0, 	     0, 	     0,
												0,				0,				0,				0,
												0,				0,			K_INS,				0
                                  };
UInt8					gInNumPadKey[] =  {	
												0,				0,	            0,              0,
												0,				0,	            0,				0,
												0,				0,	            0,				0,
												0,				0,	            0,				0,
												0,				0,	            0,				0,
												0,				0,	            0,				0,
												0,				0,	            0,				0,
												0,				0,	            0,				0,
												0,				0,	            0,				0,
												0,				0,	            0,				0,
												0,				0,	            0,				0,
												0,				0,	            0,				0,
												0,				0,	            0,				0,
												0,				0,	            0,				0,
												0,				0,	            0,				0,
												0,				0,	            0,				0,
												0,			K_DEL,	            0,		  KP_STAR,
												0,		  KP_PLUS,	            0,				0,
												0,				0,	            0,				0,
										 KP_ENTER,       KP_SLASH,       KP_MINUS,				0,
												0,       KP_EQUAL,
											 KP_0,           KP_1,           KP_2,           KP_3,
											 KP_4,           KP_5,           KP_6,           KP_7,
											 0,           KP_8,
										     KP_9,				0,	            0,				0
                                 };

static BOOL					gInMouseMoved;
static in_mousepos_t		gInMousePosition,
							gInMouseNewPosition,
							gInMouseOldPosition;
float mouse_x, mouse_y;

#pragma mark -

//_________________________________________________________________________________________________________fUNCTION_pROTOTYPES

#pragma mark =Function Prototypes=

static io_connect_t	IN_GetIOHandle (void);
static void 		IN_SetMouseScalingEnabled (BOOL theState);

#pragma mark -

//__________________________________________________________________________________________________________Toggle_AuxLook_f()

void	Toggle_AuxLook_f (void)
{
    if (aux_look.value)
    {
        Cvar_Set (&aux_look,"0");
    }
    else
    {
        Cvar_Set (&aux_look,"1");
    }
}

//________________________________________________________________________________________________________Force_CenterView_f()

void	Force_CenterView_f (void)
{
    cl.viewangles[PITCH] = 0;
}

//____________________________________________________________________________________________________________IN_GetIOHandle()

io_connect_t IN_GetIOHandle (void)
{
    io_connect_t 	myHandle = MACH_PORT_NULL;
    kern_return_t	myStatus;
    io_service_t	myService = MACH_PORT_NULL;
    mach_port_t		myMasterPort;

    myStatus = IOMasterPort (MACH_PORT_NULL, &myMasterPort );
	
    if (myStatus != KERN_SUCCESS)
    {
        return (0);
    }

    myService = IORegistryEntryFromPath (myMasterPort, kIOServicePlane ":/IOResources/IOHIDSystem");

    if (myService == 0)
    {
        return (0);
    }

    myStatus = IOServiceOpen (myService, mach_task_self (), kIOHIDParamConnectType, &myHandle);
    IOObjectRelease (myService);

    return (myHandle);
}

//_____________________________________________________________________________________________________IN_SetF12EjectEnabled()

void	IN_SetF12EjectEnabled (qbool theState)
{
    static BOOL		myF12KeyIsEnabled = YES;
    static UInt32	myOldValue;
    io_connect_t	myIOHandle = 0;
    
    // Do we have a state change?
    if (theState == myF12KeyIsEnabled)
    {
        return;
    }

    // Get the IOKit handle:
    myIOHandle = IN_GetIOHandle ();
	
    if (myIOHandle == 0)
    {
        return;
    }

    // Set the F12 key according to the current state:
    if (theState == NO && keybindings[K_F12] != NULL && keybindings[K_F12][0] != 0x00)
    {
        UInt32			myValue = 0x00;
        IOByteCount		myCount;
        kern_return_t	myStatus;
        
        myStatus = IOHIDGetParameter (myIOHandle,
                                      CFSTR (kIOHIDF12EjectDelayKey),
                                      sizeof (UInt32),
                                      &myOldValue,
                                      &myCount);

        // change only the settings, if we were successfull!
        if (myStatus != kIOReturnSuccess)
        {
            theState = YES;
        }
        else
        {
            IOHIDSetParameter (myIOHandle, CFSTR (kIOHIDF12EjectDelayKey), &myValue, sizeof (UInt32));
        }
    }
    else
    {
        if (myF12KeyIsEnabled == NO)
        {
            IOHIDSetParameter (myIOHandle, CFSTR (kIOHIDF12EjectDelayKey),  &myOldValue, sizeof (UInt32));
        }
        theState = YES;
    }
    
    myF12KeyIsEnabled = theState;
    IOServiceClose (myIOHandle);
}

//_________________________________________________________________________________________________IN_SetMouseScalingEnabled()

void	IN_SetMouseScalingEnabled (BOOL theState)
{
    static BOOL		myMouseScalingEnabled	= YES;
    static double	myOldAcceleration		= 0.0;
    io_connect_t	myIOHandle				= 0;

    // Do we have a state change?
    if (theState == myMouseScalingEnabled)
    {
        return;
    }
    
    // Get the IOKit handle:
    myIOHandle = IN_GetIOHandle ();
	
    if (myIOHandle == 0)
    {
        return;
    }

    // Set the mouse acceleration according to the current state:
    if (theState == YES)
    {
        IOHIDSetAccelerationWithKey (myIOHandle,
                                     CFSTR (kIOHIDMouseAccelerationType),
                                     myOldAcceleration);
    }
    else
    {
        kern_return_t	myStatus;

        myStatus = IOHIDGetAccelerationWithKey (myIOHandle,
                                                CFSTR (kIOHIDMouseAccelerationType),
                                                &myOldAcceleration);

        // change only the settings, if we were successfull!
        if (myStatus != kIOReturnSuccess || myOldAcceleration == 0.0)
        {
            theState = YES;
        }
         
        // finally change the acceleration:
        if (theState == NO)
        {
            IOHIDSetAccelerationWithKey (myIOHandle,  CFSTR (kIOHIDMouseAccelerationType), -1.0);
        }
    }
    
    myMouseScalingEnabled = theState;
    IOServiceClose (myIOHandle);
}

//_____________________________________________________________________________________________________________IN_ShowCursor()

void	IN_ShowCursor (BOOL theState)
{
    static BOOL		myCursorIsVisible = YES;

    // change only if we got a state change:
    if (theState != myCursorIsVisible)
    {
        if (theState == YES)
        {
            CGAssociateMouseAndMouseCursorPosition (YES);
            IN_SetMouseScalingEnabled (YES);
            IN_CenterCursor ();
            CGDisplayShowCursor (kCGDirectMainDisplay);
        }
        else
        {
            [NSApp activateIgnoringOtherApps: YES];
            CGDisplayHideCursor (kCGDirectMainDisplay);
            CGAssociateMouseAndMouseCursorPosition (NO);
            IN_CenterCursor ();
            IN_SetMouseScalingEnabled (NO);
        }
        myCursorIsVisible = theState;
    }
}

//___________________________________________________________________________________________________________IN_CenterCursor()

void	IN_CenterCursor (void)
{
    CGPoint		myCenter;

    if (gVidDisplayFullscreen == NO)
    {
        float		myCenterX = gVidWindowPosX, myCenterY = -gVidWindowPosY;

        // calculate the window center:
        myCenterX += (float) (vid.width >> 1);
        myCenterY += (float) CGDisplayPixelsHigh (kCGDirectMainDisplay) - (float) (vid.height >> 1);
        
        myCenter = CGPointMake (myCenterX, myCenterY);
    }
    else
    {
        // just center at the middle of the screen:
        myCenter = CGPointMake ((float) (vid.width >> 1), (float) (vid.height >> 1));
    }

    // and go:
    CGDisplayMoveCursorToPoint (kCGDirectMainDisplay, myCenter);
}

//______________________________________________________________________________________________________________IN_InitMouse()

void	IN_InitMouse (void)
{
    // check for command line:
    if (COM_CheckParm ("-nomouse"))
    {
        gInMouseEnabled = NO;
        return;
    }
    else
    {
        gInMouseEnabled = YES;
    }
    
    gInMouseMoved = NO;
}

//___________________________________________________________________________________________________________________IN_Init()

void 	IN_Init (void)
{
    // register variables:
    Cvar_Register (&m_filter);
	Cvar_Register (&cl_keypad);
    Cvar_Register (&aux_look);
    
    // register console commands:
    Cmd_AddCommand ("toggle_auxlook", Toggle_AuxLook_f);
    Cmd_AddCommand ("force_centerview", Force_CenterView_f);
    
    // init the mouse:
    IN_InitMouse ();
    
    IN_SetMouseScalingEnabled (NO);
    
    // enable mouse look by default:
    Cbuf_AddText ("+mlook\n");
}

//_______________________________________________________________________________________________________________IN_Shutdown()

void 	IN_Shutdown (void)
{
    IN_SetMouseScalingEnabled (YES);
}

//_______________________________________________________________________________________________________________IN_Commands()

void 	IN_Commands (void)
{
    // avoid popping of the app back to the front:
    if ([NSApp isHidden] == YES)
    {
        return;
    }
    
    // set the cursor visibility by respecting the display mode:
    if (gVidDisplayFullscreen == 1.0f)
    {
        IN_ShowCursor (NO);
    }
    else
    {
        // is the mouse in windowed mode?
        if (gInMouseEnabled == YES && [NSApp isActive] == YES &&
            gVidIsMinimized == NO && _windowed_mouse.value != 0.0f)
        {
            IN_ShowCursor (NO);
        }
        else
        {
            IN_ShowCursor (YES);
        }
    }
}

//_______________________________________________________________________________________________________IN_ReceiveMouseMove()

void	IN_ReceiveMouseMove (CGMouseDelta theDeltaX, CGMouseDelta theDeltaY)
{
    gInMouseNewPosition.X += theDeltaX;
    gInMouseNewPosition.Y += theDeltaY;
}

//______________________________________________________________________________________________________________IN_MouseMove()

void	IN_MouseMove (usercmd_t *cmd)
{
    CGMouseDelta	myMouseX = gInMouseNewPosition.X,
                        myMouseY = gInMouseNewPosition.Y;

    if ((gVidDisplayFullscreen == NO && _windowed_mouse.value == 0.0f) ||
        gInMouseEnabled == NO || gVidIsMinimized == YES || [NSApp isActive] == NO)
    {
        return;
    }

    gInMouseNewPosition.X = 0;
    gInMouseNewPosition.Y = 0;

    if (m_filter.value != 0.0f)
    {
        gInMousePosition.X = (myMouseX + gInMouseOldPosition.X) >> 1;
        gInMousePosition.Y = (myMouseY + gInMouseOldPosition.Y) >> 1;
    }
    else
    {
        gInMousePosition.X = myMouseX;
        gInMousePosition.Y = myMouseY;
    }

    gInMouseOldPosition.X = myMouseX;
    gInMouseOldPosition.Y = myMouseY;


    if(key_dest == key_hudeditor || key_dest == key_menu)
    {
        mouse_x = myMouseX;
        mouse_y = myMouseY;
    }
	else
	{
		gInMousePosition.X *= sensitivity.value;
		gInMousePosition.Y *= sensitivity.value;

		// lookstrafe or view?
		if ((in_strafe.state & 1) || (lookstrafe.value && (in_mlook.state & 1)))
		{
			cmd->sidemove += m_side.value * gInMousePosition.X;
		}
		else
		{
			cl.viewangles[YAW] -= m_yaw.value * gInMousePosition.X;
		}
					
		if (in_mlook.state & 1)
		{
			V_StopPitchDrift ();
		}
				
		if ((in_mlook.state & 1) && !(in_strafe.state & 1))
		{
			cl.viewangles[PITCH] += m_pitch.value * gInMousePosition.Y;
			
			if (cl.viewangles[PITCH] > 80)
			{
				cl.viewangles[PITCH] = 80;
			}
			
			if (cl.viewangles[PITCH] < -70)
			{
				cl.viewangles[PITCH] = -70;
			}
		}
		else
		{
			if (in_strafe.state & 1)
			{
				cmd->upmove -= m_forward.value * gInMousePosition.Y;
			}
			else
			{
				cmd->forwardmove -= m_forward.value * gInMousePosition.Y;
			}
		}

		// force the mouse to the center, so there's room to move:
		if (myMouseX != 0 || myMouseY != 0)
		{
			IN_CenterCursor ();
		}
	}
}

//___________________________________________________________________________________________________________________IN_Move()

void IN_Move (usercmd_t *cmd)
{
    IN_MouseMove (cmd);
}

//_________________________________________________________________________________________________________________________eOF
