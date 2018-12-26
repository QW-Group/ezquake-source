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

#include "quakedef.h"
#include "vx_stuff.h"
#include "gl_model.h"
#include "gl_local.h"
#include "glsl/constants.glsl"
#include "r_texture.h"
#include "glc_local.h"
#include "r_renderer.h"
#include "r_brushmodel.h"
#include "r_program.h"
#include "tr_types.h"

#define TURBSINSIZE 128
#define TURBSCALE ((float) TURBSINSIZE / (2 * M_PI))

static byte turbsin[TURBSINSIZE] = {
	127, 133, 139, 146, 152, 158, 164, 170, 176, 182, 187, 193, 198, 203, 208, 213, 
	217, 221, 226, 229, 233, 236, 239, 242, 245, 247, 249, 251, 252, 253, 254, 254, 
	255, 254, 254, 253, 252, 251, 249, 247, 245, 242, 239, 236, 233, 229, 226, 221, 
	217, 213, 208, 203, 198, 193, 187, 182, 176, 170, 164, 158, 152, 146, 139, 133, 
	127, 121, 115, 108, 102, 96, 90, 84, 78, 72, 67, 61, 56, 51, 46, 41, 
	37, 33, 28, 25, 21, 18, 15, 12, 9, 7, 5, 3, 2, 1, 0, 0, 
	0, 0, 0, 1, 2, 3, 5, 7, 9, 12, 15, 18, 21, 25, 28, 33, 
	37, 41, 46, 51, 56, 61, 67, 72, 78, 84, 90, 96, 102, 108, 115, 121, 
};

__inline static float SINTABLE_APPROX(float time) {
	float sinlerpf, lerptime, lerp;
	int sinlerp1, sinlerp2;

	sinlerpf = time * TURBSCALE;
	sinlerp1 = floor(sinlerpf);
	sinlerp2 = sinlerp1 + 1;
	lerptime = sinlerpf - sinlerp1;

	lerp =	turbsin[sinlerp1 & (TURBSINSIZE - 1)] * (1 - lerptime) +
		turbsin[sinlerp2 & (TURBSINSIZE - 1)] * lerptime;
	return -8 + 16 * lerp / 255.0;
}

void GLC_EmitWaterPoly(msurface_t* fa)
{
	extern cvar_t r_fastturb;
	glpoly_t *p;
	float* v;
	int i;

	if (r_fastturb.integer) {
		byte* col = SurfaceFlatTurbColor(fa->texinfo->texture);
		byte color[4] = { col[0], col[1], col[2], 255 };

		GLC_StateBeginFastTurbPoly(color);
		for (p = fa->polys; p; p = p->next) {
			GLC_Begin(GL_TRIANGLE_STRIP);
			for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
				GLC_Vertex3fv(v);
			}
			GLC_End();
		}
	}
	else {
		renderer.TextureUnitBind(0, fa->texinfo->texture->gl_texturenum);
		for (p = fa->subdivided; p; p = p->next) {
			GLC_Begin(GL_TRIANGLE_STRIP);
			for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
				float os = v[3];
				float ot = v[4];
				// meag: v[5] = 8 * sin(2t), v[6] = 8 * sin(2s)
				//       v[7] = 8 * cos(2t), v[8] = 8 * cos(2s)
				// was: SINTABLE_APPROX(ot * 2 + r_refdef2.time)) / 64.0f, SINTABLE_APPROX(os * 2 + r_refdef2.time)) / 64.0f
				float s = os + v[5] * r_refdef2.cos_time + v[7] * r_refdef2.sin_time;
				float t = ot + v[6] * r_refdef2.cos_time + v[8] * r_refdef2.sin_time;

				glTexCoord2f(s, t);
				GLC_Vertex3fv(v);
			}
			GLC_End();
		}
	}
}

// Caustics
static qbool GLC_CausticsProgramCompile(void)
{
	if (R_ProgramRecompileNeeded(r_program_caustics_glc, 0)) {
		R_ProgramCompile(r_program_caustics_glc);
		R_ProgramUniform1i(r_program_uniform_caustics_glc_texSampler, 0);
		R_ProgramSetCustomOptions(r_program_caustics_glc, 0);
	}

	return R_ProgramReady(r_program_caustics_glc);
}

static void GLC_CalcCausticTexCoords(float *v, float *s, float *t)
{
	float os, ot;

	os = v[3];
	ot = v[4];

	*s = os + SINTABLE_APPROX(0.465 * (r_refdef2.time + ot));
	*s *= -3 * (0.5 / 64);

	*t = ot + SINTABLE_APPROX(0.465 * (r_refdef2.time + os));
	*t *= -3 * (0.5 / 64);
}

static void GLC_EmitCausticPolys_Program(glpoly_t* caustics_polys)
{
	unsigned int index_count = 0;
	glpoly_t *p;

	R_ProgramUse(r_program_caustics_glc);
	R_ProgramUniform1f(r_program_uniform_caustics_glc_time, cl.time);

	for (p = caustics_polys; p; p = p->caustics_chain) {
		index_count = GLC_DrawIndexedPoly(p, modelIndexes, modelIndexMaximum, index_count);
	}

	if (index_count) {
		GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
	}
	R_ProgramUse(r_program_none);
}

static void GLC_EmitCausticPolys_Immediate(glpoly_t* caustics_polys)
{
	glpoly_t *p;
	float s, t;
	int i;

	for (p = caustics_polys; p; p = p->caustics_chain) {
		GLC_Begin(GL_TRIANGLE_STRIP);
		for (i = 0; i < p->numverts; ++i) {
			GLC_CalcCausticTexCoords(p->verts[i], &s, &t);
			glTexCoord2f(s, t);
			GLC_Vertex3fv(p->verts[i]);
		}
		GLC_End();
	}
}

void GLC_EmitCausticsPolys(void)
{
	extern glpoly_t *caustics_polys;
	extern cvar_t gl_program_turbsurfaces;
	qbool use_vbo = buffers.supported && modelIndexes;

	if (!caustics_polys) {
		return;
	}

	R_TraceEnterRegion(__FUNCTION__, true);
	GLC_StateBeginCausticsPolys();

	renderer.TextureUnitBind(0, underwatertexture);

	if (use_vbo && gl_program_turbsurfaces.integer && GL_Supported(R_SUPPORT_RENDERING_SHADERS) && GLC_CausticsProgramCompile()) {
		GLC_EmitCausticPolys_Program(caustics_polys);
	}
	else {
		GLC_EmitCausticPolys_Immediate(caustics_polys);
	}

	caustics_polys = NULL;
	R_TraceLeaveRegion(true);
}
