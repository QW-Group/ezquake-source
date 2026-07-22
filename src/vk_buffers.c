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

// vk_buffers.c
// - Vulkan buffer handling

#ifdef RENDERER_OPTION_VULKAN

#include <vulkan/vulkan.h>
#include "quakedef.h"

#include "vk_local.h"
#include "r_local.h"
#include "r_buffers.h"

typedef struct vk_buffer_s {
	VkBuffer handle;
	VkDeviceMemory memory;
	buffertype_t type;
	bufferusage_t usage;
	size_t size;
	// Persistently mapped for the buffer's whole lifetime (all four
	// bufferusage_t styles use HOST_VISIBLE|HOST_COHERENT memory, so this is
	// legal per spec and needs no explicit flush): VK_BufferUpdateSection
	// used to vkMapMemory/vkUnmapMemory on every single call, which is
	// pointless host-driver overhead for memory that's always
	// host-coherent anyway. NULL only while the buffer doesn't exist yet.
	void* mapped;
} vk_buffer_t;

// Each r_buffer_id gets one VkBuffer per frame-in-flight, not a single shared
// one. With VK_MAX_FRAMES_IN_FLIGHT > 1 the CPU can be writing this frame's
// dynamic vertex/index/uniform data while the GPU is still reading the
// previous frame's data from an in-flight command buffer; without per-frame
// copies that's a write-after-read race (visible as flickering/popping
// geometry, worse at higher framerates since frames overlap more). The fix is
// to duplicate the dynamic buffer storage per frame-in-flight and select the
// live copy with the same index used for that frame's fence.
static vk_buffer_t bufferData[r_buffer_count][VK_MAX_FRAMES_IN_FLIGHT];

static void VK_BufferResize(r_buffer_id id, int size, void* data);
static void VK_BufferUpdateSection(r_buffer_id id, ptrdiff_t offset, int size, const void* data);

static vk_buffer_t* VK_BufferCurrentSlot(r_buffer_id id)
{
	return &bufferData[id][vk_options.frame.currentFrame];
}

static void VK_BufferDestroyCopies(r_buffer_id id)
{
	uint32_t i;

	if (vk_options.logicalDevice == VK_NULL_HANDLE) {
		memset(bufferData[id], 0, sizeof(bufferData[id]));
		return;
	}

	for (i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; ++i) {
		vk_buffer_t* slot = &bufferData[id][i];

		if (slot->handle != VK_NULL_HANDLE) {
			vkDestroyBuffer(vk_options.logicalDevice, slot->handle, NULL);
		}
		if (slot->memory != VK_NULL_HANDLE) {
			vkFreeMemory(vk_options.logicalDevice, slot->memory, NULL);
		}
		memset(slot, 0, sizeof(*slot));
	}
}

// Both bufferusage_reuse_many_frames (e.g. the world's static vertex data,
// re-created whole on map load / vid_restart but never updated in place
// afterwards) and bufferusage_constant_data (e.g. small fixed vertex
// buffers like the HUD brighten quad) are written exactly once at
// VK_BufferCreate time and read by the GPU every frame after that for as
// long as they exist. DEVICE_LOCAL memory makes those reads not go through
// PCIe on a discrete desktop GPU (host-visible memory is typically a small,
// slower BAR-mapped window on those); VK_BufferCreate below stages the
// initial data through a HOST_VISIBLE buffer and vkCmdCopyBuffer's it in,
// since DEVICE_LOCAL memory usually isn't itself host-visible.
static qbool VK_BufferMemoryIsDeviceLocal(bufferusage_t usage)
{
	return usage == bufferusage_reuse_many_frames || usage == bufferusage_constant_data;
}

static VkMemoryPropertyFlags VK_BufferMemoryStyle(bufferusage_t usage)
{
	switch (usage) {
		case bufferusage_once_per_frame:
			// filled & used once per frame
			return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		case bufferusage_reuse_per_frame:
			// filled & used many times per frame
			return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		case bufferusage_reuse_many_frames:
			// filled once, expect to use many times over subsequent frames
			return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		case bufferusage_constant_data:
			// filled once, never updated again
			return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		default:
			assert(false);
			return 0;
	}
}

static VkBufferUsageFlags VK_BufferUsageForType(buffertype_t type)
{
	switch (type) {
		case buffertype_vertex:
			return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		case buffertype_index:
			return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		case buffertype_indirect:
			return VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
		case buffertype_storage:
			return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		case buffertype_uniform:
			return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	}

	assert(false);
	Sys_Error("Invalid buffertype passed to VK_BufferUsage(%d)", type);
	return 0;
}

