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

#include "r_renderer.h"
#include "r_texture_internal.h"
#include "vk_local.h"
#include "image.h"

typedef struct vk_texture_s {
	VkImage image;
	VkDeviceMemory memory;
	VkImageView imageView;
	VkSampler modeSampler;
	VkSampler forcedNearestSampler;
	VkDescriptorSet descriptorSet;
	VkImageLayout layout;
	byte* pixels;
	size_t pixelsSize;
	int width;
	int height;
	int mode;
	qbool clamp;
	texture_minification_id minFilter;
	texture_magnification_id magFilter;
	float anisotropy;
	int mipLevels;
} vk_texture_t;

static vk_texture_t textureData[MAX_GLTEXTURES];
static VkDescriptorSetLayout textureDescriptorSetLayout;
static VkDescriptorPool textureDescriptorPool;
static texture_ref boundTextures[16];

#define VK_MAX_PENDING_TEXTURE_UPLOADS 1024
typedef struct vk_pending_texture_upload_s {
	texture_ref texture;
	VkDeviceSize dataOffset;
	int offsetx;
	int offsety;
	int width;
	int height;
	int mipLevel;
} vk_pending_texture_upload_t;

static vk_pending_texture_upload_t pendingTextureUploads[VK_MAX_PENDING_TEXTURE_UPLOADS];
static int pendingTextureUploadCount;
static byte* pendingTextureUploadData;
static size_t pendingTextureUploadDataSize;
static size_t pendingTextureUploadDataCapacity;
static VkBuffer frameUploadBuffers[VK_MAX_FRAMES_IN_FLIGHT];
static VkDeviceMemory frameUploadMemories[VK_MAX_FRAMES_IN_FLIGHT];
static VkDeviceSize frameUploadCapacities[VK_MAX_FRAMES_IN_FLIGHT];
static void* frameUploadMapped[VK_MAX_FRAMES_IN_FLIGHT];

static qbool VK_TextureReferenceInRange(texture_ref texture);

// A texture load can be triggered by gameplay in the MIDDLE of a frame that is
// already recording commands (vk_options.frame.active): e.g. picking up an item
// lazily loads its HUD ammo icon / model skin during SCR_UpdateScreen, after the
// world/HUD draws of this same frame already recorded vkCmdBindDescriptorSets
// referencing existing texture descriptor sets. VK_UploadTexture destroys and
// reallocates the texture's image + descriptor set (VK_TextureDestroyObjects ->
// vkFreeDescriptorSets). vkDeviceWaitIdle only drains work already SUBMITTED to
// the GPU; it cannot undo a bind already recorded into the current frame's
// command buffer that has not been submitted yet. Freeing a descriptor set that
// is still referenced by a recording command buffer invalidates that command
// buffer for the rest of the frame (validation: "VkCommandBuffer ... is now in
// an invalid state ... VkDescriptorSet was destroyed or updated without
// UPDATE_AFTER_BIND"), so every subsequent draw is dropped -- observed as alias
// models rendering black/untextured. To avoid it we defer the whole upload to
// the next VK_BeginFrame, applied at a clean frame boundary before any command
// buffer starts recording. Map-load uploads (frame not active) are unaffected
// and still run immediately.
typedef struct vk_deferred_texture_upload_s {
	texture_ref texture;
	int mode;
	int width;
	int height;
	byte* data;      // owned copy, freed after the deferred upload runs
	size_t dataSize;
} vk_deferred_texture_upload_t;

#define VK_MAX_DEFERRED_TEXTURE_UPLOADS 256
static vk_deferred_texture_upload_t deferredTextureUploads[VK_MAX_DEFERRED_TEXTURE_UPLOADS];
static int deferredTextureUploadCount;

// Same in-flight hazard as the deferred uploads above, but for the texture state
// setters (VK_TextureSetFiltering / VK_TextureSetAnisotropy / VK_TextureWrapModeClamp).
// Those change filtering/anisotropy/clamp on an EXISTING texture, whose descriptor
// set may already be recorded (vkCmdBindDescriptorSets) into the current frame's
// command buffer. VK_TextureUpdateDescriptor runs vkUpdateDescriptorSets on it,
// which is the "updated ... without UPDATE_AFTER_BIND" half of the validation
// error (the uploads above are the "destroyed" half) -- it invalidates the
// recording command buffer for the rest of the frame, dropping every later draw
// (observed as ground weapon/item models rendering black from the very first
// frame after entering a map, e.g. dm3). vkDeviceWaitIdle only drains SUBMITTED
// work; it cannot undo a bind already recorded but not yet submitted. So when a
// frame is active we only stash the new CPU-side state on the texture (harmless)
// and defer the actual descriptor refresh to the next VK_BeginFrame boundary,
// before any command buffer starts recording. Off-frame calls (map load,
// vid_restart) still refresh immediately.
#define VK_MAX_DEFERRED_DESCRIPTOR_REFRESHES 1024
static texture_ref deferredDescriptorRefreshes[VK_MAX_DEFERRED_DESCRIPTOR_REFRESHES];
static int deferredDescriptorRefreshCount;

// The "destroyed" half of the same hazard, on a DIFFERENT path than the deferred
// uploads above: R_TextureAllocateSlot (r_texture.c) calls R_DeleteTexture ->
// VK_TextureDelete -> VK_TextureDestroyObjects -> vkFreeDescriptorSets directly,
// SYNCHRONOUSLY, whenever gameplay reloads a texture "over" an existing slot at a
// different size (player-skin translation on connect/skin change, HUD/ammo icon
// reload after an item pickup, weapon-model skin swap). That runs from CPU
// gameplay/HUD code during SCR_UpdateScreen, mid-frame, AFTER this frame's world/
// HUD draws already recorded vkCmdBindDescriptorSets against the old descriptor
// set. Unlike VK_UploadTexture, this slot-delete path is NOT gated on
// frame.active and was never deferred -- it is the third free/update site that
// escaped fixes #1 (deferred uploads) and #2 (deferred setter refreshes), so the
// "VkDescriptorSet was destroyed or updated without UPDATE_AFTER_BIND" validation
// error and its command-buffer-invalidation cascade still fired right after an
// item pickup. vkDeviceWaitIdle (which VK_TextureDestroyObjects already does)
// only drains SUBMITTED work; it cannot undo a bind recorded into the current,
// not-yet-submitted command buffer. So when a frame is active we capture just the
// descriptor set handle here and free it at the next VK_BeginFrame boundary,
// before any command buffer records. The image/view/memory teardown (guarded by
// vkDeviceWaitIdle) stays immediate -- only descriptor set free/update trips the
// UPDATE_AFTER_BIND validation and corrupts the recording command buffer.
#define VK_MAX_DEFERRED_DESCRIPTOR_FREES 1024
static VkDescriptorSet deferredDescriptorFrees[VK_MAX_DEFERRED_DESCRIPTOR_FREES];
static int deferredDescriptorFreeCount;

static void VK_UploadTextureImmediate(texture_ref texture, int mode, int width, int height, byte* data);

