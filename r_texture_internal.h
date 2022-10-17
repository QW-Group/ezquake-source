
#ifndef EZQUAKE_R_TEXTURE_INTERNAL_HEADER
#define EZQUAKE_R_TEXTURE_INTERNAL_HEADER

#include "r_texture.h"

typedef struct gltexture_s {
	// Internal handle
	unsigned int texnum;

	// 
	r_texture_type_id type;
	char        identifier[MAX_QPATH];
	char*       pathname;
	int         image_width, image_height;
	int         texture_width, texture_height;
	int         texmode;
	unsigned    crc;
	int         bpp;
	int         miplevels;

	// Arrays
	qbool       is_array;
	int         depth;

	texture_ref reference;
	int         next_free;

	qbool       mipmaps_generated;
	qbool       storage_allocated;

	texture_minification_id minification_filter;
	texture_magnification_id magnification_filter;
} gltexture_t;

extern gltexture_t gltextures[MAX_GLTEXTURES];

void R_TextureSetDimensions(texture_ref ref, int width, int height);
gltexture_t* R_TextureAllocateSlot(r_texture_type_id type, const char* identifier, int width, int height, int depth, int bpp, int mode, unsigned short crc, qbool* new_texture);

void R_TextureUtil_ScaleDimensions(int width, int height, int *scaled_width, int *scaled_height, int mode);
void R_TextureUtil_Brighten32(byte *data, int size);
void R_TextureUtil_ImageDimensionsToTexture(int imageWidth, int imageHeight, int* textureWidth, int* textureHeight, int mode);

void R_TextureUtil_SetFiltering(texture_ref texture);

gltexture_t* R_NextTextureSlot(r_texture_type_id target);
gltexture_t* R_FindTexture(const char *identifier);

void R_TextureRegisterCvars(void);
void R_TextureModeChanged(texture_ref tex, qbool mipmap, qbool viewmodel);
void R_TextureModeForEach(void(*func)(texture_ref tex, qbool mipmap, qbool viewmodel));

#endif // EZQUAKE_R_TEXTURE_HEADER