static void VK_BufferStartFrame(void)
{
}

static void VK_BufferEndFrame(void)
{
}

static qbool VK_BufferReady(void)
{
	return true;
}

static qbool VK_BufferCreate(r_buffer_id id, buffertype_t type, const char* name, int size, void* data, bufferusage_t usage)
{
	VkBufferUsageFlags bufferUsage;
	VkMemoryPropertyFlags memoryStyle;
	uint32_t i;

	assert(id > r_buffer_none && id < r_buffer_count);
	if (id <= r_buffer_none || id >= r_buffer_count || size <= 0) {
		return false;
	}

	bufferUsage = VK_BufferUsageForType(type) | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	memoryStyle = VK_BufferMemoryStyle(usage);
	(void)name;

	// Re-creation is uncommon and may replace copies still referenced by either
	// in-flight frame. Keep this path synchronous until buffer retirement is
	// tracked per frame.
	if (bufferData[id][0].handle != VK_NULL_HANDLE) {
		vkDeviceWaitIdle(vk_options.logicalDevice);
		VK_BufferDestroyCopies(id);
	}

	// Recreate every frame's copy with the same size/initial contents so the
	// buffer reads correctly regardless of which frame-in-flight slot is
	// currently live.
	for (i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; ++i) {
		vk_buffer_t* slot = &bufferData[id][i];

		if (!VK_CreateBufferResource(size, bufferUsage, memoryStyle, &slot->handle, &slot->memory)) {
			VK_BufferDestroyCopies(id);
			return false;
		}

		slot->type = type;
		slot->usage = usage;
		slot->size = size;
		slot->mapped = NULL;

		if (VK_BufferMemoryIsDeviceLocal(usage)) {
			// DEVICE_LOCAL memory usually isn't host-visible, so the initial
			// contents (this style is always "filled once" -- there's no
			// VK_BufferUpdateSection path for it after this) go through a
			// temporary HOST_VISIBLE staging buffer and a GPU-side copy
			// instead of a direct memcpy into slot->memory.
			if (data) {
				VkBuffer stagingBuffer = VK_NULL_HANDLE;
				VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
				void* stagingMapped;
				VkCommandBuffer commandBuffer;
				VkBufferCopy copyRegion;

				if (!VK_CreateBufferResource(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingMemory)) {
					VK_BufferDestroyCopies(id);
					return false;
				}
				if (vkMapMemory(vk_options.logicalDevice, stagingMemory, 0, size, 0, &stagingMapped) == VK_SUCCESS) {
					memcpy(stagingMapped, data, size);
					vkUnmapMemory(vk_options.logicalDevice, stagingMemory);
				}

				commandBuffer = VK_BeginImmediateCommands();
				if (commandBuffer != VK_NULL_HANDLE) {
					VK_InitialiseStructure(copyRegion);
					copyRegion.size = size;
					vkCmdCopyBuffer(commandBuffer, stagingBuffer, slot->handle, 1, &copyRegion);
					VK_EndImmediateCommands(commandBuffer);
				}

				vkDestroyBuffer(vk_options.logicalDevice, stagingBuffer, NULL);
				vkFreeMemory(vk_options.logicalDevice, stagingMemory, NULL);
			}
		}
		else if (vkMapMemory(vk_options.logicalDevice, slot->memory, 0, size, 0, &slot->mapped) != VK_SUCCESS) {
			VK_BufferDestroyCopies(id);
			return false;
		}
		else if (data) {
			memcpy(slot->mapped, data, size);
		}
	}

	return true;
}

static void VK_BufferEnsureSize(r_buffer_id id, int size)
{
	if (id <= r_buffer_none || id >= r_buffer_count || size <= 0) {
		return;
	}
	if (bufferData[id][0].handle == VK_NULL_HANDLE || bufferData[id][0].size < (size_t)size) {
		VK_BufferResize(id, size, NULL);
	}
}

static void VK_BufferInitialiseState(void)
{
}

static size_t VK_BufferSize(r_buffer_id id)
{
	return (id > r_buffer_none && id < r_buffer_count) ? bufferData[id][0].size : 0;
}

static uintptr_t VK_BufferOffset(r_buffer_id id)
{
	return 0;
}

static void VK_BufferBind(r_buffer_id id)
{
}

static void VK_BufferBindBase(r_buffer_id id, unsigned int index)
{
}

static void VK_BufferBindRange(r_buffer_id id, unsigned int index, ptrdiff_t offset, int size)
{
}

static void VK_BufferUnBind(buffertype_t type)
{
}

