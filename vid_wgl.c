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

	$Id: vid_wgl.c,v 1.41 2007-10-04 13:48:10 dkure Exp $

*/

#include "quakedef.h"
#include "cdaudio.h"
#ifdef WITH_KEYMAP
#include "keymap.h"
#endif // WITH_KEYMAP 
#include "resource.h"
#include "winquake.h"


#define MAX_MODE_LIST	128
#define MAXWIDTH		10000
#define MAXHEIGHT		10000
#define BASEWIDTH		320
#define BASEHEIGHT		200

#define MODE_WINDOWED			0
#define NO_MODE					(MODE_WINDOWED - 1)
#define MODE_FULLSCREEN_DEFAULT	(MODE_WINDOWED + 1)

typedef struct {
	modestate_t	type;
	int			width;
	int			height;
	int			modenum;
	int			dib;
	int			fullscreen;
	int			bpp;
	int			halfscreen;
	char		modedesc[17];
} vmode_t;

typedef struct {
	int			width;
	int			height;
} lmode_t;

lmode_t	lowresmodes[] = {
	{320, 200},
	{320, 240},
	{400, 300},
	{512, 384},
};

qbool DDActive;

static vmode_t	modelist[MAX_MODE_LIST];
static int		nummodes = 0;
static vmode_t	*pcurrentmode;
static vmode_t	badmode;

static DEVMODE	gdevmode;
static qbool	vid_initialized = false;
static qbool	windowed, leavecurrentmode;
static qbool vid_canalttab = false;
static qbool vid_wassuspended = false;
static int		windowed_mouse;
extern qbool	mouseactive;  // from in_win.c
static HICON	hIcon;

DWORD		WindowStyle, ExWindowStyle;

HWND		mainwindow, dibwindow = NULL;

int				vid_modenum = NO_MODE;
int				vid_default = MODE_WINDOWED;
static int		windowed_default;
unsigned char	vid_curpal[256*3];

HGLRC	baseRC = NULL;
HDC		maindc;

glvert_t glv;

HWND WINAPI InitializeWindow (HINSTANCE hInstance, int nCmdShow);

modestate_t	modestate = MS_UNINIT;

unsigned short *currentgammaramp = NULL;
static unsigned short systemgammaramp[3][256];

qbool vid_3dfxgamma = false;
qbool vid_gammaworks = false;
qbool vid_hwgamma_enabled = false;
qbool customgamma = false;

void VID_MenuDraw (void);
void VID_MenuKey (int key);

LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void AppActivate(BOOL fActive, BOOL minimize);
char *VID_GetModeDescription (int mode);
void ClearAllStates (void);
void VID_UpdateWindowStatus (void);

qbool OnChange_vid_displayfrequency(cvar_t *var, char *string);
qbool OnChange_vid_con_xxx(cvar_t *var, char *string);
qbool OnChange_vid_mode(cvar_t *var, char *string);

/*********************************** CVARS ***********************************/

//cvar_t		_vid_default_mode = {"_vid_default_mode","0",CVAR_ARCHIVE};	// Note that 3 is MODE_FULLSCREEN_DEFAULT
//cvar_t		_vid_default_mode_win = {"_vid_default_mode_win","3",CVAR_ARCHIVE};
//cvar_t		vid_config_x = {"vid_config_x","800",CVAR_ARCHIVE};
//cvar_t		vid_config_y = {"vid_config_y","600",CVAR_ARCHIVE};

cvar_t		vid_ref				 = {"vid_ref",  "gl", CVAR_ROM};
cvar_t		vid_mode			 = {"vid_mode", "-1", CVAR_NO_RESET, OnChange_vid_mode};	// Note that -1 is NO_MODE
cvar_t		vid_displayfrequency = {"vid_displayfrequency", "75", CVAR_NO_RESET, OnChange_vid_displayfrequency};
cvar_t		vid_hwgammacontrol	 = {"vid_hwgammacontrol",   "1"};
cvar_t      vid_flashonactivity  = {"vid_flashonactivity",  "1", CVAR_ARCHIVE};

cvar_t      vid_conwidth		 = {"vid_conwidth",  "640", CVAR_NO_RESET, OnChange_vid_con_xxx};
cvar_t      vid_conheight		 = {"vid_conheight",   "0", CVAR_NO_RESET, OnChange_vid_con_xxx}; // default is 0, so i can sort out is user specify conheight on cmd line or something

cvar_t		_windowed_mouse		 = {"_windowed_mouse", "1", CVAR_ARCHIVE};

// VVD: din't restore gamma after ALT+TAB on some ATI video cards (or drivers?...) 
// HACK!!! FIXME {
cvar_t		vid_forcerestoregamma = {"vid_forcerestoregamma", "0"};
int restore_gamma = 0;
// }
qbool allow_flash = false;

typedef BOOL (APIENTRY *SWAPINTERVALFUNCPTR)(int);
SWAPINTERVALFUNCPTR wglSwapIntervalEXT = NULL;
qbool OnChange_vid_vsync(cvar_t *var, char *string);
static qbool update_vsync = true;
cvar_t	vid_vsync = {"vid_vsync", "0", 0, OnChange_vid_vsync};

BOOL (APIENTRY *wglGetDeviceGammaRamp3DFX)(HDC hDC, GLvoid *ramp);
BOOL (APIENTRY *wglSetDeviceGammaRamp3DFX)(HDC hDC, GLvoid *ramp);


int		window_center_x, window_center_y, window_x, window_y, window_width, window_height;
RECT	window_rect;

/******************************* GL EXTENSIONS *******************************/


void GL_WGL_CheckExtensions(void) {
    if (!COM_CheckParm("-noswapctrl") && CheckExtension("WGL_EXT_swap_control")) {
		if ((wglSwapIntervalEXT = (void *) wglGetProcAddress("wglSwapIntervalEXT"))) {
            Com_Printf_State(PRINT_OK, "Vsync control extensions found\n");
			Cvar_SetCurrentGroup(CVAR_GROUP_VIDEO);
			Cvar_Register (&vid_vsync);
			Cvar_ResetCurrentGroup();
		}
    }

	if (!COM_CheckParm("-no3dfxgamma") && CheckExtension("WGL_3DFX_gamma_control")) {
		wglGetDeviceGammaRamp3DFX = (void *) wglGetProcAddress("wglGetDeviceGammaRamp3DFX");
		wglSetDeviceGammaRamp3DFX = (void *) wglGetProcAddress("wglSetDeviceGammaRamp3DFX");
		vid_3dfxgamma = (wglGetDeviceGammaRamp3DFX && wglSetDeviceGammaRamp3DFX);
	}
}

qbool OnChange_vid_vsync(cvar_t *var, char *string) {
	update_vsync = true;
	return false;
}


void GL_Init_Win(void) {
	GL_WGL_CheckExtensions();
}

/******************************** WINDOW STUFF ********************************/

void VID_SetCaption (char *text) {
	if (vid_initialized) {
		SetWindowText(mainwindow, text);
		UpdateWindow(mainwindow);
	}
}

void CenterWindow(HWND hWndCenter, int width, int height, BOOL lefttopjustify) {
	int CenterX, CenterY;

	CenterX = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
	CenterY = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
	if (CenterX > CenterY * 2)
		CenterX >>= 1;	// dual screens
	CenterX = (CenterX < 0) ? 0: CenterX;
	CenterY = (CenterY < 0) ? 0: CenterY;
	SetWindowPos (hWndCenter, NULL, CenterX, CenterY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);
}

void VID_UpdateWindowStatus (void) {
	window_rect.left = window_x;
	window_rect.top = window_y;
	window_rect.right = window_x + window_width;
	window_rect.bottom = window_y + window_height;
	window_center_x = (window_rect.left + window_rect.right) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	IN_UpdateClipCursor ();
}

/******************************** VID_SETMODE ********************************/

