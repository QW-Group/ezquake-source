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

    $Id: image.c,v 1.56 2007-10-11 07:02:37 dkure Exp $
*/

#ifdef __FreeBSD__
#include <dlfcn.h>
#endif
#include "quakedef.h"
#include "image.h"

#ifdef WITH_PNG
#include "png.h"
/*#ifdef _WIN32
#pragma comment(lib, "libs/libpng.lib")
#endif*/
#endif

#ifdef WITH_JPEG
#include "jpeglib.h"
#include "jerror.h"
/*#ifdef _WIN32
#pragma comment(lib, "libs/libjpeg.lib")
#endif*/
#endif

/*#ifdef __APPLE__
#include <Carbon/Carbon.h>
#endif*/

#define IMAGE_MAX_DIMENSIONS 4096

#ifndef GLQUAKE
// Dither from 24-bit pictures to the 8-bit quake palette instead of
// just getting the nearest color (make a smother transition) when loading a PNG in software.
cvar_t image_png_dither_onload = {"image_png_dither_onload", "1"}; 
#endif // !GLQUAKE

cvar_t image_png_compression_level = {"image_png_compression_level", "1"};
cvar_t image_jpeg_quality_level = {"image_jpeg_quality_level", "75"};

/***************************** IMAGE RESAMPLING ******************************/

#ifdef GLQUAKE

static void Image_Resample32LerpLine (byte *in, byte *out, int inwidth, int outwidth) 
{
	int j, xi, oldx = 0, f, fstep, endx, lerp;

	fstep = (int) (inwidth * 65536.0f / outwidth);
	endx = (inwidth - 1);
	for (j = 0, f = 0; j < outwidth; j++, f += fstep) {
		xi = (int) f >> 16;
		if (xi != oldx) {
			in += (xi - oldx) * 4;
			oldx = xi;
		}
		if (xi < endx) {
			lerp = f & 0xFFFF;
			*out++ = (byte) ((((in[4] - in[0]) * lerp) >> 16) + in[0]);
			*out++ = (byte) ((((in[5] - in[1]) * lerp) >> 16) + in[1]);
			*out++ = (byte) ((((in[6] - in[2]) * lerp) >> 16) + in[2]);
			*out++ = (byte) ((((in[7] - in[3]) * lerp) >> 16) + in[3]);
		} else  {
			*out++ = in[0];
			*out++ = in[1];
			*out++ = in[2];
			*out++ = in[3];
		}
	}
}

static void Image_Resample24LerpLine (byte *in, byte *out, int inwidth, int outwidth) 
{
	int j, xi, oldx = 0, f, fstep, endx, lerp;

	fstep = (int) (inwidth * 65536.0f / outwidth);
	endx = (inwidth - 1);
	for (j = 0, f = 0; j < outwidth; j++, f += fstep) {
		xi = (int) f >> 16;
		if (xi != oldx) {
			in += (xi - oldx) * 3;
			oldx = xi;
		}
		if (xi < endx) {
			lerp = f & 0xFFFF;
			*out++ = (byte) ((((in[3] - in[0]) * lerp) >> 16) + in[0]);
			*out++ = (byte) ((((in[4] - in[1]) * lerp) >> 16) + in[1]);
			*out++ = (byte) ((((in[5] - in[2]) * lerp) >> 16) + in[2]);
		} else  {
			*out++ = in[0];
			*out++ = in[1];
			*out++ = in[2];
		}
	}
}

#define LERPBYTE(i) r = row1[i]; out[i] = (byte) ((((row2[i] - r) * lerp) >> 16) + r)
#define NOLERPBYTE(i) *out++ = inrow[f + i]

static void Image_Resample32 (void *indata, int inwidth, int inheight,
								void *outdata, int outwidth, int outheight, int quality) 
{
	if (quality) 
	{
		int i, j, r, yi, oldy, f, fstep, endy = (inheight - 1), lerp;
		int inwidth4 = inwidth * 4, outwidth4 = outwidth * 4;
		byte *inrow, *out, *row1, *row2, *memalloc;

		out = outdata;
		fstep = (int) (inheight * 65536.0f / outheight);

		memalloc = (byte *) Q_malloc(2 * outwidth4);
		row1 = memalloc;
		row2 = memalloc + outwidth4;
		inrow = (byte *) indata;
		oldy = 0;
		Image_Resample32LerpLine (inrow, row1, inwidth, outwidth);
		Image_Resample32LerpLine (inrow + inwidth4, row2, inwidth, outwidth);
		
		for (i = 0, f = 0; i < outheight; i++, f += fstep)	
		{
			yi = f >> 16;

			if (yi < endy) 
			{
				lerp = f & 0xFFFF;

				if (yi != oldy)
				{
					inrow = (byte *) indata + inwidth4 * yi;
					if (yi == oldy + 1)
						memcpy(row1, row2, outwidth4);
					else
						Image_Resample32LerpLine (inrow, row1, inwidth, outwidth);
					Image_Resample32LerpLine (inrow + inwidth4, row2, inwidth, outwidth);
					oldy = yi;
				}

				j = outwidth - 4;

				while(j >= 0)
				{
					LERPBYTE(0); LERPBYTE(1); LERPBYTE(2); LERPBYTE(3);
					LERPBYTE(4); LERPBYTE(5); LERPBYTE(6); LERPBYTE(7);
					LERPBYTE(8); LERPBYTE(9); LERPBYTE(10); LERPBYTE(11);
					LERPBYTE(12); LERPBYTE(13); LERPBYTE(14); LERPBYTE(15);
					out += 16;
					row1 += 16;
					row2 += 16;
					j -= 4;
				}

				if (j & 2) 
				{
					LERPBYTE(0); LERPBYTE(1); LERPBYTE(2); LERPBYTE(3);
					LERPBYTE(4); LERPBYTE(5); LERPBYTE(6); LERPBYTE(7);
					out += 8;
					row1 += 8;
					row2 += 8;
				}

				if (j & 1)
				{
					LERPBYTE(0); LERPBYTE(1); LERPBYTE(2); LERPBYTE(3);
					out += 4;
					row1 += 4;
					row2 += 4;
				}
				row1 -= outwidth4;
				row2 -= outwidth4;
			} 
			else 
			{
				if (yi != oldy) 
				{
					inrow = (byte *) indata + inwidth4 * yi;
					if (yi == oldy+1)
						memcpy(row1, row2, outwidth4);
					else
						Image_Resample32LerpLine (inrow, row1, inwidth, outwidth);
					oldy = yi;
				}
				memcpy(out, row1, outwidth4);
			}
		}

		Q_free(memalloc);
	}
	else 
	{
		int i, j;
		unsigned int frac, fracstep, *inrow, *out;

		out = outdata;

		fracstep = inwidth * 0x10000 / outwidth;
		for (i = 0; i < outheight; i++)
		{
			inrow = (unsigned int *) indata + inwidth * (i * inheight / outheight);
			frac = fracstep >> 1;
			j = outwidth - 4;
		
			while (j >= 0)
			{
				out[0] = inrow[frac >> 16]; frac += fracstep;
				out[1] = inrow[frac >> 16]; frac += fracstep;
				out[2] = inrow[frac >> 16]; frac += fracstep;
				out[3] = inrow[frac >> 16]; frac += fracstep;
				out += 4;
				j -= 4;
			}

			if (j & 2)
			{
				out[0] = inrow[frac >> 16]; frac += fracstep;
				out[1] = inrow[frac >> 16]; frac += fracstep;
				out += 2;
			}

			if (j & 1) 
			{
				out[0] = inrow[frac >> 16]; frac += fracstep;
				out += 1;
			}
		}
	}
}

static void Image_Resample24 (void *indata, int inwidth, int inheight,
					 void *outdata, int outwidth, int outheight, int quality)
{
	if (quality)
	{
		int i, j, r, yi, oldy, f, fstep, endy = (inheight - 1), lerp;
		int inwidth3 = inwidth * 3, outwidth3 = outwidth * 3;
		byte *inrow, *out, *row1, *row2, *memalloc;

		out = outdata;
		fstep = (int) (inheight * 65536.0f / outheight);

		memalloc = (byte *) Q_malloc(2 * outwidth3);
		row1 = memalloc;
		row2 = memalloc + outwidth3;
		inrow = (byte *) indata;
		oldy = 0;
		Image_Resample24LerpLine (inrow, row1, inwidth, outwidth);
		Image_Resample24LerpLine (inrow + inwidth3, row2, inwidth, outwidth);
		
		for (i = 0, f = 0; i < outheight; i++, f += fstep)	
		{
			yi = f >> 16;
			if (yi < endy) {
				lerp = f & 0xFFFF;
				if (yi != oldy)
				{
					inrow = (byte *) indata + inwidth3 * yi;
					if (yi == oldy + 1)
						memcpy(row1, row2, outwidth3);
					else
						Image_Resample24LerpLine (inrow, row1, inwidth, outwidth);
					Image_Resample24LerpLine (inrow + inwidth3, row2, inwidth, outwidth);
					oldy = yi;
				}

				j = outwidth - 4;

				while(j >= 0)
				{
					LERPBYTE(0); LERPBYTE(1); LERPBYTE(2);
					LERPBYTE(3); LERPBYTE(4); LERPBYTE(5);
					LERPBYTE(6); LERPBYTE(7); LERPBYTE(8);
					LERPBYTE(9); LERPBYTE(10); LERPBYTE(11);
					out += 12;
					row1 += 12;
					row2 += 12;
					j -= 4;
				}

				if (j & 2) 
				{
					LERPBYTE(0); LERPBYTE(1); LERPBYTE(2);
					LERPBYTE(3); LERPBYTE(4); LERPBYTE(5);
					out += 6;
					row1 += 6;
					row2 += 6;
				}

				if (j & 1) 
				{
					LERPBYTE(0); LERPBYTE(1); LERPBYTE(2);
					out += 3;
					row1 += 3;
					row2 += 3;
				}
				row1 -= outwidth3;
				row2 -= outwidth3;
			} 
			else
			{
				if (yi != oldy) 
				{
					inrow = (byte *) indata + inwidth3 * yi;
					if (yi == oldy+1)
						memcpy(row1, row2, outwidth3);
					else
						Image_Resample24LerpLine (inrow, row1, inwidth, outwidth);
					oldy = yi;
				}

				memcpy(out, row1, outwidth3);
			}
		}

		Q_free(memalloc);
	} 
	else 
	{
		int i, j, f;
		unsigned int frac, fracstep, inwidth3 = inwidth * 3;
		byte *inrow, *out;

		out = outdata;

		fracstep = inwidth * 0x10000 / outwidth;
		for (i = 0; i < outheight; i++) 
		{
			inrow = (byte *) indata + inwidth3 * (i * inheight / outheight);
			frac = fracstep >> 1;
			j = outwidth - 4;

			while (j >= 0) 
			{
				f = (frac >> 16) * 3; NOLERPBYTE(0); NOLERPBYTE(1); NOLERPBYTE(2); frac += fracstep;
				f = (frac >> 16) * 3; NOLERPBYTE(0); NOLERPBYTE(1); NOLERPBYTE(2); frac += fracstep;
				f = (frac >> 16) * 3; NOLERPBYTE(0); NOLERPBYTE(1); NOLERPBYTE(2); frac += fracstep;
				f = (frac >> 16) * 3; NOLERPBYTE(0); NOLERPBYTE(1); NOLERPBYTE(2); frac += fracstep;
				j -= 4;
			}

			if (j & 2) 
			{
				f = (frac >> 16) * 3; NOLERPBYTE(0); NOLERPBYTE(1); NOLERPBYTE(2); frac += fracstep;
				f = (frac >> 16) * 3; NOLERPBYTE(0); NOLERPBYTE(1); NOLERPBYTE(2); frac += fracstep;
				out += 2;
			}

			if (j & 1)
			{
				f = (frac >> 16) * 3; NOLERPBYTE(0); NOLERPBYTE(1); NOLERPBYTE(2); frac += fracstep;
				out += 1;
			}
		}
	}
}

