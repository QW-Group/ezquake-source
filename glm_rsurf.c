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
// glm_surf.c: surface-related refresh code (modern OpenGL)

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"
#include "utils.h"

#define TEXTURE_UNIT_MATERIAL 0
#define TEXTURE_UNIT_LIGHTMAPS 1
#define TEXTURE_UNIT_DETAIL 2
#define TEXTURE_UNIT_CAUSTICS 3

static GLuint modelIndexes[16 * 1024];
static GLuint index_count;
static qbool uniforms_set = false;
extern GLuint lightmap_texture_array;

static glm_program_t drawworld;
static GLint drawworld_RefdefCvars_block;
static GLint drawworld_WorldCvars_block;

typedef struct block_world_s {
	//
	float waterAlpha;

	// drawflat for solid surfaces
	int r_drawflat;
	int r_fastturb;
	int r_fastsky;

	// ! No padding required as above is 16 bytes...

	float r_wallcolor[4];  // only used if r_drawflat 1 or 3
	float r_floorcolor[4]; // only used if r_drawflat 1 or 2

	// drawflat for turb surfaces
	float r_telecolor[4];
	float r_lavacolor[4];
	float r_slimecolor[4];
	float r_watercolor[4];

	// drawflat for sky
	float r_skycolor[4];
} block_world_t;

#define DRAW_DETAIL_TEXTURES 1
#define DRAW_CAUSTIC_TEXTURES 2
static int drawworld_compiledOptions;
static glm_ubo_t ubo_worldcvars;
static block_world_t world;

// We re-compile whenever certain options change, to save texture bindings/lookups
static void Compile_DrawWorldProgram(qbool detail_textures, qbool caustic_textures)
{
	int drawworld_desiredOptions =
		(detail_textures ? DRAW_DETAIL_TEXTURES : 0) |
		(caustic_textures ? DRAW_CAUSTIC_TEXTURES : 0);

	if (!drawworld.program || drawworld_compiledOptions != drawworld_desiredOptions) {
		const char* included_definitions =
			detail_textures && caustic_textures ? "#define DRAW_DETAIL_TEXTURES\n#define DRAW_CAUSTIC_TEXTURES\n" :
			detail_textures ? "#define DRAW_DETAIL_TEXTURES\n" :
			caustic_textures ? "#define DRAW_CAUSTIC_TEXTURES\n" :
			"";

		GL_VFDeclare(drawworld);

		// Initialise program for drawing image
		GLM_CreateVFProgramWithInclude("DrawWorld", GL_VFParams(drawworld), &drawworld, included_definitions);

		drawworld_compiledOptions = drawworld_desiredOptions;
	}

	if (drawworld.program && !drawworld.uniforms_found) {
		GLint size;

		drawworld_RefdefCvars_block = glGetUniformBlockIndex(drawworld.program, "RefdefCvars");
		glUniformBlockBinding(drawworld.program, drawworld_RefdefCvars_block, GL_BINDINGPOINT_REFDEF_CVARS);

		drawworld_WorldCvars_block = glGetUniformBlockIndex(drawworld.program, "WorldCvars");
		glGetActiveUniformBlockiv(drawworld.program, drawworld_WorldCvars_block, GL_UNIFORM_BLOCK_DATA_SIZE, &size);

		glUniformBlockBinding(drawworld.program, drawworld_WorldCvars_block, GL_BINDINGPOINT_DRAWWORLD_CVARS);
		GL_GenUniformBuffer(&ubo_worldcvars, "world-cvars", &world, sizeof(world));
		glBindBufferBase(GL_UNIFORM_BUFFER, GL_BINDINGPOINT_DRAWWORLD_CVARS, ubo_worldcvars.ubo);

		drawworld.uniforms_found = true;
	}
}

#define PASS_COLOR_AS_4F(target, cvar) \
{ \
	target[0] = (cvar.color[0] * 1.0f / 255); \
	target[1] = (cvar.color[1] * 1.0f / 255); \
	target[2] = (cvar.color[2] * 1.0f / 255); \
	target[3] = 1.0f; \
}

