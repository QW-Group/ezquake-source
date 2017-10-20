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
#include "gl_sky.h"

static void GLM_DrawSkyDome(void);

#define SUBDIVISIONS	10
#define MAX_SKYPOLYS (SUBDIVISIONS * SUBDIVISIONS * 6)
#define FLOATS_PER_SKYVERT 5
static float skydomeVertData[(MAX_SKYPOLYS * 4 + (MAX_SKYPOLYS - 1) * 2) * FLOATS_PER_SKYVERT];
static int skyDomeVertices = 0;

static glm_program_t skyDome;
static GLint skyDome_modelView;
static GLint skyDome_projection;
static GLint skyDome_farclip;
static GLint skyDome_speedscale;
static GLint skyDome_speedscale2;
static GLint skyDome_skyTex;
static GLint skyDome_alphaTex;
static GLint skyDome_origin;
static GLuint skyDome_vbo;
static GLuint skyDome_vao;
static GLint skyDome_starts[6];
static GLsizei skyDome_length[6];

void GLM_DrawSkyChain(void)
{
	// FIXME: This is called for sky chains on entities... don't know of any maps to test this on
	//  Have disabled in modern for the moment.
	return;
}

static int AddSkyDomeVert(int vert, vec3_t pos, float s, float t)
{
	int index = vert * FLOATS_PER_SKYVERT;

	skydomeVertData[index] = pos[0];
	skydomeVertData[index+1] = pos[1];
	skydomeVertData[index+2] = pos[2];
	skydomeVertData[index+3] = s;
	skydomeVertData[index+4] = t;

	return vert + 1;
}

static void BuildSkyVertsArray(void)
{
	int i, j;
	float s, t;
	int vert = 0;
	int axis = 0;
	int k;

	if (skyDomeVertices) {
		return;
	}

	if (!skyDome.program) {
		GL_VFDeclare(skydome)

			GLM_CreateVFProgram("SkyDome", GL_VFParams(skydome), &skyDome);

		skyDome_modelView = glGetUniformLocation(skyDome.program, "modelView");
		skyDome_projection = glGetUniformLocation(skyDome.program, "projection");
		skyDome_farclip = glGetUniformLocation(skyDome.program, "farclip");
		skyDome_speedscale = glGetUniformLocation(skyDome.program, "speedscale");
		skyDome_speedscale2 = glGetUniformLocation(skyDome.program, "speedscale2");
		skyDome_skyTex = glGetUniformLocation(skyDome.program, "skyTex");
		skyDome_alphaTex = glGetUniformLocation(skyDome.program, "alphaTex");
		skyDome_origin = glGetUniformLocation(skyDome.program, "origin");
	}

	for (axis = 0; axis < 6; ++axis) {
		float fstep = 2.0 / SUBDIVISIONS;

		skyDome_starts[axis] = vert;
		for (i = 0; i < SUBDIVISIONS; i++) {
			s = (float)(i * 2 - SUBDIVISIONS) / SUBDIVISIONS;

			for (j = 0; j < SUBDIVISIONS; j++) {
				t = (float)(j * 2 - SUBDIVISIONS) / SUBDIVISIONS;

				for (k = 0; k < 4; ++k) {
					float v_s = s + (k % 2 ? fstep : 0);
					float v_t = t + (k >= 2 ? fstep : 0);
					vec3_t pos;

					MakeSkyVec2(v_s, v_t, axis, pos);

					// Degenerate?
					if (j == 0 && (i || axis) && vert && k == 0) {
						int prevIndex = (vert - 1) * FLOATS_PER_SKYVERT;

						vert = AddSkyDomeVert(vert, &skydomeVertData[prevIndex], skydomeVertData[prevIndex + 3], skydomeVertData[prevIndex + 4]);
						vert = AddSkyDomeVert(vert, pos, v_s, v_t);
						if (i == 0) {
							skyDome_starts[axis] = vert;
						}
					}

					vert = AddSkyDomeVert(vert, pos, v_s, v_t);
				}
			}
		}
		skyDome_length[axis] = vert - skyDome_starts[axis];
		Con_Printf("Axis %d: verts %d>%d (length %d)\n", axis, skyDome_starts[axis], vert, skyDome_length[axis]);
	}

	skyDomeVertices = vert;
	if (!skyDome_vbo) {
		glGenBuffers(1, &skyDome_vbo);
		glBindBufferExt(GL_ARRAY_BUFFER, skyDome_vbo);
		glBufferDataExt(GL_ARRAY_BUFFER, sizeof(float) * skyDomeVertices * FLOATS_PER_SKYVERT, skydomeVertData, GL_STATIC_DRAW);
	}

	if (!skyDome_vao) {
		glGenVertexArrays(1, &skyDome_vao);
		GL_BindVertexArray(skyDome_vao);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glBindBufferExt(GL_ARRAY_BUFFER, skyDome_vbo);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_SKYVERT, (void*)0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_SKYVERT, (void*)(3 * sizeof(float)));
	}
}

