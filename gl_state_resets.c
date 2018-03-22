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
#include "r_state.h"

rendering_state_t default3DState;
rendering_state_t spritesStateGLM;
rendering_state_t spritesStateGLC;

void R_InitialiseStates(void)
{
	R_InitRenderingState(&default3DState, true);

	R_InitRenderingState(&spritesStateGLM, true);
	spritesStateGLM.fog.enabled = false;
	spritesStateGLM.blendingEnabled = true;
	spritesStateGLM.blendFunc = r_blendfunc_premultiplied_alpha;
	spritesStateGLM.cullface.enabled = false;

	R_InitRenderingState(&spritesStateGLC, true);
	spritesStateGLC.fog.enabled = false;
	spritesStateGLC.blendingEnabled = true;
	spritesStateGLC.blendFunc = r_blendfunc_premultiplied_alpha;
	spritesStateGLC.cullface.enabled = false;
	spritesStateGLC.alphaTesting.enabled = true;
	spritesStateGLC.alphaTesting.func = r_alphatest_func_greater;
	spritesStateGLC.alphaTesting.value = 0.333f;
}

float GL_WaterAlpha(void)
{
	extern cvar_t r_wateralpha;

	return bound((1 - r_refdef2.max_watervis), r_wateralpha.value, 1);
}

void GL_StateDefault3D(void)
{
	GL_ResetRegion(false);

	ENTER_STATE;

	R_ApplyRenderingState(&default3DState);
	GL_DebugState();

	LEAVE_STATE;
}

void GLM_StateBeginDraw3DSprites(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&spritesStateGLM);

	LEAVE_STATE;
}

void GLM_StateEndDraw3DSprites(void)
{
}

void GL_StateDefaultInit(void)
{
	rendering_state_t state;

	ENTER_STATE;

	R_InitRenderingState(&state, true);
	R_ApplyRenderingState(&state);

	LEAVE_STATE;
}

void GLC_StateBeginDraw3DSprites(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&spritesStateGLC);

	LEAVE_STATE;
}

void GLC_StateEndDraw3DSprites(void)
{
}

void GL_StateEndFrame(void)
{
}

void GLC_StateEndRenderScene(void)
{
	ENTER_STATE;

	//GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	//GL_ConfigureFog();

	LEAVE_STATE;
}
