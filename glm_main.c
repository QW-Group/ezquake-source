
#include "quakedef.h"
#include "r_renderer.h"

void GL_Init(void);
qbool GLM_InitialiseVAOHandling(void);
void GL_InitialiseBufferHandling(api_buffers_t* api);
void GL_InitialiseState(void);

static void GLM_AddWaterFog(int contents)
{
}

static void GLM_RenderSceneBlur(float alpha)
{
}

static void GLM_DrawAliasModelShadow(entity_t* ent)
{
}

static void GLM_DrawAliasModelPowerupShell(entity_t* ent)
{
}

static void R_Stubs_NoOperation(void)
{
}

#define GLM_PrintGfxInfo                   GL_PrintGfxInfo
#define GLM_Viewport                       GL_Viewport
#define GLM_ClearRenderingSurface          GL_Clear
#define GLM_EnsureFinished                 GL_EnsureFinished
#define GLM_Screenshot                     GL_Screenshot
#define GLM_ConfigureFog                   R_Stubs_NoOperation
#define GLM_InitTextureState               GL_InitTextureState
#define GLM_InitialiseVAOState             GL_InitialiseVAOState
#define GLM_DescriptiveString              GL_DescriptiveString
#define GLM_Draw3DSpritesInline            R_Stubs_NoOperation
#define GLM_DrawDisc                       R_Stubs_NoOperation
#define GLM_IsFramebufferEnabled3D         GL_FramebufferEnabled3D
#define GLM_Begin2DRendering               GL_Framebuffer2DSwitch

#define RENDERER_METHOD(returntype, name, ...) \
{ \
	extern returntype GLM_ ## name(__VA_ARGS__); \
	renderer.name = GLM_ ## name; \
}

void GLM_Initialise(void)
{
#include "r_renderer_structure.h"

	GL_Init();
	renderer.vaos_supported = GLM_InitialiseVAOHandling();
	GL_InitialiseBufferHandling(&buffers);
	GL_InitialiseState();
}
