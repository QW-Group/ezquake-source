/*
Copyright (C) 2026 ezQuake team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/

#ifdef RENDERER_OPTION_VULKAN

#include <vulkan/vulkan.h>
#include "quakedef.h"

#include "vk_local.h"

static VkCommandPool immediateCommandPool;

uint32_t VK_FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memoryProperties;
	uint32_t i;

	vkGetPhysicalDeviceMemoryProperties(vk_options.physicalDevice, &memoryProperties);
	for (i = 0; i < memoryProperties.memoryTypeCount; ++i) {
		if ((type_filter & (1u << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	Sys_Error("vulkan: no compatible memory type found");
	return 0;
}

// memoryTypeSelector lets callers pick a memory type from the buffer's real
// memoryTypeBits instead of a fixed VkMemoryPropertyFlags match -- used by
// vk_buffers.c to prefer a DEVICE_LOCAL|HOST_VISIBLE heap when the device
// exposes one for this resource, falling back to plain HOST_VISIBLE
// otherwise (VK_BufferPreferredMemoryType). NULL selector keeps the
// original exact-flags behaviour via VK_FindMemoryType.
static qbool VK_CreateBufferResourceInternal(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
	uint32_t (*memoryTypeSelector)(VkMemoryPropertyFlags, uint32_t), VkBuffer* buffer, VkDeviceMemory* memory)
{
	VkBufferCreateInfo bufferInfo;
	VkMemoryRequirements memRequirements;
	VkMemoryAllocateInfo allocInfo;

	VK_InitialiseStructure(bufferInfo);
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (vkCreateBuffer(vk_options.logicalDevice, &bufferInfo, NULL, buffer) != VK_SUCCESS) {
		return false;
	}

	vkGetBufferMemoryRequirements(vk_options.logicalDevice, *buffer, &memRequirements);

	VK_InitialiseStructure(allocInfo);
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = memoryTypeSelector ?
		memoryTypeSelector(properties, memRequirements.memoryTypeBits) :
		VK_FindMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(vk_options.logicalDevice, &allocInfo, NULL, memory) != VK_SUCCESS) {
		vkDestroyBuffer(vk_options.logicalDevice, *buffer, NULL);
		*buffer = VK_NULL_HANDLE;
		return false;
	}

	vkBindBufferMemory(vk_options.logicalDevice, *buffer, *memory, 0);
	return true;
}

qbool VK_CreateBufferResource(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* memory)
{
	return VK_CreateBufferResourceInternal(size, usage, properties, NULL, buffer, memory);
}

qbool VK_CreateBufferResourceWithSelector(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
	uint32_t (*memoryTypeSelector)(VkMemoryPropertyFlags, uint32_t), VkBuffer* buffer, VkDeviceMemory* memory)
{
	return VK_CreateBufferResourceInternal(size, usage, properties, memoryTypeSelector, buffer, memory);
}

qbool VK_CreateImageResource(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits samples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* image, VkDeviceMemory* memory)
{
	VkImageCreateInfo imageInfo;
	VkMemoryRequirements memRequirements;
	VkMemoryAllocateInfo allocInfo;

	VK_InitialiseStructure(imageInfo);
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = max(1, mipLevels);
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = samples ? samples : VK_SAMPLE_COUNT_1_BIT;

	if (vkCreateImage(vk_options.logicalDevice, &imageInfo, NULL, image) != VK_SUCCESS) {
		return false;
	}

	vkGetImageMemoryRequirements(vk_options.logicalDevice, *image, &memRequirements);

	VK_InitialiseStructure(allocInfo);
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = VK_FindMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(vk_options.logicalDevice, &allocInfo, NULL, memory) != VK_SUCCESS) {
		vkDestroyImage(vk_options.logicalDevice, *image, NULL);
		*image = VK_NULL_HANDLE;
		return false;
	}

	vkBindImageMemory(vk_options.logicalDevice, *image, *memory, 0);
	return true;
}

VkCommandBuffer VK_BeginImmediateCommands(void)
{
	VkCommandPoolCreateInfo poolInfo;
	VkCommandBufferAllocateInfo allocInfo;
	VkCommandBufferBeginInfo beginInfo;
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

	if (immediateCommandPool == VK_NULL_HANDLE) {
		VK_InitialiseStructure(poolInfo);
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = VK_PhysicalDeviceGraphicsQueueFamilyIndex();
		if (vkCreateCommandPool(vk_options.logicalDevice, &poolInfo, NULL, &immediateCommandPool) != VK_SUCCESS) {
			return VK_NULL_HANDLE;
		}
	}

	VK_InitialiseStructure(allocInfo);
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = immediateCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;
	if (vkAllocateCommandBuffers(vk_options.logicalDevice, &allocInfo, &commandBuffer) != VK_SUCCESS) {
		return VK_NULL_HANDLE;
	}

	VK_InitialiseStructure(beginInfo);
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
		vkFreeCommandBuffers(vk_options.logicalDevice, immediateCommandPool, 1, &commandBuffer);
		return VK_NULL_HANDLE;
	}

	return commandBuffer;
}

qbool VK_EndImmediateCommands(VkCommandBuffer command_buffer)
{
	return VK_EndImmediateCommandsAfter(command_buffer, VK_NULL_HANDLE, 0);
}

// Same as VK_EndImmediateCommands, but the submit waits on waitSemaphore at
// waitStage first -- for command buffers (like VK_Screenshot's copy) that
// touch a swapchain image and must not start before the image has actually
// been returned by vkAcquireNextImageKHR's own semaphore signal.
qbool VK_EndImmediateCommandsAfter(VkCommandBuffer command_buffer, VkSemaphore waitSemaphore, VkPipelineStageFlags waitStage)
{
	VkSubmitInfo submitInfo;
	qbool success = true;

	if (command_buffer == VK_NULL_HANDLE) {
		return false;
	}

	vkEndCommandBuffer(command_buffer);

	VK_InitialiseStructure(submitInfo);
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &command_buffer;
	if (waitSemaphore != VK_NULL_HANDLE) {
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &waitSemaphore;
		submitInfo.pWaitDstStageMask = &waitStage;
	}

	if (vkQueueSubmit(vk_options.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
		success = false;
	}
	vkQueueWaitIdle(vk_options.graphicsQueue);
	vkFreeCommandBuffers(vk_options.logicalDevice, immediateCommandPool, 1, &command_buffer);

	return success;
}

void VK_DestroyImmediateCommandPool(void)
{
	if (immediateCommandPool != VK_NULL_HANDLE) {
		vkDestroyCommandPool(vk_options.logicalDevice, immediateCommandPool, NULL);
		immediateCommandPool = VK_NULL_HANDLE;
	}
}

#endif // #ifdef RENDERER_OPTION_VULKAN
