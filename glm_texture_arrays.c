
#include "quakedef.h"
#include "vx_stuff.h"
#include "vx_tracker.h"
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"
#ifndef  __APPLE__
#include "tr_types.h"
#endif

static void GLM_CreateAliasModelVAO(void);
static void GL_PrintTextureSizes(struct common_texture_s* list);
static qbool GL_SkipTexture(model_t* mod, texture_t* tx);

static GLubyte* tempBuffer;
static GLuint tempBufferSize;

static glm_vbo_t instance_vbo;
glm_vao_t aliasModel_vao;
glm_vbo_t aliasModel_vbo;

typedef struct common_texture_s {
	int width;
	int height;
	int count;
	int any_size_count;
	GLuint gl_texturenum;
	int gl_width;
	int gl_height;
	int gl_depth;

	int allocated;

	struct common_texture_s* next;
	struct common_texture_s* final;
} common_texture_t;

enum {
	TEXTURETYPES_ALIASMODEL,
	TEXTURETYPES_BRUSHMODEL,
	TEXTURETYPES_WORLDMODEL,
	TEXTURETYPES_SPRITES,

	TEXTURETYPES_COUNT
};

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
			if (delete_textures && tex->gl_texturenum) {
				GL_DeleteTextureArray(&tex->gl_texturenum);
			}
			Q_free(tex);
		}
	}

	memset(commonTextures, 0, sizeof(commonTextures));
}

static qbool AliasModelIsAnySize(model_t* mod)
{
	return false; //mod->max_tex[0] <= 1.0 && mod->max_tex[1] <= 1.0 && mod->min_tex[0] >= 0 && mod->min_tex[1] >= 0;
}

static qbool BrushModelIsAnySize(model_t* mod)
{
	//int i;

	//Con_Printf("BrushModelIsAnySize(%s) = [%f %f => %f %f]\n", mod->name, mod->min_tex[0], mod->min_tex[1], mod->max_tex[0], mod->max_tex[1]);

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
	mod->simpletexture_array = 0;

	// clear brush model data
	if (mod->type == mod_brush) {
		for (j = 0; j < mod->numtextures; ++j) {
			texture_t* tex = mod->textures[j];
			if (tex) {
				tex->gl_texture_scaleS = tex->gl_texture_scaleT = 0;
				tex->gl_texture_array = tex->gl_texture_index = 0;
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
			frame->gl_arraynum = frame->gl_arrayindex = 0;
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

static void GL_RegisterCommonTextureSize(int type, GLint texture, qbool any_size)
{
	common_texture_t* list = commonTextures[type];
	GLint width, height;

	if (!texture) {
		return;
	}

	GL_Bind(texture);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

	while (list) {
		if (list->width == width && list->height == height && list->gl_depth == 0) {
			list->count++;
			if (any_size) {
				list->any_size_count++;
			}
			return;
		}

		list = list->next;
	}

	list = Q_malloc(sizeof(common_texture_t));
	list->next = commonTextures[type];
	list->width = width;
	list->height = height;
	list->count = 1;
	if (any_size) {
		list->any_size_count++;
	}
	commonTextures[type] = list;
}

static void GL_AddTextureToArray(GLuint arrayTexture, int width, int height, int final_width, int final_height, int index, GLuint tex2dname, qbool tile)
{
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

	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, tempBuffer);
	GL_ProcessErrors(va("GL_AddTextureToArray(%u => %u)/glGetTexImage", tex2dname, arrayTexture));

	// Might need to tile multiple times
	for (x = 0; x < ratio_x; ++x) {
		for (y = 0; y < ratio_y; ++y) {
			GL_TexSubImage3D(GL_TEXTURE0, GL_TEXTURE_2D_ARRAY, arrayTexture, 0, x * width, y * height, index, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, tempBuffer);
#ifdef GL_PARANOIA
			if (glGetError() != GL_NO_ERROR) {
				int array_width, array_height, array_depth;

				GL_BindTextureUnit(GL_TEXTURE0, GL_TEXTURE_2D_ARRAY, arrayTexture);
				glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_WIDTH, &array_width);
				glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_HEIGHT, &array_height);
				glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_DEPTH, &array_depth);

				GL_Paranoid_Printf("Failed to import texture %u to array %u[%d] (%d x %d x %d)\n", tex2dname, arrayTexture, index, array_width, array_height, array_depth);
				GL_Paranoid_Printf(">  Parameters: %d %d %d, %d %d 1, tempBuffer = %X\n", x * width, y * height, index, width, height, tempBuffer);
			}
#endif
		}
	}
}

