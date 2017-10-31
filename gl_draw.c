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

void CachePics_Init(void);
void Draw_InitCharset(void);
void CachePics_LoadAmmoPics(mpic_t* ibar);

extern cvar_t crosshair, cl_crossx, cl_crossy, crosshaircolor, crosshairsize;
extern cvar_t con_shift, hud_faderankings;

cvar_t	scr_conalpha		= {"scr_conalpha", "0.8"};
cvar_t	scr_conback			= {"scr_conback", "1"};
void OnChange_scr_conpicture(cvar_t *, char *, qbool *);
cvar_t  scr_conpicture      = {"scr_conpicture", "conback", 0, OnChange_scr_conpicture};
cvar_t	scr_menualpha		= {"scr_menualpha", "0.7"};
cvar_t	scr_menudrawhud		= {"scr_menudrawhud", "0"};


void OnChange_crosshairimage(cvar_t *, char *, qbool *);
static cvar_t crosshairimage          = {"crosshairimage", "", 0, OnChange_crosshairimage};

cvar_t crosshairalpha                 = {"crosshairalpha", "1"};

static cvar_t crosshairscalemethod         = {"crosshairscalemethod", "0"};
static cvar_t crosshairscale               = {"crosshairscale", "0"};
static int    current_crosshair_pixel_size = 0;

mpic_t			*draw_disc;
static mpic_t	*draw_backtile;

static mpic_t	conback;

mpic_t      crosshairtexture_txt;
mpic_t      crosshairpic;
mpic_t      crosshairs_builtin[NUMCROSSHAIRS];

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
	CachePics_MarkAtlasDirty();
}

void customCrosshair_Init(void)
{
	char ch;
	vfsfile_t *f;
	vfserrno_t err;
	int i = 0, c;

	customcrosshair_loaded = CROSSHAIR_NONE;
	GL_TextureReferenceInvalidate(crosshairtexture_txt.texnum);

	if (!(f = FS_OpenVFS("crosshairs/crosshair.txt", "rb", FS_ANY))) {
		return;
	}

	while (i < 64) {
		VFS_READ(f, &ch, sizeof(char), &err);
		if (err == VFSERR_EOF) {
			Com_Printf("Invalid format in crosshair.txt (Need 64 X's and O's)\n");
			VFS_CLOSE(f);
			return;
		}
		c = ch;

		if (isspace(c))
			continue;

		if (tolower(c) != 'x' && tolower(c) != 'o') {
			Com_Printf("Invalid format in crosshair.txt (Only X's and O's and whitespace permitted)\n");
			VFS_CLOSE(f);
			return;
		}
		customcrosshairdata[i++] = (c == 'x' || c == 'X') ? 0xfe : 0xff;
	}

	VFS_CLOSE(f);
	crosshairtexture_txt.texnum = GL_LoadTexture("cross:custom", 8, 8, customcrosshairdata, TEX_ALPHA, 1);
	crosshairtexture_txt.sl = crosshairtexture_txt.tl = 0;
	crosshairtexture_txt.sh = crosshairtexture_txt.th = 1;
	customcrosshair_loaded |= CROSSHAIR_TXT;
	CachePics_MarkAtlasDirty();
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
		crosshairs_builtin[i].texnum = GL_LoadTexture (str, crosshair_size, crosshair_size, crosshair_buffer, TEX_ALPHA, 1);
		crosshairs_builtin[i].sl = crosshairs_builtin[i].tl = 0;
		crosshairs_builtin[i].sh = crosshairs_builtin[i].th = 1;
		crosshairs_builtin[i].height = crosshairs_builtin[i].width = 16;

		GL_TexParameteri(GL_TEXTURE0, crosshairs_builtin[i].texnum, GL_TEXTURE_WRAP_S, GL_CLAMP);
		GL_TexParameteri(GL_TEXTURE0, crosshairs_builtin[i].texnum, GL_TEXTURE_WRAP_T, GL_CLAMP);

		Q_free(crosshair_buffer);
	}
	current_crosshair_pixel_size = crosshair_size;
	CachePics_MarkAtlasDirty();
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

static int          scrap_allocated[MAX_SCRAPS][BLOCK_WIDTH];
static byte         scrap_texels[MAX_SCRAPS][BLOCK_WIDTH*BLOCK_HEIGHT];
static int          scrap_dirty; // Bit mask.
static texture_ref  scrap_texnum[MAX_SCRAPS];

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

