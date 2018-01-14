
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"
#include "utils.h"

#define VBO_VERT_FOFS(x) (void*)((intptr_t)&(((vbo_world_vert_t*)0)->x))

static glm_vbo_t brushModel_vbo;
glm_vao_t brushModel_vao;
static GLuint modelIndexes[16 * 1024];
static GLuint index_count;

typedef struct block_brushmodels_s {
	int apply_lightmap[32][4];
	float color[32][4];
	float modelMatrix[32][16];
} block_brushmodels_t;

static glm_program_t drawBrushModelProgram;
static GLuint drawbrushmodel_RefdefCvars_block;
static GLuint drawbrushmodel_BrushData_block;
static glm_ubo_t ubo_brushdata;
static glm_vbo_t vbo_elements;
static block_brushmodels_t brushmodels;
glm_vbo_t vbo_brushIndirectDraw;

void GLM_CreateBrushModelProgram(void)
{
	if (!drawBrushModelProgram.program) {
		GL_VFDeclare(model_brush);

		GLM_CreateVFProgram("BrushModel", GL_VFParams(model_brush), &drawBrushModelProgram);
	}

	if (drawBrushModelProgram.program && !drawBrushModelProgram.uniforms_found) {
		GLint size;

		drawbrushmodel_RefdefCvars_block = glGetUniformBlockIndex(drawBrushModelProgram.program, "RefdefCvars");
		drawbrushmodel_BrushData_block = glGetUniformBlockIndex(drawBrushModelProgram.program, "ModelData");

		glGetActiveUniformBlockiv(drawBrushModelProgram.program, drawbrushmodel_BrushData_block, GL_UNIFORM_BLOCK_DATA_SIZE, &size);

		glUniformBlockBinding(drawBrushModelProgram.program, drawbrushmodel_RefdefCvars_block, GL_BINDINGPOINT_REFDEF_CVARS);
		glUniformBlockBinding(drawBrushModelProgram.program, drawbrushmodel_BrushData_block, GL_BINDINGPOINT_BRUSHMODEL_CVARS);

		GL_GenUniformBuffer(&ubo_brushdata, "brush-data", &brushmodels, sizeof(brushmodels));
		glBindBufferBase(GL_UNIFORM_BUFFER, GL_BINDINGPOINT_BRUSHMODEL_CVARS, ubo_brushdata.ubo);

		drawBrushModelProgram.uniforms_found = true;
	}
}

// Sets tex->next_same_size to link up all textures of common size
int R_ChainTexturesBySize(model_t* m)
{
	texture_t* tx;
	int i, j;
	int num_sizes = 0;

	// Initialise chain
	for (i = 0; i < m->numtextures; ++i) {
		tx = m->textures[i];
		if (!tx || !tx->loaded) {
			continue;
		}

		tx->next_same_size = -1;
		tx->size_start = false;
	}

	for (i = 0; i < m->numtextures; ++i) {
		tx = m->textures[i];
		if (!tx || !tx->loaded || tx->next_same_size >= 0 || !tx->gl_width || !tx->gl_height || !tx->gl_texturenum) {
			continue; // not loaded or already processed
		}

		++num_sizes;
		tx->size_start = true;
		for (j = i + 1; j < m->numtextures; ++j) {
			texture_t* next = m->textures[j];
			if (!next || !next->loaded || next->next_same_size >= 0) {
				continue; // not loaded or already processed
			}

			if (tx->gl_width == next->gl_width && tx->gl_height == next->gl_height) {
				tx->next_same_size = j;
				tx = next;
				next->next_same_size = m->numtextures;
			}
		}
	}

	return num_sizes;
}

