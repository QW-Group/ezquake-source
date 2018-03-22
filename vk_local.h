
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
		int imageCount;
	} swapChain;
} vk_options_t;

extern vk_options_t vk_options;

#endif
