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

// vk_physical_devices.c

#ifdef RENDERER_OPTION_VULKAN

#include <vulkan/vulkan.h>
#include "quakedef.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "vk_local.h"

static const char* requiredDeviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

static int VK_PhysicalDeviceTypeScore(VkPhysicalDeviceType type)
{
	switch (type) {
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
			return 3;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
			return 2;
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
			return 1;
		default:
			return 0;
	}
}

static const char* VK_PhysicalDeviceTypeName(VkPhysicalDeviceType type)
{
	switch (type) {
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
			return "discrete";
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
			return "virtual";
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
			return "integrated";
		case VK_PHYSICAL_DEVICE_TYPE_CPU:
			return "cpu";
		default:
			return "other";
	}
}

static void VK_PhysicalDeviceQueryQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface, int* graphics_queue_index, int* compute_queue_index, int* present_queue_index)
{
	uint32_t queue_families_count;
	VkQueueFamilyProperties* queue_family_properties;
	uint32_t j;

	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_families_count, NULL);
	queue_family_properties = Q_malloc(sizeof(VkQueueFamilyProperties) * queue_families_count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_families_count, queue_family_properties);

	*graphics_queue_index = *compute_queue_index = *present_queue_index = -1;
	for (j = 0; j < queue_families_count; ++j) {
		if (queue_family_properties[j].queueCount > 0) {
			if ((queue_family_properties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) && *graphics_queue_index < 0) {
				*graphics_queue_index = j;
			}

			if ((queue_family_properties[j].queueFlags & VK_QUEUE_COMPUTE_BIT) && *compute_queue_index < 0) {
				*compute_queue_index = j;
			}

			if (surface != VK_NULL_HANDLE && *present_queue_index < 0) {
				VkBool32 present_supported = VK_FALSE;

				if (vkGetPhysicalDeviceSurfaceSupportKHR(device, j, surface, &present_supported) == VK_SUCCESS && present_supported) {
					*present_queue_index = j;
				}
			}
		}
	}

	Q_free(queue_family_properties);
}

static qbool VK_PhysicalDeviceSupportsRequiredExtensions(VkPhysicalDevice device)
{
	// 
	uint32_t count;
	VkExtensionProperties* properties;
	uint32_t foundCount = 0;
	uint32_t i, j;

	vkEnumerateDeviceExtensionProperties(device, NULL, &count, NULL);
	properties = Q_malloc(count * sizeof(VkExtensionProperties));
	vkEnumerateDeviceExtensionProperties(device, NULL, &count, properties);

	for (i = 0; i < count; ++i) {
		for (j = 0; j < sizeof(requiredDeviceExtensions) / sizeof(requiredDeviceExtensions[0]); ++j) {
			if (!strcmp(properties[i].extensionName, requiredDeviceExtensions[j])) {
				++foundCount;
				break;
			}
		}
	}
	Q_free(properties);

	return (foundCount == sizeof(requiredDeviceExtensions) / sizeof(requiredDeviceExtensions[0]));
}

// Unlike VK_PhysicalDeviceSupportsRequiredExtensions, this never fails the
// device -- it just reports which anti-lag/low-latency extensions are
// present, so VK_CreateLogicalDevice can enable exactly those by name. AMD's anti-lag
// extension additionally needs its pNext feature bit confirmed separately
// (see the VkPhysicalDeviceAntiLagFeaturesAMD query in VK_CreateLogicalDevice)
// before it's safe to treat as supported -- the extension string alone isn't
// enough, same as how samplerAnisotropy can't just be assumed from a feature
// existing in the struct.
static void VK_PhysicalDeviceSupportsOptionalExtensions(VkPhysicalDevice device, qbool* supportsAmdAntiLagExt, qbool* supportsNvLowLatency2)
{
	uint32_t count;
	VkExtensionProperties* properties;
	uint32_t i;

	*supportsAmdAntiLagExt = false;
	*supportsNvLowLatency2 = false;

	vkEnumerateDeviceExtensionProperties(device, NULL, &count, NULL);
	properties = Q_malloc(count * sizeof(VkExtensionProperties));
	vkEnumerateDeviceExtensionProperties(device, NULL, &count, properties);

	for (i = 0; i < count; ++i) {
		if (!strcmp(properties[i].extensionName, VK_AMD_ANTI_LAG_EXTENSION_NAME)) {
			*supportsAmdAntiLagExt = true;
		}
		else if (!strcmp(properties[i].extensionName, VK_NV_LOW_LATENCY_2_EXTENSION_NAME)) {
			*supportsNvLowLatency2 = true;
		}
	}
	Q_free(properties);
}

