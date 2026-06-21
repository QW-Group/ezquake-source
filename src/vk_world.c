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
#ifdef __ANDROID__
#include <android/log.h>
#endif

#include "gl_model.h"
#include "r_aliasmodel.h"
#include "r_brushmodel.h"
#include "r_buffers.h"
#include "r_brushmodel_sky.h"
#include "r_lightmaps.h"
#include "r_lightmaps_internal.h"
#include "r_matrix.h"
#include "r_texture.h"
#include "glsl/constants.glsl"
#include "tr_types.h"
#include "vk_local.h"

extern const unsigned char vk_world_flat_vert_spv[];
extern const unsigned int vk_world_flat_vert_spv_len;
extern const unsigned char vk_world_flat_frag_spv[];
extern const unsigned int vk_world_flat_frag_spv_len;
extern const unsigned char vk_world_textured_vert_spv[];
extern const unsigned int vk_world_textured_vert_spv_len;
extern const unsigned char vk_world_textured_frag_spv[];
extern const unsigned int vk_world_textured_frag_spv_len;
extern const unsigned char vk_world_overlay_frag_spv[];
extern const unsigned int vk_world_overlay_frag_spv_len;
extern const unsigned char vk_world_lightmapped_vert_spv[];
extern const unsigned int vk_world_lightmapped_vert_spv_len;
extern const unsigned char vk_world_lightmapped_frag_spv[];
extern const unsigned int vk_world_lightmapped_frag_spv_len;
extern const unsigned char vk_world_alpha_textured_vert_spv[];
extern const unsigned int vk_world_alpha_textured_vert_spv_len;
extern const unsigned char vk_world_alpha_textured_frag_spv[];
extern const unsigned int vk_world_alpha_textured_frag_spv_len;

extern cvar_t r_drawflat;
extern cvar_t r_drawflat_mode;
extern cvar_t r_fastsky;
extern cvar_t r_fastturb;
extern cvar_t r_skycolor;
extern cvar_t gl_fb_bmodels;
extern cvar_t gl_lumatextures;
extern cvar_t gl_detail;

byte* SurfaceFlatTurbColor(texture_t* texture);

typedef struct vk_world_draw_s {
	uint32_t firstIndex;
	uint32_t indexCount;
	float modelView[16];
	texture_ref texture;
	texture_ref lightmap;
	texture_ref overlayTexture;
	float alpha;
	float flatColor[4];
	float surfaceType;
	qbool textured;
	qbool lightmapped;
	qbool blended;
	qbool detail;
	qbool polygonOffset;
	int overlayMode;
} vk_world_draw_t;

typedef struct vk_world_push_s {
	float mvp[16];
	float color[4];
	float cameraPosition[4];
	float time;
	float alpha;
	float surfaceType;
	float useSkyTexture;
	float fastTurb;
	float detailEnabled;
	float padding[2];
} vk_world_push_t;

static VkPipelineLayout worldFlatPipelineLayout;
static VkPipeline worldFlatPipeline;
static VkDescriptorSetLayout worldFlatSkyDescriptorSetLayout;
static VkDescriptorPool worldFlatSkyDescriptorPool;
static VkDescriptorSet worldFlatSkyDescriptorSet;
static VkPipelineLayout worldTexturedPipelineLayout;
static VkPipeline worldTexturedPipeline;
static VkPipelineLayout worldOverlayPipelineLayout;
static VkPipeline worldLumaPipeline;
static VkPipeline worldFullbrightPipeline;
static VkPipelineLayout worldLightmappedPipelineLayout;
static VkPipeline worldLightmappedPipeline;
static VkPipelineLayout worldAlphaTexturedPipelineLayout;
static VkPipeline worldAlphaTexturedPipeline;
static vk_world_draw_t* worldDraws;
static int worldDrawCount;
static int worldDrawCapacity;
static uint32_t worldIndexCount;

#define VK_WORLD_OVERLAY_NONE       0
#define VK_WORLD_OVERLAY_LUMA       1
#define VK_WORLD_OVERLAY_FULLBRIGHT 2
#define VK_WORLD_SKY_TEXTURE_COUNT  (2 + MAX_SKYBOXTEXTURES)
#define VK_WORLD_SKY_MODE_NONE      0.0f
#define VK_WORLD_SKY_MODE_CLASSIC   1.0f
#define VK_WORLD_SKY_MODE_SKYBOX    2.0f

texture_t *R_TextureAnimation(entity_t* ent, texture_t *base);

static float VK_WorldSurfaceType(msurface_t* surf)
{
	int type = 0;

	if (!surf) {
		return 0.0f;
	}
	if (surf->flags & SURF_DRAWSKY) {
		type = TEXTURE_TURB_SKY;
	}
	else if (surf->flags & SURF_DRAWTURB) {
		type = surf->texinfo->texture->turbType & EZQ_SURFACE_TYPE;
		if (!type) {
			type = TEXTURE_TURB_OTHER;
		}
	}

	return (float)type;
}

static void VK_WorldFlatColorForSurface(msurface_t* surf, float* color)
{
	byte* turbColor;
	byte rgba[4];

	color[0] = color[1] = color[2] = color[3] = 1.0f;

	if (!surf || !surf->texinfo || !surf->texinfo->texture) {
		return;
	}
	if (surf->flags & SURF_DRAWSKY) {
		color[0] = (float)r_skycolor.color[0] / 255.0f;
		color[1] = (float)r_skycolor.color[1] / 255.0f;
		color[2] = (float)r_skycolor.color[2] / 255.0f;
		return;
	}
	if (surf->flags & SURF_DRAWTURB) {
		turbColor = SurfaceFlatTurbColor(surf->texinfo->texture);
		color[0] = (float)turbColor[0] / 255.0f;
		color[1] = (float)turbColor[1] / 255.0f;
		color[2] = (float)turbColor[2] / 255.0f;
		return;
	}

	COLOR_TO_RGBA(surf->texinfo->texture->flatcolor3ub, rgba);
	color[0] = (float)rgba[0] / 255.0f;
	color[1] = (float)rgba[1] / 255.0f;
	color[2] = (float)rgba[2] / 255.0f;
}

static qbool VK_WorldSkyTexturesReady(void)
{
	return !r_fastsky.integer && VK_TextureReady(solidskytexture) && VK_TextureReady(alphaskytexture);
}

static qbool VK_WorldSkyboxTexturesReady(void)
{
	int i;

	if (r_fastsky.integer || !r_skyboxloaded) {
		return false;
	}

	for (i = 0; i < MAX_SKYBOXTEXTURES; ++i) {
		if (!VK_TextureReady(skyboxtextures[i])) {
			return false;
		}
	}

	return true;
}

