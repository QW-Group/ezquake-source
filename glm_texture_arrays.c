
#include "quakedef.h"
#include "vx_stuff.h"
#include "vx_tracker.h"
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"
#ifndef  __APPLE__
#include "tr_types.h"
#endif

void GL_MD3ModelAddToVBO(model_t* mod, buffer_ref vbo, buffer_ref ssbo, int position);

#define MAX_ARRAY_DEPTH 64
#define VBO_ALIASVERT_FOFS(x) (void*)((intptr_t)&(((vbo_model_vert_t*)0)->x))

typedef enum {
	TEXTURETYPES_ALIASMODEL,
	TEXTURETYPES_BRUSHMODEL,
	TEXTURETYPES_WORLDMODEL,
	TEXTURETYPES_SPRITES,

	TEXTURETYPES_COUNT
} texture_type;

typedef struct texture_array_ref_s {
	texture_ref ref;
	int index;
	float scale_s;
	float scale_t;
} texture_array_ref_t;

typedef struct texture_flag_s {
	texture_ref ref;
	int subsequent;
	int flags;

	texture_array_ref_t array_ref[TEXTURETYPES_COUNT];
} texture_flag_t;

static texture_flag_t texture_flags[MAX_GLTEXTURES];
static int flagged_type = 0;

static void GL_ClearTextureFlags(void)
{
	int i;

	memset(texture_flags, 0, sizeof(texture_flags));
	for (i = 0; i < MAX_GLTEXTURES; ++i) {
		texture_flags[i].ref.index = i;
	}
}

static int SortFlaggedTextures(const void* lhs_, const void* rhs_)
{
	int width_diff;
	int height_diff;
	texture_flag_t* tex1 = (texture_flag_t*)lhs_;
	texture_flag_t* tex2 = (texture_flag_t*)rhs_;

	qbool included1 = tex1->flags & (1 << flagged_type);
	qbool included2 = tex2->flags & (1 << flagged_type);

	if (included1 && !included2) {
		return -1;
	}
	else if (!included1 && included2) {
		return 1;
	}

	if (!GL_TextureReferenceIsValid(tex1->ref)) {
		if (!GL_TextureReferenceIsValid(tex2->ref)) {
			return 0;
		}
		return 1;
	}
	else if (!GL_TextureReferenceIsValid(tex2->ref)) {
		return -1;
	}

	width_diff = GL_TextureWidth(tex1->ref) - GL_TextureWidth(tex2->ref);
	height_diff = GL_TextureHeight(tex1->ref) - GL_TextureHeight(tex2->ref);

	if (width_diff) {
		return width_diff;
	}
	if (height_diff) {
		return height_diff;
	}

	return 0;
}

static int SortTexturesByReference(const void* lhs_, const void* rhs_)
{
	texture_flag_t* tex1 = (texture_flag_t*)lhs_;
	texture_flag_t* tex2 = (texture_flag_t*)rhs_;

	if (tex1->ref.index < tex2->ref.index) {
		return -1;
	}
	return 1;
}

static void GLM_CreateAliasModelVAO(void);
static qbool GL_SkipTexture(model_t* mod, texture_t* tx);

static GLubyte* tempBuffer;
static GLuint tempBufferSize;

static buffer_ref instance_vbo;
glm_vao_t aliasModel_vao;
buffer_ref aliasModel_vbo;
buffer_ref aliasModel_ssbo;

void GLM_CreateBrushModelVAO(buffer_ref instance_vbo);

static qbool AliasModelIsAnySize(model_t* mod)
{
	// Technically every alias model is any size?
	// Not putting alias models in an array at the moment just because player skins are a special case
	//   and can change dynamically due to all the different skin-override settings

	return false;
}

static qbool BrushModelIsAnySize(model_t* mod)
{
	// Some brush models texture repeat and others don't - if it doesn't repeat then we should be
	//   able to insert onto a larger texture and then adjust texture coordinates so it doesn't matter

	return false;
}

#define MAX_INSTANCES 128

static void GLM_CreateInstanceVBO(void)
{
	unsigned int values[MAX_INSTANCES];
	int i;

	for (i = 0; i < MAX_INSTANCES; ++i) {
		values[i] = i;
	}

	instance_vbo = GL_GenFixedBuffer(GL_ARRAY_BUFFER, "instance#", sizeof(values), values, GL_STATIC_DRAW);
}