int GetBestFreq (int w, int h, int bpp) {
	int freq = 0;
	DEVMODE	testMode;

	memset((void*) &testMode, 0, sizeof(testMode));
	testMode.dmSize = sizeof(testMode);

	testMode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
	testMode.dmPelsWidth  = w; // here we must pass right value if modelist[vid_modenum].halfscreen
	testMode.dmPelsHeight = h;
	testMode.dmBitsPerPel = bpp;

	for (freq = 301; freq >= 0; freq--)
	{
		testMode.dmDisplayFrequency = freq;
		if (ChangeDisplaySettings (&testMode, CDS_FULLSCREEN | CDS_TEST ) != DISP_CHANGE_SUCCESSFUL)
			continue; // mode can't be set

		break; // wow, we found something
	}

	return max(0, freq);
}

void VID_ShowFreq_f(void) {
	int freq, cnt = 0;
	DEVMODE	testMode;

	if (!vid_initialized || vid_modenum < 0 || vid_modenum >= MAX_MODE_LIST)
		return;

	memset((void*) &testMode, 0, sizeof(testMode));
	testMode.dmSize = sizeof(testMode);

	Com_Printf("Possible display frequency:");

	testMode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
	testMode.dmPelsWidth  = modelist[vid_modenum].width << modelist[vid_modenum].halfscreen;
	testMode.dmPelsHeight = modelist[vid_modenum].height;
	testMode.dmBitsPerPel = modelist[vid_modenum].bpp;

	for (freq = 1; freq < 301; freq++)
	{
		testMode.dmDisplayFrequency = freq;
		if (ChangeDisplaySettings (&testMode, CDS_FULLSCREEN | CDS_TEST ) != DISP_CHANGE_SUCCESSFUL)
			continue; // mode can't be set

		Com_Printf(" %d", freq);
		cnt++;
	}

	Com_Printf("%s\n", cnt ? "" : " none");
}

int GetCurrentFreq(void) {
	DEVMODE	testMode;

	memset((void*) &testMode, 0, sizeof(testMode));
	testMode.dmSize = sizeof(testMode);
	return EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &testMode) ? testMode.dmDisplayFrequency : 0;
}

qbool ChangeFreq(int freq) {
	DWORD oldFields = gdevmode.dmFields, oldFreq = gdevmode.dmDisplayFrequency; // so we can return old values if we failed

	if (!vid_initialized || !host_initialized)
		return true; // hm, -freq xxx or +set vid_displayfrequency xxx cmdline params? allow then

	if (!ActiveApp || Minimized || !vid_canalttab || vid_wassuspended) {
		Com_Printf("Can't switch display frequency while minimized\n");
		return false;
	}

	if (modestate != MS_FULLDIB) {
		Com_Printf("Can't switch display frequency in non full screen mode\n");
		return false;
	}

	if (GetCurrentFreq() == freq) {
		Com_Printf("Display frequency %d alredy set\n", freq);
		return false;
	}

	gdevmode.dmDisplayFrequency = freq;
	gdevmode.dmFields |= DM_DISPLAYFREQUENCY;

	if (   ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN | CDS_TEST ) != DISP_CHANGE_SUCCESSFUL
		|| ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL
	   ) {
		Com_Printf("Can't switch display frequency to %d\n", freq);
		gdevmode.dmDisplayFrequency = oldFreq;
		gdevmode.dmFields = oldFields;
		return false;
	}

	Com_Printf("Switching display frequency to %d\n", freq);

	return true;
}

qbool OnChange_vid_displayfrequency (cvar_t *var, char *string) {
	return !ChangeFreq(Q_atoi(string));
}

qbool OnChange_vid_con_xxx (cvar_t *var, char *string) {

// this is safe but do not allow set this variables from cmd line
//	if (!vid_initialized || !host_initialized || vid_modenum < 0 || vid_modenum >= nummodes)
//		return true;

	if (var == &vid_conwidth) {
		int width = Q_atoi(string);

		width = max(320, width);
		width &= 0xfff8; // make it a multiple of eight

		if (vid_modenum >= 0 && vid_modenum < nummodes)
			vid.width = vid.conwidth = width = min(modelist[vid_modenum].width, width);
		else
			vid.conwidth = width; // issued from cmd line ? then do not set vid.width because code may relay what it 0

		Cvar_SetValue(var, (float)width);

		Draw_AdjustConback ();

		vid.recalc_refdef = 1;

		return true;
	}

	if (var == &vid_conheight) {
		int height = Q_atoi(string);

		height = max(200, height);
//		height &= 0xfff8; // make it a multiple of eight

		if (vid_modenum >= 0 && vid_modenum < nummodes)
			vid.height = vid.conheight = height = min(modelist[vid_modenum].height, height);
		else
			vid.conheight = height; // issued from cmd line ? then do not set vid.height because code may relay what it 0

		Cvar_SetValue(var, (float)height);

		Draw_AdjustConback ();

		vid.recalc_refdef = 1;

		return true;
	}

	return true;
}


qbool VID_SetWindowedMode (int modenum) {
	HDC hdc;
	int lastmodestate, width, height;
	RECT rect;

	lastmodestate = modestate;

	rect.top = rect.left = 0;
	rect.right  = modelist[modenum].width;
	rect.bottom = modelist[modenum].height;

	window_width  = modelist[modenum].width;
	window_height = modelist[modenum].height;

	WindowStyle = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	ExWindowStyle = 0;

	AdjustWindowRectEx(&rect, WindowStyle, FALSE, 0);

	width  = rect.right - rect.left;
	height = rect.bottom - rect.top;

	// Create the DIB window
	if (!dibwindow) // create window, if not exist yet
		dibwindow = CreateWindowEx (
			 ExWindowStyle,
			 "ezQuake",
			 "ezQuake",
			 WindowStyle,
			 rect.left, rect.top,
			 width,
			 height,
			 NULL,
			 NULL,
			 global_hInstance,
			 NULL);
	else // just update size
		SetWindowPos (dibwindow, NULL, 0, 0, width, height, 0);

	if (!dibwindow)
		Sys_Error ("Couldn't create DIB window");

	// Center and show the DIB window
	CenterWindow(dibwindow, modelist[modenum].width, modelist[modenum].height, false);

	ShowWindow (dibwindow, SW_SHOWDEFAULT);
	UpdateWindow (dibwindow);

	modestate = MS_WINDOWED;

	// Because we have set the background brush for the window to NULL (to avoid flickering when re-sizing the window on the desktop),
	// we clear the window to black when created, otherwise it will be  empty while Quake starts up.
	hdc = GetDC(dibwindow);
	PatBlt(hdc, 0, 0, modelist[modenum].width, modelist[modenum].height, BLACKNESS);
	ReleaseDC(dibwindow, hdc);

	if (vid.conheight > modelist[modenum].height)
		vid.conheight = modelist[modenum].height;
	if (vid.conwidth > modelist[modenum].width)
		vid.conwidth = modelist[modenum].width;
	vid.width = vid.conwidth;
	vid.height = vid.conheight;

	vid.numpages = 2;

	mainwindow = dibwindow;

	SendMessage (mainwindow, WM_SETICON, (WPARAM) TRUE, (LPARAM) hIcon);
	SendMessage (mainwindow, WM_SETICON, (WPARAM) FALSE, (LPARAM) hIcon);

	return true;
}

