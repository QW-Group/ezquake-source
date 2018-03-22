
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
*/

// Functions taken from cl_screen.c, specific to OpenGL immediate mode

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"

/*
void GLC_DrawAccelBar(int x, int y, int length, int charsize, int pos)
{
	glPushAttrib(GL_TEXTURE_BIT);
	glDisable(GL_TEXTURE_2D);

	// draw the coloured indicator strip
	//Draw_Fill(x, y, length, charsize, 184);
	R_CustomColor(1.f, 1.f, 1.f, 1.0f);
	glBegin(GL_QUADS);
	glVertex2f(x, y);
	glVertex2f(x + length, y);
	glVertex2f(x + length, y + charsize);
	glVertex2f(x, y + charsize);
	glEnd();

	//Draw_Fill(x + length/2 - 2, y, 5, charsize, 152);
	R_CustomColor(0.f, 1.f, 0.f, 1.0f);
	glBegin(GL_QUADS);
	glVertex2f(x + length / 2 - 2, y);
	glVertex2f(x + length / 2 - 2 + 5, y);
	glVertex2f(x + length / 2 - 2 + 5, y + charsize);
	glVertex2f(x + length / 2 - 2, y + charsize);
	glEnd();

	//Draw_Fill(x + pos - 1, y, 3, charsize, 192);
	R_CustomColor(0.f, 0.f, 1.f, 1.0f);
	glBegin(GL_QUADS);
	glVertex2f(x + pos - 1, y);
	glVertex2f(x + pos - 1 + 3, y);
	glVertex2f(x + pos - 1 + 3, y + charsize);
	glVertex2f(x + pos - 1, y + charsize);
	glEnd();

	glPopAttrib();
}
*/