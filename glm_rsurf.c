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

extern GLuint lightmap_texture_array;

static glm_program_t turbPolyProgram;
static GLint turb_modelViewMatrix;
static GLint turb_projectionMatrix;
static GLint turb_materialTex;
static GLint turb_alpha;
static GLint turb_time;
static GLint turb_gamma3d;

// Very similar to GLM_DrawPoly, but with manipulation of texture coordinates
static void GLM_CompileTurbPolyProgram(void)
{
	if (!turbPolyProgram.program) {
		GL_VFDeclare(turb_poly);

		// Initialise program for drawing image
		GLM_CreateVFProgram("Turb poly", GL_VFParams(turb_poly), &turbPolyProgram);
	}

	if (turbPolyProgram.program && !turbPolyProgram.uniforms_found) {
		turb_modelViewMatrix = glGetUniformLocation(turbPolyProgram.program, "modelViewMatrix");
		turb_projectionMatrix = glGetUniformLocation(turbPolyProgram.program, "projectionMatrix");
		turb_materialTex = glGetUniformLocation(turbPolyProgram.program, "materialTex");
		turb_alpha = glGetUniformLocation(turbPolyProgram.program, "alpha");
		turb_time = glGetUniformLocation(turbPolyProgram.program, "time");
		turb_gamma3d = glGetUniformLocation(turbPolyProgram.program, "gamma3d");
		turbPolyProgram.uniforms_found = true;

		glProgramUniform1i(turbPolyProgram.program, turb_materialTex, 0);
	}
}

void GLM_DrawIndexedTurbPolys(unsigned int vao, GLuint* indices, int vertices, float alpha)
{
	GLM_CompileTurbPolyProgram();

	if (turbPolyProgram.program && vao) {
		float modelViewMatrix[16];
		float projectionMatrix[16];

		GLM_GetMatrix(GL_MODELVIEW, modelViewMatrix);
		GLM_GetMatrix(GL_PROJECTION, projectionMatrix);

		GL_UseProgram(turbPolyProgram.program);
		glUniformMatrix4fv(turb_modelViewMatrix, 1, GL_FALSE, modelViewMatrix);
		glUniformMatrix4fv(turb_projectionMatrix, 1, GL_FALSE, projectionMatrix);
		glUniform1f(turb_alpha, alpha);
		glUniform1f(turb_time, cl.time);
		glUniform1f(turb_gamma3d, v_gamma.value);

		GL_BindVertexArray(vao);
		//glDisable(GL_CULL_FACE);
		glDrawElements(GL_TRIANGLE_STRIP, vertices, GL_UNSIGNED_INT, indices);
		//glEnable(GL_CULL_FACE);
	}
}

void GLM_DrawTurbPolys(unsigned int vao, int vertices, float alpha)
{
	GLM_CompileTurbPolyProgram();

	if (turbPolyProgram.program && vao) {
		float modelViewMatrix[16];
		float projectionMatrix[16];

		GLM_GetMatrix(GL_MODELVIEW, modelViewMatrix);
		GLM_GetMatrix(GL_PROJECTION, projectionMatrix);

		GL_UseProgram(turbPolyProgram.program);
		glUniformMatrix4fv(turb_modelViewMatrix, 1, GL_FALSE, modelViewMatrix);
		glUniformMatrix4fv(turb_projectionMatrix, 1, GL_FALSE, projectionMatrix);
		glUniform1f(turb_alpha, alpha);
		glUniform1f(turb_time, cl.time);
		glUniform1f(turb_gamma3d, v_gamma.value);

		GL_BindVertexArray(vao);
		//glDisable(GL_CULL_FACE);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, vertices);
		//glEnable(GL_CULL_FACE);
	}
}

static glm_program_t drawFlatPolyProgram;
static GLint drawFlat_modelViewMatrix;
static GLint drawFlat_projectionMatrix;
static GLint drawFlat_color;
static GLint drawFlat_materialTex;
static GLint drawFlat_lightmapTex;
static GLint drawFlat_apply_lightmap;
static GLint drawFlat_apply_texture;
static GLint drawFlat_alpha_texture;

