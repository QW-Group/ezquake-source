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
// qwsvdef.h -- primary header for server


//define	PARANOID			// speed sapping error checking

#include <time.h>
#include "common.h"
#include "fs.h"

#ifdef GLQUAKE
#include "gl_model.h"
#else
#include "r_model.h"
#endif

#include "server.h"

#ifdef USE_PR2
// Angel -->
#include "pr2_vm.h"
#include "pr2.h"
#include "g_public.h"
// <-- Angel
#endif


#include "net.h"
#include "crc.h"
#include "sha1.h"
#include "pmove.h"
#include "version.h"
#include "sv_log.h"
#include "sv_world.h"