static void GL_ClearModelTextureReferences(model_t* mod, qbool all_textures)
{
	int j;

	memset(mod->texture_arrays, 0, sizeof(mod->texture_arrays));
	memset(mod->texture_arrays_scale_s, 0, sizeof(mod->texture_arrays_scale_s));
	memset(mod->texture_arrays_scale_t, 0, sizeof(mod->texture_arrays_scale_t));
	memset(mod->texture_array_first, 0, sizeof(mod->texture_array_first));
	mod->texture_array_count = 0;

	memset(mod->simpletexture_scalingS, 0, sizeof(mod->simpletexture_scalingS));
	memset(mod->simpletexture_scalingT, 0, sizeof(mod->simpletexture_scalingT));
	memset(mod->simpletexture_indexes, 0, sizeof(mod->simpletexture_indexes));
	memset(mod->simpletexture_array, 0, sizeof(mod->simpletexture_array));

	if (all_textures) {
		memset(mod->simpletexture, 0, sizeof(mod->simpletexture));
	}

	// clear brush model data
	if (mod->type == mod_brush) {
		for (j = 0; j < mod->numtextures; ++j) {
			texture_t* tex = mod->textures[j];
			if (tex) {
				if (all_textures) {
					GL_TextureReferenceInvalidate(tex->fb_texturenum);
					GL_TextureReferenceInvalidate(tex->gl_texturenum);
				}
				tex->gl_texture_scaleS = tex->gl_texture_scaleT = 0;
				GL_TextureReferenceInvalidate(tex->gl_texture_array);
				tex->gl_texture_index = 0;
			}
		}
	}

	// clear sprite model data
	if (mod->type == mod_sprite) {
		msprite2_t* psprite = (msprite2_t*)Mod_Extradata(mod);
		int j;

		for (j = 0; j < psprite->numframes; ++j) {
			int offset = psprite->frames[j].offset;
			int numframes = psprite->frames[j].numframes;
			mspriteframe_t* frame;

			if (offset < (int)sizeof(msprite2_t) || numframes < 1) {
				continue;
			}

			frame = ((mspriteframe_t*)((byte*)psprite + offset));

			if (all_textures) {
				GL_TextureReferenceInvalidate(frame->gl_texturenum);
			}
			frame->gl_scalingS = frame->gl_scalingT = 0;
			GL_TextureReferenceInvalidate(frame->gl_arraynum);
			frame->gl_arrayindex = 0;
		}
	}

	if (mod->type == mod_alias && all_textures) {
		aliashdr_t* paliashdr = (aliashdr_t *)Mod_Extradata(mod);

		memset(paliashdr->gl_texturenum, 0, sizeof(paliashdr->gl_texturenum));
		memset(paliashdr->fb_texturenum, 0, sizeof(paliashdr->fb_texturenum));
	}
	else if (mod->type == mod_alias3 && all_textures) {
		md3model_t* md3Model = (md3model_t *)Mod_Extradata(mod);
		surfinf_t* surfaceInfo = MD3_ExtraSurfaceInfoForModel(md3Model);

		// One day there will be more than one of these...
		GL_TextureReferenceInvalidate(surfaceInfo->texnum);
	}
}

// Called during disconnect
void GL_DeleteModelData(void)
{
	int i;
	for (i = 0; i < MAX_MODELS; ++i) {
		if (cl.model_precache[i]) {
			GL_ClearModelTextureReferences(cl.model_precache[i], false);
		}
	}

	for (i = 0; i < MAX_VWEP_MODELS; ++i) {
		if (cl.vw_model_precache[i]) {
			GL_ClearModelTextureReferences(cl.vw_model_precache[i], false);
		}
	}
}

