/*

Copyright (C) 1996-2003 A Nourai, Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef _IMAGE_H

#define _IMAGE_H

#if defined(WITH_PNG) && !defined(WITH_ZLIB)
#error WITH_PNG requires WITH_ZLIB
#endif

void Image_Init(void);

void Image_Resample (void *indata, int inwidth, int inheight,
					 void *outdata, int outwidth, int outheight, int bpp, int quality);
void Image_MipReduce (const byte *in, byte *out, int *width, int *height, int bpp);

#if defined(WITH_PNG)
#include <png.h>

typedef struct
{
	byte *data;
	png_textp textchunks;
	size_t text_count;
} png_data;

png_textp Image_LoadPNG_Comments (char *filename, int *text_count);
png_data *Image_LoadPNG_All (vfsfile_t *vin, const char *filename, int matchwidth, int matchheight, int loadflag, int *real_width, int *real_height);
#endif

byte *Image_LoadPNG (vfsfile_t *v, const char *path, int matchwidth, int matchheight, int *real_width, int *real_height);
byte *Image_LoadTGA (vfsfile_t *v, const char *path, int matchwidth, int matchheight, int *real_width, int *real_height);
byte *Image_LoadPCX (vfsfile_t *v, const char *path, int matchwidth, int matchheight, int *real_width, int *real_height);
byte *Image_LoadJPEG(vfsfile_t *v, const char *path, int matchwidth, int matchheight, int *real_width, int *real_height);
// this does't load 32bit pcx, just convert 8bit color buffer to 32bit buffer, so we can make from this texture
byte *Image_LoadPCX_As32Bit (vfsfile_t *v, const char *path, int matchwidth, int matchheight, int *real_width, int *real_height);

int Image_WritePNG(char *filename, int compression, byte *pixels, size_t width, size_t height);
int Image_WriteTGA(char *filename, byte *pixels, size_t width, size_t height);
int Image_WriteJPEG(char *filename, int quality, byte *pixels, int width, int height);
int Image_WritePCX(char *filename, byte *data, int width, int height, byte *palette);

qbool Image_OpenAPNG(char* filename, int compression, int width, int height, int frames);
qbool Image_WriteAPNGFrame(byte* pixels, size_t width, size_t height, int fps);
qbool Image_CloseAPNG(void);

extern cvar_t image_jpeg_quality_level, image_png_compression_level;

#endif	//_IMAGE_H

