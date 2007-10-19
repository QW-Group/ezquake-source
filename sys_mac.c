/*

	$Id: sys_mac.c,v 1.30 2007-10-19 18:14:16 zwoch Exp $

*/
// sys_mac.c -- Macintosh system driver

#define __OPENTRANSPORTPROVIDERS__

#include "quakedef.h"
#include "keys.h"

#include <pthread.h>
#include <semaphore.h>

#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <Carbon/Carbon.h>
#include <AGL/agl.h>
#include <Gestalt.h>
#include <DrawSprocket/DrawSprocket.h>
#include <CFString.h>

#include "mac.h"
#include "mac_prefs.h"

#include "gl_texture.h"
#include <stdio.h>

char *kAppName = "ezQuake";

cvar_t	sys_yieldcpu = {"sys_yieldcpu", "0"};

// The following are needed for activate/deactivate
extern qbool video_restart;
extern AGLContext gContext;
void MacSetupScreen (void);
void MacShutdownScreen (void);
GLboolean _aglSetDrawableMonitor (AGLContext inContext, Boolean inWindow);
void Release_Cursor_f (void);
void Capture_Cursor_f (void);
void VID_SetConWidth (void);

qbool forceOptions = false;
qbool background = false;

static enum { QUIT, START, FAILED } action = START;

enum {
	// Dialog resources
	rAlertDialog = 128,

	// GetKeys character codes
	kCommandKey = 0x37,
	kOptionKey = 0x3a,
	kLShiftKey = 0x38,
	kRShiftKey = 0x3C
};

// Macintosh keymap
byte scantokey[128] =
{
//	 0		 1		 2		 3			 4		 5			 6		 	7
//	 8		 9		 A		 B			 C		 D			 E			F
	'=',	'9',	'7',	'-',		'8',	'0',		']',		'o',
	'y',	't',	'1',	'2',		'3',	'4',		'6',		'5',		// 0
	'c',	'v',	K_PARA,	'b',		'q',	'w',		'e',		'r',
	'a',	's',	'd',	'f',		'h',	'g',		'z',		'x',		// 1
	K_SHIFT,K_CAPSLOCK,K_ALT,K_CTRL,	0,		0,			0,			0,
	K_TAB,	K_SPACE,'`',	K_BACKSPACE,0,		K_ESCAPE,	0,			K_CMD,		// 2
	'k',	';',	'\\',	',',		'/',	'n',		'm',		'.',
	'u',	'[',	'i',	'p',		 K_ENTER,'l',		'j',		'\'',		// 3
	KP_6,	KP_7,	0,		KP_8,		KP_9,	0,			0,			0,
	0,		KP_EQUAL,KP_0,	KP_1,		KP_2,	KP_3,		KP_4,		KP_5,		// 4
	0,		0,		0,		KP_SLASH,	KP_ENTER,0,			KP_MINUS,	0,
	0,		KP_DOT,	0,		KP_STAR,	0,		KP_PLUS,	0,			KP_NUMLOCK,	// 5
	K_F2,	K_PGDN,	K_F1,	K_LEFTARROW,K_RIGHTARROW,K_DOWNARROW,K_UPARROW,	0,
	0,		K_F15,	K_INS,	K_HOME,		K_PGUP,	K_DEL,		K_F4,		K_END,		// 6
	0,		K_F13,	0,		K_F14,		0,		K_F10,		0,			K_F12,
	K_F5,	K_F6,	K_F7,	K_F3,		K_F8,	K_F9,		0,			K_F11,		// 7
};

// Local prototypes
void SetDefaultDirectory (void);
void InitializeAppleEvents (void);
OSErr HandleQuitApplicationEvent(const AppleEvent *theAppleEvent, AppleEvent *reply, UInt32 handlerRefcon);
void VerifySystemSpecs (void);
void HandleEvents (void);
static void Sys_Deactivate (void);
static void Sys_Activate (void);

//	Initialize everything for the program, make sure we can run

int noconinput = 0;

qbool stdin_ready;
int do_stdin = 1;

cvar_t sys_nostdout = {"sys_nostdout", "0"};
cvar_t sys_extrasleep = {"sys_extrasleep", "0"};
/*
===============================================================================
FILE IO
===============================================================================
*/

// filelength
int filelength (int h)
{
	off_t	pos;
	off_t	end;

	pos = lseek (h, 0, SEEK_SET);
	end = lseek (h, 0, SEEK_END);
	lseek (h, pos, SEEK_SET);

	return end;
}

int Sys_FileOpenRead (char *path, int *hndl)
{
	int h;

	h = open (path, O_RDONLY, 0666);

	*hndl = h;
	if (h == -1)
		return -1;

	return filelength(h);
}

int Sys_FileOpenWrite (char *path)
{
	int     handle;

	umask (0);

	handle = open(path,O_RDWR | O_CREAT | O_TRUNC
	, 0666);

	if (handle == -1)
		Sys_Error ("Error opening %s: %s", path,strerror(errno));

	return handle;
}

void Sys_FileClose (int handle)
{
	close (handle);
}

void Sys_FileSeek (int handle, int position)
{
	lseek (handle, position, SEEK_SET);
}

int Sys_FileRead (int handle, void *dest, int count)
{
    return read (handle, dest, count);
}

int Sys_FileWrite (int handle, void *src, int count)
{
	return write (handle, src, count);
}

// Sys_FileExists
qbool Sys_FileExists (char *path)
{
	struct	stat	buf;

	if (stat (path,&buf) == -1)
		return false;

	return true;
}

void Sys_mkdir (const char *path)
{
	mkdir(path, 0777);
}