qbool VID_SetFullDIBMode (int modenum) {
	HDC hdc;
	int lastmodestate, width, height;
	RECT rect;

	if (!leavecurrentmode) {
		gdevmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
		gdevmode.dmBitsPerPel = modelist[modenum].bpp;
		gdevmode.dmPelsWidth = modelist[modenum].width << modelist[modenum].halfscreen;
		gdevmode.dmPelsHeight = modelist[modenum].height;
		gdevmode.dmSize = sizeof (gdevmode);

		if (vid_displayfrequency.value) // freq was somehow specified, use it
			gdevmode.dmDisplayFrequency = vid_displayfrequency.value;
		else // guess best possible freq
			gdevmode.dmDisplayFrequency = GetBestFreq (gdevmode.dmPelsWidth, gdevmode.dmPelsHeight, gdevmode.dmBitsPerPel);
		gdevmode.dmFields |= DM_DISPLAYFREQUENCY;
		
		if (ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL) {
			gdevmode.dmFields &= ~DM_DISPLAYFREQUENCY; // okay trying default refresh rate (60hz?) then
			if (ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
				Sys_Error ("Couldn't set fullscreen DIB mode"); // failed for default refresh rate too, bad luck :E
		}

		gdevmode.dmDisplayFrequency = GetCurrentFreq();
		Cvar_SetValue(&vid_displayfrequency, (float)(int)gdevmode.dmDisplayFrequency); // so variable will we set to actual value (some times this fail, but does't cause any damage)
	}

	lastmodestate = modestate;
	modestate = MS_FULLDIB;

	rect.top = rect.left = 0;
	rect.right  = modelist[modenum].width;
	rect.bottom = modelist[modenum].height;

	window_width  = modelist[modenum].width;
	window_height = modelist[modenum].height;

	WindowStyle = WS_POPUP;
	ExWindowStyle = 0;

	AdjustWindowRectEx(&rect, WindowStyle, FALSE, 0);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	// Create the DIB window
	if (!dibwindow) // create window, if not exist yet
		dibwindow = CreateWindowEx (
		 	ExWindowStyle,
		 	"ezQuake",
		 	"ezQuake",
		 	WindowStyle,
		 	rect.left, rect.top,
		 	width,
		 	height,
		 	NULL,
		 	NULL,
		 	global_hInstance,
		 	NULL);
	else // just update size
		SetWindowPos (dibwindow, NULL, 0, 0, width, height, 0);

	if (!dibwindow)
		Sys_Error ("Couldn't create DIB window");

	ShowWindow (dibwindow, SW_SHOWDEFAULT);
	UpdateWindow (dibwindow);

	// Because we have set the background brush for the window to NULL (to avoid flickering when re-sizing the window on the desktop),
	// we clear the window to black when created, otherwise it will be  empty while Quake starts up.
	hdc = GetDC(dibwindow);
	PatBlt(hdc, 0, 0, modelist[modenum].width, modelist[modenum].height, BLACKNESS);
	ReleaseDC(dibwindow, hdc);

	if (vid.conheight > modelist[modenum].height)
		vid.conheight = modelist[modenum].height;
	if (vid.conwidth > modelist[modenum].width)
		vid.conwidth = modelist[modenum].width;
	vid.width = vid.conwidth;
	vid.height = vid.conheight;

	vid.numpages = 2;

	// needed because we're not getting WM_MOVE messages fullscreen on NT
	window_x = window_y = 0;

	mainwindow = dibwindow;

	SendMessage (mainwindow, WM_SETICON, (WPARAM) TRUE, (LPARAM) hIcon);
	SendMessage (mainwindow, WM_SETICON, (WPARAM) FALSE, (LPARAM) hIcon);

	return true;
}

int VID_SetMode (int modenum, unsigned char *palette) {
	int temp;
	qbool stat;
//    MSG msg;

//	if ((windowed && modenum) || (!windowed && modenum < 1) || (!windowed && modenum >= nummodes))
	if (modenum < 0 || modenum >= nummodes)
		Sys_Error ("Bad video mode");

	// so Com_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	CDAudio_Pause();

	// Set either the fullscreen or windowed mode
	if (modelist[modenum].type == MS_WINDOWED) {
		if (_windowed_mouse.value && key_dest == key_game) {
			stat = VID_SetWindowedMode(modenum);
			IN_ActivateMouse ();
			IN_HideMouse ();
		} else {
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			stat = VID_SetWindowedMode(modenum);
		}
	} else if (modelist[modenum].type == MS_FULLDIB) {
		stat = VID_SetFullDIBMode(modenum);
		IN_ActivateMouse ();
		IN_HideMouse ();
	} else {
		Sys_Error ("VID_SetMode: Bad mode type in modelist");
	}

	VID_UpdateWindowStatus ();

	CDAudio_Resume ();
	scr_disabled_for_loading = temp;

	if (!stat)
		Sys_Error ("Couldn't set video mode");

	// now we try to make sure we get the focus on the mode switch, because sometimes in some systems we don't.
	// We grab the foreground, then finish setting up, pump all our messages, and sleep for a little while
	// to let messages finish bouncing around the system, then we put ourselves at the top of the z order,
	// then grab the foreground again. Who knows if it helps, but it probably doesn't hurt
	SetForegroundWindow (mainwindow);
	//	VID_SetPalette (palette);

	vid_modenum = modenum;

	Cvar_SetValue (&vid_mode, (float) vid_modenum);

// { after vid_modenum set we can safe do this
	Cvar_SetValue (&vid_conwidth,  (float) vid.conwidth);
	Cvar_SetValue (&vid_conheight, (float) vid.conheight);
	Draw_AdjustConback (); // need this even vid_conwidth have callback which leads to call this
// }

/*
	while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE)) {
      	TranslateMessage (&msg);
      	DispatchMessage (&msg);
	}

	Sleep (100);
*/

	SetWindowPos (mainwindow, HWND_TOP, 0, 0, 0, 0, SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOCOPYBITS);

	SetForegroundWindow (mainwindow);

	//fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates ();

	Com_Printf_State (PRINT_OK, "Video mode %s initialized\n", VID_GetModeDescription (vid_modenum));

	//VID_SetPalette (palette);

	vid.recalc_refdef = 1;

	return true;
}

qbool OnChange_vid_mode(cvar_t *var, char *string) {
	int modenum;

	if (!vid_initialized || !host_initialized)
		return false; // set from cmd line or from VID_Init(), allow change but do not issue callback

	if (leavecurrentmode) {
		Com_Printf ("Can't switch vid mode when using -current cmd line parammeter\n");
		return true;
	}

	if (!ActiveApp || Minimized || !vid_canalttab || vid_wassuspended) {
		Com_Printf("Can't switch vid mode while minimized\n");
		return true;
	}

	modenum = Q_atoi(string);

	if (host_initialized) { // exec or cfg_load or may be from console, prevent set same mode again, no hurt but less annoying
		if (modenum == vid_mode.value) {
			Com_Printf ("Vid mode %d alredy set\n", modenum);
			return true;
		}
	}

	if (modenum < 0 || modenum >= nummodes
		|| ( windowed && modelist[modenum].type != MS_WINDOWED)
		|| (!windowed && modelist[modenum].type != MS_FULLDIB)
	   ) {
		Com_Printf ("Invalid vid mode %d\n", modenum);
		return true;
	}

	// we call few Cvar_SetValue in VID_SetMode and deeper finctions but they callbacks will be not triggered
	VID_SetMode(modenum, host_basepal);

	return true;
}

int bChosePixelFormat(HDC hDC, PIXELFORMATDESCRIPTOR *pfd, PIXELFORMATDESCRIPTOR *retpfd) {
	int pixelformat;

	if (!(pixelformat = ChoosePixelFormat(hDC, pfd))) {
		MessageBox(NULL, "ChoosePixelFormat failed", "Error", MB_OK);
		return 0;
	}

	if (!(DescribePixelFormat(hDC, pixelformat, sizeof(PIXELFORMATDESCRIPTOR), retpfd))) {
		MessageBox(NULL, "DescribePixelFormat failed", "Error", MB_OK);
		return 0;
	}

	return pixelformat;
}

BOOL bSetupPixelFormat(HDC hDC) {
	int pixelformat;
	PIXELFORMATDESCRIPTOR retpfd, pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
		1,								// version number
		PFD_DRAW_TO_WINDOW 				// support window
		|  PFD_SUPPORT_OPENGL 			// support OpenGL
		|  PFD_DOUBLEBUFFER ,			// double buffered
		PFD_TYPE_RGBA,					// RGBA type
		24,								// 24-bit color depth
		0, 0, 0, 0, 0, 0,				// color bits ignored
		0,								// no alpha buffer
		0,								// shift bit ignored
		0,								// no accumulation buffer
		0, 0, 0, 0, 					// accum bits ignored
		32,								// 32-bit z-buffer
		0,								// no stencil buffer
		0,								// no auxiliary buffer
		PFD_MAIN_PLANE,					// main layer
		0,								// reserved
		0, 0, 0							// layer masks ignored
	};

	if (!(pixelformat = bChosePixelFormat(hDC, &pfd, &retpfd)))
		return FALSE;

	if (retpfd.cDepthBits < 24) {


		pfd.cDepthBits = 24;
		if (!(pixelformat = bChosePixelFormat(hDC, &pfd, &retpfd)))
			return FALSE;
	}

	if (!SetPixelFormat(hDC, pixelformat, &retpfd)) {
		MessageBox(NULL, "SetPixelFormat failed", "Error", MB_OK);
		return FALSE;
	}

	if (retpfd.cDepthBits < 24)
		gl_allow_ztrick = false;

	return TRUE;
}

