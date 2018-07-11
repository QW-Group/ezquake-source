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
#include "utils.h"
#include "common_draw.h"
#include "glm_draw.h"
#include "glm_vao.h"
#include "r_state.h"
#include "r_matrix.h"
#include "r_buffers.h"

static buffer_ref circleVBO;

glm_circle_framedata_t circleData;

extern float overall_alpha;

void R_Draw_AlphaPieSliceRGB(int x, int y, float radius, float startangle, float endangle, float thickness, qbool fill, color_t color)
{
	float* pointData;
	double angle;
	byte color_bytes[4];
	int i;
	int start;
	int end;
	int points;

	if (circleData.circleCount >= CIRCLES_PER_FRAME) {
		return;
	}
	if (!GLM_LogCustomImageType(imagetype_circle, circleData.circleCount)) {
		return;
	}

	// Get the vertex index where to start and stop drawing.
	start = Q_rint((startangle * CIRCLE_LINE_COUNT) / (2 * M_PI));
	end = Q_rint((endangle * CIRCLE_LINE_COUNT) / (2 * M_PI));

	// If the end is less than the start, increase the index so that
	// we start on a "new" circle.
	if (end < start) {
		end = end + CIRCLE_LINE_COUNT;
	}

	points = 0;
	pointData = circleData.drawCirclePointData + (FLOATS_PER_CIRCLE * circleData.circleCount);
	COLOR_TO_RGBA(color, color_bytes);
	circleData.drawCircleColors[circleData.circleCount][0] = (color_bytes[0] / 255.0f) * overall_alpha;
	circleData.drawCircleColors[circleData.circleCount][1] = (color_bytes[1] / 255.0f) * overall_alpha;
	circleData.drawCircleColors[circleData.circleCount][2] = (color_bytes[2] / 255.0f) * overall_alpha;
	circleData.drawCircleColors[circleData.circleCount][3] = (color_bytes[3] / 255.0f) * overall_alpha;
	circleData.drawCircleFill[circleData.circleCount] = fill;
	circleData.drawCircleThickness[circleData.circleCount] = thickness;
	++circleData.circleCount;

	// Create a vertex at the exact position specified by the start angle.
	pointData[points * 2 + 0] = x + radius * cos(startangle);
	pointData[points * 2 + 1] = y - radius * sin(startangle);
	++points;

	// TODO: Use lookup table for sin/cos?
	for (i = start; i < end; i++) {
		angle = (i * 2 * M_PI) / CIRCLE_LINE_COUNT;
		pointData[points * 2 + 0] = x + radius * cos(angle);
		pointData[points * 2 + 1] = y - radius * sin(angle);
		++points;

		// When filling we're drawing triangles so we need to
		// create a vertex in the middle of the vertex to fill
		// the entire pie slice/circle.
		if (fill) {
			pointData[points * 2 + 0] = x;
			pointData[points * 2 + 1] = y;
			++points;
		}
	}

	pointData[points * 2 + 0] = x + radius * cos(endangle);
	pointData[points * 2 + 1] = y - radius * sin(endangle);
	++points;

	// Create a vertex for the middle point if we're not drawing a complete circle.
	if (endangle - startangle < 2 * M_PI) {
		pointData[points * 2 + 0] = x;
		pointData[points * 2 + 1] = y;
		++points;
	}

	circleData.drawCirclePoints[circleData.circleCount - 1] = points;
}
