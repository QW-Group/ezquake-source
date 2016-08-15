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

$Id: gl_texture.c,v 1.44 2007-10-05 19:06:24 johnnycz Exp $
*/

#include "quakedef.h"
#include "crc.h"
#include "image.h"
#include "gl_model.h"
#include "gl_local.h"


void OnChange_gl_max_size (cvar_t *var, char *string, qbool *cancel);
void OnChange_gl_texturemode (cvar_t *var, char *string, qbool *cancel);
void OnChange_gl_miptexLevel (cvar_t *var, char *string, qbool *cancel);
void OnChange_gl_anisotropy (cvar_t *var, char *string, qbool *cancel);


static qbool no24bit, forceTextureReload;


extern unsigned d_8to24table2[256];
extern byte vid_gamma_table[256];
extern float vid_gamma;


extern int anisotropy_ext;
int	anisotropy_tap = 1; //  1 - is off
int	gl_max_size_default;
int	gl_lightmap_format = 3, gl_solid_format = 3, gl_alpha_format = 4;

cvar_t	gl_max_size			= {"gl_max_size", "2048", 0, OnChange_gl_max_size};
cvar_t	gl_picmip			= {"gl_picmip", "0"};
cvar_t	gl_miptexLevel		= {"gl_miptexLevel", "0", 0, OnChange_gl_miptexLevel};
cvar_t	gl_lerpimages		= {"gl_lerpimages", "1"};
cvar_t	gl_texturemode		= {"gl_texturemode", "GL_LINEAR_MIPMAP_LINEAR", 0, OnChange_gl_texturemode};
cvar_t	gl_texturemode2d	= {"gl_texturemode2d", "GL_LINEAR", 0, OnChange_gl_texturemode};
cvar_t	gl_anisotropy		= {"gl_anisotropy","1", 0, OnChange_gl_anisotropy};

cvar_t	gl_scaleModelTextures		= {"gl_scaleModelTextures", "0"};
cvar_t	gl_scaleTurbTextures		= {"gl_scaleTurbTextures", "1"};
cvar_t	gl_externalTextures_world	= {"gl_externalTextures_world", "1"};
cvar_t	gl_externalTextures_bmodels	= {"gl_externalTextures_bmodels", "1"};
cvar_t  gl_no24bit                  = {"gl_no24bit", "0", CVAR_LATCH};

cvar_t  gl_wicked_luma_level        = {"gl_luma_level", "1", CVAR_LATCH};

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

static gltexture_t	gltextures[MAX_GLTEXTURES];
static int			numgltextures = 0;
	   int			texture_extension_number = 1; // non static, sad but used in gl_framebufer.c too

void OnChange_gl_max_size (cvar_t *var, char *string, qbool *cancel) 
{
	int i;
	float newvalue = Q_atof(string);

	if (newvalue > gl_max_size_default) 
	{
		Com_Printf("Your hardware doesn't support texture sizes bigger than %dx%d\n", gl_max_size_default, gl_max_size_default);
		*cancel = true;
		return;
	}

	Q_ROUND_POWER2(newvalue, i);

	if (i != newvalue) 
	{
		Com_Printf("Valid values for %s are powers of 2 only\n", var->name);
		*cancel = true;
		return;
	}
}