/********************************** PBUFFER ***********************************/

#ifdef PBUFFER
#define PBUFFER_MAX_ATTRIBS		256
#define PBUFFER_MAX_PFORMATS	256

/*
typedef struct pbuffer_s
{
    HPBUFFERARB  hpbuffer;      // Handle to a pbuffer.
    HDC          hdc;           // Handle to a device context.
    HGLRC        hglrc;         // Handle to a GL rendering context.
    int          width;         // Width of the pbuffer
    int          height;        // Height of the pbuffer
} pbuffer_s;
*/

/*
typedef void (APIENTRY *lpMTexFUNC) (GLenum, GLfloat, GLfloat);
typedef void (APIENTRY *lpSelTexFUNC) (GLenum);
*/

/*
typedef void (APIENTRY *lpwglCreatePbufferARBFUNC) (HDC hDC, int iPixelFormat, int iWidth, int iHeight, const int *piAttribList);
typedef void (APIENTRY *lpReleasePbufferDCARBFUNC) (HPBUFFERARB hPbuffer, HDC hDC);
*/
extern PFNWGLCREATEPBUFFERARBPROC qglCreatePBuffer;
extern PFNWGLRELEASEPBUFFERDCARBPROC qglReleasePBuffer;


/*
HPBUFFERARB wglCreatePbufferARB(HDC hDC,
				    int iPixelFormat,
				    int iWidth,
				    int iHeight,
				    const int *piAttribList);

    HDC wglGetPbufferDCARB(HPBUFFERARB hPbuffer);

    int wglReleasePbufferDCARB(HPBUFFERARB hPbuffer,
			       HDC hDC);

    BOOL wglDestroyPbufferARB(HPBUFFERARB hPbuffer);

    BOOL wglQueryPbufferARB(HPBUFFERARB hPbuffer,
			    int iAttribute,
			    int *piValue);*/

void *GL_GetProcAddress (const char *ExtName);

void VID_GetPBufferProcAddresses()
{
	/*
	wglReleasePbufferDCARB
	wglDestroyPbufferARB
	wglChoosePixelFormatARB
	wglGetPbufferDCARB 
	wglCreatePbufferARB
	wglReleaseTexImageARB
	*/
	qglCreatePBuffer = GL_GetProcAddress("wglCreatePbufferARB");
	qglReleasePBuffer = GL_GetProcAddress("wglReleasePbufferDCARB");
	GL_GetProcAddress("wglDestroyPbufferARB");
	GL_GetProcAddress("wglChoosePixelFormatARB");
	GL_GetProcAddress("wglGetPbufferDCARB");
	
	GL_GetProcAddress("wglReleaseTexImageARB");
}

qbool VID_IsPBufferSupported()
{
	static qbool supported = false;
	static qbool checked = false;

	if(!checked)
	{
		supported = 
			   CheckExtension("GL_ARB_multitexture") 
			&& CheckExtension("WGL_ARB_pbuffer")
			&& CheckExtension("WGL_ARB_pixel_format")
			&& CheckExtension("WGL_ARB_render_texture");
		checked = true;
	}

	return supported;
}

void VID_ShutdownPBuffer(pbuffer_s * pbuffer)
{
	if (pbuffer->hpbuffer)
	{
		// Check if we are currently rendering in the pbuffer
        if (wglGetCurrentContext() == pbuffer->hglrc )
		{
            wglMakeCurrent(0,0);
		}
		
		// Delete the pbuffer context
		wglDeleteContext( pbuffer->hglrc );
        wglReleasePbufferDCARB( pbuffer->hpbuffer, pbuffer->hdc );
        wglDestroyPbufferARB( pbuffer->hpbuffer );
        pbuffer->hpbuffer = 0;
	}
}

/*
"GL_ARB_multitexture " \
"WGL_ARB_pbuffer " \
"WGL_ARB_pixel_format " \
"WGL_ARB_render_texture " \
"GL_SGIS_generate_mipmap "
*/

void VID_InitPBuffer(pbuffer_s * pbuffer, int width, int height)
{
	HDC		hdc;
	HGLRC	hglrc;
	int     pformat[PBUFFER_MAX_PFORMATS];
	unsigned int nformats;    
	int     iattributes[2 * PBUFFER_MAX_ATTRIBS];
	float   fattributes[2 * PBUFFER_MAX_ATTRIBS];
	int     nfattribs = 0;
	int     niattribs = 0;
	int     format;

	if (!VID_IsPBufferSupported())
	{
		return;
	}

	memset(pbuffer, 0, sizeof(pbuffer_s));

	hdc = wglGetCurrentDC();
	hglrc = wglGetCurrentContext();

	//
	// Query for a suitable pixel format based on the specified mode.
	//
	{
		// Attribute arrays must be "0" terminated - for simplicity, first
		// just zero-out the array entire, then fill from left to right.
		memset(iattributes, 0, sizeof(int) * 2 * PBUFFER_MAX_ATTRIBS);
		memset(fattributes, 0, sizeof(float) * 2 * PBUFFER_MAX_ATTRIBS);

		// Since we are trying to create a pbuffer, the pixel format we
		// request (and subsequently use) must be "p-buffer capable".
		iattributes[niattribs  ] = WGL_DRAW_TO_PBUFFER_ARB;
		iattributes[++niattribs] = GL_TRUE;

		// We are asking for a pbuffer that is meant to be bound
		// as an RGBA texture - therefore we need a color plane
		iattributes[++niattribs] = WGL_BIND_TO_TEXTURE_RGBA_ARB;
		iattributes[++niattribs] = GL_TRUE;

		if ( !wglChoosePixelFormatARB( hdc, iattributes, fattributes, PBUFFER_MAX_PFORMATS, pformat, &nformats ) )
		{
			Sys_Error ("pbuffer creation error:  wglChoosePixelFormatARB() failed.\n");
		}

		if ( nformats <= 0 )
		{
			Sys_Error ("pbuffer creation error:  Couldn't find a suitable pixel format.\n");
		}

		format = pformat[0];
	}

	//
	// Set up the pbuffer attributes
	//
	{
		// Reset the array.
		memset(iattributes, 0, sizeof(int) * 2 * PBUFFER_MAX_ATTRIBS);
		niattribs = 0;
		
		// The render texture format is RGBA
		iattributes[niattribs] = WGL_TEXTURE_FORMAT_ARB;
		iattributes[++niattribs] = WGL_TEXTURE_RGBA_ARB;

		// The render texture target is GL_TEXTURE_2D
		iattributes[++niattribs] = WGL_TEXTURE_TARGET_ARB;
		iattributes[++niattribs] = WGL_TEXTURE_2D_ARB;

		// Ask to allocate room for the mipmaps
		//iattributes[++niattribs] = WGL_MIPMAP_TEXTURE_ARB;
		//iattributes[++niattribs] = TRUE;

		// Ask to allocate the largest pbuffer it can, if it is
		// unable to allocate for the width and height
		iattributes[++niattribs] = WGL_PBUFFER_LARGEST_ARB;
		iattributes[++niattribs] = FALSE;
	}

	//
	// Create the p-buffer.
	//
	{
		pbuffer->hpbuffer = wglCreatePbufferARB( hdc, format, width, height, iattributes );
	    
		if ( pbuffer->hpbuffer == 0)
		{
			Sys_Error ("pbuffer creation error:  wglCreatePbufferARB() failed\n");
		}
	}

	//
	// Get the device context.
	//
	{
		pbuffer->hdc = wglGetPbufferDCARB( pbuffer->hpbuffer );
		if ( pbuffer->hdc == 0)
		{
			Sys_Error ("pbuffer creation error:  wglGetPbufferDCARB() failed\n");
		}
	}

	//
	// Create a gl context for the p-buffer.
	//
	{
		pbuffer->hglrc = wglCreateContext( pbuffer->hdc );
		if ( pbuffer->hglrc == 0)
		{
			 Sys_Error ("pbuffer creation error:  wglCreateContext() failed\n");
		}
	}

	//
	// Determine the actual width and height we were able to create.
	//
	{
		wglQueryPbufferARB( pbuffer->hpbuffer, WGL_PBUFFER_WIDTH_ARB, &pbuffer->width );
		wglQueryPbufferARB( pbuffer->hpbuffer, WGL_PBUFFER_HEIGHT_ARB, &pbuffer->height );
	}
}

