/*
Copyright (C) 2018 ezQuake team

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
#include "r_sprite3d.h"
#include "glm_local.h"
#include "r_renderer.h"
#include "glm_brushmodel.h"

void GLM_RenderView(void)
{
	GLM_UploadFrameConstants();
	R_UploadChangedLightmaps();
	GLM_PrepareWorldModelBatch();
	GLM_PrepareAliasModelBatches();
	renderer.Prepare3DSprites();

	GLM_DrawWorldModelBatch(opaque_world);

	R_TraceEnterNamedRegion("GLM_DrawEntities");
	GLM_DrawAliasModelBatches();
	R_TraceLeaveNamedRegion();

	renderer.Draw3DSprites();

	GLM_DrawWorldModelBatch(alpha_surfaces);

	GLM_DrawAliasModelPostSceneBatches();
}

void GLM_PrepareModelRendering(qbool vid_restart)
{
	extern buffer_ref brushModel_vbo, vbo_brushElements;

	buffer_ref instance_vbo = R_CreateInstanceVBO();

	GLM_BuildCommonTextureArrays(vid_restart);

	GL_CreateAliasModelVBO(instance_vbo);
	R_BrushModelCreateVBO(instance_vbo);
	GLM_CreateBrushModelVAO(brushModel_vbo, vbo_brushElements, instance_vbo);

	GLM_InitPrograms();
}

