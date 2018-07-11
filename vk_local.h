/*
Copyright (C) 2018 ezQuake team.

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

#ifndef EZQUAKE_VK_LOCAL_HEADER
#define EZQUAKE_VK_LOCAL_HEADER

#define EZ_VKFUNC_DECL_LOAD(instance, func) PFN_##func q##func = (PFN_##func)vkGetInstanceProcAddr(instance, #func)
#define EZ_VKFUNC_LOAD(instance, func) q##func = (PFN_##func)vkGetInstanceProcAddr(instance, #func)

// vk_main.c
qbool VK_Initialise(SDL_Window* window);
void VK_Shutdown(void);

// vk_instance.c
qbool VK_CreateInstance(SDL_Window* window, VkInstance* instance);

// vk_debug.c
void VK_ShutdownDebugCallback(VkInstance instance);
void VK_InitialiseDebugCallback(VkInstance instance);

// vk_physical_devices.c
qbool VK_SelectPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
uint32_t VK_PhysicalDeviceGraphicsQueueFamilyIndex(void);
uint32_t VK_PhysicalDeviceComputeQueueFamilyIndex(void);
uint32_t VK_PhysicalDevicePresentQueueFamilyIndex(void);
qbool VK_CreateLogicalDevice(VkInstance instance);

// vk_window_surface.c
qbool VK_CreateWindowSurface(SDL_Window* window, VkInstance instance, VkSurfaceKHR* surface);
void VK_DestroyWindowSurface(VkInstance instance, VkSurfaceKHR surface);

// vk_swapchain.c
qbool VK_CreateSwapChain(SDL_Window* window, VkInstance instance, VkSurfaceKHR surface);
void VK_DestroySwapChain(void);

// (common)
typedef struct vk_options_s {
	VkInstance instance;
	VkSurfaceKHR surface;

	VkPhysicalDevice physicalDevice;
	VkPhysicalDeviceFeatures physicalDeviceFeatures;
	VkPhysicalDeviceProperties physicalDeviceProperties;
	uint32_t physicalDeviceGraphicsQueueFamilyIndex;
	uint32_t physicalDeviceComputeQueueFamilyIndex;
	uint32_t physicalDevicePresentQueueFamilyIndex;
	VkPresentModeKHR physicalDevicePresentationMode;
	VkSurfaceFormatKHR physicalDeviceSurfaceFormat;
	VkSurfaceCapabilitiesKHR physicalDeviceSurfaceCapabilities;
	VkDevice logicalDevice;
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	struct {
		VkSwapchainKHR handle;
		VkImage* images;
		VkImageView* imageViews;
		VkExtent2D imageSize;
		int imageCount;
	} swapChain;
} vk_options_t;

extern vk_options_t vk_options;

void VK_PrintGfxInfo(void);

#endif
