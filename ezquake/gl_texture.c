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
#include "crc.h"
#include "image.h"
#include "gl_local.h"


qboolean OnChange_gl_max_size (cvar_t *var, char *string);
qboolean OnChange_gl_texturemode (cvar_t *var, char *string);
qboolean OnChange_gl_miptexLevel (cvar_t *var, char *string);


static qboolean no24bit, forceTextureReload;


extern unsigned d_8to24table2[256];
extern byte vid_gamma_table[256];
extern float vid_gamma;


int texture_extension_number = 1;
int	gl_max_size_default;		
int	gl_lightmap_format = 3, gl_solid_format = 3, gl_alpha_format = 4;


cvar_t	gl_max_size			= {"gl_max_size", "1024", 0, OnChange_gl_max_size};
cvar_t	gl_picmip			= {"gl_picmip", "0"};
cvar_t	gl_miptexLevel		= {"gl_miptexLevel", "0", 0, OnChange_gl_miptexLevel};
cvar_t	gl_lerpimages		= {"gl_lerpimages", "1"};
cvar_t	gl_texturemode		= {"gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST", 0, OnChange_gl_texturemode};

cvar_t	gl_scaleModelTextures		= {"gl_scaleModelTextures", "0"};
cvar_t	gl_scaleTurbTextures		= {"gl_scaleTurbTextures", "1"};
cvar_t	gl_externalTextures_world	= {"gl_externalTextures_world", "1"};
cvar_t	gl_externalTextures_bmodels	= {"gl_externalTextures_bmodels", "1"};

typedef struct {
	int			texnum;
	char		identifier[MAX_QPATH];
	char		*pathname;
	int			width, height;
	int			scaled_width, scaled_height;
	int			texmode;
	unsigned	crc;
	int			bpp;
} gltexture_t;

gltexture_t	gltextures[MAX_GLTEXTURES];
int	numgltextures;

#define Q_ROUND_POWER2(in, out) {						\
	_mathlib_temp_int1 = in;							\
	for (out = 1; out < _mathlib_temp_int1; out <<= 1)	\
	;												\
}

qboolean OnChange_gl_max_size (cvar_t *var, char *string) {
	int i;
	float newvalue = Q_atof(string);

	if (newvalue > gl_max_size_default) {
		Com_Printf("Your hardware doesn't support texture sizes bigger than %dx%d\n", gl_max_size_default, gl_max_size_default);
		return true;
	}

	Q_ROUND_POWER2(newvalue, i);

	if (i != newvalue) {
		Com_Printf("Valid values for %s are powers of 2 only\n", var->name);
		return true;
	}

	return false;
}

typedef struct {
	char *name;
	int	minimize, maximize;
} glmode_t;

glmode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

#define GLMODE_NUMODES	(sizeof(modes) / sizeof(glmode_t))

static int gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
static int gl_filter_max = GL_LINEAR;

