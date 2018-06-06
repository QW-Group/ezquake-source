
#include "quakedef.h"
#include "r_renderer.h"
#include "r_buffers.h"
#include "glc_vao.h"
#include "r_brushmodel.h"
#include "r_aliasmodel.h"

void GL_Init(void);
qbool GLC_InitialiseVAOHandling(void);
void GL_InitialiseBufferHandling(api_buffers_t* buffers);
void GL_InitialiseState(void);

static void GLC_CvarForceRecompile(cvar_t* cvar)
{
}

static void GLC_NoOperation(void)
{
}

static void GLC_BindVertexArrayElementBuffer(r_vao_id vao, buffer_ref ref)
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
#define GLC_PostProcessScreen              V_UpdatePalette
#define GLC_Screenshot                     GL_Screenshot
#define GLC_InitialiseVAOState             GL_InitialiseVAOState
#define GLC_DescriptiveString              GL_DescriptiveString
#define GLC_Draw3DSprites                  GLC_NoOperation
#define GLC_Prepare3DSprites               GLC_NoOperation
#define GLC_Begin2DRendering               GLC_NoOperation    // GL_Framebuffer2DSwitch
#define GLC_IsFramebufferEnabled3D         GLC_False
#define GLC_TextureDelete                  GL_TextureDelete
#define GLC_TextureMipmapGenerate          GL_TextureMipmapGenerate
#define GLC_TextureWrapModeClamp           GL_TextureWrapModeClamp
#define GLC_TextureLabelSet                GL_TextureLabelSet
#define GLC_TextureUnitBind                GL_EnsureTextureUnitBound
#define GLC_TextureGet                     GL_TextureGet
#define GLC_TextureCompressionSet          GL_TextureCompressionSet
#define GLC_TextureCreate2D                GL_TextureCreate2D
#define GLC_TextureUnitMultiBind           GL_TextureUnitMultiBind
#define GLC_TexturesCreate                 GL_TexturesCreate
#define GLC_TextureReplaceSubImageRGBA     GL_TextureReplaceSubImageRGBA

#define RENDERER_METHOD(returntype, name, ...) \
{ \
	extern returntype GLC_ ## name(__VA_ARGS__); \
	renderer.name = GLC_ ## name; \
}

void GLC_Initialise(void)
{
	#include "r_renderer_structure.h"

	GL_Init();
	renderer.vaos_supported = GLC_InitialiseVAOHandling();
	GL_InitialiseBufferHandling(&buffers);
	GL_InitialiseState();
}

void GLC_PrepareModelRendering(qbool vid_restart)
{
	if (buffers.supported) {
		buffer_ref instance_vbo = R_CreateInstanceVBO();

		R_CreateAliasModelVBO(instance_vbo);
		R_BrushModelCreateVBO(instance_vbo);
	}
}
