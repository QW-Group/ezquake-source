
#include "quakedef.h"
#include "vx_stuff.h"
#include "vx_tracker.h"
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"
#ifndef  __APPLE__
#include "tr_types.h"
#endif

#define VBO_VERT_FOFS(x) (void*)((intptr_t)&(((vbo_world_vert_t*)0)->x))

static glm_vao_t model_vao;
static glm_vbo_t instance_vbo;

void GL_BuildCommonTextureArrays(void);
static void GLM_CreatePowerupShellTexture(GLuint texture_array, int maxWidth, int maxHeight, int slice);
static void GLM_CreateBrushModelVAO(void);

static qbool AliasModelIsAnySize(model_t* mod)
{
	// mod->max_tex[0] <= 1.0 && mod->max_tex[1] <= 1.0 && mod->min_tex[0] >= 0 && mod->min_tex[1] >= 0
	return false;
}

static qbool BrushModelIsAnySize(model_t* mod)
{
	//int i;

	//Con_Printf("BrushModelIsAnySize(%s) = [%f %f => %f %f]\n", mod->name, mod->min_tex[0], mod->min_tex[1], mod->max_tex[0], mod->max_tex[1]);

	return false;
}

#define MAX_INSTANCES 64

static void GLM_CreateModelVAO(GLuint model_vbo, GLuint required_vbo_length, float* new_vbo_buffer);
static void GLM_CreateInstanceVBO(void)
{
	unsigned int values[MAX_INSTANCES];
	int i;

	GL_GenBuffer(&instance_vbo, __FUNCTION__);
	glBindBufferExt(GL_ARRAY_BUFFER, instance_vbo.vbo);

	for (i = 0; i < MAX_INSTANCES; ++i) {
		values[i] = i;
	}

	glBufferDataExt(GL_ARRAY_BUFFER, sizeof(values), values, GL_STATIC_DRAW);
}

static void GL_DeleteModelTextures(model_t* mod)
{
	int j;

	memset(mod->texture_arrays, 0, sizeof(mod->texture_arrays));
	memset(mod->texture_arrays_scale_s, 0, sizeof(mod->texture_arrays_scale_s));
	memset(mod->texture_arrays_scale_t, 0, sizeof(mod->texture_arrays_scale_t));
	memset(mod->texture_array_first, 0, sizeof(mod->texture_array_first));
	mod->texture_array_count = 0;

	for (j = 0; j < MAX_SIMPLE_TEXTURES; ++j) {
		mod->simpletexture_scalingS[j] = mod->simpletexture_scalingT[j] = 0;
		mod->simpletexture_array = mod->simpletexture_indexes[j] = 0;
	}

	// clear alias model data
	if (mod->type == mod_alias) {
		aliashdr_t* paliashdr = (aliashdr_t *)Mod_Extradata(mod);

		memset(paliashdr->gl_scalingS, 0, sizeof(paliashdr->gl_scalingS));
		memset(paliashdr->gl_scalingT, 0, sizeof(paliashdr->gl_scalingT));
		memset(paliashdr->gl_arrayindex, 0, sizeof(paliashdr->gl_arrayindex));
		memset(paliashdr->gl_fb_arrayindex, 0, sizeof(paliashdr->gl_fb_arrayindex));
	}

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
			GL_DeleteModelTextures(cl.model_precache[i]);
		}
	}

	for (i = 0; i < MAX_VWEP_MODELS; ++i) {
		if (cl.vw_model_precache[i]) {
			GL_DeleteModelTextures(cl.vw_model_precache[i]);
		}
	}
}

typedef struct common_texture_s {
	int width;
	int height;
	int count;
	int any_size_count;
	GLuint gl_texturenum;
	int gl_width;
	int gl_height;

	int allocated;

	struct common_texture_s* next;
} common_texture_t;

