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

#include "vk_local.h"

static void VK_DestroySwapChainDepthResources(void)
{
	if (vk_options.swapChain.depthImageView != VK_NULL_HANDLE) {
		vkDestroyImageView(vk_options.logicalDevice, vk_options.swapChain.depthImageView, NULL);
		vk_options.swapChain.depthImageView = VK_NULL_HANDLE;
	}
	if (vk_options.swapChain.depthImage != VK_NULL_HANDLE) {
		vkDestroyImage(vk_options.logicalDevice, vk_options.swapChain.depthImage, NULL);
		vk_options.swapChain.depthImage = VK_NULL_HANDLE;
	}
	if (vk_options.swapChain.depthImageMemory != VK_NULL_HANDLE) {
		vkFreeMemory(vk_options.logicalDevice, vk_options.swapChain.depthImageMemory, NULL);
		vk_options.swapChain.depthImageMemory = VK_NULL_HANDLE;
	}
}

static qbool VK_CreateSwapChainDepthResources(void)
{
	VkImageViewCreateInfo createImageViewInfo;

	VK_DestroySwapChainDepthResources();

	if (!VK_CreateImageResource(
			vk_options.swapChain.imageSize.width,
			vk_options.swapChain.imageSize.height,
			1,
			vk_options.msaaSamples,
			VK_DepthFormat(),
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&vk_options.swapChain.depthImage,
			&vk_options.swapChain.depthImageMemory)) {
		Com_Printf("vulkan: failed to create depth image resource\n");
		return false;
	}

	VK_InitialiseStructure(createImageViewInfo);
	createImageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createImageViewInfo.image = vk_options.swapChain.depthImage;
	createImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createImageViewInfo.format = VK_DepthFormat();
	createImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	createImageViewInfo.subresourceRange.baseMipLevel = 0;
	createImageViewInfo.subresourceRange.levelCount = 1;
	createImageViewInfo.subresourceRange.baseArrayLayer = 0;
	createImageViewInfo.subresourceRange.layerCount = 1;

	{
		VkResult result = vkCreateImageView(vk_options.logicalDevice, &createImageViewInfo, NULL, &vk_options.swapChain.depthImageView);
		if (result != VK_SUCCESS) {
			Com_Printf("vulkan: vkCreateImageView() failed for depth buffer: %d\n", result);
			VK_DestroySwapChainDepthResources();
			return false;
		}
	}

	return true;
}

static void VK_DestroySwapChainMSAAColorResources(void)
{
	if (vk_options.swapChain.msaaColorImageView != VK_NULL_HANDLE) {
		vkDestroyImageView(vk_options.logicalDevice, vk_options.swapChain.msaaColorImageView, NULL);
		vk_options.swapChain.msaaColorImageView = VK_NULL_HANDLE;
	}
	if (vk_options.swapChain.msaaColorImage != VK_NULL_HANDLE) {
		vkDestroyImage(vk_options.logicalDevice, vk_options.swapChain.msaaColorImage, NULL);
		vk_options.swapChain.msaaColorImage = VK_NULL_HANDLE;
	}
	if (vk_options.swapChain.msaaColorImageMemory != VK_NULL_HANDLE) {
		vkFreeMemory(vk_options.logicalDevice, vk_options.swapChain.msaaColorImageMemory, NULL);
		vk_options.swapChain.msaaColorImageMemory = VK_NULL_HANDLE;
	}
}