typedef struct 
{
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
static int gl_filter_max_2d = GL_LINEAR;

void OnChange_gl_texturemode (cvar_t *var, char *string, qbool *cancel) 
{
	int i, filter_min, filter_max;
	qbool mipmap;
	gltexture_t	*glt;

	for (i = 0; i < GLMODE_NUMODES; i++) 
	{
		if (!strcasecmp (modes[i].name, string))
			break;
	}

	if (i == GLMODE_NUMODES) 
	{
		Com_Printf ("bad filter name: %s\n", string);
		*cancel = true;
		return;
	}

	if (var == &gl_texturemode)
	{
		gl_filter_min = filter_min = modes[i].minimize;
		gl_filter_max = filter_max = modes[i].maximize;
		mipmap = true;
	}
	else if (var == &gl_texturemode2d)
	{
		gl_filter_max_2d = filter_min = filter_max = modes[i].maximize;
		mipmap = false;
	}
	else
	{
		Sys_Error("OnChange_gl_texturemode: unexpected cvar!");
		return;
	}

	// Make sure we set the proper texture filters for textures.
	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++)
	{
		if (glt->texmode & TEX_NO_TEXTUREMODE)
			continue;	// This texture must NOT be affected by texture mode changes,
						// for example charset which rather controlled by gl_smoothfont.

		// true == true or false == false
		if ( mipmap == !!(glt->texmode & TEX_MIPMAP) )
		{
			if (developer.integer > 100)
				Com_DPrintf("texturemode: %s\n", glt->identifier);
			GL_Bind (glt->texnum);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter_min);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_max);
		}
	}
}

void OnChange_gl_anisotropy (cvar_t *var, char *string, qbool *cancel) 
{
	int i;
	gltexture_t *glt;

	int newvalue = Q_atoi(string);

	anisotropy_tap = max(1, newvalue); // 0 is bad, 1 is off, 2 and higher are valid modes

	if (!anisotropy_ext)
		return; // we doesn't have such extension

	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++)
	{
		if (glt->texmode & TEX_NO_TEXTUREMODE)
			continue;	// This texture must NOT be affected by texture mode changes,
						// for example charset which rather controlled by gl_smoothfont.

		if (glt->texmode & TEX_MIPMAP)
		{
			GL_Bind (glt->texnum);
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy_tap);
		}
	}
}

void OnChange_gl_miptexLevel (cvar_t *var, char *string, qbool *cancel)
{
	float newval = Q_atof(string);

	if (newval != 0 && newval != 1 && newval != 2 && newval != 3) 
	{
		Com_Printf("Valid values for %s are 0,1,2,3 only\n", var->name);
		*cancel = true;
	}
}

int currenttexture = -1;

void GL_Bind (int texnum)
{
	if (currenttexture == texnum)
		return;

	currenttexture = texnum;
	glBindTexture (GL_TEXTURE_2D, texnum);
}

static GLenum oldtarget = GL_TEXTURE0;
static int cnttextures[4] = {-1, -1, -1, -1};
static qbool mtexenabled = false;

void GL_SelectTexture (GLenum target) 
{
	if (target == oldtarget)
		return;

	qglActiveTexture (target);

	cnttextures[oldtarget - GL_TEXTURE0] = currenttexture;
	currenttexture = cnttextures[target - GL_TEXTURE0];
	oldtarget = target;
}

void GL_DisableMultitexture (void) 
{
	if (mtexenabled) 
	{
		glDisable (GL_TEXTURE_2D);
		GL_SelectTexture (GL_TEXTURE0);
		mtexenabled = false;
	}
}

void GL_EnableMultitexture (void) 
{
	if (gl_mtexable) 
	{
		GL_SelectTexture (GL_TEXTURE1);
		glEnable (GL_TEXTURE_2D);
		mtexenabled = true;
	}
}

void GL_EnableTMU (GLenum target) 
{
	GL_SelectTexture(target);
	glEnable(GL_TEXTURE_2D);
}

void GL_DisableTMU(GLenum target) 
{
	GL_SelectTexture(target);
	glDisable(GL_TEXTURE_2D);
}

static void ScaleDimensions(int width, int height, int *scaled_width, int *scaled_height, int mode) 
{
	int maxsize, picmip;
	qbool scale = (mode & TEX_MIPMAP) && !(mode & TEX_NOSCALE);

	if (gl_support_arb_texture_non_power_of_two)
	{
		*scaled_width = width;
		*scaled_height = height;
	}
	else
	{
		Q_ROUND_POWER2(width, *scaled_width);
		Q_ROUND_POWER2(height, *scaled_height);
	}

	if (scale) 
	{
		picmip = (int) bound(0, gl_picmip.value, 16);
		*scaled_width >>= picmip;
		*scaled_height >>= picmip;
	}

	maxsize = scale ? gl_max_size.value : gl_max_size_default;

	*scaled_width = bound(1, *scaled_width, maxsize);
	*scaled_height = bound(1, *scaled_height, maxsize);
}

