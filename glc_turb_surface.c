/*
Copyright (C) 1996-1997 Id Software, Inc.

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
// glc_turb_surface.c: surface-related refresh code
#ifdef RENDERER_OPTION_CLASSIC_OPENGL

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "glc_local.h"
#include "rulesets.h"
#include "utils.h"
#include "r_brushmodel.h"
#include "r_renderer.h"
#include "tr_types.h"
#include "r_program.h"

extern msurface_t* waterchain;
void GLC_EmitWaterPoly(msurface_t* fa);

#define TURBFLAGS_FLATCOLOR           (1 << 0)
#define TURBFLAGS_FOG_LINEAR          (1 << 1)
#define TURBFLAGS_FOG_EXP             (1 << 2)
#define TURBFLAGS_FOG_EXP2            (1 << 3)
#define TURBFLAGS_FOG_ENABLED         (TURBFLAGS_FOG_LINEAR | TURBFLAGS_FOG_EXP | TURBFLAGS_FOG_EXP2)

qbool GLC_TurbSurfaceProgramCompile(void)
{
	extern cvar_t r_fastturb;
	int option =
		(r_fastturb.integer ? TURBFLAGS_FLATCOLOR : 0) |
		(r_refdef2.fog_calculation == fogcalc_linear ? TURBFLAGS_FOG_LINEAR : 0) |
		(r_refdef2.fog_calculation == fogcalc_exp ? TURBFLAGS_FOG_EXP : 0) |
		(r_refdef2.fog_calculation == fogcalc_exp2 ? TURBFLAGS_FOG_EXP2 : 0);

	if (R_ProgramRecompileNeeded(r_program_turb_glc, option)) {
		char included_definitions[512];

		included_definitions[0] = '\0';

		if (option & TURBFLAGS_FLATCOLOR) {
			strlcat(included_definitions, "#define FLAT_COLOR\n", sizeof(included_definitions));
		}
		if (option & TURBFLAGS_FOG_ENABLED) {
			strlcat(included_definitions, "#define DRAW_FOG\n", sizeof(included_definitions));
			if (option & TURBFLAGS_FOG_LINEAR) {
				strlcat(included_definitions, "#define FOG_LINEAR\n", sizeof(included_definitions));
			}
			else if (option & TURBFLAGS_FOG_EXP) {
				strlcat(included_definitions, "#define FOG_EXP\n", sizeof(included_definitions));
			}
			else if (option & TURBFLAGS_FOG_EXP2) {
				strlcat(included_definitions, "#define FOG_EXP2\n", sizeof(included_definitions));
			}
		}

		R_ProgramCompileWithInclude(r_program_turb_glc, included_definitions);
		R_ProgramUniform1i(r_program_uniform_turb_glc_texSampler, 0);
		R_ProgramSetCustomOptions(r_program_turb_glc, option);
	}

	if (option & TURBFLAGS_FOG_ENABLED) {
		R_ProgramUniform3fv(r_program_uniform_turb_glc_fog_color, r_refdef2.fog_color);

		if (option & TURBFLAGS_FOG_LINEAR) {
			R_ProgramUniform1f(r_program_uniform_turb_glc_fog_minZ, r_refdef2.fog_linear_start);
			R_ProgramUniform1f(r_program_uniform_turb_glc_fog_maxZ, r_refdef2.fog_linear_end);
		}
		else {
			R_ProgramUniform1f(r_program_uniform_turb_glc_fog_density, r_refdef2.fog_density);
		}
	}

	return R_ProgramReady(r_program_turb_glc);
}

static void GLC_DrawWaterSurfaces_Program(void)
{
	qbool use_vbo = buffers.supported && modelIndexes;
	msurface_t* fa;
	texture_ref prev_tex = null_texture_reference;
	int index_count = 0;
	float water_alpha = r_refdef2.wateralpha;
	qbool textured = !(R_ProgramCustomOptions(r_program_turb_glc) & TURBFLAGS_FLATCOLOR);

	R_ProgramUse(r_program_turb_glc);
	if (textured) {
		R_ProgramUniform1f(r_program_uniform_turb_glc_time, cl.time);
		R_ProgramUniform1f(r_program_uniform_turb_glc_alpha, water_alpha);
	}

	for (fa = waterchain; fa; fa = fa->texturechain) {
		glpoly_t *p;

		if (!R_TextureReferenceEqual(fa->texinfo->texture->gl_texturenum, prev_tex)) {
			prev_tex = fa->texinfo->texture->gl_texturenum;

			if (index_count) {
				GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
				index_count = 0;
			}
			if (textured) {
				renderer.TextureUnitBind(0, prev_tex);
			}
			else {
				byte* base = SurfaceFlatTurbColor(fa->texinfo->texture);
				float color[4];

				color[0] = (base[0] / 255.0f) * water_alpha;
				color[1] = (base[1] / 255.0f) * water_alpha;
				color[2] = (base[2] / 255.0f) * water_alpha;
				color[3] = water_alpha;

				R_ProgramUniform4fv(r_program_uniform_turb_glc_color, color);
			}
		}

		if (use_vbo) {
			for (p = fa->polys; p; p = p->next) {
				index_count = GLC_DrawIndexedPoly(p, modelIndexes, modelIndexMaximum, index_count);
			}
		}
		else {
			GLC_EmitWaterPoly(fa);
		}
	}

	if (index_count) {
		GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
	}
	R_ProgramUse(r_program_none);
}

static void GLC_DrawWaterSurfaces_Immediate(void)
{
	msurface_t *s;

	for (s = waterchain; s; s = s->texturechain) {
		renderer.TextureUnitBind(0, s->texinfo->texture->gl_texturenum);

		GLC_EmitWaterPoly(s);
	}
}

void GLC_DrawWaterSurfaces(void)
{
	extern cvar_t gl_program_turbsurfaces;

	if (!waterchain) {
		return;
	}

	R_TraceEnterRegion(__func__, true);
	GLC_StateBeginWaterSurfaces();

	if (gl_program_turbsurfaces.integer && GL_Supported(R_SUPPORT_RENDERING_SHADERS) && GLC_TurbSurfaceProgramCompile()) {
		GLC_DrawWaterSurfaces_Program();
	}
	else {
		GLC_DrawWaterSurfaces_Immediate();
	}

	R_TraceLeaveRegion(true);

	waterchain = NULL;
}

#endif // #ifdef RENDERER_OPTION_CLASSIC_OPENGL