static pbuffer_s q_pbuffer = {0, 0, 0, 0, 0};

pbuffer_s *VID_GetPBuffer()
{
	if ( !q_pbuffer.hpbuffer )
	{
		VID_InitPBuffer(&q_pbuffer, vid.width, vid.height);
	}

	return &q_pbuffer;
}

#endif PBUFFER

/********************************** HW GAMMA **********************************/

void VID_ShiftPalette (unsigned char *palette) {}

//Note: ramps must point to a static array
void VID_SetDeviceGammaRamp(unsigned short *ramps) {
	if (!vid_gammaworks)
		return;

	currentgammaramp = ramps;

	if (!vid_hwgamma_enabled)
		return;

	customgamma = true;

	if (Win2K) {
		int i, j;

		for (i = 0; i < 128; i++) {
			for (j = 0; j < 3; j++)
				ramps[j * 256 + i] = min(ramps[j * 256 + i], (i + 0x80) << 8);
		}
		for (j = 0; j < 3; j++)
			ramps[j * 256 + 128] = min(ramps[j * 256 + 128], 0xFE00);
	}

	if (vid_3dfxgamma)
		wglSetDeviceGammaRamp3DFX(maindc, ramps);
	else
		SetDeviceGammaRamp(maindc, ramps);
}

void InitHWGamma (void) {
	if (COM_CheckParm("-nohwgamma") && (!strncasecmp(Rulesets_Ruleset(), "MTFL", 4))) // FIXME
		return;
	if (vid_3dfxgamma)
		vid_gammaworks = wglGetDeviceGammaRamp3DFX(maindc, systemgammaramp);
	else
	{
		vid_gammaworks = GetDeviceGammaRamp(maindc, systemgammaramp);
		if (vid_gammaworks && !COM_CheckParm("-nogammareset"))
		{
			int i, j;
			for (i = 0; i < 3; i++)
				for (j = 0; j < 256; j++)
					systemgammaramp[i][j] = (j << 8);
		}
	}
}

static void RestoreHWGamma(void) {
	if (vid_gammaworks && customgamma) {
		customgamma = false;
		if (vid_3dfxgamma)
			wglSetDeviceGammaRamp3DFX(maindc, systemgammaramp);
		else
			SetDeviceGammaRamp(maindc, systemgammaramp);
	}
}

/*********************************** OPENGL ***********************************/

void GL_BeginRendering (int *x, int *y, int *width, int *height) {
	*x = *y = 0;
	*width  = window_width;
	*height = window_height;
}

void GL_EndRendering (void) {
	static qbool old_hwgamma_enabled;

	vid_hwgamma_enabled = vid_hwgammacontrol.value && vid_gammaworks && ActiveApp && !Minimized;
	vid_hwgamma_enabled = vid_hwgamma_enabled && (modestate == MS_FULLDIB || vid_hwgammacontrol.value == 2);
	if (vid_hwgamma_enabled != old_hwgamma_enabled) {
		old_hwgamma_enabled = vid_hwgamma_enabled;
		if (vid_hwgamma_enabled && currentgammaramp)
			VID_SetDeviceGammaRamp (currentgammaramp);
		else
			RestoreHWGamma ();
	}

	if (!scr_skipupdate || block_drawing) {

		if (wglSwapIntervalEXT && update_vsync && vid_vsync.string[0])
			wglSwapIntervalEXT(vid_vsync.value ? 1 : 0);
		update_vsync = false;

		// Multiview - Only swap the back buffer to front when all views have been drawn in multiview.
		if (cl_multiview.value && cls.mvdplayback) 
		{
			if (CURRVIEW == 1)
			{
				SwapBuffers(maindc);
			}
		}
		else 
		{
			// Normal, swap on each frame.
			SwapBuffers(maindc); 
		}
	}

	// handle the mouse state when windowed if that's changed
	if (modestate == MS_WINDOWED) {
		if (!_windowed_mouse.value) {
			if (windowed_mouse)	{
				IN_DeactivateMouse ();
				IN_ShowMouse ();
				windowed_mouse = false;
			}
		} else {
			windowed_mouse = true;
			if (key_dest == key_game && !mouseactive && ActiveApp) {
				IN_ActivateMouse ();
				IN_HideMouse ();
			} else if (mouseactive && key_dest != key_game) {
				IN_DeactivateMouse ();
				IN_ShowMouse ();
			}
		}
	}
}

/******************************** VID SHUTDOWN ********************************/

void VID_Shutdown (void) {
   	HGLRC hRC;
   	HDC hDC;

	if (vid_initialized) {
		RestoreHWGamma ();

		vid_canalttab = false;
		hRC = wglGetCurrentContext();
    	hDC = wglGetCurrentDC();

    	wglMakeCurrent(NULL, NULL);

		if (hRC)
			wglDeleteContext(hRC);

		if (hDC && dibwindow)
			ReleaseDC(dibwindow, hDC);

		if (modestate == MS_FULLDIB)
			ChangeDisplaySettings (NULL, 0);

		if (maindc && dibwindow)
			ReleaseDC (dibwindow, maindc);

		AppActivate(false, false);
	}
}

/************************ SIGNALS, MESSAGES, INPUT ETC ************************/

void IN_ClearStates (void);

void ClearAllStates (void) {
	int i;

	// send an up event for each key, to make sure the server clears them all
	for (i = 0; i < sizeof(keydown) / sizeof(*keydown); i++) {
		if (keydown[i])
			Key_Event (i, false);
	}

	Key_ClearStates ();
	IN_ClearStates ();
}

