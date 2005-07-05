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
#include "r_local.h"

#define LIGHT_MIN	5		// lowest light value we'll allow, to avoid the need for inner-loop light clamping

mtriangle_t		*ptriangles;
affinetridesc_t	r_affinetridesc;

void			*acolormap;	// FIXME: should go away

trivertx_t		*r_oldapverts;
trivertx_t		*r_apverts;
float			r_framelerp;
float			r_lerpdistance;

// TODO: these probably will go away with optimized rasterization
mdl_t				*pmdl;
static model_t		*pmodel;
vec3_t				r_plightvec;
int					r_ambientlight;
float				r_shadelight;
aliashdr_t			*paliashdr;
finalvert_t			*pfinalverts;
auxvert_t			*pauxverts;
static float		ziscale;

static vec3_t		alias_forward, alias_right, alias_up;

static maliasskindesc_t	*pskindesc;

int				r_amodels_drawn;
int				a_skinwidth;
int				r_anumverts;

float	aliastransform[3][4];

typedef struct {
	int	index0;
	int	index1;
} aedge_t;

static aedge_t	aedges[12] = {
{0, 1}, {1, 2}, {2, 3}, {3, 0},
{4, 5}, {5, 6}, {6, 7}, {7, 4},
{0, 5}, {1, 4}, {2, 7}, {3, 6}
};

#define NUMVERTEXNORMALS	162

float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

#define R_AliasTransformVector(in, out)											\
	(((out)[0] = DotProduct((in), aliastransform[0]) + aliastransform[0][3]),	\
	((out)[1] = DotProduct((in), aliastransform[1]) + aliastransform[1][3]),	\
	((out)[2] = DotProduct((in), aliastransform[2]) + aliastransform[2][3]))

void R_AliasTransformAndProjectFinalVerts (finalvert_t *fv, stvert_t *pstverts);
void R_AliasSetUpTransform (int trivial_accept);
void R_AliasTransformFinalVert (finalvert_t *fv, auxvert_t *av, trivertx_t *pverts1, trivertx_t *pverts2, stvert_t *pstverts);
void R_AliasProjectFinalVert (finalvert_t *fv, auxvert_t *av);

extern cvar_t r_lerpframes;

void R_AliasCheckBBoxFrame (int frame, trivertx_t **mins, trivertx_t **maxs) {
	int	i, numframes;
	maliasgroup_t *paliasgroup;
	float *pintervals, fullinterval, targettime;

	if (paliashdr->frames[frame].type == ALIAS_SINGLE) {
		*mins = &paliashdr->frames[frame].bboxmin;
		*maxs = &paliashdr->frames[frame].bboxmax;
	} else {
		paliasgroup = (maliasgroup_t *) ((byte *) paliashdr + paliashdr->frames[frame].frame);
		pintervals = (float *) ((byte *) paliashdr + paliasgroup->intervals);
		numframes = paliasgroup->numframes;
		fullinterval = pintervals[numframes - 1];

		// when loading in Mod_LoadAliasGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by 0
		targettime = cl.time - ((int) (cl.time / fullinterval)) * fullinterval;

		for (i = 0; i < numframes - 1; i++) {
			if (pintervals[i] > targettime)
				break;
		}
		*mins = &paliasgroup->frames[i].bboxmin;
		*maxs = &paliasgroup->frames[i].bboxmax;
	}
}

