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

qbool VK_CreateSwapChain(SDL_Window* window, VkInstance instance, VkSurfaceKHR surface)
{
	uint32_t requestedImageCount;
	uint32_t queueFamilyIndices[2];
	uint32_t swapChainImageCount;
	uint32_t i;
	VkSwapchainCreateInfoKHR createInfo = { 0 };

	requestedImageCount = vk_options.physicalDeviceSurfaceCapabilities.minImageCount;
	if (vk_options.physicalDevicePresentationMode == VK_PRESENT_MODE_MAILBOX_KHR) {
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
	if (vk_options.physicalDeviceSurfaceCapabilities.currentExtent.width == ~(uint32_t)0) {
		createInfo.imageExtent = vk_options.physicalDeviceSurfaceCapabilities.currentExtent;
	}
	else {
		int width, height;

		SDL_GL_GetDrawableSize(window, &width, &height);

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
	createInfo.preTransform = vk_options.physicalDeviceSurfaceCapabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = vk_options.physicalDevicePresentationMode;
	createInfo.clipped = VK_FALSE; // meag: setting this to false so we can read-back for screenshots
	createInfo.oldSwapchain = vk_options.swapChain.handle;

	if (vkCreateSwapchainKHR(vk_options.logicalDevice, &createInfo, NULL, &vk_options.swapChain.handle) != VK_SUCCESS) {
		return false;
	}

	// Create images
	Q_free(vk_options.swapChain.images);
	if (vkGetSwapchainImagesKHR(vk_options.logicalDevice, vk_options.swapChain.handle, &swapChainImageCount, NULL) != VK_SUCCESS) {
		return false;
	}
	vk_options.swapChain.images = Q_malloc(swapChainImageCount * sizeof(vk_options.swapChain.images[0]));
	if (vkGetSwapchainImagesKHR(vk_options.logicalDevice, vk_options.swapChain.handle, &swapChainImageCount, vk_options.swapChain.images) != VK_SUCCESS) {
		Q_free(vk_options.swapChain.images);
		return false;
	}
	vk_options.swapChain.imageCount = swapChainImageCount;
	vk_options.swapChain.imageSize = createInfo.imageExtent;

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
			return false;
		}
	}

	return true;
}

void VK_DestroySwapChain(void)
{
	if (vk_options.swapChain.imageViews) {
		uint32_t i;

		for (i = 0; i < vk_options.swapChain.imageCount; ++i) {
			vkDestroyImageView(vk_options.logicalDevice, vk_options.swapChain.imageViews[i], NULL);
		}

		Q_free(vk_options.swapChain.imageViews);
	}

	if (vk_options.swapChain.handle != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(vk_options.logicalDevice, vk_options.swapChain.handle, NULL);
		vk_options.swapChain.handle = VK_NULL_HANDLE;
	}
}

#endif // RENDERER_OPTION_VULKAN
