
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"
#include "utils.h"

#if 0
void GLM_DrawFlat(model_t* model)
{
	byte wallColor[4];
	byte floorColor[4];
	int i, waterline, v;
	msurface_t* surf;
	const qbool draw_whole_map = false;

	GL_DisableMultitexture();

	memcpy(wallColor, r_wallcolor.color, 3);
	memcpy(floorColor, r_floorcolor.color, 3);
	wallColor[3] = floorColor[3] = 255;

	if (model->vao) {
		int lightmap;

		glDisable(GL_CULL_FACE);
		GLM_EnterBatchedPolyRegion(color_white, model->vao, true, true, false);

		glActiveTexture(GL_TEXTURE0);
		for (i = 0; i < model->numtextures; i++) {
			texture_t* tex = model->textures[i];
			GLsizei count;
			GLushort indices[4096];

			if (!tex) {
				continue;
			}
			if (!tex->texturechain[0] && !tex->texturechain[1]) {
				continue;
			}

			GL_Bind(model->textures[i]->gl_texturenum);
			glBindTexture(GL_TEXTURE_2D_ARRAY, lightmap_texture_array);

			lightmap = tex->gl_first_lightmap;
			count = 0;
			while (lightmap >= 0 && tex->gl_vbo_length[lightmap]) {
				if (draw_whole_map) {
					GLM_DrawPolygonByType(GL_TRIANGLE_STRIP, color_white, model->vao, tex->gl_vbo_start[lightmap], tex->gl_vbo_length[lightmap], true, true, false);
				}
				else {
					for (waterline = 0; waterline < 2; waterline++) {
						for (surf = model->textures[i]->texturechain[waterline]; surf; surf = surf->texturechain) {
							int newVerts = surf->polys->numverts;

							// Fixme: change texturechain to sort by lightmap...
							if (surf->lightmaptexturenum != lightmap) {
								continue;
							}

							if (count + 2 + newVerts > sizeof(indices) / sizeof(indices[0])) {
								GL_EnterRegion(va("TextureOverflow %d", i));
								GLM_DrawLightmapIndexedPolygonByType(GL_TRIANGLE_STRIP, color_white, model->vao, indices, count, true, true, false);
								GL_LeaveRegion();
								count = 0;
							}

							// Degenerate triangle strips (remove)
							if (count) {
								int prev = count - 1;

								indices[count++] = indices[prev];
								indices[count++] = surf->polys->vbo_start;
							}

							for (v = 0; v < newVerts; ++v) {
								indices[count++] = surf->polys->vbo_start + v;
							}
						}
					}
				}
				lightmap = tex->gl_next_lightmap[lightmap];
			}

			// Moving on to a new texture, always write out what we've got so far
			if (count) {
				GLM_DrawIndexedPolygonByType(GL_TRIANGLE_STRIP, color_white, model->vao, indices, count, true, true, false);
				count = 0;
			}
		}
		glEnable(GL_CULL_FACE);
		GLM_ExitBatchedPolyRegion();
		return;
	}

	for (i = 0; i < model->numtextures; i++) {
		if (!model->textures[i] || (!model->textures[i]->texturechain[0] && !model->textures[i]->texturechain[1])) {
			continue;
		}

		glActiveTexture(GL_TEXTURE0);
		GL_Bind(model->textures[i]->gl_texturenum);

		for (waterline = 0; waterline < 2; waterline++) {
			for (surf = model->textures[i]->texturechain[waterline]; surf; surf = surf->texturechain) {
				vec3_t normal;
				qbool isFloor;

				// FIXME: fix lightmap code
				VectorCopy(surf->plane->normal, normal);
				VectorNormalize(normal);

				// r_drawflat 1 == All solid colors
				// r_drawflat 2 == Solid floor/ceiling only
				// r_drawflat 3 == Solid walls only

				isFloor = normal[2] < -0.5 || normal[2] > 0.5;
				if (r_drawflat.integer == 1 || (r_drawflat.integer == 2 && isFloor) || (r_drawflat.integer == 3 && !isFloor)) {
					if (!lightmap_texture_array) {
						GL_Bind(lightmap_textures[surf->lightmaptexturenum]);
					}
					GLM_DrawFlatPoly(isFloor ? floorColor : wallColor, surf->polys->vao, surf->polys->numverts, model->isworldmodel);
				}
				else {
					if (!lightmap_texture_array) {
						GL_Bind(lightmap_textures[surf->lightmaptexturenum]);
						GLM_DrawPolygonByType(GL_TRIANGLE_FAN, color_white, surf->polys->vao, 0, surf->polys->numverts, model->isworldmodel, true, false);
					}
					else {
						GLM_DrawLightmapArrayPolygonByType(GL_TRIANGLE_FAN, color_white, surf->polys->vao, 0, surf->polys->numverts, model->isworldmodel, true, false);
					}
				}

				// TODO: Caustics
				// START shaman FIX /r_drawflat + /gl_caustics {
				/*if (waterline && draw_caustics) {
				s->polys->caustics_chain = caustics_polys;
				caustics_polys = s->polys;
				}*/
				// } END shaman FIX /r_drawflat + /gl_caustics
			}
		}
	}

	// TODO: Caustics
}
#endif

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

		drawBrushModel_modelViewMatrix = glGetUniformLocation(drawBrushModelProgram.program, "modelViewMatrix");
		drawBrushModel_projectionMatrix = glGetUniformLocation(drawBrushModelProgram.program, "projectionMatrix");
		drawBrushModel_color = glGetUniformLocation(drawBrushModelProgram.program, "color");
		drawBrushModel_materialTex = glGetUniformLocation(drawBrushModelProgram.program, "materialTex");
		drawBrushModel_lightmapTex = glGetUniformLocation(drawBrushModelProgram.program, "lightmapTex");
		drawBrushModel_applyTexture = glGetUniformLocation(drawBrushModelProgram.program, "apply_texture");
		drawBrushModel_applyLightmap = glGetUniformLocation(drawBrushModelProgram.program, "apply_lightmap");
	}
}

