/*
Copyright (C) 2017 ezQuake team

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
#include "wad.h"
#include "stats_grid.h"
#include "utils.h"
#include "sbar.h"
#include "common_draw.h"
#ifndef __APPLE__
#include "tr_types.h"
#endif
#include "glm_draw.h"

#define MAX_2D_ELEMENTS 4096

extern float overall_alpha;

typedef struct draw_hud_element_s {
	glm_image_type_t type;
	texture_ref texture;
	int index;
} draw_hud_element_t;

static draw_hud_element_t elements[MAX_2D_ELEMENTS];
static int hudElementCount = 0;

void GLM_DrawCircles(int start, int end);
void GLM_DrawLines(int start, int end);
void GLM_DrawPolygons(int start, int end);
void GLM_DrawImageArraySequence(texture_ref texture, int start, int length);

void GLC_DrawCircles(int start, int end);
void GLC_DrawLines(int start, int end);
void GLC_DrawPolygons(int start, int end);
void GLC_DrawImageArraySequence(texture_ref texture, int start, int length);

void GLM_DrawRectangle(float x, float y, float width, float height, byte* color);

void GLM_DrawAlphaRectangleRGB(int x, int y, int w, int h, float thickness, qbool fill, byte* bytecolor)
{
	if (fill) {
		GLM_DrawRectangle(x, y, w, h, bytecolor);
	}
	else {
		GLM_DrawRectangle(x, y, w, thickness, bytecolor);
		GLM_DrawRectangle(x, y + thickness, thickness, h - thickness, bytecolor);
		GLM_DrawRectangle(x + w - thickness, y + thickness, thickness, h - thickness, bytecolor);
		GLM_DrawRectangle(x, y + h, w, thickness, bytecolor);
	}
}

void GLM_Draw_SAlphaSubPic2(int x, int y, mpic_t *pic, int src_width, int src_height, float newsl, float newtl, float newsh, float newth, float scale_x, float scale_y, float alpha)
{
	byte color[] = { 255, 255, 255, alpha * 255 };

	GLM_DrawImage(x, y, scale_x * src_width, scale_y * src_height, newsl, newtl, newsh - newsl, newth - newtl, color, false, pic->texnum, false);
}

void GLM_Draw_FadeScreen(float alpha)
{
	Draw_AlphaRectangleRGB(0, 0, vid.width, vid.height, 0.0f, true, RGBA_TO_COLOR(0, 0, 0, (alpha < 1 ? alpha * 255 : 255)));
}

void GLM_PrepareImageDraw(void)
{
	GLM_PrepareImages();
	GLM_PreparePolygons();
	GLM_PrepareCircles();
	GLM_PrepareLines();
}

static void GLM_DrawHudElements(glm_image_type_t type, texture_ref texture, int start, int end)
{
	switch (type) {
		case imagetype_image:
			if (GL_ShadersSupported()) {
				GLM_DrawImageArraySequence(texture, elements[start].index, elements[end].index);
			}
			else {
				GLC_DrawImageArraySequence(texture, elements[start].index, elements[end].index);
			}
			break;
		case imagetype_circle:
			if (GL_ShadersSupported()) {
				GLM_DrawCircles(elements[start].index, elements[end].index);
			}
			else {
				GLC_DrawCircles(elements[start].index, elements[end].index);
			}
			break;
		case imagetype_line:
			if (GL_ShadersSupported()) {
				GLM_DrawLines(elements[start].index, elements[end].index);
			}
			else {
				GLC_DrawLines(elements[start].index, elements[end].index);
			}
			break;
		case imagetype_polygon:
			if (GL_ShadersSupported()) {
				GLM_DrawPolygons(elements[start].index, elements[end].index);
			}
			else {
				GLC_DrawPolygons(elements[start].index, elements[end].index);
			}
			break;
	}
}

static void GLM_FlushImageDraw(void)
{
	if (hudElementCount && glConfig.initialized) {
		texture_ref currentTexture = null_texture_reference;
		glm_image_type_t type = imagetype_image;
		int start = 0;
		int i;

		for (i = 0; i < hudElementCount; ++i) {
			qbool texture_changed = (GL_TextureReferenceIsValid(currentTexture) && GL_TextureReferenceIsValid(elements[i].texture) && !GL_TextureReferenceEqual(currentTexture, elements[i].texture));

			if (i && (elements[i].type != type || texture_changed)) {
				GLM_DrawHudElements(type, currentTexture, start, i - 1);

				start = i;
			}

			if (GL_TextureReferenceIsValid(elements[i].texture)) {
				currentTexture = elements[i].texture;
			}
			type = elements[i].type;
		}

		if (hudElementCount - start) {
			GLM_DrawHudElements(type, currentTexture, start, hudElementCount - 1);
		}
	}
}

void GL_EmptyImageQueue(void)
{
	hudElementCount = imageData.imageCount = circleData.circleCount = lineData.lineCount = polygonData.polygonCount = 0;
}

void GL_FlushImageDraw(void)
{
	GLM_FlushImageDraw();

	GL_EmptyImageQueue();
}

qbool GLM_LogCustomImageType(glm_image_type_t type, int index)
{
	if (hudElementCount >= MAX_2D_ELEMENTS) {
		return false;
	}

	elements[hudElementCount].type = type;
	elements[hudElementCount].index = index;
	hudElementCount++;
	return true;
}

qbool GLM_LogCustomImageTypeWithTexture(glm_image_type_t type, int index, texture_ref texture)
{
	if (hudElementCount >= MAX_2D_ELEMENTS) {
		return false;
	}

	elements[hudElementCount].type = type;
	elements[hudElementCount].index = index;
	elements[hudElementCount].texture = texture;
	hudElementCount++;
	return true;
}
