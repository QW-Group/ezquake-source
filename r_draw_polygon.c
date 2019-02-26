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

void R_Draw_Polygon(float x, float y, vec3_t *vertices, int num_vertices, color_t color)
{
	int i;
	texture_ref texture;
	float texCoords[2];

	if (polygonData.polygonCount >= MAX_POLYGONS_PER_FRAME) {
		return;
	}
	if (num_vertices <= 2 || num_vertices >= 4 * (MAX_MULTI_IMAGE_BATCH - imageData.imageCount)) {
		return;
	}

	Atlas_SolidTextureCoordinates(&texture, &texCoords[0], &texCoords[1]);

	if (!R_LogCustomImageTypeWithTexture(imagetype_polygon, polygonData.polygonCount, texture)) {
		return;
	}

	polygonData.polygonVerts[polygonData.polygonCount] = num_vertices;
	polygonData.polygonImageIndexes[polygonData.polygonCount] = imageData.imageCount * 4;

	for (i = 0; i < num_vertices; ++i) {
		extern float cachedMatrix[16];
		float v1[4] = { vertices[i][0], vertices[i][1], vertices[i][2], 1 };
		glm_image_t* img = &imageData.images[imageData.imageCount * 4 + i];

		R_MultiplyVector(cachedMatrix, v1, v1);

		COLOR_TO_RGBA_PREMULT(color, img->colour);
		img->tex[0] = texCoords[0];
		img->tex[1] = texCoords[1];
		img->pos[0] = v1[0];
		img->pos[1] = v1[1];
	}
	imageData.imageCount += (num_vertices + 3) / 4;
	++polygonData.polygonCount;
}
