
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"

void GLM_RenderView(void)
{
	GLM_UploadFrameConstants();
	R_UploadChangedLightmaps();
	GLM_PrepareWorldModelBatch();
	GLM_PrepareAliasModelBatches();
	GLM_Prepare3DSprites();

	GL_DrawWorldModelBatch(opaque_world);

	GL_EnterRegion("GLM_DrawEntities");
	GLM_DrawAliasModelBatches();
	GL_LeaveRegion();

	GLM_Draw3DSprites();

	GL_DrawWorldModelBatch(alpha_surfaces);

	GLM_DrawAliasModelPostSceneBatches();
}
