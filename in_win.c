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

$Id: in_win.c,v 1.39 2007-09-16 17:48:37 qqshka Exp $
*/
// in_win.c -- windows 95 mouse and joystick code

#define DIRECTINPUT_VERSION	0x0700

#include <dinput.h>

#ifndef DIMOFS_BUTTON4
	#pragma message("Warning: You don't have directx7 headers installed, directinput disabled")
	#undef DIRECTINPUT_VERSION
	#define DIRECTINPUT_VERSION 0
#endif


#include "quakedef.h"
#include "winquake.h"
#ifdef WITH_KEYMAP
#include "keymap.h"
#endif // WITH_KEYMAP

#include "movie.h"
#include "input.h"
#include "keys.h"


//#define iDirectInputCreate(a,b,c,d)	pDirectInputCreate(a,b,c,d)

//HRESULT (WINAPI *pDirectInputCreate)(HINSTANCE hinst, DWORD dwVersion,
	//LPDIRECTINPUT * lplpDirectInput, LPUNKNOWN punkOuter);
static HRESULT (WINAPI *pDirectInputCreateEx)(HINSTANCE hinst,
		DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, LPUNKNOWN punkOuter) = NULL;

// mouse variables
cvar_t	m_filter = {"m_filter", "0", CVAR_ARCHIVE | CVAR_SILENT};

// compatibility with old Quake -- setting to 0 disables KP_* codes
cvar_t	cl_keypad = {"cl_keypad", "1", CVAR_ARCHIVE};

int			mouse_buttons;
int			mouse_oldbuttonstate;
POINT		current_pos;
int			mx_accum, my_accum;

static qbool	restore_spi;
static int originalmouseparms[3], newmouseparms[3];
qbool mouseinitialized = false;
static qbool	mouseparmsvalid, mouseactivatetoggle;
static qbool	dinput_acquired;
static unsigned int mstate_di;
unsigned int uiWheelMessage;

qbool	mouseactive;

qbool   input_initialized = false;

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

qbool	joy_avail, joy_advancedinit, joy_haspov;
DWORD		joy_oldbuttonstate, joy_oldpovstate;

int			joy_id;
DWORD		joy_flags;
DWORD		joy_numbuttons;

static JOYINFOEX	ji;

#if DIRECTINPUT_VERSION >= 0x700
static LPDIRECTINPUT7		g_pdi;
static LPDIRECTINPUTDEVICE7	g_pMouse;

static HINSTANCE hInstDI;

static qbool	dinput;

// Some drivers send DIMOFS_Z, some send WM_MOUSEWHEEL, and some send both.
// To get the mouse wheel to work in any case but avoid duplicate events,
// we will only use one event source, wherever we receive the first event.
mwheelmsg_t		in_mwheeltype;

typedef struct MYDATA {
	LONG  lX;                   // X axis goes here
	LONG  lY;                   // Y axis goes here
	LONG  lZ;                   // Z axis goes here
	BYTE  bButtonA;             // One button goes here
	BYTE  bButtonB;             // Another button goes here
	BYTE  bButtonC;             // Another button goes here
	BYTE  bButtonD;             // Another button goes here
	BYTE  bButtonE;             // Another button goes here
	BYTE  bButtonF;             // Another button goes here
	BYTE  bButtonG;             // Another button goes here
	BYTE  bButtonH;             // Another button goes here
} MYDATA;

static DIOBJECTDATAFORMAT rgodf[] = {
	{ &GUID_XAxis,	FIELD_OFFSET(MYDATA, lX),		DIDFT_AXIS | DIDFT_ANYINSTANCE, 0},
	{ &GUID_YAxis,	FIELD_OFFSET(MYDATA, lY),		DIDFT_AXIS | DIDFT_ANYINSTANCE, 0},
	{ &GUID_ZAxis,	FIELD_OFFSET(MYDATA, lZ),		0x80000000 | DIDFT_AXIS | DIDFT_ANYINSTANCE, 0},
	{ 0,			FIELD_OFFSET(MYDATA, bButtonA),	DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0},
	{ 0,			FIELD_OFFSET(MYDATA, bButtonB),	DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0},
	{ 0,			FIELD_OFFSET(MYDATA, bButtonC),	0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0},
	{ 0,			FIELD_OFFSET(MYDATA, bButtonD),	0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0},
	{ 0,			FIELD_OFFSET(MYDATA, bButtonE),	0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0},
	{ 0,			FIELD_OFFSET(MYDATA, bButtonF),	0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0},
	{ 0,			FIELD_OFFSET(MYDATA, bButtonG),	0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0},
	{ 0,			FIELD_OFFSET(MYDATA, bButtonH),	0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0}
};

#define NUM_OBJECTS (sizeof(rgodf) / sizeof(rgodf[0]))

static DIDATAFORMAT	df = {
	sizeof(DIDATAFORMAT),       // this structure
	sizeof(DIOBJECTDATAFORMAT), // size of object data format
	DIDF_RELAXIS,               // absolute axis coordinates
	sizeof(MYDATA),             // device data size
	NUM_OBJECTS,                // number of objects
	rgodf,                      // and here they are
};

#else
#define dinput false
#endif

typedef enum { mt_none = 0, mt_normal, mt_dinput } mousetype_t;

// forward-referenced functions
void IN_StartupJoystick (void);
void Joy_AdvancedUpdate_f (void);
void IN_JoyMove (usercmd_t *cmd);

// directinput features
cvar_t	m_rate				= {"m_rate",	         "125", CVAR_SILENT};
cvar_t	m_showrate			= {"m_showrate",         "0",   CVAR_SILENT};
cvar_t  in_mouse			= {"in_mouse",           "1",   CVAR_LATCH};  // NOTE: "1" is mt_normal
cvar_t  in_m_smooth			= {"in_m_smooth",        "0",   CVAR_LATCH};

