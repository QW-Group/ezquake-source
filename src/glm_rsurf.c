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

#ifdef RENDERER_OPTION_MODERN_OPENGL

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"
#include "utils.h"
#include "tr_types.h"
#include "glsl/constants.glsl"
#include "glm_brushmodel.h"
#include "r_brushmodel_sky.h"
#include "r_matrix.h"
#include "glm_vao.h"
#include "glm_local.h"
#include "r_texture.h"
#include "r_brushmodel.h"
#include "r_buffers.h"
#include "r_program.h"
#include "r_renderer.h"

#define GLM_DRAWCALL_INCREMENT 8

void GL_StartWaterSurfaceBatch(void);

static GLuint index_count;

typedef struct glm_worldmodel_req_s {
	// This is DrawElementsIndirectCmd, from OpenGL spec
	GLuint count;           // Number of indexes to pull
	GLuint instanceCount;   // Always 1... ?
	GLuint firstIndex;      // Position of first index in array
	GLuint baseVertex;      // Offset of vertices in VBO
	GLuint baseInstance;    // We use this to pull from array of uniforms in shader

	float mvMatrix[16];
	int flags;
	int samplerMappingBase;
	int samplerMappingCount;
	int firstTexture;
	qbool isAlphaTested;
	float alpha;
	qbool polygonOffset;
	qbool worldmodel;
	model_t* model;
	int nonDynamicSampler;
} glm_worldmodel_req_t;

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

static glm_brushmodel_drawcall_t* GL_FlushWorldModelBatch(void);
static void GL_SortDrawCalls(glm_brushmodel_drawcall_t* drawcall);

static glm_brushmodel_drawcall_t* drawcalls;
static int maximum_drawcalls = 0;
static int current_drawcall = 0;

static void GLM_CheckDrawCallSize(void)
{
	if (current_drawcall >= maximum_drawcalls) {
		maximum_drawcalls += GLM_DRAWCALL_INCREMENT;
		drawcalls = Q_realloc(drawcalls, sizeof(glm_brushmodel_drawcall_t) * maximum_drawcalls);
	}
}

#define DRAW_DETAIL_TEXTURES       (1 <<  0)
#define DRAW_CAUSTIC_TEXTURES      (1 <<  1)
#define DRAW_LUMA_TEXTURES         (1 <<  2)
#define DRAW_SKYBOX                (1 <<  3)
#define DRAW_SKYDOME               (1 <<  4)
#define DRAW_FLATFLOORS            (1 <<  5)
#define DRAW_FLATWALLS             (1 <<  6)
#define DRAW_LUMA_TEXTURES_FB      (1 <<  7)
#define DRAW_TEXTURELESS           (1 <<  8)
#define DRAW_GEOMETRY              (1 <<  9)
#define DRAW_DRAWFLAT_NORMAL       (1 << 10)
#define DRAW_DRAWFLAT_TINTED       (1 << 11)
#define DRAW_DRAWFLAT_BRIGHT       (1 << 12)
#define DRAW_ALPHATESTED           (1 << 13)

static int material_samplers_max;
static int TEXTURE_UNIT_MATERIAL; // Must always be the first non-standard texture unit
static int TEXTURE_UNIT_LIGHTMAPS;
static int TEXTURE_UNIT_DETAIL;
static int TEXTURE_UNIT_CAUSTICS;
static int TEXTURE_UNIT_SKYBOX;
static int TEXTURE_UNIT_SKYDOME_TEXTURE;
static int TEXTURE_UNIT_SKYDOME_CLOUD_TEXTURE;