static qbool VK_PhysicalDeviceBestPresentationMode(VkPhysicalDevice device, VkSurfaceKHR surface, VkPresentModeKHR* best)
{
	extern cvar_t r_swapInterval;
	// With vsync off, GLC/GLM give real swap-interval-0 tearing/uncapped
	// behaviour (SDL_GL_SetSwapInterval(0)); IMMEDIATE_KHR is the Vulkan
	// equivalent. MAILBOX_KHR is still vsync'd to the display (no tearing,
	// frame replacement instead of blocking) -- picking it ahead of
	// IMMEDIATE when the user asked for vsync off silently gave a
	// synced/capped Vulkan present regardless of the vid_vsync setting.
	// IMMEDIATE first (matches vid_vsync 0 semantics), MAILBOX as the next
	// best low-latency fallback if unsupported. The historical concern with
	// IMMEDIATE (see R_EndRendering()'s comment in vid_sdl.c: mid-transition
	// re-enumeration during unrelated swapchain recreates was observed to
	// settle on IMMEDIATE permanently and freeze the app) is about *when*
	// this function gets called, not which mode it prefers here -- that call
	// site already scopes re-evaluation to explicit r_swapInterval changes.
	VkPresentModeKHR preferredModesOff[] = {
		VK_PRESENT_MODE_IMMEDIATE_KHR,     // tearing, uncapped -- matches vid_vsync 0
		VK_PRESENT_MODE_MAILBOX_KHR,       // triple buffered, still vsync'd
		VK_PRESENT_MODE_FIFO_RELAXED_KHR,
		VK_PRESENT_MODE_FIFO_KHR
	};
	// GLC/GLM's vid_vsync -1 -> SDL_GL_SetSwapInterval(-1) ("adaptive" vsync):
	// vsync'd normally, but if a frame missed its interval the next present
	// goes out immediately instead of waiting a full extra interval like FIFO
	// would -- FIFO_RELAXED_KHR is Vulkan's direct equivalent (KHR spec:
	// behaves like FIFO except a late frame presents immediately without
	// waiting for the next vblank). r_swapInterval.integer was truthy for any
	// nonzero value including -1, so this used to fall into the `> 0` FIFO
	// branch below and silently give plain FIFO instead.
	VkPresentModeKHR preferredModesAdaptive[] = {
		VK_PRESENT_MODE_FIFO_RELAXED_KHR,
		VK_PRESENT_MODE_FIFO_KHR
	};
	VkPresentModeKHR* preferredModes;
	uint32_t preferredModeCount;
	VkPresentModeKHR* presentationModes;
	uint32_t count;
	uint32_t i, j;

	// This is guaranteed to be supported
	if (r_swapInterval.integer > 0) {
		*best = VK_PRESENT_MODE_FIFO_KHR;
		return true;
	}

	if (r_swapInterval.integer < 0) {
		preferredModes = preferredModesAdaptive;
		preferredModeCount = sizeof(preferredModesAdaptive) / sizeof(preferredModesAdaptive[0]);
	}
	else {
		preferredModes = preferredModesOff;
		preferredModeCount = sizeof(preferredModesOff) / sizeof(preferredModesOff[0]);
	}

	if (vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, NULL) != VK_SUCCESS) {
		return false;
	}

	presentationModes = Q_malloc(count * sizeof(presentationModes[0]));
	if (vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, presentationModes) != VK_SUCCESS) {
		Q_free(presentationModes);
		return false;
	}

	// TEMP diagnostic (Ciscon's "vsync 0 still capped at monitor Hz" report,
	// investigating whether IMMEDIATE_KHR is actually offered by this
	// WSI/compositor -- known to be commonly absent under Wayland).
	{
		const char* names[] = { "?", "MAILBOX", "IMMEDIATE", "FIFO_RELAXED", "FIFO", "SHARED_DEMAND_REFRESH", "SHARED_CONTINUOUS_REFRESH" };
		for (j = 0; j < count; ++j) {
			VkPresentModeKHR m = presentationModes[j];
			const char* name =
				m == VK_PRESENT_MODE_MAILBOX_KHR ? names[1] :
				m == VK_PRESENT_MODE_IMMEDIATE_KHR ? names[2] :
				m == VK_PRESENT_MODE_FIFO_RELAXED_KHR ? names[3] :
				m == VK_PRESENT_MODE_FIFO_KHR ? names[4] : names[0];
			Con_Printf("vulkan: surface offers present mode %d (%s)\n", (int)m, name);
		}
	}

	for (i = 0; i < preferredModeCount; ++i) {
		for (j = 0; j < count; ++j) {
			if (preferredModes[i] == presentationModes[j]) {
				Con_Printf("vulkan: selected present mode %d\n", (int)preferredModes[i]);
				Q_free(presentationModes);
				*best = preferredModes[i];
				return true;
			}
		}
	}

	Q_free(presentationModes);
	*best = VK_PRESENT_MODE_FIFO_KHR;
	return true;
}

