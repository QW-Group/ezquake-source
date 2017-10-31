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
#include "gl_model.h"
#include "gl_local.h"
#include "gl_sky.h"

#define SUBDIVISIONS 10

static void GLC_DrawSkyBox(void);
static void GLC_DrawSkyDome(void);
static void GLC_DrawSkyFace(int axis);
static void GLC_MakeSkyVec(float s, float t, int axis);

static float speedscale, speedscale2;		// for top sky and bottom sky

void GLC_EmitSkyPolys(msurface_t *fa, qbool mtex)
{
	glpoly_t *p;
	float *v, s, t, ss = 0.0, tt = 0.0, length;
	int i;
	vec3_t dir;

	for (p = fa->polys; p; p = p->next) {
		glBegin(GL_POLYGON);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE) {
			VectorSubtract(v, r_origin, dir);
			dir[2] *= 3;	// flatten the sphere

			length = VectorLength(dir);
			length = 6 * 63 / length;

			dir[0] *= length;
			dir[1] *= length;

			if (mtex) {
				s = (speedscale + dir[0]) * (1.0 / 128);
				t = (speedscale + dir[1]) * (1.0 / 128);

				ss = (speedscale2 + dir[0]) * (1.0 / 128);
				tt = (speedscale2 + dir[1]) * (1.0 / 128);
			}
			else {
				s = (speedscale + dir[0]) * (1.0 / 128);
				t = (speedscale + dir[1]) * (1.0 / 128);
			}

			if (mtex) {
				qglMultiTexCoord2f(GL_TEXTURE0, s, t);
				qglMultiTexCoord2f(GL_TEXTURE1, ss, tt);
			}
			else {
				glTexCoord2f(s, t);
			}
			glVertex3fv(v);
		}
		glEnd();
	}
}

void GLC_DrawSkyChain(void)
{
	msurface_t *fa;
	extern cvar_t gl_fogsky;

	if (!skychain) {
		return;
	}

	if (r_fastsky.integer || cl.worldmodel->bspversion == HL_BSPVERSION) {
		GLC_StateBeginFastSky();

		for (fa = skychain; fa; fa = fa->texturechain) {
			glpoly_t* p;

			for (p = fa->polys; p; p = p->next) {
				GLC_DrawFlatPoly(p);
			}
		}

		GLC_StateEndFastSky();
	}
	else if (gl_mtexable) {
		GLC_StateBeginMultiTextureSkyChain();

		speedscale = r_refdef2.time * 8;
		speedscale -= (int)speedscale & ~127;
		speedscale2 = r_refdef2.time * 16;
		speedscale2 -= (int)speedscale2 & ~127;

		for (fa = skychain; fa; fa = fa->texturechain) {
			GLC_EmitSkyPolys(fa, true);
		}

		GLC_StateEndMultiTextureSkyChain();
	}
	else {
		GLC_StateBeginSingleTextureSkyPass();

		speedscale = r_refdef2.time * 8;
		speedscale -= (int)speedscale & ~127;

		for (fa = skychain; fa; fa = fa->texturechain) {
			GLC_EmitSkyPolys(fa, false);
		}

		GLC_StateBeginSingleTextureCloudPass();

		speedscale = r_refdef2.time * 16;
		speedscale -= (int)speedscale & ~127;

		for (fa = skychain; fa; fa = fa->texturechain) {
			GLC_EmitSkyPolys(fa, false);
		}

		GLC_StateEndSingleTextureSky();
	}
}

void GLC_DrawSky(void)
{
	msurface_t	*fa;
	qbool		ignore_z;
	extern msurface_t *skychain;

	if (r_fastsky.integer) {
		GLC_StateBeginFastSky();

		for (fa = skychain; fa; fa = fa->texturechain) {
			glpoly_t *p;

			for (p = fa->polys; p; p = p->next) {
				GLC_DrawFlatPoly(p);
			}
		}

		GLC_StateEndFastSky();

		skychain = NULL;
		return;
	}

	if (!R_DetermineSkyLimits(&ignore_z)) {
		return;
	}

	// turn off Z tests & writes to avoid problems on large maps
	GLC_StateBeginSky();

	// draw a skybox or classic quake clouds
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

		for (fa = skychain; fa; fa = fa->texturechain) {
			glpoly_t *p;

			for (p = fa->polys; p; p = p->next) {
				GLC_DrawFlatPoly(p);
			}
		}

		GLC_StateEndSkyZBufferPass();
	}
	else {
		GLC_StateEndSkyNoZBufferPass();
	}
}

