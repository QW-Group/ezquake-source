
#include "quakedef.h"
#include "r_texture.h"
#include "r_texture_internal.h"
#include "tr_types.h"

void R_TextureUtil_ScaleDimensions(int width, int height, int *scaled_width, int *scaled_height, int mode)
{
	int maxsize, picmip;
	qbool scale = (mode & TEX_MIPMAP) && !(mode & TEX_NOSCALE);
	extern cvar_t gl_picmip;

	if (gl_support_arb_texture_non_power_of_two) {
		*scaled_width = width;
		*scaled_height = height;
	}
	else {
		Q_ROUND_POWER2(width, *scaled_width);
		Q_ROUND_POWER2(height, *scaled_height);
	}

	if (scale) {
		picmip = (int)bound(0, gl_picmip.value, 16);
		*scaled_width >>= picmip;
		*scaled_height >>= picmip;
	}

	maxsize = scale ? gl_max_size.value : glConfig.gl_max_size_default;

	*scaled_width = bound(1, *scaled_width, maxsize);
	*scaled_height = bound(1, *scaled_height, maxsize);
}

void R_TextureUtil_Brighten32(byte *data, int size)
{
	byte *p;
	int i;

	p = data;
	for (i = 0; i < size / 4; i++) {
		p[0] = min(p[0] * 2.0 / 1.5, 255);
		p[1] = min(p[1] * 2.0 / 1.5, 255);
		p[2] = min(p[2] * 2.0 / 1.5, 255);
		p += 4;
	}
}

void R_TextureUtil_ImageDimensionsToTexture(int imageWidth, int imageHeight, int* textureWidth, int* textureHeight, int mode)
{
	int scaledWidth, scaledHeight;

	R_TextureUtil_ScaleDimensions(imageWidth, imageHeight, &scaledWidth, &scaledHeight, mode);
	while (imageWidth > scaledWidth || imageHeight > scaledHeight) {
		imageWidth = max(1, imageWidth / 2);
		imageHeight = max(1, imageHeight / 2);
	}
	*textureWidth = imageWidth;
	*textureHeight = imageHeight;
}

void R_TextureUtil_SetFiltering(texture_ref texture)
{
	int mode = gltextures[texture.index].texmode;

	if (mode & TEX_MIPMAP) {
		R_TextureModeChanged(texture);
		R_TextureAnisotropyChanged(texture);
	}
}