void Image_Resample (void *indata, int inwidth, int inheight,
					 void *outdata, int outwidth, int outheight, int bpp, int quality) 
{
	if (bpp == 4)
		Image_Resample32(indata, inwidth, inheight, outdata, outwidth, outheight, quality);
	else if (bpp == 3)
		Image_Resample24(indata, inwidth, inheight, outdata, outwidth, outheight, quality);
	else
		Sys_Error("Image_Resample: unsupported bpp (%d)", bpp);
}

void Image_MipReduce (byte *in, byte *out, int *width, int *height, int bpp) 
{
	int x, y, nextrow;

	nextrow = *width * bpp;

	if (*width > 1) 
	{
		*width >>= 1;

		if (*height > 1) 
		{
			*height >>= 1;
		
			if (bpp == 4)
			{
				for (y = 0; y < *height; y++) 
				{
					for (x = 0; x < *width; x++) 
					{
						out[0] = (byte) ((in[0] + in[4] + in[nextrow] + in[nextrow + 4]) >> 2);
						out[1] = (byte) ((in[1] + in[5] + in[nextrow + 1] + in[nextrow + 5]) >> 2);
						out[2] = (byte) ((in[2] + in[6] + in[nextrow + 2] + in[nextrow + 6]) >> 2);
						out[3] = (byte) ((in[3] + in[7] + in[nextrow + 3] + in[nextrow + 7]) >> 2);
						out += 4;
						in += 8;
					}
					in += nextrow;
				}
			} 
			else if (bpp == 3) 
			{
				for (y = 0; y < *height; y++)
				{
					for (x = 0; x < *width; x++)
					{
						out[0] = (byte) ((in[0] + in[3] + in[nextrow] + in[nextrow + 3]) >> 2);
						out[1] = (byte) ((in[1] + in[4] + in[nextrow + 1] + in[nextrow + 4]) >> 2);
						out[2] = (byte) ((in[2] + in[5] + in[nextrow + 2] + in[nextrow + 5]) >> 2);
						out += 3;
						in += 6;
					}
					in += nextrow;
				}
			} 
			else 
			{
				Sys_Error("Image_MipReduce: unsupported bpp (%d)", bpp);
			}
		} 
		else 
		{
			if (bpp == 4)
			{
				for (y = 0; y < *height; y++) 
				{
					for (x = 0; x < *width; x++) 
					{
						out[0] = (byte) ((in[0] + in[4]) >> 1);
						out[1] = (byte) ((in[1] + in[5]) >> 1);
						out[2] = (byte) ((in[2] + in[6]) >> 1);
						out[3] = (byte) ((in[3] + in[7]) >> 1);
						out += 4;
						in += 8;
					}
				}
			} 
			else if (bpp == 3) 
			{
				for (y = 0; y < *height; y++) 
				{
					for (x = 0; x < *width; x++)
					{
						out[0] = (byte) ((in[0] + in[3]) >> 1);
						out[1] = (byte) ((in[1] + in[4]) >> 1);
						out[2] = (byte) ((in[2] + in[5]) >> 1);
						out += 3;
						in += 6;
					}
				}
			} 
			else 
			{
				Sys_Error("Image_MipReduce: unsupported bpp (%d)", bpp);
			}
		}
	}
	else if (*height > 1)
	{
		*height >>= 1;

		if (bpp == 4) 
		{
			for (y = 0; y < *height; y++) 
			{
				for (x = 0; x < *width; x++) 
				{
					out[0] = (byte) ((in[0] + in[nextrow]) >> 1);
					out[1] = (byte) ((in[1] + in[nextrow + 1]) >> 1);
					out[2] = (byte) ((in[2] + in[nextrow + 2]) >> 1);
					out[3] = (byte) ((in[3] + in[nextrow + 3]) >> 1);
					out += 4;
					in += 4;
				}
				in += nextrow;
			}
		}
		else if (bpp == 3) 
		{
			for (y = 0; y < *height; y++) 
			{
				for (x = 0; x < *width; x++) 
				{
					out[0] = (byte) ((in[0] + in[nextrow]) >> 1);
					out[1] = (byte) ((in[1] + in[nextrow + 1]) >> 1);
					out[2] = (byte) ((in[2] + in[nextrow + 2]) >> 1);
					out += 3;
					in += 3;
				}
				in += nextrow;
			}
		} 
		else 
		{
			Sys_Error("Image_MipReduce: unsupported bpp (%d)", bpp);
		}
	} 
	else
	{
		Sys_Error("Image_MipReduce: Input texture has dimensions %dx%d", width, height);
	}
}

#endif

/************************************ PNG ************************************/
#ifdef WITH_PNG

// FIXME: If we still want code for dynamically loading pnglib we should make it use the same function for loading, so that stuff doesn't have to be changed in two places like now... 
#ifndef WITH_PNG_STATIC

static QLIB_HANDLETYPE_T png_handle = NULL;
static QLIB_HANDLETYPE_T zlib_handle = NULL;

static void (*qpng_set_sig_bytes)(png_structp, int);
static int (*qpng_sig_cmp)(png_bytep, png_size_t, png_size_t);
static png_structp (*qpng_create_read_struct)(png_const_charp, png_voidp, png_error_ptr, png_error_ptr);
static png_structp (*qpng_create_write_struct)(png_const_charp, png_voidp, png_error_ptr, png_error_ptr);
static png_infop (*qpng_create_info_struct)(png_structp);
static void (*qpng_write_info)(png_structp, png_infop);
static void (*qpng_read_info)(png_structp, png_infop);
static void (*qpng_set_expand)(png_structp);
static void (*qpng_set_gray_1_2_4_to_8)(png_structp);
static void (*qpng_set_palette_to_rgb)(png_structp);
static void (*qpng_set_tRNS_to_alpha)(png_structp);
static void (*qpng_set_gray_to_rgb)(png_structp);
static void (*qpng_set_filler)(png_structp, png_uint_32, int);
static void (*qpng_set_strip_16)(png_structp);
static void (*qpng_read_update_info)(png_structp, png_infop);
static void (*qpng_read_image)(png_structp, png_bytepp);
static void (*qpng_write_image)(png_structp, png_bytepp);
static void (*qpng_write_end)(png_structp, png_infop);
static void (*qpng_read_end)(png_structp, png_infop);
static void (*qpng_destroy_read_struct)(png_structpp, png_infopp, png_infopp);
static void (*qpng_destroy_write_struct)(png_structpp, png_infopp);
static void (*qpng_set_compression_level)(png_structp, int);
static void (*qpng_set_write_fn)(png_structp, png_voidp, png_rw_ptr, png_flush_ptr);
static void (*qpng_set_read_fn)(png_structp, png_voidp, png_rw_ptr);
static png_voidp (*qpng_get_io_ptr)(png_structp);
static png_uint_32 (*qpng_get_valid)(png_structp, png_infop, png_uint_32);
static png_uint_32 (*qpng_get_rowbytes)(png_structp, png_infop);
static png_byte (*qpng_get_channels)(png_structp, png_infop);
static png_byte (*qpng_get_bit_depth)(png_structp, png_infop);
static png_uint_32 (*qpng_get_IHDR)(png_structp, png_infop, png_uint_32 *, png_uint_32 *, int *, int *, int *, int *, int *);
static void (*qpng_set_IHDR)(png_structp, png_infop, png_uint_32, png_uint_32, int, int, int, int, int);
static void (*qpng_set_PLTE)(png_structp, png_infop, png_colorp, int);

#define NUM_PNGPROCS	(sizeof(pngprocs)/sizeof(pngprocs[0]))

qlib_dllfunction_t pngprocs[] = {
	{"png_set_sig_bytes", (void **) &qpng_set_sig_bytes},
	{"png_sig_cmp", (void **) &qpng_sig_cmp},
	{"png_create_read_struct", (void **) &qpng_create_read_struct},
	{"png_create_write_struct", (void **) &qpng_create_write_struct},
	{"png_create_info_struct", (void **) &qpng_create_info_struct},
	{"png_write_info", (void **) &qpng_write_info},
	{"png_read_info", (void **) &qpng_read_info},
	{"png_set_expand", (void **) &qpng_set_expand},
	{"png_set_gray_1_2_4_to_8", (void **) &qpng_set_gray_1_2_4_to_8},
	{"png_set_palette_to_rgb", (void **) &qpng_set_palette_to_rgb},
	{"png_set_tRNS_to_alpha", (void **) &qpng_set_tRNS_to_alpha},
	{"png_set_gray_to_rgb", (void **) &qpng_set_gray_to_rgb},
	{"png_set_filler", (void **) &qpng_set_filler},
	{"png_set_strip_16", (void **) &qpng_set_strip_16},
	{"png_read_update_info", (void **) &qpng_read_update_info},
	{"png_read_image", (void **) &qpng_read_image},
	{"png_write_image", (void **) &qpng_write_image},
	{"png_write_end", (void **) &qpng_write_end},
	{"png_read_end", (void **) &qpng_read_end},
	{"png_destroy_read_struct", (void **) &qpng_destroy_read_struct},
	{"png_destroy_write_struct", (void **) &qpng_destroy_write_struct},
	{"png_set_compression_level", (void **) &qpng_set_compression_level},
	{"png_set_write_fn", (void **) &qpng_set_write_fn},
	{"png_set_read_fn", (void **) &qpng_set_read_fn},
	{"png_get_io_ptr", (void **) &qpng_get_io_ptr},
	{"png_get_valid", (void **) &qpng_get_valid},
	{"png_get_rowbytes", (void **) &qpng_get_rowbytes},
	{"png_get_channels", (void **) &qpng_get_channels},
	{"png_get_bit_depth", (void **) &qpng_get_bit_depth},
	{"png_get_IHDR", (void **) &qpng_get_IHDR},
	{"png_set_IHDR", (void **) &qpng_set_IHDR},
	{"png_set_PLTE", (void **) &qpng_set_PLTE},
};

static void PNG_FreeLibrary(void) {
	if (png_handle)
		QLIB_FREELIBRARY(png_handle);
	if (zlib_handle)
		QLIB_FREELIBRARY(zlib_handle);
}

