
#include "quakedef.h"
#include "vx_stuff.h"
#include "vx_tracker.h"
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"
#ifndef  __APPLE__
#include "tr_types.h"
#endif

void GL_MD3ModelAddToVBO(model_t* mod, glm_vbo_t* vbo, glm_vbo_t* ssbo, int position);

#define MAX_ARRAY_DEPTH 64
#define VBO_ALIASVERT_FOFS(x) (void*)((intptr_t)&(((vbo_model_vert_t*)0)->x))

typedef struct common_texture_registration_s {
	texture_ref original;   // The original texture to be moved into first reserved element of array

	texture_ref* array_ref; // Will be set to the array allocated
	int* array_indices;     // Indices into array
	float* array_scalingS;  // If moved onto a larger texture, adjust texture coordinates
	float* array_scalingT;  // "

	int count;              // Number of slices of the array that need to be reserved (only the first is inserted)
	qbool allocated;
	qbool any_size;
} common_texture_registration_t;

typedef struct common_texture_s {
	int width;
	int height;

	int count;
	int any_size_count;

	texture_ref gl_texturenum;
	int gl_width;
	int gl_height;
	int gl_depth;

	int allocated;

	struct common_texture_s* next;
	struct common_texture_s* final;

	struct common_texture_s* overflow;

	common_texture_registration_t registrations[MAX_ARRAY_DEPTH];
} common_texture_t;

typedef enum {
	TEXTURETYPES_ALIASMODEL,
	TEXTURETYPES_BRUSHMODEL,
	TEXTURETYPES_WORLDMODEL,
	TEXTURETYPES_SPRITES,

	TEXTURETYPES_COUNT
} texture_type;

static void GLM_CreateAliasModelVAO(void);
static void GL_PrintTextureSizes(common_texture_t* list);
static common_texture_t* GL_AppendOverflowTextureAllocation(common_texture_t* tex, int allocation_size);
static qbool GL_SkipTexture(model_t* mod, texture_t* tx);

static GLubyte* tempBuffer;
static GLuint tempBufferSize;

static glm_vbo_t instance_vbo;
glm_vao_t aliasModel_vao;
glm_vbo_t aliasModel_vbo;
glm_vbo_t aliasModel_ssbo;

static common_texture_t* commonTextures[TEXTURETYPES_COUNT];

void GLM_CreateBrushModelVAO(glm_vbo_t* instance_vbo);

static void GL_DeleteExistingArrays(qbool delete_textures)
{
	int type;

	for (type = 0; type < TEXTURETYPES_COUNT; ++type) {
		common_texture_t* tex;
		common_texture_t* next;

		for (tex = commonTextures[type]; tex; tex = next) {
			next = tex->next;
			if (delete_textures && GL_TextureReferenceIsValid(tex->gl_texturenum)) {
				GL_DeleteTextureArray(&tex->gl_texturenum);
			}
			Q_free(tex);
		}
	}

	memset(commonTextures, 0, sizeof(commonTextures));
}

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

#define MAX_INSTANCES 64

static void GLM_CreateInstanceVBO(void)
{
	unsigned int values[MAX_INSTANCES];
	int i;

	for (i = 0; i < MAX_INSTANCES; ++i) {
		values[i] = i;
	}

	GL_GenFixedBuffer(&instance_vbo, GL_ARRAY_BUFFER, __FUNCTION__, sizeof(values), values, GL_STATIC_DRAW);
}

static void GL_ClearModelTextureReferences(model_t* mod)
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
	GL_TextureReferenceInvalidate(mod->simpletexture_array);
	//memset(mod->simpletexture_arrays, 0, sizeof(mod->simpletexture_arrays));

	// clear brush model data
	if (mod->type == mod_brush) {
		for (j = 0; j < mod->numtextures; ++j) {
			texture_t* tex = mod->textures[j];
			if (tex) {
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

			frame->gl_scalingS = frame->gl_scalingT = 0;
			GL_TextureReferenceInvalidate(frame->gl_arraynum);
			frame->gl_arrayindex = 0;
		}
	}
}

// Called during disconnect
void GL_DeleteModelData(void)
{
	int i;
	for (i = 0; i < MAX_MODELS; ++i) {
		if (cl.model_precache[i]) {
			GL_ClearModelTextureReferences(cl.model_precache[i]);
		}
	}

	for (i = 0; i < MAX_VWEP_MODELS; ++i) {
		if (cl.vw_model_precache[i]) {
			GL_ClearModelTextureReferences(cl.vw_model_precache[i]);
		}
	}
}