static qbool VK_WorldEnsureFlatSkyDescriptorSetLayout(void)
{
	VkDescriptorSetLayoutBinding bindings[VK_WORLD_SKY_TEXTURE_COUNT];
	VkDescriptorSetLayoutCreateInfo layoutInfo;
	int i;

	if (worldFlatSkyDescriptorSetLayout != VK_NULL_HANDLE) {
		return true;
	}

	for (i = 0; i < VK_WORLD_SKY_TEXTURE_COUNT; ++i) {
		VK_InitialiseStructure(bindings[i]);
		bindings[i].binding = i;
		bindings[i].descriptorCount = 1;
		bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[i].pImmutableSamplers = NULL;
		bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	}

	VK_InitialiseStructure(layoutInfo);
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = VK_WORLD_SKY_TEXTURE_COUNT;
	layoutInfo.pBindings = bindings;

	return vkCreateDescriptorSetLayout(vk_options.logicalDevice, &layoutInfo, NULL, &worldFlatSkyDescriptorSetLayout) == VK_SUCCESS;
}

static qbool VK_WorldEnsureFlatSkyDescriptorSet(void)
{
	VkDescriptorPoolSize poolSize;
	VkDescriptorPoolCreateInfo poolInfo;
	VkDescriptorSetAllocateInfo allocInfo;

	if (worldFlatSkyDescriptorSet != VK_NULL_HANDLE) {
		return true;
	}
	if (!VK_WorldEnsureFlatSkyDescriptorSetLayout()) {
		return false;
	}
	if (worldFlatSkyDescriptorPool == VK_NULL_HANDLE) {
		VK_InitialiseStructure(poolSize);
		poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSize.descriptorCount = VK_WORLD_SKY_TEXTURE_COUNT;

		VK_InitialiseStructure(poolInfo);
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		poolInfo.maxSets = 1;

		if (vkCreateDescriptorPool(vk_options.logicalDevice, &poolInfo, NULL, &worldFlatSkyDescriptorPool) != VK_SUCCESS) {
			return false;
		}
	}

	VK_InitialiseStructure(allocInfo);
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = worldFlatSkyDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &worldFlatSkyDescriptorSetLayout;

	return vkAllocateDescriptorSets(vk_options.logicalDevice, &allocInfo, &worldFlatSkyDescriptorSet) == VK_SUCCESS;
}

static qbool VK_WorldFlatSkyDescriptorSet(VkDescriptorSet* descriptorSet)
{
	static const int skytexorder[MAX_SKYBOXTEXTURES] = { 0, 2, 1, 3, 4, 5 };
	VkDescriptorImageInfo imageInfos[VK_WORLD_SKY_TEXTURE_COUNT];
	VkWriteDescriptorSet descriptorWrites[VK_WORLD_SKY_TEXTURE_COUNT];
	texture_ref textures[VK_WORLD_SKY_TEXTURE_COUNT];
	int i;

	if (!descriptorSet || !VK_WorldEnsureFlatSkyDescriptorSet()) {
		return false;
	}

	textures[0] = VK_TextureReady(solidskytexture) ? solidskytexture : solidwhite_texture;
	textures[1] = VK_TextureReady(alphaskytexture) ? alphaskytexture : transparent_texture;
	for (i = 0; i < MAX_SKYBOXTEXTURES; ++i) {
		texture_ref skybox = skyboxtextures[(int)bound(0, skytexorder[i], MAX_SKYBOXTEXTURES - 1)];
		textures[i + 2] = VK_TextureReady(skybox) ? skybox : solidwhite_texture;
	}

	for (i = 0; i < VK_WORLD_SKY_TEXTURE_COUNT; ++i) {
		if (!VK_TextureDescriptorImageInfo(textures[i], false, &imageInfos[i])) {
			return false;
		}

		VK_InitialiseStructure(descriptorWrites[i]);
		descriptorWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[i].dstSet = worldFlatSkyDescriptorSet;
		descriptorWrites[i].dstBinding = i;
		descriptorWrites[i].descriptorCount = 1;
		descriptorWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[i].pImageInfo = &imageInfos[i];
	}

	vkUpdateDescriptorSets(vk_options.logicalDevice, VK_WORLD_SKY_TEXTURE_COUNT, descriptorWrites, 0, NULL);
	*descriptorSet = worldFlatSkyDescriptorSet;
	return true;
}

static int VK_WorldOverlayMode(texture_t* texture)
{
	if (!texture || !R_TextureReferenceIsValid(texture->fb_texturenum) || !VK_TextureReady(texture->fb_texturenum)) {
		return VK_WORLD_OVERLAY_NONE;
	}
	if (texture->isLumaTexture) {
		return (gl_lumatextures.integer && r_refdef2.allow_lumas) ? VK_WORLD_OVERLAY_LUMA : VK_WORLD_OVERLAY_NONE;
	}
	return gl_fb_bmodels.integer ? VK_WORLD_OVERLAY_FULLBRIGHT : VK_WORLD_OVERLAY_NONE;
}

static qbool VK_WorldDetailTextureReady(void)
{
	return gl_detail.integer && VK_TextureReady(detailtexture);
}

static VkDescriptorSet VK_WorldDetailDescriptorSet(void)
{
	texture_ref texture = VK_WorldDetailTextureReady() ? detailtexture : solidwhite_texture;

	return VK_TextureDescriptorSet(texture);
}

static void VK_WorldDebugLog(const char* fmt, ...)
{
	(void)fmt;
}

static VkShaderModule VK_WorldCreateShaderModule(const unsigned char* bytes, unsigned int length)
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

static void VK_WorldSetViewportScissor(VkCommandBuffer commandBuffer)
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

static qbool VK_WorldEnsureDrawCapacity(void)
{
	if (worldDrawCount >= worldDrawCapacity) {
		int newCapacity = worldDrawCapacity ? worldDrawCapacity * 2 : 128;
		vk_world_draw_t* newDraws = Q_malloc(newCapacity * sizeof(newDraws[0]));

		if (worldDraws) {
			memcpy(newDraws, worldDraws, worldDrawCount * sizeof(worldDraws[0]));
			Q_free(worldDraws);
		}
		worldDraws = newDraws;
		worldDrawCapacity = newCapacity;
	}

	return worldDraws != NULL;
}

static qbool VK_WorldAppendIndex(uint32_t index)
{
	if (worldIndexCount >= modelIndexMaximum) {
		return false;
	}

	modelIndexes[worldIndexCount++] = index;
	return true;
}

static qbool VK_WorldAppendSurface(msurface_t* surf, uint32_t* emittedPolys)
{
	glpoly_t* poly;

	for (poly = surf->polys; poly; poly = poly->next) {
		int i;

		if (!poly->numverts) {
			continue;
		}

		if (*emittedPolys && !VK_WorldAppendIndex(UINT32_MAX)) {
			return false;
		}

		for (i = 0; i < poly->numverts; ++i) {
			if (!VK_WorldAppendIndex(poly->vbo_start + i)) {
				return false;
			}
		}

		++(*emittedPolys);
	}

	return true;
}

