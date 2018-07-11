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

glm_polygon_framedata_t polygonData;

void R_Draw_Polygon(int x, int y, vec3_t *vertices, int num_vertices, color_t color)
{
	if (num_vertices < 0 || num_vertices > MAX_POLYGON_POINTS) {
		return;
	}
	if (polygonData.polygonCount >= MAX_POLYGONS_PER_FRAME) {
		return;
	}
	if (!GLM_LogCustomImageType(imagetype_polygon, 0)) {
		return;
	}

	polygonData.polygonVerts[polygonData.polygonCount] = num_vertices;
	COLOR_TO_FLOATVEC_PREMULT(color, polygonData.polygonColor[polygonData.polygonCount]);
	polygonData.polygonX[polygonData.polygonCount] = x;
	polygonData.polygonY[polygonData.polygonCount] = y;
	memcpy(polygonData.polygonVertices, vertices, sizeof(polygonData.polygonVertices[0]) * num_vertices);
	++polygonData.polygonCount;
}
