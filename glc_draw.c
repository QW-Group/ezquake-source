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

void GLC_DrawImage(float x, float y, float ofs1, float ofs2, float sl, float tl, float sh, float th)
{
	glBegin(GL_QUADS);
	glTexCoord2f(sl, tl);
	glVertex2f(x - ofs1, y - ofs1);
	glTexCoord2f(sh, tl);
	glVertex2f(x + ofs2, y - ofs1);
	glTexCoord2f(sh, th);
	glVertex2f(x + ofs2, y + ofs2);
	glTexCoord2f(sl, th);
	glVertex2f(x - ofs1, y + ofs2);
	glEnd();
	glColor3ubv (color_white);
}

void GLC_Draw_AlphaPieSliceRGB(int x, int y, float radius, float startangle, float endangle, float thickness, qbool fill, color_t color)
{
	byte bytecolor[4];
	double angle;
	int i;
	int start;
	int end;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glDisable(GL_TEXTURE_2D);

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
	COLOR_TO_RGBA(color, bytecolor);
	glColor4ub(bytecolor[0], bytecolor[1], bytecolor[2], bytecolor[3] * overall_alpha);

	if (thickness > 0.0) {
		glLineWidth(thickness);
	}

	if (fill) {
		glBegin(GL_TRIANGLE_STRIP);
	}
	else {
		glBegin(GL_LINE_LOOP);
	}

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

	glEnable(GL_TEXTURE_2D);

	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
	glColor4ubv(color_white);

	glPopAttrib();
}

void GLC_Draw_SAlphaSubPic2(int x, int y, mpic_t *pic, int src_width, int src_height, float newsl, float newtl, float newsh, float newth, float scale_x, float scale_y, float alpha)
{
	if (alpha < 1.0) {
		GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_TextureEnvMode(GL_MODULATE);
		GL_CullFace(GL_FRONT);
		glColor4f(1, 1, 1, alpha);
	}

	GL_Bind(pic->texnum);

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

	if (alpha < 1.0) {
		GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
		GL_TextureEnvMode(GL_REPLACE);
		glColor4f(1, 1, 1, 1);
	}
}

void GLC_DrawAlphaRectangeRGB(int x, int y, int w, int h, float thickness, qbool fill, byte* bytecolor)
{
	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
	glDisable (GL_TEXTURE_2D);
	glColor4ub(bytecolor[0], bytecolor[1], bytecolor[2], bytecolor[3] * overall_alpha);
	if (fill) {
		glRectf(x, y, x + w, y + h);
	}
	else {
		glRectf(x, y, x + w, y + thickness);
		glRectf(x, y + thickness, x + thickness, y + h - thickness);
		glRectf(x + w - thickness, y + thickness, x + w, y + h - thickness);
		glRectf(x, y + h, x + w, y + h - thickness);
	}
	glColor4ubv (color_white);
	glEnable (GL_TEXTURE_2D);
	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
}

void GLC_Draw_FadeScreen(float alpha)
{
	if (alpha < 1) {
		GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
		glColor4f(0, 0, 0, alpha);
	}
	else {
		glColor3f(0, 0, 0);
	}

	glDisable(GL_TEXTURE_2D);

	glBegin(GL_QUADS);
	glVertex2f(0, 0);
	glVertex2f(vid.width, 0);
	glVertex2f(vid.width, vid.height);
	glVertex2f(0, vid.height);
	glEnd();

	glColor3ubv(color_white);
	glEnable(GL_TEXTURE_2D);

	if (alpha < 1) {
		GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
	}
}
