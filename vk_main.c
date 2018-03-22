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

// vk_main.c
// - Main entry point for Vulkan

#ifdef RENDERER_OPTION_VULKAN

#include <vulkan/vulkan.h>
#include "quakedef.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include "vk_local.h"

vk_options_t vk_options;

qbool VK_Initialise(SDL_Window* window)
{
	memset(&vk_options, 0, sizeof(vk_options));

	if (!VK_CreateInstance(window, &vk_options.instance)) {
		return false;
	}

	if (!VK_CreateWindowSurface(window, vk_options.instance, &vk_options.surface)) {
		VK_Shutdown();
		return false;
	}

	if (!VK_SelectPhysicalDevice(vk_options.instance, vk_options.surface)) {
		VK_Shutdown();
		return false;
	}

	if (!VK_CreateLogicalDevice(vk_options.instance)) {
		VK_Shutdown();
		return false;
	}

	if (!VK_CreateSwapChain(window, vk_options.instance, vk_options.surface)) {
		VK_Shutdown();
		return false;
	}

	Con_Printf("Vulkan initialised successfully\n");
	return true;
}

void VK_Shutdown(void)
{
	VK_DestroySwapChain();
	
	if (vk_options.instance) {
		VK_DestroyWindowSurface(vk_options.instance, vk_options.surface);
		VK_ShutdownDebugCallback(vk_options.instance);
		vkDestroyInstance(vk_options.instance, NULL);
	}

	memset(&vk_options, 0, sizeof(vk_options));
}

#endif
