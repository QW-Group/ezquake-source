
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

renderer_api_t renderer;

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

void R_SelectRenderer(void)
{
	int i;
	const int renderer_options[] = { 
#ifdef RENDERER_OPTION_CLASSIC_OPENGL
		0,
#endif
#ifdef RENDERER_OPTION_MODERN_OPENGL
		1,
#endif
#ifdef RENDERER_OPTION_VULKAN
		2
#endif
	};

	for (i = 0; i < sizeof(renderer_options) / sizeof(renderer_options[0]); ++i) {
		if (renderer_options[i] == vid_renderer.integer) {
			// Selected renderer is valid
			return;
		}
	}

	// Fall back to first on list
	Cvar_LatchedSetValue(&vid_renderer, renderer_options[0]);
	return;
}

void R_Initialise(void)
{
	R_SelectRenderer();

#ifdef RENDERER_OPTION_MODERN_OPENGL
	if (R_UseModernOpenGL()) {
		GLM_Initialise();
		GL_Init();
		GL_PopulateConfig();
		GLM_InitialiseVAOHandling();
		GL_InitialiseBufferHandling(&buffers);
		GL_InitialiseState();
	}
#endif
#ifdef RENDERER_OPTION_CLASSIC_OPENGL
	if (R_UseImmediateOpenGL()) {
		GLC_Initialise();
	}
#endif
#ifdef RENDERER_OPTION_VULKAN
	if (R_UseVulkan()) {
		VK_Initialise();
		VK_PopulateConfig();
		VK_InitialiseVAOHandling();
		VK_InitialiseBufferHandling(&buffers);
		VK_InitialiseState();
	}
#endif
	R_Hud_Initialise();
}

// Called during disconnect
void R_OnDisconnect(void)
{
	R_ClearModelTextureData();
}