static void Scrap_Init(void)
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
wadpic_t wad_pictures[WADPIC_PIC_COUNT];

mpic_t *Draw_CacheWadPic(char *name, int code)
{
	qpic_t *p;
	mpic_t *pic, *pic_24bit;
	wadpic_t* wadpic = NULL;

	if (code >= 0 && code < WADPIC_PIC_COUNT) {
		wadpic = &wad_pictures[code];
	}

	p = W_GetLumpName (name);
	pic = (mpic_t *)p;
	if (wadpic) {
		strlcpy(wadpic->name, name, sizeof(wadpic->name));
		wadpic->pic = pic;
	}

	if ((pic_24bit = GL_LoadPicImage(va("textures/wad/%s", name), name, 0, 0, TEX_ALPHA)) ||
		(pic_24bit = GL_LoadPicImage(va("gfx/%s", name), name, 0, 0, TEX_ALPHA)))
	{
		// Only keep the size info from the lump. The other stuff is copied from the 24 bit image.
		pic->sh		= pic_24bit->sh;
		pic->sl		= pic_24bit->sl;
		pic->texnum = pic_24bit->texnum;
		pic->th		= pic_24bit->th;
		pic->tl		= pic_24bit->tl;

		if (code == WADPIC_SB_IBAR) {
			CachePics_LoadAmmoPics(pic);
		}

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
			GL_LoadPicTexture(name, pic, p->data);
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

		if (!GL_TextureReferenceIsValid(scrap_texnum[texnum])) {
			Com_Printf("Scrap_Upload: texture[%d] still not set, HUD will be broken\n", texnum);
		}

		pic->texnum = scrap_texnum[texnum];
	}
	else
	{
		GL_LoadPicTexture(name, pic, p->data);
	}

	if (code == WADPIC_SB_IBAR) {
		CachePics_LoadAmmoPics(pic);
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
mpic_t *Draw_CachePicSafe(const char *path, qbool crash, qbool only24bit)
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
	}
	else if (dat) {
		mpic_t tmp = {0};
		tmp.width = dat->width;
		tmp.height = dat->height;
		GL_LoadPicTexture(path, &tmp, dat->data);
		return CachePic_Add(path, &tmp);
	}
	else {
		if(crash) {
			Sys_Error ("Draw_CachePicSafe: failed to load %s", path);
		}
		return NULL;
	}
}

static const char* cache_pic_paths[] = {
	"gfx/pause.lmp",
	"gfx/loading.lmp",
	"gfx/box_tl.lmp",
	"gfx/box_ml.lmp",
	"gfx/box_bl.lmp",
	"gfx/box_tm.lmp",
	"gfx/box_mm.lmp",
	"gfx/box_mm2.lmp",
	"gfx/box_bm.lmp",
	"gfx/box_tr.lmp",
	"gfx/box_mr.lmp",
	"gfx/box_br.lmp",
	"gfx/ttl_main.lmp",
	"gfx/mainmenu.lmp",
	"gfx/menudot1.lmp",
	"gfx/menudot2.lmp",
	"gfx/menudot3.lmp",
	"gfx/menudot4.lmp",
	"gfx/menudot5.lmp",
	"gfx/menudot6.lmp",
	"gfx/ttl_sgl.lmp",
	"gfx/qplaque.lmp",
	"gfx/sp_menu.lmp",
	"gfx/p_load.lmp",
	"gfx/p_save.lmp",
	"gfx/p_multi.lmp",
	"gfx/ranking.lmp",
	"gfx/complete.lmp",
	"gfx/inter.lmp",
	"gfx/finale.lmp"
};

mpic_t *Draw_CachePic(cache_pic_id_t id)
{
	if (id < 0 || id >= CACHEPIC_NUM_OF_PICS) {
		Sys_Error("Draw_CachePic(%d) - out of range", id);
	}

	return Draw_CachePicSafe(cache_pic_paths[id], true, false);
}

static void Draw_Precache(void)
{
	int i;
	for (i = 0; i < CACHEPIC_NUM_OF_PICS; ++i) {
		Draw_CachePic(i);
	}
}

void Draw_InitConback (void);