cvar_t  in_m_mwhook			= {"in_m_mwhook",        "0",   CVAR_LATCH};

cvar_t  in_m_os_parameters	= {"in_m_os_parameters", "0",   CVAR_LATCH};

cvar_t  in_di_bufsize		= {"in_di_bufsize",      "16",  CVAR_LATCH}; // if you change default, then change DI_BufSize() too

qbool use_m_smooth = false;
HANDLE m_event;

HANDLE smooth_thread;

#define	 M_HIST_SIZE  64
#define	 M_HIST_MASK  (M_HIST_SIZE - 1)

typedef	struct msnap_s {
	long   data;	 //	data (relative axis	pos)
	double time;	// timestamp
} msnap_t;

msnap_t	 m_history_x[M_HIST_SIZE];	// history
msnap_t	 m_history_y[M_HIST_SIZE];
int		 m_history_x_wseq =	0;		// write sequence
int		 m_history_y_wseq =	0;
int		 m_history_x_rseq =	0;		// read	sequence
int		 m_history_y_rseq =	0;
int		 wheel_up_count	= 0;
int		 wheel_dn_count	= 0;

int		 last_wseq_printed = 0;

#define INPUT_CASE_DIMOFS_BUTTON(NUM)	\
	case (DIMOFS_BUTTON0 + NUM):		\
		if (od.dwData &	0x80)			\
			mstate_di |= (1	<< NUM);	\
		else							\
			mstate_di &= ~(1 <<	NUM);	\
		break;

#define INPUT_CASE_DINPUT_MOUSE_BUTTONS		\
		INPUT_CASE_DIMOFS_BUTTON(0);		\
		INPUT_CASE_DIMOFS_BUTTON(1);		\
		INPUT_CASE_DIMOFS_BUTTON(2);		\
		INPUT_CASE_DIMOFS_BUTTON(3);		\
		INPUT_CASE_DIMOFS_BUTTON(4);		\
		INPUT_CASE_DIMOFS_BUTTON(5);		\
		INPUT_CASE_DIMOFS_BUTTON(6);		\
		INPUT_CASE_DIMOFS_BUTTON(7);		\

#if DIRECTINPUT_VERSION >= 0x700

static unsigned int DI_BufSize(void)
{
	return bound(16, in_di_bufsize.integer, 256);
}

static void SetBufferSize(void) {
	static DIPROPDWORD dipdw = {
		{ sizeof(dipdw), sizeof(dipdw.diph), 0, DIPH_DEVICE },
		0
	};
	HRESULT hr;
	unsigned int bufsize;

	if (in_di_bufsize.integer)
	{  // we don't wont dynamic buffer change
		Com_Printf_State(PRINT_OK, "DirectInput overflow, increasing skipped because of %s.\n", in_di_bufsize.name);
		return;
	}

	bufsize = max(dipdw.dwData, DI_BufSize()); // well, DI_BufSize() return 16, since in_di_bufsize is zero
	dipdw.dwData = bufsize + max(1, bufsize / 2);

	Com_Printf_State(PRINT_INFO, "DirectInput overflow, increasing buffer size to %u.\n", dipdw.dwData);

	IN_DeactivateMouse();

	hr = IDirectInputDevice_SetProperty(g_pMouse, DIPROP_BUFFERSIZE, &dipdw.diph);

	IN_ActivateMouse();

	if(FAILED(hr)) {
		Com_Printf_State (PRINT_FAIL, "Unable to increase DirectInput buffer size.\n");
	}
}

DWORD WINAPI IN_SMouseProc (void* lpParameter) {
	// read mouse events and generate history tables
	DWORD ret;
	int value;

	while (1) {

		if (!use_m_smooth)
			ExitThread(1); // someone kill us

		if ((ret = WaitForSingleObject(m_event,	500/*INFINITE*/)) == WAIT_OBJECT_0) {
			int mx = 0, my = 0;
			DIDEVICEOBJECTDATA	od;
			HRESULT hr;
			double time;

			if (!ActiveApp || Minimized || !mouseactive || !dinput_acquired) {
				Sleep(50);
				continue;
			}

			time = Sys_DoubleTime();

			while (1) {
				DWORD dwElements = 1;

				hr = IDirectInputDevice_GetDeviceData(g_pMouse,	
					sizeof(DIDEVICEOBJECTDATA),	&od, &dwElements, 0);

				if ((hr	== DIERR_INPUTLOST)	|| (hr == DIERR_NOTACQUIRED)) {
					dinput_acquired	= false;
					break;
				}

				if(hr == DI_BUFFEROVERFLOW)
					SetBufferSize();

				/* Unable to read data or no data available	*/
				if (FAILED(hr) || !dwElements)
					break;

				/* Look	at the element to see what happened	*/
				switch (od.dwOfs) {
					case DIMOFS_X:
						m_history_x[m_history_x_wseq & M_HIST_MASK].time = time;
						m_history_x[m_history_x_wseq & M_HIST_MASK].data = od.dwData;
						m_history_x_wseq++;
						break;

					case DIMOFS_Y:
						m_history_y[m_history_y_wseq & M_HIST_MASK].time = time;
						m_history_y[m_history_y_wseq & M_HIST_MASK].data = od.dwData;
						m_history_y_wseq++;
						break;

					INPUT_CASE_DINPUT_MOUSE_BUTTONS;

				
					case DIMOFS_Z:
						if (in_mwheeltype != MWHEEL_WINDOWMSG)
						{
							in_mwheeltype = MWHEEL_DINPUT;
							value = od.dwData;

							if (value > 0)
								wheel_up_count++;
							else if (value < 0)
								wheel_dn_count++;
							else
								; // value == 0
						}
						break;

				}
			}
		}
	}
}