static texture_ref VK_WorldLightmapTextureForSurface(msurface_t* surf)
{
	texture_ref lightmap = null_texture_reference;

	if (!surf || surf->lightmaptexturenum < 0 || surf->lightmaptexturenum >= (int)lightmap_array_size) {
		return lightmap;
	}
	if (surf->flags & (SURF_DRAWTURB | SURF_DRAWSKY)) {
		return lightmap;
	}

	lightmap = lightmaps[surf->lightmaptexturenum].gl_texref;
	return VK_TextureReady(lightmap) ? lightmap : null_texture_reference;
}

static void VK_WorldQueueSurface(model_t* model, msurface_t* surf, qbool drawflat, texture_t* materialTexture, texture_ref texture, float alpha, qbool blended, const float* modelView, qbool polygonOffset)
{
	uint32_t firstIndex;
	uint32_t emittedPolys = 0;
	vk_world_draw_t* draw;
	texture_ref lightmap;

	if (!model || !surf || !modelIndexes || modelIndexMaximum == 0 || !VK_WorldEnsureDrawCapacity()) {
		VK_WorldDebugLog(
			"queue surface skipped model=%s surface=%p modelIndexes=%p max=%u drawCapacity=%d",
			model ? model->name : "(null)",
			(void*)surf,
			(void*)modelIndexes,
			modelIndexMaximum,
			worldDrawCapacity);
		return;
	}

	firstIndex = worldIndexCount;
	if (!VK_WorldAppendSurface(surf, &emittedPolys)) {
		VK_WorldDebugLog("queue surface overflow model=%s surface=%d indices=%u max=%u", model->name, surf->surfacenum, worldIndexCount, modelIndexMaximum);
	}
	if (worldIndexCount == firstIndex) {
		VK_WorldDebugLog("queue surface empty model=%s surface=%d", model->name, surf->surfacenum);
		return;
	}

	lightmap = VK_WorldLightmapTextureForSurface(surf);
	draw = &worldDraws[worldDrawCount++];
	draw->firstIndex = firstIndex;
	draw->indexCount = worldIndexCount - firstIndex;
	memcpy(draw->modelView, modelView, sizeof(draw->modelView));
	draw->texture = texture;
	draw->lightmap = lightmap;
	draw->overlayTexture = materialTexture ? materialTexture->fb_texturenum : null_texture_reference;
	draw->alpha = bound(0.0f, alpha, 1.0f);
	VK_WorldFlatColorForSurface(surf, draw->flatColor);
	draw->surfaceType = VK_WorldSurfaceType(surf);
	draw->textured = !drawflat && VK_TextureReady(texture);
	draw->lightmapped = draw->textured && !blended && VK_TextureReady(lightmap);
	draw->blended = blended;
	draw->detail = model->isworldmodel && !(surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB));
	draw->polygonOffset = polygonOffset;
	draw->overlayMode = draw->textured ? VK_WorldOverlayMode(materialTexture) : VK_WORLD_OVERLAY_NONE;
	VK_WorldDebugLog(
		"queued surface model=%s draw=%d surface=%d first=%u count=%u polys=%u textured=%d lightmapped=%d blended=%d overlay=%d alpha=%.2f type=%.0f tex=%u lightmap=%u",
		model->name,
		worldDrawCount,
		surf->surfacenum,
		draw->firstIndex,
		draw->indexCount,
		emittedPolys,
		draw->textured,
		draw->lightmapped,
		draw->blended,
		draw->overlayMode,
		draw->alpha,
		draw->surfaceType,
		texture.index,
		lightmap.index);
}

static void VK_WorldQueueDrawflatSurfaces(model_t* model, const float* modelView, qbool polygonOffset)
{
	msurface_t* surf;

	for (surf = model ? model->drawflat_chain : NULL; surf; surf = surf->drawflatchain) {
		VK_WorldQueueSurface(model, surf, true, NULL, null_texture_reference, 1.0f, false, modelView, polygonOffset);
	}
}

static void VK_WorldQueueModel(model_t* model, entity_t* ent, qbool polygonOffset)
{
	int i;
	float modelView[16];

	if (!model) {
		return;
	}

	R_GetModelviewMatrix(modelView);
	VK_WorldQueueDrawflatSurfaces(model, modelView, polygonOffset);

	for (i = max(model->first_texture_chained, 0); i <= model->last_texture_chained && i < model->numtextures; ++i) {
		texture_t* texture = model->textures[i];
		texture_t* animatedTexture;
		msurface_t* surf;

		if (!texture || !texture->texturechain) {
			continue;
		}

		animatedTexture = R_TextureAnimation(ent, texture);
		for (surf = texture->texturechain; surf; surf = surf->texturechain) {
			VK_WorldQueueSurface(model, surf, false, animatedTexture, animatedTexture ? animatedTexture->gl_texturenum : null_texture_reference, 1.0f, false, modelView, polygonOffset);
		}
	}
}

static qbool VK_WorldCreateFlatPipeline(void)
{
	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;
	VkPipelineShaderStageCreateInfo shaderStages[2];
	VkVertexInputBindingDescription bindingDescription;
	VkVertexInputAttributeDescription attributeDescriptions[2];
	VkPipelineVertexInputStateCreateInfo vertexInputInfo;
	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	VkPipelineViewportStateCreateInfo viewportState;
	VkPipelineRasterizationStateCreateInfo rasterizer;
	VkPipelineMultisampleStateCreateInfo multisampling;
	VkPipelineDepthStencilStateCreateInfo depthStencil;
	VkPipelineColorBlendAttachmentState colorBlendAttachment;
	VkPipelineColorBlendStateCreateInfo colorBlending;
	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_BLEND_CONSTANTS, VK_DYNAMIC_STATE_DEPTH_BIAS };
	VkPipelineDynamicStateCreateInfo dynamicState;
	VkPushConstantRange pushConstantRange;
	VkPipelineLayoutCreateInfo pipelineLayoutInfo;
	VkGraphicsPipelineCreateInfo pipelineInfo;

	if (worldFlatPipeline != VK_NULL_HANDLE) {
		return true;
	}

	if (!VK_WorldEnsureFlatSkyDescriptorSetLayout()) {
		return false;
	}

	vertShaderModule = VK_WorldCreateShaderModule(vk_world_flat_vert_spv, vk_world_flat_vert_spv_len);
	fragShaderModule = VK_WorldCreateShaderModule(vk_world_flat_frag_spv, vk_world_flat_frag_spv_len);
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
	bindingDescription.stride = sizeof(vbo_world_vert_t);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VK_InitialiseStructure(attributeDescriptions[0]);
	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[0].offset = VK_VBO_FIELDOFFSET(vbo_world_vert_t, position);

	VK_InitialiseStructure(attributeDescriptions[1]);
	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[1].offset = VK_VBO_FIELDOFFSET(vbo_world_vert_t, flatcolor);

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
	rasterizer.depthBiasEnable = VK_TRUE;

	VK_InitialiseStructure(multisampling);
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VK_InitialiseStructure(depthStencil);
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = glConfig.reversed_depth ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	VK_InitialiseStructure(colorBlendAttachment);
	colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VK_InitialiseStructure(colorBlending);
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	VK_InitialiseStructure(dynamicState);
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
	dynamicState.pDynamicStates = dynamicStates;

	VK_InitialiseStructure(pushConstantRange);
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(vk_world_push_t);

	VK_InitialiseStructure(pipelineLayoutInfo);
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &worldFlatSkyDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
	if (vkCreatePipelineLayout(vk_options.logicalDevice, &pipelineLayoutInfo, NULL, &worldFlatPipelineLayout) != VK_SUCCESS) {
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
	pipelineInfo.layout = worldFlatPipelineLayout;
	pipelineInfo.renderPass = VK_MainRenderPass();
	pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(vk_options.logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &worldFlatPipeline) != VK_SUCCESS) {
		worldFlatPipeline = VK_NULL_HANDLE;
	}

	vkDestroyShaderModule(vk_options.logicalDevice, fragShaderModule, NULL);
	vkDestroyShaderModule(vk_options.logicalDevice, vertShaderModule, NULL);
	return worldFlatPipeline != VK_NULL_HANDLE;
}