#ifdef WITH_FTE_VFS
int Sys_EnumerateFiles (char *gpath, char *match, int (*func)(char *, int, void *), void *parm) {
	DIR *dir, *dir2;
	char apath[MAX_OSPATH];
	char file[MAX_OSPATH];
	char truepath[MAX_OSPATH];
	char *s;
	struct dirent *ent;

	//printf("path = %s\n", gpath);
	//printf("match = %s\n", match);

	if (!gpath)
		gpath = "";
	*apath = '\0';

	strncpy(apath, match, sizeof(apath));
	for (s = apath+strlen(apath)-1; s >= apath; s--)
	{
		if (*s == '/')
		{
			s[1] = '\0';
			match += s - apath+1;
			break;
		}
	}
	if (s < apath)  //didn't find a '/'
		*apath = '\0';

	snprintf(truepath, sizeof(truepath), "%s/%s", gpath, apath);


	//printf("truepath = %s\n", truepath);
	//printf("gamepath = %s\n", gpath);
	//printf("apppath = %s\n", apath);
	//printf("match = %s\n", match);
	dir = opendir(truepath);
	if (!dir)
	{
		Com_DPrintf("Failed to open dir %s\n", truepath);
		return true;
	}
	do
	{
		ent = readdir(dir);
		if (!ent)
			break;
		if (*ent->d_name != '.')
			if (wildcmp(match, ent->d_name))
			{
				snprintf(file, sizeof(file), "%s/%s", gpath, ent->d_name);
				//would use stat, but it breaks on fat32.

				if ((dir2 = opendir(file)))
				{
					closedir(dir2);
					snprintf(file, sizeof(file), "%s%s/", apath, ent->d_name);
					//printf("is directory = %s\n", file);
				}
				else
				{
					snprintf(file, sizeof(file), "%s%s", apath, ent->d_name);
					//printf("file = %s\n", file);
				}

				if (!func(file, -2, parm))
				{
					closedir(dir);
					return false;
				}
			}
	} while(1);
	closedir(dir);

	return true;
}
#endif

/*
===============================================================================
SYSTEM IO
===============================================================================
*/

void Sys_Init(void)
{
	if (dedicated) {
		Cvar_Register (&sys_nostdout);
		Cvar_Register (&sys_extrasleep);
	}
}

void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length){}

void Sys_Error (char *error, ...)
{
	va_list argptr;
	char outTxt[2048];

	va_start (argptr, error);
	outTxt[0] = vsprintf (outTxt+1, error, argptr);
	va_end (argptr);
	Host_Shutdown ();
#ifdef NDEBUG
	if (!dedicated)
	{
		AlertStdAlertParamRec param;
		short itemHit;
		Str255 briefMsg;

		param.movable 		= 0;
		param.filterProc 	= nil;
		param.defaultText	= nil;
		param.cancelText 	= nil;
		param.otherText 	= nil;
		param.helpButton 	= false;
		param.defaultButton	= kAlertStdAlertOKButton;
		param.cancelButton	= 0;
		param.position		= kWindowDefaultPosition;

		sprintf ((char *)briefMsg, "%s has encountered an error.", kAppName);
		c2pstrcpy (briefMsg, (char *)briefMsg);

		StandardAlert (kAlertStopAlert, briefMsg, (StringPtr) outTxt, &param, &itemHit);
	}
	else
#endif
	{
	printf ("Fatal error: %s\n",outTxt);
	}
	exit (1);
}

// Non-fatal message box
void Sys_Message (Str255 briefMsg, char *error, ...)
{
	va_list argptr;
	char outTxt[2048];

	va_start (argptr, error);
	outTxt[0] = vsprintf (outTxt+1, error, argptr);
	va_end (argptr);

	{
		AlertStdAlertParamRec param;
		short itemHit;

		param.movable 		= 0;
		param.filterProc 	= NULL;
		param.defaultText	= kAlertDefaultOKText;
		param.cancelText 	= NULL;
		param.otherText 	= NULL;
		param.helpButton 	= false;
		param.defaultButton	= kAlertStdAlertOKButton;
		param.cancelButton	= 0;
		param.position		= kWindowDefaultPosition;

		c2pstrcpy (briefMsg, (char *)briefMsg);
		StandardAlert (kAlertNoteAlert, briefMsg, (StringPtr) outTxt, &param, &itemHit);
	}
}


void Sys_Printf (char *fmt, ...)
{
	va_list         argptr;

#ifdef NDEBUG
	if (!dedicated)
		return;
#endif

	va_start (argptr,fmt);
	vprintf (fmt,argptr);
	va_end (argptr);
}

void Sys_Quit (void)
{
	// Save out our prefs
	SavePrefs ();
	Host_Shutdown();
	FlushEvents(everyEvent, 0);
	exit (0);
}

double Sys_DoubleTime (void)
{
	static union {
		long long big;
		UnsignedWide system;
	} then,now;
	Microseconds(&now.system);

	then.big=now.big;

	return now.big / 1000000.0;
}

#ifndef id386
void Sys_HighFPPrecision (void){}

void Sys_LowFPPrecision (void){}
#endif

void Sys_DebugLog(char *file, char *fmt, ...)
{
    va_list argptr;
    static char data[1024];
    int fd;

    va_start(argptr, fmt);
    vsprintf(data, fmt, argptr);
    va_end(argptr);

    fd = open(file, O_WRONLY | O_CREAT | O_APPEND, 0666);
    write(fd, data, strlen(data));
    close(fd);
}

char *Sys_ConsoleInput (void) {
	static char text[256];
	int len;

	if (!dedicated)
		return NULL;

	if (!stdin_ready || !do_stdin)
		return NULL; // the select didn't say it was ready
	stdin_ready = false;

	len = read (0, text, sizeof(text));
	if (len == 0) { // end of file
		do_stdin = 0;
		return NULL;
	}
	if (len < 1)
		return NULL;
	text[len - 1] = 0; // rip off the /n and terminate

	return text;
}

