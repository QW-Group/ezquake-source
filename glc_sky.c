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

	GL_DisableMultitexture();

	if (gl_fogsky.value) {
		GL_EnableFog();
	}

	if (r_fastsky.value || cl.worldmodel->bspversion == HL_BSPVERSION) {
		glDisable(GL_TEXTURE_2D);

		glColor3ubv(r_skycolor.color);

		for (fa = skychain; fa; fa = fa->texturechain) {
			glpoly_t* p;

			for (p = fa->polys; p; p = p->next) {
				GLC_DrawFlatPoly(p);
			}
		}

		glEnable(GL_TEXTURE_2D);
		glColor3ubv(color_white);
	}
	else if (gl_mtexable) {
		GL_TextureEnvMode(GL_MODULATE);
		GL_Bind(solidskytexture);

		GL_EnableMultitexture();
		GL_TextureEnvMode(GL_DECAL);
		GL_Bind(alphaskytexture);

		speedscale = r_refdef2.time * 8;
		speedscale -= (int)speedscale & ~127;
		speedscale2 = r_refdef2.time * 16;
		speedscale2 -= (int)speedscale2 & ~127;

		for (fa = skychain; fa; fa = fa->texturechain) {
			GLC_EmitSkyPolys(fa, true);
		}

		GL_DisableMultitexture();
		GL_TextureEnvMode(GL_REPLACE);
	}
	else {
		GL_Bind(solidskytexture);
		speedscale = r_refdef2.time * 8;
		speedscale -= (int)speedscale & ~127;

		for (fa = skychain; fa; fa = fa->texturechain) {
			GLC_EmitSkyPolys(fa, false);
		}

		GL_AlphaBlendFlags(GL_BLEND_ENABLED);
		GL_Bind(alphaskytexture);

		speedscale = r_refdef2.time * 16;
		speedscale -= (int)speedscale & ~127;

		for (fa = skychain; fa; fa = fa->texturechain) {
			GLC_EmitSkyPolys(fa, false);
		}

		GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	}

	if (gl_fogsky.value) {
		GL_DisableFog();
	}
}

void GLC_DrawSky(void)
{
	msurface_t	*fa;
	qbool		ignore_z;
	extern msurface_t *skychain;

	GL_DisableMultitexture();

	if (r_fastsky.value) {
		glDisable(GL_TEXTURE_2D);
		glColor3ubv(r_skycolor.color);

		for (fa = skychain; fa; fa = fa->texturechain) {
			glpoly_t *p;

			for (p = fa->polys; p; p = p->next) {
				GLC_DrawFlatPoly(p);
			}
		}
		skychain = NULL;

		glEnable(GL_TEXTURE_2D);
		glColor3f(1, 1, 1);
		return;
	}

	if (!R_DetermineSkyLimits(&ignore_z)) {
		return;
	}

	// turn off Z tests & writes to avoid problems on large maps
	glDisable(GL_DEPTH_TEST);

	// draw a skybox or classic quake clouds
	if (r_skyboxloaded) {
		GLC_DrawSkyBox();
	}
	else {
		GLC_DrawSkyDome();
	}

	glEnable(GL_DEPTH_TEST);

	// draw the sky polys into the Z buffer
	// don't need depth test yet
	if (!ignore_z) {
		if (gl_fogenable.value && gl_fogsky.value) {
			GL_EnableFog();
			glColor4f(gl_fogred.value, gl_foggreen.value, gl_fogblue.value, 1);
			glBlendFunc(GL_ONE, GL_ZERO);
		}
		else {
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			glBlendFunc(GL_ZERO, GL_ONE);
		}
		glDisable(GL_TEXTURE_2D);
		GL_AlphaBlendFlags(GL_BLEND_ENABLED);

		for (fa = skychain; fa; fa = fa->texturechain) {
			glpoly_t *p;

			for (p = fa->polys; p; p = p->next) {
				GLC_DrawFlatPoly(p);
			}
		}

		if (gl_fogenable.value && gl_fogsky.value) {
			GL_DisableFog();
		}
		else {
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		}
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_TEXTURE_2D);
		GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	}
}

static void GLC_DrawSkyFace(int axis)
{
	int i, j;
	vec3_t	vecs[4];
	float s, t;
	int v;
	float fstep = 2.0 / SUBDIVISIONS;

	glBegin(GL_QUADS);
	for (v = 0, i = 0; i < SUBDIVISIONS; i++, v++)
	{
		s = (float)(i*2 - SUBDIVISIONS) / SUBDIVISIONS;

		if (s + fstep < skymins[0][axis] || s > skymaxs[0][axis])
			continue;

		for (j = 0; j < SUBDIVISIONS; j++, v++) {
			t = (float)(j*2 - SUBDIVISIONS) / SUBDIVISIONS;

			if (t + fstep < skymins[1][axis] || t > skymaxs[1][axis])
				continue;

			{
				float scale_ = max(r_farclip.value, 4096) * 0.577;

				MakeSkyVec2(s, t, axis, vecs[0]);
				MakeSkyVec2(s, t + fstep, axis, vecs[1]);
				MakeSkyVec2(s + fstep, t + fstep, axis, vecs[2]);
				MakeSkyVec2(s + fstep, t, axis, vecs[3]);

				/*
				VectorAdd(vecs[0], r_origin, vecs[0]);
				VectorAdd(vecs[1], r_origin, vecs[1]);
				VectorAdd(vecs[2], r_origin, vecs[2]);
				VectorAdd(vecs[3], r_origin, vecs[3]);
				VectorScale(vecs[0], scale_, vecs[0]);
				VectorScale(vecs[1], scale_, vecs[1]);
				VectorScale(vecs[2], scale_, vecs[2]);
				VectorScale(vecs[3], scale_, vecs[3]);
				*/
				EmitSkyVert(vecs[0], false);
				EmitSkyVert(vecs[1], false);
				EmitSkyVert(vecs[2], false);
				EmitSkyVert(vecs[3], false);
			}
		}
	}
	glEnd();
}

static void GLC_DrawSkyDome(void)
{
	int i;

	GL_DisableMultitexture();
	GL_Bind(solidskytexture);

	GL_AlphaBlendFlags(GL_BLEND_DISABLED);

	speedscale = r_refdef2.time * 8;
	speedscale -= (int)speedscale & ~127;

	for (i = 0; i < 6; i++) {
		if ((skymins[0][i] >= skymaxs[0][i] || skymins[1][i] >= skymaxs[1][i]))
			continue;
		GLC_DrawSkyFace(i);
	}

	GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	GL_Bind(alphaskytexture);

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

		GL_Bind(skyboxtextures[(int)bound(0, skytexorder[i], MAX_SKYBOXTEXTURES - 1)]);

		glBegin(GL_QUADS);
		MakeSkyVec(skymins[0][i], skymins[1][i], i);
		MakeSkyVec(skymins[0][i], skymaxs[1][i], i);
		MakeSkyVec(skymaxs[0][i], skymaxs[1][i], i);
		MakeSkyVec(skymaxs[0][i], skymins[1][i], i);
		glEnd();
	}
}
