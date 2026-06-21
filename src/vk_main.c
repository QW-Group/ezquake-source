/*
Copyright (C) 2018 ezQuake team

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

// vk_main.c
// - Main entry point for Vulkan

#ifdef RENDERER_OPTION_VULKAN

#include <vulkan/vulkan.h>
#include "quakedef.h"

#include <SDL.h>
#include <SDL_vulkan.h>
#ifdef __ANDROID__
#include <jni.h>
#include <SDL_system.h>
#include <android/log.h>
#endif

#include "gl_model.h"
#include "r_aliasmodel.h"
#include "r_renderer.h"
#include "tr_types.h"
#include "glsl/constants.glsl"
#include "vk_local.h"

vk_options_t vk_options;
static qbool vk_recreate_swapchain_requested;
static qbool vk_recreate_surface_requested;

#ifdef __ANDROID__
static qbool vk_android_startup_splash_hidden;
static double vk_profile_recording_start;

static void VK_AndroidHideStartupSplash(void)
{
	JNIEnv* env;
	jobject activity;
	jclass activity_class;
	jmethodID hide_method;

	if (vk_android_startup_splash_hidden) {
		return;
	}

	env = (JNIEnv*) SDL_AndroidGetJNIEnv();
	activity = (jobject) SDL_AndroidGetActivity();
	if (!env || !activity) {
		return;
	}

	activity_class = (*env)->GetObjectClass(env, activity);
	if (!activity_class) {
		(*env)->DeleteLocalRef(env, activity);
		return;
	}

	hide_method = (*env)->GetMethodID(env, activity_class, "hideStartupSplash", "()V");
	if (hide_method) {
		(*env)->CallVoidMethod(env, activity, hide_method);
		vk_android_startup_splash_hidden = true;
	}

	(*env)->DeleteLocalRef(env, activity_class);
	(*env)->DeleteLocalRef(env, activity);
}
#endif

void VK_DrawImage(float x, float y, float width, float height, float tex_s, float tex_t, float tex_width, float tex_height, byte* color, int flags);
void VK_DrawRectangle(float x, float y, float width, float height, byte* color);
void VK_AdjustImages(int first, int last, float x_offset);
void VK_TextureLoadCubemapFace(texture_ref cubemap, r_cubemap_direction_id direction, const byte* data, int width, int height);
void VK_CreateLightmapTextures(void);
void VK_UploadLightmap(int textureUnit, int lightmapnum);
void VK_BuildLightmap(int lightmapnum);
void VK_InvalidateLightmapTextures(void);
void VK_LightmapFrameInit(void);
void VK_LightmapShutdown(void);
void VK_RenderDynamicLightmaps(msurface_t* surface, qbool world);
void VK_DrawWaterSurfaces(void);
void VK_DeleteVAOs(void);
void VK_GenVertexArray(r_vao_id vao, const char* name);
void VK_BindVertexArray(r_vao_id vao);
void VK_BindVertexArrayElementBuffer(r_vao_id vao, r_buffer_id ref);
qbool VK_VertexArrayCreated(r_vao_id vao);
qbool VK_InitialiseVAOHandling(void);

static void VK_NoOperation(void)
{
}

static void VK_NoOperationCvar(cvar_t* cvar)
{
	(void)cvar;
}

static void VK_NoOperationBool(qbool value)
{
	(void)value;
}

static void VK_NoOperationState(r_state_id state)
{
	R_ApplyRenderingState(state);
}

static void VK_NoOperationEntity(entity_t* ent)
{
	(void)ent;
}

static void VK_NoOperationFloat4(float v[4])
{
	(void)v;
}

static void VK_NoOperationViewport(int x, int y, int width, int height)
{
	(void)x;
	(void)y;
	(void)width;
	(void)height;
}

static void VK_NoOperationAliasModel(model_t* model, aliashdr_t* hdr)
{
	(void)model;
	(void)hdr;
}

static void VK_NoOperationAliasFrame(entity_t* ent, model_t* model, int pose1, int pose2, texture_ref texture, texture_ref fb_texture, qbool outline, int effects, int render_effects, float lerpfrac)
{
	(void)ent;
	(void)model;
	(void)pose1;
	(void)pose2;
	(void)texture;
	(void)fb_texture;
	(void)outline;
	(void)effects;
	(void)render_effects;
	(void)lerpfrac;
}

static void VK_NoOperationSimpleItem(model_t* model, int skin, vec3_t origin, float scale, vec3_t up, vec3_t right)
{
	(void)model;
	(void)skin;
	(void)origin;
	(void)scale;
	(void)up;
	(void)right;
}

static void VK_NoOperationParticles(int count)
{
	(void)count;
}

static int VK_BrushModelCopyVertToBuffer(model_t* mod, void* vbo_buffer_, int position, float* source, int lightmap, int material, float scaleS, float scaleT, msurface_t* surf, qbool has_fb_texture, qbool has_luma_texture)
{
	(void)has_fb_texture;
	(void)has_luma_texture;
	{
		vbo_world_vert_t* target = (vbo_world_vert_t*)vbo_buffer_ + position;
		byte rgba[4];

		VectorCopy(source, target->position);
		target->material_coords[0] = source[3] * (scaleS ? scaleS : 1);
		target->material_coords[1] = source[4] * (scaleT ? scaleT : 1);
		target->material_coords[2] = material;
		target->lightmap_coords[0] = source[5];
		target->lightmap_coords[1] = source[6];
		target->lightmap_coords[2] = lightmap;
		target->detail_coords[0] = source[7];
		target->detail_coords[1] = source[8];

		if (surf->flags & SURF_DRAWSKY) {
			target->flags = TEXTURE_TURB_SKY;
		}
		else if (surf->flags & SURF_DRAWTURB) {
			target->flags = (surf->texinfo->texture->turbType & EZQ_SURFACE_TYPE);
			if (!target->flags) {
				target->flags = TEXTURE_TURB_OTHER;
			}
		}
		else if (mod->isworldmodel) {
			target->flags = EZQ_SURFACE_WORLD;
			target->flags += (surf->flags & SURF_DRAWFLAT_FLOOR ? EZQ_SURFACE_IS_FLOOR : 0);
			target->flags += (surf->flags & SURF_UNDERWATER ? EZQ_SURFACE_UNDERWATER : 0);
		}
		else {
			target->flags = 0;
		}
		target->flags += (surf->flags & SURF_DRAWALPHA ? EZQ_SURFACE_ALPHATEST : 0);

		COLOR_TO_RGBA(surf->texinfo->texture->flatcolor3ub, rgba);
		VectorScale(rgba, 1 / 255.0f, target->flatcolor);
		target->surface_num = mod->isworldmodel ? surf - mod->surfaces : 0;
	}

	return position + 1;
}

static qbool VK_False(void)
{
	return false;
}

static qbool VK_FalseFramebuffer(framebuffer_id id, int width, int height)
{
	(void)id;
	(void)width;
	(void)height;
	return false;
}

static const char* VK_DescriptiveString(void)
{
	if (vk_options.physicalDevice != VK_NULL_HANDLE && vk_options.physicalDeviceProperties.deviceName[0]) {
		return vk_options.physicalDeviceProperties.deviceName;
	}
	return "Vulkan";
}

static void VK_Screenshot(byte* buffer, size_t size)
{
	VkCommandBuffer cmd;
	VkBuffer stagingBuffer = VK_NULL_HANDLE;
	VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
	VkImage srcImage;
	VkImageMemoryBarrier barrier;
	VkBufferImageCopy region;
	void* mapped;
	uint32_t width, height;
	qbool swizzleBGRA;

	memset(buffer, 0, size);

	if (vk_options.logicalDevice == VK_NULL_HANDLE || !vk_options.swapChain.images || vk_options.swapChain.imageCount <= 0) {
		return;
	}

	width = vk_options.swapChain.imageSize.width;
	height = vk_options.swapChain.imageSize.height;
	if (!width || !height || size < (size_t)width * height * 3 || vk_options.frame.imageIndex >= (uint32_t)vk_options.swapChain.imageCount) {
		return;
	}

	// Screenshots are rare/not perf-sensitive, so a full vkDeviceWaitIdle to make
	// sure the swapchain image we're about to read has actually finished
	// presenting is fine here -- unlike the per-frame path, this has no business
	// being clever about synchronization.
	vkDeviceWaitIdle(vk_options.logicalDevice);

	srcImage = vk_options.swapChain.images[vk_options.frame.imageIndex];

	if (!VK_CreateBufferResource((VkDeviceSize)width * height * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingMemory)) {
		return;
	}

	cmd = VK_BeginImmediateCommands();
	if (cmd == VK_NULL_HANDLE) {
		vkDestroyBuffer(vk_options.logicalDevice, stagingBuffer, NULL);
		vkFreeMemory(vk_options.logicalDevice, stagingMemory, NULL);
		return;
	}

	VK_InitialiseStructure(barrier);
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = srcImage;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

	VK_InitialiseStructure(region);
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageExtent.width = width;
	region.imageExtent.height = height;
	region.imageExtent.depth = 1;
	vkCmdCopyImageToBuffer(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &region);

	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.dstAccessMask = 0;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

	VK_EndImmediateCommands(cmd);

	swizzleBGRA = (vk_options.physicalDeviceSurfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM ||
		vk_options.physicalDeviceSurfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB);

	if (vkMapMemory(vk_options.logicalDevice, stagingMemory, 0, (VkDeviceSize)width * height * 4, 0, &mapped) == VK_SUCCESS) {
		uint32_t x, y;

		// Vulkan images are stored top-down; callers of renderer.Screenshot()
		// (SCR_ScreenshotWrite, Image_WriteJPEG with a negative stride, ...)
		// expect the GL convention instead: row 0 is the bottom of the image.
		for (y = 0; y < height; ++y) {
			const byte* srcRow = (const byte*)mapped + (size_t)y * width * 4;
			byte* dstRow = buffer + (size_t)(height - 1 - y) * width * 3;

			for (x = 0; x < width; ++x) {
				const byte* px = srcRow + (size_t)x * 4;
				byte* out = dstRow + (size_t)x * 3;

				if (swizzleBGRA) {
					out[0] = px[2];
					out[1] = px[1];
					out[2] = px[0];
				}
				else {
					out[0] = px[0];
					out[1] = px[1];
					out[2] = px[2];
				}
			}
		}
		vkUnmapMemory(vk_options.logicalDevice, stagingMemory);
	}

	vkDestroyBuffer(vk_options.logicalDevice, stagingBuffer, NULL);
	vkFreeMemory(vk_options.logicalDevice, stagingMemory, NULL);
}

static size_t VK_ScreenshotWidth(void)
{
	return vk_options.swapChain.imageSize.width ? vk_options.swapChain.imageSize.width : glConfig.vidWidth;
}

static size_t VK_ScreenshotHeight(void)
{
	return vk_options.swapChain.imageSize.height ? vk_options.swapChain.imageSize.height : glConfig.vidHeight;
}

static void VK_ClearRenderingSurface(qbool clear_color)
{
	(void)clear_color;
}

static void VK_DestroyFrameResources(void)
{
	uint32_t i;

	if (vk_options.logicalDevice == VK_NULL_HANDLE) {
		return;
	}

	for (i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; ++i) {
		if (vk_options.frame.imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
			vkDestroySemaphore(vk_options.logicalDevice, vk_options.frame.imageAvailableSemaphores[i], NULL);
		}
		if (vk_options.frame.renderFinishedSemaphores[i] != VK_NULL_HANDLE) {
			vkDestroySemaphore(vk_options.logicalDevice, vk_options.frame.renderFinishedSemaphores[i], NULL);
		}
		if (vk_options.frame.inFlightFences[i] != VK_NULL_HANDLE) {
			vkDestroyFence(vk_options.logicalDevice, vk_options.frame.inFlightFences[i], NULL);
		}
	}
	if (vk_options.frame.commandPool != VK_NULL_HANDLE) {
		vkDestroyCommandPool(vk_options.logicalDevice, vk_options.frame.commandPool, NULL);
	}
	Q_free(vk_options.frame.commandBuffers);
	Q_free(vk_options.frame.imageInFlightFences);
	memset(&vk_options.frame, 0, sizeof(vk_options.frame));
}

static qbool VK_CreateFrameResources(void)
{
	VkCommandPoolCreateInfo poolInfo = { 0 };
	VkCommandBufferAllocateInfo allocInfo = { 0 };
	VkSemaphoreCreateInfo semaphoreInfo = { 0 };
	VkFenceCreateInfo fenceInfo = { 0 };
	uint32_t i;

	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = VK_PhysicalDeviceGraphicsQueueFamilyIndex();
	if (vkCreateCommandPool(vk_options.logicalDevice, &poolInfo, NULL, &vk_options.frame.commandPool) != VK_SUCCESS) {
		return false;
	}

	vk_options.frame.commandBuffers = Q_malloc(vk_options.swapChain.imageCount * sizeof(vk_options.frame.commandBuffers[0]));
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = vk_options.frame.commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = vk_options.swapChain.imageCount;
	if (vkAllocateCommandBuffers(vk_options.logicalDevice, &allocInfo, vk_options.frame.commandBuffers) != VK_SUCCESS) {
		VK_DestroyFrameResources();
		return false;
	}

	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	for (i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; ++i) {
		if (vkCreateSemaphore(vk_options.logicalDevice, &semaphoreInfo, NULL, &vk_options.frame.imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(vk_options.logicalDevice, &semaphoreInfo, NULL, &vk_options.frame.renderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(vk_options.logicalDevice, &fenceInfo, NULL, &vk_options.frame.inFlightFences[i]) != VK_SUCCESS) {
			VK_DestroyFrameResources();
			return false;
		}
	}
	vk_options.frame.imageInFlightFences = Q_calloc(vk_options.swapChain.imageCount, sizeof(vk_options.frame.imageInFlightFences[0]));

	return true;
}

static qbool VK_RecreateSwapChain(void)
{
	if (vk_options.window == NULL || vk_options.logicalDevice == VK_NULL_HANDLE || vk_options.surface == VK_NULL_HANDLE) {
		return false;
	}

	vkDeviceWaitIdle(vk_options.logicalDevice);

	VK_DestroyFrameResources();
	VK_DestroySwapChain();

	if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_options.physicalDevice, vk_options.surface, &vk_options.physicalDeviceSurfaceCapabilities) != VK_SUCCESS) {
		return false;
	}

	if (!VK_CreateSwapChain(vk_options.window, vk_options.instance, vk_options.surface)) {
		return false;
	}
	if (!VK_CreateSwapChainFramebuffers()) {
		return false;
	}
	if (!VK_CreateFrameResources()) {
		return false;
	}

	VK_HudSwapchainChanged();
	return true;
}

static qbool VK_RecreateSurfaceAndSwapChain(void)
{
	if (vk_options.window == NULL || vk_options.instance == VK_NULL_HANDLE || vk_options.logicalDevice == VK_NULL_HANDLE) {
		return false;
	}

	vkDeviceWaitIdle(vk_options.logicalDevice);

	VK_DestroyFrameResources();
	VK_DestroySwapChain();
	VK_DestroyWindowSurface(vk_options.instance, vk_options.surface);
	vk_options.surface = VK_NULL_HANDLE;

	if (!VK_CreateWindowSurface(vk_options.window, vk_options.instance, &vk_options.surface)) {
		return false;
	}
	if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_options.physicalDevice, vk_options.surface, &vk_options.physicalDeviceSurfaceCapabilities) != VK_SUCCESS) {
		return false;
	}
	if (!VK_CreateSwapChain(vk_options.window, vk_options.instance, vk_options.surface)) {
		return false;
	}
	if (!VK_CreateSwapChainFramebuffers()) {
		return false;
	}
	if (!VK_CreateFrameResources()) {
		return false;
	}

	VK_HudSwapchainChanged();
	return true;
}

void VK_RequestSwapChainRecreate(void)
{
	vk_recreate_swapchain_requested = true;
}

void VK_RequestSurfaceRecreate(void)
{
	vk_recreate_surface_requested = true;
	vk_recreate_swapchain_requested = true;
}

#ifdef __ANDROID__
typedef struct vk_profile_accum_s {
	double accum;
	double min;
	double max;
	unsigned int count;
} vk_profile_accum_t;

// Shared 60-sample rolling average logger for the per-frame timing points
// below (acquire/present/etc) -- avoids repeating the same accumulate+log
// boilerplate at every call site.
static void VK_ProfileAccumulate(vk_profile_accum_t* a, const char* label, double dt)
{
	a->accum += dt;
	a->min = (a->count == 0) ? dt : min(a->min, dt);
	a->max = max(a->max, dt);
	if (++a->count >= 60) {
		__android_log_print(ANDROID_LOG_INFO, "VK_PROFILE", "%s avg=%.2fms min=%.2fms max=%.2fms",
			label, (a->accum / a->count) * 1000.0, a->min * 1000.0, a->max * 1000.0);
		a->accum = 0;
		a->min = 1e9;
		a->max = 0;
		a->count = 0;
	}
}
#endif

void VK_BeginFrame(void)
{
	VkResult result;
	VkCommandBufferBeginInfo beginInfo = { 0 };
	VkRenderPassBeginInfo renderPassInfo = { 0 };
	VkClearValue clearValues[2] = { 0 };
	VkCommandBuffer commandBuffer;
	uint32_t frameIndex;
	VkFence frameFence;

#ifdef __ANDROID__
	{
		static double profile_last_time;
		static double profile_accum;
		static double profile_min = 1e9;
		static double profile_max;
		static unsigned int profile_count;
		double now = Sys_DoubleTime();

		if (profile_last_time > 0) {
			double dt = now - profile_last_time;

			profile_accum += dt;
			profile_min = min(profile_min, dt);
			profile_max = max(profile_max, dt);
			if (++profile_count >= 60) {
				__android_log_print(ANDROID_LOG_INFO, "VK_PROFILE",
					"frame interval avg=%.2fms min=%.2fms max=%.2fms (%.1f fps avg)",
					(profile_accum / profile_count) * 1000.0, profile_min * 1000.0, profile_max * 1000.0,
					profile_count / profile_accum);
				profile_accum = 0;
				profile_min = 1e9;
				profile_max = 0;
				profile_count = 0;
			}
		}
		profile_last_time = now;
	}
#endif

	if (vk_options.logicalDevice == VK_NULL_HANDLE || vk_options.frame.active) {
		return;
	}
	if (vk_recreate_surface_requested) {
		// Clear the request before attempting recreation, not after: if this
		// fails, retrying it unconditionally on every subsequent frame turns
		// a single transient failure (e.g. surface capabilities glitching
		// during a display state change) into an infinite recreate loop that
		// never reaches vkQueueSubmit/Present again -- the screen freezes
		// forever instead of getting another real chance via the normal
		// VK_ERROR_OUT_OF_DATE_KHR path next time it actually occurs.
		vk_recreate_surface_requested = false;
		vk_recreate_swapchain_requested = false;
		if (!VK_RecreateSurfaceAndSwapChain()) {
			return;
		}
	}
	else if (vk_recreate_swapchain_requested) {
		vk_recreate_swapchain_requested = false;
		if (!VK_RecreateSwapChain()) {
			return;
		}
	}
	if (vk_options.swapChain.handle == VK_NULL_HANDLE) {
		return;
	}

	frameIndex = vk_options.frame.currentFrame;
	frameFence = vk_options.frame.inFlightFences[frameIndex];
#ifdef __ANDROID__
	// CPU-vs-GPU split: time blocked here is the CPU sitting idle waiting for
	// the GPU to finish a previous frame. Compared against the frame interval
	// logged above, a wait that tracks the interval closely means we are
	// GPU-bound; a wait near zero means the bottleneck is elsewhere (CPU game
	// logic, command recording, present/compositor).
	{
		static double profile_wait_accum;
		static double profile_wait_min = 1e9;
		static double profile_wait_max;
		static unsigned int profile_wait_count;
		double waitStart = Sys_DoubleTime();
		double waitDt;

		vkWaitForFences(vk_options.logicalDevice, 1, &frameFence, VK_TRUE, UINT64_MAX);
		waitDt = Sys_DoubleTime() - waitStart;

		profile_wait_accum += waitDt;
		profile_wait_min = min(profile_wait_min, waitDt);
		profile_wait_max = max(profile_wait_max, waitDt);
		if (++profile_wait_count >= 60) {
			__android_log_print(ANDROID_LOG_INFO, "VK_PROFILE",
				"gpu fence wait avg=%.2fms min=%.2fms max=%.2fms",
				(profile_wait_accum / profile_wait_count) * 1000.0, profile_wait_min * 1000.0, profile_wait_max * 1000.0);
			profile_wait_accum = 0;
			profile_wait_min = 1e9;
			profile_wait_max = 0;
			profile_wait_count = 0;
		}
	}
#else
	vkWaitForFences(vk_options.logicalDevice, 1, &frameFence, VK_TRUE, UINT64_MAX);
#endif

	// Finite timeout, not UINT64_MAX: with non-blocking present modes
	// (IMMEDIATE/MAILBOX) the CPU can render faster than the compositor
	// drains the swapchain's buffer queue for long enough to exhaust every
	// image. When that happens the compositor stops handing back buffers
	// (observed as BLASTBufferQueue "NO_BUFFER_AVAILABLE" in logcat) and an
	// infinite-timeout wait here never returns -- the app freezes forever
	// with no way to recover. A finite wait turns that into a recoverable
	// "no frame this loop", same idea as the OUT_OF_DATE/SUBOPTIMAL handling
	// below.
#ifdef __ANDROID__
	{
		static vk_profile_accum_t acquireAccum;
		double t0 = Sys_DoubleTime();
		result = vkAcquireNextImageKHR(vk_options.logicalDevice, vk_options.swapChain.handle, 1000000000ULL, vk_options.frame.imageAvailableSemaphores[frameIndex], VK_NULL_HANDLE, &vk_options.frame.imageIndex);
		VK_ProfileAccumulate(&acquireAccum, "vkAcquireNextImageKHR", Sys_DoubleTime() - t0);
	}
#else
	result = vkAcquireNextImageKHR(vk_options.logicalDevice, vk_options.swapChain.handle, 1000000000ULL, vk_options.frame.imageAvailableSemaphores[frameIndex], VK_NULL_HANDLE, &vk_options.frame.imageIndex);
#endif
	if (result == VK_TIMEOUT || result == VK_NOT_READY) {
		return;
	}
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		VK_RequestSwapChainRecreate();
		return;
	}
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		Con_DPrintf("vulkan: vkAcquireNextImageKHR failed: %d\n", result);
		return;
	}
	if (vk_options.frame.imageInFlightFences[vk_options.frame.imageIndex] != VK_NULL_HANDLE) {
		VkResult waitResult;
#ifdef __ANDROID__
		{
			static vk_profile_accum_t imageWaitAccum;
			double t0 = Sys_DoubleTime();
			waitResult = vkWaitForFences(vk_options.logicalDevice, 1, &vk_options.frame.imageInFlightFences[vk_options.frame.imageIndex], VK_TRUE, 1000000000ULL);
			VK_ProfileAccumulate(&imageWaitAccum, "image-in-flight wait", Sys_DoubleTime() - t0);
		}
#else
		waitResult = vkWaitForFences(vk_options.logicalDevice, 1, &vk_options.frame.imageInFlightFences[vk_options.frame.imageIndex], VK_TRUE, 1000000000ULL);
#endif
		if (waitResult == VK_TIMEOUT) {
			return;
		}
	}
	vk_options.frame.imageInFlightFences[vk_options.frame.imageIndex] = frameFence;

	commandBuffer = vk_options.frame.commandBuffers[vk_options.frame.imageIndex];
	vkResetCommandBuffer(commandBuffer, 0);

	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
		Con_DPrintf("vulkan: vkBeginCommandBuffer failed\n");
		return;
	}

	// Dynamic lightmaps queued by the previous frame are copied before the
	// render pass. This keeps transfer work on the frame submission and avoids
	// the queue-wide stalls caused by immediate command buffers.
	VK_TextureFlushPendingUploads(commandBuffer, frameIndex);

	clearValues[0].color.float32[0] = vk_options.clearColor[0];
	clearValues[0].color.float32[1] = vk_options.clearColor[1];
	clearValues[0].color.float32[2] = vk_options.clearColor[2];
	clearValues[0].color.float32[3] = vk_options.clearColor[3] ? vk_options.clearColor[3] : 1.0f;
	clearValues[1].depthStencil.depth = glConfig.reversed_depth ? 0.0f : 1.0f;
	clearValues[1].depthStencil.stencil = 0;

	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = VK_MainRenderPass();
	renderPassInfo.framebuffer = vk_options.swapChain.framebuffers[vk_options.frame.imageIndex];
	renderPassInfo.renderArea.offset.x = 0;
	renderPassInfo.renderArea.offset.y = 0;
	renderPassInfo.renderArea.extent = vk_options.swapChain.imageSize;
	renderPassInfo.clearValueCount = sizeof(clearValues) / sizeof(clearValues[0]);
	renderPassInfo.pClearValues = clearValues;

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	vk_options.frame.active = true;
#ifdef __ANDROID__
	vk_profile_recording_start = Sys_DoubleTime();
#endif
}

VkCommandBuffer VK_CurrentCommandBuffer(void)
{
	if (!vk_options.frame.active || !vk_options.frame.commandBuffers) {
		return VK_NULL_HANDLE;
	}
	return vk_options.frame.commandBuffers[vk_options.frame.imageIndex];
}

void VK_EndFrame(void)
{
	VkCommandBuffer commandBuffer;
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSubmitInfo submitInfo = { 0 };
	VkPresentInfoKHR presentInfo = { 0 };
	VkResult result;
	qbool presented = false;
	uint32_t frameIndex;
	VkFence frameFence;

	if (!vk_options.frame.active) {
		return;
	}
	frameIndex = vk_options.frame.currentFrame;
	frameFence = vk_options.frame.inFlightFences[frameIndex];

	commandBuffer = vk_options.frame.commandBuffers[vk_options.frame.imageIndex];
	vkCmdEndRenderPass(commandBuffer);
	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		vk_options.frame.active = false;
		Con_DPrintf("vulkan: vkEndCommandBuffer failed\n");
		return;
	}
#ifdef __ANDROID__
	if (vk_profile_recording_start > 0) {
		static vk_profile_accum_t recordingAccum;
		VK_ProfileAccumulate(&recordingAccum, "command recording (3D+HUD)", Sys_DoubleTime() - vk_profile_recording_start);
	}
#endif

	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &vk_options.frame.imageAvailableSemaphores[frameIndex];
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &vk_options.frame.renderFinishedSemaphores[frameIndex];

	vkResetFences(vk_options.logicalDevice, 1, &frameFence);
#ifdef __ANDROID__
	{
		static vk_profile_accum_t submitAccum;
		double t0 = Sys_DoubleTime();
		result = vkQueueSubmit(vk_options.graphicsQueue, 1, &submitInfo, frameFence) == VK_SUCCESS ? VK_SUCCESS : VK_ERROR_UNKNOWN;
		VK_ProfileAccumulate(&submitAccum, "vkQueueSubmit", Sys_DoubleTime() - t0);
		if (result != VK_SUCCESS) {
			vk_options.frame.active = false;
			Con_DPrintf("vulkan: vkQueueSubmit failed\n");
			return;
		}
	}
#else
	if (vkQueueSubmit(vk_options.graphicsQueue, 1, &submitInfo, frameFence) != VK_SUCCESS) {
		vk_options.frame.active = false;
		Con_DPrintf("vulkan: vkQueueSubmit failed\n");
		return;
	}
#endif

	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &vk_options.frame.renderFinishedSemaphores[frameIndex];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &vk_options.swapChain.handle;
	presentInfo.pImageIndices = &vk_options.frame.imageIndex;

#ifdef __ANDROID__
	{
		static vk_profile_accum_t presentAccum;
		double t0 = Sys_DoubleTime();
		result = vkQueuePresentKHR(vk_options.presentQueue, &presentInfo);
		VK_ProfileAccumulate(&presentAccum, "vkQueuePresentKHR", Sys_DoubleTime() - t0);
	}
#else
	result = vkQueuePresentKHR(vk_options.presentQueue, &presentInfo);
#endif
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		// Only a real error requires recreating the swapchain. SUBOPTIMAL is
		// just advisory -- the image was still presented fine, it just isn't
		// an exact match for the surface's "ideal" current extent. We
		// deliberately render at a smaller-than-display buffer size on
		// Android (see SDLSurface's render-scaling cap), so SUBOPTIMAL is the
		// expected steady state, not a transient condition: treating it as a
		// recreate trigger meant the swapchain (and a vkDeviceWaitIdle) was
		// being torn down and rebuilt on every single frame.
		VK_RequestSwapChainRecreate();
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		Con_DPrintf("vulkan: vkQueuePresentKHR failed: %d\n", result);
	}
	else {
		presented = true;
	}
#ifdef __ANDROID__
	if (presented) {
		VK_AndroidHideStartupSplash();
	}
#endif

	vk_options.frame.active = false;
	vk_options.frame.currentFrame = (frameIndex + 1) % VK_MAX_FRAMES_IN_FLIGHT;
}

qbool VK_Initialise(SDL_Window* window)
{
	memset(&vk_options, 0, sizeof(vk_options));
	vk_options.window = window;
	vk_options.clearColor[3] = 1.0f;

	if (!VK_CreateInstance(window, &vk_options.instance)) {
		return false;
	}

	if (!VK_CreateWindowSurface(window, vk_options.instance, &vk_options.surface)) {
		VK_Shutdown(r_shutdown_full);
		return false;
	}

	if (!VK_SelectPhysicalDevice(vk_options.instance, vk_options.surface)) {
		VK_Shutdown(r_shutdown_full);
		return false;
	}

	if (!VK_CreateLogicalDevice(vk_options.instance)) {
		VK_Shutdown(r_shutdown_full);
		return false;
	}

	if (!VK_CreateSwapChain(window, vk_options.instance, vk_options.surface)) {
		VK_Shutdown(r_shutdown_full);
		return false;
	}

	if (!VK_RenderPassCreate()) {
		VK_Shutdown(r_shutdown_full);
		return false;
	}

	if (!VK_CreateSwapChainFramebuffers()) {
		VK_Shutdown(r_shutdown_full);
		return false;
	}

	if (!VK_CreateFrameResources()) {
		VK_Shutdown(r_shutdown_full);
		return false;
	}

	Con_Printf("Vulkan initialised successfully\n");
	return true;
}

void VK_Shutdown(r_shutdown_mode_t mode)
{
	if (mode != r_shutdown_reload) {
		if (vk_options.logicalDevice != VK_NULL_HANDLE) {
			vkDeviceWaitIdle(vk_options.logicalDevice);
		}

		VK_HudResourcesShutdown();
		VK_WorldResourcesShutdown();
		VK_AliasModelResourcesShutdown();
		VK_Sprite3DResourcesShutdown();
		VK_TextureShutdown();
		VK_DestroyImmediateCommandPool();
		VK_DestroyFrameResources();

		VK_DestroySwapChain();

		VK_RenderPassDelete();

		if (vk_options.logicalDevice != VK_NULL_HANDLE) {
			vkDestroyDevice(vk_options.logicalDevice, NULL);
			vk_options.logicalDevice = VK_NULL_HANDLE;
		}

		if (vk_options.instance) {
			VK_DestroyWindowSurface(vk_options.instance, vk_options.surface);
			VK_ShutdownDebugCallback(vk_options.instance);
			vkDestroyInstance(vk_options.instance, NULL);
		}

		memset(&vk_options, 0, sizeof(vk_options));
	}

	// FIXME
}

void VK_PopulateConfig(void)
{
	const VkPhysicalDeviceLimits* limits = &vk_options.physicalDeviceProperties.limits;

	memset(&renderer, 0, sizeof(renderer));

	glConfig.renderer_string = (const unsigned char*)vk_options.physicalDeviceProperties.deviceName;
	glConfig.vendor_string = (const unsigned char*)"Vulkan";
	glConfig.version_string = (const unsigned char*)"Vulkan 1.0";
	glConfig.glsl_version = (const unsigned char*)"SPIR-V";
	glConfig.majorVersion = VK_VERSION_MAJOR(vk_options.physicalDeviceProperties.apiVersion);
	glConfig.minorVersion = VK_VERSION_MINOR(vk_options.physicalDeviceProperties.apiVersion);
	glConfig.texture_units = 16;
	glConfig.gl_max_size_default = limits->maxImageDimension2D ? (int)limits->maxImageDimension2D : 4096;
	glConfig.max_texture_depth = limits->maxImageArrayLayers ? (int)limits->maxImageArrayLayers : 1;
	glConfig.max_3d_texture_size = limits->maxImageDimension3D ? (int)limits->maxImageDimension3D : 1;
	glConfig.uniformBufferOffsetAlignment = (int)limits->minUniformBufferOffsetAlignment;
	glConfig.shaderStorageBufferOffsetAlignment = (int)limits->minStorageBufferOffsetAlignment;
	glConfig.supported_features =
		R_SUPPORT_FRAMEBUFFERS |
		R_SUPPORT_RENDERING_SHADERS |
		R_SUPPORT_PRIMITIVERESTART |
		R_SUPPORT_MULTITEXTURING |
		R_SUPPORT_TEXTURE_SAMPLERS |
		R_SUPPORT_CUBE_MAPS |
		R_SUPPORT_FRAMEBUFFERS_SRGB;

#define VK_CvarForceRecompile             VK_NoOperationCvar
#define VK_PrintGfxInfo                   VK_PrintGfxInfo
#define VK_DescriptiveString              VK_DescriptiveString
#define VK_Viewport                       VK_NoOperationViewport
#define VK_InvalidateViewport             VK_NoOperation
#define VK_ApplyRenderingState            VK_NoOperationState
#define VK_PrepareModelRendering          VK_PrepareModelRendering
#define VK_PrepareAliasModel              GL_PrepareAliasModel
#define VK_DrawSky                        VK_NoOperation
#define VK_DrawWorld                      VK_DrawWorld
#define VK_DrawAliasFrame                 VK_DrawAliasFrame
#define VK_DrawAliasModelShadow           VK_DrawAliasModelShadow
#define VK_DrawAliasModelPowerupShell     VK_NoOperationEntity
#define VK_DrawAlias3ModelPowerupShell    VK_NoOperationEntity
#define VK_DrawSpriteModel                VK_DrawSpriteModel
#define VK_DrawSimpleItem                 VK_DrawSimpleItem
#define VK_DrawClassicParticles           VK_DrawClassicParticles
#define VK_DrawDisc                       VK_NoOperation
#define VK_LightmapFrameInit              VK_LightmapFrameInit
#define VK_RenderDynamicLightmaps         VK_RenderDynamicLightmaps
#define VK_InvalidateLightmapTextures     VK_InvalidateLightmapTextures
#define VK_LightmapShutdown               VK_LightmapShutdown
#define VK_SetupGL                        VK_NoOperation
#define VK_ChainBrushModelSurfaces        VK_ChainBrushModelSurfaces
#define VK_DrawBrushModel                 VK_DrawBrushModel
#define VK_BrushModelCopyVertToBuffer     VK_BrushModelCopyVertToBuffer
#define VK_ClearRenderingSurface          VK_ClearRenderingSurface
#define VK_DrawWaterSurfaces              VK_DrawWaterSurfaces
#define VK_ScreenDrawStart                VK_NoOperation
#define VK_EnsureFinished                 VK_NoOperation
#define VK_Begin2DRendering               VK_NoOperation
#define VK_IsFramebufferEnabled3D         VK_False
#define VK_RenderView                     VK_RenderView
#define VK_PreRenderView                  VK_PreRenderView
#define VK_PostProcessScreen              VK_NoOperation
#define VK_BrightenScreen                 VK_NoOperation
#define VK_PolyBlend                      VK_NoOperationFloat4
#define VK_TimeRefresh                    VK_NoOperation
#define VK_Screenshot                     VK_Screenshot
#define VK_ScreenshotWidth                VK_ScreenshotWidth
#define VK_ScreenshotHeight               VK_ScreenshotHeight
#define VK_Prepare3DSprites               VK_Prepare3DSprites
#define VK_Draw3DSprites                  VK_Draw3DSprites
#define VK_Draw3DSpritesInline            VK_NoOperation
#define VK_RenderFramebuffers             VK_NoOperation
#define VK_FramebufferCreate              VK_FalseFramebuffer
#define VK_ProgramsInitialise             VK_NoOperation
#define VK_ProgramsShutdown               VK_NoOperationBool

#define RENDERER_METHOD(returntype, name, ...) renderer.name = VK_ ## name;
#include "r_renderer_structure.h"
#undef RENDERER_METHOD

	renderer.vaos_supported = VK_InitialiseVAOHandling();
}

#endif