void AppActivate(BOOL fActive, BOOL minimize) {
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
	static BOOL	sound_active;

	ActiveApp = fActive;
	Minimized = minimize;

	// enable/disable sound on focus gain/loss
	if (!ActiveApp && sound_active) {
		S_BlockSound ();
		sound_active = false;
	} else if (ActiveApp && !sound_active) {
		S_UnblockSound ();
		sound_active = true;
	}

	if (fActive) {
		if (modestate == MS_FULLDIB) {
			IN_ActivateMouse ();
			IN_HideMouse ();

			if (vid_canalttab && vid_wassuspended) {
				vid_wassuspended = false;
				if (ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
					Sys_Error ("Couldn't set fullscreen DIB mode");
				ShowWindow (mainwindow, SW_SHOWNORMAL);

				// Fix for alt-tab bug in NVidia drivers
				MoveWindow (mainwindow, 0, 0, gdevmode.dmPelsWidth, gdevmode.dmPelsHeight, false);

				// scr_fullupdate = 0;
				Sbar_Changed ();
			}
		} else if (modestate == MS_WINDOWED && Minimized) {
			ShowWindow (mainwindow, SW_RESTORE);
		} else if ((modestate == MS_WINDOWED) && _windowed_mouse.value && key_dest == key_game) {
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
		if ((vid_canalttab && !Minimized) && currentgammaramp) {
			VID_SetDeviceGammaRamp (currentgammaramp);
			// VVD: din't restore gamma after ALT+TAB on some ATI video cards (or drivers?...)
			// HACK!!! FIXME {
			if (restore_gamma == 0 && (int)vid_forcerestoregamma.value)
				restore_gamma = 1;
			// }
		}
	} else {
		allow_flash = true;
		RestoreHWGamma ();
		if (modestate == MS_FULLDIB) {
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			if (vid_canalttab) { 
				ChangeDisplaySettings (NULL, 0);
				vid_wassuspended = true;
			}
		} else if ((modestate == MS_WINDOWED) && _windowed_mouse.value) {
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		}
	}
}

LONG CDAudio_MessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#ifdef WITH_KEYMAP
#else // WITH_KEYMAP
int IN_MapKey (int key);
#endif // WITH_KEYMAP else


#include "mw_hook.h"
void MW_Hook_Message (long buttons);


/* main window procedure */
LONG WINAPI MainWndProc (HWND    hWnd, UINT    uMsg, WPARAM  wParam, LPARAM  lParam) {
    LONG lRet = 1;
	int fActive, fMinimized, temp;
	extern unsigned int uiWheelMessage;

	// VVD: din't restore gamma after ALT+TAB on some ATI video cards (or drivers?...)
	// HACK!!! FIXME {
	static time_t time_old;
	if (restore_gamma == 2 && currentgammaramp) {
		if (time(NULL) - time_old > 0) {
			VID_SetDeviceGammaRamp (currentgammaramp);
			restore_gamma = 0;
		}
	}
	// }

	if (uMsg == uiWheelMessage) {
		uMsg = WM_MOUSEWHEEL;
		wParam <<= 16;
	}

    switch (uMsg) {
		case WM_KILLFOCUS:
			if (modestate == MS_FULLDIB)
				ShowWindow(mainwindow, SW_SHOWMINNOACTIVE);
			break;

		case WM_CREATE:
			break;

		case WM_MOVE:
			window_x = (int) LOWORD(lParam);
			window_y = (int) HIWORD(lParam);
			VID_UpdateWindowStatus ();
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
#ifdef WITH_KEYMAP
			IN_TranslateKeyEvent (lParam, wParam, true);
#else // WITH_KEYMAP
			Key_Event (IN_MapKey(lParam), true);
#endif // WITH_KEYMAP else
			break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
#ifdef WITH_KEYMAP
			IN_TranslateKeyEvent (lParam, wParam, false);
#else // WITH_KEYMAP
			Key_Event (IN_MapKey(lParam), false);
#endif // WITH_KEYMAP else
			break;

		case WM_SYSCHAR:
			// keep Alt-Space from happening
			break;
		// this is complicated because Win32 seems to pack multiple mouse events into
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

		// JACK: This is the mouse wheel with the Intellimouse
		// Its delta is either positive or neg, and we generate the proper Event.
		case WM_MOUSEWHEEL:
			if (in_mwheeltype != MWHEEL_DINPUT)
			{
				in_mwheeltype = MWHEEL_WINDOWMSG;

				if ((short) HIWORD(wParam) > 0) {
					Key_Event(K_MWHEELUP, true);
					Key_Event(K_MWHEELUP, false);
				} else {
					Key_Event(K_MWHEELDOWN, true);
					Key_Event(K_MWHEELDOWN, false);
				}

				// when an application processes the WM_MOUSEWHEEL message, it must return zero
				lRet = 0;
			}
			break;

		case WM_MWHOOK:
			MW_Hook_Message (lParam);
			break;


    	case WM_SIZE:
            break;

   	    case WM_CLOSE:
			if (MessageBox (mainwindow, "Are you sure you want to quit?", "ezQuake : Confirm Exit",
						MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES)
			{
				Host_Quit ();
			}

	        break;

		case WM_ACTIVATE:
			fActive = LOWORD(wParam);
			fMinimized = (BOOL) HIWORD(wParam);
			AppActivate(!(fActive == WA_INACTIVE), fMinimized);

			// VVD: din't restore gamma after ALT+TAB on some ATI video cards (or drivers?...)
			// HACK!!! FIXME {
			if (restore_gamma == 1) {
				time_old = time(NULL);
				restore_gamma = 2;
			}
			// }
			// fix the leftover Alt from any Alt-Tab or the like that switched us away
			ClearAllStates ();

			break;

   	    case WM_DESTROY:
			if (dibwindow)
				DestroyWindow (dibwindow);

            PostQuitMessage (0);
			break;

		case MM_MCINOTIFY:
			lRet = CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);
			break;

		default:
			/* pass all unhandled messages to DefWindowProc */
			lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
			break;
	}

	/* return 1 if handled message, 0 if not */
	return lRet;
}

/********************************* VID MODES *********************************/

int VID_NumModes (void) {
	return nummodes;
}

vmode_t *VID_GetModePtr (int modenum) {
	if (modenum >= 0 && modenum < nummodes)
		return &modelist[modenum];

	return &badmode;
}

char *VID_GetModeDescription (int mode) {
	char *pinfo;
	vmode_t *pv;
	static char	temp[100];

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	if (!leavecurrentmode) {
		pv = VID_GetModePtr (mode);
		pinfo = pv->modedesc;
	} else {
		sprintf (temp, "Desktop resolution (%dx%d)", modelist[MODE_FULLSCREEN_DEFAULT].width, modelist[MODE_FULLSCREEN_DEFAULT].height);
		pinfo = temp;
	}

	return pinfo;
}


// KJB: Added this to return the mode driver name in description for console

char *VID_GetExtModeDescription (int mode) {
	static char	pinfo[40];
	vmode_t *pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	pv = VID_GetModePtr (mode);
	if (modelist[mode].type == MS_FULLDIB) {
		if (!leavecurrentmode) {
			sprintf(pinfo,"%12s fullscreen", pv->modedesc); // "%dx%dx%d" worse is WWWWxHHHHxBB
		} else {
			sprintf (pinfo, "Desktop resolution (%dx%d)",
					 modelist[MODE_FULLSCREEN_DEFAULT].width,
					 modelist[MODE_FULLSCREEN_DEFAULT].height);
		}
	} else {
		if (modestate == MS_WINDOWED)
			sprintf(pinfo, "%12s windowed", pv->modedesc); //  "%dx%d" worse is WWWWxHHHH
		else
			strcpy(pinfo, "windowed");
	}

	return pinfo;
}

void VID_ModeList_f (void) {
	int i, lnummodes, t, width = -1, height = -1, bpp = -1;
	char *pinfo;
	vmode_t *pv;

	if ((i = Cmd_CheckParm("-w")) && i + 1 < Cmd_Argc())
		width = Q_atoi(Cmd_Argv(i+1));

	if ((i = Cmd_CheckParm("-h")) && i + 1 < Cmd_Argc())
		height = Q_atoi(Cmd_Argv(i+1));

	if ((i = Cmd_CheckParm("-b")) && i + 1 < Cmd_Argc())
		bpp = Q_atoi(Cmd_Argv(i+1));

	if ((i = Cmd_CheckParm("b32")))
		bpp = 32;

	if ((i = Cmd_CheckParm("b16")))
		bpp = 16;

	lnummodes = VID_NumModes ();

	t = leavecurrentmode;
	leavecurrentmode = 0;

	for (i = 1; i < lnummodes; i++) {
		if (width != -1 && modelist[i].width != width)
			continue;

		if (height != -1 && modelist[i].height != height)
			continue;

		if (bpp != -1 && modelist[i].bpp != bpp)
			continue;

		pv = VID_GetModePtr (i);
		pinfo = VID_GetExtModeDescription (i);
		Com_Printf ("%3d: %s\n", i, pinfo);
	}

	leavecurrentmode = t;
}

/********************************** VID INIT **********************************/

void VID_InitDIB (HINSTANCE hInstance) {
	int temp;
	WNDCLASS wc;

	/* Register the frame class */
    wc.style         = 0;
    wc.lpfnWndProc   = (WNDPROC)MainWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = 0;
    wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wc.hbrBackground = NULL;
    wc.lpszMenuName  = 0;
    wc.lpszClassName = "ezQuake";

    if (!RegisterClass (&wc) )
		Sys_Error ("Couldn't register window class");

	modelist[0].type = MS_WINDOWED;

	if ((temp = COM_CheckParm("-width")) && temp + 1 < COM_Argc())
		modelist[0].width = Q_atoi(COM_Argv(temp + 1));
	else
		modelist[0].width = 640;

	if (modelist[0].width < 320)
		modelist[0].width = 320;

	if ((temp = COM_CheckParm("-height")) && temp + 1 < COM_Argc())
		modelist[0].height= Q_atoi(COM_Argv(temp + 1));
	else
		modelist[0].height = modelist[0].width * 240 / 320;

	if (modelist[0].height < 240)
		modelist[0].height = 240;

	sprintf (modelist[0].modedesc, "%dx%d", modelist[0].width, modelist[0].height);

	modelist[0].modenum = MODE_WINDOWED;
	modelist[0].dib = 1;
	modelist[0].fullscreen = 0;
	modelist[0].halfscreen = 0;
	modelist[0].bpp = 0;

	nummodes = 1;
}

void VID_InitFullDIB (HINSTANCE hInstance) {
	DEVMODE	devmode;
	int i, modenum, originalnummodes, numlowresmodes, j, bpp, done;
	BOOL stat;

	// enumerate >8 bpp modes
	originalnummodes = nummodes;
	modenum = 0;

	do {
		stat = EnumDisplaySettings (NULL, modenum, &devmode);

		if (devmode.dmBitsPerPel >= 15 && devmode.dmPelsWidth <= MAXWIDTH && devmode.dmPelsHeight <= MAXHEIGHT && nummodes < MAX_MODE_LIST) {
			devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

			if (ChangeDisplaySettings (&devmode, CDS_TEST | (windowed ? 0 : CDS_FULLSCREEN)) == DISP_CHANGE_SUCCESSFUL) {
				modelist[nummodes].type = (windowed ? MS_WINDOWED : MS_FULLDIB);
				modelist[nummodes].width = devmode.dmPelsWidth;
				modelist[nummodes].height = devmode.dmPelsHeight;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].halfscreen = 0;
				modelist[nummodes].dib = 1;
				modelist[nummodes].fullscreen = (windowed ? 0 : 1);
				modelist[nummodes].bpp = (windowed ? 0 : devmode.dmBitsPerPel);

				if (windowed)
					sprintf (modelist[nummodes].modedesc, "%dx%d", devmode.dmPelsWidth, devmode.dmPelsHeight);
				else
					sprintf (modelist[nummodes].modedesc, "%dx%dx%d", devmode.dmPelsWidth, devmode.dmPelsHeight, devmode.dmBitsPerPel);

				// if the width is more than twice the height, reduce it by half because this is probably a dual-screen monitor
				if (!COM_CheckParm("-noadjustaspect")) {
					if (modelist[nummodes].width > (modelist[nummodes].height << 1)) {
						modelist[nummodes].width >>= 1;
						modelist[nummodes].halfscreen = 1;
						sprintf (modelist[nummodes].modedesc, "%dx%dx%d",
							modelist[nummodes].width, modelist[nummodes].height, modelist[nummodes].bpp);
					}
				}

				for (i = originalnummodes; i < nummodes; i++) {
					if (modelist[nummodes].width == modelist[i].width &&
						modelist[nummodes].height == modelist[i].height &&
						modelist[nummodes].bpp == modelist[i].bpp)
					{
						break;
					}
				}
				if (i == nummodes)
					nummodes++;
			}
		}
		modenum++;
	} while (stat);

	// see if there are any low-res modes that aren't being reported
	numlowresmodes = sizeof(lowresmodes) / sizeof(lowresmodes[0]);
	bpp = 16;
	done = 0;

	do {
		for (j = 0; j < numlowresmodes && nummodes < MAX_MODE_LIST; j++) {
			devmode.dmBitsPerPel = bpp;
			devmode.dmPelsWidth = lowresmodes[j].width;
			devmode.dmPelsHeight = lowresmodes[j].height;
			devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

			if (ChangeDisplaySettings (&devmode, CDS_TEST | (windowed ? 0 : CDS_FULLSCREEN)) == DISP_CHANGE_SUCCESSFUL) {
				modelist[nummodes].type = (windowed ? MS_WINDOWED : MS_FULLDIB);
				modelist[nummodes].width = devmode.dmPelsWidth;
				modelist[nummodes].height = devmode.dmPelsHeight;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].halfscreen = 0;
				modelist[nummodes].dib = 1;
				modelist[nummodes].fullscreen = (windowed ? 0 : 1);
				modelist[nummodes].bpp = (windowed ? 0 : devmode.dmBitsPerPel);

				if (windowed)
					sprintf (modelist[nummodes].modedesc, "%dx%d", devmode.dmPelsWidth, devmode.dmPelsHeight);
				else
					sprintf (modelist[nummodes].modedesc, "%dx%dx%d", devmode.dmPelsWidth, devmode.dmPelsHeight, devmode.dmBitsPerPel);

				for (i = originalnummodes; i < nummodes; i++) {
					if (modelist[nummodes].width == modelist[i].width &&
						modelist[nummodes].height == modelist[i].height &&
						modelist[nummodes].bpp == modelist[i].bpp)
					{
						break;
					}
				}

				if (i == nummodes)
					nummodes++;
			}
		}
		switch (bpp) {
			case 16:
				bpp = 32; break;
			case 32:
				bpp = 24; break;
			case 24:
				done = 1; break;
		}
	} while (!done);

	if (nummodes == originalnummodes)
		Com_Printf ("No fullscreen DIB modes found\n");
}

