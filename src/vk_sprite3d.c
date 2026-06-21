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

#include "gl_model.h"
#include "particles_classic.h"
#include "r_buffers.h"
#include "r_matrix.h"
#include "r_sprite3d.h"
#include "r_sprite3d_internal.h"
#include "r_texture.h"
#include "tr_types.h"
#include "vk_local.h"

extern const unsigned char vk_sprite3d_vert_spv[];
extern const unsigned int vk_sprite3d_vert_spv_len;
extern const unsigned char vk_sprite3d_frag_spv[];
extern const unsigned int vk_sprite3d_frag_spv_len;

extern texture_ref particletexture;
extern r_sprite3d_vert_t glvertices[ABSOLUTE_MAX_PARTICLES * 3];

mspriteframe_t* R_GetSpriteFrame(entity_t* e, msprite2_t* psprite);

typedef struct vk_sprite3d_push_s {
	float modelView[16];
	float projection[16];
	float alphaThreshold;
	float padding[3];
} vk_sprite3d_push_t;

typedef enum vk_sprite_pipeline_id_s {
	VK_SPRITE_PIPELINE_PREMULT_DEPTH,
	VK_SPRITE_PIPELINE_PREMULT_DEPTH_WRITE,
	VK_SPRITE_PIPELINE_ENTITY_ALPHA_TEST,
	VK_SPRITE_PIPELINE_DARKEN_DEPTH,
	VK_SPRITE_PIPELINE_ADDITIVE_NODEPTH,

	VK_SPRITE_PIPELINE_COUNT
} vk_sprite_pipeline_id_t;

typedef struct vk_sprite_pipeline_state_s {
	r_blendfunc_t blendFunc;
	qbool depthTest;
	qbool depthWrite;
	qbool depthBias;
} vk_sprite_pipeline_state_t;

static VkPipelineLayout spritePipelineLayout;
static VkPipeline spritePipelines[r_primitive_count][VK_SPRITE_PIPELINE_COUNT];

static const vk_sprite_pipeline_state_t spritePipelineStates[VK_SPRITE_PIPELINE_COUNT] = {
	{ r_blendfunc_premultiplied_alpha, true,  false, false },
	{ r_blendfunc_premultiplied_alpha, true,  true,  false },
	{ r_blendfunc_premultiplied_alpha, true,  true,  true  },
	{ r_blendfunc_src_zero_dest_one_minus_src_color, true, false, false },
	{ r_blendfunc_additive_blending, false, false, false },
};

static vk_sprite_pipeline_id_t VK_SpritePipelineForBatch(const gl_sprite3d_batch_t* batch)
{
	if (!batch) {
		return VK_SPRITE_PIPELINE_PREMULT_DEPTH;
	}

	switch (batch->rendering_state) {
		case r_state_sprites_textured:
			return VK_SPRITE_PIPELINE_ENTITY_ALPHA_TEST;
		case r_state_chaticon:
			return VK_SPRITE_PIPELINE_PREMULT_DEPTH_WRITE;
		case r_state_particles_qmb_textured_blood:
			return VK_SPRITE_PIPELINE_DARKEN_DEPTH;
		case r_state_coronas:
			return VK_SPRITE_PIPELINE_ADDITIVE_NODEPTH;
		default:
			return VK_SPRITE_PIPELINE_PREMULT_DEPTH;
	}
}

static float VK_SpriteAlphaThresholdForBatch(const gl_sprite3d_batch_t* batch)
{
	return batch && batch->rendering_state == r_state_sprites_textured ? 0.333f : 0.0f;
}

static VkShaderModule VK_SpriteCreateShaderModule(const unsigned char* bytes, unsigned int length)
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

static VkPrimitiveTopology VK_SpriteTopology(r_primitive_id primitive)
{
	switch (primitive) {
		case r_primitive_triangle_strip:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		case r_primitive_triangle_fan:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
		case r_primitive_triangles:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		default:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	}
}

static void VK_SpriteSetViewportScissor(VkCommandBuffer commandBuffer)
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