static qbool PNG_LoadLibrary(void) {
	if (COM_CheckParm("-nolibpng"))
		return false;

#ifdef _WIN32
	if (!(png_handle = LoadLibrary("libpng.dll"))) {
#else
#ifdef __APPLE__
	if (!(png_handle = dlopen("libpng12.dylib", RTLD_NOW))) {
		if (!(zlib_handle = dlopen("libz.dylib", RTLD_NOW))) {
#else
	if (!(png_handle = dlopen("libpng12.so.0", RTLD_NOW)) && !(png_handle = dlopen("libpng.so", RTLD_NOW))) {
		if (!(zlib_handle = dlopen("libz.so", RTLD_NOW | RTLD_GLOBAL))) {
#endif
			QLib_MissingModuleError(QLIB_ERROR_MODULE_NOT_FOUND, "libz", "-nolibpng", "png image features");
			png_handle = zlib_handle = NULL;
			return false;
		}
#ifdef __APPLE__
		if (!(png_handle = dlopen("libpng12.dylib", RTLD_NOW)))
#else
		if (!(png_handle = dlopen("libpng12.so.0", RTLD_NOW)) && !(png_handle = dlopen("libpng.so", RTLD_NOW)))
#endif
#endif
		{
			PNG_FreeLibrary();
			QLib_MissingModuleError(QLIB_ERROR_MODULE_NOT_FOUND, "libpng", "-nolibpng", "png image features");
			png_handle = zlib_handle = NULL;
			return false;
		}
	}

	if (!QLib_ProcessProcdef(png_handle, pngprocs, NUM_PNGPROCS)) {
		PNG_FreeLibrary();
		QLib_MissingModuleError(QLIB_ERROR_MODULE_MISSING_PROC, "libpng", "-nolibpng", "png image features");
		png_handle = zlib_handle = NULL;
		return false;
	}

	return true;
}


#ifndef WITH_FTE_VFS
static void PNG_IO_user_read_data(png_structp png_ptr, png_bytep data, png_size_t length) {
	FILE *f = (FILE *) qpng_get_io_ptr(png_ptr);
	fread(data, 1, length, f);
}

static void PNG_IO_user_write_data(png_structp png_ptr, png_bytep data, png_size_t length) {
	FILE *f = (FILE *) qpng_get_io_ptr(png_ptr);
	fwrite(data, 1, length, f);
}

static void PNG_IO_user_flush_data(png_structp png_ptr) {
	FILE *f = (FILE *) qpng_get_io_ptr(png_ptr);
	fflush(f);
}
#else
static void PNG_IO_user_read_data(png_structp png_ptr, png_bytep data, png_size_t length) {
	vfsfile_t *v = (vfsfile_t *) qpng_get_io_ptr(png_ptr);
	vfserrno_t err;
	VFS_READ(v, data, length, &err);
}

static void PNG_IO_user_write_data(png_structp png_ptr, png_bytep data, png_size_t length) {
	vfsfile_t *v = (vfsfile_t *) qpng_get_io_ptr(png_ptr);
	VFS_WRITE(v, data, length);
}

static void PNG_IO_user_flush_data(png_structp png_ptr) {
	vfsfile_t *v = (vfsfile_t *) qpng_get_io_ptr(png_ptr);
	VFS_FLUSH(v);
}
#endif


#ifndef WITH_FTE_VFS
byte *Image_LoadPNG (FILE *fin, const char *filename, int matchwidth, int matchheight, int *real_width, *real_height) 
#else
byte *Image_LoadPNG (vfsfile_t *fin, const char *filename, int matchwidth, int matchheight, int *real_width, *real_height) 
#endif // WITH_FTE_VFS
{
	byte header[8], **rowpointers, *data;
	png_structp png_ptr;
	png_infop pnginfo;
	int y, width, height, bitdepth, colortype, interlace, compression, filter, bytesperpixel;
	unsigned long rowbytes;

	if (!png_handle)
		return NULL;

#ifndef WITH_FTE_VFS
	if (!fin && FS_FOpenFile (filename, &fin) == -1)
		return NULL;
#else
	if (!fin && !(fin = FS_OpenVFS(filename, "rb", FS_ANY)))
		return NULL;
#endif // WITH_FTE_VFS

	fread(header, 1, 8, fin);

	if (qpng_sig_cmp(header, 0, 8)) {
		Com_DPrintf ("Invalid PNG image %s\n", COM_SkipPath(filename));
#ifndef WITH_FTE_VFS
		fclose(fin);
#else
		VFS_CLOSE(fin);
#endif
		return NULL;
	}

	if (!(png_ptr = qpng_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))) {
#ifndef WITH_FTE_VFS
		fclose(fin);
#else
		VFS_CLOSE(fin);
#endif
		return NULL;
	}

	if (!(pnginfo = qpng_create_info_struct(png_ptr))) {
		qpng_destroy_read_struct(&png_ptr, &pnginfo, NULL);
#ifndef WITH_FTE_VFS
		fclose(fin);
#else
		VFS_CLOSE(fin);
#endif
		return NULL;
	}

	if (setjmp(png_ptr->jmpbuf)) {
		qpng_destroy_read_struct(&png_ptr, &pnginfo, NULL);
#ifndef WITH_FTE_VFS
		fclose(fin);
#else
		VFS_CLOSE(fin);
#endif
		return NULL;
	}

    qpng_set_read_fn(png_ptr, fin, PNG_IO_user_read_data);
	qpng_set_sig_bytes(png_ptr, 8);
	qpng_read_info(png_ptr, pnginfo);
	qpng_get_IHDR(png_ptr, pnginfo, (png_uint_32 *) &width, (png_uint_32 *) &height, &bitdepth,
		&colortype, &interlace, &compression, &filter);

	// Return the width and height.
	if (real_width)
		(*real_width) = width;

	if (real_height)
		(*real_height) = height;

	if (width > IMAGE_MAX_DIMENSIONS || height > IMAGE_MAX_DIMENSIONS) {
		Com_DPrintf ("PNG image %s exceeds maximum supported dimensions\n", COM_SkipPath(filename));
		qpng_destroy_read_struct(&png_ptr, &pnginfo, NULL);
#ifndef WITH_FTE_VFS
		fclose(fin);
#else
		VFS_CLOSE(fin);
#endif
		return NULL;
	}

	if ((matchwidth && width != matchwidth) || (matchheight && height != matchheight)) {
		qpng_destroy_read_struct(&png_ptr, &pnginfo, NULL);
#ifndef WITH_FTE_VFS
		fclose(fin);
#else
		VFS_CLOSE(fin);
#endif
		return NULL;
	}

	if (colortype == PNG_COLOR_TYPE_PALETTE) {
		qpng_set_palette_to_rgb(png_ptr);
		qpng_set_filler(png_ptr, 255, PNG_FILLER_AFTER);		
	}

	if (colortype == PNG_COLOR_TYPE_GRAY && bitdepth < 8) 
		qpng_set_gray_1_2_4_to_8(png_ptr);
	
	if (qpng_get_valid(png_ptr, pnginfo, PNG_INFO_tRNS))	
		qpng_set_tRNS_to_alpha(png_ptr);

	if (colortype == PNG_COLOR_TYPE_GRAY || colortype == PNG_COLOR_TYPE_GRAY_ALPHA)
		qpng_set_gray_to_rgb(png_ptr);

	if (colortype != PNG_COLOR_TYPE_RGBA)				
		qpng_set_filler(png_ptr, 255, PNG_FILLER_AFTER);

	if (bitdepth < 8)
		qpng_set_expand (png_ptr);
	else if (bitdepth == 16)
		qpng_set_strip_16(png_ptr);


	qpng_read_update_info(png_ptr, pnginfo);
	rowbytes = qpng_get_rowbytes(png_ptr, pnginfo);
	bytesperpixel = qpng_get_channels(png_ptr, pnginfo);
	bitdepth = qpng_get_bit_depth(png_ptr, pnginfo);

	if (bitdepth != 8 || bytesperpixel != 4) {
		Com_DPrintf ("Unsupported PNG image %s: Bad color depth and/or bpp\n", COM_SkipPath(filename));
		qpng_destroy_read_struct(&png_ptr, &pnginfo, NULL);
#ifndef WITH_FTE_VFS
		fclose(fin);
#else
		VFS_CLOSE(fin);
#endif
		return NULL;
	}

	data = (byte *) Q_malloc(height * rowbytes );
	rowpointers = (byte **) Q_malloc(height * sizeof(*rowpointers));

	for (y = 0; y < height; y++)
		rowpointers[y] = data + y * rowbytes;

	qpng_read_image(png_ptr, rowpointers);
	qpng_read_end(png_ptr, NULL);

	qpng_destroy_read_struct(&png_ptr, &pnginfo, NULL);
	Q_free(rowpointers);
#ifndef WITH_FTE_VFS
	fclose(fin);
#else
	VFS_CLOSE(fin);
#endif
	return data;
}

int Image_WritePNG (char *filename, int compression, byte *pixels, int width, int height) {
	char name[MAX_PATH];
	int i, bpp = 3, pngformat, width_sign;
#ifndef WITH_FTE_VFS
	FILE *fp;
#else
	vfsfile_t *fp;
#endif
	png_structp png_ptr;
	png_infop info_ptr;
	png_byte **rowpointers;
	snprintf (name, sizeof(name), "%s", filename);

	if (!png_handle)
		return false;

	width_sign = (width < 0) ? -1 : 1;
	width = abs(width);

#ifndef WITH_FTE_VFS
	if (!(fp = fopen (name, "wb"))) {
		COM_CreatePath (name);
		if (!(fp = fopen (name, "wb")))
			return false;
	}
#else
	if (!(fp = FS_OpenVFS(name, "wb", FS_NONE_OS))) {
		COM_CreatePath (name);
		if (!(fp = FS_OpenVFS(name, "wb", FS_NONE_OS)))
			return false;
	}
#endif

	if (!(png_ptr = qpng_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))) {
#ifndef WITH_FTE_VFS
		fclose(fp);
#else
		VFS_CLOSE(fp);
#endif
		return false;
	}

	if (!(info_ptr = qpng_create_info_struct(png_ptr))) {
		qpng_destroy_write_struct(&png_ptr, (png_infopp) NULL);
#ifndef WITH_FTE_VFS
		fclose(fp);
#else
		VFS_CLOSE(fp);
#endif
		return false;
	}

	if (setjmp(png_ptr->jmpbuf)) {
		qpng_destroy_write_struct(&png_ptr, &info_ptr);
#ifndef WITH_FTE_VFS
		fclose(fp);
#else
		VFS_CLOSE(fp);
#endif
		return false;
	}

    qpng_set_write_fn(png_ptr, fp, PNG_IO_user_write_data, PNG_IO_user_flush_data);
	qpng_set_compression_level(png_ptr, bound(Z_NO_COMPRESSION, compression, Z_BEST_COMPRESSION));

	pngformat = (bpp == 4) ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB;
	qpng_set_IHDR(png_ptr, info_ptr, width, height, 8, pngformat,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	qpng_write_info(png_ptr, info_ptr);

	rowpointers = (png_byte **) Q_malloc (height * sizeof(*rowpointers));
	for (i = 0; i < height; i++)
		rowpointers[i] = pixels + i * width_sign * width * bpp;
	qpng_write_image(png_ptr, rowpointers);
	qpng_write_end(png_ptr, info_ptr);
	Q_free(rowpointers);
	qpng_destroy_write_struct(&png_ptr, &info_ptr);

#ifndef WITH_FTE_VFS
		fclose(fp);
#else
		VFS_CLOSE(fp);
#endif
	return true;
}

int Image_WritePNGPLTE (char *filename, int compression,
#ifdef GLQUAKE
	byte *pixels, int width, int height, byte *palette)
#else
	byte *pixels, int width, int height, int rowbytes, byte *palette)
#endif
{
#ifdef GLQUAKE
	int rowbytes = width;
#endif
	int i;
	char name[MAX_PATH];
#ifndef WITH_FTE_VFS
	FILE *fp;
#else
	vfsfile_t *fp;
#endif
	png_structp png_ptr;
	png_infop info_ptr;
	png_byte **rowpointers;

	if (!png_handle)
		return false;

	snprintf (name, sizeof(name), "%s", filename);
	
#ifndef WITH_FTE_VFS
	if (!(fp = fopen (name, "wb"))) {
		COM_CreatePath (name);
		if (!(fp = fopen (name, "wb")))
			return false;
	}
#else
	if (!(fp = FS_OpenVFS(name, "wb", FS_NONE_OS))) {
		COM_CreatePath (name);
		if (!(fp = FS_OpenVFS(name, "wb", FS_NONE_OS)))
			return false;
	}

#endif

	if (!(png_ptr = qpng_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))) {
#ifndef WITH_FTE_VFS
		fclose(fp);
#else
		VFS_CLOSE(fp);
#endif
		return false;
	}

	if (!(info_ptr = qpng_create_info_struct(png_ptr))) {
		qpng_destroy_write_struct(&png_ptr, (png_infopp) NULL);
#ifndef WITH_FTE_VFS
		fclose(fp);
#else
		VFS_CLOSE(fp);
#endif
		return false;
	}

	if (setjmp(png_ptr->jmpbuf)) {
		qpng_destroy_write_struct(&png_ptr, &info_ptr);
#ifndef WITH_FTE_VFS
		fclose(fp);
#else
		VFS_CLOSE(fp);
#endif
		return false;
	}

    qpng_set_write_fn(png_ptr, fp, PNG_IO_user_write_data, PNG_IO_user_flush_data);
	qpng_set_compression_level(png_ptr, bound(Z_NO_COMPRESSION, compression, Z_BEST_COMPRESSION));

	qpng_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_PALETTE,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	qpng_set_PLTE(png_ptr, info_ptr, (png_color *) palette, 256);

	qpng_write_info(png_ptr, info_ptr);

	rowpointers = (png_byte **) Q_malloc (height * sizeof(*rowpointers));
	for (i = 0; i < height; i++)
		rowpointers[i] = pixels + i * rowbytes;
	qpng_write_image(png_ptr, rowpointers);
	qpng_write_end(png_ptr, info_ptr);
	Q_free(rowpointers);
	qpng_destroy_write_struct(&png_ptr, &info_ptr);

#ifndef WITH_FTE_VFS
	fclose(fp);
#else
	VFS_CLOSE(fp);
#endif
	return true;
}