static void GL_AddTextureToArray(texture_ref arrayTexture, int index, texture_ref tex2dname, qbool tile)
{
	int width = GL_TextureWidth(tex2dname);
	int height = GL_TextureHeight(tex2dname);
	int final_width = GL_TextureWidth(arrayTexture);
	int final_height = GL_TextureHeight(arrayTexture);

	int ratio_x = tile ? final_width / width : 0;
	int ratio_y = tile ? final_height / height : 0;
	int x, y;

	if (ratio_x == 0) {
		ratio_x = 1;
	}
	if (ratio_y == 0) {
		ratio_y = 1;
	}

	if (tempBufferSize < width * height * 4 * sizeof(GLubyte)) {
		Q_free(tempBuffer);
		tempBufferSize = width * height * 4 * sizeof(GLubyte);
		tempBuffer = Q_malloc(tempBufferSize);
	}

	GL_GetTexImage(GL_TEXTURE0, tex2dname, 0, GL_RGBA, GL_UNSIGNED_BYTE, tempBufferSize, tempBuffer);
	GL_ProcessErrors(va("GL_AddTextureToArray(%u => %u)/glGetTexImage", tex2dname, arrayTexture));

	// Might need to tile multiple times
	for (x = 0; x < ratio_x; ++x) {
		for (y = 0; y < ratio_y; ++y) {
			GL_TexSubImage3D(GL_TEXTURE0, arrayTexture, 0, x * width, y * height, index, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, tempBuffer);
#ifdef GL_PARANOIA
			if (glGetError() != GL_NO_ERROR) {
				int array_width = GL_TextureWidth(arrayTexture);
				int array_height = GL_TextureHeight(arrayTexture);
				int array_depth = GL_TextureDepth(arrayTexture);

				GL_Paranoid_Printf("Failed to import texture %u to array %u[%d] (%d x %d x %d)\n", tex2dname, arrayTexture, index, array_width, array_height, array_depth);
				GL_Paranoid_Printf(">  Parameters: %d %d %d, %d %d 1, tempBuffer = %X\n", x * width, y * height, index, width, height, tempBuffer);
			}
#endif
		}
	}
}

// FIXME: Rename to signify that it is a single array for whole model
static void GL_SetModelTextureArray(model_t* mod, texture_ref array_num, float widthRatio, float heightRatio)
{
	mod->texture_array_count = 1;
	mod->texture_arrays[0] = array_num;
	mod->texture_arrays_scale_s[0] = widthRatio;
	mod->texture_arrays_scale_t[0] = heightRatio;
}

static void GL_FlagTexturesForModel(model_t* mod)
{
	int j;

	switch (mod->type) {
	case mod_alias:
	case mod_alias3:
		for (j = 0; j < MAX_SIMPLE_TEXTURES; ++j) {
			if (GL_TextureReferenceIsValid(mod->simpletexture[j])) {
				texture_flags[mod->simpletexture[j].index].flags |= (1 << TEXTURETYPES_SPRITES);
			}
		}
		break;
	case mod_sprite:
		{
			msprite2_t* psprite = (msprite2_t*)Mod_Extradata(mod);
			int count = 0;

			for (j = 0; j < psprite->numframes; ++j) {
				int offset    = psprite->frames[j].offset;
				int numframes = psprite->frames[j].numframes;
				mspriteframe_t* frame;

				if (offset < (int)sizeof(msprite2_t) || numframes < 1) {
					continue;
				}

				frame = ((mspriteframe_t*)((byte*)psprite + offset));
				if (GL_TextureReferenceIsValid(frame->gl_texturenum)) {
					texture_flags[frame->gl_texturenum.index].flags |= (1 << TEXTURETYPES_SPRITES);
				}
				++count;
			}
			break;
		}
	case mod_brush:
		{
			int i, j;

			// Ammo-boxes etc can be replaced with simple textures
			for (j = 0; j < MAX_SIMPLE_TEXTURES; ++j) {
				if (GL_TextureReferenceIsValid(mod->simpletexture[j])) {
					texture_flags[mod->simpletexture[j].index].flags |= (1 << TEXTURETYPES_SPRITES);
				}
			}

			// Brush models can be boxes (ammo, health), static world or moving platforms
			for (i = 0; i < mod->numtextures; i++) {
				texture_t* tx = mod->textures[i];
				texture_type type = mod->isworldmodel ? TEXTURETYPES_WORLDMODEL : TEXTURETYPES_BRUSHMODEL;

				if (GL_SkipTexture(mod, tx) || !GL_TextureReferenceIsValid(tx->gl_texturenum)) {
					continue;
				}

				if (GL_TextureReferenceIsValid(tx->fb_texturenum) && !GL_TexturesAreSameSize(tx->gl_texturenum, tx->fb_texturenum)) {
					GL_TextureReferenceInvalidate(tx->fb_texturenum);
				}

				texture_flags[tx->gl_texturenum.index].flags |= (1 << type);
				if (GL_TextureReferenceIsValid(tx->fb_texturenum)) {
					texture_flags[tx->gl_texturenum.index].subsequent = 1;
				}
			}
			break;
		}
	}
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
	}
}

