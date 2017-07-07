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

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"

void GLC_PolyBlend(float v_blend[4])
{
	glDisable(GL_TEXTURE_2D);

	glColor4fv(v_blend);

	glBegin(GL_QUADS);
	glVertex2f(r_refdef.vrect.x, r_refdef.vrect.y);
	glVertex2f(r_refdef.vrect.x + r_refdef.vrect.width, r_refdef.vrect.y);
	glVertex2f(r_refdef.vrect.x + r_refdef.vrect.width, r_refdef.vrect.y + r_refdef.vrect.height);
	glVertex2f(r_refdef.vrect.x, r_refdef.vrect.y + r_refdef.vrect.height);
	glEnd();

	glEnable(GL_TEXTURE_2D);
	glColor3ubv(color_white);
}

void GLC_BrightenScreen(void)
{
	extern float vid_gamma;
	float f;

	if (vid_hwgamma_enabled)
		return;
	if (v_contrast.value <= 1.0)
		return;

	f = min(v_contrast.value, 3);
	f = pow(f, vid_gamma);

	glDisable(GL_TEXTURE_2D);
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	glBlendFunc(GL_DST_COLOR, GL_ONE);
	glBegin(GL_QUADS);
	while (f > 1) {
		if (f >= 2) {
			glColor3ubv(color_white);
		}
		else {
			glColor3f(f - 1, f - 1, f - 1);
		}

		glVertex2f(0, 0);
		glVertex2f(vid.width, 0);
		glVertex2f(vid.width, vid.height);
		glVertex2f(0, vid.height);

		f *= 0.5;
	}
	glEnd();
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_TEXTURE_2D);
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	glColor3ubv(color_white);
}