static void VK_TextureDestroyFrameUploadBuffer(uint32_t frameIndex)
{
	if (frameIndex >= VK_MAX_FRAMES_IN_FLIGHT) return;
	if (vk_options.logicalDevice != VK_NULL_HANDLE) {
		if (frameUploadMapped[frameIndex] && frameUploadMemories[frameIndex] != VK_NULL_HANDLE) vkUnmapMemory(vk_options.logicalDevice, frameUploadMemories[frameIndex]);
		if (frameUploadBuffers[frameIndex] != VK_NULL_HANDLE) vkDestroyBuffer(vk_options.logicalDevice, frameUploadBuffers[frameIndex], NULL);
		if (frameUploadMemories[frameIndex] != VK_NULL_HANDLE) vkFreeMemory(vk_options.logicalDevice, frameUploadMemories[frameIndex], NULL);
	}
	frameUploadBuffers[frameIndex] = VK_NULL_HANDLE;
	frameUploadMemories[frameIndex] = VK_NULL_HANDLE;
	frameUploadCapacities[frameIndex] = 0;
	frameUploadMapped[frameIndex] = NULL;
}

static void VK_TextureDestroyFrameUploadResources(void)
{
	uint32_t i;
	for (i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; ++i) VK_TextureDestroyFrameUploadBuffer(i);
	Q_free(pendingTextureUploadData);
	pendingTextureUploadData = NULL;
	pendingTextureUploadDataSize = 0;
	pendingTextureUploadDataCapacity = 0;
	pendingTextureUploadCount = 0;
}

static void VK_TextureDiscardDeferredUploads(void)
{
	int i;
	for (i = 0; i < deferredTextureUploadCount; ++i) {
		Q_free(deferredTextureUploads[i].data);
		deferredTextureUploads[i].data = NULL;
	}
	deferredTextureUploadCount = 0;
	deferredDescriptorRefreshCount = 0;
	// Queued handles belong to textureDescriptorPool, which every caller of this
	// (vid_restart full-reset, shutdown) destroys wholesale right after -- that
	// frees the sets, so just drop the queue rather than free individually.
	deferredDescriptorFreeCount = 0;
}

static qbool VK_TextureUpdateDescriptor(texture_ref texture);

// Called by the texture state setters while a frame is recording: record the
// request so the descriptor gets refreshed at the next frame boundary instead
// of mid-recording. Coalesces duplicates (same texture set repeatedly in one
// frame) and falls back to an immediate refresh only if the (large) queue fills.
static void VK_TextureQueueDeferredDescriptorRefresh(texture_ref texture)
{
	int i;

	for (i = 0; i < deferredDescriptorRefreshCount; ++i) {
		if (deferredDescriptorRefreshes[i].index == texture.index) {
			return;
		}
	}
	if (deferredDescriptorRefreshCount >= VK_MAX_DEFERRED_DESCRIPTOR_REFRESHES) {
		// Pathological (a single frame reconfigured >1024 distinct textures).
		// An immediate refresh may still trip the hazard for this one texture,
		// but that is strictly better than never applying the state change.
		VK_TextureUpdateDescriptor(texture);
		return;
	}
	deferredDescriptorRefreshes[deferredDescriptorRefreshCount++] = texture;
}

static qbool VK_TextureReferenceInRange(texture_ref texture)
{
	return texture.index > 0 && texture.index < MAX_GLTEXTURES;
}

// Shared by VK_UploadTexture and VK_TextureReplaceSubImageRGBA: queues raw
// pixel data for upload on the next VK_TextureFlushPendingUploads call instead
// of an immediate per-texture submit+vkQueueWaitIdle. Returns false if the
// batch is full so callers can fall back to an immediate upload.
static qbool VK_TextureQueuePendingUpload(texture_ref texture, int offsetx, int offsety, int width, int height, int mipLevel, const byte* buffer)
{
	VkDeviceSize imageSize = (VkDeviceSize)width * height * 4;
	vk_pending_texture_upload_t* upload;
	size_t requiredSize;

	if (pendingTextureUploadCount >= VK_MAX_PENDING_TEXTURE_UPLOADS) {
		return false;
	}

	upload = &pendingTextureUploads[pendingTextureUploadCount++];
	requiredSize = pendingTextureUploadDataSize + (size_t)imageSize;
	if (requiredSize > pendingTextureUploadDataCapacity) {
		size_t newCapacity = max((size_t)(4 * 1024 * 1024), requiredSize * 2);
		pendingTextureUploadData = Q_realloc(pendingTextureUploadData, newCapacity);
		pendingTextureUploadDataCapacity = newCapacity;
	}
	upload->texture = texture;
	upload->dataOffset = pendingTextureUploadDataSize;
	upload->offsetx = offsetx;
	upload->offsety = offsety;
	upload->width = width;
	upload->height = height;
	upload->mipLevel = mipLevel;
	memcpy(pendingTextureUploadData + pendingTextureUploadDataSize, buffer, (size_t)imageSize);
	pendingTextureUploadDataSize = requiredSize;
	return true;
}

// A texture slot can be destroyed/recreated at a different size within the
// same frame it queued a pending upload (e.g. map-load texture churn). Without
// this, VK_TextureFlushPendingUploads would later copy the stale queued
// width/height into the slot's new (possibly smaller) image, tripping
// vkCmdCopyBufferToImage's extent validation and corrupting the command
// buffer for the rest of the frame.
static void VK_TextureInvalidatePendingUploads(texture_ref texture)
{
	int i;

	for (i = 0; i < pendingTextureUploadCount; ++i) {
		if (pendingTextureUploads[i].texture.index == texture.index) {
			pendingTextureUploads[i].texture.index = -1;
		}
	}
}

static void VK_TextureDestroyObjects(texture_ref texture)
{
	vk_texture_t* vktex;

	if (!VK_TextureReferenceInRange(texture) || vk_options.logicalDevice == VK_NULL_HANDLE) {
		return;
	}

	VK_TextureInvalidatePendingUploads(texture);

	vktex = &textureData[texture.index];

	// The GPU may still be reading this texture's image/descriptor from an
	// in-flight command buffer (we run up to VK_MAX_FRAMES_IN_FLIGHT frames
	// ahead of the GPU). Destroying it underneath an in-progress draw is
	// undefined behaviour. Only pay the (otherwise idle, near-free) wait when
	// there's an actual GPU resource to tear down.
	if (vktex->image != VK_NULL_HANDLE) {
		vkDeviceWaitIdle(vk_options.logicalDevice);
	}

	if (vktex->descriptorSet != VK_NULL_HANDLE && textureDescriptorPool != VK_NULL_HANDLE) {
		// Mid-frame (gameplay reloading a skin/HUD icon over an existing slot):
		// this descriptor set may already be bound into the current recording
		// command buffer. Freeing it now would invalidate that command buffer
		// for the rest of the frame (UPDATE_AFTER_BIND validation error +
		// cascade). Defer the free to the next frame boundary instead. The
		// handle is captured here; the memset() below clears vktex->descriptorSet
		// so the slot can be safely reused (reallocating a distinct new set) this
		// same frame without touching the still-queued old one.
		if (vk_options.frame.active && deferredDescriptorFreeCount < VK_MAX_DEFERRED_DESCRIPTOR_FREES) {
			deferredDescriptorFrees[deferredDescriptorFreeCount++] = vktex->descriptorSet;
		}
		else {
			// Off-frame (map load, vid_restart, shutdown) or the queue is full:
			// safe/necessary to free immediately. vkDeviceWaitIdle above already
			// drained any submitted frame still reading this set.
			vkFreeDescriptorSets(vk_options.logicalDevice, textureDescriptorPool, 1, &vktex->descriptorSet);
		}
	}
	// modeSampler/forcedNearestSampler are borrowed references into the shared
	// samplerCache (see VK_TextureCachedSampler) -- the cache owns them, not
	// this texture, so they're cleared (by the memset below) but not
	// destroyed here. Destroyed once at full shutdown by
	// VK_TextureSamplerCacheShutdown instead.
	if (vktex->imageView != VK_NULL_HANDLE) {
		vkDestroyImageView(vk_options.logicalDevice, vktex->imageView, NULL);
	}
	if (vktex->image != VK_NULL_HANDLE) {
		vkDestroyImage(vk_options.logicalDevice, vktex->image, NULL);
	}
	if (vktex->memory != VK_NULL_HANDLE) {
		vkFreeMemory(vk_options.logicalDevice, vktex->memory, NULL);
	}
	Q_free(vktex->pixels);
	memset(vktex, 0, sizeof(*vktex));
}

