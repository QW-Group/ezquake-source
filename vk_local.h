
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

#endif