static void GLM_DrawFastSky(void)
{
	GLsizei count;
	GLushort indices[4096];
	msurface_t* fa;
	byte color[4] = {
		r_skycolor.color[0],
		r_skycolor.color[1],
		r_skycolor.color[2],
		255
	};

	glDisable(GL_CULL_FACE);
	count = 0;
	for (fa = skychain; fa; fa = fa->texturechain) {
		glpoly_t* poly;

		for (poly = fa->polys; poly; poly = poly->next) {
			int newVerts = poly->numverts;
			int v;

			if (count + 2 + newVerts > sizeof(indices) / sizeof(indices[0])) {
				GLM_DrawIndexedPolygonByType(GL_TRIANGLE_STRIP, color, cl.worldmodel->vao, indices, count, false, false, false);
				count = 0;
			}

			if (count) {
				int prev = count - 1;

				indices[count++] = indices[prev];
				indices[count++] = poly->vbo_start;
			}
			for (v = 0; v < newVerts; ++v) {
				indices[count++] = poly->vbo_start + v;
			}
		}
	}

	if (count) {
		GLM_DrawIndexedPolygonByType(GL_TRIANGLE_STRIP, color, cl.worldmodel->vao, indices, count, false, false, false);
		count = 0;
	}

	glEnable(GL_CULL_FACE);
}

void GLM_DrawSky(void)
{
	qbool		ignore_z;

	if (r_fastsky.value) {
		GLM_DrawFastSky();
		return;
	}

	if (!R_DetermineSkyLimits(&ignore_z)) {
		return;
	}

	// turn off Z tests & writes to avoid problems on large maps
	glDisable (GL_DEPTH_TEST);

	// draw a skybox or classic quake clouds
	/*if (r_skyboxloaded) {
	R_DrawSkyBox();
	}
	else*/ {
		GLM_DrawSkyDome();
	}

	glEnable (GL_DEPTH_TEST);

	// draw the sky polys into the Z buffer
	// don't need depth test yet
	if (!ignore_z) {
		if (gl_fogenable.value && gl_fogsky.value) {
			GL_EnableFog();
			glColor4f(gl_fogred.value, gl_foggreen.value, gl_fogblue.value, 1);
			GL_BlendFunc(GL_ONE, GL_ZERO);
		}
		else {
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			GL_BlendFunc(GL_ZERO, GL_ONE);
		}
		GL_AlphaBlendFlags(GL_BLEND_ENABLED);

		GLM_DrawFastSky();

		if (gl_fogenable.value && gl_fogsky.value) {
			GL_DisableFog();
		}
		else {
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		}
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	}
}

static void GLM_DrawSkyVerts(void)
{
	/*
	if (!glm_sky_vbo) {
	glGenBuffers(1, &glm_sky_vbo);
	}
	glBindBufferExt(GL_ARRAY_BUFFER, glm_sky_vbo);
	glBufferDataExt(GL_ARRAY_BUFFER, sizeof(float) * skyDome, glm_sky_verts, GL_STATIC_DRAW);

	if (!glm_sky_vao) {
	glGenVertexArrays(1, &glm_sky_vao);
	GL_BindVertexArray(glm_sky_vao);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_SKYVERT, (void*) 0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * FLOATS_PER_SKYVERT, (void*) (sizeof(float) * 3));
	}

	GL_EnterRegion(__FUNCTION__);
	GLM_DrawPolygonByType(GL_TRIANGLE_STRIP, color_white, skyDome, 0, skyVerts / FLOATS_PER_SKYVERT, false, true, false);
	GL_LeaveRegion();
	skyVerts = 0;
	*/
}
/*
static void QueueSkyVert(vec3_t v, float s, float t)
{
if (skyVerts + FLOATS_PER_SKYVERT < sizeof(glm_sky_verts) / sizeof(glm_sky_verts[0])) {
glm_sky_verts[skyVerts + 0] = v[0];
glm_sky_verts[skyVerts + 1] = v[1];
glm_sky_verts[skyVerts + 2] = v[2];
glm_sky_verts[skyVerts + 3] = s;
glm_sky_verts[skyVerts + 4] = t;
skyVerts += FLOATS_PER_SKYVERT;
}
}*/

