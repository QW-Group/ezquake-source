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

#ifdef RENDERER_OPTION_CLASSIC_OPENGL

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "r_brushmodel_sky.h"
#include "glc_local.h"
#include "r_brushmodel_sky.h"
#include "r_renderer.h"
#include "tr_types.h"
#include "r_brushmodel.h"
#include "r_program.h"
#include "r_matrix.h"

#define SUBDIVISIONS 10

static float skymins[2][6], skymaxs[2][6];

static void GLC_DrawSkyBox(void);
static void GLC_DrawSkyDome(void);
static void GLC_DrawSkyFace(int axis, qbool multitexture);
static void GLC_MakeSkyVec(float s, float t, int axis);

static void R_SkyAddBoxSurface(msurface_t *fa);
static void R_SkyClearVisibleLimits(void);

static float speedscale, speedscale2;		// for sky and clouds, respectively

typedef enum {
	skypoly_mode_mtex,
	skypoly_mode_sky,
	skypoly_mode_clouds,
} skypoly_mode_id;

static void GLC_DrawSky_Program(void);
qbool GLC_SkyProgramCompile(void);
extern cvar_t gl_program_sky;

/*
==============
R_DrawSkyBox
==============
*/

// 1 = s, 2 = t, 3 = 2048
static int st_to_vec[6][3] = {
	{3,-1,2},
	{-3,1,2},

	{1,3,2},
	{-1,-3,2},

	{-2,-1,3},		// 0 degrees yaw, look straight up
	{2,-1,-3}		// look straight down
};

// s = [0]/[2], t = [1]/[2]
static int vec_to_st[6][3] = {
	{-2,3,1},
	{2,3,-1},

	{1,3,2},
	{-1,3,-2},

	{-2,-1,3},
	{-2,1,-3}
};

// s and t range from -1 to 1
static void Sky_MakeSkyVec2(float s, float t, int axis, vec3_t v)
{
	vec3_t		b;
	int			j, k;
	b[0] = s;
	b[1] = t;
	b[2] = 1;

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

static void GLC_DrawFlatSkyPoly(glpoly_t* p, qbool texture)
{
	int i;

	R_TraceAPI("%s()", __func__);

	GLC_Begin(GL_TRIANGLE_STRIP);
	for (i = 0; i < p->numverts; ++i) {
		float* v = p->verts[i];
		if (texture) {
			vec3_t direction;
			
			VectorSubtract(v, r_refdef.vieworg, direction);

			glTexCoord3f(direction[0], direction[2], direction[1]);
		}
		GLC_Vertex3fv(v);
	}
	GLC_End();
}

static void GLC_DrawFastSkyChain(void)
{
	qbool use_vbo = buffers.supported && modelIndexes;
	int index_count = 0;
	msurface_t* fa;

	for (fa = skychain; fa; fa = fa->texturechain) {
		glpoly_t *p;

		for (p = fa->polys; p; p = p->next) {
			if (use_vbo) {
				index_count = GLC_DrawIndexedPoly(p, modelIndexes, modelIndexMaximum, index_count);
			}
			else {
				GLC_DrawFlatSkyPoly(p, false);
			}
		}
	}
	if (index_count) {
		GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
	}
}

static void GLC_EmitSkyPolys(msurface_t *fa, skypoly_mode_id mode)
{
	glpoly_t *p;
	float *v, s, t, ss = 0.0, tt = 0.0, length;
	int i;
	vec3_t dir;

	R_TraceAPI("%s()", __func__);
	for (p = fa->polys; p; p = p->next) {
		GLC_Begin(GL_TRIANGLE_STRIP);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
			VectorSubtract(v, r_origin, dir);
			dir[2] *= 3;	// flatten the sphere

			length = VectorLength(dir);
			length = 6 * 63 / length;

			dir[0] *= length;
			dir[1] *= length;

			s = (speedscale + dir[0]) * (1.0 / 128);
			t = (speedscale + dir[1]) * (1.0 / 128);

			ss = (speedscale2 + dir[0]) * (1.0 / 128);
			tt = (speedscale2 + dir[1]) * (1.0 / 128);

			switch (mode) {
				case skypoly_mode_mtex:
					GLC_MultiTexCoord2f(GL_TEXTURE0, s, t);
					GLC_MultiTexCoord2f(GL_TEXTURE1, ss, tt);
					break;
				case skypoly_mode_sky:
					glTexCoord2f(s, t);
					break;
				case skypoly_mode_clouds:
					glTexCoord2f(ss, tt);
					break;
			}

			GLC_Vertex3fv(v);
		}
		GLC_End();
	}
}

