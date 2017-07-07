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

extern float bubblecolor[NUM_DLIGHTTYPES][4];
extern float bubble_sintable[17], bubble_costable[17];

void GLC_RenderDlight(dlight_t* light)
{
	int i, j;
	vec3_t v, v_right, v_up;
	float length, rad, *bub_sin, *bub_cos;

	rad = light->radius * 0.35;
	VectorSubtract(light->origin, r_origin, v);
	length = VectorNormalize(v);

	if (length < rad) {
		// view is inside the dlight
		V_AddLightBlend(1, 0.5, 0, light->radius * 0.0003);
		return;
	}

	glBegin(GL_TRIANGLE_FAN);
	if (light->type == lt_custom) {
		glColor3ubv(light->color);
	}
	else {
		glColor3fv(bubblecolor[light->type]);
	}

	VectorVectors(v, v_right, v_up);

	if (length - rad > 8) {
		VectorScale(v, rad, v);
	}
	else {
		// make sure the light bubble will not be clipped by near z clip plane
		VectorScale(v, length - 8, v);
	}

	VectorSubtract(light->origin, v, v);

	glVertex3fv(v);
	glColor3ubv(color_black);

	bub_sin = bubble_sintable;
	bub_cos = bubble_costable;

	for (i = 16; i >= 0; i--) {
		for (j = 0; j < 3; j++)
			v[j] = light->origin[j] + (v_right[j] * (*bub_cos) +
				+v_up[j] * (*bub_sin)) * rad;
		bub_sin++;
		bub_cos++;
		glVertex3fv(v);
	}

	glEnd();
}
