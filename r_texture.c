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

// 
#include "quakedef.h"
#include "gl_model.h"
#include "r_local.h"
#include "r_texture.h"
#include "r_texture_internal.h"
#include "r_lighting.h"
#include "r_aliasmodel.h"
#include "r_brushmodel_sky.h"
#include "r_trace.h"
#include "r_state.h"
#include "gl_texture.h"
#include "r_renderer.h"
#include "image.h"

#ifdef WITH_RENDERING_TRACE
#include "utils.h"
#endif

static void R_AllocateTextureNames(gltexture_t* glt);

gltexture_t  gltextures[MAX_GLTEXTURES];
static int   numgltextures = 1;
static int   next_free_texture = 0;
static texture_ref invalid_texture_reference = { 0 };

static void R_ClearModelTextureReferences(model_t* mod, qbool all_textures)
{
	int j;

	if (!mod) {
		return;
	}

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
					R_TextureReferenceInvalidate(tex->fb_texturenum);
					R_TextureReferenceInvalidate(tex->gl_texturenum);
				}
				tex->gl_texture_scaleS = tex->gl_texture_scaleT = 0;
				R_TextureReferenceInvalidate(tex->gl_texture_array);
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
				R_TextureReferenceInvalidate(frame->gl_texturenum);
			}
			frame->gl_scalingS = frame->gl_scalingT = 0;
			R_TextureReferenceInvalidate(frame->gl_arraynum);
			frame->gl_arrayindex = 0;
		}
	}

	if (mod->type == mod_alias && all_textures) {
		aliashdr_t* paliashdr = (aliashdr_t *)Mod_Extradata(mod);

		memset(paliashdr->gl_texturenum, 0, sizeof(paliashdr->gl_texturenum));
		memset(paliashdr->glc_fb_texturenum, 0, sizeof(paliashdr->glc_fb_texturenum));
	}
	else if (mod->type == mod_alias3 && all_textures) {
		md3model_t* md3Model = (md3model_t *)Mod_Extradata(mod);
		surfinf_t* surfaceInfo = MD3_ExtraSurfaceInfoForModel(md3Model);

		// One day there will be more than one of these...
		R_TextureReferenceInvalidate(surfaceInfo->texnum);
	}
}

void R_TexturesInvalidateAllReferences(void)
{
	int i;

	for (i = 1; i < MAX_MODELS; ++i) {
		R_ClearModelTextureReferences(cl.model_precache[i], true);
	}

	for (i = 0; i < MAX_VWEP_MODELS; i++) {
		R_ClearModelTextureReferences(cl.vw_model_precache[i], true);
	}
}

void R_ClearModelTextureData(void)
{
	int i;

	for (i = 0; i < MAX_MODELS; ++i) {
		R_ClearModelTextureReferences(cl.model_precache[i], false);
	}

	for (i = 0; i < MAX_VWEP_MODELS; ++i) {
		R_ClearModelTextureReferences(cl.vw_model_precache[i], false);
	}
}

qbool R_TextureValid(texture_ref ref)
{
	return ref.index && ref.index < numgltextures && gltextures[ref.index].texnum;
}

qbool R_TexturesAreSameSize(texture_ref tex1, texture_ref tex2)
{
	assert(tex1.index < sizeof(gltextures) / sizeof(gltextures[0]));
	assert(tex2.index < sizeof(gltextures) / sizeof(gltextures[0]));

	return
		(gltextures[tex1.index].texture_width == gltextures[tex2.index].texture_width) &&
		(gltextures[tex1.index].texture_height == gltextures[tex2.index].texture_height);
}

int R_TextureWidth(texture_ref ref)
{
	assert(ref.index && ref.index < numgltextures);
	if (ref.index >= numgltextures) {
		return 0;
	}

	return gltextures[ref.index].texture_width;
}

int R_TextureHeight(texture_ref ref)
{
	assert(ref.index && ref.index < numgltextures);
	if (ref.index >= numgltextures) {
		return 0;
	}

	return gltextures[ref.index].texture_height;
}

int R_TextureDepth(texture_ref ref)
{
	assert(ref.index && ref.index < numgltextures);
	assert(gltextures[ref.index].is_array);
	if (ref.index >= numgltextures || !gltextures[ref.index].is_array) {
		return 0;
	}

	return gltextures[ref.index].depth;
}