// This is used for brushmodel entity surfaces only
void GLC_SkyDrawChainedSurfaces(void)
{
	msurface_t *fa;

	if (!skychain) {
		return;
	}

	if (gl_program_sky.integer && GL_Supported(R_SUPPORT_RENDERING_SHADERS) && GLC_SkyProgramCompile()) {
		GLC_DrawSky_Program();
	}
	else {
		R_ProgramUse(r_program_none);
		if (r_fastsky.integer || cl.worldmodel->bspversion == HL_BSPVERSION) {
			GLC_StateBeginFastSky(false);
			GLC_DrawFastSkyChain();
		}
		else if (gl_mtexable) {
			GLC_StateBeginMultiTextureSkyChain();

			speedscale = r_refdef2.time * 8;
			speedscale -= (int)speedscale & ~127;
			speedscale2 = r_refdef2.time * 16;
			speedscale2 -= (int)speedscale2 & ~127;

			for (fa = skychain; fa; fa = fa->texturechain) {
				GLC_EmitSkyPolys(fa, skypoly_mode_mtex);
			}
		}
		else {
			GLC_StateBeginSingleTextureSkyPass();

			speedscale = r_refdef2.time * 8;
			speedscale -= (int)speedscale & ~127;

			for (fa = skychain; fa; fa = fa->texturechain) {
				GLC_EmitSkyPolys(fa, skypoly_mode_sky);
			}

			GLC_StateBeginSingleTextureCloudPass();

			speedscale = r_refdef2.time * 16;
			speedscale -= (int)speedscale & ~127;

			for (fa = skychain; fa; fa = fa->texturechain) {
				GLC_EmitSkyPolys(fa, skypoly_mode_clouds);
			}
		}
	}

	skychain = NULL;
	skychain_tail = &skychain;
}

static qbool R_DetermineSkyLimits(qbool *ignore_z)
{
	if (r_viewleaf->contents == CONTENTS_SOLID) {
		// always draw if we're in a solid leaf (probably outside the level)
		// FIXME: we don't really want to add all six planes every time!
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
		if (!skychain) {
			return false;
		}

		// figure out how much of the sky box we need to draw
		R_SkyClearVisibleLimits();
		for (fa = skychain; fa; fa = fa->texturechain) {
			R_SkyAddBoxSurface(fa);
		}

		*ignore_z = false;
	}

	return true;
}

#define PROGRAMFLAGS_SKYBOX       1
#define PROGRAMFLAGS_SKYWIND      2

qbool GLC_SkyProgramCompile(void)
{
	int flags = (r_skyboxloaded && R_UseCubeMapForSkyBox() ? PROGRAMFLAGS_SKYBOX : 0);
	if (flags && Skywind_Active()) {
		flags |= PROGRAMFLAGS_SKYWIND;
	}

	if (R_ProgramRecompileNeeded(r_program_sky_glc, flags)) {
		static char included_definitions[512];

		memset(included_definitions, 0, sizeof(included_definitions));
		if (flags & PROGRAMFLAGS_SKYBOX) {
			strlcat(included_definitions, "#define DRAW_SKYBOX\n", sizeof(included_definitions));
			if (flags & PROGRAMFLAGS_SKYWIND) {
				strlcat(included_definitions, "#define DRAW_SKYWIND\n", sizeof(included_definitions));
			}
		}

		R_ProgramCompileWithInclude(r_program_sky_glc, included_definitions);
		if (flags & PROGRAMFLAGS_SKYBOX) {
			R_ProgramUniform1i(r_program_uniform_sky_glc_skyTex, 0);
		}
		else {
			R_ProgramUniform1i(r_program_uniform_sky_glc_skyDomeTex, 0);
			R_ProgramUniform1i(r_program_uniform_sky_glc_skyDomeCloudTex, 1);
		}
		R_ProgramSetCustomOptions(r_program_sky_glc, flags);
	}

	R_ProgramSetStandardUniforms(r_program_sky_glc);

	return R_ProgramReady(r_program_sky_glc);
}