// 'source' is from GLC's float[VERTEXSIZE]
static int CopyVertToBuffer(vbo_world_vert_t* vbo_buffer, int position, float* source, int lightmap, int material, float scaleS, float scaleT, msurface_t* surf)
{
	vbo_world_vert_t* target = vbo_buffer + position;

	VectorCopy(source, target->position);
	target->material_coords[0] = source[3];
	target->material_coords[1] = source[4];
	target->lightmap_coords[0] = source[5] * 65535;
	target->lightmap_coords[1] = source[6] * 65535;
	target->detail_coords[0] = source[7];
	target->detail_coords[1] = source[8];
	if (scaleS) {
		target->material_coords[0] *= scaleS;
	}
	if (scaleT) {
		target->material_coords[1] *= scaleT;
	}
	target->lightmap_index = lightmap;
	target->material_index = material;

	if (surf->flags & SURF_DRAWSKY) {
		target->flags = TEXTURE_TURB_SKY;
	}
	else {
		target->flags = surf->texinfo->texture->turbType & EZQ_SURFACE_TYPE;
	}

	target->flags |=
		(surf->flags & SURF_UNDERWATER ? EZQ_SURFACE_UNDERWATER : 0) +
		(surf->flags & SURF_DRAWFLAT_FLOOR ? EZQ_SURFACE_IS_FLOOR : 0);
	memcpy(target->flatcolor, &surf->texinfo->texture->flatcolor3ub, sizeof(target->flatcolor));

	return position + 1;
}

int GLM_MeasureVBOSizeForBrushModel(model_t* m)
{
	int j, total_surf_verts = 0, total_surfaces = 0;

	for (j = 0; j < m->numsurfaces; ++j) {
		msurface_t* surf = m->surfaces + j;
		glpoly_t* poly;

		if (!(surf->flags & (SURF_DRAWTURB | SURF_DRAWSKY))) {
			if (surf->texinfo->flags & TEX_SPECIAL) {
				continue;
			}
		}
		if (!m->textures[surf->texinfo->miptex]) {
			continue;
		}

		for (poly = surf->polys; poly; poly = poly->next) {
			total_surf_verts += poly->numverts;
			++total_surfaces;
		}
	}

	if (total_surf_verts <= 0 || total_surfaces < 1) {
		return 0;
	}

	return (total_surf_verts);// +2 * (total_surfaces - 1));
}

// Create VBO, ordering by texture array
int GLM_PopulateVBOForBrushModel(model_t* m, vbo_world_vert_t* vbo_buffer, int vbo_pos)
{
	int i, j;
	int combinations = 0;
	int original_pos = vbo_pos;

	// Clear lightmap data, we don't use it
	for (i = 0; i < m->numtextures; ++i) {
		if (m->textures[i]) {
			memset(m->textures[i]->gl_vbo_length, 0, sizeof(m->textures[i]->gl_vbo_length));
			memset(m->textures[i]->gl_next_lightmap, 0, sizeof(m->textures[i]->gl_next_lightmap));
			m->textures[i]->gl_first_lightmap = -1;
			for (j = 0; j < MAX_LIGHTMAPS; ++j) {
				m->textures[i]->gl_next_lightmap[j] = -1;
			}
		}
	}

	// Order vertices in the VBO by texture & lightmap
	for (i = 0; i < m->numtextures; ++i) {
		int lightmap = -1;
		int length = 0;
		int surface_count = 0;
		int tex_vbo_start = vbo_pos;

		if (!m->textures[i]) {
			continue;
		}

		for (j = 0; j < m->numsurfaces; ++j) {
			msurface_t* surf = m->surfaces + j;
			int lightmap = surf->flags & (SURF_DRAWTURB | SURF_DRAWSKY) ? -1 : surf->lightmaptexturenum;
			glpoly_t* poly;

			if (surf->texinfo->miptex != i) {
				continue;
			}

			// copy verts into buffer (alternate to turn fan into triangle strip)
			for (poly = surf->polys; poly; poly = poly->next) {
				int end_vert = 0;
				int start_vert = 1;
				int output = 0;
				int material = m->textures[i]->gl_texture_index;
				float scaleS = m->textures[i]->gl_texture_scaleS;
				float scaleT = m->textures[i]->gl_texture_scaleT;

				if (!poly->numverts) {
					continue;
				}

				// Store position for drawing individual polys
				poly->vbo_start = vbo_pos;
				vbo_pos = CopyVertToBuffer(vbo_buffer, vbo_pos, poly->verts[0], lightmap, material, scaleS, scaleT, surf);
				++output;

				start_vert = 1;
				end_vert = poly->numverts - 1;

				while (start_vert <= end_vert) {
					vbo_pos = CopyVertToBuffer(vbo_buffer, vbo_pos, poly->verts[start_vert], lightmap, material, scaleS, scaleT, surf);
					++output;

					if (start_vert < end_vert) {
						vbo_pos = CopyVertToBuffer(vbo_buffer, vbo_pos, poly->verts[end_vert], lightmap, material, scaleS, scaleT, surf);
						++output;
					}

					++start_vert;
					--end_vert;
				}

				length += poly->numverts;
				++surface_count;
			}
		}
	}

	return vbo_pos;
}





