static qbool VK_WorldCreateTexturedPipeline(void)
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
	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_BIAS };
	VkPipelineDynamicStateCreateInfo dynamicState;
	VkPushConstantRange pushConstantRange;
	VkPipelineLayoutCreateInfo pipelineLayoutInfo;
	VkGraphicsPipelineCreateInfo pipelineInfo;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSetLayout descriptorSetLayouts[2];

	if (worldTexturedPipeline != VK_NULL_HANDLE) {
		return true;
	}

	descriptorSetLayout = VK_TextureDescriptorSetLayout();
	if (descriptorSetLayout == VK_NULL_HANDLE) {
		return false;
	}
	descriptorSetLayouts[0] = descriptorSetLayout;
	descriptorSetLayouts[1] = descriptorSetLayout;

	vertShaderModule = VK_WorldCreateShaderModule(vk_world_textured_vert_spv, vk_world_textured_vert_spv_len);
	fragShaderModule = VK_WorldCreateShaderModule(vk_world_textured_frag_spv, vk_world_textured_frag_spv_len);
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
	bindingDescription.stride = sizeof(vbo_world_vert_t);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VK_InitialiseStructure(attributeDescriptions[0]);
	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[0].offset = VK_VBO_FIELDOFFSET(vbo_world_vert_t, position);

	VK_InitialiseStructure(attributeDescriptions[1]);
	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[1].offset = VK_VBO_FIELDOFFSET(vbo_world_vert_t, material_coords);

	VK_InitialiseStructure(attributeDescriptions[2]);
	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[2].offset = VK_VBO_FIELDOFFSET(vbo_world_vert_t, detail_coords);

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
	rasterizer.depthBiasEnable = VK_TRUE;

	VK_InitialiseStructure(multisampling);
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VK_InitialiseStructure(depthStencil);
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = glConfig.reversed_depth ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	VK_InitialiseStructure(colorBlendAttachment);
	colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VK_InitialiseStructure(colorBlending);
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	VK_InitialiseStructure(dynamicState);
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
	dynamicState.pDynamicStates = dynamicStates;

	VK_InitialiseStructure(pushConstantRange);
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(vk_world_push_t);

	VK_InitialiseStructure(pipelineLayoutInfo);
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = sizeof(descriptorSetLayouts) / sizeof(descriptorSetLayouts[0]);
	pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
	if (vkCreatePipelineLayout(vk_options.logicalDevice, &pipelineLayoutInfo, NULL, &worldTexturedPipelineLayout) != VK_SUCCESS) {
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
	pipelineInfo.layout = worldTexturedPipelineLayout;
	pipelineInfo.renderPass = VK_MainRenderPass();
	pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(vk_options.logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &worldTexturedPipeline) != VK_SUCCESS) {
		worldTexturedPipeline = VK_NULL_HANDLE;
	}

	vkDestroyShaderModule(vk_options.logicalDevice, fragShaderModule, NULL);
	vkDestroyShaderModule(vk_options.logicalDevice, vertShaderModule, NULL);
	return worldTexturedPipeline != VK_NULL_HANDLE;
}

static qbool VK_WorldCreateOverlayPipeline(qbool luma)
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
	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_BIAS };
	VkPipelineDynamicStateCreateInfo dynamicState;
	VkPushConstantRange pushConstantRange;
	VkPipelineLayoutCreateInfo pipelineLayoutInfo;
	VkGraphicsPipelineCreateInfo pipelineInfo;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSetLayout descriptorSetLayouts[2];
	VkPipeline* pipeline = luma ? &worldLumaPipeline : &worldFullbrightPipeline;

	if (*pipeline != VK_NULL_HANDLE) {
		return true;
	}

	descriptorSetLayout = VK_TextureDescriptorSetLayout();
	if (descriptorSetLayout == VK_NULL_HANDLE) {
		return false;
	}
	descriptorSetLayouts[0] = descriptorSetLayout;
	descriptorSetLayouts[1] = descriptorSetLayout;

	vertShaderModule = VK_WorldCreateShaderModule(vk_world_textured_vert_spv, vk_world_textured_vert_spv_len);
	fragShaderModule = VK_WorldCreateShaderModule(vk_world_overlay_frag_spv, vk_world_overlay_frag_spv_len);
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
	bindingDescription.stride = sizeof(vbo_world_vert_t);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VK_InitialiseStructure(attributeDescriptions[0]);
	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[0].offset = VK_VBO_FIELDOFFSET(vbo_world_vert_t, position);

	VK_InitialiseStructure(attributeDescriptions[1]);
	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[1].offset = VK_VBO_FIELDOFFSET(vbo_world_vert_t, material_coords);

	VK_InitialiseStructure(attributeDescriptions[2]);
	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[2].offset = VK_VBO_FIELDOFFSET(vbo_world_vert_t, detail_coords);

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
	rasterizer.depthBiasEnable = VK_TRUE;

	VK_InitialiseStructure(multisampling);
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VK_InitialiseStructure(depthStencil);
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_FALSE;
	depthStencil.depthCompareOp = glConfig.reversed_depth ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	VK_InitialiseStructure(colorBlendAttachment);
	colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = luma ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = luma ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = luma ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
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

	VK_InitialiseStructure(pushConstantRange);
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(vk_world_push_t);

	if (worldOverlayPipelineLayout == VK_NULL_HANDLE) {
		VK_InitialiseStructure(pipelineLayoutInfo);
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = sizeof(descriptorSetLayouts) / sizeof(descriptorSetLayouts[0]);
		pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		if (vkCreatePipelineLayout(vk_options.logicalDevice, &pipelineLayoutInfo, NULL, &worldOverlayPipelineLayout) != VK_SUCCESS) {
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
	pipelineInfo.layout = worldOverlayPipelineLayout;
	pipelineInfo.renderPass = VK_MainRenderPass();
	pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(vk_options.logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, pipeline) != VK_SUCCESS) {
		*pipeline = VK_NULL_HANDLE;
	}

	vkDestroyShaderModule(vk_options.logicalDevice, fragShaderModule, NULL);
	vkDestroyShaderModule(vk_options.logicalDevice, vertShaderModule, NULL);
	return *pipeline != VK_NULL_HANDLE;
}

