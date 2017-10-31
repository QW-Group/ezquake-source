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

	GL_FlushImageDraw(true);

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
