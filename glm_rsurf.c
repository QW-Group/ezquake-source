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
#include "tr_types.h"
#include "glsl/constants.glsl"

static void GL_SortDrawCalls(int* split);

static int sampler_mappings;

extern buffer_ref brushModel_vbo;
extern GLuint* modelIndexes;
extern GLuint modelIndexMaximum;

static glm_worldmodel_req_t worldmodel_requests[MAX_WORLDMODEL_BATCH];
static GLuint index_count;
static qbool uniforms_set = false;
static glm_program_t drawworld;
static GLint drawWorld_outlines;
static GLuint drawworld_RefdefCvars_block;
static GLuint drawworld_WorldCvars_block;

static int batch_count = 0;
static int matrix_count = 0;
static buffer_ref vbo_worldIndirectDraw;

#define DRAW_DETAIL_TEXTURES       1
#define DRAW_CAUSTIC_TEXTURES      2
#define DRAW_LUMA_TEXTURES         4
#define DRAW_SKYBOX                8
#define DRAW_HARDWARE_LIGHTING    16
#define DRAW_SKYDOME              32
#define DRAW_FLATFLOORS           64
#define DRAW_FLATWALLS           128
#define DRAW_LIGHTMAPS           256
#define DRAW_LUMA_TEXTURES_FB    512
#define DRAW_TEXTURELESS        1024
static buffer_ref ubo_worldcvars;
static uniform_block_world_t world;

#define MAXIMUM_MATERIAL_SAMPLERS 32  // Can be fewer in practise, based on drive limit
#define MAX_STANDARD_TEXTURES 5 // Update this if adding more.  currently lm+detail+caustics+2*skydome=5
static int material_samplers_max;
static int material_samplers;
static texture_ref allocated_samplers[MAXIMUM_MATERIAL_SAMPLERS];
static int TEXTURE_UNIT_MATERIAL; // Must always be the first non-standard texture unit
static int TEXTURE_UNIT_LIGHTMAPS;
static int TEXTURE_UNIT_DETAIL;
static int TEXTURE_UNIT_CAUSTICS;
static int TEXTURE_UNIT_SKYBOX;
static int TEXTURE_UNIT_SKYDOME_TEXTURE;
static int TEXTURE_UNIT_SKYDOME_CLOUD_TEXTURE;

