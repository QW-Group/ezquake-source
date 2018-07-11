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
#include "r_matrix.h"
#include "glc_matrix.h"

static rendering_state_t default3DState;
static rendering_state_t spritesStateGLM;

void R_InitialiseStates(void)
{
	R_InitRenderingState(&default3DState, true, "default3DState", vao_none);

	R_Init3DSpriteRenderingState(&spritesStateGLM, "spritesStateGLM");
}

float GL_WaterAlpha(void)
{
	extern cvar_t r_wateralpha;

	return bound((1 - r_refdef2.max_watervis), r_wateralpha.value, 1);
}

void GL_StateDefault3D(void)
{
	GL_ResetRegion(false);

	if (R_UseImmediateOpenGL()) {
		GLC_PauseMatrixUpdate();
	}
	GL_IdentityModelView();
	GL_RotateModelview(-90, 1, 0, 0);	    // put Z going up
	GL_RotateModelview(90, 0, 0, 1);	    // put Z going up
	GL_RotateModelview(-r_refdef.viewangles[2], 1, 0, 0);
	GL_RotateModelview(-r_refdef.viewangles[0], 0, 1, 0);
	GL_RotateModelview(-r_refdef.viewangles[1], 0, 0, 1);
	GL_TranslateModelview(-r_refdef.vieworg[0], -r_refdef.vieworg[1], -r_refdef.vieworg[2]);
	if (R_UseImmediateOpenGL()) {
		GLC_ResumeMatrixUpdate();
		GLC_LoadModelviewMatrix();
	}
	GL_GetModelviewMatrix(r_world_matrix);

	ENTER_STATE;

	R_ApplyRenderingState(&default3DState);
	GL_DebugState();

	LEAVE_STATE;
}

void GLM_StateBeginDraw3DSprites(void)
{
}

void GL_StateDefaultInit(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&default3DState);

	LEAVE_STATE;
}
