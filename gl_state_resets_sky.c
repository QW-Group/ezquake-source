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
#include "r_state.h"
#include "r_trace.h"

static rendering_state_t fastSkyState;
static rendering_state_t fastSkyStateFogged;
static rendering_state_t skyDomeFirstPassState;
static rendering_state_t skyDomeCloudPassState;
static rendering_state_t skyDomeSinglePassState;
static rendering_state_t skyDomeZPassState;
static rendering_state_t skyDomeZPassFoggedState;

extern texture_ref solidskytexture, alphaskytexture;

void GLC_InitialiseSkyStates(void)
{
	R_InitRenderingState(&fastSkyState, true, "fastSkyState", vao_brushmodel);
	fastSkyState.depth.test_enabled = false;

	R_InitRenderingState(&fastSkyStateFogged, true, "fastSkyStateFogged", vao_brushmodel);
	fastSkyStateFogged.depth.test_enabled = false;
	fastSkyStateFogged.fog.enabled = true;

	R_InitRenderingState(&skyDomeZPassState, true, "skyDomeZPassState", vao_none);
	skyDomeZPassState.depth.test_enabled = true;
	skyDomeZPassState.blendingEnabled = true;
	skyDomeZPassState.fog.enabled = false;
	skyDomeZPassState.colorMask[0] = skyDomeZPassState.colorMask[1] = skyDomeZPassState.colorMask[2] = skyDomeZPassState.colorMask[3] = false;
	skyDomeZPassState.blendFunc = r_blendfunc_src_zero_dest_one;

	R_InitRenderingState(&skyDomeZPassFoggedState, true, "skyDomeZPassFoggedState", vao_none);
	skyDomeZPassFoggedState.depth.test_enabled = true;
	skyDomeZPassFoggedState.blendingEnabled = true;
	skyDomeZPassFoggedState.fog.enabled = true;
	skyDomeZPassFoggedState.blendFunc = r_blendfunc_src_one_dest_zero;

	R_InitRenderingState(&skyDomeFirstPassState, true, "skyDomeFirstPassState", vao_none);
	skyDomeFirstPassState.depth.test_enabled = false;
	skyDomeFirstPassState.blendingEnabled = false;
	skyDomeFirstPassState.textureUnits[0].enabled = true;
	skyDomeFirstPassState.textureUnits[0].mode = r_texunit_mode_replace;

	R_InitRenderingState(&skyDomeCloudPassState, true, "skyDomeCloudPassState", vao_none);
	skyDomeCloudPassState.depth.test_enabled = false;
	skyDomeCloudPassState.blendingEnabled = true;
	skyDomeCloudPassState.blendFunc = r_blendfunc_premultiplied_alpha;
	skyDomeCloudPassState.textureUnits[0].enabled = true;
	skyDomeCloudPassState.textureUnits[0].mode = r_texunit_mode_replace;

	R_InitRenderingState(&skyDomeSinglePassState, true, "skyDomeSinglePassState", vao_none);
	skyDomeSinglePassState.depth.test_enabled = false;
	skyDomeSinglePassState.blendingEnabled = false;
	skyDomeSinglePassState.textureUnits[0].enabled = true;
	skyDomeSinglePassState.textureUnits[0].mode = r_texunit_mode_replace;
	skyDomeSinglePassState.textureUnits[1].enabled = true;
	skyDomeSinglePassState.textureUnits[1].mode = r_texunit_mode_decal;
}

void GLC_StateBeginFastSky(void)
{
	extern cvar_t gl_fogsky, gl_fogenable, r_skycolor;

	ENTER_STATE;

	if (gl_fogsky.integer && gl_fogenable.integer) {
		R_ApplyRenderingState(&fastSkyStateFogged);
	}
	else {
		R_ApplyRenderingState(&fastSkyState);
	}
	R_CustomColor(r_skycolor.color[0] / 255.0f, r_skycolor.color[1] / 255.0f, r_skycolor.color[2] / 255.0f, 1.0f);

	LEAVE_STATE;
}

void GLC_StateBeginSkyZBufferPass(void)
{
	extern cvar_t gl_fogsky, gl_fogenable, r_skycolor, gl_fogred, gl_foggreen, gl_fogblue;

	ENTER_STATE;

	if (gl_fogenable.integer && gl_fogsky.integer) {
		R_ApplyRenderingState(&skyDomeZPassFoggedState);
		R_CustomColor(gl_fogred.value, gl_foggreen.value, gl_fogblue.value, 1);
	}
	else {
		R_ApplyRenderingState(&skyDomeZPassState);
	}

	LEAVE_STATE;
}

void GLC_StateBeginSingleTextureSkyDome(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&skyDomeFirstPassState);
	R_TextureUnitBind(0, solidskytexture);

	LEAVE_STATE;
}

void GLC_StateBeginSingleTextureSkyDomeCloudPass(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&skyDomeCloudPassState);
	R_TextureUnitBind(0, alphaskytexture);

	LEAVE_STATE;
}

void GLC_StateBeginMultiTextureSkyDome(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&skyDomeSinglePassState);
	R_TextureUnitBind(0, solidskytexture);
	R_TextureUnitBind(1, alphaskytexture);

	LEAVE_STATE;
}

void GLC_StateBeginMultiTextureSkyChain(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&skyDomeSinglePassState);
	R_TextureUnitBind(0, solidskytexture);
	R_TextureUnitBind(1, alphaskytexture);

	LEAVE_STATE;
}

void GLC_StateBeginSingleTextureSkyPass(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&skyDomeFirstPassState);
	R_TextureUnitBind(0, solidskytexture);

	LEAVE_STATE;
}

void GLC_StateBeginSingleTextureCloudPass(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&skyDomeCloudPassState);
	R_TextureUnitBind(0, alphaskytexture);

	LEAVE_STATE;
}
