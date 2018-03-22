/*
Copyright (C) 2018 ezQuake team

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

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "tr_types.h"
/*
void GL_EnableFog(void)
{
	if (GL_UseImmediateMode() && gl_fogenable.integer) {
		glEnable(GL_FOG);
	}
}

void GL_DisableFog(void)
{
	if (GL_UseImmediateMode() && gl_fogenable.integer) {
		glDisable(GL_FOG);
	}
}
*/
void GL_ConfigureFog(void)
{
	vec3_t colors;

	if (GL_UseGLSL()) {
		// TODO
		return;
	}

	// START shaman BUG fog was out of control when fogstart>fogend {
	if (gl_fogenable.integer && gl_fogstart.value >= 0 && gl_fogstart.value < gl_fogend.value) {
		// } END shaman BUG fog was out of control when fogstart>fogend
		glFogi(GL_FOG_MODE, GL_LINEAR);
		colors[0] = gl_fogred.value;
		colors[1] = gl_foggreen.value;
		colors[2] = gl_fogblue.value;
		glFogfv(GL_FOG_COLOR, colors);
		glFogf(GL_FOG_START, gl_fogstart.value);
		glFogf(GL_FOG_END, gl_fogend.value);
		glEnable(GL_FOG);
	}
	else {
		glDisable(GL_FOG);
	}
}

void GL_EnableWaterFog(int contents)
{
	extern cvar_t gl_waterfog_color_water;
	extern cvar_t gl_waterfog_color_lava;
	extern cvar_t gl_waterfog_color_slime;
	extern cvar_t gl_waterfog;
	float colors[4];

	if (!gl_waterfog.value || COM_CheckParm(cmdline_param_client_nomultitexturing) || contents == CONTENTS_EMPTY || contents == CONTENTS_SOLID) {
		GL_DisableFog();
		return;
	}

	// TODO
	if (GL_UseGLSL()) {
		return;
	}

	switch (contents) {
		case CONTENTS_LAVA:
			colors[0] = (float)gl_waterfog_color_lava.color[0] / 255.0;
			colors[1] = (float)gl_waterfog_color_lava.color[1] / 255.0;
			colors[2] = (float)gl_waterfog_color_lava.color[2] / 255.0;
			colors[3] = (float)gl_waterfog_color_lava.color[3] / 255.0;
			break;
		case CONTENTS_SLIME:
			colors[0] = (float)gl_waterfog_color_slime.color[0] / 255.0;
			colors[1] = (float)gl_waterfog_color_slime.color[1] / 255.0;
			colors[2] = (float)gl_waterfog_color_slime.color[2] / 255.0;
			colors[3] = (float)gl_waterfog_color_slime.color[3] / 255.0;
			break;
		default:
			colors[0] = (float)gl_waterfog_color_water.color[0] / 255.0;
			colors[1] = (float)gl_waterfog_color_water.color[1] / 255.0;
			colors[2] = (float)gl_waterfog_color_water.color[2] / 255.0;
			colors[3] = (float)gl_waterfog_color_water.color[3] / 255.0;
			break;
	}

	glFogfv(GL_FOG_COLOR, colors);
	if (((int)gl_waterfog.value) == 2) {
		glFogf(GL_FOG_DENSITY, 0.0002 + (0.0009 - 0.0002) * bound(0, gl_waterfog_density.value, 1));
		glFogi(GL_FOG_MODE, GL_EXP);
	}
	else {
		glFogi(GL_FOG_MODE, GL_LINEAR);
		glFogf(GL_FOG_START, 150.0f);
		glFogf(GL_FOG_END, 4250.0f - (4250.0f - 1536.0f) * bound(0, gl_waterfog_density.value, 1));
	}
	glEnable(GL_FOG);
}

// FIXME: Move functions under here
void R_AddWaterfog(int contents)
{
	if (GL_UseImmediateMode()) {
		GL_EnableWaterFog(contents);
	}
}

void R_ConfigureFog(void)
{
	if (GL_UseImmediateMode()) {
		GL_ConfigureFog();
	}
}