void R_TextureModeForEach(void(*func)(texture_ref tex, qbool mipmap, qbool viewmodel))
{
	int i;
	gltexture_t* glt;

	// Make sure we set the proper texture filters for textures.
	for (i = 1, glt = gltextures + 1; i < numgltextures; i++, glt++) {
		if (!R_TextureReferenceIsValid(glt->reference)) {
			continue;
		}

		if (glt->texmode & TEX_NO_TEXTUREMODE) {
			continue;	// This texture must NOT be affected by texture mode changes,
						// for example charset which rather controlled by gl_smoothfont.
		}

		func(glt->reference, glt->texmode & TEX_MIPMAP, glt->texmode & TEX_VIEWMODEL);
	}
}

const char* R_TextureIdentifier(texture_ref ref)
{
	assert(ref.index < sizeof(gltextures) / sizeof(gltextures[0]));

	if (ref.index <= 0 || ref.index >= sizeof(gltextures) / sizeof(gltextures[0])) {
		return "null-texture";
	}

	return gltextures[ref.index].identifier[0] ? gltextures[ref.index].identifier : "unnamed-texture";
}

void R_TextureSetFlag(texture_ref ref, int mode)
{
	if (ref.index <= 0 || ref.index >= sizeof(gltextures) / sizeof(gltextures[0]))
		return;

	gltextures[ref.index].texmode = mode;
}

int R_TextureGetFlag(texture_ref ref)
{
	if (ref.index <= 0 || ref.index >= sizeof(gltextures) / sizeof(gltextures[0]))
		return 0;

	return gltextures[ref.index].texmode;
}

void R_TextureSetDimensions(texture_ref ref, int width, int height)
{
	assert(ref.index < sizeof(gltextures) / sizeof(gltextures[0]));

	gltextures[ref.index].texture_width = width;
	gltextures[ref.index].texture_height = height;
}

void R_Texture_Init(void)
{
	int i;

	// Reset some global vars, probably we need here even more...

	// Reset textures array and linked globals
	for (i = 0; i < numgltextures; i++) {
		if (gltextures[i].texnum) {
			renderer.TextureDelete(gltextures[i].reference);
		}
		Q_free(gltextures[i].pathname);
	}

	memset(gltextures, 0, sizeof(gltextures));

	numgltextures = 1;
	next_free_texture = 0;

	renderer.TextureInitialiseState();

	// Powerup shells.
	R_TextureReferenceInvalidate(shelltexture); // Force reload.

	// Sky.
	memset(skyboxtextures, 0, sizeof(skyboxtextures)); // Force reload.

	R_TextureRegisterCvars();
}

void R_DeleteTexture(texture_ref* texture)
{
	if (!texture || !R_TextureReferenceIsValid(*texture)) {
		return;
	}

	// Delete renderer's version
	if (renderer.TextureDelete) {
		renderer.TextureDelete(*texture);
	}

	// Free structure, updated linked list so we can re-use this slot
	Q_free(gltextures[texture->index].pathname);
	memset(&gltextures[texture->index], 0, sizeof(gltextures[0]));
	gltextures[texture->index].next_free = next_free_texture;
	next_free_texture = texture->index;

	// Invalidate the reference
	texture->index = 0;
}

void R_DeleteCubeMap(texture_ref* texture)
{
	R_DeleteTexture(texture);
}

void R_DeleteTextureArray(texture_ref* texture)
{
	R_DeleteTexture(texture);
}

void R_GenerateMipmapsIfNeeded(texture_ref ref)
{
	assert(ref.index && ref.index < numgltextures);
	if (!ref.index || ref.index >= numgltextures) {
		return;
	}

	if (!gltextures[ref.index].mipmaps_generated) {
		renderer.TextureMipmapGenerate(ref);
		gltextures[ref.index].mipmaps_generated = true;
	}
}

// Called during vid_shutdown
void R_DeleteTextures(void)
{
	extern void R_InvalidateLightmapTextures(void);
	int i;

	R_InvalidateLightmapTextures();
	Skin_InvalidateTextures();

#ifdef RENDERER_OPTION_MODERN_OPENGL
	{
		extern texture_ref particletexture_array;
		R_TextureReferenceInvalidate(particletexture_array);
	}
#endif

	for (i = 0; i < numgltextures; ++i) {
		R_DeleteTexture(&gltextures[i].reference);
	}

	memset(gltextures, 0, sizeof(gltextures));
	numgltextures = 0;
	next_free_texture = 0;
}

texture_ref R_CreateCubeMap(const char* identifier, int width, int height, int mode)
{
	qbool new_texture;
	gltexture_t* slot = R_TextureAllocateSlot(texture_type_cubemap, identifier, width, height, 0, 4, mode | TEX_NOSCALE, 0, &new_texture);

	if (!slot) {
		return invalid_texture_reference;
	}

	if (slot && !new_texture) {
		return slot->reference;
	}

	R_TextureUtil_SetFiltering(slot->reference);
	renderer.TextureWrapModeClamp(slot->reference);

	return slot->reference;
}