static qbool VK_WorldCreateAlphaTexturedPipeline(void)
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
	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_BIAS };
	VkPipelineDynamicStateCreateInfo dynamicState;
	VkPushConstantRange pushConstantRange;
	VkPipelineLayoutCreateInfo pipelineLayoutInfo;
	VkGraphicsPipelineCreateInfo pipelineInfo;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSetLayout descriptorSetLayouts[2];

	if (worldAlphaTexturedPipeline != VK_NULL_HANDLE) {
		return true;
	}

	descriptorSetLayout = VK_TextureDescriptorSetLayout();
	if (descriptorSetLayout == VK_NULL_HANDLE) {
		return false;
	}
	descriptorSetLayouts[0] = descriptorSetLayout;
	descriptorSetLayouts[1] = descriptorSetLayout;

	vertShaderModule = VK_WorldCreateShaderModule(vk_world_alpha_textured_vert_spv, vk_world_alpha_textured_vert_spv_len);
	fragShaderModule = VK_WorldCreateShaderModule(vk_world_alpha_textured_frag_spv, vk_world_alpha_textured_frag_spv_len);
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
	bindingDescription.stride = sizeof(vbo_world_vert_t);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VK_InitialiseStructure(attributeDescriptions[0]);
	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[0].offset = VK_VBO_FIELDOFFSET(vbo_world_vert_t, position);

	VK_InitialiseStructure(attributeDescriptions[1]);
	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[1].offset = VK_VBO_FIELDOFFSET(vbo_world_vert_t, material_coords);

	VK_InitialiseStructure(attributeDescriptions[2]);
	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[2].offset = VK_VBO_FIELDOFFSET(vbo_world_vert_t, detail_coords);

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
	rasterizer.depthBiasEnable = VK_TRUE;

	VK_InitialiseStructure(multisampling);
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VK_InitialiseStructure(depthStencil);
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_FALSE;
	depthStencil.depthCompareOp = glConfig.reversed_depth ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	VK_InitialiseStructure(colorBlendAttachment);
	colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_CONSTANT_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
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

	VK_InitialiseStructure(pushConstantRange);
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(vk_world_push_t);

	VK_InitialiseStructure(pipelineLayoutInfo);
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = sizeof(descriptorSetLayouts) / sizeof(descriptorSetLayouts[0]);
	pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
	if (vkCreatePipelineLayout(vk_options.logicalDevice, &pipelineLayoutInfo, NULL, &worldAlphaTexturedPipelineLayout) != VK_SUCCESS) {
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
	pipelineInfo.layout = worldAlphaTexturedPipelineLayout;
	pipelineInfo.renderPass = VK_MainRenderPass();
	pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(vk_options.logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &worldAlphaTexturedPipeline) != VK_SUCCESS) {
		worldAlphaTexturedPipeline = VK_NULL_HANDLE;
	}

	vkDestroyShaderModule(vk_options.logicalDevice, fragShaderModule, NULL);
	vkDestroyShaderModule(vk_options.logicalDevice, vertShaderModule, NULL);
	return worldAlphaTexturedPipeline != VK_NULL_HANDLE;
}

static qbool VK_WorldCreateLightmappedPipeline(void)
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
	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_BIAS };
	VkPipelineDynamicStateCreateInfo dynamicState;
	VkPushConstantRange pushConstantRange;
	VkPipelineLayoutCreateInfo pipelineLayoutInfo;
	VkGraphicsPipelineCreateInfo pipelineInfo;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSetLayout descriptorSetLayouts[3];

	if (worldLightmappedPipeline != VK_NULL_HANDLE) {
		return true;
	}

	descriptorSetLayout = VK_TextureDescriptorSetLayout();
	if (descriptorSetLayout == VK_NULL_HANDLE) {
		return false;
	}
	descriptorSetLayouts[0] = descriptorSetLayout;
	descriptorSetLayouts[1] = descriptorSetLayout;
	descriptorSetLayouts[2] = descriptorSetLayout;

	vertShaderModule = VK_WorldCreateShaderModule(vk_world_lightmapped_vert_spv, vk_world_lightmapped_vert_spv_len);
	fragShaderModule = VK_WorldCreateShaderModule(vk_world_lightmapped_frag_spv, vk_world_lightmapped_frag_spv_len);
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
	bindingDescription.stride = sizeof(vbo_world_vert_t);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VK_InitialiseStructure(attributeDescriptions[0]);
	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[0].offset = VK_VBO_FIELDOFFSET(vbo_world_vert_t, position);

	VK_InitialiseStructure(attributeDescriptions[1]);
	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[1].offset = VK_VBO_FIELDOFFSET(vbo_world_vert_t, material_coords);

	VK_InitialiseStructure(attributeDescriptions[2]);
	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[2].offset = VK_VBO_FIELDOFFSET(vbo_world_vert_t, lightmap_coords);

	VK_InitialiseStructure(attributeDescriptions[3]);
	attributeDescriptions[3].binding = 0;
	attributeDescriptions[3].location = 3;
	attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[3].offset = VK_VBO_FIELDOFFSET(vbo_world_vert_t, detail_coords);

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
	rasterizer.depthBiasEnable = VK_TRUE;

	VK_InitialiseStructure(multisampling);
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VK_InitialiseStructure(depthStencil);
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = glConfig.reversed_depth ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	VK_InitialiseStructure(colorBlendAttachment);
	colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VK_InitialiseStructure(colorBlending);
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	VK_InitialiseStructure(dynamicState);
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
	dynamicState.pDynamicStates = dynamicStates;

	VK_InitialiseStructure(pushConstantRange);
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(vk_world_push_t);

	VK_InitialiseStructure(pipelineLayoutInfo);
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = sizeof(descriptorSetLayouts) / sizeof(descriptorSetLayouts[0]);
	pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
	if (vkCreatePipelineLayout(vk_options.logicalDevice, &pipelineLayoutInfo, NULL, &worldLightmappedPipelineLayout) != VK_SUCCESS) {
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
	pipelineInfo.layout = worldLightmappedPipelineLayout;
	pipelineInfo.renderPass = VK_MainRenderPass();
	pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(vk_options.logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &worldLightmappedPipeline) != VK_SUCCESS) {
		worldLightmappedPipeline = VK_NULL_HANDLE;
	}

	vkDestroyShaderModule(vk_options.logicalDevice, fragShaderModule, NULL);
	vkDestroyShaderModule(vk_options.logicalDevice, vertShaderModule, NULL);
	return worldLightmappedPipeline != VK_NULL_HANDLE;
}

