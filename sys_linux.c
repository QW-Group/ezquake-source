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
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>

#include "quakedef.h"

int noconinput = 0;


qboolean stdin_ready;
int do_stdin = 1;


cvar_t sys_nostdout = {"sys_nostdout", "0"};	
cvar_t sys_extrasleep = {"sys_extrasleep", "0"};


void Sys_Printf (char *fmt, ...) {
	va_list argptr;
	char text[2048];
	unsigned char *p;

	if (!dedicated)
		return;

	va_start (argptr,fmt);
	vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);

	if (strlen(text) > sizeof(text))
		Sys_Error("memory overwrite in Sys_Printf");

    if (sys_nostdout.value)
        return;

	for (p = (unsigned char *) text; *p; p++)
		if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
			printf("[%02x]", *p);
		else
			putc(*p, stdout);
}

void Sys_Quit (void) {
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
	exit(0);
}

void Sys_Init(void) {
	if (dedicated) {
		Cvar_Register (&sys_nostdout);
		Cvar_Register (&sys_extrasleep);
	}
}

void Sys_Error (char *error, ...) { 
    va_list argptr;
    char string[1024];

	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);	//change stdin to non blocking
    
    va_start (argptr, error);
    vsnprintf (string, sizeof(string), error, argptr);
    va_end (argptr);
	fprintf(stderr, "Error: %s\n", string);

	Host_Shutdown ();
	exit (1);
}

//returns -1 if not present
int	Sys_FileTime (char *path) {
	struct stat buf;
	
	if (stat(path, &buf) == -1)
		return -1;	
	return buf.st_mtime;
}

void Sys_mkdir (char *path) {
    mkdir (path, 0777);
}

double Sys_DoubleTime (void) {
    struct timeval tp;
    struct timezone tzp; 
    static int secbase; 

    gettimeofday(&tp, &tzp);  

    if (!secbase) {
        secbase = tp.tv_sec;
        return tp.tv_usec/1000000.0;
    }

    return (tp.tv_sec - secbase) + tp.tv_usec / 1000000.0;
}

void floating_point_exception_handler (int whatever) {
	signal(SIGFPE, floating_point_exception_handler);
}

char *Sys_ConsoleInput (void) {
	static char text[256];
	int len;

	if (!dedicated)
		return NULL;

	if (!stdin_ready || !do_stdin)
		return NULL;		// the select didn't say it was ready
	stdin_ready = false;

	len = read (0, text, sizeof(text));
	if (len == 0) {	// end of file		
		do_stdin = 0;
		return NULL;
	}
	if (len < 1)
		return NULL;
	text[len - 1] = 0;	// rip off the /n and terminate
	
	return text;
}

#if !id386
void Sys_HighFPPrecision (void) {}

void Sys_LowFPPrecision (void) {}
#endif

int skipframes;

int main (int argc, char **argv) {
	double time, oldtime, newtime;

	COM_InitArgv (argc, argv);
#if !defined(CLIENTONLY)
	dedicated = COM_CheckParm ("-dedicated");
#endif

	if (!dedicated) {
		signal(SIGFPE, SIG_IGN);

		// we need to check for -noconinput and -nostdout before Host_Init is called
		if (!(noconinput = COM_CheckParm("-noconinput")))
			fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);

		if (COM_CheckParm("-nostdout"))
			sys_nostdout.value = 1;

		#if id386
			Sys_SetFPCW();
		#endif
	}

    Host_Init (argc, argv, 16 * 1024 * 1024);

    oldtime = Sys_DoubleTime ();
    while (1) {
		if (dedicated)
			NET_Sleep (10);

		// find time spent rendering last frame
        newtime = Sys_DoubleTime ();
        time = newtime - oldtime;
		oldtime = newtime;

		Host_Frame(time);

		if (dedicated) {
			if (sys_extrasleep.value)
				usleep (sys_extrasleep.value);
		}
    }
}

void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length) {
	int r;
	unsigned long addr;
	int psize = getpagesize();

	addr = (startaddr & ~(psize - 1)) - psize;
	r = mprotect((char*) addr, length + startaddr - addr + psize, 7);
	if (r < 0)
    		Sys_Error("Protection change failed");
}

/********************************* CLIPBOARD *********************************/

#define SYS_CLIPBOARD_SIZE		256
static char clipboard_buffer[SYS_CLIPBOARD_SIZE] = {0};

char *Sys_GetClipboardData(void) {
	return clipboard_buffer;
}

void Sys_CopyToClipboard(char *text) {
	Q_strncpyz(clipboard_buffer, text, SYS_CLIPBOARD_SIZE);
}