static void GLM_EnterBatchedWorldRegion(qbool detail_tex, qbool caustics)
{
	extern glm_vao_t brushModel_vao;
	float wateralpha = bound((1 - r_refdef2.max_watervis), r_wateralpha.value, 1);
	extern cvar_t r_telecolor, r_lavacolor, r_slimecolor, r_watercolor, r_fastturb, r_skycolor;

	Compile_DrawWorldProgram(detail_tex, caustics);

	world.waterAlpha = wateralpha;

	world.r_drawflat = r_drawflat.integer;
	PASS_COLOR_AS_4F(world.r_wallcolor, r_wallcolor);
	PASS_COLOR_AS_4F(world.r_floorcolor, r_floorcolor);

	world.r_fastturb = r_fastturb.integer;
	PASS_COLOR_AS_4F(world.r_telecolor, r_telecolor);
	PASS_COLOR_AS_4F(world.r_lavacolor, r_lavacolor);
	PASS_COLOR_AS_4F(world.r_slimecolor, r_slimecolor);
	PASS_COLOR_AS_4F(world.r_watercolor, r_watercolor);

	world.r_fastsky = r_fastsky.integer;
	PASS_COLOR_AS_4F(world.r_skycolor, r_skycolor);

	GL_BindBuffer(GL_UNIFORM_BUFFER, ubo_worldcvars.ubo);
	GL_BufferDataUpdate(GL_UNIFORM_BUFFER, sizeof(world), &world);

	GL_UseProgram(drawworld.program);

	GL_BindVertexArray(brushModel_vao.vao);

	// Bind lightmap array
	GL_BindTextureUnit(GL_TEXTURE0 + TEXTURE_UNIT_LIGHTMAPS, GL_TEXTURE_2D_ARRAY, lightmap_texture_array);
	if (detail_tex) {
		GL_BindTextureUnit(GL_TEXTURE0 + TEXTURE_UNIT_DETAIL, GL_TEXTURE_2D, detailtexture);
	}
	if (caustics) {
		GL_BindTextureUnit(GL_TEXTURE0 + TEXTURE_UNIT_CAUSTICS, GL_TEXTURE_2D, underwatertexture);
	}
	GL_SelectTexture(GL_TEXTURE0 + TEXTURE_UNIT_MATERIAL);
}

void GLM_ExitBatchedPolyRegion(void)
{
	uniforms_set = false;
}

typedef struct glm_brushmodel_req_s {
	// This is DrawElementsIndirectCmd, from OpenGL spec
	GLuint count;           // Number of indexes to pull
	GLuint instanceCount;   // Always 1... ?
	GLuint firstIndex;      // Position of first index in array
	GLuint baseVertex;      // Offset of vertices in VBO
	GLuint baseInstance;    // We use this to pull from array of uniforms in shader

	GLuint texture_array;
} glm_worldmodel_req_t;

#define MAX_WORLDMODEL_BATCH 32
static glm_worldmodel_req_t worldmodel_requests[MAX_WORLDMODEL_BATCH];
static GLuint batch_count = 0;
static glm_vbo_t vbo_elements;
static glm_vbo_t vbo_indirectDraw;

static void GL_FlushWorldModelBatch(void);

static glm_worldmodel_req_t* GLM_NextBatchRequest(model_t* model, float* base_color, GLuint texture_array)
{
	glm_worldmodel_req_t* req;

	if (batch_count >= MAX_WORLDMODEL_BATCH) {
		GL_FlushWorldModelBatch();
	}

	req = &worldmodel_requests[batch_count];

	req->count = 0;
	req->texture_array = texture_array;
	req->instanceCount = 1;
	req->firstIndex = index_count;
	req->baseVertex = 0;
	req->baseInstance = batch_count;

	++batch_count;
	return req;
}

