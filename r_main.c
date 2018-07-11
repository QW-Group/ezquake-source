
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "r_local.h"
#include "r_lightmaps.h"

void R_BuildCommonTextureFormats(qbool vid_restart)
{
	if (R_UseModernOpenGL()) {
		GL_BuildCommonTextureArrays(vid_restart);
	}
	else if (R_UseImmediateOpenGL()) {

	}
	else if (R_UseVulkan()) {

	}
}

static buffer_ref R_CreateInstanceVBO(void)
{
	unsigned int values[MAX_STANDARD_ENTITIES];
	int i;

	for (i = 0; i < MAX_STANDARD_ENTITIES; ++i) {
		values[i] = i;
	}

	return buffers.Create(buffertype_vertex, "instance#", sizeof(values), values, bufferusage_constant_data);
}

void R_PrepareModelRendering(qbool vid_restart)
{
	if (R_UseModernOpenGL() || (R_UseImmediateOpenGL() && buffers.supported)) {
		buffer_ref instance_vbo = R_CreateInstanceVBO();

		GL_CreateAliasModelVBO(instance_vbo);
		GL_CreateBrushModelVBO(instance_vbo);
	}
}

void R_NewMapPrepare(qbool vid_restart)
{
	if (cl.worldmodel) {
		R_BuildLightmaps();
	}
	R_BuildCommonTextureFormats(vid_restart);
	R_PrepareModelRendering(vid_restart);
	if (R_UseModernOpenGL()) {
		GLM_InitPrograms();
	}
}

void VID_GfxInfo_f(void)
{
	extern void GL_PrintGfxInfo(void);
	extern void VK_PrintGfxInfo(void);

	if (R_UseImmediateOpenGL()) {
		GL_PrintGfxInfo();
	}
	else if (R_UseModernOpenGL()) {
		GL_PrintGfxInfo();
	}
	else if (R_UseVulkan()) {
		VK_PrintGfxInfo();
	}
}
