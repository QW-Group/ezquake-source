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

// vk_blending.c
// - Converts internal v_blendfunc_t => Vulkan structures

#ifdef RENDERER_OPTION_VULKAN

#include <vulkan/vulkan.h>
#include "quakedef.h"
#include "r_state.h"

#include "vk_local.h"

void VK_BlendingConfigure(VkPipelineColorBlendStateCreateInfo* info, VkPipelineColorBlendAttachmentState* blending, r_blendfunc_t func)
{
	memset(info, 0, sizeof(*info));

	info->sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	info->logicOpEnable = VK_FALSE;
	info->logicOp = VK_LOGIC_OP_COPY;
	info->attachmentCount = 1;
	info->pAttachments = blending;
	info->blendConstants[0] = info->blendConstants[1] = info->blendConstants[2] = info->blendConstants[3] = 0.0f;

	memset(blending, 0, sizeof(*blending));

	blending->colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blending->colorBlendOp = blending->alphaBlendOp = VK_BLEND_OP_ADD;

	switch (func) {
		case r_blendfunc_overwrite:
			blending->srcColorBlendFactor = blending->srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			blending->dstColorBlendFactor = blending->dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			blending->blendEnable = false;
			break;
		case r_blendfunc_additive_blending:
			blending->srcColorBlendFactor = blending->srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			blending->dstColorBlendFactor = blending->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			blending->blendEnable = true;
			break;
		case r_blendfunc_premultiplied_alpha:
			blending->srcColorBlendFactor = blending->srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			blending->dstColorBlendFactor = blending->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			blending->blendEnable = true;
			break;
		case r_blendfunc_src_dst_color_dest_zero:
			blending->srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
			blending->srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
			blending->dstColorBlendFactor = blending->dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			blending->blendEnable = true;
			break;
		case r_blendfunc_src_dst_color_dest_one:
			blending->srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
			blending->srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
			blending->dstColorBlendFactor = blending->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			blending->blendEnable = true;
			break;
		case r_blendfunc_src_dst_color_dest_src_color:
			blending->srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
			blending->srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
			blending->dstColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
			blending->dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			blending->blendEnable = true;
			break;
		case r_blendfunc_src_zero_dest_one_minus_src_color:
			blending->srcColorBlendFactor = blending->srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			blending->dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
			blending->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			blending->blendEnable = true;
			break;
		case r_blendfunc_src_zero_dest_src_color:
			blending->srcColorBlendFactor = blending->srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			blending->dstColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
			blending->dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			blending->blendEnable = true;
			break;
		case r_blendfunc_src_one_dest_zero:
			blending->srcColorBlendFactor = blending->srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			blending->dstColorBlendFactor = blending->dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			blending->blendEnable = true;
			break;
		case r_blendfunc_src_zero_dest_one:
			blending->srcColorBlendFactor = blending->srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			blending->dstColorBlendFactor = blending->dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			blending->blendEnable = true;
			break;
	}
}

#endif // RENDERER_OPTION_VULKAN
