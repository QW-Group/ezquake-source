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

// vk_debug.c
// - Vulkan debug callback

#ifdef RENDERER_OPTION_VULKAN

#include <vulkan/vulkan.h>
#include "quakedef.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include "vk_local.h"

static qbool callbackRegistered = false;
static VkDebugReportCallbackEXT callbackHandle;

static VKAPI_ATTR VkBool32 VKAPI_CALL VK_DebugCallback
(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT objType,
	uint64_t obj,
	size_t location,
	int32_t code,
	const char* layerPrefix,
	const char* msg,
	void* userData
)
{
	Con_Printf("vulkan: %s\n", msg);

	return VK_FALSE;
}

void VK_InitialiseDebugCallback(VkInstance instance)
{
	EZ_VKFUNC_DECL_LOAD(instance, vkCreateDebugReportCallbackEXT);

	if (qvkCreateDebugReportCallbackEXT) {
		VkDebugReportCallbackCreateInfoEXT createInfo = { 0 };
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
		createInfo.pfnCallback = VK_DebugCallback;

		if (qvkCreateDebugReportCallbackEXT(instance, &createInfo, NULL, &callbackHandle) == VK_SUCCESS) {
			callbackRegistered = true;
			Con_Printf("Callback registered\n");
		}
		else {
			Con_Printf("Callback registration failed\n");
		}
	}
	else {
		Con_Printf("Callback not supported\n");
	}
}

void VK_ShutdownDebugCallback(VkInstance instance)
{
	if (callbackRegistered) {
		EZ_VKFUNC_DECL_LOAD(instance, vkDestroyDebugReportCallbackEXT);

		if (qvkDestroyDebugReportCallbackEXT) {
			qvkDestroyDebugReportCallbackEXT(instance, callbackHandle, NULL);
		}
		callbackRegistered = false;
	}
}

#endif // RENDERER_OPTION_VULKAN