// Re-evaluates the present mode against the current r_swapInterval value.
// VK_PhysicalDeviceBestPresentationMode() is normally only invoked once,
// while picking the physical device at startup; toggling "Vertical Sync" in
// the menu needs this re-run before the swapchain is recreated, otherwise
// the swapchain just gets rebuilt with the same present mode it already had.
qbool VK_RefreshPresentationMode(void)
{
	VkPresentModeKHR best;

	if (vk_options.physicalDevice == VK_NULL_HANDLE || vk_options.surface == VK_NULL_HANDLE) {
		return false;
	}
	if (!VK_PhysicalDeviceBestPresentationMode(vk_options.physicalDevice, vk_options.surface, &best)) {
		return false;
	}
	vk_options.physicalDevicePresentationMode = best;
	return true;
}

static qbool VK_PhysicalDeviceSwapChainCompatible(VkPhysicalDevice device, VkSurfaceKHR surface, VkSurfaceFormatKHR* preferred_format, VkSurfaceCapabilitiesKHR* capabilities)
{
	extern cvar_t vid_gammacorrection;
	uint32_t num_formats;
	VkSurfaceFormatKHR* formats;
	VkColorSpaceKHR req_color_space = (vid_gammacorrection.integer ? VK_COLOR_SPACE_SRGB_NONLINEAR_KHR : VK_COLOR_SPACE_PASS_THROUGH_EXT);

	if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, capabilities) != VK_SUCCESS) {
		return false;
	}

	if (vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &num_formats, NULL) != VK_SUCCESS || num_formats == 0) {
		return false;
	}
	formats = Q_malloc(num_formats * sizeof(formats[0]));
	if (vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &num_formats, formats) != VK_SUCCESS) {
		Q_free(formats);
		return false;
	}

	if (num_formats == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
		// Special case: no preferred format
		preferred_format->colorSpace = req_color_space;
		preferred_format->format = VK_FORMAT_B8G8R8A8_UNORM;
	}
	else {
		// vkGetPhysicalDeviceSurfaceFormatsKHR's enumeration order isn't
		// guaranteed by the spec, so it varies by vendor -- falling back to
		// formats[0] unconditionally (as this used to) risked picking
		// whatever the driver happened to list first, with no format or
		// colorspace preference at all, on any device where the exact
		// B8G8R8A8_UNORM/req_color_space combination below isn't offered.
		// The two-pass search below still prefers an exact match, but the
		// fallback now prefers matching just the format (any colorspace),
		// then just the colorspace (any 8-bit BGRA/RGBA format), before
		// finally taking formats[0] as a last resort.
		uint32_t i;
		int best_format_only = -1;
		int best_colorspace_only = -1;

		for (i = 0; i < num_formats; ++i) {
			qbool format_match = (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM);
			qbool colorspace_match = (formats[i].colorSpace == req_color_space);

			if (format_match && colorspace_match) {
				preferred_format->colorSpace = req_color_space;
				preferred_format->format = VK_FORMAT_B8G8R8A8_UNORM;
				break;
			}
			if (format_match && best_format_only < 0) {
				best_format_only = (int)i;
			}
			if (colorspace_match && best_colorspace_only < 0) {
				best_colorspace_only = (int)i;
			}
		}

		if (i >= num_formats) {
			int fallback = (best_format_only >= 0) ? best_format_only : (best_colorspace_only >= 0 ? best_colorspace_only : 0);

			// vid_gammacorrection 2 ("require sRGB, reject the device rather
			// than fall back") only makes that promise for the colorspace --
			// matches GLC/GLM's vid_options[] fallback ladder (vid_sdl.c),
			// which for gammacorrection==2 skips every candidate lacking
			// VID_GAMMACORRECTED instead of silently accepting a
			// non-sRGB-capable context. Without this, 1 and 2 were
			// indistinguishable: both accepted whatever colorspace the
			// fallback search above landed on.
			if (vid_gammacorrection.integer == 2 && formats[fallback].colorSpace != req_color_space) {
				Q_free(formats);
				return false;
			}

			preferred_format->colorSpace = formats[fallback].colorSpace;
			preferred_format->format = formats[fallback].format;
		}
	}
	Q_free(formats);
	return true;
}

