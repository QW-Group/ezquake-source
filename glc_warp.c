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

#define TURBSINSIZE 128
#define TURBSCALE ((float) TURBSINSIZE / (2 * M_PI))

byte turbsin[TURBSINSIZE] = {
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

void GLC_DrawFlatPoly(glpoly_t* p)
{
	float* v;
	int i;

	glBegin(GL_POLYGON);
	for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
		glVertex3fv(v);
	}
	glEnd();
}

void GLC_EmitFlatWaterPoly(msurface_t* fa)
{
	glpoly_t *p;
	float *v;
	int i;
	vec3_t nv;

	for (p = fa->polys; p; p = p->next) {
		glBegin(GL_POLYGON);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
			VectorCopy(v, nv);
			nv[2] = v[2] + (bound(0, amf_waterripple.value, 20))*sin(v[0] * 0.02 + r_refdef2.time)*sin(v[1] * 0.02 + r_refdef2.time)*sin(v[2] * 0.02 + r_refdef2.time);
			glVertex3fv(nv);
		}
		glEnd();
	}
}

static void EmitFlatWaterPoly(msurface_t *fa)
{
	glpoly_t *p;

	if (!amf_waterripple.value || !(cls.demoplayback || cl.spectator) || fa->texinfo->texture->turbType == TEXTURE_TURB_TELE) {
		for (p = fa->polys; p; p = p->next) {
			GLC_DrawFlatPoly(p);
		}
	}
	else {
		GLC_EmitFlatWaterPoly(fa);
	}
}

void GLC_EmitWaterPoly(msurface_t* fa, byte* col, float wateralpha)
{
	extern cvar_t r_fastturb;

	GL_DisableMultitexture();

	GL_EnableFog();

	if (r_fastturb.value) {
		byte old_alpha = col[3];

		glDisable(GL_TEXTURE_2D);

		glColor3ubv(col);

		// START shaman FIX /gl_turbalpha + /r_fastturb {
		if (wateralpha < 1.0 && wateralpha >= 0) {
			GL_AlphaBlendFlags(GL_BLEND_ENABLED);
			col[3] = wateralpha * 255;
			glColor4ubv(col); // 1, 1, 1, wateralpha
			GL_TextureEnvMode(GL_MODULATE);
			if (wateralpha < 0.9) {
				GL_DepthMask(GL_FALSE);
			}
		}
		// END shaman FIX /gl_turbalpha + /r_fastturb {

		EmitFlatWaterPoly(fa);

		// START shaman FIX /gl_turbalpha + /r_fastturb {
		if (wateralpha < 1.0 && wateralpha >= 0) {
			GL_TextureEnvMode(GL_REPLACE);
			glColor3ubv(color_white);
			GL_AlphaBlendFlags(GL_BLEND_DISABLED);
			if (wateralpha < 0.9) {
				GL_DepthMask(GL_TRUE);
			}
			col[3] = old_alpha;
		}
		// END shaman FIX /gl_turbalpha + /r_fastturb {

		glEnable(GL_TEXTURE_2D);
		glColor3ubv(color_white);
		// END shaman RFE 1022504
	}
	else {
		glpoly_t *p;
		float *v, s, t, os, ot;
		vec3_t nv;
		int i;

		GL_DisableMultitexture();
		glEnable(GL_TEXTURE_2D);
		GL_BindTextureUnit(GL_TEXTURE0, fa->texinfo->texture->gl_texturenum);
		for (p = fa->polys; p; p = p->next) {
			glBegin(GL_POLYGON);
			for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
				os = v[3];
				ot = v[4];

				s = os + SINTABLE_APPROX(ot * 2 + r_refdef2.time);
				s *= (1.0 / 64);

				t = ot + SINTABLE_APPROX(os * 2 + r_refdef2.time);
				t *= (1.0 / 64);

				//VULT RIPPLE : Not sure where this came from first, but I've seen in it more than one engine
				//I got this one from the QMB engine though
				VectorCopy(v, nv);
				//Over 20 this setting gets pretty cheaty
				if (amf_waterripple.value && (cls.demoplayback || cl.spectator) && fa->texinfo->texture->turbType != TEXTURE_TURB_TELE) {
					nv[2] = v[2] + (bound(0, amf_waterripple.value, 20)) *sin(v[0] * 0.02 + r_refdef2.time)*sin(v[1] * 0.02 + r_refdef2.time)*sin(v[2] * 0.02 + r_refdef2.time);
				}

				glTexCoord2f(s, t);
				glVertex3fv(nv);
			}
			glEnd();
		}
	}

	GL_DisableFog();
}

// Caustics

static void CalcCausticTexCoords(float *v, float *s, float *t) {
	float os, ot;

	os = v[3];
	ot = v[4];

	*s = os + SINTABLE_APPROX(0.465 * (r_refdef2.time + ot));
	*s *= -3 * (0.5 / 64);

	*t = ot + SINTABLE_APPROX(0.465 * (r_refdef2.time + os));
	*t *= -3 * (0.5 / 64);
}

void GLC_EmitCausticsPolys (void) {
	glpoly_t *p;
	int i;
	float s, t, *v;
	extern glpoly_t *caustics_polys;

	GL_BindTextureUnit(GL_TEXTURE0, underwatertexture);
	GL_TextureEnvMode(GL_DECAL);
	GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);

	for (p = caustics_polys; p; p = p->caustics_chain) {
		glBegin(GL_POLYGON);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
			CalcCausticTexCoords(v, &s, &t);

			glTexCoord2f(s, t);
			glVertex3fv(v);
		}
		glEnd();
	}

	GL_TextureEnvMode(GL_REPLACE);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);

	caustics_polys = NULL;
}