static void GLC_DrawSky_Program(void)
{
	extern msurface_t *skychain;
	float skySpeedscale = r_refdef2.time * 8;
	float skySpeedscale2 = r_refdef2.time * 16;
	float skyWind[4];

	skySpeedscale -= (int)skySpeedscale & ~127;
	skySpeedscale2 -= (int)skySpeedscale2 & ~127;

	skySpeedscale /= 128.0f;
	skySpeedscale2 /= 128.0f;

	R_TraceAPI("%s()", __func__);
	R_ProgramUse(r_program_sky_glc);
	R_ProgramUniform1f(r_program_uniform_sky_glc_speedscale, skySpeedscale);
	R_ProgramUniform1f(r_program_uniform_sky_glc_speedscale2, skySpeedscale2);
	R_ProgramUniform3fv(r_program_uniform_sky_glc_cameraPosition, r_refdef.vieworg);
	if (Skywind_GetDirectionAndPhase(skyWind, &skyWind[3])) {
		R_ProgramUniform4fv(r_program_uniform_sky_glc_skyWind, skyWind);
	}

	if (r_skyboxloaded && R_UseCubeMapForSkyBox()) {
		extern texture_ref skybox_cubeMap;

		glEnable(GL_TEXTURE_CUBE_MAP);
		R_ApplyRenderingState(r_state_skybox);
		renderer.TextureUnitBind(0, skybox_cubeMap);
		GLC_DrawFastSkyChain();
		glDisable(GL_TEXTURE_CUBE_MAP);
	}
	else {
		GLC_StateBeginMultiTextureSkyDome(true);
		GLC_DrawFastSkyChain();
	}

	R_ProgramUse(r_program_none);
	skychain = NULL;
}

void GLC_DrawSky(void)
{
	msurface_t	*fa;
	qbool		ignore_z;
	extern msurface_t *skychain;

	R_TraceAPI("%s()", __func__);
	if (gl_program_sky.integer && GL_Supported(R_SUPPORT_RENDERING_SHADERS) && GLC_SkyProgramCompile()) {
		GLC_DrawSky_Program();
	}
	else if (r_fastsky.integer) {
		R_ProgramUse(r_program_none);
		GLC_StateBeginFastSky(true);
		GLC_DrawFastSkyChain();
	}
	else if (r_skyboxloaded && R_UseCubeMapForSkyBox()) {
		extern texture_ref skybox_cubeMap;

		R_ProgramUse(r_program_none);
		R_ApplyRenderingState(r_state_skybox);
		renderer.TextureUnitBind(0, skybox_cubeMap);
		GL_BuiltinProcedure(glEnable, "cap=GL_TEXTURE_CUBE_MAP", GL_TEXTURE_CUBE_MAP);

		for (fa = skychain; fa; fa = fa->texturechain) {
			glpoly_t *p;

			for (p = fa->polys; p; p = p->next) {
				GLC_DrawFlatSkyPoly(p, true);
			}
		}

		GL_BuiltinProcedure(glDisable, "cap=GL_TEXTURE_CUBE_MAP", GL_TEXTURE_CUBE_MAP);
	}
	else if (R_DetermineSkyLimits(&ignore_z)) {
		// draw a skybox or classic quake clouds
		R_ProgramUse(r_program_none);
		if (r_skyboxloaded) {
			GLC_DrawSkyBox();
		}
		else {
			GLC_DrawSkyDome();
		}

		// draw the sky polys into the Z buffer
		// don't need depth test yet
		if (!ignore_z) {
			GLC_StateBeginSkyZBufferPass();
			GLC_DrawFastSkyChain();
		}
	}

	skychain = NULL;
	return;
}

