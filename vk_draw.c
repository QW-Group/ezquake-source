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

// vk_draw.c

#ifdef RENDERER_OPTION_VULKAN

#include "quakedef.h"

void VK_HudDrawComplete(void)
{

}

void VK_HudPrepareCircles(void)
{
}

void VK_HudDrawCircles(texture_ref tex, int start, int end)
{
}

void VK_HudPrepareImages(void)
{
}

void VK_HudDrawImages(texture_ref tex, int start, int end)
{
}

void VK_HudPreparePolygons(void)
{
}

void VK_HudDrawPolygons(texture_ref tex, int start, int end)
{
}

void VK_HudPrepareLines(void)
{
}

void VK_HudDrawLines(texture_ref ref, int start, int end)
{
}

void VK_DrawImage(float x, float y, float width, float height, float tex_s, float tex_t, float tex_width, float tex_height, byte* color, int flags)
{
}

void VK_DrawRectangle(float x, float y, float width, float height, byte* color)
{
}

void VK_AdjustImages(int first, int last, float x_offset)
{
}

#endif // #ifdef RENDERER_OPTION_VULKAN