void VK_WorldResourcesShutdown(void)
{
	if (vk_options.logicalDevice != VK_NULL_HANDLE) {
		if (worldFlatPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(vk_options.logicalDevice, worldFlatPipeline, NULL);
			worldFlatPipeline = VK_NULL_HANDLE;
		}
		if (worldFlatPipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(vk_options.logicalDevice, worldFlatPipelineLayout, NULL);
			worldFlatPipelineLayout = VK_NULL_HANDLE;
		}
		if (worldFlatSkyDescriptorPool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(vk_options.logicalDevice, worldFlatSkyDescriptorPool, NULL);
			worldFlatSkyDescriptorPool = VK_NULL_HANDLE;
			worldFlatSkyDescriptorSet = VK_NULL_HANDLE;
		}
		if (worldFlatSkyDescriptorSetLayout != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(vk_options.logicalDevice, worldFlatSkyDescriptorSetLayout, NULL);
			worldFlatSkyDescriptorSetLayout = VK_NULL_HANDLE;
		}
		if (worldTexturedPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(vk_options.logicalDevice, worldTexturedPipeline, NULL);
			worldTexturedPipeline = VK_NULL_HANDLE;
		}
		if (worldTexturedPipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(vk_options.logicalDevice, worldTexturedPipelineLayout, NULL);
			worldTexturedPipelineLayout = VK_NULL_HANDLE;
		}
		if (worldLumaPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(vk_options.logicalDevice, worldLumaPipeline, NULL);
			worldLumaPipeline = VK_NULL_HANDLE;
		}
		if (worldFullbrightPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(vk_options.logicalDevice, worldFullbrightPipeline, NULL);
			worldFullbrightPipeline = VK_NULL_HANDLE;
		}
		if (worldOverlayPipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(vk_options.logicalDevice, worldOverlayPipelineLayout, NULL);
			worldOverlayPipelineLayout = VK_NULL_HANDLE;
		}
		if (worldLightmappedPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(vk_options.logicalDevice, worldLightmappedPipeline, NULL);
			worldLightmappedPipeline = VK_NULL_HANDLE;
		}
		if (worldLightmappedPipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(vk_options.logicalDevice, worldLightmappedPipelineLayout, NULL);
			worldLightmappedPipelineLayout = VK_NULL_HANDLE;
		}
		if (worldAlphaTexturedPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(vk_options.logicalDevice, worldAlphaTexturedPipeline, NULL);
			worldAlphaTexturedPipeline = VK_NULL_HANDLE;
		}
		if (worldAlphaTexturedPipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(vk_options.logicalDevice, worldAlphaTexturedPipelineLayout, NULL);
			worldAlphaTexturedPipelineLayout = VK_NULL_HANDLE;
		}
	}

	Q_free(worldDraws);
	worldDraws = NULL;
	worldDrawCount = 0;
	worldDrawCapacity = 0;
	worldIndexCount = 0;
}

void VK_PrepareModelRendering(qbool vid_restart)
{
	if (vid_restart) {
		VK_WorldResourcesShutdown();
	}

	if (cl.worldmodel) {
		R_CreateInstanceVBO();
		R_CreateAliasModelVBO();
		R_BrushModelCreateVBO();
		VK_WorldDebugLog(
			"prepare map=%s maxIndexes=%u vboValid=%d vboSize=%u iboValid=%d iboSize=%u",
			cl.worldmodel->name,
			modelIndexMaximum,
			R_BufferReferenceIsValid(r_buffer_brushmodel_vertex_data),
			(unsigned int)buffers.Size(r_buffer_brushmodel_vertex_data),
			R_BufferReferenceIsValid(r_buffer_brushmodel_index_data),
			(unsigned int)buffers.Size(r_buffer_brushmodel_index_data));
	}
	else {
		VK_WorldDebugLog("prepare skipped: no worldmodel");
	}
}

void VK_PreRenderView(void)
{
	worldDrawCount = 0;
	worldIndexCount = 0;
	VK_AliasModelFrameReset();
}

void VK_DrawWorld(void)
{
	if (!cl.worldmodel) {
		return;
	}

	VK_WorldQueueModel(cl.worldmodel, NULL, false);
}

void VK_ChainBrushModelSurfaces(model_t* clmodel, entity_t* ent)
{
	int i;
	msurface_t* psurf;
	qbool drawFlatFloors;
	qbool drawFlatWalls;

	(void)ent;

	if (!clmodel) {
		return;
	}

	drawFlatFloors = r_drawflat_mode.integer == 0 && (r_drawflat.integer == 2 || r_drawflat.integer == 1) && clmodel->isworldmodel;
	drawFlatWalls = r_drawflat_mode.integer == 0 && (r_drawflat.integer == 3 || r_drawflat.integer == 1) && clmodel->isworldmodel;

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
	for (i = 0; i < clmodel->nummodelsurfaces; i++, psurf++) {
		if (psurf->flags & SURF_DRAWSKY) {
			CHAIN_SURF_B2F(psurf, clmodel->drawflat_chain);
			clmodel->drawflat_todo = true;
			clmodel->first_texture_chained = min(clmodel->first_texture_chained, psurf->texinfo->miptex);
			clmodel->last_texture_chained = max(clmodel->last_texture_chained, psurf->texinfo->miptex);
		}
		else if (psurf->flags & SURF_DRAWTURB) {
			if (r_fastturb.integer) {
				CHAIN_SURF_B2F(psurf, clmodel->drawflat_chain);
				clmodel->drawflat_todo = true;
			}
			else {
				CHAIN_SURF_B2F(psurf, psurf->texinfo->texture->texturechain);
			}
			clmodel->first_texture_chained = min(clmodel->first_texture_chained, psurf->texinfo->miptex);
			clmodel->last_texture_chained = max(clmodel->last_texture_chained, psurf->texinfo->miptex);
		}
		else {
			qbool alphaSurface = (psurf->flags & SURF_DRAWALPHA);

			if (!alphaSurface && drawFlatFloors && (psurf->flags & SURF_DRAWFLAT_FLOOR)) {
				chain_surfaces_simple_drawflat(&clmodel->drawflat_chain, psurf);
				clmodel->drawflat_todo = true;
			}
			else if (!alphaSurface && drawFlatWalls && !(psurf->flags & SURF_DRAWFLAT_FLOOR)) {
				chain_surfaces_simple_drawflat(&clmodel->drawflat_chain, psurf);
				clmodel->drawflat_todo = true;
			}
			else {
				chain_surfaces_simple(&psurf->texinfo->texture->texturechain, psurf);
				clmodel->first_texture_chained = min(clmodel->first_texture_chained, psurf->texinfo->miptex);
				clmodel->last_texture_chained = max(clmodel->last_texture_chained, psurf->texinfo->miptex);
			}
		}
	}
}

void VK_DrawBrushModel(entity_t* ent, qbool polygonOffset, qbool caustics)
{
	(void)caustics;

	if (!ent || !ent->model) {
		return;
	}

	VK_WorldQueueModel(ent->model, ent, polygonOffset);
}

