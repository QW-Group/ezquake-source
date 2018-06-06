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

//VULT RIPPLE : Not sure where this came from first, but I've seen in it more than one engine
//I got this one from the QMB engine though
static void GLC_ApplyTurbRippleVertex(msurface_t* surf, float* v)
{
	if (r_refdef2.turb_ripple && surf->texinfo->texture->turbType != TEXTURE_TURB_TELE) {
		GLC_Vertex3f(v[0], v[1], v[2] + r_refdef2.turb_ripple * sin(v[0] * 0.02 + r_refdef2.time) * sin(v[1] * 0.02 + r_refdef2.time) * sin(v[2] * 0.02 + r_refdef2.time));
	}
	else {
		GLC_Vertex3fv(v);
	}
}

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
		for (p = r_refdef2.turb_ripple ? fa->subdivided : fa->polys; p; p = p->next) {
			GLC_Begin(GL_TRIANGLE_STRIP);
			for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
				GLC_ApplyTurbRippleVertex(fa, v);
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
				GLC_ApplyTurbRippleVertex(fa, v);
			}
			GLC_End();
		}
	}
}

// Caustics

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

void GLC_EmitCausticsPolys(qbool use_vbo)
{
	glpoly_t *p;
	int i;
	float s, t, *v;
	extern glpoly_t *caustics_polys;

	if (!caustics_polys) {
		return;
	}

	GLC_StateBeginCausticsPolys();

	renderer.TextureUnitBind(0, underwatertexture);
	for (p = caustics_polys; p; p = p->caustics_chain) {
		GLC_Begin(GL_TRIANGLE_STRIP);
		for (i = 0; i < p->numverts; ++i) {
			GLC_CalcCausticTexCoords(p->verts[i], &s, &t);
			glTexCoord2f(s, t);
			GLC_Vertex3fv(p->verts[i]);
		}
		GLC_End();
	}

	caustics_polys = NULL;
}
