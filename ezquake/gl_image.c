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

#include "quakedef.h"
#include "gl_local.h"
#include "image.h"

#ifdef WITH_PNG
#include "png.h"
#endif

#ifdef WITH_JPEG
#include "jpeglib.h"
#endif

int image_width, image_height;

cvar_t	image_png_compression_level = {"image_png_compression_level", "1"};
cvar_t	image_jpeg_quality_level = {"image_jpeg_quality_level", "75"};

extern cvar_t scr_sshot_apply_palette;

/**************************************** UTILS ********************************************/
extern unsigned short	ramps[3][256];

//applies hwgamma to RGB data
void Image_ApplyGamma(byte *buffer, int size) {
	int i;

	if (!vid_hwgamma_enabled || !scr_sshot_apply_palette.value)
		return;

	for (i = 0; i < size; i += 3) {
		buffer[i + 0] = ramps[0][buffer[i + 0]] >> 8;
		buffer[i + 1] = ramps[1][buffer[i + 1]] >> 8;
		buffer[i + 2] = ramps[2][buffer[i + 2]] >> 8;
	}
}
/***************************************** PNG *********************************************/
#ifdef WITH_PNG

byte *Image_LoadPNG (char *filename, int matchwidth, int matchheight) {
	byte header[8], **rowpointers, *data;
	png_structp png;
	png_infop pnginfo;
	int y, width, height, bitdepth, colortype, interlace, compression, filter, bytesperpixel;
	unsigned long rowbytes;
	FILE *fin;

	FS_FOpenFile (filename, &fin);
	if (!fin)
		return NULL;

	fread(header, 1, 8, fin);

	if (png_sig_cmp(header, 0, 8)) {
		Com_Printf ("%s : Invalid PNG file\n", filename);
		fclose(fin);
		return NULL;
	}

	if (!(png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))) {
		fclose(fin);
		return NULL;
	}

	if (!(pnginfo = png_create_info_struct(png))) {
		png_destroy_read_struct(&png, &pnginfo, NULL);
		fclose(fin);
		return NULL;
	}

	if (setjmp(png->jmpbuf)) {
        png_destroy_read_struct(&png, &pnginfo, NULL);
		fclose(fin);
        return NULL;;
    }

	png_init_io(png, fin);
	png_set_sig_bytes(png, 8);
	png_read_info(png, pnginfo);
	png_get_IHDR(png, pnginfo, (png_uint_32 *) &width, (png_uint_32 *) &height, &bitdepth, &colortype, &interlace, &compression, &filter);

	if ((matchwidth && width != matchwidth) || (matchheight && height != matchheight)) {
		png_destroy_read_struct(&png, &pnginfo, NULL);
		fclose(fin);
		return NULL;
	}

	if (colortype == PNG_COLOR_TYPE_PALETTE) {
		png_set_palette_to_rgb(png);
		png_set_filler(png, 255, PNG_FILLER_AFTER);			
	}

	if (colortype == PNG_COLOR_TYPE_GRAY && bitdepth < 8) 
		png_set_gray_1_2_4_to_8(png);
	
	if (png_get_valid( png, pnginfo, PNG_INFO_tRNS ))	
		png_set_tRNS_to_alpha(png); 

	if (bitdepth == 8 && colortype == PNG_COLOR_TYPE_RGB)
		png_set_filler(png, 255, PNG_FILLER_AFTER);
	
	if (colortype == PNG_COLOR_TYPE_GRAY || colortype == PNG_COLOR_TYPE_GRAY_ALPHA) {
		png_set_gray_to_rgb( png );
		png_set_filler(png, 255, PNG_FILLER_AFTER);			
	}

	if (bitdepth < 8)
		png_set_expand (png);
	else if (bitdepth == 16)
		png_set_strip_16(png);


	png_read_update_info( png, pnginfo );
	rowbytes = png_get_rowbytes( png, pnginfo );
	bytesperpixel = png_get_channels( png, pnginfo );
	bitdepth = png_get_bit_depth(png, pnginfo);

	if (bitdepth != 8 || bytesperpixel != 4) {	
		Com_Printf ("%s : Bad PNG color depth and/or bpp\n", filename);
		fclose(fin);
		png_destroy_read_struct(&png, &pnginfo, NULL);
		return NULL;
	}

	data = Q_Malloc(height * rowbytes );
	rowpointers = Q_Malloc(height * 4);

	for (y = 0; y < height; y++)
		rowpointers[y] = data + y * rowbytes;

	png_read_image(png, rowpointers);
	png_read_end(png, NULL);

	png_destroy_read_struct(&png, &pnginfo, NULL);
	free(rowpointers);
	fclose(fin);
	image_width = width;
	image_height = height;
	return data;
}