void VID_Restart_f (void);

void VID_Init (unsigned char *palette) {
	int i, temp, basenummodes, width, height, bpp, findbpp, done;
	HDC hdc;
	DEVMODE	devmode;

	if (COM_CheckParm("-window") || COM_CheckParm("-startwindowed"))
		windowed = true;

	memset(&devmode, 0, sizeof(devmode));

	Cvar_SetCurrentGroup(CVAR_GROUP_VIDEO);

//	Cvar_Register (&_vid_default_mode);
//	Cvar_Register (&_vid_default_mode_win);
//	Cvar_Register (&vid_config_x);
//	Cvar_Register (&vid_config_y);

	Cvar_Register (&vid_ref);
	Cvar_Register (&vid_mode);
	Cvar_Register (&vid_conwidth);
	Cvar_Register (&vid_conheight);
	Cvar_Register (&vid_hwgammacontrol);
	Cvar_Register (&vid_displayfrequency);
	Cvar_Register (&vid_flashonactivity);
    Cvar_Register (&vid_forcerestoregamma);

	Cvar_Register (&_windowed_mouse);

	Cvar_ResetCurrentGroup();

	Cmd_AddCommand ("vid_modelist", VID_ModeList_f);
	Cmd_AddCommand ("vid_restart", VID_Restart_f);

	hIcon = LoadIcon (global_hInstance, MAKEINTRESOURCE (IDI_APPICON));

	VID_InitDIB (global_hInstance);
	basenummodes = nummodes;

	VID_InitFullDIB (global_hInstance);

	if (windowed) {
		hdc = GetDC (NULL);

		if (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE)
			Sys_Error ("Can't run in non-RGB mode");

		ReleaseDC (NULL, hdc);

		if ((temp = COM_CheckParm("-mode")) && temp + 1 < COM_Argc())
			vid_default = Q_atoi(COM_Argv(temp + 1));
		else if (vid_mode.value != NO_MODE) // serve +set vid_mode
			vid_default = vid_mode.value;
		else {
			vid_default = NO_MODE;

			width  = modelist[0].width;
			height = modelist[0].height;

			for (i = 1; i < nummodes; i++)
				if (modelist[i].width == width && (!height || modelist[i].height == height)) {
					vid_default = i;
					break;
				}

			vid_default = (vid_default == NO_MODE ? MODE_WINDOWED : vid_default);
		}
	}
	else {
		Cmd_AddCommand ("vid_showfreq", VID_ShowFreq_f);

		if (nummodes == 1)
			Sys_Error ("No RGB fullscreen modes available");

		windowed = false;

		if ((temp = COM_CheckParm("-mode")) && temp + 1 < COM_Argc()) {
			vid_default = Q_atoi(COM_Argv(temp + 1));
		}
		else if (vid_mode.value != NO_MODE) { // serve +set vid_mode
			vid_default = vid_mode.value;
		}
		else if (COM_CheckParm("-current")) {
			modelist[MODE_FULLSCREEN_DEFAULT].width = GetSystemMetrics (SM_CXSCREEN);
			modelist[MODE_FULLSCREEN_DEFAULT].height = GetSystemMetrics (SM_CYSCREEN);
			vid_default = MODE_FULLSCREEN_DEFAULT;
			leavecurrentmode = 1;
		} else {
			if ((temp = COM_CheckParm("-width")) && temp + 1 < COM_Argc())
				width = Q_atoi(COM_Argv(temp + 1));
			else
				width = 640;

			if ((temp = COM_CheckParm("-bpp")) && temp + 1 < COM_Argc()) {
				bpp = Q_atoi(COM_Argv(temp + 1));
				findbpp = 0;
			} else {
				bpp = 32;
				findbpp = 1;
			}

			if ((temp = COM_CheckParm("-height")) && temp + 1 < COM_Argc())
				height = Q_atoi(COM_Argv(temp + 1));

			// if they want to force it, add the specified mode to the list
			if (COM_CheckParm("-force") && nummodes < MAX_MODE_LIST) {
				modelist[nummodes].type = MS_FULLDIB;
				modelist[nummodes].width = width;
				modelist[nummodes].height = height;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].halfscreen = 0;
				modelist[nummodes].dib = 1;
				modelist[nummodes].fullscreen = 1;
				modelist[nummodes].bpp = bpp;
				sprintf (modelist[nummodes].modedesc, "%dx%dx%d", devmode.dmPelsWidth, devmode.dmPelsHeight, devmode.dmBitsPerPel);

				for (i = nummodes; i < nummodes; i++) {
					if ((modelist[nummodes].width == modelist[i].width)   &&
						(modelist[nummodes].height == modelist[i].height) &&
						(modelist[nummodes].bpp == modelist[i].bpp))
					{
						break;
					}
				}

				if (i == nummodes)
					nummodes++;
			}

			done = 0;

			do {
				height = 0;
				if ((temp = COM_CheckParm("-height")) && temp + 1 < COM_Argc())
					height = Q_atoi(COM_Argv(temp + 1));
				else
					height = 0;

				for (i = 1, vid_default = 0; i < nummodes; i++) {
					if (modelist[i].width == width && (!height || modelist[i].height == height) && modelist[i].bpp == bpp) {
						vid_default = i;
						done = 1;
						break;
					}
				}

				if (findbpp && !done) {
					switch (bpp) {
					case 32:
						bpp = 16; break;
					case 16:
						bpp = 15; break;
					case 15:
						bpp = 24; break;
					case 24:
						done = 1; break;
					}
				} else {
					done = 1;
				}
			} while (!done);

			if (!vid_default)
				Sys_Error ("Specified video mode not available");
		}
	}

	if ((i = COM_CheckParm("-freq")) && i + 1 < COM_Argc())
		Cvar_Set(&vid_displayfrequency, COM_Argv(i + 1));

	vid_initialized = true;

	if ((i = COM_CheckParm("-conwidth")) && i + 1 < COM_Argc())
		Cvar_SetValue(&vid_conwidth, (float)Q_atoi(COM_Argv(i + 1)));
	else // this is ether +set vid_con... or just default value which we select in cvar initialization
		Cvar_SetValue(&vid_conwidth, vid_conwidth.value); // must trigger callback which validate value

	if ((i = COM_CheckParm("-conheight")) && i + 1 < COM_Argc())
		Cvar_SetValue(&vid_conheight, (float)Q_atoi(COM_Argv(i + 1)));
	else // this is ether +set vid_con... or just default value which we select in cvar initialization
		 // also select vid_conheight with proper aspect ratio if user omit it
		Cvar_SetValue(&vid_conheight, vid_conheight.value ? vid_conheight.value : vid_conwidth.value * 3 / 4); // must trigger callback which validate value

	vid.colormap = host_colormap;

	Check_Gamma(palette);
	VID_SetPalette (palette);

	VID_SetMode (vid_default, palette);

	maindc = GetDC(mainwindow);
	if (!bSetupPixelFormat(maindc))
		Sys_Error ("bSetupPixelFormat failed");

	InitHWGamma ();

	if (!(baseRC = wglCreateContext(maindc)))
		Sys_Error ("Could not initialize GL (wglCreateContext failed).\n\nMake sure you in are 65535 color mode, and try running -window.");
	if (!wglMakeCurrent(maindc, baseRC))
		Sys_Error ("wglMakeCurrent failed");

	GL_Init();
	GL_Init_Win();

	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;

	strlcpy (badmode.modedesc, "Bad mode", 9);
	vid_canalttab = true;
}

