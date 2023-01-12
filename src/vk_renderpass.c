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

// vk_vao.c
// - Vulkan VAO-equivalent functions

#ifdef RENDERER_OPTION_VULKAN

#include <vulkan/vulkan.h>
#include "quakedef.h"
#include "r_state.h"

#include "vk_local.h"

typedef enum {
	vk_renderpass_none,

	vk_renderpass_count
} vk_renderpass_id;

static VkRenderPass renderPasses[vk_renderpass_count];

qbool VK_RenderPassCreate(vk_renderpass_id id)
{
	VkAttachmentDescription colorAttachment;
	VkAttachmentReference colorAttachmentRef;
	VkSubpassDescription subpass;
	VkRenderPassCreateInfo renderPassInfo;

	// single color buffer attachment
	VK_InitialiseStructure(colorAttachment);
	colorAttachment.format = vk_options.physicalDeviceSurfaceFormat.format;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// attachment reference
	VK_InitialiseStructure(colorAttachmentRef);
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Sub-passes
	VK_InitialiseStructure(subpass);
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	// Render pass
	VK_InitialiseStructure(renderPassInfo);
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	if (vkCreateRenderPass(vk_options.logicalDevice, &renderPassInfo, NULL, &renderPasses[id]) != VK_SUCCESS) {
		return false;
	}

	return true;
}

void VK_RenderPassDelete(vk_renderpass_id id)
{
	int i;

	for (i = 0; i < vk_renderpass_count; ++i) {
		if (renderPasses[i]) {
			vkDestroyRenderPass(vk_options.logicalDevice, renderPasses[i], NULL);
		}
	}
}

#endif // RENDERER_OPTION_VULKAN
