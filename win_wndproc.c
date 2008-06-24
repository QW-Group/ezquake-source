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

	$Id: win_wndproc.c,v 1.6 2007-07-20 19:02:06 tonik Exp $

*/

#include <time.h>

#include "quakedef.h"
#ifdef GLQUAKE
#include "gl_model.h"
#include "gl_local.h"
#if defined(_WIN32) || defined(__linux__) || defined(__FreeBSD__)
#include "tr_types.h"
#endif // _WIN32 || __linux__ || __FreeBSD__
#endif
#include "cdaudio.h"
#ifdef WITH_KEYMAP
#include "keymap.h"
#endif // WITH_KEYMAP 
#include "resource.h"
#include "winquake.h"

#include "in_raw.h"


qbool DDActive; // dummy, unused

extern void WG_AppActivate(BOOL fActive, BOOL minimized);

HWND		mainwindow;

/************************ SIGNALS, MESSAGES, INPUT ETC ************************/

void IN_ClearStates (void);

void ClearAllStates (void) 
{
	int i;

	// Send an up event for each key, to make sure the server clears them all.
	for (i = 0; i < sizeof(keydown) / sizeof(*keydown); i++) 
	{
		if (keydown[i])
			Key_Event (i, false);
	}

	Key_ClearStates();
	IN_ClearStates();
}

/******************************************************************************
*
* Function:     AppActivate
* Parameters:   fActive - True if app is activating
*
* Description:  If the application is activating, then swap the system
*               into SYSPAL_NOSTATIC mode so that our palettes will display
*               correctly.
*
******************************************************************************/
void AppActivate(BOOL fActive, BOOL minimize)
{
	static BOOL	sound_active;
	extern cvar_t sys_inactivesound;

	ActiveApp = fActive;
	Minimized = minimize;

	// enable/disable sound on focus gain/loss
	if (!ActiveApp && sound_active && !sys_inactivesound.value) 
	{
		S_BlockSound();
		sound_active = false;
	} 
	else if (ActiveApp && !sound_active) 
	{
		S_UnblockSound();
		sound_active = true;
	}

	if ( fActive )
	{
//		if ( glConfig.isFullscreen /* || ( !glConfig.isFullscreen && _windowed_mouse.value && key_dest == key_game ) */ )
		{
			IN_ActivateMouse();
			IN_HideMouse();
		}
	}
	else
	{
//		if ( glConfig.isFullscreen /* || ( !glConfig.isFullscreen && _windowed_mouse.value ) */ )
		{
			IN_DeactivateMouse();
			IN_ShowMouse();
		}
	}

	// FIXME: where we must put this, before mouse activeate/deactivate or after?
	WG_AppActivate(fActive, minimize);
}

LONG CDAudio_MessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#ifdef WITH_KEYMAP
#else // WITH_KEYMAP
int IN_MapKey (int key);
#endif // WITH_KEYMAP else


#include "mw_hook.h"
void MW_Hook_Message (long buttons);

void IN_RawInput_MouseRead(HANDLE in_device_handle);