static void EmitSkyVert(vec3_t v, qbool multitexture)
{
	vec3_t dir;
	float length;

	VectorCopy(v, dir);
	VectorAdd(v, r_origin, v);
	//VectorSubtract(v, r_origin, dir);
	dir[2] *= 3;	// flatten the sphere

	length = VectorLength(dir);
	length = 6 * 63 / length;

	dir[0] *= length;
	dir[1] *= length;

	if (multitexture) {
		GLC_MultiTexCoord2f(GL_TEXTURE0, (speedscale + dir[0]) * (1.0 / 128), (speedscale + dir[1]) * (1.0 / 128));

		GLC_MultiTexCoord2f(GL_TEXTURE1, (speedscale2 + dir[0]) * (1.0 / 128), (speedscale2 + dir[1]) * (1.0 / 128));
	}
	else {
		glTexCoord2f((speedscale + dir[0]) * (1.0 / 128), (speedscale + dir[1]) * (1.0 / 128));
	}
	GLC_Vertex3fv(v);
}

static void GLC_MakeSkyVec2(float s, float t, int axis, float range, vec3_t v)
{
	Sky_MakeSkyVec2(s, t, axis, v);

	v[0] *= range;
	v[1] *= range;
	v[2] *= range;
}

static void GLC_DrawSkyFace(int axis, qbool multitexture)
{
	int i, j;
	vec3_t vecs[4];
	float s, t;
	int v;
	float fstep = 2.0 / SUBDIVISIONS;
	float skyrange = R_FarPlaneZ() * 0.577; // 0.577 < 1/sqrt(3)

	GLC_Begin(GL_QUADS);
	for (v = 0, i = 0; i < SUBDIVISIONS; i++, v++)
	{
		s = (float)(i*2 - SUBDIVISIONS) / SUBDIVISIONS;

		if (s + fstep < skymins[0][axis] || s > skymaxs[0][axis]) {
			continue;
		}

		for (j = 0; j < SUBDIVISIONS; j++, v++) {
			t = (float)(j * 2 - SUBDIVISIONS) / SUBDIVISIONS;

			if (t + fstep < skymins[1][axis] || t > skymaxs[1][axis]) {
				continue;
			}

			GLC_MakeSkyVec2(s, t, axis, skyrange, vecs[0]);
			GLC_MakeSkyVec2(s, t + fstep, axis, skyrange, vecs[1]);
			GLC_MakeSkyVec2(s + fstep, t + fstep, axis, skyrange, vecs[2]);
			GLC_MakeSkyVec2(s + fstep, t, axis, skyrange, vecs[3]);

			EmitSkyVert(vecs[0], multitexture);
			EmitSkyVert(vecs[1], multitexture);
			EmitSkyVert(vecs[2], multitexture);
			EmitSkyVert(vecs[3], multitexture);
		}
	}
	GLC_End();
}

static void GLC_DrawSkyDome(void)
{
	int i;

	if (gl_mtexable) {
		GLC_StateBeginMultiTextureSkyDome(false);
	}
	else {
		GLC_StateBeginSingleTextureSkyDome();
	}

	speedscale = r_refdef2.time * 8;
	speedscale -= (int)speedscale & ~127;

	speedscale2 = r_refdef2.time * 16;
	speedscale2 -= (int)speedscale2 & ~127;

	for (i = 0; i < 6; i++) {
		if ((skymins[0][i] >= skymaxs[0][i] || skymins[1][i] >= skymaxs[1][i])) {
			continue;
		}
		GLC_DrawSkyFace(i, gl_mtexable);
	}

	if (!gl_mtexable) {
		GLC_StateBeginSingleTextureSkyDomeCloudPass();

		speedscale = speedscale2;

		for (i = 0; i < 6; i++) {
			if ((skymins[0][i] >= skymaxs[0][i] || skymins[1][i] >= skymaxs[1][i])) {
				continue;
			}
			GLC_DrawSkyFace(i, false);
		}
	}
}

