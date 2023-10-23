/*
Copyright (C) 2018 ezQuake team

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

// glm_texture_arrays.c
#ifdef RENDERER_OPTION_MODERN_OPENGL

// gathers textures of common size into arrays so we can pack more references into fewer samplers

#include "quakedef.h"
#include "vx_stuff.h"
#include "vx_tracker.h"
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"
#ifndef  __APPLE__
#include "tr_types.h"
#endif
#include "glm_texture_arrays.h"
#include "r_texture.h"
#include "r_chaticons.h"
#include "gl_texture_internal.h"
#include "r_renderer.h"

static const texture_array_ref_t zero_array_ref[TEXTURETYPES_COUNT];
static texture_flag_t texture_flags[MAX_GLTEXTURES];
static int flagged_type = 0;

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

	if (!R_TextureReferenceIsValid(tex1->ref)) {
		if (!R_TextureReferenceIsValid(tex2->ref)) {
			return 0;
		}
		return 1;
	}
	else if (!R_TextureReferenceIsValid(tex2->ref)) {
		return -1;
	}

	width_diff = R_TextureWidth(tex1->ref) - R_TextureWidth(tex2->ref);
	height_diff = R_TextureHeight(tex1->ref) - R_TextureHeight(tex2->ref);

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

static int SortTexturesByArrayReference(const void* lhs_, const void* rhs_)
{
	texture_flag_t* tex1 = (texture_flag_t*)lhs_;
	texture_flag_t* tex2 = (texture_flag_t*)rhs_;

	int zero1 = !memcmp(tex1->array_ref, zero_array_ref, sizeof(zero_array_ref));
	int zero2 = !memcmp(tex2->array_ref, zero_array_ref, sizeof(zero_array_ref));

	if (zero1 && !zero2) {
		return 1;
	}
	else if (!zero1 && zero2) {
		return -1;
	}
	return 0;
}

static void GL_DeleteExistingTextureArrays(qbool delete_textures)
{
	int i;

	if (delete_textures) {
		int j;

		for (i = 0; i < MAX_GLTEXTURES; ++i) {
			if (!memcmp(zero_array_ref, texture_flags[i].array_ref, sizeof(zero_array_ref))) {
				break;
			}

			for (j = 0; j < TEXTURETYPES_COUNT; ++j) {
				if (R_TextureReferenceIsValid(texture_flags[i].array_ref[j].ref)) {
					R_DeleteTextureArray(&texture_flags[i].array_ref[j].ref);
				}
			}
		}
	}

	memset(texture_flags, 0, sizeof(texture_flags));
	for (i = 0; i < MAX_GLTEXTURES; ++i) {
		texture_flags[i].ref.index = i;
	}
}

static qbool GL_SkipTexture(model_t* mod, texture_t* tx);

static GLubyte* tempTextureBuffer;
static GLubyte* emptyTextureBuffer;
static GLuint tempTextureBufferSize;
static GLuint emptyTextureBufferSize;

/*
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
*/

void GL_AddTextureToArray(texture_ref arrayTexture, int index, texture_ref tex2dname, qbool tile)
{
	int width = R_TextureWidth(tex2dname);
	int height = R_TextureHeight(tex2dname);
	int final_width = R_TextureWidth(arrayTexture);
	int final_height = R_TextureHeight(arrayTexture);

	int ratio_x = tile ? final_width / width : 0;
	int ratio_y = tile ? final_height / height : 0;
	int x, y;

	if (ratio_x == 0) {
		ratio_x = 1;
	}
	if (ratio_y == 0) {
		ratio_y = 1;
	}

	if (tempTextureBufferSize < width * height * 4 * sizeof(GLubyte)) {
		Q_free(tempTextureBuffer);
		tempTextureBufferSize = width * height * 4 * sizeof(GLubyte);
		tempTextureBuffer = Q_malloc(tempTextureBufferSize);
	}

	if (emptyTextureBufferSize < final_width * final_height * 4 * sizeof(GLubyte)) {
		Q_free(emptyTextureBuffer);
		emptyTextureBufferSize = final_width * final_height * 4 * sizeof(GLubyte);
		emptyTextureBuffer = Q_malloc(emptyTextureBufferSize);
	}

	GL_GetTexImage(GL_TEXTURE0, tex2dname, 0, GL_RGBA, GL_UNSIGNED_BYTE, tempTextureBufferSize, tempTextureBuffer);

	// Clear
	GL_TexSubImage3D(0, arrayTexture, 0, 0, 0, index, final_width, final_height, 1, GL_RGBA, GL_UNSIGNED_BYTE, emptyTextureBuffer);

	// Might need to tile multiple times
	for (x = 0; x < ratio_x; ++x) {
		for (y = 0; y < ratio_y; ++y) {
			GL_TexSubImage3D(0, arrayTexture, 0, x * width, y * height, index, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, tempTextureBuffer);
		}
	}
}