qboolean R_AliasCheckBBox (entity_t *ent) {
	int i, flags, numv, minz;
	float zi, basepts[8][3], v0, v1, frac;
	finalvert_t *pv0, *pv1, viewpts[16];
	auxvert_t *pa0, *pa1, viewaux[16];
	qboolean zclipped, zfullyclipped;
	unsigned anyclip, allclip;
	trivertx_t *mins, *maxs, *oldmins, *oldmaxs;

	ent->trivial_accept = 0;

	// expand, rotate, and translate points into worldspace
	R_AliasSetUpTransform (0);

	// construct the base bounding box for this frame
	if (r_framelerp == 1) {
		R_AliasCheckBBoxFrame (ent->frame, &mins, &maxs);

		// x worldspace coordinates
		basepts[0][0] = basepts[1][0] = basepts[2][0] = basepts[3][0] = (float) mins->v[0];
		basepts[4][0] = basepts[5][0] = basepts[6][0] = basepts[7][0] = (float) maxs->v[0];
		// y worldspace coordinates
		basepts[0][1] = basepts[3][1] = basepts[5][1] = basepts[6][1] = (float) mins->v[1];
		basepts[1][1] = basepts[2][1] = basepts[4][1] = basepts[7][1] = (float) maxs->v[1];
		// z worldspace coordinates
		basepts[0][2] = basepts[1][2] = basepts[4][2] = basepts[5][2] = (float) mins->v[2];
		basepts[2][2] = basepts[3][2] = basepts[6][2] = basepts[7][2] = (float) maxs->v[2];
	} else {
		R_AliasCheckBBoxFrame (ent->oldframe, &oldmins, &oldmaxs);
		R_AliasCheckBBoxFrame (ent->frame, &mins, &maxs);

		// x worldspace coordinates
		basepts[0][0] = basepts[1][0] = basepts[2][0] = basepts[3][0] =	(float) min(mins->v[0], oldmins->v[0]);
		basepts[4][0] = basepts[5][0] = basepts[6][0] = basepts[7][0] =	(float) max(maxs->v[0], oldmaxs->v[0]);
		// y worldspace coordinates
		basepts[0][1] = basepts[3][1] = basepts[5][1] = basepts[6][1] =	(float) min(mins->v[1], oldmins->v[1]);
		basepts[1][1] = basepts[2][1] = basepts[4][1] = basepts[7][1] =	(float) max(maxs->v[1], oldmaxs->v[1]);
		// z worldspace coordinates
		basepts[0][2] = basepts[1][2] = basepts[4][2] = basepts[5][2] =	(float) min(mins->v[2], oldmins->v[2]);
		basepts[2][2] = basepts[3][2] = basepts[6][2] = basepts[7][2] = (float) max(maxs->v[2], oldmaxs->v[2]);
	}

	zclipped = false;
	zfullyclipped = true;

	minz = 9999;
	for (i = 0; i < 8; i++) {
		R_AliasTransformVector  (&basepts[i][0], &viewaux[i].fv[0]);

		if (viewaux[i].fv[2] < ALIAS_Z_CLIP_PLANE) {
			// we must clip points that are closer than the near clip plane
			viewpts[i].flags = ALIAS_Z_CLIP;
			zclipped = true;
		} else {
			if (viewaux[i].fv[2] < minz)
				minz = viewaux[i].fv[2];
			viewpts[i].flags = 0;
			zfullyclipped = false;
		}
	}

	if (zfullyclipped)
		return false;	// everything was near-z-clipped

	numv = 8;

	if (zclipped) {
		// organize points by edges, use edges to get new points (possible trivial reject)
		for (i = 0; i < 12; i++) {
			// edge endpoints
			pv0 = &viewpts[aedges[i].index0];
			pv1 = &viewpts[aedges[i].index1];
			pa0 = &viewaux[aedges[i].index0];
			pa1 = &viewaux[aedges[i].index1];

			// if one end is clipped and the other isn't, make a new point
			if (pv0->flags ^ pv1->flags) {
				frac = (ALIAS_Z_CLIP_PLANE - pa0->fv[2]) / (pa1->fv[2] - pa0->fv[2]);
				viewaux[numv].fv[0] = pa0->fv[0] + (pa1->fv[0] - pa0->fv[0]) * frac;
				viewaux[numv].fv[1] = pa0->fv[1] + (pa1->fv[1] - pa0->fv[1]) * frac;
				viewaux[numv].fv[2] = ALIAS_Z_CLIP_PLANE;
				viewpts[numv].flags = 0;
				numv++;
			}
		}
	}

	// project the vertices that remain after clipping
	anyclip = 0;
	allclip = ALIAS_XY_CLIP_MASK;

	// TODO: probably should do this loop in ASM, especially if we use floats
	for (i = 0; i < numv; i++) {
		// we don't need to bother with vertices that were z-clipped
		if (viewpts[i].flags & ALIAS_Z_CLIP)
			continue;

		zi = 1.0 / viewaux[i].fv[2];

		// FIXME: do with chop mode in ASM, or convert to float
		v0 = (viewaux[i].fv[0] * xscale * zi) + xcenter;
		v1 = (viewaux[i].fv[1] * yscale * zi) + ycenter;

		flags = 0;

		if (v0 < r_refdef.fvrectx)
			flags |= ALIAS_LEFT_CLIP;
		if (v1 < r_refdef.fvrecty)
			flags |= ALIAS_TOP_CLIP;
		if (v0 > r_refdef.fvrectright)
			flags |= ALIAS_RIGHT_CLIP;
		if (v1 > r_refdef.fvrectbottom)
			flags |= ALIAS_BOTTOM_CLIP;

		anyclip |= flags;
		allclip &= flags;
	}

	if (allclip)
		return false;	// trivial reject off one side

	ent->trivial_accept = !anyclip & !zclipped;

	if (ent->trivial_accept) {
		if (minz > (r_aliastransition + (pmdl->size * r_resfudge)))
			ent->trivial_accept |= 2;
	}

	return true;
}