static void Compile_DrawFlatPolyProgram(void)
{
	if (!drawFlatPolyProgram.program) {
		GL_VFDeclare(generic_poly);

		// Initialise program for drawing image
		GLM_CreateVFProgram("Drawflat poly", GL_VFParams(generic_poly), &drawFlatPolyProgram);
	}

	if (drawFlatPolyProgram.program && !drawFlatPolyProgram.uniforms_found) {
		drawFlat_modelViewMatrix = glGetUniformLocation(drawFlatPolyProgram.program, "modelViewMatrix");
		drawFlat_projectionMatrix = glGetUniformLocation(drawFlatPolyProgram.program, "projectionMatrix");
		drawFlat_color = glGetUniformLocation(drawFlatPolyProgram.program, "color");
		drawFlat_materialTex = glGetUniformLocation(drawFlatPolyProgram.program, "materialTex");
		drawFlat_lightmapTex = glGetUniformLocation(drawFlatPolyProgram.program, "lightmapTex");
		drawFlat_apply_lightmap = glGetUniformLocation(drawFlatPolyProgram.program, "apply_lightmap");
		drawFlat_apply_texture = glGetUniformLocation(drawFlatPolyProgram.program, "apply_texture");
		drawFlat_alpha_texture = glGetUniformLocation(drawFlatPolyProgram.program, "alpha_texture");

		drawFlatPolyProgram.uniforms_found = true;

		glProgramUniform1i(drawFlatPolyProgram.program, drawFlat_materialTex, 0);
		glProgramUniform1i(drawFlatPolyProgram.program, drawFlat_lightmapTex, 2);
	}
}

static glm_program_t lightmapPolyProgram;
static GLint lightmapPoly_modelViewMatrix;
static GLint lightmapPoly_projectionMatrix;
static GLint lightmapPoly_color;
static GLint lightmapPoly_materialTex;
static GLint lightmapPoly_lightmapTex;
static GLint lightmapPoly_apply_lightmap;
static GLint lightmapPoly_apply_texture;
static GLint lightmapPoly_alpha_texture;

static qbool uniforms_set = false;

static void Compile_LightmapPolyProgram(void)
{
	if (!lightmapPolyProgram.program) {
		GL_VFDeclare(lightmaparray_poly);

		// Initialise program for drawing image
		GLM_CreateVFProgram("Lightmap poly", GL_VFParams(lightmaparray_poly), &lightmapPolyProgram);
	}

	if (lightmapPolyProgram.program && !lightmapPolyProgram.uniforms_found) {
		lightmapPoly_modelViewMatrix = glGetUniformLocation(lightmapPolyProgram.program, "modelViewMatrix");
		lightmapPoly_projectionMatrix = glGetUniformLocation(lightmapPolyProgram.program, "projectionMatrix");
		lightmapPoly_color = glGetUniformLocation(lightmapPolyProgram.program, "color");
		lightmapPoly_materialTex = glGetUniformLocation(lightmapPolyProgram.program, "materialTex");
		lightmapPoly_lightmapTex = glGetUniformLocation(lightmapPolyProgram.program, "lightmapTex");
		lightmapPoly_apply_lightmap = glGetUniformLocation(lightmapPolyProgram.program, "apply_lightmap");
		lightmapPoly_apply_texture = glGetUniformLocation(lightmapPolyProgram.program, "apply_texture");
		lightmapPoly_alpha_texture = glGetUniformLocation(lightmapPolyProgram.program, "alpha_texture");
		lightmapPolyProgram.uniforms_found = true;

		glProgramUniform1i(lightmapPolyProgram.program, lightmapPoly_materialTex, 0);
		glProgramUniform1i(lightmapPolyProgram.program, lightmapPoly_lightmapTex, 2);
	}
}

qbool GLM_PrepareLightmapProgram(GLenum type, byte* color, unsigned int vao, qbool apply_lightmap, qbool apply_texture, qbool alpha_texture)
{
	Compile_LightmapPolyProgram();

	if (lightmapPolyProgram.program && vao) {
		if (!uniforms_set) {
			float modelViewMatrix[16];
			float projectionMatrix[16];

			GLM_GetMatrix(GL_MODELVIEW, modelViewMatrix);
			GLM_GetMatrix(GL_PROJECTION, projectionMatrix);

			GL_UseProgram(lightmapPolyProgram.program);
			glUniformMatrix4fv(lightmapPoly_modelViewMatrix, 1, GL_FALSE, modelViewMatrix);
			glUniformMatrix4fv(lightmapPoly_projectionMatrix, 1, GL_FALSE, projectionMatrix);
			glUniform4f(lightmapPoly_color, color[0] * 1.0f / 255, color[1] * 1.0f / 255, color[2] * 1.0f / 255, color[3] * 1.0f / 255);
			glUniform1i(lightmapPoly_apply_lightmap, apply_lightmap ? 1 : 0);
			glUniform1i(lightmapPoly_apply_texture, apply_texture ? 1 : 0);
			glUniform1i(lightmapPoly_alpha_texture, alpha_texture ? 1 : 0);

			GL_BindVertexArray(vao);
		}
		return true;
	}

	return false;
}

