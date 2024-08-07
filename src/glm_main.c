
#include "quakedef.h"
#include "glm_vao.h"
#include "r_renderer.h"
#include "gl_framebuffer.h"

#ifdef RENDERER_OPTION_MODERN_OPENGL

void GL_Init(void);
void GL_InitialiseBufferHandling(api_buffers_t* api);
void GL_InitialiseState(void);

static void R_Stubs_NoOperationEntity(entity_t* ent)
{
}

static void R_Stubs_NoOperation(void)
{
}

#ifndef WITH_RENDERING_TRACE
static void GLM_TextureLabelSetNull(texture_ref texture, const char* name)
{
}
#endif

static void GLM_Begin2DRendering(void)
{
	GL_Framebuffer2DSwitch();
}

#define GLM_PrintGfxInfo                   GL_PrintGfxInfo
#define GLM_Viewport                       GL_Viewport
#define GLM_InvalidateViewport             GL_InvalidateViewport
#define GLM_ClearRenderingSurface          GL_Clear
#define GLM_EnsureFinished                 GL_EnsureFinished
#define GLM_Screenshot                     GL_Screenshot
#define GLM_ScreenshotWidth                GL_ScreenshotWidth
#define GLM_ScreenshotHeight               GL_ScreenshotHeight
#define GLM_InitialiseVAOState             GL_InitialiseVAOState
#define GLM_DescriptiveString              GL_DescriptiveString
#define GLM_Draw3DSpritesInline            R_Stubs_NoOperation
#define GLM_DrawDisc                       R_Stubs_NoOperation
#define GLM_IsFramebufferEnabled3D         GL_FramebufferEnabled3D
#define GLM_DrawAliasModelShadow           R_Stubs_NoOperationEntity
#define GLM_DrawAliasModelPowerupShell     R_Stubs_NoOperationEntity
#define GLM_DrawAlias3ModelPowerupShell    R_Stubs_NoOperationEntity
#define GLM_TextureInitialiseState         GL_TextureInitialiseState
#define GLM_TextureDelete                  GL_TextureDelete
#define GLM_TextureMipmapGenerate          GL_TextureMipmapGenerate
#define GLM_TextureWrapModeClamp           GL_TextureWrapModeClamp
#ifdef WITH_RENDERING_TRACE
#define GLM_TextureLabelSet                GL_TextureLabelSet
#else
#define GLM_TextureLabelSet                GLM_TextureLabelSetNull
#endif
#define GLM_TextureUnitBind                GL_EnsureTextureUnitBound
#define GLM_TextureGet                     GL_TextureGet
#define GLM_TextureCompressionSet          GL_TextureCompressionSet
#define GLM_TextureCreate2D                GL_TextureCreate2D
#define GLM_TextureUnitMultiBind           GL_TextureUnitMultiBind
#define GLM_TexturesCreate                 GL_TexturesCreate
#define GLM_TextureReplaceSubImageRGBA     GL_TextureReplaceSubImageRGBA
#define GLM_TextureSetAnisotropy           GL_TextureSetAnisotropy
#define GLM_TextureSetFiltering            GL_TextureSetFiltering
#define GLM_TextureLoadCubemapFace         GL_TextureLoadCubemapFace
#define GLM_CvarForceRecompile             GL_CvarForceRecompile
#define GLM_ProgramsInitialise             GL_ProgramsInitialise
#define GLM_ProgramsShutdown               GL_ProgramsShutdown
#define GLM_DrawSky                        R_Stubs_NoOperation
#define GLM_PrepareAliasModel              GL_PrepareAliasModel
#define GLM_TextureIsUnitBound             GL_IsTextureBound
#define GLM_FramebufferCreate              GL_FramebufferCreate

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

#endif // #ifdef RENDERER_OPTION_MODERN_OPENGL