void VID_Restart ()
{
	// TODO: de-init more things, and re-init it

	if (baseRC)
		wglDeleteContext (baseRC);

	baseRC = NULL;

	VID_SetMode (vid_mode.value, host_basepal);

	baseRC = wglCreateContext( maindc );
	if (!baseRC)
		Sys_Error ("Could not initialize GL (wglCreateContext failed).\n\nMake sure you in are 65535 color mode, and try running -window.");

	if (!wglMakeCurrent( maindc, baseRC ))
		Sys_Error ("wglMakeCurrent failed");
}

void VID_Restart_f (void)
{
	extern void GFX_Init(void);

	if (!host_initialized) { // sanity
		Com_Printf("Can't do %s yet\n", Cmd_Argv(0));
		return;
	}

	VID_Restart();

	GL_Init();
#ifdef WIN32
	GL_Init_Win();
#else
	// TODO: some *nix related here
#endif

	// force models to reload (just flush, no actual loading code here)
	Cache_Flush();

	// reload 2D textures, particles textures, some other textures and gfx.wad
	GFX_Init();

	// we need done something like for map reloading, for example reload textures for brush models
	R_NewMap(true);

	// force all cached models to be loaded, so no short HDD lag then u walk over level and discover new model
	Mod_TouchModels();
}


/********************************** VID MENU **********************************/

#define VID_ROW_SIZE	3

void M_Menu_Options_f (void);
void M_Print (int cx, int cy, char *str);
void M_PrintWhite (int cx, int cy, char *str);
void M_DrawPic (int x, int y, mpic_t *pic);

static int	vid_line, vid_wmodes;

typedef struct {
	int		modenum;
	char	*desc;
	int		iscur;
} modedesc_t;


#define MAX_COLUMN_SIZE		9
#define MODE_AREA_HEIGHT	(MAX_COLUMN_SIZE + 2)
#define MAX_MODEDESCS		(MAX_COLUMN_SIZE * 3)

static modedesc_t	modedescs[MAX_MODEDESCS];

void VID_MenuDraw (void) {
	mpic_t *p;
	char *ptr;
	int lnummodes, i, k, column, row;
	vmode_t *pv;

	p = Draw_CachePic ("gfx/vidmodes.lmp");
	M_DrawPic ( (320 - p->width)/2, 4, p);

	vid_wmodes = 0;
	lnummodes = VID_NumModes ();

	for (i = 1; i < lnummodes && vid_wmodes < MAX_MODEDESCS; i++) {
		ptr = VID_GetModeDescription (i);
		pv = VID_GetModePtr (i);

		k = vid_wmodes;

		modedescs[k].modenum = i;
		modedescs[k].desc = ptr;
		modedescs[k].iscur = 0;

		if (i == vid_modenum)
			modedescs[k].iscur = 1;

		vid_wmodes++;

	}

	if (vid_wmodes > 0) {
		M_Print (2 * 8, 36 + 0 * 8, "Fullscreen Modes (WIDTHxHEIGHTxBPP)");

		column = 8;
		row = 36 + 2 * 8;

		for (i = 0; i < vid_wmodes; i++) {
			if (modedescs[i].iscur)
				M_PrintWhite (column, row, modedescs[i].desc);
			else
				M_Print (column, row, modedescs[i].desc);

			column += 13*8;

			if ((i % VID_ROW_SIZE) == (VID_ROW_SIZE - 1)) {
				column = 8;
				row += 8;
			}
		}
	}

	M_Print (3 * 8, 36 + MODE_AREA_HEIGHT  *  8 + 8 * 2, "Video modes must be set from the");
	M_Print (3 * 8, 36 + MODE_AREA_HEIGHT  *  8 + 8 * 3, "command line with -width <width>");
	M_Print (3 * 8, 36 + MODE_AREA_HEIGHT  *  8 + 8 * 4, "and -bpp <bits-per-pixel>");
	M_Print (3 * 8, 36 + MODE_AREA_HEIGHT  *  8 + 8 * 6, "Select windowed mode with -window");
}

void VID_MenuKey (int key) {
	switch (key) {
	case K_ESCAPE:
		S_LocalSound ("misc/menu1.wav");
		M_Menu_Options_f ();
		break;

	default:
		break;
	}
}

void VID_NotifyActivity(void) {
    if (!ActiveApp && vid_flashonactivity.value) { // && allow_flash
        FlashWindow(mainwindow, TRUE);
        allow_flash = false;
    }
}