void GLM_DrawMultiLightmapIndexedPolygonByType(GLenum type, byte* color, unsigned int vao, GLuint** indices, GLsizei* lengths, int count, qbool apply_lightmap, qbool apply_texture, qbool alpha_texture)
{
	if (GLM_PrepareLightmapProgram(type, color, vao, apply_lightmap, apply_texture, alpha_texture)) {
		glMultiDrawElements(type, lengths, GL_UNSIGNED_INT, indices, count);
	}
}

void GLM_DrawLightmapIndexedPolygonByType(GLenum type, byte* color, unsigned int vao, GLuint* indices, int count, qbool apply_lightmap, qbool apply_texture, qbool alpha_texture)
{
	if (GLM_PrepareLightmapProgram(type, color, vao, apply_lightmap, apply_texture, alpha_texture)) {
		glDrawElements(type, count, GL_UNSIGNED_INT, indices);
	}
}

static glm_program_t drawworld;
static GLint drawworld_RefdefCvars_block;
static GLint drawworld_WorldCvars_block;

typedef struct block_refdef_s {
	float modelViewMatrix[16];
	float projectionMatrix[16];
	float time;
	float gamma3d;

	// if enabled, texture coordinates are always 0,0
	int r_textureless;
	// Total size must be multiple of vec4
	int padding;
} block_refdef_t;

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
static glm_ubo_t ubo_refdef;
static glm_ubo_t ubo_worldcvars;
static block_refdef_t refdef;
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
		drawworld_WorldCvars_block = glGetUniformBlockIndex(drawworld.program, "WorldCvars");

		glGetActiveUniformBlockiv(drawworld.program, drawworld_RefdefCvars_block, GL_UNIFORM_BLOCK_DATA_SIZE, &size);
		Con_Printf("sizeof(refdef) = %d, expected = %d\n", sizeof(refdef), size);

		glGetActiveUniformBlockiv(drawworld.program, drawworld_WorldCvars_block, GL_UNIFORM_BLOCK_DATA_SIZE, &size);
		Con_Printf("sizeof(world) = %d, expected = %d\n", sizeof(world), size);

		glUniformBlockBinding(drawworld.program, drawworld_RefdefCvars_block, GL_BINDINGPOINT_REFDEF_CVARS);
		glUniformBlockBinding(drawworld.program, drawworld_WorldCvars_block, GL_BINDINGPOINT_DRAWWORLD_CVARS);

		GL_GenUniformBuffer(&ubo_refdef, "refdef", &refdef, sizeof(refdef));
		glBindBufferBase(GL_UNIFORM_BUFFER, GL_BINDINGPOINT_REFDEF_CVARS, ubo_refdef.ubo);
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

static void GLM_EnterBatchedWorldRegion(unsigned int vao, qbool detail_tex, qbool caustics)
{
	float wateralpha = bound((1 - r_refdef2.max_watervis), r_wateralpha.value, 1);
	extern cvar_t r_telecolor, r_lavacolor, r_slimecolor, r_watercolor, r_fastturb, gl_textureless, r_skycolor;

	Compile_DrawWorldProgram(detail_tex, caustics);

	GLM_GetMatrix(GL_MODELVIEW, refdef.modelViewMatrix);
	GLM_GetMatrix(GL_PROJECTION, refdef.projectionMatrix);
	refdef.time = cl.time;
	refdef.gamma3d = v_gamma.value;
	refdef.r_textureless = gl_textureless.integer;

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

	glBindBufferExt(GL_UNIFORM_BUFFER, ubo_refdef.ubo);
	glBufferDataExt(GL_UNIFORM_BUFFER, sizeof(refdef), &refdef, GL_DYNAMIC_DRAW);
	glBindBufferExt(GL_UNIFORM_BUFFER, ubo_worldcvars.ubo);
	glBufferDataExt(GL_UNIFORM_BUFFER, sizeof(world), &world, GL_DYNAMIC_DRAW);

	GL_UseProgram(drawworld.program);

	GL_BindVertexArray(vao);
}