//General clipped case
void R_AliasPreparePoints (void) {
	int i;
	stvert_t *pstverts;
	finalvert_t *fv;
	auxvert_t *av;
	mtriangle_t *ptri;
	finalvert_t *pfv[3];

	pstverts = (stvert_t *)((byte *)paliashdr + paliashdr->stverts);
	r_anumverts = pmdl->numverts;
 	fv = pfinalverts;
	av = pauxverts;

	for (i = 0; i < r_anumverts; i++, fv++, av++, r_oldapverts++, r_apverts++, pstverts++) {
		R_AliasTransformFinalVert (fv, av, r_oldapverts, r_apverts, pstverts);
		if (av->fv[2] < ALIAS_Z_CLIP_PLANE) {
			fv->flags |= ALIAS_Z_CLIP;
		} else {
			 R_AliasProjectFinalVert (fv, av);

			if (fv->v[0] < r_refdef.aliasvrect.x)
				fv->flags |= ALIAS_LEFT_CLIP;
			if (fv->v[1] < r_refdef.aliasvrect.y)
				fv->flags |= ALIAS_TOP_CLIP;
			if (fv->v[0] > r_refdef.aliasvrectright)
				fv->flags |= ALIAS_RIGHT_CLIP;
			if (fv->v[1] > r_refdef.aliasvrectbottom)
				fv->flags |= ALIAS_BOTTOM_CLIP;	
		}
	}

	r_affinetridesc.numtriangles = 1;

	ptri = (mtriangle_t *)((byte *)paliashdr + paliashdr->triangles);
	for (i = 0; i < pmdl->numtris; i++, ptri++) {
		pfv[0] = &pfinalverts[ptri->vertindex[0]];
		pfv[1] = &pfinalverts[ptri->vertindex[1]];
		pfv[2] = &pfinalverts[ptri->vertindex[2]];

		if (pfv[0]->flags & pfv[1]->flags & pfv[2]->flags & (ALIAS_XY_CLIP_MASK | ALIAS_Z_CLIP))
			continue;		// completely clipped
		
		if (!((pfv[0]->flags | pfv[1]->flags | pfv[2]->flags) & (ALIAS_XY_CLIP_MASK | ALIAS_Z_CLIP))) {	
			// totally unclipped
			r_affinetridesc.pfinalverts = pfinalverts;
			r_affinetridesc.ptriangles = ptri;
			D_PolysetDraw ();
		} else{	
			// partially clipped
			R_AliasClipTriangle (ptri);
		}
	}
}