typedef struct glm_brushmodel_req_s {
	// This is DrawElementsIndirectCmd, from OpenGL spec
	GLuint count;           // Number of indexes to pull
	GLuint instanceCount;   // Always 1... ?
	GLuint firstIndex;      // Position of first index in array
	GLuint baseVertex;      // Offset of vertices in VBO
	GLuint baseInstance;    // We use this to pull from array of uniforms in shader

	float mvMatrix[16];
	float baseColor[4];
	qbool applyLightmap;
	qbool polygonOffset;    // whether or not to apply polygon offset before rendering
	GLuint vbo_count;       // Number of verts to draw

	GLuint vao;
	GLuint texture_array;
	int texture_index;
	qbool isworldmodel;

	int texture_model;
	int effects;
} glm_brushmodel_req_t;

#define MAX_BRUSHMODEL_BATCH 32
static glm_brushmodel_req_t brushmodel_requests[MAX_BRUSHMODEL_BATCH];
static int batch_count = 0;
static GLuint prev_texture_array = 0;
static qbool in_batch_mode = false;
static qbool firstBrushModel = true;

void GL_BrushModelInitState(void)
{
	if (GL_ShadersSupported()) {
		GL_EnterRegion("BrushModels");
		GLM_CreateBrushModelProgram();

		GL_AlphaBlendFlags(GL_BLEND_DISABLED);
		GL_UseProgram(drawBrushModelProgram.program);

		GL_SelectTexture(GL_TEXTURE0);
	}
	else {
		// FIXME: Why is classic code in GLM-only module?
		GL_EnableTMU(GL_TEXTURE0);
	}

	firstBrushModel = false;
}

void GL_BeginDrawBrushModels(void)
{
	firstBrushModel = true;
}

static int GL_BatchRequestSorter(const void* lhs_, const void* rhs_)
{
	const glm_brushmodel_req_t* lhs = (glm_brushmodel_req_t*)lhs_;
	const glm_brushmodel_req_t* rhs = (glm_brushmodel_req_t*)rhs_;

	// Sort by VAO first
	if (lhs->vao < rhs->vao) {
		return -1;
	}
	else if (lhs->vao > rhs->vao) {
		return 1;
	}

	// Then by texture array
	if (lhs->texture_array < rhs->texture_array) {
		return -1;
	}
	else if (lhs->texture_array > rhs->texture_array) {
		return 1;
	}
	return 0;
}