// We re-compile whenever certain options change, to save texture bindings/lookups
static qbool GLM_CompileDrawWorldProgramImpl(r_program_id program_id, qbool alpha_test)
{
	extern cvar_t gl_lumatextures;
	extern cvar_t gl_textureless;
	extern cvar_t gl_outline;

	qbool detail_textures = gl_detail.integer && R_TextureReferenceIsValid(detailtexture);
	qbool caustic_textures = r_refdef2.drawCaustics;
	qbool luma_textures = gl_lumatextures.integer && r_refdef2.allow_lumas;
	qbool skybox = r_skyboxloaded && !r_fastsky.integer;
	qbool skydome = !skybox && !r_fastsky.integer && R_TextureReferenceIsValid(solidskytexture);

	int drawworld_desiredOptions =
		(detail_textures ? DRAW_DETAIL_TEXTURES : 0) |
		(caustic_textures ? DRAW_CAUSTIC_TEXTURES : 0) |
		(luma_textures ? DRAW_LUMA_TEXTURES : 0) |
		(gl_fb_bmodels.integer ? DRAW_LUMA_TEXTURES_FB : 0) |
		(skybox ? DRAW_SKYBOX : (skydome ? DRAW_SKYDOME : 0)) |
		(r_drawflat_mode.integer == 0 && r_drawflat.integer != 0 ? DRAW_DRAWFLAT_NORMAL : 0) |
		(r_drawflat_mode.integer == 1 && r_drawflat.integer != 0 ? DRAW_DRAWFLAT_TINTED : 0) |
		(r_drawflat_mode.integer == 2 && r_drawflat.integer != 0 ? DRAW_DRAWFLAT_BRIGHT : 0) |
		(r_drawflat.integer == 1 || r_drawflat.integer == 2 ? DRAW_FLATFLOORS : 0) |
		(r_drawflat.integer == 1 || r_drawflat.integer == 3 ? DRAW_FLATWALLS : 0) |
		(gl_textureless.integer ? DRAW_TEXTURELESS : 0) |
		((gl_outline.integer & 2) ? DRAW_GEOMETRY : 0) |
		(alpha_test ? DRAW_ALPHATESTED : 0);

	if (R_ProgramRecompileNeeded(program_id, drawworld_desiredOptions)) {
		static char included_definitions[2048];
		int samplers = 0;

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
		if (r_drawflat.integer != 1 || r_drawflat_mode.integer != 0) {
			if (luma_textures) {
				strlcat(included_definitions, "#define DRAW_LUMA_TEXTURES\n", sizeof(included_definitions));
			}
			if (gl_fb_bmodels.integer) {
				strlcat(included_definitions, "#define DRAW_LUMA_TEXTURES_FB\n", sizeof(included_definitions));
			}
		}
		if (drawworld_desiredOptions & DRAW_DRAWFLAT_NORMAL) {
			strlcat(included_definitions, "#define DRAW_DRAWFLAT_NORMAL\n", sizeof(included_definitions));
		}
		else if (drawworld_desiredOptions & DRAW_DRAWFLAT_TINTED) {
			strlcat(included_definitions, "#define DRAW_DRAWFLAT_TINTED\n", sizeof(included_definitions));
		}
		else if (drawworld_desiredOptions & DRAW_DRAWFLAT_BRIGHT) {
			strlcat(included_definitions, "#define DRAW_DRAWFLAT_BRIGHT\n", sizeof(included_definitions));
		}
		if (drawworld_desiredOptions & DRAW_ALPHATESTED) {
			strlcat(included_definitions, "#define DRAW_ALPHATEST_ENABLED\n", sizeof(included_definitions));
		}

		if (skybox) {
			TEXTURE_UNIT_SKYBOX = samplers++;

			strlcat(included_definitions, "#define DRAW_SKYBOX\n", sizeof(included_definitions));
			strlcat(included_definitions, va("#define SAMPLER_SKYBOX_TEXTURE %d\n", TEXTURE_UNIT_SKYBOX), sizeof(included_definitions));
		}
		else if (skydome) {
			TEXTURE_UNIT_SKYDOME_TEXTURE = samplers++;
			TEXTURE_UNIT_SKYDOME_CLOUD_TEXTURE = samplers++;

			strlcat(included_definitions, "#define DRAW_SKYDOME\n", sizeof(included_definitions));
			strlcat(included_definitions, va("#define SAMPLER_SKYDOME_TEXTURE %d\n", TEXTURE_UNIT_SKYDOME_TEXTURE), sizeof(included_definitions));
			strlcat(included_definitions, va("#define SAMPLER_SKYDOME_CLOUDTEXTURE %d\n", TEXTURE_UNIT_SKYDOME_CLOUD_TEXTURE), sizeof(included_definitions));
		}
		if (gl_outline.integer & 2) {
			strlcat(included_definitions, "#define DRAW_GEOMETRY\n", sizeof(included_definitions));
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
		if (gl_textureless.integer) {
			strlcat(included_definitions, "#define DRAW_TEXTURELESS\n", sizeof(included_definitions));
		}

		// Initialise program for drawing image
		R_ProgramCompileWithInclude(program_id, included_definitions);

		R_ProgramSetCustomOptions(program_id, drawworld_desiredOptions);
	}

	if (!R_BufferReferenceIsValid(r_buffer_brushmodel_drawcall_data)) {
		buffers.Create(r_buffer_brushmodel_drawcall_data, buffertype_storage, "ssbo_worldcvars", sizeof(drawcalls[0].calls) * GLM_DRAWCALL_INCREMENT, NULL, bufferusage_once_per_frame);
	}
	if (!R_BufferReferenceIsValid(r_buffer_brushmodel_worldsamplers_ssbo)) {
		buffers.Create(r_buffer_brushmodel_worldsamplers_ssbo, buffertype_storage, "ssbo_worldsamplers", sizeof(drawcalls[0].mappings) * GLM_DRAWCALL_INCREMENT, NULL, bufferusage_once_per_frame);
	}

	if (!R_BufferReferenceIsValid(r_buffer_brushmodel_drawcall_indirect)) {
		buffers.Create(r_buffer_brushmodel_drawcall_indirect, buffertype_indirect, "world-indirect", sizeof(drawcalls[0].worldmodel_requests) * GLM_DRAWCALL_INCREMENT, NULL, bufferusage_once_per_frame);
	}

	return R_ProgramReady(program_id);
}

qbool GLM_CompileDrawWorldProgram(void)
{
	return GLM_CompileDrawWorldProgramImpl(r_program_brushmodel, false);
}

qbool GLM_CompileDrawWorldProgramAlphaTested(void)
{
	return GLM_CompileDrawWorldProgramImpl(r_program_brushmodel_alphatested, true);
}

static glm_worldmodel_req_t* GLM_CurrentRequest(void)
{
	glm_brushmodel_drawcall_t* call = &drawcalls[current_drawcall];

	return &call->worldmodel_requests[call->batch_count - 1];
}

static void GL_StartWorldBatch(qbool alphatest_enabled)
{
	texture_ref std_textures[MAX_STANDARD_TEXTURES];
	r_program_id program_id = (alphatest_enabled ? r_program_brushmodel_alphatested : r_program_brushmodel);
	int options = R_ProgramCustomOptions(program_id);

	R_ProgramUse(program_id);
	R_BindVertexArray(vao_brushmodel);

	// Bind standard textures
	std_textures[TEXTURE_UNIT_LIGHTMAPS] = GLM_LightmapArray();
	if (options & DRAW_DETAIL_TEXTURES) {
		std_textures[TEXTURE_UNIT_DETAIL] = detailtexture;
	}
	if (options & DRAW_CAUSTIC_TEXTURES) {
		std_textures[TEXTURE_UNIT_CAUSTICS] = underwatertexture;
	}
	if (options & DRAW_SKYBOX) {
		extern texture_ref skybox_cubeMap;

		std_textures[TEXTURE_UNIT_SKYBOX] = skybox_cubeMap;
	}
	else if (options & DRAW_SKYDOME) {
		extern texture_ref solidskytexture, alphaskytexture;

		std_textures[TEXTURE_UNIT_SKYDOME_TEXTURE] = solidskytexture;
		std_textures[TEXTURE_UNIT_SKYDOME_CLOUD_TEXTURE] = alphaskytexture;
	}
	renderer.TextureUnitMultiBind(0, TEXTURE_UNIT_MATERIAL, std_textures);
}

void GLM_EnterBatchedWorldRegion(void)
{
	GLM_CompileDrawWorldProgram();
	GLM_CompileDrawWorldProgramAlphaTested();

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
	glm_brushmodel_drawcall_t* drawcall = &drawcalls[current_drawcall];

	if (index >= sizeof(drawcall->mappings) / sizeof(drawcall->mappings[0])) {
		return false;
	}

	for (i = 0; i < drawcall->material_samplers; ++i) {
		if (R_TextureReferenceEqual(texture->gl_texture_array, drawcall->allocated_samplers[i])) {
			sampler = i;
			break;
		}
	}

	if (sampler < 0) {
		if (drawcall->material_samplers >= material_samplers_max) {
			drawcall = GL_FlushWorldModelBatch();
		}

		sampler = drawcall->material_samplers++;
		drawcall->allocated_samplers[sampler] = texture->gl_texture_array;
	}

	drawcall->mappings[index].samplerIndex = sampler;
	drawcall->mappings[index].arrayIndex = texture->gl_texture_index;
	drawcall->mappings[index].flags = R_TextureReferenceIsValid(texture->fb_texturenum) && texture->isLumaTexture ? EZQ_SURFACE_HAS_LUMA : 0;
	drawcall->mappings[index].flags |= R_TextureReferenceIsValid(texture->fb_texturenum) && !texture->isLumaTexture ? EZQ_SURFACE_HAS_FB : 0;
	drawcall->mappings[index].flags |= R_TextureReferenceIsValid(texture->fb_texturenum) && texture->isAlphaTested ? EZQ_SURFACE_ALPHATEST : 0;
	drawcall->mappings[index].flags |= texture->isLitTurb ? EZQ_SURFACE_LIT_TURB : 0;
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
		caustics &= ((R_ProgramCustomOptions(r_program_brushmodel) & DRAW_CAUSTIC_TEXTURES) == DRAW_CAUSTIC_TEXTURES);
	}

	// See if previous batch has same texture & matrix, if so just continue
	if (drawcall->batch_count) {
		req = &drawcall->worldmodel_requests[drawcall->batch_count - 1];

		if (model == req->model && req->samplerMappingCount == num_textures && req->firstTexture == first_texture && drawcall->batch_count < MAX_WORLDMODEL_BATCH) {
			// Duplicate details from previous request, but with different matrix
			glm_worldmodel_req_t* newreq = &drawcall->worldmodel_requests[drawcall->batch_count];

			memcpy(newreq, req, sizeof(*newreq));
			R_GetModelviewMatrix(newreq->mvMatrix);
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

static glm_worldmodel_req_t* GLM_NextBatchRequest(model_t* model, float alpha, int num_textures, int first_texture, qbool polygonOffset, qbool caustics, qbool allow_duplicate, qbool isAlphaTested)
{
	glm_worldmodel_req_t* req;
	qbool worldmodel = model ? model->isworldmodel : false;
	int flags = (caustics ? EZQ_SURFACE_UNDERWATER : 0);
	glm_brushmodel_drawcall_t* drawcall = &drawcalls[current_drawcall];
	float mvMatrix[16];

	R_GetModelviewMatrix(mvMatrix);

	// If user has switched off caustics (or no texture), ignore
	if (caustics) {
		caustics &= ((R_ProgramCustomOptions(r_program_brushmodel) & DRAW_CAUSTIC_TEXTURES) == DRAW_CAUSTIC_TEXTURES);
	}

	// See if previous batch has same texture & matrix, if so just continue
	if (drawcall->batch_count) {
		req = &drawcall->worldmodel_requests[drawcall->batch_count - 1];

		if (allow_duplicate && model == req->model && req->samplerMappingCount == num_textures && req->firstTexture == first_texture && drawcall->batch_count < MAX_WORLDMODEL_BATCH && isAlphaTested == req->isAlphaTested) {
			// Duplicate details from previous request, but with different matrix
			glm_worldmodel_req_t* newreq = &drawcall->worldmodel_requests[drawcall->batch_count];

			memcpy(newreq, req, sizeof(*newreq));
			R_GetModelviewMatrix(newreq->mvMatrix);
			newreq->alpha = alpha;
			newreq->flags = flags;
			newreq->polygonOffset = polygonOffset;
			newreq->worldmodel = worldmodel;
			newreq->baseInstance = drawcall->batch_count;
			++drawcall->batch_count;

			return NULL;
		}

		// Try and continue the previous batch
		if (worldmodel == req->worldmodel && !memcmp(req->mvMatrix, mvMatrix, sizeof(req->mvMatrix)) && polygonOffset == req->polygonOffset && req->flags == flags && req->isAlphaTested == isAlphaTested) {
			if (num_textures == 0) {
				// We don't care about materials, so can draw with previous batch
				return req;
			}
			else if (req->samplerMappingCount == 0) {
				// Previous block didn't care about materials, allocate now
				if (drawcall->sampler_mappings < MAX_SAMPLER_MAPPINGS) {
					req->samplerMappingBase = drawcall->sampler_mappings - first_texture;
					req->samplerMappingCount = min(num_textures, MAX_SAMPLER_MAPPINGS - drawcall->sampler_mappings);
					req->firstTexture = first_texture;
					drawcall->sampler_mappings += req->samplerMappingCount;
					return req;
				}
			}
		}
	}

	if (drawcall->sampler_mappings >= MAX_SAMPLER_MAPPINGS || drawcall->batch_count >= MAX_WORLDMODEL_BATCH) {
		drawcall = GL_FlushWorldModelBatch();
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
	req->firstTexture = first_texture;
	req->isAlphaTested = isAlphaTested;

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

	alpha = r_refdef2.wateralpha;

	// Waterchain has list of alpha-blended surfaces
	R_TraceEnterNamedRegion(__func__);

	GL_StartWaterSurfaceBatch();
	for (surf = waterchain; surf; surf = surf->texturechain) {
		glpoly_t* poly;
		texture_t* tex = R_TextureAnimation(NULL, surf->texinfo->texture);

		req = GLM_NextBatchRequest(NULL, alpha, 1, surf->texinfo->miptex, false, false, false, tex->isAlphaTested);
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

	R_TraceLeaveNamedRegion();

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

static glm_worldmodel_req_t* GLM_DrawTexturedChain(glm_worldmodel_req_t* req, msurface_t* surf)
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

qbool GLM_CompilePostProcessVAO(void);

qbool GLM_CompileSimple3dProgram(void)
{
	if (R_ProgramRecompileNeeded(r_program_simple3d, 0)) {
		R_ProgramCompile(r_program_simple3d);
	}

	return R_ProgramReady(r_program_simple3d) && GLM_CompilePostProcessVAO();
}

static glm_brushmodel_drawcall_t* GL_FlushWorldModelBatch(void)
{
	const glm_brushmodel_drawcall_t* prev;
	glm_brushmodel_drawcall_t* current;
	int last = current_drawcall++;

	GLM_CheckDrawCallSize();

	prev = &drawcalls[last];
	current = &drawcalls[current_drawcall];

	memset(current, 0, sizeof(*current));
	current->type = prev->type;
	return current;
}

void GL_StartWaterSurfaceBatch(void)
{
	GL_FlushWorldModelBatch();

	drawcalls[current_drawcall].type = alpha_surfaces;
}

void GLM_PrepareWorldModelBatch(void)
{
	GLintptr drawOffset = 0;
	GLintptr indexOffset = buffers.BufferOffset(r_buffer_brushmodel_index_data) / sizeof(modelIndexes[0]);
	int samplerMappingOffset = 0;
	int batchOffset = 0;
	int draw, i;

	R_TraceEnterNamedRegion(__func__);
	buffers.Update(r_buffer_brushmodel_index_data, sizeof(modelIndexes[0]) * index_count, modelIndexes);
	buffers.EnsureSize(r_buffer_brushmodel_drawcall_indirect, sizeof(drawcalls[0].worldmodel_requests) * maximum_drawcalls);
	buffers.EnsureSize(r_buffer_brushmodel_drawcall_data, sizeof(drawcalls[0].calls) * maximum_drawcalls);
	buffers.EnsureSize(r_buffer_brushmodel_worldsamplers_ssbo, sizeof(drawcalls[0].mappings) * maximum_drawcalls);

	buffers.BindRange(r_buffer_brushmodel_drawcall_data, EZQ_GL_BINDINGPOINT_BRUSHMODEL_DRAWDATA, buffers.BufferOffset(r_buffer_brushmodel_drawcall_data), sizeof(drawcalls[0].calls) * maximum_drawcalls);
	buffers.BindRange(r_buffer_brushmodel_worldsamplers_ssbo, EZQ_GL_BINDINGPOINT_BRUSHMODEL_SAMPLERS, buffers.BufferOffset(r_buffer_brushmodel_worldsamplers_ssbo), sizeof(drawcalls[0].mappings) * maximum_drawcalls);

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
		buffers.UpdateSection(r_buffer_brushmodel_drawcall_indirect, drawOffset, sizeof(drawcall->worldmodel_requests[0]) * drawcall->batch_count, &drawcall->worldmodel_requests);
		drawOffset += sizeof(drawcall->worldmodel_requests[0]) * drawcall->batch_count;

		buffers.UpdateSection(r_buffer_brushmodel_drawcall_data, sizeof(drawcall->calls[0]) * batchOffset, sizeof(drawcall->calls[0]) * drawcall->batch_count, &drawcall->calls);
		buffers.UpdateSection(r_buffer_brushmodel_worldsamplers_ssbo, sizeof(drawcall->mappings[0]) * samplerMappingOffset, sizeof(drawcall->mappings[0]) * drawcall->sampler_mappings, &drawcall->mappings);

		batchOffset += drawcall->batch_count;
		samplerMappingOffset += drawcall->sampler_mappings;
	}

	R_TraceLeaveNamedRegion();
}

static void GLM_DrawWorldExecuteCalls(glm_brushmodel_drawcall_t* drawcall, uintptr_t offset, int begin, int count)
{
	int i;
	int prevSampler = -1;
	const qbool batch = false;
	qbool prev_alphaTested = false;

	for (i = begin; i < begin + count; ++i) {
		glm_worldmodel_req_t* req = &drawcall->worldmodel_requests[i];
		int sampler = req->nonDynamicSampler;
		int batchCount = 1;

		if (prevSampler != sampler || req->isAlphaTested != prev_alphaTested) {
			if (req->isAlphaTested) {
				R_ProgramUse(r_program_brushmodel_alphatested);
				R_ProgramUniform1i(r_program_uniform_brushmodel_alphatested_sampler, prevSampler = sampler);
			}
			else {
				R_ProgramUse(r_program_brushmodel);
				R_ProgramUniform1i(r_program_uniform_brushmodel_sampler, prevSampler = sampler);
			}
			prev_alphaTested = req->isAlphaTested;
		}

		if (batch) {
			while (i + batchCount < begin + count && drawcall->worldmodel_requests[i + batchCount].nonDynamicSampler == sampler && drawcall->worldmodel_requests[i + batchCount].isAlphaTested == req->isAlphaTested) {
				++batchCount;
			}
		}

		if (batchCount == 1) {
			GL_DrawElementsInstancedBaseVertexBaseInstance(
				GL_TRIANGLE_STRIP,
				req->count,
				GL_UNSIGNED_INT,
				(void*)(req->firstIndex * sizeof(GLuint)),
				req->instanceCount,
				req->baseVertex,
				req->baseInstance
			);
		}
		else {
			GL_MultiDrawElementsIndirect(GL_TRIANGLE_STRIP, GL_UNSIGNED_INT, (void*)(offset + sizeof(drawcall->worldmodel_requests[0]) * i), batchCount, sizeof(drawcall->worldmodel_requests[0]));

			i += batchCount - 1;
		}
	}
}

void GLM_DrawWorldModelBatch(glm_brushmodel_drawcall_type type)
{
	int draw;
	qbool first = true;
	unsigned int extra_offset = 0;
	qbool alphablended = (type == alpha_surfaces);

	for (draw = 0; draw <= current_drawcall; ++draw) {
		glm_brushmodel_drawcall_t* drawcall = &drawcalls[draw];

		if (!drawcall->batch_count || drawcall->type != type) {
			continue;
		}

		if (first) {
			R_TraceEnterNamedRegion(__func__);
			GL_StartWorldBatch(alphablended);
			buffers.Bind(r_buffer_brushmodel_index_data);
			buffers.Bind(r_buffer_brushmodel_drawcall_indirect);
			extra_offset = buffers.BufferOffset(r_buffer_brushmodel_drawcall_indirect);

			first = false;
		}

		R_TraceEnterNamedRegion(va("WorldModelBatch(%d/%d)", draw, current_drawcall));

		// Bind texture units
		renderer.TextureUnitMultiBind(TEXTURE_UNIT_MATERIAL, drawcall->material_samplers, drawcall->allocated_samplers);

		if (drawcall->polygonOffsetSplit >= 0 && drawcall->polygonOffsetSplit < drawcall->batch_count) {
			uintptr_t normal_offset = drawcall->indirectDrawOffset + extra_offset;
			uintptr_t polygonOffset_offset = (extra_offset + drawcall->indirectDrawOffset + sizeof(drawcall->worldmodel_requests[0]) * drawcall->polygonOffsetSplit);

			if (drawcall->polygonOffsetSplit) {
				GLM_BeginDrawWorld(alphablended, false);
				GLM_DrawWorldExecuteCalls(drawcall, normal_offset, 0, drawcall->polygonOffsetSplit);
			}

			GLM_BeginDrawWorld(alphablended, true);
			GLM_DrawWorldExecuteCalls(drawcall, polygonOffset_offset, drawcall->polygonOffsetSplit, drawcall->batch_count - drawcall->polygonOffsetSplit);
		}
		else {
			GLM_BeginDrawWorld(alphablended, false);
			GLM_DrawWorldExecuteCalls(drawcall, extra_offset + drawcall->indirectDrawOffset, 0, drawcall->batch_count);
		}

		R_TraceLeaveNamedRegion();

		frameStats.subdraw_calls += drawcall->batch_count;
		drawcall->batch_count = 0;
		drawcall->material_samplers = 0;
		drawcall->sampler_mappings = 0;
	}

	if (!first) {
		R_TraceLeaveNamedRegion();
	}
}

void GLM_DrawBrushModel(entity_t* ent, qbool polygonOffset, qbool caustics)
{
	int i;
	glm_worldmodel_req_t* req = NULL;
	model_t* model = ent->model;

	if (GLM_DuplicatePreviousRequest(model, 1.0f, model->last_texture_chained - model->first_texture_chained + 1, model->first_texture_chained, polygonOffset, caustics)) {
		return;
	}

	if (model->drawflat_chain) {
		req = GLM_NextBatchRequest(model, 1.0f, 0, 0, false, false, false, false);

		req = GLM_DrawFlatChain(req, model->drawflat_chain);

		model->drawflat_chain = NULL;
	}

	if (model->last_texture_chained < 0) {
		return;
	}

	for (i = model->first_texture_chained; i <= model->last_texture_chained; ++i) {
		texture_t* tex = model->textures[i];

		if (!tex || !tex->loaded || !tex->texturechain || !R_TextureReferenceIsValid(tex->gl_texture_array)) {
			continue;
		}

		req = GLM_NextBatchRequest(model, 1.0f, 1, i, polygonOffset, caustics, false, tex->isAlphaTested);
		tex = R_TextureAnimation(ent, tex);
		if (!GLM_AssignTexture(i, tex)) {
			req = GLM_NextBatchRequest(model, 1.0f, 1, i, polygonOffset, caustics, false, tex->isAlphaTested);
			GLM_AssignTexture(i, tex);
		}

		req = GLM_DrawTexturedChain(req, model->textures[i]->texturechain);
	}
}

static int GL_DrawCallComparison(const void* lhs_, const void* rhs_)
{
	const glm_worldmodel_req_t* lhs = (glm_worldmodel_req_t*)lhs_;
	const glm_worldmodel_req_t* rhs = (glm_worldmodel_req_t*)rhs_;

	if (lhs->polygonOffset && !rhs->polygonOffset) {
		return 1;
	}
	if (!lhs->polygonOffset && rhs->polygonOffset) {
		return -1;
	}

	if (lhs->isAlphaTested && !rhs->isAlphaTested) {
		return 1;
	}
	else if (!lhs->isAlphaTested && rhs->isAlphaTested) {
		return -1;
	}

	return lhs->nonDynamicSampler - rhs->nonDynamicSampler;
}

static void GL_SortDrawCalls(glm_brushmodel_drawcall_t* drawcall)
{
	int i;

	drawcall->polygonOffsetSplit = drawcall->batch_count;
	if (drawcall->batch_count == 0) {
		return;
	}

	for (i = 0; i < drawcall->batch_count; ++i) {
		glm_worldmodel_req_t* thisReq = &drawcall->worldmodel_requests[i];
		int mappingIndex = bound(0, thisReq->samplerMappingBase + thisReq->firstTexture, sizeof(drawcall->mappings) / sizeof(drawcall->mappings[0]) - 1);
		int sampler = drawcall->mappings[mappingIndex].samplerIndex;

		thisReq->nonDynamicSampler = sampler;

		if (thisReq->polygonOffset && drawcall->polygonOffsetSplit > i) {
			drawcall->polygonOffsetSplit = i;
		}
	}

	qsort(drawcall->worldmodel_requests, drawcall->batch_count, sizeof(drawcall->worldmodel_requests[0]), GL_DrawCallComparison);

	for (i = 0; i < drawcall->batch_count; ++i) {
		glm_worldmodel_req_t* thisReq = &drawcall->worldmodel_requests[i];

		drawcall->calls[i].alpha = thisReq->alpha;
		drawcall->calls[i].flags = thisReq->flags;
		memcpy(drawcall->calls[i].modelMatrix, thisReq->mvMatrix, sizeof(drawcall->calls[i].modelMatrix));
		drawcall->calls[i].samplerBase = thisReq->samplerMappingBase;
		thisReq->baseInstance = i;
	}
}

void GLM_DrawWorld(void)
{
	entity_t ent;

	memset(&ent, 0, sizeof(ent));
	ent.model = cl.worldmodel;

	GLM_EnterBatchedWorldRegion();
	GLM_DrawBrushModel(&ent, false, false);
}

#endif // #ifdef RENDERER_OPTION_MODERN_OPENGL
