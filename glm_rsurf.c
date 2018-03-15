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
#include "glm_brushmodel.h"

#define GLM_DRAWCALL_INCREMENT 8

void GL_StartWaterSurfaceBatch(void);

extern buffer_ref brushModel_vbo;
extern GLuint* modelIndexes;
extern GLuint modelIndexMaximum;
static GLuint index_count;
static buffer_ref vbo_worldIndirectDraw;
static buffer_ref ssbo_worldcvars;
static buffer_ref ssbo_worldsamplers;

typedef struct glm_brushmodel_drawcall_s {
	uniform_block_world_calldata_t calls[MAX_WORLDMODEL_BATCH];
	sampler_mapping_t mappings[MAX_SAMPLER_MAPPINGS];

	glm_worldmodel_req_t worldmodel_requests[MAX_WORLDMODEL_BATCH];
	texture_ref allocated_samplers[MAXIMUM_MATERIAL_SAMPLERS];
	int material_samplers;
	int sampler_mappings;
	int batch_count;
	GLintptr indirectDrawOffset;

	int polygonOffsetSplit;
	glm_brushmodel_drawcall_type type;

	struct glm_brushmodel_drawcall_s* next;
} glm_brushmodel_drawcall_t;

static void GL_SortDrawCalls(glm_brushmodel_drawcall_t* drawcall);

static glm_brushmodel_drawcall_t* drawcalls;
static int maximum_drawcalls = 0;
static int current_drawcall = 0;

static void GLM_CheckDrawCallSize(void)
{
	if (current_drawcall >= maximum_drawcalls) {
		maximum_drawcalls += GLM_DRAWCALL_INCREMENT;
		drawcalls = realloc(drawcalls, sizeof(glm_brushmodel_drawcall_t) * maximum_drawcalls);
	}
}

static glm_program_t drawworld;
static GLint drawWorld_outlines;
static GLuint drawworld_WorldCvars_block;

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

static int material_samplers_max;
static int TEXTURE_UNIT_MATERIAL; // Must always be the first non-standard texture unit
static int TEXTURE_UNIT_LIGHTMAPS;
static int TEXTURE_UNIT_DETAIL;
static int TEXTURE_UNIT_CAUSTICS;
static int TEXTURE_UNIT_SKYBOX;
static int TEXTURE_UNIT_SKYDOME_TEXTURE;
static int TEXTURE_UNIT_SKYDOME_CLOUD_TEXTURE;

// We re-compile whenever certain options change, to save texture bindings/lookups
static void Compile_DrawWorldProgram(void)
{
	extern cvar_t gl_lumaTextures;
	extern cvar_t gl_textureless;

	qbool detail_textures = gl_detail.integer && GL_TextureReferenceIsValid(detailtexture);
	qbool caustic_textures = gl_caustics.integer && GL_TextureReferenceIsValid(underwatertexture);
	qbool luma_textures = gl_lumaTextures.integer && r_refdef2.allow_lumas;
	qbool skybox = r_skyboxloaded && !r_fastsky.integer;

	int drawworld_desiredOptions =
		(detail_textures ? DRAW_DETAIL_TEXTURES : 0) |
		(caustic_textures ? DRAW_CAUSTIC_TEXTURES : 0) |
		(luma_textures ? DRAW_LUMA_TEXTURES : 0) |
		(luma_textures && gl_fb_bmodels.integer ? DRAW_LUMA_TEXTURES_FB : 0) |
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
		drawWorld_outlines = glGetUniformLocation(drawworld.program, "draw_outlines");

		ssbo_worldcvars = GL_CreateFixedBuffer(GL_SHADER_STORAGE_BUFFER, NULL, sizeof(drawcalls[0].calls) * GLM_DRAWCALL_INCREMENT, NULL, write_once_use_once);
		ssbo_worldsamplers = GL_CreateFixedBuffer(GL_SHADER_STORAGE_BUFFER, NULL, sizeof(drawcalls[0].mappings) * GLM_DRAWCALL_INCREMENT, NULL, write_once_use_once);

		drawworld.uniforms_found = true;
	}

	if (!GL_BufferReferenceIsValid(vbo_worldIndirectDraw)) {
		vbo_worldIndirectDraw = GL_CreateFixedBuffer(GL_DRAW_INDIRECT_BUFFER, "world-indirect", sizeof(drawcalls[0].worldmodel_requests) * GLM_DRAWCALL_INCREMENT, NULL, write_once_use_once);
	}
}

