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
// gl_warp.c -- water polygons

#include "quakedef.h"
#include "vx_stuff.h"
#include "gl_model.h"
#include "gl_local.h"
#include "teamplay.h"
#include "utils.h"

extern model_t *loadmodel;
//ISUNDERWATER(TruePointContents(start)
extern cvar_t r_fastturb;

static msurface_t *warpface;

void BoundPoly (int numverts, float *verts, vec3_t mins, vec3_t maxs) {
	int i, j;
	float *v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for (i=0 ; i<numverts ; i++) {
		for (j = 0; j < 3; j++, v++) {
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
	}
}

void GLM_CreateVAOForPoly(glpoly_t *poly);
void GLM_CreateVAOForWarpPoly(msurface_t* surf);

void SubdividePolygon (int numverts, float *verts) {
	int i, j, k, f, b;
	vec3_t mins, maxs, front[64], back[64];
	float m, *v, dist[64], frac, s, t;
	glpoly_t *poly;
	float subdivide_size;	

	if (numverts > 60)
		Sys_Error ("numverts = %i", numverts);

	subdivide_size = max(1, gl_subdivide_size.value);
	BoundPoly (numverts, verts, mins, maxs);

	for (i = 0; i < 3; i++) {
		m = (mins[i] + maxs[i]) * 0.5;
		m = subdivide_size * floor (m / subdivide_size + 0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		v = verts + i;
		for (j = 0; j < numverts; j++, v += 3)
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v -= i;
		VectorCopy (verts, v);

		f = b = 0;
		v = verts;
		for (j = 0; j < numverts; j++, v += 3) {
			if (dist[j] >= 0) {
				VectorCopy (v, front[f]);
				f++;
			}
			if (dist[j] <= 0) {
				VectorCopy (v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j + 1] == 0)
				continue;
			if ( (dist[j] > 0) != (dist[j + 1] > 0) ) {
				// clip point
				frac = dist[j] / (dist[j] - dist[j + 1]);
				for (k = 0; k < 3; k++)
					front[f][k] = back[b][k] = v[k] + frac * (v[3 + k] - v[k]);
				f++;
				b++;
			}
		}
		SubdividePolygon (f, front[0]);
		SubdividePolygon (b, back[0]);
		return;
	}

	poly = (glpoly_t *) Hunk_Alloc (sizeof(glpoly_t) + (numverts - 4) * VERTEXSIZE * sizeof(float));
	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts;
	for (i = 0; i < numverts; i++, verts += 3)	{
		VectorCopy (verts, poly->verts[i]);
		s = DotProduct (verts, warpface->texinfo->vecs[0]);
		t = DotProduct (verts, warpface->texinfo->vecs[1]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;
	}
}

// Breaks a polygon up along axial 64 unit boundaries so that turbulent and sky warps can be done reasonably.
void GL_SubdivideSurface(msurface_t *fa)
{
	vec3_t verts[64];
	int numverts, i, lindex;
	float *vec;

	warpface = fa;

	// convert edges back to a normal polygon
	numverts = 0;
	for (i = 0; i < fa->numedges && numverts < 64; i++) {
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0) {
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		}
		else {
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		}
		VectorCopy(vec, verts[numverts]);
		numverts++;
	}

	SubdividePolygon(numverts, verts[0]);

	if (GL_ShadersSupported()) {
		GLM_CreateVAOForWarpPoly(fa);
	}
}

// Just build the gl polys, don't subdivide
void GL_BuildSkySurfacePolys (msurface_t *fa)
{
	vec3_t		verts[64];
	int			numverts;
	int			i;
	int			lindex;
	float		*vec;
	glpoly_t	*poly;
	float		*vert;

	//
	// convert edges back to a normal polygon
	//
	numverts = 0;
	for (i=0 ; i<fa->numedges ; i++)
	{
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy (vec, verts[numverts]);
		numverts++;
	}

	poly = Hunk_Alloc (sizeof(glpoly_t) + (numverts-4) * VERTEXSIZE*sizeof(float));
	poly->next = NULL;
	fa->polys = poly;
	poly->numverts = numverts;
	vert = verts[0];
	for (i=0 ; i<numverts ; i++, vert+= 3)
		VectorCopy (vert, poly->verts[i]);
	if (GL_ShadersSupported()) {
		GLM_CreateVAOForPoly(poly);
	}
}


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

void EmitFlatPoly (msurface_t *fa) {
	glpoly_t *p;
	float *v;
	int i;
	for (p = fa->polys; p; p = p->next) {
		if (GL_ShadersSupported()) {
			GLM_DrawFlatPoly(color_white, p->vao, p->numverts, false);
		}
		else {
			glBegin(GL_POLYGON);
			for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
				glVertex3fv(v);
			}
			glEnd();
		}
	}
}


void EmitFlatWaterPoly (msurface_t *fa) {
	glpoly_t *p;
	float *v;
	int i;
	vec3_t nv;

	if (!amf_waterripple.value || !(cls.demoplayback || cl.spectator) || fa->texinfo->texture->turbType == TEXTURE_TURB_TELE) {
		EmitFlatPoly (fa);
		return;
	}

	for (p = fa->polys; p; p = p->next) {
		glBegin (GL_POLYGON);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
			VectorCopy(v, nv);
				nv[2] = v[2] + (bound(0, amf_waterripple.value, 20))*sin(v[0]*0.02+r_refdef2.time)*sin(v[1]*0.02+r_refdef2.time)*sin(v[2]*0.02+r_refdef2.time);
			glVertex3fv (nv);
		}
		glEnd ();
	}
}

