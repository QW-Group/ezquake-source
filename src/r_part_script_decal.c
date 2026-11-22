/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2016      Spike

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

// Clipping scripted decals to the surfaces of brush models.

#include "quakedef.h"
#include "gl_model.h"
#include "r_part_script_local.h"

// Longest polygon handled, before and after each clip; longer faces are
// skipped. Intact faces have at most 64 edges plus one per clip plane.
#define MAXFRAGMENTVERTS 384
// Triangles per callback. The fan of a 64-edge face clipped to six planes
// fits in one batch.
#define MAXFRAGMENTTRIS 128
// Vertices this close to a plane count as on it and are kept unchanged, so
// clipping does not cut slivers off faces that merely touch a plane.
#define FRAG_EPSILON (1.0f / 32)

#define FRAG_BACK  0
#define FRAG_FRONT 1
#define FRAG_ON    2

typedef struct fragmentdecal_s {
	vec3_t center;
	vec3_t normal;       // unit length, into the surfaces the decal lands on
	vec3_t planenorm[6]; // unit length, into the decal's box
	float planedist[6];
	float radius;
	int nodebudget; // nodes the walk may still enter
	qbool noperpendicular;
	unsigned int surfflagmask;
	unsigned int surfflagmatch;
	pscript_decalcallback_t callback;
	void *ctx;

	// Scratch space, kept here so the frames of the recursive node walk stay
	// small whatever the compiler inlines into it.
	vec3_t verts[2][MAXFRAGMENTVERTS];
	float dist[MAXFRAGMENTVERTS];
	byte side[MAXFRAGMENTVERTS];
	vec3_t tris[MAXFRAGMENTTRIS * 3];
} fragmentdecal_t;

// Clips inverts to the front of the decal's plane p into outverts. Returns 0
// when nothing is left, and also when more than MAXFRAGMENTVERTS would be,
// which a damaged map's face can cause since nothing makes it convex.
static int Fragment_ClipPolyToPlane(fragmentdecal_t *dec, vec3_t *inverts, int incount, vec3_t *outverts, int p)
{
	int i, next, outcount;
	float frac;

	for (i = 0; i < incount; i++) {
		dec->dist[i] = DotProduct(inverts[i], dec->planenorm[p]) - dec->planedist[p];
		if (dec->dist[i] < -FRAG_EPSILON) {
			dec->side[i] = FRAG_BACK;
		}
		else if (dec->dist[i] > FRAG_EPSILON) {
			dec->side[i] = FRAG_FRONT;
		}
		else {
			dec->side[i] = FRAG_ON;
		}
	}

	outcount = 0;
	for (i = 0; i < incount; i++) {
		next = (i + 1) % incount;
		if (dec->side[i] != FRAG_BACK) {
			if (outcount == MAXFRAGMENTVERTS) {
				return 0;
			}
			VectorCopy(inverts[i], outverts[outcount]);
			outcount++;
		}
		if (dec->side[i] == FRAG_ON || dec->side[next] == FRAG_ON || dec->side[i] == dec->side[next]) {
			continue;
		}
		if (outcount == MAXFRAGMENTVERTS) {
			return 0;
		}
		frac = dec->dist[i] / (dec->dist[i] - dec->dist[next]);
		PS_VectorInterpolate(inverts[i], frac, inverts[next], outverts[outcount]);
		outcount++;
	}
	return outcount;
}

// Clips the polygon in dec->verts[0] to the decal's box and passes what is
// left to the callback as a fan with the polygon's winding, in batches.
static void Fragment_ClipPoly(fragmentdecal_t *dec, int numverts)
{
	vec3_t d1, d2, n;
	vec3_t *verts;
	int p, cur, i, numtris;

	verts = dec->verts[0];
	if (dec->noperpendicular) {
		// The caller tested the face's plane; this tests its vertices, which a
		// damaged face's plane need not match. Quake faces wind clockwise seen
		// from the front, so the normal points into the face like dec->normal.
		VectorSubtract(verts[1], verts[0], d1);
		for (p = 2;; p++) {
			if (p >= numverts) {
				return;
			}
			VectorSubtract(verts[p], verts[0], d2);
			CrossProduct(d1, d2, n);
			if (DotProduct(n, n) > 0.1f) {
				break;
			}
		}
		VectorNormalize(n);
		if (DotProduct(n, dec->normal) < 0.1f) {
			return;
		}
	}

	cur = 0;
	for (p = 0; p < 6; p++) {
		numverts = Fragment_ClipPolyToPlane(dec, dec->verts[cur], numverts, dec->verts[!cur], p);
		cur = !cur;
		if (numverts < 3) {
			return;
		}
	}

	verts = dec->verts[cur];
	numtris = 0;
	for (i = 2; i < numverts; i++) {
		if (numtris == MAXFRAGMENTTRIS) {
			dec->callback(dec->ctx, dec->tris, numtris);
			numtris = 0;
		}
		VectorCopy(verts[0], dec->tris[numtris * 3 + 0]);
		VectorCopy(verts[i - 1], dec->tris[numtris * 3 + 1]);
		VectorCopy(verts[i], dec->tris[numtris * 3 + 2]);
		numtris++;
	}
	dec->callback(dec->ctx, dec->tris, numtris);
}

