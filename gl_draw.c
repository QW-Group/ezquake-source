/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

$Id: gl_draw.c,v 1.104 2007-10-18 05:28:23 dkure Exp $
*/

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "wad.h"
#include "stats_grid.h"
#include "utils.h"
#include "sbar.h"
#include "common_draw.h"
#ifndef __APPLE__
#include "tr_types.h"
#endif

extern cvar_t crosshair, cl_crossx, cl_crossy, crosshaircolor, crosshairsize;
extern cvar_t scr_coloredText, con_shift, hud_faderankings;

cvar_t	scr_conalpha		= {"scr_conalpha", "0.8"};
cvar_t	scr_conback			= {"scr_conback", "1"};
void OnChange_scr_conpicture(cvar_t *, char *, qbool *);
cvar_t  scr_conpicture      = {"scr_conpicture", "conback", 0, OnChange_scr_conpicture};
cvar_t	scr_menualpha		= {"scr_menualpha", "0.7"};
cvar_t	scr_menudrawhud		= {"scr_menudrawhud", "0"};


void OnChange_crosshairimage(cvar_t *, char *, qbool *);
static cvar_t crosshairimage          = {"crosshairimage", "", 0, OnChange_crosshairimage};

void OnChange_gl_consolefont (cvar_t *, char *, qbool *);
cvar_t gl_consolefont                 = {"gl_consolefont", "povo5", 0, OnChange_gl_consolefont};
static cvar_t gl_alphafont            = {"gl_alphafont", "1"};

cvar_t crosshairalpha                 = {"crosshairalpha", "1"};

static cvar_t crosshairscalemethod         = {"crosshairscalemethod", "0"};
static cvar_t crosshairscale               = {"crosshairscale", "0"};
static int    current_crosshair_pixel_size = 0;

void OnChange_gl_smoothfont (cvar_t *var, char *string, qbool *cancel);
cvar_t gl_smoothfont = {"gl_smoothfont", "1", 0, OnChange_gl_smoothfont};

byte			*draw_chars;						// 8*8 graphic characters
mpic_t			*draw_disc;
static mpic_t	*draw_backtile;

int		translate_texture;

int		char_textures[MAX_CHARSETS];
int		char_range[MAX_CHARSETS];

static mpic_t	conback;

#define		NUMCROSSHAIRS 6
int			crosshairtextures[NUMCROSSHAIRS];
int			crosshairtexture_txt;
mpic_t		crosshairpic;

static byte customcrosshairdata[64];

#define CROSSHAIR_NONE	0
#define CROSSHAIR_TXT	1
#define CROSSHAIR_IMAGE	2
static int customcrosshair_loaded = CROSSHAIR_NONE;

#define CH_POINT(x,y,size) ((x) + (y) * size)

static void CreateBuiltinCrosshair(byte* data, int size, int format)
{
	int i = 0;
	int middle = (size / 2) - 1;

	memset(data, 0xff, size * size);

	switch (format) {
	case 2:
		// simple cross, alternating stipple effect
		for (i = 0; i < size; i += 2) {
			// vertical
			data[CH_POINT(middle, i, size)] = 0xfe;

			// horizontal
			data[CH_POINT(i, middle, size)] = 0xfe;
		}
		break;
	case 3:
		// small cross in center
		{
			int length = max(0, size / 8);

			data[CH_POINT(middle, middle, size)] = 0xfe;
			for (i = 1; i <= length; ++i) {
				int p1 = CH_POINT(middle - i, middle, size);
				int p2 = CH_POINT(middle + i, middle, size);
				int p3 = CH_POINT(middle, middle - i, size);
				int p4 = CH_POINT(middle, middle + i, size);

				data[p1] = 0xfe;
				data[p2] = 0xfe;
				data[p3] = 0xfe;
				data[p4] = 0xfe;
			}
		}
		break;
	case 4:
		// just a dot (scale to make square then circle?)
		data[CH_POINT(middle, middle, size)] = 0xfe;
		break;
	case 5:
		// diagonals (middle missing)
		for (i = 0; i < size; ++i) {
			if (i >= middle - 1 && i <= middle + 1) {
				continue;
			}
			data[CH_POINT(i, i, size)] = 0xfe;
			data[CH_POINT(size - 1 - i, i, size)] = 0xfe;
		}
		break;
	case 6:
		// Supposed to be a smiley face, not converting that...
	case 7:
		// Square with dot in centre
		data[CH_POINT(middle, middle, size)] = 0xfe;
		for (i = 2; i <= size - 4; ++i) {
			data[CH_POINT(i, 0, size)] = 0xfe;
			data[CH_POINT(0, i, size)] = 0xfe;
			data[CH_POINT(i, size - 2, size)] = 0xfe;
			data[CH_POINT(size - 2, i, size)] = 0xfe;
		}
		break;
	}
}