static void GL_FlagTexturesForModel(model_t* mod)
{
	int j;

	switch (mod->type) {
	case mod_alias:
	case mod_alias3:
		for (j = 0; j < MAX_SIMPLE_TEXTURES; ++j) {
			if (R_TextureReferenceIsValid(mod->simpletexture[j])) {
				texture_flags[mod->simpletexture[j].index].flags |= (1 << TEXTURETYPES_SPRITES);
			}
		}
		break;
	case mod_sprite:
		{
			msprite2_t* psprite = (msprite2_t*)Mod_Extradata(mod);

			for (j = 0; j < psprite->numframes; ++j) {
				int offset    = psprite->frames[j].offset;
				int numframes = psprite->frames[j].numframes;
				mspriteframe_t* frame;

				if (offset < (int)sizeof(msprite2_t) || numframes < 1) {
					continue;
				}

				frame = ((mspriteframe_t*)((byte*)psprite + offset));
				if (R_TextureReferenceIsValid(frame->gl_texturenum)) {
					texture_flags[frame->gl_texturenum.index].flags |= (1 << TEXTURETYPES_SPRITES);
				}
			}
			break;
		}
	case mod_brush:
		{
			int i, j;

			// Ammo-boxes etc can be replaced with simple textures
			for (j = 0; j < MAX_SIMPLE_TEXTURES; ++j) {
				if (R_TextureReferenceIsValid(mod->simpletexture[j])) {
					texture_flags[mod->simpletexture[j].index].flags |= (1 << TEXTURETYPES_SPRITES);
				}
			}

			// Brush models can be boxes (ammo, health), static world or moving platforms
			for (i = 0; i < mod->numtextures; i++) {
				texture_t* tx = mod->textures[i];
				texture_type type = mod->isworldmodel ? TEXTURETYPES_WORLDMODEL : TEXTURETYPES_BRUSHMODEL;

				if (GL_SkipTexture(mod, tx) || !R_TextureReferenceIsValid(tx->gl_texturenum)) {
					continue;
				}

				if (R_TextureReferenceIsValid(tx->fb_texturenum) && !R_TexturesAreSameSize(tx->gl_texturenum, tx->fb_texturenum)) {
					Con_Printf("Warning: luma texture mismatch: %s (%dx%d vs %dx%d)\n", tx->name, R_TextureWidth(tx->gl_texturenum), R_TextureHeight(tx->gl_texturenum), R_TextureWidth(tx->fb_texturenum), R_TextureHeight(tx->fb_texturenum));
					R_TextureReferenceInvalidate(tx->fb_texturenum);
				}

				texture_flags[tx->gl_texturenum.index].flags |= (1 << type);
				if (R_TextureReferenceIsValid(tx->fb_texturenum)) {
					texture_flags[tx->gl_texturenum.index].subsequent = 1;
				}
			}
			break;
		}
	case mod_unknown:
		// Keep compiler happy
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

		if (!tex || !tex->loaded || !R_TextureReferenceIsValid(tex->gl_texture_array)) {
			continue;
		}

		for (j = 0; j < i; ++j) {
			texture_t* prev_tex = mod->textures[j];
			if (prev_tex && R_TextureReferenceEqual(prev_tex->gl_texture_array, tex->gl_texture_array)) {
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
		if (!tex->loaded || !R_TextureReferenceIsValid(tex->gl_texture_array)) {
			continue;
		}

		for (j = i - 1; j >= 0; --j) {
			texture_t* prev_tex = mod->textures[j];
			if (prev_tex && R_TextureReferenceEqual(prev_tex->gl_texture_array, tex->gl_texture_array)) {
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

static void GL_ImportTexturesForModel(model_t* mod)
{
	int j;

	for (j = 0; j < MAX_SIMPLE_TEXTURES; ++j) {
		if (R_TextureReferenceIsValid(mod->simpletexture[j])) {
			texture_array_ref_t* array_ref = &texture_flags[mod->simpletexture[j].index].array_ref[TEXTURETYPES_SPRITES];

			mod->simpletexture_array[j] = array_ref->ref;
			mod->simpletexture_indexes[j] = array_ref->index;
			mod->simpletexture_scalingS[j] = array_ref->scale_s;
			mod->simpletexture_scalingT[j] = array_ref->scale_t;
		}
	}

	if (mod->type == mod_sprite) {
		msprite2_t* psprite = (msprite2_t*)Mod_Extradata(mod);

		for (j = 0; j < psprite->numframes; ++j) {
			int offset    = psprite->frames[j].offset;
			int numframes = psprite->frames[j].numframes;
			mspriteframe_t* frame;

			if (offset < (int)sizeof(msprite2_t) || numframes < 1) {
				continue;
			}

			frame = ((mspriteframe_t*)((byte*)psprite + offset));

			if (R_TextureReferenceIsValid(frame->gl_texturenum)) {
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

			if (GL_SkipTexture(mod, tx) || !R_TextureReferenceIsValid(tx->gl_texturenum)) {
				continue;
			}

			array_ref = &texture_flags[tx->gl_texturenum.index].array_ref[type];
			tx->gl_texture_array = array_ref->ref;
			tx->gl_texture_index = array_ref->index;
			tx->gl_texture_scaleS = array_ref->scale_s;
			tx->gl_texture_scaleT = array_ref->scale_t;

			// fb has to be the same size and will be subsequent
			if (R_TextureReferenceIsValid(tx->fb_texturenum)) {
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

static int GLM_FindPotentialSizes(int i, int* potential_sizes, int flagged_type, qbool maximise, int* req_width, int* req_height)
{
	int size_index = 0;
	int j;
	int width = R_TextureWidth(texture_flags[i].ref);
	int height = R_TextureHeight(texture_flags[i].ref);

	potential_sizes[size_index++] = 1 + texture_flags[i].subsequent;

	// Count how many textures of the same size we have
	for (j = i + 1; j < MAX_GLTEXTURES; ++j) {
		if (!(texture_flags[j].flags & (1 << flagged_type))) {
			break;
		}

		if (maximise) {
			width = max(width, R_TextureWidth(texture_flags[j].ref));
			height = max(height, R_TextureHeight(texture_flags[j].ref));
		}
		else if (R_TextureWidth(texture_flags[j].ref) != width || R_TextureHeight(texture_flags[j].ref) != height) {
			break;
		}

		potential_sizes[size_index] = potential_sizes[size_index - 1] + 1 + texture_flags[j].subsequent;
		++size_index;
	}

	*req_width = width;
	*req_height = height;
	return size_index;
}

static void GLM_CopyTexturesToArray(texture_ref array_ref, int flagged_type, int min_index, int max_index, int width, int height)
{
	int array_index = 0;
	int k;

	// texture created ok
	if (flagged_type == TEXTURETYPES_SPRITES) {
		renderer.TextureWrapModeClamp(array_ref);
	}

	// Copy the 2D textures across
	for (k = min_index; k <= max_index; ++k) {
		texture_ref ref_2d = texture_flags[k].ref;

		// TODO: compression: flag as ANYSIZE and set scale_s/scale_t accordingly
		GL_AddTextureToArray(array_ref, array_index, ref_2d, false);
		texture_flags[k].array_ref[flagged_type].ref = array_ref;
		texture_flags[k].array_ref[flagged_type].index = array_index;
		texture_flags[k].array_ref[flagged_type].scale_s = (R_TextureWidth(ref_2d) * 1.0f) / width;
		texture_flags[k].array_ref[flagged_type].scale_t = (R_TextureHeight(ref_2d) * 1.0f) / height;

		// (subsequent are the luma textures on brush models)
		array_index += 1 + texture_flags[k].subsequent;
	}
}

static int GLM_CreateArrayFromPotentialSizes(int i, int* potential_sizes, int size_index_max, qbool return_on_failure, int width, int height)
{
	int size_attempt, depth;

	const char* textureTypeNames[] = {
		"aliasmodel",
		"brushmodel",
		"worldmodel",
		"sprites",
	};

	for (size_attempt = size_index_max - 1; size_attempt >= 0; --size_attempt) {
		// create array of desired size
		char name[64];
		texture_ref array_ref;

		R_TextureReferenceInvalidate(array_ref);
		depth = potential_sizes[size_attempt];
		snprintf(name, sizeof(name), "%s_%d[%dx%dx%d]", textureTypeNames[flagged_type], i, width, height, depth);

		array_ref = R_CreateTextureArray(name, width, height, depth, TEX_MIPMAP | TEX_ALPHA);
		if (R_TextureReferenceIsValid(array_ref)) {
			GLM_CopyTexturesToArray(array_ref, flagged_type, i, i + size_attempt, width, height);
			return size_attempt;
		}
	}

	if (!return_on_failure) {
		Sys_Error("Failed to create array size %dx%dx%d\n", width, height, potential_sizes[size_index_max - 1]);
	}
	return -1;
}

// Called from R_NewMap
void GLM_BuildCommonTextureArrays(qbool vid_restart)
{
	int i;
	int potential_sizes[MAX_GLTEXTURES];

	GL_DeleteExistingTextureArrays(!vid_restart);
	R_ClearModelTextureData();

	if (cls.state == ca_disconnected) {
		return;
	}

	for (i = 1; i < MAX_MODELS; ++i) {
		model_t* mod = cl.model_precache[i];

		if (mod && (mod == cl.worldmodel || !mod->isworldmodel)) {
			GL_FlagTexturesForModel(mod);
		}
	}

	for (i = 0; i < MAX_VWEP_MODELS; i++) {
		model_t* mod = cl.vw_model_precache[i];

		if (mod) {
			GL_FlagTexturesForModel(mod);
		}
	}

	// custom models are explicitly loaded by client, not notified by server
	for (i = 0; i < custom_model_count; ++i) {
		model_t* mod = Mod_CustomModel(i, false);

		if (mod) {
			GL_FlagTexturesForModel(mod);
		}
	}

	// Add non-model textures we need (generally sprites)
	{
		QMB_FlagTexturesForArray(texture_flags);
		R_FlagChatIconTexturesForArray(texture_flags);
		VX_FlagTexturesForArray(texture_flags);
		Part_FlagTexturesForArray(texture_flags);
	}

	for (flagged_type = 0; flagged_type < TEXTURETYPES_COUNT; ++flagged_type) {
		qsort(texture_flags, MAX_GLTEXTURES, sizeof(texture_flags[0]), SortFlaggedTextures);

		for (i = 0; i < MAX_GLTEXTURES; ++i) {
			int size_index_max = 0;
			int extra_slots_added;
			int req_width, req_height;

			if (!(texture_flags[i].flags & (1 << flagged_type))) {
				continue;
			}

			size_index_max = GLM_FindPotentialSizes(i, potential_sizes, flagged_type, flagged_type == TEXTURETYPES_SPRITES, &req_width, &req_height);
			extra_slots_added = GLM_CreateArrayFromPotentialSizes(i, potential_sizes, size_index_max, flagged_type == TEXTURETYPES_SPRITES, req_width, req_height);
			if (extra_slots_added == -1) {
				// try again without maximising
				GLM_FindPotentialSizes(i, potential_sizes, flagged_type, false, &req_width, &req_height);
				extra_slots_added = GLM_CreateArrayFromPotentialSizes(i, potential_sizes, size_index_max, false, req_width, req_height);
			}
			i += extra_slots_added;
		}
	}

	qsort(texture_flags, MAX_GLTEXTURES, sizeof(texture_flags[0]), SortTexturesByReference);

	{
		QMB_ImportTextureArrayReferences(texture_flags);
		R_ImportChatIconTextureArrayReferences(texture_flags);
		VX_ImportTextureArrayReferences(texture_flags);
		Part_ImportTexturesForArrayReferences(texture_flags);

		// Go back through all models, importing textures into arrays and creating new VBO
		for (i = 1; i < MAX_MODELS; ++i) {
			model_t* mod = cl.model_precache[i];

			if (mod) {
				GL_ImportTexturesForModel(mod);
			}
		}

		for (i = 0; i < MAX_VWEP_MODELS; i++) {
			model_t* mod = cl.vw_model_precache[i];

			if (mod) {
				GL_ImportTexturesForModel(mod);
			}
		}

		// custom models are explicitly loaded by client, not notified by server
		for (i = 0; i < custom_model_count; ++i) {
			model_t* mod = Mod_CustomModel(i, false);

			if (mod) {
				GL_ImportTexturesForModel(mod);
			}
		}
	}

	// 2nd pass through the models should have filled the arrays, so can create mipmaps now
	qsort(texture_flags, MAX_GLTEXTURES, sizeof(texture_flags[0]), SortTexturesByArrayReference);

	for (i = 0; i < MAX_GLTEXTURES; ++i) {
		int j;
		qbool any = false;

		for (j = 0; j < TEXTURETYPES_COUNT; ++j) {
			if (R_TextureReferenceIsValid(texture_flags[i].array_ref[j].ref)) {
				R_GenerateMipmapsIfNeeded(texture_flags[i].array_ref[j].ref);

				if (j == TEXTURETYPES_BRUSHMODEL) {
					R_DeleteTexture(&texture_flags[i].ref);
				}
				any = true;
			}
		}

		if (!any) {
			break;
		}
	}

	Q_free(tempTextureBuffer);
	Q_free(emptyTextureBuffer);
	emptyTextureBufferSize = tempTextureBufferSize = 0;
}

#endif // #ifdef RENDERER_OPTION_MODERN_OPENGL