static qbool VK_TextureEnsureInfrastructure(void)
{
	VkDescriptorSetLayoutBinding samplerLayoutBinding;
	VkDescriptorSetLayoutCreateInfo layoutInfo;
	VkDescriptorPoolSize poolSize;
	VkDescriptorPoolCreateInfo poolInfo;

	if (textureDescriptorSetLayout != VK_NULL_HANDLE && textureDescriptorPool != VK_NULL_HANDLE) {
		return true;
	}

	if (textureDescriptorSetLayout == VK_NULL_HANDLE) {
		VK_InitialiseStructure(samplerLayoutBinding);
		samplerLayoutBinding.binding = 0;
		samplerLayoutBinding.descriptorCount = 2;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.pImmutableSamplers = NULL;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VK_InitialiseStructure(layoutInfo);
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &samplerLayoutBinding;

		if (vkCreateDescriptorSetLayout(vk_options.logicalDevice, &layoutInfo, NULL, &textureDescriptorSetLayout) != VK_SUCCESS) {
			return false;
		}
	}

	if (textureDescriptorPool == VK_NULL_HANDLE) {
		VK_InitialiseStructure(poolSize);
		poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSize.descriptorCount = MAX_GLTEXTURES * 2;

		VK_InitialiseStructure(poolInfo);
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		poolInfo.maxSets = MAX_GLTEXTURES;

		if (vkCreateDescriptorPool(vk_options.logicalDevice, &poolInfo, NULL, &textureDescriptorPool) != VK_SUCCESS) {
			return false;
		}
	}

	return true;
}

// Samplers bake filter/clamp/anisotropy in at creation time (can't be edited
// in place), but nearly every texture shares one of a handful of
// combinations -- the same global gl_anisotropy level, one of the 6 GL
// min-filter/mipmap combos, linear or nearest magnification, clamped or
// repeat. Caching by that combination instead of giving every texture its
// own sampler objects means switching anisotropy, gl_texturemode or wrap
// mode is just a lookup, not a destroy+recreate+vkDeviceWaitIdle -- which
// matters because filtering and anisotropy are both applied right after
// every single texture load (R_TextureUtil_SetFiltering), so a per-texture
// recreate would mean a full GPU drain per texture during map load, the
// same class of stall VK_TextureQueuePendingUpload's batching elsewhere was
// written to avoid.
#define VK_SAMPLER_CACHE_ANISOTROPY_LEVELS 17 /* 0-16 */
#define VK_SAMPLER_CACHE_SIZE (2 /* minFilter */ * 2 /* magFilter */ * 2 /* mipmapMode */ * 2 /* clamp */ * 2 /* hasMipmap */ * VK_SAMPLER_CACHE_ANISOTROPY_LEVELS)
static VkSampler samplerCache[VK_SAMPLER_CACHE_SIZE];
static qbool samplerCacheValid[VK_SAMPLER_CACHE_SIZE];

static int VK_SamplerCacheIndex(VkFilter minFilter, VkFilter magFilter, VkSamplerMipmapMode mipmapMode, qbool clamp, qbool hasMipmap, int anisotropy)
{
	int minIdx = (minFilter == VK_FILTER_LINEAR) ? 0 : 1;
	int magIdx = (magFilter == VK_FILTER_LINEAR) ? 0 : 1;
	int mipIdx = (mipmapMode == VK_SAMPLER_MIPMAP_MODE_LINEAR) ? 0 : 1;
	int clampIdx = clamp ? 1 : 0;
	int hasMipmapIdx = hasMipmap ? 1 : 0;
	int anisoIdx = bound(0, anisotropy, VK_SAMPLER_CACHE_ANISOTROPY_LEVELS - 1);

	return ((((minIdx * 2 + magIdx) * 2 + mipIdx) * 2 + clampIdx) * 2 + hasMipmapIdx) * VK_SAMPLER_CACHE_ANISOTROPY_LEVELS + anisoIdx;
}

// hasMipmap distinguishes GL_NEAREST/GL_LINEAR (texture_minification_nearest/
// _linear -- no "_mipmap_" suffix) from the other 4 gl_texturemode values:
// on GL those two sample only mip 0, giving the pixel-perfect/software-like
// look at any distance, everything else blends across the full mip chain.
// mipmapMode alone can't express that in Vulkan -- it only picks how to
// interpolate *between* mips once sampling has decided to use more than one,
// it doesn't disable the chain -- so hasMipmap=false additionally clamps
// maxLod to 0.25f (the standard "round down to mip 0, never reach mip 1"
// trick) instead of the usual VK_LOD_CLAMP_NONE.
static VkSampler VK_TextureCachedSampler(VkFilter minFilter, VkFilter magFilter, VkSamplerMipmapMode mipmapMode, qbool clamp, qbool hasMipmap, int anisotropy)
{
	int index = VK_SamplerCacheIndex(minFilter, magFilter, mipmapMode, clamp, hasMipmap, anisotropy);

	if (!samplerCacheValid[index]) {
		VkSamplerCreateInfo samplerInfo;

		VK_InitialiseStructure(samplerInfo);
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = magFilter;
		samplerInfo.minFilter = minFilter;
		samplerInfo.addressModeU = clamp ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = samplerInfo.addressModeU;
		samplerInfo.addressModeW = samplerInfo.addressModeU;
		samplerInfo.anisotropyEnable = (anisotropy > 1 && vk_options.physicalDeviceFeatures.samplerAnisotropy) ? VK_TRUE : VK_FALSE;
		samplerInfo.maxAnisotropy = samplerInfo.anisotropyEnable
			? min((float)anisotropy, vk_options.physicalDeviceProperties.limits.maxSamplerAnisotropy)
			: 1.0f;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = mipmapMode;
		samplerInfo.minLod = 0.0f;
		if (hasMipmap) {
			// VK_LOD_CLAMP_NONE instead of a fixed value: this sampler is shared
			// across every texture with this (filter, clamp, anisotropy) combo,
			// and they don't all have the same mip count. Each texture's own
			// image view subresourceRange.levelCount (set from vktex->mipLevels)
			// is what actually limits which levels are sampled; the sampler just
			// needs to not clamp below that.
			samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
		}
		else {
			samplerInfo.maxLod = 0.25f;
		}

		if (vkCreateSampler(vk_options.logicalDevice, &samplerInfo, NULL, &samplerCache[index]) != VK_SUCCESS) {
			return VK_NULL_HANDLE;
		}
		samplerCacheValid[index] = true;
	}
	return samplerCache[index];
}