int IN_GetMouseRate(void) {
// returns frequency of mouse input in Hz
	double t;

	if (m_history_x_wseq > last_wseq_printed) {
		last_wseq_printed =	m_history_x_wseq;
		t = m_history_x[(m_history_x_rseq - 1) & M_HIST_MASK].time -
			m_history_x[(m_history_x_rseq - 2) & M_HIST_MASK].time;

		if (t >	0.0001) {
			Print_flags[Print_current] |= PR_TR_SKIP;
			return 1/t;
		}
	}
	return 0;
}

void IN_SMouseRead (int *mx, int *my) {
	static int acc_x, acc_y;
	int	x = 0, y = 0;
	double t1, t2, maxtime, mintime;
	int mouserate;

	// acquire device
	IDirectInputDevice_Acquire(g_pMouse);
	dinput_acquired	= true;

	// gather data from	last read seq to now
	for ( ; m_history_x_rseq < m_history_x_wseq; m_history_x_rseq++)
		x += m_history_x[m_history_x_rseq&M_HIST_MASK].data;
	for ( ; m_history_y_rseq < m_history_y_wseq; m_history_y_rseq++)
		y += m_history_y[m_history_y_rseq&M_HIST_MASK].data;

	x -= acc_x;
	y -= acc_y;

	acc_x = acc_y = 0;

	// show rate if requested
	if (m_showrate.value && (mouserate = IN_GetMouseRate()))
		Com_Printf("mouse rate: %4d\n", mouserate);

	// smoothing goes here
	mintime = maxtime =	1.0	/ max(m_rate.value,	10);
	maxtime *= 1.2;
	mintime *= 0.7;

	// X axis
	t1 = m_history_x[(m_history_x_rseq - 2) & M_HIST_MASK].time;
	t2 = m_history_x[(m_history_x_rseq - 1) & M_HIST_MASK].time;

	if (t2 - t1 > mintime  &&  t2 - t1 < maxtime) {
		double vel = m_history_x[(m_history_x_rseq - 1) & M_HIST_MASK].data / (t2 - t1);

		t1 = t2;
		t2 = Sys_DoubleTime();

		if (t2 - t1 < maxtime)
			acc_x = vel * (t2 - t1);
	}

	// Y axis
	t1 = m_history_y[(m_history_y_rseq - 2) & M_HIST_MASK].time;
	t2 = m_history_y[(m_history_y_rseq - 1) & M_HIST_MASK].time;

	if (t2 - t1	> mintime  &&  t2 - t1 < maxtime) {
		double vel = m_history_y[(m_history_y_rseq-1) & M_HIST_MASK].data / (t2 - t1);

		t1 = t2;
		t2 = Sys_DoubleTime();

		if	(t2 - t1 < maxtime)
			acc_y = vel * (t2 - t1);
	}

	x += acc_x;
	y += acc_y;

	// return data
	*mx = x;
	*my = y;

	// serve wheel
//	bound(0, wheel_dn_count, 10);
//	bound(0, wheel_up_count, 10);

	while (wheel_dn_count >	0) {
		Key_Event(K_MWHEELDOWN,	true);
		Key_Event(K_MWHEELDOWN,	false);
		wheel_dn_count--;
	}
	while (wheel_up_count >	0) {
		Key_Event(K_MWHEELUP, true);
		Key_Event(K_MWHEELUP, false);
		wheel_up_count--;
	}
}

void IN_SMouseShutdown(void)
{
	double start = Sys_DoubleTime();

	if (!use_m_smooth)
		return;

	use_m_smooth = false; // signaling to thread, time to die

	//
	// wait thread termination
	//
	while( smooth_thread ) {
    	DWORD exitCode;

    	if(!GetExitCodeThread(smooth_thread, &exitCode))
			Sys_Error("IN_SMouseShutdown: GetExitCodeThread: failed");

		if (exitCode != STILL_ACTIVE) {
			// ok, thread terminated

			// Terminating a thread does not necessarily remove the thread object from the operating system.
			// A thread object is deleted when the last handle to the thread is closed. 
 			CloseHandle(smooth_thread);
			smooth_thread = NULL;
			break;
		}

		if (Sys_DoubleTime() - start > 5)
			Sys_Error("IN_SMouseShutdown: thread does't respond");

		Sys_MSleep(1); // sleep a bit, may be that help thread die fast
	}

	if (m_event) {
		CloseHandle(m_event); // close event
		m_event = NULL;
	}
}

void IN_SMouseInit(void) {
	HRESULT	res;
	DWORD threadid; // we does't use it, so its not global

	IN_SMouseShutdown();

	if (!in_m_smooth.integer)
		return;

	// create event	object
	m_event	= CreateEvent(
					NULL,		  // NULL secutity	attributes
					FALSE,		  // automatic reset
					FALSE,		  // initial state = nonsignaled
					NULL);		  // NULL name

	if (m_event	== NULL)
		return;

	// enable di notification
	if ((res = IDirectInputDevice_SetEventNotification(g_pMouse, m_event)) != DI_OK	 &&	 res !=	DI_POLLEDDEVICE)
		return;

	use_m_smooth = true; // better set it to true ASAP, because it used in IN_SMouseProc()

	smooth_thread = CreateThread (
    	NULL,               // pointer to security attributes
    	0,                  // initial thread stack size
    	IN_SMouseProc,      // pointer to thread function
    	NULL,               // argument for new thread
    	CREATE_SUSPENDED,   // creation flags
    	&threadid);         // pointer to receive thread ID

	if (!smooth_thread) {
		Com_Printf("IN_SMouseInit: CreateThread: failed\n");

		CloseHandle(m_event); // close event
		m_event = NULL;

		use_m_smooth = false;
		return;
	}

	SetThreadPriority(smooth_thread, THREAD_PRIORITY_HIGHEST);
	ResumeThread(smooth_thread); // wake up thread
}
#else
int IN_GetMouseRate(void) {
	return -1;
}
#endif

