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
#include "r_brushmodel.h"
#include "r_buffers.h"
#include "r_brushmodel_sky.h"
#include "r_lightmaps.h"
#include "r_lightmaps_internal.h"
#include "r_local.h"
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
extern const unsigned char vk_world_normals_vert_spv[];
extern const unsigned int vk_world_normals_vert_spv_len;
extern const unsigned char vk_world_normals_frag_spv[];
extern const unsigned int vk_world_normals_frag_spv_len;

extern cvar_t r_drawflat;
extern cvar_t r_drawflat_mode;
extern cvar_t r_fastsky;
extern cvar_t r_fastturb;
extern cvar_t gl_textureless;
extern cvar_t r_skycolor;
extern cvar_t r_floorcolor;
extern cvar_t r_wallcolor;
extern cvar_t gl_fb_bmodels;
extern cvar_t gl_lumatextures;
extern cvar_t gl_detail;
// Normalizes the normals pass's linear depth the same way the outline shader
// un-normalizes it (see vk_world_normals.frag / vk_world_outline.frag) -- both
// sides must agree on this value or the depth-discontinuity threshold is
// meaningless.
extern cvar_t r_farclip;

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
	qbool drawflatCvar;
	qbool textured;
	qbool lightmapped;
	qbool blended;
	qbool detail;
	qbool caustics;
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
	float textureless;
	// 1.0 tells vk_world_flat.frag to paint surfaceType==0 (normal wall/floor)
	// surfaces with pushConstants.color -- the real r_wallcolor/r_floorcolor
	// value computed by VK_WorldFlatColorForSurface -- instead of the
	// per-vertex baked texture-average color it otherwise falls back to for
	// surfaces whose real texture just isn't ready yet on Vulkan.
	float drawflatColor;
	// Textured/lightmapped-pipeline equivalent of GLC/GLM's applyColorTinting():
	// r_drawflat_mode 1 (tinted) multiplies the real texture by these colors,
	// mode 2 (bright) replaces it with a luminance-preserving recolor, gated
	// per-fragment by the EZQ_SURFACE_IS_FLOOR bit carried in vbo_world_vert_t's
	// flags field (see the new location-3/4 "inFlags" vertex attribute in
	// vk_world_textured.vert/vk_world_lightmapped.vert). Unlike vk_world_flat's
	// drawflatColor (mode 0, whole surface replaced with a solid fill, routed
	// through a completely separate pipeline), this keeps the real texture
	// visible -- matching GLC/GLM, where tinted/bright surfaces never leave the
	// normal textured draw path.
	float floorColor[4];
	float wallColor[4];
	// 0 = off, 1 = tinted, 2 = bright -- mirrors r_drawflat_mode.integer.
	float drawflatMode;
	// Independently gate floor vs wall tinting, matching GLC/GLM's
	// DRAW_FLATFLOORS/DRAW_FLATWALLS (r_drawflat 1 = both, 2 = floors only,
	// 3 = walls only) -- these are NOT gated by drawflatMode/r_drawflat_mode,
	// only by r_drawflat itself, same as the GLSL side.
	float tintFloors;
	float tintWalls;
	// vk_world_flat.vert/.frag end their matching GLSL struct with a trailing
	// vec3, which the std430-style push-constant layout rules align to 16
	// bytes -- bumping that shader's real compiled block to 140 bytes even
	// though every *other* field here is a plain scalar/array with no such
	// jump. Padded to the next 16-byte multiple so this one shared C struct
	// covers all 5 world pipelines that reuse it; validation correctly flags
	// an undersized block for world_flat otherwise. 176 bytes total, above the
	// Vulkan-guaranteed minimum of 128 but comfortably within the 256 typical
	// desktop AMD/NVIDIA/Intel drivers expose -- same portability caveat as
	// the rest of this struct (desktop-only branch; do not carry to Android).
	// This trailing slot used to be pure padding; the textured/lightmapped/
	// alpha_textured shaders never read a drawflatColor-shaped field here, so
	// it's repurposed as causticsEnabled (GLC/GLM's gl_caustics, applied only
	// to EZQ_SURFACE_UNDERWATER fragments) without growing the struct.
	float causticsEnabled;
} vk_world_push_t;

// Must match the push_constant block in vk_world_normals.vert /
// vk_world_normals.frag exactly. mat4 + vec4 + 2 floats = 88 bytes, well
// inside the 128-byte guaranteed minimum (unlike vk_world_push_t, which is
// desktop-only at 176) -- this one is portable as-is.
typedef struct vk_world_normals_push_s {
	float mvp[16];
	float cameraPosition[4];
	float surfaceType;
	float zFar;
} vk_world_normals_push_t;

