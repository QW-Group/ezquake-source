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
void Image_MipReduce (byte *in, byte *out, int *width, int *height, int bpp);

byte *Image_LoadPNG (FILE *, char *, int, int);
byte *Image_LoadTGA (FILE *, char *, int, int);
byte *Image_LoadPCX (FILE *, char *, int, int);

int Image_WritePNG(char *filename, int compression, byte *pixels, int width, int height);
#ifdef GLQUAKE
int Image_WritePNGPLTE (char *filename, int compression, byte *pixels,
						int width, int height, byte *palette);
#else
int Image_WritePNGPLTE (char *filename, int compression, byte *pixels,
						int width, int height, int rowbytes, byte *palette);
#endif
int Image_WriteTGA(char *filename, byte *pixels, int width, int height);
int Image_WriteJPEG(char *filename, int quality, byte *pixels, int width, int height);
#ifdef GLQUAKE
int Image_WritePCX (char *filename, byte *data, int width, int height, byte *palette);
#else
int Image_WritePCX (char *filename, byte *data, int width, int height, int rowbytes, byte *palette);
#endif

extern cvar_t image_jpeg_quality_level, image_png_compression_level;

extern int image_width, image_height;

#endif	//_IMAGE_H