#else

#ifndef WITH_FTE_VFS
static void PNG_IO_user_read_data(png_structp png_ptr, png_bytep data, png_size_t length) 
{
	FILE *f = (FILE *) png_get_io_ptr(png_ptr);
	fread(data, 1, length, f);
}

static void PNG_IO_user_write_data(png_structp png_ptr, png_bytep data, png_size_t length) 
{
	FILE *f = (FILE *) png_get_io_ptr(png_ptr);
	fwrite(data, 1, length, f);
}

static void PNG_IO_user_flush_data(png_structp png_ptr) 
{
	FILE *f = (FILE *) png_get_io_ptr(png_ptr);
	fflush(f);
}

#else
static void PNG_IO_user_read_data(png_structp png_ptr, png_bytep data, png_size_t length) {
	vfsfile_t *v = (vfsfile_t *) png_get_io_ptr(png_ptr);
	vfserrno_t err;
	VFS_READ(v, data, length, &err);
}

static void PNG_IO_user_write_data(png_structp png_ptr, png_bytep data, png_size_t length) {
	vfsfile_t *v = (vfsfile_t *) png_get_io_ptr(png_ptr);
	VFS_WRITE(v, data, length);
}

static void PNG_IO_user_flush_data(png_structp png_ptr) {
	vfsfile_t *v = (vfsfile_t *) png_get_io_ptr(png_ptr);
	VFS_FLUSH(v);
}
#endif // WITH_FTE_VFS

#define PNG_HEADER_LENGTH	8
#define PNG_LOAD_TEXT		1
#define PNG_LOAD_DATA		2

#ifndef WITH_FTE_VFS
static qbool PNG_HasHeader (FILE *fin)
{
	byte header[PNG_HEADER_LENGTH];

	fread(header, 1, PNG_HEADER_LENGTH, fin);
	if (png_sig_cmp(header, 0, PNG_HEADER_LENGTH)) 
	{
		fclose(fin);
		fin = NULL;
		return false;
	}

	return true;
}

#else
static qbool PNG_HasHeader (vfsfile_t *fin)
{
	byte header[PNG_HEADER_LENGTH];
	vfserrno_t err;

	VFS_READ(fin, header, PNG_HEADER_LENGTH, &err);
	if (png_sig_cmp(header, 0, PNG_HEADER_LENGTH)) 
	{
		VFS_CLOSE(fin);
		fin = NULL;
		return false;
	}

	return true;
}
#endif // WITH_FTE_VFS


#ifndef WITH_FTE_VFS
png_data *Image_LoadPNG_All (FILE *fin, const char *filename, int matchwidth, int matchheight, int loadflag, int *real_width, int *real_height)
#else
png_data *Image_LoadPNG_All (vfsfile_t *fin, const char *filename, int matchwidth, int matchheight, int loadflag, int *real_width, int *real_height)
#endif // WITH_FTE_VFS
{
	byte **rowpointers = NULL;
	byte *data = NULL;
	png_structp png_ptr = NULL;
	png_infop pnginfo = NULL;			
	png_textp textchunks = NULL;		// Actual text chunks that will be returned.
	png_textp png_text_ptr = NULL;		// Text chunks are read into this (will be scrapped by libpng).
	png_data *png_return_val = NULL;	// Return struct containing data + text chunks.
	int n_textcount = 0;				// Number of text chunks.
	int y, width, height, bitdepth, colortype, interlace, compression, filter, bytesperpixel;
	unsigned long rowbytes;

	// Check if we were given a non-null file pointer
	// if so then use it, otherwise try to open the specified filename.
#ifndef WITH_FTE_VFS
	if (!fin && FS_FOpenFile (filename, &fin) == -1)
#else
	if (!fin && !(fin = FS_OpenVFS(filename, "rb", FS_ANY)))
#endif
	{
		return NULL;
	}

	// Check if the loaded file contains a PNG header.
	if (!PNG_HasHeader (fin))
	{
		Com_DPrintf ("Invalid PNG image %s\n", COM_SkipPath(filename));
		return NULL;
	}

	// Try creating a PNG structure for reading the file.
	if (!(png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))) 
	{
#ifndef WITH_FTE_VFS
		fclose(fin);
#else
		VFS_CLOSE(fin);
#endif
		fin = NULL;
		return NULL;
	}

	// Create a structure for reading the characteristics of the image.
	if (!(pnginfo = png_create_info_struct(png_ptr))) 
	{
		png_destroy_read_struct(&png_ptr, &pnginfo, NULL);
#ifndef WITH_FTE_VFS
		fclose(fin);
#else
		VFS_CLOSE(fin);
#endif
		fin = NULL;
		return NULL;
	}

	// Set the return adress that PNGLib should return to if
	// an error occurs during reading.
	if (setjmp(png_ptr->jmpbuf)) 
	{
		png_destroy_read_struct(&png_ptr, &pnginfo, NULL);
#ifndef WITH_FTE_VFS
		fclose(fin);
#else
		VFS_CLOSE(fin);
#endif
		fin = NULL;
		return NULL;
	}

	// Set the read function that should be used.
    png_set_read_fn(png_ptr, fin, PNG_IO_user_read_data);

	// Tell PNG-lib that we have already handled the first <num_bytes> magic bytes.
	// Handling more than 8 bytes from the beginning of the file is an error.
	png_set_sig_bytes(png_ptr, PNG_HEADER_LENGTH);

	// Read all the file information until the actual image data.
	png_read_info(png_ptr, pnginfo);

	// Get the IHDR info and set transformations based on it's contents
	// suc as pallete type / bit depth.
	//
	// The IHDR chunk contains the width, height, bpp, colortype, 
	// filter method and compression type used.
	// This must be the first chunk in the file.
	{
		png_get_IHDR(png_ptr, pnginfo, (png_uint_32 *) &width, (png_uint_32 *) &height, &bitdepth,
			&colortype, &interlace, &compression, &filter);

		// Return the width and height.
		if (real_width)
			(*real_width) = width;

		if (real_height)
			(*real_height) = height;

		// Too big?
		if (width > IMAGE_MAX_DIMENSIONS || height > IMAGE_MAX_DIMENSIONS) 
		{
			Com_DPrintf ("PNG image %s exceeds maximum supported dimensions\n", COM_SkipPath(filename));
			png_destroy_read_struct(&png_ptr, &pnginfo, NULL);
#ifndef WITH_FTE_VFS
			fclose(fin);
#else
			VFS_CLOSE(fin);
#endif
			fin = NULL;
			return NULL;
		}

		// If the caller wanted the image to be a specific size
		// make sure it is that size, otherwise cleanup and return.
		if ((matchwidth && width != matchwidth) || (matchheight && height != matchheight)) 
		{
			png_destroy_read_struct(&png_ptr, &pnginfo, NULL);
#ifndef WITH_FTE_VFS
			fclose(fin);
#else
			VFS_CLOSE(fin);
#endif
			fin = NULL;
			return NULL;
		}

		if (colortype == PNG_COLOR_TYPE_PALETTE) 
		{
			png_set_palette_to_rgb(png_ptr);
			png_set_filler(png_ptr, 255, PNG_FILLER_AFTER);		
		}

		if (colortype == PNG_COLOR_TYPE_GRAY && bitdepth < 8) 
		{
			png_set_gray_1_2_4_to_8(png_ptr);
		}
		
		if (png_get_valid(png_ptr, pnginfo, PNG_INFO_tRNS))	
		{
			png_set_tRNS_to_alpha(png_ptr);
		}

		if (colortype == PNG_COLOR_TYPE_GRAY || colortype == PNG_COLOR_TYPE_GRAY_ALPHA)
		{
			png_set_gray_to_rgb(png_ptr);
		}

		if (colortype != PNG_COLOR_TYPE_RGBA)
		{
			png_set_filler(png_ptr, 255, PNG_FILLER_AFTER);
		}

		if (bitdepth < 8)
		{
			png_set_expand (png_ptr);
		}
		else if (bitdepth == 16)
		{
			png_set_strip_16(png_ptr);
		}

		// 
		// Dither the image using the 8-bit quake pallete for software.
		// 
		#ifndef GLQUAKE
		if (image_png_dither_onload.integer)
		{
			png_uint_16p histogram;
			png_colorp quake_pal = (png_colorp)host_basepal;
			png_color_16 bg;
			
			// Set the background color to the transparent quake color.
			bg.red		= host_basepal[(255 * 3)];
			bg.green	= host_basepal[(255 * 3) + 1];
			bg.blue		= host_basepal[(255 * 3) + 2];

			png_set_background(png_ptr, &bg, PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);

			// Check if the image contains a histogram, use it in that case.
			if (!png_get_hIST(png_ptr, pnginfo, &histogram))
				histogram = NULL;

			png_set_dither(png_ptr, quake_pal, 256, 256, histogram, true);
		}
		#endif // !GLQUAKE

		// Update the pnginfo structure with our transformation changes.
		png_read_update_info(png_ptr, pnginfo);
	}

	//
	// Read the text chunks before the image data.
	//
	if (loadflag & PNG_LOAD_TEXT)
	{
		// Get the text chunks found before the image data.
		n_textcount = 0;
		png_get_text(png_ptr, pnginfo, &png_text_ptr, &n_textcount);
	}

	//
	// Read the image data.
	//
	{
		// Get the number of bytes a row will use / number of channels / bitdepth.
		rowbytes = png_get_rowbytes(png_ptr, pnginfo);
		bytesperpixel = png_get_channels(png_ptr, pnginfo);
		bitdepth = png_get_bit_depth(png_ptr, pnginfo);

		// We don't support some formats.
		if (bitdepth != 8 || (bytesperpixel != 4 && bytesperpixel != 1)) 
		{
			Com_DPrintf ("Unsupported PNG image %s: Bad color depth and/or bpp\n", COM_SkipPath(filename));
			png_destroy_read_struct(&png_ptr, &pnginfo, NULL);
#ifndef WITH_FTE_VFS
			fclose(fin);
#else
			VFS_CLOSE(fin);
#endif
			fin = NULL;
			return NULL;
		}

		// Allocate a byte array for the actual image data.
		data = (byte *) Q_malloc(height * rowbytes );

		// Even though we just allocated the memory for all the image data
		// PNG lib wants this in the form of rowpointers.
		rowpointers = (byte **) Q_malloc(height * sizeof(*rowpointers));
		for (y = 0; y < height; y++)
		{
			rowpointers[y] = data + y * rowbytes;
		}

		// Read the actual image data.
		png_read_image(png_ptr, rowpointers);
		png_read_end(png_ptr, pnginfo);
	}
	
	//
	// Read the text chunks after the image data.
	//
	if (loadflag & PNG_LOAD_TEXT)
	{
		// Read text chunks after the image data also.
		png_get_text(png_ptr, pnginfo, &png_text_ptr, &n_textcount);
	
		// Return the text chunks if we found any.
		if(n_textcount > 0)
		{
			int i = 0;
			int len = 0;
			textchunks = (png_textp)Q_calloc(n_textcount, sizeof(png_text));

			for(i = 0; i < n_textcount; i++)
			{
				len = strlen(png_text_ptr[i].key);

				textchunks[i].key = (char *)Q_calloc(len + 1, sizeof(char));
				textchunks[i].text = (char *)Q_calloc(png_text_ptr[i].text_length + 1, sizeof(char));

				strlcpy (textchunks[i].key, png_text_ptr[i].key, len + 1);
				strlcpy (textchunks[i].text, png_text_ptr[i].text, png_text_ptr[i].text_length + 1);
				
				textchunks[i].text_length = png_text_ptr[i].text_length;
				textchunks[i].compression = png_text_ptr[i].compression;
			}
		}
		else
		{
			textchunks = NULL;
		}
	}

	// Clean up.
	png_destroy_read_struct (&png_ptr, &pnginfo, NULL);
	Q_free (rowpointers);
#ifndef WITH_FTE_VFS
	fclose(fin);
#else
	VFS_CLOSE(fin);
