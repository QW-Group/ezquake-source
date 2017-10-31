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

static void GL_SortDrawCalls(int* polygonOffsetStart);

glm_worldmodel_req_t worldmodel_requests[MAX_WORLDMODEL_BATCH];

extern buffer_ref brushModel_vbo;
extern GLuint modelIndexes[MAX_WORLDMODEL_INDEXES];

static GLuint index_count;
static qbool uniforms_set = false;
extern texture_ref lightmap_texture_array;

static glm_program_t drawworld;
static GLint drawworld_RefdefCvars_block;
static GLint drawworld_WorldCvars_block;

typedef struct block_world_s {
	float modelMatrix[MAX_WORLDMODEL_BATCH][16];
	float color[MAX_WORLDMODEL_BATCH][4];
	GLint samplerMappings[MAX_WORLDMODEL_BATCH][4];
	GLint flags[MAX_WORLDMODEL_BATCH][4];

	// sky
	float skySpeedscale;
	float skySpeedscale2;
	float r_farclip;

	//
	float waterAlpha;

	// drawflat for solid surfaces
	int r_drawflat;
	int r_fastturb;
	int r_fastsky;

	// 
	int padding1;

	float r_wallcolor[4];  // only used if r_drawflat 1 or 3
	float r_floorcolor[4]; // only used if r_drawflat 1 or 2

	// drawflat for turb surfaces
	float r_telecolor[4];
	float r_lavacolor[4];
	float r_slimecolor[4];
	float r_watercolor[4];

	// drawflat for sky
	float r_skycolor[4];
	int r_texture_luma_fb;
} block_world_t;

static int batch_count = 0;
static buffer_ref vbo_worldIndirectDraw;

#define DRAW_DETAIL_TEXTURES     1
#define DRAW_CAUSTIC_TEXTURES    2
#define DRAW_LUMA_TEXTURES       4
#define DRAW_SKYBOX              8
static buffer_ref ubo_worldcvars;
static block_world_t world;

#define MAXIMUM_MATERIAL_SAMPLERS 32
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
	int drawworld_desiredOptions =
		(detail_textures ? DRAW_DETAIL_TEXTURES : 0) |
		(caustic_textures ? DRAW_CAUSTIC_TEXTURES : 0) |
		(luma_textures ? DRAW_LUMA_TEXTURES : 0) |
		(skybox ? DRAW_SKYBOX : 0);

	if (GLM_ProgramRecompileNeeded(&drawworld, drawworld_desiredOptions)) {
		static char included_definitions[512];
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
		if (luma_textures) {
			strlcat(included_definitions, "#define DRAW_LUMA_TEXTURES\n", sizeof(included_definitions));
		}
		if (skybox) {
			TEXTURE_UNIT_SKYBOX = samplers++;

			strlcat(included_definitions, "#define DRAW_SKYBOX\n", sizeof(included_definitions));
			strlcat(included_definitions, va("#define SAMPLER_SKYBOX_TEXTURE %d\n", TEXTURE_UNIT_SKYBOX), sizeof(included_definitions));
		}
		else {
			TEXTURE_UNIT_SKYDOME_TEXTURE = samplers++;
			TEXTURE_UNIT_SKYDOME_CLOUD_TEXTURE = samplers++;

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

		// Initialise program for drawing image
		GLM_CreateVFProgramWithInclude("DrawWorld", GL_VFParams(drawworld), &drawworld, included_definitions);

		drawworld.custom_options = drawworld_desiredOptions;
	}

	if (drawworld.program && !drawworld.uniforms_found) {
		GLint size;

		drawworld_RefdefCvars_block = glGetUniformBlockIndex(drawworld.program, "RefdefCvars");
		glUniformBlockBinding(drawworld.program, drawworld_RefdefCvars_block, GL_BINDINGPOINT_REFDEF_CVARS);

		drawworld_WorldCvars_block = glGetUniformBlockIndex(drawworld.program, "WorldCvars");
		glGetActiveUniformBlockiv(drawworld.program, drawworld_WorldCvars_block, GL_UNIFORM_BLOCK_DATA_SIZE, &size);

		glUniformBlockBinding(drawworld.program, drawworld_WorldCvars_block, GL_BINDINGPOINT_DRAWWORLD_CVARS);
		ubo_worldcvars = GL_GenUniformBuffer("world-cvars", &world, sizeof(world));
		GL_BindBufferBase(ubo_worldcvars, GL_BINDINGPOINT_DRAWWORLD_CVARS);

		drawworld.uniforms_found = true;
	}

	if (!GL_BufferReferenceIsValid(vbo_worldIndirectDraw)) {
		vbo_worldIndirectDraw = GL_GenFixedBuffer(GL_DRAW_INDIRECT_BUFFER, "world-indirect", sizeof(worldmodel_requests), NULL, GL_STREAM_DRAW);
	}
}

