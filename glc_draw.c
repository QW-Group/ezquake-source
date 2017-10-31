/*
Copyright (C) 1996-1997 Id Software, Inc.

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

$Id: gl_draw.c,v 1.104 2007-10-18 05:28:23 dkure Exp $
*/

#include "quakedef.h"
#include <time.h>
#include "vx_tracker.h"
#include "gl_model.h"
#include "gl_local.h"

extern float overall_alpha;
#define CIRCLE_LINE_COUNT	40

#if 0

void GLC_DrawTileClear(texture_ref texnum, int x, int y, int w, int h)
{
	GL_BindTextureUnit(GL_TEXTURE0, texnum);

	glBegin(GL_QUADS);
	glTexCoord2f(x / 64.0, y / 64.0);
	glVertex2f(x, y);
	glTexCoord2f((x + w) / 64.0, y / 64.0);
	glVertex2f(x + w, y);
	glTexCoord2f((x + w) / 64.0, (y + h) / 64.0);
	glVertex2f(x + w, y + h);
	glTexCoord2f(x / 64.0, (y + h) / 64.0);
	glVertex2f(x, y + h);
	glEnd();
}

void GLC_DrawImage(float x, float y, float width, float height, float sl, float tl, float tex_width, float tex_height, byte* color, qbool alpha, texture_ref texnum, qbool isText)
{
	GLC_StateBeginDrawImage(alpha, color);

	GL_BindTextureUnit(GL_TEXTURE0, texnum);

	glBegin(GL_QUADS);
	glTexCoord2f(sl, tl);
	glVertex2f(x, y);
	glTexCoord2f(sl + tex_width, tl);
	glVertex2f(x + width, y);
	glTexCoord2f(sl + tex_width, tl + tex_height);
	glVertex2f(x + width, y + height);
	glTexCoord2f(sl, tl + tex_height);
	glVertex2f(x, y + height);
	glEnd();

	GLC_StateEndDrawImage();
}

void GLC_Draw_SAlphaSubPic2(int x, int y, mpic_t *pic, int src_width, int src_height, float newsl, float newtl, float newsh, float newth, float scale_x, float scale_y, float alpha)
{
	GLC_StateBeginAlphaPic(alpha);

	GL_BindTextureUnit(GL_TEXTURE0, pic->texnum);

	glBegin(GL_QUADS);
	{
		// Upper left corner.
		glTexCoord2f(newsl, newtl);
		glVertex2f(x, y);

		// Upper right corner.
		glTexCoord2f(newsh, newtl);
		glVertex2f(x + (scale_x * src_width), y);

		// Bottom right corner.
		glTexCoord2f(newsh, newth);
		glVertex2f(x + (scale_x * src_width), y + (scale_y * src_height));

		// Bottom left corner.
		glTexCoord2f(newsl, newth);
		glVertex2f(x, y + (scale_y * src_height));
	}
	glEnd();

	GLC_StateEndAlphaPic(alpha);
}

void GLC_Draw_FadeScreen(float alpha)
{
	GLC_StateBeginFadeScreen(alpha);

	glBegin(GL_QUADS);
	glVertex2f(0, 0);
	glVertex2f(vid.width, 0);
	glVertex2f(vid.width, vid.height);
	glVertex2f(0, vid.height);
	glEnd();

	GLC_StateEndFadeScreen();
}
#else

#endif

void GLC_Draw_Polygon(int x, int y, vec3_t *vertices, int num_vertices, color_t color)
{
	byte bytecolor[4];
	int i = 0;
	int oldFlags = GL_AlphaBlendFlags(GL_ALPHATEST_NOCHANGE | GL_BLEND_NOCHANGE);

	GLC_StateBeginDrawPolygon();

	COLOR_TO_RGBA(color, bytecolor);
	glColor4ub(bytecolor[0], bytecolor[1], bytecolor[2], bytecolor[3] * overall_alpha);
	glBegin(GL_TRIANGLE_STRIP);
	for (i = 0; i < num_vertices; i++) {
		glVertex2f(x + vertices[i][0], y + vertices[i][1]);
	}
	glEnd();

	GLC_StateEndDrawPolygon(oldFlags);
}

void GLC_Draw_LineRGB(byte* bytecolor, int x_start, int y_start, int x_end, int y_end)
{
	glColor4ub(bytecolor[0], bytecolor[1], bytecolor[2], bytecolor[3] * overall_alpha);
	glBegin(GL_LINES);
	glVertex2f(x_start, y_start);
	glVertex2f(x_end, y_end);
	glEnd();
	glColor3ubv(color_white);
}

void GLC_Draw_AlphaPieSliceRGB(int x, int y, float radius, float startangle, float endangle, float thickness, qbool fill, color_t color)
{
	byte bytecolor[4];
	double angle;
	int i;
	int start;
	int end;

	GLC_StateBeginDrawAlphaPieSliceRGB(thickness);

	COLOR_TO_RGBA(color, bytecolor);
	glColor4ub(bytecolor[0], bytecolor[1], bytecolor[2], bytecolor[3] * overall_alpha);

	glBegin(fill ? GL_TRIANGLE_STRIP : GL_LINE_LOOP);

	// Get the vertex index where to start and stop drawing.
	start = Q_rint((startangle * CIRCLE_LINE_COUNT) / (2 * M_PI));
	end = Q_rint((endangle   * CIRCLE_LINE_COUNT) / (2 * M_PI));

	// If the end is less than the start, increase the index so that
	// we start on a "new" circle.
	if (end < start) {
		start = start + CIRCLE_LINE_COUNT;
	}

	// Create a vertex at the exact position specified by the start angle.
	glVertex2f(x + radius * cos(startangle), y - radius * sin(startangle));

	// TODO: Use lookup table for sin/cos?
	for (i = start; i < end; i++) {
		angle = (i * 2 * M_PI) / CIRCLE_LINE_COUNT;
		glVertex2f(x + radius * cos(angle), y - radius * sin(angle));

		// When filling we're drawing triangles so we need to
		// create a vertex in the middle of the vertex to fill
		// the entire pie slice/circle.
		if (fill) {
			glVertex2f(x, y);
		}
	}

	glVertex2f(x + radius * cos(endangle), y - radius * sin(endangle));

	// Create a vertex for the middle point if we're not drawing a complete circle.
	if (endangle - startangle < 2 * M_PI) {
		glVertex2f(x, y);
	}

	glEnd();

	GLC_StateEndDrawAlphaPieSliceRGB(thickness);
}