void VK_DrawWaterSurfaces(void)
{
	extern msurface_t* waterchain;
	msurface_t* surf;
	float modelView[16];
	float alpha;

	if (!waterchain || !cl.worldmodel) {
		return;
	}

	R_GetModelviewMatrix(modelView);
	alpha = bound(0.0f, r_refdef2.wateralpha, 1.0f);

	for (surf = waterchain; surf; surf = surf->texturechain) {
		texture_t* texture = R_TextureAnimation(NULL, surf->texinfo->texture);

		VK_WorldQueueSurface(
			cl.worldmodel,
			surf,
			false,
			texture,
			texture ? texture->gl_texturenum : null_texture_reference,
			alpha,
			true,
			modelView,
			false);
	}

	waterchain = NULL;
}

static void VK_WorldDrawOverlay(VkCommandBuffer commandBuffer, const vk_world_draw_t* draw, const vk_world_push_t* push, qbool lumaPipelineReady, qbool fullbrightPipelineReady)
{
	VkDescriptorSet descriptorSets[2];
	VkPipeline pipeline;

	if (!draw || draw->overlayMode == VK_WORLD_OVERLAY_NONE || !VK_TextureReady(draw->overlayTexture)) {
		return;
	}
	if (draw->overlayMode == VK_WORLD_OVERLAY_LUMA) {
		if (!lumaPipelineReady) {
			return;
		}
		pipeline = worldLumaPipeline;
	}
	else {
		if (!fullbrightPipelineReady) {
			return;
		}
		pipeline = worldFullbrightPipeline;
	}

	descriptorSets[0] = VK_TextureDescriptorSet(draw->overlayTexture);
	descriptorSets[1] = VK_WorldDetailDescriptorSet();
	if (descriptorSets[0] == VK_NULL_HANDLE || descriptorSets[1] == VK_NULL_HANDLE) {
		return;
	}

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, worldOverlayPipelineLayout, 0, 2, descriptorSets, 0, NULL);
	vkCmdPushConstants(commandBuffer, worldOverlayPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(*push), push);
	vkCmdDrawIndexed(commandBuffer, draw->indexCount, 1, draw->firstIndex, 0, 0);
}