void R_AliasSetUpTransform (int trivial_accept) {
	int i;
	float scale, rotationmatrix[3][4], t2matrix[3][4];
	vec3_t angles;
	static float tmatrix[3][4], viewmatrix[3][4];
	extern cvar_t r_viewmodelsize;

	// TODO: should really be stored with the entity instead of being reconstructed
	// TODO: should use a look-up table
	// TODO: could cache lazily, stored in the entity

	angles[ROLL] = currententity->angles[ROLL];
	angles[PITCH] = -currententity->angles[PITCH];
	angles[YAW] = currententity->angles[YAW];
	AngleVectors (angles, alias_forward, alias_right, alias_up);

	tmatrix[0][0] = pmdl->scale[0];
	tmatrix[1][1] = pmdl->scale[1];
	tmatrix[2][2] = pmdl->scale[2];

	if ((currententity->flags & RF_WEAPONMODEL)) {		
		scale = 0.5 + bound(0, r_viewmodelsize.value, 1) / 2; 
		tmatrix[0][0] *= scale;
	}

	tmatrix[0][3] = pmdl->scale_origin[0];
	tmatrix[1][3] = pmdl->scale_origin[1];
	tmatrix[2][3] = pmdl->scale_origin[2];

	// TODO: can do this with simple matrix rearrangement
	for (i = 0; i < 3; i++) {
		t2matrix[i][0] = alias_forward[i];
		t2matrix[i][1] = -alias_right[i];
		t2matrix[i][2] = alias_up[i];
	}

	t2matrix[0][3] = -modelorg[0];
	t2matrix[1][3] = -modelorg[1];
	t2matrix[2][3] = -modelorg[2];

	// FIXME: can do more efficiently than full concatenation
	R_ConcatTransforms (t2matrix, tmatrix, rotationmatrix);

	// TODO: should be global, set when vright, etc., set
	VectorCopy (vright, viewmatrix[0]);
	VectorNegate (vup, viewmatrix[1]);
	VectorCopy (vpn, viewmatrix[2]);

	R_ConcatTransforms (viewmatrix, rotationmatrix, aliastransform);

	// do the scaling up of x and y to screen coordinates as part of the transform
	// for the unclipped case (it would mess up clipping in the clipped case).
	// Also scale down z, so 1/z is scaled 31 bits for free, and scale down x and y
	// correspondingly so the projected x and y come out right
	// FIXME: make this work for clipped case too?
	if (trivial_accept) {
		for (i = 0; i < 4; i++) {
			aliastransform[0][i] *= aliasxscale * (1.0 / ((float) 0x8000 * 0x10000));
			aliastransform[1][i] *= aliasyscale * (1.0 / ((float) 0x8000 * 0x10000));
			aliastransform[2][i] *= 1.0 / ((float) 0x8000 * 0x10000);

		}
	}
}

void R_AliasTransformFinalVert (finalvert_t *fv, auxvert_t *av, trivertx_t *pverts1, trivertx_t *pverts2, stvert_t *pstverts) {
	int temp;
	float lightcos, lerpfrac;
	vec3_t interpolated_verts, interpolated_norm;


	lerpfrac = r_framelerp;
	if ((currententity->flags & RF_LIMITLERP)) {
		
		lerpfrac = VectorL2Compare(pverts1->v, pverts2->v, r_lerpdistance) ? r_framelerp : 1;
	}

	VectorInterpolate(pverts1->v, lerpfrac, pverts2->v, interpolated_verts);

	av->fv[0] = DotProduct(interpolated_verts, aliastransform[0]) + aliastransform[0][3];
	av->fv[1] = DotProduct(interpolated_verts, aliastransform[1]) + aliastransform[1][3];
	av->fv[2] = DotProduct(interpolated_verts, aliastransform[2]) + aliastransform[2][3];

	fv->v[2] = pstverts->s;
	fv->v[3] = pstverts->t;

	fv->flags = pstverts->onseam;

	// lighting
	VectorInterpolate(r_avertexnormals[pverts1->lightnormalindex], lerpfrac,
		r_avertexnormals[pverts1->lightnormalindex], interpolated_norm);
	lightcos = DotProduct (interpolated_norm, r_plightvec);
	temp = r_ambientlight;


	if (lightcos < 0) {
		temp += (int) (r_shadelight * lightcos);
		// clamp; because we limited the minimum ambient and shading light, we don't have to clamp low light, just bright
		temp = max(temp, 0);
	}
	fv->v[4] = temp;
}