// We re-compile whenever certain options change, to save texture bindings/lookups
static void Compile_DrawWorldProgram(qbool detail_textures, qbool caustic_textures, qbool luma_textures, qbool skybox)
{
	extern cvar_t gl_textureless;
	int drawworld_desiredOptions =
		(detail_textures ? DRAW_DETAIL_TEXTURES : 0) |
		(caustic_textures ? DRAW_CAUSTIC_TEXTURES : 0) |
		(luma_textures ? DRAW_LUMA_TEXTURES : 0) |
		(luma_textures && gl_fb_bmodels.integer ? DRAW_LUMA_TEXTURES_FB : 0) |
		(r_dynamic.integer == 2 ? DRAW_HARDWARE_LIGHTING : 0) |
		(r_fastsky.integer ? 0 : (skybox ? DRAW_SKYBOX : DRAW_SKYDOME)) |
		(r_drawflat.integer == 1 || r_drawflat.integer == 2 ? DRAW_FLATFLOORS : 0) |
		(r_drawflat.integer == 1 || r_drawflat.integer == 3 ? DRAW_FLATWALLS : 0) |
		(R_DrawLightmaps() ? DRAW_LIGHTMAPS : 0) |
		(gl_textureless.integer ? DRAW_TEXTURELESS : 0);

	if (GLM_ProgramRecompileNeeded(&drawworld, drawworld_desiredOptions)) {
		static char included_definitions[1024];
		int samplers = 0;
		GL_VFDeclare(drawworld);

		memset(included_definitions, 0, sizeof(included_definitions));
		if (detail_textures) {
			TEXTURE_UNIT_DETAIL = samplers++;

			strlcat(included_definitions, "#define DRAW_DETAIL_TEXTURES\n", sizeof(included_definitions));
			strlcat(included_definitions, va("#define SAMPLER_DETAIL_TEXTURE %d\n", TEXTURE_UNIT_DETAIL), sizeof(included_definitions));
		}
		if (caustic_textures) {
			TEXTURE_UNIT_CAUSTICS = samplers++;

			strlcat(included_definitions, "#define DRAW_CAUSTIC_TEXTURES\n", sizeof(included_definitions));
			strlcat(included_definitions, va("#define SAMPLER_CAUSTIC_TEXTURE %d\n", TEXTURE_UNIT_CAUSTICS), sizeof(included_definitions));
		}
		if (luma_textures && r_drawflat.integer != 1 && !R_DrawLightmaps()) {
			strlcat(included_definitions, "#define DRAW_LUMA_TEXTURES\n", sizeof(included_definitions));
			if (gl_fb_bmodels.integer) {
				strlcat(included_definitions, "#define DRAW_LUMA_TEXTURES_FB\n", sizeof(included_definitions));
			}
		}
		if (skybox) {
			TEXTURE_UNIT_SKYBOX = samplers++;

			strlcat(included_definitions, "#define DRAW_SKYBOX\n", sizeof(included_definitions));
			strlcat(included_definitions, va("#define SAMPLER_SKYBOX_TEXTURE %d\n", TEXTURE_UNIT_SKYBOX), sizeof(included_definitions));
		}
		else if (!r_fastsky.integer) {
			TEXTURE_UNIT_SKYDOME_TEXTURE = samplers++;
			TEXTURE_UNIT_SKYDOME_CLOUD_TEXTURE = samplers++;

			strlcat(included_definitions, "#define DRAW_SKYDOME\n", sizeof(included_definitions));
			strlcat(included_definitions, va("#define SAMPLER_SKYDOME_TEXTURE %d\n", TEXTURE_UNIT_SKYDOME_TEXTURE), sizeof(included_definitions));
			strlcat(included_definitions, va("#define SAMPLER_SKYDOME_CLOUDTEXTURE %d\n", TEXTURE_UNIT_SKYDOME_CLOUD_TEXTURE), sizeof(included_definitions));
		}
		TEXTURE_UNIT_LIGHTMAPS = samplers++;
		strlcat(included_definitions, va("#define SAMPLER_LIGHTMAP_TEXTURE %d\n", TEXTURE_UNIT_LIGHTMAPS), sizeof(included_definitions));
		TEXTURE_UNIT_MATERIAL = samplers++;
		strlcat(included_definitions, va("#define SAMPLER_MATERIAL_TEXTURE_START %d\n", TEXTURE_UNIT_MATERIAL), sizeof(included_definitions));
		material_samplers_max = min(glConfig.texture_units - TEXTURE_UNIT_MATERIAL, MAXIMUM_MATERIAL_SAMPLERS);
		strlcat(included_definitions, va("#define SAMPLER_MATERIAL_TEXTURE_COUNT %d\n", material_samplers_max), sizeof(included_definitions));
		strlcat(included_definitions, va("#define MAX_INSTANCEID %d\n", MAX_WORLDMODEL_BATCH), sizeof(included_definitions));
		strlcat(included_definitions, va("#define MAX_MATRICES %d\n", MAX_WORLDMODEL_MATRICES), sizeof(included_definitions));
		if (r_dynamic.integer == 2) {
			strlcat(included_definitions, "#define HARDWARE_LIGHTING\n", sizeof(included_definitions));
		}
		if (r_drawflat.integer == 1 || r_drawflat.integer == 2) {
			strlcat(included_definitions, "#define DRAW_FLATFLOORS\n", sizeof(included_definitions));
		}
		if (r_drawflat.integer == 1 || r_drawflat.integer == 3) {
			strlcat(included_definitions, "#define DRAW_FLATWALLS\n", sizeof(included_definitions));
		}
		if (R_DrawLightmaps()) {
			strlcat(included_definitions, "#define DRAW_LIGHTMAPS\n", sizeof(included_definitions));
		}
		if (gl_textureless.integer) {
			strlcat(included_definitions, "#define DRAW_TEXTURELESS\n", sizeof(included_definitions));
		}

		// Initialise program for drawing image
		GLM_CreateVFProgramWithInclude("DrawWorld", GL_VFParams(drawworld), &drawworld, included_definitions);

		drawworld.custom_options = drawworld_desiredOptions;
	}

	if (drawworld.program && !drawworld.uniforms_found) {
		GLint size;

		drawworld_WorldCvars_block = glGetUniformBlockIndex(drawworld.program, "WorldCvars");
		glGetActiveUniformBlockiv(drawworld.program, drawworld_WorldCvars_block, GL_UNIFORM_BLOCK_DATA_SIZE, &size);

		drawWorld_outlines = glGetUniformLocation(drawworld.program, "draw_outlines");

		glUniformBlockBinding(drawworld.program, drawworld_WorldCvars_block, GL_BINDINGPOINT_DRAWWORLD_CVARS);
		ubo_worldcvars = GL_GenUniformBuffer("world-cvars", &world, sizeof(world));
		GL_BindBufferBase(ubo_worldcvars, GL_BINDINGPOINT_DRAWWORLD_CVARS);

		drawworld.uniforms_found = true;
	}

	if (!GL_BufferReferenceIsValid(vbo_worldIndirectDraw)) {
		vbo_worldIndirectDraw = GL_GenFixedBuffer(GL_DRAW_INDIRECT_BUFFER, "world-indirect", sizeof(worldmodel_requests), NULL, GL_STREAM_DRAW);
	}
}