static qbool VK_SpriteCreatePipeline(r_primitive_id primitive, vk_sprite_pipeline_id_t pipelineId)
{
	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;
	VkPipelineShaderStageCreateInfo shaderStages[2];
	VkVertexInputBindingDescription bindingDescription;
	VkVertexInputAttributeDescription attributeDescriptions[3];
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
	const vk_sprite_pipeline_state_t* state;

	VkPipeline* pipeline;

	if (primitive < 0 || primitive >= r_primitive_count || pipelineId < 0 || pipelineId >= VK_SPRITE_PIPELINE_COUNT) {
		return false;
	}
	pipeline = &spritePipelines[primitive][pipelineId];
	state = &spritePipelineStates[pipelineId];
	if (*pipeline != VK_NULL_HANDLE) {
		return true;
	}

	descriptorSetLayout = VK_TextureDescriptorSetLayout();
	if (descriptorSetLayout == VK_NULL_HANDLE) {
		return false;
	}

	vertShaderModule = VK_SpriteCreateShaderModule(vk_sprite3d_vert_spv, vk_sprite3d_vert_spv_len);
	fragShaderModule = VK_SpriteCreateShaderModule(vk_sprite3d_frag_spv, vk_sprite3d_frag_spv_len);
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
	bindingDescription.stride = sizeof(r_sprite3d_vert_t);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VK_InitialiseStructure(attributeDescriptions[0]);
	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[0].offset = VK_VBO_FIELDOFFSET(r_sprite3d_vert_t, position);

	VK_InitialiseStructure(attributeDescriptions[1]);
	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[1].offset = VK_VBO_FIELDOFFSET(r_sprite3d_vert_t, tex);

	VK_InitialiseStructure(attributeDescriptions[2]);
	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R8G8B8A8_UNORM;
	attributeDescriptions[2].offset = VK_VBO_FIELDOFFSET(r_sprite3d_vert_t, color);

	VK_InitialiseStructure(vertexInputInfo);
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = sizeof(attributeDescriptions) / sizeof(attributeDescriptions[0]);
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

	VK_InitialiseStructure(inputAssembly);
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_SpriteTopology(primitive);
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
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = state->depthBias ? VK_TRUE : VK_FALSE;
	rasterizer.depthBiasConstantFactor = glConfig.reversed_depth ? 2.0f : -2.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	VK_InitialiseStructure(multisampling);
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VK_InitialiseStructure(depthStencil);
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = state->depthTest ? VK_TRUE : VK_FALSE;
	depthStencil.depthWriteEnable = state->depthWrite ? VK_TRUE : VK_FALSE;
	depthStencil.depthCompareOp = glConfig.reversed_depth ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	VK_InitialiseStructure(colorBlendAttachment);
	colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;
	VK_BlendingConfigure(&colorBlending, &colorBlendAttachment, state->blendFunc);

	VK_InitialiseStructure(dynamicState);
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
	dynamicState.pDynamicStates = dynamicStates;

	if (spritePipelineLayout == VK_NULL_HANDLE) {
		VK_InitialiseStructure(pushConstantRange);
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(vk_sprite3d_push_t);

		VK_InitialiseStructure(pipelineLayoutInfo);
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		if (vkCreatePipelineLayout(vk_options.logicalDevice, &pipelineLayoutInfo, NULL, &spritePipelineLayout) != VK_SUCCESS) {
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
	pipelineInfo.layout = spritePipelineLayout;
	pipelineInfo.renderPass = VK_MainRenderPass();
	pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(vk_options.logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, pipeline) != VK_SUCCESS) {
		*pipeline = VK_NULL_HANDLE;
	}

	vkDestroyShaderModule(vk_options.logicalDevice, fragShaderModule, NULL);
	vkDestroyShaderModule(vk_options.logicalDevice, vertShaderModule, NULL);
	return *pipeline != VK_NULL_HANDLE;
}

void VK_Sprite3DResourcesShutdown(void)
{
	int i;

	if (vk_options.logicalDevice != VK_NULL_HANDLE) {
		for (i = 0; i < r_primitive_count; ++i) {
			int j;

			for (j = 0; j < VK_SPRITE_PIPELINE_COUNT; ++j) {
				if (spritePipelines[i][j] != VK_NULL_HANDLE) {
					vkDestroyPipeline(vk_options.logicalDevice, spritePipelines[i][j], NULL);
					spritePipelines[i][j] = VK_NULL_HANDLE;
				}
			}
		}
		if (spritePipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(vk_options.logicalDevice, spritePipelineLayout, NULL);
			spritePipelineLayout = VK_NULL_HANDLE;
		}
	}
}

void VK_DrawClassicParticles(int particles_to_draw)
{
	r_sprite3d_vert_t* vert;

	if (particles_to_draw <= 0 || !R_TextureReferenceIsValid(particletexture)) {
		return;
	}

	R_Sprite3DInitialiseBatch(SPRITE3D_PARTICLES_CLASSIC, r_state_particles_classic, particletexture, 0, r_primitive_triangles);
	vert = R_Sprite3DAddEntry(SPRITE3D_PARTICLES_CLASSIC, 3 * particles_to_draw);
	if (vert) {
		memcpy(vert, glvertices, particles_to_draw * 3 * sizeof(glvertices[0]));
	}
}

void VK_DrawSimpleItem(model_t* model, int skin, vec3_t origin, float scale, vec3_t up, vec3_t right)
{
	texture_ref simpletexture;
	r_sprite3d_vert_t* vert;

	if (!model || skin < 0 || skin >= MAX_SIMPLE_TEXTURES) {
		return;
	}

	simpletexture = model->simpletexture[skin];
	if (!R_TextureReferenceIsValid(simpletexture)) {
		return;
	}

	vert = R_Sprite3DAddEntrySpecific(SPRITE3D_ENTITIES, 4, simpletexture, 0);
	if (vert) {
		R_Sprite3DRender(vert, origin, up, right, scale, -scale, -scale, scale, 1.0f, 1.0f, 0);
	}
}

void VK_DrawSpriteModel(entity_t* e)
{
	vec3_t right;
	vec3_t up;
	mspriteframe_t* frame;
	msprite2_t* psprite;
	r_sprite3d_vert_t* vert;

	if (!e || !e->model) {
		return;
	}

	psprite = (msprite2_t*)Mod_Extradata(e->model);
	frame = R_GetSpriteFrame(e, psprite);
	if (!frame || !R_TextureReferenceIsValid(frame->gl_texturenum)) {
		return;
	}

	if (psprite->type == SPR_ORIENTED) {
		AngleVectors(e->angles, NULL, right, up);
	}
	else if (psprite->type == SPR_FACING_UPRIGHT) {
		VectorSet(up, 0, 0, 1);
		right[0] = e->origin[1] - r_origin[1];
		right[1] = -(e->origin[0] - r_origin[0]);
		right[2] = 0;
		VectorNormalizeFast(right);
	}
	else if (psprite->type == SPR_VP_PARALLEL_UPRIGHT) {
		VectorSet(up, 0, 0, 1);
		VectorCopy(vright, right);
	}
	else {
		VectorCopy(vup, up);
		VectorCopy(vright, right);
	}

	vert = R_Sprite3DAddEntrySpecific(SPRITE3D_ENTITIES, 4, frame->gl_texturenum, 0);
	if (vert) {
		R_Sprite3DRender(vert, e->origin, up, right, frame->up, frame->down, frame->left, frame->right, 1.0f, 1.0f, 0);
	}
}

void VK_Prepare3DSprites(void)
{
	if (!batchCount || !vertexCount || (batchCount == 1 && !batches[0].count)) {
		return;
	}

	R_Sprite3DCreateVBO();
	if (R_BufferReferenceIsValid(r_buffer_sprite_vertex_data)) {
		buffers.Update(r_buffer_sprite_vertex_data, vertexCount * sizeof(verts[0]), verts);
	}
}

void VK_Draw3DSprites(void)
{
	VkCommandBuffer commandBuffer;
	VkBuffer vertexBuffer;
	VkDeviceSize vertexOffset = 0;
	vk_sprite3d_push_t push;
	unsigned int i;

	if (!batchCount || !vertexCount || (batchCount == 1 && !batches[0].count)) {
		return;
	}

	commandBuffer = VK_CurrentCommandBuffer();
	if (commandBuffer == VK_NULL_HANDLE) {
		R_Sprite3DClearBatches();
		return;
	}

	vertexBuffer = VK_BufferHandle(r_buffer_sprite_vertex_data);
	if (vertexBuffer == VK_NULL_HANDLE) {
		R_Sprite3DClearBatches();
		return;
	}

	memset(&push, 0, sizeof(push));
	memcpy(push.modelView, R_ModelviewMatrix(), sizeof(push.modelView));
	memcpy(push.projection, R_ProjectionMatrix(), sizeof(push.projection));

	VK_SpriteSetViewportScissor(commandBuffer);
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &vertexOffset);

	for (i = 0; i < batchCount; ++i) {
		gl_sprite3d_batch_t* batch = &batches[i];
		vk_sprite_pipeline_id_t pipelineId = VK_SpritePipelineForBatch(batch);
		unsigned int j;

		if (!batch->count || !VK_SpriteCreatePipeline(batch->primitive_id, pipelineId)) {
			continue;
		}

		push.alphaThreshold = VK_SpriteAlphaThresholdForBatch(batch);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spritePipelines[batch->primitive_id][pipelineId]);
		vkCmdPushConstants(commandBuffer, spritePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);

		for (j = 0; j < batch->count; ++j) {
			texture_ref texture = R_TextureReferenceIsValid(batch->texture) ? batch->texture : batch->textures[j];
			VkDescriptorSet descriptorSet;

			if (!R_TextureReferenceIsValid(texture)) {
				texture = solidwhite_texture;
			}
			if (!VK_TextureReady(texture)) {
				continue;
			}

			descriptorSet = VK_TextureDescriptorSet(texture);
			if (descriptorSet == VK_NULL_HANDLE) {
				continue;
			}

			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spritePipelineLayout, 0, 1, &descriptorSet, 0, NULL);
			vkCmdDraw(commandBuffer, batch->numVertices[j], 1, batch->firstVertices[j], 0);
		}
	}

	R_Sprite3DClearBatches();
}

#endif // RENDERER_OPTION_VULKAN