#define PASS_COLOR_AS_4F(target, cvar) \
{ \
	target[0] = (cvar.color[0] * 1.0f / 255); \
	target[1] = (cvar.color[1] * 1.0f / 255); \
	target[2] = (cvar.color[2] * 1.0f / 255); \
	target[3] = 1.0f; \
}

static void GLM_EnterBatchedWorldRegion(qbool detail_tex, qbool caustics, qbool lumas)
{
	extern glm_vao_t brushModel_vao;
	extern buffer_ref vbo_brushElements;
	extern cvar_t gl_lumaTextures;

	texture_ref std_textures[MAX_STANDARD_TEXTURES];
	int std_count = 0;

	float wateralpha = bound((1 - r_refdef2.max_watervis), r_wateralpha.value, 1);
	qbool skybox = r_skyboxloaded && !r_fastsky.integer;

	extern cvar_t r_telecolor, r_lavacolor, r_slimecolor, r_watercolor, r_fastturb, r_skycolor;

	Compile_DrawWorldProgram(detail_tex, caustics, lumas, skybox);

	world.r_farclip = max(r_farclip.value, 4096) * 0.577;
	world.skySpeedscale = r_refdef2.time * 8;
	world.skySpeedscale -= (int)world.skySpeedscale & ~127;
	world.skySpeedscale2 = r_refdef2.time * 16;
	world.skySpeedscale2 -= (int)world.skySpeedscale2 & ~127;

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

	world.r_texture_luma_fb = gl_fb_bmodels.integer ? 1 : 0;

	GL_UseProgram(drawworld.program);

	GL_BindVertexArray(&brushModel_vao);

	// Bind standard textures (warning: these must be in the same order)
	std_textures[TEXTURE_UNIT_LIGHTMAPS] = lightmap_texture_array;
	if (detail_tex) {
		std_textures[TEXTURE_UNIT_DETAIL] = detailtexture;
	}
	if (caustics) {
		std_textures[TEXTURE_UNIT_CAUSTICS] = underwatertexture;
	}
	if (skybox) {
		extern texture_ref skybox_cubeMap;

		std_textures[TEXTURE_UNIT_SKYBOX] = skybox_cubeMap;
	}
	else {
		extern texture_ref solidskytexture, alphaskytexture;

		std_textures[TEXTURE_UNIT_SKYDOME_TEXTURE] = solidskytexture;
		std_textures[TEXTURE_UNIT_SKYDOME_CLOUD_TEXTURE] = alphaskytexture;
	}

	GL_BindTextures(0, TEXTURE_UNIT_MATERIAL, std_textures);

	material_samplers = 0;
}

void GLM_ExitBatchedPolyRegion(void)
{
	//uniforms_set = false;

	//Cvar_SetValue(&developer, 0);
}

void GL_FlushWorldModelBatch(void);