typedef void (*MW_DllFunc1)(void);
typedef int (*MW_DllFunc2)(HWND);

MW_DllFunc1 DLL_MW_RemoveHook = NULL;
MW_DllFunc2 DLL_MW_SetHook = NULL;
qbool MW_Hook_enabled = false;
HINSTANCE mw_hDLL;

static long mw_old_buttons = 0;

static void MW_Set_Hook (void) {
	if (MW_Hook_enabled) {
		Com_Printf("MouseWare hook already loaded\n");
		return;
	}

	mw_old_buttons = 0;

	if (!(mw_hDLL = LoadLibrary("mw_hook.dll"))) {
		Com_Printf("Couldn't find mw_hook.dll\n");
		return;
	}
	DLL_MW_RemoveHook = (MW_DllFunc1) GetProcAddress(mw_hDLL, "MW_RemoveHook");
	DLL_MW_SetHook = (MW_DllFunc2) GetProcAddress(mw_hDLL, "MW_SetHook");
	if (!DLL_MW_SetHook || !DLL_MW_RemoveHook) {
		Com_Printf("Error initializing MouseWare hook\n");
		FreeLibrary(mw_hDLL);
		return;
	}
	if (!DLL_MW_SetHook(mainwindow)) {
		Com_Printf("Couldn't initialize MouseWare hook\n");
		FreeLibrary(mw_hDLL);
		return;
	}

	MW_Hook_enabled = true;
	Com_Printf_State (PRINT_OK, "MouseWare hook initialized\n");
}

static void MW_Remove_Hook (void) {
	if (MW_Hook_enabled) {
		DLL_MW_RemoveHook();
		FreeLibrary(mw_hDLL);
		MW_Hook_enabled = false;
		Com_Printf("MouseWare hook removed\n");
		return;
	}
	Com_Printf("MouseWare hook not loaded\n");
}

static void MW_Shutdown(void) {
	if (!MW_Hook_enabled)
		return;
	MW_Remove_Hook();
}

void MW_Hook_Message (long buttons) {
	int key, flag;
	long changed_buttons;

	buttons &= 0xFFF8;
	changed_buttons = buttons ^ mw_old_buttons;

	for (key = K_MOUSE4, flag = 8; key <= K_MOUSE8; key++, flag <<= 1) {
		if (changed_buttons & flag) {
			Key_Event(key, !!(buttons & flag));
		}
	}

	mw_old_buttons = buttons;
}

void IN_UpdateClipCursor (void) {
	if (mouseinitialized && mouseactive && !dinput)
		ClipCursor (&window_rect);
}

void IN_ShowMouse (void) {

	while (ShowCursor (TRUE) < 0)
		;
}

void IN_HideMouse (void) {

	while (ShowCursor (FALSE) >= 0)
		;
}

qbool IN_InitDInput (void);

void IN_ActivateMouse (void) {
	mouseactivatetoggle = true;

	if (mouseinitialized) {
#if DIRECTINPUT_VERSION	>= 0x0700
		if (dinput) {
			if (g_pMouse) {
				if (!dinput_acquired) {
					HRESULT		hr;

					// we may fail to reacquire if the window has been recreated
					hr = IDirectInputDevice_Acquire(g_pMouse);

					if (FAILED(hr)) {
						if ( !IN_InitDInput() ) {
							Com_Printf ("WARNING: can't reinitializing dinput mouse...\n");
							Com_Printf ("Falling back to Win32 mouse support...\n");
							Cvar_LatchedSetValue( &in_mouse, mt_normal );
							Cbuf_AddText ("in_restart\n");
						}
					}
					else {
						dinput_acquired = true;
					}
				}
			} else {
				return;
			}
		} else
#endif
		{
			if (mouseparmsvalid)
				restore_spi = SystemParametersInfo (SPI_SETMOUSE, 0, newmouseparms, 0);

			SetCursorPos (window_center_x, window_center_y);
			SetCapture (mainwindow);
			ClipCursor (&window_rect);
		}
		mouseactive = true;
	}
}

void IN_SetQuakeMouseState (void) {
	if (mouseactivatetoggle)
		IN_ActivateMouse ();
}

void IN_DeactivateMouse (void) {
	mouseactivatetoggle = false;

	if (mouseinitialized) {
#if DIRECTINPUT_VERSION	>= 0x0700
		if (dinput) {
			if (g_pMouse) {
				if (dinput_acquired) {
					IDirectInputDevice_Unacquire(g_pMouse);
					dinput_acquired = false;
				}
			}
		} else
#endif
		{
			if (restore_spi)
				SystemParametersInfo (SPI_SETMOUSE, 0, originalmouseparms, 0);

			ClipCursor (NULL);
			ReleaseCapture ();
		}

		mouseactive = false;
	}
}

void IN_RestoreOriginalMouseState (void) {
	if (mouseactivatetoggle) {
		IN_DeactivateMouse ();
		mouseactivatetoggle = true;
	}

	// try to redraw the cursor so it gets reinitialized, because sometimes it has garbage after the mode switch
	ShowCursor (TRUE);
	ShowCursor (FALSE);
}