static void GL_RegisterCommonTextureSize(common_texture_t* list, GLint texture, qbool any_size)
{
	GLint width, height;

	if (!texture) {
		return;
	}

	GL_Bind(texture);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

	while (list) {
		if (list->width == width && list->height == height) {
			list->count++;
			if (any_size) {
				list->any_size_count++;
			}
			break;
		}
		else if (!list->next) {
			list->next = Q_malloc(sizeof(common_texture_t));
			list->next->width = width;
			list->next->height = height;
		}

		list = list->next;
	}
}

static void GL_AddTextureToArray(GLuint arrayTexture, int width, int height, int index)
{
	int level = 0;
	GLubyte* buffer;

	buffer = Q_malloc(width * height * 4 * sizeof(GLubyte));

	GL_BindTexture(GL_TEXTURE_2D_ARRAY, arrayTexture, true);
	for (level = 0; width && height; ++level, width /= 2, height /= 2) {
		glGetTexImage(GL_TEXTURE_2D, level, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, level, 0, 0, index, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	}

	Q_free(buffer);
}

static common_texture_t* GL_FindTextureBySize(common_texture_t* list, int width, int height)
{
	while (list) {
		if (list->width == width && list->height == height) {
			return list;
		}

		list = list->next;
	}

	return NULL;
}

static void GL_CopyToTextureArraySize(common_texture_t* list, GLuint stdTexture, qbool anySize, float* scaleS, float* scaleT, GLuint* texture_array, GLuint* texture_array_index)
{
	GLint width, height;
	common_texture_t* tex;

	if (!stdTexture) {
		if (scaleS && scaleT) {
			*scaleS = *scaleT = 0;
		}
		if (texture_array_index) {
			*texture_array_index = -1;
		}
		if (texture_array) {
			*texture_array = -1;
		}
		return;
	}

	GL_Bind(stdTexture);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

	if (anySize) {
		tex = GL_FindTextureBySize(list, 0, 0);
	}
	else {
		tex = GL_FindTextureBySize(list, width, height);
		if (!tex->gl_texturenum) {
			tex->gl_texturenum = GL_CreateTextureArray("", width, height, tex->count - tex->any_size_count, TEX_MIPMAP);
			tex->gl_width = width;
			tex->gl_height = height;
		}
	}
	if (scaleS && scaleT) {
		*scaleS = width * 1.0f / tex->gl_width;
		*scaleT = height * 1.0f / tex->gl_height;
	}

	GL_AddTextureToArray(tex->gl_texturenum, width, height, tex->allocated);
	if (texture_array) {
		*texture_array = tex->gl_texturenum;
	}
	if (texture_array_index) {
		*texture_array_index = tex->allocated;
	}
	tex->allocated++;
}

static void GL_FreeTextureSizeList(common_texture_t* tex)
{
	common_texture_t* next = tex->next;
	while (next) {
		Q_free(tex);
		tex = next;
		next = tex->next;
	}
	Q_free(tex);
}

// FIXME: Remove, debugging only
static void GL_PrintTextureSizes(common_texture_t* list)
{
	common_texture_t* tex;

	for (tex = list; tex; tex = tex->next) {
		if (tex->count) {
			//Con_Printf("%dx%d = %d (%d anysize)\n", tex->width, tex->height, tex->count, tex->any_size_count);
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

static void GL_MeasureTexturesForModel(model_t* mod, common_texture_t* common, int* required_vbo_length)
{
	int j;

	switch (mod->type) {
	case mod_alias:
	{
		aliashdr_t* paliashdr = (aliashdr_t *)Mod_Extradata(mod);
		qbool any_size = AliasModelIsAnySize(mod);

		for (j = 0; j < paliashdr->numskins; ++j) {
			int anim;
			for (anim = 0; anim < 4; ++anim) {
				if (anim == 0 || paliashdr->gl_texturenum[j][anim] != paliashdr->gl_texturenum[j][anim - 1]) {
					GL_RegisterCommonTextureSize(common, paliashdr->gl_texturenum[j][anim], any_size);
				}
				if (anim == 0 || paliashdr->fb_texturenum[j][anim] != paliashdr->fb_texturenum[j][anim - 1]) {
					GL_RegisterCommonTextureSize(common, paliashdr->fb_texturenum[j][anim], any_size);
				}
			}
		}

		for (j = 0; j < MAX_SIMPLE_TEXTURES; ++j) {
			if (mod->simpletexture[j]) {
				GL_RegisterCommonTextureSize(common, mod->simpletexture[j], true);
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

			GL_RegisterCommonTextureSize(common, ((mspriteframe_t* )((byte*)psprite + offset))->gl_texturenum, true);
			++count;
		}
		break;
	}
	case mod_brush:
	{
		int i;

		// Ammo-boxes etc can be replaced with simple textures
		for (j = 0; j < MAX_SIMPLE_TEXTURES; ++j) {
			if (mod->simpletexture[j]) {
				GL_RegisterCommonTextureSize(common, mod->simpletexture[j], true);
			}
		}

		// Brush models can be boxes (ammo, health), static world or moving platforms
		for (i = 0; i < mod->numtextures; i++) {
			texture_t* tx = mod->textures[i];
			if (!tx || !tx->loaded) {
				continue;
			}

			GL_RegisterCommonTextureSize(common, tx->gl_texturenum, BrushModelIsAnySize(mod));
		}
		break;
	}
	}
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

	//Con_Printf("%s\n", mod->name);
	for (i = 0; i < mod->numtextures; ++i) {
		texture_t* tex = mod->textures[i];

		if (!tex) {
			continue;
		}

		if (!tex->loaded || tex->gl_texture_array == 0) {
			continue;
		}

		//Con_Printf("  %2d: %s, %d [%2d] %s\n", i, tex->name, tex->gl_texture_array, tex->next_same_size, tex->size_start ? "start" : "cont");
	}

	for (i = 0; i < mod->texture_array_count; ++i) {
		//Con_Printf("A %2d: %d first %d, %.3f x %.3f\n", i, mod->texture_arrays[i], mod->texture_array_first[i], mod->texture_arrays_scale_s[i], mod->texture_arrays_scale_t[i]);
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

		//Con_Printf("  Surface %d (tex %d)\n", i, s->texinfo->texture->);
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

void GL_ImportTexturesForModel(model_t* mod, common_texture_t* common, common_texture_t* commonTex, int maxWidth, int maxHeight, glm_vbo_t model_vbo, float* new_vbo_buffer, int* new_vbo_position)
{
	int count = 0;
	int j;

	if (mod->type == mod_alias) {
		aliashdr_t* paliashdr = (aliashdr_t *)Mod_Extradata(mod);
		qbool any_size = AliasModelIsAnySize(mod);

		for (j = 0; j < paliashdr->numskins; ++j) {
			int anim;
			for (anim = 0; anim < 4; ++anim) {
				if (anim == 0 || paliashdr->gl_texturenum[j][anim] != paliashdr->gl_texturenum[j][anim - 1]) {
					GL_CopyToTextureArraySize(common, paliashdr->gl_texturenum[j][anim], any_size, &paliashdr->gl_scalingS[j][anim], &paliashdr->gl_scalingT[j][anim], NULL, &paliashdr->gl_arrayindex[j][anim]);
				}
				else {
					paliashdr->gl_arrayindex[j][anim] = paliashdr->gl_arrayindex[j][anim - 1];
					paliashdr->gl_scalingS[j][anim] = paliashdr->gl_scalingS[j][anim - 1];
					paliashdr->gl_scalingT[j][anim] = paliashdr->gl_scalingT[j][anim - 1];
				}

				if (anim == 0 || paliashdr->fb_texturenum[j][anim] != paliashdr->fb_texturenum[j][anim - 1]) {
					float fb_s, fb_t;

					GL_CopyToTextureArraySize(common, paliashdr->fb_texturenum[j][anim], any_size, &fb_s, &fb_t, NULL, &paliashdr->gl_fb_arrayindex[j][anim]);
				}
				else {
					paliashdr->gl_fb_arrayindex[j][anim] = paliashdr->gl_fb_arrayindex[j][anim - 1];
				}
			}
		}

		for (j = 0; j < MAX_SIMPLE_TEXTURES; ++j) {
			if (mod->simpletexture[j]) {
				GL_CopyToTextureArraySize(common, mod->simpletexture[j], true, &mod->simpletexture_scalingS[j], &mod->simpletexture_scalingT[j], &mod->simpletexture_array, &mod->simpletexture_indexes[j]);
			}
		}

		GL_SetModelTextureArray(mod, commonTex->gl_texturenum, commonTex->width * 1.0f / maxWidth, commonTex->height * 1.0f / maxHeight);

		// Copy VBO info to buffer (FIXME: Free the memory?  but is cached.  But CacheAlloc() fails... argh)
		memcpy(&new_vbo_buffer[(*new_vbo_position) * MODELVERTEXSIZE], mod->temp_vbo_buffer, mod->vertsInVBO * MODELVERTEXSIZE * sizeof(float));
		//Q_free(mod->temp_vbo_buffer);

		mod->vao_simple = mod->vao = model_vao;
		mod->vbo = model_vbo;
		mod->vbo_start = *new_vbo_position;

		paliashdr->vbo = model_vbo;
		paliashdr->vao = model_vao;
		paliashdr->vertsOffset = *new_vbo_position;

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
			GL_CopyToTextureArraySize(common, frame->gl_texturenum, true, &frame->gl_scalingS, &frame->gl_scalingT, &frame->gl_arraynum, &frame->gl_arrayindex);
		}

		mod->vao_simple = model_vao;
		// FIXME: scaling factors
		GL_SetModelTextureArray(mod, commonTex->gl_texturenum, 0.5f, 0.5f);
		mod->vbo = model_vbo;
		mod->vbo_start = 0;
	}
	else if (mod->type == mod_brush) {
		GLM_FindBrushModelTextureExtents(mod);

		for (j = 0; j < MAX_SIMPLE_TEXTURES; ++j) {
			if (mod->simpletexture[j]) {
				GL_CopyToTextureArraySize(common, mod->simpletexture[j], true, &mod->simpletexture_scalingS[j], &mod->simpletexture_scalingT[j], &mod->simpletexture_array, &mod->simpletexture_indexes[j]);
			}
		}
		mod->vao_simple = model_vao;
		mod->vbo_start = 0;

		for (j = 0; j < mod->numtextures; ++j) {
			texture_t* tex = mod->textures[j];
			if (tex && tex->loaded && !tex->gl_texture_array) {
				GL_CopyToTextureArraySize(common, tex->gl_texturenum, BrushModelIsAnySize(mod), &tex->gl_texture_scaleS, &tex->gl_texture_scaleT, &tex->gl_texture_array, &tex->gl_texture_index);
			}
		}

		mod->texture_array_count = GLM_CountTextureArrays(mod);
		if (mod->texture_array_count >= MAX_TEXTURE_ARRAYS_PER_MODEL) {
			Sys_Error("Model %s has >= %d texture dimensions", mod->name, MAX_TEXTURE_ARRAYS_PER_MODEL);
			return;
		}

		GLM_SetTextureArrays(mod);
		if (mod == cl.worldmodel) {
			GLM_PrintTextureArrays(mod);
		}
	}
	else {
		//Con_Printf("***: type %d (%s)\n", mod->type, mod->name);
	}
}

static void GLM_CreateSpriteVBO(float* new_vbo_buffer)
{
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
}

// Called from R_NewMap
void GL_BuildCommonTextureArrays(void)
{
	common_texture_t* common = Q_malloc(sizeof(common_texture_t));
	int required_vbo_length = 4;
	static glm_vbo_t model_vbo;
	int i;

	GL_DeleteModelData();

	GL_GenBuffer(&model_vbo, __FUNCTION__);
	GL_GenVertexArray(&model_vao);

	for (i = 1; i < MAX_MODELS; ++i) {
		model_t* mod = cl.model_precache[i];

		if (mod && (mod == cl.worldmodel || !mod->isworldmodel)) {
			GL_MeasureTexturesForModel(mod, common, &required_vbo_length);
		}
	}

	for (i = 0; i < MAX_VWEP_MODELS; i++) {
		model_t* mod = cl.vw_model_precache[i];

		if (mod) {
			GL_MeasureTexturesForModel(mod, common, &required_vbo_length);
		}
	}

	{
		// Find highest dimensions, stick everything in there for the moment unless texture is tiling
		// FIXME: this is a memory vs texture-switch tradeoff
		int maxWidth = 0;
		int maxHeight = 0;
		common_texture_t* tex;
		common_texture_t* commonTex = NULL;
		int anySizeCount = 0;
		float* new_vbo_buffer = Q_malloc(required_vbo_length * MODELVERTEXSIZE * sizeof(float));
		int new_vbo_position = 0;

		for (tex = common; tex; tex = tex->next) {
			if (tex->width == 0 && tex->height == 0) {
				commonTex = tex;
			}
			else {
				maxWidth = max(maxWidth, tex->width);
				maxHeight = max(maxHeight, tex->height);
			}
			anySizeCount += tex->any_size_count;
		}

		// Create non-specific array to fit everything that doesn't require tiling
		commonTex->gl_texturenum = GL_CreateTextureArray("", maxWidth, maxHeight, anySizeCount + 1, TEX_MIPMAP);
		commonTex->gl_width = maxWidth;
		commonTex->gl_height = maxHeight;

		// VBO starts with simple-model/sprite vertices
		GLM_CreateSpriteVBO(new_vbo_buffer);
		new_vbo_position = 4;

		// First texture is the powerup shell
		GLM_CreatePowerupShellTexture(commonTex->gl_texturenum, maxWidth, maxHeight, commonTex->allocated++);

		// Go back through all models, importing textures into arrays and creating new VBO
		for (i = 1; i < MAX_MODELS; ++i) {
			model_t* mod = cl.model_precache[i];

			if (mod) {
				GL_ImportTexturesForModel(mod, common, commonTex, maxWidth, maxHeight, model_vbo, new_vbo_buffer, &new_vbo_position);
			}
		}

		for (i = 0; i < MAX_VWEP_MODELS; i++) {
			model_t* mod = cl.vw_model_precache[i];

			if (mod) {
				GL_ImportTexturesForModel(mod, common, commonTex, maxWidth, maxHeight, model_vbo, new_vbo_buffer, &new_vbo_position);
			}
		}

		GLM_CreateInstanceVBO();
		GLM_CreateModelVAO(model_vbo.vbo, required_vbo_length, new_vbo_buffer);
		GLM_CreateBrushModelVAO();
	}

	GL_FreeTextureSizeList(common);
}

static void GLM_CreateModelVAO(GLuint model_vbo, GLuint required_vbo_length, float* new_vbo_buffer)
{
	glBindBufferExt(GL_ARRAY_BUFFER, model_vbo);
	glBufferDataExt(GL_ARRAY_BUFFER, required_vbo_length * MODELVERTEXSIZE * sizeof(float), new_vbo_buffer, GL_STATIC_DRAW);
	Q_free(new_vbo_buffer);

	GL_BindVertexArray(model_vao.vao);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glEnableVertexAttribArray(3);
	glBindBufferExt(GL_ARRAY_BUFFER, model_vbo);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * MODELVERTEXSIZE, (void*)0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * MODELVERTEXSIZE, (void*)(sizeof(float) * 3));
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(float) * MODELVERTEXSIZE, (void*)(sizeof(float) * 5));
	glBindBufferExt(GL_ARRAY_BUFFER, instance_vbo.vbo);
	glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT, sizeof(GLuint), 0);
	glVertexAttribDivisor(3, 1);
}

static void GLM_CreatePowerupShellTexture(GLuint texture_array, int maxWidth, int maxHeight, int slice)
{
	float x, y, d;
	int level = 0;
	int height = maxHeight;
	int width = maxWidth;
	int minDimensions = min(maxWidth, maxHeight);
	byte* data = Q_malloc(4 * maxWidth * maxHeight);
	extern GLuint shelltexture2;

	while (width && height) {
		memset(data, 0, 4 * maxWidth * maxHeight);
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				int base = (x * height + y) * 4;

				d = (sin(4 * x * M_PI / minDimensions) + cos(4 * y * M_PI / minDimensions)) * 64 + 64;
				d = bound(0, d, 255);

				data[base] = data[base + 1] = data[base + 2] = d;
				data[base + 3] = 255;
			}
		}
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, level, 0, 0, 0, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);

		++level;
		width /= 2;
		height /= 2;
		minDimensions /= 2;
	}

	//GL_LoadTexture("shelltexture", 32, 32, &data[0][0][0], TEX_MIPMAP, 4);
	Q_free(data);
}