qbool VK_SelectPhysicalDevice(VkInstance instance, VkSurfaceKHR surface)
{
	extern cvar_t vid_vulkan_device;
	uint32_t deviceCount = 0;
	VkResult result;
	VkPhysicalDevice* physicalDevices;
	uint32_t i;
	int best_score = -1;

	result = vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);
	if (result != VK_SUCCESS) {
		Con_Printf("vulkan: enumerating physical devices failed\n");
		return false;
	}
	physicalDevices = Q_malloc(deviceCount * sizeof(VkPhysicalDevice));
	result = vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices);
	if (result != VK_SUCCESS) {
		Q_free(physicalDevices);
		return false;
	}

	vk_options.physicalDevice = VK_NULL_HANDLE;
	for (i = 0; i < deviceCount; ++i) {
		VkPhysicalDeviceFeatures features;
		VkPhysicalDeviceProperties properties;
		VkPresentModeKHR best_presentation_mode;
		qbool new_best = true;
		int graphics_queue_index = -1;
		int compute_queue_index = -1;
		int present_queue_index = -1;
		VkSurfaceFormatKHR preferred_format;
		VkSurfaceCapabilitiesKHR capabilities;

		vkGetPhysicalDeviceProperties(physicalDevices[i], &properties);
		vkGetPhysicalDeviceFeatures(physicalDevices[i], &features);
		Con_Printf("Device %d: %s (%s)\n", i, properties.deviceName, VK_PhysicalDeviceTypeName(properties.deviceType));

		if (vid_vulkan_device.integer >= 0 && (uint32_t)vid_vulkan_device.integer != i) {
			Com_Printf("Device %d: %s - skipped, vid_vulkan_device is forcing device %d\n", i, properties.deviceName, vid_vulkan_device.integer);
			continue;
		}

		if (!VK_PhysicalDeviceSupportsRequiredExtensions(physicalDevices[i])) {
			Com_Printf("Device %d: %s - rejected, missing required extension(s) (VK_KHR_swapchain)\n", i, properties.deviceName);
			continue;
		}

		// Must support graphics queues
		VK_PhysicalDeviceQueryQueueFamilies(physicalDevices[i], surface, &graphics_queue_index, &compute_queue_index, &present_queue_index);
		if (graphics_queue_index < 0) {
			Com_Printf("Device %d: %s - rejected, no graphics-capable queue family\n", i, properties.deviceName);
			continue;
		}
		if (compute_queue_index < 0) {
			compute_queue_index = graphics_queue_index;
		}
		if (present_queue_index < 0) {
			Com_Printf("Device %d: %s - rejected, no queue family can present to this surface\n", i, properties.deviceName);
			continue;
		}

		if (!VK_PhysicalDeviceSwapChainCompatible(physicalDevices[i], surface, &preferred_format, &capabilities)) {
			Com_Printf("Device %d: %s - rejected, incompatible swapchain/surface (capabilities or formats query failed)\n", i, properties.deviceName);
			continue;
		}

		if (!VK_PhysicalDeviceBestPresentationMode(physicalDevices[i], surface, &best_presentation_mode)) {
			Com_Printf("Device %d: %s - rejected, failed to query present modes\n", i, properties.deviceName);
			continue;
		}

		// Prefer discrete GPUs over integrated/virtual/CPU ones, unless vid_vulkan_device
		// is forcing a specific index (handled above) -- a laptop with both an Intel/AMD
		// iGPU and a dedicated NVIDIA/AMD GPU should not end up running on the iGPU just
		// because it happened to enumerate first.
		{
			int score = VK_PhysicalDeviceTypeScore(properties.deviceType);
			new_best = (vk_options.physicalDevice == VK_NULL_HANDLE || score > best_score);

			if (new_best) {
				best_score = score;
			}
		}

		if (new_best) {
			vk_options.physicalDevice = physicalDevices[i];
			memcpy(&vk_options.physicalDeviceFeatures, &features, sizeof(vk_options.physicalDeviceFeatures));
			memcpy(&vk_options.physicalDeviceProperties, &properties, sizeof(vk_options.physicalDeviceProperties));
			vk_options.physicalDeviceGraphicsQueueFamilyIndex = graphics_queue_index;
			vk_options.physicalDeviceComputeQueueFamilyIndex = compute_queue_index;
			vk_options.physicalDevicePresentQueueFamilyIndex = present_queue_index;
			vk_options.physicalDevicePresentationMode = best_presentation_mode;
			vk_options.physicalDeviceSurfaceFormat = preferred_format;
			vk_options.physicalDeviceSurfaceCapabilities = capabilities;
		}
	}

	Q_free(physicalDevices);
	if (vk_options.physicalDevice == VK_NULL_HANDLE) {
		Com_Printf("No appropriate device found :(\n");
		return false;
	}

	Com_Printf("Selected device: %s (%s)\n", vk_options.physicalDeviceProperties.deviceName, VK_PhysicalDeviceTypeName(vk_options.physicalDeviceProperties.deviceType));

	return true;
}