// call it when gl_smoothfont changed or after we load charset
static void Apply_OnChange_gl_smoothfont(int value)
{
	int i;

	if (!char_textures[0])
		return;

	for (i = 0; i < MAX_CHARSETS; i++)
	{
		if (!char_textures[i])
			break;

		GL_Bind(char_textures[i]);
		if (value)
		{
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		else
		{
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
	}
}

void OnChange_gl_smoothfont (cvar_t *var, char *string, qbool *cancel)
{
	Apply_OnChange_gl_smoothfont( Q_atoi(string) );
}

/*
* Draw_CopyMPICKeepSize
* Copy data from src to dst but keep unchanged dst->width and dst->height
*/
static void Draw_CopyMPICKeepSize(mpic_t *dst, mpic_t *src)
{
	byte width[sizeof(dst->width)];
	byte height[sizeof(dst->height)];

	// remember particular fields
	memcpy(width, (byte*)&dst->width, sizeof(width));
	memcpy(height, (byte*)&dst->height, sizeof(height));

	// bit by bit copy
	*dst = *src;

	// restore fields
	memcpy((byte*)&dst->width, width, sizeof(width));
	memcpy((byte*)&dst->height, height, sizeof(height));
}

void OnChange_scr_conpicture(cvar_t *v, char *s, qbool *cancel)
{
	mpic_t *pic_24bit;

	if (!s[0])
		return;

	if (!(pic_24bit = GL_LoadPicImage(va("gfx/%s", s), "conback", 0, 0, 0)))
	{
		Com_Printf("Couldn't load image gfx/%s\n", s);
		return;
	}

	Draw_CopyMPICKeepSize(&conback, pic_24bit);
	Draw_AdjustConback();
}

void OnChange_crosshairimage(cvar_t *v, char *s, qbool *cancel)
{
	mpic_t *pic;

	customcrosshair_loaded &= ~CROSSHAIR_IMAGE;

	if (!s[0])
		return;

	if (!(pic = Draw_CachePicSafe(va("crosshairs/%s", s), false, true)))
	{
		Com_Printf("Couldn't load image %s\n", s);
		return;
	}

	crosshairpic = *pic;
	customcrosshair_loaded |= CROSSHAIR_IMAGE;
}

void customCrosshair_Init(void)
{
	char ch;
	vfsfile_t *f;
	vfserrno_t err;
	int i = 0, c;

	customcrosshair_loaded = CROSSHAIR_NONE;
	crosshairtexture_txt = 0;

	if (!(f = FS_OpenVFS("crosshairs/crosshair.txt", "rb", FS_ANY)))
		return;

	while (i < 64)
	{
		VFS_READ(f, &ch, sizeof(char), &err);
		if (err == VFSERR_EOF) 
		{
			Com_Printf("Invalid format in crosshair.txt (Need 64 X's and O's)\n");
			VFS_CLOSE(f);
			return;
		}
		c = ch;

		if (isspace(c))
			continue;

		if (tolower(c) != 'x' && tolower(c) != 'o')
		{
			Com_Printf("Invalid format in crosshair.txt (Only X's and O's and whitespace permitted)\n");
			VFS_CLOSE(f);
			return;
		}
		customcrosshairdata[i++] = (c == 'x' || c  == 'X') ? 0xfe : 0xff;
	}

	VFS_CLOSE(f);
	crosshairtexture_txt = GL_LoadTexture ("cross:custom", 8, 8, customcrosshairdata, TEX_ALPHA, 1);
	customcrosshair_loaded |= CROSSHAIR_TXT;
}

static int CrosshairPixelSize(void)
{
	// 0 = 8, 1 = 16 etc
	return pow(2, 3 + (int)bound(0, crosshairscale.integer, 5));
}

static void BuildBuiltinCrosshairs(void)
{
	int i;
	char str[256] = {0};
	int crosshair_size = CrosshairPixelSize();

	if (!(customcrosshair_loaded & CROSSHAIR_IMAGE)) {
		memset(&crosshairpic, 0, sizeof(crosshairpic));
	}
	for (i = 0; i < NUMCROSSHAIRS; i++) {
		byte* crosshair_buffer = (byte*)Q_malloc(crosshair_size * crosshair_size);

		snprintf(str, sizeof(str), "cross:hardcoded%d", i);
		CreateBuiltinCrosshair(crosshair_buffer, crosshair_size, i + 2);
		crosshairtextures[i] = GL_LoadTexture (str, crosshair_size, crosshair_size, crosshair_buffer, TEX_ALPHA, 1);

		Q_free(crosshair_buffer);
	}
	current_crosshair_pixel_size = crosshair_size;
}

void Draw_InitCrosshairs(void)
{
	char str[256] = {0};

	BuildBuiltinCrosshairs();
	customCrosshair_Init(); // safe re-init

	snprintf(str, sizeof(str), "%s", crosshairimage.string);
	Cvar_Set(&crosshairimage, str);
}

float overall_alpha = 1.0;

void Draw_SetOverallAlpha(float alpha)
{
	clamp(alpha, 0.0, 1.0);
	overall_alpha = alpha;
}

void Draw_EnableScissorRectangle(int x, int y, int width, int height)
{
	float resdif_w = (glwidth / (float)vid.conwidth);
	float resdif_h = (glheight / (float)vid.conheight);

	glEnable(GL_SCISSOR_TEST);
	glScissor(
		Q_rint(x * resdif_w), 
		Q_rint((vid.conheight - (y + height)) * resdif_h), 
		Q_rint(width * resdif_w), 
		Q_rint(height * resdif_h));
}

void Draw_EnableScissor(int left, int right, int top, int bottom)
{
	Draw_EnableScissorRectangle(left, top, (right - left), (bottom - top));
}

void Draw_DisableScissor(void)
{
	glDisable(GL_SCISSOR_TEST);
}

//
// =============================================================================
//  Scrap allocation
//
//  Allocate all the little status bar objects into a single texture
//  to crutch up stupid hardware / drivers
// =============================================================================

// Some cards have low quality of alpha pics, so load the pics
// without transparent pixels into a different scrap block.
// scrap 0 is solid pics, 1 is transparent.
#define	MAX_SCRAPS		2 // funny, but you can't change this size unless you rewrote code,
						  // you can looks in FTE how they done it.
						  // there really no point to split transparent and solid images.
#define	BLOCK_WIDTH		256
#define	BLOCK_HEIGHT	256

static int			scrap_allocated[MAX_SCRAPS][BLOCK_WIDTH];
static byte			scrap_texels[MAX_SCRAPS][BLOCK_WIDTH*BLOCK_HEIGHT];
static int			scrap_dirty;	// Bit mask.
static int			scrap_texnum[MAX_SCRAPS];

// Returns false if allocation failed.
static qbool Scrap_AllocBlock (int scrapnum, int w, int h, int *x, int *y)
{
	int i, j, best, best2;

	best = BLOCK_HEIGHT;

	for (i = 0; i < BLOCK_WIDTH - w; i++)
	{
		best2 = 0;

		for (j = 0; j < w; j++)
		{
			if (scrap_allocated[scrapnum][i + j] >= best)
				break;
			if (scrap_allocated[scrapnum][i + j] > best2)
				best2 = scrap_allocated[scrapnum][i + j];
		}

		if (j == w)
		{
			// This is a valid spot.
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > BLOCK_HEIGHT)
		return false;

	for (i = 0; i < w; i++)
		scrap_allocated[scrapnum][*x + i] = best + h;

	scrap_dirty |= (1 << scrapnum);

	return true;
}

static void Scrap_Upload (void)
{
	int i;

	for (i = 0; i < MAX_SCRAPS; i++)
	{
		// is dirty?
		if (scrap_dirty & (1 << i))
		{
			char id[64];
			// generate id
			snprintf(id, sizeof(id), "scrap:%d", i);
			// upload it
			scrap_texnum[i] = GL_LoadTexture(id, BLOCK_WIDTH, BLOCK_HEIGHT, scrap_texels[i], TEX_ALPHA | TEX_NOSCALE, 1);
		}
	}

	// no we are clear!
	scrap_dirty = 0;
}

static void Scap_Init(void)
{
	memset (scrap_allocated, 0, sizeof(scrap_allocated));
	memset (scrap_texels,    0, sizeof(scrap_texels));
	memset (scrap_texnum,    0, sizeof(scrap_texnum));
	scrap_dirty = ~0;	// Bit mask - make all bits dirty!

	// upload at least first time, so we set scrap_texnum[], its required
	Scrap_Upload();
}

//=============================================================================
// Support Routines

mpic_t *Draw_CacheWadPic (char *name)
{
	qpic_t	*p;
	mpic_t	*pic, *pic_24bit;

	p = W_GetLumpName (name);
	pic = (mpic_t *)p;

	if ((pic_24bit = GL_LoadPicImage(va("textures/wad/%s", name), name, 0, 0, TEX_ALPHA)) ||
		(pic_24bit = GL_LoadPicImage(va("gfx/%s", name), name, 0, 0, TEX_ALPHA)))
	{
		// Only keep the size info from the lump. The other stuff is copied from the 24 bit image.
		pic->sh		= pic_24bit->sh;
		pic->sl		= pic_24bit->sl;
		pic->texnum = pic_24bit->texnum;
		pic->th		= pic_24bit->th;
		pic->tl		= pic_24bit->tl;

		return pic;
	}

	// Load little ones into the scrap.
	if (p->width < 64 && p->height < 64)
	{
		int x = 0, y = 0, i, j, k;
		// is this pic contain alpha bytes (255)
		qbool alpha = memchr(p->data, 255, p->width * p->height) != NULL;
		// FIXME: as you can see, MAX_SCRAPS hardcoded to 2 and define is just a fiction!
		int texnum = alpha ? 1 : 0;

		if (!Scrap_AllocBlock (texnum, p->width, p->height, &x, &y))
		{
			GL_LoadPicTexture (name, pic, p->data);
			return pic;
		}

		k = 0;

		for (i = 0; i < p->height; i++)
		{
			for (j  = 0; j < p->width; j++, k++)
			{
				scrap_texels[texnum][(y + i) * BLOCK_WIDTH + x + j] = p->data[k];
			}
		}

		pic->sl = (x + 0.25) / (float) BLOCK_WIDTH;
		pic->sh = (x + p->width - 0.25) / (float) BLOCK_WIDTH;
		pic->tl = (y + 0.25) / (float) BLOCK_WIDTH;
		pic->th = (y + p->height - 0.25) / (float) BLOCK_WIDTH;

		if (!scrap_texnum[texnum])
		{
			Com_Printf("Scrap_Upload: texture[%d] still not set, HUD will be broken\n", texnum);
		}

		pic->texnum = scrap_texnum[texnum];
	}
	else
	{
		GL_LoadPicTexture (name, pic, p->data);
	}

	return pic;
}

//
// Loads an image into the cache.
// Variables:
//		crash - Crash the client if loading fails.
//		only24bit - Don't fall back to loading the normal 8-bit texture if
//					loading the 24-bit version fails.
//
mpic_t *Draw_CachePicSafe (const char *path, qbool crash, qbool only24bit)
{
	char stripped_path[MAX_PATH];
	char lmp_path[MAX_PATH];
	mpic_t *fpic;
	mpic_t *pic_24bit;
	qbool lmp_found = false;
	qpic_t *dat = NULL;
	vfsfile_t *v = NULL;

	// Check if the picture was already cached, if so inc refcount.
	if ((fpic = CachePic_Find(path, true))) {
		return fpic;
	}

	// Get the filename without extension.
	COM_StripExtension(path, stripped_path, sizeof(stripped_path));
	snprintf(lmp_path, MAX_PATH, "%s.lmp", stripped_path);

	// Try loading the pic from disk.

	// Only load the 24-bit version of the picture.
	if (only24bit) {
		if (!(pic_24bit = GL_LoadPicImage(path, NULL, 0, 0, TEX_ALPHA))) {
			if(crash) {
				Sys_Error ("Draw_CachePicSafe: failed to load %s", path);
			}
			return NULL;
		}

		/* This will make a copy of the pic struct */
		return CachePic_Add(path, pic_24bit);
	}

	// Load the ".lmp" file.
	if ((v = FS_OpenVFS(lmp_path, "rb", FS_ANY))) {
		VFS_CLOSE(v);

		if (!(dat = (qpic_t *)FS_LoadTempFile(lmp_path, NULL))) {
			if(crash) {
				Sys_Error ("Draw_CachePicSafe: failed to load %s", lmp_path);
			}
			return NULL;
		}
		lmp_found = true;

		// Make sure the width and height are correct.
		SwapPic (dat);
	}

	// Try loading the 24-bit picture.
	// If that fails load the data for the lmp instead.
	if ((pic_24bit = GL_LoadPicImage(path, NULL, 0, 0, TEX_ALPHA))) {
		// Only use the lmp-data if there was one.
		if (lmp_found) {
			pic_24bit->width = dat->width;
			pic_24bit->height = dat->height;
		}
		return CachePic_Add(path, pic_24bit);
	} else if (dat) {
		mpic_t tmp = {0};
		tmp.width = dat->width;
		tmp.height = dat->height;
		GL_LoadPicTexture(path, &tmp, dat->data);
		return CachePic_Add(path, &tmp);
	} else {
		if(crash) {
			Sys_Error ("Draw_CachePicSafe: failed to load %s", path);
		}
		return NULL;
	}
}

mpic_t *Draw_CachePic (char *path)
{
	return Draw_CachePicSafe (path, true, false);
}

/*
* Load_LMP_Charset
*/
static int Load_LMP_Charset (char *name, int flags)
{
	int i;
	byte	buf[128*256];
	byte	*data;
	byte	*src, *dest;
	int texnum;
	int filesize;

	// We expect an .lmp to be in QPIC format, but it's ok if it's just raw data.
	if (!strcasecmp(name, "charset"))
	{
		// work around for original charset
		data = draw_chars;
		filesize = 128*128;
	}
	else
	{
		data = FS_LoadTempFile (va("gfx/%s.lmp", name), &filesize);	
	}

	if (!data)
		return 0;

	if (filesize == 128*128)
	{
		// Raw data.
	}
	else if (filesize == 128*128 + 8)
	{
		qpic_t *p = (qpic_t *)data;
		SwapPic (p);
		if (p->width != 128 || p->height != 128)
			return 0;
		data += 8;
	}
	else
	{
		return 0;
	}

	for (i=0 ; i < (256 * 64) ; i++)
	{
		if (data[i] == 0)
			data[i] = 255;	// Proper transparent color.
	}

	// Convert the 128*128 conchars texture to 128*256 leaving
	// empty space between rows so that chars don't stumble on
	// each other because of texture smoothing.
	// This hack costs us 64K of GL texture memory
	memset (buf, 255, sizeof(buf));
	src = data;
	dest = buf;

	for (i = 0 ; i < 16 ; i++)
	{
		memcpy (dest, src, 128*8);
		src += 128*8;
		dest += 128*8*2;
	}

	texnum = GL_LoadTexture (va("pic:%s", name), 128, 256, buf, flags, 1);
	return texnum;
}

/*
* Load_Locale_Charset
*/
static int Load_Locale_Charset (const char *name, const char *locale, unsigned int num, int range, int flags)
{
	char texture[1024], id[256], lmp[256], basename[MAX_QPATH];

	if (num >= MAX_CHARSETS)
		return 0;

	char_range[num] = 0;

	COM_StripExtension(name, basename, sizeof(basename));
	snprintf(texture, sizeof(texture), "textures/charsets/%s-%s", basename, locale);
	snprintf(id, sizeof(id), "pic:charset-%s", locale);
	snprintf(lmp, sizeof(lmp), "conchars-%s", locale);

	// try first 24 bit
	char_textures[num] = GL_LoadCharsetImage (texture, id, flags);
	// then 8 bit
	if (!char_textures[num])
		char_textures[num] = Load_LMP_Charset (lmp, flags);

	char_range[num] = char_textures[num] ? range : 0;

	return char_textures[num];
}

static int Draw_LoadCharset(const char *name)
{
	int flags = TEX_ALPHA | TEX_NOCOMPRESS | TEX_NOSCALE | TEX_NO_TEXTUREMODE;
	int texnum;
	int i = 0;
	qbool loaded = false;

	//
	// NOTE: we trying to not change char_textures[0] if we can't load charset.
	//		This way user still have some charset and can fix issue.
	//

	if (!strcasecmp(name, "original"))
	{
		if ((texnum = Load_LMP_Charset("charset", flags)))
		{
			char_textures[0] = texnum;
			loaded = true;
		}
	}
	else if ((texnum = GL_LoadCharsetImage(va("textures/charsets/%s", name), "pic:charset", flags)))
	{
		char_textures[0] = texnum;
		loaded = true;
	}

	if (!loaded)
	{
		Com_Printf ("Couldn't load charset \"%s\"\n", name);
		return 1;
	}

	// Load alternate charsets if available
	for (i = 1; i < MAX_CHARSETS; ++i)
	{
		char charsetName[10];
		int textureNumber;
		
		snprintf(charsetName, sizeof(charsetName), "%03d", i);

		textureNumber = Load_Locale_Charset(name, charsetName, i, i << 8, flags);

		// 2.2 only supported -cyr suffix
		if (i == 4 && !textureNumber)
			Load_Locale_Charset(name, "cyr", 4, 0x0400, flags);
	}
	
	// apply gl_smoothfont
	Apply_OnChange_gl_smoothfont(gl_smoothfont.integer);

	return 0;
}

void OnChange_gl_consolefont(cvar_t *var, char *string, qbool *cancel)
{
	*cancel = Draw_LoadCharset(string);
}

void Draw_LoadCharset_f (void)
{
	switch (Cmd_Argc())
	{
		case 1:
			Com_Printf("Current charset is \"%s\"\n", gl_consolefont.string);
			break;
		case 2:
			Cvar_Set(&gl_consolefont, Cmd_Argv(1));
			break;
		default:
			Com_Printf("Usage: %s <charset>\n", Cmd_Argv(0));
	}
}

void Draw_InitCharset(void)
{
	int i;

	memset(char_textures, 0, sizeof(char_textures));
	memset(char_range,    0, sizeof(char_range));

	draw_chars = W_GetLumpName ("conchars");
	for (i = 0; i < 256 * 64; i++)
	{
		if (draw_chars[i] == 0)
			draw_chars[i] = 255;
	}

	Draw_LoadCharset(gl_consolefont.string);

	if (!char_textures[0])
		Cvar_Set(&gl_consolefont, "original");

	if (!char_textures[0])
		Sys_Error("Draw_InitCharset: Couldn't load charset");
}

void Draw_InitConback (void);

void Draw_Init (void)
{
	extern void HUD_Common_Reset_Group_Pics(void);
	Cmd_AddCommand("loadcharset", Draw_LoadCharset_f);

	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register (&scr_conalpha);
	Cvar_Register (&scr_conback);
	Cvar_Register (&scr_conpicture);
	Cvar_Register (&gl_smoothfont);
	Cvar_Register (&gl_consolefont);
	Cvar_Register (&gl_alphafont);

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register (&scr_menualpha);
	Cvar_Register (&scr_menudrawhud);

	Cvar_SetCurrentGroup(CVAR_GROUP_CROSSHAIR);
	Cvar_Register (&crosshairimage);
	Cvar_Register (&crosshairalpha);
	Cvar_Register (&crosshairscale);
	Cvar_Register (&crosshairscalemethod);

	Cvar_ResetCurrentGroup();

	draw_chars = NULL;
	draw_disc = draw_backtile = NULL;

	W_LoadWadFile("gfx.wad"); // Safe re-init.
	CachePics_DeInit();
	HUD_Common_Reset_Group_Pics();

	GL_Texture_Init();  // Probably safe to re-init now.

	// Clear the scrap, should be called ASAP after textures initialization
	Scap_Init();

	// Load the console background and the charset by hand, because we need to write the version
	// string into the background before turning it into a texture.
	Draw_InitCharset(); // Safe re-init.
	Draw_InitConback(); // Safe re-init.

	// Load the crosshair pics
	Draw_InitCrosshairs();

	// Get the other pics we need.
	draw_disc     = Draw_CacheWadPic("disc");
	draw_backtile = Draw_CacheWadPic("backtile");
}

qbool R_CharAvailable (wchar num)
{
	int i;

	if (num == (num & 0xff))
		return true;

	for (i = 1; i < MAX_CHARSETS; i++)
		if (char_range[i] == (num & 0xFF00))
			return true;

	return false;
}

#define CHARSET_CHARS_PER_ROW	16
#define CHARSET_WIDTH			1.0
#define CHARSET_HEIGHT			1.0
#define CHARSET_CHAR_WIDTH		(CHARSET_WIDTH / CHARSET_CHARS_PER_ROW)
#define CHARSET_CHAR_HEIGHT		(CHARSET_HEIGHT / CHARSET_CHARS_PER_ROW)

// x, y					= Pixel position of char.
// num					= The character to draw.
// scale				= The scale of the character.
// apply_overall_alpha	= Should the overall alpha for all drawing apply to this char?
// color				= Color!
// bigchar				= Draw this char using the big character charset.
// gl_statechange		= Change the gl state before drawing?
static void Draw_CharacterBase (int x, int y, wchar num, float scale, qbool apply_overall_alpha, byte color[4], qbool bigchar, qbool gl_statechange)
{
	float frow, fcol;
	int i;
	int slot;
	int char_size = (bigchar ? 64 : 8);

	// Totally off screen.
	if (y <= (-char_size * scale))
		return;

	// Space.
	if (num == 32)
		return;

	// Only apply overall opacity if it's not fully opague.
	apply_overall_alpha = (apply_overall_alpha && (overall_alpha < 1.0));

	// Only change the GL state if we're told to. We keep track of the need for GL state changes
	// in the string drawing function instead. For character drawing functions we do this every time.
	// (For instance, only change color in a string when the actual color changes, instead of doing
	// it on each character always).
	if (gl_statechange)
	{
		// Turn on alpha transparency.
		if ((gl_alphafont.value || apply_overall_alpha))
		{
			glDisable(GL_ALPHA_TEST);
		}

		glEnable(GL_BLEND);

		if (scr_coloredText.integer)
		{
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		}
		else
		{
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		}

		// Set the overall alpha.
		glColor4ub(color[0], color[1], color[2], color[3] * overall_alpha);
	}

	if (bigchar)
	{
		mpic_t *p = Draw_CachePicSafe(MCHARSET_PATH, false, true);

		if (p)
		{
			int sx = 0;
			int sy = 0;
			int char_width = (p->width / 8);
			int char_height = (p->height / 8);
			char c = (char)(num & 0xFF);

			Draw_GetBigfontSourceCoords(c, char_width, char_height, &sx, &sy);

			if (sx >= 0)
			{
				// Don't apply alpha here, since we already applied it above.
				Draw_SAlphaSubPic(x, y, p, sx, sy, char_width, char_height, (((float)char_size / char_width) * scale), 1);
			}

			return;
		}

		// TODO : Force players to have mcharset.png or fallback to overscaling normal font? :s
	}

	slot = 0;
	
	// Is this is a wchar, find a charset that has the char in it.
	if ((num & 0xFF00) != 0)
	{
		for (i = 1; i < MAX_CHARSETS; i++)
		{
			if (char_range[i] == (num & 0xFF00))
			{
				slot = i;
				break;
			}
		}

		if (i == MAX_CHARSETS)
			num = '?';
	}

	num &= 0xFF;	// Only use the first byte.

	// Find the texture coordinates for the character.
	frow = (num >> 4) * CHARSET_CHAR_HEIGHT;	// row = num * (16 chars per row)
	fcol = (num & 0x0F) * CHARSET_CHAR_WIDTH;

	GL_Bind(char_textures[slot]);

	// Draw the character polygon.
	glBegin(GL_QUADS);
	{
		float scale8 = scale * 8;
		float scale8_2 = scale8 * 2;

		// Top left.
		glTexCoord2f (fcol, frow);
		glVertex2f (x, y);

		// Top right.
		glTexCoord2f(fcol + CHARSET_CHAR_WIDTH, frow);
		glVertex2f(x + scale8, y);

		// Bottom right.
		glTexCoord2f(fcol + CHARSET_CHAR_WIDTH, frow + CHARSET_CHAR_WIDTH);
		glVertex2f(x + scale8, y + scale8_2);

		// Bottom left.
		glTexCoord2f(fcol, frow + CHARSET_CHAR_WIDTH);
		glVertex2f(x, y + scale8_2);
	}
	glEnd();
}

static void Draw_ResetCharGLState(void)
{
	glEnable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glColor4ubv(color_white);
}

void Draw_BigCharacter(int x, int y, char c, color_t color, float scale, float alpha)
{
	byte rgba[4];
	COLOR_TO_RGBA(color, rgba);
	Draw_CharacterBase(x, y, char2wc(c), scale, true, rgba, true, true);
	Draw_ResetCharGLState();
}

void Draw_SColoredCharacterW (int x, int y, wchar num, color_t color, float scale)
{
	byte rgba[4];
	COLOR_TO_RGBA(color, rgba);
	Draw_CharacterBase(x, y, num, scale, true, rgba, false, true);
	Draw_ResetCharGLState();
}

void Draw_SCharacter (int x, int y, int num, float scale)
{
	Draw_CharacterBase(x, y, char2wc(num), scale, true, color_white, false, true);
	Draw_ResetCharGLState();
}

void Draw_SCharacterW (int x, int y, wchar num, float scale)
{
	Draw_CharacterBase(x, y, num, scale, true, color_white, false, true);
	Draw_ResetCharGLState();
}

void Draw_CharacterW (int x, int y, wchar num)
{
	Draw_CharacterBase(x, y, num, 1, true, color_white, false, true);
	Draw_ResetCharGLState();
}

void Draw_Character (int x, int y, int num)
{
	Draw_CharacterBase(x, y, char2wc(num), 1, true, color_white, false, true);
	Draw_ResetCharGLState();
}

void Draw_SetColor(byte *rgba, float alpha)
{
	if (scr_coloredText.integer)
	{
		glColor4ub(rgba[0], rgba[1], rgba[2], rgba[3] * alpha * overall_alpha);
	}
}

static void Draw_StringBase (int x, int y, const wchar *text, clrinfo_t *color, int color_count, int red, float scale, float alpha, qbool bigchar, int char_gap)
{
	byte rgba[4];
	qbool color_is_white = true;
	int i, r, g, b;
	int curr_char;
	int color_index = 0;
	color_t last_color = COLOR_WHITE;

	// Nothing to draw.
	if (!*text)
		return;

	Draw_SetColor(color_white, alpha);

	// Turn on alpha transparency.
	if (gl_alphafont.value || (overall_alpha < 1.0))
	{
		glDisable(GL_ALPHA_TEST);
	}

	glEnable(GL_BLEND);

	// Make sure we set the color from scratch so that the 
	// overall opacity is applied properly.
	if (scr_coloredText.integer)
	{
		if (color_count > 0)
		{
			COLOR_TO_RGBA(color[color_index].c, rgba);
		}

		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}
	else
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		memcpy(rgba, color_white, sizeof(byte) * 4);
	}

	// Draw the string.
	for (i = 0; text[i]; i++)
	{
		// If we didn't get a color array, check for color codes in the text instead.
		if (!color)
		{
			if (text[i] == '&')
			{
				if (text[i + 1] == 'c' && text[i + 2] && text[i + 3] && text[i + 4])
				{
					r = HexToInt(text[i + 2]);
					g = HexToInt(text[i + 3]);
					b = HexToInt(text[i + 4]);

					if (r >= 0 && g >= 0 && b >= 0)
					{
						if (scr_coloredText.value)
						{
							rgba[0] = (r * 16);
							rgba[1] = (g * 16);
							rgba[2] = (b * 16);
							rgba[3] = 255;
							color_is_white = false;							
						}

						color_count++; // Keep track on how many colors we're using.

						Draw_SetColor(rgba, alpha);

						i += 4;
						continue;
					}
				}
				else if (text[i + 1] == 'r')
				{
					if (!color_is_white)
					{
						memcpy(rgba, color_white, sizeof(byte) * 4);
						color_is_white = true;
						Draw_SetColor(rgba, alpha);
					}

					i++;
					continue;
				}
			}
		}
		else if (scr_coloredText.value && (color_index < color_count) && (i == color[color_index].i))
		{
			// Change color if the color array tells us this index should have a new color.

			// Set the new color if it's not the same as the last.
			if (color[color_index].c != last_color)
			{
				last_color = color[color_index].c;
				COLOR_TO_RGBA(color[color_index].c, rgba);
				rgba[3] = 255;
				Draw_SetColor(rgba, alpha);
			}

			color_index++; // Goto next color.
		}

		curr_char = text[i];

		// Do not convert the character to red if we're applying color to the text.
		if (red && color_count <= 0)
			curr_char |= 128;

		// Draw the character but don't apply overall opacity, we've already done that
		// And don't update the glstate, we've done that also!
		Draw_CharacterBase(x, y, curr_char, scale, false, rgba, bigchar, false);

		x += ((bigchar ? 64 : 8) * scale) + char_gap;
	}

	Draw_ResetCharGLState();
}

void Draw_BigString (int x, int y, const char *text, clrinfo_t *color, int color_count, float scale, float alpha, int char_gap)
{
	Draw_StringBase(x, y, str2wcs(text), color, color_count, false, scale, alpha, true, char_gap);
}

void Draw_SColoredAlphaString (int x, int y, const wchar *text, clrinfo_t *color, int color_count, int red, float scale, float alpha)
{
	Draw_StringBase(x, y, text, color, color_count, red, scale, alpha, false, 0);
}

void Draw_SColoredString (int x, int y, const wchar *text, clrinfo_t *color, int color_count, int red, float scale)
{
	Draw_StringBase(x, y, text, color, color_count, red, scale, 1, false, 0);
}

void Draw_SString (int x, int y, const char *text, float scale)
{
	Draw_StringBase(x, y, str2wcs(text), NULL, 0, false, scale, 1, false, 0);
}

void Draw_SStringAlpha(int x, int y, const char *text, float scale, float alpha)
{
	Draw_StringBase(x, y, str2wcs(text), NULL, 0, false, scale, alpha, false, 0);
}

void Draw_SAlt_String (int x, int y, const char *text, float scale)
{
	Draw_StringBase(x, y, str2wcs(text), NULL, 0, true, scale, 1, false, 0);
}

void Draw_ColoredString3W(int x, int y, const wchar *text, clrinfo_t *color, int color_count, int red)
{
	Draw_StringBase(x, y, text, color, color_count, red, 1, 1, false, 0);
}

void Draw_ColoredString3(int x, int y, const char *text, clrinfo_t *color, int color_count, int red)
{
	Draw_StringBase(x, y, str2wcs(text), color, color_count, red, 1, 1, false, 0);
}

void Draw_ColoredString(int x, int y, const char *text, int red)
{
	Draw_StringBase(x, y, str2wcs(text), NULL, 0, red, 1, 1, false, 0);
}

void Draw_SColoredStringBasic(int x, int y, const char *text, int red, float scale)
{
	Draw_StringBase(x, y, str2wcs(text), NULL, 0, red, scale, 1, false, 0);
}

void Draw_Alt_String(int x, int y, const char *text)
{
	Draw_StringBase(x, y, str2wcs(text), NULL, 0, true, 1, 1, false, 0);
}

void Draw_AlphaString(int x, int y, const char *text, float alpha)
{
	Draw_StringBase(x, y, str2wcs(text), NULL, 0, false, 1, alpha, false, 0);
}

void Draw_StringW (int x, int y, const wchar *text)
{
	Draw_StringBase(x, y, text, NULL, 0, false, 1, 1, false, 0);
}

void Draw_String (int x, int y, const char *text)
{
	Draw_StringBase(x, y, str2wcs(text), NULL, 0, false, 1, 1, false, 0);
}

qbool CL_MultiviewGetCrosshairCoordinates(qbool use_screen_coords, float* cross_x, float* cross_y, qbool* half_size);

void Draw_Crosshair (void)
{
	float x = 0.0, y = 0.0, ofs1, ofs2, sh, th, sl, tl;
	byte *col;
	extern vrect_t scr_vrect;
	float crosshair_scale = (crosshairscalemethod.integer ? 1 : ((float)glwidth / 320));
	int crosshair_pixel_size = CrosshairPixelSize();

	if (current_crosshair_pixel_size != crosshair_pixel_size) {
		BuildBuiltinCrosshairs();
	}

	if ((crosshair.value >= 2 && crosshair.value <= NUMCROSSHAIRS + 1) ||
		((customcrosshair_loaded & CROSSHAIR_TXT) && crosshair.value == 1) ||
		(customcrosshair_loaded & CROSSHAIR_IMAGE))
	{
		qbool half_size = false;

		if (!crosshairalpha.value) {
			return;
		}

		if (!CL_MultiviewGetCrosshairCoordinates(true, &x, &y, &half_size)) {
			return;
		}

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, glwidth, glheight, 0, -99999, 99999);

		x += (crosshairscalemethod.integer ? 1 : (float)glwidth / vid.width) * cl_crossx.value;
		y += (crosshairscalemethod.integer ? 1 : (float)glheight / vid.height) * cl_crossy.value;

		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		col = crosshaircolor.color;

		glDisable(GL_ALPHA_TEST);
		glEnable (GL_BLEND);
		col[3] = bound(0, crosshairalpha.value, 1) * 255;
		glColor4ubv (col);

		if (customcrosshair_loaded & CROSSHAIR_IMAGE) {
			GL_Bind(crosshairpic.texnum);
			ofs1 = (crosshairpic.width * 0.5f - 0.5f) * bound(0, crosshairsize.value, 20);
			ofs2 = (crosshairpic.height * 0.5f + 0.5f) * bound(0, crosshairsize.value, 20);
			sh = crosshairpic.sh;
			sl = crosshairpic.sl;
			th = crosshairpic.th;
			tl = crosshairpic.tl;
		}
		else {
			GL_Bind ((crosshair.value >= 2) ? crosshairtextures[(int) crosshair.value - 2] : crosshairtexture_txt);
			ofs1 = (crosshair_pixel_size * 0.5f - 0.5f) * crosshair_scale * bound(0, crosshairsize.value, 20);
			ofs2 = (crosshair_pixel_size * 0.5f + 0.5f) * crosshair_scale * bound(0, crosshairsize.value, 20);
			tl = sl = 0;
			sh = th = 1;
		}

		if (half_size) {
			ofs1 *= 0.5f;
			ofs2 *= 0.5f;
		}

#ifdef GL_CLAMP_TO_EDGE
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#else
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
#endif
		glBegin(GL_QUADS);
		glTexCoord2f(sl, tl);
		glVertex2f(x - ofs1, y - ofs1);
		glTexCoord2f(sh, tl);
		glVertex2f(x + ofs2, y - ofs1);
		glTexCoord2f(sh, th);
		glVertex2f(x + ofs2, y + ofs2);
		glTexCoord2f(sl, th);
		glVertex2f(x - ofs1, y + ofs2);
		glEnd();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glDisable(GL_BLEND);
		glEnable (GL_ALPHA_TEST);

		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glColor3ubv (color_white);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, vid.width, vid.height, 0, -99999, 99999);
	}
	else if (crosshair.value) {
		// Multiview
		if (CL_MultiviewInsetEnabled()) {
			if (CL_MultiviewInsetView()) {
				if (cl_sbar.value) {
					Draw_Character (vid.width - (vid.width / 3) / 2 - 4, ((vid.height / 3) - sb_lines / 3) / 2 - 2, '+');
				}
				else {
					Draw_Character (vid.width - (vid.width / 3) / 2 - 4, (vid.height / 3) / 2 - 2, '+');
				}
			}
			else {
				Draw_Character (scr_vrect.x + scr_vrect.width / 2 - 4 + cl_crossx.value, scr_vrect.y + scr_vrect.height / 2 - 4 + cl_crossy.value, '+');
			}
		}
		else if (CL_MultiviewActiveViews() == 2) {
			Draw_Character (vid.width / 2 - 4, vid.height * 3/4 - 2, '+');
			Draw_Character (vid.width / 2 - 4, vid.height / 4 - 2, '+');
		}
		else if (CL_MultiviewActiveViews() == 3) {
			Draw_Character (vid.width / 2 - 4, vid.height / 4 - 2, '+');
			Draw_Character (vid.width / 4 - 4, vid.height/2 + vid.height/4 - 2, '+');
			Draw_Character (vid.width/2 + vid.width/4 - 4, vid.height/2 + vid.height/4 - 2, '+');
		}
		else if (CL_MultiviewActiveViews() == 4) {
			Draw_Character (vid.width/4 - 4, vid.height/4 - 2, '+');
			Draw_Character (vid.width/2 + vid.width/4 - 4, vid.height/4 - 2, '+');
			Draw_Character (vid.width/4 - 4, vid.height/2 + vid.height/4 - 2, '+');
			Draw_Character (vid.width/2 + vid.width/4 - 4, vid.height/2 + vid.height/4 - 2, '+');
		}
		else {
			Draw_Character (scr_vrect.x + scr_vrect.width / 2 - 4 + cl_crossx.value, scr_vrect.y + scr_vrect.height / 2 - 4 + cl_crossy.value, '+');
		}
	}
}