static glm_worldmodel_req_t* GLM_CurrentRequest(void)
{
	glm_brushmodel_drawcall_t* call = &drawcalls[current_drawcall];

	return &call->worldmodel_requests[call->batch_count - 1];
}

static void GL_StartWorldBatch(void)
{
	extern glm_vao_t brushModel_vao;
	texture_ref std_textures[MAX_STANDARD_TEXTURES];

	GL_UseProgram(drawworld.program);
	GL_BindVertexArray(&brushModel_vao);

	// Bind standard textures
	std_textures[TEXTURE_UNIT_LIGHTMAPS] = GLM_LightmapArray();
	if (drawworld.custom_options & DRAW_DETAIL_TEXTURES) {
		std_textures[TEXTURE_UNIT_DETAIL] = detailtexture;
	}
	if (drawworld.custom_options & DRAW_CAUSTIC_TEXTURES) {
		std_textures[TEXTURE_UNIT_CAUSTICS] = underwatertexture;
	}
	if (drawworld.custom_options & DRAW_SKYBOX) {
		extern texture_ref skybox_cubeMap;

		std_textures[TEXTURE_UNIT_SKYBOX] = skybox_cubeMap;
	}
	else if (drawworld.custom_options & DRAW_SKYDOME) {
		extern texture_ref solidskytexture, alphaskytexture;

		std_textures[TEXTURE_UNIT_SKYDOME_TEXTURE] = solidskytexture;
		std_textures[TEXTURE_UNIT_SKYDOME_CLOUD_TEXTURE] = alphaskytexture;
	}
	GL_BindTextures(0, TEXTURE_UNIT_MATERIAL, std_textures);
}

void GLM_EnterBatchedWorldRegion(void)
{
	Compile_DrawWorldProgram();

	current_drawcall = 0;
	index_count = 0;
	GLM_CheckDrawCallSize();
	memset(drawcalls, 0, sizeof(drawcalls[0]) * maximum_drawcalls);
}

static qbool GLM_AssignTexture(int texture_num, texture_t* texture)
{
	glm_worldmodel_req_t* req = GLM_CurrentRequest();
	int index = req->samplerMappingBase + texture_num;
	int i;
	int sampler = -1;

	if (index >= sizeof(drawcalls[current_drawcall].mappings) / sizeof(drawcalls[current_drawcall].mappings[0])) {
		return false;
	}

	for (i = 0; i < drawcalls[current_drawcall].material_samplers; ++i) {
		if (GL_TextureReferenceEqual(texture->gl_texture_array, drawcalls[current_drawcall].allocated_samplers[i])) {
			sampler = i;
			break;
		}
	}

	if (sampler < 0) {
		if (drawcalls[current_drawcall].material_samplers >= material_samplers_max) {
			GL_FlushWorldModelBatch();
		}

		sampler = drawcalls[current_drawcall].material_samplers++;
		drawcalls[current_drawcall].allocated_samplers[sampler] = texture->gl_texture_array;
	}

	drawcalls[current_drawcall].mappings[index].samplerIndex = sampler;
	drawcalls[current_drawcall].mappings[index].arrayIndex = texture->gl_texture_index;
	drawcalls[current_drawcall].mappings[index].flags = GL_TextureReferenceIsValid(texture->fb_texturenum) ? EZQ_SURFACE_HAS_LUMA : 0;
	return true;
}

static qbool GLM_DuplicatePreviousRequest(model_t* model, float alpha, int num_textures, int first_texture, qbool polygonOffset, qbool caustics)
{
	glm_worldmodel_req_t* req;
	qbool worldmodel = model ? model->isworldmodel : false;
	int flags = (caustics ? EZQ_SURFACE_UNDERWATER : 0);
	glm_brushmodel_drawcall_t* drawcall = &drawcalls[current_drawcall];

	// If user has switched off caustics (or no texture), ignore
	if (caustics) {
		caustics &= ((drawworld.custom_options & DRAW_CAUSTIC_TEXTURES) == DRAW_CAUSTIC_TEXTURES);
	}

	// See if previous batch has same texture & matrix, if so just continue
	if (drawcall->batch_count) {
		req = &drawcall->worldmodel_requests[drawcall->batch_count - 1];

		if (model == req->model && req->samplerMappingCount == num_textures && req->firstTexture == first_texture && drawcall->batch_count < MAX_WORLDMODEL_BATCH) {
			// Duplicate details from previous request, but with different matrix
			glm_worldmodel_req_t* newreq = &drawcall->worldmodel_requests[drawcall->batch_count];

			memcpy(newreq, req, sizeof(*newreq));
			GL_GetMatrix(GL_MODELVIEW, newreq->mvMatrix);
			newreq->alpha = alpha;
			newreq->flags = flags;
			newreq->polygonOffset = polygonOffset;
			newreq->worldmodel = worldmodel;
			newreq->baseInstance = drawcall->batch_count;
			++drawcall->batch_count;

			return true;
		}
	}

	return false;
}

