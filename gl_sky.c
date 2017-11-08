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
// gl_sky.c -- sky polygons (was previously in gl_warp.c)

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "teamplay.h"
#include "gl_sky.h"

int solidskytexture, alphaskytexture;

float speedscale, speedscale2;		// for top sky and bottom sky
float skymins[2][6], skymaxs[2][6];
qbool r_skyboxloaded;
extern msurface_t *skychain;
extern msurface_t **skychain_tail;

void R_DrawSkyChain (void) {
	if (GL_ShadersSupported()) {
		GLM_DrawSkyChain();
	}
	else {
		GLC_DrawSkyChain();
	}

	skychain = NULL;
	skychain_tail = &skychain;
}

//A sky texture is 256 * 128, with the right side being a masked overlay
void R_InitSky (texture_t *mt) {
	int i, j, p, r, g, b;
	byte *src;
	unsigned trans[128 * 128], transpix, *rgba;

	src = (byte *) mt + mt->offsets[0];

	// make an average value for the back to avoid a fringe on the top level
	r = g = b = 0;
	for (i = 0; i < 128; i++) {
		for (j = 0; j < 128; j++) {
			p = src[i * 256 + j + 128];
			rgba = &d_8to24table[p];
			trans[(i * 128) + j] = *rgba;
			r += ((byte *) rgba)[0];
			g += ((byte *) rgba)[1];
			b += ((byte *) rgba)[2];
		}
	}

	((byte *) &transpix)[0] = r / (128 * 128);
	((byte *) &transpix)[1] = g / (128 * 128);
	((byte *) &transpix)[2] = b / (128 * 128);
	((byte *) &transpix)[3] = 0;

	solidskytexture = GL_LoadTexture ("***solidskytexture***", 128, 128, (byte *)trans, TEX_MIPMAP, 4);

	for (i = 0; i < 128; i++) {
		for (j = 0; j < 128; j++) {
			p = src[i * 256 + j];
			trans[(i * 128) + j] = p ? d_8to24table[p] : transpix;
		}
	}

	alphaskytexture = GL_LoadTexture ("***alphaskytexture***", 128, 128, (byte *)trans, TEX_ALPHA | TEX_MIPMAP, 4);
}


static char *skybox_ext[MAX_SKYBOXTEXTURES] = {"rt", "bk", "lf", "ft", "up", "dn"};


int R_SetSky(char *skyname)
{
	int i;
	int real_width, real_height;
	char *groupname;

	r_skyboxloaded = false;

	// set skyname to groupname if any
	skyname	= (groupname = TP_GetSkyGroupName(TP_MapName(), NULL)) ? groupname : skyname;

	if (!skyname || !skyname[0] || strchr(skyname, '.'))
	{
		// Empty name or contain dot(dot causing troubles with skipping part of the name as file extenson),
		// so do nothing.
		return 1;
	}

	for (i = 0; i < MAX_SKYBOXTEXTURES; i++)
	{
		byte *data;

		if (
			(data = GL_LoadImagePixels (va("env/%s%s", skyname, skybox_ext[i]), 0, 0, 0, &real_width, &real_height))
			|| (data = GL_LoadImagePixels (va("gfx/env/%s%s", skyname, skybox_ext[i]), 0, 0, 0, &real_width, &real_height))
			|| (data = GL_LoadImagePixels (va("env/%s_%s", skyname, skybox_ext[i]), 0, 0, 0, &real_width, &real_height))
			|| (data = GL_LoadImagePixels (va("gfx/env/%s_%s", skyname, skybox_ext[i]), 0, 0, 0, &real_width, &real_height))
			)
		{
			skyboxtextures[i] = GL_LoadTexture(
				va("skybox:%s", skybox_ext[i]),
				real_width, real_height, data, TEX_NOCOMPRESS | TEX_MIPMAP, 4);
			// we shold free data from GL_LoadImagePixels()
			Q_free(data);
		}
		else
		{
			skyboxtextures[i] = 0; // GL_LoadImagePixels() fail to load anything
		}

		if (!skyboxtextures[i])
		{
			Com_Printf ("Couldn't load skybox \"%s\"\n", skyname);
			return 1;
		}
	}

	// everything was OK
	r_skyboxloaded = true;
	return 0;
}

void OnChange_r_skyname (cvar_t *v, char *skyname, qbool* cancel)
{
	if (!skyname[0]) {		
		r_skyboxloaded = false;
		return;
	}

	*cancel = R_SetSky(skyname);
}