static void GLM_DrawSkyFaces(void)
{
	if (skyDome.program) {
		float modelView[16];
		float projection[16];
		int i;
		GLint faceStarts[6 * 10];
		GLsizei faceLengths[6 * 10];
		int faces = 0;

		GL_GetMatrix(GL_MODELVIEW, modelView);
		GL_GetMatrix(GL_PROJECTION, projection);

		for (i = 0; i < 6; i++) {
			int minIndex[2];
			int maxIndex[2];
			int strip;

			if ((skymins[0][i] >= skymaxs[0][i] || skymins[1][i] >= skymaxs[1][i])) {
				continue;
			}

			if (skymins[0][i] == -1 && skymins[1][i] == -1 && skymaxs[0][i] == 1 && skymaxs[1][i] == 1) {
				faceStarts[faces] = skyDome_starts[i];
				faceLengths[faces] = skyDome_length[i];
			}
			else {
				minIndex[0] = (int)floor(bound(0, (skymins[0][i] + 1) / 2.0, 1) * SUBDIVISIONS);
				minIndex[1] = (int)floor(bound(0, (skymins[1][i] + 1) / 2.0, 1) * SUBDIVISIONS);
				maxIndex[0] = (int)ceil(bound(0, (skymaxs[0][i] + 1) / 2.0, 1) * SUBDIVISIONS);
				maxIndex[1] = (int)ceil(bound(0, (skymaxs[1][i] + 1) / 2.0, 1) * SUBDIVISIONS);

				for (strip = minIndex[0]; strip < maxIndex[0]; ++strip) {
					faceStarts[faces] = skyDome_starts[i] + 4 * (strip * SUBDIVISIONS + minIndex[1]) + 2 * strip;
					faceLengths[faces] = (maxIndex[1] - minIndex[1]) * 4;

					++faces;
				}
			}
		}

		if (faces) {
			GL_EnterRegion("SkyDome");
			GL_UseProgram(skyDome.program);
			glUniformMatrix4fv(skyDome_modelView, 1, GL_FALSE, modelView);
			glUniformMatrix4fv(skyDome_projection, 1, GL_FALSE, projection);
			glUniform1f(skyDome_farclip, max(r_farclip.value, 4096) * 0.577);
			glUniform1f(skyDome_speedscale, r_refdef2.time * 8 - ((int)speedscale & ~127));
			glUniform1f(skyDome_speedscale2, r_refdef2.time * 16 - ((int)speedscale & ~127));
			glUniform1i(skyDome_skyTex, 0);
			glUniform1i(skyDome_alphaTex, 1);
			glUniform3f(skyDome_origin, r_origin[0], r_origin[1], r_origin[2]);
			GL_BindVertexArray(skyDome_vao);
			glDisable(GL_CULL_FACE);
			glMultiDrawArrays(GL_TRIANGLE_STRIP, faceStarts, faceLengths, faces);
			glEnable(GL_CULL_FACE);
			GL_LeaveRegion();
		}
		return;
	}
}

static void GLM_DrawSkyDome(void)
{
	BuildSkyVertsArray();

	GL_SelectTexture(GL_TEXTURE0);
	GL_Bind(solidskytexture);
	GL_SelectTexture(GL_TEXTURE1);
	GL_Bind(alphaskytexture);

	GL_AlphaBlendFlags(GL_BLEND_DISABLED);

	GLM_DrawSkyFaces();
}

void GLM_DrawSkyFace(int axis)
{
	if (skyDome.program) {
		float modelView[16];
		float projection[16];

		GL_GetMatrix(GL_MODELVIEW, modelView);
		GL_GetMatrix(GL_PROJECTION, projection);

		GL_EnterRegion("SkyDome");
		GL_UseProgram(skyDome.program);
		glUniformMatrix4fv(skyDome_modelView, 1, GL_FALSE, modelView);
		glUniformMatrix4fv(skyDome_projection, 1, GL_FALSE, projection);
		glUniform1f(skyDome_farclip, max(r_farclip.value, 4096) * 0.577);
		glUniform1f(skyDome_speedscale, speedscale);
		glUniform1i(skyDome_skyTex, 0);
		glUniform3f(skyDome_origin, r_origin[0], r_origin[1], r_origin[2]);
		GL_BindVertexArray(skyDome_vao);
		glDisable(GL_CULL_FACE);
		glDrawArrays(GL_TRIANGLE_STRIP, skyDome_starts[axis], skyDome_length[axis]);
		glEnable(GL_CULL_FACE);
		GL_LeaveRegion();
	}
}
