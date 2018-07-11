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
#include "common_draw.h"
#include "glm_draw.h"
#include "glm_vao.h"
#include "r_state.h"
#include "r_matrix.h"
#include "r_buffers.h"

glm_line_framedata_t lineData;

void GLM_Draw_Line3D(byte* color, vec3_t start, vec3_t end)
{
	if (lineData.lineCount >= MAX_LINES_PER_FRAME) {
		return;
	}
}

void R_Draw_LineRGB(float thickness, byte* color, int x_start, int y_start, int x_end, int y_end)
{
	if (lineData.lineCount >= MAX_LINES_PER_FRAME) {
		return;
	}
	if (!GLM_LogCustomImageType(imagetype_line, lineData.lineCount)) {
		return;
	}

	VectorSet(lineData.line_points[lineData.lineCount * 2 + 0].position, x_start, y_start, 0);
	memcpy(lineData.line_points[lineData.lineCount * 2 + 0].color, color, sizeof(lineData.line_points[lineData.lineCount * 2 + 0].color));
	VectorSet(lineData.line_points[lineData.lineCount * 2 + 1].position, x_end, y_end, 0);
	memcpy(lineData.line_points[lineData.lineCount * 2 + 1].color, color, sizeof(lineData.line_points[lineData.lineCount * 2 + 0].color));
	lineData.line_thickness[lineData.lineCount] = thickness;
	++lineData.lineCount;
}