void R_LoadSky_f(void) {
	switch (Cmd_Argc()) {
	case 1:
		if (r_skyboxloaded) {
			Com_Printf("Current skybox is \"%s\"\n", r_skyname.string);
		}
		else {
			Com_Printf("No skybox has been set\n");
		}
		break;
	case 2:
		if (!strcasecmp(Cmd_Argv(1), "none")) {
			Cvar_Set(&r_skyname, "");
		}
		else {
			Cvar_Set(&r_skyname, Cmd_Argv(1));
		}
		break;
	default:
		Com_Printf("Usage: %s <skybox>\n", Cmd_Argv(0));
		break;
	}
}

static vec3_t skyclip[6] = {
	{1,1,0},
	{1,-1,0},
	{0,-1,1},
	{0,1,1},
	{1,0,1},
	{-1,0,1} 
};

#define skybox_range	2400.0

// 1 = s, 2 = t, 3 = 2048
static int	st_to_vec[6][3] = {
	{3,-1,2},
	{-3,1,2},

	{1,3,2},
	{-1,-3,2},

	{-2,-1,3},		// 0 degrees yaw, look straight up
	{2,-1,-3}		// look straight down
};

// s = [0]/[2], t = [1]/[2]
static int	vec_to_st[6][3] = {
	{-2,3,1},
	{2,3,-1},

	{1,3,2},
	{-1,3,-2},

	{-2,-1,3},
	{-2,1,-3}
};

void DrawSkyPolygon (int nump, vec3_t vecs) {
	int i,j, axis;
	vec3_t v, av;
	float s, t, dv, *vp;

	// decide which face it maps to
	VectorClear (v);
	for (i = 0, vp = vecs; i < nump; i++, vp += 3)
		VectorAdd (vp, v, v);

	av[0] = fabs(v[0]);
	av[1] = fabs(v[1]);
	av[2] = fabs(v[2]);
	if (av[0] > av[1] && av[0] > av[2])
		axis = (v[0] < 0) ? 1 : 0;
	else if (av[1] > av[2] && av[1] > av[0])
		axis = (v[1] < 0) ? 3 : 2;
	else
		axis = (v[2] < 0) ? 5 : 4;

	// project new texture coords
	for (i = 0; i < nump; i++, vecs += 3) {
		j = vec_to_st[axis][2];
		dv = (j > 0) ? vecs[j - 1] : -vecs[-j - 1];

		j = vec_to_st[axis][0];
		s = (j < 0) ? -vecs[-j -1] / dv : vecs[j-1] / dv;

		j = vec_to_st[axis][1];
		t = (j < 0) ? -vecs[-j -1] / dv : vecs[j-1] / dv;

		if (s < skymins[0][axis])
			skymins[0][axis] = s;
		if (t < skymins[1][axis])
			skymins[1][axis] = t;
		if (s > skymaxs[0][axis])
			skymaxs[0][axis] = s;
		if (t > skymaxs[1][axis])
			skymaxs[1][axis] = t;
	}
}