void R_AliasTransformAndProjectFinalVerts (finalvert_t *fv, stvert_t *pstverts) {
	int i, temp;
	float lightcos, zi;
	trivertx_t *pverts1, *pverts2;
	vec3_t interpolated_verts, interpolated_norm;

	pverts1 = r_oldapverts;
	pverts2 = r_apverts;

	for (i = 0; i < r_anumverts; i++, fv++, pverts1++, pverts2++, pstverts++) {

		VectorInterpolate(pverts1->v, r_framelerp, pverts2->v, interpolated_verts);
		// transform and project
		zi = 1.0 / (DotProduct(interpolated_verts, aliastransform[2]) + aliastransform[2][3]);

		// x, y, and z are scaled down by 1/2**31 in the transform, so 1/z is
		// scaled up by 1/2**31, and the scaling cancels out for x and y in the projection
		fv->v[5] = zi;

		fv->v[0] = ((DotProduct(interpolated_verts, aliastransform[0]) + aliastransform[0][3]) * zi) + aliasxcenter;
		fv->v[1] = ((DotProduct(interpolated_verts, aliastransform[1]) + aliastransform[1][3]) * zi) + aliasycenter;

		fv->v[2] = pstverts->s;
		fv->v[3] = pstverts->t;
		fv->flags = pstverts->onseam;

		// lighting
		VectorInterpolate(r_avertexnormals[pverts1->lightnormalindex], r_framelerp,
			r_avertexnormals[pverts1->lightnormalindex], interpolated_norm);
		lightcos = DotProduct (interpolated_norm, r_plightvec);
		temp = r_ambientlight;


		if (lightcos < 0) {
			temp += (int) (r_shadelight * lightcos);
			// clamp; because we limited the minimum ambient and shading light, we don't have to clamp low light, just bright
			temp = max(temp, 0);
		}

		fv->v[4] = temp;
	}
}

void R_AliasProjectFinalVert (finalvert_t *fv, auxvert_t *av) {
	float zi;

	// project points
	zi = 1.0 / av->fv[2];
	fv->v[5] = zi * ziscale;
	fv->v[0] = (av->fv[0] * aliasxscale * zi) + aliasxcenter;
	fv->v[1] = (av->fv[1] * aliasyscale * zi) + aliasycenter;
}

void R_AliasPrepareUnclippedPoints (void) {
	stvert_t *pstverts;

	pstverts = (stvert_t *)((byte *)paliashdr + paliashdr->stverts);
	r_anumverts = pmdl->numverts;

	R_AliasTransformAndProjectFinalVerts(pfinalverts, pstverts);

	if (r_affinetridesc.drawtype)
		D_PolysetDrawFinalVerts (pfinalverts, r_anumverts);

	r_affinetridesc.pfinalverts = pfinalverts;
	r_affinetridesc.ptriangles = (mtriangle_t *) ((byte *)paliashdr + paliashdr->triangles);
	r_affinetridesc.numtriangles = pmdl->numtris;

	D_PolysetDraw ();
}