static qbool GL_SkipTexture(model_t* mod, texture_t* tx)
{
	int j;

	if (!tx || !tx->loaded) {
		return true;
	}

	if (tx->anim_next) {
		return false;
	}

	for (j = 0; j < mod->numtexinfo; ++j) {
		if (mod->texinfo[j].texture == tx) {
			if (mod->texinfo[j].surfaces && !mod->texinfo[j].skippable) {
				return false;
			}
		}
	}

	return true;
}

static int GLM_CountTextureArrays(model_t* mod)
{
	int i, j;
	int num_arrays = 0;

	for (i = 0; i < mod->numtextures; ++i) {
		texture_t* tex = mod->textures[i];
		qbool seen_prev = false;

		if (!tex || !tex->loaded || !GL_TextureReferenceIsValid(tex->gl_texture_array)) {
			continue;
		}

		for (j = 0; j < i; ++j) {
			texture_t* prev_tex = mod->textures[j];
			if (prev_tex && GL_TextureReferenceEqual(prev_tex->gl_texture_array, tex->gl_texture_array)) {
				seen_prev = true;
				break;
			}
		}

		if (!seen_prev) {
			++num_arrays;
		}
	}

	return num_arrays;
}

static void GLM_SetTextureArrays(model_t* mod)
{
	int i, j;
	int num_arrays = 0;

	for (i = 0; i < mod->numtextures; ++i) {
		texture_t* tex = mod->textures[i];
		qbool seen_prev = false;

		if (!tex) {
			continue;
		}

		tex->next_same_size = -1;
		if (!tex->loaded || !GL_TextureReferenceIsValid(tex->gl_texture_array)) {
			continue;
		}

		for (j = i - 1; j >= 0; --j) {
			texture_t* prev_tex = mod->textures[j];
			if (prev_tex && GL_TextureReferenceEqual(prev_tex->gl_texture_array, tex->gl_texture_array)) {
				seen_prev = true;
				prev_tex->next_same_size = i;
				break;
			}
		}

		if (!seen_prev) {
			mod->texture_array_first[num_arrays] = i;
			mod->texture_arrays[num_arrays] = tex->gl_texture_array;
			mod->textures[i]->size_start = true;
			++num_arrays;
		}
	}
}

static void GL_ImportModelToVBO(model_t* mod, int* new_vbo_position)
{
	int count = 0;

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

static void GL_ImportTexturesForModel(model_t* mod)
{
	int count = 0;
	int j;

	for (j = 0; j < MAX_SIMPLE_TEXTURES; ++j) {
		if (GL_TextureReferenceIsValid(mod->simpletexture[j])) {
			texture_array_ref_t* array_ref = &texture_flags[mod->simpletexture[j].index].array_ref[TEXTURETYPES_SPRITES];

			mod->simpletexture_array[j] = array_ref->ref;
			mod->simpletexture_indexes[j] = array_ref->index;
			mod->simpletexture_scalingS[j] = array_ref->scale_s;
			mod->simpletexture_scalingT[j] = array_ref->scale_t;
		}
	}

	if (mod->type == mod_sprite) {
		msprite2_t* psprite = (msprite2_t*)Mod_Extradata(mod);
		int count = 0;

		for (j = 0; j < psprite->numframes; ++j) {
			int offset    = psprite->frames[j].offset;
			int numframes = psprite->frames[j].numframes;
			mspriteframe_t* frame;

			if (offset < (int)sizeof(msprite2_t) || numframes < 1) {
				continue;
			}

			frame = ((mspriteframe_t*)((byte*)psprite + offset));

			if (GL_TextureReferenceIsValid(frame->gl_texturenum)) {
				texture_array_ref_t* array_ref = &texture_flags[frame->gl_texturenum.index].array_ref[TEXTURETYPES_SPRITES];

				frame->gl_arraynum = array_ref->ref;
				frame->gl_arrayindex = array_ref->index;
				frame->gl_scalingS = array_ref->scale_s;
				frame->gl_scalingT = array_ref->scale_t;
			}
		}
	}
	else if (mod->type == mod_brush) {
		texture_type type = mod->isworldmodel ? TEXTURETYPES_WORLDMODEL : TEXTURETYPES_BRUSHMODEL;
		int i;

		for (i = 0; i < mod->numtextures; i++) {
			texture_t* tx = mod->textures[i];
			texture_array_ref_t* array_ref;

			if (GL_SkipTexture(mod, tx) || !GL_TextureReferenceIsValid(tx->gl_texturenum)) {
				continue;
			}

			array_ref = &texture_flags[tx->gl_texturenum.index].array_ref[type];
			tx->gl_texture_array = array_ref->ref;
			tx->gl_texture_index = array_ref->index;
			tx->gl_texture_scaleS = array_ref->scale_s;
			tx->gl_texture_scaleT = array_ref->scale_t;

			// fb has to be the same size and will be subsequent
			if (GL_TextureReferenceIsValid(tx->fb_texturenum)) {
				GL_AddTextureToArray(array_ref->ref, array_ref->index + 1, tx->fb_texturenum, false);

				// flag so we delete the 2d version later
				texture_flags[tx->fb_texturenum.index].flags |= (1 << type);
			}
		}

		mod->texture_array_count = GLM_CountTextureArrays(mod);
		if (mod->texture_array_count >= MAX_TEXTURE_ARRAYS_PER_MODEL) {
			// FIXME: Simply fail & fallback to simple textures... or use allocated memory, don't crash the client
			Sys_Error("Model %s has >= %d texture dimensions", mod->name, MAX_TEXTURE_ARRAYS_PER_MODEL);
			return;
		}

		GLM_SetTextureArrays(mod);
	}
}

static void GLM_CreateSpriteVBO(void)
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

	GL_UpdateVBOSection(aliasModel_vbo, 0, sizeof(verts), verts);
}