// texture_minification_id packs both the base filter and the inter-mip blend
// mode (GL_*_MIPMAP_* has no Vulkan equivalent enum, hence the split out
// param); texture_magnification_id only ever carries NEAREST or LINEAR, GL
// has no magnification-mipmap combination. hasMipmap is false only for the
// two non-"_mipmap_" GL modes (GL_NEAREST/GL_LINEAR) -- see
// VK_TextureCachedSampler for what that actually changes.
static void VK_FilterFromMinification(texture_minification_id id, VkFilter* filter, VkSamplerMipmapMode* mipmapMode, qbool* hasMipmap)
{
	*hasMipmap = true;
	switch (id) {
		case texture_minification_nearest:
			*filter = VK_FILTER_NEAREST;
			*mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			*hasMipmap = false;
			break;
		case texture_minification_nearest_mipmap_nearest:
			*filter = VK_FILTER_NEAREST;
			*mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			break;
		case texture_minification_nearest_mipmap_linear:
			*filter = VK_FILTER_NEAREST;
			*mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			break;
		case texture_minification_linear_mipmap_nearest:
			*filter = VK_FILTER_LINEAR;
			*mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			break;
		case texture_minification_linear_mipmap_linear:
			*filter = VK_FILTER_LINEAR;
			*mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			break;
		case texture_minification_linear:
			*filter = VK_FILTER_LINEAR;
			*mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			*hasMipmap = false;
			break;
		default:
			*filter = VK_FILTER_LINEAR;
			*mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			break;
	}
}

static VkFilter VK_FilterFromMagnification(texture_magnification_id id)
{
	return (id == texture_magnification_nearest) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
}

static void VK_TextureSamplerCacheShutdown(void)
{
	int i;

	for (i = 0; i < VK_SAMPLER_CACHE_SIZE; ++i) {
		if (samplerCacheValid[i]) {
			vkDestroySampler(vk_options.logicalDevice, samplerCache[i], NULL);
			samplerCache[i] = VK_NULL_HANDLE;
			samplerCacheValid[i] = false;
		}
	}
}

static qbool VK_TextureEnsureSamplers(vk_texture_t* vktex)
{
	VkFilter minFilter, magFilter;
	VkSamplerMipmapMode mipmapMode;
	qbool hasMipmap;

	VK_FilterFromMinification(vktex->minFilter, &minFilter, &mipmapMode, &hasMipmap);
	magFilter = VK_FilterFromMagnification(vktex->magFilter);

	// modeSampler actually reflects this texture's gl_texturemode-driven
	// filter/mipmap settings; forcedNearestSampler is a fixed pixel-perfect
	// override slot some draws (HUD icons, crosshair) explicitly opt into via
	// VK_TextureDescriptorImageInfo's nearest param, independent of
	// gl_texturemode -- see hudTexture[0]/[1] in vk_hud_image.frag. Forced
	// nearest always disables mipmapping too (matches its "pixel-perfect"
	// intent regardless of gl_texturemode's own mip setting).
	vktex->modeSampler = VK_TextureCachedSampler(minFilter, magFilter, mipmapMode, vktex->clamp, hasMipmap, (int)vktex->anisotropy);
	vktex->forcedNearestSampler = VK_TextureCachedSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, vktex->clamp, false, 1);
	if (vktex->modeSampler == VK_NULL_HANDLE || vktex->forcedNearestSampler == VK_NULL_HANDLE) {
		return false;
	}
	return true;
}

static qbool VK_TextureEnsureDescriptor(texture_ref texture)
{
	vk_texture_t* vktex;
	VkDescriptorSetAllocateInfo allocInfo;

	if (!VK_TextureEnsureInfrastructure()) {
		return false;
	}
	vktex = &textureData[texture.index];
	if (vktex->descriptorSet != VK_NULL_HANDLE) {
		return true;
	}

	VK_InitialiseStructure(allocInfo);
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = textureDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &textureDescriptorSetLayout;

	{
		VkResult result = vkAllocateDescriptorSets(vk_options.logicalDevice, &allocInfo, &vktex->descriptorSet);
		if (result != VK_SUCCESS) {
			return false;
		}
	}
	return true;
}

static qbool VK_TextureUpdateDescriptor(texture_ref texture)
{
	vk_texture_t* vktex;
	VkDescriptorImageInfo imageInfos[2];
	VkWriteDescriptorSet descriptorWrite;
	qbool wasAlreadyAllocated;

	if (!VK_TextureReferenceInRange(texture)) {
		return false;
	}
	vktex = &textureData[texture.index];
	if (vktex->imageView == VK_NULL_HANDLE || !VK_TextureEnsureSamplers(vktex)) {
		return false;
	}
	wasAlreadyAllocated = (vktex->descriptorSet != VK_NULL_HANDLE);
	if (!VK_TextureEnsureDescriptor(texture)) {
		return false;
	}

	VK_InitialiseStructure(imageInfos[0]);
	imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfos[0].imageView = vktex->imageView;
	imageInfos[0].sampler = vktex->modeSampler;

	VK_InitialiseStructure(imageInfos[1]);
	imageInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfos[1].imageView = vktex->imageView;
	imageInfos[1].sampler = vktex->forcedNearestSampler;

	VK_InitialiseStructure(descriptorWrite);
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = vktex->descriptorSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = 2;
	descriptorWrite.pImageInfo = imageInfos;

	// A descriptor set that already existed may be bound into a command
	// buffer still executing on the GPU (we run up to VK_MAX_FRAMES_IN_FLIGHT
	// frames ahead of it) -- vkUpdateDescriptorSets on a set in that pending
	// state is undefined per spec without UPDATE_AFTER_BIND, confirmed by
	// validation layers corrupting unrelated in-flight draws (one texture's
	// image showing up on a completely different surface). A set we just
	// allocated above can't be bound to anything yet, so only the
	// reconfigure-an-existing-texture path (filtering/anisotropy/clamp
	// changes) needs to wait; VK_UploadTexture's own path already frees and
	// reallocates the descriptor via VK_TextureDestroyObjects beforehand
	// (which already waits), so this doesn't double up there.
	if (wasAlreadyAllocated) {
		vkDeviceWaitIdle(vk_options.logicalDevice);
	}

	vkUpdateDescriptorSets(vk_options.logicalDevice, 1, &descriptorWrite, 0, NULL);
	return true;
}

static void VK_TextureRecordTransitionBarrier(VkCommandBuffer commandBuffer, vk_texture_t* vktex, VkImageLayout newLayout)
{
	VkImageMemoryBarrier barrier;
	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (vktex->image == VK_NULL_HANDLE || vktex->layout == newLayout) {
		return;
	}

	VK_InitialiseStructure(barrier);
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = vktex->layout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = vktex->image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = max(1, vktex->mipLevels);
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	if (vktex->layout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (vktex->layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = 0;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	}

	vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, NULL, 0, NULL, 1, &barrier);
	vktex->layout = newLayout;
}