#endif
	fin = NULL;

	// If we don't care about the data.
	if (!(loadflag & PNG_LOAD_DATA))
	{
		Q_free (data);
	}

	// Gather up the return data.
	png_return_val = (png_data *)Q_malloc(sizeof(png_data));	

	#ifndef GLQUAKE
	if (!image_png_dither_onload.integer)
	{
		// SOFTWARE - Convert to the 8-bit quake palette by taking the nearest color.

		byte *Draw_Convert24bitTo8bit(byte *src, int bytes_per_pixel, int width, int height, qbool dither);
		byte *data_8bit = Draw_Convert24bitTo8bit(data, 4, width, height, false);
		Q_free(data);

		png_return_val->data = data_8bit;
	}
	else
	#endif // !GLQUAKE
	{
		// If in GLQuake or dithering is turned on when in software
		// pnglib has done the palette conversion for us already.
		png_return_val->data = data;
	}
	png_return_val->textchunks = textchunks;
	png_return_val->text_count = n_textcount;

	return png_return_val;
}

png_textp Image_LoadPNG_Comments (char *filename, int *text_count)
{
	png_textp textchunks = NULL;
	png_data *pdata = NULL;

	pdata = Image_LoadPNG_All (NULL, filename, 0, 0, PNG_LOAD_TEXT, NULL, NULL);

	if (pdata)
	{		
		// Return text chunks + count.
		(*text_count) = pdata->text_count;
		textchunks = pdata->textchunks;

		Q_free(pdata);
	}

	return textchunks;
}

#ifndef WITH_FTE_VFS
byte *Image_LoadPNG (FILE *fin, const char *filename, int matchwidth, int matchheight, int *real_width, int *real_height) 
#else
byte *Image_LoadPNG (vfsfile_t *fin, const char *filename, int matchwidth, int matchheight, int *real_width, int *real_height) 
#endif // WITH_FTE_VFS
{
	byte *data = NULL;
	png_data *pdata;
	
	// Load the actual image.
	pdata = Image_LoadPNG_All (fin, filename, matchwidth, matchheight, PNG_LOAD_DATA, real_width, real_height);
	
	if (pdata)
	{
		// Save the data and free the rest.
		data = pdata->data;
		Q_free(pdata);
	}
	
	return data;
}

int Image_WritePNG (char *filename, int compression, byte *pixels, int width, int height) 
{
	char name[MAX_PATH];
	int i, bpp = 3, pngformat, width_sign;
#ifndef WITH_FTE_VFS
	FILE *fp;
#else
	vfsfile_t *fp;
#endif
	png_structp png_ptr;
	png_infop info_ptr;
	png_byte **rowpointers;
	snprintf (name, sizeof(name), "%s", filename);

	width_sign = (width < 0) ? -1 : 1;
	width = abs(width);

#ifndef WITH_FTE_VFS
	if (!(fp = fopen (name, "wb"))) {
		FS_CreatePath (name);
		if (!(fp = fopen (name, "wb")))
			return false;
	}
#else
	if (!(fp = FS_OpenVFS(name, "wb", FS_NONE_OS))) {
		FS_CreatePath (name);
		if (!(fp = FS_OpenVFS(name, "wb", FS_NONE_OS)))
			return false;
	}
#endif

	if (!(png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))) {
#ifndef WITH_FTE_VFS
		fclose(fp);
#else
		VFS_CLOSE(fp);
#endif
		return false;
	}

	if (!(info_ptr = png_create_info_struct(png_ptr))) {
		png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
#ifndef WITH_FTE_VFS
		fclose(fp);
#else
		VFS_CLOSE(fp);
#endif
		return false;
	}

	if (setjmp(png_ptr->jmpbuf)) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
#ifndef WITH_FTE_VFS
		fclose(fp);
#else
		VFS_CLOSE(fp);
#endif
		return false;
	}

    png_set_write_fn(png_ptr, fp, PNG_IO_user_write_data, PNG_IO_user_flush_data);
	png_set_compression_level(png_ptr, bound(Z_NO_COMPRESSION, compression, Z_BEST_COMPRESSION));

	pngformat = (bpp == 4) ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB;
	png_set_IHDR(png_ptr, info_ptr, width, height, 8, pngformat,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	png_write_info(png_ptr, info_ptr);

	rowpointers = (png_byte **) Q_malloc (height * sizeof(*rowpointers));
	for (i = 0; i < height; i++)
		rowpointers[i] = pixels + i * width_sign * width * bpp;
	png_write_image(png_ptr, rowpointers);
	png_write_end(png_ptr, info_ptr);
	Q_free(rowpointers);
	png_destroy_write_struct(&png_ptr, &info_ptr);
#ifndef WITH_FTE_VFS
		fclose(fp);
#else
		VFS_CLOSE(fp);
#endif
	return true;
}

int Image_WritePNGPLTE (char *filename, int compression,
#ifdef GLQUAKE
	byte *pixels, int width, int height, byte *palette)
#else // SOFTWARE
	byte *pixels, int width, int height, int rowbytes, byte *palette)
#endif // GLQUAKE
{
	#ifdef GLQUAKE
	int rowbytes = width;
	#endif // GLQUAKE

	int i;
	char name[MAX_PATH];
#ifndef WITH_FTE_VFS
	FILE *fp;
#else
	vfsfile_t *fp;
#endif
	png_structp png_ptr;
	png_infop info_ptr;
	png_byte **rowpointers;

	snprintf (name, sizeof(name), "%s", filename);
	
#ifndef WITH_FTE_VFS
	if (!(fp = fopen (name, "wb"))) {
		FS_CreatePath (name);
		if (!(fp = fopen (name, "wb")))
			return false;
	}
#else
	if (!(fp = FS_OpenVFS(name, "wb", FS_NONE_OS))) {
		FS_CreatePath (name);
		if (!(fp = FS_OpenVFS(name, "wb", FS_NONE_OS)))
			return false;
	}
#endif

	if (!(png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))) {
#ifndef WITH_FTE_VFS
		fclose(fp);
#else
		VFS_CLOSE(fp);
#endif
		return false;
	}

	if (!(info_ptr = png_create_info_struct(png_ptr))) {
		png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
#ifndef WITH_FTE_VFS
		fclose(fp);
#else
		VFS_CLOSE(fp);
#endif
		return false;
	}

	if (setjmp(png_ptr->jmpbuf)) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
#ifndef WITH_FTE_VFS
		fclose(fp);
#else
		VFS_CLOSE(fp);
#endif
		return false;
	}

    png_set_write_fn(png_ptr, fp, PNG_IO_user_write_data, PNG_IO_user_flush_data);
	png_set_compression_level(png_ptr, bound(Z_NO_COMPRESSION, compression, Z_BEST_COMPRESSION));

	png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_PALETTE,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_set_PLTE(png_ptr, info_ptr, (png_color *) palette, 256);

	png_write_info(png_ptr, info_ptr);

	rowpointers = (png_byte **) Q_malloc (height * sizeof(*rowpointers));
	for (i = 0; i < height; i++)
		rowpointers[i] = pixels + i * rowbytes;
	png_write_image(png_ptr, rowpointers);
	png_write_end(png_ptr, info_ptr);
	Q_free(rowpointers);
	png_destroy_write_struct(&png_ptr, &info_ptr);

#ifndef WITH_FTE_VFS
	fclose(fp);
#else
	VFS_CLOSE(fp);
#endif
	return true;
}
#endif
#endif

/************************************ TGA ************************************/

// Definitions for image types
#define TGA_MAPPED		1	// Uncompressed, color-mapped images
#define TGA_MAPPED_RLE	9	// Runlength encoded color-mapped images
#define TGA_RGB			2	// Uncompressed, RGB images
#define TGA_RGB_RLE		10	// Runlength encoded RGB images
#define TGA_MONO		3	// Uncompressed, black and white images
#define TGA_MONO_RLE	11	// Compressed, black and white images

// Custom definitions to simplify code
#define MYTGA_MAPPED	80
#define MYTGA_RGB15		81
#define MYTGA_RGB24		82
#define MYTGA_RGB32		83
#define MYTGA_MONO8		84
#define MYTGA_MONO16	85

typedef struct TGAHeader_s 
{
	byte			idLength, colormapType, imageType;
	unsigned short	colormapIndex, colormapLength;
	byte			colormapSize;
	unsigned short	xOrigin, yOrigin, width, height;
	byte			pixelSize, attributes;
} TGAHeader_t;


static void TGA_upsample15(byte *dest, byte *src, qbool alpha) 
{
	dest[2] = (byte) ((src[0] & 0x1F) << 3);
	dest[1] = (byte) ((((src[1] & 0x03) << 3) + ((src[0] & 0xE0) >> 5)) << 3);
	dest[0] = (byte) (((src[1] & 0x7C) >> 2) << 3);
	dest[3] = (alpha && !(src[1] & 0x80)) ? 0 : 255;
}

static void TGA_upsample24(byte *dest, byte *src) 
{
	dest[2] = src[0];
	dest[1] = src[1];
	dest[0] = src[2];
	dest[3] = 255;
}

static void TGA_upsample32(byte *dest, byte *src) 
{
	dest[2] = src[0];
	dest[1] = src[1];
	dest[0] = src[2];
	dest[3] = src[3];
}


#define TGA_ERROR(msg)	{if (msg) {Com_DPrintf((msg), COM_SkipPath(filename));} Q_free(fileBuffer); return NULL;}