void GLM_ExitBatchedPolyRegion(void)
{
	uniforms_set = false;
}

// Still used by fastturb...
void GLM_DrawIndexedPolygonByType(GLenum type, byte* color, unsigned int vao, GLuint* indices, int count, qbool apply_lightmap, qbool apply_texture, qbool alpha_texture)
{
	Compile_DrawFlatPolyProgram();

	if (drawFlatPolyProgram.program && vao) {
		if (!uniforms_set) {
			float modelViewMatrix[16];
			float projectionMatrix[16];

			GLM_GetMatrix(GL_MODELVIEW, modelViewMatrix);
			GLM_GetMatrix(GL_PROJECTION, projectionMatrix);

			GL_UseProgram(drawFlatPolyProgram.program);
			glUniformMatrix4fv(drawFlat_modelViewMatrix, 1, GL_FALSE, modelViewMatrix);
			glUniformMatrix4fv(drawFlat_projectionMatrix, 1, GL_FALSE, projectionMatrix);
			glUniform4f(drawFlat_color, color[0] * 1.0f / 255, color[1] * 1.0f / 255, color[2] * 1.0f / 255, color[3] * 1.0f / 255);
			glUniform1i(drawFlat_apply_lightmap, apply_lightmap ? 1 : 0);
			glUniform1i(drawFlat_apply_texture, apply_texture ? 1 : 0);
			glUniform1i(drawFlat_alpha_texture, alpha_texture ? 1 : 0);

			GL_BindVertexArray(vao);
		}

		glDrawElements(type, count, GL_UNSIGNED_INT, indices);
	}
}

void GLM_DrawLightmapArrayPolygonByType(GLenum type, byte* color, unsigned int vao, int start, int vertices, qbool apply_lightmap, qbool apply_texture, qbool alpha_texture)
{
	if (GLM_PrepareLightmapProgram(type, color, vao, apply_lightmap, apply_texture, alpha_texture)) {
		glDrawArrays(type, start, vertices);
	}
}

// Very simple polygon drawing until we fix
void GLM_DrawPolygonByType(GLenum type, byte* color, unsigned int vao, int start, int vertices, qbool apply_lightmap, qbool apply_texture, qbool alpha_texture)
{
	if (!drawFlatPolyProgram.program) {
		Compile_DrawFlatPolyProgram();
	}

	if (drawFlatPolyProgram.program && vao) {
		float modelViewMatrix[16];
		float projectionMatrix[16];

		GLM_GetMatrix(GL_MODELVIEW, modelViewMatrix);
		GLM_GetMatrix(GL_PROJECTION, projectionMatrix);

		GL_UseProgram(drawFlatPolyProgram.program);
		glUniformMatrix4fv(drawFlat_modelViewMatrix, 1, GL_FALSE, modelViewMatrix);
		glUniformMatrix4fv(drawFlat_projectionMatrix, 1, GL_FALSE, projectionMatrix);
		glUniform4f(drawFlat_color, color[0] * 1.0f / 255, color[1] * 1.0f / 255, color[2] * 1.0f / 255, color[3] * 1.0f / 255);
		glUniform1i(drawFlat_materialTex, 0);
		glUniform1i(drawFlat_lightmapTex, 2);
		glUniform1i(drawFlat_apply_lightmap, apply_lightmap ? 1 : 0);
		glUniform1i(drawFlat_apply_texture, apply_texture ? 1 : 0);
		glUniform1i(drawFlat_alpha_texture, alpha_texture ? 1 : 0);

		GL_BindVertexArray(vao);
		glDrawArrays(type, start, vertices);
	}
}

void GLM_DrawPolygon(byte* color, unsigned int vao, int start, int vertices, qbool apply_lightmap, qbool apply_texture, qbool alpha_texture)
{
	GLM_DrawPolygonByType(GL_TRIANGLE_FAN, color, vao, start, vertices, apply_lightmap, apply_texture, alpha_texture);
}

void GLM_DrawFlatPoly(byte* color, unsigned int vao, int vertices, qbool apply_lightmap)
{
	GLM_DrawPolygon(color, vao, 0, vertices, apply_lightmap, false, false);
}

