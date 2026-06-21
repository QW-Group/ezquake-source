/*
Copyright (C) 2018 ezQuake team.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef EZQUAKE_VK_LOCAL_HEADER
#define EZQUAKE_VK_LOCAL_HEADER

#include <vulkan/vulkan.h>

#include "r_local.h"
#include "r_state.h"

#define VK_MAX_FRAMES_IN_FLIGHT 2

typedef struct SDL_Window SDL_Window;
typedef struct gltexture_s gltexture_t;
struct entity_s;
struct model_s;

#define EZ_VKFUNC_DECL_LOAD(instance, func) PFN_##func q##func = (PFN_##func)vkGetInstanceProcAddr(instance, #func)
#define EZ_VKFUNC_LOAD(instance, func) q##func = (PFN_##func)vkGetInstanceProcAddr(instance, #func)
#define VK_InitialiseStructure(x) memset(&(x), 0, sizeof(x))

// vk_main.c
qbool VK_Initialise(SDL_Window* window);
void VK_Shutdown(r_shutdown_mode_t mode);
void VK_PopulateConfig(void);
void VK_RequestSwapChainRecreate(void);
void VK_RequestSurfaceRecreate(void);

// vk_instance.c
qbool VK_CreateInstance(SDL_Window* window, VkInstance* instance);

// vk_debug.c
void VK_ShutdownDebugCallback(VkInstance instance);
void VK_InitialiseDebugCallback(VkInstance instance);

// vk_physical_devices.c
qbool VK_SelectPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
uint32_t VK_PhysicalDeviceGraphicsQueueFamilyIndex(void);
uint32_t VK_PhysicalDeviceComputeQueueFamilyIndex(void);
qbool VK_RefreshPresentationMode(void);
uint32_t VK_PhysicalDevicePresentQueueFamilyIndex(void);
qbool VK_CreateLogicalDevice(VkInstance instance);

// vk_window_surface.c
qbool VK_CreateWindowSurface(SDL_Window* window, VkInstance instance, VkSurfaceKHR* surface);
void VK_DestroyWindowSurface(VkInstance instance, VkSurfaceKHR surface);

// vk_swapchain.c
qbool VK_CreateSwapChain(SDL_Window* window, VkInstance instance, VkSurfaceKHR surface);
void VK_DestroySwapChain(void);
qbool VK_CreateSwapChainFramebuffers(void);
void VK_DestroySwapChainFramebuffers(void);

// vk_renderpass.c
qbool VK_RenderPassCreate(void);
void VK_RenderPassDelete(void);
VkRenderPass VK_MainRenderPass(void);
VkFormat VK_DepthFormat(void);

// vk_blending.c
void VK_BlendingConfigure(VkPipelineColorBlendStateCreateInfo* info, VkPipelineColorBlendAttachmentState* blending, r_blendfunc_t func);

// vk_resources.c
uint32_t VK_FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties);
qbool VK_CreateBufferResource(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* memory);
qbool VK_CreateImageResource(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* image, VkDeviceMemory* memory);
VkCommandBuffer VK_BeginImmediateCommands(void);
qbool VK_EndImmediateCommands(VkCommandBuffer command_buffer);
void VK_DestroyImmediateCommandPool(void);

// vk_buffers.c
VkBuffer VK_BufferHandle(r_buffer_id id);
VkDeviceSize VK_BufferDeviceOffset(r_buffer_id id);

// vk_texture.c
void VK_AllocateTextureNames(gltexture_t* glt);
void VK_UploadTexture(texture_ref texture, int mode, int width, int height, byte* data);
VkDescriptorSetLayout VK_TextureDescriptorSetLayout(void);
VkDescriptorSet VK_TextureDescriptorSet(texture_ref texture);
qbool VK_TextureDescriptorImageInfo(texture_ref texture, qbool nearest, VkDescriptorImageInfo* info);
qbool VK_TextureReady(texture_ref texture);
void VK_TextureInitialiseState(void);
void VK_TextureShutdown(void);
void VK_TextureDelete(texture_ref texture);
void VK_TextureMipmapGenerate(texture_ref texture);
void VK_TextureWrapModeClamp(texture_ref tex);
void VK_TextureLabelSet(texture_ref texture, const char* label);
qbool VK_TextureUnitBind(int unit, texture_ref texture);
qbool VK_TextureIsUnitBound(int unit, texture_ref texture);
void VK_TextureUnitMultiBind(int first_unit, int num_textures, texture_ref* textures);
void VK_TextureGet(texture_ref texture, int buffer_size, byte* buffer, int bpp);
void VK_TextureCompressionSet(qbool enabled);
void VK_TextureCreate2D(texture_ref* reference, int width, int height, const char* name, qbool is_lightmap);
void VK_TexturesCreate(r_texture_type_id type, int count, texture_ref* textures);
void VK_TextureReplaceSubImageRGBA(texture_ref texture, int offsetx, int offsety, int width, int height, byte* buffer);
void VK_TextureFlushPendingUploads(VkCommandBuffer commandBuffer, uint32_t frameIndex);
void VK_TextureSetFiltering(texture_ref texture, texture_minification_id min_filter, texture_magnification_id mag_filter);
void VK_TextureSetAnisotropy(texture_ref texture, int anisotropy);

// vk_draw.c
void VK_HudResourcesShutdown(void);
void VK_HudSwapchainChanged(void);

// vk_world.c
void VK_WorldResourcesShutdown(void);
void VK_PrepareModelRendering(qbool vid_restart);
void VK_PreRenderView(void);
void VK_DrawWorld(void);
void VK_ChainBrushModelSurfaces(struct model_s* model, struct entity_s* ent);
void VK_DrawBrushModel(struct entity_s* ent, qbool polygonOffset, qbool caustics);
void VK_RenderView(void);

// vk_aliasmodel.c
void VK_AliasModelResourcesShutdown(void);
void VK_AliasModelFrameReset(void);
void VK_AliasQueueDraw(struct entity_s* ent, struct model_s* model, int firstVertex, int vertexCount, texture_ref texture, qbool outline, int effects, int render_effects, float lerpfrac);
void VK_DrawAliasFrame(struct entity_s* ent, struct model_s* model, int pose1, int pose2, texture_ref texture, texture_ref fb_texture, qbool outline, int effects, int render_effects, float lerpfrac);
void VK_DrawAliasModelShadow(struct entity_s* ent);
void VK_DrawAlias3Model(struct entity_s* ent, qbool outline, qbool additive_pass);
void VK_RenderAliasModels(qbool postscene);

// vk_sprite3d.c
void VK_Sprite3DResourcesShutdown(void);
void VK_DrawSpriteModel(struct entity_s* ent);
void VK_DrawSimpleItem(struct model_s* model, int skin, vec3_t origin, float scale, vec3_t up, vec3_t right);
void VK_DrawClassicParticles(int particles_to_draw);
void VK_Prepare3DSprites(void);
void VK_Draw3DSprites(void);

// vk_main.c
VkCommandBuffer VK_CurrentCommandBuffer(void);

// (common)
typedef struct vk_options_s {
	VkInstance instance;
	VkSurfaceKHR surface;
	SDL_Window* window;

	VkPhysicalDevice physicalDevice;
	VkPhysicalDeviceFeatures physicalDeviceFeatures;
	VkPhysicalDeviceProperties physicalDeviceProperties;
	uint32_t physicalDeviceGraphicsQueueFamilyIndex;
	uint32_t physicalDeviceComputeQueueFamilyIndex;
	uint32_t physicalDevicePresentQueueFamilyIndex;
	VkPresentModeKHR physicalDevicePresentationMode;
	VkSurfaceFormatKHR physicalDeviceSurfaceFormat;
	VkSurfaceCapabilitiesKHR physicalDeviceSurfaceCapabilities;
	VkDevice logicalDevice;
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	struct {
		VkSwapchainKHR handle;
		VkImage* images;
		VkImageView* imageViews;
		VkFramebuffer* framebuffers;
		VkImage depthImage;
		VkDeviceMemory depthImageMemory;
		VkImageView depthImageView;
		VkExtent2D imageSize;
		int imageCount;
	} swapChain;
	struct {
		VkCommandPool commandPool;
		VkCommandBuffer* commandBuffers;
		VkSemaphore imageAvailableSemaphores[VK_MAX_FRAMES_IN_FLIGHT];
		VkSemaphore renderFinishedSemaphores[VK_MAX_FRAMES_IN_FLIGHT];
		VkFence inFlightFences[VK_MAX_FRAMES_IN_FLIGHT];
		VkFence* imageInFlightFences;
		uint32_t currentFrame;
		uint32_t imageIndex;
		qbool active;
	} frame;
	float clearColor[4];
} vk_options_t;

extern vk_options_t vk_options;

void VK_PrintGfxInfo(void);

#endif