static void brighten32 (byte *data, int size)
{
	byte *p;
	int i;

	p = data;
	for (i = 0; i < size/4; i++)
	{
		p[0] = min(p[0] * 2.0/1.5, 255);
		p[1] = min(p[1] * 2.0/1.5, 255);
		p[2] = min(p[2] * 2.0/1.5, 255);
		p += 4;
	}
}

//
// Uploads a 32-bit texture to OpenGL. Makes sure it's the correct size and creates mipmaps if requested.
//
static void GL_Upload32 (unsigned *data, int width, int height, int mode) 
{
	int	internal_format, tempwidth, tempheight, miplevel;
	unsigned int *newdata;

	if (gl_support_arb_texture_non_power_of_two)
	{
		tempwidth = width;
		tempheight = height;
	}
	else
	{
		Q_ROUND_POWER2(width, tempwidth);
		Q_ROUND_POWER2(height, tempheight);
	}

	newdata = (unsigned int *) Q_malloc(tempwidth * tempheight * 4);

	// Resample the image if it's not scaled to the power of 2,
	// we take care of this when drawing using the texture coordinates.
	if (width < tempwidth || height < tempheight) 
	{
		Image_Resample (data, width, height, newdata, tempwidth, tempheight, 4, !!gl_lerpimages.value);
		width = tempwidth;
		height = tempheight;
	} 
	else 
	{
		// Scale is a power of 2, just copy the data.
		memcpy (newdata, data, width * height * 4);
	}

	if ((mode & TEX_FULLBRIGHT) && (mode & TEX_LUMA) && gl_wicked_luma_level.integer > 0)
	{
		int i, cnt = width * height * 4, level = gl_wicked_luma_level.integer;
		byte *bdata = (byte*)newdata;

		for (i = 0; i < cnt; i += 4)
		{
			if (bdata[i] < level && bdata[i+1] < level && bdata[i+2] < level)
				bdata[i+3] = 0; // make black pixels transparent, well not always black, depends of level...
		}
	}

	// Get the scaled dimension (scales according to gl_picmip and max allowed texture size).
	ScaleDimensions(width, height, &tempwidth, &tempheight, mode);

	// If the image size is bigger than the max allowed size or 
	// set picmip value we calculate it's next closest mip map.
	while (width > tempwidth || height > tempheight)
		Image_MipReduce ((byte *) newdata, (byte *) newdata, &width, &height, 4);

	if (mode & TEX_BRIGHTEN)
		brighten32 ((byte *)newdata, width * height * 4);

	if(gl_gammacorrection.integer)
	{
		internal_format = (mode & TEX_ALPHA) ? GL_SRGB8_ALPHA8 : GL_SRGB8;
	}
	else if(mode & TEX_NOCOMPRESS)
	{
		internal_format = (mode & TEX_ALPHA) ? 4 : 3;
	}
	else
	{
		internal_format = (mode & TEX_ALPHA) ? gl_alpha_format : gl_solid_format;
	}

	// Upload the main texture to OpenGL.
	miplevel = 0;
	glTexImage2D (GL_TEXTURE_2D, 0, internal_format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, newdata);

	if (mode & TEX_MIPMAP)
	{
		// Calculate the mip maps for the images.
		while (width > 1 || height > 1)
		{
			Image_MipReduce ((byte *) newdata, (byte *) newdata, &width, &height, 4);
			miplevel++;
			glTexImage2D (GL_TEXTURE_2D, miplevel, internal_format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, newdata);
		}

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

		if (anisotropy_ext)
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy_tap);
	} 
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max_2d);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max_2d);
	}

	Q_free(newdata);
}