// Only called when vk_options.msaaSamples > VK_SAMPLE_COUNT_1_BIT (see
// VK_CreateSwapChainFramebuffers). This image is the multisampled render
// target the main render pass actually draws into; the render pass resolves
// it straight into the real swapchain image via a resolve attachment, so it's
// never sampled or read back -- TRANSIENT_ATTACHMENT_BIT lets tile-based GPUs
// (most Android hardware) keep it in on-chip tile memory instead of writing
// it out to VRAM, which is the whole point of doing MSAA this way instead of
// through an offscreen target meant to be read later.
static qbool VK_CreateSwapChainMSAAColorResources(void)
{
	VkImageViewCreateInfo createImageViewInfo;

	VK_DestroySwapChainMSAAColorResources();

	if (!VK_CreateImageResource(
			vk_options.swapChain.imageSize.width,
			vk_options.swapChain.imageSize.height,
			1,
			vk_options.msaaSamples,
			vk_options.physicalDeviceSurfaceFormat.format,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&vk_options.swapChain.msaaColorImage,
			&vk_options.swapChain.msaaColorImageMemory)) {
		Com_Printf("vulkan: failed to create MSAA color image resource\n");
		return false;
	}

	VK_InitialiseStructure(createImageViewInfo);
	createImageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createImageViewInfo.image = vk_options.swapChain.msaaColorImage;
	createImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createImageViewInfo.format = vk_options.physicalDeviceSurfaceFormat.format;
	createImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	createImageViewInfo.subresourceRange.baseMipLevel = 0;
	createImageViewInfo.subresourceRange.levelCount = 1;
	createImageViewInfo.subresourceRange.baseArrayLayer = 0;
	createImageViewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(vk_options.logicalDevice, &createImageViewInfo, NULL, &vk_options.swapChain.msaaColorImageView) != VK_SUCCESS) {
		Com_Printf("vulkan: vkCreateImageView() failed for MSAA color buffer\n");
		VK_DestroySwapChainMSAAColorResources();
		return false;
	}

	return true;
}

static void VK_DestroyPostProcessDescriptors(void)
{
	if (vk_options.swapChain.postProcessDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(vk_options.logicalDevice, vk_options.swapChain.postProcessDescriptorPool, NULL);
		vk_options.swapChain.postProcessDescriptorPool = VK_NULL_HANDLE;
	}
	Q_free(vk_options.swapChain.postProcessDescriptorSets);
	vk_options.swapChain.postProcessDescriptorSets = NULL;
}

void VK_DestroyPostProcessResources(void)
{
	uint32_t i;

	VK_DestroyPostProcessDescriptors();

	if (vk_options.swapChain.postProcessFramebuffers) {
		for (i = 0; i < vk_options.swapChain.imageCount; ++i) {
			if (vk_options.swapChain.postProcessFramebuffers[i] != VK_NULL_HANDLE) {
				vkDestroyFramebuffer(vk_options.logicalDevice, vk_options.swapChain.postProcessFramebuffers[i], NULL);
			}
		}
		Q_free(vk_options.swapChain.postProcessFramebuffers);
		vk_options.swapChain.postProcessFramebuffers = NULL;
	}

	if (vk_options.swapChain.postProcessCompositeFramebuffers) {
		for (i = 0; i < vk_options.swapChain.imageCount; ++i) {
			if (vk_options.swapChain.postProcessCompositeFramebuffers[i] != VK_NULL_HANDLE) {
				vkDestroyFramebuffer(vk_options.logicalDevice, vk_options.swapChain.postProcessCompositeFramebuffers[i], NULL);
			}
		}
		Q_free(vk_options.swapChain.postProcessCompositeFramebuffers);
		vk_options.swapChain.postProcessCompositeFramebuffers = NULL;
	}

	if (vk_options.swapChain.postProcessColorImageViews) {
		for (i = 0; i < vk_options.swapChain.imageCount; ++i) {
			if (vk_options.swapChain.postProcessColorImageViews[i] != VK_NULL_HANDLE) {
				vkDestroyImageView(vk_options.logicalDevice, vk_options.swapChain.postProcessColorImageViews[i], NULL);
			}
		}
		Q_free(vk_options.swapChain.postProcessColorImageViews);
		vk_options.swapChain.postProcessColorImageViews = NULL;
	}

	if (vk_options.swapChain.postProcessColorImages) {
		for (i = 0; i < vk_options.swapChain.imageCount; ++i) {
			if (vk_options.swapChain.postProcessColorImages[i] != VK_NULL_HANDLE) {
				vkDestroyImage(vk_options.logicalDevice, vk_options.swapChain.postProcessColorImages[i], NULL);
			}
		}
		Q_free(vk_options.swapChain.postProcessColorImages);
		vk_options.swapChain.postProcessColorImages = NULL;
	}

	if (vk_options.swapChain.postProcessColorImageMemory) {
		for (i = 0; i < vk_options.swapChain.imageCount; ++i) {
			if (vk_options.swapChain.postProcessColorImageMemory[i] != VK_NULL_HANDLE) {
				vkFreeMemory(vk_options.logicalDevice, vk_options.swapChain.postProcessColorImageMemory[i], NULL);
			}
		}
		Q_free(vk_options.swapChain.postProcessColorImageMemory);
		vk_options.swapChain.postProcessColorImageMemory = NULL;
	}

	vk_options.swapChain.postProcessActive = false;
}