qbool IN_InitDInput (void) {
#if DIRECTINPUT_VERSION	>= 0x0700
    HRESULT hr;
	DIPROPDWORD	dipdw = {
		{
			sizeof(DIPROPDWORD),        // diph.dwSize
			sizeof(DIPROPHEADER),       // diph.dwHeaderSize
			0,                          // diph.dwObj
			DIPH_DEVICE,                // diph.dwHow
		},
		DI_BufSize(),              // dwData
	};
	DIDATAFORMAT qdf = {
		sizeof(DIDATAFORMAT),			// this structure
			sizeof(DIOBJECTDATAFORMAT),		// size of object data format
			DIDF_RELAXIS,					// absolute axis coordinates
			sizeof(MYDATA),					// device data size
			sizeof(rgodf)/sizeof(rgodf[0]),	// number of objects
			rgodf							// and here they are
	};

	if (!hInstDI) {
		hInstDI = LoadLibrary("dinput.dll");
		
		if (hInstDI == NULL) {
			Com_Printf_State(PRINT_FAIL, "Couldn't load dinput.dll\n");
			return false;
		}
	}

	if (!pDirectInputCreateEx) {
		pDirectInputCreateEx = (void *)GetProcAddress(hInstDI,"DirectInputCreateEx");
		if (!pDirectInputCreateEx) {
			Com_Printf_State(PRINT_FAIL, "Couldn't get DI proc addr\n");
			return false;
		}
	}

	// register with DirectInput and get an IDirectInput to play with.
	hr = pDirectInputCreateEx(global_hInstance, DIRECTINPUT_VERSION, &IID_IDirectInput7, (LPVOID *) &g_pdi, NULL);

	if (FAILED(hr))
		return false;

	// obtain an interface to the system mouse device.
	hr = IDirectInput7_CreateDeviceEx(g_pdi, &GUID_SysMouse, &IID_IDirectInputDevice7, (LPVOID *) &g_pMouse, NULL);

	if (FAILED(hr)) {
		Com_Printf_State(PRINT_FAIL, "Couldn't open DI mouse device\n");
		return false;
	}

	// set the data format to "mouse format".
	hr = IDirectInputDevice7_SetDataFormat(g_pMouse, &qdf);

	if (FAILED(hr)) {
		Com_Printf_State(PRINT_FAIL, "Couldn't set DI mouse format\n");
		return false;
	}

	// set the cooperativity level.
	hr = IDirectInputDevice7_SetCooperativeLevel(g_pMouse, mainwindow, DISCL_EXCLUSIVE | DISCL_FOREGROUND);

	if (FAILED(hr)) {
		Com_Printf_State(PRINT_FAIL, "Couldn't set DI coop level\n");
		return false;
	}

	// set the buffer size to DI_BufSize() elements.
	// the buffer size is a DWORD property associated with the device
	hr = IDirectInputDevice7_SetProperty(g_pMouse, DIPROP_BUFFERSIZE, &dipdw.diph);

	if (FAILED(hr)) {
		Com_Printf_State(PRINT_FAIL, "Couldn't set DI buffersize\n");
		return false;
	}


	IN_SMouseInit();


	return true;
#else
	return false;
#endif
}

void IN_StartupMouse (void) {
	if (in_mouse.integer == mt_none)
		return;

	mouseinitialized = true;

	in_mwheeltype = MWHEEL_UNKNOWN;

#if DIRECTINPUT_VERSION	>= 0x0700
	if (in_mouse.integer == mt_dinput) {
		dinput = IN_InitDInput ();

		if (dinput) {
			Com_Printf_State (PRINT_OK, "DirectInput initialized\n");
			mouse_buttons = 8;
			if (use_m_smooth)				
				Com_Printf_State (PRINT_OK, "Mouse smoothing initialized\n");
		} else {
			Com_Printf_State (PRINT_FAIL, "DirectInput not initialized\n");
		}
	}
#endif

	if (!dinput) {
		mouseparmsvalid = SystemParametersInfo (SPI_GETMOUSE, 0, originalmouseparms, 0);

		if (mouseparmsvalid) {
			if (in_m_os_parameters.integer == 1)    // keeps the OS acceleration settings
				newmouseparms[2] = originalmouseparms[2];

			if (in_m_os_parameters.integer == 2) {  // keeps the OS speed settings
				newmouseparms[0] = originalmouseparms[0];
				newmouseparms[1] = originalmouseparms[1];
			}

            if (in_m_os_parameters.integer > 2) {   // keeps both OS acceleration and speed settings
				newmouseparms[0] = originalmouseparms[0];
				newmouseparms[1] = originalmouseparms[1];
				newmouseparms[2] = originalmouseparms[2];
			}
		}
		mouse_buttons = 8;
	}

	if (in_m_mwhook.integer)
		MW_Set_Hook();

	// if a fullscreen video mode was set before the mouse was initialized, set the mouse state appropriately
	if (mouseactivatetoggle)
		IN_ActivateMouse ();
}

//
// we have million variables, reset it to some default values, so each IN_Init() acts same.
// I've set only mouse variables, know nothing about JOYSTICK.
//
void IN_SetGlobals(void) {

    input_initialized = false;

	// reset to default params, like global variables have
	mouse_buttons = mouse_oldbuttonstate = 0;
	mx_accum      = my_accum = 0;
	restore_spi   = false;
	memset(&current_pos,       0, sizeof(current_pos));
	memset(originalmouseparms, 0, sizeof(originalmouseparms));
	memset(newmouseparms,      0, sizeof(newmouseparms));

	mouseparmsvalid  = mouseactivatetoggle = false;
	dinput_acquired  = false;
	mstate_di        = 0;
	uiWheelMessage   = 0;

	mouseactive      = false;
	mouseinitialized = false;

	// Some drivers send DIMOFS_Z, some send WM_MOUSEWHEEL, and some send both.
	// To get the mouse wheel to work in any case but avoid duplicate events,
	// we will only use one event source, wherever we receive the first event.
	in_mwheeltype = MWHEEL_UNKNOWN;

#if DIRECTINPUT_VERSION	>= 0x0700
	use_m_smooth  = false;
	m_event       = NULL;
	smooth_thread = NULL;

	memset(m_history_x, 0, sizeof(m_history_x));	// history
	memset(m_history_y, 0, sizeof(m_history_y));
	m_history_x_wseq =	0;		// write sequence
	m_history_y_wseq =	0;
	m_history_x_rseq =	0;		// read	sequence
	m_history_y_rseq =	0;
	wheel_up_count	 =  0;
	wheel_dn_count	 =  0;

	last_wseq_printed=  0;

	dinput = false;
#endif
}