static glm_worldmodel_req_t* GLM_NextBatchRequest(model_t* model, float alpha, int num_textures, int first_texture, qbool polygonOffset, qbool caustics, qbool allow_duplicate)
{
	glm_worldmodel_req_t* req;
	qbool worldmodel = model ? model->isworldmodel : false;
	int flags = (caustics ? EZQ_SURFACE_UNDERWATER : 0);
	glm_brushmodel_drawcall_t* drawcall = &drawcalls[current_drawcall];
	float mvMatrix[16];

	GL_GetMatrix(GL_MODELVIEW, mvMatrix);

	// If user has switched off caustics (or no texture), ignore
	if (caustics) {
		caustics &= ((drawworld.custom_options & DRAW_CAUSTIC_TEXTURES) == DRAW_CAUSTIC_TEXTURES);
	}

	// See if previous batch has same texture & matrix, if so just continue
	if (drawcall->batch_count) {
		req = &drawcall->worldmodel_requests[drawcall->batch_count - 1];

		if (allow_duplicate && model == req->model && req->samplerMappingCount == num_textures && req->firstTexture == first_texture && drawcall->batch_count < MAX_WORLDMODEL_BATCH) {
			// Duplicate details from previous request, but with different matrix
			glm_worldmodel_req_t* newreq = &drawcall->worldmodel_requests[drawcall->batch_count];

			memcpy(newreq, req, sizeof(*newreq));
			GL_GetMatrix(GL_MODELVIEW, newreq->mvMatrix);
			newreq->alpha = alpha;
			newreq->flags = flags;
			newreq->polygonOffset = polygonOffset;
			newreq->worldmodel = worldmodel;
			newreq->baseInstance = drawcall->batch_count;
			++drawcall->batch_count;

			return NULL;
		}

		// Try and continue the previous batch
		if (worldmodel == req->worldmodel && !memcmp(req->mvMatrix, mvMatrix, sizeof(req->mvMatrix)) && polygonOffset == req->polygonOffset && req->flags == flags) {
			if (num_textures == 0) {
				// We don't care about materials, so can draw with previous batch
				return req;
			}
			else if (req->samplerMappingCount == 0) {
				// Previous block didn't care about materials, allocate now
				if (drawcall->sampler_mappings < MAX_SAMPLER_MAPPINGS) {
					req->samplerMappingBase = drawcall->sampler_mappings - first_texture;
					req->samplerMappingCount = min(num_textures, MAX_SAMPLER_MAPPINGS - drawcall->sampler_mappings);
					drawcall->sampler_mappings += req->samplerMappingCount;
					return req;
				}
			}
		}
	}

	if (drawcall->sampler_mappings >= MAX_SAMPLER_MAPPINGS || drawcall->batch_count >= MAX_WORLDMODEL_BATCH) {
		GL_FlushWorldModelBatch();

		drawcall = &drawcalls[current_drawcall];
		memset(drawcall, 0, sizeof(*drawcall));
	}

	req = &drawcall->worldmodel_requests[drawcall->batch_count];

	// If matrix list was full previously, will be okay now
	memcpy(req->mvMatrix, mvMatrix, sizeof(req->mvMatrix));
	req->alpha = alpha;
	req->samplerMappingBase = drawcall->sampler_mappings - first_texture;
	req->samplerMappingCount = min(num_textures, MAX_SAMPLER_MAPPINGS - drawcall->sampler_mappings);
	req->flags = flags;
	req->polygonOffset = polygonOffset;
	req->worldmodel = worldmodel;
	req->model = model;

	req->count = 0;
	req->instanceCount = 1;
	req->firstIndex = index_count;
	req->baseVertex = 0;
	req->baseInstance = drawcall->batch_count;

	drawcall->sampler_mappings += req->samplerMappingCount;
	++drawcall->batch_count;
	return req;
}

