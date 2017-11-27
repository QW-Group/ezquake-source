
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"
#include "utils.h"

static glm_program_t drawBrushModelProgram;
static GLint drawBrushModel_modelViewMatrix;
static GLint drawBrushModel_projectionMatrix;
static GLint drawBrushModel_color;
static GLint drawBrushModel_materialTex;
static GLint drawBrushModel_lightmapTex;
static GLint drawBrushModel_applyLightmap;
static GLint drawBrushModel_applyTexture;

void GLM_CreateBrushModelProgram(void)
{
	if (!drawBrushModelProgram.program) {
		GL_VFDeclare(model_brush);

		GLM_CreateVFProgram("BrushModel", GL_VFParams(model_brush), &drawBrushModelProgram);
	}

	if (drawBrushModelProgram.program && !drawBrushModelProgram.uniforms_found) {
		drawBrushModel_modelViewMatrix = glGetUniformLocation(drawBrushModelProgram.program, "modelViewMatrix");
		drawBrushModel_projectionMatrix = glGetUniformLocation(drawBrushModelProgram.program, "projectionMatrix");
		drawBrushModel_color = glGetUniformLocation(drawBrushModelProgram.program, "color");
		drawBrushModel_materialTex = glGetUniformLocation(drawBrushModelProgram.program, "materialTex");
		drawBrushModel_lightmapTex = glGetUniformLocation(drawBrushModelProgram.program, "lightmapTex");
		drawBrushModel_applyTexture = glGetUniformLocation(drawBrushModelProgram.program, "apply_texture");
		drawBrushModel_applyLightmap = glGetUniformLocation(drawBrushModelProgram.program, "apply_lightmap");
		drawBrushModelProgram.uniforms_found = true;

		glProgramUniform1i(drawBrushModelProgram.program, drawBrushModel_materialTex, 0);
		glProgramUniform1i(drawBrushModelProgram.program, drawBrushModel_lightmapTex, 2);
		glProgramUniform1i(drawBrushModelProgram.program, drawBrushModel_applyTexture, 1);
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

static int CopyVertToBuffer(float* target, int position, float* source, int lightmap, int material, float scaleS, float scaleT)
{
	memcpy(&target[position], source, sizeof(float) * VERTEXSIZE);
	if (scaleS) {
		target[position + 3] *= scaleS;
	}
	if (scaleT) {
		target[position + 4] *= scaleT;
	}
	target[position + 9] = lightmap;
	target[position + 10] = material;

	return position + VERTEXSIZE;
}

static int DuplicateVertex(float* target, int position)
{
	memcpy(&target[position], &target[position - VERTEXSIZE], sizeof(float) * VERTEXSIZE);

	return position + VERTEXSIZE;
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

// This populates VBO, splitting up by lightmap for efficient
//   rendering when not using texture arrays
int GLC_PopulateVBOForBrushModel(model_t* m, float* vbo_buffer, int vbo_pos)
{
	int i, j;
	int combinations = 0;
	int original_pos = vbo_pos;

	for (i = 0; i < m->numtextures; ++i) {
		if (m->textures[i]) {
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

		// Find first lightmap for this texture
		for (j = 0; j < m->numsurfaces; ++j) {
			msurface_t* surf = m->surfaces + j;

			if (surf->texinfo->miptex != i) {
				continue;
			}

			if (!(surf->flags & (SURF_DRAWTURB | SURF_DRAWSKY))) {
				if (surf->texinfo->flags & TEX_SPECIAL) {
					continue;
				}
			}

			if (surf->lightmaptexturenum >= 0 && (lightmap < 0 || surf->lightmaptexturenum < lightmap)) {
				lightmap = surf->lightmaptexturenum;
			}
		}

		m->textures[i]->gl_first_lightmap = lightmap;

		// Build the VBO in order of lightmaps...
		while (lightmap >= 0) {
			int next_lightmap = -1;

			length = 0;
			m->textures[i]->gl_vbo_start[lightmap] = vbo_pos / VERTEXSIZE;
			++combinations;

			for (j = 0; j < m->numsurfaces; ++j) {
				msurface_t* surf = m->surfaces + j;
				glpoly_t* poly;

				if (surf->texinfo->miptex != i) {
					continue;
				}
				if (surf->lightmaptexturenum > lightmap && (next_lightmap < 0 || surf->lightmaptexturenum < next_lightmap)) {
					next_lightmap = surf->lightmaptexturenum;
				}

				if (surf->lightmaptexturenum == lightmap) {
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
						poly->vbo_start = vbo_pos / VERTEXSIZE;
						vbo_pos = CopyVertToBuffer(vbo_buffer, vbo_pos, poly->verts[0], surf->lightmaptexturenum, material, scaleS, scaleT);
						++output;

						start_vert = 1;
						end_vert = poly->numverts - 1;

						while (start_vert <= end_vert) {
							vbo_pos = CopyVertToBuffer(vbo_buffer, vbo_pos, poly->verts[start_vert], surf->lightmaptexturenum, material, scaleS, scaleT);
							++output;

							if (start_vert < end_vert) {
								vbo_pos = CopyVertToBuffer(vbo_buffer, vbo_pos, poly->verts[end_vert], surf->lightmaptexturenum, material, scaleS, scaleT);
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

			m->textures[i]->gl_vbo_length[lightmap] = length;
			m->textures[i]->gl_next_lightmap[lightmap] = next_lightmap;
			lightmap = next_lightmap;
		}
	}

	Con_Printf("%s = %d verts, reserved %d\n", m->name, (vbo_pos - original_pos) / VERTEXSIZE, GLM_MeasureVBOSizeForBrushModel(m));
	return vbo_pos;
}

// Create VBO, ordering by texture array
int GLM_PopulateVBOForBrushModel(model_t* m, float* vbo_buffer, int vbo_pos)
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
				poly->vbo_start = vbo_pos / VERTEXSIZE;
				vbo_pos = CopyVertToBuffer(vbo_buffer, vbo_pos, poly->verts[0], lightmap, material, scaleS, scaleT);
				++output;

				start_vert = 1;
				end_vert = poly->numverts - 1;

				while (start_vert <= end_vert) {
					vbo_pos = CopyVertToBuffer(vbo_buffer, vbo_pos, poly->verts[start_vert], lightmap, material, scaleS, scaleT);
					++output;

					if (start_vert < end_vert) {
						vbo_pos = CopyVertToBuffer(vbo_buffer, vbo_pos, poly->verts[end_vert], lightmap, material, scaleS, scaleT);
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

	Con_Printf("%s = %d verts, reserved %d\n", m->name, (vbo_pos - original_pos) / VERTEXSIZE, GLM_MeasureVBOSizeForBrushModel(m));
	return vbo_pos;
}





















typedef struct glm_brushmodel_req_s {
	GLuint vbo_count;
	GLuint instanceCount;
	GLuint vbo_start;
	GLuint baseInstance;
	float mvMatrix[16];
	float baseColor[4];
	qbool applyLightmap;
	GLuint count;
	GLuint start;

	GLuint vao;
	GLuint texture_array;
	int texture_index;
	qbool isworldmodel;
	GLuint indices[1024];

	int texture_model;
	int effects;
} glm_brushmodel_req_t;

typedef struct DrawElementsIndirectCommand_s {
	GLuint count;
	GLuint instanceCount;
	GLuint firstIndex;
	GLuint baseVertex;
	GLuint baseInstance;
} DrawElementsIndirectCommand_t;

#define MAX_BRUSHMODEL_BATCH 32
static glm_brushmodel_req_t brushmodel_requests[MAX_BRUSHMODEL_BATCH];
static int batch_count = 0;
static GLuint prev_texture_array = 0;
static qbool in_batch_mode = false;
static qbool firstBrushModel = true;

void GL_BrushModelInitState(void)
{
	static float projectionMatrix[16];

	if (GL_ShadersSupported()) {
		GLM_CreateBrushModelProgram();

		GLM_GetMatrix(GL_PROJECTION, projectionMatrix);

		GL_AlphaBlendFlags(GL_BLEND_DISABLED);
		GL_UseProgram(drawBrushModelProgram.program);
		glUniformMatrix4fv(drawBrushModel_projectionMatrix, 1, GL_FALSE, projectionMatrix);

		//glDisable(GL_CULL_FACE);
		GL_SelectTexture(GL_TEXTURE0);
	}
	else {
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
	int i, j;
	GLuint last_vao = 0;
	GLuint last_array = 0;
	qbool was_worldmodel = 0;

	float mvMatrix[MAX_BRUSHMODEL_BATCH][16];
	float colors[MAX_BRUSHMODEL_BATCH][4];
	int use_lightmaps[MAX_BRUSHMODEL_BATCH];
	int base = 0;

	if (!batch_count) {
		return;
	}

	if (firstBrushModel) {
		GL_BrushModelInitState();
	}

	qsort(brushmodel_requests, batch_count, sizeof(brushmodel_requests[0]), GL_BatchRequestSorter);

	for (i = 0; i < batch_count; ++i) {
		glm_brushmodel_req_t* req = &brushmodel_requests[i];

		if (i == 0) {
			GL_BindVertexArray(req->vao);
		}

		if (req->texture_array != last_array) {
			if (i - base) {
				glUniformMatrix4fv(drawBrushModel_modelViewMatrix, i - base, GL_FALSE, (const GLfloat*) mvMatrix);
				glUniform4fv(drawBrushModel_color, i - base, (const GLfloat*) colors);
				glUniform1iv(drawBrushModel_applyLightmap, i - base, use_lightmaps);

				for (j = base; j < i; ++j) {
					glm_brushmodel_req_t* req = &brushmodel_requests[j];

					glDrawElementsInstancedBaseInstance(GL_TRIANGLE_STRIP, req->count, GL_UNSIGNED_INT, req->indices, 1, j - base);
				}

				base = i;
			}

			GL_BindTexture(GL_TEXTURE_2D_ARRAY, last_array = req->texture_array, true);
		}

		memcpy(&mvMatrix[i - base], req->mvMatrix, sizeof(mvMatrix[i]));
		memcpy(&colors[i - base], req->baseColor, sizeof(req->baseColor));
		use_lightmaps[i - base] = req->isworldmodel ? 1 : 0;
	}

	glUniformMatrix4fv(drawBrushModel_modelViewMatrix, batch_count - base, GL_FALSE, (const GLfloat*) mvMatrix);
	glUniform4fv(drawBrushModel_color, batch_count - base, (const GLfloat*) colors);
	glUniform1iv(drawBrushModel_applyLightmap, batch_count - base, use_lightmaps);

	for (i = base; i < batch_count; ++i) {
		glm_brushmodel_req_t* req = &brushmodel_requests[i];

		glDrawElementsInstancedBaseInstance(GL_TRIANGLE_STRIP, req->count, GL_UNSIGNED_INT, req->indices, 1, i - base);
	}

	batch_count = 0;
}

void GL_EndDrawBrushModels(void)
{
	if (GL_ShadersSupported()) {
		GL_FlushBrushModelBatch();
	}
}

static glm_brushmodel_req_t* GLM_NextBatchRequest(model_t* model, float* base_color, GLuint texture_array)
{
	glm_brushmodel_req_t* req;

	if (batch_count >= MAX_BRUSHMODEL_BATCH) {
		GL_FlushBrushModelBatch();
	}

	req = &brushmodel_requests[batch_count];

	GLM_GetMatrix(GL_MODELVIEW, req->mvMatrix);
	memcpy(req->baseColor, base_color, sizeof(req->baseColor));
	req->isworldmodel = model->isworldmodel;
	req->vao = model->vao.vao;
	if (!req->vao) {
		req->vao = req->vao;
		Con_Printf("Model VAO not set [%s]\n", model->name);
	}
	req->count = 0;
	req->start = model->vbo_start;
	req->texture_array = texture_array;

	++batch_count;
	return req;
}

void GLM_DrawBrushModel(model_t* model)
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

			req = GLM_NextBatchRequest(model, base_color, tex->gl_texture_array);
			for (waterline = 0; waterline < 2; waterline++) {
				for (surf = tex->texturechain[waterline]; surf; surf = surf->texturechain) {
					int newVerts = surf->polys->numverts;

					if (req->count + 3 + newVerts > sizeof(req->indices) / sizeof(req->indices[0])) {
						req = GLM_NextBatchRequest(model, base_color, tex->gl_texture_array);
					}

					// Degenerate triangle strips
					if (req->count) {
						int prev = req->count - 1;

						if (req->count % 2 == 1) {
							req->indices[req->count++] = req->indices[prev];
						}
						req->indices[req->count++] = req->indices[prev];
						req->indices[req->count++] = surf->polys->vbo_start;
					}

					for (v = 0; v < newVerts; ++v) {
						req->indices[req->count++] = surf->polys->vbo_start + v;
					}
				}
			}
		}
	}

	if (req && req->count) {
		GL_FlushBrushModelBatch();
	}

	return;
}