// Real gamma/contrast curve and/or FXAA only differ from the GLM/GLC shader
// path here -- this is what gates whether the main render pass draws into the
// offscreen target + composite pass, or straight into the swapchain image
// like before this feature existed. v_gamma/v_contrast/vid_framebuffer_fxaa
// are all unlatched (no vid_restart needed to change them), so this is a
// plain per-frame check, not a one-time decision -- the offscreen resources
// themselves are allocated unconditionally in VK_CreatePostProcessResources
// (called once per swapchain build) so toggling these cvars live never needs
// a restart.
qbool VK_PostProcessActive(void)
{
	extern cvar_t v_gamma, v_contrast;
	extern cvar_t vid_framebuffer_fxaa;
	extern cvar_t vid_software_palette;

	// Matches GLM_CompilePostProcessProgram()'s POST_PROCESS_PALETTE gate: the
	// real gamma/contrast curve is only a shader pass when vid_software_palette
	// is on -- otherwise GL leaves gamma to the OS/hardware gamma ramp and this
	// shader must stay a no-op, or any non-default gl_gamma/gl_contrast washes
	// the image out (e.g. gl_gamma 0.4 + gl_contrast 2 -> pow(2,0.4) > 1, clamps
	// to white) even though GL itself never applies that curve in this mode.
	if (!vid_software_palette.integer) {
		return vid_framebuffer_fxaa.integer != 0;
	}

	return v_gamma.value != 1.0f || v_contrast.value != 1.0f || vid_framebuffer_fxaa.integer != 0;
}

