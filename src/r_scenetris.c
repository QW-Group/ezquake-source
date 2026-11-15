/*
Copyright (C) 2026 ezQuake team

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

// Scene triangles: a per-view list of textured, vertex-coloured triangles and lines

#include "quakedef.h"
#include "r_state.h"
#include "r_texture.h"
#include "r_renderer.h"
#include "tr_types.h"
#include "r_scenetris.h"

// A run opens whenever texture, blend or flags change; scripted particles open about one per
// visible type.
#define SCENETRIS_MAX_RUNS 4096
#define SCENETRIS_PASSES   3

typedef struct scenetris_run_s {
	texture_ref texture;
	scenetris_blend_t blend;
	int flags;
	int first;
	int count;
} scenetris_run_t;

// Passes: blends that darken the frame, then alpha-blended, then additive. Fog fades the
// source factor each blend weights by: alpha, colour or both.
static const struct {
	int pass;
	r_state_id state;
	qbool fog_rgb;
	qbool fog_alpha;
} scenetris_blends[] = {
	{ 1, r_state_scenetris_alpha, false, true },         // scenetris_blend_alpha
	{ 1, r_state_scenetris_colour, true, false },        // scenetris_blend_colour
	{ 2, r_state_scenetris_add_alpha, false, true },     // scenetris_blend_add_alpha
	{ 2, r_state_scenetris_add_colour, true, false },    // scenetris_blend_add_colour
	{ 0, r_state_scenetris_subtract, true, false },      // scenetris_blend_subtract
	{ 0, r_state_scenetris_invmod_alpha, false, true },  // scenetris_blend_invmod_alpha
	{ 0, r_state_scenetris_invmod_colour, true, false }, // scenetris_blend_invmod_colour
	{ 2, r_state_scenetris_premul, true, true },         // scenetris_blend_premul
};

#ifdef C_ASSERT
C_ASSERT(sizeof(scenetris_blends) / sizeof(scenetris_blends[0]) == scenetris_blend_count);
#endif

// Allocated by the first call that submits anything, so clients that never do pay nothing
static scenetris_vert_t *scenetris_verts;
static scenetris_run_t *scenetris_runs;
static int scenetris_numverts;
static int scenetris_numruns;

// Returns room for numverts vertices, drawn this view with texture (solid white when null),
// blend and flags, or NULL when the count isn't whole triangles or lines or the view is full.
// vid_restart reloads textures into other slots, so look texture up again after one.
scenetris_vert_t *R_SceneTrisAlloc(texture_ref texture, scenetris_blend_t blend, int flags, int numverts)
{
	scenetris_run_t *run = (scenetris_numruns ? &scenetris_runs[scenetris_numruns - 1] : NULL);
	scenetris_vert_t *verts;

	if ((unsigned int)blend >= scenetris_blend_count || numverts <= 0 || numverts % ((flags & SCENETRIS_LINES) ? 2 : 3)) {
		return NULL;
	}
	if (numverts > SCENETRIS_MAX_VERTS - scenetris_numverts) {
		return NULL;
	}
	if (!scenetris_verts) {
		scenetris_verts = Q_malloc(SCENETRIS_MAX_VERTS * sizeof(scenetris_verts[0]));
		scenetris_runs = Q_malloc(SCENETRIS_MAX_RUNS * sizeof(scenetris_runs[0]));
	}
	if (!run || !R_TextureReferenceEqual(run->texture, texture) || run->blend != blend || run->flags != flags) {
		if (scenetris_numruns >= SCENETRIS_MAX_RUNS) {
			return NULL;
		}
		run = &scenetris_runs[scenetris_numruns++];
		run->texture = texture;
		run->blend = blend;
		run->flags = flags;
		run->first = scenetris_numverts;
		run->count = 0;
	}

	verts = &scenetris_verts[scenetris_numverts];
	run->count += numverts;
	scenetris_numverts += numverts;
	return verts;
}

// The factor the GLSL fog functions in gl_program.c mix by at fog depth z, 1 meaning no fog
static float R_SceneTrisFogFactor(float z)
{
	float range = r_refdef2.fog_linear_end - r_refdef2.fog_linear_start;
	float factor = 1;

	if (r_refdef2.fog_calculation == fogcalc_linear) {
		factor = (range ? (r_refdef2.fog_linear_end - z) / range : z < r_refdef2.fog_linear_end);
	}
	else if (r_refdef2.fog_calculation == fogcalc_exp) {
		factor = exp(-r_refdef2.fog_density * z);
	}
	else if (r_refdef2.fog_calculation == fogcalc_exp2) {
		factor = exp(-r_refdef2.fog_density * z * z);
	}

	return bound(0, factor, 1);
}

// Fades the view's vertices on the CPU, so neither renderer needs fog state for them and both
// look the same. Colours are clamped first as the renderers clamp them anyway, and an overbright
// colour would otherwise not start fading until the fog factor fell below its inverse.
static void R_SceneTrisFadeForFog(void)
{
	float near_z = R_NearPlaneZ();
	float far_z = R_FarPlaneZ();
	float view_depth = DotProduct(r_origin, vpn);
	int i, j, k;

	for (i = 0; i < scenetris_numruns; ++i) {
		const scenetris_run_t *run = &scenetris_runs[i];
		qbool fog_rgb = scenetris_blends[run->blend].fog_rgb;
		qbool fog_alpha = scenetris_blends[run->blend].fog_alpha;

		for (j = run->first; j < run->first + run->count; ++j) {
			scenetris_vert_t *vert = &scenetris_verts[j];
			float eye_depth = DotProduct(vert->position, vpn) - view_depth;
			// fogFragDepth(): eye depth with reversed depth, else window depth times eye depth
			float fog = R_SceneTrisFogFactor(glConfig.reversed_depth ? eye_depth : far_z * (eye_depth - near_z) / (far_z - near_z));

			if (fog_rgb) {
				for (k = 0; k < 3; ++k) {
					vert->color[k] = bound(0, vert->color[k], 1) * fog;
				}
			}
			if (fog_alpha) {
				vert->color[3] = bound(0, vert->color[3], 1) * fog;
			}
		}
	}
}

// Readies the view's vertices for R_SceneTrisDraw and returns how many there are. Call once per
// view: it fades the vertices for fog in place.
int R_SceneTrisPrepare(void)
{
	if (scenetris_numverts && r_refdef2.fog_render && r_refdef2.fog_calculation != fogcalc_none) {
		R_SceneTrisFadeForFog();
	}

	return scenetris_numverts;
}

// The view's vertices, at an address that stays the same once anything has been submitted
scenetris_vert_t *R_SceneTrisVertices(void)
{
	return scenetris_verts;
}

// Has draw issue each run, first counting from R_SceneTrisVertices(). Runs keep submission
// order within a pass and aren't sorted by depth.
void R_SceneTrisDraw(scenetris_draw_func_t draw)
{
	int pass, i;

	for (pass = 0; pass < SCENETRIS_PASSES; ++pass) {
		for (i = 0; i < scenetris_numruns; ++i) {
			const scenetris_run_t *run = &scenetris_runs[i];

			if (scenetris_blends[run->blend].pass != pass) {
				continue;
			}

			R_ApplyRenderingState(scenetris_blends[run->blend].state);
			renderer.TextureUnitBind(0, R_TextureReferenceIsValid(run->texture) ? run->texture : solidwhite_texture);
			if (run->flags & SCENETRIS_DECAL) {
				R_CustomPolygonOffset(r_polygonoffset_decal);
			}
			draw((run->flags & SCENETRIS_LINES) != 0, run->first, run->count);
			if (run->flags & SCENETRIS_DECAL) {
				R_CustomPolygonOffset(r_polygonoffset_disabled);
			}
		}
	}
}

void R_SceneTrisClear(void)
{
	scenetris_numverts = scenetris_numruns = 0;
}
