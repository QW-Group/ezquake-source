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
#include "r_local.h"
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
extern const unsigned char vk_post_process_vert_spv[];
extern const unsigned int vk_post_process_vert_spv_len;
extern const unsigned char vk_post_process_frag_spv[];
extern const unsigned int vk_post_process_frag_spv_len;
extern const unsigned char vk_world_outline_frag_spv[];
extern const unsigned int vk_world_outline_frag_spv_len;

typedef struct vk_hud_image_push_s {
	float alphaTestFont;
	int premultAlphaHack;
	int unused0;
	int unused1;
} vk_hud_image_push_t;

typedef struct vk_hud_color_push_s {
	float color[4];
} vk_hud_color_push_t;

typedef struct vk_post_process_push_s {
	float blend[4];
	float gamma;
	float contrast;
	float invWidth;
	float invHeight;
	int fxaaEnabled;
	float fxaaQuality;
} vk_post_process_push_t;

typedef struct vk_world_outline_push_s {
	float outlineColor[3];
	float outlineScale;
	float outlineDepthThreshold;
	float outlineNormalThreshold;
	float invWidth;
	float invHeight;
	float zFar;
} vk_world_outline_push_t;

static VkPipelineLayout hudImagePipelineLayout;
static VkPipelineLayout hudColorPipelineLayout;
static VkPipeline hudImagePipeline;
static VkPipeline hudCircleFillPipeline;
static VkPipeline hudCircleLinePipeline;
static VkPipeline hudBrightenPipeline;
static qbool hudImageBufferDirty;

static VkDescriptorSetLayout postProcessDescriptorSetLayout;
static VkPipelineLayout postProcessPipelineLayout;
static VkPipeline postProcessPipeline;
static VkSampler postProcessSampler;

static VkDescriptorSetLayout worldOutlineDescriptorSetLayout;
static VkPipelineLayout worldOutlinePipelineLayout;
static VkPipeline worldOutlinePipeline;
// NEAREST, not postProcessSampler's LINEAR: the outline shader is a direct
// port of GLM's texelFetch()-based edge detect (fx_world_geometry.fragment.
// glsl) -- bilinear filtering here would blur exactly the discontinuities
// it's trying to measure.
static VkSampler worldNormalsSampler;

static glm_image_t lineQuadData[MAX_LINES_PER_FRAME * 4];

static void VK_SetCoordinates(glm_image_t* targ, float x1, float y1, float x2, float y2, float s, float s_width, float t, float t_height, int flags)
{
	// Transform all 4 corners independently rather than 2 diagonal corners
	// (v1,v2) and mixing v1.x/v2.y to build the other two -- that mixing is
	// only valid when cachedMatrix is axis-aligned (no x/y swap). Today
	// cachedMatrix is always axis-aligned on desktop, so this isn't an active
	// bug, but transforming all 4 corners is more robust to future matrix
	// changes and costs nothing measurable.
	float vTL[4] = { x1, y1, 0, 1 };
	float vTR[4] = { x2, y1, 0, 1 };
	float vBL[4] = { x1, y2, 0, 1 };
	float vBR[4] = { x2, y2, 0, 1 };

	R_MultiplyVector(cachedMatrix, vTL, vTL);
	R_MultiplyVector(cachedMatrix, vTR, vTR);
	R_MultiplyVector(cachedMatrix, vBL, vBL);
	R_MultiplyVector(cachedMatrix, vBR, vBR);

	targ[0].pos[0] = vTL[0]; targ[0].pos[1] = vTL[1]; targ[0].tex[0] = s;           targ[0].tex[1] = t;
	targ[1].pos[0] = vBL[0]; targ[1].pos[1] = vBL[1]; targ[1].tex[0] = s;           targ[1].tex[1] = t + t_height;
	targ[2].pos[0] = vTR[0]; targ[2].pos[1] = vTR[1]; targ[2].tex[0] = s + s_width; targ[2].tex[1] = t;
	targ[3].pos[0] = vBR[0]; targ[3].pos[1] = vBR[1]; targ[3].tex[0] = s + s_width; targ[3].tex[1] = t + t_height;

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
	multisampling.rasterizationSamples = vk_options.msaaSamples ? vk_options.msaaSamples : VK_SAMPLE_COUNT_1_BIT;

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

	if (vkCreateGraphicsPipelines(vk_options.logicalDevice, vk_options.pipelineCache, 1, &pipelineInfo, NULL, &hudImagePipeline) != VK_SUCCESS) {
		hudImagePipeline = VK_NULL_HANDLE;
	}

	vkDestroyShaderModule(vk_options.logicalDevice, fragShaderModule, NULL);
	vkDestroyShaderModule(vk_options.logicalDevice, vertShaderModule, NULL);
	return hudImagePipeline != VK_NULL_HANDLE;
}

