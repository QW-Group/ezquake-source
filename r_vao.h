/*
Copyright (C) 2018 ezQuake team.

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

#ifndef EZQUAKE_R_VAO_HEADER
#define EZQUAKE_R_VAO_HEADER

#include "r_buffers.h"

typedef enum {
	vao_none,
	vao_aliasmodel,
	vao_brushmodel,
	vao_3dsprites,
	vao_hud_circles,
	vao_hud_images,
	vao_hud_lines,
	vao_hud_polygons,
	vao_postprocess,

	// non-shader (classic OpenGL) only
	vao_aliasmodel_powerupshell,
	vao_brushmodel_details,
	vao_brushmodel_lightmap_pass,
	vao_brushmodel_lm_unit1,
	vao_brushmodel_simpletex,
	vao_hud_images_non_glsl,

	vao_count
} r_vao_id;

void R_GenVertexArray(r_vao_id vao);
void R_BindVertexArray(r_vao_id vao);
qbool R_VertexArrayCreated(r_vao_id vao);
void R_InitialiseVAOState(void);
qbool R_VAOBound(void);
void R_BindVertexArrayElementBuffer(r_buffer_id ref);

typedef struct r_vaos_api_s {
	void(*Bind)(r_vao_id vao);
} r_vaos_api_t;

#endif // EZQUAKE_R_VAO_HEADER
