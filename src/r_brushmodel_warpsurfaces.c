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
#include "glsl/constants.glsl"
#include "r_local.h"
#include "r_brushmodel.h"
#include "utils.h"

extern model_t *loadmodel;
static msurface_t *warpface;

static void R_WarpSurfaceBoundPoly(int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	int i, j;
	float *v;

	mins[0] = mins[1] = mins[2] = BRUSHMODEL_MAX_SURFACE_EXTENTS;
	maxs[0] = maxs[1] = maxs[2] = BRUSHMODEL_MIN_SURFACE_EXTENTS;
	v = verts;
	for (i = 0; i < numverts; i++) {
		for (j = 0; j < 3; j++, v++) {
			if (i == 0 || *v < mins[j]) {
				mins[j] = *v;
			}
			if (i == 0 || *v > maxs[j]) {
				maxs[j] = *v;
			}
		}
	}
}

static void R_WarpSurfaceSubdividePolygon(int numverts, float *verts)
{
	int i, j, k, f, b;
	vec3_t mins, maxs, front[64], back[64];
	float m, *v, dist[64], frac, s, t;
	glpoly_t *poly;
	float subdivide_size;
	extern cvar_t gl_subdivide_size;

	if (numverts > 60) {
		Sys_Error("numverts = %i", numverts);
	}

	subdivide_size = max(1, gl_subdivide_size.value);
	R_WarpSurfaceBoundPoly(numverts, verts, mins, maxs);

	for (i = 0; i < 3; i++) {
		m = (mins[i] + maxs[i]) * 0.5;
		m = subdivide_size * floor(m / subdivide_size + 0.5);
		if (maxs[i] - m < 8) {
			continue;
		}
		if (m - mins[i] < 8) {
			continue;
		}

		// cut it
		v = verts + i;
		for (j = 0; j < numverts; j++, v += 3) {
			dist[j] = *v - m;
		}

		// wrap cases
		dist[j] = dist[0];
		v -= i;
		VectorCopy(verts, v);

		f = b = 0;
		v = verts;
		for (j = 0; j < numverts; j++, v += 3) {
			if (dist[j] >= 0) {
				VectorCopy(v, front[f]);
				f++;
			}
			if (dist[j] <= 0) {
				VectorCopy(v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j + 1] == 0) {
				continue;
			}
			if ((dist[j] > 0) != (dist[j + 1] > 0)) {
				// clip point
				frac = dist[j] / (dist[j] - dist[j + 1]);
				for (k = 0; k < 3; k++) {
					front[f][k] = back[b][k] = v[k] + frac * (v[3 + k] - v[k]);
				}
				f++;
				b++;
			}
		}
		R_WarpSurfaceSubdividePolygon(f, front[0]);
		R_WarpSurfaceSubdividePolygon(b, back[0]);
		return;
	}

	poly = (glpoly_t *)Hunk_AllocName(sizeof(glpoly_t) + (numverts - 4) * VERTEXSIZE * sizeof(float), "subdivpoly");
	poly->next = warpface->subdivided;
	warpface->subdivided = poly;
	poly->numverts = numverts;
	for (i = 0; i < numverts; i++, verts += 3) {
		VectorCopy(verts, poly->verts[i]);
		s = DotProduct(verts, warpface->texinfo->vecs[0]);
		t = DotProduct(verts, warpface->texinfo->vecs[1]);
		poly->verts[i][3] = s / 64.0f;
		poly->verts[i][4] = t / 64.0f;
		poly->verts[i][5] = 8 * sin(2 * t) / 64.0f;
		poly->verts[i][6] = 8 * sin(2 * s) / 64.0f;
		poly->verts[i][7] = 8 * cos(2 * t) / 64.0f;
		poly->verts[i][8] = 8 * cos(2 * s) / 64.0f;
	}
	R_BrushModelPolygonToTriangleStrip(poly);
}

// Breaks a polygon up along axial 64 unit boundaries so that turbulent and sky warps can be done reasonably.
void R_TurbSurfacesSubdivide(msurface_t *fa)
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

	R_WarpSurfaceSubdividePolygon(numverts, verts[0]);
}

// Just build the gl polys, don't subdivide
void R_SkySurfacesBuildPolys(msurface_t *fa)
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
	for (i = 0; i < fa->numedges; i++) {
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

	poly = Hunk_AllocName(sizeof(glpoly_t) + (numverts - 4) * VERTEXSIZE * sizeof(float), "subdivpoly");
	poly->next = NULL;
	fa->polys = poly;
	poly->numverts = numverts;
	vert = verts[0];
	for (i = 0; i < numverts; i++, vert += 3) {
		VectorCopy(vert, poly->verts[i]);
	}
	R_BrushModelPolygonToTriangleStrip(poly);
}

byte* SurfaceFlatTurbColor(texture_t* texture)
{
	extern cvar_t r_telecolor, r_watercolor, r_slimecolor, r_lavacolor, r_skycolor;

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
	case TEXTURE_TURB_SKY:
		return r_skycolor.color;
	}

	return (byte *)&texture->flatcolor3ub;
}

//Tei, add fire to lava
void EmitParticleEffect(msurface_t *fa, void(*fun)(vec3_t nv))
{
	glpoly_t *p;
	float *v;
	int i;
	vec3_t nv;

	for (p = fa->subdivided; p; p = p->next) {
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