#define	MAX_CLIP_VERTS	64
void ClipSkyPolygon (int nump, vec3_t vecs, int stage) {
	float *norm, *v, d, e, dists[MAX_CLIP_VERTS];
	qbool front, back;
	int sides[MAX_CLIP_VERTS], newc[2], i, j;
	vec3_t newv[2][MAX_CLIP_VERTS];

	if (nump > MAX_CLIP_VERTS - 2)
		Sys_Error ("ClipSkyPolygon: nump > MAX_CLIP_VERTS - 2");
	if (stage == 6) {	
		// fully clipped, so draw it
		DrawSkyPolygon (nump, vecs);
		return;
	}

	front = back = false;
	norm = skyclip[stage];
	for (i = 0, v = vecs; i < nump; i++, v += 3) {
		d = DotProduct (v, norm);
		if (d > ON_EPSILON) {
			front = true;
			sides[i] = SIDE_FRONT;
		} else if (d < -ON_EPSILON) {
			back = true;
			sides[i] = SIDE_BACK;
		} else {
			sides[i] = SIDE_ON;
		}
		dists[i] = d;
	}

	if (!front || !back) {	
		// not clipped
		ClipSkyPolygon (nump, vecs, stage + 1);
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy (vecs, (vecs + (i * 3)));
	newc[0] = newc[1] = 0;

	for (i = 0, v = vecs; i < nump; i++, v += 3) {
		switch (sides[i]) {
		case SIDE_FRONT:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			break;
		case SIDE_BACK:
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		case SIDE_ON:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		}

		if (sides[i] == SIDE_ON || sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i+1]);
		for (j = 0; j < 3; j++) {
			e = v[j] + d * (v[j + 3] - v[j]);
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon (newc[0], newv[0][0], stage + 1);
	ClipSkyPolygon (newc[1], newv[1][0], stage + 1);
}

/*
=================
R_AddSkyBoxSurface
=================
*/
void R_AddSkyBoxSurface (msurface_t *fa) {
	int i;
	vec3_t verts[MAX_CLIP_VERTS];
	glpoly_t *p;

	// calculate vertex values for sky box
	for (p = fa->polys; p; p = p->next) {
		for (i = 0; i < p->numverts; i++)
			VectorSubtract (p->verts[i], r_origin, verts[i]);
		ClipSkyPolygon (p->numverts, verts[0], 0);
	}
}

void MakeSkyVec(float s, float t, int axis)
{
	vec3_t v, b;
	int j, k;
	float skyrange;

	skyrange = max(r_farclip.value, 4096) * 0.577; // 0.577 < 1/sqrt(3)
	b[0] = s*skyrange;
	b[1] = t*skyrange;
	b[2] = skyrange;

	for (j = 0; j < 3; j++) {
		k = st_to_vec[axis][j];
		v[j] = (k < 0) ? -b[-k - 1] : b[k - 1];
		v[j] += r_origin[j];
	}

	// avoid bilerp seam
	s = (s + 1) * 0.5;
	t = (t + 1) * 0.5;

	s = bound(1.0 / 512, s, 511.0 / 512);
	t = bound(1.0 / 512, t, 511.0 / 512);

	t = 1.0 - t;
	glTexCoord2f(s, t);
	glVertex3fv(v);
}

/*
==============
R_DrawSkyBox
==============
*/
#define SUBDIVISIONS	10

// s and t range from -1 to 1
void MakeSkyVec2(float s, float t, int axis, vec3_t v)
{
	vec3_t		b;
	int			j, k;
	float skyrange;

	skyrange = max(r_farclip.value, 4096) * 0.577; // 0.577 < 1/sqrt(3)
	b[0] = s * skyrange;
	b[1] = t * skyrange;
	b[2] = 1 * skyrange;

	for (j = 0; j < 3; j++) {
		k = st_to_vec[axis][j];
		if (k < 0) {
			v[j] = -b[-k - 1];
		}
		else {
			v[j] = b[k - 1];
		}
	}
}

void EmitSkyVert(vec3_t v, qbool newPoly)
{
	vec3_t dir;
	float s, t;
	float length;

	VectorCopy(v, dir);
	VectorAdd(v, r_origin, v);
	//VectorSubtract(v, r_origin, dir);
	dir[2] *= 3;	// flatten the sphere

	length = VectorLength(dir);
	length = 6 * 63 / length;

	dir[0] *= length;
	dir[1] *= length;

	s = (speedscale + dir[0]) * (1.0 / 128);
	t = (speedscale + dir[1]) * (1.0 / 128);

	glTexCoord2f(s, t);
	glVertex3fv(v);
}

void ClearSky (void)
{
	int i;
	for (i = 0; i < 6; i++) {
		skymins[0][i] = skymins[1][i] = 9999;
		skymaxs[0][i] = skymaxs[1][i] = -9999;
	}
}

qbool R_DetermineSkyLimits(qbool *ignore_z)
{
	if (r_viewleaf->contents == CONTENTS_SOLID) {
		// always draw if we're in a solid leaf (probably outside the level)
		// FIXME: we don't really want to add all six planes every time!
		// FIXME: also handle r_fastsky case
		int i;
		for (i = 0; i < 6; i++) {
			skymins[0][i] = skymins[1][i] = -1;
			skymaxs[0][i] = skymaxs[1][i] = 1;
		}
		*ignore_z = true;
	}
	else {
		msurface_t* fa;

		// no sky at all
		if (!skychain)
			return false;

		// figure out how much of the sky box we need to draw
		ClearSky();
		for (fa = skychain; fa; fa = fa->texturechain) {
			R_AddSkyBoxSurface(fa);
		}

		*ignore_z = false;
	}

	return true;
}

/*
==============
R_DrawSky

Draw either the classic cloudy quake sky or a skybox
==============
*/
void R_DrawSky (void)
{
	if (!skychain) {
		return;
	}

	GL_EnterRegion("R_DrawSky");
	if (GL_ShadersSupported()) {
		GLM_DrawSky();
	}
	else {
		GLC_DrawSky();
	}
	GL_LeaveRegion();

	skychain = NULL;
	skychain_tail = &skychain;
}