gltexture_t* R_TextureAllocateSlot(r_texture_type_id type, const char* identifier, int width, int height, int depth, int bpp, int mode, unsigned short crc, qbool* new_texture)
{
	int gl_width, gl_height;
	gltexture_t* glt = NULL;
	qbool load_over_existing = false;
	int miplevels = 1;

	*new_texture = true;

	if (lightmode != 2) {
		mode &= ~TEX_BRIGHTEN;
	}
	if (bpp == 1 && (mode & TEX_FULLBRIGHT)) {
		mode |= TEX_ALPHA;
	}

	R_TextureUtil_ImageDimensionsToTexture(width, height, &gl_width, &gl_height, mode);

	if (mode & TEX_MIPMAP) {
		int temp_w = gl_width;
		int temp_h = gl_height;

		while (temp_w > 1 || temp_h > 1) {
			temp_w = max(1, temp_w / 2);
			temp_h = max(1, temp_h / 2);
			++miplevels;
		}
	}

	// If we were given an identifier for the texture, search through
	// the list of loaded texture and see if we find a match, if so
	// return the texnum for the already loaded texture.
	if (identifier[0]) {
		int i;
		for (i = 0, glt = gltextures; i < numgltextures; i++, glt++) {
			if (!strncmp(identifier, glt->identifier, sizeof(glt->identifier) - 1)) {
				qbool same_dimensions = (width == glt->image_width && height == glt->image_height && depth == glt->depth);
				qbool same_scaling = (gl_width == glt->texture_width && gl_height == glt->texture_height);
				qbool same_format = (glt->bpp == bpp && glt->type == type);
				qbool same_options = (mode & ~(TEX_COMPLAIN | TEX_NOSCALE)) == (glt->texmode & ~(TEX_COMPLAIN | TEX_NOSCALE));

				// Identifier matches, make sure everything else is the same
				// so that we can be really sure this is the correct texture.
				if (same_dimensions && same_scaling && same_format && same_options && crc == glt->crc) {
					*new_texture = false;
					return glt;
				}
				else {
					// Same identifier but different texture, so overwrite
					// the already loaded texture.
					load_over_existing = true;
					break;
				}
			}
		}
	}

	// If the identifier was the same as another textures, we won't bother
	// with taking up a new texture slot, just load the new texture
	// over the old one.
	if (!load_over_existing) {
		glt = R_NextTextureSlot(type);

		strlcpy(glt->identifier, identifier, sizeof(glt->identifier));
		R_AllocateTextureNames(glt);
		renderer.TextureLabelSet(glt->reference, glt->identifier);
	}
	else if (glt && !glt->texnum) {
		R_AllocateTextureNames(glt);
		renderer.TextureLabelSet(glt->reference, glt->identifier);
	}
	else if (glt && glt->storage_allocated) {
		if (gl_width != glt->texture_width || gl_height != glt->texture_height || glt->bpp != bpp) {
			texture_ref ref = glt->reference;

			R_DeleteTexture(&ref);

			return R_TextureAllocateSlot(type, identifier, width, height, 0, bpp, mode, crc, new_texture);
		}
	}

	if (!glt) {
		Sys_Error("R_LoadTexture: glt not initialized\n");
	}

	glt->image_width = width;
	glt->image_height = height;
	glt->depth = depth;
	glt->texmode = mode;
	glt->crc = crc;
	glt->bpp = bpp;
	glt->is_array = (depth > 0);
	glt->texture_width = gl_width;
	glt->texture_height = gl_height;
	glt->miplevels = miplevels;
	Q_free(glt->pathname);
	if (bpp == 4 && fs_netpath[0]) {
		glt->pathname = Q_strdup(fs_netpath);
	}

	// Allocate storage
	if (!glt->storage_allocated && !glt->is_array) {
		if (R_UseImmediateOpenGL() || R_UseModernOpenGL()) {
			GL_AllocateStorage(glt);
		}
		else if (R_UseVulkan()) {
			// VK_AllocateStorage(glt);
		}
		glt->storage_allocated = true;
	}

	return glt;
}

void R_SetTextureArraySize(texture_ref tex, int width, int height, int depth, int bpp)
{
	gltextures[tex.index].bpp = bpp;
	gltextures[tex.index].texture_width = width;
	gltextures[tex.index].texture_height = height;
	gltextures[tex.index].depth = depth;
	gltextures[tex.index].is_array = true;
}

