
#include "quakedef.h"
#include "common_draw.h"
#include "r_local.h"
#include "r_vao.h"
#include "r_lightmaps.h"
#include "gl_local.h"
#include "glc_local.h"
#include "glm_local.h"
#include "r_texture.h"
#include "r_renderer.h"

extern void CachePics_Shutdown(void);
extern void R_LightmapShutdown(void);
extern void GL_DeleteBrushModelIndexBuffer(void);

void R_NewMapPrepare(qbool vid_restart)
{
	if (cl.worldmodel) {
		R_BuildLightmaps();
	}
	renderer.PrepareModelRendering(vid_restart);
}

void VID_GfxInfo_f(void)
{
	renderer.PrintGfxInfo();
}

void R_Shutdown(qbool restart)
{
	renderer.Shutdown(restart);

	CachePics_Shutdown();
	R_LightmapShutdown();
	GL_DeleteBrushModelIndexBuffer();
	R_DeleteVAOs();
	buffers.Shutdown();
	R_DeleteTextures();
	R_TexturesInvalidateAllReferences();
}

void R_Initialise(void)
{
	R_InitialiseBufferHandling();
	R_Hud_Initialise();
	R_PopulateConfig();

#ifdef RENDERER_OPTION_MODERN_OPENGL
	if (R_UseModernOpenGL()) {
		GLM_Initialise();
		GL_InitialiseState();
	}
#endif
#ifdef RENDERER_OPTION_CLASSIC_OPENGL
	if (R_UseImmediateOpenGL()) {
		GLC_Initialise();
		GL_InitialiseState();
	}
#endif
#ifdef RENDERER_OPTION_VULKAN
	if (R_UseVulkan()) {
		VK_Initialise();
		VK_InitialiseState();
	}
#endif
}

// Called during disconnect
void R_OnDisconnect(void)
{
	R_ClearModelTextureData();
}

void R_CvarForceRecompile(cvar_t* var)
{
	renderer.CvarForceRecompile(var);
}