void IN_Init (void) {
	//
	// reset globals to default
	//
	IN_SetGlobals();

	Cvar_SetCurrentGroup (CVAR_GROUP_INPUT_MOUSE); // mouse variables
// can be changed at any time
	Cvar_Register (&m_rate);
	Cvar_Register (&m_showrate);

	Cvar_Register (&m_filter);

// latched
    Cvar_Register (&in_mouse);
    Cvar_Register (&in_m_smooth);
    Cvar_Register (&in_m_mwhook);
    Cvar_Register (&in_m_os_parameters);
    Cvar_Register (&in_di_bufsize);

    if (!host_initialized)
    {
		void IN_Restart_f(void);

		if (COM_CheckParm("-dinput"))
			Cvar_LatchedSetValue (&in_mouse, mt_dinput);

		if (COM_CheckParm ("-nomouse"))
			Cvar_LatchedSetValue (&in_mouse, mt_none);

		if (COM_CheckParm("-m_smooth"))
			Cvar_LatchedSetValue (&in_m_smooth, 1);

		if (COM_CheckParm ("-m_mwhook"))
			Cvar_LatchedSetValue (&in_m_mwhook, 1);

		if (COM_CheckParm ("-noforcemspd")) 
			Cvar_LatchedSetValue (&in_m_os_parameters, 1);

		if (COM_CheckParm ("-noforcemaccel"))
			Cvar_LatchedSetValue (&in_m_os_parameters, 2);

        if (COM_CheckParm ("-noforcemparms"))
			Cvar_LatchedSetValue (&in_m_os_parameters, 3);

		Cvar_SetCurrentGroup (CVAR_GROUP_INPUT_KEYBOARD); // keyboard variables
		Cvar_Register (&cl_keypad);
    
		Cvar_SetCurrentGroup (CVAR_GROUP_INPUT_JOY); // joystick variables
		Cvar_Register (&in_joystick);

		Cmd_AddCommand ("in_restart", IN_Restart_f);
	}

	Cvar_ResetCurrentGroup ();

	switch(in_mouse.integer) {
		case mt_none:
			break;

		case mt_normal:
			if (in_m_smooth.integer)
				Com_Printf ("%s will be ignored with non dinput mouse\n", in_m_smooth.name);
			break;
				
		case mt_dinput:
#if DIRECTINPUT_VERSION	>= 0x0700
			if (in_m_os_parameters.integer)
				Com_Printf ("%s will be ignored with dinput mouse\n", in_m_os_parameters.name);
			break;
#else
			Com_Printf ("DirectInput not supported in this build\n");
			Cvar_LatchedSetValue (&in_mouse, mt_normal);
			break;
#endif
		default:
			Com_Printf("Unknow value %d of %s, using normal mouse\n", in_mouse.integer, in_mouse.name);
			Cvar_LatchedSetValue (&in_mouse, mt_normal);
			break;
	}

#ifdef WITH_KEYMAP
	if (!host_initialized)
		IN_StartupKeymap();
#endif // WITH_KEYMAP

	uiWheelMessage = RegisterWindowMessage ("MSWHEEL_ROLLMSG");

	IN_StartupMouse ();
	IN_StartupJoystick ();

    input_initialized = true;
}

void IN_Shutdown (void) {
	IN_DeactivateMouse ();
	IN_ShowMouse ();

#if DIRECTINPUT_VERSION	>= 0x0700

	IN_SMouseShutdown();

    if (g_pMouse) {
		IDirectInputDevice_Release(g_pMouse);
		g_pMouse = NULL;
	}

    if (g_pdi) {
		IDirectInput_Release(g_pdi);
		g_pdi = NULL;
	}

	MW_Shutdown(); // mouseware hook

	if (hInstDI) {
		FreeLibrary(hInstDI);
		hInstDI = NULL;
	}

#else

	MW_Shutdown(); // mouseware hook

#endif

	//
	// reset globals to default
	//
	IN_SetGlobals();

    input_initialized = false;
}

void IN_Restart_f(void)
{
	if (!host_initialized) { // sanity
		Com_Printf("Can't do %s yet\n", Cmd_Argv(0));
		return;
	}

	IN_Shutdown();
	IN_Init();
}

void IN_MouseEvent (int mstate) 
{
	int i;

	if (mouseactive && !dinput) 
	{
		// perform button actions
		for (i = 0; i < mouse_buttons; i++) 
		{
			if ((mstate & (1 << i)) && !(mouse_oldbuttonstate & (1 << i)))
				Key_Event (K_MOUSE1 + i, true);

			if (!(mstate & (1 << i)) && (mouse_oldbuttonstate & (1 << i)))
				Key_Event (K_MOUSE1 + i, false);
		}
		mouse_oldbuttonstate = mstate;
	}
}

float mouse_x, mouse_y;