int Image_WritePNG (char *filename, int compression, byte *pixels, int width, int height) {
	char name[MAX_OSPATH];
	int i;
	FILE *fp;
	png_structp png_ptr;
	png_infop info_ptr;
	png_byte **row_pointers;
	Q_snprintfz (name, sizeof(name), "%s/%s", com_basedir, filename);
	
	if (!(fp = fopen (name, "wb"))) {
		COM_CreatePath (name);
		if (!(fp = fopen (name, "wb")))
			return false;
	}

    if (!(png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))) {
		fclose(fp);
			return false;
	}

    if (!(info_ptr = png_create_info_struct(png_ptr))) {
		png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
		fclose(fp);
		return false;
    }

	if (setjmp(png_ptr->jmpbuf)) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
		return false;
    }

	png_init_io(png_ptr, fp);
	png_set_compression_level(png_ptr, bound(Z_NO_COMPRESSION, compression, Z_BEST_COMPRESSION));

	png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);	
	png_write_info(png_ptr, info_ptr);

	row_pointers = Q_Malloc (4 * height);
	for (i = 0; i < height; i++)
		row_pointers[height - i - 1] = pixels + i * width * 3;
	png_write_image(png_ptr, row_pointers);
	png_write_end(png_ptr, info_ptr);
	free(row_pointers);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(fp);
	return true;
}

#endif
/***************************************** TGA *********************************************/

#define TGA_MAXCOLORS 16384

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

