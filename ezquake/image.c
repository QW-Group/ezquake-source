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

#include "quakedef.h"
#include "image.h"
#include "modules.h"

#ifdef WITH_PNG
#include "png.h"
#endif

#ifdef WITH_JPEG
#include "jpeglib.h"
#include <setjmp.h>
#endif

#define	IMAGE_MAX_DIMENSIONS	4096

int image_width, image_height;

cvar_t	image_png_compression_level = {"image_png_compression_level", "1"};
cvar_t	image_jpeg_quality_level = {"image_jpeg_quality_level", "75"};

/***************************** IMAGE RESAMPLING ******************************/

#ifdef GLQUAKE

static void Image_Resample32LerpLine (byte *in, byte *out, int inwidth, int outwidth) {
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

static void Image_Resample24LerpLine (byte *in, byte *out, int inwidth, int outwidth) {
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
								void *outdata, int outwidth, int outheight, int quality) {
	if (quality) {
		int i, j, r, yi, oldy, f, fstep, endy = (inheight - 1), lerp;
		int inwidth4 = inwidth * 4, outwidth4 = outwidth * 4;
		byte *inrow, *out, *row1, *row2, *memalloc;

		out = outdata;
		fstep = (int) (inheight * 65536.0f / outheight);

		memalloc = Q_Malloc(2 * outwidth4);
		row1 = memalloc;
		row2 = memalloc + outwidth4;
		inrow = (byte *) indata;
		oldy = 0;
		Image_Resample32LerpLine (inrow, row1, inwidth, outwidth);
		Image_Resample32LerpLine (inrow + inwidth4, row2, inwidth, outwidth);
		for (i = 0, f = 0; i < outheight; i++, f += fstep)	{
			yi = f >> 16;
			if (yi < endy) {
				lerp = f & 0xFFFF;
				if (yi != oldy) {
					inrow = (byte *) indata + inwidth4 * yi;
					if (yi == oldy + 1)
						memcpy(row1, row2, outwidth4);
					else
						Image_Resample32LerpLine (inrow, row1, inwidth, outwidth);
					Image_Resample32LerpLine (inrow + inwidth4, row2, inwidth, outwidth);
					oldy = yi;
				}
				j = outwidth - 4;
				while(j >= 0) {
					LERPBYTE(0); LERPBYTE(1); LERPBYTE(2); LERPBYTE(3);
					LERPBYTE(4); LERPBYTE(5); LERPBYTE(6); LERPBYTE(7);
					LERPBYTE(8); LERPBYTE(9); LERPBYTE(10); LERPBYTE(11);
					LERPBYTE(12); LERPBYTE(13); LERPBYTE(14); LERPBYTE(15);
					out += 16;
					row1 += 16;
					row2 += 16;
					j -= 4;
				}
				if (j & 2) {
					LERPBYTE(0); LERPBYTE(1); LERPBYTE(2); LERPBYTE(3);
					LERPBYTE(4); LERPBYTE(5); LERPBYTE(6); LERPBYTE(7);
					out += 8;
					row1 += 8;
					row2 += 8;
				}
				if (j & 1) {
					LERPBYTE(0); LERPBYTE(1); LERPBYTE(2); LERPBYTE(3);
					out += 4;
					row1 += 4;
					row2 += 4;
				}
				row1 -= outwidth4;
				row2 -= outwidth4;
			} else {
				if (yi != oldy) {
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
		free(memalloc);
	} else {
		int i, j;
		unsigned int frac, fracstep, *inrow, *out;

		out = outdata;

		fracstep = inwidth * 0x10000 / outwidth;
		for (i = 0; i < outheight; i++) {
			inrow = (int *) indata + inwidth * (i * inheight / outheight);
			frac = fracstep >> 1;
			j = outwidth - 4;
			while (j >= 0) {
				out[0] = inrow[frac >> 16]; frac += fracstep;
				out[1] = inrow[frac >> 16]; frac += fracstep;
				out[2] = inrow[frac >> 16]; frac += fracstep;
				out[3] = inrow[frac >> 16]; frac += fracstep;
				out += 4;
				j -= 4;
			}
			if (j & 2) {
				out[0] = inrow[frac >> 16]; frac += fracstep;
				out[1] = inrow[frac >> 16]; frac += fracstep;
				out += 2;
			}
			if (j & 1) {
				out[0] = inrow[frac >> 16]; frac += fracstep;
				out += 1;
			}
		}
	}
}

static void Image_Resample24 (void *indata, int inwidth, int inheight,
					 void *outdata, int outwidth, int outheight, int quality) {
	if (quality) {
		int i, j, r, yi, oldy, f, fstep, endy = (inheight - 1), lerp;
		int inwidth3 = inwidth * 3, outwidth3 = outwidth * 3;
		byte *inrow, *out, *row1, *row2, *memalloc;

		out = outdata;
		fstep = (int) (inheight * 65536.0f / outheight);

		memalloc = Q_Malloc(2 * outwidth3);	
		row1 = memalloc;
		row2 = memalloc + outwidth3;
		inrow = (byte *) indata;
		oldy = 0;
		Image_Resample24LerpLine (inrow, row1, inwidth, outwidth);
		Image_Resample24LerpLine (inrow + inwidth3, row2, inwidth, outwidth);
		for (i = 0, f = 0; i < outheight; i++, f += fstep)	{
			yi = f >> 16;
			if (yi < endy) {
				lerp = f & 0xFFFF;
				if (yi != oldy) {
					inrow = (byte *) indata + inwidth3 * yi;
					if (yi == oldy + 1)
						memcpy(row1, row2, outwidth3);
					else
						Image_Resample24LerpLine (inrow, row1, inwidth, outwidth);
					Image_Resample24LerpLine (inrow + inwidth3, row2, inwidth, outwidth);
					oldy = yi;
				}
				j = outwidth - 4;
				while(j >= 0) {
					LERPBYTE(0); LERPBYTE(1); LERPBYTE(2);
					LERPBYTE(3); LERPBYTE(4); LERPBYTE(5);
					LERPBYTE(6); LERPBYTE(7); LERPBYTE(8);
					LERPBYTE(9); LERPBYTE(10); LERPBYTE(11);
					out += 12;
					row1 += 12;
					row2 += 12;
					j -= 4;
				}
				if (j & 2) {
					LERPBYTE(0); LERPBYTE(1); LERPBYTE(2);
					LERPBYTE(3); LERPBYTE(4); LERPBYTE(5);
					out += 6;
					row1 += 6;
					row2 += 6;
				}
				if (j & 1) {
					LERPBYTE(0); LERPBYTE(1); LERPBYTE(2);
					out += 3;
					row1 += 3;
					row2 += 3;
				}
				row1 -= outwidth3;
				row2 -= outwidth3;
			} else {
				if (yi != oldy) {
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
		free(memalloc);
	} else {
		int i, j, f;
		unsigned int frac, fracstep, inwidth3 = inwidth * 3;
		byte *inrow, *out;

		out = outdata;

		fracstep = inwidth * 0x10000 / outwidth;
		for (i = 0; i < outheight; i++) {
			inrow = (byte *) indata + inwidth3 * (i * inheight / outheight);
			frac = fracstep >> 1;
			j = outwidth - 4;
			while (j >= 0) {
				f = (frac >> 16) * 3; NOLERPBYTE(0); NOLERPBYTE(1); NOLERPBYTE(2); frac += fracstep;
				f = (frac >> 16) * 3; NOLERPBYTE(0); NOLERPBYTE(1); NOLERPBYTE(2); frac += fracstep;
				f = (frac >> 16) * 3; NOLERPBYTE(0); NOLERPBYTE(1); NOLERPBYTE(2); frac += fracstep;
				f = (frac >> 16) * 3; NOLERPBYTE(0); NOLERPBYTE(1); NOLERPBYTE(2); frac += fracstep;
				j -= 4;
			}
			if (j & 2) {
				f = (frac >> 16) * 3; NOLERPBYTE(0); NOLERPBYTE(1); NOLERPBYTE(2); frac += fracstep;
				f = (frac >> 16) * 3; NOLERPBYTE(0); NOLERPBYTE(1); NOLERPBYTE(2); frac += fracstep;
				out += 2;
			}
			if (j & 1) {
				f = (frac >> 16) * 3; NOLERPBYTE(0); NOLERPBYTE(1); NOLERPBYTE(2); frac += fracstep;
				out += 1;

			}
		}
	}
}

void Image_Resample (void *indata, int inwidth, int inheight,
					 void *outdata, int outwidth, int outheight, int bpp, int quality) {
	if (bpp == 4)
		Image_Resample32(indata, inwidth, inheight, outdata, outwidth, outheight, quality);
	else if (bpp == 3)
		Image_Resample24(indata, inwidth, inheight, outdata, outwidth, outheight, quality);
	else
		Sys_Error("Image_Resample: unsupported bpp (%d)", bpp);
}

void Image_MipReduce (byte *in, byte *out, int *width, int *height, int bpp) {
	int x, y, nextrow;

	nextrow = *width * bpp;

	if (*width > 1) {
		*width >>= 1;
		if (*height > 1) {

			*height >>= 1;
			if (bpp == 4) {
				for (y = 0; y < *height; y++) {
					for (x = 0; x < *width; x++) {
						out[0] = (byte) ((in[0] + in[4] + in[nextrow] + in[nextrow + 4]) >> 2);
						out[1] = (byte) ((in[1] + in[5] + in[nextrow + 1] + in[nextrow + 5]) >> 2);
						out[2] = (byte) ((in[2] + in[6] + in[nextrow + 2] + in[nextrow + 6]) >> 2);
						out[3] = (byte) ((in[3] + in[7] + in[nextrow + 3] + in[nextrow + 7]) >> 2);
						out += 4;
						in += 8;
					}
					in += nextrow;
				}
			} else if (bpp == 3) {
				for (y = 0; y < *height; y++) {
					for (x = 0; x < *width; x++) {
						out[0] = (byte) ((in[0] + in[3] + in[nextrow] + in[nextrow + 3]) >> 2);
						out[1] = (byte) ((in[1] + in[4] + in[nextrow + 1] + in[nextrow + 4]) >> 2);
						out[2] = (byte) ((in[2] + in[5] + in[nextrow + 2] + in[nextrow + 5]) >> 2);
						out += 3;
						in += 6;
					}
					in += nextrow;
				}
			} else {
				Sys_Error("Image_MipReduce: unsupported bpp (%d)", bpp);
			}
		} else {

			if (bpp == 4) {
				for (y = 0; y < *height; y++) {
					for (x = 0; x < *width; x++) {
						out[0] = (byte) ((in[0] + in[4]) >> 1);
						out[1] = (byte) ((in[1] + in[5]) >> 1);
						out[2] = (byte) ((in[2] + in[6]) >> 1);
						out[3] = (byte) ((in[3] + in[7]) >> 1);
						out += 4;
						in += 8;
					}
				}
			} else if (bpp == 3) {
				for (y = 0; y < *height; y++) {
					for (x = 0; x < *width; x++) {
						out[0] = (byte) ((in[0] + in[3]) >> 1);
						out[1] = (byte) ((in[1] + in[4]) >> 1);
						out[2] = (byte) ((in[2] + in[5]) >> 1);
						out += 3;
						in += 6;
					}
				}
			} else {
				Sys_Error("Image_MipReduce: unsupported bpp (%d)", bpp);
			}
		}
	} else if (*height > 1) {

		*height >>= 1;
		if (bpp == 4) {
			for (y = 0; y < *height; y++) {
				for (x = 0; x < *width; x++) {
					out[0] = (byte) ((in[0] + in[nextrow]) >> 1);
					out[1] = (byte) ((in[1] + in[nextrow + 1]) >> 1);
					out[2] = (byte) ((in[2] + in[nextrow + 2]) >> 1);
					out[3] = (byte) ((in[3] + in[nextrow + 3]) >> 1);
					out += 4;
					in += 4;
				}
				in += nextrow;
			}
		} else if (bpp == 3) {
			for (y = 0; y < *height; y++) {
				for (x = 0; x < *width; x++) {
					out[0] = (byte) ((in[0] + in[nextrow]) >> 1);
					out[1] = (byte) ((in[1] + in[nextrow + 1]) >> 1);
					out[2] = (byte) ((in[2] + in[nextrow + 2]) >> 1);
					out += 3;
					in += 3;
				}
				in += nextrow;
			}
		} else {
			Sys_Error("Image_MipReduce: unsupported bpp (%d)", bpp);
		}
	} else {
		Sys_Error("Image_MipReduce: Input texture has dimensions %dx%d", width, height);
	}
}

#endif

/************************************ PNG ************************************/

#ifdef WITH_PNG


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

static qboolean PNG_LoadLibrary(void) {
	if (COM_CheckParm("-nolibpng"))
		return false;

	if (!(png_handle = QLIB_LOADLIBRARY("libpng"))) {
#ifdef __linux__
		if (!(zlib_handle = dlopen("libz.so", RTLD_NOW | RTLD_GLOBAL))) {
			QLib_MissingModuleError(QLIB_ERROR_MODULE_NOT_FOUND, "libz", "-nolibpng", "png image features");
			return false;
		}
		if (!(png_handle = QLIB_LOADLIBRARY("libpng")))
#endif
		{
			PNG_FreeLibrary();
			QLib_MissingModuleError(QLIB_ERROR_MODULE_NOT_FOUND, "libpng", "-nolibpng", "png image features");
			return false;
		}
	}

	if (!QLib_ProcessProcdef(png_handle, pngprocs, NUM_PNGPROCS)) {
		PNG_FreeLibrary();
		QLib_MissingModuleError(QLIB_ERROR_MODULE_MISSING_PROC, "libpng", "-nolibpng", "png image features");
		return false;
	}

	return true;
}


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


byte *Image_LoadPNG (FILE *fin, char *filename, int matchwidth, int matchheight) {
	byte header[8], **rowpointers, *data;
	png_structp png_ptr;
	png_infop pnginfo;
	int y, width, height, bitdepth, colortype, interlace, compression, filter, bytesperpixel;
	unsigned long rowbytes;

	if (!png_handle)
		return NULL;

	if (!fin && FS_FOpenFile (filename, &fin) == -1)
		return NULL;

	fread(header, 1, 8, fin);

	if (qpng_sig_cmp(header, 0, 8)) {
		Com_DPrintf ("Invalid PNG image %s\n", COM_SkipPath(filename));
		fclose(fin);
		return NULL;
	}

	if (!(png_ptr = qpng_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))) {
		fclose(fin);
		return NULL;
	}

	if (!(pnginfo = qpng_create_info_struct(png_ptr))) {
		qpng_destroy_read_struct(&png_ptr, &pnginfo, NULL);
		fclose(fin);
		return NULL;
	}

	if (setjmp(png_ptr->jmpbuf)) {
		qpng_destroy_read_struct(&png_ptr, &pnginfo, NULL);
		fclose(fin);
		return NULL;
	}

    qpng_set_read_fn(png_ptr, fin, PNG_IO_user_read_data);
	qpng_set_sig_bytes(png_ptr, 8);
	qpng_read_info(png_ptr, pnginfo);
	qpng_get_IHDR(png_ptr, pnginfo, (png_uint_32 *) &width, (png_uint_32 *) &height, &bitdepth,
		&colortype, &interlace, &compression, &filter);

	if (width > IMAGE_MAX_DIMENSIONS || height > IMAGE_MAX_DIMENSIONS) {
		Com_DPrintf ("PNG image %s exceeds maximum supported dimensions\n", COM_SkipPath(filename));
		qpng_destroy_read_struct(&png_ptr, &pnginfo, NULL);
		fclose(fin);
		return NULL;
	}

	if ((matchwidth && width != matchwidth) || (matchheight && height != matchheight)) {
		qpng_destroy_read_struct(&png_ptr, &pnginfo, NULL);
		fclose(fin);
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
		fclose(fin);
		return NULL;
	}

	data = Q_Malloc(height * rowbytes );
	rowpointers = Q_Malloc(height * sizeof(*rowpointers));

	for (y = 0; y < height; y++)
		rowpointers[y] = data + y * rowbytes;

	qpng_read_image(png_ptr, rowpointers);
	qpng_read_end(png_ptr, NULL);

	qpng_destroy_read_struct(&png_ptr, &pnginfo, NULL);
	free(rowpointers);
	fclose(fin);
	image_width = width;
	image_height = height;
	return data;
}

int Image_WritePNG (char *filename, int compression, byte *pixels, int width, int height) {
	char name[MAX_OSPATH];
	int i, bpp = 3, pngformat, width_sign;
	FILE *fp;
	png_structp png_ptr;
	png_infop info_ptr;
	png_byte **rowpointers;
	Q_snprintfz (name, sizeof(name), "%s/%s", com_basedir, filename);

	if (!png_handle)
		return false;

	width_sign = (width < 0) ? -1 : 1;
	width = abs(width);

	if (!(fp = fopen (name, "wb"))) {
		COM_CreatePath (name);
		if (!(fp = fopen (name, "wb")))
			return false;
	}

	if (!(png_ptr = qpng_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))) {
		fclose(fp);
		return false;
	}

	if (!(info_ptr = qpng_create_info_struct(png_ptr))) {
		qpng_destroy_write_struct(&png_ptr, (png_infopp) NULL);
		fclose(fp);
		return false;
	}

	if (setjmp(png_ptr->jmpbuf)) {
		qpng_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(fp);
		return false;
	}

    qpng_set_write_fn(png_ptr, fp, PNG_IO_user_write_data, PNG_IO_user_flush_data);
	qpng_set_compression_level(png_ptr, bound(Z_NO_COMPRESSION, compression, Z_BEST_COMPRESSION));

	pngformat = (bpp == 4) ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB;
	qpng_set_IHDR(png_ptr, info_ptr, width, height, 8, pngformat,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	qpng_write_info(png_ptr, info_ptr);

	rowpointers = Q_Malloc (height * sizeof(*rowpointers));
	for (i = 0; i < height; i++)
		rowpointers[i] = pixels + i * width_sign * width * bpp;
	qpng_write_image(png_ptr, rowpointers);
	qpng_write_end(png_ptr, info_ptr);
	free(rowpointers);
	qpng_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(fp);
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
	char name[MAX_OSPATH];
	FILE *fp;
	png_structp png_ptr;
	png_infop info_ptr;
	png_byte **rowpointers;

	if (!png_handle)
		return false;

	Q_snprintfz (name, sizeof(name), "%s/%s", com_basedir, filename);
	
	if (!(fp = fopen (name, "wb"))) {
		COM_CreatePath (name);
		if (!(fp = fopen (name, "wb")))
			return false;
	}

	if (!(png_ptr = qpng_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))) {
		fclose(fp);
		return false;
	}

	if (!(info_ptr = qpng_create_info_struct(png_ptr))) {
		qpng_destroy_write_struct(&png_ptr, (png_infopp) NULL);
		fclose(fp);
		return false;
	}

	if (setjmp(png_ptr->jmpbuf)) {
		qpng_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(fp);
		return false;
	}

    qpng_set_write_fn(png_ptr, fp, PNG_IO_user_write_data, PNG_IO_user_flush_data);
	qpng_set_compression_level(png_ptr, bound(Z_NO_COMPRESSION, compression, Z_BEST_COMPRESSION));

	qpng_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_PALETTE,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	qpng_set_PLTE(png_ptr, info_ptr, (png_color *) palette, 256);

	qpng_write_info(png_ptr, info_ptr);

	rowpointers = Q_Malloc (height * sizeof(*rowpointers));
	for (i = 0; i < height; i++)
		rowpointers[i] = pixels + i * rowbytes;
	qpng_write_image(png_ptr, rowpointers);
	qpng_write_end(png_ptr, info_ptr);
	free(rowpointers);
	qpng_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(fp);
	return true;
}

#endif

/************************************ TGA ************************************/

#define TGA_MAXCOLORS	(1 << 14)

/* Definitions for image types. */
#define TGA_Null		0	/* no image data */
#define TGA_Map			1	/* Uncompressed, color-mapped images. */
#define TGA_RGB			2	/* Uncompressed, RGB images. */
#define TGA_Mono		3	/* Uncompressed, black and white images. */
#define TGA_RLEMap		9	/* Runlength encoded color-mapped images. */
#define TGA_RLERGB		10	/* Runlength encoded RGB images. */
#define TGA_RLEMono		11	/* Compressed, black and white images. */
#define TGA_CompMap		32	/* Compressed color-mapped data, using Huffman, Delta, and runlength encoding. */
#define TGA_CompMap4	33	/* Compressed color-mapped data, using Huffman, Delta, and runlength encoding.  4-pass quadtree-type process. */

/* Definitions for interleave flag. */
#define TGA_IL_None		0	/* non-interleaved. */
#define TGA_IL_Two		1	/* two-way (even/odd) interleaving */
#define TGA_IL_Four		2	/* four way interleaving */
#define TGA_IL_Reserved	3	/* reserved */

/* Definitions for origin flag */
#define TGA_O_UPPER		0	/* Origin in lower left-hand corner. */
#define TGA_O_LOWER		1	/* Origin in upper left-hand corner. */

typedef struct targaHeader_s {
	byte			id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	byte			colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	byte			pixel_size, attributes;
} targaHeader_t;

static int fgetLittleShort (FILE *f) {
	byte b1, b2;

	b1 = fgetc(f);
	b2 = fgetc(f);

	return (short) (b1 + (b2 << 8));
}

byte *Image_LoadTGA (FILE *fin, char *filename, int matchwidth, int matchheight) {
	int w, h, x, y, realrow, truerow, baserow, i, temp1, temp2, pixel_size, map_idx;
	int RLE_count, RLE_flag, size, interleave, origin;
	qboolean mapped, rlencoded;
	byte *data, *dst, r, g, b, a, j, k, l, *ColorMap;
	targaHeader_t header;

	if (!fin && FS_FOpenFile (filename, &fin) == -1)
		return NULL;

	header.id_length = fgetc(fin);
	header.colormap_type = fgetc(fin);
	header.image_type = fgetc(fin);

	header.colormap_index = fgetLittleShort(fin);
	header.colormap_length = fgetLittleShort(fin);
	header.colormap_size = fgetc(fin);
	header.x_origin = fgetLittleShort(fin);
	header.y_origin = fgetLittleShort(fin);
	header.width = fgetLittleShort(fin);
	header.height = fgetLittleShort(fin);
	header.pixel_size = fgetc(fin);
	header.attributes = fgetc(fin);

	if (header.width > IMAGE_MAX_DIMENSIONS || header.height > IMAGE_MAX_DIMENSIONS) {
		Com_DPrintf ("TGA image %s exceeds maximum supported dimensions\n", COM_SkipPath(filename));
		fclose(fin);
		return NULL;
	}

	if ((matchwidth && header.width != matchwidth) || (matchheight && header.height != matchheight)) {
		fclose(fin);
		return NULL;
	}

	if (header.id_length != 0)
		fseek(fin, header.id_length, SEEK_CUR);

	// validate TGA type
	switch (header.image_type) {		
		case TGA_Map: case TGA_RGB: case TGA_Mono: case TGA_RLEMap: case TGA_RLERGB: case TGA_RLEMono:
			break;
		default:
			Com_DPrintf ("Unsupported TGA image %s: "
				"Only type 1 (map), 2 (RGB), 3 (mono), 9 (RLEmap), 10 (RLERGB), 11 (RLEmono) TGA images supported\n",
				COM_SkipPath(filename));
			fclose(fin);
			return NULL;
	}

	// validate color depth
	switch (header.pixel_size) {
		case 8:	case 15: case 16: case 24: case 32:
			break;
		default:
			Com_DPrintf ("Unsupported TGA image %s: Only 8, 15, 16, 24 or 32 bit images (with colormaps) supported\n",
				COM_SkipPath(filename));
			fclose(fin);
			return NULL;
	}

	r = g = b = a = l = 0;

	// if required, read the color map information
	ColorMap = NULL;
	mapped = ( header.image_type == TGA_Map || header.image_type == TGA_RLEMap) && header.colormap_type == 1;
	if ( mapped ) {
		// validate colormap size
		switch( header.colormap_size ) {
			case 8:	case 15: case 16: case 32: case 24:
				break;
			default:
				Com_DPrintf ("Unsupported TGA image %s: Only 8, 15, 16, 24 or 32 bit colormaps supported\n",
					COM_SkipPath(filename));
				fclose(fin);
				return NULL;
		}

		temp1 = header.colormap_index;
		temp2 = header.colormap_length;
		if ((temp1 + temp2 + 1) >= TGA_MAXCOLORS) {
			fclose(fin);
			return NULL;
		}
		ColorMap = Q_Malloc (TGA_MAXCOLORS * 4);
		map_idx = 0;
		for (i = temp1; i < temp1 + temp2; ++i, map_idx += 4 ) {
			// read appropriate number of bytes, break into rgb & put in map
			switch( header.colormap_size ) {
				case 8:	// grey scale, read and triplicate
					r = g = b = getc(fin);
					a = 255;
					break;
				case 15:	// 5 bits each of red green and blue
							// watch byte order
					j = getc(fin);
					k = getc(fin);
					l = ((unsigned int) k << 8) + j;
					r = (byte) ( ((k & 0x7C) >> 2) << 3 );
					g = (byte) ( (((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3 );
					b = (byte) ( (j & 0x1F) << 3 );
					a = 255;
					break;
				case 16:	// 5 bits each of red green and blue, 1 alpha bit
							// watch byte order
					j = getc(fin);
					k = getc(fin);
					l = ((unsigned int) k << 8) + j;
					r = (byte) ( ((k & 0x7C) >> 2) << 3 );
					g = (byte) ( (((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3 );
					b = (byte) ( (j & 0x1F) << 3 );
					a = (k & 0x80) ? 255 : 0;
					break;
				case 24:	// 8 bits each of blue, green and red
					b = getc(fin);
					g = getc(fin);
					r = getc(fin);
					a = 255;
					l = 0;
					break;
				case 32:	// 8 bits each of blue, green, red and alpha
					b = getc(fin);
					g = getc(fin);
					r = getc(fin);
					a = getc(fin);
					l = 0;
					break;
			}
			ColorMap[map_idx + 0] = r;
			ColorMap[map_idx + 1] = g;
			ColorMap[map_idx + 2] = b;
			ColorMap[map_idx + 3] = a;
		}
	}

	// check run-length encoding
	rlencoded = (
		header.image_type == TGA_RLEMap ||
		header.image_type == TGA_RLERGB ||
		header.image_type == TGA_RLEMono
		);
	RLE_count = RLE_flag = 0;

	image_width = w = header.width;
	image_height = h = header.height;

	size = w * h * 4;
	data = Q_Calloc(size, 1);

	// read the Targa file body and convert to portable format
	pixel_size = header.pixel_size;
	origin = (header.attributes & 0x20) >> 5;
	interleave = (header.attributes & 0xC0) >> 6;
	truerow = 0;
	baserow = 0;
	for (y = 0; y < h; y++) {
		realrow = truerow;
		if ( origin == TGA_O_UPPER )
			realrow = h - realrow - 1;

		dst = data + realrow * w * 4;

		for (x = 0; x < w; x++) {
			// check if run length encoded
			if( rlencoded ) {
				if( !RLE_count ) {
					// have to restart run
					i = getc(fin);
					RLE_flag = (i & 0x80);
					if( !RLE_flag ) {
						// stream of unencoded pixels
						RLE_count = i + 1;
					} else {
						// single pixel replicated
						RLE_count = i - 127;
					}
					// decrement count & get pixel
					--RLE_count;
				} else {
					// have already read count & (at least) first pixel
					--RLE_count;
					if( RLE_flag )
						// replicated pixels
						goto PixEncode;
				}
			}

			// read appropriate number of bytes, break into RGB. */
			switch (pixel_size) {
				case 8:	// grey scale, read and triplicate. */
					r = g = b = l = getc(fin);
					a = 255;
					break;
				case 15:	// 5 bits each of red green and blue. */
							// watch byte order. */
					j = getc(fin);
					k = getc(fin);
					l = ((unsigned int) k << 8) + j;
					r = (byte) ( ((k & 0x7C) >> 2) << 3 );
					g = (byte) ( (((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3 );
					b = (byte) ( (j & 0x1F) << 3 );
					a = 255;
					break;
				case 16:	// 5 bits each of red green and blue, 1 alpha bit. */
							// watch byte order. */
					j = getc(fin);
					k = getc(fin);
					l = ((unsigned int) k << 8) + j;
					r = (byte) ( ((k & 0x7C) >> 2) << 3 );
					g = (byte) ( (((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3 );
					b = (byte) ( (j & 0x1F) << 3 );
					a = (k & 0x80) ? 255 : 0;
					break;
				case 24:	// 8 bits each of blue, green and red. */
					b = getc(fin);
					g = getc(fin);
					r = getc(fin);
					a = 255;
					l = 0;
					break;
				case 32:	// 8 bits each of blue, green, red and alpha. */
					b = getc(fin);
					g = getc(fin);
					r = getc(fin);
					a = getc(fin);
					l = 0;
					break;
				default:
					Com_DPrintf ("Malformed TGA image %s: Illegal pixel_size '%d'\n",
						COM_SkipPath(filename), pixel_size);
					fclose(fin);
					free(data);
					if (mapped)
						free(ColorMap);
					return NULL;
			}
PixEncode:
			if (mapped) {
				map_idx = l * 4;
				*dst++ = ColorMap[map_idx + 0];
				*dst++ = ColorMap[map_idx + 1];
				*dst++ = ColorMap[map_idx + 2];
				*dst++ = ColorMap[map_idx + 3];
			} else {
				*dst++ = r;
				*dst++ = g;
				*dst++ = b;
				*dst++ = a;
			}
		}

		if (interleave == TGA_IL_Four)
			truerow += 4;
		else if (interleave == TGA_IL_Two)
			truerow += 2;
		else
			truerow++;
		if (truerow >= h)
			truerow = ++baserow;
	}

	if (mapped)
		free(ColorMap);
	fclose(fin);
	return data;
}

int Image_WriteTGA (char *filename, byte *pixels, int width, int height) {
	byte *buffer;
	int size;
	qboolean retval = true;

	size = width * height * 3;
	buffer = Q_Malloc (size + 18);
	memset (buffer, 0, 18);
	buffer[2] = 2; 
	buffer[12] = width & 255;
	buffer[13] = width >> 8;
	buffer[14] = height & 255;
	buffer[15] = height >> 8;
	buffer[16] = 24;

	memcpy (buffer + 18, pixels, size);

	if (!(COM_WriteFile (filename, buffer, size + 18)))
		retval = false;
	free (buffer);
	return retval;
}

/*********************************** JPEG ************************************/

#ifdef WITH_JPEG


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

static qboolean JPEG_LoadLibrary(void) {
	if (COM_CheckParm("-nolibjpeg"))
		return false;

	if (!(jpeg_handle = QLIB_LOADLIBRARY("libjpeg"))) {
		QLib_MissingModuleError(QLIB_ERROR_MODULE_NOT_FOUND, "libjpeg", "-nolibjpeg", "jpeg image features");
		return false;
	}

	if (!QLib_ProcessProcdef(jpeg_handle, jpegprocs, NUM_JPEGPROCS)) {
		JPEG_FreeLibrary();
		QLib_MissingModuleError(QLIB_ERROR_MODULE_MISSING_PROC, "libjpeg", "-nolibjpeg", "jpeg image features");
		return false;
	}

	return true;
}


typedef struct {
  struct jpeg_destination_mgr pub; 
  FILE *outfile;
  JOCTET *buffer;
} my_destination_mgr;

typedef my_destination_mgr *my_dest_ptr;

#define JPEG_OUTPUT_BUF_SIZE  4096

static qboolean jpeg_in_error = false;

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

static void JPEG_IO_set_dest (j_compress_ptr cinfo, FILE *outfile) {
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
	char name[MAX_OSPATH];
	FILE *outfile;
	jpeg_error_mgr_wrapper jerr;
	struct jpeg_compress_struct cinfo;
	JSAMPROW row_pointer[1];

	if (!jpeg_handle)
		return false;

	Q_snprintfz (name, sizeof(name), "%s/%s", com_basedir, filename);	
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

#endif

/************************************ PCX ************************************/

typedef struct pcx_s {
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

byte *Image_LoadPCX (FILE *fin, char *filename, int matchwidth, int matchheight) {
	pcx_t *pcx;
	byte *pcxbuf, *data, *out, *pix;
	int x, y, dataByte, runLength, width, height;

	if (!fin && FS_FOpenFile (filename, &fin) == -1)
		return NULL;

	pcxbuf = Q_Malloc(com_filesize);
	if (fread (pcxbuf, 1, com_filesize, fin) != com_filesize) {
		Com_DPrintf ("Image_LoadPCX: fread() failed on %s\n", COM_SkipPath(filename));
		fclose(fin);
		free(pcxbuf);
		return NULL;
	}
	fclose(fin);


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

	if (pcx->manufacturer != 0x0a || pcx->version != 5 || pcx->encoding != 1 || pcx->bits_per_pixel != 8) {
		Com_DPrintf ("Invalid PCX image %s\n", COM_SkipPath(filename));
		free(pcxbuf);
		return NULL;
	}

	width = pcx->xmax + 1;
	height = pcx->ymax + 1;

	if (width > IMAGE_MAX_DIMENSIONS || height > IMAGE_MAX_DIMENSIONS) {
		Com_DPrintf ("PCX image %s exceeds maximum supported dimensions\n", COM_SkipPath(filename));
		free(pcxbuf);
		return NULL;
	}

	if ((matchwidth && width != matchwidth) || (matchheight && height != matchheight)) {
		free(pcxbuf);
		return NULL;
	}
 
	data = out = Q_Malloc (width * height);

	for (y = 0; y < height; y++, out += width) {
		for (x = 0; x < width; ) {
			if (pix - (byte *) pcx > com_filesize) {
				Com_DPrintf ("Malformed PCX image %s\n", COM_SkipPath(filename));
				free(pcxbuf);
				free(data);
				return NULL;
			}

			dataByte = *pix++;

			if ((dataByte & 0xC0) == 0xC0) {
				runLength = dataByte & 0x3F;
				if (pix - (byte *) pcx > com_filesize) {
					Com_DPrintf ("Malformed PCX image %s\n", COM_SkipPath(filename));
					free(pcxbuf);
					free(data);
					return NULL;
				}
				dataByte = *pix++;
			} else {
				runLength = 1;
			}


			if (runLength + x > width + 1) {
				Com_DPrintf ("Malformed PCX image %s\n", COM_SkipPath(filename));
				free(pcxbuf);
				free(data);
				return NULL;
			}

			while (runLength-- > 0)
				out[x++] = dataByte;
		}
	}

	if (pix - (byte *) pcx > com_filesize) {
		Com_DPrintf ("Malformed PCX image %s\n", COM_SkipPath(filename));
		free(pcxbuf);
		free(data);
		return NULL;
	}

	free(pcxbuf);
	image_width = width;
	image_height = height;
	return data;
}

#ifdef GLQUAKE
int Image_WritePCX (char *filename, byte *data, int width, int height, byte *palette)
#else
int Image_WritePCX (char *filename, byte *data, int width, int height, int rowbytes, byte *palette)
#endif
{
#ifdef GLQUAKE
	int rowbytes = width;
#endif
	int i, j, length;
	byte *pack;
	pcx_t *pcx;

	if (!(pcx = Q_Malloc (width * height * 2 + 1000)))
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

	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
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
	if (!(COM_WriteFile (filename, pcx, length))) {
		free(pcx);
		return false;
	}

	free(pcx);
	return true;
}

/*********************************** INIT ************************************/

void Image_Init(void) {
	Cvar_SetCurrentGroup(CVAR_GROUP_SCREENSHOTS);
#ifdef WITH_PNG
	if (PNG_LoadLibrary())
		QLib_RegisterModule(qlib_libpng, PNG_FreeLibrary);
	Cvar_Register (&image_png_compression_level);
#endif
#ifdef WITH_JPEG
	if (JPEG_LoadLibrary())
		QLib_RegisterModule(qlib_libjpeg, JPEG_FreeLibrary);
	Cvar_Register (&image_jpeg_quality_level);
#endif

	Cvar_ResetCurrentGroup();
}