void IN_MouseMove (usercmd_t *cmd) {
	static int old_mouse_x = 0, old_mouse_y = 0;
	int mx, my;
#if DIRECTINPUT_VERSION	>= 0x0700
	int i, value;
	DIDEVICEOBJECTDATA od;
	DWORD dwElements;
	HRESULT hr;
#endif

	if (!mouseactive)
		return;

#if DIRECTINPUT_VERSION	>= 0x0700
	if (dinput) {
		mx = my = 0;

		if (use_m_smooth) {
			IN_SMouseRead(&mx, &my);
		} else {
			while (1) {
				dwElements = 1;

				hr = IDirectInputDevice_GetDeviceData(g_pMouse, sizeof(DIDEVICEOBJECTDATA), &od, &dwElements, 0);

				if ((hr == DIERR_INPUTLOST) || (hr == DIERR_NOTACQUIRED)) {
					dinput_acquired = true;
					IDirectInputDevice_Acquire(g_pMouse);
					break;
				}

				if(hr == DI_BUFFEROVERFLOW)
					SetBufferSize();

				/* Unable to read data or no data available */
				if (FAILED(hr) || !dwElements)
					break;

				/* Look at the element to see what happened */
				switch (od.dwOfs) {
					case DIMOFS_X:
						mx += od.dwData;
						break;

					case DIMOFS_Y:
						my += od.dwData;
						break;

					INPUT_CASE_DINPUT_MOUSE_BUTTONS

					
					case DIMOFS_Z:
						if (in_mwheeltype != MWHEEL_WINDOWMSG)
						{
							in_mwheeltype = MWHEEL_DINPUT;
							value = od.dwData;

							if (value > 0) {
								Key_Event(K_MWHEELUP, true);
								Key_Event(K_MWHEELUP, false);
							} else if (value < 0) {
								Key_Event(K_MWHEELDOWN, true);
								Key_Event(K_MWHEELDOWN, false);
							} else {
								; // value == 0
							}
						}
						break;
				
				}
			}
		}
		// perform button actions
		for (i = 0; i < mouse_buttons; i++) {
			if ( (mstate_di & (1 << i)) && !(mouse_oldbuttonstate & (1 << i)) )
				Key_Event (K_MOUSE1 + i, true);

			if ( !(mstate_di & (1 << i)) && (mouse_oldbuttonstate & (1 << i)) )
				Key_Event (K_MOUSE1 + i, false);
		}
		mouse_oldbuttonstate = mstate_di;
	} 
	else
#endif
	{
		GetCursorPos (&current_pos);
		mx = current_pos.x - window_center_x + mx_accum;
		my = current_pos.y - window_center_y + my_accum;
		mx_accum = my_accum = 0;
	}

	//
	// Do not move the player if we're in HUD editor or menu mode. 
	// And don't apply ingame sensitivity, since that will make movements jerky.
	//
	if(key_dest == key_hudeditor || key_dest == key_menu || key_dest == key_demo_controls)
	{
		old_mouse_x = mouse_x = mx * cursor_sensitivity.value;
		old_mouse_y = mouse_y = my * cursor_sensitivity.value;
	}
	else
	{
		// Normal game mode.

		if (m_filter.value) 
		{
			float filterfrac = bound(0, m_filter.value, 1) / 2.0;
			mouse_x = (mx * (1 - filterfrac) + old_mouse_x * filterfrac);
			mouse_y = (my * (1 - filterfrac) + old_mouse_y * filterfrac);
		} 
		else 
		{
			mouse_x = mx;
			mouse_y = my;
		}

		old_mouse_x = mx;
		old_mouse_y = my;

		if (m_accel.value) 
		{
			float mousespeed = (sqrt (mx * mx + my * my)) / (1000.0f * (float)cls.trueframetime);
			mouse_x *= (mousespeed * m_accel.value + sensitivity.value);
			mouse_y *= (mousespeed * m_accel.value + sensitivity.value);
		} 
		else
		{
			mouse_x *= sensitivity.value;
			mouse_y *= sensitivity.value;
		}

		// add mouse X/Y movement to cmd
		if ( (in_strafe.state & 1) || (lookstrafe.value && mlook_active))
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

	// if the mouse has moved, force it to the center, so there's room to move
	if (mx || my)
	{
		SetCursorPos (window_center_x, window_center_y);
	}
}

void IN_Move (usercmd_t *cmd) {
	if (ActiveApp && !Minimized) {
		IN_MouseMove (cmd);
		IN_JoyMove (cmd);
	}
}
void IN_Accumulate (void) {
	if (mouseactive) {
		GetCursorPos (&current_pos);

		// Cursor free while not ingame
		if (key_dest == key_game || key_dest == key_hudeditor || key_dest == key_menu || key_dest == key_demo_controls)
		{
			mx_accum += current_pos.x - window_center_x;
			my_accum += current_pos.y - window_center_y;
		}
		else
		{
			mx_accum = 0;
			my_accum = 0;
		}

		// force the mouse to the center, so there's room to move
		if (key_dest == key_game || key_dest == key_hudeditor || key_dest == key_menu || key_dest == key_demo_controls)
			SetCursorPos (window_center_x, window_center_y);
		// Avoid center with no-game mode
	}
}


void IN_ClearStates (void) {
	if (mouseactive)
		mx_accum = my_accum = mouse_oldbuttonstate = 0;
}
 
void IN_StartupJoystick (void) { 
	int numdevs;
	JOYCAPS jc;
	MMRESULT mmr;
 
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
	
	// verify joystick driver is present
	if ((numdevs = joyGetNumDevs ()) == 0) {
		Com_Printf ("\njoystick not found -- driver not present\n\n");
		return;
	}

	// cycle through the joystick ids for the first valid one
	for (joy_id = 0; joy_id < numdevs; joy_id++) {
		memset (&ji, 0, sizeof(ji));
		ji.dwSize = sizeof(ji);
		ji.dwFlags = JOY_RETURNCENTERED;

		if ((mmr = joyGetPosEx (joy_id, &ji)) == JOYERR_NOERROR)
			break;
	}

	// abort startup if we didn't find a valid joystick
	if (mmr != JOYERR_NOERROR) {
		Com_DPrintf ("\njoystick not found -- no valid joysticks (%x)\n\n", mmr);
		return;
	}

	// get the capabilities of the selected joystick
	// abort startup if command fails
	memset (&jc, 0, sizeof(jc));
	if ((mmr = joyGetDevCaps (joy_id, &jc, sizeof(jc))) != JOYERR_NOERROR) {
		Com_Printf ("\njoystick not found -- invalid joystick capabilities (%x)\n\n", mmr); 
		return;
	}

	// save the joystick's number of buttons and POV status
	joy_numbuttons = jc.wNumButtons;
	joy_haspov = jc.wCaps & JOYCAPS_HASPOV;

	// old button and POV states default to no buttons pressed
	joy_oldbuttonstate = joy_oldpovstate = 0;

	// mark the joystick as available and advanced initialization not completed
	// this is needed as cvars are not available during initialization
	joy_avail = true; 
	joy_advancedinit = false;

	Com_Printf ("\njoystick detected\n\n"); 
}

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

void Joy_AdvancedUpdate_f (void) {
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
}

void IN_Commands (void) {
	int i, key_index;
	DWORD buttonstate, povstate;

	if (!joy_avail)
		return;
	
	// loop through the joystick buttons
	// key a joystick event or auxillary event for higher number buttons for each state change
	buttonstate = ji.dwButtons;
	for (i = 0; i < joy_numbuttons; i++) {
		if ((buttonstate & (1<<i)) && !(joy_oldbuttonstate & (1 << i))) {
			key_index = (i < 4) ? K_JOY1 : K_AUX1;
			Key_Event (key_index + i, true);
		}

		if (!(buttonstate & (1 << i)) && (joy_oldbuttonstate & (1 << i))) {
			key_index = (i < 4) ? K_JOY1 : K_AUX1;
			Key_Event (key_index + i, false);
		}
	}
	joy_oldbuttonstate = buttonstate;

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
}

qbool IN_ReadJoystick (void) {
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
		// turning off the joystick seems too harsh for 1 read error,\
		// but what should be done?
		// Com_Printf ("IN_ReadJoystick: no response\n");
		// joy_avail = false;
		return false;
	}
}