static void GL_FlushBrushModelBatch(void)
{
	int i;
	GLuint last_vao = 0;
	GLuint last_array = 0;
	qbool was_worldmodel = 0;
	GLuint prevVAO = 0;
	qbool polygonOffset;
	extern cvar_t gl_brush_polygonoffset;

	if (!batch_count) {
		return;
	}

	memset(&brushmodels, 0, sizeof(brushmodels));
	if (firstBrushModel) {
		GL_BrushModelInitState();
	}

	qsort(brushmodel_requests, batch_count, sizeof(brushmodel_requests[0]), GL_BatchRequestSorter);

	for (i = 0; i < batch_count; ++i) {
		glm_brushmodel_req_t* req = &brushmodel_requests[i];

		req->baseInstance = i;
		memcpy(&brushmodels.modelMatrix[i][0], req->mvMatrix, sizeof(brushmodels.modelMatrix[i]));
		memcpy(&brushmodels.color[i][0], req->baseColor, sizeof(brushmodels.color[i]));
		brushmodels.apply_lightmap[i][0] = req->isworldmodel ? 1 : 0;
	}

	// Update data
	GL_UpdateUBO(&ubo_brushdata, sizeof(brushmodels), &brushmodels);
	GL_UpdateVBO(&vbo_brushIndirectDraw, sizeof(brushmodel_requests), &brushmodel_requests);

	GL_BindVertexArray(&brushModel_vao);
	GL_BufferDataUpdate(GL_ELEMENT_ARRAY_BUFFER, sizeof(modelIndexes[0]) * index_count, modelIndexes);

	polygonOffset = brushmodel_requests[0].polygonOffset;
	GL_PolygonOffset(polygonOffset ? POLYGONOFFSET_STANDARD : POLYGONOFFSET_DISABLED);
	for (i = 0; i < batch_count; ++i) {
		int last;
		GLuint texArray = brushmodel_requests[i].texture_array;
		polygonOffset = brushmodel_requests[i].polygonOffset;

		for (last = i; last < batch_count - 1; ++last) {
			int next = brushmodel_requests[last + 1].texture_array;
			qbool next_polygonOffset = brushmodel_requests[last + 1].polygonOffset;

			if (polygonOffset != next_polygonOffset || (next != 0 && texArray != 0 && next != texArray)) {
				break;
			}
			texArray = next;
		}

		if (texArray) {
			GL_BindTexture(GL_TEXTURE_2D_ARRAY, texArray, true);
		}
		GL_PolygonOffset(brushmodel_requests[i].polygonOffset ? POLYGONOFFSET_STANDARD : POLYGONOFFSET_DISABLED);

		glMultiDrawElementsIndirect(
			GL_TRIANGLE_STRIP,
			GL_UNSIGNED_INT,
			(void*)(i * sizeof(brushmodel_requests[0])),
			last - i + 1,
			sizeof(brushmodel_requests[0])
		);

		i = last;
	}

	batch_count = 0;
	index_count = 0;
}

void GL_EndDrawBrushModels(void)
{
	if (GL_ShadersSupported()) {
		GL_FlushBrushModelBatch();

		if (!firstBrushModel) {
			GL_PolygonOffset(POLYGONOFFSET_DISABLED);

			GL_LeaveRegion();
		}
	}
}

static glm_brushmodel_req_t* GLM_NextBatchRequest(model_t* model, float* base_color, GLuint texture_array, qbool polygonOffset)
{
	glm_brushmodel_req_t* req;

	if (batch_count >= MAX_BRUSHMODEL_BATCH) {
		GL_FlushBrushModelBatch();
	}

	req = &brushmodel_requests[batch_count];

	GLM_GetMatrix(GL_MODELVIEW, req->mvMatrix);
	memcpy(req->baseColor, base_color, sizeof(req->baseColor));
	req->isworldmodel = model->isworldmodel;
	req->vao = brushModel_vao.vao;
	req->count = 0;
	req->texture_array = texture_array;
	req->instanceCount = 1;
	req->firstIndex = index_count;
	req->baseVertex = 0;
	req->baseInstance = batch_count;
	req->polygonOffset = polygonOffset;

	++batch_count;
	return req;
}