static void GLC_DrawSkyBox(void)
{
	static int skytexorder[MAX_SKYBOXTEXTURES] = {0,2,1,3,4,5};
	int i;

	R_ApplyRenderingState(r_state_skybox);

	if (R_UseCubeMapForSkyBox()) {
		extern texture_ref skybox_cubeMap;

		renderer.TextureUnitBind(0, skybox_cubeMap);
		glEnable(GL_TEXTURE_CUBE_MAP);
	}

	for (i = 0; i < MAX_SKYBOXTEXTURES; i++) {
		if ((skymins[0][i] >= skymaxs[0][i] || skymins[1][i] >= skymaxs[1][i])) {
			continue;
		}

		if (!R_UseCubeMapForSkyBox()) {
			renderer.TextureUnitBind(0, skyboxtextures[(int)bound(0, skytexorder[i], MAX_SKYBOXTEXTURES - 1)]);
		}

		GLC_Begin(GL_QUADS);
		GLC_MakeSkyVec(skymins[0][i], skymins[1][i], i);
		GLC_MakeSkyVec(skymins[0][i], skymaxs[1][i], i);
		GLC_MakeSkyVec(skymaxs[0][i], skymaxs[1][i], i);
		GLC_MakeSkyVec(skymaxs[0][i], skymins[1][i], i);
		GLC_End();
	}

	if (R_UseCubeMapForSkyBox()) {
		extern texture_ref skybox_cubeMap;

		renderer.TextureUnitBind(0, skybox_cubeMap);
		glDisable(GL_TEXTURE_CUBE_MAP);
	}
}

static void GLC_MakeSkyVec(float s, float t, int axis)
{
	vec3_t v, b;
	float skyrange = R_FarPlaneZ() * 0.577; // 0.577 < 1/sqrt(3)

	Sky_MakeSkyVec2(s, t, axis, b);
	VectorMA(r_origin, skyrange, b, v);

	if (R_UseCubeMapForSkyBox()) {
		glTexCoord3f(v[0], v[2], v[1]);
	}
	else {
		// avoid bilerp seam
		s = (s + 1) * 0.5;
		t = (t + 1) * 0.5;

		s = bound(1.0 / 512, s, 511.0 / 512);
		t = bound(1.0 / 512, t, 511.0 / 512);

		t = 1.0 - t;

		glTexCoord2f(s, t);
	}
	GLC_Vertex3fv(v);
}

static vec3_t skyclip[6] = {
	{ 1,1,0 },
	{ 1,-1,0 },
	{ 0,-1,1 },
	{ 0,1,1 },
	{ 1,0,1 },
	{ -1,0,1 }
};

#define skybox_range	2400.0

static void R_SkyPolygonCalcLimits(int nump, vec3_t vecs)
{
	int i, j, axis;
	vec3_t v, av;
	float s, t, dv, *vp;

	// decide which face it maps to
	VectorClear(v);
	for (i = 0, vp = vecs; i < nump; i++, vp += 3) {
		VectorAdd(vp, v, v);
	}

	av[0] = fabs(v[0]);
	av[1] = fabs(v[1]);
	av[2] = fabs(v[2]);
	if (av[0] > av[1] && av[0] > av[2]) {
		axis = (v[0] < 0) ? 1 : 0;
	}
	else if (av[1] > av[2] && av[1] > av[0]) {
		axis = (v[1] < 0) ? 3 : 2;
	}
	else {
		axis = (v[2] < 0) ? 5 : 4;
	}

	// project new texture coords
	for (i = 0; i < nump; i++, vecs += 3) {
		j = vec_to_st[axis][2];
		dv = (j > 0) ? vecs[j - 1] : -vecs[-j - 1];

		j = vec_to_st[axis][0];
		s = (j < 0) ? -vecs[-j - 1] / dv : vecs[j - 1] / dv;

		j = vec_to_st[axis][1];
		t = (j < 0) ? -vecs[-j - 1] / dv : vecs[j - 1] / dv;

		if (s < skymins[0][axis]) {
			skymins[0][axis] = s;
		}
		if (t < skymins[1][axis]) {
			skymins[1][axis] = t;
		}
		if (s > skymaxs[0][axis]) {
			skymaxs[0][axis] = s;
		}
		if (t > skymaxs[1][axis]) {
			skymaxs[1][axis] = t;
		}
	}
}

