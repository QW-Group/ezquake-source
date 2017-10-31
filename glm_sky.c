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
static void GLM_DrawSkyBox(void);

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

typedef struct SkyboxCvars_s {
	vec3_t origin;
	float farclip;
} SkyboxCvars_t;

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
static GLuint skybox_RefdefCvars_block;
static GLuint skybox_SkyBoxData_block;
static buffer_ref skyDome_vbo;
static buffer_ref skyDomeCommands_vbo;
static glm_vao_t skyDome_vao;
static glm_vao_t skyBox_vao;
static buffer_ref ubo_skydomeData;
static buffer_ref ubo_skyboxData;
static texture_ref skybox_cubeMap;

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

static qbool BuildSkyDomeProgram(void)
{
	static glm_program_t skyDome;

	if (!skyDome.program) {
		GL_VFDeclare(skydome);

		GLM_CreateVFProgram("SkyDome", GL_VFParams(skydome), &skyDome);
	}

	if (!GL_BufferReferenceIsValid(skyDomeCommands_vbo)) {
		skyDomeCommands_vbo = GL_GenFixedBuffer(GL_DRAW_INDIRECT_BUFFER, "skydome-commands", sizeof(DrawElementsIndirectCommand) * NUMBER_AXIS * SUBDIVISIONS, NULL, GL_STREAM_DRAW);
	}

	if (skyDome.program && !skyDome.uniforms_found) {
		GLint size;
		GLuint skydome_RefdefCvars_block;
		GLuint skydome_SkyDomeData_block;

		skydome_RefdefCvars_block = glGetUniformBlockIndex(skyDome.program, "RefdefCvars");
		skydome_SkyDomeData_block = glGetUniformBlockIndex(skyDome.program, "SkydomeData");

		glGetActiveUniformBlockiv(skyDome.program, skydome_SkyDomeData_block, GL_UNIFORM_BLOCK_DATA_SIZE, &size);

		glUniformBlockBinding(skyDome.program, skydome_RefdefCvars_block, GL_BINDINGPOINT_REFDEF_CVARS);
		glUniformBlockBinding(skyDome.program, skydome_SkyDomeData_block, GL_BINDINGPOINT_SKYDOME_CVARS);

		ubo_skydomeData = GL_GenUniformBuffer("skydome-data", NULL, sizeof(SkydomeCvars_t));
		GL_BindBufferBase(ubo_skydomeData, GL_BINDINGPOINT_SKYDOME_CVARS);

		skyDome.uniforms_found = true;
	}

	if (skyDome.program && skyDome.uniforms_found && GL_BufferReferenceIsValid(skyDomeCommands_vbo)) {
		GL_UseProgram(skyDome.program);
		return true;
	}

	return false;
}

static qbool BuildSkyBoxProgram(void)
{
	static glm_program_t skyBox;

	if (!skyBox.program) {
		GL_VFDeclare(skybox);

		GLM_CreateVFProgram("SkyBox", GL_VFParams(skybox), &skyBox);
	}

	if (skyBox.program && !skyBox.uniforms_found) {
		GLint size;

		skybox_RefdefCvars_block = glGetUniformBlockIndex(skyBox.program, "RefdefCvars");
		skybox_SkyBoxData_block = glGetUniformBlockIndex(skyBox.program, "SkyboxData");

		glGetActiveUniformBlockiv(skyBox.program, skybox_SkyBoxData_block, GL_UNIFORM_BLOCK_DATA_SIZE, &size);

		glUniformBlockBinding(skyBox.program, skybox_RefdefCvars_block, GL_BINDINGPOINT_REFDEF_CVARS);
		glUniformBlockBinding(skyBox.program, skybox_SkyBoxData_block, GL_BINDINGPOINT_SKYBOX_CVARS);

		ubo_skyboxData = GL_GenUniformBuffer("skybox-data", NULL, sizeof(SkyboxCvars_t));
		GL_BindBufferBase(ubo_skyboxData, GL_BINDINGPOINT_SKYBOX_CVARS);

		skyBox.uniforms_found = true;
	}

	if (skyBox.program && skyBox.uniforms_found) {
		GL_UseProgram(skyBox.program);
		return true;
	}

	return false;
}