static common_texture_t* GL_FindTextureBySize(common_texture_t* list, int width, int height)
{
	common_texture_t* start = list;

	while (list) {
		if (list->width == width && list->height == height) {
			common_texture_t* candidate = list;
			if (candidate->final) {
				candidate = candidate->final;
				while (candidate->gl_texturenum != 0 && candidate->allocated >= candidate->gl_depth) {
					candidate = candidate->next;
				}
			}
			if (candidate->gl_texturenum == 0 || candidate->allocated < candidate->gl_depth) {
				return candidate;
			}
		}

		list = list->next;
	}

	return NULL;
}

static void GL_CopyToTextureArraySize(int type, GLuint stdTexture, qbool anySize, float* scaleS, float* scaleT, GLuint* texture_array, GLuint* texture_array_index)
{
	GLint width, height;
	common_texture_t* tex;
	common_texture_t* list = commonTextures[type];
	GLint desired_width, desired_height;

	if (scaleS && scaleT) {
		*scaleS = *scaleT = 0;
	}
	if (texture_array_index) {
		*texture_array_index = 0;
	}
	if (texture_array) {
		*texture_array = 0;
	}

	if (!stdTexture) {
		return;
	}

	GL_BindTextureUnit(GL_TEXTURE0, GL_TEXTURE_2D, stdTexture);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

	desired_width = width;
	desired_height = height;

	GL_Paranoid_Printf("  ... texture is %d x %d (%s)\n", width, height, anySize ? "anysize" : "fixed");

	if (anySize) {
		tex = GL_FindTextureBySize(list, 0, 0);
		if (tex == NULL) {
			common_texture_t* t;

			tex = Q_malloc(sizeof(common_texture_t));
			tex->width = 0;
			tex->height = 0;

			// Get requirements from existing list
			tex->count = 0;
			desired_width = 0;
			desired_height = 0;
			for (t = commonTextures[type]; t; t = t->next) {
				if (t->width && t->height && t->any_size_count) {
					desired_width = max(desired_width, t->width);
					desired_height = max(desired_height, t->height);
				}
				tex->count += t->any_size_count;
			}

			tex->next = commonTextures[type];
			commonTextures[type] = tex;

			GL_Paranoid_Printf("> Creating anysize texture for type %d\n", type);
		}
	}
	else {
		tex = GL_FindTextureBySize(list, width, height);

		assert(tex);

		desired_width = tex->width;
		desired_height = tex->height;
	}

	GL_Paranoid_Printf("..... Texture found is %d x %d [#%d] (%dx%d)\n", tex->width, tex->height, tex->gl_texturenum, tex->gl_width, tex->gl_height);

	if (!tex->gl_texturenum) {
		int depth = tex->count - tex->any_size_count;
		int requested = depth;

		tex->gl_texturenum = GL_CreateTextureArray("", desired_width, desired_height, &depth, TEX_MIPMAP);
		GL_Paranoid_Printf("> Array %d created (%d/%d) slices @ %d x %d\n", tex->gl_texturenum, depth, requested, desired_width, desired_height);
		tex->gl_width = desired_width;
		tex->gl_height = desired_height;
		tex->gl_depth = depth;
		if (depth < requested) {
			// Create a new texture request for the difference
			common_texture_t* overflow = Q_malloc(sizeof(common_texture_t));
			overflow->width = tex->width;
			overflow->height = tex->height;
			overflow->count = requested - depth;
			overflow->next = tex->next;
			tex->next = overflow;
		}
	}

	if (scaleS && scaleT) {
		*scaleS = width * 1.0f / tex->gl_width;
		*scaleT = height * 1.0f / tex->gl_height;
	}

	GL_AddTextureToArray(tex->gl_texturenum, width, height, tex->gl_width, tex->gl_height, tex->allocated, stdTexture, tex->width != 0);
	if (texture_array) {
		*texture_array = tex->gl_texturenum;
	}
	if (texture_array_index) {
		*texture_array_index = tex->allocated;
	}
	tex->allocated++;
}