qboolean OnChange_gl_texturemode (cvar_t *var, char *string) {
	int i;
	gltexture_t	*glt;

	for (i = 0; i < GLMODE_NUMODES; i++) {
		if (!Q_strcasecmp (modes[i].name, string ) )
			break;
	}
	if (i == GLMODE_NUMODES) {
		Com_Printf ("bad filter name: %s\n", string);
		return true;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;


	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++) {
		if (glt->texmode & TEX_MIPMAP) {
			GL_Bind (glt->texnum);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}
	return false;
}

qboolean OnChange_gl_miptexLevel (cvar_t *var, char *string) {
	float newval = Q_atof(string);

	if (newval != 0 && newval != 1 && newval != 2 && newval != 3) {
		Com_Printf("Valid values for %s are 0,1,2,3 only\n", var->name);
		return true;
	}
	return false;
}

int currenttexture = -1;			

void GL_Bind (int texnum) {
	if (currenttexture == texnum)
		return;

	currenttexture = texnum;
	glBindTexture (GL_TEXTURE_2D, texnum);
}

static GLenum oldtarget = GL_TEXTURE0_ARB;
static int cnttextures[4] = {-1, -1, -1, -1};   
static qboolean mtexenabled = false;


void GL_SelectTexture (GLenum target) {
	if (target == oldtarget) 
		return;

	qglActiveTexture (target);

	cnttextures[oldtarget - GL_TEXTURE0_ARB] = currenttexture;
	currenttexture = cnttextures[target - GL_TEXTURE0_ARB];
	oldtarget = target;
}

void GL_DisableMultitexture (void) {
	if (mtexenabled) {
		glDisable (GL_TEXTURE_2D);
		GL_SelectTexture (GL_TEXTURE0_ARB);
		mtexenabled = false;
	}
}

void GL_EnableMultitexture (void) {
	if (gl_mtexable) {
		GL_SelectTexture (GL_TEXTURE1_ARB);
		glEnable (GL_TEXTURE_2D);
		mtexenabled = true;
	}
}

void GL_EnableTMU (GLenum target) {
	GL_SelectTexture(target);
	glEnable(GL_TEXTURE_2D);
}

void GL_DisableTMU(GLenum target) {
	GL_SelectTexture(target);
	glDisable(GL_TEXTURE_2D);
}

static void ScaleDimensions(int width, int height, int *scaled_width, int *scaled_height, int mode) {
	int maxsize, picmip;
	qboolean scale;

	scale = (mode & TEX_MIPMAP) && !(mode & TEX_NOSCALE);

	Q_ROUND_POWER2(width, *scaled_width);
	Q_ROUND_POWER2(height, *scaled_height);

	if (scale) {
		picmip = (int) bound(0, gl_picmip.value, 16);
		*scaled_width >>= picmip;
		*scaled_height >>= picmip;		
	}

	maxsize = scale ? gl_max_size.value : gl_max_size_default;

	*scaled_width = bound(1, *scaled_width, maxsize);
	*scaled_height = bound(1, *scaled_height, maxsize);
}

void GL_Upload32 (unsigned *data, int width, int height, int mode) {
	int	internal_format, tempwidth, tempheight, miplevel;
	unsigned int *newdata;

	Q_ROUND_POWER2(width, tempwidth);
	Q_ROUND_POWER2(height, tempheight);

	newdata = Q_Malloc(tempwidth * tempheight * 4);
	if (width < tempwidth || height < tempheight) {
		Image_Resample (data, width, height, newdata, tempwidth, tempheight, 4, !!gl_lerpimages.value);
		width = tempwidth;
		height = tempheight;
	} else {
		memcpy (newdata, data, width * height * 4);
	}

	ScaleDimensions(width, height, &tempwidth, &tempheight, mode);

	while (width > tempwidth || height > tempheight)
		Image_MipReduce ((byte *) newdata, (byte *) newdata, &width, &height, 4);

	if (mode & TEX_NOCOMPRESS)
		internal_format = (mode & TEX_ALPHA) ? 4 : 3;
	else
		internal_format = (mode & TEX_ALPHA) ? gl_alpha_format : gl_solid_format;

	miplevel = 0;
	glTexImage2D (GL_TEXTURE_2D, 0, internal_format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, newdata);
	if (mode & TEX_MIPMAP) {
		while (width > 1 || height > 1) {
			Image_MipReduce ((byte *) newdata, (byte *) newdata, &width, &height, 4);
			miplevel++;
			glTexImage2D (GL_TEXTURE_2D, miplevel, internal_format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, newdata);
		}
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	} else {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}

	free(newdata);
}

void GL_Upload8 (byte *data, int width, int height, int mode) {
	static unsigned trans[640 * 480];
	int	i, image_size, p;
	unsigned *table;

	table = (mode & TEX_BRIGHTEN) ? d_8to24table2 : d_8to24table;
	image_size = width * height;

	if (image_size * 4 > sizeof(trans))
		Sys_Error ("GL_Upload8: image too big");

	if (mode & TEX_FULLBRIGHT) {
		mode |= TEX_ALPHA;
		for (i = 0; i < image_size; i++) {
			p = data[i];
			if (p < 224)
				trans[i] = table[p] & 0x00FFFFFF;	
			else
				trans[i] = table[p];			
		}
	} else if (mode & TEX_ALPHA) {

		mode &= ~TEX_ALPHA;
		for (i = 0; i < image_size; i++) {
			if ((p = data[i]) == 255)
				mode |= TEX_ALPHA;
			trans[i] = table[p];
		}
	} else {
		if (image_size & 3)
			Sys_Error ("GL_Upload8: image_size & 3");
		for (i = 0; i < image_size; i += 4) {
			trans[i] = table[data[i]];
			trans[i + 1] = table[data[i + 1]];
			trans[i + 2] = table[data[i + 2]];
			trans[i + 3] = table[data[i + 3]];
		}
	}
	GL_Upload32 (trans, width, height, mode);
}

int GL_LoadTexture (char *identifier, int width, int height, byte *data, int mode, int bpp) {
	int	i, scaled_width, scaled_height, crc = 0;
	gltexture_t	*glt;

	ScaleDimensions(width, height, &scaled_width, &scaled_height, mode);


	if (identifier[0]) {
		crc = CRC_Block (data, width * height * bpp);
		for (i = 0, glt = gltextures; i < numgltextures; i++, glt++) {
			if (!strncmp (identifier, glt->identifier, sizeof(glt->identifier) - 1)) {
				if (
					width == glt->width && height == glt->height &&
					scaled_width == glt->scaled_width && scaled_height == glt->scaled_height &&
					crc == glt->crc && glt->bpp == bpp &&
					(mode & ~(TEX_COMPLAIN|TEX_NOSCALE)) == (glt->texmode & ~(TEX_COMPLAIN|TEX_NOSCALE))
				) {
					GL_Bind(gltextures[i].texnum);
					return gltextures[i].texnum;	
				} else {
					goto setup_gltexture;	
				}
			}
		}
	}

	if (numgltextures == MAX_GLTEXTURES)
		Sys_Error ("GL_LoadTexture: numgltextures == MAX_GLTEXTURES");

	glt = &gltextures[numgltextures];
	numgltextures++;

	Q_strncpyz (glt->identifier, identifier, sizeof(glt->identifier));
	glt->texnum = texture_extension_number;
	texture_extension_number++;

setup_gltexture:
	glt->width = width;
	glt->height = height;
	glt->scaled_width = scaled_width;
	glt->scaled_height = scaled_height;
	glt->texmode = mode;
	glt->crc = crc;
	glt->bpp = bpp;
	if (glt->pathname) {
		Z_Free(glt->pathname);
		glt->pathname = NULL;
	}
	if (bpp == 4 && com_netpath[0])	
		glt->pathname = CopyString(com_netpath);

	GL_Bind(glt->texnum);

	switch (bpp) {
	case 1:
		GL_Upload8 (data, width, height, mode); break;
	case 4:
		GL_Upload32 ((void *) data, width, height, mode); break;
	default:
		Sys_Error("GL_LoadTexture: unknown bpp\n"); break;
	}

	return glt->texnum;
}

int GL_LoadPicTexture (char *name, mpic_t *pic, byte *data) {
	int glwidth, glheight, i;
	char fullname[MAX_QPATH] = "pic:";
	byte *src, *dest, *buf;

	Q_ROUND_POWER2(pic->width, glwidth);
	Q_ROUND_POWER2(pic->height, glheight);

	Q_strncpyz (fullname + 4, name, sizeof(fullname) - 4);
	if (glwidth == pic->width && glheight == pic->height) {
		pic->texnum = GL_LoadTexture (fullname, glwidth, glheight, data, TEX_ALPHA, 1);
		pic->sl = 0;
		pic->sh = 1;
		pic->tl = 0;
		pic->th = 1;
	} else {
		buf = Q_Calloc (glwidth * glheight, 1);

		src = data;
		dest = buf;
		for (i = 0; i < pic->height; i++) {
			memcpy (dest, src, pic->width);
			src += pic->width;
			dest += glwidth;
		}
		pic->texnum = GL_LoadTexture (fullname, glwidth, glheight, buf, TEX_ALPHA, 1);
		pic->sl = 0;
		pic->sh = (float) pic->width / glwidth;
		pic->tl = 0;
		pic->th = (float) pic->height / glheight;
		free (buf);
	}

	return pic->texnum;
}

gltexture_t *GL_FindTexture (char *identifier) {
	int i;

	for (i = 0; i < numgltextures; i++) {
		if (!strcmp (identifier, gltextures[i].identifier))
			return &gltextures[i];
	}

	return NULL;
}

static gltexture_t *current_texture = NULL;

#define CHECK_TEXTURE_ALREADY_LOADED	\
	if (CheckTextureLoaded(mode)) {		\
		current_texture = NULL;			\
		fclose(f);						\
		return NULL;					\
	}

static qboolean CheckTextureLoaded(int mode) {
	int scaled_width, scaled_height;

	if (!forceTextureReload) {
		if (current_texture && current_texture->pathname && !strcmp(com_netpath, current_texture->pathname)) {

			ScaleDimensions(current_texture->width, current_texture->height, &scaled_width, &scaled_height, mode);
			if (current_texture->scaled_width == scaled_width && current_texture->scaled_height == scaled_height)
				return true;
		}
	}
	return false;
}

byte *GL_LoadImagePixels (char *filename, int matchwidth, int matchheight, int mode) {
	char basename[MAX_QPATH], name[MAX_QPATH];
	byte *c, *data;
	FILE *f;

	COM_StripExtension(filename, basename);
	for (c = basename; *c; c++)
		if (*c == '*')
			*c = '#';

	Q_snprintfz (name, sizeof(name), "%s.tga", basename);
	if (FS_FOpenFile (name, &f) != -1) {
		CHECK_TEXTURE_ALREADY_LOADED;
		if ((data = Image_LoadTGA (f, name, matchwidth, matchheight)))
			return data;
	}

#ifdef WITH_PNG
	Q_snprintfz (name, sizeof(name), "%s.png", basename);
	if (FS_FOpenFile (name, &f) != -1) {
		CHECK_TEXTURE_ALREADY_LOADED;
		if ((data = Image_LoadPNG (f, name, matchwidth, matchheight)))
			return data;
	}
#endif

	if (mode & TEX_COMPLAIN) {
		if (!no24bit)
			Com_Printf ("Couldn't load %s image\n", COM_SkipPath(filename));
	}

	return NULL;
}

int GL_LoadTexturePixels (byte *data, char *identifier, int width, int height, int mode) {
	int i, j, image_size;
	qboolean gamma;

	image_size = width * height;
	gamma = (vid_gamma != 1);

	if (mode & TEX_LUMA) {
		gamma = false;
	} else if (mode & TEX_ALPHA) {
		mode &= ~TEX_ALPHA;
		for (j = 0; j < image_size; j++) {
			if ( ( (((unsigned *) data)[j] >> 24 ) & 0xFF ) < 255 ) {
				mode |= TEX_ALPHA;
				break;
			}
		}
	}

	if (gamma) {
		for (i = 0; i < image_size; i++){
			data[4 * i] = vid_gamma_table[data[4 * i]];
			data[4 * i + 1] = vid_gamma_table[data[4 * i + 1]];
			data[4 * i + 2] = vid_gamma_table[data[4 * i + 2]];
		}
	}

	return GL_LoadTexture (identifier, width, height, data, mode, 4);
}

int GL_LoadTextureImage (char *filename, char *identifier, int matchwidth, int matchheight, int mode) {
	int texnum;
	byte *data;
	gltexture_t *gltexture;

	if (no24bit)
		return 0;

	if (!identifier)
		identifier = filename;

	gltexture = current_texture = GL_FindTexture(identifier);

	if (!(data = GL_LoadImagePixels (filename, matchwidth, matchheight, mode))) {
		texnum =  (gltexture && !current_texture) ? gltexture->texnum : 0;
	} else {
		texnum = GL_LoadTexturePixels(data, identifier, image_width, image_height, mode);
		free(data);
	}

	current_texture = NULL;
	return texnum;
}

mpic_t *GL_LoadPicImage (char *filename, char *id, int matchwidth, int matchheight, int mode) {
	int width, height, i;
	char identifier[MAX_QPATH] = "pic:";
	byte *data, *src, *dest, *buf;
	static mpic_t pic;

	if (no24bit)
		return NULL;

	if (!(data = GL_LoadImagePixels (filename, matchwidth, matchheight, 0)))
		return NULL;

	pic.width = image_width;
	pic.height = image_height;

	if (mode & TEX_ALPHA) {
		mode &= ~TEX_ALPHA;
		for (i = 0; i < image_width * image_height; i++) {
			if ( ( (((unsigned *) data)[i] >> 24 ) & 0xFF ) < 255) {
				mode |= TEX_ALPHA;
				break;
			}
		}
	}

	Q_ROUND_POWER2(pic.width, width);
	Q_ROUND_POWER2(pic.height, height);

	Q_strncpyz (identifier + 4, id ? id : filename, sizeof(identifier) - 4);
	if (width == pic.width && height == pic.height) {
		pic.texnum = GL_LoadTexture (identifier, pic.width, pic.height, data, mode, 4);
		pic.sl = 0;
		pic.sh = 1;
		pic.tl = 0;
		pic.th = 1;
	} else {
		buf = Q_Calloc (width * height, 4);

		src = data;
		dest = buf;
		for (i = 0; i < pic.height; i++) {
			memcpy (dest, src, pic.width * 4);
			src += pic.width * 4;
			dest += width * 4;
		}
		pic.texnum = GL_LoadTexture (identifier, width, height, buf, mode & ~TEX_MIPMAP, 4);
		pic.sl = 0;
		pic.sh = (float) pic.width / width;
		pic.tl = 0;
		pic.th = (float) pic.height / height;
		free (buf);
	}

	free(data);
	return &pic;
}

int GL_LoadCharsetImage (char *filename, char *identifier) {
	int i, texnum, image_size;
	byte *data, *buf, *dest, *src;

	if (no24bit)
		return 0;

	if (!(data = GL_LoadImagePixels (filename, 0, 0, 0)))
		return 0;

	if (!identifier)
		identifier = filename;

	image_size = image_width * image_height;

	buf = dest = Q_Calloc(image_size * 2, 4); 
	src = data;
	for (i = 0 ; i < 16 ; i++) {
		memcpy (dest, src, image_size >> 2);
		src += image_size >> 2;
		dest += image_size >> 1;
	}

	texnum = GL_LoadTexture (identifier, image_width, image_height * 2, buf, TEX_ALPHA | TEX_NOCOMPRESS, 4);

	free(buf);
	free(data);
	return texnum;
}

void GL_Texture_Init(void) {
	Cvar_SetCurrentGroup(CVAR_GROUP_TEXTURES);
	Cvar_Register(&gl_max_size);
	Cvar_Register(&gl_picmip);
	Cvar_Register(&gl_lerpimages);
	Cvar_Register(&gl_texturemode);
	Cvar_Register(&gl_scaleModelTextures);
	Cvar_Register(&gl_scaleTurbTextures);
	Cvar_Register(&gl_miptexLevel);
	Cvar_Register(&gl_externalTextures_world);
	Cvar_Register(&gl_externalTextures_bmodels);

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &gl_max_size_default);
	Cvar_SetDefault(&gl_max_size, gl_max_size_default);

	no24bit = COM_CheckParm("-no24bit") ? true : false;
	forceTextureReload = COM_CheckParm("-forceTextureReload") ? true : false;
}