void GLM_DrawBrushModel(model_t* model)
{
	GLushort indices[4096];
	int i, waterline, v;
	msurface_t* surf;
	float base_color[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float modelViewMatrix[16];
	float projectionMatrix[16];

	glDisable(GL_CULL_FACE);

	GLM_CreateBrushModelProgram();

	GLM_GetMatrix(GL_MODELVIEW, modelViewMatrix);
	GLM_GetMatrix(GL_PROJECTION, projectionMatrix);

	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	GL_UseProgram(drawBrushModelProgram.program);
	glUniformMatrix4fv(drawBrushModel_modelViewMatrix, 1, GL_FALSE, modelViewMatrix);
	glUniformMatrix4fv(drawBrushModel_projectionMatrix, 1, GL_FALSE, projectionMatrix);
	glUniform4f(drawBrushModel_color, base_color[0], base_color[1], base_color[2], base_color[3]);
	glUniform1i(drawBrushModel_materialTex, 0);
	glUniform1i(drawBrushModel_lightmapTex, 2);
	glUniform1i(drawBrushModel_applyLightmap, model->isworldmodel ? 1 : 0);
	glUniform1i(drawBrushModel_applyTexture, 1);

	glBindVertexArray(model->vao);

	glActiveTexture(GL_TEXTURE0);
	for (i = 0; i < model->texture_array_count; ++i) {
		texture_t* base_tex = model->textures[model->texture_array_first[i]];
		qbool first_in_this_array = true;
		int texIndex;
		int count = 0;

		if (!base_tex || !base_tex->size_start) {
			continue;
		}

		for (texIndex = model->texture_array_first[i]; texIndex >= 0 && texIndex < model->numtextures; texIndex = model->textures[texIndex]->next_same_size) {
			texture_t* tex = model->textures[texIndex];

			if (!tex->texturechain[0] && !tex->texturechain[1]) {
				continue;
			}

			// Going to draw at least one surface, so bind the texture array
			if (first_in_this_array) {
				glBindTexture(GL_TEXTURE_2D_ARRAY, model->texture_arrays[i]);
				first_in_this_array = false;
			}

			for (waterline = 0; waterline < 2; waterline++) {
				for (surf = tex->texturechain[waterline]; surf; surf = surf->texturechain) {
					int newVerts = surf->polys->numverts;

					if (count + 2 + newVerts > sizeof(indices) / sizeof(indices[0])) {
						glDrawElements(GL_TRIANGLE_STRIP, count, GL_UNSIGNED_SHORT, indices);
						count = 0;
					}

					// Degenerate triangle strips
					if (count) {
						int prev = count - 1;

						indices[count++] = indices[prev];
						indices[count++] = surf->polys->vbo_start;
					}

					for (v = 0; v < newVerts; ++v) {
						indices[count++] = surf->polys->vbo_start + v;
					}
				}
			}
		}

		if (count) {
			glDrawElements(GL_TRIANGLE_STRIP, count, GL_UNSIGNED_SHORT, indices);
		}
	}

	glEnable(GL_CULL_FACE);
	return;
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

void GLM_LoadBrushModelTextures(model_t* loadmodel)
{
	int max_width = 0;
	int max_height = 0;
	int num_sizes;
	int array_index;
	int i;

	// Find textures with common dimensions
	for (i = 0; i < loadmodel->numtextures; i++) {
		texture_t* tx = loadmodel->textures[i];
		if (!tx || !tx->loaded) {
			continue;
		}

		{
			int w, h, mips;
			int miplevel = 0;

			glBindTexture(GL_TEXTURE_2D, tx->gl_texturenum);
			glGetTexLevelParameteriv(GL_TEXTURE_2D, miplevel, GL_TEXTURE_WIDTH, &w);
			glGetTexLevelParameteriv(GL_TEXTURE_2D, miplevel, GL_TEXTURE_HEIGHT, &h);
			glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, &mips);

			tx->gl_width = w;
			tx->gl_height = h;

			max_width = max(max_width, w);
			max_height = max(max_height, h);
		}
	}

	num_sizes = R_ChainTexturesBySize(loadmodel);
	loadmodel->texture_arrays = Hunk_Alloc(sizeof(GLuint) * num_sizes);
	loadmodel->texture_array_first = Hunk_Alloc(sizeof(int) * num_sizes);
	loadmodel->texture_array_count = num_sizes;

	array_index = 0;
	for (i = 0; i < loadmodel->numtextures; ++i) {
		int texIndex, sizeCount;
		texture_t* tx;

		tx = loadmodel->textures[i];
		if (!tx || !tx->loaded || !tx->size_start) {
			continue;
		}

		// Count textures with this size
		sizeCount = 0;
		for (texIndex = i; texIndex >= 0 && texIndex < loadmodel->numtextures; texIndex = loadmodel->textures[texIndex]->next_same_size) {
			++sizeCount;
		}

		// Create texture array and copy textures in
		{
			int texture_index = 0;
			GLuint texture_array;
			int max_miplevels = 0;
			int min_dimension = min(tx->gl_width, tx->gl_height);
			GLubyte* buffer = Q_malloc(tx->gl_width * tx->gl_height * 4);

			// 
			while (min_dimension > 0) {
				max_miplevels++;
				min_dimension /= 2;
			}
			glGenTextures(1, &texture_array);
			glBindTexture(GL_TEXTURE_2D_ARRAY, texture_array);
			glTexStorage3D(GL_TEXTURE_2D_ARRAY, max_miplevels, GL_RGBA8, tx->gl_width, tx->gl_height, sizeCount);

			loadmodel->texture_arrays[array_index] = texture_array;
			loadmodel->texture_array_first[array_index] = i;

			// Load textures into the array
			texture_index = 0;
			for (texIndex = i; texIndex >= 0 && texIndex < loadmodel->numtextures; texIndex = loadmodel->textures[texIndex]->next_same_size) {
				int w, h, level;
				texture_t* thisTex = loadmodel->textures[texIndex];

				glBindTexture(GL_TEXTURE_2D, thisTex->gl_texturenum);
				w = tx->gl_width;
				h = tx->gl_height;

				for (level = 0; level < max_miplevels && w && h; ++level, w /= 2, h /= 2) {
					glGetTexImage(GL_TEXTURE_2D, level, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
					glTexSubImage3D(GL_TEXTURE_2D_ARRAY, level, 0, 0, texture_index, w, h, 1, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
				}

				// store details
				thisTex->gl_texture_index = texture_index;

				++texture_index;
			}

			Q_free(buffer);
		}

		++array_index;
	}
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

void GLM_CreateVAOForModel(model_t* m)
{
	int total_surf_verts = 0;
	int total_surfaces = 0;
	int i, j;
	int vbo_pos = 0;
	int vbo_buffer_size = 0;
	float* vbo_buffer;
	int combinations = 0;

	for (i = 0; i < m->numtextures; ++i) {
		if (m->textures[i]) {
			m->textures[i]->gl_first_lightmap = -1;
			for (j = 0; j < MAX_LIGHTMAPS; ++j) {
				m->textures[i]->gl_next_lightmap[j] = -1;
			}
		}
	}

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
		return;
	}

	vbo_buffer_size = VERTEXSIZE * (total_surf_verts + 2 * (total_surfaces - 1));
	vbo_buffer = Q_malloc(sizeof(float) * vbo_buffer_size);

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

						if (length) {
							vbo_pos = DuplicateVertex(vbo_buffer, vbo_pos);
							vbo_pos = CopyVertToBuffer(vbo_buffer, vbo_pos, poly->verts[0], surf->lightmaptexturenum, material, scaleS, scaleT);
							length += 2;
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

	if (vbo_pos / VERTEXSIZE != (total_surf_verts + 2 * (total_surfaces - combinations))) {
		Con_Printf("WARNING: Model used %d verts, expected %d\n", vbo_pos / VERTEXSIZE, (total_surf_verts + 2 * (total_surfaces - combinations)));
		Con_Printf("         Difference %d, across %d surfaces [%d combinations]\n", vbo_pos / VERTEXSIZE - (total_surf_verts + 2 * (total_surfaces - combinations)), total_surfaces, combinations);
	}

	if (strstr(m->name, "/b_nail1.bsp")) {
		Con_Printf("b_nail1.bsp:\n");
		for (i = 0; i < vbo_pos / VERTEXSIZE; ++i) {
			float* vert = &vbo_buffer[i * VERTEXSIZE];
			Con_Printf("%d: %d %d %d (%f %f) [%d]\n", i, (int)vert[0], (int)vert[1], (int)vert[2], vert[3], vert[4], (int)vert[10]);
		}
	}

	if (!m->vbo) {
		// Count total number of verts
		glGenBuffers(1, &m->vbo);
		glBindBufferExt(GL_ARRAY_BUFFER, m->vbo);
		glBufferDataExt(GL_ARRAY_BUFFER, sizeof(float) * vbo_pos, vbo_buffer, GL_STATIC_DRAW);
	}
	Q_free(vbo_buffer);

	if (!m->vao) {
		glGenVertexArrays(1, &m->vao);
		glBindVertexArray(m->vao);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);
		glEnableVertexAttribArray(3);
		glEnableVertexAttribArray(4);
		glEnableVertexAttribArray(5);
		glBindBufferExt(GL_ARRAY_BUFFER, m->vbo);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * VERTEXSIZE, (void*) 0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * VERTEXSIZE, (void*) (sizeof(float) * 3));
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(float) * VERTEXSIZE, (void*) (sizeof(float) * 5));
		glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(float) * VERTEXSIZE, (void*) (sizeof(float) * 7));
		glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(float) * VERTEXSIZE, (void*) (sizeof(float) * 9));
		glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(float) * VERTEXSIZE, (void*) (sizeof(float) * 10));
	}
}
