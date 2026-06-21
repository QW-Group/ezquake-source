/*
Copyright (C) 2026 ezQuake team.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/

#if defined(RENDERER_OPTION_VULKAN) && !defined(RENDERER_OPTION_CLASSIC_OPENGL) && !defined(RENDERER_OPTION_MODERN_OPENGL)

#include "quakedef.h"
#include "gl_model.h"
#include "r_aliasmodel.h"
#include "r_aliasmodel_md3.h"
#include "r_local.h"
#include "r_matrix.h"
#include "r_renderer.h"
#include "r_state.h"
#include "r_texture.h"
#include "r_vao.h"
#include "tr_types.h"
#include "vk_local.h"
#include "glsl/constants.glsl"

const texture_ref null_texture_reference = { 0 };
byte color_white[4] = { 255, 255, 255, 255 };
byte color_black[4] = { 0, 0, 0, 255 };
qbool gl_mtexable = false;
int gl_textureunits = 1;

static rendering_state_t vk_states[r_state_count];
static rendering_state_t vk_current_state;
static r_vao_id vk_current_vao = vao_none;

extern glconfig_t glConfig;

void GL_AliasModelFixNormals(vbo_model_vert_t* vbo_buffer, int v, int vertsPerPose);

rendering_state_t* R_InitRenderingState(r_state_id id, qbool default_state, const char* name, r_vao_id vao)
{
	rendering_state_t* state = &vk_states[id];

	memset(state, 0, sizeof(*state));
	strlcpy(state->name, name, sizeof(state->name));

	state->depth.func = r_depthfunc_less;
	state->depth.nearRange = 0;
	state->depth.farRange = 1;
	state->depth.test_enabled = false;
	state->depth.mask_enabled = false;
	state->blendFunc = r_blendfunc_overwrite;
	state->cullface.mode = r_cullface_back;
	state->line.width = 1.0f;
	state->fog.mode = r_fogmode_disabled;
	state->polygonMode = r_polygonmode_fill;
	state->color[0] = state->color[1] = state->color[2] = state->color[3] = 1;
	state->colorMask[0] = state->colorMask[1] = state->colorMask[2] = state->colorMask[3] = true;
	state->pack_alignment = 4;
	state->vao_id = vao;

	if (default_state) {
		state->cullface.mode = r_cullface_front;
		state->cullface.enabled = true;
		state->depth.func = r_depthfunc_lessorequal;
		state->depth.test_enabled = true;
		state->depth.mask_enabled = true;
		state->clearColor[3] = 1;
		state->blendFunc = r_blendfunc_premultiplied_alpha;
		state->framebuffer_srgb = true;
		state->pack_alignment = 1;
	}

	state->currentViewportWidth = glConfig.vidWidth;
	state->currentViewportHeight = glConfig.vidHeight;
	state->fullScreenViewportWidth = glConfig.vidWidth;
	state->fullScreenViewportHeight = glConfig.vidHeight;
	state->initialized = true;
	return state;
}

rendering_state_t* R_CopyRenderingState(r_state_id dest_id, r_state_id source_id, const char* name)
{
	rendering_state_t* state = &vk_states[dest_id];

	memcpy(state, &vk_states[source_id], sizeof(*state));
	strlcpy(state->name, name, sizeof(state->name));
	return state;
}

rendering_state_t* R_Init3DSpriteRenderingState(r_state_id id, const char* name)
{
	rendering_state_t* state = R_InitRenderingState(id, true, name, vao_3dsprites);

	state->fog.mode = r_fogmode_enabled;
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	state->cullface.enabled = false;
	return state;
}

void R_ApplyRenderingState(r_state_id id)
{
	if (id > r_state_null && id < r_state_count) {
		memcpy(&vk_current_state, &vk_states[id], sizeof(vk_current_state));
	}
}

void R_BufferInvalidateBoundState(r_buffer_id ref)
{
}

void R_CustomColor(float r, float g, float b, float a)
{
	vk_current_state.color[0] = r;
	vk_current_state.color[1] = g;
	vk_current_state.color[2] = b;
	vk_current_state.color[3] = a;
}

void R_CustomColor4ubv(const byte* color)
{
	R_CustomColor(color[0] / 255.0f, color[1] / 255.0f, color[2] / 255.0f, color[3] / 255.0f);
}

void R_CustomLineWidth(float width)
{
	vk_current_state.line.width = width;
}

void R_CustomPolygonOffset(r_polygonoffset_t mode)
{
	vk_current_state.polygonOffset.option = mode;
}

void R_EnableScissorTest(int x, int y, int width, int height)
{
}

void R_DisableScissorTest(void)
{
}

void R_ClearColor(float r, float g, float b, float a)
{
	vk_current_state.clearColor[0] = r;
	vk_current_state.clearColor[1] = g;
	vk_current_state.clearColor[2] = b;
	vk_current_state.clearColor[3] = a;
	vk_options.clearColor[0] = r;
	vk_options.clearColor[1] = g;
	vk_options.clearColor[2] = b;
	vk_options.clearColor[3] = a;
}

void R_Viewport(int x, int y, int width, int height)
{
	if (renderer.Viewport) {
		renderer.Viewport(x, y, width, height);
	}
	vk_current_state.currentViewportX = x;
	vk_current_state.currentViewportY = y;
	vk_current_state.currentViewportWidth = width;
	vk_current_state.currentViewportHeight = height;
}

void R_GetViewport(int* view)
{
	view[0] = vk_current_state.currentViewportX;
	view[1] = vk_current_state.currentViewportY;
	view[2] = vk_current_state.currentViewportWidth;
	view[3] = vk_current_state.currentViewportHeight;
}

void R_SetFullScreenViewport(int x, int y, int width, int height)
{
	vk_current_state.fullScreenViewportX = x;
	vk_current_state.fullScreenViewportY = y;
	vk_current_state.fullScreenViewportWidth = width;
	vk_current_state.fullScreenViewportHeight = height;
}

void R_GetFullScreenViewport(int* viewport)
{
	viewport[0] = vk_current_state.fullScreenViewportX;
	viewport[1] = vk_current_state.fullScreenViewportY;
	viewport[2] = vk_current_state.fullScreenViewportWidth;
	viewport[3] = vk_current_state.fullScreenViewportHeight;
}

void R_InitialiseVAOState(void)
{
	vk_current_vao = vao_none;
}

qbool R_VertexArrayCreated(r_vao_id vao)
{
	return renderer.VertexArrayCreated ? renderer.VertexArrayCreated(vao) : (vao == vao_none);
}

void R_BindVertexArray(r_vao_id vao)
{
	if (renderer.BindVertexArray) {
		renderer.BindVertexArray(vao);
	}
	vk_current_vao = vao;
}

void R_GenVertexArray(r_vao_id vao)
{
	if (renderer.GenVertexArray) {
		renderer.GenVertexArray(vao, "?");
	}
}

qbool R_VAOBound(void)
{
	return vk_current_vao != vao_none;
}

void R_BindVertexArrayElementBuffer(r_buffer_id ref)
{
	if (vk_current_vao != vao_none && renderer.BindVertexArrayElementBuffer) {
		renderer.BindVertexArrayElementBuffer(vk_current_vao, ref);
	}
}

void GL_FramebufferSetFiltering(qbool linear)
{
}

qbool GL_FramebufferEnabled2D(void)
{
	return false;
}

qbool GL_FramebufferEnabled3D(void)
{
	return false;
}

void GL_BenchmarkLightmapFormats(void)
{
}

void R_ProgramCompileAll(void)
{
}

void GLM_MakeAlias3DisplayLists(model_t* model)
{
	md3Surface_t* surf;
	md3St_t* texCoords;
	ezMd3XyzNormal_t* vertices;
	int surfnum;
	int framenum;
	md3Triangle_t* triangles;
	int v;
	md3model_t* md3Model = (md3model_t *)Mod_Extradata(model);
	md3Header_t* pheader = MD3_HeaderForModel(md3Model);
	vbo_model_vert_t* vbo;

	model->vertsInVBO = 0;
	MD3_ForEachSurface(pheader, surf, surfnum) {
		model->vertsInVBO += 3 * surf->numTriangles;
	}
	model->vertsInVBO *= pheader->numFrames;
	model->temp_vbo_buffer = vbo = Q_malloc(sizeof(vbo_model_vert_t) * model->vertsInVBO);

	for (framenum = 0, v = 0; framenum < pheader->numFrames; ++framenum) {
		int initial_v = v;

		MD3_ForEachSurface(pheader, surf, surfnum) {
			int i, triangle;

			texCoords = MD3_SurfaceTextureCoords(surf);
			vertices = MD3_SurfaceVertices(surf);
			triangles = MD3_SurfaceTriangles(surf);

			for (triangle = 0; triangle < surf->numTriangles; ++triangle) {
				for (i = 0; i < 3; ++i, ++v) {
					int vertexNumber = framenum * surf->numVerts + triangles[triangle].indexes[i];
					int nextFrame = Mod_ExpectedNextFrame(model, framenum, pheader->numFrames);
					int nextVertexNumber = nextFrame * surf->numVerts + triangles[triangle].indexes[i];
					ezMd3XyzNormal_t* vert = &vertices[vertexNumber];
					ezMd3XyzNormal_t* nextVert = &vertices[nextVertexNumber];
					float s = texCoords[triangles[triangle].indexes[i]].s;
					float t = texCoords[triangles[triangle].indexes[i]].t;

					VectorCopy(vert->xyz, vbo[v].position);
					VectorCopy(vert->normal, vbo[v].normal);
					vbo[v].texture_coords[0] = s;
					vbo[v].texture_coords[1] = t;
					vbo[v].flags = 0;
					VectorSubtract(nextVert->xyz, vert->xyz, vbo[v].direction);
					if (model->renderfx & RF_LIMITLERP) {
						if ((vert->xyz[0] > 0) != (nextVert->xyz[0] > 0)) {
							vbo[v].flags |= AM_VERTEX_NOLERP;
						}
					}
				}
			}
		}

		GL_AliasModelFixNormals(vbo, initial_v, v - initial_v);
	}
}

#endif