void Sys_Sleep (void){}

/*	pOx - We always poll keys, even in carbon because:
	a). It works, and we could care less how many cycles it wastes when we're the front process
	b). A lot of work has gone into Key_Event to get key repeats working, and I don't want to re-do that!
*/
void Sys_SendKeyEvents (void)
{
	extern cvar_t	cl_keypad;
	static KeyMap 	oldKeyMap;
	KeyMap		newKeyMap;
	int		i, n, key;
	Boolean		newBit;
	Boolean		oldBit;

	if (background) return;

	GetKeys(newKeyMap);

	// Don't repeat in-game (just send key that have changed)
	for (i=0;i<4;i++)
	{
		// if the keymap has changed then generate an event
#ifdef __BIG_ENDIAN__
		if (oldKeyMap[i] != newKeyMap[i])
#else
		if (oldKeyMap[i].bigEndianValue != newKeyMap[i].bigEndianValue)
#endif
		{
			// if a bit is different, the key state has changed

			for (n=0;n<32;n++)
			{
#ifdef __BIG_ENDIAN__
				newBit = ((newKeyMap[i] >> n) & 1);
				oldBit = ((oldKeyMap[i] >> n) & 1);
#else
				newBit = ((BigLong(newKeyMap[i].bigEndianValue) >> n) & 1);
				oldBit = ((BigLong(oldKeyMap[i].bigEndianValue) >> n) & 1);
#endif

				if (newBit != oldBit)
				{
					key = scantokey[i*32 + n];
					if (key==0)
					{
						Com_Printf("Unrecognised key %d\n", i*32+n);
						continue;
					}
					if (!cl_keypad.value)
					{
						switch (key)
						{
						case KP_PLUS:	key = '+'; break;
						case KP_MINUS:	key = '-'; break;
						case KP_SLASH:	key = '/'; break;
						case KP_ENTER:	key = K_ENTER; break;
						default:		break;
						}
					}
					if (newBit == 1)
						Key_Event(key, true);	// key down
					else
						Key_Event(key, false);	// key up
				}
			}
		}
	}

	// save keymap to compare next time
	memcpy(oldKeyMap, newKeyMap, sizeof(KeyMap));
}

extern Point glWindowPos;

#define keyEvents (keyDownMask|keyUpMask|autoKeyMask)
#define notKeyEvents (everyEvent & ~keyEvents)

void OnMouseButton (EventRecord *myEvent, qbool down)
{
	short	    		part;
	WindowRef		window;
	Rect 			r;
	GrafPtr			origPort;
	extern WindowRef 	glWindow;
	extern qbool 	suspend_mouse;

	part = FindWindow(myEvent->where, &window);

	switch(part)
	{
		case inDrag:
	        if(!window) return;

	        if (suspend_mouse)
			{
		        GetRegionBounds(GetGrayRgn(), &r);
		        GetPort(&origPort);
		        SetPortWindowPort(window);
		        DragWindow(window, myEvent->where, &r);
		        if (window == glWindow)
		        {
				aglUpdateContext(gContext);
		        	// Update the global window position (saved in prefs)
		        	GetWindowPortBounds(window, &r);
		        	glWindowPos.v = r.top;
		        	glWindowPos.h = r.left;
		        	LocalToGlobal(&glWindowPos);
		        }
		        SetPort(origPort);
			}
			break;
		case inGoAway:
			break;
		case inGrow:
			break;
		case inZoomIn:
		case inZoomOut:
			break;
		case inContent:
			if(!window) return;
			if(window != FrontWindow())
				SelectWindow(window);
			/*else
			{
				GrafPtr	origPort;
				GetPort(&origPort);
				SetPortWindowPort(window);
				GlobalToLocal(&myEvent->where);
				if(myEvent->modifiers & cmdKey)
					mousebutton = K_MOUSE2;
				else if(myEvent->modifiers & optionKey)
					mousebutton = K_MOUSE3;
				else
					mousebutton = K_MOUSE1;
				Key_Event(mousebutton, down);
				SetPort(origPort);
			}*/
			break;
	}
}

/*static*/ void HandleEvents (void)
{
	EventRecord		myEvent;
	Boolean			gotEvent;

	if (video_restart) return;

	// FIXME! - just calling GetNextEvent in carbon seems to enable apple-tab to switch processes
	// in OS9 which ABSOLUTELY BLOWS in fullscreen if you have TAB and COMMAND bound (like I do).
	// This doesn't happen on X since the dock handles that sort of thing and DSp puts it out of the picture.
	// GNE also has the annoyoning side effect of causing the finder to think it can't accomodate pop-up folders
	// during a "finder" or vid_restart switch (even if the game resolution is BIGGER than the desktop res !?!?!?)
	//
	// Anyway, when in fullscreen classic, flush all events and be happy.
	// (NOTE: We CAN'T just flush in OSX, because we rely on this for CarbonEvents to fire)

	if (background)
		gotEvent = WaitNextEvent(notKeyEvents, &myEvent, 1L, NULL);
	else
		gotEvent = GetNextEvent (everyEvent, &myEvent);

	while (gotEvent)
	{
		if (myEvent.what == nullEvent)
			break;
		switch(myEvent.what)
		{
			case nullEvent:
				break;

			case mouseDown:
				if (inwindow)
					OnMouseButton(&myEvent, true);
				break;

			case mouseUp:
				break;

			// We DON'T want cmd-q to quit unless the mouse is loose and we can see the menubar
			case keyDown:
			case autoKey:
				break;

			case updateEvt:
				break;

			case diskEvt:
				break;

			case activateEvt:
				break;

			case osEvt:
				if (((myEvent.message >> 24) & 0x0FF) == suspendResumeMessage)
				{
					if ((myEvent.message & resumeFlag) == 0)
						Sys_Deactivate();
					else
						Sys_Activate();
				}
				break;

			case kHighLevelEvent:
				AEProcessAppleEvent(&myEvent);
				break;
		}

		if (background)
			break;
		else
			gotEvent = GetNextEvent (everyEvent, &myEvent);
	}
}

