/*

Copyright (C) 2001-2002       A Nourai

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

#ifndef __GL_IMAGE_H

#define __GL_IMAGE_H

void GL_Image_Init(void);

int Image_WriteJPEG(char *filename, int quality, byte *pixels, int width, int height);
int Image_WritePNG(char *filename, int compression, byte *pixels, int width, int height);
int Image_WriteTGA(char *filename, byte *pixels, int width, int height);

void Image_ApplyGamma(byte *buffer, int size) ;

byte *Image_LoadTGA (char *, int, int);
byte *Image_LoadPNG (char *, int, int);

extern cvar_t image_jpeg_quality_level, image_png_compression_level;

extern int image_width, image_height;

#endif