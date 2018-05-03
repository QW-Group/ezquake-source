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

// gl_state_resets.c
// moving state init/reset to here with intention of getting rid of all resets

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"

float GL_WaterAlpha(void)
{
	extern cvar_t r_wateralpha;

	return bound((1 - r_refdef2.max_watervis), r_wateralpha.value, 1);
}

void GL_StateDefault3D(void)
{
	GL_ResetRegion(false);

	ENTER_STATE;

	// set drawing parms
	GL_CullFace(GL_FRONT);
	if (gl_cull.value) {
		glEnable(GL_CULL_FACE);
	}
	else {
		glDisable(GL_CULL_FACE);
	}

	if (CL_MultiviewEnabled()) {
		glClear(GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 1;
		GL_DepthFunc(GL_LEQUAL);
	}

	GL_DepthRange(gldepthmin, gldepthmax);
	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_DISABLED);
	GL_Enable(GL_DEPTH_TEST);

	if (gl_gammacorrection.integer) {
		glEnable(GL_FRAMEBUFFER_SRGB);
	}
	else {
		glDisable(GL_FRAMEBUFFER_SRGB);
	}
	GL_TextureEnvModeForUnit(GL_TEXTURE0, GL_REPLACE);
	GL_DebugState();

	LEAVE_STATE;
}

void GLM_StateBeginDraw3DSprites(void)
{
	ENTER_STATE;

	GL_DisableFog();
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	GL_Disable(GL_CULL_FACE);

	LEAVE_STATE;
}

void GLM_StateEndDraw3DSprites(void)
{
}

void GL_StateDefaultInit(void)
{
	ENTER_STATE;

#ifndef __APPLE__
	GL_ClearColor(0, 0, 0, 1);
#else
	GL_ClearColor(0.2, 0.2, 0.2, 1.0);
#endif

	GL_CullFace(GL_FRONT);
	if (GL_UseImmediateMode()) {
		glEnable(GL_TEXTURE_2D);
	}

	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED);
	GL_AlphaFunc(GL_GREATER, 0.666);

	GL_PolygonMode(GL_FILL);

	GL_BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	GL_TextureEnvMode(GL_REPLACE);

	LEAVE_STATE;
}

void GLC_StateBeginDraw3DSprites(void)
{
	ENTER_STATE;

	GL_DisableFog();
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	GLC_InitTextureUnitsNoBind1(GL_MODULATE);
	GLC_EnsureTMUEnabled(GL_TEXTURE0);
	glColor4ubv(color_white);
	GL_Disable(GL_CULL_FACE);
	GL_AlphaFunc(GL_GREATER, 0.333f);

	LEAVE_STATE;
}

void GLC_StateEndDraw3DSprites(void)
{
	ENTER_STATE;

	GL_DepthMask(GL_TRUE);
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	GL_BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	GL_TextureEnvMode(GL_REPLACE);
	GL_EnableFog();
	GL_AlphaFunc(GL_GREATER, 0.666f);

	LEAVE_STATE;
}

void GL_StateEndFrame(void)
{
}

void GLC_StateEndRenderScene(void)
{
	ENTER_STATE;

	GLC_InitTextureUnitsNoBind1(GL_REPLACE);

	GL_ConfigureFog();

	LEAVE_STATE;
}
