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

$Id: gl_draw.c,v 1.69 2007-07-03 00:07:05 cokeman1982 Exp $
*/

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "wad.h"
#include "stats_grid.h"
#include "utils.h"
#include "sbar.h"

extern cvar_t crosshair, cl_crossx, cl_crossy, crosshaircolor, crosshairsize;
extern cvar_t scr_coloredText, con_shift;

cvar_t	scr_conalpha		= {"scr_conalpha", "0.8"};
cvar_t	scr_conback			= {"scr_conback", "1"};
cvar_t	scr_menualpha		= {"scr_menualpha", "0.7"};


qbool OnChange_gl_crosshairimage(cvar_t *, char *);
cvar_t	gl_crosshairimage   = {"crosshairimage", "", 0, OnChange_gl_crosshairimage};

qbool OnChange_gl_consolefont (cvar_t *, char *);
cvar_t	gl_consolefont		= {"gl_consolefont", "povo5", 0, OnChange_gl_consolefont};
cvar_t	gl_alphafont		= {"gl_alphafont", "1"};

cvar_t	gl_crosshairalpha	= {"crosshairalpha", "1"};


qbool OnChange_gl_smoothfont (cvar_t *var, char *string);
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


static byte crosshairdata[NUMCROSSHAIRS][64] = {
    {
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    },
    {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    },
    {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    },
    {
	0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
	0xff, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff,
	0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff,
	0xff, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff,
	0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe
    },
    {
	0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xff,
	0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xff,
	0xfe, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xfe, 0xff,
	0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xff,
	0xfe, 0xff, 0xfe, 0xfe, 0xfe, 0xff, 0xfe, 0xff,
	0xff, 0xfe, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff,
	0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    },
    {
	0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff,
	0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff,
	0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    }
};