byte* SurfaceFlatTurbColor(texture_t* texture)
{
	extern cvar_t r_telecolor, r_watercolor, r_slimecolor, r_lavacolor;

	switch (texture->turbType)
	{
	case TEXTURE_TURB_WATER:
		return r_watercolor.color;
	case TEXTURE_TURB_SLIME:
		return r_slimecolor.color;
	case TEXTURE_TURB_LAVA:
		return r_lavacolor.color;
	case TEXTURE_TURB_TELE:
		return r_telecolor.color;
	}

	return (byte *)&texture->flatcolor3ub;
}

//Does a water warp on the pre-fragmented glpoly_t chain
void EmitWaterPolys (msurface_t *fa)
{
	glpoly_t *p;
	float *v, s, t, os, ot;
	int i;
	byte *col;
	float wateralpha = bound((1 - r_refdef2.max_watervis), r_wateralpha.value, 1);

	vec3_t nv;

	col = SurfaceFlatTurbColor(fa->texinfo->texture);
	if (GL_ShadersSupported()) {
		if (r_fastturb.value) {
			byte old_alpha = col[3];

			// FIXME: turbripple effect, transparent water
			col[3] = 255;
			for (p = fa->polys; p; p = p->next) {
				GLM_DrawFlatPoly(col, p->vao, p->numverts, false);
			}
			col[3] = old_alpha;
		}
		else {
			wateralpha = bound(0, wateralpha, 1);

			glActiveTexture(GL_TEXTURE0);
			//glBindTexture(GL_TEXTURE_2D, fa->texinfo->texture->gl_texturenum);
			if (wateralpha < 1.0 && wateralpha >= 0) {
				GL_AlphaBlendFlags(GL_BLEND_ENABLED);
				GL_TextureEnvMode(GL_MODULATE);
				if (wateralpha < 0.9) {
					glDepthMask(GL_FALSE);
				}
			}

			{
				// FIXME: don't calculate number of verts each time
				int verts = 0;
				int polys = 0;
				for (p = fa->polys; p; p = p->next) {
					verts += p->numverts;
					++polys;
				}
				GLM_DrawTurbPolys(fa->polys->vao, verts + 2 * (polys - 1), wateralpha);
			}
			if (wateralpha < 1.0 && wateralpha >= 0) {
				GL_TextureEnvMode(GL_REPLACE);
				GL_AlphaBlendFlags(GL_BLEND_DISABLED);
				if (wateralpha < 0.9) {
					glDepthMask(GL_TRUE);
				}
			}
		}
	}
	else {
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
					glDepthMask(GL_FALSE);
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
					glDepthMask(GL_TRUE);
				}
				col[3] = old_alpha;
			}
			// END shaman FIX /gl_turbalpha + /r_fastturb {

			glEnable(GL_TEXTURE_2D);
			glColor3ubv(color_white);
			// END shaman RFE 1022504
		}
		else {
			GL_Bind(fa->texinfo->texture->gl_texturenum);
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
					if (amf_waterripple.value && (cls.demoplayback || cl.spectator) && !strstr(fa->texinfo->texture->name, "tele"))
						nv[2] = v[2] + (bound(0, amf_waterripple.value, 20)) *sin(v[0] * 0.02 + r_refdef2.time)*sin(v[1] * 0.02 + r_refdef2.time)*sin(v[2] * 0.02 + r_refdef2.time);

					glTexCoord2f(s, t);
					glVertex3fv(nv);

				}
				glEnd();
			}
		}

		GL_DisableFog();
	}
}



//Tei, add fire to lava
void EmitParticleEffect(msurface_t *fa, void(*fun)(vec3_t nv))
{
	glpoly_t *p;
	float *v;
	int i;
	vec3_t nv;

	for (p = fa->polys; p; p = p->next) {
		float min_x, min_y, max_x, max_y;
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
			min_x = i == 0 ? v[0] : min(min_x, v[0]);
			min_y = i == 0 ? v[1] : min(min_y, v[1]);
			max_x = i == 0 ? v[0] : max(max_x, v[0]);
			max_y = i == 0 ? v[1] : max(max_y, v[1]);
		}

		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
			if (rand() % 100 <= (cls.frametime / 0.00416) * 4) {
				VectorCopy(v, nv);

				nv[0] += f_rnd(min_x - v[0], max_x - v[0]);
				nv[1] += f_rnd(min_y - v[1], max_y - v[1]);

				fun(nv);
			}
		}
	}
}
//Tei, add fire to lava



void CalcCausticTexCoords(float *v, float *s, float *t) {
	float os, ot;

	os = v[3];
	ot = v[4];

	*s = os + SINTABLE_APPROX(0.465 * (r_refdef2.time + ot));
	*s *= -3 * (0.5 / 64);

	*t = ot + SINTABLE_APPROX(0.465 * (r_refdef2.time + os));
	*t *= -3 * (0.5 / 64);
}
void EmitCausticsPolys (void) {
	glpoly_t *p;
	int i;
	float s, t, *v;
	extern glpoly_t *caustics_polys;

	GL_Bind (underwatertexture);
	GL_TextureEnvMode(GL_DECAL);
	glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
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
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);

	caustics_polys = NULL;
}