// Called once from VK_Initialise, right after VK_SelectPhysicalDevice has
// populated vk_options.physicalDeviceProperties -- needs the device's actual
// limits before render pass/swapchain/pipeline creation, all of which read
// vk_options.msaaSamples. vid_framebuffer_multisample is the same cvar the GL
// renderers already use for their own framebuffer MSAA (see gl_framebuffer.c);
// "0" (its default) maps to VK_SAMPLE_COUNT_1_BIT, i.e. no MSAA, taking the
// exact same render pass/framebuffer/pipeline path as before this feature
// existed.
void VK_DetermineMSAASampleCount(void)
{
	extern cvar_t vid_framebuffer_multisample;
	VkSampleCountFlags supported;
	int requestedInt;
	VkSampleCountFlagBits candidate;

	vk_options.msaaSamples = VK_SAMPLE_COUNT_1_BIT;

	if (vid_framebuffer_multisample.integer <= 1) {
		return;
	}

	// Needs to be supported for *both* color and depth attachments, since
	// they're both in the same subpass at the same sample count.
	supported = vk_options.physicalDeviceProperties.limits.framebufferColorSampleCounts &
		vk_options.physicalDeviceProperties.limits.framebufferDepthSampleCounts;

	Q_ROUND_POWER2(min(vid_framebuffer_multisample.integer, 64), requestedInt);

	// Walk down from the requested count to the largest supported count that
	// doesn't exceed it, rather than failing outright on hardware that caps
	// out lower than requested (e.g. requesting 16x on a device that maxes
	// out at 4x should silently give 4x, not 1x/none).
	for (candidate = (VkSampleCountFlagBits)requestedInt; candidate > VK_SAMPLE_COUNT_1_BIT; candidate >>= 1) {
		if (supported & candidate) {
			vk_options.msaaSamples = candidate;
			if (vk_options.msaaSamples != requestedInt) {
				Con_Printf("vulkan: %dx multisampling requested, device supports up to %dx\n", requestedInt, (int)vk_options.msaaSamples);
			}
			return;
		}
	}

	Con_Printf("vulkan: device doesn't support multisampling, vid_framebuffer_multisample disabled\n");
}

uint32_t VK_PhysicalDeviceGraphicsQueueFamilyIndex(void)
{
	assert(vk_options.physicalDevice != VK_NULL_HANDLE);

	return vk_options.physicalDeviceGraphicsQueueFamilyIndex;
}

uint32_t VK_PhysicalDeviceComputeQueueFamilyIndex(void)
{
	assert(vk_options.physicalDevice != VK_NULL_HANDLE);

	return vk_options.physicalDeviceComputeQueueFamilyIndex;
}

uint32_t VK_PhysicalDevicePresentQueueFamilyIndex(void)
{
	assert(vk_options.physicalDevice != VK_NULL_HANDLE);

	return vk_options.physicalDevicePresentQueueFamilyIndex;
}