static VkPipelineLayout worldFlatPipelineLayout;
static VkPipeline worldFlatPipeline;
static VkDescriptorSetLayout worldFlatSkyDescriptorSetLayout;
static VkDescriptorPool worldFlatSkyDescriptorPool;
// One set per frame-in-flight: this set's bindings get rewritten every time
// the sky is drawn (textures can change underneath via r_skyname/skywind),
// and a single shared set would mean rewriting one that's still bound to a
// previous frame's command buffer still executing on the GPU -- the same
// pending-descriptor-set hazard fixed for per-texture descriptors in
// vk_texture.c, but here it fired every frame instead of only on a cvar
// change. Each frame index is exclusively owned by the CPU until that
// frame's command buffer is submitted, so rewriting frameIndex's slot is
// safe the moment VK_BeginFrame's fence wait for that slot has returned.
static VkDescriptorSet worldFlatSkyDescriptorSets[VK_MAX_FRAMES_IN_FLIGHT];
static VkPipelineLayout worldTexturedPipelineLayout;
static VkPipeline worldTexturedPipeline;
static VkPipelineLayout worldOverlayPipelineLayout;
static VkPipeline worldLumaPipeline;
static VkPipeline worldFullbrightPipeline;
static VkPipelineLayout worldLightmappedPipelineLayout;
static VkPipeline worldLightmappedPipeline;
static VkPipelineLayout worldAlphaTexturedPipelineLayout;
static VkPipeline worldAlphaTexturedPipeline;
// gl_outline & 2 world-outline normals prepass. Deliberately NOT sharing
// vk_world_push_t: this pipeline has no descriptor sets at all and its
// shaders declare their own much smaller block (see vk_world_normals.vert),
// so reusing the 176-byte world block would just push 160 bytes of garbage
// the shader never reads.
static VkPipelineLayout worldNormalsPipelineLayout;
static VkPipeline worldNormalsPipeline;
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

	if (surf->flags & SURF_DRAWFLAT_FLOOR) {
		color[0] = (float)r_floorcolor.color[0] / 255.0f;
		color[1] = (float)r_floorcolor.color[1] / 255.0f;
		color[2] = (float)r_floorcolor.color[2] / 255.0f;
		return;
	}
	if (r_drawflat.integer) {
		color[0] = (float)r_wallcolor.color[0] / 255.0f;
		color[1] = (float)r_wallcolor.color[1] / 255.0f;
		color[2] = (float)r_wallcolor.color[2] / 255.0f;
		return;
	}

	// Fallback path only (texture not ready yet on Vulkan while r_drawflat is
	// off): the flat pipeline is still bound so *something* has to go into
	// this push constant, even though the fragment shader actually paints
	// this case with the per-vertex baked texture-average color instead.
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
	VkDescriptorSetLayout layouts[VK_MAX_FRAMES_IN_FLIGHT];
	int i;

	if (worldFlatSkyDescriptorSets[0] != VK_NULL_HANDLE) {
		return true;
	}
	if (!VK_WorldEnsureFlatSkyDescriptorSetLayout()) {
		return false;
	}
	if (worldFlatSkyDescriptorPool == VK_NULL_HANDLE) {
		VK_InitialiseStructure(poolSize);
		poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSize.descriptorCount = VK_WORLD_SKY_TEXTURE_COUNT * VK_MAX_FRAMES_IN_FLIGHT;

		VK_InitialiseStructure(poolInfo);
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		poolInfo.maxSets = VK_MAX_FRAMES_IN_FLIGHT;

		if (vkCreateDescriptorPool(vk_options.logicalDevice, &poolInfo, NULL, &worldFlatSkyDescriptorPool) != VK_SUCCESS) {
			return false;
		}
	}

	for (i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; ++i) {
		layouts[i] = worldFlatSkyDescriptorSetLayout;
	}

	VK_InitialiseStructure(allocInfo);
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = worldFlatSkyDescriptorPool;
	allocInfo.descriptorSetCount = VK_MAX_FRAMES_IN_FLIGHT;
	allocInfo.pSetLayouts = layouts;

	return vkAllocateDescriptorSets(vk_options.logicalDevice, &allocInfo, worldFlatSkyDescriptorSets) == VK_SUCCESS;
}