void Draw_TextBox (int x, int y, int width, int lines)
{
	mpic_t *p;
	int cx, cy, n;

	// draw left side
	cx = x;
	cy = y;
	p = Draw_CachePic ("gfx/box_tl.lmp");
	Draw_TransPic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_ml.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		Draw_TransPic (cx, cy, p);
	}

	p = Draw_CachePic ("gfx/box_bl.lmp");
	Draw_TransPic (cx, cy+8, p);

	// Draw middle.
	cx += 8;
	while (width > 0)
	{
		cy = y;
		p = Draw_CachePic ("gfx/box_tm.lmp");
		Draw_TransPic (cx, cy, p);
		p = Draw_CachePic ("gfx/box_mm.lmp");

		for (n = 0; n < lines; n++)
		{
			cy += 8;
			if (n == 1)
				p = Draw_CachePic ("gfx/box_mm2.lmp");
			Draw_TransPic (cx, cy, p);
		}

		p = Draw_CachePic ("gfx/box_bm.lmp");
		Draw_TransPic (cx, cy+8, p);
		width -= 2;
		cx += 16;
	}

	// Draw right side.
	cy = y;
	p = Draw_CachePic ("gfx/box_tr.lmp");
	Draw_TransPic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_mr.lmp");

	for (n = 0; n < lines; n++)
	{
		cy += 8;
		Draw_TransPic (cx, cy, p);
	}

	p = Draw_CachePic ("gfx/box_br.lmp");
	Draw_TransPic (cx, cy+8, p);
}

