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
#include "glc_local.h"
#include "r_matrix.h"
#include "r_state.h"

// motion blur.
texture_ref sceneblur_texture;

void GLC_PolyBlend(float v_blend[4])
{
	GLC_StateBeginPolyBlend(v_blend);

	GLC_Begin(GL_QUADS);
	GLC_Vertex2f(r_refdef.vrect.x, r_refdef.vrect.y);
	GLC_Vertex2f(r_refdef.vrect.x + r_refdef.vrect.width, r_refdef.vrect.y);
	GLC_Vertex2f(r_refdef.vrect.x + r_refdef.vrect.width, r_refdef.vrect.y + r_refdef.vrect.height);
	GLC_Vertex2f(r_refdef.vrect.x, r_refdef.vrect.y + r_refdef.vrect.height);
	GLC_End();
}

void GLC_BrightenScreen(void)
{
	extern float vid_gamma;
	float f;

	if (vid_hwgamma_enabled) {
		return;
	}
	if (v_contrast.value <= 1.0) {
		return;
	}

	f = min(v_contrast.value, 3);
	f = pow(f, vid_gamma);

	GLC_StateBeginBrightenScreen();

	GLC_Begin(GL_QUADS);
	while (f > 1) {
		if (f >= 2) {
			R_CustomColor(1, 1, 1, 1);
		}
		else {
			R_CustomColor(f - 1, f - 1, f - 1, 1);
		}

		GLC_Vertex2f(0, 0);
		GLC_Vertex2f(vid.width, 0);
		GLC_Vertex2f(vid.width, vid.height);
		GLC_Vertex2f(0, vid.height);

		f *= 0.5;
	}
	GLC_End();
}

/*
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

	GL_PushModelviewMatrix(oldMatrix);

	GL_TranslateModelview((*origin)[0], (*origin)[1], (*origin)[2]);
	GL_RotateModelview(yaw_degrees, 0.f, 0.f, 1.f);
	GL_TranslateModelview(show_velocity_3d_offset_forward.value, 0.f, -show_velocity_3d_offset_down.value);

	glPushAttrib(GL_LINE_BIT | GL_TEXTURE_BIT);
	glDisable(GL_TEXTURE_2D);
	switch (show_velocity_3d.integer)
	{
	case 1:
		//show vertical
		glEnable(GL_LINE_STIPPLE);
		glLineStipple(1, 0xFF00);
		R_CustomLineWidth(stipple_line_width);

		R_CustomColor(stipple_line_colour[0], stipple_line_colour[1], stipple_line_colour[2], stipple_line_colour[3]);
		GLC_Begin(GL_LINES);
		GLC_Vertex3f(v_forward, v_side, 0.f);
		GLC_Vertex3f(v_forward, v_side, v_up);
		GLC_End();

		glDisable(GL_LINE_STIPPLE);
		R_CustomLineWidth(line_width);
		R_CustomColor(0.f, 1.f, 0.f, 1.0f);

		GLC_Begin(GL_LINES);
		GLC_Vertex3fv(v3_zero);
		GLC_Vertex3f(v_forward, v_side, v_up);
		GLC_End();
		//no break here

	case 2:
		//show horizontal velocity only
		R_CustomColor(1.f, 0.f, 0.f, 1.0f);
		R_CustomLineWidth(line_width);
		GLC_Begin(GL_LINES);
		GLC_Vertex3fv(v3_zero);
		glVertex3f(v_forward, v_side, 0.f);
		GLC_End();

		glEnable(GL_LINE_STIPPLE);
		glLineStipple(1, 0xFF00);
		R_CustomColor(stipple_line_colour[0], stipple_line_colour[1], stipple_line_colour[2], stipple_line_colour[3]);
		R_CustomLineWidth(stipple_line_width);

		GLC_Begin(GL_LINE_LOOP);
		GLC_Vertex3fv(v3_zero);
		GLC_Vertex3f(0.f, v_side, 0.f);
		GLC_Vertex3f(v_forward, v_side, 0.f);
		GLC_Vertex3f(v_forward, 0.f, 0.f);
		GLC_End();

	default:
		break;
	}
	glPopAttrib();
	GL_PopModelviewMatrix(oldMatrix);
}*/

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

	// alpha more than 0.5 are wrong.
	alpha = bound(0.1, alpha, 0.5);

	R_TextureSizeRoundUp(glwidth, glheight, &vwidth, &vheight);

	// go 2d
	GL_PushProjectionMatrix(oldProjectionMatrix);
	GL_PushModelviewMatrix(oldModelviewMatrix);

	GLC_StateBeginSceneBlur();

	//blend the last frame onto the scene
	//the maths is because our texture is over-sized (must be power of two)
	cs = vs = (float)glwidth / vwidth * 0.5;
	ct = vt = (float)glheight / vheight * 0.5;
	// qqshka: I don't get what is gl_motionblurscale, so simply removed it.
	vs *= 1;//gl_motionblurscale.value;
	vt *= 1;//gl_motionblurscale.value;

	R_TextureUnitBind(GL_TEXTURE0, sceneblur_texture);
	R_CustomColor(alpha, alpha, alpha, alpha);
	if (draw)
	{
		GLC_Begin(GL_QUADS);
		glTexCoord2f(cs-vs, ct-vt);
		GLC_Vertex2f(0, 0);
		glTexCoord2f(cs+vs, ct-vt);
		GLC_Vertex2f(glwidth, 0);
		glTexCoord2f(cs+vs, ct+vt);
		GLC_Vertex2f(glwidth, glheight);
		glTexCoord2f(cs-vs, ct+vt);
		GLC_Vertex2f(0, glheight);
		GLC_End();
	}

	// Restore matrices.
	GL_PopProjectionMatrix(oldProjectionMatrix);
	GL_PopModelviewMatrix(oldModelviewMatrix);

	// With high frame rate frames difference is soo smaaaal, so motion blur almost unnoticeable,
	// so I copy frame not every frame.
	if (diff_time >= 1.0 / fps)
	{
		last_time = current_time;

		//copy the image into the texture so that we can play with it next frame too!
		glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, vwidth, vheight, 0);
		R_SetTextureFiltering(sceneblur_texture, texture_minification_linear, texture_magnification_linear);
	}
}

void GLC_PreRenderView(void)
{
	// TODO
}

void GLC_SetupGL(void)
{
}

void GLC_Shutdown(qbool restarting)
{
	GLC_FreeAliasPoseBuffer();
}