qbool VK_CreatePostProcessResources(void)
{
	uint32_t i;
	qbool msaa = vk_options.msaaSamples > VK_SAMPLE_COUNT_1_BIT;
	VkRenderPass mainRenderPass = VK_MainRenderPass();
	VkRenderPass compositeRenderPass = VK_PostProcessRenderPass();
	VkDescriptorPoolSize poolSize;
	VkDescriptorPoolCreateInfo poolInfo;

	VK_DestroyPostProcessResources();

	if (mainRenderPass == VK_NULL_HANDLE || compositeRenderPass == VK_NULL_HANDLE || !vk_options.swapChain.imageCount) {
		return false;
	}

	vk_options.swapChain.postProcessColorImages = Q_calloc(vk_options.swapChain.imageCount, sizeof(vk_options.swapChain.postProcessColorImages[0]));
	vk_options.swapChain.postProcessColorImageMemory = Q_calloc(vk_options.swapChain.imageCount, sizeof(vk_options.swapChain.postProcessColorImageMemory[0]));
	vk_options.swapChain.postProcessColorImageViews = Q_calloc(vk_options.swapChain.imageCount, sizeof(vk_options.swapChain.postProcessColorImageViews[0]));
	// postProcessFramebuffers: what the main render pass draws into (offscreen
	// target as attachment 0, same depth/MSAA attachments VK_CreateSwapChainFramebuffers
	// uses) -- bound in VK_BeginFrame when VK_PostProcessActive(). postProcessCompositeFramebuffers:
	// the composite pass's target -- single color attachment, the real
	// swapchain image view, built against VK_PostProcessRenderPass() -- bound
	// in VK_EndFrame via VK_PostProcessFramebuffer().
	vk_options.swapChain.postProcessFramebuffers = Q_calloc(vk_options.swapChain.imageCount, sizeof(vk_options.swapChain.postProcessFramebuffers[0]));
	vk_options.swapChain.postProcessCompositeFramebuffers = Q_calloc(vk_options.swapChain.imageCount, sizeof(vk_options.swapChain.postProcessCompositeFramebuffers[0]));

	for (i = 0; i < vk_options.swapChain.imageCount; ++i) {
		VkImageViewCreateInfo viewInfo;
		VkFramebufferCreateInfo framebufferInfo;
		VkImageView mainAttachments[3];

		if (!VK_CreateImageResource(
				vk_options.swapChain.imageSize.width,
				vk_options.swapChain.imageSize.height,
				1,
				VK_SAMPLE_COUNT_1_BIT,
				vk_options.physicalDeviceSurfaceFormat.format,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				&vk_options.swapChain.postProcessColorImages[i],
				&vk_options.swapChain.postProcessColorImageMemory[i])) {
			VK_DestroyPostProcessResources();
			return false;
		}

		VK_InitialiseStructure(viewInfo);
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = vk_options.swapChain.postProcessColorImages[i];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = vk_options.physicalDeviceSurfaceFormat.format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(vk_options.logicalDevice, &viewInfo, NULL, &vk_options.swapChain.postProcessColorImageViews[i]) != VK_SUCCESS) {
			VK_DestroyPostProcessResources();
			return false;
		}

		if (msaa) {
			mainAttachments[0] = vk_options.swapChain.msaaColorImageView;
			mainAttachments[1] = vk_options.swapChain.depthImageView;
			mainAttachments[2] = vk_options.swapChain.postProcessColorImageViews[i];
		}
		else {
			mainAttachments[0] = vk_options.swapChain.postProcessColorImageViews[i];
			mainAttachments[1] = vk_options.swapChain.depthImageView;
		}

		VK_InitialiseStructure(framebufferInfo);
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = mainRenderPass;
		framebufferInfo.attachmentCount = msaa ? 3 : 2;
		framebufferInfo.pAttachments = mainAttachments;
		framebufferInfo.width = vk_options.swapChain.imageSize.width;
		framebufferInfo.height = vk_options.swapChain.imageSize.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(vk_options.logicalDevice, &framebufferInfo, NULL, &vk_options.swapChain.postProcessFramebuffers[i]) != VK_SUCCESS) {
			VK_DestroyPostProcessResources();
			return false;
		}

		VK_InitialiseStructure(framebufferInfo);
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = compositeRenderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &vk_options.swapChain.imageViews[i];
		framebufferInfo.width = vk_options.swapChain.imageSize.width;
		framebufferInfo.height = vk_options.swapChain.imageSize.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(vk_options.logicalDevice, &framebufferInfo, NULL, &vk_options.swapChain.postProcessCompositeFramebuffers[i]) != VK_SUCCESS) {
			VK_DestroyPostProcessResources();
			return false;
		}
	}

	VK_InitialiseStructure(poolSize);
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize.descriptorCount = vk_options.swapChain.imageCount;

	VK_InitialiseStructure(poolInfo);
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = vk_options.swapChain.imageCount;

	if (vkCreateDescriptorPool(vk_options.logicalDevice, &poolInfo, NULL, &vk_options.swapChain.postProcessDescriptorPool) != VK_SUCCESS) {
		VK_DestroyPostProcessResources();
		return false;
	}

	vk_options.swapChain.postProcessDescriptorSets = Q_calloc(vk_options.swapChain.imageCount, sizeof(vk_options.swapChain.postProcessDescriptorSets[0]));

	vk_options.swapChain.postProcessActive = true;
	return true;
}