#ifndef WITH_FTE_VFS
byte *Image_LoadTGA(FILE *fin, int filesize, const char *filename, int matchwidth, int matchheight, int *real_width, int *real_height) 
#else
byte *Image_LoadTGA(vfsfile_t *fin, const char *filename, int matchwidth, int matchheight, int *real_width, int *real_height) 
#endif // WITH_FTE_VFS
{
	TGAHeader_t header;
	int i, x, y, bpp, alphabits, compressed, mytype, row_inc, runlen, readpixelcount;
	int image_width = -1, image_height = -1;
	byte *fileBuffer, *in, *out, *data, *enddata, rgba[4], palette[256 * 4];
#ifdef WITH_FTE_VFS
	int filesize;
#endif

#ifndef WITH_FTE_VFS
	if (!fin && (filesize = FS_FOpenFile (filename, &fin)) == -1)
		return NULL;
	fileBuffer = (byte *) Q_malloc(filesize);
	fread(fileBuffer, 1, filesize, fin);
	fclose(fin);

#else
	if (!fin && !(fin = FS_OpenVFS(filename, "rb", FS_ANY)))
		return NULL;
	filesize = VFS_GETLEN(fin);
	fileBuffer = (byte *) Q_malloc(filesize);

	VFS_READ(fin, fileBuffer, filesize, NULL);
	VFS_CLOSE(fin);
#endif // WITH_FTE_VFS

	if (filesize < 19)
		TGA_ERROR(NULL);

	header.idLength = fileBuffer[0];
	header.colormapType = fileBuffer[1];
	header.imageType = fileBuffer[2];

	header.colormapIndex = BuffLittleShort(fileBuffer + 3);
	header.colormapLength = BuffLittleShort(fileBuffer + 5);
	header.colormapSize = fileBuffer[7];
	header.xOrigin = BuffLittleShort(fileBuffer + 8);
	header.yOrigin = BuffLittleShort(fileBuffer + 10);
	header.width = image_width = BuffLittleShort(fileBuffer + 12);
	header.height = image_height = BuffLittleShort(fileBuffer + 14);
	header.pixelSize = fileBuffer[16];
	header.attributes = fileBuffer[17];

	// Return the width and height.
	if (real_width)
		(*real_width) = image_width;

	if (real_height)
		(*real_height) = image_height;

	if (image_width > IMAGE_MAX_DIMENSIONS || image_height > IMAGE_MAX_DIMENSIONS || image_width <= 0 || image_height <= 0)
		TGA_ERROR(NULL);
	if ((matchwidth && image_width != matchwidth) || (matchheight && image_height != matchheight))
		TGA_ERROR(NULL);

	bpp = (header.pixelSize + 7) >> 3;
	alphabits = (header.attributes & 0x0F);
	compressed = (header.imageType & 0x08);

	in = fileBuffer + 18 + header.idLength;
	enddata = fileBuffer + filesize;

	// error check the image type's pixel size
	if (header.imageType == TGA_RGB || header.imageType == TGA_RGB_RLE) {
		if (!(header.pixelSize == 15 || header.pixelSize == 16 || header.pixelSize == 24 || header.pixelSize == 32))
			TGA_ERROR("Unsupported TGA image %s: Bad pixel size for RGB image\n");
		mytype = (header.pixelSize == 24) ? MYTGA_RGB24 : (header.pixelSize == 32) ? MYTGA_RGB32 : MYTGA_RGB15;
	} else if (header.imageType == TGA_MAPPED || header.imageType == TGA_MAPPED_RLE) {
		if (header.pixelSize != 8)
			TGA_ERROR("Unsupported TGA image %s: Bad pixel size for color-mapped image.\n");
		if (!(header.colormapSize == 15 || header.colormapSize == 16 || header.colormapSize == 24 || header.colormapSize == 32))
			TGA_ERROR("Unsupported TGA image %s: Bad colormap size.\n");
		if (header.colormapType != 1 || header.colormapLength * 4 > sizeof(palette))
			TGA_ERROR("Unsupported TGA image %s: Bad colormap type and/or length for color-mapped image.\n");

		// read in the palette
		if (header.colormapSize == 15 || header.colormapSize == 16) {
			for (i = 0, out = palette; i < header.colormapLength; i++, in += 2, out += 4)
				TGA_upsample15(out, in, alphabits == 1);
		} else if (header.colormapSize == 24) {
			for (i = 0, out = palette; i < header.colormapLength; i++, in += 3, out += 4)
				TGA_upsample24(out, in);
		} else if (header.colormapSize == 32) {
			for (i = 0, out = palette; i < header.colormapLength; i++, in += 4, out += 4)
				TGA_upsample32(out, in);
		}
		mytype = MYTGA_MAPPED;
	} else if (header.imageType == TGA_MONO || header.imageType == TGA_MONO_RLE) {
		if (!(header.pixelSize == 8 || (header.pixelSize == 16 && alphabits == 8)))
			TGA_ERROR("Unsupported TGA image %s: Bad pixel size for grayscale image.\n");
		mytype = (header.pixelSize == 8) ? MYTGA_MONO8 : MYTGA_MONO16;
	} else {
		TGA_ERROR("Unsupported TGA image %s: Bad image type.\n");
	}

	if (header.attributes & 0x10)
		TGA_ERROR("Unsupported TGA image %s: Pixel data spans right to left.\n");

	data = (byte *) Q_malloc(image_width * image_height * 4);

	// if bit 5 of attributes isn't set, the image has been stored from bottom to top
	if ((header.attributes & 0x20)) {
		out = data;
		row_inc = 0;
	} else {
		out = data + (image_height - 1) * image_width * 4;
		row_inc = -image_width * 4 * 2;
	}

	x = y = 0;
	rgba[0] = rgba[1] = rgba[2] = rgba[3] = 255;

	while (y < image_height) {
		// decoder is mostly the same whether it's compressed or not
		readpixelcount = runlen = 0x7FFFFFFF;
		if (compressed && in < enddata) {
			runlen = *in++;
			// high bit indicates this is an RLE compressed run
			if (runlen & 0x80)
				readpixelcount = 1;
			runlen = 1 + (runlen & 0x7F);
		}

		while (runlen-- && y < image_height) {
			if (readpixelcount > 0) {
				readpixelcount--;
				rgba[0] = rgba[1] = rgba[2] = rgba[3] = 255;

				if (in + bpp <= enddata) {
					switch(mytype) {
					case MYTGA_MAPPED:
						for (i = 0; i < 4; i++)
							rgba[i] = palette[in[0] * 4 + i];
						break;
					case MYTGA_RGB15:
						TGA_upsample15(rgba, in, alphabits == 1);
						break;
					case MYTGA_RGB24:
						TGA_upsample24(rgba, in);
						break;
					case MYTGA_RGB32:
						TGA_upsample32(rgba, in);
						break;
					case MYTGA_MONO8:
						rgba[0] = rgba[1] = rgba[2] = in[0];
						break;
					case MYTGA_MONO16:
						rgba[0] = rgba[1] = rgba[2] = in[0];
						rgba[3] = in[1];
						break;
					}
					in += bpp;
				}
			}
			for (i = 0; i < 4; i++)
				*out++ = rgba[i];
			if (++x == image_width) {
				// end of line, advance to next
				x = 0;
				y++;
				out += row_inc;
			}
		}
	}

	Q_free(fileBuffer);
	return data;
}

int Image_WriteTGA (char *filename, byte *pixels, int width, int height) 
{
	byte *buffer;
	int size;
	qbool retval = true;

	size = width * height * 3;
	buffer = (byte *) Q_malloc (size + 18);
	memset (buffer, 0, 18);
	buffer[2] = 2;          // uncompressed type
	buffer[12] = width & 255;
	buffer[13] = width >> 8;
	buffer[14] = height & 255;
	buffer[15] = height >> 8;
	buffer[16] = 24;

	memcpy (buffer + 18, pixels, size);

	if (!(FS_WriteFile_2 (filename, buffer, size + 18)))
		retval = false;
	Q_free (buffer);
	return retval;
}

/*********************************** JPEG ************************************/

#ifdef WITH_JPEG
#ifndef WITH_JPEG_STATIC

static QLIB_HANDLETYPE_T jpeg_handle = NULL;

#define qjpeg_create_compress(cinfo) \
    qjpeg_CreateCompress((cinfo), JPEG_LIB_VERSION, (size_t) sizeof(struct jpeg_compress_struct))

static struct jpeg_error_mgr *(*qjpeg_std_error)(struct jpeg_error_mgr *);
static void (*qjpeg_destroy_compress)(j_compress_ptr);
static void (*qjpeg_set_defaults)(j_compress_ptr);
static void (*qjpeg_set_quality)(j_compress_ptr, int, boolean);
static void (*qjpeg_start_compress)(j_compress_ptr, boolean);
static JDIMENSION (*qjpeg_write_scanlines)(j_compress_ptr, JSAMPARRAY, JDIMENSION);
static void (*qjpeg_finish_compress)(j_compress_ptr);
static void (*qjpeg_CreateCompress)(j_compress_ptr, int, size_t);

#define NUM_JPEGPROCS	(sizeof(jpegprocs)/sizeof(jpegprocs[0]))

qlib_dllfunction_t jpegprocs[] = {
	{"jpeg_std_error", (void **) &qjpeg_std_error},
	{"jpeg_destroy_compress", (void **) &qjpeg_destroy_compress},
	{"jpeg_set_defaults", (void **) &qjpeg_set_defaults},
	{"jpeg_set_quality", (void **) &qjpeg_set_quality},
	{"jpeg_start_compress", (void **) &qjpeg_start_compress},
	{"jpeg_write_scanlines", (void **) &qjpeg_write_scanlines},
	{"jpeg_finish_compress", (void **) &qjpeg_finish_compress},
	{"jpeg_CreateCompress", (void **) &qjpeg_CreateCompress},
};

static void JPEG_FreeLibrary(void) {
	if (jpeg_handle)
		QLIB_FREELIBRARY(jpeg_handle);
}

