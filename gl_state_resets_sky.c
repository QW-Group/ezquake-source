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

// gl_state_resets_sky.c
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"

#define ENTER_STATE GL_EnterTracedRegion(__FUNCTION__, true)
#define MIDDLE_STATE GL_MarkEvent(__FUNCTION__)
#define LEAVE_STATE GL_LeaveRegion()

extern texture_ref solidskytexture, alphaskytexture;

void GLC_StateBeginFastSky(void)
{
	ENTER_STATE;

	if (gl_fogsky.value) {
		GL_EnableFog();
	}
	GLC_DisableAllTexturing();
	glColor3ubv(r_skycolor.color);

	LEAVE_STATE;
}

void GLC_StateEndFastSky(void)
{
	ENTER_STATE;

	GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	GL_Color4ubv(color_white);
	if (gl_fogsky.value) {
		GL_DisableFog();
	}

	LEAVE_STATE;
}

void GLC_StateBeginSky(void)
{
	ENTER_STATE;

	GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	glDisable(GL_DEPTH_TEST);

	LEAVE_STATE;
}

void GLC_StateBeginSkyZBufferPass(void)
{
	ENTER_STATE;

	if (gl_fogenable.value && gl_fogsky.value) {
		glEnable(GL_DEPTH_TEST);
		GL_EnableFog();
		glColor4f(gl_fogred.value, gl_foggreen.value, gl_fogblue.value, 1);
		GL_BlendFunc(GL_ONE, GL_ZERO);
	}
	else {
		glEnable(GL_DEPTH_TEST);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		GL_BlendFunc(GL_ZERO, GL_ONE);
	}
	GLC_DisableAllTexturing();
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);

	LEAVE_STATE;
}

void GLC_StateEndSkyZBufferPass(void)
{
	ENTER_STATE;

	// FIXME: GL_ResetState()
	if (gl_fogenable.value && gl_fogsky.value) {
		GL_DisableFog();
	}
	else {
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);

	LEAVE_STATE;
}

void GLC_StateEndSkyNoZBufferPass(void)
{
	ENTER_STATE;

	glEnable(GL_DEPTH_TEST);

	LEAVE_STATE;
}

void GLC_StateBeginSkyDome(void)
{
	ENTER_STATE;

	GLC_InitTextureUnits1(solidskytexture, GL_REPLACE);
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);

	LEAVE_STATE;
}

void GLC_StateBeginSkyDomeCloudPass(void)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	GL_EnsureTextureUnitBound(GL_TEXTURE0, alphaskytexture);

	LEAVE_STATE;
}

void GLC_StateBeginMultiTextureSkyDome(void)
{
	ENTER_STATE;

	GLC_InitTextureUnits2(solidskytexture, GL_REPLACE, alphaskytexture, GL_DECAL);

	LEAVE_STATE;
}

void GLC_StateEndMultiTextureSkyDome(void)
{
	ENTER_STATE;

	GLC_InitTextureUnitsNoBind1(GL_REPLACE);

	LEAVE_STATE;
}

void GLC_StateBeginMultiTextureSkyChain(void)
{
	ENTER_STATE;

	if (gl_fogsky.value) {
		GL_EnableFog();
	}

	GLC_InitTextureUnits2(solidskytexture, GL_MODULATE, alphaskytexture, GL_DECAL);

	LEAVE_STATE;
}

void GLC_StateEndMultiTextureSkyChain(void)
{
	ENTER_STATE;

	GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	if (gl_fogsky.value) {
		GL_DisableFog();
	}

	LEAVE_STATE;
}

void GLC_StateBeginSingleTextureSkyPass(void)
{
	ENTER_STATE;

	if (gl_fogsky.value) {
		GL_EnableFog();
	}
	GLC_InitTextureUnits1(solidskytexture, GL_REPLACE);

	LEAVE_STATE;
}

void GLC_StateBeginSingleTextureCloudPass(void)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	GL_EnsureTextureUnitBound(GL_TEXTURE0, alphaskytexture);

	LEAVE_STATE;
}

void GLC_StateEndSingleTextureSky(void)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	if (gl_fogsky.value) {
		GL_DisableFog();
	}

	LEAVE_STATE;
}