static void GL_Upload8 (byte *data, int width, int height, int mode) 
{
	static unsigned trans[640 * 480];
	int	i, image_size, p;
	unsigned *table;

	table = (mode & TEX_BRIGHTEN) ? d_8to24table2 : d_8to24table;
	image_size = width * height;

	if (image_size * 4 > sizeof(trans))
		Sys_Error ("GL_Upload8: image too big");

	if (mode & TEX_FULLBRIGHT) 
	{
		// This is a fullbright mask, so make all non-fullbright colors transparent.
		mode |= TEX_ALPHA;
	
		for (i = 0; i < image_size; i++)
		{
			p = data[i];
			if (p < 224)
				trans[i] = table[p] & LittleLong(0x00FFFFFF); // Transparent.
			else
				trans[i] = (p == 255) ? LittleLong(0xff535b9f) : table[p]; // Fullbright. Disable transparancy on fullbright colors (255).
		}
	} 
	else if (mode & TEX_ALPHA) 
	{
		// If there are no transparent pixels, make it a 3 component
		// texture even if it was specified as otherwise
		mode &= ~TEX_ALPHA;
	
		for (i = 0; i < image_size; i++) 
		{
			if ((p = data[i]) == 255)
				mode |= TEX_ALPHA;
			trans[i] = table[p];
		}
	} 
	else
	{
		if (image_size & 3)
			Sys_Error ("GL_Upload8: image_size & 3");

		// Convert the 8-bit colors to 24-bit.
		for (i = 0; i < image_size; i += 4)
		{
			trans[i] = table[data[i]];
			trans[i + 1] = table[data[i + 1]];
			trans[i + 2] = table[data[i + 2]];
			trans[i + 3] = table[data[i + 3]];
		}
	}

	GL_Upload32 (trans, width, height, mode & ~TEX_BRIGHTEN);
}

int GL_LoadTexture (char *identifier, int width, int height, byte *data, int mode, int bpp) 
{
	int	i, scaled_width, scaled_height;
	unsigned short crc = 0;
	qbool load_over_existing = false;
	gltexture_t *glt = NULL;

	if (lightmode != 2)
		mode &= ~TEX_BRIGHTEN;

	ScaleDimensions(width, height, &scaled_width, &scaled_height, mode);

	if (developer.integer >= 3)
	{
		Com_DPrintf("Texture: %s %dx%d -> %dx%d %s\n",
				identifier, width, height, scaled_width, scaled_height,
				((scaled_width & (scaled_width-1)) || (scaled_height & (scaled_height-1))) ? "non power of two" : "");
	}

	// If we were given an identifier for the texture, search through
	// the list of loaded texture and see if we find a match, if so
	// return the texnum for the already loaded texture.
	if (identifier[0]) 
	{
		crc = CRC_Block (data, width * height * bpp);

		for (i = 0, glt = gltextures; i < numgltextures; i++, glt++) 
		{
			if (!strncmp (identifier, glt->identifier, sizeof(glt->identifier) - 1)) 
			{
				// Identifier matches, make sure everything else is the same
				// so that we can be really sure this is the correct texture.
				if (width == glt->width && height == glt->height &&
					scaled_width == glt->scaled_width && scaled_height == glt->scaled_height &&
					crc == glt->crc && glt->bpp == bpp &&
					(mode & ~(TEX_COMPLAIN | TEX_NOSCALE)) == (glt->texmode & ~(TEX_COMPLAIN | TEX_NOSCALE)))
				{
					GL_Bind(gltextures[i].texnum);
					return gltextures[i].texnum;
				} 
				else 
				{
					// Same identifier but different texture, so overwrite
					// the already loaded texture.
					load_over_existing = true;
					break;
				}
			}
		}
	} 

	if (numgltextures >= MAX_GLTEXTURES)
		Sys_Error ("GL_LoadTexture: numgltextures == MAX_GLTEXTURES");

	// If the identifier was the same as another textures, we won't bother
	// with taking up a new texture slot, just load the new texture
	// over the old one.
	if (!load_over_existing)
	{
		glt = &gltextures[numgltextures];
		numgltextures++;

		strlcpy (glt->identifier, identifier, sizeof(glt->identifier));
		glt->texnum = texture_extension_number;
		texture_extension_number++;
	}

	if (!glt)
		Sys_Error("GL_LoadTexture: glt not initialized\n");

	glt->width			= width;
	glt->height			= height;
	glt->scaled_width	= scaled_width;
	glt->scaled_height	= scaled_height;
	glt->texmode		= mode;
	glt->crc			= crc;
	glt->bpp			= bpp;
	
	Q_free(glt->pathname);
	
	if (bpp == 4 && fs_netpath[0])
		glt->pathname = Q_strdup(fs_netpath);

	// Tell OpenGL the texnum of the texture before uploading it.
	GL_Bind(glt->texnum);

	// Upload the texture to OpenGL based on the bytes per pixel.
	switch (bpp) 
	{
		case 1:
			GL_Upload8 (data, width, height, mode); break;
		case 4:
			GL_Upload32 ((void *) data, width, height, mode); break;
		default:
			Sys_Error("GL_LoadTexture: unknown bpp\n"); break;
	}

	return glt->texnum;
}

