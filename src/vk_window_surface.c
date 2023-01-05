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

// vk_window_surface.c
// - Window surfaces

#ifdef RENDERER_OPTION_VULKAN

#include <vulkan/vulkan.h>
#include "quakedef.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include "vk_local.h"

qbool VK_CreateWindowSurface(SDL_Window* window, VkInstance instance, VkSurfaceKHR* surface)
{
	surface = VK_NULL_HANDLE;

	if (SDL_Vulkan_CreateSurface(window, instance, surface) != SDL_TRUE) {
		return false;
	}

	return true;
}

void VK_DestroyWindowSurface(VkInstance instance, VkSurfaceKHR surface)
{
	if (surface != VK_NULL_HANDLE) {
		vkDestroySurfaceKHR(instance, surface, NULL);
		surface = VK_NULL_HANDLE;
	}
}

#endif