// Copies surf's vertices to verts in winding order. Returns their number, or
// 0 when there are fewer than 3 or more than maxverts, or for indices out of
// range, which the loader does not check for every BSP version.
int PScript_SurfacePolygon(const model_t *mod, const msurface_t *surf, vec3_t *verts, int maxverts)
{
	int i, lindex;
	unsigned int v;

	if (surf->numedges < 3 || surf->numedges > maxverts || surf->firstedge < 0 || surf->firstedge > mod->numsurfedges - surf->numedges) {
		return 0;
	}
	for (i = 0; i < surf->numedges; i++) {
		lindex = mod->surfedges[surf->firstedge + i];
		if (lindex >= mod->numedges || lindex <= -mod->numedges) {
			return 0;
		}
		v = lindex > 0 ? mod->edges[lindex].v[0] : mod->edges[-lindex].v[1];
		if (v >= (unsigned int)mod->numvertexes) {
			return 0;
		}
		VectorCopy(mod->vertexes[v].position, verts[i]);
	}
	return surf->numedges;
}

// With r_decal_noperpendicular set, surfaces facing more than 60 degrees away
// from the decal are skipped.
static void Q1BSP_Fragment_Surface(fragmentdecal_t *dec, const model_t *mod, const msurface_t *surf)
{
	float facing;
	int numverts;

	if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB)) {
		return;
	}
	if (dec->surfflagmask && ((unsigned int)surf->texinfo->flags & dec->surfflagmask) != dec->surfflagmatch) {
		return;
	}
	if (dec->noperpendicular) {
		facing = DotProduct(surf->plane->normal, dec->normal);
		if (surf->flags & SURF_PLANEBACK) {
			facing = -facing;
		}
		if (facing > -0.5f) {
			return;
		}
	}

	numverts = PScript_SurfacePolygon(mod, surf, dec->verts[0], MAXFRAGMENTVERTS);
	if (numverts) {
		Fragment_ClipPoly(dec, numverts);
	}
}

// depth is the number of nodes the walk may still enter below this call, and
// dec->nodebudget the number it may enter in all. An intact tree, which has
// each node once, never runs out of either, so they only stop a damaged map's
// children that loop back or are shared.
static void Q1BSP_ClipDecalToNodes(fragmentdecal_t *dec, const model_t *mod, mnode_t *node, int depth)
{
	const msurface_t *surf;
	unsigned int i;
	float dist;

	for (; depth > 0 && dec->nodebudget > 0 && node->contents >= 0; depth--) {
		dec->nodebudget--;
		dist = DotProduct(dec->center, node->plane->normal) - node->plane->dist;
		if (dist > dec->radius) {
			node = node->children[0];
			continue;
		}
		if (dist < -dec->radius) {
			node = node->children[1];
			continue;
		}

		// The loader does not check a node's surface range.
		if (node->firstsurface <= (unsigned int)mod->numsurfaces && node->numsurfaces <= (unsigned int)mod->numsurfaces - node->firstsurface) {
			surf = mod->surfaces + node->firstsurface;
			for (i = 0; i < node->numsurfaces; i++, surf++) {
				Q1BSP_Fragment_Surface(dec, mod, surf);
			}
		}

		Q1BSP_ClipDecalToNodes(dec, mod, node->children[0], depth - 1);
		node = node->children[1];
	}
}

// Clips the box around center that reaches size / 2 times each vector's
// length along tangent1, tangent2 and normal (pointing into the surfaces) to
// the surfaces of mod, the world or an inline brush model, in mod's space,
// passing triangles wound as their faces to callback.
void PScript_ClipDecal(model_t *mod, vec3_t center, vec3_t normal, vec3_t tangent1, vec3_t tangent2, float size, unsigned int surfflagmask, unsigned int surfflagmatch, pscript_decalcallback_t callback, void *ctx)
{
	fragmentdecal_t dec;
	float *axis[3];
	float len, extent;
	int i;

	// Inline models share the nodes of the map they come from and start at
	// their own head node, whose index the loader allows one past the end.
	if (!mod || mod->needload || mod->type != mod_brush || !mod->nodes || mod->firstnode < 0 || mod->firstnode >= mod->numnodes) {
		return;
	}
	if (!isfinite(size) || size <= 0) {
		return;
	}

	axis[0] = tangent1;
	axis[1] = tangent2;
	axis[2] = normal;
	dec.radius = 0;
	for (i = 0; i < 3; i++) {
		len = VectorLength(axis[i]);
		if (!(len > 0)) {
			return;
		}
		extent = len * size / 2;
		PS_VectorScale(axis[i], 1 / len, dec.planenorm[i * 2]);
		VectorNegate(dec.planenorm[i * 2], dec.planenorm[i * 2 + 1]);
		dec.planedist[i * 2] = DotProduct(center, dec.planenorm[i * 2]) - extent;
		dec.planedist[i * 2 + 1] = -DotProduct(center, dec.planenorm[i * 2]) - extent;
		// Also catches a non-finite centre and the overflows of huge vectors.
		if (!isfinite(dec.planedist[i * 2]) || !isfinite(dec.planedist[i * 2 + 1])) {
			return;
		}
		// Nodes farther than the largest half-extent are pruned, though a
		// sloped node plane can pass closer to the box's corners than that.
		dec.radius = max(dec.radius, extent);
	}

	VectorCopy(center, dec.center);
	VectorCopy(dec.planenorm[4], dec.normal);
	dec.nodebudget = mod->numnodes;
	dec.noperpendicular = r_decal_noperpendicular.value != 0;
	dec.surfflagmask = surfflagmask;
	dec.surfflagmatch = surfflagmatch;
	dec.callback = callback;
	dec.ctx = ctx;

	Q1BSP_ClipDecalToNodes(&dec, mod, mod->nodes + mod->firstnode, mod->numnodes);
}
