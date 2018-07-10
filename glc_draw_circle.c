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

void GLC_DrawCircles(texture_ref texture, int start, int end)
{
	// FIXME: Not very efficient (but rarely used either)
	int i, j;

	start = max(0, start);
	end = min(end, circleData.circleCount - 1);

	for (i = start; i <= end; ++i) {
		GLC_StateBeginDrawAlphaPieSliceRGB(circleData.drawCircleThickness[i]);
		R_CustomColor(
			circleData.drawCircleColors[i][0],
			circleData.drawCircleColors[i][1],
			circleData.drawCircleColors[i][2],
			circleData.drawCircleColors[i][3]
		);

		glBegin(circleData.drawCircleFill[i] ? GL_TRIANGLE_STRIP : GL_LINE_LOOP);
		for (j = 0; j < circleData.drawCirclePoints[i]; ++j) {
			glVertex2fv(&circleData.drawCirclePointData[i * FLOATS_PER_CIRCLE + j * 2]);
		}
		glEnd();
	}
}