static qbool JPEG_LoadLibrary(void) {
	if (COM_CheckParm("-nolibjpeg"))
		return false;

#ifdef _WIN32
	if (!(jpeg_handle = LoadLibrary("libjpeg.dll"))) {
#else
#ifdef __APPLE__
	if (!(jpeg_handle = dlopen("libjpeg.62.dylib", RTLD_NOW))) {
#else
	if (!(jpeg_handle = dlopen("libjpeg.so.62", RTLD_NOW)) && !(jpeg_handle = dlopen("libjpeg.so", RTLD_NOW))) {
#endif
#endif
		QLib_MissingModuleError(QLIB_ERROR_MODULE_NOT_FOUND, "libjpeg", "-nolibjpeg", "jpeg image features");
		jpeg_handle = NULL;
		return false;
	}

	if (!QLib_ProcessProcdef(jpeg_handle, jpegprocs, NUM_JPEGPROCS)) {
		JPEG_FreeLibrary();
		QLib_MissingModuleError(QLIB_ERROR_MODULE_MISSING_PROC, "libjpeg", "-nolibjpeg", "jpeg image features");
		jpeg_handle = NULL;
		return false;
	}

	return true;
}


typedef struct {
  struct jpeg_destination_mgr pub; 
#ifndef WITH_FTE_VFS
  FILE *outfile;
#else
  vfsfile_t *outfile;
#endif
  JOCTET *buffer;
} my_destination_mgr;

typedef my_destination_mgr *my_dest_ptr;

#define JPEG_OUTPUT_BUF_SIZE  4096

static qbool jpeg_in_error = false;

static void JPEG_IO_init_destination(j_compress_ptr cinfo) {
	my_dest_ptr dest = (my_dest_ptr) cinfo->dest;
	dest->buffer = (JOCTET *) (cinfo->mem->alloc_small)
		((j_common_ptr) cinfo, JPOOL_IMAGE, JPEG_OUTPUT_BUF_SIZE * sizeof(JOCTET));
	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = JPEG_OUTPUT_BUF_SIZE;
}

static boolean JPEG_IO_empty_output_buffer (j_compress_ptr cinfo) {
	my_dest_ptr dest = (my_dest_ptr) cinfo->dest;

	if (fwrite(dest->buffer, 1, JPEG_OUTPUT_BUF_SIZE, dest->outfile) != JPEG_OUTPUT_BUF_SIZE) {
		jpeg_in_error = true;
		return false;
	}
	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = JPEG_OUTPUT_BUF_SIZE;
	return true;
}

static void JPEG_IO_term_destination (j_compress_ptr cinfo) {
	my_dest_ptr dest = (my_dest_ptr) cinfo->dest;
	size_t datacount = JPEG_OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

	if (datacount > 0) {
		if (fwrite(dest->buffer, 1, datacount, dest->outfile) != datacount) {
			jpeg_in_error = true;
			return;
		}
	}
	fflush(dest->outfile);
}

#ifndef WITH_FTE_VFS
static void JPEG_IO_set_dest (j_compress_ptr cinfo, FILE *outfile) {
#else
static void JPEG_IO_set_dest (j_compress_ptr cinfo, vfsfile_t *outfile) {
#endif
	my_dest_ptr dest;

	if (!cinfo->dest) {
		cinfo->dest = (struct jpeg_destination_mgr *) (cinfo->mem->alloc_small)(
							(j_common_ptr) cinfo,JPOOL_PERMANENT, sizeof(my_destination_mgr));
	}

	dest = (my_dest_ptr) cinfo->dest;
	dest->pub.init_destination = JPEG_IO_init_destination;
	dest->pub.empty_output_buffer = JPEG_IO_empty_output_buffer;
	dest->pub.term_destination = JPEG_IO_term_destination;
	dest->outfile = outfile;
}

typedef struct my_error_mgr {
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
} jpeg_error_mgr_wrapper;

void jpeg_error_exit (j_common_ptr cinfo) {	
  longjmp(((jpeg_error_mgr_wrapper *) cinfo->err)->setjmp_buffer, 1);
}


int Image_WriteJPEG(char *filename, int quality, byte *pixels, int width, int height) {
	char name[MAX_PATH];
	FILE *outfile;
	jpeg_error_mgr_wrapper jerr;
	struct jpeg_compress_struct cinfo;
	JSAMPROW row_pointer[1];

	if (!jpeg_handle)
		return false;

	snprintf (name, sizeof(name), "%s", filename);	
	if (!(outfile = fopen (name, "wb"))) {
		COM_CreatePath (name);
		if (!(outfile = fopen (name, "wb")))
			return false;
	}

	cinfo.err = qjpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = jpeg_error_exit;
	if (setjmp(jerr.setjmp_buffer)) {
		fclose(outfile);
		return false;
	}
	qjpeg_create_compress(&cinfo);

	jpeg_in_error = false;
	JPEG_IO_set_dest(&cinfo, outfile);

	cinfo.image_width = abs(width); 	
	cinfo.image_height = height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;
	qjpeg_set_defaults(&cinfo);
	qjpeg_set_quality (&cinfo, bound(0, quality, 100), true);
	qjpeg_start_compress(&cinfo, true);

	while (cinfo.next_scanline < height) {
	    *row_pointer = &pixels[cinfo.next_scanline * width * 3];
	    qjpeg_write_scanlines(&cinfo, row_pointer, 1);
		if (jpeg_in_error)
			break;
	}

	qjpeg_finish_compress(&cinfo);
	fclose(outfile);
	qjpeg_destroy_compress(&cinfo);
	return true;
}

#else

#define jpeg_create_compress(cinfo) \
    jpeg_CreateCompress((cinfo), JPEG_LIB_VERSION, (size_t) sizeof(struct jpeg_compress_struct))

typedef struct 
{
  struct jpeg_destination_mgr pub; 
#ifndef WITH_FTE_VFS
  FILE *outfile;
#else
  vfsfile_t *outfile;
#endif
  JOCTET *buffer;
} my_destination_mgr;

typedef my_destination_mgr *my_dest_ptr;

#define JPEG_OUTPUT_BUF_SIZE  4096

static qbool jpeg_in_error = false;

static void JPEG_IO_init_destination(j_compress_ptr cinfo)
{
	my_dest_ptr dest = (my_dest_ptr) cinfo->dest;
	dest->buffer = (JOCTET *) (cinfo->mem->alloc_small)
		((j_common_ptr) cinfo, JPOOL_IMAGE, JPEG_OUTPUT_BUF_SIZE * sizeof(JOCTET));
	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = JPEG_OUTPUT_BUF_SIZE;
}

static boolean JPEG_IO_empty_output_buffer (j_compress_ptr cinfo) 
{
	my_dest_ptr dest = (my_dest_ptr) cinfo->dest;

#ifndef WITH_FTE_VFS
	if (fwrite(dest->buffer, 1, JPEG_OUTPUT_BUF_SIZE, dest->outfile) != JPEG_OUTPUT_BUF_SIZE)
#else
	if (VFS_WRITE(dest->outfile, dest->buffer, JPEG_OUTPUT_BUF_SIZE) != JPEG_OUTPUT_BUF_SIZE)
#endif
	{
		jpeg_in_error = true;
		return false;
	}
	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = JPEG_OUTPUT_BUF_SIZE;
	return true;
}

static void JPEG_IO_term_destination (j_compress_ptr cinfo)
{
	my_dest_ptr dest = (my_dest_ptr) cinfo->dest;
	size_t datacount = JPEG_OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

	if (datacount > 0) {
#ifndef WITH_FTE_VFS
		if (fwrite(dest->buffer, 1, datacount, dest->outfile) != datacount)
#else
		if (((size_t) VFS_WRITE(dest->outfile, dest->buffer, datacount)) != datacount)
#endif
		{
			jpeg_in_error = true;
			return;
		}
	}
#ifndef WITH_FTE_VFS
	fflush(dest->outfile);
#else
	VFS_FLUSH(dest->outfile);
#endif
}

#ifndef WITH_FTE_VFS
static void JPEG_IO_set_dest (j_compress_ptr cinfo, FILE *outfile)
#else
static void JPEG_IO_set_dest (j_compress_ptr cinfo, vfsfile_t *outfile)
#endif
{
	my_dest_ptr dest;

	if (!cinfo->dest) {
		cinfo->dest = (struct jpeg_destination_mgr *) (cinfo->mem->alloc_small)(
							(j_common_ptr) cinfo,JPOOL_PERMANENT, sizeof(my_destination_mgr));
	}

	dest = (my_dest_ptr) cinfo->dest;
	dest->pub.init_destination = JPEG_IO_init_destination;
	dest->pub.empty_output_buffer = JPEG_IO_empty_output_buffer;
	dest->pub.term_destination = JPEG_IO_term_destination;
	dest->outfile = outfile;
}

typedef struct my_error_mgr 
{
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
} jpeg_error_mgr_wrapper;

void jpeg_error_exit (j_common_ptr cinfo)
{	
	longjmp(((jpeg_error_mgr_wrapper *) cinfo->err)->setjmp_buffer, 1);
}

int Image_WriteJPEG(char *filename, int quality, byte *pixels, int width, int height) 
{
	char name[MAX_PATH];
#ifndef WITH_FTE_VFS
	FILE *outfile;
#else
	vfsfile_t *outfile;
#endif
	jpeg_error_mgr_wrapper jerr;
	struct jpeg_compress_struct cinfo;
	JSAMPROW row_pointer[1];

	snprintf (name, sizeof(name), "%s", filename);	
#ifndef WITH_FTE_VFS
	if (!(outfile = fopen (name, "wb"))) {
		FS_CreatePath (name);
		if (!(outfile = fopen (name, "wb")))
			return false;
	}
#else
	if (!(outfile = FS_OpenVFS(name, "wb", FS_NONE_OS))) {
		FS_CreatePath (name);
		if (!(outfile = FS_OpenVFS(name, "wb", FS_NONE_OS)))
			return false;
	}
#endif

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = jpeg_error_exit;
	if (setjmp(jerr.setjmp_buffer)) {
#ifndef WITH_FTE_VFS
		fclose(outfile);
#else
		VFS_CLOSE(outfile);
#endif
		return false;
	}
	jpeg_create_compress(&cinfo);

	jpeg_in_error = false;
	JPEG_IO_set_dest(&cinfo, outfile);

	cinfo.image_width = abs(width); 	
	cinfo.image_height = height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality (&cinfo, bound(0, quality, 100), true);
	jpeg_start_compress(&cinfo, true);

	while (cinfo.next_scanline < height) {
	    *row_pointer = &pixels[cinfo.next_scanline * width * 3];
	    jpeg_write_scanlines(&cinfo, row_pointer, 1);
		if (jpeg_in_error)
			break;
	}

	jpeg_finish_compress(&cinfo);
#ifndef WITH_FTE_VFS
	fclose(outfile);
#else
	VFS_CLOSE(outfile);
#endif
	jpeg_destroy_compress(&cinfo);
	return true;
}


//
// jpeg loading
// stolen from Spikes FTE
//

typedef struct my_error_mgr * my_error_ptr;

// Here's the routine that will replace the standard error_exit method:
METHODDEF(void)
my_error_exit (j_common_ptr cinfo)
{
	// cinfo->err really points to a my_error_mgr struct, so coerce pointer
	my_error_ptr myerr = (my_error_ptr) cinfo->err;

	// Always display the message.
	// We could postpone this until after returning, if we chose.
	(*cinfo->err->output_message) (cinfo);

	// Return control to the setjmp point 
	longjmp(myerr->setjmp_buffer, 1);
}

//
// Sample routine for JPEG decompression.  We assume that the source file name
// is passed in.  We want to return 1 on success, 0 on error.
//

// Expanded data source object for stdio input 

typedef struct 
{
	struct jpeg_source_mgr pub;	// Public fields.

	byte * infile;				// Source stream.
	int currentpos;
	int maxlen;
	JOCTET * buffer;			// Start of buffer.
	boolean start_of_file;		// Have we gotten any data yet?
} my_source_mgr;

typedef my_source_mgr * my_src_ptr;

#define INPUT_BUF_SIZE  4096	// Choose an efficiently fread'able size.

METHODDEF(void)
init_source (j_decompress_ptr cinfo)
{
  my_src_ptr src = (my_src_ptr) cinfo->src;

  src->start_of_file = TRUE;
}

METHODDEF(boolean)
fill_input_buffer (j_decompress_ptr cinfo)
{
	my_source_mgr *src = (my_source_mgr*) cinfo->src;
	size_t nbytes;
  
	nbytes = src->maxlen - src->currentpos;
	if (nbytes > INPUT_BUF_SIZE)
		nbytes = INPUT_BUF_SIZE;
	memcpy(src->buffer, &src->infile[src->currentpos], nbytes);
	src->currentpos+=nbytes;

	if (nbytes <= 0) 
	{
		if (src->start_of_file)	// Treat empty input file as fatal error.
			ERREXIT(cinfo, JERR_INPUT_EMPTY);
		
		WARNMS(cinfo, JWRN_JPEG_EOF);
		
		// Insert a fake EOI marker.
		src->buffer[0] = (JOCTET) 0xFF;
		src->buffer[1] = (JOCTET) JPEG_EOI;
		nbytes = 2;
	}

	src->pub.next_input_byte = src->buffer;
	src->pub.bytes_in_buffer = nbytes;
	src->start_of_file = FALSE;

	return TRUE;
}


METHODDEF(void)
skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
	my_source_mgr *src = (my_source_mgr*) cinfo->src;

	if (num_bytes > 0) 
	{
		while (num_bytes > (long) src->pub.bytes_in_buffer) 
		{
			num_bytes -= (long) src->pub.bytes_in_buffer;
			(void) fill_input_buffer(cinfo);
		}
		src->pub.next_input_byte += (size_t) num_bytes;
		src->pub.bytes_in_buffer -= (size_t) num_bytes;
	}
}

METHODDEF(void)
term_source (j_decompress_ptr cinfo)
{
}

GLOBAL(void)
jpeg_mem_src (j_decompress_ptr cinfo, byte * infile, int maxlen)
{
	my_source_mgr *src;

	if (cinfo->src == NULL) 
	{	
		// First time for this JPEG object?
		cinfo->src = (struct jpeg_source_mgr *)(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof(my_source_mgr));
		
		src = (my_source_mgr*) cinfo->src;
		
		src->buffer = (JOCTET *)(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT, INPUT_BUF_SIZE * sizeof(JOCTET));
	}

	src = (my_source_mgr*) cinfo->src;
	src->pub.init_source = init_source;
	src->pub.fill_input_buffer = fill_input_buffer;
	src->pub.skip_input_data = skip_input_data;
	src->pub.resync_to_restart = jpeg_resync_to_restart; // Use default method.
	src->pub.term_source = term_source;
	src->infile = infile;
	src->pub.bytes_in_buffer = 0; // Forces fill_input_buffer on first read.
	src->pub.next_input_byte = NULL; // until buffer loaded.

	src->currentpos = 0;
	src->maxlen = maxlen;
}

#ifndef WITH_FTE_VFS
byte *Image_LoadJPEG(FILE *fin, int filesize, const char *filename, int matchwidth, int matchheight, int *real_width, int *real_height)
#else
byte *Image_LoadJPEG(vfsfile_t *fin, const char *filename, int matchwidth, int matchheight, int *real_width, int *real_height)
#endif // WITH_FTE_VFS
{
	byte *mem = NULL, *in, *out;
	int i, image_width, image_height;

	byte *infile = NULL;
	int length;
#ifdef WITH_FTE_VFS
	int filesize;
#endif // WITH_FTE_VFS

	// This struct contains the JPEG decompression parameters and pointers to
	// working space (which is allocated as needed by the JPEG library).
	struct jpeg_decompress_struct cinfo;
	
	// We use our private extension JPEG error handler.
	// Note that this struct must live as long as the main JPEG parameter
	// struct, to avoid dangling-pointer problems.
	struct my_error_mgr jerr;
	
	// More stuff 
	JSAMPARRAY buffer;		// Output row buffer.
	int size_stride;		// physical row width in output buffer.

#ifndef WITH_FTE_VFS
	if (!fin && (filesize = FS_FOpenFile (filename, &fin)) == -1)
		return NULL;
#else
	if (!fin && !(fin = FS_OpenVFS(filename, "rb", FS_ANY)))
		return NULL;
	filesize = VFS_GETLEN(fin);
#endif

	infile = (byte *) Q_malloc(length = filesize);
#ifndef WITH_FTE_VFS
	if (fread (infile, 1, filesize, fin) != filesize) 
	{
		Com_DPrintf ("Image_LoadJPEG: fread() failed on %s\n", COM_SkipPath(filename));
		fclose(fin);
		Q_free(infile);
		return NULL;
	}

	fclose(fin);
#else
	if (VFS_READ(fin, infile, filesize, NULL) != filesize) 
	{
		Com_DPrintf ("Image_LoadJPEG: fread() failed on %s\n", COM_SkipPath(filename));
		VFS_CLOSE(fin);
		Q_free(infile);
		return NULL;
	}

	VFS_CLOSE(fin);
#endif // WITH_FTE_VFS

	// Step 1: allocate and initialize JPEG decompression object.

	// We set up the normal JPEG error routines, then override error_exit. 
	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_error_exit;

	// Establish the setjmp return context for my_error_exit to use. */
	if (setjmp(jerr.setjmp_buffer)) 
	{
		// If we get here, the JPEG code has signaled an error.

badjpeg:

		jpeg_destroy_decompress(&cinfo);    

		Q_free(infile);
		Q_free(mem);
		Com_DPrintf ("Image_LoadJPEG: badjpeg %s, len %d\n", COM_SkipPath(filename), length);
		return 0;
	}

	jpeg_create_decompress(&cinfo);

	jpeg_mem_src(&cinfo, infile, length);

	(void) jpeg_read_header(&cinfo, TRUE);  

	(void) jpeg_start_decompress(&cinfo);

	image_width  = cinfo.output_width;
	image_height = cinfo.output_height;

	// Return the width and height.
	if (real_width)
		(*real_width) = image_width;

	if (real_height)
		(*real_height) = image_height;

	if (image_width > IMAGE_MAX_DIMENSIONS || image_height > IMAGE_MAX_DIMENSIONS || image_width <= 0 || image_height <= 0)
	{
		Com_Printf("Bad actual dimensions %dx%d in jpeg %s\n", image_width, image_height, COM_SkipPath(filename));
		goto badjpeg;
	}

	if ((matchwidth && image_width != matchwidth) || (matchheight && image_height != matchheight))
	{
		Com_Printf("Bad match dimensions %dx%d vs %dx%d in jpeg %s\n", image_width, image_height, matchwidth, matchheight, COM_SkipPath(filename));
		goto badjpeg; 
	}

	if (cinfo.output_components!=3)
	{
		Com_Printf("Bad number of componants in jpeg %s\n", COM_SkipPath(filename));
		goto badjpeg;
	}

	size_stride = cinfo.output_width * cinfo.output_components;
	// Make a one-row-high sample array that will go away when done with image.
	buffer = (*cinfo.mem->alloc_sarray)
		((j_common_ptr) &cinfo, JPOOL_IMAGE, size_stride, 1);

	out = mem = Q_malloc(cinfo.output_height*cinfo.output_width*4);
	memset(out, 0, cinfo.output_height*cinfo.output_width*4);

	while (cinfo.output_scanline < cinfo.output_height) 
	{
		(void) jpeg_read_scanlines(&cinfo, buffer, 1);    

		in = buffer[0];
		for (i = 0; i < cinfo.output_width; i++)
		{
			// RGB to RGBA
			*out++ = *in++;
			*out++ = *in++;
			*out++ = *in++;
			*out++ = 255;	
		}	
	}

	(void) jpeg_finish_decompress(&cinfo);

	jpeg_destroy_decompress(&cinfo);

	Q_free(infile);
	return mem;
}

#endif // WITH_JPEG_STATIC
#endif // WITH_JPEG

/************************************ PCX ************************************/

typedef struct pcx_s 
{
    char			manufacturer;
    char			version;
    char			encoding;
    char			bits_per_pixel;
    unsigned short	xmin,ymin,xmax,ymax;
    unsigned short	hres,vres;
    byte			palette[48];
    char			reserved;
    char			color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    char			filler[58];
    byte			data;		
} pcx_t;

#ifndef WITH_FTE_VFS
byte *Image_LoadPCX (FILE *fin, int filesize, const char *filename, int matchwidth, int matchheight, int *real_width, int *real_height) 
#else
byte *Image_LoadPCX (vfsfile_t *fin, const char *filename, int matchwidth, int matchheight, int *real_width, int *real_height) 
#endif // WITH_FTE_VFS
{
	pcx_t *pcx;
	byte *pcxbuf, *data, *out, *pix;
	int x, y, dataByte, runLength, width, height;
#ifdef WITH_FTE_VFS
	int filesize;
#endif

#ifndef WITH_FTE_VFS
	if (!fin && (filesize = FS_FOpenFile (filename, &fin)) == -1)
		return NULL;
#else
	if (!fin && !(fin = FS_OpenVFS(filename, "rb", FS_ANY)))
		return NULL;
	filesize = VFS_GETLEN(fin);
#endif // WITH_FTE_VFS

	pcxbuf = (byte *) Q_malloc(filesize);
#ifndef WITH_FTE_VFS
	if (fread (pcxbuf, 1, filesize, fin) != filesize) 
	{
		Com_DPrintf ("Image_LoadPCX: fread() failed on %s\n", COM_SkipPath(filename));
		fclose(fin);
		Q_free(pcxbuf);
		return NULL;
	}
	fclose(fin);
#else
	if (VFS_READ(fin, pcxbuf, filesize, NULL) != filesize) 
	{
		Com_DPrintf ("Image_LoadPCX: fread() failed on %s\n", COM_SkipPath(filename));
		VFS_CLOSE(fin);
		Q_free(pcxbuf);
		return NULL;
	}
	VFS_CLOSE(fin);
#endif // WITH_FTE_VFS

	pcx = (pcx_t *) pcxbuf;
	pcx->xmax = LittleShort (pcx->xmax);
	pcx->xmin = LittleShort (pcx->xmin);
	pcx->ymax = LittleShort (pcx->ymax);
	pcx->ymin = LittleShort (pcx->ymin);
	pcx->hres = LittleShort (pcx->hres);
	pcx->vres = LittleShort (pcx->vres);
	pcx->bytes_per_line = LittleShort (pcx->bytes_per_line);
	pcx->palette_type = LittleShort (pcx->palette_type);

	pix = &pcx->data;

	if (pcx->manufacturer != 0x0a || pcx->version != 5 || pcx->encoding != 1 || pcx->bits_per_pixel != 8) 
	{
		Com_DPrintf ("Invalid PCX image %s\n", COM_SkipPath(filename));
		Q_free(pcxbuf);
		return NULL;
	}

	width = pcx->xmax + 1;
	height = pcx->ymax + 1;

	// Return the width and height.
	if (real_width)
		(*real_width) = width;

	if (real_height)
		(*real_height) = height;

	if (width > IMAGE_MAX_DIMENSIONS || height > IMAGE_MAX_DIMENSIONS)
	{
		Com_DPrintf ("PCX image %s exceeds maximum supported dimensions\n", COM_SkipPath(filename));
		Q_free(pcxbuf);
		return NULL;
	}

	if ((matchwidth && width != matchwidth) || (matchheight && height != matchheight))
	{
		Q_free(pcxbuf);
		return NULL;
	}
 
	data = out = (byte *) Q_malloc (width * height);

	for (y = 0; y < height; y++, out += width)
	{
		for (x = 0; x < width; ) 
		{
			if (pix - (byte *) pcx > filesize) 
			{
				Com_DPrintf ("Malformed PCX image %s\n", COM_SkipPath(filename));
				Q_free(pcxbuf);
				Q_free(data);
				return NULL;
			}

			dataByte = *pix++;

			if ((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				if (pix - (byte *) pcx > filesize)
				{
					Com_DPrintf ("Malformed PCX image %s\n", COM_SkipPath(filename));
					Q_free(pcxbuf);
					Q_free(data);
					return NULL;
				}
				dataByte = *pix++;
			}
			else 
			{
				runLength = 1;
			}

			if (runLength + x > width + 1) 
			{
				Com_DPrintf ("Malformed PCX image %s\n", COM_SkipPath(filename));
				Q_free(pcxbuf);
				Q_free(data);
				return NULL;
			}

			while (runLength-- > 0)
				out[x++] = dataByte;
		}
	}

	if (pix - (byte *) pcx > filesize) 
	{
		Com_DPrintf ("Malformed PCX image %s\n", COM_SkipPath(filename));
		Q_free(pcxbuf);
		Q_free(data);
		return NULL;
	}

	Q_free(pcxbuf);
	return data;
}

#ifdef GLQUAKE

// This does't load 32bit pcx, just convert 8bit color buffer to 32bit buffer, so we can make from this texture.
#ifndef WITH_FTE_VFS
byte *Image_LoadPCX_As32Bit (FILE *fin, int filesize, char *filename, int matchwidth, int matchheight, int *real_width, int *real_height)
#else
byte *Image_LoadPCX_As32Bit (vfsfile_t *fin, char *filename, int matchwidth, int matchheight, int *real_width, int *real_height)
#endif // WITH_FTE_VFS
{
	int image_width, image_height;
	unsigned *out;
	int size, i;

#ifndef WITH_FTE_VFS
	byte *pix = Image_LoadPCX (fin, filesize, filename, matchwidth, matchheight, &image_width, &image_height);
#else
	byte *pix = Image_LoadPCX (fin, filename, matchwidth, matchheight, &image_width, &image_height);
#endif // WITH_FTE_VFS
	if (!pix)
		return NULL;

	if (real_width)
		(*real_width) = image_width;

	if (real_height)
		(*real_height) = image_height;

	size = image_width * image_height;
	out = Q_malloc(size * sizeof(unsigned));

	for (i = 0; i < size; i++)
		out[i] = d_8to24table[pix[i]];

	Q_free(pix);

	return (byte*) out;
}

#endif

#ifdef GLQUAKE
int Image_WritePCX (char *filename, byte *data, int width, int height, byte *palette)
#else // SOFTWARE
int Image_WritePCX (char *filename, byte *data, int width, int height, int rowbytes, byte *palette)
#endif // GLQUAKE
{
	#ifdef GLQUAKE
	int rowbytes = width;
	#endif // GLQUAKE
	
	int i, j, length;
	byte *pack;
	pcx_t *pcx;

	if (!(pcx = (pcx_t *) Q_malloc (width * height * 2 + 1000)))
		return false;

	pcx->manufacturer = 0x0a;
	pcx->version = 5;		
 	pcx->encoding = 1;		
	pcx->bits_per_pixel = 8;
	pcx->xmin = 0;
	pcx->ymin = 0;
	pcx->xmax = LittleShort((short) (width - 1));
	pcx->ymax = LittleShort((short) (height - 1));
	pcx->hres = LittleShort((short) width);
	pcx->vres = LittleShort((short) height);
	memset (pcx->palette, 0, sizeof(pcx->palette));
	pcx->color_planes = 1;		
	pcx->bytes_per_line = LittleShort((short) width);
	pcx->palette_type = LittleShort(1);		
	memset (pcx->filler, 0, sizeof(pcx->filler));

	pack = &pcx->data;

	for (i = 0; i < height; i++) 
	{
		for (j = 0; j < width; j++) 
		{
			if ((*data & 0xc0) != 0xc0)
				*pack++ = *data++;
			else {
				*pack++ = 0xc1;
				*pack++ = *data++;
			}
		}
		data += rowbytes - width;
	}

	*pack++ = 0x0c;	
	for (i = 0; i < 768; i++)
		*pack++ = *palette++;

	length = pack - (byte *) pcx;
	if (!(FS_WriteFile_2 (filename, pcx, length)))
	{
		Q_free(pcx);
		return false;
	}

	Q_free(pcx);
	return true;
}

/*********************************** INIT ************************************/

void Image_Init(void) 
{
	Cvar_SetCurrentGroup(CVAR_GROUP_SCREENSHOTS);

	// Software.
	#ifndef GLQUAKE
	Cvar_Register (&image_png_dither_onload);
	#endif // !GLQUAKE
	
	#ifdef WITH_PNG
	#ifndef WITH_PNG_STATIC
	if (PNG_LoadLibrary())
		QLib_RegisterModule(qlib_libpng, PNG_FreeLibrary);
	#endif // WITH_PNG_STATIC
	Cvar_Register (&image_png_compression_level);
	#endif // WITH_PNG

	#ifdef WITH_JPEG
	#ifndef WITH_JPEG_STATIC
	if (JPEG_LoadLibrary())
		QLib_RegisterModule(qlib_libjpeg, JPEG_FreeLibrary);
	#endif // WITH_JPEG_STATIC
	Cvar_Register (&image_jpeg_quality_level);
	#endif // WITH_JPEG

	Cvar_ResetCurrentGroup();
}