VkFramebuffer VK_PostProcessFramebuffer(uint32_t imageIndex)
{
	if (!vk_options.swapChain.postProcessFramebuffers || imageIndex >= vk_options.swapChain.imageCount) {
		return VK_NULL_HANDLE;
	}
	return vk_options.swapChain.postProcessFramebuffers[imageIndex];
}

VkFramebuffer VK_PostProcessCompositeFramebuffer(uint32_t imageIndex)
{
	if (!vk_options.swapChain.postProcessCompositeFramebuffers || imageIndex >= vk_options.swapChain.imageCount) {
		return VK_NULL_HANDLE;
	}
	return vk_options.swapChain.postProcessCompositeFramebuffers[imageIndex];
}

qbool VK_CreateSwapChain(SDL_Window* window, VkInstance instance, VkSurfaceKHR surface)
{
	uint32_t requestedImageCount;
	uint32_t queueFamilyIndices[2];
	uint32_t swapChainImageCount;
	uint32_t i;
	VkSwapchainCreateInfoKHR createInfo = { 0 };

	requestedImageCount = vk_options.physicalDeviceSurfaceCapabilities.minImageCount;
	// The "+1" is only needed to turn a tight (2-image) minimum into the 3
	// images MAILBOX needs to be truly non-blocking. Some WSI drivers already
	// report a generous minImageCount for the surface; piling another image
	// on top of that can exceed what the platform's present queue can
	// actually keep acquired at once.
	//
	// IMMEDIATE gets the same bump: some WSI implementations (observed on
	// Wayland/RADV) don't release swapchain images back to the app as fast
	// as a spec-compliant non-blocking IMMEDIATE present implies -- buffer
	// release ends up paced by the compositor's own repaint cadence unless
	// the compositor negotiates an explicit tearing protocol
	// (wp_tearing_control_v1) with the driver, which isn't universal. With
	// only 2 images that shows up as vkAcquireNextImageKHR effectively
	// blocking at vsync cadence despite IMMEDIATE being selected -- a strict
	// FPS cap at monitor refresh even with vid_vsync 0 and cl_maxfps
	// unbounded. A 3rd image gives the CPU more slack before it has to wait
	// on a release; doesn't fix a compositor that refuses to tear at all,
	// but removes this as a contributing factor.
	if ((vk_options.physicalDevicePresentationMode == VK_PRESENT_MODE_MAILBOX_KHR ||
		vk_options.physicalDevicePresentationMode == VK_PRESENT_MODE_IMMEDIATE_KHR) &&
		vk_options.physicalDeviceSurfaceCapabilities.minImageCount < 3) {
		requestedImageCount += 1;
	}
	if (vk_options.physicalDeviceSurfaceCapabilities.maxImageCount > 0) {
		requestedImageCount = min(requestedImageCount, vk_options.physicalDeviceSurfaceCapabilities.maxImageCount);
	}

	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.minImageCount = requestedImageCount;
	createInfo.surface = surface;
	createInfo.imageArrayLayers = 1;
	createInfo.imageColorSpace = vk_options.physicalDeviceSurfaceFormat.colorSpace;
	createInfo.imageFormat = vk_options.physicalDeviceSurfaceFormat.format;
	if (vk_options.physicalDeviceSurfaceCapabilities.currentExtent.width != ~(uint32_t)0) {
		createInfo.imageExtent = vk_options.physicalDeviceSurfaceCapabilities.currentExtent;
	}
	else {
		int width, height;

		SDL_GetWindowSizeInPixels(window, &width, &height);

		width = bound(vk_options.physicalDeviceSurfaceCapabilities.minImageExtent.width, width, vk_options.physicalDeviceSurfaceCapabilities.maxImageExtent.width);
		height = bound(vk_options.physicalDeviceSurfaceCapabilities.minImageExtent.height, height, vk_options.physicalDeviceSurfaceCapabilities.maxImageExtent.height);

		createInfo.imageExtent.width = width;
		createInfo.imageExtent.height = height;
	}
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // VK_IMAGE_USAGE_TRANSFER_DST_BIT if pre-processing enabled
	if (VK_PhysicalDeviceGraphicsQueueFamilyIndex() != VK_PhysicalDevicePresentQueueFamilyIndex()) {
		queueFamilyIndices[0] = VK_PhysicalDeviceGraphicsQueueFamilyIndex();
		queueFamilyIndices[1] = VK_PhysicalDevicePresentQueueFamilyIndex();

		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = NULL;
	}
	{
		createInfo.preTransform = vk_options.physicalDeviceSurfaceCapabilities.currentTransform;
	}
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = vk_options.physicalDevicePresentationMode;
	createInfo.clipped = VK_FALSE; // meag: setting this to false so we can read-back for screenshots
	createInfo.oldSwapchain = vk_options.swapChain.handle;

	{
		VkResult result = vkCreateSwapchainKHR(vk_options.logicalDevice, &createInfo, NULL, &vk_options.swapChain.handle);
		if (result != VK_SUCCESS) {
			Com_Printf("vulkan: vkCreateSwapchainKHR() failed: %d\n", result);
			return false;
		}
	}

	// Create images
	Q_free(vk_options.swapChain.images);
	if (vkGetSwapchainImagesKHR(vk_options.logicalDevice, vk_options.swapChain.handle, &swapChainImageCount, NULL) != VK_SUCCESS) {
		Com_Printf("vulkan: vkGetSwapchainImagesKHR() (count query) failed\n");
		return false;
	}
	vk_options.swapChain.images = Q_malloc(swapChainImageCount * sizeof(vk_options.swapChain.images[0]));
	if (vkGetSwapchainImagesKHR(vk_options.logicalDevice, vk_options.swapChain.handle, &swapChainImageCount, vk_options.swapChain.images) != VK_SUCCESS) {
		Com_Printf("vulkan: vkGetSwapchainImagesKHR() failed\n");
		Q_free(vk_options.swapChain.images);
		return false;
	}
	vk_options.swapChain.imageCount = swapChainImageCount;
	vk_options.swapChain.imageSize = createInfo.imageExtent;
	// TEMP diagnostic (frame-cap-despite-IMMEDIATE investigation).
	Com_Printf("vulkan: swapchain created with %u images (requested %u), present mode %d\n",
		swapChainImageCount, requestedImageCount, (int)vk_options.physicalDevicePresentationMode);

	// Create image views
	vk_options.swapChain.imageViews = Q_malloc(swapChainImageCount * sizeof(vk_options.swapChain.imageViews[0]));
	for (i = 0; i < vk_options.swapChain.imageCount; ++i) {
		VkImageViewCreateInfo createImageViewInfo = { 0 };
		createImageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createImageViewInfo.image = vk_options.swapChain.images[i];
		createImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createImageViewInfo.format = createInfo.imageFormat;
		createImageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createImageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createImageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createImageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createImageViewInfo.subresourceRange.baseMipLevel = 0;
		createImageViewInfo.subresourceRange.levelCount = 1;
		createImageViewInfo.subresourceRange.baseArrayLayer = 0;
		createImageViewInfo.subresourceRange.layerCount = 1;
		if (vkCreateImageView(vk_options.logicalDevice, &createImageViewInfo, NULL, &vk_options.swapChain.imageViews[i]) != VK_SUCCESS) {
			Com_Printf("vulkan: vkCreateImageView() failed for swapchain image %d\n", i);
			return false;
		}
	}

	return true;
}

