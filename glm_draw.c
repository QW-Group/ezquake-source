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

void GLM_PreparePolygon(void);
void GLM_PrepareLineProgram(void);
void GLM_PrepareImages(void);

void GLM_PrepareCircleDraw(void);
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
	byte color[] = { 255, 255, 255, 255 };
	if (alpha < 1.0) {
		color[0] = color[1] = color[2] = color[3] = alpha * 255;
	}

	GLM_DrawImage(x, y, scale_x * src_width, scale_y * src_height, newsl, newtl, newsh - newsl, newth - newtl, color, false, pic->texnum, false);
}

void GLM_Draw_FadeScreen(float alpha)
{
	Draw_AlphaRectangleRGB(0, 0, vid.width, vid.height, 0.0f, true, RGBA_TO_COLOR(0, 0, 0, (alpha < 1 ? alpha * 255 : 255)));
}

void GLM_PrepareImageDraw(void)
{
	GLM_PrepareImages();
	GLM_PreparePolygon();
	GLM_PrepareCircleDraw();
	GLM_PrepareLineProgram();
}

static void GLM_DrawHudElements(glm_image_type_t type, texture_ref texture, int start, int end)
{
	switch (type) {
		case imagetype_image:
			GLM_DrawImageArraySequence(texture, elements[start].index, elements[end].index);
			break;
		case imagetype_circle:
			GLM_DrawCircles(elements[start].index, elements[end].index);
			break;
		case imagetype_line:
			GLM_DrawLines(elements[start].index, elements[end].index);
			break;
		case imagetype_polygon:
			GLM_DrawPolygons(elements[start].index, elements[end].index);
			break;
	}
}