static void VK_BufferUpdate(r_buffer_id id, int size, void* data)
{
	VK_BufferUpdateSection(id, 0, size, data);
}

static void VK_BufferUpdateSection(r_buffer_id id, ptrdiff_t offset, int size, const void* data)
{
	vk_buffer_t* slot;

	if (id <= r_buffer_none || id >= r_buffer_count || size <= 0 || !data) {
		return;
	}

	slot = VK_BufferCurrentSlot(id);
	if (slot->handle == VK_NULL_HANDLE || offset < 0 || (size_t)(offset + size) > slot->size) {
		VK_BufferEnsureSize(id, offset + size);
		slot = VK_BufferCurrentSlot(id);
	}
	if (slot->handle == VK_NULL_HANDLE || !slot->mapped) {
		return;
	}

	// Persistently mapped in VK_BufferCreate/VK_BufferResize -- no
	// vkMapMemory/vkUnmapMemory needed per update, and no explicit flush
	// either: every bufferusage_t style uses HOST_COHERENT memory (see
	// VK_BufferMemoryStyle), which guarantees the GPU sees this write
	// without one.
	memcpy((byte*)slot->mapped + offset, data, size);
}

static void VK_BufferResize(r_buffer_id id, int size, void* data)
{
	buffertype_t type;
	bufferusage_t usage;

	if (id <= r_buffer_none || id >= r_buffer_count || size <= 0) {
		return;
	}

	type = bufferData[id][0].type ? bufferData[id][0].type : buffertype_vertex;
	usage = bufferData[id][0].usage ? bufferData[id][0].usage : bufferusage_once_per_frame;
	VK_BufferCreate(id, type, NULL, size, data, usage);
}

static qbool VK_BufferIsValid(r_buffer_id id)
{
	return (id > r_buffer_none && id < r_buffer_count && VK_BufferCurrentSlot(id)->handle != VK_NULL_HANDLE);
}

static void VK_BufferSetElementArray(r_buffer_id id)
{
	return;
}

static void VK_BufferShutdown(void)
{
	int i;

	// R_Shutdown() calls this while the device is still alive, before any
	// frame the GPU is still executing has been waited on (that's the whole
	// point of the ordering -- see the comment in r_main.c). Without this
	// wait, vkDestroyBuffer()/vkFreeMemory() below can run against a buffer
	// the GPU is still reading from an in-flight frame's draw calls, which
	// showed up as a full driver TDR (all monitors blanking) on vid_restart
	// during a live session, not just on the boot-time no-op case where
	// nothing was ever in flight.
	if (vk_options.logicalDevice != VK_NULL_HANDLE) {
		vkDeviceWaitIdle(vk_options.logicalDevice);
	}

	for (i = 0; i < r_buffer_count; ++i) {
		VK_BufferDestroyCopies(i);
	}
	return;
}

VkBuffer VK_BufferHandle(r_buffer_id id)
{
	if (id <= r_buffer_none || id >= r_buffer_count) {
		return VK_NULL_HANDLE;
	}
	return VK_BufferCurrentSlot(id)->handle;
}

VkDeviceSize VK_BufferDeviceOffset(r_buffer_id id)
{
	(void)id;
	return 0;
}

#ifdef WITH_RENDERING_TRACE
static void VK_PrintBufferState(FILE* output, int depth)
{
}
#endif

void VK_InitialiseBufferHandling(api_buffers_t* api)
{
	memset(api, 0, sizeof(*api));

	api->InitialiseState = VK_BufferInitialiseState;

	api->StartFrame = VK_BufferStartFrame;
	api->EndFrame = VK_BufferEndFrame;
	api->FrameReady = VK_BufferReady;

	api->Size = VK_BufferSize;
	api->Create = VK_BufferCreate;
	api->BufferOffset = VK_BufferOffset;

	api->Bind = VK_BufferBind;
	api->BindBase = VK_BufferBindBase;
	api->BindRange = VK_BufferBindRange;
	api->UnBind = VK_BufferUnBind;

	api->Update = VK_BufferUpdate;
	api->UpdateSection = VK_BufferUpdateSection;
	api->Resize = VK_BufferResize;
	api->EnsureSize = VK_BufferEnsureSize;

	api->IsValid = VK_BufferIsValid;
	api->SetElementArray = VK_BufferSetElementArray;
	api->Shutdown = VK_BufferShutdown;

#ifdef WITH_RENDERING_TRACE
	api->PrintState = VK_PrintBufferState;
#endif
}

#endif // #ifdef RENDERER_OPTION_VULKAN