static void EmitSkyVert(vec3_t v)
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

static void GLC_MakeSkyVec2(float s, float t, int axis, float range, vec3_t v)
{
	Sky_MakeSkyVec2(s, t, axis, v);

	v[0] *= range;
	v[1] *= range;
	v[2] *= range;
}

static void GLC_DrawSkyFace(int axis)
{
	int i, j;
	vec3_t	vecs[4];
	float s, t;
	int v;
	float fstep = 2.0 / SUBDIVISIONS;
	float skyrange = max(r_farclip.value, 4096) * 0.577; // 0.577 < 1/sqrt(3)

	glBegin(GL_QUADS);
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

			EmitSkyVert(vecs[0]);
			EmitSkyVert(vecs[1]);
			EmitSkyVert(vecs[2]);
			EmitSkyVert(vecs[3]);
		}
	}
	glEnd();
}

static void GLC_DrawSkyDome(void)
{
	int i;

	GLC_StateBeginSkyDome();

	speedscale = r_refdef2.time * 8;
	speedscale -= (int)speedscale & ~127;

	for (i = 0; i < 6; i++) {
		if ((skymins[0][i] >= skymaxs[0][i] || skymins[1][i] >= skymaxs[1][i])) {
			continue;
		}
		GLC_DrawSkyFace(i);
	}

	GLC_StateBeginSkyDomeCloudPass();

	speedscale = r_refdef2.time * 16;
	speedscale -= (int)speedscale & ~127;

	//skyVerts = 0;
	for (i = 0; i < 6; i++) {
		if ((skymins[0][i] >= skymaxs[0][i] || skymins[1][i] >= skymaxs[1][i])) {
			continue;
		}
		GLC_DrawSkyFace(i);
	}
}

static void GLC_DrawSkyBox(void)
{
	static int skytexorder[MAX_SKYBOXTEXTURES] = {0,2,1,3,4,5};
	int i;

	for (i = 0; i < MAX_SKYBOXTEXTURES; i++) {
		if ((skymins[0][i] >= skymaxs[0][i] || skymins[1][i] >= skymaxs[1][i])) {
			continue;
		}

		GL_BindTextureUnit(GL_TEXTURE0, skyboxtextures[(int)bound(0, skytexorder[i], MAX_SKYBOXTEXTURES - 1)]);

		glBegin(GL_QUADS);
		GLC_MakeSkyVec(skymins[0][i], skymins[1][i], i);
		GLC_MakeSkyVec(skymins[0][i], skymaxs[1][i], i);
		GLC_MakeSkyVec(skymaxs[0][i], skymaxs[1][i], i);
		GLC_MakeSkyVec(skymaxs[0][i], skymins[1][i], i);
		glEnd();
	}
}

static void GLC_MakeSkyVec(float s, float t, int axis)
{
	vec3_t v, b;
	float skyrange = max(r_farclip.value, 4096) * 0.577; // 0.577 < 1/sqrt(3)

	Sky_MakeSkyVec2(s, t, axis, b);
	VectorMA(r_origin, skyrange, b, v);

	// avoid bilerp seam
	s = (s + 1) * 0.5;
	t = (t + 1) * 0.5;

	s = bound(1.0 / 512, s, 511.0 / 512);
	t = bound(1.0 / 512, t, 511.0 / 512);

	t = 1.0 - t;
	glTexCoord2f(s, t);
	glVertex3fv(v);
}

qbool GLC_LoadSkyboxTextures(char* skyname)
{
	return Sky_LoadSkyboxTextures(skyname);
}
