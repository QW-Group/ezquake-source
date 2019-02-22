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

void R_Draw_LineRGB(float thickness, byte* color, float x_start, float y_start, float x_end, float y_end)
{
	extern float cachedMatrix[16];
	float v1[4] = { x_start, y_start, 0, 1 };
	float v2[4] = { x_end, y_end, 0, 1 };

	if (lineData.lineCount >= MAX_LINES_PER_FRAME) {
		return;
	}
	if (!R_LogCustomImageType(imagetype_line, lineData.lineCount)) {
		return;
	}

	R_MultiplyVector(cachedMatrix, v1, v1);
	R_MultiplyVector(cachedMatrix, v2, v2);

	VectorSet(lineData.line_points[lineData.lineCount * 2 + 0].position, v1[0], v1[1], 0);
	memcpy(lineData.line_points[lineData.lineCount * 2 + 0].color, color, sizeof(lineData.line_points[lineData.lineCount * 2 + 0].color));
	VectorSet(lineData.line_points[lineData.lineCount * 2 + 1].position, v2[0], v2[1], 0);
	memcpy(lineData.line_points[lineData.lineCount * 2 + 1].color, color, sizeof(lineData.line_points[lineData.lineCount * 2 + 0].color));
	lineData.line_thickness[lineData.lineCount] = thickness;
	++lineData.lineCount;
}