//====================================================================================
//	InitializeAppleEvents
//
//	Install handlers for the 4 basic AppleEvents.
//====================================================================================
void InitializeAppleEvents (void)
{
	AEInstallEventHandler(kCoreEventClass, kAEQuitApplication, NewAEEventHandlerUPP((AEEventHandlerProcPtr) HandleQuitApplicationEvent), 0, false);
}

char  *commandLine;

// OSX's cmd-q triggers this, but we DON'T quit unless the mouse is loose and we can see the menubar
OSErr HandleQuitApplicationEvent(const AppleEvent *theAppleEvent, AppleEvent *reply, UInt32 handlerRefcon)
{
	extern qbool suspend_mouse;

	Sys_Printf("Got quit event.\n");

	if (inwindow && suspend_mouse)
		M_Menu_Quit_f();// Treat as if the in-game Quit was selected
	return noErr;
}

//====================================================================================
//	SetDefaultDirectory
//
//	Under native OS X, the default directory for apps is some bizarro Unix
//  location. Here we redefine it to the classic MacOS location, which is the
//  same folder that the app is running from.
//====================================================================================
void SetDefaultDirectory (void)
{
	ProcessSerialNumber serial;
	FSRef location, parent_ref;
	char path[PATH_MAX];
	OSStatus err;

	serial.highLongOfPSN = 0;
	serial.lowLongOfPSN = kCurrentProcess;
	err = GetProcessBundleLocation (&serial, &location);
	if (err != noErr)
		return;
	err = FSGetCatalogInfo (&location, kFSCatInfoNone, NULL, NULL, NULL, &parent_ref);
	if (err != noErr)
		return;

	FSRefMakePath (&parent_ref, path, sizeof(path) - 1);
	if (-1 == chdir (path))
		Sys_Printf ("Failed to change to working directory %s\n", path);
}

//====================================================================================
//	ErrorAlert
//
//	Throws up an error dialog, can optionally quit the app if needed
//====================================================================================
void ErrorAlert (Str255 inAlertString, int inError, Boolean inQuit)
{
	short		itemHit;
	DialogRef	alertDialog;
	Str255		numString;
	GWorldPtr	savedPort;
	GDHandle	savedDevice;

	// attempt to fetch the alert dialog
	alertDialog = GetNewDialog (rAlertDialog, NULL, (WindowRef) -1);
	if (alertDialog == NULL)
	{
		fprintf (stderr, "Error Alert Dialog failed!\n");
/* 		MAC_DEBUGSTR("\pError Alert Dialog failed!"); */
		goto cleanup;
	}

	// point to the port
	GetGWorld(&savedPort, &savedDevice);
	SetPortDialogPort(alertDialog);

	// set the OK item
	SetDialogDefaultItem(alertDialog, ok);

	// produce the string and make it paramtext
	if (inError)
	{
		NumToString(inError, numString);
		ParamText(inAlertString, numString, 0, 0);
	}
	else
		ParamText(inAlertString, 0, 0, 0);

	// display the alert and make sure the cursor is an arrow
	ShowWindow(GetDialogWindow(alertDialog));
	{
		Cursor arrow;
		SetCursor((Cursor *)GetQDGlobalsArrow(&arrow));
	}

	// loop on ModalDialog until we're done
	while (itemHit != ok)
		ModalDialog(NULL, &itemHit);

	// tear it down
	DisposeDialog(alertDialog);
	SetGWorld(savedPort, savedDevice);

cleanup:

	// ExitToShell now if we need to quit
	if (inQuit)
		ExitToShell();
}

//====================================================================================
// IsPressed
//
// Polls GetKeys to quickly see if a given key is pressed
//====================================================================================
short IsPressed (unsigned short k)
{
	unsigned char km[16];
	KeyMap *keymap = (KeyMap*) &km;
	GetKeys ((SInt32 *) *keymap);
	return ((km[k>>3] >> (k & 7) ) & 1);
}

void VID_GetVideoModesForActiveDisplays (void);

qbool forcewindowed = false;

// We need 8.6 or better with DrawSprocket 1.7.x and InputSprocket for classic.
// OSX 10.1 with DrawSprocket 1.99 or better is HIGHLY recommended, but we
// let it run (or try to run) on older versions, with warnings...
void VerifySystemSpecs (void)
{
	long result;
	Str255 errorString;
	NumVersion versDSp;

	// We require MacOS 8.6 or higher
	Gestalt (gestaltSystemVersion, &result);
	// Some of our HID hacks *require* 10.1 or later
	if (result < 0x01010) {
		/* We may hack around to allow pre 10.1 to run, but not worth the effort ATM...
		sprintf ((char *)errorString, "NOTE: MacOSX 10.1 or later is recommended for optimal performance.");
		c2pstrcpy (errorString, errorString);
		ErrorAlert (errorString, 0, false);
		*/
		Sys_Error ("%s requires MacOSX 10.1 or later.", kAppName);
	}

	Sys_Printf("MacOS Version: %x\n", result);

	// Sanity check for OpenGL
	if ((void *)aglGetVersion == (void *)kUnresolvedCFragSymbolAddress)
		Sys_Error ("%s requires OpenGL.", kAppName);

	// We cheat - DSpGetVersion is a new call in DSp 1.7, so we merely need to check
	// for its presence to determine if the user has DSp 1.7 and higher.
	if ((void *) kUnresolvedCFragSymbolAddress == (void *) DSpGetVersion)
		Sys_Error ("%s requires DrawSprocket 1.7 or later.", kAppName);
	else
		versDSp = DSpGetVersion ();

	// This version of DrawSprocket is not completely functional on Mac OS X
	if ((versDSp.majorRev == 0x01) && (versDSp.minorAndBugRev < 0x99))
	{
		sprintf ((char *)errorString, "NOTE: DrawSprocket 1.99 or later for MacOSX is recommended for optimal performance.");
		c2pstrcpy (errorString, errorString);
		ErrorAlert (errorString, 0, false);
	}

}