void GLM_DrawBrushModel(model_t* model, qbool polygonOffset)
{
	int i, waterline, v;
	msurface_t* surf;
	float base_color[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glm_brushmodel_req_t* req = NULL;

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

			req = GLM_NextBatchRequest(model, base_color, tex->gl_texture_array, polygonOffset);
			for (waterline = 0; waterline < 2; waterline++) {
				for (surf = tex->texturechain[waterline]; surf; surf = surf->texturechain) {
					glpoly_t* poly;

					for (poly = surf->polys; poly; poly = poly->next) {
						int newVerts = poly->numverts;

						if (index_count + 1 + newVerts > sizeof(modelIndexes) / sizeof(modelIndexes[0])) {
							GL_FlushBrushModelBatch();
							req = GLM_NextBatchRequest(model, base_color, tex->gl_texture_array, polygonOffset);
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

void GLM_CreateBrushModelVAO(glm_vbo_t* instance_vbo)
{
	int i;
	int size = 0;
	int position = 0;
	vbo_world_vert_t* buffer = NULL;

	for (i = 1; i < MAX_MODELS; ++i) {
		model_t* mod = cl.model_precache[i];
		if (mod && mod->type == mod_brush) {
			size += GLM_MeasureVBOSizeForBrushModel(mod);
		}
	}

	for (i = 0; i < MAX_VWEP_MODELS; i++) {
		model_t* mod = cl.vw_model_precache[i];
		if (mod && mod->type == mod_brush) {
			if (mod == cl.worldmodel || !mod->isworldmodel) {
				size += GLM_MeasureVBOSizeForBrushModel(mod);
			}
		}
	}

	// Create vbo buffer
	buffer = Q_malloc(size * sizeof(vbo_world_vert_t));

	// Create vao
	GL_GenVertexArray(&brushModel_vao);

	GL_GenFixedBuffer(&vbo_elements, GL_ELEMENT_ARRAY_BUFFER, "brushmodel-elements", sizeof(modelIndexes), modelIndexes, GL_STREAM_DRAW);
	GL_GenFixedBuffer(&vbo_brushIndirectDraw, GL_DRAW_INDIRECT_BUFFER, "indirect-draw", sizeof(brushmodel_requests), brushmodel_requests, GL_STREAM_DRAW);

	// Copy data into buffer
	for (i = 1; i < MAX_MODELS; ++i) {
		model_t* mod = cl.model_precache[i];
		if (mod && mod->type == mod_brush) {
			if (mod == cl.worldmodel || !mod->isworldmodel) {
				position = GLM_PopulateVBOForBrushModel(mod, buffer, position);
			}
		}
	}

	for (i = 0; i < MAX_VWEP_MODELS; i++) {
		model_t* mod = cl.vw_model_precache[i];
		if (mod && mod->type == mod_brush) {
			position = GLM_PopulateVBOForBrushModel(mod, buffer, position);
		}
	}

	// Copy VBO buffer across
	GL_GenFixedBuffer(&brushModel_vbo, GL_ARRAY_BUFFER, __FUNCTION__, size * sizeof(vbo_world_vert_t), buffer, GL_STATIC_DRAW);

	GL_ConfigureVertexAttribPointer(&brushModel_vao, &brushModel_vbo, 0, 3, GL_FLOAT, GL_FALSE, sizeof(vbo_world_vert_t), VBO_VERT_FOFS(position));
	GL_ConfigureVertexAttribPointer(&brushModel_vao, &brushModel_vbo, 1, 2, GL_FLOAT, GL_FALSE, sizeof(vbo_world_vert_t), VBO_VERT_FOFS(material_coords));
	GL_ConfigureVertexAttribPointer(&brushModel_vao, &brushModel_vbo, 2, 2, GL_UNSIGNED_SHORT, GL_TRUE, sizeof(vbo_world_vert_t), VBO_VERT_FOFS(lightmap_coords));
	GL_ConfigureVertexAttribPointer(&brushModel_vao, &brushModel_vbo, 3, 2, GL_FLOAT, GL_FALSE, sizeof(vbo_world_vert_t), VBO_VERT_FOFS(detail_coords));
	GL_ConfigureVertexAttribIPointer(&brushModel_vao, &brushModel_vbo, 4, 1, GL_SHORT, sizeof(vbo_world_vert_t), VBO_VERT_FOFS(lightmap_index));
	GL_ConfigureVertexAttribIPointer(&brushModel_vao, &brushModel_vbo, 5, 1, GL_SHORT, sizeof(vbo_world_vert_t), VBO_VERT_FOFS(material_index));
	// 
	GL_ConfigureVertexAttribIPointer(&brushModel_vao, instance_vbo, 6, 1, GL_UNSIGNED_INT, sizeof(GLuint), 0);
	GL_ConfigureVertexAttribIPointer(&brushModel_vao, &brushModel_vbo, 7, 1, GL_UNSIGNED_BYTE, sizeof(vbo_world_vert_t), VBO_VERT_FOFS(flags));
	GL_ConfigureVertexAttribIPointer(&brushModel_vao, &brushModel_vbo, 8, 3, GL_UNSIGNED_BYTE, sizeof(vbo_world_vert_t), VBO_VERT_FOFS(flatcolor));

	glVertexAttribDivisor(6, 1);

	Q_free(buffer);
}