static qbool VK_HudCreateColorPipeline(VkPrimitiveTopology topology, r_blendfunc_t blendFunc, VkPipeline* pipeline)
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
	multisampling.rasterizationSamples = vk_options.msaaSamples ? vk_options.msaaSamples : VK_SAMPLE_COUNT_1_BIT;

	VK_InitialiseStructure(depthStencil);
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_FALSE;
	depthStencil.depthWriteEnable = VK_FALSE;

	VK_BlendingConfigure(&colorBlending, &blending, blendFunc);

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

	if (vkCreateGraphicsPipelines(vk_options.logicalDevice, vk_options.pipelineCache, 1, &pipelineInfo, NULL, pipeline) != VK_SUCCESS) {
		*pipeline = VK_NULL_HANDLE;
	}

	vkDestroyShaderModule(vk_options.logicalDevice, fragShaderModule, NULL);
	vkDestroyShaderModule(vk_options.logicalDevice, vertShaderModule, NULL);
	return *pipeline != VK_NULL_HANDLE;
}

static qbool VK_PostProcessCreatePipeline(void)
{
	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;
	VkPipelineShaderStageCreateInfo shaderStages[2];
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
	VkSamplerCreateInfo samplerInfo;
	VkDescriptorSetLayoutBinding binding;
	VkDescriptorSetLayoutCreateInfo layoutInfo;

	if (postProcessPipeline != VK_NULL_HANDLE) {
		return true;
	}

	if (postProcessSampler == VK_NULL_HANDLE) {
		VK_InitialiseStructure(samplerInfo);
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.maxAnisotropy = 1.0f;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		if (vkCreateSampler(vk_options.logicalDevice, &samplerInfo, NULL, &postProcessSampler) != VK_SUCCESS) {
			return false;
		}
	}

	if (postProcessDescriptorSetLayout == VK_NULL_HANDLE) {
		VK_InitialiseStructure(binding);
		binding.binding = 0;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.descriptorCount = 1;
		binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VK_InitialiseStructure(layoutInfo);
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &binding;
		if (vkCreateDescriptorSetLayout(vk_options.logicalDevice, &layoutInfo, NULL, &postProcessDescriptorSetLayout) != VK_SUCCESS) {
			return false;
		}
	}

	vertShaderModule = VK_HudCreateShaderModule(vk_post_process_vert_spv, vk_post_process_vert_spv_len);
	fragShaderModule = VK_HudCreateShaderModule(vk_post_process_frag_spv, vk_post_process_frag_spv_len);
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

	VK_InitialiseStructure(vertexInputInfo);
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VK_InitialiseStructure(inputAssembly);
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

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

	VK_BlendingConfigure(&colorBlending, &blending, r_blendfunc_overwrite);

	VK_InitialiseStructure(dynamicState);
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
	dynamicState.pDynamicStates = dynamicStates;

	if (postProcessPipelineLayout == VK_NULL_HANDLE) {
		VK_InitialiseStructure(pushConstantRange);
		pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(vk_post_process_push_t);

		VK_InitialiseStructure(pipelineLayoutInfo);
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &postProcessDescriptorSetLayout;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		if (vkCreatePipelineLayout(vk_options.logicalDevice, &pipelineLayoutInfo, NULL, &postProcessPipelineLayout) != VK_SUCCESS) {
			vkDestroyShaderModule(vk_options.logicalDevice, fragShaderModule, NULL);
			vkDestroyShaderModule(vk_options.logicalDevice, vertShaderModule, NULL);
			return false;
		}
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
	pipelineInfo.layout = postProcessPipelineLayout;
	pipelineInfo.renderPass = VK_PostProcessRenderPass();
	pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(vk_options.logicalDevice, vk_options.pipelineCache, 1, &pipelineInfo, NULL, &postProcessPipeline) != VK_SUCCESS) {
		postProcessPipeline = VK_NULL_HANDLE;
	}

	vkDestroyShaderModule(vk_options.logicalDevice, fragShaderModule, NULL);
	vkDestroyShaderModule(vk_options.logicalDevice, vertShaderModule, NULL);
	return postProcessPipeline != VK_NULL_HANDLE;
}

