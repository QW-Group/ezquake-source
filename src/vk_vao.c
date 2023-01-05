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
#include "gl_model.h"
#include "r_local.h"
#include "r_vao.h"
#include "r_state.h"

#include "vk_local.h"

#define MAX_VAO_BINDINGS 16

typedef struct {
	VkVertexInputBindingDescription bindings[MAX_VAO_BINDINGS];
	VkVertexInputAttributeDescription attributes[MAX_VAO_BINDINGS];
	VkPipelineVertexInputStateCreateInfo vertexInfo;
} vk_vao_t;

static vk_vao_t vaos[vao_count];

void VK_ConfigureVertexAttribPointer(r_vao_id vao, buffer_ref vbo, uint32_t index, VkFormat format, int stride, uint32_t offset, qbool instanced)
{
	VkVertexInputBindingDescription* binding = vaos[vao].bindings + index;
	VkVertexInputAttributeDescription* attribute = vaos[vao].attributes + index;

	assert(index < MAX_VAO_BINDINGS);

	memset(attribute, 0, sizeof(*attribute));
	memset(binding, 0, sizeof(*binding));

	binding->binding = index;
	binding->stride = stride;
	binding->inputRate = (instanced ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX);

	attribute->binding = index;
	attribute->location = index;
	attribute->format = format;
	attribute->offset = offset;
}

qbool VK_CreateAliasModelPipeline(buffer_ref aliasModelVBO, buffer_ref instanceVBO)
{
	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	VkViewport viewport = { 0 };
	VkRect2D scissor = { 0 };
	VkPipelineViewportStateCreateInfo viewportState;
	VkPipelineRasterizationStateCreateInfo rasterizer;
	VkPipelineDepthStencilStateCreateInfo depthStencil;
	VkPipelineMultisampleStateCreateInfo multisampling;
	VkPipelineColorBlendAttachmentState blending;
	VkPipelineColorBlendStateCreateInfo colorBlending;
	VkPipeline pipelineLayout;
	VkPipelineLayoutCreateInfo pipelineLayoutInfo;

	VK_InitialiseStructure(inputAssembly);
	VK_InitialiseStructure(viewport);
	VK_InitialiseStructure(scissor);
	VK_InitialiseStructure(viewportState);
	VK_InitialiseStructure(rasterizer);
	VK_InitialiseStructure(depthStencil);
	VK_InitialiseStructure(multisampling);
	VK_InitialiseStructure(blending);
	VK_InitialiseStructure(pipelineLayoutInfo);

	VK_InitialiseStructure(vaos[vao_aliasmodel].vertexInfo);
	vaos[vao_aliasmodel].vertexInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vaos[vao_aliasmodel].vertexInfo.pVertexBindingDescriptions = vaos[vao_aliasmodel].bindings;
	vaos[vao_aliasmodel].vertexInfo.pVertexAttributeDescriptions = vaos[vao_aliasmodel].attributes;

	VK_ConfigureVertexAttribPointer(vao_aliasmodel, aliasModelVBO, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(vbo_model_vert_t), VK_VBO_FIELDOFFSET(vbo_model_vert_t, position), false);
	VK_ConfigureVertexAttribPointer(vao_aliasmodel, aliasModelVBO, 1, VK_FORMAT_R32G32_SFLOAT, sizeof(vbo_model_vert_t), VK_VBO_FIELDOFFSET(vbo_model_vert_t, texture_coords), false);
	VK_ConfigureVertexAttribPointer(vao_aliasmodel, aliasModelVBO, 2, VK_FORMAT_R32G32B32_SFLOAT, sizeof(vbo_model_vert_t), VK_VBO_FIELDOFFSET(vbo_model_vert_t, normal), false);
	VK_ConfigureVertexAttribPointer(vao_aliasmodel, instanceVBO, 3, VK_FORMAT_R32_UINT, sizeof(uint32_t), 0, true);
	//VK_ConfigureVertexAttribPointer(vao_aliasmodel, aliasModelVBO, 4, VK_FORMAT_R32_SINT, sizeof(vbo_model_vert_t), VK_VBO_FIELDOFFSET(vbo_model_vert_t, flags), false);
	
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)vk_options.swapChain.imageSize.width;
	viewport.height = (float)vk_options.swapChain.imageSize.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	scissor.offset.x = scissor.offset.y = 0;
	scissor.extent = vk_options.swapChain.imageSize;

	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = NULL; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	// depth-buffer: todo

	// blending modes
	VK_BlendingConfigure(&colorBlending, &blending, r_blendfunc_premultiplied_alpha);

	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 0; // Optional
	pipelineLayoutInfo.pSetLayouts = NULL; // Optional
	pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
	pipelineLayoutInfo.pPushConstantRanges = NULL; // Optional

	if (vkCreatePipelineLayout(vk_options.logicalDevice, &pipelineLayoutInfo, NULL, &pipelineLayout) != VK_SUCCESS) {
		return false;
	}

	return true;
}

void VK_DeleteVAOs(void)
{
}

void VK_GenVertexArray(r_vao_id vao)
{
}

qbool VK_VertexArrayCreated(r_vao_id vao)
{
	return false;
}

void VK_BindVertexArray(r_vao_id vao)
{
}

qbool VK_InitialiseVAOHandling(void)
{
	return true;
}

#endif // RENDERER_OPTION_VULKAN
