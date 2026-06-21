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

#include <stdarg.h>

#include "gl_model.h"
#include "r_aliasmodel.h"
#include "r_aliasmodel_md3.h"
#include "r_buffers.h"
#include "r_matrix.h"
#include "r_texture.h"
#include "rulesets.h"
#include "tr_types.h"
#include "vk_local.h"

extern const unsigned char vk_alias_model_vert_spv[];
extern const unsigned int vk_alias_model_vert_spv_len;
extern const unsigned char vk_alias_model_frag_spv[];
extern const unsigned int vk_alias_model_frag_spv_len;

#define VK_ALIAS_MODE_NORMAL     0.0f
#define VK_ALIAS_MODE_FULLBRIGHT 1.0f
#define VK_ALIAS_MODE_SHELL      2.0f
#define VK_ALIAS_MODE_SHADOW     3.0f
#define VK_ALIAS_MODE_OUTLINE    4.0f

#define VK_ALIAS_BLEND_AUTO     -1
#define VK_ALIAS_BLEND_OPAQUE    0
#define VK_ALIAS_BLEND_ALPHA     1
#define VK_ALIAS_BLEND_ADDITIVE  2
#define VK_ALIAS_BLEND_SHADOW    3

extern cvar_t gl_powerupshells;
extern cvar_t gl_outline_color_model;
extern cvar_t gl_outline_color_team;
extern cvar_t gl_outline_color_enemy;
extern cvar_t gl_outline_use_player_color;

typedef struct vk_alias_draw_s {
	uint32_t firstVertex;
	uint32_t vertexCount;
	float mvp[16];
	float color[4];
	float altColor[4];
	float lerp;
	float textured;
	float weapon;
	float mode;
	float minLumaMix;
	float scrollS;
	float scrollT;
	float pad0;
	texture_ref texture;
	int blendMode;
	qbool postscene;
	qbool player;
} vk_alias_draw_t;

typedef struct vk_alias_push_s {
	float mvp[16];
	float color[4];
	float altColor[4];
	float lerp;
	float textured;
	float weapon;
	float mode;
	float minLumaMix;
	float scrollS;
	float scrollT;
	float pad0;
} vk_alias_push_t;

static VkPipelineLayout aliasPipelineLayout;
static VkPipeline aliasOpaquePipeline;
static VkPipeline aliasBlendedPipeline;
static VkPipeline aliasAdditivePipeline;
static VkPipeline aliasShadowPipeline;
static vk_alias_draw_t* aliasDraws;
static int aliasDrawCount;
static int aliasDrawCapacity;

static void VK_AliasDebugLog(const char* fmt, ...)
{
	(void)fmt;
}

static VkShaderModule VK_AliasCreateShaderModule(const unsigned char* bytes, unsigned int length)
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

static void VK_AliasSetViewportScissor(VkCommandBuffer commandBuffer)
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

static qbool VK_AliasEnsureDrawCapacity(void)
{
	if (aliasDrawCount >= aliasDrawCapacity) {
		int newCapacity = aliasDrawCapacity ? aliasDrawCapacity * 2 : 128;
		vk_alias_draw_t* newDraws = Q_malloc(newCapacity * sizeof(newDraws[0]));

		if (aliasDraws) {
			memcpy(newDraws, aliasDraws, aliasDrawCount * sizeof(aliasDraws[0]));
			Q_free(aliasDraws);
		}
		aliasDraws = newDraws;
		aliasDrawCapacity = newCapacity;
	}

	return aliasDraws != NULL;
}