// Main window procedure.
LONG WINAPI MainWndProc (HWND    hWnd, UINT    uMsg, WPARAM  wParam, LPARAM  lParam) 
{
	extern void VID_UpdateWindowStatus (void);

    LONG lRet = 1;
	int fActive, fMinimized, temp;
	extern unsigned int uiWheelMessage;

	// VVD: didn't restore gamma after ALT+TAB on some ATI video cards (or drivers?...)
	// HACK!!! FIXME {
	extern int	restore_gamma;
	static time_t time_old;
	static float old_gamma;
	if (restore_gamma == 2) 
	{
		if (time(NULL) - time_old > 0) 
		{
			Cvar_SetValue(&v_gamma, old_gamma);
			restore_gamma = 0;
		}
	}
	// }

	if (uMsg == uiWheelMessage) 
	{
		uMsg = WM_MOUSEWHEEL;
		wParam <<= 16;
	}

    switch (uMsg) 
	{
		#ifdef USINGRAWINPUT
		case WM_INPUT:
		{
			// raw input handling
			IN_RawInput_MouseRead((HANDLE)lParam);
			break;
		}
		#endif // USINGRAWINPUT
		case WM_KILLFOCUS:
            break;
    	case WM_SIZE:
			break;
		case WM_CREATE:
		{
			mainwindow = hWnd;
			break;
		}
   	    case WM_DESTROY:
		{
			// Let sound and input know about this?
			mainwindow = NULL;

			break;
		}
		case WM_MOVE:
		{
			int		xPos, yPos;
			RECT r;
			int		style;

			if ( !r_fullscreen.integer )
			{
				xPos = (short) LOWORD(lParam);    // horizontal position 
				yPos = (short) HIWORD(lParam);    // vertical position 

				r.left   = 0;
				r.top    = 0;
				r.right  = 1;
				r.bottom = 1;

				style = GetWindowLong( hWnd, GWL_STYLE );
				AdjustWindowRect( &r, style, FALSE );

				Cvar_SetValue( &vid_xpos, xPos + r.left );
				Cvar_SetValue( &vid_ypos, yPos + r.top );
				vid_xpos.modified = false;
				vid_ypos.modified = false;
			}

			VID_UpdateWindowStatus();
			
			break;
		}
   	    case WM_CLOSE:
		{
			if (MessageBox (mainwindow, "Are you sure you want to quit?", "ezQuake : Confirm Exit",
						MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES)
			{
				Host_Quit ();
			}

	        break;
		}
		case WM_ACTIVATE:
		{
			fActive = LOWORD(wParam);
			fMinimized = (BOOL) HIWORD(wParam);
			AppActivate(!(fActive == WA_INACTIVE), fMinimized);

			// VVD: didn't restore gamma after ALT+TAB on some ATI video cards (or drivers?...)
			// HACK!!! FIXME {
			if (restore_gamma == 1) 
			{
				time_old = time(NULL);
				restore_gamma = 2;
				old_gamma = v_gamma.value;
				Cvar_SetValue(&v_gamma, old_gamma + 0.01);
			}
			// }
			// fix the leftover Alt from any Alt-Tab or the like that switched us away
			ClearAllStates ();

			break;
		}
		case WM_SYSKEYDOWN:
		{
			if ( wParam == 13 )
			{
				if ( glw_state.vid_canalttab )
				{
					Cvar_LatchedSetValue( &r_fullscreen, !r_fullscreen.integer );
					Cbuf_AddText( "vid_restart\n" );
				}
				return 0;
			}
			// fall through
		}
		case WM_KEYDOWN:
		{
			#ifdef WITH_KEYMAP
			IN_TranslateKeyEvent (lParam, wParam, true);
			#else // WITH_KEYMAP
			Key_Event (IN_MapKey(lParam), true);
			#endif // WITH_KEYMAP else
			break;
		}
		case WM_KEYUP:
		case WM_SYSKEYUP:
		{
			#ifdef WITH_KEYMAP
			IN_TranslateKeyEvent (lParam, wParam, false);
			#else // WITH_KEYMAP
			Key_Event (IN_MapKey(lParam), false);
			#endif // WITH_KEYMAP else
			break;
		}
		case WM_SYSCHAR:
			// keep Alt-Space from happening
			break;
		// This is complicated because Win32 seems to pack multiple mouse events into
		// one update sometimes, so we always check all states and look for events
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
		#ifdef MM_CODE
		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDBLCLK:
		case WM_XBUTTONDBLCLK:
		#endif // MM_CODE
		{
			temp = 0;

			if (wParam & MK_LBUTTON)
				temp |= 1;

			if (wParam & MK_RBUTTON)
				temp |= 2;

			if (wParam & MK_MBUTTON)
				temp |= 4;

			if (wParam & MK_XBUTTON1)
				temp |= 8;

			if (wParam & MK_XBUTTON2)
				temp |= 16;

			IN_MouseEvent (temp);

			break;
		}
		// JACK: This is the mouse wheel with the Intellimouse
		// Its delta is either positive or neg, and we generate the proper Event.
		case WM_MOUSEWHEEL:
		{
			if (in_mwheeltype != MWHEEL_DINPUT)
			{
				in_mwheeltype = MWHEEL_WINDOWMSG;

				if ((short) HIWORD(wParam) > 0) 
				{
					Key_Event(K_MWHEELUP, true);
					Key_Event(K_MWHEELUP, false);
				} 
				else 
				{
					Key_Event(K_MWHEELDOWN, true);
					Key_Event(K_MWHEELDOWN, false);
				}

				// when an application processes the WM_MOUSEWHEEL message, it must return zero
				lRet = 0;
			}
			break;
		}
		case WM_MWHOOK:
		{
			MW_Hook_Message (lParam);
			break;
		}
		case MM_MCINOTIFY:
		{
			lRet = CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);
			break;
		}
		default:
		{
			// Pass all unhandled messages to DefWindowProc.
			lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
			break;
		}
	}

	// return 1 if handled message, 0 if not.
	return lRet;
}