// Called from R_NewMap
void GL_BuildCommonTextureArrays(qbool vid_restart)
{
	extern model_t* Mod_LoadModel(model_t *mod, qbool crash);
	int required_vbo_length = 4;
	int i;

	tempBufferSize = 0;

	//GL_DeleteExistingArrays(!vid_restart);
	GL_DeleteModelData();

	GL_ClearTextureFlags();
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
			GL_FlagTexturesForModel(mod);
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
			GL_FlagTexturesForModel(mod);
		}
	}

	// Add non-model textures we need (generally sprites)
	{
		// TODO
	}

	for (flagged_type = 0; flagged_type < TEXTURETYPES_COUNT; ++flagged_type) {
		qsort(texture_flags, MAX_GLTEXTURES, sizeof(texture_flags[0]), SortFlaggedTextures);

		for (i = 0; i < MAX_GLTEXTURES; ++i) {
			if (!(texture_flags[i].flags & (1 << flagged_type))) {
				continue;
			}
		}

		for (i = 0; i < MAX_GLTEXTURES; ++i) {
			int base = i;
			int same_size = 0;
			int width, height;
			int j;
			int depth;

			if (!(texture_flags[i].flags & (1 << flagged_type))) {
				continue;
			}

			width = GL_TextureWidth(texture_flags[i].ref);
			height = GL_TextureHeight(texture_flags[i].ref);
			same_size = 1 + texture_flags[i].subsequent;

			// Count how many textures of the same size we have
			for (j = i + 1; j < MAX_GLTEXTURES; ++j) {
				if (!(texture_flags[j].flags & (1 << flagged_type))) {
					break;
				}

				if (flagged_type == TEXTURETYPES_SPRITES) {
					width = max(width, GL_TextureWidth(texture_flags[j].ref));
					height = max(height, GL_TextureHeight(texture_flags[j].ref));
				}
				else if (GL_TextureWidth(texture_flags[j].ref) != width || GL_TextureHeight(texture_flags[j].ref) != height) {
					break;
				}

				same_size += 1 + texture_flags[j].subsequent;
			}

			// Create an array of that size
			while (same_size > 0) {
				texture_ref array_ref;
				int index = 0;
				int k;

				depth = same_size;
				array_ref = GL_CreateTextureArray("", width, height, &depth, TEX_MIPMAP | TEX_ALPHA, 1);

				// Copy the 2D textures across
				for (k = i; k < j; ++k) {
					texture_ref ref_2d = texture_flags[k].ref;

					// TODO: compression: flag as ANYSIZE and set scale_s/scale_t accordingly
					GL_AddTextureToArray(array_ref, index, ref_2d, false);
					texture_flags[k].array_ref[flagged_type].ref = array_ref;
					texture_flags[k].array_ref[flagged_type].index = index++;
					texture_flags[k].array_ref[flagged_type].scale_s = (GL_TextureWidth(ref_2d) * 1.0f) / width;
					texture_flags[k].array_ref[flagged_type].scale_t = (GL_TextureHeight(ref_2d) * 1.0f) / height;

					// Skip these and fill them in later
					index += texture_flags[k].subsequent;
				}

				//GL_GenerateMipmapsIfNeeded(array_ref);
				same_size -= depth;
				i = j;
			}

			i--;
		}
	}

	qsort(texture_flags, MAX_GLTEXTURES, sizeof(texture_flags[0]), SortTexturesByReference);

	{
		int new_vbo_position = 0;

		aliasModel_vbo = GL_GenFixedBuffer(GL_ARRAY_BUFFER, "aliasmodel-vertex-data", required_vbo_length * sizeof(vbo_model_vert_t), NULL, GL_STATIC_DRAW);
		aliasModel_ssbo = GL_GenFixedBuffer(GL_SHADER_STORAGE_BUFFER, "aliasmodel-vertex-ssbo", required_vbo_length * sizeof(vbo_model_vert_t), NULL, GL_STATIC_COPY);

		// VBO starts with simple-model/sprite vertices
		GLM_CreateSpriteVBO();
		new_vbo_position = 4;

		// Go back through all models, importing textures into arrays and creating new VBO
		for (i = 1; i < MAX_MODELS; ++i) {
			model_t* mod = cl.model_precache[i];

			if (mod) {
				GL_ImportTexturesForModel(mod);
				GL_ImportModelToVBO(mod, &new_vbo_position);
			}
		}

		for (i = 0; i < MAX_VWEP_MODELS; i++) {
			model_t* mod = cl.vw_model_precache[i];

			if (mod) {
				GL_ImportTexturesForModel(mod);
				GL_ImportModelToVBO(mod, &new_vbo_position);
			}
		}

		GLM_CreateInstanceVBO();
		GLM_CreateAliasModelVAO();
		GLM_CreateBrushModelVAO(instance_vbo);
		GL_BindBufferBase(aliasModel_ssbo, GL_BINDINGPOINT_ALIASMODEL_SSBO);
	}

	// 2nd pass through the models should have filled the arrays, so can create mipmaps now
	for (i = 0; i < MAX_GLTEXTURES; ++i) {
		int j;

		for (j = 0; j < TEXTURETYPES_COUNT; ++j) {
			qbool any = false;

			if (GL_TextureReferenceIsValid(texture_flags[i].array_ref[j].ref)) {
				GL_GenerateMipmapsIfNeeded(texture_flags[i].array_ref[j].ref);

				any = true;
			}

			if (any) {
				// Delete the original texture
				GL_DeleteTexture(&texture_flags[i].ref);
			}
		}
	}

	Q_free(tempBuffer);
	tempBufferSize = 0;
}

