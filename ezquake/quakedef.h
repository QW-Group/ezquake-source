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
// quakedef.h -- primary header for client


//define	PARANOID // speed sapping error checking

#ifndef  __QUAKEDEF_H_
#define  __QUAKEDEF_H_

#define XML_STATIC

#ifdef WITH_PNG
#ifndef WITH_PNG_STATIC
#define WITH_PNG_STATIC
#endif
#endif

#ifdef GLQUAKE
#ifdef WITH_JPEG
#ifndef WITH_JPEG_STATIC
#define WITH_JPEG_STATIC
#endif
#endif
#endif

#define TCPCONNECT

#include "common.h"

#include <expat.h>
#include <pcre.h>
#ifdef EMBED_TCL
#include "embed_tcl.h"
#endif

#include "vid.h"
#include "draw.h"
#include "screen.h"
#include "render.h"
#include "console.h"
#include "cl_view.h"

#include "client.h"
#include "qsound.h"
#include "sbar.h"
#include "utils.h"
#include "hud.h"
#include "hud_common.h"
#include "image.h"
#include "modules.h"
#include "keys.h"
#include "fmod.h"
#include "auth.h"
#include "menu.h"
#include "input.h"
#include "stats_grid.h"

#include "version.h"
#include "crc.h"

#ifdef GLQUAKE
#include "gl_model.h"
#include "gl_local.h"
#include "vx_stuff.h"
#else
#include "r_model.h"
#include "r_local.h"
#endif

#include "pmove.h"
#include "rulesets.h"
#include "teamplay.h"

#include "xsd.h"
#include "Ctrl.h"
#include "Ctrl_Tab.h"
#include "Ctrl_PageViewer.h"
#include "EX_FileList.h"
#include "help.h"

// HUD -> hexum
extern  int         host_screenupdatecount; // kazik, incremented every screen update, never reset

#endif //__QUAKEDEF_H_
