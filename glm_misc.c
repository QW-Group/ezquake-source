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

void GLM_PolyBlend(float v_blend[4])
{
	color_t v_blend_color = RGBA_TO_COLOR(
		bound(0, v_blend[0], 1) * 255,
		bound(0, v_blend[1], 1) * 255,
		bound(0, v_blend[2], 1) * 255,
		bound(0, v_blend[3], 1) * 255
	);

	Draw_AlphaRectangleRGB(r_refdef.vrect.x, r_refdef.vrect.y, r_refdef.vrect.width, r_refdef.vrect.height, 0.0f, true, v_blend_color);
}

void GLM_BrightenScreen(void)
{
	// MEAG: TODO
}

void GLM_DrawVelocity3D(void)
{
	// MEAG: TODO
}

void GLM_RenderSceneBlurDo(float alpha)
{
	// MEAG: TODO
}

void GLM_EmitCausticsPolys(void)
{
	// MEAG: TODO
	// Is this ever going to be a thing?
}

void GLM_PreRenderView(void)
{
	// TODO
}