void Draw_Init (void)
{
	extern void HUD_Common_Reset_Group_Pics(void);
	extern void Draw_Charset_Init(void);

	Draw_Charset_Init();

	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register (&scr_conalpha);
	Cvar_Register (&scr_conback);
	Cvar_Register (&scr_conpicture);

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register (&scr_menualpha);
	Cvar_Register (&scr_menudrawhud);

	Cvar_SetCurrentGroup(CVAR_GROUP_CROSSHAIR);
	Cvar_Register (&crosshairimage);
	Cvar_Register (&crosshairalpha);
	Cvar_Register (&crosshairscale);
	Cvar_Register (&crosshairscalemethod);

	Cvar_ResetCurrentGroup();

	draw_disc = draw_backtile = NULL;

	W_LoadWadFile("gfx.wad"); // Safe re-init.
	CachePics_DeInit();
	HUD_Common_Reset_Group_Pics();

	GL_Texture_Init();  // Probably safe to re-init now.

	// Clear the scrap, should be called ASAP after textures initialization
	Scrap_Init();
	CachePics_Init();

	// Load the console background and the charset by hand, because we need to write the version
	// string into the background before turning it into a texture.
	Draw_InitCharset(); // Safe re-init.
	Draw_InitConback(); // Safe re-init.

	// Load the crosshair pics
	Draw_InitCrosshairs();

	// Get the other pics we need.
	draw_disc     = Draw_CacheWadPic("disc", WADPIC_DISC);
	draw_backtile = Draw_CacheWadPic("backtile", WADPIC_BACKTILE);

	Draw_Precache();
}

qbool CL_MultiviewGetCrosshairCoordinates(qbool use_screen_coords, float* cross_x, float* cross_y, qbool* half_size);

