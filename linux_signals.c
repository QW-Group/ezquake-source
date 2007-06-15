/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================

    $Id: linux_signals.c,v 1.4 2007-06-15 12:26:07 johnnycz Exp $

*/
#include <signal.h>

#include "quakedef.h"
#include "input.h"

static qbool signalcaught = false;

static void signal_handler(int sig) // bk010104 - replace this... (NOTE TTimo huh?)
{
	if (signalcaught)
	{
		printf("DOUBLE SIGNAL FAULT: Received signal %d, exiting...\n", sig);
		Sys_Quit();
		exit(0);
	}

	signalcaught = true;
	printf("Received signal %d, exiting...\n", sig);

#ifndef SERVERONLY
//
// client related things
//
#ifdef WITH_EVDEV
	IN_Shutdown();
#endif
	VID_Shutdown();  // bk010104 - shouldn't this be CL_Shutdown

#endif

	Sys_Quit();
	exit(0);
}

void InitSig(void)
{
	signal(SIGHUP,  signal_handler);
	signal(SIGINT,  signal_handler); // btw, q3 do not have this signal handling
	signal(SIGQUIT, signal_handler);
	signal(SIGILL,  signal_handler);
	signal(SIGTRAP, signal_handler);
	signal(SIGIOT,  signal_handler);
	signal(SIGBUS,  signal_handler);
	signal(SIGFPE,  signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGTERM, signal_handler);
}
