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

void GLC_DrawTileClear(int texnum, int x, int y, int w, int h)
{
	GL_Bind(texnum);
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

void GLC_Draw_LineRGB(byte* bytecolor, int x_start, int y_start, int x_end, int y_end)
{
	glColor4ub(bytecolor[0], bytecolor[1], bytecolor[2], bytecolor[3] * overall_alpha);
	glBegin(GL_LINES);
	glVertex2f(x_start, y_start);
	glVertex2f(x_end, y_end);
	glEnd();
	glColor3ubv(color_white);
}

void GLC_Draw_Polygon(int x, int y, vec3_t *vertices, int num_vertices, qbool fill, color_t color)
{
	byte bytecolor[4];
	int i = 0;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);

	COLOR_TO_RGBA(color, bytecolor);
	glColor4ub(bytecolor[0], bytecolor[1], bytecolor[2], bytecolor[3] * overall_alpha);

	glDisable(GL_TEXTURE_2D);

	if (fill) {
		glBegin(GL_TRIANGLE_FAN);
	}
	else {
		glBegin(GL_LINE_LOOP);
	}

	for (i = 0; i < num_vertices; i++) {
		glVertex2f(x + vertices[i][0], y + vertices[i][1]);
	}

	glEnd();

	glPopAttrib();
}
