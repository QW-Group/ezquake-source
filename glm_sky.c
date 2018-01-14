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

typedef struct skydome_vert_s {
	vec3_t pos;
	float s, t;
} skydome_vert_t;

typedef struct SkydomeCvars_s {
	float farclip;
	float speedscale;
	float speedscale2;
	float padding;

	vec3_t origin;

	float padding2;
} SkydomeCvars_t;

#define VBO_SKYDOME_FOFS(x) (void*)((intptr_t)&(((skydome_vert_t*)0)->x))

#define SUBDIVISIONS	10
#define MAX_SKYPOLYS (SUBDIVISIONS * SUBDIVISIONS * 6)
static skydome_vert_t skydomeVertData[(MAX_SKYPOLYS * 4 + (MAX_SKYPOLYS - 1) * 2)];
static int skyDomeVertices = 0;
static GLuint skydome_RefdefCvars_block;
static GLuint skydome_SkyDomeData_block;
static SkydomeCvars_t skyDomeData;

static glm_program_t skyDome;
static glm_vbo_t skyDome_vbo;
static glm_vao_t skyDome_vao;
static glm_ubo_t ubo_skydomeData;
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
	int index = vert;

	VectorCopy(pos, skydomeVertData[index].pos);
	skydomeVertData[index+3].s = s;
	skydomeVertData[index+4].t = t;

	return vert + 1;
}

static void BuildSkyVertsArray(void)
{
	int i, j;
	float s, t;
	int vert = 0;
	int axis = 0;
	int k;

	if (!skyDome.program) {
		GL_VFDeclare(skydome);

		GLM_CreateVFProgram("SkyDome", GL_VFParams(skydome), &skyDome);
	}

	if (skyDome.program && !skyDome.uniforms_found) {
		GLint size;

		skydome_RefdefCvars_block = glGetUniformBlockIndex(skyDome.program, "RefdefCvars");
		skydome_SkyDomeData_block = glGetUniformBlockIndex(skyDome.program, "SkydomeData");

		glGetActiveUniformBlockiv(skyDome.program, skydome_SkyDomeData_block, GL_UNIFORM_BLOCK_DATA_SIZE, &size);

		glUniformBlockBinding(skyDome.program, skydome_RefdefCvars_block, GL_BINDINGPOINT_REFDEF_CVARS);
		glUniformBlockBinding(skyDome.program, skydome_SkyDomeData_block, GL_BINDINGPOINT_SKYDOME_CVARS);

		GL_GenUniformBuffer(&ubo_skydomeData, "skydome-data", &skyDomeData, sizeof(skyDomeData));
		glBindBufferBase(GL_UNIFORM_BUFFER, GL_BINDINGPOINT_SKYDOME_CVARS, ubo_skydomeData.ubo);

		skyDome.uniforms_found = true;
	}

	if (!skyDome_vbo.vbo) {
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
							int prevIndex = (vert - 1);

							vert = AddSkyDomeVert(vert, skydomeVertData[prevIndex].pos, skydomeVertData[prevIndex].s, skydomeVertData[prevIndex].t);
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
		}
		skyDomeVertices = vert;

		GL_GenFixedBuffer(&skyDome_vbo, GL_ARRAY_BUFFER, __FUNCTION__, sizeof(skydome_vert_t) * skyDomeVertices, GL_STATIC_DRAW);
		GL_BufferDataUpdate(GL_ARRAY_BUFFER, sizeof(skydome_vert_t) * skyDomeVertices, skydomeVertData);
	}

	if (!skyDome_vao.vao) {
		GL_GenVertexArray(&skyDome_vao);
		GL_BindVertexArray(skyDome_vao.vao);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		GL_BindBuffer(GL_ARRAY_BUFFER, skyDome_vbo.vbo);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(skydome_vert_t), VBO_SKYDOME_FOFS(pos));
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(skydome_vert_t), VBO_SKYDOME_FOFS(s));
	}
}

void GLM_DrawSky(void)
{
	qbool		ignore_z;

	if (r_fastsky.value) {
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
}

static void GLM_DrawSkyFaces(void)
{
	if (skyDome.program) {
		int i;
		GLint faceStarts[6 * 10];
		GLsizei faceLengths[6 * 10];
		int faces = 0;

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
				++faces;
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
			skyDomeData.farclip = max(r_farclip.value, 4096) * 0.577;
			skyDomeData.speedscale = r_refdef2.time * 8 - ((int)speedscale & ~127);
			skyDomeData.speedscale2 = r_refdef2.time * 16 - ((int)speedscale & ~127);
			VectorCopy(r_origin, skyDomeData.origin);

			GL_EnterRegion("SkyDome");
			GL_UseProgram(skyDome.program);
			GL_BindVertexArray(skyDome_vao.vao);
			GL_BindBuffer(GL_UNIFORM_BUFFER, ubo_skydomeData.ubo);
			GL_BufferDataUpdate(GL_UNIFORM_BUFFER, sizeof(skyDomeData), &skyDomeData);

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
