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

#define NUMBER_AXIS 6
#define SUBDIVISIONS 10
#define VERTS_PER_STRIP ((SUBDIVISIONS + 1) * 2)
#define VERTS_PER_AXIS (VERTS_PER_STRIP * SUBDIVISIONS)
#define VERTS_PER_SKYDOME (VERTS_PER_AXIS * NUMBER_AXIS)
#define INDEXES_PER_STRIP (VERTS_PER_STRIP)
#define INDEXES_PER_AXIS (INDEXES_PER_STRIP * SUBDIVISIONS + (SUBDIVISIONS - 1))
#define INDEXES_PER_SKYDOME (INDEXES_PER_AXIS * NUMBER_AXIS + (NUMBER_AXIS - 1))
//#define MAX_SKYPOLYS (SUBDIVISIONS * SUBDIVISIONS * 6)
static skydome_vert_t skydomeVertData[VERTS_PER_SKYDOME];
static GLuint skydomeIndexes[INDEXES_PER_SKYDOME];
static int skyDomeVertices = 0;
static int skyDomeIndexCount = 0;
static GLuint skydome_RefdefCvars_block;
static GLuint skydome_SkyDomeData_block;
static SkydomeCvars_t skyDomeData;

static glm_program_t skyDome;
static glm_vbo_t skyDome_vbo;
static glm_vbo_t skyDomeIndexes_vbo;
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

static void AddSkyDomeVert(int vert, vec3_t pos, float s, float t)
{
	VectorCopy(pos, skydomeVertData[vert].pos);
	skydomeVertData[vert].s = s;
	skydomeVertData[vert].t = t;
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
		skyDomeIndexCount = 0;
		for (axis = 0; axis < 6; ++axis) {
			float fstep = 2.0 / SUBDIVISIONS;

			skyDome_starts[axis] = vert; //skyDomeIndexCount;
			for (i = 0; i < SUBDIVISIONS; i++) {
				s = (float)(i * 2 - SUBDIVISIONS) / SUBDIVISIONS;

				for (j = 0; j < SUBDIVISIONS; j++) {
					t = (float)(j * 2 - SUBDIVISIONS) / SUBDIVISIONS;

					for (k = j ? 2 : 0; k < 4; ++k) {
						// We want to wind the other direction to save CULL_FACE toggles
						int winding_k = k % 2 == 0 ? k + 1 : k - 1;
						float v_s = s + (winding_k % 2 ? fstep : 0);
						float v_t = t + (winding_k >= 2 ? fstep : 0);
						vec3_t pos;

						MakeSkyVec2(v_s, v_t, axis, pos);

						// Degenerate?
						if (j == 0 && (i || axis) && vert && k == 0) {
							skydomeIndexes[skyDomeIndexCount++] = ~(GLuint)0;
							if (i == 0) {
								skyDome_starts[axis] = vert; //skyDomeIndexCount;
							}
						}

						skydomeIndexes[skyDomeIndexCount++] = vert;
						AddSkyDomeVert(vert++, pos, v_s, v_t);
					}
				}
			}
			//skyDome_length[axis] = skyDomeIndexCount - skyDome_starts[axis];
			skyDome_length[axis] = vert - skyDome_starts[axis];
			//Con_Printf("Skydome: axis %d is %d, length %d\n", axis, skyDome_starts[axis], skyDome_length[axis]);
		}
		skyDomeVertices = vert;

		//Con_Printf("Skydome: %d/%d verts, %d/%d indexes\n", vert, sizeof(skydomeVertData) / sizeof(skydomeVertData[0]), skyDomeIndexCount, sizeof(skydomeIndexes) / sizeof(skydomeIndexes[0]));

		GL_GenFixedBuffer(&skyDome_vbo, GL_ARRAY_BUFFER, __FUNCTION__, sizeof(skydome_vert_t) * skyDomeVertices, skydomeVertData, GL_STATIC_DRAW);
		GL_GenFixedBuffer(&skyDomeIndexes_vbo, GL_ELEMENT_ARRAY_BUFFER, __FUNCTION__, sizeof(skydomeIndexes[0]) * skyDomeIndexCount, skydomeIndexes, GL_STATIC_DRAW);
	}

	if (!skyDome_vao.vao) {
		GL_GenVertexArray(&skyDome_vao);
		GL_BindVertexArray(skyDome_vao.vao);
		GL_BindBuffer(GL_ARRAY_BUFFER, skyDome_vbo.vbo);
		GL_BindBuffer(GL_ELEMENT_ARRAY_BUFFER, skyDomeIndexes_vbo.vbo);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(skydome_vert_t), VBO_SKYDOME_FOFS(pos));
		glEnableVertexAttribArray(1);
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
				for (strip = 0; strip < SUBDIVISIONS; ++strip) {
					faceStarts[faces] = skyDome_starts[i] + strip * VERTS_PER_STRIP;
					faceLengths[faces] = VERTS_PER_STRIP;

					++faces;
				}
			}
			else {
				minIndex[0] = (int)floor(bound(0, (skymins[0][i] + 1) / 2.0, 1) * SUBDIVISIONS);
				minIndex[1] = (int)floor(bound(0, (skymins[1][i] + 1) / 2.0, 1) * SUBDIVISIONS);
				maxIndex[0] = (int)ceil(bound(0, (skymaxs[0][i] + 1) / 2.0, 1) * SUBDIVISIONS);
				maxIndex[1] = (int)ceil(bound(0, (skymaxs[1][i] + 1) / 2.0, 1) * SUBDIVISIONS);

				for (strip = minIndex[0]; strip < maxIndex[0]; ++strip) {
					faceStarts[faces] = skyDome_starts[i] + strip * VERTS_PER_STRIP + minIndex[1] * 2;
					faceLengths[faces] = (maxIndex[1] - minIndex[1] + 1) * 2;

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

			glMultiDrawArrays(GL_TRIANGLE_STRIP, faceStarts, faceLengths, faces);

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