static glm_vbo_t brushModel_vbo;
static glm_vao_t brushModel_vao;

static void GLM_CreateBrushModelVAO(void)
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
	GL_GenBuffer(&brushModel_vbo, __FUNCTION__);
	glBindBufferExt(GL_ARRAY_BUFFER, brushModel_vbo.vbo);

	// Create vao
	GL_GenVertexArray(&brushModel_vao);
	GL_BindVertexArray(brushModel_vao.vao);

	// Copy data into buffer
	for (i = 1; i < MAX_MODELS; ++i) {
		model_t* mod = cl.model_precache[i];
		if (mod && mod->type == mod_brush) {
			if (mod == cl.worldmodel || !mod->isworldmodel) {
				position = GLM_PopulateVBOForBrushModel(mod, buffer, position);
			}
			mod->vao = brushModel_vao;
		}
	}

	for (i = 0; i < MAX_VWEP_MODELS; i++) {
		model_t* mod = cl.vw_model_precache[i];
		if (mod && mod->type == mod_brush) {
			position = GLM_PopulateVBOForBrushModel(mod, buffer, position);
			mod->vao = brushModel_vao;
		}
	}

	// Copy VBO buffer across
	glBindBufferExt(GL_ARRAY_BUFFER, brushModel_vbo.vbo);
	glBufferDataExt(GL_ARRAY_BUFFER, size * sizeof(vbo_world_vert_t), buffer, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vbo_world_vert_t), VBO_VERT_FOFS(position));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vbo_world_vert_t), VBO_VERT_FOFS(material_coords));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_UNSIGNED_SHORT, GL_TRUE, sizeof(vbo_world_vert_t), VBO_VERT_FOFS(lightmap_coords));
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(vbo_world_vert_t), VBO_VERT_FOFS(detail_coords));
	glEnableVertexAttribArray(4);
	glVertexAttribIPointer(4, 1, GL_SHORT, sizeof(vbo_world_vert_t), VBO_VERT_FOFS(lightmap_index));
	glEnableVertexAttribArray(5);
	glVertexAttribIPointer(5, 1, GL_SHORT, sizeof(vbo_world_vert_t), VBO_VERT_FOFS(material_index));
	glEnableVertexAttribArray(7);
	glVertexAttribIPointer(7, 1, GL_UNSIGNED_BYTE, sizeof(vbo_world_vert_t), VBO_VERT_FOFS(flags));
	glEnableVertexAttribArray(8);
	glVertexAttribPointer(8, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vbo_world_vert_t), VBO_VERT_FOFS(flatcolor));

	glEnableVertexAttribArray(6);
	glBindBufferExt(GL_ARRAY_BUFFER, instance_vbo.vbo);
	glVertexAttribIPointer(6, 1, GL_UNSIGNED_INT, sizeof(GLuint), 0);
	glVertexAttribDivisor(6, 1);

	Q_free(buffer);
}