// Combines transition+copy+transition into a single immediate command buffer
// submit/wait instead of three: VK_EndImmediateCommands does a full
// vkQueueWaitIdle, so calling it 3x per texture (as the old TransitionLayout/
// CopyBufferToImage call sequence did) meant 3 full GPU drains per texture --
// for a dm3-scale map's ~420 world textures that is ~1260 drains during load.
static void VK_TextureUploadBufferToImageImmediate(VkBuffer buffer, vk_texture_t* vktex, const VkBufferImageCopy* regions, uint32_t regionCount)
{
	VkCommandBuffer commandBuffer;

	commandBuffer = VK_BeginImmediateCommands();
	if (commandBuffer == VK_NULL_HANDLE) {
		return;
	}

	VK_TextureRecordTransitionBarrier(commandBuffer, vktex, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	vkCmdCopyBufferToImage(commandBuffer, buffer, vktex->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regionCount, regions);
	VK_TextureRecordTransitionBarrier(commandBuffer, vktex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	VK_EndImmediateCommands(commandBuffer);
}

static qbool VK_TextureCreateImageView(texture_ref texture)
{
	vk_texture_t* vktex;
	VkImageViewCreateInfo viewInfo;

	vktex = &textureData[texture.index];
	VK_InitialiseStructure(viewInfo);
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = vktex->image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = max(1, vktex->mipLevels);
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	return vkCreateImageView(vk_options.logicalDevice, &viewInfo, NULL, &vktex->imageView) == VK_SUCCESS;
}

void VK_AllocateTextureNames(gltexture_t* glt)
{
	if (glt && glt->reference.index) {
		glt->texnum = glt->reference.index;
	}
}

#define VK_MAX_TEXTURE_MIPS 16

typedef struct vk_mip_level_s {
	int width;
	int height;
	size_t offset;
	size_t size;
} vk_mip_level_t;

// Mirrors vkQuake's approach (TexMgr_LoadImage32): derive the mip count,
// downsample into one contiguous staging buffer, then a single multi-region
// vkCmdCopyBufferToImage with two layout transitions total -- instead of a
// vkCmdBlitImage chain with barriers between every level. Built on this
// codebase's own Image_MipReduce downsampler (already used for picmip/
// max-size scaling in r_texture_load.c) rather than adding vkQuake's
// stb_image_resize dependency for something we already have an equivalent
// of. Terminates when both dimensions reach 1 (matching Image_MipReduce's
// own clamp-at-1 behaviour), not vkQuake's let-a-dimension-hit-0 loop.
static int VK_BuildMipPyramid(const byte* baseData, int baseWidth, int baseHeight, byte* outBuffer, vk_mip_level_t* levels)
{
	int numLevels = 1;

	levels[0].width = baseWidth;
	levels[0].height = baseHeight;
	levels[0].offset = 0;
	levels[0].size = (size_t)baseWidth * baseHeight * 4;
	memcpy(outBuffer, baseData, levels[0].size);

	while ((levels[numLevels - 1].width > 1 || levels[numLevels - 1].height > 1) && numLevels < VK_MAX_TEXTURE_MIPS) {
		int width = levels[numLevels - 1].width;
		int height = levels[numLevels - 1].height;
		size_t offset = levels[numLevels - 1].offset + levels[numLevels - 1].size;

		memcpy(outBuffer + offset, outBuffer + levels[numLevels - 1].offset, levels[numLevels - 1].size);
		Image_MipReduce(outBuffer + offset, outBuffer + offset, &width, &height, 4);

		levels[numLevels].width = width;
		levels[numLevels].height = height;
		levels[numLevels].offset = offset;
		levels[numLevels].size = (size_t)width * height * 4;
		++numLevels;
	}
	return numLevels;
}

static void VK_UploadTextureImmediate(texture_ref texture, int mode, int width, int height, byte* data)
{
	vk_texture_t* vktex;
	VkDeviceSize imageSize;
	qbool wantsMipmap;
	vk_mip_level_t mipLevelInfo[VK_MAX_TEXTURE_MIPS];
	int numMipLevels;
	byte* pyramid;
	size_t pyramidSize;
	int i;

	if (!VK_TextureReferenceInRange(texture) || vk_options.logicalDevice == VK_NULL_HANDLE || width <= 0 || height <= 0) {
		return;
	}

	imageSize = (VkDeviceSize)width * height * 4;
	VK_TextureDestroyObjects(texture);
	vktex = &textureData[texture.index];
	vktex->width = width;
	vktex->height = height;
	vktex->mode = mode;
	vktex->layout = VK_IMAGE_LAYOUT_UNDEFINED;
	vktex->minFilter = texture_minification_linear;
	vktex->magFilter = texture_magnification_linear;
	vktex->pixelsSize = (size_t)imageSize;
	vktex->pixels = Q_malloc(vktex->pixelsSize);
	if (data) {
		memcpy(vktex->pixels, data, vktex->pixelsSize);
	}
	else {
		memset(vktex->pixels, 0, vktex->pixelsSize);
	}

	// vktex->pixels stays base-level-only (VK_TextureGet/screenshot readback
	// depend on that); the mip pyramid is a separate, short-lived buffer.
	wantsMipmap = (mode & TEX_MIPMAP) ? true : false;
	numMipLevels = 1;
	pyramid = NULL;
	pyramidSize = (size_t)imageSize;

	if (wantsMipmap) {
		// Geometric series of halvings sums to < 4/3 of the base level; 2x is
		// generous headroom without bothering to derive the exact total.
		pyramid = Q_malloc((size_t)imageSize * 2);
		numMipLevels = VK_BuildMipPyramid(vktex->pixels, width, height, pyramid, mipLevelInfo);
		pyramidSize = mipLevelInfo[numMipLevels - 1].offset + mipLevelInfo[numMipLevels - 1].size;
	}
	vktex->mipLevels = numMipLevels;

	if (!VK_CreateImageResource(width, height, numMipLevels, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vktex->image, &vktex->memory)) {
		Q_free(pyramid);
		VK_TextureDestroyObjects(texture);
		return;
	}
	if (!VK_TextureCreateImageView(texture)) {
		Q_free(pyramid);
		VK_TextureDestroyObjects(texture);
		return;
	}
	if (!VK_TextureUpdateDescriptor(texture)) {
		Q_free(pyramid);
		VK_TextureDestroyObjects(texture);
		return;
	}

	if (!wantsMipmap) {
		// Queue the GPU upload through the same shared per-frame batch the
		// dynamic lightmap path uses, instead of an immediate submit+wait per
		// texture: bulk map loads create hundreds of world textures back to
		// back, and that used to mean one full GPU drain each. Falls back to
		// a single immediate upload only if the batch capacity is exhausted.
		if (!VK_TextureQueuePendingUpload(texture, 0, 0, width, height, 0, vktex->pixels)) {
			VkBuffer stagingBuffer = VK_NULL_HANDLE;
			VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
			void* mapped;
			VkBufferImageCopy region;

			if (!VK_CreateBufferResource(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingMemory)) {
				return;
			}
			if (vkMapMemory(vk_options.logicalDevice, stagingMemory, 0, imageSize, 0, &mapped) == VK_SUCCESS) {
				memcpy(mapped, vktex->pixels, (size_t)imageSize);
				vkUnmapMemory(vk_options.logicalDevice, stagingMemory);
			}
			VK_InitialiseStructure(region);
			region.bufferOffset = 0;
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = 0;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;
			region.imageExtent.width = width;
			region.imageExtent.height = height;
			region.imageExtent.depth = 1;
			VK_TextureUploadBufferToImageImmediate(stagingBuffer, vktex, &region, 1);
			vkDestroyBuffer(vk_options.logicalDevice, stagingBuffer, NULL);
			vkFreeMemory(vk_options.logicalDevice, stagingMemory, NULL);
		}
	}
	else {
		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
		void* mapped;
		VkBufferImageCopy regions[VK_MAX_TEXTURE_MIPS];

		if (VK_CreateBufferResource(pyramidSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingMemory)) {
			if (vkMapMemory(vk_options.logicalDevice, stagingMemory, 0, pyramidSize, 0, &mapped) == VK_SUCCESS) {
				memcpy(mapped, pyramid, pyramidSize);
				vkUnmapMemory(vk_options.logicalDevice, stagingMemory);
			}
			for (i = 0; i < numMipLevels; ++i) {
				VK_InitialiseStructure(regions[i]);
				regions[i].bufferOffset = mipLevelInfo[i].offset;
				regions[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				regions[i].imageSubresource.mipLevel = i;
				regions[i].imageSubresource.baseArrayLayer = 0;
				regions[i].imageSubresource.layerCount = 1;
				regions[i].imageExtent.width = mipLevelInfo[i].width;
				regions[i].imageExtent.height = mipLevelInfo[i].height;
				regions[i].imageExtent.depth = 1;
			}
			VK_TextureUploadBufferToImageImmediate(stagingBuffer, vktex, regions, numMipLevels);
			vkDestroyBuffer(vk_options.logicalDevice, stagingBuffer, NULL);
			vkFreeMemory(vk_options.logicalDevice, stagingMemory, NULL);
		}
		Q_free(pyramid);
	}

	gltextures[texture.index].texnum = texture.index;
}

// If an earlier deferred upload for this same texture is still queued, drop it:
// this newer call supersedes it, and letting the stale copy run later would
// overwrite the fresh contents (and leak its data buffer).
static void VK_TextureDropDeferredUpload(texture_ref texture)
{
	int i;

	for (i = 0; i < deferredTextureUploadCount; ++i) {
		if (deferredTextureUploads[i].texture.index == texture.index) {
			Q_free(deferredTextureUploads[i].data);
			deferredTextureUploads[i] = deferredTextureUploads[--deferredTextureUploadCount];
			--i;
		}
	}
}

void VK_UploadTexture(texture_ref texture, int mode, int width, int height, byte* data)
{
	vk_deferred_texture_upload_t* deferred;
	size_t dataSize;

	if (!VK_TextureReferenceInRange(texture) || vk_options.logicalDevice == VK_NULL_HANDLE || width <= 0 || height <= 0) {
		return;
	}

	// Not mid-frame (map load, init, vid_restart): safe to tear down and
	// recreate the image/descriptor right now -- no command buffer is
	// recording, so nothing can be holding a stale bind.
	if (!vk_options.frame.active) {
		VK_TextureDropDeferredUpload(texture);
		VK_UploadTextureImmediate(texture, mode, width, height, data);
		return;
	}

	// Mid-frame: capture the request (with a private copy of the pixels, since
	// the caller frees its buffer right after returning) and apply it at the
	// next frame boundary in VK_TextureApplyDeferredUploads.
	VK_TextureDropDeferredUpload(texture);
	if (deferredTextureUploadCount >= VK_MAX_DEFERRED_TEXTURE_UPLOADS) {
		// Queue full (pathological). Falling back to an immediate upload here
		// is the lesser evil: it may still trip the in-flight-descriptor hazard
		// for this one texture, but dropping the upload entirely would leave the
		// texture permanently wrong. In practice a single frame never loads
		// hundreds of new textures once past map load.
		VK_UploadTextureImmediate(texture, mode, width, height, data);
		return;
	}

	dataSize = (size_t)width * height * 4;
	deferred = &deferredTextureUploads[deferredTextureUploadCount++];
	deferred->texture = texture;
	deferred->mode = mode;
	deferred->width = width;
	deferred->height = height;
	deferred->dataSize = dataSize;
	deferred->data = Q_malloc(dataSize);
	if (data) {
		memcpy(deferred->data, data, dataSize);
	}
	else {
		memset(deferred->data, 0, dataSize);
	}
}

// Called from VK_BeginFrame before the frame's command buffer starts recording,
// so the destroy/recreate of image + descriptor set below happens at a point
// where no command buffer references the old descriptor set. VK_UploadTextureImmediate
// still runs its own vkDeviceWaitIdle (via VK_TextureDestroyObjects) to cover
// any previously-submitted frame that is still executing on the GPU.
void VK_TextureApplyDeferredUploads(void)
{
	int i;

	// Descriptor set frees deferred from mid-frame slot deletes
	// (VK_TextureDestroyObjects via R_TextureAllocateSlot). Freed first, at the
	// frame boundary before any command buffer records and before the uploads
	// below allocate new sets, so no recording command buffer still references a
	// set being freed, and the pool capacity is reclaimed ahead of new allocs.
	if (deferredDescriptorFreeCount > 0 && textureDescriptorPool != VK_NULL_HANDLE) {
		vkFreeDescriptorSets(vk_options.logicalDevice, textureDescriptorPool, deferredDescriptorFreeCount, deferredDescriptorFrees);
	}
	deferredDescriptorFreeCount = 0;

	// Uploads first: VK_UploadTextureImmediate fully reallocates the image and
	// descriptor set, so a refresh queued for the same texture the same frame
	// becomes redundant afterwards (but stays harmless -- it just re-writes the
	// same already-correct descriptor).
	for (i = 0; i < deferredTextureUploadCount; ++i) {
		vk_deferred_texture_upload_t* deferred = &deferredTextureUploads[i];
		VK_UploadTextureImmediate(deferred->texture, deferred->mode, deferred->width, deferred->height, deferred->data);
		Q_free(deferred->data);
		deferred->data = NULL;
	}
	deferredTextureUploadCount = 0;

	// Descriptor refreshes deferred from mid-frame texture state setters
	// (filtering/anisotropy/clamp). Applied here at the frame boundary, before
	// this frame's command buffer starts recording, so vkUpdateDescriptorSets
	// can't invalidate an already-bound-and-recording descriptor set.
	for (i = 0; i < deferredDescriptorRefreshCount; ++i) {
		VK_TextureUpdateDescriptor(deferredDescriptorRefreshes[i]);
	}
	deferredDescriptorRefreshCount = 0;
}

VkDescriptorSetLayout VK_TextureDescriptorSetLayout(void)
{
	VK_TextureEnsureInfrastructure();
	return textureDescriptorSetLayout;
}

VkDescriptorSet VK_TextureDescriptorSet(texture_ref texture)
{
	if (!VK_TextureReady(texture)) {
		return VK_NULL_HANDLE;
	}
	return textureData[texture.index].descriptorSet;
}

qbool VK_TextureDescriptorImageInfo(texture_ref texture, qbool nearest, VkDescriptorImageInfo* info)
{
	vk_texture_t* vktex;

	if (!info || !VK_TextureReferenceInRange(texture)) {
		return false;
	}
	vktex = &textureData[texture.index];
	if (vktex->imageView == VK_NULL_HANDLE || !VK_TextureEnsureSamplers(vktex)) {
		return false;
	}

	VK_InitialiseStructure(*info);
	info->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	info->imageView = vktex->imageView;
	info->sampler = nearest ? vktex->forcedNearestSampler : vktex->modeSampler;
	return info->sampler != VK_NULL_HANDLE;
}

qbool VK_TextureReady(texture_ref texture)
{
	if (!VK_TextureReferenceInRange(texture)) {
		return false;
	}
	return textureData[texture.index].image != VK_NULL_HANDLE &&
		textureData[texture.index].imageView != VK_NULL_HANDLE &&
		textureData[texture.index].descriptorSet != VK_NULL_HANDLE;
}

void VK_TextureInitialiseState(void)
{
	int i;

	if (vk_options.logicalDevice == VK_NULL_HANDLE) {
		return;
	}

	for (i = 0; i < MAX_GLTEXTURES; ++i) {
		texture_ref ref;
		ref.index = i;
		VK_TextureDestroyObjects(ref);
	}

	// Drop any queued deferred descriptor frees: their handles belong to the
	// pool destroyed just below (which frees them wholesale), and freeing them
	// individually afterwards would use a dangling pool handle.
	deferredDescriptorFreeCount = 0;

	if (textureDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(vk_options.logicalDevice, textureDescriptorPool, NULL);
		textureDescriptorPool = VK_NULL_HANDLE;
	}
	if (textureDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(vk_options.logicalDevice, textureDescriptorSetLayout, NULL);
		textureDescriptorSetLayout = VK_NULL_HANDLE;
	}
	memset(boundTextures, 0, sizeof(boundTextures));
	VK_TextureEnsureInfrastructure();
}

void VK_TextureShutdown(void)
{
	int i;

	if (vk_options.logicalDevice == VK_NULL_HANDLE) {
		return;
	}
	VK_TextureDestroyFrameUploadResources();
	VK_TextureDiscardDeferredUploads();

	for (i = 0; i < MAX_GLTEXTURES; ++i) {
		texture_ref ref;
		ref.index = i;
		VK_TextureDestroyObjects(ref);
	}

	VK_TextureSamplerCacheShutdown();

	if (textureDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(vk_options.logicalDevice, textureDescriptorPool, NULL);
		textureDescriptorPool = VK_NULL_HANDLE;
	}
	if (textureDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(vk_options.logicalDevice, textureDescriptorSetLayout, NULL);
		textureDescriptorSetLayout = VK_NULL_HANDLE;
	}
	memset(boundTextures, 0, sizeof(boundTextures));
}

void VK_TextureDelete(texture_ref texture)
{
	if (VK_TextureReferenceInRange(texture)) {
		VK_TextureDestroyObjects(texture);
		gltextures[texture.index].texnum = 0;
	}
}

void VK_TextureMipmapGenerate(texture_ref texture)
{
	(void)texture;
}

void VK_TextureWrapModeClamp(texture_ref texture)
{
	vk_texture_t* vktex;

	if (!VK_TextureReferenceInRange(texture)) {
		return;
	}
	vktex = &textureData[texture.index];
	vktex->clamp = true;
	// No destroy needed: VK_TextureEnsureSamplers re-resolves both samplers
	// from samplerCache on every call, including picking up the new clamp
	// mode, since it's keyed by (filter, clamp, anisotropy) rather than
	// owning a sampler outright.
	if (vk_options.frame.active) {
		VK_TextureQueueDeferredDescriptorRefresh(texture);
	}
	else {
		VK_TextureUpdateDescriptor(texture);
	}
}

void VK_TextureLabelSet(texture_ref texture, const char* label)
{
	(void)texture;
	(void)label;
}

qbool VK_TextureUnitBind(int unit, texture_ref texture)
{
	if (unit >= 0 && unit < (int)(sizeof(boundTextures) / sizeof(boundTextures[0]))) {
		boundTextures[unit] = texture;
	}
	return VK_TextureReady(texture);
}

qbool VK_TextureIsUnitBound(int unit, texture_ref texture)
{
	if (unit < 0 || unit >= (int)(sizeof(boundTextures) / sizeof(boundTextures[0]))) {
		return false;
	}
	return R_TextureReferenceEqual(boundTextures[unit], texture);
}

void VK_TextureUnitMultiBind(int first_unit, int num_textures, texture_ref* textures)
{
	int i;

	for (i = 0; i < num_textures; ++i) {
		VK_TextureUnitBind(first_unit + i, textures[i]);
	}
}

void VK_TextureGet(texture_ref texture, int buffer_size, byte* buffer, int bpp)
{
	vk_texture_t* vktex;
	int pixelCount;
	int i;
	int maxPixels;

	if (!buffer || buffer_size <= 0) {
		return;
	}
	memset(buffer, 0, buffer_size);
	if (!VK_TextureReferenceInRange(texture)) {
		return;
	}
	vktex = &textureData[texture.index];
	if (!vktex->pixels) {
		return;
	}

	pixelCount = vktex->width * vktex->height;
	maxPixels = buffer_size / max(1, bpp);
	pixelCount = min(pixelCount, maxPixels);
	if (bpp == 4) {
		memcpy(buffer, vktex->pixels, min(buffer_size, (int)vktex->pixelsSize));
	}
	else if (bpp == 3) {
		for (i = 0; i < pixelCount; ++i) {
			buffer[i * 3 + 0] = vktex->pixels[i * 4 + 0];
			buffer[i * 3 + 1] = vktex->pixels[i * 4 + 1];
			buffer[i * 3 + 2] = vktex->pixels[i * 4 + 2];
		}
	}
}

void VK_TextureCompressionSet(qbool enabled)
{
	(void)enabled;
}

void VK_TextureCreate2D(texture_ref* reference, int width, int height, const char* name, qbool is_lightmap)
{
	gltexture_t* slot;
	byte* blank;

	(void)is_lightmap;
	if (!reference || width <= 0 || height <= 0) {
		return;
	}

	slot = R_NextTextureSlot(texture_type_2d);
	if (!slot) {
		R_TextureReferenceInvalidate(*reference);
		return;
	}
	if (name) {
		strlcpy(slot->identifier, name, sizeof(slot->identifier));
	}
	slot->image_width = slot->texture_width = width;
	slot->image_height = slot->texture_height = height;
	slot->bpp = 4;
	slot->texmode = TEX_ALPHA | TEX_NOSCALE | TEX_NO_TEXTUREMODE;
	slot->storage_allocated = true;
	VK_AllocateTextureNames(slot);

	blank = Q_calloc(width * height, 4);
	VK_UploadTexture(slot->reference, slot->texmode, width, height, blank);
	Q_free(blank);

	*reference = slot->reference;
	VK_TextureSetFiltering(*reference, texture_minification_linear, texture_magnification_linear);
	VK_TextureWrapModeClamp(*reference);
}

void VK_TexturesCreate(r_texture_type_id type, int count, texture_ref* textures)
{
	int i;
	gltexture_t* slot;

	if (!textures || count <= 0) {
		return;
	}
	for (i = 0; i < count; ++i) {
		slot = R_NextTextureSlot(type);
		if (slot) {
			VK_AllocateTextureNames(slot);
			textures[i] = slot->reference;
		}
		else {
			R_TextureReferenceInvalidate(textures[i]);
		}
	}
}

void VK_TextureFlushPendingUploads(VkCommandBuffer commandBuffer, uint32_t frameIndex)
{
	VkDeviceSize requiredCapacity;
	int i;


	if (commandBuffer == VK_NULL_HANDLE || frameIndex >= VK_MAX_FRAMES_IN_FLIGHT || pendingTextureUploadCount == 0 || pendingTextureUploadDataSize == 0) return;

	requiredCapacity = pendingTextureUploadDataSize;
	if (frameUploadCapacities[frameIndex] < requiredCapacity) {
		VkDeviceSize newCapacity = max((VkDeviceSize)(4 * 1024 * 1024), requiredCapacity);
		VK_TextureDestroyFrameUploadBuffer(frameIndex);
		if (!VK_CreateBufferResource(newCapacity, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&frameUploadBuffers[frameIndex], &frameUploadMemories[frameIndex]) ||
			vkMapMemory(vk_options.logicalDevice, frameUploadMemories[frameIndex], 0, newCapacity, 0, &frameUploadMapped[frameIndex]) != VK_SUCCESS) {
			VK_TextureDestroyFrameUploadBuffer(frameIndex);
			return;
		}
		frameUploadCapacities[frameIndex] = newCapacity;
	}

	memcpy(frameUploadMapped[frameIndex], pendingTextureUploadData, pendingTextureUploadDataSize);
	for (i = 0; i < pendingTextureUploadCount; ++i) {
		vk_pending_texture_upload_t* upload = &pendingTextureUploads[i];
		vk_texture_t* vktex;
		VkBufferImageCopy region;

		if (!VK_TextureReady(upload->texture)) continue;
		vktex = &textureData[upload->texture.index];
		// Reads/updates vktex->layout instead of assuming SHADER_READ_ONLY_OPTIMAL,
		// so freshly created textures (real layout UNDEFINED) queued here by
		// VK_UploadTexture get a correct discard transition instead of a bogus
		// "read" barrier on contents that were never written.
		VK_TextureRecordTransitionBarrier(commandBuffer, vktex, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		VK_InitialiseStructure(region);
		region.bufferOffset = upload->dataOffset;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = upload->mipLevel;
		region.imageSubresource.layerCount = 1;
		region.imageOffset.x = upload->offsetx;
		region.imageOffset.y = upload->offsety;
		region.imageExtent.width = upload->width;
		region.imageExtent.height = upload->height;
		region.imageExtent.depth = 1;
		vkCmdCopyBufferToImage(commandBuffer, frameUploadBuffers[frameIndex], vktex->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		VK_TextureRecordTransitionBarrier(commandBuffer, vktex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	pendingTextureUploadCount = 0;
	pendingTextureUploadDataSize = 0;
}

void VK_TextureReplaceSubImageRGBA(texture_ref texture, int offsetx, int offsety, int width, int height, byte* buffer)
{
	vk_texture_t* vktex;
	VkBuffer stagingBuffer = VK_NULL_HANDLE;
	VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
	VkDeviceSize imageSize;
	void* mapped;
	int row;

	if (!VK_TextureReady(texture) || !buffer || width <= 0 || height <= 0) {
		return;
	}
	vktex = &textureData[texture.index];
	if (offsetx < 0 || offsety < 0 || offsetx + width > vktex->width || offsety + height > vktex->height) {
		return;
	}

	for (row = 0; row < height; ++row) {
		memcpy(vktex->pixels + ((offsety + row) * vktex->width + offsetx) * 4, buffer + row * width * 4, width * 4);
	}

	imageSize = (VkDeviceSize)width * height * 4;
	// Queue regardless of whether a frame is currently active: bulk callers
	// outside the render loop (initial lightmap build at map load) rely on
	// this path too, so the whole batch gets flushed through one shared
	// command buffer/wait instead of one immediate submit per surface.
	if (VK_TextureQueuePendingUpload(texture, offsetx, offsety, width, height, 0, buffer)) {
		return;
	}
	if (!VK_CreateBufferResource(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingMemory)) {
		return;
	}
	if (vkMapMemory(vk_options.logicalDevice, stagingMemory, 0, imageSize, 0, &mapped) == VK_SUCCESS) {
		memcpy(mapped, buffer, (size_t)imageSize);
		vkUnmapMemory(vk_options.logicalDevice, stagingMemory);
	}

	{
		VkBufferImageCopy region;

		VK_InitialiseStructure(region);
		region.bufferOffset = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset.x = offsetx;
		region.imageOffset.y = offsety;
		region.imageOffset.z = 0;
		region.imageExtent.width = width;
		region.imageExtent.height = height;
		region.imageExtent.depth = 1;
		VK_TextureUploadBufferToImageImmediate(stagingBuffer, vktex, &region, 1);
	}

	vkDestroyBuffer(vk_options.logicalDevice, stagingBuffer, NULL);
	vkFreeMemory(vk_options.logicalDevice, stagingMemory, NULL);
}

void VK_TextureSetFiltering(texture_ref texture, texture_minification_id min_filter, texture_magnification_id mag_filter)
{
	vk_texture_t* vktex;

	if (!VK_TextureReferenceInRange(texture)) {
		return;
	}
	vktex = &textureData[texture.index];
	vktex->minFilter = min_filter;
	vktex->magFilter = mag_filter;
	if (vk_options.frame.active) {
		VK_TextureQueueDeferredDescriptorRefresh(texture);
	}
	else {
		VK_TextureUpdateDescriptor(texture);
	}
}

void VK_TextureSetAnisotropy(texture_ref texture, int anisotropy)
{
	vk_texture_t* vktex;

	if (!VK_TextureReferenceInRange(texture)) {
		return;
	}
	vktex = &textureData[texture.index];
	if (vktex->anisotropy == (float)anisotropy) {
		return;
	}
	vktex->anisotropy = (float)anisotropy;

	// No destroy/recreate (and no vkDeviceWaitIdle) needed here: see
	// VK_TextureCachedSampler -- this just switches which cached sampler
	// VK_TextureEnsureSamplers resolves to next.
	if (vk_options.frame.active) {
		VK_TextureQueueDeferredDescriptorRefresh(texture);
	}
	else {
		VK_TextureUpdateDescriptor(texture);
	}
}

void VK_TextureLoadCubemapFace(texture_ref cubemap, r_cubemap_direction_id direction, const byte* data, int width, int height)
{
	(void)cubemap;
	(void)direction;
	(void)data;
	(void)width;
	(void)height;
}

#endif // #ifdef RENDERER_OPTION_VULKAN
