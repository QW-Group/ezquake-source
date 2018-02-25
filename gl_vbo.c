// gl_vbo.c

#include "quakedef.h"
#include "vx_stuff.h"
#include "vx_tracker.h"
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"
#ifndef  __APPLE__
#include "tr_types.h"
#endif
#include "glsl/constants.glsl"

void GL_MD3ModelAddToVBO(model_t* mod, buffer_ref vbo, buffer_ref ssbo, int position);

static buffer_ref aliasModel_vbo;
static buffer_ref aliasModel_ssbo;

static buffer_ref GL_CreateInstanceVBO(void)
{
	unsigned int values[MAX_STANDARD_ENTITIES];
	int i;

	for (i = 0; i < MAX_STANDARD_ENTITIES; ++i) {
		values[i] = i;
	}

	return GL_GenFixedBuffer(GL_ARRAY_BUFFER, "instance#", sizeof(values), values, GL_STATIC_DRAW);
}

static void GL_MeasureVBOForModel(model_t* mod, int* required_vbo_length)
{
	switch (mod->type) {
	case mod_alias:
	case mod_alias3:
		*required_vbo_length += mod->vertsInVBO;
		break;
	case mod_sprite:
		// We allocated 4 points at beginning of VBO, no need to duplicate per-model
		break;
	case mod_brush:
		// These are in different format/vbo at the moment
		break;
	case mod_unknown:
		// Keep compiler happy
		break;
	}
}

static void GL_ImportModelToVBO(model_t* mod, int* new_vbo_position)
{
	if (mod->type == mod_alias) {
		aliashdr_t* paliashdr = (aliashdr_t *)Mod_Extradata(mod);

		GL_AliasModelAddToVBO(mod, paliashdr, aliasModel_vbo, aliasModel_ssbo, *new_vbo_position);
		*new_vbo_position += mod->vertsInVBO;
	}
	else if (mod->type == mod_alias3) {
		GL_MD3ModelAddToVBO(mod, aliasModel_vbo, aliasModel_ssbo, *new_vbo_position);

		*new_vbo_position += mod->vertsInVBO;
	}
	else if (mod->type == mod_sprite) {
		mod->vbo_start = 0;
	}
	else if (mod->type == mod_brush) {
		mod->vbo_start = 0;
	}
}

static void GL_ImportSpriteCoordsToVBO(int* position)
{
	vbo_model_vert_t verts[4];

	VectorSet(verts[0].position, 0, -1, -1);
	verts[0].texture_coords[0] = 1;
	verts[0].texture_coords[1] = 1;
	verts[0].vert_index = 0;

	VectorSet(verts[1].position, 0, -1, 1);
	verts[1].texture_coords[0] = 1;
	verts[1].texture_coords[1] = 0;
	verts[1].vert_index = 1;

	VectorSet(verts[2].position, 0, 1, 1);
	verts[2].texture_coords[0] = 0;
	verts[2].texture_coords[1] = 0;
	verts[2].vert_index = 2;

	VectorSet(verts[3].position, 0, 1, -1);
	verts[3].texture_coords[0] = 0;
	verts[3].texture_coords[1] = 1;
	verts[3].vert_index = 3;

	GL_UpdateBufferSection(aliasModel_vbo, *position, sizeof(verts), verts);
	*position += sizeof(verts) / sizeof(verts[0]);
}

void GL_CreateModelVBOs(qbool vid_restart)
{
	extern model_t* Mod_LoadModel(model_t *mod, qbool crash);
	int required_vbo_length = 4;
	int i;
	int new_vbo_position = 0;
	buffer_ref instance_vbo;

	for (i = 1; i < MAX_MODELS; ++i) {
		model_t* mod = cl.model_precache[i];

		if (mod && (mod == cl.worldmodel || !mod->isworldmodel)) {
			if (!vid_restart && (mod->type == mod_alias || mod->type == mod_alias3)) {
				if (mod->vertsInVBO && !mod->temp_vbo_buffer) {
					// Invalidate cache so VBO buffer gets refilled
					Cache_Free(&mod->cache);
				}
			}
			Mod_LoadModel(mod, true);

			GL_MeasureVBOForModel(mod, &required_vbo_length);
		}
	}

	for (i = 0; i < MAX_VWEP_MODELS; i++) {
		model_t* mod = cl.vw_model_precache[i];

		if (mod) {
			if (!vid_restart && (mod->type == mod_alias || mod->type == mod_alias3)) {
				if (mod->vertsInVBO && !mod->temp_vbo_buffer) {
					// Invalidate cache so VBO buffer gets refilled
					Cache_Free(&mod->cache);
				}
			}
			Mod_LoadModel(mod, true);

			GL_MeasureVBOForModel(mod, &required_vbo_length);
		}
	}

	aliasModel_vbo = GL_GenFixedBuffer(GL_ARRAY_BUFFER, "aliasmodel-vertex-data", required_vbo_length * sizeof(vbo_model_vert_t), NULL, GL_STATIC_DRAW);
	if (GL_ShadersSupported()) {
		aliasModel_ssbo = GL_GenFixedBuffer(GL_SHADER_STORAGE_BUFFER, "aliasmodel-vertex-ssbo", required_vbo_length * sizeof(vbo_model_vert_t), NULL, GL_STATIC_COPY);
	}

	// VBO starts with simple-model/sprite vertices
	GL_ImportSpriteCoordsToVBO(&new_vbo_position);

	// Go back through all models, importing textures into arrays and creating new VBO
	for (i = 1; i < MAX_MODELS; ++i) {
		model_t* mod = cl.model_precache[i];

		if (mod) {
			GL_ImportModelToVBO(mod, &new_vbo_position);
		}
	}

	for (i = 0; i < MAX_VWEP_MODELS; i++) {
		model_t* mod = cl.vw_model_precache[i];

		if (mod) {
			GL_ImportModelToVBO(mod, &new_vbo_position);
		}
	}

	instance_vbo = GL_CreateInstanceVBO();
	GL_CreateAliasModelVAO(aliasModel_vbo, instance_vbo);
	GL_CreateBrushModelVAO(instance_vbo);
	if (GL_ShadersSupported()) {
		GL_BindBufferBase(aliasModel_ssbo, EZQ_GL_BINDINGPOINT_ALIASMODEL_SSBO);
	}
	else {
		GLC_AllocateAliasPoseBuffer();
	}
}
