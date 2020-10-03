
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
// r_surf.c: surface-related refresh code

#include "quakedef.h"
#include "r_framestats.h"
#include "r_local.h"
#include "r_renderer.h"

static cvar_t* framestats_shown;

void R_PerformanceBeginFrame(void)
{
	qbool frameStatsVisible;

	if (!framestats_shown) {
		framestats_shown = Cvar_Find("hud_framestats_show");
	}
	frameStatsVisible = framestats_shown && framestats_shown->integer;

	if (r_speeds.integer) {
		renderer.EnsureFinished();
	}

	memcpy(&prevFrameStats, &frameStats, sizeof(prevFrameStats));
	memset(&frameStats, 0, sizeof(frameStats));
	if (r_speeds.integer || frameStatsVisible || cls.timedemo) {
		frameStats.start_time = Sys_DoubleTime();
	}
	frameStats.hotloop = true;
}

void R_PerformanceEndFrame(void)
{
	qbool frameStatsVisible;

	if (!framestats_shown) {
		framestats_shown = Cvar_Find("hud_framestats_show");
	}
	frameStatsVisible = framestats_shown && framestats_shown->integer;
	frameStats.hotloop = false;

	if (r_speeds.integer || frameStatsVisible || cls.timedemo) {
		frameStats.end_time = Sys_DoubleTime();

		if (cls.timedemo && cls.td_starttime) {
			int ms = (int)ceil((frameStats.end_time - frameStats.start_time) * 10000.0);

			ms = bound(0, ms, sizeof(cls.td_frametime_stats) / sizeof(cls.td_frametime_stats[0]) - 1);

			if (ms > cls.td_frametime_max) {
				cls.td_frametime_max = ms;
				cls.td_frametime_max_frame = cls.framecount - cls.td_startframe - 1;
			}

			++cls.td_frametime_stats[ms];
		}

		if (r_speeds.integer) {
			double time = frameStats.end_time - frameStats.start_time;

			Print_flags[Print_current] |= PR_TR_SKIP;
			if (cl.standby || com_serveractive) {
				Com_Printf("%5.2f ms %4i wpoly, %4i epoly, %4i texbinds\n", (time * 1000), frameStats.classic.polycount[polyTypeWorldModel], frameStats.classic.polycount[polyTypeAliasModel], frameStats.texture_binds);
			}
			else {
				Com_Printf("%5.2f ms %4i wpoly\n", (time * 1000), frameStats.classic.polycount[polyTypeWorldModel]);
			}
		}
	}
}