static void GLM_CreateAliasModelVAO(void)
{
	GL_GenVertexArray(&aliasModel_vao);

	GL_ConfigureVertexAttribPointer(&aliasModel_vao, aliasModel_vbo, 0, 3, GL_FLOAT, GL_FALSE, sizeof(vbo_model_vert_t), VBO_ALIASVERT_FOFS(position));
	GL_ConfigureVertexAttribPointer(&aliasModel_vao, aliasModel_vbo, 1, 2, GL_FLOAT, GL_FALSE, sizeof(vbo_model_vert_t), VBO_ALIASVERT_FOFS(texture_coords));
	GL_ConfigureVertexAttribPointer(&aliasModel_vao, aliasModel_vbo, 2, 3, GL_FLOAT, GL_FALSE, sizeof(vbo_model_vert_t), VBO_ALIASVERT_FOFS(normal));
	GL_ConfigureVertexAttribIPointer(&aliasModel_vao, instance_vbo, 3, 1, GL_UNSIGNED_INT, sizeof(GLuint), 0);
	GL_ConfigureVertexAttribIPointer(&aliasModel_vao, aliasModel_vbo, 4, 1, GL_INT, sizeof(vbo_model_vert_t), VBO_ALIASVERT_FOFS(vert_index));

	glVertexAttribDivisor(3, 1);
}

void GL_InvalidateAllTextureReferences(void)
{
	int i;

	for (i = 1; i < MAX_MODELS; ++i) {
		model_t* mod = cl.model_precache[i];

		if (mod) {
			GL_ClearModelTextureReferences(mod, true);
		}
	}

	for (i = 0; i < MAX_VWEP_MODELS; i++) {
		model_t* mod = cl.vw_model_precache[i];

		if (mod) {
			GL_ClearModelTextureReferences(mod, true);
		}
	}
}
