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


//define PARANOID // speed sapping error checking

#ifndef  __QUAKEDEF_H__
#define  __QUAKEDEF_H__

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

#if defined(WITH_WINAMP) || defined(WITH_AUDACIOUS) || defined(WITH_XMMS) \
	|| defined (WITH_XMMS2) || defined(WITH_MPD)
#define WITH_MP3_PLAYER
#endif

//#define WITH_ASMLIB

#include "common.h"

#include "vid.h"
#include "screen.h"
#include "render.h"
#include "draw.h"
#include "console.h"
#include "cl_view.h"
#ifdef WITH_FTE_VFS
#include "fs.h"
#endif

#include "client.h"

#endif /* !__QUAKEDEF_H__ */