// We could flag the textures as they're created and then move all 2d>3d to this module?
// FIXME: Ensure not called in immediate-mode OpenGL
texture_ref R_CreateTextureArray(const char* identifier, int width, int height, int depth, int mode)
{
	qbool new_texture = false;
	gltexture_t* slot;
	texture_ref gl_texturenum;

	slot = R_TextureAllocateSlot(texture_type_2d_array, identifier, width, height, depth, 4, mode | TEX_NOSCALE, 0, &new_texture);
	if (!slot) {
		return invalid_texture_reference;
	}

	if (slot && !new_texture) {
		return slot->reference;
	}

	gl_texturenum = slot->reference;

#ifdef RENDERER_OPTION_MODERN_OPENGL
	if (R_UseModernOpenGL()) {
		texture_ref tex = slot->reference;

		renderer.TextureUnitBind(0, tex);
		if (GLM_TextureAllocateArrayStorage(slot)) {
			R_TextureUtil_SetFiltering(slot->reference);
			return tex;
		}

		R_DeleteTexture(&tex);
		return null_texture_reference;
	}
#endif
#ifdef RENDERER_OPTION_VULKAN
	if (R_UseVulkan()) {

	}
#endif

	return gl_texturenum;
}

gltexture_t* R_NextTextureSlot(r_texture_type_id type)
{
	int slot;
	gltexture_t* glt;

	// Re-use a deleted slot if possible
	if (next_free_texture) {
		slot = next_free_texture;
		next_free_texture = gltextures[slot].next_free;
		gltextures[slot].next_free = 0;
	}
	else if (numgltextures >= MAX_GLTEXTURES) {
		Sys_Error("R_LoadTexture: numgltextures == MAX_GLTEXTURES");
		return NULL;
	}
	else {
		slot = numgltextures++;
	}

	glt = &gltextures[slot];
	glt->reference.index = slot;
	glt->type = type;
	return glt;
}

static void R_AllocateTextureNames(gltexture_t* glt)
{
	if (R_UseImmediateOpenGL()) {
		GL_AllocateTextureNames(glt);
	}
	else if (R_UseModernOpenGL()) {
		GL_AllocateTextureNames(glt);
	}
	else if (R_UseVulkan()) {
		// VK_AllocateTextureNames(...);
	}
}

gltexture_t* R_FindTexture(const char *identifier)
{
	int i;

	for (i = 0; i < numgltextures; i++) {
		if (!strcmp(identifier, gltextures[i].identifier)) {
			return &gltextures[i];
		}
	}

	return NULL;
}

void R_AllocateTextureReferences(r_texture_type_id type_id, int width, int height, int mode, int number, texture_ref* references)
{
	int i;

	for (i = 0; i < number; ++i) {
		qbool new_texture;
		gltexture_t* slot = R_TextureAllocateSlot(type_id, "", width, height, 0, 4, mode | TEX_NOSCALE, 0, &new_texture);

		if (slot) {
			references[i] = slot->reference;
		}
		else {
			references[i] = null_texture_reference;
		}
	}
}

r_texture_type_id R_TextureType(texture_ref ref)
{
	return gltextures[ref.index].type;
}

void R_TextureFindIdentifierByReference(unsigned int ref, char* label, int labelsize)
{
	int i;

	for (i = 0; i < numgltextures; ++i) {
		if (gltextures[i].texnum == ref) {
			strlcpy(label, gltextures[i].identifier, labelsize);
			return;
		}
	}

	label[0] = '\0';
}

// re-scale texture to match underlying (useful for fullbrights/lumas etc)
void R_TextureRescaleOverlay(byte** overlay_pixels, int* overlay_width, int* overlay_height, int underlying_width, int underlying_height)
{
	if (underlying_width != *overlay_width || underlying_height != *overlay_height) {
		byte* new_pixels = Q_malloc(underlying_width * underlying_height * 4);

		Image_Resample(*overlay_pixels, *overlay_width, *overlay_height, new_pixels, underlying_width, underlying_height, 4, true);

		Q_free(*overlay_pixels);
		*overlay_pixels = new_pixels;
		*overlay_width = underlying_width;
		*overlay_height = underlying_height;
	}
}

int R_TextureCount(void)
{
	return numgltextures;
}

#ifdef WITH_RENDERING_TRACE
void Dev_TextureList(void)
{
	int i = 0;

	for (i = 1; i < numgltextures; ++i) {
		if (Cmd_Argc() > 1 && !Utils_RegExpMatch(Cmd_Argv(1), gltextures[i].identifier)) {
			continue;
		}

		if (gltextures[i].texnum && gltextures[i].texture_width) {
			Con_Printf("%03d: %s, %d (%dx%d %d)\n", i, gltextures[i].identifier, gltextures[i].texture_width, gltextures[i].texture_height, gltextures[i].texmode);
		}
	}
}
#endif