int GL_LoadPicTexture (const char *name, mpic_t *pic, byte *data) 
{
	int glwidth, glheight, i;
	char fullname[MAX_QPATH] = "pic:";
	byte *src, *dest, *buf;

	if (gl_support_arb_texture_non_power_of_two)
	{
		glwidth = pic->width;
		glheight = pic->height;
	}
	else
	{
		Q_ROUND_POWER2(pic->width, glwidth);
		Q_ROUND_POWER2(pic->height, glheight);
	}

	strlcpy (fullname + 4, name, sizeof(fullname) - 4);
	if (glwidth == pic->width && glheight == pic->height)
	{
		pic->texnum = GL_LoadTexture (fullname, glwidth, glheight, data, TEX_ALPHA, 1);
		pic->sl = 0;
		pic->sh = 1;
		pic->tl = 0;
		pic->th = 1;
	} 
	else 
	{
		buf = Q_calloc(glwidth * glheight, 1);

		src = data;
		dest = buf;
		for (i = 0; i < pic->height; i++) {
			memcpy(dest, src, pic->width);
			src += pic->width;
			dest += glwidth;
		}
		pic->texnum = GL_LoadTexture(fullname, glwidth, glheight, buf, TEX_ALPHA, 1);
		pic->sl = 0;
		pic->sh = (float) pic->width / glwidth;
		pic->tl = 0;
		pic->th = (float) pic->height / glheight;
		Q_free (buf);
	}

	return pic->texnum;
}

gltexture_t *GL_FindTexture (char *identifier) 
{
	int i;

	for (i = 0; i < numgltextures; i++) 
	{
		if (!strcmp (identifier, gltextures[i].identifier))
			return &gltextures[i];
	}

	return NULL;
}

static gltexture_t *current_texture = NULL;

#define CHECK_TEXTURE_ALREADY_LOADED	\
	if (CheckTextureLoaded(mode)) {		\
		current_texture = NULL;			\
		VFS_CLOSE(f);					\
		return NULL;					\
	}


static qbool CheckTextureLoaded(int mode) 
{
	int scaled_width, scaled_height;

	if (!forceTextureReload) 
	{
		if (current_texture && current_texture->pathname && !strcmp(fs_netpath, current_texture->pathname)) 
		{
			ScaleDimensions(current_texture->width, current_texture->height, &scaled_width, &scaled_height, mode);
			if (current_texture->scaled_width == scaled_width && current_texture->scaled_height == scaled_height)
				return true;
		}
	}
	return false;
}

typedef byte* (*ImageLoadFunction)(vfsfile_t *fin, const char *filename, int matchwidth, int matchheight, int *real_width, int *real_height);
typedef struct image_load_format_s {
	const char* extension;
	ImageLoadFunction function;
	int filter_mask;
} image_load_format_t;