qbool VK_CreateLogicalDevice(VkInstance instance)
{
	VkDeviceQueueCreateInfo queueInfos[2] = { { 0 } };
	VkDeviceCreateInfo deviceInfo = { 0 };
	VkPhysicalDeviceFeatures deviceFeatures = { 0 };
	VkPhysicalDeviceAntiLagFeaturesAMD antiLagFeatures = { 0 };
	VkPhysicalDeviceFeatures2 features2 = { 0 };
	VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingQuery = { 0 };
	VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingEnable = { 0 };
	float priorities[] = { 1.0f };
	uint32_t queueCount = 0;
	const char* enabledExtensions[4];
	uint32_t enabledExtensionCount = 0;
	qbool amdAntiLagExtPresent = false;
	qbool nvLowLatency2Present = false;
	uint32_t i;

	// gl_anisotropy needs this enabled device-wide before any sampler can
	// set anisotropyEnable -- only requested if the physical device actually
	// supports it (queried into vk_options.physicalDeviceFeatures during
	// VK_SelectPhysicalDevice), so it's a no-op rather than a vkCreateDevice
	// failure on whatever hardware doesn't.
	deviceFeatures.samplerAnisotropy = vk_options.physicalDeviceFeatures.samplerAnisotropy;

	for (i = 0; i < sizeof(requiredDeviceExtensions) / sizeof(requiredDeviceExtensions[0]); ++i) {
		enabledExtensions[enabledExtensionCount++] = requiredDeviceExtensions[i];
	}

	// Only probe/enable the vendor low-latency extensions when the user has
	// actually opted in via vid_vulkan_antilag. Enabling either extension on
	// the device unconditionally (regardless of whether anything ever calls
	// its functions) was found to cause VK_ERROR_DEVICE_LOST on NVIDIA during
	// normal Vulkan init, even with the cvar left at its default of 0 -- the
	// AMD path never exercises this since the AMD driver used for testing
	// didn't report support, but better to not enable either string unless
	// requested.
	{
		extern cvar_t vid_vulkan_antilag;

		if (vid_vulkan_antilag.integer) {
			VK_PhysicalDeviceSupportsOptionalExtensions(vk_options.physicalDevice, &amdAntiLagExtPresent, &nvLowLatency2Present);
		}
	}

	vk_options.supportsAmdAntiLag = false;
	if (amdAntiLagExtPresent) {
		// VK_AMD_anti_lag also gates itself behind a pNext feature bit --
		// the extension being present in vkEnumerateDeviceExtensionProperties
		// is not sufficient on its own (some drivers expose the extension
		// string but report the feature as unsupported).
		antiLagFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ANTI_LAG_FEATURES_AMD;
		features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features2.pNext = &antiLagFeatures;
		vkGetPhysicalDeviceFeatures2(vk_options.physicalDevice, &features2);

		if (antiLagFeatures.antiLag) {
			vk_options.supportsAmdAntiLag = true;
			enabledExtensions[enabledExtensionCount++] = VK_AMD_ANTI_LAG_EXTENSION_NAME;
		}
	}

	vk_options.supportsNvLowLatency2 = nvLowLatency2Present;
	if (nvLowLatency2Present) {
		enabledExtensions[enabledExtensionCount++] = VK_NV_LOW_LATENCY_2_EXTENSION_NAME;
	}

	// Core-in-1.2 feature, no extension string needed -- just query support
	// via the pNext chain (same pattern as antiLagFeatures above) and, if
	// present, request it be enabled on the logical device via the same
	// pNext chain below. See VK_TextureBindlessDescriptorSet in vk_texture.c
	// for what this unlocks.
	{
		VkPhysicalDeviceFeatures2 indexingQueryFeatures2 = { 0 };

		descriptorIndexingQuery.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
		indexingQueryFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		indexingQueryFeatures2.pNext = &descriptorIndexingQuery;
		vkGetPhysicalDeviceFeatures2(vk_options.physicalDevice, &indexingQueryFeatures2);

		vk_options.supportsDescriptorIndexing =
			descriptorIndexingQuery.shaderSampledImageArrayNonUniformIndexing &&
			descriptorIndexingQuery.descriptorBindingPartiallyBound &&
			descriptorIndexingQuery.descriptorBindingVariableDescriptorCount &&
			descriptorIndexingQuery.runtimeDescriptorArray &&
			descriptorIndexingQuery.descriptorBindingSampledImageUpdateAfterBind;
	}

	queueInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfos[0].queueFamilyIndex = VK_PhysicalDeviceGraphicsQueueFamilyIndex();
	queueInfos[0].queueCount = sizeof(priorities) / sizeof(priorities[0]);
	queueInfos[0].pQueuePriorities = priorities;
	++queueCount;

	if (VK_PhysicalDeviceGraphicsQueueFamilyIndex() != VK_PhysicalDevicePresentQueueFamilyIndex()) {
		queueInfos[queueCount].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfos[queueCount].queueFamilyIndex = VK_PhysicalDevicePresentQueueFamilyIndex();
		queueInfos[queueCount].queueCount = sizeof(priorities) / sizeof(priorities[0]);
		queueInfos[queueCount].pQueuePriorities = priorities;
		++queueCount;
	}

	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.pQueueCreateInfos = queueInfos;
	deviceInfo.queueCreateInfoCount = queueCount;
	deviceInfo.pEnabledFeatures = &deviceFeatures;

	// Chain every optional pNext feature struct we're actually requesting onto
	// deviceInfo.pNext together -- each assignment below only ever OVERWRITES
	// deviceInfo.pNext if it hasn't been claimed by an earlier one, otherwise
	// it links onto the existing chain, so enabling more than one of these
	// doesn't silently drop an earlier request.
	if (vk_options.supportsDescriptorIndexing) {
		// Re-using descriptorIndexingEnable as a fresh request struct (only
		// the 5 bits VK_CreateLogicalDevice actually checked support for
		// above are requested here, not a blind copy of the query result --
		// the query struct may report other descriptor-indexing bits this
		// code doesn't use and has no business requesting).
		descriptorIndexingEnable.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
		descriptorIndexingEnable.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
		descriptorIndexingEnable.descriptorBindingPartiallyBound = VK_TRUE;
		descriptorIndexingEnable.descriptorBindingVariableDescriptorCount = VK_TRUE;
		descriptorIndexingEnable.runtimeDescriptorArray = VK_TRUE;
		descriptorIndexingEnable.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
		descriptorIndexingEnable.pNext = (void*)deviceInfo.pNext;
		deviceInfo.pNext = &descriptorIndexingEnable;
	}

	if (vk_options.supportsAmdAntiLag) {
		// Re-using antiLagFeatures (already populated above) to actually
		// request the feature be enabled on the logical device, same struct
		// instance just attached to a different sType-chain root this time.
		antiLagFeatures.pNext = (void*)deviceInfo.pNext;
		deviceInfo.pNext = &antiLagFeatures;
	}

	deviceInfo.enabledExtensionCount = enabledExtensionCount;
	deviceInfo.ppEnabledExtensionNames = enabledExtensions;
	// Device-level layers are legacy/ignored since Vulkan 1.0 -- only
	// instance layers (VK_CreateInstance/VK_AddValidationLayers) matter.
	// Leaving enabledLayerCount non-zero here just trips validation:
	// "vkCreateDevice(): pCreateInfo->enabledLayerCount is 1 (not zero)".
	deviceInfo.enabledLayerCount = 0;

	vk_options.logicalDevice = VK_NULL_HANDLE;
	{
		VkResult result = vkCreateDevice(vk_options.physicalDevice, &deviceInfo, NULL, &vk_options.logicalDevice);
		if (result != VK_SUCCESS) {
			Con_Printf("vulkan: vkCreateDevice() failed: %d\n", result);
			return false;
		}
	}

	vkGetDeviceQueue(vk_options.logicalDevice, VK_PhysicalDeviceGraphicsQueueFamilyIndex(), 0, &vk_options.graphicsQueue);
	if (VK_PhysicalDeviceGraphicsQueueFamilyIndex() != VK_PhysicalDevicePresentQueueFamilyIndex()) {
		vkGetDeviceQueue(vk_options.logicalDevice, VK_PhysicalDevicePresentQueueFamilyIndex(), 0, &vk_options.presentQueue);
	}
	else {
		vk_options.presentQueue = vk_options.graphicsQueue;
	}

	if (vk_options.supportsAmdAntiLag) {
		vk_options.antiLagUpdateAMD = (PFN_vkAntiLagUpdateAMD)vkGetDeviceProcAddr(vk_options.logicalDevice, "vkAntiLagUpdateAMD");
		vk_options.supportsAmdAntiLag = (vk_options.antiLagUpdateAMD != NULL);
	}

	if (vk_options.supportsNvLowLatency2) {
		vk_options.setLatencySleepModeNV = (PFN_vkSetLatencySleepModeNV)vkGetDeviceProcAddr(vk_options.logicalDevice, "vkSetLatencySleepModeNV");
		vk_options.latencySleepNV = (PFN_vkLatencySleepNV)vkGetDeviceProcAddr(vk_options.logicalDevice, "vkLatencySleepNV");
		vk_options.setLatencyMarkerNV = (PFN_vkSetLatencyMarkerNV)vkGetDeviceProcAddr(vk_options.logicalDevice, "vkSetLatencyMarkerNV");
		vk_options.supportsNvLowLatency2 = (vk_options.setLatencySleepModeNV && vk_options.latencySleepNV && vk_options.setLatencyMarkerNV);

		if (vk_options.supportsNvLowLatency2) {
			// vkLatencySleepNV signals via signalSemaphore/value, which the
			// app then waits on with vkWaitSemaphores -- that wait-by-value
			// API only works against a timeline semaphore, not a plain
			// binary one, so this needs the VkSemaphoreTypeCreateInfo pNext.
			VkSemaphoreTypeCreateInfo typeInfo = { 0 };
			VkSemaphoreCreateInfo semaphoreInfo = { 0 };

			typeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
			typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
			typeInfo.initialValue = 0;

			semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			semaphoreInfo.pNext = &typeInfo;
			if (vkCreateSemaphore(vk_options.logicalDevice, &semaphoreInfo, NULL, &vk_options.latencySleepSemaphore) != VK_SUCCESS) {
				vk_options.supportsNvLowLatency2 = false;
				vk_options.latencySleepSemaphore = VK_NULL_HANDLE;
			}
		}
	}

	if (vk_options.supportsAmdAntiLag) {
		Com_Printf("vulkan: AMD Anti-Lag supported\n");
	}
	if (vk_options.supportsNvLowLatency2) {
		Com_Printf("vulkan: NVIDIA Low Latency 2 supported\n");
	}

	VK_LoadPipelineCache();

	return true;
}

