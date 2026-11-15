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

#ifndef EZQUAKE_R_SCENETRIS_HEADER
#define EZQUAKE_R_SCENETRIS_HEADER

typedef enum {
	scenetris_blend_alpha,         // GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA
	scenetris_blend_colour,        // GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR
	scenetris_blend_add_alpha,     // GL_SRC_ALPHA, GL_ONE
	scenetris_blend_add_colour,    // GL_SRC_COLOR, GL_ONE
	scenetris_blend_subtract,      // GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR
	scenetris_blend_invmod_alpha,  // GL_ZERO, GL_ONE_MINUS_SRC_ALPHA
	scenetris_blend_invmod_colour, // GL_ZERO, GL_ONE_MINUS_SRC_COLOR
	scenetris_blend_premul,        // GL_ONE, GL_ONE_MINUS_SRC_ALPHA
	scenetris_blend_count
} scenetris_blend_t;

#define SCENETRIS_LINES 1 // vertices are line pairs, not triangles
#define SCENETRIS_DECAL 2 // lies on a surface; every run gets the decal polygon offset regardless

typedef struct scenetris_vert_s {
	float position[3];
	float texcoord[2];
	float color[4]; // straight RGBA, not clamped by the caller
} scenetris_vert_t;

scenetris_vert_t *R_SceneTrisAlloc(texture_ref texture, scenetris_blend_t blend, int flags, int numverts);

// Renderer backends
#define SCENETRIS_MAX_VERTS (65536 * 6)

typedef void (*scenetris_draw_func_t)(qbool lines, int first, int count);

int R_SceneTrisPrepare(void);
scenetris_vert_t *R_SceneTrisVertices(void);
void R_SceneTrisDraw(scenetris_draw_func_t draw);
void R_SceneTrisClear(void);

#endif // EZQUAKE_R_SCENETRIS_HEADER