void GLM_DrawWaterSurfaces(void)
{
	extern msurface_t* waterchain;
	msurface_t* surf;
	glm_worldmodel_req_t* req = NULL;
	float alpha;
	int v;

	if (!waterchain) {
		return;
	}

	alpha = GL_WaterAlpha();

	// Waterchain has list of alpha-blended surfaces
	GL_EnterRegion(__FUNCTION__);

	GL_StartWaterSurfaceBatch();
	for (surf = waterchain; surf; surf = surf->texturechain) {
		glpoly_t* poly;
		texture_t* tex = R_TextureAnimation(surf->texinfo->texture);

		req = GLM_NextBatchRequest(NULL, alpha, 1, surf->texinfo->miptex, false, false, false);
		GLM_AssignTexture(surf->texinfo->miptex, tex);
		for (poly = surf->polys; poly; poly = poly->next) {
			int newVerts = poly->numverts;

			if (index_count + 1 + newVerts > modelIndexMaximum) {
				// Should never happen now, we allocate enough space
				continue;
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

	GL_LeaveRegion();

	waterchain = NULL;
}

static glm_worldmodel_req_t* GLM_DrawFlatChain(glm_worldmodel_req_t* req, msurface_t* surf)
{
	glpoly_t* poly;
	int v;

	while (surf) {
		for (poly = surf->polys; poly; poly = poly->next) {
			int newVerts = poly->numverts;

			if (index_count + 1 + newVerts > modelIndexMaximum) {
				// Should never happen now
				continue;
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

		surf = surf->drawflatchain;
	}

	return req;
}

static glm_worldmodel_req_t* GLM_DrawTexturedChain(glm_worldmodel_req_t* req, msurface_t* surf, texture_t* tex, int i)
{
	glpoly_t* poly;
	int v;

	while (surf) {
		for (poly = surf->polys; poly; poly = poly->next) {
			int newVerts = poly->numverts;

			if (index_count + 1 + newVerts > modelIndexMaximum) {
				// Should never happen now
				continue;
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

		surf = surf->texturechain;
	}

	return req;
}

static void GLM_DrawWorldModelOutlines(glm_brushmodel_drawcall_t* drawcall)
{
	int begin = -1;
	int i;
	unsigned int extra_offset = GL_BufferOffset(vbo_worldIndirectDraw);

	//
	glUniform1i(drawWorld_outlines, 1);

	GLM_StateBeginDrawWorldOutlines();

	for (i = 0; i <= drawcall->batch_count; ++i) {
		if (!drawcall->worldmodel_requests[i].worldmodel) {
			if (begin >= 0) {
				// Draw outline models so far
				GL_MultiDrawElementsIndirect(GL_TRIANGLE_STRIP, GL_UNSIGNED_INT, (void*)(extra_offset + begin * sizeof(drawcall->worldmodel_requests[0])), i - begin, sizeof(drawcall->worldmodel_requests[0]));
			}
			begin = -1;
			continue;
		}
		else if (begin < 0) {
			begin = i;
		}
	}
	if (begin >= 0) {
		// Draw the rest
		GL_MultiDrawElementsIndirect(GL_TRIANGLE_STRIP, GL_UNSIGNED_INT, (void*)(extra_offset + begin * sizeof(drawcall->worldmodel_requests[0])), drawcall->batch_count - begin, sizeof(drawcall->worldmodel_requests[0]));
	}

	// Valid to reset the uniforms here as this is the only code that expects it
	glUniform1i(drawWorld_outlines, 0);

	GLM_StateEndDrawWorldOutlines();
}

void GL_FlushWorldModelBatch(void)
{
	current_drawcall++;

	GLM_CheckDrawCallSize();
}

void GL_StartWaterSurfaceBatch(void)
{
	GL_FlushWorldModelBatch();

	drawcalls[current_drawcall].type = alpha_surfaces;
}

void GLM_PrepareWorldModelBatch(void)
{
	extern buffer_ref vbo_brushElements;
	GLintptr drawOffset = 0;
	GLintptr indexOffset = GL_BufferOffset(vbo_brushElements) / sizeof(modelIndexes[0]);
	int samplerMappingOffset = 0;
	int batchOffset = 0;
	int draw, i;

	GL_EnterRegion(__FUNCTION__);
	GL_UpdateBuffer(vbo_brushElements, sizeof(modelIndexes[0]) * index_count, modelIndexes);
	GL_EnsureBufferSize(vbo_worldIndirectDraw, sizeof(drawcalls[0].worldmodel_requests) * maximum_drawcalls);
	GL_EnsureBufferSize(ssbo_worldcvars, sizeof(drawcalls[0].calls) * maximum_drawcalls);
	GL_EnsureBufferSize(ssbo_worldsamplers, sizeof(drawcalls[0].mappings) * maximum_drawcalls);

	GL_BindBufferRange(ssbo_worldcvars, EZQ_GL_BINDINGPOINT_BRUSHMODEL_DRAWDATA, GL_BufferOffset(ssbo_worldcvars), sizeof(drawcalls[0].calls) * maximum_drawcalls);
	GL_BindBufferRange(ssbo_worldsamplers, EZQ_GL_BINDINGPOINT_BRUSHMODEL_SAMPLERS, GL_BufferOffset(ssbo_worldsamplers), sizeof(drawcalls[0].mappings) * maximum_drawcalls);

	for (draw = 0; draw <= current_drawcall; ++draw) {
		glm_brushmodel_drawcall_t* drawcall = &drawcalls[draw];

		if (!drawcall->batch_count) {
			break;
		}

		GL_SortDrawCalls(drawcall);

		for (i = 0; i < drawcall->batch_count; ++i) {
			drawcall->calls[i].samplerBase += samplerMappingOffset;
			drawcall->worldmodel_requests[i].baseInstance += batchOffset;
			drawcall->worldmodel_requests[i].firstIndex += indexOffset;
		}

		drawcall->indirectDrawOffset = drawOffset;
		GL_UpdateBufferSection(vbo_worldIndirectDraw, drawOffset, sizeof(drawcall->worldmodel_requests[0]) * drawcall->batch_count, &drawcall->worldmodel_requests);
		drawOffset += sizeof(drawcall->worldmodel_requests[0]) * drawcall->batch_count;

		GL_UpdateBufferSection(ssbo_worldcvars, sizeof(drawcall->calls[0]) * batchOffset, sizeof(drawcall->calls[0]) * drawcall->batch_count, &drawcall->calls);
		GL_UpdateBufferSection(ssbo_worldsamplers, sizeof(drawcall->mappings[0]) * samplerMappingOffset, sizeof(drawcall->mappings[0]) * drawcall->sampler_mappings, &drawcall->mappings);

		batchOffset += drawcall->batch_count;
		samplerMappingOffset += drawcall->sampler_mappings;
	}

	GL_LeaveRegion();
}

void GL_DrawWorldModelBatch(glm_brushmodel_drawcall_type type)
{
	extern buffer_ref vbo_brushElements;
	extern glm_vao_t brushModel_vao;
	int draw;
	qbool first = true;
	unsigned int extra_offset = GL_BufferOffset(vbo_worldIndirectDraw);

	for (draw = 0; draw <= current_drawcall; ++draw) {
		glm_brushmodel_drawcall_t* drawcall = &drawcalls[draw];

		if (!drawcall->batch_count || drawcall->type != type) {
			continue;
		}

		if (first) {
			GL_EnterRegion(__FUNCTION__);
			GL_StartWorldBatch();
			GL_BindBuffer(vbo_brushElements);
			GL_BindBuffer(vbo_worldIndirectDraw);

			GL_AlphaBlendFlags(type == alpha_surfaces ? GL_BLEND_ENABLED : GL_BLEND_DISABLED);

			first = false;
		}

		// Bind texture units
		GL_BindTextures(TEXTURE_UNIT_MATERIAL, drawcall->material_samplers, drawcall->allocated_samplers);

		if (drawcall->polygonOffsetSplit >= 0 && drawcall->polygonOffsetSplit < drawcall->batch_count) {
			if (drawcall->polygonOffsetSplit) {
				GL_MultiDrawElementsIndirect(
					GL_TRIANGLE_STRIP,
					GL_UNSIGNED_INT,
					(void*)(drawcall->indirectDrawOffset + extra_offset),
					drawcall->polygonOffsetSplit,
					sizeof(drawcall->worldmodel_requests[0])
				);
			}

			GL_PolygonOffset(POLYGONOFFSET_STANDARD);
			GL_MultiDrawElementsIndirect(
				GL_TRIANGLE_STRIP,
				GL_UNSIGNED_INT,
				(void*)(extra_offset + drawcall->indirectDrawOffset + sizeof(drawcall->worldmodel_requests[0]) * drawcall->polygonOffsetSplit),
				drawcall->batch_count - drawcall->polygonOffsetSplit,
				sizeof(drawcall->worldmodel_requests[0])
			);
			GL_PolygonOffset(POLYGONOFFSET_DISABLED);

			frameStats.draw_calls += 2;
		}
		else {
			GL_MultiDrawElementsIndirect(
				GL_TRIANGLE_STRIP,
				GL_UNSIGNED_INT,
				(void*)(extra_offset + drawcall->indirectDrawOffset),
				drawcall->batch_count,
				sizeof(drawcall->worldmodel_requests[0])
			);

			frameStats.draw_calls++;
		}

		if (R_DrawWorldOutlines()) {
			GLM_DrawWorldModelOutlines(drawcall);
		}

		frameStats.subdraw_calls += drawcall->batch_count;
		drawcall->batch_count = 0;
		drawcall->material_samplers = 0;
		drawcall->sampler_mappings = 0;
	}

	if (!first) {
		GL_LeaveRegion();
	}
}

void GLM_DrawBrushModel(model_t* model, qbool polygonOffset, qbool caustics)
{
	int i;
	glm_worldmodel_req_t* req = NULL;

	if (model->last_texture_chained < 0) {
		return;
	}

	if (GLM_DuplicatePreviousRequest(model, 1.0f, model->last_texture_chained - model->first_texture_chained + 1, model->first_texture_chained, polygonOffset, caustics)) {
		return;
	}

	if (model->drawflat_chain[0] || model->drawflat_chain[1]) {
		req = GLM_NextBatchRequest(model, 1.0f, 0, 0, false, false, false);

		req = GLM_DrawFlatChain(req, model->drawflat_chain[0]);
		req = GLM_DrawFlatChain(req, model->drawflat_chain[1]);
	}

	req = GLM_NextBatchRequest(model, 1.0f, model->last_texture_chained - model->first_texture_chained + 1, model->first_texture_chained, polygonOffset, caustics, false);
	for (i = model->first_texture_chained; i <= model->last_texture_chained; ++i) {
		texture_t* tex = model->textures[i];

		if (!tex || !tex->loaded || (!tex->texturechain[0] && !tex->texturechain[1]) || !GL_TextureReferenceIsValid(tex->gl_texture_array)) {
			continue;
		}

		tex = R_TextureAnimation(tex);
		if (!GLM_AssignTexture(i, tex)) {
			req = GLM_NextBatchRequest(model, 1.0f, model->last_texture_chained - i + 1, i, polygonOffset, caustics, false);
			GLM_AssignTexture(i, tex);
		}

		req = GLM_DrawTexturedChain(req, model->textures[i]->texturechain[0], tex, i);
		req = GLM_DrawTexturedChain(req, model->textures[i]->texturechain[1], tex, i);
	}
}

static void GL_SortDrawCalls(glm_brushmodel_drawcall_t* drawcall)
{
	int i;
	int back;

	drawcall->polygonOffsetSplit = drawcall->batch_count;
	if (drawcall->batch_count == 0) {
		return;
	}

	// All 'true' values need to be moved to the back
	back = drawcall->batch_count - 1;
	while (back >= 0 && drawcall->worldmodel_requests[back].polygonOffset) {
		drawcall->polygonOffsetSplit = back;
		--back;
	}

	for (i = 0; i < drawcall->batch_count; ++i) {
		glm_worldmodel_req_t* this = &drawcall->worldmodel_requests[i];

		if (this->polygonOffset && i < back) {
			glm_worldmodel_req_t copy = drawcall->worldmodel_requests[back];
			drawcall->worldmodel_requests[back] = *this;
			*this = copy;

			while (back >= 0 && drawcall->worldmodel_requests[back].polygonOffset) {
				drawcall->polygonOffsetSplit = back;
				--back;
			}
		}
	}

	for (i = 0; i < drawcall->batch_count; ++i) {
		glm_worldmodel_req_t* this = &drawcall->worldmodel_requests[i];

		drawcall->calls[i].alpha = this->alpha;
		drawcall->calls[i].flags = this->flags;
		memcpy(drawcall->calls[i].modelMatrix, this->mvMatrix, sizeof(drawcall->calls[i].modelMatrix));
		drawcall->calls[i].samplerBase = this->samplerMappingBase;
		this->baseInstance = i;
	}
}