// Allocates/updates the descriptor set for swapchain image imageIndex,
// pointing the sampler at that image's offscreen post-process color target.
// Done lazily here (not in VK_CreatePostProcessResources) because the
// descriptor *contents* depend on the sampler/layout created above, which in
// turn are only created the first time post-process is actually used.
static VkDescriptorSet VK_PostProcessDescriptorSet(uint32_t imageIndex)
{
	VkDescriptorSet set;
	VkDescriptorSetAllocateInfo allocInfo;
	VkDescriptorImageInfo imageInfo;
	VkWriteDescriptorSet write;

	if (!vk_options.swapChain.postProcessDescriptorSets || imageIndex >= vk_options.swapChain.imageCount) {
		return VK_NULL_HANDLE;
	}

	set = vk_options.swapChain.postProcessDescriptorSets[imageIndex];
	if (set != VK_NULL_HANDLE) {
		return set;
	}

	VK_InitialiseStructure(allocInfo);
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = vk_options.swapChain.postProcessDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &postProcessDescriptorSetLayout;
	if (vkAllocateDescriptorSets(vk_options.logicalDevice, &allocInfo, &set) != VK_SUCCESS) {
		return VK_NULL_HANDLE;
	}

	VK_InitialiseStructure(imageInfo);
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = vk_options.swapChain.postProcessColorImageViews[imageIndex];
	imageInfo.sampler = postProcessSampler;

	VK_InitialiseStructure(write);
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = set;
	write.dstBinding = 0;
	write.dstArrayElement = 0;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.descriptorCount = 1;
	write.pImageInfo = &imageInfo;
	vkUpdateDescriptorSets(vk_options.logicalDevice, 1, &write, 0, NULL);

	vk_options.swapChain.postProcessDescriptorSets[imageIndex] = set;
	return set;
}

// Composite pass: reads the offscreen target the main render pass just wrote
// for this swapchain image, applies real gamma/contrast and (optionally) FXAA,
// writes the swapchain image directly. Caller (VK_EndFrame) has already begun
// VK_PostProcessRenderPass() against VK_PostProcessFramebuffer(imageIndex)
// before calling this.
// VK_MainRenderPass() leaves the offscreen target in PRESENT_SRC_KHR (same
// finalLayout it uses for the swapchain image in the no-postprocess path, see
// VK_RenderPassCreate) -- transition it to something the sampler can actually
// read. Must run outside the composite render pass instance (before
// vkCmdBeginRenderPass), since this barrier targets an image that isn't an
// attachment of that render pass. No barrier back afterwards: next frame's
// main pass attachment has initialLayout=UNDEFINED and loadOp=CLEAR, so
// whatever layout this image is in when that pass starts doesn't matter.
void VK_PostProcessTransitionForSampling(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	VkImageMemoryBarrier barrier;

	if (!vk_options.swapChain.postProcessColorImages || imageIndex >= vk_options.swapChain.imageCount) {
		return;
	}

	VK_InitialiseStructure(barrier);
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = vk_options.swapChain.postProcessColorImages[imageIndex];
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
}

