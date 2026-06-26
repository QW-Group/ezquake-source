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

#include <SDL.h>
#include <SDL_vulkan.h>

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

// Anti-lag / low-latency extensions are optional, vendor-specific, and never
// disqualify a device when missing -- unlike requiredDeviceExtensions above,
// where any miss rules the device out entirely.
static const char* optionalAntiLagExtensions[] = { VK_AMD_ANTI_LAG_EXTENSION_NAME, VK_NV_LOW_LATENCY_2_EXTENSION_NAME };

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
// device -- it just reports which of optionalAntiLagExtensions[] are present,
// so VK_CreateLogicalDevice can enable exactly those by name. AMD's anti-lag
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
	VkPresentModeKHR preferredModes[] = {
		VK_PRESENT_MODE_MAILBOX_KHR,       // triple buffered
		VK_PRESENT_MODE_IMMEDIATE_KHR,     // tearing
		VK_PRESENT_MODE_FIFO_RELAXED_KHR,
		VK_PRESENT_MODE_FIFO_KHR
	};
	VkPresentModeKHR* presentationModes;
	uint32_t count;
	uint32_t i, j;

	// This is guaranteed to be supported
	if (r_swapInterval.integer) {
		*best = VK_PRESENT_MODE_FIFO_KHR;
		return true;
	}

	if (vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, NULL) != VK_SUCCESS) {
		return false;
	}

	presentationModes = Q_malloc(count * sizeof(presentationModes[0]));
	if (vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, presentationModes) != VK_SUCCESS) {
		Q_free(presentationModes);
		return false;
	}

	for (i = 0; i < sizeof(preferredModes) / sizeof(preferredModes[0]); ++i) {
		for (j = 0; j < count; ++j) {
			if (preferredModes[i] == presentationModes[j]) {
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
		// Find optimal format.... FIXME (what are we really doing here?)
		uint32_t i;

		for (i = 0; i < num_formats; ++i) {
			if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM && formats[i].colorSpace == req_color_space) {
				preferred_format->colorSpace = req_color_space;
				preferred_format->format = VK_FORMAT_B8G8R8A8_UNORM;
				break;
			}
		}

		if (i >= num_formats) {
			preferred_format->colorSpace = formats[0].colorSpace;
			preferred_format->format = formats[0].format;
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

	if (vk_options.supportsAmdAntiLag) {
		// Re-using antiLagFeatures (already populated above) to actually
		// request the feature be enabled on the logical device, same struct
		// instance just attached to a different sType-chain root this time.
		antiLagFeatures.pNext = NULL;
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

	return true;
}

#endif // RENDERER_OPTION_VULKAN