void GLM_EnterBatchedWorldRegion(void)
{
	extern glm_vao_t brushModel_vao;
	extern buffer_ref vbo_brushElements;
	extern cvar_t gl_lumaTextures;
	extern cvar_t gl_lumaTextures;

	qbool draw_detail_texture = gl_detail.integer && GL_TextureReferenceIsValid(detailtexture);
	qbool draw_caustics = gl_caustics.integer && GL_TextureReferenceIsValid(underwatertexture);
	qbool draw_lumas = gl_lumaTextures.integer && r_refdef2.allow_lumas;

	texture_ref std_textures[MAX_STANDARD_TEXTURES];
	int std_count = 0;

	qbool skybox = r_skyboxloaded && !r_fastsky.integer;
	qbool skydome = !(r_skyboxloaded || r_fastsky.integer);

	Compile_DrawWorldProgram(draw_detail_texture, draw_caustics, draw_lumas, skybox);

	GL_UseProgram(drawworld.program);
	GL_BindVertexArray(&brushModel_vao);

	// Bind standard textures (warning: these must be in the same order)
	std_textures[TEXTURE_UNIT_LIGHTMAPS] = GLM_LightmapArray();
	if (draw_detail_texture) {
		std_textures[TEXTURE_UNIT_DETAIL] = detailtexture;
	}
	if (draw_caustics) {
		std_textures[TEXTURE_UNIT_CAUSTICS] = underwatertexture;
	}
	if (skybox) {
		extern texture_ref skybox_cubeMap;

		std_textures[TEXTURE_UNIT_SKYBOX] = skybox_cubeMap;
	}
	else if (skydome) {
		extern texture_ref solidskytexture, alphaskytexture;

		std_textures[TEXTURE_UNIT_SKYDOME_TEXTURE] = solidskytexture;
		std_textures[TEXTURE_UNIT_SKYDOME_CLOUD_TEXTURE] = alphaskytexture;
	}

	GL_BindTextures(0, TEXTURE_UNIT_MATERIAL, std_textures);

	material_samplers = 0;
	sampler_mappings = 0;
}