void GLM_DrawTexturedWorld(model_t* model)
{
	int i, waterline, v;
	msurface_t* surf;
	qbool draw_detail_texture = gl_detail.integer && detailtexture;
	qbool draw_caustics = gl_caustics.integer && underwatertexture;
	glm_worldmodel_req_t* req = NULL;

	GLM_EnterBatchedWorldRegion(draw_detail_texture, draw_caustics);

	for (waterline = 0; waterline < 2; waterline++) {
		for (surf = model->drawflat_chain[waterline]; surf; surf = surf->drawflatchain) {
			glpoly_t* poly;

			req = GLM_NextBatchRequest(model, NULL, 0);
			for (poly = surf->polys; poly; poly = poly->next) {
				int newVerts = poly->numverts;

				if (index_count + 1 + newVerts > sizeof(modelIndexes) / sizeof(modelIndexes[0])) {
					GL_FlushWorldModelBatch();
					req = GLM_NextBatchRequest(model, NULL, 0);
				}

				if (index_count) {
					modelIndexes[index_count++] = ~(GLuint)0;
					req->count++;
				}

				for (v = 0; v < newVerts; ++v) {
					modelIndexes[index_count++] = poly->vbo_start + v;
					req->count++;
				}
			}
		}
	}

	for (i = 0; i < model->texture_array_count; ++i) {
		texture_t* base_tex = model->textures[model->texture_array_first[i]];
		qbool first_in_this_array = true;
		int texIndex;

		if (!base_tex || !base_tex->size_start || !base_tex->gl_texture_array) {
			continue;
		}

		for (texIndex = model->texture_array_first[i]; texIndex >= 0 && texIndex < model->numtextures; texIndex = model->textures[texIndex]->next_same_size) {
			texture_t* tex = model->textures[texIndex];

			if (!tex->texturechain[0] && !tex->texturechain[1]) {
				continue;
			}

			req = GLM_NextBatchRequest(model, NULL, tex->gl_texture_array);
			for (waterline = 0; waterline < 2; waterline++) {
				for (surf = tex->texturechain[waterline]; surf; surf = surf->texturechain) {
					glpoly_t* poly;

					for (poly = surf->polys; poly; poly = poly->next) {
						int newVerts = poly->numverts;

						if (index_count + 1 + newVerts > sizeof(modelIndexes) / sizeof(modelIndexes[0])) {
							GL_FlushWorldModelBatch();
							req = GLM_NextBatchRequest(model, NULL, tex->gl_texture_array);
						}

						if (req->count) {
							modelIndexes[index_count++] = ~(GLuint)0;
							req->count++;
						}

						for (v = 0; v < newVerts; ++v) {
							modelIndexes[index_count++] = poly->vbo_start + v;
							req->count++;
						}
					}
				}
			}
		}
	}

	if (index_count) {
		GL_FlushWorldModelBatch();
	}

	GLM_ExitBatchedPolyRegion();
	return;
}

static void GL_FlushWorldModelBatch(void)
{
	int i;
	GLuint last_vao = 0;
	GLuint last_array = 0;
	qbool was_worldmodel = 0;
	int draw_pos = 0;

	if (!batch_count) {
		return;
	}

	GL_BufferDataUpdate(GL_ELEMENT_ARRAY_BUFFER, sizeof(modelIndexes[0]) * index_count, modelIndexes);
	GL_BufferDataUpdate(GL_DRAW_INDIRECT_BUFFER, sizeof(worldmodel_requests), &worldmodel_requests);

	draw_pos = 0;
	for (i = 0; i < batch_count; ++i) {
		int last = i;
		GLuint texArray = worldmodel_requests[i].texture_array;

		for (last = i; last < batch_count - 1; ++last) {
			int next = worldmodel_requests[last + 1].texture_array;
			if (next != 0 && texArray != 0 && next != texArray) {
				break;
			}
			texArray = next;
		}

		if (texArray) {
			GL_BindTexture(GL_TEXTURE_2D_ARRAY, texArray, true);
		}

		// FIXME: All brush models are in the same VAO, sort this out
		glMultiDrawElementsIndirect(
			GL_TRIANGLE_STRIP,
			GL_UNSIGNED_INT,
			(void*)(i * sizeof(worldmodel_requests[0])),
			last - i + 1,
			sizeof(worldmodel_requests[0])
		);

		i = last;
	}

	batch_count = 0;
	index_count = 0;
}

void GLM_DrawWorld(model_t* model)
{
	const qbool use_texture_array = true;

	if (model->texture_array_count) {
		GLM_DrawTexturedWorld(model);
	}
}

void GLM_NewMap(void)
{
}

// FIXME: This a) doesn't work in modern, b) shouldn't be in this module
//draws transparent textures for HL world and nonworld models
void R_DrawAlphaChain(msurface_t* alphachain)
{
	int k;
	msurface_t *s;
	texture_t *t;
	float *v;

	if (!alphachain)
		return;

	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED);
	for (s = alphachain; s; s = s->texturechain) {
		t = s->texinfo->texture;
		R_RenderDynamicLightmaps(s);

		//bind the world texture
		GL_DisableMultitexture();
		GL_Bind(t->gl_texturenum);

		if (gl_mtexable) {
			GLC_MultitextureLightmap(s->lightmaptexturenum);
		}

		glBegin(GL_POLYGON);
		v = s->polys->verts[0];
		for (k = 0; k < s->polys->numverts; k++, v += VERTEXSIZE) {
			if (gl_mtexable) {
				qglMultiTexCoord2f(GL_TEXTURE0, v[3], v[4]);
				qglMultiTexCoord2f(GL_TEXTURE1, v[5], v[6]);
			}
			else {
				glTexCoord2f(v[3], v[4]);
			}
			glVertex3fv(v);
		}
		glEnd();
	}

	alphachain = NULL;

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED);
	GL_DisableMultitexture();
	GL_TextureEnvMode(GL_REPLACE);
}