// TODO: process polygonOffset
static glm_worldmodel_req_t* GLM_NextBatchRequest(model_t* model, float* base_color, texture_ref texture_array, qbool polygonOffset, qbool caustics)
{
	glm_worldmodel_req_t* req;
	int sampler = -1;

	// If user has switched off caustics (or no texture), ignore
	if (caustics) {
		caustics &= ((drawworld.custom_options & DRAW_CAUSTIC_TEXTURES) == DRAW_CAUSTIC_TEXTURES);
	}

	// See if previous batch has same texture & matrix, if so just continue
	if (batch_count) {
		float mvMatrix[16];

		if (batch_count == 1) {
			batch_count = batch_count;
		}

		req = &worldmodel_requests[batch_count - 1];
		GLM_GetMatrix(GL_MODELVIEW, mvMatrix);
		if (!memcmp(mvMatrix, req->mvMatrix, sizeof(mvMatrix))) {
			if (!GL_TextureReferenceIsValid(texture_array)) {
				// We don't care about materials, so no problem here...
				return req;
			}

			// Previous block didn't care about materials, pick now
			if (req->sampler < 0) {
				int i;

				for (i = 0; i < material_samplers; ++i) {
					if (GL_TextureReferenceEqual(texture_array, allocated_samplers[i])) {
						req->sampler = i;
						break;
					}
				}
				if (req->sampler < 0 && material_samplers < material_samplers_max) {
					req->sampler = material_samplers++;
					allocated_samplers[req->sampler] = texture_array;
				}
			}

			if (GL_TextureReferenceEqual(allocated_samplers[req->sampler], texture_array)) {
				return req;
			}
		}
	}

	if (batch_count >= MAX_WORLDMODEL_BATCH) {
		GL_FlushWorldModelBatch();
	}

	if (!GL_TextureReferenceIsValid(texture_array)) {
		sampler = -1;
	}
	else {
		int i;
		for (i = 0; i < material_samplers; ++i) {
			if (GL_TextureReferenceEqual(texture_array, allocated_samplers[i])) {
				sampler = i;
				break;
			}
		}

		if (sampler < 0) {
			if (material_samplers >= material_samplers_max) {
				GL_FlushWorldModelBatch();
			}

			sampler = material_samplers++;
			allocated_samplers[sampler] = texture_array;
		}
	}

	req = &worldmodel_requests[batch_count];

	GLM_GetMatrix(GL_MODELVIEW, req->mvMatrix);
	if (base_color) {
		memcpy(&req->color, base_color, sizeof(req->color));
	}
	else {
		req->color[0] = req->color[1] = req->color[2] = req->color[3] = 1.0f;
	}
	req->sampler = sampler;
	req->flags = (caustics ? EZQ_SURFACE_UNDERWATER : 0);
	req->polygonOffset = polygonOffset;

	req->count = 0;
	req->instanceCount = 1;
	req->firstIndex = index_count;
	req->baseVertex = 0;
	req->baseInstance = batch_count;

	++batch_count;
	return req;
}