byte *GL_LoadImagePixels (const char *filename, int matchwidth, int matchheight, int mode, int *real_width, int *real_height) 
{
	char basename[MAX_QPATH], name[MAX_QPATH];
	byte *c, *data = NULL;
	vfsfile_t *f;

	COM_StripExtension(filename, basename, sizeof(basename));
	for (c = (byte *) basename; *c; c++)
	{
		if (*c == '*')
			*c = '#';
	}

	snprintf (name, sizeof(name), "%s.link", basename);
	if ((f = FS_OpenVFS(name, "rb", FS_ANY))) 
	{
		char link[128];
		int len;
		VFS_GETS(f, link, sizeof(link));

		len = strlen(link);

		// Strip endline.
		if (link[len-1] == '\n') 
		{
			link[len-1] = '\0';
			--len;
		}

		if (link[len-1] == '\r') 
		{
			link[len-1] = '\0';
			--len;
		}

		snprintf (name, sizeof(name), "textures/%s", link);
		if ((f = FS_OpenVFS(name, "rb", FS_ANY))) 
		{
       		CHECK_TEXTURE_ALREADY_LOADED;
       		if( !data && !strcasecmp(link + len - 3, "tga") )
			{
				data = Image_LoadTGA (f, name, matchwidth, matchheight, real_width, real_height);
			}

			#ifdef WITH_PNG
       		if( !data && !strcasecmp(link + len - 3, "png") )
			{
       			data = Image_LoadPNG (f, name, matchwidth, matchheight, real_width, real_height);
			}
			#endif // WITH_PNG
			
			#ifdef WITH_JPEG
       		if( !data && !strcasecmp(link + len - 3, "jpg") )
			{
				data = Image_LoadJPEG (f, name, matchwidth, matchheight, real_width, real_height);
			}
			#endif // WITH_JPEG

			// TEX_NO_PCX - preventing loading skins here
       		if( !(mode & TEX_NO_PCX) && !data && !strcasecmp(link + len - 3, "pcx") )
			{
				data = Image_LoadPCX_As32Bit (f, name, matchwidth, matchheight, real_width, real_height);
			}

       		if ( data )
				return data;
		}
	}

	image_load_format_t formats[] = {
		{ "tga", Image_LoadTGA, 0 },
#ifdef WITH_PNG
		{ "png", Image_LoadPNG, 0 },
#endif
#ifdef WITH_JPEG
		{ "jpg", Image_LoadJPEG, 0 },
#endif
		{ "pcx", Image_LoadPCX_As32Bit, TEX_NO_PCX }
	};
	int i = 0;

	image_load_format_t* best = NULL;
	for (i = 0; i < sizeof (formats) / sizeof (formats[0]); ++i) {
		vfsfile_t *file = NULL;

		if (mode & formats[i].filter_mask)
			continue;

		snprintf (name, sizeof (name), "%s.%s", basename, formats[i].extension);
		if ((file = FS_OpenVFS (name, "rb", FS_ANY))) {
			if (f == NULL || (f->copyprotected && !file->copyprotected)) {
				if (f) {
					VFS_CLOSE (f);
				}
				f = file;
				best = &formats[i];
			}
			else {
				VFS_CLOSE (file);
			}
		}
	}

	if (best && f) {
		CHECK_TEXTURE_ALREADY_LOADED;
		snprintf (name, sizeof (name), "%s.%s", basename, best->extension);
		if ((data = best->function (f, name, matchwidth, matchheight, real_width, real_height))) {
			return data;
		}
	}

	if (mode & TEX_COMPLAIN) 
	{
		if (!no24bit)
			Com_Printf_State(PRINT_FAIL, "Couldn't load %s image\n", COM_SkipPath(filename));
	}

	return NULL;
}