void GLM_DrawTexturedPoly(byte* color, unsigned int vao, int start, int vertices, qbool apply_lightmap, qbool alpha_test)
{
	GLM_DrawPolygon(color, vao, start, vertices, apply_lightmap, true, alpha_test);
}

void GLM_DrawTexturedWorld(model_t* model)
{
	GLuint indices[4096];
	int i, waterline, v;
	msurface_t* surf;
	qbool draw_detail_texture = gl_detail.integer && detailtexture;
	qbool draw_caustics = gl_caustics.integer && underwatertexture;
	int count = 0;

	GLM_EnterBatchedWorldRegion(model->vao.vao, draw_detail_texture, draw_caustics);

	// Bind lightmap array
	GL_SelectTexture(GL_TEXTURE0 + TEXTURE_UNIT_LIGHTMAPS);
	GL_BindTexture(GL_TEXTURE_2D_ARRAY, lightmap_texture_array, true);
	if (draw_detail_texture) {
		GL_SelectTexture(GL_TEXTURE0 + TEXTURE_UNIT_DETAIL);
		GL_Bind(detailtexture);
	}
	if (draw_caustics) {
		GL_SelectTexture(GL_TEXTURE0 + TEXTURE_UNIT_CAUSTICS);
		GL_Bind(underwatertexture);
	}
	GL_SelectTexture(GL_TEXTURE0 + TEXTURE_UNIT_MATERIAL);

	for (i = 0; i < model->texture_array_count; ++i) {
		texture_t* base_tex = model->textures[model->texture_array_first[i]];
		qbool first_in_this_array = true;
		int texIndex;

		if (!base_tex || !base_tex->size_start || !base_tex->gl_texture_array) {
			continue;
		}

		count = 0;
		for (texIndex = model->texture_array_first[i]; texIndex >= 0 && texIndex < model->numtextures; texIndex = model->textures[texIndex]->next_same_size) {
			texture_t* tex = model->textures[texIndex];

			if (!tex->texturechain[0] && !tex->texturechain[1]) {
				continue;
			}

			// Going to draw at least one surface, so bind the texture array
			if (first_in_this_array) {
				GL_BindTexture(GL_TEXTURE_2D_ARRAY, model->texture_arrays[i], true);
				first_in_this_array = false;
			}

			for (waterline = 0; waterline < 2; waterline++) {
				for (surf = tex->texturechain[waterline]; surf; surf = surf->texturechain) {
					glpoly_t* poly;

					for (poly = surf->polys; poly; poly = poly->next) {
						int newVerts = poly->numverts;

						if (count + 3 + newVerts > sizeof(indices) / sizeof(indices[0])) {
							glDrawElements(GL_TRIANGLE_STRIP, count, GL_UNSIGNED_INT, indices);
							count = 0;
						}

						// Degenerate triangle strips
						if (count) {
							int prev = count - 1;

							if (count % 2 == 1) {
								indices[count++] = indices[prev];
							}
							indices[count++] = indices[prev];
							indices[count++] = poly->vbo_start;
						}

						for (v = 0; v < newVerts; ++v) {
							indices[count++] = poly->vbo_start + v;
						}
					}
				}
			}
		}

		if (count) {
			glDrawElements(GL_TRIANGLE_STRIP, count, GL_UNSIGNED_INT, indices);
		}
	}

	count = 0;
	for (waterline = 0; waterline < 2; waterline++) {
		for (surf = model->drawflat_chain[waterline]; surf; surf = surf->texturechain) {
			glpoly_t* poly;

			for (poly = surf->polys; poly; poly = poly->next) {
				int newVerts = poly->numverts;

				if (count + 3 + newVerts > sizeof(indices) / sizeof(indices[0])) {
					glDrawElements(GL_TRIANGLE_STRIP, count, GL_UNSIGNED_INT, indices);
					count = 0;
				}

				// Degenerate triangle strips
				if (count) {
					int prev = count - 1;

					if (count % 2 == 1) {
						indices[count++] = indices[prev];
					}
					indices[count++] = indices[prev];
					indices[count++] = poly->vbo_start;
				}

				for (v = 0; v < newVerts; ++v) {
					indices[count++] = poly->vbo_start + v;
				}
			}
		}
	}
	if (count) {
		glDrawElements(GL_TRIANGLE_STRIP, count, GL_UNSIGNED_INT, indices);
	}

	GLM_ExitBatchedPolyRegion();
	return;
}

void GLM_DrawFlat(model_t* model);

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



