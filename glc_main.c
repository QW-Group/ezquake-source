
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

void GLC_BindVertexArrayElementBuffer(r_vao_id vao, buffer_ref ref)
{
}

qbool GLC_False(void)
{
	return false;
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
#define GLC_InitialiseVAOState             GL_InitialiseVAOState
#define GLC_DescriptiveString              GL_DescriptiveString
#define GLC_Draw3DSprites                  GLC_NoOperation
#define GLC_Prepare3DSprites               GLC_NoOperation
#define GLC_Begin2DRendering               GLC_NoOperation    // GL_Framebuffer2DSwitch
#define GLC_IsFramebufferEnabled3D         GLC_False

#define RENDERER_METHOD(returntype, name, ...) \
{ \
	extern returntype GLC_ ## name(__VA_ARGS__); \
	renderer.name = GLC_ ## name; \
}

void GLC_Initialise(void)
{
	#include "r_renderer_structure.h"

	GL_Init();
	GL_PopulateConfig();
	renderer.vaos_supported = GLC_InitialiseVAOHandling();
	GL_InitialiseBufferHandling(&buffers);
	GL_InitialiseState();
}

void GLC_PrepareModelRendering(qbool vid_restart)
{
	if (buffers.supported) {
		buffer_ref instance_vbo = R_CreateInstanceVBO();

		GL_CreateAliasModelVBO(instance_vbo);
		GL_CreateBrushModelVBO(instance_vbo);
	}
}