int GL_LoadTexturePixels (byte *data, char *identifier, int width, int height, int mode) 
{
	int i, j, image_size;
	qbool gamma;

	image_size = width * height;
	gamma = (vid_gamma != 1);

	if (mode & TEX_LUMA)
	{
		gamma = false;
	}
	else if (mode & TEX_ALPHA)
	{
		mode &= ~TEX_ALPHA;

		for (j = 0; j < image_size; j++) 
		{
			if ( ( (((unsigned *) data)[j] >> 24 ) & 0xFF ) < 255 )
			{
				mode |= TEX_ALPHA;
				break;
			}
		}
	}

	if (gamma) 
	{
		for (i = 0; i < image_size; i++)
		{
			data[4 * i] = vid_gamma_table[data[4 * i]];
			data[4 * i + 1] = vid_gamma_table[data[4 * i + 1]];
			data[4 * i + 2] = vid_gamma_table[data[4 * i + 2]];
		}
	}

	return GL_LoadTexture (identifier, width, height, data, mode, 4);
}

int GL_LoadTextureImage (char *filename, char *identifier, int matchwidth, int matchheight, int mode) 
{
	int texnum;
	byte *data;
	int image_width = -1, image_height = -1;
	gltexture_t *gltexture;

	if (no24bit)
		return 0;

	if (!identifier)
		identifier = filename;

	gltexture = current_texture = GL_FindTexture(identifier);

	if (!(data = GL_LoadImagePixels (filename, matchwidth, matchheight, mode, &image_width, &image_height))) 
	{
		texnum =  (gltexture && !current_texture) ? gltexture->texnum : 0;
	} 
	else 
	{
		texnum = GL_LoadTexturePixels(data, identifier, image_width, image_height, mode);
		Q_free(data);	// Data was Q_malloc'ed by GL_LoadImagePixels.
	}

	current_texture = NULL;
	return texnum;
}

mpic_t *GL_LoadPicImage (const char *filename, char *id, int matchwidth, int matchheight, int mode) 
{
	int width, height, i, real_width, real_height;
	char identifier[MAX_QPATH] = "pic:";
	byte *data, *src, *dest, *buf;
	static mpic_t pic;

	// this is 2D texture loading so it must not have MIP MAPS
	mode = mode & ~TEX_MIPMAP;

	if (no24bit)
		return NULL;

	// Load the image data from file.
	if (!(data = GL_LoadImagePixels (filename, matchwidth, matchheight, 0, &real_width, &real_height)))
		return NULL;

	pic.width = real_width; 
	pic.height = real_height;

	// Check if there's any actual alpha transparency in the image.
	if (mode & TEX_ALPHA) 
	{
		mode &= ~TEX_ALPHA;
		for (i = 0; i < real_width * real_height; i++) 
		{
			if ( ( (((unsigned *) data)[i] >> 24 ) & 0xFF ) < 255) 
			{
				mode |= TEX_ALPHA;
				break;
			}
		}
	}

	if (gl_support_arb_texture_non_power_of_two)
	{
		width = pic.width;
		height = pic.height;
	}
	else
	{
		Q_ROUND_POWER2(pic.width, width);
		Q_ROUND_POWER2(pic.height, height);
	}

	strlcpy (identifier + 4, id ? id : filename, sizeof(identifier) - 4);

	// Upload the texture to OpenGL.
	if (width == pic.width && height == pic.height) 
	{
		// Size of the image is scaled by the power of 2 so we can just
		// load the texture as it is.
		pic.texnum = GL_LoadTexture (identifier, pic.width, pic.height, data, mode, 4);
		pic.sl = 0;
		pic.sh = 1;
		pic.tl = 0;
		pic.th = 1;
	} 
	else 
	{
		// The size of the image data is not a power of 2, so we
		// need to load it into a bigger texture and then set the
		// texture coordinates so that only the relevant part of it is used.

		buf = (byte *) Q_calloc (width * height, 4);

		src = data;
		dest = buf;

		for (i = 0; i < pic.height; i++) 
		{
			memcpy (dest, src, pic.width * 4);
			src += pic.width * 4;	// Next line in the source.
			dest += width * 4;		// Next line in the target.
		}

		pic.texnum = GL_LoadTexture (identifier, width, height, buf, mode, 4);
		pic.sl = 0;
		pic.sh = (float) pic.width / width;
		pic.tl = 0;
		pic.th = (float) pic.height / height;
		Q_free (buf);
	}

	Q_free(data);	// Data was Q_malloc'ed by GL_LoadImagePixels
	return &pic;
}