qbool VK_CreateSwapChainFramebuffers(void)
{
	uint32_t i;
	VkRenderPass renderPass = VK_MainRenderPass();

	if (renderPass == VK_NULL_HANDLE || !vk_options.swapChain.imageViews) {
		Com_Printf("vulkan: VK_CreateSwapChainFramebuffers() called before render pass/swapchain images were ready\n");
		return false;
	}

	if (!VK_CreateSwapChainDepthResources()) {
		return false;
	}

	if (vk_options.msaaSamples > VK_SAMPLE_COUNT_1_BIT && !VK_CreateSwapChainMSAAColorResources()) {
		return false;
	}

	vk_options.swapChain.framebuffers = Q_calloc(vk_options.swapChain.imageCount, sizeof(vk_options.swapChain.framebuffers[0]));
	for (i = 0; i < vk_options.swapChain.imageCount; ++i) {
		// With MSAA, the render pass's color attachment 0 is the shared
		// multisampled image (one resource for every swapchain image, like
		// depth above) and the per-image swapchain view only appears as the
		// resolve attachment (2) -- see VK_RenderPassCreateVariant. Without
		// MSAA, attachment 0 is the swapchain image directly, same as before
		// this feature existed.
		qbool msaa = vk_options.msaaSamples > VK_SAMPLE_COUNT_1_BIT;
		VkImageView attachments[3];
		VkFramebufferCreateInfo framebufferInfo = { 0 };

		if (msaa) {
			attachments[0] = vk_options.swapChain.msaaColorImageView;
			attachments[1] = vk_options.swapChain.depthImageView;
			attachments[2] = vk_options.swapChain.imageViews[i];
		}
		else {
			attachments[0] = vk_options.swapChain.imageViews[i];
			attachments[1] = vk_options.swapChain.depthImageView;
		}

		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = msaa ? 3 : 2;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = vk_options.swapChain.imageSize.width;
		framebufferInfo.height = vk_options.swapChain.imageSize.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(vk_options.logicalDevice, &framebufferInfo, NULL, &vk_options.swapChain.framebuffers[i]) != VK_SUCCESS) {
			Com_Printf("vulkan: vkCreateFramebuffer() failed for swapchain image %d\n", i);
			VK_DestroySwapChainFramebuffers();
			return false;
		}
	}

	// Allocated unconditionally alongside the main framebuffers (not gated on
	// VK_PostProcessActive() right now) since v_gamma/v_contrast/
	// vid_framebuffer_fxaa can all change without a vid_restart -- see the
	// comment on VK_PostProcessActive itself.
	if (!VK_CreatePostProcessResources()) {
		VK_DestroySwapChainFramebuffers();
		return false;
	}

	return true;
}