void IN_JoyMove (usercmd_t *cmd) {
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
}

//==========================================================================

#ifdef WITH_KEYMAP
#else
static byte scantokey[128] = {
//  0       1        2       3       4       5       6       7
//  8       9        A       B       C       D       E       F
	0  ,   K_ESCAPE,'1',    '2',    '3',    '4',    '5',    '6',
	'7',    '8',    '9',    '0',    '-',    '=',    K_BACKSPACE, 9,   // 0
	'q',    'w',    'e',    'r',    't',    'y',    'u',    'i',
	'o',    'p',    '[',    ']',    K_ENTER,K_LCTRL, 'a',    's',      // 1
	'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';',
	'\'',   '`',    K_LSHIFT,'\\',   'z',    'x',    'c',    'v',      // 2
	'b',    'n',    'm',    ',',    '.',    '/',    K_RSHIFT,KP_STAR,
	K_LALT,  ' ',  K_CAPSLOCK,K_F1,  K_F2,   K_F3,   K_F4,   K_F5,     // 3
	K_F6,   K_F7,   K_F8,   K_F9,   K_F10,  K_PAUSE,K_SCRLCK,K_HOME,
	K_UPARROW,K_PGUP,KP_MINUS,K_LEFTARROW,KP_5,K_RIGHTARROW,KP_PLUS,K_END, // 4
	K_DOWNARROW,K_PGDN,K_INS,K_DEL, 0,      0,      0,      K_F11,
	K_F12,  0,      0,      K_LWIN, K_RWIN, K_MENU, 0,      0,         // 5
	0,      0,      0,      0,      0,      0,      0,      0,
	0,      0,      0,      0,      0,      0,      0,      0,
	0,      0,      0,      0,      0,      0,      0,      0,
	0,      0,      0,      0,      0,      0,      0,      0
};

//Map from windows to quake keynums
int IN_MapKey (int key) {
	int extended;

	extended = (key >> 24) & 1;

	key = (key >> 16) & 255;
	if (key > 127)
		return 0;

	key = scantokey[key];

	if (cl_keypad.value) {
		if (extended) {
			switch (key) {
				case K_ENTER:		return KP_ENTER;
				case '/':			return KP_SLASH;
				case K_PAUSE:		return KP_NUMLOCK;
				case K_LALT:		return K_RALT;
				case K_LCTRL:		return K_RCTRL;
			};
		} else {
			switch (key) {
				case K_HOME:		return KP_HOME;
				case K_UPARROW:		return KP_UPARROW;
				case K_PGUP:		return KP_PGUP;
				case K_LEFTARROW:	return KP_LEFTARROW;
				case K_RIGHTARROW:	return KP_RIGHTARROW;
				case K_END:			return KP_END;
				case K_DOWNARROW:	return KP_DOWNARROW;
				case K_PGDN:		return KP_PGDN;
				case K_INS:			return KP_INS;
				case K_DEL:			return KP_DEL;
			}
		}
	} else {
		// cl_keypad 0, compatibility mode
		switch (key) {
			case KP_STAR:	return '*';
			case KP_MINUS:	return '-';
			case KP_5:		return '5';
			case KP_PLUS:	return '+';
		}
	}

	return key;
}
#endif // WITH_KEYMAP else

int isAltDown(void)
{
    if (GetKeyState(VK_MENU) < 0)
        return 1;
    return 0;
//    extern qbool    keydown[256];
//    return keydown[K_ALT] || keydown[K_RALT];
}
int isCtrlDown(void)
{
    if (GetKeyState(VK_CONTROL) < 0)
        return 1;
    return 0;
//    extern qbool    keydown[256];
//    return keydown[K_CTRL] || keydown[K_RCTRL];
}
int isShiftDown(void)
{
    if (GetKeyState(VK_SHIFT) < 0)
        return 1;
    return 0;
//    extern qbool    keydown[256];
//    return keydown[K_SHIFT] || keydown[K_RSHIFT];
}
