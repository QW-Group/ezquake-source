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

#ifndef EZQUAKE_R_STATE_HEADER
#define EZQUAKE_R_STATE_HEADER

// rendering state
typedef enum {
	r_depthfunc_less,
	r_depthfunc_equal,
	r_depthfunc_lessorequal,
	r_depthfunc_count
} r_depthfunc_t;

typedef enum {
	r_cullface_front,
	r_cullface_back,

	r_cullface_count
} r_cullface_t;

typedef enum {
	r_blendfunc_overwrite,
	r_blendfunc_additive_blending,
	r_blendfunc_premultiplied_alpha,
	r_blendfunc_src_dst_color_dest_zero,
	r_blendfunc_src_dst_color_dest_one,
	r_blendfunc_src_dst_color_dest_src_color,
	r_blendfunc_src_zero_dest_one_minus_src_color,
	r_blendfunc_src_zero_dest_src_color,

	r_blendfunc_count
} r_blendfunc_t;

typedef enum {
	r_polygonmode_fill,
	r_polygonmode_line,

	r_polygonmode_count
} r_polygonmode_t;

typedef enum {
	r_polygonoffset_disabled,
	r_polygonoffset_standard,
	r_polygonoffset_outlines,

	r_polygonoffset_count
} r_polygonoffset_t;

typedef struct {
	struct {
		r_depthfunc_t func;
		double nearRange;
		double farRange;
		qbool test_enabled;
		qbool mask_enabled;
	} depth;

	// FIXME: currentWidth & currentHeight should be initialised to dimensions of window
	int currentViewportX, currentViewportY;
	int currentViewportWidth, currentViewportHeight;

	qbool framebuffer_srgb;

	struct {
		qbool smooth;
		float width;
		qbool flexible_width;
	} line;

	struct {
		qbool enabled;
	} fog;

	struct {
		r_cullface_t mode;
		qbool enabled;
	} cullface;

	r_blendfunc_t blendFunc;

	struct {
		r_polygonoffset_t option;
		float factor;
		float units;
		qbool fillEnabled;
		qbool lineEnabled;
	} polygonOffset;

	r_polygonmode_t polygonMode;
	float clearColor[4];
	qbool blendingEnabled;
	qbool alphaTestingEnabled;
} rendering_state_t;

void R_InitRenderingState(rendering_state_t* state, qbool default_state);
void R_ApplyRenderingState(rendering_state_t* state);

#endif // EZQUAKE_R_STATE_HEADER