// These are saved in the startup prefs, so don't let an auto-config change them!!
// After this point, they can be used with vid_restart to change display modes on the fly
// Should probably replace these with C globals, but cvars are far easier for testing...
void SetVideoCvarsForPrefs (void)
{
	video_restart = true;

	Cvar_SetValue (&vid_mode, gScreen.profile->mode);
	Cvar_SetValue (&gl_vid_screen, gScreen.profile->screen);
	Cvar_SetValue (&gl_vid_colorbits, gScreen.profile->colorbits);
	Cvar_SetValue (&gl_texturebits, gScreen.profile->texturebits);

	if (inwindow)
  		Cvar_Set (&gl_vid_windowed, "1");
	else
		Cvar_Set (&gl_vid_windowed, "0");

	video_restart = false;
}

//====================================================================================
// SetGlobalsFromPrefs
//
// App-specific routine, this sets up any relevant global variables based on
// the current preferences settings.
//====================================================================================
static void SetGlobalsFromPrefs (void)
{
	extern int gFrequency;

	switch (macPrefs.sound_rate)
	{
		case kMenuSound11: gFrequency = 11025; break;
		case kMenuSound44: gFrequency = 44100; break;
		case kMenuSound22:
		default:
						   gFrequency = 22050; break;
	}

	if (macPrefs.color_depth == kMenuBpp16)
		vid_currentprofile.colorbits = 16;
//hack	else
		vid_currentprofile.colorbits = 32;

	if (macPrefs.tex_depth == kMenuBpp16) {
		vid_currentprofile.texturebits = 16;
		gl_solid_format = GL_RGB5;
		gl_alpha_format = GL_RGBA4;
//hack	} else {
		vid_currentprofile.texturebits = 32;
		gl_solid_format = GL_RGB8;
		gl_alpha_format = GL_RGBA8;
	}

	glWindowPos.v = macPrefs.glwindowpos.v ? macPrefs.glwindowpos.v : 52;
	glWindowPos.h = macPrefs.glwindowpos.h ? macPrefs.glwindowpos.h : 52;
	inwindow = macPrefs.window;

	// Can't use fullscreen if forcewindowed (DEBUG, or pre 1.99 DSp on OSX)
	if (forcewindowed)
		inwindow = true;

	// This is our startup video profile
	vid_currentprofile.screen = macPrefs.device;
	vid_currentprofile.mode = macPrefs.vid_mode;
	vid_currentprofile.window = inwindow;

	// Set up our global video setting pointers
	gScreen.profile = &vid_currentprofile;
	gScreen.device = &vid_devices[vid_currentprofile.screen];
	gScreen.mode = &vid_modes[vid_currentprofile.screen][vid_currentprofile.mode];
}

//====================================================================================
// Sys_Deactivate
//
// Get out of fullscreen & show the mouse - the game is still running, but won't
// process key or mouse events when the "background" flag is set
//====================================================================================
static void Sys_Deactivate (void)
{
	Com_DPrintf ("Sys_Deactivate...\n");

	// This will (sometimes) happen with Sys_SwitchToFinder_f
	if (background) {
		Com_DPrintf ("Already deactivated!\n");
		return;
	}

	// To avoid problems, we basically shut down the screen
	if (!inwindow)
	{
		video_restart = true;// hack

		S_ClearBuffer();

		VID_FadeOut ();
		aglSetDrawable (gContext, NULL);
		MacShutdownScreen();
		VID_FadeIn ();

		video_restart = false;// hack
	}

	// For some reason, on OS9, the app gets an activate event the first time it deactivates (?)
	// Grabbing events here seems to stop this...
	{
		EventRecord		myEvent;
		GetNextEvent (everyEvent, &myEvent);
	}

	Release_Cursor_f();
	background = true;

	VID_ShiftPalette (NULL);// Reset Desktop Gamma
}

//====================================================================================
// Sys_Activate
//
// Go back into fullscreen & hide the mouse
//====================================================================================
static void Sys_Activate (void)
{
	Com_DPrintf ("Sys_Activate...\n");

	if (!background) {
		Com_DPrintf ("Already active!\n");
		return;
	}

	// Do a mini vid_restart for fullscreen...
	if (!inwindow)
	{
		video_restart = true;// hack

		S_ClearBuffer();
		VID_FadeOut ();
		aglSetDrawable (gContext, NULL);
		MacSetupScreen ();
		_aglSetDrawableMonitor (gContext, inwindow);

		vid.width = gScreen.mode->width;
		vid.height = gScreen.mode->height;

		VID_SetConWidth();

		vid.recalc_refdef = true;

		video_restart = false;// hack
		VID_FadeIn ();
	}

	background = false;

	// Try again (seems to stick for some people)
	if (!inwindow || (inwindow && key_dest != key_menu))
		Capture_Cursor_f();

	VID_ShiftPalette (NULL);// Reset in-Game Gamma
}

