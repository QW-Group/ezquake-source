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

typedef  struct {
	GLuint  count;
	GLuint  instanceCount;
	GLuint  firstIndex;
	GLuint  baseVertex;
	GLuint  baseInstance;
} DrawElementsIndirectCommand;

#define VBO_SKYDOME_FOFS(x) (void*)((intptr_t)&(((skydome_vert_t*)0)->x))

#define NUMBER_AXIS 6
#define SUBDIVISIONS 10
#define VERTS_PER_STRIP (SUBDIVISIONS + 1)
#define VERTS_PER_AXIS (VERTS_PER_STRIP * (SUBDIVISIONS + 1))
#define VERTS_PER_SKYDOME (VERTS_PER_AXIS * NUMBER_AXIS)
#define INDEXES_PER_STRIP ((SUBDIVISIONS + 1) * 2)
#define INDEXES_PER_AXIS (INDEXES_PER_STRIP * SUBDIVISIONS + (SUBDIVISIONS - 1))
static skydome_vert_t skydomeVertData[VERTS_PER_SKYDOME];
static GLushort skydomeIndexes[INDEXES_PER_AXIS];
static int skyDomeIndexCount = 0;
static GLuint skydome_RefdefCvars_block;
static GLuint skydome_SkyDomeData_block;
static SkydomeCvars_t skyDomeData;

static glm_program_t skyDome;
static glm_vbo_t skyDome_vbo;
static glm_vbo_t skyDomeIndexes_vbo;
static glm_vbo_t skyDomeCommands_vbo;
static glm_vao_t skyDome_vao;
static glm_ubo_t ubo_skydomeData;

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

		// Generate vertices (for each axis)
		for (axis = 0; axis < 6; ++axis) {
			float fstep = 2.0 / SUBDIVISIONS;
			vec3_t pos;

			for (i = 0; i <= SUBDIVISIONS; i++) {
				s = (float)(i * 2 - SUBDIVISIONS) / SUBDIVISIONS;

				for (j = 0; j <= SUBDIVISIONS; j++) {
					t = (float)(j * 2 - SUBDIVISIONS) / SUBDIVISIONS;

					MakeSkyVec2(s, t, axis, pos);

					AddSkyDomeVert(vert++, pos, s, t);
				}
			}
		}

		// Generate indexes (each axis is the same)
		for (i = 0; i < SUBDIVISIONS; ++i) {
			for (j = 0; j <= SUBDIVISIONS; ++j) {
				// primitive restart for 'whole axis' drawcall
				if (j == 0 && i) {
					skydomeIndexes[skyDomeIndexCount++] = ~(GLuint)0;
				}

				// Grab 'left' vert from previous row, sequential
				skydomeIndexes[skyDomeIndexCount++] = (i + 1) * VERTS_PER_STRIP + j;
				skydomeIndexes[skyDomeIndexCount++] = i * VERTS_PER_STRIP + j;
			}
		}
	}

	if (!skyDome_vao.vao) {
		GL_GenVertexArray(&skyDome_vao);
		GL_BindVertexArray(skyDome_vao.vao);
		GL_GenFixedBuffer(&skyDome_vbo, GL_ARRAY_BUFFER, __FUNCTION__, sizeof(skydome_vert_t) * vert, skydomeVertData, GL_STATIC_DRAW);
		GL_GenFixedBuffer(&skyDomeIndexes_vbo, GL_ELEMENT_ARRAY_BUFFER, __FUNCTION__, sizeof(skydomeIndexes[0]) * skyDomeIndexCount, skydomeIndexes, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(skydome_vert_t), VBO_SKYDOME_FOFS(pos));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(skydome_vert_t), VBO_SKYDOME_FOFS(s));
	}

	if (!skyDomeCommands_vbo.vbo) {
		GL_GenFixedBuffer(&skyDomeCommands_vbo, GL_DRAW_INDIRECT_BUFFER, __FUNCTION__, sizeof(DrawElementsIndirectCommand) * NUMBER_AXIS * SUBDIVISIONS, NULL, GL_STREAM_DRAW);
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
		int axis;
		DrawElementsIndirectCommand commandData[NUMBER_AXIS * SUBDIVISIONS];
		GLuint commands = 0;

		for (axis = 0; axis < 6; axis++) {
			int minIndex[2];
			int maxIndex[2];
			int strip;

			if ((skymins[0][axis] >= skymaxs[0][axis] || skymins[1][axis] >= skymaxs[1][axis])) {
				continue;
			}

			if (skymins[0][axis] == -1 && skymins[1][axis] == -1 && skymaxs[0][axis] == 1 && skymaxs[1][axis] == 1) {
				// Draw the whole axis
				DrawElementsIndirectCommand* cmd = &commandData[commands++];
				cmd->baseInstance = 0;
				cmd->baseVertex = axis * VERTS_PER_AXIS;
				cmd->count = INDEXES_PER_AXIS;
				cmd->firstIndex = 0;
				cmd->instanceCount = 1;
			}
			else {
				minIndex[0] = (int)floor(bound(0, (skymins[0][axis] + 1) / 2.0, 1) * SUBDIVISIONS);
				minIndex[1] = (int)floor(bound(0, (skymins[1][axis] + 1) / 2.0, 1) * SUBDIVISIONS);
				maxIndex[0] = (int)ceil(bound(0, (skymaxs[0][axis] + 1) / 2.0, 1) * SUBDIVISIONS);
				maxIndex[1] = (int)ceil(bound(0, (skymaxs[1][axis] + 1) / 2.0, 1) * SUBDIVISIONS);

				for (strip = minIndex[0]; strip < maxIndex[0]; ++strip) {
					DrawElementsIndirectCommand* cmd = &commandData[commands++];
					cmd->baseInstance = 0;
					cmd->baseVertex = axis * VERTS_PER_AXIS;
					cmd->count = (maxIndex[1] - minIndex[1] + 1) * 2;
					cmd->firstIndex = strip * (INDEXES_PER_STRIP + 1) + minIndex[1] * 2;
					cmd->instanceCount = 1;
				}
			}
		}

		if (commands) {
			skyDomeData.farclip = max(r_farclip.value, 4096) * 0.577;
			skyDomeData.speedscale = r_refdef2.time * 8 - ((int)speedscale & ~127);
			skyDomeData.speedscale2 = r_refdef2.time * 16 - ((int)speedscale & ~127);
			VectorCopy(r_origin, skyDomeData.origin);

			GL_EnterRegion("SkyDome");
			GL_UseProgram(skyDome.program);
			GL_BindVertexArray(skyDome_vao.vao);
			GL_BindBuffer(GL_DRAW_INDIRECT_BUFFER, skyDomeCommands_vbo.vbo);
			GL_BufferDataUpdate(GL_DRAW_INDIRECT_BUFFER, sizeof(commandData[0]) * commands, commandData);
			GL_BindBuffer(GL_UNIFORM_BUFFER, ubo_skydomeData.ubo);
			GL_BufferDataUpdate(GL_UNIFORM_BUFFER, sizeof(skyDomeData), &skyDomeData);

			glMultiDrawElementsIndirect(
				GL_TRIANGLE_STRIP,
				GL_UNSIGNED_SHORT,
				0,
				commands,
				0
			);

			GL_LeaveRegion();
		}
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