static common_texture_registration_t* GL_AddRegistration(common_texture_t* list, texture_ref texture, qbool any_size, texture_ref* array_ref, int* array_indices, float* scaling_s, float* scaling_t, int number)
{
	common_texture_registration_t* reg;

	while (list->overflow) {
		list = list->overflow;
	}

	if (list->count >= sizeof(list->registrations) / sizeof(list->registrations[0])) {
		list = GL_AppendOverflowTextureAllocation(list, 0);
	}

	reg = &list->registrations[list->count++];
	reg->any_size = any_size;

	reg->count = number;
	reg->original = texture;
	reg->array_ref = array_ref;
	reg->array_scalingS = scaling_s;
	reg->array_scalingT = scaling_t;
	reg->array_indices = array_indices;

	// Keep master list up to date
	if (any_size) {
		list->any_size_count += number;
	}
	return reg;
}

static void GL_CopyRegistration(common_texture_t* list, common_texture_registration_t* src)
{
	GL_AddRegistration(list, src->original, src->any_size, src->array_ref, src->array_indices, src->array_scalingS, src->array_scalingT, src->count);
	src->allocated = true;
}

static void GL_RegisterCommonTextureSize(int type, texture_ref texture, qbool any_size, texture_ref* array_ref, int* array_indices, float* scaling_s, float* scaling_t, int number)
{
	common_texture_t* list = commonTextures[type];
	GLint width, height;

	if (!GL_TextureReferenceIsValid(texture)) {
		return;
	}

	width = GL_TextureWidth(texture);
	height = GL_TextureHeight(texture);

	while (list) {
		if (list->width == width && list->height == height && list->gl_depth == 0) {
			GL_AddRegistration(list, texture, any_size, array_ref, array_indices, scaling_s, scaling_t, number);
			return;
		}

		list = list->next;
	}

	list = Q_malloc(sizeof(common_texture_t));
	list->next = commonTextures[type];
	commonTextures[type] = list;
	list->width = width;
	list->height = height;

	GL_AddRegistration(list, texture, any_size, array_ref, array_indices, scaling_s, scaling_t, number);
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

static common_texture_t* GL_AppendOverflowTextureAllocation(common_texture_t* tex, int allocation_size)
{
	// Create a new texture request for the difference
	common_texture_t* overflow = Q_malloc(sizeof(common_texture_t));
	overflow->width = tex->width;
	overflow->height = tex->height;
	overflow->count = allocation_size;
	overflow->next = NULL;
	tex->overflow = overflow;
	return overflow;
}

// FIXME: Remove, debugging only
static void GL_PrintTextureSizes(common_texture_t* list)
{
	common_texture_t* tex;

	for (tex = list; tex; tex = tex->next) {
		if (tex->count) {
			int i;

			GL_Paranoid_Printf("  %dx%d = %d (%d anysize) array %d\n", tex->width, tex->height, tex->count, tex->any_size_count, tex->gl_texturenum.index);
			for (i = 0; i < tex->count; ++i) {
				common_texture_registration_t* reg = &tex->registrations[i];

				GL_Paranoid_Printf("    %2d) %d textures: [%u]\n", i, reg->count, reg->original.index);
			}
		}

		if (tex->overflow) {
			GL_PrintTextureSizes(tex->overflow);
		}
	}
}

static void GL_SortTextureSizes(common_texture_t** first)
{
	qbool changed = true;

	while (changed) {
		common_texture_t** link = first;

		changed = false;
		while (*link && (*link)->next) {
			common_texture_t* current = *link;
			common_texture_t* next = current->next;

			if (next && current->any_size_count < next->any_size_count) {
				*link = next;
				current->next = next->next;
				next->next = current;
				changed = true;
				link = &next->next;
			}
			else {
				link = &current->next;
			}
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

static void GL_MeasureTexturesForModel(model_t* mod, int* required_vbo_length)
{
	int j;

	switch (mod->type) {
	case mod_alias:
	case mod_alias3:
	{
		for (j = 0; j < MAX_SIMPLE_TEXTURES; ++j) {
			if (GL_TextureReferenceIsValid(mod->simpletexture[j])) {
				GL_RegisterCommonTextureSize(TEXTURETYPES_SPRITES, mod->simpletexture[j], true, &mod->simpletexture_array, &mod->simpletexture_indexes[j], &mod->simpletexture_scalingS[j], &mod->simpletexture_scalingT[j], 1);
			}
		}

		*required_vbo_length += mod->vertsInVBO;
		break;
	}
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
				GL_RegisterCommonTextureSize(TEXTURETYPES_SPRITES, frame->gl_texturenum, true, &frame->gl_arraynum, &frame->gl_arrayindex, &frame->gl_scalingS, &frame->gl_scalingT, 1);
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
				GL_RegisterCommonTextureSize(TEXTURETYPES_SPRITES, mod->simpletexture[j], true, &mod->simpletexture_array, &mod->simpletexture_indexes[j], &mod->simpletexture_scalingS[j], &mod->simpletexture_scalingT[j], 1);
			}
		}

		// Brush models can be boxes (ammo, health), static world or moving platforms
		for (i = 0; i < mod->numtextures; i++) {
			texture_t* tx = mod->textures[i];
			texture_type type = mod->isworldmodel ? TEXTURETYPES_WORLDMODEL : TEXTURETYPES_BRUSHMODEL;
			qbool skip = true;

			if (GL_SkipTexture(mod, tx) || !GL_TextureReferenceIsValid(tx->gl_texturenum)) {
				continue;
			}

			if (GL_TextureReferenceIsValid(tx->fb_texturenum) && !GL_TexturesAreSameSize(tx->gl_texturenum, tx->fb_texturenum)) {
				GL_TextureReferenceInvalidate(tx->fb_texturenum);
			}

			GL_Paranoid_Printf("> Registering texture %d for model %s (tex name %s)\n", tx->gl_texturenum, mod->name, tx->name);
			if (GL_TextureReferenceIsValid(tx->fb_texturenum)) {
				GL_RegisterCommonTextureSize(type, tx->gl_texturenum, BrushModelIsAnySize(mod), &tx->gl_texture_array, &tx->gl_texture_index, &tx->gl_texture_scaleS, &tx->gl_texture_scaleT, 2);
			}
			else {
				GL_RegisterCommonTextureSize(type, tx->gl_texturenum, BrushModelIsAnySize(mod), &tx->gl_texture_array, &tx->gl_texture_index, &tx->gl_texture_scaleS, &tx->gl_texture_scaleT, 1);
			}
		}
		break;
	}
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

static common_texture_t* GL_CreateAnySizeList(common_texture_t* sizes)
{
	common_texture_t* any;
	common_texture_t* size;
	common_texture_t* arr;
	int max_width = 0;
	int max_height = 0;
	int anysize_count = 0;
	int i;

	// Get overall totals
	for (size = sizes; size; size = size->next) {
		for (arr = size; arr; arr = arr->overflow) {
			if (arr->any_size_count) {
				max_width = max(max_width, arr->width);
				max_height = max(max_height, arr->height);
				anysize_count += arr->any_size_count;
			}
		}
	}

	// Nothing to be done here
	if (!anysize_count) {
		GL_Paranoid_Printf(">> (no any-size textures)\n");
		return sizes;
	}

	// Create new size for non-repeating
	any = Q_malloc(sizeof(common_texture_t));
	any->width = max_width;
	any->height = max_height;

	// Move registrations onto the 'any' list
	for (size = sizes; size; size = size->next) {
		for (arr = size; arr; arr = arr->overflow) {
			for (i = 0; i < arr->count; ++i) {
				if (arr->registrations[i].any_size) {
					common_texture_t* overflow = arr->overflow;

					GL_CopyRegistration(any, &arr->registrations[i]);

					// If we over-flowed this bucket, take from the next
					if (overflow && overflow->count) {
						arr->registrations[arr->count - 1] = overflow->registrations[overflow->count - 1];
						--overflow->count;

						// Overflow bucket is empty, remove
						if (!overflow->count) {
							arr->overflow = overflow->overflow;
							Q_free(overflow);
						}
					}
					else {
						// Otherwise take from end of this list
						if (i < arr->count - 1) {
							arr->registrations[i] = arr->registrations[arr->count - 1];
						}
						--arr->count;
						--i;
					}
				}
			}
		}
	}

	GL_Paranoid_Printf(">> (any-size: %d x %d x %d)\n", any->width, any->height, any->count);
	any->next = sizes;
	return any;
}

static int GL_TextureRegistrationComparison(const void* lhs_, const void* rhs_)
{
	const common_texture_registration_t* lhs = (common_texture_registration_t*)lhs_;
	const common_texture_registration_t* rhs = (common_texture_registration_t*)rhs_;

	return rhs->count - lhs->count;
}

static void GL_SortTextureRegistrations(common_texture_t* list)
{
	common_texture_t* overflow = list->overflow;

	if (overflow) {
		common_texture_registration_t complete[2 * MAX_ARRAY_DEPTH];
		int total = list->count + overflow->count;

		GL_SortTextureRegistrations(overflow);

		memcpy(complete, list->registrations, list->count * sizeof(list->registrations[0]));
		memcpy(complete + list->count, overflow->registrations, overflow->count);

		qsort(complete, total, sizeof(list->registrations[0]), GL_TextureRegistrationComparison);

		list->count = total - overflow->count;
		overflow->count = total - list->count;
		memcpy(list->registrations, complete, list->count * sizeof(list->registrations[0]));
		memcpy(list->registrations, complete, overflow->count * sizeof(list->registrations[0]));
	}
	else {
		qsort(list->registrations, list->count, sizeof(list->registrations[0]), GL_TextureRegistrationComparison);
	}
}

static int GL_UnallocatedArrayDepth(common_texture_t* src)
{
	int i;
	int total = 0;

	if (!src) {
		return 0;
	}

	for (i = 0; i < src->count; ++i) {
		if (!src->registrations[i].allocated) {
			total += src->registrations[i].count;
		}
	}

	return total + GL_UnallocatedArrayDepth(src->overflow);
}

static void GL_AssignTextureArrayToRegistrations(common_texture_t* size, int reg_index, texture_ref array_ref, int array_index)
{
	int i;
	int width = GL_TextureWidth(array_ref);
	int height = GL_TextureHeight(array_ref);
	int depth = GL_TextureDepth(array_ref);

	for (i = reg_index; i < size->count; ++i) {
		common_texture_registration_t* reg = &size->registrations[i];

		if (reg->allocated) {
			continue;
		}

		GL_Paranoid_Printf(">>>> Reg %d: %d entries, %d space\n", i, reg->count, depth - array_index);

		if (reg->count <= depth - array_index) {
			int allocation = array_index;
			int original_width = GL_TextureWidth(reg->original);
			int original_height = GL_TextureHeight(reg->original);

			// It fits here
			reg->allocated = true;

			// FIXME: These can be NULL, but probably shouldn't be...
			*reg->array_ref = array_ref;
			*reg->array_indices = allocation;
			*reg->array_scalingS = original_width * 1.0f / width;
			*reg->array_scalingT = original_height * 1.0f / height;

			GL_AddTextureToArray(array_ref, allocation, reg->original, !reg->any_size);
			GL_Paranoid_Printf(">>>> Allocating %d into array %d[%d] %s\n", reg->original, array_ref.index, allocation, reg->array_ref ? "(stored)" : "(NULL)");

			array_index += reg->count;
			if (array_index >= depth) {
				return;
			}
		}
	}

	if (size->overflow) {
		GL_AssignTextureArrayToRegistrations(size->overflow, 0, array_ref, array_index);
	}
}

static void GL_CreateAnySizeLists(void)
{
	int i;

	for (i = 0; i < sizeof(commonTextures) / sizeof(commonTextures[0]); ++i) {
		if (!commonTextures[i]) {
			continue;
		}

		GL_Paranoid_Printf("> Anysize list for Type %d\n", i);
		commonTextures[i] = GL_CreateAnySizeList(commonTextures[i]);
	}
}

static qbool GL_CreateCommonTextureArrays(void)
{
	int i;
	common_texture_t* size;
	common_texture_t* arr;

	for (i = 0; i < sizeof(commonTextures) / sizeof(commonTextures[0]); ++i) {
		if (!commonTextures[i]) {
			continue;
		}

		// Work through all the sizes 
		for (size = commonTextures[i]; size; size = size->next) {
			GL_SortTextureRegistrations(size);

			GL_Paranoid_Printf(">> Processing %d x %d\n", size->width, size->height);
			for (arr = size; arr; arr = size->overflow) {
				int j;

				for (j = 0; j < arr->count; ++j) {
					int depth = GL_UnallocatedArrayDepth(arr);
					int requested_depth = depth;
					texture_ref array_ref;

					if (depth == 0) {
						continue;
					}

					// Try and allocate full array
					array_ref = GL_CreateTextureArray("", size->width, size->height, &depth, TEX_MIPMAP | TEX_ALPHA, arr->registrations[j].count);
					if (!GL_TextureReferenceIsValid(array_ref)) {
						// Oh dear, couldn't keep those textures in one array...
						GL_Paranoid_Printf("!!!!! Unrecoverable error, array creation failed\n");
						return false;
					}

					// Assign to registrations
					GL_Paranoid_Printf(">>> Assigning array %d to registrations (depth %d)\n", array_ref.index, depth);
					GL_AssignTextureArrayToRegistrations(arr, j, array_ref, 0);
				}
			}
		}
	}

	return true;
}

static void GL_ImportTexturesForModel(model_t* mod, int* new_vbo_position)
{
	int count = 0;
	int j;

	if (mod->type == mod_alias) {
		aliashdr_t* paliashdr = (aliashdr_t *)Mod_Extradata(mod);

		GL_AliasModelAddToVBO(mod, paliashdr, &aliasModel_vbo, &aliasModel_ssbo, *new_vbo_position);
		*new_vbo_position += mod->vertsInVBO;
	}
	else if (mod->type == mod_alias3) {
		GL_MD3ModelAddToVBO(mod, &aliasModel_vbo, &aliasModel_ssbo, *new_vbo_position);

		*new_vbo_position += mod->vertsInVBO;
	}
	else if (mod->type == mod_sprite) {
		mod->vbo_start = 0;
	}
	else if (mod->type == mod_brush) {
		mod->vbo_start = 0;

		for (j = 0; j < mod->numtextures; ++j) {
			texture_t* tex = mod->textures[j];

			if (GL_SkipTexture(mod, tex) || !GL_TextureReferenceIsValid(tex->gl_texturenum)) {
				continue;
			}

			if (GL_TextureReferenceIsValid(tex->gl_texture_array) && GL_TextureReferenceIsValid(tex->fb_texturenum) && GL_TexturesAreSameSize(tex->gl_texturenum, tex->fb_texturenum)) {
				GL_AddTextureToArray(tex->gl_texture_array, tex->gl_texture_index + 1, tex->fb_texturenum, true);
			}
			else {
				GL_TextureReferenceInvalidate(tex->fb_texturenum);
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

	GL_UpdateVBOSection(&aliasModel_vbo, 0, sizeof(verts), verts);
}

void GL_CompressTextureArrays(common_texture_t* list)
{
	common_texture_t* this_tex;
	common_texture_t* cur;

	for (this_tex = list; this_tex; this_tex = this_tex->next) {
		int non_any = this_tex->count - this_tex->any_size_count;

		if (this_tex->width == 0 || non_any == 0) {
			continue;
		}

		this_tex->final = NULL;
		for (cur = list; cur; cur = cur->next) {
			int ratio_x, ratio_y;

			if (cur->width == 0 || cur == this_tex) {
				continue;
			}

			ratio_x = cur->width / this_tex->width;
			ratio_y = cur->height / this_tex->height;

			if (ratio_x * this_tex->width == cur->width && ratio_y * this_tex->height == cur->height) {
				if (!this_tex->final || cur->width > this_tex->final->width || cur->height > this_tex->final->height) {
					this_tex->final = cur;
				}
			}
		}

		if (this_tex->final) {
			this_tex->final->count += non_any;
			this_tex->count -= non_any;
		}
	}
}

// Called from R_NewMap
void GL_BuildCommonTextureArrays(qbool vid_restart)
{
	extern model_t* Mod_LoadModel(model_t *mod, qbool crash);
	int required_vbo_length = 4;
	int i;
	int type;

	tempBufferSize = 0;

	GL_DeleteExistingArrays(!vid_restart);
	GL_DeleteModelData();

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

			GL_MeasureTexturesForModel(mod, &required_vbo_length);
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

			GL_MeasureTexturesForModel(mod, &required_vbo_length);
		}
	}

	GL_Paranoid_Printf("-- Pre-sort --\n");
	for (type = 0; type < TEXTURETYPES_COUNT; ++type) {
		GL_Paranoid_Printf("- Type %d\n", type);
		GL_PrintTextureSizes(commonTextures[type]);
	}

	// Add non-model textures we need (generally sprites)
	{
		// TODO
	}

	{
		common_texture_t* tex;
		int new_vbo_position = 0;

		GL_GenFixedBuffer(&aliasModel_vbo, GL_ARRAY_BUFFER, "aliasmodel-vertex-data", required_vbo_length * sizeof(vbo_model_vert_t), NULL, GL_STATIC_DRAW);
		GL_GenFixedBuffer(&aliasModel_ssbo, GL_SHADER_STORAGE_BUFFER, "aliasmodel-vertex-ssbo", required_vbo_length * sizeof(vbo_model_vert_t), NULL, GL_STATIC_COPY);

		GL_Paranoid_Printf("-- Sorted, pre-allocation --\n");
		for (type = 0; type < TEXTURETYPES_COUNT; ++type) {
			GL_SortTextureSizes(&commonTextures[type]);

			GL_Paranoid_Printf("- Type %d\n", type);
			GL_PrintTextureSizes(commonTextures[type]);
		}

		/*for (type = 0; type < TEXTURETYPES_COUNT; ++type) {
			GL_CompressTextureArrays(commonTextures[type]);
		}*/

		GL_Paranoid_Printf("GL_CreateAnySizeLists\n");
		GL_CreateAnySizeLists();

		GL_Paranoid_Printf("-- Sorted, with any-size --\n");
		for (type = 0; type < TEXTURETYPES_COUNT; ++type) {
			GL_Paranoid_Printf("- Type %d\n", type);
			GL_PrintTextureSizes(commonTextures[type]);
		}

		GL_Paranoid_Printf("GL_CreateCommonTextureArrays\n");
		GL_CreateCommonTextureArrays();

		// VBO starts with simple-model/sprite vertices
		GLM_CreateSpriteVBO();
		new_vbo_position = 4;

		// Go back through all models, importing textures into arrays and creating new VBO
		for (i = 1; i < MAX_MODELS; ++i) {
			model_t* mod = cl.model_precache[i];

			if (mod) {
				GL_Paranoid_Printf("%s\n", mod->name);
				GL_ImportTexturesForModel(mod, &new_vbo_position);
			}
		}

		for (i = 0; i < MAX_VWEP_MODELS; i++) {
			model_t* mod = cl.vw_model_precache[i];

			if (mod) {
				GL_Paranoid_Printf("%s\n", mod->name);
				GL_ImportTexturesForModel(mod, &new_vbo_position);
			}
		}

		GL_Paranoid_Printf("-- Post-allocation --\n");
		for (type = 0; type < TEXTURETYPES_COUNT; ++type) {
			GL_Paranoid_Printf("- Type %d\n", type);
			GL_PrintTextureSizes(commonTextures[type]);
		}

		// Generate mipmaps for every texture array
		for (type = 0; type < TEXTURETYPES_COUNT; ++type) {
			for (tex = commonTextures[type]; tex; tex = tex->next) {
				common_texture_t* arr;

				for (arr = tex; arr; arr = arr->overflow) {
					int i;

					for (i = 0; i < arr->count; ++i) {
						if (arr->registrations[i].array_ref) {
							texture_ref ref = arr->registrations[i].array_ref[0];
							if (GL_TextureReferenceIsValid(ref)) {
								GL_GenerateMipmapsIfNeeded(ref);
							}
						}
					}
				}
			}
		}

		GLM_CreateInstanceVBO();
		GLM_CreateAliasModelVAO();
		GLM_CreateBrushModelVAO(&instance_vbo);
		GL_BindBufferBase(&aliasModel_ssbo, GL_BINDINGPOINT_ALIASMODEL_SSBO);
	}

	Q_free(tempBuffer);
	tempBufferSize = 0;
}

static void GLM_CreateAliasModelVAO(void)
{
	GL_GenVertexArray(&aliasModel_vao);

	GL_ConfigureVertexAttribPointer(&aliasModel_vao, &aliasModel_vbo, 0, 3, GL_FLOAT, GL_FALSE, sizeof(vbo_model_vert_t), VBO_ALIASVERT_FOFS(position));
	GL_ConfigureVertexAttribPointer(&aliasModel_vao, &aliasModel_vbo, 1, 2, GL_FLOAT, GL_FALSE, sizeof(vbo_model_vert_t), VBO_ALIASVERT_FOFS(texture_coords));
	GL_ConfigureVertexAttribPointer(&aliasModel_vao, &aliasModel_vbo, 2, 3, GL_FLOAT, GL_FALSE, sizeof(vbo_model_vert_t), VBO_ALIASVERT_FOFS(normal));
	GL_ConfigureVertexAttribIPointer(&aliasModel_vao, &instance_vbo, 3, 1, GL_UNSIGNED_INT, sizeof(GLuint), 0);
	GL_ConfigureVertexAttribIPointer(&aliasModel_vao, &aliasModel_vbo, 4, 1, GL_INT, sizeof(vbo_model_vert_t), VBO_ALIASVERT_FOFS(vert_index));

	glVertexAttribDivisor(3, 1);
}
