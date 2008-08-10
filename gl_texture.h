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

#ifndef __GL_TEXTURE_H

#define __GL_TEXTURE_H

#define	TEX_COMPLAIN		(1<<0)
#define TEX_MIPMAP			(1<<1)
#define TEX_ALPHA			(1<<2)
#define TEX_LUMA			(1<<3)
#define TEX_FULLBRIGHT		(1<<4)
#define	TEX_NOSCALE			(1<<5)
#define	TEX_BRIGHTEN		(1<<6)
#define TEX_NOCOMPRESS		(1<<7)
#define TEX_NO_PCX			(1<<8) // do not load pcx images

#define MAX_GLTEXTURES 1024


void GL_Bind (int texnum);


void GL_SelectTexture (GLenum target);
void GL_DisableMultitexture(void);
void GL_EnableMultitexture(void);
void GL_EnableTMU (GLenum target);
void GL_DisableTMU(GLenum target);


void GL_Upload8 (byte *data, int width, int height, int mode);
void GL_Upload32 (unsigned *data, int width, int height, int mode);
int GL_LoadTexture (char *identifier, int width, int height, byte *data, int mode, int bpp);
int GL_LoadPicTexture (const char *name, mpic_t *pic, byte *data);


byte *GL_LoadImagePixels (const char *filename, int matchwidth, int matchheight, int mode, int *real_width, int *real_height);
int GL_LoadTexturePixels (byte *, char *, int, int, int);
int GL_LoadTextureImage (char * , char *, int, int, int);
mpic_t *GL_LoadPicImage (const char *, char *, int, int, int);
int GL_LoadCharsetImage (char *, char *);


void GL_Texture_Init(void);


extern int gl_lightmap_format, gl_solid_format, gl_alpha_format;

extern cvar_t gl_max_size, gl_scaleModelTextures, gl_scaleTurbTextures, gl_miptexLevel;
extern cvar_t gl_externalTextures_world, gl_externalTextures_bmodels;
extern cvar_t gl_no24bit;
extern cvar_t gl_wicked_luma_level;

extern int currenttexture;

extern int gl_max_size_default;

#endif	//__GL_TEXTURE_H
