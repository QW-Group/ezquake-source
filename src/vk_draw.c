/*
Copyright (C) 2026 ezQuake team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/

// vk_draw.c

#ifdef RENDERER_OPTION_VULKAN

#include <vulkan/vulkan.h>
#include "quakedef.h"

#include "glm_draw.h"
#include "gl_model.h"
#include "r_buffers.h"
#include "r_draw.h"
#include "r_matrix.h"
#include "r_renderer.h"
#include "r_state.h"
#include "r_texture_internal.h"
#include "tr_types.h"
#include "glsl/constants.glsl"
#include "vk_local.h"

void Atlas_SolidTextureCoordinates(texture_ref* ref, float* s, float* t);

extern float overall_alpha;
extern float cachedMatrix[16];
extern cvar_t gl_alphafont;
extern cvar_t r_smoothalphahack;

extern const unsigned char vk_hud_image_vert_spv[];
extern const unsigned int vk_hud_image_vert_spv_len;
extern const unsigned char vk_hud_image_frag_spv[];
extern const unsigned int vk_hud_image_frag_spv_len;
extern const unsigned char vk_hud_color_vert_spv[];
extern const unsigned int vk_hud_color_vert_spv_len;
extern const unsigned char vk_hud_color_frag_spv[];
extern const unsigned int vk_hud_color_frag_spv_len;

typedef struct vk_hud_image_push_s {
	float alphaTestFont;
	int premultAlphaHack;
	int unused0;
	int unused1;
} vk_hud_image_push_t;

typedef struct vk_hud_color_push_s {
	float color[4];
} vk_hud_color_push_t;

static VkPipelineLayout hudImagePipelineLayout;
static VkPipelineLayout hudColorPipelineLayout;
static VkPipeline hudImagePipeline;
static VkPipeline hudCircleFillPipeline;
static VkPipeline hudCircleLinePipeline;
static qbool hudImageBufferDirty;

static glm_image_t lineQuadData[MAX_LINES_PER_FRAME * 4];

static void VK_SetCoordinates(glm_image_t* targ, float x1, float y1, float x2, float y2, float s, float s_width, float t, float t_height, int flags)
{
	float v1[4] = { x1, y1, 0, 1 };
	float v2[4] = { x2, y2, 0, 1 };

	R_MultiplyVector(cachedMatrix, v1, v1);
	R_MultiplyVector(cachedMatrix, v2, v2);

	targ[0].pos[0] = v1[0]; targ[0].tex[0] = s;
	targ[1].pos[0] = v1[0]; targ[1].tex[0] = s;
	targ[2].pos[0] = v2[0]; targ[2].tex[0] = s + s_width;
	targ[3].pos[0] = v2[0]; targ[3].tex[0] = s + s_width;
	targ[0].pos[1] = v1[1]; targ[0].tex[1] = t;
	targ[1].pos[1] = v2[1]; targ[1].tex[1] = t + t_height;
	targ[2].pos[1] = v1[1]; targ[2].tex[1] = t;
	targ[3].pos[1] = v2[1]; targ[3].tex[1] = t + t_height;

	targ[0].flags = targ[1].flags = targ[2].flags = targ[3].flags = flags;
}

static VkShaderModule VK_HudCreateShaderModule(const unsigned char* bytes, unsigned int length)
{
	VkShaderModuleCreateInfo createInfo;
	VkShaderModule shaderModule = VK_NULL_HANDLE;
	void* alignedCode;

	if (!bytes || !length || (length & 3)) {
		return VK_NULL_HANDLE;
	}

	alignedCode = Q_malloc(length);
	memcpy(alignedCode, bytes, length);

	VK_InitialiseStructure(createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = length;
	createInfo.pCode = (const uint32_t*)alignedCode;

	if (vkCreateShaderModule(vk_options.logicalDevice, &createInfo, NULL, &shaderModule) != VK_SUCCESS) {
		shaderModule = VK_NULL_HANDLE;
	}

	Q_free(alignedCode);
	return shaderModule;
}

static void VK_HudSetViewportScissor(VkCommandBuffer commandBuffer)
{
	VkViewport viewport;
	VkRect2D scissor;

	VK_InitialiseStructure(viewport);
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)vk_options.swapChain.imageSize.width;
	viewport.height = (float)vk_options.swapChain.imageSize.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VK_InitialiseStructure(scissor);
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent = vk_options.swapChain.imageSize;

	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

static qbool VK_HudCreateImagePipeline(void)
{
	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;
	VkPipelineShaderStageCreateInfo shaderStages[2];
	VkVertexInputBindingDescription bindingDescription;
	VkVertexInputAttributeDescription attributeDescriptions[4];
	VkPipelineVertexInputStateCreateInfo vertexInputInfo;
	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	VkPipelineViewportStateCreateInfo viewportState;
	VkPipelineRasterizationStateCreateInfo rasterizer;
	VkPipelineMultisampleStateCreateInfo multisampling;
	VkPipelineDepthStencilStateCreateInfo depthStencil;
	VkPipelineColorBlendAttachmentState blending;
	VkPipelineColorBlendStateCreateInfo colorBlending;
	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState;
	VkPushConstantRange pushConstantRange;
	VkPipelineLayoutCreateInfo pipelineLayoutInfo;
	VkGraphicsPipelineCreateInfo pipelineInfo;
	VkDescriptorSetLayout descriptorSetLayout;

	if (hudImagePipeline != VK_NULL_HANDLE) {
		return true;
	}

	descriptorSetLayout = VK_TextureDescriptorSetLayout();
	if (descriptorSetLayout == VK_NULL_HANDLE) {
		return false;
	}

	vertShaderModule = VK_HudCreateShaderModule(vk_hud_image_vert_spv, vk_hud_image_vert_spv_len);
	fragShaderModule = VK_HudCreateShaderModule(vk_hud_image_frag_spv, vk_hud_image_frag_spv_len);
	if (vertShaderModule == VK_NULL_HANDLE || fragShaderModule == VK_NULL_HANDLE) {
		return false;
	}

	VK_InitialiseStructure(shaderStages[0]);
	shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStages[0].module = vertShaderModule;
	shaderStages[0].pName = "main";

	VK_InitialiseStructure(shaderStages[1]);
	shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStages[1].module = fragShaderModule;
	shaderStages[1].pName = "main";

	VK_InitialiseStructure(bindingDescription);
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(glm_image_t);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VK_InitialiseStructure(attributeDescriptions[0]);
	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[0].offset = VK_VBO_FIELDOFFSET(glm_image_t, pos);

	VK_InitialiseStructure(attributeDescriptions[1]);
	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[1].offset = VK_VBO_FIELDOFFSET(glm_image_t, tex);

	VK_InitialiseStructure(attributeDescriptions[2]);
	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R8G8B8A8_UNORM;
	attributeDescriptions[2].offset = VK_VBO_FIELDOFFSET(glm_image_t, colour);

	VK_InitialiseStructure(attributeDescriptions[3]);
	attributeDescriptions[3].binding = 0;
	attributeDescriptions[3].location = 3;
	attributeDescriptions[3].format = VK_FORMAT_R32_SINT;
	attributeDescriptions[3].offset = VK_VBO_FIELDOFFSET(glm_image_t, flags);

	VK_InitialiseStructure(vertexInputInfo);
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = sizeof(attributeDescriptions) / sizeof(attributeDescriptions[0]);
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

	VK_InitialiseStructure(inputAssembly);
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	inputAssembly.primitiveRestartEnable = VK_TRUE;

	VK_InitialiseStructure(viewportState);
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VK_InitialiseStructure(rasterizer);
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

	VK_InitialiseStructure(multisampling);
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VK_InitialiseStructure(depthStencil);
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_FALSE;
	depthStencil.depthWriteEnable = VK_FALSE;

	VK_BlendingConfigure(&colorBlending, &blending, r_blendfunc_premultiplied_alpha);

	VK_InitialiseStructure(dynamicState);
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
	dynamicState.pDynamicStates = dynamicStates;

	VK_InitialiseStructure(pushConstantRange);
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(vk_hud_image_push_t);

	VK_InitialiseStructure(pipelineLayoutInfo);
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
	if (vkCreatePipelineLayout(vk_options.logicalDevice, &pipelineLayoutInfo, NULL, &hudImagePipelineLayout) != VK_SUCCESS) {
		vkDestroyShaderModule(vk_options.logicalDevice, fragShaderModule, NULL);
		vkDestroyShaderModule(vk_options.logicalDevice, vertShaderModule, NULL);
		return false;
	}

	VK_InitialiseStructure(pipelineInfo);
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = hudImagePipelineLayout;
	pipelineInfo.renderPass = VK_MainRenderPass();
	pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(vk_options.logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &hudImagePipeline) != VK_SUCCESS) {
		hudImagePipeline = VK_NULL_HANDLE;
	}

	vkDestroyShaderModule(vk_options.logicalDevice, fragShaderModule, NULL);
	vkDestroyShaderModule(vk_options.logicalDevice, vertShaderModule, NULL);
	return hudImagePipeline != VK_NULL_HANDLE;
}

static qbool VK_HudCreateColorPipeline(VkPrimitiveTopology topology, VkPipeline* pipeline)
{
	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;
	VkPipelineShaderStageCreateInfo shaderStages[2];
	VkVertexInputBindingDescription bindingDescription;
	VkVertexInputAttributeDescription attributeDescription;
	VkPipelineVertexInputStateCreateInfo vertexInputInfo;
	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	VkPipelineViewportStateCreateInfo viewportState;
	VkPipelineRasterizationStateCreateInfo rasterizer;
	VkPipelineMultisampleStateCreateInfo multisampling;
	VkPipelineDepthStencilStateCreateInfo depthStencil;
	VkPipelineColorBlendAttachmentState blending;
	VkPipelineColorBlendStateCreateInfo colorBlending;
	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState;
	VkPushConstantRange pushConstantRange;
	VkPipelineLayoutCreateInfo pipelineLayoutInfo;
	VkGraphicsPipelineCreateInfo pipelineInfo;

	if (*pipeline != VK_NULL_HANDLE) {
		return true;
	}

	vertShaderModule = VK_HudCreateShaderModule(vk_hud_color_vert_spv, vk_hud_color_vert_spv_len);
	fragShaderModule = VK_HudCreateShaderModule(vk_hud_color_frag_spv, vk_hud_color_frag_spv_len);
	if (vertShaderModule == VK_NULL_HANDLE || fragShaderModule == VK_NULL_HANDLE) {
		return false;
	}

	if (hudColorPipelineLayout == VK_NULL_HANDLE) {
		VK_InitialiseStructure(pushConstantRange);
		pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(vk_hud_color_push_t);

		VK_InitialiseStructure(pipelineLayoutInfo);
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		if (vkCreatePipelineLayout(vk_options.logicalDevice, &pipelineLayoutInfo, NULL, &hudColorPipelineLayout) != VK_SUCCESS) {
			vkDestroyShaderModule(vk_options.logicalDevice, fragShaderModule, NULL);
			vkDestroyShaderModule(vk_options.logicalDevice, vertShaderModule, NULL);
			return false;
		}
	}

	VK_InitialiseStructure(shaderStages[0]);
	shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStages[0].module = vertShaderModule;
	shaderStages[0].pName = "main";

	VK_InitialiseStructure(shaderStages[1]);
	shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStages[1].module = fragShaderModule;
	shaderStages[1].pName = "main";

	VK_InitialiseStructure(bindingDescription);
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(float) * 2;
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VK_InitialiseStructure(attributeDescription);
	attributeDescription.binding = 0;
	attributeDescription.location = 0;
	attributeDescription.format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescription.offset = 0;

	VK_InitialiseStructure(vertexInputInfo);
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = 1;
	vertexInputInfo.pVertexAttributeDescriptions = &attributeDescription;

	VK_InitialiseStructure(inputAssembly);
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = topology;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VK_InitialiseStructure(viewportState);
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VK_InitialiseStructure(rasterizer);
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

	VK_InitialiseStructure(multisampling);
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VK_InitialiseStructure(depthStencil);
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_FALSE;
	depthStencil.depthWriteEnable = VK_FALSE;

	VK_BlendingConfigure(&colorBlending, &blending, r_blendfunc_premultiplied_alpha);

	VK_InitialiseStructure(dynamicState);
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
	dynamicState.pDynamicStates = dynamicStates;

	VK_InitialiseStructure(pipelineInfo);
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = hudColorPipelineLayout;
	pipelineInfo.renderPass = VK_MainRenderPass();
	pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(vk_options.logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, pipeline) != VK_SUCCESS) {
		*pipeline = VK_NULL_HANDLE;
	}

	vkDestroyShaderModule(vk_options.logicalDevice, fragShaderModule, NULL);
	vkDestroyShaderModule(vk_options.logicalDevice, vertShaderModule, NULL);
	return *pipeline != VK_NULL_HANDLE;
}

static qbool VK_HudEnsureResources(void)
{
	if (!R_BufferReferenceIsValid(r_buffer_hud_image_vertex_data)) {
		if (!buffers.Create(r_buffer_hud_image_vertex_data, buffertype_vertex, "vk-hud-image-vbo", sizeof(imageData.images), NULL, bufferusage_once_per_frame)) {
			return false;
		}
	}
	if (!R_BufferReferenceIsValid(r_buffer_hud_image_index_data)) {
		int imageIndexLength = MAX_MULTI_IMAGE_BATCH * 5 * sizeof(uint32_t);
		uint32_t* imageIndexData = Q_malloc(imageIndexLength);
		uint32_t i;

		for (i = 0; i < MAX_MULTI_IMAGE_BATCH; ++i) {
			imageIndexData[i * 5 + 0] = i * 4;
			imageIndexData[i * 5 + 1] = i * 4 + 1;
			imageIndexData[i * 5 + 2] = i * 4 + 2;
			imageIndexData[i * 5 + 3] = i * 4 + 3;
			imageIndexData[i * 5 + 4] = ~(uint32_t)0;
		}
		if (!buffers.Create(r_buffer_hud_image_index_data, buffertype_index, "vk-hud-image-indexes", imageIndexLength, imageIndexData, bufferusage_reuse_many_frames)) {
			Q_free(imageIndexData);
			return false;
		}
		Q_free(imageIndexData);
	}
	if (!R_BufferReferenceIsValid(r_buffer_hud_circle_vertex_data)) {
		if (!buffers.Create(r_buffer_hud_circle_vertex_data, buffertype_vertex, "vk-hud-circle-vbo", sizeof(circleData.drawCirclePointData), NULL, bufferusage_once_per_frame)) {
			return false;
		}
	}

	if (!VK_HudCreateImagePipeline()) {
		return false;
	}
	if (!VK_HudCreateColorPipeline(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, &hudCircleFillPipeline)) {
		return false;
	}
	if (!VK_HudCreateColorPipeline(VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, &hudCircleLinePipeline)) {
		return false;
	}
	return true;
}

static qbool VK_HudBindImagePipeline(VkCommandBuffer commandBuffer, texture_ref texture, const char* draw_type, int start, int end)
{
	VkBuffer vertexBuffer;
	VkDeviceSize offsets[] = { 0 };
	VkDescriptorSet descriptorSet;
	vk_hud_image_push_t push;

	(void)draw_type;
	(void)start;
	(void)end;

	if (!VK_TextureReady(texture)) {
		return false;
	}

	descriptorSet = VK_TextureDescriptorSet(texture);
	if (descriptorSet == VK_NULL_HANDLE) {
		return false;
	}

	push.alphaTestFont = gl_alphafont.integer ? 0.0f : 1.0f;
	push.premultAlphaHack = r_smoothalphahack.integer ? 1 : 0;
	push.unused0 = push.unused1 = 0;

	vertexBuffer = VK_BufferHandle(r_buffer_hud_image_vertex_data);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hudImagePipeline);
	VK_HudSetViewportScissor(commandBuffer);
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hudImagePipelineLayout, 0, 1, &descriptorSet, 0, NULL);
	vkCmdPushConstants(commandBuffer, hudImagePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
	return true;
}

static void VK_HudUploadImageDataIfDirty(void)
{
	if (hudImageBufferDirty && imageData.imageCount) {
		buffers.Update(r_buffer_hud_image_vertex_data, sizeof(imageData.images[0]) * imageData.imageCount * 4, imageData.images);
		hudImageBufferDirty = false;
	}
}

void VK_HudDrawComplete(void)
{
}

void VK_HudPrepareCircles(void)
{
	if (!VK_HudEnsureResources()) {
		return;
	}
	if (circleData.circleCount) {
		buffers.Update(r_buffer_hud_circle_vertex_data, circleData.circleCount * FLOATS_PER_CIRCLE * sizeof(circleData.drawCirclePointData[0]), circleData.drawCirclePointData);
	}
}

void VK_HudDrawCircles(texture_ref texture, int start, int end)
{
	VkCommandBuffer commandBuffer;
	VkBuffer vertexBuffer;
	VkDeviceSize offsets[] = { 0 };
	int i;

	(void)texture;
	commandBuffer = VK_CurrentCommandBuffer();
	if (commandBuffer == VK_NULL_HANDLE || !VK_HudEnsureResources()) {
		return;
	}

	start = max(0, start);
	end = min(end, circleData.circleCount - 1);
	vertexBuffer = VK_BufferHandle(r_buffer_hud_circle_vertex_data);
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
	VK_HudSetViewportScissor(commandBuffer);

	for (i = start; i <= end; ++i) {
		vk_hud_color_push_t push;
		int firstVertex = i * FLOATS_PER_CIRCLE / 2;

		memcpy(push.color, circleData.drawCircleColors[i], sizeof(push.color));
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, circleData.drawCircleFill[i] ? hudCircleFillPipeline : hudCircleLinePipeline);
		vkCmdPushConstants(commandBuffer, hudColorPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
		vkCmdDraw(commandBuffer, circleData.drawCirclePoints[i], 1, firstVertex, 0);
	}
}

void VK_HudPrepareImages(void)
{
	if (!VK_HudEnsureResources()) {
		return;
	}
	if (imageData.imageCount) {
		buffers.Update(r_buffer_hud_image_vertex_data, sizeof(imageData.images[0]) * imageData.imageCount * 4, imageData.images);
		hudImageBufferDirty = false;
	}
}

void VK_HudDrawImages(texture_ref texture, int start, int end)
{
	VkCommandBuffer commandBuffer;
	VkBuffer indexBuffer;

	commandBuffer = VK_CurrentCommandBuffer();
	if (commandBuffer == VK_NULL_HANDLE || !VK_HudEnsureResources()) {
		return;
	}
	VK_HudUploadImageDataIfDirty();
	if (!VK_HudBindImagePipeline(commandBuffer, texture, "IMAGES", start, end)) {
		return;
	}

	indexBuffer = VK_BufferHandle(r_buffer_hud_image_index_data);
	vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(commandBuffer, (end - start + 1) * 5, 1, start * 5, 0, 0);
}

void VK_HudPreparePolygons(void)
{
}

void VK_HudDrawPolygons(texture_ref texture, int start, int end)
{
	VkCommandBuffer commandBuffer;
	int i;

	commandBuffer = VK_CurrentCommandBuffer();
	if (commandBuffer == VK_NULL_HANDLE || !VK_HudEnsureResources()) {
		return;
	}
	VK_HudUploadImageDataIfDirty();
	if (!VK_HudBindImagePipeline(commandBuffer, texture, "POLYGONS", start, end)) {
		return;
	}

	start = max(0, start);
	end = min(end, polygonData.polygonCount - 1);
	for (i = start; i <= end; ++i) {
		vkCmdDraw(commandBuffer, polygonData.polygonVerts[i], 1, polygonData.polygonImageIndexes[i], 0);
	}
}

void VK_HudPrepareLines(void)
{
}

static void VK_ExpandLineToQuad(glm_image_t* out, const glm_image_t* in, float thickness)
{
	float width = (float)max(1, vk_options.swapChain.imageSize.width);
	float height = (float)max(1, vk_options.swapChain.imageSize.height);
	float x0 = in[0].pos[0];
	float y0 = in[0].pos[1];
	float x1 = in[1].pos[0];
	float y1 = in[1].pos[1];
	float sx0 = (x0 * 0.5f + 0.5f) * width;
	float sy0 = (-y0 * 0.5f + 0.5f) * height;
	float sx1 = (x1 * 0.5f + 0.5f) * width;
	float sy1 = (-y1 * 0.5f + 0.5f) * height;
	float dx = sx1 - sx0;
	float dy = sy1 - sy0;
	float len = sqrt(dx * dx + dy * dy);
	float nx;
	float ny;
	float half = max(1.0f, thickness) * 0.5f;
	int i;

	if (len <= 0.0001f) {
		len = 1.0f;
	}
	nx = -dy / len * half;
	ny = dx / len * half;

	memcpy(&out[0], &in[0], sizeof(out[0]));
	memcpy(&out[1], &in[0], sizeof(out[1]));
	memcpy(&out[2], &in[1], sizeof(out[2]));
	memcpy(&out[3], &in[1], sizeof(out[3]));

#define SET_LINE_POINT(vertex, sx, sy) \
	out[vertex].pos[0] = ((sx) / width) * 2.0f - 1.0f; \
	out[vertex].pos[1] = -(((sy) / height) * 2.0f - 1.0f)

	SET_LINE_POINT(0, sx0 + nx, sy0 + ny);
	SET_LINE_POINT(1, sx0 - nx, sy0 - ny);
	SET_LINE_POINT(2, sx1 + nx, sy1 + ny);
	SET_LINE_POINT(3, sx1 - nx, sy1 - ny);
#undef SET_LINE_POINT

	for (i = 0; i < 4; ++i) {
		out[i].tex[0] = in[0].tex[0];
		out[i].tex[1] = in[0].tex[1];
		out[i].flags = IMAGEPROG_FLAGS_TEXTURE | IMAGEPROG_FLAGS_NEAREST;
	}
}

void VK_HudDrawLines(texture_ref texture, int start, int end)
{
	VkCommandBuffer commandBuffer;
	VkBuffer indexBuffer;
	int i;
	int lineCount;

	commandBuffer = VK_CurrentCommandBuffer();
	if (commandBuffer == VK_NULL_HANDLE || !VK_HudEnsureResources()) {
		return;
	}

	start = max(0, start);
	end = min(end, lineData.lineCount - 1);
	lineCount = end - start + 1;
	if (lineCount <= 0 || lineCount > MAX_LINES_PER_FRAME) {
		return;
	}

	for (i = 0; i < lineCount; ++i) {
		const glm_image_t* source = &imageData.images[lineData.imageIndex[start + i]];
		VK_ExpandLineToQuad(&lineQuadData[i * 4], source, lineData.line_thickness[start + i]);
	}
	buffers.Update(r_buffer_hud_image_vertex_data, sizeof(lineQuadData[0]) * lineCount * 4, lineQuadData);
	hudImageBufferDirty = true;

	if (!VK_HudBindImagePipeline(commandBuffer, texture, "LINES", 0, lineCount - 1)) {
		return;
	}

	indexBuffer = VK_BufferHandle(r_buffer_hud_image_index_data);
	vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(commandBuffer, lineCount * 5, 1, 0, 0, 0);
}

void VK_DrawImage(float x, float y, float width, float height, float tex_s, float tex_t, float tex_width, float tex_height, byte* color, int flags)
{
	float alpha;
	glm_image_t* img;

	if (imageData.imageCount >= MAX_MULTI_IMAGE_BATCH) {
		return;
	}

	alpha = (color[3] * overall_alpha / 255.0f);
	img = &imageData.images[imageData.imageCount * 4];

	img->colour[0] = (img + 1)->colour[0] = (img + 2)->colour[0] = (img + 3)->colour[0] = color[0] * alpha;
	img->colour[1] = (img + 1)->colour[1] = (img + 2)->colour[1] = (img + 3)->colour[1] = color[1] * alpha;
	img->colour[2] = (img + 1)->colour[2] = (img + 2)->colour[2] = (img + 3)->colour[2] = color[2] * alpha;
	img->colour[3] = (img + 1)->colour[3] = (img + 2)->colour[3] = (img + 3)->colour[3] = color[3] * overall_alpha;

	VK_SetCoordinates(img, x, y, x + width, y + height, tex_s, tex_width, tex_t, tex_height, flags);

	++imageData.imageCount;
}

void VK_DrawRectangle(float x, float y, float width, float height, byte* color)
{
	texture_ref solidTexture;
	float s;
	float t;
	float alpha;
	glm_image_t* img;

	if (imageData.imageCount >= MAX_MULTI_IMAGE_BATCH) {
		return;
	}

	Atlas_SolidTextureCoordinates(&solidTexture, &s, &t);
	if (!R_LogCustomImageTypeWithTexture(imagetype_image, imageData.imageCount, solidTexture)) {
		return;
	}

	alpha = (color[3] * overall_alpha / 255.0f);
	img = &imageData.images[imageData.imageCount * 4];

	img->colour[0] = (img + 1)->colour[0] = (img + 2)->colour[0] = (img + 3)->colour[0] = color[0] * alpha;
	img->colour[1] = (img + 1)->colour[1] = (img + 2)->colour[1] = (img + 3)->colour[1] = color[1] * alpha;
	img->colour[2] = (img + 1)->colour[2] = (img + 2)->colour[2] = (img + 3)->colour[2] = color[2] * alpha;
	img->colour[3] = (img + 1)->colour[3] = (img + 2)->colour[3] = (img + 3)->colour[3] = color[3] * overall_alpha;

	VK_SetCoordinates(img, x, y, x + width, y + height, s, 0, t, 0, IMAGEPROG_FLAGS_TEXTURE);

	++imageData.imageCount;
}

void VK_AdjustImages(int first, int last, float x_offset)
{
	int i;

	for (i = first; i < last; ++i) {
		imageData.images[i * 4 + 0].pos[0] += x_offset;
		imageData.images[i * 4 + 1].pos[0] += x_offset;
		imageData.images[i * 4 + 2].pos[0] += x_offset;
		imageData.images[i * 4 + 3].pos[0] += x_offset;
	}
}

void VK_HudResourcesShutdown(void)
{
	if (vk_options.logicalDevice == VK_NULL_HANDLE) {
		return;
	}

	if (hudImagePipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(vk_options.logicalDevice, hudImagePipeline, NULL);
		hudImagePipeline = VK_NULL_HANDLE;
	}
	if (hudCircleFillPipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(vk_options.logicalDevice, hudCircleFillPipeline, NULL);
		hudCircleFillPipeline = VK_NULL_HANDLE;
	}
	if (hudCircleLinePipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(vk_options.logicalDevice, hudCircleLinePipeline, NULL);
		hudCircleLinePipeline = VK_NULL_HANDLE;
	}
	if (hudImagePipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(vk_options.logicalDevice, hudImagePipelineLayout, NULL);
		hudImagePipelineLayout = VK_NULL_HANDLE;
	}
	if (hudColorPipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(vk_options.logicalDevice, hudColorPipelineLayout, NULL);
		hudColorPipelineLayout = VK_NULL_HANDLE;
	}
	hudImageBufferDirty = false;
}

void VK_HudSwapchainChanged(void)
{
	VK_HudResourcesShutdown();
}

#endif // #ifdef RENDERER_OPTION_VULKAN