static qbool VK_WorldFlatSkyDescriptorSet(VkDescriptorSet* descriptorSet)
{
	static const int skytexorder[MAX_SKYBOXTEXTURES] = { 0, 2, 1, 3, 4, 5 };
	VkDescriptorImageInfo imageInfos[VK_WORLD_SKY_TEXTURE_COUNT];
	VkWriteDescriptorSet descriptorWrites[VK_WORLD_SKY_TEXTURE_COUNT];
	texture_ref textures[VK_WORLD_SKY_TEXTURE_COUNT];
	int i;
	uint32_t frameIndex = vk_options.frame.currentFrame;

	if (frameIndex >= VK_MAX_FRAMES_IN_FLIGHT || !descriptorSet || !VK_WorldEnsureFlatSkyDescriptorSet()) {
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
		descriptorWrites[i].dstSet = worldFlatSkyDescriptorSets[frameIndex];
		descriptorWrites[i].dstBinding = i;
		descriptorWrites[i].descriptorCount = 1;
		descriptorWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[i].pImageInfo = &imageInfos[i];
	}

	vkUpdateDescriptorSets(vk_options.logicalDevice, VK_WORLD_SKY_TEXTURE_COUNT, descriptorWrites, 0, NULL);
	*descriptorSet = worldFlatSkyDescriptorSets[frameIndex];
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

// GLC/GLM's gl_caustics: an animated multiplicative overlay applied only to
// EZQ_SURFACE_UNDERWATER fragments (see vk_world_textured.frag/vk_world_
// lightmapped.frag/vk_world_alpha_textured.frag). r_refdef2.drawCaustics
// (cl_view.c) already gates on both gl_caustics and underwatertexture being
// loaded; VK_TextureReady additionally confirms the GPU-side upload landed.
static qbool VK_WorldCausticsTextureReady(void)
{
	return r_refdef2.drawCaustics && VK_TextureReady(underwatertexture);
}

static VkDescriptorSet VK_WorldCausticsDescriptorSet(void)
{
	texture_ref texture = VK_WorldCausticsTextureReady() ? underwatertexture : solidwhite_texture;

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
	draw->drawflatCvar = drawflat;
	draw->surfaceType = VK_WorldSurfaceType(surf);
	draw->textured = !drawflat && VK_TextureReady(texture);
	draw->lightmapped = draw->textured && !blended && VK_TextureReady(lightmap);
	draw->blended = blended;
	draw->detail = model->isworldmodel && !(surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB));
	// GLM gates caustics purely on the per-vertex EZQ_SURFACE_UNDERWATER flag
	// (draw_world.fragment.glsl), not on a per-model/per-surface CPU decision
	// beyond "this is a normal textured surface" -- the fragment shader does
	// the real filtering. Excluding sky/turb here just avoids wasting the
	// uniform on pipelines that don't carry inFlags meaningfully for it.
	draw->caustics = draw->textured && !(surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB));
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
	VkVertexInputAttributeDescription attributeDescriptions[3];
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
	VkDescriptorSetLayout setLayouts[2];
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

	VK_InitialiseStructure(attributeDescriptions[2]);
	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[2].offset = VK_VBO_FIELDOFFSET(vbo_world_vert_t, lightmap_coords);

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
	multisampling.rasterizationSamples = vk_options.msaaSamples ? vk_options.msaaSamples : VK_SAMPLE_COUNT_1_BIT;

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

	setLayouts[0] = worldFlatSkyDescriptorSetLayout;
	setLayouts[1] = VK_TextureDescriptorSetLayout();
	if (setLayouts[1] == VK_NULL_HANDLE) {
		vkDestroyShaderModule(vk_options.logicalDevice, fragShaderModule, NULL);
		vkDestroyShaderModule(vk_options.logicalDevice, vertShaderModule, NULL);
		return false;
	}

	VK_InitialiseStructure(pipelineLayoutInfo);
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 2;
	pipelineLayoutInfo.pSetLayouts = setLayouts;
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

	if (vkCreateGraphicsPipelines(vk_options.logicalDevice, vk_options.pipelineCache, 1, &pipelineInfo, NULL, &worldFlatPipeline) != VK_SUCCESS) {
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

	if (worldTexturedPipeline != VK_NULL_HANDLE) {
		return true;
	}

	descriptorSetLayout = VK_TextureDescriptorSetLayout();
	if (descriptorSetLayout == VK_NULL_HANDLE) {
		return false;
	}
	descriptorSetLayouts[0] = descriptorSetLayout;
	descriptorSetLayouts[1] = descriptorSetLayout;
	descriptorSetLayouts[2] = descriptorSetLayout;

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

	VK_InitialiseStructure(attributeDescriptions[3]);
	attributeDescriptions[3].binding = 0;
	attributeDescriptions[3].location = 3;
	attributeDescriptions[3].format = VK_FORMAT_R32_UINT;
	attributeDescriptions[3].offset = VK_VBO_FIELDOFFSET(vbo_world_vert_t, flags);

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
	multisampling.rasterizationSamples = vk_options.msaaSamples ? vk_options.msaaSamples : VK_SAMPLE_COUNT_1_BIT;

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

	if (vkCreateGraphicsPipelines(vk_options.logicalDevice, vk_options.pipelineCache, 1, &pipelineInfo, NULL, &worldTexturedPipeline) != VK_SUCCESS) {
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

	// vk_world_textured_vert (reused here for the fullbright/luma overlay
	// pass) gained an "inFlags" location-3 input when r_drawflat_mode
	// tinted/bright support was added to it -- this pipeline shares that
	// same shader module but its vertex input state wasn't updated to
	// match, leaving vkCreateGraphicsPipelines to build a pipeline with an
	// unbound input at that location. Some AMD/RADV driver versions
	// silently mis-render instead of failing pipeline creation outright,
	// which is the likely root cause of a reported bug where world/alias
	// models render black/translucent and item models lose their texture
	// entirely under Vulkan.
	VK_InitialiseStructure(attributeDescriptions[3]);
	attributeDescriptions[3].binding = 0;
	attributeDescriptions[3].location = 3;
	attributeDescriptions[3].format = VK_FORMAT_R32_UINT;
	attributeDescriptions[3].offset = VK_VBO_FIELDOFFSET(vbo_world_vert_t, flags);

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
	multisampling.rasterizationSamples = vk_options.msaaSamples ? vk_options.msaaSamples : VK_SAMPLE_COUNT_1_BIT;

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

	if (vkCreateGraphicsPipelines(vk_options.logicalDevice, vk_options.pipelineCache, 1, &pipelineInfo, NULL, pipeline) != VK_SUCCESS) {
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

	if (worldAlphaTexturedPipeline != VK_NULL_HANDLE) {
		return true;
	}

	descriptorSetLayout = VK_TextureDescriptorSetLayout();
	if (descriptorSetLayout == VK_NULL_HANDLE) {
		return false;
	}
	descriptorSetLayouts[0] = descriptorSetLayout;
	descriptorSetLayouts[1] = descriptorSetLayout;
	descriptorSetLayouts[2] = descriptorSetLayout;

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

	VK_InitialiseStructure(attributeDescriptions[3]);
	attributeDescriptions[3].binding = 0;
	attributeDescriptions[3].location = 3;
	attributeDescriptions[3].format = VK_FORMAT_R32_UINT;
	attributeDescriptions[3].offset = VK_VBO_FIELDOFFSET(vbo_world_vert_t, flags);

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
	multisampling.rasterizationSamples = vk_options.msaaSamples ? vk_options.msaaSamples : VK_SAMPLE_COUNT_1_BIT;

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

	if (vkCreateGraphicsPipelines(vk_options.logicalDevice, vk_options.pipelineCache, 1, &pipelineInfo, NULL, &worldAlphaTexturedPipeline) != VK_SUCCESS) {
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
	VkVertexInputAttributeDescription attributeDescriptions[5];
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
	VkDescriptorSetLayout descriptorSetLayouts[4];

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
	descriptorSetLayouts[3] = descriptorSetLayout;

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

	VK_InitialiseStructure(attributeDescriptions[4]);
	attributeDescriptions[4].binding = 0;
	attributeDescriptions[4].location = 4;
	attributeDescriptions[4].format = VK_FORMAT_R32_UINT;
	attributeDescriptions[4].offset = VK_VBO_FIELDOFFSET(vbo_world_vert_t, flags);

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
	multisampling.rasterizationSamples = vk_options.msaaSamples ? vk_options.msaaSamples : VK_SAMPLE_COUNT_1_BIT;

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

	if (vkCreateGraphicsPipelines(vk_options.logicalDevice, vk_options.pipelineCache, 1, &pipelineInfo, NULL, &worldLightmappedPipeline) != VK_SUCCESS) {
		worldLightmappedPipeline = VK_NULL_HANDLE;
	}

	vkDestroyShaderModule(vk_options.logicalDevice, fragShaderModule, NULL);
	vkDestroyShaderModule(vk_options.logicalDevice, vertShaderModule, NULL);
	return worldLightmappedPipeline != VK_NULL_HANDLE;
}

// Minimal position-only pipeline for the gl_outline & 2 normals prepass.
// Belongs to vk_renderpass_worldnormals (always single-sample, its own
// depth), NOT the main render pass -- hence the hardcoded
// VK_SAMPLE_COUNT_1_BIT instead of vk_options.msaaSamples that every other
// pipeline in this file uses. No descriptor sets: the fragment shader only
// needs the interpolated world position and the push block.
static qbool VK_WorldCreateNormalsPipeline(void)
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
	VkPipelineColorBlendAttachmentState colorBlendAttachment;
	VkPipelineColorBlendStateCreateInfo colorBlending;
	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState;
	VkPushConstantRange pushConstantRange;
	VkPipelineLayoutCreateInfo pipelineLayoutInfo;
	VkGraphicsPipelineCreateInfo pipelineInfo;
	VkRenderPass renderPass;

	if (worldNormalsPipeline != VK_NULL_HANDLE) {
		return true;
	}

	renderPass = VK_WorldNormalsRenderPass();
	if (renderPass == VK_NULL_HANDLE) {
		return false;
	}

	vertShaderModule = VK_WorldCreateShaderModule(vk_world_normals_vert_spv, vk_world_normals_vert_spv_len);
	fragShaderModule = VK_WorldCreateShaderModule(vk_world_normals_frag_spv, vk_world_normals_frag_spv_len);
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

	// Same vertex buffer and stride as every other world pipeline -- only the
	// position attribute is bound, so the normals pass can reuse the exact
	// buffers/indices the main loop already uploaded without a second copy.
	VK_InitialiseStructure(bindingDescription);
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(vbo_world_vert_t);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VK_InitialiseStructure(attributeDescription);
	attributeDescription.binding = 0;
	attributeDescription.location = 0;
	attributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescription.offset = VK_VBO_FIELDOFFSET(vbo_world_vert_t, position);

	VK_InitialiseStructure(vertexInputInfo);
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = 1;
	vertexInputInfo.pVertexAttributeDescriptions = &attributeDescription;

	// Same topology/restart as the main world pipelines -- the index buffer
	// being reused is built with UINT32_MAX restart separators.
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
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	// No depth bias here (unlike the main world pipelines): polygonOffset only
	// exists to stop brush-model entities z-fighting the world surface they're
	// flush against in the *visible* image. In the normals buffer a 1-pixel
	// z-fight between two surfaces with near-identical normals produces no
	// edge either way, so the dynamic state is simply not needed.
	rasterizer.depthBiasEnable = VK_FALSE;

	VK_InitialiseStructure(multisampling);
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VK_InitialiseStructure(depthStencil);
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = glConfig.reversed_depth ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;

	VK_InitialiseStructure(colorBlendAttachment);
	colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VK_InitialiseStructure(colorBlending);
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	VK_InitialiseStructure(dynamicState);
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
	dynamicState.pDynamicStates = dynamicStates;

	if (worldNormalsPipelineLayout == VK_NULL_HANDLE) {
		VK_InitialiseStructure(pushConstantRange);
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(vk_world_normals_push_t);

		VK_InitialiseStructure(pipelineLayoutInfo);
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 0;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		if (vkCreatePipelineLayout(vk_options.logicalDevice, &pipelineLayoutInfo, NULL, &worldNormalsPipelineLayout) != VK_SUCCESS) {
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
	pipelineInfo.layout = worldNormalsPipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(vk_options.logicalDevice, vk_options.pipelineCache, 1, &pipelineInfo, NULL, &worldNormalsPipeline) != VK_SUCCESS) {
		worldNormalsPipeline = VK_NULL_HANDLE;
	}

	vkDestroyShaderModule(vk_options.logicalDevice, fragShaderModule, NULL);
	vkDestroyShaderModule(vk_options.logicalDevice, vertShaderModule, NULL);
	return worldNormalsPipeline != VK_NULL_HANDLE;
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
			memset(worldFlatSkyDescriptorSets, 0, sizeof(worldFlatSkyDescriptorSets));
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
		if (worldNormalsPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(vk_options.logicalDevice, worldNormalsPipeline, NULL);
			worldNormalsPipeline = VK_NULL_HANDLE;
		}
		if (worldNormalsPipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(vk_options.logicalDevice, worldNormalsPipelineLayout, NULL);
			worldNormalsPipelineLayout = VK_NULL_HANDLE;
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

	// r_drawflat_mode only selects a color-blend *style* on GLC/GLM (0=normal
	// solid replace, 1=tinted multiply, 2=bright luminance-multiply -- see
	// applyColorTinting() in glc_world_textured.fragment.glsl) -- it must not
	// gate whether drawflat activates at all. The old `mode == 0` guard here
	// meant any config with r_drawflat_mode 1 or 2 (e.g. a real user's
	// racat.cfg, mode 2/"bright") silently disabled drawflat under Vulkan
	// while GL still rendered flat-colored floors/walls, which is exactly the
	// "world looks different between GL and Vulkan" symptom reported by a
	// user comparing the same config across renderers. The Vulkan drawflat
	// pipeline (vk_world_flat.frag) only has a solid-color path (no texture
	// sampler bound), so mode 1/2 currently render identically to mode 0
	// here -- tracked as a known gap, not implemented, since neither FTEQW
	// nor vkQuake implement a textured tinted/bright drawflat variant either
	// (see research notes from this investigation).
	drawFlatFloors = (r_drawflat.integer == 2 || r_drawflat.integer == 1) && clmodel->isworldmodel;
	drawFlatWalls = (r_drawflat.integer == 3 || r_drawflat.integer == 1) && clmodel->isworldmodel;

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
	// Unlike GLC (which ORs this per-model flag into its per-surface
	// SURF_UNDERWATER check), GLM ignores it and gates caustics purely on the
	// per-vertex EZQ_SURFACE_UNDERWATER flag baked into the surface itself
	// (draw_world.fragment.glsl). We follow GLM here -- see draw->caustics in
	// VK_WorldQueueSurface -- since GLM is the shader-based reference this
	// port is closest to architecturally. The parameter stays in the
	// signature to match renderer_api_t's shared dispatch shape across all
	// three backends.
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

// Returns true if it issued its own pipeline/descriptor-set binds (unconditional,
// not cached), so the caller's world bind cache can be invalidated accordingly.
static qbool VK_WorldDrawOverlay(VkCommandBuffer commandBuffer, const vk_world_draw_t* draw, const vk_world_push_t* push, qbool lumaPipelineReady, qbool fullbrightPipelineReady)
{
	VkDescriptorSet descriptorSets[2];
	VkPipeline pipeline;

	if (!draw || draw->overlayMode == VK_WORLD_OVERLAY_NONE || !VK_TextureReady(draw->overlayTexture)) {
		return false;
	}
	if (draw->overlayMode == VK_WORLD_OVERLAY_LUMA) {
		if (!lumaPipelineReady) {
			return false;
		}
		pipeline = worldLumaPipeline;
	}
	else {
		if (!fullbrightPipelineReady) {
			return false;
		}
		pipeline = worldFullbrightPipeline;
	}

	descriptorSets[0] = VK_TextureDescriptorSet(draw->overlayTexture);
	descriptorSets[1] = VK_WorldDetailDescriptorSet();
	if (descriptorSets[0] == VK_NULL_HANDLE || descriptorSets[1] == VK_NULL_HANDLE) {
		return false;
	}

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, worldOverlayPipelineLayout, 0, 2, descriptorSets, 0, NULL);
	vkCmdPushConstants(commandBuffer, worldOverlayPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(*push), push);
	vkCmdDrawIndexed(commandBuffer, draw->indexCount, 1, draw->firstIndex, 0, 0);
	return true;
}

// Returns true (and binds) only when the requested pipeline/descriptor-set
// state differs from what's already bound on commandBuffer; skips the redundant
// vkCmdBind* calls otherwise. *lastPipeline/lastSets/*lastSetCount are the
// caller's running "currently bound" cache, updated in place.
static void VK_WorldBindIfChanged(VkCommandBuffer commandBuffer, VkPipeline pipeline, VkPipelineLayout layout,
	const VkDescriptorSet* descriptorSets, int descriptorSetCount,
	VkPipeline* lastPipeline, VkDescriptorSet* lastSets, int* lastSetCount)
{
	qbool pipelineChanged = (pipeline != *lastPipeline);
	qbool setsChanged = (descriptorSetCount != *lastSetCount) ||
		(memcmp(lastSets, descriptorSets, descriptorSetCount * sizeof(VkDescriptorSet)) != 0);

	if (pipelineChanged) {
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		*lastPipeline = pipeline;
	}
	// A pipeline change invalidates descriptor-set compatibility guarantees even
	// when the sets themselves are unchanged, so force a rebind in that case too.
	if (pipelineChanged || setsChanged) {
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, descriptorSetCount, descriptorSets, 0, NULL);
		memcpy(lastSets, descriptorSets, descriptorSetCount * sizeof(VkDescriptorSet));
		*lastSetCount = descriptorSetCount;
	}
}

// Reopens the main render pass mid-frame, after the normals prepass below has
// ended it. Mirrors VK_BeginFrame's own framebuffer selection exactly (the
// post-process offscreen target when active, the swapchain image otherwise) --
// getting this wrong would draw the rest of the frame into the wrong image.
//
// Uses the LOAD (noclear) variant so the colour the first main-pass instance
// produced survives. Note the depth attachment still CLEARs in *both*
// variants (see VK_RenderPassCreateVariant: depth always clears, matching
// GL_Clear()), which is precisely why this reopen has to happen before any
// world geometry is drawn -- at this point the first main-pass instance
// contained nothing but its own clear, so re-clearing depth loses nothing.
static void VK_WorldBeginMainRenderPassNoClear(VkCommandBuffer commandBuffer)
{
	VkRenderPassBeginInfo renderPassInfo = { 0 };
	VkClearValue clearValues[3] = { { { { 0 } } } };
	uint32_t imageIndex = vk_options.frame.imageIndex;

	clearValues[1].depthStencil.depth = glConfig.reversed_depth ? 0.0f : 1.0f;
	clearValues[1].depthStencil.stencil = 0;

	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = VK_FrameRenderPass(false);
	renderPassInfo.framebuffer = vk_options.swapChain.postProcessActive ?
		VK_PostProcessFramebuffer(imageIndex) : vk_options.swapChain.framebuffers[imageIndex];
	if (renderPassInfo.framebuffer == VK_NULL_HANDLE) {
		renderPassInfo.framebuffer = vk_options.swapChain.framebuffers[imageIndex];
	}
	renderPassInfo.renderArea.offset.x = 0;
	renderPassInfo.renderArea.offset.y = 0;
	renderPassInfo.renderArea.extent = vk_options.swapChain.imageSize;
	renderPassInfo.clearValueCount = sizeof(clearValues) / sizeof(clearValues[0]);
	renderPassInfo.pClearValues = clearValues;

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

// gl_outline & 2 normals prepass. Called from VK_RenderView with the main
// render pass ALREADY ENDED by the caller, and leaves it re-begun on return
// (so the caller's existing draw loop continues unchanged).
//
// Why a separate geometry pass at all, when GLM gets this for free: GLM's
// GL_FramebufferStartWorldNormals attaches a second colour target and its
// world shaders write normals via MRT in the same draw. Replicating that here
// would mean giving all five world pipelines a second colour attachment plus
// a matching main-render-pass variant for every MSAA/post-process
// combination. Redrawing position-only geometry into a tiny dedicated pass is
// far less invasive and touches none of the existing pipelines -- at the cost
// of a second pass over the world's vertices, which is why the whole thing is
// gated on VK_WorldOutlineActive().
//
// Returns false if anything was unavailable, in which case the main render
// pass has NOT been disturbed and the caller should just proceed normally.
static qbool VK_DrawWorldNormalsPass(VkCommandBuffer commandBuffer, VkBuffer vertexBuffer, VkBuffer indexBuffer)
{
	VkRenderPassBeginInfo renderPassInfo = { 0 };
	VkClearValue clearValues[2] = { { { { 0 } } } };
	VkFramebuffer framebuffer;
	VkDeviceSize vertexOffset = 0;
	float lastModelView[16];
	float lastMvp[16];
	qbool haveLastMvp = false;
	float zFar;
	int i;

	if (!VK_WorldCreateNormalsPipeline()) {
		return false;
	}

	framebuffer = VK_WorldNormalsFramebuffer(vk_options.frame.imageIndex);
	if (framebuffer == VK_NULL_HANDLE) {
		return false;
	}

	zFar = bound(R_MINIMUM_FARCLIP, r_farclip.value, R_MAXIMUM_FARCLIP);

	// Colour clears to all-zero: the outline shader treats alpha == 0 as
	// "nothing drawn here" and skips those pixels entirely, so this is what
	// makes sky/empty regions produce no spurious edges.
	clearValues[1].depthStencil.depth = glConfig.reversed_depth ? 0.0f : 1.0f;
	clearValues[1].depthStencil.stencil = 0;

	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = VK_WorldNormalsRenderPass();
	renderPassInfo.framebuffer = framebuffer;
	renderPassInfo.renderArea.offset.x = 0;
	renderPassInfo.renderArea.offset.y = 0;
	renderPassInfo.renderArea.extent = vk_options.swapChain.imageSize;
	renderPassInfo.clearValueCount = sizeof(clearValues) / sizeof(clearValues[0]);
	renderPassInfo.pClearValues = clearValues;

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	VK_WorldSetViewportScissor(commandBuffer);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, worldNormalsPipeline);
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &vertexOffset);
	vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

	for (i = 0; i < worldDrawCount; ++i) {
		vk_world_normals_push_t push;

		// Opaque geometry only -- matches GLM, where the normals MRT target is
		// only bound for GLM_DrawWorldModelBatch(opaque_world) and the
		// alpha_surfaces batch is drawn later, after the outline is already
		// composited. Water/alpha surfaces contributing normals here would
		// outline things the player sees through.
		if (worldDraws[i].blended) {
			continue;
		}

		memset(&push, 0, sizeof(push));
		// Same redundant-recompute skip as the main loop's MVP cache.
		if (haveLastMvp && memcmp(lastModelView, worldDraws[i].modelView, sizeof(lastModelView)) == 0) {
			memcpy(push.mvp, lastMvp, sizeof(push.mvp));
		}
		else {
			R_MultiplyMatrix(worldDraws[i].modelView, R_ProjectionMatrix(), push.mvp);
			memcpy(lastModelView, worldDraws[i].modelView, sizeof(lastModelView));
			memcpy(lastMvp, push.mvp, sizeof(lastMvp));
			haveLastMvp = true;
		}
		push.cameraPosition[0] = r_refdef.vieworg[0];
		push.cameraPosition[1] = r_refdef.vieworg[1];
		push.cameraPosition[2] = r_refdef.vieworg[2];
		push.cameraPosition[3] = 0.0f;
		push.surfaceType = worldDraws[i].surfaceType;
		push.zFar = zFar;

		vkCmdPushConstants(commandBuffer, worldNormalsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
		vkCmdDrawIndexed(commandBuffer, worldDraws[i].indexCount, 1, worldDraws[i].firstIndex, 0, 0);
	}

	vkCmdEndRenderPass(commandBuffer);

	// No-op today (the render pass's finalLayout already leaves the colour
	// attachment in SHADER_READ_ONLY_OPTIMAL), kept as the named seam for
	// that guarantee -- see vk_draw.c.
	VK_WorldNormalsTransitionForSampling(commandBuffer, vk_options.frame.imageIndex);

	return true;
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
	// The overwhelming majority of draws are static world geometry sharing
	// the same view matrix (only brush-model entities like doors/plats have
	// their own); caching the last modelView this multiplied against skips
	// R_MultiplyMatrix's 4x4 multiply-add for every draw that repeats it,
	// without changing what's drawn or how -- purely a redundant-recompute
	// skip, not a batching/merging change.
	float lastMultipliedModelView[16];
	float lastMvp[16];
	qbool haveLastMvp = false;
	// Consecutive draws are usually pre-sorted by material (same texture/
	// lightmap/pipeline), so re-issuing vkCmdBindPipeline/vkCmdBindDescriptorSets
	// for a state that's already bound is pure redundant driver overhead --
	// same "skip the recompute/rebind when nothing changed" idea as the MVP
	// cache above, just applied to bind state instead of the push constant.
	// Only tracks what each branch below actually binds (pipeline + its
	// descriptor sets); doesn't touch draw ordering, geometry, or call count.
	VkPipeline lastBoundPipeline = VK_NULL_HANDLE;
	VkDescriptorSet lastBoundDescriptorSets[3] = { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE };
	int lastBoundDescriptorSetCount = 0;
	qbool worldOutline = false;

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

	// gl_outline & 2 (world outline). The normals prepass needs its own render
	// pass, and Vulkan render passes cannot nest -- so the main render pass
	// VK_BeginFrame opened has to be closed and reopened around it. This is
	// the earliest point in the frame where worldDraws[] is fully populated
	// (R_DrawWorld/R_DrawEntities already ran, feeding VK_WorldQueueModel) AND
	// nothing has been drawn into the main pass yet beyond its own clear --
	// which is what makes closing it here lossless, since the LOAD variant
	// used to reopen it still re-clears depth. Doing this any later (e.g. after
	// the opaque loop) would throw away the world's depth buffer.
	//
	// Everything is inside the same command buffer, sequentially; no extra
	// barrier is needed beyond the normals render pass's own attachment
	// finalLayout, and render-pass boundaries themselves guarantee the
	// write-then-sample ordering the composite depends on.
	if (VK_WorldOutlineActive()) {
		vkCmdEndRenderPass(commandBuffer);
		worldOutline = VK_DrawWorldNormalsPass(commandBuffer, vertexBuffer, indexBuffer);
		VK_WorldBeginMainRenderPassNoClear(commandBuffer);
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
			// Composite the outline here, between the opaque world batch and
			// everything that follows -- the same slot GLM_RenderView uses
			// (GLM_DrawWorldOutlines right after
			// GLM_DrawWorldModelBatch(opaque_world), before alias models,
			// sprites and the alpha_surfaces batch). Compositing any earlier
			// would just be painted over by the opaque geometry; any later
			// would draw outlines on top of models and water that GL does not.
			if (worldOutline) {
				VK_WorldOutlineComposite(commandBuffer, vk_options.frame.imageIndex);
				// The composite bound its own fullscreen pipeline/descriptor
				// set and reset the viewport; invalidate the world bind cache
				// and restore state, same as the alias-model/sprite path below.
				lastBoundPipeline = VK_NULL_HANDLE;
				lastBoundDescriptorSetCount = 0;
			}
			VK_RenderAliasModels(false);
			VK_Draw3DSprites();
			VK_WorldSetViewportScissor(commandBuffer);
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &vertexOffset);
			vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
			// Alias models/sprites above issued their own pipeline/descriptor-set
			// binds on this same command buffer; the world's bind cache no longer
			// reflects reality, so drop it rather than risk a stale skip.
			lastBoundPipeline = VK_NULL_HANDLE;
			lastBoundDescriptorSetCount = 0;
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
			if (haveLastMvp && memcmp(lastMultipliedModelView, worldDraws[i].modelView, sizeof(lastMultipliedModelView)) == 0) {
				memcpy(push.mvp, lastMvp, sizeof(push.mvp));
			}
			else {
				R_MultiplyMatrix(worldDraws[i].modelView, R_ProjectionMatrix(), push.mvp);
				memcpy(lastMultipliedModelView, worldDraws[i].modelView, sizeof(lastMultipliedModelView));
				memcpy(lastMvp, push.mvp, sizeof(lastMvp));
				haveLastMvp = true;
			}
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
			push.causticsEnabled = (worldDraws[i].caustics && VK_WorldCausticsTextureReady()) ? 1.0f : 0.0f;
			// Matches Modern OpenGL's DRAW_TEXTURELESS: keep the normal
			// lit/lightmapped pipeline (so depth shading, outlines, detail
			// textures etc. are unaffected), just force the diffuse texture
			// sample to a single fixed texel in the fragment shader instead
			// of the surface's real UVs.
			push.textureless = gl_textureless.integer ? 1.0f : 0.0f;
			// Tinted/bright r_drawflat_mode (1/2) for the textured/lightmapped
			// pipelines -- mode 0 ("normal", solid replace) is already handled
			// entirely by the separate vk_world_flat pipeline via drawflatColor
			// above and must stay untouched here. See vk_world_textured.frag /
			// vk_world_lightmapped.frag for how these are applied per-fragment,
			// gated by the surface's EZQ_SURFACE_IS_FLOOR bit (inFlags).
			if (r_drawflat.integer && r_drawflat_mode.integer) {
				push.drawflatMode = (float)r_drawflat_mode.integer;
				push.tintFloors = (r_drawflat.integer == 1 || r_drawflat.integer == 2) ? 1.0f : 0.0f;
				push.tintWalls = (r_drawflat.integer == 1 || r_drawflat.integer == 3) ? 1.0f : 0.0f;
				push.floorColor[0] = (float)r_floorcolor.color[0] / 255.0f;
				push.floorColor[1] = (float)r_floorcolor.color[1] / 255.0f;
				push.floorColor[2] = (float)r_floorcolor.color[2] / 255.0f;
				push.floorColor[3] = 1.0f;
				push.wallColor[0] = (float)r_wallcolor.color[0] / 255.0f;
				push.wallColor[1] = (float)r_wallcolor.color[1] / 255.0f;
				push.wallColor[2] = (float)r_wallcolor.color[2] / 255.0f;
				push.wallColor[3] = 1.0f;
			}
			if (worldDraws[i].surfaceType == TEXTURE_TURB_SKY) {
				if (VK_WorldSkyboxTexturesReady()) {
					push.useSkyTexture = VK_WORLD_SKY_MODE_SKYBOX;
				}
				else if (VK_WorldSkyTexturesReady()) {
					push.useSkyTexture = VK_WORLD_SKY_MODE_CLASSIC;
				}
			}

			if (drawBlended) {
				VkDescriptorSet descriptorSets[3];

				descriptorSets[0] = VK_TextureDescriptorSet(worldDraws[i].texture);
				descriptorSets[1] = VK_WorldDetailDescriptorSet();
				descriptorSets[2] = VK_WorldCausticsDescriptorSet();
				if (descriptorSets[0] != VK_NULL_HANDLE && descriptorSets[1] != VK_NULL_HANDLE && descriptorSets[2] != VK_NULL_HANDLE) {
					float blendConstants[4] = { 0.0f, 0.0f, 0.0f, worldDraws[i].alpha };

					layout = worldAlphaTexturedPipelineLayout;
					VK_WorldBindIfChanged(commandBuffer, worldAlphaTexturedPipeline, layout, descriptorSets, 3,
						&lastBoundPipeline, lastBoundDescriptorSets, &lastBoundDescriptorSetCount);
					vkCmdSetBlendConstants(commandBuffer, blendConstants);
					drawTextured = false;
				}
				else {
					drawBlended = false;
				}
			}
			if (!drawBlended && drawLightmapped) {
				VkDescriptorSet descriptorSets[4];

				descriptorSets[0] = VK_TextureDescriptorSet(worldDraws[i].texture);
				descriptorSets[1] = VK_TextureDescriptorSet(worldDraws[i].lightmap);
				descriptorSets[2] = VK_WorldDetailDescriptorSet();
				descriptorSets[3] = VK_WorldCausticsDescriptorSet();
				if (descriptorSets[0] != VK_NULL_HANDLE && descriptorSets[1] != VK_NULL_HANDLE && descriptorSets[2] != VK_NULL_HANDLE && descriptorSets[3] != VK_NULL_HANDLE) {
					layout = worldLightmappedPipelineLayout;
					VK_WorldBindIfChanged(commandBuffer, worldLightmappedPipeline, layout, descriptorSets, 4,
						&lastBoundPipeline, lastBoundDescriptorSets, &lastBoundDescriptorSetCount);
					drawTextured = false;
				}
				else {
					drawLightmapped = false;
				}
			}
			if (!drawBlended && !drawLightmapped && drawTextured) {
				VkDescriptorSet descriptorSets[3];

				descriptorSets[0] = VK_TextureDescriptorSet(worldDraws[i].texture);
				descriptorSets[1] = VK_WorldDetailDescriptorSet();
				descriptorSets[2] = VK_WorldCausticsDescriptorSet();
				if (descriptorSets[0] != VK_NULL_HANDLE && descriptorSets[1] != VK_NULL_HANDLE && descriptorSets[2] != VK_NULL_HANDLE) {
					layout = worldTexturedPipelineLayout;
					VK_WorldBindIfChanged(commandBuffer, worldTexturedPipeline, layout, descriptorSets, 3,
						&lastBoundPipeline, lastBoundDescriptorSets, &lastBoundDescriptorSetCount);
				}
				else {
					drawTextured = false;
				}
			}
			if (!drawBlended && !drawLightmapped && !drawTextured) {
				VkDescriptorSet descriptorSets[2];
				texture_ref lightmapTex = VK_TextureReady(worldDraws[i].lightmap) ? worldDraws[i].lightmap : solidwhite_texture;

				// This C field is causticsEnabled everywhere else, but
				// vk_world_flat's own GLSL block still names the same byte
				// offset drawflatColor -- see vk_world_push_t's comment.
				push.causticsEnabled = worldDraws[i].drawflatCvar ? 1.0f : 0.0f;
				descriptorSets[1] = VK_TextureDescriptorSet(lightmapTex);
				if (VK_WorldFlatSkyDescriptorSet(&descriptorSets[0]) && descriptorSets[1] != VK_NULL_HANDLE) {
					VK_WorldBindIfChanged(commandBuffer, worldFlatPipeline, worldFlatPipelineLayout, descriptorSets, 2,
						&lastBoundPipeline, lastBoundDescriptorSets, &lastBoundDescriptorSetCount);
				}
				else if (worldFlatPipeline != lastBoundPipeline) {
					vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, worldFlatPipeline);
					lastBoundPipeline = worldFlatPipeline;
					lastBoundDescriptorSetCount = 0;
				}
			}

			if (!!worldDraws[i].polygonOffset != depthBiasActive) {
				depthBiasActive = worldDraws[i].polygonOffset;
				// Pushes brush-model entities (doors, plats, buttons, ...) embedded flush
				// against world geometry slightly toward the camera so they win the depth
				// test deterministically instead of z-fighting/flickering against the
				// static world surface they're attached to. These values target a
				// D32_SFLOAT depth buffer and match gl_brush_polygonoffset on GLC/GLM.
				vkCmdSetDepthBias(commandBuffer, depthBiasActive ? -4.0f : 0.0f, 0.0f, depthBiasActive ? -0.125f : 0.0f);
			}

			vkCmdPushConstants(commandBuffer, layout, pushStages, 0, sizeof(push), &push);
			vkCmdDrawIndexed(commandBuffer, worldDraws[i].indexCount, 1, worldDraws[i].firstIndex, 0, 0);
			if (VK_WorldDrawOverlay(commandBuffer, &worldDraws[i], &push, lumaPipelineReady, fullbrightPipelineReady)) {
				lastBoundPipeline = VK_NULL_HANDLE;
				lastBoundDescriptorSetCount = 0;
			}
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

}

#endif // RENDERER_OPTION_VULKAN
