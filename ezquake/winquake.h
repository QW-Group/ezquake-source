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
// winquake.h: Win32-specific Quake header file

#ifdef _WIN32 

#include <windows.h>

#ifndef SERVERONLY

#define WM_MOUSEWHEEL		0x020A
#define WM_XBUTTONDOWN		0x020B
#define WM_XBUTTONUP		0x020C

#define MK_XBUTTON1         0x0020
#define MK_XBUTTON2         0x0040

#ifndef WITHOUT_WINKEYHOOK

#define LLKHF_UP			(KF_UP >> 8)
#define KF_UP				0x8000

typedef struct {
    DWORD   vkCode;
    DWORD   scanCode;
    DWORD   flags;
    DWORD   time;
    ULONG_PTR dwExtraInfo;
} *PKBDLLHOOKSTRUCT;

#endif

#include <ddraw.h>
#include <dsound.h>

extern	HINSTANCE	global_hInstance;

extern qboolean		DDActive;

extern LPDIRECTSOUND		pDS;
extern LPDIRECTSOUNDBUFFER	pDSBuf;
extern DWORD				gSndBufSize;

typedef enum {MS_WINDOWED, MS_FULLSCREEN, MS_FULLDIB, MS_UNINIT} modestate_t;

extern modestate_t	modestate;

extern HWND			mainwindow;
extern qboolean		ActiveApp, Minimized;

extern qboolean	WinNT;

void IN_ShowMouse (void);
void IN_DeactivateMouse (void);
void IN_HideMouse (void);
void IN_ActivateMouse (void);
void IN_RestoreOriginalMouseState (void);
void IN_SetQuakeMouseState (void);
void IN_MouseEvent (int mstate);

extern int		window_center_x, window_center_y;
extern RECT		window_rect;

extern HANDLE	hinput, houtput;

void IN_UpdateClipCursor (void);

void S_BlockSound (void);
void S_UnblockSound (void);

#endif // !SERVERONLY

#endif // _WIN32
