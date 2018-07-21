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

void GLC_ConfigureFog(int contents)
{
	// Configure fog
	qbool fog_valid = gl_fogstart.value >= 0 && gl_fogstart.value < gl_fogend.value;
	qbool underwater = ISUNDERWATER(contents);
	qbool fog_enabled_by_user = (underwater && gl_waterfog.integer) || (!underwater && gl_fogenable.integer);

	r_refdef2.fog_enabled = (fog_valid && fog_enabled_by_user);

	if (r_refdef2.fog_enabled) {
		if (underwater) {
			extern cvar_t gl_waterfog_color_water;
			extern cvar_t gl_waterfog_color_lava;
			extern cvar_t gl_waterfog_color_slime;
			extern cvar_t gl_waterfog;
			float colors[4];

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
			if (gl_waterfog.integer == 2) {
				glFogf(GL_FOG_DENSITY, 0.0002 + (0.0009 - 0.0002) * bound(0, gl_waterfog_density.value, 1));
				glFogi(GL_FOG_MODE, GL_EXP);
			}
			else {
				glFogi(GL_FOG_MODE, GL_LINEAR);
				glFogf(GL_FOG_START, 150.0f);
				glFogf(GL_FOG_END, 4250.0f - (4250.0f - 1536.0f) * bound(0, gl_waterfog_density.value, 1));
			}
		}
		else {
			vec3_t colors;

			glFogi(GL_FOG_MODE, GL_LINEAR);
			colors[0] = gl_fogred.value;
			colors[1] = gl_foggreen.value;
			colors[2] = gl_fogblue.value;
			glFogfv(GL_FOG_COLOR, colors);
			glFogf(GL_FOG_START, gl_fogstart.value);
			glFogf(GL_FOG_END, gl_fogend.value);
		}
	}
}