//====================================================================================
// Sys_FindProcess
//
// Utility routine used by 'finder' command
//====================================================================================
static OSStatus Sys_FindProcess( OSType creator, OSType type, ProcessSerialNumber *outProcess )
{
	ProcessInfoRec		theProc;
	OSStatus			outStatus = 0L;
	ProcessSerialNumber	psn;

	// Start from kNoProcess
	psn.highLongOfPSN = 0;
	psn.lowLongOfPSN = kNoProcess;

	// Initialize ProcessInfoRec fields, or we'll have memory hits in random locations
	theProc.processInfoLength = sizeof( ProcessInfoRec );
	theProc.processName = nil;
	theProc.processAppSpec = nil;
	theProc.processLocation = nil;

	while(true)
	{
		// Keep looking for the process until we find it
		outStatus = GetNextProcess(&psn);
		if( outStatus != noErr )
			break;

		// Is the current process the one we're looking for?
		outStatus = GetProcessInformation(&psn, &theProc);
		if( outStatus != noErr )
			break;
		if( (theProc.processType == type ) && (theProc.processSignature == creator) )
			break;
	}
	*outProcess = psn;
	return outStatus;
}

//====================================================================================
// Sys_SwitchToFinder_f
//
// 'finder' console command allows users to switch to the finder at any time (even in fullscreen)
//====================================================================================

void Sys_SwitchToFinder_f (void)
{
	const OSType		kFinderSignature = 'MACS';
	const OSType		kFinderType = 'FNDR';
	ProcessSerialNumber	finderProcess;

	// Get out of fullscreen before switching
	// We *have* to do this on 9 since fullscreen flushes events, and we'll never get a deactivate.
	// We *have* to do this on X to allow menu 'extensions' to re-align themselves.
	Sys_Deactivate ();

	if( Sys_FindProcess( kFinderSignature, kFinderType, &finderProcess ) == noErr)
		SetFrontProcess( &finderProcess );
	else
		Com_Printf ("Couldn't activate finder process.\n");

}

//
// Initialize
//
// Sets up the Mac toolbox, checks our system requirements and loads the prefs.
//
static void Initialize (void)
{
	long randSeed;

	// pOx - had to do this first for some reason (startup events went unhandled otherwise) ??
	InitializeAppleEvents ();

	// Can we run?
	VerifySystemSpecs ();

	// Tell OSX to set the local directory to the one the app lives in
	SetDefaultDirectory ();

	MoreMasterPointers (64UL * 3);

	InitCursor ();
	RegisterAppearanceClient ();

	//
	//	To make the Random sequences truly random, we need to make the seed start
	//	at a different number.  An easy way to do this is to put the current time
	//	and date into the seed.  Since it is always incrementing the starting seed
	//	will always be different.  Done for each call of Random, or the sequence
	//	will no longer be random.  Only needed once, here in the init.
	//
	randSeed = GetQDGlobalsRandomSeed ();
	GetDateTime ((unsigned long*) &randSeed);

	// Get our display hardware specs.
	VID_GetVideoModesForActiveDisplays();

	// Get our last settings
	LoadPrefs ();
}

static OSStatus			OptionsHandler( EventHandlerCallRef inHandler, EventRef inEvent, void* userData );

static const ControlID	kModePopup	 	= { 'PREF', 1 };
static const ControlID	kColorsPopup	= { 'PREF', 2 };
static const ControlID	kCmdLineText	= { 'PREF', 3 };
static const ControlID	kWindowed		= { 'PREF', 4 };

static void
DoOptions()
{
	IBNibRef			nibRef;
	EventTypeSpec		cmdEvent = { kEventClassCommand, kEventCommandProcess };
	WindowRef			window;
	OSStatus			status;
//	OSErr				err;
	EventHandlerUPP		handler;
	ControlRef			mode_ctl, bpp_ctl, cmdline_ctl, win_ctl;
	MenuRef				mode_menu;
	Size				cmdline_size;
	int					i;

	// Open our nib, create the window, and close the nib.
	status = CreateNibReference( CFSTR( "Options" ), &nibRef );
	require_noerr( status, CantOpenNib );

	DrawMenuBar();

	status = CreateWindowFromNib( nibRef, CFSTR( "ezQuake" ), &window );
	require_noerr( status, CantCreateWindow );

	DisposeNibReference( nibRef );

	// For each control of interest, set its value as appropriate
	// for the settings we currently have.

	GetControlByID (window, &kCmdLineText, &cmdline_ctl);
	SetControlData (cmdline_ctl, kControlEditTextPart, kControlEditTextTextTag,
					strlen (macPrefs.command_line), macPrefs.command_line);

	GetControlByID (window, &kColorsPopup, &bpp_ctl);
	if (macPrefs.color_depth)
		SetControlValue (bpp_ctl, 2);

	GetControlByID (window, &kModePopup, &mode_ctl);
	status = CreateNewMenu (0x700, 0, &mode_menu);
	require_noerr( status, OptionsFailed );

	GetControlByID (window, &kWindowed, &win_ctl);
	SetControlValue (win_ctl, macPrefs.window != 0);

	for (i = 0; i < vid_devices[0].numModes; ++i)
	{
		char		vid_mode[64];
		CFStringRef	mode_str;

		snprintf (vid_mode, 64, "%dx%d @ %dHz", (int) vid_modes[0][i].width,
				  (int) vid_modes[0][i].height, (int) vid_modes[0][i].refresh >> 16);
		mode_str = CFStringCreateWithCString (NULL, vid_mode, kCFStringEncodingASCII);
		AppendMenuItemTextWithCFString (mode_menu, mode_str, 0, 0, NULL);
	}

	SetControlPopupMenuHandle (mode_ctl, mode_menu);
	SetControlMaximum (mode_ctl, vid_devices[0].numModes);
	if (macPrefs.vid_mode)
		SetControlValue (mode_ctl, macPrefs.vid_mode + 1);

	// Now create our UPP and install the handler.
	handler = NewEventHandlerUPP( OptionsHandler );

	InstallApplicationEventHandler( handler, 1, &cmdEvent, 0, NULL );

	// Position and show the window

	RepositionWindow( window, NULL, kWindowAlertPositionOnMainScreen );
	ShowWindow( window );

	// Now we run modally. We will remain here until the PrefHandler
	// calls QuitAppModalLoopForWindow if the user clicks OK or
	// Cancel.

	RunApplicationEventLoop ();

	macPrefs.window = GetControlValue (win_ctl);
	macPrefs.vid_mode = GetControlValue (mode_ctl) - 1;
	macPrefs.color_depth = GetControlValue (bpp_ctl) - 1;
	GetControlData (cmdline_ctl, kControlEditTextPart, kControlEditTextTextTag,
					PREF_CMDLINE_LEN, macPrefs.command_line, &cmdline_size);
	macPrefs.command_line[cmdline_size] = 0;

	// OK, we're done. Dispose of our window and our UPP.
	// We do the UPP last because DisposeWindow can send out
	// CarbonEvents, and we haven't explicitly removed our
	// handler. If we disposed the UPP, the Toolbox might try
	// to call it. That would be bad.

	DisposeWindow( window );
	DisposeEventHandlerUPP( handler );

	return;

CantCreateWindow:
	Sys_Printf ("Can't create options window\n");
	DisposeNibReference( nibRef );
	return;

OptionsFailed:
CantOpenNib:
	Sys_Printf ("Can't open Nib resource\n");
	return;
}