int GL_LoadCharsetImage (char *filename, char *identifier, int flags) 
{
	int i, texnum, image_size, real_width, real_height;
	byte *data, *buf, *dest, *src;

	if (no24bit)
		return 0;

	if (!(data = GL_LoadImagePixels (filename, 0, 0, flags, &real_width, &real_height)))
		return 0;

	if (!identifier)
		identifier = filename;

	image_size = real_width * real_height;

	buf = dest = (byte *) Q_calloc(image_size * 2, 4);
	src = data;
	for (i = 0 ; i < 16 ; i++) 
	{
		memcpy (dest, src, image_size >> 2);
		src += image_size >> 2;
		dest += image_size >> 1;
	}

	texnum = GL_LoadTexture (identifier, real_width, real_height * 2, buf, flags, 4);

	Q_free(buf);
	Q_free(data);	// data was Q_malloc'ed by GL_LoadImagePixels
	return texnum;
}

void GL_Texture_Init(void) 
{
	cvar_t *cv;
	int i;
	extern int translate_texture, lightmap_textures, sceneblur_texture;

	// Reset some global vars, probably we need here even more...

	// Reset textures array and linked globals
	for (i = 0; i < numgltextures; i++)
		Q_free(gltextures[i].pathname);

	memset(gltextures, 0, sizeof(gltextures));

	texture_extension_number = 1;
	numgltextures  = 0;
	currenttexture = -1;
	current_texture = NULL; // nice names

	// Multi texture.
	oldtarget = GL_TEXTURE0;
	for (i = 0; i < sizeof(cnttextures) / sizeof(cnttextures[0]); i++)
		cnttextures[i] = -1;
	mtexenabled = false;

	// Save a texture slot for translated picture.
	translate_texture = texture_extension_number++;

	// Netgraph.
	netgraphtexture = texture_extension_number++;

	// Player skins.
	playertextures = texture_extension_number;
	texture_extension_number += MAX_CLIENTS; // Normal skins.
	texture_extension_number += MAX_CLIENTS; // Fullbright skins.

	// Lightmap.
	lightmap_textures = texture_extension_number;
	texture_extension_number += MAX_LIGHTMAPS;

	// Motion blur.
	sceneblur_texture = texture_extension_number++;

	// Powerup shells.
	shelltexture = 0; // Force reload.

	// Particles.
	particletexture = 0; // Force reload.

	// Sky.
	memset(skyboxtextures, 0, sizeof(skyboxtextures)); // Force reload.

	i = (cv = Cvar_Find(gl_max_size.name)) ? cv->integer : 0;

	Cvar_SetCurrentGroup(CVAR_GROUP_TEXTURES);
	Cvar_Register(&gl_max_size);
	Cvar_Register(&gl_picmip);
	Cvar_Register(&gl_lerpimages);
	Cvar_Register(&gl_texturemode);
	Cvar_Register(&gl_texturemode2d);
	Cvar_Register(&gl_anisotropy);
	Cvar_Register(&gl_scaleModelTextures);
	Cvar_Register(&gl_scaleTurbTextures);
	Cvar_Register(&gl_miptexLevel);
	Cvar_Register(&gl_externalTextures_world);
	Cvar_Register(&gl_externalTextures_bmodels);
    Cvar_Register(&gl_no24bit);
	Cvar_Register(&gl_wicked_luma_level);

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint *)&gl_max_size_default);
	Cvar_SetDefault(&gl_max_size, gl_max_size_default);

	// This way user can specifie gl_max_size in his cfg.
	if (i) 
		Cvar_SetValue(&gl_max_size, i);

	no24bit = (COM_CheckParm("-no24bit") || gl_no24bit.integer) ? true : false;
	forceTextureReload = COM_CheckParm("-forceTextureReload") ? true : false;
}