void R_AliasSetupSkin (entity_t *ent) {
	byte *base;
	int skinnum, i, numskins;
	maliasskingroup_t *paliasskingroup;
	float *pskinintervals, fullskininterval, skintargettime, skintime;

	skinnum = ent->skinnum;
	if (skinnum >= pmdl->numskins || skinnum < 0) {
		Com_DPrintf ("R_AliasSetupSkin: no such skin # %d\n", skinnum);
		skinnum = 0;
	}

	pskindesc = ((maliasskindesc_t *) ((byte *) paliashdr + paliashdr->skindesc)) + skinnum;
	a_skinwidth = pmdl->skinwidth;

	if (pskindesc->type == ALIAS_SKIN_GROUP) {
		paliasskingroup = (maliasskingroup_t *) ((byte *) paliashdr + pskindesc->skin);
		pskinintervals = (float *) ((byte *) paliashdr + paliasskingroup->intervals);
		numskins = paliasskingroup->numskins;
		fullskininterval = pskinintervals[numskins - 1];
	
		skintime = cl.time;
	
		// when loading in Mod_LoadAliasSkinGroup, we guaranteed all interval
		// values are positive, so we don't have to worry about division by 0
		skintargettime = skintime - ((int) (skintime / fullskininterval)) * fullskininterval;
	
		for (i = 0; i < numskins - 1; i++) {
			if (pskinintervals[i] > skintargettime)
				break;
		}

		pskindesc = &paliasskingroup->skindescs[i];
	}

	r_affinetridesc.pskindesc = pskindesc;
	r_affinetridesc.pskin = (void *)((byte *)paliashdr + pskindesc->skin);
	r_affinetridesc.skinwidth = a_skinwidth;
	r_affinetridesc.seamfixupX16 =  (a_skinwidth >> 1) << 16;
	r_affinetridesc.skinheight = pmdl->skinheight;

	if (ent->scoreboard) {
		if (!ent->scoreboard->skin)
			Skin_Find (ent->scoreboard);
		base = Skin_Cache (ent->scoreboard->skin);
		if (base) {
			r_affinetridesc.pskin = base;
			r_affinetridesc.skinwidth = 320;
			r_affinetridesc.skinheight = 200;
		}
	}
}

void R_AliasSetupLighting (entity_t *ent) {
	int lnum, minlight, ambientlight, shadelight;
	float add, fbskins;
	vec3_t dist;

	ambientlight = shadelight = R_LightPoint (currententity->origin);

	for (lnum = 0; lnum < MAX_DLIGHTS; lnum++) {
		if (cl_dlights[lnum].die < cl.time || !cl_dlights[lnum].radius)
			continue;

		VectorSubtract (currententity->origin, cl_dlights[lnum].origin,	dist);
		add = cl_dlights[lnum].radius - VectorLength(dist);

		if (add > 0)
			ambientlight += add;
	}

	// clamp lighting so it doesn't overbright as much
	if (ambientlight > 128)
		ambientlight = 128;
	if (ambientlight + shadelight > 192)
		shadelight = 192 - ambientlight;

	// always give the gun some light
	if ((currententity->flags & RF_WEAPONMODEL) && ambientlight < 24)
		ambientlight = shadelight = 24;

	// never allow players to go totally black
	if (currententity->model->modhint == MOD_PLAYER) {
		if (ambientlight < 8)
			ambientlight = shadelight = 8;
	}


	if (currententity->model->modhint == MOD_PLAYER) {
		extern cvar_t r_fullbrightSkins;
		fbskins = bound(0, r_fullbrightSkins.value, cl.fbskins);
		if (fbskins) {
			ambientlight = max (ambientlight, 8 + fbskins * 120);
			shadelight = max (shadelight, 8 + fbskins * 120);
		}
	}


	minlight = cl.minlight;

	if (ambientlight < minlight)
		ambientlight = shadelight = minlight;

	// guarantee that no vertex will ever be lit below LIGHT_MIN, so we don't have to clamp off the bottom
	r_ambientlight = max(ambientlight, LIGHT_MIN);
	r_ambientlight = (255 - r_ambientlight) << VID_CBITS;
	r_ambientlight = max(r_ambientlight, LIGHT_MIN);
	r_shadelight = max(shadelight, 0);
	r_shadelight *= VID_GRADES;

	// rotate the lighting vector into the model's frame of reference
	VectorSet(r_plightvec, -alias_forward[0], alias_right[0], -alias_up[0]);
}