static OSStatus
OptionsHandler( EventHandlerCallRef inHandler, EventRef inEvent, void* userData )
{
	HICommand			cmd;
	OSStatus			result = eventNotHandledErr;

	GetEventParameter( inEvent, kEventParamDirectObject, typeHICommand, NULL, sizeof( cmd ), NULL, &cmd );

	switch (cmd.commandID)
	{
	case kHICommandOK:
		action = START;
		QuitApplicationEventLoop();
		break;
	case kHICommandQuit:
		action = QUIT;
		QuitApplicationEventLoop();
		break;
	}

	return result;
}

// int main (int argc, char **argv)
int main (int argc, char *argv[])
{
	double	now, then = 0.0;

	COM_InitArgv (argc, argv);
#if !defined(CLIENTONLY)
	dedicated = COM_CheckParm ("-dedicated");
#endif
	if (!dedicated) {
		Str255 cleanCommandLine;
		char *ptr;

		Initialize();

		InitCursor();

		DoOptions();

		// If the user wanted to quit from the options dialog, we oblige
		if (action == QUIT)
			return (0);

		// Copy the ini pref to the command line if there's no drag'n drop overide, but only if 'use' is checked.
		if (macPrefs.command_line[0])
		{
			commandLine = malloc(strlen(macPrefs.command_line)+2);
			commandLine[0] = ' ';
			commandLine[1] = 0;
			strcat(commandLine, macPrefs.command_line);
		}

		cleanCommandLine[0] = ' ';
		cleanCommandLine[1] = '\0';

		// OSX is touchy...
		// Clear white space from the command line (but leave a single space between each arg)
		if (commandLine && action != FAILED)
		{
			int		count = 1;// leave an initial leading space in cleanCommandLine
			qbool 	white = false;

			// First char is a space (this was a leftover from the old code - FIXME I guess)
			ptr = &commandLine[1];

			while (ptr[0])
			{
				// convert tabs to spaces
				if (ptr[0] == '\t') ptr[0] = ' ';

				// skip any extra leading spaces
				if (count == 1 && ptr[0] == ' ') ptr++;

				// clean up white space
				else if (ptr[0] == ' ')
				{
					if (white)
						ptr++;
					else
					{
						white = true;
						cleanCommandLine[count++] = ptr[0];// single space
						if (count == 255) break;
						ptr++;
					}
				}
				else
				{
					white = false;
					cleanCommandLine[count++] = ptr[0];
					if (count == 255) break;
					ptr++;
				}
			}

			cleanCommandLine[count] = '\0';
		}

		if (cleanCommandLine[1])
		{
			char maxArgs = 2; // this gets sized up quickly

			argv = malloc(sizeof(char*) * maxArgs);
			argv[0] = cleanCommandLine;
			Sys_Printf("Active Command Line: \"%s\"\n",argv[0]);

			argc = 1;
			argv[1]=strtok(argv[0]," ");
			argv[0] = kAppName; // program name is always argv[0].
			do
			{
				if (++argc == maxArgs)
				argv = realloc(argv,sizeof(char*) * (maxArgs*=2));
				argv[argc] = strtok(NULL," ");
			} while (argv[argc]);
		}

		if (COM_CheckParm("-window") || COM_CheckParm("-startwindowed"))
			forcewindowed = true;

		// Set up the system state based on what the preferences say
		SetGlobalsFromPrefs ();

	}

	Sys_Printf ("Host_Init\n");
	Host_Init (argc, argv, 16 * 1024 * 1024);

	if (!dedicated) {
	// Stupid! - Override video cvars with the pref values
		SetVideoCvarsForPrefs();
	}

	while (1) {
		if (dedicated)
			NET_Sleep (10);

		now = Sys_DoubleTime ();
		HandleEvents ();
		Host_Frame (now-then);
		then = now;

		if (dedicated) {
			if (sys_extrasleep.value)
				usleep (sys_extrasleep.value);
		}
	}
}