void VK_PostProcessComposite(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	extern cvar_t v_gamma, v_contrast;
	extern cvar_t vid_framebuffer_fxaa;
	extern cvar_t vid_software_palette;
	VkDescriptorSet descriptorSet;
	vk_post_process_push_t push;
	qbool paletteActive = vid_software_palette.integer != 0;

	if (!VK_PostProcessCreatePipeline()) {
		return;
	}

	descriptorSet = VK_PostProcessDescriptorSet(imageIndex);
	if (descriptorSet == VK_NULL_HANDLE) {
		return;
	}

	push.blend[0] = push.blend[1] = push.blend[2] = 0.0f;
	push.blend[3] = 1.0f;
	// Same POST_PROCESS_PALETTE gate as GLM_CompilePostProcessProgram() --
	// see VK_PostProcessActive(). Without it, gl_gamma/gl_contrast values meant
	// for the (unimplemented here) hardware gamma ramp get applied as a shader
	// curve instead, which can wash the image to solid white.
	push.gamma = paletteActive ? bound(0.3f, v_gamma.value, 3.0f) : 1.0f;
	push.contrast = paletteActive ? bound(1.0f, v_contrast.value, 3.0f) : 1.0f;
	push.invWidth = 1.0f / (float)max(1, vk_options.swapChain.imageSize.width);
	push.invHeight = 1.0f / (float)max(1, vk_options.swapChain.imageSize.height);
	push.fxaaEnabled = vid_framebuffer_fxaa.integer != 0 ? 1 : 0;
	// GLC/GLM select one of 17 distinct FXAA_QUALITY__PRESET shader variants
	// (GL_FramebufferFxaaPreset); this single-pass approximation has no
	// variants to switch between, so the preset index instead scales the
	// algorithm's two continuous knobs -- see vk_post_process.frag's
	// ApplyFXAA for what 0..1 actually changes. bound(0,17) here mirrors
	// GL_FramebufferFxaaPreset's own clamp of the same cvar.
	push.fxaaQuality = (float)bound(0, vid_framebuffer_fxaa.integer, 17) / 17.0f;

	VK_HudSetViewportScissor(commandBuffer);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, postProcessPipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, postProcessPipelineLayout, 0, 1, &descriptorSet, 0, NULL);
	vkCmdPushConstants(commandBuffer, postProcessPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
	vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}

// gl_outline & 2: matches GLM's R_DrawWorldOutlines() gate (r_brushmodel_
// surfaces.c) -- world geometry outlines, independent of gl_outline & 1
// (model outlines, handled entirely in vk_aliasmodel.c).
qbool VK_WorldOutlineActive(void)
{
	extern qbool R_DrawWorldOutlines(void);
	return R_DrawWorldOutlines();
}