static void BuildSkyVertsArray(void)
{
	static buffer_ref skyDomeIndexes_vbo;
	static GLushort skydomeIndexes[INDEXES_PER_AXIS];

	int skyDomeIndexCount = 0;
	int i, j;
	float s, t;
	int vert = 0;
	int axis = 0;

	// Skydome VBO
	if (!GL_BufferReferenceIsValid(skyDome_vbo)) {
		// Generate vertices (for each axis)
		for (axis = 0; axis < 6; ++axis) {
			float fstep = 2.0 / SUBDIVISIONS;
			vec3_t pos;

			for (i = 0; i <= SUBDIVISIONS; i++) {
				s = (float)(i * 2 - SUBDIVISIONS) / SUBDIVISIONS;

				for (j = 0; j <= SUBDIVISIONS; j++) {
					t = (float)(j * 2 - SUBDIVISIONS) / SUBDIVISIONS;

					Sky_MakeSkyVec2(s, t, axis, pos);

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

		skyDome_vbo = GL_GenFixedBuffer(GL_ARRAY_BUFFER, "skydome-vbo", sizeof(skydome_vert_t) * vert, skydomeVertData, GL_STATIC_DRAW);
		skyDomeIndexes_vbo = GL_GenFixedBuffer(GL_ELEMENT_ARRAY_BUFFER, "skydome-indexes", sizeof(skydomeIndexes[0]) * skyDomeIndexCount, skydomeIndexes, GL_STATIC_DRAW);
	}

	if (!skyDome_vao.vao) {
		GL_GenVertexArray(&skyDome_vao);
		GL_SetVertexArrayElementBuffer(&skyDome_vao, skyDomeIndexes_vbo);

		GL_ConfigureVertexAttribPointer(&skyDome_vao, skyDome_vbo, 0, 3, GL_FLOAT, GL_FALSE, sizeof(skydome_vert_t), VBO_SKYDOME_FOFS(pos));
		GL_ConfigureVertexAttribPointer(&skyDome_vao, skyDome_vbo, 1, 2, GL_FLOAT, GL_FALSE, sizeof(skydome_vert_t), VBO_SKYDOME_FOFS(s));
	}

	if (!skyBox_vao.vao) {
		GL_GenVertexArray(&skyBox_vao);
		GL_SetVertexArrayElementBuffer(&skyBox_vao, null_buffer_reference);

		GL_ConfigureVertexAttribPointer(&skyBox_vao, skyDome_vbo, 0, 3, GL_FLOAT, GL_FALSE, sizeof(skydome_vert_t), VBO_SKYDOME_FOFS(pos));
		GL_ConfigureVertexAttribPointer(&skyBox_vao, skyDome_vbo, 1, 2, GL_FLOAT, GL_FALSE, sizeof(skydome_vert_t), VBO_SKYDOME_FOFS(s));
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
	BuildSkyVertsArray();
	if (r_skyboxloaded) {
		GLM_DrawSkyBox();
	}
	else {
		GLM_DrawSkyDome();
	}

	glEnable (GL_DEPTH_TEST);
}

static void GLM_DrawSkyDomeFaces(void)
{
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
		SkydomeCvars_t skyDomeData;
		skyDomeData.farclip = max(r_farclip.value, 4096) * 0.577;
		skyDomeData.speedscale = r_refdef2.time * 8;
		skyDomeData.speedscale -= ((int)skyDomeData.speedscale & ~127);
		skyDomeData.speedscale2 = r_refdef2.time * 16;
		skyDomeData.speedscale2 -= ((int)skyDomeData.speedscale2 & ~127);
		VectorCopy(r_origin, skyDomeData.origin);

		GL_UpdateVBO(skyDomeCommands_vbo, sizeof(commandData[0]) * commands, commandData);
		GL_UpdateVBO(ubo_skydomeData, sizeof(skyDomeData), &skyDomeData);

		GL_BindVertexArray(&skyDome_vao);

		glMultiDrawElementsIndirect(
			GL_TRIANGLE_STRIP,
			GL_UNSIGNED_SHORT,
			0,
			commands,
			0
		);

		++frameStats.draw_calls;
	}
}

static void GLM_DrawSkyDome(void)
{
	GL_EnterRegion("SkyDome");
	if (BuildSkyDomeProgram()) {
		GL_BindTextureUnit(GL_TEXTURE0, solidskytexture);
		GL_BindTextureUnit(GL_TEXTURE1, alphaskytexture);

		GL_AlphaBlendFlags(GL_BLEND_DISABLED);

		GLM_DrawSkyDomeFaces();
	}
}

static void GLM_DrawSkyBox(void)
{
	if (GL_TextureReferenceIsValid(skybox_cubeMap) && BuildSkyBoxProgram()) {
		GLsizei count[NUMBER_AXIS];
		GLushort indices[NUMBER_AXIS * 5];
		int number_to_draw = 0;

		GL_BindTextureUnit(GL_TEXTURE0, skybox_cubeMap);

		GL_AlphaBlendFlags(GL_BLEND_DISABLED);

		// GLM_DrawSkyBox
		int axis;
		for (axis = 0; axis < NUMBER_AXIS; axis++) {
			int minIndex[2];
			int maxIndex[2];
			int base = axis * VERTS_PER_AXIS;

			if ((skymins[0][axis] >= skymaxs[0][axis] || skymins[1][axis] >= skymaxs[1][axis])) {
				continue;
			}

			minIndex[0] = (int)floor(bound(0, (skymins[0][axis] + 1) / 2.0, 1) * SUBDIVISIONS);
			minIndex[1] = (int)floor(bound(0, (skymins[1][axis] + 1) / 2.0, 1) * SUBDIVISIONS);
			maxIndex[0] = (int)ceil(bound(0, (skymaxs[0][axis] + 1) / 2.0, 1) * SUBDIVISIONS);
			maxIndex[1] = (int)ceil(bound(0, (skymaxs[1][axis] + 1) / 2.0, 1) * SUBDIVISIONS);

			// Draw the sub-section
			indices[number_to_draw * 5 + 0] = base + maxIndex[0] * VERTS_PER_STRIP + minIndex[1];
			indices[number_to_draw * 5 + 1] = base + minIndex[0] * VERTS_PER_STRIP + minIndex[1];
			indices[number_to_draw * 5 + 2] = base + maxIndex[0] * VERTS_PER_STRIP + maxIndex[1];
			indices[number_to_draw * 5 + 3] = base + minIndex[0] * VERTS_PER_STRIP + maxIndex[1];
			indices[number_to_draw * 5 + 4] = ~(GLushort)0;
			count[number_to_draw] = 5;
			++number_to_draw;
		}

		// Update uniforms
		SkyboxCvars_t cvars;
		cvars.farclip = max(r_farclip.value, 4096) * 0.577;
		VectorCopy(r_origin, cvars.origin);

		GL_UpdateVBO(ubo_skyboxData, sizeof(cvars), &cvars);

		GL_BindVertexArray(&skyBox_vao);
		glDrawElements(GL_TRIANGLE_STRIP, number_to_draw * 5, GL_UNSIGNED_SHORT, indices);
		++frameStats.draw_calls;
	}
}

static void GLM_CopySkyboxTexturesToCubeMap(texture_ref cubemap, int width, int height)
{
	static int skytexorder[MAX_SKYBOXTEXTURES] = {0,2,1,3,4,5};
	int i;
	GLbyte* data;

	// Copy data from 2d images into cube-map
	data = Q_malloc(4 * width * height);
	for (i = 0; i < MAX_SKYBOXTEXTURES; ++i) {
		GL_GetTexImage(GL_TEXTURE0, skyboxtextures[skytexorder[i]], 0, GL_RGBA, GL_UNSIGNED_BYTE, 4 * width * height, data);

		GL_TexSubImage3D(GL_TEXTURE0, cubemap, 0, 0, 0, i, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
	}
	Q_free(data);

	GL_GenerateMipmap(GL_TEXTURE0, cubemap);
}

qbool GLM_LoadSkyboxTextures(char* skyname)
{
	int widths[MAX_SKYBOXTEXTURES], heights[MAX_SKYBOXTEXTURES];
	qbool all_same = true;
	int i;

	// FIXME: Delete previous?
	GL_TextureReferenceInvalidate(skybox_cubeMap);

	if (!Sky_LoadSkyboxTextures(skyname)) {
		return false;
	}

	// Get the actual sizes (may have been rescaled)
	for (i = 0; i < MAX_SKYBOXTEXTURES; ++i) {
		widths[i] = GL_TextureWidth(skyboxtextures[i]);
		heights[i] = GL_TextureHeight(skyboxtextures[i]);
	}

	// Check if they're all the same
	for (i = 0; i < MAX_SKYBOXTEXTURES - 1 && all_same; ++i) {
		all_same &= widths[i] == widths[i + 1];
		all_same &= heights[i] == heights[i + 1];
	}

	if (!all_same) {
		Con_Printf("Skybox found, but textures differ in dimensions\n");
		return false;
	}

	skybox_cubeMap = GL_CreateCubeMap("skybox:cubemap", widths[0], heights[0], TEX_NOCOMPRESS | TEX_MIPMAP);

	GLM_CopySkyboxTexturesToCubeMap(skybox_cubeMap, widths[0], heights[0]);

	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	return true;
}