void VK_DestroySwapChainFramebuffers(void)
{
	VK_DestroyPostProcessResources();

	if (vk_options.swapChain.framebuffers) {
		uint32_t i;

		for (i = 0; i < vk_options.swapChain.imageCount; ++i) {
			if (vk_options.swapChain.framebuffers[i] != VK_NULL_HANDLE) {
				vkDestroyFramebuffer(vk_options.logicalDevice, vk_options.swapChain.framebuffers[i], NULL);
			}
		}

		Q_free(vk_options.swapChain.framebuffers);
		vk_options.swapChain.framebuffers = NULL;
	}

	VK_DestroySwapChainMSAAColorResources();
	VK_DestroySwapChainDepthResources();
}

void VK_DestroySwapChain(void)
{
	VK_DestroySwapChainFramebuffers();

	if (vk_options.swapChain.imageViews) {
		uint32_t i;

		for (i = 0; i < vk_options.swapChain.imageCount; ++i) {
			vkDestroyImageView(vk_options.logicalDevice, vk_options.swapChain.imageViews[i], NULL);
		}

		Q_free(vk_options.swapChain.imageViews);
		vk_options.swapChain.imageViews = NULL;
	}

	if (vk_options.swapChain.handle != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(vk_options.logicalDevice, vk_options.swapChain.handle, NULL);
		vk_options.swapChain.handle = VK_NULL_HANDLE;
	}

	Q_free(vk_options.swapChain.images);
	vk_options.swapChain.images = NULL;
	vk_options.swapChain.imageCount = 0;
}

#endif // RENDERER_OPTION_VULKAN