void GLM_ExitBatchedPolyRegion(void)
{
	//uniforms_set = false;

	//Cvar_SetValue(&developer, 0);
}

// Matrices often duplicated (camera position, entity position) so shared over draw calls
// Finds matrix in the common list, adds if missing or returns -1 if list is full
static int FindMatrix(void)
{
	float mvMatrix[16];
	int i;

	GLM_GetMatrix(GL_MODELVIEW, mvMatrix);
	for (i = 0; i < matrix_count; ++i) {
		if (!memcmp(world.modelMatrix[i], mvMatrix, sizeof(mvMatrix))) {
			return i;
		}
	}

	if (matrix_count < MAX_WORLDMODEL_MATRICES) {
		memcpy(world.modelMatrix[matrix_count], mvMatrix, sizeof(mvMatrix));
		return matrix_count++;
	}

	return -1;
}

static qbool GLM_AssignTexture(glm_worldmodel_req_t* req, int texture_num, texture_t* texture)
{
	int index = req->samplerMappingBase + texture_num;
	int i;
	int sampler = -1;

	if (index >= sizeof(world.mappings) / sizeof(world.mappings[0])) {
		return false;
	}

	for (i = 0; i < material_samplers; ++i) {
		if (GL_TextureReferenceEqual(texture->gl_texture_array, allocated_samplers[i])) {
			sampler = i;
			break;
		}
	}

	if (sampler < 0) {
		if (material_samplers >= material_samplers_max) {
			GL_FlushWorldModelBatch();
		}

		sampler = material_samplers++;
		allocated_samplers[sampler] = texture->gl_texture_array;
	}

	world.mappings[index].samplerIndex = sampler;
	world.mappings[index].arrayIndex = texture->gl_texture_index;
	world.mappings[index].flags = GL_TextureReferenceIsValid(texture->fb_texturenum) ? EZQ_SURFACE_HAS_LUMA : 0;
	return true;
}

static glm_worldmodel_req_t* GLM_NextBatchRequest(model_t* model, float alpha, int num_textures, int first_texture, qbool polygonOffset, qbool caustics, qbool allow_duplicate)
{
	glm_worldmodel_req_t* req;
	int sampler = -1;
	int matrixMapping = FindMatrix();
	qbool worldmodel = model ? model->isworldmodel : false;
	int flags = (caustics ? EZQ_SURFACE_UNDERWATER : 0);

	// If user has switched off caustics (or no texture), ignore
	if (caustics) {
		caustics &= ((drawworld.custom_options & DRAW_CAUSTIC_TEXTURES) == DRAW_CAUSTIC_TEXTURES);
	}

	// See if previous batch has same texture & matrix, if so just continue
	if (batch_count && matrixMapping >= 0) {
		req = &worldmodel_requests[batch_count - 1];

		if (allow_duplicate && developer.integer == 0 && model == req->model && req->samplerMappingCount == num_textures && req->firstTexture == first_texture && batch_count < MAX_WORLDMODEL_BATCH) {
			// Duplicate details from previous request, but with different matrix
			glm_worldmodel_req_t* newreq = &worldmodel_requests[batch_count];

			memcpy(newreq, req, sizeof(*newreq));
			newreq->mvMatrixMapping = matrixMapping;
			newreq->alpha = alpha;
			newreq->flags = flags;
			newreq->polygonOffset = polygonOffset;
			newreq->worldmodel = worldmodel;
			newreq->baseInstance = batch_count;
			++batch_count;

			return NULL;
		}

		// Try and continue the previous batch
		if (worldmodel == req->worldmodel && matrixMapping == req->mvMatrixMapping && polygonOffset == req->polygonOffset && req->flags == flags) {
			if (num_textures == 0) {
				// We don't care about materials, so can draw with previous batch
				return req;
			}
			else if (req->samplerMappingCount == 0) {
				// Previous block didn't care about materials, allocate now
				if (sampler_mappings < MAX_SAMPLER_MAPPINGS) {
					req->samplerMappingCount = min(num_textures, MAX_SAMPLER_MAPPINGS - sampler_mappings);
					return req;
				}
			}
		}
	}

	if (sampler_mappings >= MAX_SAMPLER_MAPPINGS || batch_count >= MAX_WORLDMODEL_BATCH) {
		GL_FlushWorldModelBatch();
	}

	req = &worldmodel_requests[batch_count];

	// If matrix list was full previously, will be okay now
	if (matrixMapping < 0) {
		matrixMapping = FindMatrix();
	}
	req->mvMatrixMapping = matrixMapping;
	req->alpha = alpha;
	req->samplerMappingBase = sampler_mappings - first_texture;
	req->samplerMappingCount = min(num_textures, MAX_SAMPLER_MAPPINGS - sampler_mappings);
	req->flags = flags;
	req->polygonOffset = polygonOffset;
	req->worldmodel = worldmodel;
	req->model = model;

	req->count = 0;
	req->instanceCount = 1;
	req->firstIndex = index_count;
	req->baseVertex = 0;
	req->baseInstance = batch_count;

	sampler_mappings += req->samplerMappingCount;
	++batch_count;
	return req;
}