static qbool VK_WorldOutlineCreatePipeline(void)
{
	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;
	VkPipelineShaderStageCreateInfo shaderStages[2];
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
	VkSamplerCreateInfo samplerInfo;
	VkDescriptorSetLayoutBinding binding;
	VkDescriptorSetLayoutCreateInfo layoutInfo;

	if (worldOutlinePipeline != VK_NULL_HANDLE) {
		return true;
	}

	if (worldNormalsSampler == VK_NULL_HANDLE) {
		VK_InitialiseStructure(samplerInfo);
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_NEAREST;
		samplerInfo.minFilter = VK_FILTER_NEAREST;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.maxAnisotropy = 1.0f;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		if (vkCreateSampler(vk_options.logicalDevice, &samplerInfo, NULL, &worldNormalsSampler) != VK_SUCCESS) {
			return false;
		}
	}

	if (worldOutlineDescriptorSetLayout == VK_NULL_HANDLE) {
		VK_InitialiseStructure(binding);
		binding.binding = 0;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.descriptorCount = 1;
		binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VK_InitialiseStructure(layoutInfo);
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &binding;
		if (vkCreateDescriptorSetLayout(vk_options.logicalDevice, &layoutInfo, NULL, &worldOutlineDescriptorSetLayout) != VK_SUCCESS) {
			return false;
		}
	}

	vertShaderModule = VK_HudCreateShaderModule(vk_post_process_vert_spv, vk_post_process_vert_spv_len);
	fragShaderModule = VK_HudCreateShaderModule(vk_world_outline_frag_spv, vk_world_outline_frag_spv_len);
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

	VK_InitialiseStructure(vertexInputInfo);
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VK_InitialiseStructure(inputAssembly);
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

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

	// Drawn inline in the main render pass (whatever MSAA state it has) --
	// unlike post-process, this pipeline is not its own separate render pass,
	// it composites over the already-drawn scene alongside HUD/alias-model
	// draws in the same pass instance.
	VK_InitialiseStructure(multisampling);
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = vk_options.msaaSamples ? vk_options.msaaSamples : VK_SAMPLE_COUNT_1_BIT;

	VK_InitialiseStructure(depthStencil);
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_FALSE;
	depthStencil.depthWriteEnable = VK_FALSE;

	// The shader only ever writes colour when alpha is exactly 1 (edge) or
	// leaves it 0 (no edge) -- at those two endpoints premultiplied-alpha
	// blending (ONE, ONE_MINUS_SRC_ALPHA) is identical to a straight alpha-
	// over, so the pipeline reuses that existing blend mode rather than
	// adding a new one just for this binary case.
	VK_BlendingConfigure(&colorBlending, &blending, r_blendfunc_premultiplied_alpha);

	VK_InitialiseStructure(dynamicState);
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
	dynamicState.pDynamicStates = dynamicStates;

	if (worldOutlinePipelineLayout == VK_NULL_HANDLE) {
		VK_InitialiseStructure(pushConstantRange);
		pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(vk_world_outline_push_t);

		VK_InitialiseStructure(pipelineLayoutInfo);
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &worldOutlineDescriptorSetLayout;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		if (vkCreatePipelineLayout(vk_options.logicalDevice, &pipelineLayoutInfo, NULL, &worldOutlinePipelineLayout) != VK_SUCCESS) {
			vkDestroyShaderModule(vk_options.logicalDevice, fragShaderModule, NULL);
			vkDestroyShaderModule(vk_options.logicalDevice, vertShaderModule, NULL);
			return false;
		}
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
	pipelineInfo.layout = worldOutlinePipelineLayout;
	pipelineInfo.renderPass = VK_MainRenderPass();
	pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(vk_options.logicalDevice, vk_options.pipelineCache, 1, &pipelineInfo, NULL, &worldOutlinePipeline) != VK_SUCCESS) {
		worldOutlinePipeline = VK_NULL_HANDLE;
	}

	vkDestroyShaderModule(vk_options.logicalDevice, fragShaderModule, NULL);
	vkDestroyShaderModule(vk_options.logicalDevice, vertShaderModule, NULL);
	return worldOutlinePipeline != VK_NULL_HANDLE;
}

// Same lazy-allocate-and-cache pattern as VK_PostProcessDescriptorSet.
static VkDescriptorSet VK_WorldOutlineDescriptorSet(uint32_t imageIndex)
{
	VkDescriptorSet set;
	VkDescriptorSetAllocateInfo allocInfo;
	VkDescriptorImageInfo imageInfo;
	VkWriteDescriptorSet write;

	if (!vk_options.swapChain.worldNormalsDescriptorSets || imageIndex >= vk_options.swapChain.imageCount) {
		return VK_NULL_HANDLE;
	}

	set = vk_options.swapChain.worldNormalsDescriptorSets[imageIndex];
	if (set != VK_NULL_HANDLE) {
		return set;
	}

	VK_InitialiseStructure(allocInfo);
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = vk_options.swapChain.worldNormalsDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &worldOutlineDescriptorSetLayout;
	if (vkAllocateDescriptorSets(vk_options.logicalDevice, &allocInfo, &set) != VK_SUCCESS) {
		return VK_NULL_HANDLE;
	}

	VK_InitialiseStructure(imageInfo);
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = vk_options.swapChain.worldNormalsColorImageViews[imageIndex];
	imageInfo.sampler = worldNormalsSampler;

	VK_InitialiseStructure(write);
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = set;
	write.dstBinding = 0;
	write.dstArrayElement = 0;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.descriptorCount = 1;
	write.pImageInfo = &imageInfo;
	vkUpdateDescriptorSets(vk_options.logicalDevice, 1, &write, 0, NULL);

	vk_options.swapChain.worldNormalsDescriptorSets[imageIndex] = set;
	return set;
}

// VK_WorldNormalsRenderPassCreate leaves the colour attachment in
// SHADER_READ_ONLY_OPTIMAL as its finalLayout already, so unlike
// VK_PostProcessTransitionForSampling this needs no barrier -- kept as a
// named entry point (called right after the normals pass ends, before the
// main render pass begins) in case that render pass's finalLayout choice
// ever changes.
void VK_WorldNormalsTransitionForSampling(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	(void)commandBuffer;
	(void)imageIndex;
}