#define VK_PIPELINE_CACHE_FILE "vulkan/pipeline_cache.bin"

// VkPipelineCacheHeaderVersionOne, the layout of the first bytes of any
// pipeline cache blob (Vulkan spec 10.9, "Pipeline Cache"). Not exposed as a
// struct by the headers this project builds against, so laid out by hand
// here purely to validate a blob before ever handing it to the driver.
#pragma pack(push, 1)
typedef struct {
	uint32_t headerSize;
	uint32_t headerVersion;
	uint32_t vendorID;
	uint32_t deviceID;
	uint8_t  pipelineCacheUUID[VK_UUID_SIZE];
} vk_pipeline_cache_header_t;
#pragma pack(pop)

// Loads a previously saved driver pipeline cache blob, if any, so
// vkCreateGraphicsPipelines() at the various call sites can skip re-compiling
// shaders/pipelines it has already seen on this GPU+driver. A missing, empty,
// or driver-rejected (stale/foreign) blob is not an error: VkPipelineCache is
// purely an optimization hint, and vkCreatePipelineCache() with no/garbage
// initial data still returns a valid, usable (just initially empty) cache.
//
// The header is still checked by hand before that, rather than trusting the
// driver to reject a mismatched blob cleanly: this file's on-disk cache can
// span multiple ezquake builds/driver updates/GPU swaps over its lifetime,
// and hasn't been proven safe to hand a blob from a different GPU/driver to
// vkCreatePipelineCache() on every driver this project supports -- cheaper to
// just not pass it through at all when the header doesn't match this device.
void VK_LoadPipelineCache(void)
{
	VkPipelineCacheCreateInfo cacheInfo = { 0 };
	int cacheLen = 0;
	byte* cacheData = FS_LoadHeapFile(VK_PIPELINE_CACHE_FILE, &cacheLen);
	qbool headerValid = false;

	if (cacheData && (size_t)cacheLen >= sizeof(vk_pipeline_cache_header_t)) {
		vk_pipeline_cache_header_t* header = (vk_pipeline_cache_header_t*)cacheData;

		headerValid = (
			header->headerVersion == VK_PIPELINE_CACHE_HEADER_VERSION_ONE &&
			header->vendorID == vk_options.physicalDeviceProperties.vendorID &&
			header->deviceID == vk_options.physicalDeviceProperties.deviceID &&
			memcmp(header->pipelineCacheUUID, vk_options.physicalDeviceProperties.pipelineCacheUUID, VK_UUID_SIZE) == 0
		);
	}

	cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	if (headerValid) {
		cacheInfo.initialDataSize = (size_t)cacheLen;
		cacheInfo.pInitialData = cacheData;
	}

	vk_options.pipelineCache = VK_NULL_HANDLE;
	if (vkCreatePipelineCache(vk_options.logicalDevice, &cacheInfo, NULL, &vk_options.pipelineCache) != VK_SUCCESS) {
		vk_options.pipelineCache = VK_NULL_HANDLE;
	}

	if (cacheData) {
		Q_free(cacheData);
	}
}

// Persists the (possibly now-larger) pipeline cache so the next run starts
// warm. Called once on shutdown, after every pipeline that might be created
// this session already has been -- see VK_Shutdown.
void VK_SavePipelineCache(void)
{
	size_t dataSize = 0;
	void* data;

	if (vk_options.pipelineCache == VK_NULL_HANDLE || vk_options.logicalDevice == VK_NULL_HANDLE) {
		return;
	}

	if (vkGetPipelineCacheData(vk_options.logicalDevice, vk_options.pipelineCache, &dataSize, NULL) != VK_SUCCESS || dataSize == 0) {
		return;
	}

	data = Q_malloc(dataSize);
	if (vkGetPipelineCacheData(vk_options.logicalDevice, vk_options.pipelineCache, &dataSize, data) == VK_SUCCESS) {
		FS_WriteFile(VK_PIPELINE_CACHE_FILE, data, (int)dataSize);
	}
	Q_free(data);
}

#endif // RENDERER_OPTION_VULKAN
