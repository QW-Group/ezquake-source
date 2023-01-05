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

$Id: gl_rmisc.c,v 1.27 2007-09-17 19:37:55 qqshka Exp $
*/
// gl_rmisc.c

#ifdef RENDERER_OPTION_CLASSIC_OPENGL

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "tr_types.h"
#include "r_renderer.h"

void GLC_TimeRefresh(void)
{
	int i;
	float start, stop, time;

	if (!GL_FramebufferEnabled2D()) {
#ifndef __APPLE__
		if (glConfig.hardwareType != GLHW_INTEL) {
			// Causes the console to flicker on Intel cards.
			GL_BuiltinProcedure(glDrawBuffer, "mode=GL_FRONT", GL_FRONT)
		}
#endif
	}

	renderer.EnsureFinished();

	start = Sys_DoubleTime();
	for (i = 0; i < 128; i++) {
		r_refdef.viewangles[1] = i * (360.0 / 128.0);
		R_SetupFrame();
		R_RenderView();
	}

	renderer.EnsureFinished();
	stop = Sys_DoubleTime();
	time = stop - start;
	Com_Printf("%f seconds (%f fps)\n", time, 128 / time);

	if (!GL_FramebufferEnabled2D()) {
#ifndef __APPLE__
		if (glConfig.hardwareType != GLHW_INTEL) {
			GL_BuiltinProcedure(glDrawBuffer, "mode=GL_BACK", GL_BACK)
		}
#endif
	}

	R_EndRendering();
}

#endif // #ifdef RENDERER_OPTION_CLASSIC_OPENGL
