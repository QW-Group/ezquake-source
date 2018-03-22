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

// vk_instance.c
// - Code to create a vulkan instance

#ifdef RENDERER_OPTION_VULKAN

#include <vulkan/vulkan.h>
#include "quakedef.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include "vk_local.h"

// VK_EXT_debug_report extension to get debug messages 
#define EZ_MAX_ADDITIONAL_EXTENSIONS 1

static const char* validationLayers[] = { "VK_LAYER_LUNARG_standard_validation" };

static qbool VK_AddValidationLayers(VkInstanceCreateInfo* createInfo)
{
	VkLayerProperties* availableLayers = NULL;

	createInfo->enabledLayerCount = 0;
	if (COM_CheckParm(cmdline_param_developer_mode)) {
		unsigned int layerCount, j;
		int i;
		qbool available = true;
		VkResult result;

		result = vkEnumerateInstanceLayerProperties(&layerCount, NULL);
		if (result != VK_SUCCESS) {
			return false;
		}

		availableLayers = Q_malloc(sizeof(availableLayers[0]) * layerCount);
		result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);
		if (result != VK_SUCCESS) {
			Q_free(availableLayers);
			return false;
		}
		for (i = 0; i < sizeof(validationLayers) / sizeof(validationLayers[0]); ++i) {
			qbool found = false;
			for (j = 0; j < layerCount && !found; ++j) {
				found |= !strcmp(availableLayers[j].layerName, validationLayers[i]);
			}
			available &= found;
		}
		Q_free(availableLayers);

		if (available) {
			createInfo->ppEnabledLayerNames = validationLayers;
			createInfo->enabledLayerCount = sizeof(validationLayers) / sizeof(validationLayers[0]);
			return true;
		}
	}

	return false;
}

qbool VK_CreateInstance(SDL_Window* window, VkInstance* instance)
{
	qbool debugCallback = false;
	VkInstanceCreateInfo createInfo = { 0 };
	VkApplicationInfo appInfo = { 0 };
	VkResult result;
	uint32_t extensionCount = 0;
	const char** extensionStrings;

	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "ezQuake";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "?";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, NULL)) {
		Con_Printf("vulkan:GetInstanceExtensions() failed\n");
		return false;
	}

	extensionStrings = Q_malloc(sizeof(extensionStrings[0]) * (extensionCount + EZ_MAX_ADDITIONAL_EXTENSIONS));
	if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensionStrings)) {
		Con_Printf("vulkan:GetInstanceExtensions() failed\n");
		return false;
	}

	if (VK_AddValidationLayers(&createInfo)) {
		// Add VK_EXT_DEBUG_REPORT_EXTENSION_NAME
		extensionStrings[extensionCount++] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
		Con_Printf("vulkan: validation layers available\n");
		debugCallback = true;
	}

	createInfo.enabledExtensionCount = extensionCount;
	createInfo.ppEnabledExtensionNames = extensionStrings;

	result = vkCreateInstance(&createInfo, NULL, instance);
	Q_free((void*)extensionStrings);
	if (result != VK_SUCCESS) {
		*instance = NULL;
		Con_Printf("vulkan:GetInstanceExtensions() failed\n");
		return false;
	}

	Con_Printf("Vulkan initialised successfully!");

	if (debugCallback) {
		VK_InitialiseDebugCallback(*instance);
	}

	return true;
}

#endif // RENDERER_OPTION_VULKAN