// This repeats a 64 * 64 tile graphic to fill the screen around a sized down refresh window.
void Draw_TileClear (int x, int y, int w, int h)
{
	GL_Bind (draw_backtile->texnum);
	glBegin (GL_QUADS);
	glTexCoord2f (x / 64.0, y / 64.0);
	glVertex2f (x, y);
	glTexCoord2f ((x + w) / 64.0, y / 64.0);
	glVertex2f (x + w, y);
	glTexCoord2f ((x + w) / 64.0, (y + h) / 64.0);
	glVertex2f (x + w, y + h);
	glTexCoord2f (x / 64.0, (y + h) / 64.0 );
	glVertex2f (x, y + h);
	glEnd ();
}

void Draw_AlphaRectangleRGB (int x, int y, int w, int h, float thickness, qbool fill, color_t color)
{
	byte bytecolor[4];

	// Is alpha 0?
	if ((byte)(color >> 24 & 0xFF) == 0)
		return;

	glDisable (GL_TEXTURE_2D);
	glEnable (GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	COLOR_TO_RGBA(color, bytecolor);
	glColor4ub(bytecolor[0], bytecolor[1], bytecolor[2], bytecolor[3] * overall_alpha);

	thickness = max(0, thickness);

	if (fill)
	{
		glRectf(x, y, x + w, y + h);
	}
	else
	{
		glRectf(x, y, x + w	, y + thickness);
		glRectf(x, y + thickness, x + thickness, y + h - thickness);
		glRectf(x + w - thickness, y + thickness, x + w, y + h - thickness);
		glRectf(x, y + h, x + w, y + h - thickness);
	}

	glEnable (GL_TEXTURE_2D);
	glEnable (GL_ALPHA_TEST);
	glDisable (GL_BLEND);

	glColor4ubv (color_white);
}

void Draw_AlphaRectangle (int x, int y, int w, int h, byte c, float thickness, qbool fill, float alpha)
{
	Draw_AlphaRectangleRGB(x, y, w, h, thickness, fill,
		RGBA_TO_COLOR(host_basepal[c * 3], host_basepal[c * 3 + 1], host_basepal[c * 3 + 2], (byte)(alpha * 255)));
}

void Draw_AlphaFillRGB (int x, int y, int w, int h, color_t color)
{
	Draw_AlphaRectangleRGB (x, y, w, h, 1, true, color);
}

void Draw_AlphaFill (int x, int y, int w, int h, byte c, float alpha)
{
	Draw_AlphaRectangle(x, y, w, h, c, 1, true, alpha);
}

void Draw_Fill (int x, int y, int w, int h, byte c)
{
	Draw_AlphaFill(x, y, w, h, c, 1);
}

void Draw_AlphaLineRGB (int x_start, int y_start, int x_end, int y_end, float thickness, color_t color)
{
	byte bytecolor[4];
	glDisable (GL_TEXTURE_2D);

	glEnable (GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	COLOR_TO_RGBA(color, bytecolor);
	glColor4ub(bytecolor[0], bytecolor[1], bytecolor[2], bytecolor[3] * overall_alpha);

	if(thickness > 0.0)
	{
		glLineWidth(thickness);
	}

	glBegin (GL_LINES);
	glVertex2f (x_start, y_start);
	glVertex2f (x_end, y_end);
	glEnd ();

	glEnable (GL_TEXTURE_2D);
	glEnable(GL_ALPHA_TEST);
	glDisable (GL_BLEND);

	glColor3ubv (color_white);
}

void Draw_AlphaLine (int x_start, int y_start, int x_end, int y_end, float thickness, byte c, float alpha)
{
	Draw_AlphaLineRGB (x_start, y_start, x_end, y_end, thickness,
		RGBA_TO_COLOR(host_basepal[c * 3], host_basepal[c * 3 + 1], host_basepal[c * 3 + 2], 255));
}

void Draw_Polygon(int x, int y, vec3_t *vertices, int num_vertices, qbool fill, color_t color)
{
	byte bytecolor[4];
	int i = 0;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glEnable (GL_BLEND);
	glDisable(GL_ALPHA_TEST);

	COLOR_TO_RGBA(color, bytecolor);
	glColor4ub(bytecolor[0], bytecolor[1], bytecolor[2], bytecolor[3] * overall_alpha);

	glDisable (GL_TEXTURE_2D);

	if(fill)
	{
		glBegin(GL_TRIANGLE_FAN);
	}
	else
	{
		glBegin (GL_LINE_LOOP);
	}

	for(i = 0; i < num_vertices; i++)
	{
		glVertex2f(x + vertices[i][0], y + vertices[i][1]);
	}

	glEnd();

	glPopAttrib();
}

#define CIRCLE_LINE_COUNT	40

void Draw_AlphaPieSliceRGB (int x, int y, float radius, float startangle, float endangle, float thickness, qbool fill, color_t color)
{
	byte bytecolor[4];
	double angle;
	int i;
	int start;
	int end;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glDisable (GL_TEXTURE_2D);

	glEnable (GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	COLOR_TO_RGBA(color, bytecolor);
	glColor4ub(bytecolor[0], bytecolor[1], bytecolor[2], bytecolor[3] * overall_alpha);

	if(thickness > 0.0)
	{
		glLineWidth(thickness);
	}

	if(fill)
	{
		glBegin(GL_TRIANGLE_STRIP);
	}
	else
	{
		glBegin (GL_LINE_LOOP);
	}

	// Get the vertex index where to start and stop drawing.
	start	= Q_rint((startangle * CIRCLE_LINE_COUNT) / (2 * M_PI));
	end		= Q_rint((endangle   * CIRCLE_LINE_COUNT) / (2 * M_PI));

	// If the end is less than the start, increase the index so that
	// we start on a "new" circle.
	if(end < start)
	{
		start = start + CIRCLE_LINE_COUNT;
	}

	// Create a vertex at the exact position specified by the start angle.
	glVertex2f (x + radius * cos(startangle), y - radius * sin(startangle));

	// TODO: Use lookup table for sin/cos?
	for(i = start; i < end; i++)
	{
		angle = (i * 2 * M_PI) / CIRCLE_LINE_COUNT;
		glVertex2f (x + radius * cos(angle), y - radius * sin(angle));

		// When filling we're drawing triangles so we need to
		// create a vertex in the middle of the vertex to fill
		// the entire pie slice/circle.
		if(fill)
		{
			glVertex2f (x, y);
		}
    }

	glVertex2f (x + radius * cos(endangle), y - radius * sin(endangle));

	// Create a vertex for the middle point if we're not drawing a complete circle.
	if(endangle - startangle < 2 * M_PI)
	{
		glVertex2f (x, y);
	}

	glEnd ();

	glEnable (GL_TEXTURE_2D);

	glEnable (GL_ALPHA_TEST);
	glDisable (GL_BLEND);

	glColor4ubv (color_white);

	glPopAttrib();
}

void Draw_AlphaPieSlice (int x, int y, float radius, float startangle, float endangle, float thickness, qbool fill, byte c, float alpha)
{
	Draw_AlphaPieSliceRGB (x, y, radius, startangle, endangle, thickness, fill,
		RGBA_TO_COLOR(host_basepal[c * 3], host_basepal[c * 3 + 1], host_basepal[c * 3 + 2], (byte)Q_rint(255 * alpha)));
}

void Draw_AlphaCircleRGB (int x, int y, float radius, float thickness, qbool fill, color_t color)
{
	Draw_AlphaPieSliceRGB (x, y, radius, 0, 2 * M_PI, thickness, fill, color);
}

void Draw_AlphaCircle (int x, int y, float radius, float thickness, qbool fill, byte c, float alpha)
{
	Draw_AlphaPieSlice (x, y, radius, 0, 2 * M_PI, thickness, fill, c, alpha);
}

void Draw_AlphaCircleOutlineRGB (int x, int y, float radius, float thickness, color_t color)
{
	Draw_AlphaCircleRGB (x, y, radius, thickness, false, color);
}

void Draw_AlphaCircleOutline (int x, int y, float radius, float thickness, byte color, float alpha)
{
	Draw_AlphaCircle (x, y, radius, thickness, false, color, alpha);
}

void Draw_AlphaCircleFillRGB (int x, int y, float radius, color_t color)
{
	Draw_AlphaCircleRGB (x, y, radius, 1.0, true, color);
}

void Draw_AlphaCircleFill (int x, int y, float radius, byte color, float alpha)
{
	Draw_AlphaCircle (x, y, radius, 1.0, true, color, alpha);
}

//
// SCALE versions of some functions
//

//=============================================================================
// Draw picture functions
//=============================================================================
void Draw_SAlphaSubPic2 (int x, int y, mpic_t *pic, int src_x, int src_y, int src_width, int src_height, float scale_x, float scale_y, float alpha)
{
	float newsl, newtl, newsh, newth;
    float oldglwidth, oldglheight;

    if (scrap_dirty)
	{
        Scrap_Upload ();
	}

    oldglwidth = pic->sh - pic->sl;
    oldglheight = pic->th - pic->tl;

    newsl = pic->sl + (src_x * oldglwidth) / (float)pic->width;
    newsh = newsl + (src_width * oldglwidth) / (float)pic->width;

    newtl = pic->tl + (src_y * oldglheight) / (float)pic->height;
    newth = newtl + (src_height * oldglheight) / (float)pic->height;

	alpha *= overall_alpha;
	if(alpha < 1.0) {
		glDisable (GL_ALPHA_TEST);
		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glCullFace (GL_FRONT);
		glColor4f (1, 1, 1, alpha);
	}

    GL_Bind (pic->texnum);

    glBegin (GL_QUADS);
	{
		// Upper left corner.
		glTexCoord2f (newsl, newtl);
		glVertex2f (x, y);

		// Upper right corner.
		glTexCoord2f (newsh, newtl);
		glVertex2f (x + (scale_x * src_width), y);

		// Bottom right corner.
		glTexCoord2f (newsh, newth);
		glVertex2f (x + (scale_x * src_width), y + (scale_y * src_height));

		// Bottom left corner.
		glTexCoord2f (newsl, newth);
		glVertex2f (x, y + (scale_y * src_height));
	}
	glEnd ();
	
	if(alpha < 1.0) {
		glEnable (GL_ALPHA_TEST);
		glDisable (GL_BLEND);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glColor4f (1, 1, 1, 1);
	}
}

void Draw_SAlphaSubPic (int x, int y, mpic_t *pic, int src_x, int src_y, int src_width, int src_height, float scale, float alpha)
{
	Draw_SAlphaSubPic2 (x, y, pic, src_x, src_y, src_width, src_height, scale, scale, alpha);
}

void Draw_SSubPic(int x, int y, mpic_t *gl, int srcx, int srcy, int width, int height, float scale)
{
	Draw_SAlphaSubPic (x, y, gl, srcx, srcy, width, height, scale, 1);
}

void Draw_AlphaSubPic (int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height, float alpha)
{
	Draw_SAlphaSubPic (x, y, pic, srcx, srcy, width, height, 1, alpha);
}

void Draw_SubPic (int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height)
{
	Draw_SAlphaSubPic (x, y, pic, srcx, srcy, width, height, 1, 1);
}

void Draw_AlphaPic (int x, int y, mpic_t *pic, float alpha)
{
	Draw_SAlphaSubPic (x , y, pic, 0, 0, pic->width, pic->height, 1, alpha);
}

void Draw_SAlphaPic (int x, int y, mpic_t *gl, float alpha, float scale)
{
	Draw_SAlphaSubPic (x ,y , gl, 0, 0, gl->width, gl->height, scale, alpha);
}

void Draw_SPic (int x, int y, mpic_t *gl, float scale)
{
	Draw_SAlphaSubPic (x, y, gl, 0, 0, gl->width, gl->height, scale, 1.0);
}

void Draw_FitPic (int x, int y, int fit_width, int fit_height, mpic_t *gl)
{
    float sw, sh;
    sw = (float) fit_width / (float) gl->width;
    sh = (float) fit_height / (float) gl->height;
    Draw_SPic(x, y, gl, min(sw, sh));
}

void Draw_STransPic (int x, int y, mpic_t *pic, float scale)
{
    Draw_SPic (x, y, pic, scale);
}

void Draw_TransSubPic(int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height)
{
    Draw_SubPic(x, y, pic, srcx, srcy, width, height);
}

void Draw_Pic (int x, int y, mpic_t *pic)
{
	Draw_SAlphaSubPic (x, y, pic, 0, 0, pic->width, pic->height, 1, 1);
}

void Draw_TransPic (int x, int y, mpic_t *pic)
{
	Draw_Pic (x, y, pic);
}

void Draw_SFill (int x, int y, int w, int h, byte c, float scale)
{
    glDisable(GL_TEXTURE_2D);
    glColor4ub(host_basepal[c * 3], host_basepal[(c * 3) + 1], host_basepal[(c * 3) + 2 ], overall_alpha);

    glBegin(GL_QUADS);

    glVertex2f(x, y);
    glVertex2f(x + (w * scale), y);
    glVertex2f(x + (w * scale), y + (h * scale));
    glVertex2f(x, y + (h * scale));

    glEnd();
    glColor4ubv(color_white);
    glEnable(GL_TEXTURE_2D);
}

static char last_mapname[MAX_QPATH] = {0};
static mpic_t *last_lvlshot = NULL;

// If conwidth or conheight changes, adjust conback sizes too.
void Draw_AdjustConback (void)
{
	conback.width  = vid.conwidth;
	conback.height = vid.conheight;

	if (last_lvlshot) {
		// Resize.
		last_lvlshot->width = conback.width;
		last_lvlshot->height = conback.height;
	}
}

void Draw_InitConback (void)
{
	qpic_t *cb;
	int start;
	mpic_t *pic_24bit;

	// Level shots init. It's cache based so don't free!
	// Expect the cache to be wiped thus render the old data invalid
	last_lvlshot = NULL;
	last_mapname[0] = 0;

	start = Hunk_LowMark ();

	if (!(cb = (qpic_t *) FS_LoadHunkFile ("gfx/conback.lmp", NULL)))
		Sys_Error ("Couldn't load gfx/conback.lmp");
	SwapPic (cb);

	if (cb->width != 320 || cb->height != 200)
		Sys_Error ("Draw_InitConback: conback.lmp size is not 320x200");

	if ((pic_24bit = GL_LoadPicImage(va("gfx/%s", scr_conpicture.string), "conback", 0, 0, 0)))
	{
		Draw_CopyMPICKeepSize(&conback, pic_24bit);
	}
	else
	{
		conback.width = cb->width;
		conback.height = cb->height;
		GL_LoadPicTexture ("conback", &conback, cb->data);
	}

	Draw_AdjustConback();

	// Free loaded console.
	Hunk_FreeToLowMark (start);
}

void Draw_ConsoleBackground (int lines)
{
	mpic_t *lvlshot = NULL;
	float alpha = (SCR_NEED_CONSOLE_BACKGROUND ? 1 : bound(0, scr_conalpha.value, 1)) * overall_alpha;

	if (host_mapname.string[0]											// We have mapname.
		 && (    scr_conback.value == 2									// Always per level conback.
			 || (scr_conback.value == 1 && SCR_NEED_CONSOLE_BACKGROUND) // Only at load time.
			))
	{
		// Here we limit call Draw_CachePicSafe() once per level,
		// because if image not found Draw_CachePicSafe() will try open image again each frame, that cause HDD lag.
		if (strncmp(host_mapname.string, last_mapname, sizeof(last_mapname)))
		{
			char name[MAX_QPATH];

			snprintf(name, sizeof(name), "textures/levelshots/%s.xxx", host_mapname.string);
			if ((last_lvlshot = Draw_CachePicSafe(name, false, true)))
			{
				// Resize.
				last_lvlshot->width  = conback.width;
				last_lvlshot->height = conback.height;
			}

			strlcpy(last_mapname, host_mapname.string, sizeof(last_mapname)); // Save.
		}

		lvlshot = last_lvlshot;
	}

	if (alpha) {
		int con_shift_value = cls.state == ca_active ? con_shift.value : 0;
		Draw_AlphaPic(0, (lines - vid.height) + con_shift_value, lvlshot ? lvlshot : &conback, alpha);
	}
}

void Draw_FadeScreen (float alpha)
{
	alpha = bound(0, alpha, 1) * overall_alpha;
	if (!alpha)
		return;

	if (alpha < 1)
	{
		glDisable (GL_ALPHA_TEST);
		glEnable (GL_BLEND);
		glColor4f (0, 0, 0, alpha);
	}
	else
	{
		glColor3f (0, 0, 0);
	}

	glDisable (GL_TEXTURE_2D);

	glBegin (GL_QUADS);
	glVertex2f (0, 0);
	glVertex2f (vid.width, 0);
	glVertex2f (vid.width, vid.height);
	glVertex2f (0, vid.height);
	glEnd ();

	if (alpha < 1)
	{
		glDisable (GL_BLEND);
		glEnable (GL_ALPHA_TEST);
	}
	glColor3ubv (color_white);
	glEnable (GL_TEXTURE_2D);

	Sbar_Changed();
}

//=============================================================================
// Draws the little blue disc in the corner of the screen.
// Call before beginning any disc IO.
void Draw_BeginDisc (void)
{
	if (!draw_disc)
		return;

	// Intel cards, most notably Intel 915GM/910GML has problems with
	// writing directly to the front buffer and then flipping the back buffer,
	// so don't draw the I/O disc on those cards, it will cause the console
	// to flicker.
	//
	// From Intels dev network:
	// "Using two dimensional data within a 3D scene is sometimes used to render
	// objects like scoreboards and road signs. When that blit request is sent 
	// to or from a buffer, the data contained within must be updated, causing 
	// a pipeline flush and disabling Zone Rendering. One easy way to generate 
	// the same effect is to use a quad or a billboard that is aligned to the 
	// view frustrum. Similarly, a flip operation while rendering to a back buffer 
	// will cause serialization. Be sure you are done altering the back buffer
	// before you flip.
#ifndef __APPLE__
	if (glConfig.hardwareType == GLHW_INTEL)
		return;
#endif

	glDrawBuffer  (GL_FRONT);
	Draw_Pic (vid.width - 24, 0, draw_disc);
	glDrawBuffer  (GL_BACK);
}

// Erases the disc icon.
// Call after completing any disc IO
void Draw_EndDisc (void) {}

//
// Changes the projection to orthogonal (2D drawing).
//
void GL_Set2D (void)
{
	glViewport (glx, gly, glwidth, glheight);

	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();
	glOrtho (0, vid.width, vid.height, 0, -99999, 99999);

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();

	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);
	glDisable (GL_BLEND);
	glEnable (GL_ALPHA_TEST);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glColor3ubv (color_white);
}

void Draw_2dAlphaTexture(float x, float y, float width, float height, int texture_num, float alpha)
{
	mpic_t pic;

	pic.height = height;
	pic.width = width;
	pic.th = 1;
	pic.tl = 0;
	pic.sh = 1;
	pic.sl = 0;
	pic.texnum = texture_num;

	Draw_AlphaPic(x, y, &pic, alpha);
/*
	Draw_AlphaPic
	glPushAttrib(GL_ENABLE_BIT);

	glDisable(GL_CULL_FACE);
	glEnable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);

	GL_Bind(texture_num);

	glBegin (GL_QUADS);

	glTexCoord2f (0, 0);
	glVertex2f (x, y);

	glTexCoord2f (1, 0);
	glVertex2f (x + width, y);

	glTexCoord2f (0, 1);
	glVertex2f (x, y + height);

	glTexCoord2f (1, 1);
	glVertex2f (x + width, y + height);

	glEnd ();

	glPopAttrib();*/
}