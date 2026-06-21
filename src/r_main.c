
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

void GLM_Initialise(void);
void GLC_Initialise(void);
#ifdef RENDERER_OPTION_VULKAN
void VK_PopulateConfig(void);
qbool VK_InitialiseVAOHandling(void);
void VK_InitialiseBufferHandling(api_buffers_t* api);
#endif

void CachePics_Shutdown(void);
void R_LightmapShutdown(void);
void R_Hud_Initialise(void);
void R_BrushModelFreeMemory(void);

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

void R_Shutdown(r_shutdown_mode_t mode)
{
	// VAOs/buffers must be torn down while the renderer's context/device is
	// still alive. For Vulkan, renderer.Shutdown() (VK_Shutdown) destroys the
	// VkDevice/VkInstance and zeroes vk_options outright, so calling
	// VK_DeleteVAOs/VK_BufferShutdown afterwards meant vkDestroyBuffer() etc.
	// ran against an already-NULL VkDevice -- the loader dereferences that to
	// find the dispatch table, which crashed in vulkan-1.dll on every
	// vid_restart away from the Vulkan renderer. GL is unaffected by the
	// ordering since its context isn't destroyed until later in
	// VID_Shutdown(), well after R_Shutdown() returns.
	if (mode != r_shutdown_reload) {
		R_BrushModelFreeMemory();
		if (renderer.DeleteVAOs) {
			renderer.DeleteVAOs();
		}
		if (buffers.Shutdown) {
			buffers.Shutdown();
		}
	}

	if (renderer.Shutdown) {
		renderer.Shutdown(mode);
	}

	CachePics_Shutdown();
	R_LightmapShutdown();

	R_DeleteTextures();
	R_TexturesInvalidateAllReferences();
}

#ifdef EZ_MULTIPLE_RENDERERS
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
#endif // EZ_MULTIPLE_RENDERERS

void R_Initialise(void)
{
	R_SelectRenderer();

#ifdef RENDERER_OPTION_MODERN_OPENGL
	if (R_UseModernOpenGL()) {
		GLM_Initialise();
	}
#endif
#ifdef RENDERER_OPTION_CLASSIC_OPENGL
	if (R_UseImmediateOpenGL()) {
		GLC_Initialise();
	}
#endif
#ifdef RENDERER_OPTION_VULKAN
	if (R_UseVulkan()) {
		VK_PopulateConfig();
		VK_InitialiseVAOHandling();
		VK_InitialiseBufferHandling(&buffers);
	}
#endif
	R_Hud_Initialise();
}

// Called during disconnect
void R_OnDisconnect(void)
{
	R_ClearModelTextureData();
}
