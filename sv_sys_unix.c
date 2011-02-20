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

	$Id: sv_sys_unix.c,v 1.11 2006-12-30 11:24:54 disconn3ct Exp $

*/
#include <sys/types.h>
#include "qwsvdef.h"

#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#else
#include <sys/dir.h>
#endif

cvar_t	sys_nostdout = {"sys_nostdout","0"};
cvar_t	sys_extrasleep = {"sys_extrasleep","0"};

qbool stdin_ready;
int do_stdin = 1;

/*
===============================================================================

				REQUIRED SYS FUNCTIONS

===============================================================================
*/


/*
============
Sys_mkdir

============
*/
void Sys_mkdir (const char *path)
{
	if (mkdir (path, 0777) != -1)
		return;
	if (errno != EEXIST)
		Sys_Error ("mkdir %s: %s",path, strerror(errno)); 
}


/*
================
Sys_DoubleTime
================
*/
double Sys_DoubleTime (void)
{
	struct timeval tp;
	struct timezone tzp;
	static int		secbase;

	gettimeofday(&tp, &tzp);
	
	if (!secbase)
	{
		secbase = tp.tv_sec;
		return tp.tv_usec/1000000.0;
	}
	
	return (tp.tv_sec - secbase) + tp.tv_usec/1000000.0;
}


/*
================
Sys_Error
================
*/
void Sys_Error (char *error, ...)
{
	va_list argptr;
	char string[1024];
	
	va_start (argptr ,error);
	vsnprintf (string, sizeof(string), error, argptr);
	va_end (argptr);
	printf ("Fatal error: %s\n",string);
	
	exit (1);
}


/*
================
Sys_Printf
================
*/
void Sys_Printf (char *fmt, ...)
{
	va_list argptr;
	static char text[2048];
	unsigned char *p;

	va_start (argptr, fmt);
	vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);

	if (sys_nostdout.value)
		return;

	for (p = (unsigned char *)text; *p; p++) {
		*p &= 0x7f;
		if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
			printf("[%02x]", *p);
		else
			putc(*p, stdout);
	}
	fflush(stdout);
}


/*
================
Sys_Quit
================
*/
void Sys_Quit (void)
{
	exit (0);		// appkit isn't running
}


/*
=============
Sys_Init

Quake calls this so the system can register variables before host_hunklevel
is marked
=============
*/
void Sys_Init (void)
{
	Cvar_Register (&sys_nostdout);
	Cvar_Register (&sys_extrasleep);
}

/*
=============
main
=============
*/
int main (int argc, char *argv[])
{
	double	time, oldtime, newtime;

	Host_Init (argc, argv, 16*1024*1024);

//
// main loop
//
	oldtime = Sys_DoubleTime () - 0.1;
	while (1)
	{
		// select on the net socket and stdin
		NET_Sleep (10);

		// find time passed since last cycle
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;
		oldtime = newtime;
		
		Host_Frame (time);		
		
		// extrasleep is just a way to generate a fucked up connection on purpose
		if (sys_extrasleep.value)
			usleep (sys_extrasleep.value);
	}	
}