#define	MAX_CLIP_VERTS	64

static void R_SkyPolygonClip(int nump, vec3_t vecs, int stage)
{
	float *norm, *v, d, e, dists[MAX_CLIP_VERTS];
	qbool front, back;
	int sides[MAX_CLIP_VERTS], newc[2], i, j;
	vec3_t newv[2][MAX_CLIP_VERTS];

	if (nump > MAX_CLIP_VERTS - 2) {
		Sys_Error("ClipSkyPolygon: nump > MAX_CLIP_VERTS - 2");
	}
	if (stage == 6) {
		// fully clipped, so draw it
		R_SkyPolygonCalcLimits(nump, vecs);
		return;
	}

	front = back = false;
	norm = skyclip[stage];
	for (i = 0, v = vecs; i < nump; i++, v += 3) {
		d = DotProduct(v, norm);
		if (d > ON_EPSILON) {
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if (d < -ON_EPSILON) {
			back = true;
			sides[i] = SIDE_BACK;
		}
		else {
			sides[i] = SIDE_ON;
		}
		dists[i] = d;
	}

	if (!front || !back) {
		// not clipped
		R_SkyPolygonClip(nump, vecs, stage + 1);
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy(vecs, (vecs + (i * 3)));
	newc[0] = newc[1] = 0;

	for (i = 0, v = vecs; i < nump; i++, v += 3) {
		switch (sides[i]) {
			case SIDE_FRONT:
				VectorCopy(v, newv[0][newc[0]]);
				newc[0]++;
				break;
			case SIDE_BACK:
				VectorCopy(v, newv[1][newc[1]]);
				newc[1]++;
				break;
			case SIDE_ON:
				VectorCopy(v, newv[0][newc[0]]);
				newc[0]++;
				VectorCopy(v, newv[1][newc[1]]);
				newc[1]++;
				break;
		}

		if (sides[i] == SIDE_ON || sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i + 1]);
		for (j = 0; j < 3; j++) {
			e = v[j] + d * (v[j + 3] - v[j]);
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	R_SkyPolygonClip(newc[0], newv[0][0], stage + 1);
	R_SkyPolygonClip(newc[1], newv[1][0], stage + 1);
}

/*
=================
R_AddSkyBoxSurface
=================
*/
static void R_SkyAddBoxSurface(msurface_t *fa)
{
	int i, j, v;
	vec3_t verts[MAX_CLIP_VERTS];
	glpoly_t *p;

	// calculate vertex values for sky box
	for (p = fa->polys; p; p = p->next) {
		// Meag: the R_SkyPolygonClip() requires verts in convex polygon order, i'm not messing
		//       with it... so we add the verts in a weird order to get back from triangle strip
		//       to verts
		VectorSubtract(p->verts[0], r_origin, verts[0]);
		for (v = 1, i = 1, j = p->numverts - 1; v < p->numverts; ++v) {
			if (v % 2) {
				VectorSubtract(p->verts[v], r_origin, verts[i]);
				++i;
			}
			else {
				VectorSubtract(p->verts[v], r_origin, verts[j]);
				--j;
			}
		}
		R_SkyPolygonClip(p->numverts, verts[0], 0);
	}
}

static void R_SkyClearVisibleLimits(void)
{
	int i;
	for (i = 0; i < 6; i++) {
		skymins[0][i] = skymins[1][i] = 9999;
		skymaxs[0][i] = skymaxs[1][i] = -9999;
	}
}

#endif // #ifdef RENDERER_OPTION_CLASSIC_OPENGL