// disconnect -->
int  Sys_CreateThread(DWORD WINAPI (*func)(void *), void *param)
{
    pthread_t thread;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setschedpolicy(&attr, SCHED_OTHER);   // ale gowno

    pthread_create(&thread, &attr, (void *)func, param);
    return 1;
}

// kazik -->
// directory listing functions
int CopyDirent(sys_dirent *ent, struct dirent *tmpent)
{
    struct stat statbuf;
    struct tm * lintime;
    if (stat(tmpent->d_name, &statbuf) != 0)
        return 0;

    strncpy(ent->fname, tmpent->d_name, MAX_PATH_LENGTH);
    ent->fname[MAX_PATH_LENGTH-1] = 0;

    lintime = localtime(&statbuf.st_mtime);
    UnixtimeToWintime(&ent->time, lintime);

    ent->size = statbuf.st_size;

    ent->directory = (statbuf.st_mode & S_IFDIR);

    return 1;
}

unsigned long Sys_ReadDirFirst(sys_dirent *ent)
{
    struct dirent *tmpent;
    DIR *dir = opendir(".");
    if (dir == NULL)
        return 0;

    tmpent = readdir(dir);
    if (tmpent == NULL)
    {
        closedir(dir);
        return 0;
    }

    if (!CopyDirent(ent, tmpent))
    {
        closedir(dir);
        return 0;
    }

    return (unsigned long)dir;
}

int Sys_ReadDirNext(unsigned long search, sys_dirent *ent)
{
    struct dirent *tmpent;

    tmpent = readdir((DIR*)search);

    if (tmpent == NULL)
        return 0;

    if (!CopyDirent(ent, tmpent))
        return 0;

    return 1;
}

void Sys_ReadDirClose(unsigned long search)
{
    closedir((DIR *)search);
}

void _splitpath(const char *path, char *drive, char *dir, char *file, char *ext)
{
    const char *f, *e;

    if (drive)
    drive[0] = 0;

    f = path;
    while (strchr(f, '/'))
    f = strchr(f, '/') + 1;

    if (dir)
    {
    strncpy(dir, path, min(f-path, _MAX_DIR));
        dir[_MAX_DIR-1] = 0;
    }

    e = f;
    while (*e == '.')   // skip dots at beginning
    e++;
    if (strchr(e, '.'))
    {
    while (strchr(e, '.'))
        e = strchr(e, '.')+1;
    e--;
    }
    else
    e += strlen(e);

    if (file)
    {
        strncpy(file, f, min(e-f, _MAX_FNAME));
    file[min(e-f, _MAX_FNAME-1)] = 0;
    }

    if (ext)
    {
    strncpy(ext, e, _MAX_EXT);
    ext[_MAX_EXT-1] = 0;
    }
}

// full path
char *Sys_fullpath(char *absPath, const char *relPath, int maxLength)
{
    // too small buffer, copy in tmp[] and then look is enough space in output buffer aka absPath
    if (maxLength-1 < PATH_MAX)	{
			 char tmp[PATH_MAX+1];
			 if (realpath(relPath, tmp) && absPath && strlen(tmp) < maxLength+1) {
					strlcpy(absPath, tmp, maxLength+1);
          return absPath;
			 }

       return NULL;
		}

    return realpath(relPath, absPath);
}
// kazik <--

int Sys_remove (char *path)
{
	return unlink(path);
}

// kazik -->
int Sys_chdir (const char *path)
{
    return (chdir(path) == 0);
}

char * Sys_getcwd (char *buf, int bufsize)
{
    return getcwd(buf, bufsize);
}
// kazik <--

/********************************* CLIPBOARD *********************************/

#define SYS_CLIPBOARD_SIZE		256
static char clipboard_buffer[SYS_CLIPBOARD_SIZE] = {0};

char *Sys_GetClipboardData(void) {
	return clipboard_buffer;
}

void Sys_CopyToClipboard(char *text) {
	strlcpy(clipboard_buffer, text, SYS_CLIPBOARD_SIZE);
}

// <-- disconnect

void *Sys_GetProcAddress (const char *ExtName)
{
	static CFBundleRef cl_gBundleRefOpenGL = 0;
	if (cl_gBundleRefOpenGL == 0)
	{
		cl_gBundleRefOpenGL = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.opengl"));
		if (cl_gBundleRefOpenGL == 0)
			Sys_Error("Unable to find com.apple.opengl bundle");
	}

	return CFBundleGetFunctionPointerForName(
		cl_gBundleRefOpenGL,
		CFStringCreateWithCStringNoCopy(
			0,
			ExtName,
			CFStringGetSystemEncoding(),
			0));
}

wchar *Sys_GetClipboardTextW(void) {
	return NULL;
}

/*************************** INTER PROCESS CALLS *****************************/

void Sys_InitIPC()
{
	// TODO : Implement Sys_InitIPC() me on mac.
}

void Sys_ReadIPC()
{
	// TODO : Implement Sys_ReadIPC() me on mac.
	// TODO : Pass the read char buffer to COM_ParseIPCData()
}

void Sys_CloseIPC()
{
	// TODO : Implement Sys_CloseIPC() me on mac.
}

unsigned int Sys_SendIPC(const char *buf)
{
	// TODO : Implement Sys_SendIPC() me on mac.
}

/********************************** SEMAPHORES *******************************/
/* Sys_Sem*() returns 0 on success; on error, -1 is returned */
int Sys_SemInit(sem_t *sem, int value, int max_value) 
{
	return sem_init(sem, 0, value); // Don't share between processes
}

int Sys_SemWait(sem_t *sem) 
{
	return sem_wait(sem);
}

int Sys_SemPost(sem_t *sem)
{
	return sem_post(sem);
}

int Sys_SemDestroy(sem_t *sem)
{
	return sem_destroy(sem);
}

