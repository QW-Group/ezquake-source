/*
Copyright (C) 2017-2018 ezQuake team

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
#include "glm_draw.h"
#include "r_state.h"

void GLC_DrawLines(texture_ref texture, int start, int end)
{
	int i;

	for (i = start; i <= end; ++i) {
		GL_StateBeginAlphaLineRGB(lineData.line_thickness[i]);
		glBegin(GL_LINES);
		R_CustomColor4ubv(lineData.line_points[i * 2 + 0].color);
		glVertex3fv(lineData.line_points[i * 2 + 0].position);
		R_CustomColor4ubv(lineData.line_points[i * 2 + 1].color);
		glVertex3fv(lineData.line_points[i * 2 + 1].position);
		glEnd();
	}
}