qbool OnChange_gl_smoothfont (cvar_t *var, char *string) {
	float newval;
	int i;

	newval = Q_atof (string);

	if (!newval == !gl_smoothfont.value || !char_textures[0])
			return false;

	for (i = 0; i < MAX_CHARSETS; i++) {
		if (!char_textures[i])
			break;
		GL_Bind(char_textures[i]);
		if (newval)	{
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		} else {
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
	}
	return false;
}


qbool OnChange_gl_crosshairimage(cvar_t *v, char *s) {
	mpic_t *pic;

	customcrosshair_loaded &= ~CROSSHAIR_IMAGE;

	if (!s[0])
		return false;

	if (!(pic = Draw_CachePicSafe(va("crosshairs/%s", s), false, true))) {
		Com_Printf("Couldn't load image %s\n", s);
		return false;
	}
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	crosshairpic = *pic;
	customcrosshair_loaded |= CROSSHAIR_IMAGE;
	return false;
}

void customCrosshair_Init(void) {
	FILE *f;
	int i = 0, c;

	customcrosshair_loaded = CROSSHAIR_NONE;
	crosshairtexture_txt = 0;

	if (FS_FOpenFile("crosshairs/crosshair.txt", &f) == -1)
		return;

	while (i < 64) {
		c = fgetc(f);
		if (c == EOF) {
			Com_Printf("Invalid format in crosshair.txt (Need 64 X's and O's)\n");
			fclose(f);
			return;
		}
		if (isspace(c))
			continue;
		if (tolower(c) != 'x' && tolower(c) != 'o') {
			Com_Printf("Invalid format in crosshair.txt (Only X's and O's and whitespace permitted)\n");
			fclose(f);
			return;
		}
		customcrosshairdata[i++] = (c == 'x' || c  == 'X') ? 0xfe : 0xff;
	}
	fclose(f);
	crosshairtexture_txt = GL_LoadTexture ("", 8, 8, customcrosshairdata, TEX_ALPHA, 1);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	customcrosshair_loaded |= CROSSHAIR_TXT;
}

void Draw_InitCrosshairs(void) {
	int i;
	char str[256] = {0};

	memset(&crosshairpic, 0, sizeof(crosshairpic));

	for (i = 0; i < NUMCROSSHAIRS; i++) {
		crosshairtextures[i] = GL_LoadTexture ("", 8, 8, crosshairdata[i], TEX_ALPHA, 1);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	customCrosshair_Init(); // safe re-init

	snprintf(str, sizeof(str), "%s", gl_crosshairimage.string);
	Cvar_Set(&gl_crosshairimage, str);
}

void GL_EnableScissorRectangle(int x, int y, int width, int height)
{
	glEnable(GL_SCISSOR_TEST);
	glScissor(x, glheight - (y + height), width, height);
}

void GL_EnableScissor(int left, int right, int top, int bottom)
{
	GL_EnableScissorRectangle(left, top, (right - left), (bottom - top));
}

void GL_DisableScissor()
{
	glDisable(GL_SCISSOR_TEST);
}

/*
=============================================================================
  scrap allocation

  Allocate all the little status bar objects into a single texture
  to crutch up stupid hardware / drivers
=============================================================================
*/

// some cards have low quality of alpha pics, so load the pics
// without transparent pixels into a different scrap block.
// scrap 0 is solid pics, 1 is transparent
#define	BLOCK_WIDTH		256
#define	BLOCK_HEIGHT	256

int			scrap_allocated[MAX_SCRAPS][BLOCK_WIDTH];
byte		scrap_texels[MAX_SCRAPS][BLOCK_WIDTH*BLOCK_HEIGHT*4];
int			scrap_dirty = 0;	// bit mask
int			scrap_texnum;

// returns false if allocation failed
qbool Scrap_AllocBlock (int scrapnum, int w, int h, int *x, int *y) {
	int i, j, best, best2;

	best = BLOCK_HEIGHT;

	for (i = 0; i < BLOCK_WIDTH - w; i++) {
		best2 = 0;

		for (j = 0; j < w; j++) {
			if (scrap_allocated[scrapnum][i + j] >= best)
				break;
			if (scrap_allocated[scrapnum][i + j] > best2)
				best2 = scrap_allocated[scrapnum][i + j];
		}
		if (j == w) {	// this is a valid spot
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

void Scrap_Upload (void) {
	int i;

	for (i = 0; i < MAX_SCRAPS; i++) {
		if (!(scrap_dirty & (1 << i)))
			continue;
		scrap_dirty &= ~(1 << i);
		GL_Bind(scrap_texnum + i);
		GL_Upload8 (scrap_texels[i], BLOCK_WIDTH, BLOCK_HEIGHT, TEX_ALPHA);
	}
}

//=============================================================================
/* Support Routines */

typedef struct cachepic_s {
	char		name[MAX_QPATH];
	mpic_t		pic;
} cachepic_t;

typedef struct cachepic_node_s {
	cachepic_t data;
	struct cachepic_node_s *next;
} cachepic_node_t;

#define	CACHED_PICS_HDSIZE		64
cachepic_node_t	*cachepics[CACHED_PICS_HDSIZE];

//int		pic_texels, pic_count;

mpic_t *Draw_CacheWadPic (char *name) {
	qpic_t	*p;
	mpic_t	*pic, *pic_24bit;

	p = W_GetLumpName (name);
	pic = (mpic_t *)p;

	if (
		(pic_24bit = GL_LoadPicImage(va("textures/wad/%s", name), name, 0, 0, TEX_ALPHA)) ||
		(pic_24bit = GL_LoadPicImage(va("gfx/%s", name), name, 0, 0, TEX_ALPHA))
	) {
		memcpy(&pic->texnum, &pic_24bit->texnum, sizeof(mpic_t) - 8);
		return pic;
	}

	// load little ones into the scrap
	if (p->width < 64 && p->height < 64) {
		int x = 0, y = 0, i, j, k, texnum;

		texnum = memchr(p->data, 255, p->width*p->height) != NULL;
		if (!Scrap_AllocBlock (texnum, p->width, p->height, &x, &y)) {
			GL_LoadPicTexture (name, pic, p->data);
			return pic;
		}
		k = 0;
		for (i = 0; i < p->height; i++)
			for (j  = 0; j < p->width; j++, k++)
				scrap_texels[texnum][(y + i) * BLOCK_WIDTH + x + j] = p->data[k];
		texnum += scrap_texnum;
		pic->texnum = texnum;
		pic->sl = (x + 0.25) / (float) BLOCK_WIDTH;
		pic->sh = (x + p->width - 0.25) / (float) BLOCK_WIDTH;
		pic->tl = (y + 0.25) / (float) BLOCK_WIDTH;
		pic->th = (y + p->height - 0.25) / (float) BLOCK_WIDTH;

//		pic_count++;
//		pic_texels += p->width * p->height;
	} else {
		GL_LoadPicTexture (name, pic, p->data);
	}

	return pic;
}

mpic_t *CachePic_Find(const char *path) {
	int key = Com_HashKey(path) % CACHED_PICS_HDSIZE;
	cachepic_node_t *searchpos = cachepics[key];

	while (searchpos) {
		if (!strcmp(searchpos->data.name, path)) {
			return &searchpos->data.pic;
		}
		searchpos = searchpos->next;
	}

	return NULL;
}

mpic_t* CachePic_Add(const char *path, mpic_t *pic) {
	int key = Com_HashKey(path) % CACHED_PICS_HDSIZE;
	cachepic_node_t *searchpos = cachepics[key];
	cachepic_node_t **nextp = cachepics + key;

	while (searchpos) {
		nextp = &searchpos->next;
		searchpos = searchpos->next;
	}

	searchpos = (cachepic_node_t *) Q_malloc(sizeof(cachepic_node_t));
	// write data
	memcpy(&searchpos->data.pic, pic, sizeof(mpic_t));
	strncpy(searchpos->data.name, path, sizeof(searchpos->data.name));
	searchpos->next = NULL; // terminate the list
	*nextp = searchpos;// connect to the list

	return &searchpos->data.pic;
}

void CachePics_DeInit(void) {
	int i;
	cachepic_node_t *cur, *nxt;

	for (i = 0; i < CACHED_PICS_HDSIZE; i++)
		for (cur = cachepics[i]; cur; cur = nxt) {
			nxt = cur->next;
			Q_free(cur);
		}

	memset(cachepics, 0, sizeof(cachepics));
}

mpic_t *Draw_CachePicSafe (char *path, qbool crash, qbool only24bit)
{
	mpic_t pic, *fpic, *pic_24bit;
	qpic_t *dat;

	if ((fpic = CachePic_Find(path)))
		return fpic;

	// load the pic from disk

	if (only24bit)  // that not for loading 24 bit versions of ".lmp" files, but for some new images
	{
		if ((pic_24bit = GL_LoadPicImage(path, NULL, 0, 0, TEX_ALPHA)))
			return CachePic_Add(path, pic_24bit);

		if(crash)
			Sys_Error ("Draw_CachePic: failed to load %s", path);

		return NULL;
	}

	if (!(dat = (qpic_t *)FS_LoadTempFile (path)))
	{
		if(crash)
			Sys_Error ("Draw_CachePic: failed to load %s", path);

		return NULL;
	}

	SwapPic (dat);

	if ((pic_24bit = GL_LoadPicImage(path, NULL, 0, 0, TEX_ALPHA)))
	{
		memcpy(&pic, pic_24bit, sizeof(mpic_t));
		pic.width = dat->width;
		pic.height = dat->height;
	}
	else
	{
		pic.width = dat->width;
		pic.height = dat->height;
		GL_LoadPicTexture (path, &pic, dat->data);
	}

	return CachePic_Add(path, &pic);
}

mpic_t *Draw_CachePic (char *path)
{
	return Draw_CachePicSafe (path, true, false);
}

// if conwidth or conheight changes, adjust conback sizes too
void Draw_AdjustConback (void) {
	conback.width  = vid.conwidth;
	conback.height = vid.conheight;
}

void Draw_InitConback (void) {
	qpic_t *cb;
	int start;
	mpic_t *pic_24bit;

	start = Hunk_LowMark ();

	if (!(cb = (qpic_t *) FS_LoadHunkFile ("gfx/conback.lmp")))
		Sys_Error ("Couldn't load gfx/conback.lmp");
	SwapPic (cb);

	if (cb->width != 320 || cb->height != 200)
		Sys_Error ("Draw_InitConback: conback.lmp size is not 320x200");


	if ((pic_24bit = GL_LoadPicImage("gfx/conback", "conback", 0, 0, 0))) {
		memcpy(&conback.texnum, &pic_24bit->texnum, sizeof(mpic_t) - 8);
	} else {
		conback.width = cb->width;
		conback.height = cb->height;
		GL_LoadPicTexture ("conback", &conback, cb->data);
	}

	Draw_AdjustConback();

	// free loaded console
	Hunk_FreeToLowMark (start);
}

static int Draw_LoadCharset(char *name) {
	int texnum;

	if (!strcasecmp(name, "original")) {
		int i;
		char buf[128 * 256], *src, *dest;

		memset (buf, 255, sizeof(buf));
		src = (char *) draw_chars;
		dest = buf;
		for (i = 0; i < 16; i++) {
			memcpy (dest, src, 128 * 8);
			src += 128 * 8;
			dest += 128 * 8 * 2;
		}
		char_textures[0] = GL_LoadTexture ("pic:charset", 128, 256, (byte *)buf, TEX_ALPHA, 1);
		goto done;
	}

	if ((texnum = GL_LoadCharsetImage (va("textures/charsets/%s", name), "pic:charset"))) {
		char_textures[0] = texnum;
		goto done;
	}

	Com_Printf ("Couldn't load charset \"%s\"\n", name);
	return 1;

done:
	if (!gl_smoothfont.value) {
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	return 0;
}

qbool OnChange_gl_consolefont(cvar_t *var, char *string) {
	return Draw_LoadCharset(string);
}

void Draw_LoadCharset_f (void) {
	switch (Cmd_Argc()) {
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

static int LoadAlternateCharset (char *name)
{
	int i;
	byte	buf[128*256];
	byte	*data;
	byte	*src, *dest;
	int texnum;

	/* We expect an .lmp to be in QPIC format, but it's ok if it's just raw data */
	data = FS_LoadTempFile (va("gfx/%s.lmp", name));
	if (!data)
		return 0;
	if (fs_filesize == 128*128)
		/* raw data */;
	else if (fs_filesize == 128*128 + 8) {
		qpic_t *p = (qpic_t *)data;
		SwapPic (p);
		if (p->width != 128 || p->height != 128)
			return 0;
		data += 8;
	}
	else
		return 0;

	for (i=0 ; i<256*64 ; i++)
		if (data[i] == 0)
			data[i] = 255;	// proper transparent color

	// Convert the 128*128 conchars texture to 128*256 leaving
	// empty space between rows so that chars don't stumble on
	// each other because of texture smoothing.
	// This hack costs us 64K of GL texture memory
	memset (buf, 255, sizeof(buf));
	src = data;
	dest = buf;
	for (i=0 ; i<16 ; i++) {
		memcpy (dest, src, 128*8);
		src += 128*8;
		dest += 128*8*2;
	}

	texnum = GL_LoadTexture (va("pic:%s", name), 128, 256, buf, TEX_ALPHA, 1);
	if (!gl_smoothfont.value)
	{
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	return texnum;
}



void Draw_InitCharset(void) {
	int i;

	memset(char_textures, 0, sizeof(char_textures));
	memset(char_range,    0, sizeof(char_range));

	draw_chars = W_GetLumpName ("conchars");
	for (i = 0; i < 256 * 64; i++) {
		if (draw_chars[i] == 0)
			draw_chars[i] = 255;
	}

	Draw_LoadCharset(gl_consolefont.string);

	if (!char_textures[0])
		Cvar_Set(&gl_consolefont, "original");

	if (!char_textures[0])
		Sys_Error("Draw_InitCharset: Couldn't load charset");

	char_textures[1] = LoadAlternateCharset ("conchars-cyr");
	if (char_textures[1])
		char_range[1] = 0x0400;
	else
		char_range[1] = 0;
}

void CP_Init (void);
void Draw_InitConsoleBackground(void);

void Draw_Init (void) {
	Cmd_AddCommand("loadcharset", Draw_LoadCharset_f);

	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register (&scr_conalpha);
	Cvar_Register (&scr_conback);
	Cvar_Register (&gl_smoothfont);
	Cvar_Register (&gl_consolefont);
	Cvar_Register (&gl_alphafont);

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register (&scr_menualpha);

	Cvar_SetCurrentGroup(CVAR_GROUP_CROSSHAIR);
	Cvar_Register (&gl_crosshairimage);
	Cvar_Register (&gl_crosshairalpha);

	Cvar_ResetCurrentGroup();

	draw_chars = NULL;
	draw_disc = draw_backtile = NULL;

	W_LoadWadFile("gfx.wad"); // safe re-init

	CachePics_DeInit();

// { scrap clearing
	memset (scrap_allocated, 0, sizeof(scrap_allocated));
	memset (scrap_texels,    0, sizeof(scrap_texels));

	scrap_dirty = 0; // bit mask
// }

	GL_Texture_Init();  // probably safe to re-init now

	// load the console background and the charset by hand, because we need to write the version
	// string into the background before turning it into a texture
	Draw_InitCharset(); // safe re-init imo
	Draw_InitConback(); // safe re-init imo
	CP_Init();			// safe re-init

	// Load the crosshair pics
	Draw_InitCrosshairs();
	// so console background will be re-init
	Draw_InitConsoleBackground();

	// get the other pics we need
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

__inline static void Draw_CharPoly(int x, int y, int num) {
	float frow, fcol;

	frow = (num >> 4) * 0.0625;
	fcol = (num & 15) * 0.0625;

	glTexCoord2f (fcol, frow);
	glVertex2f (x, y);
	glTexCoord2f (fcol + 0.0625, frow);
	glVertex2f (x + 8, y);
	glTexCoord2f (fcol + 0.0625, frow + 0.03125);
	glVertex2f (x + 8, y + 8);
	glTexCoord2f (fcol, frow + 0.03125);
	glVertex2f (x, y + 8);
}

//Draws one 8*8 graphics character with 0 being transparent.
//It can be clipped to the top of the screen to allow the console to be smoothly scrolled off.
void Draw_Character (int x, int y, int num) {
	Draw_CharacterW (x, y, char2wc(num));
}
void Draw_CharacterW (int x, int y, wchar num) {
	qbool atest = false;
	qbool blend = false;
	int i, slot;

	if (y <= -8)
		return;			// totally off screen

	if (num == 32)
		return;		// space

	slot = 0;
	if ((num & 0xFF00) != 0)
	{
		for (i = 1; i < MAX_CHARSETS; i++)
			if (char_range[i] == (num & 0xFF00)) {
				slot = i;
				break;
			}
		if (i == MAX_CHARSETS)
			num = '?';
	}

	num &= 255;

	if (gl_alphafont.value)	{
		if ((atest = glIsEnabled(GL_ALPHA_TEST)))
			glDisable(GL_ALPHA_TEST);
		if (!(blend = glIsEnabled(GL_BLEND)))
			glEnable(GL_BLEND);
	}

	GL_Bind (char_textures[slot]);

	glBegin (GL_QUADS);
	Draw_CharPoly(x, y, num);
	glEnd ();

	if (gl_alphafont.value)	{
		if (atest)
			glEnable(GL_ALPHA_TEST);
		if (!blend)
			glDisable(GL_BLEND);
	}
}

void Draw_String (int x, int y, const char *str) {
	int num;
	qbool atest = false;
	qbool blend = false;

	if (y <= -8)
		return;			// totally off screen

	if (!*str)
		return;

	if (gl_alphafont.value)	{
		if ((atest = glIsEnabled(GL_ALPHA_TEST)))
			glDisable(GL_ALPHA_TEST);
		if (!(blend = glIsEnabled(GL_BLEND)))
			glEnable(GL_BLEND);
	}

	GL_Bind (char_textures[0]);

	glBegin (GL_QUADS);

	while (*str) {	// stop rendering when out of characters
		if ((num = *str++) != 32)	// skip spaces
			Draw_CharPoly(x, y, num);

		x += 8;
	}

	glEnd ();

	if (gl_alphafont.value)	{
		if (atest)
			glEnable(GL_ALPHA_TEST);
		if (!blend)
			glDisable(GL_BLEND);
	}
}

void Draw_StringW (int x, int y, const wchar *ws)
{
	if (y <= -8)
		return;			// totally off screen
	while (*ws)
	{
		Draw_CharacterW (x, y, *ws++);
		x += 8;
	}
}

void Draw_AlphaString (int x, int y, const char *str, float alpha)
{
	int num;
	qbool atest = false;
	qbool blend = false;
	GLfloat value = 0;

	alpha = bound (0, alpha, 1);
	if (!alpha)
		return;

	if (y <= -8)
		return;			// totally off screen

	if (!str || !str[0])
		return;

//	if (gl_alphafont.value)	{ // need this anyway for alpha
		if ((atest = glIsEnabled(GL_ALPHA_TEST)))
			glDisable(GL_ALPHA_TEST);
		if (!(blend = glIsEnabled(GL_BLEND)))
			glEnable(GL_BLEND);
//	}
	glGetTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &value); // save current value
	if (value != GL_MODULATE)
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glColor4f (1, 1, 1, alpha);

	GL_Bind (char_textures[0]);

	glBegin (GL_QUADS);

	while (*str) {	// stop rendering when out of characters
		if ((num = *str++) != 32)	// skip spaces
			Draw_CharPoly(x, y, num);

		x += 8;
	}

	glEnd ();

//	if (gl_alphafont.value)	{ // need this anyway for alpha
		if (atest)
			glEnable(GL_ALPHA_TEST);
		if (!blend)
			glDisable(GL_BLEND);
//	}
	if (value != GL_MODULATE) // restore
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, value);
	glColor3ubv (color_white);
}


void Draw_Alt_String (int x, int y, const char *str) {
	int num;
	qbool atest = false;
	qbool blend = false;

	if (y <= -8)
		return;			// totally off screen

	if (!*str)
		return;

	if (gl_alphafont.value)	{
		if ((atest = glIsEnabled(GL_ALPHA_TEST)))
			glDisable(GL_ALPHA_TEST);
		if (!(blend = glIsEnabled(GL_BLEND)))
			glEnable(GL_BLEND);
	}

	GL_Bind (char_textures[0]);

	glBegin (GL_QUADS);

	while (*str) {// stop rendering when out of characters
		if ((num = *str++ | 128) != (32 | 128))	// skip spaces
			Draw_CharPoly(x, y, num);

		x += 8;
	}

	glEnd ();

	if (gl_alphafont.value)	{
		if (atest)
			glEnable(GL_ALPHA_TEST);
		if (!blend)
			glDisable(GL_BLEND);
	}
}


int HexToInt(char c)
{
	if (isdigit(c))
		return c - '0';
	else if (c >= 'a' && c <= 'f')
		return 10 + c - 'a';
	else if (c >= 'A' && c <= 'F')
		return 10 + c - 'A';
	else
		return -1;
}

void Draw_ColoredString (int x, int y, const char *text, int red) {
	int r, g, b, num;
	qbool white = true;
	qbool atest = false;
	qbool blend = false;

	if (y <= -8)
		return;			// totally off screen

	if (!*text)
		return;

	if (gl_alphafont.value)	{
		if ((atest = glIsEnabled(GL_ALPHA_TEST)))
			glDisable(GL_ALPHA_TEST);
		if (!(blend = glIsEnabled(GL_BLEND)))
			glEnable(GL_BLEND);
	}

	GL_Bind (char_textures[0]);

	if (scr_coloredText.value)
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glBegin (GL_QUADS);

	for ( ; *text; text++) {

		if (*text == '&') {
			if (text[1] == 'c' && text[2] && text[3] && text[4]) {
				r = HexToInt(text[2]);
				g = HexToInt(text[3]);
				b = HexToInt(text[4]);
				if (r >= 0 && g >= 0 && b >= 0) {
					if (scr_coloredText.value) {
						glColor3f(r / 16.0, g / 16.0, b / 16.0);
						white = false;
					}
					text += 4;
					continue;
				}
            } else if (text[1] == 'r')	{
				if (!white) {
					glColor3ubv(color_white);
					white = true;
				}
				text += 1;
				continue;
			}
		}

		num = *text & 255;
		if (!scr_coloredText.value && red)
			num |= 128;

		if (num != 32 && num != (32 | 128))
			Draw_CharPoly(x, y, num);

		x += 8;
	}

	glEnd ();

	if (gl_alphafont.value)	{
		if (atest)
			glEnable(GL_ALPHA_TEST);
		if (!blend)
			glDisable(GL_BLEND);
	}

	if (!white)
		glColor3ubv(color_white);

	if (scr_coloredText.value)
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

const int int_white = 0xFFFFFFFF;

int RGBA_2_Int(byte r, byte g, byte b, byte a) {
	return ((r << 0) | (g << 8) | (b << 16) | (a << 24)) & 0xFFFFFFFF;
}

byte* Int_2_RGBA(int i, byte rgba[4]) {
	rgba[0] = (i >> 0  & 0xFF);
	rgba[1] = (i >> 8  & 0xFF);
	rgba[2] = (i >> 16 & 0xFF);
	rgba[3] = (i >> 24 & 0xFF);

	return rgba;
}

/*
	Instead of keeping color info in *text we provide color for each symbol in different array

	char rgb[] = "rgb";
	int i_rgb[3] = {RGBA_2_Int(255,0,0,255), RGBA_2_Int(0,255,0,255), RGBA_2_Int(0,0,255,255)};
	// this will draw "rgb" non transparent string where r symbol will be red, g will be green and b is blue
	Draw_ColoredString2 (0, 10, rgb, i_rgba, false)

	If u want use alpha u must use "gl_alphafont 1", "scr_coloredText 1" and "red" param must be false
*/
void Draw_ColoredString2 (int x, int y, const char *text, int *clr, int red) {
	byte white4[4] = {255, 255, 255, 255}, rgba[4];
	int num, i, last;
	qbool atest = false;
	qbool blend = false;

	if (y <= -8)
		return;			// totally off screen

	if (!*text || !clr)
		return;

	if (gl_alphafont.value)	{
		if ((atest = glIsEnabled(GL_ALPHA_TEST)))
			glDisable(GL_ALPHA_TEST);
		if (!(blend = glIsEnabled(GL_BLEND)))
			glEnable(GL_BLEND);
	}

	GL_Bind (char_textures[0]);

	if (scr_coloredText.value)
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glColor4ubv(white4);

	glBegin (GL_QUADS);

	for (last = int_white, i = 0; text[i]; i++) {
		if (scr_coloredText.value && clr[i] != last) {
			// probably here we may made some trick like glColor4ubv((byte*)&last); instead of Int_2_RGBA()
			glColor4ubv(Int_2_RGBA(last = clr[i], rgba));
		}

		num = text[i] & 255;
		if (!scr_coloredText.value && red) // do not convert to red if we use coloredText
			num |= 128;

		if (num != 32 && num != (32 | 128))
			Draw_CharPoly(x, y, num);

		x += 8;
	}

	glEnd ();

	if (gl_alphafont.value)	{
		if (atest)
			glEnable(GL_ALPHA_TEST);
		if (!blend)
			glDisable(GL_BLEND);
	}

	glColor4ubv(white4);

	if (scr_coloredText.value)
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}


/*
	Instead of keeping color info in *text we provide info then particular color starts

	char str[] = "redgreen";
	clrinfo_t info[2] = { {RGBA_2_Int(255,0,0,255), 0}, {RGBA_2_Int(0,255,0,255), 3} };
	// this will draw "redgreen" non transparent string where "red" will be red, "green" will be green
	Draw_ColoredString2 (0, 10, str, info, 2, false)

	If u want use alpha u must use "gl_alphafont 1", "scr_coloredText 1" and "red" param must be false
*/
void Draw_ColoredString3 (int x, int y, const char *text, clrinfo_t *clr, int clr_cnt, int red) {
	Draw_ColoredString3W (x, y, str2wcs(text), clr, clr_cnt, red);
}
void Draw_ColoredString3W (int x, int y, const wchar *text, clrinfo_t *clr, int clr_cnt, int red) {
	byte white4[4] = {255, 255, 255, 255}, rgba[4];
	int num, i, last, j, k;
	qbool atest = false;
	qbool blend = false;
	int slot, oldslot;

	if (y <= -8)
		return;			// totally off screen

	if (!*text || !clr)
		return;

	if (gl_alphafont.value)	{
		if ((atest = glIsEnabled(GL_ALPHA_TEST)))
			glDisable(GL_ALPHA_TEST);
		if (!(blend = glIsEnabled(GL_BLEND)))
			glEnable(GL_BLEND);
	}

	if (scr_coloredText.value)
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	GL_Bind (char_textures[0]);
	oldslot = 0;

	glColor4ubv(white4);

	glBegin (GL_QUADS);

	for (last = int_white, j = i = 0; text[i]; i++) {
		if (scr_coloredText.value && j < clr_cnt && i == clr[j].i) {
			if (clr[j].c != last)
				glColor4ubv(Int_2_RGBA(last = clr[j].c, rgba));
			j++;
		}

		num = text[i];
		if (num == 32)
			goto _continue;

		slot = 0;
		if ((num & 0xFF00) != 0)
		{
			for (k = 1; k < MAX_CHARSETS; k++)
				if (char_range[k] == (num & 0xFF00)) {
					slot = k;
					break;
				}
			if (k == MAX_CHARSETS)
				num = '?';
		}
		if (slot != oldslot) {
			glEnd ();
			GL_Bind (char_textures[slot]);
			glBegin (GL_QUADS);
			oldslot = slot;
		}

		num &= 255;
		if (!scr_coloredText.value && red) // do not convert to red if we use coloredText
			num |= 128;

		Draw_CharPoly(x, y, num);

_continue:
		x += 8;
	}

	glEnd ();

	if (gl_alphafont.value)	{
		if (atest)
			glEnable(GL_ALPHA_TEST);
		if (!blend)
			glDisable(GL_BLEND);
	}

	glColor4ubv(white4);

	if (scr_coloredText.value)
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

void Draw_ScalableColoredString (int x, int y, const wchar *text, clrinfo_t *clr, int clr_cnt, int red, float scale)
{
	byte white4[4] = {255, 255, 255, 255}, rgba[4];
	int num, i, last, j, k;
	qbool atest = false;
	qbool blend = false;
	int slot, oldslot;

	if (!*text || !clr)
		return;

	if (gl_alphafont.value)
	{
		if ((atest = glIsEnabled(GL_ALPHA_TEST)))
			glDisable(GL_ALPHA_TEST);
		if (!(blend = glIsEnabled(GL_BLEND)))
			glEnable(GL_BLEND);
	}

	if (scr_coloredText.value)
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	GL_Bind (char_textures[0]);
	oldslot = 0;

	glColor4ubv(white4);

	glBegin (GL_QUADS);

	for (last = int_white, j = i = 0; text[i]; i++)
	{
		if (scr_coloredText.value && j < clr_cnt && i == clr[j].i)
		{
			if (clr[j].c != last)
				glColor4ubv(Int_2_RGBA(last = clr[j].c, rgba));
			j++;
		}

		num = text[i];
		if (num == 32)
		{
			x += (8 * scale);
			continue;
		}

		slot = 0;
		if ((num & 0xFF00) != 0)
		{
			for (k = 1; k < MAX_CHARSETS; k++)
			{
				if (char_range[k] == (num & 0xFF00))
				{
					slot = k;
					break;
				}
			}

			if (k == MAX_CHARSETS)
				num = '?';
		}

		if (slot != oldslot)
		{
			glEnd ();
			GL_Bind (char_textures[slot]);
			glBegin (GL_QUADS);
			oldslot = slot;
		}

		num &= 255;
		// Do not convert to red if we use coloredText.
		if (!scr_coloredText.value && red) 
			num |= 128;

		Draw_SCharacter(x, y, num, scale);

		x += (8 * scale);
	}

	glEnd ();

	if (gl_alphafont.value) 
	{
		if (atest)
			glEnable(GL_ALPHA_TEST);
		if (!blend)
			glDisable(GL_BLEND);
	}

	glColor4ubv(white4);

	if (scr_coloredText.value)
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

void Draw_Crosshair (void) {
	float x = 0.0, y = 0.0, ofs1, ofs2, sh, th, sl, tl;
	byte *col;
	extern vrect_t scr_vrect;

	// Multiview
	if (cls.mvdplayback && cl_multiview.value == 2 && CURRVIEW == 1 && !cl_mvinsetcrosshair.value)
	{
		return;
	}

	if ((crosshair.value >= 2 && crosshair.value <= NUMCROSSHAIRS + 1) ||
		((customcrosshair_loaded & CROSSHAIR_TXT) && crosshair.value == 1) ||
		(customcrosshair_loaded & CROSSHAIR_IMAGE))
	{
		// Multiview
		if (cl_multiview.value && cls.mvdplayback)
		{
			if (cl_multiview.value == 1)
			{
				x = scr_vrect.x + scr_vrect.width / 2 + cl_crossx.value;
				y = scr_vrect.y + scr_vrect.height / 2 + cl_crossy.value;
			}
			else if (cl_multiview.value == 2)
			{
				if (!cl_mvinset.value)
				{
					if (CURRVIEW == 1)
					{
						x = vid.width / 2;
						y = vid.height * 3/4;
					}
					else if (CURRVIEW == 2)
					{
						// top cv2
						x = vid.width / 2;
						y = vid.height / 4;
					}
				}
				else
				{
					// inset
					if (CURRVIEW == 2)
					{
						// normal
						x = scr_vrect.x + scr_vrect.width / 2 + cl_crossx.value;
						y = scr_vrect.y + scr_vrect.height / 2 + cl_crossy.value;
					}
					else if (CURRVIEW == 1)
					{
						x = vid.width - (vid.width/3)/2;
						if (cl_sbar.value)
						{
							y = ((vid.height/3)-sb_lines/3)/2;
						}
						else
						{
							// no sbar
							y = (vid.height/3)/2;
						}
					}
				}
			}
			else if (cl_multiview.value == 3)
			{
				if (CURRVIEW == 2)
				{
					// top
					x = vid.width / 2;
					y = vid.height / 4;
				}
				else if (CURRVIEW == 3)
				{
					// bl
					x = vid.width / 4;
					y = vid.height/2 + vid.height/4;
				}
				else
				{
					// br
					x = vid.width/2 + vid.width/4;
					y = vid.height/2 + vid.height/4;
				}

			}
			else if (cl_multiview.value >= 4)
			{
				if (CURRVIEW == 2)
				{
					// tl
					x = vid.width/4;
					y = vid.height/4;
				}
				else if (CURRVIEW == 3)
				{
					// tr
					x = vid.width/2 + vid.width/4;
					y = vid.height/4;

				}
				else if (CURRVIEW == 4)
				{
					// bl
					x = vid.width/4;
					y = vid.height/2 + vid.height/4;

				}
				else if (CURRVIEW == 1)
				{
					// br
					x = vid.width/2 + vid.width/4;
					y = vid.height/2 + vid.height/4;
				}
			}
		}
		else
		{
			// not mv
			x = scr_vrect.x + scr_vrect.width / 2 + cl_crossx.value;
			y = scr_vrect.y + scr_vrect.height / 2 + cl_crossy.value;
		}

		if (!gl_crosshairalpha.value)
			return;


		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);


		col = StringToRGB(crosshaircolor.string);



		if (gl_crosshairalpha.value) {
            glDisable(GL_ALPHA_TEST);
            glEnable (GL_BLEND);
			col[3] = bound(0, gl_crosshairalpha.value, 1) * 255;
			glColor4ubv (col);
		} else {
			glColor3ubv (col);
		}


		if (customcrosshair_loaded & CROSSHAIR_IMAGE) {
			GL_Bind (crosshairpic.texnum);
			ofs1 = 4 - 4.0 / crosshairpic.width;
			ofs2 = 4 + 4.0 / crosshairpic.width;
			sh = crosshairpic.sh;
			sl = crosshairpic.sl;
			th = crosshairpic.th;
			tl = crosshairpic.tl;
		} else {
			GL_Bind ((crosshair.value >= 2) ? crosshairtextures[(int) crosshair.value - 2] : crosshairtexture_txt);
			ofs1 = 3.5;
			ofs2 = 4.5;
			tl = sl = 0;
			sh = th = 1;
		}

		// Multiview for the case of mv == 2 with mvinset
		if (cl_multiview.value == 2 && cls.mvdplayback && cl_mvinset.value)
		{
			if (CURRVIEW == 1)
			{
				ofs1 *= (vid.width / 320) * bound(0, crosshairsize.value*0.5, 20);
				ofs2 *= (vid.width / 320) * bound(0, crosshairsize.value*0.5, 20);
			}
			else
			{
				// normal
				ofs1 *= (vid.width / 320) * bound(0, crosshairsize.value, 20);
				ofs2 *= (vid.width / 320) * bound(0, crosshairsize.value, 20);
			}
		}
		else if (cl_multiview.value > 1 && cls.mvdplayback)
		{
			ofs1 *= (vid.width / 320) * bound(0, crosshairsize.value*0.5, 20);
			ofs2 *= (vid.width / 320) * bound(0, crosshairsize.value*0.5, 20);
		}
		else
		{
			ofs1 *= (vid.width / 320) * bound(0, crosshairsize.value, 20);
			ofs2 *= (vid.width / 320) * bound(0, crosshairsize.value, 20);
		}

		glBegin (GL_QUADS);
		glTexCoord2f (sl, tl);
		glVertex2f (x - ofs1, y - ofs1);
		glTexCoord2f (sh, tl);
		glVertex2f (x + ofs2, y - ofs1);
		glTexCoord2f (sh, th);
		glVertex2f (x + ofs2, y + ofs2);
		glTexCoord2f (sl, th);
		glVertex2f (x - ofs1, y + ofs2);
		glEnd ();

		if (gl_crosshairalpha.value)
		{
			glDisable(GL_BLEND);
			glEnable (GL_ALPHA_TEST);
		}

		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glColor3ubv (color_white);
	}
	else if (crosshair.value)
	{
		// Multiview
		if (cls.mvdplayback && cl_multiview.value == 2 && cl_mvinset.value && CURRVIEW == 1)
		{
			if (cl_sbar.value)
			{
				Draw_Character (vid.width - (vid.width/3)/2-4, ((vid.height/3)-sb_lines/3)/2 - 2, '+');
			}
			else
			{
				Draw_Character (vid.width - (vid.width/3)/2-4, (vid.height/3)/2 - 2, '+');
			}
		}
		else if (cls.mvdplayback && cl_multiview.value == 2 && !cl_mvinset.value)
		{
			Draw_Character (vid.width / 2 - 4, vid.height * 3/4 - 2, '+');
			Draw_Character (vid.width / 2 - 4, vid.height / 4 - 2, '+');
		}
		else if (cls.mvdplayback && cl_multiview.value == 3)
		{
			Draw_Character (vid.width / 2 - 4, vid.height / 4 - 2, '+');
			Draw_Character (vid.width / 4 - 4, vid.height/2 + vid.height/4 - 2, '+');
			Draw_Character (vid.width/2 + vid.width/4 - 4, vid.height/2 + vid.height/4 - 2, '+');
		}
		else if (cls.mvdplayback && cl_multiview.value >= 4)
		{
			Draw_Character (vid.width/4 - 4, vid.height/4 - 2, '+');
			Draw_Character (vid.width/2 + vid.width/4 - 4, vid.height/4 - 2, '+');
			Draw_Character (vid.width/4 - 4, vid.height/2 + vid.height/4 - 2, '+');
			Draw_Character (vid.width/2 + vid.width/4 - 4, vid.height/2 + vid.height/4 - 2, '+');
		}
		else
		{
			Draw_Character (scr_vrect.x + scr_vrect.width / 2 - 4 + cl_crossx.value, scr_vrect.y + scr_vrect.height / 2 - 4 + cl_crossy.value, '+');
		}
	}
}

void Draw_TextBox (int x, int y, int width, int lines) {
	mpic_t *p;
	int cx, cy, n;

	// draw left side
	cx = x;
	cy = y;
	p = Draw_CachePic ("gfx/box_tl.lmp");
	Draw_TransPic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_ml.lmp");
	for (n = 0; n < lines; n++) {
		cy += 8;
		Draw_TransPic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_bl.lmp");
	Draw_TransPic (cx, cy+8, p);

	// draw middle
	cx += 8;
	while (width > 0) {
		cy = y;
		p = Draw_CachePic ("gfx/box_tm.lmp");
		Draw_TransPic (cx, cy, p);
		p = Draw_CachePic ("gfx/box_mm.lmp");
		for (n = 0; n < lines; n++) {
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

	// draw right side
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

//This repeats a 64 * 64 tile graphic to fill the screen around a sized down refresh window.
void Draw_TileClear (int x, int y, int w, int h) {
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

void Draw_AlphaRectangleRGB (int x, int y, int w, int h, float r, float g, float b, float thickness, qbool fill, float alpha)
{
	alpha = bound(0, alpha, 1);

	if (!alpha)
	{
		return;
	}

	glDisable (GL_TEXTURE_2D);
	if (alpha < 1)
	{
		glEnable (GL_BLEND);
		glDisable(GL_ALPHA_TEST);
		glColor4f (r, g, b, alpha);
	}
	else
	{
		glColor3f (r, g, b);
	}

	thickness = max(0, thickness);

	if(fill)
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
	if (alpha < 1)
	{
		glEnable (GL_ALPHA_TEST);
		glDisable (GL_BLEND);
	}
	glColor3ubv (color_white);
}

void Draw_AlphaRectangle (int x, int y, int w, int h, int c, float thickness, qbool fill, float alpha)
{
	Draw_AlphaRectangleRGB (x, y, w, h,
		host_basepal[c * 3] / 255.0,
		host_basepal[c * 3 + 1] / 255.0,
		host_basepal[c * 3 + 2] / 255.0,
		thickness, fill, alpha);
}

void Draw_AlphaFillRGB (int x, int y, int w, int h, float r, float g, float b, float alpha)
{
	Draw_AlphaRectangleRGB (x, y, w, h, r, g, b, 1, true, alpha);
}

void Draw_AlphaFill (int x, int y, int w, int h, int c, float alpha)
{
	Draw_AlphaRectangle(x, y, w, h, c, 1, true, alpha);
}

void Draw_FillRGB (int x, int y, int w, int h, float r, float g, float b)
{
	Draw_AlphaFillRGB (x, y, w, h, r, g, b, 1);
}

void Draw_Fill (int x, int y, int w, int h, int c)
{
	Draw_AlphaFill(x, y, w, h, c, 1);
}

void Draw_AlphaOutlineRGB (int x, int y, int w, int h, float r, float g, float b, float thickness, float alpha)
{
	Draw_AlphaRectangleRGB (x, y, w, h, r, g, b, thickness, false, alpha);
}

void Draw_AlphaOutline (int x, int y, int w, int h, int c, float thickness, float alpha)
{
	Draw_AlphaRectangle (x, y, w, h, c, thickness, false, alpha);
}


void Draw_OutlineRGB (int x, int y, int w, int h, float r, float g, float b, float thickness)
{
	Draw_AlphaRectangleRGB (x, y, w, h, r, g, b, thickness, false, 1);
}

void Draw_Outline (int x, int y, int w, int h, int c, float thickness)
{
	Draw_AlphaRectangle (x, y, w, h, c, thickness, false, 1);
}

// HUD -> Cokeman
//

void Draw_AlphaLineRGB (int x_start, int y_start, int x_end, int y_end, float thickness, float r, float g, float b, float alpha)
{
	alpha = bound(0, alpha, 1);

	if (!alpha)
	{
		return;
	}

	glDisable (GL_TEXTURE_2D);
	if (alpha < 1)
	{
		glEnable (GL_BLEND);
		glDisable(GL_ALPHA_TEST);
		glColor4f (r, g, b, alpha);
	}
	else
	{
		glColor3f (r, g, b);
	}

	if(thickness > 0.0)
	{
		glLineWidth(thickness);
	}

	glBegin (GL_LINES);
	glVertex2f (x_start, y_start);
	glVertex2f (x_end, y_end);
	glEnd ();

	glEnable (GL_TEXTURE_2D);
	if (alpha < 1)
	{
		glEnable(GL_ALPHA_TEST);
		glDisable (GL_BLEND);
	}
	glColor3ubv (color_white);
}

void Draw_AlphaLine (int x_start, int y_start, int x_end, int y_end, float thickness, int c, float alpha)
{
	Draw_AlphaLineRGB (x_start, y_start, x_end, y_end, thickness,
		host_basepal[c * 3] / 255.0,
		host_basepal[c * 3 + 1] / 255.0,
		host_basepal[c * 3 + 2] / 255.0,
		alpha);
}

void Draw_Polygon(int x, int y, vec3_t *vertices, int num_vertices, qbool fill, int color)
{
	byte bytecolor[4];
	int i = 0;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glEnable (GL_BLEND);
	glDisable(GL_ALPHA_TEST);

	glColor4ubv(Int_2_RGBA(color, bytecolor));
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

void Draw_AlphaPieSliceRGB (int x, int y, float radius, float startangle, float endangle, float thickness, qbool fill, float r, float g, float b, float alpha)
{
	double angle;
	int i;
	int start;
	int end;

	alpha = bound(0, alpha, 1);

	if (!alpha)
	{
		return;
	}

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glDisable (GL_TEXTURE_2D);
	if (alpha < 1)
	{
		glEnable (GL_BLEND);
		glDisable(GL_ALPHA_TEST);
		glColor4f (r, g, b, alpha);
	}
	else
	{
		glColor3f (r, g, b);
	}

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
	start	= Q_rint((startangle * CIRCLE_LINE_COUNT) / (2*M_PI));
	end		= Q_rint((endangle   * CIRCLE_LINE_COUNT) / (2*M_PI));

	// If the end is less than the start, increase the index so that
	// we start on a "new" circle.
	if(end < start)
	{
		start = start + CIRCLE_LINE_COUNT;
	}

	// Create a vertex at the exact position specified by the start angle.
	glVertex2f (x + radius*cos(startangle), y - radius*sin(startangle));

	// TODO: Use lookup table for sin/cos?
	for(i = start; i < end; i++)
	{
      angle = i*2*M_PI / CIRCLE_LINE_COUNT;
      glVertex2f (x + radius*cos(angle), y - radius*sin(angle));

	  // When filling we're drawing triangles so we need to
	  // create a vertex in the middle of the vertex to fill
	  // the entire pie slice/circle.
	  if(fill)
	  {
		glVertex2f (x, y);
	  }
    }

	glVertex2f (x + radius*cos(endangle), y - radius*sin(endangle));

	// Create a vertex for the middle point if we're not drawing a complete circle.
	if(endangle - startangle < 2*M_PI)
	{
		glVertex2f (x, y);
	}

	glEnd ();

	glEnable (GL_TEXTURE_2D);
	if (alpha < 1)
	{
		glEnable (GL_ALPHA_TEST);
		glDisable (GL_BLEND);
	}
	glColor3ubv (color_white);

	glPopAttrib();
}

void Draw_AlphaPieSlice (int x, int y, float radius, float startangle, float endangle, float thickness, qbool fill, int c, float alpha)
{
	Draw_AlphaPieSliceRGB (x, y, radius, startangle, endangle, thickness, fill,
		host_basepal[c * 3] / 255.0,
		host_basepal[c * 3 + 1] / 255.0,
		host_basepal[c * 3 + 2] / 255.0,
		alpha);
}

void Draw_AlphaCircleRGB (int x, int y, float radius, float thickness, qbool fill, float r, float g, float b, float alpha)
{
	Draw_AlphaPieSliceRGB (x, y, radius, 0, 2*M_PI, thickness, fill, r,	g, b, alpha);
}

void Draw_AlphaCircle (int x, int y, float radius, float thickness, qbool fill, int c, float alpha)
{
	Draw_AlphaPieSlice (x, y, radius, 0, 2*M_PI, thickness, fill, c, alpha);
}

void Draw_AlphaCircleOutlineRGB (int x, int y, float radius, float thickness, float r, float g, float b, float alpha)
{
	Draw_AlphaCircleRGB (x, y, radius, thickness, false, r, g, b, alpha);
}

void Draw_AlphaCircleOutline (int x, int y, float radius, float thickness, int color, float alpha)
{
	Draw_AlphaCircle (x, y, radius, thickness, false, color, alpha);
}

void Draw_AlphaCircleFillRGB (int x, int y, float radius, float r, float g, float b, float alpha)
{
	Draw_AlphaCircleRGB (x, y, radius, 1.0, true, r, g, b, alpha);
}

void Draw_AlphaCircleFill (int x, int y, float radius, int color, float alpha)
{
	Draw_AlphaCircle (x, y, radius, 1.0, true, color, alpha);
}

// HUD -> hexum
// kazik -->
//
// SCALE versions of some functions
//

void Draw_SCharacter (int x, int y, int num, float scale)
{
    int row, col;
    float frow, fcol, size;
	qbool atest = false;
	qbool blend = false;

    if (num == 32)
        return;     // space

    num &= 255;

    if (y <= -8 * scale)
        return;         // totally off screen

    row = num>>4;
    col = num&15;

    frow = row*0.0625;
    fcol = col*0.0625;
    size = 0.0625;

	if (gl_alphafont.value)	{
		if ((atest = glIsEnabled(GL_ALPHA_TEST)))
			glDisable(GL_ALPHA_TEST);
		if (!(blend = glIsEnabled(GL_BLEND)))
			glEnable(GL_BLEND);
	}

    GL_Bind (char_textures[0]);

    glBegin (GL_QUADS);
    glTexCoord2f (fcol, frow);
    glVertex2f (x, y);
    glTexCoord2f (fcol + size, frow);
    glVertex2f (x+scale*8, y);
    glTexCoord2f (fcol + size, frow + size);
    glVertex2f (x+scale*8, y+scale*8*2); // disconnect: hack, hack, hack?
    glTexCoord2f (fcol, frow + size);
    glVertex2f (x, y+scale*8*2); // disconnect: hack, hack, hack?
    glEnd ();

	if (gl_alphafont.value)	{
		if (atest)
			glEnable(GL_ALPHA_TEST);
		if (!blend)
			glDisable(GL_BLEND);
	}
}

void Draw_SString (int x, int y, const char *str, float scale)
{
    while (*str)
    {
        Draw_SCharacter (x, y, *str, scale);
        str++;
        x += 8 * scale;
    }
}

void Draw_SAlt_String (int x, int y, char *str, float scale)
{
    while (*str)
    {
        Draw_SCharacter (x, y, (*str) | 0x80, scale);
        str++;
        x += 8 * scale;
    }
}

//=============================================================================
// Draw picture functions
//=============================================================================
void Draw_SAlphaSubPic2 (int x, int y, mpic_t *gl, int srcx, int srcy, int width, int height, float scale_x, float scale_y, float alpha)
{
	float newsl, newtl, newsh, newth;
    float oldglwidth, oldglheight;

    if (scrap_dirty)
	{
        Scrap_Upload ();
	}

    oldglwidth = gl->sh - gl->sl;
    oldglheight = gl->th - gl->tl;

    newsl = gl->sl + (srcx * oldglwidth) / gl->width;
    newsh = newsl + (width * oldglwidth) / gl->width;

    newtl = gl->tl + (srcy * oldglheight) / gl->height;
    newth = newtl + (height * oldglheight) / gl->height;

	if(alpha < 1.0)
	{
		glDisable (GL_ALPHA_TEST);
		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glCullFace (GL_FRONT);
		glColor4f (1, 1, 1, alpha);
	}

    GL_Bind (gl->texnum);
    glBegin (GL_QUADS);

	// Upper left corner.
	glTexCoord2f (newsl, newtl);
    glVertex2f (x, y);

	// Upper right corner.
	glTexCoord2f (newsh, newtl);
    glVertex2f (x + scale_x * width, y);

	// Bottom right corner.
	glTexCoord2f (newsh, newth);
    glVertex2f (x + scale_x * width, y + scale_y * height);

	// Bottom left corner.
	glTexCoord2f (newsl, newth);
    glVertex2f (x, y + scale_y * height);

	glEnd ();

	if(alpha < 1.0)
	{
		glEnable (GL_ALPHA_TEST);
		glDisable (GL_BLEND);
//		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
//		glCullFace (GL_FRONT);
		glColor4f (1, 1, 1, 1);
	}
}

void Draw_SAlphaSubPic (int x, int y, mpic_t *gl, int srcx, int srcy, int width, int height, float scale, float alpha)
{
	Draw_SAlphaSubPic2 (x, y, gl, srcx, srcy, width, height, scale, scale, alpha);
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
	Draw_SAlphaSubPic (x, y, gl, 0, 0, gl->width, gl->height, scale, 1);
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
    /*if (   x < 0 || (unsigned)(x + scale*pic->width) > vid.width
		|| y < 0 || (unsigned)(y + scale*pic->height) > vid.height )
    {
        Sys_Error ("Draw_TransPic: bad coordinates");
    }*/

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
	/*
	// Why do this? GL Can handle drawing partialy outside the screen.
	if (   x < 0 || (unsigned) (x + pic->width) > vid.width
		|| y < 0 || (unsigned) (y + pic->height) > vid.height )
		Sys_Error ("Draw_TransPic: bad coordinates");
	*/

	Draw_Pic (x, y, pic);
}

void Draw_SFill (int x, int y, int w, int h, int c, float scale)
{
    glDisable (GL_TEXTURE_2D);
    glColor3f (host_basepal[c*3]/255.0,
        host_basepal[c*3+1]/255.0,
        host_basepal[c*3+2]/255.0);

    glBegin (GL_QUADS);

    glVertex2f (x,y);
    glVertex2f (x+w*scale, y);
    glVertex2f (x+w*scale, y+h*scale);
    glVertex2f (x, y+h*scale);

    glEnd ();
    glColor3f (1,1,1);
    glEnable (GL_TEXTURE_2D);
}

static char last_mapname[MAX_QPATH] = {0};
static mpic_t *last_lvlshot = NULL;

// need for vid_restart
void Draw_InitConsoleBackground(void)
{
	last_lvlshot = NULL;
	last_mapname[0] = 0;
}

void Draw_ConsoleBackground (int lines) 
{
	mpic_t *lvlshot = NULL;
	float alpha = (SCR_NEED_CONSOLE_BACKGROUND ? 1 : bound(0, scr_conalpha.value, 1));

	if (host_mapname.string[0] // we have mapname
		 && (    scr_conback.value == 2 // always per level conback
			 || (scr_conback.value == 1 && SCR_NEED_CONSOLE_BACKGROUND) // only at load time
			)
	   ) {
		if (strncmp(host_mapname.string, last_mapname, sizeof(last_mapname))) { // lead to call Draw_CachePicSafe() once per level
			char name[MAX_QPATH];

			snprintf(name, sizeof(name), "textures/levelshots/%s.xxx", host_mapname.string);
			if ((last_lvlshot = Draw_CachePicSafe(name, false, true))) { // resize
				last_lvlshot->width  = conback.width;
				last_lvlshot->height = conback.height;
			}

			strlcpy(last_mapname, host_mapname.string, sizeof(last_mapname)); // save
		}

		lvlshot = last_lvlshot;
	}

	if (alpha)
		Draw_AlphaPic(0, (lines - vid.height) + (int)con_shift.value, lvlshot ? lvlshot : &conback, alpha);
}

// ================
// Draw_FadeBox
// ================
/*
// Use Draw_AlphaFillRGB instead
void Draw_FadeBox (int x, int y, int width, int height,
                   float r, float g, float b, float a)
{
    if (a <= 0)
        return;

    glDisable(GL_ALPHA_TEST);
    glEnable (GL_BLEND);
    glDisable (GL_TEXTURE_2D);

    glColor4f (r, g, b, a);

    glBegin (GL_QUADS);
    glVertex2f (x, y);
    glVertex2f (x + width, y);
    glVertex2f (x + width, y + height);
    glVertex2f (x, y + height);
    glEnd ();

    glColor3f (1,1,1);
    glEnable(GL_ALPHA_TEST);
    glDisable (GL_BLEND);
    glEnable (GL_TEXTURE_2D);
}*/
// kazik <--

void Draw_FadeScreen (void) {
	float alpha;

	alpha = bound(0, scr_menualpha.value, 1);
	if (!alpha)
		return;

	if (alpha < 1) {
		glDisable (GL_ALPHA_TEST);
		glEnable (GL_BLEND);
		glColor4f (0, 0, 0, alpha);
	} else {
		glColor3f (0, 0, 0);
	}
	glDisable (GL_TEXTURE_2D);

	glBegin (GL_QUADS);
	glVertex2f (0, 0);
	glVertex2f (vid.width, 0);
	glVertex2f (vid.width, vid.height);
	glVertex2f (0, vid.height);
	glEnd ();

	if (alpha < 1) {
		glDisable (GL_BLEND);
		glEnable (GL_ALPHA_TEST);
	}
	glColor3ubv (color_white);
	glEnable (GL_TEXTURE_2D);

	Sbar_Changed();
}

//=============================================================================

//Draws the little blue disc in the corner of the screen.
//Call before beginning any disc IO.
void Draw_BeginDisc (void) {
	if (!draw_disc)
		return;
	glDrawBuffer  (GL_FRONT);
	Draw_Pic (vid.width - 24, 0, draw_disc);
	glDrawBuffer  (GL_BACK);
}

//Erases the disc icon.
//Call after completing any disc IO
void Draw_EndDisc (void) {}

void GL_Set2D (void) {
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