static void GLM_FlushImageDraw(void)
{
	if (hudElementCount && glConfig.initialized) {
		texture_ref currentTexture = null_texture_reference;
		glm_image_type_t type;
		int start = 0;
		int i;

		for (i = 0; i < hudElementCount; ++i) {
			qbool texture_changed = (GL_TextureReferenceIsValid(currentTexture) && GL_TextureReferenceIsValid(elements[i].texture) && !GL_TextureReferenceEqual(currentTexture, elements[i].texture));

			if (i && elements[i].type != type || texture_changed) {
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

static void GLC_FlushImageDraw(void)
{

}
/*
static void GLC_FlushImageDraw(void)
{
	if (imageCount) {
		int i, j;
		extern cvar_t gl_alphafont, scr_coloredText;
		float modelviewMatrix[16];
		float projectionMatrix[16];
		byte current_color[4];

		GL_PushMatrix(GL_MODELVIEW, modelviewMatrix);
		GL_PushMatrix(GL_PROJECTION, projectionMatrix);

		GL_IdentityModelView();
		GL_IdentityProjectionView();

		GLC_StateBeginImageDraw();

		if (GL_BuffersSupported()) {
			GL_BindVertexArray(NULL);
			GL_UnBindBuffer(GL_ELEMENT_ARRAY_BUFFER);

			if (!GL_BufferReferenceIsValid(imageVBO)) {
				imageVBO = GL_GenFixedBuffer(GL_ARRAY_BUFFER, "image-vbo", sizeof(glc_images), glc_images, GL_STREAM_DRAW);
				GL_BindBuffer(imageVBO);
			}
			else {
				GL_BindAndUpdateBuffer(imageVBO, imageCount * 4 * sizeof(glc_images[0]), glc_images);
			}

			glVertexPointer(2, GL_FLOAT, sizeof(glc_image_t), VBO_FIELDOFFSET(glc_image_t, pos));
			glEnableClientState(GL_VERTEX_ARRAY);

			glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(glc_image_t), VBO_FIELDOFFSET(glc_image_t, colour));
			glEnableClientState(GL_COLOR_ARRAY);

			qglClientActiveTexture(GL_TEXTURE0);
			glTexCoordPointer(2, GL_FLOAT, sizeof(glc_image_t), VBO_FIELDOFFSET(glc_image_t, tex));
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}

		for (i = 0; i < imageCount; ++i) {
			qbool alpha_test = images[i].flags & IMAGEPROG_FLAGS_ALPHATEST;
			qbool texture = images[i].flags & IMAGEPROG_FLAGS_TEXTURE;
			qbool text = images[i].flags & IMAGEPROG_FLAGS_TEXT;

			GLC_EnsureTMUEnabled(GL_TEXTURE0);
			GL_EnsureTextureUnitBound(GL_TEXTURE0, images[i].texNumber);

			if (text) {
				alpha_test = !gl_alphafont.integer;

				if (scr_coloredText.integer) {
					GL_TextureEnvMode(GL_MODULATE);
				}
				else {
					GL_TextureEnvMode(GL_REPLACE);
				}
			}
			else {
				GL_TextureEnvMode(GL_MODULATE);
			}

			GL_AlphaBlendFlags(alpha_test ? GL_ALPHATEST_ENABLED : GL_ALPHATEST_DISABLED);
			GL_Color4ubv(images[i].colour);
			memcpy(current_color, images[i].colour, sizeof(current_color));

			for (j = i; j < imageCount; ++j) {
				glm_image_t* next = &images[j];
				qbool next_alpha_test = next->flags & IMAGEPROG_FLAGS_ALPHATEST;
				qbool next_texture = next->flags & IMAGEPROG_FLAGS_TEXTURE;
				qbool next_text = next->flags & IMAGEPROG_FLAGS_TEXT;

				if (next_text) {
					next_alpha_test = !gl_alphafont.integer;
					if (!text && !scr_coloredText.integer) {
						break; // will need to toggle TMU
					}
				}

				if (next_alpha_test != alpha_test) {
					GL_MarkEvent("(break for alpha-test)");
					break;
				}
				if (next_texture != texture) {
					GL_MarkEvent("(break for texturing-toggle)");
					break;
				}
				if (next_texture && !GL_TextureReferenceEqual(images[i].texNumber, next->texNumber)) {
					GL_MarkEvent("(break for texture)");
					break;
				}
			}

			if (GL_BuffersSupported()) {
				GL_DrawArrays(GL_QUADS, i * 4, (j - i) * 4);

				i = j - 1;
			}
			else {
				glBegin(GL_QUADS);

				while (i < j) {
					glm_image_t* next = &images[i];

					// Don't need to break for colour
					if (memcmp(next->colour, current_color, sizeof(current_color))) {
						memcpy(current_color, next->colour, sizeof(current_color));
						GL_Color4ubv(next->colour);
					}

					if (texture) {
						glTexCoord2f(next->s1, next->t2);
					}
					glVertex2f(next->x1, next->y2);
					if (texture) {
						glTexCoord2f(next->s1, next->t1);
					}
					glVertex2f(next->x1, next->y1);
					if (texture) {
						glTexCoord2f(next->s2, next->t1);
					}
					glVertex2f(next->x2, next->y1);
					if (texture) {
						glTexCoord2f(next->s2, next->t2);
					}
					glVertex2f(next->x2, next->y2);
				}

				--i;
				glEnd();
			}
		}

		GL_PopMatrix(GL_PROJECTION, projectionMatrix);
		GL_PopMatrix(GL_MODELVIEW, modelviewMatrix);

		if (GL_BuffersSupported()) {
			glDisableClientState(GL_VERTEX_ARRAY);
			glDisableClientState(GL_COLOR_ARRAY);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}
	}
}
*/

void GL_EmptyImageQueue(void)
{
	hudElementCount = imageData.imageCount = circleData.circleCount = lineData.lineCount = polygonData.polygonCount = 0;
}

void GL_FlushImageDraw(void)
{
	if (GL_ShadersSupported()) {
		GLM_FlushImageDraw();
	}
	else {
		GLC_FlushImageDraw();
	}

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
