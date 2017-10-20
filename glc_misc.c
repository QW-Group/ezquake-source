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

// motion blur.
int sceneblur_texture;

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
	GL_BlendFunc(GL_DST_COLOR, GL_ONE);
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
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_TEXTURE_2D);
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	glColor3ubv(color_white);
}

void GLC_DrawVelocity3D(void)
{
	extern cvar_t show_velocity_3d_offset_forward;
	extern cvar_t show_velocity_3d_offset_down;
	extern cvar_t show_velocity_3d;

	vec3_t *origin = &r_refdef.vieworg;
	vec3_t *angles = &r_refdef.viewangles;

	const float vx = cl.simvel[0];
	const float vy = cl.simvel[1];
	const float vz = cl.simvel[2];

	const float yaw_degrees = (*angles)[YAW];
	const float yaw = DEG2RAD(yaw_degrees);

	const double c = cos(yaw);
	const double s = sin(yaw);

	const double scale_factor = 0.04;
	const float v_side = (float) (scale_factor * (-vx * s + vy * c));
	const float v_forward = (float) (scale_factor * (vx * c + vy * s));
	const float v_up = (float) (scale_factor * vz);

	const float line_width = 10.f;
	const float stipple_line_width = 5.f;
	const float stipple_line_colour[3] = { 0.5f, 0.5f, 0.5f };
	const vec3_t v3_zero = {0.f, 0.f, 0.f };
	float oldMatrix[16];

	GL_PushMatrix(GL_MODELVIEW, oldMatrix);

	GL_Translate(GL_MODELVIEW, (*origin)[0], (*origin)[1], (*origin)[2]);
	GL_Rotate(GL_MODELVIEW, yaw_degrees, 0.f, 0.f, 1.f);
	GL_Translate(GL_MODELVIEW, show_velocity_3d_offset_forward.value, 0.f, -show_velocity_3d_offset_down.value);

	glPushAttrib(GL_LINE_BIT | GL_TEXTURE_BIT);
	glDisable(GL_TEXTURE_2D);
	switch (show_velocity_3d.integer)
	{
	case 1:
		//show vertical
		glEnable(GL_LINE_STIPPLE);
		glLineStipple(1, 0xFF00);
		glLineWidth(stipple_line_width);

		glColor3fv(stipple_line_colour);
		glBegin(GL_LINES);
		glVertex3f(v_forward, v_side, 0.f);
		glVertex3f(v_forward, v_side, v_up);
		glEnd();

		glDisable(GL_LINE_STIPPLE);
		glLineWidth(line_width);
		glColor3f(0.f, 1.f, 0.f);

		glBegin(GL_LINES);
		glVertex3fv(v3_zero);
		glVertex3f(v_forward, v_side, v_up);
		glEnd();
		//no break here

	case 2:
		//show horizontal velocity only
		glColor3f(1.f, 0.f, 0.f);
		glLineWidth(line_width);
		glBegin(GL_LINES);
		glVertex3fv(v3_zero);
		glVertex3f(v_forward, v_side, 0.f);
		glEnd();

		glEnable(GL_LINE_STIPPLE);
		glLineStipple(1, 0xFF00);
		glColor3fv(stipple_line_colour);
		glLineWidth(stipple_line_width);

		glBegin(GL_LINE_LOOP);
		glVertex3fv(v3_zero);
		glVertex3f(0.f, v_side, 0.f);
		glVertex3f(v_forward, v_side, 0.f);
		glVertex3f(v_forward, 0.f, 0.f);
		glEnd();

	default:
		break;
	}
	glPopAttrib();
	GL_PopMatrix(GL_MODELVIEW, oldMatrix);
}

void GLC_RenderSceneBlurDo(float alpha)
{
	extern cvar_t gl_motion_blur_fps;
	static double last_time;
	double current_time = Sys_DoubleTime(), diff_time = current_time - last_time;
	double fps = gl_motion_blur_fps.value > 0 ? gl_motion_blur_fps.value : 77;
	qbool draw = (alpha >= 0); // negative alpha mean we don't draw anything but copy screen only.
	float oldProjectionMatrix[16];
	float oldModelviewMatrix[16];

	int vwidth = 1, vheight = 1;
	float vs, vt, cs, ct;

	// Remember all attributes.
	glPushAttrib(GL_ALL_ATTRIB_BITS);

	// alpha more than 0.5 are wrong.
	alpha = bound(0.1, alpha, 0.5);

	if (gl_support_arb_texture_non_power_of_two)
	{	//we can use any size, supposedly
		vwidth = glwidth;
		vheight = glheight;
	}
	else
	{	//limit the texture size to square and use padding.
		while (vwidth < glwidth)
			vwidth *= 2;
		while (vheight < glheight)
			vheight *= 2;
	}

	glViewport (0, 0, glwidth, glheight);

	GL_Bind(sceneblur_texture);

	// go 2d
	GL_PushMatrix(GL_PROJECTION, oldProjectionMatrix);
	GL_PushMatrix(GL_MODELVIEW, oldModelviewMatrix);
	GL_OrthographicProjection(0, glwidth, 0, glheight, -99999, 99999);
	GL_IdentityModelView();

	//blend the last frame onto the scene
	//the maths is because our texture is over-sized (must be power of two)
	cs = vs = (float)glwidth / vwidth * 0.5;
	ct = vt = (float)glheight / vheight * 0.5;
	// qqshka: I don't get what is gl_motionblurscale, so simply removed it.
	vs *= 1;//gl_motionblurscale.value;
	vt *= 1;//gl_motionblurscale.value;

	glDisable(GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);
	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);

	glColor4f(1, 1, 1, alpha);

	if (draw)
	{
		glBegin(GL_QUADS);
		glTexCoord2f(cs-vs, ct-vt);
		glVertex2f(0, 0);
		glTexCoord2f(cs+vs, ct-vt);
		glVertex2f(glwidth, 0);
		glTexCoord2f(cs+vs, ct+vt);
		glVertex2f(glwidth, glheight);
		glTexCoord2f(cs-vs, ct+vt);
		glVertex2f(0, glheight);
		glEnd();
	}

	// Restore matrices.
	GL_PopMatrix(GL_PROJECTION, oldProjectionMatrix);
	GL_PopMatrix(GL_MODELVIEW, oldModelviewMatrix);

	// With high frame rate frames difference is soo smaaaal, so motion blur almost unnoticeable,
	// so I copy frame not every frame.
	if (diff_time >= 1.0 / fps)
	{
		last_time = current_time;

		//copy the image into the texture so that we can play with it next frame too!
		glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, vwidth, vheight, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	// Restore attributes.
	glPopAttrib();
}
