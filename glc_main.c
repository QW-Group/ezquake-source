
#include "quakedef.h"
#include "r_renderer.h"
#include "r_buffers.h"
#include "glc_vao.h"

static void GLC_CvarForceRecompile(cvar_t* cvar)
{
}

void GLC_NoOperation(void)
{
}

#define GLC_PrintGfxInfo                   GL_PrintGfxInfo
#define GLC_Viewport                       GL_Viewport
#define GLC_RenderDynamicLightmaps         R_RenderDynamicLightmaps
#define GLC_InvalidateLightmapTextures     GLC_NoOperation
#define GLC_LightmapFrameInit              GLC_NoOperation
#define GLC_LightmapShutdown               GLC_NoOperation
#define GLC_ClearRenderingSurface          GL_Clear
#define GLC_ScreenDrawStart                GLC_NoOperation
#define GLC_EnsureFinished                 GL_EnsureFinished
#define GLC_RenderSceneBlur                GLC_RenderSceneBlurDo
#define GLC_RenderView                     GLC_NoOperation
#define GLC_PostProcessScreen              GLC_NoOperation
#define GLC_Screenshot                     GL_Screenshot
#define GLC_InitTextureState               GL_InitTextureState
#define GLC_InitialiseVAOState             GL_InitialiseVAOState
#define GLC_VAOBound                       GL_VAOBound
#define GLC_BindVertexArrayElementBuffer   GL_BindVertexArrayElementBuffer

#define RENDERER_METHOD(returntype, name, ...) \
{ \
	extern returntype GLC_ ## name(__VA_ARGS__); \
	renderer.name = GLC_ ## name; \
}

void GLC_Initialise(void)
{
	#include "r_renderer_structure.h"
}

void GLC_PrepareModelRendering(qbool vid_restart)
{
	if (buffers.supported) {
		buffer_ref instance_vbo = R_CreateInstanceVBO();

		GL_CreateAliasModelVBO(instance_vbo);
		GL_CreateBrushModelVBO(instance_vbo);
	}
}