void VK_RenderView(void)
{
	VkCommandBuffer commandBuffer;
	VkBuffer vertexBuffer;
	VkBuffer indexBuffer;
	VkDeviceSize vertexOffset = 0;
	int i;
	int pass;
	int texturedDraws = 0;
	int lightmappedDraws = 0;
	int blendedDraws = 0;
	int lumaDraws = 0;
	int fullbrightDraws = 0;
	qbool texturedPipelineReady = false;
	qbool lightmappedPipelineReady = false;
	qbool alphaTexturedPipelineReady = false;
	qbool lumaPipelineReady = false;
	qbool fullbrightPipelineReady = false;
	qbool depthBiasActive = false;

	if (!worldDrawCount || !worldIndexCount) {
		VK_WorldDebugLog("render skipped: draws=%d indices=%u", worldDrawCount, worldIndexCount);
		return;
	}

	R_UploadChangedLightmaps();
	VK_Prepare3DSprites();

	if (!VK_WorldCreateFlatPipeline()) {
		VK_WorldDebugLog("render skipped: flat pipeline creation failed");
		return;
	}
	for (i = 0; i < worldDrawCount; ++i) {
		texturedDraws += worldDraws[i].textured ? 1 : 0;
		lightmappedDraws += worldDraws[i].lightmapped ? 1 : 0;
		blendedDraws += worldDraws[i].blended ? 1 : 0;
		lumaDraws += worldDraws[i].overlayMode == VK_WORLD_OVERLAY_LUMA ? 1 : 0;
		fullbrightDraws += worldDraws[i].overlayMode == VK_WORLD_OVERLAY_FULLBRIGHT ? 1 : 0;
	}
	if (lumaDraws) {
		lumaPipelineReady = VK_WorldCreateOverlayPipeline(true);
		if (!lumaPipelineReady) {
			VK_WorldDebugLog("render warning: luma overlay pipeline unavailable, skipping %d draws", lumaDraws);
		}
	}
	if (fullbrightDraws) {
		fullbrightPipelineReady = VK_WorldCreateOverlayPipeline(false);
		if (!fullbrightPipelineReady) {
			VK_WorldDebugLog("render warning: fullbright overlay pipeline unavailable, skipping %d draws", fullbrightDraws);
		}
	}
	if (blendedDraws) {
		alphaTexturedPipelineReady = VK_WorldCreateAlphaTexturedPipeline();
		if (!alphaTexturedPipelineReady) {
			VK_WorldDebugLog("render warning: alpha textured pipeline unavailable, falling back for %d draws", blendedDraws);
		}
	}
	if (lightmappedDraws) {
		lightmappedPipelineReady = VK_WorldCreateLightmappedPipeline();
		if (!lightmappedPipelineReady) {
			VK_WorldDebugLog("render warning: lightmapped pipeline unavailable, falling back for %d draws", lightmappedDraws);
		}
	}
	if (texturedDraws) {
		texturedPipelineReady = VK_WorldCreateTexturedPipeline();
		if (!texturedPipelineReady) {
			VK_WorldDebugLog("render warning: textured pipeline unavailable, falling back to flat for %d draws", texturedDraws);
		}
	}
	if (!R_BufferReferenceIsValid(r_buffer_brushmodel_vertex_data) || !R_BufferReferenceIsValid(r_buffer_brushmodel_index_data)) {
		VK_WorldDebugLog(
			"render skipped: buffers invalid vbo=%d ibo=%d",
			R_BufferReferenceIsValid(r_buffer_brushmodel_vertex_data),
			R_BufferReferenceIsValid(r_buffer_brushmodel_index_data));
		return;
	}

	commandBuffer = VK_CurrentCommandBuffer();
	if (commandBuffer == VK_NULL_HANDLE) {
		VK_WorldDebugLog("render skipped: no active command buffer");
		return;
	}

	buffers.Update(r_buffer_brushmodel_index_data, worldIndexCount * sizeof(modelIndexes[0]), modelIndexes);

	vertexBuffer = VK_BufferHandle(r_buffer_brushmodel_vertex_data);
	indexBuffer = VK_BufferHandle(r_buffer_brushmodel_index_data);
	if (vertexBuffer == VK_NULL_HANDLE || indexBuffer == VK_NULL_HANDLE) {
		VK_WorldDebugLog("render skipped: null VkBuffer vertex=%p index=%p", (void*)vertexBuffer, (void*)indexBuffer);
		return;
	}

	VK_WorldSetViewportScissor(commandBuffer);
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &vertexOffset);
	vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
	// VK_DYNAMIC_STATE_DEPTH_BIAS must be set at least once before any draw call
	// that uses a pipeline with depthBiasEnable=VK_TRUE, or its value is
	// undefined; this establishes the disabled baseline before the loop below
	// only toggles it on/off when a draw's polygonOffset flag actually changes.
	vkCmdSetDepthBias(commandBuffer, 0.0f, 0.0f, 0.0f);

	for (pass = 0; pass < 2; ++pass) {
		qbool blendedPass = (pass != 0);

		if (blendedPass) {
			VK_RenderAliasModels(false);
			VK_Draw3DSprites();
			VK_WorldSetViewportScissor(commandBuffer);
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &vertexOffset);
			vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		}

		for (i = 0; i < worldDrawCount; ++i) {
			vk_world_push_t push;
			VkPipelineLayout layout = worldFlatPipelineLayout;
			VkShaderStageFlags pushStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
			qbool drawBlended = worldDraws[i].blended && worldDraws[i].textured && alphaTexturedPipelineReady;
			qbool drawLightmapped = !drawBlended && worldDraws[i].lightmapped && lightmappedPipelineReady;
			qbool drawTextured = worldDraws[i].textured && texturedPipelineReady;

			if (!!worldDraws[i].blended != blendedPass) {
				continue;
			}

			memset(&push, 0, sizeof(push));
			R_MultiplyMatrix(worldDraws[i].modelView, R_ProjectionMatrix(), push.mvp);
			memcpy(push.color, worldDraws[i].flatColor, sizeof(push.color));
			push.cameraPosition[0] = r_refdef.vieworg[0];
			push.cameraPosition[1] = r_refdef.vieworg[1];
			push.cameraPosition[2] = r_refdef.vieworg[2];
			push.cameraPosition[3] = 0.0f;
			push.time = r_refdef2.time;
			push.alpha = worldDraws[i].alpha;
			push.surfaceType = worldDraws[i].surfaceType;
			push.useSkyTexture = VK_WORLD_SKY_MODE_NONE;
			push.fastTurb = (worldDraws[i].surfaceType > 0.5f && worldDraws[i].surfaceType < 5.5f && r_fastturb.integer) ? 1.0f : 0.0f;
			push.detailEnabled = (worldDraws[i].detail && VK_WorldDetailTextureReady()) ? 1.0f : 0.0f;
			if (worldDraws[i].surfaceType == TEXTURE_TURB_SKY) {
				if (VK_WorldSkyboxTexturesReady()) {
					push.useSkyTexture = VK_WORLD_SKY_MODE_SKYBOX;
				}
				else if (VK_WorldSkyTexturesReady()) {
					push.useSkyTexture = VK_WORLD_SKY_MODE_CLASSIC;
				}
			}

			if (drawBlended) {
				VkDescriptorSet descriptorSets[2];

				descriptorSets[0] = VK_TextureDescriptorSet(worldDraws[i].texture);
				descriptorSets[1] = VK_WorldDetailDescriptorSet();
				if (descriptorSets[0] != VK_NULL_HANDLE && descriptorSets[1] != VK_NULL_HANDLE) {
					float blendConstants[4] = { 0.0f, 0.0f, 0.0f, worldDraws[i].alpha };

					layout = worldAlphaTexturedPipelineLayout;
					vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, worldAlphaTexturedPipeline);
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 2, descriptorSets, 0, NULL);
					vkCmdSetBlendConstants(commandBuffer, blendConstants);
					drawTextured = false;
				}
				else {
					drawBlended = false;
				}
			}
			if (!drawBlended && drawLightmapped) {
				VkDescriptorSet descriptorSets[3];

				descriptorSets[0] = VK_TextureDescriptorSet(worldDraws[i].texture);
				descriptorSets[1] = VK_TextureDescriptorSet(worldDraws[i].lightmap);
				descriptorSets[2] = VK_WorldDetailDescriptorSet();
				if (descriptorSets[0] != VK_NULL_HANDLE && descriptorSets[1] != VK_NULL_HANDLE && descriptorSets[2] != VK_NULL_HANDLE) {
					layout = worldLightmappedPipelineLayout;
					vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, worldLightmappedPipeline);
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 3, descriptorSets, 0, NULL);
					drawTextured = false;
				}
				else {
					drawLightmapped = false;
				}
			}
			if (!drawBlended && !drawLightmapped && drawTextured) {
				VkDescriptorSet descriptorSets[2];

				descriptorSets[0] = VK_TextureDescriptorSet(worldDraws[i].texture);
				descriptorSets[1] = VK_WorldDetailDescriptorSet();
				if (descriptorSets[0] != VK_NULL_HANDLE && descriptorSets[1] != VK_NULL_HANDLE) {
					layout = worldTexturedPipelineLayout;
					vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, worldTexturedPipeline);
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 2, descriptorSets, 0, NULL);
				}
				else {
					drawTextured = false;
				}
			}
			if (!drawBlended && !drawLightmapped && !drawTextured) {
				VkDescriptorSet descriptorSet;

				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, worldFlatPipeline);
				if (VK_WorldFlatSkyDescriptorSet(&descriptorSet)) {
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, worldFlatPipelineLayout, 0, 1, &descriptorSet, 0, NULL);
				}
			}

			if (!!worldDraws[i].polygonOffset != depthBiasActive) {
				depthBiasActive = worldDraws[i].polygonOffset;
				// Pushes brush-model entities (doors, plats, buttons, ...) embedded flush
				// against world geometry slightly toward the camera so they win the depth
				// test deterministically instead of z-fighting/flickering against the
				// static world surface they're attached to. Same constants vkQuake uses
				// for a D32_SFLOAT depth buffer; matches gl_brush_polygonoffset on GLC/GLM.
				vkCmdSetDepthBias(commandBuffer, depthBiasActive ? -4.0f : 0.0f, 0.0f, depthBiasActive ? -0.125f : 0.0f);
			}

			vkCmdPushConstants(commandBuffer, layout, pushStages, 0, sizeof(push), &push);
			vkCmdDrawIndexed(commandBuffer, worldDraws[i].indexCount, 1, worldDraws[i].firstIndex, 0, 0);
			VK_WorldDrawOverlay(commandBuffer, &worldDraws[i], &push, lumaPipelineReady, fullbrightPipelineReady);
		}
	}

	VK_RenderAliasModels(true);

	VK_WorldDebugLog(
		"render issued draws=%d textured=%d lightmapped=%d blended=%d luma=%d fullbright=%d indices=%u firstIndex0=%u count0=%u",
		worldDrawCount,
		texturedDraws,
		lightmappedDraws,
		blendedDraws,
		lumaDraws,
		fullbrightDraws,
		worldIndexCount,
		worldDraws[0].firstIndex,
		worldDraws[0].indexCount);

#ifdef __ANDROID__
	{
		static unsigned int profile_frames;
		static unsigned int profile_draws_accum;
		static unsigned int profile_indices_accum;

		profile_draws_accum += worldDrawCount;
		profile_indices_accum += worldIndexCount;
		if (++profile_frames >= 60) {
			__android_log_print(ANDROID_LOG_INFO, "VK_PROFILE",
				"world avg draws=%.1f avg indices=%.0f (last textured=%d lightmapped=%d blended=%d luma=%d fullbright=%d)",
				profile_draws_accum / (float)profile_frames,
				profile_indices_accum / (float)profile_frames,
				texturedDraws, lightmappedDraws, blendedDraws, lumaDraws, fullbrightDraws);
			profile_frames = 0;
			profile_draws_accum = 0;
			profile_indices_accum = 0;
		}
	}
#endif
}

#endif // RENDERER_OPTION_VULKAN