void GLM_DrawTexturedWorld(model_t* model)
{
	extern cvar_t gl_lumaTextures;

	int i, waterline, v;
	msurface_t* surf;
	qbool draw_detail_texture = gl_detail.integer && GL_TextureReferenceIsValid(detailtexture);
	qbool draw_caustics = gl_caustics.integer && GL_TextureReferenceIsValid(underwatertexture);
	qbool draw_lumas = gl_lumaTextures.integer && r_refdef2.allow_lumas;
	glm_worldmodel_req_t* req = NULL;

	GLM_EnterBatchedWorldRegion(draw_detail_texture, draw_caustics, draw_lumas);

	for (waterline = 0; waterline < 2; waterline++) {
		for (surf = model->drawflat_chain[waterline]; surf; surf = surf->drawflatchain) {
			glpoly_t* poly;

			req = GLM_NextBatchRequest(model, NULL, null_texture_reference, false, false);
			for (poly = surf->polys; poly; poly = poly->next) {
				int newVerts = poly->numverts;

				if (index_count + 1 + newVerts > sizeof(modelIndexes) / sizeof(modelIndexes[0])) {
					GL_FlushWorldModelBatch();
					req = GLM_NextBatchRequest(model, NULL, null_texture_reference, false, false);
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

	for (i = 0; i < model->texture_array_count; ++i) {
		texture_t* base_tex = model->textures[model->texture_array_first[i]];
		qbool first_in_this_array = true;
		int texIndex;

		if (!base_tex || !base_tex->size_start || !GL_TextureReferenceIsValid(base_tex->gl_texture_array)) {
			continue;
		}

		for (texIndex = model->texture_array_first[i]; texIndex >= 0 && texIndex < model->numtextures; texIndex = model->textures[texIndex]->next_same_size) {
			texture_t* tex = model->textures[texIndex];

			if (!tex->texturechain[0] && !tex->texturechain[1]) {
				continue;
			}

			req = GLM_NextBatchRequest(model, NULL, tex->gl_texture_array, false, false);
			for (waterline = 0; waterline < 2; waterline++) {
				for (surf = tex->texturechain[waterline]; surf; surf = surf->texturechain) {
					glpoly_t* poly;

					for (poly = surf->polys; poly; poly = poly->next) {
						int newVerts = poly->numverts;

						if (index_count + 1 + newVerts > sizeof(modelIndexes) / sizeof(modelIndexes[0])) {
							GL_FlushWorldModelBatch();
							req = GLM_NextBatchRequest(model, NULL, tex->gl_texture_array, false, false);
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

	return;
}

void GL_FlushWorldModelBatch(void)
{
	GLuint last_vao = 0;
	GLuint last_array = 0;
	qbool was_worldmodel = 0;
	int draw_pos = 0;
	int polygonOffsetStart = 0;
	extern buffer_ref vbo_brushElements;

	if (!batch_count) {
		return;
	}

	GL_SortDrawCalls(&polygonOffsetStart);

	GL_UseProgram(drawworld.program);
	GL_UpdateVBO(ubo_worldcvars, sizeof(world), &world);
	GL_UpdateVBO(vbo_brushElements, sizeof(modelIndexes[0]) * index_count, modelIndexes);
	GL_UpdateVBO(vbo_worldIndirectDraw, sizeof(worldmodel_requests[0]) * batch_count, &worldmodel_requests);

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

	batch_count = 0;
	index_count = 0;
	material_samplers = 0;
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

	for (i = 0; i < model->texture_array_count; ++i) {
		texture_t* base_tex = model->textures[model->texture_array_first[i]];
		int texIndex;

		if (!base_tex || !base_tex->size_start) {
			continue;
		}

		for (texIndex = model->texture_array_first[i]; texIndex >= 0 && texIndex < model->numtextures; texIndex = model->textures[texIndex]->next_same_size) {
			texture_t* tex = model->textures[texIndex];

			if (!tex->texturechain[0] && !tex->texturechain[1]) {
				continue;
			}

			req = GLM_NextBatchRequest(model, base_color, tex->gl_texture_array, polygonOffset, caustics);
			for (waterline = 0; waterline < 2; waterline++) {
				for (surf = tex->texturechain[waterline]; surf; surf = surf->texturechain) {
					glpoly_t* poly;

					for (poly = surf->polys; poly; poly = poly->next) {
						int newVerts = poly->numverts;

						if (index_count + 1 + newVerts > sizeof(modelIndexes) / sizeof(modelIndexes[0])) {
							GL_FlushWorldModelBatch();
							req = GLM_NextBatchRequest(model, base_color, tex->gl_texture_array, polygonOffset, caustics);
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

	return;
}
static void GL_SortDrawCalls(int* polygonOffsetStart)
{
	static GLuint newModelIndexes[MAX_WORLDMODEL_INDEXES];
	int i;
	int back;
	int index = 0;
	int overall_index_count = index_count;

	*polygonOffsetStart = batch_count;
	if (batch_count == 0) {
		return;
	}

	if (true) {
		// All polygon offset calls need to be moved to the back
		back = batch_count - 1;
		while (back >= 0 && worldmodel_requests[back].polygonOffset) {
			*polygonOffsetStart = back;
			--back;
		}

		for (i = 0; i < batch_count; ++i) {
			glm_worldmodel_req_t* this = &worldmodel_requests[i];

			if (this->polygonOffset && i < back) {
				glm_worldmodel_req_t copy = worldmodel_requests[back];
				worldmodel_requests[back] = *this;
				*this = copy;

				while (back >= 0 && worldmodel_requests[back].polygonOffset) {
					*polygonOffsetStart = back;
					--back;
				}
			}
		}
	}

	for (i = 0; i < batch_count; ++i) {
		glm_worldmodel_req_t* this = &worldmodel_requests[i];

		memcpy(world.color[i], this->color, sizeof(world.color[0]));
		memcpy(world.modelMatrix[i], this->mvMatrix, sizeof(world.modelMatrix[0]));
		world.flags[i][0] = this->flags;
		world.samplerMappings[i][0] = this->sampler;
	}
}