void GLM_DrawWaterSurfaces(void)
{
	extern msurface_t* waterchain;
	msurface_t* surf;
	glm_worldmodel_req_t* req = NULL;
	qbool alpha = GL_WaterAlpha();
	int v;

	if (!waterchain) {
		return;
	}

	// Waterchain has list of alpha-blended surfaces
	GLM_EnterBatchedWorldRegion();
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	for (surf = waterchain; surf; surf = surf->texturechain) {
		glpoly_t* poly;
		texture_t* tex = R_TextureAnimation(surf->texinfo->texture);

		req = GLM_NextBatchRequest(NULL, alpha, 1, surf->texinfo->miptex, false, false, false);
		GLM_AssignTexture(req, surf->texinfo->miptex, tex);
		for (poly = surf->polys; poly; poly = poly->next) {
			int newVerts = poly->numverts;

			if (index_count + 1 + newVerts > modelIndexMaximum) {
				GL_FlushWorldModelBatch();
				req = GLM_NextBatchRequest(NULL, alpha, 1, surf->texinfo->miptex, false, false, false);
				GLM_AssignTexture(req, surf->texinfo->miptex, tex);
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

void GLM_DrawTexturedWorld(model_t* model)
{
	int i, waterline, v;
	msurface_t* surf;
	glm_worldmodel_req_t* req = NULL;

	GLM_EnterBatchedWorldRegion();

	for (waterline = 0; waterline < 2; waterline++) {
		for (surf = model->drawflat_chain[waterline]; surf; surf = surf->drawflatchain) {
			glpoly_t* poly;

			if (!req) {
				req = GLM_NextBatchRequest(model, 1.0f, 0, 0, false, false, false);
			}

			for (poly = surf->polys; poly; poly = poly->next) {
				int newVerts = poly->numverts;

				if (index_count + 1 + newVerts > modelIndexMaximum) {
					GL_FlushWorldModelBatch();
					req = GLM_NextBatchRequest(model, 1.0f, 0, 0, false, false, false);
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

	req = GLM_NextBatchRequest(model, 1.0f, model->last_texture_chained - model->first_texture_chained + 1, model->first_texture_chained, false, false, false);
	for (i = model->first_texture_chained; i <= model->last_texture_chained; ++i) {
		texture_t* tex = model->textures[i];

		if (!tex || !tex->loaded || (!tex->texturechain[0] && !tex->texturechain[1])) {
			continue;
		}

		tex = R_TextureAnimation(tex);
		if (!GLM_AssignTexture(req, i, tex)) {
			req = GLM_NextBatchRequest(model, 1.0f, model->last_texture_chained - i + 1, i, false, false, false);
			GLM_AssignTexture(req, i, tex);
		}

		for (waterline = 0; waterline < 2; waterline++) {
			for (surf = model->textures[i]->texturechain[waterline]; surf; surf = surf->texturechain) {
				glpoly_t* poly;

				for (poly = surf->polys; poly; poly = poly->next) {
					int newVerts = poly->numverts;

					if (index_count + 1 + newVerts > modelIndexMaximum) {
						GL_FlushWorldModelBatch();
						req = GLM_NextBatchRequest(model, 1.0f, model->last_texture_chained - i + 1, i, false, false, false);
						GLM_AssignTexture(req, i, tex);
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

	GL_FlushWorldModelBatch();

	return;
}

static void GLM_DrawWorldModelOutlines(void)
{
	extern cvar_t gl_outline_width;
	int begin = -1;
	int i;

	//
	glUniform1i(drawWorld_outlines, 1);

	GLM_StateBeginDrawWorldOutlines();

	for (i = 0; i <= batch_count; ++i) {
		if (!worldmodel_requests[i].worldmodel) {
			if (begin >= 0) {
				// Draw outline models so far
				glMultiDrawElementsIndirect(GL_TRIANGLE_STRIP, GL_UNSIGNED_INT, (void*)(begin * sizeof(worldmodel_requests[0])), i - begin, sizeof(worldmodel_requests[0]));
			}
			begin = -1;
			continue;
		}
		else if (begin < 0) {
			begin = i;
		}
	}
	if (begin >= 0) {
		// Draw outline models so far
		glMultiDrawElementsIndirect(GL_TRIANGLE_STRIP, GL_UNSIGNED_INT, (void*)(begin * sizeof(worldmodel_requests[0])), batch_count - begin, sizeof(worldmodel_requests[0]));
	}

	// Valid to reset the uniforms here as this is the only code that expects it
	glUniform1i(drawWorld_outlines, 0);

	GLM_StateEndDrawWorldOutlines();
}

void GL_FlushWorldModelBatch(void)
{
	int polygonOffsetStart = 0;
	extern buffer_ref vbo_brushElements;
	extern glm_vao_t brushModel_vao;

	if (!batch_count) {
		return;
	}

	GL_SortDrawCalls(&polygonOffsetStart);

	GL_UseProgram(drawworld.program);
	GL_BindVertexArray(&brushModel_vao);
	GL_UpdateBuffer(ubo_worldcvars, sizeof(world), &world);
	GL_BindAndUpdateBuffer(vbo_brushElements, sizeof(modelIndexes[0]) * index_count, modelIndexes);
	GL_BindAndUpdateBuffer(vbo_worldIndirectDraw, sizeof(worldmodel_requests[0]) * batch_count, &worldmodel_requests);

	// Bind texture units
	GL_BindTextures(TEXTURE_UNIT_MATERIAL, material_samplers, allocated_samplers);

	if (polygonOffsetStart >= 0 && polygonOffsetStart < batch_count) {
		if (polygonOffsetStart) {
			glMultiDrawElementsIndirect(
				GL_TRIANGLE_STRIP,
				GL_UNSIGNED_INT,
				(void*)0,
				polygonOffsetStart,
				sizeof(worldmodel_requests[0])
			);
		}

		GL_PolygonOffset(POLYGONOFFSET_STANDARD);
		glMultiDrawElementsIndirect(
			GL_TRIANGLE_STRIP,
			GL_UNSIGNED_INT,
			(void*)(sizeof(worldmodel_requests[0]) * polygonOffsetStart),
			batch_count - polygonOffsetStart,
			sizeof(worldmodel_requests[0])
		);
		GL_PolygonOffset(POLYGONOFFSET_DISABLED);

		frameStats.draw_calls += 2;
	}
	else {
		glMultiDrawElementsIndirect(
			GL_TRIANGLE_STRIP,
			GL_UNSIGNED_INT,
			(void*)0,
			batch_count,
			sizeof(worldmodel_requests[0])
		);

		frameStats.draw_calls++;
	}

	if (R_DrawWorldOutlines()) {
		GLM_DrawWorldModelOutlines();
	}

	frameStats.subdraw_calls += batch_count;
	batch_count = 0;
	index_count = 0;
	matrix_count = 0;
	material_samplers = 0;
	sampler_mappings = 0;
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

void GLM_DrawBrushModel(model_t* model, qbool polygonOffset, qbool caustics)
{
	int i, waterline, v;
	msurface_t* surf;
	float base_color[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glm_worldmodel_req_t* req = NULL;

	req = GLM_NextBatchRequest(model, 1.0f, model->last_texture_chained - model->first_texture_chained + 1, model->first_texture_chained, polygonOffset, caustics, true);
	if (!req) {
		return;
	}

	for (waterline = 0; waterline < 2; waterline++) {
		for (surf = model->drawflat_chain[waterline]; surf; surf = surf->drawflatchain) {
			glpoly_t* poly;

			if (!req) {
				req = GLM_NextBatchRequest(model, 1.0f, 0, 0, false, false, false);
			}

			for (poly = surf->polys; poly; poly = poly->next) {
				int newVerts = poly->numverts;

				if (index_count + 1 + newVerts > modelIndexMaximum) {
					GL_FlushWorldModelBatch();
					req = GLM_NextBatchRequest(model, 1.0f, 0, 0, false, false, false);
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

	for (i = model->first_texture_chained; i <= model->last_texture_chained; ++i) {
		texture_t* tex = model->textures[i];

		if (!tex || !GL_TextureReferenceIsValid(tex->gl_texture_array)) {
			continue;
		}

		if (!tex->texturechain[0] && !tex->texturechain[1]) {
			continue;
		}

		tex = R_TextureAnimation(tex);

		GLM_AssignTexture(req, i, tex);
		for (waterline = 0; waterline < 2; ++waterline) {
			for (surf = model->textures[i]->texturechain[waterline]; surf; surf = surf->texturechain) {
				glpoly_t* poly;

				for (poly = surf->polys; poly; poly = poly->next) {
					int newVerts = poly->numverts;

					if (index_count + 1 + newVerts > modelIndexMaximum) {
						GL_FlushWorldModelBatch();
						req = GLM_NextBatchRequest(model, 1.0f, model->last_texture_chained - i + 1, i, polygonOffset, caustics, false);
						GLM_AssignTexture(req, i, tex);
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

	return;
}

static void GL_SortDrawCalls(int* split)
{
	int i;
	int back;
	int index = 0;
	int overall_index_count = index_count;

	*split = batch_count;
	if (batch_count == 0) {
		return;
	}

	// All 'true' values need to be moved to the back
	back = batch_count - 1;
	while (back >= 0 && worldmodel_requests[back].polygonOffset) {
		*split = back;
		--back;
	}

	for (i = 0; i < batch_count; ++i) {
		glm_worldmodel_req_t* this = &worldmodel_requests[i];

		if (this->polygonOffset && i < back) {
			glm_worldmodel_req_t copy = worldmodel_requests[back];
			worldmodel_requests[back] = *this;
			*this = copy;

			while (back >= 0 && worldmodel_requests[back].polygonOffset) {
				*split = back;
				--back;
			}
		}
	}

	for (i = 0; i < batch_count; ++i) {
		glm_worldmodel_req_t* this = &worldmodel_requests[i];

		world.calls[i].alpha = this->alpha;
		world.calls[i].flags = this->flags;
		world.calls[i].matrixMapping = this->mvMatrixMapping;
		world.calls[i].samplerBase = this->samplerMappingBase;
		this->baseInstance = i;
	}
}