static qbool VK_AliasCreatePipeline(int blendMode)
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
	VkPipelineColorBlendAttachmentState colorBlendAttachment;
	VkPipelineColorBlendStateCreateInfo colorBlending;
	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState;
	VkPushConstantRange pushConstantRange;
	VkPipelineLayoutCreateInfo pipelineLayoutInfo;
	VkGraphicsPipelineCreateInfo pipelineInfo;
	VkDescriptorSetLayout descriptorSetLayout;
	VkPipeline* pipeline =
		blendMode == VK_ALIAS_BLEND_SHADOW ? &aliasShadowPipeline :
		blendMode == VK_ALIAS_BLEND_ADDITIVE ? &aliasAdditivePipeline :
		blendMode == VK_ALIAS_BLEND_ALPHA ? &aliasBlendedPipeline :
		&aliasOpaquePipeline;

	if (*pipeline != VK_NULL_HANDLE) {
		return true;
	}

	descriptorSetLayout = VK_TextureDescriptorSetLayout();
	if (descriptorSetLayout == VK_NULL_HANDLE) {
		return false;
	}

	vertShaderModule = VK_AliasCreateShaderModule(vk_alias_model_vert_spv, vk_alias_model_vert_spv_len);
	fragShaderModule = VK_AliasCreateShaderModule(vk_alias_model_frag_spv, vk_alias_model_frag_spv_len);
	if (vertShaderModule == VK_NULL_HANDLE || fragShaderModule == VK_NULL_HANDLE) {
		if (fragShaderModule != VK_NULL_HANDLE) {
			vkDestroyShaderModule(vk_options.logicalDevice, fragShaderModule, NULL);
		}
		if (vertShaderModule != VK_NULL_HANDLE) {
			vkDestroyShaderModule(vk_options.logicalDevice, vertShaderModule, NULL);
		}
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
	bindingDescription.stride = sizeof(vbo_model_vert_t);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VK_InitialiseStructure(attributeDescriptions[0]);
	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[0].offset = VK_VBO_FIELDOFFSET(vbo_model_vert_t, position);

	VK_InitialiseStructure(attributeDescriptions[1]);
	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[1].offset = VK_VBO_FIELDOFFSET(vbo_model_vert_t, texture_coords);

	VK_InitialiseStructure(attributeDescriptions[2]);
	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[2].offset = VK_VBO_FIELDOFFSET(vbo_model_vert_t, normal);

	VK_InitialiseStructure(attributeDescriptions[3]);
	attributeDescriptions[3].binding = 0;
	attributeDescriptions[3].location = 3;
	attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[3].offset = VK_VBO_FIELDOFFSET(vbo_model_vert_t, direction);

	VK_InitialiseStructure(vertexInputInfo);
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = sizeof(attributeDescriptions) / sizeof(attributeDescriptions[0]);
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

	VK_InitialiseStructure(inputAssembly);
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

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
	rasterizer.cullMode = blendMode == VK_ALIAS_BLEND_SHADOW ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

	VK_InitialiseStructure(multisampling);
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VK_InitialiseStructure(depthStencil);
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = (blendMode == VK_ALIAS_BLEND_OPAQUE || blendMode == VK_ALIAS_BLEND_SHADOW) ? VK_TRUE : VK_FALSE;
	depthStencil.depthCompareOp = glConfig.reversed_depth ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	VK_InitialiseStructure(colorBlendAttachment);
	colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = blendMode == VK_ALIAS_BLEND_OPAQUE ? VK_FALSE : VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = (blendMode == VK_ALIAS_BLEND_ADDITIVE || blendMode == VK_ALIAS_BLEND_SHADOW) ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = blendMode == VK_ALIAS_BLEND_ADDITIVE ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = blendMode == VK_ALIAS_BLEND_ADDITIVE ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VK_InitialiseStructure(colorBlending);
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	VK_InitialiseStructure(dynamicState);
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
	dynamicState.pDynamicStates = dynamicStates;

	if (aliasPipelineLayout == VK_NULL_HANDLE) {
		VK_InitialiseStructure(pushConstantRange);
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(vk_alias_push_t);

		VK_InitialiseStructure(pipelineLayoutInfo);
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		if (vkCreatePipelineLayout(vk_options.logicalDevice, &pipelineLayoutInfo, NULL, &aliasPipelineLayout) != VK_SUCCESS) {
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
	pipelineInfo.layout = aliasPipelineLayout;
	pipelineInfo.renderPass = VK_MainRenderPass();
	pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(vk_options.logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, pipeline) != VK_SUCCESS) {
		*pipeline = VK_NULL_HANDLE;
	}

	vkDestroyShaderModule(vk_options.logicalDevice, fragShaderModule, NULL);
	vkDestroyShaderModule(vk_options.logicalDevice, vertShaderModule, NULL);
	return *pipeline != VK_NULL_HANDLE;
}

void VK_AliasModelResourcesShutdown(void)
{
	if (vk_options.logicalDevice != VK_NULL_HANDLE) {
		if (aliasOpaquePipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(vk_options.logicalDevice, aliasOpaquePipeline, NULL);
			aliasOpaquePipeline = VK_NULL_HANDLE;
		}
		if (aliasBlendedPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(vk_options.logicalDevice, aliasBlendedPipeline, NULL);
			aliasBlendedPipeline = VK_NULL_HANDLE;
		}
		if (aliasAdditivePipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(vk_options.logicalDevice, aliasAdditivePipeline, NULL);
			aliasAdditivePipeline = VK_NULL_HANDLE;
		}
		if (aliasShadowPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(vk_options.logicalDevice, aliasShadowPipeline, NULL);
			aliasShadowPipeline = VK_NULL_HANDLE;
		}
		if (aliasPipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(vk_options.logicalDevice, aliasPipelineLayout, NULL);
			aliasPipelineLayout = VK_NULL_HANDLE;
		}
	}

	Q_free(aliasDraws);
	aliasDraws = NULL;
	aliasDrawCount = 0;
	aliasDrawCapacity = 0;
}

void VK_AliasModelFrameReset(void)
{
	aliasDrawCount = 0;
}

static float VK_AliasMinLumaMix(entity_t* ent)
{
	return 1.0f - (ent && ent->full_light ? bound(0, gl_fb_models.integer, 1) : 0);
}

static void VK_AliasPowerupShellColor(int layer, int effects, float* color)
{
	float baseLevel = bound(0, (layer == 0 ? gl_powerupshells_base1level.value : gl_powerupshells_base2level.value), 1);
	float effectLevel = bound(0, (layer == 0 ? gl_powerupshells_effect1level.value : gl_powerupshells_effect2level.value), 1);
	float shellAlpha = bound(0, gl_powerupshells.value, 1);

	color[0] = (baseLevel + ((effects & EF_RED) ? effectLevel : 0)) * shellAlpha;
	color[1] = (baseLevel + ((effects & EF_GREEN) ? effectLevel : 0)) * shellAlpha;
	color[2] = (baseLevel + ((effects & EF_BLUE) ? effectLevel : 0)) * shellAlpha;
	color[3] = shellAlpha;
}

static qbool VK_AliasShouldDrawShell(entity_t* ent, model_t* model, int effects, int render_effects)
{
	if (!ent || !model || (render_effects & RF_ADDITIVEBLEND)) {
		return false;
	}

	return (effects & (EF_RED | EF_GREEN | EF_BLUE)) &&
		R_TextureReferenceIsValid(shelltexture) &&
		VK_TextureReady(shelltexture) &&
		Ruleset_AllowPowerupShell(model);
}

static void VK_AliasQueuePreparedDraw(
	entity_t* ent,
	model_t* model,
	int firstVertex,
	int vertexCount,
	texture_ref texture,
	int render_effects,
	float lerpfrac,
	const float* color,
	const float* altColor,
	float mode,
	float minLumaMix,
	int requestedBlendMode,
	float scrollS,
	float scrollT
);

static void VK_AliasOutlineColor(entity_t* ent, float* color)
{
	cvar_t* outlineColor = &gl_outline_color_model;

	if (ent && ent->scoreboard) {
		if (gl_outline_use_player_color.integer) {
			int tc = 16 * bound(0, ent->scoreboard->topcolor, 13) + 8;

			color[0] = (float)host_basepal[tc * 3 + 0] / 255.0f;
			color[1] = (float)host_basepal[tc * 3 + 1] / 255.0f;
			color[2] = (float)host_basepal[tc * 3 + 2] / 255.0f;
			color[3] = 1.0f;
			return;
		}
		else if (ent->scoreboard->teammate && gl_outline_color_team.string[0]) {
			outlineColor = &gl_outline_color_team;
		}
		else if (!ent->scoreboard->teammate && gl_outline_color_enemy.string[0]) {
			outlineColor = &gl_outline_color_enemy;
		}
	}

	color[0] = (float)outlineColor->color[0] / 255.0f;
	color[1] = (float)outlineColor->color[1] / 255.0f;
	color[2] = (float)outlineColor->color[2] / 255.0f;
	color[3] = 1.0f;
}

static void VK_AliasQueueOutlineDraw(entity_t* ent, model_t* model, int firstVertex, int vertexCount, int render_effects, float lerpfrac)
{
	float outlineColor[4];
	float outlineParams[4] = { 0, 0, 0, 0 };

	if (!ent || !model || vertexCount <= 0 || !VK_TextureReady(solidwhite_texture)) {
		return;
	}

	VK_AliasOutlineColor(ent, outlineColor);
	outlineParams[0] = ent->outlineScale * RuleSets_ModelOutlineScale();
	if (outlineParams[0] <= 0) {
		return;
	}

	VK_AliasQueuePreparedDraw(
		ent,
		model,
		firstVertex,
		vertexCount,
		solidwhite_texture,
		render_effects | RF_ALPHABLEND,
		lerpfrac,
		outlineColor,
		outlineParams,
		VK_ALIAS_MODE_OUTLINE,
		1.0f,
		VK_ALIAS_BLEND_ALPHA,
		0.0f,
		0.0f
	);
}

static void VK_AliasQueuePreparedDraw(
	entity_t* ent,
	model_t* model,
	int firstVertex,
	int vertexCount,
	texture_ref texture,
	int render_effects,
	float lerpfrac,
	const float* color,
	const float* altColor,
	float mode,
	float minLumaMix,
	int requestedBlendMode,
	float scrollS,
	float scrollT
)
{
	vk_alias_draw_t* draw;
	float modelView[16];
	float mvp[16];
	qbool textureReady;
	static const float emptyAltColor[4] = { 0, 0, 0, 0 };

	if (!ent || !model || vertexCount <= 0 || !VK_AliasEnsureDrawCapacity()) {
		return;
	}

	textureReady = VK_TextureReady(texture);
	if (mode != VK_ALIAS_MODE_NORMAL && !textureReady) {
		return;
	}

	R_GetModelviewMatrix(modelView);
	// R_MultiplyMatrix(lhs, rhs) writes rhs * lhs for the engine's matrix layout.
	R_MultiplyMatrix(modelView, R_ProjectionMatrix(), mvp);

	draw = &aliasDraws[aliasDrawCount++];
	draw->firstVertex = (uint32_t)firstVertex;
	draw->vertexCount = (uint32_t)vertexCount;
	memcpy(draw->mvp, mvp, sizeof(draw->mvp));
	memcpy(draw->color, color, sizeof(draw->color));
	memcpy(draw->altColor, altColor ? altColor : emptyAltColor, sizeof(draw->altColor));
	draw->lerp = bound(0.0f, lerpfrac, 1.0f);
	draw->textured = textureReady ? 1.0f : 0.0f;
	draw->weapon = (render_effects & RF_WEAPONMODEL) ? 1.0f : 0.0f;
	draw->mode = mode;
	draw->minLumaMix = minLumaMix;
	draw->scrollS = scrollS;
	draw->scrollT = scrollT;
	draw->pad0 = 0.0f;
	draw->texture = draw->textured ? texture : solidwhite_texture;
	if (requestedBlendMode != VK_ALIAS_BLEND_AUTO) {
		draw->blendMode = requestedBlendMode;
	}
	else if (render_effects & RF_ADDITIVEBLEND) {
		draw->blendMode = VK_ALIAS_BLEND_ADDITIVE;
	}
	else if ((color[3] < 1.0f) || (render_effects & RF_ALPHABLEND)) {
		draw->blendMode = VK_ALIAS_BLEND_ALPHA;
	}
	else {
		draw->blendMode = VK_ALIAS_BLEND_OPAQUE;
	}
	draw->postscene = (render_effects & RF_WEAPONMODEL) || (render_effects & RF_ADDITIVEBLEND);
	draw->player = (render_effects & RF_PLAYERMODEL) ? true : false;
}

static void VK_AliasQueueFullbrightDraw(entity_t* ent, model_t* model, int firstVertex, int vertexCount, texture_ref fbTexture, int render_effects, float lerpfrac)
{
	static const float white[4] = { 1, 1, 1, 1 };

	if (!R_TextureReferenceIsValid(fbTexture) || !VK_TextureReady(fbTexture)) {
		return;
	}

	VK_AliasQueuePreparedDraw(
		ent,
		model,
		firstVertex,
		vertexCount,
		fbTexture,
		render_effects | RF_ALPHABLEND,
		lerpfrac,
		white,
		NULL,
		VK_ALIAS_MODE_FULLBRIGHT,
		1.0f,
		VK_ALIAS_BLEND_ALPHA,
		0.0f,
		0.0f
	);
}

void VK_AliasQueueDraw(entity_t* ent, model_t* model, int firstVertex, int vertexCount, texture_ref texture, qbool outline, int effects, int render_effects, float lerpfrac)
{
	float color[4];
	float shellColor1[4];
	float shellColor2[4];
	float light;
	qbool invalidate_texture;

	if (!ent || !model || vertexCount <= 0) {
		return;
	}

	if (outline) {
		VK_AliasQueueOutlineDraw(ent, model, firstVertex, vertexCount, render_effects, lerpfrac);
		return;
	}

	R_AliasModelColor(ent, color, &invalidate_texture);
	if (invalidate_texture) {
		R_TextureReferenceInvalidate(texture);
	}

	light = ent->full_light ? 1.0f : bound(0.125f, (ent->ambientlight + ent->shadelight) / 256.0f, 1.0f);
	color[0] *= light;
	color[1] *= light;
	color[2] *= light;

	VK_AliasQueuePreparedDraw(
		ent,
		model,
		firstVertex,
		vertexCount,
		texture,
		render_effects,
		lerpfrac,
		color,
		NULL,
		VK_ALIAS_MODE_NORMAL,
		VK_AliasMinLumaMix(ent),
		VK_ALIAS_BLEND_AUTO,
		0.0f,
		0.0f
	);

	if (!invalidate_texture && VK_AliasShouldDrawShell(ent, model, effects, render_effects)) {
		VK_AliasPowerupShellColor(0, effects, shellColor1);
		VK_AliasPowerupShellColor(1, effects, shellColor2);
		VK_AliasQueuePreparedDraw(
			ent,
			model,
			firstVertex,
			vertexCount,
			shelltexture,
			render_effects | RF_ALPHABLEND,
			lerpfrac,
			shellColor1,
			shellColor2,
			VK_ALIAS_MODE_SHELL,
			1.0f,
			VK_ALIAS_BLEND_ADDITIVE,
			r_refdef2.powerup_scroll_params[0],
			r_refdef2.powerup_scroll_params[1]
		);
	}
}

void VK_DrawAliasFrame(entity_t* ent, model_t* model, int pose1, int pose2, texture_ref texture, texture_ref fb_texture, qbool outline, int effects, int render_effects, float lerpfrac)
{
	aliashdr_t* paliashdr;
	int firstVertex;

	(void)pose2;
	if (!model) {
		return;
	}

	paliashdr = (aliashdr_t*)Mod_Extradata(model);
	if (!paliashdr) {
		return;
	}

	firstVertex = model->vbo_start + pose1 * paliashdr->vertsPerPose;
	VK_AliasQueueDraw(ent, model, firstVertex, paliashdr->vertsPerPose, texture, outline, effects, render_effects, lerpfrac);
	if (!outline) {
		VK_AliasQueueFullbrightDraw(ent, model, firstVertex, paliashdr->vertsPerPose, fb_texture, render_effects, lerpfrac);
	}
}

void VK_DrawAliasModelShadow(entity_t* ent)
{
	aliashdr_t* paliashdr;
	const maliasframedesc_t* frame;
	const maliasframedesc_t* oldframe;
	float oldMatrix[16];
	float shadeVector[3];
	float shadowParams[4] = { 0, 0, 0, 0 };
	float shadowColor[4] = { 0, 0, 0, 0.5f };
	float theta;
	float shadeScale = 1.0f / sqrt(2.0f);
	float lerpfrac;
	float lheight;
	int pose1;
	int pose2;
	int firstVertex;

	if (!ent || !ent->model) {
		return;
	}

	paliashdr = (aliashdr_t*)Mod_Extradata(ent->model);
	if (!paliashdr || !VK_TextureReady(solidwhite_texture)) {
		return;
	}

	frame = &paliashdr->frames[ent->frame];
	oldframe = &paliashdr->frames[ent->oldframe];
	R_AliasModelDeterminePoses(oldframe, frame, &pose1, &pose2, &lerpfrac);
	(void)pose2;

	theta = -ent->angles[1] / 180.0f * M_PI;
	VectorSet(shadeVector, cos(theta) * shadeScale, sin(theta) * shadeScale, shadeScale);
	lheight = ent->origin[2] - ent->lightspot[2];

	shadowParams[0] = shadeVector[0];
	shadowParams[1] = shadeVector[1];
	shadowParams[2] = lheight;

	R_PushModelviewMatrix(oldMatrix);
	R_TranslateModelview(ent->origin[0], ent->origin[1], ent->origin[2]);
	R_RotateModelview(ent->angles[1], 0, 0, 1);

	firstVertex = ent->model->vbo_start + pose1 * paliashdr->vertsPerPose;
	VK_AliasQueuePreparedDraw(
		ent,
		ent->model,
		firstVertex,
		paliashdr->vertsPerPose,
		solidwhite_texture,
		0,
		lerpfrac,
		shadowColor,
		shadowParams,
		VK_ALIAS_MODE_SHADOW,
		1.0f,
		VK_ALIAS_BLEND_SHADOW,
		0.0f,
		0.0f
	);

	R_PopModelviewMatrix(oldMatrix);
}

void VK_RenderAliasModels(qbool postscene)
{
	VkCommandBuffer commandBuffer;
	VkBuffer vertexBuffer;
	VkDeviceSize vertexOffset = 0;
	int i;
	int submitted = 0;
	int queuedForPass = 0;
	int queuedPostscene = 0;
	int queuedWeapons = 0;
	int queuedPlayers = 0;
	int skippedTexture = 0;

	if (!aliasDrawCount || !R_BufferReferenceIsValid(r_buffer_aliasmodel_vertex_data)) {
		return;
	}

	commandBuffer = VK_CurrentCommandBuffer();
	if (commandBuffer == VK_NULL_HANDLE) {
		return;
	}

	vertexBuffer = VK_BufferHandle(r_buffer_aliasmodel_vertex_data);
	if (vertexBuffer == VK_NULL_HANDLE) {
		return;
	}

	if (!VK_AliasCreatePipeline(VK_ALIAS_BLEND_OPAQUE) ||
		!VK_AliasCreatePipeline(VK_ALIAS_BLEND_ALPHA) ||
		!VK_AliasCreatePipeline(VK_ALIAS_BLEND_ADDITIVE) ||
		!VK_AliasCreatePipeline(VK_ALIAS_BLEND_SHADOW)) {
		return;
	}

	VK_AliasSetViewportScissor(commandBuffer);
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &vertexOffset);

	for (i = 0; i < aliasDrawCount; ++i) {
		vk_alias_draw_t* draw = &aliasDraws[i];
		texture_ref texture = VK_TextureReady(draw->texture) ? draw->texture : solidwhite_texture;
		VkDescriptorSet descriptorSet;
		vk_alias_push_t push;

		if (draw->postscene != postscene) {
			continue;
		}
		++queuedForPass;
		queuedPostscene += draw->postscene ? 1 : 0;
		queuedWeapons += draw->weapon != 0.0f ? 1 : 0;
		queuedPlayers += draw->player ? 1 : 0;
		if (!VK_TextureReady(texture)) {
			++skippedTexture;
			continue;
		}

		descriptorSet = VK_TextureDescriptorSet(texture);
		if (descriptorSet == VK_NULL_HANDLE) {
			++skippedTexture;
			continue;
		}

		memcpy(push.mvp, draw->mvp, sizeof(push.mvp));
		memcpy(push.color, draw->color, sizeof(push.color));
		memcpy(push.altColor, draw->altColor, sizeof(push.altColor));
		push.lerp = draw->lerp;
		push.textured = draw->textured;
		push.weapon = draw->weapon;
		push.mode = draw->mode;
		push.minLumaMix = draw->minLumaMix;
		push.scrollS = draw->scrollS;
		push.scrollT = draw->scrollT;
		push.pad0 = 0.0f;

		vkCmdBindPipeline(
			commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			draw->blendMode == VK_ALIAS_BLEND_SHADOW ? aliasShadowPipeline :
			draw->blendMode == VK_ALIAS_BLEND_ADDITIVE ? aliasAdditivePipeline :
			draw->blendMode == VK_ALIAS_BLEND_ALPHA ? aliasBlendedPipeline :
			aliasOpaquePipeline
		);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aliasPipelineLayout, 0, 1, &descriptorSet, 0, NULL);
		vkCmdPushConstants(commandBuffer, aliasPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
		vkCmdDraw(commandBuffer, draw->vertexCount, 1, draw->firstVertex, 0);
		++submitted;
	}

	VK_AliasDebugLog(
		"render postscene=%d total=%d queued=%d submitted=%d skippedTexture=%d postQueued=%d weapons=%d players=%d",
		postscene ? 1 : 0,
		aliasDrawCount,
		queuedForPass,
		submitted,
		skippedTexture,
		queuedPostscene,
		queuedWeapons,
		queuedPlayers);
}

#endif // RENDERER_OPTION_VULKAN