void Draw_Crosshair (void)
{
	float x = 0.0, y = 0.0, ofs1, ofs2, sh, th, sl, tl;
	byte col[4];
	extern vrect_t scr_vrect;
	float crosshair_scale = (crosshairscalemethod.integer ? 1 : ((float)glwidth / 320));
	int crosshair_pixel_size = CrosshairPixelSize();

	if (current_crosshair_pixel_size != crosshair_pixel_size) {
		BuildBuiltinCrosshairs();
	}

	if ((crosshair.value >= 2 && crosshair.value <= NUMCROSSHAIRS + 1) ||
		((customcrosshair_loaded & CROSSHAIR_TXT) && crosshair.value == 1) ||
		(customcrosshair_loaded & CROSSHAIR_IMAGE)) {
		qbool half_size = false;
		texture_ref texnum;

		if (!crosshairalpha.value) {
			return;
		}

		if (!CL_MultiviewGetCrosshairCoordinates(true, &x, &y, &half_size)) {
			return;
		}

		GL_OrthographicProjection(0, glwidth, glheight, 0, -99999, 99999);

		x += (crosshairscalemethod.integer ? 1 : (float)glwidth / vid.width) * cl_crossx.value;
		y += (crosshairscalemethod.integer ? 1 : (float)glheight / vid.height) * cl_crossy.value;

		memcpy(col, crosshaircolor.color, 3);
		col[3] = bound(0, crosshairalpha.value, 1) * 255;

		if (customcrosshair_loaded & CROSSHAIR_IMAGE) {
			texnum = crosshairpic.texnum;
			ofs1 = (crosshairpic.width * 0.5f - 0.5f) * bound(0, crosshairsize.value, 20);
			ofs2 = (crosshairpic.height * 0.5f + 0.5f) * bound(0, crosshairsize.value, 20);

			sh = crosshairpic.sh;
			sl = crosshairpic.sl;
			th = crosshairpic.th;
			tl = crosshairpic.tl;
		}
		else {
			mpic_t* pic = ((crosshair.value >= 2) ? &crosshairs_builtin[(int)crosshair.value - 2] : &crosshairtexture_txt);

			texnum = pic->texnum;
			ofs1 = (crosshair_pixel_size * 0.5f - 0.5f) * crosshair_scale * bound(0, crosshairsize.value, 20);
			ofs2 = (crosshair_pixel_size * 0.5f + 0.5f) * crosshair_scale * bound(0, crosshairsize.value, 20);

			sh = pic->sh;
			sl = pic->sl;
			th = pic->th;
			tl = pic->tl;
		}

		if (half_size) {
			ofs1 *= 0.5f;
			ofs2 *= 0.5f;
		}

		GLM_DrawImage(x - ofs1, y - ofs1, ofs1 + ofs2, ofs1 + ofs2, sl, tl, sh - sl, th - tl, col, false, texnum, false);

		GL_OrthographicProjection(0, vid.width, vid.height, 0, -99999, 99999);
	}
	else if (crosshair.value) {
		// Multiview
		if (CL_MultiviewInsetEnabled()) {
			if (CL_MultiviewInsetView()) {
				if (cl_sbar.value) {
					Draw_Character(vid.width - (vid.width / 3) / 2 - 4, ((vid.height / 3) - sb_lines / 3) / 2 - 2, '+');
				}
				else {
					Draw_Character(vid.width - (vid.width / 3) / 2 - 4, (vid.height / 3) / 2 - 2, '+');
				}
			}
			else {
				Draw_Character(scr_vrect.x + scr_vrect.width / 2 - 4 + cl_crossx.value, scr_vrect.y + scr_vrect.height / 2 - 4 + cl_crossy.value, '+');
			}
		}
		else if (CL_MultiviewActiveViews() == 2) {
			Draw_Character(vid.width / 2 - 4, vid.height * 3 / 4 - 2, '+');
			Draw_Character(vid.width / 2 - 4, vid.height / 4 - 2, '+');
		}
		else if (CL_MultiviewActiveViews() == 3) {
			Draw_Character(vid.width / 2 - 4, vid.height / 4 - 2, '+');
			Draw_Character(vid.width / 4 - 4, vid.height / 2 + vid.height / 4 - 2, '+');
			Draw_Character(vid.width / 2 + vid.width / 4 - 4, vid.height / 2 + vid.height / 4 - 2, '+');
		}
		else if (CL_MultiviewActiveViews() == 4) {
			Draw_Character(vid.width / 4 - 4, vid.height / 4 - 2, '+');
			Draw_Character(vid.width / 2 + vid.width / 4 - 4, vid.height / 4 - 2, '+');
			Draw_Character(vid.width / 4 - 4, vid.height / 2 + vid.height / 4 - 2, '+');
			Draw_Character(vid.width / 2 + vid.width / 4 - 4, vid.height / 2 + vid.height / 4 - 2, '+');
		}
		else {
			Draw_Character(scr_vrect.x + scr_vrect.width / 2 - 4 + cl_crossx.value, scr_vrect.y + scr_vrect.height / 2 - 4 + cl_crossy.value, '+');
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
	p = Draw_CachePic (CACHEPIC_BOX_TL);
	Draw_TransPic (cx, cy, p);
	p = Draw_CachePic (CACHEPIC_BOX_ML);
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		Draw_TransPic (cx, cy, p);
	}

	p = Draw_CachePic (CACHEPIC_BOX_BL);
	Draw_TransPic (cx, cy+8, p);

	// Draw middle.
	cx += 8;
	while (width > 0)
	{
		cy = y;
		p = Draw_CachePic (CACHEPIC_BOX_TM);
		Draw_TransPic (cx, cy, p);
		p = Draw_CachePic (CACHEPIC_BOX_MM);

		for (n = 0; n < lines; n++)
		{
			cy += 8;
			if (n == 1)
				p = Draw_CachePic (CACHEPIC_BOX_MM2);
			Draw_TransPic (cx, cy, p);
		}

		p = Draw_CachePic (CACHEPIC_BOX_BM);
		Draw_TransPic (cx, cy+8, p);
		width -= 2;
		cx += 16;
	}

	// Draw right side.
	cy = y;
	p = Draw_CachePic (CACHEPIC_BOX_TR);
	Draw_TransPic (cx, cy, p);
	p = Draw_CachePic (CACHEPIC_BOX_MR);

	for (n = 0; n < lines; n++)
	{
		cy += 8;
		Draw_TransPic (cx, cy, p);
	}

	p = Draw_CachePic (CACHEPIC_BOX_BR);
	Draw_TransPic (cx, cy+8, p);
}

// This repeats a 64 * 64 tile graphic to fill the screen around a sized down refresh window.
void Draw_TileClear(int x, int y, int w, int h)
{
	GLM_DrawImage(x, y, w, h, x / 64.0, y / 64.0, w / 64.0, h / 64.0, color_white, false, draw_backtile->texnum, false);
}

void Draw_AlphaRectangleRGB (int x, int y, int w, int h, float thickness, qbool fill, color_t color)
{
	byte bytecolor[4];

	// Is alpha 0?
	if ((byte)(color >> 24 & 0xFF) == 0)
		return;

	COLOR_TO_RGBA(color, bytecolor);
	thickness = max(0, thickness);

	GLM_DrawAlphaRectangeRGB(x, y, w, h, thickness, fill, bytecolor);
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

	GL_StateBeginAlphaLineRGB(thickness);

	COLOR_TO_RGBA(color, bytecolor);

	if (GL_ShadersSupported()) {
		GLM_Draw_LineRGB(bytecolor, x_start, y_start, x_end, y_end);
	}
	else {
		GLC_Draw_LineRGB(bytecolor, x_start, y_start, x_end, y_end);
	}

	GL_StateEndAlphaLineRGB();
}

void Draw_AlphaLine (int x_start, int y_start, int x_end, int y_end, float thickness, byte c, float alpha)
{
	Draw_AlphaLineRGB (x_start, y_start, x_end, y_end, thickness,
		RGBA_TO_COLOR(host_basepal[c * 3], host_basepal[c * 3 + 1], host_basepal[c * 3 + 2], 255));
}

void Draw_Polygon(int x, int y, vec3_t *vertices, int num_vertices, qbool fill, color_t color)
{
	if (GL_ShadersSupported()) {
		GLM_Draw_Polygon(x, y, vertices, num_vertices, fill, color);
	}
	else {
		GLC_Draw_Polygon(x, y, vertices, num_vertices, fill, color);
	}
}

void Draw_AlphaPieSliceRGB (int x, int y, float radius, float startangle, float endangle, float thickness, qbool fill, color_t color)
{
	if (GL_ShadersSupported()) {
		GLM_Draw_AlphaPieSliceRGB(x, y, radius, startangle, endangle, thickness, fill, color);
	}
	else {
		GLC_Draw_AlphaPieSliceRGB(x, y, radius, startangle, endangle, thickness, fill, color);
	}
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

    if (scrap_dirty) {
        Scrap_Upload();
	}

    oldglwidth = pic->sh - pic->sl;
    oldglheight = pic->th - pic->tl;

    newsl = pic->sl + (src_x * oldglwidth) / (float)pic->width;
    newsh = newsl + (src_width * oldglwidth) / (float)pic->width;

    newtl = pic->tl + (src_y * oldglheight) / (float)pic->height;
    newth = newtl + (src_height * oldglheight) / (float)pic->height;

	alpha *= overall_alpha;

	GLM_Draw_SAlphaSubPic2(x, y, pic, src_width, src_height, newsl, newtl, newsh, newth, scale_x, scale_y, alpha);
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

void Draw_FitPicAlpha(int x, int y, int fit_width, int fit_height, mpic_t *gl, float alpha)
{
	float sw, sh;
	sw = (float) fit_width / (float) gl->width;
	sh = (float) fit_height / (float) gl->height;
	Draw_SAlphaPic(x, y, gl, alpha, min(sw, sh));
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

	if (!glConfig.initialized) {
		return;
	}

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
		GL_LoadPicTexture("conback", &conback, cb->data);
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
			mpic_t* old_levelshot = last_lvlshot;

			snprintf(name, sizeof(name), "textures/levelshots/%s.xxx", host_mapname.string);
			if ((last_lvlshot = Draw_CachePicSafe(name, false, true)))
			{
				// Resize.
				last_lvlshot->width  = conback.width;
				last_lvlshot->height = conback.height;

				if (old_levelshot) {
					GL_DeleteTexture(&old_levelshot->texnum);
					if (!CachePic_RemoveByPic(old_levelshot)) {
						GL_TextureReferenceInvalidate(old_levelshot->texnum);
					}
				}
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

void Draw_FadeScreen(float alpha)
{
	alpha = bound(0, alpha, 1) * overall_alpha;
	if (!alpha) {
		return;
	}

	GLM_Draw_FadeScreen(alpha);

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
void GL_Set2D(void)
{
	GL_Viewport(glx, gly, glwidth, glheight);

	GL_OrthographicProjection(0, vid.width, vid.height, 0, -99999, 99999);
	GL_IdentityModelView();

	GL_StateDefault2D();
}

void Draw_2dAlphaTexture(float x, float y, float width, float height, texture_ref texture_num, float alpha)
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
}

qbool Draw_IsConsoleBackground(mpic_t* pic)
{
	return pic == &conback || pic == last_lvlshot;
}