void R_AliasSetupFrameVerts (int frame, trivertx_t **verts) {
	int	i, numframes;
	maliasgroup_t *paliasgroup;
	float *pintervals, fullinterval, targettime;

	if (paliashdr->frames[frame].type == ALIAS_SINGLE) {
		*verts = (trivertx_t *) ((byte *) paliashdr + paliashdr->frames[frame].frame);
	} else {
		paliasgroup = (maliasgroup_t *) ((byte *)paliashdr + paliashdr->frames[frame].frame);
		pintervals = (float *) ((byte *) paliashdr + paliasgroup->intervals);
		numframes = paliasgroup->numframes;
		fullinterval = pintervals[numframes - 1];

		// when loading in Mod_LoadAliasGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by 0
		targettime = cl.time - ((int) (cl.time / fullinterval)) * fullinterval;

		for (i = 0; i < numframes - 1; i++) {
			if (pintervals[i] > targettime)
				break;
		}
		*verts = (trivertx_t *) ((byte *) paliashdr + paliasgroup->frames[i].frame);
	}
}

//set r_oldapverts, r_apverts
void R_AliasSetupFrame (entity_t *ent) {
	R_AliasSetupFrameVerts(ent->oldframe, &r_oldapverts);
	R_AliasSetupFrameVerts(ent->frame, &r_apverts);
}

void R_AliasDrawModel (entity_t *ent) {
	finalvert_t finalverts[MAXALIASVERTS + ((CACHE_SIZE - 1) / sizeof(finalvert_t)) + 1];
	auxvert_t auxverts[MAXALIASVERTS];

	VectorCopy (ent->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	pmodel = ent->model;
	paliashdr = Mod_Extradata (pmodel);
	pmdl = (mdl_t *) ((byte *) paliashdr + paliashdr->model);

	if (ent->frame >= pmdl->numframes || ent->frame < 0) {
		Com_DPrintf ("R_AliasDrawModel: no such frame %d\n", ent->frame);
		ent->frame = 0;
	}
	if (ent->oldframe >= pmdl->numframes || ent->oldframe < 0) {
		Com_DPrintf ("R_AliasDrawModel: no such oldframe %d\n", ent->oldframe);
		ent->oldframe = 0;
	}


	if (!r_lerpframes.value || ent->framelerp < 0 || ent->oldframe == ent->frame)
		r_framelerp = 1.0;
	else
		r_framelerp = min (ent->framelerp, 1);


	if (!(ent->flags & RF_WEAPONMODEL)) {
		if (!R_AliasCheckBBox(ent))
			return;
	}

	r_amodels_drawn++;

	// cache align
	pfinalverts = (finalvert_t *) (((long) &finalverts[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
	pauxverts = &auxverts[0];

	R_AliasSetupSkin (ent);
	R_AliasSetUpTransform (ent->trivial_accept);
	R_AliasSetupLighting (ent);
	R_AliasSetupFrame (ent);

	if (!ent->colormap)
		Sys_Error ("R_AliasDrawModel: !ent->colormap");

	r_affinetridesc.drawtype = (ent->trivial_accept == 3);

	if (r_affinetridesc.drawtype) {
		D_PolysetUpdateTables ();		// FIXME: precalc...
	}
	else
	{
#if id386
		D_Aff8Patch (ent->colormap);
#endif
	}

	acolormap = ent->colormap;

	if (!(ent->flags & RF_WEAPONMODEL))
		ziscale = (float) 0x8000 * (float) 0x10000;
	else
		ziscale = (float) 0x8000 * (float) 0x10000 * 3.0;

	if (ent->trivial_accept)
		R_AliasPrepareUnclippedPoints ();
	else
		R_AliasPreparePoints ();
}