typedef struct _TargaHeader {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;

static int fgetLittleShort (FILE *f) {
	byte b1, b2;

	b1 = fgetc(f);
	b2 = fgetc(f);

	return (short) (b1 + (b2 << 8));
}

static int fgetLittleLong (FILE *f) {
	byte b1, b2, b3, b4;

	b1 = fgetc(f);
	b2 = fgetc(f);
	b3 = fgetc(f);
	b4 = fgetc(f);

	return b1 + (b2 << 8) + (b3 << 16) + (b4 << 24);
}

byte *Image_LoadTGA (char *name, int matchwidth, int matchheight) {
	FILE		*fin;
	int			w, h, x, y, realrow, truerow, baserow, i, temp1, temp2, pixel_size, map_idx;
	int			RLE_count, RLE_flag, size, interleave, origin;
	qboolean	mapped, rlencoded;
	byte		*data, *dst, r, g, b, a, j, k, l, *ColorMap;
	TargaHeader header;

	/* load file */
	FS_FOpenFile (name, &fin);
	if (!fin)
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

	if (header.id_length != 0)
		fseek(fin, header.id_length, SEEK_CUR); 

	if (matchwidth && header.width != matchwidth) {
		fclose(fin);
		return NULL;
	} else if (matchheight && header.height != matchheight) {
		fclose(fin);
		return NULL;
	}

	/* validate TGA type */
	switch (header.image_type) {		
		case TGA_Map: case TGA_RGB: case TGA_Mono: case TGA_RLEMap: case TGA_RLERGB: case TGA_RLEMono:
			break;
		Com_Printf ("%s : Only type 1 (map), 2 (RGB), 3 (mono), 9 (RLEmap), 10 (RLERGB), 11 (RLEmono) TGA images supported\n", name);
		fclose(fin);
		return NULL;
	}

	/* validate color depth */
	switch (header.pixel_size) {
		case 8:	case 15: case 16: case 24: case 32:
			break;
		default:
			fclose(fin);
			Com_Printf ("%s : Only 8, 15, 16, 24 or 32 bit images (with colormaps) supported\n", name);
			return NULL;
	}

	r = g = b = a = l = 0;

	/* if required, read the color map information. */
	ColorMap = NULL;
	mapped = ( header.image_type == TGA_Map || header.image_type == TGA_RLEMap) && header.colormap_type == 1;
	if ( mapped ) {
		/* validate colormap size */
		switch( header.colormap_size ) {
			case 8:	case 15: case 16: case 32: case 24:
				break;
			default:
				fclose(fin);
				Com_Printf ("%s : Only 8, 15, 16, 24 or 32 bit colormaps supported\n", name);
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
			/* read appropriate number of bytes, break into rgb & put in map. */
			switch( header.colormap_size ) {
				case 8:	/* grey scale, read and triplicate. */
					r = g = b = getc(fin);
					a = 255;
					break;
				case 15:	/* 5 bits each of red green and blue. */
							/* watch byte order. */
					j = getc(fin);
					k = getc(fin);
					l = ((unsigned int) k << 8) + j;
					r = (byte) ( ((k & 0x7C) >> 2) << 3 );
					g = (byte) ( (((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3 );
					b = (byte) ( (j & 0x1F) << 3 );
					a = 255;
					break;
				case 16:	/* 5 bits each of red green and blue, 1 alpha bit. */
							/* watch byte order. */
					j = getc(fin);
					k = getc(fin);
					l = ((unsigned int) k << 8) + j;
					r = (byte) ( ((k & 0x7C) >> 2) << 3 );
					g = (byte) ( (((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3 );
					b = (byte) ( (j & 0x1F) << 3 );
					a = (k & 0x80) ? 255 : 0;
					break;
				case 24:	/* 8 bits each of blue, green and red. */
					b = getc(fin);
					g = getc(fin);
					r = getc(fin);
					a = 255;
					l = 0;
					break;
				case 32:	/* 8 bits each of blue, green, red and alpha. */
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

	/* check run-length encoding. */
	rlencoded = (header.image_type == TGA_RLEMap || header.image_type == TGA_RLERGB || header.image_type == TGA_RLEMono);
	RLE_count = RLE_flag = 0;

	image_width = w = header.width;
	image_height = h = header.height;

	size = w * h * 4;
	data = Q_Calloc(size, 1);

	/* read the Targa file body and convert to portable format. */
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
			/* check if run length encoded. */
			if( rlencoded ) {
				if( !RLE_count ) {
					/* have to restart run. */
					i = getc(fin);
					RLE_flag = (i & 0x80);
					if( !RLE_flag ) {
						/* stream of unencoded pixels. */
						RLE_count = i + 1;
					} else {
						/* single pixel replicated. */
						RLE_count = i - 127;
					}
					/* decrement count & get pixel. */
					--RLE_count;
				} else {
					/* have already read count & (at least) first pixel. */
					--RLE_count;
					if( RLE_flag )
						/* replicated pixels. */
						goto PixEncode;
				}
			}

			/* read appropriate number of bytes, break into RGB. */
			switch (pixel_size) {
				case 8:	/* grey scale, read and triplicate. */
					r = g = b = l = getc(fin);
					a = 255;
					break;
				case 15:	/* 5 bits each of red green and blue. */
							/* watch byte order. */
					j = getc(fin);
					k = getc(fin);
					l = ((unsigned int) k << 8) + j;
					r = (byte) ( ((k & 0x7C) >> 2) << 3 );
					g = (byte) ( (((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3 );
					b = (byte) ( (j & 0x1F) << 3 );
					a = 255;
					break;
				case 16:	/* 5 bits each of red green and blue, 1 alpha bit. */
							/* watch byte order. */
					j = getc(fin);
					k = getc(fin);
					l = ((unsigned int) k << 8) + j;
					r = (byte) ( ((k & 0x7C) >> 2) << 3 );
					g = (byte) ( (((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3 );
					b = (byte) ( (j & 0x1F) << 3 );
					a = (k & 0x80) ? 255 : 0;
					break;
				case 24:	/* 8 bits each of blue, green and red. */
					b = getc(fin);
					g = getc(fin);
					r = getc(fin);
					a = 255;
					l = 0;
					break;
				case 32:	/* 8 bits each of blue, green, red and alpha. */
					b = getc(fin);
					g = getc(fin);
					r = getc(fin);
					a = getc(fin);
					l = 0;
					break;
				default:
					fclose(fin);
					Com_Printf ("%s : Illegal pixel_size '%d'\n", name, pixel_size);
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

//writes RGB data to type 2 TGA.
int Image_WriteTGA (char *filename, byte *pixels, int width, int height) {
	byte	*buffer;
	int		i, size, temp;
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


	for (i = 18 ; i < size + 18 ; i += 3)	{
		temp = buffer[i];
		buffer[i] = buffer[i + 2];
		buffer[i + 2] = temp;
	}
	if (!(COM_WriteFile (filename, buffer, size + 18)))
		retval = false;
	free (buffer);
	return retval;
}

/********************************************* JPEG *********************************************/
#ifdef WITH_JPEG

typedef struct my_error_mgr {	
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
} jpeg_error_mgr_wrapper;

void jpeg_error_exit (j_common_ptr cinfo) {	
  longjmp(((jpeg_error_mgr_wrapper *) cinfo->err)->setjmp_buffer, 1);
}

//writes RGB data to jpeg.  quality is 0..100, filename is relative to basedir
int Image_WriteJPEG(char *filename, int quality, byte *pixels, int width, int height) {
	char	name[MAX_OSPATH];
	FILE	*outfile;
	jpeg_error_mgr_wrapper jerr;
	struct jpeg_compress_struct cinfo;
	JSAMPROW row_pointer[1];

	Q_snprintfz (name, sizeof(name), "%s/%s", com_basedir, filename);	
	if (!(outfile = fopen (name, "wb"))) {
		COM_CreatePath (name);
		if (!(outfile = fopen (name, "wb")))
			return false;
	}

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = jpeg_error_exit;
	if (setjmp(jerr.setjmp_buffer)) {
		fclose(outfile);
		return false;
	}
	jpeg_create_compress(&cinfo);

	jpeg_stdio_dest(&cinfo, outfile);
	cinfo.image_width = width; 	
	cinfo.image_height = height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality (&cinfo, bound(0, quality, 100), true);
	jpeg_start_compress(&cinfo, true);

	while (cinfo.next_scanline < cinfo.image_height) {
	    *row_pointer = &pixels[(cinfo.image_height - cinfo.next_scanline - 1) * cinfo.image_width * 3];
	    jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_compress(&cinfo);
	fclose(outfile);
	jpeg_destroy_compress(&cinfo);
	return true;
}
#endif

/****************************************** INIT *******************************************/

void GL_Image_Init(void) {
	Cvar_SetCurrentGroup(CVAR_GROUP_SCREENSHOTS);
	Cvar_Register (&image_png_compression_level);
	Cvar_Register (&image_jpeg_quality_level);

	Cvar_ResetCurrentGroup();
}