// FIXME: Remove, debugging only
static void GL_PrintTextureSizes(struct common_texture_s* list)
{
	common_texture_t* tex;

	for (tex = list; tex; tex = tex->next) {
		if (tex->count) {
			GL_Paranoid_Printf("  %dx%d = %d (%d anysize)\n", tex->width, tex->height, tex->count, tex->any_size_count);
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
static void GL_SetModelTextureArray(model_t* mod, GLuint array_num, float widthRatio, float heightRatio)
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
	{
		for (j = 0; j < MAX_SIMPLE_TEXTURES; ++j) {
			if (mod->simpletexture[j]) {
				GL_RegisterCommonTextureSize(TEXTURETYPES_SPRITES, mod->simpletexture[j], true);
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

			if (offset < (int)sizeof(msprite2_t) || numframes < 1) {
				continue;
			}

			GL_RegisterCommonTextureSize(TEXTURETYPES_SPRITES, ((mspriteframe_t* )((byte*)psprite + offset))->gl_texturenum, true);
			++count;
		}
		break;
	}
	case mod_brush:
	{
		int i, j;

		// Ammo-boxes etc can be replaced with simple textures
		for (j = 0; j < MAX_SIMPLE_TEXTURES; ++j) {
			if (mod->simpletexture[j]) {
				GL_RegisterCommonTextureSize(TEXTURETYPES_SPRITES, mod->simpletexture[j], true);
			}
		}

		// Brush models can be boxes (ammo, health), static world or moving platforms
		for (i = 0; i < mod->numtextures; i++) {
			texture_t* tx = mod->textures[i];
			qbool skip = true;

			//if (GL_SkipTexture(mod, tx)) {
			//	continue;
			//}

			GL_RegisterCommonTextureSize(mod->isworldmodel ? TEXTURETYPES_WORLDMODEL : TEXTURETYPES_BRUSHMODEL, tx->gl_texturenum, BrushModelIsAnySize(mod));
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

		if (!tex || !tex->loaded || tex->gl_texture_array == 0) {
			continue;
		}

		for (j = 0; j < i; ++j) {
			texture_t* prev_tex = mod->textures[j];
			if (prev_tex && prev_tex->gl_texture_array == tex->gl_texture_array) {
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

static void GLM_PrintTextureArrays(model_t* mod)
{
	int i;

	GL_Paranoid_Printf("%s\n", mod->name);
	for (i = 0; i < mod->numtextures; ++i) {
		texture_t* tex = mod->textures[i];

		if (!tex) {
			continue;
		}

		if (!tex->loaded || tex->gl_texture_array == 0) {
			continue;
		}

		GL_Paranoid_Printf("  %2d: %s, %d [%2d] %s\n", i, tex->name, tex->gl_texture_array, tex->next_same_size, tex->size_start ? "start" : "cont");
	}

	for (i = 0; i < mod->texture_array_count; ++i) {
		GL_Paranoid_Printf("A %2d: %d first %d, %.3f x %.3f\n", i, mod->texture_arrays[i], mod->texture_array_first[i], mod->texture_arrays_scale_s[i], mod->texture_arrays_scale_t[i]);
	}
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
		if (!tex->loaded || tex->gl_texture_array == 0) {
			continue;
		}

		for (j = i - 1; j >= 0; --j) {
			texture_t* prev_tex = mod->textures[j];
			if (prev_tex && prev_tex->gl_texture_array == tex->gl_texture_array) {
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

void GLM_FindBrushModelTextureExtents(model_t* mod)
{
	int i;

	// Clear dimensions for model
	mod->min_tex[0] = mod->min_tex[1] = 9999;
	mod->max_tex[0] = mod->max_tex[1] = -9999;

	// Clear dimensions for 
	for (i = 0; i < mod->numsurfaces; ++i) {
		msurface_t* s = &mod->surfaces[i];
		glpoly_t* poly;

		//GL_Paranoid_Printf("  Surface %d (tex %d)\n", i, s->texinfo->texture->);
		for (poly = s->polys; poly; poly = poly->next) {
			int j;

			float surfmin[2] = { +9999.9f, +9999.9f };
			float surfmax[2] = { -9999.9f, -9999.9f };
			float repeats[2];
			qbool simple;

			for (j = 0; j < poly->numverts; ++j) {
				float s = poly->verts[j][3];
				float t = poly->verts[j][4];

				surfmin[0] = min(surfmin[0], s);
				surfmax[0] = max(surfmax[0], s);
				surfmin[1] = min(surfmin[1], t);
				surfmax[1] = max(surfmax[1], t);
			}

			repeats[0] = fabs(surfmax[0] - surfmin[0]);
			repeats[1] = fabs(surfmax[1] - surfmin[1]);
			simple = repeats[0] <= 1 && repeats[1] <= 1;

			if (repeats[0] > 1 || repeats[1] > 1) {
				//Con_Printf("    Poly range: %f,%f\n", repeats[0], repeats[1]);
			}
		}
	}
}

void GL_ImportTexturesForModel(model_t* mod, int* new_vbo_position)
{
	int count = 0;
	int j;

	if (mod->type == mod_alias) {
		aliashdr_t* paliashdr = (aliashdr_t *)Mod_Extradata(mod);

		for (j = 0; j < MAX_SIMPLE_TEXTURES; ++j) {
			if (mod->simpletexture[j]) {
				GL_CopyToTextureArraySize(TEXTURETYPES_SPRITES, mod->simpletexture[j], true, &mod->simpletexture_scalingS[j], &mod->simpletexture_scalingT[j], &mod->simpletexture_array, &mod->simpletexture_indexes[j]);
			}
		}

		//GL_SetModelTextureArray(mod, commonTex->gl_texturenum, commonTex->width * 1.0f / maxWidth, commonTex->height * 1.0f / maxHeight);

		GL_AliasModelAddToVBO(mod, paliashdr, &aliasModel_vbo, *new_vbo_position);
		*new_vbo_position += mod->vertsInVBO;
	}
	else if (mod->type == mod_sprite) {
		msprite2_t* psprite = (msprite2_t*)Mod_Extradata(mod);

		for (j = 0; j < psprite->numframes; ++j) {
			int offset    = psprite->frames[j].offset;
			int numframes = psprite->frames[j].numframes;
			mspriteframe_t* frame;

			if (offset < (int)sizeof(msprite2_t) || numframes < 1) {
				continue;
			}

			frame = ((mspriteframe_t*)((byte*)psprite + offset));
			GL_CopyToTextureArraySize(TEXTURETYPES_SPRITES, frame->gl_texturenum, true, &frame->gl_scalingS, &frame->gl_scalingT, &frame->gl_arraynum, &frame->gl_arrayindex);
		}

		mod->vbo_start = 0;
	}
	else if (mod->type == mod_brush) {
		GLM_FindBrushModelTextureExtents(mod);

		for (j = 0; j < MAX_SIMPLE_TEXTURES; ++j) {
			if (mod->simpletexture[j]) {
				GL_CopyToTextureArraySize(TEXTURETYPES_SPRITES, mod->simpletexture[j], true, &mod->simpletexture_scalingS[j], &mod->simpletexture_scalingT[j], &mod->simpletexture_array, &mod->simpletexture_indexes[j]);
			}
		}
		mod->vbo_start = 0;

		for (j = 0; j < mod->numtextures; ++j) {
			texture_t* tex = mod->textures[j];

			if (GL_SkipTexture(mod, tex) || tex->gl_texture_array || !tex->gl_texturenum) {
				continue;
			}

			if (mod->isworldmodel) {
				GL_Paranoid_Printf("... adding %s\n", tex->name);
			}
			GL_CopyToTextureArraySize(mod->isworldmodel ? TEXTURETYPES_WORLDMODEL : TEXTURETYPES_BRUSHMODEL, tex->gl_texturenum, BrushModelIsAnySize(mod), &tex->gl_texture_scaleS, &tex->gl_texture_scaleT, &tex->gl_texture_array, &tex->gl_texture_index);
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
	float new_vbo_buffer[4 * MODELVERTEXSIZE];
	float* vert;

	vert = new_vbo_buffer;
	VectorSet(vert, 0, -1, -1);
	vert[3] = 1;
	vert[4] = 1;

	vert = new_vbo_buffer + MODELVERTEXSIZE;
	VectorSet(vert, 0, -1, 1);
	vert[3] = 1;
	vert[4] = 0;

	vert = new_vbo_buffer + MODELVERTEXSIZE * 2;
	VectorSet(vert, 0, 1, 1);
	vert[3] = 0;
	vert[4] = 0;

	vert = new_vbo_buffer + MODELVERTEXSIZE * 3;
	VectorSet(vert, 0, 1, -1);
	vert[3] = 0;
	vert[4] = 1;

	GL_UpdateVBOSection(&aliasModel_vbo, 0, sizeof(new_vbo_buffer), new_vbo_buffer);
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
			Mod_LoadModel(mod, false);

			GL_MeasureTexturesForModel(mod, &required_vbo_length);
		}
	}

	// Add non-model textures we need (generally sprites)
	{
		// TODO
	}

	{
		// Find highest dimensions, stick everything in there for the moment unless texture is tiling
		common_texture_t* tex;
		int new_vbo_position = 0;

		GL_GenFixedBuffer(&aliasModel_vbo, GL_ARRAY_BUFFER, __FUNCTION__, required_vbo_length * MODELVERTEXSIZE * sizeof(float), NULL, GL_STATIC_DRAW);
		Con_Printf("Created alias model VBO: %d, size %d/%d\n", aliasModel_vbo.vbo, required_vbo_length, required_vbo_length * MODELVERTEXSIZE * sizeof(float));

		GL_Paranoid_Printf("-- Pre-allocation --\n");
		for (type = 0; type < TEXTURETYPES_COUNT; ++type) {
			GL_SortTextureSizes(&commonTextures[type]);

			GL_Paranoid_Printf("- Type %d\n", type);
			GL_PrintTextureSizes(commonTextures[type]);
		}

		for (type = 0; type < TEXTURETYPES_COUNT; ++type) {
			GL_CompressTextureArrays(commonTextures[type]);
		}

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
				if (tex->gl_texturenum) {
					GL_BindTexture(GL_TEXTURE_2D_ARRAY, tex->gl_texturenum, true);
					glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
				}
			}
		}

		GLM_CreateInstanceVBO();
		GLM_CreateAliasModelVAO();
		GLM_CreateBrushModelVAO(&instance_vbo);
	}

	Q_free(tempBuffer);
	tempBufferSize = 0;
}

#define VAO_FLOAT_ATTRIBUTE    1
#define VAO_INTEGER_ATTRIBUTE  2

static void GLM_CreateAliasModelVAO(void)
{
	GL_GenVertexArray(&aliasModel_vao);

	GL_ConfigureVertexAttribPointer(&aliasModel_vao, &aliasModel_vbo, 0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * MODELVERTEXSIZE, (void*)0);
	GL_ConfigureVertexAttribPointer(&aliasModel_vao, &aliasModel_vbo, 1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * MODELVERTEXSIZE, (void*)(sizeof(float) * 3));
	GL_ConfigureVertexAttribPointer(&aliasModel_vao, &aliasModel_vbo, 2, 3, GL_FLOAT, GL_FALSE, sizeof(float) * MODELVERTEXSIZE, (void*)(sizeof(float) * 5));
	GL_ConfigureVertexAttribIPointer(&aliasModel_vao, &instance_vbo, 3, 1, GL_UNSIGNED_INT, sizeof(GLuint), 0);

	glVertexAttribDivisor(3, 1);
}