// Composites the outline over the scene the main render pass has already
// drawn (world + entities so far) -- caller (VK_DrawWorld, right after the
// opaque world batch, matching GLM_RenderView's GLM_DrawWorldOutlines() call
// site) is inside the main render pass instance already; this just adds one
// more draw to it.
void VK_WorldOutlineComposite(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	extern cvar_t gl_outline_color_world, gl_outline_world_depth_threshold, gl_outline_world_normal_threshold;
	extern cvar_t r_farclip;
	VkDescriptorSet descriptorSet;
	vk_world_outline_push_t push;
	float fbScaleX, fbScaleY;

	if (!VK_WorldOutlineCreatePipeline()) {
		return;
	}

	descriptorSet = VK_WorldOutlineDescriptorSet(imageIndex);
	if (descriptorSet == VK_NULL_HANDLE) {
		return;
	}

	push.outlineColor[0] = (float)gl_outline_color_world.color[0] / 255.0f;
	push.outlineColor[1] = (float)gl_outline_color_world.color[1] / 255.0f;
	push.outlineColor[2] = (float)gl_outline_color_world.color[2] / 255.0f;
	push.outlineDepthThreshold = bound(1.0f, gl_outline_world_depth_threshold.value, 16.0f);
	push.outlineNormalThreshold = bound(0.0f, gl_outline_world_normal_threshold.value, 0.999f);
	push.invWidth = 1.0f / (float)max(1, vk_options.swapChain.imageSize.width);
	push.invHeight = 1.0f / (float)max(1, vk_options.swapChain.imageSize.height);
	push.zFar = bound(R_MINIMUM_FARCLIP, r_farclip.value, R_MAXIMUM_FARCLIP);

	// Same scaling logic as GLM_DrawWorldOutlines: the normals target is
	// rendered at the real 3D viewport resolution, which can differ from the
	// window resolution (viewsize / scr_scale) -- scale the sample offset so
	// the outline stays roughly 1 screen pixel wide either way.
	fbScaleX = (float)VID_ScaledWidth3D() / (float)max(1, glConfig.vidWidth);
	fbScaleY = (float)VID_ScaledHeight3D() / (float)max(1, glConfig.vidHeight);
	push.outlineScale = bound(1.0f, max(fbScaleX, fbScaleY), 4.0f);

	VK_HudSetViewportScissor(commandBuffer);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, worldOutlinePipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, worldOutlinePipelineLayout, 0, 1, &descriptorSet, 0, NULL);
	vkCmdPushConstants(commandBuffer, worldOutlinePipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
	vkCmdDraw(commandBuffer, 3, 1, 0, 0);
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
	if (!R_BufferReferenceIsValid(r_buffer_hud_brighten_vertex_data)) {
		// NDC-space full-screen quad, triangle-strip order (TL, BL, TR, BR)
		// matching VK_SetCoordinates' convention elsewhere in this file.
		static const float fullscreenQuad[] = {
			-1.0f, -1.0f,
			-1.0f,  1.0f,
			 1.0f, -1.0f,
			 1.0f,  1.0f,
		};
		if (!buffers.Create(r_buffer_hud_brighten_vertex_data, buffertype_vertex, "vk-hud-brighten-vbo", sizeof(fullscreenQuad), (void*)fullscreenQuad, bufferusage_constant_data)) {
			return false;
		}
	}

	if (!VK_HudCreateImagePipeline()) {
		return false;
	}
	if (!VK_HudCreateColorPipeline(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, r_blendfunc_premultiplied_alpha, &hudCircleFillPipeline)) {
		return false;
	}
	if (!VK_HudCreateColorPipeline(VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, r_blendfunc_premultiplied_alpha, &hudCircleLinePipeline)) {
		return false;
	}
	// Must match GLC/GLM's r_state_brighten_screen exactly: dst*(1+src), not
	// flat additive (src+dst) -- additive saturates to solid white on the
	// very first f>=2 pass regardless of the underlying scene, since it adds
	// pure white instead of scaling the existing pixel up.
	if (!VK_HudCreateColorPipeline(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, r_blendfunc_src_dst_color_dest_one, &hudBrightenPipeline)) {
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

// Vulkan has no post-process/palette pass (renderer.RenderFramebuffers and
// renderer.PostProcessScreen are both no-ops here), so unlike GLC/GLM this is
// not just a fallback for the vid_software_palette-disabled case -- it's the
// only contrast boost Vulkan has. Same technique as GLC_BrightenScreen: redraw
// a full-screen quad with additive blending, halving brightness each pass,
// until the requested contrast is exhausted. This only covers v_contrast > 1;
// a real v_gamma curve (including darkening, gamma < 1) would need an actual
// post-process pass reading back the rendered frame, which Vulkan doesn't
// have yet -- see AGENTS.md for the tracked limitation.
void VK_BrightenScreen(void)
{
	VkCommandBuffer commandBuffer;
	VkBuffer vertexBuffer;
	VkDeviceSize offsets[] = { 0 };
	extern cvar_t v_contrast;
	float f;

	if (v_contrast.value <= 1.0) {
		return;
	}

	commandBuffer = VK_CurrentCommandBuffer();
	if (commandBuffer == VK_NULL_HANDLE || !VK_HudEnsureResources()) {
		return;
	}

	f = min(v_contrast.value, 3);
	if (R_OldGammaBehaviour()) {
		extern float vid_gamma;

		f = pow(f, vid_gamma);
	}

	vertexBuffer = VK_BufferHandle(r_buffer_hud_brighten_vertex_data);
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
	VK_HudSetViewportScissor(commandBuffer);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hudBrightenPipeline);

	while (f > 1) {
		vk_hud_color_push_t push;

		if (f >= 2) {
			push.color[0] = push.color[1] = push.color[2] = 1.0f;
		}
		else {
			push.color[0] = push.color[1] = push.color[2] = f - 1;
		}
		push.color[3] = 1.0f;

		vkCmdPushConstants(commandBuffer, hudColorPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
		vkCmdDraw(commandBuffer, 4, 1, 0, 0);

		f *= 0.5f;
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

// Damage/pickup/quad/pent screen tint and underwater colour, same technique
// as GLC_PolyBlend/GLM_PolyBlend: a single translucent rectangle over the
// 3D viewport using the existing premultiplied-alpha HUD rectangle pipeline.
void VK_PolyBlend(float v_blend[4])
{
	byte color[4];

	color[0] = (byte)(bound(0, v_blend[0], 1) * 255);
	color[1] = (byte)(bound(0, v_blend[1], 1) * 255);
	color[2] = (byte)(bound(0, v_blend[2], 1) * 255);
	color[3] = (byte)(bound(0, v_blend[3], 1) * 255);

	VK_DrawRectangle((float)r_refdef.vrect.x, (float)r_refdef.vrect.y, (float)r_refdef.vrect.width, (float)r_refdef.vrect.height, color);
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
	if (hudBrightenPipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(vk_options.logicalDevice, hudBrightenPipeline, NULL);
		hudBrightenPipeline = VK_NULL_HANDLE;
	}
	if (hudImagePipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(vk_options.logicalDevice, hudImagePipelineLayout, NULL);
		hudImagePipelineLayout = VK_NULL_HANDLE;
	}
	if (hudColorPipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(vk_options.logicalDevice, hudColorPipelineLayout, NULL);
		hudColorPipelineLayout = VK_NULL_HANDLE;
	}
	if (postProcessPipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(vk_options.logicalDevice, postProcessPipeline, NULL);
		postProcessPipeline = VK_NULL_HANDLE;
	}
	if (postProcessPipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(vk_options.logicalDevice, postProcessPipelineLayout, NULL);
		postProcessPipelineLayout = VK_NULL_HANDLE;
	}
	if (postProcessDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(vk_options.logicalDevice, postProcessDescriptorSetLayout, NULL);
		postProcessDescriptorSetLayout = VK_NULL_HANDLE;
	}
	if (postProcessSampler != VK_NULL_HANDLE) {
		vkDestroySampler(vk_options.logicalDevice, postProcessSampler, NULL);
		postProcessSampler = VK_NULL_HANDLE;
	}
	hudImageBufferDirty = false;
}

void VK_HudSwapchainChanged(void)
{
	VK_HudResourcesShutdown();
}

#endif // #ifdef RENDERER_OPTION_VULKAN
