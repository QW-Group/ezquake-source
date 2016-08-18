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
// sys.h -- non-portable functions

#ifdef _WIN32
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

#ifdef _WIN32
#define Sys_MSleep(x) Sleep(x)
#else
#define Sys_MSleep(x) usleep((x) * 1000)
#endif

#ifndef _WIN32
#define DWORD unsigned int
#define WINAPI
#ifndef MAX_PATH
	#define MAX_PATH (1024)
#endif
#define _MAX_FNAME 1024
#define _MAX_EXT 64
#define _MAX_DIR 1024
#endif



#include "localtime.h"

// create detached thread
int  Sys_CreateDetachedThread(int (*func)(void *), void *data);

#define MAX_PATH_LENGTH 1024

typedef struct sys_dirent_s
{
    int directory;
    int hidden;
    char fname[MAX_PATH_LENGTH];
    unsigned int size;
    SYSTEMTIME time;
} sys_dirent; 

#ifdef _WIN32
typedef HANDLE SysDirEnumHandle;
#else
typedef unsigned long SysDirEnumHandle;
#endif

char *Sys_fullpath(char *absPath, const char *relPath, int maxLength);
SysDirEnumHandle  Sys_ReadDirFirst(sys_dirent *);               // 0 if failed
int            Sys_ReadDirNext(SysDirEnumHandle, sys_dirent *); // 0 if failed (EOF)
void           Sys_ReadDirClose(SysDirEnumHandle);
int    Sys_chdir (const char *path);
char * Sys_getcwd (char *buf, int bufsize);


// file IO
void Sys_mkdir (const char *path);
int Sys_remove (char *path);
int Sys_EnumerateFiles (char *gpath, char *match, int (*func)(char *, int, void *), void *parm);

// memory protection
void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length);

// an error will cause the entire program to exit
void Sys_Error (char *error, ...);

// send text to the console
void Sys_Printf (char *fmt, ...);

void Sys_Quit (void);

double Sys_DoubleTime (void);


// Perform Key_Event () callbacks until the input que is empty
void Sys_SendKeyEvents (void);

void Sys_Init (void);

wchar *Sys_GetClipboardTextW(void);
void Sys_CopyToClipboard(char *);

void Sys_GetFullExePath(char *path, unsigned int path_length, int long_name);

// Inter Process Call functions.
void Sys_InitIPC(void);
void Sys_ReadIPC(void);
void Sys_CloseIPC(void);
unsigned int Sys_SendIPC(const char *buf);

// Semaphore functions
#ifdef _WIN32
typedef HANDLE sem_t;
#else
#include <semaphore.h>
#endif
int Sys_SemInit(sem_t *sem, int value, int max_value);
int Sys_SemWait(sem_t *sem);
int Sys_SemPost(sem_t *sem);
int Sys_SemDestroy(sem_t *sem);

// Timer Resolution
// On windows to Sleep(1) really take only 1 ms it is necessary to explicitly request
// such a high precision, otherwise the thread would sleep for much higher time (materials mention 18 ms or 50 ms)
// This interface allows to request a temporary increased resolution of the timer device
typedef struct timerresolution_session_s {
	int set;
	unsigned int interval;
} timerresolution_session_t;

#ifdef _WIN32
// for initialization of the session structure
void Sys_TimerResolution_InitSession(timerresolution_session_t * s);

// request minimum timer resolution for given session
// call this before a block in which you are using Sleep() with low argument value
void Sys_TimerResolution_RequestMinimum(timerresolution_session_t * s);

// release the requested resolution for this session
// lets timer device use lower resolution
// always call this after you don't need precise Sleep anymore!
void Sys_TimerResolution_Clear(timerresolution_session_t * s);

// Cancel deadkey combination for keyboards where console toggle is also deadkey
void Sys_CancelDeadKey (void);
#else

// not implemented on other platforms
#define Sys_TimerResolution_InitSession(x)
#define Sys_TimerResolution_RequestMinimum(x)
#define Sys_TimerResolution_Clear(x)

#endif


// MVDSV compatibility

#define MAX_DIRFILES 4096
#define MAX_DEMO_NAME 196

typedef struct
{
	char	name[MAX_DEMO_NAME];
	int	size;
	int	time;
	qbool	isdir; //bliP: list dir
} file_t;

typedef struct
{
	file_t *files;
	int	size;
	int	numfiles;
	int	numdirs;
} dir_t;

int		Sys_Script (const char *path, const char *args);

int		Sys_rmdir (const char *path);
dir_t	Sys_listdir (const char *path, const char *ext, int sort_type);

int		Sys_compare_by_date (const void *a, const void *b);
int		Sys_compare_by_name (const void *a, const void *b);
#define SORT_NO			0
#define SORT_BY_DATE	1
#define SORT_BY_NAME	2


// library loading.

typedef struct
{
	void **funcptr;
	char *name;
} dllfunction_t;

typedef void *dllhandle_t;

dllhandle_t *Sys_LoadLibrary(const char *name, dllfunction_t *funcs);
void Sys_CloseLibrary(dllhandle_t *lib);
void *Sys_GetAddressForName(dllhandle_t *module, const char *exportname);

void Sys_CvarInit(void);
